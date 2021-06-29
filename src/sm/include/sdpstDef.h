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
 * $Id$
 *
 * �� ������ Treelist Managed Segment ����� ���� �ڷᱸ���� ������
 * ��������̴�.
 *
 ***********************************************************************/

# ifndef _O_SDPST_DEF_H_
# define _O_SDPST_DEF_H_ 1

# include <iduMutex.h>
# include <sdpDef.h>

/*
 * BMP Type & Stack Depth
 * BMP ���� type�� stack depth�� �����Ѵ�.
 *
 * EMPTY, VIRTBMP Ÿ���� BMP �� �������� �ʴ´�. �̵��� ���� sdpstStackMgr
 * �� �����ϱ� ���� �����ϴ� Ÿ���̴�.
 *
 * ���� sdpstBfrAllocExtInfo, sdpstAftAllocExtInfo ���� ����
 * VIRTBMP�� EXTDIR�� ���ȴ�.
 *
 * Treelist�� Depth�� 4�̴�.
 */
typedef enum sdpstBMPType
{
    SDPST_EMPTY   = -1,
    SDPST_VIRTBMP = 0,
    SDPST_RTBMP,
    SDPST_ITBMP,
    SDPST_LFBMP,
    SDPST_BMP_TYPE_MAX
} sdpstBMPType;

/* sdpstBfrAllocExtInfo, sdpstAftAllocExtInfo �� ���ؼ� ������. */
#define SDPST_EXTDIR    (SDPST_VIRTBMP)

/*
 * Ư�� ����� Ư�� Slot�� ������ ��Ÿ����.
 */
typedef struct sdpstPosItem
{
    scPageID         mNodePID;
    SShort           mIndex;
    UChar            mAlign[2];
} sdpstPosItem;

/*
 * Treelist������ ��ġ�� ǥ���ϴ� �ڷᱸ���� Stack�� �����Ѵ�.
 *
 * �⺻������ Treelist�� traverse�ϴµ� ���Ǹ�,  HWM��
 * Hint It BMP �������� ��ġ�� �����ϰ� �����ϱ� ���� �ڷᱸ���̸�,
 * Treelist�������� ������ġ ���踦 ���ϴµ� �ʼ����� �ڷᱸ���̴�.
 */
typedef struct sdpstStack
{
    sdpstPosItem    mPosition[ SDPST_BMP_TYPE_MAX ];
    SInt            mDepth;
    UChar           mAlign[4];
} sdpstStack;

/* ���� bmp�鿡 ���� ���뵵 ���� ���� ���� ��ġ ���� */
typedef enum sdpstChangeMFNLPhase
{
    SDPST_CHANGEMFNL_RTBMP_PHASE = 0,
    SDPST_CHANGEMFNL_ITBMP_PHASE,
    SDPST_CHANGEMFNL_LFBMP_PHASE,
    SDPST_CHANGEMFNL_MAX_PHASE
} sdpstChangeMFNLPhase;

/* Page�� High/Low Water Mark ���� */
typedef struct sdpstWM
{
    scPageID        mWMPID;          /* WM PID */
    scPageID        mExtDirPID;      /* ExtDir �������� PID */
    sdpstStack      mStack;          /* WM stack */
    SShort          mSlotNoInExtDir; /* ExtDir �������������� ExtDesc ���� */
    UChar           mAlign[6]; 
} sdpstWM;

/* Bitmap ������������ ��ȿ���� ���� Slot ����
 * ( Slot �˻��� ��ȿ���� �ʴ� ��쿡 ��ȯ������ ����Ѵ�) */
# define SDPST_INVALID_SLOTNO         (-1)

// leaf bitmap ������������ ��ȿ���� ���� Page ���� (PBS = PageBitSet)
# define SDPST_INVALID_PBSNO         (-1)

// ������ Bitmap ������ ���� �������� �ʴ´�.
# define SDPST_FAR_AWAY_OFF            (ID_SSHORT_MAX)

