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
 * $Id: sdcDef.h 88545 2020-09-10 09:14:02Z emlee $
 **********************************************************************/
#ifndef  _O_SDC_DEF_H_
#define  _O_SDC_DEF_H_  1

#include <smDef.h>
#include <sdp.h>
#include <smrDef.h>
#include <sdbDef.h>


#define SDC_MOVE_PTR_TRIPLE(a1stPtr, a2ndPtr, a3rdPtr, aSize)   \
    IDE_DASSERT((a1stPtr) != NULL);                             \
    IDE_DASSERT((a2ndPtr) != NULL);                             \
    IDE_DASSERT((a3rdPtr) != NULL);                             \
    IDE_DASSERT((aSize) > 0);                                   \
    IDE_DASSERT( ID_SIZEOF(*(a1stPtr)) == 1 );                  \
    IDE_DASSERT( ID_SIZEOF(*(a2ndPtr)) == 1 );                  \
    IDE_DASSERT( ID_SIZEOF(*(a3rdPtr)) == 1 );                  \
    (a1stPtr) += (aSize);                                       \
    (a2ndPtr) += (aSize);                                       \
    (a3rdPtr) += (aSize)


typedef enum sdcTSState
{
    SDC_TSS_STATE_ACTIVE,   // TSS�� Ʈ����ǿ� Binding�� ����
    SDC_TSS_STATE_COMMIT,   // Ʈ������� Ŀ���� ����
    SDC_TSS_STATE_ABORT     // Ʈ������� �ѹ��� ����
} sdcTSState;

/**********************************************************************
 * undo record�� Ÿ�� ����
 **********************************************************************/
# define SDC_UNDO_INSERT_ROW_PIECE             (0x00)
# define SDC_UNDO_INSERT_ROW_PIECE_FOR_UPDATE  (0x01)
# define SDC_UNDO_UPDATE_ROW_PIECE             (0x02)
# define SDC_UNDO_OVERWRITE_ROW_PIECE          (0x03)
# define SDC_UNDO_CHANGE_ROW_PIECE_LINK        (0x04)
# define SDC_UNDO_DELETE_FIRST_COLUMN_PIECE    (0x05)
# define SDC_UNDO_DELETE_ROW_PIECE_FOR_UPDATE  (0x06)
# define SDC_UNDO_DELETE_ROW_PIECE             (0x07)
# define SDC_UNDO_LOCK_ROW                     (0x08)
# define SDC_UNDO_INDEX_CTS                    (0x09) // PROJ-1704 Disk MVCC Renewal
# define SDC_UNDO_UPDATE_LOB_LEAF_KEY          (0x0a) // PROJ-2047 Strengthening LOB

/**********************************************************************
 * Transaction Status Slot ����
 *
 * TSS�� ������ Ʈ������� ���� �� CommitSCN�� �ٸ� Ʈ������� Ȯ���� �� �ִ�.
 * �ٸ� Ʈ������� Row Time-Stamping ����� TSS�κ��� �ǵ��� CommitSCN��
 * ���� �ִ� Row Piece Header�鿡 �����Ѵ�.
 * Ʈ������� �Ҵ����� ������ Infinite SCN(0x8000000000000000)�� �����Ǹ�,
 * Commit�ÿ��� CommitSCN, Rollback�ÿ��� Init SCN(0x0000000000000000)��
 * �����ȴ�.
 * ���� Commit���Ŀ� CommitSCN�� �������� ���� ��쿡�� ������ ������ ������
 * ��� ���� Restart Recovery �������� Commit Log�� ���ؼ� Init SCN
 * (0x0000000000000000)�� �����Ͽ� �ٸ� Ʈ������� �����ϰų� ���� �ְ� �Ѵ�.
 **********************************************************************/
typedef struct sdcTSS
{
    smTID             mTransID;      // Ʈ����� ID
    sdcTSState        mState;        // Active/Commit/Rollback
    smSCN             mCommitSCN;    // InfiniteSCN Ȥ�� CommitSCN
} sdcTSS;

/**********************************************************************
 * TSS Page Control Header
 *
 * TSS �������� �Ҵ��� Ȥ�� �ٷ� ������ ����ߴ� Ʈ������� ������ ����Ͽ� TSS��
 * ���뿩�θ� �Ǵ��� �� �ֵ��� �Ѵ�.
 *
 * TSSPage.TxBeginSCN > Page�� Commit ���� ���� CTS.TxBeginSCN
 * �� �����ϸ� ����� ������ �Ǵ��� �� �ִ�.
 **********************************************************************/
