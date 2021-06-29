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
 * $Id: smpDef.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description:
 *
 * TABLE Layer�� PageList������ ������ ����.
 *
 * TableHeader
 *     �� AllocPageList[] (For Fixed)
 *     �� PageListEntry (For Fixed)
 *                     :
 *             �� FreePagePool
 *             �� FreePageList[] - SizeClassList[]
 *                                        :
 *                     :
 *     �� AllocPageList[] (For Var)
 *     �� PageListEntry[] (For Var)
 *             :
 * ���̺������ Fixed�� Var���� PageListEntry�� ���� �����ϸ�
 * ������ PageListEntry���� Page�� Alloc������ ���� AllocPageList��
 * Page�� Free������ ���� FreePageList�� �����Ѵ�.
 * ������ AllocPageList�� FreePageList�� ������Ƽ�� PageListGroupCount������
 * ����ȭ�Ǿ� Ʈ������� ���� ���ٰ����ϰ� �Ͽ�����(PROJ-1490),
 * FreePageList���� Slot�� ��뷮�� ���� SizeClass���� �׷����Ͽ���.
 * ���� FreePageList���� �뷱���� ���� FreePagePool�� �����Ͽ���.
 *
 * Transaction
 *     �� PrivatePageListCachePtr
 *     �� PrivatePageListHashTable
 *
 * Ʈ������� DB���� �Ҵ��� Page���� �ٸ� Ʈ������� ������� �ʵ���
 * PrivatePageList���� �����ϵ��� �Ͽ�����(PROJ-1464),
 * �ֱٿ� �����ߴ� PrivatePageList�� ���� Cache�����͸� �ξ�
 * ������ ���� �� �ֵ��� �Ͽ���.
 *
 **********************************************************************/

#ifndef _O_SMP_DEF_H_
#define _O_SMP_DEF_H_ 1

#include <smDef.h>
#include <idu.h>

/* PROJ-2162 RestartRiskReduction */
// PageType�� ������
typedef UInt smpPageType;

#define  SMP_PAGETYPE_MASK                  (0x00000003)
#define  SMP_PAGETYPE_NONE                  (0x00000000)
#define  SMP_PAGETYPE_FIX                   (0x00000001)
#define  SMP_PAGETYPE_VAR                   (0x00000002)

#define  SMP_PAGEINCONSISTENCY_MASK         (0x80000000)
#define  SMP_PAGEINCONSISTENCY_FALSE        (0x00000000)
#define  SMP_PAGEINCONSISTENCY_TRUE         (0x80000000)

// alloc/free slot�ϴ� ��� ���̺� Ÿ��
typedef enum
{
    SMP_TABLE_NORMAL = 0,  // �Ϲ� ���̺�
    SMP_TABLE_TEMP         // TEMP ���̺�
} smpTableType;

/* ----------------------------------------------------------------------------
 *    PersPage Def
 * --------------------------------------------------------------------------*/
// BUGBUG : If smpPersPageHeader is changed,
//          SMM_MEMBASE_OFFSET should be check also
// Critical : smpPersPageHeader�� mPrev/mNext�� PageList�� ���� ���̴�.
//            �̰��� DB�� TABLE�� �ְ� ������ �ӽ÷� ����Ҷ��� ������ ����Ʈ������,
//            ���̺� ��ϵ� ���� AllocPageList�� ����ȭ �Ǿ� �����Ƿ�
//            mPrev/mNext�� �ٷ� �����ϴ� �� ���� smpManager::getPrevAllocPageID()
//            getNextAllocPageID()�� ����ؾ� ���̺� ����ȭ�� PageList�� �ùٷ�
//            ������ �� �ְ� �ȴ�.
typedef struct smpPersPageHeader
{
    scPageID           mSelfPageID;   // �ش� Page�� PID
    scPageID           mPrevPageID;   // ���� Page�� PID
    scPageID           mNextPageID;   // ���� Page�� PID
    smpPageType        mType;         // �ش� Page�� Page Type
    smOID              mTableOID;
    UInt               mAllocListID;  // �ش� Page�� AllocPageList�� ListID
#if defined(COMPILE_64BIT)
    SChar              mDummy[4];
#endif
} smpPersPageHeader;


