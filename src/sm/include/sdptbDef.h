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
 * $Id: sdptbDef.h 27220 2008-07-23 14:56:22Z newdaily $
 *
 * Bitmap-based tablespace�� ���� �ڷᱸ���� ��ũ�θ� �����Ѵ�.
 ***********************************************************************/

#ifndef _O_SDPTB_DEF_H_
#define _O_SDPTB_DEF_H_ 1

#include <iduLatch.h>
#include <sdpModule.h>
#include <sdpDef.h>
#include <sdpPhyPage.h>
#include <sdpSglPIDList.h>
#include <smDef.h>

#define SDPTB_BITS_PER_BYTE    (8)     //�ѹ���Ʈ�� �ִ� ��Ʈ��
#define SDPTB_BITS_PER_UINT    (SDPTB_BITS_PER_BYTE * 4) //UInt �� �ִ� ��Ʈ��  // BUG-47666
#define SDPTB_BITS_PER_ULONG   (SDPTB_BITS_PER_BYTE * 8) //ULong�� �ִ� ��Ʈ��


#define SDPTB_BIT_OFF          (0)
#define SDPTB_BIT_ON           (1)

/*
 * ������ ���뿩�θ� bit�� �����Ҷ� �Ҵ��ؾ� �ϴ� �迭�� ũ������
 * TBS���� �ִ� 1024���� ������ �������ִ�.
 * �Ϲ������� SDPTB_GLOBAL_GROUP_ARRAY_SIZE�� 16 �̴�.
 */
#define SDPTB_GLOBAL_GROUP_ARRAY_SIZE  \
            ( SD_MAX_FID_COUNT/(ID_SIZEOF(ULong)* SDPTB_BITS_PER_BYTE) )

//GG hdr�� ���� ������ ����
#define SDPTB_GG_HDR_PAGE_CNT  (1)

/*  LG hdr�� ���� ������ ����
 * alloc LG header,dealloc LG header�� ���� ���� 1���� �������� �ʿ��ϹǷ�
 * �� 2���� �������� �ʿ��ϴ�.
 */
#define SDPTB_LG_HDR_PAGE_CNT  (2)

//�ϳ��� Extent�� ũ�⸦ ����Ʈ�� ���
#define SDPTB_EXTENT_SIZE_IN_BYTES( pages_per_extent )  \
                                   ((pages_per_extent)*SD_PAGE_SIZE)


#define SDPTB_LGID_MAX          (SDPTB_BITS_PER_ULONG*2)

//LG���� ��Ʈ������� ����ϴ� �����ѹ�
#define SDPTB_LG_BITMAP_MAGIC       (ID_ULONG(0xaf))

//�ϳ���  LG�� �ִ� ������ ����( �̰��� ������� ������ũ���̴�)
#define SDPTB_PAGES_PER_LG(pages_per_extent)  \
    ( ( sdptbGroup::nBitsPerLG() * pages_per_extent ) + SDPTB_LG_HDR_PAGE_CNT )

//FID�� GG hdr�� PID�� ����.
#define SDPTB_GLOBAL_GROUP_HEADER_PID(file_id)  SD_CREATE_PID(file_id, 0)

/*
 * LG hdr�� PID�� ����.
 * which�� 0�̸� �ϳ��� LG group�� �ִ� LG hdr�� �������� PID
 * which�� 1�̸� �ϳ��� LG group�� �ִ� LG hdr�� ���ǰ��� PID
 */
#define SDPTB_LG_HDR_PID_FROM_LGID( file_id , lg_id, which, pages_per_extent )        \
    ( SD_CREATE_PID( file_id, SDPTB_GG_HDR_PAGE_CNT +                                 \
                      lg_id * SDPTB_PAGES_PER_LG( pages_per_extent ) + which ) )

