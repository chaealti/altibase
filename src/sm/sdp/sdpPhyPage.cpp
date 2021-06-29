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
 * $Id: sdpPhyPage.cpp 87929 2020-07-03 00:38:42Z et16 $
 *
 * Description :
 *
 * # physical page�� ��� ����
 *
 * page�� physical header�� �����Ѵ�.
 * ���� ��û�� ���� keymap�� �����ϰ� �����ϸ�
 * free offset�� free size�� �����Ѵ�.
 *
 * sdpPhysicalPage����  free offset���� �����Ǵ� �����
 * sdpPageList������ last slot�� ������ ����ϰ�,
 * �̿� �߰��Ͽ� �������� free�� ������ ���� ������ ������ ���̴�.
 *
 * ���� : sdcRecord.cpp
 *
 * # API ��Ģ
 * - �Լ� ���ڷ� �޴� UChar * page ptr�� �������� ������ �������̴�.
 * - �Լ� ���ڷ� �޴� UChar * start ptr�� �������� ���������Դϴ�.
 * - page ptr�� ���ڷ� ���� ���� ���ο����� page �������� ���ؾ� �Ѵ�.
 * - page ptr�� ���ڷ� �޴� ������ layer�� ȣ�⶧���̴�.
 * - ���� layerȤ�� ���� layer���� ȣ��Ǵ� ��� �Լ��� sdpPhyPageHdr*��
 *   ���ڷ� ������, �̸� ȣ���ϴ� �ʿ��� ĳ�����Ͽ� �Ѱܾ� �Ѵ�.
 * - getHdr �Լ���  sdpPhyPageHdr�� ���� �� �ִ�.
 * - getPageStart �Լ��� page�� ���������� ���� �� �ִ�.
 *
 **********************************************************************/

#include <smDef.h>
#include <ide.h>
#include <sdr.h>
#include <smr.h>
#include <smxTrans.h>
#include <smErrorCode.h>
#include <sdpDef.h>
#include <sdpPhyPage.h>
#include <sdpReq.h>
#include <sdpDblPIDList.h>
#include <sdpSglPIDList.h>


/***********************************************************************
 *
 * Description :
 *  physical page�� �ʱ�ȭ�Ѵ�.
 *
 *  aPageHdr    - [IN] physical page header
 *  aPageID     - [IN] page id
 *  aParentInfo - [IN] parent page info
 *  aPageState  - [IN] page state
 *  aPageType   - [IN] page type
 *  aTableOID   - [IN] table oid
 *  aIndexID    - [IN] index id
 **********************************************************************/
IDE_RC sdpPhyPage::initialize( sdpPhyPageHdr  *aPageHdr,
                               scPageID        aPageID,
                               sdpParentInfo  *aParentInfo,
                               UShort          aPageState,
                               sdpPageType     aPageType,
                               smOID           aTableOID,
                               UInt            aIndexID )
{
    sdbFrameHdr       sFrameHdr;
    sdpPageFooter    *sPageFooter;

    IDE_DASSERT( (UChar*)aPageHdr == getPageStartPtr((UChar*)aPageHdr));

    sFrameHdr  = *((sdbFrameHdr*)aPageHdr);

    // XXX: Index�ʿ� ������ �ִ°� �����ϴ�. ���Ŀ� ������ �ʿ��մϴ�.
    idlOS::memset( (void *)aPageHdr, SDP_INIT_PAGE_VALUE, SD_PAGE_SIZE );

    sPageFooter = (sdpPageFooter*) getPageFooterStartPtr( (UChar*)aPageHdr );

    SM_LSN_INIT( sPageFooter->mPageLSN );

    aPageHdr->mFrameHdr = sFrameHdr;

    aPageHdr->mPageID   = aPageID;
    aPageHdr->mPageType = aPageType;

    aPageHdr->mLogicalHdrSize = 0;
    aPageHdr->mSizeOfCTL      = 0;

    aPageHdr->mFreeSpaceBeginOffset =
        getLogicalHdrStartPtr((UChar*)aPageHdr) - (UChar *)aPageHdr;

    aPageHdr->mFreeSpaceEndOffset =
        getPageFooterStartPtr((UChar*)aPageHdr) - (UChar *)aPageHdr;

    aPageHdr->mTotalFreeSize = aPageHdr->mAvailableFreeSize =
        aPageHdr->mFreeSpaceEndOffset - aPageHdr->mFreeSpaceBeginOffset;
    aPageHdr->mPageState     = aPageState;
    aPageHdr->mIsConsistent  = SDP_PAGE_CONSISTENT;

    // ���� �κ��� ���� �ʱ�ȭ�� �ʿ� ������
    // SDP_INIT_PAGE_VALUE ���� 0�� �ƴ� ��쿡
    // �ݵ�� �ʿ��ϴ�. ����� 0�̹Ƿ� �� �ʿ���� �κ���.
    aPageHdr->mFrameHdr.mCheckSum   = 0;
    SM_LSN_INIT( aPageHdr->mFrameHdr.mPageLSN );
    aPageHdr->mFrameHdr.mIndexSMONo = 0;

    aPageHdr->mLinkState      = SDP_PAGE_LIST_UNLINK;
    aPageHdr->mListNode.mNext = SD_NULL_PID;
    aPageHdr->mListNode.mPrev = SD_NULL_PID;

    if ( aParentInfo != NULL )
    {
        aPageHdr->mParentInfo = *aParentInfo;
    }
    else
    {
        aPageHdr->mParentInfo.mParentPID   = SD_NULL_PID;
        aPageHdr->mParentInfo.mIdxInParent = ID_SSHORT_MAX;
    }

    aPageHdr->mTableOID = aTableOID;

    aPageHdr->mIndexID  = aIndexID;

    aPageHdr->mSeqNo = 0;

    return IDE_SUCCESS;
}

/* ------------------------------------------------
 * Description :
 *
 * page �Ҵ�� physical header �ʱ�ȭ �� �α�
 *
 * - �������� SM_PAGE_SIZE��ŭ 0x00�� �ʱ�ȭ�Ѵ�.
 * - ������ free offset�� physical header�� logical header
 *   ���� ��ġ�� set
 *
 * - ���� ������ ����Ͽ� free size�� set�Ѵ�.
 *                      -> sdpPhysicalPage::calcFreeSize
 *                      -> setFreeSize
 *
 * - ������ state�� INSERTABLE�� �Ѵ�.
 * - ��Ÿ ��� ��
 * - SDR_INIT_FILE_PAGE Ÿ�� �α�
 *
 * - extent RID, page ID
 *   �� ���� �ݵ�� write �Ǿ�� �ϰ�, logging �Ǿ�� �Ѵ�.
 *   page id�� extent rid�� page ptr�κ��� space , page id�� ���� �� �ʿ��ϴ�.
 *   extent RID�� mtx �α�� ����Ÿ�� write�ȴ�.
 *
 * �� �Լ��� sdpSegment::allocPage���� ȣ��Ǹ�
 * page�� create�ϰ��� �ϴ� �Լ��� �� �Լ��� ȣ���ϰ� �ȴ�.
 *
 * ----------------------------------------------*/
IDE_RC sdpPhyPage::logAndInit( sdpPhyPageHdr *aPageHdr,
                               scPageID       aPageID,
                               sdpParentInfo *aParentInfo,
                               UShort         aPageState,
                               sdpPageType    aPageType,
                               smOID          aTableOID,
                               UInt           aIndexID,
                               sdrMtx        *aMtx )
{
    sdrInitPageInfo  sInitPageInfo;

    IDE_DASSERT( (UChar*)aPageHdr == getPageStartPtr((UChar*)aPageHdr));
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( ID_SIZEOF( aPageID ) == ID_SIZEOF( UInt ) );

    IDE_TEST( initialize( aPageHdr,
                          aPageID,
                          aParentInfo,
                          aPageState,
                          aPageType,
                          aTableOID,
                          aIndexID ) != IDE_SUCCESS );

    makeInitPageInfo( &sInitPageInfo,
                      aPageID,
                      aParentInfo,
                      aPageState,
                      aPageType,
                      aTableOID,
                      aIndexID );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPageHdr,
                                         &sInitPageInfo,
                                         ID_SIZEOF( sInitPageInfo ),
                                         SDR_SDP_INIT_PHYSICAL_PAGE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �ѹ��� Format�� �������� �ʿ��� ������ Ÿ������ �ʱ�ȭ�Ѵ�. */
IDE_RC sdpPhyPage::setLinkState( sdpPhyPageHdr    * aPageHdr,
                                 sdpPageListState   aLinkState,
                                 sdrMtx           * aMtx )
{
    UShort sLinkState = (UShort)aLinkState;

    IDE_ASSERT( aPageHdr != NULL );
    IDE_ASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&(aPageHdr->mLinkState),
                                      &sLinkState,
                                      ID_SIZEOF(UShort) );
}


