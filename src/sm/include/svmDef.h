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
 
#ifndef _O_SVM_DEF_H_
#define _O_SVM_DEF_H_ 1

#include <idu.h>
#include <iduMutex.h>
#include <smDef.h>
#include <smuList.h>
#include <sctDef.h>


// Tablespace���� ù ������ ������ ���̵�
// 0�� ������ ���̵�� SM_NULL_PID�̹Ƿ� ����� ���Ѵ�.
// PCHArray[0]�� m_page�� NULL�̴�.
// �� ������ meta page(membase)�� ��������.
#define SVM_TBS_FIRST_PAGE_ID      (1)

/* PROJ-1490 DB Free Page List
 */
typedef struct svmDBFreePageList
{
    // Free Page List ID
    // svmMemBase.mFreePageLists�������� index
    scPageID   mFirstFreePageID ; // ù��° Free Page �� ID
    vULong     mFreePageCount ;   // Free Page ��
} svmDBFreePageList ;

// �ִ� Free Page List�� �� 
#define SVM_MAX_FPL_COUNT (SM_MAX_PAGELIST_COUNT)

typedef struct svmMemBase
{
    SChar              mDBname[SM_MAX_DB_NAME]; // DB Name
    vULong             mAllocPersPageCount;
    vULong             mExpandChunkPageCnt;    // ExpandChunk �� Page��
    vULong             mCurrentExpandChunkCnt; // ���� �Ҵ��� ExpandChunk�� ��
    UInt               mFreePageListCount;
    svmDBFreePageList  mFreePageLists[SVM_MAX_FPL_COUNT];
} svmMemBase;

/* ----------------------------------------------------------------------------
 *    TempPage Def
 * --------------------------------------------------------------------------*/
#if defined(WRS_VXWORKS)
#undef m_next
#endif
struct svmTempPage;
typedef struct svmTempPageHeader
{
    svmTempPage * m_self;
    svmTempPage * m_prev;
    svmTempPage * m_next;
} svmTempPageHeader;

struct __svmTempPage__
{
    svmTempPageHeader m_header;
    vULong            m_body[1]; // for align calculation 
};

#define SVM_TEMP_PAGE_BODY_SIZE (SM_PAGE_SIZE - offsetof(__svmTempPage__, m_body))

struct svmTempPage
{
    svmTempPageHeader m_header;
#if !defined(COMPILE_64BIT)
    UInt              m_align;
#endif
    UChar             m_body[SVM_TEMP_PAGE_BODY_SIZE];
};
/* ========================================================================*/

typedef  smPCHBase svmPCH;  // BUG-48513

// BUG-43463 ���� ��� scanlist link/unlink �ÿ�
// page�� List���� ���ŵǰų� ���� �� �� �����Ѵ�.
// Ȧ���̸� ������, ¦���̸� ���� �Ϸ�
#define SVM_PCH_SET_MODIFYING( aPCH ) \
    SMM_PCH_SET_MODIFYING( aPCH );

#define SVM_PCH_SET_MODIFIED( aPCH ) \
    SMM_PCH_SET_MODIFIED( aPCH );              \

// BUG-43463 smnnSeq::moveNext/Prev���� �Լ����� ���
// ���� page�� link/unlink�۾� ������ Ȯ���Ѵ�.
#define SVM_PAGE_IS_MODIFYING( aScanModifySeq ) \
    SMM_PAGE_IS_MODIFYING( aScanModifySeq )


/* ------------------------------------------------
 *  CPU CACHE Set : BUGBUG : ID Layer�� �ű�� ����!
 * ----------------------------------------------*/
#if defined(SPARC_SOLARIS)
#define SVM_CPU_CACHE_LINE      (64)
#else
#define SVM_CPU_CACHE_LINE      (32)
#endif
#define SVM_CACHE_ALIGN(data)   ( ((data) + (SVM_CPU_CACHE_LINE - 1)) & ~(SVM_CPU_CACHE_LINE - 1))

/* ------------------------------------------------
 *  OFFSET OF MEMBASE PAGE
 * ----------------------------------------------*/
