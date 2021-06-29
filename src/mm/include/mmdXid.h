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

#ifndef _O_MMD_XID_H_
#define _O_MMD_XID_H_ 1

#include <idl.h>
#include <ide.h>
#include <smi.h>
#include <mmcDef.h>
#include <mmdDef.h>
#include <mmdXidManager.h>
#include <mmtSessionManager.h>


class mmcSession;

/* PROJ-1381 FAC : AssocFetchList�� ���� ����ü */
typedef struct AssocFetchListItem
{
    iduList      mFetchList;    /**< Xid�� ���õ� FetchList */
    mmcSession  *mSession;      /**< FetchList�� ������ Session */

    iduListNode  mListNode;     /**< Node for iduList */
} AssocFetchListItem;

class mmdXid
{
private:
    mmcTransObj *mTrans;

    mmdXidMutex *mMutex;
    idBool       mMutexNeedDestroy;

    /* PROJ-1381 Fetch Across Commits */
    iduList     mAssocFetchList;    /**< ���õ� FetchList. commit, rollback���� ó���ؾ� �� �͵� */

    /* fix BUG-35374 To improve scalability about XA, latch granularity of XID hash should be more better than now.
       that is to say , chanage the granularity from global to bucket level.
     */
    iduList     mListNode;

public:
    /* BUG-18981 */
    IDE_RC initialize(ID_XID           *aUserXid,
                      mmcTransObj      *aTrans,
                      mmdXidHashBucket *aBucket);
    IDE_RC finalize(mmdXidHashBucket *aBucket, idBool aFreeTrans);

    IDE_RC attachTrans(UInt aSlotID);
    void   beginTrans(mmcSession *aSession);
    //fix BUG-22651 smrLogFileGroup::updateTransLSNInfo, server FATAL
    IDE_RC prepareTrans(mmcSession *aSession);
    IDE_RC commitTrans(mmcSession *aSession);
    void   rollbackTrans(mmcSession *aSession);
    //fix BUG-22669 need to XID List performance view.
    //fix BUG-23656 session,xid ,transaction�� ������ performance view�� �����ϰ�,
    //�׵鰣�� ���踦 ��Ȯ�� �����ؾ� ��.
    void  associate(mmcSession *aSession);
    void  disAssociate(mmdXaState aState);  //BUG-26164
    // fix BUG-23306 XA ���� ����� �ڽ��� ������ XID�� rollback ó�� �ؾ��մϴ�.
    inline mmcSessID    getAssocSessionID();
    
    // fix BUG-25020 XA�� Active �����ε�, rollback�� ���� 
    // XA_Rollback Timeout���� ��ٸ� �Ŀ� Query Timeout���� ó��
    inline mmcSession  *getAssocSession();

    /* PROJ-1381 Fetch Across Commits */
    IDE_RC addAssocFetchListItem(mmcSession *aSession);

public:
    /* BUG-18981 */
    ID_XID        *getXid();
    iduListNode   *getLstNode();
    mmcTransObj   *getTransPtr();

    mmdXaState  getState();
    void        setState(mmdXaState aState);

    void        fix();
    /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
     XA fix �ÿ� xid list latch�� shared �ɶ� xid fix count ���ռ��� ���� �Լ� */
    void        fixWithLatch();
    void        unfix();
    /* BUG-27968 XA Fix/Unfix Scalability�� �����Ѿ� �մϴ�.
     XA Unfix �ÿ� latch duaration�� ���̱����Ͽ� xid fix-Count�� xid list latch release����
     ���Ѵ�.*/
    void        unfix(UInt *aFixCount);

    void        lock();
    void        unlock();

    mmdXaState  mState;
    //BUG-25078   State�� ����� �ð��� Duration
    UInt        mStateStartTime;     
    ULong       mStateDuration;      
    
    idBool      mBeginFlag;
    UInt        mFixCount;
    //fix BUG-22669 need to XID List performance view. 
    mmcSessID   mAssocSessionID;
    /* BUG-18981 */
    //fix BUG-23656 session,xid ,transaction�� ������ performance view�� �����ϰ�,
    //�׵鰣�� ���踦 ��Ȯ�� �����ؾ� ��.
    ID_XID      mXid;
    
    //BUG-25020
    mmcSession *mAssocSession;
    
    /* BUG-29078 
     * XA_ACTIVE ���¿��� XA_ROLLBACK�� ������ ��쿡 Heuristic XA END�� �����ؾ� �ϸ�,
     * Heuristic XA END�� ó���ؾ��ϴ��� ����
     */
    idBool        mHeuristicXaEnd;
    
public:
    inline static  idBool isNullXid(ID_XID*  aXID);
    inline static  void  initXid(ID_XID*  aXID);
    static UInt    hashFunc(void *aKey);
    static SInt    compFunc(void *aLhs, void *aRhs);
    inline idBool  getHeuristicXaEnd();
    inline void    setHeuristicXaEnd(idBool aHeuristicXaEnd);
};


/* BUG-29078 */
inline idBool mmdXid::getHeuristicXaEnd()
{
    return mHeuristicXaEnd;
}

inline void mmdXid::setHeuristicXaEnd(idBool aHeuristicXaEnd)
{
    mHeuristicXaEnd = aHeuristicXaEnd;
}

inline ID_XID *mmdXid::getXid()
{
    return &mXid;
}

inline iduList *mmdXid::getLstNode()
{
    return &mListNode;
}

inline mmcTransObj *mmdXid::getTransPtr()
{
    return mTrans;
}

inline mmdXaState mmdXid::getState()
{
    return mState;
}

inline void mmdXid::setState(mmdXaState aState)
{
    mState          = aState;
    mStateStartTime = mmtSessionManager::getBaseTime();         //BUG-25078 - State�� ����� �ð� ����
}



inline void mmdXid::fix()
{
    mFixCount++;
}

inline void mmdXid::unfix()
{
    // prvent minus 
    IDE_ASSERT(mFixCount != 0);
    
    mFixCount--;
}

inline void mmdXid::lock()
{
    // do TASK-3873 code-sonar
    IDE_ASSERT(mMutex->mMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
}

inline void mmdXid::unlock()
{
    // do TASK-3873 code-sonar
    IDE_ASSERT(mMutex->mMutex.unlock() == IDE_SUCCESS);
}

// fix BUG-23306 XA ���� ����� �ڽ��� ������ XID�� rollback ó�� �ؾ��մϴ�.
inline mmcSessID mmdXid::getAssocSessionID()
{
    return mAssocSessionID;
}

// fix BUG-25020
inline mmcSession *mmdXid::getAssocSession()
{
    return mAssocSession;
}

inline idBool mmdXid::isNullXid(ID_XID*  aXID)
{
    IDE_ASSERT( aXID != NULL);
    if((aXID->formatID == 0) &&(aXID->gtrid_length == 0) &&  \
       (aXID->bqual_length == 0))
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}        
inline void mmdXid::initXid(ID_XID*  aXID)
{
    idlOS::memset(aXID, 0x00, ID_SIZEOF(ID_XID));
}

#endif
