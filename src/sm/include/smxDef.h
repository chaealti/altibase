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
 * $Id: smxDef.h 90177 2021-03-11 04:29:45Z lswhh $
 **********************************************************************/

#ifndef _O_SMX_DEF_H_
# define _O_SMX_DEF_H_ 1

# include <smDef.h>
# include <svrDef.h>
# include <sdpDef.h>

// smxTrans.mFlag (0x00000030)
# define SMX_REPL_MASK         SMI_TRANSACTION_REPL_MASK
# define SMX_REPL_DEFAULT      SMI_TRANSACTION_REPL_DEFAULT
# define SMX_REPL_NONE         SMI_TRANSACTION_REPL_NONE
# define SMX_REPL_RECOVERY     SMI_TRANSACTION_REPL_RECOVERY
# define SMX_REPL_REPLICATED   SMI_TRANSACTION_REPL_REPLICATED
# define SMX_REPL_PROPAGATION  SMI_TRANSACTION_REPL_PROPAGATION

//---------------------------------------------------------------------
// BUG-15396
// COMMIT WRITE WAIT type
// - SMX_COMMIT_WRITE_WAIT
//   commit �ÿ� log�� disk�� �ݿ��ɶ����� ��ٸ�
//   ���� durability ����
// - SMX_COMMIT_WRITE_NOWAIT
//   commit �ÿ� log�� disk�� �ݿ��ɶ����� ��ٸ��� ����
//   ���� durability ���� �� ��
// ps ) SMI_COMMIT_WRITE_MASK ���� ����
//---------------------------------------------------------------------
// smxTrans.mFlag (0x00000100)
# define SMX_COMMIT_WRITE_MASK    SMI_COMMIT_WRITE_MASK
# define SMX_COMMIT_WRITE_NOWAIT  SMI_COMMIT_WRITE_NOWAIT
# define SMX_COMMIT_WRITE_WAIT    SMI_COMMIT_WRITE_WAIT

# define SMX_MUTEX_LIST_NODE_POOL_SIZE (10)
# define SMX_PRIVATE_BUCKETCOUNT  (smuProperty::getPrivateBucketCount())

// BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
// �����ϱ� ���� transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
// Ư�� segment entry�� �Ҵ����� �ʰ� segment free list����
// round-robin ������� �Ҵ��ϴ� ����� ���(���� ���)
# define SMX_AUTO_BINDING_TRANSACTION_SEGMENT_ENTRY (512)

// Trans Lock Flag
# define SMX_TRANS_UNLOCKED (0)
# define SMX_TRANS_LOCKED   (1)

// Savepoint Name For PSM
#define SMX_PSM_SAVEPOINT_NAME "$$PSM_SVP"
/**
 * Ʈ������� partial rollback�� �����Ҷ�, partial rollback�� ����� �Ǵ�
 * oidList���� mFlag�� ���ؼ� mAndMask�� And�ϰ�, mOrMask�� or�Ѵ�.
 * mAndMask�� �����Ǿ��ִ� flag�߿� ��� �����ϱ� ���� ���̰�,
 * mOrMask�� �����Ǿ��ִ� flag�߿� ��� �߰��ϰ��� �ϱ� ���� ���δ�.
 * �ٷ� AGER LIST�� �Ŵ��� �ʰ�, �� �ٷ� ó������ �ʰ� ���� Ʈ������� commit�ϰų�,
 * rollback�Ҷ� ó���ϰ� �ȴ�.
 * */
typedef struct smxOidSavePointMaskType
{
    UInt mAndMask;
    UInt mOrMask;
}
smxOidSavePointMaskType;

extern const smxOidSavePointMaskType smxOidSavePointMask[SM_OID_OP_COUNT];

