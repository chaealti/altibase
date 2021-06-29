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
 * $Id: sdpstLfBMP.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Leaf Bitmap �������� ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPST_LF_BMP_H_
# define _O_SDPST_LF_BMP_H_ 1

# include <sdpstDef.h>

# include <sdpPhyPage.h>
# include <sdpstBMP.h>

class sdpstLfBMP
{

public:

    static IDE_RC createAndInitPages( idvSQL               * aStatistics,
                                      sdrMtxStartInfo      * aStartInfo,
                                      scSpaceID              aSpaceID,
                                      sdpstExtDesc         * aExtDesc,
                                      sdpstBfrAllocExtInfo * aBfrAllocExtInfo,
                                      sdpstAftAllocExtInfo * aAftAllocExtInfo );

    static IDE_RC logAndInitBMPHdr( sdrMtx          * aMtx,
                                    sdpstLfBMPHdr   * aBMPHdr,
                                    sdpstPageRange    aPageRange,
                                    scPageID          aParentItBMP,
                                    UShort            aSlotNoInParent,
                                    UShort            aNewPageCount,
                                    UShort            aMetaPageCnt,
                                    scPageID          aExtDirPID,
                                    SShort            aSlotNoInExtDir,
                                    scPageID          aRangeFstPID,
                                    UShort            aMaxSlotCnt,
                                    idBool          * aNeedToChangeMFNL );

    static void  initBMPHdr( sdpstLfBMPHdr   * aBMPHdr,
                             sdpstPageRange    aPageRange,
                             scPageID          aParentItBMP,
                             UShort            aSlotNoInParent,
                             UShort            aNewPageCount,
                             UShort            aMetaPageCnt,
                             scPageID          aExtDirPID,
                             SShort            aSlotNoInExtDir,
                             scPageID          aRangeFstPID,
                             UShort            aMaxSlotCnt,
                             idBool          * aIsChangeBitSet );

    static inline UShort  getFreeSlotCnt( sdpstLfBMPHdr * aBMPHdr );
    static inline UShort  getFreePBSCnt( sdpstLfBMPHdr * aBMPHdr );

    static inline sdpstLfBMPHdr * getHdrPtr( UChar   * aPagePtr );
    static inline sdpstLfBMPHdr * getHdrPtr( sdpstBMPHdr * aBMPHdr );

    static inline sdpstBMPHdr * getBMPHdrPtr( UChar   * aPagePtr );
    static inline sdpstBMPHdr * getBMPHdrPtr( sdpstLfBMPHdr * aLfBMPHdr );

    static inline idBool isGtFN( sdpstPBS aSrcBitSet,
                                 sdpstPBS aDestBitSet );

    static inline idBool isEqFN( sdpstPBS aSrcBitSet,
                                 sdpstPBS aDestBitSet );

    static IDE_RC tryToChangeMFNL( idvSQL          * aStatistics,
                                   sdrMtx          * aMtx,
                                   scSpaceID         aSpaceID,
                                   scPageID          aLeafBMP,
                                   SShort            aPBSNo,
                                   sdpstPBS          aNewPBS,
                                   idBool          * aNeedToChangeMFNL,
                                   sdpstMFNL       * aNewMFNL,
                                   scPageID        * aParentItBMP,
                                   SShort          * aSlotNoInParent );

    static IDE_RC tryToCreatePageInRangeSlot(
                                 idvSQL              * aStatistics,
                                 sdrMtx              * aMtx,
                                 sdrMtx              * aNtaMtx,
                                 scSpaceID             aSpaceID,
                                 sdpSegHandle        * aSegHandle,
                                 scPageID              aLeafBMP,
                                 sdpstCandidatePage  * aDataPage,
                                 sdpPageType           aPageType,
                                 sdpstRangeSlot     ** aRangeSlot,
                                 UChar              ** aNewPagePtr,
                                 scPageID            * aWMPID,
                                 UShort              * aNewPageCnt,
                                 UShort              * aMetaPageCnt,
                                 idBool              * aNeedToChangeMFNL,
                                 sdpstMFNL           * aNewMFNL,
                                 scPageID            * aParentItBMP,
                                 SShort              * aSlotNoInParent,
                                 SShort              * aLstFmtPBSNo );

