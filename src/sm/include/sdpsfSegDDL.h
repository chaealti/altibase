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
 * $Id: sdpsfSegDDL.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * �� ������ Segment�� Create/Drop/Alter/Reset ������ STATIC
 * �������̽����� �����Ѵ�.
 *
 ***********************************************************************/

#ifndef _O_SDPSF_SEG_DDL_H_
#define _O_SDPSF_SEG_DDL_H_ 1

#include <sdpDef.h>
#include <sdpsfDef.h>

class sdpsfSegDDL
{

public:
    /* [ INTERFACE ] Segment �Ҵ� */
    static IDE_RC createSegment( idvSQL                * aStatistics,
                                 sdrMtx                * aMtx,
                                 scSpaceID               aSpaceID,
                                 sdpSegType              aSegType,
                                 sdpSegHandle          * aSegmentHandle );

    /* [ INTERFACE ] Segment ���� */
    static IDE_RC dropSegment( idvSQL              * aStatistics,
                               sdrMtx              * aMtx,
                               scSpaceID             aSpaceID,
                               scPageID              aSegPID );

    /*  [ INTERFACE ] Segment ����  */
    static IDE_RC resetSegment( idvSQL           * aStatistics,
                                sdrMtx           * aMtx,
                                scSpaceID          aSpaceID,
                                sdpSegHandle     * aSegmentHandle,
                                sdpSegType         aSegType );

    static IDE_RC trim( idvSQL           *aStatistics,
                        sdrMtx           *aMtx,
                        scSpaceID         aSpaceID,
                        sdpsfSegHdr      *aSegHdr,
                        sdRID             aExtRID );

private:

    static IDE_RC allocateSegment( idvSQL          * aStatistics,
                                   sdrMtx          * aMtx,
                                   scSpaceID         aSpaceID,
                                   sdpSegType        aSegType,
                                   sdpSegHandle    * aSegHandle );
};


#endif // _O_SDPSF_SEG_DDL_H_
