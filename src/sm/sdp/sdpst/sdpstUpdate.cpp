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
 * $Id: sdpstUpdate.cpp 27229 2008-07-23 17:37:19Z newdaily $
 **********************************************************************/

# include <sdb.h>

# include <sdpstSH.h>
# include <sdpstAllocPage.h>
# include <sdpstStackMgr.h>
# include <sdpstUpdate.h>

# include <sdpstExtDir.h>
# include <sdpstRtBMP.h>
# include <sdpstItBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstDPath.h>

# include <sdpSegDescMgr.h>

/***********************************************************************
 * Description : Direct-Path ��������߿� Intenal Bitmap ��������
 *               Bitmap�� �����ϴ� ������ Undo �Լ��̴�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] SpaceID
 * aBMP         - [IN] BMP �������� PID
 * aFmPBSNo     - [IN] From Slot Idx
 * aToPBSNo     - [IN] To Slot Idx
 * aFmMFNL      - [IN] From MFNL
 * aToMFNL      - [IN] To   MFNL
 ***********************************************************************/
IDE_RC sdpstUpdate::undo_SDPST_UPDATE_BMP_4DPATH( idvSQL    * aStatistics,
                                                  sdrMtx    * aMtx,
                                                  scSpaceID   aSpaceID,
                                                  scPageID    aBMP,
                                                  SShort      aFmSlotNo,
                                                  SShort      aToSlotNo,
                                                  sdpstMFNL   aPrvMFNL,
                                                  sdpstMFNL   aPrvLstSlotMFNL )
{
    UChar        * sPagePtr;
    sdpstBMPHdr  * sBMPHdr;
    UInt           sPageCnt;
    sdpstMFNL      sNewMFNL;
    idBool         sNeedToChangeMFNL;

    IDE_ERROR( aBMP      != SD_NULL_PID );
    IDE_ERROR( aFmSlotNo <= aToSlotNo );
    IDE_ERROR( aFmSlotNo != SDPST_INVALID_PBSNO );
    IDE_ERROR( aToSlotNo != SDPST_INVALID_PBSNO );

    sPageCnt = aToSlotNo - aFmSlotNo + 1;

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aBMP,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sBMPHdr = sdpstBMP::getHdrPtr( sPagePtr );

    if ( sPageCnt - 1 > 0 )
    {
        if ( aToSlotNo <= aFmSlotNo )
        {
            ideLog::log( IDE_SERVER_0,
                         "Space ID: %u, "
                         "Page ID: %u, "
                         "From SlotNo: %d, "
                         "To SlotNo: %d, "
                         "Prev MFNL: %d, "
                         "Prev Lst MFNL: %d\n",
                         aSpaceID,
                         aBMP,
                         aFmSlotNo,
                         aToSlotNo,
                         aPrvMFNL,
                         aPrvLstSlotMFNL );

            sdpstBMP::dump( sPagePtr );

            IDE_ERROR( 0 );
        }

        IDE_TEST( sdpstBMP::logAndUpdateMFNL( aMtx,
                                              sBMPHdr,
                                              aFmSlotNo,
                                              aToSlotNo - 1,
                                              SDPST_MFNL_FUL,
                                              aPrvMFNL,
                                              sPageCnt - 1,
                                              &sNewMFNL,
                                              &sNeedToChangeMFNL )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdpstBMP::logAndUpdateMFNL( aMtx,
                                          sBMPHdr,
                                          aToSlotNo,
                                          aToSlotNo,
                                          aPrvLstSlotMFNL,
                                          aPrvMFNL,
                                          1,
                                          &sNewMFNL,
                                          &sNeedToChangeMFNL ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Direct-Path ��������߿� Leaf Bitmap �������� Bitmap��
 *               �����ϴ� ������ Undo �Լ��̴�.
 *
 * aStatistics  - [IN] �������
 * aSpaceID     - [IN] SpaceID
 * aLfBMP       - [IN] Leaf BMP �������� PID
 * aFmPBSNo     - [IN] From Page Idx
 * aToPBSNo     - [IN] To Page Idx
 * aPrvMFNL     - [IN] ���� MFNL
 * aMtx         - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::undo_SDPST_UPDATE_MFNL_4DPATH( idvSQL    * aStatistics,
                                                   sdrMtx    * aMtx,
                                                   scSpaceID   aSpaceID,
                                                   scPageID    aLfBMP,
                                                   SShort      aFmPBSNo,
                                                   SShort      aToPBSNo,
                                                   sdpstMFNL   aPrvMFNL )
{
    UShort               sPageCnt;
    sdpstMFNL            sPrvMFNL;
    UChar              * sPagePtr;
    sdpstLfBMPHdr      * sLfBMPHdr;
    idBool               sNeedToChangeMFNL;
    UShort               sMetaPageCnt;

    IDE_ERROR( aLfBMP   != SD_NULL_PID );
    IDE_ERROR( aFmPBSNo <= aToPBSNo );
    IDE_ERROR( aFmPBSNo != SDPST_INVALID_PBSNO );
    IDE_ERROR( aToPBSNo != SDPST_INVALID_PBSNO );

    sPrvMFNL = aPrvMFNL;

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aLfBMP,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sLfBMPHdr = sdpstLfBMP::getHdrPtr( sPagePtr );
    sPageCnt  = aToPBSNo - aFmPBSNo + 1;

    // From ���� To���� ������ bitset�� �����Ѵ�.
    sdpstLfBMP::updatePBS( sdpstLfBMP::getMapPtr( sLfBMPHdr ),
                           aFmPBSNo,
                           (sdpstPBS)(SDPST_BITSET_PAGETP_DATA |
                                      SDPST_BITSET_PAGEFN_UNF),
                           sPageCnt,
                           &sMetaPageCnt);

    IDE_TEST( sdpstBMP::logAndUpdateMFNL( aMtx,
                                          sdpstLfBMP::getBMPHdrPtr(sLfBMPHdr),
                                          SDPST_INVALID_SLOTNO,
                                          SDPST_INVALID_SLOTNO,
                                          SDPST_MFNL_FUL,
                                          SDPST_MFNL_FMT,
                                          sPageCnt - sMetaPageCnt,
                                          &sPrvMFNL,
                                          &sNeedToChangeMFNL ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : SegDesc�� AllocExtRID, HWM�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] SpaceID
 * aSegRID        - [IN] Seg�� RID
 * aNewFmtPageCnt - [IN] DPath�� �Ҵ�� ������ ����
 * aHWMExtRID    - [IN] ���� HWM�� ExtRID
 * aHWMPID       - [IN] ���� HWM�� PID
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::undo_SDPST_UPDATE_WM_4DPATH( idvSQL    * aStatistics,
                                                 sdrMtx    * aMtx,
                                                 scSpaceID   aSpaceID,
                                                 scPageID    aSegPID,
                                                 sdRID       aHWMExtRID,
                                                 scPageID    aHWMPID )
{
    UChar            * sToSegPagePtr;
    sdpstSegHdr      * sSegHdr;
    sdpstStack         sStack;
    scPageID           sLstEXTDIRPID;
    SShort             sLstSlotNo;

    IDE_ERROR( aSegPID != SD_NULL_PID );
    IDE_ERROR( aHWMExtRID != SD_NULL_RID );
    IDE_ERROR( aMtx != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sToSegPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
          != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr(sToSegPagePtr);

    sdpstDPath::calcExtRID2ExtInfo( aSegPID, sToSegPagePtr, aHWMExtRID,
                                    &sLstEXTDIRPID, &sLstSlotNo );
    sdpstStackMgr::initialize( &sStack );
    IDE_TEST( sdpstAllocPage::makeOrderedStackFromDataPage( aStatistics,
                                                    aSpaceID,
                                                    aSegPID,
                                                    aHWMPID,
                                                    &sStack ) != IDE_SUCCESS );
    // HWM�� ���� HWM �� �����Ѵ�.
    IDE_TEST( sdpstSH::logAndUpdateWM( aMtx,
                                        &(sSegHdr->mHWM),
                                         aHWMPID,
                                         sLstEXTDIRPID,
                                         sLstSlotNo,
                                         &sStack ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment Header �ʱ�ȭ�� REDO�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_INIT_SEGHDR( SChar       * aData,
                                            UInt          aLength,
                                            UChar       * aPagePtr,
                                            sdrRedoInfo * /*aRedoInfo*/,
                                            sdrMtx      * aMtx )
{
    sdpstRedoInitSegHdr sLogData;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstSH::initSegHdr( (sdpstSegHdr*)aPagePtr,
                         sLogData.mSegPID,
                         sLogData.mSegType,
                         sLogData.mPageCntInExt,
                         sLogData.mMaxExtDescCntInExtDir,
                         sLogData.mMaxSlotCntInRtBMP );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Leaf Bitmap ������ �ʱ�ȭ�� REDO�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_INIT_LFBMP( SChar       * aData,
                                           UInt          aLength,
                                           UChar       * aPagePtr,
                                           sdrRedoInfo * /*aRedoInfo*/,
                                           sdrMtx      * aMtx )
{
    sdpstRedoInitLfBMP  sLogData;
    idBool              sNeedToChangeMFNL;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    /* page range slot�ʱ�ȭ�� ���ش�. */
    sdpstLfBMP::initBMPHdr( (sdpstLfBMPHdr*)aPagePtr,
                            sLogData.mPageRange,
                            sLogData.mParentInfo.mParentPID,
                            sLogData.mParentInfo.mIdxInParent,
                            sLogData.mNewPageCnt,
                            sLogData.mMetaPageCnt,
                            sLogData.mExtDirPID,
                            sLogData.mSlotNoInExtDir,
                            sLogData.mRangeFstPID,
                            sLogData.mMaxSlotCnt,
                            &sNeedToChangeMFNL );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Internal Bitmap ������ �ʱ�ȭ�� REDO�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_INIT_BMP( SChar       * aData,
                                         UInt          aLength,
                                         UChar       * aPagePtr,
                                         sdrRedoInfo * /*aRedoInfo*/,
                                         sdrMtx      * aMtx )
{
    sdpstRedoInitBMP    sLogData;
    idBool              sNeedToChangeMFNL;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstBMP::initBMPHdr( (sdpstBMPHdr*)aPagePtr,
                          sLogData.mType,
                          sLogData.mParentInfo.mParentPID,
                          sLogData.mParentInfo.mIdxInParent,
                          sLogData.mFstChildBMP,
                          sLogData.mLstChildBMP,
                          sLogData.mFullBMPCnt,
                          sLogData.mMaxSlotCnt,
                          &sNeedToChangeMFNL );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir ������ �ʱ�ȭ�� REDO�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_INIT_EXTDIR( SChar       * aData,
                                            UInt          aLength,
                                            UChar       * aPagePtr,
                                            sdrRedoInfo * /*aRedoInfo*/,
                                            sdrMtx      * aMtx )
{
    sdpstRedoInitExtDir   sLogData;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstExtDir::initExtDirHdr( (sdpstExtDirHdr*)aPagePtr,
                                sLogData.mMaxExtCnt,
                                sLogData.mBodyOffset );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RangeSlot�� Leaf BMP �������� �߰��Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_ADD_RANGESLOT( SChar       * aData,
                                              UInt          aLength,
                                              UChar       * aPagePtr,
                                              sdrRedoInfo * /*aRedoInfo*/,
                                              sdrMtx      * aMtx )
{
    sdpstRedoAddRangeSlot     sLogData;
    idBool                    sNeedToChangeMFNL;
    sdpstMFNL                 sNewMFNL;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstLfBMP::addPageRangeSlot( (sdpstLfBMPHdr*)aPagePtr,
                                  sLogData.mFstPID,
                                  sLogData.mLength,
                                  sLogData.mMetaPageCnt,
                                  sLogData.mExtDirPID,
                                  sLogData.mSlotNoInExtDir,
                                  &sNeedToChangeMFNL,
                                  &sNewMFNL );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : rt/itslot�� rt/it BMP �������� �߰��Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_ADD_SLOTS( SChar       * aData,
                                          UInt          aLength,
                                          UChar       * aPagePtr,
                                          sdrRedoInfo * /*aRedoInfo*/,
                                          sdrMtx      * /*aMtx*/ )
{
    sdpstRedoAddSlots     sLogData;
    idBool                sNeedToChangeMFNL;
    sdpstMFNL             sNewMFNL;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstBMP::addSlots( (sdpstBMPHdr*)aPagePtr,
                        sLogData.mLstSlotNo,
                        sLogData.mFstChildBMP,
                        sLogData.mLstChildBMP,
                        sLogData.mFullBMPCnt,
                        &sNeedToChangeMFNL,
                        &sNewMFNL );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : EXTDESC�� ExtDir �������� �߰��Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_ADD_EXTDESC( SChar       * aData,
                                            UInt          aLength,
                                            UChar       * aPagePtr,
                                            sdrRedoInfo * /*aRedoInfo*/,
                                            sdrMtx      * aMtx )
{
    sdpstRedoAddExtDesc   sLogData;
    sdpstExtDesc          sExtDesc;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sExtDesc.mExtFstPID     = sLogData.mExtFstPID;
    sExtDesc.mLength        = sLogData.mLength;
    sExtDesc.mExtMgmtLfBMP  = sLogData.mExtMgmtLfBMP;
    sExtDesc.mExtFstDataPID = sLogData.mExtFstDataPID;

    sdpstExtDir::addExtDesc( (sdpstExtDirHdr*)aPagePtr,
                             sLogData.mLstSlotNo,
                             &sExtDesc );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : EXTDESC�� ExtDir �������� �߰��Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_ADD_EXT_TO_SEGHDR( SChar       * aData,
                                                  UInt          aLength,
                                                  UChar       * aPagePtr,
                                                  sdrRedoInfo * /*aRedoInfo*/,
                                                  sdrMtx      * aMtx )
{
    sdpstRedoAddExtToSegHdr   sLogData;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstSH::linkBMPsToSegHdr( (sdpstSegHdr*)aPagePtr,
                               sLogData.mAllocPageCnt,
                               sLogData.mMetaPageCnt,
                               sLogData.mNewLstLfBMP,
                               sLogData.mNewLstItBMP,
                               sLogData.mNewLstRtBMP,
                               sLogData.mNewRtBMPCnt );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
/***********************************************************************
 * Description : HWM�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_UPDATE_WM( SChar       * aData,
                                          UInt          aLength,
                                          UChar       * aPagePtr,
                                          sdrRedoInfo * /*aRedoInfo*/,
                                          sdrMtx      * aMtx )
{
    sdpstRedoUpdateWM     sLogData;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstSH::updateWM( (sdpstWM*)aPagePtr,
                       sLogData.mWMPID,
                       sLogData.mExtDirPID,
                       sLogData.mSlotNoInExtDir,
                       &sLogData.mStack );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : BMP Slot�� MFNL�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_UPDATE_MFNL( SChar       * aData,
                                            UInt          aLength,
                                            UChar       * aPagePtr,
                                            sdrRedoInfo * /*aRedoInfo*/,
                                            sdrMtx      * aMtx )
{
    sdpstRedoUpdateMFNL   sLogData;
    sdpstMFNL             sNewBMPMFNL;
    idBool                sNeedToChangeMFNL;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstBMP::updateMFNL( (sdpstBMPHdr*)aPagePtr,
                          sLogData.mFmSlotNo,
                          sLogData.mToSlotNo,
                          sLogData.mFmMFNL,
                          sLogData.mToMFNL,
                          sLogData.mPageCnt,
                          &sNewBMPMFNL,
                          &sNeedToChangeMFNL );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : LfBMP�������� PBS�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_UPDATE_PBS( SChar       * aData,
                                           UInt          aLength,
                                           UChar       * aPagePtr,
                                           sdrRedoInfo * /*aRedoInfo*/,
                                           sdrMtx      * aMtx )
{
    sdpstRedoUpdatePBS    sLogData;
    UShort                sMetaPageCount;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstLfBMP::updatePBS( (sdpstRangeMap*)aPagePtr,
                                      sLogData.mFstPBSNo,
                                      sLogData.mPBS,
                                      sLogData.mPageCnt,
                                      &sMetaPageCount );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct-Path Insert Merge �������� Leaf Bitmap ��������
 *               PageBitSet�� FULL�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aData          - [IN] Log Pointer
 * aLength        - [IN] Log ����
 * aPagePtr       - [IN] ������ Pointer
 * aRedoInfo      - [IN] Redo ����
 * aMtx           - [IN] Mini Transaction Pointer
 ***********************************************************************/
IDE_RC sdpstUpdate::redo_SDPST_UPDATE_LFBMP_4DPATH( SChar       * aData,
                                                    UInt          aLength,
                                                    UChar       * aPagePtr,
                                                    sdrRedoInfo * /*aRedoInfo*/,
                                                    sdrMtx      * aMtx )
{
    sdpstRedoUpdateLfBMP4DPath    sLogData;
    UShort   sMetaPageCnt;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx     != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(sLogData) );

    idlOS::memcpy( &sLogData, aData, ID_SIZEOF(sLogData) );

    sdpstLfBMP::updatePBS( (sdpstRangeMap*)aPagePtr,
                                      sLogData.mFmPBSNo,
                                      (sdpstPBS)(SDPST_BITSET_PAGETP_DATA |
                                                 SDPST_BITSET_PAGEFN_FUL),
                                      sLogData.mPageCnt,
                                      &sMetaPageCnt );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC sdpstUpdate::undo_SDPST_ALLOC_PAGE( idvSQL    * aStatistics,
                                           sdrMtx    * aMtx,
                                           scSpaceID   aSpaceID,
                                           scPageID    aSegPID,
                                           scPageID    aPID )
{
    sdpPageType       sPageType;
    const smiColumn * sLobColumn;
    UChar           * sPagePtr;
    void            * sTableHeader;
    smOID             sTableOID;
    
    IDE_ERROR( aMtx      != NULL );
    IDE_ERROR( aSpaceID  != 0 );
    IDE_ERROR( aPID   != SD_NULL_RID );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL /*aTrySuccess*/,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );
    sTableOID = ((sdpPhyPageHdr*)sPagePtr)->mTableOID;

    IDE_ERROR( smLayerCallback::getTableHeaderFromOID( sTableOID,
                                                       &sTableHeader )
               == IDE_SUCCESS );

    switch(sPageType)
    {
        case SDP_PAGE_LOB_INDEX:
        case SDP_PAGE_LOB_DATA:
            sLobColumn = smLayerCallback::getLobColumn( sTableHeader,
                                                        aSpaceID,
                                                        aSegPID );
   
            IDE_TEST( sLobColumn == NULL ); 

            IDE_TEST( sdpSegDescMgr::getSegMgmtOp((smiColumn*)sLobColumn)->mFreePage(
                          aStatistics,
                          aMtx,
                          aSpaceID,
                          sdpSegDescMgr::getSegHandle((smiColumn*)sLobColumn),
                          sPagePtr )
                      != IDE_SUCCESS );

            break;
            
        default:
            /* �ٸ� �������� Ÿ���� ��� undo ���� �ʴ´� */
            IDE_ASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
