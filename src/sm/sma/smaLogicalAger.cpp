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
 * $Id: smaLogicalAger.cpp 90259 2021-03-19 01:22:22Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smm.h>
#include <sma.h>
#include <smx.h>
#include <smi.h>
#include <sgmManager.h>

smmSlotList*    smaLogicalAger::mSlotList;
iduMutex        smaLogicalAger::mBlock;
iduMutex        smaLogicalAger::mCheckMutex;
smaOidList**    smaLogicalAger::mHead;
smaOidList**    smaLogicalAger::mTailLogicalAger;
smaOidList**    smaLogicalAger::mTailDeleteThread;
UInt            smaLogicalAger::mListCnt;
smaLogicalAger* smaLogicalAger::mLogicalAgerList;
ULong           smaLogicalAger::mAddCount;
ULong           smaLogicalAger::mHandledCount;
UInt            smaLogicalAger::mCreatedThreadCount;
UInt            smaLogicalAger::mRunningThreadCount;
idBool          smaLogicalAger::mIsInitialized = ID_FALSE;
ULong           smaLogicalAger::mAgingRequestOIDCnt;
ULong           smaLogicalAger::mAgingProcessedOIDCnt;
ULong           smaLogicalAger::mSleepCountOnAgingCondition;
iduMutex        smaLogicalAger::mAgerCountChangeMutex;
iduMutex        smaLogicalAger::mFreeNodeListMutex;
SInt            smaLogicalAger::mBlockFreeNodeCount;
UInt            smaLogicalAger::mIsParallelMode;
iduMutex*       smaLogicalAger::mListLock;


smaLogicalAger::smaLogicalAger() : idtBaseThread()
{

}


