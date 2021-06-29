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

# ifndef _O_SDPSF_SH_H_
# define _O_SDPSF_SH_H_ 1

# include <sdpsfDef.h>
# include <sdr.h>
# include <sdpsfDef.h>
# include <sdpsfUFmtPIDList.h>
# include <sdpsfFreePIDList.h>
# include <sdpsfPvtFreePIDList.h>
# include <sdpPhyPage.h>
# include <sdpSglPIDList.h>

class sdpsfSH
{
public:

    /* [ INTERFACE ] Segment ��� �ʱ�ȭ */
    static IDE_RC initialize( sdpSegHandle * aSegHandle,
                              scSpaceID      aSpaceID,
                              sdpSegType     aSegType,
                              smOID          aObjectID,
                              UInt           aIndexID  );

    /* [ INTERFACE ] Segment ��� ���� */
    static IDE_RC destroy( sdpSegHandle * aSegHandle );

    /* Segment Descriptor�� �ʱ�ȭ�Ѵ�. */
    static IDE_RC init( idvSQL           * aStatistics,
                        sdrMtx           * aMtx,
                        scSpaceID          aSpaceID,
                        sdpsfSegHdr      * aSegHdr,
                        scPageID           aSegPageID,
                        scPageID           aHWM,
                        UInt               aPageCntInExt,
                        sdpSegType         aType );

    /* SegHdr Header Fix, Unfix Function */
    static IDE_RC fixAndGetSegHdr4Update( idvSQL        * aStatistics,
                                          sdrMtx        * aMtx,
                                          scSpaceID       aSpaceID,
                                          scPageID        aSegPID,
                                          sdpsfSegHdr  ** aSegHdr);

    static IDE_RC fixAndGetSegHdr4Update( idvSQL        * aStatistics,
                                          scSpaceID       aSpaceID,
                                          scPageID        aSegPID,
                                          sdpsfSegHdr  ** aSegHdr );


    static IDE_RC fixAndGetSegHdr4Read( idvSQL        * aStatistics,
                                        scSpaceID       aSpaceID,
                                        scPageID        aSegPID,
                                        sdpsfSegHdr  ** aSegHdr);

    static IDE_RC releaseSegHdr( idvSQL        * aStatistics,
                                 sdpsfSegHdr   * aSegHdr );

    static IDE_RC getSegInfo( idvSQL        *aStatistics,
                              scSpaceID      aSpaceID,
                              scPageID       aSegPID,
                              void          *aTableHeader,
                              sdpSegInfo    *aSetInfo );

    static IDE_RC getSegCacheInfo( idvSQL             */*aStatistics*/,
                                   sdpSegHandle       */*aSegHandle*/,
                                   sdpSegCacheInfo    *aSegCacheInfo );

    static IDE_RC getFmtPageCnt( idvSQL        *aStatistics,
                                 scSpaceID      aSpaceID,
                                 sdpSegHandle  *aSegHandle,
                                 ULong         *aFmtPageCnt );

    static IDE_RC updateHWMInfo4DPath( idvSQL           *aStatistics,
                                       sdrMtxStartInfo  *aStartInfo,
                                       scSpaceID         aSpaceID,
                                       sdpSegHandle     *aSegHandle,
                                       scPageID          aPrvLstAllocPID,
                                       sdRID             aLstAllocExtRID,
                                       scPageID          aFstPIDOfLstAllocExt,
                                       scPageID          aLstAllocPID,
                                       ULong             aAllocPageCnt,
                                       idBool            aMegeMultiSeg );

    static IDE_RC getSegState( idvSQL        *aStatistics,
                               scSpaceID      aSpaceID,
                               scPageID       aSegPID,
                               sdpSegState   *aSegState );


    static IDE_RC setMetaPID( idvSQL        *aStatistics,
                              sdrMtx        *aMtx,
                              scSpaceID      aSpaceID,
                              scPageID       aSegPID,
                              UInt           aIndex,
                              scPageID       aPageID );

    static IDE_RC getMetaPID( idvSQL        *aStatistics,
                              scSpaceID      aSpaceID,
                              scPageID       aSegPID,
                              UInt           aIndex,
                              scPageID      *aPageID );

