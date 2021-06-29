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

# ifndef _O_SDPSF_FT_H_
# define _O_SDPSF_FT_H_ 1

# include <sdpsfDef.h>
# include <sdr.h>
# include <sdpsfDef.h>
# include <sdpsfUFmtPIDList.h>
# include <sdpsfFreePIDList.h>
# include <sdpsfPvtFreePIDList.h>
# include <sdpPhyPage.h>
# include <sdpSglPIDList.h>

//------------------------------------------------------
// D$DISK_TABLE_FMS_SEGHDR
//------------------------------------------------------

typedef struct sdpsfDumpSegHdr
{
    scPageID           mSegHdrPID;     // Segment Hdr PageID 
    SChar              mSegType;       // ���� SegmentType
    UShort             mState;         // �Ҵ� ���� 
    UInt               mPageCntInExt;  // PageCnt In Ext 
    sdpSglPIDListBase  mPvtFreePIDList;// Private page list
    sdpSglPIDListBase  mUFmtPIDList;   // Unformated page list/
    sdpSglPIDListBase  mFreePIDList;   // Insert�� ������ �ִ� PageList 
    ULong              mFmtPageCnt;    // Segment���� �ѹ��̶� �Ҵ�� Page�� ���� 
    scPageID           mHWMPID;        // Segment���� Format�� ������ �������� ����Ų��. 
    sdRID              mAllocExtRID;   // Segment���� ���� Page Alloc�� ���� ������� Ext RID 
} sdpsfDumpSegHdr;

//------------------------------------------------------
// D$DISK_TABLE_FMS_FREEPIDLIST
//------------------------------------------------------
typedef struct sdpsfPageInfoOfFreeLst
{
    SChar              mSegType;    // ���� SegmentType
    scPageID           mPID;        // Free Page ID
    UInt               mRecCnt;     // Page �� Record Count
    UInt               mFreeSize;   // Page �� FreeSize
    UInt               mPageType;   // Page Type
    UInt               mState;      // Page ����
} sdpsfPageInfoOfFreeLst;

//------------------------------------------------------
// D$DISK_TABLE_FMS_PVTPIDLIST
//------------------------------------------------------
typedef struct sdpsfPageInfoOfPvtFreeLst
{
    SChar              mSegType;    // ���� SegmentType
    scPageID           mPID;        // Private Page ID
} sdpsfPageInfoOfPvtFreeLst;

//------------------------------------------------------
// D$DISK_TABLE_FMS_UFMTPIDLIST
//------------------------------------------------------
typedef struct sdpsfPageInfoOfUFmtFreeLst
{
    SChar              mSegType;    // ���� SegmentType
    scPageID           mPID;        // Unformatedd Page ID
} sdpsfPageInfoOfUFmtFreeLst;

class sdpsfFT
{
public:
    //------------------------------------------------------
    // D$DISK_TABLE_FMS_SEGHDR
    //------------------------------------------------------

    static IDE_RC buildRecord4SegHdr( idvSQL              * /*aStatistics*/,
                                      void                * aHeader,
                                      void                * aDumpObj,
                                      iduFixedTableMemory * aMemory );

    static IDE_RC dumpSegHdr( scSpaceID             aSpaceID,
                              scPageID              aPageID,
                              sdpSegType            aSegType,
                              void                * aHeader,
                              iduFixedTableMemory * aMemory );

    //------------------------------------------------------
    // D$DISK_TABLE_FMS_FREEPIDLIST
    //------------------------------------------------------
    static IDE_RC buildRecord4FreePIDList( idvSQL              * /*aStatistics*/,
                                           void                * aHeader,
                                           void                * aDumpObj,
                                           iduFixedTableMemory * aMemory );

    static IDE_RC dumpFreePIDList( scSpaceID             aSpaceID,
                                   scPageID              aPageID,
                                   sdpSegType            aSegType,
                                   void                * aHeader,
                                   iduFixedTableMemory * aMemory );

    //------------------------------------------------------
    // D$DISK_TABLE_FMS_PVTPIDLIST
    //------------------------------------------------------
    static IDE_RC buildRecord4PvtPIDList( idvSQL              * /*aStatistics*/,
                                          void                * aHeader,
                                          void                * aDumpObj,
                                          iduFixedTableMemory * aMemory );

    static IDE_RC dumpPvtPIDList( scSpaceID             aSpaceID,
                                  scPageID              aPageID,
                                  sdpSegType            aSegType,
                                  void                * aHeader,
                                  iduFixedTableMemory * aMemory );


    //------------------------------------------------------
    // D$DISK_TABLE_FMS_UFMTPIDLIST
    //------------------------------------------------------
    static IDE_RC buildRecord4UFmtPIDList( idvSQL              * /*aStatistics*/,
                                           void                * aHeader,
                                           void                * aDumpObj,
                                           iduFixedTableMemory * aMemory );

    static IDE_RC dumpUfmtPIDList( scSpaceID             aSpaceID,
                                   scPageID              aPageID,
                                   sdpSegType            aSegType,
                                   void                * aHeader,
                                   iduFixedTableMemory * aMemory );

};

#endif     // _O_SDPSF_FT_H_

