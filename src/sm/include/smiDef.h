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
 * $Id: smiDef.h 90899 2021-05-27 08:55:20Z jiwon.kim $
 **********************************************************************/

#ifndef _O_SMI_DEF_H_
# define _O_SMI_DEF_H_ 1

# include <idTypes.h>
# include <iduLatch.h>
# include <iduFixedTable.h>
# include <idv.h>
# include <idnCharSet.h>
# include <smuQueueMgr.h>

/* Class ����                                        */
class smiTable;
class smiTrans;
class smiStatement;
class smiTableCursor;

#define SMI_MINIMUM_TABLE_CTL_SIZE (0)
#define SMI_MAXIMUM_TABLE_CTL_SIZE (120)
#define SMI_MINIMUM_INDEX_CTL_SIZE (0)
#define SMI_MAXIMUM_INDEX_CTL_SIZE (50) /* <BUG-48064>
                                           sdnbLKey::mTxInfo[2]�� createCTS/limitCTS index�� ����bit�� 5->6bit�� ������.
                                           ����, CTS�� MAX������ 30->50���� ������Ŵ.
                                           (MAX�� 62������  ������ų�� ������, sdnbBTree::getMaxKeySize()���� CTS MAX���� �����
                                           MAX KEY SIZE�� �����ϱ� ������ ������ ������ ũ�Ⱑ �ǵ��� 50���� ���߾���) */

/* TASK-4990 changing the method of collecting index statistics
 * ���� ������� ���� ��� */
/*  MIN MAX Value �ִ� ���� */
#define SMI_MAX_MINMAX_VALUE_SIZE (40) // �ݵ�� 8byte align�� ����� ��.

/* ���� ��ü Page�� 4�ε� Sampling Percentage�� 10%��, �׷��� � Page��
 * Samplig ���� ���� �� �ִ�. ���� ù �������� ���� ������ Sampling�ǵ���
 * �ʱⰪ�� ���� ��´�. */
#define SMI_STAT_SAMPLING_INITVAL  (0.99f)

#define SMI_STAT_NULL              (ID_SLONG_MAX)
#define SMI_STAT_READTIME_NULL     (-1.0)

typedef struct smiSystemStat
{
    UInt     mCreateTV;                 /*TimeValue */
    SDouble  mSReadTime;                /*Single block read time */
    SDouble  mMReadTime;                /*Milti block read time */
    SLong    mDBFileMultiPageReadCount; /*DB_FILE_MULTIPAGE_READ_COUNT */

    SDouble  mHashTime;          /* ��� hashFunc() ���� �ð� */
    SDouble  mCompareTime;       /* ��� compare() ���� �ð� */
    SDouble  mStoreTime;         /* ��� mem temp table store ���� �ð� */
} smiSystemStat;

typedef struct smiTableStat
{
    UInt     mCreateTV;         /*TimeValue */
    SLong    mNumRowChange;     /*������� ���� ���� Row���� ��ȭ��(I/D)*/
    SFloat   mSampleSize;       /*1~100     */
    SLong    mNumRow;           /*TableRowCount     */
    SLong    mNumPage;          /*Page����          */
    SLong    mAverageRowLen;    /*Record ����       */
    SDouble  mOneRowReadTime;   /*Row �ϳ��� �д� ��� �ð� */

    SLong    mMetaSpace;     /* PageHeader, ExtDir�� Meta ���� */
    SLong    mUsedSpace;     /* ���� ������� ���� */
    SLong    mAgableSpace;   /* ���߿� Aging������ ���� */
    SLong    mFreeSpace;     /* Data������ ������ �� ���� */

    /* ����Ǯ�� �ö�� ���̺� ������ ����.
     * BUG-42095 : ��� �� ��*/
    SLong    mNumCachedPage;
} smiTableStat;

 /* ���̺� ����� �ε��� ����� ���Ե� ����ü
  * �ǽð����� ���̺�(�Ǵ� �ε���) ������ ��踦 �����ϱ� ������ ����� ����
  * ��Ÿ�ӿ��� ���ǹ��� �����̹Ƿ� ����ü�� �ΰ�  �����Ҵ��� ���� �����Ѵ�.
  * ������ �ϳ����ӿ��� �ұ��ϰ� �̷��� �ϴ� ������
  * FSB �� ���� �ٸ� ������Ʈ�� �ǽð� ��������� ���� ������ �и��ؼ� ������ ��
  * �����ϰ� �ذ��� �� �ֵ��� �ϱ� �����̴�.
  *
  * BUG-42095 : PROJ-2281 "buffer pool�� load�� page ��� ���� ����" ����� �����Ѵ�.
  * ����ü �� ���� ������ ���� ���� �ʰ� ��� ���� ������ ������Ʈ �κ��� �����Ѵ�.
  */
typedef struct smiCachedPageStat
{
    SLong  mNumCachedPage; /* ����Ǯ�� �ö�� ���̺� ������ ���� */
} smiCachedPageStat;


typedef struct smiIndexStat
{
    UInt   mCreateTV;      /*TimeValue */
    SFloat mSampleSize;    /*1~100     */
    SLong  mNumPage;       /*Page����  */
    SLong  mAvgSlotCnt;    /*Leaf node�� ��� slot ���� */
    SLong  mClusteringFactor;
    SLong  mIndexHeight;
    SLong  mNumDist;       /*Distinct Value */
    SLong  mKeyCount;      /*Key Count      */

    SLong  mMetaSpace;     /* PageHeader, ExtDir�� Meta ���� */
    SLong  mUsedSpace;     /* ���� ������� ���� */
    SLong  mAgableSpace;   /* ���߿� Aging������ ���� */
    SLong  mFreeSpace;     /* Data������ ������ �� ���� */

    /* mNumCachedPage: ����Ǯ�� �ö�� ���̺� ������ ����.
     * BUG-42095 : ��� ���ϵ��� ����,  
     * BUG-47885 : �ε��� ���̵� + dummy�� ���� */ 
    UInt   mId;
    UInt   mDummy;

    /* �׻� MinMax�� �� �ڿ� �־�� ��.
     * smiStatistics::setIndexStatWithoutMinMax ���� */
    /* BUG-33548   [sm_transaction] The gather table statistics function
     * doesn't consider SigBus error */
    ULong  mMinValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Min�� */
    ULong  mMaxValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Max�� */
} smiIndexStat;

typedef struct smiColumnStat
{
    UInt   mCreateTV;         /*TimeValue */
    SFloat mSampleSize;       /*1~100     */
    SLong  mNumDist;          /*Distinct Value  */
    SLong  mNumNull;          /*NullValue Count */
    SLong  mAverageColumnLen; /*��� ����       */
    /* BUG-33548   [sm_transaction] The gather table statistics function
     * doesn't consider SigBus error */
    ULong  mMinValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Min�� */
    ULong  mMaxValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Max�� */
} smiColumnStat;

// PROJ-2492 Dynamic sample selection
// ��������� ��� ���� �� �� �ִ� �ڷᱸ��
typedef struct smiAllStat
{
    smiTableStat    mTableStat;

    UInt            mColumnCount;
    UInt            mIndexCount;
    smiColumnStat * mColumnStat;
    smiIndexStat  * mIndexStat;
} smiAllStat;

/*
 * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
 */
typedef enum smiExtMgmtType
{
    SMI_EXTENT_MGMT_BITMAP_TYPE = 0,
    SMI_EXTENT_MGMT_NULL_TYPE     = 2,
    SMI_EXTENT_MGMT_MAX
} smiExtMgmtType;

/*
 * ���׸�Ʈ�� �������� ���
 */
typedef enum smiSegMgmtType
{
    SMI_SEGMENT_MGMT_FREELIST_TYPE     = 0,
    SMI_SEGMENT_MGMT_TREELIST_TYPE     = 1,
    SMI_SEGMENT_MGMT_CIRCULARLIST_TYPE = 2,
    SMI_SEGMENT_MGMT_NULL_TYPE         = 3,
    SMI_SEGMENT_MGMT_MAX
} smiSegMgmtType;

/* Segment �Ӽ��� �����Ѵ�. */
typedef struct smiSegAttr
{
    /* PCTFREE reserves space in the data block for updates to existing rows.
       It represents a percentage of the block size. Before reaching PCTFREE,
       the free space is used both for insertion of new rows
       and by the growth of the data block header. */
    UShort   mPctFree;

    /* PCTUSED determines when a block is available for inserting new rows
       after it has become full(reached PCTFREE).
       Until the space used is less then PCTUSED,
       the block is not considered for insertion. */
    UShort   mPctUsed;

    /* �� �������� �ʱ� CTS ���� */
    UShort   mInitTrans;
    /* �� �������� �ִ� CTS ���� */
    UShort   mMaxTrans;
} smiSegAttr;


/*
 * Segment�� STORAGE �Ӽ��� �����Ѵ�.
 * ����� Treelist Managed Segment������ ������.
 */
typedef struct smiSegStorageAttr
{
    /* Segment ������ Extent ���� */
    UInt     mInitExtCnt;
    /* Segment Ȯ��� Extent ���� */
    UInt     mNextExtCnt;
    /* Segment�� �ּ� Extent ���� */
    UInt     mMinExtCnt;
    /* Segment�� �ִ� Extent ���� */
    UInt     mMaxExtCnt;
} smiSegStorageAttr;

/* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
 * Partial Rollback�� �����ؾ� �մϴ�. */

/* Statment�� Depth�� ������ �ִ� �ִ밪 */
#define SMI_STATEMENT_DEPTH_MAX  (255)
/* Statment�� Depth�� �̰��� Depth�� �������� �ȵȴ� */
#define SMI_STATEMENT_DEPTH_NULL (0)

/* FOR A4 : ���̺� �����̽��� Ÿ���� ������ */
typedef enum
{
    SMI_MEMORY_SYSTEM_DICTIONARY = 0,
    SMI_MEMORY_SYSTEM_DATA,
    SMI_MEMORY_USER_DATA,
    SMI_DISK_SYSTEM_DATA,
    SMI_DISK_USER_DATA,
    SMI_DISK_SYSTEM_TEMP,
    SMI_DISK_USER_TEMP,
    SMI_DISK_SYSTEM_UNDO,
    SMI_VOLATILE_USER_DATA,
    SMI_TABLESPACE_TYPE_MAX  //  for function array
} smiTableSpaceType;

// TBS�� ���� ���� ������ ��Ÿ��
// XXX ���� smiTableSpaceType�� ���� bitset����
// ���� ������ ǥ���ϵ��� ����Ǹ� ���� �Ǿ�� ��
typedef enum
{
    SMI_TBS_DISK = 0,
    SMI_TBS_MEMORY,
    SMI_TBS_VOLATILE,
    SMI_TBS_NONE
} smiTBSLocation;

typedef enum
{
    SMI_OFFLINE_NONE = 0,
    SMI_OFFLINE_NORMAL,
    SMI_OFFLINE_IMMEDIATE
} smiOfflineType;

/* --------------------------------------------------------------------
 * Description :
 * [����] removeTableSpace�� removeDataFile�� each ���� �������� ���ϸ�,
 *  createTableSpace �� createDataFiles�� each ���� ���డ���ϴ�.
 *
 * + �������� touch ���
 *    EACH_BYMODE, ALL_NOTOUCH
 *
 * +  ���Ž��� touch ���
 *    ALL_TOUCH, ALL_NOTOUCH
 * ----------------------------------------------------------------- */
typedef enum
{
    SMI_ALL_TOUCH = 0, /* tablespace�� ��� datafile �����
                          datafile�� �ǵ帰��. */
    SMI_ALL_NOTOUCH,   /* tablespace�� ��� datafile �����
                          datafile�� �ǵ帮�� �ʴ´�. */
    SMI_EACH_BYMODE    // �� datafile ����� create ��忡 ������.

} smiTouchMode;

/* ------------------------------------------------
 * media recovery �� restart�� ���������� �ʱ�ȭ flag
 * ----------------------------------------------*/
typedef enum smiRecoverType
{
    SMI_RECOVER_RESTART = 0,
    SMI_RECOVER_COMPLETE,
    SMI_RECOVER_UNTILTIME,
    SMI_RECOVER_UNTILCANCEL,
    SMI_RECOVER_UNTILTAG
} smiRecoverType;

/* ------------------------------------------------
 * PROJ-2133 Incremental backup
 * media restore
 * ----------------------------------------------*/
typedef enum smiRestoreType
{
    SMI_RESTORE_COMPLETE = 0,
    SMI_RESTORE_UNTILTIME,
    SMI_RESTORE_UNTILTAG
} smiRestoreType;

/*-------------------------------------------------------------
 * PROJ-2133 Incremental backup
 * Description :
 * ����� backup level
 *
 * backup level�� backup type���� ����
 * + level0 backup�� backup type:
 *     1) SMI_BACKUP_TYPE_FULL
 *          level0�ΰ�� backup type�� SMI_BACKUP_TYPE_FULL�� �����ϴ�.
 *
 * + level1 backup�� backup type:
 *     1) SMI_BACKUP_TYPE_DIFFERENTIAL
 *          ���ſ� �ش� ������������ level0���� backup������ <�ְ�>
 *          DIFFERENTIAL�� backup�� ������ ���
 *
 *     2) SMI_BACKUP_TYPE_CUMULATIVE
 *          ���ſ� �ش� ������������ level0���� backup������ <�ְ�>
 *          CUMULATIVE�� backup�� ������ ���
 *
 *     3) SMI_BACKUP_TYPE_DIFFERENTIAL & SMI_BACKUP_TYPE_FULL
 *          ���ſ� �ش� ������������ level0���� backup������ <����>
 *          DIFFERENTIAL�� backup�� ������ ���
 *
 *     4) SMI_BACKUP_TYPE_CUMULATIVE & SMI_BACKUP_TYPE_FULL
 *          ���ſ� �ش� ������������ level0���� backup������ <����>
 *          CUMULATIVE�� backup�� ������ ���
 *
 *  3���� 4���� backup type�� level 0����� ����� ���� create TBS�� create
 *  dataFile�� ����Ǿ� ���ο� ������������ �����ͺ��̽��� �߰��Ȱ���̴�.
 *  �� ��� level1 backup�� ����Ȱ�� ���Ӱ� �߰��� ������������ ��ȭ������
 *  ���� base�� ���� ������ full backup�� ����ǰ� changeTracking�� ���۵ǰ�
 *  �ȴ�.
 *-----------------------------------------------------------*/
typedef enum smiBackupLevel
{
    SMI_BACKUP_LEVEL_NONE,
    SMI_BACKUP_LEVEL_0,
    SMI_BACKUP_LEVEL_1
}smiBackuplevel;

#define SMI_BACKUP_TYPE_FULL            ((UShort)(0x01))
#define SMI_BACKUP_TYPE_DIFFERENTIAL    ((UShort)(0X02))
#define SMI_BACKUP_TYPE_CUMULATIVE      ((UShort)(0X04))
#define SMI_MAX_BACKUP_TAG_NAME_LEN     (128)

/* ------------------------------------------------
 * archive �α� ��� Ÿ��
 * ------------------------------------------------ */
typedef enum
{
    SMI_LOG_NOARCHIVE = 0,
    SMI_LOG_ARCHIVE
} smiArchiveMode;

/* ------------------------------------------------
 * TASK-1842 lock type
 * ------------------------------------------------ */
typedef enum
{
    SMI_LOCK_ITEM_NONE = 0,
    SMI_LOCK_ITEM_TABLESPACE,
    SMI_LOCK_ITEM_TABLE
} smiLockItemType;

/*
  Tablespace�� Lock Mode

  Tablespace�� ���� Lock�� ȹ���Ҷ� ����Ѵ�.
 */
typedef enum smiTBSLockMode
{
    SMI_TBS_LOCK_NONE=0,
    SMI_TBS_LOCK_EXCLUSIVE,
    SMI_TBS_LOCK_SHARED
} smiTBSLockMode ;

/* ----------------------------------------------------------------------------
 *   smOID = ( PageID << SM_OFFSET_BIT_SIZE ) | Offset
 * --------------------------------------------------------------------------*/
typedef  vULong  smOID;
typedef  UInt    smTID;


// PRJ-1548 User Memory Tablespace
typedef  UShort     scSpaceID;
typedef  UInt       scPageID;
typedef  UShort     scOffset;
typedef  scOffset   scSlotNum;  /* PROJ-1705 */

typedef enum
{
    SMI_DUMP_TBL = 0,
    SMI_DUMP_IDX,
    SMI_DUMP_LOB,
    SMI_DUMP_TSS,
    SMI_DUMP_UDS
} smiDumpType;

#if defined(SMALL_FOOTPRINT)
#define  SC_MAX_SPACE_COUNT  (32)
#else
/* PROJ-2201
 * SpaceID�� ������ ���� WorkArea������ �����Ѵ�.
 * sdtDef.h�� SDT_SPACEID_WORKAREA, SDT_SPACEID_WAMAP ����
 * BUG-48513 ushort max -2 �� ������ array size �� ushort max -1 */
#define  SC_MAX_SPACE_COUNT  (ID_USHORT_MAX - 1)
/* BUG-48513 3�� �� �߰��ؼ� scSpaceID(short)�� �߸� ���͵�
 * ������ ���Ḧ ���� �ʵ��� �Ѵ�.*/
#define  SC_MAX_SPACE_ARRAY_SIZE  (ID_USHORT_MAX + (UInt)1)
#endif
#define  SC_MAX_PAGE_COUNT   (ID_UINT_MAX)
#define  SC_MAX_OFFSET       (ID_USHORT_MAX)

#define  SC_NULL_SPACEID     ((scSpaceID)(0))
#define  SC_NULL_PID         ((scPageID) (0))
#define  SC_NULL_OFFSET      ((scOffset) (0))
#define  SC_NULL_SLOTNUM     ((scSlotNum)(0))

#define  SC_VIRTUAL_NULL_OFFSET ((scOffset) (0xFFFF))

// PROJ-1705
typedef struct scGRID
{
    scSpaceID  mSpaceID;
    scOffset   mOffset;
    scPageID   mPageID;
} scGRID;

/* PROJ-1789 */
#define SC_GRID_OFFSET_FLAG_MASK      (0x8000)
#define SC_GRID_OFFSET_FLAG_OFFSET    (0x0000)
#define SC_GRID_OFFSET_FLAG_SLOTNUM   (0x8000)

#define SC_GRID_SLOTNUM_MASK      (0x7FFF)

// PRJ-1548 User Memory Tablespace
#define SC_MAKE_GRID(grid, spaceid, pid, offset)     \
    {  (grid).mSpaceID = (spaceid);                  \
       (grid).mPageID = (pid);                       \
       (grid).mOffset = (offset); }

#define SC_MAKE_GRID_WITH_SLOTNUM(grid, spaceid, pid, slotnum)     \
    {  (grid).mSpaceID = (spaceid);                  \
       (grid).mPageID = (pid);                       \
       (grid).mOffset = ((slotnum) | SC_GRID_OFFSET_FLAG_SLOTNUM); }

#define SC_MAKE_NULL_GRID(grid)                       \
    {   (grid).mSpaceID = SC_NULL_SPACEID;            \
        (grid).mPageID  = SC_NULL_PID;                \
        (grid).mOffset  = SC_NULL_OFFSET; }

#define SC_MAKE_VIRTUAL_NULL_GRID_FOR_LOB(grid)      \
    {  (grid).mSpaceID = SC_NULL_SPACEID;            \
        (grid).mPageID  = SC_NULL_PID;               \
       (grid).mOffset  = SC_VIRTUAL_NULL_OFFSET; }

#define SC_COPY_GRID(src_grid, dst_grid)            \
    {   (dst_grid).mSpaceID = (src_grid).mSpaceID;  \
        (dst_grid).mPageID  = (src_grid).mPageID;   \
        (dst_grid).mOffset  = (src_grid).mOffset; }

#define SC_MAKE_SPACE(grid)    ( (grid).mSpaceID )
#define SC_MAKE_PID(grid)      ( (grid).mPageID )

inline static scSlotNum SC_MAKE_OFFSET(scGRID aGRID)
{
    IDE_DASSERT( ((aGRID.mOffset) & SC_GRID_OFFSET_FLAG_MASK) ==
                 (SC_GRID_OFFSET_FLAG_OFFSET) );

    return aGRID.mOffset;
}

inline static scSlotNum SC_MAKE_SLOTNUM(scGRID aGRID)
{
    IDE_DASSERT( ((aGRID.mOffset) & SC_GRID_OFFSET_FLAG_MASK) ==
                 (SC_GRID_OFFSET_FLAG_SLOTNUM) );

    return (aGRID.mOffset) & SC_GRID_SLOTNUM_MASK;
}

#define SC_GRID_IS_EQUAL(grid1, grid2)            \
    (((grid1).mSpaceID == (grid2).mSpaceID) &&    \
     ((grid1).mPageID  == (grid2).mPageID)  &&    \
     ((grid1).mOffset  == (grid2).mOffset))

