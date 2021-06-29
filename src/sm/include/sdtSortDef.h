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
 * $Id: $
 **********************************************************************/

#ifndef _O_SDT_SORT_DEF_H_
# define _O_SDT_SORT_DEF_H_ 1

#include <sdtDef.h>

/*****************************************************************************
 * PROJ-2201 Innovation in sorting and hashing(temp)
 *****************************************************************************/

/*********************************************************************
 *
 * - SortTemp�� State Transition Diagram
 *      +------------------------------------------------------+
 *      |                                                      |
 * +------------+                                         +----------+
 * |InsertNSort |----<sort()>--+                          |InsertOnly|
 * +------------+   +----------+-------------------+      +----------+
 *      |           |          |                   |           |
 *      |   +------------+ +-------+ +---------+   |           |
 *   sort() |ExtractNSort|-| Merge |-|MakeIndex| sort()      scan()
 *      |   +------------+ +-------+ +---------+   |           |
 *      |                      |          |        |           |
 * +------------+        +---------+ +---------+   |      +----------+
 * |InMemoryScan|        |MergeScan| |IndexScan|---+      |   Scan   |
 * +------------+        +---------+ +---------+          +----------+
 *
 * �� ���¿� ���� Group���� ���еǸ�, Group�� ���� �Ʒ� Copy ������ �޶�����.
 *
 *
 *
 * 1. InsertNSort
 *  InsertNSort�� �ַη� �����Ͱ� ���ԵǴ� �����̴�. �ٸ� �����ϸ鼭 ���ÿ�
 * ���ĵ� �Ѵ�.
 *  �׸��� �� ���´� Limit,MinMax�� InMemoryOnly �����̳�, Range�̳�, �׿ܳ�
 *  �� ���� Group�� ����� �޶�����.
 *
 * 1) InsertNSort( InMemoryOnly )
 *  Limit, MinMax�� �ݵ�� ������ ������ �Ͼ�� �ʴ´ٴ� ������ ���� ���
 *  InMemoryOnly�� �����Ѵ�.
 * +-------+ +-------+
 * | Sort1 |-| Sort2 |
 * +-------+ +-------+
 *     |         |
 *     +<-SWAP->-+
 * �ΰ��� SortGroup�� �ְ�, �� SortGroup�� ���� �������� Compaction�뵵��
 * SWAP�� �����Ѵ�.
 *
 * 2) InsertNSort ( Normal )
 *  �Ϲ����� ���·� Merge���� �ϴ� �����̴�.
 * +-------+ +-------+
 * | Sort  |-| Flush |
 * +-------+ +-------+
 *     |          |
 *     +-NORMAL->-+
 *
 * 3) ExtractNSort ( RangeScan )
 *  RangeScan�� �ʿ��� Index�� ����� �����̴�. Normal�� ������,
 * Key�� Row�� ���� �����Ѵ�. Row���� ��� Į���� ����ְ�, Key�� IndexColumn
 * �� ����. ���� Merge���� ������ Key�� ������ ó���Ѵ�.
 * +-------+ +-------+ +--------+
 * | Sort  |-| Flush |-|SubFlush|
 * +-------+ +-------+ +--------+
 *     |           |        |
 *     +-MAKE_KEY--+        |
 *     |                    |
 *     +-<-<-<-EXTRACT_ROW--+
 *
 *
 *
 *  2. ExtractNSort
 *  InsertNSort�� 1.3�� ����������, insert�� �����͸� QP�κ��� �޴µ�����
 *  Extract�� 1.3 �������� SubFlush�� �����ص� Row�� �ٽ� ������ �����Ѵٴ�
 *  ���� Ư¡�̴�.
 * +-------+ +-------+ +--------+
 * | Sort  |-| Flush |-|SubFlush|
 * +-------+ +-------+ +--------+
 *     |           |        |
 *     +-MAKE_KEY->+        |
 *     |                    |
 *     +-<-<--<-NORMAL------+
 *
 *
 *
 *  3. Merge
 *  SortGroup�� Heaq�� ������ �� FlushGroup���� �����ϸ�, FlushGroup����
 *  Run�� �����Ѵ�. �� InsertNSort(Normal) 1.2�� �����ϴ�.
 *
 *
 *
 *  4. MakeIndex
 *  Merge�� ����������, ExtraPage��� ������ Row�� �ɰ���. �ֳ��ϸ� index��
 *  �� Node���� 2���� Key�� ���� �������̸�, ���� 2�� �̻��� Key��
 *  �� �� �ֵ���, Key�� 4000Byte�� ������ ���� Row�� 4000Byte�� �ǵ���
 *  4000Byte ������ �͵��� ExtraPage�� �����Ų��.
 *
 *  1) LeafNode ����
 * +-------+ +-------+ +--------+
 * | Sort  |-| Flush |-|SubFlush|
 * +-------+ +-------+ +--------+
 *   |        |              | |
 *   |        +-<-MAKE_LNODE-+ |
 *   |                         |
 *   +-(copyExtraRow())-->->->-+
 *
 *  2) InternalNode ����
 *  LeafNode�� Depth�� �Ʒ����� ���� �ö󰡸� ����� ����̴�. �����
 *  BreadthFirst�� �����Ͽ� �ӵ��� ������ ����.
 * +-------+ +-------+ +--------+
 * | Sort  |-| Flush |-|SubFlush|
 * +-------+ +-------+ +--------+
 *            |              |
 *            +-<-MAKE_INODE-+
 *
 *
 *
 *  5. InMemoryScan
 *  InsertNSort(Normal) 1.2�� �����ϴ�.
 *
 *
 *
 *  6. MergeScan
 *  Merge�� �����ϴ�.
 *
 *
 *
 *  7.IndexScan
 *  Copy�� ����.
 *  +-------+ +-------+
 *  | INode |-| LNode |
 *  +-------+ +-------+
 *
 * 8. InsertOnly
 *  InsertOnly �ַη� �����Ͱ� ���ԵǴ� �����̴�. �ٸ� �����ϸ鼭
 *   ������ ���� �ʴ°��� InsertNSort �� ������.
 *
 * 1) InsertOnly( InMemoryOnly )
 *   �ݵ�� ������ ������ �Ͼ�� �ʴ´ٴ� ������ ���� ���
 *  InMemoryOnly�� �����Ѵ�.
 *
 * 2) InsertOnly ( Normal )
 *  �Ϲ����� ���·� ����� ���� ������ ����.
 */

