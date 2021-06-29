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

#ifndef _O_CMP_ARG_RP_CLIENT_H_
#define _O_CMP_ARG_RP_CLIENT_H_ 1


typedef struct cmpArgRPVersion
{
    acp_uint64_t mVersion;           // Replication Version
} cmpArgRPVersion;

typedef struct cmpArgRPMetaRepl
{
    cmtVariable  mRepName;            // Replication Name(40byte)
    acp_sint32_t mItemCnt;            // Replication Table Count
    acp_sint32_t mRole;               // Role
    acp_sint32_t mConflictResolution; // Conflict resolution
    acp_uint32_t mTransTblSize;       // Transaction Table Size
    acp_uint32_t mFlags;              // Flags
    acp_uint32_t mOptions;            // Option Flags(Recovery 0x0000001)
    acp_uint64_t mRPRecoverySN;       // RP Recovery SN for recovery option1
    acp_uint32_t mReplMode;
    acp_uint32_t mParallelID;
    acp_sint32_t mIsStarted;

    /* PROJ-1915 */
    cmtVariable  mOSInfo;
    acp_uint32_t mCompileBit;
    acp_uint32_t mSmVersionID;
    acp_uint32_t mLFGCount;
    acp_uint64_t mLogFileSize;

    cmtVariable  mDBCharSet;         // BUG-23718
    cmtVariable  mNationalCharSet;
    cmtVariable  mServerID;          // BUG-6093 Server ID �߰�
    cmtVariable  mRemoteFaultDetectTime;
} cmpArgRPMetaRepl;

//PROJ-1608 recovery from replication
typedef struct cmpArgRPRequestAck
{
    acp_uint32_t mResult;            //OK, NOK
} cmpArgRPRequestAck;

typedef struct cmpArgRPMetaReplTbl
{
    cmtVariable  mRepName;           // Replication Name(40byte)
    cmtVariable  mLocalUserName;     // Local User Name(40byte)
    cmtVariable  mLocalTableName;    // Local Table Name(40byte)
    cmtVariable  mLocalPartName;     // Local Part Name(40byte)
    cmtVariable  mRemoteUserName;    // Remote User Name(40byte)
    cmtVariable  mRemoteTableName;   // Remote Table Name(40byte)
    cmtVariable  mRemotePartName;    // Remote Table Name(40byte)
    cmtVariable  mPartCondMinValues; // Partition Condition Minimum Values(variable size)
    cmtVariable  mPartCondMaxValues; // Partition Condition Maximum Values(variable size)
    acp_uint32_t mPartitionMethod;   // Partition Method
    acp_uint32_t mPartitionOrder;    // Partition Order ( hash partitioned table only )
    acp_uint64_t mTableOID;          // Local Table OID
    acp_uint32_t mPKIndexID;         // Primary Key Index ID
    acp_uint32_t mPKColCnt;          // Primary Key Column Count
    acp_sint32_t mColumnCnt;         // Columnt count
    acp_sint32_t mIndexCnt;          // Index count
    /* PROJ-1915 Invalid Max SN ���� */
    acp_uint64_t mInvalidMaxSN;
    cmtVariable  mConditionStr;
} cmpArgRPMetaReplTbl;

typedef struct cmpArgRPMetaReplCol
{
    cmtVariable  mColumnName;        // Column Name(40byte)
    acp_uint32_t mColumnID;          // Column ID
    acp_uint32_t mColumnFlag;        // Column Flag
    acp_uint32_t mColumnOffset;      // Fixed record������ column offset
    acp_uint32_t mColumnSize;        // Column Size
    acp_uint32_t mDataTypeID;        // Column�� Data type ID
    acp_uint32_t mLangID;            // Column�� Language ID
    acp_uint32_t mFlags;             // mtcColumn�� flag
    acp_sint32_t mPrecision;         // Column�� precision
    acp_sint32_t mScale;             // Column�� scale

    // PROJ-2002 Column Security
    // echar, evarchar ���� ���� Ÿ���� ����ϴ� ���
    // �Ʒ� �÷������� �߰��ȴ�.
    acp_sint32_t mEncPrecision;      // ���� Ÿ���� precision
    cmtVariable  mPolicyName;        // ���� Ÿ�Կ� ���� policy name
    cmtVariable  mPolicyCode;        // ���� Ÿ�Կ� ���� policy code
    cmtVariable  mECCPolicyName;     // ���� Ÿ�Կ� ���� ecc policy name
    cmtVariable  mECCPolicyCode;     // ���� Ÿ�Կ� ���� ecc policy code
} cmpArgRPMetaReplCol;

typedef struct cmpArgRPMetaReplIdx
{
    cmtVariable  mIndexName;         // Index Name(40byte)
    acp_uint32_t mIndexID;           // Index ID
    acp_uint32_t mIndexTypeID;       // Index Type ID
    acp_uint32_t mKeyColumnCnt;      // Index�� �����ϴ� column ����
    acp_uint32_t mIsUnique;          // Unique index ����
    acp_uint32_t mIsRange;           // Range ����
} cmpArgRPMetaReplIdx;

