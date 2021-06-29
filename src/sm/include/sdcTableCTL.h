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

# ifndef _O_SDC_TABLE_CTL_H_
# define _O_SDC_TABLE_CTL_H_ 1

# include <sdpDef.h>
# include <sdpPhyPage.h>

# include <sdcDef.h>
# include <sdcRow.h>

# define SDC_UNMASK_CTSLOTIDX( aCTSlotIdx )   \
    ( aCTSlotIdx & SDP_CTS_IDX_MASK )

# define SDC_HAS_BOUND_CTS( aCTSlotIdx )      \
    ( aCTSlotIdx <= SDP_CTS_MAX_IDX )

# define SDC_SET_CTS_LOCK_BIT( aCTSlotNum )    \
    ( (aCTSlotNum) |= SDP_CTS_LOCK_BIT )

# define SDC_CLR_CTS_LOCK_BIT( aCTSlotNum )    \
    ( (aCTSlotNum) &= ~SDP_CTS_LOCK_BIT )

# define SDC_IS_CTS_LOCK_BIT( aCTSlotNum )     \
    ( ((aCTSlotNum) & SDP_CTS_LOCK_BIT) == SDP_CTS_LOCK_BIT )

class sdcTableCTL
{

public:

    static IDE_RC logAndInit( sdrMtx        * aMtx,
                              sdpPhyPageHdr * aPageHdrPtr,
                              UChar           aInitTrans,
                              UChar           aMaxTrans );

    static IDE_RC logAndExtend( sdrMtx        * aMtx,
                                sdpPhyPageHdr * aPageHdrPtr,
                                UChar           aExtendSlotCnt,
                                UChar         * aCTSlotIdx,
                                idBool        * aTrySuccess );


    static void init( sdpPhyPageHdr   * aPageHdrPtr,
                      UChar             aInitTrans,
                      UChar             aMaxTrans );

    static IDE_RC extend( sdpPhyPageHdr   * aPageHdrPtr,
                          UChar             aExtendSlotCnt,
                          UChar           * aCTSlotIdx,
                          idBool          * aTrySuccess );

    static IDE_RC allocCTSAndSetDirty(
                            idvSQL            * aStatistics,
                            sdrMtx            * aFixMtx,
                            sdrMtxStartInfo   * aStartInfo,
                            sdpPhyPageHdr     * aPagePtr,
                            UChar             * aCTSSlotIdx );

    static IDE_RC allocCTS( idvSQL            * aStatistics,
                            sdrMtx            * aFixMtx,
                            sdrMtx            * aLogMtx,
                            sdbPageReadMode     aPageReadMode,
                            sdpPhyPageHdr     * aPagePtr,
                            UChar             * aCTSlotIdx );

    static IDE_RC bind( sdrMtx        * aMtx,
                        scSpaceID       aSpaceID,
                        UChar         * aNewSlotPtr,
                        UChar           aRowCTSlotIdx,
                        UChar           aNewCTSlotIdx,
                        scSlotNum       aRowSlotNum,
                        UShort          aFSCreditSize4RowHdrEx,
                        SShort          aNewFSCreditSize,
                        idBool          aIncDeleteRowCnt );

    static IDE_RC unbind( sdrMtx         * aMtx,
                          UChar          * aNewSlotPtr,
                          UChar            aCTSlotIdxBfrUndo,
                          UChar            aCTSlotIdxAftUndo,
                          SShort           aFSCreditSize,
                          idBool           aDecDeleteRowCnt );

    static IDE_RC canAllocCTS( sdpPhyPageHdr  * aPageHdrPtr );

    static inline sdpCTS * getCTS( sdpCTL  * aHdrPtr,
                                   UChar     aCTSlotIdx );

    static IDE_RC runFastStamping( sdSID         * aTransTSSlotSID,
                                   smSCN         * aFstDskViewSCN,
                                   sdpPhyPageHdr * aPagePtr,
                                   UChar           aCTSlotIdx,
                                   smSCN         * aCommitSCN );

    static IDE_RC stampingAll4RedoValidation( 
                                    idvSQL           * aStatistics,
                                    UChar            * aPagePtr1,
                                    UChar            * aPagePtr2 );

    static IDE_RC runDelayedStamping( idvSQL           * aStatistics,
                                      void             * aTrans,
                                      UChar              aCTSlotIdx,
                                      void             * aObjPtr,
                                      sdbPageReadMode    aPageReadMode,
                                      smSCN              aStmtViewSCN,
                                      idBool           * aTrySuccess,
                                      smTID            * aWait4TransID,
                                      smSCN            * aRowCommitSCN,
                                      SShort           * aFSCreditSize );

