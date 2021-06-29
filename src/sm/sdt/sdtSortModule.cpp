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
#include <sdtSortDef.h>
#include <sdtTempRow.h>
#include <sdtSortModule.h>
#include <sdtSortSegment.h>


/**************************************************************************
 * Description :
 * WorkArea�� �Ҵ�ް� temptableHeader���� �ʱ�ȭ ���ش�.
 * ���� insertNSort, insert �� �� �ϳ��� ���°�
 * �ȴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 ***************************************************************************/
IDE_RC sdtSortModule::init( smiTempTableHeader * aHeader )
{
    sdtSortSegHdr      * sWASeg;
    UInt                 sInitGroupPageCount;
    UInt                 sSortGroupPageCount;
    UInt                 sFlushGroupPageCount;
    smiColumnList      * sKeyColumn;

    IDE_ERROR( aHeader != NULL );

    sWASeg  = (sdtSortSegHdr*)aHeader->mWASegment;
    sKeyColumn = (smiColumnList*) aHeader->mKeyColumnList;

    /* SortTemp�� unique üũ ���� */
    IDE_ERROR( SM_IS_FLAG_OFF( aHeader->mTTFlag, SMI_TTFLAG_UNIQUE )
               == ID_TRUE );

    /************************** Group���� *******************************/
    sInitGroupPageCount = sdtSortSegment::getAllocableWAGroupPageCount(
        sWASeg, SDT_WAGROUPID_INIT );

    /* BUG-46427 �� group�� �ּ� 2�� �̻��� page�� �־�� �Ѵ�.
     * Flush group�� �� �� sub flush group�� �и� �� ���� �ִ�.
     * �� 3�� group�� �� ���� �ִ�. */
    IDE_ERROR( sInitGroupPageCount >= ( SDT_WAGROUP_MIN_PAGECOUNT * 3 ) );

    sSortGroupPageCount = sInitGroupPageCount * aHeader->mWorkGroupRatio / 100;

    if ( sSortGroupPageCount < SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
        sSortGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT;
    }

    sFlushGroupPageCount= ( sInitGroupPageCount - sSortGroupPageCount);

    if ( sFlushGroupPageCount < ( SDT_WAGROUP_MIN_PAGECOUNT * 2 ) )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�.
         * Flush Group�� �� �� sub flush�� �ѷ� ������ �� �� �ִ�.
         * �̸� 4�� �̻��� page�� �Ҵ��� ���´�. */
        sFlushGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT * 2 ;
        sSortGroupPageCount = sInitGroupPageCount - sFlushGroupPageCount;
    }
    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_SORT,
                                             sSortGroupPageCount,
                                             SDT_WA_REUSE_INMEMORY )
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_FLUSH,
                                             sFlushGroupPageCount,
                                             SDT_WA_REUSE_FIFO )
              != IDE_SUCCESS );

    aHeader->mSortGroupID       = SDT_WAGROUPID_SORT;
    aHeader->mInitMergePosition = NULL;
    aHeader->mScanPosition      = NULL;
    SC_MAKE_NULL_GRID( aHeader->mGRID );

    IDE_TEST( sdtWASortMap::create( sWASeg,
                                    aHeader->mSortGroupID,
                                    SDT_WM_TYPE_POINTER,
                                    0, /* Slot Count */
                                    2, /* aVersionCount */
                                    &sWASeg->mSortMapHdr )
              != IDE_SUCCESS );

    IDE_TEST( aHeader->mSortStack.initialize( IDU_MEM_SM_TEMP,
                                              ID_SIZEOF(smnSortStack) )
              != IDE_SUCCESS);

    /* BUG-39440 ������ ������� ������ �����ϵ��� keyColumn�� ���� ���
     * ���ľ��� insert�� ��
     */
    if ( sKeyColumn != NULL )
    {
        aHeader->mTTState = SMI_TTSTATE_SORT_INSERTNSORT;
    }
    else
    {
        aHeader->mTTState = SMI_TTSTATE_SORT_INSERTONLY;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * ���Ե� Row���� �����Ѵ�.
 * �̹� Flush�� Run�� Queue�� �����ϸ� Merge�� �ʿ��ϰ�,
 * �� ���� ���� InMemorySort�� �����Ѵ�.
 *
 * ���� WindowSort�� �ϴ� ��Ȳ�̸�, �̹� ����� Row�� �������Ѵ�.
 *
 * <IN>
 * aHeader        - ��� Table
 ***************************************************************************/
IDE_RC sdtSortModule::sort(smiTempTableHeader * aHeader)
{
    switch( aHeader->mTTState )
    {
        /********************** ������ �ʿ��� ��Ȳ ***********************/
        case SMI_TTSTATE_SORT_INSERTNSORT:
            /* keyColumn�� ���°� �ǵ��� ��Ȳ�̶�� assert �� ���� �ؾ� �Ѵ�. */
            IDE_ERROR( aHeader->mKeyColumnList != NULL );

            if ( aHeader->mRunQueue.getQueueLength() > 0 )
            {
                /* Run�� ������ ���� ����. InMemory�� �ƴ϶�� �̾߱� */
                IDE_TEST( merge( aHeader ) != IDE_SUCCESS );

                if ( SM_IS_FLAG_ON( aHeader->mTTFlag, SMI_TTFLAG_RANGESCAN ) )
                {
                    IDE_TEST( makeIndex( aHeader ) != IDE_SUCCESS );
                    IDE_TEST( indexScan( aHeader ) != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( mergeScan( aHeader ) != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_TEST( inMemoryScan( aHeader ) != IDE_SUCCESS );
            }
            break;

            /******************** �׳� ������� �о�ô� *******************/
        case SMI_TTSTATE_SORT_INSERTONLY:
        case SMI_TTSTATE_SORT_SCAN:
            IDE_ERROR( aHeader->mKeyColumnList == NULL );

            if ( aHeader->mRunQueue.getQueueLength() > 0 )
            {
                IDE_TEST( scan( aHeader ) != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( inMemoryScan( aHeader ) != IDE_SUCCESS );
            }
            break;

            /********************** �������� �䱸�� ��� *********************/
        case SMI_TTSTATE_SORT_INMEMORYSCAN:
            /* �������� RangeScan�� �䱸�� */
            IDE_ERROR( SM_IS_FLAG_ON( aHeader->mTTFlag, SMI_TTFLAG_RANGESCAN ) );
            /* ������ InMemory�̹Ƿ�, �׳� ����� Column�� ���� �������ϸ� ��*/
            IDE_TEST( inMemoryScan( aHeader ) != IDE_SUCCESS );
            break;
        case SMI_TTSTATE_SORT_INDEXSCAN:
            /* �������� RangeScan�� �䱸�� */
            IDE_ERROR( SM_IS_FLAG_ON( aHeader->mTTFlag, SMI_TTFLAG_RANGESCAN ) );
            IDE_TEST( extractNSort( aHeader ) != IDE_SUCCESS );
            break;
        default:
        case SMI_TTSTATE_SORT_EXTRACTNSORT:
        case SMI_TTSTATE_SORT_MERGE:
        case SMI_TTSTATE_SORT_MAKETREE:
        case SMI_TTSTATE_SORT_MERGESCAN:
            IDE_ERROR( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * ���������� ���� ���ġ�� ����Ѵ�
 *
 * <IN>
 * aHeader        - ��� Table
 ***************************************************************************/
IDE_RC sdtSortModule::calcEstimatedStats( smiTempTableHeader * aHeader )
{
    smiTempTableStats * sStatsPtr;
    ULong               sDataPageCount;  /* �� Data�� Page���� */
    UInt                sRowPageCount;   /* Row�� ������ �ִ� Page���� */
    UInt                sRunPageCount;   /* Run�� Page ũ�� */

    sStatsPtr      = aHeader->mStatsPtr;
    sDataPageCount = ( SDT_TR_HEADER_SIZE_FULL + aHeader->mRowSize )
        * aHeader->mRowCount / SD_PAGE_SIZE;
    sRowPageCount  = ( aHeader->mRowSize / SD_PAGE_SIZE ) + 2;
    sRunPageCount  = sRowPageCount * 2;

    /* Optimal(InMemory)�� ��� �����Ͱ� SortArea�� ��� ũ��� �ȴ�. */
    sStatsPtr->mEstimatedOptimalSize = sDataPageCount;

    /* OnePass��,
     * N���� Page�� ���� Run�� ������� �� ( InserttNSort )
     * M���� Run�� Merge�Ͽ� �Ϸ�Ǹ� Onepass�̴�.
     *
     * N = SortAreaSize / SD_PAGE_SIZE => SortArea�� ���� �� Row�� ����ϱ�
     * M = SortAreaSize / RunSize      => SortArea�� Run�� �����ϱ�
     * Data = N * M                    => N���� Page�� ���� Run M��
     *
     * Data = SortAreaSize * SortAreaSize / SD_PAGE_SIZE / RunSize
     * SortAreaSize^2 = Data * RunSize * SD_PAGE_SIZE
     * SortAreaSize = sqrt( Data*RunSize ) */
    sStatsPtr->mEstimatedSubOptimalSize = (ULong)idlOS::sqrt(
        (SDouble) sDataPageCount * sRunPageCount );

    /* SortGroupRatio�� ����ϰ�, PageHeader�� WCB���� ����Ͽ�,
     * 1.3�� ���� ������ */
    sStatsPtr->mEstimatedOptimalSize =
        (ULong)( sStatsPtr->mEstimatedOptimalSize
                 * SD_PAGE_SIZE * 100 / aHeader->mWorkGroupRatio * 1.3);
    sStatsPtr->mEstimatedSubOptimalSize =
        (ULong)( sStatsPtr->mEstimatedSubOptimalSize
                 * SD_PAGE_SIZE * 100 / aHeader->mWorkGroupRatio * 1.3);

    return IDE_SUCCESS;
}


/**************************************************************************
 * Description :
 * �����Ѵ�. WorkArea�� Cursor���� smiTemp���� �˾Ƽ� �Ѵ�.
 *
 * <IN>
 * aHeader        - ��� Table
 ***************************************************************************/
IDE_RC sdtSortModule::destroy( smiTempTableHeader * aHeader )
{
    if ( aHeader->mTTState != SMI_TTSTATE_INIT )
    {
        IDE_TEST( aHeader->mSortStack.destroy() != IDE_SUCCESS );
    }

    if ( aHeader->mInitMergePosition != NULL )
    {
        IDE_TEST( iduMemMgr::free( aHeader->mInitMergePosition )
                  != IDE_SUCCESS );
        aHeader->mInitMergePosition = NULL;
    }

    if ( aHeader->mScanPosition != NULL )
    {
        IDE_TEST( iduMemMgr::free( aHeader->mScanPosition )
                  != IDE_SUCCESS );
        aHeader->mScanPosition = NULL;
    }

    /* ����Ǹ鼭 ���� ���ġ�� ����Ѵ�. */
    IDE_TEST( calcEstimatedStats( aHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * InMemoryScan�� Ŀ���� ���ϴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * <OUT>
 * aCursor        - ��ȯ��
 ***************************************************************************/
IDE_RC sdtSortModule::openCursorInMemoryScan( smiTempTableHeader * aHeader,
                                              smiSortTempCursor  * aCursor )
{
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)aHeader->mWASegment;

    if ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT )
    {
        /*Sort ���� ������ ��� */
        IDE_ERROR( aHeader->mRunQueue.getQueueLength() == 0 );
        IDE_ERROR( aHeader->mKeyColumnList != NULL );

        aHeader->mTTState = SMI_TTSTATE_SORT_INMEMORYSCAN;
    }
    else
    {
        if ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTONLY )
        {
            IDE_ERROR( aHeader->mRunQueue.getQueueLength() == 0 );
            IDE_ERROR( aHeader->mKeyColumnList == NULL );

            aHeader->mTTState = SMI_TTSTATE_SORT_INMEMORYSCAN;
        }
        else
        {
            IDE_ERROR( aHeader->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN );
        }
    }

    SC_MAKE_NULL_GRID( aCursor->mGRID );
    aHeader->mFetchGroupID  = SDT_WAGROUPID_NONE;
    aCursor->mWAGroupID     = SDT_WAGROUPID_NONE;
    aCursor->mStoreCursor   = storeCursorInMemoryScan;
    aCursor->mRestoreCursor = restoreCursorInMemoryScan;

    if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) )
    {
        aCursor->mFetch   = fetchInMemoryScanForward;
        aCursor->mSeq     = -1;
        aCursor->mLastSeq = sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr );
    }
    else
    {
        aCursor->mFetch   = fetchInMemoryScanBackward;
        aCursor->mSeq     = sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr );
        aCursor->mLastSeq = -1;
    }

    if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) )
    {
        /* nothing to do ... */
    }
    else
    {
        IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_RANGESCAN ) );

        /* RangeScan�� �ϱ� ���� BeforeFirst, �Ǵ� AfterLast�� Ž���� */
        if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) )
        {
            IDE_TEST( traverseInMemoryScan( aHeader,
                                            &aCursor->mRange->minimum,
                                            ID_TRUE, /* aDirection */
                                            &aCursor->mSeq )
                      != IDE_SUCCESS );
            IDE_TEST( traverseInMemoryScan( aHeader,
                                            &aCursor->mRange->maximum,
                                            ID_FALSE, /* aDirection */
                                            &aCursor->mLastSeq )
                      != IDE_SUCCESS );
            aCursor->mLastSeq++;
        }
        else
        {
            IDE_TEST( traverseInMemoryScan( aHeader,
                                            &aCursor->mRange->maximum,
                                            ID_FALSE, /* aDirection */
                                            &aCursor->mSeq )
                      != IDE_SUCCESS );
            IDE_TEST( traverseInMemoryScan( aHeader,
                                            &aCursor->mRange->minimum,
                                            ID_TRUE, /* aDirection */
                                            &aCursor->mLastSeq )
                      != IDE_SUCCESS );
            aCursor->mLastSeq--;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * MergeScan�� Ŀ���� ���ϴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * <OUT>
 * aCursor        - ��ȯ��
 ***************************************************************************/
IDE_RC sdtSortModule::openCursorMergeScan( smiTempTableHeader * aHeader,
                                           smiSortTempCursor     * aCursor )
{
    IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) );
    IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) );

    aCursor->mWAGroupID     = SDT_WAGROUPID_SORT;
    aHeader->mFetchGroupID  = SDT_WAGROUPID_NONE;
    IDE_TEST( makeMergeRuns( aHeader,
                             aHeader->mInitMergePosition )
              != IDE_SUCCESS );

    /* RangeScan �Ұ��� */
    IDE_ERROR( aCursor->mRange == smiGetDefaultKeyRange() );

    SC_MAKE_NULL_GRID( aCursor->mGRID );
    aCursor->mFetch         = fetchMergeScan;
    aCursor->mStoreCursor   = storeCursorMergeScan;
    aCursor->mRestoreCursor = restoreCursorMergeScan;

    aCursor->mMergePosition = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * IndexScan�� Ŀ���� ���ϴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * <OUT>
 * aCursor        - ��ȯ��
 ***************************************************************************/
IDE_RC sdtSortModule::openCursorIndexScan( smiTempTableHeader * aHeader,
                                           smiSortTempCursor     * aCursor )
{
    IDE_ERROR( aHeader->mTTState == SMI_TTSTATE_SORT_INDEXSCAN );

    aCursor->mWAGroupID     = SDT_WAGROUPID_LNODE;
    aHeader->mFetchGroupID  = SDT_WAGROUPID_LNODE;
    if ( aHeader->mHeight == 0 )
    {
        SC_MAKE_NULL_GRID( aCursor->mGRID );
    }
    else
    {
        IDE_TEST( traverseIndexScan( aHeader, aCursor ) != IDE_SUCCESS );
    }

    if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) )
    {
        aCursor->mFetch = fetchIndexScanForward;
    }
    else
    {
        aCursor->mFetch = fetchIndexScanBackward;
    }

    aCursor->mStoreCursor   = storeCursorIndexScan;
    aCursor->mRestoreCursor = restoreCursorIndexScan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * Scan�� Ŀ���� ���ϴ�. (BUG-39450)
 *
 * <IN>
 * aHeader        - ��� Table
 * <OUT>
 * aCursor        - ��ȯ��
 ***************************************************************************/
