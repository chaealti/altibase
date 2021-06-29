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
 

/*******************************************************************************
 * $Id: sdpDef.h 90259 2021-03-19 01:22:22Z emlee $
 *
 * Description :
 *      �� ������ page layer�� �ڷᱸ���� ������ ��������̴�.
 ******************************************************************************/

#ifndef _O_SDP_DEF_H_
#define _O_SDP_DEF_H_ 1

#include <smDef.h>
#include <sdbDef.h>
#include <sdrDef.h>

# define SDP_MIN_EXTENT_PAGE_CNT   (5)
# define SDP_MAX_SEG_PID_CNT       (16)
#if defined(SMALL_FOOTPRINT)
# define SDP_MAX_TSSEG_PID_CNT     (256)
# define SDP_MAX_UDSEG_PID_CNT     (256)
#else
# define SDP_MAX_TSSEG_PID_CNT     (512)
# define SDP_MAX_UDSEG_PID_CNT     (512)
#endif

/*
 * UDS/TSS�� ���� Free Extent Dir. List Ÿ�� ����
 */
typedef enum sdpFreeExtDirType
{
    SDP_TSS_FREE_EXTDIR_LIST,
    SDP_UDS_FREE_EXTDIR_LIST,
    SDP_MAX_FREE_EXTDIR_LIST
} sdpFreeExtDirType;

/* PhyPage�� Ư�� ������ ����Ʈ�� ���ԵǾ� �ִ����� ����Ų��. */
typedef enum sdpPageListState
{
    SDP_PAGE_LIST_UNLINK = 0,
    SDP_PAGE_LIST_LINK
} sdpPageListState;

typedef struct sdpExtInfo      sdpExtInfo;
typedef struct sdpSegInfo      sdpSegInfo;
typedef struct sdpSegCacheInfo sdpSegCacheInfo;

/* --------------------------------------------------------------------
 * page�� Ÿ��
 * �� Ÿ������ keymap�� �������� �ƴ����� ������ �� �ִ�.
 * ----------------------------------------------------------------- */
typedef enum
{
    // Free or used
    SDP_PAGE_UNFORMAT = 0,
    SDP_PAGE_FORMAT,
    SDP_PAGE_INDEX_META_BTREE,
    SDP_PAGE_INDEX_META_RTREE,
    SDP_PAGE_INDEX_BTREE,
    SDP_PAGE_INDEX_RTREE,
    SDP_PAGE_DATA,
    SDP_PAGE_TEMP_TABLE_META,   // not used
    SDP_PAGE_TEMP_TABLE_DATA,   // not used
    SDP_PAGE_TSS,
    SDP_PAGE_UNDO,
    SDP_PAGE_LOB_DATA,
    SDP_PAGE_LOB_INDEX,

    /* Freelist Managed Segment�� ������ Ÿ�� ���� */
    SDP_PAGE_FMS_SEGHDR,
    SDP_PAGE_FMS_EXTDIR,

    /* Treelist Managed Segment�� ������ Ÿ�� ���� */
    SDP_PAGE_TMS_SEGHDR,
    SDP_PAGE_TMS_LFBMP,
    SDP_PAGE_TMS_ITBMP,
    SDP_PAGE_TMS_RTBMP,
    SDP_PAGE_TMS_EXTDIR,

    /* Recycled-list Managed Segment�� ������ Ÿ�� ���� */
    SDP_PAGE_CMS_SEGHDR,
    SDP_PAGE_CMS_EXTDIR,

    SDP_PAGE_FEBT_GGHDR,   // File Super Header
    SDP_PAGE_FEBT_LGHDR,   // Extent Group Header

    /* PROJ-1704 MVCC renewal */
    SDP_PAGE_LOB_META,

    SDP_PAGE_HV_NODE,

    SDP_PAGE_TYPE_MAX           // idvTypes.h�� IDV_SM_PAGE_TYPE_MAX�� �����ؾ� �Ѵ�.

} sdpPageType;

/* --------------------------------------------------------------------
 * extent page bitmap�� 1�� bit�� ����
 * ----------------------------------------------------------------- */
typedef enum
{
    SDP_PAGE_BIT_INSERT = 0,
    SDP_PAGE_BIT_UPDATE_ONLY = 1
} sdpPageInsertBit;

////PRJ-1497
#define SDP_PAGELIST_ENTRY_RESEV_SIZE  (2)
#define SDP_EXTDIR_PAGEHDR_RESERV_SIZE (3)

/* --------------------------------------------------------------------
 * extent�� ���¸� ��Ÿ����.
 * ----------------------------------------------------------------- */
typedef enum
{
    // ���� tbs�� free list�� �޸��� ���� ����
    SDP_EXT_NULL = 0,

    // tbs�� Free list�� �޸� ����
    // segment�� ���ʿ� extent�� ��� ����
    // ��� page�� free�� ����
    SDP_EXT_FREE,

    // ���� used, ��� insert �Ұ�
    SDP_EXT_FULL,

    // ext�� �Ϻ� �������� free�� ����
    SDP_EXT_INSERTABLE_FREE,

    // �������� free�� ���� ������ insert�� ������ ���� �ִ�.
    // free space�� �� ������ extent���� ã�´�.
    SDP_EXT_INSERTABLE_NO_FREE,

    // insert�����Ѱ��� ������ free�� �ִ�.
    SDP_EXT_NO_INSERTABLE_FREE

} sdpExtState;

// BUG-45598: checksum �ʱⰪ
#define SDP_CHECKSUM_INIT_VAL 0

/* --------------------------------------------------------------------
 * checksum ����� ǥ���Ѵ�.
 * ���� LSN���� ����Ѵٸ� checksum���� page���� �ƹ� �ǹ̰� ����.
 * ----------------------------------------------------------------- */
typedef enum
{
    //  ������ �糡�� LSN ���� ����Ѵ�.
    SDP_CHECK_LSN = 0,

    // ������ ���� ó���� checksum ���� ����Ͽ� ����Ѵ�.
    SDP_CHECK_CHECKSUM

} sdpCheckSumType;

/* --------------------------------------------------------------------
 * Segement Descriptor�� type
 *
 * system �� Ȥ�� user ��
 * ----------------------------------------------------------------- */
typedef enum
{
    SDP_SEG_TYPE_NONE = 0,
    SDP_SEG_TYPE_SYSTEM,
    SDP_SEG_TYPE_DWB,
    SDP_SEG_TYPE_TSS,
    SDP_SEG_TYPE_UNDO,
    SDP_SEG_TYPE_INDEX,
    SDP_SEG_TYPE_TABLE,
    SDP_SEG_TYPE_LOB,           // PROJ-1362
    SDP_SEG_TYPE_MAX
} sdpSegType;

