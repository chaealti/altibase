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
 * $Id: sdpstFindPage.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment���� ������� Ž�� ���� ���� STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <smErrorCode.h>

# include <sdpReq.h>
# include <sdpPhyPage.h>

# include <sdpstDef.h>
# include <sdpstCache.h>

# include <sdpstFindPage.h>
# include <sdpstRtBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstSegDDL.h>
# include <sdpstAllocPage.h>

# include <sdpstItBMP.h>
# include <sdpstStackMgr.h>

/***********************************************************************
 * Description : [ INTERFACE ] ������� �Ҵ� ������ Table Data ��������
 *               Ž���Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::findInsertablePage( idvSQL           * aStatistics,
                                          sdrMtx           * aMtx,
                                          scSpaceID          aSpaceID,
                                          sdpSegHandle     * aSegHandle,
                                          void             * aTableInfo,
                                          sdpPageType        aPageType,
                                          UInt               aRowSize,
                                          idBool          /* aNeedKeySlot*/,
                                          UChar           ** aPagePtr,
                                          UChar            * aCTSlotNo )
{
    sdpstSegCache     * sSegCache;
    smOID               sTableOID;
    scPageID            sHintDataPID = SD_NULL_PID;
    scPageID            sNewHintDataPID = SD_NULL_PID;
    UChar             * sPagePtr   = NULL;
    UChar               sCTSlotNo = SDP_CTS_IDX_NULL;

    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aPagePtr   != NULL );
    IDE_ASSERT( aCTSlotNo != NULL );

    sSegCache = (sdpstSegCache*)((sdpSegHandle*)aSegHandle)->mCache;
    sTableOID = sdpstCache::getTableOID( sSegCache );

    if ( smuProperty::getTmsIgnoreHintPID() == 0 )
    {
        /* Hint DataPID�� ���� Trans���� �����´�.
         * ���� Trans�� ���� ��� SegCache���� �����´�. */
        if ( (sTableOID != SM_NULL_OID) && (aTableInfo != NULL) )
        {
            smLayerCallback::getHintDataPIDofTableInfo( aTableInfo, &sHintDataPID );
        }

        if ( sHintDataPID == SD_NULL_PID )
        {
            sdpstCache::getHintDataPage( aStatistics,
                                         sSegCache,
                                         &sHintDataPID );
        }

        IDE_TEST( checkAndSearchHintDataPID( aStatistics,
                                             aMtx,
                                             aSpaceID,
                                             aSegHandle,
                                             aTableInfo,
                                             sHintDataPID,
                                             aRowSize,
                                             &sPagePtr,
                                             &sCTSlotNo,
                                             &sNewHintDataPID ) != IDE_SUCCESS );
    }

    if ( sPagePtr == NULL )
    {
        IDE_TEST( searchFreeSpace( aStatistics,
                                   aMtx,
                                   aSpaceID,
                                   aSegHandle,
                                   aRowSize,
                                   aPageType,
                                   SDPST_SEARCH_NEWSLOT,
                                   &sPagePtr,
                                   &sCTSlotNo,
                                   &sNewHintDataPID ) != IDE_SUCCESS );

        IDE_ASSERT( sPagePtr != NULL );
    }

    if ( sNewHintDataPID != sHintDataPID )
    {
        if ( (sTableOID != SM_NULL_OID) && (aTableInfo != NULL) )
        {
            smLayerCallback::setHintDataPIDofTableInfo( aTableInfo, sNewHintDataPID );
        }

        sdpstCache::setHintDataPage( aStatistics,
                                     sSegCache,
                                     sNewHintDataPID );
    }

    *aPagePtr   = sPagePtr;
    *aCTSlotNo = sCTSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( (sTableOID != SM_NULL_OID) && (aTableInfo != NULL) )
    {
        if ( sPagePtr == NULL )
        {
            // Hint Data �������� ���ؼ� ��������Ҵ��� �Ұ����Ͽ�
            // Hint Data �������� �ʱ�ȭ�Ѵ�.
            smLayerCallback::setHintDataPIDofTableInfo( aTableInfo, SD_NULL_PID );
        }
    }

    *aPagePtr   = NULL;
    *aCTSlotNo = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Search ������ ���� ��������� Ž���Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::searchFreeSpace( idvSQL            * aStatistics,
                                       sdrMtx            * aMtx,
                                       scSpaceID           aSpaceID,
                                       sdpSegHandle      * aSegHandle,
                                       UInt                aRowSize,
                                       sdpPageType         aPageType,
                                       sdpstSearchType     aSearchType,
                                       UChar            ** aPagePtr,
                                       UChar             * aCTSlotNo,
                                       scPageID          * aNewHintDataPID )
{
    UChar             * sPagePtr  = NULL;
    UInt                sLoop;
    sdpstSegCache     * sSegCache;
    sdpstPosItem        sArrLfBMP[SDPST_MAX_CANDIDATE_LFBMP_CNT];
    UInt                sCandidateLfCount = 0;
    sdpstStack          sStack;
    sdpstPosItem        sCurrPos;
    idBool              sGoNxtIt        = ID_FALSE;
    scPageID            sNewHintDataPID = SD_NULL_PID;
    UChar               sCTSlotNo       = SDP_CTS_IDX_NULL;
    UChar             * sSegHdrPagePtr;
    sdpstSegHdr       * sSegHdr;
    sdrMtx              sMtx;
    UInt                sState = 0;
    sdrMtxStartInfo     sStartInfo;
    UInt                sSearchItBMPCnt = 0;
    idBool              sNeedToExtendExt = ID_FALSE;

    IDE_ASSERT( aSegHandle      != NULL );
    IDE_ASSERT( aMtx            != NULL );
    IDE_ASSERT( aPagePtr        != NULL );
    IDE_ASSERT( aNewHintDataPID != NULL );

    if ( aPageType != SDP_PAGE_DATA )
    {
        IDE_ASSERT( aSearchType == SDPST_SEARCH_NEWPAGE );
    }

    sSegCache  = (sdpstSegCache*)(aSegHandle->mCache);
    sdpstCache::copyItHint( aStatistics, sSegCache, aSearchType, &sStack );

    sSearchItBMPCnt    = 0;

    while ( 1 )
    {
        if ( sGoNxtIt == ID_TRUE )
        {
            /* BUG-29038
             * Ž���� ItBMP ������ �ʰ��ϰų� Ȯ���� �ʿ��Ҷ� Ȯ���Ͽ�
             * Ȯ���� Extent�� ù��° �������� �ٷ� �Ҵ��Ѵ�. */
            if ( sSearchItBMPCnt <= SDPST_SEARCH_QUOTA_IN_RTBMP )
            {
                IDE_TEST( searchSpaceInRtBMP( aStatistics,
                                              aMtx,
                                              aSpaceID,
                                              aSegHandle,
                                              &sStack,
                                              aSearchType,
                                              &sNeedToExtendExt )
                          != IDE_SUCCESS );
            }
            else
            {
                sNeedToExtendExt = ID_TRUE;
            }

            if ( sNeedToExtendExt == ID_TRUE )
            {
                IDE_TEST( tryToAllocExtsAndPage( aStatistics,
                                                 aMtx,
                                                 aSpaceID,
                                                 aSegHandle,
                                                 aSearchType,
                                                 aPageType,
                                                 &sStack,
                                                 &sPagePtr ) != IDE_SUCCESS );

                IDU_FIT_POINT( "1.BUG-30667@sdpstFindPage::searchFreeSpace" );

                /* Extent Ȯ���� ���� Ʈ������� ���ÿ� �����ϰ� �Ǹ�
                 * �� Ʈ����Ǹ� Ȯ���� �����ϰ� �ٸ� Ʈ������� ����ϴٰ�
                 * Ȯ���� �Ϸ�Ǹ� �ƹ��͵� ���ϰ�, Ž���� ItHint�� sStack�� 
                 * �����ϰ� ������ �ȴ�.
                 * ���� �� ��� sPagePtr�� NULL�� ���ϵǴµ�, �̶� �ش�
                 * ItHint���� Ž���� �� �����Ѵ�. */
                if ( sPagePtr != NULL )
                {
                    break;
                }
            }
        }

        sCurrPos = sdpstStackMgr::getCurrPos( &sStack );

        if ( sdpstStackMgr::getDepth( &sStack ) != SDPST_ITBMP )
        {
            ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

            ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
            sdpstStackMgr::dump( &sStack );

            ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
            sdpstCache::dump( sSegCache );

            IDE_ASSERT( 0 );
        }

        /* Ž���� ItBMP ������ �����Ѵ�. */
        sSearchItBMPCnt++;

        IDE_TEST( searchSpaceInItBMP(
                               aStatistics,
                               aMtx,
                               aSpaceID,
                               sCurrPos.mNodePID,
                               aSearchType,
                               sSegCache,
                               &sGoNxtIt,
                               sArrLfBMP,
                               &sCandidateLfCount ) != IDE_SUCCESS );

        if ( sGoNxtIt == ID_TRUE )
        {
            // itbmp �������� �������� �ʴٸ�, �ٽ� Root bmp ������������
            // Ž���������� ���ư���.
            sdpstStackMgr::pop( &sStack );
            continue;
        }
        else
        {
            IDE_ASSERT( sCandidateLfCount > 0 );
        }

        if ( sdpstStackMgr::getDepth( &sStack ) != SDPST_ITBMP )
        {
            ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

            ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
            sdpstStackMgr::dump( &sStack );

            ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
            sdpstCache::dump( sSegCache );

            IDE_ASSERT( 0 );
        }

        for ( sLoop = 0; sLoop < sCandidateLfCount; sLoop++ )
        {
            /* �ĺ� Data �������� Stack�� ������ ���� HWM
             * �����ϱ� ���ؼ��� �����ϸ�, traverse�� ���ؼ��� ����
             * ���� �ʴ´�. �׷��Ƿ�, ��������� Stack���� Lf BMP
             * ���� ������ ����Ǿ� ������, Lf BMP ���������� Data
             * �������� Create ������ �߻��� ��쿡��, Data ��������
             * ������ Stack�� ����ȴ�. */

            /* internal depth�� ���� Slot Index�� �缳���Ѵ�. */
            sdpstStackMgr::setCurrPos( &sStack,
                                       sCurrPos.mNodePID,
                                       sArrLfBMP[sLoop].mIndex );

            IDE_TEST( searchSpaceInLfBMP( aStatistics,
                                          aMtx,
                                          aSpaceID,
                                          aSegHandle,
                                          sArrLfBMP[ sLoop ].mNodePID,
                                          &sStack,
                                          aPageType,
                                          aSearchType,
                                          aRowSize,
                                          &sPagePtr,
                                          &sCTSlotNo ) != IDE_SUCCESS );

            IDU_FIT_POINT( "1.BUG-30667@sdpstFindPage::searchFreeSpace" );

            if ( sPagePtr != NULL )
            {
                sNewHintDataPID = sdpPhyPage::getPageID( sPagePtr );
                break;
            }
        }

        if ( sPagePtr == NULL ) // Not Found !!
        {
            /* ������ Data �������� Ž������ ���ߴٸ�,
             * ItBMP �������� ���� RtBMP ���������� �ٽ� Ž���� �����Ѵ�. */
            sdpstStackMgr::pop( &sStack );
            sGoNxtIt = ID_TRUE;
        }
        else
        {
            break;
        }
    }

    *aNewHintDataPID = sNewHintDataPID;
    *aPagePtr        = sPagePtr;
    *aCTSlotNo       = sCTSlotNo;

    /* lstAllocPage�� �����Ѵ�. */
    sdpstCache::setLstAllocPage4AllocPage( aStatistics,
                                 aSegHandle,
                                 sdpPhyPage::getPageID( sPagePtr ),
                                 sdpPhyPage::getSeqNo( 
                                        sdpPhyPage::getHdr(sPagePtr) ) );

    /* Index Page�� ��� free index Page ������ �����Ѵ� */
    if ( aPageType != SDP_PAGE_DATA )
    {
        sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       &sStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS ); 
        sState = 1;

        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              aSegHandle->mSegPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              &sMtx,
                                              &sSegHdrPagePtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );

        sSegHdr = sdpstSH::getHdrPtr( sSegHdrPagePtr );

        IDE_TEST( sdpstSH::setFreeIndexPageCnt( &sMtx,
                                                sSegHdr,
                                                sSegHdr->mFreeIndexPageCnt - 1 )
                  != IDE_SUCCESS );


        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    *aNewHintDataPID = SD_NULL_PID;
    *aPagePtr        = NULL;
    *aCTSlotNo       = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Ȯ���ϰ�, Ȯ���ߴٸ� Ȯ���� Extent�� ù��° data��������
 * X-Latch�� ��� �����Ѵ�.
 *
 * �ٸ� Trans���� �̹� Ȯ�����ΰ�� Ȯ������ �ʴ� ��쵵 �ִ�.
 * �̶� Ȯ���� ������ ItBMP�� aStack�� �����ȴ�.
 * ��, ��� Ž���� �����ϸ� �ȴ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::tryToAllocExtsAndPage( idvSQL          * aStatistics,
                                             sdrMtx          * aMtx4Latch,
                                             scSpaceID         aSpaceID,
                                             sdpSegHandle    * aSegHandle,
                                             sdpstSearchType   aSearchType,
                                             sdpPageType       aPageType,
                                             sdpstStack      * aStack,
                                             UChar          ** aFstDataPagePtr )
{
    sdrMtxStartInfo     sStartInfo;
    sdpstPosItem        sPosItem;
    sdpstWM             sHWM;
    sdpstSegCache     * sSegCache;
    sdrMtx              sInitMtx;
    UChar             * sPagePtr = NULL;
    sdpstPBS            sPBS;
    sdpstStack          sRevStack;
    sdpParentInfo       sParentInfo;
    sdRID               sAllocFstExtRID;
    sdpstStack          sPrvItHint;
    UInt                sState = 0;

    IDE_ASSERT( aMtx4Latch      != NULL );
    IDE_ASSERT( aSegHandle      != NULL );
    IDE_ASSERT( aFstDataPagePtr != NULL );

    sSegCache = (sdpstSegCache*)aSegHandle->mCache;

    // ���� Root bmp �������� ������ itbmp �������� ��������
    // �ʱ� ������ Segment Ȯ���� �����Ѵ�.
    sdrMiniTrans::makeStartInfo( aMtx4Latch, &sStartInfo );

    IDE_TEST( sdpstSegDDL::allocNewExtsAndPage(
                                   aStatistics,
                                   &sStartInfo,
                                   aSpaceID,
                                   aSegHandle,
                                   aSegHandle->mSegStoAttr.mNextExtCnt,
                                   ID_TRUE, /* aNeedToUpdateHWM */
                                   &sAllocFstExtRID,
                                   aMtx4Latch,
                                   &sPagePtr )
              != IDE_SUCCESS );

    /* Extent�� ���� Ȯ���� ��� First Data Page�� �����Ͱ� ���ϵȴ�.
     * �ش� �������� �ʱ�ȭ�Ѵ�. */
    if ( sPagePtr != NULL )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sInitMtx,
                                       &sStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sInitMtx, sPagePtr )
                  != IDE_SUCCESS );
        
        IDE_TEST( sdpPhyPage::setPageType( sdpPhyPage::getHdr( sPagePtr ),
                                           aPageType,
                                           &sInitMtx ) != IDE_SUCCESS );

        sdbBufferMgr::setPageTypeToBCB( (UChar*)sPagePtr,
                                        (UInt)aPageType );
        
        if ( aPageType == SDP_PAGE_DATA )
        {
            IDE_TEST( smLayerCallback::logAndInitCTL( &sInitMtx,
                                                      sdpPhyPage::getHdr( sPagePtr ),
                                                      aSegHandle->mSegAttr.mInitTrans,
                                                      aSegHandle->mSegAttr.mMaxTrans )
                      != IDE_SUCCESS );

            IDE_TEST( sdpSlotDirectory::logAndInit(
                                        sdpPhyPage::getHdr(sPagePtr),
                                        &sInitMtx )
                    != IDE_SUCCESS );

        }
        else
        {
            sParentInfo = sdpPhyPage::getParentInfo( sPagePtr );

            sPBS = (sdpstPBS)
                   (SDPST_BITSET_PAGETP_DATA|SDPST_BITSET_PAGEFN_FUL);
            IDE_TEST( sdpPhyPage::setState( sdpPhyPage::getHdr(sPagePtr),
                                            sPBS,
                                            &sInitMtx )
                      != IDE_SUCCESS );

            // Table Page�� �ƴѰ�쿡�� �̸� bitmap�� �����Ѵ�.
            sdpstStackMgr::initialize(&sRevStack);

            // leaf bmp ���������� MFNL�� ����õ��Ͽ�
            // �������� �����Ѵ�.
            IDE_TEST( sdpstAllocPage::tryToChangeMFNLAndItHint(
                          aStatistics,
                          &sInitMtx,
                          aSpaceID,
                          aSegHandle,
                          SDPST_CHANGEMFNL_LFBMP_PHASE,
                          sParentInfo.mParentPID,  // leaf pid
                          sParentInfo.mIdxInParent,
                          (void*)&sPBS,
                          SD_NULL_PID,            // unuse
                          SDPST_INVALID_SLOTNO,  // unuse
                          &sRevStack ) != IDE_SUCCESS );
        }
        
        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sInitMtx ) != IDE_SUCCESS );

        *aFstDataPagePtr = sPagePtr;
    }

    /* Ȯ��� ���Ŀ��� ������ ItBMP�� Hint�� �����ϰ�,
     * �ش� ItBMP���� Ž���� �����Ѵ�.
     * ������ ItBMP�� HWM�� �����Ǿ� �ִ�. */
    sdpstCache::copyHWM( aStatistics, sSegCache, &sHWM );

    /* ���� stack�� �ʱ�ȭ�ϰ�, ������ ItBMP�� Ž�� ��ġ��
     * �����Ѵ�. */
    sdpstStackMgr::initialize( aStack );

    if ( sdpstStackMgr::getDepth( aStack ) != SDPST_EMPTY )
    {
        ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

        ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
        sdpstStackMgr::dump( aStack );

        ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
        sdpstCache::dump( sSegCache );

        IDE_ASSERT( 0 );
    }


    sPosItem = sdpstStackMgr::getSeekPos( &sHWM.mStack, SDPST_VIRTBMP);
    sdpstStackMgr::push( aStack, &sPosItem );

    sPosItem = sdpstStackMgr::getSeekPos( &sHWM.mStack, SDPST_RTBMP);
    sdpstStackMgr::push( aStack, &sPosItem );

    sPosItem = sdpstStackMgr::getSeekPos( &sHWM.mStack, SDPST_ITBMP );
    sPosItem.mIndex = 0;
    sdpstStackMgr::push( aStack, &sPosItem );

    if ( sdpstStackMgr::getDepth( aStack ) != SDPST_ITBMP )
    {
        ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

        ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
        sdpstStackMgr::dump( aStack );

        ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
        sdpstCache::dump( sSegCache );

        IDE_ASSERT( 0 );
    }

    /* ���� ItHint�� ��������, */
    sdpstCache::copyItHint( aStatistics,
                            sSegCache,
                            aSearchType,
                            &sPrvItHint );

    /* ������ ItBMP�� Hint�� �����Ѵ�. */
    sdpstCache::setItHintIfGT( aStatistics,
                               sSegCache,
                               aSearchType,
                               aStack );

    /* ���� ItHint�� ���Ͽ� Hint�� �մ�����ٸ�
     * rescan flag�� ���ش�. */
    if ( sdpstStackMgr::compareStackPos( &sPrvItHint, aStack ) < 0 )
    {
        if ( aSearchType == SDPST_SEARCH_NEWSLOT )
        {
            sdpstCache::setUpdateHint4Slot( sSegCache, ID_TRUE );
        }
        else
        {
            IDE_ASSERT( aSearchType == SDPST_SEARCH_NEWPAGE );
            sdpstCache::setUpdateHint4Page( sSegCache, ID_TRUE );
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sInitMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Root BMP ������������ ������� Ž���� �����Ѵ�.
 *
 * Root BMP �������� ��������� ���� Internal BMP �������� ������ ����
 * �ʴٸ�, It Hint�� �������־�� �Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::searchSpaceInRtBMP( idvSQL            * aStatistics,
                                          sdrMtx            * aMtx,
                                          scSpaceID           aSpaceID,
                                          sdpSegHandle      * aSegHandle,
                                          sdpstStack        * aStack,
                                          sdpstSearchType     aSearchType,
                                          idBool            * aNeedToExtendExt )
{
    sdrSavePoint        sInitSP;
    scPageID            sNxtItBMP;
    scPageID            sNxtRtBMP;
    SShort              sSlotNoInParent;
    sdpstBMPHdr       * sBMPHdr;
    UChar             * sPagePtr;
    sdpstPosItem        sPosItem;
    scPageID            sCurRtBMP;
    SShort              sNewSlotNoInParent;
    sdpstSegCache     * sSegCache;
    sdpstWM             sHWM;
    UInt                sState = 0;

    IDE_DASSERT( aMtx             != NULL );
    IDE_DASSERT( aSegHandle       != NULL );
    IDE_DASSERT( aStack           != NULL );
    IDE_DASSERT( aNeedToExtendExt != NULL );

    sSegCache = (sdpstSegCache*)aSegHandle->mCache;
    sNxtRtBMP = SD_NULL_PID;
    *aNeedToExtendExt = ID_FALSE;

    while( 1 )
    {
        if ( sdpstStackMgr::getDepth( aStack ) != SDPST_RTBMP )
        {
            ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

            ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
            sdpstStackMgr::dump( aStack );

            ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
            sdpstCache::dump( sSegCache );

            IDE_ASSERT( 0 );
        }

        sdpstCache::copyHWM( aStatistics, sSegCache, &sHWM );

        sPosItem        = sdpstStackMgr::getCurrPos( aStack );
        sCurRtBMP       = sPosItem.mNodePID;
        sSlotNoInParent = sPosItem.mIndex;

        IDE_ASSERT( sCurRtBMP != SD_NULL_PID );

        sdrMiniTrans::setSavePoint( aMtx, &sInitSP );

        sState = 0;
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sCurRtBMP,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPagePtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );
        sState = 1;

        sBMPHdr = sdpstBMP::getHdrPtr( sPagePtr );

        /* RtBMP���� ������ Ž���� ItBMP�� ���Ѵ�.
         * ���׸�Ʈ ���� Ȯ�� �������� �� ItBMP ���Ŀ� ��������� �ִ�
         * ItBMP�� ã�´�. Ȯ�� �Ŀ��� Ȯ��� Extent�� ���� ItBMP�� ������
         * �� �ֱ� ������ �� ItBMP���� Ž���� �����Ѵ�. */
        sdpstRtBMP::findFreeItBMP( sBMPHdr,
                                   sSlotNoInParent + 1,
                                   SDPST_SEARCH_TARGET_MIN_MFNL(aSearchType),
                                   &sHWM,
                                   &sNxtItBMP,
                                   &sNewSlotNoInParent );

        if ( sNxtItBMP != SD_NULL_PID )
        {
            sState = 0;
            // ȹ���ߴ� root bmp �������� unlatch �Ѵ�.
            IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                      != IDE_SUCCESS );

            // Internal Bitmap ������������ Ž���������� �Ѿ��.
            // Stack�� �ִ´�.
            sdpstStackMgr::setCurrPos( aStack,
                                       sCurRtBMP,
                                       sNewSlotNoInParent );

            sdpstStackMgr::push( aStack, sNxtItBMP, 0 );

            if ( sdpstStackMgr::getDepth( aStack ) != SDPST_ITBMP )
            {
                ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

                ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
                sdpstStackMgr::dump( aStack );

                ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
                sdpstCache::dump( sSegCache );

                IDE_ASSERT( 0 );
            }

            break; // Found it !!
        }

        // ������ itbmp �������� �������� �ʴ´ٸ� ����
        // rtbmp ���������� ���� itbmp �������� �Ѿ��.
        sNxtRtBMP          = sdpstRtBMP::getNxtRtBMP(sBMPHdr);
        sNewSlotNoInParent = 0; // Slot ������ ù��°�� �����Ѵ�.

        // ȹ���ߴ� root bmp �������� unlatch �Ѵ�.
        sState = 0;
        IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                  != IDE_SUCCESS );

        if ( sNxtRtBMP == SD_NULL_PID )
        {
            /* ���� ItBMP�� delete�� �߻��Ͽ� �ش� ItBMP�� MFNL�� ���ŵǴ�
             * ��� UpdateItHint Flag�� True�� �ȴ�.
             * ���̻� ���� ������ ���� ��� �ش� �÷��׸� ����, ������ �������
             * �� �ִ� ��� �ش� ItBMP�� Hint�� �����ϰ� Ž���� �����Ѵ�. */
            if ( sdpstCache::needToUpdateItHint( sSegCache,
                                                 aSearchType ) == ID_TRUE )
            {
                IDE_TEST( sdpstRtBMP::rescanItHint( aStatistics,
                                                    aSpaceID,
                                                    aSegHandle->mSegPID,
                                                    aSearchType,
                                                    sSegCache,
                                                    aStack ) != IDE_SUCCESS );
                /* aStack�� ItBMP Hint�� �����Ǿ���. ItBMP���� Ž���� ����
                 * �Ѵ�. */
                if ( sdpstStackMgr::getDepth( aStack ) != SDPST_ITBMP )
                {
                    ideLog::log( IDE_SERVER_0, "Search Stack is invalid\n" );

                    ideLog::log( IDE_SERVER_0, "======== Search Stack Dump =======\n" );
                    sdpstStackMgr::dump( aStack );

                    ideLog::log( IDE_SERVER_0, "======== SegCache Dump =======\n" );
                    sdpstCache::dump( sSegCache );

                    IDE_ASSERT( 0 );
                }

                break;
            }
            else
            {
                /* ���� Ȯ���� �����Ѵ�. */
                *aNeedToExtendExt = ID_TRUE;
                break;
            }
        }
        else
        {
            // ���� Root�� �����Ѵ�.
            sdpstStackMgr::pop( aStack );
            // ���� Node�� �����Ѵ�.
            sPosItem = sdpstStackMgr::pop( aStack );

            // ���� ��忡���� rtbmp ������ ������ ������Ų����,
            // ���� Node�� �ٽ� �ִ´�.
            sPosItem.mIndex++;
            sdpstStackMgr::push( aStack, &sPosItem );
            // ���ο� Root bmp �������� Stack�� �ִ´�.
            sdpstStackMgr::push( aStack, sNxtRtBMP, sNewSlotNoInParent );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Internal Bitmap ������������ ������� Ž���� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::searchSpaceInItBMP(
                                 idvSQL          * aStatistics,
                                 sdrMtx          * aMtx,
                                 scSpaceID         aSpaceID,
                                 scPageID          aItBMP,
                                 sdpstSearchType   aSearchType,
                                 sdpstSegCache   * aSegCache,
                                 idBool          * aGoNxtIt,
                                 sdpstPosItem    * aArrLeafBMP,
                                 UInt            * aCandidateCount )
{
    UChar             * sItPagePtr;
    sdrSavePoint        sInitSP;
    sdpstWM             sHWM;
    UInt                sState = 0;

    IDE_DASSERT( aItBMP          != SD_NULL_PID );
    IDE_DASSERT( aMtx            != NULL );
    IDE_DASSERT( aSegCache       != NULL );
    IDE_DASSERT( aGoNxtIt        != NULL );
    IDE_DASSERT( aArrLeafBMP     != NULL );
    IDE_DASSERT( aCandidateCount != NULL );

    sdrMiniTrans::setSavePoint( aMtx, &sInitSP );

    sState = 0;
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aItBMP,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sItPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sState = 1;

    /* BUG-29325 TC/Server/sm4/Project3/PROJ-2037/conc_recv/sgl/upd_sqs.sql ����
     *           FATAL �߻�
     * �ĺ� ���� �������� HWM�� ����� �� �ֱ� ������ �ش� BMP ��������
     * S-Latch�� ȹ���� ���� HWM�� �����;� �Ѵ�. */
    sdpstCache::copyHWM( aStatistics, aSegCache, &sHWM );

    /*
     * internal bitmap ���������� Ž���� leaf bmp ���������� �ĺ� �����
     * �ۼ��ϰ�, Search Type�� ���� Available ���� �ʴٸ� Cache Hint��
     * �����Ѵ�.
     */
    sdpstBMP::makeCandidateChild( aSegCache,
                                  sItPagePtr,
                                  SDPST_ITBMP,
                                  aSearchType,
                                  &sHWM,
                                  aArrLeafBMP,
                                  aCandidateCount );

    sState = 0;
    IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "1.BUG-29325@sdpstFindPage::searchSpaceInItBMP" );

    if ( *aCandidateCount == 0 )
    {
        // itbmp �������� ������ �ĺ� lfbmp ���������� �������� �ʴ´ٸ�
        // ���� rtbmp ���������� ���� itbmp �������� �Ѿ��.
        // stack�� ����Ǿ� �ִ� Root BMP �������� PID�� It SlotNo��
        // ����Ͽ� �����ϰ�, ���� �����ϴ� internal bmp �������� ���� ������
        // stack���� �����Ѵ�.
        *aGoNxtIt = ID_TRUE;
    }
    else
    {
        // ������ �ĺ� leaf bmp ���������� Ž���߱� ������
        // Stack���� internal bmp�� ���� ������ pop���� �ʴ´�.
        *aGoNxtIt = ID_FALSE;
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                    == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Leaf Bitmap ������������ ������� Ž���� �����Ѵ�.
 *
 * �ĺ� Data �������� Unformat �����̸�, ������ �Ҵ��� �����Ѵ�.
 *
 * A. �ĺ� �������� INSERTABLE �������ΰ��
 *
 * leaf bmp �������� S-latch�� ȹ���� ���¿��� �ĺ� Data ������ �����
 * �ۼ��ϰ� �ٷ� �����Ѵ�. �ֳ��ϸ�, �Ҵ����� �ĺ� �������� ���뵵��
 * �����ϱ� ���ؼ� leaf bmp �������� ���ؼ� X-latch�� ȹ���Ϸ��� �ؾ�
 * �ϱ� �����̴�. ���� S-latch�� �������� ������, �ٸ� Ʈ����ǵ鵵
 * ���������� �������� �ʱ� ������ ������ ���뵵 ������ �ϱ� ���ؼ�
 * X-latch�� ȹ������ ���ϴ� �������¿� ���� �� �ִ�.
 *
 * leaf bmp �������� ���� unlatch���Ŀ� �ĺ� Data �������� ���ڱ�
 * format �������� ����� �� �ִ�. �̸� ����ؾ��Ѵ�.
 *
 * B. �ĺ� �������� UNFORMAT �������ΰ��
 * leaf bmp �������� ���� S-latch�� �����Ͽ��� ������, �ĺ� unformat
 * �������� ���� Ʈ����ǰ��� ������ �߻��� �� �ִ�. �׷��Ƿ� �ڽ���
 * �ĺ� �������� unformat ���������� ������ leaf bmp ������
 * �� X-latch�� ȹ������ ��, PBS�� �ٽ��ѹ� Ȯ���غ��� �Ѵ�.
 *
 * ��������� Ž���Ͽ���, MFNL�� �����ؾ��Ѵٸ�, �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::searchSpaceInLfBMP( idvSQL          * aStatistics,
                                          sdrMtx          * aMtx,
                                          scSpaceID         aSpaceID,
                                          sdpSegHandle    * aSegHandle,
                                          scPageID          aLeafBMP,
                                          sdpstStack      * aStack,
                                          sdpPageType       aPageType,
                                          sdpstSearchType   aSearchType,
                                          UInt              aRowSize,
                                          UChar          ** aPagePtr,
                                          UChar           * aCTSlotNo )
{
    sdrSavePoint         sInitSP;
    UChar              * sLfPagePtr;
    UChar              * sPagePtr   = NULL;
    UChar                sCTSlotNo = SDP_CTS_IDX_NULL;
    UInt                 sState = 0;
    sdpstWM              sHWM;
    sdpstCandidatePage   sArrData[SDPST_MAX_CANDIDATE_PAGE_CNT];
    UInt                 sCandidateDataCount = 0;

    IDE_ASSERT( aSegHandle      != NULL );
    IDE_ASSERT( aLeafBMP        != SD_NULL_PID );
    IDE_ASSERT( aMtx            != NULL );
    IDE_ASSERT( aPagePtr        != NULL );

    IDU_FIT_POINT( "1.BUG-29325@sdpstFindPage::searchSpaceInLfBMP" );

    sdrMiniTrans::setSavePoint( aMtx, &sInitSP );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aLeafBMP,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sLfPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    /* BUG-29325 TC/Server/sm4/Project3/PROJ-2037/conc_recv/sgl/upd_sqs.sql ����
     *           FATAL �߻�
     * �ĺ� ���� �������� HWM�� ����� �� �ֱ� ������ �ش� BMP ��������
     * S-Latch�� ȹ���� ���� HWM�� �����;� �Ѵ�. */
    sdpstCache::copyHWM( aStatistics,
                         (sdpstSegCache*)aSegHandle->mCache,
                         &sHWM );

    /* HWM ���� Ž���Ϸ��� ��� */
    if ( sdpstStackMgr::compareStackPos( &sHWM.mStack, aStack ) < 0 )
    {
        ideLog::log( IDE_SERVER_0,
                "HWM PID: %u, "
                "ExtDir PID: %u, "
                "SlotNoInExtDir: %d\n"
                "LfBMP :%u\n",
                sHWM.mWMPID,
                sHWM.mExtDirPID,
                sHWM.mSlotNoInExtDir,
                aLeafBMP );

        sdpstStackMgr::dump( &sHWM.mStack );
        sdpstStackMgr::dump( aStack );
        sdpstCache::dump( (sdpstSegCache*)aSegHandle->mCache );
        sdpstLfBMP::dump( sLfPagePtr );

        IDE_ASSERT( 0 );
    }

    // A. Ž���� Data ������ �ĺ� ����� �ۼ��Ѵ�.
    sdpstBMP::makeCandidateChild( (sdpstSegCache*)aSegHandle->mCache,
                                  sLfPagePtr,
                                  SDPST_LFBMP,
                                  aSearchType,
                                  &sHWM,
                                  (void*)sArrData,
                                  &sCandidateDataCount );

    // �ĺ� Data ������ ����� �ۼ��Ͽ��ٸ�, lfbmp �������� ���� Latch��
    // �����Ѵ���, �ĺ� ��Ͽ� ���ؼ� No-Wait ���� �������Ž���� �����Ѵ�.
    // itbmp �������� ���� latch�� �����߱� ������, ���� �ĺ� ��� �ۼ������
    // ������ ���°� �ƴҼ��� �ִٴ� ���� ����ؾ��Ѵ�.
    sState = 0;
    IDE_TEST( sdrMiniTrans::releaseLatchToSP(aMtx, &sInitSP) != IDE_SUCCESS );

    IDU_FIT_POINT( "1.BUG-33683@sdpstFindPage::searchSpaceInLfBMP" );

    if ( sCandidateDataCount > 0 )
    {
        // �ĺ� Data �������� ���ؼ� No-Wait ���� Ž���� �����Ѵ�.
        IDE_TEST( searchPagesInCandidatePage(
                                           aStatistics,
                                           aMtx,
                                           aSpaceID,
                                           aSegHandle,
                                           aLeafBMP,
                                           sArrData,
                                           aPageType,
                                           sCandidateDataCount,
                                           aSearchType,
                                           aRowSize,
                                           SDB_WAIT_NO,
                                           &sPagePtr,
                                           &sCTSlotNo ) != IDE_SUCCESS );

        /* BUG-33683 - [SM] in sdcRow::processOverflowData, Deadlock can occur
         *
         * sdcRow::processOverflowData���� overflow �����͸� ó���ϱ� ����
         * �������� �Ҵ��ϰ� �ȴ�.
         * �̶�, sdcRow���� ���ÿ� �ϳ��� ���������� latch�� �ɾ�� �Ѵٴ�
         * ������ ������ ������ ����� �߻� ���ɼ��� �ְ� �ȴ�.
         * ���� �̷��� ��츦 �ذ��ϱ� ���� TMS���� No-wait ���� ������ ��������
         * Ȯ���غ� �� wait ���� Ȯ���ϰ� �ִµ�, �̸� no-wait ���θ� Ȯ���ϵ���
         * �����Ѵ�.
         *
         * �̷��� ���������ν� ���׸�Ʈ�� ����Ȯ���� ���� �� ������� �� �ִٴ� ������
         * ������,
         * no-wait ���� getPage�� ������ �������� ȹ������ ���� ����
         * ������ ������ contention�� ���� ����� �� �� �ְ�,
         * �̷� ��� ���׸�Ʈ ������ Ȯ���Ͽ� contention�� ���̴� ���� ���ɿ� ������ �ȴ�.
         * ���� no-wait ���� �����ϴ� ���� �� �ո����̴�. */
        if ( sPagePtr == NULL )
        {
            // �ĺ� Data �������� ���ؼ� Wait ���� Ž���Ѵ�.
            // �������Ҵ� Ž�������� Wait ���� Ž������ �ʴ´�.
            IDE_TEST( searchPagesInCandidatePage(
                                             aStatistics,
                                             aMtx,
                                             aSpaceID,
                                             aSegHandle,
                                             aLeafBMP,
                                             sArrData,
                                             aPageType,
                                             sCandidateDataCount,
                                             aSearchType,
                                             aRowSize,
                                             SDB_WAIT_NO,
                                             &sPagePtr,
                                             &sCTSlotNo )
                    != IDE_SUCCESS );
        }
    }
    else
    {
        // �ĺ������ �ۼ����� ���Ͽ��ٸ�, ���� �ĺ� itbmp �������� ����
        // Ž���������� �Ѿ��.
    }

    *aPagePtr   = sPagePtr;
    *aCTSlotNo = sCTSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 1:
            IDE_ASSERT( sdrMiniTrans::releaseLatchToSP(aMtx, &sInitSP)
                        == IDE_SUCCESS );
        default:
            break;
    }

    *aPagePtr   = NULL;
    *aCTSlotNo = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �ĺ� Data �������� ���ؼ� No-Wait ���� �������
 *               Ž���� �����Ѵ�.
 **********************************************************************/
IDE_RC sdpstFindPage::searchPagesInCandidatePage(
    idvSQL               * aStatistics,
    sdrMtx               * aMtx,
    scSpaceID              aSpaceID,
    sdpSegHandle         * aSegHandle,
    scPageID               aLeafBMP,
    sdpstCandidatePage   * aArrData,
    sdpPageType            aPageType,
    UInt                   aCandidateDataCount,
    sdpstSearchType        aSearchType,
    UInt                   aRowSize,
    sdbWaitMode            aWaitMode,
    UChar               ** aPagePtr,
    UChar                * aCTSlotNo )
{
    sdpPhyPageHdr     * sPhyPageHdr;
    sdpstPBS            sPBS;
    UInt                sLoop;
    sdrSavePoint        sInitSP;
    UChar             * sPagePtr = NULL;
    idBool              sTrySuccess;
    sdpParentInfo       sParentInfo;
    sdpstStack          sRevStack;
    sdrMtxStartInfo     sStartInfo;
    UInt                sState = 0;
    sdrMtx              sMtx;
    idBool              sCanAlloc;
    UChar               sCTSlotNo = SDP_CTS_IDX_NULL;
    sdpPageType         sOldPageType;
    ULong               sNTAData[2];
    smLSN               sNTA;

    IDE_DASSERT( aLeafBMP            != SD_NULL_PID );
    IDE_DASSERT( aArrData            != NULL );
    IDE_DASSERT( aMtx                != NULL );
    IDE_DASSERT( aPagePtr            != NULL );
    IDE_DASSERT( aCandidateDataCount > 0 );

    for ( sLoop = 0; sLoop < aCandidateDataCount; sLoop++ )
    {
        /* Unformat ���´� ����. */
        if ( sdpstLfBMP::isEqFN( aArrData[sLoop].mPBS,
                                 SDPST_BITSET_PAGEFN_UNF ) == ID_TRUE )
        {
            ideLog::log( IDE_SERVER_0,
                         "sLoop: %u\n",
                         sLoop );

            ideLog::logMem(
                IDE_SERVER_0,
                (UChar*)aArrData,
                (ULong)ID_SIZEOF(sdpstCandidatePage) * aCandidateDataCount );

            for ( sLoop = 0; sLoop < aCandidateDataCount; sLoop++ )
            {
                ideLog::log( IDE_SERVER_0,
                             "aArrData[%u]  : %u\n",
                             sLoop, aArrData[sLoop].mPBS );
            }

            IDE_ASSERT( 0 );
        }


        // Unformat �������� ���� Ž�������� ������ ��� Format �̻���
        // ���뵵�� ���� �������� ���ؼ� NoWait ���� �����غ���.
        // SLOT�Ҵ��� ������ PBS�� �˻��Ѵ�.
        sdrMiniTrans::setSavePoint( aMtx, &sInitSP );

        // �ĺ� Data �������� NoWait or Wiat���� X-latch ��û�Ѵ�.
        sState = 0;
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              aArrData[ sLoop ].mPageID,
                                              SDB_X_LATCH,
                                              aWaitMode,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPagePtr,
                                              &sTrySuccess,
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );
        sState = 1;

        if ( sTrySuccess == ID_FALSE )
        {
            sState   = 0;
            sPagePtr = NULL;
            continue;
        }

        /* �������� �̹� �ѹ� format�� �Ǿ��� ������ �ǵ��� ������
         * ���� �� �ִ�. */
        sPBS = (sdpstPBS)sdpPhyPage::getState((sdpPhyPageHdr*)sPagePtr);
        IDE_DASSERT( sdpstLfBMP::isEqFN( sPBS, SDPST_BITSET_PAGEFN_UNF )
                     == ID_FALSE );

        if ( sdpstLfBMP::isEqFN( sPBS, SDPST_BITSET_PAGEFN_FUL ) == ID_TRUE )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                      != IDE_SUCCESS );
            sPagePtr   = NULL;
            continue;
        }

        if ( sdpstLfBMP::isEqFN( sPBS, SDPST_BITSET_PAGEFN_FMT ) == ID_TRUE )
        {
            /* �ѹ� Format�� �Ǿ��� �������� Read�ؼ� �ǵ��ؼ� �о��
             * ������ Format�̸� �ʿ��� ������Ÿ������ �������ϰ� ����Ѵ� */
            sParentInfo = sdpPhyPage::getParentInfo( sPagePtr );

            if ( (sParentInfo.mParentPID != aLeafBMP) ||
                 (sParentInfo.mIdxInParent != aArrData[sLoop].mPBSNo) )
            {
                /* Page Hex Data And Header */
                (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                             sPagePtr,
                                             "Data Page's Parent Info is invalid "
                                             "(Page ID: %"ID_UINT32_FMT", "
                                             "ParentPID: %"ID_UINT32_FMT", "
                                             "IdxInParent: %"ID_UINT32_FMT", "
                                             "LfBMP PID: %"ID_UINT32_FMT", "
                                             "PBSNo In LfBMP: %"ID_UINT32_FMT")\n",
                                             aArrData[sLoop].mPageID,
                                             sParentInfo.mParentPID,
                                             sParentInfo.mIdxInParent,
                                             aLeafBMP,
                                             aArrData[sLoop].mPBSNo );

                IDE_ASSERT( 0 );
            }

            sOldPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );

            if( ( sOldPageType == SDP_PAGE_DATA ) &&
                ( aPageType    == SDP_PAGE_DATA ) )
            {
                /* BUG-32539 [sm-disk-page] The abnormal shutdown during
                 * executing INSERT make a DRDB Page unrecoverable.
                 * Page�� DataPage�� �����ߴµ�, PBS�� Format�� ���� ����ä
                 * ������ ����� �� ����. �� ���� �̹� DataPage�� ����
                 * �Ǿ�����, �� ������ �ʿ� ����. */
            }
            else
            {
                sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

                if( sStartInfo.mTrans != NULL )
                {
                    sNTA = smLayerCallback::getLstUndoNxtLSN( sStartInfo.mTrans );
                }
                   
                IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                               &sMtx,
                                               &sStartInfo,
                                               ID_TRUE,/*Undoable(PROJ-2162)*/
                                               SM_DLOG_ATTR_DEFAULT )
                          != IDE_SUCCESS );
                sState = 2;

                IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, sPagePtr )
                          != IDE_SUCCESS );

                sPhyPageHdr = sdpPhyPage::getHdr( sPagePtr );

                IDE_TEST( sdpPhyPage::setPageType( sPhyPageHdr,
                                                   aPageType,
                                                   &sMtx )
                          != IDE_SUCCESS );

                sdbBufferMgr::setPageTypeToBCB( (UChar*)sPhyPageHdr,
                                                (UInt)aPageType );

                if( aPageType == SDP_PAGE_DATA )
                {
                    IDE_TEST( smLayerCallback::logAndInitCTL( &sMtx,
                                                              sPhyPageHdr,
                                                              aSegHandle->mSegAttr.mInitTrans,
                                                              aSegHandle->mSegAttr.mMaxTrans )
                              != IDE_SUCCESS );

                    IDE_TEST( sdpSlotDirectory::logAndInit(
                                  sPhyPageHdr,
                                  &sMtx) 
                              != IDE_SUCCESS );
                }
                else
                {
                    if( (aPageType == SDP_PAGE_LOB_DATA) ||
                        (aPageType == SDP_PAGE_LOB_INDEX) )
                    {
                        if( sMtx.mTrans != NULL )
                        {
                            IDE_ASSERT( sStartInfo.mTrans != NULL );
                            
                            sNTAData[0] = aSegHandle->mSegPID;
                            sNTAData[1] = sdpPhyPage::getPageIDFromPtr(sPagePtr);

                            (void)sdrMiniTrans::setNTA(
                                &sMtx,
                                aSpaceID,
                                SDR_OP_SDPST_ALLOC_PAGE,
                                &sNTA,
                                sNTAData,
                                2 /* DataCount */ );
                        }
                    }
                    
                    sPBS = (sdpstPBS)
                        (SDPST_BITSET_PAGETP_DATA|SDPST_BITSET_PAGEFN_FUL);
                    IDU_FIT_POINT("2.BUG-42505@sdpstFindPage::tryToAllocExtsAndPage::setState");
                    IDE_TEST( sdpPhyPage::setState( sPhyPageHdr, sPBS, &sMtx )
                              != IDE_SUCCESS );

                    // Table Page�� �ƴѰ�쿡�� �̸� bitmap�� �����Ѵ�.
                    sdpstStackMgr::initialize(&sRevStack);

                    // leaf bmp ���������� MFNL�� ����õ��Ͽ�
                    // �������� �����Ѵ�.
                    IDE_TEST( sdpstAllocPage::tryToChangeMFNLAndItHint(
                                  aStatistics,
                                  &sMtx,
                                  aSpaceID,
                                  aSegHandle,
                                  SDPST_CHANGEMFNL_LFBMP_PHASE,
                                  sParentInfo.mParentPID,  // leaf pid
                                  sParentInfo.mIdxInParent,
                                  (void*)&sPBS,
                                  SD_NULL_PID,            // unuse
                                  SDPST_INVALID_SLOTNO,  // unuse
                                  &sRevStack ) != IDE_SUCCESS );
                }
                
                sState = 1;
                IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

                if ( sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr )
                     != aPageType )
                {
                    /* Page Hex Data And Header */
                    (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                                 sPagePtr,
                                                 "Data Page has invalid Page Type "
                                                 "(Page ID: %"ID_UINT32_FMT", "
                                                 "PageType: %"ID_UINT32_FMT", "
                                                 "Requested PageType: %"ID_UINT32_FMT")\n",
                                                 aArrData[sLoop].mPageID,
                                                 sdpPhyPage::getPageType((sdpPhyPageHdr*)sPagePtr),
                                                 aPageType );

                    IDE_ASSERT( 0 );
                }
            }

            break;
        }
        else
        {
            if ( aSearchType == SDPST_SEARCH_NEWPAGE )
            {
                // ������ ���������� �ƴϿ��� ������ �Ҵ翡 �������Ͽ�
                // ���� �ĺ� ����Ÿ�������� �Ѿ��.
                sState = 0;
                IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                          != IDE_SUCCESS );
                sPagePtr   = NULL;
                continue;
            }

            // ������ X-Latch�� ȹ��Ǹ�, Insert High Limit�� �����
            // �������ũ��� Row ũ�⸦ ������ �� ������ �Ǵ��Ѵ�.
            // ���� ������ �� ���ٸ�, Data ������ ���¸� Full�� �����Ѵ�.
            // �ٸ� Rowũ�⸦ �����Ҽ� �������� ������, ����Ž��������
            // ���� ������ �� ���� ���� ������ ���¸� Full�� �����Ͽ�
            // Ž������� ���ϼ� �ְ� �Ѵ�.
            // ( ���� ���׸�Ʈ�� ������ ��å�̴�)
            IDE_TEST(  checkSizeAndAllocCTS( aStatistics,
                                             aMtx,
                                             aSpaceID,
                                             aSegHandle,
                                             sPagePtr,
                                             aRowSize,
                                             &sCanAlloc,
                                             &sCTSlotNo ) != IDE_SUCCESS );

            if ( sCanAlloc == ID_TRUE )
            {
                break;
            }
            else
            {
                sState = 0;
                IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                          != IDE_SUCCESS );
            }
        }

        sPagePtr   = NULL;
        sCTSlotNo = SDP_CTS_IDX_NULL;
    }

    *aPagePtr  = sPagePtr;
    *aCTSlotNo = sCTSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                        == IDE_SUCCESS );
        default:
            break;
    }

    *aPagePtr   = NULL;
    *aCTSlotNo = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table Data �������� Freeness ������ �����ؾ��ϴ���
 *               Ȯ���Ѵ�.
 *
 *  aPageHdr       - [IN]  physical page header
 *  aSegHandle     - [IN]  Segment Handle
 *  aNewPBS        - [OUT] page bitset
 ***********************************************************************/
