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
 * $Id: sdpstDPath.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Direct-Path Insert ���� STATIC
 * �������̽��� �����Ѵ�. 
 *
 ***********************************************************************/

# include <sdpReq.h>
# include <sdpPhyPage.h>

# include <sdpstBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstItBMP.h>
# include <sdpstRtBMP.h>

# include <sdpstFindPage.h>
# include <sdpstAllocPage.h>

# include <sdpstSH.h>
# include <sdpstStackMgr.h>
# include <sdpstCache.h>
# include <sdpstFreePage.h>
# include <sdpstSegDDL.h>
# include <sdpstExtDir.h>
# include <sdpstDPath.h>
# include <sdpstCache.h>

/***********************************************************************
 * Description : [ INTERFACE ] DIRECT-PATH INSERT�� ���� Append �����
 *               Page �Ҵ� ����
 *
 * aStatistics      - [IN] ��� ����
 * aMtx             - [IN] Mini Transaction Pointer
 * aSpaceID         - [IN] TableSpace ID
 * aSegHandle       - [IN] Segment Handle
 * aPrvAllocExtRID  - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aPrvAllocPageID  - [IN] ������ �Ҵ���� PageID
 * aAllocExtRID     - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aAllocPID        - [OUT] ���Ӱ� �Ҵ���� PageID
 ***********************************************************************/
IDE_RC sdpstDPath::allocNewPage4Append(
                            idvSQL              * aStatistics,
                            sdrMtx              * aMtx,
                            scSpaceID             aSpaceID,
                            sdpSegHandle        * aSegHandle,
                            sdRID                 aPrvAllocExtRID,
                            scPageID              /*aFstPIDOfPrvAllocExt*/,
                            scPageID              aPrvAllocPageID,
                            idBool                aIsLogging,
                            sdpPageType           aPageType,
                            sdRID               * aAllocExtRID,
                            scPageID            * /*aFstPIDOfAllocExt*/,
                            scPageID            * aAllocPID,
                            UChar              ** aAllocPagePtr )
{
    UChar          * sAllocPagePtr;
    sdpParentInfo    sParentInfo;
    ULong            sSeqNo;
    sdpstPBS         sPBS = (sdpstPBS)( SDPST_BITSET_PAGETP_DATA |
                                        SDPST_BITSET_PAGEFN_FUL );

    IDE_DASSERT( aSegHandle != NULL );
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( aAllocExtRID != NULL );
    IDE_DASSERT( aAllocPID != NULL );
    IDE_DASSERT( aAllocPagePtr != NULL );

    IDE_TEST( sdpstExtDir::allocNewPageInExt( aStatistics,
                                              aMtx,
                                              aSpaceID,
                                              aSegHandle,
                                              aPrvAllocExtRID,
                                              aPrvAllocPageID,
                                              aAllocExtRID,
                                              aAllocPID,
                                              &sParentInfo ) != IDE_SUCCESS );

    IDE_TEST( sdpstExtDir::makeSeqNo( aStatistics,
                                      aSpaceID,
                                      aSegHandle->mSegPID,
                                      *aAllocExtRID,
                                      *aAllocPID,
                                      &sSeqNo ) != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::create4DPath( aStatistics,
                                        sdrMiniTrans::getTrans( aMtx ),
                                        aSpaceID,
                                        *aAllocPID,
                                        &sParentInfo,
                                        (UShort)sPBS,
                                        aPageType,
                                        ((sdpSegCCache*)aSegHandle->mCache)
                                            ->mTableOID,
                                        aIsLogging,
                                        &sAllocPagePtr )
              != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::setSeqNo( sdpPhyPage::getHdr( sAllocPagePtr ),
                                    sSeqNo,
                                    aMtx ) != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::logAndInitCTL( aMtx,
                                              (sdpPhyPageHdr *)sAllocPagePtr,
                                              aSegHandle->mSegAttr.mInitTrans,
                                              aSegHandle->mSegAttr.mMaxTrans )
              != IDE_SUCCESS );

    IDE_TEST( sdpSlotDirectory::logAndInit((sdpPhyPageHdr *) sAllocPagePtr,
                                           aMtx)
                != IDE_SUCCESS );

    *aAllocPagePtr = sAllocPagePtr;

    /*
      ideLog::log( IDE_SERVER_0, "\t [ alloc New append ] %u\n",
                 sdpPhyPage::getPageIDFromPtr(sAllocPagePtr ));
     */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aAllocPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct-Path ���꿡�� Slot���� MFNL�� FULL�� �Ѳ�����
 *               �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstDPath::updateMFNLToFull4DPath( idvSQL             * aStatistics,
                                           sdrMtxStartInfo    * aStartInfo,
                                           sdpstBMPHdr        * aBMPHdr,
                                           SShort               aFmSlotNo,
                                           SShort               aToSlotNo,
                                           sdpstMFNL          * aNewMFNL )
{
    UInt            sState = 0;
    smLSN           sNTA;
    sdrMtx          sMtx;
    ULong           sArrData[5];
    scSpaceID       sSpaceID;
    scPageID        sPageID;
    UChar         * sPagePtr;
    sdpstMFNL       sFmMFNL;
    sdpstMFNL       sMFNLOfLstSlot;
    sdpstMFNL       sNewMFNL;
    UInt            sPageCnt;
    idBool          sNeedToChangeMFNL;

    IDE_ASSERT( aBMPHdr   != NULL );
    IDE_ASSERT( aFmSlotNo <= aToSlotNo );
    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aNewMFNL   != NULL );

    sFmMFNL        = (sdpstBMP::getMapPtr( aBMPHdr ) + aToSlotNo)->mMFNL;
    sMFNLOfLstSlot = *aNewMFNL;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 1;

    sPagePtr = sdpPhyPage::getPageStartPtr( aBMPHdr );
    sSpaceID = sdpPhyPage::getSpaceID( sPagePtr );
    sPageID  = sdpPhyPage::getPageID( sPagePtr );

    IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, sPagePtr )
              != IDE_SUCCESS );

    /* root������ bitmap ���濡 ���� Rollback������ ���ؼ� ���⼭ NTA�� ���� */
    sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

    sArrData[0] = sPageID;
    sArrData[1] = aFmSlotNo;        /* from slotNo */
    sArrData[2] = aToSlotNo;        /* to slotNo */
    sArrData[3] = sFmMFNL;          /* prv MFNL */
    sArrData[4] = sMFNLOfLstSlot;   /* MFNL of Lst Slot */

    sPageCnt = aToSlotNo - aFmSlotNo + 1;

    /* ������ slot�� ������ ������ slot�� MFNL ���� */
    if ( sPageCnt - 1 > 0 )
    {
        IDE_ASSERT( aToSlotNo > aFmSlotNo );

        IDE_TEST( sdpstBMP::logAndUpdateMFNL( &sMtx,
                                              aBMPHdr,
                                              aFmSlotNo,
                                              aToSlotNo - 1,
                                              sFmMFNL,
                                              SDPST_MFNL_FUL,
                                              sPageCnt - 1,
                                              &sNewMFNL,
                                              &sNeedToChangeMFNL )
                  != IDE_SUCCESS );
    }

    /* ������ slot�� ���� MFNL ���� */
    IDE_TEST( sdpstBMP::logAndUpdateMFNL( &sMtx,
                                          aBMPHdr,
                                          aToSlotNo,
                                          aToSlotNo,
                                          sFmMFNL,
                                          sMFNLOfLstSlot,
                                          1,
                                          &sNewMFNL,
                                          &sNeedToChangeMFNL )
              != IDE_SUCCESS );


    IDE_DASSERT( sdpstBMP::verifyBMP( aBMPHdr ) == IDE_SUCCESS );

    sdrMiniTrans::setNTA( &sMtx,
                          sSpaceID,
                          SDR_OP_SDPST_UPDATE_BMP_4DPATH,
                          &sNTA,
                          sArrData,
                          5 );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

