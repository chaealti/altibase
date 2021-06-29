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

#ifndef _O_SDP_SLOT_DIRECTORY_H_
#define _O_SDP_SLOT_DIRECTORY_H_ 1

#include <sdpDef.h>
#include <smiDef.h>
#include <smErrorCode.h>

#define SDP_SLOT_ENTRY_FLAG_UNUSED  (0x8000)
#define SDP_SLOT_ENTRY_OFFSET_MASK  (0x7FFF)

#define SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum)                            \
    ( (sdpSlotEntry*)                                                        \
      ( (aSlotDirPtr) +                                                      \
        ID_SIZEOF(sdpSlotDirHdr) +                                           \
        ( ID_SIZEOF(sdpSlotEntry) * (aSlotNum) ) ) )

#define SDP_GET_OFFSET(aSlotEntry)                                           \
    ( (scOffset)(*(aSlotEntry) & SDP_SLOT_ENTRY_OFFSET_MASK ) )

#define SDP_SET_OFFSET(aSlotEntry, aOffset)                                  \
    *(aSlotEntry) &= ~SDP_SLOT_ENTRY_OFFSET_MASK;                             \
    *(aSlotEntry) |= ( (aOffset) & SDP_SLOT_ENTRY_OFFSET_MASK )

#define SDP_IS_UNUSED_SLOT_ENTRY(aSlotEntry)                                 \
    ( ( ( *(aSlotEntry) & SDP_SLOT_ENTRY_FLAG_UNUSED )                       \
        == SDP_SLOT_ENTRY_FLAG_UNUSED ) ? ID_TRUE : ID_FALSE )

#define SDP_SET_UNUSED_FLAG(aSlotEntry)                                      \
    ( *(aSlotEntry) |= SDP_SLOT_ENTRY_FLAG_UNUSED )

#define SDP_CLR_UNUSED_FLAG(aSlotEntry)                                      \
    ( *(aSlotEntry) &= ~SDP_SLOT_ENTRY_FLAG_UNUSED )


/* slot directory�� �����Ѵ�. */
class sdpSlotDirectory
{
public:

    static IDE_RC logAndInit( sdpPhyPageHdr  *aPageHdr,
                              sdrMtx         *aMtx );

    static void init(sdpPhyPageHdr    *aPhyPageHdr);

    static IDE_RC alloc(sdpPhyPageHdr    *aPhyPageHdr,
                        scOffset          aSlotOffset,
                        scSlotNum        *aAllocSlotNum);

    static void allocWithShift(sdpPhyPageHdr    *aPhyPageHdr,
                               scSlotNum         aSlotNum,
                               scOffset          aSlotOffset);

    static IDE_RC free(sdpPhyPageHdr    *aPhyPageHdr,
                       scSlotNum         aSlotNum);

    static IDE_RC freeWithShift(sdpPhyPageHdr    *aPhyPageHdr,
                                scSlotNum         aSlotNum);

    static IDE_RC find(UChar            *aSlotDirPtr,
                       scOffset          aSlotOffset,
                       SShort          *aSlotNum);

    static idBool isExistUnusedSlotEntry(UChar       *aSlotDirPtr);

    static UShort getSize(sdpPhyPageHdr    *aPhyPageHdr);

    static inline UShort getCount( UChar * aSlotDirPtr );


    /* BUG-31534 [sm-util] dump utility and fixed table do not consider 
     *           unused slot. */
    static IDE_RC getNextUnusedSlot( UChar       *aSlotDirPtr,
                                     scSlotNum    aSlotNum,
                                     scOffset    *aSlotOffset );

    static inline IDE_RC getValue(UChar       *aSlotDirPtr,
                                  scSlotNum    aSlotNum,
                                  scOffset    *aSlotOffset);

    static IDE_RC setValue(UChar       *aSlotDirPtr,
                           scSlotNum    aSlotNum,
                           scOffset     aSlotOffset);

    static inline idBool isUnusedSlotEntry( UChar      * aSlotDirPtr,
                                            scSlotNum    aSlotNum );