typedef struct smpVarPageHeader
{
    vULong               mIdx;    // BUGBUG: which var page list.
#if !defined(COMPILE_64BIT)
    SChar                mDummy[4];
#endif
} smpVarPageHeader;

struct __smpPersPage__
{
    smpPersPageHeader mHeader;
    SDouble           mBody[1]; // for align calculation
};

#define SMP_PERS_PAGE_BODY_OFFSET (offsetof(__smpPersPage__, mBody))
#define SMP_PERS_PAGE_BODY_SIZE        (SM_PAGE_SIZE - offsetof(__smpPersPage__, mBody))

#define SMP_MAX_FIXED_ROW_SIZE ( SMP_PERS_PAGE_BODY_SIZE - ID_SIZEOF(smpSlotHeader) )

struct smpPersPage
{
    smpPersPageHeader mHeader;
    UChar             mBody[SMP_PERS_PAGE_BODY_SIZE];
};

#define SMP_GET_VC_PIECE_COUNT(length) ((length + SMP_VC_PIECE_MAX_SIZE - 1) / SMP_VC_PIECE_MAX_SIZE)
#define SMP_GET_PERS_PAGE_ID(page) (((smpPersPage*)(page))->mHeader.mSelfPageID)
#define SMP_SET_PERS_PAGE_ID(page, pid) (((smpPersPage*)(page))->mHeader.mSelfPageID = (pid))
#define SMP_SET_PREV_PERS_PAGE_ID(page, pid) (((smpPersPage*)(page))->mHeader.mPrevPageID = (pid))
#define SMP_SET_NEXT_PERS_PAGE_ID(page, pid) (((smpPersPage*)(page))->mHeader.mNextPageID = (pid))

#define SMP_GET_NEXT_PERS_PAGE_ID(page) (((smpPersPage*)(page))->mHeader.mNextPageID)
#define SMP_GET_PREV_PERS_PAGE_ID(page) (((smpPersPage*)(page))->mHeader.mPrevPageID)
#define SMP_SET_PERS_PAGE_TYPE(page, aPageType) (((smpPersPage*)(page))->mHeader.mType = (aPageType))
#define SMP_GET_PERS_PAGE_TYPE( page ) (((smpPersPage*)(page))->mHeader.mType & SMP_PAGETYPE_MASK)
#define SMP_GET_PERS_PAGE_INCONSISTENCY( page ) (((smpPersPage*)(page))->mHeader.mType & SMP_PAGEINCONSISTENCY_MASK)


/* ----------------------------------------------------------------------------
 *    page item(slot) size
 * --------------------------------------------------------------------------*/
/* Max Variable Column Piece Size */
#define SMP_VC_PIECE_MAX_SIZE ( SMP_PERS_PAGE_BODY_SIZE - \
                           ID_SIZEOF(smpVarPageHeader) - ID_SIZEOF(smVCPieceHeader) )

/* BUGBUG: By Newdaily QP���� 32KȮ�� �۾��� ������ 32K�� �ٲپ���Ѵ�.  */
//#define SMP_MAX_VAR_COLUMN_SIZE   SMP_VC_PIECE_MAX_SIZE
// PROJ-1583 large geometry
#define SMP_MAX_VAR_COLUMN_SIZE  ID_UINT_MAX // 4G

/* Min Variable Column Piece Size */
#define SMP_VC_PIECE_MIN_SIZE (SM_DVAR(1) << SM_ITEM_MIN_BIT_SIZE)

/* ----------------------------------------------------------------------------
 *    FreePageHeader Def
 * --------------------------------------------------------------------------*/

typedef struct smpFreePageHeader
{
    UInt               mFreeListID;     // �ش� FreePage�� FreePageList ID
    UInt               mSizeClassID;    // �ش� FreePage�� SizeClass ID
    SChar*             mFreeSlotHead;   // �ش� FreePage������ FreeSlot Head
    SChar*             mFreeSlotTail;   // �ش� FreePage������ FreeSlot Tail
    UInt               mFreeSlotCount;  // �ش� FreePage������ FreeSlot ����
    smpFreePageHeader* mFreePrev;       // ���� FreePage�� FreePageHeader
    smpFreePageHeader* mFreeNext;       // ���� FreePage�� FreePageHeader
    scPageID           mSelfPageID;     // �ش� FreePage�� PID
    iduMutex           mMutex;          // �ش� FreePage�� Mutex
} smpFreePageHeader;



