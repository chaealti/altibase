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
 * $Id: sdpstSH.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Segment Header ����
 * STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <smErrorCode.h>
# include <sdr.h>

# include <sdpReq.h>
# include <sdpPhyPage.h>

# include <sdpstDef.h>
# include <sdpstSH.h>

# include <sdpstAllocPage.h>
# include <sdpstExtDir.h>
# include <sdpstBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstItBMP.h>
# include <sdpstRtBMP.h>
# include <sdpstStackMgr.h>
# include <sdpstCache.h>
# include <ideErrorMgr.h>

/***********************************************************************
 * Description : Water Mark ����
 *
 * aWM                - [IN] Water Mark�� ������ ����
 * aWMPID             - [IN] Water Mark PID
 * aExtDirPID         - [IN] Water Mark Page�� ���Ե� ExtDir PID
 * aExtSlotNoInExtDir - [IN] Water Mark Page�� ������ ExtDesc�� SlotNo
 * aPosStack          - [IN] Water Mark Stack
 ***********************************************************************/
void sdpstSH::updateWM( sdpstWM        * aWM,
                        scPageID         aWMPID,
                        scPageID         aExtDirPID,
                        SShort           aExtSlotNoInExtDir,
                        sdpstStack     * aPosStack )
{
    IDE_ASSERT( aWM != NULL );

    if ( sdpstStackMgr::getDepth( aPosStack ) != SDPST_LFBMP )
    {
        sdpstStackMgr::dump( aPosStack );
        IDE_ASSERT( 0 );
    }

    aWM->mWMPID           = aWMPID;
    aWM->mExtDirPID       = aExtDirPID;
    aWM->mSlotNoInExtDir  = aExtSlotNoInExtDir;
    idlOS::memcpy( &(aWM->mStack), aPosStack, ID_SIZEOF(sdpstStack) );
}

/***********************************************************************
 * Description : Segment Header�� �ʱ�ȭ �Ѵ�.
 *
 * aSegHdr              - [IN] Segment Header
 * aSegHdrPID           - [IN] Segment Header PID
 * aSegType             - [IN] Segment Type
 * aPageCntInExt        - [IN] Extent �� ������ ����
 * aMaxSlotCntInExtDir  - [IN] Segment Header�� ���Ե� ExtDir�� �ִ� Slot ����
 * aMaxSlotCntInRtBMP   - [IN] Segment Header�� ���Ե� RtBMP�� �ִ� Slot ����
 ***********************************************************************/
void sdpstSH::initSegHdr( sdpstSegHdr       * aSegHdr,
                          scPageID            aSegHdrPID,
                          sdpSegType          aSegType,
                          UInt                aPageCntInExt,
                          UShort              aMaxSlotCntInExtDir,
                          UShort              aMaxSlotCntInRtBMP )
{
    scOffset   sBodyOffset;
    idBool     sDummy;

    IDE_ASSERT( aSegHdr != NULL );

    /* Segment Control Header �ʱ�ȭ */
    aSegHdr->mSegType           = aSegType;
    aSegHdr->mSegState          = SDP_SEG_USE;
    aSegHdr->mLstLfBMP          = SD_NULL_PID;
    aSegHdr->mLstItBMP          = SD_NULL_PID;
    aSegHdr->mLstRtBMP          = aSegHdrPID;
    aSegHdr->mSegHdrPID         = aSegHdrPID;

    aSegHdr->mTotPageCnt        = 0;
    aSegHdr->mFreeIndexPageCnt  = 0;
    aSegHdr->mTotExtCnt         = 0;
    aSegHdr->mPageCntInExt      = aPageCntInExt;

    initWM( &aSegHdr->mHWM );

    // rt-bmp control header�� �ʱ�ȭ�Ͽ����Ƿ� sdpPhyPageHdr��
    // freeOffset�� total free size�� �����Ѵ�.
    sdpPhyPage::initLogicalHdr( sdpPhyPage::getHdr((UChar*)aSegHdr),
                                ID_SIZEOF(sdpstSegHdr) );

    sBodyOffset = sdpPhyPage::getDataStartOffset( ID_SIZEOF(sdpstSegHdr) );

    /* ExtDir Control Header �ʱ�ȭ */
    sdpstExtDir::initExtDirHdr( &aSegHdr->mExtDirHdr,
                                aMaxSlotCntInExtDir,
                                sBodyOffset );

    /* Segment Header�� �ִ��� ����� �� �ִ� ExtentDir Slot��
     * ������ ����Ͽ� rt-bmp ������ map�� ����Ѵ�. */
    sBodyOffset += aMaxSlotCntInExtDir * ID_SIZEOF(sdpstExtDesc);

    /* Root Bitmap Control Header �ʱ�ȭ */
    sdpstBMP::initBMPHdr( &aSegHdr->mRtBMPHdr,
                          SDPST_RTBMP,
                          sBodyOffset,
                          SD_NULL_PID,
                          0,
                          SD_NULL_PID,
                          SD_NULL_PID,
                          0,
                          aMaxSlotCntInRtBMP,
                          &sDummy );
}

