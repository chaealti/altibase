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
 * $Id: smmDef.h 90083 2021-02-26 00:58:48Z et16 $
 **********************************************************************/

#ifndef _O_SMM_DEF_H_
#define _O_SMM_DEF_H_ 1

#include <idu.h>
#include <idnCharSet.h>
#include <iduMutex.h>
#include <smDef.h>
#include <smuList.h>
#include <sctDef.h>
#include <smriDef.h>


#define SMM_PINGPONG_COUNT   (2)  // ping & pong

#define SMM_MAX_AIO_COUNT_PER_FILE  (8192)  // �� ȭ�Ͽ� ���� AIO �ִ� ����
#define SMM_MIN_AIO_FILE_SIZE       (1 * 1024 * 1024) // �ּ� AIO ���� ȭ�� ũ�� 

// BUG-29607 Checkpoint Image File Name�� ����
// TBSName "-" PingpongNumber "-"
// TBSName "-" PingpongNumber "-" FileNumber
// DirPath "//" TBSName "-" PingpongNumber "-" FileNumber
#define SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG          \
                           "%s-%"ID_UINT32_FMT"-"
#define SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG_FILENUM  \
                           SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG"%"ID_UINT32_FMT
#define SMM_CHKPT_IMAGE_NAME_WITH_PATH             \
                           "%s"IDL_FILE_SEPARATORS""SMM_CHKPT_IMAGE_NAME_TBS_PINGPONG_FILENUM
#define SMM_CHKPT_IMAGE_NAMES  "%s-.."

/* PROJ-1490 DB Free Page List
 */
typedef struct smmDBFreePageList
{
    // Free Page List ID
    // smmMemBase.mFreePageLists�������� index
    scPageID   mFirstFreePageID ; // ù��° Free Page �� ID
    vULong     mFreePageCount ;   // Free Page ��
} smmDBFreePageList ;

// �ִ� Free Page List�� �� 
#define SMM_MAX_FPL_COUNT (SM_MAX_PAGELIST_COUNT)


typedef struct smmMemBase
{
    // fix when createdb
    SChar           mDBname[SM_MAX_DB_NAME]; // DB Name
    SChar           mProductSignature[IDU_SYSTEM_INFO_LENGTH];
    SChar           mDBFileSignature[IDU_SYSTEM_INFO_LENGTH];
    UInt            mVersionID;              // Altibase Version
    UInt            mCompileBit;             // Support Mode "32" or '"64"
    idBool          mBigEndian;              // big = ID_TRUE 
    vULong          mLogSize;                // Logfile Size
    vULong          mDBFilePageCount;        // one dbFile Page Count
    UInt            mTxTBLSize;    // Transaction Table Size;
    UInt            mDBFileCount[ SMM_PINGPONG_COUNT ];

    // PROJ-1579 NCHAR
    SChar           mDBCharSet[IDN_MAX_CHAR_SET_LEN];
    SChar           mNationalCharSet[IDN_MAX_CHAR_SET_LEN];

    // change in run-time
    struct timeval  mTimestamp;
    vULong          mAllocPersPageCount;

    // PROJ 2281
    // Storing SystemStat persistantly 
    smiSystemStat   mSystemStat;

    // PROJ-1490 ������ ����Ʈ ����ȭ�� �޸� ���� ���� �����
    // Expand Chunk�� �����ͺ��̽��� Ȯ���ϴ� �ּ� �����̴�.
    // Exapnd Chunk�� ũ��� �����ͺ��̽� �����ÿ� �����Ǹ�, ������ �Ұ����Ѵ�.
    vULong          mExpandChunkPageCnt;    // ExpandChunk �� Page��
    vULong          mCurrentExpandChunkCnt; // ���� �Ҵ��� ExpandChunk�� ��
    
    // �������� ����ȭ�� DB Free Page List�� �� ����
    UInt                mFreePageListCount;
    smmDBFreePageList   mFreePageLists[ SMM_MAX_FPL_COUNT ];
    // system SCN
    smSCN           mSystemSCN;
} smmMemBase;

/* ----------------------------------------------------------------------------
 *    TempPage Def
 * --------------------------------------------------------------------------*/
#if defined(WRS_VXWORKS)
#undef m_next
#endif 
struct smmTempPage;
typedef struct smmTempPageHeader
{
    smmTempPage * m_self;
    smmTempPage * m_prev;
    smmTempPage * m_next;
} smmTempPageHeader;

struct __smmTempPage__
{
    smmTempPageHeader m_header;
    vULong            m_body[1]; // for align calculation 
};

