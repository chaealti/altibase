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
 *
 * $Id: sdpstSegDDL.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Create/Drop/Alter/Reset ������
 * STATIC �������̽����� �����Ѵ�.
 *
 ***********************************************************************/

# include <smr.h>
# include <sdr.h>
# include <sdbBufferMgr.h>
# include <sdpReq.h>

# include <sdpstDef.h>

# include <sdpstSH.h>
# include <sdptbExtent.h>
# include <sdpstAllocExtInfo.h>

/***********************************************************************
 * Description : sdpstBfrAllocExtInfo ����ü�� �ʱ�ȭ�Ѵ�.
 *
 * aBfrInfo     - [IN] Extent �Ҵ� �˰��򿡼� ����ϴ� ���� �Ҵ� ����
 ***********************************************************************/
IDE_RC sdpstAllocExtInfo::initialize( sdpstBfrAllocExtInfo *aBfrInfo )
{
    UInt    sLoop;

    idlOS::memset( aBfrInfo, 0x00, ID_SIZEOF(sdpstBfrAllocExtInfo));

    for ( sLoop = SDPST_EXTDIR; sLoop < SDPST_BMP_TYPE_MAX; sLoop++ )
    {
        aBfrInfo->mLstPID[sLoop]      = SD_NULL_PID;
        aBfrInfo->mFreeSlotCnt[sLoop] = SD_NULL_PID;
        aBfrInfo->mMaxSlotCnt[sLoop]  = SD_NULL_PID;
    }

    aBfrInfo->mTotPageCnt     = 0;
    aBfrInfo->mPageRange      = SDPST_PAGE_RANGE_NULL;
    aBfrInfo->mSlotNoInExtDir = -1;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : sdpstAftAllocExtInfo ����ü�� �ʱ�ȭ�Ѵ�.
 *
 * aAftInfo     - [IN] Extent �Ҵ� �˰��򿡼� ����ϴ� ���� �Ҵ� ����
 ***********************************************************************/
IDE_RC sdpstAllocExtInfo::initialize( sdpstAftAllocExtInfo *aAftInfo )
{
    UInt    sLoop;

    idlOS::memset( aAftInfo, 0x00, ID_SIZEOF(sdpstAftAllocExtInfo));

    for ( sLoop = SDPST_EXTDIR; sLoop < SDPST_BMP_TYPE_MAX; sLoop++ )
    {
        aAftInfo->mFstPID[sLoop]      = SD_NULL_PID;
        aAftInfo->mLstPID[sLoop]      = SD_NULL_PID;
        aAftInfo->mPageCnt[sLoop]     = SD_NULL_PID;
        aAftInfo->mFullBMPCnt[sLoop]  = SD_NULL_PID;
    }

    aAftInfo->mTotPageCnt     = 0;
    aAftInfo->mLfBMP4ExtDir   = SD_NULL_PID;
    aAftInfo->mPageRange      = SDPST_PAGE_RANGE_NULL;
    aAftInfo->mSegHdrCnt      = 0;  /* 0, 1���� ���´�. */
    aAftInfo->mSlotNoInExtDir = -1;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Segment Header�� last bmp ���������� ������ �����Ѵ�.
 *
 * Segment�� Extent �Ҵ��� Ʈ����ǿ� ���� ���ÿ�
 * ������� �ʴ´�. ���� Ʈ������� ���͵� ��
 * Ʈ������� �����ϰ� �������� �Ҵ翬���� �Ϸ�Ǳ⸦
 * ����Ѵ�.
 * �׷��Ƿ�, Extent �Ҵ� ������������� �ٸ� Ʈ����ǿ� ����
 * Segment Header�� last bmp ������ ����
 * �� last bmp �������� free slot ������ ������� �ʱ� ������
 * �̸� ����صΰ� ����Ѵ�.
 *
 * ���� ���, Extent ũ�� : 512K �϶�
 *
 * A. Segment ���� ũ�� 512K �� ����
 *
 * - Extent 1�� �Ҵ� -> Segment 512K�̹Ƿ� PageRange 16( <1M ) ����
 *
 * - lf-BMPs ���ϱ�
 *   PageRange 16���� 512 Extent�� ǥ���Ϸ���,
 *   512KB�� �������� ǥ���ϸ�, 64 ������ : pages = 512/page_size
 *   lf-BMPs = 64 pages / PageRange_pages = 4 lf-BMPs,
 *
 * - it-BMPs ���ϱ�
 *   ������ lf-bmps �� ��� ����� ��ŭ�� lf-bmp map �� �ִ°�?
 *   lf-BMP map���� �� lf-BMP slot�� �ִ���?
 *   ������, ������ lf-BMPs���� ���� lf-BMP��
 *   ����� �� �ִ� ������ ���� : lf-BMPs'
 *   it-BMPs = mod( lf-BMPs', lf-BMPs )
 *   ������ ���� �������� it-BMPs += 1
 *
 * - rt-BMPs ���ϱ�
 *
 *   ������ it-bmps �� ��� ����� ��ŭ�� it-bmp map �� �ִ°�?
 *   it-BMP map���� �� it-BMP slot�� �ִ���?
 *   ������, ������ it-BMPs���� ���� it-BMP��
 *   ����� �� �ִ� ������ ���� : it-BMPs'
 *   rt-BMPs = mod( it-BMPs', it-BMPs )
 *   ������ ���� �������� rt-BMPs += 1
 *
 *
 * aStatistics   - [IN] �������
 * aSpaceID      - [IN] SpaceID
 * aSegPID       - [IN] Segment Header PID
 * aMaxExtCnt    - [IN] Segment���� ��밡���� �ִ� Extent ��
 * aExtDesc      - [IN] �Ҵ�� Extent�� Desc
 * aBfrInfo      - [OUT] Extent �Ҵ� �˰��򿡼� ����ϴ� ���� �Ҵ� ����
 ***********************************************************************/
IDE_RC sdpstAllocExtInfo::getBfrAllocExtInfo( idvSQL               *aStatistics,
                                              scSpaceID             aSpaceID,
                                              scPageID              aSegPID,
                                              UInt                  aMaxExtCnt,
                                              sdpstBfrAllocExtInfo *aBfrInfo )
{
    UChar              * sSegHdrPagePtr;
    UChar              * sPagePtr;
    sdpstSegHdr        * sSegHdr;
    sdpstExtDirHdr     * sExtDirHdr;
    sdpstLfBMPHdr      * sLfBMPHdr;
    sdpstBMPHdr        * sBMPHdr;
    sdpstRangeSlot     * sRangeSlot;
    UInt                 sLoop;

    IDE_DASSERT( aBfrInfo != NULL );

    sSegHdrPagePtr = NULL;
    sPagePtr       = NULL;

    /* 0. Segment Header�� ���� �������� ���� ��� */
    if ( aSegPID == SD_NULL_PID )
    {
        aBfrInfo->mMaxSlotCnt[SDPST_RTBMP]   =
            SDPST_MAX_SLOT_CNT_PER_RTBMP_IN_SEGHDR();
        aBfrInfo->mMaxSlotCnt[SDPST_ITBMP]   = SDPST_MAX_SLOT_CNT_PER_ITBMP();
        aBfrInfo->mMaxSlotCnt[SDPST_LFBMP]   = SDPST_MAX_SLOT_CNT_PER_LFBMP();
        aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR]  =
            SDPST_MAX_SLOT_CNT_PER_EXTDIR_IN_SEGHDR();

        aBfrInfo->mFreeSlotCnt[SDPST_RTBMP]  =
            SDPST_MAX_SLOT_CNT_PER_RTBMP_IN_SEGHDR();
        aBfrInfo->mFreeSlotCnt[SDPST_ITBMP]  = 0;
        aBfrInfo->mFreeSlotCnt[SDPST_LFBMP]  = 0;
        aBfrInfo->mFreeSlotCnt[SDPST_EXTDIR] =
            SDPST_MAX_SLOT_CNT_PER_EXTDIR_IN_SEGHDR();

        aBfrInfo->mNxtSeqNo                  = 0;

        IDE_CONT( no_segment_header );
    }

    /* 1. Segment Header�� �����ϴ� ���
       Segment Header�κ��� last lf-bmp �������� ���Ͽ� fix �Ѵ�. */
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          &sSegHdrPagePtr ) != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sSegHdrPagePtr );

    IDE_TEST_RAISE( (sSegHdr->mTotExtCnt + 1) > aMaxExtCnt,
                    error_exceed_segment_maxextents );

    /* ���ο� ������ bitmap �������� �̸� ���� ���������� �����س���,
     * ���� ����� ��쿡 ���ؼ� �����Ѵ�. */
    aBfrInfo->mLstPID[SDPST_LFBMP]  = sSegHdr->mLstLfBMP;
    aBfrInfo->mLstPID[SDPST_ITBMP]  = sSegHdr->mLstItBMP;
    aBfrInfo->mLstPID[SDPST_RTBMP]  = sSegHdr->mLstRtBMP;
    aBfrInfo->mLstPID[SDPST_EXTDIR] = sdpstSH::getLstExtDir( sSegHdr );

    if ( (aBfrInfo->mLstPID[SDPST_LFBMP]  == SD_NULL_PID) ||
         (aBfrInfo->mLstPID[SDPST_ITBMP]  == SD_NULL_PID) ||
         (aBfrInfo->mLstPID[SDPST_RTBMP]  == SD_NULL_PID) ||
         (aBfrInfo->mLstPID[SDPST_EXTDIR] == SD_NULL_PID) )
    {
        sdpstSH::dump( sSegHdrPagePtr );
        IDE_ASSERT( 0 );
    }

    aBfrInfo->mTotPageCnt  = sSegHdr->mTotPageCnt;
    aBfrInfo->mNxtSeqNo    = sSegHdr->mLstSeqNo + 1;

    /* BMP ���������� fix �ؼ� ������ �����´�. */
    for ( sLoop = SDPST_RTBMP; sLoop < SDPST_BMP_TYPE_MAX; sLoop++ )
    {
        IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                              aSpaceID,
                                              aBfrInfo->mLstPID[sLoop],
                                              &sPagePtr ) != IDE_SUCCESS );

        if ( sLoop == SDPST_LFBMP )
        {
            sLfBMPHdr = sdpstLfBMP::getHdrPtr( sPagePtr );
            sBMPHdr   = &sLfBMPHdr->mBMPHdr;
        }
        else
        {
            sBMPHdr   = sdpstBMP::getHdrPtr( sPagePtr );
        }

        aBfrInfo->mFreeSlotCnt[sLoop] = sBMPHdr->mFreeSlotCnt;
        aBfrInfo->mMaxSlotCnt[sLoop]  = sBMPHdr->mMaxSlotCnt;

        if ( sLoop == SDPST_LFBMP )
        {
            aBfrInfo->mPageRange        = sLfBMPHdr->mPageRange;
            aBfrInfo->mFreePageRangeCnt =
                sdpstLfBMP::getFreePageRangeCnt( sLfBMPHdr );

            sRangeSlot = sdpstLfBMP::getRangeSlotBySlotNo(
                                sdpstLfBMP::getMapPtr(sLfBMPHdr),
                                sLfBMPHdr->mBMPHdr.mSlotCnt - 1 );
            aBfrInfo->mLstPBSNo = sRangeSlot->mFstPBSNo +
                                  sRangeSlot->mLength - 1;
        }

        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );
        sPagePtr = NULL;
    }

    /* 2. ������ ExtDir �������� free slot ������ ���Ѵ�. */
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aBfrInfo->mLstPID[SDPST_EXTDIR],
                                          &sPagePtr ) != IDE_SUCCESS );
    sExtDirHdr = sdpstExtDir::getHdrPtr( sPagePtr );

    aBfrInfo->mFreeSlotCnt[SDPST_EXTDIR] = sdpstExtDir::getFreeSlotCnt(sExtDirHdr);
    aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR]  = SDPST_MAX_SLOT_CNT_PER_EXTDIR();
    aBfrInfo->mSlotNoInExtDir            = sExtDirHdr->mExtCnt;

    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
              != IDE_SUCCESS );
    sPagePtr = NULL;

    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sSegHdrPagePtr )
              != IDE_SUCCESS );
    sSegHdrPagePtr = NULL;

    IDE_EXCEPTION_CONT( no_segment_header );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_exceed_segment_maxextents );
    {
         IDE_SET( ideSetErrorCode( smERR_ABORT_SegmentExceedMaxExtents,
                  aSpaceID,
                  SD_MAKE_FID(aSegPID),
                  SD_MAKE_FPID(aSegPID),
                  sSegHdr->mTotExtCnt,
                  aMaxExtCnt) );
    }
    IDE_EXCEPTION_END;

    if ( sPagePtr != NULL )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sPagePtr ) == IDE_SUCCESS );
    }
    if ( sSegHdrPagePtr != NULL )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sSegHdrPagePtr ) == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Extent �Ҵ�� ���� ������ Bitmap ��������
 *               ������ ����Ѵ�.
 *
 * ���ο� Extent�� Segment ũ�⸦ �������� ���� �����ؾ��� Bitmap
 * ���������� �����Ѵ�
 *
 * leaf bmp �������� ������ �� ����ؾ��� ������ ������ ����.
 *
 * 1. ���� lf-bmp �������� PageRange �� ���� ���õ� PageRange�� �ٸ� ���
 *
 *  1-1. ���� lf-bmp �������� ���� PageRange�� �������� �ʾҾ
 *       ���ο� Extent�� ���ο� lf-bmp�� �Ҵ��Ͽ� �����Ѵ�.
 *
 * 2. ���� lf-bmp�������� PageRange�� ���� ���õ� PageRange�� ������ ���
 *
 *  2-1. ���� lf-bmp �������� ���� PageRange�� ��� ������� ���� ���
 *       ���ο� Extent�� ��� ������ ������ ���� lf-bmp ��������
 *       ������ ���� PageRangeblocks �������� �۰ų� ���� ����
 *       ���� lf-bmp �������� �״�� ����Ѵ�.
 *
 *  2-2  ���� lf-bmp �������� ���� PageRange�� ��� ������� ���� ���
 *       ���ο� Extent�� ��� ������ ������ ���� lf-bmp ��������
 *       ������ ���� PageRangeblocks �������� ū ����
 *       ���ο� lf-bmp �������� �����Ѵ�.
 *
 * aAllocExtInfo - [IN]  Extent �Ҵ�� �ʿ��� ������ Pointer
 * aExtDesc      - [IN]  Extent Slot�� Pointer
 * aBfrInfo      - [OUT] Extent �Ҵ� �˰��򿡼� ����ϴ� ���� �Ҵ� ����
 * aAftInfo      - [OUT] Extent �Ҵ� �˰��򿡼� ����ϴ� ���� �Ҵ� ����
 ***********************************************************************/