/* sdpsf, sdpst �� Cache�� ó���� ���� Cache������ ������. */
typedef struct sdpSegCCache
{
    /* Temp Table�� Head, Tail */
    scPageID    mTmpSegHead;
    scPageID    mTmpSegTail;

    /* PROJ-1704 MVCC renewal */
    sdpSegType  mSegType;        // Segment Type
    ULong       mSegSizeByBytes; // Segment Size
    UInt        mPageCntInExt;   // ExtDesc �� Page ����

    scPageID    mMetaPID;        // Segment Meta PID

    /* BUG-31372: It is needed that the amount of space used in
     * Segment is refered. */
    ULong       mFreeSegSizeByBytes; // Free Segment Size

    /* BUG-32091 add TableOID in PageHeader */
    smOID       mTableOID;
    /* PROJ-2281 add IndexIN in PageHeader */
    UInt        mIndexID;           /* sdpPhyPageHdr::mIndexID �� ���ŵǸ� ���� �����ؾ���. */
} sdpSegCCache;

/* --------------------------------------------------------------------
 * Segement Descriptor�� type
 *
 * system �� Ȥ�� user ��
 * ----------------------------------------------------------------- */
typedef enum
{
    SDP_SEG_FREE = 0,
    SDP_SEG_USE
} sdpSegState;

#define SDP_PAGE_CONSISTENT     (1)
#define SDP_PAGE_INCONSISTENT   (0)
/* --------------------------------------------------------------------
 * sdRID list�� ���
 * ----------------------------------------------------------------- */
typedef struct sdpSglRIDListNode
{
    sdRID     mNext; /* ���� rid */
} sdpSglRIDListNode;

/* --------------------------------------------------------------------
 * sdRID list�� ���̽�
 * ----------------------------------------------------------------- */
typedef struct sdpSglRIDListBase
{
    ULong     mNodeCnt;  /* RID List�� ��� ���� */

    sdRID     mHead;     /* Head RID */
    sdRID     mTail;     /* Tail RID */
} sdpSglRIDListBase;

/* --------------------------------------------------------------------
 * sdRID list�� ���
 * ----------------------------------------------------------------- */
typedef struct sdpDblRIDListNode
{
    sdRID     mNext; /* ���� rid */
    sdRID     mPrev; /* ���� rid */
} sdpDblRIDListNode;

/* --------------------------------------------------------------------
 * sdRID list�� ���̽�
 * ----------------------------------------------------------------- */
typedef struct sdpDblRIDListBase
{
    ULong             mNodeCnt;  /* rid list�� ��� ���� */

    sdpDblRIDListNode mBase;     /* prev �� tail�� �ǹ��ϰ�,
                                 next �� head�� �ǹ��Ѵ�. */
} sdpRIDListBase;

/* --------------------------------------------------------------------
 * page list�� ���
 * ----------------------------------------------------------------- */
typedef struct sdpSglPIDListNode
{
    scPageID     mNext; /* ���� page ID */
} sdpSglPIDListNode;

/* --------------------------------------------------------------------
 * page list�� ���̽�
 * ----------------------------------------------------------------- */
typedef struct sdpSglPIDListBase
{
    ULong         mNodeCnt;    /* page list�� ��� ���� */
    scPageID      mHead;       /* page list�� ù��° */
    scPageID      mTail;       /* page list�� ������ */
} sdpSglPIDListBase;

/* --------------------------------------------------------------------
 * page list�� ���: mNext�� mPrev �տ� �;� �Ѵ�. �ֳ��ϸ� Sigle PID
 * List�� Double PID List�� �� ��ũ ������ �����Ѵ�. Single�� ������
 * �� ��ũ������ ù��° member�� Next ��ũ������ ����ϱ⶧���� mNext
 * �� �׻� ���� �;� �Ѵ�.
 *
 * ����: mNext�� mPrev���� �ٲ����� �Ǵ� �տ� ��� �߰��� ����.
 * ----------------------------------------------------------------- */
typedef struct sdpDblPIDListNode
{
    scPageID     mNext; /* ���� page ID */
    scPageID     mPrev; /* ���� page ID */
} sdpDblPIDListNode;

/* --------------------------------------------------------------------
 * page list�� ���̽�
 * ----------------------------------------------------------------- */
typedef struct sdpDblPIDListBase
{
    ULong             mNodeCnt;    /* page list�� ��� ���� */
    sdpDblPIDListNode mBase;       /* prev �� tail�� �ǹ��ϰ�,
                                      next �� head�� �ǹ��Ѵ�. */
} sdpDblPIDListBase;


/* Treelist managed Segment���� �������� ���� ��忡
 * ���� ������ ���� */
typedef struct sdpParentInfo
{
    scPageID     mParentPID;
    SShort       mIdxInParent;
} sdpParentInfo;


/* --------------------------------------------------------------------
 * Page�� Physical Header ����
 * ----------------------------------------------------------------- */
typedef struct sdpPhyPageHdr
{
    sdbFrameHdr         mFrameHdr;

    UShort              mTotalFreeSize;
    UShort              mAvailableFreeSize;

    UShort              mLogicalHdrSize;
    UShort              mSizeOfCTL;  /* Change Trans Layer ũ�� */

    UShort              mFreeSpaceBeginOffset;
    UShort              mFreeSpaceEndOffset;

    // �� �������� �뵵�� ��Ÿ����. (sdpPageType)
    UShort              mPageType;

    // �������� ���� ( sdpscPageBitSet : free, insert ����, insert �Ұ� )
    UShort              mPageState;

    // ���̺� �����̽����� �������� ���̵�
    scPageID            mPageID;

    // BUG-17930 : log���� page�� ������ ��(nologging index build, DPath insert),
    // operation�� ���� �� �Ϸ�Ǳ� �������� page�� physical header�� �ִ�
    // mIsConsistent�� F�� ǥ���ϰ�, operation�� �Ϸ�� �� mIsConsistent��
    // T�� ǥ���Ͽ� server failure �� recovery�� �� redo�� �����ϵ��� �ؾ� ��
    UChar               mIsConsistent;

    // �������� Ư�� ����Ʈ�� ���ԵǾ� �ִ� ����
    UShort              mLinkState;

    // �������� ���� ���� ����� RID
    // �� ������ �������� ���� ������带 �� �� �ִ�.
    // page�� free�� ���� �� ������ ������带 ã�Ƽ� free��ų �� �ִ�.
    sdpParentInfo       mParentInfo;  // tms

    // page list�� ���
    sdpDblPIDListNode   mListNode;

    /* BUG-32091 [sm_collection] add TableOID in PageHeader */
    smOID               mTableOID;

    /* PROJ-2281 add Index ID in PageHeader */
    UInt                mIndexID;   /* ���õ�����, ������� �ʴº���. ���� �ٸ� �뵵�� ����Ҽ��ֵ��� ���ܵ�. */

    // Page�� �����ϴ� ���׸�Ʈ������ ���� ��ȣ
    ULong               mSeqNo;
} sdpPhyPageHdr;