IDE_RC sdpPhyPage::create( idvSQL        *aStatistics,
                           scSpaceID      aSpaceID,
                           scPageID       aPageID,
                           sdpParentInfo *aParentInfo,
                           UShort         aPageState,
                           sdpPageType    aType,
                           smOID          aTableOID,
                           UInt           aIndexID,
                           sdrMtx        *aCrtMtx,
                           sdrMtx        *aInitMtx,
                           UChar        **aPagePtr )
{
    UChar         *sPagePtr;
    sdpPhyPageHdr *sPageHdr;

    IDE_ASSERT( aPagePtr != NULL );

    /* Tablespace�� 0�� meta page ���� */
    IDE_TEST( sdbBufferMgr::createPage( aStatistics,
                                        aSpaceID,
                                        aPageID,
                                        aType,
                                        aCrtMtx,
                                        &sPagePtr) != IDE_SUCCESS );
    IDE_ASSERT( sPagePtr != NULL );

    sPageHdr = getHdr( sPagePtr );

    /* ������ page�� physical header �ʱ�ȭ */
    IDE_TEST( sdpPhyPage::logAndInit( sPageHdr,
                                      aPageID,
                                      aParentInfo,
                                      aPageState,
                                      aType, 
                                      aTableOID,
                                      aIndexID,
                                      aInitMtx )
              != IDE_SUCCESS );

    if ( aInitMtx != aCrtMtx )
    {
        IDE_TEST( sdrMiniTrans::setDirtyPage( aInitMtx,
                                              sPagePtr )
                  != IDE_SUCCESS );
    }

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr = NULL;

    return IDE_FAILURE;
}


IDE_RC sdpPhyPage::create4DPath( idvSQL        *aStatistics,
                                 void          *aTrans,
                                 scSpaceID      aSpaceID,
                                 scPageID       aPageID,
                                 sdpParentInfo *aParentInfo,
                                 UShort         aPageState,
                                 sdpPageType    aType,
                                 smOID          aTableOID,
                                 idBool         aIsLogging,
                                 UChar        **aPagePtr )
{
    UChar         *sPagePtr;
    sdpPhyPageHdr *sPageHdr;

    IDE_ASSERT( aPagePtr != NULL );

    /* Tablespace�� 0�� meta page ���� */
    IDE_TEST( sdbDPathBufferMgr::createPage( aStatistics,
                                             aTrans,
                                             aSpaceID,
                                             aPageID,
                                             aIsLogging,
                                             &sPagePtr) != IDE_SUCCESS );
    IDE_ASSERT( sPagePtr !=NULL );

    sPageHdr = getHdr( sPagePtr );

    /* ������ page�� physical header �ʱ�ȭ */
    IDE_TEST( sdpPhyPage::initialize( sPageHdr,
                                      aPageID,
                                      aParentInfo,
                                      aPageState,
                                      aType,
                                      aTableOID,
                                      SM_NULL_INDEX_ID )
              != IDE_SUCCESS );

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr = NULL;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *  �������� �ʱ�ȭ �Ѵ�.
 *  initialize�� ���� �����ϳ� physical, logical header
 *  �� ����Ÿ�� �״�� �ξ�� �Ѵ�.
 *
 *  - logical header ������ �κ��� ���� �ʱ�ȭ �Ѵ�.
 *  - keyslot�� ���� �����Ѵ�.
 *  - free offset, free size�� initialize ���·� set
 *
 *  �� �Լ��� index SMO�������� �߻��Ѵ�.
 *
 *  aPageHdr        - [IN] physical page header
 *  aLogicalHdrSize - [IN] logical header size
 *  aMtx            - [IN] mini Ʈ�����
 *
 **********************************************************************/
IDE_RC sdpPhyPage::reset(
    sdpPhyPageHdr * aPageHdr,
    UInt            aLogicalHdrSize,
    sdrMtx        * aMtx )
{
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    aPageHdr->mLogicalHdrSize = aLogicalHdrSize;
    aPageHdr->mSizeOfCTL      = 0;

    aPageHdr->mFreeSpaceBeginOffset =
        getSlotDirStartPtr( (UChar*)aPageHdr ) - (UChar *)aPageHdr;

    aPageHdr->mFreeSpaceEndOffset =
        getPageFooterStartPtr( (UChar*)aPageHdr )   - (UChar *)aPageHdr;

    aPageHdr->mTotalFreeSize = aPageHdr->mAvailableFreeSize =
        aPageHdr->mFreeSpaceEndOffset - aPageHdr->mFreeSpaceBeginOffset;

    // BUG-17615
    if ( aMtx != NULL )
    {
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)aPageHdr,
                                             &aLogicalHdrSize,
                                             SDR_SDP_4BYTE,
                                             SDR_SDP_RESET_PAGE )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  logical header�� �ʱ�ȭ�Ѵ�.(with logging)
 *
 *  aPageHdr            - [IN]  physical page header
 *  aSize               - [IN]  logical header size
 *  aMtx                - [IN]  mini Ʈ�����
 *  aLogicalHdrStartPtr - [OUT] logical header ������ġ
 *
 **********************************************************************/
IDE_RC  sdpPhyPage::logAndInitLogicalHdr(
                                        sdpPhyPageHdr  *aPageHdr,
                                        UInt            aSize,
                                        sdrMtx         *aMtx,
                                        UChar         **aLogicalHdrStartPtr )
{
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );
    IDE_DASSERT( aSize > 0 );
    IDE_DASSERT( aMtx != NULL );
    IDE_DASSERT( aLogicalHdrStartPtr != NULL );

    initLogicalHdr(aPageHdr, aSize);

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPageHdr,
                                         &aSize,
                                         SDR_SDP_4BYTE,
                                         SDR_SDP_INIT_LOGICAL_HDR )
              != IDE_SUCCESS );

    *aLogicalHdrStartPtr = (UChar*)aPageHdr + getPhyHdrSize();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  logical header�� �ʱ�ȭ�Ѵ�.(no logging)
 *
 *  aPageHdr - [IN]  physical page header
 *  aSize    - [IN]  logical header size
 *
 **********************************************************************/
UChar* sdpPhyPage::initLogicalHdr( sdpPhyPageHdr  *aPageHdr,
                                   UInt            aSize )
{
    UInt sAlignedSize;

    IDE_ASSERT( aSize > 0 );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sAlignedSize= idlOS::align8(aSize);

    aPageHdr->mAvailableFreeSize -= sAlignedSize;
    aPageHdr->mTotalFreeSize     -= sAlignedSize;

    aPageHdr->mFreeSpaceBeginOffset += sAlignedSize;
    aPageHdr->mLogicalHdrSize       = sAlignedSize;

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return (UChar*)aPageHdr + getPhyHdrSize();
}

/***********************************************************************
 *
 * Description : Change Transaction Layer �ʱ�ȭ
 *
 * �������� Chained Transaction Layer�� �ʱ�ȭ�ϰ�,
 * sdpPhyPageHdr�� FreeSize �� ���� Offset�� �����Ѵ�.
 *
 ***********************************************************************/
void sdpPhyPage::initCTL( sdpPhyPageHdr   * aPageHdr,
                          UInt              aHdrSize,
                          UChar          ** aHdrStartPtr )
{
    UInt    sAlignedSize;

    IDE_ASSERT( aHdrSize     > 0 );
    IDE_ASSERT( aPageHdr     != NULL );
    IDE_ASSERT( aHdrStartPtr != NULL );

    sAlignedSize = idlOS::align8( aHdrSize );

    aPageHdr->mSizeOfCTL             = sAlignedSize;
    aPageHdr->mFreeSpaceBeginOffset += sAlignedSize;
    aPageHdr->mTotalFreeSize         =
        aPageHdr->mAvailableFreeSize =
        aPageHdr->mFreeSpaceEndOffset - aPageHdr->mFreeSpaceBeginOffset;

    *aHdrStartPtr = getCTLStartPtr( (UChar*)aPageHdr );

    return;
}


/***********************************************************************
 *
 * Description : Base Touched Transaction Layer Ȯ��
 *
 * CTL�� �ڵ�Ȯ��� Available Free Size���� ����Ѵ�.
 * �ڵ�Ȯ������ ���ؼ� SlotDirEntry�� �Ʒ��� �̵��ϸ�,
 * ������ Compaction�� �߻��Ҽ��� �ִ�.
 *
 ***********************************************************************/