// leaf bitmap page�� �ִ� ������� ���� ���� 6����
typedef enum sdpstMFNL
{
    SDPST_MFNL_FUL = 0, /* FULL      */
    SDPST_MFNL_INS,      /* INSERTABLE */
    SDPST_MFNL_FMT,      /* FORMAT    */
    SDPST_MFNL_UNF,      /* UNFORMAT  */
    SDPST_MFNL_MAX       /* �ִ밳��  */
} sdpstMFNL;


/*
 * Segment ũ���� �������� �����Ǵ� leaf bitmap page��
 * range�� �����Ѵ�.
 */
typedef enum
{
    SDPST_PAGE_RANGE_NULL = 0,     /* NULL */
    SDPST_PAGE_RANGE_16   = 16,    /*        Segment Size < 1m  */
    SDPST_PAGE_RANGE_64   = 64,    /*  1M <= Segment Size < 64M */
    SDPST_PAGE_RANGE_256  = 256,   /* 64M <= Segment Size < 1G  */
    SDPST_PAGE_RANGE_1024 = 1024   /*  1G <= Segment Size       */
} sdpstPageRange;

/* Ž�� ���� Internal BMP Hint */
typedef struct sdpstSearchHint
{
    /* Hint �缳�� ���� */
    idBool           mUpdateHintItBMP;

    /* Hint Stack ���� ���ü��� ���� Latch */
    iduLatch      mLatch4Hint;

    /* ��ġ�̷� */
    sdpstStack       mHintItStack;
} sdpstSearchHint;

/*
 * Segment Cache �ڷᱸ�� (Runtime ����)
 */
typedef struct sdpstSegCache
{
    sdpSegCCache     mCommon;

    iduLatch         mLatch4WM;          /* WM ���� ���ü��� ���� Mutex */
    iduMutex         mExtendExt;         /* Segment ���� Ȯ���� ���� Mutex */
    idBool           mOnExtend;          /* Segment Ȯ�� ���� ���� */
    iduCond          mCondVar;           /* Condition Variable */
    UInt             mWaitThrCnt4Extend; /* Waiter */

    sdpSegType       mSegType;

    /* Slot �Ҵ��� ���� �������Ž�� ����
     * internal bitmap page�� Hint*/
    sdpstSearchHint  mHint4Slot;

    /* Page �Ҵ��� ���� �������Ž�� ����
     * internal bitmap page�� Hint*/
    sdpstSearchHint  mHint4Page;

    /* Candidate Child Set�� ����� ���� Hint */
    UShort           mHint4CandidateChild;

    /* Format Page count */
    ULong            mFmtPageCnt;

    /* HWM */
    sdpstWM          mHWM;

    /* PRJ-1671 Bitmap Tablespace, Load HWM
     * BUG-29005 Fullscan ���� ����
     * ���������� �Ҵ�� ������������ Fullscan�Ѵ�. */
    iduMutex         mMutex4LstAllocPage;

    /* FullScan Hint ��� ���� */
    idBool           mUseLstAllocPageHint;

    /* ���������� �Ҵ�� ������ ���� */
    scPageID         mLstAllocPID;
    ULong            mLstAllocSeqNo;

    /* hint insertable page hint array
     * BUG-28935���� ó�� ��� �� alloc�޵��� ����*/
    scPageID       * mHint4DataPage;
} sdpstSegCache;

// Segment�� Extent �Ҵ翬�� �߿�  �����ؾ���
// Bitmap page�鿡 ���� ������ ������ �ڷᱸ��
typedef struct sdpstBfrAllocExtInfo
{
    /* ������ rt, it, lf-bmp, ExtDir�� PID */
    scPageID        mLstPID[SDPST_BMP_TYPE_MAX];

    UShort          mFreeSlotCnt[SDPST_BMP_TYPE_MAX];
    UShort          mMaxSlotCnt[SDPST_BMP_TYPE_MAX];

    /* Segment�� �� ������ ����  */
    ULong           mTotPageCnt;

    /* ���� Page Range */
    sdpstPageRange  mPageRange;
    UShort          mFreePageRangeCnt;

    SShort          mSlotNoInExtDir;
    SShort          mLstPBSNo;

    ULong           mNxtSeqNo;

} sdpstBfrAllocExtInfo;

