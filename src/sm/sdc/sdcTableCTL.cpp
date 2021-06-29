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
 * $Id$
 **********************************************************************/

# include <idl.h>

# include <smxTrans.h>
# include <smxTransMgr.h>

# include <sdpPhyPage.h>

# include <sdcRow.h>
# include <sdcTableCTL.h>


/**********************************************************************
 *
 * Description : ������ ������ Touched Transaction Layer �ʱ�ȭ
 *
 * Touche Transaction Slot�� �����ϴ� ������ �����Ѵ�.
 * ��Ÿ ��ü���� ������ �� CTL ������⿡�� �����ϸ�,
 * �� ��⿡���� �������� ������ ���� ó������ �����Ѵ�.
 *
 * ������ �����󿡼� ���� CTL ������ Logical Header ������ ��ġ�Ѵ�.
 * CTL ������ �ڵ�Ȯ���� ������ �����̴�.
 *
 * Logical Header ������ ��ġ�ϱ� ������ 8����Ʈ Align �Ǹ�,
 * Header �κе� 8 ����Ʈ Align ��Ų��.
 *
 * aStatistics - [IN] �������
 * aPageHdrPtr - [IN] ������ ��� ���� ������
 * aInitTrans  - [IN] ���������� CTS �ʱ� ����
 * aMaxTrans   - [IN] ���������� CTS �ִ� ����
 *
 **********************************************************************/
IDE_RC sdcTableCTL::logAndInit( sdrMtx         * aMtx,
                                sdpPhyPageHdr  * aPageHdrPtr,
                                UChar            aInitTrans,
                                UChar            aMaxTrans )
{
    IDE_ERROR( aMtx        != NULL );
    IDE_ERROR( aPageHdrPtr != NULL );
    IDE_ERROR( aMaxTrans  <= SDP_CTS_MAX_CNT );

    init( aPageHdrPtr, aInitTrans, aMaxTrans );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPageHdrPtr,
                                         NULL,
                                         ID_SIZEOF( aInitTrans ) +
                                         ID_SIZEOF( aMaxTrans ),
                                         SDR_SDC_INIT_CTL )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&(aInitTrans),
                                   ID_SIZEOF(aInitTrans) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&(aMaxTrans),
                                   ID_SIZEOF(aMaxTrans) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Touched Transaction Layer �ʱ�ȭ
 *
 * �������� Touched Transaction Layer�� �ʱ�ȭ�ϸ鼭
 * sdpPhyPageHdr�� FreeSize �� ���� Offset�� �����Ѵ�.
 *
 * Physical Page�� �ʱ� CTS ������ŭ CTL Ȯ���� ��û�Ѵ�.
 *
 * �ִ� CTS ������ ������ ���������� �����Ǿ�����.
 *
 * aPageHdrPtr - [IN] ������ ��� ���� ������
 * aInitTrans  - [IN] ���������� CTS �ʱ� ����
 * aMaxTrans   - [IN] ���������� CTS �ִ� ����
 *
 ***********************************************************************/
void sdcTableCTL::init( sdpPhyPageHdr   * aPageHdrPtr,
                        UChar             aInitTrans,
                        UChar             aMaxTrans )
{
    sdpCTL   * sCTL;
    UChar      sCTSlotIdx;
    idBool     sTrySuccess = ID_FALSE;

    IDE_ASSERT( aPageHdrPtr != NULL );
    IDE_ASSERT( idlOS::align8( ID_SIZEOF( sdpCTL ) )
                == ID_SIZEOF( sdpCTL ) );

    sdpPhyPage::initCTL( aPageHdrPtr,
                         ID_SIZEOF( sdpCTL ),
                         (UChar**)&sCTL );

    sCTL->mMaxCTSCnt     = aMaxTrans;
    sCTL->mTotCTSCnt     = 0;
    sCTL->mBindCTSCnt    = 0;
    sCTL->mDelRowCnt     = 0;
    sCTL->mCandAgingCnt  = 0;
    sCTL->mRowBindCTSCnt = 0;

    /*
     * aSCN4Aging�� Self-Aging�� ������ SCN�� ��Ÿ����, Ȯ���ϰ�
     * Self-Aging�� �� �� �ִ� Row Piece ���� �������� �ʰų� ���
     * ������ �� �𸣴� ��쿡�� ���Ѵ밪�� ������.
     */
    SM_SET_SCN_INFINITE( &(sCTL->mSCN4Aging) );

    if ( aInitTrans > 0 )
    {
       IDE_ASSERT( extend( aPageHdrPtr, aInitTrans, &sCTSlotIdx, &sTrySuccess ) 
                   == IDE_SUCCESS);

       /* ������ �������� CTL Ȯ���� �ݵ�� �����Ѵ�. �ֳ��ϸ� ������� �����ϱ� */
       IDE_ASSERT( sTrySuccess == ID_TRUE );
    }
}


/**********************************************************************
 * Description : ������ ������ Touched Transaction Layer �ʱ�ȭ
 *
 * Touche Transaction Slot�� �����ϴ� ������ �����Ѵ�.
 * ��Ÿ ��ü���� ������ �� CTL ������⿡�� �����ϸ�,
 * �� ��⿡���� �������� ������ ���� ó������ �����Ѵ�.

 * ������ �����󿡼� ���� CTL ������ Logical Header ������ ��ġ�Ѵ�.
 * CTL ������ �ڵ�Ȯ���� ������ �����̴�.
 *
 * Logical Header ������ ��ġ�ϱ� ������ 8����Ʈ Align �Ǹ�,
 * Header �κе� 8 ����Ʈ Align ��Ų��.
 *
 * aStatistics    - [IN]  �������
 * aMtx           - [IN]  mtx ������
 * aExtendSlotCnt - [IN]  Ȯ���� CTS ����
 * aCTSlotIdx     - [OUT] Ȯ��� ��ġ�� CTS ��ȣ
 * aTrySuccess    - [OUT] Ȯ�强������
 *
 **********************************************************************/
