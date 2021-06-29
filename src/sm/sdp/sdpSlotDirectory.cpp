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
 

#include <sdpSlotDirectory.h>
#include <sdpPhyPage.h>


/***********************************************************************
 *
 * Description :
 *  slot directory ������ �ʱ�ȭ�Ѵ�.(with logging)
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aMtx        - [IN] mini Ʈ�����
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::logAndInit( sdpPhyPageHdr  *aPageHdr,
                                     sdrMtx         *aMtx )
{
    IDE_ASSERT( aPageHdr != NULL );
    IDE_ASSERT( aMtx     != NULL );

    init(aPageHdr);

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aPageHdr,
                                         NULL, /* aValue */
                                         0,    /* aLength */
                                         SDR_SDP_INIT_SLOT_DIRECTORY )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  slot directory ������ �ʱ�ȭ�Ѵ�.(no logging)
 *
 *  aPhyPageHdr - [IN] physical page header
 *
 **********************************************************************/
void sdpSlotDirectory::init(sdpPhyPageHdr    *aPhyPageHdr)
{
    sdpSlotDirHdr    *sSlotDirHdr;

    IDE_DASSERT( aPhyPageHdr != NULL );
    IDE_DASSERT( aPhyPageHdr->mTotalFreeSize     >= ID_SIZEOF(sdpSlotDirHdr) );
    IDE_DASSERT( aPhyPageHdr->mAvailableFreeSize >= ID_SIZEOF(sdpSlotDirHdr) );

    aPhyPageHdr->mFreeSpaceBeginOffset += ID_SIZEOF(sdpSlotDirHdr);
    aPhyPageHdr->mTotalFreeSize        -= ID_SIZEOF(sdpSlotDirHdr);
    aPhyPageHdr->mAvailableFreeSize    -= ID_SIZEOF(sdpSlotDirHdr);

    sSlotDirHdr = (sdpSlotDirHdr*)
        sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);

    sSlotDirHdr->mSlotEntryCount = 0;
    sSlotDirHdr->mHead           = SDP_INVALID_SLOT_ENTRY_NUM;
}


/***********************************************************************
 *
 * Description :
 *  Slot Directory���� SlotEntry �ϳ��� �Ҵ��ϴ� �Լ��̴�.
 *
 *  aPhyPageHdr   - [IN]  physical page header
 *  aSlotOffset   - [IN]  �������������� slot offset
 *  aAllocSlotNum - [OUT] �Ҵ��� slot entry number
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::alloc(sdpPhyPageHdr    *aPhyPageHdr,
                               scOffset          aSlotOffset,
                               scSlotNum        *aAllocSlotNum)
{
    UChar           *sSlotDirPtr;
    sdpSlotDirHdr   *sSlotDirHdr;
    sdpSlotEntry    *sSlotEntry;
    scSlotNum        sAllocSlotNum;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;

    IDE_DASSERT( aPhyPageHdr != NULL );
    IDE_DASSERT( validate(sSlotDirPtr) == ID_TRUE );

    if( sSlotDirHdr->mHead == SDP_INVALID_SLOT_ENTRY_NUM )
    {
        /* unused slot entry�� ���,
         * new slot entry�� �Ҵ��ϴ� ��� */
        sAllocSlotNum = sSlotDirHdr->mSlotEntryCount;

        sSlotDirHdr->mSlotEntryCount++;
        aPhyPageHdr->mFreeSpaceBeginOffset += ID_SIZEOF(sdpSlotEntry);
        aPhyPageHdr->mTotalFreeSize        -= ID_SIZEOF(sdpSlotEntry);
        aPhyPageHdr->mAvailableFreeSize    -= ID_SIZEOF(sdpSlotEntry);

        sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sAllocSlotNum);
    }
    else
    {
        /* unused slot entry�� �����ϴ� ��� */
        IDE_ERROR( removeFromUnusedSlotEntryList( sSlotDirPtr,
                                                  &sAllocSlotNum )
                   == IDE_SUCCESS );

        sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sAllocSlotNum);
        IDE_TEST( validateSlot( (UChar*)aPhyPageHdr, 
                                 sAllocSlotNum, 
                                 sSlotEntry,
                                 ID_FALSE ) /* isUnused, Unused������ */
                  != IDE_SUCCESS);
    }

    /* unused flag�� �����Ͽ� used ���·� �����Ѵ�. */
    SDP_CLR_UNUSED_FLAG(sSlotEntry);

    /* offset���� slot entry�� �����Ѵ�. */
    SDP_SET_OFFSET(sSlotEntry, aSlotOffset);

    *aAllocSlotNum = sAllocSlotNum;
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}


