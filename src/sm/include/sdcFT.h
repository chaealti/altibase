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
 * $Id: sdcFT.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 * �� ������ Collection Layer�� Fixed Table ������� �Դϴ�.
 *
 **********************************************************************/

#ifndef _O_SDC_FT_H_
#define _O_SDC_FT_H_ 1

#include <smDef.h>
#include <smc.h>
#include <sdcDef.h>

//-------------------------------
// D$DISK_TABLE_RECORD
//-------------------------------
typedef struct sdcDumpDiskTableRow
{
    scPageID  mPageID;                                      // PAGE_ID
    scSlotNum mSlotNum;                                     // SLOT_NUM
    scOffset  mSlotOffset;                                  // SLOT_OFFSET
    SShort    mNthCTS;                                      // NTH_CTS
    smSCN     mInfiniteSCN;                                 // INFINITE_SCN
    scPageID  mUndoPID;                                     // UNDO_PAGEID
    scSlotNum mUndoSlotNum;                                 // UNDO_SLOTNUM
    UShort    mColumnCount;                                 // COLUMN_COUNT
    UShort    mRowFlag;                                     // ROW_FLAG

    scPageID  mTSSPageID;                                   // TSS_PAGEID
    scSlotNum mTSSlotNum;                                   // TSS_SLOTNUM
    UShort    mFSCredit;                                    // FSCREDIT
    smSCN     mFSCNOrCSCN;                                  // FSCN_OR_CSCN

    scPageID  mNextPiecePageID;                             // NEXT_PIECE_PAGE_ID
    scSlotNum mNextPieceSlotNum;                            // NEXT_PIECE_SLOT_NUM

    UShort    mColumnSeqInRowPiece;                         // COLUMN_SEQ
    UShort    mColumnLenInRowPiece;                         // COLUMN_LEN
    SChar     mColumnValInRowPiece[SM_DUMP_VALUE_LENGTH ];  // COLUMN_VAL
} sdcDumpDiskTableRow;


//-------------------------------
// D$DISK_TABLE_CTS �� ����
//-------------------------------

typedef struct sdcDumpCTS
{
    UInt           mPID;              // PID
    SShort         mNthSlot;          // NTH_SLOT
    smSCN          mFSCNOrCSCN;       // FSCN_OR_CSCN
    UChar          mState;            // STATE
    scPageID       mTSSPageID;        // TSS_PAGEID
    scSlotNum      mTSSlotNum;        // TSS_SLOTNUM
    UShort         mFSCredit;         // FSCREDIT
    UShort         mRefCnt;           // REF_CNT
    UShort         mRefSlotNum1;      // REF_SLOTNUM1
    UShort         mRefSlotNum2;      // REF_SLOTNUM2
} sdcDumpCTS;

//-------------------------------
// D$DISK_TABLE_CTL �� ����
//-------------------------------

typedef struct sdcDumpCTL
{
    UInt           mPID;          // PID
    UInt           mTotCTSCnt;    // TOTAL_CTS_COUNT
    UInt           mBindCTSCnt;   // BIND_CTS_COUNT
    UInt           mDelRowCnt;    // DELETE_ROW_COUNT
    smSCN          mSCN4Aging;    // SCN_FOR_AGING
    UInt           mCandAgingCnt; // CANDIDATE_AGING_COUNT
} sdcDumpCTL;


//-------------------------------
// D$DISK_TABLE_SLOTDIR
//-------------------------------
typedef struct sdcDumpDiskTableSlotDir
{
    scPageID  mPageID;         // PAGE_ID
    UShort    mSlotEntryCnt;   // SLOT_ENTRY_CNT
    scSlotNum mUnusedListHead; // UNUSED_LIST_HEAD
    scSlotNum mSlotNum;        // SLOT_NUM
    SChar     mIsUnused;       // IS_UNUSED
    scOffset  mOffset;         // OFFSET
} sdcDumpDiskTableSlotDir;


//-------------------------------
// D$DISK_TABLE_EXTLIST
//-------------------------------

/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * DumpTBSInfo�� Extent������ ����ϰ� �־� �и��Ѵ�. */

typedef struct sdcDumpDiskTableExtList
{
    UChar              mSegType;     // SegmentType
    sdRID              mExtRID;      // TBS�� ���� Extent RID 
    scPageID           mPID;         // Extent RID���� PageID 
    scOffset           mOffset;      // Extent RID���� mOffset 
    scPageID           mFstPID;      // Extent�� ù��° PID 
    scPageID           mFstDataPID;  // Extent�� ù��° Data PID 
    UInt               mPageCntInExt;// Extent�� ������ ���� 
} sdcDumpDiskTableExtList;



