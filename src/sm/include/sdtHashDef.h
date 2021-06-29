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
 * Description :
 **********************************************************************/

#ifndef _O_SDT_HASH_DEF_H_
#define _O_SDT_HASH_DEF_H_ 1

#include <sdtDef.h>

#define SDT_HASH_UNIQUE      (1)
#define SDT_HASH_NORMAL      (0)

#define SDT_MIN_HASHSLOT_EXTENTCOUNT (2)
#define SDT_MIN_SUBHASH_EXTENTCOUNT  (1)
#define SDT_MIN_HASHROW_EXTENTCOUNT  (3)
#define SDT_MIN_HASH_EXTENTCOUNT     (6)
#define SDT_MIN_HASH_SIZE            (SDT_MIN_HASH_EXTENTCOUNT * SDT_WAEXTENT_SIZE)

typedef struct sdtHashGroup
{
    scPageID   mBeginWPID;
    scPageID   mEndWPID;
    scPageID   mReuseSeqWPID;

} sdtHashGroup;

typedef struct sdtHashSegHdr
{
    UChar                mWorkType;
    UChar                mIsInMemory;    /*�Ѱܳ� Page�� �ִ°�?*/
    scSpaceID            mSpaceID;

    UInt                 mHashSlotCount;      /* Slot ���� */

    UShort               mUnique;
    UShort               mOpenCursorType;

    scPageID             mEndWPID;

    sdtWCB            *  mUsedWCBPtr;
    /* ���������� ������ WAPage. Hint �� Dump�ϱ� ���� ���� */
    /* getNPageToWCB, getWAPagePtr, getWCB �� Hint */
    sdtWCB             * mInsertHintWCBPtr;

    // Hash 0 ~ | Row ~ | Fetch ~ | sub Hash ~ | end

    sdtHashGroup         mInsertGrp;
    sdtHashGroup         mFetchGrp;
    sdtHashGroup         mSubHashGrp;
    sdtWCB             * mSubHashWCBPtr;

    sdtWAExtentInfo      mWAExtentInfo;  // 20Byte, // XXX

    UInt                 mMaxWAExtentCount;
    sdtWAExtent        * mNxtFreeWAExtent;
    sdtWAExtent        * mCurFreeWAExtent;
    UInt                 mCurrFreePageIdx;
    UInt                 mAllocWAPageCount;

    sdtWCB            ** mNPageHashPtr;
    UInt                 mNPageHashBucketCnt;

    UInt                 mNPageCount;     /*�Ҵ��� NPage�� ���� */
    sdtNExtFstPIDList    mNExtFstPIDList4Row;  /*NormalExtent���� �����ϴ� Map*/
    sdtNExtFstPIDList    mNExtFstPIDList4SubHash;  /*NormalExtent���� �����ϴ� Map*/

    /* Fifo �Ǵ� LRU ��å�� ���� ������ ��Ȱ���� ���� ����.
     * �������� �Ҵ��� WPID�� ������.
     *�� �� ������ ���������� �ѹ��� ��Ȱ����� ���� �������̱� ������,
     * �� �������� ��Ȱ���ϸ� �ȴ�. */

    UInt                 mSubHashPageCount;
    UInt                 mSubHashBuildCount;
    UInt                 mHashSlotPageCount;
    UInt                 mHashSlotAllocPageCount;

    idvSQL             * mStatistics;
    smiTempTableStats  * mStatsPtr;

    /* ������ Ž���� Hash*/
    sdtWCB               mWCBMap[1];

} sdtHashSegHdr;

#define SDT_HASH_SEGMENT_HEADER_SIZE (ID_SIZEOF(sdtHashSegHdr) - ID_SIZEOF(sdtWCB))

typedef struct sdtHashPageHdr
{
    scPageID mSelfNPID;
    UShort   mType;
    UShort   mFreeOffset;

} sdtHashPageHdr;

