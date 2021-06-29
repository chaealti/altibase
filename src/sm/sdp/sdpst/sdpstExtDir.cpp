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
 * $Id: sdpstExtDir.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� ExtDir �������� ����  STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdptbDef.h>
# include <sdptbExtent.h>

# include <sdpstSH.h>
# include <sdpstExtDir.h>

/***********************************************************************
 * Description : ExtDesc �ʱ�ȭ
 *
 * aExtDesc     - [IN] �ʱ�ȭ�� ExtDesc
 ***********************************************************************/
void sdpstExtDir::initExtDesc( sdpstExtDesc *aExtDesc )
{
    idlOS::memset( aExtDesc, 0x00, ID_SIZEOF( sdpstExtDesc ) );

    aExtDesc->mExtFstPID     = SD_NULL_PID;
    aExtDesc->mLength        = 0;
    aExtDesc->mExtMgmtLfBMP  = SD_NULL_PID;
    aExtDesc->mExtFstDataPID = SD_NULL_PID;
}

/***********************************************************************
 * Description : ExtDir Header �ʱ�ȭ
 *
 * aExtDirHdr   - [IN] ExtDir Header
 * aMaxExtCnt   - [IN] ExtDir���� ���밡���� �ִ� ExtDesc ����
 * aBodyOffset  - [IN] Body Offset
 ***********************************************************************/
void  sdpstExtDir::initExtDirHdr( sdpstExtDirHdr     * aExtDirHdr,
                                  UShort               aMaxExtCnt,
                                  scOffset             aBodyOffset )
{
    IDE_ASSERT( aExtDirHdr != NULL );

    aExtDirHdr->mExtCnt     = 0;           /* ���������� Extent ���� */
    aExtDirHdr->mMaxExtCnt  = aMaxExtCnt;
    aExtDirHdr->mBodyOffset  = aBodyOffset;

    /* Extent Slot�� �ٸ� Bitmap �������ʹ� �޸� ExtDir ��������
     * �ʱ�ȭ�� ���Ŀ� ������ �߰��Ѵ�. */

    return;
}

/***********************************************************************
 * Description : ExtDir Page ���� �� �ʱ�ȭ
 *
 * aStatistics      - [IN] �������
 * aMtx             - [IN] Mini Transaction Pointer
 * aSpaceID         - [IN] Tablespace ID
 * aExtDesc         - [IN] ExtDesc
 * aNewPageID       - [IN] ExtDir PID
 * aMaxExtCnt       - [IN] ExtDir���� ���밡���� �ִ� ExtDesc ����
 * aPagePtr         - [OUT] ExtDir Page Pointer
 ***********************************************************************/
IDE_RC sdpstExtDir::createAndInitPage( idvSQL               * aStatistics,
                                       sdrMtx               * aMtx,
                                       scSpaceID              aSpaceID,
                                       sdpstExtDesc         * aExtDesc,
                                       sdpstBfrAllocExtInfo * aBfrInfo,
                                       sdpstAftAllocExtInfo * aAftInfo,
                                       UChar               ** aPagePtr )
{
    UChar     * sPagePtr;
    sdpstPBS    sPBS;
    ULong       sSeqNo;
    scPageID    sNewPageID;
    UShort      sMaxExtCnt;

    IDE_ASSERT( aExtDesc != NULL );
    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aBfrInfo != NULL );
    IDE_ASSERT( aAftInfo != NULL );

    /* Extent���� ExtDir �������� �Ҵ��Ѵ�. */
    sPBS = (sdpstPBS)( SDPST_BITSET_PAGETP_META | SDPST_BITSET_PAGEFN_FUL );

    sNewPageID = aAftInfo->mLstPID[SDPST_EXTDIR];
    sMaxExtCnt = aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR];

    /* SeqNo�� ����Ѵ�. ExtDir�� Ext�� �� ó�� �����Ǳ� ������ 
     * ���� ������ SeqNo + 1 ������ �����Ѵ�. */
    sSeqNo = aBfrInfo->mNxtSeqNo;

    // logical header�� �Ʒ����� ������ �ʱ�ȭ�Ѵ�.
    IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                          aMtx,
                                          aSpaceID,
                                          NULL, /* aSegHandle4DataPage */
                                          sNewPageID,
                                          sSeqNo,
                                          SDP_PAGE_TMS_EXTDIR,
                                          aExtDesc->mExtFstPID,
                                          SDPST_INVALID_PBSNO,
                                          sPBS,
                                          &sPagePtr ) != IDE_SUCCESS );

    IDE_TEST( logAndInitExtDirHdr( aMtx, getHdrPtr( sPagePtr ), sMaxExtCnt )
              != IDE_SUCCESS );

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir Page Control Header �ʱ�ȭ �� write logging
 *
 * aMtx             - [IN] Mini Transaction Pointer
 * aExtDirHdr       - [IN] ExtDir Header
 * aMaxExtCnt       - [IN] ExtDir���� ���밡���� �ִ� ExtDesc ����
 ***********************************************************************/
IDE_RC sdpstExtDir::logAndInitExtDirHdr( sdrMtx           * aMtx,
                                         sdpstExtDirHdr   * aExtDirHdr,
                                         UShort             aMaxExtCnt )
{
    scOffset            sBodyOffset;
    sdpstRedoInitExtDir sLogData;

    IDE_ASSERT( aExtDirHdr != NULL );
    IDE_ASSERT( aMtx     != NULL );

    // Segment Header�� ExtDir Offset�� ExtDir ������������
    // Map Offset�� �ٸ���.
    sBodyOffset = sdpPhyPage::getDataStartOffset( ID_SIZEOF( sdpstExtDirHdr ) );

    // page range slot�ʱ�ȭ�� ���ش�.
    initExtDirHdr( aExtDirHdr, aMaxExtCnt, sBodyOffset );

    // INIT_EXTENT_MAP �� Map Offset logging
    sLogData.mMaxExtCnt = aMaxExtCnt;
    sLogData.mBodyOffset = sBodyOffset;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aExtDirHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_INIT_EXTDIR )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir�� Extent Slot�� ���
 *
 * aMapPtr      - [IN] ExtDir�� Map ���� ��ġ
 * aLstSlotNo   - [IN] ExtDesc�� �߰��� SlotNo
 * aExtDesc     - [IN] �߰��� ExtDesc
 ***********************************************************************/