typedef enum
{
    SMX_TX_BEGIN,
    SMX_TX_PRECOMMIT, /* BUG-47367 Tx�� commitSCN�� ȹ���ϱ� ���� ���� */
    SMX_TX_COMMIT_IN_MEMORY, // not used
    SMX_TX_COMMIT,
    SMX_TX_ABORT,
    SMX_TX_BLOCKED,
    SMX_TX_END,
    SMX_TX_PREPARED,  /* PROJ-2733 XA_PREPARED �� ���� */
} smxStatus;

struct smcTableHeader;

// For Global Transaction Commit State
typedef smiCommitState smxCommitState;

typedef struct smxOIDInfo
{
    smOID            mTableOID;
    smOID            mTargetOID;
    smSCN            mSCN; /* BUG-47367 FreeSlotList�� ���� SCN�� �ʿ���. */
    UInt             mFlag;
    scSpaceID        mSpaceID;
    UShort           mAlign; 
} smxOIDInfo;

typedef struct smxOIDNode
{
    smxOIDNode      *mPrvNode;
    smxOIDNode      *mNxtNode;
    UInt             mOIDCnt;
    UInt             mAlign;
    smxOIDInfo       mArrOIDInfo[1];
} smxOIDNode;

typedef struct smxOIDNodeHead
{
    smxOIDNode      *mPrvNode;
    smxOIDNode      *mNxtNode;
    UInt             mOIDCnt;
    UInt             mAlign;
} smxOIDNodeHead;


/*---------------------------------------------------------------*/
//  Save Point
/*---------------------------------------------------------------*/
#define SMX_SAVEPOINT_POOL_SIZE   (256)
#define SMX_DEF_SAVEPOINT_COUNT   (10)
#define SMX_IMPLICIT_SVP_INDEX    (0)
#define SMX_MAX_SVPNAME_SIZE      (256)