#define SC_GRID_IS_NOT_EQUAL(grid1, grid2)     \
    (((grid1).mSpaceID != (grid2).mSpaceID) || \
     ((grid1).mPageID  != (grid2).mPageID)  || \
     ((grid1).mOffset  != (grid2).mOffset))

#define SC_GRID_IS_NULL(grid1)                  \
    (((grid1).mSpaceID == SC_NULL_SPACEID)  && \
     ((grid1).mPageID  == SC_NULL_PID)      && \
     ((grid1).mOffset  == SC_NULL_OFFSET))

#define SC_GRID_IS_NOT_NULL(grid1)            \
    (((grid1).mSpaceID != SC_NULL_SPACEID) || \
     ((grid1).mPageID  != SC_NULL_PID)     || \
     ((grid1).mOffset  != SC_NULL_OFFSET))

#define SC_GRID_IS_VIRTUAL_NULL_GRID_FOR_LOB(grid)       \
    ( ((grid).mSpaceID == SC_NULL_SPACEID)  &&           \
      ((grid).mPageID  == SC_NULL_PID    )  &&    \
      ((grid).mOffset  == SC_VIRTUAL_NULL_OFFSET ) )

#define SC_GRID_IS_WITH_SLOTNUM(grid)                        \
    ( ((grid).mOffset & SC_GRID_OFFSET_FLAG_MASK) ==         \
      SC_GRID_OFFSET_FLAG_SLOTNUM )

#define SC_GRID_IS_NOT_WITH_SLOTNUM(grid)                    \
    ( ((grid).mOffset & SC_GRID_OFFSET_FLAG_MASK) !=         \
      SC_GRID_OFFSET_FLAG_SLOTNUM )

// PROJ-1705
#define SMI_MAKE_GRID              SC_MAKE_GRID
#define SMI_MAKE_NULL_GRID         SC_MAKE_NULL_GRID
#define SMI_GRID_IS_NULL           SC_GRID_IS_NULL
#define SMI_MAKE_VIRTUAL_NULL_GRID SC_MAKE_VIRTUAL_NULL_GRID_FOR_LOB
#define SMI_GRID_IS_VIRTUAL_NULL   SC_GRID_IS_VIRTUAL_NULL_GRID_FOR_LOB

extern scGRID gScNullGRID;
#define SC_NULL_GRID gScNullGRID

#define SMI_MAKE_NULL_GRID   SC_MAKE_NULL_GRID

// PROJ-2264
#define SMI_CHANGE_GRID_TO_OID(GRID) SM_MAKE_OID( (GRID).mPageID, (GRID).mOffset )
#define SMI_NULL_OID SM_NULL_OID
#define SMI_INIT_SCN( SCN ) SM_INIT_SCN( SCN )

// PRJ-1671
typedef  UShort   smFileID;  /* SM ���ο����� sdFileID�� �����Ǿ��� */

// PROJ-1362.
typedef  ULong  smLobLocator;
typedef  UInt   smLobCursorID;

typedef  struct  smiSegIDInfo
{
    scSpaceID      mSpaceID;
    void          *mTable;
    smFileID       mFID;
    UInt           mFPID;
    smiDumpType    mType;
} smiSegIDInfo;


/* ----------------------------------------------------------------------------
 *   SCN: (S)YSTEM (C)OMMIT (N)UMBER
 * ---------------------------------------------------------------------------*/
#ifdef COMPILE_64BIT
typedef ULong  smSCN;
#else
typedef struct smSCN
{
    UInt mHigh;
    UInt mLow;
} smSCN;

#endif

typedef struct smLSN
{
    /* �αװ� ��ġ�� File No */
    UInt  mFileNo;
    /* �αװ� ��ġ�� File������ ��ġ */
    UInt  mOffset;
} smLSN;

/* ------------------------------------------------
 * Description : tablespace�� datafile�� �Ӽ� �ڷᱸ��
 *
 * - Log Anchor�� ���̺����̽��� ����Ÿ������
 *   ������ write�ϰų� read�� ��쿡 ���
 *
 *   # ���̺����̽��� ������ ���������� ������
 *     Log Anchor�� ����� �׸�
 *   __________________________
 *   |      ........          |
 *   |________________________| ___
 *   |@ sddTableSpaceAttr     |   |
 *   |____________________+2��|   |
 *   |+ sddDataFileAttr       |   |-- ���̺����̽� ID 1
 *   |________________________|   |
 *   |+ sddDataFileAttr       |   |
 *   |________________________| __|
 *   |@                       |   |
 *   |____________________+3��|   |
 *   |+_______________________|   |-- ���̺����̽� ID 2
 *   |+_______________________|   |
 *   |+_______________________| __|
 *               .
 *               .
 * ----------------------------------------------*/

#define SMI_MAX_TABLESPACE_NAME_LEN    (512)
#define SMI_MAX_DATAFILE_NAME_LEN      (512)
#define SMI_MAX_DWFILE_NAME_LEN        (512)
#define SMI_MAX_CHKPT_PATH_NAME_LEN    (512)
#define SMI_MAX_SBUFFER_FILE_NAME_LEN  (512)
/* --------------------------------------------------------------------
 * Description : create ��� for reuse ����
 * ----------------------------------------------------------------- */
typedef enum
{
    SMI_DATAFILE_REUSE = 0,  // datafile�� �����Ѵ�.
    SMI_DATAFILE_CREATE,     // datafile�� �����Ѵ�.
    SMI_DATAFILE_CREATE_MODE_MAX // smiDataFileMode �� ������ �ִ� �ִ밪
} smiDataFileMode;

/* ------------------------------------------------
 * PRJ-1149 Data File Node�� ����
 * ----------------------------------------------*/
typedef enum smiDataFileState
{
    SMI_FILE_NULL           =0x00000000,
    SMI_FILE_OFFLINE        =0x00000001,
    SMI_FILE_ONLINE         =0x00000002,
    SMI_FILE_BACKUP         =0x00000004,
    SMI_FILE_CREATING       =0x00000010,
    SMI_FILE_DROPPING       =0x00000020,
    SMI_FILE_RESIZING       =0x00000040,
    SMI_FILE_DROPPED        =0x00000080
} smiDataFileState;

#define SMI_FILE_STATE_IS_OFFLINE( aFlag )              \
    (((aFlag) & SMI_FILE_OFFLINE ) == SMI_FILE_OFFLINE )

#define SMI_FILE_STATE_IS_ONLINE( aFlag )               \
    (((aFlag) & SMI_FILE_ONLINE ) == SMI_FILE_ONLINE )

#define SMI_FILE_STATE_IS_BACKUP( aFlag )         \
    (((aFlag) & SMI_FILE_BACKUP ) == SMI_FILE_BACKUP )

#define SMI_FILE_STATE_IS_NOT_BACKUP( aFlag )         \
    (((aFlag) & SMI_FILE_BACKUP ) != SMI_FILE_BACKUP )

#define SMI_FILE_STATE_IS_CREATING( aFlag )             \
    (((aFlag) & SMI_FILE_CREATING ) == SMI_FILE_CREATING )

#define SMI_FILE_STATE_IS_NOT_CREATING( aFlag )             \
    (((aFlag) & SMI_FILE_CREATING ) != SMI_FILE_CREATING )

#define SMI_FILE_STATE_IS_DROPPING( aFlag )             \
    (((aFlag) & SMI_FILE_DROPPING ) == SMI_FILE_DROPPING )

#define SMI_FILE_STATE_IS_NOT_DROPPING( aFlag )             \
    (((aFlag) & SMI_FILE_DROPPING ) != SMI_FILE_DROPPING )

#define SMI_FILE_STATE_IS_RESIZING( aFlag )             \
    (((aFlag) & SMI_FILE_RESIZING ) == SMI_FILE_RESIZING )

#define SMI_FILE_STATE_IS_NOT_RESIZING( aFlag )             \
    (((aFlag) & SMI_FILE_RESIZING ) != SMI_FILE_RESIZING )

#define SMI_FILE_STATE_IS_DROPPED( aFlag )              \
    (((aFlag) & SMI_FILE_DROPPED ) == SMI_FILE_DROPPED )

#define SMI_FILE_STATE_IS_NOT_DROPPED( aFlag )              \
    (((aFlag) & SMI_FILE_DROPPED ) != SMI_FILE_DROPPED )

/*
   fix PR-15004

   1. STATE OF DATAFILE

   OFFLINE              0x01        = 1
   ONLINE               0x02        = 2
   ONLINE|BACKUP        0x02 | 0x04 = 6
   CREATING             0x10        = 16
   ONLINE|DROPPING      0x02 | 0x20 = 34
   ONLINE|RESIZEING     0x02 | 0x40 = 66
   DROPPED              0x40        = 128
*/

// Data File Node state�� ���� �� �ִ� �ִ밪
#define SMI_DATAFILE_STATE_MAX      (0x0000000FF)

// OFFLINE | ONLINE
#define SMI_ONLINE_OFFLINE_MASK     (0x000000003)

/* ------------------------------------------------
 * PRJ-1149, table space�� ����
 * ----------------------------------------------*/
/* �� Enumeration�� ����Ǹ� smmTBSFixedTable.cpp��
   X$MEM_TABLESPACE_STATUS_DESC ���� �ڵ嵵 ����Ǿ�� �� */
typedef enum smiTableSpaceState
{
    /* ����� �� ����. ��� �ؾ���. ����, ����, OFFLINE ���ϼ� �ִ�. */
    SMI_TBS_BLOCK_BACKUP          = 0x10000000,
    SMI_TBS_OFFLINE               = 0x00000001,
    SMI_TBS_ONLINE                = 0x00000002,
    SMI_TBS_BACKUP                = 0x00000004,
    // Tablespace �������߿� �Ͻ������� INCONSISTENT�� ���°� �� �� �ִ�.
    SMI_TBS_INCONSISTENT          = 0x00000008,
    SMI_TBS_CREATING              = 0x00000010 | SMI_TBS_BLOCK_BACKUP ,
    SMI_TBS_DROPPING              = 0x00000020 | SMI_TBS_BLOCK_BACKUP ,
    // Drop Tablespace Transaction�� Commit���� Pending Action������
    SMI_TBS_DROP_PENDING          = 0x00000040,
    SMI_TBS_DROPPED               = 0x00000080,
    // Online->Offline���� ������
    SMI_TBS_SWITCHING_TO_OFFLINE  = 0x00000100 | SMI_TBS_BLOCK_BACKUP ,
    // Offline->Online���� ������
    SMI_TBS_SWITCHING_TO_ONLINE   = 0x00000200 | SMI_TBS_BLOCK_BACKUP ,
    SMI_TBS_DISCARDED             = 0x00000400
} smiTableSpaceState;

# define SMI_TBS_IS_OFFLINE(state) (((state) & SMI_ONLINE_OFFLINE_MASK ) == SMI_TBS_OFFLINE )
# define SMI_TBS_IS_ONLINE(state) (((state) & SMI_ONLINE_OFFLINE_MASK ) == SMI_TBS_ONLINE )
# define SMI_TBS_IS_BACKUP(state) (((state) & SMI_TBS_BACKUP ) == SMI_TBS_BACKUP )
# define SMI_TBS_IS_NOT_BACKUP(state) (((state) & SMI_TBS_BACKUP ) != SMI_TBS_BACKUP )
# define SMI_TBS_IS_CREATING(state) (((state) & SMI_TBS_CREATING ) == SMI_TBS_CREATING )
# define SMI_TBS_IS_DROPPING(state) (((state) & SMI_TBS_DROPPING ) == SMI_TBS_DROPPING )
# define SMI_TBS_IS_DROP_PENDING(state) (((state) & SMI_TBS_DROP_PENDING) == SMI_TBS_DROP_PENDING )
# define SMI_TBS_IS_DROPPED(state) (((state) & SMI_TBS_DROPPED) == SMI_TBS_DROPPED )
# define SMI_TBS_IS_NOT_DROPPED(state) (((state) & SMI_TBS_DROPPED) != SMI_TBS_DROPPED )
# define SMI_TBS_IS_DISCARDED(state) (((state) & SMI_TBS_DISCARDED) == SMI_TBS_DISCARDED )
# define SMI_TBS_IS_NOT_DISCARDED(state) (((state) & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED )


/*ONLINE, BACKUP �̿��� Flag�� ������ ������, offline, dropping ����̴�.*/
# define SMI_TBS_IS_INCOMPLETE(state) ((state) & ~(SMI_TBS_ONLINE|SMI_TBS_BACKUP)) 
# define SMI_TBS_IS_COMPLETE(state)   (!SMI_TBS_IS_INCOMPLETE(state))


/* ���̺����̽��� �Ӽ� �÷���

   �⺻������ ����� ���� ���� Mask���� ��� Bit�� 0���� �����Ѵ�.

 */
// V$TABLESPACES���� ����ڿ��� ���̴� Tablespace�� ���µ�
// XXX ���� ����ȭ �ؼ� ��� ���̵��� �����ؾ� �Ѵ�.
// OFFLINE(0x001) | ONLINE(0x002) | DISCARD(0x400) | DROPPED(0x080)
#define SMI_TBS_STATE_USER_MASK         (0x000000483)
// ���̺����̽����� �����Ϳ� ���� Log�� Compress������ ����
#define SMI_TBS_ATTR_LOG_COMPRESS_MASK  ( 0x00000001 )
// �⺻������ "�α� ����"�� ����ϵ��� �Ѵ�. (0���� ����)
#define SMI_TBS_ATTR_LOG_COMPRESS_TRUE  ( 0x00000000 )
#define SMI_TBS_ATTR_LOG_COMPRESS_FALSE ( 0x00000001 )

/* --------------------------------------------------------------------
 * Description : tablespace ����
 *
 * ID�� �Է°����δ� � �ǹ̸� ������ �ʰ� ���� ��°����θ� ���ȴ�.
 * tablespace create �� �ο��� ���̵� ���ϵȴ�.
 * ----------------------------------------------------------------- */
typedef struct smiDiskTableSpaceAttr
{
    smFileID              mNewFileID;      /* ������ ������ datafile ID */
    UInt                  mExtPageCount;   /* extent�� ũ��(page count) */
    smiExtMgmtType        mExtMgmtType;    /* Extent ������� */
    smiSegMgmtType        mSegMgmtType;    /* Segment ������� */
} smiDiskTableSpaceAttr;

typedef struct smiMemTableSpaceAttr
{
    key_t              mShmKey;
    SInt               mCurrentDB;
    scPageID           mMaxPageCount;
    scPageID           mNextPageCount;
    scPageID           mInitPageCount;
    idBool             mIsAutoExtend;
    UInt               mChkptPathCount;
    scPageID           mSplitFilePageCount;
} smiMemTableSpaceAttr;

typedef struct smiVolTableSpaceAttr
{
    scPageID           mMaxPageCount;
    scPageID           mNextPageCount;
    scPageID           mInitPageCount;
    idBool             mIsAutoExtend;
} smiVolTableSpaceAttr;

/*
  PRJ-1548 User Memory Tablespace

  ��� �Ӽ� Ÿ�� ����

  Loganchor�� �������̿����� �ε��Ҷ�, smiNodeAttrHead��
  ���� �о ���Ӽ��� �˾Ƴ� ��, ���� ��尡 ����� ��������
  �˾Ƴ���.
*/
typedef enum smiNodeAttrType
{
    SMI_TBS_ATTR        = 1,  // ���̺����̽� ���Ӽ�
    SMI_CHKPTPATH_ATTR  = 2,  // CHECKPOINT PATH ���Ӽ�
    SMI_DBF_ATTR        = 3,  // ����Ÿ���� ���Ӽ�
    SMI_CHKPTIMG_ATTR   = 4,  // CHECKPOINT IMAGE ���Ӽ�
    SMI_SBUFFER_ATTR    = 5   // Second Buffer File ��� �Ӽ�
} smiNodeAttrType;

/* --------------------------------------------------------------------
 * Description : tablespace ����
 *
 * ID�� �Է°����δ� � �ǹ̸� ������ �ʰ� ���� ��°����θ� ���ȴ�.
 * tablespace create �� �ο��� ���̵� ���ϵȴ�.
 * ----------------------------------------------------------------- */
typedef struct smiTableSpaceAttr
{
    smiNodeAttrType       mAttrType; // PRJ-1548 �ݵ�� ó���� ����
    scSpaceID             mID;    // ���̺����̽� ���̵�
    // NULL�� ������ ���ڿ�
    SChar                 mName[SMI_MAX_TABLESPACE_NAME_LEN + 1];
    UInt                  mNameLength;

    // ���̺� �����̽� �Ӽ� FLAG
    UInt                  mAttrFlag;

    // ���̺����̽� ����(Creating, Droppping, Online, Offline...)
    // �� ���´� Log Anchor�� ����Ǵ� �����̴�.
    // sctTableSpaceNode.mState�� ȥ���Ͽ� ���� ���� ���� ����
    // �����̸��� LA(Log Anchor)�� ����Ǵ� State���� ����Ͽ���.
    UInt                  mTBSStateOnLA;
    // ���̺����̽� ����
    // User || System, Disk || Memory, Data || Temp || Undo
    smiTableSpaceType     mType;
    // �������� Ÿ��(None, Normal, Immediate)
    smiDiskTableSpaceAttr mDiskAttr;
    smiMemTableSpaceAttr  mMemAttr;
    smiVolTableSpaceAttr  mVolAttr;
} smiTableSpaceAttr;

/* --------------------------------------------------------------------
 * Description : data file�� ����
 *
 * �� size�� tbs Ȯ�� ũ���� ����� align �ȴ�.
 * tbs Ȯ�� ũ��� tablespace�� �ѹ��� �Ҵ��ϴ� extent�� ���� *
 * extent�� ũ���̴�.
 * next size�� tbsȮ�� ũ�⺸�ٴ� Ŀ�� �Ѵ�.
 * createTableSpace�ÿ� init size�� curr size�� ���� �ݵ��
 * ���ƾ� �Ѵ�. extend�� �߻��ϸ� �� �� ���� �޶�����.
 *
 * ----------------------------------------------------------------- */
//incremental backup
typedef struct smiDataFileDescSlotID
{
    /*datafile descriptor slot�� ��ġ�� block ID*/
    UInt             mBlockID;

    /*datafile descriptor block�������� slot index*/
    UInt             mSlotIdx;
}smiDataFileDescSlotID;

typedef struct smiDataFileAttr
{
    smiNodeAttrType       mAttrType;  // PRJ-1548 �ݵ�� ó���� ����
    scSpaceID             mSpaceID;
    SChar                 mName[SMI_MAX_DATAFILE_NAME_LEN];
    UInt                  mNameLength;
    smFileID              mID;

    // unlimited�� ��� 0�� set�Ǹ�
    // system max size�� set�ȴ�.
    ULong                 mMaxSize;      /* datafile�� �ִ� page ���� */
    ULong                 mNextSize;     /* datafile�� Ȯ�� page ���� */
    ULong                 mCurrSize;     /* datafile�� �� page ���� */
    ULong                 mInitSize;     /* datafile�� �ʱ� page ���� */
    idBool                mIsAutoExtend; /* ����Ÿ������ �ڵ�Ȯ�� ���� */
    UInt                  mState;        /* ����Ÿ �����ǻ���       */
    smiDataFileMode       mCreateMode;   /* datafile ����? reuse ? */
    smLSN                 mCreateLSN;    /* ����Ÿ���� ���� LSN */
    //PROJ-2133 incremental backup
    smiDataFileDescSlotID  mDataFileDescSlotID;
} smiDataFileAttr;

/* --------------------------------------------------------------------
 * Description : checkpoint path ����
 * ----------------------------------------------------------------- */
typedef struct smiChkptPathAttr
{
    smiNodeAttrType       mAttrType;
    scSpaceID             mSpaceID; // PRJ-1548 �ݵ�� ó���� ����
    // NULL �� ������ ���ڿ�
    SChar                 mChkptPath[SMI_MAX_CHKPT_PATH_NAME_LEN+1];
} smiChkptPathAttr;


/* ���� ���� smiChkptPathAttr�� �ܹ��� Linked List�� ������ �ڷᱸ�� */
typedef struct smiChkptPathAttrList
{
    smiChkptPathAttr       mCPathAttr;
    smiChkptPathAttrList * mNext;
} smiChkptPathAttrList ;


/**********************************************************************
 * Log anchor file�� ����Ǵ� Secondaty Buffer file �� ����
 **********************************************************************/
typedef struct smiSBufferFileAttr
{
    smiNodeAttrType       mAttrType;     // PRJ-1548 �ݵ�� ó���� ����.
    SChar                 mName[SMI_MAX_SBUFFER_FILE_NAME_LEN];  // File�� ����� ��ġ
    UInt                  mNameLength;   // �̸��� ����
    ULong                 mPageCnt;      // Secondaty Buffer file�� �ִ� page ����
    smiDataFileState      mState;        // File�� ���� (online, offline)
    smLSN                 mCreateLSN;    // ���� LSN
} smiSBufferFileAttr;