typedef enum sdtCopyPurpose
{
    SDT_COPY_NORMAL,
    SDT_COPY_SWAP,
    SDT_COPY_MAKE_KEY,
    SDT_COPY_MAKE_LNODE,
    SDT_COPY_MAKE_INODE,
    SDT_COPY_EXTRACT_ROW,
} sdtCopyPurpose;

/* make MergeRun
 * +------+------+------+------+------+
 * | Size |0 PID |0 Slot|1 PID |1 Slot|
 * +------+------+------+------+------+
 * MergeRun�� ���Ͱ��� ����� Array��, MergeRunCount * 2 + 1 ���� �����ȴ�.
 */
typedef struct sdtTempMergeRunInfo
{
    /* Run��ȣ.
     * Run�� Last���� �������� ��ġ�Ǹ�, �ϳ��� Run�� MaxRowPageCount�� ũ�⸦
     * ���� ������, LastWPID - MaxRowPageCnt * No �ϸ� Run�� WPID�� �� ��
     * �ִ�. */
    UInt    mRunNo;
    UInt    mPIDSeq;  /*Run�� PageSequence */
    SShort  mSlotNo;  /*Page�� Slot��ȣ */
} sdtTempMergeRunInfo;

#define SDT_TEMP_RUNINFO_NULL ( ID_UINT_MAX )

