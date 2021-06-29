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
 * $Id: sdpPhyPage.h 87929 2020-07-03 00:38:42Z et16 $
 *
 * Description :
 *
 * Page
 *
 * # ����
 *
 * tablespace�� page�� ���� �ڷᱸ�� ����
 *
 *
 * # ����
 *     __________________SM_PAGE_SIZE(32KB)__________________________________
 *    |                                                                      |
 *     __________sdpPageHdr___________________________________________
 *    /_______________________________________________________________\______
 *    |  |  node   |       |   |         |       |           |         |     |
 *    |  |_________|       |   |         |       |           |         |     |
 *    |@ |prev|next|page id|LSN| ext.RID |SMO No |free offset|free size|     |
 *    |__|____|____|_______|___|_________|_______|___________|_________|_____|
 *      |
 *      checksum
 *
 *  - sdpPageHdr(��� Page���� �������� Physical ���)
 *
 *    + checksum :  page ���� offset�� 4bytes������ ����
 *
 *    + sdpPageListNode : previous page id�� next page id�� ����
 *
 *    + LSN : �������� ���� ������ �ݿ��� �α��� LSN (update LSN)
 *            (!!) �������� flush�ϱ��������� �� LSN���� log�� ��ũ�� �ݿ�
 *            �Ǿ� �־�� ��
 *
 *    + ext.desc.RID : �� �������� �Ҵ��� �� extent desc.�� rid�� ����
 *                     ������ free�� ���� ó���ϱ� ���ؼ���.
 *
 *    + SMO No : �ε����� ���࿩�θ� �˱����� ��ȣ�̴�.
 *
 *    + Free Offset : �� �������� free�� ��ġ�� ����Ų��. �� ��ġ����
 *                    ���ο� slot�� ����� ���� �ּҸ� ��ȯ�Ѵ�.
 *                    �ᱹ �̴� �������� last slot�� ���� �ǹ̸� ������.
 *
 *    + Free Size : �� �������� free�� ������ ��Ÿ����.
 *
 *
 *  # ���� �ڷᱸ��
 *
 *    - sdpPageHdr ����ü
 *
 **********************************************************************/

#ifndef _O_SDP_PHY_PAGE_H_
#define _O_SDP_PHY_PAGE_H_ 1

#include <smDef.h>
#include <sdr.h>
#include <sdb.h>
#include <smu.h>
#include <sdpDef.h>
#include <sdpDblPIDList.h>
#include <sdpSglPIDList.h>
#include <sdpSlotDirectory.h>
#include <smrCompareLSN.h>


class sdrMiniTrans;

class sdpPhyPage
{
public:

    // page �Ҵ�� physical header �ʱ�ȭ �� �α�
    static IDE_RC initialize( sdpPhyPageHdr  *aPageHdr,
                              scPageID        aPageID,
                              sdpParentInfo  *aParentInfo,
                              UShort          aPageState,
                              sdpPageType     aPageType,
                              smOID           aTableOID,
                              UInt            aIndexID  );

    static IDE_RC logAndInit( sdpPhyPageHdr *aPageHdr,
                              scPageID       aPageID,
                              sdpParentInfo *aParentInfo,
                              UShort         aPageState,
                              sdpPageType    aPageType,
                              smOID          aTableOID,
                              UInt           aIndexID,
                              sdrMtx        *aMtx );

    static IDE_RC create( idvSQL        *aStatistics,
                          scSpaceID      aSpaceID,
                          scPageID       aPageID,
                          sdpParentInfo *aParentInfo,
                          UShort         aPageState,
                          sdpPageType    aPageType,
                          smOID          aTableOID,
                          UInt           aIndexID,
                          sdrMtx        *aCrtMtx,
                          sdrMtx        *aInitMtx,
                          UChar        **aPagePtr);

    static IDE_RC create4DPath( idvSQL        *aStatistics,
                                void          *aTrans,
                                scSpaceID      aSpaceID,
                                scPageID       aPageID,
                                sdpParentInfo *aParentInfo,
                                UShort         aPageState,
                                sdpPageType    aType,
                                smOID          aTableOID,
                                idBool         aIsLogging,
                                UChar        **aPagePtr);

    static IDE_RC setLinkState( sdpPhyPageHdr    * aPageHdr,
                                sdpPageListState   aLinkState,
                                sdrMtx           * aMtx );

    static IDE_RC reset( sdpPhyPageHdr *aPageHdr,
                         UInt           aLogicalHdrSize,
                         sdrMtx        *aMtx );

    static IDE_RC logAndInitLogicalHdr( sdpPhyPageHdr  *aPageHdr,
                                        UInt            aSize,
                                        sdrMtx         *aMtx,
                                        UChar         **aLogicalHdrStartPtr );

    /* logcial page header�� �����ϸ鼭 physical page�� header��
     * total free size�� free offset�� logical �ϰ� ó���Ѵ�. */
    static UChar* initLogicalHdr( sdpPhyPageHdr  *aPageHdr,
                                  UInt            aSize );

    static inline IDE_RC setIdx( sdpPhyPageHdr * aPageHdr,
                                 UChar           aIdx,
                                 sdrMtx        * aMtx );

    /* slot�� �Ҵ� �����Ѱ�. */
    static idBool canAllocSlot( sdpPhyPageHdr *aPageHdr,
                                UInt           aSlotSize,
                                idBool         aNeedAllocSlotEntry,
                                UShort         aSlotAlignValue );

    static IDE_RC allocSlot4SID( sdpPhyPageHdr       *aPageHdr,
                                 UShort               aSlotSize,
                                 UChar              **aAllocSlotPtr,
                                 scSlotNum           *aAllocSlotNum,
                                 scOffset            *aAllocSlotOffset );

    static IDE_RC allocSlot( sdpPhyPageHdr       *aPageHdr,
                             scSlotNum            aSlotNum,
                             UShort               aSaveSize,
                             idBool               aNeedAllocSlotEntry,
                             UShort              *aAllowedSize,
                             UChar              **aAllocSlotPtr,
                             scOffset            *aAllocSlotOffset,
                             UChar                aSlotAlignValue );