typedef struct sdpstAftAllocExtInfo
{
    /* Ȯ������ ���ο� ù��° bmp, ExtDir �������� PID */
    scPageID        mFstPID[SDPST_BMP_TYPE_MAX];
    /* Ȯ������ ���ο� ������ bmp, ExtDir �������� PID */
    scPageID        mLstPID[SDPST_BMP_TYPE_MAX];


    /* �̹��� ���Ӱ� ������ Meta �������� ���� ��
     * ������ �������� �ִ� slot ���� */
    UShort          mPageCnt[SDPST_BMP_TYPE_MAX];
    UShort          mFullBMPCnt[SDPST_BMP_TYPE_MAX];
//    UShort          mMaxSlotCnt[SDPST_BMP_TYPE_MAX];

    /* ���ο� Segment�� �� ������ ����  */
    ULong           mTotPageCnt;

    /* ExtDir�� �����ؾ��� Leaf BMP ������ */
    scPageID        mLfBMP4ExtDir;

    /* ���� Page Range */
    sdpstPageRange  mPageRange;

    /* �̹��� ���Ӱ� ������ Segment Meta Header �������� ����
     * �߿�: 0 �Ǵ� 1 ���� ���´�. */
    UShort          mSegHdrCnt;

    SShort          mSlotNoInExtDir;
} sdpstAftAllocExtInfo;


/* Extent Slot ���� */
typedef struct sdpstExtDesc
{
    scPageID       mExtFstPID;     /* Extent ù��° �������� PID */
    UInt           mLength;        /* Extent�� ������ ���� */
    scPageID       mExtMgmtLfBMP;  /* ExtDir �������� �����ϴ� lf-bmp
                                      �������� PID */
    scPageID       mExtFstDataPID; /* Extent�� ù��° Data �������� PID */
} sdpstExtDesc;

// ExtDir Control Header ����
typedef struct sdpstExtDirHdr
{
    UShort       mExtCnt;          // ���������� Extent ����
    UShort       mMaxExtCnt;       // ���������� �ִ� ExtDesc ����
    scOffset     mBodyOffset;       // ������ ���� ExtDir�� Offset
    UChar        mAlign[2];
} sdpstExtDirHdr;


/* BMP Slot ���� */
typedef struct sdpstBMPSlot
{
    scPageID      mBMP;   // rt/it-BMP �������� PID
    sdpstMFNL     mMFNL;  // rt/it-BMP �������� Maximum Freeness Level
} sdpstBMPSlot;

/* Leaf Bitmap �������� Page RaOBnge Slot ���� */
# define SDPST_MAX_RANGE_SLOT_COUNT   (16)

/* Ž���������� Unformat �������� ������ ��� */
# define SDPST_MAX_FORMAT_PAGES_AT_ONCE (16)

/* �ϳ��� Page�� ǥ���ϴ� bitset�� ���� */
# define SDPST_PAGE_BITSET_SIZE  (8)

/*
 * lf-BMP�� Page Bitset Table ũ��
 * SDPST_PAGE_BITSET_TABLE_SIZE / SDPST_PAGE_BITSET_SIZE = 1024 ������ ǥ��
 */
# define SDPST_PAGE_BITSET_TABLE_SIZE ( 1024 )

/*
 * lf-bmp���� ������ Data Page�� range�� ������ �����ϱ� ���� range slot
 */
