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
 * - WorkAreaSortMap
 *     SortTemp�� extractNSort&insertNSort���� SortGroup�� �����ϴ� KeyMap��
 *     Merge���� Heap���� ���� �ΰ��� �������� �ִ�.
 *          WAPage�󿡸� �����Ѵ�. �� NPage�� �������� �ʴ´�.
 *          �� Pointing�ϱ� ���� �������� Array/Map������ �����Ѵ�.
 *     �� ������ ��� ���ڱ����� �Ϸ� �Ͽ�����, �󼼼��� ���� �ߺ��ڵ尡 �ɰ�
 *     ���Ƽ� sdtWASortMap���� �ϳ��� ��ģ��.
 *
 * - Ư¡
 *     NPage�� Assign ���� �ʴ´�.
 *     Array�� �����ϴ� Slot�� ũ��� �����̴�.
 *     Address�� �������� �ش� Array�� Random Access �ϴ� ���̴�.
 **********************************************************************/

#ifndef _O_SDT_WA_SORT_MAP_H_
#define _O_SDT_WA_SORT_MAP_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smiDef.h>
#include <sdtDef.h>
#include <iduLatch.h>
#include <sdtSortSegment.h>

class sdtWASortMap
{
public:
    static IDE_RC create( sdtSortSegHdr* aWASegment,
                          sdtGroupID     aWAGID,
                          sdtWMType      aWMType,
                          UInt           aSlotCount,
                          UInt           aVersionCount,
                          sdtWASortMapHdr  * aWAMapHdr );
    static void disable(  sdtSortSegHdr * aWASegment );

    static IDE_RC resetPtrAddr( sdtSortSegHdr* aWASegment );
    inline static IDE_RC set( sdtWASortMapHdr * aWAMapHdr,
                              UInt              aIdx,
                              void            * aValue );
    inline static IDE_RC setNextVersion( sdtWASortMapHdr * aWAMapHdr,
                                         UInt   aIdx,
                                         void * aValue );
    inline static IDE_RC setvULong( sdtWASortMapHdr   * aWAMapHdr,
                                    UInt     aIdx,
                                    vULong * aValue );

    inline static IDE_RC get( sdtWASortMapHdr * aWAMapHdr,
                              UInt              aIdx,
                              void            * aValue );
    inline static IDE_RC getvULong( sdtWASortMapHdr   * aWAMapHdr,
                                    UInt     aIdx,
                                    vULong * aValue );

    inline static IDE_RC getSlotPtr( sdtWASortMapHdr *  aWAMapHdr,
                                     UInt    aIdx,
                                     UInt    aSlotSize,
                                     void ** aSlotPtr );

    inline static IDE_RC getSlotPtrWithVersion( sdtWASortMapHdr *  aWAMapHdr,
                                                UInt    aVersionIdx,
                                                UInt    aIdx,
                                                UInt    aSlotSize,
                                                void ** aSlotPtr );

    static UInt getOffset( UInt   aSlotIdx,
                           UInt   aSlotSize,
                           UInt   aVersionIdx,
                           UInt   aVersionCount )
    {
        return ( ( aSlotIdx * aVersionCount ) + aVersionIdx ) * aSlotSize;
    }

    inline static IDE_RC swap( sdtWASortMapHdr * aWAMapHdr,
                               UInt aIdx1,
                               UInt aIdx2 );
    inline static IDE_RC swapvULong( sdtWASortMapHdr * aWAMapHdr,
                                     UInt   aIdx1,
                                     UInt   aIdx2 );
    inline static IDE_RC incVersionIdx( sdtWASortMapHdr * aWAMapHdr );

    static IDE_RC expand( sdtWASortMapHdr * aWAMapHdr,
                          scPageID   aLimitPID,
                          UInt     * aIdx );

    /* Map�� ũ�� (Map������ Slot����)�� ��ȯ�� */
    static UInt getSlotCount( sdtWASortMapHdr * aWAMapHdr )
    {
        return aWAMapHdr->mSlotCount;
    }