/* slot directory�� ���� ��ġ��
 * slot directory header�� �����ϰ�
 * �� ������ slot entry���� �����Ѵ�. */
typedef struct sdpSlotDirHdr
{
    /* slot entry ���� */
    UShort              mSlotEntryCount;
    /* unused slot entry list�� head */
    scSlotNum           mHead;
} sdpSlotDirHdr;

/* ------------------------------------------------
 * ------------ �� ����� �ٸ� ������� ��ġ�� --------------
 * insertable page list ����
 *
 * table�� ���� segment�� 0�� ����������
 * ��� insert ������ ��� �������� list���� �����Ѵ�.
 * �̴� insert slot�� ã�� ���� entry�� ���ȴ�.
 *
 * page list��  ������ 10���� fix�Ǿ� �ִ�.
 * �� page list�� �� �������� big size�� �������
 * �з��ȴ�.
 *
 * ����Ʈ�� ���� ��ũ�� ����Ʈ�� �����ȴ�.
 *
 *
 * mPageList[0] = 2^5 (base)
 * mPageList[1] = 2^6 (base)
 * mPageList[2] = 2^7 (base)
 * ...
 * mPageList[10] = 2^15 (base)
 * ------------------������� �ٸ� ������� ��ġ�� --------------
 * -----------------���̻� page list�� �������� �ʴ´�. --------
 *
 * segment dir�� X latch�� ������ �� �������� ��� segment desc��
 * X latch�� ���� �Ͱ� ���������̴�.
 * �̴� contention�� ������ �� �����Ƿ� segment desc�� ��� ������
 * segment�� 0��° �������� �ű��.
 * �׸��� segment desc�� ���� segment 0�� �������� pid�� ���´�.
 * segment�� 0��° �������� segment header��� �Ѵ�.
 *
 * ----------------------------------------------*/

typedef struct sdpPageFooter
{
    // ���������� �������� ������ ������ lsn
    // flush ������ write �ȴ�.
    smLSN        mPageLSN;
} sdpPageFooter;

typedef UShort    sdpSlotEntry;

/* unused slot entry list����
 * next ptr�� NULL�̶�� ���� ǥ���Ҷ�
 * SDP_INVALID_SLOT_ENTRY_NUM�� ����Ѵ�. */
#define SDP_INVALID_SLOT_ENTRY_NUM (0x7FFF)

#define SDP_SLOT_ENTRY_MIN_NUM     ((SShort)-1)

#define SDP_SLOT_ENTRY_MAX_NUM     \
    ( (SShort) (SD_PAGE_SIZE / ID_SIZEOF(sdpSlotEntry) ) )

/* --------------------------------------------------------------------
 * page �ʱ�ȭ value
 * ----------------------------------------------------------------- */
#define SDP_INIT_PAGE_VALUE     (0x00)

/* --------------------------------------------------------------------
 * dump�� ���� flag
 * ----------------------------------------------------------------- */
# define SDP_DUMP_MASK         (0x00000007)
# define SDP_DUMP_DIR_PAGE     (0x00000001)
# define SDP_DUMP_EXT          (0x00000002)
# define SDP_DUMP_SEG          (0x00000004)

/*
 * ���̺����̽� Online ����� ���� Table�� ����
 * Online Action�� �Ѱ��ִ� ����
 */
typedef struct sdpActOnlineArgs
{
    void  * mTrans; // Ʈ�����
    ULong   mSmoNo; // DRDB Index Smo No
} sdpActOnlineArgs;

struct sdpExtMgmtOp;

/*
 * PROJ-1671 Bitmap-base Tablespace And Segment Space Management
 *
 * Space Cache ����
 * ���� �����ÿ� ��ũ ���̺����̽� �����ÿ� �Ҵ�Ǵ�
 * Runtime �ڷᱸ���� ���̺����̽��� Extent �Ҵ� �� ����
 * ���꿡 �ʿ��� ������ �����Ѵ�.
 *
 */
typedef struct sdpSpaceCacheCommon
{
    /* Tablespace�� ID */
    scSpaceID          mSpaceID;

    /* Extent ������� */
    smiExtMgmtType     mExtMgmtType;

    /* Segment ������� */
    smiSegMgmtType     mSegMgmtType;

    /* Extent�� ������ ���� */
    UInt               mPagesPerExt;

} sdpSpaceCacheCommon;

/*
 * PROJ-1671 Bitmap-based TableSpace And Segment Space Management
 *
 * Segment ���������� �ʿ��� ������ Runtime ������
 * �����ϴ� �ڷᱸ��
 *
 * Segment �������� ��Ŀ� ���� Segment Handle��
 * Runtime Cache�� �ʱ�ȭ�Ѵ�.
 */
typedef struct sdpSegHandle
{
    scSpaceID            mSpaceID;
    /* ���̺� ������ �Ҵ�Ǵ� segment desc. �ش� ���̺��� record��
       �����ϱ� ���� ��� page�� �����Ѵ�. */
    scPageID             mSegPID;

    // XXX Meta Offset�� SegDesc�� ���� Offset����. �ƴ� ������ ������ġ
    // ���� Offset���� �����ؾ���. ���� SegDesc�� ���� ��ġ�� ���� Offset��.

    /* Segment�� Storage �Ӽ� */
    smiSegStorageAttr    mSegStoAttr;
    /* Segment �Ӽ� */
    smiSegAttr           mSegAttr;

    /*
     * runtime�� �����ϴ� temporary ����
     * �Ʒ��� ������� ��� disk�� �����ϸ�, runtime�ÿ�
     * ��ũ �������κ��� �о �����Ѵ�.
     */

    /* Segment ���� ������ �ʿ��� Runtime ���� */
    void                * mCache;
} sdpSegHandle;


/*
 * Segment Cache���� Hint Position ����
 */
typedef struct sdpHintPosInfo
{
    scPageID      mSPosVtPID;
    SShort        mSRtBMPIdx;
    scPageID      mSPosRtPID;
    SShort        mSItBMPIdx;
    scPageID      mSPosItPID;
    SShort        mSLfBMPIdx;
    UInt          mSRsFlag;
    UInt          mSStFlag;
    scPageID      mPPosVtPID;
    SShort        mPRtBMPIdx;
    scPageID      mPPosRtPID;
    SShort        mPItBMPIdx;
    scPageID      mPPosItPID;
    SShort        mPLfBMPIdx;
    UInt          mPRsFlag;
    UInt          mPStFlag;
} sdpHintPosInfo;


/* PROJ-1705 */
typedef UShort (*sdpGetSlotSizeFunc)( const UChar    *aSlotPtr );


/*
 * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
 *
 * ���̺��̽��� �������� �������̽����� ������Ÿ�� ����
 */

