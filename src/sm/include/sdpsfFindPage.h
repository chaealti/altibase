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
 * $Id:$
 ***********************************************************************/

# ifndef _O_SDPSF_FINDPAGE_H_
# define _O_SDPSF_FINDPAGE_H_ 1

# include <sdr.h>

# include <sdpReq.h>
# include <sdpsfDef.h>

class sdpsfFindPage
{
public:
    static IDE_RC findFreePage( idvSQL           *aStatistics,
                                sdrMtx           *aMtx,
                                scSpaceID         aSpaceID,
                                sdpSegHandle     *aSegHandle,
                                void             *aTableInfo,
                                sdpPageType       aPageType,
                                UInt              aRecordSize,
                                idBool            aNeedKeySlot,
                                UChar           **aPagePtr,
                                UChar            *aCTSlotIdx );

    static IDE_RC checkHintPID4Insert( idvSQL           *aStatistics,
                                       sdrMtx           *aMtx,
                                       scSpaceID         aSpaceID,
                                       void             *aTableInfo,
                                       UInt              aRecordSize,
                                       idBool            aNeedKeySlot,
                                       UChar           **aPagePtr,
                                       UChar            *aCTSlotIdx );

    static IDE_RC updatePageState( idvSQL           * aStatistics,
                                   sdrMtx           * aMtx,
                                   scSpaceID          aSpaceID,
                                   sdpSegHandle     * aSegHandle,
                                   UChar            * aDataPagePtr );

    static IDE_RC checkSizeAndAllocCTS(
                                   idvSQL          * aStatistics,
                                   sdrMtxStartInfo * aStartInfo,
                                   UChar           * aPagePtr,
                                   UInt              aRowSize,
                                   idBool            aNeedKeySlot,
                                   idBool          * aCanAllocSlot,
                                   idBool          * aRemFrmList,
                                   UChar           * aCTSlotIdx );

private:

    static inline idBool isPageInsertable( sdpPhyPageHdr *aPageHdr,
                                           UInt           aPctUsed );

    static inline idBool isPageUpdateOnly( sdpPhyPageHdr *aPageHdr,
                                           UInt           aPctFree );

};

/***********************************************************************
 *
 * Description :
 *  Page ���°� Insertable������ Check�Ѵ�.
 *
 *  aPageHdr - [IN] physical page header
 *  aPctUsed - [IN] PCTUSED
 *
 **********************************************************************/
inline idBool sdpsfFindPage::isPageInsertable( sdpPhyPageHdr *aPageHdr,
                                               UInt           aPctUsed )
{
    UInt   sUsedSize;
    idBool sIsInsertable;
    UInt   sPctUsedSize;

    IDE_DASSERT( aPageHdr != NULL );
    /* PCTUSED�� 0~99������ ���� ������ �Ѵ�. */
    IDE_DASSERT( aPctUsed < 100 );

    sIsInsertable = ID_FALSE;

    if( sdpPhyPage::canMakeSlotEntry( aPageHdr ) == ID_TRUE )
    {
        sUsedSize = SD_PAGE_SIZE -
            sdpPhyPage::getTotalFreeSize( aPageHdr )    -
            smLayerCallback::getTotAgingSize( aPageHdr ) +
            ID_SIZEOF(sdpSlotEntry);

        sPctUsedSize = ( SD_PAGE_SIZE * aPctUsed )  / 100 ;

        if( sUsedSize < sPctUsedSize )
        {
            /* Page�� ���� ��뷮�� PCTUSED���� �۾�����,
             * �ٽ� insert�� �� �ִ�. */
            sIsInsertable = ID_TRUE;
        }
    }

    return sIsInsertable;
}

/***********************************************************************
 *
 * Description :
 *  Page ���°� Update Only������ Check�Ѵ�.
 *
 *  aPageHdr - [IN] physical page header
 *  aPctFree - [IN] PCTFREE
 *
 **********************************************************************/
inline idBool sdpsfFindPage::isPageUpdateOnly( sdpPhyPageHdr *aPageHdr,
                                               UInt           aPctFree )
{
    UInt   sFreeSize;
    idBool sIsUpdateOnly;
    UInt   sPctFreeSize;

    IDE_DASSERT( aPageHdr != NULL );
    /* PCTFREE�� 0~99������ ���� ������ �Ѵ�. */
    IDE_DASSERT( aPctFree < 100 );

    sIsUpdateOnly = ID_FALSE;

    if( sdpPhyPage::canMakeSlotEntry( aPageHdr ) == ID_TRUE )
    {
        sFreeSize = sdpPhyPage::getTotalFreeSize( aPageHdr ) +
                    smLayerCallback::getTotAgingSize( aPageHdr );

        sPctFreeSize = ( SD_PAGE_SIZE * aPctFree )  / 100 ;

        if( sFreeSize < sPctFreeSize )
        {
            /* �������� ��������� PCTFREE���� �۰ԵǸ�,
             * �� �������� ���ο� row�� insert�� �� ����. */
            sIsUpdateOnly = ID_TRUE;
        }
    }

    return sIsUpdateOnly;
}

#endif /* _O_SDPSF_FINDPAGE_H_ */