typedef struct sdpstRangeSlot
{
    scPageID    mFstPID;            // Range slot�� ù��° �������� PID
    UShort      mLength;            // Range slot�� ������ ����
    SShort      mFstPBSNo;          // Range�������� Range slot�� ù��°
                                    // �������� ����
    scPageID    mExtDirPID;         // ExtDir Page�� PID
    SShort      mSlotNoInExtDir;    // ExtDir���� Extent Slot No
    UChar       mAlign[2];
} sdpstRangeSlot;

/* Page BitSet�� 1����Ʈ�� �����Ѵ�. */
typedef UChar sdpstPBS;

/*
 * lf-BMP ���� Page range�� �����ϱ� ���� map
 */
typedef struct sdpstRangeMap
{
    sdpstRangeSlot  mRangeSlot[ SDPST_MAX_RANGE_SLOT_COUNT ];
    sdpstPBS        mPBSTbl[ SDPST_PAGE_BITSET_TABLE_SIZE ];
}sdpstRangeMap;

/*
 * Bitmap �������� Header
 */
typedef struct sdpstBMPHdr
{
    sdpstBMPType    mType;
    sdpstMFNL       mMFNL;              /* �������� MFNL */
    UShort          mMFNLTbl[SDPST_MFNL_MAX];   /* slot�� MFNL�� ���� */
    UShort          mSlotCnt;           /* ���������� BMP �������� ���� */
    UShort          mFreeSlotCnt;       /* ������ ���� free slot ���� */
    UShort          mMaxSlotCnt;        /* ������ ���� �ִ� BMP ���� */
    SShort          mFstFreeSlotNo;     /* ù��° free slot ���� */
    sdpParentInfo   mParentInfo;        /* ���� BMP ������ PID, slot no */
    scPageID        mNxtRtBMP;          /* ���� RtBMP PID. RtBMP�� ��� */
    scOffset        mBodyOffset;        /* ���������� BMP map��  Offset */
    UChar           mAlign[2];
} sdpstBMPHdr;

/*
 * Leaf Bitmap �������� Header
 */
typedef struct sdpstLfBMPHdr
{
    sdpstBMPHdr     mBMPHdr;

    sdpstPageRange  mPageRange;       // Leaf BMP�� ������ Page Range
    UShort          mTotPageCnt;

    // ���� ��밡���ߴ� Data �������� ����
    SShort          mFstDataPagePBSNo;  // Leaf BMP���� ù��° Data ������ Index
} sdpstLfBMPHdr;

/* Segment Size�� Page ������ ��Ÿ�� */
# define SDPST_PAGE_CNT_1M     (    1 * 1024 * 1024 / SD_PAGE_SIZE )
# define SDPST_PAGE_CNT_64M    (   64 * 1024 * 1024 / SD_PAGE_SIZE )
# define SDPST_PAGE_CNT_1024M  ( 1024 * 1024 * 1024 / SD_PAGE_SIZE )

/* leaf bitmap ������ Page BitSet�� ������ ����(TyPe)�� �����Ѵ�. */
# define SDPST_BITSET_PAGETP_MASK   (0x80)
# define SDPST_BITSET_PAGETP_META   (0x80)  /* Meta Page */
# define SDPST_BITSET_PAGETP_DATA   (0x00)  /* Data Page (Meta�� ������ ������) */

/* leaf bitmap ������ Page BitSet�� ������ ���뵵(FreeNess)�� �����Ѵ�. */
# define SDPST_BITSET_PAGEFN_MASK   (0x7F)
# define SDPST_BITSET_PAGEFN_UNF    (0x00)  /* Unformat */
# define SDPST_BITSET_PAGEFN_FMT    (0x01)  /* Format */
# define SDPST_BITSET_PAGEFN_INS    (0x02)  /* Insertable */
# define SDPST_BITSET_PAGEFN_FUL    (0x04)  /* Full */

/* Free Data ������ �ĺ� �ۼ� ��Ʈ�� */
# define SDPST_BITSET_CANDIDATE_ALLOCPAGE ( SDPST_BITSET_PAGEFN_UNF  | \
                                            SDPST_BITSET_PAGEFN_FMT  )

