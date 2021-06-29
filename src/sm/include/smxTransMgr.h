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
 * $Id: smxTransMgr.h 89746 2021-01-11 06:22:36Z emlee $
 **********************************************************************/

#ifndef _O_SMX_TRANS_MGR_H_
#define _O_SMX_TRANS_MGR_H_ 1

#include <idu.h>
#include <smiDef.h>
#include <smxTrans.h>
#include <smxTransFreeList.h>
#include <smxMinSCNBuild.h>

class smxTransFreeList;
class smrUTransQueue;

#define SMX_GET_SYSTEM_VIEW_SCN( aSCN )                         \
    if ( smxTransMgr::isActiveVersioningMinTime() == ID_FALSE ) \
    {                                                           \
        smmDatabase::getViewSCN( aSCN );                        \
    }                                                           \
    else                                                        \
    {                                                           \
        smxTransMgr::getTimeSCN( aSCN  );                       \
        SM_SET_SCN_VIEW_BIT( aSCN );                            \
    }

#define SMX_GET_MIN_MEM_VIEW( aSCN, aTID )                      \
    if ( smxTransMgr::isActiveVersioningMinTime() == ID_FALSE ) \
    {                                                           \
        smxTransMgr::getMinMemViewSCNofAll( aSCN, aTID );       \
    }                                                           \
    else                                                        \
    {                                                           \
        smxTransMgr::getAgingMemViewSCN( aSCN, aTID );          \
    }

#define SMX_GET_MIN_DISK_VIEW( aSCN )                           \
    if ( smxTransMgr::isActiveVersioningMinTime() == ID_FALSE ) \
    {                                                           \
        smxTransMgr::getSysMinDskViewSCN( aSCN );               \
    }                                                           \
    else                                                        \
    {                                                           \
        smxTransMgr::getAgingDskViewSCN( aSCN );                \
    }



class smxTransMgr
{
//For Operation
public:
    // for idp::update
    enum {CONV_BUFF_SIZE=8};

    static IDE_RC calibrateTransCount(UInt *aTransCnt);
    static IDE_RC initialize();
    static IDE_RC destroy();

    //For Transaction Management
    static IDE_RC alloc(smxTrans **aTrans,
                        idvSQL    *aStatistics = NULL,
                        idBool     aIgnoreRetry = ID_FALSE);
    inline static IDE_RC freeTrans(smxTrans  *aTrans);
    // for layer callback
    static IDE_RC alloc4LayerCall(void** aTrans);
    static IDE_RC freeTrans4LayerCall(void  *aTrans);
    //fix BUG-13175
    static IDE_RC allocNestedTrans(smxTrans  **aTrans);
    static IDE_RC allocNestedTrans4LayerCall(void  **aTrans);


    static inline smxTrans* getTransByTID(smTID aTransID);
    static inline smxTrans* getTransBySID(SInt aSlotN);
    static inline void*     getTransBySID2Void(SInt aSlotN);
    static inline idvSQL*   getStatisticsBySID(SInt aSlotN);
    static inline idBool    isActiveBySID(SInt aSlotN);
    static inline smTID     getTIDBySID(SInt aSlotN);
    static inline void      setCommitLogLSN( smTID aTID, smLSN aLSN ) // BUG-47865
    {
        if (( aTID != SM_NULL_TID ) && ( aTID != 0 ))
        {
            smxTrans* sTrans = getTransByTID( aTID );
            sTrans->setCommitLogLSN( aTID,
                                     aLSN );
        }
    };

    static inline IDE_RC resetBuilderInterval();
    static inline IDE_RC rebuildMinViewSCN( idvSQL * aStatistics );
    static inline void getSysMinDskViewSCN( smSCN* aMinSCN );

    static inline void getSysMinDskFstViewSCN( smSCN* aMinDskFstViewSCN );

    // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
    static inline void getMinOldestFstViewSCN( smSCN* aMinOldestFstViewSCN );

    static void  getMinMemViewSCNofAll( smSCN* aMinSCN,
                                        smTID* aTID,
                                        idBool aUseTimeSCN = ID_FALSE );

    static void  getDskSCNsofAll( smSCN   * aMinViewSCN,
                                  smSCN   * aMinDskFstViewSCN,
                                  smSCN   * aMinOldestFstViewSCN );