/* ----------------------------------------------------------------------------
 *                            For Slot Header
 * --------------------------------------------------------------------------*/
//�̺κ��� smpFreeSlotHeader�� ���� �޸� ������ ����ϱ� ������ �����Ҷ�, ������ ��������
//����ؾ� �Ѵ�.

typedef struct smpSlotHeader
{
    smSCN       mCreateSCN;
    smSCN       mLimitSCN;
    ULong       mPosition;
    smOID       mVarOID;
} smpSlotHeader;



/* ----------------------------------------------------------------------------
 *    SCN
 *    1. Commit SCN
 *    2. Infinite SCN + TID
 *    3. Lock Row SCN + TID
 * --------------------------------------------------------------------------*/

#define SMP_GET_TID(aSCN)             SM_GET_TID_FROM_INF_SCN(aSCN)

#define SMP_SLOT_IS_LOCK_TRUE(aHdr)   SM_SCN_IS_LOCK_ROW((aHdr)->mLimitSCN)

#define SMP_SLOT_SET_LOCK(aHdr, aTID) SM_SET_SCN_LOCK_ROW(&((aHdr)->mLimitSCN), aTID)

#define SMP_SLOT_SET_UNLOCK(aHdr)     SM_SET_SCN_FREE_ROW(&((aHdr)->mLimitSCN))

/* ----------------------------------------------------------------------------
 *    Position (OID + OFFSET + FLAG)
 * --------------------------------------------------------------------------*/

/* OID */

#define SMP_SLOT_OID_OFFSET            (16)

#define SMP_SLOT_GET_NEXT_OID(a)       ((smOID)( (((smpSlotHeader*)(a))->mPosition) \
                                                >> SMP_SLOT_OID_OFFSET ))

#define SMP_SLOT_SET_NEXT_OID(a, oid) {                                      \
    IDE_DASSERT( SM_IS_OID( oid ) );                                         \
    IDE_DASSERT( SMP_SLOT_HAS_NULL_NEXT_OID( a ) );                          \
    (a)->mPosition |= ((ULong)(oid) << SMP_SLOT_OID_OFFSET);                 \
}

#define SMP_SLOT_HAS_VALID_NEXT_OID(a) SM_IS_VALID_OID( SMP_SLOT_GET_NEXT_OID( (a) ) )
#define SMP_SLOT_HAS_NULL_NEXT_OID(a)  SM_IS_NULL_OID( SMP_SLOT_GET_NEXT_OID( (a) ) )

/* OFFSET */

#define SMP_SLOT_OFFSET_MASK           (0x000000000000FFF8)
#define SMP_SLOT_GET_OFFSET(a)         ( (a)->mPosition & SMP_SLOT_OFFSET_MASK )
#define SMP_SLOT_SET_OFFSET(a, offset) ( (a)->mPosition = (ULong)offset )

#define SMP_SLOT_INIT_POSITION(a)      ( (a)->mPosition &= SMP_SLOT_OFFSET_MASK )


/* FLAG */

#define SMP_SLOT_USED_MASK             (0x0000000000000001)
#define SMP_SLOT_USED_TRUE             (0x0000000000000001)
#define SMP_SLOT_USED_FALSE            (0x0000000000000000)

#define SMP_SLOT_DROP_MASK             (0x0000000000000002)
#define SMP_SLOT_DROP_TRUE             (0x0000000000000002)
#define SMP_SLOT_DROP_FALSE            (0x0000000000000000)

#define SMP_SLOT_SKIP_REFINE_MASK      (0x0000000000000004)
#define SMP_SLOT_SKIP_REFINE_TRUE      (0x0000000000000004)
#define SMP_SLOT_SKIP_REFINE_FALSE     (0x0000000000000000)

#define SMP_SLOT_FLAG_MASK             (0x0000000000000007)

#define SMP_SLOT_GET_FLAGS(a) (((a)->mPosition) & SMP_SLOT_FLAG_MASK)