/* ------------------------------------------------
 * Description : Reserved ID of tablespace
 *
 * �Ʒ��� �⺻������ �����Ǵ� tablespace�� ���� 0���� 3�� ID����
 * �����ϸ�, �� ���� ID�� user tablespace�� �Ҵ� �����ϴ�.
 * ----------------------------------------------*/
#define SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC      ((scSpaceID)0)
#define SMI_ID_TABLESPACE_SYSTEM_MEMORY_DATA     ((scSpaceID)1)
#define SMI_ID_TABLESPACE_SYSTEM_DISK_DATA       ((scSpaceID)2)
#define SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO       ((scSpaceID)3)
#define SMI_ID_TABLESPACE_SYSTEM_DISK_TEMP       ((scSpaceID)4)

#define SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DIC    "SYS_TBS_MEM_DIC"
#define SMI_TABLESPACE_NAME_SYSTEM_MEMORY_DATA   "SYS_TBS_MEM_DATA"
#define SMI_TABLESPACE_NAME_SYSTEM_DISK_DATA     "SYS_TBS_DISK_DATA"
#define SMI_TABLESPACE_NAME_SYSTEM_DISK_UNDO     "SYS_TBS_DISK_UNDO"
#define SMI_TABLESPACE_NAME_SYSTEM_DISK_TEMP     "SYS_TBS_DISK_TEMP"

/* System Tablespace���� Attribute Flag */
// �������� ���� Dictionary Tablespace�� ���� Log�� �������� �ʴ´�.
#define SMI_TABLESPACE_ATTRFLAG_SYSTEM_MEMORY_DIC \
        ( SMI_TBS_ATTR_LOG_COMPRESS_FALSE )

#define SMI_TABLESPACE_ATTRFLAG_SYSTEM_MEMORY_DATA \
        ( SMI_TBS_ATTR_LOG_COMPRESS_TRUE )

#define SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_DATA \
        ( SMI_TBS_ATTR_LOG_COMPRESS_TRUE )

#define SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_UNDO \
        ( SMI_TBS_ATTR_LOG_COMPRESS_TRUE )

#define SMI_TABLESPACE_ATTRFLAG_SYSTEM_DISK_TEMP \
        ( SMI_TBS_ATTR_LOG_COMPRESS_TRUE )


/* ----------------------------------------------------------------------------
 *   for index
 * --------------------------------------------------------------------------*/
#if defined(SMALL_FOOTPRINT)
#define SMI_MAX_IDX_COLUMNS       (8) // -> from smn (20040715 for QP)
#else
#define SMI_MAX_IDX_COLUMNS       (32) // -> from smn (20040715 for QP)
#endif

/* ----------------------------------------------------------------------------
 *   Global Transaction ID
 * --------------------------------------------------------------------------*/

// For Global Transaction Commit State
typedef enum
{
    SMX_XA_START,
    SMX_XA_PREPARED,
    SMX_XA_COMPLETE
} smiCommitState;

typedef enum
{
    SMI_OBJECT_NONE = 0,
    SMI_OBJECT_PROCEDURE,
    SMI_OBJECT_PACKAGE,
    SMI_OBJECT_VIEW,
    SMI_OBJECT_DATABASE_LINK,
    SMI_OBJECT_TRIGGER
} smiObjectType;

typedef enum
{
    SMI_SELECT_CURSOR = 0,
    SMI_INSERT_CURSOR,
    SMI_UPDATE_CURSOR,
    SMI_DELETE_CURSOR,
    SMI_LOCKROW_CURSOR,  // for Referential constraint check
    // PROJ-1502 PARTITIONED DISK TABLE
    SMI_COMPOSITE_CURSOR
} smiCursorType;


/* ----------------------------------------------------------------------------
 *   PROJ-1362 Large Record & Internal LOB support
 *   LobLocator�� ������ ���� ���ǵȴ�.
 *   [ Shard flag | TransactionID | LobCursorID ] =  smLobLocator
 *      1bit      |  31bit        |  32bit        =  64bit.
 * --------------------------------------------------------------------------*/
#define  SMI_LOB_LOCATOR_SHARD_MASK  (0x80000000)
#define  SMI_LOB_SHARD_FLAG_BIT_SIZE (ID_ULONG(1))
#define  SMI_LOB_SHARD_FLAG_MASK     ((ID_ULONG(1) << SMI_LOB_SHARD_FLAG_BIT_SIZE) - ID_ULONG(1))
#define  SMI_LOB_TRANSID_BIT_SIZE    (ID_ULONG(31))
#define  SMI_LOB_TRANSID_MASK        ((ID_ULONG(1) << SMI_LOB_TRANSID_BIT_SIZE) - ID_ULONG(1))
#define  SMI_LOB_CURSORID_BIT_SIZE   (ID_ULONG(32))
#define  SMI_LOB_CURSORID_MASK       ((ID_ULONG(1) << SMI_LOB_CURSORID_BIT_SIZE) - ID_ULONG(1))

#define  SMI_MAKE_LOB_SHARD_FLAG(locator)                                      \
    ( ((locator) >> (SMI_LOB_CURSORID_BIT_SIZE + SMI_LOB_TRANSID_BIT_SIZE) ) & \
      SMI_LOB_SHARD_FLAG_MASK )

#define  SMI_MAKE_LOB_TRANSID(locator)                                         \
    ( ((locator) >> SMI_LOB_CURSORID_BIT_SIZE) & SMI_LOB_TRANSID_MASK )

#define  SMI_MAKE_LOB_CURSORID(locator) ( (locator) & SMI_LOB_CURSORID_MASK)
#define  SMI_MAKE_LOB_LOCATOR(tid,cursorid) ( ((smLobLocator) (tid) << SMI_LOB_CURSORID_BIT_SIZE) | (cursorid))

#define  SMI_MAKE_SHARD_LOB_LOCATOR(tid,cursorid)                              \
    ( ((smLobLocator) ((tid) | SMI_LOB_LOCATOR_SHARD_MASK)                     \
                       << SMI_LOB_CURSORID_BIT_SIZE) | (cursorid) )

#define  SMI_IS_SHARD_LOB_LOCATOR(locator)                                     \
    (idBool)( ( SMI_MAKE_LOB_SHARD_FLAG((locator)) == 1 ) &&                   \
              ( SMI_IS_NULL_LOCATOR((locator)) == 0 ) )

#define  SMI_NULL_LOCATOR  SMI_MAKE_LOB_LOCATOR(SM_NULL_TID, 0);

#define  SMI_IS_NULL_LOCATOR(locator)                                          \
    ( (SMI_MAKE_LOB_TRANSID((locator)) == SM_NULL_TID) &&                      \
      (((locator) & SMI_LOB_CURSORID_MASK) == 0) )

#define SMI_GET_SESSION_STATISTICS( aTrans ) (((smxTrans*)aTrans)->mStatistics)

typedef enum
{
    SMI_LOB_READ_MODE = 0,
    SMI_LOB_READ_WRITE_MODE,
    SMI_LOB_TABLE_CURSOR_MODE,
    SMI_LOB_READ_LAST_VERSION_MODE  /* not used */
} smiLobCursorMode;

typedef enum
{
    SMI_LOB_COLUMN_TYPE_BLOB = 0,
    SMI_LOB_COLUMN_TYPE_CLOB
} smiLobColumnType;

# define SMI_MAX_NAME_LEN                  (SM_MAX_NAME_LEN)
# define SMI_MAX_PARTKEY_COND_VALUE_LEN    (SM_MAX_PARTKEY_COND_VALUE_LEN)

/* smiColumn.id                                      */
# define SMI_COLUMN_ID_MASK                (0x000003FF)
# define SMI_COLUMN_ID_MAXIMUM                   (1024)
# define SMI_COLUMN_ID_INCREMENT           (0x00000400)

/* smiColumn.flag                                    */
# define SMI_COLUMN_TYPE_MASK              (0x000F0003)
# define SMI_COLUMN_TYPE_FIXED             (0x00000000)
# define SMI_COLUMN_TYPE_VARIABLE          (0x00000001)
# define SMI_COLUMN_TYPE_LOB               (0x00000002)
/* BUG-43840
 * PROJ-2419 UnitedVar ����Ǳ� ������ Variable Ÿ���� LargeVar��
 * ��� �ϱ� ���� Column Type�� LargeVar�� �߰��Ͽ���.
 * Geometry Type �� �׻� LargeVar ���·� ����ȴ�. */
# define SMI_COLUMN_TYPE_VARIABLE_LARGE      (0x00000003)

// PROJ-2362 memory temp ���� ȿ���� ����
// memory temp���� ���Ǵ� column type
# define SMI_COLUMN_TYPE_TEMP_1B           (0x00010000)
# define SMI_COLUMN_TYPE_TEMP_2B           (0x00020000)
# define SMI_COLUMN_TYPE_TEMP_4B           (0x00030000)

/* smiColumn.flag                                    */
// Variable Column�� IN/OUT MODE
# define SMI_COLUMN_MODE_MASK              (0x00000004)
# define SMI_COLUMN_MODE_IN                (0x00000000)
# define SMI_COLUMN_MODE_OUT               (0x00000004)

/* smiColumn.flag - Use for CreateIndex              */
# define SMI_COLUMN_ORDER_MASK             (0x00000008)
# define SMI_COLUMN_ORDER_ASCENDING        (0x00000000)
# define SMI_COLUMN_ORDER_DESCENDING       (0x00000008)

/* smiColumn.flag                                     */
// Column�� ���� ��ü�� ���� ����
# define SMI_COLUMN_STORAGE_MASK            (0x00000010)
# define SMI_COLUMN_STORAGE_MEMORY          (0x00000000)
# define SMI_COLUMN_STORAGE_DISK            (0x00000010)

// PROJ-1705
/* smiColumn.flag - table col or index key            */
# define SMI_COLUMN_USAGE_MASK              (0x00000020)
# define SMI_COLUMN_USAGE_TABLE             (0x00000000)
# define SMI_COLUMN_USAGE_INDEX             (0x00000020)

/* smiColumn.flag-for PROJ-1362 LOB logging or nologging*/
# define SMI_COLUMN_LOGGING_MASK            (0x00000040)
# define SMI_COLUMN_LOGGING                 (0x00000000)
# define SMI_COLUMN_NOLOGGING               (0x00000040)

/* smiColumn.flag-for PROJ-1362 LOB buffer or no buffer*/
# define SMI_COLUMN_USE_BUFFER_MASK         (0x00000080)
# define SMI_COLUMN_USE_BUFFER              (0x00000000)
# define SMI_COLUMN_USE_NOBUFFER            (0x00000080)

/* smiColumn.flag */
/* SM�� �÷��� ���� row piece��
 * ������ �����ص� �Ǵ��� ���θ� ��Ÿ����. */
# define SMI_COLUMN_DATA_STORE_DIVISIBLE_MASK  (0x00000100)
# define SMI_COLUMN_DATA_STORE_DIVISIBLE_FALSE (0x00000000)
# define SMI_COLUMN_DATA_STORE_DIVISIBLE_TRUE  (0x00000100)

// PROJ-1872 Disk index ���� ���� ����ȭ
/* smiColumn.flag */
/* Length�� �˷��� Column����, �ٷ� �ٴ����� ��Ÿ���� */
# define SMI_COLUMN_LENGTH_TYPE_MASK           (0x00000200)
# define SMI_COLUMN_LENGTH_TYPE_KNOWN          (0x00000000)
# define SMI_COLUMN_LENGTH_TYPE_UNKNOWN        (0x00000200)

/* findCompare �Լ����� �ʿ��� flag ���� */
# define SMI_COLUMN_COMPARE_TYPE_MASK          (0x00000C00)
# define SMI_COLUMN_COMPARE_NORMAL             (0x00000000)
# define SMI_COLUMN_COMPARE_KEY_AND_VROW       (0x00000400)
# define SMI_COLUMN_COMPARE_KEY_AND_KEY        (0x00000800)
# define SMI_COLUMN_COMPARE_DIRECT_KEY         (0x00000C00) /* PROJ-2433 */

/* smiColumn.flag */
/* PROJ-1597 Temp record size ��������
   length-unknown type������ �ִ� column precision��ŭ
   ������ Ȯ���س��� �ϴ� �÷����� ǥ��
   �� �Ӽ��� temp table�� aggregation columnó��
   update�� �ʿ��� �÷��鿡 ���� �ʿ��ϴ�.
   SMI_COLUMN_LENGTH_TYPE_UNKNOWN �̸鼭 update�Ǵ� �÷��̸�
   �� �Ӽ��� �Ѿ� �Ѵ�. (temp table���� �����) */
# define SMI_COLUMN_ALLOC_FIXED_SIZE_MASK      (0x00001000)
# define SMI_COLUMN_ALLOC_FIXED_SIZE_FALSE     (0x00000000)
# define SMI_COLUMN_ALLOC_FIXED_SIZE_TRUE      (0x00001000)

// PROJ-2264
// compression column ���θ� �Ǵ��ϵ� �ʿ��� flag ����
# define SMI_COLUMN_COMPRESSION_MASK           (0x00002000)
# define SMI_COLUMN_COMPRESSION_FALSE          (0x00000000)
# define SMI_COLUMN_COMPRESSION_TRUE           (0x00002000)

// PROJ-2429 Dictionary based data compress for on-disk DB
// Dictionary Table�� column�� ��� Ÿ���� table column��
// ���� �����͸� �������ִ����� ��Ÿ����.
# define SMI_COLUMN_COMPRESSION_TARGET_MASK    (0x00008000)
# define SMI_COLUMN_COMPRESSION_TARGET_MEMORY  (0x00000000)
# define SMI_COLUMN_COMPRESSION_TARGET_DISK    (0x00008000)

// PROJ-2362 memory temp ���� ȿ���� ����
// ���� SMI_COLUMN_TYPE_MASK���� (0x000F0000)�� ����ϰ� �ִ�.

/* PROJ-2435 ORDER BY NULLS OPTION
 * Sort Temp ������ ���.
 */
# define SMI_COLUMN_NULLS_ORDER_MASK           (0x00300000)
# define SMI_COLUMN_NULLS_ORDER_NONE           (0x00000000)
# define SMI_COLUMN_NULLS_ORDER_FIRST          (0x00100000)
# define SMI_COLUMN_NULLS_ORDER_LAST           (0x00200000)

/* For table flag */
/* aFlag VON smiTable::createTable                   */
/* aFlag VON smiTable::modifyTableInfo               */
# define SMI_TABLE_FLAG_UNCHANGE           (ID_UINT_MAX)
# define SMI_TABLE_REPLICATION_MASK        (0x00000001)
# define SMI_TABLE_REPLICATION_DISABLE     (0x00000000)
# define SMI_TABLE_REPLICATION_ENABLE      (0x00000001)

# define SMI_TABLE_REPLICATION_TRANS_WAIT_MASK        (0x00000002)
# define SMI_TABLE_REPLICATION_TRANS_WAIT_DISABLE     (0x00000000)
# define SMI_TABLE_REPLICATION_TRANS_WAIT_ENABLE      (0x00000002)

# define SMI_TABLE_LOCK_ESCALATION_MASK    (0x00000010)
# define SMI_TABLE_LOCK_ESCALATION_DISABLE (0x00000000)
# define SMI_TABLE_LOCK_ESCALATION_ENABLE  (0x00000010)

#if 0 // not used
typedef enum smiDMLType
{
    SMI_DML_INSERT,
    SMI_DML_UPDATE,
    SMI_DML_DELETE
} smiDMLType;
#endif

# define SMI_TABLE_TYPE_COUNT              (7)
# define SMI_TABLE_TYPE_TO_ID( aType )     (((aType) & SMI_TABLE_TYPE_MASK) >> 12)
# define SMI_TABLE_TYPE_MASK               (0x0000F000)
# define SMI_TABLE_META                    (0x00000000) // Catalog Tables
# define SMI_TABLE_TEMP_LEGACY             (0x00001000) // Temporary Tables
# define SMI_TABLE_MEMORY                  (0x00002000) // Memory Tables
# define SMI_TABLE_DISK                    (0x00003000) // Disk Tables
# define SMI_TABLE_FIXED                   (0x00004000) // Fixed Tables
# define SMI_TABLE_VOLATILE                (0x00005000) // Volatile Tables
# define SMI_TABLE_REMOTE                  (0x00006000) // Remote Query

/* PROJ-2083 */
/* Dual Table ���� */
# define SMI_TABLE_DUAL_MASK               (0x00000100)
# define SMI_TABLE_DUAL_TRUE               (0x00000000)
# define SMI_TABLE_DUAL_FALSE              (0x00000100)

/* PROJ-1407 Temporary Table */
#define SMI_TABLE_PRIVATE_VOLATILE_MASK    (0x00000200)
#define SMI_TABLE_PRIVATE_VOLATILE_TRUE    (0x00000200)
#define SMI_TABLE_PRIVATE_VOLATILE_FALSE   (0x00000000)

# define SMI_COLUMN_TYPE_IS_TEMP(flag) (((((flag) & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_TEMP_1B) || \
                                         (((flag) & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_TEMP_2B) || \
                                         (((flag) & SMI_COLUMN_TYPE_MASK) == SMI_COLUMN_TYPE_TEMP_4B)) \
                                         ? ID_TRUE : ID_FALSE)

# define SMI_GET_TABLE_TYPE(a)         (((a)->mFlag) & SMI_TABLE_TYPE_MASK)

# define SMI_TABLE_TYPE_IS_META(a)     (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_META) ? ID_TRUE : ID_FALSE)
/* not used
# define SMI_TABLE_TYPE_IS_TEMP(a)     (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_TEMP) ? ID_TRUE : ID_FALSE)
*/
# define SMI_TABLE_TYPE_IS_MEMORY(a)   (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_MEMORY)  ? ID_TRUE : ID_FALSE)

# define SMI_TABLE_TYPE_IS_DISK(a)     (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_DISK) ? ID_TRUE : ID_FALSE)
/* not used
# define SMI_TABLE_TYPE_IS_FIXED(a)    (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_FIXED) ? ID_TRUE : ID_FALSE)
*/
# define SMI_TABLE_TYPE_IS_VOLATILE(a) (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_VOLATILE) ? ID_TRUE : ID_FALSE)
/* not used
# define SMI_TABLE_TYPE_IS_REMOTE(a)   (((((a)->mFlag) & SMI_TABLE_TYPE_MASK) \
                                       == SMI_TABLE_REMOTE) ? ID_TRUE : ID_FALSE)
*/
/* PROJ-1665 */
/* Table���°� Consistent ���� ���� ���� */
/*
# define SMI_TABLE_CONSISTENT_MASK         (0x00010000)
# define SMI_TABLE_CONSISTENT              (0x00000000)
# define SMI_TABLE_INCONSISTENT            (0x00010000)
 * PROJ-2162 Suspended ( => Flag��� IsConsistent�� ���������� ���� )
 */

/* PROJ-1665 */
/* Table Logging ���� ( Direct-Path INSERT �ÿ��� ��ȿ ) */
# define SMI_TABLE_LOGGING_MASK            (0x00020000)
# define SMI_TABLE_LOGGING                 (0x00000000)
# define SMI_TABLE_NOLOGGING               (0x00020000)

/*  Table Type Flag calculation shift right 12 bit   */
# define SMI_SHIFT_TO_TABLE_TYPE           (12)

/* TASK-2398 Log Compress */
# define SMI_TABLE_LOG_COMPRESS_MASK       (0x00040000)
// Mask�� �ش��ϴ� ��� Bit�� 0�� �÷��׸� Default�� ����Ѵ�.
// Default => Table�� �α׸� ���� ( ���� 0 )
# define SMI_TABLE_LOG_COMPRESS_TRUE       (0x00000000)
# define SMI_TABLE_LOG_COMPRESS_FALSE      (0x00040000)


/* TASK-2401 Disk/Memory ���̺��� Log�и�
   Meta Table�� ���� Log Flush���� ����

   LFG=2�� ������ ��� Hybrid Transaction�� Commit�� ���
   Dependent�� LFG�� ���� Flush�� �Ͽ��� �Ѵ�.

   �̶� Disk Table�� Validation�������� Meta Table�� �����ϰ� �Ǿ�
   �׻� Hybrid Transaction���� �з��Ǵ� ������ �ذ��ϱ� ���� Flag�̴�.

   => ������ :
      Disk Table�� ���� DML�� Validation��������
      Table�� Meta Table�� �����ϰ� �ȴ�
      �׷��� Meta Table�� Memory Table�̱� ������,
      Memory Table(Meta)�� �а� Disk Table�� DML�� �ϰ� �Ǹ�
      Hybrid Transaction���� �з��� �Ǿ�
      Memory �αװ� �׻� Flush�Ǵ� ������ �߻��Ѵ�.

      => �ذ�å :
        Meta Table�� �д� ��� Hybrid Transaction���� �з����� �ʴ� ���
        Meta Table�� ����ÿ� Memory Log�� Flush�ϵ��� �Ѵ�.

   => �Ǵٸ� ������ :
      Meta Table�� Replication�� XSN�� ���� DML�� ������� ����ϰ�
      �����Ǵ� Meta Table�� ��� �Ź� Log�� Flush�� ��� �������Ϲ߻�

      => �ذ�å :
         Meta Table���� ������ �Ǿ��� �� Log�� Flush���� ���θ�
         Flag�� �д�.
         �� Flag�� ���� �ִ� ��쿡�� Meta Table������ Transaction��
         Commit�ÿ� �α׸� Flush�ϵ��� �Ѵ�.
 */
