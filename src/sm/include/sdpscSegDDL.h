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
 * $Id: sdpscSegDDL.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� Create, Extend �������
 * ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_SEG_DDL_H_
# define _O_SDPSC_SEG_DDL_H_ 1

# include <sdpDef.h>
# include <sdpscDef.h>

# include <sdpscED.h>

class sdpscSegDDL
{
public:

    /* [ INTERFACE ] Segment �Ҵ� */
    static IDE_RC createSegment( idvSQL                * aStatistics,
                                 sdrMtx                * aMtx,
                                 scSpaceID               aSpaceID,
                                 sdpSegType              aSegType,
                                 sdpSegHandle          * aSegmentHandle );

    /* Segment�� 1���̻��� Extent�� �Ҵ��Ѵ�. */
    static IDE_RC allocNewExts( idvSQL           * aStatistics,
                                sdrMtxStartInfo  * aStartInfo,
                                scSpaceID          aSpaceID,
                                sdpSegHandle     * aSegHandle,
                                scPageID           aCurExtDir,
                                sdpFreeExtDirType  aFreeListIdx,
                                sdRID            * aAllocExtRID,
                                scPageID         * aFstPIDOfExt,
                                scPageID         * aFstDataPIDOfExt );

    /* [INTERFACE] From ���׸�Ʈ���� To ���׸�Ʈ�� Extent Dir�� �ű��.
       Extent Dir�̵��� from�� to�� extent ���� Ʋ���� �������� freelist�� ������.  */
    static IDE_RC tryStealExts( idvSQL           * aStatistics,
                                sdrMtxStartInfo  * aStartInfo,
                                scSpaceID          aSpaceID,
                                sdpSegHandle     * aFrSegHandle,
                                scPageID           aFrSegPID,
                                scPageID           aFrCurExtDir,
                                sdpSegHandle     * aToSegHandle,
                                scPageID           aToSegPID,
                                scPageID           aToCurExtDir,
                                idBool           * aTrySuccess );

private:

    /* Segment�� �Ҵ��Ѵ� */
    static IDE_RC allocateSegment( idvSQL       * aStatistics,
                                   sdrMtx       * aMtx,
                                   scSpaceID      aSpaceID,
                                   sdpSegHandle * aSegHandle,
                                   sdpSegType     aSegType );

    /* ���ο� �ϳ��� Extent�� Segment�� �Ҵ��ϴ� ������ �Ϸ� */
    static IDE_RC addAllocExtDesc( idvSQL             * aStatistics,
                                   sdrMtx             * aMtx,
                                   scSpaceID            aSpaceID,
                                   scPageID             aSegPID,
                                   sdpscExtDirInfo    * aCurExtDirInfo,
                                   sdRID              * aAllocExtRID,
                                   sdpscExtDesc       * aExtDesc,
                                   UInt               * aTotExtDescCnt );

    /* ���ο� �ϳ��� Extent Dir.�� Segment�� �Ҵ��ϴ� ������ �Ϸ� */
    static IDE_RC addOrShrinkAllocExtDir( idvSQL                * aStatistics,
                                          sdrMtx                * aMtx,
                                          scSpaceID               aSpaceID,
                                          scPageID                aSegPID,
                                          sdpFreeExtDirType       aFreeListIdx,
                                          sdpscExtDirInfo       * aCurExtDirInfo,
                                          sdpscAllocExtDirInfo  * aAllocExtInfo );
};



#endif // _O_SDPSC_SEG_DDL_H_