IDE_RC sdpPhyPage::extendCTL( sdpPhyPageHdr * aPageHdr,
                              UInt            aSlotCnt,
                              UInt            aSlotSize,
                              UChar        ** aNewSlotStartPtr,
                              idBool        * aTrySuccess )
{
    UShort  sExtendSize;
    UChar * sSlotDirPtr;
    UChar * sNewSlotStartPtr = NULL;
    idBool  sTrySuccess      = ID_FALSE;

    IDE_ASSERT( aPageHdr  != NULL );
    IDE_ASSERT( aSlotCnt  != 0 );
    IDE_ASSERT( aSlotSize == idlOS::align8( aSlotSize ) );

    sExtendSize = aSlotCnt * aSlotSize;

    /* Check exceed freespace */
    IDE_TEST_CONT( sExtendSize > aPageHdr->mAvailableFreeSize,
                    CONT_ERR_EXTEND );

    if ( sExtendSize > sdpPhyPage::getNonFragFreeSize( aPageHdr ) )
    {
        IDE_TEST_CONT( aPageHdr->mPageType != SDP_PAGE_DATA,
                        CONT_ERR_EXTEND );

        IDE_ERROR( compactPage( aPageHdr, smLayerCallback::getRowPieceSize )
                   == IDE_SUCCESS );
    }

    sSlotDirPtr = getSlotDirStartPtr( (UChar*)aPageHdr );

    IDE_DASSERT( isSlotDirBasedPage(aPageHdr) == ID_TRUE );
    if ( sdpSlotDirectory::getSize(aPageHdr) > 0 )
    {
        shiftSlotDirToBottom(aPageHdr, sExtendSize);
    }

    aPageHdr->mSizeOfCTL            += sExtendSize;
    aPageHdr->mFreeSpaceBeginOffset += sExtendSize;
    aPageHdr->mTotalFreeSize        -= sExtendSize;
    aPageHdr->mAvailableFreeSize    -= sExtendSize;

    /* memmove �ϱ��� ���� SlotDirPtr�� Ȯ���� Ȯ��� CTS��
     * ���� Ptr ���ȴ�.*/
    sTrySuccess      = ID_TRUE;
    sNewSlotStartPtr = sSlotDirPtr;

    IDE_EXCEPTION_CONT( CONT_ERR_EXTEND );

    *aTrySuccess      = sTrySuccess;
    *aNewSlotStartPtr = sNewSlotStartPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  slot directory ��ü�� �Ʒ��� �̵���Ų��.
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aShiftSize  - [IN] shift size
 *
 **********************************************************************/
void sdpPhyPage::shiftSlotDirToBottom(sdpPhyPageHdr     *aPageHdr,
                                      UShort             aShiftSize)
{
    UChar * sSlotDirPtr;

    sSlotDirPtr = getSlotDirStartPtr( (UChar*)aPageHdr );

    idlOS::memmove( sSlotDirPtr + aShiftSize,  /* dest ptr */
                    sSlotDirPtr,               /* src ptr  */
                    sdpSlotDirectory::getSize(aPageHdr) );
}


/******************************************************************************
 * Description :
 *  slot �Ҵ��� �������� ���θ� �˷��ش�.
 *
 *  aPageHdr            - [IN] physical page header
 *  aSlotSize           - [IN] �����Ϸ��� slot�� ũ��
 *  aNeedAllocSlotEntry - [IN] slot entry �Ҵ��� �ʿ����� ����
 *  aSlotAlignValue     - [IN] align value
 *
 ******************************************************************************/
idBool sdpPhyPage::canAllocSlot( sdpPhyPageHdr *aPageHdr,
                                 UInt           aSlotSize,
                                 idBool         aNeedAllocSlotEntry,
                                 UShort         aSlotAlignValue )
{
    UShort    sAvailableSize        = 0;
    UShort    sExtraSize            = 0;
    UShort    sReserveSize4Chaining;
    UShort    sMinRowPieceSize;

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    IDE_TEST_CONT( aSlotSize > SD_PAGE_SIZE, CONT_EXCEED_PAGESIZE );

    if ( aPageHdr->mPageType == SDP_PAGE_DATA )
    {
        if ( aNeedAllocSlotEntry == ID_TRUE )
        {
            if ( sdpSlotDirectory::isExistUnusedSlotEntry(
                    getSlotDirStartPtr((UChar*)aPageHdr))
                != ID_TRUE )
            {
                sExtraSize = ID_SIZEOF(sdpSlotEntry);
            }

            sMinRowPieceSize = smLayerCallback::getMinRowPieceSize();
            /* BUG-25395
             * [SD] alloc slot, free slot�ÿ�
             * row chaining�� ���� ����(6byte)�� ����ؾ� �մϴ�. */
            if ( aSlotSize < sMinRowPieceSize )
            {
                /* slot�� ũ�Ⱑ minimum rowpiece size���� ���� ���,
                 * row migration�� �߻������� �������� ��������� �����ϸ�
                 * slot ���Ҵ翡 �����Ͽ� ������ ����Ѵ�.
                 * �׷��� �̷� ��쿡 ����Ͽ� alloc slot�ÿ�
                 * (min rowpiece size - slot size)��ŭ�� ������
                 * �߰��� �� Ȯ���صд�.
                 */
                sReserveSize4Chaining  = sMinRowPieceSize - aSlotSize;
                sExtraSize            += sReserveSize4Chaining;
            }
        }

        sAvailableSize = aPageHdr->mAvailableFreeSize;
    }
    else
    {
        if ( aNeedAllocSlotEntry == ID_TRUE )
        {
            sExtraSize = ID_SIZEOF(sdpSlotEntry);
        }

        sAvailableSize =
            aPageHdr->mFreeSpaceEndOffset - aPageHdr->mFreeSpaceBeginOffset;
    }

    if ( sAvailableSize >=
        ( idlOS::align(aSlotSize, aSlotAlignValue) + sExtraSize ) )
    {
        return ID_TRUE;
    }

    IDE_EXCEPTION_CONT( CONT_EXCEED_PAGESIZE );

    return ID_FALSE;
}


/***********************************************************************
 *
 * Description :
 *  ���������� slot�� �Ҵ��Ѵ�.
 *
 *  allocSlot() �Լ����� ������
 *  1. �ʿ�� �ڵ����� compact page ������ �����Ѵ�.
 *  2. slot directory�� shift���� �ʴ´�.
 *  3. align�� ������� �ʴ´�.
 *
 *  aPageHdr         - [IN]  ������ ���
 *  aSlotSize        - [IN]  �Ҵ��Ϸ��� slot�� ũ��
 *  aAllocSlotPtr    - [OUT] �Ҵ���� slot�� ���� pointer
 *  aAllocSlotNum    - [OUT] �Ҵ���� slot entry number
 *  aAllocSlotOffset - [OUT] �Ҵ���� slot�� offfset
 *
 **********************************************************************/
IDE_RC sdpPhyPage::allocSlot4SID( sdpPhyPageHdr       *aPageHdr,
                                  UShort               aSlotSize,
                                  UChar              **aAllocSlotPtr,
                                  scSlotNum           *aAllocSlotNum,
                                  scOffset            *aAllocSlotOffset )
{
    UChar       *sAllocSlotPtr;
    scSlotNum    sAllocSlotNum;
    scOffset     sAllocSlotOffset;
    UShort       sReserveSize4Chaining;
    UShort       sMinRowPieceSize;

    IDE_DASSERT( aPageHdr != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    /* ����� data page������ �� �Լ��� ����Ѵ�. */
    IDE_ERROR_MSG( aPageHdr->mPageType == SDP_PAGE_DATA,
                   "aPageHdr->mPageType : %"ID_UINT32_FMT,
                   aPageHdr->mPageType );

    /* �� �Լ��� ȣ���ϱ� ���� canAllocSlot�� ����
     *  �ݵ�� alloc �������� Ȯ���ϰ�, �����Ҷ��� ȣ���ؾ� �Ѵ�. */
    IDE_ERROR( canAllocSlot( aPageHdr,
                             aSlotSize,
                             ID_TRUE /* aNeedAllocSlotEntry */,
                             SDP_1BYTE_ALIGN_SIZE /* align value */ )
               == ID_TRUE );

    if ( (aSlotSize + ID_SIZEOF(sdpSlotEntry)) > getNonFragFreeSize(aPageHdr) )
    {
        /* FSEO�� FSBO ������ ������ ������ ���
         * compact page ������ �����Ͽ�
         * ����ȭ�� �������� ������
         * ���ӵ� freespace�� Ȯ���Ѵ�. */
        IDE_TEST( compactPage( aPageHdr, smLayerCallback::getRowPieceSize )
                  != IDE_SUCCESS );
    }

    /* aSlotSize ��ŭ�� ������ �������κ��� �Ҵ��Ѵ�. */
    IDE_TEST( allocSlotSpace( aPageHdr,
                              aSlotSize,
                              &sAllocSlotPtr,
                              &sAllocSlotOffset )
              != IDE_SUCCESS );

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    /* slot directory�κ��� �ϳ��� slot entry�� �Ҵ��Ѵ�. */
    IDE_ERROR( sdpSlotDirectory::alloc( aPageHdr,
                                        sAllocSlotOffset,
                                        &sAllocSlotNum )
               == IDE_SUCCESS );

    if ( aPageHdr->mPageType == SDP_PAGE_DATA )
    {
        sMinRowPieceSize = smLayerCallback::getMinRowPieceSize();
        /* BUG-25395
         * [SD] alloc slot, free slot�ÿ�
         * row chaining�� ���� ����(6byte)�� ����ؾ� �մϴ�. */
        if ( aSlotSize < sMinRowPieceSize )
        {
            /* slot�� ũ�Ⱑ minimum rowpiece size���� ���� ���,
             * row migration�� �߻������� �������� ��������� �����ϸ�
             * slot ���Ҵ翡 �����Ͽ� ������ ����Ѵ�.
             * �׷��� �̷� ��쿡 ����Ͽ�
             * (min rowpiece size - slot size)��ŭ�� ������
             * �߰��� �� Ȯ���صд�.
             */
            sReserveSize4Chaining = sMinRowPieceSize - aSlotSize;

            IDE_ERROR( aPageHdr->mAvailableFreeSize >= sReserveSize4Chaining );
            IDE_ERROR( aPageHdr->mTotalFreeSize     >= sReserveSize4Chaining );

            aPageHdr->mTotalFreeSize     -= sReserveSize4Chaining;
            aPageHdr->mAvailableFreeSize -= sReserveSize4Chaining;

            IDE_DASSERT( sdpPhyPage::validate(aPageHdr) == ID_TRUE );
        }
    }

    *aAllocSlotPtr    = sAllocSlotPtr;
    *aAllocSlotNum    = sAllocSlotNum;
    *aAllocSlotOffset = sAllocSlotOffset;

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::logMem( IDE_DUMP_0, 
                    (UChar *)aPageHdr, 
                    SD_PAGE_SIZE,
                    "===================================================\n"
                    "         slot size : %u\n"
                    "               PID : %u\n",
                    aSlotSize,
                    aPageHdr->mPageID );

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *   ���������� slot�� �Ҵ��Ѵ�.
 *
 *  allocSlot4SID() �Լ����� ������
 *  1. �ڵ����� compact page ������ �������� �ʴ´�.
 *  2. slot directory�� shift�Ѵ�.
 *  3. ����ÿ� align�� ����Ѵ�.
 *
 *  aPageHdr            - [IN] ������ ���
 *  aSlotNum            - [IN] �Ҵ��Ϸ��� slot number
 *  aSaveSize           - [IN] �Ҵ��Ϸ��� slot�� ũ��
 *  aNeedAllocSlotEntry - [IN] slot entry �Ҵ��� �ʿ����� ����
 *  aAllowedSize        - [OUT] �������κ��� �Ҵ���� ������ ũ��
 *  aAllocSlotPtr       - [OUT] �Ҵ���� slot�� ���� pointer
 *  aAllocSlotOffset    - [OUT] �Ҵ���� slot�� offfset
 *  aSlotAlignValue     - [IN] ����ؾ� �ϴ� align ũ��
 *
 **********************************************************************/
IDE_RC sdpPhyPage::allocSlot( sdpPhyPageHdr       *aPageHdr,
                              scSlotNum            aSlotNum,
                              UShort               aSaveSize,
                              idBool               aNeedAllocSlotEntry,
                              UShort              *aAllowedSize,
                              UChar              **aAllocSlotPtr,
                              scOffset            *aAllocSlotOffset,
                              UChar                aSlotAlignValue )
{
    UShort    sAlignedSize = idlOS::align(aSaveSize, aSlotAlignValue);
    UChar    *sAllocSlotPtr;
    UShort    sAllocSlotOffset;

    IDE_ERROR( aPageHdr != NULL );
    IDE_ERROR( ( getPageType(aPageHdr) != SDP_PAGE_UNFORMAT ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_DATA ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_INDEX ) );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    /* data page������ allocSlot()�� �ƴϰ� allocSlot4SID() �Լ��� ����ؾ� �Ѵ�. */
    IDE_ERROR( aPageHdr->mPageType != SDP_PAGE_DATA );

    /* �� �Լ��� ȣ���ϱ� ���� canAllocSlot�� ����
     *  �ݵ�� alloc �������� Ȯ���ϰ�, �����Ҷ��� ȣ���ؾ� �Ѵ�. */
    IDE_ERROR( canAllocSlot( aPageHdr,
                             sAlignedSize,
                             aNeedAllocSlotEntry,
                             aSlotAlignValue )
               == ID_TRUE );

    /* sAlignedSize ��ŭ�� ������ �������κ��� �Ҵ��Ѵ�. */
    IDE_TEST( allocSlotSpace( aPageHdr,
                              sAlignedSize,
                              &sAllocSlotPtr,
                              &sAllocSlotOffset )
              != IDE_SUCCESS );

    /* �ʿ��� ���, slot directory�κ��� �ϳ��� slot entry�� �Ҵ��Ѵ�. */
    if ( aNeedAllocSlotEntry == ID_TRUE )
    {
        sdpSlotDirectory::allocWithShift( aPageHdr,
                                          aSlotNum,
                                          sAllocSlotOffset );
    }
    else
    {
        /* nothing to do */
    }

    *aAllowedSize     = sAlignedSize;
    *aAllocSlotPtr    = sAllocSlotPtr;
    *aAllocSlotOffset = sAllocSlotOffset;

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_SERVER_0, 
                 "Aligned size          : %u\n"
                 "Need alloc slot entry : %u\n"
                 "Slot align value      : %u\n"
                 "Slot number           : %u\n"
                 "Save size             : %u\n", 
                 sAlignedSize,
                 aNeedAllocSlotEntry,
                 aSlotAlignValue,
                 aSlotNum,
                 aSaveSize );
    ideLog::logMem( IDE_SERVER_0, (UChar *)aPageHdr, SD_PAGE_SIZE );

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description :
 *  slot�� ������ ������ �Ҵ��Ѵ�.
 *
 *  aPageHdr         - [IN] ������ ���
 *  aSaveSize        - [IN] �Ҵ��Ϸ��� slot�� ũ��
 *  aAllocSlotPtr    - [OUT] �Ҵ���� slot�� ���� pointer
 *  aAllocSlotOffset - [OUT] �Ҵ���� slot�� offfset
 *
 **********************************************************************/
IDE_RC sdpPhyPage::allocSlotSpace( sdpPhyPageHdr       *aPageHdr,
                                   UShort               aSaveSize,
                                   UChar              **aAllocSlotPtr,
                                   scOffset            *aAllocSlotOffset )
{
    scOffset    sSlotOffset   = 0;

    IDE_DASSERT( aPageHdr != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    // BUG-30462 Page�� Max Size�� �ʰ��Ͽ� �����Ϸ��� ���� ���Ͽ�
    //           �����ڵ带 �߰��մϴ�.
    IDE_ERROR( aPageHdr->mFreeSpaceEndOffset >= aSaveSize );
    IDE_ERROR( aPageHdr->mTotalFreeSize      >= aSaveSize );
    IDE_ERROR( aPageHdr->mAvailableFreeSize  >= aSaveSize );

    aPageHdr->mFreeSpaceEndOffset -= aSaveSize;
    sSlotOffset = aPageHdr->mFreeSpaceEndOffset;

    aPageHdr->mTotalFreeSize      -= aSaveSize;
    aPageHdr->mAvailableFreeSize  -= aSaveSize;

    *aAllocSlotOffset = sSlotOffset;
    *aAllocSlotPtr    = getPagePtrFromOffset( (UChar*)aPageHdr,
                                              sSlotOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    tracePage( IDE_DUMP_0, 
               (UChar*)aPageHdr,
               "failure allocSlotSpace\n"
               "----------------------\n"
               "aSaveSize : %"ID_UINT32_FMT,
               aSaveSize );

    return IDE_FAILURE;

}


/***********************************************************************
 * Description :
 *
 * BUG-47580 1. compact page ����� ���� ����
 *           2. Assert -> Compact ���� page�� �����ϰ� ����ó��
 *
 **********************************************************************/
IDE_RC sdpPhyPage::compactPage( sdpPhyPageHdr        * aPageHdr,
                                sdpGetSlotSizeFunc     aGetSlotSizeFunc )
{
    ULong       sOldPageImage[ SD_PAGE_SIZE / ID_SIZEOF(ULong) ];
    UChar      *sSlotDirPtr;
    UChar      *sOldSlotPtr;
    UChar      *sNewSlotPtr;
    UShort      sOldSlotOffset = 0;
    UShort      sNewSlotOffset = 0;
    UShort      sFreeSpaceEndOffset = 0;
    UShort      sSlotSize;
    UShort      sCount;
    scSlotNum   sSlotNum;

    IDE_ASSERT( aPageHdr            != NULL );
    IDE_ASSERT( aGetSlotSizeFunc    != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    idlOS::memcpy( (UChar*)sOldPageImage, (UChar*)aPageHdr, SD_PAGE_SIZE );

    IDE_TEST( ((aPageHdr->mPageType != SDP_PAGE_DATA) &&
               (aPageHdr->mPageType != SDP_PAGE_TEMP_TABLE_META) &&
               (aPageHdr->mPageType != SDP_PAGE_TEMP_TABLE_DATA)) );

    sFreeSpaceEndOffset =
        getPageFooterStartPtr((UChar*)aPageHdr) - (UChar *)aPageHdr;

    sSlotDirPtr = getSlotDirStartPtr((UChar*)aPageHdr);
    sCount      = sdpSlotDirectory::getCount(sSlotDirPtr);

    for( sSlotNum = 0; sSlotNum < sCount; sSlotNum++ )
    {
        if ( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, sSlotNum)
            == ID_TRUE )
        {
            IDE_TEST( aPageHdr->mPageType != SDP_PAGE_DATA );
        }
        else
        {
            IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr,
                                                  sSlotNum,
                                                  &sOldSlotOffset )
                      != IDE_SUCCESS );
            sOldSlotPtr = (UChar*)sOldPageImage + sOldSlotOffset;

            sSlotSize = aGetSlotSizeFunc( sOldSlotPtr );

            // BUG-47580 compact page ����� ���� ����
            // 1. �����ִ� Free size (end - begin) ���� slot size�� �� ū ���
            //   or
            // 2. �����ִ� �������� Free size (end - begin - slot size) ��
            //    page header�� ��ϵ� free size ���� ���� ���
            IDE_TEST_RAISE( ( sSlotSize >
                              (sFreeSpaceEndOffset -
                               aPageHdr->mFreeSpaceBeginOffset )) ||
                            ( aPageHdr->mTotalFreeSize >
                              ( sFreeSpaceEndOffset - sSlotSize -
                                aPageHdr->mFreeSpaceBeginOffset )),
                            err_invalid_slotsize );
            IDU_FIT_POINT_RAISE( "BUG-47580@sdpPhyPage::compactPage", err_invalid_slotsize );

            sFreeSpaceEndOffset -= sSlotSize;
            sNewSlotOffset       = sFreeSpaceEndOffset;
            sNewSlotPtr          = getPagePtrFromOffset( (UChar*)aPageHdr,
                                                         sNewSlotOffset );
            idlOS::memcpy( sNewSlotPtr, sOldSlotPtr, sSlotSize );

            sdpSlotDirectory::setValue( sSlotDirPtr,
                                        sSlotNum,
                                        sNewSlotOffset );
        }
    }

    aPageHdr->mFreeSpaceEndOffset = sFreeSpaceEndOffset;

    IDE_TEST( aPageHdr->mTotalFreeSize >
              (aPageHdr->mFreeSpaceEndOffset -
               aPageHdr->mFreeSpaceBeginOffset) );

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_slotsize )
    {
        ideLog::log(IDE_ERR_0,
                    "compactPage Failure\n"
                    "Table OID: %"ID_vULONG_FMT", "
                    "Page ID: %"ID_UINT32_FMT", "
                    "Page Type: %"ID_UINT32_FMT"\n"
                    "Total Free Size: %"ID_UINT32_FMT", "
                    "Free Space Offset: %"ID_UINT32_FMT" - %"ID_UINT32_FMT", "
                    "Slot Offset: %"ID_UINT32_FMT", "
                    "Slot Size: %"ID_UINT32_FMT"\n",
                    aPageHdr->mTableOID,
                    aPageHdr->mPageID,
                    aPageHdr->mPageType,
                    aPageHdr->mTotalFreeSize ,
                    aPageHdr->mFreeSpaceBeginOffset,
                    sFreeSpaceEndOffset,
                    sOldSlotOffset,
                    sSlotSize );
    }
    IDE_EXCEPTION_END;

    // �۾����̴� page ���
    sdpPhyPage::tracePage( IDE_DUMP_0,
                           (UChar*)aPageHdr,
                           "compactPage Failure\n"
                           "NEW PAGE:");
    // �����ϰ� ����ó����.
    idlOS::memcpy( (UChar*)aPageHdr,
                   (UChar*)sOldPageImage,
                   SD_PAGE_SIZE );

    // �۾� ������ ���·� �����ϰ� page ���
    sdpPhyPage::tracePage( IDE_DUMP_0,
                           (UChar*)aPageHdr,
                           "compactPage Failure\n"
                           "OLD PAGE:");

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  ���������� slot�� �����Ѵ�.
 *
 *  freeSlot() �Լ����� ������
 *  1. slot directory�� shift���� �ʴ´�.
 *  2. align�� ������� �ʴ´�.
 *
 *  aPageHdr  - [IN]  ������ ���
 *  aSlotPtr  - [IN]  �����Ϸ��� slot�� pointer
 *  aSlotNum  - [IN]  �����Ϸ��� slot�� entry number
 *  aSlotSize - [IN]  �����Ϸ��� slot�� size
 *
 **********************************************************************/
IDE_RC sdpPhyPage::freeSlot4SID( sdpPhyPageHdr      *aPageHdr,
                                 UChar              *aSlotPtr,
                                 scSlotNum           aSlotNum,
                                 UShort              aSlotSize )
{
    UShort      sReserveSize4Chaining;
    UShort      sMinRowPieceSize;
    scOffset    sSlotOffset;

    IDE_ERROR( aPageHdr != NULL );
    IDE_ERROR( aSlotPtr != NULL );
    IDE_ERROR( aPageHdr->mPageType == SDP_PAGE_DATA );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sSlotOffset = getOffsetFromPagePtr( (UChar*) aSlotPtr);

    IDE_ERROR( sdpSlotDirectory::free( aPageHdr, aSlotNum ) 
               == IDE_SUCCESS );

    freeSlotSpace( aPageHdr, sSlotOffset, aSlotSize );

    if ( aPageHdr->mPageType == SDP_PAGE_DATA )
    {
        sMinRowPieceSize = smLayerCallback::getMinRowPieceSize();
        /* BUG-25395
         * [SD] alloc slot, free slot�ÿ�
         * row chaining�� ���� ����(6byte)�� ����ؾ� �մϴ�. */
        if ( aSlotSize < sMinRowPieceSize )
        {
            /* slot�� ũ�Ⱑ minimum rowpiece size���� ���� ���,
             * alloc slot�ÿ� (min rowpiece size - slot size)��ŭ�� ������
             * �߰��� �� �Ҵ��Ͽ���.
             * ( sdpPageList::allocSlot4SID() �Լ� ���� )
             * �׷��� free slot�ÿ� �߰��� �Ҵ��� ������ �������־�� �Ѵ�.
             */
            sReserveSize4Chaining = sMinRowPieceSize - aSlotSize;

            aPageHdr->mTotalFreeSize     += sReserveSize4Chaining;
            aPageHdr->mAvailableFreeSize += sReserveSize4Chaining;

            IDE_DASSERT( sdpPhyPage::validate(aPageHdr) == ID_TRUE );
        }
    }

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    tracePage( IDE_DUMP_0, 
               (UChar*)aPageHdr,
               "failure freeSlot4SID\n"
               "----------------------\n"
               "aSlotNum  : %"ID_UINT32_FMT,
               "aSlotSize : %"ID_UINT32_FMT,
               aSlotNum,
               aSlotSize );


    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  ���������� slot�� �����Ѵ�.
 *
 *  freeSlot4SID() �Լ����� ������
 *  1. slot directory�� shift�Ѵ�.
 *  2. ����ÿ� align�� ����Ѵ�.
 *
 *  aPageHdr           - [IN] ������ ���
 *  aSlotPtr           - [IN] �����Ϸ��� slot�� pointer
 *  aFreeSize          - [IN] �����Ϸ��� slot�� size
 *  aNeedFreeSlotEntry - [IN] slot entry�� �������� ����
 *  aSlotAlignValue    - [IN] align value
 *
 **********************************************************************/
IDE_RC sdpPhyPage::freeSlot( sdpPhyPageHdr      *aPageHdr,
                             UChar              *aSlotPtr,
                             UShort              aFreeSize,
                             idBool              aNeedFreeSlotEntry,
                             UChar               aSlotAlignValue )
{
    UChar      *sSlotDirPtr;
    SShort      sSlotNum = -1;
    scOffset    sSlotOffset;
    UShort      sAlignedSize = idlOS::align(aFreeSize, aSlotAlignValue);

    IDE_ERROR( aPageHdr != NULL );
    IDE_ERROR( aSlotPtr != NULL );
    IDE_ERROR( ( getPageType(aPageHdr) != SDP_PAGE_UNFORMAT ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_DATA ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_INDEX ) );
    IDE_ERROR( aPageHdr->mPageType != SDP_PAGE_DATA );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sSlotOffset = getOffsetFromPagePtr(aSlotPtr);

    if ( aNeedFreeSlotEntry == ID_TRUE )
    {
        sSlotDirPtr = getSlotDirStartPtr((UChar*)aPageHdr);

        IDE_ERROR( sdpSlotDirectory::find(sSlotDirPtr, sSlotOffset, &sSlotNum)
                   == IDE_SUCCESS );
        IDE_ERROR( sSlotNum >=0 );

        IDE_ERROR( sdpSlotDirectory::freeWithShift( aPageHdr, sSlotNum )
                   == IDE_SUCCESS );
    }

    freeSlotSpace( aPageHdr, sSlotOffset, sAlignedSize );

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    tracePage( IDE_DUMP_0, 
               (UChar*)aPageHdr,
               "failure freeSlot\n"
               "----------------------\n"
               "aFreeSize          : %"ID_UINT32_FMT,
               "aNeedFreeSlotEntry : %"ID_UINT32_FMT,
               "aSlotAlignValue    : %"ID_UINT32_FMT,
               "sSlotNum           : %"ID_UINT32_FMT,
               aFreeSize,
               aNeedFreeSlotEntry,
               aSlotAlignValue,
               sSlotNum );


    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  ���������� slot�� �����Ѵ�.
 *
 *  freeSlot4SID() �Լ����� ������
 *  1. slot directory�� shift�Ѵ�.
 *  2. ����ÿ� align�� ����Ѵ�.
 *
 *  aPageHdr           - [IN] ������ ���
 *  aSlotNum           - [IN] �����Ϸ��� slot�� entry number
 *  aFreeSize          - [IN] �����Ϸ��� slot�� size
 *  aSlotAlignValue    - [IN] align value
 *
 **********************************************************************/
IDE_RC sdpPhyPage::freeSlot(sdpPhyPageHdr   *aPageHdr,
                            scSlotNum        aSlotNum,
                            UShort           aFreeSize,
                            UChar            aSlotAlignValue)
{
    UChar      *sSlotDirPtr  = NULL;
    scOffset    sSlotOffset  = 0;
    UShort      sAlignedSize = idlOS::align(aFreeSize, aSlotAlignValue);

    IDE_ERROR( aPageHdr != NULL );
    IDE_ERROR( ( getPageType(aPageHdr) != SDP_PAGE_UNFORMAT ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_DATA ) &&
               ( getPageType(aPageHdr) != SDP_PAGE_LOB_INDEX ) );
    IDE_ERROR( aPageHdr->mPageType != SDP_PAGE_DATA );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    sSlotDirPtr = getSlotDirStartPtr((UChar*)aPageHdr);
    IDE_TEST( sdpSlotDirectory::getValue( sSlotDirPtr, aSlotNum, &sSlotOffset )
              != IDE_SUCCESS);

    IDE_ERROR( sdpSlotDirectory::freeWithShift( aPageHdr, aSlotNum )
               == IDE_SUCCESS);
    freeSlotSpace( aPageHdr, sSlotOffset, sAlignedSize );

    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    tracePage( IDE_DUMP_0, 
               (UChar*)aPageHdr,
               "failure freeSlot4SID\n"
               "----------------------\n"
               "aSlotNum        : %"ID_UINT32_FMT
               "aFreeSize       : %"ID_UINT32_FMT
               "aSlotAlignValue : %"ID_UINT32_FMT,
               aSlotNum,
               aFreeSize,
               aSlotAlignValue );


    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *
 *
 * Implementation :
 *
 *
 **********************************************************************/
void sdpPhyPage::freeSlotSpace( sdpPhyPageHdr        *aPageHdr,
                                scOffset              aSlotOffset,
                                UShort                aFreeSize)
{
    IDE_DASSERT( aPageHdr != NULL );
    IDE_DASSERT( validate(aPageHdr) == ID_TRUE );

    if ( aPageHdr->mFreeSpaceEndOffset == aSlotOffset )
    {
        aPageHdr->mFreeSpaceEndOffset += aFreeSize;
    }

    aPageHdr->mTotalFreeSize     += aFreeSize;
    aPageHdr->mAvailableFreeSize += aFreeSize;
}


/***********************************************************************
 *
 * Description : �� �������� ����� �ƴ��� �˻��Ѵ�.
 *               BUG-47953 ���� Ȯ�� ������ �ϴ� �Լ��� �ʿ��ϴ�.
 *
 * Implementation :
 * if ( LSN üũ�� )
 *     ù LSN�� ������ LSN ��
 *
 * else
 *     �������� üũ���� ���Ѵ�.
 *     if ( �ٸ��ٸ�  )
 *         return true
 *     return false
 *
 **********************************************************************/
idBool sdpPhyPage::isPageCorrupted( UChar  * aStartPtr )
{

    idBool isCorrupted = ID_TRUE;
    sdpPhyPageHdr *sPageHdr;
    sdpPageFooter *sPageFooter;
    smLSN          sHeadLSN;
    smLSN          sTailLSN; 

    sPageHdr    = getHdr(aStartPtr);
    sPageFooter = (sdpPageFooter*)
        getPageFooterStartPtr((UChar*)sPageHdr);

    if ( smuProperty::getCheckSumMethod() == SDP_CHECK_LSN )
    {
        sHeadLSN = getPageLSN((UChar*)sPageHdr);
        sTailLSN = sPageFooter->mPageLSN;

        isCorrupted = smrCompareLSN::isEQ( &sHeadLSN, &sTailLSN ) ?
            ID_FALSE : ID_TRUE;
    }
    else
    {
        if ( smuProperty::getCheckSumMethod() == SDP_CHECK_CHECKSUM )
        {
            // BUG-45598: checksum ���� �ʱ���� ���, �̹���  LSN ���� �˻�  
            if( getCheckSum(sPageHdr) == SDP_CHECKSUM_INIT_VAL )
            {
                sHeadLSN = getPageLSN((UChar*)sPageHdr);
                sTailLSN = sPageFooter->mPageLSN;
             
                isCorrupted = smrCompareLSN::isEQ( &sHeadLSN, &sTailLSN ) ?
                    ID_FALSE : ID_TRUE;
            }
            else
            {
                isCorrupted =
                    ( getCheckSum(sPageHdr) != calcCheckSum(sPageHdr) ?
                      ID_TRUE : ID_FALSE );
            }
        }
        else
        {
            IDE_DASSERT( ID_FALSE );
        }
    }
    return isCorrupted;
}

/***********************************************************************
 * Description : �� �������� ����� �ƴ��� �˻��ϰ�,
 *               ������ ������ inconsistent �� �����Ѵ�.
 *
 * return : sdpPhyPage::isPageCorrupted() �� ����
 *          ID_TRUE  : Corrupted, Inconsistent Page
 *          ID_FALSE : Consistent Page
 **********************************************************************/
idBool sdpPhyPage::checkAndSetPageCorrupted( scSpaceID  aSpaceID,
                                             UChar     *aStartPtr )
{
    idBool isCorrupted ;
    sdpPhyPageHdr *sPageHdr;

    isCorrupted = isPageCorrupted( aStartPtr );

    // BUG-45598: sdpPhyPage::isPageCorrupted ���� inconsistent ����
    if ( isCorrupted == ID_TRUE )
    {
        sPageHdr = getHdr(aStartPtr);
        if ( ( smrRecoveryMgr::isRestart() == ID_FALSE ) &&
             ( isConsistentPage( aStartPtr ) == ID_TRUE ) )
        {
            setPageInconsistency( sPageHdr );
        }

        // To verify CASE-6829
        if ( smuProperty::getSMChecksumDisable() == 1 )
        {
            ideLog::log(IDE_SM_0,
                        "[WARNING!!] Page(%u,%u) is corrupted\n",
                        aSpaceID,
                        sPageHdr->mPageID);
            return ID_FALSE;
        }
    }
    return isCorrupted;
}

/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 **********************************************************************/
IDE_RC sdpPhyPage::verify( sdpPhyPageHdr *aPageHdr,
                           scPageID       aSpaceID,
                           scPageID       aPageID )
{

    const UInt sErrorLen = 512;
    SChar      sErrorMsg[sErrorLen];

    IDE_DASSERT( (UChar*)aPageHdr == getPageStartPtr((UChar*)aPageHdr));

    if ( aSpaceID != getSpaceID((UChar*)aPageHdr) )
    {

        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_PHY_ERROR1,
                                aSpaceID,
                                getSpaceID((UChar*)aPageHdr));

        IDE_RAISE(err_verify_failed);
    }


    if ( aPageID != aPageHdr->mPageID )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_PHY_ERROR2,
                                (UInt)aPageID,
                                (UInt)aPageHdr->mPageID);

        IDE_RAISE(err_verify_failed);
    }


    if ( aPageHdr->mTotalFreeSize > SD_PAGE_SIZE )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_PHY_ERROR3,
                                aSpaceID,
                                (UInt)aPageHdr->mPageID,
                                SD_PAGE_SIZE,
                                (UInt)aPageHdr->mTotalFreeSize,
                                aPageHdr->mFreeSpaceEndOffset);

        IDE_RAISE(err_verify_failed);
    }

    if ( aPageHdr->mFreeSpaceEndOffset > SD_PAGE_SIZE )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_PHY_ERROR4,
                                aSpaceID,
                                (UInt)aPageHdr->mPageID,
                                SD_PAGE_SIZE,
                                (UInt)aPageHdr->mTotalFreeSize,
                                (UInt)aPageHdr->mFreeSpaceEndOffset);

        IDE_RAISE(err_verify_failed);
    }

    if ( aPageHdr->mPageType >= (UShort)SDP_PAGE_TYPE_MAX)
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_PHY_ERROR5,
                                aSpaceID,
                                (UInt)aPageHdr->mPageID,
                                (UInt)aPageHdr->mPageType);

        IDE_RAISE(err_verify_failed);
    }



    return IDE_SUCCESS;

    IDE_EXCEPTION(err_verify_failed)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidDB,
                                sErrorMsg));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * Description :
 *
 * if ( aIsConsistent == 'F' )
 *     physical header�� mIsConsistent�� 'F'�� logging�Ѵ�.
 * else // aIsConsistent == 'T'
 *     physical header�� mIsConsistent�� 'T'�� �����Ѵ�. // nologging
 *
 * log���� ������ page�� ����, �������� operation(logging mode operation)��
 * ����� �� media failure�� �߻����� �� redo ���൵�� server�� �״� ������
 * �߻��Ѵ�. (nologging noforce�� ������ index�� ���, index build��
 * ���ſ����� ����ǰ� failure�� �߻����� ���� redo ���൵�� server�� �״´�)
 * �̸� �����ϱ� ���� page�� physical header�� page�� logical redo�� �� �ִ���
 * ���θ� ǥ���ϴ� mIsConsistent�� �߰��ϰ� nologging operation�� 'F'�� �ʱ�ȭ
 * �ؼ� logging�ϰ�, nologging operation�� �Ϸ��� �� physical header�� 'T'��
 * �����Ѵ�. �̶� log�� ������� �ʴ´�.
 *
 **********************************************************************/
