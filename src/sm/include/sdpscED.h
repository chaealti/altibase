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
 * $Id: sdpscED.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� ExtDir Page ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_EXT_DIR_H_
# define _O_SDPSC_EXT_DIR_H_ 1

# include <sdpPhyPage.h>
# include <sdpscDef.h>

class sdpscExtDir
{
public:

    static IDE_RC getCurExtDirInfo( idvSQL            * aStatistics,
                                    scSpaceID           aSpaceID,
                                    sdpSegHandle      * aSegHandle,
                                    scPageID            aCurExtDir,
                                    sdpscExtDirInfo   * aCurExtDirInfo );

    /* ExtDir Page ���� �� �ʱ�ȭ */
    static IDE_RC createAndInitPage( idvSQL    * aStatistics,
                                     sdrMtx    * aMtx,
                                     scSpaceID   aSpaceID,
                                     scPageID    aNewExtDirPID,
                                     scPageID    aNxtExtDirPID,
                                     UShort      aMaxExtCntInExtDir,
                                     UChar    ** aPagePtr );

    /* ExtDir Page Control Header �ʱ�ȭ �� write logging */
    static IDE_RC logAndInitCntlHdr( sdrMtx               * aMtx,
                                     sdpscExtDirCntlHdr   * aCntlHdr,
                                     scPageID               aNxtExtDirPID,
                                     UShort                 aMaxExtCnt );

    /* ExtDir Page Control Header �ʱ�ȭ */
    static void  initCntlHdr( sdpscExtDirCntlHdr * aCntlHdr,
                              scPageID             aNxtExtDir,
                              scOffset             aMapOffset,
                              UShort               aMaxExtCnt );

    /* Segment Header�κ��� ������ ExtDir�� ��� */
    static IDE_RC logAndAddExtDesc( sdrMtx             * aMtx,
                                    sdpscExtDirCntlHdr * aCntlHdr,
                                    sdpscExtDesc       * aExtDesc );

    /* ���� Extent�κ��� ���ο� �������� �Ҵ��Ѵ�. */
    static IDE_RC allocNewPageInExt( idvSQL         * aStatistics,
                                     sdrMtx         * aMtx,
                                     scSpaceID        aSpaceID,
                                     sdpSegHandle   * aSegHandle,
                                     sdRID            aPrvAllocExtRID,
                                     scPageID         aFstPIDOfPrvAllocExt,
                                     scPageID         aPrvAllocPageID,
                                     sdRID          * aAllocExtRID,
                                     scPageID       * aFstPIDOfAllocExt,
                                     scPageID       * aAllocPID,
                                     sdpParentInfo  * aParentInfo );

    static IDE_RC tryAllocExtDir( idvSQL               * aStatistics,
                                  sdrMtxStartInfo      * aStartInfo,
                                  scSpaceID              aSpaceID,
                                  sdpscSegCache        * aSegCache,
                                  scPageID               aSegPID,
                                  sdpFreeExtDirType      aFreeListIdx,
                                  scPageID               aNxtExtDirPID,
                                  sdpscAllocExtDirInfo * aAllocExtDirInfo );

    static IDE_RC checkNxtExtDir4Steal( idvSQL               * aStatistics,
                                        sdrMtxStartInfo      * aStartInfo,
                                        scSpaceID              aSpaceID,
                                        scPageID               aSegHdrPID,
                                        scPageID               aNxtExtDirPID,
                                        sdpscAllocExtDirInfo * aAllocExtDirInfo );

    static IDE_RC setNxtExtDir( idvSQL          * aStatistics,
                                sdrMtx          * aMtx,
                                scSpaceID         aSpaceID,
                                scPageID          aToLstExtDir,
                                scPageID          aPageID );

    static IDE_RC makeExtDirFull( idvSQL     * aStatistics,
                                  sdrMtx     * aMtx,
                                  scSpaceID    aSpaceID,
                                  scPageID     aExtDir );
    
    /* extent map�� extslot�� ����Ѵ�. */
    static void addExtDesc( sdpscExtDirCntlHdr * aCntlHdr,
                            SShort               aLstDescIdx,
                            sdpscExtDesc       * aExtDesc );