/* Insertable Data ������ �ĺ� �ۼ� ��Ʈ�� ���� */
# define SDPST_BITSET_CANDIDATE_ALLOCSLOT ( SDPST_BITSET_PAGEFN_INS  | \
                                            SDPST_BITSET_PAGEFN_UNF  | \
                                            SDPST_BITSET_PAGEFN_FMT  )

/* formate�� �������� Insertable�� ������ ��Ʈ��  ���� */
# define SDPST_BITSET_CHECK_ALLOCSLOT ( SDPST_BITSET_PAGEFN_INS  | \
                                        SDPST_BITSET_PAGEFN_FMT  )

/*
 * Segment Header ������ ����
 * Segment�� Treelist�� Extent ���� BMP �������� �����Ѵ�.
 */
typedef struct sdpstSegHdr
{
    /* Segment Type�� State */
    sdpSegType          mSegType;
    sdpSegState         mSegState;

    /* Segment Header�� �����ϴ� PID */
    scPageID            mSegHdrPID;

    /* ������ BMP�� PID & Page SeqNo */
    scPageID            mLstLfBMP;
    scPageID            mLstItBMP;
    scPageID            mLstRtBMP;
    ULong               mLstSeqNo;

    /* Segment�� �Ҵ�� �� ������ ���� */
    ULong               mTotPageCnt;
    /* Segment�� �Ҵ�� �� RtBMP ������ ���� */
    ULong               mTotRtBMPCnt;
    /* Free ������ ���� */
    ULong               mFreeIndexPageCnt;
    /* Segment�� �Ҵ�� Extent �� ���� */
    ULong               mTotExtCnt;
    /* Extent�� ������ ���� */
    UInt                mPageCntInExt;
    UChar               mAlign[4];
    /* ���������� �Ҵ�� Extent �� RID */
    sdRID               mLstExtRID;

    /* HWM */
    sdpstWM             mHWM;

    /* Meta Page ID Array: Index Seg�� �� PageID Array�� Root Node PID��
     * ����Ѵ�. */
    scPageID            mArrMetaPID[ SDP_MAX_SEG_PID_CNT ];

    /* ExtDir PID List */
    sdpDblPIDListBase   mExtDirBase;

    /* Segment Header�� ���� ������ RtBMP�� ExtDir�� ����Ѵ�. */
    sdpstExtDirHdr      mExtDirHdr;
    sdpstBMPHdr         mRtBMPHdr;
} sdpstSegHdr;


/*
 * ������� Ž�� ���
 */
typedef enum sdpstSearchType
{
    SDPST_SEARCH_NEWSLOT = 0,
    SDPST_SEARCH_NEWPAGE
} sdpstSearchType;

/*
 * Ž���� �ӽ÷� �ʿ��� �ĺ� Data ������ ������ �����Ѵ�.
 */
typedef struct sdpstCandidatePage
{
    scPageID        mPageID;
    SShort          mPBSNo;
    sdpstPBS        mPBS;
    UShort          mRangeSlotNo;
} sdpstCandidatePage;

/*
 * BMP�� Fix and GetPage �Լ� ����
 */
typedef IDE_RC (*sdpstFixAndGetHdr4UpdateFunc)( idvSQL       * aStatistics,
                                                sdrMtx       * aMtx,
                                                scSpaceID      aSpaceID,
                                                scPageID       aPageID,
                                                sdpstBMPHdr ** aBMPHdr );

typedef IDE_RC (*sdpstFixAndGetHdr4ReadFunc)( idvSQL       * aStatistics,
                                              sdrMtx       * aMtx,
                                              scSpaceID      aSpaceID,
                                              scPageID       aPageID,
                                              sdpstBMPHdr ** aBMPHdr );

typedef IDE_RC (*sdpstFixAndGetHdrFunc)( idvSQL       * aStatistics,
                                         scSpaceID      aSpaceID,
                                         scPageID       aPageID,
                                         sdpstBMPHdr ** aBMPHdr );