IDE_RC smaLogicalAger::initializeStatic()
{

    UInt i;

    mLogicalAgerList = NULL;

    mSlotList = NULL;
    mListCnt  = smuProperty::getAgerListCount(); 

    /* TC/FIT/Limit/sm/sma/smaLogicalAger_alloc_malloc1.sql */
    IDU_FIT_POINT_RAISE( "smaLogicalAger::alloc::malloc1",
                          insufficient_memory );
    /* TC/FIT/Limit/sm/sma/smaLogicalAger_alloc_malloc2.sql */
    IDU_FIT_POINT_RAISE( "smaLogicalAger::alloc::malloc2",
                          insufficient_memory );
    /* TC/FIT/Limit/sm/sma/smaLogicalAger_alloc_malloc3.sql */
    IDU_FIT_POINT_RAISE( "smaLogicalAger::alloc::malloc3",
                          insufficient_memory );

    //fix bug-23007
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(smmSlotList) * mListCnt ,
                                       (void**)&(mSlotList)) != IDE_SUCCESS,
                    insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(smaOidList*) * mListCnt ,
                                       (void**)&(mHead)) != IDE_SUCCESS,
                    insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(smaOidList*) * mListCnt ,
                                       (void**)&(mTailLogicalAger)) != IDE_SUCCESS,
                    insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(smaOidList*) * mListCnt ,
                                       (void**)&(mTailDeleteThread)) != IDE_SUCCESS,
                    insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SMA,
                                       ID_SIZEOF(iduMutex) * mListCnt ,
                                       (void**)&(mListLock)) != IDE_SUCCESS,
                    insufficient_memory );

    for( i = 0; i < mListCnt; i++)
    {
        /* BUG-47367 �������� SlotList�� 3�� ���� �Ҵ�� �ݳ����� ���� �־�����
         * ������ �� List���� ���� List�� �ξ� addList�� ������ �л��Ų��.
         * List�� 1�� �̱� ������ �Ҵ�/�ݳ��ÿ� Ager�� Tx���� ������ �߻��Ѵ�. */
        IDE_TEST( mSlotList[i].initialize( IDU_MEM_SM_SMA_LOGICAL_AGER,
                                           "SMA_AGER_OID_LIST",
                                           ID_SIZEOF( smaOidList ),
                                           SMA_NODE_POOL_MAXIMUM,
                                           SMA_NODE_POOL_CACHE )
                  != IDE_SUCCESS );

        mHead[i]             = NULL;
        mTailLogicalAger[i]  = NULL;
        mTailDeleteThread[i] = NULL;

        IDE_TEST( mListLock[i].initialize( (SChar*)"MEMORY_LOGICAL_GC_MUTEX",
                                           IDU_MUTEX_KIND_NATIVE,
                                           IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    }

    IDE_TEST( mBlock.initialize((SChar*)"MEMORY_LOGICAL_GC_MUTEX",
                                IDU_MUTEX_KIND_NATIVE,
                                IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    IDE_TEST( mCheckMutex.initialize((SChar*)"MEMORY_LOGICAL_GC_CHECK_MUTEX",
                                     IDU_MUTEX_KIND_NATIVE,
                                     IDV_WAIT_INDEX_NULL)
              != IDE_SUCCESS );

    IDE_TEST( mAgerCountChangeMutex.initialize(
                                  (SChar*)"MEMORY_LOGICAL_GC_COUNT_CHANGE_MUTEX",
                                  IDU_MUTEX_KIND_NATIVE,
                                  IDV_WAIT_INDEX_NULL) 
              != IDE_SUCCESS );

    IDE_TEST( mFreeNodeListMutex.initialize(
                                (SChar*)"MEMORY_LOGICAL_GC_FREE_NODE_LIST_MUTEX",
                                IDU_MUTEX_KIND_NATIVE,
                                IDV_WAIT_INDEX_NULL) 
              != IDE_SUCCESS );


    mBlockFreeNodeCount = 0;

    mAddCount = 0;
    mHandledCount = 0;
    mSleepCountOnAgingCondition = 0;

    mAgingRequestOIDCnt = 0;
    mAgingProcessedOIDCnt = 0;


    /* TC/FIT/Limit/sm/sma/smaLogicalAger_alloc_malloc4.sql */
    IDU_FIT_POINT_RAISE( "smaLogicalAger::alloc::malloc4",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SMA,
                                 ID_SIZEOF(smaLogicalAger) *
                                 smuProperty::getMaxLogicalAgerCount(),
                                 (void**)&(mLogicalAgerList)) != IDE_SUCCESS,
                    insufficient_memory );

    mCreatedThreadCount = smuProperty::getLogicalAgerCount();
    mRunningThreadCount = mCreatedThreadCount;


    IDE_ASSERT( mCreatedThreadCount <= smuProperty::getMaxLogicalAgerCount() );

    /* BUG-35179 Add property for parallel logical ager */
    mIsParallelMode = smuProperty::getParallelLogicalAger();
        
    for(i = 0; i < mCreatedThreadCount; i++)
    {
        new (mLogicalAgerList + i) smaLogicalAger;
        IDE_TEST(mLogicalAgerList[i].initialize(i) != IDE_SUCCESS);
    }

    mIsInitialized = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::destroyStatic()
{

    UInt i;

    for(i = 0; i <  mCreatedThreadCount; i++)
    {
        IDE_TEST( mLogicalAgerList[i].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( mBlock.destroy() != IDE_SUCCESS );
    IDE_TEST( mCheckMutex.destroy() != IDE_SUCCESS );
    IDE_TEST( mAgerCountChangeMutex.destroy() != IDE_SUCCESS );
    IDE_TEST( mFreeNodeListMutex.destroy() != IDE_SUCCESS );

    for ( i = 0; i < mListCnt; i++ )
    {    
        IDE_TEST( mSlotList[i].release( ) != IDE_SUCCESS );
        IDE_TEST( mSlotList[i].destroy( ) != IDE_SUCCESS );

        IDE_TEST( mListLock[i].destroy() != IDE_SUCCESS );
    }

    IDE_TEST( iduMemMgr::free( mListLock ) != IDE_SUCCESS );
    mListLock = NULL;

    IDE_TEST( iduMemMgr::free( mTailDeleteThread ) != IDE_SUCCESS );
    mTailDeleteThread = NULL;

    IDE_TEST( iduMemMgr::free( mTailLogicalAger ) != IDE_SUCCESS );
    mTailLogicalAger = NULL;

    IDE_TEST( iduMemMgr::free( mHead ) != IDE_SUCCESS );
    mHead = NULL;

    IDE_TEST( iduMemMgr::free(mSlotList) != IDE_SUCCESS );
    mSlotList = NULL;

    IDE_TEST( iduMemMgr::free(mLogicalAgerList) != IDE_SUCCESS );
    mLogicalAgerList = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::shutdownAll()
{
    UInt i;

    for(i = 0; i < mCreatedThreadCount; i++)
    {
        IDE_TEST(mLogicalAgerList[i].shutdown() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smaLogicalAger::initialize( UInt aThreadID )
{
    SChar           sMutexName[128];

    mContinue         = ID_TRUE;

    mTimeOut = smuProperty::getAgerWaitMax();
    mThreadID = aThreadID;
    
    idlOS::snprintf( sMutexName,
                     128,
                     "MEMORY_LOGICAL_GC_THREAD_MUTEX_%"ID_UINT32_FMT,
                     aThreadID );

    /* BUG-35179 Add property for parallel logical ager
     * drop table, drop tablespace���� ���ü� ������ �ذ��ϱ� ���� �� thread����
     * lock�� �߰��Ѵ�. */
    IDE_TEST( mWaitForNoAccessAftDropTblLock.initialize(
                                        sMutexName,
                                        IDU_MUTEX_KIND_NATIVE,
                                        IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    IDE_TEST( start() != IDE_SUCCESS );
    IDE_TEST( waitToStart(0) != IDE_SUCCESS );

    SM_MAX_SCN( &mViewSCN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::destroy( void )
{
    /* BUG-35179 Add property for parallel logical ager
     * drop table, drop tablespace���� ���ü� ������ �ذ��ϱ� ���� �� thread����
     * lock�� �߰��Ѵ�. */
    IDE_TEST( mWaitForNoAccessAftDropTblLock.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC smaLogicalAger::shutdown( void )
{


    // BUGBUG smi interface layer�����Ǹ� �׶� ����.
    //IDE_RC _smiSetAger( idBool aValue );

    mContinue = ID_FALSE;
    // BUGBUG smi interface layer�����Ǹ� �׶� ����.
    //IDE_TEST( _smiSetAger( ID_TRUE ) != IDE_SUCCESS );

    IDE_TEST( setAger( ID_TRUE ) != IDE_SUCCESS );

    IDE_TEST_RAISE( join() != IDE_SUCCESS, ERR_THREAD_JOIN );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_THREAD_JOIN );
    IDE_SET( ideSetErrorCode( smERR_FATAL_Systhrjoin ) );

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::setAger( idBool aValue )
{
    static idBool sValue = ID_TRUE;

    if( aValue != sValue )
    {
        if( aValue == ID_TRUE )
        {
            IDE_TEST( smaLogicalAger::unblock() != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( smaLogicalAger::block() != IDE_SUCCESS );
        }
        sValue = aValue;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    Ager�� �ʱ�ȭ�Ǿ����� ���θ� �����Ѵ�.

    BLOCK/UNBLOCK�� ȣ���ϱ� ���� �� �Լ��� ȣ���Ͽ�
    AGER�� �ʱ�ȭ�Ǿ������� �˻��� �� �ִ�.

    Ager�� �ʱ�ȭ���� ���� META���� �ܰ迡�� BLOCK/UNBLOCK��
    ȣ������ �ʵ��� �ϱ� ���� ���ȴ�.
*/
idBool smaLogicalAger::isInitialized( void )
{
    return mIsInitialized;
}


IDE_RC smaLogicalAger::block( void )
{
    UInt sStage = 0;
    UInt i = 0;

    // Ager�� �ʱ�ȭ �� META���¿����� Block/Unblock�� ȣ��Ǿ�� �Ѵ�.
    IDE_ASSERT( mIsInitialized == ID_TRUE );

    /* ��� Ager�� ����� �Ѵ�. ��� List�� lock�� ȹ���Ѵ�. */
    for ( i = 0; i< mListCnt; i++ )
    {
        mListLock[i].lock(NULL);
    }

    sStage = 1;

    IDE_TEST( smaDeleteThread::lockAll() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sStage )
    {
        case 1:
        for ( i = 0; i < mListCnt; i++ )
        {
            mListLock[i].unlock();
        }
        break;
    }
    IDE_POP();

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::unblock( void )
{

    UInt sStage = 1;
    UInt i = 0;

    // Ager�� �ʱ�ȭ �� META���¿����� Block/Unblock�� ȣ��Ǿ�� �Ѵ�.
    IDE_ASSERT( mIsInitialized == ID_TRUE );

    IDE_TEST( smaDeleteThread::unlockAll() != IDE_SUCCESS );

    sStage = 0;
    for ( i = 0; i< mListCnt; i++ )
    {
        mListLock[i].unlock();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sStage )
    {
        case 1:
        for ( i = 0; i < mListCnt; i++ )
        {
            mListLock[i].unlock();
        }
        break;
    }
    IDE_POP();

    return IDE_FAILURE;

}

IDE_RC smaLogicalAger::addList( smTID         aTID,
                                idBool        aIsDDL,
                                smSCN       * aSCN,
                                UInt          aCondition,
                                smxOIDList  * aList,    
                                void       ** aAgerListPtr ) 
{

    smaOidList* sOidList;
    SInt        sListN;

#ifdef DEBUG
        if( aCondition == 0 ) // for dummy
        {
            IDE_DASSERT( SM_SCN_IS_INIT( *aSCN ) );
        }
        else
        {
            IDE_DASSERT( (aCondition == SM_OID_ACT_AGING_COMMIT)   ||
                         (aCondition == SM_OID_ACT_AGING_ROLLBACK) ); 
            IDE_DASSERT( SM_SCN_IS_NOT_INIT( *aSCN ) );
        }
#endif

    sListN = aTID % mListCnt;

    if( aAgerListPtr != NULL )
    {
        *(smaOidList**)aAgerListPtr = NULL;
    }

    /* SlotList�� �Ҵ�Ǿ����� Ȯ�� */
    if( mSlotList != NULL )
    {
        /* BUG-47367 Slot lock�� ȹ���Ѵ�.
         * �ش� lock�� �Ҵ�/�ݳ�/AgerList�� ����� ���� �Ѵ�. */
        mSlotList[sListN].lock();

        IDE_TEST( mSlotList[sListN].allocateSlots( 1,
                                                   (smmSlot**)&sOidList,
                                                   SMM_SLOT_LIST_MUTEX_NEEDLESS )
                  != IDE_SUCCESS );

        sOidList->mSCN       = *aSCN;
        sOidList->mErasable  = ID_FALSE;
        sOidList->mCondition = aCondition;
        sOidList->mNext      = NULL;
        sOidList->mTransID   = aTID;
        sOidList->mListN     = sListN; /* BUG-47367 �ݳ��Ҷ� List��ȣ�� �˾ƾ� �Ѵ�. */
        SM_SET_SCN_INFINITE_AND_TID(&(sOidList->mKeyFreeSCN), aTID);


        if(aAgerListPtr != NULL)
        {
            sOidList->mFinished  = ID_FALSE;
        }
        else
        {
            sOidList->mFinished  = ID_TRUE;
        }

        // BUG-15306
        // DummyOID�� ��쿣 add count�� ������Ű�� �ʴ´�.
        if( isDummyOID( sOidList ) != ID_TRUE )
        {
            acpAtomicInc64( &mAddCount );
        }

        if(aList != NULL)
        {
            IDE_ASSERT(aList->mOIDNodeListHead.mNxtNode != &(aList->mOIDNodeListHead));
            sOidList->mHead = aList->mOIDNodeListHead.mNxtNode;
            sOidList->mTail = aList->mOIDNodeListHead.mPrvNode;
        }
        else
        {
            sOidList->mHead = NULL;
            sOidList->mTail = NULL;
        }

        IDL_MEM_BARRIER;

        if( mHead[sListN] != NULL )
        {
            mHead[sListN]->mNext = sOidList;
            mHead[sListN]        = sOidList;
        }
        else
        {
            mHead[sListN]             = sOidList;
            mTailLogicalAger[sListN]  = mHead[sListN];
            mTailDeleteThread[sListN] = mHead[sListN];
        }

        mSlotList[sListN].unlock();

        if(aIsDDL == ID_TRUE)
        {
            IDE_TEST( addList( aTID,   
                               ID_FALSE,  // aIsDDL
                               aSCN, 
                               aCondition, 
                               NULL,      // aList,
                               NULL )     // aAgerListPtr
                      != IDE_SUCCESS );
        }
        
        if(aAgerListPtr != NULL)
        {
            *(smaOidList**)aAgerListPtr = sOidList;

        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/************************************************************************
 * Description :
 *    OID list�� OID�� �ϳ��� �޷� ������ �� �Լ��� ���ؼ�
 *    dummy OID�� �ϳ� �߰��Ѵ�. �̷��� �����ν�
 *    OID list�� ó�� �ȵǰ� ���� �ִ� �ϳ��� OID�� ó���ϵ��� �Ѵ�.
 *
 *    BUG-15306�� ���� �߰��� �Լ�
 ************************************************************************/
IDE_RC smaLogicalAger::addDummyOID( SInt aListN )
{
    SInt  sState = 0;
    smSCN sNullSCN;

    // OID list�� �ϳ��� ������ �� �Լ��� �ҷ�����.
    // ������ OID list�� ���÷� add�ǰ� �ֱ� ������
    // mutex�� ��� �ٽ��ѹ� �˻��ؾ� �Ѵ�.

    // addList�� ���ü� ����� ������ mSlotList�� lock�� ȹ���ؾ� �Ѵ�.
    mSlotList[aListN].lock();
    sState = 1;

    if( ( mHead[aListN] != NULL )                     &&
        ( mTailLogicalAger[aListN] == mHead[aListN] ) &&
        ( mTailLogicalAger[aListN]->mNext == NULL )   &&
        ( isDummyOID( mHead[aListN] ) == ID_FALSE ) )
    {
        SM_INIT_SCN( &sNullSCN );

        /* BUG-47367 addList���� ������ lock�� ȹ���Ϸ� �ϱ� ������
         * ȣ������ Ǯ���־���Ѵ�.
         * ���� �߰��� ���ο��� �Ǵ��ص� ������
         * dummy�� �߰��� �ִٰ� ū ������ �ȵǱ� ������ lock ������ �ٿ���. */
        sState = 0;

        mSlotList[aListN].unlock();

        IDE_TEST( addList( aListN,         // ���� TID ������ ���ο��� TID�� �̿� ListN�� �ٲ�� ������ �������
                           ID_FALSE,       // aIsDDL
                           &sNullSCN,      // aSCN
                           0,              // aCondition
                           NULL,           // aList,
                           NULL )          // aAgerListPtr 
                  != IDE_SUCCESS );
    }
    else
    {
        sState = 0;
        mSlotList[aListN].unlock();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        mSlotList[aListN].unlock();
    }

    return IDE_FAILURE;
}

void smaLogicalAger::setOIDListFinished( void* aOIDList, idBool aFlag)
{
    ((smaOidList*)aOIDList)->mFinished = aFlag;
}

/*
    Aging�� OID�� ����Ű�� Index Slot�� �����Ѵ�.
    logical ager���� ȣ���Ѵ�.

    [IN] aOIDList          - Aging�� OID List
    [OUT] aDidDelOldKey -
 */
IDE_RC smaLogicalAger::deleteIndex(smaOidList            * aOIDList,
                                   UInt                  * aDidDelOldKey )
{

    smxOIDNode*     sNode;
    smxOIDInfo*     sCursor = NULL;
    smxOIDInfo*     sFence;
    smcTableHeader* sLastTable;
    smcTableHeader* sTable;
    smcTableHeader* sNextTable;
    smcTableHeader* sCurTable = NULL;
    UInt            sStage = 0;

    sLastTable  = NULL;
    sTable      = NULL;
    sNode       = aOIDList->mTail;

    while( sNode != NULL )
    {
        sCursor = sNode->mArrOIDInfo + sNode->mOIDCnt - 1;
        sFence  = sNode->mArrOIDInfo - 1 ;
        for( ; sCursor != sFence; sCursor-- )
        {
            if( smxOIDList::checkIsAgingTarget( aOIDList->mCondition,
                                                sCursor ) == ID_FALSE )
            {
                continue;
            }

            idCore::acpAtomicInc64( &mAgingProcessedOIDCnt );
            IDE_DASSERT( checkAgingProcessedCnt() == ID_TRUE );

            if ( (( sCursor->mFlag & SM_OID_ACT_AGING_INDEX ) == SM_OID_ACT_AGING_INDEX ) &&
                 (( sCursor->mFlag & SM_OID_ACT_COMPRESSION ) == 0) )
            {
                IDE_ASSERT( sCursor->mTableOID != SM_NULL_OID );

                if (( sCurTable == NULL ) ||
                    ( sCursor->mTableOID != sCurTable->mSelfOID ))
                {
                    IDE_ASSERT( smcTable::getTableHeaderFromOID( sCursor->mTableOID,
                                                                 (void**)&sCurTable )
                                == IDE_SUCCESS );
                }

                if( sCurTable != sLastTable )
                {
                    sNextTable = sCurTable;

                    if(smcTable::isDropedTable4Ager( sNextTable , aOIDList->mSCN ) == ID_TRUE)
                    {
                        continue;
                    }

                    if( sStage == 1)
                    {
                        sStage = 0;
                        IDE_TEST( smcTable::unlatch( sTable )
                                  != IDE_SUCCESS );
                    }

                    sLastTable  = sNextTable;
                    sTable      = sNextTable;

                    IDE_TEST( smcTable::latchShared( sTable )
                              != IDE_SUCCESS );
                    sStage = 1;
                }

                /* Fit Wait */
                IDU_FIT_POINT( "2.BUG-41026@smaLogicalAger::run::wakeup_1" );
                IDU_FIT_POINT( "3.BUG-41026@smaLogicalAger::run::sleep_2" );

                IDE_ASSERT( sCursor->mTargetOID != SM_NULL_OID );
                IDE_ASSERT( sTable != NULL );

                /* Fit  Wait */
                IDU_FIT_POINT( "1.PROJ-1407@smaLogicalAger::deleteIndex" );

                //old version row�� �����״� index���� key slot�� �����Ѵ�.
                IDE_TEST_RAISE( freeOldKeyFromIndexes( sTable,
                                                       sCursor->mTargetOID,
                                                       aDidDelOldKey )
                                != IDE_SUCCESS, ERR_FREE_OIDKEY_FROM_IDX );
                sCursor->mFlag &= ~SM_OID_ACT_AGING_INDEX;
            }
        }
        sNode = sNode->mPrvNode;
    }

    if( sStage == 1 )
    {
        sStage = 0;
        IDE_TEST( smcTable::unlatch( sTable ) != IDE_SUCCESS );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FREE_OIDKEY_FROM_IDX );
    {
        sCursor->mFlag &= ~SM_OID_ACT_AGING_INDEX;
    }
    IDE_EXCEPTION_END;

    /* BUG-47619 ����� ���� ����, free slot�� �ʿ��� ager ���� ���*/
    ideLog::log( IDE_ERR_0,
                 "smaLogicalAger::deleteIndex\n"
                 "__PARALLEL_LOGICAL_AGER : %"ID_UINT32_FMT"\n"
                 "LOGICAL_AGER_COUNT_     : %"ID_UINT32_FMT"\n"
                 "MIN_LOGICAL_AGER_COUNT  : %"ID_UINT32_FMT"\n"
                 "MAX_LOGICAL_AGER_COUNT  : %"ID_UINT32_FMT"\n",
                 smuProperty::getParallelLogicalAger(),
                 smuProperty::getLogicalAgerCount(),
                 smuProperty::getMinLogicalAgerCount(),
                 smuProperty::getMaxLogicalAgerCount() );

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure of
     * index aging. */
    if( sCursor != NULL )
    {
        ideLog::log( IDE_ERR_0, 
                     "smaLogicalAger::deleteIndex\n"
                     "sCursor->mTableOID  : %"ID_UINT64_FMT"\n"
                     "sCursor->mTargetOID : %"ID_UINT64_FMT"\n"
                     "sCursor->mSpaceID   : %"ID_UINT32_FMT"\n"
                     "sCursor->mFlag      : %"ID_UINT32_FMT"\n",
                     sCursor->mTableOID,
                     sCursor->mTargetOID,
                     sCursor->mSpaceID,
                     sCursor->mFlag );
    }
  
    if ( sStage == 1 )
    {
        sStage = 0;
        IDE_ASSERT( smcTable::unlatch( sTable ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*
    Aging�� OID�� ����Ű�� Index Slot�� �����Ѵ�.
    DDL ��� �ӽ÷� �ش� ���̺� ���� ���� ��� ���� �� �� ȣ�� �ȴ�.
    BUG-47637 ���� deleteIndex���� �и��Ǿ���.

    [IN] aOIDList          - Aging�� OID List
    [IN] aAgingFilter      - Instant Aging�� ������ Filter
                             ( smaDef.h�ּ����� )
    [OUT] aDidDelOldKey -
 */
IDE_RC smaLogicalAger::deleteIndexInstantly( smaOidList            * aOIDList,
                                             smaInstantAgingFilter * aAgingFilter,
                                             UInt                  * aDidDelOldKey )
{

    smxOIDNode*     sNode;
    smxOIDInfo*     sCursor = NULL;
    smxOIDInfo*     sFence;
    smcTableHeader* sLastTable;
    smcTableHeader* sTable;
    smcTableHeader* sNextTable;
    smcTableHeader* sCurTable = NULL;
    UInt            sStage    = 0;

    sLastTable  = NULL;
    sTable      = NULL;
    sNode       = aOIDList->mTail;

    while( sNode != NULL )
    {
        sCursor = sNode->mArrOIDInfo + sNode->mOIDCnt - 1;
        sFence  = sNode->mArrOIDInfo - 1 ;
        for( ; sCursor != sFence; sCursor-- )
        {
            /* BUG-47637: SM_OID_DROP_INDEXó�� Aging ��������� 
             * old version ������ �ʿ����� ���� ��� */
            if( smxOIDList::checkIsAgingTarget( aOIDList->mCondition,
                                                sCursor ) == ID_FALSE )
            {
                continue;
            }

            if ( isAgingFilterTrue( aAgingFilter,
                                    sCursor->mSpaceID,
                                    sCursor->mTableOID ) == ID_FALSE )
            {
                continue;
            }

            IDE_ASSERT( sCursor->mTableOID != SM_NULL_OID );

            if (( sCurTable == NULL ) ||
                ( sCursor->mTableOID != sCurTable->mSelfOID ))
            {
                IDE_ASSERT( smcTable::getTableHeaderFromOID( sCursor->mTableOID,
                                                             (void**)&sCurTable )
                            == IDE_SUCCESS );
            }

            if( sCurTable != sLastTable )
            {
                sNextTable = sCurTable;

                if(smcTable::isDropedTable4Ager( sNextTable , aOIDList->mSCN ) == ID_TRUE)
                {
                    continue;
                }

                if( sStage == 1)
                {
                    sStage = 0;
                    IDE_TEST( smcTable::unlatch( sTable )
                              != IDE_SUCCESS );
                }

                sLastTable  = sNextTable;
                sTable      = sNextTable;

                IDE_TEST( smcTable::latchShared( sTable )
                          != IDE_SUCCESS );
                sStage = 1;
            }

            /* BUG-47637 ������� ����ϸ� delete thread aging ����̴�.
             * index�� �ƴϴ��� ���⿡�� count �Ѵ�. */
            idCore::acpAtomicInc64( &mAgingProcessedOIDCnt );
            IDE_DASSERT( checkAgingProcessedCnt() == ID_TRUE );

            if ( (( sCursor->mFlag & SM_OID_ACT_AGING_INDEX ) == SM_OID_ACT_AGING_INDEX  ) &&
                 (( sCursor->mFlag & SM_OID_ACT_COMPRESSION ) == 0) )
            {
                IDE_ASSERT( sCursor->mTargetOID != SM_NULL_OID );
                IDE_ASSERT( sTable != NULL );

                /* old version row�� �����״� index���� key slot�� �����Ѵ�.*/
                IDE_TEST( freeOldKeyFromIndexes( sTable,
                                                 sCursor->mTargetOID,
                                                 aDidDelOldKey ));

                /* �������� Aging�� �� Index Slot�� ����
                 * Aging�� �ѹ� �� �ϴ����� ������ Flag�� ����. */
                sCursor->mFlag &= ~SM_OID_ACT_AGING_INDEX;
            }
        }
        sNode = sNode->mPrvNode;
    }

    if( sStage == 1 )
    {
        sStage = 0;
        IDE_TEST( smcTable::unlatch( sTable ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-47619 ����� ���� ����, free slot�� �ʿ��� ager ���� ���*/
    ideLog::log( IDE_ERR_0,
                 "smaLogicalAger::deleteIndexInstantly\n"
                 "__PARALLEL_LOGICAL_AGER : %"ID_UINT32_FMT"\n"
                 "LOGICAL_AGER_COUNT_     : %"ID_UINT32_FMT"\n"
                 "MIN_LOGICAL_AGER_COUNT  : %"ID_UINT32_FMT"\n"
                 "MAX_LOGICAL_AGER_COUNT  : %"ID_UINT32_FMT"\n",
                 smuProperty::getParallelLogicalAger(),
                 smuProperty::getLogicalAgerCount(),
                 smuProperty::getMinLogicalAgerCount(),
                 smuProperty::getMaxLogicalAgerCount() );

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure of
     * index aging. */
    if( sCursor != NULL )
    {
        ideLog::log( IDE_ERR_0, 
                     "smaLogicalAger::deleteIndexInstantly\n"
                     "sCursor->mTableOID  : %"ID_UINT64_FMT"\n"
                     "sCursor->mTargetOID : %"ID_UINT64_FMT"\n"
                     "sCursor->mSpaceID   : %"ID_UINT32_FMT"\n"
                     "sCursor->mFlag      : %"ID_UINT32_FMT"\n",
                     sCursor->mTableOID,
                     sCursor->mTargetOID,
                     sCursor->mSpaceID,
                     sCursor->mFlag );
    }

    if ( sStage == 1 )
    {
        sStage = 0;
        IDE_ASSERT( smcTable::unlatch( sTable ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/*********************************************************************
  Name: sdaGC::freeOldKeyFromIndexes
  Description:
  old verion  row�� �����״� old index key slot��
  table �� �ε���(��)���� ���� free�Ѵ�.
 *********************************************************************/
IDE_RC smaLogicalAger::freeOldKeyFromIndexes(smcTableHeader*  aTableHeader,
                                             smOID            aOldVersionRowOID,
                                             UInt*            aDidDelOldKey)
{
    UInt             i ;
    UInt             sIndexCnt;
    smnIndexHeader * sIndexHeader;
    SChar          * sRowPtr;
    idBool           sIsExistFreeKey = ID_FALSE;
   
    IDE_ASSERT( smmManager::getOIDPtr( aTableHeader->mSpaceID,
                                       aOldVersionRowOID, 
                                       (void**)&sRowPtr)
                == IDE_SUCCESS );

    sIndexCnt =  smcTable::getIndexCount(aTableHeader);

    for(i = 0; i < sIndexCnt ; i++)
    {
        sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex(aTableHeader,i);

        if( smnManager::isIndexEnabled( sIndexHeader ) == ID_TRUE )
        {
            IDE_ASSERT( sIndexHeader->mHeader != NULL );
            IDE_ASSERT( sIndexHeader->mModule != NULL );
            IDE_ASSERT( sIndexHeader->mModule->mFreeSlot != NULL );

            IDE_TEST( sIndexHeader->mModule->mFreeSlot( sIndexHeader, // aIndex
                                                        sRowPtr,
                                                        ID_FALSE, /*aIgnoreNotFoundKey*/
                                                        &sIsExistFreeKey )
                      != IDE_SUCCESS );

            IDE_ASSERT( sIsExistFreeKey == ID_TRUE );
            
            *aDidDelOldKey |= 1;
        }//if
    }//for

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
  Name: sdaGC::checkOldKeyFromIndexes
  Description:
  old verion  row�� �����״� old index key slot�� �����ϴ��� �˻��Ѵ�.
 *********************************************************************/
IDE_RC smaLogicalAger::checkOldKeyFromIndexes(smcTableHeader*  aTableHeader,
                                              smOID            aOldVersionRowOID )
{
    UInt             i ;
    UInt             sIndexCnt;
    smnIndexHeader * sIndexHeader;
    SChar          * sRowPtr;
    UInt             sState = 0;

    IDE_TEST( smcTable::latchShared( aTableHeader )
              != IDE_SUCCESS );
    sState = 1;

    sIndexCnt    = smcTable::getIndexCount(aTableHeader);

    IDE_ASSERT( smmManager::getOIDPtr( aTableHeader->mSpaceID,
                                       aOldVersionRowOID, 
                                       (void**)&sRowPtr)
                == IDE_SUCCESS );

    for(i = 0; i < sIndexCnt ; i++)
    {
        sIndexHeader = (smnIndexHeader*)smcTable::getTableIndex(aTableHeader,i);

        if( sIndexHeader->mType == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID )
        {
            /* MRDB R-Tree�� mExistKey �Լ��� �����Ǿ� ���� �ʴ�. */
            continue;
        }

        if ( smnManager::isIndexEnabled( sIndexHeader ) == ID_FALSE )
        {
            continue;
        }

        IDE_ASSERT( sIndexHeader->mHeader != NULL );
        IDE_ASSERT( sIndexHeader->mModule != NULL );

        // BUG-47526 Free Slot�� Null�� ��찡 Exist Check�� �ȵȴ�.
        IDE_ASSERT( sIndexHeader->mModule->mFreeSlot != NULL );

        IDE_TEST( smnbBTree::checkExistKey( sIndexHeader, // aIndex
                                            sRowPtr )
                  != IDE_SUCCESS );
    }//for

    sState = 0;
    IDE_TEST( smcTable::unlatch( aTableHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        (void) smcTable::unlatch( aTableHeader );
    }

    return IDE_FAILURE;
}



// added for A4
// smaPhysical ager�� ���ŵʿ� ����
// �߰��� �Լ��̸�, free node list�� �����Ѵ�.
// ���� �����ʹ� smnManager�� �ִ� List�� ����Ѵ�.
// �ε��� ����(memory b+ tree,R tree)�� ����
// free node list�� ���� ������ �ȴ�.
void smaLogicalAger::addFreeNodes(void*     aFreeNodeList,
                                  smnNode*   aNodes)
{
    smnFreeNodeList * sFreeNodeList;

    IDE_DASSERT( aFreeNodeList != NULL);
    IDE_DASSERT( aNodes != NULL);

    IDE_ASSERT( mFreeNodeListMutex.lock( NULL ) == IDE_SUCCESS );
     
    sFreeNodeList = (smnFreeNodeList*) aFreeNodeList;

    if( aNodes != NULL )
    {
        sFreeNodeList->mAddCnt++;
        if(sFreeNodeList->mAddCnt == 0)
        {
            sFreeNodeList->mHandledCnt = 0;
        }
        // ���� system view scn�� �����´�.
        // node�� index���� ������ �Ŀ� view SCN�� �����ؾ� �Ѵ�.
        SMX_GET_SYSTEM_VIEW_SCN( &(aNodes->mSCN) );

        if( sFreeNodeList->mHead != NULL )
        {
            // empty node list�� �߰�.
            aNodes->mNext        = NULL;
            sFreeNodeList->mHead->mNext = aNodes;
            sFreeNodeList->mHead       = aNodes;
        }
        else
        {
            aNodes->mNext  = NULL;
            IDL_MEM_BARRIER;
            sFreeNodeList->mHead = aNodes;
            sFreeNodeList->mTail  = aNodes;
        }
    }

    IDE_ASSERT( mFreeNodeListMutex.unlock() == IDE_SUCCESS );
}

//added for A4.
//index�������� �ִ� free node list���� traverse�ϸ鼭
//���� ��尡 ������ ��带 �����.
void smaLogicalAger::freeNodesIfPossible()
{

    smnNode*         sTail;
    smnNode*         sHead = NULL;
    smnFreeNodeList* sCurFreeNodeList;
    smSCN            sMinSCN;
    smTID            sDummyTID;
    UInt             i;

    IDE_ASSERT( mFreeNodeListMutex.lock( NULL ) == IDE_SUCCESS );

    if( mBlockFreeNodeCount > 0 )
    {
        /* TASK-4990 changing the method of collecting index statistics
         * ������� �籸�� ������ Index�� ��ȸ�ϴ� ��.
         * FreeNode�� �������ν� Node�� ��Ȱ���� ����,
         * �̸� ���� ������� �籸���� ���� Index Node
         * ��ȸ�� TreeLock�� ���̵� ������ �� �ֵ��� �Ѵ�.*/
    }
    else
    {

        for(sCurFreeNodeList =  smnManager::mBaseFreeNodeList.mNext;
            sCurFreeNodeList != &smnManager::mBaseFreeNodeList;
            sCurFreeNodeList = sCurFreeNodeList->mNext)
        {
            if(sCurFreeNodeList->mTail == NULL)
            {
                continue;
            }

            // fix BUG-9620.
            ID_SERIAL_BEGIN(
                    sHead = sCurFreeNodeList->mHead );
            ID_SERIAL_END(
                    SMX_GET_MIN_MEM_VIEW( &sMinSCN, &sDummyTID ) );

            // BUG-39122 parallel logical aging������ logical ager��
            // view SCN�� ����ؾ� �մϴ�.
            if( mIsParallelMode == SMA_PARALLEL_LOGICAL_AGER_ON )
            {
                for( i = 0; i < mCreatedThreadCount; i++ )
                {
                    if( SM_SCN_IS_LT( &(mLogicalAgerList[i].mViewSCN) ,&sMinSCN ))
                    {
                        SM_SET_SCN( &sMinSCN, &(mLogicalAgerList[i].mViewSCN) );
                    }
                }
            }

            while( sCurFreeNodeList->mTail->mNext != NULL  )
            {
                if(SM_SCN_IS_LT(&(sCurFreeNodeList->mTail->mSCN),&sMinSCN))
                {
                    sTail = sCurFreeNodeList->mTail;
                    sCurFreeNodeList->mTail = sCurFreeNodeList->mTail->mNext;
                    sCurFreeNodeList->mHandledCnt++;
                    IDE_TEST( sCurFreeNodeList->mFreeNodeFunc( sTail )
                              != IDE_SUCCESS );
                    if(sTail == sHead)
                    {
                        break;
                    }

                }//if
                else
                {
                    break;
                }
            }//while
        }//for
    } // if( mBlockFreeNodeCount > 0 )

    IDE_ASSERT( mFreeNodeListMutex.unlock() == IDE_SUCCESS );

    return;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL( "smaLogicalAger" );

}

void smaLogicalAger::run()
{
    PDL_Time_Value   sTimeOut;
    idBool           sHandled = ID_FALSE;
    smSCN            sMinSCN;
    smaOidList*      sTail = NULL;
    smTID            sDummyTID;
    idBool           sState = ID_FALSE;
    idBool           sState2 = ID_FALSE;
    idBool           sDropTableLockState = ID_FALSE;
    UInt             sAgerWaitMin;
    UInt             sAgerWaitMax;
    UInt             sDidDelOldKey = 0;
    UInt             sListN = mThreadID % smaLogicalAger::mListCnt  ;

  startPos:
    sState              = ID_FALSE;
    sDropTableLockState = ID_FALSE;

    sAgerWaitMin = smuProperty::getAgerWaitMin();
    sAgerWaitMax = smuProperty::getAgerWaitMax();

    while( mContinue == ID_TRUE )
    {
        /* BUG-47601: ���� ��ȸ �ϰ� ���� Sleep�ѹ�.. */
        if ( sListN == (mThreadID % smaLogicalAger::mListCnt) )
        {
            if( sHandled == ID_TRUE )
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

            if( mTimeOut > sAgerWaitMin )
            {
                sTimeOut.set( 0, mTimeOut );
                idlOS::sleep( sTimeOut );
            }
        }

        if ( smuProperty::isRunMemGCThread() == SMU_THREAD_OFF )
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

        /* BUG-35179 Add property for parallel logical ager */
        IDE_TEST( lockWaitForNoAccessAftDropTbl() != IDE_SUCCESS );
        sDropTableLockState = ID_TRUE;

        IDU_FIT_POINT("1.smaLogicalAger::run");

        /* BUG-47367 �̹��� ó���� List�� Lock�� ȹ���Ѵ�.
         * �� List���� Lock�� ��� ������ Parallel Ager�� ���� ���� �ʾƵ�
         * ���� �ٸ� Ager�� �ٸ� List�� ���� �ϴ� ��� Parallel ó�� �����Ѵ�.
         * Parallel�� �������� List lock��� ��ü lock�� �⵵�� ���ָ� �ȴ�.
         */
//        IDE_TEST( mListLock[sListN].lock(NULL) !=IDE_SUCCESS );
//        sState = ID_TRUE;

        mListLock[sListN].trylock( sState );

        if ( sState != ID_TRUE )
        {
            IDE_CONT( skip_list );
        }

        if( mTailLogicalAger[sListN] != NULL )
        {
            // BUG-15306
            // mTailLogicalAger�� �ϳ� �ۿ� ������ ó������ ���ϹǷ�
            // DummyOID�� �ϳ� �޾Ƽ� �ΰ��� �ǰ� �Ѵ�.
            if( ( isDummyOID( mHead[sListN] ) != ID_TRUE ) && ( mTailLogicalAger[sListN]->mNext == NULL ) )
            {
                IDE_TEST( addDummyOID( sListN ) != IDE_SUCCESS );
            }

            // fix BUG-9620.
            SMX_GET_MIN_MEM_VIEW( &sMinSCN, &sDummyTID );

            /* ���ġ�� ��Ȯ���� �˻��غ� */
#if defined(DEBUG)

            /* BUG-47367 List�� ������ �� ��� ���ü� ������ ���� �� �ֱ� ������
             * �ش� �۾��� List 1�� �϶��� �����ϵ��� �Ѵ�.*/
            /* 
             * BUG-35179 Add property for parallel logical ager
             * ���� ���ġ �˻�� LogicalAger�� parallel�� ��������
             * ���� ���� ��ȿ�ϴ�. parallel�� �����Ұ�� ���ü������� ��񵿾�
             * ���ġ�� ��ġ���� ���� �� �ֱ� �����̴�.
             */
            if( ( mTailLogicalAger[sListN]->mNext == NULL ) &&
                ( isDummyOID( mTailLogicalAger[sListN] ) == ID_TRUE ) &&
                ( mIsParallelMode == SMA_PARALLEL_LOGICAL_AGER_OFF ) &&
                ( mListCnt == 1) )
            {
                /* Transaction���� ���ü��� ���� Lock */
                /* BUG-47367 addList���� ���ü��� �����ϸ� ��. List�� 1�� ���̶� �׻� 0��. */
                mSlotList[0].lock();
                /* Lock �� �ٽ� Ȯ���غ� */
                if( ( mTailLogicalAger[sListN]->mNext == NULL ) &&
                    ( isDummyOID( mTailLogicalAger[sListN] ) == ID_TRUE ) )
                {
                    IDE_ASSERT( mAddCount == mHandledCount );
                    IDE_ASSERT( checkAgingProcessedCnt() == ID_TRUE );
                    IDE_ASSERT( mAgingRequestOIDCnt == mAgingProcessedOIDCnt );
                }
                mSlotList[0].unlock();
            }
#endif

            while( ( mTailLogicalAger[sListN]->mNext != NULL )  &&
                   ( mTailLogicalAger[sListN]->mFinished == ID_TRUE ) )
            {
                if( SM_SCN_IS_LT( &(mTailLogicalAger[sListN]->mSCN), &sMinSCN ) )
                {
                    sTail                    = mTailLogicalAger[sListN];
                    mTailLogicalAger[sListN] = mTailLogicalAger[sListN]->mNext;

                    sDidDelOldKey = 0;

                    /* BUG-35179 Add property for parallel logical ager */
                    if( mIsParallelMode == SMA_PARALLEL_LOGICAL_AGER_ON )
                    {
                        // �ٸ� ager�� list�� ó���� �� �ֵ��� list lock�� Ǯ���ش�.
                        sState = ID_FALSE;
                        IDE_TEST( mListLock[sListN].unlock() != IDE_SUCCESS );

                        /* BUG-39122 parallel logical aging������
                         * logical ager�� view SCN�� ����ؾ� �մϴ�.
                         * ���� system view scn�� �����´�.*/
                        SMX_GET_SYSTEM_VIEW_SCN( &mViewSCN );
                    }

                    IDE_TEST( deleteIndex( sTail,
                                           &sDidDelOldKey )
                              != IDE_SUCCESS );

                    // delete key slot�� �Ϸ��� ��������
                    // system view scn�� ����.
                    if( sDidDelOldKey != 0 )
                    {
                        SMX_GET_SYSTEM_VIEW_SCN( &(sTail->mKeyFreeSCN) );
                    }
                    else
                    {
                        // old key�� ���� ���� ���� ��Ͻ�����
                        // physical aging���� �Ϸ��� �����̴�.
                        SM_SET_SCN( &( sTail->mKeyFreeSCN ), &( sTail->mSCN ) );
                    }

                    // BUG-15306
                    // dummy oid�� ó���� �� mHandledCount�� ������Ű�� �ʴ´�.
                    if( isDummyOID( sTail ) != ID_TRUE )
                    {
                        /* BUG-41026 */
                        acpAtomicInc( &mHandledCount );
                    }

                    sTail->mErasable = ID_TRUE;
                    sHandled         = ID_TRUE;
                    
                    /* BUG-35179 Add property for parallel logical ager */
                    /* BUG-41026 lock ��ġ ���� */
                    if ( mIsParallelMode == SMA_PARALLEL_LOGICAL_AGER_ON )
                    {
                        SM_MAX_SCN( &mViewSCN );

                        IDE_TEST( mListLock[sListN].lock(NULL) != IDE_SUCCESS );
                        sState = ID_TRUE;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }//if
                else
                {
                    //BUG-17371 [MMDB] Aging�� �и���� System�� ����ȭ ��
                    //                 Aging�� �и��� ������ �� ��ȭ��.
                    mSleepCountOnAgingCondition++;
                    break;
                }//else
            }//while

            //free nodes�鿡 ����  �����õ�.
            freeNodesIfPossible();
        }//if

        sState = ID_FALSE;
        IDE_TEST( mListLock[sListN].unlock() != IDE_SUCCESS );

        IDE_EXCEPTION_CONT( skip_list );

        /* BUG-35179 Add property for parallel logical ager */
        sDropTableLockState = ID_FALSE;
        IDE_TEST( unlockWaitForNoAccessAftDropTbl() != IDE_SUCCESS );
        
        sListN++;
        if ( sListN >= mListCnt )
        {
            sListN = 0;
        }
    }

    IDE_TEST( lock() != IDE_SUCCESS );
    sState2 = ID_TRUE;

    mRunningThreadCount--;

    // BUG-47526 Logical Ager�� ���� �� �� node�� ��� �����ϴ� �ڵ带 �����մϴ�.
    // Delete Thread ���� Thread����ÿ� ��� Delete�� ��� ���� ��
    // SCN�� Ȯ������ �����Ƿ� �ʿ���� �۾��Դϴ�.

    sState2 = ID_FALSE;
    IDE_TEST( unlock() != IDE_SUCCESS );

    return;

    IDE_EXCEPTION_END;

    /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure of
     * index aging. */
    if( sTail != NULL )
    {
        ideLog::log( IDE_ERR_0, 
                     "smaLogicalAger::run\n"
                     "sTail->mSCN         : %"ID_UINT64_FMT"\n"
                     "sTail->mKeyFreeSCN  : %"ID_UINT64_FMT"\n"
                     "sTail->mErasable    : %"ID_UINT32_FMT"\n"
                     "sTail->mFinished    : %"ID_UINT32_FMT"\n"
                     "sTail->mCondition   : %"ID_UINT32_FMT"\n"
                     "sTail->mTransID     : %"ID_UINT32_FMT"\n",
                     SM_SCN_TO_LONG( sTail->mSCN ),
                     SM_SCN_TO_LONG( sTail->mKeyFreeSCN ),
                     sTail->mErasable,
                     sTail->mFinished,
                     sTail->mCondition,
                     sTail->mTransID );
    }

    IDE_ASSERT( 0 );

    IDE_PUSH();
    
    if( sState == ID_TRUE )
    {
        IDE_ASSERT( mListLock[sListN].unlock() == IDE_SUCCESS );
    }

    if( sState2 == ID_TRUE )
    {
        IDE_ASSERT( unlock() == IDE_SUCCESS );
    }

    if( sDropTableLockState == ID_TRUE )
    {
        IDE_ASSERT( unlockWaitForNoAccessAftDropTbl() == IDE_SUCCESS );
    }

    IDE_POP();

    IDE_WARNING(SM_TRC_LOG_LEVEL_WARNNING,
                SM_TRC_MAGER_WARNNING2);

    goto startPos;

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
void smaLogicalAger::waitForNoAccessAftDropTbl()
{
    UInt i;

    /* BUG-35179 Add property for parallel logical ager 
     * drop table, drop tablespace���� ���ü� ������ �ذ��ϱ� ���� �� thread����
     * lock�� �߰��Ѵ�.
     *
     * parallel logical ager�ڵ� �߰������� ������ lock(), unlock()�Լ���
     * OidList�� ��������� ������ ���� lock���Ҹ� �Ѵ�. ����
     * �� logical ager thread ���� �����ϴ� mWaitForNoAccessAftDropTblLock��
     * ��ƾ����� drop table, drop tablespace���� ���ü� ��� �ȴ�.
     */ 

    for( i = 0; i < mRunningThreadCount; i++ )
    {
        IDE_ASSERT( mLogicalAgerList[i].lockWaitForNoAccessAftDropTbl()
                    == IDE_SUCCESS );

        IDE_ASSERT( mLogicalAgerList[i].unlockWaitForNoAccessAftDropTbl()
                    == IDE_SUCCESS );
    }

    /* BUG-15969: �޸� Delete Thread���� ������ ����
     * Delete Thread�� �������� �ʴ´ٴ� ���� �����ؾ� �Ѵ�.*/
    smaDeleteThread::waitForNoAccessAftDropTbl();

    /*
       BUG-42760
       LEGACY Transaction�� OID LIst�� AGER ���� ó������ �ʰ�,
       Transaction�� �����Ҷ� Transactino���� ���� ó���Ѵ�.
       ����, AGER�� ���������� LEGACY Transaction������ Drop�� Table�� OID��
       ó������ �ʵ��� lock�� �߰��Ѵ�.
     */
    smxLegacyTransMgr::waitForNoAccessAftDropTbl();
}



/*
    Aging Filter���ǿ� �����ϴ��� üũ

    [IN] aAgingFilter - �˻��� Filter ����
    [IN] aTBSID       - Aging�Ϸ��� OID�� ���� Tablespace�� ID
    [IN] aTableOID    - Aging�Ϸ��� OID�� ���� Table�� OID

    <RETURN> - ���ǿ� ������ ��� ID_TRUE, �ƴϸ� ID_FALSE
 */
idBool smaLogicalAger::isAgingFilterTrue( smaInstantAgingFilter * aAgingFilter,
                                          scSpaceID               aTBSID,
                                          smOID                   aTableOID )
{
    IDE_DASSERT( aAgingFilter != NULL );

    idBool sIsFilterTrue = ID_FALSE;

    // Ư�� Tablespace���� OID�� ���ؼ��� Aging�ǽ�?
    if ( aAgingFilter->mTBSID != SC_NULL_SPACEID )
    {
        if(aTBSID == aAgingFilter->mTBSID)
        {
            sIsFilterTrue = ID_TRUE;
        }
    }
    else
    {
        // Ư�� Table���� OID�� ���ؼ��� Aging�ǽ�?
        if ( aAgingFilter->mTableOID != SM_NULL_OID )
        {
            if(aTableOID == aAgingFilter->mTableOID)
            {
                sIsFilterTrue = ID_TRUE;
            }
        }
        else
        {
            // Aging Filter�� mTableOID�� mSpaceID�� �ϳ���
            // �����Ǿ� �־�� �Ѵ�.

            // �� ���� �Ѵ� �������� ���� ����̹Ƿ�
            // ASSERT�� ���δ�.
            IDE_ASSERT(0);
        }
    }

    return sIsFilterTrue;
}



/*
    Instant Aging Filter�� �ʱ�ȭ�Ѵ� ( smaDef.h ���� )
 */
void smaLogicalAger::initInstantAgingFilter( smaInstantAgingFilter * sAgingFilter )
{
    IDE_DASSERT( sAgingFilter != NULL );

    sAgingFilter->mTBSID    = SC_NULL_SPACEID;
    sAgingFilter->mTableOID = SM_NULL_OID;
}



/*
    Ư�� Table���� OID�� Ư�� Tablespace���� OID�� ���� ��� Aging�� �ǽ��Ѵ�.

    [Ư�̻���]
       Aging�Ϸ��� OID�� SCN�� Active Transaction���� Minimum ViewSCN ����
       ũ���� Aging�� �ǽ��Ѵ�.

    [���ǻ���]
       �� Function�� ���� �ٸ� �ʿ��� Aging Filter�� ������ Table�̳�
       Tablespace�� ���ؼ� �ٸ� Transaction�� �������� �ʴ´ٴ� ���� ����
       �ؾ� �Ѵ�.

       Table�̳� Tablespace��  X-Lock�� ��� ������ �̸� ������ �� �ִ�.

    [IN] aAgingFilter - Aging�� ���� ( smaDef.h ���� )
 */
IDE_RC smaLogicalAger::agingInstantly( smaInstantAgingFilter * aAgingFilter )
{
    smaOidList*      sCurOIDList;
    UInt             sDidDelOldKey = 0;
    UInt             sListN = 0;
    UInt             sLocked = 0;

    IDE_DASSERT( aAgingFilter != NULL );

    /* BUG-41026 */
    IDU_FIT_POINT( "1.BUG-41026@smaLogicalAger::agingInstantly::sleep_1" );

    /* BUG-47637 Instantly Aging ���� delete Thread���� ó���ϴ� �� ���� ��� Lock�� ��ƾ� �Ѵ�.
     * Logical Ager�� Processed Count�� counting�ϴ� ���� smaLogicalAger::deleteIndexInstantly() �̰�,
     * smaLogicalAger::deleteIndex() ���� Processed Count�� counting �� �� �����ϴ� Flag��
     * smaDeleteThread::deleteInstantly() ���� ���ŵȴ�.
     * smaLogicalAger::deleteIndexInstantly() �� smaDeleteThread::deleteInstantly() ���̿�
     * smaLogicalAger::deleteIndex() �� ������ Processed Count�� �ߺ� ��� �ȴ�.*/
    for ( sLocked = 0; sLocked < mListCnt; sLocked++ )
    {
        mListLock[ sLocked ].lock(NULL);
    }

    for ( sListN = 0 ; sListN < mListCnt; sListN ++ )
    {
        /* BUG-32655 [sm-mem-index] The MMDB Ager must not ignore the failure of
         * index aging.
         *
         * Ager�� AgingJob�� Head�� �޸��ϴ�. �׷��� Tail���� �Ѿư��� �մϴ�.
         * �׸����� ǥ�����ڸ�....
         *
         *      Next Next Next
         * mTail -> A -> B -> C -> mHead
         * ���� mHead���� mNext�� �Ѿư��� �ƹ��͵� �����ϴ�. */
        sCurOIDList = mTailLogicalAger[sListN];

        while(sCurOIDList != NULL)
        {
            IDE_TEST( deleteIndexInstantly( sCurOIDList,
                                            aAgingFilter,
                                            &sDidDelOldKey )
                      != IDE_SUCCESS );

            sCurOIDList = sCurOIDList->mNext;
        }
    }

    /* delete Thread ���ο����� ��� List�� Ž���ϵ��� �Ǿ� �ִ�. */
    /* BUG-32780 */
    IDE_TEST(smaDeleteThread::deleteInstantly( aAgingFilter )
             != IDE_SUCCESS);

    for ( sListN = 0 ; sListN < sLocked ; sListN++ )
    {
        mListLock[ sListN ].unlock();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    for ( sListN = 0 ; sListN < sLocked ; sListN++ )
    {
        mListLock[ sListN ].unlock();
    }

    return IDE_FAILURE;
}


/*
    Ư�� Tablespace�� ���� OID�� ���ؼ� ��� Aging�ǽ�

    [IN] aTBSID - Aging�� OID�� ���� Tablespace�� ID
 */
IDE_RC smaLogicalAger::doInstantAgingWithTBS( scSpaceID aTBSID )
{
    smaInstantAgingFilter sAgingFilter;

    initInstantAgingFilter( & sAgingFilter );

    sAgingFilter.mTBSID = aTBSID;

    IDE_TEST( agingInstantly( & sAgingFilter ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Ư�� Table�� ���� OID�� ���ؼ� ��� Aging�ǽ�

    [IN] aTBSID - Aging�� OID�� ���� Tablespace�� ID
 */
IDE_RC smaLogicalAger::doInstantAgingWithTable( smOID aTableOID )
{
    smaInstantAgingFilter sAgingFilter;

    initInstantAgingFilter( & sAgingFilter );

    sAgingFilter.mTableOID = aTableOID;

    IDE_TEST( agingInstantly( & sAgingFilter ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Ager Thread�� �ϳ� �߰��Ѵ�.
 */
IDE_RC smaLogicalAger::addOneAger()
{
    smaLogicalAger * sEmptyAgerObj = NULL;

    // Shutdown�߿��� �� �Լ��� ȣ��� �� ����.
    // mRunningThreadCount�� Shutdown�ÿ��� ���ҵǸ�
    // �ý��� ��߿��� mCreatedThreadCount�� �׻� ���� ���̾�� �Ѵ�.
    IDE_ASSERT( mCreatedThreadCount == mRunningThreadCount );
    
    // �ϳ� ���� �����̹Ƿ� smuProperty::getMaxAgerCount() ���ٴ� �۾ƾ���
    IDE_ASSERT( mCreatedThreadCount < smuProperty::getMaxLogicalAgerCount() );
    IDE_ASSERT( mCreatedThreadCount >= smuProperty::getMinLogicalAgerCount() );

    sEmptyAgerObj = & mLogicalAgerList[ mCreatedThreadCount ];

    new (sEmptyAgerObj) smaLogicalAger;
    
    IDE_TEST( sEmptyAgerObj->initialize( mCreatedThreadCount ) != IDE_SUCCESS );

    mCreatedThreadCount++;
    mRunningThreadCount++;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
    
/*
    Ager Thread�� �ϳ� �����Ѵ�.
 */
IDE_RC smaLogicalAger::removeOneAger()
{
    smaLogicalAger * sLastAger = NULL;

    // Shutdown�߿��� �� �Լ��� ȣ��� �� ����.
    // mRunningThreadCount�� Shutdown�ÿ��� ���ҵǸ�
    // �ý��� ��߿��� mCreatedThreadCount�� �׻� ���� ���̾�� �Ѵ�.
    IDE_ASSERT( mCreatedThreadCount == mRunningThreadCount );

    IDE_ASSERT( mCreatedThreadCount <= smuProperty::getMaxLogicalAgerCount() );
    // �ϳ� ���� �����̹Ƿ� smuProperty::getMinAgerCount() ���ٴ� Ŀ����
    IDE_ASSERT( mCreatedThreadCount > smuProperty::getMinLogicalAgerCount() );

    mCreatedThreadCount--;
    sLastAger = & mLogicalAgerList[ mCreatedThreadCount ];

    // run�Լ� �����Ű�� thread join���� �ǽ�
    IDE_TEST( sLastAger->shutdown() != IDE_SUCCESS );

    // run�Լ��� ���������鼭 mRunningThreadCount�� ���ҵȴ�
    // �׷��Ƿ� join�Ŀ� Created�� Running������ ���ƾ� �Ѵ�.
    IDE_ASSERT( mCreatedThreadCount == mRunningThreadCount );
    
    IDE_TEST( sLastAger->destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*
    Ager Thread�� ������ Ư�� ������ �ǵ��� ����,�����Ѵ�.
    
    [IN] aAgerCount - Ager thread ����,�������� Ager����

    [����] �� �Լ��� ������ ���� �������
            LOGICAL AGER Thread �� ���� ��ɿ� ���� ����ȴ�.

            ALTER SYSTEM SET LOGICA_AGER_COUNT_ = 10;
 */
IDE_RC smaLogicalAger::changeAgerCount( UInt aAgerCount )
{

    UInt sStage = 0;

    IDE_ASSERT( mCreatedThreadCount == mRunningThreadCount );

    // �� �Լ� ȣ������ aAgerCount�� ���� üũ �Ϸ��� ����.
    // AgerCount�� Min,Max���� ���ʿ� �־�� �Ѵ�.
    IDE_ASSERT( aAgerCount <= smuProperty::getMaxLogicalAgerCount() );
    IDE_ASSERT( aAgerCount >= smuProperty::getMinLogicalAgerCount() );

    // mAgerCountChangeMutex �� ����
    // - ALTER SYSTEM SET LOGICA_AGER_COUNT_ ������ ���ÿ� ������� �ʵ���
    IDE_TEST( lockChangeMtx() != IDE_SUCCESS );
    sStage = 1;

    while ( mRunningThreadCount < aAgerCount )
    {
        IDE_TEST( addOneAger() != IDE_SUCCESS );
    }

    while ( mRunningThreadCount > aAgerCount )
    {
        IDE_TEST( removeOneAger() != IDE_SUCCESS );
    }

    sStage = 0;
    IDE_TEST( unlockChangeMtx() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sStage != 0 )
    {
        IDE_ASSERT( unlockChangeMtx() == IDE_SUCCESS );
    }
    
    IDE_POP();
    
    return IDE_FAILURE;
}