IDE_RC sdtSortModule::openCursorScan( smiTempTableHeader * aHeader,
                                      smiSortTempCursor     * aCursor )
{
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)aHeader->mWASegment;

    IDE_ERROR( aHeader->mTTState == SMI_TTSTATE_SORT_SCAN );
    /* RangeScan �Ұ��� */
    IDE_ERROR( aCursor->mRange == smiGetDefaultKeyRange() );

    IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) );
    IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_ORDEREDSCAN ) );
    IDE_ERROR( aHeader->mScanPosition != NULL );

    /*Fetch�� ����ϴ� GroupID ���� */
    aCursor->mWAGroupID    = SDT_WAGROUPID_SCAN;

    aCursor->mFetch         = fetchScan;
    aCursor->mStoreCursor   = storeCursorScan;
    aCursor->mRestoreCursor = restoreCursorScan;

    aCursor->mPinIdx        = 0;
    aHeader->mScanPosition[ SDT_TEMP_SCANPOS_PINIDX ] = 0;

    SC_MAKE_GRID( aCursor->mGRID,
                  aHeader->mSpaceID,
                  aHeader->mScanPosition[ SDT_TEMP_SCANPOS_HEADERIDX ],
                  0 /* offset */);

    IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                              aCursor->mWAGroupID,
                                              aCursor->mGRID,
                                              &aCursor->mWPID,
                                              &aCursor->mWAPagePtr,
                                              &aCursor->mSlotCount )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * InMemory������ ��� Row�� Ž���մϴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * aCallBack      - ���ϴµ� ���� �ݹ�
 * aDirection     - ����
 * <OUT>
 * aSeq           - ã�� ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::traverseInMemoryScan( smiTempTableHeader * aHeader,
                                            const smiCallBack  * aCallBack,
                                            idBool               aDirection,
                                            SInt               * aSeq )
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    SInt                 sLeft;
    SInt                 sRight;
    SInt                 sMid;
    idBool               sResult;
    sdtSortScanInfo      sScanInfo;

    sLeft  = 0;
    sRight = sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) - 1;

    sScanInfo.mFetchEndOffset = aHeader->mRowSize;

    while( sLeft <= sRight )
    {
        sMid   = ( sLeft + sRight ) >> 1;

        IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                           sMid,
                                           (vULong*)&sScanInfo.mTRPHeader )
                  != IDE_SUCCESS );

        IDE_TEST( sdtTempRow::fetch( sWASeg,
                                     SDT_WAGROUPID_NONE,
                                     aHeader->mRowBuffer4Compare,
                                     &sScanInfo )
                  != IDE_SUCCESS );

        IDE_TEST( aCallBack->callback( &sResult,
                                       sScanInfo.mValuePtr,
                                       NULL,
                                       0,
                                       SC_NULL_GRID,
                                       aCallBack->data )
                  != IDE_SUCCESS );

        if ( sResult == aDirection )
        {
            sRight = sMid - 1;
        }
        else
        {
            sLeft = sMid + 1;
        }
    }
    sMid   = ( sLeft + sRight ) >> 1;
    *aSeq  = sMid;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***************************************************************************
 * Description :
 * IndexScan������ ��� Row�� Ž���մϴ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * aCursor        - ��� Ŀ��
 ***************************************************************************/