void sdpstExtDir::addExtDescToMap( sdpstExtDesc * aMapPtr,
                                   SShort         aLstSlotNo,
                                   sdpstExtDesc * aExtDesc )
{
    IDE_DASSERT( aMapPtr     != NULL );
    IDE_DASSERT( aExtDesc    != NULL );
    IDE_DASSERT( aLstSlotNo  != SDPST_INVALID_SLOTNO );
    IDE_DASSERT( aExtDesc->mExtFstPID     != SD_NULL_PID );
    IDE_DASSERT( aExtDesc->mLength        >= SDP_MIN_EXTENT_PAGE_CNT );
    IDE_DASSERT( aExtDesc->mExtMgmtLfBMP  != SD_NULL_PID );
    IDE_DASSERT( aExtDesc->mExtFstDataPID != SD_NULL_PID );

    aMapPtr[aLstSlotNo] = *aExtDesc;
}

/***********************************************************************
 * Description : ExtDir�� extslot�� ����Ѵ�.
 *
 * aExtDirHdr   - [IN] ExtDir Header
 * aLstSlotNo   - [IN] ExtDesc�� �߰��� SlotNo
 * aExtDesc     - [IN] �߰��� ExtDesc
 ***********************************************************************/
void sdpstExtDir::addExtDesc( sdpstExtDirHdr * aExtDirHdr,
                              SShort           aLstSlotNo,
                              sdpstExtDesc   * aExtDesc )
{
    IDE_DASSERT( aExtDirHdr != NULL );
    IDE_DASSERT( aExtDesc != NULL );
    IDE_DASSERT( aLstSlotNo != SDPST_INVALID_SLOTNO );

    addExtDescToMap( getMapPtr(aExtDirHdr), aLstSlotNo, aExtDesc );
    aExtDirHdr->mExtCnt++;

    return;
}

/***********************************************************************
 * Descripton : Segment Header�κ��� ������ ExtDir�� ���
 *
 * aMtx         - [IN] Mini Transaction Pointer
 * aExtDirHdr   - [IN] ExtDir Header
 * aExtDesc     - [IN] �߰��� ExtDesc
 * aAllocExtRID - [OUT] ExtRID
 ***********************************************************************/
IDE_RC sdpstExtDir::logAndAddExtDesc( sdrMtx             * aMtx,
                                      sdpstExtDirHdr     * aExtDirHdr,
                                      sdpstExtDesc       * aExtDesc,
                                      sdRID              * aAllocExtRID )
{
    sdpstExtDesc        * sMapPtr;
    sdpstRedoAddExtDesc   sLogData;

    IDE_DASSERT( aMtx               != NULL );
    IDE_DASSERT( aExtDirHdr         != NULL );
    IDE_DASSERT( aExtDesc           != NULL );
    IDE_DASSERT( aAllocExtRID       != NULL );
    IDE_DASSERT( aExtDesc->mExtFstPID     != SD_NULL_PID );
    IDE_DASSERT( aExtDesc->mLength         > 0 );
    IDE_DASSERT( aExtDesc->mExtMgmtLfBMP  != SD_NULL_PID );
    IDE_DASSERT( aExtDesc->mExtFstDataPID != SD_NULL_PID );

    addExtDesc( aExtDirHdr, aExtDirHdr->mExtCnt, aExtDesc );

    // ADD_EXTSLOT logging
    sLogData.mExtFstPID     = aExtDesc->mExtFstPID;
    sLogData.mLength        = aExtDesc->mLength;
    sLogData.mExtMgmtLfBMP  = aExtDesc->mExtMgmtLfBMP;
    sLogData.mExtFstDataPID = aExtDesc->mExtFstDataPID;
    sLogData.mLstSlotNo     = aExtDirHdr->mExtCnt - 1;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aExtDirHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_ADD_EXTDESC )
              != IDE_SUCCESS );

    if ( aExtDirHdr->mExtCnt <= 0 )
    {
        ideLog::log( IDE_SERVER_0,
                     "mExtCnt: %u\n",
                     aExtDirHdr->mExtCnt );
        (void)dump( sdpPhyPage::getPageStartPtr( aExtDirHdr ) );
        IDE_ASSERT( 0 );
    }

    sMapPtr = getMapPtr( aExtDirHdr );
    *aAllocExtRID = sdpPhyPage::getRIDFromPtr(
                                (UChar*)&(sMapPtr[aExtDirHdr->mExtCnt - 1]) );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtDirCntlHdr�� ����Ű�� �������� ������ ExtDesc�� ����
 *               �Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header
 * aExtDirHdr     - [IN] Extent Direct Control Header
 **********************************************************************/