    // Write �������� ExtDir �������� Control Header�� fix�Ѵ�.
    static IDE_RC fixAndGetCntlHdr4Write(
                      idvSQL              * aStatistics,
                      sdrMtx              * aMtx,
                      scSpaceID             aSpaceID,
                      scPageID              aExtDirPID,
                      sdpscExtDirCntlHdr ** aCntlHdr );

    // Read �������� ExtDir �������� Control Header�� fix�Ѵ�.
    static IDE_RC fixAndGetCntlHdr4Read(
                      idvSQL              * aStatistics,
                      sdrMtx              * aMtx,
                      scSpaceID             aSpaceID,
                      scPageID              aExtDirPID,
                      sdpscExtDirCntlHdr ** aCntlHdr );

    // Control Header�� �����ִ� �������� release�Ѵ�.
    static IDE_RC releaseCntlHdr( idvSQL              * aStatistics,
                                  sdpscExtDirCntlHdr  * aCntlHdr );


    /* [ INTERFACE ] ���� ExtDesc�� RID�� ��ȯ�Ѵ�. */
    static IDE_RC getNxtExtRID( idvSQL       *aStatistics,
                                scSpaceID     aSpaceID,
                                scPageID      aSegHdrPID,
                                sdRID         aCurrExtRID,
                                sdRID        *aNxtExtRID );

    /* [ INTERFACE ] Extent ������ ��ȯ�Ѵ�. */
    static IDE_RC getExtInfo( idvSQL       *aStatistics,
                              scSpaceID     aSpaceID,
                              sdRID         aExtRID,
                              sdpExtInfo   *aExtInfo );

    /* [ INTERFACE ] Sequential Scan�� ���� ������ ��ȯ */
    static IDE_RC getNxtAllocPage( idvSQL           * aStatistics,
                                   scSpaceID          aSpaceID,
                                   sdpSegInfo       * aSegInfo,
                                   sdpSegCacheInfo  * /*aSegCacheInfo*/,
                                   sdRID            * aExtRID,
                                   sdpExtInfo       * aExtInfo,
                                   scPageID         * aPageID );

    static IDE_RC markSCN4ReCycle( idvSQL        * aStatistics,
                                   scSpaceID       aSpaceID,
                                   scPageID        aSegPID,
                                   sdpSegHandle  * aSegHandle,
                                   sdRID           aFstExtRID,
                                   sdRID           aLstExtRID,
                                   smSCN         * aCommitSCN );


    /* BUG-31055 Can not reuse undo pages immediately after it is used to 
     *           aborted transaction */
    /* [ INTERFACE ] Extent���� Tablespace�� ��ȯ��.
     * Abort�� Transaction�� ����� Extent���� ��� ��Ȱ���ϱ� ����. */
    static IDE_RC shrinkExts( idvSQL            * aStatistics,
                              scSpaceID           aSpaceID,
                              scPageID            aSegPID,
                              sdpSegHandle      * aSegHandle,
                              sdrMtxStartInfo   * aStartInfo,
                              sdpFreeExtDirType   aFreeListIdx,
                              sdRID               aFstExtRID,
                              sdRID               aLstExtRID );
    /* BUG-42975 undo tablespace�� undo segment�� ���� ����ϸ� tss segment�� 
                 Ȯ���� �Ҽ��� ����. �� ��� undo segment���� steal�� �ؾ� �ϸ�
                 steal�� extCnt �� ExtDir���� �����ϴ� ExtCnt���� ũ�� ū��� ExtDir�� shrink �Ѵ�.*/ 
    static IDE_RC shrinkExtDir( idvSQL                * aStatistics,
                                sdrMtx                * aMtx,
                                scSpaceID               aSpaceID,
                                sdpSegHandle          * aToSegHandle,
                                sdpscAllocExtDirInfo  * aAllocExtDirInfo );
  
    static IDE_RC setSCNAtAlloc( idvSQL        * aStatistics,
                                 scSpaceID       aSpaceID,
                                 sdRID           aExtRID,
                                 smSCN         * aTransBSCN );

    /* ExtDir �������� map ptr�� ��ȯ�Ѵ�. */
    static inline sdpscExtDirMap * getMapPtr( sdpscExtDirCntlHdr * aCntlHdr );