    static IDE_RC buildRecord4Dump( idvSQL              * /*aStatistics*/,
                                    void                * aHeader,
                                    void                * aDumpObj,
                                    iduFixedTableMemory * aMemory );

    static IDE_RC freePage( idvSQL            * aStatistics,
                            sdrMtx            * aMtx,
                            scSpaceID           aSpaceID,
                            sdpSegHandle      * aSegmentHandle,
                            UChar             * aPagePtr );

    static IDE_RC updateHWM( idvSQL           *aStatistics,
                             sdrMtxStartInfo  *aStartInfo,
                             scSpaceID         aSpaceID,
                             scPageID          aSegPID,
                             sdRID             aLstAllocExtRID,
                             scPageID          aLstAllocPID );

    static IDE_RC reformatPage4DPath( idvSQL           * /*aStatistics*/,
                                      sdrMtxStartInfo  * /*aStartInfo*/,
                                      scSpaceID          /*aSpaceID*/,
                                      sdpSegHandle     * /*aSegHandle*/,
                                      sdRID              /*aLstAllocExtRID*/,
                                      scPageID           /*aLstPID*/ );

    static IDE_RC setLstAllocPage( idvSQL           * /*aStatistics*/,
                                   sdpSegHandle     * /*aSegHandle*/,
                                   scPageID           /*aLstAllocPID*/,
                                   ULong              /*aLstAllocSeqNo*/ );

/* inline function */
public:
    /* Segment Ÿ���� ��ȯ�Ѵ�. */
    inline static sdpSegType getType( sdpsfSegHdr *aSegHdr );

    inline static IDE_RC setSegHdrPID( sdrMtx       * aMtx,
                                       sdpsfSegHdr  * aSegHdr,
                                       scPageID       aSegHdrPID );

    /* Segment Ÿ���� �����Ѵ�. */
    inline static IDE_RC setType( sdrMtx      *aMtx,
                                  sdpsfSegHdr *aSegHdr,
                                  sdpSegType   aType );

    /* Segment ���¸� ��ȯ�Ѵ�. */
    inline static sdpSegState getState( sdpsfSegHdr *aSegHdr );

    /* Segment ���¸� �����Ѵ�. */
    inline static IDE_RC setState( sdrMtx      *aMtx,
                                   sdpsfSegHdr *aSegHdr,
                                   sdpSegState  aState );

    /* Segment�� Extent Used List�� Base ��带 ��ȯ�Ѵ�. */
    inline static sdpSglPIDListBase* getPvtFreePIDList( sdpsfSegHdr *aSegHdr );
    inline static sdpSglPIDListBase* getFreePIDList( sdpsfSegHdr *aSegHdr );
    inline static sdpSglPIDListBase* getUFmtPIDList( sdpsfSegHdr *aSegHdr );

    inline static ULong getFmtPageCnt( sdpsfSegHdr *aSegHdr );

    inline static sdpsfSegHdr* getSegHdrFromPagePtr( UChar*  aPagePtr );

    inline static IDE_RC setHWM( sdrMtx       * aMtx,
                                 sdpsfSegHdr  * aSegHdr,
                                 scPageID       aPageID );


    inline static IDE_RC setMetaPID( sdrMtx       * aMtx,
                                     sdpsfSegHdr  * aSegHdr,
                                     UInt           aIndex,
                                     scPageID       aMetaPID );

    inline static scPageID getMetaPID( sdpsfSegHdr * aSegHdr,
                                       UInt aIndex );

    inline static IDE_RC setPageCntInExt( sdrMtx       * aMtx,
                                          sdpsfSegHdr  * aSegHdr,
                                          UInt           aPageCntInExt );

    inline static IDE_RC setMaxExtCntInSegHdrPage( sdrMtx       * aMtx,
                                                   sdpsfSegHdr  * aSegHdr,
                                                   UShort         aMaxExtCnt );

    inline static IDE_RC setMaxExtCntInExtDirPage( sdrMtx       * aMtx,
                                                   sdpsfSegHdr  * aSegHdr,
                                                   UShort         aMaxExtCnt );

    inline static IDE_RC setAllocExtRID( sdrMtx       * aMtx,
                                         sdpsfSegHdr  * aSegHdr,
                                         sdRID          aExtRID );