    static IDE_RC runDelayedStampingAll( idvSQL          * aStatistics,
                                         void            * aTrans,
                                         UChar           * aPagePtr,
                                         sdbPageReadMode   aPageReadMode );

    static IDE_RC logAndRunRowStamping( sdrMtx    * aMtx,
                                        UChar       aCTSlotIdx,
                                        void      * aObjPtr,
                                        SShort      aFSCreditSize,
                                        smSCN     * aCommitSCN );

    static IDE_RC runRowStampingAll( idvSQL          * aStatistics,
                                     sdrMtxStartInfo * aStartInfo,
                                     UChar           * aPagePtr,
                                     sdbPageReadMode   aPageReadMode );

    static IDE_RC logAndRunDelayedRowStamping(
                                        idvSQL          * aStatistics,
                                        sdrMtx          * aMtx,
                                        UChar             aCTSlotIdx,
                                        void            * aObjPtr,
                                        sdbPageReadMode   aPageReadMode,
                                        smTID           * aWait4TransID,
                                        smSCN           * aRowCommitSCN );

    static inline sdpCTL * getCTL( sdpPhyPageHdr  * aPageHdrPtr );

    static IDE_RC runRowStampingOnCTS( sdpCTS * aCTS,
                                       UChar    aCTSlotIdx,
                                       smSCN  * aCommitSCN  );

    static IDE_RC runRowStampingOnRow( UChar      * aSlotPtr,
                                       UChar        aCTSlotIdx,
                                       smSCN      * aCommitSCN );

    static inline sdpCTS * getCTS( sdpPhyPageHdr  * aPagePtr,
                                   UChar            aCTSlotIdx );

    static IDE_RC restoreFSCredit( sdpPhyPageHdr  * aPageHdrPtr,
                                   SShort           aRestoreSize );

    static IDE_RC restoreFSCredit( sdrMtx        * aMtx,
                                   sdpPhyPageHdr * aPageHdrPtr,
                                   SShort          aRestoreSize );

    static IDE_RC checkAndMakeSureRowStamping(
                                    idvSQL          * aStatistics,
                                    sdrMtxStartInfo * aStartInfo,
                                    UChar           * aTargetRow,
                                    sdbPageReadMode   aPageReadMode );

    static IDE_RC checkAndRunSelfAging( idvSQL           * aStatistics,
                                        sdrMtxStartInfo  * aMtx,
                                        sdpPhyPageHdr    * aPageHdrPtr,
                                        sdpSelfAgingFlag * aCheckFlag );

    static IDE_RC runSelfAging( sdpPhyPageHdr * aPageHdrPtr,
                                smSCN         * aSCNtoAging,
                                UShort        * aAgedRowCnt );

    static UInt getTotAgingSize( sdpPhyPageHdr * aPageHdrPtr );

    static IDE_RC runDelayedStampingOnCTS( idvSQL           * aStatistics,
                                           void             * aTrans,
                                           sdpCTS           * aCTS,
                                           sdbPageReadMode    aPageReadMode,
                                           smSCN              aStmtViewSCN,
                                           idBool           * aTrySuccess,
                                           smTID            * aWait4TransID,
                                           smSCN            * aRowCommitSCN,
                                           SShort           * aFSCreditSize );

    static IDE_RC runDelayedStampingOnRow( idvSQL           * aStatistics,
                                           void             * aTrans,
                                           UChar            * aRowSlotPtr,
                                           sdbPageReadMode    aPageReadMode,
                                           smSCN              aStmtViewSCN,
                                           idBool           * aTrySuccess,
                                           smTID            * aWait4TransID,
                                           smSCN            * aRowCommitSCN,
                                           SShort           * aFSCreditSize );

    static IDE_RC bindCTS4REDO( sdpPhyPageHdr * aPageHdrPtr,
                                UChar           aOldCTSlotIdx,
                                UChar           aNewCTSlotIdx,
                                scSlotNum       aRowSlotNum,
                                smSCN         * aFstDskViewSCN,
                                sdSID           aTSSlotSID,
                                SShort          aNewFSCreditSize,
                                idBool          aIncDeleteRowCnt );

    static IDE_RC unbindCTS4REDO( sdpPhyPageHdr  * aPageHdrPtr,
                                  UChar            aCTSlotIdxBfrUndo,
                                  UChar            aCTSlotIdxAftUndo,
                                  SShort           aFSCreditSize,
                                  idBool           aDecDelRowCnt );

    static IDE_RC bindRow4REDO( UChar      * aNewSlotPtr,
                                sdSID        aTSSlotRID,
                                smSCN        aFstDskViewSCN,
                                SShort       aFSCreditSizeToWrite,
                                idBool       aIncDelRowCnt );