    static IDE_RC freeSlot4SID( sdpPhyPageHdr   *aPageHdr,
                                UChar           *aSlotPtr,
                                scSlotNum        aSlotNum,
                                UShort           aSlotSize );

    static IDE_RC freeSlot( sdpPhyPageHdr   *aPageHdr,
                            UChar           *aSlotPtr,
                            UShort           aFreeSize,
                            idBool           aNeedFreeSlotEntry,
                            UChar            aSlotAlignValue );

    static IDE_RC freeSlot( sdpPhyPageHdr   *aPageHdr,
                            scSlotNum        aSlotNum,
                            UShort           aFreeSize,
                            UChar            aSlotAlignValue );

    static IDE_RC compactPage( sdpPhyPageHdr         *aPageHdr,
                               sdpGetSlotSizeFunc     aGetSlotSizeFunc );

    static idBool isPageCorrupted( UChar      *aStartPtr );
    static idBool checkAndSetPageCorrupted( scSpaceID  aSpaceID,
                                            UChar      *aStartPtr );
    //TASK-4007 [SM]PBT�� ���� ��� �߰�
    //�������� ���� Hex�ڵ�� �ѷ��ش�.
    static IDE_RC dumpHdr   ( const UChar *sPage ,
                              SChar       *aOutBuf ,
                              UInt         aOutSize );

    static IDE_RC tracePage( UInt           aChkFlag,
                             ideLogModule   aModule,
                             UInt           aLevel,
                             const UChar  * aPage,
                             const SChar  * aTitle,
                             ... );

    static IDE_RC tracePageInternal( UInt           aChkFlag,
                                     ideLogModule   aModule,
                                     UInt           aLevel,
                                     const UChar  * aPage,
                                     const SChar  * aTitle,
                                     va_list        ap );

    /* BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
     * ����� ���� �߰�. */
    static IDE_RC writeToPage( UChar * aDestPagePtr,
                               UChar * aSrcPtr,
                               UInt    aLength );

    static idBool validate( sdpPhyPageHdr *aPageHdr );

    static IDE_RC verify( sdpPhyPageHdr * aPageHdr,
                          scPageID        aSpaceID,
                          scPageID        aPageID );

    // BUG-17930 : log���� page�� ������ ��(nologging index build, DPath insert),
    // operation�� ���� �� �Ϸ�Ǳ� �������� page�� physical header�� �ִ�
    // mIsConsistent�� F�� ǥ���ϰ�, operation�� �Ϸ�� �� mIsConsistent��
    // T�� ǥ���Ͽ� server failure �� recovery�� �� redo�� �����ϵ��� �ؾ� ��
    static IDE_RC setPageConsistency( sdrMtx        *aMtx,
                                      sdpPhyPageHdr *aPageHdr,
                                      UChar         *aIsConsistent );

    /* PROJ-2162 RestartRiskReduction
     * Page�� Inconsistent�ϴٰ� ������ */
    static IDE_RC setPageInconsistency( scSpaceID       aSpaceID,
                                        scPageID        aPID );

    /* BUG-45598 Page�� Inconsistent�ϴٰ� ���� ( ������ ��� �̿�, ��� ) */
    static IDE_RC setPageInconsistency( sdpPhyPageHdr   *aPageHdr );

    // PROJ-1665 : Page�� consistent ���� ���� ��ȯ
    static inline idBool isConsistentPage( UChar * aPageHdr );

    static idBool isSlotDirBasedPage( sdpPhyPageHdr    *aPageHdr );

    static inline void makeInitPageInfo( sdrInitPageInfo * aInitInfo,
                                         scPageID          aPageID,
                                         sdpParentInfo   * aParentInfo,
                                         UShort            aPageState,
                                         sdpPageType       aPageType,
                                         smOID             aTableOID,
                                         UInt              aIndexID );

/* inline function */
public:

    /* �������� page id ��ȯ */
    inline static scPageID  getPageID( UChar*  aStartPtr );

    /* ���������� ������ Pointer���� page id ��ȯ */
    inline static scPageID getPageIDFromPtr( void*  aPtr );

    /* �������� space id ��ȯ*/
    inline static scSpaceID  getSpaceID( UChar* aStartPtr );

    /* �������� page double linked list node ��ȯ */
    inline static sdpDblPIDListNode* getDblPIDListNode( sdpPhyPageHdr* aPageHdr );

    /* �������� page double linked prev page id ��ȯ */
    inline static scPageID  getPrvPIDOfDblList( sdpPhyPageHdr* aPageHdr );

    /* �������� page double linked next page id ��ȯ */
    inline static scPageID  getNxtPIDOfDblList( sdpPhyPageHdr* aPageHdr );

    /* �������� page single linked list node ��ȯ */
    inline static sdpSglPIDListNode* getSglPIDListNode( sdpPhyPageHdr* aPageHdr );

    /* �������� single linked list next page id ��ȯ */
    inline static scPageID getNxtPIDOfSglList( sdpPhyPageHdr* aPageHdr );

    /* �������� pageLSN ��� */
    inline static smLSN getPageLSN( UChar* aStartPtr );

    /* �������� pageLSN ���� */
    inline static void setPageLSN( UChar      * aPageHdr,
                                   smLSN      * aPageLSN );

    /* �������� checksum ���*/
    inline static UInt getCheckSum( sdpPhyPageHdr* aPageHdr );

    inline static void calcAndSetCheckSum( UChar *aPageHdr );

    /* �������� Parent Node ��� */
    inline static sdpParentInfo getParentInfo( UChar * aStartPtr );

    /* smo number�� get�Ѵ�.**/
    inline static ULong getIndexSMONo( sdpPhyPageHdr * aPageHdr );

    /* �������� index smo no�� set�Ѵ�. */
    inline static void setIndexSMONo( UChar *aStartPtr, ULong aValue );
    inline static void setIndexSMONo( sdpPhyPageHdr *aPageHdr, ULong aValue );

    /* page ���¸� get�Ѵ�.*/
    inline static UShort getState( sdpPhyPageHdr * aPageHdr );