IDE_RC sdpstExtDir::freeLstExt( idvSQL             * aStatistics,
                                sdrMtx             * aMtx,
                                scSpaceID            aSpaceID,
                                sdpstSegHdr        * aSegHdr,
                                sdpstExtDirHdr     * aExtDirHdr )
{
    sdpstExtDesc     * sExtDesc;
    UInt               sFreeExtCnt;

    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aSpaceID   != 0 );
    IDE_ASSERT( aSegHdr    != NULL );
    IDE_ASSERT( aExtDirHdr != NULL );

    sExtDesc = getLstExtDesc( aExtDirHdr );

    if ( sExtDesc->mExtFstPID == SD_NULL_PID )
    {
        (void)dump( sdpPhyPage::getPageStartPtr( aExtDirHdr ) );
        IDE_ASSERT( 0 );
    }


    IDE_TEST( sdptbExtent::freeExt( aStatistics,
                                    aMtx,
                                    aSpaceID,
                                    sExtDesc->mExtFstPID,
                                    &sFreeExtCnt )
              != IDE_SUCCESS );

    IDE_TEST( setExtDescCnt( aMtx,
                             aExtDirHdr,
                             aExtDirHdr->mExtCnt - 1 )
              != IDE_SUCCESS );

    IDE_TEST( sdpstSH::setTotExtCnt( aMtx,
                                     aSegHdr,
                                     aSegHdr->mTotExtCnt - 1 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtDirCntlHdr�� ����Ű�� ���������� ù��° Extent
 *               �����ϰ� ��� Extent�� free�Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aStartInfo     - [IN] Mini Transaction Start Info
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header Pointer
 * aExtDirHdr     - [IN] ExtDir Header
 **********************************************************************/
IDE_RC sdpstExtDir::freeAllExtExceptFst( idvSQL             * aStatistics,
                                         sdrMtxStartInfo    * aStartInfo,
                                         scSpaceID            aSpaceID,
                                         sdpstSegHdr        * aSegHdr,
                                         sdpstExtDirHdr     * aExtDirHdr )
{
    UShort             sExtDescCnt;
    sdrMtx             sFreeMtx;
    SInt               sState = 0;
    UChar            * sExtDirPagePtr;
    UChar            * sSegHdrPagePtr;

    IDE_ASSERT( aStartInfo     != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirHdr != NULL );

    sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aExtDirHdr );
    sSegHdrPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    /* ù��° ExtDesc�� ExtDirPage�� �ִ� Extent�̱⶧���� ���� ���߿� Free
     * ���� Extent Dir Page�� Extent�� �������� Free�Ѵ�. */
    sExtDescCnt    = aExtDirHdr->mExtCnt;

    while( sExtDescCnt > 1 )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       aStartInfo,
                                       ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        sExtDescCnt--;

        IDE_TEST( freeLstExt( aStatistics,
                              &sFreeMtx,
                              aSpaceID,
                              aSegHdr,
                              aExtDirHdr ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sExtDirPagePtr )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sSegHdrPagePtr )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] Squential Scan�� Extent �� ù���� Data
 *               �������� ��ȯ�Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aSpaceID       - [IN] TableSpace ID
 * aExtRID        - [IN] ExtRID
 * aExtInfo       - [OUT] ExtInfo
 ***********************************************************************/
IDE_RC sdpstExtDir::getExtInfo( idvSQL       *aStatistics,
                                scSpaceID     aSpaceID,
                                sdRID         aExtRID,
                                sdpExtInfo   *aExtInfo )
{
    UInt             sState = 0;
    sdpstExtDesc   * sExtDesc;

    IDE_ASSERT( aExtInfo    != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByRID( aStatistics, /* idvSQL */
                                          aSpaceID,
                                          aExtRID,
                                          (UChar **)&sExtDesc )
              != IDE_SUCCESS );
    sState = 1;

    aExtInfo->mFstPID       = sExtDesc->mExtFstPID;
    aExtInfo->mFstDataPID   = sExtDesc->mExtFstDataPID;
    aExtInfo->mExtMgmtLfBMP = sExtDesc->mExtMgmtLfBMP;
    aExtInfo->mLstPID       = sExtDesc->mExtFstPID + sExtDesc->mLength - 1;

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar*)sExtDesc )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    if (sState != 0)
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, (UChar*)sExtDesc )
                    == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] Extent���� ���� Data �������� ��ȯ�Ѵ�.
 *
 * aStatistics      - [IN] �������
 * aSpaceID         - [IN] Tablespace ID
 * aSegInfo         - [IN] Segment Info
 * aSegCacheInfo    - [IN] Segment Cache Info
 * aExtRID          - [IN] �� ExtRID
 * aExtInfo         - [IN] �� Extent Info
 * aPageID          - [OUT] �� Extent ���� ���� PID
 ***********************************************************************/