    static IDE_RC unbindRow4REDO( UChar     * aRowPieceBfrUndo,
                                  SShort      aFSCreditSize,
                                  idBool      aDecDelRowCnt );

    static void getChangedTransInfo( UChar   * aTargetRow,
                                     UChar   * aCTSlotIdx,
                                     void   ** aObjPtr,
                                     SShort  * aFSCreditSize,
                                     smSCN   * aFSCNOrCSCN );

    static IDE_RC getCommitSCN( idvSQL   * aStatistics,
                                void     * aTrans,
                                UChar      aCTSlotIdx,
                                void     * aObjPtr,
                                smSCN      aStmtViewSCN,
                                smTID    * aTID4Wait,
                                smSCN    * aCommitSCN );

    static UInt getCountOfCTS( sdpPhyPageHdr  * aPageHdrPtr );

    /* TASK-4007 [SM] PBT�� ���� ��� �߰�
     * CTS Dump�� �� �ִ� ��� �߰�*/
    static IDE_RC dump( UChar *aPage ,
                        SChar *aOutBuf ,
                        UInt   aOutSize );
private:

    static IDE_RC logAndBindCTS( sdrMtx         * aMtx,
                                 scSpaceID        aSpaceID,
                                 sdpPhyPageHdr  * aPageHdrPtr,
                                 UChar            aOldCTSlotIdx,
                                 UChar            aNewCTSlotIdx,
                                 scSlotNum        aRowSlotNum,
                                 SShort           aNewFSCreditSize,
                                 idBool           aIncDeleteRowCnt );

    static IDE_RC logAndUnbindCTS( sdrMtx         * aMtx,
                                   sdpPhyPageHdr  * aPageHdrPtr,
                                   UChar            aCTSlotIdxBfrUndo,
                                   UChar            aCTSlotIdxAftUndo,
                                   SShort           aFSCreditSize,
                                   idBool           aDecDelRowCnt );

    static IDE_RC logAndBindRow( sdrMtx         * aMtx,
                                 UChar          * aNewSlotPtr,
                                 SShort           aFSCreditSizeToWrite,
                                 idBool           aIncDelRowCnt );

    static IDE_RC logAndUnbindRow( sdrMtx         * aMtx,
                                   UChar          * aNewSlotPtr,
                                   SShort           aFSCreditSize,
                                   idBool           aDecDelRowCnt );

    static sdpSelfAgingFlag canAgingBySelf(
                                   sdpPhyPageHdr * aPageHdrPtr,
                                   smSCN         * aSCNtoAging );

    static IDE_RC logAndRunSelfAging( idvSQL          * aStatistics,
                                      sdrMtxStartInfo * aStartInfo,
                                      sdpPhyPageHdr   * aPageHdrPtr,
                                      smSCN           * aSCNtoAging);

    static inline void incDelRowCntOfCTL( sdpCTL * aCTL );
    static inline void decDelRowCntOfCTL( sdpCTL * aCTL );
    static inline void incBindCTSCntOfCTL( sdpCTL * aCTL );
    static inline void decBindCTSCntOfCTL( sdpCTL * aCTL,
                                           sdpCTS * aCTS );

    static inline void incBindRowCTSCntOfCTL( sdpCTL * aCTL );
    static inline void decBindRowCTSCntOfCTL( sdpCTL * aCTL );

    static inline void incFSCreditOfCTS( sdpCTS * aCTS,
                                         UShort   aFSCredit );
    static inline void decFSCreditOfCTS( sdpCTS * aCTS,
                                         UShort   aFSCredit );


    static inline idBool hasState( UChar    aState, UChar  aStateSet );
    static inline UInt getCnt( sdpCTL * aHdrPtr );
#if 0  //not used
    static idBool isMyTrans( sdSID         * aTransTSSlotSID,
                             smSCN         * aTransOldestBSCN,
                             sdpPhyPageHdr * aPagePtr,
                             UChar           aCTSlotIdx );
#endif
    static idBool isMyTrans( sdSID         * aTransTSSlotSID,
                             smSCN         * aTransOldestBSCN,
                             sdpCTS        * aCTS );

};

/***********************************************************************
 *
 * Description : CTL����κ��� �� CTS ���� ��ȯ
 *
 * aCTL  - [IN] CTL ��� ������
 *
 ***********************************************************************/
inline UInt sdcTableCTL::getCnt( sdpCTL * aCTL )
{
    return aCTL->mTotCTSCnt;
}

