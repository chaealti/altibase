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

#include <sdtDef.h>
#include <smiMisc.h>
#include <smuProperty.h>
#include <sdtTempPage.h>
#include <sdtHashModule.h>
#include <sdtWAExtentMgr.h>

smiColumn     sdtHashModule::mBlankColumn;
UInt          sdtHashModule::mSubHashPageCount;
UInt          sdtHashModule::mHashSlotPageCount;
iduMutex      sdtHashModule::mMutex;
smuMemStack   sdtHashModule::mWASegmentPool;
smuMemStack   sdtHashModule::mNHashMapPool;
UInt          sdtHashModule::mMaxWAExtentCount;

/**************************************************************************
 * Description : Hash SegPool�� ����� Mutex�� ���� �Ѵ�.
 *               ���� �ø��� ȣ��ȴ�.
 ***************************************************************************/
IDE_RC sdtHashModule::initializeStatic()
{    
    UInt    sState = 0;

    idlOS::memset( &mBlankColumn, 0, ID_SIZEOF( smiColumn ) );

    IDE_TEST( mMutex.initialize( (SChar*)"SDT_HASH_SEGMENT_MUTEX",
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    mMaxWAExtentCount = calcWAExtentCount( smuProperty::getHashAreaSize() );
    resetTempHashGroupPageCount();
    resetTempSubHashGroupPageCount();
    
    IDE_TEST( mWASegmentPool.initialize( IDU_MEM_SM_TEMP,
                                         getWASegmentSize(),
                                         smuProperty::getTmpInitWASegCnt())
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mNHashMapPool.initialize( IDU_MEM_SM_TEMP,
                                        getMaxWAExtentCount() * SDT_WAEXTENT_PAGECOUNT *
                                        ID_SIZEOF(sdtWCB*) / smuProperty::getTempHashBucketDensity(),
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
 * Description : Mutex�� ���� Pool�� free �Ѵ�.
 *               ���� ������ ȣ��ȴ�.
 ***************************************************************************/
void sdtHashModule::destroyStatic()
{
    (void)mWASegmentPool.destroy();

    (void)mNHashMapPool.destroy();

    mMutex.destroy();
}

/**************************************************************************
 * Description : Hash Group Page Count�� �ʱ�ȭ �Ѵ�.
 ***************************************************************************/
void sdtHashModule::resetTempHashGroupPageCount()
{
    mHashSlotPageCount = SDT_WAEXTENT_PAGECOUNT *
        calcTempHashGroupExtentCount( getMaxWAExtentCount() );
}

/**************************************************************************
 * Description : Hash Group Extent Count�� ��� �Ѵ�.
 *               HashSlot�� ���� 65535�� ����� �Ǿ�� �Ѵ�. �̷��� �ϸ�,
 *               Hash Value 4Byte �� ���� 2Byte�� ���� ��ġ�ϰ� �ȴ�.
 *               �׷� Sub Hash�� ���� ���� 2Byte������ ���� ���� �����ϰ� �ȴ�.
 ***************************************************************************/
UInt sdtHashModule::calcTempHashGroupExtentCount( UInt    aMaxWAExtentCount )
{
    UInt sHashGroupExtentCount;

    sHashGroupExtentCount = aMaxWAExtentCount * smuProperty::getTempHashGroupRatio() / 100;

    if ( sHashGroupExtentCount > ( aMaxWAExtentCount
                                   - SDT_MIN_HASHROW_EXTENTCOUNT
                                   - SDT_MIN_SUBHASH_EXTENTCOUNT ) )
    {
        sHashGroupExtentCount = ( aMaxWAExtentCount
                                  - SDT_MIN_HASHROW_EXTENTCOUNT
                                  - SDT_MIN_SUBHASH_EXTENTCOUNT );
    }

    if ( sHashGroupExtentCount <= SDT_MIN_HASHSLOT_EXTENTCOUNT )
    {
        /* �ּҰ� ����*/
        sHashGroupExtentCount = SDT_MIN_HASHSLOT_EXTENTCOUNT;
    }
    else
    {
        /* 2�� WAExtent�� ����� �Ҵ� �Ͽ��� �Ѵ�. */
        if (( sHashGroupExtentCount % SDT_MIN_HASHSLOT_EXTENTCOUNT ) > 0 )
        {
            sHashGroupExtentCount -= ( sHashGroupExtentCount % SDT_MIN_HASHSLOT_EXTENTCOUNT );
        }
    }
    return sHashGroupExtentCount;
}

/**************************************************************************
 * Description : Sub Hash Group Page Count�� �ʱ�ȭ �Ѵ�.
 ***************************************************************************/
void sdtHashModule::resetTempSubHashGroupPageCount()
{
    mSubHashPageCount = SDT_WAEXTENT_PAGECOUNT *
        calcTempSubHashGroupExtentCount( getMaxWAExtentCount() ) ;
}

/**************************************************************************
 * Description : Sub Hash Group Extent Count�� ��� �Ѵ�.
 ***************************************************************************/
UInt sdtHashModule::calcTempSubHashGroupExtentCount( UInt    aMaxWAExtentCount )
{
    SInt sSubHashGroupExtentCount;
    SInt sHashGroupExtentCount = (SInt)getHashSlotPageCount() / SDT_WAEXTENT_PAGECOUNT;

    sSubHashGroupExtentCount = (SInt)aMaxWAExtentCount * smuProperty::getTempSubHashGroupRatio() / 100;

    if ( sSubHashGroupExtentCount > ( (SInt)aMaxWAExtentCount
                                      - sHashGroupExtentCount
                                      - SDT_MIN_HASHROW_EXTENTCOUNT ) )
    {
        sSubHashGroupExtentCount = ( (SInt)aMaxWAExtentCount
                                     - sHashGroupExtentCount
                                     - SDT_MIN_HASHROW_EXTENTCOUNT );
    }

    if ( sSubHashGroupExtentCount < SDT_MIN_SUBHASH_EXTENTCOUNT )
    {
        sSubHashGroupExtentCount = SDT_MIN_SUBHASH_EXTENTCOUNT;
    }

    return sSubHashGroupExtentCount ;
}

/**************************************************************************
 * Description : Row Extent�� ���� �������� ������ Hash Extent�� ���� ����Ͽ� ��ȯ
 ***************************************************************************/
ULong sdtHashModule::calcOptimalWorkAreaSize( UInt  aRowPageCount )
{
    UInt     sRowExtentCount;
    UInt     sAllExtentCount;
    UInt     sHashExtentCount;
    UInt     sSubHashExtentCount;

    sRowExtentCount = ( aRowPageCount + SDT_WAEXTENT_PAGECOUNT - 1 ) / SDT_WAEXTENT_PAGECOUNT;

    if ( sRowExtentCount < SDT_MIN_HASHROW_EXTENTCOUNT )
    {
        sRowExtentCount = SDT_MIN_HASHROW_EXTENTCOUNT ;
    }

    sAllExtentCount = SDT_GET_FULL_SIZE( sRowExtentCount,
                                         ( 100 - smuProperty::getTempHashGroupRatio() - smuProperty::getTempSubHashGroupRatio() ) );

    sHashExtentCount    = calcTempHashGroupExtentCount( sAllExtentCount );
    sSubHashExtentCount = calcTempSubHashGroupExtentCount( sAllExtentCount );

    sAllExtentCount = ( sRowExtentCount + sHashExtentCount + sSubHashExtentCount );

    if ( sAllExtentCount < SDT_MIN_HASH_EXTENTCOUNT )
    {
        sAllExtentCount = SDT_MIN_HASH_EXTENTCOUNT;
    }

    return (ULong)sAllExtentCount * SDT_WAEXTENT_SIZE;
}

/**************************************************************************
 * Description : Row Count �� �������� ������ Work Area Size�� ����Ͽ� ��ȯ
 ***************************************************************************/
ULong sdtHashModule::calcSubOptimalWorkAreaSizeByRowCnt( ULong aRowCount )
{
    UInt     sKeyCount;
    UInt     sHashPageCount;
    UInt     sHashExtentCount;

    sKeyCount = aRowCount * 5 / 4; /* SubHashNodeHeader �� �� 20%�� ����*/

    /* SubHash�� ��� load�ϴµ� �ʿ��� Extent*/
    sHashPageCount   = ( sKeyCount * ID_SIZEOF(sdtSubHashKey) + SD_PAGE_SIZE - 1 ) / SD_PAGE_SIZE;
    sHashExtentCount = ( sHashPageCount + SDT_WAEXTENT_PAGECOUNT - 1 ) / SDT_WAEXTENT_PAGECOUNT ;

    return calcSubOptimalWorkAreaSizeBySubHashExtCnt( sHashExtentCount ) ;
}

/**************************************************************************
 * Description : Sub Hash�� NExtent Count �� ��������
 *               ������ Work Area Size �� ����Ͽ� ��ȯ
 *               onepass �� �Ƿ��� Sub Hash�� �ѹ��� load�����ؾ� �Ѵ�.
 *               Hash Group�� 6%�̰� Key�� 16Byte�ε� ���ؼ�
 *               Sub Hash�� Fetch �� �� Row Area �� 90%���� Ȯ��ǰ�,
 *               Sub Hash Key�� 8Byte�̴�,
 *               Hash Slot ���� �� ���� Sub Hash Key�� ���� �� �ִ�.
 *               Hash Group���� ��� Ŀ�� ���� �� ���� In Memory�� �ٸ�����.
 *               �׷��� Sub Hash �������� ����Ѵ�.
 ***************************************************************************/
ULong sdtHashModule::calcSubOptimalWorkAreaSizeBySubHashExtCnt( UInt aSubHashExtentCount )
{
    UInt     sAllExtentCount;
    UInt     sRowExtentCount;
    UInt     sHashExtentCount;

    /* HashScan ������ SubHash�� ��� Load�ϱ� ���� Row Area�� ũ��*/
    sRowExtentCount  = SDT_GET_FULL_SIZE( aSubHashExtentCount,
                                          smuProperty::getTmpHashFetchSubHashMaxRatio() );

    if ( sRowExtentCount > SDT_MIN_HASHROW_EXTENTCOUNT )
    {
        /* Hash Slot Group Extent Count ���*/
        sAllExtentCount  = SDT_GET_FULL_SIZE( sRowExtentCount,
                                              ( 100 - smuProperty::getTempHashGroupRatio() ) );
        sHashExtentCount = calcTempHashGroupExtentCount( sAllExtentCount );

        /* Hash Group�� Row Area�� ���Ͽ� HASH_AREA_SIZE�� ���� */
        sAllExtentCount  = sHashExtentCount + sRowExtentCount;

        if ( sAllExtentCount < SDT_MIN_HASH_EXTENTCOUNT )
        {
            sAllExtentCount = SDT_MIN_HASH_EXTENTCOUNT;
        }
    }
    else
    {
        // Default Size���� �۴ٸ� Min Size�� �ѱ��.
        sAllExtentCount = SDT_MIN_HASH_EXTENTCOUNT;
    }

    return (ULong)sAllExtentCount * SDT_WAEXTENT_SIZE;
}


/**************************************************************************
 * Description : HASH_AREA_SIZE ���濡 ���� Hash Segment �� ũ�⸦ �����Ѵ�.
 *               �̹� ������� Hash Segment�� ���� ũ�⸦ �����ϰ�,
 *               ���� �����Ǵ� Hash Segment���� �� ũ��� �����ȴ�.
 *               lock�� ���� ü�� ȣ�� �Ǿ�� �Ѵ�.
 *
 * aNewWorkAreaSize  - [IN] ���� �� ũ��
 ***************************************************************************/
IDE_RC sdtHashModule::resetWASegmentSize( ULong aNewWorkAreaSize )
{
    UInt   sNewWAExtentCount = calcWAExtentCount( aNewWorkAreaSize ) ;
    UInt   sNewWAPageCount   = sNewWAExtentCount * SDT_WAEXTENT_PAGECOUNT ;
    UInt   sNewWASegmentSize = calcWASegmentSize( sNewWAPageCount );
    UInt   sOldWAExtentCount = getMaxWAExtentCount();
    UInt   sHashBucketCnt    = sNewWAPageCount  / smuProperty::getTempHashBucketDensity();
    UInt   sState = 0;

    IDE_TEST( mWASegmentPool.resetDataSize( sNewWASegmentSize ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( mNHashMapPool.resetDataSize( sHashBucketCnt * ID_SIZEOF(sdtWCB*) ) != IDE_SUCCESS );
    sState = 2;

    mMaxWAExtentCount = sNewWAExtentCount;
    sState = 3;

    /* HASH AREA SIZE�� ����Ǹ�, group size�� ���� �ؾ� �Ѵ�.*/
    sdtHashModule::resetTempHashGroupPageCount();
    sdtHashModule::resetTempSubHashGroupPageCount();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            mMaxWAExtentCount = sOldWAExtentCount;
            sdtHashModule::resetTempHashGroupPageCount();
            sdtHashModule::resetTempSubHashGroupPageCount();
        case 2:
            (void)mNHashMapPool.resetDataSize( sOldWAExtentCount * SDT_WAEXTENT_PAGECOUNT /
                                               smuProperty::getTempHashBucketDensity()
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
 *               Hash Segment �� ũ�⸦ �����Ѵ�. �� ȣ����� �ʴ´�.
 *
 * aNewWASegmentCount  - [IN] ���� �� ��
 ***************************************************************************/
IDE_RC sdtHashModule::resizeWASegmentPool( UInt   aNewWASegmentCount )
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
 * Description : temp table header�� �ʿ��� ���� �����Ѵ�.
 *
 * aHeader  - [IN] ��� Table
 ***************************************************************************/
void sdtHashModule::initHashTempHeader( smiTempTableHeader * aHeader )
{
    sdtHashSegHdr   * sWASeg = (sdtHashSegHdr*)aHeader->mWASegment;

    if ( SM_IS_FLAG_ON( aHeader->mTTFlag,
                        SMI_TTFLAG_UNIQUE ) == ID_TRUE )
    {
        sWASeg->mUnique = SDT_HASH_UNIQUE ;
    }
    else
    {
        sWASeg->mUnique = SDT_HASH_NORMAL ;
    }
    // temp header
    aHeader->mStatsPtr->mExtraStat1 = sWASeg->mHashSlotCount;

    return ;
}

/**************************************************************************
 * Description : WA Group�� �ʱ�ȭ �Ѵ�.
 *               [ HashSlot ][      Insert Group     ][ SubHash ]
 *               ���� ���������� fetch group�� ������ �ʴ´�.
 *
 * aWASeg  - [IN] ��� HashSegment
 ***************************************************************************/
void sdtHashModule::initHashGrp( sdtHashSegHdr   * aWASeg )
{
    IDE_DASSERT(( aWASeg->mSubHashPageCount % SDT_WAEXTENT_PAGECOUNT ) == 0 );
    IDE_DASSERT(( aWASeg->mHashSlotPageCount % ( SDT_WAEXTENT_PAGECOUNT * SDT_MIN_HASHSLOT_EXTENTCOUNT )) == 0 );

    aWASeg->mInsertGrp.mBeginWPID    = aWASeg->mHashSlotPageCount;
    aWASeg->mInsertGrp.mReuseSeqWPID = aWASeg->mHashSlotPageCount;
    aWASeg->mInsertGrp.mEndWPID      = aWASeg->mEndWPID - aWASeg->mSubHashPageCount;

    /* discard�� �Ǳ� �� ������ fetch�� page�� ��� inmemory�� �ִ�.
     * �׷��Ƿ� fetch group�� ���� discard �� �߻� �� �Ŀ� �����Ѵ�.*/
    aWASeg->mFetchGrp.mBeginWPID    = aWASeg->mEndWPID;
    aWASeg->mFetchGrp.mReuseSeqWPID = aWASeg->mEndWPID;
    aWASeg->mFetchGrp.mEndWPID      = aWASeg->mEndWPID;

    /* �Ĺ濡 Sub Hash������ ���� ������ ���ܵд�.*/
    aWASeg->mSubHashGrp.mBeginWPID    = aWASeg->mEndWPID - aWASeg->mSubHashPageCount;
    aWASeg->mSubHashGrp.mReuseSeqWPID = aWASeg->mEndWPID - aWASeg->mSubHashPageCount;
    aWASeg->mSubHashGrp.mEndWPID      = aWASeg->mEndWPID;

    IDE_DASSERT( aWASeg->mInsertGrp.mEndWPID  > aWASeg->mInsertGrp.mBeginWPID );
    IDE_DASSERT( aWASeg->mSubHashGrp.mEndWPID > aWASeg->mSubHashGrp.mBeginWPID );

    return ;
}

/**************************************************************************
 * Description : Hash Segment�� �����Ͽ� Header�� �޾��ش�.
 *
 * aHeader           - [IN] TempTable Header
 * aInitWorkAreaSize - [IN] ���� �Ҵ��� WA�� ũ��
 ***************************************************************************/
IDE_RC sdtHashModule::createHashSegment( smiTempTableHeader * aHeader,
                                         ULong                aInitWorkAreaSize )
{
    sdtHashSegHdr  * sWASeg = NULL;
    UInt             sMaxWAExtentCount;
    UInt             sInitWAExtentCount;
    UInt             sMaxWAPageCount;
    UInt             sSubHashPageCount;
    UInt             sHashSlotPageCount;
    UInt             sState  = 0;
    idBool           sIsLock = ID_FALSE;

    lock();
    sIsLock = ID_TRUE;
    sMaxWAExtentCount = getMaxWAExtentCount();
    sMaxWAPageCount   = sMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT;

    IDE_TEST( mWASegmentPool.getDataSize() != calcWASegmentSize( sMaxWAPageCount ) );

    IDE_TEST( mWASegmentPool.pop( (UChar**)&sWASeg ) != IDE_SUCCESS );
    sState = 1;

    /* sMaxWAPageCount, sSubHashPageCount, sHashSlotPageCount��
     * ���� ������ �ִ� ũ���̹Ƿ� lock �ȿ��� �ѹ��� �޾ƿ;� �Ѵ�.*/
    sSubHashPageCount  = getSubHashPageCount();
    sHashSlotPageCount = getHashSlotPageCount();

    IDE_DASSERT( sWASeg != NULL );

    sIsLock = ID_FALSE;
    unlock();

    idlOS::memset( (UChar*)sWASeg, 0, SDT_HASH_SEGMENT_HEADER_SIZE );

    sWASeg->mMaxWAExtentCount = sMaxWAExtentCount ;

    if ( aInitWorkAreaSize == 0 )
    {
        sInitWAExtentCount = smuProperty::getTmpMinInitWAExtCnt();
    }
    else
    {
        sInitWAExtentCount = sdtWAExtentMgr::calcWAExtentCount( aInitWorkAreaSize );

        // Hash Area�� Row Area�� 1����
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
    sWASeg->mSpaceID           = aHeader->mSpaceID;
    sWASeg->mWorkType          = SDT_WORK_TYPE_HASH;
    sWASeg->mStatsPtr          = aHeader->mStatsPtr;
    sWASeg->mNPageCount        = 0;
    sWASeg->mIsInMemory        = SDT_WORKAREA_IN_MEMORY;
    sWASeg->mStatistics        = aHeader->mStatistics;
    sWASeg->mAllocWAPageCount  = 0;
    sWASeg->mUsedWCBPtr        = SDT_USED_PAGE_PTR_TERMINATED;
    sWASeg->mEndWPID           = sMaxWAPageCount;
    sWASeg->mSubHashPageCount  = sSubHashPageCount;
    sWASeg->mHashSlotPageCount = sHashSlotPageCount;
    sWASeg->mNExtFstPIDList4Row.mPageSeqInLFE     = SDT_WAEXTENT_PAGECOUNT;
    sWASeg->mNExtFstPIDList4SubHash.mPageSeqInLFE = SDT_WAEXTENT_PAGECOUNT;

    sWASeg->mHashSlotCount = sWASeg->mHashSlotPageCount * (SD_PAGE_SIZE/ID_SIZEOF(sdtHashSlot));
    IDE_DASSERT( sWASeg->mHashSlotCount % (ID_USHORT_MAX+1) == 0 );

    sWASeg->mStatsPtr->mRuntimeMemSize += mWASegmentPool.getNodeSize((UChar*) sWASeg );

    IDE_TEST( sdtWAExtentMgr::initWAExtents( sWASeg->mStatistics,
                                             sWASeg->mStatsPtr,
                                             &sWASeg->mWAExtentInfo,
                                             sInitWAExtentCount )
              != IDE_SUCCESS );
    sState = 2;

    sWASeg->mCurFreeWAExtent = sWASeg->mWAExtentInfo.mHead;
    sWASeg->mCurrFreePageIdx = 0;
    sWASeg->mNxtFreeWAExtent = sWASeg->mCurFreeWAExtent->mNextExtent;

    aHeader->mWASegment = sWASeg;

    initHashGrp( sWASeg );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_FALSE )
    {
        lock();
    }

    switch( sState )
    {
        case 2:
            (void)sdtWAExtentMgr::freeWAExtents( &sWASeg->mWAExtentInfo );
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
 * Description : Segment�� Drop �ϰ� �ڿ��� �����Ѵ�.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::dropHashSegment( sdtHashSegHdr* aWASegment )
{
    if ( aWASegment != NULL )
    {
        setInsertHintWCB( aWASegment, NULL );

        IDE_TEST( clearHashSegment( aWASegment ) != IDE_SUCCESS );
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Segment�� Drop�ϰ� ���� Extent�� ��ȯ�Ѵ�.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::clearHashSegment( sdtHashSegHdr* aWASegment )
{
    UInt   sState = 0;
    idBool sIsLock = ID_FALSE;

    if ( aWASegment != NULL )
    {
        sState = 1;

        aWASegment->mNExtFstPIDList4Row.mLastFreeExtFstPID = SM_NULL_PID;

        clearAllWCBs( aWASegment );

        sState = 2;
        IDE_TEST( sdtWAExtentMgr::freeWAExtents( &aWASegment->mWAExtentInfo ) != IDE_SUCCESS );

        sState = 3;
        IDE_TEST( sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList4Row )
                  != IDE_SUCCESS );

        sState = 4;
        IDE_TEST( sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList4SubHash )
                  != IDE_SUCCESS );

        lock();
        sIsLock = ID_TRUE;

        sState = 5;
        if ( aWASegment->mNPageHashPtr != NULL )
        {
#ifdef DEBUG
            for( UInt i = 0 ; i < aWASegment->mNPageHashBucketCnt ; i++ )
            {
                IDE_ASSERT( aWASegment->mNPageHashPtr[i] == NULL );
            }
#endif
            IDE_TEST( mNHashMapPool.push( (UChar*)(aWASegment->mNPageHashPtr) )
                      != IDE_SUCCESS );

            aWASegment->mNPageHashPtr = NULL;
            aWASegment->mNPageHashBucketCnt = 0;
        }

        sState = 6;
        IDE_TEST( mWASegmentPool.push( (UChar*)aWASegment )
                  != IDE_SUCCESS );

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
            clearAllWCBs( aWASegment );
            (void)sdtWAExtentMgr::freeWAExtents( &aWASegment->mWAExtentInfo );
        case 2:
            (void)sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList4Row );
        case 3:
            (void)sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                                   &aWASegment->mNExtFstPIDList4SubHash );
        case 4:
            if ( sIsLock == ID_FALSE )
            {
                lock();
                sIsLock = ID_TRUE;
            }
            if ( aWASegment->mNPageHashPtr != NULL )
            {
                (void)mNHashMapPool.push( (UChar*)(aWASegment->mNPageHashPtr) );
            }
        case 5:
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
 * Description : �� WASegment�� ����� ��� WCB�� Free�Ѵ�.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
void sdtHashModule::clearAllWCBs( sdtHashSegHdr* aWASegment )
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
        sCurWCBPtr = aWASegment->mUsedWCBPtr;

        while( sCurWCBPtr != SDT_USED_PAGE_PTR_TERMINATED )
        {
            sNxtWCBPtr = sCurWCBPtr->mNxtUsedWCBPtr;

            if (( sCurWCBPtr->mNPageID != SD_NULL_PID )&&
                (  aWASegment->mNPageHashPtr != NULL ))
            {
                sHashValue = getNPageHashValue( aWASegment, sCurWCBPtr->mNPageID );

                aWASegment->mNPageHashPtr[ sHashValue ] = NULL;
            }

            initWCB(sCurWCBPtr);
            IDE_DASSERT( aWASegment->mInsertHintWCBPtr != sCurWCBPtr );
            sCurWCBPtr->mNxtUsedWCBPtr = NULL;
            sCurWCBPtr->mWAPagePtr     = NULL;

            sCurWCBPtr = sNxtWCBPtr;
        }

        aWASegment->mUsedWCBPtr = SDT_USED_PAGE_PTR_TERMINATED;
#ifdef DEBUG
        sCurWCBPtr = &aWASegment->mWCBMap[0];
        for( i = 0 ;
             i< getMaxWAPageCount( aWASegment ) ;
             i++ , sCurWCBPtr++ )
        {
            IDE_DASSERT( sCurWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT );
            IDE_DASSERT( sCurWCBPtr->mFix     == 0 );
            IDE_DASSERT( sCurWCBPtr->mNPageID    == SM_NULL_PID );
            IDE_DASSERT( sCurWCBPtr->mNextWCB4Hash  == NULL );
            IDE_DASSERT( sCurWCBPtr->mNxtUsedWCBPtr == NULL );
            IDE_DASSERT( sCurWCBPtr->mWAPagePtr     == NULL );
        }
        for( i = 0 ; i < aWASegment->mNPageHashBucketCnt ; i++ )
        {
            IDE_ASSERT( aWASegment->mNPageHashPtr[i] == NULL );
        }
#endif
    }
    else
    {
        /* ��� memset�ϴ� ���� �������� , ���� BUG�� ����� ���� �ȵɶ��� ����ؼ�
         * memset���� �ʱ�ȭ �ϴ� ��嵵 �����ش�. */
        idlOS::memset( &aWASegment->mWCBMap,
                       0,
                       aWASegment->mMaxWAExtentCount *
                       SDT_WAEXTENT_PAGECOUNT * ID_SIZEOF(sdtWCB) );

        if ( aWASegment->mNPageHashPtr != NULL )
        {
            idlOS::memset( aWASegment->mNPageHashPtr,
                           0,
                           aWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*) );
        }

        aWASegment->mUsedWCBPtr = SDT_USED_PAGE_PTR_TERMINATED;
    }
}

/**************************************************************************
 * Description : Segment�� ��Ȱ���ϱ� ���� �ʱ�ȭ �Ѵ�.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::truncateHashSegment( sdtHashSegHdr* aWASegment )
{
    IDE_ERROR( aWASegment != NULL );

    setInsertHintWCB( aWASegment, NULL );

    initHashGrp( aWASegment );

    aWASegment->mOpenCursorType = SMI_HASH_CURSOR_NONE;

    clearAllWCBs( aWASegment );

#ifdef DEBUG
    if ( aWASegment->mNPageHashPtr != NULL )
    {
        for( UInt i = 0 ; i < aWASegment->mNPageHashBucketCnt ; i++ )
        {
            IDE_ASSERT( aWASegment->mNPageHashPtr[i] == NULL );
        }
    }
#endif
    aWASegment->mUsedWCBPtr        = SDT_USED_PAGE_PTR_TERMINATED;
    aWASegment->mCurFreeWAExtent   = aWASegment->mWAExtentInfo.mHead;
    aWASegment->mCurrFreePageIdx   = 0;
    aWASegment->mNxtFreeWAExtent   = aWASegment->mCurFreeWAExtent->mNextExtent;
    aWASegment->mAllocWAPageCount  = 0;
    aWASegment->mIsInMemory        = SDT_WORKAREA_IN_MEMORY;
    aWASegment->mOpenCursorType    = SMI_HASH_CURSOR_NONE;
    aWASegment->mSubHashBuildCount = 0;
    aWASegment->mHashSlotAllocPageCount = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description : HASH_AREA_SIZE �������� ����Ѵ�.
 *               x$tempinfo ���� Ȯ�� �����ϴ�.
 *
 * aHeader   - [IN] ��� Table
 ***************************************************************************/
void sdtHashModule::estimateHashSize( smiTempTableHeader * aHeader )
{
    sdtHashSegHdr     * sWASegment = (sdtHashSegHdr*)aHeader->mWASegment;
    smiTempTableStats * sStatsPtr  = aHeader->mStatsPtr;

    if ( aHeader->mRowCount > 0 )
    {
        sWASegment = (sdtHashSegHdr*)aHeader->mWASegment;

        IDE_DASSERT( sWASegment->mNPageCount > 0 );

        /* ����Ǹ鼭 ���� ���ġ�� ����Ѵ�. */
        /* Optimal(InMemory)�� ��� �����Ͱ� HashArea�� ��� ũ��� �ȴ�. */
        sStatsPtr->mEstimatedOptimalSize = calcOptimalWorkAreaSize( sWASegment->mNPageCount );

        /* Onepass �� HashSlot�� �ѹ��� Load �� �� �ִ� ũ���̴�.
         * Row Area�� Disk���� �о�� �Ѵ�.*/
        if ( sWASegment->mNExtFstPIDList4SubHash.mCount > 0 )
        {
            /* Out Memory, ��갪�� ���� Size���� �۴�. In Memory �� �ϱ⿡�� ������ �����ߴ�. */
            /* Unique Insert������ �߰� �ʿ��� �κ��� Fetch���� Size��꿡�� ������ ũ�Դ� �Ű� �Ƚᵵ �ȴ�. */
            sStatsPtr->mEstimatedSubOptimalSize =
                calcSubOptimalWorkAreaSizeBySubHashExtCnt( sWASegment->mNExtFstPIDList4SubHash.mCount );
        }
        else
        {
            if ( sStatsPtr->mEstimatedOptimalSize == SDT_MIN_HASH_SIZE )
            {
                /* optimal �� min���̸� ��� �ʿ����.*/
                sStatsPtr->mEstimatedSubOptimalSize = SDT_MIN_HASH_SIZE ;
            }
            else
            {
                /* In Memory, ��갪�� ���� Size���� �۴�. ������ �˳��ߴ�. */
                /* Sub Hash�� �Ҵ���� �ʾҾ��ٰ� �ؼ� ���°��� �ƴϴ�.
                 * Row Count�� �������� ������ ������. */
                sStatsPtr->mEstimatedSubOptimalSize = calcSubOptimalWorkAreaSizeByRowCnt( aHeader->mRowCount );            
            }
        }
    }
    else
    {
        /* ����� ���� ���� ���̺��� ��� ���� �ʴ´�. */
        IDE_DASSERT( aHeader->mRowCount == 0 );
        sStatsPtr->mEstimatedOptimalSize = 0;
        sStatsPtr->mEstimatedSubOptimalSize = 0;
    }

    sStatsPtr->mExtraStat1 = sWASegment->mHashSlotAllocPageCount ;

    return;
}

/**************************************************************************
 * Description : unique violation Ȯ��
 *               ��� inmemory�� ���� �� Ž�� �Լ�,
 *               sub hash���� ������� �ʴ´�.
 *               insert 4 update������ ���� �ϰ� insert�ϹǷ�
 *               �浹�� �߻��ϸ� �ȵȴ�.
 *
 * aHeader     - [IN] ��� Temp Table Header
 * aValueList  - [IN] �ߺ� Ű ���� �� Value
 * aHashValue  - [IN] �ߺ� Ű ���� �� HashValue
 * aTRPHeader  - [IN] Ž�� ���� row
 * aResult     - [OUT] insert�����ϸ� IDE_TRUE, �浹�� ������ ID_FALSE
 ***************************************************************************/
IDE_RC sdtHashModule::chkUniqueInsertableInMem( smiTempTableHeader  * aHeader,
                                                const smiValue      * aValueList,
                                                UInt                  aHashValue,
                                                sdtHashTRPHdr       * aTRPHeader,
                                                idBool              * aResult )
{
    sdtHashScanInfo  sTRPInfo;
    sdtHashSegHdr  * sWASeg = (sdtHashSegHdr*)aHeader->mWASegment;
    UInt             sLoop = 0;

    *aResult = ID_TRUE;
    /* fetch �� �� ũ��, ������ �ʿ��� ��ġ ������ �����Ѵ�.*/
    sTRPInfo.mFetchEndOffset = aHeader->mKeyEndOffset;

    do
    {
        sLoop++;

        IDE_DASSERT( SM_IS_FLAG_ON( aTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

        if ( aTRPHeader->mHashValue == aHashValue )
        {
            sTRPInfo.mTRPHeader = aTRPHeader;

            IDE_TEST( fetch( sWASeg,
                             aHeader->mRowBuffer4Fetch,
                             &sTRPInfo )
                      != IDE_SUCCESS );

            if ( compareRowAndValue( aHeader,
                                     sTRPInfo.mValuePtr,
                                     aValueList ) == ID_TRUE )
            {
                /* for update������ ã�� ��찡 ������ �ȵȴ� */
                IDE_DASSERT( sWASeg->mOpenCursorType != SMI_HASH_CURSOR_HASH_UPDATE );
                *aResult = ID_FALSE;
                break;
            }
        }

        /* to next */
        aTRPHeader = aTRPHeader->mChildRowPtr;

    } while( aTRPHeader != NULL );

    aHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : unique violation Ȯ��
 *               hash area�� �Ѿ out memory �� �� Ž�� �Լ�,
 *               sub hash, single row���� ����ؾ� �Ѵ�.
 *               insert 4 update������ ���� �ϰ� insert�ϹǷ�
 *               �浹�� �߻��ϸ� �ȵȴ�.
 *
 * aHeader     - [IN] ��� Temp Table Header
 * aValueList  - [IN] �ߺ� Ű ���� �� Value
 * aHashValue  - [IN] �ߺ� Ű ���� �� HashValue
 * aTRPHeader  - [IN] Ž�� ���� row
 * aResult     - [OUT] insert�����ϸ� IDE_TRUE, �浹�� ������ ID_FALSE
 ***************************************************************************/
IDE_RC sdtHashModule::chkUniqueInsertableOutMem( smiTempTableHeader  * aHeader,
                                                 const smiValue      * aValueList,
                                                 UInt                  aHashValue,
                                                 sdtHashSlot         * aHashSlot,
                                                 idBool              * aResult )
{
    UInt             sLoop = 0;
    sdtHashSegHdr  * sWASeg = (sdtHashSegHdr*)aHeader->mWASegment;
    sdtHashScanInfo  sTRPInfo;
    sdtHashTRPHdr  * sTRPHeader;
    sdtHashTRPHdr  * sPrvTRPHeader;
    scPageID         sSingleRowPageID = SM_NULL_PID;
    scOffset         sSingleRowOffset;
    scPageID         sSubHashPageID = SM_NULL_PID;
    scOffset         sSubHashOffset;

    IDE_DASSERT( sWASeg->mIsInMemory == SDT_WORKAREA_OUT_MEMORY );

    sTRPInfo.mFetchEndOffset = aHeader->mKeyEndOffset;

    if ( SDT_IS_NOT_SINGLE_ROW( aHashSlot->mOffset ) )
    {
        if ( aHashSlot->mRowPtrOrHashSet == (ULong)NULL )
        {
            /* in memory row�� ����.
             * �ٷ� sub hashŽ������ �Ѿ��.
             * small hash set�� ���� �Ѿ������, null pid�� ���� ����.*/
            sSubHashPageID = aHashSlot->mPageID;
            sSubHashOffset = aHashSlot->mOffset;

            IDE_DASSERT( sSubHashPageID != SD_NULL_PID );
        }
        else
        {
            /* in memory�� ���� Ž���Ѵ�. */

            sTRPHeader = (sdtHashTRPHdr*)aHashSlot->mRowPtrOrHashSet;

            do
            {
                sLoop++;

                IDE_DASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

                if ( sTRPHeader->mHashValue == aHashValue )
                {
                    sTRPInfo.mTRPHeader = sTRPHeader;

                    IDE_TEST( fetch( sWASeg,
                                     aHeader->mRowBuffer4Fetch,
                                     &sTRPInfo )
                              != IDE_SUCCESS );

                    if ( compareRowAndValue( aHeader,
                                             sTRPInfo.mValuePtr,
                                             aValueList ) == ID_TRUE )
                    {
                        /* ã�Ҵ�, �� �浹�� �߻� �Ͽ���. */
                        *aResult = ID_FALSE;

                        /* for Update������ �̹� Ž�� �ϰ� ���Դ�, �浹�� �߻��ϸ� �ȵȴ�. */
                        IDE_DASSERT( sWASeg->mOpenCursorType != SMI_HASH_CURSOR_HASH_UPDATE );
                        IDE_CONT( CONT_COMPLETE );
                    }
                }

                sPrvTRPHeader = sTRPHeader;
                sTRPHeader    = sPrvTRPHeader->mChildRowPtr ;

                /* Next Offset�� Single Row���� Ȯ��
                 * Single Row ���δ� Prev Row�� Offset���� Ȯ���Ѵ�.
                 * Single Row�� Child Row�� hash value�̴�. */
                if ( SDT_IS_SINGLE_ROW( sPrvTRPHeader->mChildOffset ) )
                {
                    break;
                }

            } while( sTRPHeader != NULL );

            if ( SDT_IS_SINGLE_ROW( sPrvTRPHeader->mChildOffset ) )
            {
                /* in memory row�� �������� ����� single row�� hash value���� ��ġ�Ѵ�.
                 * Disk �� ���� �Ǿ� �ִ� row�� Ȯ�� �� ���� �Ѵ�.*/
                if ( (UInt)((ULong)sPrvTRPHeader->mChildRowPtr) == aHashValue )
                {
                    sSingleRowPageID = sPrvTRPHeader->mChildPageID;
                    sSingleRowOffset = SDT_DEL_SINGLE_FLAG_IN_OFFSET( sPrvTRPHeader->mChildOffset );

                    IDE_DASSERT( sSingleRowPageID != SD_NULL_PID );
                }
                else
                {
                    /* Hash value�� ��ġ ���� �ʴ´�. ã�� ���ߴ�.*/
                }
            }
            else
            {
                /* Single row�� �ƴϴ�. �����Ǵ� ���� sub hash �̰ų� null pid �̴�. */
                sSubHashPageID = sPrvTRPHeader->mChildPageID;
                sSubHashOffset = sPrvTRPHeader->mChildOffset;
            }
        }
    }
    else
    {
        if ( (UInt)aHashSlot->mRowPtrOrHashSet == aHashValue )
        {
            /* Single Row �ϳ� �ִ� ����̸�, hash value�� ��ġ�Ѵ�.
             * �о Ȯ���� ���� �Ѵ�. */
            sSingleRowPageID = aHashSlot->mPageID;
            sSingleRowOffset = SDT_DEL_SINGLE_FLAG_IN_OFFSET( aHashSlot->mOffset );

            IDE_DASSERT( sSingleRowPageID != SD_NULL_PID );
        }
        else
        {
            /* Single Row�̳� hash value�� ��ġ���� �ʴ´�.*/
        }
    }

    /* ���� ã�� ���ߴ�. Insert �� ���� �ִ�. */
    *aResult = ID_TRUE;

    if ( sSingleRowPageID != SM_NULL_PID )
    {
        IDE_TEST( chkUniqueInsertableSingleRow( aHeader,
                                                sWASeg,
                                                aValueList,
                                                sSingleRowPageID,
                                                sSingleRowOffset,
                                                aResult ) != IDE_SUCCESS );

        IDE_CONT( CONT_COMPLETE );
    }

    if ( sSubHashPageID != SD_NULL_PID )
    {
        IDE_TEST( chkUniqueInsertableSubHash( aHeader,
                                              sWASeg,
                                              aValueList,
                                              aHashValue,
                                              sSubHashPageID,
                                              sSubHashOffset,
                                              aResult ) != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( CONT_COMPLETE );

    aHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : unique violation Ȯ�� single row�� �� Ȯ��
 *               Hash Value���� Ȯ���ϰ� ���´�.
 *
 * aHeader          - [IN] ��� Temp Table Header
 * aWASegment       - [IN] Hash Segment
 * aValueList       - [IN] �ߺ� Ű ���� �� Value
 * aSingleRowPageID - [IN] Ȯ�� �� Row�� page ID
 * aSingleRowOffset - [IN] Ȯ�� �� Row�� Offset
 * aResult          - [OUT] insert�����ϸ� IDE_TRUE, �浹�� ������ ID_FALSE
 ***************************************************************************/
IDE_RC sdtHashModule::chkUniqueInsertableSingleRow( smiTempTableHeader  * aHeader,
                                                    sdtHashSegHdr       * aWASegment,
                                                    const smiValue      * aValueList,
                                                    scPageID              aSingleRowPageID,
                                                    scOffset              aSingleRowOffset,
                                                    idBool              * aResult )
{
    sdtHashScanInfo  sTRPInfo;
    sdtWCB         * sWCBPtr;
    idBool           sIsFixedPage = ID_FALSE;

    sTRPInfo.mFetchEndOffset = aHeader->mKeyEndOffset;

    IDE_TEST( getSlotPtr( aWASegment,
                          &aWASegment->mFetchGrp,
                          aSingleRowPageID,
                          aSingleRowOffset,
                          (UChar**)&sTRPInfo.mTRPHeader,
                          &sWCBPtr )
              != IDE_SUCCESS );

    IDE_DASSERT( (sTRPInfo.mTRPHeader)->mIsRow == SDT_HASH_ROW_HEADER );
    IDE_DASSERT( SM_IS_FLAG_ON( (sTRPInfo.mTRPHeader)->mTRFlag, SDT_TRFLAG_CHILDGRID ) );
    IDE_DASSERT( SM_IS_FLAG_ON( (sTRPInfo.mTRPHeader)->mTRFlag, SDT_TRFLAG_HEAD ) );
    IDE_DASSERT( (sTRPInfo.mTRPHeader)->mChildPageID == SM_NULL_PID );

    fixWAPage( sWCBPtr );
    sIsFixedPage = ID_TRUE;

    IDE_TEST( fetch( aWASegment,
                     aHeader->mRowBuffer4Fetch,
                     &sTRPInfo )
              != IDE_SUCCESS );

    sIsFixedPage = ID_FALSE;
    unfixWAPage( sWCBPtr );

    if ( compareRowAndValue( aHeader,
                             sTRPInfo.mValuePtr,
                             aValueList ) == ID_TRUE )
    {
        /* ã�Ҵ�!, Unique Insert �� �� ����. */
        IDE_DASSERT( aWASegment->mOpenCursorType != SMI_HASH_CURSOR_HASH_UPDATE );
        *aResult = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        unfixWAPage( sWCBPtr );
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : unique violation Ȯ�� single row�� �� Ȯ��
 *               Hash Value���� Ȯ���ϰ� ���´�.
 *
 * aHeader        - [IN] ��� Temp Table Header
 * aWASegment     - [IN] Hash Segment
 * aValueList     - [IN] �ߺ� Ű ���� �� Value
 * aHashValue     - [IN] �ߺ� Ű ���� �� HashValue
 * aSubHashPageID - [IN] Ȯ�� �� Sub Hash�� page ID
 * aSubHashOffset - [IN] Ȯ�� �� Sub Hash�� Offset
 * aResult        - [OUT] insert�����ϸ� IDE_TRUE, �浹�� ������ ID_FALSE
 ***************************************************************************/
IDE_RC sdtHashModule::chkUniqueInsertableSubHash( smiTempTableHeader  * aHeader,
                                                  sdtHashSegHdr       * aWASegment,
                                                  const smiValue      * aValueList,
                                                  UInt                  aHashValue,
                                                  scPageID              aSubHashPageID,
                                                  scOffset              aSubHashOffset,
                                                  idBool              * aResult )
{
    UInt             i;
    UInt             sLoop = 0;
    UShort           sHashHigh;
    sdtSubHash     * sSubHash;
    sdtHashScanInfo  sTRPInfo;
    sdtWCB         * sWCBPtr;
    idBool           sIsFixedPage = ID_FALSE;

    sTRPInfo.mFetchEndOffset = aHeader->mKeyEndOffset;

    /* Sub Hash���� ���� 2Byte������ ���� �Ǿ� �ִ�.
     * ���� 2Byte���������� �� �Ѵ�.*/
    sHashHigh = SDT_MAKE_HASH_HIGH( aHashValue );

    do
    {
        sLoop++;

        /* Read Sub hash*/
        IDE_TEST( getSlotPtr( aWASegment,
                              &aWASegment->mFetchGrp,
                              aSubHashPageID,
                              aSubHashOffset,
                              (UChar**)&sSubHash,
                              &sWCBPtr )
                  != IDE_SUCCESS );

        IDE_DASSERT( ((sdtHashTRPHdr*)sSubHash)->mIsRow != SDT_HASH_ROW_HEADER );
        IDE_DASSERT( sSubHash->mHashLow == SDT_MAKE_HASH_LOW( aHashValue ) );

        for( i = 0 ; i < sSubHash->mKeyCount ; i++ )
        {
            if ( sSubHash->mKey[i].mHashHigh == sHashHigh )
            {
                sLoop++;
                /* Read Row */
                IDE_TEST( getSlotPtr( aWASegment,
                                      &aWASegment->mFetchGrp,
                                      sSubHash->mKey[i].mPageID ,
                                      sSubHash->mKey[i].mOffset ,
                                      (UChar**)&sTRPInfo.mTRPHeader,
                                      &sWCBPtr )
                          != IDE_SUCCESS );

                IDE_DASSERT( SM_IS_FLAG_ON( sTRPInfo.mTRPHeader->mTRFlag, SDT_TRFLAG_CHILDGRID ) );
                IDE_DASSERT( SM_IS_FLAG_ON( sTRPInfo.mTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );
                IDE_DASSERT( sTRPInfo.mTRPHeader->mIsRow == SDT_HASH_ROW_HEADER );
                IDE_DASSERT( sTRPInfo.mTRPHeader->mHashValue == aHashValue );

                fixWAPage( sWCBPtr );
                sIsFixedPage = ID_TRUE;

                IDE_TEST( fetch( aWASegment,
                                 aHeader->mRowBuffer4Fetch,
                                 &sTRPInfo )
                          != IDE_SUCCESS );

                sIsFixedPage = ID_FALSE;
                unfixWAPage( sWCBPtr );

                if ( compareRowAndValue( aHeader,
                                         sTRPInfo.mValuePtr,
                                         aValueList ) == ID_TRUE )
                {
                    /* ã�Ҵ�! Insert �� �� ����. */
                    *aResult = ID_FALSE;

                    IDE_DASSERT( aWASegment->mOpenCursorType == SMI_HASH_CURSOR_NONE );
                    IDE_CONT( CONT_COMPLETE );
                }
            }
        }

        IDE_DASSERT( ( i+1 ) >= sSubHash->mKeyCount );

        /* sub hash���� ã�� ���߰ų�, ������ key���� hit�� ��� */
        aSubHashPageID = sSubHash->mNextPageID;
        aSubHashOffset = sSubHash->mNextOffset;
    } while( aSubHashPageID != SM_NULL_PID );

    IDE_EXCEPTION_CONT( CONT_COMPLETE );

    aHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        unfixWAPage( sWCBPtr );
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :Unique �浹 �˻�� Filter
 *
 * aHeader     - [IN] Temp Table Header
 * aRow        - [IN] ��� Row
 * aValueList  - [IN] ���� Filter Data
 ***************************************************************************/
idBool sdtHashModule::compareRowAndValue( smiTempTableHeader * aHeader,
                                          const void         * aRow,
                                          const smiValue     * aValueList )
{
    smiTempColumn * sKeyColumn = NULL;
    smiValueInfo    sValue1;
    smiValueInfo    sValue2;
    UInt            sColumnCount = 0;
    idBool          sCompResult = ID_TRUE;

    sKeyColumn = aHeader->mKeyColumnList;

    IDE_DASSERT( sKeyColumn != NULL );

    while( sKeyColumn != NULL )
    {
        IDE_ASSERT( ++sColumnCount <= aHeader->mColumnCount );
        IDE_ASSERT( sKeyColumn->mIdx < aHeader->mColumnCount );

        /* PROJ-2180 valueForModule
           SMI_OFFSET_USELESS �� ���ϴ� �÷��� mBlankColumn �� ����ؾ� �Ѵ�.
           Compare �Լ����� valueForModule �� ȣ������ �ʰ�
           offset �� ����Ҽ� �ֱ� �����̴�. */
        sValue1.column = &mBlankColumn;
        sValue1.value  = aValueList[ sKeyColumn->mIdx ].value;
        sValue1.length = aValueList[ sKeyColumn->mIdx ].length;
        sValue1.flag   = SMI_OFFSET_USELESS;

        sValue2.column = &aHeader->mColumns[ sKeyColumn->mIdx ].mColumn;
        sValue2.value  = aRow;
        sValue2.length = 0;
        sValue2.flag   = SMI_OFFSET_USE;

        if ( sKeyColumn->mCompare( &sValue1,
                                   &sValue2 ) != 0 )
        {
            sCompResult = ID_FALSE;
            break;
        }
        else
        {
            /* nothing  to do */
        }
        sKeyColumn = sKeyColumn->mNextKeyColumn;
    }

    return sCompResult;
}

/**************************************************************************
 * Description : HintPage�� ������.
 *               HintPage�� unfix�Ǹ� �ȵǱ⿡ �������ѵ�
 *
 * aWASegment    - [IN] ��� WASegment
 * aHintWCBPtr   - [IN] Hint ���� �� WCB
 ***************************************************************************/
void sdtHashModule::setInsertHintWCB( sdtHashSegHdr * aSegHdr,
                                      sdtWCB        * aHintWCBPtr )
{
    if ( aSegHdr->mInsertHintWCBPtr != aHintWCBPtr )
    {
        if ( aSegHdr->mInsertHintWCBPtr != NULL )
        {
            unfixWAPage( aSegHdr->mInsertHintWCBPtr );
        }

        aSegHdr->mInsertHintWCBPtr = aHintWCBPtr;

        if ( aHintWCBPtr != NULL )
        {
            fixWAPage( aHintWCBPtr );
        }
    }
}

/**************************************************************************
 * Description : �ش� WAPage�� Row�� �����Ѵ�.
 *
 * aWASegment      - [IN] ��� WASegment
 * aTRPInfo        - [IN] ������ Row �� ����
 * aTRInsertResult - [OUT] ������ ��� �� ����
 ***************************************************************************/
IDE_RC sdtHashModule::append( sdtHashSegHdr        * aWASegment,
                              sdtHashInsertInfo    * aTRPInfo,
                              sdtHashInsertResult  * aTRInsertResult )
{
    sdtWCB        * sWCBPtr;

    IDE_DASSERT( SDT_HASH_TR_HEADER_SIZE_FULL ==
                 ( IDU_FT_SIZEOF( sdtHashTRPHdr, mTRFlag )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mIsRow )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mHitSequence )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mValueLength )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mHashValue )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mNextOffset )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mNextPageID )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mChildOffset )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mChildPageID )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mChildRowPtr )
                   + IDU_FT_SIZEOF( sdtHashTRPHdr, mNextRowPiecePtr )) );

    IDE_DASSERT( SDT_HASH_TR_HEADER_SIZE_FULL  == ID_SIZEOF( sdtHashTRPHdr ) );

    idlOS::memset( aTRInsertResult, 0, ID_SIZEOF( sdtHashInsertResult ) );

    if ( aWASegment->mInsertHintWCBPtr != NULL )
    {
        sWCBPtr = aWASegment->mInsertHintWCBPtr;
    }
    else
    {
        /* ���� �����̸� page�Ҵ� */
        IDE_TEST( allocNewPage( aWASegment,
                                aTRPInfo,
                                &sWCBPtr )
                  != IDE_SUCCESS );

        /* ���� �����̸�, �ݵ�� �����ؾ� �Ѵ�. */
        IDE_ASSERT( sWCBPtr != NULL );
    }
    IDE_TEST( appendRowPiece( sWCBPtr,
                              aTRPInfo,
                              aTRInsertResult )
              != IDE_SUCCESS );

    while( aTRInsertResult->mComplete == ID_FALSE  )
    {
        IDE_TEST( allocNewPage( aWASegment,
                                aTRPInfo,
                                &sWCBPtr )
                  != IDE_SUCCESS );

        IDE_ASSERT( sWCBPtr != NULL );

        IDE_TEST( appendRowPiece( sWCBPtr,
                                  aTRPInfo,
                                  aTRInsertResult )
                  != IDE_SUCCESS );
    }

    setInsertHintWCB( aWASegment, sWCBPtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    dumpTempTRPInfo4Insert,
                                    aTRPInfo );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : ������ �� �������� �Ҵ����
 *
 * aWASegment  - [IN] Hash WASegment
 * aTRPInfo    - [IN] Insert ����
 * aNewWCBPtr  - [OUT] ���� �Ҵ� ���� page�� WCB
 ***************************************************************************/
IDE_RC sdtHashModule::allocNewPage( sdtHashSegHdr    * aWASegment,
                                    sdtHashInsertInfo* aTRPInfo,
                                    sdtWCB          ** aNewWCBPtr )
{
    sdtWCB     * sTargetWCBPtr;

    if (( aWASegment->mInsertGrp.mReuseSeqWPID % SDT_WAEXTENT_PAGECOUNT ) == 0 )
    {
        /* Extent ������ flush �ϰų�, Discard ���θ� Ȯ���Ѵ�. */
        IDE_TEST( checkExtentTerm4Insert( aWASegment,
                                          aTRPInfo ) != IDE_SUCCESS );
    }

    sTargetWCBPtr = getWCBPtr( aWASegment,
                               aWASegment->mInsertGrp.mReuseSeqWPID );

    aWASegment->mInsertGrp.mReuseSeqWPID++;

    IDE_ASSERT( SDT_IS_NOT_FIXED_PAGE( sTargetWCBPtr ) );

    /* update �߿� dirty page �� �߻� �� �� �ִ�.
     * ���� ���������� page�� dirty�̸� Write�Ѵ�. */
    if ( sTargetWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
    {
        IDE_TEST( writeNPage( aWASegment,
                              sTargetWCBPtr ) != IDE_SUCCESS );
    }

    /* NPage Hash�� �����ϰ�, �̹� ��� �� flush�� ������ ������
     * NPage Hash���� ������ �ش�.*/
    if (( aWASegment->mNPageHashPtr != NULL ) &&
        ( sTargetWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN ))
    {
        IDE_TEST( unassignNPage( aWASegment,
                                 sTargetWCBPtr )
                  != IDE_SUCCESS );
    }

    initWCB( sTargetWCBPtr );

    /* NPage�� �Ҵ��ؼ� Binding���Ѿ� �� */
    IDE_TEST( allocAndAssignNPage( aWASegment,
                                   &aWASegment->mNExtFstPIDList4Row,
                                   sTargetWCBPtr )
              != IDE_SUCCESS );

    /* Row�� ���� NPage�� Counting */
    aWASegment->mNPageCount++;

    initTempPage( (sdtHashPageHdr*)sTargetWCBPtr->mWAPagePtr,
                  SDT_TEMP_PAGETYPE_HASHROWS,
                  sTargetWCBPtr->mNPageID );/* Next */

    (*aNewWCBPtr) = sTargetWCBPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Dirty �� page�� Write�Ѵ�.
 *
 * aWASegment - [IN] Hash WASegment
 * aWCBPtr    - [IN] Dirty page�� WCB
 ***************************************************************************/
IDE_RC sdtHashModule::writeNPage( sdtHashSegHdr* aWASegment,
                                  sdtWCB       * aWCBPtr )
{
    sdtWCB    * sPrvWCBPtr ;
    sdtWCB    * sCurWCBPtr;
    sdtWCB    * sEndWCBPtr;
    UInt        sPageCount = 1 ;
    
    /* dirty�� �� �� flush queue�� ����.
     * ������ �ƴ϶� Write�ϸ� �ȴ�.*/
    IDE_DASSERT( aWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY );

    /* ù��° ��ġ�� ������ self PID */
    IDE_ASSERT_MSG( aWCBPtr->mNPageID == ((sdtHashPageHdr*)aWCBPtr->mWAPagePtr)->mSelfNPID,
                    "%"ID_UINT32_FMT" != %"ID_UINT32_FMT"\n",
                    aWCBPtr->mNPageID,
                    ((sdtHashPageHdr*)aWCBPtr->mWAPagePtr)->mSelfNPID );

    sEndWCBPtr = getWCBPtr( aWASegment,
                            aWASegment->mEndWPID - 1 );

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
                 "Hash Temp WriteNPage Error\n"
                 "write page count : %"ID_UINT32_FMT"\n",
                 sPageCount );

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    sdtHashModule::dumpWCB,
                                    aWCBPtr );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : RowPiece�ϳ��� �� �������� �����Ѵ�.
 *               Append���� ����Ѵ�.
 *
 * aWCBPtr         - [IN] ������ ��� Page�� WCB
 * aTRPInfo        - [IN] ������ Row Info
 * aTRInsertResult - [OUT] ������ ���
 ***************************************************************************/
IDE_RC sdtHashModule::appendRowPiece( sdtWCB              * aWCBPtr,
                                      sdtHashInsertInfo   * aTRPInfo,
                                      sdtHashInsertResult * aTRInsertResult )
{
    UInt           sRowPieceSize;
    const UInt     sRowPieceHeaderSize = SDT_HASH_TR_HEADER_SIZE_FULL ;
    UInt           sSlotSize;
    UChar        * sSlotPtr;
    UChar        * sRowPtr;
    UInt           sBeginOffset;
    UChar        * sWAPagePtr;
    sdtHashTRPHdr * sTRPHeader;

    IDE_ERROR( aWCBPtr != NULL );
    IDE_DASSERT( sRowPieceHeaderSize == ID_SIZEOF(sdtHashTRPHdr));

    sWAPagePtr = aWCBPtr->mWAPagePtr;

    /* FreeSpace��� */
    sRowPieceSize = sRowPieceHeaderSize + aTRPInfo->mValueLength;

    sSlotSize = allocSlot( (sdtHashPageHdr*)sWAPagePtr,
                           sRowPieceSize,
                           &sSlotPtr );

    /* Slot �Ҵ� ���� */
    IDE_TEST_CONT( sSlotSize == 0, SKIP );

    sTRPHeader = (sdtHashTRPHdr*)sSlotPtr;
    aTRInsertResult->mHeadRowpiecePtr = sTRPHeader;

    /* Row Piece Header�� ���� ���� */
    idlOS::memcpy( sTRPHeader,
                   &aTRPInfo->mTRPHeader,
                   sRowPieceHeaderSize );

    aTRInsertResult->mHeadPageID = aWCBPtr->mNPageID;
    aTRInsertResult->mHeadOffset = sSlotPtr - sWAPagePtr;

    if ( sSlotSize == sRowPieceSize )
    {
        /* Cutting�� �͵� ����, �׳� ���� Copy�ϸ� �� */

        /* �������� ��ϵǴ� ���� Row Header�̴�. */
        SM_SET_FLAG_ON( sTRPHeader->mTRFlag,
                         SDT_TRFLAG_HEAD );

        sTRPHeader->mValueLength = aTRPInfo->mValueLength;

        /* ���� Row�� ���� ��ġ */
        sRowPtr = ((UChar*)aTRPInfo->mValueList[ 0 ].value)
            - aTRPInfo->mColumns[ 0 ].mColumn.offset;

        /* ���� �Ϸ��� */
        aTRInsertResult->mComplete = ID_TRUE;
    }
    else
    {
        IDE_DASSERT( sRowPieceSize > sSlotSize );
        sBeginOffset = sRowPieceSize - sSlotSize; /* �䱸���� ������ ��ŭ
                                                   * ����ϴ� OffsetRange�� ����� */
        IDE_ERROR( aTRPInfo->mValueLength > sBeginOffset );
        sTRPHeader->mValueLength = aTRPInfo->mValueLength - sBeginOffset;

        /* ���� ���� �κ��� ���ƴ� ������,
         * ������ TRPHeader���� NextGRID�� ���̰� (������ �����϶��) */
        SM_SET_FLAG_ON( aTRPInfo->mTRPHeader.mTRFlag,
                        SDT_TRFLAG_NEXTGRID );

        /* ���� Row�� ���� ��ġ */
        sRowPtr = ((UChar*)aTRPInfo->mValueList[ 0 ].value)
            - aTRPInfo->mColumns[ 0 ].mColumn.offset + sBeginOffset;

        aTRPInfo->mValueLength -= sTRPHeader->mValueLength;

        aTRPInfo->mTRPHeader.mNextPageID = aTRInsertResult->mHeadPageID;
        aTRPInfo->mTRPHeader.mNextOffset = aTRInsertResult->mHeadOffset;
        aTRPInfo->mTRPHeader.mNextRowPiecePtr = sTRPHeader;
    }

    /* Row Value�� ���� */
    idlOS::memcpy( sSlotPtr + sRowPieceHeaderSize,
                   sRowPtr,
                   sTRPHeader->mValueLength );

    IDE_DASSERT( aWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );
    aWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Page���� Slot�� �ϳ� �Ҵ�޾� �´�.
 *               MinSize < Return Size < NeedSize
 *               Min�� Need���̿� �ִ��� ū ���� ��ȯ�Ѵ�.
 *               Min���� ������ �Ҵ� ����,
 *
 * aPageHdr   - [IN] ������ ��� Page
 * aNeedSize  - [IN] �ʿ��� ũ��
 * aRetSize   - [OUT] �Ҵ� ���� ũ��
 ***************************************************************************/
UInt sdtHashModule::allocSlot( sdtHashPageHdr  * aPageHdr,
                               UInt              aNeedSize,
                               UChar          ** aSlotPtr )
{
    UInt sBeginFreeOffset = getFreeOffset( aPageHdr );
    UInt sFreeSize        = ( SD_PAGE_SIZE - sBeginFreeOffset );
    UInt sSlotSize;

    /* Align�� ���� Padding ������ sRetOffset�� ���� �˳��ϰ� �� ��
     * ����. ���� Min���� �ٿ��� */
    if ( sFreeSize > aNeedSize )
    {
        sFreeSize = idlOS::align8( aNeedSize );

        IDE_DASSERT( ( sFreeSize & 0x07 ) == 0  ) ;
        IDE_DASSERT( sFreeSize >= aNeedSize ) ;

        setFreeOffset( aPageHdr,
                       sBeginFreeOffset + sFreeSize );

        *aSlotPtr = (UChar*)aPageHdr + sBeginFreeOffset;

        sSlotSize = aNeedSize;
    }
    else
    {
        if ( sFreeSize >= smuProperty::getTempRowSplitThreshold() )
        {
            setFreeOffset( aPageHdr,
                           sBeginFreeOffset + sFreeSize );

            *aSlotPtr = (UChar*)aPageHdr + sBeginFreeOffset;

            sSlotSize = sFreeSize;
        }
        else
        {
            sSlotSize = 0;
        }
    }

    return sSlotSize;
}

/**************************************************************************
 * Description : ���ο� WA Extent�� �Ҵ� ���� ���Ͽ��� �� �ʿ��� ������ ��Ƶ�
 *               1. OutMemory �� ����
 *               2. sub hash�� build�ϰ�, TRPInfo �� ����,
 *               TOTAL WA�� ������ ���� �Ϲ������� �� �߻����� �����Ƿ�
 *               ������ ���� �� �ξ���.
 *
 * aWASegment    - [IN] ��� WASegment
 * aTRPInfo      - [IN] ���� insert���� row�� ����
 ***************************************************************************/
IDE_RC sdtHashModule::lackTotalWAPage( sdtHashSegHdr    * aWASegment,
                                       sdtHashInsertInfo* aTRPInfo )
{
    IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_WORKAREA );

    IDE_ASSERT( aWASegment->mInsertGrp.mReuseSeqWPID >=
                ( aWASegment->mInsertGrp.mBeginWPID + SDT_WAEXTENT_PAGECOUNT ) );

    if ( aWASegment->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        if (( aWASegment->mNPageHashPtr == NULL ) &&
            ( aWASegment->mUnique == SDT_HASH_UNIQUE ))
        {
            /* Unique insert�̰ų� scan�� insert�� �Բ� �ϴ� ��쿡��(�̰͵� unique)
             * ������� ���� hash map�� �����.�ݴ��, unique�� �ƴ� normal insert �� ��쿡��
             * insert�Ϸ� �� �� ���� scan�� ����. */
            IDE_TEST( buildNPageHashMap( aWASegment ) != IDE_SUCCESS );
        }
    }

    /* �� �Ҵ� ���� ���ؼ� ù page�� ���ư��� ���� sub hash�� ���� �Ѵ�.
     * Sub Hash�� ������ page���� ��� ��� �� �ڿ� ���� �ؾ� �Ѵ�. */
    IDE_TEST( buildSubHashAndResetInsertInfo( aWASegment,
                                              aTRPInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : ���ο� insert�� page�� �Ҵ� �ϴ� �� Extent������ ����
 * extent������ page�� �Ҵ��ϰų�, extent flush �� ���� ������ �ʴ� �͵���
 * ��Ƶξ���.
 *
 * aWASegment  - [IN] ��� WASegment
 * aTRPInfo    - [IN] ���� insert���� row�� ����
 ***************************************************************************/
IDE_RC sdtHashModule::checkExtentTerm4Insert( sdtHashSegHdr    * aWASegment,
                                              sdtHashInsertInfo* aTRPInfo )
{
    sdtWCB * sCurWCBPtr;
    scPageID sCurWPageID = aWASegment->mInsertGrp.mReuseSeqWPID;
    scPageID sEndWPageID = aWASegment->mInsertGrp.mEndWPID;

    if (( sCurWPageID + SDT_WAEXTENT_PAGECOUNT ) >= sEndWPageID )
    {
        if ( aWASegment->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
        {
            /* ������ extent�Ҵ翡 �����ϸ� NPage Hash�� ����� */
            if (( aWASegment->mNPageHashPtr == NULL ) &&
                ( aWASegment->mUnique == SDT_HASH_UNIQUE ))
            {
                /* Unique insert�̰ų� scan�� insert�� �Բ� �ϴ� ��쿡��(�̰͵� unique)
                 * ������� ���� hash map�� �����.�ݴ��, unique�� �ƴ� normal insert �� ��쿡��
                 * insert�Ϸ� �� �� ���� scan�� ����. */
                IDE_TEST( buildNPageHashMap( aWASegment ) != IDE_SUCCESS );
            }
        }
    }

    if ( sCurWPageID >= sEndWPageID )
    {
        /* insert group�� ��� ����Ͽ���.
         * sub hash�� �����ϰ�, ����� hash slot�� ���� TRPInfo�� �����Ѵ�.*/
        IDE_TEST( buildSubHashAndResetInsertInfo( aWASegment,
                                                  aTRPInfo )
                  != IDE_SUCCESS );
    }

    if ( sCurWPageID != sEndWPageID )
    {
        sCurWCBPtr = getWCBPtr( aWASegment,
                                sCurWPageID );

        /* InMemory���� Ȯ������ �ʰ� Ȯ���� ����.
         * TOTAL WA�� �����ؼ� �Ҵ� ���� ������ ���, ���⿡�� �ٽ� Ȯ�� �Ҵ� �޴´�.*/
        if ( sCurWCBPtr->mWAPagePtr == NULL )
        {
            if ( set64WAPagesPtr( aWASegment,
                                  sCurWCBPtr ) != IDE_SUCCESS )
            {
                /* Insert �߿� Total WA Page�� �����ϸ�,
                 * �ִ� ��ŭ�� ����ؼ� ó���Ѵ�. */
                IDE_TEST( lackTotalWAPage( aWASegment,
                                           aTRPInfo ) != IDE_SUCCESS );
            }
        }
    }
    else
    {
        /* EndWPageID ���� ���� �ߴٸ�, InsertGroup���� ��� page�� �Ҵ� �� ���̴�. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : sub hash�� ����� insert info�� �����Ѵ�.
 *               sub hash�� ����� hash slot�� ���� ����Ǳ� ������
 *               insert info�� ���� �� ��� �Ѵ�.
 *               Unique Insert�� ��쿡�� work area������ ����ȴ�.
 *
 * aWASegment  - [IN] ��� WASegment
 * aTRPInfo    - [IN] ���� insert���� row�� ����
 ***************************************************************************/
IDE_RC sdtHashModule::buildSubHashAndResetInsertInfo( sdtHashSegHdr    * aWASegment,
                                                      sdtHashInsertInfo* aTRPInfo )
{
    UInt         sPageCount;
    sdtHashSlot* sHashSlot;

    if ( aWASegment->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        if ( aWASegment->mUnique == SDT_HASH_UNIQUE )
        {
            IDE_DASSERT( aWASegment->mFetchGrp.mBeginWPID == aWASegment->mFetchGrp.mEndWPID );

            /* Insert�� fetch�� ���ÿ� �̷������, ó�� discard ������ �����Ͽ���.
             * Bfr [ Hash |     Row     | SubHash ] Row������ Fetch������
             * Aft [ Hash | Fetch | Row | SubHash ] ���ݾ����� ���� ����
             * ���� extent�� Ȧ���̸� fetch�ʿ� ���� �� �д�. */
            sPageCount = ( aWASegment->mSubHashGrp.mBeginWPID - aWASegment->mInsertGrp.mBeginWPID ) / 2;
            sPageCount = (( sPageCount +  SDT_WAEXTENT_PAGECOUNT - 1 )
                          / SDT_WAEXTENT_PAGECOUNT
                          * SDT_WAEXTENT_PAGECOUNT ); 

            IDE_ASSERT( sPageCount % SDT_WAEXTENT_PAGECOUNT == 0 );

            aWASegment->mFetchGrp.mBeginWPID = aWASegment->mInsertGrp.mBeginWPID + sPageCount;
            aWASegment->mFetchGrp.mEndWPID   = aWASegment->mSubHashGrp.mBeginWPID;
            aWASegment->mInsertGrp.mEndWPID  = aWASegment->mFetchGrp.mBeginWPID;

            IDE_ASSERT( aWASegment->mInsertGrp.mBeginWPID  < aWASegment->mInsertGrp.mEndWPID );
            IDE_ASSERT( aWASegment->mFetchGrp.mBeginWPID   < aWASegment->mFetchGrp.mEndWPID );
            IDE_ASSERT( aWASegment->mSubHashGrp.mBeginWPID < aWASegment->mSubHashGrp.mEndWPID );
        }
        aWASegment->mIsInMemory = SDT_WORKAREA_OUT_MEMORY;
    }
    aWASegment->mInsertGrp.mReuseSeqWPID = aWASegment->mInsertGrp.mBeginWPID;

    /* �ѹ� ��� ��ȯ�ϸ� sub hash�� build�Ѵ�.
       Sub Hash�� ������ page���� ��� ��� �� �ڿ� ���� �ؾ� �Ѵ�. */
    IDE_TEST( buildSubHash( aWASegment ) != IDE_SUCCESS );

    IDE_TEST( getHashSlotPtr( aWASegment,
                              aTRPInfo->mTRPHeader.mHashValue,
                              (void**)&sHashSlot )
              != IDE_SUCCESS );

    aTRPInfo->mTRPHeader.mChildRowPtr = (sdtHashTRPHdr*)sHashSlot->mRowPtrOrHashSet;
    aTRPInfo->mTRPHeader.mChildPageID = sHashSlot->mPageID;
    aTRPInfo->mTRPHeader.mChildOffset = sHashSlot->mOffset;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : sub hash�� �����.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::buildSubHash( sdtHashSegHdr * aWASegment )
{
    UInt            i;
    UInt            sLoop = 0;
    UInt            sSlotMaxCount;
    sdtWCB        * sWCBPtr;
    sdtWCB        * sEndWCBPtr;
    sdtHashSlot   * sHashSlot;
    sdtHashTRPHdr * sRowHdrPtr;
    sdtSubHash    * sSubHash;
    sdtSubHash    * sNextSubHash;
    scPageID        sSubHashPageID;
    scOffset        sSubHashOffset;
#ifdef DEBUG
    sdtHashTRPHdr * sPrvRowHdrPtr;
#endif
    // reset�� ���� �̹� �Ҵ� �Ǿ� �ִ�.
    aWASegment->mSubHashBuildCount++;

    sWCBPtr    = getWCBPtr( aWASegment, 0 );
    sEndWCBPtr = getWCBPtr( aWASegment,
                            aWASegment->mHashSlotPageCount );

    if ( aWASegment->mSubHashWCBPtr == NULL )
    {
        /* ���� �Ҵ� */
        IDE_TEST( getFreeWAPage( aWASegment,
                                 &aWASegment->mSubHashGrp,
                                 &aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );


        IDE_TEST( allocAndAssignNPage( aWASegment,
                                       &aWASegment->mNExtFstPIDList4SubHash,
                                       aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );

        initTempPage( (sdtHashPageHdr*)aWASegment->mSubHashWCBPtr->mWAPagePtr,
                      SDT_TEMP_PAGETYPE_SUBHASH,
                      aWASegment->mSubHashWCBPtr->mNPageID );
    }

    for( ; sWCBPtr < sEndWCBPtr ; sWCBPtr++ )
    {
        if ( sWCBPtr->mWAPagePtr == NULL )
        {
            /* ������ ���� ���� Page */
            continue;
        }
        sHashSlot = (sdtHashSlot*)sWCBPtr->mWAPagePtr;

        for( i = 0 ; i < SD_PAGE_SIZE/ID_SIZEOF(sdtHashSlot) ; i++ )
        {
            if ( sHashSlot[i].mRowPtrOrHashSet == 0 )
            {
                // 0. �̹��ҿ� ���� row�� ����, ChildPageID�� �ִ� �ϴ��� ������ SubHash�̴�.
                continue;
            }

            if ( SDT_IS_SINGLE_ROW( sHashSlot[i].mOffset )  )
            {
                // 1���� row�� �ִ�. �����ҿ� �������.
                continue;
            }

            sRowHdrPtr = (sdtHashTRPHdr*)sHashSlot[i].mRowPtrOrHashSet;

            if (( sRowHdrPtr->mChildRowPtr == NULL ) &&
                ( sRowHdrPtr->mChildPageID == SM_NULL_PID ))
            {
                // �̹� �ҿ� Next �� ���� 1���� Row�� �ִ�.
                sHashSlot[i].mRowPtrOrHashSet = sRowHdrPtr->mHashValue;
                SDT_SET_SINGLE_ROW( sHashSlot[i].mOffset );
                continue;
            }

            sHashSlot[i].mRowPtrOrHashSet = 0;

            // subHashMap�� �����´�.

            IDE_TEST( reserveSubHash( aWASegment,
                                      &aWASegment->mSubHashGrp,
                                      5,
                                      &sSubHash,
                                      &sSlotMaxCount,
                                      &sSubHashPageID,
                                      &sSubHashOffset ) != IDE_SUCCESS );

            IDE_DASSERT( sSlotMaxCount <= SDT_SUBHASH_MAX_KEYCOUNT );

            // sub hash ����

            sSubHash->mKeyCount         = 1;
            sSubHash->mHashLow          = SDT_MAKE_HASH_LOW( sRowHdrPtr->mHashValue );
            sSubHash->mKey[0].mPageID   = sHashSlot[i].mPageID;
            sSubHash->mKey[0].mOffset   = sHashSlot[i].mOffset;
            sSubHash->mKey[0].mHashHigh = SDT_MAKE_HASH_HIGH( sRowHdrPtr->mHashValue );
            sLoop++;

            sHashSlot[i].mPageID = sSubHashPageID;
            sHashSlot[i].mOffset = sSubHashOffset;

            while( sRowHdrPtr->mChildRowPtr != NULL )
            {
                if ( sSubHash->mKeyCount == sSlotMaxCount )
                {
                    addSubHashPageFreeOffset( aWASegment,
                                              ((sSubHash->mKeyCount * ID_SIZEOF(sdtSubHashKey)) +
                                               SDT_SUB_HASH_SIZE));

                    // ���� Ȯ��, ������ ���� page
                    IDE_TEST( reserveSubHash( aWASegment,
                                              &aWASegment->mSubHashGrp,
                                              5,
                                              &sNextSubHash,
                                              &sSlotMaxCount,
                                              &sSubHash->mNextPageID,
                                              &sSubHash->mNextOffset ) != IDE_SUCCESS );

                    IDE_DASSERT( sSlotMaxCount <= SDT_SUBHASH_MAX_KEYCOUNT );

                    sNextSubHash->mHashLow = sSubHash->mHashLow;
                    sSubHash               = sNextSubHash;
                    sSubHash->mKeyCount    = 0;
                }

                if ( SDT_IS_SINGLE_ROW( sRowHdrPtr->mChildOffset ))
                {
                    /* ���� page���� �з��� ���� ���� row�̴�.
                     * chile row ptr ( high_hash 0x0001 )
                     * row�� �ϳ� ���̾ sub hash�� ������ �ʰ� �Ѱ��. */
                    sSubHash->mKey[sSubHash->mKeyCount].mPageID = sRowHdrPtr->mChildPageID;
                    sSubHash->mKey[sSubHash->mKeyCount].mOffset =
                        SDT_DEL_SINGLE_FLAG_IN_OFFSET( sRowHdrPtr->mChildOffset );
                    sSubHash->mKey[sSubHash->mKeyCount].mHashHigh =
                        SDT_MAKE_HASH_HIGH( (ULong)sRowHdrPtr->mChildRowPtr );

                    IDE_DASSERT( sSubHash->mKeyCount < SDT_SUBHASH_MAX_KEYCOUNT );

                    sSubHash->mKeyCount++;
                    sLoop++;

                    break;
                }
                else
                {
                    sSubHash->mKey[sSubHash->mKeyCount].mPageID = sRowHdrPtr->mChildPageID;
                    sSubHash->mKey[sSubHash->mKeyCount].mOffset = sRowHdrPtr->mChildOffset;
                }
#ifdef DEBUG
                sPrvRowHdrPtr = sRowHdrPtr;
#endif
                sRowHdrPtr    = sRowHdrPtr->mChildRowPtr;

                IDE_DASSERT( ( sPrvRowHdrPtr->mHashValue & 0x0000FFFF ) == ( sRowHdrPtr->mHashValue & 0x0000FFFF ));
                IDE_DASSERT( sSubHash->mHashLow == SDT_MAKE_HASH_LOW( sRowHdrPtr->mHashValue ) );

                sSubHash->mKey[sSubHash->mKeyCount].mHashHigh = SDT_MAKE_HASH_HIGH( sRowHdrPtr->mHashValue );

                IDE_DASSERT( sSubHash->mKeyCount < SDT_SUBHASH_MAX_KEYCOUNT );
                sSubHash->mKeyCount++;
                sLoop++;
            }

            addSubHashPageFreeOffset( aWASegment,
                                      ((sSubHash->mKeyCount * ID_SIZEOF(sdtSubHashKey)) +
                                       SDT_SUB_HASH_SIZE));

            if (( sRowHdrPtr->mChildPageID != SM_NULL_PID ) &&
                ( SDT_IS_NOT_SINGLE_ROW( sRowHdrPtr->mChildOffset ) ))
            {
                // ChildPointer�� NULL�̰�, ChildPageID�� �ִ�.
                // ���� ���� sub hash map Ȥ�� row�� �ּҴ�
                sSubHash->mNextPageID = sRowHdrPtr->mChildPageID;
                sSubHash->mNextOffset = sRowHdrPtr->mChildOffset ;
            }
            else
            {
                sSubHash->mNextPageID = SM_NULL_PID;
            }
        }
    }

    IDE_DASSERT( aWASegment->mSubHashWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );
    aWASegment->mSubHashWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : sub hash key array�� �Ҵ�޾� �´�.
 *
 * aWASegment       - [IN] ��� WASegment
 * aGroup           - [IN] ���� �� Group
 * aReqMinSlotCount - [IN] �ּ� ��û ũ��
 * aSubHashPtr      - [OUT] ��ȯ SubHashArray�� Pointer
 * aSlotMaxCount    - [OUT] ��ȯ�� Sub Hash Array�� Slot����
 * aPageID          - [OUT] SubHashArray�� ���Ե� PageID
 * aOffset          - [OUT] SubHashArray�� ��ġ Offset
 ***************************************************************************/
IDE_RC sdtHashModule::reserveSubHash( sdtHashSegHdr * aWASegment,
                                      sdtHashGroup  * aGroup,
                                      SInt            aReqMinSlotCount,
                                      sdtSubHash   ** aSubHashPtr,
                                      UInt          * aSlotMaxCount,
                                      scPageID      * aPageID,
                                      scOffset      * aOffset  )
{
    sdtHashPageHdr * sPageHdr;
    SInt             sSlotMaxCount;
    sdtWCB         * sPrvWCBPtr;
    idBool           sIsFixedPage = ID_FALSE;

    sPageHdr = (sdtHashPageHdr*)aWASegment->mSubHashWCBPtr->mWAPagePtr;

    IDE_ASSERT( sPageHdr->mFreeOffset <= SD_PAGE_SIZE );

    if ( aReqMinSlotCount > SDT_ALLOC_SUBHASH_MIN_KEYCOUNT )
    {
        aReqMinSlotCount = SDT_ALLOC_SUBHASH_MIN_KEYCOUNT;
    }

    sSlotMaxCount = ( SD_PAGE_SIZE - sPageHdr->mFreeOffset - SDT_SUB_HASH_SIZE )/ID_SIZEOF(sdtSubHashKey);


    if ( sSlotMaxCount < aReqMinSlotCount )
    {
        sPrvWCBPtr = aWASegment->mSubHashWCBPtr;
        fixWAPage( sPrvWCBPtr );
        sIsFixedPage = ID_TRUE;

        IDE_TEST( getFreeWAPage( aWASegment,
                                 aGroup,
                                 &aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );

        IDE_TEST( allocAndAssignNPage( aWASegment,
                                       &aWASegment->mNExtFstPIDList4SubHash,
                                       aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );

        sPageHdr = (sdtHashPageHdr*)aWASegment->mSubHashWCBPtr->mWAPagePtr;
        initTempPage( sPageHdr,
                      SDT_TEMP_PAGETYPE_SUBHASH,
                      aWASegment->mSubHashWCBPtr->mNPageID );

        *aSlotMaxCount = SDT_SUBHASH_MAX_KEYCOUNT;

        /* �� ������ aPageID, aOffset�� sPrvPage�ȿ� ��ġ�ϰ� �ִ�.
         * �ݵ�� aPageID, aOffset ��� �Ŀ� sPrevWCBPtr�� dirty, write �ؾ� �Ѵ�. */
        *aPageID = sPageHdr->mSelfNPID;
        *aOffset = sPageHdr->mFreeOffset ;

        sIsFixedPage = ID_FALSE;
        unfixWAPage( sPrvWCBPtr );

        IDE_DASSERT( sPrvWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );
        sPrvWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
    }
    else
    {
        *aPageID = sPageHdr->mSelfNPID;
        *aOffset = sPageHdr->mFreeOffset ;

        if ( sSlotMaxCount > SDT_SUBHASH_MAX_KEYCOUNT )
        {
            *aSlotMaxCount = SDT_SUBHASH_MAX_KEYCOUNT;
        }
        else
        {
            *aSlotMaxCount = sSlotMaxCount;
        }
    }
    IDE_ASSERT( aWASegment->mSubHashWCBPtr->mNPageID == *aPageID );
    IDE_ASSERT( sPageHdr->mFreeOffset < SD_PAGE_SIZE );
    IDE_ASSERT( *aSlotMaxCount <= SDT_SUBHASH_MAX_KEYCOUNT );
    IDE_ASSERT(( sPageHdr->mFreeOffset + SDT_SUB_HASH_SIZE + (ID_SIZEOF(sdtSubHashKey)*(*aSlotMaxCount)) )
               <= SD_PAGE_SIZE );

    *aSubHashPtr = (sdtSubHash*)((UChar*)sPageHdr + sPageHdr->mFreeOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        unfixWAPage( sPrvWCBPtr );
    }

    return IDE_FAILURE;
}

/****************************************************************************
 * Description : insert�� ��� �Ϸ� �� ������ sub hash�� ��Ƽ� merge�Ѵ�.
 *               hash slot�� hash set�� small hash set���� large hash set����
 *               �籸���Ͽ� ��Ȯ���� ���δ�.
 *
 * aWASegment - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::mergeSubHash( sdtHashSegHdr* aWASegment )
{
    sdtNExtFstPIDList sSubHashNExtent;
    sdtHashSlot     * sHashSlot;
    sdtHashSlot     * sLstHashSlot;
    sdtWCB          * sEndWCBPtr;
    sdtWCB          * sWCBPtr;
    UInt              sSlotIdx;

    IDE_DASSERT( aWASegment->mSubHashWCBPtr != NULL );

    aWASegment->mSubHashWCBPtr = NULL;

    idlOS::memcpy( &sSubHashNExtent,
                   &aWASegment->mNExtFstPIDList4SubHash,
                   ID_SIZEOF(sdtNExtFstPIDList));

    idlOS::memset( &aWASegment->mNExtFstPIDList4SubHash,
                   0,
                   ID_SIZEOF(sdtNExtFstPIDList));

    aWASegment->mNExtFstPIDList4SubHash.mPageSeqInLFE = SDT_WAEXTENT_PAGECOUNT;

    IDE_TEST( getFreeWAPage( aWASegment,
                             &aWASegment->mInsertGrp,
                             &aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );

    IDE_TEST( allocAndAssignNPage( aWASegment,
                                   &aWASegment->mNExtFstPIDList4SubHash,
                                   aWASegment->mSubHashWCBPtr ) != IDE_SUCCESS );

    initTempPage( (sdtHashPageHdr*)aWASegment->mSubHashWCBPtr->mWAPagePtr,
                  SDT_TEMP_PAGETYPE_SUBHASH,
                  aWASegment->mSubHashWCBPtr->mNPageID );

    sWCBPtr    = getWCBPtr( aWASegment, 0 );
    sEndWCBPtr = getWCBPtr( aWASegment,
                            aWASegment->mHashSlotPageCount );
    sSlotIdx = 0;

    for( ; sWCBPtr < sEndWCBPtr ; sWCBPtr++ )
    {
        if ( sWCBPtr->mWAPagePtr == NULL )
        {
            sSlotIdx += SD_PAGE_SIZE/ID_SIZEOF(sdtHashSlot);
            continue;
        }
        sHashSlot    = (sdtHashSlot*)sWCBPtr->mWAPagePtr;
        sLstHashSlot = (sdtHashSlot*)(sWCBPtr->mWAPagePtr + SD_PAGE_SIZE);

        for( ; sHashSlot < sLstHashSlot ; sHashSlot++,sSlotIdx++ )
        {
            if ( sHashSlot->mPageID == SM_NULL_PID )
            {
                // �ƹ��͵� ����
                continue;
            }

            if ( SDT_IS_SINGLE_ROW( sHashSlot->mOffset ) )
            {
                // 1���� row�� �ִ�. �����ҿ� �������.
                continue;
            }

            IDE_TEST( mergeSubHashOneSlot( aWASegment,
                                           sHashSlot,
                                           sSlotIdx ) != IDE_SUCCESS );
        }
    }

    IDE_DASSERT( aWASegment->mSubHashWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );
    aWASegment->mSubHashWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;

    // �����Ѵ�. // ���������� ������ sub hash slot�̴�. ��� ������.
    sWCBPtr    = getWCBPtr( aWASegment,
                            aWASegment->mSubHashGrp.mBeginWPID );
    sEndWCBPtr = getWCBPtr( aWASegment,
                            aWASegment->mSubHashGrp.mEndWPID - 1 );
    for( ; sWCBPtr <= sEndWCBPtr ; sWCBPtr++ )
    {
        if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT )
        {
            /* ���ķ� ��� INIT�̴�. */
            IDE_DASSERT( sWCBPtr->mNPageID == SD_NULL_PID );
            break;
        }
                
        switch ( sWCBPtr->mWPState )
        {
            case SDT_WA_PAGESTATE_DIRTY:
                /* unassignNPage()�� Clean �� �޾Ƶ��δ�.
                 * Dirty�̸� Clean���� state�� �����Ѵ�. */
                sWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;
            case SDT_WA_PAGESTATE_CLEAN:
                IDE_TEST( unassignNPage( aWASegment,
                                         sWCBPtr )
                          != IDE_SUCCESS );
                initWCB( sWCBPtr );
                break;
            default:
                break;
        }

        IDE_DASSERT( sWCBPtr->mNPageID == SD_NULL_PID );
        IDE_DASSERT( sWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT );
    }
#ifdef DEBUG
    /* ���� ������ ���� �κ��� ��� INIT�̾�� �Ѵ�. */
    for( sWCBPtr++ ; sWCBPtr <= sEndWCBPtr ; sWCBPtr++ )
    {
        IDE_ASSERT( sWCBPtr->mWPState == SDT_WA_PAGESTATE_INIT );
    }
#endif

    IDE_TEST( sdtWAExtentMgr::freeAllNExtents( aWASegment->mSpaceID,
                                               &sSubHashNExtent )
              != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : sub hash�� ���� (BUG-48235)
 *
 * aSubHashArray  - [IN] Sub Hash Key Array
 * aLeft          - [IN] quick sort left
 * aRight         - [IN] quick sort Right
 ***************************************************************************/
void sdtHashModule::quickSortSubHash( sdtSubHashKey aSubHashArray[], SInt aLeft, SInt aRight )
{
    SInt      i     = aLeft;
    SInt      j     = aRight;
    UShort    pivot = aSubHashArray[ ( aLeft + aRight ) / 2 ].mHashHigh;
    ULong     temp;

    do
    {
        while ( aSubHashArray[i].mHashHigh < pivot )
        {
            i++;
        }
        while ( aSubHashArray[j].mHashHigh > pivot )
        {
            j--;
        }

        if ( i <= j )
        {
            if( i != j )
            {
                temp = *(ULong*)(aSubHashArray+i);
                *(ULong*)(aSubHashArray+i) = *(ULong*)(aSubHashArray+j);
                *(ULong*)(aSubHashArray+j) = temp;
            }
            i++;
            j--;
        }
    } while ( i <= j );

    /* recursion */
    if ( aLeft < j )
    {
        quickSortSubHash( aSubHashArray, aLeft, j );
    }

    if ( i < aRight )
    {
        quickSortSubHash( aSubHashArray, i, aRight );
    }
}

/****************************************************************************
 * Description : 1���� hash slot�� ����Ǿ��ִ� sub hash ���� merge �Ѵ�.
 *
 * aWASegment - [IN] ��� WASegment
 * aHashSlot  - [IN] ��� HashSlot
 * aSlotIdx   - [IN] ��� Hash Slot�� Index
 ***************************************************************************/
IDE_RC sdtHashModule::mergeSubHashOneSlot( sdtHashSegHdr* aWASegment,
                                           sdtHashSlot  * aHashSlot,
                                           UInt           aSlotIdx )
{
    sdtSubHash      * sNewSubHash;
    sdtSubHash      * sOldSubHash;
    sdtWCB          * sOldSubWCBPtr;
    idBool            sIsFixed = ID_FALSE;
    UInt              sSlotMaxCount;
    SInt              sCopiedKeyCount;
    SInt              sRemainKeyCount;
    UChar             sHashLow;
    UInt              i;

    aHashSlot->mRowPtrOrHashSet = 0;

    IDE_TEST( getSlotPtr( aWASegment,
                          &aWASegment->mSubHashGrp,
                          aHashSlot->mPageID,
                          aHashSlot->mOffset,
                          (UChar**)&sOldSubHash,
                          &sOldSubWCBPtr )
              != IDE_SUCCESS );

    fixWAPage( sOldSubWCBPtr );
    sIsFixed = ID_TRUE;

    sCopiedKeyCount = 0;
    sRemainKeyCount = sOldSubHash->mKeyCount;

    sHashLow = sOldSubHash->mHashLow;

    IDE_ASSERT_MSG( SDT_MAKE_HASH_LOW( aSlotIdx ) == sHashLow,
                    "%"ID_UINT32_FMT" %"ID_UINT32_FMT" %"ID_UINT32_FMT", "
                    "%"ID_UINT32_FMT" %"ID_UINT32_FMT" %"ID_UINT32_FMT"\n",     
                    aSlotIdx,
                    SDT_MAKE_HASH_LOW( aSlotIdx ),
                    sHashLow,
                    sOldSubHash->mKeyCount,
                    sOldSubHash->mNextPageID,
                    sOldSubHash->mNextOffset );

    IDE_TEST( reserveSubHash( aWASegment,
                              &aWASegment->mInsertGrp,
                              20,
                              &sNewSubHash,
                              &sSlotMaxCount,
                              &aHashSlot->mPageID,
                              &aHashSlot->mOffset ) != IDE_SUCCESS );

    sNewSubHash->mHashLow  = sHashLow;
    sNewSubHash->mKeyCount = 0;

    while(1)
    {
        while( sRemainKeyCount > 0 )
        {
            if ( sRemainKeyCount <= (SInt)sSlotMaxCount )
            {
                idlOS::memcpy( sNewSubHash->mKey + sNewSubHash->mKeyCount,
                               sOldSubHash->mKey + sCopiedKeyCount,
                               ID_SIZEOF( sdtSubHashKey ) * sRemainKeyCount );

                // ���� �� new slot�� �����ִ�.
                sSlotMaxCount          -= sRemainKeyCount ;
                sNewSubHash->mKeyCount += sRemainKeyCount ;
                // sCopiedKeyCount += sRemainKeyCount;
                sRemainKeyCount  = 0;
                break;
            }
            else // ( sRemainKeyCount >= sSlotMaxCount )
            {
                idlOS::memcpy( sNewSubHash->mKey + sNewSubHash->mKeyCount ,
                               sOldSubHash->mKey + sCopiedKeyCount ,
                               ID_SIZEOF( sdtSubHashKey ) * sSlotMaxCount );

                sNewSubHash->mKeyCount += sSlotMaxCount ;
                sCopiedKeyCount        += sSlotMaxCount;
                sRemainKeyCount        -= sSlotMaxCount;
                // new sub hash�� key�� ��� ����Ͽ���.
                // ���� �Ҵ� �޾Ƽ� ä�� �־�� �Ѵ�.

                if (( sRemainKeyCount > 0 ) ||
                    ( sOldSubHash->mNextPageID != SM_NULL_PID ))
                {
                    addSubHashPageFreeOffset( aWASegment,
                                              ((sNewSubHash->mKeyCount * ID_SIZEOF(sdtSubHashKey)) +
                                               SDT_SUB_HASH_SIZE));

                    for( i = 0 ; i < sNewSubHash->mKeyCount ; i++ )
                    {
                        SDT_ADD_LARGE_HASHSET( aHashSlot, sNewSubHash->mKey[i].mHashHigh );
                    }
                    quickSortSubHash( sNewSubHash->mKey, 0, sNewSubHash->mKeyCount -1 );
                    IDE_TEST( reserveSubHash( aWASegment,
                                              &aWASegment->mInsertGrp,
                                              20,
                                              &sNewSubHash,
                                              &sSlotMaxCount,
                                              &sNewSubHash->mNextPageID,
                                              &sNewSubHash->mNextOffset ) != IDE_SUCCESS );

                    sNewSubHash->mKeyCount = 0;
                    sNewSubHash->mHashLow  = sHashLow;
                }
            }
        }

        if ( sOldSubHash->mNextPageID == SM_NULL_PID )
        {
            break;
        }

        // slot�� �ϳ� �о�ͼ� ó��
        sIsFixed = ID_FALSE;
        unfixWAPage( sOldSubWCBPtr );

        IDE_TEST( getSlotPtr( aWASegment,
                              &aWASegment->mSubHashGrp,
                              sOldSubHash->mNextPageID ,
                              sOldSubHash->mNextOffset ,
                              (UChar**)&sOldSubHash,
                              &sOldSubWCBPtr )
                  != IDE_SUCCESS );

        fixWAPage( sOldSubWCBPtr );
        sIsFixed = ID_TRUE;

        IDE_ASSERT_MSG( sOldSubHash->mHashLow == sHashLow,
                        "%"ID_UINT32_FMT" %"ID_UINT32_FMT" %"ID_UINT32_FMT", "
                        "%"ID_UINT32_FMT" %"ID_UINT32_FMT" %"ID_UINT32_FMT" %"ID_UINT32_FMT"\n",
                        aSlotIdx,
                        SDT_MAKE_HASH_LOW( aSlotIdx ),
                        sHashLow,
                        sOldSubHash->mHashLow,
                        sOldSubHash->mKeyCount,
                        sOldSubHash->mNextPageID,
                        sOldSubHash->mNextOffset );

        sCopiedKeyCount = 0;
        sRemainKeyCount = sOldSubHash->mKeyCount;
    }

    for( i = 0 ; i < sNewSubHash->mKeyCount ; i++ )
    {
        SDT_ADD_LARGE_HASHSET( aHashSlot, sNewSubHash->mKey[i].mHashHigh );
    }
    quickSortSubHash( sNewSubHash->mKey, 0, sNewSubHash->mKeyCount -1 );

    IDE_ASSERT( sRemainKeyCount == 0 );
    IDE_ASSERT( sOldSubHash->mNextPageID == SM_NULL_PID );

    sIsFixed = ID_FALSE;
    unfixWAPage( sOldSubWCBPtr );

    addSubHashPageFreeOffset( aWASegment,
                              ((sNewSubHash->mKeyCount * ID_SIZEOF(sdtSubHashKey)) + SDT_SUB_HASH_SIZE));

    sNewSubHash->mNextPageID = SM_NULL_PID;
    sNewSubHash->mNextOffset = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixed == ID_TRUE )
    {
        unfixWAPage( sOldSubWCBPtr );
    }

    return IDE_FAILURE;
}
/***************************************************************************
 * Description : NPageHash�� ������ �ش� NPage�� ���� WPage�� ã�Ƴ�
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::buildNPageHashMap( sdtHashSegHdr* aWASegment )
{
    sdtWCB * sWCBPtr;
    sdtWCB * sEndWCBPtr;
    UInt     sHashValue;
    idBool   sIsLock  = ID_FALSE;

    /* Null �� ������ ȣ���, ������ ���� ȣ�� ���� �����Ƿ� �ѹ� �� �˻��Ѵ�.*/
    if ( aWASegment->mNPageHashPtr == NULL )
    {
        lock();
        sIsLock = ID_TRUE;

        aWASegment->mNPageHashBucketCnt = mNHashMapPool.getDataSize()/ID_SIZEOF(sdtWCB*);

        IDE_TEST( mNHashMapPool.pop( (UChar**)&(aWASegment->mNPageHashPtr) ) != IDE_SUCCESS );
        IDE_DASSERT( aWASegment->mNPageHashPtr != NULL );

        sIsLock = ID_FALSE;
        unlock();

#ifdef DEBUG
        for( UInt i = 0 ; i < aWASegment->mNPageHashBucketCnt ; i++ )
        {
            /* �Ҵ���� NPageHashMap�� ��� Null���� Ȯ�� */
            IDE_ASSERT( aWASegment->mNPageHashPtr[i] == NULL );
        }
#endif
        sWCBPtr    = getWCBPtr( aWASegment, aWASegment->mInsertGrp.mBeginWPID );
        sEndWCBPtr = getWCBPtr( aWASegment, aWASegment->mEndWPID - 1 );

        for( ; sWCBPtr <= sEndWCBPtr ; sWCBPtr++ )
        {
            if ( sWCBPtr->mNPageID != SM_NULL_PID )
            {
                IDE_DASSERT( sWCBPtr->mNextWCB4Hash == NULL );

                sHashValue = getNPageHashValue( aWASegment,
                                                sWCBPtr->mNPageID );

                sWCBPtr->mNextWCB4Hash = aWASegment->mNPageHashPtr[ sHashValue ] ;
#ifdef DEBUG
                if ( sWCBPtr->mNextWCB4Hash  != NULL )
                {
                    /* �ռ� �ִ� WCB�� �ִٸ� �� WCB�� NPageID�� Hash���� ���ƾ� �Ѵ�. */
                    IDE_DASSERT( getNPageHashValue( aWASegment, sWCBPtr->mNextWCB4Hash->mNPageID )
                                 == sHashValue );
                }
#endif
                aWASegment->mNPageHashPtr[ sHashValue ] = sWCBPtr;
            }
        }
        aWASegment->mStatsPtr->mIOPassNo++;
        aWASegment->mStatsPtr->mRuntimeMemSize += mNHashMapPool.getNodeSize( (UChar*)aWASegment->mNPageHashPtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_TRUE )
    {
        unlock();
        sIsLock = ID_FALSE;
    }

    /* ���ܰ� �߻� �Ͽ��ٸ�, �Լ� ���� ���� NULL�̾��� ���̴�.*/
    if ( aWASegment->mNPageHashPtr != NULL )
    {
        /* Push���� ��� �ʱ�ȭ �Ǿ� �־�� �Ѵ�.
         * �������� ��Ȳ�̴� �����ϰ� Memset���� ó�� */
        idlOS::memset( aWASegment->mNPageHashPtr,
                       0,
                       aWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*) );
        IDE_ASSERT( mNHashMapPool.push( (UChar*)(aWASegment->mNPageHashPtr) )
                    != IDE_SUCCESS );
        aWASegment->mNPageHashPtr = NULL;
        aWASegment->mNPageHashBucketCnt = 0;
    }

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : cursor alloc ������ Insert�� ������ �ϴ� �۾��� �Ѵ�.
 *               Insert End�� ���� �����Ƿ�, Fetch ���� ������ �������ش�.
 *               ���� cursor open �ÿ� sub hash �� �籸�� �ϰų�,
 *               Insert �� ���� Dirty Page�� flush�ϰų�,
 *               NPageHashMap�� ���ٸ� �����ϴ� ���ҵ� �Ѵ�.
 *
 * aCursor     - [IN] open�� cursor
 * aCursorType - [IN] open�� cursor�� Type ()
 ***************************************************************************/
IDE_RC sdtHashModule::initHashCursor( smiHashTempCursor  * aCursor,
                                      UInt                 aCursorType )
{
    sdtHashSegHdr * sWASegment = (sdtHashSegHdr*)aCursor->mWASegment;
    sdtWCB        * sWCBPtr;
    sdtWCB        * sEndWCBPtr;

    IDE_DASSERT( aCursorType != SMI_HASH_CURSOR_NONE );

    /* check Hash Slot���� WASegment�� �������� �ʰ� ������ ó�� �ϱ� ����
     * Cursor�� �����صд�. */
    aCursor->mIsInMemory    = sWASegment->mIsInMemory;
    aCursor->mHashSlotCount = sWASegment->mHashSlotCount;

    if (( sWASegment->mIsInMemory == SDT_WORKAREA_OUT_MEMORY ) &&
        ( sWASegment->mNPageHashPtr == NULL ))
    {
        /* Normal Insert �� ������ Insert�� NPageHashMap�� ������ �ʴ´�.
         * Cursor Open������ NPageHashMap�� ����� ����Ѵ�.
         * InMemory ������ NPageHashMap�� ������� �ʴ´�.*/
        IDE_TEST( buildNPageHashMap( sWASegment ) != IDE_SUCCESS );
    }

    if ( aCursorType == SMI_HASH_CURSOR_HASH_UPDATE )
    {
        /* Update Cursor�� Insert, Fetch, Update�� ���ÿ� �̷�����Ƿ� ���������� ��� */
        sWASegment->mOpenCursorType  = SMI_HASH_CURSOR_HASH_UPDATE;
        aCursor->mTTHeader->mTTState = SMI_TTSTATE_HASH_FETCH_UPDATE;
    }

    /* Update�� Cursor�� �ƴϰ�, OutMemory�̰�,
     * ó������ Cursor�� ���� ��Ȳ�̴�. */
    if (( sWASegment->mIsInMemory == SDT_WORKAREA_OUT_MEMORY ) &&
        ( sWASegment->mOpenCursorType == SMI_HASH_CURSOR_NONE ))
    {
        /* Insert�� ������. Hint ���� */
        setInsertHintWCB( sWASegment, NULL );

        /* ������ Extent�� Flush�Ѵ�.
         * ReuseWPID %1~63 = 0~62 Write �� 0���� ReuseWPID - 1 ���� Write
         * ReuseWPID %0    = -64 ~ -1 - Insert�� Write������ AllocPage �̹Ƿ�,
         *                 �� �� Write�� �ߴٸ� ReuseWPID�� 0�� �Ҵ��ϰ� 1�� �Ǿ��� ���̴�.
         *                 0�̶�� ���� Write������ ���� Extent������ Write�� ��� �Ѵ�. */
        if (( sWASegment->mInsertGrp.mReuseSeqWPID % SDT_WAEXTENT_PAGECOUNT ) != 0 )
        {
            sWCBPtr = getWCBPtr( sWASegment,
                                 sWASegment->mInsertGrp.mReuseSeqWPID -
                                 ( sWASegment->mInsertGrp.mReuseSeqWPID %
                                   SDT_WAEXTENT_PAGECOUNT ) );
        }
        else
        {
            /* ReuseSeqWPID % Extent�� 0 �� ��쿡 ReuseSeqWPID �� BeginWPID�� ���Ƽ�
             * - ExtentCount �ϸ� BeginWPID ���� ���� ���� �ʴ°� �ϴ� ����� ���� ����,
             * �� �ǵ� Insert���� ���� ��츦 �����ϰ�� �� ������ ReuseWPID == BeginWPID�� ���� ����.
             *   (EndWPID�� �����Ͽ� BeginWPID �����ؼ� page�Ҵ� �׸��� +1 ���� ���� �ѹ��� �Ѵ�. )
             * �׸��� �� �ǵ� Insert���� �ʾҴٸ�, OutMemory�� �ƴϾ��� ���̴�.
             * �׷��Ƿ� ReuseSeqWPID % Extent�� 0 �� ���� BeginWPID ���� �����Ƿ�,
             * ReuseSeqWPID - ExtentCount �Ͽ��� BeginWPID ������ �Ѿ�� ��찡 �ƴϴ�.*/
            sWCBPtr = getWCBPtr( sWASegment,
                                 sWASegment->mInsertGrp.mReuseSeqWPID -
                                 SDT_WAEXTENT_PAGECOUNT );  
        }
        sEndWCBPtr = getWCBPtr( sWASegment,
                                sWASegment->mInsertGrp.mReuseSeqWPID );

        for( ; sWCBPtr < sEndWCBPtr ; sWCBPtr++ )
        {
            if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
            {
                IDE_TEST( writeNPage( sWASegment,
                                      sWCBPtr ) != IDE_SUCCESS );
            }
        }

        /* ���� ������� ���� ���� ������ In Memory���� Row�� SubHash�� ����� */
        IDE_TEST( buildSubHash( sWASegment ) != IDE_SUCCESS );

        /* ������ SubHash�� ��� Merge�Ѵ�. */
        IDE_TEST( mergeSubHash( sWASegment ) != IDE_SUCCESS );

        /* BUG-48313 mergeSubHash�� �ι� ȣ��Ǵ� ���� �ذ� */
        sWASegment->mOpenCursorType = SMI_HASH_CURSOR_INIT;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : cursor open������ ���� HashScan �ϴ� ���� �ƴ϶��
 *               Hash Work Area�� Hash Ž���ϱ� ���� ���� �Ѵ�.
 *               Hash Scan Cursor�� ���� �ݺ������� Open�ϹǷ�,
 *               ���� 1�� Cursor Open������ ���� �� �� �ʿ䰡 �ִ�.
 *
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
void sdtHashModule::initToHashScan( sdtHashSegHdr* aWASegment )
{
    UInt    sAllExtCount;
    UInt    sFetchRowExtCount;
    UInt    sSubHashExtCount;

    setInsertHintWCB( aWASegment, NULL );

    sAllExtCount = ( aWASegment->mEndWPID - aWASegment->mHashSlotPageCount )
        / SDT_WAEXTENT_PAGECOUNT;

    if ( aWASegment->mNExtFstPIDList4SubHash.mCount < sAllExtCount )
    {
        sSubHashExtCount = aWASegment->mNExtFstPIDList4SubHash.mCount;
        sFetchRowExtCount = sAllExtCount - sSubHashExtCount;
    }
    else
    {
        if ( aWASegment->mStatsPtr->mIOPassNo < 2 )
        {
            aWASegment->mStatsPtr->mIOPassNo++;
        }

        sSubHashExtCount = sAllExtCount * smuProperty::getTmpHashFetchSubHashMaxRatio() / 100 ;
        if ( sSubHashExtCount == 0 )
        {
            sSubHashExtCount++; // �ּ� �Ѱ�
        }
        else if( sSubHashExtCount == sAllExtCount )
        {
            sSubHashExtCount--; // fetch������ �־�� �Ѵ�. 
        }
        sFetchRowExtCount = sAllExtCount - sSubHashExtCount ;
    }

    aWASegment->mSubHashGrp.mBeginWPID    = aWASegment->mHashSlotPageCount;
    aWASegment->mSubHashGrp.mReuseSeqWPID = aWASegment->mHashSlotPageCount;
    aWASegment->mSubHashGrp.mEndWPID      = aWASegment->mHashSlotPageCount;
    aWASegment->mSubHashGrp.mEndWPID     += sSubHashExtCount * SDT_WAEXTENT_PAGECOUNT;

    aWASegment->mFetchGrp.mBeginWPID    = aWASegment->mSubHashGrp.mEndWPID;
    aWASegment->mFetchGrp.mReuseSeqWPID = aWASegment->mSubHashGrp.mEndWPID;
    aWASegment->mFetchGrp.mEndWPID      = aWASegment->mSubHashGrp.mEndWPID;
    aWASegment->mFetchGrp.mEndWPID     += sFetchRowExtCount * SDT_WAEXTENT_PAGECOUNT;

    IDE_DASSERT( aWASegment->mFetchGrp.mEndWPID == aWASegment->mEndWPID );

    aWASegment->mInsertGrp.mBeginWPID    = aWASegment->mHashSlotPageCount;
    aWASegment->mInsertGrp.mReuseSeqWPID = aWASegment->mHashSlotPageCount;
    aWASegment->mInsertGrp.mEndWPID      = aWASegment->mHashSlotPageCount;

    return ;
}

/***************************************************************************
 * Description : cursor open������ ���� FullScan �ϴ� ���� �ƴ϶��
 *               Hash Work Area�� Full Ž���ϱ� ���� ���� �Ѵ�.
 *               FullScan Cursor Open�� ó�� �ѹ��� �ϹǷ�,
 *               Cursor Open���� �Բ� �ϸ� �Ǳ� ������,
 *               
 * aWASegment  - [IN] ��� WASegment
 ***************************************************************************/
void sdtHashModule::initToFullScan( sdtHashSegHdr* aWASegment )
{
    setInsertHintWCB( aWASegment, NULL );

    aWASegment->mFetchGrp.mBeginWPID = aWASegment->mHashSlotPageCount;
    aWASegment->mFetchGrp.mEndWPID   = aWASegment->mEndWPID;

    if ( aWASegment->mOpenCursorType == SMI_HASH_CURSOR_INIT )
    {
        /* �������� insert���̾���. */
        aWASegment->mFetchGrp.mReuseSeqWPID = aWASegment->mInsertGrp.mReuseSeqWPID;
    }
    else
    {
        IDE_ASSERT( aWASegment->mOpenCursorType != SMI_HASH_CURSOR_NONE );

        /* �������� hash scan���� �߾���.
         * ���� fetch reuse�� �״�� �� �ξ �ȴ�.*/
    }

    if ( aWASegment->mFetchGrp.mReuseSeqWPID >= aWASegment->mFetchGrp.mEndWPID )
    {
        aWASegment->mFetchGrp.mReuseSeqWPID = aWASegment->mFetchGrp.mBeginWPID;
    }

    IDE_DASSERT( aWASegment->mFetchGrp.mBeginWPID <= aWASegment->mFetchGrp.mReuseSeqWPID );
    IDE_DASSERT( aWASegment->mFetchGrp.mEndWPID   >  aWASegment->mFetchGrp.mReuseSeqWPID );

    aWASegment->mSubHashGrp.mBeginWPID    = aWASegment->mEndWPID;
    aWASegment->mSubHashGrp.mReuseSeqWPID = aWASegment->mEndWPID;
    aWASegment->mSubHashGrp.mEndWPID      = aWASegment->mEndWPID;

    aWASegment->mInsertGrp.mBeginWPID    = aWASegment->mEndWPID;
    aWASegment->mInsertGrp.mReuseSeqWPID = aWASegment->mEndWPID;
    aWASegment->mInsertGrp.mEndWPID      = aWASegment->mEndWPID;

    return ;
}

/***************************************************************************
 * Description : FullScan Cursor�� ���ϴ�.
 *
 * aHeader   - [IN] ��� Table
 * aCursor   - [OUT] ��ȯ��
 * aRowGRID  - [OUT] Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::openFullScanCursor( smiHashTempCursor  * aCursor,
                                          UChar              ** aRow,
                                          scGRID              * aRowGRID )
{
    sdtHashSegHdr * sWASegment;
    /* Row�� ��ġ : page�� fix ���� �����Ƿ� �ٲ�� �־ ���� ���� �ʿ� */

    sWASegment = (sdtHashSegHdr*)aCursor->mWASegment;

    sWASegment->mOpenCursorType = SMI_HASH_CURSOR_FULL_SCAN;

    aCursor->mRowPtr      = NULL;
    aCursor->mChildRowPtr = NULL;

    aCursor->mSeq = 0;

    aCursor->mTTHeader->mTTState = SMI_TTSTATE_HASH_FETCH_FULLSCAN;

    // �ΰ���
    // 1. w page�� ����Ͽ��ٸ� w page �� ���
    // 2. n page �� ��� �Ͽ��ٸ� n page ����,

    aCursor->mChildOffset  = ID_SIZEOF(sdtHashPageHdr);

    if ( aCursor->mIsInMemory == SDT_WORKAREA_IN_MEMORY )
    {
        IDE_DASSERT( sWASegment->mInsertGrp.mReuseSeqWPID >= sWASegment->mInsertGrp.mBeginWPID );

        aCursor->mWCBPtr    = getWCBPtr( sWASegment,
                                         sWASegment->mInsertGrp.mBeginWPID );

        aCursor->mWAPagePtr = ((sdtWCB*)aCursor->mWCBPtr)->mWAPagePtr;

        if ( aCursor->mWAPagePtr != NULL )
        {
            /* beginWPID�� ���� WCB�� page ptr�� Null �̸�,
             * 1���� row�� insert���� �ʾҴٴ� ���̴�.
             * �׷��� end wcb ptr�� �ʿ����.*/
            aCursor->mEndWCBPtr = getWCBPtr( sWASegment,
                                             sWASegment->mInsertGrp.mReuseSeqWPID ) ;
        }

        aCursor->mChildPageID = ((sdtWCB*)aCursor->mWCBPtr)->mNPageID;
    }
    else
    {
        initToFullScan( sWASegment );

        aCursor->mSeq           = 0;
        aCursor->mChildPageID   = sWASegment->mNExtFstPIDList4Row.mHead->mMap[0];
        aCursor->mCurNExtentArr = sWASegment->mNExtFstPIDList4Row.mHead;
        aCursor->mNExtLstPID    = aCursor->mChildPageID + SDT_WAEXTENT_PAGECOUNT - 1;
        aCursor->mSeq++;

        aCursor->mWCBPtr = findWCB( sWASegment, aCursor->mChildPageID );

        if ( aCursor->mWCBPtr == NULL )
        {
            /* Sub hash group� ���� �Ҵ���� ���� wa page�� ���� �� �ִ�. */
            IDE_TEST( getFreeWAPage( sWASegment,
                                     &sWASegment->mFetchGrp,
                                     (sdtWCB**)&(aCursor->mWCBPtr) )
                      != IDE_SUCCESS );

            IDE_TEST( readNPage( sWASegment,
                                 (sdtWCB*)aCursor->mWCBPtr,
                                 aCursor->mChildPageID )
                      != IDE_SUCCESS );
        }

        aCursor->mWAPagePtr = ((sdtWCB*)aCursor->mWCBPtr)->mWAPagePtr;

        IDE_TEST( preReadNExtents( sWASegment,
                                   aCursor->mChildPageID,
                                   SDT_WAEXTENT_PAGECOUNT )
                  != IDE_SUCCESS );
    }

    IDE_TEST( fetchFullNext( aCursor,
                             aRow,
                             aRowGRID) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : HashScan Cursor�� ChildPageID�� �������� Row�� 1�� �����´�..
 *               Single Row������ HashValue�� �̹� Ȯ�� �ߴ�.
 *
 * aCursor   - [IN]  Ž���� ������� Hash Cursor
 * aRow      - [OUT] ã�� Row
 * aRowGRID  - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::fetchHashScanInternalGRID( smiHashTempCursor * aCursor,
                                                 UChar            ** aRow,
                                                 scGRID            * aRowGRID )
{
    idBool           sResult = ID_FALSE;
    sdtHashTRPHdr  * sTRPHeader;
    sdtWCB         * sWCBPtr = NULL;
    idBool           sIsFixedPage = ID_FALSE;

    IDE_TEST( getSlotPtr( (sdtHashSegHdr*)aCursor->mWASegment,
                          &((sdtHashSegHdr*)aCursor->mWASegment)->mFetchGrp,
                          aCursor->mChildPageID ,
                          aCursor->mChildOffset ,
                          (UChar**)&sTRPHeader,
                          &sWCBPtr )
              != IDE_SUCCESS );

    IDE_ASSERT( sTRPHeader->mHashValue == aCursor->mHashValue );

    //  �о�� ���� SubHash�� �����ְ�, row �ϼ��� �ִ�.
    IDE_ASSERT ( sTRPHeader->mIsRow == SDT_HASH_ROW_HEADER );
    // Row�� �о�� ���

    fixWAPage( sWCBPtr );
    sIsFixedPage = ID_TRUE;

    IDE_TEST( filteringAndFetch( aCursor,
                                 sTRPHeader,
                                 *aRow,
                                 &sResult )
              != IDE_SUCCESS );

    sIsFixedPage = ID_FALSE;
    unfixWAPage( sWCBPtr );

    IDE_DASSERT( sTRPHeader->mChildPageID == SM_NULL_PID );

    if ( sResult == ID_TRUE )
    {
        SC_MAKE_GRID( *aRowGRID,
                      aCursor->mTTHeader->mSpaceID,
                      aCursor->mChildPageID,
                      aCursor->mChildOffset );
    }
    else
    {
        sWCBPtr = NULL;
        *aRow   = NULL;
        SC_MAKE_NULL_GRID(*aRowGRID);
    }

    aCursor->mWCBPtr      = sWCBPtr;
    aCursor->mChildPageID = SM_NULL_PID;
    aCursor->mChildOffset = 0;

    aCursor->mTTHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aCursor->mTTHeader->mStatsPtr->mExtraStat2, 1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        unfixWAPage( sWCBPtr );
    }

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : sub hash���� hash�� ���� �κ��� Ž�� (BUG-48235)
 *
 * aSubHash   - [IN] Sub Hash Node
 * aHashValue - [IN] ã�� hash value
 ***************************************************************************/
UInt sdtHashModule::bSearchSubHash( sdtSubHash * aSubHash, UInt aHashValue )
{
    sdtSubHashKey * sSubHashArray = aSubHash->mKey;
    SInt            sMid          = SDT_SUBHASH_MAX_KEYCOUNT ;
    SInt            sFirst        = 0;
    SInt            sLast         = aSubHash->mKeyCount - 1;

    while( sFirst <= sLast )
    {
        sMid = ( sFirst + sLast ) / 2;

        if (( aHashValue == sSubHashArray[ sMid ].mHashHigh ) &&
            (( sFirst == sMid ) || ( aHashValue != sSubHashArray[ sMid-1 ].mHashHigh )))
        {
            break;
        }

        if ( aHashValue > sSubHashArray[ sMid ].mHashHigh )
        {
            sFirst = sMid + 1;
        }
        else
        {
            sLast = sMid - 1 ;
        }
    }

    return sMid;
}


/***************************************************************************
 * Description : HashScan Cursor�� ChildPageID�� �������� SubHash�� Ž���ؼ�
 *               Next Row�� �����´�.
 *
 * aCursor   - [IN]  Ž���� ������� Hash Cursor
 * aRow      - [OUT] ã�� Row
 * aRowGRID  - [OUT] ã�� Row�� GRID
 ***************************************************************************/
IDE_RC sdtHashModule::fetchHashScanInternalSubHash( smiHashTempCursor * aCursor,
                                                    UChar            ** aRow,
                                                    scGRID            * aRowGRID )
{
    idBool           sResult = ID_FALSE;
    idBool           sIsSorted = ((sdtHashSegHdr*)aCursor->mWASegment)->mUnique == ID_TRUE ? ID_FALSE : ID_TRUE;
    sdtSubHash     * sSubHash;
    sdtHashTRPHdr  * sTRPHeader;
    UInt             sLoop;
    UInt             i;
    UShort           sHashHigh;
    sdtWCB         * sSubWCBPtr = NULL;
    sdtWCB         * sRowWCBPtr;
    idBool           sIsFixedSubHash = ID_FALSE;
    idBool           sIsFixedRowPage = ID_FALSE;

    sLoop = 0;
    /* ���� Ž�� */

    if( aCursor->mTTHeader->mCheckCnt > SMI_TT_STATS_INTERVAL )
    {
        IDE_TEST( iduCheckSessionEvent( aCursor->mTTHeader->mStatistics ) != IDE_SUCCESS);
        aCursor->mTTHeader->mCheckCnt = 0;
    }

    do
    {
        if ( aCursor->mSubHashPtr == NULL )
        {
            sLoop++;
            IDE_TEST( getSlotPtr( (sdtHashSegHdr*)aCursor->mWASegment,
                                  &((sdtHashSegHdr*)aCursor->mWASegment)->mSubHashGrp,
                                  aCursor->mChildPageID ,
                                  aCursor->mChildOffset ,
                                  (UChar**)&sSubHash,
                                  &sSubWCBPtr )
                      != IDE_SUCCESS );
        }
        else
        {
            sSubHash   = (sdtSubHash*)aCursor->mSubHashPtr;
            sSubWCBPtr = (sdtWCB*)aCursor->mSubHashWCBPtr4Fetch;
        }

        //  �о�� ���� SubHash�� �����ְ�, row �ϼ��� �ִ�.
        IDE_ASSERT( ((sdtHashTRPHdr*)sSubHash)->mIsRow != SDT_HASH_ROW_HEADER );

        // SubHash �� ���� ���
        IDE_DASSERT( sSubHash->mHashLow ==  SDT_MAKE_HASH_LOW( aCursor->mHashValue ) );

        sHashHigh = SDT_MAKE_HASH_HIGH( aCursor->mHashValue );

        fixWAPage( sSubWCBPtr );
        sIsFixedSubHash = ID_TRUE;

        /* BUG-48235 ���ĵǾ� �ִٸ� ����Ž������ ã�´�.*/
        if (( sIsSorted == ID_TRUE ) && ( aCursor->mSubHashIdx == 0 ))
        {
            aCursor->mSubHashIdx = bSearchSubHash( sSubHash, sHashHigh );
            if ( aCursor->mSubHashIdx == SDT_SUBHASH_MAX_KEYCOUNT )
            {
                aCursor->mSubHashIdx = sSubHash->mKeyCount ;
            }
        }

        for( i = aCursor->mSubHashIdx ; i < sSubHash->mKeyCount ; i++ )
        {
            if ( sSubHash->mKey[i].mHashHigh != sHashHigh )
            {
                if ( sIsSorted == ID_FALSE )
                {
                    continue;
                }
                else
                {
                    i = aCursor->mSubHashIdx = sSubHash->mKeyCount ; 
                    break;
                }
            }
            sLoop++;

            IDE_TEST( getSlotPtr( (sdtHashSegHdr*)aCursor->mWASegment,
                                  &((sdtHashSegHdr*)aCursor->mWASegment)->mFetchGrp,
                                  sSubHash->mKey[i].mPageID ,
                                  sSubHash->mKey[i].mOffset ,
                                  (UChar**)&sTRPHeader,
                                  &sRowWCBPtr )
                      != IDE_SUCCESS );

            IDE_ASSERT_MSG(( ((sdtHashPageHdr*)sRowWCBPtr->mWAPagePtr)->mSelfNPID ==
                             sSubHash->mKey[i].mPageID ),
                           "%"ID_UINT32_FMT" != %"ID_UINT32_FMT" %"ID_UINT32_FMT"\n",
                           ((sdtHashPageHdr*)sRowWCBPtr->mWAPagePtr)->mSelfNPID ,
                           sSubHash->mKey[i].mPageID,
                           sRowWCBPtr->mNPageID );
            IDE_ASSERT_MSG(( sRowWCBPtr->mNPageID == sSubHash->mKey[i].mPageID ),
                           "%"ID_UINT32_FMT" != %"ID_UINT32_FMT"\n",
                           sRowWCBPtr->mNPageID ,
                           sSubHash->mKey[i].mPageID );

            IDE_ASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_CHILDGRID ) );
            IDE_ASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );
            IDE_ASSERT( sTRPHeader->mIsRow == SDT_HASH_ROW_HEADER );
            IDE_ASSERT( sTRPHeader->mHashValue == aCursor->mHashValue );

            fixWAPage( sRowWCBPtr );
            sIsFixedRowPage = ID_TRUE;

            IDE_TEST( filteringAndFetch( aCursor,
                                         sTRPHeader,
                                         *aRow,
                                         &sResult )
                      != IDE_SUCCESS );

            sIsFixedRowPage = ID_FALSE;
            unfixWAPage( sRowWCBPtr );

            if ( sResult == ID_TRUE )
            {
                break;
            }
        }
        sIsFixedSubHash = ID_FALSE;
        unfixWAPage( sSubWCBPtr );

        if ( ( i+1 ) >= sSubHash->mKeyCount )
        {
            // sub hash���� ã�� ���߰ų�, ������ key���� hit�� ���
            aCursor->mChildPageID = sSubHash->mNextPageID;
            aCursor->mChildOffset = sSubHash->mNextOffset;
            aCursor->mSubHashIdx  = 0;
            aCursor->mSubHashPtr  = NULL;
            aCursor->mSubHashWCBPtr4Fetch = NULL;

            if ( sResult == ID_TRUE )
            {
                break;
            }
        }
        else
        {
            IDE_ASSERT( sResult == ID_TRUE );

            // ã�����
            aCursor->mSubHashIdx    = i+1;
            aCursor->mSubHashPtr    = sSubHash;
            aCursor->mSubHashWCBPtr4Fetch = sSubWCBPtr;

            break;
        }

    }while( aCursor->mChildPageID != SM_NULL_PID );

    if ( sResult == ID_TRUE )
    {
        IDE_DASSERT( sRowWCBPtr != NULL );

        SC_MAKE_GRID( *aRowGRID,
                      aCursor->mTTHeader->mSpaceID,
                      sSubHash->mKey[i].mPageID,
                      sSubHash->mKey[i].mOffset );
    }
    else
    {
        sRowWCBPtr = NULL;
        *aRow      = NULL;
        SC_MAKE_NULL_GRID(*aRowGRID);
    }

    aCursor->mWCBPtr = sRowWCBPtr;
    aCursor->mTTHeader->mStatsPtr->mExtraStat2 = IDL_MAX( aCursor->mTTHeader->mStatsPtr->mExtraStat2, sLoop );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedSubHash == ID_TRUE )
    {
        unfixWAPage( sSubWCBPtr );
    }

    if ( sIsFixedRowPage == ID_TRUE )
    {
        unfixWAPage( sRowWCBPtr );
    }

    smiTempTable::checkAndDump( aCursor->mTTHeader );

    return IDE_FAILURE;
}

/***************************************************************************
 * Description : HashScan Cursor�� ChildPageID�� �������� SubHash�� Ž���ؼ�
 *               Next Row�� �����´�.
 *
 * aCursor   - [IN]  Ž���� ������� Cursor
 ***************************************************************************/
IDE_RC sdtHashModule::getNextNPageForFullScan( smiHashTempCursor  * aCursor )
{
    sdtHashSegHdr * sWASeg = (sdtHashSegHdr*)aCursor->mWASegment;
    sdtWCB        * sWCBPtr;
    scPageID        sNPageID;

    IDE_DASSERT( aCursor->mIsInMemory == SDT_WORKAREA_OUT_MEMORY );

    sWCBPtr = (sdtWCB*)aCursor->mWCBPtr;

    IDE_ASSERT( SDT_IS_FIXED_PAGE( sWCBPtr ) );
    unfixWAPage( sWCBPtr );

    /* 0. ���� Page Write��û
     *
     * ������ �о�� �ϴ� NPageID���� Ȯ��
     * 1. ���� Extent���̸� +1
     * 2. Extent������ Page�̸� ���� Extent�� FirstPageID�� �����´�.
     * 3. �̸� 64�� Page�� �о�д�.*/
    if ( aCursor->mChildPageID < aCursor->mNExtLstPID )
    {
        sNPageID = aCursor->mChildPageID + 1;
    }
    else
    {
        if ( aCursor->mSeq < sWASeg->mNExtFstPIDList4Row.mCount )
        {
            // BUG-47544 Next Normal Extent Array�� �Ѱ����� �ʴ� ���� �ذ�
            if (( aCursor->mSeq % SDT_NEXTARR_EXTCOUNT ) == 0 )
            {
                // seq == 0 �� Open Cursor���� �̹� ó���Ǿ���.
                // next array�� �Ѿ�� �� ��ġ���� 0�� �� ����.
                IDE_ERROR( aCursor->mSeq > 0 );

                aCursor->mCurNExtentArr = ((sdtNExtentArr*)aCursor->mCurNExtentArr)->mNextArr;
            }

            sNPageID = ((sdtNExtentArr*)aCursor->mCurNExtentArr)->mMap[ aCursor->mSeq % SDT_NEXTARR_EXTCOUNT ];

            if ( sNPageID != sWASeg->mNExtFstPIDList4Row.mLastFreeExtFstPID )
            {
                aCursor->mNExtLstPID = sNPageID + SDT_WAEXTENT_PAGECOUNT - 1;

                IDE_TEST( preReadNExtents( sWASeg,
                                           sNPageID,
                                           SDT_WAEXTENT_PAGECOUNT )
                          != IDE_SUCCESS );
            }
            else
            {
                aCursor->mNExtLstPID = sNPageID + sWASeg->mNExtFstPIDList4Row.mPageSeqInLFE - 1;

                IDE_TEST( preReadNExtents( sWASeg,
                                           sNPageID,
                                           sWASeg->mNExtFstPIDList4Row.mPageSeqInLFE  )
                          != IDE_SUCCESS );
            }

            aCursor->mSeq++;
        }
        else
        {
            // Full Scan�� ��� �Ϸ��
            aCursor->mChildPageID = SM_NULL_PID;
            aCursor->mWCBPtr    = NULL;
            aCursor->mWAPagePtr = NULL;

            IDE_CONT( SKIP );
        }
    }

    sWCBPtr = findWCB( sWASeg, sNPageID );
    IDE_ASSERT( sWCBPtr != NULL );

    /* pre read extent�� �̸� �о� �״µ� �������� �ֳ�?
     * Ȯ���غ����� */
    IDE_DASSERT( sWCBPtr->mWAPagePtr != NULL );
    IDE_DASSERT( sWCBPtr->mNPageID == sNPageID );

    aCursor->mWCBPtr      = sWCBPtr;
    aCursor->mWAPagePtr   = sWCBPtr->mWAPagePtr;
    aCursor->mChildPageID = sNPageID;
    aCursor->mChildOffset = ID_SIZEOF(sdtHashPageHdr);

    IDE_DASSERT_MSG(( ((sdtHashPageHdr*)aCursor->mWAPagePtr)->mSelfNPID == aCursor->mChildPageID ),
                    "%"ID_UINT32_FMT" != %"ID_UINT32_FMT"\n",
                    ((sdtHashPageHdr*)aCursor->mWAPagePtr)->mSelfNPID ,
                    aCursor->mChildPageID );

    IDE_DASSERT_MSG(( ((sdtWCB*)aCursor->mWCBPtr)->mNPageID == aCursor->mChildPageID ),
                    "%"ID_UINT32_FMT" != %"ID_UINT32_FMT"\n",
                    ((sdtWCB*)aCursor->mWCBPtr)->mNPageID ,
                    aCursor->mChildPageID );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description : fetch�ϰ� �´��� Ȯ�� ��.
 *
 * aTempCursor  - [IN] ��� Ŀ��
 * aTargetPtr   - [IN] ��� Row�� Ptr
 * aRow         - [OUT] ��� Row�� ���� Bufer
 * aResult      - [OUT] Filtering ��� ����
 ***************************************************************************/
IDE_RC sdtHashModule::filteringAndFetch( smiHashTempCursor  * aTempCursor,
                                         sdtHashTRPHdr      * aTRPHeader,
                                         UChar              * aRow,
                                         idBool             * aResult )
{
    smiTempTableHeader * sHeader;
    sdtHashSegHdr      * sWASeg;
    sdtHashScanInfo      sTRPInfo;
    UInt                 sFlag;

    *aResult = ID_FALSE;

    sHeader = aTempCursor->mTTHeader;
    sWASeg  = (sdtHashSegHdr*)aTempCursor->mWASegment;

    IDE_DASSERT( SM_IS_FLAG_ON( aTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) );

    sFlag = aTempCursor->mTCFlag;

    /* HashScan�ε� HashValue�� �ٸ��� �ȵ� */
    if ( sFlag & SMI_TCFLAG_HASHSCAN )
    {
        IDE_TEST_CONT( aTRPHeader->mHashValue != aTempCursor->mHashValue, SKIP );
    }
    else
    {
        /* nothing to do */
    }

    /* HItFlagüũ�� �ʿ��Ѱ�? */
    if ( sFlag & SMI_TCFLAG_HIT_MASK )
    {
        /* Hit������ ũ�� �� Flag�̴�.
         * IgnoreHit -> Hit��ü�� ��� ����
         * Hit       -> Hit�� �͸� ������
         * NoHit     -> Hit �ȵ� �͸� ������
         *
         * �� if���� IgnoreHit�� �ƴ����� �����ϴ� ���̰�.
         * �Ʒ� IF���� Hit���� NoHit���� �����ϴ� ���̴�. */
        if ( sFlag & SMI_TCFLAG_HIT )
        {
            /* HIT �� ã�´�. NOHIT�̸� SKIP */
            IDE_DASSERT( !( sFlag & SMI_TCFLAG_NOHIT ) );/*Hit Nohit �Ѵ� ���� �Ǿ� ������ �ȵȴ�.*/
            IDE_TEST_CONT( aTRPHeader->mHitSequence != sHeader->mHitSequence, SKIP );
        }
        else
        {
            /* NOHIT �� ã�´�. HIT�̸� SKIP */
            IDE_DASSERT( sFlag & SMI_TCFLAG_NOHIT );/*Hit Nohit �Ѵ� ���� �ȵǾ� ������ �ȵȴ�.*/
            IDE_TEST_CONT( aTRPHeader->mHitSequence == sHeader->mHitSequence, SKIP );
        }
    }
    else
    {
        /* nothing to do */
    }

    sTRPInfo.mTRPHeader = aTRPHeader;
    sTRPInfo.mFetchEndOffset = sHeader->mRowSize;

    IDE_TEST( fetch( sWASeg,
                     aRow,
                     &sTRPInfo ) != IDE_SUCCESS );

    IDE_DASSERT( sTRPInfo.mValueLength <= sHeader->mRowSize );

    if ( sFlag & SMI_TCFLAG_FILTER_ROW )
    {
        IDE_TEST( aTempCursor->mRowFilter->callback(
                      aResult,
                      sTRPInfo.mValuePtr,
                      NULL,
                      0,
                      SC_NULL_GRID,
                      aTempCursor->mRowFilter->data )
                  != IDE_SUCCESS );
        IDE_TEST_CONT( *aResult == ID_FALSE, SKIP );
    }
    else
    {
        /* nothing to do */
    }

    *aResult             = ID_TRUE;
    aTempCursor->mRowPtr = (UChar*)aTRPHeader;

    if ( aRow != sTRPInfo.mValuePtr )
    {
        IDE_DASSERT( SM_IS_FLAG_OFF( sFlag, SDT_TRFLAG_NEXTGRID ) );
        idlOS::memcpy( aRow,
                       sTRPInfo.mValuePtr,
                       sTRPInfo.mValueLength );
    }

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Row�� Chaining, �� �ٸ� Page�� ���� ��ϵ� ��� ���Ǹ�,
 *               ��ģ �κ��� Fetch�ؿ´�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aRowBuffer   - [IN] �ɰ��� Column�� �����ϱ� ���� Buffer
 * aTRPInfo     - [OUT] Fetch�� ���
 ***************************************************************************/
IDE_RC sdtHashModule::fetchChainedRowPiece( sdtHashSegHdr   * aWASegment,
                                            UChar           * aRowBuffer,
                                            sdtHashScanInfo * aTRPInfo )
{
    UChar        * sCursor;
    sdtHashTRPHdr* sTRPHeader;
    UChar          sFlag;
    UInt           sRowBufferOffset = 0;
    UInt           sTRPHeaderSize;

    sTRPHeaderSize =  SDT_HASH_TR_HEADER_SIZE_FULL ;

    /* Chainig Row�ϰ��, �ڽ��� ReadPage�ϸ鼭
     * HeadRow�� unfix�� �� �ֱ⿡, Header�� �����صΰ� �̿���*/
#ifdef DEBUG
    sdtHashTRPHdr  sTRPHeaderBuffer;
    idlOS::memcpy( &sTRPHeaderBuffer,
                   aTRPInfo->mTRPHeader,
                   sTRPHeaderSize );
#endif

    sCursor = ((UChar*)aTRPInfo->mTRPHeader) + sTRPHeaderSize;

    sTRPHeader           = aTRPInfo->mTRPHeader;
    aTRPInfo->mValuePtr  = aRowBuffer;

    idlOS::memcpy( aRowBuffer,
                   sCursor,
                   sTRPHeader->mValueLength );

    IDE_ASSERT( sTRPHeader->mValueLength < SD_PAGE_SIZE );

    sRowBufferOffset       = sTRPHeader->mValueLength;
    aTRPInfo->mValueLength = sTRPHeader->mValueLength;

    do
    {
        if ( aWASegment->mIsInMemory == SDT_WORKAREA_OUT_MEMORY )
        {
            IDE_TEST( getSlotPtr( aWASegment,
                                  &aWASegment->mFetchGrp,
                                  sTRPHeader->mNextPageID,
                                  sTRPHeader->mNextOffset,
                                  &sCursor,
                                  NULL )
                      != IDE_SUCCESS );
        }
        else
        {
            sCursor = (UChar*)sTRPHeader->mNextRowPiecePtr;
        }

        sFlag      = ((sdtHashTRPHdr*)sCursor)->mTRFlag;
        sTRPHeader = (sdtHashTRPHdr*)sCursor;

        IDE_ASSERT( sTRPHeader->mValueLength < SD_PAGE_SIZE );

        sCursor   +=  SDT_HASH_TR_HEADER_SIZE_FULL ;

        IDE_DASSERT( sTRPHeaderBuffer.mHashValue == sTRPHeader->mHashValue );

        idlOS::memcpy( aRowBuffer + sRowBufferOffset,
                       sCursor,
                       sTRPHeader->mValueLength );

        sRowBufferOffset       += sTRPHeader->mValueLength;
        aTRPInfo->mValueLength += sTRPHeader->mValueLength;

        if ( aTRPInfo->mFetchEndOffset <= sRowBufferOffset )
        {
            /* ��������� �ʿ��ϴ�, ���Ĵ� Scan���� �ʴ´�.*/
            break;
        }

        IDE_ERROR ( SM_IS_FLAG_ON( sFlag, SDT_TRFLAG_NEXTGRID ) )
    }
    while( sTRPHeader->mNextPageID != SM_NULL_PID );

#ifdef DEBUG
    IDE_DASSERT( idlOS::memcmp( &sTRPHeaderBuffer,
                                aTRPInfo->mTRPHeader,
                                sTRPHeaderSize ) == 0 );
#endif
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    dumpTempTRPHeader,
                                    (void*)aTRPInfo->mTRPHeader );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Row�� Chaining, �� �ٸ� Page�� ���� ��ϵ� ��� ���Ǹ�,
 *               ��ģ �κ��� Update �Ѵ�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aValueList   - [IN] 
 ***************************************************************************/
IDE_RC sdtHashModule::updateChainedRowPiece( smiHashTempCursor * aTempCursor,
                                             smiValue          * aValueList )
{
    const smiColumnList * sUpdateColumn;
    const smiColumn * sColumn;
    smiValue        * sValueList;
    UChar           * sRowPos;
    sdtHashTRPHdr   * sTRPHeader;
    sdtWCB          * sWCBPtr;
    UInt              sBeginOffset = 0;
    UInt              sEndOffset;
    UInt              sDstOffset;
    UInt              sSrcOffset;
    UInt              sSize;
    idBool            sIsWrite;

    sRowPos = aTempCursor->mRowPtr;
    sWCBPtr = (sdtWCB*)aTempCursor->mWCBPtr;

    while( 1 )
    {
        sTRPHeader = (sdtHashTRPHdr*)sRowPos;
        sEndOffset = sBeginOffset + sTRPHeader->mValueLength;

        sRowPos   +=  SDT_HASH_TR_HEADER_SIZE_FULL ;

        sUpdateColumn = aTempCursor->mUpdateColumns;
        sValueList    = aValueList;
        sIsWrite      = ID_FALSE;

        while( sUpdateColumn != NULL )
        {
            sColumn = ( smiColumn*)sUpdateColumn->column;

            if (( sEndOffset > sColumn->offset ) &&                          // �������� end ���� �۰�
                (( sColumn->offset + sValueList->length ) >= sBeginOffset )) // �� ���� Begin���� ũ��
            {
                sSize = sValueList->length;
                if ( sColumn->offset < sBeginOffset ) /* ���� ���� �κ��� �߷ȴ°�? */
                {
                    // �߷ȴ�.
                    sDstOffset = 0;
                    sSrcOffset = sBeginOffset - sColumn->offset;
                    sSize -= sSrcOffset;
                }
                else
                {
                    // ���߷ȴ�.
                    sDstOffset = sColumn->offset - sBeginOffset;
                    sSrcOffset = 0;
                }

                /* ������ �� �κ��� �߷ȴ°�? */
                if ( (sColumn->offset + sValueList->length ) > sEndOffset )
                {
                    // �߷ȴ�.
                    sSize -= (sColumn->offset + sValueList->length - sEndOffset);
                }

                idlOS::memcpy( sRowPos + sDstOffset,
                               ((UChar*)sValueList->value) + sSrcOffset,
                               sSize );
                sIsWrite = ID_TRUE;
            }

            sValueList++;
            sUpdateColumn = sUpdateColumn->next;
        }

        sBeginOffset = sEndOffset;

        if (( sIsWrite == ID_TRUE ) && ( aTempCursor->mWCBPtr != NULL ))
        {
            IDE_DASSERT( sWCBPtr->mWPState != SDT_WA_PAGESTATE_INIT );

            sWCBPtr->mWPState = SDT_WA_PAGESTATE_DIRTY;
        }

        if ( aTempCursor->mUpdateEndOffset <= sEndOffset )
        {
            // update �Ϸ�
            break;
        }

        IDE_ASSERT( SM_IS_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_NEXTGRID ) );

        if ( aTempCursor->mIsInMemory == SDT_WORKAREA_OUT_MEMORY )
        {
            IDE_TEST( getSlotPtr( (sdtHashSegHdr*)aTempCursor->mWASegment,
                                  &((sdtHashSegHdr*)aTempCursor->mWASegment)->mFetchGrp,
                                  sTRPHeader->mNextPageID,
                                  sTRPHeader->mNextOffset,
                                  &sRowPos,
                                  &sWCBPtr )
                      != IDE_SUCCESS );
        }
        else
        {
            sRowPos = (UChar*)sTRPHeader->mNextRowPiecePtr;
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : WCB�� �ʱ�ȭ�Ѵ�.
 *
 * aWCBPtr   - [IN] ��� WCBPtr
 ***************************************************************************/
void sdtHashModule::initWCB( sdtWCB       * aWCBPtr )
{
    aWCBPtr->mWPState   = SDT_WA_PAGESTATE_INIT;
    aWCBPtr->mNPageID   = SC_NULL_PID;
    aWCBPtr->mFix       = 0;
    aWCBPtr->mNextWCB4Hash = NULL;
}

/**************************************************************************
 * Description : NPageHash�� ������ �ش� NPage�� ���� WPage�� ã�Ƴ�
 *
 * aWASegment - [IN] ��� WASegment
 * aNPID      - [IN] ã�� Normal Page�� ID
 ***************************************************************************/
sdtWCB * sdtHashModule::findWCB( sdtHashSegHdr* aWASegment,
                                 scPageID       aNPID )
{
    UInt       sHashValue = getNPageHashValue( aWASegment, aNPID );
    sdtWCB   * sWCBPtr;

    IDE_DASSERT( aWASegment->mNPageHashPtr != NULL );

    sWCBPtr = aWASegment->mNPageHashPtr[ sHashValue ];

    while( sWCBPtr != NULL )
    {
        IDE_DASSERT( getNPageHashValue( aWASegment, sWCBPtr->mNPageID )
                     == sHashValue );

        if ( sWCBPtr->mNPageID == aNPID )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        sWCBPtr  = sWCBPtr->mNextWCB4Hash;
    }

    return sWCBPtr;
}

/**************************************************************************
 * Description : ������ �׷쿡�� �� Page�� �ϳ� �Ҵ�޴´�.
 *
 * aWASegment  - [IN] ��� WASegment
 * aGroup      - [IN] ��� Group
 * aWCBPtr     - [OUT] �Ҵ���� page�� WCBPtr
 ***************************************************************************/
IDE_RC sdtHashModule::getFreeWAPage( sdtHashSegHdr* aWASegment,
                                     sdtHashGroup * aGroup,
                                     sdtWCB      ** aWCBPtr )
{
    sdtWCB   * sWCBPtr;

    IDE_ASSERT( aGroup->mBeginWPID < aGroup->mEndWPID );

    /* Group�� �ѹ� ���� ��ȸ�� */

    if ( aGroup->mReuseSeqWPID >= aGroup->mEndWPID )
    {
        aGroup->mReuseSeqWPID = aGroup->mBeginWPID;
    }

    while( 1 )
    {
        sWCBPtr = getWCBPtr( aWASegment, aGroup->mReuseSeqWPID );
        aGroup->mReuseSeqWPID++;

        /* Group�� �ѹ� ���� ��ȸ�� */
        if ( aGroup->mReuseSeqWPID == aGroup->mEndWPID )
        {
            aGroup->mReuseSeqWPID = aGroup->mBeginWPID;
        }

        if ( sWCBPtr->mWAPagePtr == NULL )
        {
            if ( set64WAPagesPtr( aWASegment,
                                  sWCBPtr ) == IDE_SUCCESS )
            {
                /* ó�� �Ҵ�� Page�� ���� ���� ���� ���� Page�̴�.
                 * State�� Ȯ�� ���� �ʰ� �ٷ� ����ص� �ȴ�.*/
                IDE_DASSERT( SDT_IS_NOT_FIXED_PAGE( sWCBPtr ) );
                break;
            }
            else
            {
                /* �Ҵ翡 ���� �Ͽ��� TOTAL WA �� ������ ��Ȳ�̴�. */
                IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_WORKAREA );

                if ( aGroup->mBeginWPID == getWPageID( aWASegment,
                                                       sWCBPtr ) )
                {
                    /* �ϳ��� �Ҵ� �ȵǾ� ������ �ϳ� �Ҵ��Ѵ�.*/
                    IDE_TEST( sdtWAExtentMgr::memAllocWAExtent( &aWASegment->mWAExtentInfo )
                              != IDE_SUCCESS );
                    aWASegment->mNxtFreeWAExtent = aWASegment->mWAExtentInfo.mTail;
                    aWASegment->mStatsPtr->mOverAllocCount++;

                    // �̰� �ݵ�� �����ؾ� �Ѵ�.
                    IDE_TEST( set64WAPagesPtr( aWASegment,
                                               sWCBPtr ) != IDE_SUCCESS );
                    break;
                }
                else
                {
                    /* �ϳ��� �Ҵ�Ǿ� �ִٸ�, ó������ �ٽ� �õ��Ѵ�. */
                    aGroup->mReuseSeqWPID = aGroup->mBeginWPID;
                    continue;
                }
            }
        }

        if ( SDT_IS_NOT_FIXED_PAGE( sWCBPtr ) )
        {
            /* ����� ����� */
            if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
            {
                IDE_TEST( writeNPage( aWASegment,
                                      sWCBPtr ) != IDE_SUCCESS );
            }
            break;
        }
    }

    /* �Ҵ���� �������� �� �������� ���� */

    /* Assign�Ǿ������� Assign �� �������� �ʱ�ȭ�����ش�. */
    if (( aWASegment->mNPageHashPtr != NULL ) &&
        ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_CLEAN ))
    {
        IDE_TEST( unassignNPage( aWASegment,
                                 sWCBPtr )
                  != IDE_SUCCESS );
    }

    initWCB( sWCBPtr );

    IDE_DASSERT( aWASegment->mInsertHintWCBPtr != sWCBPtr );

    (*aWCBPtr) = sWCBPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
//XXX �̸� ���� getSlotPtr

/**************************************************************************
 * Description :
 *     WGRID�� �������� �ش� Slot�� �����͸� ��ȯ�Ѵ�.
 *
 * aWASegment   - [IN] ��� Hash Segment
 * aFetchGrp    - [IN] ��� Group ��ü
 * aPageID      - [IN] ��� PageID
 * aOffset      - [IN] ��� Offset
 * aNSlotPtr    - [OUT] ���� ����
 * aFixedWCBPtr - [OUT] Slot�� ���Ե� Page�� WCB
 ***************************************************************************/
IDE_RC sdtHashModule::getSlotPtr( sdtHashSegHdr * aWASegment,
                                  sdtHashGroup  * aFetchGrp,
                                  scPageID        aNPageID,
                                  scOffset        aOffset,
                                  UChar        ** aNSlotPtr,
                                  sdtWCB       ** aFixedWCBPtr )
{
    sdtWCB        * sWCBPtr;

    sWCBPtr = findWCB( aWASegment, aNPageID );

    if ( sWCBPtr == NULL )
    {
        IDE_TEST( getFreeWAPage( aWASegment,
                                 aFetchGrp,
                                 &sWCBPtr )
                  != IDE_SUCCESS );

        IDE_TEST( readNPage( aWASegment,
                             sWCBPtr,
                             aNPageID )
                  != IDE_SUCCESS );
    }

    IDE_DASSERT( sWCBPtr->mWAPagePtr != NULL );
    IDE_DASSERT( sWCBPtr->mNPageID == aNPageID );

    if ( aFixedWCBPtr != NULL )
    {
        *aFixedWCBPtr = sWCBPtr;
    }

    IDE_DASSERT( aOffset < SD_PAGE_SIZE );
    *aNSlotPtr = sWCBPtr->mWAPagePtr + aOffset ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_DUMP_0,
                 "aNSpaceID  : %"ID_UINT32_FMT"\n"
                 "aNPID      : %"ID_UINT32_FMT"\n"
                 "sWPID      : %"ID_UINT32_FMT"\n",
                 aWASegment->mSpaceID,
                 aNPageID,
                 (sWCBPtr != NULL) ? sdtHashModule::getWPageID( aWASegment, sWCBPtr ) : SD_NULL_PID );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : TempPage�� Header�� �ʱ�ȭ �Ѵ�.
 *
 * aPageHdr   - [IN] �ʱ�ȭ �� Page�� Pointer
 * aType      - [IN] Page�� Type
 * aSelfPID   - [IN] Page�� Normal PageID
 ***************************************************************************/
void sdtHashModule::initTempPage( sdtHashPageHdr  * aPageHdr,
                                  sdtTempPageType   aType,
                                  scPageID          aSelfPID )
{
    aPageHdr->mSelfNPID   = aSelfPID;
    aPageHdr->mType       = aType ;
    aPageHdr->mFreeOffset = ID_SIZEOF( sdtHashPageHdr );
}

/**************************************************************************
 * Description : WCB�� �ش� SpaceID �� PageID�� �����Ѵ�.
 *               �� Page�� �д� ���� �ƴ϶� ������ �� ������,
 *               Disk�� �ִ� �Ϲ�Page�� ������ '����'�ȴ�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aWCBPtr      - [IN] ��� WCBPtr
 * aNPageID     - [IN] ��� NPage�� �ּ�
 ***************************************************************************/
IDE_RC sdtHashModule::assignNPage(sdtHashSegHdr* aWASegment,
                                  sdtWCB       * aWCBPtr,
                                  scPageID       aNPageID)
{
    UInt             sHashValue;

    /* ����. �̹� Hash�� �Ŵ޸� ���� �ƴϾ�� �� */
    IDE_DASSERT( findWCB( aWASegment, aNPageID ) == NULL );
    /* ����ε� WPID���� �� */
    IDE_DASSERT( getWPageID( aWASegment, aWCBPtr ) < getMaxWAPageCount( aWASegment ) );

    /* NPID �� ������ */
    IDE_ERROR( aWCBPtr->mNPageID   == SC_NULL_PID );
    IDE_ERROR( aWCBPtr->mWAPagePtr != NULL );

    aWCBPtr->mNPageID = aNPageID;
    aWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;

    /* Hash�� �����ϱ� */
    sHashValue = getNPageHashValue( aWASegment, aNPageID );
    aWCBPtr->mNextWCB4Hash = aWASegment->mNPageHashPtr[ sHashValue ];
    aWASegment->mNPageHashPtr[ sHashValue ] = aWCBPtr;

    /* Hash�� �Ŵ޷���, Ž���Ǿ�� �� */
    IDE_DASSERT( findWCB( aWASegment, aNPageID ) == aWCBPtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : NPage�� ����. �� WPage�� �÷��� �ִ� ���� ������.
 *               ���� �ʱ�ȭ�ϰ� Hash���� �����ϴ� �� �ۿ� ���Ѵ�.
 *
 * aWASegment  - [IN] ��� WASegment
 * aWPID       - [IN] ��� WPID
 ***************************************************************************/
IDE_RC sdtHashModule::unassignNPage( sdtHashSegHdr* aWASegment,
                                     sdtWCB       * aWCBPtr )
{
    UInt             sHashValue = 0;
    scPageID         sTargetNPID;
    sdtWCB        ** sWCBPtrPtr;
    sdtWCB         * sWCBPtr = aWCBPtr;

    /* Fix�Ǿ����� �ʾƾ� �� */
    IDE_ERROR( sWCBPtr->mFix == 0 );
    IDE_ERROR( sWCBPtr->mNPageID  != SC_NULL_PID );

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
 * Description : bindPage�ϰ� Disk���� �Ϲ� Page�κ��� ������ �о� �ø���.
 *
 * aWASegment   - [IN] ��� WASegment
 * aWCBPtr      - [IN] �о�帱�� ��� �� WCB
 * aNPageID     - [IN] ��� NPage�� �ּ�
 ***************************************************************************/
IDE_RC sdtHashModule::readNPage( sdtHashSegHdr* aWASegment,
                                 sdtWCB       * aWCBPtr,
                                 scPageID       aNPageID )
{
    IDE_DASSERT( aWCBPtr->mNxtUsedWCBPtr != NULL );
    IDE_DASSERT( aWCBPtr->mWAPagePtr != NULL );

    /* ������ ���� �������� Read�ؼ� �ø� */
    IDE_TEST( assignNPage( aWASegment,
                           aWCBPtr,
                           aNPageID ) != IDE_SUCCESS );

    IDE_TEST( sddDiskMgr::read( aWASegment->mStatistics,
                                aWASegment->mSpaceID,
                                aNPageID,
                                1,
                                aWCBPtr->mWAPagePtr ) != IDE_SUCCESS );

    // TC/FIT/Server/sm/Bugs/BUG-45263/BUG-45263.tc
    IDU_FIT_POINT( "BUG-45263@sdtHashModule::readNPage::iduFileopen" );

    // ù��° ��ġ�� ������ self PID
    IDE_ASSERT( aNPageID == ((sdtHashPageHdr*)aWCBPtr->mWAPagePtr)->mSelfNPID );

    aWASegment->mStatsPtr->mReadCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Disk���� �Ϲ� Page�κ��� ���ӵ� ���� page�� ������ �о� �ø���.
 *
 * aWASegment   - [IN] ��� WASegment
 * aFstNPageID  - [IN] �о���� ���ӵ� ��� NPage ���� ù��° Page�� �ּ�
 * aPageCount   - [IN] �о���� page�� ��
 * aFstWCBPtr   - [IN] �о�帱�� ��� �� WCB
 * aNeedFixPage - [IN] Page Fix ������ ��Ÿ����.
 ***************************************************************************/
IDE_RC sdtHashModule::readNPages( sdtHashSegHdr* aWASegment,
                                  scPageID       aFstNPageID,
                                  UInt           aPageCount,
                                  sdtWCB       * aFstWCBPtr,
                                  idBool         aNeedFixPage )
{
    scPageID     sNPageID;
    scPageID     sLstNPageID;
    sdtWCB     * sWCBPtr;
    sdtWCB     * sNxtWCBPtr;

    IDE_DASSERT( aFstWCBPtr->mNxtUsedWCBPtr != NULL );
    IDE_DASSERT( aFstWCBPtr->mWAPagePtr     != NULL );

    IDE_TEST( sddDiskMgr::read( aWASegment->mStatistics,
                                aWASegment->mSpaceID,
                                aFstNPageID,
                                aPageCount,
                                aFstWCBPtr->mWAPagePtr ) != IDE_SUCCESS );

    aWASegment->mStatsPtr->mReadCount += aPageCount;

    sWCBPtr = aFstWCBPtr;
    sLstNPageID = aFstNPageID + aPageCount;

    for( sNPageID = aFstNPageID ; sNPageID < sLstNPageID ; sNPageID++ )
    {
        IDE_ASSERT( sWCBPtr != NULL );
        IDE_ASSERT( sWCBPtr->mWAPagePtr != NULL );
        IDE_ASSERT( sNPageID == ((sdtHashPageHdr*)sWCBPtr->mWAPagePtr)->mSelfNPID );

        sNxtWCBPtr = sWCBPtr->mNextWCB4Hash;
        sWCBPtr->mNextWCB4Hash = NULL;

        IDE_TEST( assignNPage( aWASegment,
                               sWCBPtr,
                               sNPageID ) != IDE_SUCCESS );

        if ( aNeedFixPage == ID_TRUE )
        {
            fixWAPage( sWCBPtr );
        }

        sWCBPtr = sNxtWCBPtr;
    }
    IDE_DASSERT( sWCBPtr == NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Disk���� Page�� Extent ������ ������ �о� �ø���.
 *               AIX�� preadv �� ��� �� �� �����Ƿ� pread�� ����Ѵ�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aFstNPageID  - [IN] �о�帱 ù��° NPage�� �ּ�
 * aPageCount   - [IN] Page�� ��
 ***************************************************************************/
IDE_RC sdtHashModule::preReadNExtents( sdtHashSegHdr * aWASegment,
                                       scPageID        aFstNPageID,
                                       UInt            aPageCount )
{
    UInt         sCount = 0;
    scPageID     sCurNPageID;
    scPageID     sFstNPageID = SM_NULL_PID;
    scPageID     sEndNPageID = aFstNPageID + aPageCount;
    sdtWCB     * sWCBPtr;
    sdtWCB     * sPrvWCBPtr = NULL;
    sdtWCB     * sFstWCBPtr = NULL;

    for( sCurNPageID = aFstNPageID;
         sCurNPageID < sEndNPageID ;
         sCurNPageID++ )
    {
        sWCBPtr = findWCB( aWASegment, sCurNPageID );

        // WorkArea�� Page�� ����.
        if ( sWCBPtr == NULL )
        {
            if ( sFstNPageID == SM_NULL_PID )
            {
                /* ���ӵ� ù��° page�̴�. */
                sFstNPageID = sCurNPageID;
            }

            sWCBPtr = getFreePage4PreRead( aWASegment,
                                           aFstNPageID,
                                           sEndNPageID,
                                           sPrvWCBPtr );

            IDE_DASSERT( sWCBPtr != sPrvWCBPtr );

            if ( sWCBPtr->mNPageID != SM_NULL_PID )
            {
                IDE_TEST( unassignNPage( aWASegment,
                                         sWCBPtr ) != IDE_SUCCESS );

                initWCB( sWCBPtr );
            }

            IDE_DASSERT( sWCBPtr->mNextWCB4Hash == NULL );

            if ( sPrvWCBPtr == NULL )
            {
                // ù��° WCB
                sFstWCBPtr = sWCBPtr;
            }
            else
            {
                if (( sPrvWCBPtr->mWAPagePtr + SD_PAGE_SIZE ) == sWCBPtr->mWAPagePtr )
                {
                    // sPrevWCB�� mWAPagePtr�� Pointer�� ���ӵǾ� �ִ�.
                    IDE_DASSERT( sPrvWCBPtr->mNextWCB4Hash == NULL );
                    sPrvWCBPtr->mNextWCB4Hash = sWCBPtr;
                }
                else
                {
                    // sPrevWCB�� mWAPagePtr�� Pointer�� ���ӵ��� �ʾҴ�.
                    // FstWCB ���� PrevWCB ���� �о���̰�,
                    // ��� ������ sWCBPtr ���� �ٽ� counting �Ѵ�.
                    IDE_TEST( readNPages( aWASegment,
                                          sFstNPageID,
                                          sCount,
                                          sFstWCBPtr,
                                          ID_TRUE ) != IDE_SUCCESS );
                    sCount = 0;
                    sFstNPageID = sCurNPageID;
                    sFstWCBPtr = sWCBPtr;
                }
            }
            sCount++;
            sPrvWCBPtr = sWCBPtr;
        }
        else
        {
            // Work Area�� �ش� Page�� �ִٸ� Fix�� �д�.
            fixWAPage( sWCBPtr );

            if ( sCount > 0 )
            {
                // TC/FIT/Server/sm/Bugs/BUG-45263/BUG-45263.tc
                IDU_FIT_POINT( "1.BUG-45263@sdtHashModule::preReadNExtent::iduFileopen" );

                IDE_TEST( readNPages( aWASegment,
                                      sFstNPageID,
                                      sCount,
                                      sFstWCBPtr,
                                      ID_TRUE ) != IDE_SUCCESS );
                sCount = 0;
                sFstNPageID = SM_NULL_PID;
                sPrvWCBPtr  = NULL;
                sFstWCBPtr  = NULL;
            }
        }
    }

    if ( sCount > 0 )
    {
        // TC/FIT/Server/sm/Bugs/BUG-45263/BUG-45263.tc
        IDU_FIT_POINT( "2.BUG-45263@sdtHashModule::preReadNExtent::iduFileopen" );
        IDE_TEST( readNPages( aWASegment,
                              sFstNPageID,
                              sCount,
                              sFstWCBPtr,
                              ID_TRUE ) != IDE_SUCCESS );
    }

#ifdef DEBUG
    for( sCurNPageID = aFstNPageID ; sCurNPageID < sEndNPageID ; sCurNPageID++ )
    {
        sWCBPtr = findWCB( aWASegment,
                           sCurNPageID );
        if ( sWCBPtr != NULL )
        {
            IDE_ASSERT(((sdtHashPageHdr*)sWCBPtr->mWAPagePtr)->mSelfNPID == sCurNPageID );
            IDE_ASSERT( sWCBPtr->mNPageID == sCurNPageID );
        }
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : preReadNExtents() �� �� Page�� ã�´�.
 *
 * aWASegment   - [IN] ��� WASegment
 * aFstNPageID  - [IN] skip��� NPageID�� ù��° pageID
 * aEndNPageID  - [IN] skip��� NPageID�� ������ pageID
 * aTailWCBPtr  - [IN] ���� Page�� Pointer
 ***************************************************************************/
sdtWCB * sdtHashModule::getFreePage4PreRead( sdtHashSegHdr * aWASegment,
                                             scPageID        aFstSkipNPageID,
                                             scPageID        aEndSkipNPageID,
                                             sdtWCB        * aTailWCBPtr )
{
    sdtWCB     * sWCBPtr;
    sdtWCB     * sEmptyWCBPtr;

    sWCBPtr = getWCBPtr( aWASegment,
                         aWASegment->mFetchGrp.mReuseSeqWPID );

    while (1)
    {
        if ( sWCBPtr->mWAPagePtr != NULL )
        {
            if( sWCBPtr->mNPageID != SD_NULL_PID )
            {
                /* ���� dirty��� write�Ѵ�,
                 * Write �Ǿ����� Ȯ�� �� �ʿ�� ����.*/
                if ( sWCBPtr->mWPState == SDT_WA_PAGESTATE_DIRTY )
                {
                    (void)writeNPage( aWASegment,
                                      sWCBPtr );
                }
                /* �Ҵ�Ǿ��ִ� page�̸� ���� page���� Ȯ���Ѵ�.*/
                if(( sWCBPtr->mWPState != SDT_WA_PAGESTATE_DIRTY ) &&
                   ( SDT_IS_NOT_FIXED_PAGE( sWCBPtr ) ) &&
                   (( sWCBPtr->mNPageID <  aFstSkipNPageID ) ||
                    ( sWCBPtr->mNPageID >= aEndSkipNPageID )))
                {
                    /* ã�Ҵ� */
                    break;
                }
            }
            else
            {
                /* NPage�� NULL�ε� Next Hash�� NULL�� �ƴϸ�
                 * �̹� vec�� ������ WCB�̴�.
                 * �ѹ��� ���Ƽ� aTailWCBPtr�� �������� �ִ�.*/
                if (( sWCBPtr->mNextWCB4Hash == NULL ) &&
                    ( sWCBPtr != aTailWCBPtr ))
                {
                    break;
                }
                else
                {
                    /* �ѹ��� ���Ƽ� sFstWCBPtr ���� �͹��ȴ�.
                     * Hash Area Size�� ������ �߻� �� �� �ִ�.
                     * �ٽ� ã�´�.*/
                }
            }

            /* ���� page�� �Ѿ��. */
            aWASegment->mFetchGrp.mReuseSeqWPID++;
            sWCBPtr++;
        }
        else
        {
            /* �� page�̴�. wapage�� extent������ �Ҵ��ϹǷ�
             * sWCBPtr�� extent ù��° page�� ������ ����������,
             * ���� page �� ������ ����������, �׷��� Ȯ���Ѵ�. */
            sEmptyWCBPtr = sWCBPtr - ( aWASegment->mFetchGrp.mReuseSeqWPID % SDT_WAEXTENT_PAGECOUNT );
            IDE_DASSERT( sEmptyWCBPtr->mWAPagePtr == NULL );
            IDE_DASSERT( (sEmptyWCBPtr+SDT_WAEXTENT_PAGECOUNT-1)->mWAPagePtr == NULL );

            /* Total wa�� ���� �� ��� �Ҵ� ���� �� �� �ִ�.
             * reused seq�� begin���� �ǵ����� ó������ �ٽ� Ȯ���Ѵ�.
             * ������� ������ insert�� ��� �Ǵ� page�� �������̴�.*/
            if( set64WAPagesPtr( aWASegment,
                                 sEmptyWCBPtr ) == IDE_SUCCESS )
            {
                /* �Ҵ� ���� ������ �� page�̴�.
                 * �ƴϸ� ó������ �ٽ� ã�´�.*/
                break;
            }
        }

        /* ���� page�� �Ѿ���µ� End�� �Ѿ�ų�
         * wapage�� �Ҵ�޴µ� ������ ���
         * ó������ �ٽ� ã�´�. */
        if (( aWASegment->mFetchGrp.mReuseSeqWPID >= aWASegment->mFetchGrp.mEndWPID ) ||
            ( sWCBPtr->mWAPagePtr == NULL ))
        {
            aWASegment->mFetchGrp.mReuseSeqWPID = aWASegment->mFetchGrp.mBeginWPID;

            sWCBPtr = getWCBPtr( aWASegment,
                                 aWASegment->mFetchGrp.mReuseSeqWPID );

        }
    }

    aWASegment->mFetchGrp.mReuseSeqWPID++;

    if ( aWASegment->mFetchGrp.mReuseSeqWPID >= aWASegment->mFetchGrp.mEndWPID )
    {
        aWASegment->mFetchGrp.mReuseSeqWPID = aWASegment->mFetchGrp.mBeginWPID;
    }

    return sWCBPtr;
}

/**************************************************************************
 * Description : NPage�� �Ҵ�ް� �����Ѵ�.
 *
 * aWASegment      - [IN] ��� WASegment
 * aNExtFstPIDList - [IN] �Ҵ� ���� NExtent 
 * aTargetWCBPtr   - [IN] ������ WCBPtr
 ***************************************************************************/
IDE_RC sdtHashModule::allocAndAssignNPage( sdtHashSegHdr    * aWASegment,
                                           sdtNExtFstPIDList* aNExtFstPIDList,
                                           sdtWCB           * aTargetWCBPtr )
{
    scPageID sFreeNPID;

    if ( aNExtFstPIDList->mPageSeqInLFE == SDT_WAEXTENT_PAGECOUNT )
    {
        /* ������ NExtent�� �ٽ��� , ��ũ extent�� �ϳ�  �� �� �޾ƿ´�. */
        IDE_TEST( sdtWAExtentMgr::allocFreeNExtent( aWASegment->mStatistics,
                                                    aWASegment->mStatsPtr,
                                                    aWASegment->mSpaceID,
                                                    aNExtFstPIDList )!= IDE_SUCCESS );
        
    }
    else
    {
        /* ������ �Ҵ��ص� Extent���� ������ */
    }

    sFreeNPID = aNExtFstPIDList->mLastFreeExtFstPID
        + aNExtFstPIDList->mPageSeqInLFE;
    aNExtFstPIDList->mPageSeqInLFE++;

    ((sdtHashPageHdr*)aTargetWCBPtr->mWAPagePtr)->mSelfNPID = sFreeNPID;

    if ( aWASegment->mNPageHashPtr == NULL )
    {
        aTargetWCBPtr->mNPageID = sFreeNPID;
        aTargetWCBPtr->mWPState = SDT_WA_PAGESTATE_CLEAN;
    }
    else
    {
        IDE_TEST( assignNPage( aWASegment,
                               aTargetWCBPtr,
                               sFreeNPID ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : Total WA���� WAExtent�� �Ҵ�޾� �´�.
 *
 * aWASegment     - [IN] ��� WASegment
 ***************************************************************************/
IDE_RC sdtHashModule::expandFreeWAExtent( sdtHashSegHdr* aWASeg )
{
    SInt          sExtentCount;
    sdtWAExtent * sTailExtent;

    IDE_ERROR( aWASeg->mMaxWAExtentCount > aWASeg->mWAExtentInfo.mCount );

    sExtentCount = smuProperty::getTmpAllocWAExtCnt();

    if ( aWASeg->mMaxWAExtentCount < ( sExtentCount + aWASeg->mWAExtentInfo.mCount ) )
    {
        sExtentCount = aWASeg->mMaxWAExtentCount - aWASeg->mWAExtentInfo.mCount;
    }
    IDE_ERROR( sExtentCount > 0 );

    sTailExtent = aWASeg->mWAExtentInfo.mTail;

    IDE_DASSERT( sTailExtent->mNextExtent == NULL );

    IDE_TEST( sdtWAExtentMgr::allocWAExtents( &aWASeg->mWAExtentInfo,
                                              sExtentCount ) != IDE_SUCCESS );

    IDE_DASSERT( sTailExtent->mNextExtent != NULL );

    aWASeg->mNxtFreeWAExtent = sTailExtent->mNextExtent;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : HashSlot������ �Ѱ��� WAPage�� WCB�� �Ҵ��Ѵ�
 *
 * aWASegment  - [IN] ��� WASegment
 * aWCBPtr     - [IN] �Ҵ��� ù��° WCB�� Pointer  
 ***************************************************************************/
IDE_RC sdtHashModule::setWAPagePtr4HashSlot( sdtHashSegHdr * aWASegment,
                                             sdtWCB        * aWCBPtr )
{
    IDE_DASSERT( aWCBPtr->mWAPagePtr == NULL );
    IDE_DASSERT( aWCBPtr->mNxtUsedWCBPtr == NULL );

    if ( aWASegment->mCurrFreePageIdx >= SDT_WAEXTENT_PAGECOUNT )
    {
        if ( aWASegment->mNxtFreeWAExtent == NULL )
        {
            // Extent�� 50%�� ���δ�.
            if ( expandFreeWAExtent( aWASegment ) != IDE_SUCCESS )
            {
                // �뷮 ���������� ��õ� �Ѵ�.
                IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_WORKAREA );

                IDE_TEST( sdtWAExtentMgr::memAllocWAExtent( &aWASegment->mWAExtentInfo )
                          != IDE_SUCCESS );
                aWASegment->mStatsPtr->mOverAllocCount++;

                // �ٷ� �Ʒ����� cur�� �Ű����� null�� �缳�� �ȴ�.
                aWASegment->mNxtFreeWAExtent = aWASegment->mWAExtentInfo.mTail;
            }
        }

        aWASegment->mCurrFreePageIdx = 0;
        aWASegment->mCurFreeWAExtent = aWASegment->mNxtFreeWAExtent;
        aWASegment->mNxtFreeWAExtent = aWASegment->mNxtFreeWAExtent->mNextExtent;
    }

    aWCBPtr->mWAPagePtr     = aWASegment->mCurFreeWAExtent->mPage[ aWASegment->mCurrFreePageIdx ];
    aWCBPtr->mNxtUsedWCBPtr = aWASegment->mUsedWCBPtr;
    aWASegment->mUsedWCBPtr = aWCBPtr;

    idlOS::memset( aWCBPtr->mWAPagePtr, 0, SD_PAGE_SIZE );

    aWASegment->mCurrFreePageIdx++;
    aWASegment->mAllocWAPageCount++;
    aWASegment->mHashSlotAllocPageCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : WCB�� WAPage�� WAExtent������ �Ҵ��Ѵ�
 *
 * aWASegment  - [IN] ��� WASegment
 * aWCBPtr     - [IN] �Ҵ��� ù��° WCB�� Pointer  
 ***************************************************************************/
IDE_RC sdtHashModule::set64WAPagesPtr( sdtHashSegHdr * aWASegment,
                                       sdtWCB        * aWCBPtr )
{
    UInt i;

    IDE_DASSERT(( getWPageID( aWASegment, aWCBPtr ) % SDT_WAEXTENT_PAGECOUNT ) == 0 );

    if ( aWASegment->mNxtFreeWAExtent == NULL )
    {
        IDE_TEST( expandFreeWAExtent( aWASegment ) != IDE_SUCCESS );
    }

    for( i = 0;
         i < SDT_WAEXTENT_PAGECOUNT;
         aWCBPtr++, i++ )
    {
        IDE_DASSERT( aWCBPtr->mWAPagePtr     == NULL );
        IDE_DASSERT( aWCBPtr->mNxtUsedWCBPtr == NULL );

        aWCBPtr->mNxtUsedWCBPtr = aWASegment->mUsedWCBPtr;
        aWASegment->mUsedWCBPtr = aWCBPtr;

        aWCBPtr->mWAPagePtr = aWASegment->mNxtFreeWAExtent->mPage[i];
    }

    aWASegment->mNxtFreeWAExtent = aWASegment->mNxtFreeWAExtent->mNextExtent;

    aWASegment->mAllocWAPageCount += SDT_WAEXTENT_PAGECOUNT ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description : WASegemnt ������ File�� Dump�Ѵ�.
 *
 * aWASegment   - [IN] ��� WASegment
 ***************************************************************************/
void sdtHashModule::exportHashSegmentToFile( sdtHashSegHdr* aWASegment )
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
              != IDE_SUCCESS );
    sState = 1;

    smuUtility::getTimeString( smiGetCurrTime(),
                               SMI_TT_STR_SIZE,
                               sDateString );

    idlOS::snprintf( sFileName,
                     SM_MAX_FILE_NAME,
                     "%s%c%s_hash.td",
                     smuProperty::getTempDumpDirectory(),
                     IDL_FILE_SEPARATOR,
                     sDateString );

    IDE_TEST( sFile.setFileName( sFileName ) != IDE_SUCCESS );
    IDE_TEST( sFile.createUntilSuccess( smiSetEmergency )
              != IDE_SUCCESS );
    IDE_TEST( sFile.open( ID_FALSE ) != IDE_SUCCESS ); //DIRECT_IO
    sState = 2;

    // 1. sdtHashSegHdr Pointer (���߿� pointer �������� ���)
    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           0,
                           (UChar*)&aWASegment,
                           ID_SIZEOF(sdtHashSegHdr*) )
              != IDE_SUCCESS );
    sOffset += ID_SIZEOF(sdtHashSegHdr*);

    // sdtHashSegHdr+ WCB Map
    sMaxPageCount = getMaxWAPageCount( aWASegment );

    sSize = calcWASegmentSize( sMaxPageCount );

    // 2. sdtHashSegHdr + WCB Array + WAFlushQueue
    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           sOffset,
                           (UChar*)aWASegment,
                           sSize )
              != IDE_SUCCESS );
    sOffset += sSize;

    // Normal page Hash Map
    if ( aWASegment->mNPageHashPtr != NULL )
    {
        // 3. n page hash map
        sSize = aWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*);
        IDE_TEST( sFile.write( aWASegment->mStatistics,
                               sOffset,
                               (UChar*)aWASegment->mNPageHashPtr,
                               sSize )
                  != IDE_SUCCESS );
        sOffset += sSize;
    }

    // 4. Normal Extent Map for Row
    for ( sNExtentArr =  aWASegment->mNExtFstPIDList4Row.mHead;
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

    // 5. Normal Extent Map for SubHash
    for ( sNExtentArr =  aWASegment->mNExtFstPIDList4SubHash.mHead;
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

    // 6. wa page ( wapageID, page )
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

    // 7. special pid (the end)
    sWPageID = SM_SPECIAL_PID;
    IDE_TEST( sFile.write( aWASegment->mStatistics,
                           sOffset,
                           (UChar*)&sWPageID,
                           ID_SIZEOF(scPageID) )
              != IDE_SUCCESS );

    IDE_TEST( sFile.sync() != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( sFile.close() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sFile.destroy() != IDE_SUCCESS );

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

/***************************************************************************
 * Description : File�κ��� WASegment�� �о���δ�.
 *
 * aFileName      - [IN]  ��� File
 * aHashSegHdr    - [OUT] �о���� WASegment�� ������ ���
 * aRawHashSegHdr - [OUT] �о���� ������ �״�� �����Ѵ�.
 * aPtr           - [OUT] �о���� Buffer
 ***************************************************************************/
IDE_RC sdtHashModule::importHashSegmentFromFile( SChar         * aFileName,
                                                 sdtHashSegHdr** aHashSegHdr,
                                                 sdtHashSegHdr * aRawHashSegHdr,
                                                 UChar        ** aPtr )
{
    iduFile         sFile;
    UInt            sState          = 0;
    UInt            sMemAllocState  = 0;
    ULong           sSize = 0;
    sdtHashSegHdr * sWASegment = NULL;
    UChar         * sPtr;
    ULong           sMaxPageCount;
    UChar         * sOldWASegPtr;
    sdtNExtentArr * sNExtentArr;

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
    // 1. Old sdtHashSegHdr Pointer
    sOldWASegPtr = *(UChar**)sPtr;

    sPtr += ID_SIZEOF(sdtHashSegHdr*);

    // 2. sdtHashSegHdr + WCB Map
    sWASegment = (sdtHashSegHdr*)sPtr;

    // ���� �ִ� �״�� ��� �� �ʿ䵵 �ִ�.
    idlOS::memcpy((UChar*)aRawHashSegHdr,
                  (UChar*)sWASegment,
                  SDT_HASH_SEGMENT_HEADER_SIZE);

    sMaxPageCount = getMaxWAPageCount( sWASegment ) ;
    sPtr += calcWASegmentSize( sMaxPageCount );

    IDE_ERROR( (ULong)sPtr % ID_SIZEOF(ULong) == 0);

    // 3. Normal page Hash Map
    if ( sWASegment->mNPageHashPtr != NULL )
    {
        sWASegment->mNPageHashPtr = (sdtWCB**)sPtr;
        sPtr += sWASegment->mNPageHashBucketCnt * ID_SIZEOF(sdtWCB*);
    }

    // 4. Normal Extent Map for Row
    if ( sWASegment->mNExtFstPIDList4Row.mHead != NULL )
    {
        sWASegment->mNExtFstPIDList4Row.mHead = (sdtNExtentArr*)sPtr;
        sPtr += ID_SIZEOF( sdtNExtentArr );

        for ( sNExtentArr =  sWASegment->mNExtFstPIDList4Row.mHead;
              sNExtentArr->mNextArr != NULL;
              sNExtentArr = sNExtentArr->mNextArr )
        {
            sNExtentArr->mNextArr = (sdtNExtentArr*)sPtr;
            sPtr += ID_SIZEOF( sdtNExtentArr );
        }
        sWASegment->mNExtFstPIDList4Row.mTail = sNExtentArr;
    }

    // 5. Normal Extent Map for SubHash
    if ( sWASegment->mNExtFstPIDList4SubHash.mHead != NULL )
    {
        sWASegment->mNExtFstPIDList4SubHash.mHead = (sdtNExtentArr*)sPtr;
        sPtr += ID_SIZEOF( sdtNExtentArr );

        for ( sNExtentArr =  sWASegment->mNExtFstPIDList4SubHash.mHead;
              sNExtentArr->mNextArr != NULL;
              sNExtentArr = sNExtentArr->mNextArr )
        {
            sNExtentArr->mNextArr = (sdtNExtentArr*)sPtr;
            sPtr += ID_SIZEOF( sdtNExtentArr );
        }
        sWASegment->mNExtFstPIDList4SubHash.mTail = sNExtentArr;
    }

    IDE_TEST( resetWCBInfo4Dump( sWASegment,
                                 sOldWASegPtr,
                                 sMaxPageCount,
                                 &sPtr )
              != IDE_SUCCESS );

    if ( sWASegment->mUsedWCBPtr != NULL )
    {
        sWASegment->mUsedWCBPtr =
            (sdtWCB*)(((UChar*)sWASegment->mUsedWCBPtr)
                      - sOldWASegPtr + (UChar*)sWASegment) ;
    }

    if ( sWASegment->mInsertHintWCBPtr != NULL )
    {
        sWASegment->mInsertHintWCBPtr =
            (sdtWCB*)(((UChar*)sWASegment->mInsertHintWCBPtr)
                      - sOldWASegPtr + (UChar*)sWASegment) ;
    }
    if ( sWASegment->mSubHashWCBPtr != NULL )
    {
        sWASegment->mSubHashWCBPtr =
            (sdtWCB*)(((UChar*)sWASegment->mSubHashWCBPtr)
                      - sOldWASegPtr + (UChar*)sWASegment) ;
    }

    sState = 1;
    IDE_ERROR( sFile.close() == IDE_SUCCESS );
    sState = 0;
    IDE_ERROR( sFile.destroy() == IDE_SUCCESS );

    *aHashSegHdr  = sWASegment;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sMemAllocState == 1 )
    {
        IDE_ASSERT( iduMemMgr::free( sPtr ) == IDE_SUCCESS );
        sPtr = NULL;
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
 * aWASegment     - [IN] �籸�� ���� Hash Segment
 * aOldWASegPtr   - [IN] Dump �ϱ� ���� Hash Segment�� Pointer
 * aMaxPageCount  - [IN] Page�� Count
 * aPtr           - [IN] �о���� Buffer
 ***************************************************************************/
IDE_RC sdtHashModule::resetWCBInfo4Dump( sdtHashSegHdr * aWASegment,
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
/***************************************************************************
 * Description : Hash Segment�� Dump �Ѵ�.
 *
 * aWASegment - [IN] dump ��� Hash WASegment
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpHashSegment( sdtHashSegHdr * aWASegment,
                                     SChar         * aOutBuf,
                                     UInt            aOutSize )
{
    scPageID  sUsedWPageID = 0;
    scPageID  sInsertHintWPageID = 0;
    scPageID  sSubHashWPageID = 0;

    if ( aWASegment->mUsedWCBPtr != NULL )
    {
        sUsedWPageID = getWPageID( aWASegment, aWASegment->mUsedWCBPtr );
    }

    if ( aWASegment->mInsertHintWCBPtr != NULL )
    {
        sInsertHintWPageID = getWPageID( aWASegment, aWASegment->mInsertHintWCBPtr );
    }

    if ( aWASegment->mSubHashWCBPtr != NULL )
    {
        sSubHashWPageID = getWPageID( aWASegment, aWASegment->mSubHashWCBPtr );
    }

    (void)idlVA::appendFormat(
        aOutBuf,
        aOutSize,
        "DUMP HASH WASEGMENT:\n"
        "\tWASegPtr         : 0x%"ID_xINT64_FMT"\n"
        "\tSpaceID          : %"ID_UINT32_FMT"\n"
        "\tType             : %"ID_UINT32_FMT" %s\n"
        "\tHashSlotCount    : %"ID_UINT32_FMT"\n"

        "\tInMemory         : %s\n"

        "\tUnique           : %"ID_UINT32_FMT"\n"
        "\tOpenCursorType   : %"ID_UINT32_FMT"\n"
        "\tEndWPID          : %"ID_UINT32_FMT"\n"

        "\tUsedWCBPtr       : 0x%"ID_xPOINTER_FMT" (%"ID_UINT32_FMT")\n"
        "\tInsertHintWCBPtr : 0x%"ID_xPOINTER_FMT" (%"ID_UINT32_FMT")\n"

        "\tInsertGroup BeginWPID : %"ID_UINT32_FMT"\n"
        "\tInsertGroup EndWPID   : %"ID_UINT32_FMT"\n"
        "\tInsertGroup ReuseWPID : %"ID_UINT32_FMT"\n"

        "\tFetchGroup BeginWPID : %"ID_UINT32_FMT"\n"
        "\tFetchGroup EndWPID   : %"ID_UINT32_FMT"\n"
        "\tFetchGroup ReuseWPID : %"ID_UINT32_FMT"\n"

        "\tSubHashGroup BeginWPID : %"ID_UINT32_FMT"\n"
        "\tSubHashGroup EndWPID   : %"ID_UINT32_FMT"\n"
        "\tSubHashGroup ReuseWPID : %"ID_UINT32_FMT"\n"

        "\tSubHashWCBPtr    : 0x%"ID_xPOINTER_FMT" (%"ID_UINT32_FMT")\n"

        "\tWAExtentListHead : 0x%"ID_xPOINTER_FMT"\n"
        "\tWAExtentListTail : 0x%"ID_xPOINTER_FMT"\n"
        "\tWAExtentListCount: %"ID_UINT32_FMT"\n"

        "\tMaxWAExtentCount : %"ID_UINT32_FMT"\n"
        "\tNextFreeWAExtent : 0x%"ID_xPOINTER_FMT"\n"
        "\tCurrFreeWAExtent : 0x%"ID_xPOINTER_FMT"\n"
        "\tCurrFreePageIdx  : %"ID_UINT32_FMT"\n"
        "\tAllocWAPageCount : %"ID_UINT32_FMT"\n"

        "\tNPageHashBucketCnt: %"ID_UINT32_FMT"\n"
        "\tNPageHashBucket  : 0x%"ID_xPOINTER_FMT"\n"

        "\tNPageCount       : %"ID_UINT32_FMT"\n"

        "\tNExtentFstPID4Row\n"
        "\t Count           : %"ID_UINT32_FMT"\n"
        "\t PageSeqInLFE    : %"ID_UINT32_FMT"\n"
        "\t LstFreeExtFstPID: %"ID_UINT32_FMT"\n"
        "\t ExtentArrayHead : 0x%"ID_xPOINTER_FMT"\n"
        "\t ExtentArrayTail : 0x%"ID_xPOINTER_FMT"\n"

        "\tNExtentFstPID4SubHash\n"
        "\t Count           : %"ID_UINT32_FMT"\n"
        "\t PageSeqInLFE    : %"ID_UINT32_FMT"\n"
        "\t LstFreeExtFstPID: %"ID_UINT32_FMT"\n"
        "\t ExtentArrayHead : 0x%"ID_xPOINTER_FMT"\n"
        "\t ExtentArrayTail : 0x%"ID_xPOINTER_FMT"\n"

        "\tSubHashPageCount : %"ID_UINT32_FMT"\n"
        "\tSubHashBuildCount: %"ID_UINT32_FMT"\n"
        "\tHashSlotPageCount: %"ID_UINT32_FMT"\n"
        "\n",
        aWASegment,
        aWASegment->mSpaceID,
        aWASegment->mWorkType,
        ( aWASegment->mWorkType == SDT_WORK_TYPE_HASH ) ? "HASH" : "SORT",
        aWASegment->mHashSlotCount,
        aWASegment->mIsInMemory == SDT_WORKAREA_OUT_MEMORY ? "OutMemory" : "InMemory",
        aWASegment->mUnique,
        aWASegment->mOpenCursorType,
        aWASegment->mEndWPID,

        aWASegment->mUsedWCBPtr,
        sUsedWPageID,
        aWASegment->mInsertHintWCBPtr,
        sInsertHintWPageID,

        aWASegment->mInsertGrp.mBeginWPID,
        aWASegment->mInsertGrp.mEndWPID,
        aWASegment->mInsertGrp.mReuseSeqWPID,

        aWASegment->mFetchGrp.mBeginWPID,
        aWASegment->mFetchGrp.mEndWPID,
        aWASegment->mFetchGrp.mReuseSeqWPID,

        aWASegment->mSubHashGrp.mBeginWPID,
        aWASegment->mSubHashGrp.mEndWPID,
        aWASegment->mSubHashGrp.mReuseSeqWPID,

        aWASegment->mSubHashWCBPtr,
        sSubHashWPageID,

        aWASegment->mWAExtentInfo.mHead,
        aWASegment->mWAExtentInfo.mTail,
        aWASegment->mWAExtentInfo.mCount,

        aWASegment->mMaxWAExtentCount,

        aWASegment->mNxtFreeWAExtent,
        aWASegment->mCurFreeWAExtent,
        aWASegment->mCurrFreePageIdx,
        aWASegment->mAllocWAPageCount,

        aWASegment->mNPageHashBucketCnt,
        aWASegment->mNPageHashPtr,
        aWASegment->mNPageCount,

        aWASegment->mNExtFstPIDList4Row.mCount,
        aWASegment->mNExtFstPIDList4Row.mPageSeqInLFE,
        aWASegment->mNExtFstPIDList4Row.mLastFreeExtFstPID,
        aWASegment->mNExtFstPIDList4Row.mHead,
        aWASegment->mNExtFstPIDList4Row.mTail,

        aWASegment->mNExtFstPIDList4SubHash.mCount,
        aWASegment->mNExtFstPIDList4SubHash.mPageSeqInLFE,
        aWASegment->mNExtFstPIDList4SubHash.mLastFreeExtFstPID,
        aWASegment->mNExtFstPIDList4SubHash.mHead,
        aWASegment->mNExtFstPIDList4SubHash.mTail,

        aWASegment->mSubHashPageCount,
        aWASegment->mSubHashBuildCount,
        aWASegment->mHashSlotPageCount );
    return;
}

/***************************************************************************
 * Description : hash row header�� dump �Ѵ�.
 *
 * aTRPHeader - [IN] dump ��� Row Header
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ****************************************************************************/
void sdtHashModule::dumpTempTRPHeader( void     * aTRPHeader,
                                       SChar    * aOutBuf,
                                       UInt       aOutSize )
{
    sdtHashTRPHdr* sTRPHeader = (sdtHashTRPHdr*)aTRPHeader;

    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "DUMP TRPHeader:\n"
                               "mFlag        : %"ID_UINT32_FMT"\n"
                               "mHitSequence : %"ID_UINT32_FMT"\n"
                               "mValueLength : %"ID_UINT32_FMT"\n"
                               "mHashValue   : %"ID_XINT32_FMT"\n"
                               "mNext    : <%"ID_UINT32_FMT
                               ",%"ID_UINT32_FMT">\n"
                               "mChild   : <%"ID_UINT32_FMT
                               ",%"ID_UINT32_FMT">\n",
                               sTRPHeader->mTRFlag,
                               sTRPHeader->mHitSequence,
                               sTRPHeader->mValueLength,
                               sTRPHeader->mHashValue,
                               sTRPHeader->mNextPageID,
                               sTRPHeader->mNextOffset,
                               sTRPHeader->mChildPageID,
                               sTRPHeader->mChildOffset);
    return;
}

/***************************************************************************
 * Description : sub hash�� dump �Ѵ�.
 *
 * aSubHash   - [IN] dump ��� SubHash
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpSubHash( sdtSubHash * aSubHash,
                                 SChar      * aOutBuf,
                                 UInt         aOutSize )
{
    UInt i;

    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "DUMP SubHash :\n"
                               "mHashLow    : %"ID_XINT32_FMT"..\n"
                               "mKeyCount   : %"ID_UINT32_FMT"\n"
                               "mNextPageID : %"ID_UINT32_FMT"\n"
                               "mNextOffset : %"ID_UINT32_FMT"\n"
                               "%6s %8s %9s %6s %6s\n",
                               (UInt)aSubHash->mHashLow,
                               aSubHash->mKeyCount,
                               aSubHash->mNextPageID,
                               aSubHash->mNextOffset,
                               "KeyIdx",
                               "HashHigh",
                               "HashValue",
                               "PageID",
                               "Offset");

    for( i = 0 ; i < aSubHash->mKeyCount ; i++ )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   " [%3"ID_XINT32_FMT"] "
                                   "%8"ID_XINT32_FMT" "
                                   "%7"ID_XINT32_FMT".. "
                                   "%6"ID_UINT32_FMT" "
                                   "%6"ID_UINT32_FMT"\n",
                                   i,
                                   aSubHash->mKey[i].mHashHigh,
                                   (((UInt)aSubHash->mKey[i].mHashHigh << 16) +
                                    ( (UInt)aSubHash->mHashLow << 8 )),
                                   aSubHash->mKey[i].mPageID,
                                   aSubHash->mKey[i].mOffset );
    }

    return;
}

/***************************************************************************
 * Description : hash segment�� Ư�� WA Page�� dump�Ѵ�.
 *
 * aWASegment - [IN] dump ��� Page�� �ִ� Hash Table�� WASegment
 * aWAPageID  - [IN] dump ��� Page
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpHashTempPage( sdtHashSegHdr  * aWASegment,
                                      scPageID         aWAPageID,
                                      SChar          * aOutBuf,
                                      UInt             aOutSize )
{
    UChar * sPagePtr;
    UInt    sSize;

    if ( aWAPageID >= aWASegment->mEndWPID )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "\nWAPageID %"ID_UINT32_FMT" is too big.\n",
                                   aWAPageID );
        IDE_TEST( ID_TRUE );
    }

    sPagePtr = getWCBPtr( aWASegment,
                          aWAPageID )->mWAPagePtr;

    if ( sPagePtr == NULL )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "\nWAPageID %"ID_UINT32_FMT" is empty.\n",
                                   aWAPageID );
        IDE_TEST( ID_TRUE );
    }

    if ( aWAPageID < aWASegment->mHashSlotPageCount )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "\nDUMP HASH TEMP PAGE:\n" );

        sSize = idlOS::strlen( aOutBuf );

        IDE_TEST( ideLog::ideMemToHexStr( (UChar*)sPagePtr,
                                          SD_PAGE_SIZE,
                                          IDE_DUMP_FORMAT_FULL,
                                          aOutBuf + sSize,
                                          aOutSize - sSize )
                  != IDE_SUCCESS );

        (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n");

        // Hash Slot Page
        dumpWAHashPage( sPagePtr,
                        aWAPageID,
                        ID_TRUE,
                        aOutBuf,
                        aOutSize);
    }
    else
    {
        dumpHashRowPage( sPagePtr,
                         aOutBuf,
                         aOutSize );
    }

    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n");

    return;

    IDE_EXCEPTION_END;

    return;
}

/***************************************************************************
 * Description : Hash segment�� ��� hash page�� header�� dump�Ѵ�.
 *
 * aWASegment - [IN] dump ��� Hash WASegment
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpHashPageHeaders( sdtHashSegHdr  * aWASegment,
                                         SChar          * aOutBuf,
                                         UInt             aOutSize )
{
    sdtHashPageHdr * sPagePtr;
    sdtWCB         * sWCBPtr;
    sdtWCB         * sEndWCBPtr;
    scPageID         sWPageID;

    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "HASH TEMP PAGE HEADERS:\n"
                               "%10s %10s %16s %10s %10s %10s\n",
                               "WPID",
                               "TYPE",
                               "TYPENAME",
                               "FREEOFF",
                               "SELFPID",
                               "WCB->NPID" );

    sWCBPtr    = aWASegment->mWCBMap;
    sEndWCBPtr = sWCBPtr + aWASegment->mHashSlotPageCount ;

    for( sWPageID = 0 ; sWCBPtr < sEndWCBPtr; sWCBPtr++, sWPageID++ )
    {
        if ( sWCBPtr->mWAPagePtr != NULL )
        {
            (void)idlVA::appendFormat( aOutBuf,
                                       aOutSize,
                                       "%10"ID_UINT32_FMT
                                       " %27s\n",
                                       sWPageID,
                                       "HASH_SLOT_PAGE");
        }
    }

    sEndWCBPtr = aWASegment->mWCBMap + (aWASegment->mMaxWAExtentCount * SDT_WAEXTENT_PAGECOUNT);

    for( ; sWCBPtr < sEndWCBPtr; sWCBPtr++, sWPageID++ )
    {
        sPagePtr = (sdtHashPageHdr*)sWCBPtr->mWAPagePtr;

        if ( sPagePtr == NULL )
        {
            continue;
        }

        if ( sWCBPtr->mNPageID == SM_NULL_PID )
        {
            continue;
        }

        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "%10"ID_UINT32_FMT
                                   " %10"ID_UINT32_FMT,
                                   sWPageID,
                                   sPagePtr->mType );

        if ( SDT_TEMP_PAGETYPE_HASHROWS == sPagePtr->mType )
        {
            (void)idlVA::appendFormat( aOutBuf,
                                       aOutSize,
                                       "    HASH_ROW_PAGE" );

        }
        else if ( SDT_TEMP_PAGETYPE_SUBHASH == sPagePtr->mType )
        {
            (void)idlVA::appendFormat( aOutBuf,
                                       aOutSize,
                                       "    SUB_HASH_PAGE" );
        }
        else
        {
            (void)idlVA::appendFormat( aOutBuf,
                                       aOutSize,
                                       "                 " );
        }
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   " %10"ID_UINT32_FMT
                                   " %10"ID_UINT32_FMT
                                   " %10"ID_UINT32_FMT"\n",
                                   sPagePtr->mFreeOffset,
                                   sPagePtr->mSelfNPID,
                                   sWCBPtr->mNPageID );
    }

    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );

    return;
}

/***************************************************************************
 * Description : Hash Row Page�� dump�Ѵ�.
 *
 * aPagePtr   - [IN] dump ��� Hash Row Page
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpHashRowPage( void   * aPagePtr,
                                     SChar  * aOutBuf,
                                     UInt     aOutSize )
{
    sdtHashPageHdr * sPageHdr;
    UChar          * sPagePtr = (UChar*)aPagePtr;
    sdtSubHash     * sSubHash;
    sdtHashTRPHdr  * sTRPHeader;
    UInt             sIdx;
    UInt             sSize;
    scOffset         sCurOffset;


    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "\nDUMP HASH TEMP PAGE:\n" );
    sSize = idlOS::strlen( aOutBuf );
    IDE_TEST( ideLog::ideMemToHexStr( (UChar*)sPagePtr,
                                      SD_PAGE_SIZE,
                                      IDE_DUMP_FORMAT_FULL,
                                      aOutBuf + sSize,
                                      aOutSize - sSize )
              != IDE_SUCCESS );

    sPageHdr = (sdtHashPageHdr*)sPagePtr;
    (void)idlVA::appendFormat( aOutBuf,
                               aOutSize,
                               "\n\nDUMP TEMP PAGE HEADER:\n"
                               "mSelfNPID   : %"ID_UINT32_FMT"\n"
                               "mFreeOffset : %"ID_UINT32_FMT"\n"
                               "mType       : %"ID_UINT32_FMT"\n",
                               sPageHdr->mType,
                               sPageHdr->mFreeOffset,
                               sPageHdr->mSelfNPID );

    sIdx = 0;
    sCurOffset = ID_SIZEOF(sdtHashPageHdr) ;

    switch( sPageHdr->mType )
    {
        case SDT_TEMP_PAGETYPE_HASHROWS:
            {
                (void)idlVA::appendFormat( aOutBuf,
                                           aOutSize,
                                           "TypeName    : HASH_ROW_PAGE\n\n"
                                           "Row :\n" );

                while( sCurOffset < sPageHdr->mFreeOffset )
                {
                    sTRPHeader  = (sdtHashTRPHdr*)((UChar*)sPagePtr + sCurOffset );

                    (void)idlVA::appendFormat( aOutBuf,
                                               aOutSize,
                                               "[%4"ID_UINT32_FMT":%4"ID_UINT32_FMT"] ",
                                               sIdx++,
                                               sCurOffset );

                    sCurOffset += ID_SIZEOF(sdtHashTRPHdr) + sTRPHeader->mValueLength;

                    if ( ( sCurOffset & 0x07 ) != 0 )
                    {
                        sCurOffset += ( 0x08 - ( sCurOffset & 0x07 )) ; // XXX Align �Լ��� ����
                    }
                    dumpTempTRPHeader( sTRPHeader,
                                       aOutBuf,
                                       aOutSize );

                    (void)idlVA::appendFormat( aOutBuf,
                                               aOutSize,
                                               "Value : ");

                    sSize = idlOS::strlen( aOutBuf );

                    IDE_TEST( ideLog::ideMemToHexStr( (UChar*)sTRPHeader + ID_SIZEOF( sdtHashTRPHdr ),
                                                      sTRPHeader->mValueLength,
                                                      IDE_DUMP_FORMAT_VALUEONLY,
                                                      aOutBuf + sSize,
                                                      aOutSize - sSize )
                              != IDE_SUCCESS );

                    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n" );
                }
            }
            break;
        case SDT_TEMP_PAGETYPE_SUBHASH:
            {
                (void)idlVA::appendFormat( aOutBuf,
                                           aOutSize,
                                           "TypeName    : SUB_HASH_PAGE\n\n"
                                           "Sub Hash :\n" );

                while( sCurOffset < sPageHdr->mFreeOffset )
                {
                    sSubHash    = (sdtSubHash*)((UChar*)sPagePtr + sCurOffset );

                    (void)idlVA::appendFormat( aOutBuf,
                                               aOutSize,
                                               "[%4"ID_UINT32_FMT":%4"ID_UINT32_FMT"] ",
                                               sIdx++,
                                               sCurOffset );

                    sCurOffset += SDT_SUB_HASH_SIZE + (sSubHash->mKeyCount * ID_SIZEOF(sdtSubHashKey));
                    if ( ( sCurOffset & 0x07 ) != 0 )
                    {
                        sCurOffset += ( 0x08 - ( sCurOffset & 0x07 )) ;
                    }
                    dumpSubHash( sSubHash,
                                 aOutBuf,
                                 aOutSize );

                    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n" );
                }
            }
            break;
        default:
            break;

    }

    (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n");

    return;

    IDE_EXCEPTION_END;

    return;
}

/***************************************************************************
 * Description : Hash Map�� dump�Ѵ�.
 *
 * aWASegment - [IN] dump ��� Hash WASegment
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpWAHashMap( sdtHashSegHdr * aWASeg,
                                   SChar         * aOutBuf,
                                   UInt            aOutSize )
{
    sdtWCB     * sWCBPtr;
    sdtWCB     * sEndWCBPtr;
    scPageID     sWAPageID;
    idBool       sTitle = ID_TRUE;

    if ( aWASeg != NULL )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "DUMP WAMAP: HASH\n"
                                   "       BeginWPID:0\n"
                                   "       EndWPID  :%"ID_UINT32_FMT"\n"
                                   "       AllocPage:%"ID_UINT32_FMT"\n",
                                   aWASeg->mHashSlotPageCount,
                                   aWASeg->mHashSlotAllocPageCount );

        sWCBPtr          = aWASeg->mWCBMap;
        sEndWCBPtr       = sWCBPtr +  aWASeg->mHashSlotPageCount;

        for( sWAPageID = 0 ;
             sWCBPtr < sEndWCBPtr ;
             sWCBPtr++ , sWAPageID++ )
        {
            if ( sWCBPtr->mWAPagePtr != NULL )
            {
                dumpWAHashPage( sWCBPtr->mWAPagePtr,
                                sWAPageID,
                                sTitle,
                                aOutBuf,
                                aOutSize );
                sTitle = ID_FALSE;
            }
        }
        (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );
    }

    return;
}

/***************************************************************************
 * Description : Hash Map �� �� Page�� dump�Ѵ�.
 *
 * aPagePtr   - [IN] dump ��� WA Hash Page       
 * aWAPageID  - [IN] dump ��� hash page�� WAPageID, ��¿�
 * aTitle     - [IN] Title�� ��� �� ������ ����
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpWAHashPage( void        * aPagePtr,
                                    scPageID      aWAPageID,
                                    idBool        aTitle,
                                    SChar       * aOutBuf,
                                    UInt          aOutSize )
{
    sdtHashSlot * sSlotPtr;
    UInt          sCurSlotIdx;
    UInt          sEndSlotIdx;

    if ( aPagePtr != NULL )
    {
        if ( aTitle == ID_TRUE )
        {
            (void)idlVA::appendFormat( aOutBuf,
                                       aOutSize,
                                       "[%8s] %7s <%7s,%6s> : %s\n",
                                       "SLOTNO",
                                       "HASHSET",
                                       "NPAGEID",
                                       "OFFSET",
                                       "POINTER");
        }

        (void)idlVA::appendFormat( aOutBuf,
                                   aOutSize,
                                   "WAPageID : %"ID_UINT32_FMT"\n",
                                   aWAPageID );


        sSlotPtr = (sdtHashSlot*)aPagePtr;

        sCurSlotIdx = aWAPageID * (SD_PAGE_SIZE / ID_SIZEOF( sdtHashSlot ));
        sEndSlotIdx = sCurSlotIdx + (SD_PAGE_SIZE / ID_SIZEOF( sdtHashSlot ));

        for(  ;
              sCurSlotIdx < sEndSlotIdx ;
              sSlotPtr++ , sCurSlotIdx++ )
        {
            if ( sSlotPtr->mHashSet != 0 )
            {
                (void)idlVA::appendFormat( aOutBuf,
                                           aOutSize,
                                           "[%8"ID_UINT32_FMT"] "
                                           "%7"ID_XINT32_FMT
                                           " <%8"ID_UINT32_FMT
                                           ", %4"ID_INT32_FMT"> : "
                                           "0x%016"ID_XPOINTER_FMT
                                           " %s\n",
                                           sCurSlotIdx,
                                           sSlotPtr->mHashSet,
                                           sSlotPtr->mPageID,
                                           SDT_DEL_SINGLE_FLAG_IN_OFFSET( sSlotPtr->mOffset ),
                                           (void*)sSlotPtr->mRowPtrOrHashSet,
                                           SDT_IS_SINGLE_ROW( sSlotPtr->mOffset ) ? "(Single Row)" : "" );
            }
        }

        (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n" );
    }

    return;
}

/***************************************************************************
 * Description : Insert�� Temp Row Info�� Dump�Ѵ�.
 *
 * aTRPInfo   - [IN] dump �� TRPInfo
 * aOutBuf    - [IN] dump�� ������ ���� Buffer 
 * aOutSize   - [IN] Buffer�� ũ��
 ***************************************************************************/
void sdtHashModule::dumpTempTRPInfo4Insert( void       * aTRPInfo,
                                            SChar      * aOutBuf,
                                            UInt         aOutSize )
{
    sdtHashInsertInfo * sTRPInfo = (sdtHashInsertInfo*)aTRPInfo;
    UInt                sSize;
    UInt                i;

    dumpTempTRPHeader( &sTRPInfo->mTRPHeader, aOutBuf, aOutSize );

    for( i = 0 ; i < sTRPInfo->mColumnCount ; i++ )
    {
        (void)idlVA::appendFormat( aOutBuf,
                                    aOutSize,
                                    "[%2"ID_UINT32_FMT"] Length:%4"ID_UINT32_FMT"\n",
                                    i,
                                    sTRPInfo->mValueList[ i ].length );

        (void)idlVA::appendFormat( aOutBuf,
                                    aOutSize,
                                    "Value : ");
        sSize = idlOS::strlen( aOutBuf );
        IDE_TEST( ideLog::ideMemToHexStr(
                      (UChar*)sTRPInfo->mValueList[ i ].value,
                      sTRPInfo->mValueList[ i ].length ,
                      IDE_DUMP_FORMAT_VALUEONLY,
                      aOutBuf + sSize,
                      aOutSize - sSize )
                  != IDE_SUCCESS );
        (void)idlVA::appendFormat( aOutBuf, aOutSize, "\n\n" );
    }

    return;

    IDE_EXCEPTION_END;

    return;
}


/**************************************************************************
 * Description : Hash Segment�� ��� WCB�� ������ �����
 *
 * aWASegment - [IN] WCB�� Dump �� Hash Segment
 * aOutBuf    - [IN] Dump�� ������ ��Ƶ� ��� Buffer
 * aOutSize   - [IN] ��� Buffer�� Size
 ***************************************************************************/
void sdtHashModule::dumpWCBs( void    * aWASegment,
                              SChar   * aOutBuf,
                              UInt      aOutSize )
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

    sWCBPtr    = ((sdtHashSegHdr*)aWASegment)->mWCBMap;
    sEndWCBPtr = sWCBPtr + sdtHashModule::getMaxWAPageCount( (sdtHashSegHdr*)aWASegment );

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
void sdtHashModule::dumpWCB( void    * aWCBPtr,
                             SChar   * aOutBuf,
                             UInt      aOutSize )
{
    sdtWCB   * sWCBPtr;

    sWCBPtr = (sdtWCB*) aWCBPtr;

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