    static inline SShort calcOffset2DescIdx(
                                       sdpscExtDirCntlHdr * aCntlHdr,
                                       scOffset             aOffset );

    static inline scOffset calcDescIdx2Offset(
                                       sdpscExtDirCntlHdr * aCntlHdr,
                                       SShort               aExtDescIdx );

    /* ExtDir Control Header�� Ptr ��ȯ */
    static inline sdpscExtDirCntlHdr * getHdrPtr( UChar   * aPagePtr );

    /* ExtDir ������������ ������ ExtDesc ���� ��ȯ */
    static inline UShort  getFreeDescCnt( sdpscExtDirCntlHdr * aCntlHdr );

    static void setLatestCSCN( sdpscExtDirCntlHdr * aCntlHdr,
                               smSCN              * aCommitSCN );

    static void setFstDskViewSCN( sdpscExtDirCntlHdr * aCntlHdr,
                                  smSCN              * aMyFstDskViewSCN );

    static inline sdpscExtDesc *
                   getExtDescByIdx( sdpscExtDirCntlHdr * aExtDirCntlHdr,
                                    UShort               aExtDescIdx );

    static IDE_RC setNxtExtDir( sdrMtx             * aMtx,
                                sdpscExtDirCntlHdr * aExtDirCntlHdr,
                                scPageID             aNxtExtDir );

    static inline void initExtDesc( sdpscExtDesc  * aExtDesc );

private:

    /* ExtDir�� ExtDesc�� ����Ѵ�. */
    static void addExtDescToMap( sdpscExtDirMap  * aMapPtr,
                                 SShort            aLstIdx,
                                 sdpscExtDesc    * aExtDesc );

    /* Extent���� �������� �Ҵ��� �� �ִ��� ���θ� ��ȯ�Ѵ�. */
    static inline idBool isFreePIDInExt( UInt         aPageCntInExt,
                                         scPageID     aFstPIDOfAllocExt,
                                         scPageID     aLstAllocPageID );

    /* ���� ExtDesc�� ��ġ�� ��ȯ�Ѵ�. */
    static inline void getNxtExt( sdpscExtDirCntlHdr  * aExtDirCntlHdr,
                                  sdRID                 aCurExtRID,
                                  sdRID               * aNxtExtRID,
                                  sdpscExtDesc        * aNxtExtDesc );

    static IDE_RC checkExtDirState4Reuse(
                                    idvSQL           * aStatistics,
                                    scSpaceID          aSpaceID,
                                    scPageID           aSegPID,
                                    scPageID           aCurExtDir,
                                    smSCN            * aMyFstDskViewSCN,
                                    smSCN            * aSysMinDskViewSCN,
                                    sdpscExtDirState * aExtDirState,
                                    sdRID            * aAllocExtRID,
                                    sdpscExtDesc     * aExtDesc,
                                    scPageID         * aNxtExtDir,
                                    UInt             * aExtCntInExtDir );

    static IDE_RC getNxtExt4Alloc( idvSQL       * aStatistics,
                                   scSpaceID      aSpaceID,
                                   sdRID          aCurExtRID,
                                   sdRID        * aNxtExtRID,
                                   scPageID     * aFstPIDOfExt,
                                   scPageID     * aFstDataPIDOfNxtExt );

    static IDE_RC getExtDescInfo( idvSQL        * aStatistics,
                                  scSpaceID       aSpaceID,
                                  scPageID        aExtDirPID,
                                  UShort          aIdx,
                                  sdRID         * aExtRID,
                                  sdpscExtDesc  * aExtDescPtr );

    static inline idBool isExeedShrinkThreshold( sdpscSegCache * aSegCache );

    static inline void initExtDirInfo( sdpscExtDirInfo  * aExtDirInfo );
    static inline void initAllocExtDirInfo(
                                       sdpscAllocExtDirInfo  * aAllocExtDirInfo );
};

