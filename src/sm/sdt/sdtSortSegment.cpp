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
 * $Id $
 **********************************************************************/

#include <sdpDef.h>
#include <smiMisc.h>
#include <smuProperty.h>
#include <sdtWAExtentMgr.h>
#include <sdtWASortMap.h>
#include <sdtSortSegment.h>

iduMutex      sdtSortSegment::mMutex;
smuMemStack   sdtSortSegment::mWASegmentPool;
smuMemStack   sdtSortSegment::mNHashMapPool;

IDE_RC sdtSortSegment::initializeStatic()
{
    UInt    sState = 0;

    IDE_TEST( mMutex.initialize( (SChar*)"SDT_SORT_SEGMENT_MUTEX",
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS);
    sState = 1;

    IDE_TEST( mWASegmentPool.initialize( IDU_MEM_SM_TEMP,
                                         getWASegmentSize(),
                                         smuProperty::getTmpInitWASegCnt())
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mNHashMapPool.initialize( IDU_MEM_SM_TEMP,
                                        getWAPageCount() * ID_SIZEOF(sdtWCB*) /
                                        smuProperty::getTempHashBucketDensity(),
                                        smuProperty::getTmpInitWASegCnt() )
              != IDE_SUCCESS );
    sState = 3;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            (void)mNHashMapPool.destroy();
        case 2:
            (void)mWASegmentPool.destroy();
        case 1:
            mMutex.destroy();
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * ���� ������ ȣ��ȴ�. Mutex�� ���� Pool�� free �Ѵ�.
 ***************************************************************************/
void sdtSortSegment::destroyStatic()
{
    (void)mWASegmentPool.destroy();

    (void)mNHashMapPool.destroy();

    mMutex.destroy();
}

/**************************************************************************
 * Description : SORT_AREA_SIZE ���濡 ���� Sort Segment �� ũ�⸦ �����Ѵ�.
 *               �̹� ������� Sort Segment�� ���� ũ�⸦ �����ϰ�,
 *               ���� �����Ǵ� Sort Segment���� �� ũ��� �����ȴ�.
 *               lock�� ���� ü�� ȣ�� �Ǿ�� �Ѵ�.
 *
 * aNewWorkAreaSize  - [IN] ���� �� ũ��
 ***************************************************************************/
IDE_RC sdtSortSegment::resetWASegmentSize( ULong aNewWorkAreaSize )
{
    UInt   sNewWAPageCount   = calcWAPageCount( aNewWorkAreaSize ) ;
    UInt   sNewWASegmentSize = calcWASegmentSize( sNewWAPageCount );
    UInt   sHashBucketCnt    = sNewWAPageCount  / smuProperty::getTempHashBucketDensity();
    UInt   sState = 0;

    IDE_TEST( mWASegmentPool.resetDataSize( sNewWASegmentSize ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( mNHashMapPool.resetDataSize( sHashBucketCnt * ID_SIZEOF(sdtWCB*) ) != IDE_SUCCESS );
    sState = 2;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)mNHashMapPool.resetDataSize( getWAPageCount() / smuProperty::getTempHashBucketDensity()
                                               * ID_SIZEOF(sdtWCB*) );
        case 1:
            (void)mWASegmentPool.resetDataSize( getWASegmentSize() );
        default :
            break;
    }
    return IDE_FAILURE;
}

/**************************************************************************
 * Description : __TEMP_INIT_WASEGMENT_COUNT ���濡 ����
 *               Sort Segment �� ũ�⸦ �����Ѵ�. �� ȣ����� �ʴ´�.
 *
 * aNewWASegmentCount  - [IN] ���� �� ��
 ***************************************************************************/
IDE_RC sdtSortSegment::resizeWASegmentPool( UInt   aNewWASegmentCount )
{
    idBool sIsLock = ID_FALSE;

    lock();
    sIsLock = ID_TRUE;

    IDE_TEST( mWASegmentPool.resetStackInitCount( aNewWASegmentCount ) != IDE_SUCCESS );

    sIsLock = ID_FALSE;
    unlock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_TRUE )
    {
        unlock();
    }

    return IDE_FAILURE;
}


/******************************************************************
 * Segment
 ******************************************************************/
/**************************************************************************
 * Description :
 * Segment�� Sizeũ���� WExtent�� ����ü �����Ѵ�
 *
 * <IN>
 * aStatistics - �������
 * aStatsPtr   - TempTable�� �������
 * aLogging    - Logging���� (����� ��ȿ��)
 * aSpaceID    - Extent�� ������ TablespaceID
 * aSize       - ������ WA�� ũ��
 * <OUT>
 * aWASegment  - ������ WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::createSortSegment( smiTempTableHeader * aHeader,
                                          ULong                aInitWorkAreaSize )
{
    sdtSortSegHdr  * sWASeg = NULL;
    sdtWCB        ** sNPageHashPtr = NULL;
    ULong            sMaxWAExtentCount;
    ULong            sInitWAExtentCount;
    UInt             sNPageHashBucketCnt;
    ULong            sMaxWAPageCount;
    sdtSortGroup   * sInitGrpInfo;
    UInt             sState = 0;
    UInt             i;
    idBool           sIsLock = ID_FALSE;

    lock();
    sIsLock = ID_TRUE;
    sMaxWAExtentCount = getWAExtentCount();
    sMaxWAPageCount   = sMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT;

    IDE_TEST( mWASegmentPool.getDataSize() != calcWASegmentSize( sMaxWAPageCount ) );

    IDE_TEST( mWASegmentPool.pop( (UChar**)&sWASeg ) != IDE_SUCCESS );
    sState = 1;
    IDE_DASSERT( sWASeg != NULL );

    IDE_TEST( mNHashMapPool.pop( (UChar**)&sNPageHashPtr ) != IDE_SUCCESS );
    sState = 2;
    IDE_DASSERT( sNPageHashPtr != NULL );
    sNPageHashBucketCnt = sMaxWAPageCount / smuProperty::getTempHashBucketDensity();

    sIsLock = ID_FALSE;
    unlock();

    idlOS::memset( (UChar*)sWASeg, 0, SDT_SORT_SEGMENT_HEADER_SIZE );

    sWASeg->mNPageHashBucketCnt = sNPageHashBucketCnt;
    sWASeg->mNPageHashPtr       = sNPageHashPtr;
    sWASeg->mMaxWAExtentCount   = sMaxWAExtentCount;

#ifdef DEBUG
    for( i = 0 ; i < sWASeg->mNPageHashBucketCnt ; i++ )
    {
        IDE_ASSERT( sNPageHashPtr[i] == NULL );
    }
#endif

    if ( aInitWorkAreaSize == 0 )
    {
        sInitWAExtentCount = smuProperty::getTmpMinInitWAExtCnt();
    }
    else
    {
        sInitWAExtentCount = sdtWAExtentMgr::calcWAExtentCount( aInitWorkAreaSize );

        // Sort Area�� Row Area�� 1����
        // �ּ��� 4�� �̻��� �Ǿ�� �Ѵ�.
        if ( sInitWAExtentCount < smuProperty::getTmpMinInitWAExtCnt() )
        {
            sInitWAExtentCount = smuProperty::getTmpMinInitWAExtCnt();
        }
        else
        {
            if ( sMaxWAExtentCount < sInitWAExtentCount )
            {
                sInitWAExtentCount = sMaxWAExtentCount;
            }
        }
    }

    /***************************** initialize ******************************/
    sWASeg->mSpaceID          = aHeader->mSpaceID;
    sWASeg->mWorkType         = SDT_WORK_TYPE_SORT;
    sWASeg->mStatsPtr         = aHeader->mStatsPtr;
    sWASeg->mNPageCount       = 0;
    sWASeg->mIsInMemory       = SDT_WORKAREA_IN_MEMORY;
    sWASeg->mStatistics       = aHeader->mStatistics;
    sWASeg->mAllocWAPageCount = 0;
    sWASeg->mUsedWCBPtr       = SDT_USED_PAGE_PTR_TERMINATED;
    sWASeg->mNExtFstPIDList.mPageSeqInLFE = SDT_WAEXTENT_PAGECOUNT;

    sWASeg->mStatsPtr->mRuntimeMemSize += mWASegmentPool.getNodeSize((UChar*) sWASeg );
    sWASeg->mStatsPtr->mRuntimeMemSize += mNHashMapPool.getNodeSize((UChar*) sNPageHashPtr );

    IDE_TEST( sdtWAExtentMgr::initWAExtents( sWASeg->mStatistics,
                                             sWASeg->mStatsPtr,
                                             &sWASeg->mWAExtentInfo,
                                             sInitWAExtentCount )
              != IDE_SUCCESS );
    sState = 3;

    sWASeg->mCurFreeWAExtent = sWASeg->mWAExtentInfo.mHead;
    sWASeg->mCurrFreePageIdx = 0;

    sWASeg->mHintWCBPtr = getWCBWithLnk( sWASeg, 1 );
    sState = 4;

    /***************************** InitGroup *******************************/
    for( i = 0 ; i< SDT_WAGROUPID_MAX ; i++ )
    {
        sWASeg->mGroup[ i ].mPolicy = SDT_WA_REUSE_NONE;
    }

    /* InitGroup�� �����Ѵ�. */
    sInitGrpInfo = &sWASeg->mGroup[ 0 ];
    /* Segment ���� WAExtentPtr�� ��ġ�� �� Range�� �����̴�. */
    sInitGrpInfo->mBeginWPID = 1;
    sInitGrpInfo->mEndWPID = sMaxWAPageCount;

    sInitGrpInfo->mPolicy       = SDT_WA_REUSE_INMEMORY;
    sInitGrpInfo->mSortMapHdr   = NULL;
    sInitGrpInfo->mReuseWPIDSeq = SC_NULL_PID;
    sInitGrpInfo->mReuseWPIDTop = SC_NULL_PID;
    sInitGrpInfo->mReuseWPIDBot = SC_NULL_PID;

    /***************************** NPageMgr *********************************/
    IDE_TEST( sWASeg->mFreeNPageStack.initialize( IDU_MEM_SM_TEMP,
                                                  ID_SIZEOF( scPageID ) )
              != IDE_SUCCESS );
    sState = 5;

    aHeader->mWASegment = sWASeg;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_FALSE )
    {
        lock();
    }

    switch( sState )
    {
        case 5:
            sWASeg->mFreeNPageStack.destroy();
        case 4:
            clearAllWCB( sWASeg );
        case 3:
            (void)sdtWAExtentMgr::freeWAExtents( &sWASeg->mWAExtentInfo );
        case 2:
            (void)mNHashMapPool.push( (UChar*)sNPageHashPtr );
        case 1:
            (void)mWASegmentPool.push( (UChar*)sWASeg );
            break;
        default:
            break;
    }

    aHeader->mWASegment = NULL;

    unlock();

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Segment�� Drop�ϰ� ���� Extent�� ��ȯ�Ѵ�.
 *
 * <IN> aWASegment     - ��� WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::dropSortSegment( sdtSortSegHdr* aWASegment )
{
    if ( aWASegment != NULL )
    {
        IDE_TEST( dropAllWAGroup( aWASegment ) != IDE_SUCCESS );

        IDE_TEST( clearSortSegment( aWASegment ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Segment�� ��Ȱ���Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::clearSortSegment( sdtSortSegHdr* aWASegment )
{
    UInt     sState = 0;
    idBool   sIsLock = ID_FALSE;

    if ( aWASegment != NULL )
    {
        sState = 1;

        aWASegment->mNExtFstPIDList.mLastFreeExtFstPID = SM_NULL_PID;

        clearAllWCB( aWASegment );

        sState = 2;
        IDE_TEST( sdtWAExtentMgr::freeWAExtents( &aWASegment->mWAExtentInfo ) != IDE_SUCCESS );

        sState = 3;
        IDE_TEST( sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList ) != IDE_SUCCESS );

        IDE_TEST( aWASegment->mFreeNPageStack.destroy() != IDE_SUCCESS );

        lock();
        sIsLock = ID_TRUE;

        sState = 4;
        IDE_TEST( mNHashMapPool.push( (UChar*)(aWASegment->mNPageHashPtr) ) != IDE_SUCCESS );

        aWASegment->mNPageHashPtr       = NULL;
        aWASegment->mNPageHashBucketCnt = 0;

        sState = 5;
        IDE_TEST( mWASegmentPool.push( (UChar*)aWASegment ) != IDE_SUCCESS );

        sIsLock = ID_FALSE;
        unlock();
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        /* BUG-42751 ���� �ܰ迡�� ���ܰ� �߻��ϸ� �ش� �ڿ��� �����Ǳ⸦ ��ٸ��鼭
           server stop �� HANG�� �߻��Ҽ� �����Ƿ� ��������� ��. */
        case 1:
            clearAllWCB( aWASegment );
            (void)sdtWAExtentMgr::freeWAExtents( &aWASegment->mWAExtentInfo );
        case 2:
            (void)sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList );
        case 3:
            if ( sIsLock == ID_FALSE )
            {
                lock();
                sIsLock = ID_TRUE;
            }
            (void)mNHashMapPool.push( (UChar*)(aWASegment->mNPageHashPtr) );
        case 4:
            if ( sIsLock == ID_FALSE )
            {
                lock();
                sIsLock = ID_TRUE;
            }
            (void)mWASegmentPool.push( (UChar*)aWASegment );
        default:
            break;
    }
    if ( sIsLock == ID_TRUE )
    {
        unlock();
    }
    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Segment�� ��Ȱ���Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::truncateSortSegment( sdtSortSegHdr* aWASegment )
{
    sdtWCB         * sWCBPtr;
    sdtWCB         * sNxtWCBPtr;
    UInt             sCount = 0;

    IDE_ERROR( aWASegment != NULL );

    IDE_TEST( dropAllWAGroup( aWASegment ) != IDE_SUCCESS );

    /* ���� Init���·� �����. */
    sWCBPtr = aWASegment->mUsedWCBPtr;
    while( sWCBPtr != SDT_USED_PAGE_PTR_TERMINATED )
    {
        if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT )
        {
            initWCB( sWCBPtr );

            IDE_DASSERT( aWASegment->mGroup[1].mHintWCBPtr != sWCBPtr );
        }
        else
        {
            IDE_DASSERT( ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN ) ||
                         ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY ));

            /* Initpage�� ���� �ʿ䰡 ����.
             * assign �ž��ֱ� ���� */
            IDE_TEST( makeInitPage( aWASegment, 
                                    sWCBPtr, 
                                    ID_FALSE ) // aWait4Flush
                      != IDE_SUCCESS );
        }

        sCount++;
        sNxtWCBPtr = sWCBPtr->mNxtUsedWCBPtr;

        sWCBPtr->mNxtUsedWCBPtr = NULL;
        sWCBPtr->mWAPagePtr     = NULL;

        sWCBPtr = sNxtWCBPtr;
    }
    aWASegment->mUsedWCBPtr      = SDT_USED_PAGE_PTR_TERMINATED;
    aWASegment->mCurFreeWAExtent = aWASegment->mWAExtentInfo.mHead;
    aWASegment->mCurrFreePageIdx = 0;
    aWASegment->mHintWCBPtr      = getWCBWithLnk( aWASegment, 1 );

    aWASegment->mAllocWAPageCount = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************
 * Group
 ******************************************************************/
/**************************************************************************
 * Description :
 * Segment�� Group�� �����Ѵ�. �̶� InitGroup���κ��� Extent�� �����´�.
 * Size�� 0�̸�, InitGroup��üũ��� �����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ������ Group ID
 * aPageCount     - ��� Group�� ���� Page ����
 * aPolicy        - ��� Group�� ����� FreePage ��Ȱ�� ��å
 ***************************************************************************/
IDE_RC sdtSortSegment::createWAGroup( sdtSortSegHdr    * aWASegment,
                                      sdtGroupID         aWAGroupID,
                                      UInt               aPageCount,
                                      sdtWAReusePolicy   aPolicy )
{
    sdtSortGroup   *sInitGrpInfo = getWAGroupInfo( aWASegment, SDT_WAGROUPID_INIT );
    sdtSortGroup   *sGrpInfo     = getWAGroupInfo( aWASegment, aWAGroupID );

    IDE_ERROR( sGrpInfo->mPolicy == SDT_WA_REUSE_NONE );

    /* ũ�⸦ �������� �ʾ�����, ���� �뷮�� ���� �ο��� */
    if ( aPageCount == 0 )
    {
        aPageCount = getAllocableWAGroupPageCount( sInitGrpInfo );
    }

    IDE_ERROR( aPageCount >= SDT_WAGROUP_MIN_PAGECOUNT );

    /* ������ ���� InitGroup���� ������
     * <---InitGroup---------------------->
     * <---InitGroup---><----CurGroup----->*/
    IDE_ERROR( sInitGrpInfo->mEndWPID >= aPageCount );  /* ����üũ */
    sGrpInfo->mBeginWPID = sInitGrpInfo->mEndWPID - aPageCount;
    sGrpInfo->mEndWPID   = sInitGrpInfo->mEndWPID;
    sInitGrpInfo->mEndWPID -= aPageCount;
    IDE_ERROR( sInitGrpInfo->mBeginWPID <= sInitGrpInfo->mEndWPID );

    sGrpInfo->mHintWCBPtr = NULL;
    sGrpInfo->mPolicy     = aPolicy;

    IDE_TEST( clearWAGroup( sGrpInfo ) != IDE_SUCCESS );
    IDE_DASSERT( validateLRUList( aWASegment, sGrpInfo ) == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : WAGroup�� ��Ȱ�� �� �� �ֵ��� �����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aWait4Flush    - Dirty�� Page���� Flush�ɶ����� ��ٸ� ���ΰ�.
 ***************************************************************************/
IDE_RC sdtSortSegment::resetWAGroup( sdtSortSegHdr     * aWASegment,
                                     sdtGroupID          aWAGroupID,
                                     idBool              aWait4Flush )
{
    sdtSortGroup   * sGrpInfo = getWAGroupInfo(aWASegment,aWAGroupID);
    sdtWCB         * sWCBPtr;
    sdtWCB         * sEndWCBPtr;

    if ( sGrpInfo->mPolicy != SDT_WA_REUSE_NONE )
    {
        if ( aWait4Flush == ID_TRUE )
        {
            /* ���⼭ HintPage�� �ʱ�ȭ�Ͽ� Unfix���� ������,
             * ���� makeInit�ܰ迡�� hintPage�� unassign�� �� ����
             * ������ �߻��Ѵ�. */
            setHintWCB( sGrpInfo, NULL );

            sWCBPtr = getWCBInternal( aWASegment,
                                      sGrpInfo->mBeginWPID );

            sEndWCBPtr = getWCBInternal( aWASegment,
                                         sGrpInfo->mEndWPID - 1 );

            /* Write�� �ʿ��� Page�� ��� Write �Ѵ�.
             * makeInitPage���� �ϳ��� �ϸ鼭 ����ϸ� ������. */
            /* Disk�� Bind�Ǵ� Page�� ���� �Ҵ� �Ǹ� �ȵȴ�. */
            while(( sWCBPtr <= sEndWCBPtr ) &&
                  ( sWCBPtr->mNxtUsedWCBPtr != NULL ))
            { 
                if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
                {
                    IDE_TEST( writeNPage( aWASegment, sWCBPtr ) != IDE_SUCCESS );
                }
                sWCBPtr++;
            }

            sWCBPtr = getWCBInternal( aWASegment,
                                      sGrpInfo->mBeginWPID );

            while(( sWCBPtr <= sEndWCBPtr ) &&
                  ( sWCBPtr->mNxtUsedWCBPtr != NULL ))
            {
                IDE_TEST( makeInitPage( aWASegment,
                                        sWCBPtr,
                                        ID_TRUE )
                          != IDE_SUCCESS );
                sWCBPtr++;
            }
        }
        IDE_TEST( clearWAGroup( sGrpInfo ) != IDE_SUCCESS );
        IDE_DASSERT( validateLRUList( aWASegment, sGrpInfo ) == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * WAGroup�� ������ �ʱ�ȭ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
IDE_RC sdtSortSegment::clearWAGroup( sdtSortGroup      * aGrpInfo )
{
    aGrpInfo->mSortMapHdr = NULL;
    setHintWCB( aGrpInfo, NULL );

    /* ��Ȱ�� ��å�� ���� �ʱⰪ ������. */
    switch( aGrpInfo->mPolicy  )
    {
        case SDT_WA_REUSE_INMEMORY:
            /* InMemoryGroup�� ���� �ʱ�ȭ. ������ ��Ȱ�� �ڷᱸ���� �����Ѵ�.
             * �ڿ��� ������ ���� ������ Ȱ����.
             * �ֳ��ϸ� InMemoryGroup�� WAMap�� ������ �� ������,
             * WAMap�� �տ��� �������� Ȯ���ϸ鼭 �ε��� �� �ֱ� ������ */
            aGrpInfo->mReuseWPIDSeq = aGrpInfo->mEndWPID - 1;
            break;
        case SDT_WA_REUSE_FIFO:
            /* FIFOGroup�� ���� �ʱ�ȭ. ������ ��Ȱ�� �ڷᱸ���� �����Ѵ�.
             * �տ��� �ڷ� ���� ������ Ȱ����. MultiBlockWrite�� �����ϱ�����. */
            aGrpInfo->mReuseWPIDSeq = aGrpInfo->mBeginWPID;
            break;
        case SDT_WA_REUSE_LRU:
            /* LRUGroup�� ���� �ʱ�ȭ. ������ ��Ȱ�� �ڷᱸ���� �����Ѵ�.
             * MultiBlockWrite�� �����ϱ����� �տ��� �ڷ� ���� ������ Ȱ���ϵ���
             * �����ص� */
            aGrpInfo->mReuseWPIDSeq = aGrpInfo->mBeginWPID;
            aGrpInfo->mReuseWPIDTop = SC_NULL_PID;
            aGrpInfo->mReuseWPIDBot = SC_NULL_PID;
            break;
        default:
            IDE_ERROR( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/**************************************************************************
 * Description :
 * �� Group�� �ϳ��� (aWAGroupID1 ������) ��ģ��.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID1    - ��� GroupID ( �������� ������ )
 * aWAGroupID2    - ��� GroupID ( �Ҹ�� )
 * aPolicy        - ������ group�� ������ ��å
 ***************************************************************************/
IDE_RC sdtSortSegment::mergeWAGroup(sdtSortSegHdr    * aWASegment,
                                    sdtGroupID         aWAGroupID1,
                                    sdtGroupID         aWAGroupID2,
                                    sdtWAReusePolicy   aPolicy )
{
    sdtSortGroup   * sGrpInfo1 = getWAGroupInfo( aWASegment, aWAGroupID1 );
    sdtSortGroup   * sGrpInfo2 = getWAGroupInfo( aWASegment, aWAGroupID2 );

    IDE_ERROR( sGrpInfo1->mPolicy != SDT_WA_REUSE_NONE );
    IDE_ERROR( sGrpInfo2->mPolicy != SDT_WA_REUSE_NONE );

    /* ������ ���� ���¿��� ��.
     * <----Group2-----><---Group1--->*/
    IDE_ERROR( sGrpInfo2->mEndWPID == sGrpInfo1->mBeginWPID );

    /* <----Group2----->
     *                  <---Group1--->
     *
     * ���� ������ ���� ������.
     *
     * <----Group2----->
     * <--------------------Group1---> */
    sGrpInfo1->mBeginWPID = sGrpInfo2->mBeginWPID;

    /* Policy �缳�� */
    sGrpInfo1->mPolicy = aPolicy;
    /* �ι�° Group�� �ʱ�ȭ��Ŵ */
    sGrpInfo2->mPolicy = SDT_WA_REUSE_NONE;

    IDE_TEST( clearWAGroup( sGrpInfo1 ) != IDE_SUCCESS );
    IDE_DASSERT( validateLRUList( aWASegment, sGrpInfo1 ) == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �� Group�� �� Group���� ������. ( ũ��� ���� )
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aSrcWAGroupID  - �ɰ� ���� GroupID
 * aDstWAGroupID  - �ɰ����� ������� Group
 * aPolicy        - �� group�� ������ ��å
 ***************************************************************************/
IDE_RC sdtSortSegment::splitWAGroup(sdtSortSegHdr    * aWASegment,
                                    sdtGroupID         aSrcWAGroupID,
                                    sdtGroupID         aDstWAGroupID,
                                    sdtWAReusePolicy   aPolicy )
{
    sdtSortGroup   * sSrcGrpInfo= getWAGroupInfo( aWASegment, aSrcWAGroupID );
    sdtSortGroup   * sDstGrpInfo= getWAGroupInfo( aWASegment, aDstWAGroupID );
    UInt             sPageCount;

    IDE_ERROR( sDstGrpInfo->mPolicy == SDT_WA_REUSE_NONE );

    sPageCount = getAllocableWAGroupPageCount( sSrcGrpInfo )/2;

    IDE_ERROR( sPageCount >= SDT_WAGROUP_MIN_PAGECOUNT );

    /* ������ ���� Split��
     * <------------SrcGroup------------>
     * <---DstGroup----><----SrcGroup--->*/
    IDE_ERROR( sSrcGrpInfo->mEndWPID >= sPageCount );
    sDstGrpInfo->mBeginWPID = sSrcGrpInfo->mBeginWPID;
    sDstGrpInfo->mEndWPID   = sSrcGrpInfo->mBeginWPID + sPageCount;
    sSrcGrpInfo->mBeginWPID+= sPageCount;

    IDE_ERROR( sSrcGrpInfo->mBeginWPID < sSrcGrpInfo->mEndWPID );
    IDE_ERROR( sDstGrpInfo->mBeginWPID < sDstGrpInfo->mEndWPID );

    /* �ι�° Group�� ���� ������ */
    sDstGrpInfo->mHintWCBPtr  = NULL;
    sDstGrpInfo->mPolicy      = aPolicy;

    IDE_TEST( clearWAGroup( sSrcGrpInfo ) != IDE_SUCCESS );
    IDE_TEST( clearWAGroup( sDstGrpInfo ) != IDE_SUCCESS );
    IDE_DASSERT( validateLRUList( aWASegment, sSrcGrpInfo ) == IDE_SUCCESS );
    IDE_DASSERT( validateLRUList( aWASegment, sDstGrpInfo ) == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : ��� Group�� Drop�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::dropAllWAGroup( sdtSortSegHdr* aWASegment )
{
    SInt      sWAGroupID;

    for( sWAGroupID = SDT_WAGROUPID_MAX - 1 ; sWAGroupID >  SDT_WAGROUPID_INIT ; sWAGroupID-- )
    {
        if ( aWASegment->mGroup[ sWAGroupID ].mPolicy != SDT_WA_REUSE_NONE )
        {
            IDE_TEST( dropWAGroup( aWASegment,
                                   sWAGroupID,
                                   ID_FALSE ) != IDE_SUCCESS );

            aWASegment->mGroup[ sWAGroupID ].mPolicy = SDT_WA_REUSE_NONE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Group�� Drop�ϰ� InitGroup�� Extent�� ��ȯ�Ѵ�. ������
 * dropWASegment�ҰŶ�� ���� �� �ʿ� ����.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aWait4Flush    - Dirty�� Page���� Flush�ɶ����� ��ٸ� ���ΰ�.
 ***************************************************************************/
IDE_RC sdtSortSegment::dropWAGroup(sdtSortSegHdr* aWASegment,
                                   sdtGroupID     aWAGroupID,
                                   idBool         aWait4Flush)
{
    sdtSortGroup   * sInitGrpInfo =
        getWAGroupInfo( aWASegment, SDT_WAGROUPID_INIT );
    sdtSortGroup   * sGrpInfo = getWAGroupInfo( aWASegment, aWAGroupID );

    IDE_TEST( resetWAGroup( aWASegment, aWAGroupID, aWait4Flush )
              != IDE_SUCCESS );

    /* ���� �ֱٿ� �Ҵ��� Group�̾����.
     * �� ������ ���� ���¿��� ��.
     * <---InitGroup---><----CurGroup----->*/
    IDE_ERROR( sInitGrpInfo->mEndWPID == sGrpInfo->mBeginWPID );

    /* �Ʒ� ������ ���� ������ ���� ����. ������ ������ Group��
     * ���õ� ���̱⿡ ��� ����.
     *                  <----CurGroup----->
     * <----------------------InitGroup---> */
    sInitGrpInfo->mEndWPID = sGrpInfo->mEndWPID;
    sGrpInfo->mPolicy      = SDT_WA_REUSE_NONE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Group�� ��å�� ���� ������ �� �ִ� WAPage�� ��ȯ�Ѵ�.
 *               �����ϸ� NULL Pointer�� Return�ȴ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGrpInfo       - ��� Group
 * <OUT>
 * aWCBPtr        - Ž���� FreeWAPage�� WCB
 ***************************************************************************/
IDE_RC sdtSortSegment::getFreeWAPageINMEMORY( sdtSortSegHdr* aWASegment,
                                              sdtSortGroup * aGrpInfo,
                                              sdtWCB      ** aWCBPtr )
{
    sdtWCB * sWCBPtr;
    scPageID sMapEndPID;

    /* sort area , hash group */
    IDE_ERROR( aGrpInfo->mSortMapHdr != NULL );
    IDE_DASSERT( aGrpInfo->mPolicy == SDT_WA_REUSE_INMEMORY );

    sMapEndPID = sdtWASortMap::getEndWPID( aGrpInfo->mSortMapHdr );

    /* Map�� �ִ� ������ ������, �� ��ٴ� �̾߱� */
    if ( sMapEndPID >= aGrpInfo->mReuseWPIDSeq )
    {
        /* InMemoryGroup�� victimReplace�� �ȵȴ�.
         * �ʱ�ȭ �ϰ�, NULL�� ��ȯ�Ѵ�.*/
        aGrpInfo->mReuseWPIDSeq = aGrpInfo->mEndWPID - 1;

        *aWCBPtr = NULL;
    }
    else
    {
        IDE_ASSERT( aGrpInfo->mReuseWPIDSeq != SD_NULL_PID );

        sWCBPtr = getWCBWithLnk( aWASegment,
                                 aGrpInfo->mReuseWPIDSeq );

        if ( sWCBPtr->mWAPagePtr == NULL )
        {
            IDE_TEST( setWAPagePtr(aWASegment, sWCBPtr) != IDE_SUCCESS );
            IDE_ASSERT( sWCBPtr->mWAPagePtr != NULL );
        }

        aGrpInfo->mReuseWPIDSeq--;

        /* �Ҵ���� �������� �� �������� ���� */
        IDE_TEST( makeInitPage( aWASegment,
                                sWCBPtr,
                                ID_TRUE ) /*wait 4 flush */
                  != IDE_SUCCESS );

        (*aWCBPtr) = sWCBPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Group�� ��å�� ���� ������ �� �ִ� WAPage�� ��ȯ�Ѵ�.
 *               ���� �� �� ���� ��õ� �ȴ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGrpInfo       - ��� Group
 * <OUT>
 * aWCBPtr        - Ž���� FreeWAPage�� WCB
 ***************************************************************************/
IDE_RC sdtSortSegment::getFreeWAPageFIFO( sdtSortSegHdr* aWASegment,
                                          sdtSortGroup * aGrpInfo,
                                          sdtWCB      ** aWCBPtr )
{
    scPageID     sRetPID;
    sdtWCB     * sWCBPtr;

    IDE_DASSERT( aGrpInfo->mPolicy ==  SDT_WA_REUSE_FIFO );
    IDE_ERROR( aGrpInfo->mSortMapHdr == NULL );

    while( 1 )
    {
        /* hash sub group */
        sRetPID = aGrpInfo->mReuseWPIDSeq;
        aGrpInfo->mReuseWPIDSeq++;

        /* Group�� �ѹ� ���� ��ȸ�� */
        if ( aGrpInfo->mReuseWPIDSeq == aGrpInfo->mEndWPID )
        {
            aGrpInfo->mReuseWPIDSeq = aGrpInfo->mBeginWPID;
        }

        sWCBPtr = getWCBWithLnk( aWASegment,
                                 sRetPID );

        if ( sWCBPtr->mWAPagePtr == NULL )
        {
            IDE_TEST( setWAPagePtr( aWASegment,
                                    sWCBPtr ) != IDE_SUCCESS );

            IDE_ASSERT( isFixedPage( sWCBPtr ) == ID_FALSE );
            break;
        }

        if ( isFixedPage( sWCBPtr ) == ID_FALSE )
        {
            /* dirty page�� write�Ѵ�.
             * clean�� page�� �ϳ��� ���� ��쿡�� �ѹ��� ���Ƽ� ���ڸ��� ����
             * write�� page�� �Ҵ� ���� �� �ְԵȴ�. */
            if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
            {
                IDE_TEST( writeNPage( aWASegment, sWCBPtr ) != IDE_SUCCESS );
            }
            break;
        }
        else
        {
            /* Fix�� page�� new page�� �Ҵ� ���� �� ����.
             * ���� ���ɼ��� �����Ƿ� writeNPage�� ���� �ʴ´�. */   
        }
    }

    IDE_DASSERT( sRetPID != SD_NULL_PID );
    /* �Ҵ���� �������� �� �������� ���� */

    /* Assign�Ǿ������� Assign �� �������� �ʱ�ȭ�����ش�. */
    if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN )
    {
        IDE_TEST( unassignNPage( aWASegment,
                                 sWCBPtr )
                  != IDE_SUCCESS );
    }

    initWCB( sWCBPtr );

    (*aWCBPtr) = sWCBPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Group�� ��å�� ���� ������ �� �ִ� WAPage�� ��ȯ�Ѵ�.
 *               ���� �� �� ���� ��õ� �ȴ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aGrpInfo       - ��� Group
 * <OUT>
 * aWCBPtr        - Ž���� FreeWAPage�� WCB
 ***************************************************************************/
IDE_RC sdtSortSegment::getFreeWAPageLRU( sdtSortSegHdr* aWASegment,
                                         sdtSortGroup * aGrpInfo,
                                         sdtWCB      ** aWCBPtr )
{
    scPageID     sRetPID;
    sdtWCB     * sWCBPtr = NULL;
    sdtWCB     * sOldTopWCBPtr;

    IDE_DASSERT( aGrpInfo->mPolicy ==  SDT_WA_REUSE_LRU );
    IDE_DASSERT( aGrpInfo->mSortMapHdr == NULL );

    while(1)
    {
        /* clearGroup �� ó������ ���������� �Ҵ��Ѵ�,
         * ���� EndWPageID���� �����ϸ� LRU List�� ���� �Ҵ��Ѵ�.*/
        /* clearGroup�� �Ͽ�����, ���� Dirty Page�� �����ϴ� ���
         * Free Page Ž�� ���߿� ���� �Ҵ翡�� LRU �Ҵ����� �Ѿ� �� ���� �ִ�.
         * �� Reuse Ȯ���� while()�ȿ��� �ؾ� �Ѵ�.*/
        if ( aGrpInfo->mReuseWPIDSeq < aGrpInfo->mEndWPID )
        {
            /* ���� �ѹ��� ��Ȱ�� �ȵ� �������� ������ */
            sRetPID = aGrpInfo->mReuseWPIDSeq;
            sWCBPtr = getWCBWithLnk( aWASegment, sRetPID );

            aGrpInfo->mReuseWPIDSeq++;

            if ( aGrpInfo->mReuseWPIDTop == SC_NULL_PID )
            {
                /* ���� ����� ��� */
                IDE_ERROR( aGrpInfo->mReuseWPIDBot == SC_NULL_PID );

                sWCBPtr->mLRUPrevPID    = SC_NULL_PID;
                sWCBPtr->mLRUNextPID    = SC_NULL_PID;
                aGrpInfo->mReuseWPIDTop = sRetPID;
                aGrpInfo->mReuseWPIDBot = sRetPID;
            }
            else
            {
                /* �̹� List�� �����Ǿ��� ���, ���ο� �Ŵ� */
                sOldTopWCBPtr = getWCBInternal( aWASegment,
                                                aGrpInfo->mReuseWPIDTop );

                sWCBPtr->mLRUPrevPID       = SD_NULL_PID;
                sWCBPtr->mLRUNextPID       = aGrpInfo->mReuseWPIDTop;
                sOldTopWCBPtr->mLRUPrevPID = sRetPID;
                aGrpInfo->mReuseWPIDTop    = sRetPID;
            }

            IDE_DASSERT( sRetPID != SD_NULL_PID );

            if ( sWCBPtr->mWAPagePtr == NULL )
            {
                IDE_TEST( setWAPagePtr( aWASegment,
                                        sWCBPtr ) != IDE_SUCCESS );
                IDE_DASSERT( isFixedPage( sWCBPtr ) == ID_FALSE );

                (*aWCBPtr) = sWCBPtr;
                break;
            }

            if ( isFixedPage( sWCBPtr ) == ID_FALSE )
            {
                // Dirty�̸� Write�ϰ� ������
                if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
                {
                    IDE_TEST( writeNPage( aWASegment, sWCBPtr ) != IDE_SUCCESS );
                }
                (*aWCBPtr) = sWCBPtr;
                break;
            }
            else
            {
                /* Fix�� page�� new page�� �Ҵ� ���� �� ����.
                 * ���� �� ���ɼ��� �����Ƿ� writeNPage�� ���� �ʴ´�. */   
            }
        }
        else
        {
            IDE_TEST( getFreeWAPageLRUfromDisk( aWASegment,
                                                aGrpInfo,
                                                aWCBPtr )
                      != IDE_SUCCESS );
            sWCBPtr = (*aWCBPtr);
            break;
        }
    }

    /* �Ҵ���� �������� �� �������� ���� */

    IDE_TEST( makeInitPage( aWASegment,
                            sWCBPtr,
                            ID_TRUE ) /*wait 4 flush */
              != IDE_SUCCESS );

    aWASegment->mHintWCBPtr = sWCBPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdtSortSegment::getFreeWAPageLRUfromDisk( sdtSortSegHdr* aWASegment,
                                                 sdtSortGroup * aGrpInfo,
                                                 sdtWCB      ** aWCBPtr )
{
    sdtWCB     * sWCBPtr;
    sdtWCB     * sBeginWCBPtr;

    // List�� ���� ��Ȱ���� 
    sBeginWCBPtr = getWCBInternal( aWASegment,
                                   aGrpInfo->mReuseWPIDBot );
    sWCBPtr = sBeginWCBPtr;

    while( isFixedPage( sWCBPtr ) == ID_TRUE )
    {
        if ( sWCBPtr->mLRUPrevPID == SD_NULL_PID )
        {
            // ã�� ���ߴ�, ó������ �ٽ� �Ѵ�.
            sWCBPtr = sBeginWCBPtr;

            continue;
        }

        sWCBPtr = getWCBInternal( aWASegment,
                                  sWCBPtr->mLRUPrevPID );
    }

    // Dirty�̸� Clean�ϰ� ������
    if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
    {
        IDE_TEST( writeNPage( aWASegment, sWCBPtr ) != IDE_SUCCESS );
    }

    if ( aGrpInfo->mReuseWPIDTop != getWPageID( aWASegment,
                                                sWCBPtr ) )
    {
        moveLRUListToTopInternal( aWASegment,
                                  aGrpInfo,
                                  sWCBPtr );
    }

    (*aWCBPtr) = sWCBPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * ��� Page�� �ʱ�ȭ�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� WPID
 * aWait4Flush    - Dirty�� Page���� Flush�ɶ����� ��ٸ� ���ΰ�.
 ***************************************************************************/
IDE_RC sdtSortSegment::makeInitPage( sdtSortSegHdr* aWASegment,
                                     sdtWCB       * aWCBPtr,
                                     idBool         aWait4Flush )
{
    /* Page�� �Ҵ�޾�����, �ش� Page�� ���� Write���� �ʾ��� �� �ִ�.
     * �׷��� ������ Write�� �õ��Ѵ�. */
    if ( aWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
    {
        if ( aWait4Flush == ID_TRUE )
        {
            IDE_TEST( writeNPage( aWASegment,
                                  aWCBPtr ) != IDE_SUCCESS );
        }
        else
        {
            aWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN ;
        }
    }

    /* Assign�Ǿ������� Assign �� �������� �ʱ�ȭ�����ش�. */
    if ( aWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN )
    {
        aWASegment->mIsInMemory = SDT_WORKAREA_OUT_MEMORY;
        IDE_TEST( unassignNPage( aWASegment,
                                 aWCBPtr )
                  != IDE_SUCCESS );
        initWCB( aWCBPtr );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �ش� WAPage�� FlushQueue�� ����ϰų� ���� Write�Ѵ�.
 * �ݵ�� bindPage,readPage������ SpaceID,PageID�� ������ WAPage���� �Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - �о�帱 WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::writeNPage( sdtSortSegHdr* aWASegment,
                                   sdtWCB       * aWCBPtr )
{
    sdtWCB    * sPrvWCBPtr ;
    sdtWCB    * sCurWCBPtr;
    sdtWCB    * sEndWCBPtr;
    UInt        sPageCount = 1 ;
    
    /* dirty�� �� �� flush queue�� ����.
     * ������ �ƴ϶� Write�ϸ� �ȴ�.*/
    IDE_DASSERT( aWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY );

    sEndWCBPtr = getWCBInternal( aWASegment,
                                 getMaxWAPageCount( aWASegment ) - 1 );

    sPrvWCBPtr = aWCBPtr;
    sCurWCBPtr = aWCBPtr + 1;

    while(( sCurWCBPtr <= sEndWCBPtr ) &&
          ( sCurWCBPtr->mWPState   == SDT_WA_PAGESTATE_DIRTY ) &&
          ( sCurWCBPtr->mNPageID   == ( sPrvWCBPtr->mNPageID + 1 ) ) &&
          ( sCurWCBPtr->mWAPagePtr == ( sPrvWCBPtr->mWAPagePtr + SD_PAGE_SIZE )))
    {
        sCurWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;
        sCurWCBPtr++;
        sPageCount++;
        sPrvWCBPtr++;
    }

    IDE_TEST( sddDiskMgr::writeMultiPage( aWASegment->mStatistics,
                                          aWASegment->mSpaceID,
                                          aWCBPtr->mNPageID,
                                          sPageCount,
                                          aWCBPtr->mWAPagePtr )
              != IDE_SUCCESS );

    aWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;

    aWASegment->mStatsPtr->mWriteCount++;
    aWASegment->mStatsPtr->mWritePageCount += sPageCount;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    /* sPageCount ��ŭ Dirty -> Clean���� ���� �Ǿ���.
     * �ٽ� Dirty�� �ǵ����ش�.*/
    sEndWCBPtr = aWCBPtr + sPageCount;

    for( sCurWCBPtr = aWCBPtr ;
         sCurWCBPtr < sEndWCBPtr ;
         sCurWCBPtr++ )
    {
        sCurWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
    }

    ideLog::log( IDE_DUMP_0,
                 "Sort Temp WriteNPage Error\n"
                 "write page count : %"ID_UINT32_FMT"\n",
                 sPageCount );

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    sdtSortSegment::dumpWCB,
                                    aWCBPtr );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * WCB�� �ش� SpaceID �� PageID�� �����Ѵ�. �� Page�� �д� ���� �ƴ϶�
 * ������ �� ������, Disk�� �ִ� �Ϲ�Page�� ������ '����'�ȴ�.

 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� WPID
 * aNPID          - ��� NPage�� �ּ�
 ***************************************************************************/
IDE_RC sdtSortSegment::assignNPage(sdtSortSegHdr* aWASegment,
                                   sdtWCB       * aWCBPtr,
                                   scPageID       aNPageID)
{
    UInt             sHashValue;

    /* ����. �̹� Hash�� �Ŵ޸� ���� �ƴϾ�� �� */
    IDE_DASSERT( findWCB( aWASegment, aNPageID )
                 == NULL );
    /* ����ε� WPID���� �� */
    IDE_DASSERT( getWPageID( aWASegment, aWCBPtr ) < getMaxWAPageCount( aWASegment ) );

    /* NPID �� ������ */
    IDE_ERROR( aWCBPtr->mNPageID  == SC_NULL_PID );

    aWCBPtr->mNPageID    = aNPageID;
    aWCBPtr->mBookedFree = ID_FALSE;
    aWCBPtr->mWPState    = SDT_WA_PAGESTATE_CLEAN;

    /* Hash�� �����ϱ� */
    sHashValue = getNPageHashValue( aWASegment, aNPageID );
    aWCBPtr->mNextWCB4Hash = aWASegment->mNPageHashPtr[sHashValue];
    aWASegment->mNPageHashPtr[sHashValue] = aWCBPtr;

    /* Hash�� �Ŵ޷���, Ž���Ǿ�� �� */
    IDE_DASSERT( findWCB( aWASegment, aNPageID )
                 == aWCBPtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * NPage�� ����. �� WPage�� �÷��� �ִ� ���� ������.
 * �ϴ� ���� ���� �ʱ�ȭ�ϰ� Hash���� �����ϴ� �� �ۿ� ���Ѵ�.

 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - ��� WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::unassignNPage( sdtSortSegHdr* aWASegment,
                                      sdtWCB       * aWCBPtr )
{
    UInt             sHashValue = 0;
    scPageID         sTargetNPID;
    sdtWCB        ** sWCBPtrPtr;
    sdtWCB         * sWCBPtr = aWCBPtr;

    /* Fix�Ǿ����� �ʾƾ� �� */
    IDE_ERROR( sWCBPtr->mFix == 0 );
    IDE_ERROR( sWCBPtr->mNPageID != SC_NULL_PID );

    /* CleanPage���� �� */
    IDE_ERROR( sWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN );

    /************************** Hash���� ����� *********************/
    sTargetNPID  = sWCBPtr->mNPageID;
    /* �Ŵ޷� �־�� �ϸ�, ���� Ž���� �� �־�� �� */
    IDE_DASSERT( findWCB( aWASegment, sTargetNPID ) == sWCBPtr );
    sHashValue   = getNPageHashValue( aWASegment, sTargetNPID );

    sWCBPtrPtr   = &aWASegment->mNPageHashPtr[ sHashValue ];

    sWCBPtr      = *sWCBPtrPtr;
    while( sWCBPtr != NULL )
    {
        if ( sWCBPtr->mNPageID == sTargetNPID )
        {
            /* ã�Ҵ�, �ڽ��� �ڸ��� ���� ���� �Ŵ� */
            (*sWCBPtrPtr) = sWCBPtr->mNextWCB4Hash;
            break;
        }

        sWCBPtrPtr = &sWCBPtr->mNextWCB4Hash;
        sWCBPtr    = *sWCBPtrPtr;
    }

    /* ��������� �� */
    IDE_DASSERT( findWCB( aWASegment, sTargetNPID ) == NULL );

    /***** Free�� �����Ǿ�����, unassign���� �������� ������ Free���ش�. ****/
    if ( sWCBPtr->mBookedFree == ID_TRUE )
    {
        IDE_TEST( pushFreePage( aWASegment,
                                sWCBPtr->mNPageID )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_DUMP_0,
                 "HASHVALUE : %"ID_UINT32_FMT"\n"
                 "WPID      : %"ID_UINT32_FMT"\n",
                 sHashValue,
                 getWPageID( aWASegment, aWCBPtr ) );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * WPage�� �ٸ� ������ �ű��. npage, npagehash���� ����ؾ� �Ѵ�.

 * <IN>
 * aWASegment     - ��� WASegment
 * aSrcWAGroupID  - ���� Group ID
 * aSrcWPID       - ���� WPID
 * aDstWAGroupID  - ��� Group ID
 * aDstWPID       - ��� WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::moveWAPage( sdtSortSegHdr* aWASegment,
                                   sdtWCB       * aSrcWCBPtr,
                                   sdtWCB       * aDstWCBPtr )
{
    UChar        * sSrcWAPagePtr;
    scPageID       sNPageID;
    sdtWAPageState sWAPageState;

    /* Dest Page�� NPage�� �Ҵ�Ǿ� ���� �ʾƾ� �Ѵ�. */
    IDE_ERROR( aDstWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT );

    sNPageID = getNPID( aSrcWCBPtr );
 
    if ( sNPageID != SC_NULL_PID )
    {
        /* ���� �������� ���� Flush���� �ƴϸ� State�� �Բ� �Ű��ش�. */
        sWAPageState = aSrcWCBPtr->mWPState;
        aSrcWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;

        IDE_TEST( unassignNPage( aWASegment,
                                 aSrcWCBPtr )
                  != IDE_SUCCESS );

        initWCB( aSrcWCBPtr );

        IDE_TEST( assignNPage( aWASegment,
                               aDstWCBPtr,
                               sNPageID )
                  != IDE_SUCCESS );

        aDstWCBPtr->mWPState = sWAPageState;
    }

    /* PagePtr�� ���� ��ȯ�Ѵ�. */
    // XXX Null üũ �ʿ�
    sSrcWAPagePtr          = aSrcWCBPtr->mWAPagePtr;
    aSrcWCBPtr->mWAPagePtr = aDstWCBPtr->mWAPagePtr;
    aDstWCBPtr->mWAPagePtr = sSrcWAPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * bindPage�ϰ� Disk���� �Ϲ�Page�κ��� ������ �о� �ø���.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aWPID          - �о�帱 WPID
 * aNPID          - ��� NPage�� �ּ�
 ***************************************************************************/
IDE_RC sdtSortSegment::readNPageFromDisk( sdtSortSegHdr* aWASegment,
                                          sdtGroupID     aWAGroupID,
                                          sdtWCB       * aWCBPtr,
                                          scPageID       aNPageID )
{
    UChar        * sWAPagePtr;

    IDE_DASSERT( aWCBPtr->mWAPagePtr != NULL );
    IDE_DASSERT( findWCB( aWASegment, aNPageID ) == NULL );

    /* ������ ���� �������� Read�ؼ� �ø� */
    IDE_TEST( assignNPage( aWASegment,
                           aWCBPtr,
                           aNPageID ) != IDE_SUCCESS );

    sWAPagePtr = getWAPagePtr( aWASegment,
                               aWAGroupID,
                               aWCBPtr );

    IDE_TEST( sddDiskMgr::read( aWASegment->mStatistics,
                                aWASegment->mSpaceID,
                                aNPageID,
                                1,
                                sWAPagePtr ) != IDE_SUCCESS );

    // TC/FIT/Server/sm/Bugs/BUG-45263/BUG-45263.tc
    IDU_FIT_POINT( "BUG-45263@sdtSortSegment::readNPageFromDisk::iduFileopen" );

    aWASegment->mStatsPtr->mReadCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * bindPage�ϰ� Disk���� �Ϲ�Page�κ��� ������ �о� �ø���.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aWPID          - �о�帱 WPID
 * aNPID          - ��� NPage�� �ּ�
 ***************************************************************************/
IDE_RC sdtSortSegment::readNPage( sdtSortSegHdr* aWASegment,
                                  sdtGroupID     aWAGroupID,
                                  sdtWCB       * aWCBPtr,
                                  scPageID       aNPageID )
{
    sdtWCB * sOldWCB;

    sOldWCB = findWCB( aWASegment, aNPageID );
    if ( sOldWCB == NULL )
    {
        IDE_TEST( readNPageFromDisk( aWASegment,
                                     aWAGroupID,
                                     aWCBPtr,
                                     aNPageID )
                  != IDE_SUCCESS );
    }
    else
    {
        /* ������ �ִ� ��������, movePage�� �ű��. */
        if ( sOldWCB != aWCBPtr )
        {
            IDE_TEST( moveWAPage( aWASegment,
                                  sOldWCB,
                                  aWCBPtr )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * �� Page�� Stack�� �����Ѵ�. ���߿� allocAndAssignNPage ���� ��Ȱ���Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWPID          - �о�帱 WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::pushFreePage(sdtSortSegHdr* aWASegment,
                                    scPageID       aNPID)
{
    IDE_TEST( aWASegment->mFreeNPageStack.push(ID_FALSE, /* lock */
                                               (void*) &aNPID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * NPage�� �Ҵ�ް� �����Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aTargetPID     - ������ WPID
 ***************************************************************************/
IDE_RC sdtSortSegment::allocAndAssignNPage(sdtSortSegHdr* aWASegment,
                                           sdtWCB       * aTargetWCBPtr )
{
    idBool         sIsEmpty;
    scPageID       sFreeNPID;

    IDE_TEST( aWASegment->mFreeNPageStack.pop( ID_FALSE, /* lock */
                                               (void*) &sFreeNPID,
                                               &sIsEmpty )
              != IDE_SUCCESS );

    if ( sIsEmpty == ID_TRUE )
    {
        if ( aWASegment->mNExtFstPIDList.mLastFreeExtFstPID == SM_NULL_PID )
        {
            /* ���� Alloc �õ� */
            IDE_TEST( sdtWAExtentMgr::allocFreeNExtent( aWASegment->mStatistics,
                                                        aWASegment->mStatsPtr,
                                                        aWASegment->mSpaceID,
                                                        &aWASegment->mNExtFstPIDList )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( aWASegment->mNExtFstPIDList.mPageSeqInLFE == SDT_WAEXTENT_PAGECOUNT )
            {
                /* ������ NExtent�� �ٽ��� */
                IDE_TEST( sdtWAExtentMgr::allocFreeNExtent( aWASegment->mStatistics,
                                                            aWASegment->mStatsPtr,
                                                            aWASegment->mSpaceID,
                                                            &aWASegment->mNExtFstPIDList )
                          != IDE_SUCCESS );
            }
            else
            {
                /* ������ �Ҵ��ص� Extent���� ������ */
            }
        }

        sFreeNPID = aWASegment->mNExtFstPIDList.mLastFreeExtFstPID
            + aWASegment->mNExtFstPIDList.mPageSeqInLFE;
        aWASegment->mNExtFstPIDList.mPageSeqInLFE++;
        aWASegment->mNPageCount++;
    }
    else
    {
        /* Stack�� ���� ��Ȱ������ */
    }

    IDE_TEST( assignNPage( aWASegment,
                           aTargetWCBPtr,
                           sFreeNPID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �� WASegment�� ����� ��� WASegment�� Free�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 ***************************************************************************/
void sdtSortSegment::clearAllWCB( sdtSortSegHdr* aWASegment )
{
    sdtWCB   * sNxtWCBPtr;
    sdtWCB   * sCurWCBPtr;
    UInt       sHashValue;
#ifdef DEBUG
    UInt       i;
#endif

    if (( smuProperty::getWCBCleanMemset() == ID_FALSE ) &&
        ( aWASegment->mIsInMemory == SDT_WORKAREA_IN_MEMORY ))
    {
        IDE_ASSERT( aWASegment->mNPageHashPtr != NULL );

#ifdef DEBUG
        sCurWCBPtr = &aWASegment->mWCBMap[0];
        for( i = 0 ;
             i< getMaxWAPageCount( aWASegment ) ;
             i++ , sCurWCBPtr++ )
        {
            IDE_ASSERT( sCurWCBPtr->mFix >= 0 );
        }
#endif
        sCurWCBPtr = aWASegment->mUsedWCBPtr;

        while( sCurWCBPtr != SDT_USED_PAGE_PTR_TERMINATED )
        {
            sNxtWCBPtr = sCurWCBPtr->mNxtUsedWCBPtr;

            if ( sCurWCBPtr->mNPageID != SD_NULL_PID )
            {
                sHashValue = getNPageHashValue( aWASegment, sCurWCBPtr->mNPageID );

                aWASegment->mNPageHashPtr[ sHashValue ] = NULL;
            }
#ifdef DEBUG
            if ( aWASegment->mHintWCBPtr == sCurWCBPtr )
            {
                aWASegment->mHintWCBPtr = NULL;
            }
#endif
            initWCB(sCurWCBPtr);

            sCurWCBPtr->mNxtUsedWCBPtr = NULL;
            sCurWCBPtr->mWAPagePtr     = NULL;

            sCurWCBPtr = sNxtWCBPtr;
        }

        aWASegment->mUsedWCBPtr = SDT_USED_PAGE_PTR_TERMINATED;
        IDE_DASSERT( aWASegment->mHintWCBPtr == NULL );
#ifdef DEBUG
        sCurWCBPtr = aWASegment->mWCBMap;
        for( i = 0 ;
             i< getMaxWAPageCount( aWASegment ) ;
             i++ , sCurWCBPtr++ )
        {
            IDE_ASSERT( sCurWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT );
            IDE_ASSERT( sCurWCBPtr->mFix     == 0 );
            IDE_ASSERT( sCurWCBPtr->mBookedFree == ID_FALSE );
            IDE_ASSERT( sCurWCBPtr->mNPageID    == SM_NULL_PID );
            IDE_ASSERT( sCurWCBPtr->mNextWCB4Hash  == NULL );
            IDE_ASSERT( sCurWCBPtr->mNxtUsedWCBPtr == NULL );
            IDE_ASSERT( sCurWCBPtr->mWAPagePtr     == NULL );
        }
#endif
    }
    else
    {
        idlOS::memset( &aWASegment->mWCBMap,
                       0,
                       getMaxWAPageCount( aWASegment ) * ID_SIZEOF(sdtWCB) );
        idlOS::memset( aWASegment->mNPageHashPtr,
                       0,
                       aWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*) );

        aWASegment->mUsedWCBPtr = SDT_USED_PAGE_PTR_TERMINATED;
    }
}

/**************************************************************************
 * Description :
 * LRU ��Ȱ�� ��å ����, ����� �ش� Page�� Top���� �̵����� Drop�� �����.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 * aPID           - ��� WPID
 ***************************************************************************/
void sdtSortSegment::moveLRUListToTopInternal( sdtSortSegHdr* aWASegment,
                                               sdtSortGroup * aGrpInfo,
                                               sdtWCB       * aWCBPtr )
{
    scPageID         sCurWPID = getWPageID( aWASegment, aWCBPtr );
    scPageID         sPrevPID;
    scPageID         sNextPID;
    sdtWCB         * sPrevWCBPtr;
    sdtWCB         * sNextWCBPtr;
    sdtWCB         * sOldTopWCBPtr;

    IDE_DASSERT( validateLRUList( aWASegment, aGrpInfo )
                 == IDE_SUCCESS );

    sOldTopWCBPtr = getWCBInternal( aWASegment, aGrpInfo->mReuseWPIDTop );

    IDE_DASSERT( sOldTopWCBPtr != NULL );

    sPrevPID = aWCBPtr->mLRUPrevPID;
    sNextPID = aWCBPtr->mLRUNextPID;

    /* PrevPage�κ��� ���� Page�� ���� ��ũ�� ��� */

    if ( sNextPID == SD_NULL_PID )
    {
        /* �ڽ��� ������ Page���� ��� */
        IDE_ASSERT( aGrpInfo->mReuseWPIDBot == sCurWPID );
        aGrpInfo->mReuseWPIDBot = sPrevPID;
    }
    else
    {
        sNextWCBPtr = getWCBInternal( aWASegment, sNextPID );
        sNextWCBPtr->mLRUPrevPID = sPrevPID;
    }
    sPrevWCBPtr = getWCBInternal( aWASegment, sPrevPID );
    sPrevWCBPtr->mLRUNextPID = sNextPID;

    /* ���ο� �Ŵ� */
    aWCBPtr->mLRUPrevPID       = SD_NULL_PID;
    aWCBPtr->mLRUNextPID       = aGrpInfo->mReuseWPIDTop;
    sOldTopWCBPtr->mLRUPrevPID = sCurWPID;
    aGrpInfo->mReuseWPIDTop    = sCurWPID;

    IDE_DASSERT( validateLRUList( aWASegment,
                                  aGrpInfo ) == IDE_SUCCESS );
    return;
}


#ifdef DEBUG
/**************************************************************************
 * Description :
 * LRU����Ʈ�� �����Ѵ�. Debugging ��� ��
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
IDE_RC sdtSortSegment::validateLRUList( sdtSortSegHdr* aWASegment,
                                        sdtSortGroup * aGrpInfo )
{
    sdtWCB         * sWCBPtr;
    UInt             sPageCount = 0;
    scPageID         sPrevWPID;
    scPageID         sNextWPID;

    if ( aGrpInfo->mPolicy == SDT_WA_REUSE_LRU )
    {
        sPrevWPID = SC_NULL_PID;
        sNextWPID = aGrpInfo->mReuseWPIDTop;
        while( sNextWPID != SC_NULL_PID )
        {
            sWCBPtr = getWCBInternal( aWASegment, sNextWPID );

            IDE_ASSERT( sWCBPtr != NULL );
            IDE_ASSERT( sWCBPtr->mLRUPrevPID == sPrevWPID );

            sPrevWPID = sNextWPID;
            sNextWPID = sWCBPtr->mLRUNextPID;

            sPageCount++;
        }
        IDE_ASSERT( sPrevWPID == aGrpInfo->mReuseWPIDBot );
        IDE_ASSERT( sNextWPID == SC_NULL_PID );
    }
    return IDE_SUCCESS;
}
#endif

/**************************************************************************
 * Description :
 * PID�� �������� �� PID�� ������ Group�� ã�´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aPID           - ��� PID
 ***************************************************************************/
sdtGroupID   sdtSortSegment::findGroup( sdtSortSegHdr* aWASegment,
                                        scPageID       aWPageID )
{
    UInt     i;

    for( i = 0 ; i < SDT_WAGROUPID_MAX ; i++ )
    {
        if ( ( aWASegment->mGroup[ i ].mPolicy != SDT_WA_REUSE_NONE ) &&
             ( aWASegment->mGroup[ i ].mBeginWPID <= aWPageID ) &&
             ( aWASegment->mGroup[ i ].mEndWPID   >  aWPageID ) )
        {
            return i;
        }
    }

    /* ã�� ���� */
    return SDT_WAGROUPID_MAX;
}

/**************************************************************************
 * Description : �ش� page�� ���� �׷��� Page ��Ȱ�� ��å�� Ȯ���Ѵ�.
 *
 * aWASegment     - [IN] ��� WASegment
 * aWAPageID      - [IN] ��� WPageID
 ***************************************************************************/
sdtWAReusePolicy sdtSortSegment::getReusePolicy( sdtSortSegHdr* aWASegment,
                                                 scPageID       aWAPageID )
{
    sdtGroupID       sWAGroupID;
    sdtSortGroup   * sWAGroupInfo;
    sdtWAReusePolicy sReusePolicy = SDT_WA_REUSE_NONE;

    sWAGroupID = sdtSortSegment::findGroup( aWASegment,
                                            aWAPageID );
    if ( sWAGroupID != SDT_WAGROUPID_MAX )
    {
        sWAGroupInfo = getWAGroupInfo( aWASegment,
                                       sWAGroupID );

        sReusePolicy = sWAGroupInfo->mPolicy;
    }
    return sReusePolicy;
}

/**************************************************************************
 * Description : Total WA���� WAExtent�� �Ҵ�޾� �´�.
 *
 * aWASegment     - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtSortSegment::expandFreeWAExtent( sdtSortSegHdr* aWASeg )
{
    SInt        sExtentCount;

    sExtentCount = smuProperty::getTmpAllocWAExtCnt();

    if ( aWASeg->mMaxWAExtentCount < ( sExtentCount + aWASeg->mWAExtentInfo.mCount ) )
    {
        sExtentCount = aWASeg->mMaxWAExtentCount - aWASeg->mWAExtentInfo.mCount;
    }
    IDE_ERROR( sExtentCount > 0 );

    if ( sdtWAExtentMgr::allocWAExtents( &aWASeg->mWAExtentInfo,
                                         sExtentCount )
         != IDE_SUCCESS )
    {
        // BUGBUG ������ �Ҵ�޵��� �Ǿ� �ִ�. ���ľ���.
        IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_WORKAREA );

        IDE_TEST( sdtWAExtentMgr::memAllocWAExtent( &aWASeg->mWAExtentInfo )
                  != IDE_SUCCESS );
        aWASeg->mStatsPtr->mOverAllocCount++;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : �Ѱ��� WAPage�� WCB�� �Ҵ��Ѵ�
 *
 * aWASegment  - [IN] ��� WASegment
 * aWCBPtr     - [IN] �Ҵ��� ù��° WCB�� Pointer  
 ***************************************************************************/
IDE_RC sdtSortSegment::setWAPagePtr( sdtSortSegHdr * aWASegment,
                                     sdtWCB        * aWCBPtr  )
{
    IDE_ASSERT( aWCBPtr->mWAPagePtr == NULL );
    IDE_DASSERT( aWCBPtr->mNxtUsedWCBPtr != NULL );

    if ( aWASegment->mCurrFreePageIdx >= SDT_WAEXTENT_PAGECOUNT )
    {
        if ( aWASegment->mCurFreeWAExtent->mNextExtent == NULL )
        {
            IDE_ERROR( aWASegment->mMaxWAExtentCount > aWASegment->mWAExtentInfo.mCount );

            IDE_TEST( expandFreeWAExtent( aWASegment ) != IDE_SUCCESS );
            IDE_DASSERT( aWASegment->mCurFreeWAExtent->mNextExtent != NULL );
        }

        aWASegment->mCurrFreePageIdx = 0;
        aWASegment->mCurFreeWAExtent = aWASegment->mCurFreeWAExtent->mNextExtent;
    }
    aWCBPtr->mWAPagePtr = aWASegment->mCurFreeWAExtent->mPage[aWASegment->mCurrFreePageIdx];
    aWASegment->mCurrFreePageIdx++;
    aWASegment->mAllocWAPageCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Map�� �����ϴ� ��ġ�� ��, ��� ������ Group�� ũ�⸦ Page������ �޴´�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 * aWAGroupID     - ��� Group ID
 ***************************************************************************/
UInt sdtSortSegment::getAllocableWAGroupPageCount( sdtSortGroup * aGrpInfo )
{
    return aGrpInfo->mEndWPID - aGrpInfo->mBeginWPID
        - sdtWASortMap::getWPageCount( aGrpInfo->mSortMapHdr );
}

/**************************************************************************
 * Description :
 * WASegemnt ������ File�� Dump�Ѵ�.
 *
 * <IN>
 * aWASegment     - ��� WASegment
 ***************************************************************************/
void sdtSortSegment::exportSortSegmentToFile( sdtSortSegHdr* aWASegment )
{
    SChar       sFileName[ SM_MAX_FILE_NAME ];
    SChar       sDateString[ SMI_TT_STR_SIZE ];
    iduFile     sFile;
    UInt        sState = 0;
    ULong       sOffset = 0;
    ULong       sSize;
    ULong       sMaxPageCount;
    scPageID    sWPageID;
    sdtWCB    * sWCBPtr;
    sdtWCB    * sEndWCBPtr;
    sdtNExtentArr * sNExtentArr;

    IDE_ASSERT( aWASegment != NULL );

    IDE_TEST( sFile.initialize( IDU_MEM_SM_TEMP,
                                1, // Max Open FD Count
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS);
    sState = 1;

    smuUtility::getTimeString( smiGetCurrTime(),
                               SMI_TT_STR_SIZE,
                               sDateString );

    idlOS::snprintf( sFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s_sort.td",
                     smuProperty::getTempDumpDirectory(),
                     IDL_FILE_SEPARATOR,
                     sDateString );

    IDE_TEST( sFile.setFileName( sFileName ) != IDE_SUCCESS);
    IDE_TEST( sFile.createUntilSuccess( smiSetEmergency )
              != IDE_SUCCESS);
    IDE_TEST( sFile.open( ID_FALSE ) != IDE_SUCCESS ); //DIRECT_IO
    sState = 2;

    // sdtSortSegHdr������ �ּ�
    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           0,
                           (UChar*)&aWASegment,
                           ID_SIZEOF(sdtSortSegHdr*) )
              != IDE_SUCCESS );
    sOffset += ID_SIZEOF(sdtSortSegHdr*);

    // sdtSortSegHdr+ WCB Map
    sMaxPageCount = getMaxWAPageCount( aWASegment );
    sSize = calcWASegmentSize( sMaxPageCount );

    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           sOffset,
                           (UChar*)aWASegment,
                           sSize )
              != IDE_SUCCESS );
    sOffset += sSize;

    // Normal page Hash Map
    sSize = aWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*);

    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           sOffset,
                           (UChar*)aWASegment->mNPageHashPtr,
                           sSize )
              != IDE_SUCCESS );
    sOffset += sSize;

    // Normal Extent Map
    for ( sNExtentArr =  aWASegment->mNExtFstPIDList.mHead;
          sNExtentArr != NULL;
          sNExtentArr = sNExtentArr->mNextArr )
    {
        IDE_TEST( sFile.write( aWASegment->mStatistics,
                               sOffset,
                               (UChar*)sNExtentArr,
                               ID_SIZEOF( sdtNExtentArr ) )
                  != IDE_SUCCESS );
        sOffset += ID_SIZEOF( sdtNExtentArr );
    }

    // WA Extents

    sWCBPtr    = aWASegment->mWCBMap;
    sEndWCBPtr = sWCBPtr + sMaxPageCount;
    sWPageID   = 0;

    while( sWCBPtr < sEndWCBPtr )
    {
        if ( sWCBPtr->mWAPagePtr != NULL )
        {
            IDE_TEST( sFile.write( aWASegment->mStatistics,
                                   sOffset,
                                   (UChar*)&sWPageID,
                                   ID_SIZEOF(scPageID) )
                      != IDE_SUCCESS );
            sOffset += ID_SIZEOF(ULong);

            IDE_TEST( sFile.write( aWASegment->mStatistics,
                                   sOffset,
                                   (UChar*)sWCBPtr->mWAPagePtr,
                                   SD_PAGE_SIZE )
                      != IDE_SUCCESS );
            sOffset += SD_PAGE_SIZE;
        }

        sWCBPtr++;
        sWPageID++;
    }

    sWPageID = SM_SPECIAL_PID;
    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           sOffset,
                           (UChar*)&sWPageID,
                           ID_SIZEOF(scPageID) )
              != IDE_SUCCESS );

    IDE_TEST( sFile.sync() != IDE_SUCCESS);

    sState = 1;
    IDE_TEST( sFile.close() != IDE_SUCCESS);

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS);

    ideLog::log( IDE_DUMP_0, "TEMP_DUMP_FILE : %s", sFileName );

    return;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void) sFile.close() ;
        case 1:
            (void) sFile.destroy();
            break;
        default:
            break;
    }

    return;
}

/**************************************************************************
 * Description :
 * File�κ��� WASegment�� �о���δ�.
 *
 * <IN>
 * aFileName      - ��� File
 * <OUT>
 * aSortSegHdr    - �о�帰 WASegment�� ��ġ
 * aRawSortSegHdr - WA���� ������ ���� ��ġ
 * aPtr           - �� ���۸� Align�� ���� �������� ��ġ
 ***************************************************************************/
IDE_RC sdtSortSegment::importSortSegmentFromFile( SChar         * aFileName,
                                                  sdtSortSegHdr** aSortSegHdr,
                                                  sdtSortSegHdr * aRawSortSegHdr,
                                                  UChar        ** aPtr )
{
    iduFile         sFile;
    UInt            sState          = 0;
    UInt            sMemAllocState  = 0;
    ULong           sSize = 0;
    sdtSortSegHdr * sWASegment = NULL;
    UChar         * sPtr;
    ULong           sMaxPageCount;
    UChar         * sOldWASegPtr;
    sdtNExtentArr * sNExtentArr;
    UInt            i;

    IDE_ERROR( sFile.initialize(IDU_MEM_SM_SMU,
                                1, // Max Open FD Count
                                IDU_FIO_STAT_OFF,
                                IDV_WAIT_INDEX_NULL )
               == IDE_SUCCESS );
    sState = 1;

    IDE_ERROR( sFile.setFileName( aFileName ) == IDE_SUCCESS );
    IDE_ERROR( sFile.open() == IDE_SUCCESS );
    sState = 2;

    IDE_ERROR( sFile.getFileSize( &sSize ) == IDE_SUCCESS );

    IDE_ERROR( iduMemMgr::malloc( IDU_MEM_SM_TEMP,
                                  sSize,
                                  (void**)&sPtr )
               == IDE_SUCCESS );
    sMemAllocState = 1;

    IDE_ERROR( sFile.read( NULL,  // aStatistics
                           0,     // aWhere
                           sPtr,
                           sSize,
                           NULL ) // aEmergencyFunc
               == IDE_SUCCESS );

    // Pointer ������ ���� �޸� ��ġ�� �߸��Ǿ� ������, �������Ѵ�.
    *aPtr        = sPtr;
    // sdtSortSegHdr+ WCB Map
    sOldWASegPtr = *(UChar**)sPtr;

    sPtr += ID_SIZEOF(sdtSortSegHdr*);

    sWASegment = (sdtSortSegHdr*)sPtr;

    // ���� �ִ� �״�� ��� �� �ʿ䵵 �ִ�.
    idlOS::memcpy((UChar*)aRawSortSegHdr,
                  (UChar*)sWASegment,
                  SDT_SORT_SEGMENT_HEADER_SIZE);

    sMaxPageCount = getMaxWAPageCount( sWASegment ) ;
    sPtr += calcWASegmentSize( sMaxPageCount );

    IDE_ERROR( (ULong)sPtr % ID_SIZEOF(ULong) == 0);

    // Normal page Hash Map
    sWASegment->mNPageHashPtr = (sdtWCB**)sPtr;
    sPtr += sWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*);

    // Normal Extent Map
    if ( sWASegment->mNExtFstPIDList.mHead != NULL )
    {
        sWASegment->mNExtFstPIDList.mHead = (sdtNExtentArr*)sPtr;
        sPtr += ID_SIZEOF( sdtNExtentArr );

        for ( sNExtentArr =  sWASegment->mNExtFstPIDList.mHead;
              sNExtentArr->mNextArr != NULL;
              sNExtentArr = sNExtentArr->mNextArr )
        {
            sNExtentArr->mNextArr = (sdtNExtentArr*)sPtr;
            sPtr += ID_SIZEOF( sdtNExtentArr );
        }
        sWASegment->mNExtFstPIDList.mTail = sNExtentArr;
    }

    IDE_TEST( resetWCBInfo4Dump( sWASegment,
                                 sOldWASegPtr,
                                 sMaxPageCount,
                                 &sPtr )
              != IDE_SUCCESS );

    for( i = 0 ; i < SDT_WAGROUPID_MAX ; i++ )
    {
        if ( sWASegment->mGroup[ i ].mHintWCBPtr != NULL )
        {
            sWASegment->mGroup[ i ].mHintWCBPtr =
                (sdtWCB*)(((UChar*)sWASegment->mGroup[ i ].mHintWCBPtr)
                          - sOldWASegPtr + (UChar*)sWASegment) ;
        }
    }

    if ( sWASegment->mUsedWCBPtr != NULL )
    {
        sWASegment->mUsedWCBPtr =
            (sdtWCB*)(((UChar*)sWASegment->mUsedWCBPtr)
                      - sOldWASegPtr + (UChar*)sWASegment) ;
    }

// Map�� Segment��ġ �� Group���� Pointer ������
    IDE_ERROR( sdtWASortMap::resetPtrAddr( sWASegment )
               == IDE_SUCCESS );

    if ( sWASegment->mSortMapHdr.mWASegment != NULL )
    {
        sWASegment->mSortMapHdr.mWASegment = sWASegment ;
    }

    sState = 1;
    IDE_ERROR( sFile.close() == IDE_SUCCESS );
    sState = 0;
    IDE_ERROR( sFile.destroy() == IDE_SUCCESS );

    *aSortSegHdr  = sWASegment;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMemAllocState == 1  )
    {
        IDE_ASSERT( iduMemMgr::free( sPtr ) == IDE_SUCCESS );
    }

    switch(sState)
    {
        case 2:
            IDE_ASSERT( sFile.close() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sFile.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}


/***************************************************************************
 * Description : WAMap�� WCB�� Pointer�� Load�� Memory�� �籸���Ѵ�.
 *
 * aWASegment     - [IN] �籸�� ���� Sort Segment
 * aOldWASegPtr   - [IN] Dump �ϱ� ���� Sort Segment�� Pointer
 * aMaxPageCount  - [IN] Page�� Count
 * aPtr           - [IN] �о���� Buffer
 ***************************************************************************/
IDE_RC sdtSortSegment::resetWCBInfo4Dump( sdtSortSegHdr * aWASegment,
                                          UChar         * aOldWASegPtr,
                                          UInt            aMaxPageCount,
                                          UChar        ** aPtr )
{
    sdtWCB        * sWCBPtr;
    sdtWCB        * sEndWCBPtr;
    scPageID        sWPageID;
    scPageID        sNxtPageID;
    UChar         * sPtr = *aPtr;

    sWCBPtr    = aWASegment->mWCBMap;
    sEndWCBPtr = sWCBPtr + aMaxPageCount;
    sWPageID   = 0;
    sNxtPageID = *(scPageID*)sPtr;
    sPtr += ID_SIZEOF(ULong);

    while( sWCBPtr < sEndWCBPtr )
    {
        if ( sWCBPtr->mNextWCB4Hash != NULL )
        {
            sWCBPtr->mNextWCB4Hash  = (sdtWCB*)(((UChar*)sWCBPtr->mNextWCB4Hash)
                                                - aOldWASegPtr + (UChar*)aWASegment) ;
        }
        if ( sWCBPtr->mNxtUsedWCBPtr > SDT_USED_PAGE_PTR_TERMINATED )
        {
            sWCBPtr->mNxtUsedWCBPtr = (sdtWCB*)(((UChar*)sWCBPtr->mNxtUsedWCBPtr)
                                                - aOldWASegPtr + (UChar*)aWASegment) ;
        }

        // 6. wa page ( wapageID, page )
        if ( sWPageID == sNxtPageID )
        {
            sWCBPtr->mWAPagePtr = sPtr;
            sPtr += SD_PAGE_SIZE;

            sNxtPageID = *(scPageID*)sPtr;
            sPtr += ID_SIZEOF(ULong);

            IDE_ERROR(( sWPageID < sNxtPageID ) ||
                      ( sNxtPageID == SM_NULL_PID ));
        }
        else
        {
            sWCBPtr->mWAPagePtr = NULL;
        }
        sWCBPtr++;
        sWPageID++;
    }

    *aPtr = sPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void sdtSortSegment::dumpWASegment( void           * aWASegment,
                                    SChar          * aOutBuf,
                                    UInt             aOutSize )
{
    sdtSortSegHdr  * sWASegment = (sdtSortSegHdr*)aWASegment;
    SInt             i;

    (void)idlVA::appendFormat(
        aOutBuf,
        aOutSize,
        "DUMP WASEGMENT:\n"
        "\tWASegPtr         : 0x%"ID_xINT64_FMT"\n"
        "\tSpaceID          : %"ID_UINT32_FMT"\n"
        "\tType             : SORT\n"
        "\tNExtentCount     : %"ID_UINT32_FMT"\n"
        "\tWAExtentCount    : %"ID_UINT32_FMT"\n"
        "\tMaxWAExtentCount : %"ID_UINT32_FMT"\n"
        "\tPageSeqInLFE     : %"ID_UINT32_FMT"\n"

        "\tHintWCBPtr       : 0x%"ID_xPOINTER_FMT"\n"
        "\tWAExtentListHead : 0x%"ID_xPOINTER_FMT"\n"
        "\tWAExtentListTail : 0x%"ID_xPOINTER_FMT"\n"
        "\tCurFreeWAExtent  : 0x%"ID_xPOINTER_FMT"\n"
        "\tmCurrFreePageIdx : %"ID_UINT32_FMT"\n"

        "\tAllocPageCount   : %"ID_UINT32_FMT"\n"
        "\tNPageCount       : %"ID_UINT32_FMT"\n"
        "\tLastFreeExtFstPID: %"ID_UINT32_FMT"\n"
        "\tInMemory         : %s\n"
        "\tNPageHashBucketCnt: %"ID_UINT32_FMT"\n"
        "\tUsedWCBPtr       : 0x%"ID_xPOINTER_FMT"\n",
        sWASegment,
        sWASegment->mSpaceID,
        getNExtentCount( sWASegment ),
        sWASegment->mWAExtentInfo.mCount,
        sWASegment->mMaxWAExtentCount,
        sWASegment->mNExtFstPIDList.mPageSeqInLFE,

        sWASegment->mHintWCBPtr,
        sWASegment->mWAExtentInfo.mHead,
        sWASegment->mWAExtentInfo.mTail,
        sWASegment->mCurFreeWAExtent,
        sWASegment->mCurrFreePageIdx,

        sWASegment->mAllocWAPageCount,
        sWASegment->mNPageCount,
        sWASegment->mNExtFstPIDList.mLastFreeExtFstPID,
        sWASegment->mIsInMemory == SDT_WORKAREA_OUT_MEMORY ? "OutMemory" : "InMemory",
        sWASegment->mNPageHashBucketCnt,
        sWASegment->mUsedWCBPtr );

    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );

    for( i = 0 ; i < SDT_WAGROUPID_MAX ; i++ )
    {
        dumpWAGroup( sWASegment, i, aOutBuf, aOutSize );
    }

    return;
}


void sdtSortSegment::dumpWAGroup( sdtSortSegHdr  * aWASegment,
                                  sdtGroupID       aWAGroupID,
                                  SChar          * aOutBuf,
                                  UInt             aOutSize )
{
    sdtSortGroup   * sGrpInfo = getWAGroupInfo( aWASegment, aWAGroupID );
    scPageID         sHintPageID;

    if ( sGrpInfo->mPolicy != SDT_WA_REUSE_NONE )
    {
        sHintPageID = (sGrpInfo->mHintWCBPtr == NULL ) ? 0 : getWPageID( aWASegment,
                                                                         sGrpInfo->mHintWCBPtr );
        (void)idlVA::appendFormat(
            aOutBuf,
            aOutSize,
            "DUMP WAGROUP:\n"
            "\tID     : %"ID_UINT32_FMT"\n"
            "\tPolicy : %-4"ID_UINT32_FMT
            "(0:None, 1:InMemory, 2:FIFO, 3:LRU, 4:HASH)\n"
            "\tRange  : %"ID_UINT32_FMT" <-> %"ID_UINT32_FMT"\n"
            "\tReuseP : Seq: %"ID_UINT32_FMT", "
            "Top: %"ID_UINT32_FMT", Bot: %"ID_UINT32_FMT"\n"
            "\tHint   : %"ID_UINT32_FMT"\n"
            "\tHintPtr: 0x%"ID_xPOINTER_FMT"\n"
            "\tMapPtr : 0x%"ID_xINT64_FMT"\n\n",
            aWAGroupID,
            sGrpInfo->mPolicy,
            sGrpInfo->mBeginWPID,
            sGrpInfo->mEndWPID,
            sGrpInfo->mReuseWPIDSeq,
            sGrpInfo->mReuseWPIDTop,
            sGrpInfo->mReuseWPIDBot,
            sHintPageID,
            sGrpInfo->mHintWCBPtr,
            sGrpInfo->mSortMapHdr );
    }

    return;
}

/**************************************************************************
 * Description : Sort Segment�� ��� WCB�� ������ �����
 *
 * aWASegment - [IN] WCB�� Dump �� Sort Segment
 * aOutBuf    - [IN] Dump�� ������ ��Ƶ� ��� Buffer
 * aOutSize   - [IN] ��� Buffer�� Size
 ***************************************************************************/
void sdtSortSegment::dumpWCBs( void           * aWASegment,
                               SChar          * aOutBuf,
                               UInt             aOutSize )
{
    sdtWCB         * sWCBPtr;
    sdtWCB         * sEndWCBPtr;
    scPageID         sWPageID;

    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "WCB:\n%5s %5s %4s %8s %8s %8s %-18s %-18s %-18s %-18s\n",
                               "WPID",
                               "STATE",
                               "FIX",
                               "NPAGE_ID",
                               "LRU_PREV",
                               "LRU_NEXT",
                               "HASH_NEXT",
                               "USED_NEXT",
                               "WA_PAGE_PTR" );

    sWCBPtr    = ((sdtSortSegHdr*)aWASegment)->mWCBMap;
    sEndWCBPtr = sWCBPtr + sdtSortSegment::getMaxWAPageCount( (sdtSortSegHdr*)aWASegment );
 
    sWPageID   = 0;

    while( sWCBPtr < sEndWCBPtr )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "%5"ID_UINT32_FMT"",
                                   sWPageID );
        dumpWCB( (void*)sWCBPtr,aOutBuf,aOutSize );
        sWPageID++;
        sWCBPtr++;
    }

    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );

    return;
}

/**************************************************************************
 * Description : 1���� WCB�� ������ �����
 *
 * aWCBPtr    - [IN] Dump �� WCB�� Pointer
 * aOutBuf    - [IN] Dump�� ������ ��Ƶ� ��� Buffer
 * aOutSize   - [IN] ��� Buffer�� Size
 ***************************************************************************/
void sdtSortSegment::dumpWCB( void           * aWCB,
                              SChar          * aOutBuf,
                              UInt             aOutSize )
{
    sdtWCB   * sWCBPtr;

    sWCBPtr = (sdtWCB*) aWCB;

    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               " %5"ID_UINT32_FMT
                               " %4"ID_UINT32_FMT
                               " %8"ID_UINT32_FMT
                               " %8"ID_UINT32_FMT
                               " %8"ID_UINT32_FMT
                               " %s%-16"ID_xPOINTER_FMT
                               " %s%-16"ID_xPOINTER_FMT
                               " %s%-16"ID_xPOINTER_FMT"\n",
                               sWCBPtr->mWPState,
                               sWCBPtr->mFix,
                               sWCBPtr->mNPageID,
                               sWCBPtr->mLRUPrevPID,
                               sWCBPtr->mLRUNextPID,
                               (sWCBPtr->mNextWCB4Hash != NULL ) ?  "0x" : "  ",
                               sWCBPtr->mNextWCB4Hash,
                               (sWCBPtr->mNxtUsedWCBPtr > (sdtWCB*)1 ) ? "0x" : "  ",
                               sWCBPtr->mNxtUsedWCBPtr,
                               (sWCBPtr->mWAPagePtr != NULL ) ? "0x" : "  ",
                               sWCBPtr->mWAPagePtr );
}