/* BMP ����� �����´�. */
typedef sdpstBMPHdr * (*sdpstGetBMPHdrFunc)( UChar * aPagePtr );

/* SlotCnt �Ǵ� PageCnt�� �����´�. */
typedef UShort (*sdpstGetSlotOrPageCntFunc)( UChar * aPagePtr );

/* depth���� stack���� �Ÿ����� ���Ѵ�. */
typedef SShort (*sdpstGetDistInDepthFunc)( sdpstPosItem * aLHS,
                                           sdpstPosItem * aRHS );

/*
 * for makeCandidateChild
 */
/* Candidate Child ���θ� Ȯ���Ѵ�. */
typedef idBool (*sdpstIsCandidateChildFunc)( UChar     * aPagePtr,
                                             SShort      aSlotNo,
                                             sdpstMFNL   aTargetMFNL );

/* Candidate Child ������ Ž�� ���� ��ġ�� ã�´�. */
typedef UShort (*sdpstGetStartSlotOrPBSNoFunc)( UChar  * aPagePtr,
                                                SShort   aTransID );

/* Candidate Child ������ Ž�� ��� ������ ���Ѵ�. */
typedef SShort (*sdpstGetChildCountFunc)( UChar         * aPagePtr,
                                          sdpstWM       * aHWM );

/* �ĺ��� �����ϴ� �迭�� �ĺ��� �����Ѵ�. */
typedef void (*sdpstSetCandidateArrayFunc)( UChar     * aPagePtr,
                                            UShort      aSlotNo,
                                            UShort      aArrIdx,
                                            void      * aCandidateArray );

/* ù��° free slot �Ǵ� free PBS */
typedef SShort (*sdpstGetFstFreeSlotOrPBSNoFunc)( UChar     * aPagePtr );

/* �ִ� �ĺ� ���� ������ ���Ѵ�. */
typedef UInt (*sdpstGetMaxCandidateCntFunc)();

/*
 * updateBMPuntilHWM
 */
typedef IDE_RC (*sdpstUpdateBMPUntilHWMFunc)( idvSQL            * aStatistics,
                                              sdrMtxStartInfo   * aStartInfo,
                                              scSpaceID           aSpaceID,
                                              scPageID            aSegPID,
                                              sdpstStack        * aHWMStack,
                                              sdpstStack        * aCurStack,
                                              SShort            * aFmSlotNo,
                                              SShort            * aFmItSlotNo,
                                              sdpstBMPType      * aPrvDepth,
                                              idBool            * aIsFinish,
                                              sdpstMFNL         * aNewMFNL );

/* MFNL�� �������� Ȯ���Ѵ�. */
typedef IDE_RC (*sdpstIsEqUnfFunc)( sdpstBMPHdr * aBMPHdr,
                                    UShort        aSlotNo );

/* ���� BMP�� �̵��Ѵ�. */
typedef void (*sdpstGoParentBMPFunc)( sdpstBMPHdr   * aBMPHdr,
                                      sdpstStack    * aStack );

/* ���� BMP�� �̵��Ѵ�. */
typedef void (*sdpstGoChildBMPFunc)( sdpstStack    * aStack,
                                     sdpstBMPHdr   * aBMPHdr,
                                     UShort          aSlotNo );

typedef scPageID (*sdpstGetLstChildBMPFunc)( sdpstBMPHdr * aBMPHdr );