IDE_RC sdpstDPath::reformatPage4DPath( idvSQL           *aStatistics,
                                       sdrMtxStartInfo  *aStartInfo,
                                       scSpaceID         aSpaceID,
                                       sdpSegHandle     *aSegHandle,
                                       sdRID             aLstAllocExtRID,
                                       scPageID          aLstPID )
{
    sdpstExtDesc        * sExtDescPtr;
    sdpstExtDesc          sExtDesc;
    sdRID                 sNxtExtRID;
    scPageID              sFstFmtPID;
    scPageID              sLstFmtPID;
    scPageID              sLstFmtExtDirPID;
    SShort                sLstFmtSlotNoInExtDir;
    UInt                  sState = 0;

    IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                          aSpaceID,
                                          aLstAllocExtRID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          NULL, /* sdrMtx */
                                          (UChar**)&sExtDescPtr,
                                          NULL  /* aTrySuccess */ )
              != IDE_SUCCESS );
    sState = 1;

    sExtDesc = *sExtDescPtr;

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sExtDescPtr )
              != IDE_SUCCESS );

    if ( sdpstExtDir::isFreePIDInExt( &sExtDesc, aLstPID ) == ID_FALSE )
    {
        IDE_CONT( skip_reformat_page );
    }

    /* �� Extent ���� ���������� �Ҵ�� �������� �ִٸ�
     * �ش� ������ ���� ���������� reformat �Ѵ�. */
    IDE_TEST( sdpstAllocPage::formatDataPagesInExt(
                                            aStatistics,
                                            aStartInfo,
                                            aSpaceID,
                                            aSegHandle,
                                            &sExtDesc,
                                            aLstAllocExtRID,
                                            aLstPID + 1 ) != IDE_SUCCESS );


    /* ���� Extent�� �����´�. */
    IDE_TEST( sdpstExtDir::getNxtExtRID( aStatistics,
                                         aSpaceID,
                                         aSegHandle->mSegPID,
                                         aLstAllocExtRID,
                                         &sNxtExtRID ) != IDE_SUCCESS );

    /* 
     * reformat�� Extent�� �����ϴ� LfBMP�� ���� �ٸ� Extent ����
     * reformat�Ѵ�.
     * Extent���� �ش� Extent�� �����ϰ� �ִ� LfBMP�� ���� ������
     * mExtMgmtLfBMP�� ����ִ�.
     * Next Extent�� ��� ���󰡸鼭 �� ó�� ���� ExtMgmtLfBMP ���� ������ 
     * mExtMgmtLfBMP�� ���� Extent�� ������ ù��° Extent�� ������ LfBMP��
     * ���Ѵ� �� �� �ֱ� ������ reformat �Ѵ�.
     */
    if ( sNxtExtRID != SD_NULL_RID )
    {
        IDE_TEST( sdpstExtDir::formatPageUntilNxtLfBMP( aStatistics,
                                                        aStartInfo,
                                                        aSpaceID,
                                                        aSegHandle,
                                                        sNxtExtRID,
                                                        &sFstFmtPID,
                                                        &sLstFmtPID,
                                                        &sLstFmtExtDirPID,
                                                        &sLstFmtSlotNoInExtDir )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( skip_reformat_page );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sExtDescPtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : merge�� Segment�� HWM �� MFNL�� �����Ѵ�.
 ***********************************************************************/
IDE_RC  sdpstDPath::updateWMInfo4DPath(
                                      idvSQL           *aStatistics,
                                      sdrMtxStartInfo  *aStartInfo,
                                      scSpaceID         aSpaceID,
                                      sdpSegHandle     *aSegHandle,
                                      scPageID          aFstAllocPID,
                                      sdRID             aLstAllocExtRID,
                                      scPageID        /*aFstPIDOfAllocExtOfFM*/,
                                      scPageID          aLstPID,
                                      ULong           /*aAllocPageCnt*/,
                                      idBool          /*aMergeMultiSeg*/ )
{
    sdrMtx              sMtx;
    SInt                sState = 0;
    scPageID            sSegPID;
    UChar             * sPagePtr;
    sdpstExtDesc      * sLstExtDesc;
    scPageID            sLstPIDInExt;
    scPageID            sLstExtDirPID;
    SShort              sLstSlotNoInExtDir;
    sdpstStack          sNewHWMStack;
    smLSN               sNTA;
    ULong               sNTAData[5];
    sdRID               sExtRID4HWM;
    sdpstSegCache     * sSegCache;
    sdpstSegHdr       * sSegHdr;

    IDE_ASSERT( aSegHandle      != NULL );
    IDE_ASSERT( aFstAllocPID    != SD_NULL_RID );
    IDE_ASSERT( aLstAllocExtRID != SD_NULL_RID );
    IDE_ASSERT( aLstPID         != SD_NULL_PID );
    IDE_ASSERT( aStartInfo      != NULL );

    sSegPID   = aSegHandle->mSegPID;
    sSegCache = (sdpstSegCache *)(aSegHandle->mCache);

    IDE_ASSERT( sSegPID != SD_NULL_PID );


    /* Serial Direct-Path Insert�ÿ��� HWM�� �ű������
     * merge�� ������ bitmap�� �����Ѵ�. */
    IDE_TEST( sdpstDPath::updateBMPUntilHWM( aStatistics,
                                             aSpaceID,
                                             sSegPID,
                                             aFstAllocPID,
                                             aLstPID,
                                             aStartInfo ) != IDE_SUCCESS );

    /* ������ ExtDesc�� ���� ������ PID�� ����Ѵ�. */
    IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                          aSpaceID,
                                          aLstAllocExtRID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          NULL, /* sdrMtx */
                                          (UChar**)&sLstExtDesc,
                                          NULL  /* aTruSuccess */ )
              != IDE_SUCCESS );
    sState = 1;

    sPagePtr = sdpPhyPage::getPageStartPtr( sLstExtDesc );

    sLstPIDInExt       = sLstExtDesc->mExtFstPID + sLstExtDesc->mLength - 1;
    sLstExtDirPID      = sdpPhyPage::getPageID( sPagePtr );
    sLstSlotNoInExtDir = sdpstExtDir::calcOffset2SlotNo(
                                   sdpstExtDir::getHdrPtr( sPagePtr ),
                                   SD_MAKE_OFFSET( aLstAllocExtRID ) );

    /* ���� */
    if ( (sLstExtDirPID != SD_MAKE_PID( aLstAllocExtRID ) ) ||
         (sLstSlotNoInExtDir > sdpstExtDir::getHdrPtr(sPagePtr)->mExtCnt - 1) )
    {
        ideLog::log( IDE_SERVER_0,
                     "LstAllocExtDirPID: %u, "
                     "ExtentCnt: %u, "
                     "ExtFstPID: %u, "
                     "ExtLstPID: %u, "
                     "SlotNoInExtDir: %d, "
                     "LstPID: %u\n",
                     SD_MAKE_PID( aLstAllocExtRID ),
                     sdpstExtDir::getHdrPtr(sPagePtr)->mExtCnt,
                     sLstExtDesc->mExtFstPID,
                     sLstPIDInExt,
                     aLstPID );
        sdpstExtDir::dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sLstExtDesc )
              != IDE_SUCCESS );

    /* ������ ��ġ�� ���� Stack�� �����Ѵ�. */
    sdpstStackMgr::initialize( &sNewHWMStack );
    IDE_TEST( sdpstAllocPage::makeOrderedStackFromDataPage(
                                         aStatistics,
                                         aSpaceID,
                                         sSegPID,
                                         sLstPIDInExt,
                                         &sNewHWMStack ) != IDE_SUCCESS );

    sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

    /* ������ �Ҵ�� ������ ������ HWM�� �����Ѵ�. */
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 1;

    /* ������ �Ҵ�� ������ ���� ����
     * ������ alloc page�� ���������� DPath Insert�� �������̴�. */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aLstPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sdpstCache::setLstAllocPage( aStatistics,
                                 aSegHandle,
                                 aLstPID,
                                 sdpPhyPage::getSeqNo(
                                     sdpPhyPage::getHdr(sPagePtr) ) );

    /* Segment Header �� Segment Cache�� HWM ���� */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          sSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    /* ���� HWM�� �ӽ÷� �����Ѵ�. */
    calcExtInfo2ExtRID( sSegPID,
                        sPagePtr,
                        sSegHdr->mHWM.mExtDirPID,
                        sSegHdr->mHWM.mSlotNoInExtDir,
                        &sExtRID4HWM );

    sNTAData[0] = aSegHandle->mSegPID;
    sNTAData[1] = sExtRID4HWM;
    sNTAData[2] = sSegHdr->mHWM.mWMPID;

    IDE_TEST( sdpstSH::logAndUpdateWM( &sMtx,
                                       &(sSegHdr->mHWM),
                                       sLstPIDInExt,
                                       sLstExtDirPID,
                                       sLstSlotNoInExtDir,
                                       &sNewHWMStack ) != IDE_SUCCESS );

    sdpstCache::lockHWM4Write( aStatistics, sSegCache );
    sSegCache->mHWM = sSegHdr->mHWM;
    sdpstCache::unlockHWM( sSegCache );

    /* BUG-29203 [SM] DPath Insert ��, HWM ������Ʈ ���� ���� KILL ���� ��
     *           restart recovery �� ASSERT�� ������ ����
     * rollback�ϱ� ���� logical undo log�� ����Ѵ�. */
    sdrMiniTrans::setNTA( &sMtx,
                          aSpaceID,
                          SDR_OP_SDPST_UPDATE_WMINFO_4DPATH,
                          &sNTA,
                          sNTAData,
                          3 /* DataCount */ );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : merge�� ������ bitmap�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstDPath::updateBMPUntilHWM( idvSQL           * aStatistics,
                                      scSpaceID          aSpaceID,
                                      scPageID           aSegPID,
                                      scPageID           aFstAllocPIDOfFM,
                                      scPageID           aLstAllocPIDOfFM,
                                      sdrMtxStartInfo  * aStartInfo )
{
    sdpstMFNL          sNewMFNL;
    idBool             sIsFinish;
    sdpstBMPType       sDepth;
    sdpstBMPType       sPrvDepth;
    SShort             sLfFstSlotNo;
    SShort             sItFstSlotNo;
    sdpstStack         sFstPageStack;
    sdpstStack         sLstPageStack;
    sdpstStack       * sCurStack;

    IDE_ASSERT( aSegPID          != SD_NULL_PID );
    IDE_ASSERT( aLstAllocPIDOfFM != SD_NULL_RID );
    IDE_ASSERT( aStartInfo       != NULL );

    /* ���� ù��°�� �Ҵ��ߴ� �������� Stack �����Ѵ�. */
    sdpstStackMgr::initialize( &sFstPageStack );
    IDE_TEST( sdpstAllocPage::makeOrderedStackFromDataPage(
                                         aStatistics,
                                         aSpaceID,
                                         aSegPID,
                                         aFstAllocPIDOfFM,
                                         &sFstPageStack ) != IDE_SUCCESS );

    /* ������ �����ߴ� �������� ���� Stack �� �����Ѵ�. */
    sdpstStackMgr::initialize( &sLstPageStack );
    IDE_TEST( sdpstAllocPage::makeOrderedStackFromDataPage(
                                         aStatistics,
                                         aSpaceID,
                                         aSegPID,
                                         aLstAllocPIDOfFM,
                                         &sLstPageStack ) != IDE_SUCCESS );

    /* Append�� ���ؼ� ���� �Ҵ�� �������� ���� ���� �� �Լ���
     * ȣ������ �ʴ´�. */
    if ( sdpstStackMgr::compareStackPos( &sLstPageStack,
                                         &sFstPageStack ) < 0 )
    {
        ideLog::log( IDE_SERVER_0, "========= Lst Page Stack ========\n" );
        sdpstStackMgr::dump( &sLstPageStack );
        ideLog::log( IDE_SERVER_0, "========= Fst Page Stack ========\n" );
        sdpstStackMgr::dump( &sFstPageStack );
        IDE_ASSERT( 0 );
    }

    /*
     * ������ ������ bitmap �����ϴ� �˰����̴�.
     *
     * A-update.
     * ����, ������ ���������� �����ߴٸ�, ���� leaf�� ã�´�. => B-move
     * ������ leaf�� New HHWM�� ���ٸ�, ������ ���������� data ��������
     * ��� Full���·� �����Ѵ�. �� �� ���� leaf�� ã�´�. => B-move
     * ������, New HHWM�� �ִٸ� New HHWM���� Full���·� �����ϰ�,
     * ������ �ö󰣴�. => B-move
     *
     * B-move.
     * ������ internal�� ���� leaf�� New HHWM�̰ų� �׷��� �ʰų�
     * ���� leaf�� �����Ѵ�. => A-update
     *
     * B-update.
     * New HHWM �̸� �ش� lf slot�� �־��� MFNL�� �����ϰ�,
     * ������ �����Ѵ�. => C-move
     * ����, ���� leaf�� ���ٸ�, lfslot���� MFNL�� FULL�� �����ϰ�,
     * ������ �����Ѵ�. => C-move
     *
     * C-move.
     * ������ root�� ���� internal�� New HHWM�̰ų� �׷��� �ʰų�
     * ���� internal�� �����Ѵ�. => B-move
     *
     * C-update.
     * New HHWM�̸� itslot�� MFNL�� �־��� MFNL�� �����ϰ�
     * ������ �Ϸ��Ѵ�. => end
     * ����, ���� internal�� ���ٸ� itslot���� MFNL�� FULL�� �����ϰ�,
     * ���� root�� �̵��Ѵ�. => C-move
     */

    /* LeafBMP �� InternalBMP�� ���� SlotNumber.
     * ���� BMP�� MFNL�� �����Ҷ�, ��� ���� �����ؾ� ������ Traverse
     * �ϴ� ���� �����Ѵ�. */
    sLfFstSlotNo = SDPST_INVALID_SLOTNO;
    sItFstSlotNo = SDPST_INVALID_SLOTNO;
    sIsFinish    = ID_FALSE;

    sCurStack   = &sFstPageStack;

    /* Lf-BMP���� �����ϹǷ� ���� depth�� It-BMP�� ������ ���´�. */
    sPrvDepth   = SDPST_ITBMP;
    if ( sdpstStackMgr::getDepth( sCurStack ) != SDPST_LFBMP )
    {
        sdpstStackMgr::dump( sCurStack );
        IDE_ASSERT( 0 );
    }

    while( 1 )
    {
        sDepth = sdpstStackMgr::getDepth( sCurStack );

        if ( (sDepth != SDPST_VIRTBMP) &&
             (sDepth != SDPST_LFBMP) &&
             (sDepth != SDPST_ITBMP) &&
             (sDepth != SDPST_RTBMP) )
        {
            sdpstStackMgr::dump( sCurStack );
            IDE_ASSERT( 0 );
        }

        IDE_TEST( sdpstBMP::mBMPOps[sDepth]->mUpdateBMPUntilHWM(
                                                aStatistics,
                                                aStartInfo,
                                                aSpaceID,
                                                aSegPID,
                                                &sLstPageStack,
                                                sCurStack,
                                                &sLfFstSlotNo,
                                                &sItFstSlotNo,
                                                &sPrvDepth,
                                                &sIsFinish,
                                                &sNewMFNL ) != IDE_SUCCESS );

        /* ���� ���� */
        if ( (sDepth == SDPST_RTBMP) && (sIsFinish == ID_TRUE) )
        {
            break;
        }
    }

    /* �� ������ MFNL�� ������ �ʿ䰡 ������ �����ȴ�. ���� INVALID_SLOTNO, ��
     * -1�� �ƴ� �ٸ� ���� ���, ������ �ʿ䰡 �ִµ� ������ ���� ��찡 �ȴ�*/
    if( ( sLfFstSlotNo != SDPST_INVALID_SLOTNO ) ||
        ( sItFstSlotNo != SDPST_INVALID_SLOTNO ) )
    {
        ideLog::log( IDE_SERVER_0, 
                     "LeafSlot     - %d\n"
                     "InternalSlot - %d\n"
                     "SpaceID      - %u\n"
                     "SegPID       - %u\n",
                     sLfFstSlotNo,
                     sItFstSlotNo,
                     aSpaceID,
                     aSegPID );
        sdpstStackMgr::dump( sCurStack );
        sdpstStackMgr::dump( &sLstPageStack );
        IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Direct-Path ���꿡�� �Ҵ�� ������ bitmap�� update �Ѵ�.
 ***********************************************************************/
IDE_RC sdpstDPath::updateBMPUntilHWMInLfBMP(
                                       idvSQL            * aStatistics,
                                       sdrMtxStartInfo   * aStartInfo,
                                       scSpaceID           aSpaceID,
                                       scPageID            /*aSegPID*/,
                                       sdpstStack        * aHWMStack,
                                       sdpstStack        * aCurStack,
                                       SShort            * /*aFmLfSlotNo*/,
                                       SShort            * /*aFmItSlotNo*/,
                                       sdpstBMPType      * aPrvDepth,
                                       idBool            * aIsFinish,
                                       sdpstMFNL         * aNewMFNL )
{
    UInt                 sState = 0;
    smLSN                sNTA;
    sdrMtx               sMtx;
    ULong                sArrData[4];
    SShort               sFmPBSNo;
    SShort               sToPBSNo;
    sdpstPosItem         sHwmPos;
    sdpstPosItem         sCurPos;
    UShort               sMetaPageCnt;
    UShort               sPageCnt;
    SShort               sDist;
    idBool               sIsFinish;
    idBool               sNeedToChangeMFNL;
    UChar              * sPagePtr;
    sdpstLfBMPHdr      * sLfBMPHdr;
    sdpstRedoUpdateLfBMP4DPath sLogData;

    IDE_ASSERT( aHWMStack  != NULL );
    IDE_ASSERT( aCurStack  != NULL );
    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aNewMFNL   != NULL );
    IDE_ASSERT( aIsFinish  != NULL );
    IDE_ASSERT( *aPrvDepth == SDPST_ITBMP);

    /* Lf-BMP���� HWM�� ������ �������� ��� ���� �ܰ�� �����Ѵ�. */
    sIsFinish  = ID_FALSE;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    // HWM�� ���� leaf �������� �ִ°�?
    sDist = sdpstBMP::mBMPOps[SDPST_LFBMP]->mGetDistInDepth(
                           sdpstStackMgr::getAllPos(aHWMStack),
                           sdpstStackMgr::getAllPos(aCurStack) );

    if ( sDist < 0 )
    {
        sdpstStackMgr::dump( aHWMStack );
        sdpstStackMgr::dump( aCurStack );
        IDE_ASSERT( 0 );
    }

    sCurPos = sdpstStackMgr::getCurrPos( aCurStack );
    IDE_ASSERT( sCurPos.mNodePID != SD_NULL_PID );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          sCurPos.mNodePID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sLfBMPHdr = sdpstLfBMP::getHdrPtr( sPagePtr );

    if ( sdpstLfBMP::isLstPBSNo( sPagePtr, sCurPos.mIndex ) == ID_TRUE )
    {
        // ������ ��������� ���� leaf�� �̵��ؾ��Ѵ�. ������, New HHWM
        // ���, GoNext�� ID_FALSE�� �����Ǿ� ������ �ö󰡾��Ѵ�.
        // �Ʒ��κп��� ó���Ѵ�.
        *aNewMFNL = sLfBMPHdr->mBMPHdr.mMFNL;
    }

    /* ������ ������ ã�´�. */
    sFmPBSNo = sCurPos.mIndex;
    if ( sDist == SDPST_FAR_AWAY_OFF )
    {
        // HHWM�� ������ �������� �ƴѰ�� leaf bmp�������� ����������
        // �˻��Ѵ�.
        sToPBSNo = sLfBMPHdr->mTotPageCnt - 1;
    }
    else
    {
        // HWM�� ������ �������� ��� HWM������ Ž���Ѵ�.
        sHwmPos  = sdpstStackMgr::getSeekPos( aHWMStack, SDPST_LFBMP );
        sToPBSNo = sHwmPos.mIndex;

        // HHWM�� ������ ������ ������ root���� �Ϸ���Ѿ��Ѵ�.
        sIsFinish = ID_TRUE;
    }

    sPageCnt = sToPBSNo - sFmPBSNo + 1;

    /* Lf-BMP ������ ��� ��쿡�� PBS�� �������� �ʴ´�. */
    if ( sPageCnt == 0 )
    {
        IDE_CONT( leave_update_bmp_in_lfbmp );
    }
    else
    {
        IDE_ASSERT( sPageCnt > 0 );
    }

    /* leaf������ bitmap ���濡 ���� Rollback ������ ���ؼ� ���⼭
     * NTA�� �����Ѵ�. */
    sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

    /* from -> to ���� PBS ���� */
    sdpstLfBMP::updatePBS( sdpstLfBMP::getMapPtr(sLfBMPHdr),
                                      sFmPBSNo,
                                      (sdpstPBS)(SDPST_BITSET_PAGETP_DATA |
                                                 SDPST_BITSET_PAGEFN_FUL),
                                      sPageCnt,
                                      &sMetaPageCnt );

    sLogData.mFmPBSNo = sFmPBSNo;
    sLogData.mPageCnt = sPageCnt;

    IDE_TEST( sdrMiniTrans::writeLogRec( &sMtx,
                                         (UChar*)sdpstLfBMP::getMapPtr(sLfBMPHdr),
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_UPDATE_LFBMP_4DPATH )
              != IDE_SUCCESS );



    /* Lf-BMP �������� MFNL Table�� ��ǥ MFNL�� �����Ѵ�. */
    IDE_TEST( sdpstBMP::logAndUpdateMFNL(
                                    &sMtx,
                                    sdpstLfBMP::getBMPHdrPtr(sLfBMPHdr),
                                    SDPST_INVALID_SLOTNO,
                                    SDPST_INVALID_SLOTNO,
                                    SDPST_MFNL_FMT,
                                    SDPST_MFNL_FUL,
                                    sPageCnt - sMetaPageCnt,
                                    aNewMFNL,
                                    &sNeedToChangeMFNL ) != IDE_SUCCESS );

    sArrData[0] = sCurPos.mNodePID;
    sArrData[1] = sFmPBSNo;
    sArrData[2] = sToPBSNo;
    sArrData[3] = sLfBMPHdr->mBMPHdr.mMFNL;

    IDE_DASSERT( sdpstLfBMP::verifyBMP( sLfBMPHdr ) == IDE_SUCCESS );

    sdrMiniTrans::setNTA( &sMtx,
                          aSpaceID,
                          SDR_OP_SDPST_UPDATE_MFNL_4DPATH,
                          &sNTA,
                          sArrData,
                          4 /* DataCount */ );

    IDE_EXCEPTION_CONT( leave_update_bmp_in_lfbmp );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    sdpstStackMgr::pop( aCurStack );

    *aPrvDepth = SDPST_LFBMP;
    *aIsFinish = sIsFinish;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct-Path ���꿡�� �Ҵ�� ������ Bitmap����
 *               �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstDPath::updateBMPUntilHWMInRtAndItBMP(
                                       idvSQL            * aStatistics,
                                       sdrMtxStartInfo   * aStartInfo,
                                       scSpaceID           aSpaceID,
                                       scPageID            /*aSegPID*/,
                                       sdpstStack        * aHWMStack,
                                       sdpstStack        * aCurStack,
                                       SShort            * aFmLfSlotNo,
                                       SShort            * aFmItSlotNo,
                                       sdpstBMPType      * aPrvDepth,
                                       idBool            * aIsFinish,
                                       sdpstMFNL         * aNewMFNL )
{
    UInt                 sState = 0;
    sdrMtx               sMtx;
    UChar              * sPagePtr;
    SShort             * sFmSlotNo;
    SShort               sToSlotNo = 0;
    SShort               sNxtSlotNo;
    sdpstPosItem         sCurPos;
    SShort               sDist;
    sdpstBMPHdr        * sBMPHdr;
    idBool               sIsUpdateMFNL;
    idBool               sIsLstSlot;
    idBool               sGoDown;
    sdrMtxStartInfo      sStartInfo;
    sdpstBMPType         sCurDepth;

    IDE_ASSERT( aHWMStack   != NULL );
    IDE_ASSERT( aCurStack   != NULL );
    IDE_ASSERT( aStartInfo  != NULL );
    IDE_ASSERT( aFmLfSlotNo != NULL );
    IDE_ASSERT( aFmItSlotNo != NULL );
    IDE_ASSERT( aNewMFNL    != NULL );
    IDE_ASSERT( aIsFinish   != NULL );
    IDE_ASSERT( (*aPrvDepth == SDPST_LFBMP) ||
                (*aPrvDepth == SDPST_ITBMP) ||
                (*aPrvDepth == SDPST_RTBMP) ||
                (*aPrvDepth == SDPST_VIRTBMP) );

    sIsUpdateMFNL = ID_FALSE;
    sCurDepth     = sdpstStackMgr::getDepth( aCurStack );

    /* HWM �� ���� It-BMP�� �����ϴ°�? */
    sDist = sdpstBMP::mBMPOps[sCurDepth]->mGetDistInDepth(
                           sdpstStackMgr::getAllPos(aHWMStack),
                           sdpstStackMgr::getAllPos(aCurStack) );

    /* �����ö��� sDist > 0, �ö�ö��� sDist >= 0 */
    if ( sDist < 0 )
    {
        sdpstStackMgr::dump( aHWMStack );
        sdpstStackMgr::dump( aCurStack );
        IDE_ASSERT( 0 );
    }

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 1;

    sCurPos = sdpstStackMgr::getCurrPos( aCurStack );

    /* BUG-32164 [sm-disk-page] If Table Segment is expanded to 12gb by
     * Direct-path insert operation, Server fatal
     * ���簡 RootBMP�̸� internalSlotNumber�� ����ϰ�,
     * ���簡 InternalBMP�̸� LeafSlotNumber�� ����Ѵ�. */
    if( sCurDepth == SDPST_ITBMP )
    {
        sFmSlotNo = aFmLfSlotNo;
        sToSlotNo = sCurPos.mIndex;
    }
    else
    {
        sFmSlotNo = aFmItSlotNo;
        sToSlotNo = sCurPos.mIndex;
    }

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          sCurPos.mNodePID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sBMPHdr = sdpstBMP::getHdrPtr( sPagePtr );

    sIsLstSlot = sdpstBMP::isLstSlotNo( sPagePtr, sCurPos.mIndex );

    // [1] Bitmap ���濩�θ� �����Ѵ�.
    // [1-1] Bitmap�� �����ؾ��� ������ ToSlotIdx�� ���Ѵ�.
    if ( SDPST_BMP_CHILD_TYPE( sCurDepth ) == *aPrvDepth )
    {
        // * leaf���� �ö�� ��� *
        // ���� slot�� �������̰ų� New HHWM���
        // ���� internal bmp�� �������� �ʾҴ� lf slot�鿡 ���ؼ�
        // �ѹ��� mfnl ������ �����Ѵ�.

        /*    |
         * +-----+-+-+-+-+  +-----+-+-+-+
         * |0IBMP|0|1|2|3|--|1IBMP|0|1|2|
         * +-----+-+-+-+-+  +-----+-+-+-+
         *        | | | |          | | | 
         *  <-Case2-2-> |          <Case1>
         *        <Case2-1>
         * �� IBMP�� ������ Slot�� �����ϴ� �����, MFNL ����*/

        if ( sDist == 0 )
        {
            /* CASE 1 */
            // leaf���� New HHWM���� bitmap�� �����ϰ� �ö󰡴� ���̴�.
            // ���̻� next�� ���� �ʴ´�.
            IDE_ASSERT( *aIsFinish == ID_TRUE );

            sIsUpdateMFNL = ID_TRUE;
        }
        else
        {
            if ( sIsLstSlot == ID_TRUE )
            {
                /* CASE 2-1 */
                // ������ Slot�̸� aFmLfSlotIdx ���� ������
                // Full�� �����Ѵ�.
                IDE_ASSERT( sCurPos.mIndex == sBMPHdr->mSlotCnt - 1  );
                sIsUpdateMFNL = ID_TRUE;
            }
            else
            {
                /* CASE 2-2 */
                // Leaf���� �ö�Դµ� �������� �ƴϸ�
                // ���� leaf�� ��������.
                sIsUpdateMFNL = ID_FALSE;
            }
        }
    }
    else
    {
        /* ���� BMP���� �������� ���̶�� ������ ��������. */
    }

    // [1-2] Bitmap ���� ������ �����Ѵ�.
    if ( *sFmSlotNo == SDPST_INVALID_SLOTNO )
    {
        // root���� ó�� ������ ��� Ȥ��
        // leaf���� �ö���� �߿��� bitmap ������� LfBMP slot��
        // ������ �����Ѵ�.
        // ����, �̹� ���� HWM�� ���Ե� LfBMP ���� pageBitmap��
        // �ߺ������Ͽ��� ������ ����.
        *sFmSlotNo = sCurPos.mIndex;
    }

    // [1-3] ������ RtBMP �̴� LfBMP�̴� MFNL�� FULL�� �ƴѰ��
    if ( *aNewMFNL != SDPST_MFNL_FUL )
    {
        if( *aPrvDepth == SDPST_BMP_CHILD_TYPE( sCurDepth ) )
        {
            // leaf�� mfnl�� full�� �ƴ� ��쿡�� �ٷ� lfslot��
            // mfnl�� �����Ѵ�.
            sIsUpdateMFNL = ID_TRUE;
        }
    }
    else
    {
        // ������ RtBMP �̴� LfBMP�̴� MFNL�� FULL�� ���
    }

    // [3] Bitmap�� �����Ѵ�.
    if ( sIsUpdateMFNL == ID_TRUE )
    {
        sdrMiniTrans::makeStartInfo( &sMtx, &sStartInfo );

        IDE_ASSERT( sToSlotNo >= *sFmSlotNo );
        IDE_ASSERT( *sFmSlotNo != SDPST_INVALID_SLOTNO );

        IDE_TEST( sdpstDPath::updateMFNLToFull4DPath( aStatistics,
                                    &sStartInfo,
                                    sBMPHdr,
                                    *sFmSlotNo,
                                    sToSlotNo,
                                    aNewMFNL ) != IDE_SUCCESS );

        // update�� �Ͽ��� ������ sFmSlotNo�� �ʱ�ȭ���ش�.
        *sFmSlotNo = SDPST_INVALID_SLOTNO;
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    if ( (sIsLstSlot == ID_TRUE) || (*aIsFinish == ID_TRUE) )
    {
        if ( *aPrvDepth == SDPST_BMP_CHILD_TYPE( sCurDepth ) )
        {
            sdpstStackMgr::pop( aCurStack );
            sGoDown = ID_FALSE;
        }
        else
        {
            sGoDown = ID_TRUE;
        }
    }
    else
    {
        sGoDown = ID_TRUE;
    }

    if ( sGoDown == ID_TRUE )
    {
        /* ���� BMP���� �ö�� ��� ���� ���� BMP�� �������� �Ѵ�.
         * (���� Slot���� �̵�)
         * ���� BMP���� ������ ��� �׳� �ٷ� ��������. */
        if ( *aPrvDepth == SDPST_BMP_CHILD_TYPE( sCurDepth ) )
        {
            sNxtSlotNo = sCurPos.mIndex + 1;
        }
        else
        {
            sNxtSlotNo = sCurPos.mIndex;
        }

        sdpstStackMgr::setCurrPos( aCurStack,
                                   sCurPos.mNodePID,
                                   sNxtSlotNo );

        sdpstStackMgr::push(
            aCurStack,
            sdpstBMP::getSlot(sBMPHdr, sNxtSlotNo)->mBMP,
            0 );
    }

    *aPrvDepth = sCurDepth;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Direct-Path ���꿡�� �Ҵ�� ������ Bitmap����
 *               �����ϱ� ����, ���� BMP�� push�Ͽ� �Ѱ��ش�.
 ***********************************************************************/
IDE_RC sdpstDPath::updateBMPUntilHWMInVtBMP(
                                       idvSQL            * aStatistics,
                                       sdrMtxStartInfo   * /*aStartInfo*/,
                                       scSpaceID           aSpaceID,
                                       scPageID            aSegPID,
                                       sdpstStack        * aHWMStack,
                                       sdpstStack        * aCurStack,
                                       SShort            * /*aFmSlotNo*/,
                                       SShort            * /*aFmItSlotNo*/,
                                       sdpstBMPType      * aPrvDepth,
                                       idBool            * /*aIsFinish*/,
                                       sdpstMFNL         * /*aNewMFNL*/ )
{
    sdpstPosItem         sCurPos;
    SShort               sNxtSlotNo;
    scPageID             sCurRtBMP;
    UChar              * sPagePtr;
    sdpstBMPHdr        * sBMPHdr;
    UInt                 sState = 0;
    UInt                 i;

    IDE_ASSERT( aCurStack != NULL );
    IDE_ASSERT( aPrvDepth != NULL );

    /* VirtualBMP�� �������� �ʴ� BMP�� Root�� ��ġ�� ����Ű�� ���� �����Ѵ�. ��
     * VirtualBMP�� 0�� Slot�� ù��° RootBMP, 1�� Slot�� ���� RootBMP��
     * �ǹ��Ѵ�. ���� VirtualBMP�� BMP�� ����(MFNL ����)�� ���� �ʾƵ� �Ǹ�,
     * ������ ������ Root�� Ž���ϸ� �ȴ�.
     * �̶� ������ Root�� ù��° Root�� �����ϴ� SegmentPage�κ��� NextRoot��
     * ã�� Ž���ϸ� �ȴ�. */

    /* ������ Root�� ������ SegmentPage�� �ִ�. */
    sCurRtBMP = aSegPID;
    IDE_ASSERT( sCurRtBMP != SD_NULL_PID );

    /* ������ Root�� ���� Ž��  */
    sCurPos   = sdpstStackMgr::getCurrPos( aCurStack );
    sNxtSlotNo = sCurPos.mIndex + 1;

    for( i = 0 ; i < (UInt)sNxtSlotNo ; i ++ )
    {
        IDE_TEST( sdbBufferMgr::getPage( aStatistics,
                                         aSpaceID,
                                         sCurRtBMP,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         (UChar**)&sPagePtr,
                                         NULL  /* aTrySuccess */ )
                  != IDE_SUCCESS );
        sState = 1;

        sBMPHdr   = sdpstBMP::getHdrPtr( sPagePtr );
        sCurRtBMP = sdpstRtBMP::getNxtRtBMP( sBMPHdr );

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sPagePtr )
                  != IDE_SUCCESS );

        /* NxtSlotNo�� ���� Root�������� Ŭ ��� */
        if( sCurRtBMP == SC_NULL_PID )
        {
            ideLog::log( IDE_SERVER_0, 
                         "i            - %d\n"
                         "sNxtSlotNo   - %d\n"
                         "SpaceID      - %u\n"
                         "SegPID       - %u\n",
                         i,
                         sNxtSlotNo,
                         aSpaceID,
                         aSegPID );
            sdpstStackMgr::dump( aCurStack );
            sdpstStackMgr::dump( aHWMStack );

            IDE_ASSERT( sCurRtBMP == SC_NULL_PID );
        }
    }

    /* ���� Slot�� ����Ű���� �����Ѵ�. */
    sdpstStackMgr::setCurrPos( aCurStack,
                               sCurPos.mNodePID,
                               sNxtSlotNo );

    sdpstStackMgr::push(
        aCurStack,
        sCurRtBMP,   /* ������ Bmp */
        0 );         /* Slot Number */

    *aPrvDepth = SDPST_VIRTBMP;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