IDE_RC sdpstExtDir::getNxtAllocPage( idvSQL             * aStatistics,
                                     scSpaceID            aSpaceID,
                                     sdpSegInfo         * aSegInfo,
                                     sdpSegCacheInfo    * aSegCacheInfo,
                                     sdRID              * aExtRID,
                                     sdpExtInfo         * aExtInfo,
                                     scPageID           * aPageID )
{
    IDE_TEST( getNxtPage( aStatistics,
                          aSpaceID,
                          aSegInfo,
                          aSegCacheInfo,
                          aExtRID,
                          aExtInfo,
                          aPageID ) != IDE_SUCCESS );

    /* Meta Page�� skip�Ѵ�. */
    if ( *aPageID == aExtInfo->mFstPID )
    {
        *aPageID = aExtInfo->mFstDataPID;
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Extent���� ���� ������(Meta Page ����)�� ��ȯ�Ѵ�.
 *
 * aStatistics      - [IN] �������
 * aSpaceID         - [IN] Tablespace ID
 * aSegInfo         - [IN] Segment Info
 * aSegCacheInfo    - [IN] Segment Cache Info
 * aExtRID          - [IN] �� ExtRID
 * aExtInfo         - [IN] �� Extent Info
 * aPageID          - [OUT] �� Extent ���� ���� PID
 ***********************************************************************/
IDE_RC sdpstExtDir::getNxtPage( idvSQL             * aStatistics,
                                scSpaceID            aSpaceID,
                                sdpSegInfo         * aSegInfo,
                                sdpSegCacheInfo    * aSegCacheInfo,
                                sdRID              * aExtRID,
                                sdpExtInfo         * aExtInfo,
                                scPageID           * aPageID )
{
    sdRID               sNxtExtRID;

    IDE_ASSERT( aExtInfo    != NULL );

    if ( *aPageID == SD_NULL_PID )
    {
        *aPageID = aExtInfo->mFstPID;
    }
    else
    {
        /* BUG-22474     [valgrind]sdbMPRMgr::getMPRCnt�� UMR�ֽ��ϴ�. */
        IDE_TEST_CONT( *aPageID == aSegInfo->mHWMPID,
                        cont_no_more_page );

        /* BUG-29005 Fullscan ���� ����
         * SegCache Info�� �־����� Fullscan hint ������ �ŷ��� �� �ִٸ� */
        if ( (aSegCacheInfo != NULL) &&
             (aSegCacheInfo->mUseLstAllocPageHint == ID_TRUE) )
        {
            IDE_TEST_CONT( *aPageID == aSegCacheInfo->mLstAllocPID,
                            cont_no_more_page );
        }

        // ���� Extent ������ �Ѿ�� �������̸�,
        // ���� extent ������ ���� �Ѵ�.
        if( *aPageID == aExtInfo->mLstPID )
        {
            IDE_TEST( getNxtExtRID( aStatistics,
                                    aSpaceID,
                                    aSegInfo->mSegHdrPID,
                                    *aExtRID,
                                    &sNxtExtRID ) != IDE_SUCCESS );

            IDE_TEST_CONT( sNxtExtRID == SD_NULL_RID,
                            cont_no_more_page );

            IDE_TEST( getExtInfo( aStatistics,
                                  aSpaceID,
                                  sNxtExtRID,
                                  aExtInfo )
                      != IDE_SUCCESS );

            *aExtRID = sNxtExtRID;
            *aPageID = aExtInfo->mFstPID;
        }
        else
        {
            *aPageID += 1;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( cont_no_more_page );

    *aPageID = SD_NULL_PID;
    *aExtRID = SD_NULL_RID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ExtDesc�� RID�� ��ȯ�Ѵ�.
 *
 * aStatistics      - [IN] �������
 * aSpaceID         - [IN] Tablespace ID
 * aSegHdrPID       - [IN] Segment Header PID
 * aCurExtRID       - [IN] �� ExtRID
 * aNxtExtRID       - [OUT] ���� ExtRID
 ***********************************************************************/
IDE_RC sdpstExtDir::getNxtExtRID( idvSQL       *aStatistics,
                                  scSpaceID     aSpaceID,
                                  scPageID      aSegHdrPID,
                                  sdRID         aCurrExtRID,
                                  sdRID        *aNxtExtRID )
{
    UInt                  sState = 0 ;
    scPageID              sNxtExtDir;
    sdpstExtDirHdr      * sExtDirHdr;
    UChar               * sPagePtr;
    SShort                sExtDescSlotNo;

    IDE_ASSERT( aNxtExtRID != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          SD_MAKE_PID( aCurrExtRID ),
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sExtDirHdr = getHdrPtr( sPagePtr );

    sExtDescSlotNo = calcOffset2SlotNo( sExtDirHdr,
                                        SD_MAKE_OFFSET( aCurrExtRID ) );

    if ( sExtDirHdr->mExtCnt <= sExtDescSlotNo )
    {
        ideLog::log( IDE_SERVER_0,
                     "mExtCnt: %u, "
                     "ExtRID:  %llu, "
                     "SlotNo:  %d\n",
                     sExtDirHdr->mExtCnt,
                     aCurrExtRID,
                     sExtDescSlotNo );
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }



    if ( sExtDirHdr->mExtCnt > (sExtDescSlotNo + 1) )
    {
        // ExtDir ���������� ������ ExtDesc�� �ƴѰ��
        *aNxtExtRID = SD_MAKE_RID(
                      SD_MAKE_PID( aCurrExtRID ),
                      calcSlotNo2Offset( sExtDirHdr, (sExtDescSlotNo + 1) ) );
    }
    else
    {
        sNxtExtDir = getNxtExtDir( sExtDirHdr, aSegHdrPID );

        // ExtDir ���������� ������ ExtDesc�� ���
        if ( sNxtExtDir == SD_NULL_PID )
        {
            *aNxtExtRID = SD_NULL_RID;
        }
        else
        {
            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sExtDirHdr )
                      != IDE_SUCCESS );
            
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  aSpaceID,
                                                  sNxtExtDir,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  NULL, /* sdrMtx */
                                                  &sPagePtr,
                                                  NULL, /* aTrySuccess */
                                                  NULL  /* aIsCorruptPage */ )
                      != IDE_SUCCESS );
            sState = 1;

            sExtDirHdr = sdpstExtDir::getHdrPtr( sPagePtr );

            // ������� �����Ѵ�.
            *aNxtExtRID = SD_MAKE_RID( sNxtExtDir,
                                       calcSlotNo2Offset( sExtDirHdr, 0 ));
        }
    }

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         (UChar*)sExtDirHdr) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sExtDirHdr)
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ExtDir ���������� ù��° ExtDesc�� RID�� ��ȯ�Ѵ�.
 ***********************************************************************/
sdRID sdpstExtDir::getFstExtRID( sdpstExtDirHdr  * aExtDirHdr )
{
    sdRID           sExtRID;
    sdpstExtDesc   *sFstExtDesc;

    IDE_ASSERT( aExtDirHdr != NULL);

    if ( aExtDirHdr->mExtCnt > 0 )
    {
        sFstExtDesc = getFstExtDesc( aExtDirHdr );
        sExtRID = sdpPhyPage::getRIDFromPtr( (void*)sFstExtDesc );
    }
    else
    {
        sExtRID = SD_NULL_RID;
    }
    return sExtRID;
}



/***********************************************************************
 *
 * Description : ���� Extent�κ��� ���ο� �������� �Ҵ��Ѵ�.
 *
 * [ ���� ]
 *
 * aPrvAllocExtRID�� ����Ű�� Extent�� aPrvAllocPageID����
 * Page�� �����ϴ� �ϴ��� üũ�ؼ� ������ ���ο� ����
 * Extent�� �̵��ϰ� ���� Extent�� ������ TBS�� ���� ���ο�
 * Extent�� �Ҵ�޴´�. ���� Extent���� Free Page�� ã�Ƽ�
 * Page�� �Ҵ�� ExtRID�� PageID�� �Ѱ��ش�.
 *
 * [ ���� ]
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aSegDesc         - [IN] Segment Extent List
 * aPrvAllocExtRID  - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aPrvAllocPageID  - [IN] ������ �Ҵ���� PageID
 * aMtx             - [IN] Mini Transaction Pointer
 * aAllocExtRID     - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aAllocPID        - [OUT] ���Ӱ� �Ҵ���� PageID
 * aParentInfo      - [OUT] Parent Info
 *
 ***********************************************************************/
IDE_RC sdpstExtDir::allocNewPageInExt( idvSQL         * aStatistics,
                                       sdrMtx         * aMtx,
                                       scSpaceID        aSpaceID,
                                       sdpSegHandle   * aSegHandle,
                                       sdRID            aPrvAllocExtRID,
                                       scPageID         aPrvAllocPageID,
                                       sdRID          * aAllocExtRID,
                                       scPageID       * aAllocPID,
                                       sdpParentInfo  * aParentInfo )
{
    UChar               * sPagePtr;
    sdpstExtDirHdr      * sExtDirHdr;
    SShort                sSlotNo;
    UInt                  sState = 0 ;
    sdRID                 sAllocExtRID;
    sdpstExtDesc        * sMapPtr;
    sdpstExtDesc        * sExtDesc;
    sdrMtxStartInfo       sStartInfo;
    scPageID              sNxtExtDirPID;

    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aMtx != NULL );
    IDE_ASSERT( aPrvAllocExtRID != SD_NULL_RID );
    IDE_ASSERT( aPrvAllocPageID != SD_NULL_PID );
    IDE_ASSERT( aAllocExtRID != NULL );
    IDE_ASSERT( aAllocPID != NULL );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    sAllocExtRID  = SD_NULL_RID;

    /* ������ �������� �Ҵ���� Extent�� aPrvAllocPageID ����
     * �������� �����ϴ� �� Check�Ѵ�. */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          SD_MAKE_PID( aPrvAllocExtRID ),
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sExtDirHdr = sdpstExtDir::getHdrPtr( sPagePtr );

    sSlotNo = calcOffset2SlotNo( sExtDirHdr,
                                 SD_MAKE_OFFSET( aPrvAllocExtRID ) );

    sMapPtr  = getMapPtr( sExtDirHdr );
    sExtDesc = &sMapPtr[sSlotNo];

    if( isFreePIDInExt( sExtDesc, aPrvAllocPageID ) == ID_FALSE )
    {
        /* aPrvAllocExtRID���� ���� ExtDesc�� �����ϴ��� Check�Ѵ�. */
        getNxtExtRID( sExtDirHdr, sSlotNo, &sAllocExtRID );

        if ( sAllocExtRID == SD_NULL_RID )
        {
            /* ���� ��� ���� ExtDir�� ù��° ExtDesc�� ��ȯ�Ѵ�. */
            sNxtExtDirPID = sdpstExtDir::getNxtExtDir( sExtDirHdr,
                                                       aSegHandle->mSegPID );
            if ( sNxtExtDirPID != SD_NULL_PID )
            {
                IDE_TEST( getFstExtRIDInNxtExtDir( aStatistics,
                                                   aSpaceID,
                                                   sNxtExtDirPID,
                                                   &sAllocExtRID )
                          != IDE_SUCCESS );
            }
        }

        if( sAllocExtRID == SD_NULL_RID )
        {
            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sExtDirHdr )
                      != IDE_SUCCESS );

            /* ���ο� Extent�� TBS�κ��� �Ҵ�޴´�. */
            IDE_TEST( sdpstSegDDL::allocNewExtsAndPage(
                               aStatistics,
                               &sStartInfo,
                               aSpaceID,
                               aSegHandle,
                               aSegHandle->mSegStoAttr.mNextExtCnt,
                               ID_FALSE, /* aNeedToUpdateHWM */
                               &sAllocExtRID,
                               NULL,     /* aMtx */
                               NULL      /* aFstDataPage */)
                      != IDE_SUCCESS );
        }

        // ������ Extent���� �Ҵ��� �������� PID �� leaf bmp�� ����
        // parent ������ ��ȯ�Ѵ�.
        IDE_TEST( getFstDataPIDOfExt( aStatistics,
                                      aSpaceID,
                                      sAllocExtRID,
                                      aAllocPID,
                                      aParentInfo ) != IDE_SUCCESS );

        /* ���ο� Extent�̱⶧���� Extent�� ù��° PID��
         * Free PageID�� ������ �ش�. */
        *aAllocExtRID = sAllocExtRID;
    }
    else
    {
        /* �� Extent���� �������� ���ӵǾ� �����Ƿ� ���ο�
         * PageID�� ���� Alloc�ߴ� �������� 1���� ���� �ȴ�.*/
        *aAllocPID    = aPrvAllocPageID + 1;
        *aAllocExtRID = aPrvAllocExtRID;

        IDE_TEST( calcLfBMPInfo( aStatistics,
                                  aSpaceID,
                                  sExtDesc,
                                  *aAllocPID,
                                  aParentInfo) != IDE_SUCCESS );
    }

    if ( sState == 1 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sExtDirHdr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               (UChar*)sExtDirHdr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� ExtDir �� ù��° Extent�� RID�� ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstExtDir::getFstExtRIDInNxtExtDir( idvSQL     * aStatistics,
                                             scSpaceID    aSpaceID,
                                             scPageID     aNxtExtDirPID,
                                             sdRID      * aFstExtRID )
{
    UChar           * sPagePtr;
    sdpstExtDirHdr  * sExtDirHdr;
    UInt              sState = 0 ;

    IDE_ASSERT( aNxtExtDirPID != SD_NULL_PID );
    IDE_ASSERT( aFstExtRID    != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aNxtExtDirPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sExtDirHdr  = sdpstExtDir::getHdrPtr( sPagePtr );
    *aFstExtRID = getFstExtRID( sExtDirHdr );

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sPagePtr ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �־��� Ext���� �Ҵ��� ������ ID�� leaf bmp�� ����
 *               ��ġ������ ��ȯ�Ѵ�
 ***********************************************************************/
IDE_RC sdpstExtDir::getFstDataPIDOfExt( idvSQL         * aStatistics,
                                        scSpaceID        aSpaceID,
                                        sdRID            aAllocExtRID,
                                        scPageID       * aAllocPID,
                                        sdpParentInfo  * aParentInfo )
{
    UInt           sState = 0 ;
    idBool         sDummy;
    sdpstExtDesc * sExtDescPtr;

    IDE_ASSERT( aAllocExtRID != SD_NULL_RID );
    IDE_ASSERT( aAllocPID != NULL );
    IDE_ASSERT( aParentInfo != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByRID( aStatistics,
                                          aSpaceID,
                                          aAllocExtRID,
                                          (UChar **)&sExtDescPtr,
                                          &sDummy ) != IDE_SUCCESS );
    sState = 1;

    *aAllocPID = sExtDescPtr->mExtFstDataPID;

    IDE_TEST( calcLfBMPInfo( aStatistics,
                              aSpaceID,
                              sExtDescPtr,
                              *aAllocPID,
                              aParentInfo ) != IDE_SUCCESS );
    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar*)sExtDescPtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             (UChar*)sExtDescPtr )
                    == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �־��� PID�� leaf bmp ������������ ��ġ������ ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstExtDir::calcLfBMPInfo( idvSQL          * aStatistics,
                                    scSpaceID         aSpaceID,
                                    sdpstExtDesc    * aExtDescPtr,
                                    scPageID          aPageID,
                                    sdpParentInfo   * aParentInfo )
{
    UInt            sGap;
    UInt            sShift;
    SShort          sPBSNo;
    UChar         * sPagePtr;
    sdpstLfBMPHdr * sLfBMPHdr;
    sdpstPageRange  sPageRange;
    UInt            sState = 0;

    IDE_ASSERT( aExtDescPtr != NULL );
    IDE_ASSERT( aPageID != SD_NULL_PID );
    IDE_ASSERT( aParentInfo != NULL );

    /* ���� Lf-BMP �� fix�Ͽ� PageRange ���� �����;� �Ѵ�.
     * �� Extent�� ������ ��� LfBMP�� PageRange�� ����. */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aExtDescPtr->mExtMgmtLfBMP,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sLfBMPHdr  = sdpstLfBMP::getHdrPtr( sPagePtr );
    sPageRange = sLfBMPHdr->mPageRange;

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sLfBMPHdr )
              != IDE_SUCCESS );

    if ( aExtDescPtr->mExtFstPID == aExtDescPtr->mExtMgmtLfBMP )
    {
        // leaf bmp�� ������ ���
        sGap = aPageID - aExtDescPtr->mExtFstPID;
        aParentInfo->mIdxInParent = sGap % sPageRange;
    }
    else
    {
        if ( (aExtDescPtr->mExtFstPID < aExtDescPtr->mExtMgmtLfBMP) &&
             ((aExtDescPtr->mExtFstPID + aExtDescPtr->mLength) >
              aExtDescPtr->mExtMgmtLfBMP))
        {
            // leaf bmp�� ����������, ù��° �������� ExtDir�� ���ǰ�
            // �ٷ� ���� �������� ù��° lf bmp�� ������ ���
            sGap = aPageID - aExtDescPtr->mExtFstPID - 1;
            aParentInfo->mIdxInParent = sGap % sPageRange;
        }
        else
        {
            // ���� leaf bmp���� �����Ǵ� ���
            sGap = 0;
            IDE_TEST( sdpstLfBMP::getPBSNoByExtFstPID(
                                        aStatistics,
                                        aSpaceID,
                                        aExtDescPtr->mExtMgmtLfBMP,
                                        aExtDescPtr->mExtFstPID,
                                        &sPBSNo ) != IDE_SUCCESS );

            aParentInfo->mIdxInParent =
                sPBSNo + (aPageID - aExtDescPtr->mExtFstPID);
        }
    }

    /* Extent�� PageRange�� ���� �����Ǳ� ���� */
    sShift = sGap / sPageRange;
    aParentInfo->mParentPID = aExtDescPtr->mExtMgmtLfBMP + sShift;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, (UChar *)sLfBMPHdr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Extent�� RID�� ����� ����.
 ***********************************************************************/
IDE_RC sdpstExtDir::makeExtRID( idvSQL          * aStatistics,
                                scSpaceID         aSpaceID,
                                scPageID          aExtDirPID,
                                SShort            aSlotNoInExtDir,
                                sdRID           * aExtRID )
{
    UChar           * sPagePtr;
    sdpstExtDirHdr  * sExtDirHdr;
    sdRID             sExtRID;
    scOffset          sOffset;
    UInt              sState = 0;

    IDE_ASSERT( aExtDirPID      != SD_NULL_PID );
    IDE_ASSERT( aSlotNoInExtDir != SDPST_INVALID_SLOTNO );
    IDE_ASSERT( aExtRID         != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aExtDirPID,
                                          &sPagePtr ) != IDE_SUCCESS );
    sState = 1;

    sExtDirHdr = getHdrPtr( sPagePtr );

    if ( sExtDirHdr->mExtCnt < aSlotNoInExtDir )
    {
        ideLog::log( IDE_SERVER_0,
                     "Extent Count: %d, "
                     "SlotNo: %d\n",
                     sExtDirHdr->mExtCnt,
                     aSlotNoInExtDir );

        (void)dump( sPagePtr );

        IDE_ASSERT( 0 );
    }

    sOffset = calcSlotNo2Offset( sExtDirHdr, aSlotNoInExtDir );

    sExtRID = SD_MAKE_RID( aExtDirPID, sOffset );

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr ) != IDE_SUCCESS );

    *aExtRID = sExtRID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