inline idBool sdpscExtDir::isExeedShrinkThreshold( sdpscSegCache * aSegCache )
{
    // BUG-27329 CodeSonar::Uninitialized Variable (2)
    UInt sSizeThreshold = ID_UINT_MAX;

    switch( aSegCache->mCommon.mSegType )
    {
        case SDP_SEG_TYPE_TSS:
            sSizeThreshold = smuProperty::getTSSegSizeShrinkThreshold();
            break;
        case SDP_SEG_TYPE_UNDO:
            sSizeThreshold = smuProperty::getUDSegSizeShrinkThreshold();
            break;
            // BUG-27329 CodeSonar::Uninitialized Variable (2)
            // SDP_SEG_TYPE_TSS, SDP_SEG_TYPE_UNDO �̿��� ���� �� �� ����.
            IDE_ASSERT(0);
        default:
            break;
    }

    if ( aSegCache->mCommon.mSegSizeByBytes >= sSizeThreshold )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

/* ������ ExtDir���� ���� ExtDesc�� ��ġ�� ��ȯ�Ѵ�. */
inline void sdpscExtDir::getNxtExt( sdpscExtDirCntlHdr  * aExtDirCntlHdr,
                                    sdRID                 aCurExtRID,
                                    sdRID               * aNxtExtRID,
                                    sdpscExtDesc        * aNxtExtDesc )
{
    UShort              sIdx;
    sdpscExtDesc      * sNxtExtDescPtr;
    sdRID               sNxtExtRID;

    IDE_ASSERT( aExtDirCntlHdr != NULL );
    IDE_ASSERT( aNxtExtRID     != NULL );
    IDE_ASSERT( aNxtExtDesc    != NULL );

    sIdx = calcOffset2DescIdx( aExtDirCntlHdr,
                               SD_MAKE_OFFSET( aCurExtRID ));

    if ( sIdx == (aExtDirCntlHdr->mExtCnt - 1))
    {
        // ������ Desc�̹Ƿ� ���� ExtDir ������������
        // ���� ExtDesc�� �������� �ʴ´�.
        sNxtExtRID   = SD_NULL_RID;
    }
    else
    {
        sNxtExtDescPtr = getExtDescByIdx( aExtDirCntlHdr, sIdx+1 );
        sNxtExtRID     = sdpPhyPage::getRIDFromPtr( sNxtExtDescPtr );

        *aNxtExtDesc   = *sNxtExtDescPtr; // copy ���ش�.
    }

    *aNxtExtRID  = sNxtExtRID;

    return;
}


/* ExtDir ���������� ExtDesc�� index�� ����Ѵ�. */
inline SShort sdpscExtDir::calcOffset2DescIdx( sdpscExtDirCntlHdr * aCntlHdr,
                                               scOffset             aOffset )
{
    UShort  sMapOffset;

    if ( aCntlHdr == NULL )
    {
        sMapOffset = sdpPhyPage::getDataStartOffset( ID_SIZEOF(sdpscExtDirCntlHdr) );
    }
    else
    {
        sMapOffset = aCntlHdr->mMapOffset;
    }
    return (SShort)((aOffset - sMapOffset) / ID_SIZEOF(sdpscExtDesc));
}

/* ExtDir ���������� ExtDesc�� Offset�� ����Ѵ�. */
inline scOffset sdpscExtDir::calcDescIdx2Offset( sdpscExtDirCntlHdr * aCntlHdr,
                                                 SShort               aExtDescIdx )
{
    UShort  sMapOffset;

    if ( aCntlHdr == NULL )
    {
        sMapOffset = sdpPhyPage::getDataStartOffset( ID_SIZEOF(sdpscExtDirCntlHdr) );
    }
    else
    {
        sMapOffset = aCntlHdr->mMapOffset;
    }

    return (scOffset)(sMapOffset + ( aExtDescIdx * ID_SIZEOF(sdpscExtDesc)));
}

/* ExtDir �������� map ptr�� ��ȯ�Ѵ�. */
inline sdpscExtDirMap * sdpscExtDir::getMapPtr( sdpscExtDirCntlHdr * aCntlHdr )
{
    IDE_ASSERT( aCntlHdr->mMapOffset != 0 );

    return (sdpscExtDirMap*)
        (sdpPhyPage::getPageStartPtr((UChar*)aCntlHdr) +
          aCntlHdr->mMapOffset);
}

/* ExtDir Control Header�� Ptr ��ȯ */
inline sdpscExtDirCntlHdr * sdpscExtDir::getHdrPtr( UChar   * aPagePtr )
{
    return (sdpscExtDirCntlHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPagePtr );
}

/* ExtDir ������������ ������ ExtDesc ���� ��ȯ */
inline UShort  sdpscExtDir::getFreeDescCnt( sdpscExtDirCntlHdr * aCntlHdr )
{
    IDE_ASSERT( aCntlHdr->mMaxExtCnt >= aCntlHdr->mExtCnt );
    return (UShort)( aCntlHdr->mMaxExtCnt - aCntlHdr->mExtCnt );
}

/* aPageID�� ���� Extent�� aPageID���ķ� Free Page�� �ִ��� �����Ѵ�. */
inline idBool sdpscExtDir::isFreePIDInExt( 
                                 UInt         aPageCntInExt,
                                 scPageID     aFstPIDOfAllocExt,
                                 scPageID     aLstAllocPageID )
{
    UInt sAllocPageCntInExt = aLstAllocPageID - aFstPIDOfAllocExt + 1;

    IDE_ASSERT( aPageCntInExt >= SDP_MIN_EXTENT_PAGE_CNT );

    if( sAllocPageCntInExt < aPageCntInExt )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/* Extent Dir. Map���� aExtDescIdx��° ExtDesc�� ��ȯ�Ѵ�. */
inline sdpscExtDesc * sdpscExtDir::getExtDescByIdx(
                                   sdpscExtDirCntlHdr * aExtDirCntlHdr,
                                   UShort               aExtDescIdx )
{
    sdpscExtDirMap * sExtMapPtr = getMapPtr( aExtDirCntlHdr );

    return &(sExtMapPtr->mExtDesc[ aExtDescIdx ] );
}


/***********************************************************************
 *
 * Description : Extent Desc �ʱ�ȭ
 *
 * aExtDesc  - [OUT] Extent Desc.������
 *
 ***********************************************************************/
inline void sdpscExtDir::initExtDesc( sdpscExtDesc  * aExtDesc )
{
    aExtDesc->mExtFstPID     = SD_NULL_PID;
    aExtDesc->mLength        = 0;
    aExtDesc->mExtFstDataPID = SD_NULL_PID;
}

/***********************************************************************
 *
 * Description : ExtDir ���� �ڷᱸ��
 *
 * aExtDirInfo - [OUT] ExtDirInfo �ڷᱸ�� ������
 *
 ***********************************************************************/
inline void sdpscExtDir::initExtDirInfo( sdpscExtDirInfo  * aExtDirInfo )
{
    IDE_ASSERT( aExtDirInfo != NULL );

    aExtDirInfo->mExtDirPID    = SD_NULL_PID;
    aExtDirInfo->mNxtExtDirPID = SD_NULL_PID;
    aExtDirInfo->mIsFull       = ID_FALSE;
    aExtDirInfo->mTotExtCnt    = 0;
    aExtDirInfo->mMaxExtCnt    = 0;
}

/***********************************************************************
 *
 * Description : ���׸�Ʈ ���� Ȥ�� Ȯ�� ����� �ʿ��� ������ �����ϴ� �ڷᱸ�� �ʱ�ȭ
 *
 * aAllocExtInfo - [IN] Extent Ȯ������ ������
 *
 ***********************************************************************/
inline void sdpscExtDir::initAllocExtDirInfo( sdpscAllocExtDirInfo  * aAllocExtDirInfo )
{
    IDE_ASSERT( aAllocExtDirInfo != NULL );

    aAllocExtDirInfo->mIsAllocNewExtDir  = ID_FALSE;
    aAllocExtDirInfo->mNewExtDirPID      = SD_NULL_PID;
    aAllocExtDirInfo->mNxtPIDOfNewExtDir = SD_NULL_PID;
    aAllocExtDirInfo->mShrinkExtDirPID   = SD_NULL_PID;

    initExtDesc( &aAllocExtDirInfo->mFstExtDesc );

    aAllocExtDirInfo->mTotExtCntOfSeg    = 0;
    aAllocExtDirInfo->mFstExtDescRID     = SD_NULL_RID;

    aAllocExtDirInfo->mExtCntInShrinkExtDir = 0;
}

#endif // _O_SDPSC_EXT_DIR_H_