    inline static IDE_RC setFstPIDOfAllocExt( sdrMtx       * aMtx,
                                              sdpsfSegHdr  * aSegHdr,
                                              scPageID       aExtPID );

    inline static IDE_RC setFmtPageCnt( sdrMtx       * aMtx,
                                        sdpsfSegHdr  * aSegHdr,
                                        ULong          aPageCnt );

    inline static ULong getTotalPageCnt( sdpsfSegHdr * aSegHdr );

    /*  Segment Descriptor �ڷᱸ���� ũ�⸦ ��ȯ�Ѵ� */
    inline static UInt getDescSize();

    inline static ULong getFreePageCnt( sdpsfSegHdr * aSegHdr );

    inline static IDE_RC setTotExtCnt( sdrMtx       * aMtx,
                                       sdpsfSegHdr  * aSegHdr,
                                       ULong          aTotExtCnt );
};

/* Description: ������ ptr�� extent dir hdr�� ��ȯ */
inline sdpsfSegHdr* sdpsfSH::getSegHdrFromPagePtr( UChar*  aPagePtr )
{
    UChar*  sStartPtr;

    IDE_DASSERT( aPagePtr != NULL );

    sStartPtr = sdpPhyPage::getPageStartPtr(aPagePtr);

    return (sdpsfSegHdr*)(sStartPtr + SDPSF_SEGHDR_OFFSET );

}

/* Segment Descriptor�� Type�� ��ȯ�Ѵ�. */
sdpSegType sdpsfSH::getType( sdpsfSegHdr  *aSegHdr )
{
    return (sdpSegType)(aSegHdr->mType);
}

/* Segment Descriptor�� ���¸� ��ȯ�Ѵ�. */
sdpSegState sdpsfSH::getState( sdpsfSegHdr     *aSegHdr )
{
    return (sdpSegState)(aSegHdr->mState);
}

/* Segment Descriptor�� ũ�⸦ ��ȯ�Ѵ�. */
UInt sdpsfSH::getDescSize()
{
    return idlOS::align8((UInt)ID_SIZEOF(sdpsfSegHdr));
}

/* Private Page List�� ��ȯ�Ѵ�. */
inline sdpSglPIDListBase* sdpsfSH::getPvtFreePIDList( sdpsfSegHdr *aSegHdr )
{
    return &aSegHdr->mPvtFreePIDList;
}

/* Free Page List�� ��ȯ�Ѵ�. */
inline sdpSglPIDListBase* sdpsfSH::getFreePIDList( sdpsfSegHdr *aSegHdr )
{
    return &aSegHdr->mFreePIDList;
}

/* UnFormat Page List�� ��ȯ�Ѵ�. */
inline sdpSglPIDListBase* sdpsfSH::getUFmtPIDList( sdpsfSegHdr *aSegHdr )
{
    return &aSegHdr->mUFmtPIDList;
}

/* Alloc Page������ ��ȯ�Ѵ�. */
inline ULong sdpsfSH::getFmtPageCnt( sdpsfSegHdr *aSegHdr )
{
    return aSegHdr->mFmtPageCnt;
}