IDE_RC  sdpPhyPage::setPageConsistency(
    sdrMtx         *aMtx,
    sdpPhyPageHdr  *aPageHdr,
    UChar          *aIsConsistent )
{
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aPageHdr->mIsConsistent,
                                         aIsConsistent,
                                         ID_SIZEOF(*aIsConsistent) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2162 Mtx���� �˾Ƽ� �����ϱ� */
IDE_RC sdpPhyPage::setPageInconsistency( scSpaceID       aSpaceID,
                                         scPageID        aPageID )
{
    sdrMtx          sMtx;
    idBool          sIsMtxBegin = ID_FALSE;
    idBool          sIsSuccess  = ID_FALSE;
    sdpPhyPageHdr * sPagePtr;
    UChar           sIsConsistent = SDP_PAGE_INCONSISTENT;

    IDE_TEST( sdrMiniTrans::begin( NULL, // idvSQL
                                   &sMtx,
                                   NULL, // Trans
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DUMMY )
              != IDE_SUCCESS );
    sIsMtxBegin = ID_TRUE;

    IDE_TEST( sdbBufferMgr::getPageByPID( NULL, // idvSQL
                                          aSpaceID,
                                          aPageID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          (UChar**)&sPagePtr,
                                          &sIsSuccess,
                                          NULL ) // IsCorruptPage
              != IDE_SUCCESS );
    IDE_ERROR( sIsSuccess == ID_TRUE );

    /* �߸��� �������� ���� ������ ����� */
    sdpPhyPage::tracePage( IDE_DUMP_0,
                           (UChar*)sPagePtr,
                           "DRDB PAGE:");
    IDE_TEST( sdpPhyPage::setPageConsistency( &sMtx,
                                              sPagePtr,
                                              &sIsConsistent )
              != IDE_SUCCESS );

    sIsMtxBegin = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsMtxBegin == ID_TRUE )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }
    return IDE_FAILURE;
}