/***********************************************************************
 *
 * Description :
 *  Slot Directory���� SlotEntry �ϳ��� �Ҵ��ϴ� �Լ��̴�.
 *  �ʿ��� ���, shift ������ �����Ѵ�.
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aSlotNum    - [IN] �Ҵ��Ϸ��� �ϴ� slot number
 *  aSlotOffset - [IN] �������������� slot offset
 *
 **********************************************************************/
void sdpSlotDirectory::allocWithShift(sdpPhyPageHdr    *aPhyPageHdr,
                                      scSlotNum         aSlotNum,
                                      scOffset          aSlotOffset)
{
    UChar           *sSlotDirPtr;
    sdpSlotDirHdr   *sSlotDirHdr;
    sdpSlotEntry    *sSlotEntry;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;

    IDE_DASSERT( aPhyPageHdr != NULL );
    IDE_DASSERT( validate(sSlotDirPtr) == ID_TRUE );
    IDE_DASSERT( aSlotNum <= sSlotDirHdr->mSlotEntryCount );

    if( aSlotNum != sSlotDirHdr->mSlotEntryCount )
    {
        shiftToBottom(aPhyPageHdr, sSlotDirPtr, aSlotNum);
    }

    sSlotDirHdr->mSlotEntryCount++;
    aPhyPageHdr->mFreeSpaceBeginOffset += ID_SIZEOF(sdpSlotEntry);
    aPhyPageHdr->mTotalFreeSize        -= ID_SIZEOF(sdpSlotEntry);
    aPhyPageHdr->mAvailableFreeSize    -= ID_SIZEOF(sdpSlotEntry);

    sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, aSlotNum);

    /* unused flag�� �����Ͽ� used ���·� �����Ѵ�. */
    SDP_CLR_UNUSED_FLAG(sSlotEntry);

    /* offset���� slot entry�� �����Ѵ�. */
    SDP_SET_OFFSET(sSlotEntry, aSlotOffset);
}


