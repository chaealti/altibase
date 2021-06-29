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
 * $Id$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer 
 **********************************************************************/

/******************************************************************************
 * Description :
 *    sdsBufferArea ��  BCB�� ����� �����Ѵ�.
 ******************************************************************************/
#include <smu.h>
#include <smErrorCode.h>
#include <sds.h>

/******************************************************************************
 * Description :
 *  aExtentCnt [IN] : Extent �� (Secondary Buffer�� �ͽ���Ʈ������ �����Ѵ�)
 *  aBCBCnt    [IN] : page cnt ��: BCB�� ��
 ******************************************************************************/
IDE_RC sdsBufferArea::initializeStatic( UInt aExtentCnt, 
                                        UInt aBCBCnt ) 
{
    sdsBCB    * sBCB;
    UInt        sBCBID  = 0;
    SInt        sState  = 0;
    UInt        i;
    SChar       sTmpName[128];

    mBCBCount       = 0;
    mBCBExtentCount = aExtentCnt;
    mMovedownPos    = 0;
    mFlushPos       = 0;
    mWaitingClientCount = 0;

    IDU_FIT_POINT_RAISE( "sdsBufferArea::initialize::malloc1", 
                          ERR_INSUFFICIENT_MEMORY );

    /* BCB Arrary */
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (UInt)ID_SIZEOF(sdsBCB*) * aBCBCnt,
                                       (void**)&mBCBArray ) 
                    != IDE_SUCCESS,
                    ERR_INSUFFICIENT_MEMORY );
    sState = 1;

    /* BCB�ʱ�ȭ �� �Ҵ� */
    IDE_TEST( mBCBMemPool.initialize( IDU_MEM_SM_SDS,
                                      (SChar*)"SDS_BCB_MEMORY_POOL",
                                      1,                 // Multi Pool Cnt
                                      ID_SIZEOF(sdsBCB), // Element size
                                      aBCBCnt,           // Element count In Chun 
                                      ID_UINT_MAX,       // chunk limit 
                                      ID_FALSE,          // use mutex
                                      8,                 // align byte
                                      ID_FALSE,			 // ForcePooling
                                      ID_TRUE,			 // GarbageCollection
                                      ID_TRUE,			 // HWCacheLine
                                      IDU_MEMPOOL_TYPE_LEGACY  /* mempool type */) 
                != IDE_SUCCESS);			
    sState = 2;

    /* BCB�ʱ�ȭ �� �Ҵ� */
    for( i = 0; i < aBCBCnt ; i++ )
    {
        IDE_TEST( mBCBMemPool.alloc((void**)&sBCB) != IDE_SUCCESS );

        IDE_TEST( sBCB->initialize( sBCBID ) != IDE_SUCCESS );
        
        mBCBArray[sBCBID] = sBCB;
        sBCBID++;
    }
    mBCBCount = sBCBID;
    
    // wait mutex �ʱ�ȭ
    idlOS::snprintf( sTmpName,
                     ID_SIZEOF( sTmpName ),
                     "SECONDARY_EXTENT_WAIT_MUTEX" );
    
    IDE_TEST( mMutexForWait.initialize( 
                sTmpName,
                IDU_MUTEX_KIND_POSIX,
                IDV_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_EXTENT_WAIT )
              != IDE_SUCCESS );
    sState = 3;
 
    // condition variable �ʱ�ȭ
    idlOS::snprintf( sTmpName,
                     ID_SIZEOF(sTmpName),
                     "SECONDARY_BUFFER_COND" );

    IDE_TEST_RAISE( mCondVar.initialize(sTmpName) != IDE_SUCCESS,
                    ERR_COND_VAR_INIT );
    sState = 4;

    /* �ͽ���Ʈ ���� */
    idlOS::snprintf( sTmpName,
                     ID_SIZEOF( sTmpName ),
                     "SECONDARY_FLUSH_EXTENT_MUTEX" );
    
    IDE_TEST( mExtentMutex.initialize( 
                sTmpName,
                IDU_MUTEX_KIND_NATIVE,
                IDV_WAIT_INDEX_LATCH_FREE_DRDB_SECONDARY_BUFFER_EXTENT_MUTEX )
              != IDE_SUCCESS );
    sState = 5;

    IDU_FIT_POINT_RAISE( "sdsBufferArea::initialize::malloc2", 
                          ERR_INSUFFICIENT_MEMORY );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_SDS,
                                       (ULong)ID_SIZEOF(sdsExtentStateType) *
                                       mBCBExtentCount,
                                       (void**)&mBCBExtState ) 
                   != IDE_SUCCESS,
                   ERR_INSUFFICIENT_MEMORY );
    sState = 6;

    /* �ʱ�ȭ */ 
    for( i = 0 ; i< mBCBExtentCount ; i++ )
    {
        mBCBExtState[i] = SDS_EXTENT_STATE_FREE; 
    } 
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INSUFFICIENT_MEMORY );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION( ERR_COND_VAR_INIT );
    {
        IDE_SET( ideSetErrorCode( smERR_FATAL_ThrCondInit ) );
    }
    IDE_EXCEPTION_END;

    for( ; sBCBID > 0 ; sBCBID-- )
    {
        sBCB = mBCBArray[sBCBID-1];

        IDE_ASSERT( sBCB->destroy() == IDE_SUCCESS );
        mBCBMemPool.memfree( sBCB );
    }
    
    switch( sState )
    {
        case 6:
            IDE_ASSERT( iduMemMgr::free( mBCBExtState ) == IDE_SUCCESS );
        case 5:
            IDE_ASSERT( mExtentMutex.destroy() == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( mCondVar.destroy() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( mMutexForWait.destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mBCBMemPool.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( iduMemMgr::free( mBCBArray ) == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 ******************************************************************************/
IDE_RC sdsBufferArea::destroyStatic()
{
    UInt     i;
    sdsBCB  *sBCB;

    for( i = 0; i < mBCBCount ; i++ )
    {
        sBCB = mBCBArray[i];
        IDE_ASSERT( sBCB->destroy() == IDE_SUCCESS );
        mBCBMemPool.memfree( sBCB );
    }

    IDE_ASSERT( iduMemMgr::free( mBCBExtState ) == IDE_SUCCESS );
    IDE_ASSERT( mExtentMutex.destroy() == IDE_SUCCESS );
    IDE_ASSERT( mCondVar.destroy() == 0 );
    IDE_ASSERT( mMutexForWait.destroy() == IDE_SUCCESS );
    IDE_ASSERT( mBCBMemPool.destroy() == IDE_SUCCESS );
    IDE_ASSERT( iduMemMgr::free( mBCBArray ) == IDE_SUCCESS );
    mBCBArray = NULL;

    return IDE_SUCCESS;
}

/******************************************************************************
 * Description : Secondary Flusher �� flush �ؾ��� Extent�� �ִ��� Ȯ�� 
 *  aExtIndex [OUT] - flush ��� Extent
 ******************************************************************************/
idBool sdsBufferArea::getTargetFlushExtentIndex( UInt *aExtIndex )
{
    sdsExtentStateType sState;

    idBool sIsSuccess = ID_FALSE;   

    IDE_ASSERT( mExtentMutex.lock( NULL /*aStatistics*/ ) == IDE_SUCCESS );

    sState = mBCBExtState[mFlushPos];

    switch( sState )
    {
        /* sdbFlusher�� movedown�� �Ϸ� ������ ���� datafile�� �ݿ� ����
           Ext�� �����´� */
        case SDS_EXTENT_STATE_MOVEDOWN_DONE :
            *aExtIndex = mFlushPos;
            sIsSuccess  = ID_TRUE;
            mBCBExtState[mFlushPos] = SDS_EXTENT_STATE_FLUSH_ING;
            mFlushPos = (mFlushPos+1) % mBCBExtentCount;
            break;

        /* ������ ���ų� 
           secondary Buffer�� Flusher ��û �����ų� ���Ƽ� �ѹ����� ����
           �� ��Ȳ. */
        case SDS_EXTENT_STATE_FREE:
        case SDS_EXTENT_STATE_FLUSH_ING:
        case SDS_EXTENT_STATE_FLUSH_DONE:
            /* nothing to do */
            break; 

        /* sdbFlusher�� �հ��� ���Ⱦ��� ������ �����Ͽ� flush��ų�� �ֵ���
           mFlushPos ���� ���� */
        case SDS_EXTENT_STATE_MOVEDOWN_ING:
            /* nothing to do */
            break; 

        default:
            ideLog::log( IDE_SERVER_0, 
                    "Unexpected BCBExtent [%d] : %d", 
                    mFlushPos, sState );
            IDE_DASSERT( 0 );
            break;
    }

    IDE_ASSERT( mExtentMutex.unlock() == IDE_SUCCESS );

     /* �ε����� �޾� ���� ���� ��Ȳ�̸� False ��ȯ */
    return sIsSuccess;
}

/******************************************************************************
 * Description : sdbFlusher�� secondary Buffer�� movedown �� idx Ȯ��
 *  aExtIndex -[OUT]  
 ******************************************************************************/
IDE_RC sdsBufferArea::getTargetMoveDownIndex( idvSQL  * aStatistics, 
                                              UInt    * aExtIndex )
{
    sdsExtentStateType sState;
    idBool             sIsSuccess;   
    PDL_Time_Value     sTV;

retry:
    IDE_ASSERT( mExtentMutex.lock( aStatistics ) == IDE_SUCCESS );

    sIsSuccess = ID_FALSE;

    sState = mBCBExtState[mMovedownPos];

    switch ( sState )
    {
            /* �ʱ�ȭ�� ���İų�
               �̹� datafile�� �ݿ��� �Ǿ�����      
               �ش� Ext�� �����´�   */
        case SDS_EXTENT_STATE_FREE:
        case SDS_EXTENT_STATE_FLUSH_DONE:
            *aExtIndex = mMovedownPos;
            sIsSuccess = ID_TRUE;
            mBCBExtState[mMovedownPos] = SDS_EXTENT_STATE_MOVEDOWN_ING;
            mMovedownPos = (mMovedownPos+1) % mBCBExtentCount;
            break;

            /* secondary Buffer�� Flusher �� ��û �з��� �ѹ����� ����
               ����� ��Ȳ�̴�.
               �� �������� ã�� ���� secondary Flusher�� ������ ��ٸ���. */  
        case SDS_EXTENT_STATE_MOVEDOWN_ING:
        case SDS_EXTENT_STATE_MOVEDOWN_DONE:
            /* nothing to do */
            break;

            /* sdsFlusher�� datafile�� flush ���̸� 
               ���� Ext�� Flush ���ϼ� ������
               �� �������� ã�� ���� ���� �Ͽ� �ش� Ext�� movedown �Ҽ��ֵ���
               mMovedownPos ���� ��Ű�� �ʰ� ��� */
        case SDS_EXTENT_STATE_FLUSH_ING:
            /* nothing to do */
            break;

        default:
            ideLog::log( IDE_SERVER_0, 
                         "Unexpected BCBExtent [%d] : %d", 
                         mMovedownPos, sState );
            IDE_DASSERT( 0 );
            break;
    }

    IDE_ASSERT( mExtentMutex.unlock() == IDE_SUCCESS );
    
    if( sIsSuccess != ID_TRUE )
    {
        IDE_TEST( sdsFlushMgr::flushPagesInExtent( aStatistics ) 
                  != IDE_SUCCESS );

        sdsBufferMgr::applyVictimWaits(); 
        sTV.set( 0, 100 );
        idlOS::sleep(sTV);

        goto retry;
    }
    else 
    {
        /* ���������� �ε����� �޾Ƽ� flusher�� ���۽�Ų�� */
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : flush ���� Extent�� sdbFlusher�� movedown�Ҽ��ֵ��� ���º��� 
 *  aExtIndex [IN]  
 ******************************************************************************/
IDE_RC sdsBufferArea::changeStateFlushDone( UInt aExtIndex )
{
    IDE_ASSERT( mExtentMutex.lock( NULL /*aStatistics*/ ) == IDE_SUCCESS );

    mBCBExtState[aExtIndex] = SDS_EXTENT_STATE_FLUSH_DONE;

    IDE_ASSERT( mExtentMutex.unlock() == IDE_SUCCESS );
    // �� EXTENT�� ��ٸ��� �ִ� �����尡 ������ �����.
    if( mWaitingClientCount > 0 )
    {
        IDE_ASSERT( mMutexForWait.lock( NULL /*aStatistics*/ ) == IDE_SUCCESS );

        IDE_TEST_RAISE( mCondVar.broadcast() != IDE_SUCCESS,
                        ERR_COND_SIGNAL );

        IDE_ASSERT( mMutexForWait.unlock() == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COND_SIGNAL );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondSignal) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : movedown�� Extent�� sdsFlusher�� flush�Ҽ��ֵ��� ���¸� ����
 *  aExtIndex [IN]
 ******************************************************************************/
IDE_RC sdsBufferArea::changeStateMovedownDone( UInt aExtIndex )
{
    IDE_ASSERT( mExtentMutex.lock( NULL /*aStatistics*/ ) == IDE_SUCCESS );

    mBCBExtState[aExtIndex] = SDS_EXTENT_STATE_MOVEDOWN_DONE;

    IDE_ASSERT( mExtentMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/******************************************************************************
 * Description :  
 * aFunc     [IN]  �� BCB�� ������ �Լ�
 * aObj      [IN]  aFunc�����Ҷ� �ʿ��� ����
 ******************************************************************************/
IDE_RC sdsBufferArea::applyFuncToEachBCBs( sdsBufferAreaActFunc   aFunc,
                                           void                 * aObj )
{
    sdsBCB  * sSBCB;
    UInt      i;

    for( i = 0 ; i < mBCBCount ; i++ )
    {
        sSBCB = getBCB( i );
        IDE_TEST( aFunc( sSBCB, aObj) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