    static inline IDE_RC getPagePtrFromSlotNum(UChar       *aSlotDirPtr,
                                               scSlotNum    aSlotNum,
                                               UChar      **aSlotPtr);

    static inline UShort getDistance( UChar      *aSlotDirPtr,
                                      scSlotNum   aEntry1,
                                      scSlotNum   aEntry2 );

    static idBool validate(UChar    *aSlotDirPtr);

    /* TASK-4007 [SM] PBT�� ���� ��� �߰�
     * SlotEntry�� Dump�� �� �ִ� ��� �߰�*/
    static IDE_RC dump( UChar *sPage ,
                        SChar *aOutBuf,
                        UInt   aOutSize );

    /* BUG-33543 [sm-disk-page] add debugging information to
     * sdpSlotDirectory::getValue function 
     * Unused Slot�̸�, ���� ����ϰ� ������ ��ȯ�Ѵ�. */
    static inline IDE_RC validateSlot(UChar        * aPagePtr,
                                      scSlotNum      aSlotNum,
                                      sdpSlotEntry * aSlotEntry,
                                      idBool         aIsUnusedSlot);

    /* TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::getPageStartPtr �Լ���
     * sdpSlotDirectory::getPageStartPtr�� �ߺ����� �߰��Ѵ�. 
     * �� �Լ��� �����Ǹ� ������ �Լ��� �����ؾ� �Ѵ�.*/
    inline static UChar * getPageStartPtr( void * aPagePtr )
    {
        return (UChar *)idlOS::alignDown( (void*)aPagePtr, (UInt)SD_PAGE_SIZE );
    }

    /* TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::getPagePtrFromOffset �Լ���
     * sdpSlotDirectory::getPagePtrFromOffset�� �ߺ����� �߰��Ѵ�. 
     * �� �Լ��� �����Ǹ� ������ �Լ��� �����ؾ� �Ѵ�.*/
    inline static UChar * getPagePtrFromOffset( UChar   * aPagePtr,
                                                scOffset  aOffset)
    {
        return ( sdpSlotDirectory::getPageStartPtr(aPagePtr) + aOffset );
    }

private:

    static void shiftToBottom(sdpPhyPageHdr    *aPhyPageHdr,
                              UChar            *aSlotDirPtr,
                              UShort            aSlotNum);

    static void shiftToTop(sdpPhyPageHdr    *aPhyPageHdr,
                           UChar            *aSlotDirPtr,
                           UShort            aSlotNum);

    static IDE_RC addToUnusedSlotEntryList(UChar            *aSlotDirPtr,
                                           scSlotNum         aSlotNum);

    static IDE_RC removeFromUnusedSlotEntryList(UChar            *aSlotDirPtr,
                                                scSlotNum        *aSlotNum);

    static sdpPhyPageHdr * getPageHdr( UChar * aPagePtr );

    static void tracePage( UChar * aSlotDirPtr );
};

inline UShort sdpSlotDirectory::getDistance( UChar      *aSlotDirPtr,
                                             scSlotNum   aEntry1,
                                             scSlotNum   aEntry2 )
{
    sdpSlotEntry * sEntry1;
    sdpSlotEntry * sEntry2;
    UShort         sDiff;

    sEntry1 = SDP_GET_SLOT_ENTRY( aSlotDirPtr, aEntry1 );
    sEntry2 = SDP_GET_SLOT_ENTRY( aSlotDirPtr, aEntry2 );

    if( SDP_GET_OFFSET( sEntry1 ) > SDP_GET_OFFSET( sEntry2) )
    {
        sDiff = SDP_GET_OFFSET( sEntry1 ) - SDP_GET_OFFSET( sEntry2 );
    }
    else
    {
        sDiff = SDP_GET_OFFSET( sEntry2 ) - SDP_GET_OFFSET( sEntry1 );
    }

    return sDiff;
}