typedef struct sdpstBMPOps
{
    sdpstGetBMPHdrFunc                 mGetBMPHdr;

    sdpstGetSlotOrPageCntFunc          mGetSlotOrPageCnt;
    sdpstGetDistInDepthFunc            mGetDistInDepth;

    /* �ĺ� Ž�� ���� �Լ� */
    sdpstIsCandidateChildFunc          mIsCandidateChild;
    sdpstGetStartSlotOrPBSNoFunc       mGetStartSlotOrPBSNo;
    sdpstGetChildCountFunc             mGetChildCount;
    sdpstSetCandidateArrayFunc         mSetCandidateArray;
    sdpstGetFstFreeSlotOrPBSNoFunc     mGetFstFreeSlotOrPBSNo;
    sdpstGetMaxCandidateCntFunc        mGetMaxCandidateCnt;

    /* updateBMPUntilHWM */
    sdpstUpdateBMPUntilHWMFunc         mUpdateBMPUntilHWM;
} sdpstBMPOps;

# define SDPST_SEARCH_QUOTA_IN_RTBMP        (3)

# define SDPST_MAX_CANDIDATE_LFBMP_CNT      (1024)
# define SDPST_MAX_CANDIDATE_PAGE_CNT       (1024)

/* Search Ÿ�Կ� ���� �ּ� �����ؾ��ϴ� MFNL ���� */
# define SDPST_SEARCH_TARGET_MIN_MFNL( aSearchType ) \
    (aSearchType == SDPST_SEARCH_NEWPAGE ? SDPST_MFNL_FMT : SDPST_MFNL_INS )

/* TMS���� ����ϴ� ������Ƽ */
# define SDPST_SEARCH_WITHOUT_HASHING()     \
    ( smuProperty::getTmsSearchWithoutHashing() )
# define SDPST_CANDIDATE_LFBMP_CNT()        \
    ( smuProperty::getTmsCandidateLfBMPCnt() )
# define SDPST_CANDIDATE_PAGE_CNT()         \
    ( smuProperty::getTmsCandidatePageCnt() )

# define SDPST_MAX_SLOT_CNT_PER_RTBMP()     \
    ( smuProperty::getTmsMaxSlotCntPerRtBMP() )
# define SDPST_MAX_SLOT_CNT_PER_ITBMP()     \
    ( smuProperty::getTmsMaxSlotCntPerItBMP() )
# define SDPST_MAX_SLOT_CNT_PER_LFBMP()     \
    ( SDPST_MAX_RANGE_SLOT_COUNT )
# define SDPST_MAX_SLOT_CNT_PER_EXTDIR()    \
    ( smuProperty::getTmsMaxSlotCntPerExtDir() )

# define SDPST_MAX_SLOT_CNT_PER_RTBMP_IN_SEGHDR()        \
    ( (SDPST_MAX_SLOT_CNT_PER_RTBMP() <=                 \
            sdpstBMP::getMaxSlotCnt4Property() / 2) ?    \
      SDPST_MAX_SLOT_CNT_PER_RTBMP() :                   \
      SDPST_MAX_SLOT_CNT_PER_RTBMP() / 2 )
# define SDPST_MAX_SLOT_CNT_PER_EXTDIR_IN_SEGHDR()       \
    ( (SDPST_MAX_SLOT_CNT_PER_EXTDIR() <=                \
            sdpstExtDir::getMaxSlotCnt4Property() / 2) ? \
      SDPST_MAX_SLOT_CNT_PER_EXTDIR() :                  \
      SDPST_MAX_SLOT_CNT_PER_EXTDIR() / 2 )

/*
 * SDR_SDPST_INIT_SEGHDR
 */
typedef struct sdpstRedoInitSegHdr
{
    sdpSegType      mSegType;
    scPageID        mSegPID;
    UInt            mPageCntInExt;
    UShort          mMaxExtDescCntInExtDir;
    UShort          mMaxSlotCntInRtBMP;
} sdpstRedoInitSegHdr;

/*
 * SDR_SDPST_UPDATE_WM
 */
typedef struct sdpstRedoUpdateWM
{
    scPageID        mWMPID;
    scPageID        mExtDirPID;
    sdpstStack      mStack;
    SShort          mSlotNoInExtDir;
    UChar           mAligh[6];
} sdpstRedoUpdateWM;

/*
 * SDR_SDPST_INIT_BMP
 */
