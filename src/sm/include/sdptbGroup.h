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
 * $Id: sdptbGroup.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * Bitmap based TBS���� Global Group( Space Header) �� Local Group�� 
 * �����ϱ� ���� �Լ����̴�.
 ***********************************************************************/

# ifndef _O_SDPTB_GROUP_H_
# define _O_SDPTB_GROUP_H_ 1

class sdptbGroup
{
public:
    static IDE_RC initialize(){ return IDE_SUCCESS; }
    static IDE_RC destroy(){ return IDE_SUCCESS; }

    /* ���̺����̽� ��忡 Space Cache�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.*/
    static IDE_RC allocAndInitSpaceCache( scSpaceID         aSpaceID,
                                          smiExtMgmtType    aExtMgmtType,
                                          smiSegMgmtType    aSegMgmtType,
                                          UInt              aExtPageCount );

    static IDE_RC refineCache( scSpaceID  aSpaceID );

    /* ���̺����̽� ��忡�� Space Cache�� �޸� �����Ѵ�.*/
    static IDE_RC destroySpaceCache( sctTableSpaceNode * aSpaceNode );

    /* GG �� LG header�� �����Ѵ�.*/
    static IDE_RC makeMetaHeaders( idvSQL           *aStatistics,
                                   sdrMtxStartInfo  *aStartInfo,
                                   UInt              aSpaceID,
                                   sdptbSpaceCache  *aCache,
                                   smiDataFileAttr **aFileAttr,
                                   UInt              aFileAttrCount );

    static IDE_RC makeMetaHeadersForAutoExtend( 
                                         idvSQL           *aStatistics,
                                         sdrMtxStartInfo  *aStartInfo,
                                         UInt              aSpaceID,
                                         sdptbSpaceCache  *aCache,
                                         UInt              aNeededPageCnt);

    static IDE_RC makeNewLGHdrs( idvSQL           *aStatistics,
                                 sdrMtx          * aMtx,
                                 UInt              aSpaceID,
                                 sdptbGGHdr      * aGGHdrPtr,
                                 UInt              aStartLGID,
                                 UInt              aLGCntOfGG,
                                 sdFileID          aFID,
                                 UInt             aPagesPerExt,
                                 UInt             aPageCntOfGG );

    static IDE_RC resizeGGCore( idvSQL            * aStatistics,
                                sdrMtx            * aMtx,
                                scSpaceID           aSpaceID,
                                sdptbGGHdr        * sGGHdr,
                                UInt                aNewPageCnt );

    static IDE_RC resizeGG( idvSQL             * aStatistics,
                            sdrMtx             * aMtx,
                            scSpaceID            aSpaceID,
                            UInt                 aGGID,
                            UInt                 aNewPageCnt );

    /* LG ��� �ϳ��� �����Ѵ� */
    static IDE_RC resizeLGHdr( idvSQL     * aStatistics,
                               sdrMtx     * aMtx,
                               scSpaceID    aSpaceID,
                               ULong        aValidBitsNew, //������ Ext Cnt
                               scPageID     aAllocLGPID,              
                               scPageID     aDeallocLGPID,
                               UInt       * aFreeInLG );

    /*������ ũ�⸦ page������ �޾Ƽ� ��� LG�� ������ִ��� ����Ѵ� */
    static UInt getNGroups( ULong            aSize,
                            sdptbSpaceCache *aCache, 
                            idBool          *aHasExtraLG);


    /* GG��������� �α�ó���� �Ѵ�. */
    static IDE_RC logAndInitGGHdrPage( sdrMtx                * aMtx,
                                       UInt                    aSpaceID,
                                       sdptbGGHdr            * aGGHdrPtr,
                                       sdptbGGID               aGGID,
                                       UInt                    aPagesPerExt,
                                       UInt                    aLGCnt,
                                       UInt                    aPageCnt,
                                       idBool                  aIsExtraLG);

    /* LG��������� �α�ó���� �Ѵ�. */
    static IDE_RC logAndInitLGHdrPage( idvSQL          *  aStatistics,
                                       sdrMtx          *  aMtx,
                                       UInt               aSpaceID,
                                       sdptbGGHdr      *  aGGHdrPtr,
                                       sdFileID           aFID,
                                       UInt               aLGID,
                                       sdptbLGType        aInitLGType,
                                       UInt               aValidBits,
                                       UInt               aPagesPerExt);


