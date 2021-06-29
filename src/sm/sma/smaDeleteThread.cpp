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
 * $Id: smaDeleteThread.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smx.h>
#include <smcLob.h>
#include <sma.h>
#include <svcRecord.h>
#include <smi.h>
#include <sgmManager.h>
#include <smr.h>
#include <smlLockMgr.h>
#include <svm.h>

iduMutex           smaDeleteThread::mMutex;
ULong              smaDeleteThread::mHandledCnt; // atomicInc ���
smaDeleteThread*   smaDeleteThread::mDeleteThreadList;
UInt               smaDeleteThread::mThreadCnt;
smxTrans**         smaDeleteThread::mTrans;
smxTrans*          smaDeleteThread::mTrans4Self;
ULong              smaDeleteThread::mAgingProcessedOIDCnt; // AtomicInc ���
ULong              smaDeleteThread::mSleepCountOnAgingCondition; // lock ȹ���Ŀ��� ������
iduMutex *         smaDeleteThread::mListLock;
UInt               smaDeleteThread::mIsParallelMode;
iduMutex           smaDeleteThread::mCheckMutex4Self;

smaDeleteThread::smaDeleteThread() : idtBaseThread()
{

}


IDE_RC smaDeleteThread::initializeStatic()
{
    UInt i;

    mHandledCnt = 0;
    mAgingProcessedOIDCnt = 0;
    mDeleteThreadList = NULL;

    mIsParallelMode = smuProperty::getParallelDeleteThread();

    IDE_TEST(mMutex.initialize( (SChar*)"DELETE_THREAD_MUTEX",
                                IDU_MUTEX_KIND_POSIX,
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS);

    IDE_TEST(mCheckMutex4Self.initialize((SChar*)"DELETE_THREAD_CHECK_MUTEX",
                                         IDU_MUTEX_KIND_POSIX,
                                         IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    mThreadCnt = smuProperty::getDeleteAgerCount();

    /* TC/FIT/Limit/sm/sma/smaDeleteThread_alloc_malloc.sql */
    IDU_FIT_POINT_RAISE( "smaDeleteThread::alloc::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMA,
                               (ULong)ID_SIZEOF(smaDeleteThread) * mThreadCnt,
                               (void**)&mDeleteThreadList) != IDE_SUCCESS,
                   insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                      (ULong)ID_SIZEOF( smxTrans* ) * mThreadCnt,
                                      (void**)&mTrans ) != IDE_SUCCESS,
                   insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(iduMutex) * smaLogicalAger::mListCnt ,
                                       (void**)&(mListLock)) != IDE_SUCCESS,
                    insufficient_memory );

    for ( i = 0; i < smaLogicalAger::mListCnt ; i++ )
    {
        IDE_TEST( mListLock[i].initialize( (SChar*)"DELETE_THREAD_MUTEX",
                                           IDU_MUTEX_KIND_POSIX,
                                           IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    }

    for(i = 0; i < mThreadCnt; i++)
    {
        new (mDeleteThreadList + i) smaDeleteThread;
        IDE_TEST(mDeleteThreadList[i].initialize( i ) != IDE_SUCCESS);
    }

    IDE_TEST(smxTransMgr::alloc(&mTrans4Self) != IDE_SUCCESS);

    mTrans4Self->mConnectDeleteThread = &mCheckMutex4Self;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaDeleteThread::destroyStatic()
{
    UInt i;

    mTrans4Self->mConnectDeleteThread = NULL;
    IDE_TEST(smxTransMgr::freeTrans( mTrans4Self ) != IDE_SUCCESS);

    for( i = 0; i < smaLogicalAger::mListCnt; i++ )
    {
        IDE_TEST( mListLock[i].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free( mListLock )
              != IDE_SUCCESS );
    mListLock = NULL;

    for(i = 0; i < mThreadCnt; i++)
    {
        IDE_TEST(mDeleteThreadList[i].destroy() != IDE_SUCCESS);
    }

    IDE_TEST( mCheckMutex4Self.destroy() != IDE_SUCCESS );

    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    if(mDeleteThreadList != NULL)
    {
        IDE_TEST(iduMemMgr::free(mDeleteThreadList)
                 != IDE_SUCCESS);
        mDeleteThreadList = NULL;
    }

    if ( mTrans != NULL )
    {
        IDE_TEST( iduMemMgr::free( mTrans )
                  != IDE_SUCCESS );
        mTrans = NULL;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaDeleteThread::shutdownAll()
{
    UInt i;

    for(i = 0; i < mThreadCnt; i++)
    {
        IDE_TEST(mDeleteThreadList[i].shutdown() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaDeleteThread::initialize( UInt aThreadID )
{

    mFinished  = ID_FALSE;
    mHandled   = ID_FALSE;
    mSleepCountOnAgingCondition = 0;
    mAgingCntAfterATransBegin   = 0;

    mTimeOut   = smuProperty::getAgerWaitMax();
    mThreadID  = aThreadID;

    mTrans[mThreadID] = NULL;

    IDE_TEST(mCheckMutex.initialize((SChar*)"DELETE_THREAD_CHECK_MUTEX",
                                    IDU_MUTEX_KIND_POSIX,
                                    IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    IDE_TEST(smxTransMgr::alloc( &mTrans[mThreadID]) != IDE_SUCCESS);

    mTrans[mThreadID]->mConnectDeleteThread = &mCheckMutex;

    IDE_TEST( start() != IDE_SUCCESS );

    IDE_TEST(waitToStart(0) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smaDeleteThread::destroy()
{
    mTrans[mThreadID]->mConnectDeleteThread = NULL;
    IDE_TEST(smxTransMgr::freeTrans(mTrans[mThreadID]) != IDE_SUCCESS);

    IDE_TEST( mCheckMutex.destroy() != IDE_SUCCESS );

    mTrans[mThreadID] = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Table Drop�� Commit�Ŀ� Transaction�� ���� �����ϱ⶧����
 *               Ager�� Drop Table�� ���ؼ� Aging�۾��� �ϸ� �ȵȴ�. �̸� ����
 *               Ager�� �ش� Table�� ���� Aging�۾��� �ϱ����� Table�� Drop��
 *               ������ �����Ѵ�. �׷��� �� Table�� Drop�Ǿ����� Check�ϰ� ������
 *               �� Table�� Transaction�� ���ؼ� Drop�� �� �ֱ� ������ �������ȴ�.
 *               �̶����� Ager�� ���� ����� mCheckMutex�� �ɰ� �۾��� �����Ѵ�.
 *               ���� Table Drop�� ������ Transaction�� mCheckMutex�� ��Ҵٴ�
 *               �̾߱�� �ƹ��� Ager�� Table�� �������� �ʰ� �ִٴ� ���� �����ϰ�
 *               ���� �� ���Ŀ� Ager�� �����Ҷ��� ���̺� Set�� Drop Flag��
 *               ���� �Ǿ� Table�� ���� Aging�۾��� �������� �ʴ´�.
 *
 * ���� BUG: 15047
 **********************************************************************/
void smaDeleteThread::waitForNoAccessAftDropTbl()
{
    UInt i;

    for ( i = 0; i < mThreadCnt; i++ )
    {
        IDE_ASSERT( mDeleteThreadList[i].lockCheckMtx()
                    == IDE_SUCCESS);
        IDE_ASSERT( mDeleteThreadList[i].unlockCheckMtx() 
                    == IDE_SUCCESS);
    }

    IDE_ASSERT( mCheckMutex4Self.lock( NULL ) == IDE_SUCCESS );
    IDE_ASSERT( mCheckMutex4Self.unlock()     == IDE_SUCCESS );
   
}

void smaDeleteThread::run()
{

    PDL_Time_Value      sTimeOut;
    UInt                sState = 0;
    UInt  sAgerWaitMin;
    UInt  sAgerWaitMax;
    UInt   sListN   = mThreadID % smaLogicalAger::mListCnt ;
    idBool sHandled = ID_FALSE;

  startPos :

    sState = 0;

    sAgerWaitMax = smuProperty::getAgerWaitMax();
    sAgerWaitMin = smuProperty::getAgerWaitMin();

    while(mFinished != ID_TRUE)
    {
        /* BUG-47601: ���� ��ȸ �ϰ� ���� Sleep�ѹ�.. */
        if ( sListN == (mThreadID % smaLogicalAger::mListCnt) )
        {
            if ( sHandled == ID_TRUE )
            {
                mTimeOut >>= 1;
                mTimeOut = ( mTimeOut < sAgerWaitMin ) ? sAgerWaitMin : mTimeOut;
                sHandled = ID_FALSE;
            }
            else
            {
                mTimeOut <<= 1;
                mTimeOut = ( mTimeOut > sAgerWaitMax ) ? sAgerWaitMax : mTimeOut;
            }

            if ( mTimeOut > sAgerWaitMin )
            {
                sTimeOut.set(0, mTimeOut);
                idlOS::sleep(sTimeOut);
            }
        }

        if ( smuProperty::isRunMemDeleteThread() == SMU_THREAD_OFF )
        {
            // To Fix PR-14783
            // System Thread�� �۾��� �������� �ʵ��� �Ѵ�.
            mTimeOut = sAgerWaitMax;
            continue;
        }
        else
        {
            // Go Go
        }

        IDU_FIT_POINT("1.smaDeleteThread::run");

        /* realDelete ���ο��� DeleteThread���� ���ü� ��� �Ѵ�. */
        //Delete All Record and Drop Table
        IDE_TEST(realDelete(ID_FALSE, sListN) != IDE_SUCCESS);

        if( mHandled == ID_TRUE )
        {
            sHandled = ID_TRUE; 
        }

        /* ���� list�� Ž���Ϸ� ���� */
        sListN++;

        if (sListN >= smaLogicalAger::mListCnt )
        {
            sListN = 0;
        }
    }

    IDE_TEST(lock() != IDE_SUCCESS);
    sState = 1;

    /* aging thread�� �����Ǵ� �ϴ� �� ���ƺ���. */
    for ( sListN = 0; sListN < smaLogicalAger::mListCnt ; sListN++ )
    {
        IDE_TEST(realDelete(ID_TRUE, sListN) != IDE_SUCCESS);
    }

    sState = 0;
    IDE_TEST(unlock() != IDE_SUCCESS);

    return;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        (void)unlock();
    }

    IDE_WARNING(SM_TRC_LOG_LEVEL_WARNNING,
                SM_TRC_MAGER_WARNNING);

    goto startPos;

}

/*
 * Aging OID List�� ���ؼ� Aging�� �����Ѵ�. �� �Լ��� ũ��
 * �ΰ��� ��Ȳ���� Aging ��� ���ؼ� Aging�� �����Ѵ�.
 *
 * 1. Server�� ����ÿ� aDeleteAll�� ID_TRUE�� �Ѿ�ͼ�
 *    Aging OID List�� ���ؼ� ��� Aging������ �����Ѵ�.
 *
 * 2. OID List Header�� mKeyFreeSCN��
 *
 *    MinViewSCN = MIN( system scn + 1, MIN( Transaction���� View SCN )
 *
 *   2.1 MinViewSCN���� ������
 *
 *   2.2 MinViewSCN�� ����, Transaction�� View SCN�� ���� ���
 *     - system scn + 1�� ���� , Transaction���� ��� View�� ���� ����
 *       ���� ��� �� List�� � Transaction�� ���� �ʴ´ٴ� ���� �����ϱ�
 *       ������ Aging�� �� �ִ�.
 *
 *  Data Structure
 *   - <smaOidList ( mNext ) >  -> < smaOidList ( mNext ) >  ->  ...
 *      -  smxOIDNode* mHead         - smxOIDNode* mHead
 *      -  smxOIDNode* mTail         - smxOIDNode* mTail
 *
 *   - smaOIDList�� Header�̰� Header���� mNext�� �̿��ؼ� Linked List�� ����
 *     �Ѵ�.
 *   - smaOIDList�� mHead, mTail�� Node����Ʈ�� Head�� Tail�� ����Ų��.
 */
IDE_RC smaDeleteThread::realDelete( idBool aDeleteAll, SInt aListN )
{
    smaOidList         * sCurOIDHead        = NULL;
    smSCN                sMinViewSCN;
    smSCN                sSystemSCN;
    smTID                sOldestTID;
    smaOidList         * sLogicalAgerHead   = NULL;
    SInt                 sListN = aListN;
    idBool               sState             = ID_FALSE;
    idBool               sIncreaseSystemSCN = ID_FALSE;
    
    mHandled = ID_FALSE;

//    IDE_TEST( mListLock[sListN].lock(NULL) !=IDE_SUCCESS );
//    sState = ID_TRUE;
    mListLock[sListN].trylock( sState );

    if( sState != ID_TRUE )
    {
        IDE_CONT( skip_list );
    }

    if( smaLogicalAger::mTailDeleteThread[sListN] != NULL )
    {
        /* BUG-19308: Server Stop�� Delete Thread�� Aging�۾��� �Ϸ����� �ʰ�
         *             �����մϴ�.
         *
         * aDeleteAll = ID_TRUE�̸� ������ Aging�� �ϵ��� �մϴ�. */

        if( ( smaLogicalAger::mTailDeleteThread[sListN]->mErasable == ID_TRUE ) ||
            ( aDeleteAll == ID_TRUE ) )
        {
            /* Transaction�� Minimum View SCN�� ���Ѵ�. */
            ID_SERIAL_BEGIN(
                    sLogicalAgerHead = smaLogicalAger::mTailLogicalAger[sListN] );
            ID_SERIAL_END(
                    SMX_GET_MIN_MEM_VIEW( &sMinViewSCN, &sOldestTID ) );

            beginATrans();

            while( 1 )
            {
                sCurOIDHead = smaLogicalAger::mTailDeleteThread[sListN];

                /* �ش� Node�� Aging�� �����ص� �Ǵ��� �����Ѵ�. */
                if( isAgingTarget( sCurOIDHead,
                                   aDeleteAll,
                                   &sMinViewSCN )
                    == ID_FALSE )
                {
                    break;
                }

                smaLogicalAger::mTailDeleteThread[sListN] = sCurOIDHead->mNext;

                if ( mIsParallelMode == SMA_PARALLEL_DELETE_THREAD_ON )
                {
                    /* ó���� OID�� ���������� lock�� Ǯ���ش�. */
                    sState = ID_FALSE;
                    IDE_TEST( mListLock[sListN].unlock() != IDE_SUCCESS );
                }

                /* sCurOIDHead�� �޴޷� �ִ� OID Node List�� ���ؼ�
                   Aging�� �����Ѵ�. */
                IDE_ASSERT( processAgingOIDNodeList( sCurOIDHead, aDeleteAll )
                            == IDE_SUCCESS );

                if ( mIsParallelMode == SMA_PARALLEL_DELETE_THREAD_ON )
                {
                    /* ���� OID Ž���� ���� lock�� ȹ���Ѵ�. */
                    IDE_TEST( mListLock[sListN].lock(NULL) !=IDE_SUCCESS );
                    sState = ID_TRUE;
                }

                if( sCurOIDHead == sLogicalAgerHead )
                {
                    break;
                }

            } /* While */

            commitATrans();

            /* OIDHead�� mKeyFreeSCN�� Min SCN���� ������ Aging�ϴµ� Key Free SCN��
             * System SCN���� �����Ǳ� ������ mKeyFreeSCN�� ������ ���Ŀ� Commit��
             * Transaction�� ���� ��� System SCN�� �������� �ʱ⶧���� Delete Thread��
             * ��� ����ϴ� ������ �߻��Ѵ�. �Ͽ� Aging List�� Aging����� ������
             * System SCN�� KeyFreeSCN�� �����ϰ� ViewSCN�� ������ Transaction�� ���ٸ�
             * System SCN�� ������Ų��. �̶� ���� ���� ���� �ʵ��� mTimeOut�� �ִ�ġ �϶�
             * �� ������Ų��. */

            sIncreaseSystemSCN = ID_FALSE;

            if ( ( mHandled   == ID_FALSE ) &&
                 ( mTimeOut   == smuProperty::getAgerWaitMax() ) &&
                 ( sOldestTID == SM_NULL_TID ) )
            {
                if ( smxTransMgr::isActiveVersioningMinTime() == ID_TRUE )
                {
                    /* PROJ-2733
                       AgingViewSCN�� �̿��� Aging ���̶��,
                       AgingViewSCN�� system SCN ���� �������� ���� ��찡 �������ִ�.
                       AgingViewsCN�� system SCN �� �Ѿ ���Ŀ��� system SCN�� ������Ű���� �Ѵ�. */

                    sSystemSCN = smmDatabase::getLstSystemSCN();

                    if ( SM_SCN_IS_GT( &sMinViewSCN, &sSystemSCN ) )
                    {
                        sIncreaseSystemSCN = ID_TRUE;
                    }
                }
                else
                {
                    sIncreaseSystemSCN = ID_TRUE;
                }
            }

            if ( sIncreaseSystemSCN == ID_TRUE )
            {
                IDE_TEST( smmDatabase::getCommitSCN( NULL,     /* aTrans* */
                                                     ID_FALSE, /* aIsLegacytrans */
                                                     NULL )    /* aStatus */
                          != IDE_SUCCESS );
            }
        }
    }

    /* �ش� List���� �� �̻� ó���� OID�� ����. lock�� Ǯ���ش�. */
    sState = ID_FALSE;
    IDE_TEST( mListLock[sListN].unlock() != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_list );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == ID_TRUE )
    {
        sState = ID_FALSE;
        IDE_ASSERT( mListLock[sListN].unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC smaDeleteThread::processAgingOIDNodeList( smaOidList *aOIDHead,
                                                 idBool      aDeleteAll )
{
    smxOIDNode         *sCurOIDNode;
    smxOIDNode         *sNxtOIDNode;
    smxOIDInfo         *sOIDInfo;
    UInt                i;
    UInt                sCondition;
    idBool              sIsProcessed;

    //delete record and drop table
    sCurOIDNode = aOIDHead->mHead;
    sCondition  = aOIDHead->mCondition;

    while( sCurOIDNode != NULL )
    {
        for( i = 0; i < sCurOIDNode->mOIDCnt; i++ )
        {
            sOIDInfo = sCurOIDNode->mArrOIDInfo + i;

            if ( (( sOIDInfo->mFlag & sCondition ) == sCondition) && 
                 (( sOIDInfo->mFlag & SM_OID_ACT_COMPRESSION ) == 0) )
            {
                /* BUG-15047 */
                IDE_ASSERT( lockCheckMtx() == IDE_SUCCESS );

                /* Ager Thread���� ������д� Fatal�̹Ƿ� ����ó������
                 * �ʴ´�.*/
                IDE_ASSERT( processJob( mTrans[mThreadID] ,
                                        sOIDInfo,
                                        aDeleteAll,
                                        &sIsProcessed,
                                        aOIDHead->mSCN )
                            == IDE_SUCCESS );

                IDE_ASSERT( unlockCheckMtx() == IDE_SUCCESS );

                if( sIsProcessed == ID_TRUE )
                {
                    mAgingCntAfterATransBegin++;
                }//if sIsProcessd
            }//if

            /* BUG-17417 V$Ager������ Add OID������ ���� Ager��
             *           �ؾ��� �۾��� ������ �ƴϴ�.
             *
             * �ϳ��� OID�� ���ؼ� Aging�� �����ϸ�
             * mAgingProcessedOIDCnt�� 1���� ��Ų��. */
            if( smxOIDList::checkIsAgingTarget( aOIDHead->mCondition,
                                                sOIDInfo ) == ID_TRUE )
            {
                acpAtomicInc64( &mAgingProcessedOIDCnt );
            }

        }//for

        sNxtOIDNode = sCurOIDNode->mNxtNode;

        mHandled = ID_TRUE;

        IDE_ASSERT( smxOIDList::freeMem( sCurOIDNode )
                    == IDE_SUCCESS );

        sCurOIDNode = sNxtOIDNode;

        commitAndBeginATransIfNeed();
    }//while

    // BUG-15306
    // DummyOID�� ��쿣 ó���� count�� �������� �ʴ´�.
    if( smaLogicalAger::isDummyOID( aOIDHead ) != ID_TRUE )
    {
        acpAtomicInc64( &mHandledCnt );
    }

    IDE_ASSERT( freeOIDNodeListHead( aOIDHead ) == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***********************************************************

  Ư�� Table���� OID�� Ư�� Tablespace���� OID�� ���ؼ���
  ��� Aging�� �ǽ��Ѵ�.

  Argument:
      1. smOID aTableOID: Aging�� �ﰢ������ ������ Table�� OID
      2. smaOidList *aEndPtr: aEndPtr���� Aging�� ����.

  Return Value:
      1. SUCCESS : IDE_SUCCESS
      2. FAIL    : IDE_FAILURE

  Description:
    Aging OID List�� �Ŵ޷� �ִ� ���߿���
    Ư�� Table���� OID�� Ư�� Tablespace���� OID��
    ���ؼ��� Aging�� �����Ѵ�.

    Aging�Ϸ��� OID�� SCN�� Active Transaction���� Minimum ViewSCN ����
    ũ���� Aging�� �ǽ��Ѵ�.

  Caution :
    �� Function�� ���� �ٸ� �ʿ��� Aging Filter�� ������ Table�̳�
    Tablespace�� ���ؼ� �ٸ� Transaction�� �������� �ʴ´ٴ� ���� ����
    �ؾ� �Ѵ�.

    Table�̳� Tablespace��  X-Lock�� ��� ������ �̸� ������ �� �ִ�.

**********************************************************/
IDE_RC smaDeleteThread::deleteInstantly( smaInstantAgingFilter * aAgingFilter )
{
    smxOIDNode         * sCurOIDNodePtr;
    smxOIDNode         * sNxtOIDNodePtr;
    smaOidList         * sCurOIDHeadPtr;
    UInt                 i;
    smxOIDInfo         * sOIDInfoPtr;
    UInt                 sCondition;
    idBool               sIsProcessed;
    UInt                 sState = 0;
    /* BUG-41026 */
    PDL_Time_Value       sTimeOut;
    UInt                 sVarTime = 0;
    UInt                 sAgerWaitMin = 0;
    UInt                 sAgerWaitMax = 0;

    UInt                 sListN = 0;
    idBool               sIsLock = ID_FALSE;
    smcTableHeader     * sCurTable;

    sAgerWaitMax = smuProperty::getAgerWaitMax();
    sAgerWaitMin = smuProperty::getAgerWaitMin();
    sVarTime = sAgerWaitMin;
    sTimeOut.set(0, sVarTime);

    /* BUG-47367 DeleteThread���� ���ü����� ������
     * Instant Aging�� �����ϴ� Tx������ ���ü� ��� �ʿ��ϴ�.
     * Instant Aging ������ �Ҵ�� mTrans4Self�� 1���̱� ������
     * ���ÿ� Instant Aging�� �����ϸ� �ȵȴ�. */
    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;

    /*
      ������ Transaction���� ó���Ѵ�. �ֳ��ϸ� Aging�� ������
      Transaction�� �׻� Commit�ؾ��ϴµ� Rollback�� �߻���
      �� �ֱ� ������ ������ Transaction���� ó���Ѵ�.
    */
    IDE_ASSERT( mTrans4Self->begin( NULL,
                                    ( SMI_TRANSACTION_REPL_NONE |
                                      SMI_COMMIT_WRITE_NOWAIT ),
                                    SMX_NOT_REPL_TX_ID )
               == IDE_SUCCESS);
    sState = 2;

    /* Transaction�� RSGroupID�� 0���� �����Ѵ�.
     * 0�� PageList�� �������� �ݳ��ϰ� �ȴ�. */
    smxTrans::setRSGroupID((void*)mTrans4Self, 0);

    /* instant aging�� Ư�� Table,TableSpace�� ������� �ϱ� ������
     * ��� list�� �� ������ �ϴ� �δ��� �ִ�. */
    for ( sListN = 0; sListN < smaLogicalAger::mListCnt ; sListN++ )
    {
        mListLock[sListN].lock( NULL );
        sIsLock = ID_TRUE;

        /* BUG-32780  [sm-mem-index] The delete thread can access removed page
         * because the range of instant aging is abnormal.
         * Lock�� ��� mTailDeleteThread ���� �����;� �Ѵ�. */
        sCurOIDHeadPtr = smaLogicalAger::mTailDeleteThread[sListN];

        while(sCurOIDHeadPtr != NULL)
        {
            sCondition = sCurOIDHeadPtr->mCondition;

            sCurOIDNodePtr = sCurOIDHeadPtr->mHead;

            while(sCurOIDNodePtr != NULL)
            {
                for(i = 0; i < sCurOIDNodePtr->mOIDCnt; i++)
                {
                    sOIDInfoPtr = sCurOIDNodePtr->mArrOIDInfo + i;

                    if ( ((sOIDInfoPtr->mFlag & sCondition) == sCondition) &&
                         ((sOIDInfoPtr->mFlag & SM_OID_ACT_COMPRESSION) == 0) )
                    {
                        /* BUG-47367 LogicalAger���� Drop�� OID ��ü�� ���ؼ��� 
                         * Filter ���ǿ� �¾Ƶ� Aging ó���� ������ �ʴ´�.(Flag Ȯ�ο��� HANG)
                         * LogicalAger�� ������ ������ Drop Check�� �ϵ��� ���ش�. */
                        /* Instant Ager�� ������ TABLE or TBS lock�� ��� �ϱ� ������
                         * �߰��� Drop �ǰų� ������ ������ ���� OID�� ���� 
                         * ������ Drop�� Table�� OID�� ������ �޷� �ִ� OIDList�� �����Ҽ� �ִ�. */
                        IDE_ASSERT( smcTable::getTableHeaderFromOID( sOIDInfoPtr->mTableOID,
                                                                     (void**)&sCurTable)
                                    == IDE_SUCCESS );

                        if ( smcTable::isDropedTable4Ager( sCurTable, sCurOIDHeadPtr->mSCN ) == ID_TRUE )
                        {
                            continue;
                        }

                        // Filter������ ����?
                        if ( smaLogicalAger::isAgingFilterTrue(
                                                 aAgingFilter,
                                                 sOIDInfoPtr->mSpaceID,
                                                 sOIDInfoPtr->mTableOID ) == ID_TRUE )
                        {
                            /* BUG-41026 : parallel�� �������� normal aging�� ������
                             * �̸� ���� �� ���� ����Ѵ�. */
                            while ( 1 )
                            {
                                /* BUG-41026 */
                                IDU_FIT_POINT( "4.BUG-41026@smaDeleteThread::deleteInstantly::wakeup_2");

                                if ( ( sOIDInfoPtr->mFlag & SM_OID_ACT_AGING_INDEX ) == 0 )
                                {
                                    break;
                                }
                                else
                                {
                                    idlOS::sleep(sTimeOut);
                                    
                                    sVarTime <<= 1;
                                    sVarTime = (sVarTime > sAgerWaitMax) ? sAgerWaitMax : sVarTime;
                                    sTimeOut.set(0, sVarTime);
                                }
                            }

                            IDE_TEST( processJob( mTrans4Self,
                                                  sOIDInfoPtr,
                                                  ID_FALSE,   /* aDeleteAll */
                                                  &sIsProcessed,
                                                  sCurOIDHeadPtr->mSCN )
                                      != IDE_SUCCESS );
                            acpAtomicInc64( &mAgingProcessedOIDCnt );

                            /*
                              �ٽ� �۾��� �ݺ����� �ʱ� ���ؼ� flag�� clear��Ų��.
                            */
                            sOIDInfoPtr->mFlag &= ~sCondition;
                        }
                    }
                }

                sNxtOIDNodePtr = sCurOIDNodePtr->mNxtNode;
                sCurOIDNodePtr = sNxtOIDNodePtr;
                
            }

            sCurOIDHeadPtr = sCurOIDHeadPtr->mNext;

        }
        sIsLock = ID_FALSE;
        mListLock[sListN].unlock();
    }

    sState = 1;
    IDE_TEST( mTrans4Self->commit() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 2:
        IDE_ASSERT(mTrans4Self->commit() == IDE_SUCCESS);

        case 1:
        IDE_ASSERT( unlock() == IDE_SUCCESS );
        break;
    }

    if( sIsLock == ID_TRUE )
    {
        mListLock[sListN].unlock();
    }

    return IDE_FAILURE;
}
/********************************************************

  Function Name: processJob(smxTrans    *aTransPtr,
                            smxOIDNode  *aCurOIDNodePtr,
                            smxOIDInfo  *aOIDInfoPtr,
                            idBool      *aIsProcessed)
  Argument:
      1. smxTrans    *aTransPtr : Begin�� Transaction Pointer.
      2. smxOIDNode  *aCurOIDNodePtr: Aging�� ������ OID Header
                                      Pointer
      3. smxOIDInfo  *aOIDInfoPtr: Aging�� ������ OID Info Pointer
      4. idBool      *aIsProcessed : Out argument�μ� delete�۾���
                                     �־����� ID_TRUE, �ƴϸ� ID_FALSE
      5. smSCN        aSCN : Aging�� ������ OID �� CommitSCN ��

  Return Value:
      1. SUCCESS : IDE_SUCCESS
      2. FAIL    : IDE_FAILURE

  Description:
      aTransPtr�� ����Ű�� Transaction���� aOIDInfoPtr�� �����Ͽ� Aging��
      ������.

  Caution:
      �� Function�� ������ ����� �۾��� �ϱ⶧���� Aging����� �� �̻� ��������
      ���� ��쿡�� ȣ��Ǿ�߸� �Ѵ�.

*********************************************************/
IDE_RC smaDeleteThread::processJob( smxTrans    * aTransPtr,
                                    smxOIDInfo  * aOIDInfoPtr,
                                    idBool        aDeleteAll,
                                    idBool      * aIsProcessed,
                                    smSCN         aSCN )
{
    smcTableHeader *sCatalogTable = (smcTableHeader*)SMC_CAT_TABLE;
    smcTableHeader *sTableHeader = NULL;
    SChar          *sRowPtr = NULL;
    smxSavepoint   *sISavepoint = NULL;
    UInt            sDummy = 0;

    ACP_UNUSED( aDeleteAll );

    *aIsProcessed  = ID_TRUE;

    IDE_ASSERT( smcTable::getTableHeaderFromOID( aOIDInfoPtr->mTableOID,
                                                 (void**)&sTableHeader )
                == IDE_SUCCESS );

    /*
      Table can be droped by ager before ager free fix/var record.
      because table lock of transaction is freed, when transaction
      inserting record is partially rolled back. and then other transaction can drop table.
      so aging sequence could be drop table pending, free slot record.
      so we must check table is droped before free fix/var record.
      - BUG-13968
    */
    switch(aOIDInfoPtr->mFlag & SM_OID_TYPE_MASK)
    {
        case SM_OID_TYPE_INSERT_FIXED_SLOT:
        case SM_OID_TYPE_UPDATE_FIXED_SLOT:
        case SM_OID_TYPE_DELETE_FIXED_SLOT:
            if(smcTable::isDropedTable4Ager(sTableHeader, aSCN) == ID_FALSE)
            {
#if defined(DEBUG)
                /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the 
                 * failure of index aging. 
                 * Index Key�� ���� ���������� Ȯ���Ѵ�.
                 * DeleteAll�� �����Ǹ� logical ager�� �����ϰ� ���
                 * OIDList�� aging �ϱ� ������ index key�� �������� �ʰ�
                 * ������ �� �ִ�. */
                /* BUG-47526 1. Delete All �� ��쿡�� Ȯ���ϹǷ�,
                 * Delete All�� �ƴ� ��쿡 Index key�� �������� ���� node��
                 * ���� �ȵ˴ϴ�. (smaLogicalAger::run()���� ����)
                 * 2. Assert ��ġ�� �ű�ϴ�.
                 * ���� �� ���ٴ� Index�� key�� Ȯ���� ������ Assert�Ǿ��
                 * �� �˻簡 �����ѵ� �Ͽ� �Ʒ��� ���������ϴ�. */
                if( aDeleteAll == ID_FALSE )
                {
                    IDE_TEST( smaLogicalAger::checkOldKeyFromIndexes(
                                                    sTableHeader,
                                                    aOIDInfoPtr->mTargetOID )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* do nothing */
                }
# endif
                IDE_ASSERT( smmManager::getOIDPtr( aOIDInfoPtr->mSpaceID,
                                                   aOIDInfoPtr->mTargetOID,
                                                   (void**)&sRowPtr )
                            == IDE_SUCCESS );

                if( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE )
                {
                    /* PROJ-1594 Volatile TBS */
                    /* volatile table�� ��� svcRecord�� ȣ���ؾ� �Ѵ�. */
                    IDE_TEST(svcRecord::setFreeFixRowPending(
                                                         aTransPtr,
                                                         sTableHeader,
                                                         sRowPtr,
                                                         aSCN )
                             != IDE_SUCCESS);
                }
                else
                {
                    IDE_TEST(smcRecord::setFreeFixRowPending(
                                                         aTransPtr,
                                                         sTableHeader,
                                                         sRowPtr,
                                                         aSCN )
                             != IDE_SUCCESS);
                }
            }
            break;

        case SM_OID_TYPE_FREE_LPCH:
            IDE_ASSERT( iduMemMgr::free((void*)(aOIDInfoPtr->mTargetOID))
                        == IDE_SUCCESS );
            break;

        case SM_OID_TYPE_VARIABLE_SLOT:
            if(smcTable::isDropedTable4Ager(sTableHeader,aSCN) == ID_FALSE)
            {
                /* PROJ-1594 Volatile TBS */
                /* volatile table�� ��� svcRecord�� ȣ���ؾ� �Ѵ�. */
                    IDE_ASSERT( smmManager::getOIDPtr( aOIDInfoPtr->mSpaceID,
                                                       aOIDInfoPtr->mTargetOID,
                                                       (void**)&sRowPtr )
                                == IDE_SUCCESS );
                if( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE )
                {
                    IDE_TEST( svcRecord::setFreeVarRowPending(
                                                     aTransPtr,
                                                     sTableHeader,
                                                     aOIDInfoPtr->mTargetOID,
                                                     sRowPtr,
                                                     aSCN )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( smcRecord::setFreeVarRowPending(
                                                     aTransPtr,
                                                     sTableHeader,
                                                     aOIDInfoPtr->mTargetOID,
                                                     sRowPtr,
                                                     aSCN )
                              != IDE_SUCCESS );
                }
            }

            break;

        case SM_OID_TYPE_DROP_TABLE:
            /* BUG-32237 [sm_transaction] Free lock node when dropping table.
             * DropTablePending ���� �����ص� freeLockNode�� �����մϴ�. */
            if( ( SMI_TABLE_TYPE_IS_DISK( sTableHeader )     == ID_TRUE ) ||
                ( SMI_TABLE_TYPE_IS_MEMORY( sTableHeader )   == ID_TRUE ) ||
                ( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE ) )
            {
                IDE_ASSERT( smcTable::isDropedTable4Ager(sTableHeader, aSCN)
                            == ID_TRUE );

                /* Transaction�� �� �ϳ� �����ϰ�, �ش� Transaction�� Drop��
                 * ����������, Aging List�� �����ϰ� Transaction�� Lock�� Ǯ��
                 * ������, Ager�� LockItem�� ���� ������ �� Lock�� Ǯ����
                 * ��찡 ���� �� ����. ���� Table�� XLock�� ��� ���ü�
                 * ������ ������ �� (������ Drop�� Table�̶� �������� ����)
                 * Lock�� �����ؾ� ��. */
                /* �ٸ� TABLE_LOCK_ENABLE�� �������� ��쿡�� CreateTable��
                 * ������ ���, Property ���� ���� �Ŵ޸� Job���� ���� ������
                 * ���Ŀ��� DDL�� ���� ������ Locküũ�� �ʿ䰡 ���� */
                if( smuProperty::getTableLockEnable() == 1 )
                {
                    IDE_TEST( aTransPtr->setImpSavepoint( &sISavepoint, sDummy )
                              != IDE_SUCCESS );
                    /* BUG-42928 No Partition Lock
                     *  �̹� DROP�� Table/Partition�̹Ƿ�, mLock�� ����Ѵ�.
                     */
                    IDE_TEST( smlLockMgr::lockTableModeX( aTransPtr,
                                                          sTableHeader->mLock )
                              != IDE_SUCCESS );
                    IDE_TEST( aTransPtr->abortToImpSavepoint(
                                  sISavepoint )
                              != IDE_SUCCESS );
                    IDE_TEST( aTransPtr->unsetImpSavepoint( sISavepoint )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* nothing to do... */
                }

                IDE_TEST( smcTable::finLockItem( sTableHeader )
                          != IDE_SUCCESS );

                /* ������ free �� slot�̹Ƿ� ���� �߸��Ǿ �������.
                    * DASSERT�θ� Ȯ���Ѵ�. */
                IDE_DASSERT( sTableHeader->mSelfOID == aOIDInfoPtr->mTableOID );

                if(( sTableHeader->mFlag & SMI_TABLE_PRIVATE_VOLATILE_MASK )
                   == SMI_TABLE_PRIVATE_VOLATILE_TRUE )
                {
                    /* PROJ-1407 Temporary table
                     * User temp table�� �� ���Ǹ� �����ϰ� create/drop��
                     * ����ϹǷ�, table header�� �ٷ� Free�Ѵ�.*/

                    IDE_DASSERT(( sTableHeader->mFlag & SMI_TABLE_TYPE_MASK )
                                == SMI_TABLE_VOLATILE );

                    IDE_TEST( smmManager::getOIDPtr(
                                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                              aOIDInfoPtr->mTableOID,
                                              (void**)&sRowPtr )
                              != IDE_SUCCESS );

                    IDE_TEST( smpFixedPageList::freeSlot(
                                              NULL, //aStatistics,
                                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                              &(SMC_CAT_TEMPTABLE->mFixed.mMRDB),
                                              sRowPtr,
                                              SMP_TABLE_TEMP,
                                              aSCN )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* PROJ-2268 Reuse Catalog Table Slot
                     * Property�� ���� ��Ȱ�� ���θ� ������ �� �־�� �Ѵ�. */
                    if ( smuProperty::getCatalogSlotReusable() == 1 )
                    {
                        IDE_TEST( smmManager::getOIDPtr(
                                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                              aOIDInfoPtr->mTableOID,
                                              (void**)&sRowPtr )
                                  != IDE_SUCCESS );

                        IDE_TEST( smcRecord::setFreeFixRowPending( aTransPtr,
                                                                   sCatalogTable,
                                                                   sRowPtr,
                                                                   aSCN )
                                  != IDE_SUCCESS);
                    }
                    else
                    {
                        /* Dictionary Table Slot�� ��Ȱ������ �ʴ´�. */
                    }
                }
            }
            else
            {
                /* PROJ-2268 Reuse Catalog Table Slot
                 * Sequence, Procedure, Pakage ��� ����� Catalog Slot�� ���
                 * Table Type�� Meta�� ����Ǳ� ������ ���� ó���� �־�� �Ѵ�. */
                if ( smuProperty::getCatalogSlotReusable()  == 1 )
                {
                    if ( SMI_TABLE_TYPE_IS_META( sTableHeader ) == ID_TRUE )
                    {
                        IDE_ASSERT( smcTable::isDropedTable4Ager(sTableHeader, aSCN) == ID_TRUE );

                        /* Transaction�� �� �ϳ� �����ϰ�, �ش� Transaction�� Drop��
                         * ����������, Aging List�� �����ϰ� Transaction�� Lock�� Ǯ��
                         * ������, Ager�� LockItem�� ���� ������ �� Lock�� Ǯ����
                         * ��찡 ���� �� ����. ���� Table�� XLock�� ��� ���ü�
                         * ������ ������ �� (������ Drop�� Table�̶� �������� ����)
                         * Lock�� �����ؾ� ��. */
                        /* �ٸ� TABLE_LOCK_ENABLE�� �������� ��쿡�� CreateTable��
                         * ������ ���, Property ���� ���� �Ŵ޸� Job���� ���� ������
                         * ���Ŀ��� DDL�� ���� ������ Locküũ�� �ʿ䰡 ���� */
                        if ( smuProperty::getTableLockEnable() == 1 )
                        {
                            IDE_TEST( aTransPtr->setImpSavepoint( &sISavepoint, sDummy )
                                      != IDE_SUCCESS );
                            /* BUG-42928 No Partition Lock
                             *  �̹� DROP�� Table/Partition�̹Ƿ�, mLock�� ����Ѵ�.
                             */
                            IDE_TEST( smlLockMgr::lockTableModeX( aTransPtr,
                                                                  sTableHeader->mLock )
                                      != IDE_SUCCESS );
                            IDE_TEST( aTransPtr->abortToImpSavepoint( sISavepoint )
                                      != IDE_SUCCESS );
                            IDE_TEST( aTransPtr->unsetImpSavepoint( sISavepoint )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* nothing to do... */
                        }

                        IDE_TEST( smcTable::finLockItem( sTableHeader )
                                  != IDE_SUCCESS );

                        /* ������ free �� slot�̹Ƿ� ���� �߸��Ǿ �������.
                         * DASSERT�θ� Ȯ���Ѵ�. */
                        IDE_DASSERT( sTableHeader->mSelfOID == aOIDInfoPtr->mTableOID );

                        /* PROJ-2268 Reuse Catalog Table Slot
                         * Property�� ���� ��Ȱ�� ���θ� ������ �� �־�� �Ѵ�. */
                        if ( smuProperty::getCatalogSlotReusable() == 1 )
                        {
                            IDE_TEST( smmManager::getOIDPtr( SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                                             aOIDInfoPtr->mTableOID,
                                                             (void**)&sRowPtr )
                                      != IDE_SUCCESS );

                            IDE_TEST( smcRecord::setFreeFixRowPending( aTransPtr,
                                                                       sCatalogTable,
                                                                       sRowPtr,
                                                                       aSCN )
                                      != IDE_SUCCESS);
                        }
                        else
                        {
                            /* Dictionary Table Slot�� ��Ȱ������ �ʴ´�. */
                        }
                    }
                    else
                    {
                        /* Meta ���� �������� ������� ���� �ʴ´�. */
                    }
                }
                else
                {
                    /* ��Ȱ������ �ʴ´ٸ� ������ �����ϰ� �����Ͽ��� �Ѵ�. */
                }
            }
            break;

        case SM_OID_TYPE_DELETE_TABLE_BACKUP:
            /* BUG-16161: Add Column�� �������� �ٽ� Add Column�� �����ϸ�
               Session�� Hang���·� �����ϴ�.: Transaction�� Commit�̳� Abort�ÿ�
               Backup File������ ������.*/
            break;

        default:

            *aIsProcessed  = ID_FALSE;
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aIsProcessed  = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC smaDeleteThread::shutdown()
{

    mFinished = ID_TRUE;

    IDE_TEST_RAISE(join() != 0, err_thr_join);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Commit�̳� Abort�Ŀ� FreeSlot�� ���� FreeSlotList�� �Ŵܴ�.
 *
 * BUG-14093
 * freeSlot()���� slot�� ���� Free�۾��� �����ϰ�
 * ager Tx�� commit�� ���Ŀ� FreeSlotList�� ����ϴ� �۾��� �����Ѵ�.
 */
IDE_RC smaDeleteThread::processFreeSlotPending(smxTrans   *aTrans,
                                               smxOIDList *aOIDList)
{
    smxOIDNode         *sCurOIDNode;
    smxOIDNode         *sNxtOIDNode;
    smxOIDInfo         *sOIDInfo;
    smcTableHeader     *sTableHeader;
    SChar              *sRowPtr;
    UInt                i;
    UInt                sState = 0;
    iduMutex           *sCheckMutex = (iduMutex *)aTrans->mConnectDeleteThread;

    if( aOIDList->isEmpty() == ID_FALSE )
    {
        /* BUG-47367 �Ϲ������� FreeSlotList���� Ager Tx���� add �ϳ�
         * DropPending ���� �����ϱ� ���� �ӽ÷� �Ҵ�� Tx ���� ��� FreeSlotList�� ����� �� �ִ�.
         * Ager Tx�� ���� ���� checkMutex�� ������ �۾��ϰ�
         * DropPending ���� Tx�� ��� mCheckMutex4Self�� ����ϵ��� �Ѵ�.
         */
        if ( sCheckMutex == NULL )
        {
            sCheckMutex = &mCheckMutex4Self; 
        }

        /* BUG-15047 */
        IDE_ASSERT( sCheckMutex->lock( NULL ) == IDE_SUCCESS );
        sState = 1;

        // OIDList�� Head�� NULL Node���� �����ϹǷ� Head�� Next Node���� ����
        sCurOIDNode = aOIDList->mOIDNodeListHead.mNxtNode;

        // CurOIDNode�� ��ȸ�ؼ� �ٽ� OIDList�� Head�� ���ƿ��� ������.
        while(sCurOIDNode != &(aOIDList->mOIDNodeListHead))
        {
            for(i = 0; i < sCurOIDNode->mOIDCnt; i++)
            {
                sOIDInfo     = sCurOIDNode->mArrOIDInfo + i;
                IDE_ASSERT( smcTable::getTableHeaderFromOID( sOIDInfo->mTableOID,
                                                             (void**)&sTableHeader )
                            == IDE_SUCCESS );
                /* BUG-15969: �޸� Delete Thread���� ������ ���� */
                if(smcTable::isDropedTable4Ager( sTableHeader, sOIDInfo->mSCN ) == ID_FALSE)
                {
                    /* PROJ-1594 Volatile TBS */
                    IDE_ASSERT( smmManager::getOIDPtr( sOIDInfo->mSpaceID,
                                                       sOIDInfo->mTargetOID, 
                                                       (void**)&sRowPtr)
                                == IDE_SUCCESS );

                    IDE_ASSERT( sTableHeader->mSpaceID == sOIDInfo->mSpaceID );

                    switch(sOIDInfo->mFlag)
                    {
                        case SM_OID_TYPE_FREE_FIXED_SLOT :
                            if( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE )
                            {
                                IDE_TEST(svcRecord::freeFixRowPending(
                                                             aTrans,
                                                             sTableHeader,
                                                             sRowPtr)
                                         != IDE_SUCCESS);
                            }
                            else
                            {
                                IDE_TEST(smcRecord::freeFixRowPending(
                                                             aTrans,
                                                             sTableHeader,
                                                             sRowPtr)
                                         != IDE_SUCCESS);
                            }
                            break;

                        case SM_OID_TYPE_FREE_VAR_SLOT :
                            if( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE )
                            {
                                IDE_TEST(svcRecord::freeVarRowPending(
                                                             aTrans,
                                                             sTableHeader,
                                                             sOIDInfo->mTargetOID,
                                                             sRowPtr)
                                     != IDE_SUCCESS);
                            }
                            else
                            {
                                IDE_TEST(smcRecord::freeVarRowPending(
                                                             aTrans,
                                                             sTableHeader,
                                                             sOIDInfo->mTargetOID,
                                                             sRowPtr)
                                         != IDE_SUCCESS);
                            }
                            break;
                        case SM_OID_TYPE_UNLOCK_FIXED_SLOT :
                            if( SMI_TABLE_TYPE_IS_VOLATILE( sTableHeader ) == ID_TRUE )
                            {
                                IDE_TEST(svcRecord::unlockRow( aTrans,
                                                               sRowPtr )
                                         != IDE_SUCCESS);
                            }
                            else
                            {
                                IDE_TEST(smcRecord::unlockRow( aTrans,
                                                               sOIDInfo->mSpaceID,
                                                               sRowPtr )
                                         != IDE_SUCCESS);
                            }
                            break;
                        default:
                            // OIDFreeSlotList���� �ٸ� �۾��� ����.
                            IDE_ASSERT(0);
                            break;
                    }
                }//for
            }

            sNxtOIDNode = sCurOIDNode->mNxtNode;

            sCurOIDNode->mNxtNode->mPrvNode = sCurOIDNode->mPrvNode;
            sCurOIDNode->mPrvNode->mNxtNode = sCurOIDNode->mNxtNode;

            IDE_TEST(smxOIDList::freeMem(sCurOIDNode) != IDE_SUCCESS);

            sCurOIDNode = sNxtOIDNode;
        }//while

        /* BUG-15047 */
        sState = 0;
        IDE_ASSERT( sCheckMutex->unlock() == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sCheckMutex->unlock() == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

IDE_RC  smaDeleteThread::lockAll()
{
    UInt i;
    for ( i = 0; i < smaLogicalAger::mListCnt ; i++ )
    {
        mListLock[i].lock( NULL );
    }

    return IDE_SUCCESS;
}

IDE_RC  smaDeleteThread::unlockAll()
{
    UInt i;
    for ( i = 0; i < smaLogicalAger::mListCnt ; i++ )
    {
        mListLock[i].unlock();
    }
   
    return IDE_SUCCESS; 
}
