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
 * $Id
 *
 * Description :
 *
 * TempPage
 * Persistent Page�� sdpPhyPage�� �����Ǵ� ���� �� ����ü�̴�.
 * TempPage�� LSN, SmoNo, PageState, Consistent �� sdpPhyPageHdr�� �ִ� ���
 * ���� ����� �����ϴ�.
 * ���� ����ȿ���� ���� ���� �̸� ��� �����Ѵ�.
 *
 *  # ���� �ڷᱸ��
 *
 *    - sdpPageHdr ����ü
 *
 **********************************************************************/

#ifndef _O_SDT_PAGE_H_
#define _O_SDT_PAGE_H_ 1

#include <smDef.h>
#include <sdr.h>
#include <smu.h>
#include <sdtSortDef.h>
#include <smrCompareLSN.h>

class sdtTempPage
{
public:
    static void init( sdtSortPageHdr  * aPageHdr,
                      sdtTempPageType   aType,
                      scPageID   aPrev,
                      scPageID   aSelf,
                      scPageID   aNext );

    inline static scSlotNum allocSlotDir( UChar * aPagePtr );

    inline static UChar * allocSlot( sdtSortPageHdr * aPageHdr,
                                     scSlotNum   aSlotNo,
                                     UInt        aNeedSize,
                                     UInt        aMinSize,
                                     UInt      * aRetSize );

    inline static IDE_RC getSlotPtr( UChar      * aPagePtr,
                                     scSlotNum    aSlotNo,
                                     UChar     ** aSlotPtr );

    inline static UInt getSlotDirOffset( scSlotNum    aSlotNo )
    {
        return SDT_SORT_PAGE_HEADER_SIZE + ( aSlotNo * ID_SIZEOF( scOffset ) );
    };

    inline static scOffset getSlotOffset( UChar      * aPagePtr,
                                          scSlotNum    aSlotNo )
    {
        return ((sdtSortPageHdr*)aPagePtr)->mSlotDir[aSlotNo];
    };

    inline static UInt getSlotCount( UChar * aPagePtr );

    /********************** GetSet ************************/
    static scPageID getPrevPID( UChar * aPagePtr )
    {
        return ( (sdtSortPageHdr*)aPagePtr )->mPrevPID;
    }
    static scPageID getSelfPID( UChar * aPagePtr )
    {
        return ( (sdtSortPageHdr*)aPagePtr )->mSelfPID;
    }
    static scPageID getNextPID( UChar * aPagePtr )
    {
        return ( (sdtSortPageHdr*)aPagePtr )->mNextPID;
    }

    static void setNextPID( UChar * aPagePtr, scPageID aNextNPID )
    {
        ((sdtSortPageHdr*)aPagePtr)->mNextPID = aNextNPID;
    }

    static sdtTempPageType getType( sdtSortPageHdr *aPageHdr )
    {
        return (sdtTempPageType)(aPageHdr->mTypeAndFreeOffset >> SDT_TEMP_TYPE_SHIFT);
    }

    /* FreeSpace�� ���۵Ǵ� Offset */
    static UInt getBeginFreeOffset( sdtSortPageHdr *aPageHdr )
    {
        return idlOS::align8( SDT_SORT_PAGE_HEADER_SIZE +
                              ( aPageHdr->mSlotCount * ID_SIZEOF( scOffset ) ));
    }

    /* FreeSpace�� ������ Offset */
    static UInt getFreeOffset( sdtSortPageHdr *aPageHdr )
    {
        return aPageHdr->mTypeAndFreeOffset & SDT_TEMP_FREEOFFSET_BITMASK;
    }
    static void setFreeOffset( sdtSortPageHdr *aPageHdr,
                               scOffset        aFreeOffset )
    {
        aPageHdr->mTypeAndFreeOffset = ( aPageHdr->mTypeAndFreeOffset & SDT_TEMP_TYPE_BITMASK ) | aFreeOffset;
    }


public:
    static void dumpTempPage( void  * aPagePtr,
                              SChar * aOutBuf,
                              UInt    aOutSize );
    static void dumpWAPageHeaders( void           * aWASegment,
                                   SChar          * aOutBuf,
                                   UInt             aOutSize );
    static SChar mPageName[ SDT_TEMP_PAGETYPE_MAX ][ SMI_TT_STR_SIZE ];
};


/**************************************************************************
 * Description :
 * ������ Pointer�� �����´�.
 *
 * <IN>
 * aPagePtr       - �ʱ�ȭ�� ��� Page
 * aSlotNo        - �Ҵ�޾Ҵ� Slot ��ȣ
 * <OUT>
 * aSlotPtr       - Slot�� ��ġ
 ***************************************************************************/
