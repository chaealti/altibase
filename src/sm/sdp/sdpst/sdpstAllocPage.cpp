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
 * $Id: sdpstAllocPage.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment���� ������� �Ҵ� ���� ���� STATIC
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

/***********************************************************************
 * Description : [INTERFACE] Segment�� Ž���Ͽ� �� �������� �Ҵ��Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] Table Space�� ID
 * aSegHandle   - [IN] Segment�� Handle
 * aPageType    - [IN] �Ҵ��Ϸ��� �������� Ÿ��
 * aNewPagePtr  - [OUT] �Ҵ��� �������� Pointer
 ***********************************************************************/
IDE_RC sdpstAllocPage::allocateNewPage( idvSQL             * aStatistics,
                                        sdrMtx             * aMtx,
                                        scSpaceID            aSpaceID,
                                        sdpSegHandle       * aSegHandle,
                                        sdpPageType          aPageType,
                                        UChar             ** aNewPagePtr )
{
    UChar                * sPagePtr;
    scPageID               sNewHintDataPID;
    UChar                  sCTSlotNo;

    IDE_DASSERT( aSegHandle  != NULL );
    IDE_DASSERT( aMtx        != NULL );
    IDE_DASSERT( aNewPagePtr != NULL );

    sPagePtr   = NULL;
    sCTSlotNo = SDP_CTS_IDX_NULL;

    /* Segment�� ��������� Ž���Ѵ�.
     * Slot�� Ž���ϸ鼭, �������� �����ϰ� �Ǹ� Table ������Ÿ������
     * �ʱ�ȭ�Ͽ� ��ȯ�Ѵ�. */
    IDE_TEST( sdpstFindPage::searchFreeSpace( aStatistics,
                                              aMtx,
                                              aSpaceID,
                                              aSegHandle,
                                              0, /* unuse rowsize */
                                              aPageType,
                                              SDPST_SEARCH_NEWPAGE,
                                              &sPagePtr,
                                              &sCTSlotNo,
                                              &sNewHintDataPID )
              != IDE_SUCCESS );

    // �ݵ�� �������� �����Ͽ� ��ȯ�Ѵ�. ������������ �������� ���ϴ�
    // ��쿡�� Exception�� �߻��Ѵ�.
    IDE_ASSERT( sPagePtr   != NULL );

    *aNewPagePtr = sPagePtr;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] ��û�� Free ������ ������ ������ �� �ֵ���
 *               Segment�� Extent�� �̸� Ȯ���Ѵ�.
 *
 * �� �Լ��� ȣ���Ҷ����� ���ÿ� �ٸ� Ʈ������� �ش� Segment����
 * ��������� �Ҵ��� �� �� ����. ��, ���ü��� ������� �ʾƵ� �ȴ�.
 * �ε��������� ����Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] Table Space�� ID
 * aSegHandle   - [IN] Segment�� Handle
 * aCountWanted - [IN] �ʿ��� ������ ����
 ***********************************************************************/