    static IDE_RC logAndAddPageRangeSlot( sdrMtx         * aMtx,
                                          sdpstLfBMPHdr  * aBMPHdr,
                                          scPageID         aFstPID,
                                          UShort           aLength,
                                          UShort           aMetaPageCnt,
                                          scPageID         aExtMapPID,
                                          SShort           aSlotNoInExtDir,
                                          idBool         * aNeedToChangeMFNL,
                                          sdpstMFNL      * aNewMFNL );

    static inline idBool isLstPBSNo( UChar * aPagePtr, SShort aIndex );

    static inline sdpstRangeMap * getMapPtr( sdpstLfBMPHdr * aBMPHdr );

    static IDE_RC logAndUpdatePBS( sdrMtx        * aMtx,
                                   sdpstRangeMap * aMapPtr,
                                   SShort          aFstPBSNo,
                                   sdpstPBS        aPBS,
                                   UShort          aPageCnt,
                                   UShort        * aMetaPageCnt );

    static void updatePBS( sdpstRangeMap * aMapPtr,
                           SShort          aFstPBSNo,
                           sdpstPBS        aPBS,
                           UShort          aPageCnt,
                           UShort        * aMetaPageCnt );


    static inline SShort findSlotNoByExtPID( sdpstLfBMPHdr  * aBMPHdr,
                                             scPageID         aExtFstPID );

    static inline SShort findPBSNoByPID( sdpstRangeMap  * aMapPtr,
                                        SShort           aSlotCnt,
                                        scPageID         aPageID );

    static inline SShort findPBSNoByPID( sdpstLfBMPHdr  * aBMPHdr,
                                        scPageID         aPageID );

    static IDE_RC getPBSNoByExtFstPID( idvSQL     * aStatistics,
                                      scSpaceID    aSpaceID,
                                      scPageID     aLfBMP,
                                      scPageID     aExtFstPID,
                                      SShort     * aPBSNo );

    static void addPageRangeSlot( sdpstLfBMPHdr  * aBMPHdr,
                                  scPageID         aFstPID,
                                  UShort           aNewPageCount,
                                  UShort           aMetaPageCount,
                                  scPageID         aExtMapPID,
                                  SShort           aSlotNoInExtDir,
                                  idBool         * aNeedToChangeMFNL,
                                  sdpstMFNL      * aNewMFNL );

    static inline sdpstMFNL convertPBSToMFNL( sdpstPBS  aPBSt );

    static inline UShort getFreePageRangeCnt( sdpstLfBMPHdr * aLfBMPHdr );

    static inline idBool isCandidatePage( UChar     * aPagePtr,
                                          SShort      aPBSNo,
                                          sdpstMFNL   aTargetMFNL );
    static inline void setCandidateArray( UChar     * aPagePtr,
                                          UShort      aPBSNo,
                                          UShort      aArrIdx,
                                          void      * aArray );

    static inline UShort getStartPBSNo( UChar     * aPagePtr,
                                        UShort      aHintIdx );

    static inline SShort getTotPageCnt( UChar   * aPagePtr,
                                        sdpstWM * aHWM );
    static inline SShort getFstFreePBSNo( UChar    * aPagePtr );

    static IDE_RC verifyBMP( sdpstLfBMPHdr  * aBMPHdr );

    static inline SShort getRangeSlotNoByPBSNo( sdpstRangeMap  * aMapPtr,
                                                SShort           aPBSNo );

    static inline scPageID getPageIDByPBSNo( sdpstRangeSlot * aRangeSlot,
                                             SShort           aPBSNo );

    static inline idBool isDataPage( sdpstPBS aPBS );

    static inline sdpstRangeSlot * getRangeSlotByPBSNo(
                                           sdpstRangeMap * aMapPtr,
                                           SShort          aSlotNo,
                                           SShort          aPBSNo );

    static inline sdpstRangeSlot * getRangeSlotByPBSNo(
                                       sdpstLfBMPHdr * aBMPHdr,
                                       SShort          aPBSNo );

    static inline sdpstRangeSlot * getRangeSlotBySlotNo(
                                       sdpstRangeMap * aMapPtr,
                                       SShort          aSlotNo );

    static IDE_RC dumpHdr( UChar * aPagePtr,
                           SChar * aOutBuf,
                           UInt    aOutSize );

    static IDE_RC dumpBody( UChar * aPagePtr,
                            SChar * aOutBuf,
                            UInt    aOutSize );

