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
 * $Id:$
 ***********************************************************************/
#include <smErrorCode.h>
#include <sdr.h>
#include <sdp.h>
#include <sdpPhyPage.h>
#include <sdpSglRIDList.h>

#include <sdpsfDef.h>
#include <sdpsfSH.h>
#include <sdpReq.h>
#include <sdpsfExtent.h>
#include <sdpsfExtDirPageList.h>
#include <sdptbExtent.h>
#include <sdpsfExtMgr.h>

/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdpsfExtMgr::initialize()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdpsfExtMgr::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Segment�� Create����� �ʱ���·� �����.
 *
 * Caution: aSegHdr�� XLatch�� �ɷ��� �´�.
 *
 * aStatistics      - [IN] ��� ����
 * aStartInfo       - [IN] Start Info
 * aSpaceID         - [IN] SpaceID
 * aSegHdr          - [IN] Segment Desc
 *
 * aNewExtRID       - [OUT] �Ҵ�� Extent�� RID
 * aFstPIDOfExt     - [OUT] Extent�� ù��° PID
 * aFstDataPIDOfExt - [OUT] �Ҵ�� Extent�� First Data Page ID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::allocExt( idvSQL          * aStatistics,
                              sdrMtxStartInfo * aStartInfo,
                              scSpaceID         aSpaceID,
                              sdpsfSegHdr     * aSegHdr )
{
    sdrMtx                sMtx;
    UInt                  sState = 0;
    idBool                sIsSegXLatched;
    sdRID                 sNewExtRID;
    UChar               * sSegPagePtr;
    idBool                sIsSuccess;
    smLSN                 sNTA;
    scPageID              sFstPIDOfExt;
    sdpExtDesc            sExtDesc;
    sdpsfExtDesc        * sAllocExtDesc;
    sdpsfExtDirCntlHdr  * sExtDirCntlHdr;

    IDE_ASSERT( aSpaceID         != 0 );
    IDE_ASSERT( aStartInfo       != NULL );
    IDE_ASSERT( aSegHdr          != NULL );

    sIsSegXLatched = ID_TRUE;

    if( aStartInfo->mTrans != NULL )
    {
        sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );
    }
    else
    {
        /* no logging */
    }

    sSegPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    /* SegHdr�� ���ؼ� XLatch�� Ǭ��. ���⼭ Latch�� Ǯ�⶧���� SegHdr�� ���ؼ�
     * ������ Mini Transaction�߿��� Begin���ΰ��� ������ �ȵ˴ϴ�. */
    sIsSegXLatched = ID_FALSE;

    sdbBufferMgr::unlatchPage( sSegPagePtr );

    IDE_TEST( sdptbExtent::allocExts( aStatistics,
                                      aStartInfo,
                                      aSpaceID,
                                      1, /* alloc extent count */
                                      &sExtDesc ) != IDE_SUCCESS );

    sFstPIDOfExt = sExtDesc.mExtFstPID;


    sdbBufferMgr::latchPage( aStatistics,
                             sSegPagePtr,
                             SDB_X_LATCH,
                             SDB_WAIT_NORMAL,
                             &sIsSuccess  );
    sIsSegXLatched = ID_TRUE;

    /* �Ҵ��� Extent�� Segment�� ���δ�. */
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    /* SegHdr�� ���� �����ϱ� ������ Dirty�� ����Ѵ�. */
    IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx,
                                          sSegPagePtr )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfExtDirPageList::addExtDesc( aStatistics,
                                               &sMtx,
                                               aSpaceID,
                                               aSegHdr,
                                               sFstPIDOfExt,
                                               &sExtDirCntlHdr,
                                               &sNewExtRID,
                                               &sAllocExtDesc )
              != IDE_SUCCESS );

    IDE_ASSERT( sAllocExtDesc != NULL );

    if ( aStartInfo->mTrans != NULL )
    {
        sdrMiniTrans::setNullNTA( &sMtx,
                                  aSpaceID,
                                  &sNTA );
    }
    else
    {
        /* no logging */
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    if( sIsSegXLatched == ID_FALSE )
    {
        sdbBufferMgr::latchPage( aStatistics,
                                 sSegPagePtr,
                                 SDB_X_LATCH,
                                 SDB_WAIT_NORMAL,
                                 &sIsSuccess  );

        /* BUGBUG: �����ϴ� ��찡 �ִ°�? */
        IDE_ASSERT( sIsSuccess == ID_TRUE );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Create����� �ʱ���·� �����.
 *
 * Caution: aSegHdr�� XLatch�� �ɷ��� �´�.
 *
 * aStatistics      - [IN] ��� ����
 * aStartInfo       - [IN] Start Info
 * aSpaceID         - [IN] SpaceID
 * aSegHdr          - [IN] Segment Header
 * aNxtExtCnt       - [IN] SegmentȮ��� �þ�� Extent�� ����
 *
 * aNewExtRID       - [OUT] �Ҵ�� Extent�� RID
 * aFstPIDOfExt     - [OUT] Extent�� ù���� PID
 * aFstDataPIDOfExt - [OUT] �Ҵ�� Extent�� First Data Page ID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::extend( idvSQL          * aStatistics,
                            sdrMtxStartInfo * aStartInfo,
                            scSpaceID         aSpaceID,
                            sdpsfSegHdr     * aSegHdr,
                            sdpSegHandle    * aSegHandle,
                            UInt              aNxtExtCnt )
{
    UInt      sLoop;
    UInt      sMaxExtCnt;
    UInt      sCurExtCnt;

    IDE_ASSERT( aStartInfo       != NULL );
    IDE_ASSERT( aSpaceID         != 0 );
    IDE_ASSERT( aSegHdr          != NULL );
    IDE_ASSERT( aNxtExtCnt       != 0 );

    sMaxExtCnt = aSegHandle->mSegStoAttr.mMaxExtCnt;
    sCurExtCnt = sdpsfExtMgr::getExtCnt( aSegHdr );

    IDE_TEST_RAISE( sCurExtCnt + 1 > sMaxExtCnt,
            error_exceed_segment_maxextents );

    for( sLoop = 0; sLoop < aNxtExtCnt; sLoop++ )
    {
        IDE_TEST( allocExt( aStatistics,
                            aStartInfo,
                            aSpaceID,
                            aSegHdr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_exceed_segment_maxextents );
    {
         IDE_SET( ideSetErrorCode( smERR_ABORT_SegmentExceedMaxExtents, 
                  aSpaceID,
                  SD_MAKE_FID( aSegHandle->mSegPID ),
                  SD_MAKE_FPID( aSegHandle->mSegPID ),
                  sCurExtCnt,
                  sMaxExtCnt) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Extent�� aExtCount������ŭ Tablespace�� ����
 *               �Ҵ��Ѵ�.
 *
 * aStatistics      - [IN] ��� ����
 * aStartInfo       - [IN] Start Info
 * aSpaceID         - [IN] SpaceID
 * aSegHandle       - [IN] Segment Handle
 * aExtCount        - [IN] �Ҵ��ϰ��� �ϴ� Extent����
 ***********************************************************************/
IDE_RC sdpsfExtMgr::allocMutliExt( idvSQL           * aStatistics,
                                   sdrMtxStartInfo  * aStartInfo,
                                   scSpaceID          aSpaceID,
                                   sdpSegHandle     * aSegHandle,
                                   UInt               aExtCount )
{
    sdpsfSegHdr *sSegHdr;
    SInt         sState = 0;
    UInt         i;
    UInt         sMaxExtCnt;
    UInt         sCurExtCnt;

    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aSegHandle   != NULL );
    IDE_ASSERT( aExtCount    != 0 );

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                               aSpaceID,
                                               aSegHandle->mSegPID,
                                               &sSegHdr )
              != IDE_SUCCESS );
    sState = 1;

    sMaxExtCnt = aSegHandle->mSegStoAttr.mMaxExtCnt;

    for( i = 0 ; i < aExtCount; i++ )
    {
        sCurExtCnt = sdpsfExtMgr::getExtCnt( sSegHdr );

        IDE_TEST_RAISE( sCurExtCnt + 1 > sMaxExtCnt,
                        error_exceed_segment_maxextents );

        IDE_TEST( allocExt( aStatistics,
                            aStartInfo,
                            aSpaceID,
                            sSegHdr )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdpsfSH::releaseSegHdr( aStatistics,
                                      sSegHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_exceed_segment_maxextents );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_SegmentExceedMaxExtents, 
                                  aSpaceID,
                                  SD_MAKE_FID( aSegHandle->mSegPID ),
                                  SD_MAKE_FPID( aSegHandle->mSegPID ),
                                  sCurExtCnt,
                                  sMaxExtCnt) );
    }
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdpsfSH::releaseSegHdr( aStatistics,
                                            sSegHdr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtRID�� �ش��ϴ� ExtDesc�� aExtDescPtr�� �����ؼ�
 *               �ش�.
 *
 * Caution:
 *  1. ExtDesc�� ������ ExtDesc�� ��ġ�� �������� ���� Fix�� �Ѵ�. �
 *     Latch�� ���� �ʴ´�. ���� ����� �ϴ� ExtDesc�� ���ŵ� ���ɼ���
 *     �ִٸ� �� �Լ��� ����ؼ��� �ȵȴ�.
 *
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aExtRID          - [IN] ExtDesc�� ����� �ϴ� ExtRID
 *
 * aExtDescPtr      - [OUT] ExtDesc�� �������� ����
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getExtDesc( idvSQL       * aStatistics,
                                scSpaceID      aSpaceID,
                                sdRID          aExtRID,
                                sdpsfExtDesc * aExtDescPtr )
{
    sdpsfExtDesc *sExtDescPtr;
    UChar        *sPagePtr;
    SInt          sState = 0;

    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aExtRID     != SD_NULL_RID );
    IDE_ASSERT( aExtDescPtr != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByRID( aStatistics,
                                          aSpaceID,
                                          aExtRID,
                                          (UChar **)&sExtDescPtr )
              != IDE_SUCCESS );

    IDE_ASSERT( sExtDescPtr != NULL );

    /* �����Ѵ�. */
    *aExtDescPtr = *sExtDescPtr;

    sPagePtr = sdpPhyPage::getPageStartPtr( sExtDescPtr);

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                       sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� ��� Extent�� Free��Ų��.
 *
 * Caution:
 *  1. �� �Լ��� Return�ɶ� TBS Header�� XLatch�� �ɷ��ִ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHdr      - [IN] Segment Hdr
 ***********************************************************************/
IDE_RC sdpsfExtMgr::freeAllExts( idvSQL         * aStatistics,
                                 sdrMtx         * aMtx,
                                 scSpaceID        aSpaceID,
                                 sdpsfSegHdr    * aSegHdr )
{
    sdpDblPIDListBase * sExtDirPIDLst;
    sdrMtxStartInfo     sStartInfo;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );

    sStartInfo.mTrans = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

    sExtDirPIDLst = &aSegHdr->mExtDirPIDList;

    /* Parallel Direct Path Insert�� ������ Segment�� ���
     * Merge�Ŀ� Temp Segment�� ������ �� Segment�� �ǰ� �ȴ�. */
    if( sdpDblPIDList::getNodeCnt( sExtDirPIDLst ) != 0 )
    {
        IDE_TEST( freeExtsExceptFst( aStatistics,
                                     aMtx,
                                     aSpaceID,
                                     aSegHdr )
                  != IDE_SUCCESS );

        IDU_FIT_POINT( "1.PROJ-1671@sdpsfExtMgr::freeAllExts" );

        /* Parallel DPath Insert�� Merge Step���� Segment Hdr�� ���Ե� ù��°
         * Extent�� �̹� DPath Insert��� Segment�� Move�Ǿ��ٸ� �� Segment
         * Hdr�� ���� Extent�� TBS�� ��ȯ�ؼ��� �ȵȴ�. */
        if( sdpDblPIDList::getNodeCnt( sExtDirPIDLst ) != 0 )
        {
            IDE_TEST( sdpDblPIDList::initBaseNode( sExtDirPIDLst,
                                                   aMtx )
                      != IDE_SUCCESS );

            IDE_TEST( sdpsfExtDirPage::freeLstExt( aStatistics,
                                                   aMtx,
                                                   aSpaceID,
                                                   aSegHdr,
                                                   &aSegHdr->mExtDirCntlHdr )
                      != IDE_SUCCESS );
        }

        IDU_FIT_POINT( "2.PROJ-1671@sdpsfExtMgr::freeAllExts" );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� ù���� Extent�� ������ ��� Extent�� TBS��
 *               ��ȯ�Ѵ�.
 *
 * Caution:
 *  1. �� �Լ��� Return�ɶ� TBS Header�� XLatch�� �ɷ��ִ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHdr      - [IN] Segment Header
 ***********************************************************************/
IDE_RC sdpsfExtMgr::freeExtsExceptFst( idvSQL       * aStatistics,
                                       sdrMtx       * aMtx,
                                       scSpaceID      aSpaceID,
                                       sdpsfSegHdr  * aSegHdr )
{
    sdrMtx               sFreeMtx;
    ULong                sExtPageCount;
    sdrMtxStartInfo      sStartInfo;
    sdpsfExtDirCntlHdr * sExtDirCntlHdr;
    scPageID             sCurExtDirPID;
    scPageID             sPrvExtDirPID;
    UChar              * sExtDirPagePtr;
    sdpPhyPageHdr      * sPhyHdrOfExtDirPage;
    SInt                 sState = 0;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

    /* Segment Header�� ExtDirPage�� �ִ� Extent�� ù��°�� ������ ��� Extent
     * �� Free�Ѵ�. ù��°�� Segment Header�� ������ Extent�̹Ƿ� ���� ��������
     * Free�ϵ��� �Ѵ�. */
    sExtPageCount = sdpDblPIDList::getNodeCnt( &aSegHdr->mExtDirPIDList );
    sCurExtDirPID = sdpDblPIDList::getListTailNode( &aSegHdr->mExtDirPIDList );

    while( ( sExtPageCount != 0 ) && ( aSegHdr->mSegHdrPID != sCurExtDirPID ) )
    {
        sExtPageCount--;

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       &sStartInfo,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        /* Extent Dir Page�� First Extent�� ������ ��� Extent��
         * Free�Ѵ�. */
        IDE_TEST( sdpsfExtDirPage::getPage4Update( aStatistics,
                                                   &sFreeMtx,
                                                   aSpaceID,
                                                   sCurExtDirPID,
                                                   &sExtDirPagePtr,
                                                   &sExtDirCntlHdr )
                  != IDE_SUCCESS );

        sPhyHdrOfExtDirPage = sdpPhyPage::getHdr( sExtDirPagePtr );
        sPrvExtDirPID       = sdpPhyPage::getPrvPIDOfDblList( sPhyHdrOfExtDirPage );

        IDE_TEST( sdpsfExtDirPage::freeAllExtExceptFst( aStatistics,
                                                        &sStartInfo,
                                                        aSpaceID,
                                                        aSegHdr,
                                                        sExtDirCntlHdr )
                  != IDE_SUCCESS );

        /* Extent Dir Page�� ������ ���� Fst Extent�� Free�ϰ� Fst Extent
         * �� ���� �ִ� Extent Dir Page�� ����Ʈ���� �����Ѵ�. �� �ο�����
         * �ϳ��� Mini Transaction���� ����� �Ѵ�. �ֳĸ� Extent�� Free��
         * ExtDirPage�� free�Ǳ� ������ �� �������� ExtDirPage List����
         * ���ŵǾ�� �Ѵ�. */
        IDE_TEST( sdpsfExtDirPage::freeLstExt( aStatistics,
                                               &sFreeMtx,
                                               aSpaceID,
                                               aSegHdr,
                                               sExtDirCntlHdr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpsfExtDirPageList::unlinkPage( aStatistics,
                                                   &sFreeMtx,
                                                   aSegHdr,
                                                   sExtDirCntlHdr )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx )
                  != IDE_SUCCESS );

        sCurExtDirPID = sPrvExtDirPID;
    }

    /* FIT/ART/sm/Projects/PROJ-1671/freelist-seg/free_extent.tc */
    IDU_FIT_POINT( "1.PROJ-1671@sdpsfExtMgr::freeExtsExceptFst" );

    /* Parallel Direct Insert�� Temp Segemnt�� Target Segment�� Merge�ÿ�
     * Temp Segment�� ù��° ExtDirPage�� ��� Extent�� �ű�� �� ��������
     * Link���� ������ ���·� ������ ����ȴٸ� Temp Segment�� SegHdr��
     * ���� Extent�� Target Segment�� add�� �����̰� SegHdr�� ExtDirPage
     * List���� ���ŵ� �����̴�. �׷��Ƿ� Segment Hdr�� �����ϰ� ���
     * ExtDirPage�� �������� �� ExtDirPage List�� ���� �ִ� �������� ���ٸ�
     * ���� ���� ��Ȳ�� �߻��� ���̴�. �̶��� SegHdr�� Extent�� �����Ѵ�. */
    if( sCurExtDirPID != SD_NULL_PID )
    {
        IDE_ASSERT( aSegHdr->mSegHdrPID == sCurExtDirPID );

        sExtDirCntlHdr = &aSegHdr->mExtDirCntlHdr;

        IDE_TEST( sdpsfExtDirPage::freeAllExtExceptFst( aStatistics,
                                                        &sStartInfo,
                                                        aSpaceID,
                                                        aSegHdr,
                                                        sExtDirCntlHdr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) ==
                    IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ѹ��� �Ҵ���� ���� ���ο� �������� �Ҵ��Ѵ�. ������
 *               HWM�� �̵��Ѵ�. HWM�� ������ ���� ������ �� �ֱٿ�
 *               �Ҵ�� �������� ����Ų��.
 *
 * Caution:
 *  1. �� �Լ��� ȣ��ɶ� SegHeader�� �ִ� �������� XLatch�� �ɷ�
 *     �־�� �Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aAllocMtx      - [IN] Page All Mini Transaction Pointer
 * aCrtMtx        - [IN] Page Create Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header
 * aNextExtCnt    - [IN] Segment Ȯ��� �Ҵ�Ǵ� Extent����
 * aPageType      - [IN] Page Type
 *
 * aPageID        - [OUT] �Ҵ�� PageID
 * aAllocPagePtr  - [OUT] �Ҵ�� Page Pointer
 ***********************************************************************/
IDE_RC sdpsfExtMgr::allocPage( idvSQL               * aStatistics,
                               sdrMtx               * aAllocMtx,
                               sdrMtx               * aCrtMtx,
                               scSpaceID              aSpaceID,
                               sdpsfSegHdr          * aSegHdr,
                               sdpSegHandle         * aSegHandle,
                               UInt                   aNextExtCnt,
                               sdpPageType            aPageType,
                               scPageID             * aPageID,
                               UChar               ** aAllocPagePtr )
{
    scPageID            sAllocPID;
    scPageID            sFstExtPID;
    sdRID               sAllocExtRID;
    UChar              *sSegPagePtr;

    IDE_ASSERT( aAllocMtx     != NULL );
    IDE_ASSERT( aCrtMtx       != NULL );
    IDE_ASSERT( aSpaceID      != 0 );
    IDE_ASSERT( aSegHdr       != NULL );
    IDE_ASSERT( aNextExtCnt   != 0 );
    IDE_ASSERT( aPageType     < SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aPageID       != NULL );
    IDE_ASSERT( aAllocPagePtr != NULL );

    sSegPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    /* Ext List���� Free Page�� �Ҵ��Ѵ�. */
    IDE_TEST( allocNewPage( aStatistics,
                            aAllocMtx,
                            aSpaceID,
                            aSegHdr,
                            aSegHandle,
                            aNextExtCnt,
                            aSegHdr->mAllocExtRID,
                            aSegHdr->mFstPIDOfAllocExt,
                            aSegHdr->mHWMPID,
                            &sAllocExtRID,
                            &sFstExtPID,
                            &sAllocPID,
                            ID_FALSE )
              != IDE_SUCCESS );

    IDE_ASSERT( sAllocPID != SD_NULL_PID );

    IDE_TEST( sdrMiniTrans::setDirtyPage( aAllocMtx, sSegPagePtr )
              != IDE_SUCCESS );

    /* Alloc Page������ �÷� �ش�. */
    IDE_TEST( sdpsfSH::setFmtPageCnt( aAllocMtx,
                                      aSegHdr,
                                      aSegHdr->mFmtPageCnt + 1 )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfSH::setHWM( aAllocMtx, aSegHdr, sAllocPID )
              != IDE_SUCCESS );

    /* Alloc Extent�� �̵��Ͽ����� �������ش�. */
    if( sAllocExtRID != aSegHdr->mAllocExtRID )
    {
        IDE_TEST( sdpsfSH::setAllocExtRID( aAllocMtx, aSegHdr, sAllocExtRID )
                  != IDE_SUCCESS );

        IDE_TEST( sdpsfSH::setFstPIDOfAllocExt( aAllocMtx, aSegHdr, sFstExtPID )
                  != IDE_SUCCESS );
    }

    /* Alloc�� �������� X Latch�� �� �Լ��� ���ϵǴ��� Ǯ����
     * �ʰ� �Ѵ�. */
    IDE_TEST( sdpPhyPage::create( aStatistics,
                                  aSpaceID,
                                  sAllocPID,
                                  NULL,      /* Parent Info */
                                  SDPSF_PAGE_USED_INSERTABLE,
                                  aPageType,
                                  ((sdpSegCCache*)aSegHandle->mCache)
                                      ->mTableOID,
                                  ((sdpSegCCache*)aSegHandle->mCache)
                                      ->mIndexID,
                                  aCrtMtx,   /* Create Page Mtx */
                                  aAllocMtx, /* Init Page Mtx */
                                  aAllocPagePtr )
              != IDE_SUCCESS );

    *aPageID = sAllocPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aAllocPagePtr = NULL;
    *aPageID       = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aPrvAllocExtRID�� ����Ű�� Extent�� aPrvAllocPageID����
 *               Page�� �����ϴ� �ϴ��� üũ�ؼ� ������ ���ο� ����
 *               Extent�� �̵��ϰ� ���� Extent�� ������ TBS�� ���� ���ο�
 *               Extent�� �Ҵ�޴´�. ���� Extent���� Free Page�� ã�Ƽ�
 *               Page�� �Ҵ�� ExtRID�� PageID�� �Ѱ��ش�.
 *
 * Caution:
 *  1. �� �Լ��� ȣ��ɶ� SegHdr�� �ִ� �������� XLatch�� �ɷ� �־�� �Ѵ�.
 *
 * aStatistics             - [IN] ��� ����
 * aMtx                    - [IN] Mini Transaction Pointer
 * aSpaceID                - [IN] TableSpace ID
 * aSegHdr                 - [IN] Segment Header
 * aNxtExtCnt              - [IN] Ȯ��� �Ҵ�Ǵ� Extent�� ����
 * aPrvAllocExtRID         - [IN] ������ Page�� �Ҵ��� Extent RID
 * aFstPIDOfPrvExtAllocExt - [IN] ������ Page�� �Ҵ��� Extent�� ù��° PID
 * aPrvAllocPageID         - [IN] ������ �Ҵ���� PageID
 *
 * aAllocExtRID      - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aFstPIDOfAllocExt - [OUT] ���ο� Page�� �Ҵ�� Extent�� ù���� PID
 * aAllocPID         - [OUT] ���Ӱ� �Ҵ���� PageID
 *
 * [BUG-21111]
 * ���� tx�� ���� extend �߿� sement latch�� ������ Append mode�� �ƴ� ���
 * aPrvAllocExtRID,aFstPIDOfPrvExtAllocExt, aPrvAllocPageID�� ������ �����
 * �� �־ extend���Ŀ� �ٽ� ���´�
 *
 ***********************************************************************/
IDE_RC sdpsfExtMgr::allocNewPage( idvSQL              * aStatistics,
                                  sdrMtx              * aMtx,
                                  scSpaceID             aSpaceID,
                                  sdpsfSegHdr         * aSegHdr,
                                  sdpSegHandle        * aSegHandle,
                                  UInt                  aNxtExtCnt,
                                  sdRID                 aPrvAllocExtRID,
                                  scPageID              aFstPIDOfPrvExtAllocExt,
                                  scPageID              aPrvAllocPageID,
                                  sdRID               * aAllocExtRID,
                                  scPageID            * aFstPIDOfAllocExt,
                                  scPageID            * aAllocPID,
                                  idBool                aIsAppendMode)
{
    idBool             sIsNeedNewExt;
    sdRID              sAllocExtRID;
    sdrMtxStartInfo    sStartInfo;
    UChar             *sSegPagePtr;
    scPageID           sFstDataPIDOfExt;
    scPageID           sFstPIDOfExt = SD_NULL_PID;
    sdRID              sPrvAllocExtRID = aPrvAllocExtRID;
    scPageID           sFstPIDOfPrvExtAllocExt = aFstPIDOfPrvExtAllocExt;
    scPageID           sPrvAllocPageID = aPrvAllocPageID;

    IDE_ASSERT( aMtx               != NULL );
    IDE_ASSERT( aSpaceID           != 0 );
    IDE_ASSERT( aSegHdr            != NULL );
    IDE_ASSERT( aNxtExtCnt         != 0 );
    IDE_ASSERT( aAllocExtRID       != NULL );
    IDE_ASSERT( aFstPIDOfAllocExt  != NULL );
    IDE_ASSERT( aAllocPID          != NULL );

    sSegPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    sStartInfo.mTrans = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

  retry :

    sIsNeedNewExt = ID_TRUE;

    /* ������ �������� �Ҵ���� Extent�� sPrvAllocPageID ����
     * �������� �����ϴ� �� Check�Ѵ�. */
    if( sPrvAllocExtRID != SD_NULL_RID )
    {
        IDE_ASSERT( sPrvAllocPageID != SD_NULL_PID );

        if( isFreePIDInExt( aSegHdr,
                            sFstPIDOfPrvExtAllocExt,
                            sPrvAllocPageID) == ID_TRUE )
        {
            sIsNeedNewExt = ID_FALSE;
        }
    }

    sAllocExtRID  = SD_NULL_RID;

    if( sIsNeedNewExt == ID_TRUE )
    {
        /* sPrvAllocExtRID ���� Extent�� �����ϴ��� Check�Ѵ�. */
        if( sPrvAllocExtRID != SD_NULL_RID )
        {
            IDE_TEST( getNxtExt4Alloc( aStatistics,
                                       aSpaceID,
                                       aSegHdr,
                                       sPrvAllocExtRID,
                                       &sAllocExtRID,
                                       &sFstPIDOfExt,
                                       &sFstDataPIDOfExt ) != IDE_SUCCESS );

            if( sFstDataPIDOfExt != SD_NULL_PID )
            {
                IDE_ASSERT( sFstDataPIDOfExt != SD_MAKE_PID( sAllocExtRID ) );
            }
            else
            {
                IDE_ASSERT( sAllocExtRID == SD_NULL_RID );
            }
        }

        if( sAllocExtRID == SD_NULL_RID )
        {
            /* ���ο� Extent�� TBS�κ��� �Ҵ�޴´�. */
            IDE_TEST( extend( aStatistics,
                              &sStartInfo,
                              aSpaceID,
                              aSegHdr,
                              aSegHandle,
                              aNxtExtCnt ) != IDE_SUCCESS );

            // BUG-21111
            // ���ÿ� 2 tx �̻��� extend�� ������ ��
            // nextextents�� 2 �̻��̸� �߰��� segment x latch�� ���
            // ���� ���� ������ �����Ǿ� HWM �̵��� ��� extent��
            // �ǳʶ� �� ����
            // -->�ڽ��� �Ҵ��� ù extent�� HWM �ٷ� ���� extent���
            // ����� �� ����
            // D-Path Insert�� segment�� �������� �����Ƿ� �������
            if( aIsAppendMode == ID_FALSE )
            {
                sPrvAllocExtRID = aSegHdr->mAllocExtRID;
                sFstPIDOfPrvExtAllocExt = aSegHdr->mFstPIDOfAllocExt;
                sPrvAllocPageID = aSegHdr->mHWMPID;
            }
            goto retry;
        }

        IDE_ASSERT( sFstPIDOfExt != SD_NULL_PID );

        *aAllocPID         = sFstDataPIDOfExt;
        *aFstPIDOfAllocExt = sFstPIDOfExt;
        *aAllocExtRID      = sAllocExtRID;
    }
    else
    {
        /* �� Extent���� �������� ���ӵǾ� �����Ƿ� ���ο�
         * PageID�� ���� Alloc�ߴ� �������� 1���� ���� �ȴ�. */
        *aAllocPID         = sPrvAllocPageID + 1;
        *aFstPIDOfAllocExt = sFstPIDOfPrvExtAllocExt;
        *aAllocExtRID      = sPrvAllocExtRID;

        IDE_ASSERT( *aAllocPID != SD_MAKE_PID( sPrvAllocExtRID ) );
    }

    IDE_TEST( sdrMiniTrans::setDirtyPage( aMtx, sSegPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCurExtRID�� ����Ű�� Extent�� aPrvAllocPageID����
 *               Page�� �����ϴ� �ϴ��� üũ�ؼ� ������ ���ο� ����
 *               Extent�� �̵��ϰ� ���� Extent���� Free Page�� ã�Ƽ�
 *               Page�� �Ҵ�� ExtRID�� PageID�� �Ѱ��ش�.
 *
 *               ���� Extent�� ������ aNxtExtRID�� aFstDataPIDOfNxtExt
 *               �� SD_NULL_RID, SD_NULL_PID�� �ѱ��.
 *
 * Caution:
 *  1. �� �Լ��� ȣ��ɶ� SegHdr�� �ִ� �������� XLatch�� �ɷ� �־�� �Ѵ�.
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aSegHdr          - [IN] Segment Header
 * aCurExtRID       - [IN] ���� Extent RID
 *
 * aNxtExtRID          - [OUT] ���� Extent RID
 * aFstPIDOfExt        - [OUT] ���� Extent�� ù��° PageID
 * aFstDataPIDOfNxtExt - [OUT] ���� Extent �� ù��° Data Page ID, Extent��
 *                             ù��° �������� Extent Dir Page�� ���Ǳ⵵
 *                             �Ѵ�.
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getNxtExt4Alloc( idvSQL       * aStatistics,
                                     scSpaceID      aSpaceID,
                                     sdpsfSegHdr  * aSegHdr,
                                     sdRID          aCurExtRID,
                                     sdRID        * aNxtExtRID,
                                     scPageID     * aFstPIDOfExt,
                                     scPageID     * aFstDataPIDOfNxtExt )
{
    sdpsfExtDesc          sExtDesc;
    scPageID              sExtDirPID;
    scPageID              sNxtExtDirPID;
    scPageID              sNxtNxtExtPID;
    sdRID                 sNxtExtRID = SD_NULL_RID;
    sdRID                 sLstExtRID;
    UChar               * sExtDirPage;
    sdpsfExtDirCntlHdr  * sExtDirCntlHdr;
    sdpPhyPageHdr       * sExtPagePhyHdr;
    UShort                sExtCntInEDP;
    SInt                  sState = 0;

    IDE_ASSERT( aSpaceID            != 0 );
    IDE_ASSERT( aSegHdr             != NULL );
    IDE_ASSERT( aCurExtRID          != SD_NULL_RID );
    IDE_ASSERT( aNxtExtRID          != NULL );
    IDE_ASSERT( aFstPIDOfExt        != NULL );
    IDE_ASSERT( aFstDataPIDOfNxtExt != NULL );

    *aNxtExtRID          = SD_NULL_RID;
    *aFstDataPIDOfNxtExt = SD_NULL_PID;
    *aFstPIDOfExt        = SD_NULL_PID;

    sExtDirPID = SD_MAKE_PID( aCurExtRID );

    if( aSegHdr->mSegHdrPID != sExtDirPID )
    {
        IDE_TEST( sdpsfExtDirPage::fixPage( aStatistics,
                                            aSpaceID,
                                            sExtDirPID,
                                            &sExtDirPage,
                                            &sExtDirCntlHdr )
                  != IDE_SUCCESS );
        sState = 1;
    }
    else
    {
        sExtDirCntlHdr = &aSegHdr->mExtDirCntlHdr;
        sExtDirPage    = sdpPhyPage::getPageStartPtr( aSegHdr );
    }

    /* aCurExtRID ���� Extent�� ���� ExtDirPage���� �����ϴ�
     * �� �˻� */
    IDE_TEST( sdpsfExtDirPage::getNxtExt( sExtDirCntlHdr,
                                          aCurExtRID,
                                          &sNxtExtRID,
                                          &sExtDesc )
              != IDE_SUCCESS );

    if( sNxtExtRID == SD_NULL_RID )
    {
        /* ���� ExtentDirPage�� ���� Extent�� �������� �ʴ´ٸ�
         * ���� ExtentDirPage�� Extent�� �����ϴ� �� �˻� */
        sExtPagePhyHdr = sdpPhyPage::getHdr( sExtDirPage );
        sNxtExtDirPID  = sdpPhyPage::getNxtPIDOfDblList( sExtPagePhyHdr );

        if( sState == 1 )
        {
            sState = 0;
            IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                               sExtDirPage )
                      != IDE_SUCCESS );
        }

        if( sNxtExtDirPID != aSegHdr->mSegHdrPID )
        {
            /* aCurExtRID�� ���� ExtDirPage�� Next ExtDirPage�� ����
             * �Ѵٸ� */
            IDE_TEST( sdpsfExtDirPage::getPageInfo( aStatistics,
                                                    aSpaceID,
                                                    sNxtExtDirPID,
                                                    &sExtCntInEDP,
                                                    &sNxtExtRID,
                                                    &sExtDesc,
                                                    &sLstExtRID,
                                                    &sNxtNxtExtPID )
                      != IDE_SUCCESS );
        }
    }

    if( sNxtExtRID != SD_NULL_RID )
    {
        *aNxtExtRID          = sNxtExtRID;
        *aFstPIDOfExt        = sdpsfExtent::getFstPID( &sExtDesc );
        *aFstDataPIDOfNxtExt = sdpsfExtent::getFstDataPID( &sExtDesc );
    }

    if( sState == 1 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                           sExtDirPage )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sExtDirPage )
                    == IDE_SUCCESS );
    }

    *aNxtExtRID          = SD_NULL_RID;
    *aFstPIDOfExt        = SD_NULL_PID;
    *aFstDataPIDOfNxtExt = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCurExtRID�� ����Ű�� Extent�� ���� Extent�� RID�� ���Ѵ�.
 *
 * aStatistics         - [IN]  ��� ����
 * aSpaceID            - [IN]  TableSpace ID
* aSegHdrPID           - [IN]  SegHdr PID
 * aCurExtRID          - [IN]  ���� Extent RID
 * aNxtExtRID          - [OUT] ���� Extent RID
 * aFstPIDOfNxtExt     - [OUT] ���� Extent ù��° PageID
 * aFstDataPIDOfNxtExt - [OUT] ���� Extent�� ù���� Data PID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getNxtExt4Scan( idvSQL       * aStatistics,
                                    scSpaceID      aSpaceID,
                                    scPageID       aSegHdrPID,
                                    sdRID          aCurExtRID,
                                    sdRID        * aNxtExtRID,
                                    scPageID     * aFstPIDOfNxtExt,
                                    scPageID     * aFstDataPIDOfNxtExt )
{
    sdpsfExtDesc          sExtDesc;
    UChar               * sExtDirPage;
    sdRID                 sNxtExtRID;
    sdRID                 sLstExtRID;
    scPageID              sNxtNxtExtPID;
    UShort                sExtCntInEDP;
    scPageID              sNxtExtDirPID;
    sdpsfExtDirCntlHdr  * sExtDirCntlHdr;
    sdpPageType           sPageType;
    sdpPhyPageHdr       * sExtPhyPageHdr;
    sdpsfSegHdr         * sSegHdr;
    SInt                  sState = 0;

    IDE_ASSERT( aSpaceID            != 0 );
    IDE_ASSERT( aSegHdrPID          != SD_NULL_PID );
    IDE_ASSERT( aCurExtRID          != SD_NULL_RID );
    IDE_ASSERT( aNxtExtRID          != NULL );
    IDE_ASSERT( aFstPIDOfNxtExt     != NULL );
    IDE_ASSERT( aFstDataPIDOfNxtExt != NULL );

    *aNxtExtRID          = SD_NULL_RID;
    *aFstPIDOfNxtExt     = SD_NULL_PID;
    *aFstDataPIDOfNxtExt = SD_NULL_PID;

    IDE_TEST( sdpsfExtDirPage::fixPage( aStatistics,
                                        aSpaceID,
                                        SD_MAKE_PID( aCurExtRID ),
                                        &sExtDirPage,
                                        &sExtDirCntlHdr )
              != IDE_SUCCESS );
    sState = 1;

    sExtPhyPageHdr = sdpPhyPage::getHdr( sExtDirPage );
    sPageType      = sdpPhyPage::getPageType( sExtPhyPageHdr );

    if( sPageType == SDP_PAGE_FMS_SEGHDR )
    {
        sSegHdr        = sdpsfSH::getSegHdrFromPagePtr( sExtDirPage );
        sExtDirCntlHdr = &sSegHdr->mExtDirCntlHdr;
    }

    IDE_TEST( sdpsfExtDirPage::getNxtExt( sExtDirCntlHdr,
                                          aCurExtRID,
                                          &sNxtExtRID,
                                          &sExtDesc )
              != IDE_SUCCESS );

    if( sNxtExtRID == SD_NULL_RID )
    {
        sExtPhyPageHdr = sdpPhyPage::getHdr( sExtDirPage );
        sNxtExtDirPID  = sdpPhyPage::getNxtPIDOfDblList( sExtPhyPageHdr );

        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                           sExtDirPage )
                  != IDE_SUCCESS );

        if( sNxtExtDirPID != aSegHdrPID )
        {
            IDE_TEST( sdpsfExtDirPage::getPageInfo( aStatistics,
                                                    aSpaceID,
                                                    sNxtExtDirPID,
                                                    &sExtCntInEDP,
                                                    &sNxtExtRID,
                                                    &sExtDesc,
                                                    &sLstExtRID,
                                                    &sNxtNxtExtPID )
                      != IDE_SUCCESS );
        }
    }

    if( sNxtExtRID != SD_NULL_RID )
    {
        *aNxtExtRID          = sNxtExtRID;
        *aFstPIDOfNxtExt     = sdpsfExtent::getFstPID( &sExtDesc );
        *aFstDataPIDOfNxtExt = sdpsfExtent::getFstDataPID( &sExtDesc );
    }

    if( sState == 1 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                           sExtDirPage )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sExtDirPage )
                    == IDE_SUCCESS );
    }

    *aNxtExtRID          = SD_NULL_RID;
    *aFstPIDOfNxtExt     = SD_NULL_PID;
    *aFstDataPIDOfNxtExt = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aCurExtRID�� ����Ű�� Extent�� ���� Extent�� RID�� ���Ѵ�.
 *
 * aStatistics         - [IN]  ��� ����
 * aSpaceID            - [IN]  TableSpace ID
 * aSegHdrPID          - [IN]  SegHdr PID
 * aCurExtRID          - [IN]  ���� Extent RID
 *
 * aNxtExtRID          - [OUT] ���� Extent RID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getNxtExtRID( idvSQL       * aStatistics,
                                  scSpaceID      aSpaceID,
                                  scPageID       aSegHdrPID,
                                  sdRID          aCurExtRID,
                                  sdRID        * aNxtExtRID)
{
    scPageID     sFstPIDOfNxtExt;
    scPageID     sFstDataPIDOfNxtExt;

    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aSegHdrPID  != SD_NULL_PID );
    IDE_ASSERT( aCurExtRID  != SD_NULL_RID );
    IDE_ASSERT( aNxtExtRID  != NULL );

    IDE_TEST( getNxtExt4Scan( aStatistics,
                              aSpaceID,
                              aSegHdrPID,
                              aCurExtRID,
                              aNxtExtRID,
                              &sFstPIDOfNxtExt,
                              &sFstDataPIDOfNxtExt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Extent List�� <Extent�� ����������>, <ù��° ExtRID>,
 *               <������ ExtRID>�� ��´�.
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aSegRID          - [IN] Segment Extent List
 *
 * aPageCntInExt    - [OUT] Extent�� ������ ����
 * aFstExtRID       - [OUT] ù��° Ext RID
 * aLstExtRID       - [OUT] ������ Ext RID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getExtListInfo( idvSQL    * aStatistics,
                                    scSpaceID   aSpaceID,
                                    scPageID    aSegPID,
                                    UInt      * aPageCntInExt,
                                    sdRID     * aFstExtRID,
                                    sdRID     * aLstExtRID )
{
    sdpsfSegHdr   * sSegHdr;
    UChar         * sSegPagePtr = NULL;
    SInt            sState      = 0;

    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegPID        != SD_NULL_PID );
    IDE_ASSERT( aPageCntInExt  != NULL );
    IDE_ASSERT( aFstExtRID     != NULL );
    IDE_ASSERT( aLstExtRID     != NULL );

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Read( aStatistics,
                                             aSpaceID,
                                             aSegPID,
                                             &sSegHdr )
              != IDE_SUCCESS );
    sSegPagePtr = sdpPhyPage::getPageStartPtr( (UChar*)sSegHdr );
    sState = 1;

    *aPageCntInExt = sSegHdr->mPageCntInExt;
    *aFstExtRID    = sdpsfExtDirPage::getFstExtRID( &sSegHdr->mExtDirCntlHdr );
    *aLstExtRID    = sSegHdr->mAllocExtRID;

    sState = 0;
    IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                           sSegPagePtr )
                == IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               sSegPagePtr )
                    == IDE_SUCCESS );
    }

    *aPageCntInExt = 0;
    *aFstExtRID    = SD_NULL_RID;
    *aLstExtRID    = SD_NULL_RID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : *aPageID ���� Alloc �������� ���Ѵ�. Segment Extent List
 *                �� ���󰡸鼭 ���� �������� ���ϴ� ���̱� ������ ����
 *                Extent���� ���̰� ���̻� ������ ���� Extent�� �̵��Ѵ�.
 *                ���� ���� Extent�� Segment�� mLstAllocRID�� ���ų�
 *                ���� PageID�� Segment�� mHWM�� ���ٸ� �� ���� ����������
 *                �ѹ��� Alloc���� ���� �����̱⶧���� aExtRID, aPageID��
 *                SD_NULL_RID, SD_NULL_PID�� �����Ѵ�.
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aSegInfo         - [IN] Segment Info
 * aSegCacheInfo    - [IN] Segment Cache Info
 *
 * aExtRID          - [INOUT] Extent RID
 * aExtInfo         - [INOUT] aExtRID�� ����Ű�� Extent Info
 * aPageID          - [INOUT] IN: ������ �Ҵ��� ������ ID, OUT: ���� �Ҵ�
 *                            �� ������ ID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getNxtAllocPage( idvSQL             * aStatistics,
                                     scSpaceID            aSpaceID,
                                     sdpSegInfo         * aSegInfo,
                                     sdpSegCacheInfo    * /*aSegCacheInfo*/,
                                     sdRID              * aExtRID,
                                     sdpExtInfo         * aExtInfo,
                                     scPageID           * aPageID )
{
    sdRID    sNxtExtRID;
    sdRID    sCurExtRID;
    scPageID sFstPIDOfNxtExt;
    scPageID sFstDataPIDOfNxtExt;
    scPageID sNxtPID;
    scPageID sCurPID;

    IDE_ASSERT(  aSpaceID != 0 );
    IDE_ASSERT(  aSegInfo != NULL );
    IDE_ASSERT(  aExtRID  != NULL );
    IDE_ASSERT(  aExtInfo != NULL );
    IDE_ASSERT(  aPageID  != NULL );
    IDE_ASSERT( *aExtRID  != SD_NULL_RID );

    sCurPID    = *aPageID;
    sCurExtRID = *aExtRID;
    sNxtExtRID =  sCurExtRID;

    if( sCurPID == SD_NULL_PID )
    {
        sNxtPID = aExtInfo->mFstPID;
    }
    else
    {
        IDE_TEST_CONT( sCurPID == aSegInfo->mHWMPID,
                        cont_no_more_page );

        sNxtPID = sCurPID + 1;

        if( sdpsfExtent::isPIDInExt( aExtInfo,
                                     aSegInfo->mPageCntInExt,
                                     sNxtPID ) == ID_FALSE )
        {
            IDE_TEST_CONT( aSegInfo->mLstAllocExtRID == sCurExtRID,
                            cont_no_more_page );

            IDE_TEST( getNxtExt4Scan( aStatistics,
                                      aSpaceID,
                                      aSegInfo->mSegHdrPID,
                                      sCurExtRID,
                                      &sNxtExtRID,
                                      &sFstPIDOfNxtExt,
                                      &sFstDataPIDOfNxtExt ) != IDE_SUCCESS );

            IDE_ASSERT( sNxtExtRID != SD_NULL_RID );
            IDE_ASSERT( sFstDataPIDOfNxtExt != SD_NULL_PID );

            aExtInfo->mFstPID     = sFstPIDOfNxtExt;
            aExtInfo->mFstDataPID = sFstDataPIDOfNxtExt;

            sNxtPID = sFstPIDOfNxtExt;
        }
    }

    if( sNxtPID == aSegInfo->mHWMPID )
    {
        /* ������ Page�� Meta�̸� �� �̻� ���� �������� �������� �ʴ´�. */
        IDE_TEST_CONT( sNxtPID < aExtInfo->mFstDataPID, cont_no_more_page );
    }

    if( sNxtPID == aExtInfo->mFstPID )
    {
        /* Segment�� Meta Page�� Skip�Ѵ� .*/
        sNxtPID = aExtInfo->mFstDataPID;
    }

    *aExtRID = sNxtExtRID;
    *aPageID = sNxtPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( cont_no_more_page );

    *aExtRID = SD_NULL_RID;
    *aPageID = SD_NULL_PID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aExtRID = SD_NULL_RID;
    *aPageID = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtRID�� ����Ű�� Extent�� ������ ���Ѵ�.
 *
 * aStatistics   - [IN] ��� ����
 * aSpaceID      - [IN] TableSpace ID
 * aExtRID       - [IN] Extent RID
 *
 * aExtInfo      - [OUT] aExtRID�� ����Ű�� ExtInfo
 ***********************************************************************/
IDE_RC sdpsfExtMgr::getExtInfo( idvSQL       *aStatistics,
                                scSpaceID     aSpaceID,
                                sdRID         aExtRID,
                                sdpExtInfo   *aExtInfo )
{
    sdpsfExtDesc sExtDesc;

    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aExtRID  != SD_NULL_RID );
    IDE_ASSERT( aExtInfo != NULL );

    IDE_TEST( getExtDesc( aStatistics,
                          aSpaceID,
                          aExtRID,
                          &sExtDesc )
              != IDE_SUCCESS );

    aExtInfo->mFstPID = sExtDesc.mFstPID;

    if( SDP_SF_IS_FST_EXTDIRPAGE_AT_EXT( sExtDesc.mFlag ) )
    {
        aExtInfo->mFstDataPID = sExtDesc.mFstPID + 1;
    }
    else
    {
        aExtInfo->mFstDataPID = sExtDesc.mFstPID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment���� aExtRID ������ ��� Extent�� TBS�� ��ȯ�Ѵ�.
 *
 * aStatistics   - [IN] ��� ����
 * aMtx          - [IN] Mini Transaction Pointer
 * aSpaceID      - [IN] TableSpace ID
 * aSegHdr       - [IN] Segment Header
 * aExtRID       - [IN] Extent RID
 ***********************************************************************/
IDE_RC sdpsfExtMgr::freeAllNxtExt( idvSQL       * aStatistics,
                                   sdrMtx       * aMtx,
                                   scSpaceID      aSpaceID,
                                   sdpsfSegHdr  * aSegHdr,
                                   sdRID          aExtRID )
{
    sdrMtx               sFreeMtx;
    ULong                sExtPageCount;
    sdrMtxStartInfo      sStartInfo;
    UChar              * sExtDirPagePtr;
    sdpsfExtDirCntlHdr * sExtDirCntlHdr;
    scPageID             sCurExtDirPID;
    scPageID             sPrvExtDirPID;
    scPageID             sFstFreeExtDirPID;
    sdpPhyPageHdr      * sPhyHdrOfExtDirPage;
    SInt                 sState = 0;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );
    IDE_ASSERT( aExtRID  != SD_NULL_RID );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

    /* Segment Header�� ExtDirPage�� �ִ� Extent�� ù��°�� ������ ��� Extent
     * �� Free�Ѵ�. ù��°�� Segment Header�� ������ Extent�̹Ƿ� ���� ��������
     * Free�ϵ��� �Ѵ�. */
    sExtDirCntlHdr    = &aSegHdr->mExtDirCntlHdr;
    sExtPageCount     = sdpDblPIDList::getNodeCnt( &aSegHdr->mExtDirPIDList );
    sCurExtDirPID     = sdpDblPIDList::getListTailNode( &aSegHdr->mExtDirPIDList );
    sFstFreeExtDirPID = SD_MAKE_PID( aExtRID );

    while( sExtPageCount > 0 )
    {
        sExtPageCount--;

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       &sStartInfo,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        if( sCurExtDirPID != aSegHdr->mSegHdrPID )
        {
            IDE_TEST( sdpsfExtDirPage::getPage4Update( aStatistics,
                                                       &sFreeMtx,
                                                       aSpaceID,
                                                       sCurExtDirPID,
                                                       &sExtDirPagePtr,
                                                       &sExtDirCntlHdr )
                  != IDE_SUCCESS );
        }
        else
        {
            sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );
            sExtDirCntlHdr = &aSegHdr->mExtDirCntlHdr;

            IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx, sExtDirPagePtr )
                      != IDE_SUCCESS );
        }

        sPhyHdrOfExtDirPage = sdpPhyPage::getHdr( sExtDirPagePtr );
        sPrvExtDirPID       = sdpPhyPage::getPrvPIDOfDblList( sPhyHdrOfExtDirPage );

        if( sFstFreeExtDirPID != sCurExtDirPID )
        {
            IDE_TEST( sdpsfExtDirPage::freeAllExtExceptFst( aStatistics,
                                                            &sStartInfo,
                                                            aSpaceID,
                                                            aSegHdr,
                                                            sExtDirCntlHdr )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( sdpsfExtDirPage::freeAllNxtExt( aStatistics,
                                                      &sStartInfo,
                                                      aSpaceID,
                                                      aSegHdr,
                                                      sExtDirCntlHdr,
                                                      aExtRID )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sFreeMtx )
                      != IDE_SUCCESS );

            break;
        }

        /* Extent Dir Page�� ������ ���� Fst Extent�� Free�ϰ� Fst Extent
         * �� ���� �ִ� Extent Dir Page�� ����Ʈ���� �����Ѵ�. �� �ο�����
         * �ϳ��� Mini Transaction���� ����� �Ѵ�. �ֳĸ� Extent�� Free��
         * ExtDirPage�� free�Ǳ� ������ �� �������� ExtDirPage List����
         * ���ŵǾ�� �Ѵ�. */
        IDE_TEST( sdpsfExtDirPage::freeLstExt( aStatistics,
                                               &sFreeMtx,
                                               aSpaceID,
                                               aSegHdr,
                                               sExtDirCntlHdr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpsfExtDirPageList::unlinkPage( aStatistics,
                                                   &sFreeMtx,
                                                   aSegHdr,
                                                   sExtDirCntlHdr )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx )
                  != IDE_SUCCESS );

        sCurExtDirPID = sPrvExtDirPID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) ==
                    IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


