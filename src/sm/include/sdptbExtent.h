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
 * $Id: sdptbExtent.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * TBS���� extent�� �Ҵ��ϰ� �����ϴ� ��ƾ�� ���õ� �Լ����̴�.
 ***********************************************************************/

# ifndef _O_SDPTB_EXTENT_H_
# define _O_SDPTB_EXTENT_H_ 1

#include <sdptb.h>
#include <sdpst.h>

class sdptbExtent {
public:
    static IDE_RC initialize(){ return IDE_SUCCESS; }
    static IDE_RC destroy(){ return IDE_SUCCESS; }

    /* tablespace�κ��� extent�� �Ҵ�޴´�.*/
    static IDE_RC allocExts( idvSQL         * aStatistics,
                             sdrMtxStartInfo* aStartInfo,
                             scSpaceID        aSpaceID,
                             UInt             aOrgNrExts,
                             sdpExtDesc     * aExtDesc );

    static IDE_RC allocTmpExt( idvSQL      * aStatistics,
                               scSpaceID     aSpaceID,
                               sdpExtDesc  * aExtSlot );

    /* ExtDir �������� �Ҵ��Ѵ�. */
    static IDE_RC tryAllocExtDir( idvSQL             * aStatistics,
                                  sdrMtxStartInfo    * aStartInfo,
                                  scSpaceID            aSpaceID,
                                  sdpFreeExtDirType    aFreeListIdx,
                                  scPageID           * aExtDirPID );

    /* ExtDir �������� �����Ѵ�. */
    static IDE_RC freeExtDir( idvSQL             * aStatistics,
                              sdrMtx             * aMtx,
                              scSpaceID            aSpaceID,
                              sdpFreeExtDirType    aFreeListIdx,
                              scPageID             aExtDirPID );

    /* GG���� extent���� �Ҵ��� �õ��Ѵ� */
    static IDE_RC tryAllocExtsInGG( idvSQL             * aStatistics,
                                    sdrMtxStartInfo    * aStartInfo,
                                    sdptbSpaceCache    * aCache,  
                                    sdFileID             aFID,
                                    UInt                 aOrgNrExts,
                                    scPageID           * aExtFstPID,
                                    UInt               * aNrDone); 

    /* LG���� extent���� �Ҵ��� �õ��Ѵ� */
    static IDE_RC allocExtsInLG( idvSQL                  * aStatistics,
                                 sdrMtx                  * aMtx,
                                 sdptbSpaceCache         * aSpaceCache,  
                                 sdptbGGHdr              * aGGPtr,
                                 scPageID                  aLGHdrPID,
                                 UInt                      aOrgNrExts, 
                                 scPageID                * aExtPID,
                                 UInt                    * aNrDone, 
                                 UInt                    * aFreeInLG);

    /* space cache�κ��� ���� �Ҵ��� ������ ���ɼ��� �ִ� FID�� ��´�*/
    static IDE_RC getAvailFID( sdptbSpaceCache         * aCache,
                           smFileID                 * aFID);

    /* deallcation LG hdr�� free�� �ִٸ� switching�� �Ѵ�.*/
    static IDE_RC trySwitch( sdrMtx                    *   aMtx,
                             sdptbGGHdr                *   aGGHdrPtr,
                             idBool                    *   aRet,
                             sdptbSpaceCache           *   aCache );

    static IDE_RC prepareCachedFreeExts( idvSQL           * aStatistics,
                                         sctTableSpaceNode* aSpaceNode );

    static IDE_RC freeExt( idvSQL           *  aStatistics,
                           sdrMtx           *  aMtx,
                           scSpaceID           aSpaceID,
                           scPageID            aExtFstPID,
                           UInt             *  aNrDone );
    static IDE_RC freeTmpExt( scSpaceID           aSpaceID,
                              scPageID            aExtFstPID );

    /*[INTERFACE]  TBS�� extent�� �ݳ��Ѵ�. */
    /* LG�� extent���� �ݳ��Ѵ�.*/
    static IDE_RC freeExts( idvSQL           *  aStatistics,
                            sdrMtx           *  aMtx,
                            scSpaceID           aSpaceID,
                            ULong            *  aExtFstPIDArr,
                            UInt               aArrCount );

    static IDE_RC freeExtsInLG( idvSQL           *  aStatistics,
                                sdrMtx           *  aMtx,
                                scSpaceID           aSpaceID,
                                sdptbSortExtSlot *  aSortedExts,
                                UInt                aNrSortedExts,
                                UInt                aBeginIndex,
                                UInt             *  aEndIndex,
                                UInt             *  aNrDone);

    static IDE_RC autoExtDatafileOnDemand( idvSQL          *   aStatistics,
                                           UInt                aSpaceID,
                                           sdptbSpaceCache *   aCache,
                                           void            *   aTransForMtx,
                                           UInt                aNeededPageCnt);

    static void allocByBitmapIndex( sdptbLGHdr * aLGHdr,
                                    UInt          aIndex);

    static void freeByBitmapIndex( sdptbLGHdr *  aLGHdr,
                                   UInt          aIndex );

    static IDE_RC isFreeExtPage( idvSQL   * aStatistics,
                                 scSpaceID  aSpaceID,
                                 scPageID   aPageID,
                                 idBool   * aIsFreeExt);

    /* BUG-47666 X$DATAFILE�� Freeness Of GG ǥ�� */
    static idBool getFreenessOfGG( sddTableSpaceNode  * aTBSNode,
                                   sdFileID             aFID )
    {
        sdptbSpaceCache * sSpaceCache = sddDiskMgr::getSpaceCache( aTBSNode );

        if ( aFID <= sSpaceCache->mMaxGGID )
        {
            return sdptbBit::getBit( (void *)sSpaceCache->mFreenessOfGGs, aFID );
        }

        return ID_FALSE;
    };
};

#endif // _O_SDPTB_EXTENT_H_