/***********************************************************************
 *
 * Description : ������ �����ͷκ��� CTL ��� ��ȯ
 *
 * aPageHdrPtr - [IN] ������ ��� ���� ������
 *
 ***********************************************************************/
inline sdpCTL* sdcTableCTL::getCTL( sdpPhyPageHdr  * aPageHdrPtr )
{
    return (sdpCTL*)sdpPhyPage::getCTLStartPtr( (UChar*)aPageHdrPtr );
}

/***********************************************************************
 *
 * Description : ������ �����ͷκ��� CTS ������ ��ȯ
 *
 * aPageHdrPtr   - [IN] ������ ��� ���� ������
 * aCTSlotIdx - [IN] ��ȯ�� CTS�� ��ȣ
 *
 ***********************************************************************/
inline sdpCTS * sdcTableCTL::getCTS( sdpPhyPageHdr  * aPageHdrPtr,
                                     UChar            aCTSlotIdx )
{
    return getCTS( getCTL(aPageHdrPtr) , aCTSlotIdx );
}

/***********************************************************************
 *
 * Description : CTL ����κ��� CTS ������ ��ȯ
 *
 * aCTL       - [IN] CTL ��� ������
 * aCTSlotIdx - [IN] ��ȯ�� CTS�� ��ȣ
 *
 * [ ��ȯ�� ]
 *
 * aCTSlotIdx �� �ش��ϴ� CTS ������ ��ȯ
 *
 ***********************************************************************/
inline sdpCTS * sdcTableCTL::getCTS( sdpCTL * aHdrPtr,
                                     UChar    aCTSlotIdx )
{
    return (sdpCTS*)( (UChar*)aHdrPtr + ID_SIZEOF( sdpCTL ) +
                      (ID_SIZEOF( sdpCTS ) * aCTSlotIdx ) );
}

/***********************************************************************
 *
 * Description : CTS�� ���� Ȯ��
 *
 * aState     - [IN] CTS�� ����
 * aStateSet  - [IN] Ȯ���غ� ������ ����
 *
 * [ ��ȯ�� ]
 *
 * CTS�� ���°��� �����߿� �ϳ��� ��ġ�Ѵٸ� ID_TRUE�� ��ȯ�ϰ�
 * �׷��� �ʴٸ�, ID_FALSE�� ��ȯ�Ѵ�.
 *
 * �������, CTS�� ���°� SDP_CTS_STAT_RTS��� �Ҷ�,
 *         Ȯ���ϰ����ϴ� StateSet�� (SDP_CTS_STAT_RTS|SDC_CTS_STAT_CTS)
 *         ��� �Ѵٸ� �ϳ��� ���°� ��ġ�ϱ� ������ TRUE�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/
inline idBool sdcTableCTL::hasState( UChar    aState,
                                     UChar    aStateSet )
{
    idBool sHasState;

    if ( ( aState & aStateSet ) != 0 )
    {
        sHasState = ID_TRUE;
    }
    else
    {
        sHasState = ID_FALSE;
    }

    return sHasState;
}

/***********************************************************************
 *
 * Description : CTS�� Free Space Credit ���� ����
 *
 * Ʈ������� �ѹ��� ����Ͽ� Ʈ����� �Ϸ������� �ݵ�� Ȯ���� �ξ�� �ϴ� ������
 * ��������� �������ѵд�. �ش� Ʈ������� �Ϸ�Ǳ� �������� �ش� ���������� ������
 * ��������� �����Ǿ� �ٸ� Ʈ����ǿ� ���ؼ� �Ҵ���� ���ϵ��� �Ѵ�.
 *
 * ��, Ʈ������� �ѹ��ϴ� ��쿡 �ݵ�� �ѹ��� �����ؾ��ϱ� �����̴�.
 *
 * aCTS       - [IN] CTS ������
 * aFSCredit  - [IN] ������ų FreeSpaceCredit ũ�� (>0)
 *
 ***********************************************************************/
inline void sdcTableCTL::incFSCreditOfCTS( sdpCTS   * aCTS,
                                           UShort     aFSCredit )
{
   IDE_DASSERT( aCTS != NULL );
   aCTS->mFSCredit += aFSCredit;

   IDE_ASSERT( aCTS->mFSCredit < SD_PAGE_SIZE );
}