/***********************************************************************
 * Description : HWM�� �ʱ�ȭ�Ѵ�.
 *
 * aWM      - [IN] �ʱ�ȭ�� WM
 ***********************************************************************/
void sdpstSH::initWM( sdpstWM  * aWM )
{
    aWM->mWMPID          = SD_NULL_PID;
    aWM->mExtDirPID      = SD_NULL_PID;
    aWM->mSlotNoInExtDir = SDPST_INVALID_SLOTNO;

    sdpstStackMgr::initialize( &aWM->mStack );
}

/***********************************************************************
 * Description : Segment Header ������ ���� �� �ʱ�ȭ
 *
 * ù��° Extent�� bmp �������� ��� ������ �� Segment Header
 * �������� �����Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aStartInfo    - [IN] Mtx�� StartInfo
 * aSpaceID      - [IN] Tablespace�� ID
 * aExtDesc      - [IN] Extent Desc Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 * aSegCache     - [IN] Segment Cache
 * aSegPID       - [OUT] Segment ��� �������� PID
 ***********************************************************************/
IDE_RC sdpstSH::createAndInitPage( idvSQL               * aStatistics,
                                   sdrMtxStartInfo      * aStartInfo,
                                   scSpaceID              aSpaceID,
                                   sdpstExtDesc         * aExtDesc,
                                   sdpstBfrAllocExtInfo * aBfrInfo,
                                   sdpstAftAllocExtInfo * aAftInfo,
                                   sdpstSegCache        * aSegCache,
                                   scPageID             * aSegPID )
{
    sdrMtx              sMtx;
    UInt                sState = 0 ;
    UChar             * sPagePtr;
    UShort              sMetaPageCnt;
    sdpstPBS            sPBS;
    scPageID            sSegHdrPID;
    sdpstSegHdr       * sSegHdrPtr;
    ULong               sSeqNo;
    sdpstRedoInitSegHdr sLogData;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aSegCache  != NULL );
    IDE_ASSERT( aExtDesc   != NULL );
    IDE_ASSERT( aAftInfo   != NULL );
    IDE_ASSERT( aSegPID    != NULL );

    /* ù��° Extent�� �ʿ��� bmp ���������� ��� �����Ͽ����Ƿ�,
     * Segment Header �������� PID�� ����Ѵ�. */
    sMetaPageCnt = (aAftInfo->mPageCnt[SDPST_RTBMP] +
                    aAftInfo->mPageCnt[SDPST_ITBMP] +
                    aAftInfo->mPageCnt[SDPST_LFBMP] +
                    aAftInfo->mPageCnt[SDPST_EXTDIR]);

    /* SegHdr �������� ������ SeqNo �� SegPID�� ����Ѵ�. */
    sSeqNo     = aBfrInfo->mNxtSeqNo + sMetaPageCnt;
    sSegHdrPID = aExtDesc->mExtFstPID + (UInt)sMetaPageCnt;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    /*
     * ù��° Extent���� Segment Header�� �Ҵ��Ѵ�.
     * ������ Segment Header �������� PID�� ��ȯ�Ѵ�.
     */
    sPBS = ( SDPST_BITSET_PAGETP_META | SDPST_BITSET_PAGEFN_FUL );

    /* logical header�� �Ʒ����� ������ �ʱ�ȭ�Ѵ�. */
    IDE_TEST( sdpstAllocPage::createPage(
                                    aStatistics,
                                    &sMtx,
                                    aSpaceID,
                                    NULL, /* aSegHandle4DataPage */
                                    sSegHdrPID,
                                    sSeqNo,
                                    SDP_PAGE_TMS_SEGHDR,
                                    aExtDesc->mExtFstPID,
                                    SDPST_INVALID_PBSNO,
                                    sPBS,
                                    &sPagePtr ) != IDE_SUCCESS );

    sSegHdrPtr = getHdrPtr(sPagePtr);

    /* Segment Header ������ �ʱ�ȭ */
    initSegHdr( sSegHdrPtr,
                sSegHdrPID,
                aSegCache->mSegType,
                aExtDesc->mLength,
                aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR],
                aBfrInfo->mMaxSlotCnt[SDPST_RTBMP] );

    /* INIT_SEGMENT_META_HEADER �α� */
    sLogData.mSegType               = aSegCache->mSegType;
    sLogData.mSegPID                = sSegHdrPID;
    sLogData.mPageCntInExt          = aExtDesc->mLength;
    sLogData.mMaxExtDescCntInExtDir = aBfrInfo->mMaxSlotCnt[SDPST_EXTDIR];
    sLogData.mMaxSlotCntInRtBMP     = aBfrInfo->mMaxSlotCnt[SDPST_RTBMP];

    IDE_TEST( sdrMiniTrans::writeLogRec( &sMtx,
                                         (UChar*)sSegHdrPtr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_INIT_SEGHDR )
              != IDE_SUCCESS );

    IDE_TEST( sdpDblPIDList::initBaseNode( &sSegHdrPtr->mExtDirBase,
                                           &sMtx ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    *aSegPID = sSegHdrPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header�� WM�� �����Ѵ�.
 *
 * aMtx             - [IN] Mini Transaction Pointer
 * aWM              - [IN] ������ WM ����
 * aWMPID           - [IN] Water Mark PID
 * aExtDirPID       - [IN] Water Mark Page�� ���Ե� ExtDir PID
 * aSlotNoInExtDir  - [IN] Water Mark Page�� ������ ExtDesc�� SlotNo
 * aStack           - [IN] Water Mark Stack
 ***********************************************************************/
IDE_RC sdpstSH::logAndUpdateWM( sdrMtx         * aMtx,
                                sdpstWM        * aWM,
                                scPageID         aWMPID,
                                scPageID         aExtDirPID,
                                SShort           aSlotNoInExtDir,
                                sdpstStack     * aStack )
{
    sdpstRedoUpdateWM   sLogData;

    IDE_DASSERT( aWM             != NULL );
    IDE_DASSERT( aWMPID          != SD_NULL_PID );
    IDE_DASSERT( aExtDirPID      != SD_NULL_PID );
    IDE_DASSERT( aSlotNoInExtDir != SDPST_INVALID_SLOTNO );
    IDE_DASSERT( aStack          != NULL );
    IDE_DASSERT( aMtx            != NULL );

    // HWM �����ϱ�
    updateWM( aWM, aWMPID, aExtDirPID, aSlotNoInExtDir, aStack );

    // UPDATE WM logging
    sLogData.mWMPID          = aWMPID;
    sLogData.mExtDirPID      = aExtDirPID;
    sLogData.mSlotNoInExtDir = aSlotNoInExtDir;
    sLogData.mStack          = *aStack;

    IDE_TEST( sdrMiniTrans::writeLogRec(
                  aMtx,
                  (UChar*)aWM,
                  &sLogData,
                  ID_SIZEOF( sLogData ),
                  SDR_SDPST_UPDATE_WM ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header �������� ���ο� ExtDir Page�� ����
 *
 * aMtx              - [IN] Mini Transaction Pointer
 * aSegHdrPagePtr    - [IN] Segment Header Page Pointer
 * aNewExtDirPagePtr - [IN] ExtDir Page Pointer
 ***********************************************************************/
IDE_RC  sdpstSH::addNewExtDirPage( sdrMtx    * aMtx,
                                   UChar     * aSegHdrPagePtr,
                                   UChar     * aNewExtDirPagePtr )
{
    sdpstSegHdr         * sSegHdr;
    sdpPhyPageHdr       * sPhyPageHdr;
    sdpDblPIDListNode   * sListNode;

    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aSegHdrPagePtr    != NULL );
    IDE_ASSERT( aNewExtDirPagePtr != SD_NULL_PID );

    sSegHdr     = getHdrPtr( aSegHdrPagePtr );
    sPhyPageHdr = sdpPhyPage::getHdr( aNewExtDirPagePtr );
    sListNode   = sdpPhyPage::getDblPIDListNode( sPhyPageHdr );

    IDE_TEST( sdpDblPIDList::insertTailNode( NULL /*aStatistics*/,
                                             &sSegHdr->mExtDirBase,
                                             sListNode,
                                             aMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Extent �Ҵ��� ���ο� ������ Bitmap ������ ������
 *               Segment Header�� �ݿ��Ѵ�.
 *
 * A. Last Bitmap �������� ���� �����Ѵ�.
 * B. Segment Size�� �����Ѵ�.
 *
 * aStartInfo     - [IN] Mtx�� StartInfo
 * aSegHdrPagePtr - [IN] Segment Header Page Pointer
 * aAllocPageCnt  - [IN] �Ҵ�� ������ ����
 * aNewRtBMPCnt   - [IN] ������ RtBMP����
 * aLstLfBMP      - [IN] ������ ������ LfBMP
 * aLstItBMP      - [IN] ������ ������ ItBMP
 * aLstRtBMP      - [IN] ������ ������ RtBMP
 * aBfrLstRtBMP   - [OUT] ������ ������ RtBMP
 ***********************************************************************/
IDE_RC sdpstSH::logAndLinkBMPs( sdrMtx       * aMtx,
                                sdpstSegHdr  * aSegHdr,
                                ULong          aAllocPageCnt,
                                ULong          aMetaPageCnt,
                                UShort         aNewRtBMPCnt,
                                scPageID       aLstRtBMP,
                                scPageID       aLstItBMP,
                                scPageID       aLstLfBMP,
                                scPageID     * aBfrLstRtBMP )
{
    sdpstRedoAddExtToSegHdr   sLogData;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSegHdr      != NULL );
    IDE_ASSERT( aLstRtBMP    != SD_NULL_PID );
    IDE_ASSERT( aLstItBMP    != SD_NULL_PID );
    IDE_ASSERT( aLstLfBMP    != SD_NULL_PID );
    IDE_ASSERT( aBfrLstRtBMP != NULL );

    /* linkBMPsToSegHdr()���� ����Ǳ� ������ �ӽ� ���� */
    *aBfrLstRtBMP = aSegHdr->mLstRtBMP;

    linkBMPsToSegHdr( aSegHdr,
                      aAllocPageCnt,
                      aMetaPageCnt,
                      aLstLfBMP,
                      aLstItBMP,
                      aLstRtBMP,
                      aNewRtBMPCnt );

    sLogData.mAllocPageCnt = aAllocPageCnt;
    sLogData.mMetaPageCnt = aMetaPageCnt;
    sLogData.mNewLstLfBMP = aLstLfBMP;
    sLogData.mNewLstItBMP = aLstItBMP;
    sLogData.mNewLstRtBMP = aLstRtBMP;
    sLogData.mNewRtBMPCnt = aNewRtBMPCnt;

    // SDR_SDPST_ADD_EXT_TO_SEGHDR �� Total Page Count logging
    IDE_TEST( sdrMiniTrans::writeLogRec(
                            aMtx,
                            (UChar*)aSegHdr,
                            &sLogData,
                            ID_SIZEOF( sLogData ),
                            SDR_SDPST_ADD_EXT_TO_SEGHDR) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �Ҵ�� ���ο� Extent�� Bitmap ���������� Segment
 *               Header�� �����ϰ�, Segment ũ�⸦ ������Ų��.
 *
 * aSegHdr          - [IN] Segment Header
 * aAllocPageCnt    - [IN] �Ҵ��� Page Count
 * aNewLstLfBMP     - [IN] ������ ������ LfBMP
 * aNewLstItBMP     - [IN] ������ ������ ItBMP
 * aNewLstRtBMP     - [IN] ������ ������ RtBMP
 * aNewRtBMPCnt     - [IN] ������ RtBMP Count
 **********************************************************************/
void sdpstSH::linkBMPsToSegHdr( sdpstSegHdr  * aSegHdr,
                                ULong          aAllocPageCnt,
                                ULong          aMetaPageCnt,
                                scPageID       aNewLstLfBMP,
                                scPageID       aNewLstItBMP,
                                scPageID       aNewLstRtBMP,
                                UShort         aNewRtBMPCnt )
{
    IDE_DASSERT( aSegHdr      != NULL );
    IDE_DASSERT( aAllocPageCnt > 0 );
    IDE_DASSERT( aNewLstLfBMP != SD_NULL_PID );
    IDE_DASSERT( aNewLstItBMP != SD_NULL_PID );
    IDE_DASSERT( aNewLstRtBMP != SD_NULL_PID );

    /* �Ҵ�� Extent ��ŭ�� �뷮�� Segment�� �ݿ��Ѵ�. */
    aSegHdr->mTotPageCnt  += aAllocPageCnt;
    aSegHdr->mTotExtCnt   += 1;
    aSegHdr->mTotRtBMPCnt += aNewRtBMPCnt;
    aSegHdr->mFreeIndexPageCnt += aAllocPageCnt - aMetaPageCnt;
    aSegHdr->mLstSeqNo     = aSegHdr->mTotPageCnt - 1;

    /* ���ο� ������ Bitmap ������ ������ �����Ѵ�. */
    aSegHdr->mLstLfBMP = aNewLstLfBMP;
    aSegHdr->mLstItBMP = aNewLstItBMP;
    aSegHdr->mLstRtBMP = aNewLstRtBMP;
}


/***********************************************************************
 *
 * Description : Sequential Scan�� ���� Segment�� ������ ��ȯ�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aSegPID      - [IN] ���׸�Ʈ ��� �������� PID
 * aSegInfo     - [OUT] ������ ���׸�Ʈ ���� �ڷᱸ��
 *
 ***********************************************************************/
IDE_RC sdpstSH::getSegInfo( idvSQL        * aStatistics,
                            scSpaceID       aSpaceID,
                            scPageID        aSegPID,
                            void          * aTableHeader,
                            sdpSegInfo    * aSegInfo )
{
    UChar            * sPagePtr;
    sdpstSegHdr      * sSegHdr;
    sdpstWM          * sHWM;
    SInt               sState = 0;

    IDE_ASSERT( aSegInfo != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );
    sState = 1;

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    /* BUG-43084 X$table_info��ȸ �� ���������� �ϰų�
     * X_LOCK�� �ɸ� disk table�� ��ȸ���� ���� �� �ֽ��ϴ�. */
    if ( ( aTableHeader != NULL ) && 
         ( smLayerCallback::isNullSegPID4DiskTable( aTableHeader ) == ID_TRUE ) )
    {
        aSegInfo->mSegHdrPID = SD_NULL_PID;
        IDE_CONT( TABLE_IS_BEING_DROPPED );
    }
    else
    {
        /* nothing to do */
    }

    aSegInfo->mSegHdrPID    = aSegPID;
    aSegInfo->mType         = (sdpSegType)(sSegHdr->mSegType);
    aSegInfo->mState        = (sdpSegState)(sSegHdr->mSegState);

    aSegInfo->mPageCntInExt = sSegHdr->mPageCntInExt;
    aSegInfo->mFmtPageCnt   = sSegHdr->mTotPageCnt;
    aSegInfo->mExtCnt       = sSegHdr->mTotExtCnt;
    aSegInfo->mExtDirCnt    = sSegHdr->mExtDirBase.mNodeCnt + 1; /* 1�� Seghdr */

    if ( aSegInfo->mExtCnt <= 0 )
    {
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    aSegInfo->mFstExtRID    =
        sdpstExtDir::getFstExtRID( &sSegHdr->mExtDirHdr );

    /*
     * BUG-22474     [valgrind]sdbMPRMgr::getMPRCnt�� UMR�ֽ��ϴ�.
     */
    aSegInfo->mLstExtRID    = sSegHdr->mLstExtRID;

    /* HWM�� ExtDesc�� RID�� ���Ѵ�. */
    sHWM = &(sSegHdr->mHWM);
    aSegInfo->mHWMPID      = sHWM->mWMPID;

    if ( sHWM->mExtDirPID != aSegPID )
    {
        aSegInfo->mExtRIDHWM  = SD_MAKE_RID(
            sHWM->mExtDirPID,
            sdpstExtDir::calcSlotNo2Offset( NULL,
                                          sHWM->mSlotNoInExtDir ));
    }
    else
    {
        aSegInfo->mExtRIDHWM = SD_MAKE_RID(
            sHWM->mExtDirPID,
            sdpstExtDir::calcSlotNo2Offset( &(sSegHdr->mExtDirHdr),
                                          sHWM->mSlotNoInExtDir ));
    }

    aSegInfo->mLstAllocExtRID      = aSegInfo->mExtRIDHWM;
    aSegInfo->mFstPIDOfLstAllocExt = aSegInfo->mExtRIDHWM;

    IDE_EXCEPTION_CONT( TABLE_IS_BEING_DROPPED );

    sState = 0;

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, (UChar*)sSegHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, (UChar*)sSegHdr )
                    == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] segment ���¸� ��ȯ�Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegPID       - [IN] Segment ��� �������� PID
 * aSegState     - [IN] Segment ����
 ***********************************************************************/
IDE_RC sdpstSH::getSegState( idvSQL        *aStatistics,
                             scSpaceID      aSpaceID,
                             scPageID       aSegPID,
                             sdpSegState   *aSegState )
{
    idBool        sDummy;
    UChar       * sPagePtr;
    sdpSegState   sSegState;
    UInt          sState = 0 ;

    IDE_ASSERT( aSegPID != SD_NULL_PID );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          &sPagePtr,
                                          &sDummy ) != IDE_SUCCESS );
    sState = 1;

    sSegState = getHdrPtr( sPagePtr )->mSegState;

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    *aSegState = sSegState;

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
 * Description : Segment Header�� Meta �������� PID�� �����Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aMtx          - [IN] Mini Transaction Pointer
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegPID       - [IN] Segment ��� �������� PID
 * aIndex        - [IN] Meta PID Array �� Index
 * aMetaPID      - [IN] ������ MetaPID
 ************************************************************************/
IDE_RC sdpstSH::setMetaPID( idvSQL        *aStatistics,
                            sdrMtx        *aMtx,
                            scSpaceID      aSpaceID,
                            scPageID       aSegPID,
                            UInt           aIndex,
                            scPageID       aMetaPID )
{
    UChar        * sPagePtr;
    sdpstSegHdr  * sSegHdr;

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&sSegHdr->mArrMetaPID[ aIndex ],
                  &aMetaPID,
                  ID_SIZEOF( aMetaPID )) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Segment Header�� Meta �������� PID�� ��ȯ�Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegPID       - [IN] Segment ��� �������� PID
 * aIndex        - [IN] Meta PID Array �� Index
 * aMetaPID      - [OUT] MetaPID
 ************************************************************************/
IDE_RC sdpstSH::getMetaPID( idvSQL        *aStatistics,
                            scSpaceID      aSpaceID,
                            scPageID       aSegPID,
                            UInt           aIndex,
                            scPageID      *aMetaPID )
{
    UChar            * sPagePtr;
    sdpstSegHdr      * sSegHdr;
    scPageID           sMetaPID;


    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* sdrMtx */
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr  = sdpstSH::getHdrPtr( sPagePtr );
    sMetaPID = sSegHdr->mArrMetaPID[ aIndex ];

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    *aMetaPID = sMetaPID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aMetaPID = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstSH::dumpHdr( UChar    * aPagePtr,
                         SChar    * aOutBuf,
                         UInt       aOutSize )
{
    UChar         * sPagePtr;
    sdpstSegHdr   * sSegHdr;
    UInt            sLoop;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    /* Segment Header dump */
    sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );
    sSegHdr  = getHdrPtr( sPagePtr );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "--------------- Segment Header Begin ----------------\n"
                     "Segment Type                         : %"ID_UINT32_FMT"\n"
                     "Segment State                        : %"ID_UINT32_FMT"\n"
                     "Segment Page ID                      : %"ID_UINT32_FMT"\n"
                     "Lst LfBMP Page ID                    : %"ID_UINT32_FMT"\n"
                     "Lst ItBMP Page ID                    : %"ID_UINT32_FMT"\n"
                     "Lst RtBMP Page ID                    : %"ID_UINT32_FMT"\n"
                     "Lst SeqNo                            : %"ID_UINT32_FMT"\n"
                     "Total Page Count                     : %"ID_UINT64_FMT"\n"
                     "Total RtBMP Count                    : %"ID_UINT64_FMT"\n"
                     "Free Index Page Count                : %"ID_UINT64_FMT"\n"
                     "Total Extent Count                   : %"ID_UINT64_FMT"\n"
                     "Page Count In Extent                 : %"ID_UINT32_FMT"\n"
                     "Lst Extent RID                       : %"ID_UINT64_FMT"\n"
                     "HWM.mWMPID                           : %"ID_UINT32_FMT"\n"
                     "HWM.mExtDirPID                       : %"ID_UINT32_FMT"\n"
                     "HWM.mSlotNoInExtDir                  : %"ID_INT32_FMT"\n"
                     "HWM.mStack.mDepth                    : %"ID_INT32_FMT"\n"
                     "HWM.mStack.mPosition[VTBMP].mNodePID : %"ID_UINT32_FMT"\n"
                     "HWM.mStack.mPosition[VTBMP].mIndex   : %"ID_INT32_FMT"\n"
                     "HWM.mStack.mPosition[RTBMP].mNodePID : %"ID_UINT32_FMT"\n"
                     "HWM.mStack.mPosition[RTBMP].mIndex   : %"ID_INT32_FMT"\n"
                     "HWM.mStack.mPosition[ITBMP].mNodePID : %"ID_UINT32_FMT"\n"
                     "HWM.mStack.mPosition[ITBMP].mIndex   : %"ID_INT32_FMT"\n"
                     "HWM.mStack.mPosition[LFBMP].mNodePID : %"ID_UINT32_FMT"\n"
                     "HWM.mStack.mPosition[LFBMP].mIndex   : %"ID_INT32_FMT"\n"
                     "ExtDir PIDList Base.mNodeCnt         : %"ID_UINT32_FMT"\n"
                     "ExtDir PIDList Base.mBase.mNext      : %"ID_UINT32_FMT"\n"
                     "ExtDir PIDList Base.mBase.mPrev      : %"ID_UINT32_FMT"\n",
                     sSegHdr->mSegType,
                     sSegHdr->mSegState,
                     sSegHdr->mSegHdrPID,
                     sSegHdr->mLstLfBMP,
                     sSegHdr->mLstItBMP,
                     sSegHdr->mLstRtBMP,
                     sSegHdr->mLstSeqNo,
                     sSegHdr->mTotPageCnt,
                     sSegHdr->mTotRtBMPCnt,
                     sSegHdr->mFreeIndexPageCnt,
                     sSegHdr->mTotExtCnt,
                     sSegHdr->mPageCntInExt,
                     sSegHdr->mLstExtRID,
                     sSegHdr->mHWM.mWMPID,
                     sSegHdr->mHWM.mExtDirPID,
                     sSegHdr->mHWM.mSlotNoInExtDir,
                     sSegHdr->mHWM.mStack.mDepth,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_VIRTBMP].mNodePID,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_VIRTBMP].mIndex,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_RTBMP].mNodePID,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_RTBMP].mIndex,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_ITBMP].mNodePID,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_ITBMP].mIndex,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_LFBMP].mNodePID,
                     sSegHdr->mHWM.mStack.mPosition[SDPST_LFBMP].mIndex,
                     sSegHdr->mExtDirBase.mNodeCnt,
                     sSegHdr->mExtDirBase.mBase.mNext,
                     sSegHdr->mExtDirBase.mBase.mPrev );

    /* Meta Page ID String�� �����. */
    for ( sLoop = 0; sLoop < SDP_MAX_SEG_PID_CNT; sLoop++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "MetaPID Array[%02"ID_UINT32_FMT"]"
                             "                     : %"ID_UINT32_FMT"\n",
                             sLoop,
                             sSegHdr->mArrMetaPID[sLoop] );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "---------------- Segment Header End -----------------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstSH::dumpBody( UChar    * aPagePtr,
                          SChar    * aOutBuf,
                          UInt       aOutSize )
{
    UChar         * sPagePtr;
    SChar         * sDumpBuf;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    if( iduMemMgr::calloc(
            IDU_MEM_SM_SDP, 1,
            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
            (void**)&sDumpBuf ) == IDE_SUCCESS )
    {
        /* Segment Header dump */
        sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );

        /* RtBMP In Segment Header dump */
        sdpstBMP::dumpHdr( sPagePtr, sDumpBuf, IDE_DUMP_DEST_LIMIT );
        idlOS::snprintf( aOutBuf,
                         aOutSize,
                         "%s",
                         sDumpBuf );

        sdpstBMP::dumpBody( sPagePtr, sDumpBuf, IDE_DUMP_DEST_LIMIT );
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%s",
                             sDumpBuf );

        /* ExtDir In Segment Header dump */
        sdpstExtDir::dumpHdr( sPagePtr, sDumpBuf, IDE_DUMP_DEST_LIMIT );
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%s",
                             sDumpBuf );

        sdpstExtDir::dumpBody( sPagePtr, sDumpBuf, IDE_DUMP_DEST_LIMIT );
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%s",
                             sDumpBuf );

        (void)iduMemMgr::free( sDumpBuf );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Segment Header�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstSH::dump( UChar    * aPagePtr )
{
    UChar         * sPagePtr;
    SChar         * sDumpBuf;

    IDE_ERROR( aPagePtr != NULL );

    /* Segment Header dump */
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
        /* Header Dump */
        if( dumpHdr( sPagePtr,
                     sDumpBuf,
                     IDE_DUMP_DEST_LIMIT ) == IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "%s", sDumpBuf );
        }

        /* Body(RtBMP, ExtDir) Dump */
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
