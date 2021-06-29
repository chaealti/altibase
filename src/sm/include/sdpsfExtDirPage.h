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
 *
 * $Id:$
 *
 * �� ������ extent directory page�� ���� ��������̴�.
 *
 **********************************************************************/

#ifndef _O_SDPSF_EXT_DIR_PAGE_H_
#define _O_SDPSF_EXT_DIR_PAGE_H_ 1

#include <sdr.h>
#include <sdpDef.h>
#include <sdpsfDef.h>
#include <sdpPhyPage.h>

class sdpsfExtDirPage
{
public:
    /* ext. dir. �ʱ�ȭ */
    static IDE_RC initialize( sdrMtx*              aMtx,
                              sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                              UShort               aMaxExtDescCnt );

    /* dummy �Լ� */
    static IDE_RC destroy();

    static IDE_RC getPage4Update( idvSQL              * aStatistics,
                                  sdrMtx              * aMtx,
                                  scSpaceID             aSpaceID,
                                  scPageID              aPageID,
                                  UChar              ** aPagePtr,
                                  sdpsfExtDirCntlHdr ** aRetExtDirHdr );

    static IDE_RC fixPage( idvSQL              * aStatistics,
                           scSpaceID             aSpaceID,
                           scPageID              aPageID,
                           UChar              ** aPagePtr,
                           sdpsfExtDirCntlHdr ** aRetExtDirHdr );

    static IDE_RC create( idvSQL              * aStatistics,
                          sdrMtx              * aMtx,
                          scSpaceID             aSpaceID,
                          scPageID              aPageID,
                          UChar              ** aPagePtr,
                          sdpsfExtDirCntlHdr ** aRetExtDirHdr);

    static IDE_RC addNewExtDescAtLst( sdrMtx             * aMtx,
                                      sdpsfExtDirCntlHdr * aExtDirHdr,
                                      scPageID             aFstPID,
                                      UInt                 aFlag,
                                      sdpsfExtDesc      ** aNewExtDesc );

    static IDE_RC freeLstExt( idvSQL             * aStatistics,
                              sdrMtx             * aMtx,
                              scSpaceID            aSpaceID,
                              sdpsfSegHdr        * aSegHdr,
                              sdpsfExtDirCntlHdr * aExtDirPageHdr );

    static IDE_RC freeAllExtExceptFst( idvSQL             * aStatistics,
                                       sdrMtxStartInfo    * aStartInfo,
                                       scSpaceID            aSpaceID,
                                       sdpsfSegHdr        * aSegHdr,
                                       sdpsfExtDirCntlHdr * aExtDirPageHdr );


    static IDE_RC freeAllNxtExt( idvSQL             * aStatistics,
                                 sdrMtxStartInfo    * aStartInfo,
                                 scSpaceID            aSpaceID,
                                 sdpsfSegHdr        * aSegHdr,
                                 sdpsfExtDirCntlHdr * aExtDirCntlHdr,
                                 sdRID                aExtRID );

    static IDE_RC getNxtExt( sdpsfExtDirCntlHdr * aExtDirHdr,
                             sdRID                aCurExtRID,
                             sdRID              * aNxtExtRID,
                             sdpsfExtDesc       * aExtDesc );

    static IDE_RC getPageInfo( idvSQL        * aStatistics,
                               scSpaceID       aSpaceID,
                               scPageID        aExtDirPID,
                               UShort        * aExtCnt,
                               sdRID         * aFstExtRID,
                               sdpsfExtDesc  * aFstExtDescPtr,
                               sdRID         * aLstExtRID,
                               scPageID      * aNxtExtDirPID );

    static IDE_RC cutFstExt( idvSQL             * aStatistics,
                             sdrMtx             * aMtx,
                             sdpsfExtDirCntlHdr * aExtDirCntlHdr,
                             sdpsfExtDesc      ** aExtDesc );

    static IDE_RC dump( sdpsfExtDirCntlHdr * aExtDirCntlHdr );

/* inline function */
public:
    /* ext dir page�� ������ �� �ִ� ext desc ���� ��ȯ */
    static inline UInt calcExtDescCntPerExtDir( UInt aExtDescSize );

    /* ext dir hdr�� ���� ptr (logical hdr) */
    static inline sdpsfExtDirCntlHdr* getExtDirCntlHdr( UChar* aPagePtr );

    static inline UInt  getFstExtOffset();
    static inline sdRID getFstExtRID( sdpsfExtDirCntlHdr* aExtDirPageHdr );
    static inline sdRID getFstExtRID( scPageID aExtDirPID );
    static inline sdRID getLstExtRID( sdpsfExtDirCntlHdr* aExtDirPageHdr );