/***********************************************************************
 *
 * Description :
 *  Slot Directory���� SlotEntry �ϳ��� �����ϴ� �Լ��̴�.
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aSlotNum    - [IN] �����Ϸ��� �ϴ� slot number
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::free(sdpPhyPageHdr    *aPhyPageHdr,
                              scSlotNum         aSlotNum)
{
    UChar           *sSlotDirPtr;
#ifdef DEBUG
    sdpSlotDirHdr   *sSlotDirHdr;
#endif
    sdpSlotEntry    *sSlotEntry;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);

#ifdef DEBUG
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;
#endif
    IDE_DASSERT( aPhyPageHdr != NULL );
    IDE_DASSERT( validate(sSlotDirPtr) == ID_TRUE );
    IDE_DASSERT( sSlotDirHdr->mSlotEntryCount > 0 );
    IDE_DASSERT( aSlotNum < sSlotDirHdr->mSlotEntryCount );

    sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, aSlotNum);
    /* �����Ϸ��� slot entry�� �ݵ�� used ���¿��� �Ѵ�.
     * unused�� slot entry�� �����Ϸ��� �ϴ� ����
     * �������� �ɰ��� �����̴�. */
    IDE_TEST( validateSlot( (UChar*)aPhyPageHdr,
                             aSlotNum,
                             sSlotEntry,
                             ID_TRUE ) /* isUnused, Unused�� �ȵ� */
               != IDE_SUCCESS);

    SDP_SET_OFFSET(sSlotEntry, SDP_INVALID_SLOT_ENTRY_NUM);
    SDP_SET_UNUSED_FLAG(sSlotEntry);

    IDE_TEST( addToUnusedSlotEntryList(sSlotDirPtr,
                                       aSlotNum)
              != IDE_SUCCESS);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  Slot Directory���� SlotEntry �ϳ��� �����ϴ� �Լ��̴�.
 *  �ʿ��� ���, shift ������ �����Ѵ�.
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aSlotNum    - [IN] �����Ϸ��� �ϴ� slot number
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::freeWithShift(sdpPhyPageHdr    *aPhyPageHdr,
                                       scSlotNum         aSlotNum)
{
    UChar           *sSlotDirPtr;
    sdpSlotDirHdr   *sSlotDirHdr;
    sdpSlotEntry    *sSlotEntry;

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;

    IDE_DASSERT( aPhyPageHdr != NULL );
    IDE_DASSERT( validate(sSlotDirPtr) == ID_TRUE );
    IDE_DASSERT( sSlotDirHdr->mSlotEntryCount > 0 );
    IDE_DASSERT( aSlotNum < sSlotDirHdr->mSlotEntryCount );

    sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, aSlotNum);
    /* �����Ϸ��� slot entry�� �ݵ�� used ���¿��� �Ѵ�.
     * unused�� slot entry�� �����Ϸ��� �ϴ� ����
     * �������� �ɰ��� �����̴�. */
    IDE_TEST( validateSlot( (UChar*)aPhyPageHdr, 
                             aSlotNum,
                             sSlotEntry, 
                             ID_TRUE ) /* isUnused, Unused�� �ȵ� */
              != IDE_SUCCESS);

    /* ���� ������ slot entry�� �����ϴ� ���,
     * slot entry���� shift�� �ʿ䰡 ����. */
    if( aSlotNum != (sSlotDirHdr->mSlotEntryCount - 1) )
    {
        shiftToTop(aPhyPageHdr, sSlotDirPtr, aSlotNum);
    }

    sSlotDirHdr->mSlotEntryCount--;
    aPhyPageHdr->mFreeSpaceBeginOffset -= ID_SIZEOF(sdpSlotEntry);
    aPhyPageHdr->mTotalFreeSize        += ID_SIZEOF(sdpSlotEntry);
    aPhyPageHdr->mAvailableFreeSize    += ID_SIZEOF(sdpSlotEntry);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  offset ��ġ�� slot�� ����Ű�� slot entry�� ã�´�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotOffset - [IN] �������������� slot offset
 *  aSlotNum    - [OUT]  offset ��ġ�� slot ��ȣ
 **********************************************************************/
IDE_RC sdpSlotDirectory::find(UChar            *aSlotDirPtr,
                              scOffset          aSlotOffset,
                              SShort           *aSlotNum )
{
    sdpSlotEntry    *sSlotEntry;
    SShort           sSlotNum = -1;

    IDE_DASSERT( aSlotDirPtr != NULL );

    for( sSlotNum = 0; sSlotNum < getCount(aSlotDirPtr); sSlotNum++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, sSlotNum);

        if( SDP_GET_OFFSET(sSlotEntry) == aSlotOffset )
        {
            IDE_TEST( validateSlot( aSlotDirPtr, 
                                    sSlotNum,
                                    sSlotEntry, 
                                    ID_TRUE ) /* isUnused, Unused�� �ȵ� */
                      != IDE_SUCCESS);
            break;
        }
    }
    *aSlotNum = sSlotNum;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  unused slot entry�� �ϳ��� �����ϴ��� ���θ� �����Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *
 **********************************************************************/