#define SDT_TEMP_MERGEPOS_SIZEIDX     ( 0 )
#define SDT_TEMP_MERGEPOS_PIDIDX(a)   ( (a) * 2 + 1 )
#define SDT_TEMP_MERGEPOS_SLOTIDX(a)  ( (a) * 2 + 2 )

#define SDT_TEMP_MERGEPOS_SIZE(a)     ( (a) * 2 + 1 )

typedef scPageID sdtTempMergePos;


/* make ScanRun
 * +------+-----+-------+-------+------+
 * | Size | pin | 0 PID | 1 PID |
 * +------+-----+-------+-------+------+
 *  ScanRund ��  ���� ���� ����� array�� RunCount +2  �� �����ȴ�.
 */
#define SDT_TEMP_SCANPOS_SIZEIDX     ( 0 )
#define SDT_TEMP_SCANPOS_PINIDX      ( 1 )
#define SDT_TEMP_SCANPOS_HEADERIDX   ( 2 )

#define SDT_TEMP_SCANPOS_PIDIDX(a)   ( (a) + 2 )
#define SDT_TEMP_SCANPOS_SIZE(a)     ( (a) + 2 )

typedef scPageID sdtTempScanPos;


/****************************************************************************
 * PROJ-2201 Innovation in sorting and hashing(temp)
 ****************************************************************************/
#define SDT_WAGROUPID_MAX        (4)    /* �ִ� �׷� ���� */

/*******************************
 * Group
 *****************************/
/* Common */
/* sdtTempRow::fetch�� SDT_WAGROUPID_NONE�� ����Ѵٴ� ����,
 * BufferMiss�� �ش� WAGroup�� ReadPage�Ͽ� �ø��� �ʰڴٴ� �� */
#define SDT_WAGROUPID_NONE     ID_UINT_MAX
#define SDT_WAGROUPID_INIT     (0)
/* Sort */
#define SDT_WAGROUPID_SORT     (1)
#define SDT_WAGROUPID_FLUSH    (2)
#define SDT_WAGROUPID_SUBFLUSH (3)
/* Sort IndexScan */
#define SDT_WAGROUPID_INODE    (1)
#define SDT_WAGROUPID_LNODE    (2)
/* Scan*/
#define SDT_WAGROUPID_SCAN     (1)
#define SDT_WAGROUPID_MAX      (4)
#define SDT_HASH_GROUP_MAX     (3)

typedef UInt sdtGroupID;

#define SDT_WAGROUP_MIN_PAGECOUNT (2)

struct sdtWASortMapHdr;

typedef struct sdtSortGroup
{
    sdtWAReusePolicy mPolicy;

    /* WAGroup�� ����.
     * ���⼭ End�� ������ ��밡���� PageID + 1 �̴�. */
    scPageID         mBeginWPID;
    scPageID         mEndWPID;
    /* Fifo �Ǵ� LRU ��å�� ���� ������ ��Ȱ���� ���� ����.
     * �������� �Ҵ��� WPID�� ������.
     *�� �� ������ ���������� �ѹ��� ��Ȱ����� ���� �������̱� ������,
     * �� �������� ��Ȱ���ϸ� �ȴ�. */
    scPageID         mReuseWPIDSeq;

    /* LRU�϶� LRU List�� ���ȴ� */
    scPageID         mReuseWPIDTop; /* �ֱٿ� ����� ������ */
    scPageID         mReuseWPIDBot; /* ������� ������ ��Ȱ�� ��� */

    /* ������ ������ �õ��� ������.
     * WPID�� ��Ī�Ǹ�, ���� Fix�Ǿ� �����ȴ�. unassign�Ǿ� �ٸ� ������
     * �� ��ü�Ǹ�, �����ϴ� ������ �߰��� ����� �����̴�. */
    sdtWCB     *     mHintWCBPtr;

    /* WAGroup�� Map�� ������, �װ��� Group��ü�� �ϳ��� ū �������� ����
     * InMemory������ �����Ѵٴ� �ǹ��̴�. */
    sdtWASortMapHdr *mSortMapHdr;
} sdtSortGroup;