    /* ù��° ext desc�� ptr�� ��ȯ */
    static inline sdpsfExtDesc* getFstExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr );
    static inline sdpsfExtDesc* getLstExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr );
    static inline sdpsfExtDesc* getNthExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr,
                                               UInt                aNthExt );

    inline static idBool isFull( sdpsfExtDirCntlHdr * aExtDirHdr );

    inline static UShort getMaxExtDescCntInSegHdrPage();
    inline static UShort getMaxExtDescCntInExtDirPage();

    inline static sdpsfExtDesc* getExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr,
                                            sdRID               aExtRID );

private:
    inline static IDE_RC setMaxExtDescCnt( sdrMtx*              aMtx,
                                           sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                           UShort               aMaxExtDescCnt );

    inline static IDE_RC setExtDescCnt( sdrMtx*              aMtx,
                                        sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                        UShort               aExtDescCnt );

    inline static IDE_RC setFstExtDescOffset( sdrMtx*              aMtx,
                                              sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                              scOffset             aFstExtDescOffset );

    inline static UInt getUsableFreeSize();
};

/***********************************************************************
 * Description: ������ ptr�� extent dir hdr�� ��ȯ
 **********************************************************************/
inline sdpsfExtDirCntlHdr* sdpsfExtDirPage::getExtDirCntlHdr( UChar*  aPagePtr )
{
    UChar*  sStartPtr;

    IDE_DASSERT( aPagePtr != NULL );

    sStartPtr = sdpPhyPage::getPageStartPtr(aPagePtr);

    return (sdpsfExtDirCntlHdr*)(sStartPtr + idlOS::align8((UInt)ID_SIZEOF(sdpPhyPageHdr)));
}

/***********************************************************************
 * Description :
 **********************************************************************/
inline UInt sdpsfExtDirPage::getUsableFreeSize()
{
    return (SD_PAGE_SIZE
            - idlOS::align8(ID_SIZEOF(sdpPhyPageHdr))
            - idlOS::align8(ID_SIZEOF(sdpsfExtDirCntlHdr))
            - idlOS::align8(ID_SIZEOF(sdpPageFooter)));
}

inline UInt  sdpsfExtDirPage::getFstExtOffset()
{
    return sdpPhyPage::getPhyHdrSize() + ID_SIZEOF( sdpsfExtDirCntlHdr );
}

inline sdRID sdpsfExtDirPage::getFstExtRID( sdpsfExtDirCntlHdr* aExtDirPageHdr )
{
    sdpsfExtDesc *sFstExtDesc = sdpsfExtDirPage::getFstExtDesc( aExtDirPageHdr );

    return sdpPhyPage::getRIDFromPtr( sFstExtDesc ); 
}

inline sdRID sdpsfExtDirPage::getLstExtRID( sdpsfExtDirCntlHdr* aExtDirPageHdr )
{
    sdpsfExtDesc *sLstExtDesc = sdpsfExtDirPage::getLstExtDesc( aExtDirPageHdr );

    return sdpPhyPage::getRIDFromPtr( sLstExtDesc ); 
}

inline sdRID sdpsfExtDirPage::getFstExtRID( scPageID aExtDirPID )
{
    return SD_MAKE_RID( aExtDirPID, sdpsfExtDirPage::getFstExtOffset() );
}

/***********************************************************************
 * Description : dir page���� header�� ������ ����Ÿ�� ����� ù ��ġ
 * ext dir �������� physical header�� logical header ũ�� ������ ptr
 * �� ���Ѵ�.
 * : page ptr + sdpPhyPageHdr ũ�� + sdpsfExtDirCntlHdr ũ�� +
 **********************************************************************/
inline sdpsfExtDesc* sdpsfExtDirPage::getFstExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr )
{
    IDE_DASSERT( aExtDirPageHdr != NULL );

    return (sdpsfExtDesc*)( sdpPhyPage::getPageStartPtr( aExtDirPageHdr )
                            + aExtDirPageHdr->mFstExtDescOffset );
}

inline sdpsfExtDesc* sdpsfExtDirPage::getLstExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr )
{
    IDE_DASSERT( aExtDirPageHdr != NULL );

    return (sdpsfExtDesc*)( sdpPhyPage::getPageStartPtr( aExtDirPageHdr )
                            + aExtDirPageHdr->mFstExtDescOffset
                            + ID_SIZEOF(sdpsfExtDesc) * ( aExtDirPageHdr->mExtDescCnt - 1 ) );
}

inline sdpsfExtDesc* sdpsfExtDirPage::getNthExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr,
                                                     UInt                aNthExt )
{
    sdpsfExtDesc *sExtDesc = sdpsfExtDirPage::getFstExtDesc( aExtDirPageHdr );

    IDE_DASSERT( aExtDirPageHdr != NULL );

    sExtDesc = sExtDesc + aNthExt;

    return sExtDesc;
}

