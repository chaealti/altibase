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
 * $Id: sdpscDef.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment ����� ���� �ڷᱸ���� ������
 * ��������̴�.
 *
 * Undo Tablespace�� TSS Segment�� Undo Segment���� �����ϰ� �����Ѵ�.
 * TSS Segment�� Undo Segment�� CMS�� �����Ǹ�, Database ����, Ʈ����� ����,
 * Query�� ���� Read Consistency�� �����Ѵ�.
 *
 * CMS (Circular-List Managed Segment)�� FMS�� TMS�ʹ� �ٸ� ������ �Ҵ�
 * �˰���, Extent �Ҵ� �� ���� �˰���, Extent Dir. Shrink �˰������
 * �����ϰ� �ִ�.
 *
 * CMS�� ���� �����Ǵ� Undo Segment�� TSS Segment�� ������ ���� Ư¡�� ������.
 *
 * (1) UDSEG�� TSSEG�� ��Ƽ���̽� ������ ���ؼ� �ڵ� �����Ǹ�,��� ����� �̸���
 *     ����Ѵ�.
 * (2) Expired�� Extent Dir.�� ����ǰų� Shrink �� �� �ִ�.
 * (3) ��߿� Segment�� �ڵ����� Shrink �ȴ�.
 * (4) Segment�� HWM ������ �������� �ʴ´�.
 * (5) UDSEG�� TSSEG�� ���� Extent Dir.�� Extent ������ ������ �� �ִ�
 *     ����� ������Ƽ�� �����Ѵ�
 * (6) Steal ��å���� ���׸�Ʈ���� Expire�� ExtDir�� ������ �� �ִ�.
 *
 ***********************************************************************/

# ifndef _O_SDPSC_DEF_H_
# define _O_SDPSC_DEF_H_ 1

# include <iduMutex.h>

# include <sdpDef.h>

/***********************************************************************
 * Segment Cache �ڷᱸ�� (Runtime ����)
 *
 ***********************************************************************/
typedef struct sdpscSegCache
{
    sdpSegCCache     mCommon;            // ���׸�Ʈ ĳ�� ���� ���
} sdpscSegCache;

/***********************************************************************
 * ExtDir���� �ڷᱸ��
 ***********************************************************************/
typedef struct sdpscExtDirInfo
{
    // ExtDir�� PID
    scPageID          mExtDirPID;

    /* Cur. ExtDir�� Full �� NewExtDir �ʿ� */
    idBool            mIsFull;

    /* ExtDir���� ���ο� Extent�� ���� */
    SShort            mTotExtCnt;

    /* ExtDir�� ��ϵ� �� �ִ� �ִ� ExtDesc ���� */
    UShort            mMaxExtCnt;

    // ExtDir�� Nxt. ExtDir PID
    scPageID          mNxtExtDirPID;

} sdpscExtDirInfo;

/***********************************************************************
 *  Extent Descriptor ����
 ***********************************************************************/
typedef struct sdpscExtDesc
{
    scPageID       mExtFstPID;     // Extent ù��° �������� PID
    UInt           mLength;        // Extent�� ������ ����
    scPageID       mExtFstDataPID; // Extent�� ù��° Data �������� PID
} sdpscExtDesc;


/***********************************************************************
 * Extent �Ҵ� ����
 ***********************************************************************/
typedef struct sdpscAllocExtDirInfo
{
    /* Ȯ������ ���ο� Cur. ExtDir �������� PID */
    scPageID          mNewExtDirPID;

    /* Ȯ������ ���ο� Nxt. ExtDir �������� PID */
    scPageID          mNxtPIDOfNewExtDir;

    /* Shrink�ؾ��� ExtDir */
    scPageID          mShrinkExtDirPID;

    /* TBS�κ��� �Ҵ���� ExtDir�� Cur.ExtDir�� Nxt.�� �����ؾ��ϴ��� ���� */
    idBool            mIsAllocNewExtDir;

    sdpscExtDesc      mFstExtDesc;       /* �Ҵ�� ExtDesc ���� */

    sdRID             mFstExtDescRID;    /* �Ҵ�� ExtDesc�� RID */

    UInt              mTotExtCntOfSeg;   /* ���׸�Ʈ�� �� ExtDesc ���� */

    /* Shrink�ؾ��� ExtDir �� ���ѵ� Extent ���� */
    UShort            mExtCntInShrinkExtDir;

} sdpscAllocExtDirInfo;