/* WAMap�� Slot�� Ŀ���� 16Byte */
#define SDT_WAMAP_SLOT_MAX_SIZE  (16)

typedef enum
{
    SDT_WM_TYPE_RUNINFO,
    SDT_WM_TYPE_POINTER,
    SDT_WM_TYPE_MAX
} sdtWMType;

struct sdtSortSegHdr;

typedef struct sdtWMTypeDesc
{
    sdtWMType     mWMType;
    const SChar * mName;
    UInt          mSlotSize;
    smuDumpFunc   mDumpFunc;
} sdtWMTypeDesc;

typedef struct sdtWASortMapHdr
{
    sdtSortSegHdr* mWASegment;
    sdtGroupID     mWAGID;          /* Map�� ���� Group */
    scPageID       mBeginWPID;      /* WAMap�� ���۵Ǵ� PID */
    UInt           mSlotCount;      /* Slot ���� */
    UInt           mSlotSize;       /* Slot �ϳ��� ũ�� */
    sdtWMType      mWMType;         /* Map Slot�� ����(Pointer, GRID �� )*/
    UInt           mVersionCount;   /* Slot�� Versioning ���� */
    UInt           mVersionIdx;     /* ������ Version Index */
} sdtWASortMapHdr;

typedef struct sdtSortSegHdr
{
    UChar                mWorkType;
    UChar                mIsInMemory;    /*�Ѱܳ� Page�� �ִ°�?*/
    scSpaceID            mSpaceID;

/* ���������� ������ WAPage. Hint �� Dump�ϱ� ���� ���� */
    /* getNPageToWCB, getWAPagePtr, getWCB �� Hint */
    sdtWCB             * mHintWCBPtr;

    UInt                 mNPageHashBucketCnt;
    sdtWCB            ** mNPageHashPtr;
    sdtWCB            *  mUsedWCBPtr;

    sdtWASortMapHdr      mSortMapHdr; /*KeyMap, Heap, PIDList���� Map */

    sdtWAExtentInfo      mWAExtentInfo;
    UInt                 mMaxWAExtentCount;
    sdtWAExtent        * mCurFreeWAExtent;
    UInt                 mCurrFreePageIdx;
    UInt                 mAllocWAPageCount;

    sdtNExtFstPIDList    mNExtFstPIDList;  /*NormalExtent���� �����ϴ� Map*/
    UInt                 mNPageCount;     /*�Ҵ��� NPage�� ���� */

    sdtSortGroup         mGroup[ SDT_WAGROUPID_MAX ];  /*WAGroup�� ���� ����*/
    iduStackMgr          mFreeNPageStack; /*FreenPage�� ��Ȱ��� */

    idvSQL             * mStatistics;
    smiTempTableStats  * mStatsPtr;

    /* ������ Ž���� Hash*/
    sdtWCB               mWCBMap[1];

} sdtSortSegHdr;

#define SDT_SORT_SEGMENT_HEADER_SIZE (ID_SIZEOF(sdtSortSegHdr) - ID_SIZEOF(sdtWCB))

typedef struct sdtSortPageHdr
{
    /* ���� 13bit�� FreeOffset, ���� 3bit�� Type���� ����� */
    scPageID mSelfPID;
    UShort   mTypeAndFreeOffset;
    UShort   mSlotCount;
    scPageID mPrevPID;
    scPageID mNextPID;
    scOffset mSlotDir[2]; // padding���� ���� ��� ������ 2���� �ִ´�.
} sdtSortPageHdr;

#define SDT_SORT_PAGE_HEADER_SIZE (ID_SIZEOF(sdtSortPageHdr) - (ID_SIZEOF(scOffset)*2) )