typedef struct sdcTSSPageCntlHdr
{
    smTID       mTransID;       // Undo Record�� ����� Ʈ����� ID
    smSCN       mFstDskViewSCN; // TSS�� �Ҵ��ذ� Ʈ������� Begin SCN
} sdcTSSPageCntlHdr;

/* ------------------------------------------------
 * undo record header ����
 * ----------------------------------------------*/

typedef UChar sdcUndoRecType;
typedef UChar sdcUndoRecFlag;

#define SDC_UNDOREC_HDR_FIELD_OFFSET(aField)    \
    (aField ## _OFFSET)

#define SDC_UNDOREC_HDR_FIELD_SIZE(aField)      \
    (aField ## _SIZE)

#define SDC_UNDOREC_HDR_TYPE_SIZE       (1)
#define SDC_UNDOREC_HDR_FLAG_SIZE       (1)
#define SDC_UNDOREC_HDR_TABLEOID_SIZE   (ID_SIZEOF(smOID))
#define SDC_UNDOREC_HDR_SIZE            \
    ( SDC_UNDOREC_HDR_TYPE_SIZE +       \
      SDC_UNDOREC_HDR_FLAG_SIZE +       \
      SDC_UNDOREC_HDR_TABLEOID_SIZE )

/*
 * undo record header���� tableoid �ʵ�(smOID type)�� �ִµ�
 * smOID(vULong) type�� �� bit ��������� ���� ũ�Ⱑ �������̴�.
 * (32bit ��� : 4byte,  64bit ��� : 8byte)
 * �̷� ���� SDC_MAX_ROWPIECE_SIZE�� ũ�⵵
 * �� ��Ʈ ��������� ���� �������� �ȴ�.
 * �̷��� �Ǹ� PROJ-1705���� �߰��� �׽�Ʈ���̽�����
 * diff�� �߻��ϴµ�(dump�� ��� ����), lst�� �ΰ�(32bit, 64bit) ����� ����
 * �ʹ� ���ŷӴٰ� �����ߴ�. �׷��� SDC_UNDOREC_HDR_MAX_SIZE ��ũ�θ�
 * �����ϰ� tableoid�� ũ�⸦ 8byte�� ����ϵ��� �Ͽ���.
 * */
#define SDC_UNDOREC_HDR_MAX_SIZE        \
    ( SDC_UNDOREC_HDR_TYPE_SIZE +       \
      SDC_UNDOREC_HDR_FLAG_SIZE +       \
      ID_SIZEOF(ULong) )

#define SDC_UNDOREC_HDR_TYPE_OFFSET     (0)
#define SDC_UNDOREC_HDR_FLAG_OFFSET     \
    ( SDC_UNDOREC_HDR_TYPE_OFFSET +     \
      SDC_UNDOREC_HDR_TYPE_SIZE )
#define SDC_UNDOREC_HDR_TABLEOID_OFFSET \
    ( SDC_UNDOREC_HDR_FLAG_OFFSET +     \
      SDC_UNDOREC_HDR_FLAG_SIZE )

#define SDC_GET_UNDOREC_HDR_FIELD_PTR(aHdr, aField)   \
    ( (aHdr) + SDC_UNDOREC_HDR_FIELD_OFFSET(aField) )

#define SDC_GET_UNDOREC_HDR_FIELD(aHdr, aField, aRetPtr)                \
    (idlOS::memcpy( (void*)(aRetPtr),                                   \
                    (void*)SDC_GET_UNDOREC_HDR_FIELD_PTR(aHdr, aField), \
                    SDC_UNDOREC_HDR_FIELD_SIZE(aField) ))

#define SDC_SET_UNDOREC_HDR_FIELD(aHdr, aField, aValPtr)                \
    (idlOS::memcpy( (void*)SDC_GET_UNDOREC_HDR_FIELD_PTR(aHdr, aField), \
                    (void*)(aValPtr),                                   \
                    SDC_UNDOREC_HDR_FIELD_SIZE(aField) ))

#define SDC_GET_UNDOREC_HDR_1B_FIELD(aHdr, aField, aRet)              \
    (*aRet = *(UChar*)SDC_GET_UNDOREC_HDR_FIELD_PTR(aHdr, aField))

#define SDC_SET_UNDOREC_HDR_1B_FIELD(aHdr, aField, aVal)              \
    (*(UChar*)SDC_GET_UNDOREC_HDR_FIELD_PTR(aHdr, aField) = (aVal))


#define SDC_SET_UNDOREC_FLAG_ON(set, f) \
    ((set) |= (f))

#define SDC_SET_UNDOREC_FLAG_OFF(set, f)     \
    ((set) &= ~(f))


/* Undo Record Header�� Flag���� */
#define SDC_UNDOREC_FLAG_IS_VALID_MASK  (0x01)
#define SDC_UNDOREC_FLAG_IS_VALID_TRUE  (0x01)
#define SDC_UNDOREC_FLAG_IS_VALID_FALSE (0x00)

#define SDC_UNDOREC_FLAG_UNDO_FOR_HEAD_ROWPIECE_MASK  (0x02)
#define SDC_UNDOREC_FLAG_UNDO_FOR_HEAD_ROWPIECE_TRUE  (0x02)
#define SDC_UNDOREC_FLAG_UNDO_FOR_HEAD_ROWPIECE_FALSE (0x00)

#define SDC_UNDOREC_FLAG_UNDO_FOR_LOB_UPDATE_MASK  (0x04)
#define SDC_UNDOREC_FLAG_UNDO_FOR_LOB_UPDATE_TRUE  (0x04)
#define SDC_UNDOREC_FLAG_UNDO_FOR_LOB_UPDATE_FALSE (0x00)

#define SDC_UNDOREC_FLAG_IS_UNDO_FOR_LOB_UPDATE(aFlag)               \
    ( ( ( (aFlag) & SDC_UNDOREC_FLAG_UNDO_FOR_LOB_UPDATE_MASK )      \
        == SDC_UNDOREC_FLAG_UNDO_FOR_LOB_UPDATE_TRUE ) ?             \
      ID_TRUE : ID_FALSE )

#define SDC_UNDOREC_FLAG_IS_UNDO_FOR_HEAD_ROWPIECE(aFlag)               \
    ( ( ( (aFlag) & SDC_UNDOREC_FLAG_UNDO_FOR_HEAD_ROWPIECE_MASK )      \
        == SDC_UNDOREC_FLAG_UNDO_FOR_HEAD_ROWPIECE_TRUE ) ?             \
      ID_TRUE : ID_FALSE )

#define SDC_UNDOREC_FLAG_IS_VALID(aFlag)                                \
    ( ( ( (aFlag) & SDC_UNDOREC_FLAG_IS_VALID_MASK )                    \
        == SDC_UNDOREC_FLAG_IS_VALID_TRUE ) ?                           \
      ID_TRUE : ID_FALSE )

typedef struct sdcVarColHdr
{
    UShort    offset;
    UShort    length;
} sdcVarColHdr;

/* BUG-25624 ��ũ���̺��� CTS�Ҵ� ���з� ���� ����ȭ ADI��
 *           Resource Deadlock����
 * RowHdr�� Ȯ�念���� ����ϴ� Ʈ����� ���� ����
 * ��ũ ���̺� ���������� ����Ʈ������� CTS��
 * �Ҵ���� ��� RowPiece ����� Ȯ�念���� Ʈ�������
 * ���������� ����Ѵ�. */
typedef struct sdcRowHdrExInfo
{
    scPageID   mTSSPageID;
    scSlotNum  mTSSlotNum;
    UShort     mFSCredit;
    smSCN      mFSCNOrCSCN;
} sdcRowHdrExInfo;

typedef struct sdcRowHdrInfo
{
    UChar            mCTSlotIdx;
    smSCN            mInfiniteSCN;
    sdSID            mUndoSID;
    UShort           mColCount;
    UChar            mRowFlag;
    sdcRowHdrExInfo  mExInfo;
} sdcRowHdrInfo;


typedef struct sdcUndoRecHdrInfo
{
    UChar        mType;
    UChar        mFlag;
    smOID        mTableOID;
} sdcUndoRecHdrInfo;

typedef enum sdcColInOutMode
{
    SDC_COLUMN_IN_MODE      = SMI_COLUMN_MODE_IN,
    SDC_COLUMN_OUT_MODE_LOB = SMI_COLUMN_MODE_OUT
} sdcColInOutMode;

// Value�� ����Ű�� Pointer
// ���� �����Ѿ� �� �� ���
typedef struct sdcValue
{
    smiValue          mValue;
    smiValue          mOutValue;
    sdcColInOutMode   mInOutMode; // Value�� In Out Mode
} sdcValue;

// PROJ-2399 rowTemplate fetch�� ����� colum value ����ü
typedef struct sdcValue4Fetch
{
    sdcColInOutMode mInOutMode; // Value�� In Out Mode
    UShort          mColLenStoreSize;
    smiValue        mValue;
} sdcValue4Fetch;

typedef struct sdcColumnInfo4Insert
{
    const smiColumn * mColumn;
    idBool            mIsUptCol;    // Insert Row Piece For Update����
                                    // ����ϸ� Update Column����
                                    // �ƴ����� ��Ÿ����.
    sdcValue          mValueInfo;
} sdcColumnInfo4Insert;


/* insert rowpiece ����ÿ� ����ϴ� �ڷᱸ���̴�. */
typedef struct sdcRowPieceInsertInfo
{
    UInt                   mStartColOffset;
    UShort                 mStartColSeq;

    UShort                 mEndColSeq;
    UInt                   mEndColOffset;

    UShort                 mRowPieceSize;   /* �����Ϸ��� rowpiece�� ũ�� */
    UShort                 mColCount;       /* total column count in rowpiece */
    
    idBool                 mIsInsert4Upt;   /* insert rowpiece for update �������� ���� */
    idBool                 mIsUptLobByAPI;  /* API�� ���� LOB Update������ ���� */

    UShort                 mLobDescCnt;

    /* �÷����� �迭 */
    sdcColumnInfo4Insert   mColInfoList[SMI_COLUMN_ID_MAXIMUM];

} sdcRowPieceInsertInfo;

typedef struct sdcColumnInfo4Update
{
    /* mColumn�� ���� ���� update �÷����� ���θ� �Ǵ��Ѵ�.
     * mColumn == NULL : update �÷� X
     * mColumn != NULL : update �÷� O */
    const smiColumn * mColumn;
    sdcValue          mNewValueInfo; // Update�Ϸ��� New Value
    sdcValue          mOldValueInfo; // ����Ǿ��ִ� Old Value

} sdcColumnInfo4Update;

typedef struct sdcRowPieceUpdateInfo
{
    sdcRowHdrInfo         *mNewRowHdrInfo;
    const sdcRowHdrInfo   *mOldRowHdrInfo;

    UInt                   mNewRowPieceSize;
    UShort                 mOldRowPieceSize;

    /* In Mode LOB�� �����Ͽ� In Mode�� Update Column�� Cnt */
    UShort                 mUptBfrInModeColCnt;
    UShort                 mUptAftInModeColCnt;
    UShort                 mUptAftLobDescCnt;
    UShort                 mUptBfrLobDescCnt;
    
    UShort                 mTrailingNullUptCount;

    /* delete first column piece ������ �������� ���� */
    idBool                 mIsDeleteFstColumnPiece;
    /* update inplace ������ �������� ���� */
    idBool                 mIsUpdateInplace;
    /* trailing null�� update �Ϸ����� ���� */
    idBool                 mIsTrailingNullUpdate;
    /* API�� ���� LOB Update������ ���� */
    idBool                 mIsUptLobByAPI; 
    
    sdSID                  mRowPieceSID;

    /* �÷����� �迭 */
    sdcColumnInfo4Update   mColInfoList[SMI_COLUMN_ID_MAXIMUM];
    /* rowpiece�� ����� �÷����� old value�� �����ϱ� ���� 8K buffer */
    UChar                  mSpace4CopyOldValue[SD_PAGE_SIZE];

} sdcRowPieceUpdateInfo;

typedef struct sdcRowPieceOverwriteInfo
{
    /* sdcRowPieceUpdateInfo �ڷᱸ����
     * mColInfoList �迭�� ����Ű�� pointer�̴�. */
    const sdcColumnInfo4Update  *mColInfoList;

    sdcRowHdrInfo               *mNewRowHdrInfo;
    const sdcRowHdrInfo         *mOldRowHdrInfo;

    UInt                         mNewRowPieceSize;
    UShort                       mOldRowPieceSize;

    UShort                       mUptAftInModeColCnt;
    UShort                       mUptAftLobDescCnt;

    /* only use for rp logging */
    UShort                       mUptInModeColCntBfrSplit;
    
    UShort                       mTrailingNullUptCount;

    /* overwrite rowpiece ������ ���
     * split�� �߻����� �� �ֱ� ������, last column�� �����Ҷ�
     * �߷��� ũ��(mLstColumnOverwriteSize)��ŭ�� �����ؾ� �Ѵ�. */
    UShort                       mLstColumnOverwriteSize;

    /* API�� ���� LOB Update������ ���� */
    idBool                       mIsUptLobByAPI;

    sdSID                        mRowPieceSID;
} sdcRowPieceOverwriteInfo;

/* update ������ ������¿� ���� ������ �����ϴ� �ڷᱸ���̴�. */
typedef struct sdcRowUpdateStatus
{
    UShort    mTotalUpdateColCount;    /* update�� �����ؾ� �� column�� ���� */
    UShort    mUpdateDoneColCount;     /* update ������ �Ϸ��� column�� ���� */

    UShort    mFstColumnSeq;           /* row piece���� ù��° column piece��
                                        * sequence */

    UShort    mLstUptColumnSeq;        /* ������ update column�� sequence
                                        * trailing null update ó���ÿ�
                                        * �� ������ �̿��Ѵ�. */

    UChar     mPrevRowPieceRowFlag;    /* BUG-32278: ���� row piece�� row flag
                                        * �� �����Ѵ�. row flag�� ����������
                                        * ���ȴ�. */ 
} sdcRowUpdateStatus;

#define SDC_SUPPLEMENT_JOB_NONE                             (0x00000000)
#define SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_UPDATE     (0x00000001)
#define SDC_SUPPLEMENT_JOB_UPDATE_LOB_COLS_AFTER_OVERWRITE  (0x00000002)
#define SDC_SUPPLEMENT_JOB_CHANGE_ROWPIECE_LINK             (0x00000004)
#define SDC_SUPPLEMENT_JOB_REMOVE_OLD_OUT_MODE_LOB          (0x00000008)

typedef struct sdcSupplementJobInfo
{
    // Supplement Job�� ������ ����Ǿ� �ִ�.
    UInt                    mJobType;

    /* change rowpiece link ����ÿ�,
     * �Ʒ� ������ �����ص� next rowpiece sid ���� �̿��Ѵ�. */
    sdSID                   mNextRowPieceSID;
} sdcSupplementJobInfo;

typedef struct sdcColumnInfo4Fetch
{
    /* mColumn�� ���� ���� fetch �÷����� ���θ� �Ǵ��Ѵ�.
     * mColumn == NULL : fetch �÷� X
     * mColumn != NULL : fetch �÷� O */
    const smiColumn              *mColumn;
    sdcValue                      mValueInfo;

    /* �����͸� �����Ҷ�, MT datatype format���� �������� �ʰ�
     * raw value�� �����Ѵ�.(���� ������ ���̱� ���ؼ��̴�.)
     * �׷��� ����� data�� QP���� ������ ��,
     * memcpy�� �ϸ� �ȵǰ�, QP�� ������ callback �Լ��� ȣ���ؾ� �Ѵ�.
     * QP�� ������ callback function pointer�� mCallback�� �����Ѵ�. */
    smiCopyDiskColumnValueFunc    mCopyDiskColumn;

    /* MT datatype format�� size�� ���Ҷ� ����ϴ� callback �Լ� */
    smiActualSizeFunc             mActualSize;

    UShort                        mColSeqInRowPiece;
} sdcColumnInfo4Fetch;

typedef struct sdcRowPieceFetchInfo
{
    /* �÷����� �迭 */
    sdcColumnInfo4Fetch   mColInfoList[SMI_COLUMN_ID_MAXIMUM];

    UShort                mFetchColCount;
} sdcRowPieceFetchInfo;

typedef struct sdcRowFetchStatus
{
    UShort    mTotalFetchColCount;    /* fetch�� �����ؾ� �� column�� ���� */
    UShort    mFetchDoneColCount;     /* fetch ������ �Ϸ��� column�� ���� */

    UShort    mFstColumnSeq;          /* row piece���� ù��° column piece��
                                       * sequence */

    UInt      mAlreadyCopyedSize;     /* ���� rowpiece�� ������ ����� �÷���
                                       * fetch�ϴ� ���, copy offset ������
                                       * �����Ѵ�. */
    const smiFetchColumnList * mFstFetchConlumn; /* row piece���� ã�ƾ��� ù��°
                                                  * fetch ��� column */
} sdcRowFetchStatus;

/* BUG-22943 index bottom up build ���ɰ��� */
typedef IDE_RC (*sdcCallbackFunc4Index)( const smiColumn * aIndexVRowColumn,
                                         UInt              aCopyOffset,
                                         const smiValue  * aColumnValue,
                                         void            * aIndexInfo );

/* BUG-22943 index bottom up build ���ɰ��� */
typedef struct sdcIndexInfo4Fetch
{
    const void                *mTableHeader;
    UShort                     mVarOffset;
    sdcCallbackFunc4Index      mCallbackFunc4Index;
    smiValue                   mValueList[ SMI_COLUMN_ID_MAXIMUM ];
    UChar                     *mBuffer;
    UChar                     *mBufferCursor;

    /* BUG-24091
     * [SD-����߰�] vrow column ���鶧 ���ϴ� ũ�⸸ŭ�� �����ϴ� ��� �߰� */
    /* vrow column ���鶧 fetchSize ũ�� �̻��� �������� �ʴ´�. */
    UInt                      mFetchSize;
} sdcIndexInfo4Fetch;

typedef struct sdcColumnInfo4PK
{
    const smiColumn    *mColumn;
    smiValue            mValue;
    sdcColInOutMode     mInOutMode;
} sdcColumnInfo4PK;

/* pk ������ �����ϴ� �ڷᱸ���̴�. */
typedef struct sdcPKInfo
{
    /* �÷����� �迭 */
    sdcColumnInfo4PK    mColInfoList[SMI_MAX_IDX_COLUMNS];
    /* PK value�� �����ϱ� ���� 4K buffer */
    UChar               mSpace4CopyPKValue[SD_PAGE_SIZE/2];

    /* primary key index�� �����Ǿ� �ִ� column�� ����  */
    UShort              mTotalPKColCount;
    /* ���縦 �Ϸ��� pk column�� ���� */
    UShort              mCopyDonePKColCount;

    /* row piece���� ù��° column piece�� sequence */
    UShort              mFstColumnSeq;
} sdcPKInfo;


/* ------------------------------------------------
 * updatable check ���°�
 * ----------------------------------------------*/
typedef enum sdcUpdateState
{
    SDC_UPTSTATE_NULL,

    // update ���� ����
    SDC_UPTSTATE_UPDATABLE,

    // delete �� ����
    SDC_UPTSTATE_ALREADY_DELETED,

    // �̹� �ڽ��� statement�� ���� ���ŵ�
    SDC_UPTSTATE_INVISIBLE_MYUPTVERSION,

    // �ٸ� Tx�� ���������� ���� commit���� �ʾ�����
    // commit�Ǳ⸦ ��ٷ� retry�Ѵ�.
    SDC_UPTSTATE_UPDATE_BYOTHER,

    // �̹� �ٸ� Ʈ������� �� StmtSCN���� ū CSCN
    // ���� ������ ���
    SDC_UPTSTATE_REBUILD_ALREADY_MODIFIED,

    // �ε������� Unique Vilolation �� ���
    SDC_UPTSTATE_UNIQUE_VIOLATION,

    // Row Retry �� �ʿ��� ���
    SDC_UPTSTATE_ROW_RETRY,
} sdcUpdateState;

/* X$TSSEG ������� �ڷᱸ�� */
typedef struct sdcTSSegInfo
{
    UInt          mSpaceID;        // TBSID
    scPageID      mSegPID;         // ���׸�Ʈ PID
    UShort        mType;           // ���׸�Ʈ Ÿ��
    UShort        mState;          // ���׸�Ʈ ����
    UInt          mTXSegID;        // Ʈ����� ���׸�Ʈ ID
    ULong         mTotExtCnt;      // �� ExtDesc ����
    ULong         mTotExtDirCnt;   // �� ExtDir ����
    sdRID         mCurAllocExtRID; // ���� ������� ExtDesc RID
    scPageID      mCurAllocPID;    // ���� ������� �������� PID
    UInt          mPageCntInExt;   // ExtDesc �� ������ ����
} sdcTSSegInfo;

typedef sdcTSSegInfo sdcUDSegInfo;

/*
 * X$DISK_UNDO_RECORDS ����ü
 */
typedef struct sdcUndoRec4FT
{
    UInt       mSegSeq;                        // SEG_SEQ
    scPageID   mSegPID;                        // SEG_PID
    scPageID   mPageID;                        // PAGE_ID
    scOffset   mOffset;                        // OFFSET
    UShort     mNthSlot;                       // NTH_SLOT
    UInt       mSize;                          // SIZE
    UInt       mType;                          // TYPE
    UInt       mFlag;                          // FLAG
    ULong      mTableOID;                      // TABLE_OID
} sdcUndoRec4FT;

/*
 * X$DISK_TSS_RECORDS ����ü
 */
typedef struct sdcTSS4FT
{
    UInt       mSegSeq;          // SEG_SEQ
    scPageID   mSegPID;          // SEG_PID
    scPageID   mPageID;          // PAGE_ID
    scOffset   mOffset;          // OFFSET
    SShort     mNthSlot;         // NTH_SLOT
    smTID      mTransID;         // TID
    SChar    * mCSCN;            // CSCN
    sdcTSState mState;           // STATE
} sdcTSS4FT;

/*
 * ROJ-1704 Disk MVCC Renewal
 *
 * Ʈ����� ���׸�Ʈ ��Ʈ���� ���°��� �����Ѵ�.
 *
 * OFFLINE - Ʈ����� ���׸�Ʈ ��Ʈ���� �Ҵ���� ���� ����
 * ONLINE  - Ʈ����� ���׸�Ʈ ��Ʈ���� Ʈ����ǿ� �Ҵ�� ����
 */
typedef enum sdcTXSegStatus
{
    SDC_TXSEG_OFFLINE = 0,
    SDC_TXSEG_ONLINE
} sdcTXSegStatus;

/* Row Version�� ���� ���� ���� ���� */
typedef enum sdcOperToMakeRowVer
{
    SDC_MVCC_MAKE_VALROW,
    SDC_REDO_MAKE_NEWROW,
    SDC_UNDO_MAKE_OLDROW
} sdcOperToMakeRowVer;

/* CTS�� FSCOrCSCN�� ���� COMMIT SCN ���θ��� Ȯ���Ѵ� */
#ifdef COMPILE_64BIT
#define SDC_CTS_SCN_IS_COMMITTED( SCN )        \
    ( ( ( SCN ) & SM_SCN_COMMIT_PARITY_BIT ) != SM_SCN_COMMIT_PARITY_BIT )
#define SDC_CTS_SCN_IS_NOT_COMMITTED( SCN )    \
    ( ( ( SCN ) & SM_SCN_COMMIT_PARITY_BIT ) == SM_SCN_COMMIT_PARITY_BIT )
#define SDC_CTS_SCN_IS_LEGACY( SCN )        \
    ( ( ( SCN ) & SM_SCN_COMMIT_LEGACY_BIT ) == SM_SCN_COMMIT_LEGACY_BIT )
#define SDC_CTS_SCN_IS_NOT_LEGACY( SCN )    \
    ( ( ( SCN ) & SM_SCN_COMMIT_LEGACY_BIT ) != SM_SCN_COMMIT_LEGACY_BIT )
#else
# define SDC_CTS_SCN_IS_COMMITTED( SCN )        \
    ( ((SCN).mLow & SM_SCN_COMMIT_PARITY_BIT ) != SM_SCN_COMMIT_PARITY_BIT )
# define SDC_CTS_SCN_IS_NOT_COMMITTED( SCN )    \
    ( ((SCN).mLow & SM_SCN_COMMIT_PARITY_BIT ) == SM_SCN_COMMIT_PARITY_BIT )
# define SDC_CTS_SCN_IS_LEGACY( SCN )        \
    ( ((SCN).mLow & SM_SCN_COMMIT_LEGACY_BIT ) == SM_SCN_COMMIT_LEGACY_BIT )
# define SDC_CTS_SCN_IS_NOT_LEGACY( SCN )    \
    ( ((SCN).mLow & SM_SCN_COMMIT_LEGACY_BIT ) != SM_SCN_COMMIT_LEGACY_BIT )
#endif

// Direct-Path INSERT�� ���� �ڷ� ����
// Transaction ������ �����ȴ�.
typedef struct sdcDPathEntry
{
    sdbDPathBuffInfo    mDPathBuffInfo;
    sdpDPathInfo        mDPathInfo;
} sdcDPathEntry;

// X$DIRECT_PATH_INSERT�� ����� ���� ��� ���� ����
typedef struct sdcDPathStat
{
    ULong   mCommitTXCnt;
    ULong   mAbortTXCnt;
    ULong   mInsRowCnt;
    ULong   mAllocBuffPageTryCnt;
    ULong   mAllocBuffPageFailCnt;
    ULong   mBulkIOCnt;
} sdcDPathStat;



/**********************************************************************
 * Disk LOB �ڷ� ����
 **********************************************************************/

#define SDC_LOB_INVALID_KEY_SEQ     (-1)
#define SDC_LOB_INVALID_ENTRY_SEQ   (-1)
#define SDC_LOB_MAX_IN_MODE_SIZE    SM_LOB_MAX_IN_ROW_SIZE   // Max In Mode LOB Size

/*
 * LOB Descriptor
 */

#define SDC_LOB_DESC_NULL_MASK   (0x0001)
#define SDC_LOB_DESC_NULL_FALSE  (0x0000)
#define SDC_LOB_DESC_NULL_TRUE   (0x0001)

#define SDC_LOB_MAX_DIRECT_PAGE_CNT (4)

typedef struct sdcLobDesc
{
    ULong       mLobVersion;
    UShort      mLobDescFlag;
    UShort      mLastPageSize;
    UInt        mLastPageSeq;
    UInt        mDirectCnt;
    scPageID    mRootNodePID;
    scPageID    mDirect[SDC_LOB_MAX_DIRECT_PAGE_CNT];
} sdcLobDesc;

/*
 * LOB Meta Page
 */

typedef struct sdcLobMeta
{
    idBool              mLogging;       // Reserved
    idBool              mBuffer;        // Reserved
    UInt                mColumnID;
    sdpDblPIDListBase   mAgingListBase;
} sdcLobMeta;

/*
 * LOB Index & Data Page
 */

typedef struct sdcLobNodeHdr
{
    UShort  mHeight;        // Internal, Leaf Page
    UShort  mKeyCnt;        // Internal, Leaf Page
    UInt    mNodeSeq;       // Internal, Leaf Page
    UShort  mStoreSize;     // Data Page
    UShort  mAlign;         // All
    UInt    mLobPageState;  // All
    ULong   mLobVersion;    // All
    sdSID   mTSSlotSID;     // All
    smSCN   mFstDskViewSCN; // All
} sdcLobNodeHdr;

/* sdcLOBIKey must be aligned. */
typedef struct sdcLobIKey
{
    scPageID    mChild;
} sdcLobIKey;

#define SDC_LOB_MAX_ENTRY_CNT   (8)
 
/* sdcLOBLKey must be aligned. */
typedef struct sdcLobLKey
{
    UShort      mLobVersion;
    UShort      mEntryCnt;
    UInt        mReserved;
    sdSID       mUndoSID;
    scPageID    mEntry[SDC_LOB_MAX_ENTRY_CNT];
} sdcLobLKey;

typedef enum sdcLobPageState
{
    SDC_LOB_UNUSED  = 0,
    SDC_LOB_USED,
    SDC_LOB_AGING_LIST
} sdcLobPageState;

#define SDC_LOB_PAGE_BODY_SIZE  ( sdpPhyPage::getEmptyPageFreeSize() -          \
                                  idlOS::align8(ID_SIZEOF(sdcLobNodeHdr)) )

/*
 * Stuff
 */

typedef struct sdcLobInfo4Fetch
{
    /* LobCursor open mode(read or read write) */
    smiLobCursorMode    mOpenMode;  // LOB Cursor Open Mode
    sdcColInOutMode     mInOutMode; // LOB Data�� In Out ����
} sdcLobInfo4Fetch;

typedef struct sdcColumnInfo4Lob
{
    const smiColumn * mColumn;
    sdcLobDesc      * mLobDesc;
} sdcColumnInfo4Lob;

typedef struct sdcLobInfoInRowPiece
{
    sdcColumnInfo4Lob   mColInfoList[SD_PAGE_SIZE/ID_SIZEOF(sdcLobDesc)];
    UChar               mSpace4CopyLobDesc[SD_PAGE_SIZE];
    UShort              mLobDescCnt;
} sdcLobInfoInRowPiece;

typedef struct sdcLobColBuffer
{   
    UChar           * mBuffer;
    UInt              mLength;
    sdcColInOutMode   mInOutMode;
    idBool            mIsNullLob;
} sdcLobColBuffer;

typedef enum sdcLobWriteType
{
    SDC_LOB_WTYPE_WRITE = 0,
    SDC_LOB_WTYPE_ERASE,
    SDC_LOB_WTYPE_TRIM
} sdcLobWriteType;

typedef enum sdcLobChangeType
{
    SDC_LOB_IN_TO_IN = 0,
    SDC_LOB_IN_TO_OUT,
    SDC_LOB_OUT_TO_OUT,
} sdcLobChangeType;

#define SDC_LOB_STACK_SIZE  (6)

typedef struct sdcLobStack
{
    SInt        mPos;
    scPageID    mStack[SDC_LOB_STACK_SIZE];
} sdcLobStack;

#define SDC_LOB_SET_IKEY( aKey, aChild )     \
{                                            \
    (aKey)->mChild = aChild; \
}

#endif
