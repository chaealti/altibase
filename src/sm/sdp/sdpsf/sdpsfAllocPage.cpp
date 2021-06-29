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
#include <sdr.h>
#include <sdp.h>
#include <sdpPhyPage.h>
#include <sdpSglRIDList.h>

#include <sdpsfExtent.h>

#include <sdpsfDef.h>
#include <sdpsfSH.h>
#include <sdpReq.h>
#include <sdpsfExtMgr.h>
#include <sdpsfAllocPage.h>

/***********************************************************************
 * Description : ������ ���������� ã�´�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHdr      - [IN] Segment Header
 * aSegHandle   - [IN] Segment Handle
 * aPageType    - [IN] Page Type
 *
 * aPagePtr     - [OUT] ������ �� Page�� ���� Pointer�� �����ȴ�.
 *                      return �� �ش� �������� XLatch�� �ɷ��ִ�.
 *
 ***********************************************************************/
IDE_RC sdpsfAllocPage::allocNewPage( idvSQL        *aStatistics,
                                     sdrMtx        *aMtx,
                                     scSpaceID      aSpaceID,
                                     sdpSegHandle  *aSegHandle,
                                     sdpsfSegHdr   *aSegHdr,
                                     sdpPageType    aPageType,
                                     UChar        **aPagePtr )
{
    UChar             *sPagePtr = NULL;
    sdpPhyPageHdr     *sPageHdr = NULL;
    sdrMtxStartInfo    sStartInfo;
    scPageID           sAllocPID;
    sdrMtx             sAllocMtx;
    ULong              sNTAData[2];
    SInt               sState = 0;
    smLSN              sNTA;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aSegHdr     != NULL );
    IDE_ASSERT( aPageType   < SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aPagePtr    != NULL );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    if( sStartInfo.mTrans != NULL )
    {
        sNTA = smLayerCallback::getLstUndoNxtLSN( sStartInfo.mTrans );
    }

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sAllocMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    /* Private PageList���� ã�´�. */
    IDE_TEST( sdpsfPvtFreePIDList::removeAtHead(
                  aStatistics,
                  &sAllocMtx,
                  aMtx, /* BM Create Page Mtx */
                  aSpaceID,
                  aSegHdr,
                  aPageType,
                  &sAllocPID,
                  &sPagePtr )
              != IDE_SUCCESS );

    if( sPagePtr == NULL )
    {
        /* UnFormat PageList���� ã�´�. */
        IDE_TEST( sdpsfUFmtPIDList::removeAtHead(
                      aStatistics,
                      &sAllocMtx,
                      aMtx, /* BM Create Page Mtx */
                      aSpaceID,
                      aSegHdr,
                      aPageType,
                      &sAllocPID,
                      &sPagePtr )
                  != IDE_SUCCESS );
    }

    if( sPagePtr == NULL )
    {
        /* ExtList���� ���ο� �������� Ȯ���Ѵ�. */
        IDE_TEST( sdpsfExtMgr::allocPage( aStatistics,
                                          &sAllocMtx,
                                          aMtx, /* BM Create Page Mtx */
                                          aSpaceID,
                                          aSegHdr,
                                          aSegHandle,
                                          aSegHandle->mSegStoAttr.mNextExtCnt,
                                          aPageType,
                                          &sAllocPID,
                                          &sPagePtr )
                  != IDE_SUCCESS );
    }

    sPageHdr = sdpPhyPage::getHdr( sPagePtr );

    IDE_ASSERT( sPagePtr != NULL );
    IDE_ASSERT( sPageHdr != NULL );

    /* FIT/ART/sm/Projects/PROJ-1671/freelist-seg/alloc_page.tc */
    IDU_FIT_POINT( "1.PROJ-1671@sdpsfAllocPage::allocNewPage" );

    if( aPageType != SDP_PAGE_DATA )
    {
        /* TABLE �������� �ƴѰ��� UPDATE ONLY�� �����Ѵ�. FREE�����ʴ°��ܿ���
         * �ٽ� Free�������� �ɼ� ����. */
        IDE_TEST( sdpPhyPage::setState( sPageHdr,
                                        (UShort)SDPSF_PAGE_USED_UPDATE_ONLY,
                                        aMtx )
                  != IDE_SUCCESS );

        sNTAData[0] = sdpPhyPage::getPageIDFromPtr( aSegHdr );
        sNTAData[1] = sAllocPID;

        sdrMiniTrans::setNTA( &sAllocMtx,
                              aSpaceID,
                              SDR_OP_SDPSF_ALLOC_PAGE,
                              &sNTA,
                              sNTAData,
                              2 /* DataCount */ );
    }
    else
    {
        IDE_TEST( sdpsfFreePIDList::addPage( &sAllocMtx,
                                             aSegHdr,
                                             sPagePtr )
                  != IDE_SUCCESS );

        /* Initialize Change Transaction Layer */
        IDE_TEST( smLayerCallback::logAndInitCTL( &sAllocMtx,
                                                  sPageHdr,
                                                  aSegHandle->mSegAttr.mInitTrans,
                                                  aSegHandle->mSegAttr.mMaxTrans )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSlotDirectory::logAndInit(sPageHdr,
                                               &sAllocMtx)
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sAllocMtx ) != IDE_SUCCESS );
   
    /* FIT/ART/sm/Projects/PROJ-1671/freelist-seg/alloc_page.tc */
    IDU_FIT_POINT( "2.PROJ-1671@sdpsfAllocPage::allocNewPage" );

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sAllocMtx ) == IDE_SUCCESS );
    }

    *aPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment�� aCountWanted��ŭ�� Free �������� �����ϴ���
 *               Check�Ѵ�. ���� ���ٸ� ���ο� Extent�� �Ҵ��Ѵ�.
 *
 * aStatistics   - [IN] ��� ����
 * aMtx          - [IN] Mini Transaction Pointer
 * aSpaceID      - [IN] TableSpace ID
 * aSegHandle    - [IN] Segment Handle
 * aCountWanted  - [IN] Ȯ���ϱ� ���ϴ� ������ ����
 *
 ***********************************************************************/