IDE_RC sdcTableCTL::logAndExtend( sdrMtx         * aMtx,
                                  sdpPhyPageHdr  * aPageHdrPtr,
                                  UChar            aExtendSlotCnt,
                                  UChar          * aCTSlotIdx,
                                  idBool         * aTrySuccess )
{
    UChar    sCTSlotIdx;
    idBool   sTrySuccess = ID_FALSE;

    IDE_ERROR( aMtx        != NULL );
    IDE_ERROR( aPageHdrPtr != NULL );

    IDE_ERROR( extend( aPageHdrPtr,
                       aExtendSlotCnt,
                       &sCTSlotIdx,
                       &sTrySuccess )
               == IDE_SUCCESS );

    IDE_TEST_CONT( sTrySuccess != ID_TRUE, CONT_ERR_EXTEND );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPageHdrPtr,
                                         NULL,
                                         ID_SIZEOF( aExtendSlotCnt ),
                                         SDR_SDC_EXTEND_CTL )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&aExtendSlotCnt,
                                   ID_SIZEOF( aExtendSlotCnt ))
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( CONT_ERR_EXTEND );

    *aTrySuccess = sTrySuccess;
    *aCTSlotIdx  = sCTSlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aCTSlotIdx  = SDP_CTS_IDX_NULL;
    *aTrySuccess = ID_FALSE;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Touched Transaction Layer Ȯ��
 *
 * Data �������� �־��� ������ŭ CTS�� Ȯ���Ѵ�. ���� Ȯ���� ������ Max CTS ������
 * Align ���� �ʴ� ���� ������ ���� �ִ�. ������� ���� 119���� �ִµ� 2���� ��
 * Ȯ���ؾ��ϴ� ��� max 120����� Error�� �߻��Ѵ�.
 * ( ����� Index�� 2���� �����ϰ�, Data �������� 1���� �����Ѵ�.)
 * Data Page������ ���� ��� ������ ���� ����.
 *
 * aPageHdrPtr     - [IN]  Data ������ ���
 * aExtendSlotCnt  - [IN]  Ȯ���� CTS ����
 * aCTSlotIdx      - [OUT] Ȯ��� ��ġ�� CTS ��ȣ
 * aTrySuccess     - [OUT] Ȯ�强������
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::extend( sdpPhyPageHdr   * aPageHdrPtr,
                            UChar             aExtendSlotCnt,
                            UChar           * aCTSlotIdx,
                            idBool          * aTrySuccess )
{
    UChar    i;
    sdpCTS * sCTS;
    sdpCTL * sCTL;
    UChar    sCTSlotIdx;
    idBool   sTrySuccess = ID_FALSE;

    IDE_ASSERT( aPageHdrPtr != NULL );
    IDE_ASSERT( idlOS::align8(ID_SIZEOF(sdpCTS)) == ID_SIZEOF(sdpCTS) );

    sCTSlotIdx = SDP_CTS_IDX_NULL;
    sCTL       = (sdpCTL*)sdpPhyPage::getCTLStartPtr( (UChar*)aPageHdrPtr );

    if ( (sCTL->mTotCTSCnt + aExtendSlotCnt) <= sCTL->mMaxCTSCnt )
    {
        IDE_ERROR( sdpPhyPage::extendCTL( aPageHdrPtr,
                                          aExtendSlotCnt,
                                          ID_SIZEOF( sdpCTS ),
                                          (UChar**)&sCTS,
                                          &sTrySuccess )
                  == IDE_SUCCESS );

        if ( sTrySuccess == ID_TRUE )
        {
            for ( i = 0; i < aExtendSlotCnt; i++ )
            {
                sCTS->mTSSPageID     = SD_NULL_PID;
                sCTS->mTSSlotNum     = 0;
                sCTS->mFSCredit      = 0;
                /* Ȯ�����ķ� �ѹ��� �Ҵ������ ���� ���� */
                sCTS->mStat          = SDP_CTS_STAT_NUL;
                sCTS->mRefCnt        = 0;
                sCTS->mRefRowSlotNum[0] = SC_NULL_SLOTNUM;
                sCTS->mRefRowSlotNum[1] = SC_NULL_SLOTNUM;

                /* �Ҵ���� ���� ��쿡�� mFSCNOrCSCN�� 0���� �ʱ�ȭ���ش�. */
                SM_INIT_SCN( &sCTS->mFSCNOrCSCN );

                sCTS++;
            }

            sCTSlotIdx        = sCTL->mTotCTSCnt;
            sCTL->mTotCTSCnt += aExtendSlotCnt;
        }
    }
    else
    {
        sTrySuccess = ID_FALSE;
    }

    *aCTSlotIdx  = sCTSlotIdx;
    *aTrySuccess = sTrySuccess;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : CTS �Ҵ� �� Set Dirty ( Wrapper-Function )
 *
 * ������ mtx�� �α��� �����ϸ鼭, �������� ������ CTS�� �Ҵ��Ѵ�.
 * ������� FixMtx�� �α��� �Ǿ� �ִ°�쿡�� ����Ÿ �������� ���� Latch�� �����ߴٰ�
 * �ٽ� ���� �� �����Ƿ� ������ Mtx�� ó���Ѵ�.
 * �̿� �ٸ� ���� Data �������� ������ �Բ� CTS�� �Ҵ��ϴ� ����ε�, �� ����
 * ������ Mtx�� ����� �� ����.
 *
 * aStatistics         - [IN]  �������
 * aFixMtx             - [IN]  ����Ÿ�������� X-Latch�� ȹ���� mtx ������
 * aStartInfo          - [IN]  ������ Logging�� Mtx�� ���� mtx ��������
 * aPageHdrPtr         - [IN]  ����Ÿ������ ��� ���� ������
 * aCTSlotIdx          - [OUT] �Ҵ�� CTS ��ȣ
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::allocCTSAndSetDirty( idvSQL          * aStatistics,
                                         sdrMtx          * aFixMtx,
                                         sdrMtxStartInfo * aStartInfo,
                                         sdpPhyPageHdr   * aPageHdrPtr,
                                         UChar           * aCTSlotIdx )
{
    sdrMtx          sLogMtx;
    UInt            sState = 0;

    IDE_ERROR( aPageHdrPtr != NULL );
    IDE_ERROR( aCTSlotIdx  != NULL );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sLogMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( allocCTS( aStatistics,
                        aFixMtx,
                        &sLogMtx,
                        SDB_SINGLE_PAGE_READ,
                        aPageHdrPtr,
                        aCTSlotIdx ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sLogMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sLogMtx ) == IDE_SUCCESS );
    }

    *aCTSlotIdx = SDP_CTS_IDX_NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : CTS �Ҵ�
 *
 * ���������� CTS�� �ִ� CTS �������� Ȯ���� ���������� ��·�ų� ������ �ڿ��̴�.
 * CTS �Ҵ� �˰����� �ִ��� Ȯ���� �����ϴ� ����� �����ϸ�, �ִ��� �ʱ�ȭ�Ǿ���
 * CTS�� ��Ե� �����Ұ��� ����Ѵ�.
 *
 * ������ ���� �켱������ CTS�� �Ҵ��Ѵ�.
 *
 * 1. CTS TimeStamping �Ǿ� �ִ� CTS�� Row TimeStamping��
 *    �����ϰ�, CTS�� ���밡���ϰ� �Ѵ�
 *    ��� �ѹ� TotCTSCnt ������ŭ ��� Ȯ���Ѵ�. �߰��� �Ҵ簡���ϴٰ� Ȯ�ΰ�����
 *    �������� �ʴ´�.
 *    Delayed Row Stamping Ȯ�ΰ����� ������ CTS���� Cache �صд�.
 *    ��ư ��� Ȯ���Ŀ� Row Stamping�� ���� ���� ��ȣ�� ���� CTS�� �Ҵ��Ѵ�.
 *
 * 2. Delayed Row Stamping���� Ȯ���غ� CTS���� Ȯ���Ͽ�
 *    1�����̶� �Ҵ簡���ϴٸ� Ȯ�ΰ����� �����ϰ� �Ҵ��Ѵ�.
 *
 * 3. �̹� �ʱ�ȭ�� NULL ������ CTS�� �����ϴٸ�, �Ҵ��Ѵ�.
 *
 * 4. CTL�� Ȯ���� ���ؼ� ���Ӱ� �ʱ�ȭ�� CTS�� �Ҵ��ذ���.
 *
 * aStatistics         - [IN]  �������
 * aFixMtx             - [IN]  �������� X-Latch�� fix�� Mtx
 * aLogMtx             - [IN]  Logging�� ���� Mtx
 * aPageReadMode       - [IN]  page read mode(SPR or MPR)
 * aPageHdrPtr         - [IN]  ����Ÿ������ ��� ���� ������
 * aCTSlotIdx          - [OUT] �Ҵ�� CTS ��ȣ
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::allocCTS( idvSQL          * aStatistics,
                              sdrMtx          * aFixMtx,
                              sdrMtx          * aLogMtx,
                              sdbPageReadMode   aPageReadMode,
                              sdpPhyPageHdr   * aPageHdrPtr,
                              UChar           * aCTSlotIdx )
{
    sdpCTL        * sCTL;
    sdpCTS        * sCTS;
    UChar           sCTSIdx;
    UChar           sTotCnt;
    UChar           sAllocCTSIdx;
    sdSID           sTransTSSlotSID;
    smSCN           sFstDskViewSCN;
    UChar           sFstNullCTSIdx;
    idBool          sIsNeedSetDirty = ID_FALSE;
    UChar           sActCTSCnt;
    idBool          sTrySuccess = ID_FALSE;
    UChar           sArrActCTSIdx[ SDP_CTS_MAX_CNT ];
    smTID           sWait4TransID;
    smSCN           sRowCommitSCN;

    IDE_ERROR( aLogMtx     != NULL );
    IDE_ERROR( aPageHdrPtr != NULL );
    IDE_ERROR( aCTSlotIdx  != NULL );

    sTransTSSlotSID = smxTrans::getTSSlotSID( aLogMtx->mTrans );
    sFstDskViewSCN  = smxTrans::getFstDskViewSCN( aLogMtx->mTrans );

    sFstNullCTSIdx  = SDP_CTS_IDX_NULL;
    sAllocCTSIdx    = SDP_CTS_IDX_NULL;

    sCTL    = getCTL( aPageHdrPtr );
    sTotCnt = getCnt( sCTL );

    IDE_TEST_CONT( sTotCnt == 0, cont_skip_allocate_cts );

    /*
     * 1. CTS TimeStamping �Ǿ� �ִ� CTS�� Row TimeStamping��
     *    �����ϰ�, CTS�� ���밡���ϰ� �Ѵ�.
     */
    for( sCTSIdx = 0, sActCTSCnt = 0; sCTSIdx < sTotCnt; sCTSIdx++ )
    {
        sCTS = getCTS( sCTL, sCTSIdx );

        // 'A', 'R', 'T', 'N', 'O'
        if ( hasState( sCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE )
        {
            if ( isMyTrans( &sTransTSSlotSID,
                            &sFstDskViewSCN,
                            sCTS ) == ID_TRUE )
            {
                // 'A'������ ��� �ڽ��� �̹� ���ε��� CTS�� ã���� �ٷ� �Ҵ��Ѵ�.
                sAllocCTSIdx = sCTSIdx;
                break;
            }

            sArrActCTSIdx[sActCTSCnt++] = sCTSIdx;
        }
        else
        {
            // 'R', 'T', 'N', 'O'
            if ( hasState( sCTS->mStat, SDP_CTS_STAT_NUL ) == ID_TRUE )
            {
                // ���ķ� ��� NUL ������ ���̱� ������ ���⼭
                // CTS Ȯ�ΰ����� �Ϸ��Ѵ�.
                sFstNullCTSIdx = sCTSIdx;
                break;
            }

            // 'R', 'T', 'O'
            if ( hasState( sCTS->mStat, SDP_CTS_STAT_CTS ) == ID_TRUE )
            {
                // Soft Row Stamping 'T' -> 'R'
                IDE_TEST( logAndRunRowStamping( aLogMtx,
                                                sCTSIdx,
                                                (void*)sCTS,
                                                sCTS->mFSCredit,
                                                &sCTS->mFSCNOrCSCN )
                          != IDE_SUCCESS );

                sIsNeedSetDirty = ID_TRUE;
            }
            else
            {
                // 'R', 'O' ���¸� �����ϴ�
            }

            // RTS �� CTS �� ���� ���� CTS�� �����Ѵ�.
            if ( sAllocCTSIdx == SDP_CTS_IDX_NULL )
            {
                sAllocCTSIdx = sCTSIdx;
            }
        }
    }

    IDE_TEST_CONT( sAllocCTSIdx != SDP_CTS_IDX_NULL,
                    cont_success_allocate_cts );

    /*
     * 2. Delayed Row TimeStamping�� �õ��غ� CTS�� �����ϴ� ���
     */
    for( sCTSIdx = 0; sCTSIdx < sActCTSCnt; sCTSIdx++ )
    {
        sCTS = getCTS( sCTL, sArrActCTSIdx[ sCTSIdx ] );

        IDE_ERROR( hasState( sCTS->mStat, SDP_CTS_STAT_ACT )
                    == ID_TRUE );

        // 'A' -> 'R' ���·� ����õ�
        IDE_TEST( logAndRunDelayedRowStamping(
                                  aStatistics,
                                  aLogMtx,
                                  sArrActCTSIdx[ sCTSIdx ],
                                  (void*)sCTS,
                                  aPageReadMode,
                                  &sWait4TransID,  
                                  &sRowCommitSCN ) != IDE_SUCCESS );

        // Row TimeStamping�� �Ϸ�Ǿ����Ƿ� �Ҵ�
        if ( hasState( sCTS->mStat, SDP_CTS_STAT_RTS ) == ID_TRUE )
        {
            sAllocCTSIdx    = sArrActCTSIdx[ sCTSIdx ];
            sIsNeedSetDirty = ID_TRUE;
            break;
        }
    }

    IDE_TEST_CONT( sAllocCTSIdx != SDP_CTS_IDX_NULL,
                    cont_success_allocate_cts );

    /*
     * 3. ������ ������ ���� �������� �ʴ´ٸ� NULL ������ CTS�� ����Ѵ�.
     */
    if ( sFstNullCTSIdx != SDP_CTS_IDX_NULL )
    {
        IDE_ERROR( sTotCnt > sFstNullCTSIdx );

        sAllocCTSIdx = sFstNullCTSIdx;
        IDE_CONT( cont_success_allocate_cts );
    }

    /*
     * 4. CTL Ȯ���� ���Ͽ� CTS�� Ȯ���Ѵ�.
     *
     *    CTS�� ��� �˻��ϰ� ������ CTS�� �������� �ʴ´ٸ�,
     *    CTL ������ Ȯ���Ѵ�.
     */
    IDE_TEST( logAndExtend( aLogMtx,
                            aPageHdrPtr,
                            1,       /* aCTSCnt */
                            &sAllocCTSIdx,
                            &sTrySuccess ) != IDE_SUCCESS );

    if ( sTrySuccess == ID_TRUE )
    {
        sIsNeedSetDirty = ID_TRUE;
    }

    IDE_EXCEPTION_CONT( cont_success_allocate_cts );

    /* To Fix BUG-23241 [F1-valgrind] Insert�� AllocCTS��������
     * RowTimeStamping �� �� SetDirtyPage�� �������� �ʾ�
     * ��������������� ��������
     * Page�� �����ϰ� SetDirtyPage�� ���� ������, �α�LSN��
     * BCB�� �ݿ����� �ʾƼ� PageLSN�� �������� �ʴ´�.
     * insert �ÿ��� aFixMtx == NULL �̸�, Logging�� aLogMtx�� ������
     * ����ϱ� ������ setDirtyPage�� ���־�� �Ѵ�.
     * ������, �׿� ���� update/delete ������ aFixMtx == aLogMtx�̸�,
     * x-latch�� ȹ���ϰ� �������� ������ setdirty�� ���� �ʿ���� */
    if ( (sIsNeedSetDirty == ID_TRUE) && (aFixMtx != aLogMtx) )
    {
        IDE_TEST( sdrMiniTrans::setDirtyPage( aLogMtx, (UChar*)aPageHdrPtr )
                  != IDE_SUCCESS );
    }

    IDE_EXCEPTION_CONT( cont_skip_allocate_cts );

    *aCTSlotIdx = sAllocCTSIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aCTSlotIdx = SDP_CTS_IDX_NULL;

    if ( (sIsNeedSetDirty == ID_TRUE) && (aFixMtx != aLogMtx) )
    {
       (void)sdrMiniTrans::setDirtyPage( aLogMtx, (UChar*)aPageHdrPtr );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : �ڽ��� ���ε��� CTS���� �Ǵ��Ѵ�.
 *
 * ������ ���� CTS�� ��ϵ� TSSlotSID�� TransBSCN�� ����Ͽ� Ʈ����� �ڽ���
 * ���ε��� CTS������ ��Ȯ�ϰ� �Ǵ��� �� �ִ�.
 *
 * �ϴ� ������ Ʈ������̶�� TSSlotSID�� �����ؾ�������, TSS�� ������ �����ϱ� ������
 * �ٸ� Ʈ����� ������ �����Ҽ� �ִ�.
 * �׷��� CTS�� ���ε��� Ʈ������� TransBSCN�� ���� Ʈ����ǰ� �����Ѱ͵� ���ؾ��ϴµ�
 * �̰��� TransBSCN�� ���� ��쿡�� TSS ������� �ʾҴٴ� ���� �������ֱ� �����̴�.
 *
 * aTransTSSlotSID - [IN] CTS�� ��ϵ� TSSlotSID
 * aTransBSCN      - [IN] CTS�� ��ϵ� ���ε��� Ʈ������� Begin SCN ������
 * aPagePtr        - [IN] ����Ÿ �������� ��� ���� ������
 * aCTSlotIdx      - [IN] ��� CTSlot ��ȣ
 *
 ***********************************************************************/
#if 0 // not used
idBool sdcTableCTL::isMyTrans( sdSID         * aTransTSSlotSID,
                               smSCN         * aTransBSCN,
                               sdpPhyPageHdr * aPagePtr,
                               UChar           aTSSlotIdx )
{
    sdpCTS * sCTS;

    sCTS = getCTS( aPagePtr, aTSSlotIdx );

    if ( (SD_MAKE_PID( *aTransTSSlotSID )    == sCTS->mTSSPageID) &&
         (SD_MAKE_OFFSET( *aTransTSSlotSID ) == sCTS->mTSSlotNum) )
    {
        if ( SM_SCN_IS_EQ( &(sCTS->mFSCNOrCSCN), aTransBSCN ) )
        {
            return ID_TRUE;
        }
    }

    return ID_FALSE;
}
#endif

/***********************************************************************
 *
 * Description : �ڽ��� ���ε��� CTS���� �Ǵ��Ѵ�.
 *
 * ���� sdcTableCTL::isMyTrans() �Լ��ʹ�
 * �Ű������� CTS index ��� CTS pointer�� �޴� �͸� �����ϰ� �ٸ���. 
 *
 ***********************************************************************/
idBool sdcTableCTL::isMyTrans( sdSID         * aTransTSSlotSID,
                               smSCN         * aTransBSCN,
                               sdpCTS        * aCTS )
{
    if ( ( SD_MAKE_PID( *aTransTSSlotSID )    == aCTS->mTSSPageID ) &&
         ( SD_MAKE_OFFSET( *aTransTSSlotSID ) == aCTS->mTSSlotNum ) )
    {
        if ( SM_SCN_IS_EQ( &(aCTS->mFSCNOrCSCN), aTransBSCN ) )
        {
            return ID_TRUE;
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        /* nothing to do */
    }

    return ID_FALSE;
}

/***********************************************************************
 *
 * Description : Ʈ����� ������ CTS�� ����ϰų� Row�� ���ε��Ѵ�.
 *
 * ������ ���������� CTS�� �Ҵ��� ��쿡�� CTS�� Ʈ����� ������ ���ε��ϰ�,
 * �Ҵ���� ���� ��쿡�� RowPiece���� CTS ������ ����Ѵ�.
 *
 *
 * aStatistics         - [IN]  �������
 * aMtx                - [IN]  Mtx ������
 * aSpaceID            - [IN]  ����Ÿ�������� �Ҽӵ� ���̺����̽� ID
 * aRowPiecePtr        - [IN]  ������ RowPiece �� ������
 * aRowCTSlotIdx       - [IN]  Row Piece�� CTS ��ȣ (�̹� ���ε��� Row�ϼ��� �ְ�
 *                                             �׷��� ���� ���� �ִ�. )
 * aAllocCTSlotIdx     - [IN]  Ʈ������� ������ ���� �Ҵ��� CTS ��ȣ
 * aRowSlotNum         - [IN]  CTS�� Caching�� RowPiece SlotEntry ��ȣ
 * aFSCreditSize       - [IN]  �������� ������ Free Space Credit ũ��
 * aIncDelRowCnt       - [IN]  Delete ������ ���� ���ε��� ��� TRUE,
 *                             Delete Row ������ ������Ų��.
 **********************************************************************/
IDE_RC sdcTableCTL::bind( sdrMtx               * aMtx,
                          scSpaceID              aSpaceID,
                          UChar                * aNewSlotPtr,
                          UChar                  aOldCTSlotIdx,
                          UChar                  aNewCTSlotIdx,
                          scSlotNum              aRowSlotNum,
                          UShort                 aFSCreditSize4RowHdrEx,
                          SShort                 aNewFSCreditSize,
                          idBool                 aIncDelRowCnt )
{
    UChar     sOldCTSlotIdx;
    UChar     sNewCTSlotIdx;
    SShort    sNewFSCreditSize;

    IDE_ERROR( aNewSlotPtr != NULL );

    sOldCTSlotIdx = SDC_UNMASK_CTSLOTIDX( aOldCTSlotIdx );
    sNewCTSlotIdx = SDC_UNMASK_CTSLOTIDX( aNewCTSlotIdx );

    sNewFSCreditSize = aFSCreditSize4RowHdrEx;

    if( aNewFSCreditSize > 0 )
    {
        sNewFSCreditSize += aNewFSCreditSize;
    }

    if( SDC_HAS_BOUND_CTS(aNewCTSlotIdx) )
    {
#ifdef DEBUG
        if ( sOldCTSlotIdx != sNewCTSlotIdx )
        {
            IDE_ASSERT( !SDC_HAS_BOUND_CTS(sOldCTSlotIdx) ||
                        !SDC_HAS_BOUND_CTS(sNewCTSlotIdx) );
        }
#endif

        IDE_TEST( logAndBindCTS( aMtx,
                                 aSpaceID,
                                 sdpPhyPage::getHdr(aNewSlotPtr),
                                 sOldCTSlotIdx,
                                 sNewCTSlotIdx,
                                 aRowSlotNum,
                                 aNewFSCreditSize,
                                 aIncDelRowCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ERROR( sNewCTSlotIdx  == SDP_CTS_IDX_NULL );
        IDE_ERROR( (sOldCTSlotIdx == SDP_CTS_IDX_NULL) ||
                   (sOldCTSlotIdx == SDP_CTS_IDX_UNLK) );

        IDE_TEST( logAndBindRow( aMtx,
                                 aNewSlotPtr,
                                 sNewFSCreditSize,
                                 aIncDelRowCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : CTS�� ����ε��ϰų� Ȥ�� RowPiece�� CTS ������ ����ε��Ѵ�.
 *
 * aMtx              - [IN] Mtx ������
 * aRowPieceBfrUndo  - [IN] ����ε��� RowPiece�� ������
 * aCTSlotIdxBfrUndo - [IN] ����ϱ� ���� Row Piece ����� CTS ��ȣ
 * aCTSlotIdxAftUndo - [IN] ����� �Ŀ� Row Piece ����� CTS ��ȣ
 *                          �ߺ� ������ �Ȱ��� ����ϱ����̳� �ĳ� CTS ��ȣ��
 *                          �����ϸ�, �׷��� �ʴٸ� SDP_CTS_IDX_UNLK �̾�� �Ѵ�.
 * aFSCreditSize     - [IN] �������� ������ Free Space Credit ũ��
 * aDecDelRowCnt     - [IN] Delete ������ ���� ����ε��� ��� TRUE,
 *                          Delete Row ������ ���ҽ�Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::unbind( sdrMtx         * aMtx,
                            UChar          * aRowPieceBfrUndo,
                            UChar            aCTSlotIdxBfrUndo,
                            UChar            aCTSlotIdxAftUndo,
                            SShort           aFSCreditSize,
                            idBool           aDecDelRowCnt )
{
    UChar  sCTSlotIdxBfrUndo;
    UChar  sCTSlotIdxAftUndo;

    IDE_ERROR( aRowPieceBfrUndo != NULL );

    sCTSlotIdxBfrUndo = SDC_UNMASK_CTSLOTIDX( aCTSlotIdxBfrUndo );
    sCTSlotIdxAftUndo = SDC_UNMASK_CTSLOTIDX( aCTSlotIdxAftUndo );

    if ( SDC_HAS_BOUND_CTS(sCTSlotIdxBfrUndo) )
    {
        /* ���������� CTS�� �Ҵ���� ��쿡�� RowHdr�� CTSIdx��
         * Undo �������� 0~125 ������ ��ȿ�� ���� �ü� �ִ�. */
        IDE_TEST( logAndUnbindCTS( aMtx,
                                   sdpPhyPage::getHdr(aRowPieceBfrUndo),
                                   sCTSlotIdxBfrUndo,
                                   sCTSlotIdxAftUndo,
                                   aFSCreditSize,
                                   aDecDelRowCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ERROR( sCTSlotIdxBfrUndo  == SDP_CTS_IDX_NULL );
        IDE_ERROR( (sCTSlotIdxAftUndo == SDP_CTS_IDX_NULL) ||
                   (sCTSlotIdxAftUndo == SDP_CTS_IDX_UNLK) );

        IDE_TEST( logAndUnbindRow( aMtx,
                                   aRowPieceBfrUndo,
                                   aFSCreditSize,
                                   aDecDelRowCnt )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : CTS�� ���ε��Ѵ�.
 *
 * ����Ÿ���������� ������ Ʈ������� ������ RowPiece���� �ϳ��� CTS���� ���ε� ��
 * �� ������, ���� ���� �ٸ� Row Piece�� �����ϴ� ��쿡�� CTS�� RefCnt�� ������Ű��
 * �͸����� ���ε������� �����Ѵ�. ���� ������ Row Piece�� �ߺ� ������ ��쿡�� RefCnt
 * �� ������Ű�� �ʴ´�.
 * ���� RefCnt�� �����Ҷ����� ������Ų�ٸ� �ߺ� Row Piece�� ���ؼ� ������ ���ŵǴ�
 * ��쿡 2����Ʈ�� RefCnt�� ������� ���� ���̴�.
 *
 * ���� ���ε��������� �Բ� ó���ϴ� ���� 2������ �ִ� ���ſ��꿡 ���� ����Ǿ�� ��
 * Free Space Credit�� ���� ó���� Delete�� Row�� �������� �װ��̴�.
 * �̴� ������ �α����� ���ε� �Լ��ȿ��� ó���ȴ�.
 *
 * aStatistics      - [IN] �������
 * aMtx             - [IN] Mtx ������
 * aSpaceID         - [IN] ����Ÿ�������� �Ҽӵ� ���̺����̽� ID
 * aPageHdrPtr      - [IN] ����Ÿ�������� ��� ���� ������
 * aOldCTSlotIdx    - [IN] Row Piece�� CTS ��ȣ (�̹� ���ε��� Row�ϼ��� �ְ�
 *                                             �׷��� ���� ���� �ִ�. )
 * aNewCTSlotIdx    - [IN] Ʈ������� ������ ���� �Ҵ��� CTS ��ȣ
 * aRowSlotNum      - [IN] CTS�� Caching�� RowPiece SlotEntry ��ȣ
 * aNewFSCreditSize - [IN] �������� ������ Free Space Credit ũ��
 * aIncDelRowCnt    - [IN] Delete ������ ���� ���ε��� ��� TRUE,
 *                         Delete Row ������ ������Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::logAndBindCTS( sdrMtx         * aMtx,
                                   scSpaceID        aSpaceID,
                                   sdpPhyPageHdr  * aPageHdrPtr,
                                   UChar            aOldCTSlotIdx,
                                   UChar            aNewCTSlotIdx,
                                   scSlotNum        aRowSlotNum,
                                   SShort           aNewFSCreditSize,
                                   idBool           aIncDelRowCnt )
{
    sdpCTS        * sCTS;
    idBool          sIsActiveCTS;
    UInt            sLogSize;
    sdSID           sTSSlotSID;
    smSCN           sFstDskViewSCN;

    IDE_ERROR( aPageHdrPtr != NULL );

    sTSSlotSID = SD_NULL_SID;

    SM_INIT_SCN( &sFstDskViewSCN );

    sLogSize = ID_SIZEOF(UChar)     + // aRowCTSlotIdx
               ID_SIZEOF(UChar)     + // aNewCTSlotIdx
               ID_SIZEOF(scSlotNum) + // aSlotNum
               ID_SIZEOF(SShort)    + // aFSCreditSize
               ID_SIZEOF(idBool);     // aIncDelRowCnt

    sCTS         = getCTS( aPageHdrPtr, aNewCTSlotIdx );
    sIsActiveCTS = hasState( sCTS->mStat, SDP_CTS_STAT_ACT );

    if ( sIsActiveCTS == ID_FALSE )
    {
        // �ش� ���������� Ʈ������� ó������ CTS�� ���ε��ϴ� ����̴�.
        sLogSize += (ID_SIZEOF(smSCN) + ID_SIZEOF(sdSID));

        sFstDskViewSCN  = smxTrans::getFstDskViewSCN( aMtx->mTrans );
        sTSSlotSID      = smxTrans::getTSSlotSID( aMtx->mTrans );

        IDE_TEST( ((smxTrans*)aMtx->mTrans)->addTouchedPage(
                    aSpaceID,
                    aPageHdrPtr->mPageID,
                    aNewCTSlotIdx ) != IDE_SUCCESS );
    }
    else
    {
        /* CTS�� �̹� Ʈ����ǿ� ���ε� �� ��쿡��
           RefCnt, FSCredit�� DeleteCnt�� ������Ų��. */
    }

/*
    ideLog::log(IDE_SERVER_0, "<tid %d bind> pid(%d), ctslotidx(%x,o(%x)), slotnum(%d), tss(%d,%d)\n ",
                              smxTrans::getTransID( aMtx->mTrans ),
                              aPageHdrPtr->mPageID,
                              aNewCTSlotIdx,
                              aOldCTSlotIdx,
                              aRowSlotNum,
                              SD_MAKE_PID( sTSSlotSID),
                              SD_MAKE_OFFSET( sTSSlotSID) );
*/

    IDE_TEST( bindCTS4REDO( aPageHdrPtr,
                            aOldCTSlotIdx,
                            aNewCTSlotIdx,
                            aRowSlotNum,
                            &sFstDskViewSCN,
                            sTSSlotSID,
                            aNewFSCreditSize,
                            aIncDelRowCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)sCTS,
                                        NULL,
                                        sLogSize,
                                        SDR_SDC_BIND_CTS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aOldCTSlotIdx,
                                   ID_SIZEOF( UChar ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aNewCTSlotIdx,
                                   ID_SIZEOF( UChar ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aRowSlotNum,
                                   ID_SIZEOF( scSlotNum ) )
              != IDE_SUCCESS );

    if ( sIsActiveCTS == ID_FALSE )
    {
        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       &sFstDskViewSCN,
                                       ID_SIZEOF( smSCN ) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::write( aMtx,
                                       &sTSSlotSID,
                                       ID_SIZEOF( sdSID ) )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aNewFSCreditSize,
                                   ID_SIZEOF( SShort ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aIncDelRowCnt,
                                   ID_SIZEOF( idBool ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Row�� ���ε��Ѵ�.
 *
 * Parameters:
 *  aStatistics           - [IN] �������
 *  aMtx                  - [IN] Mtx ������
 *  aNewSlotPtr           - [IN] ���ο� Rowpiece ������
 *  aFSCreditSizeToWrite  - [IN] �̹��� �������� ���� �����ؾ��� FSCreditSize
 *  aIncDelRowCnt         - [IN] Delete ������ ���� ���ε��� ��� TRUE,
 *                               Delete Row ������ ������Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::logAndBindRow( sdrMtx           * aMtx,
                                   UChar            * aNewSlotPtr,
                                   SShort             aFSCreditSizeToWrite,
                                   idBool             aIncDelRowCnt )
{
    UInt    sLogSize;
    smSCN   sFstDskViewSCN;
    sdSID   sTSSlotSID;

    IDE_ERROR( aNewSlotPtr          != NULL );
    IDE_ERROR( aFSCreditSizeToWrite >= 0 );

    sLogSize = ( ID_SIZEOF(sdSID)  +
                 ID_SIZEOF(smSCN)  +
                 ID_SIZEOF(UShort) +
                 ID_SIZEOF(idBool) );

    sFstDskViewSCN = smxTrans::getFstDskViewSCN( aMtx->mTrans );
    sTSSlotSID     = smxTrans::getTSSlotSID( aMtx->mTrans );

    IDE_TEST( bindRow4REDO( aNewSlotPtr,
                            sTSSlotSID,
                            sFstDskViewSCN,
                            aFSCreditSizeToWrite,
                            aIncDelRowCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)aNewSlotPtr,
                                        (UChar*)NULL,
                                        sLogSize,
                                        SDR_SDC_BIND_ROW )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &sTSSlotSID,
                                   ID_SIZEOF(sdSID) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &sFstDskViewSCN,
                                   ID_SIZEOF(smSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aFSCreditSizeToWrite,
                                   ID_SIZEOF(UShort) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aIncDelRowCnt,
                                   ID_SIZEOF(idBool) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ������� Row�� CTS�� ���ε��Ѵ�.
 *
 * ������ Row Piece ����� CTS ��ȣ�� ����ϴ� ������ ���ε��۾��� ����Ǿ�� ������,
 * CTS ���ε� �Լ� ȣ�� ���Ŀ� sdcRow::writeRowHdr() �Լ��� ���ؼ� ���ε��� CTS
 * ��ȣ�� Row�� ����Ѵ�. ��, �� �Լ������� CTS�� ���� ���ε� ���길 �����Ѵ�.
 *
 * aPageHdrPtr       - [IN] ����Ÿ������ ��� ���� ������
 * aOldCTSlotIdx     - [IN] ������ ���ε��� CTS ��ȣ
 * aNewCTSlotIdx     - [IN] ���ε��� CTS ��ȣ
 * aRowSlotNum       - [IN] CTS�� Caching�� RowPiece SlotEntry ��ȣ
 * aFstDskViewSCN    - [IN] Ʈ������� ù��° Disk Stmt�� ViewSCN
 * aTSSlotSID        - [IN] ���ε��� Ʈ������� TSSlot SID
 * aNewFSCreditSize  - [IN] �������� ������ Free Space Credit ũ��
 * aIncDelRowCnt     - [IN] Delete ������ ���� ���ε��� ��� TRUE,
 *                          Delete Row ������ ������Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::bindCTS4REDO( sdpPhyPageHdr * aPageHdrPtr,
                                  UChar           aOldCTSlotIdx,
                                  UChar           aNewCTSlotIdx,
                                  scSlotNum       aRowSlotNum,
                                  smSCN         * aFstDskViewSCN,
                                  sdSID           aTSSlotSID,
                                  SShort          aNewFSCreditSize,
                                  idBool          aIncDelRowCnt )
{
    sdpCTL  * sCTL;
    sdpCTS  * sCTS;
    UShort    sRefCnt;

    IDE_ERROR( aPageHdrPtr   != NULL );
    IDE_ERROR( aNewCTSlotIdx != SDP_CTS_IDX_NULL );

    sCTL = getCTL( aPageHdrPtr );
    sCTS = getCTS( aPageHdrPtr, aNewCTSlotIdx );

    // �̹� CTS�� ���ε��� Row�� �ٽ� �����ϴ� ��쿡��
    // CTS�� ���ε� ���� �ʴ´�.
    IDE_TEST_CONT( aOldCTSlotIdx == aNewCTSlotIdx,
                    cont_already_bound );

    if( hasState( sCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE )
    {
        // �̹� ���ε��Ǿ� �ִ� ��쿡�� RefCnt�� ������Ų��.
        sRefCnt = sCTS->mRefCnt;
        sRefCnt++;
    }
    else
    {
        // �ش� ���������� ���� ���ε��� ���

        // 'N', 'R', 'O'
        IDE_ERROR( hasState( sCTS->mStat, SDP_CTS_SS_FREE )
                   == ID_TRUE );

        sCTS->mStat      = SDP_CTS_STAT_ACT;
        sRefCnt          = 1;
        sCTS->mTSSPageID    = SD_MAKE_PID( aTSSlotSID );
        sCTS->mTSSlotNum = SD_MAKE_OFFSET( aTSSlotSID );

        SM_SET_SCN( &(sCTS->mFSCNOrCSCN), aFstDskViewSCN );

        sCTS->mRefRowSlotNum[0] = SC_NULL_SLOTNUM;
        sCTS->mRefRowSlotNum[1] = SC_NULL_SLOTNUM;

        /* ���ε� CTS ������ ���� ���ε��ɶ� ������Ű��, Ʈ������� �ѹ���
         * �߻��Ͽ� �������������� ������ ����ε��� �����Ҷ� ���ҽ�Ų��.
         * Ŀ�Ե� ��쿡 Row Stamping�� �߻��ϴ� ��쿡�� ���ҵȴ�. */
        incBindCTSCntOfCTL( sCTL );
    }

    /* Row Piece�� ���� Slot Entry ��ȣ�� 2�������� Cache�Ѵ�.
     * ���� ���������� �Ϲ������� 2���� Row Piece������ �����ϴ� ����
     * �Ϲ����ΰ�?? �� ���� ��Ȥ�� ���������, 2�� ������ ��찡 �ִٸ�
     * Row Stamping�ÿ� Slot Directory�� FullScan�ϴ� ��찡 ���ŵǹǷ�
     * �̷��� �ڷᱸ���� ����Ǿ���. ����� ����Ŭ���� ���� �ڷᱸ���̴�. */
    if ( sRefCnt <= SDP_CACHE_SLOT_CNT )
    {
        sCTS->mRefRowSlotNum[ sRefCnt-1 ] = aRowSlotNum;
    }

    sCTS->mRefCnt = sRefCnt;

    IDE_EXCEPTION_CONT( cont_already_bound );

    if ( aNewFSCreditSize > 0 )
    {
        incFSCreditOfCTS( sCTS, aNewFSCreditSize );
    }

    if ( aIncDelRowCnt == ID_TRUE )
    {
        incDelRowCntOfCTL( sCTL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ������� Row�� ���ε��Ѵ�.
 *
 * aNewSlotPtr               - [IN] Realloc�� ���ο� RowPiece�� ������
 * aTSSlotSID                - [IN] ���ε��� Ʈ������� TSSlot SID
 * aFstDskViewSCN            - [IN] Ʈ������� ù��° Disk Stmt�� ViewSCN
 * aFSCreditSizeToWrite      - [IN] RowExCTSInfo�� ������ FSCreditSize
 * aIncDelRowCnt             - [IN] Delete ������ ���� ���ε��� ��� TRUE,
 *                                  Delete Row ������ ������Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::bindRow4REDO( UChar      * aNewSlotPtr,
                                  sdSID        aTSSlotSID,
                                  smSCN        aFstDskViewSCN,
                                  SShort       aFSCreditSizeToWrite,
                                  idBool       aIncDelRowCnt )
{
    sdpCTL          * sCTL;
    sdcRowHdrExInfo   sRowHdrExInfo;

    IDE_ERROR( aNewSlotPtr         != NULL );
    IDE_ERROR( aFSCreditSizeToWrite >= 0 );

    SDC_INIT_ROWHDREX_INFO( &sRowHdrExInfo,
                            aTSSlotSID,
                            aFSCreditSizeToWrite,
                            aFstDskViewSCN )

    sCTL = getCTL( sdpPhyPage::getHdr(aNewSlotPtr) );
    incBindRowCTSCntOfCTL( sCTL );

    sdcRow::writeRowHdrExInfo( aNewSlotPtr, &sRowHdrExInfo );

    if ( aIncDelRowCnt == ID_TRUE )
    {
        incDelRowCntOfCTL( sCTL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : CTS ����̵�
 *
 * Ʈ������� �ѹ��� �Ǹ� ���ε��� CTS�� ����ε��ϸ鼭 ����Ÿ�������� �����ߴ�
 * Row Piece�� ���� Undo�� �����Ѵ�.
 *
 * aMtx              - [IN] Mtx ������
 * aPageHdrPtr       - [IN] ����Ÿ�������� ��� ���� ������
 * aCTSlotIdxBfrUndo - [IN] ����ϱ� ���� Row Piece ����� CTS ��ȣ
 * aCTSlotIdxAftUndo - [IN] ����� �Ŀ� Row Piece ����� CTS ��ȣ
 *                          �ߺ� ������ �Ȱ��� ����ϱ����̳� �ĳ� CTS ��ȣ��
 *                          �����ϸ�, �׷��� �ʴٸ� SDP_CTS_IDX_UNLK �̾�� �Ѵ�.
 * aFSCreditSize     - [IN] �������� ������ Free Space Credit ũ��
 * aDecDelRowCnt     - [IN] Delete ������ ���� ����ε��� ��� TRUE,
 *                          Delete Row ������ ���ҽ�Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::logAndUnbindCTS( sdrMtx        * aMtx,
                                     sdpPhyPageHdr * aPageHdrPtr,
                                     UChar           aCTSlotIdxBfrUndo,
                                     UChar           aCTSlotIdxAftUndo,
                                     SShort          aFSCreditSize,
                                     idBool          aDecDelRowCnt )
{
    UInt  sLogSize;

    sLogSize = (ID_SIZEOF(UChar) * 2)       + // CTSlotIdx * 2
                ID_SIZEOF(aFSCreditSize)    +
                ID_SIZEOF(aDecDelRowCnt);

    IDE_TEST( unbindCTS4REDO( aPageHdrPtr,
                              aCTSlotIdxBfrUndo,
                              aCTSlotIdxAftUndo,
                              aFSCreditSize,
                              aDecDelRowCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)aPageHdrPtr,
                                        NULL,
                                        sLogSize,
                                        SDR_SDC_UNBIND_CTS )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aCTSlotIdxBfrUndo,
                                   ID_SIZEOF(aCTSlotIdxBfrUndo) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aCTSlotIdxAftUndo,
                                   ID_SIZEOF(aCTSlotIdxAftUndo) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aFSCreditSize,
                                   ID_SIZEOF(aFSCreditSize) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aDecDelRowCnt,
                                   ID_SIZEOF(aDecDelRowCnt) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ������� CTS�� Row�κ��� ����ε� �Ѵ�.
 *
 * �ߺ� ���ſ� ���� Undo�� �ƴѰ�쿡�� CTS ���ε��� �������״� RefCnt��
 * ���ҽ�Ų��. ���� ���ε��� CTS�� �����صξ��� Free Space Credit�� �����Ѵٸ�,
 * CTS�� ���� FSC�κ��� �ش� Ʈ������� ��ΰ������� �����ߴ� Free Space Credit
 * ��ŭ ���ش�.
 *
 * aPageHdrPtr       - [IN] ����Ÿ�������� ��� ���� ������
 * aCTSlotIdxBfrUndo - [IN] ����ϱ� ���� Row Piece ����� CTS ��ȣ
 * aCTSlotIdxAftUndo - [IN] ����� �Ŀ� Row Piece ����� CTS ��ȣ
 * aFSCreditSize     - [IN] �������� ������ Free Space Credit ũ��
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::unbindCTS4REDO( sdpPhyPageHdr   * aPageHdrPtr,
                                    UChar             aCTSlotIdxBfrUndo,
                                    UChar             aCTSlotIdxAftUndo,
                                    SShort            aFSCreditSize,
                                    idBool            aDecDelRowCnt )
{
    sdpCTL         * sCTL;
    sdpCTS         * sCTS;

    IDE_ERROR( aPageHdrPtr       != NULL );
    IDE_ERROR( aCTSlotIdxBfrUndo != SDP_CTS_IDX_UNLK );

    sCTL = getCTL( aPageHdrPtr );
    sCTS = getCTS( aPageHdrPtr, aCTSlotIdxBfrUndo );

    IDE_ERROR( sCTS->mRefCnt > 0 );
    IDE_ERROR( hasState( sCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE );

    // Undo�� �Ǿ ���� Row Piece�� �ٸ� Ʈ����ǿ� ���� Commit��
    // ��쿡�� RefCnt�� ���ҽ�Ų��.
    if( aCTSlotIdxAftUndo != aCTSlotIdxBfrUndo )
    {
        // BUG-27718 5.3.3 Dog Food] ������������ BMT�ó��������� startup����
        IDE_ERROR( (aCTSlotIdxAftUndo == SDP_CTS_IDX_UNLK) ||
                   (aCTSlotIdxAftUndo == SDP_CTS_IDX_NULL) );
        sCTS->mRefCnt--;
    }
    else
    {
        // �׷��� ���� ���� �ڽ��� ������ Row Piece�� �ߺ� ������
        // ���̿��� RefCnt�� ������Ű�� �ʾ����Ƿ�,���ҽ�Ű����
        // �ʴ´�. ��, FSCredit�� ���ҽ��Ѿ��Ѵ�.
        IDE_ERROR( aCTSlotIdxAftUndo != SDP_CTS_IDX_UNLK );
    }

    if ( aDecDelRowCnt == ID_TRUE )
    {
        decDelRowCntOfCTL( sCTL );
    }

    if ( aFSCreditSize > 0 )
    {
        IDE_ERROR( sCTS->mFSCredit >= aFSCreditSize );

        IDE_TEST( restoreFSCredit( aPageHdrPtr, aFSCreditSize ) 
                  != IDE_SUCCESS );

        decFSCreditOfCTS( sCTS, aFSCreditSize );
    }

    decBindCTSCntOfCTL( sCTL, sCTS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Unbind Row
 *
 * Ʈ������� �ѹ��� �Ǹ� ���ε��� CTS�� ����ε��ϸ鼭 ����Ÿ�������� �����ߴ�
 * Row Piece�� ���� Undo�� �����Ѵ�.
 *
 * aMtx              - [IN] Mtx ������
 * aRowPieceBfrUndo  - [IN] ����Ÿ�������� ��� ���� ������
 * aFSCreditSize     - [IN] �������� ������ Free Space Credit ũ��
 * aDecDelRowCnt     - [IN] Delete ������ ���� ����ε��� ��� TRUE,
 *                          Delete Row ������ ���ҽ�Ų��.
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::logAndUnbindRow( sdrMtx      * aMtx,
                                     UChar       * aRowPieceBfrUndo,
                                     SShort        aFSCreditSize,
                                     idBool        aDecDelRowCnt )
{
    UInt  sLogSize;

    sLogSize = ID_SIZEOF(aFSCreditSize) + ID_SIZEOF(aDecDelRowCnt);

    IDE_TEST( unbindRow4REDO( aRowPieceBfrUndo,
                              aFSCreditSize,
                              aDecDelRowCnt )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        (UChar*)aRowPieceBfrUndo,
                                        NULL,
                                        sLogSize,
                                        SDR_SDC_UNBIND_ROW )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aFSCreditSize,
                                   ID_SIZEOF(aFSCreditSize) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aDecDelRowCnt,
                                   ID_SIZEOF(aDecDelRowCnt) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Row bound CTS�� unbound�Ѵ�.
 *
 * aRowPieceBfrUndo  - [IN] ����Ÿ�������� ��� ���� ������
 * aCTSlotIdxBfrUndo - [IN] ����ϱ� ���� Row Piece ����� CTS ��ȣ
 * aCTSlotIdxAftUndo - [IN] ����� �Ŀ� Row Piece ����� CTS ��ȣ
 * aFSCreditSize     - [IN] �������� ������ Free Space Credit ũ��
 *
 ***********************************************************************/
IDE_RC sdcTableCTL::unbindRow4REDO( UChar     * aRowPieceBfrUndo,
                                  SShort      aFSCreditSize,
                                  idBool      aDecDelRowCnt )
{
    sdpCTL         * sCTL;
    sdpPhyPageHdr  * sPageHdrPtr;
    UShort           sFSCreditSize;

    IDE_ERROR( aRowPieceBfrUndo != NULL );

    SDC_GET_ROWHDR_FIELD( aRowPieceBfrUndo,
                          SDC_ROWHDR_FSCREDIT,
                          &sFSCreditSize );

    sPageHdrPtr = sdpPhyPage::getHdr(aRowPieceBfrUndo);

    sCTL = getCTL( sPageHdrPtr );
    decBindRowCTSCntOfCTL( sCTL );

    if ( aDecDelRowCnt == ID_TRUE )
    {
        decDelRowCntOfCTL( sCTL );
    }

    if ( aFSCreditSize > 0 )
    {
        IDE_ERROR( sFSCreditSize >= aFSCreditSize );

        IDE_TEST( restoreFSCredit( sPageHdrPtr, aFSCreditSize )
                  != IDE_SUCCESS );
        sFSCreditSize -= aFSCreditSize;

        SDC_SET_ROWHDR_FIELD( aRowPieceBfrUndo,
                              SDC_ROWHDR_FSCREDIT,
                              &sFSCreditSize );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 *
 * Description : Page�� ���Ͽ� Fast CTS TimeStamping ����
 *
 * Ʈ����� Ŀ�԰������� No-Logging ���� CTS�� Ʈ������� CommitSCN��
 * �����Ѵ�. Ʈ������� �湮�ߴ� �������� ���ؼ� ��湮�� �ϴ� ���ε�,
 * ���� Partial Rollback������ ���ؼ� �湮�ߴ� �������� �ڽſ� �ش��ϴ�
 * CTS�� ���� ���� �����Ƿ� �ݵ�� �ڽ��� CTS�������θ� �Ǵ��� �� Stamping��
 * �����Ѵ�.
 *
 * aTransTSSlotSID- [IN] Ʈ������� TSSlotSID �� ������
 * aFstDskViewSCN - [IN] Ʈ������� ù��° Disk Stmt�� ViewSCN
 * aPagePtr       - [IN] ������ ��� ���� ������
 * aCTSlotIdx     - [IN] Ʈ������� DML�������� �湮������ ���ε��ߴ� CTS ��ȣ
 * aCommitSCN     - [IN] Ʈ������� �Ҵ��� CommitSCN�� ������
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runFastStamping( sdSID         * aTransTSSlotSID,
                                     smSCN         * aFstDskViewSCN,
                                     sdpPhyPageHdr * aPagePtr,
                                     UChar           aCTSlotIdx,
                                     smSCN         * aCommitSCN )
{
    sdpCTS  * sCTS;

    sCTS = getCTS( (sdpPhyPageHdr*)aPagePtr, aCTSlotIdx );

    IDE_ERROR( SM_SCN_IS_NOT_INIT( *aCommitSCN ) );

    /* �ڽ��� ���ε��ߴ� CTS�� ACT �����̴�. ���� �ٸ� Ʈ������� ������̶�
     * ACT �����̹Ƿ� �ڽ��� ������ �ݵ�� Ȯ���Ѵ�. */
    if( hasState( sCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE )
    {
        if( isMyTrans( aTransTSSlotSID,
                       aFstDskViewSCN,
                       sCTS ) == ID_TRUE )
        {
            SM_SET_SCN( &(sCTS->mFSCNOrCSCN), aCommitSCN );
            sCTS->mStat = SDP_CTS_STAT_CTS;
        }
    }
    else
    {
        // ACT�� �ƴѰ��� �ڽ��� ���ε��ߴٰ� Partial Rollback���� ����
        // ����ε��� �Ŀ� ROL ���·� ���� �ְų�, �ٸ� Ʈ������� �����
        // Stamping�� ������ ����̴�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : DRDBRedo Valdation�� ���� FastStamping�� �õ��մϴ�.
 *
 * 1) Stamping�� �ߴ��� ���� �ʾҴ��Ŀ� ���� SCN�� �ٸ� �� �ֱ� �����̴ϴ�.
 * 2) Stamping �Ǿ�� �ٽ� Stamping�� �õ��մϴ�. TSS�� ������ ��Ȱ��
 *    �Ǿ� CommitSCN�� 0�� ���� �ֱ� �����Դϴ�.
 * 3) �� �������� ���ÿ� Stamping�մϴ�. �ι��� ���� Stamping�ϸ�
 *    getCommitSCN�� Ÿ�ֿ̹� ����, ������ Stamping �Ǹ鼭 �ٸ� ������
 *    Stamping �ȵ� ���� �ֱ� �����Դϴ�.
 *
 * aStatistics    - [IN] Dummy�������
 * aPagePtr1      - [IN] ������ ��� ���� ������
 * aPagePtr2      - [IN] ������ ��� ���� ������
 *
 *********************************************************************/
IDE_RC sdcTableCTL::stampingAll4RedoValidation( 
                                            idvSQL           * aStatistics,
                                            UChar            * aPagePtr1,
                                            UChar            * aPagePtr2 )
{
    sdpCTL          * sCTL1;
    sdpCTS          * sCTS1;
    sdpCTL          * sCTL2;
    sdpCTS          * sCTS2;
    smTID             sDummyTID4Wait;
    UInt              sTotCTSCnt;
    UChar             sIdx;
    sdSID             sTSSlotSID;
    smSCN             sRowCommitSCN;
    SInt              sSlotIdx;
    UChar           * sSlotDirPtr1;
    UChar           * sSlotDirPtr2;
    UChar           * sSlotPtr;
    UShort            sTotSlotCnt;
    UChar             sCTSlotIdx;
    sdcRowHdrExInfo   sRowHdrExInfo;
 
    /************************************************************************
     * CTSBoundChangeInfo�� ���� FastStamping �õ�
     ***********************************************************************/
    sCTL1                = getCTL( (sdpPhyPageHdr*)aPagePtr1);
    sCTL2                = getCTL( (sdpPhyPageHdr*)aPagePtr2);

    IDE_ERROR( sCTL1->mTotCTSCnt == sCTL2->mTotCTSCnt );

    sTotCTSCnt           = sCTL1->mTotCTSCnt;
    for ( sIdx = 0; sIdx < sTotCTSCnt; sIdx++ )
    {
        sCTS1  = getCTS( sCTL1, sIdx );
        sCTS2  = getCTS( sCTL2, sIdx );

        /* Stamping �� ���¶�, �ٽ� Stampnig�� �մϴ�.
         * �ֳ��ϸ� TSS�� ������ ��Ȱ��Ǿ� CommtSCN�� 0�� ������ ����
         * �ֱ� �����Դϴ�. */
        if( ( hasState( sCTS1->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE ) ||
            ( hasState( sCTS1->mStat, SDP_CTS_STAT_CTS ) == ID_TRUE ) )
        {
            IDE_ERROR( hasState( sCTS2->mStat, SDP_CTS_STAT_ACT ) || 
                       hasState( sCTS2->mStat, SDP_CTS_STAT_CTS ) );

            sTSSlotSID = SD_MAKE_SID( sCTS1->mTSSPageID, sCTS1->mTSSlotNum );

            IDE_ERROR( sTSSlotSID == 
                       SD_MAKE_SID( sCTS2->mTSSPageID, sCTS2->mTSSlotNum ) );

            IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                                  NULL,        /* aTrans */
                                                  sTSSlotSID,
                                                  &(sCTS1->mFSCNOrCSCN),
                                                  SM_SCN_INIT, /* aStmtViewSCN */
                                                  &sDummyTID4Wait,
                                                  &sRowCommitSCN )
                      != IDE_SUCCESS );

            if ( SM_SCN_IS_NOT_INFINITE( sRowCommitSCN ) )
            {
                SM_SET_SCN( &(sCTS1->mFSCNOrCSCN), &sRowCommitSCN );
                sCTS1->mStat  = SDP_CTS_STAT_CTS;

                SM_SET_SCN( &(sCTS2->mFSCNOrCSCN), &sRowCommitSCN );
                sCTS2->mStat  = SDP_CTS_STAT_CTS;
            }
            /* ���� �����Ⱚ�� ���� �����ϴ�. ������ RedoValidation������
             * ���ǹ��� Diff�� �����. */
            sCTS1->mAlign = 0;
            sCTS2->mAlign = 0;
        }
        else
        {
            /* RowTimeStamping�� ��Ȱ�� ������ ���¸� ���ʿ��� diff�� ���ֱ�
             * ���� �ʱ�ȭ �Ѵ�. */
            idlOS::memset( sCTS1, 0, ID_SIZEOF( sdpCTS ) );
            sCTS1->mStat          = SDP_CTS_STAT_NUL;

            idlOS::memset( sCTS2, 0, ID_SIZEOF( sdpCTS ) );
            sCTS2->mStat          = SDP_CTS_STAT_NUL;
        }
    }

    /************************************************************************
     * RowBoundChangeInfo�� ���� Stamping�� �õ��Ѵ�.
     ***********************************************************************/
    sSlotDirPtr1 = sdpPhyPage::getSlotDirStartPtr(aPagePtr1);
    sTotSlotCnt = sdpSlotDirectory::getCount(sSlotDirPtr1);
    sSlotDirPtr2 = sdpPhyPage::getSlotDirStartPtr(aPagePtr2);

    IDE_TEST( sTotSlotCnt != sdpSlotDirectory::getCount( sSlotDirPtr2 ) );

    /* Transaction ������ Row�� bound �Ǿ��� ��츦 ����Ͽ� ��� Slot��
     * ���鼭 Stamping�� �����Ѵ�. */
    for( sSlotIdx = 0; sSlotIdx < sTotSlotCnt; sSlotIdx++ )
    {
        if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr1, sSlotIdx)
            == ID_TRUE )
        {
            continue;
        }

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr1,
                                                           sSlotIdx,
                                                           &sSlotPtr )
                  != IDE_SUCCESS );

        SDC_GET_ROWHDR_1B_FIELD( sSlotPtr,
                                 SDC_ROWHDR_CTSLOTIDX,
                                 sCTSlotIdx );

        /* BoundRow�̸� */
        if( !SDC_HAS_BOUND_CTS(sCTSlotIdx) )
        {
            sdcRow::getRowHdrExInfo( sSlotPtr, &sRowHdrExInfo );

            sTSSlotSID    = SD_MAKE_SID( sRowHdrExInfo.mTSSPageID,
                                         sRowHdrExInfo.mTSSlotNum );

            /* Stamping �� ���¶�, �ٽ� Stampnig�� �մϴ�.
             * �ֳ��ϸ� TSS�� ������ ��Ȱ��Ǿ� CommtSCN�� 0�� ������ ����
             * �ֱ� �����Դϴ�. 
             * �ٸ� TSS�� Null�� ���� �̹� RowStamping�� �Ȱ��̱� ������
             * �����մϴ�. */
            //if( SDC_CTS_SCN_IS_NOT_COMMITTED( sRowHdrExInfo.mFSCNOrCSCN) )
            if( sTSSlotSID != SD_NULL_SID )
            {
                IDE_TEST( sdcTSSegment::getCommitSCN( 
                                                    aStatistics,
                                                    NULL,        /* aTrans */
                                                    sTSSlotSID,
                                                    &sRowHdrExInfo.mFSCNOrCSCN,
                                                    SM_SCN_INIT, /* aStmtViewSCN */
                                                    &sDummyTID4Wait,
                                                    &sRowCommitSCN )
                          != IDE_SUCCESS );

                if ( SM_SCN_IS_NOT_INFINITE(sRowCommitSCN) )
                {
                    /* Page1�� ���� */
                    SDC_SET_ROWHDR_FIELD( sSlotPtr,
                                          SDC_ROWHDR_FSCNORCSCN,
                                          &sRowCommitSCN );

                    /* Page2�� ���� */
                    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                                sSlotDirPtr2,
                                                                sSlotIdx,
                                                                &sSlotPtr )
                              != IDE_SUCCESS );
                    SDC_SET_ROWHDR_FIELD( sSlotPtr,
                                          SDC_ROWHDR_FSCNORCSCN,
                                          &sRowCommitSCN );
                }
            }
        }
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 *
 * Description : Delayed CTS TimeStamping ����
 *
 * Delayed Row TimeStamping�� Row�� GetValidVersion�̳� canUpdateRowPiece
 * ����������� Row�� CommitSCN�� �˰��� �Ҷ� ����ȴ�.
 * �� ������ Row�� ������ �Ϲ������� TSS�� �湮�Ͽ� �ش� Row�� ������ Ʈ�������
 * ���¸� ���� �Ϸᰡ �� ��쿡 No-Logging ���� CTS TimeStamping��
 * �����Ѵ�. Delayed�� �ǹ̴� ����Ʈ������� ���� Ŀ�԰������� ������
 * CTS TimeStamping�� �ƴ϶�, �� ���� �ٸ� Ʈ����ǿ� ���ؼ� �����Ǿ� TimeStamping
 * �� �ȴٴ� ���̴�.
 *
 * aStatistics    - [IN] �������
 * aTrans         - [IN] �ڽ��� TX�� ������
 * aCTSlotIdx     - [IN]
 * aObjPtr        - [IN] CTS ������ Ȥ�� RowPiece ������
 * aPageReadMode  - [IN] page read mode(SPR or MPR)
 * aStmtViewSCN   - [IN] �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aTrySuccess    - [OUT] Delayed CTS TimeStamping�� ��������
 * aWait4TransID  - [OUT] ����ؾ��� ��� Ʈ������� ID
 * aRowCommitSCN  - [OUT] ��� Ʈ������� CommitSCN
 * aFSCreditSize  - [OUT] CTS�� ���� FSCreditSize
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runDelayedStamping( idvSQL           * aStatistics,
                                        void             * aTrans,
                                        UChar              aCTSlotIdx,
                                        void             * aObjPtr,
                                        sdbPageReadMode    aPageReadMode,
                                        smSCN              aStmtViewSCN,
                                        idBool           * aTrySuccess,
                                        smTID            * aWait4TransID,
                                        smSCN            * aRowCommitSCN,
                                        SShort           * aFSCreditSize )
{
    IDE_ERROR( aObjPtr       != NULL );
    IDE_ERROR( aTrySuccess   != NULL );
    IDE_ERROR( aWait4TransID != NULL );
    IDE_ERROR( aRowCommitSCN != NULL );
    IDE_ERROR( aFSCreditSize != NULL );

    if ( SDC_HAS_BOUND_CTS(aCTSlotIdx) )
    {
        IDE_TEST( runDelayedStampingOnCTS( aStatistics,
                                           aTrans,
                                           (sdpCTS*)aObjPtr,
                                           aPageReadMode,
                                           aStmtViewSCN,
                                           aTrySuccess,
                                           aWait4TransID,
                                           aRowCommitSCN,
                                           aFSCreditSize )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ERROR( aCTSlotIdx == SDP_CTS_IDX_NULL );

        IDE_TEST( runDelayedStampingOnRow( aStatistics,
                                           aTrans,
                                           (UChar*)aObjPtr,
                                           aPageReadMode,
                                           aStmtViewSCN,
                                           aTrySuccess,
                                           aWait4TransID,
                                           aRowCommitSCN,
                                           aFSCreditSize )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * Description : ��� �������� ��� slot�鿡 ���Ͽ� Delayed TimeStamping��
 *               �����Ѵ�.
 *
 * BUG-48353 : �� �Լ��� Stamping �Ǵ� ĳ�� �������� ���鶧�� ����Ѵ�.
 *             ���� Pending Wait�� �����ʴ´�.
 *
 * aStatistics         - [IN] �������
 * aTrans              - [IN] �ڽ��� TX�� ������
 * aPageptr            - [IN] ������ ���� ������
 * aPageReadMode       - [IN] page read mode(SPR or MPR)
 *********************************************************************/
IDE_RC sdcTableCTL::runDelayedStampingAll( idvSQL          * aStatistics,
                                           void            * aTrans,
                                           UChar           * aPagePtr,
                                           sdbPageReadMode   aPageReadMode )
{
    smSCN       sMyFstDskViewSCN;
    sdSID       sMyTSSlotSID;
    UShort      sSlotIdx;   /* BUG-39748 */
    UChar     * sSlotDirPtr;
    UChar     * sSlotPtr;
    UShort      sTotSlotCnt;
    UChar       sCTSlotIdx;
    SShort      sFSCreditSize;
    smSCN       sRowCSCN;
    smSCN       sFstDskViewSCN;
    idBool      sTrySuccess;
    smTID       sDummyTID4Wait;
    sdpCTL    * sCTL;
    UInt        sTotCTSCnt;
    UChar       sIdx;
    sdpCTS    * sCTS;

    IDE_ERROR( aPagePtr != NULL );

    /* CTSBoundChangeInfo�� ���� Stamping�� ���� �õ��Ѵ�. */
    sCTL                 = getCTL( (sdpPhyPageHdr*)aPagePtr );
    sTotCTSCnt           = sCTL->mTotCTSCnt;
    for( sIdx = 0; sIdx < sTotCTSCnt; sIdx++ )
    {
        sCTS  = getCTS( sCTL, sIdx );
        if( hasState( sCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE )
        {
            IDE_TEST( runDelayedStampingOnCTS( aStatistics,
                                               NULL,
                                               sCTS,
                                               aPageReadMode,
                                               SM_SCN_INIT,
                                               &sTrySuccess,
                                               &sDummyTID4Wait,
                                               &sRowCSCN,
                                               &sFSCreditSize )
                      != IDE_SUCCESS );
        }
    }

    /* TASK-6105 
     * Row Bind CTS�� �����Ҷ��� Row�� bound�� Transaction������ Stamping�Ѵ� */
    if ( sCTL->mRowBindCTSCnt != 0)
    {
        /* RowBoundChangeInfo�� ���� Stamping�� �õ��Ѵ�. */
        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(aPagePtr);
        sTotSlotCnt = sdpSlotDirectory::getCount(sSlotDirPtr);

        sMyFstDskViewSCN  = smxTrans::getFstDskViewSCN( aTrans );
        sMyTSSlotSID = smxTrans::getTSSlotSID( aTrans );

        /* Transaction ������ Row�� bound �Ǿ��� ��츦 ����Ͽ� ��� Slot��
         * ���鼭 Stamping�� �����Ѵ�. */
        for ( sSlotIdx = 0; sSlotIdx < sTotSlotCnt; sSlotIdx++ )
        {
            if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, sSlotIdx)
                == ID_TRUE )
            {
                continue;
            }
 
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                               sSlotIdx,   
                                                               &sSlotPtr )
                      != IDE_SUCCESS );
 
            SDC_GET_ROWHDR_1B_FIELD( sSlotPtr, SDC_ROWHDR_CTSLOTIDX, sCTSlotIdx );
            SDC_GET_ROWHDR_FIELD( sSlotPtr, SDC_ROWHDR_FSCNORCSCN, &sRowCSCN );
 
            if ( (sCTSlotIdx == SDP_CTS_IDX_NULL) &&
                 SDC_CTS_SCN_IS_NOT_COMMITTED(sRowCSCN) )
            {
                if( sdcRow::isMyTransUpdating( aPagePtr,
                                               sSlotPtr,
                                               sMyFstDskViewSCN,
                                               sMyTSSlotSID,
                                               &sFstDskViewSCN )
                    == ID_FALSE )
 
                {
                    IDE_TEST( runDelayedStampingOnRow( aStatistics,
                                                       NULL,
                                                       sSlotPtr,
                                                       aPageReadMode,
                                                       SM_SCN_INIT,
                                                       &sTrySuccess,
                                                       &sDummyTID4Wait,
                                                       &sRowCSCN,
                                                       &sFSCreditSize )
                              != IDE_SUCCESS );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : Delayed CTS TimeStamping ����
 *
 * Delayed Row TimeStamping�� Row�� GetValidVersion�̳� canUpdateRowPiece
 * ����������� Row�� CommitSCN�� �˰��� �Ҷ� ����ȴ�.
 * �� ������ Row�� ������ �Ϲ������� TSS�� �湮�Ͽ� �ش� Row�� ������ Ʈ�������
 * ���¸� ���� �Ϸᰡ �� ��쿡 No-Logging ���� CTS TimeStamping��
 * �����Ѵ�. Delayed�� �ǹ̴� ����Ʈ������� ���� Ŀ�԰������� ������
 * CTS TimeStamping�� �ƴ϶�, �� ���� �ٸ� Ʈ����ǿ� ���ؼ� �����Ǿ� TimeStamping
 * �� �ȴٴ� ���̴�.
 *
 * aStatistics    - [IN] �������
 * aTrans         - [IN] �ڽ��� TX�� ������
 * aCTS           - [IN] CTS ������
 * aPageReadMode  - [IN] page read mode(SPR or MPR)
 * aStmtViewSCN   - [IN] �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aTrySuccess    - [OUT] Delayed CTS TimeStamping�� ��������
 * aWait4TransID  - [OUT] ����ؾ��� ��� Ʈ������� ID
 * aRowCommitSCN  - [OUT] ��� Ʈ������� CommitSCN
 * aFSCreditSize  - [OUT] CTS�� ���� FSCreditSize
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runDelayedStampingOnCTS( idvSQL           * aStatistics,
                                             void             * aTrans,
                                             sdpCTS           * aCTS,
                                             sdbPageReadMode    aPageReadMode,
                                             smSCN              aStmtViewSCN,
                                             idBool           * aTrySuccess,
                                             smTID            * aWait4TransID,
                                             smSCN            * aRowCommitSCN,
                                             SShort           * aFSCreditSize )
{
    sdSID         sTSSlotSID;
    UChar       * sPageHdr;
    idBool        sTrySuccess = ID_FALSE;

    *aFSCreditSize  = aCTS->mFSCredit;

    sTSSlotSID = SD_MAKE_SID( aCTS->mTSSPageID, aCTS->mTSSlotNum );
    IDE_ERROR( SDC_CTS_SCN_IS_NOT_COMMITTED(aCTS->mFSCNOrCSCN) );

    IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                          aTrans,
                                          sTSSlotSID,
                                          &(aCTS->mFSCNOrCSCN),
                                          aStmtViewSCN,
                                          aWait4TransID,
                                          aRowCommitSCN )
              != IDE_SUCCESS );

    if ( SM_SCN_IS_NOT_INFINITE( *aRowCommitSCN ) )
    {
        sPageHdr = sdpPhyPage::getPageStartPtr( aCTS );

        /*
         * GetValidVersion ����ÿ��� �������� S-latch �� ȹ��
         * �Ǿ� �ִ� ��쿡�� X-latch�� �������ش�.
         * (���������� Ǯ��,�ٽ� ��´�, ��, �� �ڽŸ� �������� latch��
         * ȹ���� ��쿡 ���ؼ��̴�. )
         */
        sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                            sPageHdr,
                                            aPageReadMode,
                                            &sTrySuccess );

        if( sTrySuccess == ID_TRUE )
        {
            sdbBufferMgr::setDirtyPageToBCB( aStatistics,
                                             sPageHdr );
            aCTS->mStat = SDP_CTS_STAT_CTS;
            SM_SET_SCN( &(aCTS->mFSCNOrCSCN), aRowCommitSCN );

            *aTrySuccess = ID_TRUE;
        }
        else
        {
            *aTrySuccess = ID_FALSE;
        }
    }
    else
    {
        *aTrySuccess = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : Delayed Row TimeStamping ����
 *
 * Delayed Row TimeStamping�� Row�� GetValidVersion�̳� canUpdateRowPiece
 * ����������� Row�� CommitSCN�� �˰��� �Ҷ� ����ȴ�.
 * �� ������ Row�� ������ �Ϲ������� TSS�� �湮�Ͽ� �ش� Row�� ������ Ʈ�������
 * ���¸� ���� �Ϸᰡ �� ��쿡 No-Logging ���� CTS TimeStamping��
 * �����Ѵ�. Delayed�� �ǹ̴� ����Ʈ������� ���� Ŀ�԰������� ������
 * CTS TimeStamping�� �ƴ϶�, �� ���� �ٸ� Ʈ����ǿ� ���ؼ� �����Ǿ� TimeStamping
 * �� �ȴٴ� ���̴�.
 *
 * aStatistics    - [IN] �������
 * aTrans         - [IN] �ڽ��� TX�� ������
 * aRowSlotPtr    - [IN] RowSlotPtr
 * aPageReadMode  - [IN] page read mode(SPR or MPR)
 * aStmtViewSCN   - [IN] �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aTrySuccess    - [OUT] Delayed CTS TimeStamping�� ��������
 * aWait4TransID  - [OUT] ����ؾ��� ��� Ʈ������� ID
 * aRowCommitSCN  - [OUT] ��� Ʈ������� CommitSCN
 * aFSCreditSize  - [OUT] RowPiece�� Extra �� ���� FSCreditSize
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runDelayedStampingOnRow( idvSQL           * aStatistics,
                                             void             * aTrans,
                                             UChar            * aRowSlotPtr,
                                             sdbPageReadMode    aPageReadMode,
                                             smSCN              aStmtViewSCN,
                                             idBool           * aTrySuccess,
                                             smTID            * aWait4TransID,
                                             smSCN            * aRowCommitSCN,
                                             SShort           * aFSCreditSize )
{
    sdSID            sTSSlotSID;
    UChar          * sPageHdr;
    idBool           sTrySuccess = ID_FALSE;
    sdcRowHdrExInfo  sRowHdrExInfo;
    sdpCTL         * sCTL;

    IDE_ERROR( aRowSlotPtr   != NULL );
    IDE_ERROR( aTrySuccess   != NULL );
    IDE_ERROR( aWait4TransID != NULL );
    IDE_ERROR( aRowCommitSCN != NULL );
    IDE_ERROR( aFSCreditSize != NULL );

    sdcRow::getRowHdrExInfo( aRowSlotPtr, &sRowHdrExInfo );

    IDE_DASSERT( SDC_CTS_SCN_IS_NOT_COMMITTED(sRowHdrExInfo.mFSCNOrCSCN) );

    *aFSCreditSize = sRowHdrExInfo.mFSCredit;
    sTSSlotSID     = SD_MAKE_SID( sRowHdrExInfo.mTSSPageID,
                                  sRowHdrExInfo.mTSSlotNum );

    IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                          aTrans,
                                          sTSSlotSID,
                                          &sRowHdrExInfo.mFSCNOrCSCN,
                                          aStmtViewSCN,
                                          aWait4TransID,
                                          aRowCommitSCN )
              != IDE_SUCCESS );

    if( SM_SCN_IS_NOT_INFINITE(*aRowCommitSCN) )
    {
        sPageHdr = sdpPhyPage::getPageStartPtr( aRowSlotPtr );

        /*
         * GetValidVersion ����ÿ��� �������� S-latch �� ȹ��
         * �Ǿ� �ִ� ��쿡�� X-latch�� �������ش�.
         * (���������� Ǯ��,�ٽ� ��´�, ��, �� �ڽŸ� �������� latch��
         * ȹ���� ��쿡 ���ؼ��̴�. )
         */
        sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                            sPageHdr,
                                            aPageReadMode,
                                            &sTrySuccess );

        if( sTrySuccess == ID_TRUE )
        {
            sdbBufferMgr::setDirtyPageToBCB( aStatistics,
                                             sPageHdr );

            SDC_SET_ROWHDR_FIELD( aRowSlotPtr,
                                  SDC_ROWHDR_FSCNORCSCN,
                                  aRowCommitSCN );

            sCTL = getCTL( (sdpPhyPageHdr*)sPageHdr );
            decBindRowCTSCntOfCTL( sCTL );
            
            *aTrySuccess = ID_TRUE;
        }
        else
        {
            *aTrySuccess = ID_FALSE;
        }
    }
    else
    {
        *aTrySuccess = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : Delayed Row TimeStamping �� �α� ����
 *
 * CTS�� Active ������ ��쿡 CTS�� ������ Ʈ������� Ŀ���� �ߴٸ�
 * CommitSCN�� �ǵ��Ͽ� CTS�� Nologging���� �����ϰ�
 * (Delayed CTS TimeStamping) Row TimeStamping�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mtx ������
 * aCTSlotIdx     - [IN] TimeStamping�� CTS ��ȣ
 * aObjPtr        - [IN] CTS ������ Ȥ�� RowPiece ������
 * aPageReadMode  - [IN] page read mode(SPR or MPR)
 * aWait4TransID  - [OUT] ����ؾ��� ��� Ʈ������� ID
 * aRowCommitSCN  - [OUT] ��� Ʈ������� CommitSCN
 *
 *********************************************************************/
IDE_RC sdcTableCTL::logAndRunDelayedRowStamping(
                         idvSQL          * aStatistics,
                         sdrMtx          * aMtx,
                         UChar             aCTSlotIdx,
                         void            * aObjPtr,
                         sdbPageReadMode   aPageReadMode,
                         smTID           * aWait4TransID,
                         smSCN           * aRowCommitSCN )
{
    SShort   sFSCreditSize;
    idBool   sTrySuccess;

    IDE_ERROR( aMtx    != NULL );
    IDE_ERROR( aObjPtr != NULL );

    IDE_TEST( runDelayedStamping( aStatistics,
                                  NULL,        /* aTrans */
                                  aCTSlotIdx,
                                  aObjPtr,
                                  aPageReadMode,
                                  SM_SCN_INIT, /* aStmtViewSCN */
                                  &sTrySuccess,
                                  aWait4TransID,
                                  aRowCommitSCN,
                                  &sFSCreditSize )
              != IDE_SUCCESS );

    if ( sTrySuccess == ID_TRUE )
    {
        IDE_TEST( logAndRunRowStamping( aMtx,
                                        aCTSlotIdx,
                                        aObjPtr,
                                        sFSCreditSize,
                                        aRowCommitSCN )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : Row TimeStamping �� �α� ����
 *
 * Row TimeStamping�� CTS TimeStamping�� �Ǿ� �ִٴ� �����Ͽ� Logging ����
 * ����ȴ�. Row�� CTS�� ������踦 �����ϸ鼭, CTS�� ��ϵ� CommitSCN��
 * RowPiece ����� ����ϴ� �Ϸ��� �۾��� Row TimeStamping�̶�� �Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mtx ������
 * aCTSlotIdx     - [IN] TimeStamping�� CTS ��ȣ
 * aObjPtr        - [IN] CTS ������ Ȥ�� RowSlotPtr
 * aFSCreditSize  - [IN] ������ FSCreditSize
 * aCommitSCN     - [IN] Stamping�� CommitSCN
 *
 *********************************************************************/
IDE_RC sdcTableCTL::logAndRunRowStamping( sdrMtx       * aMtx,
                                          UChar          aCTSlotIdx,
                                          void         * aObjPtr,
                                          SShort         aFSCreditSize,
                                          smSCN        * aCommitSCN )
{
    IDE_ERROR( aMtx    != NULL );
    IDE_ERROR( aObjPtr != NULL );
    IDE_ERROR( SM_SCN_IS_NOT_INFINITE(*aCommitSCN) == ID_TRUE );

    if( aFSCreditSize > 0 )
    {
        IDE_TEST( restoreFSCredit( aMtx,
                                   sdpPhyPage::getHdr((UChar*)aObjPtr),
                                   aFSCreditSize )
                  != IDE_SUCCESS );
    }

    /* Logging�� ������, setDirty�� ������ �ؾ� ��. */
    IDE_TEST( sdrMiniTrans::setDirtyPage( aMtx, (UChar*)aObjPtr )
              != IDE_SUCCESS );

    if( SDC_HAS_BOUND_CTS(aCTSlotIdx) == ID_TRUE )
    {
        IDE_TEST( runRowStampingOnCTS( (sdpCTS*)aObjPtr, 
                                       aCTSlotIdx, 
                                       aCommitSCN )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( aCTSlotIdx == SDP_CTS_IDX_NULL );

        IDE_TEST( runRowStampingOnRow( (UChar*)aObjPtr, 
                                       aCTSlotIdx, 
                                       aCommitSCN )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aObjPtr,
                                         NULL,
                                         ID_SIZEOF( UChar ) +
                                         ID_SIZEOF( smSCN ),
                                         SDR_SDC_ROW_TIMESTAMPING )
            != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aCTSlotIdx,
                                   ID_SIZEOF( UChar ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   aCommitSCN,
                                   ID_SIZEOF( smSCN ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : Row TimeStamping ����
 *
 * TimeStamping�� ������ CTS�� ����� ��� Row�� ���ؼ� ������踦 �����ϰ�,
 * CommitSCN�� RowPiece ����� �����Ѵ�. �Ϲ������� CTS�� ����� RowPiece��
 * ������ 3���̻��̸� SlotDirectory�� �����˻��Ͽ� ���� RowPiece�� ã�ƾ��ϸ�,
 * 2��(SDP_CACHE_SLOT_CNT)�����ΰ�쿡�� Cache�� Slot Entry ��ȣ�� ���ؼ� �ٷ� ã�ư���.
 *
 * ���� Row�� Delete�� �Ǿ��ٸ�, CommitSCN�� Delete Bit�� �����Ͽ� RowPiece
 * ����� ����ϸ�, �ش� �������� ���ε��� CTS�� ������ �ϳ��� �����ϰ� �ȴ�.
 *
 * Row TimeStamping�� CTS�� ���´� 'R'(RTS)�̴�
 *
 * aCTS           - [IN] CTS ������
 * aCTSlotIdx     - [IN] TimeStamping�� CTS ��ȣ
 * aCommitSCN     - [IN] RowPiece ����� ������ CommitSCN ������
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runRowStampingOnCTS( sdpCTS     * aCTS,
                                         UChar        aCTSlotIdx,
                                         smSCN      * aCommitSCN )
{
    UInt                i;
    UShort              sSlotSeq;
    UShort              sTotSlotCnt;
    UShort              sSlotCnt;
    UChar             * sSlotDirPtr;
    UChar             * sSlotPtr;
    UShort              sRefCnt;
    UChar             * sPagePtr;
    UChar               sCTSlotIdx;
    smSCN               sCommitSCN;
    sdpCTL            * sCTL;
    sdcRowHdrExInfo     sRowHdrExInfo;

    IDE_ERROR( aCTS          != NULL );
    IDE_ERROR( aCTS->mRefCnt != 0 );

    /* FIT/ART/sm/Bugs/BUG-23648/BUG-23648.tc */
    IDU_FIT_POINT( "1.BUG-23648@sdcTableCTL::runRowStampingOnCTS" );

    sPagePtr = sdpPhyPage::getPageStartPtr((UChar*)aCTS);
    SM_SET_SCN( &sCommitSCN, aCommitSCN );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sPagePtr);
    sTotSlotCnt = sdpSlotDirectory::getCount(sSlotDirPtr);
    sRefCnt     = aCTS->mRefCnt;

    sSlotCnt = ( sRefCnt > SDP_CACHE_SLOT_CNT ) ? sTotSlotCnt : sRefCnt;

    for( i = 0; i < sSlotCnt; i++ )
    {
        if( sRefCnt <= SDP_CACHE_SLOT_CNT )
        {
            sSlotSeq = aCTS->mRefRowSlotNum[i];
        }
        else
        {
            sSlotSeq = i;

            if( sdpSlotDirectory::isUnusedSlotEntry( sSlotDirPtr, sSlotSeq ) 
                == ID_TRUE )
            {
                continue;
            }
        }

        IDE_ERROR( sTotSlotCnt > sSlotSeq );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr, 
                                                           sSlotSeq,
                                                           &sSlotPtr )
                  != IDE_SUCCESS );

        SDC_GET_ROWHDR_1B_FIELD( sSlotPtr,
                                 SDC_ROWHDR_CTSLOTIDX,
                                 sCTSlotIdx );

        sCTSlotIdx = SDC_UNMASK_CTSLOTIDX( sCTSlotIdx );

        if( sCTSlotIdx == aCTSlotIdx )
        {
            sCTSlotIdx = SDP_CTS_IDX_UNLK;

            SDC_SET_ROWHDR_1B_FIELD( sSlotPtr,
                                     SDC_ROWHDR_CTSLOTIDX,
                                     sCTSlotIdx );

            SDC_STAMP_ROWHDREX_INFO( &sRowHdrExInfo,
                                     sCommitSCN );

            sdcRow::writeRowHdrExInfo( sSlotPtr, &sRowHdrExInfo );

            IDE_ERROR( aCTS->mRefCnt > 0 );

            aCTS->mRefCnt--;
        }
    }

    IDE_ERROR( aCTS->mRefCnt == 0 );

    // clear reference slot number
    aCTS->mRefRowSlotNum[0] = SC_NULL_SLOTNUM;
    aCTS->mRefRowSlotNum[1] = SC_NULL_SLOTNUM;
    aCTS->mFSCredit      = 0;
    aCTS->mStat          = SDP_CTS_STAT_RTS;

    sCTL = getCTL( (sdpPhyPageHdr*)sPagePtr );

    decBindCTSCntOfCTL( sCTL, aCTS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 *
 * Description : RowExCTS�� ���� Row TimeStamping ����
 *
 * aSlotPtr       - [IN] RowSlot ������
 * aCTSlotIdx     - [IN] TimeStamping�� CTS ��ȣ
 * aCommitSCN     - [IN] RowPiece ����� ������ CommitSCN ������
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runRowStampingOnRow( UChar      * aSlotPtr,
                                         UChar        aCTSlotIdx,
                                         smSCN      * aCommitSCN )
{
    UChar           sCTSlotIdx;
    smSCN           sCommitSCN;
    sdcRowHdrExInfo sRowHdrExInfo;

    IDE_ERROR( aSlotPtr   != NULL );
    IDE_ERROR( aCTSlotIdx == SDP_CTS_IDX_NULL );

    SM_GET_SCN( &sCommitSCN, aCommitSCN );

    sCTSlotIdx = SDP_CTS_IDX_UNLK;

    SDC_SET_ROWHDR_1B_FIELD( aSlotPtr,
                             SDC_ROWHDR_CTSLOTIDX,
                             sCTSlotIdx );

    SDC_STAMP_ROWHDREX_INFO( &sRowHdrExInfo,
                             sCommitSCN );

    sdcRow::writeRowHdrExInfo(aSlotPtr, &sRowHdrExInfo);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *  
 * Description : ����Ÿ�������� �����صξ��� Free Space Credit ��ȯ
 *
 * Ʈ������� Ŀ�Ե� ����̰ų� �ѹ��ϴ� ��� �����صξ��� Free Space Credit��
 * ��ȯ�Ѵ�. Ŀ���� ��쿡�� ��ȯ�� ������ ����������ν� �ٸ� Ʈ������� �����
 ��               
 * �ְԵǸ�, �ѹ��� ��쿡�� ��ȯ�� ������ �ڽ��� �ѹ��Ҷ� �ʿ��� �����������
 * ���ȴ�.                  
 * ��, ����Ÿ�� ���� �������� ���ؼ� ���� RowPiece ũ�⺸�� �� �۰� ���ŵǴ� ��>�쿡
 * Free Space Credit�� ������ �ȴ�.
 *  
 * aMtx          - [IN] Mtx ������
 * aPageHdrPtr   - [IN] ������ ��� ���� ������
 * aRestoreSize  - [IN] �������� ��ȯ�� ��������� ũ�� (FreeSpaceCredit)
 *  
 **********************************************************************/
IDE_RC sdcTableCTL::restoreFSCredit( sdrMtx         * aMtx,
                                     sdpPhyPageHdr  * aPageHdrPtr,
                                     SShort           aRestoreSize )
{       
    UShort     sNewAFS;             
                                    
    IDE_ASSERT( aPageHdrPtr != NULL );
    IDE_ASSERT( aRestoreSize > 0 );
    IDE_ASSERT( aRestoreSize < (SShort)SD_PAGE_SIZE );

    sNewAFS = sdpPhyPage::getAvailableFreeSize( aPageHdrPtr ) +
              aRestoreSize; 
        
    IDE_TEST( sdpPhyPage::logAndSetAvailableFreeSize(
                                aPageHdrPtr,
                                sNewAFS,
                                aMtx) != IDE_SUCCESS );
        
    return IDE_SUCCESS;
            
    IDE_EXCEPTION_END;
        
    return IDE_FAILURE;
}       

/***********************************************************************
 *
 * Description : ����Ÿ�������� �����صξ��� Free Space Credit ��ȯ
 *
 * Ʈ������� Ŀ�Ե� ����̰ų� �ѹ��ϴ� ��� �����صξ��� Free Space Credit��
 * ��ȯ�Ѵ�. Ŀ���� ��쿡�� ��ȯ�� ������ ����������ν� �ٸ� Ʈ������� ����� ��
 * �ְԵǸ�, �ѹ��� ��쿡�� ��ȯ�� ������ �ڽ��� �ѹ��Ҷ� �ʿ��� �����������
 * ���ȴ�.
 * ��, ����Ÿ�� ���� �������� ���ؼ� ���� RowPiece ũ�⺸�� �� �۰� ���ŵǴ� ��쿡
 * Free Space Credit�� ������ �ȴ�.
 *
 * aPageHdrPtr   - [IN] ������ ��� ���� ������
 * aRestoreSize  - [IN] �������� ��ȯ�� ��������� ũ�� (FreeSpaceCredit)
 *
 **********************************************************************/
IDE_RC sdcTableCTL::restoreFSCredit( sdpPhyPageHdr  * aPageHdrPtr,
                                     SShort           aRestoreSize )
{
    UShort     sNewAFS;

    IDE_ERROR( aPageHdrPtr     != NULL );
    IDE_ERROR( aRestoreSize > 0 );
    IDE_ERROR( aRestoreSize < (SShort)SD_PAGE_SIZE );

    sNewAFS = sdpPhyPage::getAvailableFreeSize( aPageHdrPtr ) +
              aRestoreSize;

    sdpPhyPage::setAvailableFreeSize( aPageHdrPtr, sNewAFS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Target Row Piece�� ���� Row Stamping�� �ݵ�� ����
 *
 * Head RowPiece�� ��쿡�� CanUpdateRowPiece �������� �ݵ�� Row Stamping��
 * �Ϸ�� �� �ش� Row�� ���� ���ſ����� ������� ����������, Head RowPiece�� �ƴ�
 * ��쿡 ���ؼ��� CanUpdateRowPiece �ܰ谡 �����Ǳ� ������ (�ֳ��ϸ�, Head
 * RowPiece�� ���ؼ� �ѹ��� �����ϸ� �Ǳ� �����̴�) Row Stamping�� ���ֱ� ����
 * �ش� �Լ��� ȣ���Ѵ�.
 *
 * �̹� Row Stamping�� �Ϸ�� RowPiece�� ��쿡�� IDE_SUCCESS �� ��ȯ�ϸ� �ȴ�.
 *
 * BUG-48353 : �� �Լ���head piece �� ������ piece �϶� ȣ��Ǵ� �Լ��̴�.
 *             ���� ���⼭ Pending Wait�� �߻��� �� ����.
 *             (�ֳ��ϸ� head piece ó���� �̹� Pending Wait �������̱� �����̴�.)
 *
 * aStatistics   - [IN] �������
 * aStartInfo    - [IN] Mtx StartInfo ������
 * aTargetRow    - [IN] Row TimeStamping�� ������ ��� RowPiece ������
 * aPageReadMode - [IN] page read mode(SPR or MPR)
 *
 **********************************************************************/
IDE_RC sdcTableCTL::checkAndMakeSureRowStamping(
                                    idvSQL          * aStatistics,
                                    sdrMtxStartInfo * aStartInfo,
                                    UChar           * aTargetRow,
                                    sdbPageReadMode   aPageReadMode )
{
    UChar           sCTSlotIdx;
    smSCN           sMyFstDskViewSCN;
    sdSID           sMyTransTSSlotSID;
    sdpPhyPageHdr * sPageHdr;
    UInt            sState = 0;
    sdrMtx          sMtx;
    smTID           sWait4TransID;
    smSCN           sRowCommitSCN;
    smSCN           sFstDskViewSCN;
    SShort          sFSCreditSize;
    void          * sObjPtr;

    IDE_ERROR( sdcRow::isHeadRowPiece( aTargetRow ) == ID_FALSE );

    SDC_GET_ROWHDR_1B_FIELD(aTargetRow, SDC_ROWHDR_CTSLOTIDX, sCTSlotIdx);
    sCTSlotIdx = SDC_UNMASK_CTSLOTIDX(sCTSlotIdx);

    IDE_TEST_CONT( sCTSlotIdx == SDP_CTS_IDX_UNLK,
                   already_row_stamping );

    getChangedTransInfo( aTargetRow,
                         &sCTSlotIdx,
                         &sObjPtr,
                         &sFSCreditSize,
                         &sRowCommitSCN );

    sPageHdr = sdpPhyPage::getHdr( aTargetRow );

    /* BUG-24906 DML�� UndoRec ����ϴ� UNDOTBS ���������ϸ� ������� !!
     * �� �Լ����� ������ Mtx�� Logging�� �����ϴ� ���� �ش� Update/Delete
     * ���꿡 ���� UndoRec ����Ҷ� mtx �� Rollback�� �ɼ� ���� Logical��
     * �����̱� �����̴�. */
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    if ( SDC_CTS_SCN_IS_NOT_COMMITTED(sRowCommitSCN) )
    {
        sMyFstDskViewSCN  = smxTrans::getFstDskViewSCN( sMtx.mTrans );
        sMyTransTSSlotSID = smxTrans::getTSSlotSID( sMtx.mTrans );

        if ( sdcRow::isMyTransUpdating( (UChar*)sPageHdr,
                                        aTargetRow,
                                        sMyFstDskViewSCN,
                                        sMyTransTSSlotSID,
                                        &sFstDskViewSCN ) == ID_FALSE )
        {
            IDE_TEST( logAndRunDelayedRowStamping(
                                      aStatistics,
                                      &sMtx,
                                      sCTSlotIdx,
                                      sObjPtr,
                                      aPageReadMode,
                                      &sWait4TransID, 
                                      &sRowCommitSCN ) != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( logAndRunRowStamping( &sMtx,
                                        sCTSlotIdx,
                                        sObjPtr,
                                        sFSCreditSize,
                                        &sRowCommitSCN )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, (UChar*)sPageHdr )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( already_row_stamping );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : RowPiece�κ��� CommitSCN�� ��ȯ�Ѵ�.
 *
 * aTargetRowPtr  - [IN]  MSCNOrCSCN�� �˰� ���� TargetRow ������
 * aCTSlotIdx     - [OUT] CTSlotIdx
 * aObjPtr        - [OUT] Object Ptr
 * aFSCreditSize  - [OUT] FSCreditSize
 * aMSCNOrCSCN    - [OUT] �ǵ��� MSCNOrCSCN
 *
 **********************************************************************/
void sdcTableCTL::getChangedTransInfo( UChar   * aTargetRow,
                                       UChar   * aCTSlotIdx,
                                       void   ** aObjPtr,
                                       SShort  * aFSCreditSize,
                                       smSCN   * aFSCNOrCSCN )
{
    void    * sObjPtr = NULL;
    sdpCTS  * sCTS;
    UChar     sCTSlotIdx;
    smSCN     sFSCNOrCSCN;
    SShort    sFSCreditSize = 0;

    SM_INIT_SCN( &sFSCNOrCSCN );

    SDC_GET_ROWHDR_1B_FIELD( aTargetRow,
                             SDC_ROWHDR_CTSLOTIDX,
                             sCTSlotIdx );

    sCTSlotIdx = SDC_UNMASK_CTSLOTIDX(sCTSlotIdx);
    
    // SDP_CTS_IDX_UNLK �� �ƴѰ� Ȯ���ϰ� ������ 
    // ����׿����� ������.         
    IDE_DASSERT( sCTSlotIdx != SDP_CTS_IDX_UNLK );
    IDE_TEST_CONT( sCTSlotIdx == SDP_CTS_IDX_UNLK,
                   cont_has_finished );

    if ( SDC_HAS_BOUND_CTS(sCTSlotIdx) )
    {
        sCTS = getCTS( sdpPhyPage::getHdr( aTargetRow ),
                       sCTSlotIdx );

        sFSCreditSize = sCTS->mFSCredit;
        SM_SET_SCN( &sFSCNOrCSCN, &sCTS->mFSCNOrCSCN );

        sObjPtr = sCTS;
    }
    else
    {
        IDE_ASSERT( sCTSlotIdx == SDP_CTS_IDX_NULL );

        SDC_GET_ROWHDR_FIELD( aTargetRow,
                              SDC_ROWHDR_FSCNORCSCN,
                              &sFSCNOrCSCN );

        SDC_GET_ROWHDR_FIELD( aTargetRow,
                              SDC_ROWHDR_FSCREDIT,
                              &sFSCreditSize );

        sObjPtr = aTargetRow;
    }

    IDE_EXCEPTION_CONT( cont_has_finished );

    *aCTSlotIdx     = sCTSlotIdx;
    *aObjPtr        = sObjPtr;
    *aFSCreditSize  = sFSCreditSize;
    SM_SET_SCN( aFSCNOrCSCN, &sFSCNOrCSCN );
}


/***********************************************************************
 * Description : TSS�κ��� CommitSCN�� ��ȯ�Ѵ�.
 *
 * aStatistics      - [IN]  �������
 * aTrans           - [IN]  �ڽ��� TX�� ������
 * aCTSlotIdx       - [IN]  Target Row�� CTSlotIdx
 * aObjPtr          - [IN]  Target Row�� CTS �Ǵ� Row Slot�� ������
 * aStmtViewSCN     - [IN]  �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aTID4Wait        - [OUT] ���� TSS�� ������ TX�� Active ������ ��� �ش� TX�� TID
 * aCommitSCN       - [OUT] TX�� CommitSCN�̳� Unbound CommitSCN Ȥ�� TSS��
 *                          ������ TX�� BeginSCN�ϼ� �ִ�.
 **********************************************************************/
IDE_RC sdcTableCTL::getCommitSCN( idvSQL   * aStatistics,
                                  void     * aTrans,
                                  UChar      aCTSlotIdx,
                                  void     * aObjPtr,
                                  smSCN      aStmtViewSCN,
                                  smTID    * aTID4Wait,
                                  smSCN    * aCommitSCN )
{
    sdpCTS            * sCTS;
    UChar             * sRowSlotPtr;
    sdcRowHdrExInfo     sRowHdrExInfo;
    sdSID               sTSSlotSID;

    IDE_ERROR( aObjPtr         != NULL );
    IDE_ERROR( aTID4Wait       != NULL );
    IDE_ERROR( aCommitSCN      != NULL );

    /* CTS�� Transaction ������ bound �Ǿ� �ִ� ��� */
    if( SDC_HAS_BOUND_CTS(aCTSlotIdx) )
    {
        sCTS = (sdpCTS*)aObjPtr;

        sTSSlotSID = SD_MAKE_SID( sCTS->mTSSPageID, sCTS->mTSSlotNum );
        IDE_ERROR( SDC_CTS_SCN_IS_NOT_COMMITTED(sCTS->mFSCNOrCSCN) );

        IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                              aTrans,
                                              sTSSlotSID,
                                              &(sCTS->mFSCNOrCSCN),
                                              aStmtViewSCN,
                                              aTID4Wait,
                                              aCommitSCN )
                  != IDE_SUCCESS );
    }
    else /* Row�� Transaction ������ bound �Ǿ� �ִ� ��� */
    {
        IDE_ERROR( aCTSlotIdx == SDP_CTS_IDX_NULL );

        sRowSlotPtr = (UChar*)aObjPtr;
        sdcRow::getRowHdrExInfo( sRowSlotPtr, &sRowHdrExInfo );
        sTSSlotSID = SD_MAKE_SID( sRowHdrExInfo.mTSSPageID,
                                  sRowHdrExInfo.mTSSlotNum );

        IDE_TEST( sdcTSSegment::getCommitSCN( aStatistics,
                                              aTrans,
                                              sTSSlotSID,
                                              &sRowHdrExInfo.mFSCNOrCSCN,
                                              aStmtViewSCN,
                                              aTID4Wait,
                                              aCommitSCN )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 *
 * Description : ����Ÿ�������� Delete�� RowPiece ���� SelfAging �� �α� ����
 *
 * aStatistics  - [IN] �������
 * aStartInfo   - [IN] �α��� ���� Mtx ��������
 * aPageHdrPtr  - [IN] ������ ��� ���� ������
 * aSCNtoAging  - [IN] ���� ������ SSCN���� Self-Aging�� ������ �Ǵ� SSCN -> SysMinDskViewSCN
 **********************************************************************/
IDE_RC sdcTableCTL::logAndRunSelfAging( idvSQL          * aStatistics,
                                        sdrMtxStartInfo * aStartInfo,
                                        sdpPhyPageHdr   * aPageHdrPtr,
                                        smSCN           * aSCNtoAging )
{
    sdrMtx   sLogMtx;
    UInt     sState      = 0;
    UShort   sAgedRowCnt = 0;

    IDE_ERROR( aStartInfo  != NULL );
    IDE_ERROR( aSCNtoAging != NULL );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sLogMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( runSelfAging( aPageHdrPtr, aSCNtoAging, &sAgedRowCnt )
              != IDE_SUCCESS );

    if( sAgedRowCnt == 0 )
    {
        IDE_TEST( runDelayedStampingAll( aStatistics,
                                         aStartInfo->mTrans,
                                         (UChar*)aPageHdrPtr,
                                         SDB_SINGLE_PAGE_READ )
                  != IDE_SUCCESS );
    }

    /* Logging�� ������, setDirty�� ������ �ؾ� ��. */

    /* BUG-31508 [sm-disk-collection]does not set dirty page
     *           when executing self aging in DRDB.
     * AgedRow, �� FreeSlot�� Record�� ������ ��� �ݵ�� dirty���Ѿ���.
     * �׷��� ���� ���, FreeSlot�� Record���� Disk�� ���� �ȵ�*/
    IDE_TEST( sdrMiniTrans::setDirtyPage( &sLogMtx,
                                          (UChar*)aPageHdrPtr )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeLogRec( &sLogMtx,
                                         (UChar*)aPageHdrPtr,
                                         (UChar*)aSCNtoAging,
                                         ID_SIZEOF(smSCN),
                                         SDR_SDC_DATA_SELFAGING )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sLogMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sLogMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ����Ÿ�������� Delete�� RowPiece ���� SelfAging ����
 *
 * ����Ÿ������������ Deleted Ŀ�Ե� Row�鵵 ������� ����Ÿ�������� �������д�.
 * �̴� RowPiece ���� Consistent Read�� �����ϱ� ���� ���ε� ����, Delete��
 * Row�� ���� ���� �� Delete�Ǳ� �� RowPiece�� �����ϴ� Statement�� ������ ���
 * Delete�� Row�� UndoSID�� ������ ���������� �����ؾ��Ѵ�. �׷��� ������
 * �� RowPiece�� �����Ǳ� ���������� �� ���ɼ��� �ִ� �̷��� Statement ���� ���
 * �Ϸ�Ǳ� �������� Delete�� RowPiece�� ����Ÿ���������� �����ؼ��� �ȵȴ�.
 *
 * ��, Self-Aging�� �� ���ɼ��� �ִ� Statement�� ��� ������ RowPiece���� ������
 * ����������� ��� ��ȯ�ϴ� �Ϸ��� �۾��� �ǹ��Ѵ�.
 *
 * Self-Aging�� �ϱ� ���ؼ��� SlotDirectory�� ���� �˻��ϸ鼭 �����ϴµ�, �̶� ����
 * Self-Aging�� ����� �Ǵ� RowPiece ������ �ִ� CommitSCN�� ���ϱ⵵ �Ѵ�.
 *
 * aPageHdrPtr  - [IN] ������ ��� ���� ������
 * aSCNtoAging  - [IN] ���� ������ SSCN���� Self-Aging�� ������ �Ǵ� SSCN -> SysMinDskViewSCN
 *
 **********************************************************************/
IDE_RC sdcTableCTL::runSelfAging( sdpPhyPageHdr * aPageHdrPtr,
                                  smSCN         * aSCNtoAging,
                                  UShort        * aAgedRowCnt )
{
    UChar       sCTSlotIdx;
    scSlotNum   sSlotNum;
    UChar    *  sSlotDirPtr;
    UChar    *  sSlotPtr;
    UShort      sTotSlotCnt;
    smSCN       sNewSCNtoAging;
    smSCN       sCommitSCN;
    UShort      sNewAgingDelRowCnt;
    sdpCTL   *  sCTL;
    UShort      sSlotSize;
    UShort      sAgedRowCnt = 0;

    IDE_ASSERT( aPageHdrPtr    != NULL );
    IDE_ASSERT( aSCNtoAging != NULL );

    IDE_ASSERT( (aPageHdrPtr->mPageType == SDP_PAGE_DATA) );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPageHdrPtr);
    sTotSlotCnt = sdpSlotDirectory::getCount(sSlotDirPtr);
    sCTL        = getCTL( aPageHdrPtr );

    SM_INIT_SCN( &sNewSCNtoAging );
    sNewAgingDelRowCnt = 0;
    *aAgedRowCnt       = 0;

    for ( sSlotNum = 0; sSlotNum < sTotSlotCnt; sSlotNum ++ )
    {
        if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, sSlotNum)
            == ID_FALSE )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( sSlotDirPtr,
                                                               sSlotNum,
                                                               &sSlotPtr )
                      != IDE_SUCCESS );

            if ( sdcRow::isDeleted( sSlotPtr ) == ID_TRUE )
            {
                SDC_GET_ROWHDR_1B_FIELD( sSlotPtr,
                                         SDC_ROWHDR_CTSLOTIDX,
                                         sCTSlotIdx );

                if( sCTSlotIdx != SDP_CTS_IDX_UNLK )
                {
                    continue;
                }

                SDC_GET_ROWHDR_FIELD( sSlotPtr,
                                      SDC_ROWHDR_FSCNORCSCN,
                                      &sCommitSCN );

                if( SM_SCN_IS_LE( &sCommitSCN, aSCNtoAging ))
                {
                    sSlotSize = sdcRow::getRowPieceSize( sSlotPtr );
                    IDE_ASSERT( sSlotSize == SDC_ROWHDR_SIZE );

                    IDE_TEST( sdpPhyPage::freeSlot4SID( aPageHdrPtr,
                                                        sSlotPtr,
                                                        sSlotNum,
                                                        sSlotSize )
                              != IDE_SUCCESS );

                    decDelRowCntOfCTL( sCTL );
                    sAgedRowCnt++;
                    continue;
                }
                else
                {
                    if ( SM_SCN_IS_GT(&sCommitSCN, &sNewSCNtoAging))
                    {
                        SM_SET_SCN( &sNewSCNtoAging, &sCommitSCN );
                    }

                    sNewAgingDelRowCnt++;
                }
            }
        }
    }

    if ( sNewAgingDelRowCnt > 0 )
    {
        IDE_ASSERT( SM_SCN_IS_NOT_INIT( sNewSCNtoAging ) );
        SM_SET_SCN( &(sCTL->mSCN4Aging), &sNewSCNtoAging );
    }
    else
    {
        IDE_ASSERT( SM_SCN_IS_INIT( sNewSCNtoAging ) );
        SM_SET_SCN_INFINITE( &sCTL->mSCN4Aging );
    }

    sCTL->mCandAgingCnt = sNewAgingDelRowCnt;
    *aAgedRowCnt = sAgedRowCnt;

    IDE_DASSERT( sdpPhyPage::validate(aPageHdrPtr) == ID_TRUE );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ����Ÿ�������� ���ؼ� SelfAging ���ɿ��� �Ǵ� �� SelfAging ����
 *
 * �ϴ� �Ź� �������� Self-Aging�� ���ؼ� ������ ������ SlotDirectory�� �����˻���
 * �ϴ� ���� �����ũ�� ������ ������� �ٷ� �Ǵ��� �� �ִ� ������ ������� �Ǵ��Ѵ�.
 * �������, Delete�� RowPiece�� ����(��� Self-Aging����� ���� �׷��� ��������
 * �ִ� )�����, Self-Aging �� �� �ִ� RowPiece �� ���� �� �ִ� CommitSCN����
 * ���� �Ǵ��Ѵ�.
 *
 * �ѹ� SelfAing�� �����ߴٸ� �ѹ��� ������ Self-Aging�� ���� ���¸� Ȯ���Ͽ� ��ȯ�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aStartInfo   - [IN] �α��� ���� Mtx ��������
 * aPageHdrPtr  - [IN] ������ ��� ���� ������
 * aCheckFlag   - [OUT] �������� ���� Self-Aging ����
 *
 **********************************************************************/
IDE_RC sdcTableCTL::checkAndRunSelfAging( idvSQL           * aStatistics,
                                          sdrMtxStartInfo  * aStartInfo,
                                          sdpPhyPageHdr    * aPageHdrPtr,
                                          sdpSelfAgingFlag * aCheckFlag )
{
    sdpSelfAgingFlag sCheckFlag;
    smSCN            sSCNtoAging;

    IDE_ERROR( aStartInfo != NULL );
    IDE_ERROR( aPageHdrPtr   != NULL );
    IDE_ERROR( aCheckFlag != NULL );

    SMX_GET_MIN_DISK_VIEW( &sSCNtoAging );
    sCheckFlag = canAgingBySelf( aPageHdrPtr, &sSCNtoAging );

    if ( (sCheckFlag == SDP_SA_FLAG_CHECK_AND_AGING) ||
         (sCheckFlag == SDP_SA_FLAG_AGING_DEAD_ROWS) )
    {
        IDE_TEST( logAndRunSelfAging( aStatistics,
                                      aStartInfo,
                                      aPageHdrPtr,
                                      &sSCNtoAging )/* xxxx isDoaging*/
                  != IDE_SUCCESS );

        sCheckFlag = canAgingBySelf( aPageHdrPtr, &sSCNtoAging );
    }

    *aCheckFlag = sCheckFlag;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ����Ÿ�������� Self-Aging ���� ���� �Ǵ�
 *
 * aPageHdrPtr     - [IN] ������ ��� ���� ������
 * aSCNtoAging  - [IN] ���� ������ SSCN���� Self-Aging�� ������ �Ǵ� SSCN
 *
 ***********************************************************************/
sdpSelfAgingFlag sdcTableCTL::canAgingBySelf(
                        sdpPhyPageHdr * aPageHdrPtr,
                        smSCN         * aSCNtoAging )
{
    sdpCTL           * sCTL;
    sdpSelfAgingFlag   sResult;

    sCTL = getCTL( aPageHdrPtr );

    if ( sCTL->mDelRowCnt > 0 )
    {
        if ( SM_SCN_IS_INFINITE(sCTL->mSCN4Aging) )
        {
            sResult = SDP_SA_FLAG_CHECK_AND_AGING;
        }
        else
        {
            if ( SM_SCN_IS_GE( aSCNtoAging, &sCTL->mSCN4Aging) )
            {
                IDE_ASSERT( sCTL->mCandAgingCnt > 0 );
                sResult = SDP_SA_FLAG_AGING_DEAD_ROWS;
            }
            else
            {
                sResult = SDP_SA_FLAG_CANNOT_AGING;
            }
        }
    }
    else
    {
        IDE_ASSERT( SM_SCN_IS_INFINITE( sCTL->mSCN4Aging ));
        sResult = SDP_SA_FLAG_NOTHING_TO_AGING;
    }

    return sResult;
}

/*********************************************************************
 *
 * Description : Row TimeStamping For ALTER TABLE AGING
 *
 * ALTER TABLE <<TABLE��>> AGING; �������� ����ڰ� Row TimeStaping��
 * ��������� �� �� �ְ� �Ѵ�.
 *
 * aStatistics         - [IN] �������
 * aStartInfo          - [IN] mtx start info
 * aPageptr            - [IN] ������ ���� ������
 * aPageReadMode       - [IN] page read mode(SPR or MPR)
 *
 *********************************************************************/
IDE_RC sdcTableCTL::runRowStampingAll(
                       idvSQL          * aStatistics,
                       sdrMtxStartInfo * aStartInfo,
                       UChar           * aPagePtr,
                       sdbPageReadMode   aPageReadMode )
{
    sdrMtx     sMtx;
    sdpCTL   * sCTL;
    UChar      sIdx;
    sdpCTS   * sCTS;
    UInt       sTotCTSCnt;
    UChar      sStat;
    idBool     sTrySuccess;
    UInt       sState = 0;
    UInt       sStampingSuccessCnt = 0;

    IDE_ERROR( aPagePtr != NULL );

    sCTL = getCTL( (sdpPhyPageHdr*)aPagePtr );

    /* Page�� Row Stamping�� ��� CTS�� �������� �ʴ´ٸ�
     * IDE_SUCCESS�� ��ȯ�ϰ� �Ϸ��Ѵ�. */
    IDE_TEST_CONT( sCTL->mBindCTSCnt == 0, skip_soft_stamping );

    /* S-latch�� ȹ��� ������ ������ X-latch�� ��ȯ���ټ� �ִٸ� ��ȯ�Ѵ�.
     * ��ȯ ���� ���� IDE_SUCCESS�� ��ȯ�ϰ� �Ϸ��Ѵ�. */
    sdbBufferMgr::tryEscalateLatchPage( aStatistics,
                                        aPagePtr,
                                        aPageReadMode,
                                        &sTrySuccess );

    IDE_TEST_CONT( sTrySuccess == ID_FALSE, skip_soft_stamping );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    sTotCTSCnt = sCTL->mTotCTSCnt;

    for ( sIdx = 0; sIdx < sTotCTSCnt; sIdx++ )
    {
        sCTS  = getCTS( sCTL, sIdx );
        sStat = sCTS->mStat;

        if ( hasState( sStat, SDP_CTS_STAT_CTS ) == ID_TRUE )
        {
            IDE_TEST( logAndRunRowStamping( &sMtx,
                                            (UChar)sIdx,
                                            (void*)sCTS,
                                            sCTS->mFSCredit,
                                            &sCTS->mFSCNOrCSCN )
                      != IDE_SUCCESS );

            sStampingSuccessCnt++;
        }
    }

    if ( sStampingSuccessCnt > 0 )
    {
        IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, aPagePtr )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_soft_stamping );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : CTL�� Delete�� Row�߿� �������� Self-Aging ���
 *               Row Piece ������ ����Ͽ� �뷫���� Aging��
 *               �� ������� ũ�� ��ȯ
 *
 * aMtx         - [IN] Mtx ������
 * aPageHdrPtr  - [IN] ������ ��� ������
 *
 ***********************************************************************/
UInt sdcTableCTL::getTotAgingSize( sdpPhyPageHdr * aPageHdrPtr )
{
    sdpCTL * sCTL;

    sCTL = getCTL( aPageHdrPtr );

    return (sCTL->mDelRowCnt * SDC_MIN_ROWPIECE_SIZE);
}

/***********************************************************************
 *
 * Description : CTL�����κ��� �� CTS ���� ��ȯ
 ***********************************************************************/
UInt sdcTableCTL::getCountOfCTS( sdpPhyPageHdr * aPageHdr )
{
    return getCnt(getCTL(aPageHdr));
}


/***********************************************************************
 * TASK-4007 [SM]PBT�� ���� ��� �߰�
 * Description : page�� Dump�Ͽ� table�� ctl�� ����Ѵ�.
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�.
 ***********************************************************************/
IDE_RC sdcTableCTL::dump( UChar *aPage ,
                          SChar *aOutBuf ,
                          UInt   aOutSize )
{
    sdpCTL        * sCTL;
    sdpCTS        * sCTS;
    UInt            sIdx;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sCTL        = getCTL( (sdpPhyPageHdr*)aPage );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
"----------- Table CTL Begin ----------\n"
"mSCN4Aging    : %"ID_UINT64_FMT"\n"
"mCandAgingCnt : %"ID_UINT32_FMT"\n"
"mDelRowCnt    : %"ID_UINT32_FMT"\n"
"mTotCTSCnt    : %"ID_UINT32_FMT"\n"
"mBindCTSCnt   : %"ID_UINT32_FMT"\n"
"mMaxCTSCnt    : %"ID_UINT32_FMT"\n"
"---+---------------------------------------------------------------------------\n"
"   |mTSSPageID TSSlotNum mFSCredit mStat mAlign mRefCnt mRefSlotNum mFSCNOrCSCN\n"
"---+---------------------------------------------------------------------------\n",
                     SM_SCN_TO_LONG( sCTL->mSCN4Aging ),
                     sCTL->mCandAgingCnt,
                     sCTL->mDelRowCnt,
                     sCTL->mTotCTSCnt,
                     sCTL->mBindCTSCnt,
                     sCTL->mMaxCTSCnt );

    for ( sIdx = 0; sIdx < sCTL->mTotCTSCnt ; sIdx++ )
    {
        sCTS  = getCTS( sCTL, sIdx );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%3"ID_UINT32_FMT"|"
                             "%-10"ID_UINT32_FMT
                             " %-9"ID_UINT32_FMT
                             " %-9"ID_UINT32_FMT
                             " %-5"ID_UINT32_FMT
                             " %-6"ID_UINT32_FMT
                             " %-7"ID_UINT32_FMT
                             " [%-4"ID_UINT32_FMT","
                             "%-4"ID_UINT32_FMT"]"
                             " %-11"ID_UINT64_FMT"\n",
                             sIdx,
                             sCTS->mTSSPageID,
                             sCTS->mTSSlotNum,
                             sCTS->mFSCredit,
                             sCTS->mStat,
                             sCTS->mAlign,
                             sCTS->mRefCnt,
                             sCTS->mRefRowSlotNum[ 0 ],
                             sCTS->mRefRowSlotNum[ 1 ],
                             SM_SCN_TO_LONG( sCTS->mFSCNOrCSCN ) );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- Table CTL End   ----------\n" );
    return IDE_SUCCESS;
}