void sdpstAllocExtInfo::getAftAllocExtInfo( scPageID                 aSegPID,
                                            sdpstExtDesc           * aExtDesc,
                                            sdpstBfrAllocExtInfo   * aBfrInfo,
                                            sdpstAftAllocExtInfo   * aAftInfo )
{
    scPageID    sNewSegPID;

    IDE_ASSERT( aExtDesc != NULL );
    IDE_ASSERT( aBfrInfo != NULL );
    IDE_ASSERT( aAftInfo != NULL );

    aAftInfo->mLstPID[SDPST_RTBMP]  = aBfrInfo->mLstPID[SDPST_RTBMP];
    aAftInfo->mLstPID[SDPST_ITBMP]  = aBfrInfo->mLstPID[SDPST_ITBMP];
    aAftInfo->mLstPID[SDPST_LFBMP]  = aBfrInfo->mLstPID[SDPST_LFBMP];
    aAftInfo->mLstPID[SDPST_EXTDIR] = aBfrInfo->mLstPID[SDPST_EXTDIR];

    aAftInfo->mSlotNoInExtDir       = aBfrInfo->mSlotNoInExtDir;

    if ( aBfrInfo->mFreeSlotCnt[SDPST_EXTDIR] == 0 )
    {
        /* ExtDesc�� ExtDir �������� �����ؼ� ����ؾ� �ϴ� ��� */
        aAftInfo->mPageCnt[SDPST_EXTDIR] = 1;
        aAftInfo->mLstPID[SDPST_EXTDIR]  = aExtDesc->mExtFstPID;
        aAftInfo->mSlotNoInExtDir        = 0;
    }

    /* ���� SegHdr�� �� ������ ������ �Ҵ��� Extent ������ ���Ѵ�. */
    aAftInfo->mTotPageCnt = aBfrInfo->mTotPageCnt + aExtDesc->mLength;

    /* ���� Page Range�� �����Ѵ�. */
    aAftInfo->mPageRange = selectPageRange( aAftInfo->mTotPageCnt );

    if ( (aBfrInfo->mPageRange == aAftInfo->mPageRange) &&
         (aBfrInfo->mFreeSlotCnt[SDPST_LFBMP] > 0) &&
         (aBfrInfo->mFreePageRangeCnt >= aExtDesc->mLength) )
    {
        /* ���� 2-1 ���׿� �ش��Ѵ�. ���� lf-bmp�� ����ϸ� �ǹǷ�
         * ���� ������ bmp ���������� ����. */
        aAftInfo->mPageCnt[SDPST_LFBMP] = 0;
        aAftInfo->mPageCnt[SDPST_ITBMP] = 0;
        aAftInfo->mPageCnt[SDPST_RTBMP] = 0;

        if ( aAftInfo->mPageCnt[SDPST_EXTDIR] == 1 )
        {
            /* ���� �����Ǵ� ExtDir �������� ���� LfBMP ���� �����ȴ�. */
            aAftInfo->mLfBMP4ExtDir = aAftInfo->mLstPID[SDPST_LFBMP];
        }
    }
    else
    {
        /* 1-1, 2-2�� �ش��ϸ�, ���ο� lf-bmp�� �����Ͽ��� �Ѵ�.
         * �׷��Ƿ� �ʿ��ϴٸ� ���� bmp �������鵵 �����Ͽ��� �Ѵ�. */
        aAftInfo->mPageCnt[SDPST_LFBMP] =
            SDPST_EST_BMP_CNT_4NEWEXT( aExtDesc->mLength,
                                       aAftInfo->mPageRange );

        aAftInfo->mFstPID[SDPST_LFBMP] = aExtDesc->mExtFstPID +
                                         aAftInfo->mPageCnt[SDPST_EXTDIR];
        aAftInfo->mLstPID[SDPST_LFBMP] = aAftInfo->mFstPID[SDPST_LFBMP] +
                                         aAftInfo->mPageCnt[SDPST_LFBMP] - 1;

        if ( aAftInfo->mPageCnt[SDPST_LFBMP] >
                aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] )
        {
            aAftInfo->mPageCnt[SDPST_ITBMP] =
                SDPST_EST_BMP_CNT_4NEWEXT( aAftInfo->mPageCnt[SDPST_LFBMP ] -
                                           aBfrInfo->mFreeSlotCnt[SDPST_ITBMP],
                                           aBfrInfo->mMaxSlotCnt[SDPST_ITBMP] );

            aAftInfo->mFstPID[SDPST_ITBMP] = aExtDesc->mExtFstPID +
                                             aAftInfo->mPageCnt[SDPST_EXTDIR] +
                                             aAftInfo->mPageCnt[SDPST_LFBMP];
            aAftInfo->mLstPID[SDPST_ITBMP] = aAftInfo->mFstPID[SDPST_ITBMP] +
                                             aAftInfo->mPageCnt[SDPST_ITBMP] - 1;
        }
        else
        {
            aAftInfo->mPageCnt[SDPST_ITBMP] = 0;
        }

        if ( aAftInfo->mPageCnt[SDPST_ITBMP] >
                aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] )
        {
            // rt-bmps ������ ���Ѵ�.
            // SegHdr �������� ����� itslots������ ����Ͽ� ���� ������
            // rt-bmp �������� ����Ѵ�.
            aAftInfo->mPageCnt[SDPST_RTBMP] =
                SDPST_EST_BMP_CNT_4NEWEXT( aAftInfo->mPageCnt[SDPST_ITBMP] -
                                           aBfrInfo->mFreeSlotCnt[SDPST_RTBMP],
                                           aBfrInfo->mMaxSlotCnt[SDPST_RTBMP] );
            aAftInfo->mFstPID[SDPST_RTBMP] = aExtDesc->mExtFstPID +
                                             aAftInfo->mPageCnt[SDPST_EXTDIR] +
                                             aAftInfo->mPageCnt[SDPST_LFBMP] +
                                             aAftInfo->mPageCnt[SDPST_ITBMP];
            aAftInfo->mLstPID[SDPST_RTBMP] = aAftInfo->mFstPID[SDPST_RTBMP] +
                                             aAftInfo->mPageCnt[SDPST_RTBMP] - 1;
        }
        else
        {
            aAftInfo->mPageCnt[SDPST_RTBMP] = 0;
        }
    }

    if ( aSegPID == SD_NULL_PID )
    {
        /* Segment Header�� ���� �������� ���� ���¿����� Extent��
         * ExtDir�� PID�� ExtDesc slotNo�� ������ �����ؾ��Ѵ�. */
        sNewSegPID = aExtDesc->mExtFstPID +
                     (aAftInfo->mPageCnt[SDPST_EXTDIR] +
                      aAftInfo->mPageCnt[SDPST_LFBMP] +
                      aAftInfo->mPageCnt[SDPST_ITBMP] +
                      aAftInfo->mPageCnt[SDPST_RTBMP]);

        aBfrInfo->mLstPID[SDPST_EXTDIR] = sNewSegPID;
        aBfrInfo->mLstPID[SDPST_RTBMP]  = sNewSegPID;

        /* mSegHdrCnt�� 0 �Ǵ� 1 ���� ����, �̹� SegHdr�� �����ϴ� ��� �׻� 0
         * �̾�� �Ѵ�. */
        aAftInfo->mSegHdrCnt            = 1;
        aAftInfo->mLstPID[SDPST_EXTDIR] = sNewSegPID;
        aAftInfo->mLstPID[SDPST_RTBMP]  = sNewSegPID;
        aAftInfo->mSlotNoInExtDir       = 0;
    }
}

 /***********************************************************************
 * Description : Segment�� Extent �Ҵ�� �ʿ��� ������ �����Ѵ�.
 ************************************************************************/
