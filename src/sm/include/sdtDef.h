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
 * $Id: sdtDef.h 79989 2017-05-15 09:58:46Z et16 $
 * Description :
 **********************************************************************/

#ifndef _O_SDT_DEF_H_
#define _O_SDT_DEF_H_ 1

#include <smDef.h>
#include <iduLatch.h>
#include <smuUtility.h>
#include <iduMutex.h>

typedef enum
{
    SDT_WA_PAGESTATE_INIT,          /* NPage�� �Ҵ�Ǿ� ���� ����. */
    SDT_WA_PAGESTATE_CLEAN,         /* Assign�ž����� ������ ������ */
    SDT_WA_PAGESTATE_DIRTY          /* ������ ����Ǿ���, ���� Write���� �ʾ���*/
} sdtWAPageState;

typedef enum
{
    SDT_WORK_TYPE_NONE,
    SDT_WORK_TYPE_HASH,
    SDT_WORK_TYPE_SORT,
    SDT_WORK_TYPE_MAX
} sdtWorkType;

typedef struct sdtWCB
{
    sdtWAPageState mWPState;
    SInt           mFix;

    /* �ڽ��� Page ������ */
    UChar        * mWAPagePtr;
    idBool         mBookedFree; /* Free�� ����� */
    /* �� WAPage�� ����� NPage : ��ũ�� �����Ѵ�.  */
    scPageID       mNPageID;

    /* ������ Ž���� Hash�� ���� LinkPID */
    sdtWCB       * mNextWCB4Hash;
    sdtWCB       * mNxtUsedWCBPtr;

    /* LRUList������ */
    scPageID       mLRUPrevPID;
    scPageID       mLRUNextPID;
} sdtWCB;

/* INMEMORY -> DiscardPage�� ������� �ʴ´�. ���� ���ʺ��� �������� �Ҵ�
 *             ���ش�. WAMap���� ���ռ��� �ø��� �����̴�.
 * FIFO -> ���ʺ��� ������� �Ҵ����ش�. ���� ����ϸ�, ���ʺ��� �����Ѵ�.
 * LRU  -> ������� ������ �������� �Ҵ��Ѵ�. getWAPage�� �ϸ� Top���� �ø���.
 * */
typedef enum
{
    SDT_WA_REUSE_NONE,
    SDT_WA_REUSE_INMEMORY, /* REUSE�� ������� ������,�ڿ������� ������ �Ҵ�*/
    SDT_WA_REUSE_FIFO,     /* ���������� �������� ����� */
    SDT_WA_REUSE_LRU       /* LRU�� ���� �������� �Ҵ����� */
} sdtWAReusePolicy;

#define SDT_MAX_WAFLUSHER_COUNT (64)

#define SDT_USED_PAGE_PTR_TERMINATED ((sdtWCB*)1)

/* Extent �ϳ��� Page���� 64���� ���� */
#define SDT_WAEXTENT_PAGECOUNT      (64)
#define SDT_WAEXTENT_PAGECOUNT_MASK (SDT_WAEXTENT_PAGECOUNT-1)

/* WGRID, NGRID ���� �����ϱ� ����, SpaceID�� ����� */
#define SDT_SPACEID_WORKAREA  ( ID_USHORT_MAX )     /*InMemory,WGRID */
#define SDT_SPACEID_WAMAP     ( ID_USHORT_MAX - 1 ) /*InMemory,WAMap Slot*/
/* Slot�� ���� ������� ���� ���� */
#define SDT_WASLOT_UNUSED     ( ID_UINT_MAX )

#define SDT_WAEXTENT_SIZE     (SD_PAGE_SIZE * SDT_WAEXTENT_PAGECOUNT)


typedef struct sdtWAExtent
{
    // Page�� Flush �ϱ� ���ؼ� align�� ����� �Ѵ�.
    UChar         mPage[SDT_WAEXTENT_PAGECOUNT][SD_PAGE_SIZE];
    // align�� ���� ���߱� ���� Next Extent�� Page �ڿ� ��ġ
    sdtWAExtent * mNextExtent;
}sdtWAExtent;