typedef struct sdpstRedoInitBMP
{
    sdpstBMPType    mType;
    sdpParentInfo   mParentInfo;
    scPageID        mFstChildBMP;
    scPageID        mLstChildBMP;
    UShort          mFullBMPCnt;
    UShort          mMaxSlotCnt;
} sdpstRedoInitBMP;

/*
 * SDR_SDPST_INIT_LFBMP
 */
typedef struct sdpstRedoInitLfBMP
{
    sdpstPageRange  mPageRange;
    sdpParentInfo   mParentInfo;
    scPageID        mRangeFstPID;
    scPageID        mExtDirPID;
    UShort          mNewPageCnt;
    UShort          mMetaPageCnt;
    SShort          mSlotNoInExtDir;
    UShort          mMaxSlotCnt;
} sdpstRedoInitLfBMP;

/*
 * SDR_SDPST_INIT_EXTDIR
 */
typedef struct sdpstRedoInitExtDir
{
    UShort          mMaxExtCnt;
    scOffset        mBodyOffset;
} sdpstRedoInitExtDir;

/*
 * SDR_SDPST_ADD_EXTDESC
 */
typedef struct sdpstRedoAddExtDesc
{
    scPageID       mExtFstPID;
    UInt           mLength;
    scPageID       mExtMgmtLfBMP;
    scPageID       mExtFstDataPID;
    UShort         mLstSlotNo;
} sdpstRedoAddExtDesc;

/*
 * SDR_SDPST_ADD_SLOT
 */
typedef struct sdpstRedoAddSlots
{
    scPageID        mFstChildBMP;
    scPageID        mLstChildBMP;
    UShort          mLstSlotNo;
    UShort          mFullBMPCnt;
} sdpstRedoAddSlots;

/*
 * SDR_SDPST_ADD_RANGESLOT
 */
typedef struct sdpstRedoAddRangeSlot
{
    scPageID        mFstPID;
    scPageID        mExtDirPID;
    UShort          mLength;
    UShort          mMetaPageCnt;
    SShort          mSlotNoInExtDir;
} sdpstRedoAddRangeSlot;

/*
 * SDR_SDPST_ADD_EXT_TO_SEGHDR
 */
typedef struct sdpstRedoAddExtToSegHdr
{
    ULong           mAllocPageCnt;
    ULong           mMetaPageCnt;
    scPageID        mNewLstLfBMP;
    scPageID        mNewLstItBMP;
    scPageID        mNewLstRtBMP;
    UShort          mNewRtBMPCnt;
} sdpstRedoAddExtToSegHdr;

/*
 * SDR_SDPST_UPDATE_PBS
 */
typedef struct sdpstRedoUpdatePBS
{
    SShort          mFstPBSNo;
    UShort          mPageCnt;
    sdpstPBS        mPBS;
} sdpstRedoUpdatePBS;

/*
 * SDR_SDPST_UPDATE_MFNL
 */
typedef struct sdpstRedoUpdateMFNL
{
    sdpstMFNL       mFmMFNL;
    sdpstMFNL       mToMFNL;
    UShort          mPageCnt;
    SShort          mFmSlotNo;
    SShort          mToSlotNo;
} sdpstRedoUpdateMFNL;

/*
 * SDR_SDPST_UPDATE_LFBMP_4DPATH
 */
typedef struct sdpstRedoUpdateLfBMP4DPath
{
    SShort          mFmPBSNo;
    UShort          mPageCnt;
} sdpstRedoUpdateLfBMP4DPath;

/*
 * SDR_SDPST_UPDATE_BMP_4DPATH
 */
typedef struct sdpstRedoUpdateBMP4DPath
{
    SShort          mFmSlotNo;
    UShort          mSlotCnt;
    sdpstMFNL       mToMFNL;
    sdpstMFNL       mMFNLOfLstSlot;
} sdpstRedoUpdateBMP4DPath;

# endif // _O_SDPST_DEF_H_