    /*  �������� page state�� set�Ѵ�.
     * - SDR_2BYTES �α� */
    inline static IDE_RC setState( sdpPhyPageHdr   *aPageHdr,
                                   UShort           aValue,
                                   sdrMtx          *aMtx );

    /* �������� Type�� set�Ѵ�.
     * - SDR_2BYTES �α� */
    inline static IDE_RC setPageType( sdpPhyPageHdr   *aPageHdr,
                                      UShort           aValue,
                                      sdrMtx          *aMtx );

    /* PROJ-2037 TMS: Segment���� Page�� SeqNo�� �����Ѵ�. */
    inline static IDE_RC setSeqNo( sdpPhyPageHdr *aPageHdr,
                                   ULong          aValue,
                                   sdrMtx         *aMtx );

    /* Page Type��  Get�Ѵ�.*/
    inline static sdpPageType getPageType( sdpPhyPageHdr *aPageHdr );

    /* BUG-32091 [sm_collection] add TableOID in PageHeader
     * TableOID�� Get�Ѵ�.*/
    inline static smOID getTableOID( UChar * aPageHdr );

    /* ���� ���̾ ���� �Լ��ν�,������ �����Ϳ� �ش��ϴ� ������ Ÿ����
     * ��´�.  To fix BUG-13462 */
    inline static UInt getPhyPageType( UChar *aPagePtr );

    /* ���� ���̾ ���� �Լ��ν�,�����Ǵ� ������ Ÿ���� ������ ��´�.
     * To fix BUG-13462 */
    inline static UInt getPhyPageTypeCount( );

    /* offset���κ��� page pointer�� ���Ѵ�. */
    inline static UChar* getPagePtrFromOffset( UChar       *aPagePtr,
                                               scOffset     aOffset );

    /* page pointer�κ��� offset�� ���Ѵ�. */
    inline static scOffset getOffsetFromPagePtr( UChar *aPagePtr );

    inline static scOffset getFreeSpaceBeginOffset( sdpPhyPageHdr *aPageHdr );

    inline static scOffset getFreeSpaceEndOffset( sdpPhyPageHdr *aPageHdr );

    /* free size�� get�Ѵ�. */
    inline static UInt getTotalFreeSize( sdpPhyPageHdr *aPageHdr );

    /* �������� free size�� set�Ѵ�. */
    /* not used
    inline static void setTotalFreeSize( sdpPhyPageHdr *aPageHdr,
                                         UShort         aValue );
    */
    inline static UShort getAvailableFreeSize( sdpPhyPageHdr *aPageHdr );

    inline static void setAvailableFreeSize( sdpPhyPageHdr *aPageHdr,
                                             UShort         aValue );

    /* page empty �϶� �� ������ ����Ѵ�. */
    inline static UInt getEmptyPageFreeSize();

    /* Fragment�� �߻����� ���� ������ ����� ���� */
    inline static UShort getNonFragFreeSize( sdpPhyPageHdr *aPageHdr );
    
    /* Fragment�� �߻����� ���� ������ ����� ���� */
    inline static UShort getAllocSlotOffset( sdpPhyPageHdr * aPageHdr,
                                             UShort          aAlignedSaveSize );

    /* �� ���������� logical hdr�� �����ϴ� �κ��� return */
    inline static UChar* getLogicalHdrStartPtr( UChar   *aPagePtr );

    /* �� ���������� CTL�� �����ϴ� �κ��� return */
    inline static UChar* getCTLStartPtr( UChar   *aPagePtr );

    /* �� ���������� slot directory�� �����ϴ� �κ��� return */
    inline static UChar* getSlotDirStartPtr( UChar   *aPagePtr );

    /* �� ���������� slot directory�� ������ �κ��� return */
    inline static UChar* getSlotDirEndPtr( sdpPhyPageHdr    *aPhyPageHdr );

    /* �� ���������� page footer�� �����ϴ� �κ��� return */
    inline static UChar* getPageFooterStartPtr( UChar    *aPagePtr );

    /* �� ���������� physical hdr + logical hdr�� ������
     * data�� ���� ����Ǵ� ���� offset */
    inline static UInt getDataStartOffset( UInt aLogicalHdrSize );

    /* �� ���������� �����Ͱ� �־������� �������� �������� ���Ѵ�.
       page���� Ư�� pointer  */
    inline static UChar* getPageStartPtr( void *aPagePtr );

    /* page�� temp table type���� �˻��Ѵ�. */
#if 0  // not used
    inline static idBool isPageTempType( UChar *aStartPtr );
#endif
    /* page�� TSS table type���� �˻��Ѵ�. */
    inline static idBool isPageTSSType( UChar *aStartPtr );

    /* page�� index type���� �˻��Ѵ�. */
    inline static idBool isPageIndexType( UChar *aStartPtr );

    /* page�� index �Ǵ� IndexMeta type���� �˻��Ѵ�. */
    inline static idBool isPageIndexOrIndexMetaType( UChar *aStartPtr );

    /* �� ���������� �����Ͱ� �־������� Phy page hdr�� ���Ѵ�.
     * page���� Ư�� pointer */
    inline static sdpPhyPageHdr* getHdr( UChar *aPagePtr );

    /* physical hdr�� ũ�⸦ ���Ѵ�. */
    inline static UShort getPhyHdrSize();

    /* logical hdr�� ũ�⸦ ���Ѵ�. */
    inline static UShort getLogicalHdrSize( sdpPhyPageHdr *aPageHdr);

    /* CTL �� ũ�⸦ ���Ѵ�. */
    inline static UShort getSizeOfCTL( sdpPhyPageHdr *aPageHdr );
    
    /* Footer �� ũ�⸦ ���Ѵ�. */
    inline static UShort getSizeOfFooter();

    /* page pointer�κ��� RID�� ��´�. */
    inline static sdRID getRIDFromPtr( void *aPtr );