#define SDT_NEXTARR_EXTCOUNT (128)

typedef struct sdtNExtentArr
{
    scPageID        mMap[SDT_NEXTARR_EXTCOUNT];
    sdtNExtentArr * mNextArr;
} sdtNExtentArr;

typedef struct sdtNExtFstPIDList
{
    ULong      mCount;
    UInt       mPageSeqInLFE;   /*LFE(LastFreeExtent)������ �Ҵ��ؼ� �ǳ��������� ��ȣ */
    scPageID   mLastFreeExtFstPID; /*NExtentArray�� �������� Extent*/
    sdtNExtentArr  * mHead;
    sdtNExtentArr  * mTail;
} sdtNExtFstPIDList;

typedef struct sdtWAExtentInfo
{
    sdtWAExtent   * mHead;
    sdtWAExtent   * mTail;
    UInt            mCount;
} sdtWAExtentInfo;

#define SDT_WORKAREA_IN_MEMORY      (1)
#define SDT_WORKAREA_OUT_MEMORY     (0)

#define SDT_TEMP_FREEOFFSET_BITSIZE (13)
#define SDT_TEMP_FREEOFFSET_BITMASK (0x1FFF)
#define SDT_TEMP_TYPE_SHIFT         (SDT_TEMP_FREEOFFSET_BITSIZE)
#define SDT_TEMP_TYPE_BITMASK       (0xE000)

#define SDT_TEMP_SLOT_UNUSED      (ID_USHORT_MAX)

typedef enum
{
    SDT_TEMP_PAGETYPE_INIT,
    SDT_TEMP_PAGETYPE_INMEMORYGROUP,
    SDT_TEMP_PAGETYPE_SORTEDRUN,
    SDT_TEMP_PAGETYPE_LNODE,
    SDT_TEMP_PAGETYPE_INODE,
    SDT_TEMP_PAGETYPE_INDEX_EXTRA,
    SDT_TEMP_PAGETYPE_HASHROWS,
    SDT_TEMP_PAGETYPE_SUBHASH,
    SDT_TEMP_PAGETYPE_MAX
} sdtTempPageType;

typedef struct sdtTempPageHdr
{
    /* ���� 13bit�� FreeOffset, ���� 3bit�� Type���� ����� */
    scPageID mSelfNPID;
    UShort   mTypeAndFreeOffset;
    UShort   mDummy;
} sdtTempPageHdr;

/*****************************************************************************
 * PROJ-2201 Innovation in sorting and hashing(temp)
 *****************************************************************************/

#define SDT_TRFLAG_NULL              (0x00)
#define SDT_TRFLAG_HEAD              (0x01) /*Head RowPiece���� ����.*/
#define SDT_TRFLAG_NEXTGRID          (0x02) /*NextGRID�� ����ϴ°�?*/
#define SDT_TRFLAG_CHILDGRID         (0x04) /*ChildGRID�� ����ϴ°�?*/
#define SDT_TRFLAG_UNSPLIT           (0x10) /*�ɰ����� �ʵ��� ������*/
/* GRID�� �����Ǿ� �ִ°�? */
#define SDT_TRFLAG_GRID     ( SDT_TRFLAG_NEXTGRID | SDT_TRFLAG_CHILDGRID )

// �κ� %�� ��ü 100%�� ũ�⸦ ����Ѵ�. 1���� ������ 1�� ����
#define SDT_GET_FULL_SIZE( aPartSize, aPartRatio )                 \
    (( (SInt)(aPartRatio) < 1 ) ?                                  \
     ( (aPartSize) * 100 ) :                                       \
     ( ((aPartSize) * 100 + (aPartRatio-1)) / (aPartRatio) ))

#endif  // _O_SDT_DEF_H_