IDE_RC sdtSortModule::traverseIndexScan( smiTempTableHeader * aHeader,
                                         smiSortTempCursor  * aCursor )
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    scPageID             sNPID;
    const smiCallBack  * sCallBack;
    idBool               sResult;
    sdtSortScanInfo      sScanInfo;
    idBool               sIsValidSlot;
    UInt                 sHeight;
    SInt                 sLeft;
    SInt                 sRight;
    SInt                 sMid;

    /* RangeScan�� �ϱ� ���� BeforeFirst, �Ǵ� AfterLast�� Ž���� */
    aCursor->mWAGroupID = SDT_WAGROUPID_INODE;
    sNPID               = aHeader->mRootWPID;
    sHeight             = aHeader->mHeight;
    sScanInfo.mFetchEndOffset = aHeader->mRowSize;

    if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) )
    {
        sCallBack = &aCursor->mRange->minimum;
    }
    else
    {
        IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_BACKWARD ) )
            sCallBack = &aCursor->mRange->maximum;
    }

    while ( sHeight > 0 )
    {
        if ( sHeight == 1 )
        {
            aCursor->mWAGroupID = SDT_WAGROUPID_LNODE;
        }
        else
        {
            /* nothing to do */
        }
        SC_MAKE_GRID( aCursor->mGRID, aHeader->mSpaceID, sNPID, 0 );

        IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                  aCursor->mWAGroupID,
                                                  aCursor->mGRID,
                                                  &aCursor->mWPID,
                                                  &aCursor->mWAPagePtr,
                                                  &aCursor->mSlotCount )
                  != IDE_SUCCESS );

        sLeft  = 0;
        sRight = aCursor->mSlotCount - 1;
        IDE_ERROR( aCursor->mSlotCount > 0 );

        while ( sLeft <= sRight )
        {
            sMid   = ( sLeft + sRight ) >> 1;

            sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                     aCursor->mSlotCount,
                                     sMid,
                                     (UChar**)&sScanInfo.mTRPHeader,
                                     &sIsValidSlot );
            IDE_ERROR( sIsValidSlot == ID_TRUE );

            IDE_TEST( sdtTempRow::fetch( sWASeg,
                                         aCursor->mWAGroupID,
                                         aHeader->mRowBuffer4Compare,
                                         &sScanInfo )
                      != IDE_SUCCESS );

            IDE_TEST( sCallBack->callback( &sResult,
                                           sScanInfo.mValuePtr,
                                           NULL,        /* aDirectKey */
                                           0,           /* aDirectKeyPartialSize */
                                           SC_NULL_GRID,
                                           sCallBack->data )
                      != IDE_SUCCESS );

            if ( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_FORWARD ) )
            {
                if ( sResult == ID_TRUE )
                {
                    sRight = sMid - 1;
                }
                else
                {
                    sLeft = sMid + 1;
                }
            }
            else
            {
                IDE_ERROR( SM_IS_FLAG_ON( aCursor->mTCFlag, SMI_TCFLAG_BACKWARD ) );
                if ( sResult == ID_TRUE )
                {
                    sLeft = sMid + 1;
                }
                else
                {
                    sRight = sMid - 1;
                }
            }
        } // while ( sLeft <= sRight )

        sMid = ( sLeft + sRight ) >> 1;

        if ( sHeight > 1 )
        {
            /* LeftMost�� Value�� ���� ����̱� ������, -1�� �� ����
             * �׷��Ƿ� 0���� ���������� */
            if ( sMid < 0 )
            {
                sMid = 0;
            }
            else
            {
                /* nothing to do */
            }
            SC_MAKE_GRID( aCursor->mGRID, aHeader->mSpaceID, sNPID, sMid );

            /* ChildNode Ž��. */
            IDE_TEST( sdtTempRow::fetchByGRID( sWASeg,
                                               aCursor->mWAGroupID,
                                               aCursor->mGRID,
                                               aHeader->mRowSize,
                                               aHeader->mRowBuffer4Fetch,
                                               &sScanInfo )
                      != IDE_SUCCESS );
            IDE_ERROR( SM_IS_FLAG_ON( sScanInfo.mTRPHeader->mTRFlag,
                                      SDT_TRFLAG_CHILDGRID ) );

            sNPID = SC_MAKE_PID( sScanInfo.mTRPHeader->mChildGRID );
        }
        else
        {
            /* Ž�� ������ */
            SC_MAKE_GRID( aCursor->mGRID, aHeader->mSpaceID, sNPID, sMid );
        }
        sHeight--;
    }// while ( sHeight > 0 )

    /* Ž���� ����� ������ */
    if ( SC_GRID_IS_NOT_NULL( aCursor->mGRID ) )
    {
        /* getPage�ص� */
        IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                  aHeader->mFetchGroupID,
                                                  aCursor->mGRID,
                                                  &aCursor->mWPID,
                                                  &aCursor->mWAPagePtr,
                                                  &aCursor->mSlotCount )
                  != IDE_SUCCESS );
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
 * Ŀ���κ��� Row�� �����ɴϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchInMemoryScanForward( smiSortTempCursor* aCursor,
                                                UChar  ** aRow,
                                                scGRID  * aRowGRID )
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aCursor->mTTHeader->mWASegment;
    sdtSortTRPHdr      * sTRPHeader;
    idBool               sResult = ID_FALSE;

    do
    {
        aCursor->mSeq++;
        if ( aCursor->mSeq >= aCursor->mLastSeq )
        {
            break;
        }

        IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                           aCursor->mSeq,
                                           (vULong*)&sTRPHeader )
                  != IDE_SUCCESS );

        /* InMemoryGroup���� ������ ���, Page�� �ƴ� SortHashMap����
         * Pointing�ؾ� �ϱ� ������, ������ ���� �����Ѵ�. */
        SC_MAKE_GRID( aCursor->mGRID,
                      SDT_SPACEID_WAMAP,
                      aCursor->mSeq,
                      0 );
        IDE_TEST( sdtTempRow::filteringAndFetch( aCursor,
                                                 sTRPHeader,
                                                 aRow,
                                                 aRowGRID,
                                                 &sResult )
                  != IDE_SUCCESS );
    }
    while( sResult == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� Row�� �����ɴϴ�. ( backward�� )
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchInMemoryScanBackward( smiSortTempCursor* aCursor,
                                                 UChar  ** aRow,
                                                 scGRID  * aRowGRID )
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aCursor->mTTHeader->mWASegment;
    sdtSortTRPHdr      * sTRPHeader;
    idBool               sResult = ID_FALSE;

    do
    {
        aCursor->mSeq--;
        if ( aCursor->mSeq <= aCursor->mLastSeq )
        {
            break;
        }

        IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                           aCursor->mSeq,
                                           (vULong*)&sTRPHeader )
                  != IDE_SUCCESS );

        /* InMemoryGroup���� ������ ���, Page�� �ƴ� SortHashMap����
         * Pointing�ؾ� �ϱ� ������, ������ ���� �����Ѵ�. */
        SC_MAKE_GRID( aCursor->mGRID,
                      SDT_SPACEID_WAMAP,
                      aCursor->mSeq,
                      0 );
        IDE_TEST( sdtTempRow::filteringAndFetch( aCursor,
                                                 sTRPHeader,
                                                 aRow,
                                                 aRowGRID,
                                                 &sResult )
                  != IDE_SUCCESS );
    }
    while( sResult == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� Row�� �����ɴϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchMergeScan( smiSortTempCursor* aCursor,
                                      UChar  ** aRow,
                                      scGRID  * aRowGRID )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)sHeader->mWASegment;
    sdtTempMergeRunInfo sTopRunInfo;
    scGRID               sGRID;
    idBool               sResult = ID_FALSE;

    /* ������ ��ġ�� GRID�� ���̺��� ���������� ������ GRID�� �ٸ���
       Ŀ���� �Ŵ޸� �������� �̿��� map �籸�� */
    if ( SC_GRID_IS_NOT_NULL(aCursor->mGRID) &&
         SC_GRID_IS_NOT_EQUAL(aCursor->mGRID, sHeader->mGRID) )
    {
        IDE_TEST( makeMergeRuns( sHeader,
                                 aCursor->mMergePosition )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    while( sResult == ID_FALSE )
    {
        /* Heap�� Top Slot�� �̾Ƴ� */
        IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                     1,   /* aIdx */
                                     (void*)&sTopRunInfo )
                  != IDE_SUCCESS );

        if ( sTopRunInfo.mRunNo == SDT_TEMP_RUNINFO_NULL )
        {
            /* Run���� Row���� ���� �̾Ƴ� */
            SC_MAKE_NULL_GRID( aCursor->mGRID );
            break;
        }

        getGRIDFromRunInfo( sHeader, &sTopRunInfo, &sGRID );
        /* ��ġ�� GRID */
        SC_COPY_GRID( sGRID, aCursor->mGRID );
        /* ���̺��� ���������� ������ GRID */
        SC_COPY_GRID( sGRID, sHeader->mGRID );

        IDE_TEST( sdtTempRow::filteringAndFetchByGRID( aCursor,
                                                       aCursor->mGRID,
                                                       aRow,
                                                       aRowGRID,
                                                       &sResult )
                  != IDE_SUCCESS );
        IDE_TEST( heapPop( sHeader ) != IDE_SUCCESS );
    }

    /* BUG-41284 : �ΰ� �̻��� Ŀ���� �����ϸ� Ŀ���� ���� �ϴ� ��������
     *  ���̺��� ���� �ִ� �������� �ٸ��� �����Ƿ�
     *  ���߿� �����Ҽ� �ֵ��� �������� �����ص� */
    IDE_TEST( makeMergePosition( sHeader,
                                 &aCursor->mMergePosition )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� Row�� �����ɴϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchIndexScanForward( smiSortTempCursor* aCursor,
                                             UChar  ** aRow,
                                             scGRID  * aRowGRID )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)sHeader->mWASegment;
    scPageID             sNPID;
    idBool               sIsValidSlot;
    const smiCallBack  * sCallBack = &aCursor->mRange->maximum;
    idBool               sResult = ID_FALSE;
    sdtSortScanInfo      sScanInfo;

    sScanInfo.mFetchEndOffset = sHeader->mRowSize;

    while ( ( sResult == ID_FALSE ) && ( SC_GRID_IS_NOT_NULL( aCursor->mGRID ) ) )
    {
        aCursor->mGRID.mOffset++;
        sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                 aCursor->mSlotCount,
                                 SC_MAKE_OFFSET( aCursor->mGRID ),
                                 (UChar**)&sScanInfo.mTRPHeader,
                                 &sIsValidSlot );

        if ( sIsValidSlot == ID_FALSE )
        {
            /*���� ��������*/
            sNPID = sdtTempPage::getNextPID( aCursor->mWAPagePtr );
            if ( sNPID == SC_NULL_PID )
            {
                SC_MAKE_NULL_GRID( aCursor->mGRID );
                break;
            }
            SC_MAKE_GRID( aCursor->mGRID, sHeader->mSpaceID, sNPID, -1 );

            IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                      sHeader->mFetchGroupID,
                                                      aCursor->mGRID,
                                                      &aCursor->mWPID,
                                                      &aCursor->mWAPagePtr,
                                                      &aCursor->mSlotCount )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( SM_IS_FLAG_ON( sScanInfo.mTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) )
            {
#ifdef DEBUG
                sdtWCB * sWCBPtr = sdtSortSegment::getWCBInternal( sWASeg,
                                                                   aCursor->mWPID );
                IDE_DASSERT( sWCBPtr->mWAPagePtr == aCursor->mWAPagePtr ) ;
                IDE_DASSERT( sWCBPtr->mNPageID   == SC_MAKE_PID( aCursor->mGRID ) ) ;
                IDE_DASSERT( sdtSortSegment::isFixedPage( sWCBPtr) == ID_TRUE );
#endif

                IDE_TEST( sdtTempRow::fetch( sWASeg,
                                             aCursor->mWAGroupID,
                                             sHeader->mRowBuffer4Fetch,
                                             &sScanInfo )
                          != IDE_SUCCESS );

                /******************** Range üũ *****************************/
                /* �� Node�� Range ������ �����Ѵ� ������ node���� �˻簡 �ʿ��� */
                IDE_TEST( sCallBack->callback( &sResult,
                                               sScanInfo.mValuePtr,
                                               NULL,        /* aDirectKey */
                                               0,           /* aDirectKeyPartialSize */
                                               SC_NULL_GRID,
                                               sCallBack->data )
                          != IDE_SUCCESS );
                if ( sResult == ID_FALSE )
                {
                    /* �� Node ������ Range�� ������. */
                    /* ���������� Ž�� �Ϸ��Ͽ��� */
                    break;
                }
                else
                {
                    /* ������ Slot�� MaxRange�� ���Եȴ�. ���� �� Node���� ���
                     * Data�� Ž�� ��� ���Եȴ�. */
                }

                IDE_TEST( sdtTempRow::filteringAndFetch( aCursor,
                                                         sScanInfo.mTRPHeader,
                                                         aRow,
                                                         aRowGRID,
                                                         &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� Row�� �����ɴϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchIndexScanBackward( smiSortTempCursor* aCursor,
                                              UChar  ** aRow,
                                              scGRID  * aRowGRID )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)sHeader->mWASegment;
    scPageID             sNPID;
    idBool               sIsValidSlot;
    const smiCallBack  * sCallBack = &aCursor->mRange->minimum;
    idBool               sResult = ID_FALSE;
    sdtSortScanInfo      sScanInfo;

    sScanInfo.mFetchEndOffset = sHeader->mRowSize;

    while ( ( sResult == ID_FALSE ) &&
            ( SC_GRID_IS_NOT_NULL( aCursor->mGRID ) ) )
    {
        if ( aCursor->mGRID.mOffset == 0 )
        {
            /*���� ��������*/
            sNPID = sdtTempPage::getPrevPID( aCursor->mWAPagePtr );
            if ( sNPID == SC_NULL_PID )
            {
                SC_MAKE_NULL_GRID( aCursor->mGRID );
                break;
            }
            SC_MAKE_GRID( aCursor->mGRID, sHeader->mSpaceID, sNPID,  0 );
            IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                      sHeader->mFetchGroupID,
                                                      aCursor->mGRID,
                                                      &aCursor->mWPID,
                                                      &aCursor->mWAPagePtr,
                                                      &aCursor->mSlotCount )
                      != IDE_SUCCESS );
            aCursor->mGRID.mOffset = aCursor->mSlotCount;
        }
        else
        {
            aCursor->mGRID.mOffset--;
            sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                     aCursor->mSlotCount,
                                     SC_MAKE_OFFSET( aCursor->mGRID ),
                                     (UChar**)&sScanInfo.mTRPHeader,
                                     &sIsValidSlot );
            IDE_ERROR( sIsValidSlot == ID_TRUE );

            if ( SM_IS_FLAG_ON( sScanInfo.mTRPHeader->mTRFlag, SDT_TRFLAG_HEAD ) )
            {
                IDE_TEST( sdtTempRow::fetch( sWASeg,
                                             aCursor->mWAGroupID,
                                             sHeader->mRowBuffer4Fetch,
                                             &sScanInfo )
                          != IDE_SUCCESS );

                /******************** Range üũ *****************************/
                /* �� Node�� Range�� �����ϴ� ������ node���� Ȯ���� �ʿ��� */
                IDE_TEST( sCallBack->callback( &sResult,
                                               sScanInfo.mValuePtr,
                                               NULL,        /* aDirectKey */
                                               0,           /* aDirectKeyPartialSize */
                                               SC_NULL_GRID,
                                               sCallBack->data )
                          != IDE_SUCCESS );

                if ( sResult == ID_FALSE )
                {
                    /* �� Node ������ Range�� ������. */
                    /* ���������� Ž�� �Ϸ��Ͽ��� */
                    break;
                }
                else
                {
                    /* MaxRange�� ���Եȴ�. ���� �� Node���� ���
                     * Data�� Ž�� ��� ���Եȴ�. */
                }

                IDE_TEST( sdtTempRow::filteringAndFetch( aCursor,
                                                         sScanInfo.mTRPHeader,
                                                         aRow,
                                                         aRowGRID,
                                                         &sResult )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���κ��� Row�� �����ɴϴ�. (BUG-39450)
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aRow           - ��� Row
 * aGRID          - ������ Row�� GRID
 ***************************************************************************/
IDE_RC sdtSortModule::fetchScan( smiSortTempCursor* aCursor,
                                 UChar  ** aRow,
                                 scGRID  * aRowGRID )
{
    smiTempTableHeader * sHeader;
    sdtSortSegHdr      * sWASeg;
    idBool               sResult = ID_FALSE;
    scPageID             sNPID;
    UChar              * sPtr = NULL;
    idBool               sIsValidSlot;
    scPageID           * sScanPos;
    UInt                 sIdx;

    IDE_ASSERT( aCursor != NULL );

    sHeader = (smiTempTableHeader *)aCursor->mTTHeader;
    sWASeg  = (sdtSortSegHdr*)sHeader->mWASegment;
    sScanPos = sHeader->mScanPosition;

    IDE_ERROR( sHeader->mTTState == SMI_TTSTATE_SORT_SCAN );
    IDE_ERROR( sHeader->mKeyColumnList == NULL );

    while( SC_GRID_IS_NOT_NULL( aCursor->mGRID ) )
    {
        sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                 aCursor->mSlotCount,
                                 SC_MAKE_OFFSET( aCursor->mGRID ),
                                 &sPtr,
                                 &sIsValidSlot );

        if ( sIsValidSlot == ID_FALSE )
        {
            /*���� ��������*/
            sNPID = sdtTempPage::getNextPID( aCursor->mWAPagePtr );

            if ( sNPID == SC_NULL_PID )
            {
                sIdx = sScanPos[ SDT_TEMP_SCANPOS_PINIDX ];

                if ( aCursor->mPinIdx == sIdx )
                {
                    /* nothing to do */
                }
                else
                {
                    /* ������ ��ġ�� page�� ���̺��� ���������� ������ page�� �ٸ���
                     *  cursor�� ���� �ϴ� ������ ���� */
                    sIdx = aCursor->mPinIdx;
                }
                sIdx++;

                /* ���̻� ���� Run �� ���� */
                if ( sScanPos[SDT_TEMP_SCANPOS_SIZEIDX] == sIdx )
                {
                    SC_MAKE_NULL_GRID( aCursor->mGRID );
                    break;
                }
                else
                {
                    /* ��ġ�� idx */
                    aCursor->mPinIdx = sIdx;
                    /* ���̺��� ���������� ������ idx */
                    sScanPos[ SDT_TEMP_SCANPOS_PINIDX ] = sIdx;
                    sNPID = sScanPos[ SDT_TEMP_SCANPOS_PIDIDX( sIdx ) ];
                }
            }
            else
            {
                /* nothing to do */
            }

            SC_MAKE_GRID( aCursor->mGRID, sHeader->mSpaceID, sNPID, 0 );
            IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                      aCursor->mWAGroupID,
                                                      aCursor->mGRID,
                                                      &aCursor->mWPID,
                                                      &aCursor->mWAPagePtr,
                                                      &aCursor->mSlotCount )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( SM_IS_FLAG_ON( ( (sdtSortTRPHdr*)sPtr )->mTRFlag, SDT_TRFLAG_HEAD ) )
            {
                /* �ش� Row�� ������ */
                IDE_TEST( sdtTempRow::filteringAndFetchByGRID( aCursor,
                                                               aCursor->mGRID,
                                                               aRow,
                                                               aRowGRID,
                                                               &sResult )

                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            aCursor->mGRID.mOffset++;

            if (sResult == ID_TRUE )
            {
                break;
            }
            else
            {
                /* nothing to do */
            }
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ��ġ�� �����մϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::storeCursorInMemoryScan( smiSortTempCursor* aCursor,
                                               smiTempPosition * aPosition )
{
    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN );
    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    SC_COPY_GRID( aCursor->mGRID, aPosition->mGRID );
    aPosition->mRowPtr = aCursor->mRowPtr;
    aPosition->mSeq    = aCursor->mSeq;


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ������ ��ġ�� �ǵ����ϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::restoreCursorInMemoryScan( smiSortTempCursor* aCursor,
                                                 smiTempPosition * aPosition )
{
    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN );

    SC_COPY_GRID( aPosition->mGRID, aCursor->mGRID );
    aCursor->mRowPtr = aPosition->mRowPtr;
    aCursor->mSeq    = aPosition->mSeq;

    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ��ġ�� �����մϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::storeCursorMergeScan( smiSortTempCursor* aCursor,
                                            smiTempPosition * aPosition )
{
    smiTempTableHeader * sHeader   = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg    = (sdtSortSegHdr*)sHeader->mWASegment;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_MERGESCAN );
    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    IDE_TEST( makeMergePosition( sHeader,
                                 &aPosition->mExtraInfo )
              != IDE_SUCCESS );

    SC_COPY_GRID( aCursor->mGRID, aPosition->mGRID );
    aPosition->mRowPtr = aCursor->mRowPtr;

    sdtSortSegment::convertFromWGRIDToNGRID( sWASeg,
                                             aPosition->mGRID,
                                             &aPosition->mGRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ������ ��ġ�� �ǵ����ϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::restoreCursorMergeScan( smiSortTempCursor* aCursor,
                                              smiTempPosition * aPosition )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)sHeader->mWASegment;
    sdtWCB             * sWCBPtr;
    idBool               sIsValid;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_MERGESCAN );

    IDE_TEST( makeMergeRuns( sHeader,
                             aPosition->mExtraInfo )
              != IDE_SUCCESS );
    SC_COPY_GRID( aPosition->mGRID, aCursor->mGRID );

    /* ���̺��� ���������� ������ GRID
       ��Ȯ�ϰԴ� smiTempTable::restoreCursor���� sHeader->mGRID ���ŵ�. */
    SC_COPY_GRID( aPosition->mGRID, sHeader->mGRID );

    if ( aPosition->mRowPtr == NULL )
    {
        aCursor->mRowPtr = NULL;
    }
    else
    {
        sWCBPtr = sdtSortSegment::findWCB( sWASeg, SC_MAKE_PID( aPosition->mGRID ) );

        // makeMergeRuns()���� load�Ǿ���. �ݵ�� �־�� �Ѵ�.
        IDE_ERROR( sWCBPtr != NULL );

        if (( aPosition->mRowPtr > sWCBPtr->mWAPagePtr ) &&
            ( aPosition->mRowPtr < ( sWCBPtr->mWAPagePtr + SD_PAGE_SIZE )))
        {
            // aPosition->mRowPtr�� NULL�̰ų�,
            // WAPagePtr���� ���̸� Cursor�� �����Ѵ�.
            // PageHdr�� Row Hdr�� �����Ƿ� <=,>= �� �˻��� �ʿ�� ����.
            aCursor->mRowPtr = aPosition->mRowPtr;
        }
        else
        {
            IDE_ERROR( sdtTempPage::getSelfPID( sWCBPtr->mWAPagePtr ) == SC_MAKE_PID( aCursor->mGRID ) );

            // BUG-46322 WAPage�� Pointer�� ���� �Ǿ��ٸ�, �ٽ� �о�´�.
            sdtSortSegment::getSlot( sWCBPtr->mWAPagePtr,
                                     sdtTempPage::getSlotCount( sWCBPtr->mWAPagePtr ),
                                     SC_MAKE_OFFSET( aCursor->mGRID ),
                                     &aCursor->mRowPtr,
                                     &sIsValid );

            IDE_ERROR( sIsValid == ID_TRUE );
        }

        IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                                  SDT_TRFLAG_HEAD ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ��ġ�� �����մϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::storeCursorIndexScan( smiSortTempCursor* aCursor,
                                            smiTempPosition * aPosition )
{
    sdtSortSegHdr      * sWASeg    = (sdtSortSegHdr*)aCursor->mTTHeader->mWASegment;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_INDEXSCAN );
    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    SC_COPY_GRID( aCursor->mGRID, aPosition->mGRID );
    aPosition->mRowPtr    = aCursor->mRowPtr;
    aPosition->mWAPagePtr = aCursor->mWAPagePtr;
    aPosition->mSlotCount = aCursor->mSlotCount;
    aPosition->mWPID      = aCursor->mWPID;

    IDE_DASSERT( aPosition->mWPID != SM_NULL_PID );

    sdtSortSegment::convertFromWGRIDToNGRID( sWASeg,
                                             aPosition->mGRID,
                                             &aPosition->mGRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ������ ��ġ�� �ǵ����ϴ�.
 *
 * <IN>
 * aCursor        - ��� Cursor
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::restoreCursorIndexScan( smiSortTempCursor* aCursor,
                                              smiTempPosition * aPosition )
{
    smiTempTableHeader * sHeader = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)sHeader->mWASegment;
    UChar              * sPtr;
    sdtWCB             * sWCBPtr;
    idBool               sIsValidSlot;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_INDEXSCAN );

    SC_COPY_GRID( aPosition->mGRID, aCursor->mGRID );
    aCursor->mRowPtr    = aPosition->mRowPtr;
    aCursor->mWAPagePtr = aPosition->mWAPagePtr;
    aCursor->mSlotCount = aPosition->mSlotCount;

    IDE_DASSERT( aCursor->mWPID   != SM_NULL_PID );
    IDE_DASSERT( aPosition->mWPID != SM_NULL_PID );

    sWCBPtr = sdtSortSegment::getWCBInternal( sWASeg,
                                              aPosition->mWPID );

    IDE_DASSERT( sWCBPtr->mWAPagePtr == aCursor->mWAPagePtr );

    if ( sWCBPtr->mNPageID != aCursor->mGRID.mPageID )
    {
        /* �� ���� ���� Position���� �о��� page�� ����Ǿ���.
         * �ٽ� ����.*/
        IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                  sHeader->mFetchGroupID,
                                                  aCursor->mGRID,
                                                  &aCursor->mWPID,
                                                  &aCursor->mWAPagePtr,
                                                  &aCursor->mSlotCount )
                  != IDE_SUCCESS );
        sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                 aCursor->mSlotCount,
                                 SC_MAKE_OFFSET( aCursor->mGRID ),
                                 &sPtr,
                                 &sIsValidSlot );
        IDE_ERROR( sIsValidSlot == ID_TRUE );

        aCursor->mRowPtr = sPtr;
    }
    else
    {
        if ( aCursor->mWPID != aPosition->mWPID )
        {
            sdtSortSegment::unfixWAPage( sWASeg, aCursor->mWPID );
            aCursor->mWPID = aPosition->mWPID;
            sdtSortSegment::fixWAPage( sWCBPtr );
        }
    }

    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ��ġ�� �����մϴ�. (BUG-39450)
 *
 * <IN>
 * aCursor        - ��� Cursor
 * <OUT>
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::storeCursorScan( smiSortTempCursor* aCursor,
                                       smiTempPosition * aPosition )
{
    sdtSortSegHdr * sWASeg = (sdtSortSegHdr*)aCursor->mTTHeader->mWASegment;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_SCAN );
    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    SC_COPY_GRID( aCursor->mGRID, aPosition->mGRID );
    aPosition->mRowPtr = aCursor->mRowPtr;
    aPosition->mPinIdx = aCursor->mPinIdx;
    aPosition->mWPID   = aCursor->mWPID;

    IDE_DASSERT( aPosition->mWPID != SM_NULL_PID );

    sdtSortSegment::convertFromWGRIDToNGRID( sWASeg,
                                             aPosition->mGRID,
                                             &aPosition->mGRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Ŀ���� ������ ��ġ�� �ǵ����ϴ�. (BUG-39450)
 *
 * <IN>
 * aCursor        - ��� Cursor
 * aPosition      - ������ ��ġ
 ***************************************************************************/
IDE_RC sdtSortModule::restoreCursorScan( smiSortTempCursor* aCursor,
                                         smiTempPosition * aPosition )
{
    smiTempTableHeader * sHeader   = aCursor->mTTHeader;
    sdtSortSegHdr      * sWASeg    = (sdtSortSegHdr*)sHeader->mWASegment;
    UChar              * sPtr;
    sdtWCB             * sWCBPtr;
    idBool               sIsValidSlot;

    IDE_ERROR( aPosition->mOwner == aCursor );
    IDE_ERROR( aPosition->mTTState == SMI_TTSTATE_SORT_SCAN );
    IDE_ERROR( sHeader->mScanPosition != NULL);

    SC_COPY_GRID( aPosition->mGRID, aCursor->mGRID );
    aCursor->mRowPtr = aPosition->mRowPtr;
    aCursor->mPinIdx = aPosition->mPinIdx;

    IDE_DASSERT( aCursor->mWPID   != SM_NULL_PID );
    IDE_DASSERT( aPosition->mWPID != SM_NULL_PID );

    sWCBPtr = sdtSortSegment::getWCBInternal( sWASeg,
                                              aPosition->mWPID );

    IDE_DASSERT( sWCBPtr->mWAPagePtr == aCursor->mWAPagePtr );

    if ( sWCBPtr->mNPageID != aCursor->mGRID.mPageID )
    {
        /* �� ���� ���� Position���� �о��� page�� ����Ǿ���.
         * �ٽ� ����.*/
        IDE_TEST( sdtSortSegment::getPageWithFix( sWASeg,
                                                  sHeader->mFetchGroupID,
                                                  aCursor->mGRID,
                                                  &aCursor->mWPID,
                                                  &aCursor->mWAPagePtr,
                                                  &aCursor->mSlotCount )

                  != IDE_SUCCESS );
        sdtSortSegment::getSlot( aCursor->mWAPagePtr,
                                 aCursor->mSlotCount,
                                 SC_MAKE_OFFSET( aCursor->mGRID ),
                                 &sPtr,
                                 &sIsValidSlot );
        IDE_ERROR( sIsValidSlot == ID_TRUE );

        aCursor->mRowPtr = sPtr;
    }
    else
    {
        if ( aCursor->mWPID != aPosition->mWPID )
        {
            sdtSortSegment::unfixWAPage( sWASeg, aCursor->mWPID );
            aCursor->mWPID = aPosition->mWPID;
            sdtSortSegment::fixWAPage( sWCBPtr );
        }
    }

    IDE_ERROR( SM_IS_FLAG_ON( ((sdtSortTRPHdr*)aCursor->mRowPtr)->mTRFlag,
                              SDT_TRFLAG_HEAD ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * ������ ������ Row�� �����Ͽ� �������մϴ�.
 *
 * �����͸� ������ �����ϴ� extractNSort �۾��� �����Ѵ�.
 * �Ϸ�Ǹ� ������ merge �Լ��� ȣ���Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::extractNSort(smiTempTableHeader * aHeader)
{
    scPageID            sNPID;
    scPageID            sNextNPID;
    scPageID            sWPID;
    UInt                sInitGroupPageCount;
    UInt                sSortGroupPageCount;
    UInt                sFlushGroupPageCount;
    UChar             * sWAPagePtr = NULL;
    scGRID              sGRID ;
    UInt                sSlotCount = 0;
    UInt                sWAMapIdx  = 0;
    sdtSortSegHdr     * sWASeg     = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortInsertResult   sTRInsertResult;
    UInt                i;

    /* Index�ܰ迡���� ��� Group�� ����. */
    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_LNODE,
                                           ID_TRUE ) /*wait4flush */
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_INODE,
                                           ID_TRUE ) /*wait4flush */
              != IDE_SUCCESS );

    /* InsertNSort�� ���� ����� Group���� ����� */
    sInitGroupPageCount = sdtSortSegment::getAllocableWAGroupPageCount(
        sWASeg, SDT_WAGROUPID_INIT );

    /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
    IDE_ERROR( sInitGroupPageCount >= ( SDT_WAGROUP_MIN_PAGECOUNT * 3 ) );

    sSortGroupPageCount = sInitGroupPageCount * aHeader->mWorkGroupRatio / 100;

    if ( sSortGroupPageCount < SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
        sSortGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT;
    }

    sFlushGroupPageCount = ( sInitGroupPageCount - sSortGroupPageCount ) / 2;

    if ( sFlushGroupPageCount < SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
        sFlushGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT;
        sSortGroupPageCount = sInitGroupPageCount - ( sFlushGroupPageCount * 2 );
    }

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_SORT,
                                             sSortGroupPageCount,
                                             SDT_WA_REUSE_INMEMORY )
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_FLUSH,
                                             sFlushGroupPageCount,
                                             SDT_WA_REUSE_FIFO )
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_SUBFLUSH,
                                             sFlushGroupPageCount,
                                             SDT_WA_REUSE_FIFO )
              != IDE_SUCCESS );

    IDE_TEST( sdtWASortMap::create( sWASeg,
                                    aHeader->mSortGroupID,
                                    SDT_WM_TYPE_POINTER,
                                    0, /* Slot Count */
                                    2, /* aVersionCount */
                                    &sWASeg->mSortMapHdr )
              != IDE_SUCCESS );

    aHeader->mTTState = SMI_TTSTATE_SORT_EXTRACTNSORT;

    sNPID = aHeader->mRowHeadNPID;
    while( sNPID != SC_NULL_PID )
    {
        SC_MAKE_GRID( sGRID, aHeader->mSpaceID, sNPID, 0 );
        IDE_TEST( sdtSortSegment::getPage( sWASeg,
                                           SDT_WAGROUPID_SUBFLUSH,
                                           sGRID,
                                           &sWPID,
                                           &sWAPagePtr )
                  != IDE_SUCCESS );
        sSlotCount = sdtTempPage::getSlotCount(sWAPagePtr);
        sNextNPID = sdtTempPage::getNextPID( sWAPagePtr );

        for( i = 0 ; i < sSlotCount ; i++ )
        {
            while( 1 )
            {
                SC_MAKE_GRID( sGRID, aHeader->mSpaceID, sNPID, i );
                IDE_TEST( copyRowByGRID( aHeader,
                                         sGRID,
                                         SDT_COPY_EXTRACT_ROW,
                                         SDT_TEMP_PAGETYPE_INMEMORYGROUP,
                                         SC_NULL_GRID, /* ChildRID */
                                         &sTRInsertResult )
                          != IDE_SUCCESS );

                /* WAMap�� ���� ���� */
                if ( sTRInsertResult.mComplete == ID_TRUE )
                {
                    /* Row�� ������ �����Ͽ��� */
                    IDE_TEST( sdtWASortMap::expand(
                                  &sWASeg->mSortMapHdr,
                                  SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID ),
                                  &sWAMapIdx )
                              != IDE_SUCCESS );
                    if ( sWAMapIdx != SDT_WASLOT_UNUSED )
                    {
                        IDE_TEST( sdtWASortMap::setvULong(
                                      &sWASeg->mSortMapHdr,
                                      sWAMapIdx,
                                      (vULong*)&sTRInsertResult.mHeadRowpiecePtr )
                                  != IDE_SUCCESS );
                        break;
                    }
                }

                /* BUG-46438 �ѹ��� �� �ǵ� ���� ���� ������ ������ �۴ٸ�,
                 * sort temp�� ��� �� �� ����.*/
                IDE_TEST_RAISE( sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) == 0 ,
                                error_invalid_sortareasize );

                IDE_TEST( sortSortGroup( aHeader ) != IDE_SUCCESS );
                IDE_TEST( storeSortedRun( aHeader ) != IDE_SUCCESS );
            }
        }

        sNPID = sNextNPID;
        if ( sNPID == SC_NULL_PID )
        {
            break;
        }
    } // end of while ( sNPID != SC_NULL_PID )
    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_SUBFLUSH,
                                           ID_TRUE ) /*wait4flush */
              != IDE_SUCCESS );

    /* �� ������, merge, makeIndex, indexScan�� ������ */
    IDE_TEST( merge( aHeader ) != IDE_SUCCESS );
    IDE_TEST( makeIndex( aHeader ) != IDE_SUCCESS );
    IDE_TEST( indexScan( aHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_sortareasize);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_SORTAREASIZE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * run���� merge�Ѵ�.
 **************************************************************************/
IDE_RC sdtSortModule::merge(smiTempTableHeader * aHeader)
{
    sdtTempMergeRunInfo   sTopRunInfo;
    scGRID                sGRID;
    scPageID              sHeadNPID = SD_NULL_PID;
    sdtSortSegHdr       * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortInsertResult sTRInsertResult;

    IDE_ERROR( ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT  ) ||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_EXTRACTNSORT ) );
    IDE_ERROR( aHeader->mKeyColumnList != NULL );

    /* ������ Run�� Slot ���� */
    aHeader->mStatsPtr->mExtraStat2 =
        sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr );

    /* ������ Run�� �����ؼ� ���� */
    if ( sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) > 0 )
    {
        IDE_TEST( sortSortGroup( aHeader ) != IDE_SUCCESS );
        IDE_TEST( storeSortedRun( aHeader ) != IDE_SUCCESS );
    }

    aHeader->mTTState = SMI_TTSTATE_SORT_MERGE;

    /* SortGroup�� �ʱ�ȭ�Ѵ�. Merge�� �������� ����� �����̴�. */
    IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                            SDT_WAGROUPID_SORT,
                                            ID_FALSE/*wait4flsuh*/ )
              != IDE_SUCCESS );
    IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                            SDT_WAGROUPID_FLUSH,
                                            ID_TRUE/*wait4flsuh*/ )
              != IDE_SUCCESS );

    while( 1 )
    {
        IDE_TEST( heapInit( aHeader ) != IDE_SUCCESS );

        if ( aHeader->mRunQueue.getQueueLength() == 0 )
        {
            /* ���̻� ���� Run�� ����. �� �̹� Merge�� �Ϸ��
             * ���� ���·� ������ */
            break;
        }

        sHeadNPID = SD_NULL_PID;
        while(1)
        {
            /* Heap�� Top Slot�� �̾Ƴ� */
            IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                         1,
                                         (void*)&sTopRunInfo )
                      != IDE_SUCCESS );

            if ( sTopRunInfo.mRunNo == SDT_TEMP_RUNINFO_NULL )
            {
                /* Run���� Row���� ���� �̾Ƴ� */
                break;
            }

            getGRIDFromRunInfo( aHeader, &sTopRunInfo, &sGRID );

            IDE_TEST( copyRowByGRID( aHeader,
                                     sGRID,
                                     SDT_COPY_NORMAL,
                                     SDT_TEMP_PAGETYPE_SORTEDRUN,
                                     SC_NULL_GRID, /* ChildRID */
                                     &sTRInsertResult )
                      != IDE_SUCCESS );

            if ( sHeadNPID == SD_NULL_PID )
            {
                sHeadNPID = SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID );
            }

            /* ���� �������� ���� Row���ġ�� ���� CurRowPageCount��
             * ������ �� ������, �ٽ� ����� */
            aHeader->mMaxRowPageCount = IDL_MAX(
                aHeader->mMaxRowPageCount, sTRInsertResult.mRowPageCount );

            IDE_TEST( heapPop( aHeader ) != IDE_SUCCESS );
        }

        IDE_TEST( aHeader->mRunQueue.enqueue(ID_FALSE, (void*)&sHeadNPID )
                  != IDE_SUCCESS );

        IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                                SDT_WAGROUPID_FLUSH,
                                                ID_TRUE/*wait4flsuh*/ )
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(aHeader->mStatistics) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * Index�� �����Ѵ�.
 **************************************************************************/