    static void getMinLSNOfAllActiveTrans( smLSN  * aLSN );
    static SInt getSlotID( smTID aTransID ){return aTransID & mSlotMask;}

    // for global transaction
    /* BUG-20727 */
    static IDE_RC existPreparedTrans (idBool * aExistFlag );
    static IDE_RC recover( SInt           * aSlotID,
                           /* BUG-18981 */
                           ID_XID         * aXID,
                           timeval        * aPreparedTime,
                           smxCommitState * aState );
    /* BUG-18981 */
    static IDE_RC getXID( smTID aTID, ID_XID  * aXID );
    static IDE_RC rebuildTransFreeList();
#if 0
    static void dump();
    static void dumpActiveTrans();
#endif
    /* BUG-20862 ���� hash resize�� �ϱ� ���ؼ� �ٸ� Ʈ����ǵ��� ��� ��������
     * ���ϰ� �ؾ� �մϴ�.*/
    static void block( void * aTrans, UInt   aTryMicroSec, idBool *aSuccess );
    static void unblock(void);
    
    static void enableTransBegin();
    static void disableTransBegin();
    static idBool  existActiveTrans(smxTrans* aTrans);

    // active transaction list releated function.
    static inline void initATL();
    static inline void addAT( smxTrans *aTrans );
    static inline void removeAT( smxTrans  * aTrans );
    static inline void initATLAnPrepareLst();
    static  void addActiveTrans( void   * aTrans,
                                 smTID    aTID,
                                 UInt     aReplFlag,
                                 idBool   aIsBeginLog,
                                 smLSN  * aCurLSN );
    static  idBool isEmptyActiveTrans();

    // global transaction recovery releated function.
    static inline void initPreparedList();
    static inline void addPreparedList( smxTrans  * aTrans );
    static inline void removePreparedList( smxTrans  *aTrans );
    static IDE_RC setXAInfoAnAddPrepareLst( void    * aTrans,
                                            idBool     aIsGCTx,
                                            timeval   aTimeVal,
                                            ID_XID    aXID, /* BUG-18981 */
                                            smSCN   * aFstDskViewSCN );
    static void rebuildPrepareTransOldestSCN();
    static IDE_RC incRecCnt4InDoubtTrans( smTID aTransID,
                                          smOID aTableOID );
    static IDE_RC decRecCnt4InDoubtTrans( smTID aTransID,
                                          smOID aTableOID );
    static  UInt   getPrepareTransCount();
    static  void*  getFirstPreparedTrx();
    static  void*  getNxtPreparedTrx(void* aTrans);

    static IDE_RC  makeTransEnd( void*    aTrans,
                                 idBool   aIsCommit );

    static IDE_RC  insertUndoLSNs( smrUTransQueue  * aTransQueue );
    static IDE_RC  abortAllActiveTrans();
    static IDE_RC  setRowSCNForInDoubtTrans();

    static IDE_RC verifyIndex4ActiveTrans( idvSQL * aStatistics );

    static inline IDE_RC lock() {return mMutex.lock( NULL /* idvSQL* */ ); }
    static inline IDE_RC unlock() {return mMutex.unlock(); }


    //move from smlLockMgr by recfactoring.
    static IDE_RC waitForLock( void             * aTrans,
                               iduMutex         * aMutex,
                               ULong              aLockWaitMicroSec );

    static idBool isWaitForTransCase( void   * aTrans,
                                      smTID    aWaitTransID );

    static IDE_RC waitForTrans( void      * aTrans,
                                smTID       aWaitTransID,
                                scSpaceID   aSpaceID,
                                ULong       aLockWaitTime );

    static SInt   getCurTransCnt();
    static void*  getTransByTID2Void( smTID aTID );

    static void   getDummySmmViewSCN( smSCN  * aSCN );
    static IDE_RC getDummySmmCommitSCN( void   * aTrans,
                                        idBool   aIsLegacyTrans,
                                        void   * aStatus );
    static void   setSmmCallbacks( );
    static void   unsetSmmCallbacks( );

    static UInt   getActiveTransCount();
    static UInt   getPreparedTransCnt() { return mPreparedTransCnt; }