# define SMI_TABLE_META_LOG_FLUSH_MASK     (0x00080000)
// �⺻�� ( Flush �ǽ� )�� 0���� ����
# define SMI_TABLE_META_LOG_FLUSH_TRUE     (0x00000000)
# define SMI_TABLE_META_LOG_FLUSH_FALSE    (0x00080000)

/* For Disable All Index */
# define SMI_TABLE_DISABLE_ALL_INDEX_MASK  (0x00100000) // Fixed Tables
# define SMI_TABLE_ENABLE_ALL_INDEX        (0x00000000) // Fixed Tables
# define SMI_TABLE_DISABLE_ALL_INDEX       (0x00100000) // Fixed Tables

// PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
// SUPPLEMENTAL LOGGING�ؾ��ϴ��� ���� �÷���
# define SMI_TABLE_SUPPLEMENTAL_LOGGING_MASK    (0x00200000)
// Mask�� �ش��ϴ� ��� Bit�� 0�� �÷��׸� Default�� ����Ѵ�.
// Default => Table�� ���� SUPPLEMENTAL LOGGING���� ����
# define SMI_TABLE_SUPPLEMENTAL_LOGGING_FALSE   (0x00000000)
# define SMI_TABLE_SUPPLEMENTAL_LOGGING_TRUE    (0x00200000)

// PROJ-2264
// dictionary table ���θ� �Ǵ��ϵ� �ʿ��� flag ����
// debug �뵵�� ����ϴ� flag
// altibase.properties ���� __FORCE_COMPRESSION_COLUMN = 1 �϶� �۵��ϴ� flag
# define SMI_TABLE_DICTIONARY_MASK         (0x00800000)
# define SMI_TABLE_DICTIONARY_FALSE        (0x00000000)
# define SMI_TABLE_DICTIONARY_TRUE         (0x00800000)

/* aFlag VON smiTable::createIndex                   */
# define SMI_INDEX_UNIQUE_MASK             (0x00000001)
# define SMI_INDEX_UNIQUE_DISABLE          (0x00000000)
# define SMI_INDEX_UNIQUE_ENABLE           (0x00000001)

/* aFlag VON smiTable::createIndex                   */
# define SMI_INDEX_TYPE_MASK               (0x00000002)
# define SMI_INDEX_TYPE_NORMAL             (0x00000000)
# define SMI_INDEX_TYPE_PRIMARY_KEY        (0x00000002)

/* aFlag VON smiTable::createIndex                   */
# define SMI_INDEX_PERSISTENT_MASK         (0x00000004)
# define SMI_INDEX_PERSISTENT_DISABLE      (0x00000000)
# define SMI_INDEX_PERSISTENT_ENABLE       (0x00000004)

/* aFlag VON smiTable::createIndex                   */
# define SMI_INDEX_USE_MASK                (0x00000008)
# define SMI_INDEX_USE_ENABLE              (0x00000000)
# define SMI_INDEX_USE_DISABLE             (0x00000008)

/* aFlag VON smiTable::createIndex                   */
// PROJ-1502 PARTITIONED DISK TABLE
# define SMI_INDEX_LOCAL_UNIQUE_MASK       (0x00000020)
# define SMI_INDEX_LOCAL_UNIQUE_DISABLE    (0x00000000)
# define SMI_INDEX_LOCAL_UNIQUE_ENABLE     (0x00000020)

/* PROJ-2433 direct key index ��� flag
 * aFlag VON smiTable::createIndex */
# define SMI_INDEX_DIRECTKEY_MASK          (0x00000040)
# define SMI_INDEX_DIRECTKEY_FALSE         (0x00000000)
# define SMI_INDEX_DIRECTKEY_TRUE          (0x00000040)

/*******************************************************************************
 * Index Bulid Flag
 *
 * - LOGGING, FORCE
 *  disk index�� ���, logging, force�� ���� �ɼ��� �����ؼ� ������ �� �ִµ�,
 * mem, vol index�� ��� logging�̳� force�� ���� �ɼ��� ������� �ʱ� ������
 * CREATE INDEX �������� logging�̳� force �ɼ��� �Է��Ͽ��� ��� ���� ������
 * �Ǵ��Ͽ� ���� �޼����� ��ȯ�Ѵ�.
 * ���� build flag���� logging, force �ɼ��� �ƹ��͵� �������� ���� ����
 * logging, nologging�� ������ ���, force, noforce�� ������ ��찡 ������
 * �����ؾ� �Ѵ�. ���� logging �ɼǰ� nologging �ɼ��� ���ÿ� ������ ��
 * �������� �ұ��ϰ� ������ bit�� ���� �����Ѵ�.
 ******************************************************************************/

/* aBuildFlag VON smiTable::createIndex              */
// PROJ-1469 NOLOGGING INDEX BUILD
# define SMI_INDEX_BUILD_DEFAULT                 (0x00000000)

/* aBuildFlag VON smiTable::createIndex              */
// PROJ-1502 PARTITIONED DISK TABLE
# define SMI_INDEX_BUILD_UNCOMMITTED_ROW_MASK    (0x00000001) /* 00000001 */
# define SMI_INDEX_BUILD_UNCOMMITTED_ROW_DISABLE (0x00000000) /* 00000000 */
# define SMI_INDEX_BUILD_UNCOMMITTED_ROW_ENABLE  (0x00000001) /* 00000001 */

/* aBuildFlag VON smiTable::createIndex              */
// PROJ-1469 NOLOGGING INDEX BUILD
# define SMI_INDEX_BUILD_FORCE_MASK              (0x00000006) /* 00000110 */
# define SMI_INDEX_BUILD_NOFORCE                 (0x00000002) /* 00000010 */
# define SMI_INDEX_BUILD_FORCE                   (0x00000004) /* 00000100 */

/* aBuildFlag VON smiTable::createIndex               */
// PROJ-1469 NOLOGGING INDEX BUILD
# define SMI_INDEX_BUILD_LOGGING_MASK            (0x00000018) /* 00011000 */
# define SMI_INDEX_BUILD_LOGGING                 (0x00000008) /* 00001000 */
# define SMI_INDEX_BUILD_NOLOGGING               (0x00000018) /* 00010000 */

/* aBuildFlag VON smiTable::createIndex               */
// TOP-DOWN, BOTTOM-UP MASK
# define SMI_INDEX_BUILD_FASHION_MASK            (0x00000020) /* 00100000 */
# define SMI_INDEX_BUILD_BOTTOMUP                (0x00000000) /* 00000000 */
# define SMI_INDEX_BUILD_TOPDOWN                 (0x00000020) /* 00100000 */

/* aBuildFlag VON smnManager::enableAllIndex          */
// PROJ-2184 RP Sync ���� ���
# define SMI_INDEX_BUILD_DISK_DEFAULT (                                 \
                    SMI_INDEX_BUILD_DEFAULT                 |           \
                    SMI_INDEX_BUILD_NOFORCE                 |           \
                    SMI_INDEX_BUILD_LOGGING )

/* BUG-44794 �ε��� ����� �ε��� ��� ������ �������� �ʴ� ���� ������Ƽ �߰�
 * smuProperty::getGatherIndexStatOnDDL()
 *  - ID_TRUE: DDL ���� �� Index runtime ��� ����
 *  - ID_FALSE: DDL ���� �� Index runtime ��� ���� ����
 * ENUM
 *  - SMI_INDEX_BUILD_RT_STAT_UPDATE: Index runtime ��� ����
 *  - SMI_INDEX_BUILD_RT_STAT_NO_UPDATE: Index runtime ��� ���� ����
 * SMI_INDEX_BUILD_NEED_RT_STAT :
 *  - Property �� Transaction�� DDL ���ο� ����
 *    Index runtime ��踦 �����ϴ��� ���� ���� �� �׿� �´� __FLAG �� ����
 *  - smuProperty::getGatherIndexStatOnDDL() == ID_TRUE �� ���
 *   + runtime ��� ����
 *  - smuProperty::getGatherIndexStatOnDDL() == ID_FALSE �� ���
 *   + __TX == NULL �� ��� OR __TX->mIsDDL == ID_TRUE �� ���� runtime ��� ���� ����
 *   + �� ���� ��� ( __TX != NULL AND __TX->mIsDDL == ID_FALSE ) runtime ��� ����
 */
# define SMI_INDEX_BUILD_RT_STAT_MASK            (0x00000001) /* 00000001 */
# define SMI_INDEX_BUILD_RT_STAT_NO_UPDATE       (0x00000000) /* 00000000 */
# define SMI_INDEX_BUILD_RT_STAT_UPDATE          (0x00000001) /* 00000001 */

# define SMI_INDEX_BUILD_NEED_RT_STAT( __FLAG, __TX ) \
                    ( ( smuProperty::getGatherIndexStatOnDDL() == ID_TRUE ) \
                      ? ( ( __FLAG ) |= SMI_INDEX_BUILD_RT_STAT_UPDATE ) \
                      : SMI_INDEX_BUILD_RT_STAT_WITHOUT_DDL( ( __FLAG ), ( __TX ) ) )
# define SMI_INDEX_BUILD_RT_STAT_WITHOUT_DDL( __FLAG, __TX ) \
                    ( ( ( __TX ) == NULL || ( __TX )->mIsDDL == ID_TRUE ) \
                        ? ( ( __FLAG ) &= ~SMI_INDEX_BUILD_RT_STAT_UPDATE ) \
                        : ( ( __FLAG ) |= SMI_INDEX_BUILD_RT_STAT_UPDATE ) )


/* aFlag VON smiTrans::begin                         */
# define SMI_ISOLATION_MASK                (0x00000003)
# define SMI_ISOLATION_CONSISTENT          (0x00000000)
# define SMI_ISOLATION_REPEATABLE          (0x00000001)
# define SMI_ISOLATION_NO_PHANTOM          (0x00000002)

/* aFlag VON smiTrans::begin                         */
# define SMI_TRANSACTION_MASK              (0x00000004)
# define SMI_TRANSACTION_NORMAL            (0x00000000)
# define SMI_TRANSACTION_UNTOUCHABLE       (0x00000004)

/* PROJ-1541 smiTrans::begin
 * transaction�� Flag�� Set�Ǹ�, MASK�� ����Ű�� 3 ��Ʈ��
 * �Ʒ��� �� �� �ϳ��� ������ ���� �� ����
 * None�̿��� ���� ��� REPICATION��� Ʈ�������
 *+----------------------------------------------+
 *|TxMode / ReplMode| Lazy    |  Acked |  Eager  |
 *|----------------------------------------------|
 *|Default          | Lazy    |  Acked |  Eager  |
 *|----------------------------------------------|
 *|None(REPLICATED) | N/A     |  N/A   |  N/A    |
 *+----------------------------------------------+
 */
# define SMI_TRANSACTION_REPL_MASK         (0x00000070) //00000001110000
# define SMI_TRANSACTION_REPL_DEFAULT      (0x00000000) //00000000000000
# define SMI_TRANSACTION_REPL_NONE         (0x00000010) //00000000010000
# define SMI_TRANSACTION_REPL_RECOVERY     (0x00000020) //00000000100000
# define SMI_TRANSACTION_REPL_NOT_SUPPORT  (0x00000040) //00000001000000
# define SMI_TRANSACTION_REPL_REPLICATED   SMI_TRANSACTION_REPL_NONE
# define SMI_TRANSACTION_REPL_PROPAGATION  SMI_TRANSACTION_REPL_RECOVERY

/* TASK-6548 duplicated unique key */
# define SMI_TRANSACTION_REPL_CONFLICT_RESOLUTION_MASK (0x00000080) //00000010000000
# define SMI_TRANSACTION_REPL_CONFLICT_RESOLUTION      (0x00000080) //00000010000000

/* PROJ-1381 Fetch Across Commits
 * aFlag for Transaction Reuse */
# define SMI_RELEASE_TRANSACTION           (0x00000000)
# define SMI_DO_NOT_RELEASE_TRANSACTION    (0x00000001)


/* aFlag VON smiTrans::begin                         */
/* BUG-15396 : commit�� log�� disk�� sync�ϴ� ���� ��ٸ����� ���� flag */
# define SMI_COMMIT_WRITE_MASK             (0x00000100)
# define SMI_COMMIT_WRITE_NOWAIT           (0x00000000)
# define SMI_COMMIT_WRITE_WAIT             (0x00000100)

/* aFlag VON smiTrans::begin
 * BUG-33539 : In-place update �� ��������, �� ���� ���� */
# define SMI_TRANS_INPLACE_UPDATE_MASK     (0x00000600) //00011000000000
# define SMI_TRANS_INPLACE_UPDATE_DISABLE  (0x00000200) //00001000000000
# define SMI_TRANS_INPLACE_UPDATE_TRY      (0x00000400) //00010000000000

/* BUG-47472 DBHang ���� ���� ����� �ڵ� */
# define SMI_TRANS_LOCK_DEBUG_INFO_MASK         (0x00000800)
# define SMI_TRANS_LOCK_DEBUG_INFO_ENABLE       (0x00000800)
# define SMI_TRANS_LOCK_DEBUG_INFO_DISABLE      (0x00000000)

# define SMI_IS_LOCK_DEBUG_INFO_ENABLE(aFlag) ( ((aFlag) & SMI_TRANS_LOCK_DEBUG_INFO_MASK)    \
                                                == SMI_TRANS_LOCK_DEBUG_INFO_ENABLE ? ID_TRUE : ID_FALSE )

/* PROJ-2733 GlobalConsistentTx : GLOBAL_TRANSACTION_LEVEL=3 ���� ������ Tx */
# define SMI_TRANS_GCTX_MASK (0x00001000)
# define SMI_TRANS_GCTX_OFF  (0x00000000)
# define SMI_TRANS_GCTX_ON   (0x00001000)

#define SMI_IS_GCTX_ON(aFlag) ( ((aFlag) & SMI_TRANS_GCTX_MASK)    \
                                 == SMI_TRANS_GCTX_OFF ? ID_FALSE : ID_TRUE )

/* aFlag VON smiStatement::begin                     */
// PROJ-2199 SELECT func() FOR UPDATE ����
// SMI_STATEMENT_FORUPDATE �߰�
# define SMI_STATEMENT_MASK                (0x0000000C)
# define SMI_STATEMENT_NORMAL              (0x00000000)
# define SMI_STATEMENT_UNTOUCHABLE         (0x00000004)
# define SMI_STATEMENT_FORUPDATE           (0x00000008)

/* aFlag VON smiStatement::begin                     */
/* FOR A4 : Memory or Disk or Either                 */
# define SMI_STATEMENT_CURSOR_MASK         (0x00000003)
# define SMI_STATEMENT_CURSOR_NONE         (0x00000000)
# define SMI_STATEMENT_MEMORY_CURSOR       (0x00000001)
# define SMI_STATEMENT_DISK_CURSOR         (0x00000002)
# define SMI_STATEMENT_ALL_CURSOR          (SMI_STATEMENT_MEMORY_CURSOR |\
                                            SMI_STATEMENT_DISK_CURSOR)

/* foreign key�� statement::begin */
# define SMI_STATEMENT_SELF_MASK           (0x00000020)
# define SMI_STATEMENT_SELF_FALSE          (0x00000000)
# define SMI_STATEMENT_SELF_TRUE           (0x00000020)

/* PROJ-2733 �л� Ʈ����� ���ռ�
 * aFlag smiStatement::begin */ 
# define SMI_STATEMENT_VIEWSCN_MASK        (0x00000040)
# define SMI_STATEMENT_VIEWSCN_LASTEST     (0x00000000)  // �ֽź並 ����.
# define SMI_STATEMENT_VIEWSCN_REQUESTED   (0x00000040)  // �䱸�� SCN �� �̿��Ѵ�. 

# define SMI_STATEMENT_VIEWSCN_IS_REQUESTED(aFlag) ( ((aFlag) & SMI_STATEMENT_VIEWSCN_MASK) \
                                                     == SMI_STATEMENT_VIEWSCN_LASTEST ? ID_FALSE : ID_TRUE )

/* aFlag VON smiTrans::commit or smiStatement::open  */
# define SMI_STATEMENT_LEGACY_MASK         (0x00000010)
# define SMI_STATEMENT_LEGACY_NONE         (0x00000000)
# define SMI_STATEMENT_LEGACY_TRUE         (0x00000010)

/* aFlag VON smiStatement::end                       */
# define SMI_STATEMENT_RESULT_MASK         (0x00000001)
# define SMI_STATEMENT_RESULT_SUCCESS      (0x00000000)
# define SMI_STATEMENT_RESULT_FAILURE      (0x00000001)

/* aFlag VON smiTableCursor::open                    */
# define SMI_LOCK_MASK                     (0x00000007)
# define SMI_LOCK_READ                     (0x00000000)
# define SMI_LOCK_WRITE                    (0x00000001)
# define SMI_LOCK_REPEATABLE               (0x00000002)
# define SMI_LOCK_TABLE_SHARED             (0x00000003)
# define SMI_LOCK_TABLE_EXCLUSIVE          (0x00000004)

/* aFlag VON smiTableCursor::open                    */
# define SMI_PREVIOUS_MASK                 (0x00000008)
# define SMI_PREVIOUS_DISABLE              (0x00000000)
# define SMI_PREVIOUS_ENABLE               (0x00000008)

/* aFlag VON smiTableCursor::open                    */
# define SMI_TRAVERSE_MASK                 (0x00000010)
# define SMI_TRAVERSE_FORWARD              (0x00000000)
# define SMI_TRAVERSE_BACKWARD             (0x00000010)

/* aFlag VON smiTableCursor::open                    */
# define SMI_INPLACE_UPDATE_MASK           (0x00000020)
# define SMI_INPLACE_UPDATE_ENABLE         (0x00000000)
# define SMI_INPLACE_UPDATE_DISABLE        (0x00000020)

/* aFlag VON smiTableCursor::open    ( BUG-47758 )   */
# define SMI_TRANS_ISOLATION_IGNORE        (0x00000040)

/* aFlag VON smiTableCursor::readOldRow/readNewRow   */
# define SMI_FIND_MODIFIED_MASK            (0x00000300)
# define SMI_FIND_MODIFIED_NONE            (0x00000000)
# define SMI_FIND_MODIFIED_OLD             (0x00000100)
# define SMI_FIND_MODIFIED_NEW             (0x00000200)

/* aFlag VON smiTableCursor::readOldRow/readNewRow   */
/* readOldRow()/readNewRow() ���� ��, ���� �а� �ִ� undo page list */
# define SMI_READ_UNDO_PAGE_LIST_MASK      (0x00000C00)
# define SMI_READ_UNDO_PAGE_LIST_NONE      (0x00000000)
# define SMI_READ_UNDO_PAGE_LIST_INSERT    (0x00000400)
# define SMI_READ_UNDO_PAGE_LIST_UPDATE    (0x00000800)

/* PROJ-1566 */
/* smiTableCursor::mFlag */
# define SMI_INSERT_METHOD_MASK            (0x00001000)
# define SMI_INSERT_METHOD_NORMAL          (0x00000000)
# define SMI_INSERT_METHOD_APPEND          (0x00001000)

#if 0
/* Proj-2059
 * Lob Nologging ���� */
/* smiTableCursor::mFlag */
# define SMI_INSERT_LOBLOGGING_MASK        (0x00002000)
# define SMI_INSERT_LOBLOGGING_ENABLE      (0x00000000)
# define SMI_INSERT_LOBLOGGING_DISABLE     (0x00002000)
#endif

/* aFlag VON smiTableCursor::readRow                 */
# define SMI_FIND_MASK                     (0x00000003)
# define SMI_FIND_NEXT                     (0x00000000)
# define SMI_FIND_PREV                     (0x00000001)
# define SMI_FIND_CURR                     (0x00000002)
# define SMI_FIND_BEFORE                   (0x00000002)
# define SMI_FIND_AFTER                    (0x00000003)
# define SMI_FIND_RETRAVERSE_NEXT          (0x00000004)
# define SMI_FIND_RETRAVERSE_BEFORE        (0x00000005)

/* aFlag VON Emergency State                 */
# define SMI_EMERGENCY_MASK                (0x00000003)
# define SMI_EMERGENCY_DB_MASK             (0x00000001)
# define SMI_EMERGENCY_LOG_MASK            (0x00000002)
# define SMI_EMERGENCY_DB_SET              (0x00000001)
# define SMI_EMERGENCY_LOG_SET             (0x00000002)
# define SMI_EMERGENCY_DB_CLR              (0xFFFFFFFE)
# define SMI_EMERGENCY_LOG_CLR             (0xFFFFFFFD)