#define SMM_TEMP_PAGE_BODY_SIZE (SM_PAGE_SIZE - offsetof(__smmTempPage__, m_body))

struct smmTempPage
{
    smmTempPageHeader m_header;
#if !defined(COMPILE_64BIT)
    UInt              m_align;
#endif
    UChar             m_body[SMM_TEMP_PAGE_BODY_SIZE];
};
/* ========================================================================*/

typedef struct smmPCH : public smPCHBase // Page Control Header : PCH
{
    void               *mPageMemHandle;

    // BUG-17770: DML Transactino�� Page�� ���� ���� �Ǵ� Read����
    //            Page�� ���ؼ� ���� X, S Latch�� ��´�. �̶� ����ϴ�
    //            Mutex�� mPageMemMutex�̴�. ������ Checkpoint�ÿ��� ������
    //            Mutex�� ����Ͽ��� �Ѵ�.

    // m_page�� Page Memory�� Free�Ϸ��� Thread�� m_page�� Disk�� ��������
    // Checkpoint Thread���� ���ü� ��� ���� Mutex.
    // mPageMemMutex�� ����ϰ� �Ǹ�, �Ϲ� Ʈ����ǵ��
    // Checkpoint Thread�� ���ʿ��� Contension�� �ɸ��� �ȴ�.
    iduMutex            mMutex;

    // DML(insert, update, delete)������ �����ϴ� Transaction��
    // ��� Mutex

    idBool              m_dirty;
    UInt                m_dirtyStat; // CASE-3768 

    //added by newdaily because dirty page list
    smmPCH             *m_pnxtDirtyPCH; 
    smmPCH             *m_pprvDirtyPCH; // CASE-3768

    // for smrDirtyPageMgr
    scSpaceID           mSpaceID;
} smmPCH;


// BUG-43463 ���� ��� scanlist link/unlink �ÿ�
// page�� List���� ���ŵǰų� ���� �� �� �����Ѵ�.
// Ȧ���̸� ������, ¦���̸� ���� �Ϸ�
#define SMM_PCH_SET_MODIFYING( aPCH ) \
IDE_ASSERT( aPCH->mModifySeqForScan % 2 == 0); \
idCore::acpAtomicInc64( &(aPCH->mModifySeqForScan) );

#define SMM_PCH_SET_MODIFIED( aPCH ) \
IDE_ASSERT( aPCH->mModifySeqForScan % 2 != 0); \
idCore::acpAtomicInc64( &(aPCH->mModifySeqForScan) );

// BUG-43463 smnnSeq::moveNext/Prev���� �Լ����� ���
// ���� page�� link/unlink�۾� ������ Ȯ���Ѵ�.
#define SMM_PAGE_IS_MODIFYING( aScanModifySeq ) \
( (( aScanModifySeq % 2 ) == 1) ? ID_TRUE : ID_FALSE )

typedef struct smmTempMemBase
{
    // for Temp Page
    iduMutex            m_mutex;
    smmTempPage        *m_first_free_temp_page;
    vULong              m_alloc_temp_page_count;
    vULong              m_free_temp_page_count;
} smmTempMemBase;

typedef struct smmDirtyPage
{
    smmDirtyPage *m_next;
    smPCSlot     *m_pch;  // BUG-48513
} smmDirtyPage;

    
struct smmShmHeader;

typedef struct smmSCH  // Shared memory Control Header
{
    smmSCH       *m_next;
    SInt          m_shm_id;
    smmShmHeader *m_header;
} smmSCH;


/* ------------------------------------------------
 *  CPU CACHE Set : BUGBUG : ID Layer�� �ű�� ����!
 * ----------------------------------------------*/
#if defined(SPARC_SOLARIS)
#define SMM_CPU_CACHE_LINE      (64)
#else
#define SMM_CPU_CACHE_LINE      (32)
#endif
#define SMM_CACHE_ALIGN(data)   ( ((data) + (SMM_CPU_CACHE_LINE - 1)) & ~(SMM_CPU_CACHE_LINE - 1))

/* ------------------------------------------------
 *  OFFSET OF MEMBASE PAGE
 * ----------------------------------------------*/