    /* PROJ-1594 Volatile TBS */
    /* Volatile logging�� �ϱ� ���� �ʿ��� �Լ� */
    static svrLogEnv *getVolatileLogEnv( void *aTrans );

    /* BUG-19245: Transaction�� �ι� Free�Ǵ� ���� Detect�ϱ� ���� �߰��� */
    static void checkFreeTransList();

    /* PROJ-1704 MVCC renwal */
    static IDE_RC addTouchedPage( void      * aTrans,
                                  scSpaceID   aSpaceID,
                                  scPageID    aPageID,
                                  SShort      aCTSlotNum );

    static IDE_RC initializeMinSCNBuilder();
    static IDE_RC startupMinSCNBuilder();
    static IDE_RC destroyMinSCNBuilder();
    static IDE_RC shutdownMinSCNBuilder();

    static idBool existActiveTrans();
    /* BUG-42724 */
    static IDE_RC setOIDFlagForInDoubtTrans();

    static idBool isReplTrans( void * aTrans );
    static idBool isSameReplID( void * aTrans, smTID aWaitTransID );
    static idBool isTransForReplConflictResolution( smTID aWaitTransID );
    /* MM callback ��� �� ȣ���Լ�*/
    static void setSessionCallback( smiSessionCallback * aSessionCallback )
    {   mSessionCallback = *aSessionCallback; };
    static void setAllocTransRetryCount( idvSQL * aStatistics,
                                         ULong    aRetryCount );

    /* PROJ-2733 �л� Ʈ����� ���ռ� */
    static void getTimeSCN( smSCN * aSCN );
    static void getAgingMemViewSCN( smSCN * aSCN, smTID * aTID );
    static void getAgingDskViewSCN( smSCN * aSCN );
    static void getAccessSCN( smSCN  * aSCN );
    static void getTimeSCNList( smxTimeSCNNode ** aList );
    static SInt getTimeSCNBaseIdx( void );
    static SInt getTimeSCNLastIdx( void );
    static SInt getTimeSCNMaxCnt( void );
    static idBool isActiveVersioningMinTime( void );
    static void setGlobalConsistentSCNAsSystemSCN( void );
    static inline IDE_RC resetVersioningMinTime( void );

    /* BUG-48250 */
    static UInt getSessIndoubtFetchTimeout( idvSQL * aStatistics );
    static UInt getSessIndoubtFetchMethod( idvSQL * aStatistics );

    static void registPendingTable( UShort aCurSlotN, UShort aRowTransSlotN );
    static void clearPendingTable( UShort aCurSlotN, UShort aRowTransSlotN );

private:
    //For Versioning
    static iduMutex           mMutex;

//For Member
public:
    
    static smxTransFreeList  *mArrTransFreeList;
    static smxTrans          *mArrTrans;
    static UInt               mTransCnt;
    static UInt               mTransFreeListCnt;
    static UInt               mCurAllocTransFreeList;
    static idBool             mEnabledTransBegin;
    static UInt               mSlotMask;
    //Transaction Active List
    static UInt               mActiveTransCnt;
    static smxTrans           mActiveTrans;
    //Prepared Transaction List for global transaction
    static UInt               mPreparedTransCnt;
    static smxTrans           mPreparedTrans;
    static smxMinSCNBuild     mMinSCNBuilder;

    static smxGetSmmViewSCNFunc   mGetSmmViewSCN;
    static smxGetSmmCommitSCNFunc mGetSmmCommitSCN;

    /* BUG-47655 �� Session�� transaction �Ҵ� ��õ� Ƚ��, ������ */
    /* X$TRANSACTION_MANAGER */
    static ULong              mTransTableFullCount;   /* ��ü ��õ� Ƚ�� */
    static ULong              mAllocRetryTransCount;  /* ��õ��� Transaction�� �� */
    /* X$SESSION */
    static smiSessionCallback mSessionCallback;       /* ������ ��õ� Ƚ�� ��� MM Callback */

    /* PROJ-2733 �л� Ʈ����� ���ռ� */
    static UChar           ** mPendingWait;
};

inline smxTrans* smxTransMgr::getTransByTID(smTID aTransID)
{
    return mArrTrans + getSlotID(aTransID);
}