IDE_RC sdpsfAllocPage::prepareNewPages( idvSQL            * aStatistics,
                                        sdrMtx            * aMtx,
                                        scSpaceID           aSpaceID,
                                        sdpSegHandle      * aSegHandle,
                                        UInt                aCountWanted )
{
    sdpsfSegHdr      *sSegHdr;
    sdrMtx            sPrepMtx;
    sdrMtxStartInfo   sStartInfo;
    ULong             sAllocPageCnt;
    ULong             sPageCntNotAlloc;
    ULong             sFreePageCnt;
    SInt              sState = 0;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aSegHandle   != NULL );
    IDE_ASSERT( aCountWanted != 0 );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sPrepMtx,
                                   &sStartInfo,
                                   ID_TRUE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                               &sPrepMtx,
                                               aSpaceID,
                                               aSegHandle->mSegPID,
                                               &sSegHdr )
              != IDE_SUCCESS );

    while( 1 )
    {
        sAllocPageCnt = sdpsfSH::getFmtPageCnt( sSegHdr );

        /* HWM���Ŀ� �����ϴ� ���������� ��� */
        sPageCntNotAlloc = sdpsfSH::getTotalPageCnt( sSegHdr ) - sAllocPageCnt;

        sFreePageCnt = sdpsfSH::getFreePageCnt( sSegHdr ) + sPageCntNotAlloc;

        /* Free�������� �����ϸ� ���ο� Extent�� TBS�� ���� �䱸�Ѵ�. */
        if( sFreePageCnt < aCountWanted )
        {
            if( sdpsfExtMgr::allocExt( aStatistics,
                                       &sStartInfo,
                                       aSpaceID,
                                       sSegHdr )
                != IDE_SUCCESS )
            {
                /* TBS FULL */
                sState = 0;
                IDE_TEST( sdrMiniTrans::commit( &sPrepMtx ) != IDE_SUCCESS );
                IDE_TEST( 1 );
            }
            else
            {
                /* nothing to do ... */
            }
        }
        else
        {
            break;
        }
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sPrepMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sPrepMtx )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ �� �������� �Ҵ��Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHandle   - [IN] Segment Handle
 * aPageType    - [IN] Page Type
 *
 * aPagePtr     - [OUT] Free������ ���� Page�� ���� Pointer�� �����ȴ�.
 *                      return �� �ش� �������� XLatch�� �ɷ��ִ�.
 *
 ***********************************************************************/
