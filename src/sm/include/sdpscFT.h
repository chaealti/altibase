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
 * $Id: sdpscFT.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� Fixed Table�� ���� ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_FT_H_
# define _O_SDPSC_FT_H_ 1

#include <sdpscDef.h>
#include <sdcTXSegFreeList.h>


/***********************************************************************
 * X$DISK_UNDO_EXTDIRHDR Dump Table�� ���ڵ�
 * Dump Extent Dir Control Header ����
 ***********************************************************************/
typedef struct sdpscDumpExtDirCntlHdr
{
    UChar        mSegType;       // Dump�� Segment�� Ÿ��
    scSpaceID    mSegSpaceID;    // Segment�� TbsID
    scPageID     mSegPageID;     // Segment�� PID
    scPageID     mMyPageID;      // ���� ExtDirCtlHeader�� PID
    UShort       mExtCnt;        // ���������� Extent ����
    scPageID     mNxtExtDir;     // ���� Extent Map �������� PID
    UShort       mMaxExtCnt;     // �ִ� Extent���� ����
    scOffset     mMapOffset;     // ������ ���� Extent Map�� Offset
    smSCN        mLstCommitSCN;  // ������ ����� Ŀ���� Ʈ������� CommitSCN
    smSCN        mFstDskViewSCN; // ����ߴ� Ȥ�� ������� Ʈ������� Fst Disk ViewSCN

    UInt          mTxExtCnt;        // TSS Segment�� �Ҵ�� Extent �� ����
    UInt          mExpiredExtCnt;   // Segment�� �Ҵ�� Expire������ Extent ����
    UInt          mUnExpiredExtCnt; // Segment�� �Ҵ�� UnExpire ������ Extent ��
    UInt          mUnStealExtCnt;   // SegHdr�� ���� ���� �ִ� Ext
    smSCN         mSysMinViewSCN;   // Segment ��ȸ�� ����� minViewSCN
    UChar         mIsOnline;        // binding �Ǿ� �ִ��� ����
} sdpscDumpExtDirCntlHdr;

/***********************************************************************
 * X$DISK_UNDO_SEGHDR Dump Table�� ���ڵ�
 * Dump Segment Header
 ***********************************************************************/
typedef struct sdpscDumpSegHdrInfo
{
    UChar         mSegType;       // Dump�� Segment�� Ÿ��
    scPageID      mSegPID;        // PID
    sdpSegState   mSegState;      // SEG_STATE
    UInt          mTotExtCnt;     // Segment�� �Ҵ�� Extent �� ����
    scPageID      mLstExtDir;     // ������ ExtDir �������� PID
    UInt          mTotExtDirCnt;  // Extent Map�� �� ���� (SegHdr �����Ѱ���)
    UInt          mPageCntInExt;  // Extent�� ������ ����
    UShort        mExtCntInPage;  // ���������� Extent ����
    scPageID      mNxtExtDir;     // ���� Extent Map �������� PID
    UShort        mMaxExtCnt;     // �ִ� Extent���� ����
    scOffset      mMapOffset;     // ������ ���� Extent Map�� Offset
    smSCN         mLstCommitSCN;  // ������ ����� Ŀ���� Ʈ������� CommitSCN
    smSCN         mFstDskViewSCN; // ����ߴ� Ȥ�� ������� Ʈ������� Fst Disk ViewSCN

    UInt          mTxExtCnt;        // TSS Segment�� �Ҵ�� Extent �� ����
    UInt          mExpiredExtCnt;   // Segment�� �Ҵ�� Expire������ Extent ����
    UInt          mUnExpiredExtCnt; // Segment�� �Ҵ�� UnExpire ������ Extent ��
    UInt          mUnStealExtCnt;   // SegHdr�� ���� ���� �ִ� Ext
    smSCN         mSysMinViewSCN;   // Segment ��ȸ�� ����� minViewSCN
    UChar         mIsOnline;        // binding �Ǿ� �ִ��� ���� 
} sdpscDumpSegHdrInfo;

class sdpscFT
{
public:
    static IDE_RC initialize();
    static IDE_RC destroy();

    // ���׸�Ʈ ��� Dump
    static IDE_RC dumpSegHdr( scSpaceID             aSpaceID,
                              scPageID              aPageID,
                              sdpSegType            aSegType,
                              sdcTXSegEntry       * aEntry,
                              sdpscDumpSegHdrInfo * aDumpSegHdr,
                              void                * aHeader,
                              iduFixedTableMemory * aMemory );

    // ExtentDirectory��� Dump
    static IDE_RC dumpExtDirHdr( scSpaceID             aSpaceID,
                                 scPageID              aPageID,
                                 sdpSegType            aSegType,
                                 void                * aHeader,
                                 iduFixedTableMemory * aMemory );
    

    /* X$DISK_UNDO_SEGHDR */
    static IDE_RC buildRecord4SegHdr(
        idvSQL              * /*aStatistics*/,
        void                * aHeader,
        void                * aDumpObj,
        iduFixedTableMemory * aMemory );

    /* X$DISK_UNDO_EXTDIRHDR */
    static IDE_RC buildRecord4ExtDirHdr(
        idvSQL              * /*aStatistics*/,
        void                * aHeader,
        void                * aDumpObj,
        iduFixedTableMemory * aMemory );
private:
    static SChar convertSegTypeToChar( sdpSegType aSegType );
    static SChar convertToChar( idBool aIsTrue );
};


#endif // _O_SDPSC_FT_H_

