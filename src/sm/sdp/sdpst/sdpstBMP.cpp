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
 * $Id: sdpstBMP.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Internal Bitmap ������ ����
 * STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdr.h>
# include <sdpReq.h>

# include <sdpstBMP.h>
# include <sdpstRtBMP.h>
# include <sdpstItBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstAllocPage.h>

# include <sdpstSH.h>
# include <sdpstStackMgr.h>
# include <sdpstCache.h>
# include <sdpstAllocExtInfo.h>

sdpstBMPOps * sdpstBMP::mBMPOps[SDPST_BMP_TYPE_MAX] =
{
    &sdpstRtBMP::mVirtBMPOps,
    &sdpstRtBMP::mRtBMPOps,
    &sdpstItBMP::mItBMPOps,
    &sdpstLfBMP::mLfBMPOps,
};

/***********************************************************************
 * Description : 1���̻��� Internal Bitmap ������ ���� �� �ʱ�ȭ
 *
 * internal bmp ���������� �Ҵ�� extent�� lf-bmp ���������� ���
 * ������ ���� �������������� ��ġ�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstBMP::createAndInitPages( idvSQL               * aStatistics,
                                     sdrMtxStartInfo      * aStartInfo,
                                     scSpaceID              aSpaceID,
                                     sdpstExtDesc         * aExtDesc,
                                     sdpstBMPType           aBMPType,
                                     sdpstBfrAllocExtInfo * aBfrInfo,
                                     sdpstAftAllocExtInfo * aAftInfo )
{
    UInt                sState = 0;
    sdrMtx              sMtx;
    SInt                sLoop;
    sdpstBMPType        sChildBMPType;
    sdpstBMPType        sParentBMPType;
    scPageID            sCurPID;
    scPageID            sParentBMP = SD_NULL_PID ;
    scPageID            sFstChildBMP;
    scPageID            sLstChildBMP;
    UShort              sSlotNoInParent;
    UShort              sMaxSlotCnt;
    UShort              sMaxSlotCntInParent = SC_NULL_SLOTNUM;
    UShort              sNewSlotCnt;
    UShort              sTotFullBMPCnt;
    UShort              sFullBMPCnt;
    UShort              sSlotCnt;
    sdpstPBS            sPBS;
    UChar             * sNewPagePtr;
    idBool              sNeedToChangeMFNL = ID_FALSE ;
    sdpPageType         sPageType;
    ULong               sSeqNo;

    IDE_DASSERT( aExtDesc   != NULL );
    IDE_DASSERT( aBfrInfo   != NULL );
    IDE_DASSERT( aAftInfo   != NULL );
    IDE_DASSERT( aStartInfo != NULL );
    IDE_DASSERT( aBMPType == SDPST_RTBMP || aBMPType == SDPST_ITBMP );

    /*
     * Rt-BMP������ Parent�� ���� ������ sParentBMPType ������ ����ϸ� �ȵȴ�. 
     * �Ʒ� ��ũ�δ� �Է°� ������ �������� �ʴ´�.
     */

    sParentBMPType = SDPST_BMP_PARENT_TYPE( aBMPType );
    sChildBMPType  = SDPST_BMP_CHILD_TYPE( aBMPType );

    /* createPage �Ҷ�, phyPage�� ������ page type �� �����Ѵ�. */
    sPageType      = aBMPType == SDPST_ITBMP ?
                     SDP_PAGE_TMS_ITBMP : SDP_PAGE_TMS_RTBMP;

    /* ������ ������ ���� BMP �������� ������ �����Ѵ�.
     * Extent�� �Ҵ� �ɶ�, ExtDir, LfBMP, ItBMP, RtBMP ������ �����Ǳ� ����.
     * ��, �׻� ���� BMP �� �ռ� �����ȴ�. */
    sCurPID     = aAftInfo->mLstPID[ sChildBMPType ] + 1;
    sMaxSlotCnt = aBfrInfo->mMaxSlotCnt[ aBMPType ];
    sPBS        = (SDPST_BITSET_PAGETP_META | SDPST_BITSET_PAGEFN_FUL);

    /* ���� BMP�� �ִ� slot ������
     * ���� BMP���� ���� ������ slot ��ġ ������ �����Ѵ�. (������ġ)
     * - RtBMP ������ �ǹ� ����.
     *
     * ���� �������� ������ SeqNo�� ����Ѵ�. */
    if ( aBMPType == SDPST_ITBMP )
    {
        sMaxSlotCntInParent = aBfrInfo->mMaxSlotCnt[ sParentBMPType ];
        sSlotNoInParent     = sMaxSlotCntInParent -
                              aBfrInfo->mFreeSlotCnt[ sParentBMPType ];

        sSeqNo = aBfrInfo->mNxtSeqNo +
                 aAftInfo->mPageCnt[SDPST_EXTDIR] +
                 aAftInfo->mPageCnt[SDPST_LFBMP];
    }
    else
    {
        sSlotNoInParent = 0;

        sSeqNo = aBfrInfo->mNxtSeqNo +
                 aAftInfo->mPageCnt[SDPST_EXTDIR] +
                 aAftInfo->mPageCnt[SDPST_LFBMP] +
                 aAftInfo->mPageCnt[SDPST_ITBMP];
    }

    /* ���� ������ BMP�������� ��ϵ� ���� BMP ���� */
    sNewSlotCnt    = aAftInfo->mPageCnt[ sChildBMPType ] -
                     aBfrInfo->mFreeSlotCnt[ aBMPType ];

    /* Full ���·� ��ϵǾ���� BMP ������ ���� */
    if ( aBfrInfo->mFreeSlotCnt[ aBMPType ] >=
         aAftInfo->mFullBMPCnt[ sChildBMPType ] )
    {
        sTotFullBMPCnt = 0;
    }
    else
    {
        sTotFullBMPCnt = aAftInfo->mFullBMPCnt[ sChildBMPType ] -
                         aBfrInfo->mFreeSlotCnt[ aBMPType ];

    }

    /* �����ؾ��� ������ ������ŭ �ݺ� */
    for ( sLoop = 0; sLoop < aAftInfo->mPageCnt[ aBMPType ]; sLoop++ )
    {
        sState = 0;
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_TRUE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
        sState = 1;

        if ( aBMPType == SDPST_ITBMP )
        {
            /* Rt-BMP�� �ƴ� ��� �ʱ�ȭ�� ���� BMP�� PID�� �����ؾ� �Ѵ�.
             * ���� �����Ǵ� �������� ����  ���� ������ ���� BMP �������� ���
             * �ɼ��� �ְ�, ���� BMP�� ��ϵ� ���� �ִ�. */
            if ( aBfrInfo->mFreeSlotCnt[ sParentBMPType ] > sLoop )
            {
                sParentBMP = aBfrInfo->mLstPID[ sParentBMPType ];
            }
            else
            {
                sParentBMP = aExtDesc->mExtFstPID +
                             aAftInfo->mPageCnt[SDPST_EXTDIR] +
                             aAftInfo->mPageCnt[SDPST_LFBMP] +
                             aAftInfo->mPageCnt[SDPST_ITBMP] +
                             ((sLoop - aBfrInfo->mFreeSlotCnt[SDPST_RTBMP]) /
                              aBfrInfo->mMaxSlotCnt[SDPST_RTBMP]);

                if ( aAftInfo->mPageCnt[ sParentBMPType ] <= 0 )
                {
                    ideLog::log( IDE_SERVER_0,
                                 "Curr BMP Type: %u, "
                                 "Parent BMP Type: %u, "
                                 "Child BMP Type: %u\n",
                                 aBMPType,
                                 sParentBMPType,
                                 sChildBMPType );

                    sdpstAllocExtInfo::dump( aBfrInfo );
                    sdpstAllocExtInfo::dump( aAftInfo );

                    IDE_ASSERT( 0 );
                }
            }

            /* �������� Parent BMP�� �� BMP�� �� slot ������ ����ؾ� �ϹǷ�
             * �� �������� �� �� �ִ� �ִ� slot������ mod �����Ѵ�. */
            sSlotNoInParent = sSlotNoInParent % sMaxSlotCntInParent;

            /* �� BMP �������� ��ϵ� ù��° ���� BMP �������� PID�� ����Ѵ� */
            sFstChildBMP = aExtDesc->mExtFstPID +
                           aAftInfo->mPageCnt[SDPST_EXTDIR] +
                           aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] +
                           (sMaxSlotCnt * sLoop);
        }
        else
        {
            IDE_DASSERT( aBMPType == SDPST_RTBMP );
            sFstChildBMP = aExtDesc->mExtFstPID +
                           aAftInfo->mPageCnt[SDPST_EXTDIR] +
                           aAftInfo->mPageCnt[SDPST_LFBMP] +
                           aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] +
                           (sMaxSlotCnt * sLoop);
        }

        if ( sNewSlotCnt >= sMaxSlotCnt )
        {
            sSlotCnt = sMaxSlotCnt;
        }
        else
        {
            sSlotCnt = sNewSlotCnt;
        }

        /* ������ ���� BMP */
        sLstChildBMP = sFstChildBMP + sSlotCnt - 1;
        /* ��������Ƿ� ��ü ����ؾ��� �������� ����. */
        sNewSlotCnt -= sSlotCnt;

        IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                              &sMtx,
                                              aSpaceID,
                                              NULL /*aSegHandle4DataPage*/,
                                              sCurPID,
                                              sSeqNo,
                                              sPageType,
                                              aExtDesc->mExtFstPID,
                                              SDPST_INVALID_SLOTNO,
                                              sPBS,
                                              &sNewPagePtr ) != IDE_SUCCESS );

        if ( sTotFullBMPCnt > 0 )
        {
            sFullBMPCnt     = ( sTotFullBMPCnt <= sSlotCnt ?
                                sTotFullBMPCnt : sSlotCnt );
            sTotFullBMPCnt -= sFullBMPCnt;
        }
        else
        {
            sFullBMPCnt = 0;
        }

        /* ItBMP �ʱ�ȭ �� write logging */
        IDE_TEST( logAndInitBMPHdr( &sMtx,
                                    getHdrPtr( sNewPagePtr ),
                                    aBMPType,
                                    sParentBMP,
                                    sSlotNoInParent,
                                    sFstChildBMP,
                                    sLstChildBMP,
                                    sFullBMPCnt,
                                    sMaxSlotCnt,
                                    &sNeedToChangeMFNL ) != IDE_SUCCESS );

        if ( sNeedToChangeMFNL == ID_TRUE )
        {
            /* FULL ���·� �����ؾ��� ���� itslot�� ���� */
            aAftInfo->mFullBMPCnt[sParentBMPType]++;
        }

        if ( aBMPType == SDPST_RTBMP )
        {
            /* ���������� ������ RtBMP�� �ƴ� ��쿡�� ���� ������ RtBMP PID
             * �� �����Ѵ�. */
            if ( sLoop != aAftInfo->mPageCnt[ aBMPType ] - 1 )
            {
                IDE_TEST( sdpstRtBMP::setNxtRtBMP( aStatistics,
                                                   &sMtx,
                                                   aSpaceID,
                                                   sCurPID,
                                                   sCurPID + 1 )
                          != IDE_SUCCESS );
            }
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        /* ������ Extent������ ���� ������ ID�� slotNo�� ���Ѵ� */
        sCurPID = sCurPID + 1;
        sSlotNoInParent++;
        sSeqNo++;
    }

    /* ���ο� BMP ������ ������ �����Ѵ�. */
    aAftInfo->mFstPID[aBMPType] = aAftInfo->mLstPID[sChildBMPType] + 1;
    aAftInfo->mLstPID[aBMPType] = aAftInfo->mLstPID[sChildBMPType] +
                                  aAftInfo->mPageCnt[aBMPType];

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : It-BMP ������ ����� �ʱ�ȭ �Ѵ�.
 *
 * aBMPHdr          - [IN] BMP header
 * aBMPType         - [IN] BMP Type ( IIBMP or RTBMP )
 * aBodyOffset      - [IN] Body Offset (RtBMP�� SegHdr�� ������ �� ���� �޶���)
 * aParentBMP       - [IN] Parent BMP PID
 * aSlotNoInParent  - [IN] Parent BMP������ SlotNo
 * aFstBMP          - [IN] ù��° child BMP PID
 * aLstBMP          - [IN] ������ child BMP PID
 * aFullBMPs        - [IN] FULL ���·� ��ϵǾ���� Child BMP ����
 * aMaxSlotCnt      - [IN] BMP�� �ִ� Slot ����
 * aNeedToChangeMFNL    - [OUT] MFNL ���� ����
 ***********************************************************************/