typedef struct smxSavepoint
{
    smLSN           mUndoLSN;

    /* PROJ-1594 Volatile TBS */
    /* volatile TBS ���� rollback�� ���� �ʿ��� */
    svrLSN          mVolatileLSN;

    /* BUG-15096: non-autocommit��忡�� select�Ϸ��� IS_LOCK�� �����Ǹ�
       ���ڽ��ϴ�.: Lock������ Transaction�� Lock Slot�� Lock Sequence��
       mLockSequence���� Ŭ������ Lock�� �����Ѵ�. */
    ULong           mLockSequence;
    SChar           mName[SMX_MAX_SVPNAME_SIZE];
    smxOIDNode     *mOIDNode;
    SInt            mOffset;
    smxSavepoint   *mPrvSavepoint;
    smxSavepoint   *mNxtSavepoint;

    /* For only implicit savepoint */
    /* Savepoint�� ���� Statement�� Depth */
    UInt            mStmtDepth;
    /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
     * Partial Rollback�� �����ؾ� �մϴ�.
     *
     * Replication�� Transaction�� Implicit Savepoint�� ��ġ��
     * �ٸ���. ������ Replication�� ���ؼ� ��ϵǴ� Savepoint Abort�α�
     * ��Ͻ� Savepoint�� �̸��� ���� Abort�� ������ Savepoint��
     * �ٸ� ��찡 �����Ѵ�.
     *
     * ex) T1: No Replication , T2: Replication
     * + Stmt1 begin
     *    - update T1(#1)
     *   + Stmt1-1 begin
     *      + Stmt1-1-1 begin
     *        - update T2(#2) <---- Replication SVP(Depth = 3)
     *      + Stmt1-1-1 end(Success)
     *      update T2(#3) <---- Replication SVP(Depth = 2)
     *   + Stmt1-1 end(Failure !!)
     *
     *   �� �������� Stmt1-1�� Abort�����Ƿ� Abort�α׶��� Stmt1-1��
     *   ��ϵǾ�� �ϳ� �׷��� �Ǹ� Standby���� #3�� Rollback�ȴ�.
     *   �ֳĸ� Depth=2��� Abort�αװ� ������ Dept=2�� �ش��ϴ�
     *   Implicit SVP�� #3���� ������ �Ǿ��� �����̴�. �̴�
     *   Stmt begin�� ù��° Replication ���� Table�� ����
     *   Update�� �� �α׸� Replication Begin�α׷� �����ϱ⶧���̴�.
     *   �� ������ �ذ��ϱ� ���ؼ� mReplImpSvpStmtDepth�� �߰��ߴ�.
     *   mReplImpSvpStmtDepth�� Partial Abort�� Implicit SVP Abort
     *   �α� ��Ͻ� SVP�� �̸��� mReplImpSvpStmtDepth���� �����Ѵ�.
     *   ������ �߻��ϴ� ������ ������ ���� mReplImpSvpStmtDepth��
     *   �̿������ν� �ذ�� �� �ִ�.
     *
     * ex) T1: No Replication , T2: Replication
     * 1: + Stmt1 begin : Set SVP1
     * 2:   - update T1(#1)
     * 3:  + Stmt1-1 begin : Set SVP2
     * 4:     + Stmt1-1-1 begin : Set SVP3
     * 5:       - update T2(#2) <---- Replication SVP(Depth = 3)
     * 6:     + Stmt1-1-1 end(Success) : Remove SVP3
     * 7:     update T2(#3) <---- 5line���� �ڱ�տ� Replication Implicit
     *                            savepoint�� �������� ���� �κ��� ��μ����߱�
     *                            ������ ���⼭ ������ �ʿ䰡 ����.
     * 8:  + Stmt1-1 end(Failure !!)
     *
     * Implicit SVP�� Set�ɶ����� Transaction�� Implicit SVP List��
     * �߰��ȴ�.
     *
     * + SMI_STATEMENT_DEPTH_NULL = 0
     * + SVP(#1, #2) :
     *    - SVP1 <-- Savepoint Name,
     *    - #1: Stmt Depth, #2: mReplImpSvpStmtDepth
     *
     * Trans: SVP1(1, 0)  + SVP2(2, 0) + SVP3(3, 0)
     * �̰��� line 4���� ������������ ����̴�. ���⼭ line5�� �����ϰ�
     * �Ǹ�
     * Trans: SVP1(1, 1)  + SVP2(2, 1) + SVP3(3, 1)
     * ���� �����ȴ�. Replication Savepoint�� �����ɶ� �տ��ִ� SVP����
     * ���� ū�� + 1�� �ڽ��� mReplImpSvpStmtDepth������ �����Ѵ�.
     * �׸��� �ڽ��� ���� SVP���� ã�ư��鼭 0�� �ƴϰ��� ���� SVP��
     * ���ö����� 0�ΰ��� ���� SVP�� ��� �ڽ��� mReplImpSvpStmtDepth������
     * ������ �ش�.
     * �� ���������� SVP3�� ���� SVP�� ���� ū���� 0�̱� ������ SVP3��
     * mReplImpSvpStmtDepth���� (0 + 1)���� �����Ѵ�. �׸��� SVP1��
     * SVP2�� 0�̹Ƿ� �̰��� �ڽ��� mReplImpSvpStmtDepth�� ���� 1��
     * ��� �ٲپ� �ش�. �ֳĸ� SVP2���� �ѹ��� Replication SVP�� ��������
     * �ʾƼ� ���� SVP2�� Abort�ȴٸ� �����δ� SVP3���� Rollback������
     * Replication�� �̸� ���� ���ϰ� �Ǳ� �����̴�.
     * Partial Rollback�ϰ� �Ǹ� mReplImpSvpStmtDepth���� Partial Abort
     * Log�� ������ش�.
     * �ٽ� line 6�� �����ϰԵǸ� SVP3�� ���ŵȴ�.
     * Trans: SVP1(1, 1)  + SVP2(2, 1)
     * �ٽ� line8�� �����ϰ� �Ǹ� SVP2�� �����ϰ� Implicit SVP Abort�α�
     * ��Ͻ� �̸��� Depth=1�� �����ϰ� �ȴ�.
     * */

    UInt            mReplImpSvpStmtDepth;

    struct smiDDLTargetTableInfo * mDDLTargetTableInfo;
} smxSavepoint;


// memory , disk view scn set callback func
typedef void   (*smxTrySetupViewSCNFuncs)( void  * aTrans, smSCN * aSCN );
typedef void   (*smxGetSmmViewSCNFunc)   ( smSCN * aSCN);
typedef IDE_RC (*smxGetSmmCommitSCNFunc) ( void   * aTrans,
                                           idBool   aIsLegacyTrans,
                                           void   * aStatus );


// PRJ-1496
#define SMX_TABLEINFO_CACHE_COUNT (10)
#define SMX_TABLEINFO_HASHTABLE_SIZE (10)

//PROJ-1362
 //fix BUG-21311
#define SMX_LOB_MIN_BUCKETCOUNT  (5)

typedef struct smxTableInfo
{
    smOID         mTableOID;
    scPageID      mDirtyPageID;
    SLong         mRecordCnt;

    /*
     * PROJ-1671 Bitmap-based Tablespace And
     *           Segment Space Management
     * Table Segment���� ������ Slot�� �Ҵ��ߴ�
     * Data �������� PID�� Cache�Ѵ�.
     */
    scPageID      mHintDataPID;
    idBool        mExistDPathIns;

    smxTableInfo *mPrevPtr;
    smxTableInfo *mNextPtr;
} smxTableInfo;

struct sdpSegMgmtOp;

// PROJ-1553 Replication self-deadlock

#define SMX_NOT_REPL_TX_ID       (ID_SINT_MAX)
#define SMX_LOCK_WAIT_REPL_TX_ID (ID_SINT_MAX - 1)

/* BUG-27709 receiver�� ���� �ݿ� ��, Ʈ����� alloc���н� �ش� receiver ���� */
#define SMX_IGNORE_TX_ALLOC_TIMEOUT (0)

// Transaction�� Log Bufferũ�⸦ �׻� �̰����� Align��Ų��.
#define SMX_TRANSACTION_LOG_BUFFER_ALIGN_SIZE (1024)

// Transaction�� �ʱ� Bufferũ��.
#define SMX_TRANSACTION_LOG_BUFFER_INIT_SIZE  (2048)

/***********************************************************************
 *
 * PROJ-1704 Disk MVCC ������
 *
 * Touch Page List ���� �ڷᱸ�� ����
 *
 * Ʈ������� ������ ��ũ ������ ����
 *
 ***********************************************************************/
typedef struct smxTouchInfo
{
    scPageID         mPageID;      // ���ŵ� �������� PageID
    scSpaceID        mSpaceID;     // ���ŵ� �������� SpaceID
    SShort           mCTSlotIdx;   // ���ŵ� ���������� �Ҵ�� CTS ����
} smxTouchInfo;

/*
 * ���ŵ� �������� ���������� �����ϴ� ��� ����
 * �޸� �Ҵ�/���� ������ �����
 * �����Ǵ� ������ TOUCH_PAGE_CNT_BY_NODE �� ���ؼ� �����ȴ�
 */
typedef struct smxTouchNode
{
    smxTouchNode   * mNxtNode;
    UInt             mPageCnt;
    UInt             mAlign;
    smxTouchInfo     mArrTouchInfo[1];
} smxTouchNode;

/*
 * smxTouchPage Hash�� �˻��� ��������� ���Ǵ� flag
 */

#define SMX_TPH_PAGE_NOT_FOUND     (-2)
#define SMX_TPH_PAGE_EXIST_UNKNOWN (-1)
#define SMX_TPH_PAGE_FOUND         (0)

/*
 * Transaction ���� X$ ��� �ڷᱸ�� ����
 */
typedef struct smxTransInfo4Perf
{
    smTID              mTransID;
    //fix BUG-23656 session,xid ,transaction�� ������ performance view�� �����ϰ�,
    //�׵鰣�� ���踦 ��Ȯ�� �����ؾ� ��.
    //transaction���� session id�� �߰�. 
    UInt               mSessionID;
    smSCN              mMscn;
    smSCN              mDscn;
    smSCN              mFstDskViewSCN;
    // BUG-24821 V$TRANSACTION�� LOB���� MinSCN�� ��µǾ�� �մϴ�. 
    smSCN              mMinMemViewSCNwithLOB;
    smSCN              mMinDskViewSCNwithLOB;
    smSCN              mLastRequestSCN;
    smSCN              mPrepareSCN;
    smSCN              mCommitSCN;
    smxStatus          mStatus;
    idBool             mIsUpdate; // durable writing�� ����
    UInt               mLogTypeFlag;
    ID_XID             mXaTransID;
    smxCommitState     mCommitState;
    timeval            mPreparedTime;
    smLSN              mFstUndoNxtLSN;
    smLSN              mCurUndoNxtLSN;
    smLSN              mLstUndoNxtLSN;
    smSN               mCurUndoNxtSN;
    SInt               mSlotN;
    ULong              mUpdateSize;
    idBool             mAbleToRollback;
    UInt               mFstUpdateTime;
    /* BUG-33895 [sm_recovery] add the estimate function 
     * the time of transaction undoing. */
    UInt               mProcessedUndoTime;
    ULong              mTotalLogCount;
    ULong              mProcessedUndoLogCount;
    UInt               mEstimatedTotalUndoTime;
    UInt               mLogBufferSize;
    UInt               mLogOffset;
    idBool             mDoSkipCheck;
    idBool             mDoSkipCheckSCN;
    idBool             mIsDDL;
    sdSID              mTSSlotSID;
    UInt               mRSGroupID;
    idBool             mDPathInsExist;
    UInt               mMemLobCursorCount;
    UInt               mDiskLobCursorCount;
    UInt               mLegacyTransCount;
    // BUG-40213 Add isolation level in V$TRANSACTION
    UInt               mIsolationLevel;

    /* PROJ-2734 */
    PDL_Time_Value     mGCTxFirstStmtTime;
    smSCN              mGCTxFirstStmtViewSCN;
    smiDistLevel       mDistLevel;
    smiShardPin        mShardPin;
    /* PROJ-2733 */
    idBool             mIsGCTx; //GlobalConsistentTx:GLOBAL_TRANSACTION_LEVEL=3 ���� ������ Tx
    
    SChar              mDetectionStr[64];
    ULong              mDieWaitTime;
    ULong              mElapsedTime;

} smxTransInfo4Perf;

/*
 * Transaction Segment ���� X$ ��� �ڷᱸ�� ����
 */
typedef struct smxTXSeg4Perf
{
    UInt               mEntryID;
    smTID              mTransID;
    smSCN              mMinDskViewSCN;
    smSCN              mCommitSCN;
    smSCN              mFstDskViewSCN;
    sdSID              mTSSlotSID;
    sdRID              mExtRID4TSS;
    sdRID              mFstExtRID4UDS;
    sdRID              mLstExtRID4UDS;
    scPageID           mFstUndoPID;
    scPageID           mLstUndoPID;
    scSlotNum          mFstUndoSlotNum;
    scSlotNum          mLstUndoSlotNum;
} smxTXSeg4Perf;

/* PROJ-1381 Fetch Across Commits
 * FAC commit���� smxTrans�� ���纻 */
typedef struct smxLegacyTrans
{
    smTID       mTransID;
    smLSN       mCommitEndLSN;
    void      * mOIDList;       /* smxOIDList * */
    smSCN       mCommitSCN;
    smSCN       mMinMemViewSCN;
    smSCN       mMinDskViewSCN;
    smSCN       mFstDskViewSCN;
    UChar       mMadeType;
    iduMutex    mWaitForNoAccessAftDropTbl; /* BUG-42760 */
} smxLegacyTrans;

typedef struct smxLegacyStmt
{
    void * mLegacyTrans;  /* smxLegacyTrans * */
    void * mStmtListHead; /* smiStatement *   */
} smxLegacyStmt;

#endif
