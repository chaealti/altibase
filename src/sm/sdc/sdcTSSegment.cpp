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
 * $Id: sdcTSSegment.cpp 91026 2021-06-21 00:23:00Z emlee $
 *
 * Description :
 *
 * �� ������ Transaction Status Slot Segment ���������Դϴ�.
 *
 **********************************************************************/

# include <smu.h>
# include <sdr.h>
# include <sdp.h>
# include <sdb.h>
# include <smx.h>

# include <sdcDef.h>
# include <sdcTSSegment.h>
# include <sdcTSSlot.h>
# include <sdcTXSegFreeList.h>
# include <sdcReq.h>

/***********************************************************************
 *
 * Description : TSS Segment �Ҵ�
 *
 * Undo Tablespace�� TSS Segment�� �����ϰ�,
 * 0���������� TSS Segment ������ �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::create( idvSQL          * aStatistics,
                             sdrMtxStartInfo * aStartInfo,
                             scPageID        * aTSSegPID )
{
    sdrMtx            sMtx;
    sdpSegMgmtOp    * sSegMgmtOP;
    UInt              sState = 0;
    sdpSegmentDesc    sTssSegDesc;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aTSSegPID  != NULL );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    sdpSegDescMgr::setDefaultSegAttr(
                                  &(sTssSegDesc.mSegHandle.mSegAttr),
                                  SDP_SEG_TYPE_TSS );
    sdpSegDescMgr::setDefaultSegStoAttr(
                                  &(sTssSegDesc.mSegHandle.mSegStoAttr) );

    IDE_TEST( sdpSegDescMgr::initSegDesc(
                                  &sTssSegDesc,
                                  SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                  SD_NULL_RID,           /* Before create a segment. */
                                  SDP_SEG_TYPE_TSS,
                                  SM_NULL_OID,
                                  SM_NULL_INDEX_ID ) != IDE_SUCCESS );

    IDE_ASSERT( sTssSegDesc.mSegMgmtType ==
                sdpTableSpace::getSegMgmtType( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO ) );

    sSegMgmtOP = sdpSegDescMgr::getSegMgmtOp( &sTssSegDesc );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOP != NULL );

    IDE_TEST( sSegMgmtOP->mCreateSegment( aStatistics,
                                          &sMtx,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          SDP_SEG_TYPE_TSS,
                                          &sTssSegDesc.mSegHandle )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    *aTSSegPID  = sdpSegDescMgr::getSegPID( &sTssSegDesc );

    IDE_TEST( sdpSegDescMgr::destSegDesc( &sTssSegDesc ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    *aTSSegPID = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : TSS ���׸�Ʈ �ʱ�ȭ
 *
 * �ý��� ������ undo tablespace meta page(0��) �������� �ǵ��Ͽ�
 * tss segment ������ sdcTSSegment�� �����Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aEntry       - [IN] �ڽ��� �Ҽӵ� Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 * aTSSegPID    - [IN] TSS ���׸�Ʈ ��� ������ ID
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::initialize( idvSQL        * aStatistics,
                                 sdcTXSegEntry * aEntry,
                                 scPageID        aTSSegPID )
{
    sdpSegInfo     sSegInfo;
    sdpSegMgmtOp * sSegMgmtOp;

    IDE_ASSERT( aEntry != NULL );
    IDE_ASSERT( aEntry->mEntryIdx < SDP_MAX_UDSEG_PID_CNT );

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    /* Undo Tablespace�� �ý��ۿ� ���ؼ� �ڵ����� �����ǹǷ�
     * Segment Storage Parameter ���� ��� �����Ѵ� */
    sdpSegDescMgr::setDefaultSegAttr(
            sdpSegDescMgr::getSegAttr( &mTSSegDesc),
            SDP_SEG_TYPE_TSS );

    sdpSegDescMgr::setDefaultSegStoAttr(
            sdpSegDescMgr::getSegStoAttr( &mTSSegDesc ));

    IDE_TEST( sdpSegDescMgr::initSegDesc( &mTSSegDesc,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          aTSSegPID,
                                          SDP_SEG_TYPE_TSS,
                                          SM_NULL_OID, 
                                          SM_NULL_INDEX_ID )
            != IDE_SUCCESS );

    mSlotSize       = ID_SIZEOF(sdcTSS);
    mAlignSlotSize  = idlOS::align8(mSlotSize);
    mEntryPtr       = aEntry;
    /* BUG-40014 [PROJ-2506] Insure++ Warning
     * - ��� ������ �ʱ�ȭ�� �ʿ��մϴ�.
     */
    mCurAllocExtRID = ID_ULONG_MAX;
    mFstPIDOfCurAllocExt = ID_UINT_MAX;
    mCurAllocPID = ID_UINT_MAX;

    IDE_TEST( sSegMgmtOp->mGetSegInfo( 
                           aStatistics,
                           SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                           aTSSegPID,
                           NULL, /* aTableHeader */
                           &sSegInfo ) != IDE_SUCCESS );

    setCurAllocInfo( sSegInfo.mFstExtRID,
                     sSegInfo.mFstPIDOfLstAllocExt,
                     SD_NULL_PID );  // CurAllocPID �ʱ�ȭ

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : TSS ���׸�Ʈ ����� ����
 *
 * ���׸�Ʈ ����ڿ��� ��Ÿ�� �޸� �ڷᱸ���� �Ҵ�Ǿ�
 * �ֱ� ������ �޸� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::destroy()
{
    IDE_ASSERT( sdpSegDescMgr::destSegDesc( &mTSSegDesc )
                == IDE_SUCCESS );

    return IDE_SUCCESS;
}


/***********************************************************************
 * Description : Transaction Status Slot �Ҵ�
 *
 * pending ������ �����ؾ��ϴ� Ʈ������̳� dml�� �����ϴ� Ʈ����ǿ�
 * ���ؼ��� tsslot�� �Ҵ��Ѵ�.
 *
 * *********************************************************************/
IDE_RC sdcTSSegment::bindTSS( idvSQL          * aStatistics,
                              sdrMtxStartInfo * aStartInfo )
{
    UInt                 sState = 0;
    sdrMtx               sMtx;
    UChar              * sSlotPtr = NULL;
    UShort               sSlotOffset;
    UShort               sSlotSize;
    sdrSavePoint         sSvp;
    sdSID                sSlotSID;
    sdpPhyPageHdr      * sPageHdr;
    UChar              * sNewCurPagePtr;
    UChar              * sSlotDirPtr;
    idBool               sIsPageCreate;
    void               * sTrans;
    smTID                sTransID;
    sdcTSSPageCntlHdr  * sCntlHdr;
    smSCN                sFstDskViewSCN;
    scSlotNum            sNewSlotNum;

    IDE_ASSERT( aStartInfo != NULL );

    sTrans         = aStartInfo->mTrans;
    sTransID       = smxTrans::getTransID( sTrans );
    sFstDskViewSCN = smxTrans::getFstDskViewSCN( sTrans );

    IDV_SQL_OPTIME_BEGIN( aStatistics,
                          IDV_OPTM_INDEX_DRDB_DML_ALLOC_TSS );

    sPageHdr         = NULL;
    sIsPageCreate    = ID_FALSE;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    if ( mCurAllocPID != SD_NULL_PID )
    {
        sdrMiniTrans::setSavePoint( &sMtx, &sSvp );

        IDE_TEST( getCurPID4Update( aStatistics,
                                    &sMtx,
                                    &sNewCurPagePtr ) != IDE_SUCCESS );

        sPageHdr = sdpPhyPage::getHdr( sNewCurPagePtr );

        if ( sdpPhyPage::canAllocSlot( sPageHdr,
                                       mAlignSlotSize,
                                       ID_TRUE, /* aIsNeedSlotEntry */
                                       SDP_8BYTE_ALIGN_SIZE )
             != ID_TRUE ) // can't alloc
        {
            sdrMiniTrans::releaseLatchToSP( &sMtx, &sSvp );

            sIsPageCreate = ID_TRUE;
        }
        else
        {
            sIsPageCreate = ID_FALSE;
        }
    }
    else
    {
        sIsPageCreate = ID_TRUE;
    }

    if ( sIsPageCreate == ID_TRUE )
    {
        IDE_TEST( allocNewPage( aStatistics,
                                &sMtx,
                                sTransID,
                                sFstDskViewSCN,
                                &sNewCurPagePtr ) != IDE_SUCCESS );
    }

    sPageHdr = sdpPhyPage::getHdr( sNewCurPagePtr );
    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sNewCurPagePtr);
    sNewSlotNum = sdpSlotDirectory::getCount(sSlotDirPtr);

    IDE_TEST( sdpPhyPage::allocSlot( sPageHdr,
                                     sNewSlotNum,
                                     (UShort)mAlignSlotSize,
                                     ID_TRUE, // aNeedSlotEntry
                                     &sSlotSize,
                                     &sSlotPtr,
                                     &sSlotOffset,
                                     SDP_8BYTE_ALIGN_SIZE )
              != IDE_SUCCESS );
    sState = 2;

    sCntlHdr = (sdcTSSPageCntlHdr*)
                sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sPageHdr );

    sSlotSID = SD_MAKE_SID( sPageHdr->mPageID, sNewSlotNum );

    IDE_TEST( sdcTSSlot::logAndInit( &sMtx,
                                     (sdcTSS*)sSlotPtr,
                                     sSlotSID,
                                     mCurAllocExtRID,
                                     sTransID,
                                     mEntryPtr->mEntryIdx ) 
              != IDE_SUCCESS );

    // To fix BUG-23687
    if( SM_SCN_IS_LT( &sFstDskViewSCN, &sCntlHdr->mFstDskViewSCN ) )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( &sMtx,
                                             (UChar*)&sCntlHdr->mTransID,
                                             (void*)&sTransID,
                                             ID_SIZEOF( sTransID ) )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes( &sMtx,
                                             (UChar*)&sCntlHdr->mFstDskViewSCN,
                                             (void*)&sFstDskViewSCN,
                                             ID_SIZEOF( smSCN ) )
                  != IDE_SUCCESS );
    }

    ((smxTrans*)sTrans)->setTSSAllocPos( mCurAllocExtRID, sSlotSID );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx,
                                    SMR_CT_END, // aContType
                                    NULL,       // aEndLSN
                                    SMR_RT_WITHMEM ) // aRedoType
              != IDE_SUCCESS );

    IDV_SQL_OPTIME_END( aStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_ALLOC_TSS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 2:
            (void) sdpPhyPage::freeSlot( sPageHdr,
                                         sSlotPtr,
                                         mAlignSlotSize,
                                         ID_TRUE, // aIsFreeSlotEntry
                                         SDP_8BYTE_ALIGN_SIZE );
        case 1:
            IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );

        default:
            break;
    }

    IDV_SQL_OPTIME_END( aStatistics,
                        IDV_OPTM_INDEX_DRDB_DML_ALLOC_TSS );

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : TSS Page�� CntlHdr �ʱ�ȭ �� �α�
 *
 * TSS Page Control Header�� �ֱ� ����� Ʈ������� ������ �����Ͽ�
 * TSS Page�� ����Ǿ����� �Ǵ��� �� �ְ� �Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aPagePtr       - [IN] �Ҵ��� ������ ������
 * aTransID       - [IN] �������� �Ҵ��� Ʈ����� ID
 * aFstDskViewSCN - [IN] �������� �Ҵ��� Ʈ������� Begin SCN
 *
 **********************************************************************/