// BUGBUG!!! : SMM Cannot Refer to SMC. Original Definition is like below.
//
// #define SMM_MEMBASE_OFFSET      SMM_CACHE_ALIGN(offsetof(smcPersPage, m_body))
//
// Currently, we Fix 'offsetof(smcPersPage, m_body)' to 32.
// If smcPersPage is modified, this definition should be checked also.
// Ask to xcom73.
#define SMM_MEMBASE_PAGEID       ((scPageID)0)
#define SMM_MEMBASE_PAGE_SIZE    (SM_PAGE_SIZE)
#define SMM_MEMBASE_OFFSET       SMM_CACHE_ALIGN(32)
#define SMM_CAT_TABLE_OFFSET     SMM_CACHE_ALIGN(SMM_MEMBASE_OFFSET + ID_SIZEOF(struct smmMemBase))


/* ------------------------------------------------
 *  Shared Memory DB Control
 * ----------------------------------------------*/

/*
    Unaligned version of Shared Memory Header
 */
typedef struct smmShmHeader
{
    UInt          m_versionID;
    idBool        m_valid_state;   // operation �߿� ������ ID_FALSE
    scPageID      m_page_count;    // �Ҵ�� page ����
    // PROJ-1548 Memory Table Space
    // ����ڰ� SHM_DB_KEY_TBS_INTERVAL�� �����Ͽ� 
    // �����޸� Chunk�� �ٸ� ���̺� �����̽���
    // attach�� �Ǵ� ��츦 üũ�ϱ� ����
    scSpaceID     mTBSID;          // �����޸� Chunk�� ���� TBS�� ID
    key_t         m_next_key;      // == 0 �̸�  next key�� ����
} smmShmHeader;

/*
    Aligned Version of Shared memory header

   �����޸� Header�� �����޸� �����ȿ���
   Cache Lineũ��� Align�� ũ�⸸ŭ�� �����Ѵ�.
   �̸� ���� Header�ٷ� ������ ���� �����޸� Page���� �����ּҰ�
   Cache Line�� Align�ǵ��� �Ѵ�
 */

// �����޸� Header�� ũ�⸦ Cache Align�Ͽ�
// �����޸� Page���� �����ּҰ� Cache Align�ǵ��� �Ѵ�
#define SMM_CACHE_ALIGNED_SHM_HEADER_SIZE \
            ( SMM_CACHE_ALIGN( ID_SIZEOF(smmShmHeader) ) )
#define SMM_SHM_DB_SIZE(a_use_page)       \
            ( SMM_CACHE_ALIGNED_SHM_HEADER_SIZE + SM_PAGE_SIZE * a_use_page)
/* ------------------------------------------------
 *  [] Fixed Memory�� ��ũ�� ��ġ�� ���
 *     �����͸� �Ʒ��� ��(0xFF..)���� ����.
 *     attach()�� ������ �� �� ���� �̿���.
 * ----------------------------------------------*/
#ifdef COMPILE_64BIT
#define SMM_SHM_LOCATION_FREE     (ID_ULONG(0xFFFFFFFFFFFFFFFF))
#else
#define SMM_SHM_LOCATION_FREE     (0xFFFFFFFF)
#endif
/* ------------------------------------------------
 *  SLOT List
 * ----------------------------------------------*/
/* aMaximum VON smmSlotList::initialize         */
# define SMM_SLOT_LIST_MAXIMUM_DEFAULT       (100)

/* IN smmSlotList::initialize                   */
# define SMM_SLOT_LIST_MAXIMUM_MINIMUM       ( 10)

/* aCache VON smmSlotList::initialize           */
# define SMM_SLOT_LIST_CACHE_DEFAULT         ( 10)

/* IN smmSlotList::initialize                   */
# define SMM_SLOT_LIST_CACHE_MINIMUM         (  5)

/* aFlag    VOM smmSlotList::allocateSlots      *
 * aFlag    VOM smmSlotList::releaseSlots       */
# define SMM_SLOT_LIST_MUTEX_MASK     (0x00000001)
# define SMM_SLOT_LIST_MUTEX_NEEDLESS (0x00000000)
# define SMM_SLOT_LIST_MUTEX_ACQUIRE  (0x00000001)

/* CASE-3768, DOC-28381
 * 1. INIT->FLUSH->REMOVE->INIT
 * 2. INIT->FLUSH->REMOVE->[FLUSHDUP->REMOVE]*->INIT
 */

# define SMM_PCH_DIRTY_STAT_MASK      (0x00000003)
# define SMM_PCH_DIRTY_STAT_INIT      (0x00000000) 
# define SMM_PCH_DIRTY_STAT_FLUSH     (0x00000001) 
# define SMM_PCH_DIRTY_STAT_FLUSHDUP  (0x00000002) 
# define SMM_PCH_DIRTY_STAT_REMOVE    (0x00000003) 

