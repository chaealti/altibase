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
 * $Id: sdpFT.h 19550 2010-01-07
 **********************************************************************/

#ifndef _O_SDP_FT_H_
#define _O_SDP_FT_H_  (1)

# include <idu.h>
# include <smDef.h>
# include <smpFT.h>
# include <sdcDef.h>

/* TASK-4007 [SM] PBT�� ���� ��� �߰� 
 *
 * Page dump�� ���� fixed table ����*/

//----------------------------------------
// D$DISK_DB_PAGE
//-----------------------------------------
typedef struct sdpDiskDBPageDump
{
    scSpaceID mTBSID;
    scPageID  mPID;
    SChar     mPageDump[SMP_DUMP_COLUMN_SIZE];
} sdpDiskDBPageDump;

//----------------------------------------
// D$DISK_DB_PHYPAGEHDR
//----------------------------------------- 
typedef struct sdpDiskDBPhyPageHdrDump
{
    UInt     mCheckSum;
    UInt     mPageLSNFILENo;
    UInt     mPageLSNOffset;
    ULong    mIndexSMONo;
    UShort   mSpaceID;
    UShort   mTotalFreeSize;
    UShort   mAvailableFreeSize;
    UShort   mLogicalHdrSize;
    UShort   mSizeOfCTL;
    UShort   mFreeSpaceBeginOffset;
    UShort   mFreeSpaceEndOffset;
    UShort   mPageType;
    UShort   mPageState;
    scPageID mPageID;
    UChar    mIsConsistent;
    UInt     mIndexID;
    UShort   mLinkState;
    scPageID mParentPID;
    SShort   mIdxInParent;
    scPageID mPrevPID;
    scPageID mNextPID;
    smOID    mTableOID;
} sdpDiskDBPhyPageHdrDump;

//----------------------------------------- 
// X$SEGMENT
//----------------------------------------- 
typedef struct sdpSegmentPerfV
{
    UInt         mSpaceID;             /* TBS ID */
    ULong        mTableOID;            /* Table OID */
    ULong        mObjectID;            /* Object ID */
    scPageID     mSegPID;              /* ���׸�Ʈ ������ PID */
    sdpSegType   mSegmentType;         /* Segment Type */
    UInt         mState;               /* Segment �Ҵ翩�� */
    ULong        mExtentTotalCnt;      /* Extent �� ���� */

    /* BUG-31372: ���׸�Ʈ �ǻ��� ������ ��ȸ�� ����� �ʿ��մϴ�. */
    ULong        mUsedTotalSize;       /* ���׸�Ʈ ���� */
} sdpSegmentPerfV;

class sdpFT
{
public:

    //------------------------------------------
    // D$DISK_DB_PAGE
    // ReadPage( Fix + latch )�� �� Binary�� �ٷ� Dump�Ѵ�.
    //------------------------------------------
    static IDE_RC buildRecordDiskDBPageDump(idvSQL              * /*aStatistics*/,
                                            void                *aHeader,
                                            void                *aDumpObj,
                                            iduFixedTableMemory *aMemory);

    //------------------------------------------
    // D$DISK_DB_PHYPAGEHDR,
    // PhysicalPageHdr�� Dump�Ѵ�.
    //------------------------------------------
    static IDE_RC buildRecordDiskDBPhyPageHdrDump(idvSQL              * /*aStatistics*/,
                                                  void                *aHeader,
                                                  void                *aDumpObj,
                                                  iduFixedTableMemory *aMemory);

    //------------------------------------------
    // X$SEGMENT            
    //------------------------------------------
    static IDE_RC buildRecordForSegment(idvSQL              * /*aStatistics*/,
                                        void        *aHeader,
                                        void        *aDumpObj,
                                        iduFixedTableMemory *aMemory);

    static IDE_RC getSegmentInfo( UInt               aSpaceID,
                                  scPageID           aSegPID,
                                  ULong              aTableOID,
                                  sdpSegCCache     * aSegCache,
                                  sdpSegmentPerfV  * aSegmentInfo );
};


#endif /* _O_SDP_FT_H_ */
