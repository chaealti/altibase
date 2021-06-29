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
# include <smErrorCode.h>
# include <sdr.h>
# include <sdp.h>
# include <sdpReq.h>

# include <sdpsfDef.h>
# include <sdpsfExtDirPageList.h>
# include <sdpsfExtDirPage.h>
# include <sdpsfExtMgr.h>
# include <sdpsfSH.h>

/***********************************************************************
 * Description : Segment�� Cache������ �Ҵ��ϰ� �ʱ�ȭ �Ѵ�.
 *
 * aSegCache     - [IN] Segment�� Cache������ �Ѿ�´�.
 ***********************************************************************/
IDE_RC sdpsfSH::initialize( sdpSegHandle * aSegHandle,
                            scSpaceID    /*aSpaceID*/,
                            sdpSegType     aSegType,
                            smOID          aTableOID,
                            UInt           aIndexID )
{
    sdpsfSegCache * sSegCache;
    UInt            sState  = 0;

    IDE_ASSERT( aSegHandle != NULL );

    /* sdpsfSH_initialize_malloc_SegCache.tc */
    IDU_FIT_POINT("sdpsfSH::initialize::malloc::SegCache");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDP,
                                 ID_SIZEOF( sdpsfSegCache ),
                                 (void **)&sSegCache,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );
    sState  = 1;

    IDE_TEST( sdpSegment::initCommonCache( &( sSegCache->mCommon ),
                                           aSegType, 
                                           0 /* aPageCntInExt */, 
                                           aTableOID,
                                           aIndexID )
              != IDE_SUCCESS );

    aSegHandle->mCache = sSegCache;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free( sSegCache ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Cache������ Free�Ѵ�.
 *
 * aSegCache      - [IN] Segment�� Cache Pointer�� �Ѿ�´�.
 ***********************************************************************/
IDE_RC sdpsfSH::destroy( sdpSegHandle * aSegHandle)
{
    IDE_ASSERT( aSegHandle->mCache != NULL );

    IDE_TEST( iduMemMgr::free( aSegHandle->mCache ) != IDE_SUCCESS );
    aSegHandle->mCache = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Desc�� �ʱ�ȭ�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] SpaceID
 * aSegHdr        - [IN] Segment�� Cache Pointer�� �Ѿ�´�.
 * aSegPageID     - [IN] Segment Header PageID
 * aHWM           - [IN] HWM ���� �ѹ��̶� �Ҵ�� �������� ���� �� ������ID
 * aPageCntInExt  - [IN] Extent���� ������ ����
 * aType          - [IN] Segment Type
 ***********************************************************************/
IDE_RC sdpsfSH::init( idvSQL           * aStatistics,
                      sdrMtx           * aMtx,
                      scSpaceID          aSpaceID,
                      sdpsfSegHdr      * aSegHdr,
                      scPageID           aSegPageID,
                      scPageID           aHWM,
                      UInt               aPageCntInExt,
                      sdpSegType         aType )
{
    UInt            i;
    sdRID           sFstExtRID;
    sdpsfExtDesc  * sExtDesc;
    UInt            sFlag;

    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aSpaceID      != 0 );
    IDE_ASSERT( aSegHdr       != NULL );
    IDE_ASSERT( aSegPageID    != SD_NULL_PID );
    IDE_ASSERT( aHWM          != SD_NULL_PID );
    IDE_ASSERT( aPageCntInExt != 0 );
    IDE_ASSERT( aType         <  SDP_SEG_TYPE_MAX );

    IDE_ASSERT( aMtx != NULL );

    IDE_TEST( setSegHdrPID( aMtx, aSegHdr, aSegPageID )
              != IDE_SUCCESS );

    IDE_TEST( setType( aMtx, aSegHdr, aType )
              != IDE_SUCCESS );

    IDE_TEST( setState( aMtx, aSegHdr, SDP_SEG_USE )
              != IDE_SUCCESS );

    IDE_TEST( setPageCntInExt( aMtx,
                               aSegHdr,
                               aPageCntInExt )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfSH::setMaxExtCntInSegHdrPage(
                  aMtx,
                  aSegHdr,
                  sdpsfExtDirPage::getMaxExtDescCntInSegHdrPage() )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfSH::setMaxExtCntInExtDirPage(
                  aMtx,
                  aSegHdr,
                  sdpsfExtDirPage::getMaxExtDescCntInExtDirPage() )
              != IDE_SUCCESS );

    IDE_TEST( sdpSglPIDList::initList( getPvtFreePIDList( aSegHdr ),
                                       aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdpSglPIDList::initList( getFreePIDList( aSegHdr ),
                                       aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdpSglPIDList::initList( getUFmtPIDList( aSegHdr ),
                                       aMtx )
              != IDE_SUCCESS );

    /* Meta Page ID�� �ʱ�ȭ �Ѵ�. */
    for( i = 0; i < SDP_MAX_SEG_PID_CNT; i ++ )
    {
        IDE_TEST( setMetaPID( aMtx,
                              aSegHdr,
                              i,
                              SD_NULL_PID )
                  != IDE_SUCCESS );
    }

    /* �ѹ��̶� �Ҵ�� �������� ���� ���� */
    IDE_TEST( setFmtPageCnt( aMtx, aSegHdr, 1 )
              != IDE_SUCCESS );

    IDE_TEST( setHWM( aMtx, aSegHdr, aHWM )
              != IDE_SUCCESS );

    /* Extent Directory Page List �ʱ�ȭ */
    IDE_TEST( sdpsfExtDirPageList::initialize( aMtx,
                                               aSegHdr )
              != IDE_SUCCESS );

    /* Segment�� ������ �ִ� Total Extent ���� ���� */
    IDE_TEST( setTotExtCnt( aMtx, aSegHdr, 1 ) != IDE_SUCCESS );

    /* Extent Directory Page List �ʱ�ȭ */
    IDE_TEST( sdpsfExtDirPage::initialize( aMtx,
                                           &aSegHdr->mExtDirCntlHdr,
                                           aSegHdr->mMaxExtCntInSegHdrPage )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfExtDirPageList::addPage2Tail( aStatistics,
                                                 aMtx,
                                                 aSegHdr,
                                                 &aSegHdr->mExtDirCntlHdr )
              != IDE_SUCCESS );

    sFlag = SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_TRUE;

    /* Extent Desc�߰� */
    IDE_TEST( sdpsfExtDirPage::addNewExtDescAtLst( aMtx,
                                                   &aSegHdr->mExtDirCntlHdr,
                                                   aSegPageID,
                                                   sFlag,
                                                   &sExtDesc )
              != IDE_SUCCESS );

    sFstExtRID = sdpPhyPage::getRIDFromPtr( sExtDesc );

    /* ���� Alloc�� ���ؼ� ������� Ext RID���� */
    IDE_TEST( setAllocExtRID( aMtx,
                              aSegHdr,
                              sFstExtRID )
              != IDE_SUCCESS );

    IDE_TEST( setFstPIDOfAllocExt( aMtx,
                                   aSegHdr,
                                   aSegPageID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� Desc�� �ִ� �������� ���� XLatch�� ���
 *               Segment�� Desc�� �Ѱ��ش�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] TableSpace ID
 * aSegPID        - [IN] Segment RID
 * aMtx           - [IN] Mini Transaction Pointer
 *
 * aSegHdr        - [OUT] Segment Descriptor�� �������� XLatch�� ���
 *                        Segment Descriptor ���� �����͸� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpsfSH::fixAndGetSegHdr4Update( idvSQL        * aStatistics,
                                        sdrMtx        * aMtx,
                                        scSpaceID       aSpaceID,
                                        scPageID        aSegPID,
                                        sdpsfSegHdr  ** aSegHdr )
{
    UChar *sSegPagePtr;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegPID  != SD_NULL_PID );
    IDE_ASSERT( aSegHdr  != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          (UChar**)&sSegPagePtr,
                                          NULL /*aTrySuccess*/,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    *aSegHdr = (sdpsfSegHdr*)( sSegPagePtr + SDPSF_SEGHDR_OFFSET );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aSegHdr = NULL;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Segment�� Desc�� �ִ� �������� ���� XLatch�� ���
 *               Segment�� Desc�� �Ѱ��ش�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] TableSpace ID
 * aSegRID        - [IN] Segment RID
 *
 * aSegHdr        - [OUT] Segment Descriptor�� �������� XLatch�� ���
 *                        Segment Descriptor ���� �����͸� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpsfSH::fixAndGetSegHdr4Update( idvSQL        * aStatistics,
                                        scSpaceID       aSpaceID,
                                        scPageID        aSegPID,
                                        sdpsfSegHdr  ** aSegHdr )
{
    idBool  sIsSuccess;
    UChar  *sSegPagePtr;

    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegPID  != SD_NULL_PID );
    IDE_ASSERT( aSegHdr  != NULL );

    IDE_TEST( sdbBufferMgr::getPage( aStatistics,
                                     aSpaceID,
                                     aSegPID,
                                     SDB_X_LATCH,
                                     SDB_WAIT_NORMAL,
                                     SDB_SINGLE_PAGE_READ,
                                     (UChar**)&sSegPagePtr,
                                     &sIsSuccess )
              != IDE_SUCCESS );

    IDE_ASSERT( sIsSuccess == ID_TRUE );

    *aSegHdr = (sdpsfSegHdr*)( sSegPagePtr + SDPSF_SEGHDR_OFFSET );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aSegHdr = NULL;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : Segment�� Desc�� �ִ� �������� ���� SLatch�� ���
 *               Segment�� Desc�� �Ѱ��ش�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] TableSpace ID
 * aSegRID        - [IN] Segment RID
 *
 * aSegHdr        - [OUT] Segment Descriptor�� �������� SLatch�� ���
 *                        Segment Descriptor ���� �����͸� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpsfSH::fixAndGetSegHdr4Read( idvSQL        * aStatistics,
                                      scSpaceID       aSpaceID,
                                      scPageID        aSegPID,
                                      sdpsfSegHdr  ** aSegHdr )
{
    UChar  *sSegPagePtr;

    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegPID  != SD_NULL_PID );
    IDE_ASSERT( aSegHdr  != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* aMtx */
                                          (UChar**)&sSegPagePtr,
                                          NULL /*aTrySuccess*/,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    *aSegHdr = (sdpsfSegHdr*)( sSegPagePtr + SDPSF_SEGHDR_OFFSET );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aSegHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aSegHdr�� �����ִ� �������� release�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aSegHdr        - [IN] Segment Desc
 ***********************************************************************/
IDE_RC sdpsfSH::releaseSegHdr( idvSQL        * aStatistics,
                               sdpsfSegHdr   * aSegHdr )
{
    UChar *sPagePtr = sdpPhyPage::getPageStartPtr( (UChar*)aSegHdr );

    IDE_ASSERT( aSegHdr  != NULL );

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : FmtPageCnt�� ����ش�.
 *
 * aStatistics  - [IN] ��� ����
 * aSpaceID     - [IN] TableSpace ID
 * aSegHandle   - [IN] Segment Handle
 *
 * aSegCache    - [OUT] SegInfo�� �����ȴ�.
 *
 ***********************************************************************/
IDE_RC sdpsfSH::getFmtPageCnt( idvSQL        *aStatistics,
                               scSpaceID      aSpaceID,
                               sdpSegHandle  *aSegHandle,
                               ULong         *aFmtPageCnt )
{
    sdpSegInfo  sSegInfo;

    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aFmtPageCnt != NULL );

    IDE_TEST( getSegInfo( aStatistics,
                          aSpaceID,
                          aSegHandle->mSegPID,
                          NULL, /* aTableHeader */
                          &sSegInfo ) != IDE_SUCCESS );

    *aFmtPageCnt = sSegInfo.mFmtPageCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : SegInfo�� ����ش�.
 *
 * aStatistics  - [IN] ��� ����
 * aSpaceID     - [IN] TableSpace ID
 * aSegRID      - [IN] Segment RID
 *
 * aSegCache    - [OUT] SegInfo�� �����ȴ�.
 *
 ***********************************************************************/
IDE_RC sdpsfSH::getSegInfo( idvSQL       * aStatistics,
                            scSpaceID      aSpaceID,
                            scPageID       aSegPID,
                            void         * /*aTableHeader*/,
                            sdpSegInfo   * aSegInfo )
{
    sdpsfSegHdr   *sSegHdr;
    SInt           sState = 0;
    UChar         *sSegPagePtr = NULL;

    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegPID  != SD_NULL_PID );
    IDE_ASSERT( aSegInfo != NULL );

    /* SegHdr�� �б� ���ؼ� SLatch�� �Ǵ�. */
    IDE_TEST( sdpsfSH::fixAndGetSegHdr4Read( aStatistics,
                                             aSpaceID,
                                             aSegPID,
                                             &sSegHdr )
              != IDE_SUCCESS );
    sState = 1;

    sSegPagePtr = sdpPhyPage::getPageStartPtr( (UChar*)sSegHdr );

    aSegInfo->mSegHdrPID      = aSegPID;
    aSegInfo->mType           = (sdpSegType)(sSegHdr->mType);
    aSegInfo->mState          = (sdpSegState)(sSegHdr->mState);

    aSegInfo->mPageCntInExt   = sSegHdr->mPageCntInExt;
    aSegInfo->mFmtPageCnt     = sSegHdr->mFmtPageCnt;
    aSegInfo->mExtCnt         = sdpsfExtMgr::getExtCnt( sSegHdr );
    aSegInfo->mFstExtRID      = sdpsfExtMgr::getFstExt( sSegHdr );
    aSegInfo->mLstExtRID      = sdpsfExtMgr::getLstExt( sSegHdr );
    aSegInfo->mLstAllocExtRID = sSegHdr->mAllocExtRID;
    aSegInfo->mHWMPID         = sSegHdr->mHWMPID;
    aSegInfo->mExtRIDHWM      = aSegInfo->mLstExtRID;

    aSegInfo->mFstPIDOfLstAllocExt = sSegHdr->mFstPIDOfAllocExt;

    IDE_ASSERT( aSegInfo->mLstAllocExtRID == aSegInfo->mLstExtRID );

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

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : SegCacheInfo�� ����ش�.
 *
 * aStatistics   - [IN] ��� ����
 * aSegHandle    - [IN] Segment Handle
 *
 * aSegCacheInfo - [OUT] SegCacheInfo�� �����ȴ�.
 *
 ***********************************************************************/
IDE_RC sdpsfSH::getSegCacheInfo( idvSQL          * /*aStatistics*/,
                                 sdpSegHandle    * /*aSegHandle*/,
                                 sdpSegCacheInfo * aSegCacheInfo )
{
    IDE_ASSERT( aSegCacheInfo != NULL );

    aSegCacheInfo->mUseLstAllocPageHint = ID_FALSE;
    aSegCacheInfo->mLstAllocPID         = SD_NULL_PID;
    aSegCacheInfo->mLstAllocSeqNo       = 0;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Segment�� ���¸� ���Ѵ�. sdnbModule.cpp�� create���� ���
 *               �Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aSpaceID     - [IN] TableSpace ID
 * aSegPID      - [IN] Segment PID
 *
 * aSegState    - [OUT] Segment�� ���¸� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdpsfSH::getSegState( idvSQL        *aStatistics,
                             scSpaceID      aSpaceID,
                             scPageID       aSegPID,
                             sdpSegState   *aSegState )
{
    sdpsfSegHdr  *sSegHdr;
    sdpSegState   sSegState;

    IDE_ASSERT( aSpaceID  != 0 );
    IDE_ASSERT( aSegPID   != SD_NULL_PID );
    IDE_ASSERT( aSegState != NULL );

    IDE_TEST( fixAndGetSegHdr4Read( aStatistics,
                                    aSpaceID,
                                    aSegPID,
                                    &sSegHdr )
              != IDE_SUCCESS );

    sSegState = getState( sSegHdr );

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sdpPhyPage::getPageStartPtr( sSegHdr)  )
              != IDE_SUCCESS );

    *aSegState = sSegState;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header�� Meta PID�� �����Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegPID      - [IN] Segment PID
 * aIndex       - [IN] Segment Header�� ���° Meta PID�� ������ ���ΰ�?
 * aMetaPID     - [IN] Meta PageID
 ************************************************************************/
IDE_RC sdpsfSH::setMetaPID( idvSQL        *aStatistics,
                            sdrMtx        *aMtx,
                            scSpaceID      aSpaceID,
                            scPageID       aSegPID,
                            UInt           aIndex,
                            scPageID       aMetaPID )
{
    sdpsfSegHdr  *sSegHdr;

    IDE_ASSERT( aMtx      != NULL );
    IDE_ASSERT( aSpaceID  != 0 );
    IDE_ASSERT( aSegPID   != SD_NULL_PID );
    IDE_ASSERT( aIndex    <  SDP_MAX_SEG_PID_CNT );

    IDE_TEST( fixAndGetSegHdr4Update( aStatistics,
                                      aMtx,
                                      aSpaceID,
                                      aSegPID,
                                      &sSegHdr )
              != IDE_SUCCESS );

    IDE_TEST( setMetaPID( aMtx,
                          sSegHdr,
                          aIndex,
                          aMetaPID ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aSegPID�� ����Ű�� SegHdr���� aIndex���� Meta PID�� ��
 *               �Ѵ�.
 *
 * aStatistics - [IN] ��� ����
 * aSpaceID    - [IN] SpaceID
 * aSegPID     - [IN] Segment PID
 * aIndex      - [IN] Meta PID Array Index
 * aMetaPID    - [OUT] Meta PID�� ��´�.
 ************************************************************************/
IDE_RC sdpsfSH::getMetaPID( idvSQL        *aStatistics,
                            scSpaceID      aSpaceID,
                            scPageID       aSegPID,
                            UInt           aIndex,
                            scPageID      *aMetaPID )
{
    sdpsfSegHdr  *sSegHdr;
    scPageID      sMetaPID;

    IDE_ASSERT( aSpaceID  != 0 );
    IDE_ASSERT( aSegPID   != SD_NULL_PID );
    IDE_ASSERT( aIndex    <  SDP_MAX_SEG_PID_CNT );
    IDE_ASSERT( aMetaPID  !=  NULL );

    IDE_TEST( fixAndGetSegHdr4Read( aStatistics,
                                    aSpaceID,
                                    aSegPID,
                                    &sSegHdr )
              != IDE_SUCCESS );

    sMetaPID = getMetaPID( sSegHdr, aIndex );

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sdpPhyPage::getPageStartPtr( sSegHdr)  )
              != IDE_SUCCESS );

    *aMetaPID = sMetaPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aMetaPID = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : HWM�� �����Ѵ�.
 *
 * aStatistics           - [IN] ��� ����
 * aStartInfo            - [IN] Mini Transaction Start Info
 * aSpaceID              - [IN] TableSpace ID
 * aSegHandle            - [IN] Segment Handle
 * aLstAllocExtRID       - [IN] �ѹ��� ������ ���� �������� ���� ����������
 *                              �Ҵ�� Extent RID, �� HWM�� ����Ű�� ��������
 *                              ���� Extent RID
 * aFstPIDOfLstAllocExt  - [IN] LstAllocExt�� ù��° PID
 * aLstAllocPID          - [IN] �ѹ��� ������ ���� �������� ���� ����������
 *                              �Ҵ�� PID, �� ���ο� HWM
 * aAllocPageCnt         - [IN] �Ҵ��� ������ ����
 ***********************************************************************/
IDE_RC sdpsfSH::updateHWMInfo4DPath( idvSQL           *aStatistics,
                                     sdrMtxStartInfo  *aStartInfo,
                                     scSpaceID         aSpaceID,
                                     sdpSegHandle     *aSegHandle,
                                     scPageID          /*aFstAllocPID*/,
                                     sdRID             aLstAllocExtRID,
                                     scPageID          aFstPIDOfLstAllocExt,
                                     scPageID          aLstAllocPID,
                                     ULong             aAllocPageCnt,
                                     idBool            /*aMergeMultiSeg */ )
{
    sdrMtx              sMtx;
    sdpsfSegHdr        *sSegHdr;
    SInt                sState = 0;
    smLSN               sNTA;
    ULong               sNTAData[5];
    scPageID            sHWM;

    IDE_ASSERT( aStartInfo           != NULL );
    IDE_ASSERT( aSpaceID             != 0 );
    IDE_ASSERT( aSegHandle           != NULL );
    IDE_ASSERT( aLstAllocExtRID      != SD_NULL_RID );
    IDE_ASSERT( aFstPIDOfLstAllocExt != SD_NULL_PID );
    IDE_ASSERT( aLstAllocPID         != SD_NULL_PID );
    IDE_ASSERT( aAllocPageCnt        != 0 );
    // BUG-27329 CodeSonar::Uninitialized Variable (2)
    IDE_ASSERT( aStartInfo->mTrans   != NULL );

    sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
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

    sHWM = aLstAllocPID;

    sNTAData[0] = aSegHandle->mSegPID;
    sNTAData[1] = aLstAllocExtRID;
    sNTAData[2] = aFstPIDOfLstAllocExt;
    sNTAData[3] = sHWM;
    sNTAData[4] = sdpsfSH::getFmtPageCnt( sSegHdr );

    IDE_TEST( setAllocExtRID( &sMtx, sSegHdr, aLstAllocExtRID )
              != IDE_SUCCESS );

    IDE_TEST( setFstPIDOfAllocExt( &sMtx, sSegHdr, aFstPIDOfLstAllocExt )
              != IDE_SUCCESS );

    IDE_TEST( setHWM( &sMtx, sSegHdr, sHWM )
              != IDE_SUCCESS );

    IDE_TEST( setFmtPageCnt( &sMtx, sSegHdr, aAllocPageCnt )
              != IDE_SUCCESS );

    /* Merge�� Rollback�ϴ� Logical Undo Log�� ����Ѵ�. */
    sdrMiniTrans::setNTA( &sMtx,
                          aSpaceID,
                          SDR_OP_SDPSF_UPDATE_HWMINFO_4DPATH,
                          &sNTA,
                          sNTAData,
                          5 ); // DataCount

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC sdpsfSH::reformatPage4DPath( idvSQL           * /*aStatistics*/,
                                    sdrMtxStartInfo  * /*aStartInfo*/,
                                    scSpaceID          /*aSpaceID*/,
                                    sdpSegHandle     * /*aSegHandle*/,
                                    sdRID              /*aLstAllocExtRID*/,
                                    scPageID           /*aLstPID*/ )
{
    /* FMS������ ���ʿ��� �Լ��̴�. */
    return IDE_SUCCESS;
}

IDE_RC sdpsfSH::setLstAllocPage( idvSQL         * /*aStatistics*/,
                                 sdpSegHandle   * /*aSegHandle*/,
                                 scPageID         /*aLstAllocPID*/,
                                 ULong            /*aLstAllocSeqNo*/ )
{
    /* TMS�� �ƴϸ� �ƹ��͵� ���Ѵ�. */
    return IDE_SUCCESS;
}