IDE_RC sdpstAllocPage::prepareNewPages( idvSQL            * aStatistics,
                                        sdrMtx            * aMtx,
                                        scSpaceID           aSpaceID,
                                        sdpSegHandle      * aSegHandle,
                                        UInt                aCountWanted )
{
    SInt              sState = 0 ;
    UChar           * sPagePtr;
    sdrMtxStartInfo   sStartInfo;
    sdpstSegHdr     * sSegHdr;
    ULong             sFreePageCnt;

    IDE_DASSERT( aSegHandle!= NULL );
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( aCountWanted > 0 );

    sdrMiniTrans::makeStartInfo ( aMtx, &sStartInfo );

    while(1)
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                              aSpaceID,
                                              aSegHandle->mSegPID,
                                              &sPagePtr ) != IDE_SUCCESS );
        sState = 1;

        sSegHdr = sdpstSH::getHdrPtr(sPagePtr);

        sFreePageCnt = sSegHdr->mFreeIndexPageCnt;

        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );

        /* Free�������� �����ϸ� ���ο� Extent�� TBS�� ���� �䱸�Ѵ�. */
        if( sFreePageCnt < (ULong)aCountWanted )
        {
            IDE_TEST( sdpstSegDDL::allocateExtents(
                                   aStatistics,
                                   &sStartInfo,
                                   aSpaceID,
                                   aSegHandle,
                                   aSegHandle->mSegStoAttr.mNextExtCnt )
                      != IDE_SUCCESS );
        }
        else
        {
            break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description :  PageBitSet�� �����ϸ鼭 ���� bitmap �������� MFNL��
 *                �����Ѵ�
 *
 * aStatistics     - [IN] �������
 * aMtx            - [IN] Mini Transaction Pointer
 * aSpaceID        - [IN] Table Space�� ID
 * aSegHandle      - [IN] Segment�� Handle
 * aChangePhase    - [IN] MFNL�� ������ BMP �ܰ�
 * aChildPID       - [IN] aCurPID�� ���� PID
 * aPBSNo          - [IN] PageBitSet No
 * aChageState     - [IN] ������ State (PBS) LfBMP���� ���
 * aCurPID         - [IN] ���� BMP PID
 * aSlotNoInParent - [IN] �θ� BMP�� �����ϴ� �� BMP�� slot No
 * aRevStack       - [IN] Reverse Stack
 ***********************************************************************/
IDE_RC sdpstAllocPage::tryToChangeMFNLAndItHint(
                                idvSQL               * aStatistics,
                                sdrMtx               * aMtx,
                                scSpaceID              aSpaceID,
                                sdpSegHandle         * aSegHandle,
                                sdpstChangeMFNLPhase   aChangePhase,
                                scPageID               aChildPID,
                                SShort                 aPBSNo,
                                void                 * aChangeState,
                                scPageID               aCurPID,
                                SShort                 aSlotNoInParent,
                                sdpstStack           * aRevStack )
{
    sdpstMFNL                sNewMFNL;
    scPageID                 sChildPID;
    scPageID                 sCurPID;
    SShort                   sSlotNoInParent;
    idBool                   sNeedToChangeMFNL;
    scPageID                 sParentPID;
    sdpstSegCache          * sSegCache;

    IDE_DASSERT( aSegHandle   != NULL );
    IDE_DASSERT( aMtx         != NULL );
    IDE_DASSERT( aChangeState != NULL );
    IDE_DASSERT( aRevStack    != NULL );

    sChildPID = aChildPID;
    sCurPID   = aCurPID;
    sSlotNoInParent = aSlotNoInParent;

    sNeedToChangeMFNL = ID_TRUE; // ó������ ������ TRUE�̴�.

    if ( aChangePhase != SDPST_CHANGEMFNL_LFBMP_PHASE )
    {
        sNewMFNL = *(sdpstMFNL*)(aChangeState);
    }

    switch( aChangePhase )
    {
        case SDPST_CHANGEMFNL_LFBMP_PHASE:

            IDE_ASSERT( aPBSNo != SDPST_INVALID_PBSNO );

            // Leaf BMP �������� ���뵵�� �����Ѵ�.
            IDE_TEST( sdpstLfBMP::tryToChangeMFNL(
                          aStatistics,
                          aMtx,
                          aSpaceID,
                          sChildPID,
                          aPBSNo,
                          *(sdpstPBS*)(aChangeState),
                          &sNeedToChangeMFNL,
                          &sNewMFNL,
                          &sParentPID,
                          &sSlotNoInParent ) != IDE_SUCCESS );

            sCurPID = sParentPID;

        case SDPST_CHANGEMFNL_ITBMP_PHASE:
            if ( sNeedToChangeMFNL == ID_TRUE )
            {
                // push (itbmp, lfslotidx)
                sdpstStackMgr::push( aRevStack,
                                     sCurPID,
                                     sSlotNoInParent );

                // Internal BMP �������� ���뵵�� �����Ѵ�.
                IDE_TEST( sdpstBMP::tryToChangeMFNL( aStatistics,
                                                     aMtx,
                                                     aSpaceID,
                                                     sCurPID,
                                                     sSlotNoInParent,
                                                     sNewMFNL,
                                                     &sNeedToChangeMFNL,
                                                     &sNewMFNL,
                                                     &sParentPID,
                                                     &sSlotNoInParent )
                          != IDE_SUCCESS );

                sChildPID = sCurPID;
                sCurPID  = sParentPID;
            }

        case SDPST_CHANGEMFNL_RTBMP_PHASE:

            if ( sNeedToChangeMFNL == ID_TRUE )
            {
                // push (rtbmp, itslotidx)
                sdpstStackMgr::push( aRevStack,
                                     sCurPID,
                                     sSlotNoInParent );

                // Root BMP �������� ���뵵�� �����Ѵ�.
                // Root BMP�� SMH�� ������ ��쿡 ������
                // �̸� ȹ��� ��찡 ������ X-latch��
                // 2�� ȹ���ϴ� ����̹Ƿ� ������ ���� �ʴ´�.
                IDE_TEST( sdpstBMP::tryToChangeMFNL( aStatistics,
                                                     aMtx,
                                                     aSpaceID,
                                                     sCurPID,
                                                     sSlotNoInParent,
                                                     sNewMFNL,
                                                     &sNeedToChangeMFNL,   /* ignored */
                                                     &sNewMFNL,
                                                     &sParentPID,
                                                     &sSlotNoInParent )
                          != IDE_SUCCESS );

                if ( sNewMFNL == SDPST_MFNL_FUL )
                {
                    // Internal Slot�� MFNL�� Full�� ����� ��쿡��
                    // Internal Hint�� ���濩�θ� �˻��Ѵ�.
                    sSegCache   = (sdpstSegCache*)aSegHandle->mCache;

                    IDE_TEST( sdpstRtBMP::forwardItHint(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  sSegCache,
                                  aRevStack,
                                  SDPST_SEARCH_NEWSLOT ) != IDE_SUCCESS );

                    IDE_TEST( sdpstRtBMP::forwardItHint(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  sSegCache,
                                  aRevStack,
                                  SDPST_SEARCH_NEWPAGE ) != IDE_SUCCESS );
                }
            }
            break;
        default:
            ideLog::log( IDE_SERVER_0,
                         "aChangePhase: %u\n",
                         aChangePhase );
            IDE_ASSERT( 0 );
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �������� �����ϰ�, �ʿ��� ������ Ÿ�� �ʱ�ȭ �� logical
 *               Header�� �ʱ�ȭ�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstAllocPage::createPage( idvSQL           * aStatistics,
                                   sdrMtx           * aMtx,
                                   scSpaceID          aSpaceID,
                                   sdpSegHandle     * aSegHandle,
                                   scPageID           aNewPageID,
                                   ULong              aSeqNo,
                                   sdpPageType        aPageType,
                                   scPageID           aParentPID,
                                   SShort             aPBSNoInParent,
                                   sdpstPBS           aPBS,
                                   UChar           ** aNewPagePtr )
{
    UChar         * sNewPagePtr;

    IDE_DASSERT( aNewPageID   != SD_NULL_PID );
    IDE_DASSERT( aParentPID   != SD_NULL_PID );
    IDE_DASSERT( aMtx         != NULL );
    IDE_DASSERT( aNewPagePtr  != NULL );

    IDE_TEST( sdbBufferMgr::createPage( aStatistics,
                                        aSpaceID,
                                        aNewPageID,
                                        aPageType,
                                        aMtx,
                                        &sNewPagePtr ) != IDE_SUCCESS );

    IDE_TEST( formatPageHdr( aMtx,
                             aSegHandle,
                             (sdpPhyPageHdr*)sNewPagePtr,
                             aNewPageID,
                             aSeqNo,
                             aPageType,
                             aParentPID,
                             aPBSNoInParent,
                             aPBS ) != IDE_SUCCESS );

    *aNewPagePtr = sNewPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : phyical hdr�� logical hdr�� page type�� �°� format�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstAllocPage::formatPageHdr( sdrMtx           * aMtx,
                                      sdpSegHandle     * aSegHandle4DataPage,
                                      sdpPhyPageHdr    * aNewPagePtr,
                                      scPageID           aNewPageID,
                                      ULong              aSeqNo,
                                      sdpPageType        aPageType,
                                      scPageID           aParentPID,
                                      SShort             aPBSNo,
                                      sdpstPBS           aPBS )
{
    sdpParentInfo    sParentInfo;
    smOID            sTableOID;
    UInt             sIndexID;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aNewPagePtr != NULL );
    IDE_ASSERT( aNewPageID  != SD_NULL_PID );

    sParentInfo.mParentPID   = aParentPID;
    sParentInfo.mIdxInParent = aPBSNo;

    if( aSegHandle4DataPage != NULL )
    {
        sTableOID = ((sdpSegCCache*) aSegHandle4DataPage->mCache )->mTableOID;
        sIndexID  = ((sdpSegCCache*) aSegHandle4DataPage->mCache )->mIndexID;
    }
    else
    {
        // Segment Meta Page
        sTableOID = SM_NULL_OID;
        sIndexID  = SM_NULL_INDEX_ID;
    }

    IDE_TEST( sdpPhyPage::logAndInit( aNewPagePtr,
                                      aNewPageID,
                                      &sParentInfo,
                                      (UShort)aPBS,
                                      aPageType,
                                      sTableOID,
                                      sIndexID,
                                      aMtx ) 
              != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::setSeqNo( aNewPagePtr,
                                    aSeqNo,
                                    aMtx ) 
              != IDE_SUCCESS );


    /* To Fix BUG-23667 [AT-F5 ART] Disk Table Insert��
     * ������� Ž���������� FMS FreePageList�� CTL�� �ʱ�ȭ���� ����
     * ��찡 ���� */
    if ( aPageType == SDP_PAGE_DATA )
    {
        if( aSegHandle4DataPage != NULL )
        {
            IDE_TEST( smLayerCallback::logAndInitCTL( aMtx,
                                                      aNewPagePtr,
                                                      aSegHandle4DataPage->mSegAttr.mInitTrans,
                                                      aSegHandle4DataPage->mSegAttr.mMaxTrans )
                      != IDE_SUCCESS );
        }

        IDE_TEST( sdpSlotDirectory::logAndInit(aNewPagePtr,
                                               aMtx)
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Data ���������� �Ҵ� Ȥ�� ���������� ������� ����
 *               ���Ŀ� �������� ���뵵 ���濩�θ� Ȯ���Ѵ�.
 *               Data �������� ���뵵 �������� ���ؼ� ���� BMP��������
 *               MFNL�� �����ϱ⵵ �Ѵ�.
 *
 *  aStatistics   - [IN] ��� ����
 *  aMtx          - [IN] Mini Transaction Pointer
 *  aSpaceID      - [IN] TableSpace ID
 *  aSegHandle    - [IN] Segment Handle
 *  aDataPagePtr  - [IN] insert, update, delete�� �߻��� Data Page Ptr
 *
 ***********************************************************************/
IDE_RC sdpstAllocPage::updatePageFN( idvSQL         * aStatistics,
                                     sdrMtx         * aMtx,
                                     scSpaceID        aSpaceID,
                                     sdpSegHandle   * aSegHandle,
                                     UChar          * aDataPagePtr )
{
    sdpPhyPageHdr    * sPageHdr;
    sdpstPBS           sPBS;
    sdpstPBS           sNewPageBitSet;
    sdpParentInfo      sParentInfo;
    sdpstStack         sRevStack;
    sdpstStack         sItHintStack;
    idBool             sIsTurnOn;
    sdpstSegCache    * sSegCache;

    IDE_DASSERT( aMtx         != NULL );
    IDE_DASSERT( aSegHandle   != NULL );
    IDE_DASSERT( aDataPagePtr != NULL );

    sPageHdr = sdpPhyPage::getHdr( aDataPagePtr );

    // �������� ���뵵 ������ �����ؾ��Ѵٸ� �����Ѵ�.
    if ( sdpstFindPage::needToChangePageFN( sPageHdr,
                                      aSegHandle,
                                      &sNewPageBitSet ) == ID_TRUE )
    {
        // Leaf Bitmap ������������ �ĺ� Data �������� PageBitSet�� �����Ѵ�.
        // �̷�����, ������ �����ؾ��� ��� Bitmap ��������
        // MFNL ���¸� �����Ѵ�.

        // ������ ���� ������ �ʿ��� ��� ó���Ѵ�.
        sParentInfo = sdpPhyPage::getParentInfo(aDataPagePtr);
        sPBS        = (sdpstPBS) sdpPhyPage::getState( sPageHdr );

        // ���� PageBitSet�� ���ü� ����.
        if ( sPBS == sNewPageBitSet )
        {
            ideLog::log( IDE_SERVER_0,
                         "Page ID: %u, "
                         "Before PBS: %u, "
                         "After PBS: %u\n",
                         sdpPhyPage::getPageID( aDataPagePtr ),
                         sPBS,
                         sNewPageBitSet );

            (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                         aDataPagePtr,
                                         "============= Data Page Dump =============\n"  );

            IDE_ASSERT( 0 );
        }

        sSegCache = (sdpstSegCache*)aSegHandle->mCache;
        sdpstStackMgr::initialize(&sRevStack);

        // leaf bmp ���������� MFNL�� ����õ��Ͽ� �������� �����Ѵ�.
        IDE_TEST( tryToChangeMFNLAndItHint(
                      aStatistics,
                      aMtx,
                      aSpaceID,
                      aSegHandle,
                      SDPST_CHANGEMFNL_LFBMP_PHASE,
                      sParentInfo.mParentPID,  // leaf pid
                      sParentInfo.mIdxInParent,
                      (void*)&sNewPageBitSet,
                      SD_NULL_PID,            // unuse : parent pid
                      SDPST_INVALID_SLOTNO,  // unuse : slotidxinparent
                      &sRevStack ) != IDE_SUCCESS );

        // tryToChangeMFNL�� �� �Ŀ� Data �������� ���뵵�� �����ؾ��Ѵ�.
        // �ֳ��ϸ�, �����Լ������� ���� ���뵵�� �����ϱ� �����̴�.
        IDE_TEST( sdpPhyPage::setState( sPageHdr, (UShort)sNewPageBitSet, aMtx )
                  != IDE_SUCCESS );

        // FreePage �Լ��� ������ �����ϱ� ������ �� �Լ��� Free Page��
        // ó������ �ʴ´�.
        IDE_ASSERT( sdpstLfBMP::isEqFN( 
                    sNewPageBitSet, SDPST_BITSET_PAGEFN_FMT ) == ID_FALSE );

        /* ���ο� pagebitset�� ��������� �þ���( freeslot�� �Ѱ�� )
         * segment cache�� it hint�� ����ɼ� �� �ִ�.
         * sRevStack Depth�� root�϶��� �����Ѵ�. �ֳ��ϸ� �ٸ� itbmp�� ���õǾ���
         * �����̴�. */
        if ( sdpstStackMgr::getDepth( &sRevStack ) == SDPST_RTBMP )
        {
            if ( (sPBS & SDPST_BITSET_PAGEFN_MASK) >
                 (sNewPageBitSet & SDPST_BITSET_PAGEFN_MASK) )
            {
                if ( sdpstCache::needToUpdateItHint( sSegCache,
                                                 SDPST_SEARCH_NEWSLOT )
                     == ID_FALSE )
                {
                    sdpstCache::copyItHint( aStatistics,
                                            sSegCache,
                                            SDPST_SEARCH_NEWSLOT,
                                            &sItHintStack );

                    if ( sdpstStackMgr::getDepth( &sItHintStack ) 
                         != SDPST_ITBMP )
                    {
                        ideLog::log( IDE_SERVER_0, "Invalid Hint Stack\n" );

                        sdpstCache::dump( sSegCache );
                        sdpstStackMgr::dump( &sItHintStack );

                        IDE_ASSERT( 0 );
                    }


                    IDE_TEST( sdpstFreePage::checkAndUpdateHintItBMP(
                                                            aStatistics,
                                                            aSpaceID,
                                                            aSegHandle->mSegPID,
                                                            &sRevStack,
                                                            &sItHintStack,
                                                            &sIsTurnOn ) 
                              != IDE_SUCCESS );

                    if ( sIsTurnOn == ID_TRUE )
                    {
                        sdpstCache::setUpdateHint4Slot( sSegCache, ID_TRUE );
                    }
                }
                else
                {
                    // �̹� on�Ǿ� �ִ� �����̹Ƿ� skip �Ѵ�.
                }
            }
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  �Ҵ��� �Ұ����� �������� Full�� ���� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstAllocPage::updatePageFNtoFull( idvSQL         * aStatistics,
                                           sdrMtx         * aMtx,
                                           scSpaceID        aSpaceID,
                                           sdpSegHandle   * aSegHandle,
                                           UChar          * aDataPagePtr )
{
    sdrMtxStartInfo  sStartInfo;
    sdpstStack       sRevStack;
    sdpParentInfo    sParentInfo;
    sdpstPBS         sNewPBS;
    sdrMtx           sMtx;
    SInt             sState = 0;

    IDE_DASSERT( aSegHandle   != NULL );
    IDE_DASSERT( aDataPagePtr != NULL );
    IDE_DASSERT( aMtx         != NULL );

    sParentInfo = sdpPhyPage::getParentInfo( aDataPagePtr );

    // Data �������� �Ҵ��� �Ұ����� �����̸�, Leaf bitmap ��������
    // X-latch�� ȹ���ϰ� ���������¸� FULL�� �����Ѵ�.
    // ����� ���� Data �������� ���ؼ��� freeness ���¸� �������ش�.
    sNewPBS = (sdpstPBS) (SDPST_BITSET_PAGETP_DATA | SDPST_BITSET_PAGEFN_FUL );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, aDataPagePtr )
              != IDE_SUCCESS );

    sdpPhyPage::setState( (sdpPhyPageHdr*)aDataPagePtr,
                          (UShort)sNewPBS,
                          &sMtx );

    // leaf bmp ���������� MFNL�� ����õ��Ͽ� �������� �����Ѵ�.
    sdpstStackMgr::initialize( &sRevStack );

    IDE_TEST( tryToChangeMFNLAndItHint(
                        aStatistics,
                        &sMtx,
                        aSpaceID,
                        aSegHandle,
                        SDPST_CHANGEMFNL_LFBMP_PHASE,
                        sParentInfo.mParentPID,     // leafbmp pid
                        sParentInfo.mIdxInParent,
                        (void*)&sNewPBS,
                        SD_NULL_PID,            // unuse : parent pid
                        SDPST_INVALID_SLOTNO,  // unuse : slotidxinparent
                        &sRevStack ) != IDE_SUCCESS );

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


/***********************************************************************
 * Description : [ INTERFACE ] ������ ������� �Ҵ� Ȥ�� ���� ���� ���뵵
 *               ���� freepage ������ ������ ó���Ѵ�.
 *
 *  aStatistics   - [IN] ��� ����
 *  aMtx          - [IN] Mini Transaction Pointer
 *  aSpaceID      - [IN] TableSpace ID
 *  aSegHandle    - [IN] Segment Handle
 *  aPagePtr      - [IN] insert, update, delete�� �߻��� Data Page Ptr
 *
 ***********************************************************************/
IDE_RC sdpstAllocPage::updatePageState( idvSQL             * aStatistics,
                                        sdrMtx             * aMtx,
                                        scSpaceID            aSpaceID,
                                        sdpSegHandle       * aSegHandle,
                                        UChar              * aPagePtr )
{
    IDE_DASSERT( aSegHandle  != NULL );
    IDE_DASSERT( aPagePtr    != NULL );

    IDE_TEST( updatePageFN( aStatistics,
                            aMtx,
                            aSpaceID,
                            aSegHandle,
                            aPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ���� bitmap �������� MFNL�� ��ȯ�Ѵ�.
 ***********************************************************************/
sdpstMFNL sdpstAllocPage::calcMFNL( UShort  * aMFNLtbl )
{
    sdpstMFNL sMFNL = SDPST_MFNL_FUL;
    SInt      sLoop;

    IDE_DASSERT( aMFNLtbl != NULL );

    for (sLoop = (SInt)SDPST_MFNL_UNF; sLoop >= (SInt)SDPST_MFNL_FUL; sLoop--)
    {
        if ( aMFNLtbl[sLoop] > 0 )
        {
            sMFNL = (sdpstMFNL)sLoop;
            break;
        }
    }
    return sMFNL;
}

/***********************************************************************
 * Description : data page���� ������ ������ �´� stack�� ������
 ***********************************************************************/
IDE_RC sdpstAllocPage::makeOrderedStackFromDataPage( idvSQL       * aStatistics,
                                                     scSpaceID      aSpaceID,
                                                     scPageID       aSegPID,
                                                     scPageID       aDataPageID,
                                                     sdpstStack   * aOrderedStack )
{
    UChar             * sPagePtr;
    SShort              sRtBMPIdx;
    sdpstStack          sRevStack;
    sdpParentInfo       sParentInfo;
    sdpstBMPHdr       * sBMPHdr;
    sdpstPosItem        sCurPos = { 0, 0, {0, 0} };
    scPageID            sCurRtBMP;
    UInt                sLoop;
    scPageID            sCurPID;
    UInt                sState = 0 ;

    IDE_DASSERT( aSegPID != SD_NULL_PID );
    IDE_DASSERT( aDataPageID != SD_NULL_PID );
    IDE_DASSERT( aOrderedStack != NULL );
    IDE_DASSERT( sdpstStackMgr::getDepth( aOrderedStack )
                == SDPST_EMPTY );

    sdpstStackMgr::initialize( &sRevStack );

    sCurPID = aDataPageID;

    /* sLoop�� ���� BMP �� �ǹ��Ѵ�.
     * sLoop��  SDPST_LFBMP -> SDPST_RTBMP ������ ����Ǵ� ���� fix�ϴ� ��������
     * Data page -> It-BMP ������ ����ȴ�.  */
    for ( sLoop = (UInt)SDPST_LFBMP; sLoop >= SDPST_RTBMP; sLoop-- )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                              aSpaceID,
                                              sCurPID,
                                              &sPagePtr ) != IDE_SUCCESS );
        sState = 1;

        if ( sLoop == (UInt)SDPST_LFBMP )
        {
            sParentInfo = sdpPhyPage::getParentInfo( sPagePtr );

            sCurPos.mNodePID = sParentInfo.mParentPID;
            sCurPos.mIndex   = sParentInfo.mIdxInParent;
        }
        else
        {
            sBMPHdr = sLoop == (UInt)SDPST_ITBMP ?
                      sdpstLfBMP::getBMPHdrPtr( sPagePtr ) :
                      sdpstBMP::getHdrPtr( sPagePtr );

            sCurPos.mNodePID = sBMPHdr->mParentInfo.mParentPID;
            sCurPos.mIndex   = sBMPHdr->mParentInfo.mIdxInParent;
        }

        sdpstStackMgr::push( &sRevStack, &sCurPos );

        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );

        sCurPID = sCurPos.mNodePID;
    }

    /*
     * RtBMP�� ������ ã�´�.
     * ������� ���� �Ǹ�, sCurPID���� RtBMP�� PID�� ����ְ� �ȴ�.
     * Segment Header���� �����Ͽ� sCurPID �� rt-BMP�� ���°�� ��ġ�� �ִ���
     * ã�´�.
     */

    sRtBMPIdx  = SDPST_INVALID_SLOTNO;
    sCurRtBMP  = aSegPID;
    IDE_ASSERT( sCurRtBMP != SD_NULL_PID );

    while( sCurRtBMP != SD_NULL_PID )
    {
        sRtBMPIdx++;
        if ( sCurPID == sCurRtBMP )
        {
            break; // found it
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                              aSpaceID,
                                              sCurRtBMP,
                                              &sPagePtr ) != IDE_SUCCESS );
        sState = 1;

        sBMPHdr   = sdpstBMP::getHdrPtr( sPagePtr );
        sCurRtBMP = sdpstRtBMP::getNxtRtBMP( sBMPHdr );

        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );
    }

    sdpstStackMgr::push( aOrderedStack, SD_NULL_PID, sRtBMPIdx );

    while( sdpstStackMgr::getDepth( &sRevStack ) != SDPST_EMPTY )
    {
        sCurPos = sdpstStackMgr::pop( &sRevStack );
        sdpstStackMgr::push( aOrderedStack, &sCurPos );
    }

    if ( sdpstStackMgr::getDepth( aOrderedStack ) != SDPST_LFBMP )
    {
        ideLog::log( IDE_SERVER_0, "Ordered Stack depth is invalid\n" );
        ideLog::log( IDE_SERVER_0, "========= Ordered Stack Dump ========\n" );
        sdpstStackMgr::dump( aOrderedStack );
        IDE_ASSERT( 0 );
    }


    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �־��� Extent�� ���� Data Page�� format �Ѵ�.
 *
 * aStatistics      - [IN] �������
 * aStartInfo       - [IN] sdrStartInfo
 * aSpaceID         - [IN] Tablespace ID
 * aSegHandle       - [IN] Segment Handle
 * aExtDesc         - [IN] Extent Desc
 * aBeginPID        - [IN] format�� ������ PID
 * aBeginSeqNo      - [IN] format�� ù��° Page�� SeqNo
 * aBeginLfBMP      - [IN] format�� �������� �����ϴ� ù��° LfBMP
 * aBeginPBSNo      - [IN] format�� �������� PBSNo
 * aLfBMPHdr        - [IN] format�� �������� �����ϴ� ù��° LfBMP Header
 ***********************************************************************/
IDE_RC sdpstAllocPage::formatDataPagesInExt(
                                        idvSQL              * aStatistics,
                                        sdrMtxStartInfo     * aStartInfo,
                                        scSpaceID             aSpaceID,
                                        sdpSegHandle        * aSegHandle,
                                        sdpstExtDesc        * aExtDesc,
                                        sdRID                 aExtRID,
                                        scPageID              aBeginPID )
{
    sdrMtx              sMtx;
    UChar             * sPagePtr;
    scPageID            sCurPID;
    sdpstPBS            sPBS;
    SShort              sPBSNo;
    scPageID            sLfBMP;
    scPageID            sPrvLfBMP;
    ULong               sSeqNo;
    sdpParentInfo       sParentInfo;
    sdpstRangeMap     * sRangeMap = NULL;
    UChar             * sLfBMPPagePtr = NULL;
    sdpstLfBMPHdr     * sLfBMPHdr = NULL;
    UInt                sState = 0;
    UInt                sLfBMPState = 0;
    sdpstPageRange      sPageRange;

    IDE_ASSERT( aStartInfo  != NULL );
    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aExtDesc    != NULL );
    IDE_ASSERT( aExtRID     != SD_NULL_RID );
    IDE_ASSERT( aBeginPID   != SD_NULL_PID );

    sPBS = SDPST_BITSET_PAGETP_DATA | SDPST_BITSET_PAGEFN_FMT;

    /* BeginPID�� �־��� Extent�� ���ϴ��� Ȯ���Ѵ�. */
    if ( (aExtDesc->mExtFstPID > aBeginPID) &&
         (aExtDesc->mExtFstPID + aExtDesc->mLength - 1 < aBeginPID) )
    {
        IDE_CONT( finish_reformat_data_pages );
    }

    IDE_TEST( sdpstExtDir::makeSeqNo( aStatistics,
                                      aSpaceID,
                                      aSegHandle->mSegPID,
                                      aExtRID,
                                      aBeginPID,
                                      &sSeqNo ) != IDE_SUCCESS );

    IDE_TEST( sdpstExtDir::calcLfBMPInfo( aStatistics,
                                          aSpaceID,
                                          aExtDesc,
                                          aBeginPID,
                                          &sParentInfo ) != IDE_SUCCESS );

    sPrvLfBMP = SD_NULL_PID;
    sLfBMP    = sParentInfo.mParentPID;
    sPBSNo    = sParentInfo.mIdxInParent;

    /* format ���� */
    for ( sCurPID = aBeginPID;
          sCurPID < aExtDesc->mExtFstPID + aExtDesc->mLength;
          sCurPID++ )
    {
        if ( sPrvLfBMP != sLfBMP )
        {
            if ( sLfBMPState == 1 )
            {
                sLfBMPState = 0;
                IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                     sLfBMPPagePtr )
                          != IDE_SUCCESS );
            }

            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  aSpaceID,
                                                  sLfBMP,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  NULL, /* sdrMtx */
                                                  &sLfBMPPagePtr,
                                                  NULL, /* aTrySuccess */
                                                  NULL  /* aIsCorruptPage */ )
                      != IDE_SUCCESS );
            sLfBMPState = 1;

            sPrvLfBMP = sLfBMP;

            sLfBMPHdr = sdpstLfBMP::getHdrPtr( sLfBMPPagePtr );
            sRangeMap = sdpstLfBMP::getMapPtr( sLfBMPHdr );
        }
        else
        {
            /* ������ ���Ҵ� ������ ����, �� Bmp�� ������ ���� */
            IDE_ERROR( sLfBMPHdr != NULL );
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_TRUE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT) != IDE_SUCCESS );
        sState = 1;

        /* Meta �������� format �ϸ� �ȵȴ�. */
        if ( (sRangeMap->mPBSTbl[sPBSNo] & SDPST_BITSET_PAGETP_MASK)
              == SDPST_BITSET_PAGETP_DATA )
        {
            IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                                  &sMtx,
                                                  aSpaceID,
                                                  aSegHandle,
                                                  sCurPID,
                                                  sSeqNo,
                                                  SDP_PAGE_FORMAT,
                                                  sLfBMP,
                                                  sPBSNo,
                                                  sPBS,
                                                  &sPagePtr ) != IDE_SUCCESS );
        }

        sPageRange = sLfBMPHdr->mPageRange;

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        sPBSNo++;
        sSeqNo++;

        /* �� Extent�� ���� LfBMP�� ���� ������ �� �ִ�.
         * Extent�� ���� ���������� format�ϱ� ���� ParentInfo�� ����س���. */
        if ( sPBSNo == sPageRange )
        {
            sLfBMP++;
            sPBSNo = 0;
        }

        if ( sPBSNo > sPageRange )
        {
            ideLog::log( IDE_SERVER_0,
                         "aExtRID: %llu, "
                         "aBeginPID: %u\n",
                         aExtRID,
                         aBeginPID );

            ideLog::log( IDE_SERVER_0,
                         "sPrvLfBMP: %u\n"
                         "sLfBMP: %u\n"
                         "sPBSNo: %d\n"
                         "sSeqNo: %llu\n"
                         "sPageRange: %u\n",
                         sPrvLfBMP,
                         sLfBMP,
                         sPBSNo,
                         sSeqNo,
                         sPageRange );

            if( sLfBMPPagePtr != NULL )
            {
                sdpstLfBMP::dump( sLfBMPPagePtr );
            }
            else
            {
                /* nothing to do ... */
            }

            IDE_ASSERT( 0 );
        }
    }

    if ( sLfBMPState == 1 )
    {
        sLfBMPState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sLfBMPPagePtr )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( finish_reformat_data_pages );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sLfBMPState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, sLfBMPPagePtr )
                    == IDE_SUCCESS );
    }

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}