/* iterator state flag */
# define SMI_RETRAVERSE_MASK               (0x00000003)
# define SMI_RETRAVERSE_BEFORE             (0x00000000)
# define SMI_RETRAVERSE_NEXT               (0x00000001)
# define SMI_RETRAVERSE_AFTER              (0x00000002)

# define SMI_ITERATOR_TYPE_MASK            (0x00000010)
# define SMI_ITERATOR_WRITE                (0x00000000)
# define SMI_ITERATOR_READ                 (0x00000010)

/* sequence read flag */
# define SMI_SEQUENCE_MASK                 (0x00000010)
# define SMI_SEQUENCE_CURR                 (0x00000000)
# define SMI_SEQUENCE_NEXT                 (0x00000010)

/* sequence circular flag */
# define SMI_SEQUENCE_CIRCULAR_MASK        (0x00000010)
# define SMI_SEQUENCE_CIRCULAR_DISABLE     (0x00000000)
# define SMI_SEQUENCE_CIRCULAR_ENABLE      (0x00000010)

/* PROJ-2365 sequence table */
/* sequence table flag */
# define SMI_SEQUENCE_TABLE_MASK           (0x00000020)
# define SMI_SEQUENCE_TABLE_FALSE          (0x00000000)
# define SMI_SEQUENCE_TABLE_TRUE           (0x00000020)

/* TASK-7217 Sharded sequence */
/* sequence locality flag */
# define SMI_SEQUENCE_LOCALITY_MASK        (0x00000700)
# define SMI_SEQUENCE_LOCALITY_LOCAL       (0x00000100)
# define SMI_SEQUENCE_LOCALITY_SHARD       (0x00000200)
# define SMI_SEQUENCE_LOCALITY_GLOBAL      (0x00000400)  // UNUSED

/* TASK-7217 Sharded sequence */
/* sequence scale of maxvalue for sharded sequence */
# define SMI_SEQUENCE_SCALE_MASK           (0x0000000F)

/* TASK-7217 Sharded sequence */
# define SMI_SEQUENCE_SCALE_FIXED_MASK     (0x00001000)
# define SMI_SEQUENCE_SCALE_FIXED_FALSE    (0x00000000)  // VARIABLE
# define SMI_SEQUENCE_SCALE_FIXED_TRUE     (0x00001000)

/* smiTimeStamp flag */
# define SMI_TIMESTAMP_RETRAVERSE_MASK     (0x00000001)
# define SMI_TIMESTAMP_RETRAVERSE_DISABLE  (0x00000000)
# define SMI_TIMESTAMP_RETRAVERSE_ENABLE   (0x00000001)

/* FOR A4 : DROP Table Space flag */
# define SMI_DROP_TBLSPACE_MASK            (0x00000111)
# define SMI_DROP_TBLSPACE_ONLY            (0x00000000)
# define SMI_DROP_TBLSPACE_CONTENTS        (0x00000001)
# define SMI_DROP_TBLSPACE_DATAFILES       (0x00000010)
# define SMI_DROP_TBLSPACE_CONSTRAINT      (0x00000100)


/* FOR A4 : Startup Phase�� �����Ҷ� ACTION  flag */
# define SMI_STARTUP_ACTION_MASK            (0x00001111)
# define SMI_STARTUP_NOACTION               (0x00000000)
# define SMI_STARTUP_NORESETLOGS            (0x00000000)
# define SMI_STARTUP_RESETLOGS              (0x00000001)
# define SMI_STARTUP_NORESETUNDO            (0x00000000)
# define SMI_STARTUP_RESETUNDO              (0x00000010)


/* RP2 smiLogRecord usage defined */
#define SMI_LOG_TYPE_MASK           SMR_LOG_TYPE_MASK
#define SMI_LOG_TYPE_NORMAL         SMR_LOG_TYPE_NORMAL
#define SMI_LOG_TYPE_REPLICATED      SMR_LOG_TYPE_REPLICATED
#define SMI_LOG_TYPE_REPL_RECOVERY  SMR_LOG_TYPE_REPL_RECOVERY

#define SMI_LOG_SAVEPOINT_MASK      SMR_LOG_SAVEPOINT_MASK
#define SMI_LOG_SAVEPOINT_NO        SMR_LOG_SAVEPOINT_NO
#define SMI_LOG_SAVEPOINT_OK        SMR_LOG_SAVEPOINT_OK

#define SMI_LOG_BEGINTRANS_MASK     SMR_LOG_BEGINTRANS_MASK
#define SMI_LOG_BEGINTRANS_NO       SMR_LOG_BEGINTRANS_NO
#define SMI_LOG_BEGINTRANS_OK       SMR_LOG_BEGINTRANS_OK

#define SMI_LOG_CONTINUE_MASK       (0x000000200)
#define SMI_LOG_COMMIT_MASK         (0x000000100)

// BUGBUG mtcDef.h�� �����ϰ� �ϰ� �Ѵ�.
// MTD_OFFSET_USELESS,MTD_OFFSET_USE
# define SMI_OFFSET_USELESS                (0x00000001)
# define SMI_OFFSET_USE                    (0x00000000)

/* PROJ-2433 Direct Key Index
 * partail direct key �ΰ�� ���õȴ�.
 * MTD_PARTIAL_KEY_ON, MTD_PARTIAL_KEY_OFF�� �����ϰ� �ؾ� �Ѵ�. */
# define SMI_PARTIAL_KEY_MASK              (0x00000002)
# define SMI_PARTIAL_KEY_ON                (0x00000002)
# define SMI_PARTIAL_KEY_OFF               (0x00000000)

/*
 * BUG-17123 [PRJ-1548] offline�� TableSpace�� ���ؼ� ����Ÿ������
 *           �����ϴٰ� Error �߻��Ͽ� diff
 *
 * DML DDL�� Validation, Execution�ÿ�
 * ���̺����̽��� ���� lock validation�� �ϱ� ���ؼ�
 * ������ ���� LV Option Type�� �Է��ؾ��Ѵ�.
 */
typedef enum smiTBSLockValidType
{
    SMI_TBSLV_NONE = 0,
    SMI_TBSLV_DDL_DML,  // OLINE TBS�� Lock ȹ�� ����
    SMI_TBSLV_DROP_TBS, // ONLINE/OFFLINE/DISCARDED TBS Lock ȹ�� ����
    SMI_TBSLV_OPER_MAXMAX
} smiTBSLockValidType;

typedef struct smiValue
{
    UInt        length;
    const void* value;
} smiValue;


typedef struct smiColumn
{
    UInt         id;
    UInt         flag;
    UInt         offset;
    UInt         varOrder;      /* Column �� variable �÷��� ���� ���� */
    /* Variable Column����Ÿ�� In, Out���� ������� �����ϴ� ����
       �μ� if Variable column length <= vcInOutBaseSize, in-mode,
       else out-mode */
    UInt         vcInOutBaseSize;
    UInt         size;
    UShort       align;         /* BUG-43117 */
    UShort       maxAlign;      /* BUG-43287 smiColumn List�� Variable Column �� ���� ū align �� */
    void       * value;

    /*
     * PROJ-1362 LOB, LOB column������ �ǹ��ִ�.
     *
     * Column�� ����� Tablespace�� ID
     * Memory Table : Table�� ���� SpaceID�� �׻� ����
     * Disk Table   : LOB�� ��� Table�� ���� SpaceID��
     *                �ٸ� Tablespace�� ����� �� �����Ƿ�
     *                Table �� Space ID�� �ٸ� ���� �ɼ��ִ�
     */
    scSpaceID     colSpace;
    scGRID        colSeg;
    UInt          colType; /* PROJ-2047 Strengthening LOB (CLOB or BLOB) */
    void        * descSeg; /* Disk Lob Segment�� ���� ����� */

    smiColumnStat mStat;
    // PROJ-2264
    smOID         mDictionaryTableOID;
} smiColumn;

// BUG-30711
// ALTER TABLE ... MODIFY COLUMN ... ���� �ÿ�
// �ٲ��� �ʴ� ������ �������ٶ� �����
#define SMI_COLUMN_LOB_INFO_COPY(  _dst_, _src_ )\
{                                                \
    _dst_->colSpace  = _src_->colSpace;          \
    _dst_->colSeg    = _src_->colSeg;            \
    _dst_->descSeg   = _src_->descSeg;           \
}

typedef struct smiColumnList
{
    smiColumnList*   next;
    const smiColumn* column;
}
smiColumnList;

// PROJ-1705
typedef struct smiFetchColumnList
{
    const smiColumn    *column;
    UShort              columnSeq;
    void               *copyDiskColumn;
    smiFetchColumnList *next;
} smiFetchColumnList;

#define SMI_GET_COLUMN_SEQ(aColumn) \
            SDC_GET_COLUMN_SEQ(aColumn)

/* TASK-5030 Full XLog
 * MRDB DML���� column list�� sort�ϱ� ���� ����ü */
typedef struct smiUpdateColumnList
{
    const smiColumn   * column;
    UInt                position;
} smiUpdateColumnList;

typedef IDE_RC (*smiCallBackFunc)( idBool     * aResult,
                                   const void * aRow,
                                   void       * aDirectKey,             /* PROJ-2433 direct key index */
                                   UInt         aDirectKeyPartialSize,  /* PROJ-2433 */
                                   const scGRID aGRID,
                                   void       * aData );

typedef struct smiCallBack
{
    // For A4 : Hash Index�� ��� Hash Value�� ���
    UInt            mHashVal; // Hash Value
    smiCallBackFunc callback;
    void*           data;
}
smiCallBack;

typedef struct smiRange
{
    idvSQL  *   statistics;
    smiRange*   prev;
    smiRange*   next;

    // For A4 : Hash Index�� ��� hash value�� ������.
    smiCallBack minimum;
    smiCallBack maximum;
}
smiRange;


/*
 * for database link
 */
typedef struct smiRemoteTableColumn
{
    SChar * mName;
    UInt mTypeId;       /* MTD_xxx_ID */
    SInt mPrecision;
    SInt mScale;
} smiRemoteTableColumn;

typedef struct smiRemoteTable
{
    idBool  mIsStore;
    SChar * mLinkName;
    SChar * mRemoteQuery;

    SInt mColumnCount;
    smiRemoteTableColumn * mColumns;
} smiRemoteTable;

typedef struct smiRemoteTableParam
{
    void * mQcStatement;
    void * mDkiSession;
} smiRemoteTableParam;


/*
 * PROJ-1784 DML without retry
 * fetch�� � ������ row�� ���� �� �� ����
 */
typedef enum
{
    SMI_FETCH_VERSION_CONSISTENT,// ���� view�� row�� �о�´�.
    SMI_FETCH_VERSION_LAST,      // �ֽ� row�� �о�´�.
    SMI_FETCH_VERSION_LASTPREV   // �ֽ� row�� �ٷ� �� ������ �д´�.
                                 //   index old key�� �����ϱ� ���� �ʿ�
} smFetchVersion;

/* PROJ-1784 DML without retry
 *  retry ������ �Ǵ��ϱ� ���� ����
 *  QP���� �����ϰ� SM���� ��� */
typedef struct smiDMLRetryInfo
{
    idBool                mIsWithoutRetry;   // QP�� retry info on/off flag
    idBool                mIsRowRetry;       // row retry���� ����
    const smiColumnList * mStmtRetryColLst;  // statement retry �Ǵ��� ���� column list
    const smiValue      * mStmtRetryValLst;  // statement retry �Ǵ��� ���� value list
    const smiColumnList * mRowRetryColLst;   // row retry �Ǵ��� ���� column list
    const smiValue      * mRowRetryValLst;   // row retry �Ǵ��� ���� value list

}smiDMLRetryInfo;

// PROJ-2402
typedef struct smiParallelReadProperties
{
    ULong      mThreadCnt;
    ULong      mThreadID;
    UInt       mParallelReadGroupID;
} smiParallelReadProperties;

/* FOR A4 : smiCursorProperties
   smiCursor::open �Լ��� ���ڸ� ���̱����� �߰���.
   ���߿� Cursor���� ��� �߰��ÿ� �� ����ü�� ����� �߰�
*/
typedef struct smiCursorProperties
{
    ULong                  mLockWaitMicroSec;
    ULong                  mFirstReadRecordPos;
    ULong                  mReadRecordCount;
    // PROJ-1502 PARTITIONED DISK TABLE
    idBool                 mIsUndoLogging;
    idvSQL                *mStatistics;

    /* for remote table */
    smiRemoteTable       * mRemoteTable;
    smiRemoteTableParam    mRemoteTableParam;

    // PROJ-1665
    UInt                   mHintParallelDegree;

    // PROJ-1705
    smiFetchColumnList    *mFetchColumnList;   // ��ġ�� ���簡 �ʿ��� �÷�����Ʈ����
    UChar                 *mLockRowBuffer;
    UInt                   mLockRowBufferSize;

    // PROJ-2113
    UChar                  mIndexTypeID;

    // PROJ-2402
    smiParallelReadProperties mParallelReadProperties;

} smiCursorProperties;

// smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, aIndexType) \
    (aProp)->mLockWaitMicroSec = ID_ULONG_MAX;     \
    (aProp)->mFirstReadRecordPos = 0;              \
    (aProp)->mReadRecordCount = ID_ULONG_MAX;      \
    (aProp)->mIsUndoLogging = ID_TRUE;             \
    (aProp)->mStatistics = aStat;                  \
    (aProp)->mRemoteTable = NULL;                  \
    (aProp)->mHintParallelDegree = 0;              \
    (aProp)->mFetchColumnList = NULL;              \
    (aProp)->mLockRowBuffer = NULL;                \
    (aProp)->mLockRowBufferSize = 0;               \
    (aProp)->mIndexTypeID = aIndexType;            \
    (aProp)->mParallelReadProperties.mThreadCnt = 1; \
    (aProp)->mParallelReadProperties.mThreadID  = 1; \
    (aProp)->mParallelReadProperties.mParallelReadGroupID = 0;

// qp meta table�� full scan�� ���Ͽ� smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT_FOR_META_FULL_SCAN(aProp, aStat) \
    SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID)

// qp meta table�� index scan�� ���Ͽ� smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT_FOR_META_INDEX_SCAN(aProp, aStat) \
    SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, SMI_BUILTIN_B_TREE_INDEXTYPE_ID)

// qp table�� full scan�� ���Ͽ� smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT_FOR_FULL_SCAN(aProp, aStat) \
    SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID)

// qp table�� index scan�� ���Ͽ� smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT_FOR_INDEX_SCAN(aProp, aStat, aIndexType) \
    SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, aIndexType)

// index handle�� �̿��Ͽ� smiCursorProperties�� �ʱ�ȭ�Ѵ�.
#define SMI_CURSOR_PROP_INIT(aProp, aStat, aIndex) \
    SMI_CURSOR_PROP_INIT_WITH_TYPE(aProp, aStat, smiGetIndexType(aIndex))

typedef struct smiIterator
{
    smSCN       SCN;
    smSCN       infinite;
    void*       trans;
    void*       table;
    SChar*      curRecPtr;
    SChar*      lstFetchRecPtr;
    scGRID      mRowGRID;
    smTID       tid;
    UInt        flag;

    smiCursorProperties  * properties;
    smiStatement         * mStatement;
} smiIterator;

//----------------------------
// PROJ-1872
// compare �Ҷ� �ʿ��� ����
//----------------------------
typedef struct smiValueInfo
{
    const smiColumn * column;
    const void      * value;
    UInt              length;
    UInt              flag;
} smiValueInfo;

#define SMI_SET_VALUEINFO( aValueInfo1, aColumn1, aValue1, aLength1, aFlag1, \
                           aValueInfo2, aColumn2, aValue2, aLength2, aFlag2) \
    (aValueInfo1)->column = (aColumn1);                                      \
    (aValueInfo1)->value  = (aValue1);                                       \
    (aValueInfo1)->length = (aLength1);                             \
    (aValueInfo1)->flag   = (aFlag1);                               \
    (aValueInfo2)->column = (aColumn2);                             \
    (aValueInfo2)->value  = (aValue2);                              \
    (aValueInfo2)->length = (aLength2);                             \
    (aValueInfo2)->flag   = (aFlag2);

/* BUG-42639 Monitoring query */
typedef struct smiFixedTableProperties
{
    ULong         mFirstReadRecordPos;
    ULong         mReadRecordCount;
    ULong         mReadRecordPos;
    idvSQL      * mStatistics;
    smiCallBack * mFilter;
    void        * mTrans;
    smiRange    * mKeyRange;
    smiColumn   * mMinColumn;
    smiColumn   * mMaxColumn;
} smiFixedTableProperties;

#define SMI_FIXED_TABLE_PROPERTIES_INIT( aProp ) \
        ( aProp )->mFirstReadRecordPos = 0;      \
        ( aProp )->mReadRecordCount    = 0;      \
        ( aProp )->mReadRecordPos      = 0;      \
        ( aProp )->mStatistics         = NULL;   \
        ( aProp )->mFilter             = NULL;   \
        ( aProp )->mTrans              = NULL;   \
        ( aProp )->mKeyRange           = NULL;   \
        ( aProp )->mMinColumn          = NULL;   \
        ( aProp )->mMaxColumn          = NULL;


typedef SInt (*smiCompareFunc)( smiValueInfo * aValueInfo1,
                                smiValueInfo * aValueInfo2 );

// PROJ-1618
typedef IDE_RC (*smiKey2StringFunc)( smiColumn  * aColumn,
                                     void       * aValue,
                                     UInt         aValueSize,
                                     UChar      * aCompileFmt,
                                     UInt         aCompileFmtLen,
                                     UChar      * aText,
                                     UInt       * aTextLen,
                                     IDE_RC     * aRet );

// PROJ-1629
typedef void (*smiNullFunc)( const smiColumn* aColumn,
                             const void*      aRow );

typedef void (*smiPartialKeyFunc)( ULong*           aPartialKey,
                                   const smiColumn* aColumn,
                                   const void*      aRow);

typedef UInt (*smiHashKeyFunc) (UInt              aHash,
                                const smiColumn * aColumn,
                                const void      * aRow );

typedef idBool (*smiIsNullFunc)( const smiColumn* aColumn,
                                 const void*      aRow );

// PROJ-1705
typedef IDE_RC (*smiCopyDiskColumnValueFunc)( UInt              aColumnSize,
                                              void            * aDestValue,
                                              UInt              aDestValueOffset,
                                              UInt              aLength,
                                              const void      * aValue );

// aColumn�� �ݵ�� NULL�� �ѱ��,
// aRow�� �ش� �÷��� value pointer�� ����Ű��,
// aFlag�� 1 ( MTD_OFFSET_USELESS ) �� �ѱ⵵�� �Ѵ�.
typedef  UInt (*smiActualSizeFunc)( const smiColumn* aColumn,
                                    const void*      aRow );

typedef IDE_RC (*smiFindCompareFunc)( const smiColumn* aColumn,
                                      UInt             aFlag,
                                      smiCompareFunc*  aCompare );

// PROJ-1618
typedef IDE_RC (*smiFindKey2StringFunc)( const smiColumn  * aColumn,
                                         UInt               aFlag,
                                         smiKey2StringFunc* aKey2String );

// PROJ-1629
typedef IDE_RC (*smiFindNullFunc)( const smiColumn*   aColumn,
                                   UInt               aFlag,
                                   smiNullFunc*       aNull );

// PROJ-1705
typedef IDE_RC (*smiFindCopyDiskColumnValueFunc)(
    const smiColumn            * aColumn,
    smiCopyDiskColumnValueFunc * aCopyDiskColumnValueFunc );

// PROJ-2429
// smiStatistics.cpp ������ ���ȴ�.
typedef IDE_RC (*smiFindCopyDiskColumnValue4DataTypeFunc)(
    const smiColumn            * aColumn,
    smiCopyDiskColumnValueFunc * aCopyDiskColumnValueFunc );

typedef IDE_RC (*smiFindActualSizeFunc)( const smiColumn   * aColumn,
                                         smiActualSizeFunc * aActualSizeFunc );

typedef IDE_RC (*smiFindPartialKeyFunc)( const smiColumn*   aColumn,
                                         UInt               aFlag,
                                         smiPartialKeyFunc* aPartialKey );

typedef IDE_RC (*smiFindHashKeyFunc)( const smiColumn* aColumn,
                                     smiHashKeyFunc * aHashKeyFunc );

typedef IDE_RC (*smiFindIsNullFunc)( const smiColumn*   aColumn,
                                     UInt               aFlag,
                                     smiIsNullFunc*     aIsNull );

typedef IDE_RC (*smiGetAlignValueFunc)( const smiColumn*   aColumn,
                                        UInt *             aAlignValue );

// PROJ-1705
typedef IDE_RC (*smiGetValueLengthFromFetchBuffer)( idvSQL          * aStatistics,
                                                    const smiColumn * aColumn,
                                                    const void      * aRow,
                                                    UInt            * aColumnSize,
                                                    idBool          * aIsNullValue );

