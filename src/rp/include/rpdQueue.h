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
 * $Id: rpdQueue.h 90491 2021-04-07 07:02:29Z lswhh $
 **********************************************************************/

#ifndef _O_RPD_QUEUE_H_
#define _O_RPD_QUEUE_H_

#include <idu.h>
#include <iduOIDMemory.h>
#include <qci.h>
#include <rp.h>

/*
typedef struct rpdLogBuffer
{
    UInt          mSize;
    SChar         mAlign[4];
    rpdLogBuffer *mNextPtr;
} rpdLogBuffer;
*/

typedef struct rpdLob
{
    /* LOB Operation Argument */
    ULong       mLobLocator;
    UInt        mLobColumnID;
    UInt        mLobOffset;
    UInt        mLobOldSize;
    UInt        mLobNewSize;
    UInt        mLobPieceLen;
    UChar      *mLobPiece;
} rpdLob;

typedef struct rpdXLog
{
    /* ���� �м����� XLog�� Ÿ�� */
    rpXLogType  mType;
    /* ���� �м����� Log�� Ʈ����� ID */
    smTID       mTID;
    /* ���� �α��� SN */
    smSN        mSN;
    /* SyncSN */
    smSN        mSyncSN;
    /* ���� �м����� Log�� ���̺� OID */
    ULong       mTableOID;

    /* Column ID Array */
    UInt       *mCIDs;
    /* ���� �м����� �α��� Before Image Column Value Array */
    smiValue   *mBCols;
    /* ���� �м����� �α��� After Image Column Value Array */
    smiValue   *mACols;
    /* ���� �м����� �α��� Primary Key Column Count */
    UInt        mPKColCnt;
    /* ���� �м����� �α��� Priamry Key Column Value�� Array */
    smiValue   *mPKCols;

    /* Column�� ���� */
    UInt        mColCnt;

    /* Savepoint �̸��� ���� */
    UInt        mSPNameLen;
    /* Savepoint �̸� */
    SChar      *mSPName;

    /* Flush Option */
    UInt        mFlushOption;

    rpdLob     *mLobPtr;

    /*Implicit SVP Depth*/
    UInt        mImplSPDepth;

    /* Restart SN */
    smSN        mRestartSN;     // BUG-17748

    /* smiValue->value �޸𸮸� ���� */
    iduMemory   mMemory;

    /* Next XLog Pointer */
    rpdXLog    *mNext;

    smSN        mWaitSNFromOtherApplier;
    UInt        mWaitApplierIndex;

    ID_XID      mXID;
    smSCN       mGlobalCommitSCN;
} rpdXLog;

class rpdQueue
{
public:
    IDE_RC initialize(SChar *aRepName);
    void   destroy();

    void   setExitFlag();

    IDE_RC read( rpdXLog **aXLogPtr, UInt  aTimeoutSec );
    void read(rpdXLog **aXLogPtr);
    void write(rpdXLog *aXLogPtr);
    UInt getSize();
    inline IDE_RC lock()   { return mMutex.lock(NULL/*idvSQL* */); }
    inline IDE_RC unlock() { return mMutex.unlock(); }

    static IDE_RC allocXLog(rpdXLog **aXLogPtr, iduMemAllocator * aAllocator);
    static void   freeXLog(rpdXLog *aXLogPtr, iduMemAllocator * aAllocator);
    static IDE_RC initializeXLog( rpdXLog         * aXLogPtr,
                                  ULong             aBufferSize,
                                  idBool            aIsAllocLob,
                                  iduMemAllocator * aAllocator );
    static IDE_RC printXLog(FILE * aFP, rpdXLog *aXLogPtr);
    static void   destroyXLog( rpdXLog * aXLogPtr, iduMemAllocator * aAllocator );
    static void   recycleXLog( rpdXLog * aXLogPtr, iduMemAllocator * aAllocator );

    static void   setWaitCondition( rpdXLog    * aXLog,
                                    UInt         aLastWaitApplierIndex,
                                    smSN         aLastWaitSN );

public:
    iduMutex      mMutex;
    SInt          mXLogCnt;
    rpdXLog      *mHeadPtr;
    rpdXLog      *mTailPtr;
    iduCond       mXLogQueueCV;
    idBool        mWaitFlag;
    idBool        mFinalFlag;
};

#endif