// slot use flag setting
#define SMP_SLOT_SET_USED(a)    ( (a)->mPosition |= SMP_SLOT_USED_TRUE )
#define SMP_SLOT_UNSET_USED(a)  ( (a)->mPosition &= ~(SMP_SLOT_USED_TRUE) )
#define SMP_SLOT_IS_USED(a)     ( ( (a)->mPosition & SMP_SLOT_USED_MASK )    \
                                  == SMP_SLOT_USED_TRUE )
#define SMP_SLOT_IS_NOT_USED(a) ( ( (a)->mPosition & SMP_SLOT_USED_MASK )    \
                                  == SMP_SLOT_USED_FALSE )
// table drop flag setting
#define SMP_SLOT_SET_DROP(a)    ( (a)->mPosition |= SMP_SLOT_DROP_TRUE )
#define SMP_SLOT_UNSET_DROP(a)  ( (a)->mPosition &= ~(SMP_SLOT_DROP_TRUE) )
#define SMP_SLOT_IS_DROP(a)     ( ( (a)->mPosition & SMP_SLOT_DROP_MASK )    \
                                  == SMP_SLOT_DROP_TRUE )
#define SMP_SLOT_IS_NOT_DROP(a) ( ( (a)->mPosition & SMP_SLOT_DROP_MASK )    \
                                  == SMP_SLOT_DROP_FALSE )

// skip refine flag setting
#define SMP_SLOT_SET_SKIP_REFINE(a)   ( (a)->mPosition |= SMP_SLOT_SKIP_REFINE_TRUE )
#define SMP_SLOT_UNSET_SKIP_REFINE(a) ( (a)->mPosition &= ~(SMP_SLOT_SKIP_REFINE_TRUE) )
#define SMP_SLOT_IS_SKIP_REFINE(a)    ( ( (a)->mPosition & SMP_SLOT_SKIP_REFINE_MASK ) \
                                        == SMP_SLOT_SKIP_REFINE_TRUE )
/* not yet used
#define SMP_SLOT_IS_NOT_SKIP_REFINE(a) ( ( (a)->mPosition & SMP_SLOT_SKIP_REFINE_MASK )\
                                         == SMP_SLOT_SKIP_REFINE_FALSE )
*/













// record pointer <-> PID
#define SMP_SLOT_GET_PID(pRow)                                           \
     (( (smpPersPageHeader *)( (SChar *)(pRow) -                          \
       SMP_SLOT_GET_OFFSET( (smpSlotHeader*)(pRow) ) ) )->mSelfPageID)

#define SMP_SLOT_GET_OID(pRow)                                           \
     SM_MAKE_OID( SMP_SLOT_GET_PID( (smpSlotHeader*)(pRow) ),            \
                  SMP_SLOT_GET_OFFSET( (smpSlotHeader *)(pRow) ) )

#define SMP_GET_PERS_PAGE_HEADER(pRow)                                   \
     ( (smpPersPageHeader *)( (SChar *)(pRow) -                          \
       SMP_SLOT_GET_OFFSET( (smpSlotHeader*)(pRow) ) ) )


#define SMP_SLOT_HEADER_SIZE (idlOS::align((UInt)ID_SIZEOF(smpSlotHeader)))

typedef struct smpFreeSlotHeader
{
    smSCN mCreateSCN;
    smSCN mLimitSCN;
    ULong mPosition;
//    smOID mVarOID;

    smpFreeSlotHeader* mNextFreeSlot;
} smpFreeSlotHeader;













/*
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 */
typedef struct smpScanPageListEntry
{
    scPageID              mHeadPageID; // Next Scan Page ID
    scPageID              mTailPageID; // Prev Scan Page ID
    iduMutex             *mMutex;      // List�� Mutex
} smpScanPageListEntry;

typedef struct smpAllocPageListEntry
{
    vULong                mPageCount;  // AllocPage ����
    scPageID              mHeadPageID; // List�� Head
    scPageID              mTailPageID; // List�� Tail
    iduMutex             *mMutex;      // List�� Mutex
                                       // (AllocList�� Durable�� �����̰�,
                                       //  Mutex�� Runtime�����̱� ������
                                       //  �����ͷ� ���´�.)
} smpAllocPageListEntry;