//LGID�κ��� �ش� LG���� extent�� �����ϴ°��� FPID�� ����.
#define SDPTB_EXTENT_START_FPID_FROM_LGID( lg_id, pages_per_extent )  \
   (( SDPTB_GG_HDR_PAGE_CNT + lg_id * SDPTB_PAGES_PER_LG(pages_per_extent) )  \
     +  SDPTB_LG_HDR_PAGE_CNT)

/* extent�� ù��° PID �� �޾Ƽ� LGID�� �����ϴ� ��ũ�� �Լ��̴�.*/
#define SDPTB_GET_LGID_BY_PID( aExtPID, aPagesPerExt)           \
                ((SD_MAKE_FPID( aExtPID ) - SDPTB_GG_HDR_PAGE_CNT )      \
                    /  SDPTB_PAGES_PER_LG( aPagesPerExt ))

// PID�κ��� �ش� LG���� extent�� �����ϴ°��� PID�� ����.
#define SDPTB_EXTENT_START_PID_FROM_PID( aPID, aPagesPerExt )            \
    ( SD_CREATE_PID( SD_MAKE_FID( aPID ) ,                                \
                     SDPTB_EXTENT_START_FPID_FROM_LGID (                  \
                           SDPTB_GET_LGID_BY_PID( aPID, aPagesPerExt ),   \
                           aPagesPerExt ) ) )

// PID�� �޾Ƽ� page�� ���� extent�� index�� �����ϴ� ��ũ�� �Լ��̴�.
#define SDPTB_EXTENT_IDX_AT_LG_BY_PID( aPID, aPagesPerExt )              \
    ( ( aPID - SDPTB_EXTENT_START_PID_FROM_PID( aPID, aPagesPerExt ) )  \
      / aPagesPerExt )

// Page�� ���� extent PID�� �����ϴ� ��ũ�� �Լ��̴�.
#define SDPTB_GET_EXTENT_PID_BY_PID( aPID, aPagesPerExt )                  \
    ( ( SDPTB_EXTENT_IDX_AT_LG_BY_PID( aPID, aPagesPerExt ) * aPagesPerExt ) \
      + SDPTB_EXTENT_START_PID_FROM_PID( aPID, aPagesPerExt ) )


typedef  UInt sdptbGGID; // Global Group ID
typedef  UInt sdptbLGID; // Local Group ID

// FEBT ���̺����̽��� Space Cache Runtime �ڷᱸ��
typedef struct sdptbSpaceCache
{
    sdpSpaceCacheCommon mCommon;      // Space Cache Header

    /* Free�� Extent Pool�� ������ �ִ�. */
    iduStackMgr         mFreeExtPool;

    /* Extent�� �Ҵ� Ž�������� ���� GGID */
    volatile sdptbGGID  mGGIDHint;

    /* MaxGGID�� ���� �ش�TBS�� ������ GG���� �ִ� ID���� �ǹ��Ѵ�.
     * free�� �ֵ���� �װ��� �������� �ƴϴ�.
     * ��, GG�� free extent�� ���� GGID�� ������ Max GGID�� �ɼ� �ִ�.
     */
    volatile sdptbGGID  mMaxGGID;

    /*
     * �Ҵ簡���� Extent�� ���翩�θ� bit�� ǥ���ϸ�,
     * DROP�� GlobalGroup�� ��� ��Ʈ�� 0���� �����ȴ�.
     */
    ULong        mFreenessOfGGs[ SDPTB_GLOBAL_GROUP_ARRAY_SIZE ];

    /* TBS���� ����Ȯ��(Autoextend) �� ���� Mutex */
    iduMutex     mMutexForExtend;
    /* BUG-31608 [sm-disk-page] add datafile during DML
     * AddDataFile�� �ϱ� ���� Mutex */
    iduMutex     mMutexForAddDataFile;
    idBool       mOnExtend;        // ���� Ȯ���� ����ǰ� �ִ°�?
    iduCond      mCondVar;
    UInt         mWaitThr4Extend;  // water���� ��

    // �Ҵ簡���� ExtDir ����
    idBool     mArrIsFreeExtDir[ SDP_MAX_FREE_EXTDIR_LIST ];
} sdptbSpaceCache;