idBool sdpSlotDirectory::isExistUnusedSlotEntry(UChar       *aSlotDirPtr)
{
    sdpSlotDirHdr *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;

    IDE_DASSERT( aSlotDirPtr != NULL );

    if( sSlotDirHdr->mHead == SDP_INVALID_SLOT_ENTRY_NUM )
    {
        return ID_FALSE;
    }
    else
    {
        return ID_TRUE;
    }
}


/***********************************************************************
 *
 * Description :
 *  slot directory�� ũ�⸦ ��ȯ�Ѵ�.
 *
 *  aPhyPageHdr - [IN] physical page header
 *
 **********************************************************************/
UShort sdpSlotDirectory::getSize(sdpPhyPageHdr    *aPhyPageHdr)
{
    UShort    sSlotDirSize;

    IDE_DASSERT( aPhyPageHdr != NULL );

    sSlotDirSize =
        sdpPhyPage::getSlotDirEndPtr(aPhyPageHdr) -
        sdpPhyPage::getSlotDirStartPtr((UChar*)aPhyPageHdr);

    return sSlotDirSize;
}

/***********************************************************************
 * BUG-31534 [sm-util] dump utility and fixed table do not consider 
 *           unused slot.
 *
 * Description :
 *           UnusedSlotList�� ���� Slot����, ���� UnusedSlot�� �����´�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *  aSlotOffset - [OUT] slot offset
 **********************************************************************/
IDE_RC sdpSlotDirectory::getNextUnusedSlot(UChar       * aSlotDirPtr,
                                           scSlotNum     aSlotNum,
                                           scOffset    * aSlotOffset )
{
    sdpSlotDirHdr   *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry    *sSlotEntry;

    IDE_DASSERT( aSlotDirPtr != NULL );
    IDE_ASSERT( aSlotNum < sSlotDirHdr->mSlotEntryCount );

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum);
    IDE_TEST( validateSlot( aSlotDirPtr,
                            aSlotNum,
                            sSlotEntry,
                            ID_FALSE ) /* isUnused, Unused������ */
              != IDE_SUCCESS);
    *aSlotOffset = SDP_GET_OFFSET(sSlotEntry);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description :
 *  slot offset���� slot entry�� �����Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *  aSlotOffset - [IN] slot offset
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::setValue(UChar       *aSlotDirPtr,
                                  scSlotNum    aSlotNum,
                                  scOffset     aSlotOffset)
{
    sdpSlotDirHdr   *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry    *sSlotEntry;

    IDE_DASSERT( validate(aSlotDirPtr) == ID_TRUE );
    IDE_ASSERT( aSlotNum < sSlotDirHdr->mSlotEntryCount );

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum);
    IDE_TEST( validateSlot( aSlotDirPtr, 
                            aSlotNum,
                            sSlotEntry, 
                            ID_TRUE ) /* isUnused, Unused�� �ȵ� */
              != IDE_SUCCESS);
    SDP_SET_OFFSET(sSlotEntry, aSlotOffset);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *
 **********************************************************************/
void sdpSlotDirectory::shiftToBottom(sdpPhyPageHdr    *aPhyPageHdr,
                                     UChar            *aSlotDirPtr,
                                     UShort            aSlotNum)
{
    sdpSlotEntry   *sSlotEntry;
    UShort          sMoveSize;

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum);
    sMoveSize = (UShort)( sdpPhyPage::getSlotDirEndPtr(aPhyPageHdr) -
                          (UChar*)sSlotEntry );

    idlOS::memmove(sSlotEntry+1, sSlotEntry, sMoveSize);
}


/***********************************************************************
 *
 * Description :
 *
 *  aPhyPageHdr - [IN] physical page header
 *  aSlotDirPtr - [IN] slot directory ���� ������
 *  aSlotNum    - [IN] slot entry number
 *
 **********************************************************************/