/***********************************************************************
 * Description :
 * TASK-6105 �Լ� inlineȭ
 *     BUG-33543 [sm-disk-page] add debugging information to
 *     sdpSlotDirectory::getValue function 
 *     Slot�� Used���� Unused���� üũ�Ͽ�, ���� �ٸ���
 *     ���� ����ϰ� �����������Ŵ.
 *
 *  aPtr          - [IN] ��� Page���� ����Ű�� Pointer
 *  aSlotNum      - [IN] ��� SlotNumber
 *  aSlotEntry    - [IN] Used������ Slot
 *  aIsUnusedSlot - [IN] �̰��� UnusedSlot�ΰ�? (�׷��� �ȵȴ�.)
 **********************************************************************/
inline IDE_RC sdpSlotDirectory::validateSlot(UChar        * aPagePtr,
                                             scSlotNum      aSlotNum,
                                             sdpSlotEntry * aSlotEntry,
                                             idBool         aIsUnusedSlot)
{
    sdpPhyPageHdr    * sPhyPageHdr;
                             
    IDU_FIT_POINT_RAISE ( "BUG-40472::validateSlot::InvalidSlot", ERR_ABORT_Invalid_slot );
    if ( SDP_IS_UNUSED_SLOT_ENTRY(aSlotEntry) == aIsUnusedSlot )
    {
        IDE_RAISE( ERR_ABORT_Invalid_slot );
    }
    else
    {
        /* nothing to do */
    }
    return IDE_SUCCESS;
     
    IDE_EXCEPTION( ERR_ABORT_Invalid_slot );
    {
        sPhyPageHdr = getPageHdr( aPagePtr );

        ideLog::log( IDE_ERR_0, 
                     "[ Invalid slot access ]\n"
                     "  SpaceID   : %"ID_UINT32_FMT"\n" 
                     "  PageID    : %"ID_UINT32_FMT"\n" 
                     "  Page Type : %"ID_UINT32_FMT"\n"
                     "  Page State: %"ID_UINT32_FMT"\n"
                     "  TableOID  : %"ID_vULONG_FMT"\n"
                     "  IndexID   : %"ID_vULONG_FMT"\n"
                     "  SlotNum   : %"ID_UINT32_FMT"\n",
                     sPhyPageHdr->mFrameHdr.mSpaceID,
                     sPhyPageHdr->mPageID,
                     sPhyPageHdr->mPageType,
                     sPhyPageHdr->mPageState,
                     sPhyPageHdr->mTableOID,
                     sPhyPageHdr->mIndexID,
                     aSlotNum );

        ideLog::logMem( IDE_ERR_0, 
                        (UChar*)sPhyPageHdr,
                        SD_PAGE_SIZE );
        ideLog::logCallStack( IDE_ERR_0 );

        IDE_SET( ideSetErrorCode( smERR_ABORT_Invalid_slot ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 * TASK-6105 �Լ� inlineȭ
 *  slot entry�� ����� slot offset���� ��ȯ�Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *  aSlotOffset - [OUT] slot entry�� ����� slot offset��
 **********************************************************************/
inline IDE_RC sdpSlotDirectory::getValue(UChar       *aSlotDirPtr,
                                         scSlotNum    aSlotNum,
                                         scOffset    *aSlotOffset )
{
    sdpSlotDirHdr   *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry    *sSlotEntry;

    IDE_DASSERT( aSlotDirPtr != NULL );
    IDE_ASSERT( aSlotNum < sSlotDirHdr->mSlotEntryCount );

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum);
    /* ���� �о� ������ slot entry�� �ݵ�� used ���¿��� �Ѵ�. 
       �ƴϸ� �������� ����. */ 
    IDE_TEST( validateSlot( aSlotDirPtr, 
                            aSlotNum,
                            sSlotEntry, 
                            ID_TRUE )  /* isUnused, used �̾�� �� */
              != IDE_SUCCESS );

    *aSlotOffset = SDP_GET_OFFSET(sSlotEntry);        
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;


}

/***********************************************************************
 *
 * Description :
 * TASK-6105 �Լ� inlineȭ
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *  
 **********************************************************************/
inline IDE_RC sdpSlotDirectory::getPagePtrFromSlotNum( UChar       *aSlotDirPtr,
                                                       scSlotNum    aSlotNum,
                                                       UChar      **aSlotPtr )
{
    sdpSlotDirHdr   * sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    UChar           * sPageHdrPtr = sdpSlotDirectory::getPageStartPtr( aSlotDirPtr );
    scOffset          sOffset     =  SC_MAX_OFFSET;

    /* BUG-34499 Undo Page ��Ȱ������ ���Ͽ� Undo Page����
     * �߸��� Offset���� �о���� ������ ���� �����ڵ� */
    if ( aSlotNum >= sSlotDirHdr->mSlotEntryCount )
    {
        ideLog::log( IDE_SERVER_0,
                     "[ Invalid slot number ]\n"
                     "  Slot Num (%"ID_UINT32_FMT") >="
                     "  Entry Count (%"ID_UINT32_FMT")\n",
                     aSlotNum,
                     sSlotDirHdr->mSlotEntryCount );

        sdpSlotDirectory::tracePage( aSlotDirPtr );

        IDE_ERROR( 0 ); 
    }

   IDE_TEST( sdpSlotDirectory::getValue(aSlotDirPtr, aSlotNum, &sOffset)
             != IDE_SUCCESS );

    /* BUG-34499 Undo Page ��Ȱ������ ���Ͽ� Undo Page����
     * �߸��� Offset���� �о���� ������ ���� �����ڵ� */
    if (( sOffset <= ( aSlotDirPtr - sPageHdrPtr ) ) ||
       ( sOffset >= SD_PAGE_SIZE ))
    {
        ideLog::log( IDE_SERVER_0,
                     "[ Invalid slot offset ]\n"
                     "  Slot Num    : %"ID_UINT32_FMT"\n"
                     "  Entry Count : %"ID_UINT32_FMT"\n"
                     "  Offset      : %"ID_UINT32_FMT"\n",
                     aSlotNum,
                     sSlotDirHdr->mSlotEntryCount,
                     sOffset );

        sdpSlotDirectory::tracePage( aSlotDirPtr );

        IDE_ERROR( 0 ); 

    }

    *aSlotPtr = sdpSlotDirectory::getPagePtrFromOffset( aSlotDirPtr,
                                                        sOffset );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  slot entry ������ ��ȯ�Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *
 **********************************************************************/
inline UShort sdpSlotDirectory::getCount( UChar * aSlotDirPtr )
{
    sdpSlotDirHdr * sSlotDirHdr = (sdpSlotDirHdr *)aSlotDirPtr;

    IDE_DASSERT( aSlotDirPtr != NULL );

#ifdef DEBUG

    UShort    sSlotDirSize;
    UShort    sSlotEntryCount;

    sSlotDirSize = getSize( (sdpPhyPageHdr *)getPageStartPtr( aSlotDirPtr ) );

    sSlotEntryCount = (sSlotDirSize - ID_SIZEOF(sdpSlotDirHdr)) / ID_SIZEOF(sdpSlotEntry);

    IDE_DASSERT( sSlotDirHdr->mSlotEntryCount == sSlotEntryCount );
#endif

    return sSlotDirHdr->mSlotEntryCount;
}

/***********************************************************************
 *
 * Description :
 *  unused slot entry���� ���θ� ��ȯ�Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *
 **********************************************************************/
inline idBool sdpSlotDirectory::isUnusedSlotEntry( UChar      * aSlotDirPtr,
                                                   scSlotNum    aSlotNum )
{
    sdpSlotEntry * sSlotEntry;

    sSlotEntry = SDP_GET_SLOT_ENTRY( aSlotDirPtr, aSlotNum );

    if ( SDP_IS_UNUSED_SLOT_ENTRY( sSlotEntry ) == ID_TRUE )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

#endif /* _O_SDP_SLOT_DIRECTORY_H_ */