IDE_RC sdtTempPage::getSlotPtr( UChar      * aPagePtr,
                                scSlotNum    aSlotNo,
                                UChar     ** aSlotPtr )
{
    IDE_ERROR( ((sdtSortPageHdr*)aPagePtr)->mSlotCount > aSlotNo );

    *aSlotPtr = aPagePtr + getSlotOffset( aPagePtr, aSlotNo );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Slot�� ������ �����´�. Header���� �������� ������, ������ Slot��
 * UnusedSlot�̶��, ������� ���� ������ ġ�⿡ �߰� ó���� �ؾ� �Ѵ�.
 *
 * <IN>
 * aPagePtr       - �ʱ�ȭ�� ��� Page
 * <OUT>
 * aSlotCount     - Slot ����
 ***************************************************************************/
UInt sdtTempPage::getSlotCount( UChar * aPagePtr )
{
    sdtSortPageHdr * sHdr = (sdtSortPageHdr*)aPagePtr;

    if ( sHdr->mSlotCount > 1 )
    {
        /* Slot�� �ϳ��� �ִٸ�, �� Slot�� Unused���� üũ�Ѵ�. */
        if ( getSlotOffset( aPagePtr, sHdr->mSlotCount - 1 ) == SDT_TEMP_SLOT_UNUSED )
        {
            return sHdr->mSlotCount - 1;
        }
    }
    return sHdr->mSlotCount;
}

/**************************************************************************
 * Description :
 * ������ ���� �Ҵ�޴´�. �� ������ �ƹ��͵� ����ġ�� ����ü,
 * UNUSED(ID_USHORT_MAX)���� �ʱ�ȭ �Ǿ��ִ�.
 *
 * <IN>
 * aPagePtr       - �ʱ�ȭ�� ��� Page
 * <OUT>
 * aSlotNo        - �Ҵ��� Slot ��ȣ
 ***************************************************************************/
scSlotNum sdtTempPage::allocSlotDir( UChar * aPagePtr )
{
    sdtSortPageHdr *sHdr = (sdtSortPageHdr*)aPagePtr;

    if ( getSlotDirOffset( sHdr->mSlotCount + 1 ) < getFreeOffset( sHdr ) )
    {
        /* �Ҵ� ���� */
        sHdr->mSlotDir[sHdr->mSlotCount] = SDT_TEMP_SLOT_UNUSED;

        return sHdr->mSlotCount++;
    }

    return SDT_TEMP_SLOT_UNUSED;
}

/**************************************************************************
 * Description :
 * ������ ������ �Ҵ�޴´�. ��û�� ũ�⸸ŭ �Ҵ�Ǿ� ��ȯ�ȴ�.
 *
 * <IN>
 * aPagePtr       - �ʱ�ȭ�� ��� Page
 * aSlotNo        - �Ҵ�޾Ҵ� Slot ��ȣ
 * aNeedSize      - �䱸�ϴ� Byte����
 * aMinSize       - aNeedSize�� �ȵǸ� �ּ� aMinSize��ŭ�� �Ҵ�޾߾� �Ѵ�
 * <OUT>
 * aRetSize       - ������ �Ҵ���� ũ��
 * aRetPtr        - �Ҵ���� Slot�� Begin����
 ***************************************************************************/
UChar* sdtTempPage::allocSlot( sdtSortPageHdr  * aPageHdr,
                               scSlotNum   aSlotNo,
                               UInt        aNeedSize,
                               UInt        aMinSize,
                               UInt      * aRetSize )
{
    UInt sEndFreeOffset   = getFreeOffset( aPageHdr );
    UInt sBeginFreeOffset = getBeginFreeOffset( aPageHdr );
    UInt sFreeSize = ( sEndFreeOffset - sBeginFreeOffset );
    scOffset sRetOffset;

    if ( sFreeSize >= aMinSize )
    {
        if ( sFreeSize < aNeedSize )
        {
            /* �ּҺ��� ������, �˳����� ����.
             * Align�� BegeinFreeOffset���� ���θ� ��ȯ���� */
            sRetOffset = sBeginFreeOffset;
        }
        else
        {
            /* �Ҵ� ������.
             * �ʿ��� ������ŭ �� BeginOffset�� 8BteAling���� ����� */
            sRetOffset = sEndFreeOffset - aNeedSize;
            sRetOffset = sRetOffset - ( sRetOffset & 7 );
        }

        /* Align�� ���� Padding ������ sRetOffset�� ���� �˳��ϰ� �� ��
         * ����. ���� Min���� �ٿ��� */
        *aRetSize = IDL_MIN( sEndFreeOffset - sRetOffset,
                             aNeedSize );
        setFreeOffset( aPageHdr, sRetOffset );
        // Slot�� �����Ѵ�
        aPageHdr->mSlotDir[aSlotNo] = sRetOffset;

        return (UChar*)aPageHdr + sRetOffset;
    }

    *aRetSize = 0;

    return NULL;
}



#endif // _SDT_PAGE_H_