IDE_RC sdpsfAllocPage::allocPage( idvSQL        *aStatistics,
                                  sdrMtx        *aMtx,
                                  scSpaceID      aSpaceID,
                                  sdpSegHandle * aSegHandle,
                                  sdpPageType    aPageType,
                                  UChar        **aPagePtr )
{
    sdpsfSegHdr  *sSegHdr;
    SInt          sState = 0;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aPageType   < SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aPagePtr    != NULL );

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                               aSpaceID,
                                               aSegHandle->mSegPID,
                                               &sSegHdr )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( allocNewPage( aStatistics,
                            aMtx,
                            aSpaceID,
                            aSegHandle,
                            sSegHdr,
                            aPageType,
                            aPagePtr )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdpsfSH::releaseSegHdr( aStatistics,
                                      sSegHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdpsfSH::releaseSegHdr( aStatistics,
                                            sSegHdr )
                    == IDE_SUCCESS );
    }

    *aPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aPagePtr�� ����Ű�� �������� ������ �� �������� �Ǿ
 *               UFmtPageList�� �߰��Ѵ�. �̶� �̹� �������� �ٸ� PageList��
 *               ���� �ִٸ� �׳� �д�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHandle   - [IN] Segment Handle
 * aPagePtr     - [IN] Free�� Page Pointer
 *
 ***********************************************************************/
IDE_RC sdpsfAllocPage::freePage( idvSQL            * aStatistics,
                                 sdrMtx            * aMtx,
                                 scSpaceID           aSpaceID,
                                 sdpSegHandle      * aSegHandle,
                                 UChar             * aPagePtr )
{
    sdpsfSegHdr  *sSegHdr;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aPagePtr    != NULL );

    /* Page�� �̹� Ư�� Free PID List�� �����ִ��� Check�Ѵ�. */
    if( ((sdpPhyPageHdr*)aPagePtr)->mLinkState == SDP_PAGE_LIST_UNLINK )
    {
        IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                                   aMtx,
                                                   aSpaceID,
                                                   aSegHandle->mSegPID,
                                                   &sSegHdr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpsfUFmtPIDList::add2Head( aMtx,
                                              sSegHdr,
                                              aPagePtr )
                  != IDE_SUCCESS );

        /* FIT/ART/sm/Projects/PROJ-1671/freelist-seg/free_page.tc */
        IDU_FIT_POINT( "1.PROJ-1671@sdpsfAllocPage::freePage" );

        /* BUG-32942 When executing rebuild Index stat, abnormally shutdown */
        IDE_TEST( sdpPhyPage::setState( (sdpPhyPageHdr*)aPagePtr,
                                        (UShort)SDPSF_PAGE_FREE,
                                        aMtx )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �������� Free�� ���θ� ��ȯ�Ѵ�.
 *
 * BUG-32942 When executing rebuild Index stat, abnormally shutdown
 *
 * aPagePtr     - [IN] Page Pointer
 *
 ***********************************************************************/
idBool sdpsfAllocPage::isFreePage( UChar * aPagePtr )
{
    idBool sRet = ID_FALSE;

    if( ((sdpPhyPageHdr*)aPagePtr)->mPageState == SDPSF_PAGE_FREE )
    {
        sRet = ID_TRUE;
    }

    return sRet;
}

/***********************************************************************
 * Description : aPagePtr�� Free PID List�� �߰��Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegRID      - [IN] Segment RID
 * aPagePtr     - [IN] Free�� Page Pointer
 *
 ***********************************************************************/
IDE_RC sdpsfAllocPage::addPageToFreeList( idvSQL          * aStatistics,
                                          sdrMtx          * aMtx,
                                          scSpaceID         aSpaceID,
                                          scPageID          aSegPID,
                                          UChar           * aPagePtr )
{
    sdpsfSegHdr      *sSegHdr;
    sdrMtxStartInfo   sStartInfo;
    sdrMtx            sAddMtx;
    SInt              sState = 0;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aSegPID     != SD_NULL_PID );
    IDE_ASSERT( aPagePtr    != NULL );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sAddMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                               aMtx,
                                               aSpaceID,
                                               aSegPID,
                                               &sSegHdr )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfFreePIDList::addPage( aMtx,
                                         sSegHdr,
                                         aPagePtr )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sAddMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sAddMtx) == IDE_SUCCESS );
    }

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
 * aStatistics          - [IN] ��� ����
 * aMtx                 - [IN] Mini Transaction Pointer
 * aSpaceID             - [IN] TableSpace ID
 * aSegHandle           - [IN] Segment Handle
 * aPrvAllocExtRID      - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aFstPIDOfPrvAllocExt - [IN] ������ �Ҵ�� Extent�� ù��° PID
 * aPrvAllocPageID      - [IN] ������ �Ҵ���� PageID
 *
 * aAllocExtRID      - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aFstPIDOfAllocExt - [OUT] �Ҵ���� Page�� �ִ� Extent�� ù��° ������ ID
 * aAllocPID         - [OUT] ���Ӱ� �Ҵ���� PageID
 * aAllocPagePtr     - [OUT] �Ҵ�� �������� ���� Pointer, ���ϵɶ� XLatch��
 *                           �ɷ��ִ�.
 ***********************************************************************/