// BUGBUG!!! : SVM Cannot Refer to SMC. Original Definition is like below.
//
// #define SVM_MEMBASE_OFFSET      SVM_CACHE_ALIGN(offsetof(smcPersPage, m_body))
//
// Currently, we Fix 'offsetof(smcPersPage, m_body)' to 32.
// If smcPersPage is modified, this definition should be checked also.
// Ask to xcom73.
#define SVM_MEMBASE_OFFSET       SVM_CACHE_ALIGN(32)
#define SVM_CAT_TABLE_OFFSET     SVM_CACHE_ALIGN(SVM_MEMBASE_OFFSET + ID_SIZEOF(struct svmMemBase))

/* ------------------------------------------------
   PROJ-1490 ����������Ʈ ����ȭ�� �޸� �ݳ� ����
 * ----------------------------------------------*/

// Free Page List �� ��
#define SVM_FREE_PAGE_LIST_COUNT (smuProperty::getPageListGroupCount())

// �ּ� ��� Page�� ���� Free Page List�� ���� �� ���ΰ�?
#define SVM_FREE_PAGE_SPLIT_THRESHOLD                       \
            ( smuProperty::getMinPagesDBTableFreeList() )


// Expand Chunk�Ҵ�� �� Free Page�� ��� Page���� ������ ������ ����.
#define SVM_PER_LIST_DIST_PAGE_COUNT                       \
            ( smuProperty::getPerListDistPageCount() )

// �������� ������ �� �ѹ��� ��� Expand Chunk�� �Ҵ���� ���ΰ�?
#define SVM_EXPAND_CHUNK_COUNT (1)

// Volatile TBS�� ��Ÿ ������ ������ �����Ѵ�.
#define SVM_TBS_META_PAGE_CNT ((vULong)1)

// PROJ-1490 ����������Ʈ ����ȭ�� �޸𸮹ݳ�
// Page�� ���̺�� �Ҵ�� �� �ش� Page�� Free List Info Page�� ������ ��
// ���� �⵿�� Free Page�� Allocated Page�� �����ϱ� ���� �뵵�� ���ȴ�.
#define SVM_FLI_ALLOCATED_PID ( SM_SPECIAL_PID )

/* BUG-31881  smmDef.h�� smmPageReservation�� ���� ����.*/

#define SVM_PAGE_RESERVATION_MAX  (64)
#define SVM_PAGE_RESERVATION_NULL ID_UINT_MAX

typedef struct svmPageReservation
{
    void     * mTrans     [ SVM_PAGE_RESERVATION_MAX ];
    SInt       mPageCount [ SVM_PAGE_RESERVATION_MAX ];
    UInt       mSize;
} svmPageReservation;


// Memory Tablespace Node
// �ϳ��� Memory Tablespace�� ���� ��� Runtime������ ���Ѵ�.
typedef struct svmTBSNode
{
    /******** FROM svmTableSpace ********************/
    // Memory / Disk TBS common informations
    sctTableSpaceNode mHeader;

    // Tablespace Attrubute
    smiTableSpaceAttr mTBSAttr;

    /******** FROM svmManager ********************/
    ULong             mDBMaxPageCount;
    
    // BUG-47487: FLI ������ memPool ( Volatile )
    iduMemPool        mFLIMemPagePool;
    
    iduMemPool        mMemPagePool;
    iduMemPool        mPCHMemPool;

    // BUG-17216
    // membase�� 0�� �������� �ƴ� TBSNode �ȿ� �ε��� �����Ѵ�.
    svmMemBase        mMemBase;
    
    /******** FROM svmFPLManager ********************/

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
    svmPageReservation * mArrPageReservation;

    /******** FROM svmExpandChunk ********************/

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
    
} svmTBSNode;

/* BUG-32461 [sm-mem-resource] add getPageState functions to smmExpandChunk
 * module */
typedef enum svmPageState
{
    SVM_PAGE_STATE_NONE = 0,
    SVM_PAGE_STATE_META,
    SVM_PAGE_STATE_FLI,
    SVM_PAGE_STATE_ALLOC, // isDataPage = True
    SVM_PAGE_STATE_FREE   // isDataPage = True
} svmPageState;

#endif // _O_SVM_DEF_H_