    /* page pointer�κ��� SID�� ��´�. */
    /* not used 
    inline static sdRID getSIDFromPtr( void *aPtr );
    */
    /* page�� �ϳ��� slot entry�� ������ ������ �ִ� �� �˻��Ѵ�. */
    inline static idBool canMakeSlotEntry( sdpPhyPageHdr *aPageHdr );

    /* page�� free ������ ������ ���� �˻��Ѵ�. */
/* not used
    inline static idBool canPageFree( sdpPhyPageHdr *aPageHdr );
*/
    /* PROJ-2037 TMS: Segment���� Page�� SeqNo�� ��´�. */
    inline static ULong getSeqNo( sdpPhyPageHdr *aPageHdr );

    // PROJ-1568 Buffer Manager Renewal
    // sdb���� Ư�� ������Ÿ�� ��ȣ�� undo page type����
    // ���ʿ䰡 �ִ�. �̴� ������� ����� ���ȴ�.
    static UInt getUndoPageType();

    static void resetIndexSMONo( sdpPhyPageHdr * aPageHdr,
                                 scSpaceID       aSpaceID,
                                 idBool          aChkOnlineTBS );

    // PROJ-1704 Disk MVCC ������
    static void initCTL( sdpPhyPageHdr  * aPageHdr,
                         UInt             aHdrSize,
                         UChar         ** aHdrStartPtr );

    static IDE_RC extendCTL( sdpPhyPageHdr * aPageHdr,
                             UInt            aSlotCnt,
                             UInt            aSlotSize,
                             UChar        ** aNewSlotStartPtr,
                             idBool        * aTrySuccess );

    static void shiftSlotDirToBottom(sdpPhyPageHdr     *aPageHdr,
                                     UShort             aShiftSize);

    inline static IDE_RC logAndSetAvailableFreeSize( sdpPhyPageHdr *aPageHdr,
                                                     UShort         aValue,
                                                     sdrMtx        *aMtx );

    inline static idBool isValidPageType( scSpaceID aSpaceID,
                                          scPageID  aPageID,
                                          UInt      aPageType );

    /* checksum�� ��� */
    inline static UInt calcCheckSum( sdpPhyPageHdr *aPageHdr );
    
    inline static UInt getSizeOfSlotDir( sdpPhyPageHdr *aPageHdr );

private:


    inline static IDE_RC logAndSetFreeSpaceBeginOffset( sdpPhyPageHdr  *aPageHdr,
                                                        scOffset        aValue,
                                                        sdrMtx         *aMtx );

    inline static IDE_RC logAndSetLogicalHdrSize( sdpPhyPageHdr  *aPageHdr,
                                                  UShort          aValue,
                                                  sdrMtx         *aMtx );

    inline static IDE_RC logAndSetTotalFreeSize( sdpPhyPageHdr *aPageHdr,
                                                 UShort         aValue,
                                                 sdrMtx        *aMtx );

    static IDE_RC allocSlotSpace( sdpPhyPageHdr       *aPageHdr,
                                  UShort               aSaveSize,
                                  UChar              **aAllocSlotPtr,
                                  scOffset            *aAllocSlotOffset );

    static void freeSlotSpace( sdpPhyPageHdr        *aPageHdr,
                               scOffset              aSlotOffset,
                               UShort                aFreeSize);

};

inline UInt sdpPhyPage::getUndoPageType()
{
    return SDP_PAGE_UNDO;
}


/* �������� page id ��ȯ */
inline scPageID  sdpPhyPage::getPageID( UChar*  aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;

    IDE_DASSERT( aStartPtr == getPageStartPtr(aStartPtr));

    sPageHdr = getHdr(aStartPtr);

    IDE_DASSERT( validate(sPageHdr) == ID_TRUE );

    return ( sPageHdr->mPageID );

}

/* ���������� ������ Pointer���� page id ��ȯ */
inline scPageID sdpPhyPage::getPageIDFromPtr( void*  aPtr )
{
    sdpPhyPageHdr *sPageHdr;
    UChar         *sStartPtr;

    sStartPtr = getPageStartPtr( (UChar*)aPtr );
    sPageHdr  = getHdr( sStartPtr );

    return sPageHdr->mPageID;
}

/* �������� space id ��ȯ*/
inline  scSpaceID  sdpPhyPage::getSpaceID( UChar*   aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;

    IDE_DASSERT( aStartPtr == getPageStartPtr(aStartPtr));

    sPageHdr = getHdr(aStartPtr);

    return ( sPageHdr->mFrameHdr.mSpaceID );
}

/* �������� page double linked list node ��ȯ */
inline  sdpDblPIDListNode* sdpPhyPage::getDblPIDListNode( sdpPhyPageHdr* aPageHdr )
{
    return &( aPageHdr->mListNode );
}

/* �������� page double linked prev page id ��ȯ */
inline  scPageID  sdpPhyPage::getPrvPIDOfDblList( sdpPhyPageHdr* aPageHdr )
{
    return ( sdpDblPIDList::getPrvOfNode( getDblPIDListNode( aPageHdr ) ) );
}

/* �������� page double linked next page id ��ȯ */
inline  scPageID  sdpPhyPage::getNxtPIDOfDblList( sdpPhyPageHdr* aPageHdr )
{
    return sdpDblPIDList::getNxtOfNode( getDblPIDListNode( aPageHdr ) );
}

/* �������� page single linked list node ��ȯ */
inline  sdpSglPIDListNode* sdpPhyPage::getSglPIDListNode( sdpPhyPageHdr* aPageHdr )
{
    return (sdpSglPIDListNode*)&( aPageHdr->mListNode );
}

/* �������� single linked list next page id ��ȯ */
inline scPageID sdpPhyPage::getNxtPIDOfSglList( sdpPhyPageHdr* aPageHdr )
{
    return sdpSglPIDList::getNxtOfNode( getSglPIDListNode( aPageHdr ) );
}

/* �������� pageLSN ��� */
inline smLSN sdpPhyPage::getPageLSN( UChar* aStartPtr )
{
    IDE_DASSERT( aStartPtr == getPageStartPtr(aStartPtr));
    return ( getHdr(aStartPtr)->mFrameHdr.mPageLSN );
}