void sdpSlotDirectory::shiftToTop(sdpPhyPageHdr    *aPhyPageHdr,
                                  UChar            *aSlotDirPtr,
                                  UShort            aSlotNum)
{
    sdpSlotEntry   *sNxtSlotEntry;
    UShort          sMoveSize;

    sNxtSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum+1);
    sMoveSize = (UShort)( sdpPhyPage::getSlotDirEndPtr(aPhyPageHdr) -
                          (UChar*)sNxtSlotEntry );

    idlOS::memmove(sNxtSlotEntry-1, sNxtSlotEntry, sMoveSize);
}


/***********************************************************************
 *
 * Description :
 *  unused slot entry list�� �ϳ��� slot entry�� �߰��Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory start ptr
 *  aSlotNum    - [IN] �߰��� slot entry number
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::addToUnusedSlotEntryList(UChar            *aSlotDirPtr,
                                                  scSlotNum         aSlotNum)
{
    sdpSlotDirHdr   *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry    *sSlotEntry;

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, aSlotNum);
    IDE_TEST( validateSlot( aSlotDirPtr,
                            aSlotNum,
                            sSlotEntry,
                            ID_FALSE ) /* isUnused, Unused������ */
              != IDE_SUCCESS);
    SDP_SET_OFFSET(sSlotEntry, sSlotDirHdr->mHead);

    sSlotDirHdr->mHead = aSlotNum;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}


/***********************************************************************
 *
 * Description :
 *  unused slot entry list���� �ϳ��� slot entry�� �����Ѵ�.
 *
 *  aSlotDirPtr - [IN]  slot directory start ptr
 *  aSlotNum    - [OUT] list���� ������ slot entry number
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::removeFromUnusedSlotEntryList(UChar     *aSlotDirPtr,
                                                       scSlotNum *aSlotNum)
{
    sdpSlotDirHdr   *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry    *sSlotEntry;
    scSlotNum        sFstUnusedSlotEntryNum;

    IDE_ASSERT( sSlotDirHdr->mHead != SDP_INVALID_SLOT_ENTRY_NUM );

    sFstUnusedSlotEntryNum = sSlotDirHdr->mHead;

    sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, sFstUnusedSlotEntryNum);
    IDE_TEST( validateSlot( aSlotDirPtr, 
                            sFstUnusedSlotEntryNum,
                            sSlotEntry, 
                            ID_FALSE ) /* isUnused, Unused������ */
              != IDE_SUCCESS);

    sSlotDirHdr->mHead = SDP_GET_OFFSET(sSlotEntry);

    *aSlotNum = sFstUnusedSlotEntryNum;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description :
 *  slot directory�� validity�� �˻��Ѵ�.
 *
 *  aSlotDirPtr - [IN] slot directory start ptr
 *
 **********************************************************************/
#ifdef DEBUG
idBool sdpSlotDirectory::validate(UChar    *aSlotDirPtr)
{
    sdpSlotDirHdr  *sSlotDirHdr = (sdpSlotDirHdr*)aSlotDirPtr;
    sdpSlotEntry   *sSlotEntry;
    UShort          sCount;
    UShort          sLoop;

    IDE_DASSERT( aSlotDirPtr != NULL );
    IDE_DASSERT( aSlotDirPtr ==
                 sdpPhyPage::getSlotDirStartPtr(aSlotDirPtr) );

    sCount      = getCount(aSlotDirPtr);

    for( sLoop = 0; sLoop < sCount; sLoop++ )
    {
        sSlotEntry = SDP_GET_SLOT_ENTRY(aSlotDirPtr, sLoop);

        if( SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) == ID_TRUE )
        {
            IDE_DASSERT( sSlotDirHdr->mHead != SDP_INVALID_SLOT_ENTRY_NUM );
        }
        else
        {
            IDE_DASSERT( SDP_GET_OFFSET(sSlotEntry) >=
                         sdpPhyPage::getFreeSpaceEndOffset(sdpPhyPage::getHdr(aSlotDirPtr)) );

            IDE_DASSERT( SDP_GET_OFFSET(sSlotEntry) <=
                         ( SD_PAGE_SIZE - ID_SIZEOF(sdpPageFooter) ) );
        }
    }
 
    return ID_TRUE;
}