    /* Slot�ϳ��� ũ�⸦ ��ȯ��. */
    static UInt getSlotSize( sdtWASortMapHdr * aWAMapHdr )
    {
        return aWAMapHdr->mSlotSize;
    }

    /* WAMap�� ����ϴ� �������� ������ ��ȯ*/
    static UInt getWPageCount( sdtWASortMapHdr * aWAMapHdr )
    {
        if ( aWAMapHdr == NULL )
        {
            return 0;
        }
        else
        {
            return ( aWAMapHdr->mSlotCount * aWAMapHdr->mSlotSize
                     * aWAMapHdr->mVersionCount
                     / SD_PAGE_SIZE ) + 1;
        }
    }
    /* WAMap�� ����ϴ� ������ �������� ��ȯ�� */
    static scPageID getEndWPID( sdtWASortMapHdr * aWAMapHdr )
    {
        return aWAMapHdr->mBeginWPID + getWPageCount( aWAMapHdr );
    }


    static void dumpWASortMap( sdtWASortMapHdr * aMapHdr,
                               SChar           * aOutBuf,
                               UInt              aOutSize );
};

/**************************************************************************
 * Description :
 * �ش� WAMap�� Index�� Slot�� Value�� �����Ѵ�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtWASortMap::set( sdtWASortMapHdr * aWAMapHdr,
                          UInt              aIdx,
                          void            * aValue )
{
    void        * sSlotPtr;
    UInt          sSlotSize;

    sSlotSize = aWAMapHdr->mSlotSize;

    IDE_TEST( getSlotPtr( aWAMapHdr, aIdx, sSlotSize, &sSlotPtr )
              != IDE_SUCCESS );

    idlOS::memcpy( sSlotPtr, aValue, sSlotSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �ش� WAMap�� Index�� Slot�� Value�� �����ϵ�, '���� ����'�� �����Ѵ�.
 * sdtSortModule::mergeSort ����
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtWASortMap::setNextVersion( sdtWASortMapHdr * aWAMapHdr,
                                     UInt              aIdx,
                                     void            * aValue )
{
    void        * sSlotPtr;
    UInt          sSlotSize;
    UInt          sVersionIdx;

    sSlotSize = aWAMapHdr->mSlotSize;
    sVersionIdx = ( aWAMapHdr->mVersionIdx + 1 ) % aWAMapHdr->mVersionCount;
    IDE_TEST( getSlotPtrWithVersion( aWAMapHdr,
                                     sVersionIdx,
                                     aIdx,
                                     sSlotSize,
                                     &sSlotPtr )
              != IDE_SUCCESS );

    idlOS::memcpy( sSlotPtr, aValue, sSlotSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * �ش� WAMap�� Index�� Slot���� vULongValue�� �����Ѵ�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtWASortMap::setvULong( sdtWASortMapHdr * aWAMapHdr,
                                UInt              aIdx,
                                vULong          * aValue )
{
    void * sSlotPtr;

    IDE_DASSERT( ID_SIZEOF( vULong ) == getSlotSize( aWAMapHdr ) );
    IDE_TEST( getSlotPtr( aWAMapHdr, aIdx, ID_SIZEOF( vULong ), &sSlotPtr )
              != IDE_SUCCESS );

    *(vULong*)sSlotPtr = *aValue;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �ش� WAMap�� Index�� Slot���� Value�� �����´�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtWASortMap::get( sdtWASortMapHdr * aWAMapHdr,
                          UInt              aIdx,
                          void            * aValue )
{
    void   * sSlotPtr;
    UInt     sSlotSize;

    sSlotSize = aWAMapHdr->mSlotSize;
    IDE_TEST( getSlotPtr( aWAMapHdr, aIdx, sSlotSize, &sSlotPtr )
              != IDE_SUCCESS );

    idlOS::memcpy( aValue, sSlotPtr, sSlotSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �ش� WAMap�� Index�� Slot���� vULongValue�� �����´�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aValue         - ������ Value
 ***************************************************************************/