idBool sdpstFindPage::needToChangePageFN( sdpPhyPageHdr    * aPageHdr,
                                          sdpSegHandle     * aSegHandle,
                                          sdpstPBS         * aNewPBS )
{
    idBool          sIsInsertable = ID_FALSE;
    idBool          sIsChangePBS  = ID_FALSE;
    sdpstPBS        sPBS;
    UChar         * sPagePtr;

    IDE_DASSERT( aPageHdr  != NULL );
    IDE_DASSERT( aNewPBS != NULL );

    sPBS     = (sdpstPBS)sdpPhyPage::getState( aPageHdr );
    *aNewPBS = sPBS;

    switch ( sPBS )
    {
        case SDPST_BITSET_PAGEFN_FMT:
            sIsInsertable = ID_TRUE;

        case SDPST_BITSET_PAGEFN_INS:

            if( isPageUpdateOnly( 
                      aPageHdr, aSegHandle->mSegAttr.mPctFree ) == ID_TRUE )
            {
                *aNewPBS = SDPST_BITSET_PAGEFN_FUL;
                sIsChangePBS = ID_TRUE;
            }
            else
            {
                if ( sIsInsertable == ID_TRUE )
                {
                    *aNewPBS = SDPST_BITSET_PAGEFN_INS;
                    sIsChangePBS = ID_TRUE;
                }
            }
            break;

        case SDPST_BITSET_PAGEFN_FUL:

            if( isPageInsertable( 
                      aPageHdr, aSegHandle->mSegAttr.mPctUsed ) == ID_TRUE )
            {
                *aNewPBS = SDPST_BITSET_PAGEFN_INS;
                sIsChangePBS = ID_TRUE;
            }
            break;

        case SDPST_BITSET_PAGEFN_UNF:
        default:
            sPagePtr = sdpPhyPage::getPageStartPtr( aPageHdr );

            /* Page Hex Data And Header */
            (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                         sPagePtr,
                                         "Invalid PageBitSet: %"ID_UINT32_FMT"\n",
                                         sPBS );
            IDE_ASSERT( 0 );
            break;
    }

    return sIsChangePBS;
}