typedef struct smmSlot
{
    smmSlot* prev;
    smmSlot* next;
}
smmSlot;

/* ------------------------------------------------
   db dir�� ������ ������ ù��° db dir�� memory db��
   create�Ѵ�. 
 * ----------------------------------------------*/
#define SMM_CREATE_DB_DIR_INDEX (0)



/* ------------------------------------------------
   PROJ-1490 ����������Ʈ ����ȭ�� �޸� �ݳ� ����
 * ----------------------------------------------*/

// Free Page List �� ��
#define SMM_FREE_PAGE_LIST_COUNT (smuProperty::getPageListGroupCount())

// �ּ� ��� Page�� ���� Free Page List�� ���� �� ���ΰ�?
#define SMM_FREE_PAGE_SPLIT_THRESHOLD                       \
            ( smuProperty::getMinPagesDBTableFreeList() )


// Expand Chunk�Ҵ�� �� Free Page�� ��� Page���� ������ ������ ����.
#define SMM_PER_LIST_DIST_PAGE_COUNT                       \
            ( smuProperty::getPerListDistPageCount() )

//// �������� ������ �� �ѹ��� ��� Expand Chunk�� �Ҵ���� ���ΰ�?
//#define SMM_EXPAND_CHUNK_COUNT (1)

// �����ͺ��̽� ��Ÿ �������� ��
// ��Ÿ �������� �����ͺ��̽��� �� ó���� ��ġ�ϸ�
// Membase�� īŻ�α� ���̺� ������ ���Ѵ�.
#define SMM_DATABASE_META_PAGE_CNT ((vULong)1)

// PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�
// Page�� ���̺�� �Ҵ�� �� �ش� Page�� Free List Info Page�� ������ ��
// ���� �⵿�� Free Page�� Allocated Page�� �����ϱ� ���� �뵵�� ���ȴ�.
#define SMM_FLI_ALLOCATED_PID ( SM_SPECIAL_PID )


// prepareDB ���� �Լ��鿡 ���޵Ǵ� �ɼ�
typedef enum smmPrepareOption
{
    SMM_PREPARE_OP_NONE = 0,                    // �ɼǾ��� 
    SMM_PREPARE_OP_DBIMAGE_NEED_RECOVERY = 1,   // DB IMAGE�� Recovery�� ����
    SMM_PREPARE_OP_DONT_CHECK_RESTORE_TYPE = 2,  // RestoreType üũ ����
    SMM_PREPARE_OP_DBIMAGE_NEED_MEDIA_RECOVERY = 3,  // DB IMAGE�� Media Recovery�� ����
    SMM_PREPARE_OP_DONT_CHECK_DB_SIGNATURE_4SHMUTIL = 4  // shmutil�� ��� 
} smmPrepareOption ;

// restoreDB ���� �Լ��鿡 ���޵Ǵ� �ɼ�
typedef enum smmRestoreOption
{
    SMM_RESTORE_OP_NONE = 0,                        // �ɼǾ���
    // Restart Recovery�� �ʿ����� ����
    // DB IMAGE�� Recovery�� ����
    SMM_RESTORE_OP_DBIMAGE_NEED_RECOVERY = 1, 
    // DB IMAGE�� Media Recovery�� ����
    SMM_RESTORE_OP_DBIMAGE_NEED_MEDIA_RECOVERY = 2  
} smmRestoreOption ;

// PRJ-1548 User Memory Tablespace
// �����ͺ��̽��� ����
typedef enum
{
    // �Ϲݸ޸�
    SMM_DB_RESTORE_TYPE_DYNAMIC    = 0, 
    // DISK�� CREATE�� �����ͺ��̽��� �����޸𸮷� RESTORE
    SMM_DB_RESTORE_TYPE_SHM_CREATE = 1,
    // �����޸𸮻� �����ϴ� �����ͺ��̽��� ATTACH
    SMM_DB_RESTORE_TYPE_SHM_ATTACH = 2,
    // ���� PREPARE/RESTORE�� ���� �ʾ���
    SMM_DB_RESTORE_TYPE_NOT_RESTORED_YET = 3,
    // TYPE���� ( �� ���� ������ ��찡 �־�� �ȵ� )
    SMM_DB_RESTORE_TYPE_NONE       = 4
} smmDBRestoreType;