IDE_RC sdtSortModule::makeIndex(smiTempTableHeader * aHeader)
{

    UInt                   sInitGroupPageCount;
    UInt                   sINodeGroupPageCount;
    UInt                   sLNodeGroupPageCount;
    scPageID               sChildNPID;
    idBool                 sNeedMore = ID_FALSE;
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;

    IDE_ERROR( aHeader->mTTState == SMI_TTSTATE_SORT_MERGE );
    aHeader->mTTState  = SMI_TTSTATE_SORT_MAKETREE;

    /* �̹� MergeRun�� �����Ǿ��ִ� ���·� ��. ���� RunQueue�� ������� */
    IDE_ERROR( aHeader->mRunQueue.getQueueLength() == 0 );

    sInitGroupPageCount = sdtSortSegment::getAllocableWAGroupPageCount(
        sWASeg,
        SDT_WAGROUPID_INIT );

    if ( sInitGroupPageCount >= SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �ӽ÷� sub flush�� ������� �����ϴ� ��찡 �ִ�.
         * �׷��Ƿ� init group�� ���� sub flush group�� ������
         * ����ִ� page�� �ִ��� Ȯ���ؼ� �ִٸ� �� ���� page��
         * sub flush group�� �����.*/
        IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                                 SDT_WAGROUPID_SUBFLUSH,
                                                 sInitGroupPageCount,
                                                 SDT_WA_REUSE_FIFO )
              != IDE_SUCCESS );
    }
    else
    {
        /* ExtraRow�� �����ϱ� ���� ��� �����Ѵ�. */
        IDE_TEST( sdtSortSegment::splitWAGroup( sWASeg,
                                                SDT_WAGROUPID_FLUSH,
                                                SDT_WAGROUPID_SUBFLUSH,
                                                SDT_WA_REUSE_FIFO )
                  != IDE_SUCCESS );
    }

    IDE_TEST( makeLNodes( aHeader, &sNeedMore ) != IDE_SUCCESS );

    /*********************** Group �籸�� *****************************/
    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_SUBFLUSH,
                                           ID_FALSE ) /*wait4flush */
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_FLUSH,
                                           ID_FALSE ) /*wait4flush */
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_SORT,
                                           ID_FALSE ) /*wait4flush */
              != IDE_SUCCESS );

    sInitGroupPageCount = sdtSortSegment::getAllocableWAGroupPageCount(
        sWASeg,
        SDT_WAGROUPID_INIT );

    /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
    IDE_ERROR( sInitGroupPageCount >= ( SDT_WAGROUP_MIN_PAGECOUNT * 2 ) );

    sLNodeGroupPageCount = sInitGroupPageCount * aHeader->mWorkGroupRatio / 100;

    if ( sLNodeGroupPageCount < SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
        sLNodeGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT;
    }

    sINodeGroupPageCount = sInitGroupPageCount - sLNodeGroupPageCount;

    if ( sINodeGroupPageCount < SDT_WAGROUP_MIN_PAGECOUNT )
    {
        /* BUG-46427 �� group�� 2�� �̻��� page�� �־�� �Ѵ�. */
        sINodeGroupPageCount = SDT_WAGROUP_MIN_PAGECOUNT;
        sLNodeGroupPageCount = sInitGroupPageCount - sINodeGroupPageCount ;
    }

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_INODE,
                                             sINodeGroupPageCount,
                                             SDT_WA_REUSE_LRU )
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_LNODE,
                                             sLNodeGroupPageCount,
                                             SDT_WA_REUSE_LRU )
              != IDE_SUCCESS );

    /* IndexScan������ Map�� ���̻� ��� ���� */
    sdtWASortMap::disable( sWASeg );

    sChildNPID = aHeader->mRowHeadNPID;
    while( sNeedMore == ID_TRUE )
    {
        IDE_TEST( makeINodes( aHeader, &sChildNPID, &sNeedMore )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Index�� LeafNode�� �����Ѵ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * <OUT>
 * aNeedMoreINode - �߰������� INode�� ������ �ϴ°�?
 **************************************************************************/
IDE_RC sdtSortModule::makeLNodes( smiTempTableHeader * aHeader,
                                  idBool             * aNeedMoreINode )
{
    sdtTempMergeRunInfo   sTopRunInfo;
    scPageID               sHeadNPID = SD_NULL_PID;
    scGRID                 sGRID;
    scPageID               sInsertedNPID;
    sdtSortInsertResult  sTRInsertResult;
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;

    *aNeedMoreINode = ID_FALSE;

    /*  Heap���� Top Slot�� �̾Ƴ� */
    IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                 1,
                                 (void*)&sTopRunInfo )
              != IDE_SUCCESS );
    while( sTopRunInfo.mRunNo != SDT_TEMP_RUNINFO_NULL )
    {
        getGRIDFromRunInfo( aHeader, &sTopRunInfo, &sGRID );

        IDE_TEST( copyRowByGRID( aHeader,
                                 sGRID,
                                 SDT_COPY_MAKE_LNODE,
                                 SDT_TEMP_PAGETYPE_LNODE,
                                 SC_NULL_GRID, /*ChildRID*/
                                 &sTRInsertResult )
                  != IDE_SUCCESS );
        sInsertedNPID = SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID );

        if ( sHeadNPID == SD_NULL_PID ) /* ���� ���� */
        {
            sHeadNPID = sInsertedNPID;

            aHeader->mRowHeadNPID = sHeadNPID;
            aHeader->mRootWPID    = sHeadNPID;
            aHeader->mHeight      = 1;
        }
        else
        {
            if ( sHeadNPID != sInsertedNPID ) /* ��尡 2�� �̻� */
            {
                *aNeedMoreINode = ID_TRUE;
            }
        }

        IDE_TEST( heapPop( aHeader ) != IDE_SUCCESS ); /* ���� row�� ������*/

        IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                     1,
                                     (void*)&sTopRunInfo )
                  != IDE_SUCCESS );
    } /*while */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Index�� INode�� �����Ѵ�.
 *
 * - INode�� ����� Logic ����
 * ���� Ʈ���� �������� �����Ѵ�.
 *       D-----+
 *    +--|1 3 7|
 *    |  +-----+
 *    |   |    |
 * A---+ B---+ C---+
 * |1 2|-|3 5|-|7 9|
 * +---+ +---+ +---+
 *
 * 1) resetWAGroup�� �ʿ伺
 *   A,B,C ��带 ���� �� D��带 ����� ���� �� ������ ������,
 * resetWAGroup�� ���� ������, C�� D�� �̿� ���� ���� �ȴ�.
 * ���� resetWAGroup�� ���� HintPage�� �ʱ�ȭ, �̿��� �ƴϰ� �Ѵ�.
 *
 * 2) INode���� ���.
 *  INode�� �����ϰ� ChildNode���� 0��° Key���� ���οø��� �ȴ�.
 * ���� ���, A,B,C �� ����� 1,3,7 �� �ö���� �ȴ�.
 *  ���� 1���� ��� LeftMost�� ������ �����Ѵ�.
 *
 * * �� ����� BufferMiss�� �Ͼ�� ���� ȿ���� �״��� ���� �ʴ�. ������
 * RangeScan�� ���� ���� SortJoin���̸�, �� ��쵵 ��κ� InMemory�� Ǯ����.
 * ���� INode�� ����� ���� ����, �� INode�� Page���� ���⿡ �̸� ���Ѵ�.
 *   ���� ���� ������ �����ϴ�. ( sdtBUBuild::makeTree���� )
 *
 * <IN>
 * aHeader        - ��� Table
 * <IN/OUT>
 * aChildNPID     - ���� ��� ChildNPID. ������, ���� ChildNPID�� ��ȯ��.
 * <OUT>
 * aNeedMoreINode - �߰������� INode�� ������ �ϴ°�?
 *****************************************************************************/