    static IDE_RC dump( UChar * aPagePtr );


private:
    static idBool isValidTotPageCnt( sdpstLfBMPHdr * aBMPHdr );


public:
    static sdpstBMPOps  mLfBMPOps;
};

/* page index�� range slot idx�� ���Ѵ�. */
inline SShort sdpstLfBMP::getRangeSlotNoByPBSNo( sdpstRangeMap  * aMapPtr,
                                                 SShort           aPBSNo )
{
    return (aPBSNo / aMapPtr->mRangeSlot[0].mLength);
}

/*
 * src bitset���� PAGETYPE bitset�� �����ϰ�, ���뵵 Bitset����
 * dest bitset�� ���뵵 ������ �˻��Ѵ�.
 */
inline idBool sdpstLfBMP::isGtFN( sdpstPBS aSrcBitSet, sdpstPBS aDestBitSet )
{
    sdpstPBS sSrcBitSet = aSrcBitSet & ~SDPST_BITSET_PAGETP_MASK;
    return ((sSrcBitSet > aDestBitSet) ? ID_FALSE : ID_TRUE);
}

/*
 * src bitset���� PAGETYPE bitset�� �����ϰ�, ���뵵 Bitset����
 * dest bitset�� ���뵵 ������ �˻��Ѵ�.
 */
inline idBool sdpstLfBMP::isEqFN( sdpstPBS aSrcBitSet, sdpstPBS aDestBitSet )
{
    sdpstPBS sSrcBitSet = aSrcBitSet & ~SDPST_BITSET_PAGETP_MASK;
    return ((sSrcBitSet == aDestBitSet) ? ID_TRUE : ID_FALSE);
}

/* �ش� �������� �ĺ����������� Ȯ���Ѵ�. */
inline idBool sdpstLfBMP::isCandidatePage( UChar     * aPagePtr,
                                           SShort      aPBSNo,
                                           sdpstMFNL   aTargetMFNL )
{
    sdpstRangeMap   * sMapPtr;
    sdpstPBS          sPBS;
    idBool            sIsAvailable;

    IDE_ASSERT( aPagePtr != NULL );
    IDE_ASSERT( aPBSNo   != SDPST_INVALID_PBSNO );

    sMapPtr = getMapPtr( getHdrPtr( aPagePtr ) );
    sPBS    = *(sMapPtr->mPBSTbl + aPBSNo);

    if ( isDataPage( sPBS ) == ID_TRUE )
    {
        switch ( aTargetMFNL )
        {
            case SDPST_MFNL_FMT:
                sIsAvailable = isGtFN( sPBS, SDPST_BITSET_CANDIDATE_ALLOCPAGE );
                break;
                /* xxxx */
            case SDPST_MFNL_INS:
                sIsAvailable = isGtFN( sPBS, SDPST_BITSET_CANDIDATE_ALLOCSLOT );
                break;
            default:
                IDE_ASSERT(0);
                break;
        }
    }
    else
    {
         sIsAvailable = ID_FALSE;
    }
    return sIsAvailable;
}

inline SShort sdpstLfBMP::getFstFreePBSNo( UChar    * aPagePtr )
{
    IDE_ASSERT( aPagePtr != NULL );
    return getHdrPtr( aPagePtr )->mFstDataPagePBSNo;
}

/* aArray�� �ĺ� �������� �߰��Ѵ�. */
inline void sdpstLfBMP::setCandidateArray( UChar     * aPagePtr,
                                           UShort      aPBSNo,
                                           UShort      aArrIdx,
                                           void      * aArray )
{
    sdpstLfBMPHdr       * sLfBMPHdr;
    sdpstRangeMap       * sRangeMap;
    sdpstCandidatePage  * sArray;
    scPageID              sPageID;
    UShort                sRangeSlotNo;

    IDE_ASSERT( aPagePtr != NULL );
    IDE_ASSERT( aArray   != NULL );

    sLfBMPHdr    = getHdrPtr( aPagePtr );
    sRangeMap    = getMapPtr( sLfBMPHdr );
    sArray       = (sdpstCandidatePage*)aArray;
    sRangeSlotNo = getRangeSlotNoByPBSNo( sRangeMap, aPBSNo );
    sPageID      = getPageIDByPBSNo( &sRangeMap->mRangeSlot[sRangeSlotNo],
                                     aPBSNo );

    sArray[aArrIdx].mPageID      = sPageID;
    sArray[aArrIdx].mPBSNo       = aPBSNo;
    sArray[aArrIdx].mPBS         = *(sRangeMap->mPBSTbl + aPBSNo);
    sArray[aArrIdx].mRangeSlotNo = sRangeSlotNo;
}

