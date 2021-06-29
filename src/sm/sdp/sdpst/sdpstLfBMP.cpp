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
 * $Id: sdpstLfBMP.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Leaf Bitmap ������ ����
 * STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpReq.h>

# include <sdpstBMP.h>
# include <sdpstItBMP.h>
# include <sdpstAllocPage.h>

# include <sdpstLfBMP.h>
# include <sdpstStackMgr.h>
# include <sdpstFindPage.h>
# include <sdpstDPath.h>
# include <sdpstCache.h>

sdpstBMPOps sdpstLfBMP::mLfBMPOps =
{
    (sdpstGetBMPHdrFunc)sdpstLfBMP::getBMPHdrPtr,
    (sdpstGetSlotOrPageCntFunc)sdpstLfBMP::getTotPageCnt,
    (sdpstGetDistInDepthFunc)sdpstStackMgr::getDistInLfDepth,
    (sdpstIsCandidateChildFunc)sdpstLfBMP::isCandidatePage,
    (sdpstGetStartSlotOrPBSNoFunc)sdpstLfBMP::getStartPBSNo,
    (sdpstGetChildCountFunc)sdpstLfBMP::getTotPageCnt,
    (sdpstSetCandidateArrayFunc)sdpstLfBMP::setCandidateArray,
    (sdpstGetFstFreeSlotOrPBSNoFunc)sdpstLfBMP::getFstFreePBSNo,
    (sdpstGetMaxCandidateCntFunc)smuProperty::getTmsCandidatePageCnt,
    (sdpstUpdateBMPUntilHWMFunc)sdpstDPath::updateBMPUntilHWMInLfBMP
};

/***********************************************************************
 * Description : LfBMP Header �ʱ�ȭ
 *
 * ������ ������ �ѹ��� �����ؾ��� ������ ��� �����Ѵ�.
 ***********************************************************************/
void  sdpstLfBMP::initBMPHdr( sdpstLfBMPHdr     * aLfBMPHdr,
                              sdpstPageRange      aPageRange,
                              scPageID            aParentBMP,
                              UShort              aSlotNoInParent,
                              UShort              aNewPageCount,
                              UShort              aMetaPageCount,
                              scPageID            aExtDirPID,
                              SShort              aSlotNoInExtDir,
                              scPageID            aRangeFstPID,
                              UShort              aMaxSlotCnt,
                              idBool            * aNeedToChangeMFNL )
{
    sdpstBMPHdr     * sBMPHdr;
    sdpstMFNL         sNewMFNL;
    sdpstRangeMap   * sMapPtr;
    UInt              sLoop;

    IDE_DASSERT( aLfBMPHdr    != NULL );
    IDE_DASSERT( aParentBMP   != SD_NULL_PID );
    IDE_DASSERT( aRangeFstPID != SD_NULL_PID );

    /* lf-bmp header�� �ʱ�ȭ�Ͽ����Ƿ� sdpPhyPageHdr��
     * freeOffset�� total free size�� �����Ѵ�. */
    sdpPhyPage::initLogicalHdr( sdpPhyPage::getHdr( (UChar*)aLfBMPHdr ),
                                ID_SIZEOF( sdpstLfBMPHdr ) );

    sBMPHdr = &(aLfBMPHdr->mBMPHdr);

    sBMPHdr->mType                    = SDPST_LFBMP;
    sBMPHdr->mParentInfo.mParentPID   = aParentBMP;
    sBMPHdr->mParentInfo.mIdxInParent = aSlotNoInParent;
    sBMPHdr->mFstFreeSlotNo           = SDPST_INVALID_SLOTNO;
    sBMPHdr->mFreeSlotCnt             = aMaxSlotCnt;
    sBMPHdr->mMaxSlotCnt              = aMaxSlotCnt;
    sBMPHdr->mSlotCnt                 = 0;
    sBMPHdr->mMFNL                    = SDPST_MFNL_FMT;

    sBMPHdr->mBodyOffset = 
        sdpPhyPage::getDataStartOffset( ID_SIZEOF( sdpstLfBMPHdr ) );

    for ( sLoop = 0; sLoop < SDPST_MFNL_MAX; sLoop++ )
    {
        sBMPHdr->mMFNLTbl[sLoop] = 0;
    }

    aLfBMPHdr->mPageRange = aPageRange;

    /* LfBMP������ ���� Data �������� PBSNo�� �����Ѵ�.
     * ���ķ� ������� �ʴ´�. ���� �ϳ��� LfBMP�� Meta Page�� ��������
     * Data �������� ���� ��찡 �ִµ� �̰�쿡�� Invalid ������ �Ѵ�. */
    if ( aMetaPageCount == aPageRange )
    {
        aLfBMPHdr->mFstDataPagePBSNo = SDPST_INVALID_PBSNO;
    }
    else
    {
        if ( aMetaPageCount >= aPageRange )
        {
            ideLog::log( IDE_SERVER_0,
                         "aMetaPageCount: %u, "
                         "aPageRange: %u\n",
                         aMetaPageCount,
                         aPageRange );

            (void)dump( sdpPhyPage::getPageStartPtr( aLfBMPHdr ) );

            IDE_ASSERT( 0 );
        }
        aLfBMPHdr->mFstDataPagePBSNo = aMetaPageCount;
    }

    /* RangeMap�� �ʱ�ȭ�Ѵ�. getMapPtr()�� ȣ���ϱ� ������ �ݵ��
     * sdpstLfBMPHdr�� mBodyOffset�� �����Ǿ� �־�� �Ѵ�. */
    sMapPtr = getMapPtr( aLfBMPHdr );
    idlOS::memset( sMapPtr, 0x00, ID_SIZEOF(sdpstRangeMap) );

    /* �ʱ� page range�� �߰��Ѵ�.
     * TotPages�� MFNLtbl[SDPST_MFNL_FUL] ������ �Ʒ� �Լ����� ������Ų��. */
    addPageRangeSlot( aLfBMPHdr,
                      aRangeFstPID,
                      aNewPageCount,
                      aMetaPageCount,
                      aExtDirPID,
                      aSlotNoInExtDir,
                      aNeedToChangeMFNL,
                      &sNewMFNL );
}