// TX's Private Free Page List
typedef struct smpPrivatePageListEntry
{
    smOID              mTableOID;           // PageList�� ���̺�OID
    smpFreePageHeader* mFixedFreePageHead;  // Fixed������ ���� FreePageList
    smpFreePageHeader* mFixedFreePageTail;  // Fixed������ ���� FreePageList
    smpFreePageHeader* mVarFreePageHead[SM_VAR_PAGE_LIST_COUNT];
                                            // Var������ ���� FreePageList
    // Fixed������ MVCC������ ���� Slot�� ���ܾ� �ϱ� ������ �߰� FreePage��
    // ���� ���ŵ� �� �־ ����� ����Ʈ�̳�,
    // Var������ ������ ����� ������ ���ŵǹǷ� �ܹ��� ����Ʈ�� �����Ѵ�.
} smpPrivatePageListEntry;

/*
 * BUG-25327 : [MDB] Free Page Size Class ������ Propertyȭ �ؾ� �մϴ�.
 * FreePageList�� SizeClass�� 4���� �ִ��̰�,
 * Property MEM_SIZE_CLASS_COUNT�� ���ؼ� ����ȴ�.
 */
#define SMP_MAX_SIZE_CLASS_COUNT (4)
#define SMP_SIZE_CLASS_COUNT(x)  (((smpRuntimeEntry*)(x))->mSizeClassCount)

typedef struct smpFreePageListEntry
{
    /* ---------------------------------------------------------------------
     * Free Page List�� Size Class���� �����ȴ�.
     * Size Class�� Pagge���� Free Slot������ ���� �����ȴ�.
     * Simple Logic�� ���� Free Page List�� Size Class �ִ� ������ �ʱ�ȭ�ȴ�.
     * --------------------------------------------------------------------- */
    vULong                mPageCount[SMP_MAX_SIZE_CLASS_COUNT];  // FreePage����
    smpFreePageHeader*    mHead[SMP_MAX_SIZE_CLASS_COUNT];       // List�� Head
    smpFreePageHeader*    mTail[SMP_MAX_SIZE_CLASS_COUNT];       // List�� Tail
    iduMutex              mMutex;  // for Alloc/Free Page
} smpFreePageListEntry;

// FreePagePool�� EmptyPage���� ������.
// FreePagePool������ �ܹ��⸮��Ʈ�� ������ �� ������
// FreePageList�� smpFreePageHeader�� �����ϰ�
// FreePageList�� �����ٶ� �ٽ� ����⸮��Ʈ�� �����ؾ� �ϹǷ�
// FreePageList���� �������� ���� ����⸮��Ʈ�� ������ �ʿ䰡 ����.
typedef struct smpFreePagePoolEntry
{
    vULong                mPageCount;   // EmptyPage����
    smpFreePageHeader*    mHead;        // List�� Head
    smpFreePageHeader*    mTail;        // List�� Tail
    iduMutex              mMutex;       // FreePagePool�� Mutex
} smpFreePagePoolEntry;

// FreePagePool�� FreePageList�� restart�ÿ� �籸���ϴ� ��������
// Durable���� �ʴ�.
typedef struct smpRuntimeEntry
{
    // TableHeader�� �ִ� AllocPageList�� ���� �����͸� ���´�.
    smpAllocPageListEntry* mAllocPageList;

    // Full Scan ���� Page List
    smpScanPageListEntry   mScanPageList;

    // FreePagePool�� EmptyPage�� �����Ƿ� �ϳ��� ����Ʈ��
    smpFreePagePoolEntry  mFreePagePool;                        // FreePagePool
    smpFreePageListEntry  mFreePageList[SM_MAX_PAGELIST_COUNT]; // FreePageList

    /*
     * BUG-25327 : [MDB] Free Page Size Class ������ Propertyȭ �ؾ� �մϴ�.
     */
    UInt                  mSizeClassCount;

    // Record Count ���� ���
    ULong                 mInsRecCnt;  // Inserted Record Count
    ULong                 mDelRecCnt;  // Deleted Record Count

    //BUG-17371 [MMDB] Aging�� �и���� System�� ������ ��
    //Aging�� �и��� ������ �� ��ȭ��.
    //���̺��� ������ old version �� ����
    ULong                 mOldVersionCount;
    //�ߺ�Ű�� �Է��Ϸ� �Ϸ��ϴ� abort�� Ƚ��
    ULong                 mUniqueViolationCount;
    //update�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...  �̷��� ����� �߻� Ƚ��.
    ULong                 mUpdateRetryCount;
    //remove�Ҷ� �̹� �ٸ� Ʈ������� ���ο� version�� �����Ͽ��ٸ�,
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...  �̷��� ����� �߻� Ƚ��.
    ULong                 mDeleteRetryCount;

    //LockRow�� �̹� �ٸ� Ʈ������� Row�� ����, ������ Commit�ߴٸ�
    //�� Tx�� retry�Ͽ� ���ο� scn�� ���� �ϴµ�...  �̷��� ����� �߻� Ƚ��.
    ULong                 mLockRowRetryCount;

    iduMutex              mMutex;      // Count Mutex

} smpRuntimeEntry;