/* ���̺����̽� ��� �ʱ�ȭ */
typedef IDE_RC (*sdptInitializeFunc)( scSpaceID          aSpaceID,
                                      smiExtMgmtType     aExtMgmtType,
                                      smiSegMgmtType     aSegMgmtType,
                                      UInt               aExtPageCount );

/* Free Extent */
typedef IDE_RC (*sdptFreeExtentFunc) ( idvSQL        * aStatistics,
                                       sdrMtx        * aMtx,
                                       scSpaceID       aSpaceID,
                                       scPageID        aExtFstPID,
                                       UInt        *   aNrDone );

/* ����Ÿ���� Autoextend ��� ���� */
typedef IDE_RC (*sdptAlterFileAutoExtendFunc)(
                           idvSQL     *aStatistics,
                           void*       aTrans,
                           scSpaceID   aSpaceID,
                           SChar      *aFileName,
                           idBool      aAutoExtend,
                           ULong       aNextSize,
                           ULong       aMaxSize,
                           SChar      *aValidDataFileName );

/* ����Ÿ���� Rename  */
typedef IDE_RC (*sdptAlterFileNameFunc)(
                           idvSQL*      aStatistics,
                           scSpaceID    aSpaceID,
                           SChar       *aOldName,
                           SChar       *aNewName );

/* ����Ÿ���� Resize ��� ���� */
typedef  IDE_RC (*sdptAlterFileResizeFunc)(
                          idvSQL       *aStatistics,
                          void         *aTrans,
                          scSpaceID     aSpaceID,
                          SChar        *aFileName,
                          ULong         aSizeWanted,
                          ULong        *aSizeChanged,
                          SChar        *aValidDataFileName );

/*
 * PROJ-1671 Bitmap-based Tablespace And Segment Space Management
 *
 * Segment �������� �������̽����� ������Ÿ�� ����
 */

/* Segment �������� ��� �ʱ�ȭ */
typedef IDE_RC (*sdpsInitializeFunc)( sdpSegHandle * aSegHandle,
                                      scSpaceID      aSpaceID,
                                      sdpSegType     aSegType,
                                      smOID          aObjectID,
                                      UInt           aIndexID );

/* Segment �������� ��� ���� */
typedef IDE_RC (*sdpsDestroyFunc)( sdpSegHandle * aSegHandle );

/* Table Segment �Ҵ� �� Segment Meta Header �ʱ�ȭ */
typedef IDE_RC (*sdpsCreateSegmentFunc)(
                       idvSQL                * aStatistics,
                       sdrMtx                * aMtx,
                       scSpaceID               aSpaceID,
                       sdpSegType              aSegType,
                       sdpSegHandle          * aSegHandle );

/* Segment ���� */
typedef IDE_RC (*sdpsDropSegmentFunc)( idvSQL             * aStatistics,
                                       sdrMtx             * aMtx,
                                       scSpaceID            aSpaceID,
                                       scPageID             aSegPID );

/* Segment ���� */
typedef IDE_RC (*sdpsResetSegmentFunc)( idvSQL           * aStatistics,
                                        sdrMtx           * aMtx,
                                        scSpaceID          aSpaceID,
                                        sdpSegHandle     * aSegHandle,
                                        sdpSegType         aSegType );

typedef IDE_RC (*sdpsExtendSegmentFunc)( idvSQL           * aStatistics,
                                         sdrMtxStartInfo  * aStartInfo,
                                         scSpaceID          aSpaceID,
                                         sdpSegHandle     * aSegHandle,
                                         UInt               aExtCount );


typedef IDE_RC (*sdpsSetMetaPIDFunc)( idvSQL           * aStatistics,
                                      sdrMtx           * aMtx,
                                      scSpaceID          aSpaceID,
                                      scPageID           aSegPID,
                                      UInt               aIndex,
                                      scPageID           aMetaPID );

typedef IDE_RC (*sdpsGetMetaPIDFunc)( idvSQL           * aStatistics,
                                      scSpaceID          aSpaceID,
                                      scPageID           aSegPID,
                                      UInt               aIndex,
                                      scPageID         * aMetaPID );

/* ���ο� ������ �Ҵ� */
typedef IDE_RC (*sdpsAllocNewPageFunc)( idvSQL           * aStatistics,
                                        sdrMtx           * aMtx,
                                        scSpaceID          aSpaceID,
                                        sdpSegHandle     * aSegHandle,
                                        sdpPageType        aPageType,
                                        UChar           ** aAllocPagePtr );

/* ������� �Ҵ����� ������ ���뵵 �� Segment ���뵵 ���� */
typedef IDE_RC (*sdpsUpdatePageState)( idvSQL           * aStatistics,
                                       sdrMtx           * aMtx,
                                       scSpaceID          aSpaceID,
                                       sdpSegHandle     * aSegHandle,
                                       UChar            * aDataPagePtr );


/* ��û�� ������ŭ Free �������� Segment�� Ȯ�� */
typedef IDE_RC (*sdpsPrepareNewPagesFunc)( idvSQL            * aStatistics,
                                           sdrMtx            * aMtxForSMO,
                                           scSpaceID           aSpaceID,
                                           sdpSegHandle      * aSegHandle,
                                           UInt                aCountWanted );


/* DPath-Insert�� TSS/UDS ������ �Ҵ� ������ ���� ������ �Ҵ� */
typedef IDE_RC (*sdpsAllocNewPage4AppendFunc) ( idvSQL               *aStatistics,
                                                sdrMtx               *aMtx,
                                                scSpaceID             aSpaceID,
                                                sdpSegHandle         *aSegHandle,
                                                sdRID                 aPrvAllocExtRID,
                                                scPageID              aFstPIDOfPrvAllocExt,
                                                scPageID              aPrvAllocPageID,
                                                idBool                aIsLogging,
                                                sdpPageType           aPageType,
                                                sdRID                *aAllocExtRID,
                                                scPageID             *aFstPIDOfAllocExt,
                                                scPageID             *aAllocPID,
                                                UChar               **aAllocPagePtr );


/* DPath-Insert�� TSS/UDS�� ���� ������ Ȯ�� */
typedef IDE_RC (*sdpsPrepareNewPage4AppendFunc)( idvSQL               *aStatistics,
                                                 sdrMtx               *aMtx,
                                                 scSpaceID             aSpaceID,
                                                 sdpSegHandle         *aSegHandle,
                                                 sdRID                 aPrvAllocExtRID,
                                                 scPageID              aFstPIDOfPrvAllocExt,
                                                 scPageID              aPrvAllocPageID );

/* ������ Free*/
typedef IDE_RC (*sdpsFreePageFunc)( idvSQL            * aStatistics,
                                    sdrMtx            * aMtx,
                                    scSpaceID           aSpaceID,
                                    sdpSegHandle      * aSegHandle,
                                    UChar             * aPagePtr );

