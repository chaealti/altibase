/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _O_CMP_ARG_RP_H_
#define _O_CMP_ARG_RP_H_ 1


typedef struct cmpArgRPVersion
{
    ULong       mVersion;           // Replication Version
} cmpArgRPVersion;

typedef struct cmpArgRPMetaRepl
{
    cmtVariable mRepName;            // Replication Name(40byte)
    SInt        mItemCnt;            // Replication Table Count
    SInt        mRole;               // Role
    SInt        mConflictResolution; // Conflict resolution
    UInt        mTransTblSize;       // Transaction Table Size
    UInt        mFlags;              // Flags
    UInt        mOptions;            // Option Flags(Recovery 0x0000001)
    ULong       mRPRecoverySN;       // RP Recovery SN for recovery option1
    UInt        mReplMode;
    UInt        mParallelID;
    SInt        mIsStarted;

    /* PROJ-1915 */
    cmtVariable mOSInfo;
    UInt        mCompileBit;
    UInt        mSmVersionID;
    UInt        mLFGCount;
    ULong       mLogFileSize;

    cmtVariable mDBCharSet;         // BUG-23718
    cmtVariable mNationalCharSet;
    cmtVariable mServerID;          // BUG-6093 Server ID �߰�
    cmtVariable mRemoteFaultDetectTime;
} cmpArgRPMetaRepl;

//PROJ-1608 recovery from replication
typedef struct cmpArgRPRequestAck
{
    UInt        mResult;            //OK, NOK
} cmpArgRPRequestAck;

typedef struct cmpArgRPMetaReplTbl
{
    cmtVariable mRepName;           // Replication Name(40byte)
    cmtVariable mLocalUserName;     // Local User Name(40byte)
    cmtVariable mLocalTableName;    // Local Table Name(40byte)
    cmtVariable mLocalPartName;     // Local Part Name(40byte)
    cmtVariable mRemoteUserName;    // Remote User Name(40byte)
    cmtVariable mRemoteTableName;   // Remote Table Name(40byte)
    cmtVariable mRemotePartName;    // Remote Table Name(40byte)
    cmtVariable mPartCondMinValues; // Partition Condition Minimum Values(variable size)
    cmtVariable mPartCondMaxValues; // Partition Condition Maximum Values(variable size)
    UInt        mPartitionMethod;   // Partition Method
    UInt        mPartitionOrder;    // Partition Order ( hash partitioned table only )
    ULong       mTableOID;          // Local Table OID
    UInt        mPKIndexID;         // Primary Key Index ID
    UInt        mPKColCnt;          // Primary Key Column Count
    SInt        mColumnCnt;         // Columnt count
    SInt        mIndexCnt;          // Index count
    /* PROJ-1915 Invalid Max SN ���� */
    ULong       mInvalidMaxSN;
    cmtVariable mConditionStr;
} cmpArgRPMetaReplTbl;

typedef struct cmpArgRPMetaReplCol
{
    cmtVariable mColumnName;        // Column Name(40byte)
    UInt        mColumnID;          // Column ID
    UInt        mColumnFlag;        // Column Flag
    UInt        mColumnOffset;      // Fixed record������ column offset
    UInt        mColumnSize;        // Column Size
    UInt        mDataTypeID;        // Column�� Data type ID
    UInt        mLangID;            // Column�� Language ID
    UInt        mFlags;             // mtcColumn�� flag
    SInt        mPrecision;         // Column�� precision
    SInt        mScale;             // Column�� scale

    // PROJ-2002 Column Security
    // echar, evarchar ���� ���� Ÿ���� ����ϴ� ���
    // �Ʒ� �÷������� �߰��ȴ�.
    SInt        mEncPrecision;      // ���� Ÿ���� precision
    cmtVariable mPolicyName;        // ���� Ÿ�Կ� ���� policy name
    cmtVariable mPolicyCode;        // ���� Ÿ�Կ� ���� policy code
    cmtVariable mECCPolicyName;     // ���� Ÿ�Կ� ���� ecc policy name
    cmtVariable mECCPolicyCode;     // ���� Ÿ�Կ� ���� ecc policy code
} cmpArgRPMetaReplCol;