// ���� �����ͺ��̽� ���°� ���� ������ �ƴ����� ���ϴ� FLAG
typedef enum
{
    SMM_STARTUP_PHASE_NON_SERVICE = 0, // �������� �ƴ�
    SMM_STARTUP_PHASE_SERVICE     = 1  // ��������.
} smmStartupPhase;

/* 
   �޸� ����Ÿ���Ͽ� ���� �������� �� �α׾�Ŀ���� Offset ��.. 
   Runtime ������ �����ϴ� ����ü
*/
typedef struct smmCrtDBFileInfo
{
    // Stable/Unstable�� ���� �������θ� �����Ѵ�.
    idBool    mCreateDBFileOnDisk[ SMM_PINGPONG_COUNT ];

    // Loganchor�� ����� Chkpt Image Attribute�� AnchorOffset�� �����Ѵ�. 
    UInt      mAnchorOffset;

} smmCrtDBFileInfo;

class smmDirtyPageMgr;


/* BUG-31881 [sm-mem-resource] When executing alter table in MRDB and 
 * using space by other transaction,
 * The server can not do restart recovery. 
 * AlterTable �� RestoreTable �ϱ� ���� Ȯ���ϴ� Page��.
 * ���ÿ� SMM_PAGE_RESERVATION_MAX��ŭ�� Tx�� �����ص� �� �ִ�.*/

#define SMM_PAGE_RESERVATION_MAX  (64)
#define SMM_PAGE_RESERVATION_NULL ID_UINT_MAX

typedef struct smmPageReservation
{
    void     * mTrans     [ SMM_PAGE_RESERVATION_MAX ];
    SInt       mPageCount [ SMM_PAGE_RESERVATION_MAX ];
    UInt       mSize;
} smmPageReservation;

// Memory Tablespace Node
// �ϳ��� Memory Tablespace�� ���� ��� Runtime������ ���Ѵ�.
typedef struct smmTBSNode
{
    /******** FROM smmTableSpace ********************/
    // Memory / Disk TBS common informations
    sctTableSpaceNode mHeader;

    // Tablespace Attrubute
    smiTableSpaceAttr mTBSAttr;
    
    // Base node of Checkpoint Path attribute list
    smuList           mChkptPathBase;

    smmDirtyPageMgr * mDirtyPageMgr;
    
    /******** FROM smmManager ********************/
    smmSCH            mBaseSCH;
    smmSCH *          mTailSCH;
    smmDBRestoreType  mRestoreType;

    // Loading ���� �ִ� PID : �޸� ��������� Ʋ����
    // PID�� �Ʒ��� ������ ���� ��� mDataArea�� ���� �����Ǿ�� �ϰ�,
    // Ŭ ��� TempMemPool�� ���� ���� �Ǿ�� ��.
    scPageID          mStartupPID;

    ULong             mDBMaxPageCount;
    ULong             mHighLimitFile;
    UInt              mDBPageCountInDisk;
    UInt              mLstCreatedDBFile;
    void **           mDBFile[ SM_DB_DIR_MAX_COUNT ];

    // BUG-17343 loganchor�� Stable/Unstable Chkpt Image�� ���� ���� 
    //           ������ ���� 
    // �޸� ����Ÿ������ ������ ���� Runtime ���� 
    smmCrtDBFileInfo* mCrtDBFileInfo; 

    // BUG-47487: FLI������ memPool 
    iduMemPool2       mFLIMemPagePool;
    
    iduMemPool2       mDynamicMemPagePool;
    iduMemPool        mPCHMemPool;
    smmTempMemBase    mTempMemBase;    // �����޸� ������ ����Ʈ

    smmMemBase *      mMemBase;
    
    /******** FROM smmFPLManager ********************/

    // �� Free Page List�� Mutex �迭
    // Mutex�� Durable�� ������ �ƴϱ� ������,
    // membase�� Free Page List�� Mutex�� �Բ� ���� �ʴ´�.
    iduMutex   * mArrFPLMutex;

    // �ϳ��� Ʈ������� Expand Chunk�� �Ҵ��ϰ� ���� ��,
    // �ٸ� Ʈ������� Page������ Expand Chunk�� �� �Ҵ����� �ʰ�,
    // Expand Chunk�Ҵ� �۾��� ����Ǳ⸦ ��ٸ����� �ϱ� ���� Mutex.
    iduMutex     mAllocChunkMutex;
    
    /* BUG-31881 [sm-mem-resource] When executing alter table in MRDB and 
     * using space by other transaction,
     * The server can not do restart recovery. 
     * FreePageListCount��ŭ ����ȭ�Ǿ� mArrFPLMutex�� ���� ����ȴ�. */
    smmPageReservation * mArrPageReservation;

    /******** FROM smmExpandChunk ********************/

    // Free List Info Page���� Slot����� ������ ��ġ 
    UInt              mFLISlotBase;
    // Free List Info Page�� ���̴�.
    UInt              mChunkFLIPageCnt;
    // �ϳ��� Expand Chunk�� ���ϴ� Page�� ��
    // Free List Info Page�� �� ���� ������ ��ü Page �� �̴�.
    vULong            mChunkPageCnt;
    
    // PRJ-1548 User Memory Tablespace
    // Loganchor �޸𸮹��ۻ��� TBS Attribute ���������
    UInt              mAnchorOffset;
    
} smmTBSNode;