    static IDE_RC doRefineSpaceCacheCore( sddTableSpaceNode * aSpaceNode );

    /*
     * LG�� ������ �� �ִ� ��Ʈ�� ����.
     * �̰��� �ϳ��� LG���� ������ �� �ִ� extent�� ������ �ǹ��ϱ⵵ �Ѵ�.
     */ 

    static inline UInt nBitsPerLG(void)
    {
        UInt sBits; 
        sBits = smuProperty::getExtCntPerExtentGroup();

        if ( sBits == 0 )
        {
            sBits = (sdpPhyPage::getEmptyPageFreeSize() -
                                    idlOS::align8((UInt)ID_SIZEOF(sdptbLGHdr)))
                    * SDPTB_BITS_PER_BYTE;
        }
        else
        {
            /* nothing to do */
        }

        return sBits;
    }

    /* LG ���� �����Ǵ� ��Ʈ���� ����(byte) */
    static inline UInt getLenBitmapOfLG()
    {
        return sdptbGroup::nBitsPerLG() / SDPTB_BITS_PER_BYTE;
    }

    static inline IDE_RC getTotalPageCount( idvSQL      * aStatistics,
                                            scSpaceID     aSpaceID,
                                            ULong       * aTotalPageCount )
    {
        return  sddDiskMgr::getTotalPageCountOfTBS( aStatistics,
                                                    aSpaceID,
                                                    aTotalPageCount );
    }


    static IDE_RC getAllocPageCount( idvSQL            * aStatistics,
                                     sddTableSpaceNode * aSpaceNode,
                                     ULong             * aAllocPageCount );

    /////////////////////////////////////////////////////////////
    //     GG       Logging ���� �Լ���.
    /////////////////////////////////////////////////////////////
    static IDE_RC logAndSetHWMOfGG( sdrMtx       * aMtx,
                                    sdptbGGHdr   * aGGHdr,
                                    UInt           aHWM );

    static IDE_RC logAndSetLGCntOfGG( sdrMtx     * aMtx,
                                      sdptbGGHdr * aGGHdr,
                                      UInt         aGroups );

    static IDE_RC logAndSetPagesOfGG( sdrMtx     * aMtx,
                                      sdptbGGHdr * aGGHdr,
                                      UInt         aPages );

    static IDE_RC logAndSetLGTypeOfGG( sdrMtx     * aMtx,
                                       sdptbGGHdr * aGGHdr,
                                       UInt         aLGType );

    static IDE_RC logAndModifyFreeExtsOfGG( sdrMtx     * aMtx,
                                            sdptbGGHdr * aGGHdr,
                                            SInt         aVal );

    static IDE_RC logAndModifyFreeExtsOfGGByLGType( sdrMtx         * aMtx,
                                                    sdptbGGHdr     * aGGHdr,
                                                    UInt             aLGType,
                                                    SInt             aVal );

    static IDE_RC logAndSetLowBitsOfGG( sdrMtx     * aMtx,
                                        sdptbGGHdr * aGGHdr,
                                        ULong        aBits );

    static IDE_RC logAndSetHighBitsOfGG(  sdrMtx     * aMtx,
                                          sdptbGGHdr * aGGHdr,
                                          ULong        aBits );

    static IDE_RC logAndSetLGFNBitsOfGG( sdrMtx      * aMtx,
                                         sdptbGGHdr  * aGGHdr,
                                         ULong         aBitIdx,
                                         UInt          aVal );

    /////////////////////////////////////////////////////////////
    //     LG       Logging ���� �Լ���.
    /////////////////////////////////////////////////////////////

    static IDE_RC logAndSetStartPIDOfLG( sdrMtx       * aMtx,
                                         sdptbLGHdr   * aLGHdr,
                                         UInt           aStartPID );

    static IDE_RC logAndSetHintOfLG( sdrMtx           * aMtx,
                                     sdptbLGHdr       * aLGHdr,
                                     UInt               aHint );

    static IDE_RC logAndSetValidBitsOfLG( sdrMtx      * aMtx,
                                          sdptbLGHdr  * aLGHdr,
                                          UInt          aValidBits );