// BUG-45598: ��� ������ corrupt �߻��� ��� üũ
IDE_RC sdpPhyPage::setPageInconsistency( sdpPhyPageHdr * aPagePtr )
{
    sdrMtx          sMtx;
    idBool          sIsMtxBegin = ID_FALSE;
    UChar           sIsConsistent = SDP_PAGE_INCONSISTENT;
    
    IDE_TEST( sdrMiniTrans::begin( NULL, // idvSQL
                                   &sMtx,
                                   NULL, // Trans
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT | SM_DLOG_ATTR_DUMMY )
              != IDE_SUCCESS );
    sIsMtxBegin = ID_TRUE;

    /* �߸��� �������� ���� ������ ����� */
    sdpPhyPage::tracePage( IDE_DUMP_0,
                           (UChar*)aPagePtr,
                           "DRDB PAGE:");
    IDE_TEST( sdpPhyPage::setPageConsistency( &sMtx,
                                              aPagePtr,
                                              &sIsConsistent )
              != IDE_SUCCESS );

    sIsMtxBegin = ID_FALSE;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsMtxBegin == ID_TRUE )
    {
        (void)sdrMiniTrans::rollback( &sMtx );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *
 **********************************************************************/
idBool sdpPhyPage::validate( sdpPhyPageHdr * aPageHdr )
{
    sdpCTL   * sCTL;

    IDE_DASSERT( (UChar*)aPageHdr == getPageStartPtr((UChar*)aPageHdr));

    if ( aPageHdr->mPageType == SDP_PAGE_UNFORMAT )
    {
        return ID_TRUE;
    }

    IDE_DASSERT( aPageHdr->mTotalFreeSize <=
                 ( getEmptyPageFreeSize() - aPageHdr->mLogicalHdrSize ) );

    IDE_DASSERT( aPageHdr->mTotalFreeSize >=
                 aPageHdr->mAvailableFreeSize );

    IDE_DASSERT( aPageHdr->mFreeSpaceBeginOffset >=
                 ( getPhyHdrSize() + aPageHdr->mLogicalHdrSize ) );

    IDE_DASSERT( aPageHdr->mFreeSpaceEndOffset   <=
                 (SD_PAGE_SIZE - ID_SIZEOF(sdpPageFooter)) );

    IDE_DASSERT( aPageHdr->mFreeSpaceBeginOffset <=
                 aPageHdr->mFreeSpaceEndOffset );

    if ( (aPageHdr->mSizeOfCTL != 0) &&
         (aPageHdr->mPageType  == SDP_PAGE_DATA) )
    {
        sCTL = (sdpCTL*)getCTLStartPtr( (UChar*)aPageHdr );

        if ( SM_SCN_IS_MAX( sCTL->mSCN4Aging ) )
        {
            IDE_DASSERT( sCTL->mCandAgingCnt == 0 );
        }
        else
        {
            /* nothing to do */
        }
    }

    return ID_TRUE;

}

/* --------------------------------------------------------------------
 * TASK-4007 [SM]PBT�� ���� ��� �߰�
 *
 * Description : dump �Լ�. sdpPhyPageHdr�� ��Ŀ� �°� �ѷ��ش�.
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�.
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�.
 * ----------------------------------------------------------------- */

IDE_RC sdpPhyPage::dumpHdr( const UChar    * aPage ,
                            SChar          * aOutBuf ,
                            UInt             aOutSize )
{
    const sdpPhyPageHdr * sPageHdr;
    smLSN sTailLSN;

    IDE_ERROR( aPage   != NULL );
    IDE_ERROR( aOutBuf != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPageHdr = (sdpPhyPageHdr*)getPageStartPtr( (UChar*)aPage );

    IDE_DASSERT( (UChar*)sPageHdr == aPage );

    sTailLSN = ((sdpPageFooter*)getPageFooterStartPtr((UChar*)sPageHdr))->mPageLSN;

    // TODO: Fixed Table���� SmoNo�� ���� ���� UInt���� ULong���� ����Ǿ
    //       �Ǵ��� Ȯ���ϱ�
    idlOS::snprintf( aOutBuf ,
                     aOutSize ,
                     "----------- Physical Page Begin ----------\n"
                     "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                     "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                     "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                     "Total Free Size           : %"ID_UINT32_FMT"\n"
                     "Available Free Size       : %"ID_UINT32_FMT"\n"
                     "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                     "Size of CTL               : %"ID_UINT32_FMT"\n"
                     "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                     "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                     "Page Type                 : %"ID_UINT32_FMT"\n"
                     "Page State                : %"ID_UINT32_FMT"\n"
                     "Page ID                   : %"ID_UINT32_FMT"\n"
                     "is Consistent             : %"ID_UINT32_FMT"\n"
                     "LinkState                 : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                     "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "TableOID                  : %"ID_vULONG_FMT"\n"
                     "IndexID                   : %"ID_UINT32_FMT"\n"
                     "Seq No                    : %"ID_UINT64_FMT"\n"
                     "TailPageLSN.mFileNo       : %"ID_UINT32_FMT"\n"
                     "TailPageLSN.mOffset       : %"ID_UINT32_FMT"\n"
                     "----------- Physical Page End ----------\n",
                     (UChar*)sPageHdr,
                     sPageHdr->mFrameHdr.mCheckSum,
                     sPageHdr->mFrameHdr.mPageLSN.mFileNo,
                     sPageHdr->mFrameHdr.mPageLSN.mOffset,
                     sPageHdr->mFrameHdr.mIndexSMONo,
                     sPageHdr->mFrameHdr.mSpaceID,
                     sPageHdr->mFrameHdr.mBCBPtr,
                     sPageHdr->mTotalFreeSize,
                     sPageHdr->mAvailableFreeSize,
                     sPageHdr->mLogicalHdrSize,
                     sPageHdr->mSizeOfCTL,
                     sPageHdr->mFreeSpaceBeginOffset,
                     sPageHdr->mFreeSpaceEndOffset,
                     sPageHdr->mPageType,
                     sPageHdr->mPageState,
                     sPageHdr->mPageID,
                     sPageHdr->mIsConsistent,
                     sPageHdr->mLinkState,
                     sPageHdr->mParentInfo.mParentPID,
                     sPageHdr->mParentInfo.mIdxInParent,
                     sPageHdr->mListNode.mPrev,
                     sPageHdr->mListNode.mNext,
                     sPageHdr->mTableOID,
                     sPageHdr->mIndexID,
                     sPageHdr->mSeqNo,
                     sTailLSN.mFileNo,
                     sTailLSN.mOffset );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* --------------------------------------------------------------------
 * PROJ-2118 BUG Reporting
 *
 * Description : Page�� Hexa data �� ������ trace file�� �ѷ��ش�.
 *               Page ID �� �ش� Page�� ���� �ٸ� ������
 *               �߰��� �Է��Ͽ� ����ϰ� �� �� �ִ�.
 *
 * ----------------------------------------------------------------- */
IDE_RC sdpPhyPage::tracePage( UInt           aChkFlag,
                              ideLogModule   aModule,
                              UInt           aLevel,
                              const UChar  * aPage,
                              const SChar  * aTitle,
                              ... )
{
    va_list ap;
    IDE_RC result;

    va_start( ap, aTitle );

    result = tracePageInternal( aChkFlag,
                                aModule,
                                aLevel,
                                aPage,
                                aTitle,
                                ap );

    va_end( ap );

    return result;
}

IDE_RC sdpPhyPage::tracePageInternal( UInt           aChkFlag,
                                      ideLogModule   aModule,
                                      UInt           aLevel,
                                      const UChar  * aPage,
                                      const SChar  * aTitle,
                                      va_list        ap )
{
    SChar         * sDumpBuf = NULL;
    const UChar   * sPage;
    UInt            sOffset;
    UInt            sState = 0;

    ideLogEntry sLog( aChkFlag,
                      aModule,
                      aLevel );

    IDE_ERROR( aPage != NULL );

    sPage = getPageStartPtr( (UChar*)aPage );

    sOffset = aPage - sPage;

    IDE_TEST( iduMemMgr::calloc( IDU_MEM_SM_SDP, 
                                 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sDumpBuf )
              != IDE_SUCCESS );
    sState = 1;

    if ( aTitle != NULL )
    {
        idlOS::vsnprintf( sDumpBuf,
                          IDE_DUMP_DEST_LIMIT,
                          aTitle,
                          ap );

        sLog.appendFormat( "%s\n"
                           "Offset : %u[%04x]\n",
                           sDumpBuf,
                           sOffset,
                           sOffset );
    }
    else
    {
        sLog.appendFormat( "[ Dump Page ]\n"
                           "Offset : %u[%04x]\n",
                           sOffset,
                           sOffset );
    }

    (void)sLog.dumpHex( sPage, SD_PAGE_SIZE, IDE_DUMP_FORMAT_FULL );

    sLog.append( "\n" );

    if ( sdpPhyPage::dumpHdr( sPage ,
                              sDumpBuf ,
                              IDE_DUMP_DEST_LIMIT )
        == IDE_SUCCESS )
    {
        sLog.appendFormat( "%s\n",
                           sDumpBuf );
    }
    else
    {
        /* nothing to do */
    }

    sLog.write();

    IDE_TEST( iduMemMgr::free( sDumpBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free( sDumpBuf ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/* --------------------------------------------------------------------
 * Description : Disk page�� memcpy�Լ�
 *               page�� ��踦 �Ѿ�ų�, sdpPhyPageHeader�� ��ġ��
 *               memcpy�ϴ��� �˻��Ѵ�.
 *
 * BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
 * ����� ���� �߰�.
 * ----------------------------------------------------------------- */
IDE_RC sdpPhyPage::writeToPage( UChar * aDestPagePtr,
                                UChar * aSrcPtr,
                                UInt    aLength )
{
    UInt   sOffset;

    IDE_ASSERT( aDestPagePtr != NULL );
    IDE_ASSERT( aSrcPtr      != NULL );

    sOffset = aDestPagePtr - getPageStartPtr( aDestPagePtr );

    IDE_TEST( ( sOffset + aLength ) > SD_PAGE_SIZE );
    IDE_TEST( sOffset < ID_SIZEOF( sdpPhyPageHdr ) );

    idlOS::memcpy( aDestPagePtr, aSrcPtr, aLength );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        ideLog::log( IDE_DUMP_0,
                     "Invalid memcpy to disk page\n"
                     "PagePtr : %"ID_xPOINTER_FMT"\n"
                     "Offset  : %u\n"
                     "Length  : %u\n"
                     "SrcPtr  : %"ID_xPOINTER_FMT"\n",
                     aDestPagePtr - sOffset,
                     sOffset,
                     aLength,
                     aSrcPtr );

        tracePage( IDE_DUMP_0,
                   aDestPagePtr,
                   NULL );
    }

    return IDE_FAILURE;
}

/* --------------------------------------------------------------------
 * Description : BUG-47429 SMO NO Reset�� �����丵 �մϴ�.
 *
 * aPageHdr      - [IN] Page�� Header
 * aSpaceID      - [IN] OnlineOffline���θ� �Ǵ� �ؾ� �ϴ� Space�� SpaceID
 * aChkOnlineTBS - [IN] Online Offline ���� �Ǵ� �ؾ��ϴ��� ����
 *                      BUG-47439�� �����ϸ鼭 ������ sOnlineTBSLSN4Idx��
 *                      INIT���� �ѱ�� ���� ID_FALSE�� ��ü�Ͽ���.
 * ----------------------------------------------------------------- */
void sdpPhyPage::resetIndexSMONo( sdpPhyPageHdr * aPageHdr,
                                  scSpaceID       aSpaceID,
                                  idBool          aChkOnlineTBS )
{
    smLSN    sStartLSN;
    smLSN    sPageLSN;
    smLSN    sOnlineTBSLSN4Idx;
    void   * sSpaceNode;

    // SMO no�� �ʱ�ȭ�ؾ� �Ѵ�.
    sStartLSN  = smrRecoveryMgr::getIdxSMOLSN();
    sPageLSN   = aPageHdr->mFrameHdr.mPageLSN ;
    // restart ���Ŀ� Runtime ������ SMO No�� �ʱ�ȭ�Ѵ�.
    if ( smLayerCallback::isLSNGT( &sPageLSN, &sStartLSN ) == ID_FALSE )
    {
        // recorvery LSN�� ���Ͽ� ���� �͸� set�Ѵ�.
        setIndexSMONo( aPageHdr, 0 );
    }
    else
    {
        /* fix BUG-17456 Disk Tablespace online���� update �߻��� index ���ѷ���
         * sPageLSN <= sStartLSN �̰ų� sPageLSN <= aOnlineTBSLSN4Idx
         *
         * BUG-17456�� �ذ��ϱ� ���� �߰��� �����̴�.
         * ��߿� offline �Ǿ��� �ٽ� Online �� TBS�� Index�� SMO No��
         * �����Ͽ���, aOnlineTBSLSN4Idx������ PageLSN�� ���� Page��
         * read�� ��쿡�� SMO No�� 0���� �ʱ�ȭ�Ͽ� index traverse�Ҷ�
         * ���� loop �� ������ �ʰ� �Ѵ�. */
        if ( aChkOnlineTBS == ID_TRUE )
        {
            sSpaceNode = sctTableSpaceMgr::findSpaceNodeIncludingDropped( aSpaceID );
            sOnlineTBSLSN4Idx = sddTableSpace::getOnlineTBSLSN4Idx( sSpaceNode );

            if ( (!( SM_IS_LSN_INIT( sOnlineTBSLSN4Idx ) ) ) &&
                 ( smLayerCallback::isLSNGT( &sPageLSN, &sOnlineTBSLSN4Idx )
                   == ID_FALSE ) )
            {
                setIndexSMONo( aPageHdr, 0 );
            }
        }
    }
}