IDE_RC sdcTSSegment::logAndInitPage( sdrMtx      * aMtx,
                                     UChar       * aPagePtr,
                                     smTID         aTransID,
                                     smSCN         aFstDskViewSCN )
{
    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aPagePtr != NULL );

    initPage( aPagePtr, aTransID, aFstDskViewSCN );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPagePtr,
                                         NULL, /* aValue */
                                         ID_SIZEOF( aTransID ) +
                                         ID_SIZEOF( smSCN ),
                                         SDR_SDC_INIT_TSS_PAGE )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aTransID,
                                   ID_SIZEOF( aTransID ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &aFstDskViewSCN,
                                   ID_SIZEOF( smSCN ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : TSS�� �Ҵ��� ExtDir�� CntlHdr�� SCN�� ����
 *
 * unbind TSS �������� CommitSCN Ȥ�� AbortSCN �� ExtDir �������� ����Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceID    - [IN] ���̺����̽� ID
 * aSegPID     - [IN] TSS�� Segment PID
 * aFstExtRID  - [IN] Ʈ������� ����� ù��° ExtRID
 * aCSCNorASCN - [IN] ������ CommitSCN Ȥ�� AbortSCN
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::markSCN4ReCycle( idvSQL   * aStatistics,
                                      scSpaceID  aSpaceID,
                                      scPageID   aSegPID,
                                      sdRID      aFstExtRID,
                                      smSCN    * aCSCNorASCN )
{
    sdpSegMgmtOp   * sSegMgmtOp;

    IDE_ASSERT( aSegPID     != SD_NULL_PID );
    IDE_ASSERT( aFstExtRID  != SD_NULL_RID );
    IDE_ASSERT( aCSCNorASCN != NULL );
    IDE_ASSERT( SM_SCN_IS_NOT_INFINITE( *aCSCNorASCN ) );
    IDE_ASSERT( SM_SCN_IS_NOT_INIT( *aCSCNorASCN ) );

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( &mTSSegDesc );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_TEST( sSegMgmtOp->mMarkSCN4ReCycle( aStatistics,
                                            aSpaceID,
                                            aSegPID,
                                            getSegHandle(),
                                            aFstExtRID,
                                            aFstExtRID,
                                            aCSCNorASCN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Unbinding TSS For Commit Trans
 *
 * ��ũ ���� Ʈ����ǿ� ���� Commit �۾��� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::unbindTSS4Commit( idvSQL  * aStatistics,
                                       sdSID     aTSSlotSID,
                                       smSCN   * aCommitSCN )
{
    sdrMtx      sMtx;
    sdcTSS    * sTSS;
    idBool      sTrySuccess;
    UInt        sState = 0;
    UInt        sLogSize;
    sdcTSState  sTSState = SDC_TSS_STATE_COMMIT;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   (void*)NULL,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DUMMY )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdbBufferMgr::getPageBySID( aStatistics,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          aTSSlotSID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          &sMtx,
                                          (UChar**)&sTSS,
                                          &sTrySuccess ) != IDE_SUCCESS );

    IDE_ASSERT( sdpPhyPage::getPageType( sdpPhyPage::getHdr((UChar*)sTSS) )
                == SDP_PAGE_TSS );

    sLogSize = ID_SIZEOF(smSCN) +
               ID_SIZEOF(sdcTSState);

    sdcTSSlot::unbind( sTSS, aCommitSCN, sTSState );

    IDE_TEST( sdrMiniTrans::writeLogRec(
                                   &sMtx,
                                   (UChar*)sTSS,
                                   NULL,
                                   sLogSize,
                                   SDR_SDC_UNBIND_TSS ) 
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   aCommitSCN,
                                   ID_SIZEOF(smSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   &sTSState,
                                   ID_SIZEOF(sdcTSState) )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Unbinding TSS For Transaction Abort
 *
 * ��ũ ���� Ʈ����ǿ� ���� Abort �۾��� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::unbindTSS4Abort( idvSQL  * aStatistics,
                                      sdrMtx  * aMtx,
                                      sdSID     aTSSlotSID,
                                      smSCN   * aInitSCN )
{
    sdcTSS             * sTSSlotPtr;
    idBool               sTrySuccess;
    UInt                 sLogSize;
    sdcTSState           sState = SDC_TSS_STATE_ABORT;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aInitSCN != NULL );
    IDE_ASSERT( aTSSlotSID != SD_NULL_SID );

    IDE_TEST( sdbBufferMgr::getPageBySID(
                  aStatistics,
                  SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                  aTSSlotSID,
                  SDB_X_LATCH,
                  SDB_WAIT_NORMAL,
                  aMtx,
                  (UChar**)&sTSSlotPtr,
                  &sTrySuccess ) != IDE_SUCCESS );

    sLogSize = ID_SIZEOF(smSCN) +
               ID_SIZEOF(sdcTSState);

    sdcTSSlot::unbind( sTSSlotPtr,
                       aInitSCN /* aSCN */,
                       SDC_TSS_STATE_ABORT );

    IDE_TEST( sdrMiniTrans::writeLogRec(
                  aMtx,
                  (UChar*)sTSSlotPtr,
                  NULL,
                  sLogSize,
                  SDR_SDC_UNBIND_TSS ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   aInitSCN,
                                   ID_SIZEOF(smSCN) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   &sState,
                                   ID_SIZEOF(sdcTSState) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : TSS�� ���� ���뿩�ο� CommitSCN ��ȯ
 *
 * Ŀ�Ե� TSS�� ���ؼ��� Ư�������� ������ TSS�� ������ �����ϴ�. ������ ���� �������
 * �ٸ� Ʈ������� TSS�� �湮�Ͽ� CommitSCN�� �Ҵ��ϴٰ� �Ҷ�, ��Ȯ�� ���� ���� �� ������,
 * ����� ��쿡�� Unbound CommitSCN(0x0000000000000000)�� ���� �� �ۿ� ����.
 * Unbound CommitSCN�� RowPiece�� ��� Statment�� ��� ���� �ְų�
 * ������ �� �ֵ��� ���ش�.
 * ����, ���� Ŀ�Ե��� ���� Ʈ������� TSS��� CommitSCN�� �����Ƿ�, TSS�� ������
 * Ʈ����� BeginSCN�� ��ȯ�Ѵ�.
 *
 * aStatistics     - [IN]  �������
 * aTrans          - [IN]  Ʈ����� ���� 
 * aTSSlotRID      - [IN]  CTS Ȥ�� RowEx�� ��ϵ� TSS�� RID
 * aFstDskViewSCN  - [IN]  CTS Ȥ�� RowEx�� ��ϵ� Ʈ������� FstDskViewSCN ( �̰ų� CommitSCN �ε� commit �� ��ϵǾ����� ���� �ȿ� )
 * aStmtViewSCN    - [IN]  �ùٸ� ������ row�� �б� ���� Statement viewscn
 * aTID4Wait       - [OUT] ���� TSS�� ������ TX�� Active ������ ��� �ش� Ʈ������� TID
 * aCommitSCN      - [OUT] Ʈ������� CommitSCN�̳� Unbound CommitSCN Ȥ�� TSS��
 *                         ������ Ʈ������� BeginSCN�ϼ� �ִ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::getCommitSCN( idvSQL      * aStatistics,
                                   void        * aTrans,
                                   sdSID         aTSSlotSID,
                                   smSCN       * aFstDskViewSCN,
                                   smSCN         aStmtViewSCN,
                                   smTID       * aTID4Wait,
                                   smSCN       * aCommitSCN )
{
    UChar               * sPagePtr    = NULL;
    sdpSlotDirHdr       * sSlotDirHdr = NULL;
    sdcTSS              * sTSS;
    sdcTSState            sTSState;
    smTID                 sTransID;
    sdcTSSPageCntlHdr   * sCntlHdr;
    sdpPageType           sPageType;
    smSCN                 sMinOldestFstViewSCN;
    smSCN                 sCommitSCN;
    idBool                sTrySuccess;
    UInt                  sState = 0;

    IDE_ASSERT( aTSSlotSID != SD_NULL_SID );
    IDE_DASSERT( ( SM_SCN_IS_VIEWSCN(*aFstDskViewSCN)   ) ||
                 ( SM_SCN_IS_COMMITSCN(*aFstDskViewSCN) ) ); //BUG-49079 validateDRDBRedo

    sTransID = SM_NULL_TID;
    SM_INIT_SCN( &sCommitSCN );

    /*
     * CTS�� ���ε��� Ʈ������� BeginSCN�� System MinDskFstSCN�� Ȯ���غ���
     * ���뿩�θ� Ȯ���Ѵ�.
     *
     * ������ ���� ���⼭ �Ǵ��Ѵٰ� �ؼ� ����ص� TSSlotRID�� getPage�ϱ�����
     * �������� �ٸ�Ʈ����ǿ� ���ؼ� ����ɼ� �ִٴ� ���� �˾ƾ� �Ѵ�.
     * ��, �Ǵܽÿ� System Agable SCN���� aTransFstSCN�� Ŀ�� ����ص�
     * getPage�ϱ����� System Agable SCN�� �����Ͽ� aTransFstSCN���� Ŀ����
     * �ִ�. �׷��Ƿ� BufferPool���� CreatePage�� GetPage�� ������ ��������
     * ���ؼ� �浹�� �� �ִ�. �̷��� ������ TSS ������������ �߻��Ѵ�.
     *
     * ���⼭ �Ǵ��ϴ� ���� ��Ȯ�ϰ� �ϱ� ���ٴ� �ѹ� �� üũ�ؼ� I/O��
     * �ٿ����ڴ� ���̴�.
     */
    // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
    // active Ʈ����ǵ� �� ���� ������ OldestDskFstViewSCN�� ����
    smxTransMgr::getMinOldestFstViewSCN( &sMinOldestFstViewSCN );

    // OldestDskFstViewSCN���� CTS�� SCN�� ������ �̹� �Ϸ�� Ʈ������̹Ƿ�
    // TSS�� �� �ʿ� ����.
    IDE_TEST_CONT( SM_SCN_IS_LT( aFstDskViewSCN, &sMinOldestFstViewSCN )
                    == ID_TRUE, CONT_ALREADY_FINISHED_TRANS );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          SD_MAKE_PID(aTSSlotSID),
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* aMtx */
                                          &sPagePtr,
                                          &sTrySuccess,
                                          NULL )
              != IDE_SUCCESS);
    sState = 1;

    /*
     * Page�� S-Latch�� ȹ���� ���¿��� SysOldestDskFstViewSCN�� üũ�غ���.
     */

    // BUG-26881 �߸��� CTS stamping���� acces�� �� ���� row�� ������
    // active Ʈ����ǵ� �� ���� ������ OldestDskFstViewSCN�� ����
    smxTransMgr::getMinOldestFstViewSCN( &sMinOldestFstViewSCN );

    // OldestDskFstViewSCN���� CTS�� SCN�� ������ �̹� �Ϸ�� Ʈ������̹Ƿ�
    // TSS�� �� �ʿ� ����.
    IDE_TEST_CONT( SM_SCN_IS_LT( aFstDskViewSCN, &sMinOldestFstViewSCN )
                    == ID_TRUE, CONT_ALREADY_FINISHED_TRANS );

    IDU_FIT_POINT( "sdcTSSegment::getCommitSCN::getPageByPID" );
    sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );

    IDE_TEST_CONT( sPageType != SDP_PAGE_TSS,
                    CONT_ALREADY_FINISHED_TRANS );

    /* BUG-32650 TSSlot�� �̹� free�Ǿ��� ���� �ֽ��ϴ�.
     * ���� Slot Number�� Slot Entry Count���� ũ��
     * �̹� �Ϸ�� Transaction�̴�. */
    sSlotDirHdr = (sdpSlotDirHdr*)sdpPhyPage::getSlotDirStartPtr( sPagePtr );

    IDE_TEST_CONT( ( SD_MAKE_SLOTNUM( aTSSlotSID )>= sSlotDirHdr->mSlotEntryCount ),
                    CONT_ALREADY_FINISHED_TRANS );

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                (UChar*)sSlotDirHdr,
                                                SD_MAKE_SLOTNUM( aTSSlotSID ),
                                                (UChar**)&sTSS )
              != IDE_SUCCESS );

    sCntlHdr = (sdcTSSPageCntlHdr*)
                                sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sTSS );

    if( SM_SCN_IS_LT( aFstDskViewSCN, &sCntlHdr->mFstDskViewSCN ) )
    {
        IDE_CONT( CONT_ALREADY_FINISHED_TRANS );
    }

    sdcTSSlot::getTransInfo( sTSS, &sTransID, &sCommitSCN );
    sTSState = sdcTSSlot::getState( sTSS );

    if ( ( sTSState != SDC_TSS_STATE_ABORT ) &&
         ( SM_SCN_IS_INFINITE( sCommitSCN ) ) &&
         ( SM_SCN_IS_NOT_INIT( aStmtViewSCN ) ) &&
         ( aTrans!= NULL ) &&
         ( ((smxTrans *)aTrans)->mIsGCTx == ID_TRUE ) )
    {
        /* BUG-48587 : waitPendingTx���� ����Ҽ������Ƿ� TSS Page Latch�� �ϴ�Ǭ��. */
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                != IDE_SUCCESS );

        IDE_TEST( smxTrans::waitPendingTx( (smxTrans*)aTrans,
                                           (sCommitSCN | sTransID), /* aRowSCN */
                                           aStmtViewSCN )           /* aViewSCN */
                  != IDE_SUCCESS );

        /* BUG-48587 
           TSS Page Latch�� Ǯ�� wait �ϴ��߿� TSS�� ���ο���� ����ɼ��ִ�.
           TSS Page�� �ٽ� ��� ���ο� ���� �����´�. */ 
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                              SD_MAKE_PID(aTSSlotSID),
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              NULL, /* aMtx */
                                              &sPagePtr,
                                              &sTrySuccess,
                                              NULL )
                  != IDE_SUCCESS );
        sState = 1;

        smxTransMgr::getMinOldestFstViewSCN( &sMinOldestFstViewSCN );

        IDE_TEST_CONT( SM_SCN_IS_LT( aFstDskViewSCN, &sMinOldestFstViewSCN )
                       == ID_TRUE, CONT_ALREADY_FINISHED_TRANS );

        sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );

        IDE_TEST_CONT( sPageType != SDP_PAGE_TSS,
                       CONT_ALREADY_FINISHED_TRANS );

        sSlotDirHdr = (sdpSlotDirHdr *)sdpPhyPage::getSlotDirStartPtr( sPagePtr );

        IDE_TEST_CONT( ( SD_MAKE_SLOTNUM( aTSSlotSID )>= sSlotDirHdr->mSlotEntryCount ),
                       CONT_ALREADY_FINISHED_TRANS );

        IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( (UChar*)sSlotDirHdr,
                                                           SD_MAKE_SLOTNUM( aTSSlotSID ),
                                                           (UChar**)&sTSS )
                  != IDE_SUCCESS );

        sCntlHdr = (sdcTSSPageCntlHdr*)sdpPhyPage::getLogicalHdrStartPtr( (UChar*)sTSS );

        if ( SM_SCN_IS_LT( aFstDskViewSCN, &sCntlHdr->mFstDskViewSCN ) )
        {
            IDE_CONT( CONT_ALREADY_FINISHED_TRANS );
        }

        sdcTSSlot::getTransInfo( sTSS, &sTransID, &sCommitSCN );
        sTSState = sdcTSSlot::getState( sTSS );
    }

    // BUG-31504: During the cached row's rollback, it can be read.
    // �̹� abort�� Ʈ������� CommitSCN�� Ȯ���� �ʿ䰡 ����.
    if ( sTSState != SDC_TSS_STATE_ABORT )
    {
        if ( SM_SCN_IS_INFINITE( sCommitSCN ) )
        {
            // [4-2] ���Ѵ��ӿ��� transaction�� commit�� ��찡 �ִ�.

            // �ֳ��ϸ� commit ������
            // 1. system SCN ����
            // 2. TSS�� commit SCN set

            // �׷��� 2�� �̻��� thread�� 1,2���̿� ���� �鶧
            // �� system SCN�� ������ ���¿��� stmt begin�ϰ�,
            // ���� TSS commit�� �ȹ��� ���¿��� record�� ���� TSS�� ���󰡸�
            // �� trans�� commit�� �Ǿ������� �ұ��ϰ� TSS slot�� ����
            // commit �ȵ� ���°� �� �� �ִ�.
            // �̷� ���� getValidVersion�� �߸��� ���ڵ带 ���� �� �ִ�.
            // �̸� �����ϱ� ���� A3�� �ϴ� �Ͱ� ����������
            // trans�� commit���� ���� ���¶� ������
            // TID�� ������ trans�� commit SCN�� ������ Ȯ���� ���� �Ѵ�.
            // �� ���� ���Ѵ밡 �ƴϸ� commit�Ȱ����� �˼� �ִ�.

            smxTrans::getTransCommitSCN( sTransID,
                                         &sCommitSCN, /* IN */
                                         &sCommitSCN  /* OUT */ );
        }
    }
    else
    {
        IDE_ASSERT( SM_SCN_IS_INFINITE(sCommitSCN) );
    }

    IDE_EXCEPTION_CONT( CONT_ALREADY_FINISHED_TRANS );

    if ( sState != 0 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                  != IDE_SUCCESS );
    }

    /* sCommitSCN�� Commit�� �Ǿ����� ������ ���̰�,
     * Active ���¶�� ���Ѵ밪�̴�. */
    if ( SM_SCN_IS_COMMITSCN( sCommitSCN ) )
    {
        /* TSS �� CommitSCN ����Ҷ� TransID �� ������ ���� (sdcTSSlot::unbind) */
        sTransID = SM_NULL_TID;
    }

    *aTID4Wait = sTransID;
    SM_SET_SCN( aCommitSCN, &sCommitSCN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : X$TSSEG�� ���ڵ带 �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::build4SegmentPerfV( void                * aHeader,
                                         iduFixedTableMemory * aMemory )
{
    sdpSegInfo           sSegInfo;
    sdcTSSegInfo         sTSSegInfo;
    sdpSegMgmtOp       * sSegMgmtOp;

    sTSSegInfo.mSegPID          = mTSSegDesc.mSegHandle.mSegPID;
    sTSSegInfo.mTXSegID         = mEntryPtr->mEntryIdx;

    sTSSegInfo.mCurAllocExtRID  = mCurAllocExtRID;
    sTSSegInfo.mCurAllocPID     = mCurAllocPID;

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_TEST_RAISE( sSegMgmtOp->mGetSegInfo(
                                    NULL,
                                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                    mTSSegDesc.mSegHandle.mSegPID,
                                    NULL, /* aTableHeader */
                                    &sSegInfo ) != IDE_SUCCESS,
                    ERR_INVALID_DUMP_OBJECT );

    sTSSegInfo.mSpaceID      = SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO;
    sTSSegInfo.mType         = sSegInfo.mType;
    sTSSegInfo.mState        = sSegInfo.mState;
    sTSSegInfo.mPageCntInExt = sSegInfo.mPageCntInExt;
    sTSSegInfo.mTotExtCnt    = sSegInfo.mExtCnt;
    sTSSegInfo.mTotExtDirCnt = sSegInfo.mExtDirCnt;

    IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                          aMemory,
                                          (void *)&sTSSegInfo )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_DUMP_OBJECT );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotExistSegment ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : X$DISK_TSS_RECORDS�� ���ڵ带 �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTSSegment::build4RecordPerfV( UInt                  aSegSeq,
                                        void                * aHeader,
                                        iduFixedTableMemory * aMemory )
{
    sdRID                 sCurExtRID;
    UInt                  sState = 0;
    sdpSegMgmtOp        * sSegMgmtOp;
    sdpSegInfo            sSegInfo;
    sdpExtInfo            sExtInfo;
    scPageID              sCurPageID;
    UChar               * sCurPagePtr;
    UChar               * sSlotDirPtr;
    sdcTSS4FT             sTSS4FT;
    idBool                sIsSuccess;
    SInt                  sSlotSeq;
    SInt                  sSlotCnt;
    sdcTSS              * sTSSlotPtr;
    SChar                 sStrCSCN[ SM_SCN_STRING_LENGTH + 1 ];

    //------------------------------------------
    // Parameter Validation
    //------------------------------------------

    IDE_ERROR( aHeader != NULL );
    IDE_ERROR( aMemory != NULL );

    /* ���׸�Ʈ�� ���µ� ��� mCurAllocPID�� SD_NULL_PID�̴�. */
    IDE_TEST_CONT( mCurAllocPID == SD_NULL_PID, CONT_NO_TSS_PAGE ); 

    //------------------------------------------
    // Get Segment Info
    //------------------------------------------
    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(
                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_ASSERT( sSegMgmtOp->mGetSegInfo( NULL,
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         mTSSegDesc.mSegHandle.mSegPID,
                                         NULL, /* aTableHeader */
                                         &sSegInfo )
                == IDE_SUCCESS );

    sCurExtRID = sSegInfo.mFstExtRID;

    IDE_TEST( sSegMgmtOp->mGetExtInfo( NULL,
                                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       sCurExtRID,
                                       &sExtInfo )
              != IDE_SUCCESS );

    sCurPageID = SD_NULL_PID;

    IDE_TEST( sSegMgmtOp->mGetNxtAllocPage( NULL, /* idvSQL */
                                            SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                            &sSegInfo,
                                            NULL,
                                            &sCurExtRID,
                                            &sExtInfo,
                                            &sCurPageID )
              != IDE_SUCCESS );

    sTSS4FT.mSegSeq = aSegSeq;
    sTSS4FT.mSegPID = mTSSegDesc.mSegHandle.mSegPID;

    while( sCurPageID != SD_NULL_PID )
    {
        IDE_TEST( sdbBufferMgr::getPage( NULL, /* idvSQL */
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         sCurPageID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         (UChar**)&sCurPagePtr,
                                         &sIsSuccess ) != IDE_SUCCESS );
        sState = 1;

        IDE_ASSERT( sdpPhyPage::getHdr( sCurPagePtr )->mPageType
                    == SDP_PAGE_TSS );

        sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sCurPagePtr);
        sSlotCnt    = sdpSlotDirectory::getCount(sSlotDirPtr);

        for( sSlotSeq = 0 ; sSlotSeq < sSlotCnt; sSlotSeq++ )
        {
            IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum( 
                                                        sSlotDirPtr,
                                                        sSlotSeq,
                                                        (UChar**)&sTSSlotPtr )
                      != IDE_SUCCESS );
            sTSS4FT.mPageID = sCurPageID;
            IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                  sSlotSeq,
                                                  &sTSS4FT.mOffset )
                      != IDE_SUCCESS );

            sTSS4FT.mNthSlot = sSlotSeq;
            sTSS4FT.mTransID = sTSSlotPtr->mTransID;

            idlOS::memset( sStrCSCN, 0x00, SM_SCN_STRING_LENGTH+1 );
            idlOS::snprintf( (SChar*)sStrCSCN, SM_SCN_STRING_LENGTH,
                             "%"ID_XINT64_FMT, sTSSlotPtr->mCommitSCN );
            sTSS4FT.mCSCN  = sStrCSCN;
            sTSS4FT.mState = sTSSlotPtr->mState;

            IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                                  aMemory,
                                                  (void *) &sTSS4FT )
                      != IDE_SUCCESS );
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( NULL, /* idvSQL */
                                             sCurPagePtr )
                  != IDE_SUCCESS );

        if ( sCurPageID == mCurAllocPID )
        {
            break;
        }

        IDE_TEST( sSegMgmtOp->mGetNxtAllocPage( NULL, /* idvSQL */
                                                SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                                &sSegInfo,
                                                NULL,
                                                &sCurExtRID,
                                                &sExtInfo,
                                                &sCurPageID )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // Finalize
    //------------------------------------------

    IDE_EXCEPTION_CONT( CONT_NO_TSS_PAGE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdbBufferMgr::releasePage( NULL, /* idvSQL */
                                               sCurPagePtr )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * TASK-4007 [SM]PBT�� ���� ��� �߰�
 * Description : page�� Dump�Ͽ� table�� ctl�� ����Ѵ�.
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�.
 ***********************************************************************/
IDE_RC sdcTSSegment::dump( UChar *aPage ,
                           SChar *aOutBuf ,
                           UInt   aOutSize )
{
    UChar         * sSlotDirPtr;
    sdpSlotEntry  * sSlotEntry;
    sdpSlotDirHdr * sSlotDirHdr;
    SInt            sSlotSeq;
    UShort          sOffset;
    UShort          sSlotCnt;
    sdcTSS        * sTSSlotPtr;

    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( sdpPhyPage::getPageStartPtr(aPage) == aPage );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr( aPage );
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
    sSlotCnt    = sSlotDirHdr->mSlotEntryCount;

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- TSS Record Begin ----------\n" );

    for( sSlotSeq = 0 ; sSlotSeq < sSlotCnt; sSlotSeq++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sSlotSeq);

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%3"ID_UINT32_FMT"[%04x]..............................\n",
                             sSlotSeq,
                             *sSlotEntry );

        if( SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) == ID_TRUE )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Unused slot\n");
        }

        sOffset = SDP_GET_OFFSET( sSlotEntry );
        if( SD_PAGE_SIZE < sOffset )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "Invalid slot offset\n" );
            continue;
        }

        sTSSlotPtr = 
            (sdcTSS*)sdpPhyPage::getPagePtrFromOffset( aPage, sOffset );

        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "mTransID            : %"ID_UINT32_FMT"\n"
                             "mState(Act,Com,Abo) : %"ID_UINT32_FMT"\n"
                             "mCommitSCN          : %"ID_UINT64_FMT"\n",
                             sTSSlotPtr->mTransID,
                             sTSSlotPtr->mState,
                             SM_SCN_TO_LONG( sTSSlotPtr->mCommitSCN ) );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- TSS Record End   ----------\n" );

    return IDE_SUCCESS;
}