IDE_RC sdpsfAllocPage::allocNewPage4Append( idvSQL               *aStatistics,
                                            sdrMtx               *aMtx,
                                            scSpaceID             aSpaceID,
                                            sdpSegHandle         *aSegHandle,
                                            sdRID                 aPrvAllocExtRID,
                                            scPageID              aFstPIDOfPrvAllocExt,
                                            scPageID              aPrvAllocPageID,
                                            idBool                aIsLogging,
                                            sdpPageType           aPageType,
                                            sdRID                *aAllocExtRID,
                                            scPageID             *aFstPIDOfAllocExt,
                                            scPageID             *aAllocPID,
                                            UChar               **aAllocPagePtr )
{
    sdpsfSegHdr    *sSegHdr;
    UChar          *sAllocPagePtr;
    SInt            sState = 0;
    sdrMtx          sMtx;
    sdrMtxStartInfo sStartInfo;

    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aSpaceID          != 0 );
    IDE_ASSERT( aSegHandle        != NULL );
    IDE_ASSERT( aPageType          < SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aAllocExtRID      != NULL );
    IDE_ASSERT( aFstPIDOfAllocExt != NULL );
    IDE_ASSERT( aAllocPID         != NULL );
    IDE_ASSERT( aAllocPagePtr     != NULL );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                               &sMtx,
                                               aSpaceID,
                                               aSegHandle->mSegPID,
                                               &sSegHdr )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfExtMgr::allocNewPage( aStatistics,
                                         &sMtx,
                                         aSpaceID,
                                         sSegHdr,
                                         aSegHandle,
                                         1, /* Next Ext Cnt */
                                         aPrvAllocExtRID,
                                         aFstPIDOfPrvAllocExt,
                                         aPrvAllocPageID,
                                         aAllocExtRID,
                                         aFstPIDOfAllocExt,
                                         aAllocPID,
                                         ID_TRUE )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::create4DPath( aStatistics,
                                        sdrMiniTrans::getTrans( aMtx ),
                                        aSpaceID,
                                        *aAllocPID,
                                        NULL, /* Parent Info */
                                        SDPSF_PAGE_USED_UPDATE_ONLY,
                                        aPageType,
                                        ((sdpSegCCache*)aSegHandle->mCache)
                                            ->mTableOID,
                                        aIsLogging,
                                        &sAllocPagePtr )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::logAndInitCTL( aMtx,
                                              (sdpPhyPageHdr*)sAllocPagePtr,
                                              aSegHandle->mSegAttr.mInitTrans,
                                              aSegHandle->mSegAttr.mMaxTrans )
              != IDE_SUCCESS );

    IDE_TEST( sdpSlotDirectory::logAndInit((sdpPhyPageHdr*)sAllocPagePtr,
                                           aMtx)
              != IDE_SUCCESS );

    *aAllocPagePtr = sAllocPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    *aAllocExtRID  = SD_NULL_RID;
    *aAllocPID     = SD_NULL_PID;
    *aAllocPagePtr = NULL;

    return IDE_FAILURE;
}