/**************************************************************************
 * TempRowPiece�� ������ ���� �����ȴ�.
 *
 * +-------------------------------+.+---------+--------+.-------------------------+
 * + RowPieceHeader                |.|GRID HEADER       |. ColumnValues.(mtdValues)|
 * +----+-----+------+---------+---+.+---------+--------+.-------------------------+
 * |flag|dummy|ValLen|HashValue|hit|.|ChildGRID|NextGRID|.ColumnValue|...
 * +----+-----+------+---------+---+.+---------+--------+.-------------------------+
 * <----------   BASE    ----------> <------Option------>
 *
 * Base�� ��� rowPiece�� ������.
 * NextGRID�� ChildGRID�� �ʿ信 ���� ���� �� �ִ�.
 * (������ ������ ������, ChildGRID�� FirstRowPiece�� �����Ѵ�. *
 **************************************************************************/

/* TRPInfo(TempRowPieceInfo)�� Runtime����ü�� Runtime��Ȳ���� ���Ǹ�
 * ���� Page�󿡴� sdtSortTRPHdr(TempRowPiece)�� Value�� ��ϵȴ�.  */
typedef struct sdtSortTRPHdr
{
    /* Row�� ���� �������ִ� TR(TempRow)Flag.
     * (HashValue��� ����, ������ ���Խ� ���� ����, Column�ɰ��� ���� ) */
    UChar       mTRFlag;
    UChar       mDummy;
    /* RowHeader�κ��� ������. Value�κ��� ���� */
    UShort      mValueLength;

    /* hitSequence�� */
    UInt        mHitSequence;

    /******************************* Optional ****************************/
    /* IndexInternalNode�� UniqueTempHash������ �ش� Slot�� ChildRID��
     * ����ȴ�. */
    scGRID         mChildGRID;
    /* �������� ���Ե� RowPiece�� RID�̴�. �׷��� ������ �������� ����Ǳ�
     * ������ ���������δ� ������ RowPiece�� RID�̴�.
     * �� FirstRowPiece �� ���� RowPiece���� �� NextRID�� ����Ǿ��ִ�. */
    scGRID         mNextGRID;
} sdtSortTRPHdr;

typedef struct sdtSortInsertInfo
{
    sdtSortTRPHdr  mTRPHeader;

    UInt            mColumnCount;  /* Column���� */
    smiTempColumn * mColumns;      /* Column���� */
    UInt            mValueLength;  /* Value�� ���� */
    smiValue      * mValueList;    /* Value�� */
} sdtSortInsertInfo;

typedef struct sdtSortInsertResult
{
    scGRID          mHeadRowpieceGRID; /* �Ӹ� Rowpiece�� GRID */
    sdtSortTRPHdr * mHeadRowpiecePtr;  /* �Ӹ� Rowpiece�� Pointer */
    UInt            mRowPageCount;     /* Row�����ϴµ� ����� page����*/
    idBool          mComplete;         /* ���� �����Ͽ��°� */
} sdtSortInsertResult;

typedef struct sdtSortScanInfo
{
    sdtSortTRPHdr * mTRPHeader;

    UInt            mValueLength;    /* Value�� ���� */
    UInt            mFetchEndOffset; /* ���� ���� fetch */
    UChar         * mValuePtr;       /* Value�� ù ��ġ*/
} sdtSortScanInfo;

/* �ݵ�� ��ϵǾ� �ϴ� �׸��. ( sdtSortTRPHdr���� )
 * mTRFlag, mValueLength, mHashValue, mHitSequence,
 * (1) + (1) + (2) + (4) = 8 */
#define SDT_TR_HEADER_SIZE_BASE ( 8 )

/* �߰��� �ɼų��� �׸�� ( sdtSortTRPHdr���� )
 * Base + mNextGRID + mChildGRID
 * (8) + ( 8 + 8 ) */
#define SDT_TR_HEADER_SIZE_FULL ( SDT_TR_HEADER_SIZE_BASE + 16 )

/* RID�� ������ 24 Byte */
#define SDT_TR_HEADER_SIZE(aFlag)  ( ( (aFlag) & SDT_TRFLAG_GRID )  ?                           \
                                     SDT_TR_HEADER_SIZE_FULL : SDT_TR_HEADER_SIZE_BASE )


#endif /* _O_SDT_SORT_DEF_H_ */
