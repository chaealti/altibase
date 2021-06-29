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
 * $Id: sdcTSSegment.h 89495 2020-12-14 05:19:22Z emlee $
 *
 * Description :
 *
 * �� ������ Transaction Status Slot Segment�� ��������Դϴ�.
 *
 * # ����
 *
 *   DRDB�� MVCC�� �����Ͽ� Ʈ����� ���¸� �����ϱ� ����
 *   Transaction Status Slot�� �����ϱ� ���� ���׸�Ʈ
 *
 **********************************************************************/

# ifndef _O_SDC_TSSEGMENT_H_
# define _O_SDC_TSSEGMENT_H_ 1

# include <sdcDef.h>

struct sdcTXSegEntry;

class sdcTSSegment
{
public:

    static IDE_RC create( idvSQL          * aStatistics,
                          sdrMtxStartInfo * aStartInfo,
                          scPageID        * aTSSegPID );

    IDE_RC initialize( idvSQL        * aStatistics,
                       sdcTXSegEntry * aEntry,
                       scPageID        aTSSegPID );

    IDE_RC destroy();

    IDE_RC resetUndoSegments( idvSQL  * aStatistics );

    IDE_RC bindTSS( idvSQL            * aStatistics,
                    sdrMtxStartInfo   * aStartInfo );

    static IDE_RC unbindTSS4Commit( idvSQL  * aStatistics,
                                    sdSID     aTSSlotSID,
                                    smSCN   * aSCN );

    static IDE_RC unbindTSS4Abort( idvSQL  * aStatistics,
                                   sdrMtx  * aMtx,
                                   sdSID     aTSSlotSID,
                                   smSCN   * aInitSCN );

    static IDE_RC getCommitSCN( idvSQL      * aStatistics,
                                void        * aTrans,
                                sdSID         aTSSlotSID,
                                smSCN       * aFstDskViewSCN,
                                smSCN         aStmtViewSCN,
                                smTID       * aTID4Wait,
                                smSCN       * aCommitSCN );

    UInt   getAlignSlotSize() { return mAlignSlotSize; }

    inline scPageID getSegPID() { return mTSSegDesc.mSegHandle.mSegPID; }
    inline scPageID getCurAllocExtDir() { return SD_MAKE_PID(mCurAllocExtRID); }
    inline sdpSegHandle * getSegHandle() {
        return sdpSegDescMgr::getSegHandle(&mTSSegDesc); };

    inline void setCurAllocInfo( sdRID     aCurAllocExtRID,
                                 scPageID  aFstPIDOfCurAllocExt,
                                 scPageID  aCurAllocPID );

    static IDE_RC abortCurAllocInfo( void * aUndoSegment );

    inline scPageID getFstPIDOfCurAllocExt() { return mFstPIDOfCurAllocExt; }

    static IDE_RC logAndInitPage( sdrMtx    * aMtx,
                                  UChar     * aPagePtr,
                                  smTID       aTransID,
                                  smSCN       aFstDskViewSCN );

    IDE_RC markSCN4ReCycle( idvSQL   * aStatistics,
                            scSpaceID  aSpaceID,
                            scPageID   aSegPID,
                            sdRID      aFstExtRID,
                            smSCN    * aCSCNorASCN );

    IDE_RC build4SegmentPerfV( void                * aHeader,
                               iduFixedTableMemory * aMemory );

    IDE_RC build4RecordPerfV( UInt                  aSegSeq,
                              void                * aHeader,
                              iduFixedTableMemory * aMemory );

    /* TASK-4007 [SM] PBT�� ���� ��� �߰�
     * TSS Dump�� �� �ִ� ��� �߰�*/
    static IDE_RC dump( UChar *aPage ,
                        SChar *aOutBuf ,
                        UInt   aOutSize );

    static inline void initPage( UChar    * aPagePtr,
                                 smTID      aTransID,
                                 smSCN      aFstDskViewSCN );

private:

    IDE_RC allocNewPage( idvSQL  * aStatistics,
                         sdrMtx  * aMtx,
                         smTID     aTransID,
                         smSCN     aTransBSCN,
                         UChar  ** aNewPagePtr );

    inline IDE_RC getCurPID4Update( idvSQL    * aStatistics,
                                    sdrMtx    * aMtx,
                                    UChar    ** aCurPagePtr );

    inline IDE_RC getCurPID4Read( idvSQL    * aStatistics,
                                  sdrMtx    * aMtx,
                                  UChar    ** aCurPagePtr );

public:

    UInt             mSlotSize;            /* TSS ������ ����Ʈ ũ�� */
    UInt             mAlignSlotSize;       /* TSS ������ ���ĵ� ����Ʈ ũ�� */
    sdcTXSegEntry *  mEntryPtr;            /* �ڽ��� �Ҽӵ� Ʈ����� ���׸�Ʈ ��Ʈ�� ������ */

    sdpSegmentDesc   mTSSegDesc;           /* Segment ����� */
    sdRID            mCurAllocExtRID;      /* ���� Ȥ�� ������ ����� ExtDesc�� RID */
    scPageID         mFstPIDOfCurAllocExt; /* ���� Ȥ�� ������ ����� ExtDesc�� ù��° PageID */
    scPageID         mCurAllocPID;         /* ���� Ȥ�� ������ ����� TSS �������� PID */

    /* �� Cache�� ���� Mtx Rollback�� �Ͼ���� �����ϱ� ���� ����� */
    sdRID            mCurAllocExtRID4MtxRollback;
    scPageID         mFstPIDOfCurAllocExt4MtxRollback;
    scPageID         mCurAllocPID4MtxRollback;
};



/***********************************************************************
 *
 * Description : TSS Page�� CntlHdr �ʱ�ȭ
 *
 * aPagePtr       - [IN] ������ ������
 * aTransID       - [IN] �������� �Ҵ��� Ʈ����� ID
 * aFstDskViewSCN - [IN] �������� �Ҵ��� Ʈ������� Begin SCN
 *
 **********************************************************************/
void sdcTSSegment::initPage( UChar  * aPagePtr,
                             smTID    aTransID,
                             smSCN    aFstDskViewSCN )
{
    sdcTSSPageCntlHdr   * sCntlHdr;

    IDE_ASSERT( aPagePtr != NULL );

    sCntlHdr =
        (sdcTSSPageCntlHdr*)
        sdpPhyPage::initLogicalHdr( (sdpPhyPageHdr*)aPagePtr,
                                    ID_SIZEOF( sdcTSSPageCntlHdr ) );

    sCntlHdr->mTransID  = aTransID;
    SM_SET_SCN( &sCntlHdr->mFstDskViewSCN, &aFstDskViewSCN );

    sdpSlotDirectory::init( (sdpPhyPageHdr*)aPagePtr );
}

/***********************************************************************
 *
 * Description : TSS Page�� X-Latch ȹ�� ��ȯ
 *
 * aStatistics [IN]  - �������
 * aMtx        [IN]  - Mtx ������
 * aCurPagePtr [OUT] - ���� ������� TSS ������ ������
 *
 **********************************************************************/
inline IDE_RC sdcTSSegment::getCurPID4Update( idvSQL    * aStatistics,
                                              sdrMtx    * aMtx,
                                              UChar    ** aCurPagePtr )
{
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aCurPagePtr != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          mCurAllocPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          aCurPagePtr,
                                          NULL,
                                          NULL )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : TSS Page�� S-Latch ȹ�� ��ȯ
 *
 * aStatistics [IN]  - �������
 * aMtx        [IN]  - Mtx ������
 * aCurPagePtr [OUT] - ���� ������� TSS ������ ������
 *
 **********************************************************************/
inline IDE_RC sdcTSSegment::getCurPID4Read( idvSQL    * aStatistics,
                                            sdrMtx    * aMtx,
                                            UChar    ** aCurPagePtr )
{
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aCurPagePtr != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                          mCurAllocPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          aCurPagePtr,
                                          NULL,
                                          NULL )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���� �Ҵ��� TSS �������� ���� ����
 *
 * [IN] aCurAllocExtRID      - ���� ������� ExtDesc RID
 * [IN] aFstPIDOfCurAllocExt - ���� ������� ExtDesc�� ù��° PID
 * [IN] aCurAllocExtRID      - ���� ������� �������� PID
 *
 **********************************************************************/
inline void sdcTSSegment::setCurAllocInfo( sdRID     aCurAllocExtRID,
                                           scPageID  aFstPIDOfCurAllocExt,
                                           scPageID  aCurAllocPID )
{
    /* Mtx Rollback�� �Ͼ���� �����ϱ� ���� ����� */
    mCurAllocExtRID4MtxRollback      = mCurAllocExtRID;
    mFstPIDOfCurAllocExt4MtxRollback = mFstPIDOfCurAllocExt;
    mCurAllocPID4MtxRollback         = mCurAllocPID;

    mCurAllocExtRID            = aCurAllocExtRID;
    mFstPIDOfCurAllocExt       = aFstPIDOfCurAllocExt;
    mCurAllocPID               = aCurAllocPID;
}

# endif // _O_SDC_TSSEGMENT_H_