// PRJ-1548 User Memory Tablespace
// �޸����̺����̽��� CHECKPOINT PATH ������ �����ϴ� ���
// ���������ÿ� Loganchor�κ��� �ʱ�ȭ�ǰų�  DDL�� ���ؼ� 
// �����ȴ�.
//
// �� ����ü�� ����� �߰��Ǹ�
// smmTBSChkptPath::initializeChkptPathNode �� �Բ� ����Ǿ�� �Ѵ�.
typedef struct smmChkptPathNode
{
    smiChkptPathAttr       mChkptPathAttr; // Chkpt Path �Ӽ�
    UInt                   mAnchorOffset;  // Loganchor���� ����� ������
} smmChkptPathNode;

// �޸� üũ����Ʈ�̹���(����Ÿ����)�� ��Ÿ���
typedef struct smmChkptImageHdr
{
    // To Fix BUG-17143 TBS CREATE/DROP/CREATE�� DBF NOT FOUND����
    scSpaceID mSpaceID;  // Tablespace�� ID

    UInt    mSmVersion;  // ���̳ʸ� ���� ���� 

    // �̵����� ���� RedoLSN
    smLSN   mMemRedoLSN;

    // �̵����� ���� CreateLSN 
    smLSN   mMemCreateLSN;

    // PROJ-2133 incremental backup
    smiDataFileDescSlotID  mDataFileDescSlotID;
    
    // PROJ-2133 incremental backup
    // incremental backup�� ���Ͽ��� �����ϴ� ����
    smriBISlot  mBackupInfo;
} smmChkptImageHdr;

/* 
   PRJ-1548 User Memory TableSpace 

   �޸� ����Ÿ������ ��Ÿ����� LogAnchor�� �����ϱ� ���� ����ü
   smmDatabaseFile ��ü�� ��������� ���ǵǾ� �ִ�. 
*/
typedef struct smmChkptImageAttr
{
    smiNodeAttrType  mAttrType;         // PRJ-1548 �ݵ�� ó���� ����
    scSpaceID        mSpaceID;          // �޸� ���̺����̽� ID
    UInt             mFileNum;          // ����Ÿ������ No. 
    // ����Ÿ������ CreateLSN
    smLSN            mMemCreateLSN;
    // Stable/Unstable ���� ���� ���� 
    idBool           mCreateDBFileOnDisk[ SMM_PINGPONG_COUNT ];
    //PROJ-2133 incremental backup
    smiDataFileDescSlotID mDataFileDescSlotID;

} smmChkptImageAttr;

// PRJ-1548 User Memory Tablespace ���� ���� 
// dirty page flush�� �� taget ����Ÿ���̽��� pingpong ��ȣ�� ��ȯ 
// �̵������꿡���� ���� stable�� ����Ÿ���̽��� flush�ؾ� �ϰ�,
// ��� üũ����Ʈ�ÿ��� ���� stable�� ����Ÿ���̽��� flush�ؾ� �Ѵ�. 
typedef SInt (*smmGetFlushTargetDBNoFunc) ( smmTBSNode * aTBSNode );

/* BUG-32461 [sm-mem-resource] add getPageState functions to smmExpandChunk
 * module */
typedef enum smmPageState
{
    SMM_PAGE_STATE_NONE = 0,
    SMM_PAGE_STATE_META,
    SMM_PAGE_STATE_FLI,
    SMM_PAGE_STATE_ALLOC, // isDataPage = True
    SMM_PAGE_STATE_FREE   // isDataPage = True
} smmPageState;


typedef IDE_RC (*smmGetPersPagePtrFunc)( scSpaceID     aSpaceID,
                                         scPageID      aPageID,
                                         void       ** aPersPagePtr );

#endif // _O_SMM_DEF_H_