#endif

/***********************************************************************
 * TASK-4007 [SM] PBT�� ���� ��� �߰�
 *
 * Description :
 *  slot directory�� dump�Ͽ� �����ش�.
 *
 *  aSlotDirPtr - [IN] page start ptr
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. *
 *
 **********************************************************************/
IDE_RC sdpSlotDirectory::dump(UChar    *aPagePtr,
                              SChar    *aOutBuf,
                              UInt      aOutSize )
{
    UChar           *sSlotDirPtr;
    sdpSlotDirHdr   *sSlotDirHdr;
    sdpSlotEntry    *sSlotEntry;
    UShort           sCount;
    UShort           sSize;
    UShort           sLoop;
    UChar           *sPagePtr;

    IDE_ERROR( aPagePtr == NULL );
    IDE_ERROR( aOutBuf  == NULL );
    IDE_ERROR( aOutSize  > 0 );

    sPagePtr = sdpPhyPage::getPageStartPtr(aPagePtr);

    IDE_DASSERT( sPagePtr == aPagePtr );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)sPagePtr);
    sSlotDirHdr = (sdpSlotDirHdr*)sSlotDirPtr;

    // Slot Directory�� ũ��� ȹ��
    sSize = getSize( (sdpPhyPageHdr*) sPagePtr );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Slot Directory Begin ----------\n"
                     "SlotDirectorySize : %"ID_UINT32_FMT"\n",
                     sSize );

    if( sSize != 0 )
    {
        sCount = sSlotDirHdr->mSlotEntryCount;


        /* BUG-31534 [sm-util] dump utility and fixed table do not consider 
         *           unused slot.
         * SlotDirHdr�� mHead���� UnusedSlot�� HeadSlot�� ����Ŵ */
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "SlotCount         : %"ID_UINT32_FMT"\n"
                             "SlotHead          : %"ID_UINT32_FMT"\n",
                             sSlotDirHdr->mSlotEntryCount,
                             sSlotDirHdr->mHead );

        // SlotDirectory�� ũ��� 4b(sdpSlotDirHdr) + SlotCount*2b(sdpSlotEntry)
        if( (sSize - ID_SIZEOF(sdpSlotDirHdr)) / ID_SIZEOF(sdpSlotEntry) !=
            sCount )
        {
            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "invalid size( slotDirSize != slotCount*%"ID_UINT32_FMT"b+%"ID_UINT32_FMT"b)\n",
                                 ID_SIZEOF(sdpSlotEntry),
                                 ID_SIZEOF(sdpSlotDirHdr) );
        }

        for( sLoop = 0; sLoop < sCount; sLoop++ )
        {
            sSlotEntry = SDP_GET_SLOT_ENTRY(sSlotDirPtr, sLoop);

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "[%"ID_UINT32_FMT"] %04x ", sLoop, *sSlotEntry );

            if( SDP_IS_UNUSED_SLOT_ENTRY(sSlotEntry) == ID_TRUE )
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "Unused slot\n");
            }
            else
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "%"ID_UINT32_FMT"\n", SDP_GET_OFFSET(sSlotEntry));
            }
        }
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- Slot Directory End   ----------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::getHdr �Լ���
 * wrapping�� �Լ��� �����.
 **********************************************************************/
sdpPhyPageHdr * sdpSlotDirectory::getPageHdr( UChar * aPagePtr )
{
    return sdpPhyPage::getHdr( aPagePtr );
}

/***********************************************************************
 * TASK-6105 �Լ� inlineȭ�� ���� sdpPhyPage::tracePage �Լ���
 * wrapping�� �Լ��� �����.
 **********************************************************************/
void sdpSlotDirectory::tracePage( UChar * aSlotDirPtr )
{
    (void)sdpPhyPage::tracePage( IDE_SERVER_0,
                                 aSlotDirPtr,
                                 NULL /*title*/ );
}

