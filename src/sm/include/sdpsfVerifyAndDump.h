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
 * $Id: sdpsfVerifyAndDump.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * �� ������ Freelist Managed Segment�� �ڷᱸ�� Ȯ�� �� ��°� ���õ� STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# ifndef _O_SDPSF_VERIFY_AND_DUMP_H_
# define _O_SDPSF_VERIFY_AND_DUMP_H_ 1

# include <sdpDef.h>
# include <sdpsfDef.h>

class sdpsfVerifyAndDump
{

public:

    /* [ INTERFACE ] Segment Descriptor�� �ڷᱸ���� ǥ��������� ����Ѵ� */
    static void dump( scSpaceID      aSpaceID,
                      sdpsfSegHdr   *aSegHdr,
                      idBool         aDisplayAll);

    /*  [ INTERFACE ] Segment Descriptor�� Ȯ���Ѵ� */
    static IDE_RC verify(idvSQL       * aStatistics,
                         scSpaceID      aSpaceID,
                         sdpsfSegHdr  * aSegHdr,
                         UInt           aFlag,
                         idBool         aAllUsed,
                         scPageID       aUsedLimit);

    /* Segment Descirptor�� �ڷᱸ���� Ȯ���Ѵ�. */
    static IDE_RC verifyStateSeg( idvSQL   *aStatistics,
                                  sdrMtx   *aMtx,
                                  scSpaceID aSpaceID,
                                  scPageID  aSegPID );
    
private:


    /* Segment Descirptor�� �ڷᱸ���� Ȯ���Ѵ�. */
    static IDE_RC verifyStateExt( idvSQL   *aStatistics,
                                  sdrMtx   *aMtx,
                                  scSpaceID aSpaceID,
                                  sdRID     aExtRID,
                                  idBool    aUsedState,
                                  sdRID    *aNextExtRID );

    /* Segment Descirptor�� �ڷᱸ���� Ȯ���Ѵ�. */
    static IDE_RC verifyStatePage( sdpsfExtDesc  *aExtDesc,
                                   UInt           aPageIndex,
                                   UInt           aExtState,
                                   UInt          *aInsCount,
                                   UInt          *aFreeCount,
                                   UInt          *aUptCount );
};


#endif // _O_SDPSF_VERIFY_AND_DUMP_H_