/* SegHdr�� HWM�� �����Ѵ�. */
inline IDE_RC sdpsfSH::setHWM( sdrMtx       * aMtx,
                               sdpsfSegHdr  * aSegHdr,
                               scPageID       aPageID )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mHWMPID,
                  &aPageID,
                  ID_SIZEOF( aPageID ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* SegHdr�� META PID�� �����Ѵ�. */
inline IDE_RC sdpsfSH::setMetaPID( sdrMtx       * aMtx,
                                   sdpsfSegHdr  * aSegHdr,
                                   UInt           aIndex,
                                   scPageID       aPageID )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mArrMetaPID[ aIndex ],
                  &aPageID,
                  ID_SIZEOF( aPageID ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Meta PID�� ��ȯ�Ѵ�. */
inline scPageID sdpsfSH::getMetaPID( sdpsfSegHdr * aSegHdr,
                                     UInt          aIndex )
{
    return aSegHdr->mArrMetaPID[ aIndex ];
}

/* Extent�� ������ ������ ������� SegHdr�� �����Ѵ�. */
inline IDE_RC sdpsfSH::setPageCntInExt( sdrMtx       * aMtx,
                                        sdpsfSegHdr  * aSegHdr,
                                        UInt           aPageCntInExt )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mPageCntInExt,
                  &aPageCntInExt,
                  ID_SIZEOF( UInt ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpsfSH::setMaxExtCntInSegHdrPage( sdrMtx       * aMtx,
                                                 sdpsfSegHdr  * aSegHdr,
                                                 UShort         aMaxExtCnt )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mMaxExtCntInSegHdrPage,
                  &aMaxExtCnt,
                  ID_SIZEOF( UShort ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpsfSH::setMaxExtCntInExtDirPage( sdrMtx       * aMtx,
                                                 sdpsfSegHdr  * aSegHdr,
                                                 UShort         aMaxExtCnt )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mMaxExtCntInExtDirPage,
                  &aMaxExtCnt,
                  ID_SIZEOF( UShort ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* ���������� Alloc�� Extent�� RID�� ��Ÿ���� */
inline IDE_RC sdpsfSH::setAllocExtRID( sdrMtx       * aMtx,
                                       sdpsfSegHdr  * aSegHdr,
                                       sdRID          aExtRID )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mAllocExtRID,
                  &aExtRID,
                  ID_SIZEOF( sdRID ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

inline IDE_RC sdpsfSH::setFstPIDOfAllocExt( sdrMtx       * aMtx,
                                            sdpsfSegHdr  * aSegHdr,
                                            scPageID       aExtFstPID )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mFstPIDOfAllocExt,
                  &aExtFstPID,
                  ID_SIZEOF( scPageID ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Alloc�� ������������ Return�Ѵ�. */
inline IDE_RC sdpsfSH::setFmtPageCnt( sdrMtx       * aMtx,
                                      sdpsfSegHdr  * aSegHdr,
                                      ULong          aPageCnt )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mFmtPageCnt,
                  &aPageCnt,
                  ID_SIZEOF( ULong ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �� ������ ������ �����Ѵ�. */
inline ULong sdpsfSH::getTotalPageCnt( sdpsfSegHdr * aSegHdr )
{
    return aSegHdr->mTotExtCnt * aSegHdr->mPageCntInExt;
}

inline IDE_RC sdpsfSH::setSegHdrPID( sdrMtx       * aMtx,
                                     sdpsfSegHdr  * aSegHdr,
                                     scPageID       aSegHdrPID )
{
    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aSegHdr->mSegHdrPID,
                                         &aSegHdrPID,
                                         ID_SIZEOF( aSegHdr->mSegHdrPID ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Segment Descriptor�� Type�� �����Ѵ�. */
inline IDE_RC sdpsfSH::setType( sdrMtx       * aMtx,
                                sdpsfSegHdr  * aSegHdr,
                                sdpSegType     aType )
{
    UShort sType = (UShort)aType;

    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aSegHdr->mType,
                                         &sType,
                                         ID_SIZEOF(sType) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* Segment Descriptor�� ���¸� �����Ѵ�. */
inline IDE_RC sdpsfSH::setState( sdrMtx      *aMtx,
                                 sdpsfSegHdr *aSegHdr,
                                 sdpSegState  aState )
{

    UShort      sState = (UShort)aState;

    IDE_DASSERT( aMtx != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aSegHdr->mState,
                                         &sState,
                                         ID_SIZEOF(sState) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/* FreePage�� ������ ���Ѵ�. */
inline ULong sdpsfSH::getFreePageCnt( sdpsfSegHdr * aSegHdr )
{
    return sdpsfUFmtPIDList::getPageCnt( aSegHdr ) + sdpsfPvtFreePIDList::getPageCnt( aSegHdr ) ;
}

inline IDE_RC sdpsfSH::setTotExtCnt( sdrMtx       * aMtx,
                                     sdpsfSegHdr  * aSegHdr,
                                     ULong          aTotExtCnt )
{
    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&aSegHdr->mTotExtCnt,
                  &aTotExtCnt,
                  ID_SIZEOF( ULong ))
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#endif // _O_SDPSF_SH_H_