/* leaf-bmp ������������ Ž�� ���� ��ġ�� Hash �Ͽ� ��´� */
inline UShort sdpstLfBMP::getStartPBSNo( UChar     * aPagePtr,
                                         UShort      aHintIdx )
{
    sdpstLfBMPHdr   * sLfBMPHdr;
    IDE_ASSERT( aPagePtr != NULL );
    sLfBMPHdr = getHdrPtr( aPagePtr );
    return sdpstBMP::doHash( aHintIdx, sLfBMPHdr->mTotPageCnt );
}

/* leaf-bmp ������������ Ž�� ���� ��ġ�� Hash �Ͽ� ��´� */
inline SShort sdpstLfBMP::getTotPageCnt( UChar      * aPagePtr,
                                         sdpstWM    * aHWM )
{
    sdpstLfBMPHdr   * sLfBMPHdr;
    sdpstPosItem      sHWMPos;
    SShort            sTotPageCnt;

    IDE_ASSERT( aPagePtr != NULL );
    IDE_ASSERT( aHWM     != NULL );

    sLfBMPHdr = getHdrPtr( aPagePtr );
    sHWMPos   = sdpstStackMgr::getSeekPos( &aHWM->mStack, SDPST_LFBMP );

    if ( sHWMPos.mNodePID ==
            sdpPhyPage::getPageID( sdpPhyPage::getPageStartPtr(aPagePtr) ) )
    {
        if ( sHWMPos.mIndex + 1 < sLfBMPHdr->mTotPageCnt )
        {
            sTotPageCnt = sHWMPos.mIndex + 1;
        }
        else
        {
            sTotPageCnt = sLfBMPHdr->mTotPageCnt;
        }
    }
    else
    {
        sTotPageCnt = sLfBMPHdr->mTotPageCnt;
    }

    return sTotPageCnt;
}

/* page index�� page id�� ���Ѵ�. */
inline scPageID sdpstLfBMP::getPageIDByPBSNo( sdpstRangeSlot * aRangeSlot,
                                              SShort           aPBSNo )
{
    IDE_ASSERT( aRangeSlot->mFstPBSNo <= aPBSNo );
    IDE_ASSERT( (aRangeSlot->mFstPBSNo + aRangeSlot->mLength) > aPBSNo );
    return (aRangeSlot->mFstPID + aPBSNo - aRangeSlot->mFstPBSNo);
}


