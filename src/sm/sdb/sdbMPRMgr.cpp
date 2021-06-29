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
 * $$Id:$
 **********************************************************************/
#include <sdbReq.h>
#include <sdbMPRMgr.h>


/* BUG-33762 The MPR should uses the mempool instead allocBuff4DirectIO
 * function. */
iduMemPool sdbMPRMgr::mReadIOBPool;

IDE_RC sdbMPRMgr::initializeStatic()
{
    IDE_TEST(mReadIOBPool.initialize(
            IDU_MEM_SM_SDB,
            (SChar*)"SDB_MPR_IOB_POOL",
            ID_SCALABILITY_SYS,                    /* Multi Pooling */
            SDB_MAX_MPR_PAGE_COUNT * SD_PAGE_SIZE, /* Block Size*/
            1,                                     /* BlockCntInChunk*/
            ID_UINT_MAX,                           /* chunk limit */
            ID_TRUE,                               /* use mutex   */
            SD_PAGE_SIZE,                          /* align byte  */
            ID_FALSE,							   /* ForcePooling */
            ID_TRUE,							   /* GarbageCollection */
            ID_TRUE,                               /* HWCacheLine */
            IDU_MEMPOOL_TYPE_LEGACY                /* mempool type */) 
           != IDE_SUCCESS);			

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbMPRMgr::destroyStatic()
{
    IDE_TEST( mReadIOBPool.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 * Multi Page Read Manager�� �ʱ�ȭ�մϴ�. MPR�� �����Ϸ��� Segment
 * �� Extent ������������ DB_FILE_MULTIPAGE_READ_COUNT���� Align�Ǿ�
 * ���������� SBR( Single Block Read )�� �ϰ� �˴ϴ�. ���� ũ�ٸ�
 * DB_FILE_MULTIPAGE_READ_COUNT������ MPR�� �ϰ� �˴ϴ�.
 *
 * aStatistics - [IN] ��� ����
 * aSpaceID    - [IN] SpaceID
 * aSegPID     - [IN] Segment PID
 * aFilter     - [IN] ���� Extent���� �Ǵ��ϱ� ���� Filter
 *                    Null�� ��� �� ����
 ****************************************************************/
IDE_RC sdbMPRMgr::initialize( idvSQL           * aStatistics,
                              scSpaceID          aSpaceID,
                              sdpSegHandle     * aSegHandle,
                              sdbMPRFilterFunc   aFilter )
{
    UInt sDBFileMRCnt;
    UInt sPageCntInExt;
    UInt sSmallTblThreshold;

    IDE_ASSERT( aSegHandle != NULL );

    mBufferPool   = sdbBufferMgr::getBufferPool();
    mStatistics   = aStatistics;
    mSpaceID      = aSpaceID;
    mSegHandle    = aSegHandle;
    mCurExtRID    = SD_NULL_RID;
    mCurPageID    = SD_NULL_PID;
    mIsFetchPage  = ID_FALSE;
    mIsCachePage  = ID_FALSE;
    mFilter       = aFilter;

    mLstAllocPID       = SD_NULL_PID;
    mLstAllocSeqNo     = 0;
    mFoundLstAllocPage = ID_FALSE;

    mCurExtSeq   = (ULong)-1;

    mSegMgmtOP  = smLayerCallback::getSegMgmtOp( aSpaceID );

    IDE_ERROR( mSegMgmtOP != NULL );

    /* Segment Info */
    IDE_TEST( mSegMgmtOP->mGetSegInfo( aStatistics,
                                       aSpaceID,
                                       aSegHandle->mSegPID,
                                       NULL, /* aTableHeader */
                                       &mSegInfo )
              != IDE_SUCCESS );

    /* Segment Cache Info */
    IDE_TEST( mSegMgmtOP->mGetSegCacheInfo( aStatistics,
                                            aSegHandle,
                                            &mSegCacheInfo )
              != IDE_SUCCESS );

    sDBFileMRCnt  = smuProperty::getDBFileMutiReadCnt();
    sPageCntInExt = mSegInfo.mPageCntInExt;

    /* Segment�� Extent���� ������ ������ DB_FILE_MULTIPAGE_READ_COUNT
     * �� Align�� ���� �ʰų� ũ�ٸ� Single Blcok Read�� �Ѵ�. */
    if( sDBFileMRCnt >= sPageCntInExt )
    {
        mMPRCnt = sPageCntInExt;
    }
    else
    {
        mMPRCnt = sDBFileMRCnt;

        if( sPageCntInExt % mMPRCnt != 0 )
        {
            mMPRCnt = 1;
        }
    }

    /* Buffer Size�� �ʹ������� SPR�� �Ѵ�. */
    if( mBufferPool->getPoolSize() < mMPRCnt * 100 )
    {
        mMPRCnt = 1;
    }

    /* ��� Segment�� ������ �Ѱ��̻��� Extent�� ������. */
    IDE_ASSERT( mSegInfo.mFstExtRID != SD_NULL_RID );

    /* BUG-33720 */
    /* Small Table�� ��쿣�� ���� ���������� LRU List�� �տ� �ִ´�. */
    sSmallTblThreshold = smuProperty::getSmallTblThreshold();
    if( mSegInfo.mFmtPageCnt <= sSmallTblThreshold )
    {
        mIsCachePage = ID_TRUE;
    }
    else
    {
        mIsCachePage = ID_FALSE;

        /* sSmallTblThreshold�� ID_UINT_MAX�ϰ�� MPR�� ���� ���������� ������
         * ���ۿ� caching�Ѵ�.
         */
        if( sSmallTblThreshold == ID_UINT_MAX )
        {
            mIsCachePage = ID_TRUE;
        }
    }

    IDE_TEST( initMPRKey() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  MPR�� ����ϴ� MPRKey�� �����Ѵ�.
 ****************************************************************/
IDE_RC sdbMPRMgr::destroy()
{
    IDE_TEST( destMPRKey() != IDE_SUCCESS );

    if ( (mFoundLstAllocPage == ID_TRUE) && (mLstAllocPID != SD_NULL_PID) )
    {
        IDE_ASSERT( mSegHandle != NULL );

        IDE_TEST( mSegMgmtOP->mSetLstAllocPage( mStatistics,
                                                mSegHandle,
                                                mLstAllocPID,
                                                mLstAllocSeqNo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  MPR�� ����ϴ� MPRKey�� IO Buffer�� �Ҵ��Ѵ�.
 ****************************************************************/
IDE_RC sdbMPRMgr::initMPRKey()
{
    /* sdbMPRMgr_initMPRKey_alloc_ReadIOB.tc */
    IDU_FIT_POINT("sdbMPRMgr::initMPRKey::alloc::ReadIOB");
    IDE_TEST( mReadIOBPool.alloc( (void**)&mMPRKey.mReadIOB ) != IDE_SUCCESS );

    mMPRKey.initFreeBCBList();

    mMPRKey.mState = SDB_MPR_KEY_CLEAN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  MPR�� ����ϴ� MPRKey�� destroy�Ѵ�. �Ҵ�� IO Buffer��
 *  Free�Ѵ�.
 ****************************************************************/
IDE_RC sdbMPRMgr::destMPRKey()
{
    sdbBCB *sFirstBCB;
    sdbBCB *sLastBCB;
    UInt    sBCBCount;

    if( mIsFetchPage == ID_TRUE )
    {
        mIsFetchPage = ID_FALSE;
        mBufferPool->cleanUpKey( mStatistics, &mMPRKey, mIsCachePage );
    }

    /* BUG-22294: Buffer Missȯ�濡�� Hang�� ��ó�� ���Դϴ�.
     *
     * removeAllBCB�ϱ����� cleanUpKey�� �����ؾ���. �����ؾ�����
     * FreeLst�� ��ȯ�ǰ� removeAllBCB�ÿ��� FreeList������ �����ͼ�
     * Free�� �ϱ⶧���Դϴ�.
     * */
    mMPRKey.removeAllBCB( &sFirstBCB, &sLastBCB, &sBCBCount );

    if (sBCBCount > 0)
    {
        mBufferPool->addBCBList2PrepareLst( mStatistics,
                                            sFirstBCB,
                                            sLastBCB,
                                            sBCBCount );
    }

    IDE_TEST( mReadIOBPool.memfree( mMPRKey.mReadIOB ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 * ù��° ������ �ٷ��� ��ġ�� MPRKey�� Index�� ��ġ��Ų��.
 * �� �Լ��Ŀ� getNxtPage�� �ϰԵǸ� ù��° �������� Get�ϰ� �ȴ�.
 ****************************************************************/
IDE_RC sdbMPRMgr::beforeFst()
{
    UInt sMPRCnt;

    if( mIsFetchPage == ID_TRUE )
    {
        mIsFetchPage = ID_FALSE;
        mBufferPool->cleanUpKey( mStatistics, &mMPRKey, mIsCachePage );
    }

    mCurExtRID = mSegInfo.mFstExtRID;
    mCurPageID = SD_NULL_PID;

    IDE_TEST( mSegMgmtOP->mGetExtInfo( mStatistics,
                                       mSpaceID,
                                       mCurExtRID,
                                       &mCurExtInfo )
              != IDE_SUCCESS );

    /* Segment�� ù��° PID�� SeqNo�� 0�̴�. */
    sMPRCnt = getMPRCnt( mCurExtRID, mCurExtInfo.mFstPID );

    IDE_TEST( mBufferPool->fetchPagesByMPR(
                  mStatistics,
                  mSpaceID,
                  mCurExtInfo.mFstPID,
                  sMPRCnt,
                  &mMPRKey )
              != IDE_SUCCESS );
    mIsFetchPage = ID_TRUE;

    mMPRKey.moveToBeforeFst();
    mCurExtSeq = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 * Filtering �� Parallel Scan�� ���ÿ� �����ϱ� ����,
 * ExtentSequence�� �������� ���� Extent���� �ƴ����� Filtering �Ѵ�.
 *
 * - Sampling ��� ���� �� 
 * Sampling Percentage�� P�� ������, �����Ǵ� �� C�� �ΰ� C�� P��
 * �������� 1�� �Ѿ�� ��쿡 Sampling �ϴ� ������ �Ѵ�.
 *
 * ��)
 * P=1, C=0
 * ù��° ������ C+=P  C=>0.25
 * �ι�° ������ C+=P  C=>0.50
 * ����° ������ C+=P  C=>0.75
 * �׹�° ������ C+=P  C=>1(Sampling!)  C--; C=>0
 * �ټ���° ������ C+=P  C=>0.25
 * ������° ������ C+=P  C=>0.50
 * �ϰ���° ������ C+=P  C=>0.75
 * ������° ������ C+=P  C=>1(Sampling!)  C--; C=>0 
 *
 * aExtReq      - [IN] Extent�� Sequence��ȣ
 * aFilterData  - [IN] ThreadID & ThreadCnt & SamplingData
 *
 * RETURN       - [OUT] ������ ����
 ****************************************************************/
idBool sdbMPRMgr::filter4SamplingAndPScan( ULong   aExtSeq,
                                           void  * aFilterData )
{
    sdbMPRFilter4SamplingAndPScan * sData;
    idBool                          sRet = ID_FALSE;
    
    sData = (sdbMPRFilter4SamplingAndPScan*)aFilterData;

    if( ( ( aExtSeq % sData->mThreadCnt ) == sData->mThreadID ) &&
        ( ( (UInt)( sData->mPercentage * aExtSeq  + SMI_STAT_SAMPLING_INITVAL ) ) !=
          ( (UInt)( sData->mPercentage * (aExtSeq+1) + SMI_STAT_SAMPLING_INITVAL ) ) ) )
    {
        sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

/****************************************************************
 * Description :
 * Parallel Scan�� ���� ExtentSequence�� Modular�Ͽ� ���� ����
 * Extent���� �ƴ����� Filtering �Ѵ�.
 *
 * aExtReq     - [IN] Extent�� Sequence��ȣ
 * aFilterData - [IN] ThreadID & ThreadCnt
 *
 * RETURN      - [OUT] ������ ����
 ****************************************************************/
idBool sdbMPRMgr::filter4PScan( ULong   aExtSeq,
                                void  * aFilterData )
{
    sdbMPRFilter4PScan * sData;
    idBool               sRet = ID_FALSE;
    
    sData = (sdbMPRFilter4PScan*)aFilterData;

    if( ( aExtSeq % sData->mThreadCnt ) == sData->mThreadID )
    {
        sRet = ID_TRUE;
    }
    else
    {
        sRet = ID_FALSE;
    }

    return sRet;
}

/****************************************************************
 * Description :
 * Scan�� mCurPageID�� ���� Page�� ���� PID�� Pointer�� ã���ش�.
 * ������ ���δ� Filter�� �Ǵ��Ѵ�.
 *
 * aFilterData - [IN] Filter���� ���� ����
 * aPageID     - [OUT] Next PID
 ****************************************************************/
IDE_RC sdbMPRMgr::getNxtPageID( void           * aFilterData,
                                scPageID       * aPageID )
{
    sdRID     sExtRID;
    scPageID  sNxtPID;

    sExtRID = mCurExtRID;
    sNxtPID = mCurPageID;

    while(1)
    {
        IDE_TEST( mSegMgmtOP->mGetNxtAllocPage( mStatistics,
                                                mSpaceID,
                                                &mSegInfo,
                                                &mSegCacheInfo,
                                                &sExtRID,
                                                &mCurExtInfo,
                                                &sNxtPID )
                  != IDE_SUCCESS );

        IDE_TEST_CONT( sNxtPID == SD_NULL_PID, cont_no_more_page );

        if( sExtRID != mCurExtRID )
        {
            mCurExtSeq++;
        }

        if( mFilter == NULL )
        {
            IDE_ERROR( aFilterData == NULL );
            break; /* Filtering ���ϸ� ���� OK */
        }
        else
        {
            IDE_ERROR( aFilterData != NULL );
            if( mFilter( mCurExtSeq, aFilterData ) == ID_TRUE )
            {
                break;
            }
            else
            {
                /*Filter�� ��� ���� */
            }
        }

        mCurExtRID = sExtRID;
    }

    IDE_TEST( fixPage( sExtRID,
                       sNxtPID )
              != IDE_SUCCESS );

    *aPageID = sNxtPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( cont_no_more_page );

    /* BUG-33719 [sm-disk-resource] set full scan hint for TMS. 
     * NoSampling SerialScan�� ���, Fullscan�� Hint�� �����ؾ� �� */
    if( aFilterData == NULL )
    {
        mFoundLstAllocPage = ID_TRUE;
    }
    *aPageID     = SD_NULL_PID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPageID     = SD_NULL_PID;

    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  MPR Ű�� �̿��Ͽ� fixPage�� �Ѵ�.
 *
 *  �̹� fixCount�� �÷� ���ұ� ������, �ݵ�� �ؽ�(����)�� ��������
 *  ���� �� �� �ִ�.  �׷��� ������ ���⼱ ������ fix�� ���� �ʰ�,
 *  �ؽõ� �������� �ʴ´�. ���� touch Count�� ���� ��Ű�� �ʴ´�.
 *
 *  getPage�������̽��� ���� �˰�����, ������ �������� ���ؼ� �����ϴ�
 *  ���� �ƴϰ�, ���ϵ� BCB�� ������ �ִ�.
 *
 *  aExtRID     - [IN] aPage�� �ִ� Extent RID
 *  aPageID     - [IN] PageID
 ****************************************************************/
IDE_RC sdbMPRMgr::fixPage( sdRID            aExtRID,
                           scPageID         aPageID )
{
    sdbBCB          *sBCB;
    sdpPhyPageHdr   *sPageHdr;
    sdpPageType      sPageType;
    scPageID         sFstReadPID;
    sdRID            sPrvExtRID;
    UInt             sMPRCnt;

    sPrvExtRID = mCurExtRID;

    /* MPR�� Read�� �������� ���� ���� ���� �������� �ִ��� �����Ѵ�. */
    while(1)
    {
        if( mMPRKey.hasNextPage() == ID_TRUE )
        {
            IDE_ASSERT( mMPRKey.hasNextPage() == ID_TRUE );
            IDE_ASSERT( aPageID != SD_NULL_PID );

            mMPRKey.moveToNext();
        }
        else
        {
            /* ���ο� �������� Disk���� �о�;� �Ѵ�. */
            IDE_ASSERT( aExtRID != SD_NULL_RID );

            /* mCurPageID�� ������ �о��� Extent�� �������� �ʴ´�.
             * ���� Extent�� �̵� �Ѵ�. */
            if( aExtRID != sPrvExtRID )
            {
                /* BUG-29450 - DB_FILE_MULTIPAGE_READ_COUNT�� Extent�� ������
                 *             �������� ũ�ų� align�� ���� �ʴ� ���
                 *             FullScan�� Hang �ɸ� �� �ִ�.
                 *
                 * MPR�� MPRCnt�� Extent�� ������ ������ align �Ǿ� �ֱ� ������
                 * sFstReadPID = mCurExtInfo.mFstDataPID;
                 * �� ���� ù��° ������ �������� �����ϸ� �ȵȴ�. */
                sFstReadPID = mCurExtInfo.mFstPID;
                sPrvExtRID  = aExtRID;
            }
            else
            {
                sBCB = mMPRKey.mBCBs[ mMPRKey.mCurrentIndex ].mBCB;

                IDE_ASSERT( sBCB != NULL );

                /* ���� Extent�� ���������� �̹Ƿ� +1 �� ���� ������
                 * �о�� �� Page ID */
                sFstReadPID = sBCB->mPageID + 1;
            }

            /* ������ Extent�� �������� HWM������ �о�� �ϱ⶧����
             * MPR Count�� �����ؾ� �Ѵ�. */
            sMPRCnt = getMPRCnt( aExtRID, sFstReadPID );

            mIsFetchPage = ID_FALSE;
            mBufferPool->cleanUpKey( mStatistics, &mMPRKey, mIsCachePage );

            IDE_ASSERT( mCurExtInfo.mFstPID <= sFstReadPID );
            IDE_ASSERT( ( sFstReadPID + sMPRCnt ) <=
                        ( mCurExtInfo.mFstPID + mSegInfo.mPageCntInExt ) );

            IDE_TEST( mBufferPool->fetchPagesByMPR(
                          mStatistics,
                          mSpaceID,
                          sFstReadPID,
                          sMPRCnt,
                          &mMPRKey )
                      != IDE_SUCCESS );
            mIsFetchPage = ID_TRUE;
        }

        sBCB = mMPRKey.mBCBs[ mMPRKey.mCurrentIndex ].mBCB;

        if( ( sBCB->mSpaceID == mSpaceID ) &&
            ( sBCB->mPageID  == aPageID  ) )
        {
            IDE_ASSERT( sBCB->mFrame != NULL );

            /* BUG-29005 FullScan ���� ���� */
            sPageHdr  = sdpPhyPage::getHdr( sBCB->mFrame );
            sPageType = sdpPhyPage::getPageType( sPageHdr );

            if ( (sPageType == SDP_PAGE_DATA) ||
                 (sPageType == SDP_PAGE_INDEX_BTREE) ||
                 (sPageType == SDP_PAGE_INDEX_RTREE) )
            {
                mLstAllocPID    = aPageID;
                mLstAllocSeqNo  = sPageHdr->mSeqNo;
            }

            break;
        }
    } /* While */

    mCurExtRID  = aExtRID;
    mCurPageID  = aPageID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  MPR�� ���� ����Ű�� �ִ� page�� pointer�� �����Ѵ�.
 *
 ****************************************************************/
UChar* sdbMPRMgr::getCurPagePtr()
{
    sdbBCB   *sBCB;

    sBCB = mMPRKey.mBCBs[ mMPRKey.mCurrentIndex ].mBCB;
    return sBCB->mFrame;
}

/****************************************************************
 * Description:
 *  MPR Count�� ���Ѵ�. ������ Extent�� �������� HWM������ �о��
 *  �ϱ⶧���� MPR Count�� �����ؾ� �Ѵ�
 *
 * aPagePtr - [IN] Page Pointer
 ****************************************************************/
UInt sdbMPRMgr::getMPRCnt( sdRID aCurReadExtRID, scPageID aFstReadPID )
{
    UInt sMPRCnt;
    UInt sState = 0;

    sMPRCnt = mMPRCnt;

    /* BUG-29005 FullScan ���� ���� */
    if ( mSegCacheInfo.mUseLstAllocPageHint == ID_TRUE )
    {
        sState = 1;
        
        /* ������ �Ҵ�� �������� ������ Extent�� read�ϴ� ��� */
        if ( (mCurExtInfo.mFstPID <= mSegCacheInfo.mLstAllocPID) &&
             (mCurExtInfo.mLstPID >= mSegCacheInfo.mLstAllocPID) )
        {
            sState = 2;
            IDE_ASSERT( ( mSegCacheInfo.mLstAllocPID + 1 ) > aFstReadPID );

            if ( ( aFstReadPID + sMPRCnt - 1 ) > mSegCacheInfo.mLstAllocPID )
            {
                sState = 3;
                sMPRCnt = mSegCacheInfo.mLstAllocPID - aFstReadPID + 1;
            }
        }
    }

    if( mSegInfo.mExtRIDHWM == aCurReadExtRID )
    {
        sState = 4;
        IDE_ASSERT( ( mSegInfo.mHWMPID + 1 ) > aFstReadPID );

        if( ( aFstReadPID + sMPRCnt - 1) > mSegInfo.mHWMPID )
        {
            sState = 5;
            sMPRCnt = mSegInfo.mHWMPID - aFstReadPID + 1;
        }
    }

    /* To Analyze BUG-27447, log a few informations */
//    IDE_ASSERT( sMPRCnt != 0 );
    if( sMPRCnt == 0 )
    {
        // !! BUG-27447 is reproduced
        ideLog::log(IDE_SERVER_0, "[BUG-27447] is reproduced");
        ideLog::log(IDE_SERVER_0, "[BUG-27447] this=%x, sState=%u, aCurReadExtRID=%llu, aFstReadPID=%u\n",this, sState, aCurReadExtRID, aFstReadPID);
        ideLog::log(IDE_SERVER_0, "[BUG-27447] mSegInfo.mSegHdrPID=%u, mSegInfo.mType=%u, mSegInfo.mState=%u, mSegInfo.mPageCntInExt=%u, mSegInfo.mFmtPageCnt=%llu, mSegInfo.mExtCnt=%llu, mSegInfo.mExtDirCnt=%llu, mSegInfo.mFstExtRID=%llu, mSegInfo.mLstExtRID=%llu, mSegInfo.mHWMPID=%u, mSegInfo.mLstAllocExtRID=%llu, mSegInfo.mFstPIDOfLstAllocExt=%u\n", mSegInfo.mSegHdrPID, mSegInfo.mType, mSegInfo.mState, mSegInfo.mPageCntInExt, mSegInfo.mFmtPageCnt, mSegInfo.mExtCnt, mSegInfo.mExtDirCnt, mSegInfo.mFstExtRID, mSegInfo.mLstExtRID, mSegInfo.mHWMPID, mSegInfo.mLstAllocExtRID, mSegInfo.mFstPIDOfLstAllocExt);
        IDE_ASSERT( 0 );
    }

    return sMPRCnt;
}