inline void* smxTransMgr::getTransByTID2Void(smTID aTransID)
{
    return (void*)(mArrTrans + getSlotID(aTransID));
}

inline smxTrans* smxTransMgr::getTransBySID(SInt aSlotN)
{
    return mArrTrans + aSlotN;
}

inline void* smxTransMgr::getTransBySID2Void(SInt aSlotN)
{
    return (void*)(mArrTrans + aSlotN);
}

inline idvSQL* smxTransMgr::getStatisticsBySID(SInt aSlotN)
{
    return mArrTrans[aSlotN].mStatistics;
}

IDE_RC smxTransMgr::freeTrans(smxTrans *aTrans)
{
    IDE_ASSERT(aTrans->mStatus == SMX_TX_END);

    IDE_TEST( aTrans->mTransFreeList->freeTrans( aTrans )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
// active transaction list���� �Լ� ����.
// active transaction list�� circular double link list.
inline void smxTransMgr::initATL()
{
    mActiveTrans.mPrvAT = &mActiveTrans;
    mActiveTrans.mNxtAT = &mActiveTrans;
}
// active transaction list�� add.
inline void smxTransMgr::addAT(smxTrans *aTrans)
{
    aTrans->mPrvAT = &mActiveTrans;
    aTrans->mNxtAT = mActiveTrans.mNxtAT;

    mActiveTrans.mNxtAT->mPrvAT = aTrans;
    mActiveTrans.mNxtAT = aTrans;
    mActiveTransCnt++;
}
// active transaction list���� remove
inline void smxTransMgr::removeAT(smxTrans *aTrans)
{
    aTrans->mPrvAT->mNxtAT = aTrans->mNxtAT;
    aTrans->mNxtAT->mPrvAT = aTrans->mPrvAT;
    mActiveTransCnt--;
}

inline idBool smxTransMgr::isEmptyActiveTrans()
{
    if(mActiveTrans.mNxtAT == &mActiveTrans)
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

// prepared transaction list���� �Լ� ����.
inline void smxTransMgr::initPreparedList()
{
    mPreparedTrans.mPrvPT = &mPreparedTrans;
    mPreparedTrans.mNxtPT = &mPreparedTrans;
}

inline void smxTransMgr::addPreparedList(smxTrans *aTrans)
{
    aTrans->mPrvPT = &mPreparedTrans;
    aTrans->mNxtPT = mPreparedTrans.mNxtPT;
    mPreparedTrans.mNxtPT->mPrvPT = aTrans;
    mPreparedTrans.mNxtPT = aTrans;
    mPreparedTransCnt++;
}

inline void smxTransMgr::removePreparedList(smxTrans *aTrans)
{
    aTrans->mPrvPT->mNxtPT = aTrans->mNxtPT;
    aTrans->mNxtPT->mPrvPT = aTrans->mPrvPT;
    mPreparedTransCnt--;
}

inline void* smxTransMgr::getFirstPreparedTrx()
{
    if (mPreparedTrans.mNxtPT == &mPreparedTrans )
    {
        return NULL;
    }
    else
    {
        return mPreparedTrans.mNxtPT;
    }
}

inline void smxTransMgr::initATLAnPrepareLst()
{
    initATL();
    initPreparedList();
}

inline SInt smxTransMgr::getCurTransCnt()
{
   return mTransCnt;
}

inline UInt smxTransMgr::getActiveTransCount()
{
    return mActiveTransCnt;
}

inline IDE_RC smxTransMgr::allocNestedTrans4LayerCall(void** aTrans)
{
    return smxTransMgr::allocNestedTrans((smxTrans**)aTrans);

}

inline idBool  smxTransMgr::isActiveBySID(SInt aSlotN)
{
    smxTrans *sCurTx;

    sCurTx = getTransBySID(aSlotN);

    return (sCurTx->mStatus != SMX_TX_END ? ID_TRUE : ID_FALSE);
}

inline smTID  smxTransMgr::getTIDBySID(SInt aSlotN)
{
    smxTrans *sCurTx;

    sCurTx = getTransBySID(aSlotN);

    return sCurTx->mTransID;
}

inline svrLogEnv *smxTransMgr::getVolatileLogEnv(void *aTrans)
{
    return ((smxTrans*)aTrans)->getVolatileLogEnv();
}

/* Description : Get MinViewSCN From MinSCNBuilder  */
inline void smxTransMgr::getSysMinDskViewSCN( smSCN* aSysMinDskViewSCN )
{
    mMinSCNBuilder.getMinDskViewSCN( aSysMinDskViewSCN );
}

/* Description : reset Interval of MinSCNBuilder  */
inline IDE_RC smxTransMgr::resetBuilderInterval()
{
    return mMinSCNBuilder.resetInterval();
}

/* PROJ-2733 �л� Ʈ����� ���ռ� */
inline IDE_RC smxTransMgr::resetVersioningMinTime()
{
    return mMinSCNBuilder.resetVersioningMinTime();
}

// BUG-24885 wrong delayed stamping
// get the minimum fstDskViewSCN of all active transactions
inline void smxTransMgr::getSysMinDskFstViewSCN( smSCN* aSysMinDskFstViewSCN )
{
    mMinSCNBuilder.getMinDskFstViewSCN( aSysMinDskFstViewSCN );
}

// BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
// system�� active transaction�� �� �ּ��� oldestViewSCN(AgableSCN)�� ����
inline void smxTransMgr::getMinOldestFstViewSCN( smSCN* aMinOldestFstViewSCN )
{
    mMinSCNBuilder.getMinOldestFstViewSCN( aMinOldestFstViewSCN );
}

/* Description : update MinViewSCN of MinSCNBuilder right now*/
inline IDE_RC smxTransMgr::rebuildMinViewSCN( idvSQL  * aStatistics )
{
    return mMinSCNBuilder.resumeAndWait( aStatistics );
}

inline idBool smxTransMgr::isReplTrans( void * aTrans )
{
    return ((smxTrans*)aTrans)->isReplTrans();
}

inline idBool smxTransMgr::isSameReplID( void   * aTrans,
                                         smTID    aWaitTransID )
{
    smxTrans * sTrans = NULL;
    smxTrans * sWaitTrans = NULL;

    sTrans = (smxTrans *)aTrans;
    sWaitTrans = getTransByTID(aWaitTransID);

    if ( sTrans->mReplID == sWaitTrans->mReplID )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

inline idBool smxTransMgr::isTransForReplConflictResolution( smTID aWaitTransID )
{
    smxTrans * sWaitTrans = NULL;

    sWaitTrans = getTransByTID( aWaitTransID );

    if ( ( sWaitTrans->mFlag & SMI_TRANSACTION_REPL_CONFLICT_RESOLUTION )
         == SMI_TRANSACTION_REPL_CONFLICT_RESOLUTION )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/* PROJ-2733 �л� Ʈ����� ���ռ� */
inline void smxTransMgr::getTimeSCN( smSCN  * aSCN )
{
    smSCN sSCN;
    mMinSCNBuilder.getTimeSCN( &sSCN );

    if ( SM_SCN_IS_INFINITE( sSCN ) )
    {
        /* mAgingViewSCN�� �ʱⰪ �״���̸�, system scn �� �޾ƿ´�. */
        sSCN = smmDatabase::getLstSystemSCN();
    }

    SM_SET_SCN( aSCN, &sSCN );
}

inline void smxTransMgr::getAgingMemViewSCN( smSCN * aSCN, smTID * aTID )
{
    smSCN sSCN;
    smSCN sDummySCN;
    smTID sOldestTID;

    mMinSCNBuilder.getAgingMemViewSCN( &sSCN );

    if ( SM_SCN_IS_INFINITE( sSCN ) )
    {
        /* mAgingViewSCN�� �ʱⰪ �״���̸�, system min view �� �޾ƿ´�. */
        smxTransMgr::getMinMemViewSCNofAll( &sSCN, &sOldestTID ); 
        IDE_DASSERT( SM_SCN_IS_NOT_INFINITE( sSCN ) );
    }
    else
    {
        /* MinSCNBuilder �����忡�� �ֱ���(0.1��)����
           AgingViewSCN�� �����ϴµ� �̶� Oldest TID �� �����߾���.
           ������, Oldest TID�� ������µ��� ������ ���ŵǱ� ������
           Oldest TID�� ��� �����ϴ� ������ �ǴܵǾ�
           Delete Ager���� System SCN�� ��� ������Ű�� ������ �־���.
         
           ����, Oldest TID�� AgingViewSCN�� ��û�Ҷ����� �ٷιٷ� ���ϵ��� �����Ͽ���.
           (����) AgingViewSCN�� AccessSCN���� ũ�� �ȵǱ⶧���� MinSCNBUilder�� ���� �̿��Ͽ��� �Ѵ�. */

        smxTransMgr::getMinMemViewSCNofAll( &sDummySCN, &sOldestTID, ID_TRUE ); 
    }

    SM_SET_SCN( aSCN, &sSCN );
    *aTID = sOldestTID;
}

inline void smxTransMgr::getAgingDskViewSCN( smSCN * aSCN )
{
    smSCN sSCN;

    /* min(DskViewSCN, TimeSCN) */
    mMinSCNBuilder.getAgingDskViewSCN( &sSCN );

    if ( SM_SCN_IS_INFINITE( sSCN ) )
    {
        /* mAgingViewSCN�� �ʱⰪ �״���̸�, system min view �� �޾ƿ´�. */
        smxTransMgr::getSysMinDskViewSCN( &sSCN ); 
    }

    SM_SET_SCN( aSCN, &sSCN );
}

inline void smxTransMgr::getAccessSCN( smSCN * aSCN )
{
    smSCN sSCN;
    mMinSCNBuilder.getAccessSCN( &sSCN );

    if ( SM_SCN_IS_INFINITE( sSCN ) )
    {
        /* mAccessSCN�� �ʱⰪ �״���̸�, system scn �� �޾ƿ´�. */
        sSCN = smmDatabase::getLstSystemSCN();
    }

    SM_SET_SCN( aSCN, &sSCN );
}

inline void smxTransMgr::getTimeSCNList( smxTimeSCNNode ** aList )
{
    mMinSCNBuilder.getTimeSCNList( aList );
}
inline SInt smxTransMgr::getTimeSCNBaseIdx()
{
    return mMinSCNBuilder.getTimeSCNBaseIdx();
}
inline SInt smxTransMgr::getTimeSCNLastIdx()
{
    return mMinSCNBuilder.getTimeSCNLastIdx();
}
inline SInt smxTransMgr::getTimeSCNMaxCnt()
{
    return mMinSCNBuilder.getTimeSCNMaxCnt();
}
inline idBool smxTransMgr::isActiveVersioningMinTime()
{
    return mMinSCNBuilder.isActiveVersioningMinTime();
}
inline void smxTransMgr::setGlobalConsistentSCNAsSystemSCN()
{
    mMinSCNBuilder.setGlobalConsistentSCNAsSystemSCN();
}

/* BUG-48250
   Session Property �� INDOBUT_FETCH_TIMEOUT ���� �����´�. */
inline UInt smxTransMgr::getSessIndoubtFetchTimeout( idvSQL * aStatistics )
{
    UInt sFetchTimeout = (UInt)IDP_SHARD_PROPERTY_INDOUBT_FETCH_TIMEOUT_DEFAULT;

    if ( aStatistics != NULL ) 
    {
        IDE_DASSERT( aStatistics->mSess != NULL );
        IDE_DASSERT( aStatistics->mSess->mSession != NULL );

        sFetchTimeout = smxTransMgr::mSessionCallback.mGetIndoubtFetchTimeout( aStatistics->mSess->mSession );
    }

    return sFetchTimeout;
}

/* BUG-48250
   Session Property �� INDOBUT_FETCH_METHOD ���� �����´�. */
inline UInt smxTransMgr::getSessIndoubtFetchMethod( idvSQL * aStatistics )
{
    UInt sFetchMethod = (UInt)IDP_SHARD_PROPERTY_INDOUBT_FETCH_METHOD_DEFAULT;

    if ( aStatistics != NULL )
    {
        IDE_DASSERT( aStatistics->mSess != NULL );
        IDE_DASSERT( aStatistics->mSess->mSession != NULL );

        sFetchMethod = smxTransMgr::mSessionCallback.mGetIndoubtFetchMethod( aStatistics->mSess->mSession );
    }

    return sFetchMethod;
}

#endif