IDE_RC sdtSortModule::makeINodes( smiTempTableHeader * aHeader,
                                  scPageID           * aChildNPID,
                                  idBool             * aNeedMoreINode )
{
    scPageID               sHeadNPID = SD_NULL_PID;
    scPageID               sChildNPID;
    scPageID               sWPID;
    scPageID               sInsertedNPID;
    scGRID                 sGRID;
    sdtSortInsertResult  sTRInsertResult;
    UChar                * sWAPagePtr = NULL;
    sdtSortSegHdr        * sWASeg     = (sdtSortSegHdr*)aHeader->mWASegment;

    IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                            SDT_WAGROUPID_INODE,
                                            ID_TRUE/*wait4flsuh*/ )
              != IDE_SUCCESS );

    sChildNPID      = *aChildNPID;
    sHeadNPID       = SD_NULL_PID;
    *aNeedMoreINode = ID_FALSE;
    while( sChildNPID != SC_NULL_PID )
    {
        /* ������ ���� Node�鿡������ ���� Ž���� �õ��� */
        SC_MAKE_GRID( sGRID, aHeader->mSpaceID, sChildNPID, 0 );
        IDE_TEST( copyRowByGRID( aHeader,
                                 sGRID,
                                 SDT_COPY_MAKE_INODE,
                                 SDT_TEMP_PAGETYPE_INODE,
                                 sGRID,        /*ChildRID*/
                                 &sTRInsertResult )
                  != IDE_SUCCESS );
        sInsertedNPID = SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID );

        if ( sHeadNPID == SD_NULL_PID )
        {
            sHeadNPID = sInsertedNPID;

            aHeader->mRootWPID = sHeadNPID;
            aHeader->mHeight++;

            /* �ƹ��� �����ص�, 1024�̻� ���̰� �Ǳ� ����� */
            IDE_ERROR( aHeader->mHeight <= 1024 );
        }
        else
        {
            if ( sHeadNPID != sInsertedNPID )
            {
                *aNeedMoreINode = ID_TRUE;
            }
        }

        /* ���� �������� ID�� �����´�. */
        IDE_TEST( sdtSortSegment::getPage( sWASeg,
                                           SDT_WAGROUPID_LNODE,
                                           sGRID,
                                           &sWPID,
                                           &sWAPagePtr )
                  != IDE_SUCCESS );
        sChildNPID = sdtTempPage::getNextPID( sWAPagePtr );
    } /*while */

    *aChildNPID = sHeadNPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * InMemoryScan���·� �����. ���� ���Ե� Wa���� Row���� �����Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::inMemoryScan(smiTempTableHeader * aHeader)
{
    smiColumnList  * sColumn = (smiColumnList*)aHeader->mKeyColumnList;

    if ( sColumn != NULL )
    {
        IDE_TEST( sortSortGroup( aHeader ) != IDE_SUCCESS );
    }
    aHeader->mTTState = SMI_TTSTATE_SORT_INMEMORYSCAN;

    aHeader->mOpenCursor    = openCursorInMemoryScan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * mergeScan ���·� �����.
 ***************************************************************************/
IDE_RC sdtSortModule::mergeScan(smiTempTableHeader * aHeader)
{
    aHeader->mTTState = SMI_TTSTATE_SORT_MERGESCAN;

    IDE_TEST( makeMergePosition( aHeader,
                                 &aHeader->mInitMergePosition )
              != IDE_SUCCESS );

    aHeader->mOpenCursor    = openCursorMergeScan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * indexScan���·� �����.
 ***************************************************************************/
IDE_RC sdtSortModule::indexScan(smiTempTableHeader * aHeader)
{
    aHeader->mTTState = SMI_TTSTATE_SORT_INDEXSCAN;

    aHeader->mOpenCursor    = openCursorIndexScan;

    return IDE_SUCCESS;
}

/**************************************************************************
 * Description :
 * fullScan���·� �����.
 ***************************************************************************/
IDE_RC sdtSortModule::scan(smiTempTableHeader * aHeader)
{
    aHeader->mTTState = SMI_TTSTATE_SORT_SCAN;

    IDE_TEST( makeScanPosition( aHeader,
                                &aHeader->mScanPosition )
              != IDE_SUCCESS );

    aHeader->mOpenCursor    = openCursorScan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * SortGroup�� Row���� �����Ѵ�.
 *
 * * ����
 * Quicksort�� �κ����� �� ��, Merge�Ѵ�.
 * �ֳ��ϸ� QuickSort�� ��� �־��� ��� n^2 ��ŭ Compare�ϱ� ������ Row��
 * ������ �����ϸ� �Ƿ� ������ �� �ֱ� �����̴�. ���� PartitionSize�� ����
 * ������ �ش� ũ�⾿ QuickSort�� ������ ��, �ش� Partition���� merge�Ѵ�.
 *
 * * MergeSort�� Version
 *   MergeSort��, ���� �� Slot�� ��ġ�� �����ϸ鼭, ���ĵ� Slot���� ������
 * ��ġ�� �� �ʿ�� �Ѵ�. ���� WAMap�� Versioning�Ѵ�.
 *
 * - ��
 *           <-Left> <-Right>
 *           @       @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: . . . . . . . .
 *
 *             @     @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 . . . . . . .
 *
 *             @       @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 2 . . . . . .
 *
 *             @         @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 2 3 . . . . .
 *
 *             @           @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 2 3 4 . . . .
 *
 *               @         @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 2 3 4 5 . . .
 *
 * ...
 *
 *                  @       @
 * Version0: 1 5 6 9 2 3 4 8 3 5 7 8
 * Version1: 1 2 3 4 5 6 8 9
 *
 * -- incVersionIdx() --
 *
 *           @               @
 * Version1: 1 2 3 4 5 6 8 9 3 5 7 8
 * Version2: 1 5 6 9 2 3 4 8         <- ������ Version0�̾���
 * �ٽ� Merge�� �ݺ���
 *
 ***************************************************************************/
IDE_RC sdtSortModule::sortSortGroup(smiTempTableHeader * aHeader)
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtWASortMapHdr    * sMapHdr = &sWASeg->mSortMapHdr;
    UInt                 sSlotCount;
    UInt                 sPartitionSize;
    vULong               sPtr;
    UInt                 i;
    UInt                 j;

    IDE_ERROR( ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT ) ||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_EXTRACTNSORT )||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_INMEMORYSCAN ) );

    IDE_ERROR ( aHeader->mKeyColumnList != NULL );

    sPartitionSize = smuProperty::getTempSortPartitionSize();
    sSlotCount     = sdtWASortMap::getSlotCount( sMapHdr );

    if ( sSlotCount > 0 )
    {
        if ( sPartitionSize == 0 )
        {
            IDE_TEST( quickSort( aHeader, 0, sSlotCount - 1 )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Partition���� ����� �κ� QuickSort�� */
            for( i = 0 ; i < sSlotCount ; i += sPartitionSize )
            {
                j = IDL_MIN ( i + sPartitionSize,
                              sSlotCount );

                IDE_TEST( quickSort( aHeader, i, j - 1 )
                          != IDE_SUCCESS );
            }

            /* MergeSort�� ������ */
            for( ; sPartitionSize < sSlotCount; sPartitionSize *= 2 )
            {
                for( i = 0 ; i < sSlotCount; i += sPartitionSize * 2 )
                {
                    if ( i + sPartitionSize < sSlotCount )
                    {
                        if ( i + sPartitionSize*2 > sSlotCount )
                        {
                            /* |----+-|
                             *
                             *  <--> <> */
                            IDE_TEST( mergeSort( aHeader,
                                                 i,
                                                 i + sPartitionSize - 1,
                                                 i + sPartitionSize,
                                                 sSlotCount - 1)
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* |----+----+--|
                             *
                             *  <--> <--> */

                            IDE_TEST( mergeSort( aHeader,
                                                 i,
                                                 i + sPartitionSize - 1,
                                                 i + sPartitionSize,
                                                 i + sPartitionSize * 2 -1 )
                                      != IDE_SUCCESS );
                        }
                    }
                    else
                    {
                        /* |--|
                         *
                         *  ~~~ <- ����, Right���� ���� �񱳾���
                         * �ٸ� Map������ ���� Version���� �Űܴ� ����� */
                        for ( j = i ; j < sSlotCount ; j++ )
                        {
                            IDE_TEST( sdtWASortMap::getvULong( sMapHdr,
                                                               j,
                                                               &sPtr )
                                      != IDE_SUCCESS );
                            IDE_TEST( sdtWASortMap::setNextVersion( sMapHdr,
                                                                    j,
                                                                    &sPtr)
                                      != IDE_SUCCESS );

                        }
                    }
                }

                IDE_TEST( sdtWASortMap::incVersionIdx( sMapHdr )
                          != IDE_SUCCESS );
            }
        }
    }
    IDE_TEST(iduCheckSessionEvent(aHeader->mStatistics) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * QuickSort�� �����Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::quickSort( smiTempTableHeader * aHeader,
                                 UInt                 aLeftPos,
                                 UInt                 aRightPos )
{
    smnSortStack         sCurStack;
    smnSortStack         sNewStack;
    idBool               sEmpty;
    SInt                 sLeft;
    SInt                 sRight;
    SInt                 sMid;
    sdtSortScanInfo      sPivotTRPInfo;
    sdtSortScanInfo      sTargetTRPInfo;
    SInt                 sResult = 0;
    idBool               sEqualSet = ID_TRUE;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                 sCompareCount = 0;

    sCurStack.mLeftPos  = aLeftPos;
    sCurStack.mRightPos = aRightPos;

    sPivotTRPInfo.mFetchEndOffset  = aHeader->mRowSize;
    sTargetTRPInfo.mFetchEndOffset = aHeader->mRowSize;

    IDE_TEST( aHeader->mSortStack.push( ID_FALSE, /*Lock*/
                                        &sCurStack ) != IDE_SUCCESS );

    sEmpty = ID_FALSE;
    while( 1 )
    {
        IDE_TEST(aHeader->mSortStack.pop(ID_FALSE, &sCurStack, &sEmpty)
                 != IDE_SUCCESS);

        // QuickSort�� �˰����, CallStack�� ItemCount���� ������ �� ����
        // ���� �̺��� ������, ���ѷ����� ������ ���ɼ��� ����.
        IDE_ERROR( aRightPos*2 >= aHeader->mSortStack.getTotItemCnt() );

        if ( sEmpty == ID_TRUE)
        {
            break;
        }

        sLeft  = sCurStack.mLeftPos;
        sRight = sCurStack.mRightPos + 1;
        sMid   = (sCurStack.mLeftPos + sCurStack.mRightPos)/2;

        if ( sCurStack.mLeftPos < sCurStack.mRightPos )
        {
            IDE_TEST( sdtWASortMap::swapvULong( &sWASeg->mSortMapHdr,
                                                sMid,
                                                sLeft )
                      != IDE_SUCCESS );

            IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                               sLeft,
                                               (vULong*)&sPivotTRPInfo.mTRPHeader )
                      != IDE_SUCCESS );

            IDE_TEST( sdtTempRow::fetch( sWASeg,
                                         SDT_WAGROUPID_NONE,
                                         aHeader->mRowBuffer4Compare,
                                         &sPivotTRPInfo )
                      != IDE_SUCCESS );

            sEqualSet = ID_TRUE;

            while( 1 )
            {
                while( ( ++sLeft ) <= sCurStack.mRightPos )
                {
                    IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                                       sLeft,
                                                       (vULong*)&sTargetTRPInfo.mTRPHeader )
                              != IDE_SUCCESS );

                    IDE_TEST( sdtTempRow::fetch(
                                  sWASeg,
                                  SDT_WAGROUPID_NONE,
                                  aHeader->mRowBuffer4CompareSub,
                                  &sTargetTRPInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( compare( aHeader,
                                       &sTargetTRPInfo,
                                       &sPivotTRPInfo,
                                       &sResult )
                              != IDE_SUCCESS );
                    sCompareCount++;
                    if ( sResult != 0 )
                    {
                        sEqualSet = ID_FALSE;
                    }

                    if ( sResult > 0 )
                    {
                        break;
                    }
                }

                while( ( --sRight ) > sCurStack.mLeftPos )
                {
                    IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                                       sRight,
                                                       (vULong*)&sTargetTRPInfo.mTRPHeader )
                              != IDE_SUCCESS );

                    IDE_TEST( sdtTempRow::fetch( sWASeg,
                                                 SDT_WAGROUPID_NONE,
                                                 aHeader->mRowBuffer4CompareSub,
                                                 &sTargetTRPInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( compare( aHeader,
                                       &sTargetTRPInfo,
                                       &sPivotTRPInfo,
                                       &sResult )
                              != IDE_SUCCESS );
                    sCompareCount++;
                    if ( sResult != 0 )
                    {
                        sEqualSet = ID_FALSE;
                    }

                    if ( sResult < 0 )
                    {
                        break;
                    }
                }

                if ( sLeft < sRight )
                {
                    IDE_TEST( sdtWASortMap::swapvULong(
                                  &sWASeg->mSortMapHdr,sLeft,sRight)
                              != IDE_SUCCESS );
                }
                else
                {
                    break;
                }
            }

            /* Pivot�� ������ */
            IDE_TEST( sdtWASortMap::swapvULong( &sWASeg->mSortMapHdr,
                                                sLeft - 1,
                                                sCurStack.mLeftPos )
                      != IDE_SUCCESS );

            if ( sEqualSet == ID_FALSE )
            {
                if ( sLeft > (sCurStack.mLeftPos + 2 ) )
                {
                    sNewStack.mLeftPos = sCurStack.mLeftPos;
                    sNewStack.mRightPos = sLeft - 2;

                    IDE_TEST(aHeader->mSortStack.push( ID_FALSE, &sNewStack)
                             != IDE_SUCCESS);
                }
                if ( sLeft < sCurStack.mRightPos )
                {
                    sNewStack.mLeftPos  = sLeft;
                    sNewStack.mRightPos = sCurStack.mRightPos;

                    IDE_TEST(aHeader->mSortStack.push(ID_FALSE, &sNewStack)
                             != IDE_SUCCESS);
                }
            }
        }
    }

    aHeader->mStatsPtr->mExtraStat1 += sCompareCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * MergeSort�� �����Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::mergeSort( smiTempTableHeader * aHeader,
                                 SInt                 aLeftBeginPos,
                                 SInt                 aLeftEndPos,
                                 SInt                 aRightBeginPos,
                                 SInt                 aRightEndPos )
{
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtWASortMapHdr    * sMapHdr = &sWASeg->mSortMapHdr;
    SInt                 sLeft;
    SInt                 sRight;
    SInt                 sNext;
    vULong               sLeftPtr;
    vULong               sRightPtr;
    sdtSortScanInfo      sLeftTRPInfo;
    sdtSortScanInfo      sRightTRPInfo;
    SInt                 sResult = 0;
    UInt                 sCompareCount = 0;
    SInt                 i;

    /* Left/Right ���� �´���־�� �� */
    IDE_ERROR( aLeftEndPos +1 == aRightBeginPos );

    sLeft   = aLeftBeginPos;
    sRight  = aRightBeginPos;
    sNext   = aLeftBeginPos;  /* Merge�� ����� �� NextVersion�� SlotIdx */

    IDE_TEST( sdtWASortMap::getvULong( sMapHdr,
                                       sLeft,
                                       (vULong*)&sLeftTRPInfo.mTRPHeader )
              != IDE_SUCCESS );
    IDE_TEST( sdtWASortMap::getvULong( sMapHdr,
                                       sRight,
                                       (vULong*)&sRightTRPInfo.mTRPHeader )
              != IDE_SUCCESS );

    sLeftTRPInfo.mFetchEndOffset  = aHeader->mRowSize;
    sRightTRPInfo.mFetchEndOffset = aHeader->mRowSize;

    IDE_TEST( sdtTempRow::fetch( sWASeg,
                                 SDT_WAGROUPID_NONE,
                                 aHeader->mRowBuffer4Compare,
                                 &sLeftTRPInfo )
              != IDE_SUCCESS );

    IDE_TEST( sdtTempRow::fetch( sWASeg,
                                 SDT_WAGROUPID_NONE,
                                 aHeader->mRowBuffer4CompareSub,
                                 &sRightTRPInfo )
              != IDE_SUCCESS );

    while( 1 )
    {
        IDE_TEST( compare( aHeader,
                           &sLeftTRPInfo,
                           &sRightTRPInfo,
                           &sResult )
                  != IDE_SUCCESS );
        sCompareCount++;
        if ( sResult > 0 )
        {
            IDE_TEST( sdtWASortMap::setNextVersion( sMapHdr,
                                                    sNext,
                                                    &sRightTRPInfo.mTRPHeader )
                      != IDE_SUCCESS );
            sNext++;
            sRight++;
            if ( sRight > aRightEndPos )
            {
                break;
            }

            IDE_TEST( sdtWASortMap::getvULong( sMapHdr,
                                               sRight,
                                               (vULong*)&sRightTRPInfo.mTRPHeader )
                      != IDE_SUCCESS );
            IDE_TEST( sdtTempRow::fetch( sWASeg,
                                         SDT_WAGROUPID_NONE,
                                         aHeader->mRowBuffer4CompareSub,
                                         &sRightTRPInfo )
                      != IDE_SUCCESS );

        }
        else
        {
            IDE_TEST( sdtWASortMap::setNextVersion( sMapHdr,
                                                    sNext,
                                                    &sLeftTRPInfo.mTRPHeader  )
                      != IDE_SUCCESS );
            sNext++;
            sLeft++;
            if ( sLeft > aLeftEndPos )
            {
                break;
            }

            IDE_TEST( sdtWASortMap::getvULong( sMapHdr,
                                               sLeft,
                                               (vULong*)&sLeftTRPInfo.mTRPHeader)
                      != IDE_SUCCESS );

            IDE_TEST( sdtTempRow::fetch( sWASeg,
                                         SDT_WAGROUPID_NONE,
                                         aHeader->mRowBuffer4Compare,
                                         &sLeftTRPInfo )
                      != IDE_SUCCESS );
        }
    }

    if ( sRight > aRightEndPos )
    {
        /* ������ Group�� ��� �ٴڳ� ����, ������ ���� �͵��� ���� ū ����
         * �� �ǰ� �ȴ�. ���� �̸� ��ġ�� ��, NextSlotArray�� ���ĵ�
         * ������ �������� �ȴ�.
         *
         * ��) ������ ���� ��츦 �����ϰ� �Ǹ�,
         *  Version0 : 3 4 5 6 7 8 1 2 3 4 5
         *                   ^              ^
         *                sLeft         sRight
         *  Version1 : 1 2 3 3 4 4 5 5 . . .
         *
         * ��, sRight�� EndPos�� �����ϰ�, Left�� ���Ѵ�.
         * ���� sLeftPos���� 6,7,8�� NextVersion�� �������ָ� �ȴ�. */
        i = sNext;
        for( ; sLeft <= aLeftEndPos ; sLeft++, i++ )
        {
            IDE_TEST( sdtWASortMap::getvULong( sMapHdr, sLeft, &sLeftPtr )
                      != IDE_SUCCESS );
            IDE_TEST( sdtWASortMap::setNextVersion( sMapHdr,
                                                    i,
                                                    &sLeftPtr )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_ERROR( sLeft > aLeftEndPos );
        /* ���� Group�� �ٴڳ� ����, �������� ���� �͵��� ū ����̴�. */

        i = sNext;
        for( ; sRight <= aRightEndPos ; sRight++, i++ )
        {
            IDE_TEST( sdtWASortMap::getvULong( sMapHdr, sRight, &sRightPtr )
                      != IDE_SUCCESS );
            IDE_ERROR( i == sRight );
            IDE_TEST( sdtWASortMap::setNextVersion( sMapHdr,
                                                    i,
                                                    &sRightPtr )
                      != IDE_SUCCESS );
        }
    }
    aHeader->mStatsPtr->mExtraStat1 += sCompareCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * row�� ��� compare�Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::compareGRIDAndGRID(smiTempTableHeader * aHeader,
                                         sdtGroupID           aGroupID,
                                         scGRID               aSrcGRID,
                                         scGRID               aDstGRID,
                                         SInt               * aResult )
{
    sdtSortSegHdr     * sWASeg     = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortScanInfo     sSrcTRPInfo;
    sdtSortScanInfo     sDstTRPInfo;

    IDE_TEST( sdtTempRow::fetchByGRID( sWASeg,
                                       aGroupID,
                                       aSrcGRID,
                                       aHeader->mRowSize,
                                       aHeader->mRowBuffer4Compare,
                                       &sSrcTRPInfo )
              != IDE_SUCCESS );

    IDE_TEST( sdtTempRow::fetchByGRID( sWASeg,
                                       aGroupID,
                                       aDstGRID,
                                       aHeader->mRowSize,
                                       aHeader->mRowBuffer4CompareSub,
                                       &sDstTRPInfo )
              != IDE_SUCCESS );

    IDE_TEST( compare( aHeader, &sSrcTRPInfo, &sDstTRPInfo, aResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    smuUtility::dumpGRID,
                                    &aSrcGRID );
    smuUtility::dumpFuncWithBuffer( IDE_DUMP_0,
                                    smuUtility::dumpGRID,
                                    &aDstGRID );

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * row�� ��� compare�Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::compare( smiTempTableHeader * aHeader,
                               sdtSortScanInfo    * aSrcTRPInfo,
                               sdtSortScanInfo    * aDstTRPInfo,
                               SInt               * aResult )
{
    smiTempColumn * sColumn;
    smiValueInfo    sValueInfo1;
    smiValueInfo    sValueInfo2;
    idBool          sIsNull1;
    idBool          sIsNull2;
    UInt            sColumnCount = 0;

    *aResult = 0;

    sValueInfo1.value  = aSrcTRPInfo->mValuePtr;
    sValueInfo1.flag   = SMI_OFFSET_USE;
    sValueInfo2.value  = aDstTRPInfo->mValuePtr;
    sValueInfo2.flag   = SMI_OFFSET_USE;

    sColumn = aHeader->mKeyColumnList;
    while( sColumn != NULL )
    {
        IDE_ERROR( ++sColumnCount <= aHeader->mColumnCount );

        sValueInfo1.column = &sColumn->mColumn;
        sValueInfo2.column = &sColumn->mColumn;

        /* PROJ-2435 order by nulls first/last */
        if ( ( sColumn->mColumn.flag & SMI_COLUMN_NULLS_ORDER_MASK )
             == SMI_COLUMN_NULLS_ORDER_NONE )
        {
            /* NULLS first/last �� ������� �������� ����*/
            *aResult = sColumn->mCompare( &sValueInfo1,
                                          &sValueInfo2 );
        }
        else
        {
            IDE_DASSERT( ( sColumn->mColumn.flag & SMI_COLUMN_TYPE_MASK )
                         == SMI_COLUMN_TYPE_FIXED );
            /* 1. 2���� value�� Null ���θ� �����Ѵ�. */
            sIsNull1 = sColumn->mIsNull( (const smiColumn *)&sColumn->mColumn,
                                         ((UChar*)sValueInfo1.value + sColumn->mColumn.offset) );
            sIsNull2 = sColumn->mIsNull( (const smiColumn *)&sColumn->mColumn,
                                         ((UChar*)sValueInfo2.value + sColumn->mColumn.offset) );

            if ( sIsNull1 == sIsNull2 )
            {
                if ( sIsNull1 == ID_FALSE  )
                {
                    /* 2. NULL�̾������ �������� ����*/
                    *aResult = sColumn->mCompare( &sValueInfo1,
                                                  &sValueInfo2 );
                }
                else
                {
                    /* 3. �Ѵ� NULL �� ��� 0 */
                    *aResult = 0;
                }
            }
            else
            {
                if ( ( sColumn->mColumn.flag & SMI_COLUMN_NULLS_ORDER_MASK )
                     == SMI_COLUMN_NULLS_ORDER_FIRST )
                {
                    /* 4. NULLS FIRST �ϰ�� Null�� �ּҷ� �Ѵ�. */
                    if ( sIsNull1 == ID_TRUE )
                    {
                        *aResult = -1;
                    }
                    else
                    {
                        *aResult = 1;
                    }
                }
                else
                {
                    /* 5. NULLS LAST �ϰ�� Null�� �ִ�� �Ѵ�. */
                    if ( sIsNull1 == ID_TRUE )
                    {
                        *aResult = 1;
                    }
                    else
                    {
                        *aResult = -1;
                    }
                }
            }
        }

        if ( *aResult != 0 )
        {
            break;
        }
        sColumn = sColumn->mNextKeyColumn;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �� �ڽ� Run�� ���Ѵ�.
 * �ڽ��� ������, ID_UINT_MAX�� ��ȯ�Ѵ�.
 *
 * <IN>
 * aHeader        - ��� Table
 * aPos           - �θ� Slot ID
 * <OUT>
 * aChild         - �� ��� ������ Child�� Slot ID
 ***************************************************************************/
IDE_RC sdtSortModule::findAndSetLoserSlot( smiTempTableHeader * aHeader,
                                           UInt                 aPos,
                                           UInt               * aChild )
{
    sdtTempMergeRunInfo sRunInfo;
    sdtTempMergeRunInfo sChildRunInfo[2];
    UInt                 sChildPos[2];
    scGRID               sChildGRID[2];
    UInt                 sSelectedChild = ID_UINT_MAX;
    UInt                 sSelectedChildPos = ID_UINT_MAX;
    SInt                 sResult = 0;
    sdtSortSegHdr      * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                 i;

    IDE_ERROR( ( aHeader->mTTState == SMI_TTSTATE_SORT_MERGE ) ||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_MERGESCAN ) ||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_MAKETREE ) );
    IDE_ERROR( aPos < sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) );

    sChildPos[0] = aPos * 2;
    sChildPos[1] = aPos * 2 + 1;

    if ( (aPos * 2) >= sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) )
    {
        /* ( sChildPos[ 0 ] >=
         *   sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) ) &&
         * ( sChildPos[ 1 ] >=
         *   sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) )  ) */
        /* ������ �ڽ��� ���� */
    }
    else
    {
        /* �ڽ����κ��� �����ϸ� �� */
        for( i = 0 ; i < 2 ; i++ )
        {
            if ( sChildPos[ i ] >=
                 sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) )
            {
                sChildRunInfo[i].mRunNo = SDT_TEMP_RUNINFO_NULL;
            }
            else
            {
                IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                             sChildPos[i],
                                             (void*)&sChildRunInfo[i] )
                          != IDE_SUCCESS );
            }

            if ( sChildRunInfo[i].mRunNo != SDT_TEMP_RUNINFO_NULL )
            {
                SC_MAKE_GRID( sChildGRID[i],
                              SDT_SPACEID_WORKAREA,
                              getWPIDFromRunInfo( aHeader,
                                                  sChildRunInfo[i].mRunNo,
                                                  sChildRunInfo[i].mPIDSeq ),
                              sChildRunInfo[i].mSlotNo );
            }
        }

        if ( ( sChildRunInfo[0].mRunNo != SDT_TEMP_RUNINFO_NULL ) &&
             ( sChildRunInfo[1].mRunNo != SDT_TEMP_RUNINFO_NULL ) )
        {
            /* Both values are not null, ( not null ). */
            /* �׻� InMemory�����̾�� �ϱ⿡, None Group */
            IDE_TEST( compareGRIDAndGRID( aHeader,
                                          SDT_WAGROUPID_NONE,
                                          sChildGRID[0],
                                          sChildGRID[1],
                                          &sResult )
                      != IDE_SUCCESS );

            if ( sResult <= 0 )
            {
                /* ������ �۰ų� ���� ������ */
                sSelectedChild = 0;
            }
            else
            {
                sSelectedChild = 1;
            }
        }
        else
        {
            if ( ( sChildRunInfo[0].mRunNo == SDT_TEMP_RUNINFO_NULL ) &&
                 ( sChildRunInfo[1].mRunNo != SDT_TEMP_RUNINFO_NULL ) )
            {
                /* 1�� ��ȿ, 0�� ��ȿ���� ���� */
                sSelectedChild = 1;
            }
            else
            {
                if ( ( sChildRunInfo[0].mRunNo!=SDT_TEMP_RUNINFO_NULL ) &&
                     ( sChildRunInfo[1].mRunNo==SDT_TEMP_RUNINFO_NULL ) )
                {
                    sSelectedChild = 0;
                }
                else
                {
                    /* ���ʴ� Null�̴� �� �÷��� ��� ���� */
                    IDE_ERROR( ( sChildRunInfo[0].mRunNo ==
                                 SDT_TEMP_RUNINFO_NULL ) &&
                               ( sChildRunInfo[1].mRunNo ==
                                 SDT_TEMP_RUNINFO_NULL ) );
                }
            }
        }
    }

    if ( sSelectedChild != ID_UINT_MAX )
    {
        /* Child�� ���õ� ���� Slot�� ���� */
        IDE_TEST( sdtWASortMap::set( &sWASeg->mSortMapHdr,
                                     aPos,
                                     (void*)&sChildRunInfo[ sSelectedChild ] )
                  != IDE_SUCCESS );
        sSelectedChildPos = sChildPos[ sSelectedChild ];
    }
    else
    {
        /* ������ �ڽ��� ������, �ڽ��� Source Slot�� �Ǿ�, Run�� ����
         * �������� ������ */
        IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                     aPos,
                                     (void*)&sRunInfo )
                  != IDE_SUCCESS );
        IDE_TEST( readNextRowByRun( aHeader,
                                    &sRunInfo )
                  != IDE_SUCCESS );
        IDE_TEST( sdtWASortMap::set( &sWASeg->mSortMapHdr,
                                     aPos,
                                     (void*)&sRunInfo )
                  != IDE_SUCCESS );

    }

    if ( aChild != NULL )
    {
        ( * aChild ) = sSelectedChildPos;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * SortGroup�� ���ĵ� Key(�Ǵ� Row)���� Run�� ���·� �����Ѵ�
 *
 * <IN>
 * aHeader        - ��� Table
 ***************************************************************************/
IDE_RC sdtSortModule::storeSortedRun( smiTempTableHeader * aHeader )
{
    scPageID              sHeadNPID = SD_NULL_PID;
    UChar               * sSrcPtr;
    sdtSortSegHdr       * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortInsertResult sTRInsertResult;
    UInt                  sSlotCount;
    UInt                  i;

    sSlotCount = sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr );

    if ( sSlotCount > 0 )
    {
        /* FlushGroup�� �ʱ�ȭ���� ������, Run���� ���δ�. */
        IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                                SDT_WAGROUPID_FLUSH,
                                                ID_TRUE/*wait4flsuh*/ )
                  != IDE_SUCCESS );

        IDE_ERROR( ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTNSORT ) ||
                   ( aHeader->mTTState == SMI_TTSTATE_SORT_INSERTONLY ) ||
                   ( aHeader->mTTState == SMI_TTSTATE_SORT_EXTRACTNSORT ) ||
                   ( aHeader->mTTState == SMI_TTSTATE_SORT_SCAN ) );

        /* SortGroup�� Row���� FlushGroup���� ���ļ� ��� ������. */
        for( i = 0 ;
             i < sSlotCount ;
             i++ )
        {
            IDE_TEST( sdtWASortMap::getvULong( &sWASeg->mSortMapHdr,
                                               i,
                                               (vULong*)&sSrcPtr )
                      != IDE_SUCCESS );

            IDE_TEST( copyRowByPtr( aHeader,
                                    sSrcPtr,
                                    SDT_COPY_NORMAL,
                                    SDT_TEMP_PAGETYPE_SORTEDRUN,
                                    SC_NULL_GRID, /* ChildRID */
                                    &sTRInsertResult )
                      != IDE_SUCCESS );

            if ( sHeadNPID == SD_NULL_PID )
            {
                sHeadNPID = SC_MAKE_PID( sTRInsertResult.mHeadRowpieceGRID );
            }
            aHeader->mMaxRowPageCount = IDL_MAX( aHeader->mMaxRowPageCount,
                                                 sTRInsertResult.mRowPageCount );
        }

        IDE_ERROR( sHeadNPID != SD_NULL_PID );
        IDE_ERROR( aHeader->mSortGroupID == SDT_WAGROUPID_SORT );

        IDE_TEST( aHeader->mRunQueue.enqueue(ID_FALSE, (void*)&sHeadNPID )
                  != IDE_SUCCESS );

        /* Run���� ����� ���� Flush������, SortGroup�� �ʱ�ȭ�� */
        IDE_TEST( sdtSortSegment::resetWAGroup( sWASeg,
                                                SDT_WAGROUPID_SORT,
                                                ID_FALSE/*wait4flsuh*/ )
                  != IDE_SUCCESS );

        /* WAMap�� �ʱ�ȭ */
        IDE_TEST( sdtWASortMap::create( sWASeg,
                                        SDT_WAGROUPID_SORT,
                                        SDT_WM_TYPE_POINTER,
                                        0, /* Slot Count */
                                        2, /* aVersionCount */
                                        &sWASeg->mSortMapHdr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Row�� �ٸ� Group���� �����Ѵ�.
 * ���� ���� (aPurpose)�� ����, ����/�纻 Group, Rowó������ �޶�����.
 *
 * <IN>
 * aHeader         - ��� Table
 * aSrcGRID        - ������ ���� Row
 * aPurpose        - �����ϴ� ����
 * aPageType       - ������ ��� Page�� Type
 * aChildGRID      - ġȯ�� ChildGRID(���� Row�� �ٸ� �κ��� �ΰ�, ChildGRID��
 *                   ������ )
 * aTRInsertResult - ������ ���.
 ***************************************************************************/
IDE_RC sdtSortModule::copyRowByGRID( smiTempTableHeader * aHeader,
                                     scGRID               aSrcGRID,
                                     sdtCopyPurpose       aPurpose,
                                     sdtTempPageType      aPageType,
                                     scGRID               aChildGRID,
                                     sdtSortInsertResult  * aTRInsertResult )
{
    sdtGroupID     sSrcGroupID  = SDT_WAGROUPID_SORT;
    sdtSortSegHdr* sWASeg       = (sdtSortSegHdr*)aHeader->mWASegment;
    UChar        * sCursor      = NULL;
    sdtWCB       * sWCBPtr;
    idBool         sIsFixedPage = ID_FALSE;

    /* ��� �׷쿡�� ��� �׷������� �̵��ΰ�  */
    switch( aPurpose )
    {
        case SDT_COPY_NORMAL:
        case SDT_COPY_MAKE_KEY:
        case SDT_COPY_MAKE_LNODE:
            sSrcGroupID   = SDT_WAGROUPID_SORT;
            break;
        case SDT_COPY_MAKE_INODE:
            sSrcGroupID   = SDT_WAGROUPID_LNODE;
            break;
        case SDT_COPY_EXTRACT_ROW:
            sSrcGroupID  = SDT_WAGROUPID_SUBFLUSH;
            break;
        default:
            break;
    }

    IDE_TEST( sdtSortSegment::getPagePtrByGRID( sWASeg,
                                                sSrcGroupID,
                                                aSrcGRID,
                                                &sCursor )
              != IDE_SUCCESS );

    if ( sSrcGroupID != SDT_WAGROUPID_SORT )
    {
        IDE_DASSERT( ( SC_MAKE_SPACE( aSrcGRID ) != SDT_SPACEID_WAMAP ) &&
                     ( SC_MAKE_SPACE( aSrcGRID ) != SDT_SPACEID_WORKAREA ) );

        sWCBPtr = sdtSortSegment::findWCB( sWASeg, SC_MAKE_PID( aSrcGRID ) );

        IDE_DASSERT( sWCBPtr != NULL ); // ��� get page�� ���� NULL�ϸ� ����.

        sdtSortSegment::fixWAPage( sWCBPtr );
        sIsFixedPage = ID_TRUE;
    }

    IDE_TEST( copyRowByPtr( aHeader,
                            sCursor,
                            aPurpose,
                            aPageType,
                            aChildGRID,
                            aTRInsertResult )
              != IDE_SUCCESS );

    if ( sIsFixedPage == ID_TRUE )
    {
        sIsFixedPage = ID_FALSE;
        sdtSortSegment::unfixWAPage( sWCBPtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsFixedPage == ID_TRUE )
    {
        sdtSortSegment::unfixWAPage( sWCBPtr );
    }
    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Row�� �ٸ� Group���� �����Ѵ�.
 * ���� ���� (aPurpose)�� ����, ����/�纻 Group, Rowó������ �޶�����.
 *
 * <IN>
 * aHeader         - ��� Table
 * aSrcPtr         - ������ ���� Row
 * aPurpose        - �����ϴ� ����
 * aPageType       - ������ ��� Page�� Type
 * aChildGRID      - ġȯ�� ChildGRID(���� Row�� �ٸ� �κ��� �ΰ�, ChildGRID��
 *                   ������ )
 * aTRInsertResult - ������ ���.
 ***************************************************************************/
IDE_RC sdtSortModule::copyRowByPtr(smiTempTableHeader * aHeader,
                                   UChar              * aSrcPtr,
                                   sdtCopyPurpose      aPurpose,
                                   sdtTempPageType      aPageType,
                                   scGRID               aChildGRID,
                                   sdtSortInsertResult  * aTRInsertResult )
{
    sdtSortScanInfo     sScanInfo4Select;
    sdtSortInsertInfo   sScanInfo4Insert;
    smiValue            sValueList[ SMI_COLUMN_ID_MAXIMUM ];
    sdtSortTRPHdr     * sTRPHeader;
    sdtGroupID          sSrcGroupID  = SDT_WAGROUPID_SORT;
    sdtGroupID          sDestGroupID = SDT_WAGROUPID_FLUSH;
    sdtSortSegHdr     * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                i;

    /* ��� �׷쿡�� ��� �׷������� �̵��ΰ�  */
    switch( aPurpose )
    {
        case SDT_COPY_NORMAL:
            sSrcGroupID   = SDT_WAGROUPID_SORT;
            sDestGroupID  = SDT_WAGROUPID_FLUSH;
            break;
        case SDT_COPY_MAKE_LNODE:
            sSrcGroupID   = SDT_WAGROUPID_SORT;
            sDestGroupID  = SDT_WAGROUPID_FLUSH;
            break;
        case SDT_COPY_MAKE_INODE:
            sSrcGroupID   = SDT_WAGROUPID_LNODE;
            sDestGroupID  = SDT_WAGROUPID_INODE;
            break;
        case SDT_COPY_EXTRACT_ROW:
            sSrcGroupID  = SDT_WAGROUPID_SUBFLUSH;
            sDestGroupID = SDT_WAGROUPID_SORT;
            break;
        default:
            break;
    }

    sScanInfo4Select.mTRPHeader      = (sdtSortTRPHdr*)aSrcPtr;
    sScanInfo4Select.mFetchEndOffset = aHeader->mRowSize;

    /*************** SortGroup���� �ش� Row�� ������ ********************/
    IDE_TEST( sdtTempRow::fetch( sWASeg,
                                 sSrcGroupID,
                                 aHeader->mRowBuffer4Fetch,
                                 &sScanInfo4Select )
              != IDE_SUCCESS );

    /**************** TRPInfo4select�� TRPInfo4Insert���� ****************/

    /* TRPHeader�� ���� �� Flag�� �����Ѵ�. */
    idlOS::memcpy( &sScanInfo4Insert.mTRPHeader,
                   sScanInfo4Select.mTRPHeader,
                   ID_SIZEOF( sdtSortTRPHdr) );
    sTRPHeader = &sScanInfo4Insert.mTRPHeader;
    /* �ϳ��� ���ļ� �б� ������, Next�� ������� �Ѵ�. */
    SM_SET_FLAG_OFF( sTRPHeader->mTRFlag, SDT_TRFLAG_NEXTGRID );
    SC_MAKE_NULL_GRID( sTRPHeader->mNextGRID );
    sScanInfo4Insert.mColumnCount  = aHeader->mColumnCount;
    sScanInfo4Insert.mColumns      = aHeader->mColumns;
    sScanInfo4Insert.mValueLength  = aHeader->mRowSize;
    sScanInfo4Insert.mValueList    = sValueList;
    for( i = 0; i < aHeader->mColumnCount; i++ )
    {
        sScanInfo4Insert.mValueList[ i ].length =
            aHeader->mColumns[ i ].mColumn.size;
        sScanInfo4Insert.mValueList[ i ].value  =
            sScanInfo4Select.mValuePtr +
            aHeader->mColumns[ i ].mColumn.offset;
    }
    if ( SC_GRID_IS_NOT_NULL( aChildGRID ) )
    {
        SC_COPY_GRID( aChildGRID, sTRPHeader->mChildGRID ) ;
        SM_SET_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_CHILDGRID );
    }

    /*************************** ��ó�� ���� ****************************/
    switch( aPurpose )
    {
        case SDT_COPY_NORMAL:
        case SDT_COPY_EXTRACT_ROW:
            /* LeafKey���� �����ϴ� ���̱⿡, Unsplit�� �ִ�.
             * �̸� ����� �˾Ƽ� �ɰ��� �����Ѵ�. */
            SM_SET_FLAG_OFF( sTRPHeader->mTRFlag, SDT_TRFLAG_UNSPLIT );
            break;
        case SDT_COPY_MAKE_KEY:
            /* Key�̱� ������ ChildGRID�� �־�� �Ѵ�. */
            IDE_ERROR( SC_GRID_IS_NOT_NULL( aChildGRID ) );
            break;
        case SDT_COPY_MAKE_LNODE:
            /* BUG-48226 appendRowPiece()���� header size ������ ���� slot�� �Ҵ�Ǵ� ������ �߻�
             * header size�� �����ϰ� value size�� property ���� ũ�⺸�� Ŀ���Ѵ�. */
            if ( sScanInfo4Insert.mValueLength > smuProperty::getTempMaxKeySize() )
            {
                /* Ű�� �ʹ� ũ�� ExtraPage�� ������ �ְ� �������� ���� */
                IDE_TEST( copyExtraRow( aHeader, &sScanInfo4Insert )
                          != IDE_SUCCESS );
            }
            SM_SET_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_UNSPLIT );
            IDE_ERROR( SC_GRID_IS_NULL( aChildGRID ) );
            break;
        case SDT_COPY_MAKE_INODE:
            /* INode�� �����Ҷ�, LNode�� ���� ExtraRow�� ������ �ȴ�.
             * ���� NextGRID�� ��������, ValueLength�� LNode�� FirstRowPiece��ŭ
             * ��ҽ�Ų��. */
            if ( SM_IS_FLAG_ON( sScanInfo4Select.mTRPHeader->mTRFlag,
                                SDT_TRFLAG_NEXTGRID ) )
            {
                SC_COPY_GRID( sScanInfo4Select.mTRPHeader->mNextGRID, sTRPHeader->mNextGRID );
                sScanInfo4Insert.mValueLength =
                    sScanInfo4Select.mTRPHeader->mValueLength;
                SM_SET_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_NEXTGRID );
            }
            SM_SET_FLAG_ON( sTRPHeader->mTRFlag, SDT_TRFLAG_UNSPLIT );
            break;
        default:
            IDE_ERROR( 0 );
            break;
    }

    /******************************* ���� ********************************/
    IDE_TEST( sdtTempRow::append( sWASeg,
                                  sDestGroupID,
                                  aPageType,
                                  0, /* CuttingOffset */
                                  &sScanInfo4Insert,
                                  aTRInsertResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Row�� Key�� ����� ����, Row�� ���� �κ��� ExtraGroup���� �����Ѵ�.
 *
 * <IN>
 * aHeader         - ��� Table
 * aTRInsertResult - ������ ���.
 ***************************************************************************/
IDE_RC sdtSortModule::copyExtraRow( smiTempTableHeader * aHeader,
                                    sdtSortInsertInfo  * aTRPInfo )
{
    sdtSortInsertResult  sTRInsertResult;
    IDE_TEST( sdtTempRow::append( (sdtSortSegHdr*)aHeader->mWASegment,
                                  SDT_WAGROUPID_SUBFLUSH,
                                  SDT_TEMP_PAGETYPE_INDEX_EXTRA,
                                  smuProperty::getTempMaxKeySize(),
                                  aTRPInfo,
                                  &sTRInsertResult )
              != IDE_SUCCESS );
    IDE_ERROR( sTRInsertResult.mComplete == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * run���� �о� Heap�� �����Ѵ�.
 * ( mergeScan���� restoreCursor ������ ����Ѵ�.)
 *
 * - ����
 * +-+-+-+-+-+-+-+---+-------+-------+-------+-------+
 * |1|1|2|1|2|3|4|   |   4   |   3   |   2   |   1   |
 * +-+-+-+-+-+-+-+---+-------+-------+-------+-------+
 * <----Slot----->   <-------------Run--------------->
 *
 * Heap�� ���Ͱ��� �����ȴ�.
 *
 * - Slot ( sdtTempMergeRunInfo )
 *   Slot�� Run�鰣 ��Ұ��踦 ������ �ִ� Array�� WaMap���� ǥ���ȴ�. ��
 *   �������� Slot���� �������� ������ ���� ���踦 ������.
 *      1
 *    1   2
 *   1 2 3 4
 *
 * - Run
 *   Run�� mMaxRowPageCount��ŭ�� Page������ �����ȴ�.
 ***************************************************************************/
IDE_RC sdtSortModule::heapInit(smiTempTableHeader * aHeader)
{
    UInt                   sLeftPos;
    UInt                   sRunNo;
    scPageID               sNextNPID;
    scPageID               sPageSeq;
    idBool                 sIsEmpty;
    UInt                   sCurLimitRunNo;
    sdtTempMergeRunInfo   sRunInfo;
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                   i;

    aHeader->mMergeRunCount = calcMaxMergeRunCount(
        aHeader, aHeader->mMaxRowPageCount );
    IDE_TEST_RAISE( aHeader->mMergeRunCount <= 1, error_invalid_sortareasize );

    /* Queue���̺��� ���� Merge�� �ʿ�� ���� */
    aHeader->mMergeRunCount = IDL_MIN( aHeader->mMergeRunCount,
                                       aHeader->mRunQueue.getQueueLength() );

    /* HeapMap�� ������.  */
    IDE_TEST( sdtWASortMap::create( sWASeg,
                                    SDT_WAGROUPID_SORT,
                                    SDT_WM_TYPE_RUNINFO,
                                    aHeader->mMergeRunCount*3 + 2 , /*count*/
                                    1, /* aVersionCount */
                                    &sWASeg->mSortMapHdr )
              != IDE_SUCCESS );

    /* Slot���� �ϴ� NULL RunNo�� �ʱ�ȭ��Ŵ */
    for( i = 0 ; i < aHeader->mMergeRunCount*3 + 2; i++ )
    {
        sRunInfo.mRunNo  = SDT_TEMP_RUNINFO_NULL;
        sRunInfo.mPIDSeq = 0;
        sRunInfo.mSlotNo = 0;

        IDE_TEST( sdtWASortMap::set( &sWASeg->mSortMapHdr,
                                     i,
                                     (void*)&sRunInfo )
                  != IDE_SUCCESS );
    }

    if ( aHeader->mMergeRunCount == 0 )
    {
        aHeader->mMergeRunSize = 1;
    }
    else
    {
        /* Run �ϳ��� ũ�⸦ ����� */
        aHeader->mMergeRunSize =
            sdtSortSegment::getAllocableWAGroupPageCount( sWASeg,
                                                          SDT_WAGROUPID_SORT )
            / aHeader->mMergeRunCount;

        /* ���� ������ Run��  ���� �������� MapSlot�� ������ �ȵ� */
        IDE_ERROR( getWPIDFromRunInfo( aHeader,
                                       aHeader->mMergeRunCount - 1,
                                       aHeader->mMergeRunSize - 1 )
                   > sdtWASortMap::getEndWPID( &sWASeg->mSortMapHdr ) -1 );

        /* LeftPos�� ������ ������ �� Line���� ���� ���� Slot�� ��ġ�̴�.
         * ��)        1
         *      2           3
         *   4     5     6     7
         *  8  9 10 11 12 13 14 15
         * ���⼭ LetPos�� �� �� �ִ� ���� 1,2,4,8�̴�.
         * �׸��� �� ���� ���ÿ� �ش� Line�� ũ���̴�. ������ ����° 4����
         * 4,5,6,7 �װ��� Slot�� �ְ�, �׹�° 8���� 8���� Slot�� �ִ�.
         * ���� sLeftPos�� MergeRunCount�� �Ѵ� ������ �ٴ��̴�.*/
        for( sLeftPos = 1 ;
             sLeftPos < aHeader->mMergeRunCount;
             sLeftPos *= 2 )
        {
            /* nothing to do!!! */
        }
        aHeader->mLeftBottomPos = sLeftPos;

        /* ���� �عٴڿ� ���� ������ ���� */
        sCurLimitRunNo = IDL_MIN( sLeftPos, aHeader->mMergeRunCount );
        for( sRunNo = 0 ; sRunNo < sCurLimitRunNo ; sRunNo++ )
        {
            IDE_TEST( aHeader->mRunQueue.dequeue( ID_FALSE, /* a_bLock */
                                                  (void*)&sNextNPID,
                                                  &sIsEmpty )
                      != IDE_SUCCESS );
            IDE_ERROR( sIsEmpty == ID_FALSE );

            /* Run�� ������ Page�� Loading */
            sPageSeq = aHeader->mMergeRunSize;
            while( sPageSeq-- )
            {
                IDE_TEST( readRunPage( aHeader,
                                       sRunNo,
                                       sPageSeq,
                                       &sNextNPID,
                                       ID_FALSE ) /*aReadNextNPID*/
                          != IDE_SUCCESS );

                if ( sNextNPID == SD_NULL_PID )
                {
                    /* �ش� run �� ���� Row�� ���� */
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }

            /* Bottom�� Run���� ���� */
            sRunInfo.mRunNo  = sRunNo;
            sRunInfo.mPIDSeq = aHeader->mMergeRunSize - 1;
            sRunInfo.mSlotNo = 0;
            IDE_TEST( sdtWASortMap::set( &sWASeg->mSortMapHdr,
                                         aHeader->mLeftBottomPos + sRunNo,
                                         (void*)&sRunInfo )
                      != IDE_SUCCESS );
        }

        IDE_TEST( buildLooserTree( aHeader ) != IDE_SUCCESS );
    }

    /* Merge�Ѵܰ� �Ҷ����� IOPass�� ������ */
    aHeader->mStatsPtr->mIOPassNo++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_invalid_sortareasize);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_SORTAREASIZE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Run�� Child���� �о� ���ϸ�, LooserTree�� �����Ѵ�
 ***************************************************************************/
IDE_RC sdtSortModule::buildLooserTree(smiTempTableHeader * aHeader)
{
    UInt                   sLeftPos;
    UInt                   sRunNo;

    IDE_ERROR( aHeader->mMergeRunSize > 0 );

    sLeftPos = aHeader->mLeftBottomPos / 2;

    /* �Ʒ��������� �ö���鼭 ��������. */
    while( sLeftPos > 0 )
    {
        for( sRunNo = 0 ;
             sRunNo < sLeftPos ;
             sRunNo++ )
        {
            IDE_TEST( findAndSetLoserSlot( aHeader,
                                           sLeftPos + sRunNo,
                                           NULL /*ChildIdx*/ )
                      != IDE_SUCCESS );
        }
        sLeftPos /= 2;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * heap���� �ֻ���� Row, �� LooserRow�� �����Ѵ�.
 ***************************************************************************/
IDE_RC sdtSortModule::heapPop(smiTempTableHeader * aHeader )
{
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtTempMergeRunInfo   sTopRunInfo;
    UInt                   sPos;

    /* Heap�� Top Slot�� �̾Ƴ� */
    IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                 1,  /* aIdx */
                                 (void*)&sTopRunInfo )
              != IDE_SUCCESS );

    IDE_ERROR( sTopRunInfo.mRunNo != SDT_TEMP_RUNINFO_NULL );

    /* �Ʒ��������� Top���� ���� ���� Row�� ã�� �ø���. */
    sPos = aHeader->mLeftBottomPos + sTopRunInfo.mRunNo;
    do
    {
        IDE_TEST( findAndSetLoserSlot( aHeader,
                                       sPos,
                                       NULL )
                  != IDE_SUCCESS );
        sPos /= 2;
    }
    while( sPos > 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Run���� ���� Row�� �д´�.
 ***************************************************************************/
IDE_RC sdtSortModule::readNextRowByRun( smiTempTableHeader   * aHeader,
                                        sdtTempMergeRunInfo * aRun )
{
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtSortTRPHdr        * sTRPHeader;
    scGRID                 sGRID;
    scPageID               sNPID;
    scPageID               sWPID;
    scPageID               sReadPIDCount = 0;
    sdtTempMergeRunInfo   sCurRunInfo;
    UChar                * sPtr = NULL;
    idBool                 sIsValidSlot = ID_FALSE;
    UChar                * sWAPagePtr = NULL;
    UInt                   sSlotCount;
    /* Head
     * +---+    +---+
     * | A |    | C |
     * +---+    +---+
     * | B | -> |   |
     * +---+    +   |
     * | C`|    |   |
     * +---+    +---+
     * (`�� Chain�� NextPage���� �ǹ�. �� C -> C` �̷��� �ǰ� C�� First)
     *
     * A���� Ž���� �����ϸ�, NextSlot�� ���Ҷ� ã�ƾ��� ���� Row�� ����
     * ���� ã�ư��� �ȴ�. ���� FirstRow�� ã�� ���� Ž���� �����ϸ�
     * �ȴ�. */
    sCurRunInfo = *aRun;

    /* FirstRowPiece�� ã��������, �� ���� Row�� ���������� �д´�. */
    while ( 1 )
    {
        sCurRunInfo.mSlotNo++;
        if ( sCurRunInfo.mRunNo == SDT_TEMP_RUNINFO_NULL )
        {
            break;
            /* �� �̾Ƴ� */
        }

        getGRIDFromRunInfo( aHeader, &sCurRunInfo, &sGRID );

        sWAPagePtr = NULL;
        sWPID      = SC_NULL_PID;

        IDE_TEST( sdtSortSegment::getPage( (sdtSortSegHdr*)
                                           aHeader->mWASegment,
                                           SDT_WAGROUPID_NONE,
                                           sGRID,
                                           &sWPID,
                                           &sWAPagePtr )
                  != IDE_SUCCESS );

        sSlotCount = sdtTempPage::getSlotCount(sWAPagePtr);

        sdtSortSegment::getSlot( sWAPagePtr,
                                 sSlotCount,
                                 SC_MAKE_OFFSET( sGRID ),
                                 &sPtr,
                                 &sIsValidSlot );

        if ( sIsValidSlot == ID_FALSE )
        {
            /* Page�� Slot�� ���� ��ȸ��. ���� �������� ������. */
            sNPID = sdtTempPage::getNextPID( sPtr );

            /* ���߿� Free�϶�� ǥ���ص�. �ٷ� ǥ�� �ߴٰ���,
             * �� �׸����� Row C�� ������ C`�� ���� �������� ���� �� ����*/
            sWPID = getWPIDFromRunInfo( aHeader,
                                        sCurRunInfo.mRunNo,
                                        sCurRunInfo.mPIDSeq );
            sdtSortSegment::bookedFree( sWASeg, sWPID );

            if ( sNPID == SD_NULL_PID )
            {
                /*���� Row�� �߰����� ���ϴ� ��Ȳ����, ����� */
                aRun->mRunNo = SDT_TEMP_RUNINFO_NULL;
                break;
            }
            else
            {
                /* ���� Loop�� ���°� �ƴ��� �˻��Ѵ�.
                 * UShortMax�� ũ���, 65536*8192 = 512MB�� ������ ũ����
                 * Row�� ����. */
                sReadPIDCount++;
                IDE_ERROR( sReadPIDCount < ID_USHORT_MAX );

                /* ���� �������� */
                sCurRunInfo.mPIDSeq++;
                sCurRunInfo.mSlotNo = -1;

                IDE_TEST( readRunPage( aHeader,
                                       sCurRunInfo.mRunNo,
                                       sCurRunInfo.mPIDSeq,
                                       &sNPID,
                                       ID_TRUE ) /*aReadNextNPID*/
                          != IDE_SUCCESS );
                continue;
            }
        }
        else
        {
            sTRPHeader = (sdtSortTRPHdr*)sPtr;
            if ( SM_IS_FLAG_ON( sTRPHeader->mTRFlag , SDT_TRFLAG_HEAD ) )
            {
                *aRun = sCurRunInfo;
                break;
            }
        } /*if */
    } /* while */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Run���� ���� Row�� �б� ����, Run�� ���� Page�� ��ȿ��
 * ��ҿ� �ø���.
 *
 * <IN>
 * aHeader        - ��� Table
 * aRunNo         - ��� Run�� ��ȣ
 * aPageSeq       - Run������ ���° Page�ΰ�.
 * <IN/OUT>
 * aNextPID       - ���� ���� PID. �׸��� �ٽ� ���� PID�� ��ȯ��
 * aReadNextNPID  - NextPID�� �о�� �ϳ�? (False�� PrevPID�� ���� )
 ***************************************************************************/
IDE_RC sdtSortModule::readRunPage( smiTempTableHeader   * aHeader,
                                   UInt                   aRunNo,
                                   UInt                   aPageSeq,
                                   scPageID             * aNextPID,
                                   idBool                 aReadNextNPID )
{
    UChar        * sWAPagePtr;
    scPageID       sRunNPID;
    scPageID       sWPID;
    sdtSortSegHdr* sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    sdtWCB       * sWCBPtr;
    sdtWCB       * sOldWCB;

    sRunNPID = *aNextPID;
    if ( sRunNPID != SD_NULL_PID )
    {
        sWPID = getWPIDFromRunInfo( aHeader, aRunNo, aPageSeq );

        /* �̹� �ش� �������� Loading �ž� �ִ� ���� �����Ѵ�.
         * ������ ���� Page�� Row�� Chaining�ž� ������, �̹� ���� ��������
         * �о��� �־��� ���� �ִ�. */
        sWCBPtr = sdtSortSegment::getWCBWithLnk( sWASeg, sWPID );
        sOldWCB = sdtSortSegment::findWCB( sWASeg, sRunNPID );

        if ( sWCBPtr != sOldWCB )
        {
            // sOldWCB�� null�̰ų�,
            // sWCBPtr->mNPage != sRunNPID �̰ų�
            if ( sdtSortSegment::getNPID( sWCBPtr ) != SC_NULL_PID )
            {
                IDE_TEST( sdtSortSegment::makeInitPage( sWASeg,
                                                        sWCBPtr,
                                                        ID_TRUE ) /*Flush */
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( sdtSortSegment::chkAndSetWAPagePtr( sWASeg,
                                                              sWCBPtr ) != IDE_SUCCESS );
            }

            IDE_TEST( sdtSortSegment::readNPage( sWASeg,
                                                 SDT_WAGROUPID_SORT,
                                                 sWCBPtr,
                                                 sRunNPID )
                      != IDE_SUCCESS );
        }

        sWAPagePtr = sdtSortSegment::getWAPagePtr( sWCBPtr );

        // BUG-46383 Disk Sort Temp���� moveWAPage�� skip�Ǵ� ��찡 �ֽ��ϴ�.
        // �� readNPage���� NPID�� WPID �� �ų��� ���ϴ� ��찡 ����.
        // ����� �о� �Դ��� Ȯ���ؾ� ��.
        IDE_ERROR( sdtTempPage::getSelfPID( sWAPagePtr ) == sRunNPID );

        /* ������ ���� Page�� ������. �ٸ� Page���� �־�,
         * NextPID ��ũ�� ������, PrevPID ��ũ�� ��������
         * ���� Boolean�� ���� �����ȴ�. */
        if ( aReadNextNPID == ID_TRUE )
        {
            *aNextPID = sdtTempPage::getNextPID( sWAPagePtr );
        }
        else
        {
            *aNextPID = sdtTempPage::getPrevPID( sWAPagePtr );
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * MergeCursor�� �����.
 *
 * +------+------+------+------+------+
 * | Size |0 PID |0 Slot|1 PID |1 Slot|
 * +------+------+------+------+------+
 * MergeRun�� ���Ͱ��� ����� Array��, MergeRunCount * 2 + 1 ���� �����ȴ�.
 ***************************************************************************/
IDE_RC sdtSortModule::makeMergePosition( smiTempTableHeader  * aHeader,
                                         void               ** aMergePos )
{
    sdtTempMergeRunInfo    sRunInfo;
    sdtTempMergePos      * sMergePos = (sdtTempMergePos*)*aMergePos;
    scPageID                sWPID;
    sdtSortSegHdr         * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                    i;
    UInt                    sState = 0;
    ULong                   sSize  = 0;

    IDE_ERROR( ( aHeader->mTTState == SMI_TTSTATE_SORT_MERGE ) ||
               ( aHeader->mTTState == SMI_TTSTATE_SORT_MERGESCAN ) );

    if ( sMergePos == NULL )
    {
        sSize  = ID_SIZEOF( sdtTempMergePos ) * SDT_TEMP_MERGEPOS_SIZE(aHeader->mMergeRunCount );

        /* TC/FIT/Server/sm/Project/PROJ-2458/Memory/sdt/
         * sdtSortModule_makeMergePosition_malloc_MergePos.tc */
        IDU_FIT_POINT_RAISE("sdtSortModule::makeMergePosition::malloc::MergePos",
                            insufficient_memory);
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_TEMP,
                                           sSize,
                                           (void**)&sMergePos )
                        != IDE_SUCCESS,
                        insufficient_memory );
        sState = 1;
    }
    else
    {
        /* nothing to do */
    }

    for( i = 0 ; i < aHeader->mMergeRunCount ; i++ )
    {
        IDE_TEST( sdtWASortMap::get( &sWASeg->mSortMapHdr,
                                     aHeader->mLeftBottomPos + i,
                                     (void*)&sRunInfo )
                  != IDE_SUCCESS );

        sWPID = getWPIDFromRunInfo( aHeader,
                                    sRunInfo.mRunNo,
                                    sRunInfo.mPIDSeq );
        if ( sWPID != SC_NULL_PID )
        {
            sMergePos[ SDT_TEMP_MERGEPOS_PIDIDX(i) ] =
                sdtSortSegment::getNPID( sWASeg, sWPID );
            sMergePos[ SDT_TEMP_MERGEPOS_SLOTIDX(i) ] =
                sRunInfo.mSlotNo;
        }
        else
        {
            /* Run�� ������� */
            sMergePos[ SDT_TEMP_MERGEPOS_PIDIDX(i) ] = SC_NULL_PID;
            sMergePos[ SDT_TEMP_MERGEPOS_SLOTIDX(i) ] = SC_NULL_PID;
        }
    }

    sMergePos[ SDT_TEMP_MERGEPOS_SIZEIDX ] = i;
    *aMergePos = (void*)sMergePos;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            if ( sMergePos != NULL )
            {
                IDE_ASSERT( iduMemMgr::free( sMergePos ) == IDE_SUCCESS );
                sMergePos = NULL;
            }
        default:
            break;
    }

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * MergeCursor�� �����.
 *
 * MergePosition���� MergeRun���� �����.
 ***************************************************************************/
IDE_RC sdtSortModule::makeMergeRuns( smiTempTableHeader  * aHeader,
                                     void                * aMergePosition )
{
    sdtTempMergeRunInfo   sRunInfo;
    sdtTempMergePos     * sMergePos;
    scPageID               sWPID;
    sdtSortSegHdr        * sWASeg = (sdtSortSegHdr*)aHeader->mWASegment;
    scPageID               sPageSeq;
    scPageID               sNPID;
    UInt                   sSlotIdx;
    UInt                   i;
    UInt                   j;

    sMergePos = (sdtTempMergePos*)aMergePosition;

    IDE_ERROR( sMergePos[ SDT_TEMP_MERGEPOS_SIZEIDX ]
               == aHeader->mMergeRunCount );
    IDE_ERROR( aHeader->mMergeRunSize > 0 );

    /* Run���� ��ŭ ��ȸ */
    for( i = 0 ; i < sMergePos[ SDT_TEMP_MERGEPOS_SIZEIDX ] ; i++ )
    {
        sNPID    = sMergePos[ SDT_TEMP_MERGEPOS_PIDIDX( i ) ];
        sSlotIdx = sMergePos[ SDT_TEMP_MERGEPOS_SLOTIDX(i) ];

        if ( sNPID == SC_NULL_PID )
        {
            sRunInfo.mRunNo  = SDT_TEMP_RUNINFO_NULL;
            sRunInfo.mPIDSeq = 0;
            sRunInfo.mSlotNo = 0;
        }
        else
        {
            /* �̹� Run���� �о���� �������� Load�Ǿ� �ִ��� Ȯ���Ѵ� */
            for( j = 0 ; j < aHeader->mMergeRunSize ; j++ )
            {
                sWPID = getWPIDFromRunInfo( aHeader, i, j );
                if ( sNPID == sdtSortSegment::getNPID( sWASeg, sWPID ) )
                {
                    break;
                }
            }

            /* ��ã����. �׷��� 0������ ���ʷ� �ҷ����� ��. */
            if ( j == aHeader->mMergeRunSize )
            {
                j = 0;
            }

            /* Run���� ���� */
            sRunInfo.mRunNo  = i;
            sRunInfo.mPIDSeq = j;
            sRunInfo.mSlotNo = sSlotIdx;

            /* �� �������� 1024�� �̻��� Slot�� �Ұ���. 8byte align��
             * ���ߴ�, 8192/8 =1024�ϱ� */
            IDE_DASSERT( sSlotIdx <= 1024 );

            /* Load�� ����������, MaxRowPageCount*2��ŭ Load���ָ� �ȴ�. */
            sPageSeq = aHeader->mMaxRowPageCount*2;
            while( sPageSeq-- )
            {
                IDE_TEST( readRunPage( aHeader,
                                       i,
                                       j,
                                       &sNPID,
                                       ID_FALSE ) /*aReadNextNPID*/
                          != IDE_SUCCESS );
                if ( j == 0 )
                {
                    j = aHeader->mMergeRunSize;
                }
                j--;

                if ( sNPID == SD_NULL_PID )
                {
                    /* �ش� run �� ���� Row�� ���� */
                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
        }

        IDE_TEST( sdtWASortMap::set( &sWASeg->mSortMapHdr,
                                     aHeader->mLeftBottomPos + i,
                                     (void*)&sRunInfo )
                  != IDE_SUCCESS );
    }

    /* Run�� ������������ LoserTree�� �����Ѵ�. */
    IDE_TEST( buildLooserTree( aHeader ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 *
 ***************************************************************************/
IDE_RC sdtSortModule::makeScanPosition( smiTempTableHeader  * aHeader,
                                        scPageID           ** aScanPosition )
{
    sdtSortSegHdr      * sWASeg  = (sdtSortSegHdr*)aHeader->mWASegment;
    UInt                 sRunCnt = aHeader->mRunQueue.getQueueLength();
    sdtTempScanPos    * sScanPos;
    UInt                 i;
    UInt                 sState  = 0;
    scPageID             sNPID   = SC_NULL_PID ;
    idBool               sIsEmpty;

    IDE_ERROR( aHeader->mTTState == SMI_TTSTATE_SORT_SCAN );

    /* run�� ���µ� �������� �������� ����. */
    IDE_ERROR( aHeader->mRunQueue.getQueueLength() != 0 );

    /* ������ Run�� Slot ���� */
    aHeader->mStatsPtr->mExtraStat2 =
        sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr );

    /* ������ Run�� �����ؼ� ���� */
    if ( sdtWASortMap::getSlotCount( &sWASeg->mSortMapHdr ) > 0 )
    {
        IDE_TEST( storeSortedRun( aHeader ) != IDE_SUCCESS );
        sRunCnt++;
    }

    /* ������ */
    IDE_DASSERT ( sRunCnt == aHeader->mRunQueue.getQueueLength() );

    /* Scan Group���� ����� */
    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_FLUSH,
                                           ID_FALSE ) /*wait4flush */
              != IDE_SUCCESS );

    IDE_TEST( sdtSortSegment::dropWAGroup( sWASeg,
                                           SDT_WAGROUPID_SORT,
                                           ID_FALSE ) /*wait4flush */
              != IDE_SUCCESS );

    /* Scan Group���� ����� */
    IDE_TEST( sdtSortSegment::createWAGroup( sWASeg,
                                             SDT_WAGROUPID_SCAN,
                                             0, /* aPageCount */
                                             SDT_WA_REUSE_FIFO )
              != IDE_SUCCESS );

    /* Map�� ���̻� ��� ���� */
    sdtWASortMap::disable( sWASeg );

    /* sdtSortModule_makeScanPosition_malloc_ScanPos.tc */
    IDU_FIT_POINT("sdtSortModule::makeScanPosition::malloc::ScanPos");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_TEMP,
                                 ID_SIZEOF( sdtTempScanPos ) *
                                 SDT_TEMP_SCANPOS_SIZE( sRunCnt ),
                                 (void**)&sScanPos )
              != IDE_SUCCESS );
    sState = 1;

    for( i = 0 ; i < sRunCnt ; i++ )
    {
        /* Queue ���� �������� �ϳ��� ����*/
        IDE_TEST( aHeader->mRunQueue.dequeue( ID_FALSE, /* a_bLock */
                                              (void*)&sNPID,
                                              &sIsEmpty ) );
        if ( sIsEmpty == ID_FALSE )
        {
            IDE_ERROR( sNPID != SC_NULL_PID );
            sScanPos[ SDT_TEMP_SCANPOS_PIDIDX(i) ] = sNPID;
        }
        else
        {
            /* Run�� ������� */
            sScanPos[ SDT_TEMP_SCANPOS_PIDIDX(i) ] = SC_NULL_PID;
        }
    }

    sScanPos[ SDT_TEMP_SCANPOS_PINIDX ] = 0;
    sScanPos[ SDT_TEMP_SCANPOS_SIZEIDX ] = i;
    *aScanPosition = sScanPos;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sScanPos ) == IDE_SUCCESS );
            sScanPos = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}