IDE_RC sdpstAllocExtInfo::getAllocExtInfo( idvSQL                * aStatistics,
                                           scSpaceID               aSpaceID,
                                           scPageID                aSegPID,
                                           UInt                    aMaxExtCnt,
                                           sdpstExtDesc          * aExtDesc,
                                           sdpstBfrAllocExtInfo  * aBfrInfo,
                                           sdpstAftAllocExtInfo  * aAftInfo )
{
    IDE_TEST( getBfrAllocExtInfo( aStatistics,
                                  aSpaceID,
                                  aSegPID,
                                  aMaxExtCnt,
                                  aBfrInfo )
              != IDE_SUCCESS );

    getAftAllocExtInfo( aSegPID, aExtDesc, aBfrInfo, aAftInfo );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment �� ũ�⸦ ����Ͽ� Leaf Bitmap ��������
 *               Page Range�� �����Ѵ�
 ************************************************************************/
sdpstPageRange sdpstAllocExtInfo::selectPageRange( ULong aTotPages )
{
    sdpstPageRange sPageRange;

    if ( aTotPages < SDPST_PAGE_CNT_1M )
    {
        /* x < 1M */
        sPageRange = SDPST_PAGE_RANGE_16;
    }
    else
    {
        if ( aTotPages >= SDPST_PAGE_CNT_64M )
        {
            if ( aTotPages >= SDPST_PAGE_CNT_1024M )
            {
                /* x >= 1G */
                sPageRange = SDPST_PAGE_RANGE_1024;
            }
            else
            {
                /* 64M <= x < 1G */
                sPageRange = SDPST_PAGE_RANGE_256;
            }
        }
        else
        {   /* 1M <= x < 64M */
            sPageRange = SDPST_PAGE_RANGE_64;
        }
    }

    return sPageRange;
}

void sdpstAllocExtInfo::dump( sdpstBfrAllocExtInfo  * aBfrInfo )
{
    IDE_ASSERT( aBfrInfo != NULL );

    ideLog::log( IDE_SERVER_0,
                 "========================================================\n" );

    ideLog::logMem( IDE_SERVER_0,
                    (UChar*)aBfrInfo,
                    ID_SIZEOF(sdpstBfrAllocExtInfo) );

    ideLog::log( IDE_SERVER_0,
                 "--------------- Before AllocExtInfo Begin ----------------\n"
                 "Last PID [EXTDIR]         :%u\n"
                 "Last PID [RTBMP]          :%u\n"
                 "Last PID [ITBMP]          :%u\n"
                 "Last PID [LFBMP]          :%u\n"
                 "Free Slot Count [EXTDIR]  :%u\n"
                 "Free Slot Count [RTBMP]   :%u\n"
                 "Free Slot Count [ITBMP]   :%u\n"
                 "Free Slot Count [LFBMP]   :%u\n"
                 "Max Slot Count [EXTDIR]   :%u\n"
                 "Max Slot Count [RTBMP]    :%u\n"
                 "Max Slot Count [ITBMP]    :%u\n"
                 "Max Slot Count [LFBMP]    :%u\n"
                 "Total Page Count          :%llu\n"
                 "PageRange                 :%u\n"
                 "Free PageRange Count      :%u\n"
                 "Slot No In ExtDir         :%d\n"
                 "Last PBS No               :%d\n"
                 "Next SeqNo                :%llu\n"
                 "---------------  Before AllocExtInfo End  ----------------\n",
                 aBfrInfo->mLstPID[SDPST_EXTDIR],
                 aBfrInfo->mLstPID[SDPST_RTBMP],
                 aBfrInfo->mLstPID[SDPST_ITBMP],
                 aBfrInfo->mLstPID[SDPST_LFBMP],

                 aBfrInfo->mFreeSlotCnt[SDPST_EXTDIR],
                 aBfrInfo->mFreeSlotCnt[SDPST_RTBMP],
                 aBfrInfo->mFreeSlotCnt[SDPST_ITBMP],
                 aBfrInfo->mFreeSlotCnt[SDPST_LFBMP],

                 aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR],
                 aBfrInfo->mMaxSlotCnt[SDPST_RTBMP],
                 aBfrInfo->mMaxSlotCnt[SDPST_ITBMP],
                 aBfrInfo->mMaxSlotCnt[SDPST_LFBMP],

                 aBfrInfo->mTotPageCnt,

                 aBfrInfo->mPageRange,
                 aBfrInfo->mFreePageRangeCnt,

                 aBfrInfo->mSlotNoInExtDir,
                 aBfrInfo->mLstPBSNo,
                 aBfrInfo->mNxtSeqNo );

    ideLog::log( IDE_SERVER_0,
                 "========================================================\n" );
}