/* �������� pageLSN ����
 * - ���۸Ŵ����� flush�ϱ� ���� ����
 * - logging ���� ���� */
inline void sdpPhyPage::setPageLSN( UChar   * aPageHdr,
                                    smLSN   * aPageLSN )
{
    sdpPhyPageHdr   * sPageHdr = (sdpPhyPageHdr*)aPageHdr;
    sdpPageFooter   * sPageFooter;

    IDE_DASSERT( aPageHdr == getPageStartPtr(aPageHdr) );

    SM_GET_LSN( sPageHdr->mFrameHdr.mPageLSN, *aPageLSN );

    sPageFooter = (sdpPageFooter*)getPageFooterStartPtr(aPageHdr);

    SM_GET_LSN( sPageFooter->mPageLSN, *aPageLSN );

    IDE_DASSERT( validate(sPageHdr) == ID_TRUE );
}

/* �������� checksum ���*/
inline UInt sdpPhyPage::getCheckSum( sdpPhyPageHdr* aPageHdr )
{
    return ( aPageHdr->mFrameHdr.mCheckSum );
}

/* �������� Leaf ���� ��� */
inline sdpParentInfo sdpPhyPage::getParentInfo( UChar * aStartPtr )
{

    sdpPhyPageHdr *sPageHdr;

    IDE_DASSERT( aStartPtr == getPageStartPtr(aStartPtr));

    sPageHdr = getHdr(aStartPtr);

    return (sPageHdr->mParentInfo);
}

/* smo number�� get�Ѵ�.**/
inline ULong sdpPhyPage::getIndexSMONo( sdpPhyPageHdr * aPageHdr )
{
    return ( aPageHdr->mFrameHdr.mIndexSMONo );
}

/* �������� index smo no�� set�Ѵ�. */
inline void sdpPhyPage::setIndexSMONo( UChar *aStartPtr, ULong aValue )
{
    sdpPhyPageHdr *sPageHdr;
    IDE_DASSERT( aStartPtr == getPageStartPtr(aStartPtr) );

    sPageHdr = getHdr(aStartPtr);

    IDE_DASSERT( validate(sPageHdr) == ID_TRUE );
    setIndexSMONo( sPageHdr, aValue );
}

inline void sdpPhyPage::setIndexSMONo( sdpPhyPageHdr *aPageHdr, ULong aValue )
{
    IDE_DASSERT( (UChar*)aPageHdr == getPageStartPtr((UChar*)aPageHdr) );
    aPageHdr->mFrameHdr.mIndexSMONo = aValue;
}
/* page ���¸� get�Ѵ�.*/
inline UShort sdpPhyPage::getState( sdpPhyPageHdr * aPageHdr )
{
    return aPageHdr->mPageState;
}

/*  �������� page state�� set�Ѵ�.
 * - SDR_2BYTES �α� */