typedef IDE_RC (*smiLockWaitFunc)(ULong aMicroSec, idBool *aWaited);
typedef IDE_RC (*smiLockWakeupFunc)();
typedef void   (*smiSetEmergencyFunc)(UInt);
typedef UInt   (*smiGetCurrTimeFunc)();
typedef void   (*smiDDLSwitchFunc)(SInt aFlag);

/*
    Disk Tablespace�� �����ϴ� �Լ� Ÿ��
 */
typedef IDE_RC (*smiCreateDiskTBSFunc)(idvSQL             *aStatistics,
                                       smiTableSpaceAttr*  aTableSpaceAttr,
                                       smiDataFileAttr**   aDataFileAttr,
                                       UInt                aDataFileAttrCount,
                                       void*               aTransForMtx);

typedef void (*smiTryUptTransViewSCNFunc)( smiStatement* aStmt );

/* PROJ-1594 Volatile TBS */
typedef IDE_RC (*smiMakeNullRowFunc)(idvSQL        *aStatistics,
                                     smiColumnList *aColumnList,
                                     smiValue      *aNullRow,
                                     SChar         *aValueBuffer);

// BUG-21895
typedef IDE_RC (*smiCheckNeedUndoRecord)(smiStatement * aSmiStmt,
                                         void         * aTableHandle,
                                         idBool       * aIsNeed);

/* BUG-19080: Old Version�� ���� �����̻� ����� �ش� Transaction��
 * Abort�ϴ� ����� �ʿ��մϴ�.*/
typedef ULong (*smiGetUpdateMaxLogSize)( idvSQL* aStatistics );

/* PROJ-2201
 * Session���κ��� SQL�� ������ ����� �ʿ��մϴ�. */
typedef IDE_RC (*smiGetSQLText)( idvSQL * aStatistics,
                                 UChar  * aStrBuffer,
                                 UInt     aStrBufferSize);


// TASK-3171
typedef IDE_RC (*smiGetNonStoringSizeFunc)( const smiColumn *aColumn,
                                            UInt * aOutSize );

// PROJ-2059 DB Upgrade ���
typedef void *(*smiGetColumnHeaderDescFunc)();
typedef void *(*smiGetTableHeaderDescFunc)();
typedef void *(*smiGetPartitionHeaderDescFunc)();
typedef UInt  (*smiGetColumnMapFromColumnHeader)( void * aColumnHeader,
                                                  UInt   aColumnIdx );

// BUG-37484
typedef idBool (*smiNeedMinMaxStatistics)( const smiColumn * aColumn );

typedef IDE_RC (*smiGetColumnStoreLen)( const smiColumn * aColumn,
                                        UInt            * aActualColen );

typedef idBool (*smiIsUsablePartialDirectKey)( void *aColumn );

typedef SInt (*smiGetDDLLockTimeout)(void * aMmSession);

typedef struct smiGlobalCallBackList
{
    smiFindCompareFunc                      findCompare;
    smiFindKey2StringFunc                   findKey2String;  // PROJ-1618
    smiFindNullFunc                         findNull;        // PROJ-1629
    smiFindCopyDiskColumnValueFunc          findCopyDiskColumnValue; // PROJ-1705
    smiFindCopyDiskColumnValue4DataTypeFunc findCopyDiskColumnValue4DataType; // PROJ-2429
    smiFindActualSizeFunc                   findActualSize;
    smiFindHashKeyFunc                      findHash;      // Hash Key ���� �Լ��� ã�� �Լ�
    smiFindIsNullFunc                       findIsNull;
    smiGetAlignValueFunc                    getAlignValue;
    smiGetValueLengthFromFetchBuffer        getValueLengthFromFetchBuffer; // PROJ-1705
    smiLockWaitFunc                         waitLockFunc;
    smiLockWakeupFunc                       wakeupLockFunc;
    smiSetEmergencyFunc                     setEmergencyFunc; // ���� ������ ���� �߻��� ȣ��.
    smiSetEmergencyFunc                     clrEmergencyFunc; // ���� ������ ���� �ذ�� ȣ��.
    smiGetCurrTimeFunc                      getCurrTimeFunc;
    smiDDLSwitchFunc                        switchDDLFunc;
    smiMakeNullRowFunc                      makeNullRow;
    smiCheckNeedUndoRecord                  checkNeedUndoRecord; // BUG-21895
    smiGetUpdateMaxLogSize                  getUpdateMaxLogSize;
    smiGetSQLText                           getSQLText;
    smiGetNonStoringSizeFunc                getNonStoringSize;
    smiGetColumnHeaderDescFunc              getColumnHeaderDescFunc;
    smiGetTableHeaderDescFunc               getTableHeaderDescFunc;
    smiGetPartitionHeaderDescFunc           getPartitionHeaderDescFunc;
    smiGetColumnMapFromColumnHeader         getColumnMapFromColumnHeaderFunc;
    smiNeedMinMaxStatistics                 needMinMaxStatistics; // BUG-37484
    smiGetColumnStoreLen                    getColumnStoreLen; // PROJ-2399
    smiIsUsablePartialDirectKey             isUsablePartialDirectKey; /* PROJ-2433 */
    smiGetDDLLockTimeout                    getDDLLockTimeout;
} smiGlobalCallBackList;


/* not used
// TASK-1421 SM UNIT enhancement , BUG-13124
typedef IDE_RC (*smiCallbackRunSQLFunc)(smiStatement * aSmiStmt,
                                        SChar        * aSqlStr,
                                        vSLong       * aRowCnt );

typedef IDE_RC (*smiCallbackGetTableHandleByNameFunc)(
                                        smiStatement * aSmiStmt,
                                        UInt           aUserID,
                                        UChar        * aTableName,
                                        SInt           aTableNameSize,
                                        void        ** aTableHandle,
                                        smSCN        * aSCN,
                                        idBool       * aIsSequence );

typedef struct smiUnitCallBackList
{
    smiCallbackRunSQLFunc mRunSQLFunc;
    smiCallbackGetTableHandleByNameFunc mGetTableHandleByNameFunc;
}smiUnitCallBackList;
*/

/* ------------------------------------------------
 *  smiInit()�� ����ϱ� ���� Macros
 *  SM�� ����ϴ� ��ƿ��Ƽ (altibase ����)����
 *  smiInit()�� �����ϴ� ����� �ٸ��� ������
 * ----------------------------------------------*/

/* not used
typedef enum
{
    SMI_INIT_ACTION_MAKE_DB_NAME     = 0x00000001, // db name ����
    SMI_INIT_ACTION_MANAGER_INIT     = 0x00000002, // ���� manager �ʱ�ȭ
    SMI_INIT_ACTION_RESTART_RECOVERY = 0x00000004, // restart recovery ����
    SMI_INIT_ACTION_REFINE_DB        = 0x00000008, // db refine ����
    SMI_INIT_ACTION_INDEX_REBUILDING = 0x00000010, // index �籸��
    SMI_INIT_ACTION_USE_SM_THREAD    = 0x00000020, // sm Thread startup
    SMI_INIT_ACTION_CREATE_LOG_FILE  = 0x00000040, // create log file
    SMI_INTI_ACTION_PRINT_INFO       = 0x00000080, // out manager info
    SMI_INIT_ACTION_SHMUTIL_INIT     = 0x00000100,
    SMI_INIT_ACTION_LOAD_ONLY_META   = 0x00000200, // Load Only Meta

    SMI_INIT_ACTION_MAX_END          = 0xFFFFFFFF
} smiInitAction;

// smiInit()�� ���ڷ� �Էµ�
#define SMI_INIT_CREATE_DB  (SMI_INIT_ACTION_MAKE_DB_NAME |\
                             SMI_INIT_ACTION_MANAGER_INIT |\
                             SMI_INIT_ACTION_CREATE_LOG_FILE)

#define SMI_INIT_META_DB    (SMI_INIT_ACTION_INDEX_REBUILDING|\
                             SMI_INIT_ACTION_USE_SM_THREAD)

#define SMI_INIT_RESTORE_DB (SMI_INIT_ACTION_MAKE_DB_NAME|\
                             SMI_INIT_ACTION_MANAGER_INIT|\
                             SMI_INIT_ACTION_RESTART_RECOVERY|\
                             SMI_INIT_ACTION_REFINE_DB|\
                             SMI_INIT_ACTION_INDEX_REBUILDING|\
                             SMI_INIT_ACTION_USE_SM_THREAD|\
                             SMI_INTI_ACTION_PRINT_INFO)

#define SMI_INIT_SHMUTIL_DB (SMI_INIT_ACTION_MAKE_DB_NAME |\
                             SMI_INIT_ACTION_SHMUTIL_INIT)
*/

/* ------------------------------------------------
 *  A4�� ���� Startup Flags
 * ----------------------------------------------*/
typedef enum
{
    SMI_STARTUP_INIT        = IDU_STARTUP_INIT,
    SMI_STARTUP_PRE_PROCESS = IDU_STARTUP_PRE_PROCESS,  /* for DB Creation    */
    SMI_STARTUP_PROCESS     = IDU_STARTUP_PROCESS,
    SMI_STARTUP_CONTROL     = IDU_STARTUP_CONTROL,
    SMI_STARTUP_META        = IDU_STARTUP_META,
    SMI_STARTUP_SERVICE     = IDU_STARTUP_SERVICE,
    SMI_STARTUP_SHUTDOWN    = IDU_STARTUP_SHUTDOWN,

    SMI_STARTUP_MAX         = IDU_STARTUP_MAX,    // 7
} smiStartupPhase;

//lock Mode�� ������ smlLockMode�� �����ϰ� �Ѵ�.
typedef enum
{
    SMI_TABLE_NLOCK       = 0x00000000,
    SMI_TABLE_LOCK_S      = 0x00000001,
    SMI_TABLE_LOCK_X      = 0x00000002,
    SMI_TABLE_LOCK_IS     = 0x00000003,
    SMI_TABLE_LOCK_IX     = 0x00000004,
    SMI_TABLE_LOCK_SIX    = 0x00000005
} smiTableLockMode;


typedef struct smiCursorPosInfo
{
    union
    {
        struct
        {
            scGRID     mLeafNode;
            smLSN      mLSN;
            scGRID     mRowGRID;
            ULong      mSmoNo;
            SInt       mSlotSeq;
            smiRange * mKeyRange; /* BUG-43913 */
        } mDRPos;
        struct
        {
            void     * mLeafNode;
            void     * mPrevNode;
            void     * mNextNode;
            IDU_LATCH  mVersion;
            void     * mRowPtr;
            smiRange * mKeyRange; /* BUG-43913 */
        } mMRPos;
        struct
        {
            SLong    mIndexPos;
            void   * mRowPtr;
        } mTRPos;  // Memory Temp Table�� ���� Cursor Information
        struct
        {
            void            * mPos;
        } mDTPos;  // DiskTempTable�� ���� CursorInformation
    } mCursor;
} smiCursorPosInfo;

//-----------------------------------------------
// INDEX TYPE�� ����
//    - �ִ� �ε��� ���� : 128 ��
// BUILT-IN INDEX�� ID
//    - ���� ȣȯ���� ���Ͽ� ���� �״�� �����Ѵ�.
//-----------------------------------------------

#define SMI_MAX_INDEXTYPE_ID                    (128)
#define SMI_BUILTIN_SEQUENTIAL_INDEXTYPE_ID       (0)
#define SMI_BUILTIN_B_TREE_INDEXTYPE_ID           (1)
#define SMI_BUILTIN_HV_B_TREE_INDEXTYPE_ID_LEGACY (2)    /* not used */
#define SMI_BUILTIN_B_TREE2_INDEXTYPE_ID          (3)    /* Deprecated */
#define SMI_BUILTIN_GRID_INDEXTYPE_ID             (4)
#define SMI_ADDITIONAL_RTREE_INDEXTYPE_ID         (6)    /* TASK-3171 */
#define SMI_INDEXTYPE_COUNT                       (7)

#define SMI_INDEXIBLE_TABLE_TYPES    (SMI_TABLE_TYPE_COUNT) // meta,temp,memory,disk,fixed,volatile,remote

/* ----------------------------------------------------------------------------
 *  db dir�� �ִ� ����
 * --------------------------------------------------------------------------*/
#define SM_DB_DIR_MAX_COUNT       (8)

/* ----------------------------------------------------------------------------
 * verify option
 * ����� TBS verify�� �����Ѵ�.
 * --------------------------------------------------------------------------*/
// tbs, seg, ext verify
#define SMI_VERIFY_TBS        (0x0001)
// page verify
#define SMI_VERIFY_PAGE       (0x0002)
// sm log�� write
#define SMI_VERIFY_WRITE_LOG  (0x0004)
// dbf verify
#define SMI_VERIFY_DBF        (0x0008)

#define SMI_IS_FIXED_COLUMN(aFlag) ( ((aFlag) & SMI_COLUMN_TYPE_MASK) \
                                     == SMI_COLUMN_TYPE_FIXED ? ID_TRUE : ID_FALSE )
#define SMI_IS_VARIABLE_COLUMN(aFlag) ( ((aFlag) & SMI_COLUMN_TYPE_MASK) \
                                        == SMI_COLUMN_TYPE_VARIABLE ? ID_TRUE : ID_FALSE )
#define SMI_IS_VARIABLE_LARGE_COLUMN(aFlag) ( ((aFlag) & SMI_COLUMN_TYPE_MASK) \
                                              == SMI_COLUMN_TYPE_VARIABLE_LARGE ? ID_TRUE : ID_FALSE )
#define SMI_IS_LOB_COLUMN(aFlag) ( ((aFlag) & SMI_COLUMN_TYPE_MASK)  \
                                   == SMI_COLUMN_TYPE_LOB ? ID_TRUE : ID_FALSE )
#define SMI_IS_OUT_MODE(aFlag) ( ((aFlag) & SMI_COLUMN_MODE_MASK)    \
                                 == SMI_COLUMN_MODE_OUT ? ID_TRUE : ID_FALSE )

//-----------------------------------------------
// PROJ-1877
// alter table modify column
//
// modify column ����߰��� memory table�� backup & restore��
// �ٸ� type���� ��ȯ�Ͽ� restore�ϴ� ����� �ʿ��ϰ� �Ǿ���.
//
// initilize, finalize�� record ������ ȣ��Ǵ� �Լ��̸�
// convertValue�� column ���� ���ϴ� type���� ��ȯ�ϴ� �Լ��̴�.
//-----------------------------------------------

typedef IDE_RC (*smiConvertInitializeForRestore)( void * aInfo );

typedef IDE_RC (*smiConvertFinalizeForRestore)( void * aInfo );

typedef IDE_RC (*smiConvertValueFuncForRestore)( idvSQL          * aStatistics,
                                                 const smiColumn * aSrcColumn,
                                                 const smiColumn * aDestColumn,
                                                 smiValue        * aValue,
                                                 void            * aInfo );

/* PROJ-1090 Function-based Index */
typedef IDE_RC (*smiCalculateValueFuncForRestore)( smiValue * aValueArr,
                                                   void     * aInfo );

// BUG-42920 DDL display data move progress
typedef IDE_RC (*smiPrintProgressLogFuncForRestore)( void  * aInfo,
                                                     idBool  aIsProgressComplete );

struct smiAlterTableCallBack {
    void                              * info;   // qdbCallBackInfo

    smiConvertInitializeForRestore      initializeConvert;
    smiConvertFinalizeForRestore        finalizeConvert;
    smiConvertValueFuncForRestore       convertValue;
    smiCalculateValueFuncForRestore     calculateValue;
    smiPrintProgressLogFuncForRestore   printProgressLog;
};




/* Proj-2059 DB Upgrade ���
 * Server �߽������� �����͸� �������� �ִ� ��� */

// Header Structure�� Endian������� �а� ���� �״�
typedef UInt   smiHeaderType;

#define SMI_DATAPORT_HEADER_OFFSETOF(s,m) (IDU_FT_OFFSETOF(s, m))
#define SMI_DATAPORT_HEADER_SIZEOF(s,m)   (IDU_FT_SIZEOF(s, m))

#define SMI_DATAPORT_HEADER_TYPE_CHAR       (0x0001)
#define SMI_DATAPORT_HEADER_TYPE_LONG       (0x0002)
#define SMI_DATAPORT_HEADER_TYPE_INTEGER    (0x0003)

#define SMI_DATAPORT_HEADER_FLAG_DESCNAME_MASK   (0x0001)
#define SMI_DATAPORT_HEADER_FLAG_DESCNAME_YES    (0x0001)
#define SMI_DATAPORT_HEADER_FLAG_DESCNAME_NO     (0x0000)

#define SMI_DATAPORT_HEADER_FLAG_COLNAME_MASK    (0x0002)
#define SMI_DATAPORT_HEADER_FLAG_COLNAME_YES     (0x0002)
#define SMI_DATAPORT_HEADER_FLAG_COLNAME_NO      (0x0000)

#define SMI_DATAPORT_HEADER_FLAG_TYPE_MASK       (0x0004)
#define SMI_DATAPORT_HEADER_FLAG_TYPE_YES        (0x0004)
#define SMI_DATAPORT_HEADER_FLAG_TYPE_NO         (0x0000)

#define SMI_DATAPORT_HEADER_FLAG_SIZE_MASK       (0x0008)
#define SMI_DATAPORT_HEADER_FLAG_SIZE_YES        (0x0008)
#define SMI_DATAPORT_HEADER_FLAG_SIZE_NO         (0x0000)

#define SMI_DATAPORT_HEADER_FLAG_FULL            \
        ( SMI_DATAPORT_HEADER_FLAG_DESCNAME_YES |\
          SMI_DATAPORT_HEADER_FLAG_COLNAME_YES  |\
          SMI_DATAPORT_HEADER_FLAG_TYPE_YES     |\
          SMI_DATAPORT_HEADER_FLAG_SIZE_YES )

//Validation�� �Լ�
typedef IDE_RC (*smiDataPortHeaderValidateFunc)( void * aDesc,
                                                  UInt   aVersion,
                                                  void * aHeader  );

typedef struct smiDataPortHeaderColDesc
{
    SChar         * mName;       // �̸�
    UInt            mOffset;     // Offset
    UInt            mSize;       // Size
    ULong           mDefaultNum; // ���������� ���, �� ���� ��� �����Ѵ�
    SChar         * mDefaultStr; // ���������� ���, �� ���� ��� �����Ѵ�
    smiHeaderType   mType;       // DataType
} smiDataPortHeaderColDesc;

typedef struct smiDataPortHeaderDesc
{
    SChar                          * mName;          // �̸�
    UInt                             mSize;          // ���� ����� ũ��
    smiDataPortHeaderColDesc       * mColumnDesc;    // column����
    smiDataPortHeaderValidateFunc    mValidateFunc;  // Validation�� �Լ�
} smiDataPortHeaderDesc;

/* ULong�� BigEndian���� Write�Ѵ�. */
#define SMI_WRITE_ULONG(src, dst){                      \
     ((UChar*)(dst))[0] = (*((ULong*)(src)))>>(56) & 255; \
     ((UChar*)(dst))[1] = (*((ULong*)(src)))>>(48) & 255; \
     ((UChar*)(dst))[2] = (*((ULong*)(src)))>>(40) & 255; \
     ((UChar*)(dst))[3] = (*((ULong*)(src)))>>(32) & 255; \
     ((UChar*)(dst))[4] = (*((ULong*)(src)))>>(24) & 255; \
     ((UChar*)(dst))[5] = (*((ULong*)(src)))>>(16) & 255; \
     ((UChar*)(dst))[6] = (*((ULong*)(src)))>>( 8) & 255; \
     ((UChar*)(dst))[7] = (*((ULong*)(src)))>>( 0) & 255; \
}

/* UInt�� BigEndian���� Write�Ѵ�. */
#define  SMI_WRITE_UINT(src, dst){                     \
     ((UChar*)(dst))[0] = (*(UInt*)(src))>>(24) & 255; \
     ((UChar*)(dst))[1] = (*(UInt*)(src))>>(16) & 255; \
     ((UChar*)(dst))[2] = (*(UInt*)(src))>>( 8) & 255; \
     ((UChar*)(dst))[3] = (*(UInt*)(src))>>( 0) & 255; \
}

/* UShort�� BigEndian���� Write�Ѵ�. */
#define  SMI_WRITE_USHORT(src, dst){                    \
    ((UChar*)(dst))[0] = (*(UShort*)(src))>>( 8) & 255; \
    ((UChar*)(dst))[1] = (*(UShort*)(src))>>( 0) & 255; \
}

/* BigEndian���� ��ϵ� ULong �� Read�Ѵ�. */
#define  SMI_READ_ULONG(src, dst){                         \
    *((ULong*)(dst)) = ((ULong)((UChar*)src)[0]<<56)       \
                   | ((ULong)((UChar*)src)[1]<<48)         \
                   | ((ULong)((UChar*)src)[2]<<40)         \
                   | ((ULong)((UChar*)src)[3]<<32)         \
                   | ((ULong)((UChar*)src)[4]<<24)         \
                   | ((ULong)((UChar*)src)[5]<<16)         \
                   | ((ULong)((UChar*)src)[6]<<8 )         \
                   | ((ULong)((UChar*)src)[7]);            \
}