//-------------------------------
// D$DISK_TABLE_PIDLIST
//-------------------------------
typedef struct sdcDumpDiskTablePIDList
{
    UChar     mSegType;                // SegmentType
    scPageID  mPageID;                 // PAGE_ID
    UShort    mPageType;               // PAGE_TYPE
    UShort    mPageState;              // PAGE_STATE

    UShort    mLogicalHdrSize;         // LOGICAL_HDR_SIZE
    UShort    mSizeOfCTL;              // CTL_SIZE

    UShort    mTotalFreeSize;          // TOFS
    UShort    mAvailableFreeSize;      // AVSP
    UShort    mFreeSpaceBeginOffset;   // FSBO
    UShort    mFreeSpaceEndOffset;     // FSEO

    ULong     mSeqNo;                  // SEQNO
} sdcDumpDiskTablePIDList;

//-------------------------------
// D$DISK_TABLE_LOB_INFO
//-------------------------------
typedef struct sdcDumpDiskTableLobInfo
{
    scPageID    mPageID;            // HEAD_ROW_PIECE_PAGE_ID
    scSlotNum   mHdrSlotNum;        // HEAD_ROW_PIECE_SLOT_NUM
    UInt        mColumnID;          // COLUMN_ID
    UInt        mColumnPieceSize;   // LOB COLUMN SIZE
    ULong       mLength;
    SChar       mIsNull; 
    UChar     * mInOutMode;         // LOB MODE (In Mode or Out Mode)

    // Lob Descriptor
    UInt        mLobDescFlag;
    UInt        mLastPageSize;
    UInt        mLastPageSeq;
    ULong       mLobVersion;
    scPageID    mRootNodePID;
    UInt        mDirectCnt;
    scPageID    mDirect[SDC_LOB_MAX_DIRECT_PAGE_CNT];
} sdcDumpDiskTableLobInfo;

//-------------------------------
// D$DISK_TABLE_LOB_AGINGLIST
//-------------------------------
typedef struct sdcDumpDiskTableLobAgingList
{
    UInt        mColumnIdx;     // Lob Column Idx

    scPageID    mPageID;        // Current Page ID
    UInt        mPageType;      // Meta Page, Index Page, Data Page

    UInt        mHeight;
    UInt        mKeyCnt;
    UInt        mStoreSize;
    UInt        mNodeSeq;
    UInt        mLobPageState;  // Lob Page State
    ULong       mLobVersion;
    sdSID       mTSSlotSID;     // TSSlot SID
    smSCN       mFstDskViewSCN; // Commit SCN

    scPageID    mPrevPID;       // Prev Page ID
    scPageID    mNextPID;       // Next Page ID
} sdcDumpDiskTableLobAgingList;

//-------------------------------
// D$DISK_TABLE_LOB_META
//-------------------------------
typedef struct sdcDumpDiskTableLobMeta
{
    UInt        mColumnIdx;
    scPageID    mSegPID;
    SChar       mLogging;
    SChar       mBuffer;
    UInt        mColumnID;
    ULong       mNodeCnt;
    scPageID    mPrevPID;
    scPageID    mNextPID;
} sdcDumpDiskTableLobMeta;

// Segment�� ��ȸ�ϸ鼭 Dump�� ��츦 ����, 
// Segement Header PID �� �Ѱ��ֱ� ���� �ݹ� �Լ��� �����.
typedef IDE_RC (*sdcSegDumpCallback)( scSpaceID             aSpaceID,
                                      scPageID              aPageID,
                                      sdpSegType            aSegType,
                                      void                * aHeader,
                                      iduFixedTableMemory * aMemory );

class sdcFT
{
public:
    //------------------------------------------
    // For X$TSSEGS
    //------------------------------------------

    static IDE_RC buildRecord4TSSEGS( idvSQL              * /*aStatistics*/,
                                      void                *aHeader,
                                      void                *aDumpObj,
                                      iduFixedTableMemory *aMemory );

    //------------------------------------------
    // For X$UDSEGS
    //------------------------------------------

    static IDE_RC buildRecord4UDSEGS( idvSQL              * /*aStatistics*/,
                                      void                *aHeader,
                                      void                *aDumpObj,
                                      iduFixedTableMemory *aMemory );

    //------------------------------------------
    // For X$DISK_TSS_RECORDS
    //------------------------------------------

    static IDE_RC buildRecordForDiskTSSRecords(idvSQL              * /*aStatistics*/,
                                               void                * aHeader,
                                               void                * /* aDumpObj */,
                                               iduFixedTableMemory * aMemory);
        
    //------------------------------------------
    // For X$DISK_UNDO_RECORDS
    //------------------------------------------

    static IDE_RC buildRecordForDiskUndoRecords(idvSQL              * /*aStatistics*/,
                                                void                * aHeader,
                                                void                * /* aDumpObj */,
                                                iduFixedTableMemory * aMemory);


    //------------------------------------------------------------------------
    // D$ Dump

    //------------------------------------------
    // For D$DISK_TABLE_RECORD
    //------------------------------------------