/***********************************************************************
 * Description : Ʈ����� TableInfo�� ������ ������� �Ҵ��ߴ� Data
 *               ���������� ������� Ž�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::checkAndSearchHintDataPID(
                           idvSQL             * aStatistics,
                           sdrMtx             * aMtx,
                           scSpaceID            aSpaceID,
                           sdpSegHandle       * aSegHandle,
                           void               * aTableInfo,
                           scPageID             aHintDataPID,
                           UInt                 aRowSize,
                           UChar             ** aPagePtr,
                           UChar              * aCTSlotNo,
                           scPageID           * aNewHintDataPID )
{
    idBool          sCanAlloc;
    idBool          sTrySuccess;
    sdrSavePoint    sInitSP;
    UChar         * sPagePtr   = NULL;
    UChar           sCTSlotNo = SDP_CTS_IDX_NULL;
    UInt            sState = 0;

    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aTableInfo != NULL );
    IDE_ASSERT( aPagePtr   != NULL );
    IDE_ASSERT( aCTSlotNo  != NULL );

    *aNewHintDataPID = aHintDataPID;

    if ( aHintDataPID != SD_NULL_PID )
    {
        sdrMiniTrans::setSavePoint( aMtx, &sInitSP );

        // No-Wait ���� Data �������� X-latch�� ȹ���Ѵ�.
        sState = 0;
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              aHintDataPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NO,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPagePtr,
                                              &sTrySuccess,
                                              NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
        sState = 1;

        if ( sTrySuccess == ID_FALSE )
        {
            sState = 0;
            sPagePtr = NULL;
            sCTSlotNo = SDP_CTS_IDX_NULL;
            IDE_CONT( cont_skip_datapage );
        }


        if ( ((sdpstPBS)
               sdpPhyPage::getState( (sdpPhyPageHdr*)sPagePtr )) !=
               ((sdpstPBS)
               (SDPST_BITSET_PAGETP_DATA|SDPST_BITSET_PAGEFN_FUL)) )
        {
            // ������ insert�� Data ���������� ��������� Ž���Ѵ�.
            // ����, Data �������� ��������� �������� �ʴ� ��쿡��
            // Hint Leaf BMP���� Ž���� �õ��Ѵ�. Leaf BMP�� Insert
            // Data ���������� ����.
            IDE_TEST( checkSizeAndAllocCTS( aStatistics,
                        aMtx,
                        aSpaceID,
                        aSegHandle,
                        sPagePtr,
                        aRowSize,
                        &sCanAlloc,
                        &sCTSlotNo ) != IDE_SUCCESS );

            if ( sCanAlloc == ID_FALSE )
            {
                sState = 0;
                IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                        != IDE_SUCCESS );

                sPagePtr   = NULL;
                sCTSlotNo = SDP_CTS_IDX_NULL;

                smLayerCallback::setHintDataPIDofTableInfo( aTableInfo, SD_NULL_PID );

                *aNewHintDataPID = SD_NULL_PID;
            }
            else
            {
                // �������� ��������Ҵ��� �����ϸ� Ž�������� �Ϸ��Ѵ�.
                // ������ ���� ������ �Ҵ����Ŀ� ó���Ѵ�.
            }
        }
        else
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )
                      != IDE_SUCCESS );
            // Hint Data �������� ���ؼ� ��������Ҵ��� �Ұ����Ͽ�
            // Hint Data �������� �ʱ�ȭ�Ѵ�.
            smLayerCallback::setHintDataPIDofTableInfo( aTableInfo, SD_NULL_PID );

            sPagePtr = NULL;
            sCTSlotNo = SDP_CTS_IDX_NULL;
            *aNewHintDataPID = SD_NULL_PID;
        }
    }

    IDE_EXCEPTION_CONT( cont_skip_datapage );

    *aPagePtr   = sPagePtr;
    *aCTSlotNo = sCTSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::releaseLatchToSP( aMtx, &sInitSP )      
                    == IDE_SUCCESS );
    }

    *aPagePtr   = NULL;
    *aCTSlotNo = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �������� ��������� Ȯ���ϰ�, TTS�� Bind�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFindPage::checkSizeAndAllocCTS( idvSQL          * aStatistics,
                                            sdrMtx          * aMtx,
                                            scSpaceID         aSpaceID,
                                            sdpSegHandle    * aSegHandle,
                                            UChar           * aPagePtr,
                                            UInt              aRowSize,
                                            idBool          * aCanAlloc,
                                            UChar           * aCTSlotNo )
{
    idBool            sUpdatePageStateToFull;
    idBool            sCanAllocSlot;
    idBool            sAfterSelfAging;
    sdrMtxStartInfo   sStartInfo;
    UChar             sCTSlotNo;
    sdpPhyPageHdr   * sPageHdr;
    sdpSelfAgingFlag  sCheckFlag = SDP_SA_FLAG_NOTHING_TO_AGING;

    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aPagePtr   != NULL );
    IDE_ASSERT( aCTSlotNo != NULL );

    sCanAllocSlot          = ID_FALSE;
    sUpdatePageStateToFull = ID_TRUE;
    sCTSlotNo             = SDP_CTS_IDX_NULL;
    sAfterSelfAging        = ID_FALSE;

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    while( 1 )
    {
        /*
         * PRJ-1704 Disk MVCC Renewal
         *
         * Insert ���� ������ �������� ���ؼ� TTS �Ҵ��������
         * Soft Row TimeStamping�� �Ҽ� �ִ� TTS�鿡 ���� �����Ͽ�
         * Ȯ���� �� �ִ� ��������� Ȯ���ϱ⵵ �Ѵ�.
         *
         * �������� �� TTS�� �Ҵ��ϴ� ���� �� �߿��ϱ� ������ Insert��
         * ��� Ȯ���� ������ ���ϰ� Free�� TTS�� �������� �ʾƼ�
         * �Ҵ����� ���ϸ�, �ٸ� Page�� �˻��ϵ��� �ش� Page�� ����
         * Ȯ�ΰ����� �Ϸ��Ѵ�.
         */
        sPageHdr = (sdpPhyPageHdr*)aPagePtr;

        IDE_TEST( smLayerCallback::allocCTSAndSetDirty( aStatistics,
                                                        NULL,         /* aFixMtx */
                                                        &sStartInfo,  /* for Logging */
                                                        sPageHdr,
                                                        &sCTSlotNo ) != IDE_SUCCESS );

        if ( ( smLayerCallback::getCountOfCTS( sPageHdr ) > 0 ) &&
             ( sCTSlotNo == SDP_CTS_IDX_NULL ) )
        {
            sCanAllocSlot = ID_FALSE;
        }
        else
        {
            sCanAllocSlot = sdpPhyPage::canAllocSlot( (sdpPhyPageHdr*)aPagePtr,
                                                      aRowSize,
                                                      ID_TRUE /* create slotentry */,
                                                      SDP_1BYTE_ALIGN_SIZE );
        }

        if ( sCanAllocSlot == ID_TRUE )
        {
            sUpdatePageStateToFull = ID_FALSE;
            break;
        }
        else
        {
            // Available Free Size�� ����������,
            // Self-Aging �ϸ� ������ ���� �ִ�.

            // Page�� Available FreeSize�� ������� �ʾƵ� ���Ŀ�
            // Total Free Size�� ����ϱ� ������ �ٽ� ������ �� �ֵ���
            // ������ �����ξ�� �Ѵ�. �׷��Ƿ� ������ ���¸�
            // FULL ���·� �������� �ʴ´�.
            sCTSlotNo = SDP_CTS_IDX_NULL;
        }

        if ( sAfterSelfAging == ID_TRUE )
        {
            if ( sCheckFlag != SDP_SA_FLAG_NOTHING_TO_AGING )
            {
                // ���� Long-term Ʈ������� �����ؼ�
                // SelfAging�� �Ҽ� ���� ��쿡�� Insertable Page��
                // �� �������� Insert ���������� ������ �ɼ��ִ�.
                sUpdatePageStateToFull = ID_FALSE;
            }
            break;
        }

        IDE_TEST( smLayerCallback::checkAndRunSelfAging( aStatistics,
                                                         &sStartInfo,
                                                         (sdpPhyPageHdr*)aPagePtr,
                                                         &sCheckFlag )
                  != IDE_SUCCESS );

        sAfterSelfAging = ID_TRUE;
    }

    if ( sUpdatePageStateToFull == ID_TRUE )
    {
        IDE_TEST( sdpstAllocPage::updatePageFNtoFull( aStatistics,
                                                      aMtx,
                                                      aSpaceID,
                                                      aSegHandle,
                                                      aPagePtr ) 
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    *aCanAlloc  = sCanAllocSlot;
    *aCTSlotNo = sCTSlotNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