/**************************************************************************
 * TempRowPiece�� ������ ���� �����ȴ�.
 *
 * +-------------------------------+.+---------+--------+.-------------------------+
 * + RowPieceHeader                |.|GRID HEADER       |. ColumnValues.(mtdValues)|
 * +----+-----+------+---------+---+.+---------+--------+.-------------------------+
 * |flag|IsRow|ValLen|HashValue|hit|.|ChildGRID|NextGRID|.ColumnValue|...                      XXX
 * +----+-----+------+---------+---+.+---------+--------+.-------------------------+
 * <----------   BASE    ---------->
 *
 * Base�� ��� rowPiece�� ������.
 * NextGRID�� ChildGRID�� �ʿ信 ���� ���� �� �ִ�.
 * (������ ������ ������, ChildGRID�� FirstRowPiece�� �����Ѵ�. *
 **************************************************************************/

#define SDT_HASH_ROW_HEADER (0x00)

/* TRPInfo(TempRowPieceInfo)�� Runtime����ü�� Runtime��Ȳ���� ���Ǹ�
 * ���� Page�󿡴� sdtHashTRPHdr(TempRowPiece)�� Value�� ��ϵȴ�.  */
typedef struct sdtHashTRPHdr
{
    /* Row�� ���� �������ִ� TR(TempRow)Flag.
     * (HashValue��� ����, ������ ���Խ� ���� ����, Column�ɰ��� ���� ) */
    UChar       mTRFlag;
    UChar       mIsRow;
    /* RowHeader�κ��� ������. Value�κ��� ���� */
    UShort      mValueLength;

    /* hashValue */
    UInt        mHashValue;

    /* hitSequence�� */
    UInt        mHitSequence;

    /******************************* Optional ****************************/
    /* IndexInternalNode�� UniqueTempHash������ �ش� Slot�� ChildRID��
     * ����ȴ�. */
    scPageID     mChildPageID;
    scOffset     mChildOffset;
    scOffset     mNextOffset;
    scPageID     mNextPageID;

    sdtHashTRPHdr * mChildRowPtr;
    sdtHashTRPHdr * mNextRowPiecePtr;
    /* �������� ���Ե� RowPiece�� RID�̴�. �׷��� ������ �������� ����Ǳ�
     * ������ ���������δ� ������ RowPiece�� RID�̴�.
     * �� FirstRowPiece �� ���� RowPiece���� �� NextRID�� ����Ǿ��ִ�. */
} sdtHashTRPHdr;

typedef struct sdtHashInsertInfo
{
    sdtHashTRPHdr   mTRPHeader;

    UInt            mColumnCount;  /* Column���� */
    smiTempColumn * mColumns;      /* Column���� */
    UInt            mValueLength;  /* Value�� ���� */
    smiValue      * mValueList;    /* Value�� */
} sdtHashInsertInfo;

typedef struct sdtHashInsertResult
{
    sdtHashTRPHdr* mHeadRowpiecePtr;  /* �Ӹ� Rowpiece�� Pointer */
    scPageID       mHeadPageID;       /* �Ӹ� Rowpiece�� PageID */
    scOffset       mHeadOffset;       /* �Ӹ� Rowpiece�� PageID */
    idBool         mComplete;         /* ���� �����Ͽ��°� */
} sdtHashInsertResult;

typedef struct sdtHashScanInfo
{
    sdtHashTRPHdr  * mTRPHeader;

    UInt            mValueLength;    /* Value�� ���� */
    UInt            mFetchEndOffset; /* ���� ���� fetch */
    UChar         * mValuePtr;       /* Value�� ù ��ġ*/
} sdtHashScanInfo;


/* �ݵ�� ��ϵǾ� �ϴ� �׸��. ( sdtHashTRPHdr ���� )
 * mTRFlag, mValueLength, mHashValue, mHitSequence,
 * (1) + (1) + (2) + (4) + (4) = 12
 * (2) + (2) + (4) + (4) + (8) + (8) = 28*/
// 12 + dummy 4,
#define SDT_HASH_TR_HEADER_SIZE_BASE ( 16 )
#define SDT_HASH_TR_HEADER_SIZE_FULL ( 40 )

#endif  // _O_SDT_HASH_DEF_H_