    static IDE_RC logAndSetFreeOfLG( sdrMtx           * aMtx,
                                     sdptbLGHdr       * aLGHdr,
                                     UInt               aFree );

    //////////////////////////////////////////////////////////////
    //  redo routine 
    //////////////////////////////////////////////////////////////
    /* BUG-46036 codesonar warning ����
    static IDE_RC initGG( sdrMtx      * aMtx,
                          UChar       * aPagePtr );
    */

    /* dealocation LG header page �� ���� ��Ʈ���� ��� 1�� �ʱ�ȭ�Ѵ�.*/
    static void initBitmapOfLG( UChar      * aPagePtr,
                                UChar        aBitVal,
                                UInt         aStartIdx,
                                UInt         aCount );

     /*
      * misc
      */

     static sdptbLGHdr *getLGHdr( UChar * aPagePtr)
     {
        return (sdptbLGHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPagePtr );
     }

     static sdptbGGHdr *getGGHdr( UChar * aPagePtr)
     {
        return (sdptbGGHdr*)sdpPhyPage::getLogicalHdrStartPtr( aPagePtr );
     }

    

    static IDE_RC prepareExtendFileOrWait( idvSQL          * aStatistics,
                                           sdptbSpaceCache * aCache,
                                           idBool          * aDoExtend );

    static IDE_RC completeExtendFileAndWakeUp( idvSQL       * aStatistics,
                                               sdptbSpaceCache * aCache );

    /* BUG-31608 [sm-disk-page] add datafile during DML
     * �Ʒ� �� �Լ��� AddDataFile�� ���ü� ó���� ����Ѵ�. ����� 
     * prepare/complete ExtendFile�Լ��� ���Ҿ� Critical-section�� 
     * �����Ѵ�. */
    static void prepareAddDataFile( idvSQL          * aStatistics,
                                    sdptbSpaceCache * aCache );

    static void completeAddDataFile( sdptbSpaceCache * aCache );


    static inline UInt getAllocLGHdrPID( sdptbGGHdr   * aGGHdr,
                                         UInt           aLGID)
    {
        IDE_ASSERT( aGGHdr != NULL);
        IDE_ASSERT( aLGID < SDPTB_LG_CNT_MAX);
        return SDPTB_LG_HDR_PID_FROM_LGID( aGGHdr->mGGID, 
                                           aLGID,
                                           getAllocLGIdx(aGGHdr),
                                           aGGHdr->mPagesPerExt);
        
    }

    static inline UInt getDeallocLGHdrPID( sdptbGGHdr   * aGGHdr,
                                           UInt           aLGID)
    {

        IDE_ASSERT( aGGHdr != NULL);
        IDE_ASSERT( aLGID < SDPTB_LG_CNT_MAX);

        return SDPTB_LG_HDR_PID_FROM_LGID( aGGHdr->mGGID, 
                                           aLGID,
                                           getDeallocLGIdx(aGGHdr),
                                           aGGHdr->mPagesPerExt);
        
    }


    /* alloc LG�� index�� ��´� */
    static inline UInt getAllocLGIdx(sdptbGGHdr   * aGGHdr)
    {
        IDE_ASSERT( aGGHdr != NULL);

        return aGGHdr->mAllocLGIdx ;
    }

    /* dealloc LG�� index�� ��´� */
    static inline UInt getDeallocLGIdx(sdptbGGHdr   * aGGHdr)
    {
        IDE_ASSERT( aGGHdr != NULL);

         /* !(aGGHdr->mAllocLGIdx)
          * �� �ڵ忡�� �ϴ� ������ �ο������ϸ� ������ ����.
          *
          *(aGGHdr->mAllocLGIdx == SDPTB_ALLOC_LG_IDX_0 ) ? 
          *                     SDPTB_ALLOC_LG_IDX_1 : SDPTB_ALLOC_LG_IDX_0 ;
          */
        return !(aGGHdr->mAllocLGIdx);
    }

    static ULong getCachedFreeExtCount( sddTableSpaceNode* aSpaceNode );

private:


    // ���� ���� Ȯ�� ���� ��ȯ
    static inline idBool isOnExtend( sdptbSpaceCache * aCache );

    //Extend�� ���� Mutex ȹ��
    static inline void  lockForExtend( idvSQL           * aStatistics,
                                       sdptbSpaceCache  * aCache );