/*
 * Extent Slot ����
 *
 * Extent�� TBS�� �����Ҷ� ���Ǵ� �ӽ� ����ü�̴�.
 */
typedef struct sdptbSortExtSlot
{
    scPageID    mExtFstPID;
    UInt        mLength;
    UShort      mLocalGroupID;
} sdptbSortExtSlot;

/*
 * ExtDesc ���� ����
 */
typedef struct sdptbExtSlot
{
    scPageID   mExtFstPID;
    UShort     mLength;
} sdptbExtSlot;

/* BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� ��
 * �ϴ�. 
 * 
 *  Temp Segement�� Extent���� Mtx Commit ���Ŀ� ��ȯ�ϵ��� �ϱ� ����
 * Pending Job���� �Ŵ�ϴ�. Pending Job���� �Ŵޱ� ���� ����ü��
 * sdptbFreeExtID�Դϴ�.
 */
typedef struct sdptbFreeExtID
{
    scSpaceID  mSpaceID;
    scPageID   mExtFstPID;
} sdptbFreeExtID;

/* Extent ���꿡 ���� �����ϴ� Local Group Header�� �ٸ���.
 * Extent �Ҵ� ���� : Alloc LG Header ����
 * Extent ���� ���� : Dealloc LG Header ���� */
#define SDPTB_LG_PINGPONG_COUNT    (2)

/*  LG������ freeness�迭�� ũ��
 *  �Ѱ��� GG�� LG�� 128�� �̻��� ������ �����Ƿ� 2�� ����ϴ�.  */
#define SDPTB_LG_FN_ARRAY_SIZE     (2)

// LG������ freeness�迭�� ũ�⸦ ��Ʈ�� ��Ÿ����
#define SDPTB_LG_FN_SIZE_IN_BITS               \
        (SDPTB_LG_FN_ARRAY_SIZE*ID_SIZEOF(ULong)*SDPTB_BITS_PER_BYTE ) //128

#define SDPTB_LG_CNT_MAX  SDPTB_LG_FN_SIZE_IN_BITS

//FID�κ��� GGPID���� ��ũ��
#define SDPTB_GET_GGHDR_PID_BY_FID( fid )        SD_CREATE_PID( fid, 0 )

#define SDPTB_FIRST_FID                         (0)

//GG hdr�� LG type�ʵ忡 ���� ��
typedef enum sdptbAllocLGIdx
{
    SDPTB_ALLOC_LG_IDX_0 = 0,   // ���� LG�� alloc LG group
                                // (ó�� LG�� �����������type)
    SDPTB_ALLOC_LG_IDX_1,       // ���� LG�� alloc LG group
    SDPTB_ALLOC_LG_IDX_CNT
}sdptbAllocLGIdx;

//alloc LG���� dealloc LG������ �����ϴ� �뵵�� ���
typedef enum sdptbLGType {
    SDPTB_ALLOC_LG,
    SDPTB_DEALLOC_LG,
    SDPTB_LGTYPE_CNT
} sdptbLGType;

/*
 * extent���� ù��° PID���� extent�� page������ ����
 * extent�� ������ PID���� ��´�.
 */
#define SDPTB_LAST_PID_OF_EXTENT(ext_first_pid ,pages_per_extent)   \
                            ( ext_first_pid + pages_per_extent -1)

/*
 * interface�� ���Ǵ� RID type
 */
typedef enum sdptbRIDType
{
    SDPTB_RID_TYPE_TSS,
    SDPTB_RID_TYPE_UDS,
    SDPTB_RID_TYPE_MAX
} sdptbRIDType;

/* Local Group�� Extent ���� ���� ������ �����ϴ� �ڷᱸ��. */
typedef struct sdptbLGFreeInfo
{
    ULong    mFreeExts;   //��� Local Group�� ���� �Ҵ� ������ �� Extent ����
    ULong    mBits[SDPTB_LG_FN_ARRAY_SIZE];
} sdptbLGFreeInfo;