typedef idBool (*sdpsIsFreePageFunc)( UChar * aPagePtr );

/* ������� �Ҵ��� ���� Ž�� */
typedef IDE_RC (*sdpsFindInsertablePageFunc)( idvSQL           * aStatistics,
                                              sdrMtx           * aMtx,
                                              scSpaceID          aSpaceID,
                                              sdpSegHandle     * aSegHandle,
                                              void             * aTableInfo,
                                              sdpPageType        aPageType,
                                              UInt               aRecordSize,
                                              idBool             aNeedKeySlot,
                                              UChar           ** aPagePtr,
                                              UChar            * aCTSlotIdx );

/* Sequential Scan�� Segment ������ ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetFmtPageCntFunc)( idvSQL       *aStatistics,
                                         scSpaceID     aSpaceID,
                                         sdpSegHandle *aSegHandle,
                                         ULong        *aFmtPageCnt );

/* Sequential Scan�� Segment ������ ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetSegInfoFunc)( idvSQL       *aStatistics,
                                      scSpaceID     aSpaceID,
                                      scPageID      aSegPID,
                                      void         *aTableHeader,
                                      sdpSegInfo   *aSegInfo );

/* Sequential Scan�� Segment ������ ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetSegCacheInfoFunc)( idvSQL            *aStatistics,
                                           sdpSegHandle      *aSegHandle,
                                           sdpSegCacheInfo   *aSegCacheInfo );

/* Sequential Scan�� Extent ������ ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetExtInfoFunc)( idvSQL       *aStatistics,
                                      scSpaceID     aSpaceID,
                                      sdRID         aExtRID,
                                      sdpExtInfo   *aExtInfo );

/* ���� Extent�� ������ ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetNxtExtInfoFunc)( idvSQL       *aStatistics,
                                         scSpaceID     aSpaceID,
                                         scPageID      aSegHdrPID,
                                         sdRID         aCurExtRID,
                                         sdRID        *aNxtExtRID );

/* Sequential Scan�� ���� �������� ��ȯ�Ѵ� */
typedef IDE_RC (*sdpsGetNxtAllocPageFunc)( idvSQL           * aStatistics,
                                           scSpaceID          aSpaceID,
                                           sdpSegInfo       * aSegInfo,
                                           sdpSegCacheInfo  * aSegCacheInfo,
                                           sdRID            * aExtRID,
                                           sdpExtInfo       * aExtInfo,
                                           scPageID         * aPageID );

/* Full Scan �Ϸ��� Last Alloc Page�� �����Ѵ�. */
typedef IDE_RC (*sdpsSetLstAllocPageFunc)( idvSQL       *aStatistics,
                                           sdpSegHandle *aSegHandle,
                                           scPageID      aLstAllocPID,
                                           ULong         aLstAllocSeqNo );

/* segment merge�Ϸ�������� HWM �����Ѵ�. */
typedef IDE_RC (*sdpsUpdateHWMInfo4DPath)( idvSQL           *aStatistics,
                                           sdrMtxStartInfo  *aStartInfo,
                                           scSpaceID         aSpaceID,
                                           sdpSegHandle     *aSegHandle,
                                           scPageID          aPrvLstAllocPID,
                                           sdRID             aLstAllocExtRID,
                                           scPageID          aFstPIDOfLstAllocExt,
                                           scPageID          aLstAllocIPID,
                                           ULong             aAllocPageCnt,
                                           idBool            aMegeMultiSeg );

/* segment merge�Ϸ�������� Dpath�� format�� �������� �ٽ� �����Ѵ�. */
typedef IDE_RC (*sdpsReformatPage4DPath)( idvSQL           *aStatistics,
                                          sdrMtxStartInfo  *aStartInfo,
                                          scSpaceID         aSpaceID,
                                          sdpSegHandle     *aSegHandle,
                                          sdRID             aLstAllocExtRID,
                                          scPageID          aLstPID );

/* Segment�� ���¸� �����Ѵ�. */
typedef IDE_RC (*sdpsGetSegStateFunc) ( idvSQL        *aStatistics,
                                        scSpaceID      aSpaceID,
                                        scPageID       aSegPID,
                                        sdpSegState   *aSegState );

/* Segment Cache�� ����� Ž��Hint������ ��ȯ�Ѵ�. */
typedef void (*sdpsGetHintPosInfoFunc) ( idvSQL          * aStatistics,
                                         void            * aSegCache,
                                         sdpHintPosInfo  * aHintPosInfo );

/* Segment �ڷᱸ���� ���� Verify */
typedef IDE_RC (*sdpsVerifyFunc)( idvSQL     * aStatistics,
                                  scSpaceID    aSpaceID,
                                  void       * aSegDesc,
                                  UInt         aFlag,
                                  idBool       aAllUsed,
                                  scPageID     aUsedLimit );

/* Segment �ڷᱸ���� ��� */
typedef IDE_RC (*sdpsDumpFunc) ( scSpaceID    aSpaceID,
                                 void        *aSegDesc,
                                 idBool       aDisplayAll );


typedef IDE_RC (*sdpsMarkSCN4ReCycleFunc)
                                  ( idvSQL        * aStatistics,
                                    scSpaceID       aSpaceID,
                                    scPageID        aSegPID,
                                    sdpSegHandle  * aSegHandle,
                                    sdRID           aFstExtRID,
                                    sdRID           aLstExtRID,
                                    smSCN         * aCommitSCN );

typedef IDE_RC (*sdpsSetSCNAtAllocFunc)
                                  ( idvSQL        * aStatistics,
                                    scSpaceID       aSpaceID,
                                    sdRID           aExtRID,
                                    smSCN         * aTransBSCN );

typedef IDE_RC (*sdpsTryStealExtsFunc) ( idvSQL           * aStatistics,
                                         sdrMtxStartInfo  * aStartInfo,
                                         scSpaceID          aSpaceID,
                                         sdpSegHandle     * aFrSegHandle,
                                         scPageID           aFrSegPID,
                                         scPageID           aFrCurExtDir,
                                         sdpSegHandle     * aToSegHandle,
                                         scPageID           aToSegPID,
                                         scPageID           aToCurExtDir,
                                         idBool           * aTrySuccess );
/* BUG-31055 Can not reuse undo pages immediately after it is used to 
 *           aborted transaction */
typedef IDE_RC (*sdpsShrinkExtsFunc) ( idvSQL            * aStatistics,
                                       scSpaceID           aSpaceID,
                                       scPageID            aSegPID,
                                       sdpSegHandle      * aSegHandle,
                                       sdrMtxStartInfo   * aStartInfo,
                                       sdpFreeExtDirType   aFreeListIdx,
                                       sdRID               aFstExtRID,
                                       sdRID               aLstExtRID);