#define  SMI_READ_UINT(src, dst){                     \
    *((UInt*)(dst)) = ((UInt)((UChar*)src)[0]<<24)    \
                   | ((UInt)((UChar*)src)[1]<<16)     \
                   | ((UInt)((UChar*)src)[2]<<8)      \
                   | ((UInt)((UChar*)src)[3]);        \
}


/* BigEndian���� ��ϵ� UShort�� Read�Ѵ�. */
#define SMI_READ_USHORT(src, dst){                 \
    *((UShort*)(dst)) = (((UChar*)src)[0]<<8)      \
    | (((UChar*)src)[1]);                          \
}

/* Proj-2059 DB Upgrade ���
 * Server �߽������� �����͸� �������� �ִ� ��� */

// DataPort����� �����ϴ� Handle

// ���� �ʱ�ȭ���� ���� RowSeq
#define SMI_DATAPORT_NULL_ROWSEQ      (ID_SLONG_MAX)

// DataPort Object�� ����
#define SMI_DATAPORT_TYPE_FILE        (0)
#define SMI_DATAPORT_TYPE_MAX         (1)

//DataPortHeader�� Version
#define SMI_DATAPORT_VERSION_1        (1)

#define SMI_DATAPORT_VERSION_BEGIN    (1)
#define SMI_DATAPORT_VERSION_LATEST   (SMI_DATAPORT_VERSION_1)
#define SMI_DATAPORT_VERSION_COUNT    (SMI_DATAPORT_VERSION_LATEST + 1)

/* BUG-30503  [PROJ-2059] SYS_DATA_PORTS_ ���̺� �÷� ���̿� qcmDataPortInfo
 * ����ü �� ���� �迭 ���� ����ġ
 * QC_MAX_NAME_LEN�� ��ġ�ؾ� ��. �ʹ� Ŭ �ʿ� ����. */
#define SMI_DATAPORT_JOBNAME_SIZE     (40)

// LobColumn ����
#define SMI_DATAPORT_LOB_COLUMN_TRUE  (true)
#define SMI_DATAPORT_LOB_COLUMN_FALSE (false)

// Object�� ����Ǵ� Header
typedef struct smiDataPortHeader
{
    UInt               mVersion; // Object�� Version.
                                 // Version���� ���� �ٸ� ���� �޸�
                                 // ���� 4Byte ����Ǿ� �д´�.
                                 // �ֳ��ϸ� mVersion�� ���� Header��
                                 // ����� �޶��� �� �ֱ� �����̴�.

    idBool             mIsBigEndian;                // BigEndian����
    UInt               mCompileBit;                 // 32Bit�ΰ� 64Bit�ΰ�
    SChar              mDBCharSet[ IDN_MAX_CHAR_SET_LEN ];
    SChar              mNationalCharSet[ IDN_MAX_CHAR_SET_LEN ];

    UInt               mPartitionCount;   // Partition�� ����
    UInt               mColumnCount;      // Column�� ����
    UInt               mBasicColumnCount; // �Ϲ�Column�� ����
    UInt               mLobColumnCount;   // LobColumn�� ����

    // ���� �������� ���� Header���, Encoder�� ������ �޾�
    // ��ϵȴ�.
    void             * mObjectHeader;     // scp?Header
    void             * mTableHeader;      // qsfTableInfo
    void             * mColumnHeader;     // qsfColumnInfo
    void             * mPartitionHeader;  // qsfPartitionInfo
} smiDataPortHeader;

extern smiDataPortHeaderDesc gSmiDataPortHeaderDesc[];

typedef struct smiRow4DP
{
    smiValue * mValueList;        // ������ Row���� Value��.
    idBool     mHasSupplementLob; // Lob�� ���� �߰������� �о�� �ϴ��� ����
} smiRow4DP;

/* TASK-4990 changing the method of collecting index statistics
 * ���� ������� ���� ��� */

/* 1Byte�� ��Bit�ΰ�? */
#define SMI_STAT_BYTE_BIT_COUNT  (8)

/* 1Byte�� ���ϱ� ���� BitMask��? */
#define SMI_STAT_BYTE_BIT_MASK   (SMI_STAT_BYTE_BIT_COUNT-1)

/* HashTable�� ����Ǵ� Bit������? */
#define SMI_STAT_HASH_TBL_BIT_COUNT  (24)

/* HashTable�� Byteũ���? */
#define SMI_STAT_HASH_TBL_SIZE   ( 1<<SMI_STAT_HASH_TBL_BIT_COUNT )

/* HashTable�� Byteũ���� mask��? */
#define SMI_STAT_HASH_TBL_MASK   ( SMI_STAT_HASH_TBL_SIZE - 1 )

/* LocalHash�� �ʱ�ȭ�Ǵ� ũ�� */
#define SMI_STAT_HASH_THRESHOLD  ( SMI_STAT_HASH_TBL_SIZE / 8 )

/* mtdHash�� ���� 4Byte(32bit)�� hash���� SMI_STAT_HASH_TBL_BIT_COUNT Size��
 * ����� */
#define SMI_STAT_HASH_COMPACT(i) ( ( ((UInt)i) & SMI_STAT_HASH_TBL_MASK ) ^ \
                                   ( ((UInt)i) >> SMI_STAT_HASH_TBL_BIT_COUNT ) )

/* HashTable���� ���° Byte���� ã�� */
#define SMI_STAT_GET_HASH_IDX(i)     ( SMI_STAT_HASH_COMPACT(i) /  \
                                       SMI_STAT_BYTE_BIT_COUNT )

/* HashTable�� �ش� Byte�� ������ Bit�� ���� */
#define SMI_STAT_GET_HASH_BIT(i)     ( 1 << ( SMI_STAT_HASH_COMPACT(i) \
                                              & SMI_STAT_BYTE_BIT_MASK ) )

/* HashTable�� �ش� ���� Bit�� ������ */
#define SMI_STAT_SET_HASH_VALUE(h,i) ( h[ SMI_STAT_GET_HASH_IDX(i) ] |= \
                                        SMI_STAT_GET_HASH_BIT(i) )

/* HashTable�� �ش� ���� Bit�� �������� */
#define SMI_STAT_GET_HASH_VALUE(h,i) ( h[ SMI_STAT_GET_HASH_IDX(i) ] & \
                                        SMI_STAT_GET_HASH_BIT(i) )

/* Column�� ���� �м��� �ڷᱸ�� */
typedef struct smiStatSystemArgument
{
    SLong  mHashTime;          /* ���� hash �ð� */
    SLong  mHashCnt;           /* ���� hash Ƚ�� */
    SLong  mCompareTime;       /* ���� compare �ð� */
    SLong  mCompareCnt;        /* ���� compare Ƚ�� */
} smiStatSystemArgument;