#define SDPTB_RID_ARRAY_SIZE_FOR_UNDOTBS        (SDPTB_RID_TYPE_MAX)

typedef struct sdptbGGHdr
{
    /* A. Global Group ���� (��ü Group�� �ͽ���Ʈ �Ҵ� �� ������ ����)
           Local Group ������ ������� �ʴ´�. */
    sdptbGGID           mGGID;        // global group id
    UInt                mPagesPerExt; // extent�� page uniform ����

    /*
     * mHWM
     * GG�� ��� extent�߿��� �ѹ��̻� �Ҵ������ �ִ� extent���߿���,
     * extent�� ù��° PID�� ����ū extent���� ������ �������� PID��.
     * �ٽø��ϸ�,GG�� ��� page�߿��� �ѹ��̻� �Ҵ������ �ִ�
     * page���߿���, ����ū PID��
     * �̰��� �������ϸ� �������� ��.��.��.
     */
    scPageID            mHWM;
    UInt                mLGCnt;//��local group ����,�ڵ�Ȯ��� ������ �� �ִ�
    UInt                mTotalPages; //���� ����Ÿ������ ������ ����

    /*
     * alloc LG index
     *
     *    SDPTB_ALLOC_LG_IDX_0  -->   alloc LG page�� ���ǰ�
     *    SDPTB_ALLOC_LG_IDX_1  -->   alloc LG page�� ���ǰ�
     */
    UInt                mAllocLGIdx;
    sdptbLGFreeInfo     mLGFreeness[ SDPTB_LG_PINGPONG_COUNT ];

    // UndoTBS�� TSS/Undo ExtDir PID ����Ʈ
    sdpSglPIDListBase   mArrFreeExtDirList[ SDP_MAX_FREE_EXTDIR_LIST ];

    //Undo ���̺����̽��� TSS ���׸�Ʈ�� Meta PID
    scPageID            mArrTSSegPID[ SDP_MAX_TSSEG_PID_CNT ];
    scPageID            mArrUDSegPID[ SDP_MAX_UDSEG_PID_CNT ];

} sdptbGGHdr;

// Local Group�ͽ���Ʈ �Ҵ� �� ������ �����Ѵ�.
typedef struct sdptbLGHdr
{
     sdptbLGID  mLGID;       // local group id
     UInt       mStartPID;   // ù��° Extent ���� PID (fid, fpid )
     UInt       mHint;       // ���� �Ҵ��� Extent�� ��Ʈ index
     UInt       mValidBits;  // group������ �ʱ�ȭ�� ��Ʈ����
                             // (��Ʈ �˻��Ҷ� ����� �Ǵ� ��Ʈ����)
     UInt       mFree;       // �Ҵ簡���� free extent ����
     ULong      mBitmap[1];  // page ũ�⿡ ���� ������ ������ŭ ����Ѵ�.
} sdptbLGHdr;

/*
 *LG header�� �ʱ�ȭ�ϴ� redo routine ���� ����������̴�.
 */
typedef struct sdptbData4InitLGHdr
{
    UChar    mBitVal;   //������ ��Ʈ��
    UInt     mStartIdx; //mBitmap���� ������ ������ index
    UInt     mCount;    //sBitVal�� �ʱ�ȭ�� ��Ʈ�� ����
                        //������ ��Ʈ�� !sBitVal�� ��Ʈ�ؾ���.
} sdptbData4InitLGHdr;

//FID, LGID���� �߰����� ���������� �˻��ϴ� magic number�Ѵ�.
#define SDPTB_NOT_FOUND        (0xFFFF)

//LG���� ��Ʈ�˻��� �߰߸�������쿡���� magic number
#define SDPTB_BIT_NOT_FOUND   (0xFFFFFFFF)


#endif // _O_SDPTB_DEF_H_