/*
 * Segment �������� ��Ŀ� ���� ������ �������̽�
 * ������ �����Ѵ�.
 */
typedef struct sdpSegMgmtOp
{
    /* ��� �ʱ�ȭ �� ���� */
    sdpsInitializeFunc                mInitialize;
    sdpsDestroyFunc                   mDestroy;

    /* Segment ���� �� ���� */
    sdpsCreateSegmentFunc             mCreateSegment;
    sdpsDropSegmentFunc               mDropSegment;
    sdpsResetSegmentFunc              mResetSegment;
    sdpsExtendSegmentFunc             mExtendSegment;

    /* ������ �Ҵ� �� ���� */
    sdpsAllocNewPageFunc              mAllocNewPage;
    sdpsPrepareNewPagesFunc           mPrepareNewPages;
    sdpsAllocNewPage4AppendFunc       mAllocNewPage4Append;
    sdpsPrepareNewPage4AppendFunc     mPrepareNewPage4Append;
    sdpsUpdatePageState               mUpdatePageState;
    sdpsFreePageFunc                  mFreePage;

    /* Page State */
    sdpsIsFreePageFunc                mIsFreePage;

    /* DPath�� Commit�� Temp Seg�� �������� Target Seg��
     * Add�Ѵ�. */
    sdpsUpdateHWMInfo4DPath           mUpdateHWMInfo4DPath;
    sdpsReformatPage4DPath            mReformatPage4DPath;

    /* ������� ������ Ž�� */
    sdpsFindInsertablePageFunc        mFindInsertablePage;

    /* Format Page Count */
    sdpsGetFmtPageCntFunc             mGetFmtPageCnt;
    /* Sequential Scan */
    sdpsGetSegInfoFunc                mGetSegInfo;
    sdpsGetExtInfoFunc                mGetExtInfo;
    sdpsGetNxtExtInfoFunc             mGetNxtExtInfo;
    sdpsGetNxtAllocPageFunc           mGetNxtAllocPage;
    sdpsGetSegCacheInfoFunc           mGetSegCacheInfo;
    sdpsSetLstAllocPageFunc           mSetLstAllocPage;

    /* Segment�� ���� */
    sdpsGetSegStateFunc               mGetSegState;
    sdpsGetHintPosInfoFunc            mGetHintPosInfo;

    /* Segment Meta PID���� */
    sdpsSetMetaPIDFunc                mSetMetaPID;
    sdpsGetMetaPIDFunc                mGetMetaPID;

    sdpsMarkSCN4ReCycleFunc           mMarkSCN4ReCycle;
    sdpsSetSCNAtAllocFunc             mSetSCNAtAlloc;
    sdpsTryStealExtsFunc              mTryStealExts;
    sdpsShrinkExtsFunc                mShrinkExts;

    /* Segment �ڷᱸ���� Verify �� Dump */
    sdpsDumpFunc                      mDump;
    sdpsVerifyFunc                    mVerify;

} sdpSegMgmtOp;

/* PROJ-1671 Segment�� �������� ���
 * Segment ���������� �ʿ��� ������ �����Ѵ�. */
typedef struct sdpSegmentDesc
{
    /* PROJ-1671 Bitmap-based Tablespace And Segment Space Management */
    sdpSegHandle     mSegHandle;
    /* Segment Space Management Type */
    smiSegMgmtType   mSegMgmtType;
    /* Segment ������ �������̽� ��� */
    sdpSegMgmtOp   * mSegMgmtOp;

} sdpSegmentDesc;

/* ------------------------------------------------
 * disk table�� ���� page list entry
 * - memory table�� page list entry�ʹ� ������
 *   ���ǵǾ� ����.
 * ----------------------------------------------*/
typedef struct sdpPageListEntry
{
    /* mItemSize�� align�� ���� fixed ������ ũ�Ⱑ page��
     * ����Ÿ �������ũ�⺸�� ū ���̺��� ������ �� ���� */
    UInt                  mSlotSize;

    /* Segment ����� */
    sdpSegmentDesc        mSegDesc;

    /* insert�� record�� ���� */
    ULong                 mRecCnt;

    /* �ټ��� transaction�� ���ÿ� page list entry��
     * ������ �� �ִ� ���ü� ���� */
    iduMutex              *mMutex;

    /* PROJ-1497 DB Migration�� �����ϱ� ���� Reserved ������ �߰� */
    ULong                  mReserveArea[SDP_PAGELIST_ENTRY_RESEV_SIZE];
} sdpPageListEntry;

#define SDP_SEG_SPECIFIC_DATA_SIZE  (64)

typedef struct sdpSegInfo
{
    /* Common */
    scPageID          mSegHdrPID;
    sdpSegType        mType;
    sdpSegState       mState;

    /* Extent�� ������ ���� */
    UInt              mPageCntInExt;

    /* Format�� ������ ���� */
    ULong             mFmtPageCnt;

    /* Extent���� */
    ULong             mExtCnt;

    /* Extent Dir Page ���� */
    ULong              mExtDirCnt;

    /* ù��° Extent RID */
    sdRID             mFstExtRID;

    /* ������ Extent RID */
    sdRID             mLstExtRID;

    /* High Water Mark */
    scPageID          mHWMPID;

    /* for FMS */
    sdRID             mLstAllocExtRID;
    scPageID          mFstPIDOfLstAllocExt;

    /* HWM�� Extent RID */
    sdRID             mExtRIDHWM;

    /* For TempSegInfo */
    ULong             mSpecData4Type[ SDP_SEG_SPECIFIC_DATA_SIZE / ID_SIZEOF( ULong ) ];
} sdpSegInfo;

typedef struct sdpSegCacheInfo
{
    /* �ʿ��� ������ �߰��ϸ� �ȴ�.
     * ����� TMS���� ������ �Ҵ�� �������� �������� ���ؼ��� ����Ѵ�. */

    /* for TMS */
    idBool        mUseLstAllocPageHint; // ������ �Ҵ�� Page Hint ��� ����
    scPageID      mLstAllocPID;         // ������ �Ҵ�� Page ID
    ULong         mLstAllocSeqNo;       // ������ �Ҵ�� Page�� SeqNo
} sdpSegCacheInfo;

typedef struct sdpExtInfo
{
    /* Extent�� ù��° Extent PID */
    scPageID     mFstPID;

    /* Extent���� ù��° Data PID */
    scPageID     mFstDataPID;

    /* Extent�� �����ϴ� LF BMP PID */
    scPageID     mExtMgmtLfBMP;

    /* Extent�� ������ PID */
    scPageID     mLstPID;

} sdpExtInfo;