typedef struct smpPageListEntry
{
    vULong                mSlotSize;   // Slot�� Size(SlotHeader ����)
    UInt                  mSlotCount;  // �� �������� �Ҵ簡���� slot ����
    smOID                 mTableOID;   // PageList�� ���� ���̺� OID

    // Runtime�ÿ� �籸���ؾ��ϴ� ������ PTR�� �����Ѵ�.
    smpRuntimeEntry       *mRuntimeEntry;
} smpPageListEntry;

// PageList�� ����ȭ�Ѵ�.
#define SMP_PAGE_LIST_COUNT     (smuProperty::getPageListGroupCount())
// PageListID�� NULL�̶�� ���� � PageList���� �޸��� ���� ��Ȳ�̴�.
#define SMP_PAGELISTID_NULL     ID_UINT_MAX
// ����ȭ�� PageList�� ������ PageListID
#define SMP_LAST_PAGELISTID     (SMP_PAGE_LIST_COUNT - 1)
// FreePagePool�� �ش��ϴ� PageListID
#define SMP_POOL_PAGELISTID     (ID_UINT_MAX - 2)
// Tx's Private Free Page List�� �ش��ϴ� PageListID
#define SMP_PRIVATE_PAGELISTID  (ID_UINT_MAX - 1)

// SizeClassID�� NULL�̶�� ���� � SizeClass�� FreePageList���� �޸��� �ʾҴ�.
#define SMP_SIZECLASSID_NULL     ID_UINT_MAX
// ����ȭ�� SizeClass FreePageList�� ������ SizeClassID
#define SMP_LAST_SIZECLASSID(x)  (SMP_SIZE_CLASS_COUNT(x) - 1)
// ������ SizeClass�� EmptyPage�� ���� SizeClass�̴�.
#define SMP_EMPTYPAGE_CLASSID(x) SMP_LAST_SIZECLASSID(x)
// Tx's Private Free Page List�� �ش��ϴ� PageListID
#define SMP_PRIVATE_SIZECLASSID (ID_UINT_MAX - 1)

// ������Ƽ�� ������ MIN_PAGES_ON_TABLE_FREE_LIST��ŭ �׻� PageList�� ����� �ȴ�.
#define SMP_FREEPAGELIST_MINPAGECOUNT (smuProperty::getMinPagesOnTableFreeList())
// PageList�� ����� ������ŭ�� FreePagePool���� FreePageList�� �����´�.
#define SMP_MOVEPAGECOUNT_POOL2LIST   (smuProperty::getMinPagesOnTableFreeList())
// DB���� Page�� �Ҵ������ FreePageList�� �ʿ��� ��ŭ�� �����´�.
#define SMP_ALLOCPAGECOUNT_FROMDB     (smuProperty::getAllocPageCount())

/* smpFixedPageList::allocSlot�� Argument */
#define  SMP_ALLOC_FIXEDSLOT_NONE           (0x00000000)
// Allocate�� ��û�ϴ� Table �� Record Count�� ������Ų��.
#define  SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT  (0x00000001)
// �Ҵ���� Slot�� Header�� Update�ϰ� Logging�϶�.
#define  SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER (0x00000002)

#endif /* _O_SMP_DEF_H_ */
