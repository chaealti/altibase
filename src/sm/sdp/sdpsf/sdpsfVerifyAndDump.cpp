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
 * $Id: sdpsfVerifyAndDump.cpp 27226 2008-07-23 17:36:35Z newdaily $
 *
 * �� ������ Segment�� �ڷᱸ�� Ȯ�� �� ��°� ���õ� STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <smErrorCode.h>

# include <sdpsfVerifyAndDump.h>
# include <sdpSglRIDList.h>
# include <sdpsfSH.h>

/*
 * [ INTERFACE ] Segment Descriptor�� �ڷᱸ���� ǥ��������� ����Ѵ�
 */
void sdpsfVerifyAndDump::dump( scSpaceID      aSpaceID,
                               sdpsfSegHdr   *aSegHdr,
                               idBool         aDisplayAll)
{
    sdRID              sRID;
    SInt               i;

    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );

    if( aDisplayAll != ID_TRUE )
    {
        if( sdpsfSH::getState(aSegHdr) != SDP_SEG_USE )
        {
            return;
        }
    }

    sRID = sdpPhyPage::getRIDFromPtr((UChar*)aSegHdr);

    idlOS::printf( "-----------  Segment Desc "
                   "< RID: %"ID_UINT64_FMT", "
                   "PID: %"ID_UINT32_FMT", Offset: %"ID_UINT32_FMT" > Begin ----------\n",
                   sRID,
                   SD_MAKE_PID(sRID),
                   SD_MAKE_OFFSET(sRID));

    idlOS::printf( "Type \t\t\t\t\t: %s\n",
                   (sdpsfSH::getState(aSegHdr) == SDP_SEG_FREE) ?
                   "SDP_SEG_FREE" : "SDP_SEG_USE" );

    for( i = 0; i < SDP_MAX_SEG_PID_CNT; i++ )
    {
        idlOS::printf( "Segment Meta Page ID %"ID_UINT32_FMT" th"
                       " \t\t\t: %"ID_UINT32_FMT"\n",
                       i,
                       aSegHdr->mArrMetaPID[i] );
    }

    idlOS::printf( "Ext List Count \t\t\t: %"ID_UINT64_FMT"\n",
                   aSegHdr->mTotExtCnt );

    idlOS::printf( "----------- Segment Desc End  ----------\n" );

    return;
}

/*
 * [ INTERFACE ] Segment �� ��� �ڷᱸ���� Ȯ���Ѵ�.
 */
IDE_RC sdpsfVerifyAndDump::verify( idvSQL       */*aStatistics*/,
                                   scSpaceID     /*aSpaceID*/,
                                   sdpsfSegHdr  */*aSegHdr*/,
                                   UInt          /*aFlag*/,
                                   idBool        /*aAllUsed*/,
                                   scPageID      /*aUsedLimit*/ )
{
    return IDE_SUCCESS;
}

/*
 * Segment�� �ڷᱸ���� Ȯ���ϴ� �������� Extent Full List��
 * Extent Used List�� Ȯ���Ѵ�.
 */
IDE_RC sdpsfVerifyAndDump::verifyStateSeg( idvSQL    */*aStatistics*/,
                                           sdrMtx    */*aMtx*/,
                                           scSpaceID  /*aSpaceID*/,
                                           scPageID   /*aSegPID*/ )
{
    return IDE_SUCCESS;
}

/*
 * Segment�� �ڷᱸ���� Ȯ���ϴ� �������� Extent�� ���¸� Ȯ���Ѵ�.
 */
IDE_RC sdpsfVerifyAndDump::verifyStateExt( idvSQL   */*aStatistics*/,
                                           sdrMtx   */*aMtx*/,
                                           scSpaceID /*aSpaceID*/,
                                           sdRID     /*aExtRID*/,
                                           idBool    /*aUsedState*/,
                                           sdRID    */*aNextExtRID*/ )
{
    return IDE_SUCCESS;
}

/*
 * Segment�� �ڷᱸ���� Ȯ���ϴ� �������� Extent�� Page Bitset Vector��
 * ���� Data �������� ���°� ��ġ�ϴ��� Ȯ���Ѵ�.
 */
IDE_RC sdpsfVerifyAndDump::verifyStatePage( sdpsfExtDesc  */*aExtDesc*/,
                                            UInt           /*aPageIndex*/,
                                            UInt           /*aExtState*/,
                                            UInt          */*aInsCount*/,
                                            UInt          */*aFreeCount*/,
                                            UInt          */*aUptCount*/ )
{
    return IDE_SUCCESS;
}