/* Column�� ���� �м��� �ڷᱸ�� */
typedef struct smiStatColumnArgument
{
    /* BUG-33548   [sm_transaction] The gather table statistics function
     * doesn't consider SigBus error */
    /* Min max */
    ULong  mMinValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Min�� */
    ULong  mMaxValue[SMI_MAX_MINMAX_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Max�� */
    UInt   mMinLength;
    UInt   mMaxLength;

    /* Column Info */
    smiColumn         *        mColumn;
    smiHashKeyFunc             mHashFunc;
    smiActualSizeFunc          mActualSizeFunc;
    smiCompareFunc             mCompare;
    smiCopyDiskColumnValueFunc mCopyDiskColumnFunc;
    smiIsNullFunc              mIsNull;
    smiKey2StringFunc          mKey2String;
    UInt                       mMtdHeaderLength;

    /* NumDist */
    UChar mLocalHashTable[ SMI_STAT_HASH_TBL_SIZE / SMI_STAT_BYTE_BIT_COUNT ];
    UChar mGlobalHashTable[ SMI_STAT_HASH_TBL_SIZE / SMI_STAT_BYTE_BIT_COUNT ];
    SLong mLocalNumDistPerGroup;
    SLong mLocalNumDist;
    SLong mGlobalNumDist;
    UInt  mLocalGroupCount;

    /* Average Column Length */
    SLong mAccumulatedSize;

    /* Result */
    SLong mNumDist;         /* NumberOfDinstinctValue(Cardinality) */
    SLong mNullCount;       /* NullValue Count   */
} smiStatColumnArgument;

/* Table�� ���� �м��� �ڷᱸ�� */
typedef struct smiStatTableArgument
{
    SFloat mSampleSize;        /* 1~100     */

    SLong  mAnalyzedRowCount;  /* �м��� �ο� ����    */
    SLong  mAccumulatedSize;   /* ���� �ο� ũ��    */

    SLong  mReadRowTime;       /* ���� one row read time */
    SLong  mReadRowCnt;        /* ���� one row read count */

    SLong  mAnalyzedPageCount;
    SLong  mMetaSpace;         /* PageHeader, ExtDir�� Meta ���� */
    SLong  mUsedSpace;         /* ���� ������� ���� */
    SLong  mAgableSpace;       /* ���߿� Aging������ ���� */
    SLong  mFreeSpace;         /* Data������ ������ �� ���� */

    smiStatColumnArgument * mColumnArgument;

    /* PROJ-2180 valueForModule
       SMI_OFFSET_USELESS �� �÷� */
    smiColumn             * mBlankColumn;
} smiStatTableArgument;

// BUG-40217
// float Ÿ���� ��� �ִ���̰� 47�̴�.
// �̸� ����Ͽ� 48���� �ø���.
#define SMI_DBMSSTAT_STRING_VALUE_SIZE (48)

/* X$DBMS_STAT �� �����ϱ� ���� */
typedef struct smiDBMSStat4Perf
{
    SChar   mCreateTime[SMI_DBMSSTAT_STRING_VALUE_SIZE];  /* TimeValue */
    SDouble mSampleSize;
    SLong   mNumRowChange;

    SChar   mType;            /* S(System), T(Table), I(Index), C(Column) */

    ULong   mTargetID;
    UInt    mColumnID;

    SDouble mSReadTime;
    SDouble mMReadTime;
    SLong   mDBFileMultiPageReadCount;

    SDouble mHashTime;
    SDouble mCompareTime;
    SDouble mStoreTime;

    SLong   mNumRow;
    SLong   mNumPage;
    SLong   mNumDist;
    SLong   mNumNull;
    SLong   mAvgLen;
    SDouble mOneRowReadTime;
    SLong   mAvgSlotCnt;
    SLong   mIndexHeight;
    SLong   mClusteringFactor;

    SLong   mMetaSpace;
    SLong   mUsedSpace;
    SLong   mAgableSpace;
    SLong   mFreeSpace;

    /* BUG-33548   [sm_transaction] The gather table statistics function
     * doesn't consider SigBus error */
    ULong   mMinValue[SMI_DBMSSTAT_STRING_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Min�� */
    ULong   mMaxValue[SMI_DBMSSTAT_STRING_VALUE_SIZE/ID_SIZEOF(ULong)]; /*Max�� */
} smiDBMSStat4Perf;

/*****************************************************************************
 * PROJ-2201 Innovation in sorting and hashing(temp)
 *****************************************************************************/

/************************** TT Flag (TempTable) *****************************/

#define SMI_TTFLAG_NONE                 (0x00000000)

/* Table�� ���� */
#define SMI_TTFLAG_TYPE_MASK            (0x00000003)
#define SMI_TTFLAG_TYPE_SORT            (0x00000001) /* SortTemp�� */
#define SMI_TTFLAG_TYPE_HASH            (0x00000002) /* �Ϲ� Hash�� */

/* ����ڰ� RangeScan�� ����� ���(ex:SortJoin)
 * ������ Ž�� �� MultipleIndex�� �������� */
#define SMI_TTFLAG_RANGESCAN            (0x00000010)
/* UniquenessViolation�� ����Ŵ */
#define SMI_TTFLAG_UNIQUE               (0x00000020)

/************************** TC Flag (TempCursor) ****************************/
/* Default�� */
#define SMI_TCFLAG_INIT           ( SMI_TCFLAG_FORWARD | \
                                    SMI_TCFLAG_ORDEREDSCAN )

#define SMI_TCFLAG_DIRECTION_MASK (0x00000003)/*Scan ����  */
#define SMI_TCFLAG_FORWARD        (0x00000001)/*������(LeftToRight) */
#define SMI_TCFLAG_BACKWARD       (0x00000002)/*������(RightToLeft) */

#define SMI_TCFLAG_HIT_MASK       (0x0000000C)/*HitFlagüũ ���� */
#define SMI_TCFLAG_IGNOREHIT      (0x00000000)/*������� Ž�� */
#define SMI_TCFLAG_HIT            (0x00000004)/*Hit�� �͸� Ž�� */
#define SMI_TCFLAG_NOHIT          (0x00000008)/*Hit�ȵ� ���� ���� */

#define SMI_TCFLAG_SCAN_MASK      (0x00000070)/*Scan�ϴ� ��� */
#define SMI_TCFLAG_FULLSCAN       (0x00000000)/*�Ϲ����� Scan */
#define SMI_TCFLAG_ORDEREDSCAN    (0x00000010)/*���ĵ� Scan */
#define SMI_TCFLAG_RANGESCAN      (0x00000020)/*Range(�Ǵ� HashValue)*/
#define SMI_TCFLAG_HASHSCAN       (0x00000040)/*Hash������ Ž��)*/

#define SMI_TCFLAG_FILTER_MASK    (0x00000700)/*Filter�� ��뿩��*/
#define SMI_TCFLAG_FILTER_RANGE   (0x00000100)/*RangeFilter�� �����*/
#define SMI_TCFLAG_FILTER_KEY     (0x00000200)/*KeyFilter�� �����*/
#define SMI_TCFLAG_FILTER_ROW     (0x00000400)/*RowFilter�� �����*/

#define SMI_HASH_CURSOR_NONE         (0x00000000) /* Hash�� cursor�� �� �� ���� �����̴� */
#define SMI_HASH_CURSOR_INIT         (0x00000001) /* Hash�� �ʱ�ȭ �Ǿ cursor�� �� �� �ִ� ���� */
#define SMI_HASH_CURSOR_FULL_SCAN    (0x00000002)
#define SMI_HASH_CURSOR_HASH_SCAN    (0x00000003)
#define SMI_HASH_CURSOR_HASH_UPDATE  (0x00000004)

typedef enum
{
    SMI_TTSTATE_INIT,
    SMI_TTSTATE_SORT_INSERTNSORT,
    SMI_TTSTATE_SORT_INSERTONLY,
    SMI_TTSTATE_SORT_EXTRACTNSORT,
    SMI_TTSTATE_SORT_MERGE,
    SMI_TTSTATE_SORT_MAKETREE,
    SMI_TTSTATE_SORT_INMEMORYSCAN,
    SMI_TTSTATE_SORT_MERGESCAN,
    SMI_TTSTATE_SORT_INDEXSCAN,
    SMI_TTSTATE_SORT_SCAN,
    SMI_TTSTATE_HASH_INSERT,
    SMI_TTSTATE_HASH_FETCH_FULLSCAN,
    SMI_TTSTATE_HASH_FETCH_HASHSCAN,
    SMI_TTSTATE_HASH_FETCH_UPDATE
} smiTempState;

struct smiSortTempCursor;
struct smiTempTableHeader;
struct smiTempPosition;

/* TempTable�� Module.
 * Fetch, Store/Restore Cursor���� Cursor�� Module�� ���� */
typedef IDE_RC (*smiTempOpenCursor)( smiTempTableHeader * aHeader,
                                     smiSortTempCursor  * aCursor );

typedef IDE_RC (*smiTempFetch)( smiSortTempCursor  * aTempCursor,
                                UChar  ** aRow,
                                scGRID  * aRowGRID );
typedef IDE_RC (*smiTempStoreCursor)( smiSortTempCursor * aCursor,
                                      smiTempPosition   * aPosition );
typedef IDE_RC (*smiTempRestoreCursor)( smiSortTempCursor * aCursor,
                                        smiTempPosition   * aPosition );

typedef struct smiTempColumn
{
    UInt                        mIdx;
    smiColumn                   mColumn;
    smiCopyDiskColumnValueFunc  mConvertToCalcForm;
    smiNullFunc                 mNull;
    smiIsNullFunc               mIsNull; /* PROJ-2435 order by nulls first/last */
    smiCompareFunc              mCompare;
    UInt                        mStoringSize;
    smiTempColumn             * mNextKeyColumn;
} smiTempColumn;

/* TempTable���� �����ϴ� �����
 * TTOPR(TempTableOperation) */
typedef enum
{
    SMI_TTOPR_NONE,
    SMI_TTOPR_CREATE,
    SMI_TTOPR_DROP,
    SMI_TTOPR_SORT, 
    SMI_TTOPR_OPENCURSOR,
    SMI_TTOPR_OPENCURSOR_HASH,
    SMI_TTOPR_OPENCURSOR_FULL,
    SMI_TTOPR_OPENCURSOR_UPDATE,
    SMI_TTOPR_RESTARTCURSOR,
    SMI_TTOPR_FETCH,
    SMI_TTOPR_FETCH_HASH,
    SMI_TTOPR_FETCH_FULL,
    SMI_TTOPR_FETCH_FROMGRID,
    SMI_TTOPR_STORECURSOR,
    SMI_TTOPR_RESTORECURSOR,
    SMI_TTOPR_CLEAR,
    SMI_TTOPR_CLEARHITFLAG,
    SMI_TTOPR_INSERT,
    SMI_TTOPR_UPDATE,
    SMI_TTOPR_SETHITFLAG,
    SMI_TTOPR_RESET_KEY_COLUMN,
    SMI_TTOPR_MAX
} smiTempTableOpr;

/* �ش� ������ SMI_TT_STATS_INTERVALȸ �����Ǹ�, ��������� ������ */
#define SMI_TT_STATS_INTERVAL (65536)
/* �� TempTable�� ������ SQL�� �����ص� ������ ũ�� */
#define SMI_TT_SQLSTRING_SIZE (16384)
/* ��Ÿ TempTable�� String�� ���� ũ�� */
#define SMI_TT_STR_SIZE          (36)

/* TempTable �ϳ��� ���� ��� ���� */
typedef struct smiTempTableStats
{

    ULong             mCount; /* mGlobalStat��. ������ ������� ���� */
    ULong             mTime;  /* mGlobalStat��. ���� �ð� */

    UInt              mCreateTV;
    UInt              mDropTV;
    UInt              mSpaceID;
    UInt              mTransID;

    smiTempTableOpr   mTTLastOpr;   /* ���������� ������ Operation */
    smiTempState      mTTState;

    ULong             mReadCount;       /* Read ȸ�� */
    ULong             mWriteCount;      /* Writeȸ�� */
    ULong             mWritePageCount;  /* Write�� Page����  */
    ULong             mOverAllocCount;  /* Extent�� �ʰ� �Ҵ��� ��   */
    ULong             mAllocWaitCount;  /* Extent �Ҵ� ����� ��   */

    SLong             mExtraStat1;     /* (Sort��, ������ ���� ��ȸ��),
                                        * (Hash�� HashBucket ����) */
    SLong             mExtraStat2;     /* (Sort��, ������ Run�� Slot����),
                                          (HashCollision �߻� ȸ�� ) */

    SLong             mMaxWorkAreaSize;/*Byte */
    SLong             mUsedWorkAreaSize;
    SLong             mNormalAreaSize; /*Byte*/
    SLong             mRuntimeMemSize; /*Byte*/
    SLong             mRecordCount;
    UInt              mRecordLength;

    UInt              mMergeRunCount;  /* SortTemp�� MergeRun�� ���� */
    UInt              mHeight;         /* SortTemp�� Index�� ���� */

    UInt              mIOPassNo;    /* 0:InMemory, 1:OnePass, 2:.. */
    ULong             mEstimatedOptimalSize; /* InMemorySort ũ�� ���� */
    ULong             mEstimatedSubOptimalSize; /* OnePassHash ũ�� ���� */

    UChar             mSQLText[ SMI_TT_SQLSTRING_SIZE ];

} smiTempTableStats;

typedef struct smiTempTableIOStats
{
    SLong     mMaxUsedWorkAreaSize;
    ULong     mReadCount;       /* Read ȸ�� */
    ULong     mWriteCount;      /* Writeȸ�� */
    ULong     mWritePageCount;  /* Write�� Page����  */

    ULong     mEstimatedOptimalSize; /* InMemoryHash ũ�� ���� */
    ULong     mEstimatedSubOptimalSize; /* OnePassHash ũ�� ���� */

    UInt      mIOPassNo;    /* 0:InMemory, 1:OnePass, 2:.. */

}smiTempTableIOStats;

/* X$TEMPTABLE_STATS */
/* PerformanceView�� ���� ���� ���� view�� ������ */
/* �������� ������� ��κ��� �������.
 * Į���� �ǹ̴� �� smiTempTableStats�� ����*/
typedef struct smiTempTableStats4Perf
{
    UInt    mSlotIdx;
    UChar   mSQLText[ SMI_TT_SQLSTRING_SIZE ];
    SChar   mCreateTime[SMI_TT_STR_SIZE];
    SChar   mDropTime[SMI_TT_STR_SIZE];
    UInt    mConsumeTime;

    UInt    mSpaceID;
    UInt    mTransID;
    SChar   mLastOpr[SMI_TT_STR_SIZE];

    SChar   mTTState[SMI_TT_STR_SIZE];
    UInt    mIOPassNo;
    ULong   mEstimatedOptimalSize;
    ULong   mEstimatedSubOptimalSize;
    ULong   mReadCount;
    ULong   mWriteCount;
    ULong   mWritePageCount;
    ULong   mOverAllocCount;
    ULong   mAllocWaitCount;

    SLong   mMaxWorkAreaSize;   /*Byte */
    SLong   mUsedWorkAreaSize;   /*Byte */
    SLong   mNormalAreaSize; /*Byte*/
    SLong   mRuntimeMemSize; /*Byte*/
    SLong   mRecordCount;
    UInt    mRecordLength;

    UInt    mMergeRunCount;
    UInt    mHeight;
    SLong   mExtraStat1;
    SLong   mExtraStat2;
} smiTempTableStats4Perf;

/* X$TEMPINFO */
/* Temp���� �������� ���� ��ο� ���� PerformanceView */
typedef struct smiTempInfo4Perf
{
    SChar   mName[SMI_TT_STR_SIZE];  /* ������ �̸� */
    SChar   mValue[SMI_TT_STR_SIZE]; /* ������ �� */
    SChar   mUnit[SMI_TT_STR_SIZE];  /* ������ ���� */
} smiTempInfo4Perf;

/* X$TEMPINFO�� Name, Value, Unit���� Record�� ����ϴ� Macro�� */
#define SMI_TT_SET_TEMPINFO_UINT( name, value, unit )                       \
 idlOS::snprintf( sInfo.mName,  SMI_TT_STR_SIZE, name  );                   \
 idlOS::snprintf( sInfo.mValue, SMI_TT_STR_SIZE, "%"ID_UINT32_FMT, value ); \
 idlOS::snprintf( sInfo.mUnit,  SMI_TT_STR_SIZE, unit  );                   \
 IDE_TEST(iduFixedTable::buildRecord( aHeader, aMemory, (void *)&sInfo )    \
                  != IDE_SUCCESS );

#define SMI_TT_SET_TEMPINFO_ULONG( name, value, unit )                      \
 idlOS::snprintf( sInfo.mName,  SMI_TT_STR_SIZE, name  );                   \
 idlOS::snprintf( sInfo.mValue, SMI_TT_STR_SIZE, "%"ID_UINT64_FMT, value ); \
 idlOS::snprintf( sInfo.mUnit,  SMI_TT_STR_SIZE, unit  );                   \
 IDE_TEST(iduFixedTable::buildRecord( aHeader, aMemory, (void *)&sInfo )    \
                  != IDE_SUCCESS );

#define SMI_TT_SET_TEMPINFO_STR( name, value, unit )                         \
 idlOS::snprintf( sInfo.mName,  SMI_TT_STR_SIZE, name  );                    \
 idlOS::snprintf( sInfo.mValue, SMI_TT_STR_SIZE, value );                    \
 idlOS::snprintf( sInfo.mUnit,  SMI_TT_STR_SIZE, unit  );                    \
 IDE_TEST(iduFixedTable::buildRecord( aHeader, aMemory, (void *)&sInfo )     \
                  != IDE_SUCCESS );

typedef struct smiTempTableHeader
{
    smiTempColumn   * mColumns;
    smiTempColumn   * mKeyColumnList;  /* mColumns�� ���������� KeyColumnList*/

    void            * mWASegment;
    UInt              mColumnCount;
    UInt              mKeyEndOffset;
    smiTempState      mTTState;        /* TempTable�� ���°� */
    UInt              mTTFlag;         /* �� TempTable�� �����ϴ� Flag */
    scSpaceID         mSpaceID;        /* NormalExtent�� ������ TablespaceID*/
    UInt              mHitSequence;    /* clearHitFlag�� ���� HitSequence��*/
    UInt              mWorkGroupRatio; /* ���� Group�� ũ�� */
    void            * mTempCursorList; /* TempCursor ��� */

    /**************************************************************
     * Row���� ����
     ***************************************************************/
    UInt           mRowSize;
    UInt           mMaxRowPageCount; /*Row�ϳ��� ����� �ִ� ������ �Ǽ� */
    UInt           mFetchGroupID;    /*FetchFromGRID���� ����� GroupID */
    SLong          mRowCount;

    /* RowPiece �ϳ��� RowInfo�� �����ö� Row�� �ɰ��� ��츦 Ŀ���ϱ� ����
     * �ִ� ����. �⺻������ RowSize��ŭ �Ҵ�� */
    UChar        * mNullRow;
    UChar        * mRowBuffer4Fetch;
    UChar        * mRowBuffer4Compare;
    UChar        * mRowBuffer4CompareSub;


    /**************************************************************
     * ���� ����� ���� ��ü���� ����ϴ� �ڷᱸ��
     ***************************************************************/
    /**************** insert(extract)NSort�� **************/
    /* extractNSort, insrtNSort�� ���Ǵ�, ���� ���ԵǴ� Sort����*/
    UChar         mSortGroupID;
    smiTempOpenCursor    mOpenCursor;   /* Sort������ ����, open cursor func */
    /**************** Merge�� **************/
    smuQueueMgr   mRunQueue;      /* Run���� FirstPageID�� ��Ƶδ� Queue*/
    UInt          mMergeRunSize;  /* Run�� ũ�� */
    UInt          mMergeRunCount; /* Run�� ���� */
    UInt          mLeftBottomPos; /* Heap�� ���� �Ʒ� ���� Slot�� ��ġ */
    iduStackMgr   mSortStack;     /* Sort�ϴµ� ���Ǵ� Stack */
    void        * mInitMergePosition;
    scGRID        mGRID;          /* ���������� ������ GRID�� �����ص� */

    /**************** Index�� **************/
    scPageID      mRootWPID;    /* Index�� RootNode, */
    UInt          mHeight;      /* Index�� ���� */
    scPageID      mRowHeadNPID; /* Resort�� �����, LeafNode�� HeadPage��
                                 * ��ġ�� �����ص� */

    /**************** Scan�� **************/
    scPageID    * mScanPosition; /* Run�� FirstPageID�� ��Ƶδ� �迭 */

    /**************** ���� **************/
    UInt                 mCheckCnt;    /* ������� ������ �������� �ʱ� ����
                                        * ������ */
    smiTempTableStats  * mStatsPtr;
    smiTempTableStats    mStatsBuffer; /* �ϴ� ���⿡ �����ϴٰ�,
                                        * TEMP_TABLE)WATCH_TIME�� �Ѿ��
                                        * Array�� ����Ѵ�. */

    idvSQL             * mStatistics;
} smiTempTableHeader;

// Temp Table Cursor, 64Byte�ȿ� �ʿ��� �������� ������.
typedef struct smiHashTempCursor
{
    UInt                 mTCFlag;
    UInt                 mHashValue;
    void               * mWASegment;
    UInt                 mHashSlotCount;
    UInt                 mSeq;
    /* ������ Fetch�� Row�� ChildGRID�� */
    UChar              * mChildRowPtr;
    scPageID             mChildPageID;
    scOffset             mChildOffset;

    /* Cache. ���������� ������ �������� ���� ����.
     * ���� �������� ���� Slot�� ������ ���, �̹� WPID, WAPagePtr,
     * SlotCount�� �˰� �ֱ� ������ ������ ���� ������. */
    UChar                mIsInMemory;
    UChar                mSubHashIdx;

    /* WAMap�� Sequence */
    void               * mSubHashPtr;
    void               * mSubHashWCBPtr;
    void               * mSubHashWCBPtr4Fetch;
    UChar              * mRowPtr;
    void               * mHashSlot;
    smiTempTableHeader * mTTHeader;
    UChar              * mWAPagePtr;

    void               * mEndWCBPtr;
    void               * mWCBPtr;

    /* Ž���� ��� ���� */
    const smiCallBack  * mRowFilter;
    const smiColumnList* mUpdateColumns;
    UInt                 mUpdateEndOffset;

    scPageID             mNExtLstPID;
    void               * mCurNExtentArr;

    smiHashTempCursor  * mNext;         /* CursorList */
} smiHashTempCursor;

typedef struct smiSortTempCursor
{
    void               * mWASegment;
    smiTempTableHeader * mTTHeader;

    /* Row�� ��ġ. Update/SetHitFlag�� �� GRID�� Row�� ��� ������*/
    scGRID               mGRID;

    UInt                 mTCFlag;
    UInt                 mWAGroupID;  /* Fetch�� ����ϴ� GroupID */
    /* Cache. ���������� ������ �������� ���� ����.
     * ���� �������� ���� Slot�� ������ ���, �̹� WPID, WAPagePtr,
     * SlotCount�� �˰� �ֱ� ������ ������ ���� ������. */
    UInt                 mSlotCount;
    scPageID             mWPID;
    UChar              * mRowPtr;
    UChar              * mWAPagePtr;
    /* WAMap�� Sequence */
    SInt                 mSeq;

    /* (Scan �Ҷ�) row�� ��ġ�� run�� �ε��� */
    UInt                 mPinIdx;
    SInt                 mLastSeq;

    /* Ž���� ��� ���� */
    UInt                 mUpdateEndOffset;
    const smiColumnList* mUpdateColumns;
    const smiRange     * mRange;
    const smiRange     * mKeyFilter;
    const smiCallBack  * mRowFilter;

    smiTempPosition    * mPositionList; /* �� Cursor�� PositionList */
    /* run ������ ���� */
    void               * mMergePosition;


    /* Module */
    smiTempFetch         mFetch;
    smiTempStoreCursor   mStoreCursor;
    smiTempRestoreCursor mRestoreCursor;
    smiSortTempCursor  * mNext;         /* CursorList */

} smiSortTempCursor;

typedef struct smiTempPosition
{
    smiSortTempCursor  * mOwner;
    smiTempPosition    * mNext;

    /* MergeScan�� Position�� �����ϱ� ���ؼ���, �� MergeRun�� ������
     * ��� �����ؾ� ��. ������ MergeRun�� ����� �𸣱� ������
     * �ش� �޸𸮸� malloc�Ͽ� ���⿡ �޾Ƶ� */
    void               * mExtraInfo;

        /* Row�� ��ġ. Update/SetHitFlag�� �� GRID�� Row�� ��� ������*/
    scGRID               mGRID;
    /* Cache. ���������� ������ �������� ���� ����.
     * ���� �������� ���� Slot�� ������ ���, �̹� WPID, WAPagePtr,
     * SlotCount�� �˰� �ֱ� ������ ������ ���� ������. */
    UInt                 mSlotCount;
    scPageID             mWPID;
    /* WAMap�� Sequence */
    UInt                 mSeq;
    smiTempState         mTTState;

    UChar              * mRowPtr;
    UChar              * mWAPagePtr;

    /* (Scan �Ҷ�) row�� ��ġ�� run�� �ε��� */
    UInt                 mPinIdx;

} smiTempPosition;

typedef enum
{
    SMI_BEFORE_LOCK_RELEASE,
    SMI_AFTER_LOCK_RELEASE
} smiCallOrderInCommitFunc;

/* PROJ-2365 sequence table
 * replication�� �����ϵ��� sequence�� table�� ���� ��´�.
 * sequence.nextval�� ���� callback function
 */
typedef IDE_RC (*smiSelectCurrValFunc)( SLong * aCurrVal,
                                        void  * aInfo );
typedef IDE_RC (*smiUpdateLastValFunc)( SLong   aLastVal,
                                        void  * aInfo );

struct smiSeqTableCallBack {
    void                 * info;           /* qdsCallBackInfo */
    smiSelectCurrValFunc   selectCurrVal;  /* open cursor, select row */
    smiUpdateLastValFunc   updateLastVal;  /* update row, close cursor */
    UInt                   scale;          /* for Sharded sequence */
};

typedef enum
{
    SMI_DTX_NONE = 0,
    SMI_DTX_PREPARE,
    SMI_DTX_COMMIT,
    SMI_DTX_ROLLBACK,
    SMI_DTX_END
} smiDtxLogType;

/* dk ���� MinLSN �� ���ϱ� ���� ����Ѵ� */
#define SMI_LSN_MAX(aLSN)                           \
{                                                   \
    (aLSN).mFileNo = (aLSN).mOffset = ID_UINT_MAX;  \
}
#define SMI_LSN_INIT(aLSN)                          \
{                                                   \
   (aLSN).mFileNo = (aLSN).mOffset = 0;             \
}

#define SMI_IS_LSN_INIT(aLSN)                       \
    (((aLSN).mFileNo == 0) && ((aLSN).mOffset == 0))

/* BUG-37778 qp ���� disk hash temp table ����� �����ϱ� ���ؼ� ����Ѵ� */
/* XXX PROJ-2647 �� plan�� ����Ǿ �ӽ÷� �������� �־���.
 * ���Ŀ� PLAN�� �����ϸ鼭 ���� ���� �Ͽ��� �Ѵ�. */
#define SMI_TR_HEADER_SIZE_FULL     (32)  //  SDT_TR_HEADER_SIZE_FULL
#define SMI_WAEXTENT_PAGECOUNT      SDT_WAEXTENT_PAGECOUNT
#define SMI_WAMAP_SLOT_MAX_SIZE     SDT_WAMAP_SLOT_MAX_SIZE

/* BUG-41765 Remove warining in smiLob.cpp */
#define SMI_LOB_LOCATOR_CLIENT_MASK        (0x00000004)
#define SMI_LOB_LOCATOR_CLIENT_TRUE        (0x00000004)
#define SMI_LOB_LOCATOR_CLIENT_FALSE       (0x00000000)

/* BUG-43408, BUG-45368 */
#define SMI_ITERATOR_SIZE                  (50 * 1024)


/* session������ ��� ���� mmcSession�� callback �Լ����� ����.
 * BUG-47655 session�� transaction �Ҵ� ��õ� Ƚ���� �����ϱ� ���� Callback �Լ� */
typedef struct smiSessionCallback
{
    void (*mSetAllocTransRetryCount)( void   * aSession,
                                      ULong    aRetryCount );
  
    UInt (*mGetIndoubtFetchTimeout)( void * aMmSession ); 
    UInt (*mGetIndoubtFetchMethod)( void * aMmSession );

} smiSessionCallback;

/* PROJ-2735 DDL Transaction */
typedef struct smiDDLTargetTableInfo
{
    UInt           mTableID;
    void         * mOldTableInfo;
    void         * mNewTableInfo;
    idBool         mIsReCreated;
    iduList        mPartInfoList;
    iduListNode    mNode;
} smiDDLTargetTableInfo;

typedef struct  smiTransactionalDDLCallback
{
    IDE_RC (*backupDDLTargetOldTableInfo)( smiTrans               * aTrans, 
                                           smOID                    aTableOID,
                                           UInt                     aPartOIDCount,
                                           smOID                  * aPartOIDArray,
                                           smiDDLTargetTableInfo ** aDDLTargetTableInfo );

    IDE_RC (*backupDDLTargetNewTableInfo)( smiTrans               * aTrans, 
                                           smOID                    aTableOID,
                                           UInt                     aPartOIDCount,
                                           smOID                  * aPartOIDArray,
                                           smiDDLTargetTableInfo ** aDDLTargetTableInfo );

    void   (*removeDDLTargetTableInfo)( smiTrans * aTrans, smiDDLTargetTableInfo * aDDLTargetTableInfo );

    void   (*restoreDDLTargetOldTableInfo)( smiDDLTargetTableInfo * aDDLTargetTableInfo );

    void   (*destroyDDLTargetNewTableInfo)( smiDDLTargetTableInfo * aDDLTargetTableInfo );
} smiTrasactionalDDLCallback;

/*
   PROJ-2734
   ������ ���� ����

   ����!!!
   �Ʒ� ������ ���� ���Ǵ�sdi.h ������ sdiShardPin ���� ���ǿ� ��ġ�Ͽ��� �Ѵ�.
 */
typedef ULong  smiShardPin;

#define SMI_SHARD_PIN_INVALID (0)

typedef enum
{
    SMI_OFFSET_VERSION      = 56,   /* << 7 byte, sMmSharMPinInfo.mVersion                  */
    SMI_OFFSET_RESERVED     = 48,   /* << 6 byte, sMmSharMPinInfo.mReserveM                 */
    SMI_OFFSET_META_NODE_ID = 32,   /* << 4 byte, sMmSharMPinInfo.mMetaNoMeInfo.mMetaNoMeIM */
    SMI_OFFSET_SEQEUNCE     = 0,    /* << 0 byte, sMmSharMPinInfo.mSeq                      */
} smihardPinFactorOffset;

#define SMI_MAX_SHARD_PIN_STR_LEN              (20) /* Without Null terminated */
#define SMI_SHARD_PIN_FORMAT_STR   "%"ID_UINT32_FMT"-%"ID_UINT32_FMT"-%"ID_UINT32_FMT
#define SMI_SHARD_PIN_FORMAT_ARG( __SHARD_PIN ) \
    ( __SHARD_PIN & ( (smiShardPin)0xff       << SMI_OFFSET_VERSION      ) ) >> SMI_OFFSET_VERSION, \
    ( __SHARD_PIN & ( (smiShardPin)0xffff     << SMI_OFFSET_META_NODE_ID ) ) >> SMI_OFFSET_META_NODE_ID, \
    ( __SHARD_PIN & ( (smiShardPin)0xffffffff << SMI_OFFSET_SEQEUNCE     ) ) >> SMI_OFFSET_SEQEUNCE

inline void SMI_DIVIDE_SHARD_PIN( smiShardPin aShardPIN,
                                  UChar *aVersion,
                                  UShort *aNodeId,
                                  UInt *aSequence )
{
    if ( aVersion != NULL )
    {
        *aVersion  = ( aShardPIN & ( (smiShardPin)0xff       << SMI_OFFSET_VERSION      ) ) >> SMI_OFFSET_VERSION;
    }

    if ( aNodeId != NULL )
    {
        *aNodeId   = ( aShardPIN & ( (smiShardPin)0xffff     << SMI_OFFSET_META_NODE_ID ) ) >> SMI_OFFSET_META_NODE_ID;
    }

    if ( aSequence != NULL )
    {
        *aSequence = ( aShardPIN & ( (smiShardPin)0xffffffff << SMI_OFFSET_SEQEUNCE     ) ) >> SMI_OFFSET_SEQEUNCE;
    }
}

/* PROJ-2734 
   �л극��
   ���� SINGLE   : �ϳ��� ��忡���� Statement�� ����Ǿ���
   ���� MULTI    : ���� ��忡�� Statement�� ����Ǿ�����, ���ð��� �ϳ��� ��忡���� ����.
   ���� PARALLEL : ���ÿ� ���� ��忡�� Statement�� ����Ǿ��� */
typedef enum
{
    SMI_DIST_LEVEL_INIT = 0,
    SMI_DIST_LEVEL_SINGLE,
    SMI_DIST_LEVEL_MULTI,
    SMI_DIST_LEVEL_PARALLEL,
    SMI_DIST_LEVEL_MAX
} smiDistLevel;

/* ��ȿ�� �л극������ Ȯ���Ѵ�.
   (�л극���� ��ȿ�� ���, �л������� ������ ��Ÿ����.) */
#define SMI_DIST_LEVEL_IS_VALID( aLevel )     \
    ( ( aLevel == SMI_DIST_LEVEL_SINGLE ) ||  \
      ( aLevel == SMI_DIST_LEVEL_MULTI )  ||  \
      ( aLevel == SMI_DIST_LEVEL_PARALLEL ) )
#define SMI_DIST_LEVEL_IS_NOT_VALID( aLevel ) \
    ( ( aLevel != SMI_DIST_LEVEL_SINGLE ) &&  \
      ( aLevel != SMI_DIST_LEVEL_MULTI )  &&  \
      ( aLevel != SMI_DIST_LEVEL_PARALLEL ) )

typedef struct smiDistTxInfo
{
    smSCN              mFirstStmtViewSCN;
    PDL_Time_Value     mFirstStmtTime;
    smiShardPin        mShardPin;
    smiDistLevel       mDistLevel;
} smiDistTxInfo;

#define SMI_SET_SMI_DIST_TX_INFO( aTargetDistTxInfo, aFirstStmtViewSCN, aFirstStmtTime, aShardPin, aDistLevel ) \
{ \
    (aTargetDistTxInfo)->mFirstStmtViewSCN = aFirstStmtViewSCN; \
    (aTargetDistTxInfo)->mFirstStmtTime    = aFirstStmtTime; \
    (aTargetDistTxInfo)->mShardPin         = aShardPin; \
    (aTargetDistTxInfo)->mDistLevel        = aDistLevel; \
}

#define SMI_MAX_ERR_MSG_LEN  (256)
#define SMI_XID_STRING_LEN   (256)  /* XID_DATA_MAX_LEN ���� */

#endif /* _O_SMI_DEF_H_ */