typedef struct cmpArgRPMetaReplIdx
{
    cmtVariable mIndexName;         // Index Name(40byte)
    UInt        mIndexID;           // Index ID
    UInt        mIndexTypeID;       // Index Type ID
    UInt        mKeyColumnCnt;      // Index�� �����ϴ� column ����
    UInt        mIsUnique;          // Unique index ����
    UInt        mIsRange;           // Range ����
} cmpArgRPMetaReplIdx;

typedef struct cmpArgRPMetaReplIdxCol
{
    UInt        mColumnID;          // Index�� �����ϴ� column ID
    UInt        mKeyColumnFlag;     // Index�� sort ����(ASC/DESC)
} cmpArgRPMetaReplIdxCol;

typedef struct cmpArgRPHandshakeAck
{
    UInt        mResult;            // ����/���� ���
    SInt        mFailbackStatus;
    ULong       mXSN;               // restart SN
    cmtVariable mMsg;               // ���� �޼���(1024 byte)
} cmpArgRPHandshakeAck;

/* PROJ-1663�� ���� ������ ���� */
typedef struct cmpArgRPTrBegin
{
    UInt        mXLogType;          // RP_X_BEGIN
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPTrBegin;

typedef struct cmpArgRPTrCommit
{
    UInt        mXLogType;          // RP_X_COMMIT
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPTrCommit;

typedef struct cmpArgRPTrAbort
{
    UInt        mXLogType;          // RP_X_ABORT
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPTrAbort;

typedef struct cmpArgRPSPSet
{
    UInt        mXLogType;          // RP_X_SP_SET
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mSPNameLen;         // Savepoint Name Length
    cmtVariable mSPName;            // Savepoint Name
} cmpArgRPSPSet;

typedef struct cmpArgRPSPAbort
{
    UInt        mXLogType;          // RP_X_SP_ABORT
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mSPNameLen;         // Savepoint Name Length
    cmtVariable mSPName;            // Savepoint Name
} cmpArgRPSPAbort;

typedef struct cmpArgRPStmtBegin
{
    UInt        mXLogType;          // RP_X_STMT_BEGIN
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mStmtID;            // Statement ID
} cmpArgRPStmtBegin;

typedef struct cmpArgRPStmtEnd
{
    UInt        mXLogType;          // RP_X_STMT_END
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mStmtID;            // Statement ID
} cmpArgRPStmtEnd;

typedef struct cmpArgRPCursorOpen
{
    UInt        mXLogType;          // RP_X_CURSOR_OPEN
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mCursorID;          // Cursor ID
} cmpArgRPCursorOpen;

typedef struct cmpArgRPCursorClose
{
    UInt        mXLogType;          // RP_X_CURSOR_CLOSE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mCursorID;          // Cursor ID
} cmpArgRPCursorClose;

typedef struct cmpArgRPInsert
{
    UInt        mXLogType;          // RP_X_INSERT
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mImplSPDepth;
    ULong       mTableOID;          // Insert ��� Table OID
    UInt        mColCnt;            // Insert Column Count
} cmpArgRPInsert;

typedef struct cmpArgRPUpdate
{
    UInt        mXLogType;          // RP_X_UPDATE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mImplSPDepth;
    ULong       mTableOID;          // Update ��� Table OID
    UInt        mPKColCnt;          // Primary Key Column Count
    UInt        mUpdateColCnt;      // Update�� �߻��� Column ����
} cmpArgRPUpdate;

typedef struct cmpArgRPDelete
{
    UInt        mXLogType;          // RP_X_DELETE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mImplSPDepth;
    ULong       mTableOID;          // Delete ��� Table OID
    UInt        mPKColCnt;          // Primary Key Column Count
} cmpArgRPDelete;

typedef struct cmpArgRPUIntID
{
    UInt        mUIntID;            // Column ID : array�� �������
} cmpArgRPUIntID;

typedef struct cmpArgRPValue
{
    cmtVariable mValue;             // ���� Value
} cmpArgRPValue;

typedef struct cmpArgRPStop
{
    UInt        mXLogType;          // RP_X_REPL_STOP
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mRestartSN;         // BUG-17748 : Restart SN
} cmpArgRPStop;

typedef struct cmpArgRPKeepAlive
{
    UInt        mXLogType;          // RP_X_KEEP_ALIVE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mRestartSN;         // BUG-17748 : Restart SN
} cmpArgRPKeepAlive;

typedef struct cmpArgRPFlush
{
    UInt        mXLogType;          // RP_X_FLUSH
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    UInt        mOption;            // Flush Option
} cmpArgRPFlush;

typedef struct cmpArgRPFlushAck
{
    UInt        mXLogType;          // RP_X_FLUSH_ACK
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPFlushAck;

typedef struct cmpArgRPAck
{
    UInt        mXLogType;          // RP_X_ACK, RP_X_STOP_ACK, RP_X_HANDSHAKE_READY
    UInt        mAbortTxCount;      // Abort Transaction Count
    UInt        mClearTxCount;      // Clear Transaction Count
    ULong       mRestartSN;         // Transaction Table's Minimum SN
    ULong       mLastCommitSN;      // ���������� Commit�� �α��� SN
    ULong       mLastArrivedSN;     // Receiver�� ������ �α��� ������ SN (Acked Mode)
    ULong       mLastProcessedSN;   // �ݿ��� �Ϸ��� ������ �α��� SN     (Eager Mode)
    ULong       mFlushSN;           // ��ũ�� Flush�� SN
    //cmpArgRPTxAck�� �̿��� Abort Transaction List�� ����
    //cmpArgRPTxAck�� �̿��� Clear Transaction List�� ����
} cmpArgRPAck;

typedef struct cmpArgRPTxAck
{
    UInt        mTID;               // Transaction ID
    ULong       mSN;                // XLog�� SN
} cmpArgRPTxAck;

typedef struct cmpArgRPLobCursorOpen
{
    UInt        mXLogType;          // RP_X_LOB_CURSOR_OPEN
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mTableOID;          // LOB cursor open ��� Table OID
    ULong       mLobLocator;        // LOB locator : �ĺ��ڷ� ���
    UInt        mColumnID;          // LOB column ID
    UInt        mPKColCnt;
} cmpArgRPLobCursorOpen;

typedef struct cmpArgRPLobCursorClose
{
    UInt        mXLogType;          // RP_X_LOB_CURSOR_CLOSE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mLobLocator;        // Close�� LOB locator
} cmpArgRPLobCursorClose;

typedef struct cmpArgRPLobPrepare4Write
{
    UInt        mXLogType;          // RP_X_PREPARE4WRITE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mLobLocator;        // Write�� LOB locator
    UInt        mOffset;            // ���� ���� offset
    UInt        mOldSize;           // ���� �� size
    UInt        mNewSize;           // ���� �� size
} cmpArgRPLobPrepare4Write;

typedef struct cmpArgRPLobPartialWrite
{
    UInt        mXLogType;          // RP_X_LOB_PARTIAL_WRITE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mLobLocator;        // Write�� LOB locator
    UInt        mOffset;            // Piece�� Write�� LOB ���� ��ġ
    UInt        mPieceLen;          // Piece�� ����
    cmtVariable mPieceValue;        // Piece�� value
} cmpArgRPLobPartialWrite;

typedef struct cmpArgRPLobFinish2Write
{
    UInt        mXLogType;          // RP_X_LOB_FINISH2WRITE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mLobLocator;        // Write�� ���� LOB locator
} cmpArgRPLobFinish2Write;

typedef struct cmpArgRPLobTrim
{
    UInt        mXLogType;          // RP_X_LOB_TRIM
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mLobLocator;        // Trim�� LOB locator
    UInt        mOffset;            // Trim offset
} cmpArgRPLobTrim;

typedef struct cmpArgRPHandshake
{
    UInt        mXLogType;          // RP_X_HANDSHAKE
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPHandshake;

typedef struct cmpArgRPSyncPKBegin
{
    UInt        mXLogType;          // RP_X_SYNC_PK_BEGIN
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPSyncPKBegin;

typedef struct cmpArgRPSyncPK
{
    UInt        mXLogType;          // RP_X_SYNC_PK
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
    ULong       mTableOID;          // Delete ��� Table OID
    UInt        mPKColCnt;          // Primary Key Column Count
} cmpArgRPSyncPK;

typedef struct cmpArgRPSyncPKEnd
{
    UInt        mXLogType;          // RP_X_SYNC_PK_END
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPSyncPKEnd;

typedef struct cmpArgRPFailbackEnd
{
    UInt        mXLogType;          // RP_X_FAILBACK_END
    UInt        mTransID;           // Transaction ID
    ULong       mSN;                // �α��� SN
    ULong       mSyncSN;
} cmpArgRPFailbackEnd;

typedef struct cmpArgRPSyncTableNumber
{
    UInt        mSyncTableNumber;
} cmpArgRPSyncTableNumber;

typedef struct cmpArgRPSyncStart
{
    UInt        mXLogType;          /* RP_X_SYNC_START */
    UInt        mTransID;           /* Transaction ID  */
    ULong       mSN;                /* �α��� SN      */
} cmpArgRPSyncStart;

typedef struct cmpArgRPSyncEnd
{
    UInt        mXLogType;          /* RP_X_DROP_INDEX */
    UInt        mTransID;           /* Transaction ID  */
    ULong       mSN;                /* �α��� SN      */
} cmpArgRPSyncEnd;

typedef union cmpArgRP
{
    cmpArgRPVersion          mVersion;
    cmpArgRPMetaRepl         mMetaRepl;
    cmpArgRPMetaReplTbl      mMetaReplTbl;
    cmpArgRPMetaReplCol      mMetaReplCol;
    cmpArgRPMetaReplIdx      mMetaReplIdx;
    cmpArgRPMetaReplIdxCol   mMetaReplIdxCol;
    cmpArgRPHandshakeAck     mHandshakeAck;
    cmpArgRPTrBegin          mTrBegin;
    cmpArgRPTrCommit         mTrCommit;
    cmpArgRPTrAbort          mTrAbort;
    cmpArgRPSPSet            mSPSet;
    cmpArgRPSPAbort          mSPAbort;
    cmpArgRPStmtBegin        mStmtBegin;
    cmpArgRPStmtEnd          mStmtEnd;
    cmpArgRPCursorOpen       mCursorOpen;
    cmpArgRPCursorClose      mCursorClose;
    cmpArgRPInsert           mInsert;
    cmpArgRPUpdate           mUpdate;
    cmpArgRPDelete           mDelete;
    cmpArgRPUIntID           mUIntID;
    cmpArgRPValue            mValue;
    cmpArgRPStop             mStop;
    cmpArgRPKeepAlive        mKeepAlive;
    cmpArgRPFlush            mFlush;
    cmpArgRPFlushAck         mFlushAck;
    cmpArgRPAck              mAck;
    cmpArgRPLobCursorOpen    mLobCursorOpen;
    cmpArgRPLobCursorClose   mLobCursorClose;
    cmpArgRPLobPrepare4Write mLobPrepare4Write;
    cmpArgRPLobPartialWrite  mLobPartialWrite;
    cmpArgRPLobFinish2Write  mLobFinish2Write;
    cmpArgRPTxAck            mTxAck;
    cmpArgRPRequestAck       mRequestAck;
    cmpArgRPHandshake        mHandshake;
    cmpArgRPSyncPKBegin      mSyncPKBegin;
    cmpArgRPSyncPK           mSyncPK;
    cmpArgRPSyncPKEnd        mSyncPKEnd;
    cmpArgRPFailbackEnd      mFailbackEnd;
    cmpArgRPSyncTableNumber  mSyncTableNumber;
    cmpArgRPSyncStart        mSyncStart;
    cmpArgRPSyncEnd          mSyncEnd;
    cmpArgRPLobTrim          mLobTrim;
} cmpArgRP;

#endif