IDE_RC sdpstExtDir::makeSeqNo( idvSQL           * aStatistics,
                               scSpaceID          aSpaceID,
                               scPageID           aSegPID,
                               sdRID              aExtRID,
                               scPageID           aPageID,
                               ULong            * aSeqNo )
{
    UChar           * sPagePtr;
    sdpstExtDirHdr  * sExtDirHdr;
    scPageID          sExtDirPID;
    SShort            sSlotNoInExtDir;
    UInt              sPageCntInExt;
    sdpstExtDesc    * sMapPtr;
    sdpstExtDesc    * sFstExtDesc;
    sdpstExtDesc    * sCurExtDesc;
    ULong             sCurSeqNo;
    ULong             sFstSeqNoInExtDir;
    UInt              sState = 0;

    sExtDirPID = SD_MAKE_PID( aExtRID );

    sState = 0;
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          sExtDirPID,
                                          &sPagePtr ) != IDE_SUCCESS );
    sState = 1;

    sExtDirHdr      = getHdrPtr( sPagePtr );
    sSlotNoInExtDir = calcOffset2SlotNo( sExtDirHdr, SD_MAKE_OFFSET(aExtRID) );

    sMapPtr         = getMapPtr( sExtDirHdr );
    sFstExtDesc     = getFstExtDesc( sExtDirHdr );
    sCurExtDesc     = &sMapPtr[sSlotNoInExtDir];

    /* Extent�� page ������ �����´�. */
    sPageCntInExt   = sFstExtDesc->mLength;


    /*
     * seqno�� extdir �������� seqno���� ���° �Ҵ�� ������������ �˸�
     * ����� �����Ѵ�.
     *      ExtDir�� seqno +
     *      ((�Ҵ�� Extent ���� - 1) * Extent�� ��������) +
     *      �ش� �������� ���� Extent���� �ش� �������� ����
     * �� SegHdr�� ���Ե� ExtDir�ΰ�� �̿� ���� ����� �ؾ��Ѵ�.
     */
    if ( aSegPID == sExtDirPID )
    {
        /* SegHdr�� ù��° Extent�� �Ҵ�ɶ� �����ȴ�.
         * SeqNo�� 0���� �����ϱ� ������ ���⼭ 0���� �����Ѵ�. */
        sFstSeqNoInExtDir = 0;
    }
    else
    {
        sFstSeqNoInExtDir = sdpPhyPage::getSeqNo(
                                    sdpPhyPage::getHdr(sPagePtr) );
    }

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr ) != IDE_SUCCESS );

    sCurSeqNo = sFstSeqNoInExtDir +
                ( sSlotNoInExtDir * sPageCntInExt ) +
                ( aPageID - sCurExtDesc->mExtFstPID );

    *aSeqNo = sCurSeqNo;

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
 * Description : aExtRID���� aExtRID�� �����ϴ� LfBMP�� ���Ե� ������
 *               Extent���� format�Ѵ�.
 *
 * aStatistics           - [IN] �������
 * aStartInfo            - [IN] Start Info
 * aSpaceID              - [IN] Tablespace ID
 * aSegHandle            - [IN] Segment Handle
 * aExtRID               - [IN] format�� ������ Extent RID
 * aFstFmtPID            - [OUT] format�� ù��° PID
 * aLstFmtPID            - [OUT] format�� ������ PID
 * aLstFmtExtDirPID      - [OUT] format�� ������ �������� ������ ExtDir PID
 * aLstFmtSlotNoInExtDir - [OUT] format�� ������ �������� ������ ExtDesc SlotNo
 ***********************************************************************/ 