void sdpstAllocExtInfo::dump( sdpstAftAllocExtInfo  * aAftInfo )
{
    IDE_ASSERT( aAftInfo != NULL );

    ideLog::log( IDE_SERVER_0,
                 "========================================================\n" );

    ideLog::logMem( IDE_SERVER_0,
                    (UChar*)aAftInfo,
                    ID_SIZEOF(sdpstAftAllocExtInfo) );

    ideLog::log( IDE_SERVER_0,
                 "--------------- After AllocExtInfo Begin ----------------\n"
                 "First PID [EXTDIR]        :%u\n"
                 "First PID [RTBMP]         :%u\n"
                 "First PID [ITBMP]         :%u\n"
                 "First PID [LFBMP]         :%u\n"
                 "Last PID [EXTDIR]         :%u\n"
                 "Last PID [RTBMP]          :%u\n"
                 "Last PID [ITBMP]          :%u\n"
                 "Last PID [LFBMP]          :%u\n"
                 "Page Count [EXTDIR]       :%u\n"
                 "Page Count [RTBMP]        :%u\n"
                 "Page Count [ITBMP]        :%u\n"
                 "Page Count [LFBMP]        :%u\n"
                 "Full BMP Count [EXTDIR]   :%u\n"
                 "Full BMP Count [RTBMP]    :%u\n"
                 "Full BMP Count [ITBMP]    :%u\n"
                 "Full BMP Count [LFBMP]    :%u\n"
                 "Total Page Count          :%llu\n"
                 "LfBMP for ExtDir          :%u\n"
                 "PageRange                 :%u\n"
                 "Segment Header (0 or 1)   :%u\n"
                 "Slot No In ExtDir         :%d\n"
                 "---------------  After AllocExtInfo End  ----------------\n",
                 aAftInfo->mFstPID[SDPST_EXTDIR],
                 aAftInfo->mFstPID[SDPST_RTBMP],
                 aAftInfo->mFstPID[SDPST_ITBMP],
                 aAftInfo->mFstPID[SDPST_LFBMP],

                 aAftInfo->mLstPID[SDPST_EXTDIR],
                 aAftInfo->mLstPID[SDPST_RTBMP],
                 aAftInfo->mLstPID[SDPST_ITBMP],
                 aAftInfo->mLstPID[SDPST_LFBMP],

                 aAftInfo->mPageCnt[SDPST_EXTDIR],
                 aAftInfo->mPageCnt[SDPST_RTBMP],
                 aAftInfo->mPageCnt[SDPST_ITBMP],
                 aAftInfo->mPageCnt[SDPST_LFBMP],

                 aAftInfo->mFullBMPCnt[SDPST_EXTDIR],
                 aAftInfo->mFullBMPCnt[SDPST_RTBMP],
                 aAftInfo->mFullBMPCnt[SDPST_ITBMP],
                 aAftInfo->mFullBMPCnt[SDPST_LFBMP],

                 aAftInfo->mTotPageCnt,
                 aAftInfo->mLfBMP4ExtDir,
                 aAftInfo->mSegHdrCnt,
                 aAftInfo->mSlotNoInExtDir );

    ideLog::log( IDE_SERVER_0,
                 "========================================================\n" );
}