IDE_RC sdtWASortMap::getvULong( sdtWASortMapHdr * aWAMapHdr,
                                UInt              aIdx,
                                vULong          * aValue )
{
    void   * sSlotPtr;

    IDE_DASSERT( ID_SIZEOF( vULong ) == getSlotSize( aWAMapHdr ) );
    IDE_TEST( getSlotPtr( aWAMapHdr, aIdx, ID_SIZEOF( vULong ), &sSlotPtr )
              != IDE_SUCCESS );

    *aValue = *(vULong*)sSlotPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * WAMap�� �ش� Slot�� �ּҰ��� ��ȯ�Ѵ�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aSlotSize      - ��� WAMap���� �� Slot�� ũ��
 * <OUT>
 * aSlotPtr       - ã�� Slot�� ��ġ
 ***************************************************************************/
IDE_RC sdtWASortMap::getSlotPtr( sdtWASortMapHdr *  aWAMapHdr,
                                 UInt               aIdx,
                                 UInt               aSlotSize,
                                 void            ** aSlotPtr )
{
    return getSlotPtrWithVersion( aWAMapHdr,
                                  aWAMapHdr->mVersionIdx,
                                  aIdx,
                                  aSlotSize,
                                  aSlotPtr );
}




/**************************************************************************
 * Description :
 * WAMap�� �ش� Slot�� �ּҰ��� ��ȯ�Ѵ�. Version�� ����Ͽ� �����´�.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx           - ��� Index
 * aSlotSize      - ��� WAMap���� �� Slot�� ũ��
 * <OUT>
 * aSlotPtr       - ã�� Slot�� ��ġ
 ***************************************************************************/
IDE_RC sdtWASortMap::getSlotPtrWithVersion( sdtWASortMapHdr * aWAMapHdr,
                                            UInt              aVersionIdx,
                                            UInt              aIdx,
                                            UInt              aSlotSize,
                                            void           ** aSlotPtr )
{
    UInt        sOffset;
    scPageID    sPageID;
    UChar     * sPagePtr;
    sdtWCB    * sWCBPtr;

    IDE_DASSERT( aIdx < aWAMapHdr->mSlotCount );

    sOffset = getOffset( aIdx,
                         aSlotSize,
                         aVersionIdx,
                         aWAMapHdr->mVersionCount );
    sPageID = aWAMapHdr->mBeginWPID + ( sOffset / SD_PAGE_SIZE ) ;

    sWCBPtr = sdtSortSegment::getWCBWithLnk( aWAMapHdr->mWASegment,
                                             sPageID );

    IDE_TEST( sdtSortSegment::chkAndSetWAPagePtr( aWAMapHdr->mWASegment,
                                                  sWCBPtr ) != IDE_SUCCESS );

    sPagePtr = sdtSortSegment::getWAPagePtr( sWCBPtr );

    *aSlotPtr = sPagePtr + ( sOffset % SD_PAGE_SIZE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**************************************************************************
 * Description :
 * �� Slot�� ���� ��ȯ�Ѵ�.
 * sdtTempTable::quickSort ����
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx1,aIdx2    - ġȯ�� �� Slot
 ***************************************************************************/
IDE_RC sdtWASortMap::swap( sdtWASortMapHdr * aWAMapHdr,
                           UInt              aIdx1,
                           UInt              aIdx2 )
{
    UChar         sBuffer[ SDT_WAMAP_SLOT_MAX_SIZE ];
    UInt          sSlotSize;
    void        * sSlotPtr1;
    void        * sSlotPtr2;

    if ( aIdx1 == aIdx2 )
    {
        /* ������ ��ȯ�� �ʿ� ���� */
    }
    else
    {
        sSlotSize = aWAMapHdr->mSlotSize;

        IDE_TEST( getSlotPtr( aWAMapHdr, aIdx1, sSlotSize, &sSlotPtr1 )
                  != IDE_SUCCESS );
        IDE_TEST( getSlotPtr( aWAMapHdr, aIdx2, sSlotSize, &sSlotPtr2 )
                  != IDE_SUCCESS );

#if defined(DEBUG)
        /* �ٸ� �� ���� ���ؾ� �Ѵ�. */
        IDE_ERROR( sSlotPtr1 != sSlotPtr2 );
        /* �� Slot���� ���̰� Slot�� ũ�⺸�� ���ų� Ŀ��,
         * ��ġ�� �ʵ��� �ؾ� �Ѵ�. */
        if ( sSlotPtr1 < sSlotPtr2 )
        {
            IDE_ERROR( ((UChar*)sSlotPtr2) - ((UChar*)sSlotPtr1) >= sSlotSize );
        }
        else
        {
            IDE_ERROR( ((UChar*)sSlotPtr1) - ((UChar*)sSlotPtr2) >= sSlotSize );
        }
#endif

        idlOS::memcpy( sBuffer, sSlotPtr1, sSlotSize );
        idlOS::memcpy( sSlotPtr1, sSlotPtr2, sSlotSize );
        idlOS::memcpy( sSlotPtr2, sBuffer, sSlotSize );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * �� vULongSlot�� ���� ��ȯ�Ѵ�.
 * sdtTempTable::quickSort ����
 *
 * <IN>
 * aWAMap         - ��� WAMap
 * aIdx1,aIdx2    - ġȯ�� �� Slot
 ***************************************************************************/
IDE_RC sdtWASortMap::swapvULong( sdtWASortMapHdr * aWAMapHdr,
                                 UInt              aIdx1,
                                 UInt              aIdx2 )
{
    UInt          sSlotSize;
    vULong      * sSlotPtr1;
    vULong      * sSlotPtr2;
    vULong        sValue;

    IDE_DASSERT( ID_SIZEOF( vULong ) == getSlotSize( aWAMapHdr ) );

    if ( aIdx1 == aIdx2 )
    {
        /* ������ ��ȯ�� �ʿ� ���� */
    }
    else
    {
        sSlotSize = ID_SIZEOF( vULong );

        IDE_TEST( getSlotPtr( aWAMapHdr, aIdx1, sSlotSize, (void**)&sSlotPtr1)
                  != IDE_SUCCESS );
        IDE_TEST( getSlotPtr( aWAMapHdr, aIdx2, sSlotSize, (void**)&sSlotPtr2)
                  != IDE_SUCCESS );

#if defined(DEBUG)
        /* �ٸ� �� ���� ���ؾ� �Ѵ�. */
        IDE_ERROR( sSlotPtr1 != sSlotPtr2 );
        /* �� Slot���� ���̰� Slot�� ũ�⺸�� ���ų� Ŀ��,
         * ��ġ�� �ʵ��� �ؾ� �Ѵ�. */
        if ( sSlotPtr1 < sSlotPtr2 )
        {
            IDE_ERROR( ((UChar*)sSlotPtr2) - ((UChar*)sSlotPtr1) >= sSlotSize );
        }
        else
        {
            IDE_ERROR( ((UChar*)sSlotPtr1) - ((UChar*)sSlotPtr2) >= sSlotSize );
        }
#endif

        sValue     = *sSlotPtr1;
        *sSlotPtr1 = *sSlotPtr2;
        *sSlotPtr2 = sValue;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**************************************************************************
 * Description :
 * Map�� ������ ������Ų��.
 *
 * <IN>
 * aWAMap         - ��� WAMap
 ***************************************************************************/
IDE_RC sdtWASortMap::incVersionIdx( sdtWASortMapHdr * aWAMapHdr )
{
    aWAMapHdr->mVersionIdx = ( aWAMapHdr->mVersionIdx + 1 ) % aWAMapHdr->mVersionCount;

    return IDE_SUCCESS;
}

#endif //_O_SDT_WA_SORT_MAP_H_