IDE_RC sdpstExtDir::formatPageUntilNxtLfBMP( idvSQL          * aStatistics,
                                             sdrMtxStartInfo * aStartInfo,
                                             scSpaceID         aSpaceID,
                                             sdpSegHandle    * aSegHandle,
                                             sdRID             aExtRID,
                                             scPageID        * aFstFmtPID,
                                             scPageID        * aLstFmtPID,
                                             scPageID        * aLstFmtExtDirPID,
                                             SShort          * aLstFmtSlotNoInExtDir )
{
    UChar           * sPagePtr;
    sdpstExtDesc    * sExtDescPtr;
    sdpstExtDesc      sExtDesc;
    sdRID             sCurExtRID;
    sdRID             sNxtExtRID;
    scPageID          sFstExtMgmtLfBMP;
    scPageID          sFstFmtPID;
    scPageID          sLstFmtPID;
    scPageID          sLstFmtExtDirPID;
    SShort            sLstFmtSlotNoInExtDir;
    UInt              sState = 0;

    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aExtRID    != SD_NULL_RID );
    IDE_ASSERT( aFstFmtPID != NULL );
    IDE_ASSERT( aLstFmtPID != NULL );
    IDE_ASSERT( aLstFmtExtDirPID      != NULL );
    IDE_ASSERT( aLstFmtSlotNoInExtDir != NULL );

    sCurExtRID       = aExtRID;
    sFstFmtPID       = SD_NULL_PID;
    sLstFmtPID       = SD_NULL_PID;
    sFstExtMgmtLfBMP = SD_NULL_PID;
    sLstFmtExtDirPID = SD_NULL_PID;
    sLstFmtSlotNoInExtDir = SDPST_INVALID_SLOTNO;

    while ( 1 )
    {
        IDE_TEST( sdbBufferMgr::getPageByRID( aStatistics,
                                              aSpaceID,
                                              sCurExtRID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              NULL, /* sdrMtx */
                                              (UChar**)&sExtDescPtr,
                                              NULL  /* aTruSuccess */ )
                  != IDE_SUCCESS );
        sState = 1;

        sExtDesc = *sExtDescPtr;

        if ( sFstExtMgmtLfBMP == SD_NULL_PID )
        {
            sFstExtMgmtLfBMP = sExtDesc.mExtMgmtLfBMP;
        }

        /* LfBMP�� ����Ǿ����� �����Ѵ�. */
        if ( sFstExtMgmtLfBMP != sExtDesc.mExtMgmtLfBMP )
        {
            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 (UChar*)sExtDescPtr )
                      != IDE_SUCCESS );
            break;
        }

        sPagePtr = sdpPhyPage::getPageStartPtr( (UChar*)sExtDescPtr );

        sLstFmtExtDirPID      = SD_MAKE_PID( sCurExtRID );
        sLstFmtSlotNoInExtDir = calcOffset2SlotNo( getHdrPtr(sPagePtr),
                                                   SD_MAKE_OFFSET(sCurExtRID) );

        if ( (sdpPhyPage::getPageID( sPagePtr ) != sLstFmtExtDirPID) ||
             (sLstFmtExtDirPID      == SD_NULL_PID) ||
             (sLstFmtSlotNoInExtDir == SDPST_INVALID_SLOTNO) )
        {
            ideLog::log( IDE_SERVER_0,
                         "ExtRID: %llu, "
                         "PID In PageHdr: %u, "
                         "LastFmtExtDirPID: %u, "
                         "SlotNoInExtDir: %d\n",
                         sCurExtRID,
                         sdpPhyPage::getPageID(sPagePtr),
                         sLstFmtExtDirPID,
                         sLstFmtSlotNoInExtDir );

            (void)dump( sPagePtr );

            IDE_ASSERT( 0 );
        }

        if ( sFstFmtPID == SD_NULL_PID )
        {
            sFstFmtPID = sExtDesc.mExtFstDataPID;
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sExtDescPtr )
                  != IDE_SUCCESS );

        /* �� Extent�� Data Page�� format�Ѵ�. */
        IDE_TEST( sdpstAllocPage::formatDataPagesInExt(
                                                aStatistics,
                                                aStartInfo,
                                                aSpaceID,
                                                aSegHandle,
                                                &sExtDesc,
                                                sCurExtRID,
                                                sExtDesc.mExtFstPID )
                != IDE_SUCCESS );

        sLstFmtPID = sExtDesc.mExtFstPID + sExtDesc.mLength - 1;

        /* ���� Extent�� �����´�. */
        IDE_TEST( sdpstExtDir::getNxtExtRID( aStatistics,
                                             aSpaceID,
                                             aSegHandle->mSegPID,
                                             sCurExtRID,
                                             &sNxtExtRID ) != IDE_SUCCESS );

        if ( sNxtExtRID == SD_NULL_RID )
        {
            break;
        }

        sCurExtRID = sNxtExtRID;
    }

    /* output ������ �����Ѵ�. */
    *aFstFmtPID            = sFstFmtPID;
    *aLstFmtPID            = sLstFmtPID;
    *aLstFmtExtDirPID      = sLstFmtExtDirPID;
    *aLstFmtSlotNoInExtDir = sLstFmtSlotNoInExtDir;

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
 * Description : ExtDir�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstExtDir::dumpHdr( UChar    * aPagePtr,
                             SChar    * aOutBuf,
                             UInt       aOutSize )
{
    UChar           * sPagePtr;
    sdpstExtDirHdr  * sExtDirHdr;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPagePtr   = sdpPhyPage::getPageStartPtr( aPagePtr );
    sExtDirHdr = getHdrPtr( sPagePtr );

    /* ExtDir Header */
    idlOS::snprintf(
                 aOutBuf,
                 aOutSize,
                 "--------------- ExtDir Header Begin ----------------\n"
                 "Extent Count          : %"ID_UINT32_FMT"\n"
                 "Max Extent Count      : %"ID_UINT32_FMT"\n"
                 "Body Offset           : %"ID_UINT32_FMT"\n"
                 "---------------  ExtDir Header End  ----------------\n",
                 sExtDirHdr->mExtCnt,
                 sExtDirHdr->mMaxExtCnt,
                 sExtDirHdr->mBodyOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstExtDir::dumpBody( UChar    * aPagePtr,
                              SChar    * aOutBuf,
                              UInt       aOutSize )
{
    UChar           * sPagePtr;
    sdpstExtDirHdr  * sExtDirHdr;
    sdpstExtDesc    * sExtDesc;
    UInt              sLoop;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPagePtr   = sdpPhyPage::getPageStartPtr( aPagePtr );
    sExtDirHdr = getHdrPtr( sPagePtr );

    /* ExtDir Body */
    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "--------------- ExtDir Body Begin ----------------\n" );

    for ( sLoop = 0; sLoop < sExtDirHdr->mExtCnt; sLoop++ )
    {
        sExtDesc = getMapPtr( sExtDirHdr ) + sLoop;

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%"ID_UINT32_FMT"] "
                             "FstPID: %"ID_UINT32_FMT", "
                             "Length: %"ID_UINT32_FMT", "
                             "MgmtLfBMP: %"ID_UINT32_FMT", "
                             "FstDataPID: %"ID_UINT32_FMT"\n",
                             sLoop,
                             sExtDesc->mExtFstPID,
                             sExtDesc->mLength,
                             sExtDesc->mExtMgmtLfBMP,
                             sExtDesc->mExtFstDataPID );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "---------------- ExtDir Body End -----------------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir�� dump�Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdpstExtDir::dump( UChar    * aPagePtr )
{
    UChar    * sPagePtr;
    SChar    * sDumpBuf;

    IDE_ERROR( aPagePtr != NULL );

    sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );

    ideLog::log( IDE_SERVER_0,
                 "==========================================================" );

    /* Physical Page */
    (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                 sPagePtr,
                                 "Physical Page:" );
    if( iduMemMgr::calloc(
            IDU_MEM_SM_SDP, 1,
            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
            (void**)&sDumpBuf ) == IDE_SUCCESS )
    {
        /* ExtDir Header */
        if( dumpHdr( sPagePtr,
                     sDumpBuf,
                     IDE_DUMP_DEST_LIMIT ) == IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "%s", sDumpBuf );
        }

        if( dumpBody( sPagePtr,
                      sDumpBuf,
                      IDE_DUMP_DEST_LIMIT ) == IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "%s", sDumpBuf );
        }

        (void)iduMemMgr::free( sDumpBuf );
    }

    ideLog::log( IDE_SERVER_0,
                 "==========================================================" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