typedef struct sdpTBSInfo
{
    /* High High Water Mark */
    scPageID           mHWM;
    /* Low  High Water Mark */
    scPageID           mLHWM;

    /* Total Ext Count */
    ULong              mTotExtCnt;

    /* Format Ext Count */
    ULong              mFmtExtCnt;

    /* Page Count In Ext */
    UInt               mPageCntInExt;

    /* Ȯ��� �����Ǵ� Extent�� ���� */
    UInt               mExtCntOfExpand;

    /* Free Extent RID List */
    sdRID              mFstFreeExtRID;
} sdpTBSInfo;

typedef struct sdpDumpTBSInfo
{
    /* TBS�� ���� Extent RID */
    sdRID              mExtRID;

    /* Extent RID���� PageID, mOffset */
    scPageID           mPID;
    scOffset           mOffset;

    /* Extent�� ù��° PID */
    scPageID           mFstPID;
    /* Extent�� ù��° Data PID */
    scPageID           mFstDataPID;
    /* Extent�� ������ ���� */
    UInt               mPageCntInExt;
} sdpDumpTBSInfo;


/*
 * Extent Desc ����
 */
typedef struct sdpExtDesc
{
    scPageID   mExtFstPID;  // Extent�� ù��° PageID
    UInt       mLength;     // Extent�� ������ ����
} sdpExtDesc;

# define SDP_1BYTE_ALIGN_SIZE   (1)
# define SDP_8BYTE_ALIGN_SIZE   (8)

/**********************************************************************
 * SelfAging Check Flag
 * ����Ÿ �������� ���ؼ� SelfAging�� �����ϱ� ������ �ռ� Ȯ�δܰ谡 �ִµ�,
 * Ȯ�ΰ������� ��ȯ�ϴ� flag ���̴�.
 *
 * (1) CANNOT_AGING
 *     AGING ��� Row Piece�� ����������, ���� ���� �ִ� Ʈ������� Active�ϱ�
 *     ������ Aging�� �� �� ���� ����̴�. AGING������ SCN�� ��ϵǾ� �ֱ� ������
 *     �ٷ� �Ǵ��� �� �ִ�.
 * (2) NOTHING_TO_AGING
 *     AGING ��� Row Piece�� �������� �ʴ´�.
 * (3) CHECK_AND_AGING
 *     AGING ����� ����������, AGING ������ SCN�� FullScan �غ��� ���ؾ��ϹǷ�
 *     �ٷ� �Ǵ��� �� ����. ���� �ѹ��� FullScan �Ѵ�.
 * (4) AGING_DEAD_ROWS
 *     Row Piece�� ���� �ִ� �ٸ� Ʈ������� ���� ������ AGING �� �� �ִ�
 *     Row Piece�� �����Ѵ�. �ٷ� �Ǵ��Ѵ�.
 **********************************************************************/
typedef enum sdpSelfAgingFlag
{
    SDP_SA_FLAG_CANNOT_AGING     = 0,
    SDP_SA_FLAG_NOTHING_TO_AGING,
    SDP_SA_FLAG_CHECK_AND_AGING,
    SDP_SA_FLAG_AGING_DEAD_ROWS
} sdpSelfAgingFlag;


/**********************************************************************
 *
 * PROJ-1705 Disk MVCC ������
 *
 * ��ũ ���̺��� ����Ÿ �������� Touched Transaction Layer ��� ����
 *
 **********************************************************************/
typedef struct sdpCTL
{
    smSCN     mSCN4Aging;    // Self-Aging�� �� �ִ� ���� SCN
    UShort    mCandAgingCnt; // Self-Aging�� �� �ִ� Dead Deleted Row
    UShort    mDelRowCnt;    // �� Deleted Or Deleting �� RowPiece ����
    UShort    mTotCTSCnt;    // �� CTS ����
    UShort    mBindCTSCnt;   // �� Bind�� CTS ����
    UShort    mMaxCTSCnt;    // �ִ� CTS ����
    UShort    mRowBindCTSCnt;
    UChar     mAlign[4];     // Dummy Align
} sdpCTL;


# define SDP_CACHE_SLOT_CNT    (2)


/**********************************************************************
 * ����Ÿ �������� Touched Transaction Slot ����
 *
 * ������ Ʈ������� ������ Row���� �Ҵ���� CTS�� ���ؼ� ������踦 �ΰԵǸ�,
 * Commit ���� �ٸ� Ʈ������� ������ Row Time-Stamping�� ���ؼ� ��������
 * �����ȴ�.
 *
 *   __________________________________________________________________
 *   |TSSlotSID.mPageID |TSSlotSID.mSlotNum| FSCredit | Stat |Align 1B |
 *   |__________________|__________________|__________|______|_________|
 *      | RefCount |  RefRowSlotNum[0] | RefRowSlotNum[1] |
 *      |__________|__________________|_______________|
 *      |Trans Begin SCN or CommitSCN |
 *      |_____________________________|
 *
 *      Touched Transaction Slot�� Size�� 24����Ʈ�̸�,
 *      8Byte�� align�Ǿ� �ִ�.
 *
 * (1) TSSlotSID
 *     ���� Ʈ����Ǹ��� �Ҵ�Ǵ� TSS�� SID�̴�. TSS �������� ����� �� ������,
 *     �������Ŀ��� Ʈ����ǵ��� TSSlotSID�� ���� TSS�� �湮�� �� �ִ�.
 *
 *     ���� Fast CTS Stamping�� ���������� ���� Ʈ����ǿ� ���ؼ� Delayed CTS
 *     Stamping Ȥ�� Hard Row Time-Stamping�ÿ� TSSlotSID�� ���� ��湮�ϰ�
 *     �Ǵµ� �̶� ����Ǿ��ٰ� �ǴܵǸ� ��Ȯ�� CommitSCN�� ���� ���� ������,
 *     ��ſ� INITSCN(0x0000000000000000)�� �����ϹǷ� �ؼ� ���� Ʈ�������
 *     ��� �����ϰų� ���� �ְ� �Ѵ�. �̰��� ������ ������ TSS �������� ����
 *     �Ǵ� ������ �Ǵ� ������ �ִ�. ��, �� TSS�� ������ ��� Old Row Version��
 *     �ǵ��� Ʈ������� �������� �ʴ� ��� TSS�� ������ �� �ֱ� �����̴�.
 *
 * (2) FSC ( Free Space Credit )
 *     Ʈ������� Row�� Update�ϰų� Delete����� Rollback �� �� �ִ� ���������
 *     Commit�Ҷ����� �ٸ� Ʈ������� �Ҵ����� ���ϰ� �ؾ��Ѵ�. ��ó�� Commit ����
 *     �� �����Ǵ� ������ ũ�⸦ FSC��� �ϰ� �̰��� CTS�� ����صд�.
 *
 * (3) FLAG
 *     CTS�� �Ҵ���� Ʈ������� Active/Rollback/Time-Stamping ���θ�
 *     �Ǵ��� �� �ִ� ���°��̴�.
 *
 *     ������ ���� 4���� ���°��� ������.
 *
 *     - 'N' (0x01)
 *     - 'A' (0x02)
 *       CTS�� �Ҵ��� Ʈ������� ���� Active�� ��� Ȥ�� �̹� Commit �Ǿ�����,
 *       Time-Stamping�� �ʿ��� ����̴�.
 *     - 'T' (0x04)
 *       �Ҵ��� Ʈ������� Commit�����̳� SCAN ����� CTS Time-Stamping�� CTS
 *       �̸�, ��Ȯ��  CommitSCN�� �����ȴ�. ���� Row Time-Stamping�� �ʿ��ϴ�.
 *     - 'R' (0x08)
 *       Row Time-Stamping�� �Ϸ�Ǿ����� �ǹ��Ѵ�. CommitSCN�� ��Ȯ�� SCN ��
 *       �̰ų� INITSCN(0x0000000000000000)�� �����ȴ�.
 *     - 'O' (0x10)
 *       ���� Row�� ��� Rollback�� �����̴�.
 *
 * (4) RefCount
 *     CTS�� ���õ� Row�� �����̴�. �������� �ߺ� ���ŵ� Row�� ���ؼ��� 1�� ����
 *     �Ѵ�.
 *
 * (5) RefRowSlotNum
 *     CTS�� ���õ� Row Piece Header�� Cache ������ �ִ� 2������ Cache�Ѵ�.
 *     �ֳ��ϸ�, �������� ������ 2�����Ϸ� ���ŵ� ��쿡�� Slot Dir. �� FullScan����
 *     �ʵ��� �ϱ� ���� ���̴�.
 *
 * (6) Ʈ����� FstDskViewSCN Ȥ�� Commit SCN
 *     ���� Ʈ������� FstDskViewSCN�� ������ ����, Commit�ÿ��� CTS
 *     Time-Stamping�� CommitSCN���� �����ϰ� �ȴ�.
 *     FstDskViewSCN�� ���� CTS Page�� ����� ��츦 üũ�ϱ� ���ؼ��̴�.
 *
 **********************************************************************/