void  sdpstBMP::initBMPHdr( sdpstBMPHdr    * aBMPHdr,
                            sdpstBMPType     aBMPType,
                            scOffset         aBodyOffset,
                            scPageID         aParentBMP,
                            UShort           aSlotNoInParent,
                            scPageID         aFstBMP,
                            scPageID         aLstBMP,
                            UShort           aFullBMPs,
                            UShort           aMaxSlotCnt,
                            idBool         * aNeedToChangeMFNL )
{
    UInt              sNewSlotCount;
    sdpstMFNL         sNewMFNL;
    UInt              sLoop;

    IDE_DASSERT( aBMPHdr != NULL );
    IDE_DASSERT( aFstBMP <= aLstBMP );

    /* ������ Extent ���� ���������̹Ƿ� PID�� ������ ������ ���Ѵ�. */
    sNewSlotCount = aLstBMP - aFstBMP + 1;

    if ( aMaxSlotCnt < sNewSlotCount )
    {
        ideLog::log( IDE_SERVER_0,
                     "aMaxSlotCnt: %u, "
                     "sNewSlotCount: %u\n",
                     aMaxSlotCnt,
                     sNewSlotCount );
        (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
        IDE_ASSERT( 0 );
    }

    if ( sNewSlotCount < aFullBMPs )
    {
        ideLog::log( IDE_SERVER_0,
                     "sNewSlotCount: %u, ",
                     "aFullBMPs: %u\n",
                     sNewSlotCount,
                     aFullBMPs );
        (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
        IDE_ASSERT( 0 );
    }

    /* it-bmp control header�� �ʱ�ȭ�Ͽ����Ƿ� sdpPhyPageHdr��
     * freeOffset�� total free size�� �����Ѵ�. */
    sdpPhyPage::initLogicalHdr( sdpPhyPage::getHdr( (UChar*)aBMPHdr ),
                                ID_SIZEOF( sdpstBMPHdr ) );

    aBMPHdr->mType                    = aBMPType;
    aBMPHdr->mParentInfo.mParentPID   = aParentBMP;
    aBMPHdr->mParentInfo.mIdxInParent = aSlotNoInParent;
    aBMPHdr->mFstFreeSlotNo           = SDPST_INVALID_SLOTNO;
    aBMPHdr->mFreeSlotCnt             = aMaxSlotCnt;
    aBMPHdr->mMaxSlotCnt              = aMaxSlotCnt;
    aBMPHdr->mNxtRtBMP                = SD_NULL_PID;

    /* ������������ Body ������ ���� Offset�� ���Ѵ�. */
    aBMPHdr->mBodyOffset = aBodyOffset;

    for ( sLoop = 0; sLoop < SDPST_MFNL_MAX; sLoop++ )
    {
        aBMPHdr->mMFNLTbl[sLoop] = 0;
    }

    aBMPHdr->mSlotCnt = 0;
    aBMPHdr->mMFNL    = SDPST_MFNL_FMT;

    if ( aFstBMP != SD_NULL_PID )
    {
        addSlots( aBMPHdr, (SShort)0, aFstBMP, aLstBMP, aFullBMPs,
                  aNeedToChangeMFNL, &sNewMFNL );
    }
}


/***********************************************************************
 * Description : Internal BMP Control Header �ʱ�ȭ
 ***********************************************************************/
void  sdpstBMP::initBMPHdr( sdpstBMPHdr    * aBMPHdr,
                            sdpstBMPType     aBMPType,
                            scPageID         aParentBMP,
                            UShort           aSlotNoInParent,
                            scPageID         aFstBMP,
                            scPageID         aLstBMP,
                            UShort           aFullBMPs,
                            UShort           aMaxSlotCnt,
                            idBool         * aNeedToChangeMFNL )
{
    initBMPHdr( aBMPHdr,
                aBMPType,
                sdpPhyPage::getDataStartOffset(ID_SIZEOF(sdpstBMPHdr)),
                aParentBMP,
                aSlotNoInParent,
                aFstBMP,
                aLstBMP,
                aFullBMPs,
                aMaxSlotCnt,
                aNeedToChangeMFNL );
}


/***********************************************************************
 * Description : Internal BMP Control Header �ʱ�ȭ �� write logging
 *
 * aMtx             - [IN] Mini Transaction Pointer
 * aBMPHdr          - [IN] BMP Header
 * aBMPType         - [IN] BMP Type ( IIBMP or RTBMP )
 * aParentBMP       - [IN] Parent BMP PID
 * aSlotNoInParent  - [IN] Parent BMP������ SlotNo
 * aFstBMP          - [IN] ù��° child BMP PID
 * aLstBMP          - [IN] ������ child BMP PID
 * aFullBMPs        - [IN] FULL ���·� ��ϵǾ���� Child BMP ����
 * aMaxSlotCnt      - [IN] BMP�� �ִ� Slot ����
 * aNeedToChangeMFNL    - [OUT] MFNL ���� ����
 ***********************************************************************/
IDE_RC sdpstBMP::logAndInitBMPHdr( sdrMtx            * aMtx,
                                   sdpstBMPHdr       * aBMPHdr,
                                   sdpstBMPType        aBMPType,
                                   scPageID            aParentBMP,
                                   UShort              aSlotNoInParent,
                                   scPageID            aFstBMP,
                                   scPageID            aLstBMP,
                                   UShort              aFullBMPs,
                                   UShort              aMaxSlotCnt,
                                   idBool            * aNeedToChangeMFNL)
{
    sdpstRedoInitBMP    sLogData;

    IDE_DASSERT( aBMPHdr       != NULL );
    IDE_DASSERT( aMtx          != NULL );
    IDE_DASSERT( aNeedToChangeMFNL != NULL );

    initBMPHdr( aBMPHdr,
                aBMPType,
                aParentBMP,
                aSlotNoInParent,
                aFstBMP,
                aLstBMP,
                aFullBMPs,
                aMaxSlotCnt,
                aNeedToChangeMFNL );

    /* INIT_BMP ������ logging */
    sLogData.mType        = aBMPType;
    sLogData.mParentInfo.mParentPID   = aParentBMP;
    sLogData.mParentInfo.mIdxInParent = aSlotNoInParent;
    sLogData.mFstChildBMP = aFstBMP;
    sLogData.mLstChildBMP = aLstBMP;
    sLogData.mFullBMPCnt  = aFullBMPs;
    sLogData.mMaxSlotCnt  = aMaxSlotCnt;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aBMPHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_INIT_BMP )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : BMP �������� Slot�� �߰��Ѵ�.
 ***********************************************************************/
void sdpstBMP::addSlots( sdpstBMPHdr * aBMPHdr,
                         SShort        aLstSlotNo,
                         scPageID      aFstBMP,
                         scPageID      aLstBMP,
                         UShort        aFullBMPs,
                         idBool      * aNeedToChangeMFNL,
                         sdpstMFNL   * aNewMFNL )
{
    sdpstMFNL   sNewMFNL;
    UShort      sNewSlotCount;

    IDE_ASSERT( aBMPHdr != NULL );
    sNewSlotCount = aLstBMP - aFstBMP + 1;

    addSlotsToMap( getMapPtr(aBMPHdr),
                   aLstSlotNo,
                   aFstBMP,
                   aLstBMP,
                   aFullBMPs );

    aBMPHdr->mSlotCnt     += sNewSlotCount;
    aBMPHdr->mFreeSlotCnt -= sNewSlotCount;

    aBMPHdr->mMFNLTbl[SDPST_MFNL_FUL] += aFullBMPs;
    aBMPHdr->mMFNLTbl[SDPST_MFNL_FMT]  += (sNewSlotCount - aFullBMPs);

    sNewMFNL = sdpstAllocPage::calcMFNL(aBMPHdr->mMFNLTbl);

    if ( sNewMFNL == SDPST_MFNL_FUL )
    {
        (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
        IDE_ASSERT( 0 );
    }


    if ( aBMPHdr->mFstFreeSlotNo == SDPST_INVALID_SLOTNO )
    {
        aBMPHdr->mFstFreeSlotNo = aLstSlotNo + aFullBMPs;
    }
    else
    {
        /* ���� MFNL���°� FULL���� FstFree�� ������ Slot�̾��ٸ�,
         * ���� �߰��� Slot���� �����Ѵ�. */
        if ( (aBMPHdr->mMFNL == SDPST_MFNL_FUL ) &&
             (aBMPHdr->mFstFreeSlotNo + 1 == aLstSlotNo ))
        {
            aBMPHdr->mFstFreeSlotNo  = aLstSlotNo + aFullBMPs;
        }
    }

    if ( aBMPHdr->mMFNL != sNewMFNL )
    {
        aBMPHdr->mMFNL = sNewMFNL;
        *aNeedToChangeMFNL = ID_TRUE;
        *aNewMFNL      = sNewMFNL;
    }
    else
    {
        /* slot�� �߰��Ǿ MFNL�� ������� �ʴ´�.  */
        *aNeedToChangeMFNL = ID_FALSE;
        *aNewMFNL      = aBMPHdr->mMFNL;
    }
}

/***********************************************************************
 * Description : Internal Bitmap �������� lf-slot���� �߰��ϰ�
 *               MFNLtbl�� MFNL�� �����ϴ� �α��� �Ѵ�.
 ************************************************************************/
IDE_RC sdpstBMP::logAndAddSlots( sdrMtx        * aMtx,
                                 sdpstBMPHdr   * aBMPHdr,
                                 scPageID        aFstBMP,
                                 scPageID        aLstBMP,
                                 UShort          aFullBMPs,
                                 idBool        * aNeedToChangeMFNL,
                                 sdpstMFNL     * aNewMFNL )
{
    SShort              sLstSlotNo;
    sdpstRedoAddSlots   sLogData;

    IDE_DASSERT( aMtx          != NULL );
    IDE_DASSERT( aBMPHdr       != NULL );
    IDE_DASSERT( aNeedToChangeMFNL != NULL );
    IDE_DASSERT( aNewMFNL      != NULL );

    // ������ ���� slot ������ �� ���� ����� slot��ġ�� ���Ѵ�.
    sLstSlotNo = aBMPHdr->mSlotCnt;

    if ( aBMPHdr->mMaxSlotCnt <= sLstSlotNo )
    {
        ideLog::log( IDE_SERVER_0,
                     "MaxSlotCnt: %u, "
                     "LstSlotNo : %d\n",
                     aBMPHdr->mMaxSlotCnt,
                     sLstSlotNo );
        (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
        IDE_ASSERT( 0 );
    }


    addSlots( aBMPHdr, sLstSlotNo, aFstBMP, aLstBMP, aFullBMPs,
              aNeedToChangeMFNL, aNewMFNL );

    sLogData.mLstSlotNo   = sLstSlotNo;
    sLogData.mFstChildBMP = aFstBMP;
    sLogData.mLstChildBMP = aLstBMP;
    sLogData.mFullBMPCnt  = aFullBMPs;

    // ADD_SLOT logging
    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aBMPHdr,
                                         &sLogData,
                                         ID_SIZEOF( sLogData ),
                                         SDR_SDPST_ADD_SLOTS ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/***********************************************************************
 * Description : Bitmap �������� map ������ slot�� �߰��Ѵ�.
 ***********************************************************************/
void sdpstBMP::addSlotsToMap( sdpstBMPSlot    * aMapPtr,
                              SShort            aSlotNo,
                              scPageID          aFstBMP,
                              scPageID          aLstBMP,
                              UShort            aFullBMPs )
{
    UInt             sLoop;
    sdpstBMPSlot   * sSlot;
    scPageID         sCurrBMP;
    UShort           sFullBMPs;

    IDE_ASSERT( aMapPtr != NULL );

    sFullBMPs = aFullBMPs;

    for( sLoop = 0, sCurrBMP = aFstBMP;
         sCurrBMP <= aLstBMP;
         sLoop++, sCurrBMP++ )
    {
        sSlot = &(aMapPtr[ aSlotNo + sLoop ]);

        sSlot->mBMP = sCurrBMP;
        if ( sFullBMPs >  0 )
        {
            sSlot->mMFNL  = SDPST_MFNL_FUL;
            sFullBMPs--;
        }
        else
        {
            sSlot->mMFNL  = SDPST_MFNL_FMT;
        }
    }
}

/***********************************************************************
 * Description : �������� MFNL ���� �� BMP ��ǥ MFNL �����Ͽ� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdpstBMP::logAndUpdateMFNL( sdrMtx            * aMtx,
                                   sdpstBMPHdr       * aBMPHdr,
                                   SShort              aFmSlotNo,
                                   SShort              aToSlotNo,
                                   sdpstMFNL           aOldSlotMFNL,
                                   sdpstMFNL           aNewSlotMFNL,
                                   UShort              aPageCount,
                                   sdpstMFNL         * aNewBMPMFNL,
                                   idBool            * aNeedToChangeMFNL )
{
    sdpstRedoUpdateMFNL sLogData;

    updateMFNL( aBMPHdr,
                aFmSlotNo,
                aToSlotNo,
                aOldSlotMFNL,
                aNewSlotMFNL,
                aPageCount,
                aNewBMPMFNL,
                aNeedToChangeMFNL );

    sLogData.mFmMFNL        = aOldSlotMFNL;
    sLogData.mToMFNL        = aNewSlotMFNL;
    sLogData.mPageCnt       = aPageCount;
    sLogData.mFmSlotNo      = aFmSlotNo;
    sLogData.mToSlotNo      = aToSlotNo;

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aBMPHdr,
                                         &sLogData,
                                         ID_SIZEOF(sLogData),
                                         SDR_SDPST_UPDATE_MFNL )
            != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}



/***********************************************************************
 * Description : �������� MFNL ���� �� BMP ��ǥ MFNL �����Ͽ� �����Ѵ�.
 *
 * aBMPHdr           - [IN] BMP Header
 * aFmSlotNo         - [IN] From SlotNo
 * aToSlotNo         - [IN] To SlotNo
 * aOldSlotMFNL      - [IN] ������ MFNL
 * aNewSlotMFNL      - [IN] ������ MFNL
 * aPageCount        - [IN] ������ CHILD ������ ����
 * aNewBMPMFNL       - [OUT] ����� �ش� BMP�� MFNL
 * aNeedToChangeMFNL - [OUT] MFNL ���� ����
 ***********************************************************************/
void sdpstBMP::updateMFNL( sdpstBMPHdr       * aBMPHdr,
                           SShort              aFmSlotNo,
                           SShort              aToSlotNo,
                           sdpstMFNL           aOldSlotMFNL,
                           sdpstMFNL           aNewSlotMFNL,
                           UShort              aPageCount,
                           sdpstMFNL         * aNewBMPMFNL,
                           idBool            * aNeedToChangeMFNL )
{
    sdpstMFNL       sNewBMPMFNL;
    sdpstBMPSlot  * sSlotPtr;
    UShort          sLoop;

    IDE_DASSERT( aBMPHdr   != NULL );
    IDE_DASSERT( aPageCount > 0 );

    if ( aBMPHdr->mMFNLTbl[aOldSlotMFNL] < aPageCount )
    {
        ideLog::log( IDE_SERVER_0,
                     "MFNL Table[UNF]: %u, "
                     "MFNL Table[FMT]: %u, "
                     "MFNL Table[INS]: %u, "
                     "MFNL Table[FUL]: %u, "
                     "Page Count     : %u\n",
                     aBMPHdr->mMFNLTbl[SDPST_MFNL_UNF],
                     aBMPHdr->mMFNLTbl[SDPST_MFNL_FMT],
                     aBMPHdr->mMFNLTbl[SDPST_MFNL_INS],
                     aBMPHdr->mMFNLTbl[SDPST_MFNL_FUL],
                     aPageCount );
        (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
        IDE_ASSERT( 0 );
    }

    /* Slot MFNL �� �����Ѵ�. ��, Lf-BMP������ �������� �ʴ´�. (���⶧��) */
    if ( (aBMPHdr->mType == SDPST_RTBMP) || (aBMPHdr->mType == SDPST_ITBMP) )
    {
        for ( sLoop = aFmSlotNo; sLoop <= aToSlotNo; sLoop++ )
        {
            sSlotPtr = getSlot( aBMPHdr, sLoop );
            sSlotPtr->mMFNL = aNewSlotMFNL;
        }
    }

    /* MFNL Table �� �����Ѵ�. */
    aBMPHdr->mMFNLTbl[aOldSlotMFNL] -= aPageCount;
    aBMPHdr->mMFNLTbl[aNewSlotMFNL] += aPageCount;

    /* BMP�� ��ǥ MFNL�� �����Ͽ� �����Ѵ�. */
    sNewBMPMFNL = sdpstAllocPage::calcMFNL( aBMPHdr->mMFNLTbl );
    if ( aBMPHdr->mMFNL != sNewBMPMFNL )
    {
        aBMPHdr->mMFNL = sNewBMPMFNL;
        *aNeedToChangeMFNL = ID_TRUE;
    }
    else
    {
        *aNeedToChangeMFNL = ID_FALSE;
    }

    *aNewBMPMFNL = sNewBMPMFNL;
}

/***********************************************************************
 * Description : 
 ***********************************************************************/
IDE_RC sdpstBMP::tryToChangeMFNL( idvSQL          * aStatistics,
                                  sdrMtx          * aMtx,
                                  scSpaceID         aSpaceID,
                                  scPageID          aCurBMP,
                                  SShort            aSlotNo,
                                  sdpstMFNL         aNewSlotMFNL,
                                  idBool          * aNeedToChangeMFNL,
                                  sdpstMFNL       * aNewMFNL,
                                  scPageID        * aParentBMP,
                                  SShort          * aSlotNoInParent )
{
    UChar               * sPagePtr;
    sdpstBMPHdr         * sBMPHdr;
    sdpstBMPSlot        * sSlotPtr;

    IDE_DASSERT( aMtx            != NULL );
    IDE_DASSERT( aCurBMP         != SD_NULL_PID );
    IDE_DASSERT( aSlotNo         != SDPST_INVALID_SLOTNO );
    IDE_DASSERT( aNeedToChangeMFNL   != NULL );
    IDE_DASSERT( aNewMFNL        != NULL );
    IDE_DASSERT( aParentBMP      != NULL );
    IDE_DASSERT( aSlotNoInParent != NULL );
   
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aCurBMP,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sBMPHdr = getHdrPtr( sPagePtr );

    sSlotPtr = getSlot( sBMPHdr, aSlotNo );

    IDE_TEST( logAndUpdateMFNL( aMtx,
                                sBMPHdr,
                                aSlotNo,
                                aSlotNo,
                                sSlotPtr->mMFNL, /* old slot MFNL */
                                aNewSlotMFNL,
                                1,
                                aNewMFNL,
                                aNeedToChangeMFNL ) != IDE_SUCCESS );

    IDE_DASSERT( sdpstBMP::verifyBMP( sBMPHdr ) == IDE_SUCCESS );

    *aParentBMP      = sBMPHdr->mParentInfo.mParentPID;
    *aSlotNoInParent = sBMPHdr->mParentInfo.mIdxInParent;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC sdpstBMP::verifyBMP( sdpstBMPHdr  * aBMPHdr )
{
    UShort             sLoop;
    UShort             sSlotCnt;
    sdpstMFNL          sMFNL;
    UShort             sMFNLtbl[ SDPST_MFNL_MAX ];
    sdpstBMPSlot     * sSlotPtr;

    sSlotCnt       = aBMPHdr->mSlotCnt;
    sSlotPtr       = getMapPtr(aBMPHdr);

    idlOS::memset( &sMFNLtbl, 0x00, ID_SIZEOF(UShort) * SDPST_MFNL_MAX );

    /* MFNL Table �� ���� */
    for( sLoop = 0; sLoop < sSlotCnt; sLoop++ )
    {
        if ( sSlotPtr[sLoop].mBMP == SD_NULL_PID )
        {
            ideLog::log( IDE_SERVER_0,
                         "sLoop: %u\n",
                         sLoop );
            (void)dump( sdpPhyPage::getPageStartPtr( aBMPHdr ) );
            IDE_ASSERT( 0 );
        }

        sMFNL            = sSlotPtr[sLoop].mMFNL;
        sMFNLtbl[sMFNL] += 1;
    }

    for ( sLoop = 0; sLoop < SDPST_MFNL_MAX; sLoop++ )
    {
        if ( aBMPHdr->mMFNLTbl[ sLoop ] != sMFNLtbl[ sLoop ] )
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
                         aBMPHdr->mMFNLTbl[SDPST_MFNL_FUL],
                         aBMPHdr->mMFNLTbl[SDPST_MFNL_INS],
                         aBMPHdr->mMFNLTbl[SDPST_MFNL_FMT],
                         aBMPHdr->mMFNLTbl[SDPST_MFNL_UNF],
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
 * Description : Search Type�� ���� Ž���� Lf BMP �������� �ĺ������
 *               �ۼ��Ѵ�.
 *
 * ���� Slot�� Ž������� Ž���� ���� �ְ�, ���� �������� Ž���������
 * Ž���Ҽ��� �ִ�.
 ***********************************************************************/
void sdpstBMP::makeCandidateChild( sdpstSegCache    * aSegCache,
                                   UChar            * aPagePtr,
                                   sdpstBMPType       aBMPType,
                                   sdpstSearchType    aSearchType,
                                   sdpstWM          * aHWM,
                                   void             * aCandidateArray,
                                   UInt             * aCandidateCount )
{
    sdpstBMPHdr       * sBMPHdr;
    UShort              sMaxCandidateCount;
    UShort              sCandidateCount;
    UShort              sSlotNo;
    SShort              sManualSlotNo = -1;
    sdpstMFNL           sTargetMFNL;
    SShort              sCurIdx;
    SShort              sLoop;
    SShort              sSlotCnt;
    UShort              sHintIdx;
    sdpstBMPOps       * sCurOps;

    IDE_DASSERT( aSegCache       != NULL );
    IDE_DASSERT( aPagePtr        != NULL );
    IDE_DASSERT( aCandidateArray != NULL );
    IDE_DASSERT( aBMPType        == SDPST_ITBMP ||
                 aBMPType        == SDPST_LFBMP );

    sCurOps         = mBMPOps[aBMPType];
    sTargetMFNL     = SDPST_SEARCH_TARGET_MIN_MFNL( aSearchType );
    sCandidateCount = 0;

    sBMPHdr = sCurOps->mGetBMPHdr( aPagePtr );

    if ( sdpstFindPage::isAvailable( sBMPHdr->mMFNL, 
                                     sTargetMFNL ) == ID_FALSE )
    {
        IDE_CONT( bmp_not_available );
    }

    sMaxCandidateCount = sCurOps->mGetMaxCandidateCnt();

    // Ž�� ������ġ�� Hashing �ؼ� �����Ѵ���, �ĺ� �����
    // �ִ� 10������ �ۼ��Ѵ�.
    // (������ ��Ʈ�̱� ������ Race Condition�� �߻��ص� �������)
    sHintIdx = aSegCache->mHint4CandidateChild++;

    /* BUG-33683 - [SM] in sdcRow::processOverflowData, Deadlock can occur
     * Ư�� ������ �Ҵ��� �����ϱ� ���� ������Ƽ ������ ��� �ش� ������Ƽ
     * ������ sSlotNo�� �����Ѵ�. */
    if ( aBMPType == SDPST_LFBMP )
    {
        sManualSlotNo = smuProperty::getTmsManualSlotNoInLfBMP();
    }
    else
    {
        if ( aBMPType == SDPST_ITBMP )
        {
            sManualSlotNo = smuProperty::getTmsManualSlotNoInItBMP();
        }
        else
        {
            /* nothing */
        }
    }

    if ( sManualSlotNo < 0 )
    {
        sSlotNo  = sCurOps->mGetStartSlotOrPBSNo( aPagePtr, sHintIdx );
        sSlotCnt = sCurOps->mGetChildCount( aPagePtr, aHWM );
    }
    else
    {
        sSlotNo  = (UShort)sManualSlotNo;
        sSlotCnt = sCurOps->mGetChildCount( aPagePtr, aHWM );
    }

    /* �������� �Ѿ�� ������ slot ���� */
    if ( sSlotNo > (sSlotCnt - 1) )
    {
        sSlotNo = sSlotCnt - 1;
    }

    for ( sLoop = 0, sCurIdx = sSlotNo; sLoop < sSlotCnt; sLoop++ )
    {
        if ( sCurIdx == SDPST_INVALID_SLOTNO )
        {
            ideLog::log( IDE_SERVER_0,
                         "sSlotNo: %u, "
                         "sSlotCnt: %d, "
                         "sHintIdx: %u, "
                         "sLoop: %d\n",
                         sSlotNo,
                         sSlotCnt,
                         sHintIdx,
                         sLoop );

            /* BMP Page dump */
            if ( aBMPType == SDPST_ITBMP )
            {
                (void)dump( aPagePtr );
            }
            else
            {
                if ( aBMPType == SDPST_LFBMP )
                {
                    sdpstLfBMP::dump( aPagePtr );
                }
                else
                {
                    ideLog::logMem( IDE_SERVER_0, aPagePtr, SD_PAGE_SIZE );
                }
            }

            /* SegCache dump */
            sdpstCache::dump( aSegCache );

            IDE_ASSERT( 0 );
        }

        if ( sCurOps->mIsCandidateChild( aPagePtr,
                                         sCurIdx,
                                         sTargetMFNL ) == ID_TRUE )
        {
            sCurOps->mSetCandidateArray( aPagePtr,
                                         sCurIdx,
                                         sCandidateCount,
                                         aCandidateArray );
            sCandidateCount++;
        }

        /* ��� �ĺ��� �����Ҷ����� */
        if ( sCandidateCount > sMaxCandidateCount - 1)
        {
            break;
        }

        /* ��� slot�� Ȯ�� �ؾ��ϹǷ� ������ slot����
         * Ȯ���ߴٸ� �ٽ� ù��° slot���� Ȯ���Ѵ�. */
        if ( sCurIdx == (sSlotCnt - 1) )
        {
            sCurIdx = 0;
        }
        else
        {
            if ( sCurIdx >= (sSlotCnt - 1) )
            {
                ideLog::log( IDE_SERVER_0,
                             "sSlotNo: %u, "
                             "sSlotCnt: %d, "
                             "sHintIdx: %u, "
                             "sLoop: %d\n",
                             sSlotNo,
                             sSlotCnt,
                             sHintIdx,
                             sLoop );

                /* BMP Page dump */
                if ( aBMPType == SDPST_ITBMP )
                {
                    (void)dump( aPagePtr );
                }
                else
                {
                    if ( aBMPType == SDPST_LFBMP )
                    {
                        sdpstLfBMP::dump( aPagePtr );
                    }
                    else
                    {
                        ideLog::logMem( IDE_SERVER_0, aPagePtr, SD_PAGE_SIZE );
                    }
                }

                /* SegCache dump */
                sdpstCache::dump( aSegCache );

                IDE_ASSERT( 0 );
            }
            sCurIdx++;
        }
    }

    IDE_EXCEPTION_CONT( bmp_not_available );

    *aCandidateCount = sCandidateCount;
}

/***********************************************************************
 * Description : RtBMP/ItBMP Header�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstBMP::dumpHdr( UChar    * aPagePtr,
                          SChar    * aOutBuf,
                          UInt       aOutSize )
{
    UChar         * sPagePtr;
    sdpstBMPHdr   * sBMPHdr;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );
    sBMPHdr  = getHdrPtr( sPagePtr );

    /* BMP Header */
    idlOS::snprintf(
                 aOutBuf,
                 aOutSize,
                 "--------------- BMP Header Begin ----------------\n"
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
                 "---------------  BMP Header End  ----------------\n",
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
                 sBMPHdr->mBodyOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RtBMP/ItBMP Body�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstBMP::dumpBody( UChar    * aPagePtr,
                           SChar    * aOutBuf,
                           UInt       aOutSize )
{
    UChar         * sPagePtr;
    sdpstBMPHdr   * sBMPHdr;
    sdpstBMPSlot  * sSlotPtr;
    SShort          sSlotNo;

    IDE_ERROR( aPagePtr != NULL );

    sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );
    sBMPHdr  = getHdrPtr( sPagePtr );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "--------------- BMP Body Begin ----------------\n" );

    /* BMP Body */
    for ( sSlotNo = 0; sSlotNo < sBMPHdr->mSlotCnt; sSlotNo++ )
    {
        sSlotPtr = getSlot( sBMPHdr, sSlotNo );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "[%"ID_INT32_FMT"] "
                             "BMP: %"ID_UINT32_FMT", "
                             "MFNL: %"ID_UINT32_FMT"\n",
                             sSlotNo,
                             sSlotPtr->mBMP,
                             sSlotPtr->mMFNL );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "---------------- BMP Body End -----------------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : RtBMP/ItBMP Header�� dump�Ѵ�.
 ***********************************************************************/
IDE_RC sdpstBMP::dump( UChar    * aPagePtr )
{
    UChar         * sPagePtr;
    SChar         * sDumpBuf;

    IDE_ERROR( aPagePtr != NULL );

    sPagePtr = sdpPhyPage::getPageStartPtr( aPagePtr );

    ideLog::log( IDE_SERVER_0,
                 "========================================================\n" );

    /* Physical Page */
    (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                 sPagePtr,
                                 "Physical Page:" );

    if( iduMemMgr::calloc(
            IDU_MEM_SM_SDP, 1,
            ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
            (void**)&sDumpBuf ) == IDE_SUCCESS )
    {
        /* BMP Header */
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
                 "========================================================\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