inline IDE_RC sdpPhyPage::setState( sdpPhyPageHdr   *aPageHdr,
                                    UShort           aValue,
                                    sdrMtx          *aMtx )
{
    UShort sPageState = (UShort)aValue;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aPageHdr->mPageState,
                                         &sPageState,
                                         ID_SIZEOF(sPageState) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpPhyPage::setPageType( sdpPhyPageHdr   *aPageHdr,
                                       UShort           aValue,
                                       sdrMtx          *aMtx )
{
    UShort sPageType = (UShort)aValue;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aPageHdr->mPageType,
                                         &sPageType,
                                         ID_SIZEOF(sPageType) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpPhyPage::setSeqNo( sdpPhyPageHdr *aPageHdr,
                                    ULong          aValue,
                                    sdrMtx         *aMtx )
{
    ULong   sSeqNo = (ULong)aValue;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aPageHdr->mSeqNo,
                                         &sSeqNo,
                                         ID_SIZEOF(sSeqNo) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Page Type��  Get�Ѵ�.*/
inline sdpPageType sdpPhyPage::getPageType( sdpPhyPageHdr *aPageHdr )
{
    return (sdpPageType)(aPageHdr->mPageType);
}
/* BUG-32091 [sm_collection] add TableOID in PageHeader
 * TableOID�� Get�Ѵ�.*/
inline smOID sdpPhyPage::getTableOID( UChar * aPageHdr )
{
    return (smOID)(((sdpPhyPageHdr *)aPageHdr)->mTableOID);
}

/* ���� ���̾ ���� �Լ��ν�,������ �����Ϳ� �ش��ϴ� ������ Ÿ����
 * ��´�.  To fix BUG-13462 */
inline UInt sdpPhyPage::getPhyPageType( UChar *aPagePtr )
{
    return (UInt)getPageType( (sdpPhyPageHdr*)aPagePtr );
}

/* ���� ���̾ ���� �Լ��ν�,�����Ǵ� ������ Ÿ���� ������ ��´�.
 * To fix BUG-13462 */
inline UInt sdpPhyPage::getPhyPageTypeCount( )
{
    return SDP_PAGE_TYPE_MAX;
}

/* offset���κ��� page pointer�� ���Ѵ�. */
inline UChar* sdpPhyPage::getPagePtrFromOffset( UChar       *aPagePtr,
                                                scOffset     aOffset )
{
    UChar * sPagePtrFromOffset;

    IDE_DASSERT( aPagePtr != NULL );

    sPagePtrFromOffset = ( getPageStartPtr(aPagePtr) + aOffset );

    /* TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::getPagePtrFromOffset �Լ���
     * sdpSlotDirectory::getPagePtrFromOffset�� �ߺ����� �߰��Ѵ�. 
     * �� �Լ��� �����Ǹ� ������ �Լ��� �����ؾ� �Ѵ�.*/
    IDE_DASSERT( sdpSlotDirectory::getPagePtrFromOffset(aPagePtr, aOffset) ==
                 sPagePtrFromOffset );

    return sPagePtrFromOffset;
}

/* page pointer�κ��� offset�� ���Ѵ�. */
inline scOffset sdpPhyPage::getOffsetFromPagePtr( UChar *aPagePtr )
{
    IDE_DASSERT( aPagePtr != NULL );
    return ( aPagePtr - getPageStartPtr(aPagePtr) );
}

inline scOffset sdpPhyPage::getFreeSpaceBeginOffset( sdpPhyPageHdr *aPageHdr )
{
    return ( aPageHdr->mFreeSpaceBeginOffset );
}

inline scOffset sdpPhyPage::getFreeSpaceEndOffset( sdpPhyPageHdr *aPageHdr )
{
    return ( aPageHdr->mFreeSpaceEndOffset );
}

inline IDE_RC sdpPhyPage::logAndSetFreeSpaceBeginOffset( 
                                sdpPhyPageHdr  *aPageHdr,
                                scOffset        aValue,
                                sdrMtx         *aMtx )
{
    UShort sFreeOffset = (UShort)aValue;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( 
                            aMtx,
                            (UChar*)&aPageHdr->mFreeSpaceBeginOffset,
                            &sFreeOffset,
                            ID_SIZEOF(sFreeOffset) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpPhyPage::logAndSetLogicalHdrSize( 
                                   sdpPhyPageHdr  *aPageHdr,
                                   UShort          aValue,
                                   sdrMtx         *aMtx )
{
    UShort sSize = (UShort)aValue;

    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( 
                                 aMtx,
                                 (UChar*)&aPageHdr->mLogicalHdrSize,
                                 &sSize,
                                 ID_SIZEOF(sSize) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


inline IDE_RC sdpPhyPage::logAndSetTotalFreeSize( 
                                sdpPhyPageHdr *aPageHdr,
                                UShort         aValue,
                                sdrMtx        *aMtx )
{
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( 
                                 aMtx,
                                 (UChar*)&aPageHdr->mTotalFreeSize,
                                 &aValue,
                                 ID_SIZEOF(aValue) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



inline IDE_RC sdpPhyPage::logAndSetAvailableFreeSize( 
                                sdpPhyPageHdr *aPageHdr,
                                UShort         aValue,
                                sdrMtx        *aMtx )
{
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aPageHdr->mAvailableFreeSize,
                                         &aValue,
                                         ID_SIZEOF(aValue) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* free size�� get�Ѵ�. */
inline UInt sdpPhyPage::getTotalFreeSize( sdpPhyPageHdr *aPageHdr )
{
    return ( aPageHdr->mTotalFreeSize );
}

inline UShort sdpPhyPage::getAvailableFreeSize( sdpPhyPageHdr *aPageHdr )
{
    return ( aPageHdr->mAvailableFreeSize );
}

inline void sdpPhyPage::setAvailableFreeSize( sdpPhyPageHdr *aPageHdr,
                                              UShort         aValue )
{
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_DASSERT( aValue <= (SD_PAGE_SIZE - 1) );

    aPageHdr->mAvailableFreeSize = aValue;
}

/* page empty �϶� �� ������ ����Ѵ�. */
inline UInt sdpPhyPage::getEmptyPageFreeSize()
{
    return ( SD_PAGE_SIZE -
             ( getPhyHdrSize() + ID_SIZEOF(sdpPageFooter) ) );
}

/******************************************************************************
 * Description :
 *    CheckSum ��Ŀ� ���� CheckSum�� �����Ѵ�.
 *
 *  aPageHdr  - [IN] Page ���� ������
 *
 ******************************************************************************/
inline void sdpPhyPage::calcAndSetCheckSum( UChar *aPageHdr )
{
    sdpPhyPageHdr   * sPageHdr;
    sdpPageFooter   * sPageFooter;

    IDE_DASSERT( aPageHdr == getPageStartPtr(aPageHdr) );

    sPageHdr = getHdr( aPageHdr );
    IDE_DASSERT( validate(sPageHdr) == ID_TRUE );

    if( smuProperty::getCheckSumMethod() == SDP_CHECK_CHECKSUM )
    {
        sPageHdr->mFrameHdr.mCheckSum = calcCheckSum(sPageHdr);
    }
    else
    {
        sPageFooter = (sdpPageFooter*)getPageFooterStartPtr( aPageHdr );

        IDE_ASSERT( smrCompareLSN::isEQ( &sPageHdr->mFrameHdr.mPageLSN,
                                         &sPageFooter->mPageLSN ) 
                    == ID_TRUE );
        
        // BUG-45598: LSN�� ����ϸ� checksum ���� 0���� �ʱ�ȭ 
        sPageHdr->mFrameHdr.mCheckSum = SDP_CHECKSUM_INIT_VAL;
    }
}

/* Fragment�� �߻����� ���� ������ ����� ���� */
inline UShort sdpPhyPage::getNonFragFreeSize( sdpPhyPageHdr *aPageHdr )
{
    return (aPageHdr->mFreeSpaceEndOffset - aPageHdr->mFreeSpaceBeginOffset);
}

/* Fragment�� �߻����� ���� ������ ����� ���� */
inline UShort sdpPhyPage::getAllocSlotOffset( sdpPhyPageHdr * aPageHdr,
                                              UShort          aAlignedSaveSize)
{
    return (aPageHdr->mFreeSpaceEndOffset - aAlignedSaveSize);
}

/* �� ���������� logical hdr�� �����ϴ� �κ��� return */
inline UChar* sdpPhyPage::getLogicalHdrStartPtr( UChar   *aPagePtr )
{
    IDE_ASSERT( aPagePtr != NULL );

    return ( getPageStartPtr(aPagePtr) +
             getPhyHdrSize() );
}

/* �� ���������� logical hdr�� �����ϴ� �κ��� return */
inline UChar* sdpPhyPage::getCTLStartPtr( UChar   *aPagePtr )
{
    IDE_DASSERT( aPagePtr != NULL );
    IDE_DASSERT( aPagePtr == getPageStartPtr( aPagePtr ) );

    return ( aPagePtr +
             getPhyHdrSize() +
             getLogicalHdrSize( (sdpPhyPageHdr*)aPagePtr ) );
}


/* �� ���������� slot directory�� �����ϴ� �κ��� return */
inline UChar* sdpPhyPage::getSlotDirStartPtr( UChar   *aPagePtr )
{
    UChar    *sStartPtr;

    IDE_DASSERT( aPagePtr != NULL );

    sStartPtr = getPageStartPtr(aPagePtr);

    return ( sStartPtr +
             getPhyHdrSize() +
             getLogicalHdrSize( (sdpPhyPageHdr*)sStartPtr ) +
             getSizeOfCTL( (sdpPhyPageHdr*)sStartPtr ) );
}


/***********************************************************************
 *
 * Description :
 *  slot directory�� ����ġ(end point)�� return.
 *
 *  aPhyPageHdr - [IN] physical page header
 *
 **********************************************************************/
inline UChar* sdpPhyPage::getSlotDirEndPtr( sdpPhyPageHdr    *aPhyPageHdr )
{
    IDE_DASSERT( aPhyPageHdr != NULL );

    return sdpPhyPage::getPagePtrFromOffset((UChar*)aPhyPageHdr,
                                            aPhyPageHdr->mFreeSpaceBeginOffset);
}


/* �� ���������� page footer�� �����ϴ� �κ��� return */
inline UChar* sdpPhyPage::getPageFooterStartPtr( UChar    *aPagePtr )
{
/***********************************************************************
 *
 * Description :
 * +1�Ͽ� align �ϹǷ� �������� ù �ּҿ� ���� �������� ������ �ּҷ�
 * align�ȴ�.
 * ���� : ���� �������� ������ �ּҸ� �ѱ�� �� ���� �������� ù
 * �ּҷ� align �ǹǷ� �̷� ��찡 ���ܼ��� �ȵȴ�.
 *
 **********************************************************************/

    UChar *sPageLastPtr;

    IDE_DASSERT( aPagePtr != NULL );

    sPageLastPtr = (UChar *)idlOS::align( aPagePtr + 1, SD_PAGE_SIZE );

    return ( sPageLastPtr - ID_SIZEOF(sdpPageFooter) );
}


/* �� ���������� physical hdr + logical hdr�� ������
 * data�� ���� ����Ǵ� ���� offset */
inline UInt sdpPhyPage::getDataStartOffset( UInt aLogicalHdrSize )
{
    return ( getPhyHdrSize() +
             idlOS::align8( aLogicalHdrSize ) );
}

/* �� ���������� �����Ͱ� �־������� �������� �������� ���Ѵ�. */
inline UChar* sdpPhyPage::getPageStartPtr( void   *aPagePtr ) // page���� Ư�� pointer
{
    UChar * sPageStartPtr;

    IDE_DASSERT( aPagePtr != NULL );

    sPageStartPtr = (UChar *)idlOS::alignDown( (void*)aPagePtr, (UInt)SD_PAGE_SIZE );

    /* TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::getPageStartPtr �Լ���
     * sdpSlotDirectory::getPageStartPtr�� �ߺ����� �߰��Ѵ�.
     * �� �Լ��� �����Ǹ� ������ �Լ��� �����ؾ� �Ѵ�.*/  
    IDE_DASSERT( sdpSlotDirectory::getPageStartPtr( aPagePtr ) == 
                 sPageStartPtr );

    return sPageStartPtr;
}

/* page�� temp table type���� �˻��Ѵ�. */
#if 0  // not used
inline idBool sdpPhyPage::isPageTempType( UChar   *aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;
    sdpPageType    sPageType;

    sPageHdr  = getHdr(aStartPtr);
    sPageType = getPageType(sPageHdr);

    if( ( sPageType == SDP_PAGE_TEMP_TABLE_DATA ) ||
        ( sPageType == SDP_PAGE_TEMP_TABLE_META ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}
#endif

/* page�� index type���� �˻��Ѵ�. */
inline idBool sdpPhyPage::isPageIndexType( UChar   *aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;
    sPageHdr = getHdr(aStartPtr);
    return ( ( getPageType(sPageHdr) == SDP_PAGE_INDEX_BTREE ) ||
             ( getPageType(sPageHdr) == SDP_PAGE_INDEX_RTREE ) ? 
             ID_TRUE : ID_FALSE );
}

/* page�� index �Ǵ� index meta type���� �˻��Ѵ�. */
inline idBool sdpPhyPage::isPageIndexOrIndexMetaType( UChar   *aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;
    sPageHdr = getHdr(aStartPtr);
    return ( ( getPageType(sPageHdr) == SDP_PAGE_INDEX_BTREE ) ||
             ( getPageType(sPageHdr) == SDP_PAGE_INDEX_RTREE ) ||
             ( getPageType(sPageHdr) == SDP_PAGE_INDEX_META_BTREE ) ||
             ( getPageType(sPageHdr) == SDP_PAGE_INDEX_META_RTREE ) ? 
                ID_TRUE : ID_FALSE);
}

/* page�� TSS table type���� �˻��Ѵ�. */
inline idBool sdpPhyPage::isPageTSSType( UChar *aStartPtr )
{
    sdpPhyPageHdr *sPageHdr;
    sPageHdr = getHdr(aStartPtr);
    return (getPageType(sPageHdr) == SDP_PAGE_TSS ?
            ID_TRUE : ID_FALSE);
}

/* checksum�� ��� */
inline UInt sdpPhyPage::calcCheckSum( sdpPhyPageHdr *aPageHdr )
{
    UChar *sCheckSumStartPtr;
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sCheckSumStartPtr = (UChar *)(&aPageHdr->mFrameHdr.mCheckSum) +
        ID_SIZEOF(aPageHdr->mFrameHdr.mCheckSum);

    return smuUtility::foldBinary( sCheckSumStartPtr,
                                   SD_PAGE_SIZE -
                                   ID_SIZEOF(aPageHdr->mFrameHdr.mCheckSum) );
}

/* �� ���������� �����Ͱ� �־������� Phy page hdr�� ���Ѵ�. */
inline sdpPhyPageHdr* sdpPhyPage::getHdr( UChar *aPagePtr ) // page���� Ư�� pointer
{
    IDE_DASSERT( aPagePtr != NULL );
    return (sdpPhyPageHdr *)getPageStartPtr(aPagePtr);
}


/* physical hdr�� ũ�⸦ ���Ѵ�. 8�� align �Ǿ�� ��.
 * 4�� align �ɶ� ���� ����� ������ ������ ���� �� �ֱ� �����̴�. */
inline UShort sdpPhyPage::getPhyHdrSize()
{
    return idlOS::align8((UShort)ID_SIZEOF(sdpPhyPageHdr));
}

inline UShort sdpPhyPage::getLogicalHdrSize( sdpPhyPageHdr *aPageHdr )
{
    return (aPageHdr->mLogicalHdrSize);
}

inline UShort sdpPhyPage::getSizeOfCTL( sdpPhyPageHdr *aPageHdr )
{
    return (aPageHdr->mSizeOfCTL);
}

inline UShort sdpPhyPage::getSizeOfFooter()
{
    return ID_SIZEOF(sdpPageFooter);
}

/* page pointer�κ��� RID�� ��´�. */
inline sdRID sdpPhyPage::getRIDFromPtr( void *aPtr )
{
    UChar *sPtr =  (UChar*)aPtr;
    UChar *sStartPtr = getPageStartPtr( sPtr );

    IDE_DASSERT( aPtr !=NULL );

    return SD_MAKE_RID( getPageID( sStartPtr ), sPtr - sStartPtr );
}

/* page�� �ϳ��� slot entry�� ������ ������ �ִ� �� �˻��Ѵ�. */
inline idBool sdpPhyPage::canMakeSlotEntry( sdpPhyPageHdr   *aPageHdr )
{

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    if( aPageHdr->mTotalFreeSize >= ID_SIZEOF(sdpSlotEntry) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

inline ULong sdpPhyPage::getSeqNo( sdpPhyPageHdr *aPageHdr )
{
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return aPageHdr->mSeqNo;
}

inline void sdpPhyPage::makeInitPageInfo( sdrInitPageInfo *aInitPageInfo,
                                          scPageID         aPageID,
                                          sdpParentInfo   *aParentInfo,
                                          UShort           aPageState,
                                          sdpPageType      aPageType,
                                          smOID            aTableOID,
                                          UInt             aIndexID )
{
    aInitPageInfo->mPageID = aPageID;

    if( aParentInfo != NULL )
    {
        aInitPageInfo->mParentPID   = aParentInfo->mParentPID;
        aInitPageInfo->mIdxInParent = aParentInfo->mIdxInParent;
    }
    else
    {
        aInitPageInfo->mParentPID   = SD_NULL_PID;
        aInitPageInfo->mIdxInParent = ID_SSHORT_MAX;
    }

    aInitPageInfo->mPageType    = aPageType;
    aInitPageInfo->mPageState   = aPageState;
    aInitPageInfo->mTableOID    = aTableOID;
    aInitPageInfo->mIndexID     = aIndexID;
}

/***********************************************************************
 *
 * Description :
 *  slot directory�� ����ϴ� ���������� ���θ� �˷��ش�.
 *
 *  aPhyPageHdr - [IN] physical page header
 *
 **********************************************************************/
inline idBool sdpPhyPage::isSlotDirBasedPage( sdpPhyPageHdr    *aPageHdr )
{
    if( (aPageHdr->mPageType == SDP_PAGE_INDEX_META_BTREE) ||
        (aPageHdr->mPageType == SDP_PAGE_INDEX_META_RTREE) ||
        (aPageHdr->mPageType == SDP_PAGE_INDEX_BTREE) ||
        (aPageHdr->mPageType == SDP_PAGE_INDEX_RTREE) ||
        (aPageHdr->mPageType == SDP_PAGE_DATA) ||
        (aPageHdr->mPageType == SDP_PAGE_TEMP_TABLE_META) ||
        (aPageHdr->mPageType == SDP_PAGE_TEMP_TABLE_DATA) ||
        (aPageHdr->mPageType == SDP_PAGE_UNDO) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

inline UInt sdpPhyPage::getSizeOfSlotDir( sdpPhyPageHdr *aPageHdr )
{
    UChar     * sSlotDirPtr;
    UShort      sSlotDirSize;

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sSlotDirPtr = getSlotDirStartPtr((UChar*)aPageHdr);

    sSlotDirSize =  sdpSlotDirectory::getSize(sdpPhyPage::getHdr(sSlotDirPtr));

#ifdef DEBUG
    sdpSlotDirHdr * sSlotDirHdr;
    UInt            sSlotCount;

    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
    sSlotCount = ( sSlotDirSize - ID_SIZEOF(sdpSlotDirHdr) ) 
                      / ID_SIZEOF(sdpSlotEntry);

    IDE_DASSERT( sSlotDirHdr->mSlotEntryCount == sSlotCount );
#endif

    return sSlotDirSize;
}

/********************************************************************
 * Description: aPageType���� SDP_PAGE_TYPE_MAX���� ũ��
 *              Invalid�� ID_FALSE, else ID_TRUE
 *
 * aSpaceID  - [IN] TableSpaceID
 * aPageID   - [IN] PageID
 * aPageType - [IN] PageType
 * *****************************************************************/
inline idBool sdpPhyPage::isValidPageType( scSpaceID aSpaceID,
                                           scPageID  aPageID,
                                           UInt      aPageType )
{
    if ( aPageType >= SDP_PAGE_TYPE_MAX )
    {
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_SDP_INVALID_PAGE_INFO,
                     aSpaceID,
                     aPageID,
                     aPageType );

        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}

/***********************************************************************
 * PROJ-1665
 * Description : Consistent Page ���� ��� ��ȯ
 *
 * Implementation :
 *     - In  : Page Header Pointer
 *     - Out : Page consistent ����
 **********************************************************************/
inline idBool sdpPhyPage::isConsistentPage( UChar * aPageHdr )
{
    sdpPhyPageHdr * sPageHdr;

    sPageHdr = (sdpPhyPageHdr *)aPageHdr;

    if ( sPageHdr->mIsConsistent == SDP_PAGE_CONSISTENT )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

#endif // _SDP_PHY_PAGE_H_