typedef struct cmpArgRPMetaReplIdxCol
{
    acp_uint32_t mColumnID;          // Index�� �����ϴ� column ID
    acp_uint32_t mKeyColumnFlag;     // Index�� sort ����(ASC/DESC)
} cmpArgRPMetaReplIdxCol;

typedef struct cmpArgRPHandshakeAck
{
    acp_uint32_t mResult;            // ����/���� ���
    acp_sint32_t mFailbackStatus;
    acp_uint64_t mXSN;
    cmtVariable  mMsg;               // ���� �޼���(1024 byte)
} cmpArgRPHandshakeAck;

/* PROJ-1663�� ���� ������ ���� */
typedef struct cmpArgRPTrBegin
{
    acp_uint32_t mXLogType;          // RP_X_BEGIN
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPTrBegin;

typedef struct cmpArgRPTrCommit
{
    acp_uint32_t mXLogType;          // RP_X_COMMIT
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPTrCommit;

typedef struct cmpArgRPTrAbort
{
    acp_uint32_t mXLogType;          // RP_X_ABORT
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPTrAbort;

typedef struct cmpArgRPSPSet
{
    acp_uint32_t mXLogType;          // RP_X_SP_SET
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mSPNameLen;         // Savepoint Name Length
    cmtVariable  mSPName;            // Savepoint Name
} cmpArgRPSPSet;

typedef struct cmpArgRPSPAbort
{
    acp_uint32_t mXLogType;          // RP_X_SP_ABORT
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mSPNameLen;         // Savepoint Name Length
    cmtVariable  mSPName;            // Savepoint Name
} cmpArgRPSPAbort;

typedef struct cmpArgRPStmtBegin
{
    acp_uint32_t mXLogType;          // RP_X_STMT_BEGIN
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mStmtID;            // Statement ID
} cmpArgRPStmtBegin;

typedef struct cmpArgRPStmtEnd
{
    acp_uint32_t mXLogType;          // RP_X_STMT_END
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mStmtID;            // Statement ID
} cmpArgRPStmtEnd;

typedef struct cmpArgRPCursorOpen
{
    acp_uint32_t mXLogType;          // RP_X_CURSOR_OPEN
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mCursorID;          // Cursor ID
} cmpArgRPCursorOpen;

typedef struct cmpArgRPCursorClose
{
    acp_uint32_t mXLogType;          // RP_X_CURSOR_CLOSE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mCursorID;          // Cursor ID
} cmpArgRPCursorClose;

typedef struct cmpArgRPInsert
{
    acp_uint32_t mXLogType;          // RP_X_INSERT
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mImplSPDepth;
    acp_uint64_t mTableOID;          // Insert ��� Table OID
    acp_uint32_t mColCnt;            // Insert Column Count
} cmpArgRPInsert;

typedef struct cmpArgRPUpdate
{
    acp_uint32_t mXLogType;          // RP_X_UPDATE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mImplSPDepth;
    acp_uint64_t mTableOID;          // Update ��� Table OID
    acp_uint32_t mPKColCnt;          // Primary Key Column Count
    acp_uint32_t mUpdateColCnt;      // Update�� �߻��� Column ����
} cmpArgRPUpdate;

typedef struct cmpArgRPDelete
{
    acp_uint32_t mXLogType;          // RP_X_DELETE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mImplSPDepth;
    acp_uint64_t mTableOID;          // Delete ��� Table OID
    acp_uint32_t mPKColCnt;          // Primary Key Column Count
} cmpArgRPDelete;

typedef struct cmpArgRPUIntID
{
    acp_uint32_t mUIntID;            // Column ID : array�� �������
} cmpArgRPUIntID;

typedef struct cmpArgRPValue
{
    cmtVariable  mValue;             // ���� Value
} cmpArgRPValue;

typedef struct cmpArgRPStop
{
    acp_uint32_t mXLogType;          // RP_X_REPL_STOP
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mRestartSN;         // BUG-17748 : Restart SN
} cmpArgRPStop;

typedef struct cmpArgRPKeepAlive
{
    acp_uint32_t mXLogType;          // RP_X_KEEP_ALIVE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mRestartSN;         // BUG-17748 : Restart SN
} cmpArgRPKeepAlive;

typedef struct cmpArgRPFlush
{
    acp_uint32_t mXLogType;          // RP_X_FLUSH
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint32_t mOption;            // Flush Option
} cmpArgRPFlush;

typedef struct cmpArgRPFlushAck
{
    acp_uint32_t mXLogType;          // RP_X_FLUSH_ACK
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPFlushAck;

typedef struct cmpArgRPAck
{
    acp_uint32_t mXLogType;          // RP_X_ACK, RP_X_STOP_ACK, RP_X_HANDSHAKE_READY
    acp_uint32_t mAbortTxCount;      // Abort Transaction Count
    acp_uint32_t mClearTxCount;      // Clear Transaction Count
    acp_uint64_t mRestartSN;         // Transaction Table's Minimum SN
    acp_uint64_t mLastCommitSN;      // ���������� Commit�� �α��� SN
    acp_uint64_t mLastArrivedSN;     // Receiver�� ������ �α��� ������ SN (Acked Mode)
    acp_uint64_t mLastProcessedSN;   // �ݿ��� �Ϸ��� ������ �α��� SN     (Eager Mode)
    acp_uint64_t mFlushSN;           // ��ũ�� Flush�� SN
    //cmpArgRPTxAck�� �̿��� Abort Transaction List�� ����
    //cmpArgRPTxAck�� �̿��� Clear Transaction List�� ����
} cmpArgRPAck;

typedef struct cmpArgRPTxAck
{
    acp_uint32_t mTID;               // Transaction ID
    acp_uint64_t mSN;                // XLog�� SN

} cmpArgRPTxAck;

typedef struct cmpArgRPLobCursorOpen
{
    acp_uint32_t mXLogType;          // RP_X_LOB_CURSOR_OPEN
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mTableOID;          // LOB cursor open ��� Table OID
    acp_uint64_t mLobLocator;        // LOB locator : �ĺ��ڷ� ���
    acp_uint32_t mColumnID;          // LOB column ID
    acp_uint32_t mPKColCnt;
} cmpArgRPLobCursorOpen;

typedef struct cmpArgRPLobCursorClose
{
    acp_uint32_t mXLogType;          // RP_X_LOB_CURSOR_CLOSE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mLobLocator;        // Close�� LOB locator
} cmpArgRPLobCursorClose;

typedef struct cmpArgRPLobPrepare4Write
{
    acp_uint32_t mXLogType;          // RP_X_PREPARE4WRITE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mLobLocator;        // Write�� LOB locator
    acp_uint32_t mOffset;            // ���� ���� offset
    acp_uint32_t mOldSize;           // ���� �� size
    acp_uint32_t mNewSize;           // ���� �� size
} cmpArgRPLobPrepare4Write;

typedef struct cmpArgRPLobPartialWrite
{
    acp_uint32_t mXLogType;          // RP_X_LOB_PARTIAL_WRITE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mLobLocator;        // Write�� LOB locator
    acp_uint32_t mOffset;            // Piece�� Write�� LOB ���� ��ġ
    acp_uint32_t mPieceLen;          // Piece�� ����
    cmtVariable  mPieceValue;        // Piece�� value
} cmpArgRPLobPartialWrite;

typedef struct cmpArgRPLobFinish2Write
{
    acp_uint32_t mXLogType;          // RP_X_LOB_FINISH2WRITE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mLobLocator;        // Write�� ���� LOB locator
} cmpArgRPLobFinish2Write;

typedef struct cmpArgRPLobTrim
{
    acp_uint32_t mXLogType;          // RP_X_LOB_TRIM
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mLobLocator;        // Trim�� LOB locator
    acp_uint32_t mOffset;            // Trim offset
} cmpArgRPLobTrim;

typedef struct cmpArgRPHandshake
{
    acp_uint32_t mXLogType;          // RP_X_HANDSHAKE
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPHandshake;

typedef struct cmpArgRPSyncPKBegin
{
    acp_uint32_t mXLogType;          // RP_X_SYNC_PK_BEGIN
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPSyncPKBegin;

typedef struct cmpArgRPSyncPK
{
    acp_uint32_t mXLogType;          // RP_X_SYNC_PK
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
    acp_uint64_t mTableOID;          // Delete ��� Table OID
    acp_uint32_t mPKColCnt;          // Primary Key Column Count
} cmpArgRPSyncPK;

typedef struct cmpArgRPSyncPKEnd
{
    acp_uint32_t mXLogType;          // RP_X_SYNC_PK_END
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPSyncPKEnd;

typedef struct cmpArgRPFailbackEnd
{
    acp_uint32_t mXLogType;          // RP_X_FAILBACK_END
    acp_uint32_t mTransID;           // Transaction ID
    acp_uint64_t mSN;                // �α��� SN
    acp_uint64_t mSyncSN;
} cmpArgRPFailbackEnd;

typedef struct cmpArgRPSyncTableNumber
{
    acp_uint32_t mSyncTableNumber;
} cmpArgRPSyncTableNumber;

typedef struct cmpArgRPSyncStart
{
    acp_uint32_t mXLogType;          /* RP_X_DROP_INDEX */
    acp_uint32_t mTransID;           /* Transaction ID  */
    acp_uint64_t mSN;                /* �α��� SN      */
} cmpArgRPSyncStart;

typedef struct cmpArgRPSyncEnd
{
    acp_uint32_t mXLogType;          /* RP_X_DROP_INDEX */
    acp_uint32_t mTransID;           /* Transaction ID  */
    acp_uint64_t mSN;                /* �α��� SN      */
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