    //Extend�� ���� Mutex ����
    static inline void  unlockForExtend( sdptbSpaceCache  * aCache );

    //addDataFile�� ���� Mutex ȹ��
    static inline void  lockForAddDataFile( idvSQL           * aStatistics,
                                            sdptbSpaceCache  * aCache );

    //addDataFile�� ���� Mutex ����
    static inline void  unlockForAddDataFile( sdptbSpaceCache  * aCache );

    /* 
     * ������ LG�� extent������ ���Ѵ�. 
     * ���⼭ aPageCnt�� GG header�� ������ ��� ũ����
     */

    static inline UInt  getExtCntOfLastLG( UInt aPageCnt, 
                                           UInt aPagesPerExt)
    {
        UInt sExtCnt;
        UInt sRestPageCnt;

        //���� LG�� ������ ������ ������ ����
        sRestPageCnt = (aPageCnt - SDPTB_GG_HDR_PAGE_CNT) 
                              % SDPTB_PAGES_PER_LG(aPagesPerExt);

        // �� ¥������... ��¥�� �����ϴ� LG�ΰ� �ƴϸ�...
        // illusion�ΰ� Ȯ���ؾ���!!
        if ( sRestPageCnt >= (SDPTB_LG_HDR_PAGE_CNT + aPagesPerExt) )
        {
            sExtCnt = ( sRestPageCnt - SDPTB_LG_HDR_PAGE_CNT ) / aPagesPerExt;
        }
        else
        {
            //illusion !
            sExtCnt = sdptbGroup::nBitsPerLG();
        }

        return sExtCnt;

    }

    /* page������ ���ڷ� �޾Ƽ� �� ũ�⿡���� extent ������ ���Ѵ�. */
    static inline UInt    getExtentCntByPageCnt( sdptbSpaceCache  *aCache,
                                                 UInt              aPageCnt )
    {
        UInt    sPagesPerExt;
        idBool  sIsExtraLG;
        UInt    sLGHdrCnt;
        UInt    sExtCnt;

        sPagesPerExt = aCache->mCommon.mPagesPerExt;
        sIsExtraLG = ID_FALSE;
        sLGHdrCnt = getNGroups( aPageCnt , aCache, &sIsExtraLG );

        IDE_ASSERT( aCache != NULL );

        if ( sIsExtraLG == ID_TRUE )
        {
            sExtCnt = ( (sLGHdrCnt - 1) * sdptbGroup::nBitsPerLG() ) + 
                      getExtCntOfLastLG(aPageCnt, sPagesPerExt ); 
        }
        else
        {
            sExtCnt = sLGHdrCnt * sdptbGroup::nBitsPerLG() ;
        }

        return sExtCnt;
    }

};

inline idBool sdptbGroup::isOnExtend( sdptbSpaceCache * aCache )
{
    return aCache->mOnExtend;
}


    // Extend Extent Mutex ȹ��
inline void  sdptbGroup::lockForExtend( idvSQL           * aStatistics,
                                        sdptbSpaceCache  * aCache )
{
    IDE_ASSERT( aCache->mMutexForExtend.lock( aStatistics ) == IDE_SUCCESS );
    return;
}

    // Extend Extent Mutex ����
inline void  sdptbGroup::unlockForExtend( sdptbSpaceCache  * aCache )
{
    IDE_ASSERT( aCache->mMutexForExtend.unlock() == IDE_SUCCESS );
    return;
}

/* BUG-31608 [sm-disk-page] add datafile during DML 
 * Add Datafile Mutex ȹ�� */
inline void  sdptbGroup::lockForAddDataFile ( idvSQL           * aStatistics,
                                              sdptbSpaceCache  * aCache )
{
    IDE_ASSERT( aCache->mMutexForAddDataFile.lock( aStatistics ) 
                == IDE_SUCCESS );
    return;
}

/* BUG-31608 [sm-disk-page] add datafile during DML 
 * Add Datafile Mutex ���� */
inline void  sdptbGroup::unlockForAddDataFile( sdptbSpaceCache  * aCache )
{
    IDE_ASSERT( aCache->mMutexForAddDataFile.unlock() == IDE_SUCCESS );
    return;
}

#endif // _O_SDPTB_GROUP_H_