/* �ش� PBS�� ������ ������ ���� Ȯ���Ѵ�. */
inline idBool sdpstLfBMP::isDataPage( sdpstPBS aPBS )
{
    if ( ( aPBS & SDPST_BITSET_PAGETP_MASK ) == SDPST_BITSET_PAGETP_DATA )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/* rangeslot idx�� Page Range Slot�� ��ȯ�Ѵ�. */
inline sdpstRangeSlot * sdpstLfBMP::getRangeSlotBySlotNo(
                                           sdpstRangeMap * aMapPtr,
                                           SShort              aSlotNo )
{
    IDE_ASSERT( aMapPtr != NULL );
    IDE_ASSERT( aSlotNo != SDPST_INVALID_SLOTNO );
    return (sdpstRangeSlot*) &(aMapPtr->mRangeSlot[ aSlotNo ]);
}

/* PBSNo�� Page Range Slot�� ��ȯ�Ѵ�. */
inline sdpstRangeSlot * sdpstLfBMP::getRangeSlotByPBSNo(
                                           sdpstRangeMap * aMapPtr,
                                           SShort          aSlotCnt,
                                           SShort          aPBSNo )
{
    SInt                  sLoop;
    SShort                sSlotNo;

    IDE_ASSERT( aMapPtr != NULL );
    IDE_ASSERT( aSlotCnt > 0 );
    IDE_ASSERT( aPBSNo  != SDPST_INVALID_PBSNO );

    sSlotNo = SDPST_INVALID_SLOTNO;
    for( sLoop = 0; sLoop < aSlotCnt; sLoop++ )
    {
        if ( (aMapPtr->mRangeSlot[sLoop].mFstPBSNo +
              aMapPtr->mRangeSlot[sLoop].mLength - 1) >= aPBSNo )
        {
            sSlotNo = (SShort)sLoop;
            break; // found it !!
        }
    }
    return getRangeSlotBySlotNo( aMapPtr, sSlotNo );
}

/* PBSNo�� Page Range Slot�� ��ȯ�Ѵ�. */
inline sdpstRangeSlot * sdpstLfBMP::getRangeSlotByPBSNo(
                                           sdpstLfBMPHdr * aLfBMPHdr,
                                           SShort          aPBSNo )
{
    sdpstRangeMap   * sMapPtr;

    IDE_ASSERT( aLfBMPHdr != NULL );
    IDE_ASSERT( aPBSNo  != SDPST_INVALID_PBSNO );

    sMapPtr = getMapPtr( aLfBMPHdr );
    return getRangeSlotByPBSNo( sMapPtr, aLfBMPHdr->mBMPHdr.mSlotCnt, aPBSNo );
}

/* �������� PID�� Leaf Bitmap ������������ Page ������ ��ȯ�Ѵ�. */
inline SShort sdpstLfBMP::findPBSNoByPID( sdpstRangeMap      * aMapPtr,
                                         SShort               aSlotCnt,
                                         scPageID             aPageID )
{
    SShort                sCurIdx;
    SShort                sPBSNo;
    scPageID              sFstPID;
    scPageID              sLstPID;
    sdpstRangeSlot      * sRangeSlotPtr;

    IDE_ASSERT( aMapPtr != NULL );
    IDE_ASSERT( aSlotCnt > 0 );
    IDE_ASSERT( aPageID != SD_NULL_PID );

    sPBSNo        = SDPST_INVALID_SLOTNO;
    sRangeSlotPtr = aMapPtr->mRangeSlot;

    for( sCurIdx = 0; sCurIdx < aSlotCnt; sCurIdx++ )
    {
        sFstPID = (sRangeSlotPtr + sCurIdx)->mFstPID;
        sLstPID = sFstPID + (sRangeSlotPtr + sCurIdx)->mLength - 1;

        if ( (sFstPID <= aPageID) && (sLstPID >= aPageID ) )
        {
            sPBSNo = (sRangeSlotPtr + sCurIdx)->mFstPBSNo +
                (aPageID - (sRangeSlotPtr + sCurIdx)->mFstPID );
            break; // found it !!
        }
    }
    return sPBSNo;
}

/* �������� PID�� Leaf Bitmap ������������ Page ������ ��ȯ�Ѵ�. */
inline SShort sdpstLfBMP::findPBSNoByPID( sdpstLfBMPHdr  * aLfBMPHdr,
                                         scPageID         aPageID )
{
    IDE_ASSERT( aLfBMPHdr != NULL );
    IDE_ASSERT( aPageID   != SD_NULL_PID );

    return findPBSNoByPID( getMapPtr(aLfBMPHdr),
                          aLfBMPHdr->mBMPHdr.mSlotCnt,
                          aPageID );
}



/* Leaf Bitmap �������� map ptr�� ��ȯ�Ѵ�. */
inline sdpstRangeMap * sdpstLfBMP::getMapPtr( sdpstLfBMPHdr * aLfBMPHdr )
{
    IDE_ASSERT( aLfBMPHdr->mBMPHdr.mBodyOffset != 0 );
    return (sdpstRangeMap*)
        (sdpPhyPage::getPageStartPtr((UChar*)aLfBMPHdr) +
         aLfBMPHdr->mBMPHdr.mBodyOffset);
}

/* ������ PBSNo ���� Ȯ���Ѵ�. */
inline idBool sdpstLfBMP::isLstPBSNo( UChar * aPagePtr, SShort aIndex )
{
    return (aIndex == getHdrPtr(aPagePtr)->mTotPageCnt - 1) ?
           ID_TRUE : ID_FALSE;
}

/*
 * Leaf Bitmap Page���� Leaf Control Header�� Ptr ��ȯ
 */
inline sdpstLfBMPHdr * sdpstLfBMP::getHdrPtr( UChar   * aPagePtr )
{
    return (sdpstLfBMPHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPagePtr );
}


inline sdpstLfBMPHdr * sdpstLfBMP::getHdrPtr( sdpstBMPHdr * aBMPHdr )
{
    IDE_ASSERT( aBMPHdr != NULL );
    IDE_ASSERT( aBMPHdr->mType == SDPST_LFBMP );
    return getHdrPtr( (UChar*)aBMPHdr );
}

/*
 * Leaf Bitmap Page���� BMP Header�� Ptr ��ȯ
 */
inline sdpstBMPHdr * sdpstLfBMP::getBMPHdrPtr( UChar   * aPagePtr )
{
    return &(getHdrPtr( aPagePtr )->mBMPHdr);
}

/*
 * Leaf Bitmap Page���� BMP Header�� Ptr ��ȯ
 */
inline sdpstBMPHdr * sdpstLfBMP::getBMPHdrPtr( sdpstLfBMPHdr * aLfBMPHdr )
{
    return &(aLfBMPHdr->mBMPHdr);
}

/*
 * Leaf BMP ������������ ������ Range Slot ���� ��ȯ
 */
inline UShort sdpstLfBMP::getFreeSlotCnt( sdpstLfBMPHdr * aLfBMPHdr )
{
    return (UShort)(SDPST_MAX_SLOT_CNT_PER_LFBMP() - aLfBMPHdr->mBMPHdr.mSlotCnt);
}

/*
 * Leaf BMP ������������ ������ PBS ���� ��ȯ
 */
inline UShort sdpstLfBMP::getFreePBSCnt( sdpstLfBMPHdr * aBMPHdr )
{
    IDE_ASSERT( aBMPHdr->mPageRange >= aBMPHdr->mBMPHdr.mMaxSlotCnt);
    return (UShort)(aBMPHdr->mBMPHdr.mMaxSlotCnt - aBMPHdr->mBMPHdr.mSlotCnt);
}

/* Extent ù��° �������� PID�� Extent Slot ���� Ž�� */
inline SShort sdpstLfBMP::findSlotNoByExtPID( sdpstLfBMPHdr  * aLfBMPHdr,
                                              scPageID         aExtFstPID )
{
    UInt              sLoop;
    SShort            sSlotNo;
    sdpstRangeMap   * sMapPtr;

    IDE_ASSERT( aLfBMPHdr  != NULL );
    IDE_ASSERT( aExtFstPID != SD_NULL_PID );

    sSlotNo = SDPST_INVALID_SLOTNO;
    sMapPtr = getMapPtr( aLfBMPHdr );

    for( sLoop = 0; sLoop < aLfBMPHdr->mBMPHdr.mSlotCnt; sLoop++ )
    {
        if ( sMapPtr->mRangeSlot[sLoop].mFstPID == aExtFstPID )
        {
            sSlotNo = (SShort)sLoop;
            break; // found it !!
        }
    }
    return sSlotNo;
}

/* PageBitSet�� �ش��ϴ� MFNL�� �����Ѵ�. */
inline sdpstMFNL sdpstLfBMP::convertPBSToMFNL( sdpstPBS  aPBS )
{
    sdpstMFNL        sMFNL;
    sdpstPBS  sFnPageBitSet;

    sFnPageBitSet = aPBS & (~SDPST_BITSET_PAGETP_MASK);
    switch ( sFnPageBitSet )
    {
        case SDPST_BITSET_PAGEFN_UNF:
            sMFNL = SDPST_MFNL_UNF;
            break;
        case SDPST_BITSET_PAGEFN_FMT:
            sMFNL = SDPST_MFNL_FMT;
            break;
        case SDPST_BITSET_PAGEFN_INS:
            sMFNL = SDPST_MFNL_INS;
            break;
        case SDPST_BITSET_PAGEFN_FUL:
            sMFNL = SDPST_MFNL_FUL;
            break;
        default:
            IDE_ASSERT( 0 );
    }
    return sMFNL;
}

/* LfBMP�� Page Range ���� �������� ������ ������ ����.
 * ��, ������ �� �ִ� Page���� */
inline UShort sdpstLfBMP::getFreePageRangeCnt( sdpstLfBMPHdr *aLfBMPHdr )
{
    return (UShort)(aLfBMPHdr->mPageRange - aLfBMPHdr->mTotPageCnt);
}

#endif // _O_SDPST_LF_BMP_H_