/***********************************************************************
 * Description : 1���̻��� Leaf Bitmap ������ ���� �� �ʱ�ȭ
 *
 * LfBMP�� �������� �ʾƵ� �Ǵ� ������ ���ο� extent��
 * ���� last LfBMP�� �߰��Ͽ��� page range �������� ���Եȴٸ�,
 * ���ο� extent�� ���ؼ� lf-bmp �������� �������� �ʾƵ� �ǹǷ�,
 * ������ ����ߴ� lf-bmp ������ ���������� �����Ѵ�.
 *
 * lf-bmp ���������� �Ҵ�� extent�� ù��° ���������� �����ϰ�
 * ��ġ�ϹǷ� bmp �߿� ó������ �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdpstLfBMP::createAndInitPages( idvSQL               * aStatistics,
                                       sdrMtxStartInfo      * aStartInfo,
                                       scSpaceID              aSpaceID,
                                       sdpstExtDesc         * aExtDesc,
                                       sdpstBfrAllocExtInfo * aBfrInfo,
                                       sdpstAftAllocExtInfo * aAftInfo )
{
    UInt              sState = 0 ;
    sdrMtx            sMtx;
    UInt              sLoop;
    scPageID          sCurrLfPID;
    scPageID          sParentItPID;
    UShort            sSlotNoInParent;
    UChar           * sNewPagePtr;
    sdpstPageRange    sCurrPageRange;
    UShort            sNewPageCnt;
    sdpstPBS          sPBS;
    UShort            sTotMetaCnt;
    UShort            sMetaCnt;
    ULong             sTotPageCnt;
    idBool            sNeedToChangeMFNL;
    scPageID          sRangeFstPID;
    ULong             sSeqNo;

    IDE_DASSERT( aExtDesc         != NULL );
    IDE_DASSERT( aBfrInfo != NULL );
    IDE_DASSERT( aAftInfo != NULL );
    IDE_DASSERT( aStartInfo       != NULL );
    IDE_DASSERT( aAftInfo->mPageCnt[SDPST_LFBMP] > 0 );

    /* ���� ���������� ���� SlotNo�� ���� SlotNo�� ���Ѵ�. */
    if ( aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] > 0 )
    {
        /* ���� ������ it-bmp�� ������ slot�� �����ϴ� ��� */
        sSlotNoInParent = aBfrInfo->mMaxSlotCnt[SDPST_ITBMP] -
                          aBfrInfo->mFreeSlotCnt[SDPST_ITBMP];
    }
    else
    {
        sSlotNoInParent = 0;
    }

    if ( aAftInfo->mPageCnt[SDPST_EXTDIR] > 0 )
    {
       IDE_ASSERT( aExtDesc->mExtFstPID ==
                   aAftInfo->mLstPID[SDPST_EXTDIR] );
    }
    else
    {
        IDE_ASSERT( aBfrInfo->mLstPID[SDPST_EXTDIR] ==
                    aAftInfo->mLstPID[SDPST_EXTDIR] );
    }

    /* ��� Meta �������� PageBitSet�� ó���Ѵ�.
     * ������ Ÿ���� META �̰�, FULL �����̴�. */
    sPBS = (sdpstPBS) (SDPST_BITSET_PAGETP_META | SDPST_BITSET_PAGEFN_FUL);

    sTotPageCnt = aExtDesc->mLength;
    sTotMetaCnt = aAftInfo->mPageCnt[SDPST_EXTDIR] +
                  aAftInfo->mPageCnt[SDPST_LFBMP] +
                  aAftInfo->mPageCnt[SDPST_ITBMP] +
                  aAftInfo->mPageCnt[SDPST_RTBMP] +
                  aAftInfo->mSegHdrCnt;

    aAftInfo->mFullBMPCnt[SDPST_LFBMP] = 0; // �ʱ�ȭ

    /* ���� Extent�� ���õ� Page Range */
    sCurrPageRange = aAftInfo->mPageRange;

    /* �Ҵ�� Extent�� ó�� �������� PID :
     * ExtDir ������ ������ ����Ͽ� ó���Ѵ�. */
    sCurrLfPID     = aExtDesc->mExtFstPID +
                     aAftInfo->mPageCnt[SDPST_EXTDIR];
  
    sNewPagePtr    = NULL;

    /* aExtDesc�� ���ؼ��� ù��° lf-bmp �������� PID��
     * ù��° Data �������� PID�� �����ؾ��Ѵ�.
     * ������ �� �Լ������� ù���� lf-bmp ��������
     * PID���� �˼� �ִ�. */
    aExtDesc->mExtMgmtLfBMP  = sCurrLfPID;
    aExtDesc->mExtFstDataPID = aExtDesc->mExtFstPID + sTotMetaCnt;

    /* BMP �������� ������ SeqNo�� ����Ѵ�. */
    sSeqNo = aBfrInfo->mNxtSeqNo + aAftInfo->mPageCnt[SDPST_EXTDIR];

    /* �����ؾ��� lf-bmp ������ ������ŭ �ݺ� */
    for ( sLoop = 0;
          sLoop < aAftInfo->mPageCnt[SDPST_LFBMP];
          sLoop++ )
    {
        sState = 0;
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        /* lf-bmp �ʱ�ȭ�ÿ� it-bmp �������� pid�� �����ؾ��Ѵ�.
         * it-bmp�� ������ ������ �� �ֱ⵵�ϰ�, ���� it-bmp�� ����Ǳ⵵
         * �ϱ� ������ �̸� ����س��� �Ѵ�.(I/O �������)
         *
         * ���� it-bmp�� ����� lf-bmp �������� ������
         * ���� ������ it-bmp �������� PID�� max slot������
         * ������ ����� �� �ִ�. */
        if ( aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] > sLoop )
        {
            /* ���� ������ itbmp �������� pid */
            sParentItPID  = aBfrInfo->mLstPID[SDPST_ITBMP];
        }
        else
        {
            /*
             * BUG-22958 bitmap segment���� it-bmp�� rt-bmp�������ϴ��� 
             * �״°�찡 ����.
             */
            /* Extent�� ù��° pid���� lf bmps���� �� ������
             * �� ������ it bmp�� ��ġ�ϹǷ�, �� ��ġ��
             * �������� PID�� ����Ѵ�. */
            sParentItPID  = aExtDesc->mExtFstPID +
                            aAftInfo->mPageCnt[SDPST_EXTDIR] +
                            aAftInfo->mPageCnt[SDPST_LFBMP] +
                            ( (sLoop - aBfrInfo->mFreeSlotCnt[SDPST_ITBMP]) /
                              aBfrInfo->mMaxSlotCnt[SDPST_ITBMP] ) ;

            IDE_ASSERT( aAftInfo->mPageCnt[SDPST_ITBMP] > 0 );
        }

        /* LfBMP�� �߰��� �������� ù��° PageID�� ���Ѵ�.  */
        sRangeFstPID =  aExtDesc->mExtFstPID + (aAftInfo->mPageRange * sLoop);

        /* �������� parent It-bmp�� slot ������ ����ؾ��ϹǷ�
         * �������� �� �� �ִ� �ִ� slot ������ mod �����Ѵ�.
         * sSlotNoInParent�� max slot �� �����ϰ� �Ǹ� �ٽ� 0���� �����ϰ� 
         * �ϱ� ���� mod ������. */
        sSlotNoInParent = sSlotNoInParent % aBfrInfo->mMaxSlotCnt[SDPST_ITBMP];

        IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                              &sMtx,
                                              aSpaceID,
                                              NULL, /* aSegHandle4DataPage */
                                              sCurrLfPID,
                                              sSeqNo,
                                              SDP_PAGE_TMS_LFBMP,
                                              sParentItPID,
                                              sSlotNoInParent,
                                              sPBS,
                                              &sNewPagePtr ) != IDE_SUCCESS );

        /* new LfBMP �� ������ ������ �����Ѵ�. */
        sNewPageCnt = ( sTotPageCnt < (UShort)sCurrPageRange ?
                        (UShort)sTotPageCnt : (UShort)sCurrPageRange );
        /* Leaf bmp�� ������ ������ ������ ����. */
        sTotPageCnt -= sNewPageCnt;

        /* ������ LfBMP���� Meta �������� �����ؾ� �Ѵٸ�,
         * PBS �̸� ����Ͽ� �����Ѵ�.
         * ������ PBS�� sPBS�� ����ִ�. */
        if ( sTotMetaCnt > 0 )
        {
            sMetaCnt = ( sTotMetaCnt < sNewPageCnt ?
                         sTotMetaCnt : sNewPageCnt );

            /* PBS�� ������ meta page ������ ����. */
            sTotMetaCnt -= sMetaCnt;
        }
        else
        {
            sMetaCnt = 0; // Meta Page�� ���� PageBitSet ������ �Ϸ��
        }

        /* LfBMP ������ �ʱ�ȭ �� write logging */
        IDE_TEST( logAndInitBMPHdr( &sMtx,
                                    getHdrPtr(sNewPagePtr),
                                    sCurrPageRange,
                                    sParentItPID,
                                    sSlotNoInParent,
                                    sNewPageCnt,
                                    sMetaCnt,
                                    aAftInfo->mLstPID[SDPST_EXTDIR],
                                    aAftInfo->mSlotNoInExtDir,
                                    sRangeFstPID,
                                    aBfrInfo->mMaxSlotCnt[SDPST_LFBMP],
                                    &sNeedToChangeMFNL ) != IDE_SUCCESS );

        if ( sNeedToChangeMFNL == ID_TRUE )
        {
            // FULL ���·� �����ؾ��� ���� BMP�� slot�� ����
            aAftInfo->mFullBMPCnt[SDPST_LFBMP]++;
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        // ������ Extent������ ���� ������ ID�� ���Ѵ�.
        sCurrLfPID++;

        // sSlotNo�� ������� ������Ų��.
        sSlotNoInParent++;

        /* SeqNo�� ������Ų��. */
        sSeqNo++;
    }

    /* ���ο� leaf bmp ������������ �����Ѵ�. */
    aAftInfo->mFstPID[SDPST_LFBMP] = aExtDesc->mExtFstPID +
                                     aAftInfo->mPageCnt[SDPST_EXTDIR];

    aAftInfo->mLstPID[SDPST_LFBMP] = aExtDesc->mExtFstPID +
                                     aAftInfo->mPageCnt[SDPST_EXTDIR] +
                                     aAftInfo->mPageCnt[SDPST_LFBMP] - 1;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Leaf BMP Control Header �ʱ�ȭ �� write logging
 *
 * aMtx             - [IN] Mini Transaction Pointer
 * aLfBMPHdr        - [IN] LfBMP Header
 * aPageRange       - [IN] Page Range
 * aParentItBMP     - [IN] Parent ItBMP
 * aSlotNoInParent  - [IN] Parent������ SlotNo
 * aNewPageCnt      - [IN] LfBMP�� �߰��� ������ ����
 * aExtDirPID       - [IN] �߰��� �������� ���Ե� ExtDirPID
 * aSlotNoInExtDir  - [IN] ExtDir �������� SlotNo
 * aRangeFstPID     - [IN] �߰��� ù��° �������� PID
 * aMaxSlotCnt      - [IN] LfBMP�� �ִ� RangeSlot ����
 * aNeedToChangeMFNL    - [OUT] MFNL ���� ����
 ***********************************************************************/