/******************************************************************************
 *
 * Description : ������ ��� �������� �Ҵ��Ѵ�.
 *
 * �ش� ���׸�Ʈ�� �������� �ʴ´ٸ�, �������� ��Ʈ���κ��� ������
 * �ͽ���Ʈ Dir.�� ��´�.
 *
 * aStatistics     - [IN]  �������
 * aStartInfo      - [IN]  Mtx ��������
 * aTransID        - [IN]  Ʈ����� ID
 * aFstDskViewSCN  - [IN]  �������� �Ҵ��� Ʈ������� FstDskViewSCN
 * aPhyPagePtr     - [OUT] �Ҵ���� ������ ������
 *
 ******************************************************************************/
IDE_RC sdcTSSegment::allocNewPage( idvSQL  * aStatistics,
                                   sdrMtx  * aMtx,
                                   smTID     aTransID,
                                   smSCN     aFstDskViewSCN,
                                   UChar  ** aNewPagePtr )
{
    sdRID           sNewCurAllocExtRID;
    scPageID        sNewFstPIDOfCurAllocExt;
    scPageID        sNewCurAllocPID;
    UChar         * sNewCurPagePtr;
    smSCN           sSysMinDskViewSCN;
    sdrMtxStartInfo sStartInfo;
    SInt            sLoop;
    idBool          sTrySuccess = ID_FALSE;
    sdpSegHandle  * sSegHandle  = getSegHandle();

    IDE_ASSERT( aNewPagePtr != NULL );

    sdrMiniTrans::makeStartInfo(aMtx, &sStartInfo );

    for ( sLoop=0; sLoop < 2; sLoop++ )
    {
        sNewCurPagePtr = NULL;

        if ( mTSSegDesc.mSegMgmtOp->mAllocNewPage4Append(
                                         aStatistics,
                                         aMtx,
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         sSegHandle,
                                         mCurAllocExtRID,
                                         mFstPIDOfCurAllocExt,
                                         mCurAllocPID,
                                         ID_TRUE,     /* aIsLogging */
                                         SDP_PAGE_TSS,
                                         &sNewCurAllocExtRID,
                                         &sNewFstPIDOfCurAllocExt,
                                         &sNewCurAllocPID,
                                         &sNewCurPagePtr ) == IDE_SUCCESS )
        {
            break;
        }

        /* Not enough space �̿��� ���� �߻��� �ٷ� ���� ó�� */
        IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_SPACE );

        /* BUG-47647 loop �� 0���� 1���� ����ȴ�. Steal �ϰ� �Ҵ� �����ϸ� ����ó��
         * loop 0 : ���� Free Page �Ҵ� �õ�, �����ϸ� Steal �õ�
         * loop 1 : �ٽ� Free Page �Ҵ� �õ�, �����ϸ� ����ó�� */
        IDE_TEST_RAISE( sLoop != 0, error_not_enough_space );

        IDE_CLEAR();

        SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

        IDE_TEST( sdcTXSegMgr::tryStealFreeExtsFromOtherEntry(
                                            aStatistics,
                                            &sStartInfo,
                                            SDP_SEG_TYPE_TSS,  /* aFromSegType */
                                            SDP_SEG_TYPE_TSS,  /* aToSegType   */ 
                                            mEntryPtr,
                                            &sSysMinDskViewSCN,
                                            &sTrySuccess ) 
                  != IDE_SUCCESS );

        /* BUG-42975 ���� SegType���� Steal���� �������� �ٸ� SegType���� ��ƿ */
        if ( sTrySuccess == ID_FALSE )
        {
            IDE_TEST( sdcTXSegMgr::tryStealFreeExtsFromOtherEntry(
                                            aStatistics,
                                            &sStartInfo,
                                            SDP_SEG_TYPE_UNDO,  /* aFromSegType */
                                            SDP_SEG_TYPE_TSS,   /* aToSegType   */
                                            mEntryPtr,
                                            &sSysMinDskViewSCN,
                                            &sTrySuccess ) 
                       != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    IDE_TEST( logAndInitPage( aMtx,
                              sNewCurPagePtr,
                              aTransID,
                              aFstDskViewSCN ) 
              != IDE_SUCCESS );

    /* Mtx�� Abort�Ǹ�, PageImage�� Rollback���� RuntimeValud��
     * �������� �ʽ��ϴ�.
     * ���� Rollback�� ���� ������ �����ϵ��� �մϴ�.
     * ������ Segment�� Transaction�� �ϳ��� ȥ�ھ��� ��ü�̱�
     * ������ ������� �ϳ��� ������ �˴ϴ�.*/
    IDE_TEST( sdrMiniTrans::addPendingJob( aMtx,
                                           ID_FALSE, // isCommitJob
                                           sdcTSSegment::abortCurAllocInfo,
                                           (void*)this )
              != IDE_SUCCESS );

    setCurAllocInfo( sNewCurAllocExtRID,
                     sNewFstPIDOfCurAllocExt,
                     sNewCurAllocPID );

    IDE_ASSERT( sNewCurPagePtr != NULL );

    *aNewPagePtr = sNewCurPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_enough_space );
    {
        /* BUG-40980 : AUTOEXTEND OFF���¿��� TBS max size�� �����Ͽ� extend �Ұ���
         *             error �޽����� altibase_sm.log���� ����Ѵ�.
         * BUG-47647 : Undo Segment, TSS Segment�� max size�� �����ص� Steal�� �Ҵ� �� �� �ִ�.
         *             free page �Ҵ� ���� �Ͽ��� �� ����ϴ� ������ ���� */
        ideLog::log( IDE_SM_0,
                     "The tablespace does not have enough free space ( TBS Name :<SYS_TBS_DISK_UNDO> ).\n"
                     "TSS Page allocation failed.( TSS Segment PageID: %"ID_UINT32_FMT" )\n",
                     sSegHandle->mSegPID );
    }
    IDE_EXCEPTION_END;

    *aNewPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ������ �Ҵ� ������ ����
 *
 *               MtxRollback�� �� ���, DiskPage�� ���������� Runtime
 *               ������ �������� �ʴ´�. ���� �����ϱ� ����
 *               ���� �����صΰ�, MtxRollback�� �����Ѵ�.
 *
 * [IN] aIsCommitJob         - �̰��� Commit�� ���� �۾��̳�, �ƴϳ�
 * [IN] aTSSegment           - Abort�Ϸ��� TSSegment
 *
 **********************************************************************/
IDE_RC sdcTSSegment::abortCurAllocInfo( void * aTSSegment )
{
    sdcTSSegment   * sTSSegment;
    
    IDE_ASSERT( aTSSegment != NULL );

    sTSSegment = ( sdcTSSegment * ) aTSSegment;

    sTSSegment->mCurAllocExtRID = 
        sTSSegment->mCurAllocExtRID4MtxRollback;
    sTSSegment->mFstPIDOfCurAllocExt = 
        sTSSegment->mFstPIDOfCurAllocExt4MtxRollback;
    sTSSegment->mCurAllocPID =
        sTSSegment->mCurAllocPID4MtxRollback;

    return IDE_SUCCESS;
}