    static IDE_RC buildRecordDiskTableRecord( idvSQL              * /*aStatistics*/,
                                              void                * aHeader,
                                              void                * aDumpObj,
                                              iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_PAGE
    //------------------------------------------

    static IDE_RC buildRecordDiskTablePage( idvSQL              * /*aStatistics*/,
                                            void                * aHeader,
                                            void                * aDumpObj,
                                            iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_CTS
    //------------------------------------------
    static IDE_RC buildRecordDiskTableCTS( idvSQL              * /*aStatistics*/,
                                           void                * aHeader,
                                           void                * aDumpObj,
                                           iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_CTL
    //------------------------------------------
    static IDE_RC buildRecordDiskTableCTL( idvSQL              * /*aStatistics*/,
                                           void                * aHeader,
                                           void                * aDumpObj,
                                           iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_SLOTDIR
    //------------------------------------------

    static IDE_RC buildRecordDiskTableSlotDir( idvSQL              * /*aStatistics*/,
                                               void                * aHeader,
                                               void                * aDumpObj,
                                               iduFixedTableMemory * aMemory );

    //------------------------------------------
    // X$DIRECT_PATH_INSERT
    //------------------------------------------
    static IDE_RC buildRecordForDPathInsert( idvSQL              * /*aStatistics*/,
                                             void                 * aHeader,
                                             void                 * aDumpObj,
                                             iduFixedTableMemory  * aMemory );

    /* TASK-4007 [SM]PBT�� ���� ��� �߰�
     * Table, Index, Lob���κ��� SegHdrPID�� �����´�*/
    static IDE_RC doAction4EachSeg( void                * aTable,
                                    sdcSegDumpCallback    aSegDumpFunc,
                                    void                * aHeader,
                                    iduFixedTableMemory * aMemory );

    // �� ���̺��� ���� Segment�κ��� ExtentList�� �����´�.
    static IDE_RC dumpExtList( scSpaceID             aSpaceID,
                               scPageID              aPageID,
                               sdpSegType            aSegType,
                               void                * aHeader,
                               iduFixedTableMemory * aMemory );

    static IDE_RC buildRecord4ExtList(
        idvSQL              * /*aStatistics*/,
        void                * aHeader,
        void                * aDumpObj,
        iduFixedTableMemory * aMemory );



    // �� ���̺��� ���� Segment�κ��� ExtentList�� �����´�.
    static IDE_RC dumpPidList( scSpaceID             aSpaceID,
                               scPageID              aPageID,
                               sdpSegType            aSegType,
                               void                * aHeader,
                               iduFixedTableMemory * aMemory );

    static IDE_RC buildRecord4PIDList(
        idvSQL              * /*aStatistics*/,
        void                * aHeader,
        void                * aDumpObj,
        iduFixedTableMemory * aMemory );

    static SChar convertSegTypeToChar( sdpSegType aSegType );

    /*
     * PROJ-2047 Strengthening LOB
     */
    
    //------------------------------------------
    // For D$DISK_TABLE_LOB_AGINGLIST
    //------------------------------------------

    static IDE_RC buildRecord4LobAgingList( idvSQL              * /*aStatistics*/,
                                            void                * aHeader,
                                            void                * aDumpObj,
                                            iduFixedTableMemory * aMemory );

    static IDE_RC dumpLobAgingList( scSpaceID             aSpaceID,
                                    scPageID              aPageID,
                                    UInt                  aColumnIdx,
                                    void                * aHeader,
                                    iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_LOB_DESC
    //------------------------------------------

    static IDE_RC buildRecordDiskTableLobInfo( idvSQL              * /*aStatistics*/,
                                               void                * aHeader,
                                               void                * aDumpObj,
                                               iduFixedTableMemory * aMemory );

    //------------------------------------------
    // For D$DISK_TABLE_LOB_META
    //------------------------------------------

    static IDE_RC buildRecord4LobMeta( idvSQL              * /*aStatistics*/,
                                       void                * aHeader,
                                       void                * aDumpObj,
                                       iduFixedTableMemory * aMemory );

    static IDE_RC dumpLobMeta( scSpaceID             aSpaceID,
                               scPageID              aPageID,
                               UInt                  aColumnIdx,
                               void                * aHeader,
                               iduFixedTableMemory * aMemory );

private:

    static void getLobColInfoLst( smcTableHeader      * aTblHdr,
                                  smiColumn           * aColumns,
                                  UInt                * aColumnCnt,
                                  UInt                * aLobColCnt );

    static IDE_RC buildLobInfoDumpRec( void                * aHeader,
                                       iduFixedTableMemory * aMemory ,
                                       void                * aTrans,
                                       scSpaceID             aSpaceID,
                                       scPageID              aCurPageID,
                                       UShort                aSlotNum,
                                       smiColumn           * aColumns,
                                       UInt                  aLobColCnt,
                                       smcTableHeader      * aTblHdr,
                                       UChar              ** aCurPagePtr,
                                       idBool              * aIsPageLatchReleased );

};

#endif // _O_SDC_FT_H_