/***********************************************************************
 * Description : ext dir page�� ����Ǵ� ext desc ���� ��ȯ
 * ( page ũ�� - align(ID_SIZEOF(physical page hdr))
 *             - align(ID_SIZEOF(logical page hdr)) ) / ext desc size ��ȯ
 **********************************************************************/
inline UInt sdpsfExtDirPage::calcExtDescCntPerExtDir( UInt aExtDescSize )
{
    IDE_DASSERT( aExtDescSize > 0 );
    return (UInt)( getUsableFreeSize() / aExtDescSize );
}

/***********************************************************************
 * Description : ext dir hdr�� Max ExtDesc Count����
 **********************************************************************/
inline IDE_RC sdpsfExtDirPage::setMaxExtDescCnt( sdrMtx*              aMtx,
                                                 sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                                 UShort               aMaxExtDescCnt )
{
    IDE_DASSERT( aExtDirPageHdr != NULL );
    IDE_DASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&( aExtDirPageHdr->mMaxExtDescCnt ),
                                      &aMaxExtDescCnt,
                                      ID_SIZEOF( aExtDirPageHdr->mMaxExtDescCnt ) );
}

/***********************************************************************
 * Description : ext dir hdr�� ExtDesc Count����
 **********************************************************************/
inline IDE_RC sdpsfExtDirPage::setExtDescCnt( sdrMtx*              aMtx,
                                              sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                              UShort               aExtDescCnt )
{
    IDE_DASSERT( aExtDirPageHdr != NULL );
    IDE_DASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&( aExtDirPageHdr->mExtDescCnt ),
                                      &aExtDescCnt,
                                      ID_SIZEOF( aExtDirPageHdr->mExtDescCnt ) );
}

inline IDE_RC sdpsfExtDirPage::setFstExtDescOffset( sdrMtx*              aMtx,
                                                    sdpsfExtDirCntlHdr*  aExtDirPageHdr,
                                                    scOffset             aFstExtDescOffset )
{
    IDE_DASSERT( aExtDirPageHdr != NULL );
    IDE_DASSERT( aMtx != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&( aExtDirPageHdr->mFstExtDescOffset ),
                                      &aFstExtDescOffset,
                                      ID_SIZEOF( aExtDirPageHdr->mFstExtDescOffset ) );
}

/***********************************************************************
 * Description : ext dir�������� Full�̸� ID_TRUE, �ƴϸ� ID_FALSE
 **********************************************************************/
inline idBool sdpsfExtDirPage::isFull( sdpsfExtDirCntlHdr * aExtDirHdr )
{
    IDE_ASSERT( aExtDirHdr->mExtDescCnt <= aExtDirHdr->mMaxExtDescCnt );

    if( aExtDirHdr->mExtDescCnt == aExtDirHdr->mMaxExtDescCnt )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/*
 * Extent Desc �������� ����� �� �ִ� �ִ� Extent Slot�� ������ȯ
 */
inline UShort sdpsfExtDirPage::getMaxExtDescCntInExtDirPage()
{
    UShort sMaxSlotCnt = (UShort)
        ((sdpPhyPage::getEmptyPageFreeSize() -
        ID_SIZEOF(sdpsfExtDirCntlHdr)) /
        ID_SIZEOF(sdpsfExtDesc));

    if( smuProperty::getExtDecCntInExtDirPage() != 0 )
    {
        return smuProperty::getExtDecCntInExtDirPage();
    }
    else
    {
        return sMaxSlotCnt;
    }
}

inline UShort sdpsfExtDirPage::getMaxExtDescCntInSegHdrPage()
{
    UShort sMaxSlotCnt = (UShort)
        ((sdpPhyPage::getEmptyPageFreeSize() -
        ID_SIZEOF(sdpsfSegHdr)) /
        ID_SIZEOF(sdpsfExtDesc));

    if( smuProperty::getExtDecCntInExtDirPage() != 0 )
    {
        return smuProperty::getExtDecCntInExtDirPage();
    }
    else
    {
        return sMaxSlotCnt;
    }
}

inline sdpsfExtDesc* sdpsfExtDirPage::getExtDesc( sdpsfExtDirCntlHdr* aExtDirPageHdr,
                                                  sdRID               aExtRID )
{
    UChar * sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aExtDirPageHdr );

    return (sdpsfExtDesc*)( sExtDirPagePtr + SD_MAKE_OFFSET( aExtRID ) );
}

#endif // _O_SDPSF_EXT_DIR_PAGE_H_