/***********************************************************************
 *
 * Description : CTS�� Free Space Credit ���� ���ҽ�Ŵ
 *
 * Ʈ������� �ѹ��� ����Ͽ� Ʈ����� �Ϸ������� �ݵ�� Ȯ���� �ξ�� �ϴ�
 * ������ ��������� �����Ѹ�ŭ CTS�� Free Space Credit�� ���ش�.
 *
 * Ʈ������� �ѹ��� �ϰų� Ŀ�����Ŀ� RowStamping �������� Free Space Credit��
 * �����ȴ�.
 *
 * aCTS       - [IN] CTS ������
 * aFSCredit  - [IN] ���ҽ�ų FreeSpaceCredit ũ�� (>0)
 *
 ***********************************************************************/
inline void sdcTableCTL::decFSCreditOfCTS( sdpCTS  * aCTS,
                                           UShort    aFSCredit )
{
    IDE_DASSERT( aCTS != NULL );

    IDE_ASSERT( aCTS->mFSCredit >= aFSCredit );
    aCTS->mFSCredit -= aFSCredit;
}

/***********************************************************************
 *
 * Description : CTL�� Delete���� Ȥ�� Delete�� Row Piece�� ������ ����
 *
 * �ش� ����Ÿ �������� Self-Aging�� �ʿ������� �𸣴� Deleted Row
 * Piece ������ ������Ų��
 *
 * aCTL       - [IN] CTL ������
 *
 ***********************************************************************/
inline void sdcTableCTL::incDelRowCntOfCTL( sdpCTL * aCTL )
{
    IDE_DASSERT( aCTL != NULL );
    aCTL->mDelRowCnt += 1;
}

/***********************************************************************
 *
 * Description : CTL�� Delete���� Ȥ�� Delete�� Row Piece�� ������ ����
 *
 * �ش� ����Ÿ �������� Self-Aging�� ó���ϰ� ���� Ȥ�� Delete ������ �ѹ��ϴ� ��쿡
 * Piece ������ ���ҽ�Ų��.
 *
 * aCTL       - [IN] CTL ������
 *
 ***********************************************************************/
inline void sdcTableCTL::decDelRowCntOfCTL( sdpCTL * aCTL )
{
    IDE_DASSERT( aCTL->mDelRowCnt > 0 );
    aCTL->mDelRowCnt -= 1;
}

/***********************************************************************
 *
 * Description : CTL�� ���ε� �� CTS�� ������ 1������Ų��.
 *
 * aCTL       - [IN] CTL ������
 *
 ************************************************************************/
inline void sdcTableCTL::incBindCTSCntOfCTL( sdpCTL * aCTL )
{
    IDE_DASSERT( aCTL != NULL );
    aCTL->mBindCTSCnt++;
}

/***********************************************************************
 *
 * Description : CTL�� ���ε� �� CTS�� ������ 1���ҽ�Ų��.
 *
 * aCTL       - [IN] CTL ������
 * aCTS       - [IN] CTS ������
 *
 ************************************************************************/
inline void sdcTableCTL::decBindCTSCntOfCTL( sdpCTL * aCTL,
                                             sdpCTS * aCTS )
{
    IDE_DASSERT( aCTL != NULL );
    IDE_DASSERT( aCTS != NULL );

    IDE_ASSERT( aCTL->mBindCTSCnt > 0 );

    if ( aCTS->mRefCnt == 0 )
    {
        if ( hasState( aCTS->mStat, SDP_CTS_STAT_ACT ) == ID_TRUE )
        {
            /* Unbind CTS */
            aCTS->mStat = SDP_CTS_STAT_ROL;
            IDE_ASSERT( aCTS->mFSCredit == 0 );
        }
        else
        {
            /* Run row timestamping */
            IDE_ASSERT( hasState( aCTS->mStat, SDP_CTS_STAT_RTS )
                        == ID_TRUE );
        }

        aCTL->mBindCTSCnt--;
    }
}

/***********************************************************************
 *
 * Description : CTL�� Row�� ���ε� �� CTS�� ������ 1������Ų��.
 *
 * aCTL       - [IN] CTL ������
 *
 ************************************************************************/
inline void sdcTableCTL::incBindRowCTSCntOfCTL( sdpCTL * aCTL )
{
    IDE_DASSERT( aCTL != NULL );
    aCTL->mRowBindCTSCnt++;
}

/***********************************************************************
 *
 * Description : CTL�� Row���ε� �� CTS�� ������ 1���ҽ�Ų��.
 *
 * aCTL       - [IN] CTL ������
 * aCTS       - [IN] CTS ������
 *
 ************************************************************************/
inline void sdcTableCTL::decBindRowCTSCntOfCTL( sdpCTL * aCTL )
{
    IDE_DASSERT( aCTL != NULL );

    IDE_ASSERT( aCTL->mRowBindCTSCnt > 0 );
    aCTL->mRowBindCTSCnt--;
}

# endif // _O_SDC_TABLE_CTL_H_