typedef struct sdpCTS
{
    scPageID    mTSSPageID;          // TSS �� PID
    scSlotNum   mTSSlotNum;          // TSS �� SlotNum
    UShort      mFSCredit;           // �������� ��ȯ���� ���� �������ũ��
    UChar       mStat;               // CTS�� ����
    UChar       mAlign;              // Align Dummy ����Ʈ
    UShort      mRefCnt;             // Cache�� Row Piece�� ����
    scSlotNum   mRefRowSlotNum[ SDP_CACHE_SLOT_CNT ];
                                     // Cache�� Row Piece�� Offset
    smSCN       mFSCNOrCSCN;         // CTS�� �Ҵ��� Ʈ����� BSCN Ȥ�� CSCN
} sdpCTS;

# define SDP_CTS_STAT_NUL    (0x01)  // 'N' CTS�� �ѹ��� ���ε����� ���� ����
# define SDP_CTS_STAT_ACT    (0x02)  // 'A' CTS�� Ʈ����ǿ� ���ε��� ����
# define SDP_CTS_STAT_CTS    (0x08)  // 'T' CTS TimeStamping�� �� ����
# define SDP_CTS_STAT_RTS    (0x10)  // 'R' Row TimeStamping�� �� ����
# define SDP_CTS_STAT_ROL    (0x20)  // 'O' Rollback�� ����

# define SDP_CTS_SS_FREE     ( SDP_CTS_STAT_NUL | \
                               SDP_CTS_STAT_RTS | \
                               SDP_CTS_STAT_ROL )

/**********************************************************************
 * Table Change Transaction Slot Idx ����
 * Page Layer���� ���� ������ ������.
 **********************************************************************/

# define SDP_CTS_MAX_IDX     (0x78)  // 0 ~ 120 01111000
# define SDP_CTS_MAX_CNT     (SDP_CTS_MAX_IDX + 1)

/* CTS�� �Ҵ���� ���ϰ� BoundRow�� ��� */
# define SDP_CTS_IDX_NULL    (0x7C)  // 124     01111100

/* Stamping ���Ŀ� �ƹ��͵� ����Ű�� ���� ��� */
# define SDP_CTS_IDX_UNLK    (0x7E)  // 126     01111110
# define SDP_CTS_IDX_MASK    (0x7F)  // 127     01111111
# define SDP_CTS_LOCK_BIT    (0x80)  // 128     10000000

// PROJ-2068 Direct-Path INSERT ���� ����
typedef struct sdpDPathSegInfo
{
    // Linked List�� Next Pointer
    smuList     mNode;

    // Undo�� DPathSegInfo�� �ĺ��ϱ� ���� ��ȣ
    UInt        mSeqNo;

    // Insert ��� Table�� SpaceID
    scSpaceID   mSpaceID;
    // insert ��� table �Ǵ� partition�� TableOID
    smOID       mTableOID;

    // BUG-29032 - DPath Insert merge �� TMS���� assert
    // ó�� �Ҵ��ߴ� PageID
    scPageID    mFstAllocPID;

    // ������ �Ҵ��ߴ� ExtRID, PageID
    scPageID    mLstAllocPID;
    sdRID       mLstAllocExtRID;
    scPageID    mFstPIDOfLstAllocExt;

    // ������ page�� ����Ű�� ������
    UChar     * mLstAllocPagePtr;
    // Table, External, Multiple page���� ���� Page ����
    UInt        mTotalPageCount;

    // Record Count
    UInt        mRecCount;

    // PROJ-1671 Bitmap-based Tablespace And Segment Space Management
    // Segment ���������� �ʿ��� ���� �� ���� ����
    // Segment Handle : Segment RID �� Semgnet Cache
    sdpSegmentDesc    * mSegDesc;

    // ���������� Insert�� SegInfo �϶� TRUE
    idBool          mIsLastSeg;
} sdpDPathSegInfo;

typedef struct sdpDPathInfo
{
    // List Of Segment Info for DPath Insert
    smuList mSegInfoList;

    //  sdpDPathSegInfo�� mSeqNo�� �Ҵ��ϱ� ���� ���� SeqNo
    //
    //  SeqNo�� Direct-Path INSERT�� rollback�� ���Ǵ� ���̴�.
    // Direct-Path INSERT�� undo �ÿ� sdpDPathSegInfo�� �޸� mSeqNo�� ����
    // NTA �α׿� ��ϵ� SeqNo�� ������ sdpDPathSegInfo�� �ı��ϴ� �������
    // undo�� �����Ѵ�.
    UInt    mNxtSeqNo;

    // X$DIRECT_PATH_INSERT ��踦 ���� ��
    ULong   mInsRowCnt;
} sdpDPathInfo;


#endif // _O_SDP_DEF_H_