/***********************************************************************
 * Segment Control Header
 *
 * Segment Meta Header �������� ��ġ�Ѵ�.
 ***********************************************************************/
typedef struct sdpscSegCntlHdr
{
    sdpSegType  mSegType;       // Segment Ÿ��
    sdpSegState mSegState;      // Segment State
} sdpscSegCntlHdr;


/***********************************************************************
 * Extent Control Header ����
 *
 * Segment Meta Header �������� ��ġ�ϸ�, Segment�� Extent ������ �����Ѵ�.
 ***********************************************************************/
typedef struct sdpscExtCntlHdr
{
    UInt           mTotExtCnt;         // Segment�� �Ҵ�� Extent �� ����
    scPageID       mLstExtDir;         // ������ ExtDir �������� PID
    UInt           mTotExtDirCnt;      // Extent Map�� �� ���� (SegHdr �����Ѱ���)
    UInt           mPageCntInExt;      // Extent�� ������ ����
} sdpscExtCntlHdr;


/***********************************************************************
 * Extent Directory Page Map ����
 ***********************************************************************/
typedef struct sdpscExtDirMap
{
    sdpscExtDesc  mExtDesc[1]; // Extent Descriptor
} sdpscExtDirMap;


/***********************************************************************
 * Extent Dir Control Header ����
 ***********************************************************************/
typedef struct sdpscExtDirCntlHdr
{
    UShort       mExtCnt;        // ���������� Extent ����
    scPageID     mNxtExtDir;     // ���� Extent Map �������� PID
    UShort       mMaxExtCnt;     // �ִ� Extent���� ����
    scOffset     mMapOffset;     // ������ ���� Extent Map�� Offset
    smSCN        mLstCommitSCN;  // ������ ����� Ŀ���� Ʈ������� CommitSCN
                                 // LatestCommitSCN
    smSCN        mFstDskViewSCN; // ����ߴ� Ȥ�� ������� Ʈ�������
                                 // First Disk ViewSCN
} sdpscExtDirHdr;


/***********************************************************************
 * Segment Header ������ ����
 ***********************************************************************/
typedef struct sdpscSegMetaHdr
{
    sdpscSegCntlHdr        mSegCntlHdr;     // Segment Control Header
    sdpscExtCntlHdr        mExtCntlHdr;     // Extent Control Header
    sdpscExtDirCntlHdr     mExtDirCntlHdr;  // ExtDir Control Header
    scPageID               mArrMetaPID[ SDP_MAX_SEG_PID_CNT ]; // Segment Meta PID
} sdpscSegMetaHdr;

/***********************************************************************
 * ��ȿ���� ���� ����
 ***********************************************************************/
# define SDPSC_INVALID_IDX         (-1)

/* Page BitSet�� 1����Ʈ�� �����Ѵ�. */
typedef UChar sdpscPageBitSet;

/***********************************************************************
 * �������� Page BitSet�� ������ ������ �����Ѵ�.
 ***********************************************************************/
# define SDPSC_BITSET_PAGETP_MASK          (0x80)
# define SDPSC_BITSET_PAGETP_META          (0x80)
# define SDPSC_BITSET_PAGETP_DATA          (0x00)

/***********************************************************************
 * �������� Page BitSet�� ������ ���뵵�� �����Ѵ�.
 ***********************************************************************/
# define SDPSC_BITSET_PAGEFN_MASK   (0x7F)
# define SDPSC_BITSET_PAGEFN_UNF    (0x00)
# define SDPSC_BITSET_PAGEFN_FMT    (0x01)
# define SDPSC_BITSET_PAGEFN_FUL    (0x02)

/*
 * ExtDir �� ���°� ����
 */
typedef enum sdpscExtDirState
{
    SDPSC_EXTDIR_EXPIRED  = 0, // ���밡���� ������ ����
    SDPSC_EXTDIR_UNEXPIRED,    // ������ �� ���� ������ ����
    SDPSC_EXTDIR_PREPARED      // prepareNewPage4Append�� ���ؼ� Ȯ����
                               // ���� ������ ���� �������� ������ �� ���� ����
} sdpscExtDirState;

# endif // _O_SDPSC_DEF_H_
