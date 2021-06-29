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
 * $id$
 **********************************************************************/

#ifndef _O_DKT_REMOTE_TX_H_
#define _O_DKT_REMOTE_TX_H_ 1

#include <idTypes.h>
#include <dkt.h>
#include <dktRemoteStmt.h>
#include <sdi.h>

class dktRemoteTx
{
private:
    idBool          mIsTimedOut;

    /*
    UShort          mLinkObjId;
    */
    UShort          mRemoteNodeSessionId;
    SShort          mStmtIdBase;

    UInt            mLinkerSessionId;
    UInt            mId;
    UInt            mGlobalTxId;
    UInt            mLocalTxId;
    dktRTxStatus    mStatus;            /* remote transaction's status */
    UInt            mStmtCnt;

    SLong           mCurRemoteStmtId;
    SLong           mNextRemoteStmtId;

    SChar           mTargetName[DK_NAME_LEN + 1];
    dktLinkerType   mLinkerType;
    sdiConnectInfo *mDataNode;          /* shard node */

    iduList         mRemoteStmtList;    /* remote statement list */
    iduList         mSavepointList;     /* savepoint list */

    iduMutex        mDktRStmtMutex;

public:

    iduListNode  mNode;
    ID_XID       mXID;
    idBool       mIsXAStarted;

    /* >> BUG-37487 */
    /* Initialize / Finalize */
    IDE_RC          initialize( UInt            aSessionId,
                                SChar          *aTargetName,
                                dktLinkerType   aLinkerType,
                                sdiConnectInfo *aDataNode,
                                UInt            aGlobalTxId,
                                UInt            aLocalTxId,
                                UInt            aRTxId );

    void            finalize();

    /* Remote statement list �� �����Ѵ�. */
    void            destroyAllRemoteStmt(); 

    /* Savepoint list �� �����Ѵ�. */
    void            deleteAllSavepoint();
    /* << BUG-37487 */

    /* ���ο� remote statement �� �ϳ� �����Ͽ� list �� �߰��Ѵ�. */
    IDE_RC          createRemoteStmt( UInt             aStmtType,
                                      SChar           *aStmtStr,
                                      dktRemoteStmt  **aRemoteStmt );

    /* �ش��ϴ� remote statement �� remote statement list �κ��� �����Ѵ�. */
    /* BUG-37487 */
    void            destroyRemoteStmt( dktRemoteStmt   *aRemoteStmt ); 

    /* list ���� �ش� id �� ���� remote statement node �� ã�� ��ȯ�Ѵ�. */
    dktRemoteStmt*  findRemoteStmt( SLong    aRemoteStmtId );

    /* �� remote transaction object �� remote statement �� ������ �˻��Ѵ�. */
    idBool          isEmptyRemoteTx();

    /* Remote statement id �� �����Ѵ�. */
    SLong           generateRemoteStmtId();

    /* Savepoint ���� �� �˻� */
    IDE_RC          setSavepoint( const SChar   *aSavepointName );

    dktSavepoint*   findSavepoint( const SChar   *aSavepointName );

    /* BUG-37487 */
    void            removeSavepoint( const SChar  *aSavepointName );

    /* BUG-37512 */
    void            removeAllNextSavepoint( const SChar *aSavepointName );

    /* PV: �� remote transaction �� ������ ��´�. */
    IDE_RC          getRemoteTxInfo( dktRemoteTxInfo    *aInfo );

    IDE_RC          getRemoteStmtInfo( dktRemoteStmtInfo    *aInfo,
                                       UInt                  aStmtCnt,
                                       UInt                 *aInfoCnt );

    IDE_RC          freeAndDestroyAllRemoteStmt( dksSession *aSession, UInt  aSessionId );
    inline UInt     getRemoteStmtCount();

    /* mIsPrepared �� ���� Ȯ���Ѵ�. */
    inline idBool   isPrepared();

    inline void     setRemoteTransactionStatus( UInt aStatus );
    inline UInt     getRemoteTransactionStatus();

    
    /* Functions for setting and getting member value */
    inline void     setStatus( dktRTxStatus aStatus );
    inline dktRTxStatus    getStatus();

    inline void     setRemoteNodeSessionId( UShort aId );
    inline UShort   getRemoteNodeSessionId();

    inline UInt     getRemoteTransactionId();

    inline SChar*   getTargetName();
    inline dktLinkerType   getLinkerType();
    inline sdiConnectInfo *getDataNode();
};

inline UInt     dktRemoteTx::getRemoteStmtCount()
{
    UInt sStmtCnt = 0;

    IDE_ASSERT( mDktRStmtMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    sStmtCnt = mStmtCnt;

    IDE_ASSERT( mDktRStmtMutex.unlock() == IDE_SUCCESS );

    return sStmtCnt;
}

inline void dktRemoteTx::setStatus( dktRTxStatus aStatus )
{
    mStatus = aStatus;
}

inline dktRTxStatus dktRemoteTx::getStatus()
{
    return mStatus;
}

inline void dktRemoteTx::setRemoteNodeSessionId( UShort  aId )
{
    mRemoteNodeSessionId = aId;
}

inline UShort dktRemoteTx::getRemoteNodeSessionId()
{
    return mRemoteNodeSessionId;
}

inline UInt dktRemoteTx::getRemoteTransactionId()
{
    return mId;
}

inline SChar*   dktRemoteTx::getTargetName()
{
    return mTargetName;
}

inline dktLinkerType dktRemoteTx::getLinkerType()
{
    return mLinkerType;
}

inline sdiConnectInfo * dktRemoteTx::getDataNode()
{
    return mDataNode;
}

#endif /* _O_DKT_REMOTE_TX_H_ */