IDE_RC sdpstLfBMP::logAndInitBMPHdr( sdrMtx            * aMtx,
                                     sdpstLfBMPHdr     * aLfBMPHdr,
                                     sdpstPageRange      aPageRange,
                                     scPageID            aParentItBMP,
                                     UShort              aSlotNoInParent,
                                     UShort              aNewPageCnt,
                                     UShort              aMetaPageCnt,
                                     scPageID            aExtDirPID,
                                     SShort              aSlotNoInExtDir,
                                     scPageID            aRangeFstPID,
                                     UShort              aMaxSlotCnt,
                                     idBool            * aNeedToChangeMFNL )
{
    sdpstRedoInitLfBMP  sLogData;

    IDE_DASSERT( aMtx    != NULL );
    IDE_DASSERT( aLfBMPHdr != NULL );

    /* page range slot�ʱ�ȭ�� ���ش�. */
    initBMPHdr( aLfBMPHdr,
                aPageRange,
                aParentItBMP,
                aSlotNoInParent,
                aNewPageCnt,
                aMetaPageCnt,
                aExtDirPID,
                aSlotNoInExtDir,
                aRangeFstPID,
                aMaxSlotCnt,
                aNeedToChangeMFNL );

    /* Logging */
    sLogData.mPageRange      = aPageRange;
    sLogData.mParentInfo.mParentPID   = aParentItBMP;
    sLogData.mParentInfo.mIdxInParent = aSlotNoInParent;
    sLogData.mRangeFstPID    = aRangeFstPID;
    sLogData.mExtDirPID      = aExtDirPID;
    sLogData.mNewPageCnt     = aNewPageCnt;
    sLogData.mMetaPageCnt    = aMetaPageCnt;
    sLogData.mSlotNoInExtDir = aSlotNoInExtDir;
    sLogData.mMaxSlotCnt     = aMaxSlotCnt;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aLfBMPHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_INIT_LFBMP )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : leaf bmp���� data �������� PageBitSet�� �����Ѵ�.
 *
 * leaf bmp �������� MFNL�� ����Ǵ� ��쿡�� ȣ���ؾ��ϹǷ� ȣ��Ǳ�
 * ���� ���濩�θ� �̸� �Ǵ��Ͽ����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::tryToChangeMFNL( idvSQL          * aStatistics,
                                    sdrMtx          * aMtx,
                                    scSpaceID         aSpaceID,
                                    scPageID          aLfBMP,
                                    SShort            aPBSNo,
                                    sdpstPBS          aNewPBS,
                                    idBool          * aNeedToChangeMFNL,
                                    sdpstMFNL       * aNewMFNL,
                                    scPageID        * aParentItBMP,
                                    SShort          * aSlotNoInParent )
{
    UChar           * sPagePtr;
    sdpstLfBMPHdr   * sLfBMPHdr;
    sdpstRangeMap   * sMapPtr;
    sdpstMFNL         sPageNewFN;
    sdpstMFNL         sPagePrvFN;
    sdpstPBS        * sPBSPtr;
    UShort            sMetaPageCount;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( aPBSNo != SDPST_INVALID_PBSNO );
    IDE_DASSERT( aNeedToChangeMFNL != NULL );
    IDE_DASSERT( aNewMFNL != NULL );
    IDE_DASSERT( aParentItBMP != NULL );
    IDE_DASSERT( aSlotNoInParent != NULL );

    /* LfBMP���� PBS�� �����Ѵ�. */
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

    sLfBMPHdr = getHdrPtr( sPagePtr );
    sMapPtr   = sdpstLfBMP::getMapPtr( sLfBMPHdr );
    IDE_DASSERT( verifyBMP( sLfBMPHdr ) == IDE_SUCCESS );

    sPBSPtr = &(sMapPtr->mPBSTbl[ aPBSNo ]);

    if ( *sPBSPtr == aNewPBS )
    {
        ideLog::log( IDE_SERVER_0,
                     "Before PBS: %u, "
                     "After PBS : %u\n",
                     *sPBSPtr,
                     aNewPBS );
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    /* ���� �������� ���뵵 ������ �����Ҷ��� Unformat�� ���� ����.
     * �ռ��� �������� �Ѳ����� ������ �Ǳ⶧���̴�. */
    if ( isEqFN( *sPBSPtr, SDPST_BITSET_PAGEFN_UNF ) == ID_TRUE )
    {
        ideLog::log( IDE_SERVER_0,
                     "PBS: %u\n",
                     *sPBSPtr );
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    if ( isDataPage( *sPBSPtr ) != ID_TRUE )
    {
        ideLog::log( IDE_SERVER_0,
                     "PBS: %u\n",
                     *sPBSPtr );
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    sPagePrvFN = convertPBSToMFNL( *sPBSPtr );

    if ( isDataPage( aNewPBS ) != ID_TRUE )
    {
        ideLog::log( IDE_SERVER_0,
                     "New PBS: %u\n",
                     aNewPBS );
        (void)dump( sPagePtr );
        IDE_ASSERT( 0 );
    }

    sPageNewFN = convertPBSToMFNL( aNewPBS );

    // Data �������� BitSet�� �����Ѵ�.
    IDE_TEST( logAndUpdatePBS( aMtx,
                               sMapPtr,
                               aPBSNo,
                               aNewPBS,
                               1,
                               &sMetaPageCount ) != IDE_SUCCESS );

    /* lf-BMP ����� MFNL Table�� ��ǥ MFNL �� �����Ѵ�. */
    IDE_TEST( sdpstBMP::logAndUpdateMFNL( aMtx,
                                          getBMPHdrPtr(sLfBMPHdr),
                                          SDPST_INVALID_SLOTNO,
                                          SDPST_INVALID_SLOTNO,
                                          sPagePrvFN,
                                          sPageNewFN,
                                          1,
                                          aNewMFNL,
                                          aNeedToChangeMFNL ) != IDE_SUCCESS );

    IDE_DASSERT( verifyBMP( sLfBMPHdr ) == IDE_SUCCESS );

    // ���� ���¸� �����ϱ� ���ؼ� �θ� it-bmp �������� PID�� ���Ѵ�.
    *aParentItBMP     = sLfBMPHdr->mBMPHdr.mParentInfo.mParentPID;
    *aSlotNoInParent  = sLfBMPHdr->mBMPHdr.mParentInfo.mIdxInParent;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Page Range Slot�� �߰��Ѵ�.
 ***********************************************************************/
void sdpstLfBMP::addPageRangeSlot( sdpstLfBMPHdr    * aLfBMPHdr,
                                   scPageID           aFstPID,
                                   UShort             aNewPageCount,
                                   UShort             aMetaPageCount,
                                   scPageID           aExtDirPID,
                                   SShort             aSlotNoInExtDir,
                                   idBool           * aNeedToChangeMFNL,
                                   sdpstMFNL        * aNewMFNL )
{
    SShort            sCurrSlotNo;
    sdpstRangeMap   * sMapPtr;
    SShort            sFstPBSNoInRange;
    sdpstMFNL         sNewMFNL;
    UShort            sMetaPageCount;

    IDE_DASSERT( aFstPID         != SD_NULL_PID );
    IDE_DASSERT( aNewPageCount   <= (UShort)SDPST_PAGE_RANGE_1024 );
    IDE_DASSERT( aNeedToChangeMFNL   != NULL );
    IDE_DASSERT( aNewMFNL        != NULL );
    IDE_DASSERT( aExtDirPID      != SD_NULL_PID );
    IDE_DASSERT( aSlotNoInExtDir != SDPST_INVALID_SLOTNO );


    sCurrSlotNo = aLfBMPHdr->mBMPHdr.mSlotCnt;
    sMapPtr     = getMapPtr( aLfBMPHdr );

    /* Range Slot�� ù��° PBSNo�� ����Ѵ�. */
    if ( sCurrSlotNo == 0 )
    {
        /* �ƹ��͵� ������ 0���� ���� */
        sFstPBSNoInRange = 0;
    }
    else
    {
        /* ���� RangeSlot�� ������ PBSNo ���� PBSNo */
        sFstPBSNoInRange = sMapPtr->mRangeSlot[ (sCurrSlotNo-1) ].mFstPBSNo +
                           sMapPtr->mRangeSlot[ (sCurrSlotNo-1) ].mLength;
    }

    sMapPtr->mRangeSlot[ sCurrSlotNo ].mFstPID         = aFstPID;
    sMapPtr->mRangeSlot[ sCurrSlotNo ].mLength         = aNewPageCount;
    sMapPtr->mRangeSlot[ sCurrSlotNo ].mFstPBSNo       = sFstPBSNoInRange;
    sMapPtr->mRangeSlot[ sCurrSlotNo ].mExtDirPID      = aExtDirPID;
    sMapPtr->mRangeSlot[ sCurrSlotNo ].mSlotNoInExtDir = aSlotNoInExtDir;

    if ( aMetaPageCount > 0 )
    {
        /* �� �Լ��� DATA �������� PBS�� �����Ѵ�.
         * ������ �ʱ�ȭ�� PBS�� 0x00 (DATA | UNF)���� �����Ͽ��� ������
         * Meta ������ �� ��ŭ PBS�� ������ �� �ִ�. */
        updatePBS( sMapPtr,
                   sFstPBSNoInRange,
                   (sdpstPBS)(SDPST_BITSET_PAGETP_META|SDPST_BITSET_PAGEFN_FUL),
                   aMetaPageCount,
                   &sMetaPageCount);
    }

    updatePBS( sMapPtr,
               sFstPBSNoInRange + aMetaPageCount,
               (sdpstPBS)(SDPST_BITSET_PAGETP_DATA|SDPST_BITSET_PAGEFN_FMT),
               aNewPageCount - aMetaPageCount,
               &sMetaPageCount );

    /* Range Slot�� �߰��� ������ ������ŭ �� page ������ ������Ų��. */
    aLfBMPHdr->mTotPageCnt                       += aNewPageCount;
    aLfBMPHdr->mBMPHdr.mSlotCnt                  += 1;
    aLfBMPHdr->mBMPHdr.mFreeSlotCnt              -= 1;
    aLfBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_FUL]  += aMetaPageCount;
    aLfBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_FMT]  += ( aNewPageCount - aMetaPageCount );

    IDE_DASSERT( isValidTotPageCnt( aLfBMPHdr ) == ID_TRUE );

    /* MFNL�� �����Ѵ�.�ʱⰪ�� SDPST_MFNL_UNF�̴�.
     * calcMFNL �Լ��� ȣ��Ǳ� ���� MFNLtbl�� ������ �Ϸ�Ǿ�� �Ѵ�. */
    sNewMFNL = sdpstAllocPage::calcMFNL( aLfBMPHdr->mBMPHdr.mMFNLTbl );

    if ( aLfBMPHdr->mBMPHdr.mMFNL != sNewMFNL )
    {
        aLfBMPHdr->mBMPHdr.mMFNL = sNewMFNL;
        *aNeedToChangeMFNL = ID_TRUE;
        *aNewMFNL = sNewMFNL;
    }
    else
    {
        *aNeedToChangeMFNL = ID_FALSE;
        *aNewMFNL = aLfBMPHdr->mBMPHdr.mMFNL;
    }
}

/***********************************************************************
 * Description : RangeSlot�� �߰��ɶ� TotPage ������ �����Ѵ�.
 ***********************************************************************/
idBool sdpstLfBMP::isValidTotPageCnt( sdpstLfBMPHdr * aLfBMPHdr )
{
   UInt   sLoop;
   UShort sPageCnt;

   sPageCnt = 0;

   for( sLoop = 0; sLoop < (UInt)SDPST_MFNL_MAX; sLoop++ )
   {
       sPageCnt += aLfBMPHdr->mBMPHdr.mMFNLTbl[sLoop];
   }

   if ( sPageCnt == aLfBMPHdr->mTotPageCnt )
   {
      return ID_TRUE;
   }
   else
   {
      return ID_FALSE;
   }
}

/***********************************************************************
 * Description : Page Range Slot�� �߰��Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::logAndAddPageRangeSlot(
                                sdrMtx               * aMtx,
                                sdpstLfBMPHdr        * aLfBMPHdr,
                                scPageID               aFstPID,
                                UShort                 aLength,
                                UShort                 aMetaPageCnt,
                                scPageID               aExtDirPID,
                                SShort                 aSlotNoInExtDir,
                                idBool               * aNeedToChangeMFNL,
                                sdpstMFNL            * aNewMFNL )
{
    sdpstRedoAddRangeSlot   sLogData;

    IDE_DASSERT( aLfBMPHdr  != NULL );
    IDE_DASSERT( aMtx      != NULL );
    IDE_DASSERT( aNeedToChangeMFNL != NULL );

    addPageRangeSlot( aLfBMPHdr,
                      aFstPID,
                      aLength,  // �� ������ ���� */
                      aMetaPageCnt,
                      aExtDirPID,
                      aSlotNoInExtDir,
                      aNeedToChangeMFNL,
                      aNewMFNL );

    // ADD_RANGESLOT logging
    sLogData.mFstPID         = aFstPID;
    sLogData.mLength         = aLength;
    sLogData.mMetaPageCnt    = aMetaPageCnt;
    sLogData.mExtDirPID      = aExtDirPID;
    sLogData.mSlotNoInExtDir = aSlotNoInExtDir;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aLfBMPHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_ADD_RANGESLOT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : data ���������� pagebitset�� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::logAndUpdatePBS( sdrMtx        * aMtx,
                                    sdpstRangeMap * aMapPtr,
                                    SShort          aFstPBSNo,
                                    sdpstPBS        aPBS,
                                    UShort          aPageCnt,
                                    UShort        * aMetaPageCnt )
{
    sdpstRedoUpdatePBS  sLogData;

    updatePBS( aMapPtr,
               aFstPBSNo,
               aPBS,
               aPageCnt,
               aMetaPageCnt );

    sLogData.mFstPBSNo = aFstPBSNo;
    sLogData.mPBS      = aPBS;
    sLogData.mPageCnt  = aPageCnt;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aMapPtr,
                                         &sLogData,
                                         ID_SIZEOF(sLogData),
                                         SDR_SDPST_UPDATE_PBS )
            != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : data ���������� pagebitset�� �����Ѵ�.
 ***********************************************************************/
void sdpstLfBMP::updatePBS( sdpstRangeMap * aMapPtr,
                            SShort          aFstPBSNo,
                            sdpstPBS        aPBS,
                            UShort          aPageCnt,
                            UShort        * aMetaPageCnt )
{
    UShort             sLoop;
    UShort             sMetaPageCnt;
    sdpstPBS         * sPBSPtr;

    IDE_DASSERT( aMapPtr != NULL );

    sMetaPageCnt   = 0;
    sPBSPtr = aMapPtr->mPBSTbl;

    for( sLoop = aFstPBSNo; sLoop < aFstPBSNo + aPageCnt; sLoop++ )
    {
        if ( isDataPage(sPBSPtr[sLoop]) == ID_TRUE )
        {
            sPBSPtr[sLoop] = aPBS;
        }
        else
        {
            sMetaPageCnt++;
        }
    }

    *aMetaPageCnt = sMetaPageCnt;
}

/***********************************************************************
 * Description : Extent�� ù��° �������� PID�� �ش��ϴ� Leaf Bitmap
 *               ������������ ������ ��ġ�� ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::getPBSNoByExtFstPID( idvSQL          * aStatistics,
                                       scSpaceID         aSpaceID,
                                       scPageID          aLfBMP,
                                       scPageID          aExtFstPID,
                                       SShort          * aPBSNo )
{
    idBool               sDummy;
    UChar              * sPagePtr;
    SShort               sSlotNo;
    sdpstLfBMPHdr      * sLfBMPHdr;

    IDE_DASSERT( aLfBMP     != SD_NULL_PID );
    IDE_DASSERT( aExtFstPID != SD_NULL_PID );
    IDE_DASSERT( aPBSNo   != NULL );

    // ���� leaf�� fix�� ���� �Ѵ�.
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aLfBMP,
                                          &sPagePtr,
                                          &sDummy) != IDE_SUCCESS );

    sLfBMPHdr = getHdrPtr(sPagePtr );

    sSlotNo = findSlotNoByExtPID( sLfBMPHdr, aExtFstPID );

    *aPBSNo = getRangeSlotBySlotNo( getMapPtr(sLfBMPHdr), sSlotNo )->mFstPBSNo;

    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       sPagePtr ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC sdpstLfBMP::verifyBMP( sdpstLfBMPHdr  * aBMPHdr )
{
    SShort             sCurIdx;
    UShort             sLoop;
    UShort             sPageCnt;
    sdpstMFNL          sMFNL;
    UShort             sMFNLtbl[ SDPST_MFNL_MAX ];
    sdpstPBS         * sPBSPtr;

    sPageCnt = aBMPHdr->mTotPageCnt;
    sPBSPtr  = getMapPtr(aBMPHdr)->mPBSTbl;

    idlOS::memset( &sMFNLtbl, 0x00, ID_SIZEOF(UShort) * SDPST_MFNL_MAX );

    for( sLoop = 0, sCurIdx = 0; sLoop < sPageCnt;
         sCurIdx++, sLoop++ )
    {
        sMFNL = convertPBSToMFNL( *(sPBSPtr + sCurIdx) );
        sMFNLtbl[sMFNL] += 1;
    }

    for ( sLoop = 0; sLoop < SDPST_MFNL_MAX; sLoop++ )
    {
        if ( aBMPHdr->mBMPHdr.mMFNLTbl[ sLoop ] != sMFNLtbl[ sLoop ] )
        {
            ideLog::log( IDE_SERVER_0,
                         "BMP Header.mMFNLTbl[FUL] = %u\n"
                         "BMP Header.mMFNLTbl[INS] = %u\n"
                         "BMP Header.mMFNLTbl[FMT] = %u\n"
                         "BMP Header.mMFNLTbl[UNF] = %u\n"
                         "sMFNLTbl[FUL]            = %u\n"
                         "sMFNLTbl[INS]            = %u\n"
                         "sMFNLTbl[FMT]            = %u\n"
                         "sMFNLTbl[UNF]            = %u\n",
                         aBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_FUL],
                         aBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_INS],
                         aBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_FMT],
                         aBMPHdr->mBMPHdr.mMFNLTbl[SDPST_MFNL_UNF],
                         sMFNLtbl[SDPST_MFNL_FUL],
                         sMFNLtbl[SDPST_MFNL_INS],
                         sMFNLtbl[SDPST_MFNL_FMT],
                         sMFNLtbl[SDPST_MFNL_UNF] );

            (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );

            IDE_ASSERT( 0 );
        }
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : LfBMP�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::dumpHdr( UChar    * aPagePtr,
                            SChar    * aOutBuf,
                            UInt       aOutSize )
{
    UChar           * sPagePtr;
    sdpstLfBMPHdr   * sLfBMPHdr;
    sdpstBMPHdr     * sBMPHdr;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPagePtr  = sdpPhyPage::getPageStartPtr( aPagePtr );
    sLfBMPHdr = getHdrPtr( sPagePtr );
    sBMPHdr   = &sLfBMPHdr->mBMPHdr;

    /* LfBMP Header */
    idlOS::snprintf(
        aOutBuf,
        aOutSize,
        "--------------- LfBMP Header Begin ----------------\n"
        "Type                   : %"ID_UINT32_FMT"\n"
        "MFNL                   : %"ID_UINT32_FMT"\n"
        "MFNL Table (UNF)       : %"ID_UINT32_FMT"\n"
        "MFNL Table (FMT)       : %"ID_UINT32_FMT"\n"
        "MFNL Table (INS)       : %"ID_UINT32_FMT"\n"
        "MFNL Table (FUL)       : %"ID_UINT32_FMT"\n"
        "Slot Count             : %"ID_UINT32_FMT"\n"
        "Free Slot Count        : %"ID_UINT32_FMT"\n"
        "Max Slot Count         : %"ID_UINT32_FMT"\n"
        "First Free SlotNo      : %"ID_INT32_FMT"\n"
        "Parent Page ID         : %"ID_UINT32_FMT"\n"
        "SlotNo In Parent       : %"ID_INT32_FMT"\n"
        "Next RtBMP Page ID     : %"ID_UINT32_FMT"\n"
        "Body Offset            : %"ID_UINT32_FMT"\n"
        "Page Range             : %"ID_UINT32_FMT"\n"
        "Total Page Count       : %"ID_UINT32_FMT"\n"
        "First Data Page PBS No : %"ID_INT32_FMT"\n"
        "---------------  LfBMP Header End  ----------------\n",
        sBMPHdr->mType,
        sBMPHdr->mMFNL,
        sBMPHdr->mMFNLTbl[SDPST_MFNL_UNF],
        sBMPHdr->mMFNLTbl[SDPST_MFNL_FMT],
        sBMPHdr->mMFNLTbl[SDPST_MFNL_INS],
        sBMPHdr->mMFNLTbl[SDPST_MFNL_FUL],
        sBMPHdr->mSlotCnt,
        sBMPHdr->mFreeSlotCnt,
        sBMPHdr->mMaxSlotCnt,
        sBMPHdr->mFstFreeSlotNo,
        sBMPHdr->mParentInfo.mParentPID,
        sBMPHdr->mParentInfo.mIdxInParent,
        sBMPHdr->mNxtRtBMP,
        sBMPHdr->mBodyOffset,
        sLfBMPHdr->mPageRange,
        sLfBMPHdr->mTotPageCnt,
        sLfBMPHdr->mFstDataPagePBSNo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : LfBMP�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::dumpBody( UChar    * aPagePtr,
                             SChar    * aOutBuf,
                             UInt       aOutSize )
{
    UChar           * sPagePtr;
    sdpstLfBMPHdr   * sLfBMPHdr;
    sdpstBMPHdr     * sBMPHdr;
    sdpstRangeMap   * sRangeMap;
    sdpstRangeSlot  * sRangeSlot;
    sdpstPBS        * sPBSTbl;
    UInt              sLoop;
    SChar           * sDumpBuf;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );

    sPagePtr  = sdpPhyPage::getPageStartPtr( aPagePtr );
    sLfBMPHdr = getHdrPtr( sPagePtr );
    sBMPHdr   = &sLfBMPHdr->mBMPHdr;
    sRangeMap = getMapPtr( sLfBMPHdr );

    /* BMP RangeSlot */
    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "--------------- LfBMP RangeSlot Begin ----------------\n" );

    for ( sLoop = 0; sLoop < sBMPHdr->mSlotCnt; sLoop++ )
    {
        sRangeSlot = &sRangeMap->mRangeSlot[sLoop];

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%"ID_UINT32_FMT"] "
                             "FstPID: %"ID_UINT32_FMT", "
                             "Length: %"ID_UINT32_FMT", "
                             "FstPBSNo: %"ID_INT32_FMT", "
                             "ExtDirPID: %"ID_UINT32_FMT", "
                             "SlotNoInExtDir: %"ID_INT32_FMT"\n",
                             sLoop,
                             sRangeSlot->mFstPID,
                             sRangeSlot->mLength,
                             sRangeSlot->mFstPBSNo,
                             sRangeSlot->mExtDirPID,
                             sRangeSlot->mSlotNoInExtDir );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "---------------  LfBMP RangeSlot End  ----------------\n" );

    if( iduMemMgr::calloc(
            IDU_MEM_SM_SDP, 1,
            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
            (void**)&sDumpBuf ) == IDE_SUCCESS )
    {
        /* BMP PBS Table */
        sPBSTbl = sRangeMap->mPBSTbl;

        ideLog::ideMemToHexStr( sPBSTbl,
                                SDPST_PAGE_BITSET_TABLE_SIZE,
                                IDE_DUMP_FORMAT_NORMAL,
                                sDumpBuf,
                                IDE_DUMP_DEST_LIMIT );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "--------------- LfBMP PBS Table Begin ----------------\n"
                             "%s\n"
                             "---------------  LfBMP PBS Table End  ----------------\n",
                             sDumpBuf );

        (void)iduMemMgr::free( sDumpBuf );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : LfBMP�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstLfBMP::dump( UChar    * aPagePtr )
{
    UChar           * sPagePtr;
    SChar           * sDumpBuf;

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
        /* LfBMP Header */
        if( dumpHdr( sPagePtr,
                     sDumpBuf,
                     IDE_DUMP_DEST_LIMIT ) == IDE_SUCCESS )
        {
            ideLog::log( IDE_SERVER_0, "%s", sDumpBuf );
        }

        /* LfBMP Body */
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
