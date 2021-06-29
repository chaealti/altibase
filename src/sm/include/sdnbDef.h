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
 * $Id: sdnbDef.h 88414 2020-08-25 04:45:02Z justin.kwon $
 **********************************************************************/

#ifndef _O_SDNB_DEF_H_
# define _O_SDNB_DEF_H_ 1

#include <idl.h>
#include <smDef.h>
#include <smnDef.h>
#include <sdnDef.h>

/* BUG-32313: The values of DRDB index cardinality converge on 0 */
#define SDNB_FIND_PREV_SAME_VALUE_KEY   (0)
#define SDNB_FIND_NEXT_SAME_VALUE_KEY   (1)

#define SDNB_SPLIT_POINT_NEW_KEY        ID_USHORT_MAX

/* Proj-1872 Disk Index ���屸�� ����ȭ
 * Ű ���� ������ ���� ����ϴ� �ӽ� ���۵��� ũ��*/
#define SDNB_MAX_LENGTH_KNOWN_COLUMN_SIZE     (SMI_MAX_MINMAX_VALUE_SIZE)
#define SDNB_MAX_KEY_BUFFER_SIZE              (SD_PAGE_SIZE/2)

/* Proj-1872 Disk Index ���屸�� ����ȭ
 * Column�� ǥ���ϱ� ���� ��ũ�ε� 
 *
 * Length-Known Į���� ���, Column Header���� Value�� �����ϸ�
 * Length-Unknown Į���� ���, 250Byte������ Į���� ���ؼ��� 1Byte,
 * �ʰ��� ���ؼ��� 3Byte�� ColumnHeader�� ����ϸ�, �� ColumnHeader
 * �� ColumnValue�� ���̰� ��ϵȴ�*/

#define SDNB_SMALL_COLUMN_VALUE_LENGTH_THRESHOLD (0xFA)

#define SDNB_NULL_COLUMN_PREFIX                  (0xFF)
#define SDNB_LARGE_COLUMN_PREFIX                 (0xFE)

#define SDNB_LARGE_COLUMN_HEADER_LENGTH          (3)
#define SDNB_SMALL_COLUMN_HEADER_LENGTH          (1)

#define SDNB_GET_COLUMN_HEADER_LENGTH( aLen )                           \
    ( ((aLen) > SDNB_SMALL_COLUMN_VALUE_LENGTH_THRESHOLD) ?             \
    SDNB_LARGE_COLUMN_HEADER_LENGTH : SDNB_SMALL_COLUMN_HEADER_LENGTH )

/* sdnbIterator stack                */
#define SDNB_STACK_DEPTH               (128)
#define SDNB_CHANGE_CALLBACK_POS        (6)
#define SDNB_ADJUST_SIZE                (8)
#define SDNB_FREE_NODE_LIMIT            (1)

/* compare flag */
#define SDNB_COMPARE_VALUE     (0x00000001)
#define SDNB_COMPARE_PID       (0x00000002)
#define SDNB_COMPARE_OFFSET    (0x00000004)

/* Key propagation mode : PROJ-1628 */
#define SDNB_SMO_MODE_SPLIT_1_2   (0x00000001)
#define SDNB_SMO_MODE_KEY_REDIST  (0x00000002)

/* Key array �������� Page ���� : PROJ-1628 */
#define SDNB_CURRENT_PAGE                (0)
#define SDNB_NEXT_PAGE                   (1)
#define SDNB_PARENT_PAGE                 (2)
#define SDNB_NO_PAGE                     (3)

#define  SDNB_IS_LEAF_NODE( node )  (                       \
    ((sdnbNodeHdr*)(node))->mLeftmostChild == SD_NULL_PID ? \
    ID_TRUE : ID_FALSE )                                         

/* Proj-1872 Disk index ���� ���� ����ȭ
 * �α��� ���� Į�� Length������ �����ִ� ColLen Info�� ũ�� */
typedef struct sdnbColLenInfoList
{
    UChar          mColumnCount;
    UChar          mColLenInfo[ SMI_MAX_IDX_COLUMNS ];
} sdnbColLenInfoList;

/* Proj-1872 Disk Index ���屸�� ����ȭ
 * ColLenInfoList�� Dump�ϱ� ���� String�� ũ��
 * ex) CREATE TABLE T1 (I INTEGER,C CHAR(40), D DATE)
 * 4,?,8 */
#define SDNB_MAX_COLLENINFOLIST_STR_SIZE               ( SMI_MAX_IDX_COLUMNS * 2 )

#define SDNB_COLLENINFO_LENGTH_UNKNOWN                 (0)
#define SDNB_COLLENINFO_LIST_SIZE(aCollenInfoList)     ((UInt)( (aCollenInfoList).mColumnCount+1 ))



/* Proj-1872 Disk index ���� ���� ����ȭ
 * �Լ��鰣�� Key�� Row�� ������ �����ϱ����� �ӽ� ����ü
 *
 * KeyInfo, ConvertedKeyInfo ������ �и�
 * Proj-1872 Disk index ���屸�� ����ȭ�� ����, ��ũ �ε������� ����ϴ�
 * Ű�� ǥ�� ������ �� �������̴�.
 * 
 *   ConvertedKeyInfo�� KeyInfo�� �����͸� smiValueList���·� ��ȯ�� �����̴�.
 * ���� KeyInfo�� �������� ��� �����Ѵ�. �ϴ�� Compare�� �ʿ��� ���, ��ȯ
 * ����� ���̱����� ���ȴ�.
 *   KeyInfo�� ����Ǵ� Key�� Length-UnknonwnŸ���̳�, Length-KnownŸ���̳Ŀ� 
 * ���� Ű ũ�⸦ �ּ�ȭ��Ų ��ũ �ε��� Ű �����̴�. 
 */
typedef struct sdnbKeyInfo
{
    UChar         *mKeyValue;   // KeyValue Pointer
    scPageID       mRowPID;     // Row PID
    scSlotNum      mRowSlotNum;
    UShort         mKeyState;   // Key�� �����Ǿ���� ����
} sdnbKeyInfo;

typedef struct sdnbConvertedKeyInfo
{
    sdnbKeyInfo    mKeyInfo;
    smiValue      *mSmiValueList;
} sdnbConvertedKeyInfo;

#define SDNB_LKEY_TO_CONVERTED_KEYINFO( aLKey, aConvertedKeyInfo, aColLenInfoList        )        \
        ID_4_BYTE_ASSIGN( &((aConvertedKeyInfo).mKeyInfo.mRowPID),     &((aLKey)->mRowPID)     ); \
        ID_2_BYTE_ASSIGN( &((aConvertedKeyInfo).mKeyInfo.mRowSlotNum), &((aLKey)->mRowSlotNum) ); \
        (aConvertedKeyInfo).mKeyInfo.mKeyState = SDNB_GET_STATE( aLKey );                         \
        (aConvertedKeyInfo).mKeyInfo.mKeyValue = SDNB_LKEY_KEY_PTR( aLKey );                      \
        sdnbBTree::makeSmiValueListFromKeyValue(                                                  \
            aColLenInfoList,                                                                      \
            (aConvertedKeyInfo).mKeyInfo.mKeyValue, (aConvertedKeyInfo).mSmiValueList );

#define SDNB_KEYINFO_TO_CONVERTED_KEYINFO( aKeyInfo, aConvertedKeyInfo, aColLenInfoList )           \
        (aConvertedKeyInfo).mKeyInfo    = aKeyInfo;                                                 \
        sdnbBTree::makeSmiValueListFromKeyValue(                                                    \
            aColLenInfoList, (aKeyInfo).mKeyValue, (aConvertedKeyInfo).mSmiValueList );

#define SDNB_GET_COLLENINFO( aKeyColumn )   \
        ( ( ( (aKeyColumn)->flag & SMI_COLUMN_LENGTH_TYPE_MASK) == SMI_COLUMN_LENGTH_TYPE_KNOWN ) ? \
          ( ( aKeyColumn )->size ) : /*LENGTH_KNOWN   */                                            \
          0 )                        /*LENGTH_UNKNOWN */
                         

#define SDNB_KEY_TO_SMIVALUEINFO( aKeyColumn, aKeyPtr, aColumnHeaderLength, aSmiValueInfo)     \
        IDE_DASSERT( ID_SIZEOF( *(aKeyPtr) ) == 1 );                                           \
        (aSmiValueInfo)->column = aKeyColumn;                                                  \
        aKeyPtr += sdnbBTree::getColumnLength(                                                 \
                                    SDNB_GET_COLLENINFO( aKeyColumn ),                         \
                                    aKeyPtr,                                                   \
                                    &sColumnHeaderLength,                                      \
                                    &(aSmiValueInfo)->length,                                  \
                                    &(aSmiValueInfo)->value);

// index segment ù��° �������� �ִ� persistent information
// runtime header �����ÿ� �̰��� �����ؼ� min, max�� ����
typedef struct sdnbMeta
{
    scPageID        mRootNode;
    scPageID        mEmptyNodeHead;
    scPageID        mEmptyNodeTail;

    scPageID        mMinNode;
    scPageID        mMaxNode;

    // FOR PROJ-1469
    // mIsCreatedWithLogging : index build�� logging mode, FALSE : Nologging
    // mNologgingCompletionLSN : index build���������� LSN
    // mIsConsistent : nologging index build���� consistent ǥ�ø� ����
    // mIsCreatedWithForce : nologging force mode, TRUE : Force
    idBool          mIsCreatedWithLogging;
    smLSN           mNologgingCompletionLSN;
    idBool          mIsConsistent;
    idBool          mIsCreatedWithForce;

    ULong           mFreeNodeCnt;
    scPageID        mFreeNodeHead;
}sdnbMeta;

typedef struct sdnbIKey
{
    scPageID       mRightChild;
    scPageID       mRowPID;     // right child�� ù��° Ű�� mRowPID
    scSlotNum      mRowSlotNum; // right child�� ù��° Ű�� mRowSlotNum
} sdnbIKey;

#define SDNB_IKEY_HEADER_LEN ( ID_SIZEOF(scPageID) + ID_SIZEOF(scPageID) + ID_SIZEOF(scSlotNum) ) 

#define SDNB_IKEY_LEN( aKeyLen )                                            \
    (SDNB_IKEY_HEADER_LEN + (aKeyLen))

#define SDNB_IKEY_KEY_PTR( aSlotPtr )                                       \
    ((UChar*)aSlotPtr + SDNB_IKEY_HEADER_LEN)

#define SDNB_KEYINFO_TO_IKEY( aKeyInfo, aRightChild, aKeyLength, aIKey )                      \
        ID_4_BYTE_ASSIGN( &((aIKey)->mRightChild),          &aRightChild );                   \
        ID_4_BYTE_ASSIGN( &((aIKey)->mRowPID),              &((aKeyInfo).mRowPID) );          \
        ID_2_BYTE_ASSIGN( &((aIKey)->mRowSlotNum),          &((aKeyInfo).mRowSlotNum) );      \
        idlOS::memcpy( SDNB_IKEY_KEY_PTR( aIKey ), (aKeyInfo).mKeyValue, aKeyLength   );           

#define SDNB_IKEY_TO_KEYINFO( aIKey, aKeyInfo )                                 \
        ID_4_BYTE_ASSIGN( &((aKeyInfo).mRowPID),     &((aIKey)->mRowPID) );     \
        ID_2_BYTE_ASSIGN( &((aKeyInfo).mRowSlotNum), &((aIKey)->mRowSlotNum) ); \
        (aKeyInfo).mKeyValue = SDNB_IKEY_KEY_PTR( aIKey );

#define SDNB_GET_RIGHT_CHILD_PID( aChildPID, aIKey )                         \
        ID_4_BYTE_ASSIGN( aChildPID, &((aIKey)->mRightChild) ); 



typedef struct sdnbLKey
{
    // ���� ���� ������ key���� mRowPID, mRowSlotNum���� ��
    scPageID    mRowPID;
    scSlotNum   mRowSlotNum;
    UChar       mTxInfo[2]; /* = { createCTS   (6bit),  // Create CTS No.
                             *     state       (2bit),  // Key ����
                             *     limitCTS    (6bit),  // Limit CTS No.
                             *     duplicated  (1bit),  // Duplicated Key ���� ����.
                             *     txBoundType (1bit) } // Key Ÿ��( SDNB_KEY_TB_CTS, SDNB_KEY_TB_KEY )
                             */
} sdnbLKey;

typedef struct sdnbLTxInfo
{
    smSCN mCreateCSCN;     // Commit SCN or Begin SCN
    sdSID mCreateTSS;
    smSCN mLimitCSCN;
    sdSID mLimitTSS;
} sdnbLTxInfo;

#define SDNB_DUPKEY_NO        (0)  // 'N'
#define SDNB_DUPKEY_YES       (1)  // 'Y'

#define SDNB_CACHE_VISIBLE_UNKNOWN    (0)
#define SDNB_CACHE_VISIBLE_YES        (1)

#define SDNB_KEY_UNSTABLE     (0)  // 'U'
#define SDNB_KEY_STABLE       (1)  // 'S'
#define SDNB_KEY_DELETED      (2)  // 'd'
#define SDNB_KEY_DEAD         (3)  // 'D'

#define SDNB_KEY_TB_CTS       (0)  // 'T'
#define SDNB_KEY_TB_KEY       (1)  // 'K'

// PROJ-1872 DiskIndex ���屸�� ����ȭ
// Bottom-up Build�� LKey ���·� Ű�� �����Ѵ�.
#define SDNB_WRITE_LKEY( aRowPID, aRowSlotNum, aKey, aKeyLength, aLKey )      \
        ID_4_BYTE_ASSIGN( &((aLKey)->mRowPID),              &(aRowPID) );     \
        ID_2_BYTE_ASSIGN( &((aLKey)->mRowSlotNum),          &(aRowSlotNum) ); \
        SDNB_SET_TB_TYPE( (aLKey),                          SDNB_KEY_TB_CTS );\
        idlOS::memcpy( SDNB_LKEY_KEY_PTR( aLKey ), aKey, aKeyLength  ); 

#define SDNB_KEYINFO_TO_LKEY( aKeyInfo, aKeyLength, aLKey,                              \
                              aCCTS, aDup, aLCTS, aState, aTbType )                     \
        ID_4_BYTE_ASSIGN( &((aLKey)->mRowPID),              &((aKeyInfo).mRowPID) );    \
        ID_2_BYTE_ASSIGN( &((aLKey)->mRowSlotNum),          &((aKeyInfo).mRowSlotNum) );\
        SDNB_SET_CCTS_NO( aLKey, aCCTS );                                               \
        SDNB_SET_DUPLICATED( aLKey, aDup );                                             \
        SDNB_SET_LCTS_NO( aLKey, aLCTS );                                               \
        SDNB_SET_STATE( aLKey, aState );                                                \
        SDNB_SET_TB_TYPE( aLKey, aTbType );                                             \
        idlOS::memcpy( SDNB_LKEY_KEY_PTR( aLKey ), (aKeyInfo).mKeyValue, aKeyLength ); 

#define SDNB_LKEY_TO_KEYINFO( aLKey, aKeyInfo )                                 \
        ID_4_BYTE_ASSIGN( &((aKeyInfo).mRowPID),     &((aLKey)->mRowPID)     ); \
        ID_2_BYTE_ASSIGN( &((aKeyInfo).mRowSlotNum), &((aLKey)->mRowSlotNum) ); \
        (aKeyInfo).mKeyState = SDNB_GET_STATE( aLKey );                         \
        (aKeyInfo).mKeyValue = SDNB_LKEY_KEY_PTR( aLKey );

#define SDNB_EQUAL_PID_AND_SLOTNUM( aLKey, aKeyInfo )                                                        \
        (idlOS::memcmp( &((aLKey)->mRowSlotNum),  &((aKeyInfo)->mRowSlotNum), ID_SIZEOF(scSlotNum) )==0 &&   \
         idlOS::memcmp( &((aLKey)->mRowPID),      &((aKeyInfo)->mRowPID),     ID_SIZEOF(scPageID)  )==0) 

/*
 * BUG-25226: [5.3.1 SN/SD] 30�� �̻��� Ʈ����ǿ��� ���� �̹ݿ�
 *            ���ڵ尡 1�� �̻��� ���, allocCTS�� �����մϴ�.
 */
typedef struct sdnbLKeyEx
{
    sdnbLKey    mSlot;
    sdnbLTxInfo mTxInfoEx;
} sdnbLKeyEx;

// PROJ-1704 MVCC Renewal
#define SDNB_GET_CCTS_NO( aKey )       \
    ((0xFC & (aKey)->mTxInfo[0]) >> 2)

#define SDNB_GET_STATE( aKey )         \
    ((0x03 & (aKey)->mTxInfo[0]))

#define SDNB_GET_LCTS_NO( aKey )       \
    ((0xFC & (aKey)->mTxInfo[1]) >> 2)

#define SDNB_GET_DUPLICATED( aKey )    \
    ((0x02 & (aKey)->mTxInfo[1]) >> 1)

#define SDNB_GET_TB_TYPE( aKey )       \
    ((0x01 & (aKey)->mTxInfo[1]))

#define SDNB_GET_TBK_CSCN( aKey, aValue )       \
    idlOS::memcpy( (aValue), &((aKey)->mTxInfoEx.mCreateCSCN), ID_SIZEOF( smSCN ) )
#define SDNB_GET_TBK_CTSS( aKey, aValue )       \
    idlOS::memcpy( (aValue), &((aKey)->mTxInfoEx.mCreateTSS), ID_SIZEOF( sdSID ) )
#define SDNB_GET_TBK_LSCN( aKey, aValue )       \
    idlOS::memcpy( (aValue), &((aKey)->mTxInfoEx.mLimitCSCN), ID_SIZEOF( smSCN ) )
#define SDNB_GET_TBK_LTSS( aKey, aValue )       \
    idlOS::memcpy( (aValue), &((aKey)->mTxInfoEx.mLimitTSS), ID_SIZEOF( sdSID ) )

#define SDNB_LKEY_LEN( aKeyLen, aTxBoundType ) \
    ( ( (aTxBoundType) == SDNB_KEY_TB_CTS ) ?   \
      ( (aKeyLen) + ID_SIZEOF(sdnbLKey) ) :    \
      ( (aKeyLen) + ID_SIZEOF(sdnbLKeyEx) ) )

#define SDNB_LKEY_KEY_PTR( aSlotPtr )                                          \
    ( ( SDNB_GET_TB_TYPE( ((sdnbLKey*)aSlotPtr) ) == SDNB_KEY_TB_CTS ) ?       \
      ( ((UChar*)aSlotPtr) + ID_SIZEOF(sdnbLKey) ) :                           \
      ( ((UChar*)aSlotPtr) + ID_SIZEOF(sdnbLKeyEx) ) )

#define SDNB_LKEY_HEADER_LEN( aSlotPtr )                                       \
    ( ( SDNB_GET_TB_TYPE( ((sdnbLKey*)aSlotPtr) ) == SDNB_KEY_TB_CTS ) ?       \
      ( ID_SIZEOF(sdnbLKey) ) :                                                \
      ( ID_SIZEOF(sdnbLKeyEx) ) )

#define SDNB_LKEY_HEADER_LEN_EX ( ID_SIZEOF(sdnbLKeyEx) )

#define SDNB_CREATE_CTS_UNMASK   ((UChar)(0x03)) // [1111 1100] [0000 0000]
#define SDNB_STATE_UNMASK        ((UChar)(0xFC)) // [0000 0011] [0000 0000]
#define SDNB_LIMIT_CTS_UNMASK    ((UChar)(0x03)) // [0000 0000] [1111 1100]
#define SDNB_DUPLICATED_UNMASK   ((UChar)(0xFD)) // [0000 0000] [0000 0010]
#define SDNB_TB_UNMASK           ((UChar)(0xFE)) // [0000 0000] [0000 0001]

#define SDNB_SET_CCTS_NO( aKey, aValue )          \
{                                                 \
    (aKey)->mTxInfo[0] &= SDNB_CREATE_CTS_UNMASK; \
    (aKey)->mTxInfo[0] |= ((UChar)aValue << 2);   \
}
#define SDNB_SET_STATE( aKey, aValue )          \
{                                               \
    (aKey)->mTxInfo[0] &= SDNB_STATE_UNMASK;    \
    (aKey)->mTxInfo[0] |= (UChar)aValue; \
}
#define SDNB_SET_LCTS_NO( aKey, aValue )          \
{                                                 \
    (aKey)->mTxInfo[1] &= SDNB_LIMIT_CTS_UNMASK;  \
    (aKey)->mTxInfo[1] |= ((UChar)aValue << 2);   \
}
#define SDNB_SET_DUPLICATED( aKey, aValue )       \
{                                                 \
    (aKey)->mTxInfo[1] &= SDNB_DUPLICATED_UNMASK; \
    (aKey)->mTxInfo[1] |= ((UChar)aValue << 1);   \
}
#define SDNB_SET_TB_TYPE( aKey, aValue )    \
{                                           \
    (aKey)->mTxInfo[1] &= SDNB_TB_UNMASK;   \
    (aKey)->mTxInfo[1] |= (UChar)aValue;    \
}

#define SDNB_SET_TBK_CSCN( aKey, aValue )       \
    idlOS::memcpy( &((aKey)->mTxInfoEx.mCreateCSCN), (aValue), ID_SIZEOF( smSCN ) )
#define SDNB_SET_TBK_CTSS( aKey, aValue )       \
    idlOS::memcpy( &((aKey)->mTxInfoEx.mCreateTSS), (aValue), ID_SIZEOF( sdSID ) )
#define SDNB_SET_TBK_LSCN( aKey, aValue )       \
    idlOS::memcpy( &((aKey)->mTxInfoEx.mLimitCSCN), (aValue), ID_SIZEOF( smSCN ) )
#define SDNB_SET_TBK_LTSS( aKey, aValue )       \
    idlOS::memcpy( &((aKey)->mTxInfoEx.mLimitTSS), (aValue), ID_SIZEOF( sdSID ) )

typedef struct sdnbRollbackContext
{
    smOID mTableOID;
    UInt  mIndexID;
    UChar mTxInfo[2];
} sdnbRollbackContext;

typedef struct sdnbRollbackContextEx
{
    sdnbRollbackContext mRollbackContext;
    sdnbLTxInfo         mTxInfoEx;
} sdnbRollbackContextEx;

// PROJ-1595
// for merge sorted block
typedef struct sdnbMergeBlkInfo
{
    sdnbKeyInfo   *mMinKeyInfo;
    sdnbKeyInfo   *mMaxKeyInfo;
} sdnbMergeBlkInfo;

typedef struct sdnbFreePageLst
{
    scPageID                  mPageID;
    struct sdnbFreePageLst *  mNext;
} sdnbFreePageLst;

typedef struct sdnbSortedBlk
{
    scPageID           mStartPID;
} sdnbSortedBlk;

typedef struct sdnbLKeyHeapNode
{
    sdRID              mExtRID;
    scPageID           mHeadPID;
    scPageID           mCurrPID;
    scPageID           mNextPID;
    UInt               mSlotSeq;
    UInt               mSlotCount;
} sdnbLKeyHeapNode;

typedef struct sdnbMergeRunInfo
{
    scPageID           mHeadPID;
    UInt               mSlotCnt;
    UInt               mSlotSeq;
    sdpPhyPageHdr    * mPageHdr;
    sdnbKeyInfo        mRunKey;
} sdnbMergeRunInfo;

#define SDNB_IN_INIT            (0)   // 'I' /* ������ init ���� */
#define SDNB_IN_USED            (1)   // 'U' /* Ű�� ���ԵǾ� �ִ� ���� */
#define SDNB_IN_EMPTY_LIST      (2)   // 'E' /* ������ Ű�� �����ϴ� ���� */
#define SDNB_IN_FREE_LIST       (3)   // 'F' /* DEAD Ű�� �����ϴ� ���� */

#define SDNB_TBK_MAX_CACHE      (2)
#define SDNB_TBK_CACHE_NULL     (0xFFFF)

typedef struct sdnbNodeHdr
{
    scPageID    mLeftmostChild;     /* ù��° �ڽ�Node�� PID. Leaf Node�� �׻� 0. (�̰����� Internal/Leaf ������) */
    scPageID    mNextEmptyNode;     /* Node�� empty node list�� �޷����� ��� */
    UShort      mHeight;            /* Leaf Node�̸� 0, �� node�� ������ 1,2,3,... ���� */
    UShort      mUnlimitedKeyCount; /* key ���°� SDNB_KEY_UNSTABLE �Ǵ� SDNB_KEY_STABLE �� key�� ���� */
    UShort      mTotalDeadKeySize;  /* key ���°� SDNB_KEY_DEAD �� key�� ������ �հ� */
    UShort      mTBKCount;          /* Node�� ���Ե� TBK ���� */
    UShort      mDummy[SDNB_TBK_MAX_CACHE]; /* BUG-44973 ������� ���� */
    UChar       mState;             /* Node�� ���� ( SDNB_IN_USED, SDNB_IN_EMPTY_LIST, SDNB_IN_FREE_LIST ) */
    UChar       mPadding[3];
} sdnbNodeHdr;

typedef struct sdnbColumn
{
    smiCompareFunc                mCompareKeyAndKey;
    smiCompareFunc                mCompareKeyAndVRow;
    smiKey2StringFunc             mKey2String;      // PROJ-1618

    smiCopyDiskColumnValueFunc    mCopyDiskColumnFunc;
    smiIsNullFunc                 mIsNull;
    /* PROJ-1872 Disk Index ���屸�� ����ȭ
     * MakeKeyFromValueRow��, Row�� Length-KnownŸ���� Null�� 1Byte�� ����
     * �Ͽ� ǥ���ϱ� ������ NullValue�� ���� ���Ѵ�. ���� �� Null�� ����
     * ���� ���� mNull �Լ��� �����Ѵ�. */
    smiNullFunc                   mNull;

    /*BUG-24449 Ű���� Header���̰� �ٸ� �� ���� */
    UInt                          mMtdHeaderLength;

    smiColumn                     mKeyColumn;       // Key������ column info
    smiColumn                     mVRowColumn;      // fetch�� Row�� column info
}sdnbColumn;

#define SDNB_MIN_KEY_VALUE_LENGTH    (1)
#define SDNB_MAX_ROW_CACHE  \
   ( (SD_PAGE_SIZE - ID_SIZEOF(sdpPhyPageHdr) - ID_SIZEOF(sdnbNodeHdr) - ID_SIZEOF(sdpPageFooter) - ID_SIZEOF(sdpSlotEntry)) / \
    (ID_SIZEOF(sdnbLKey) + SDNB_MIN_KEY_VALUE_LENGTH)) 

typedef struct sdnbPageStat
{
    ULong   mGetPageCount;
    ULong   mReadPageCount;
} sdnbPageStat;

// PROJ-1617
typedef struct sdnbStatistic
{
    ULong        mKeyCompareCount;
    ULong        mKeyValidationCount;
    ULong        mNodeRetryCount;
    ULong        mRetraverseCount;
    ULong        mOpRetryCount;
    ULong        mPessimisticCount;
    ULong        mNodeSplitCount;
    ULong        mNodeDeleteCount;
    ULong        mBackwardScanCount;
    sdnbPageStat mIndexPage;
    sdnbPageStat mMetaPage;
} sdnbStatistic;

#define SDNB_ADD_PAGE_STATISTIC( dest, src ) \
{                                                      \
    (dest)->mGetPageCount  += (src)->mGetPageCount;    \
    (dest)->mReadPageCount += (src)->mReadPageCount;   \
}

// BUG-18201 : Memory/Disk Index ���ġ
#define SDNB_ADD_STATISTIC( dest, src ) \
{                                                                           \
    (dest)->mKeyCompareCount    += (src)->mKeyCompareCount;                 \
    (dest)->mKeyValidationCount += (src)->mKeyValidationCount;              \
    (dest)->mNodeRetryCount     += (src)->mNodeRetryCount;                  \
    (dest)->mRetraverseCount    += (src)->mRetraverseCount;                 \
    (dest)->mOpRetryCount       += (src)->mOpRetryCount;                    \
    (dest)->mPessimisticCount   += (src)->mPessimisticCount;                \
    (dest)->mNodeSplitCount     += (src)->mNodeSplitCount;                  \
    (dest)->mNodeDeleteCount    += (src)->mNodeDeleteCount;                 \
    (dest)->mBackwardScanCount  += (src)->mBackwardScanCount;               \
    SDNB_ADD_PAGE_STATISTIC( &((dest)->mIndexPage), &((src)->mIndexPage) ); \
    SDNB_ADD_PAGE_STATISTIC( &((dest)->mMetaPage), &((src)->mMetaPage) );   \
}

/* BUG-31845 [sm-disk-index] Debugging information is needed for 
 * PBT when fail to check visibility using DRDB Index.
 * �� �ڷᱸ���� ����ɰ��, sdnbBTree::dumpRuntimeHeader
 * ���� �����Ǿ�� �մϴ�. */
typedef struct sdnbHeader
{
    SDN_RUNTIME_PARAMETERS

    scPageID             mRootNode;
    scPageID             mEmptyNodeHead;
    scPageID             mEmptyNodeTail;
    scPageID             mMinNode;
    scPageID             mMaxNode;

    ULong                mFreeNodeCnt;
    scPageID             mFreeNodeHead;

    /* �� ���鿡 ���� Mtx Rollback�� �Ͼ���� �����ϱ� ���� ����� */
    scPageID             mRootNode4MtxRollback;
    scPageID             mEmptyNodeHead4MtxRollback;
    scPageID             mEmptyNodeTail4MtxRollback;
    scPageID             mMinNode4MtxRollback;
    scPageID             mMaxNode4MtxRollback;
    ULong                mFreeNodeCnt4MtxRollback;
    scPageID             mFreeNodeHead4MtxRollback;


    // PROJ-1617 STMT�� AGER�� ���� ������� ����
    sdnbStatistic        mDMLStat;
    sdnbStatistic        mQueryStat;

    /* Proj-1827
     * ���� �ε��� ���� ������ �ִ��� ����ȿ���� ���̱� ���� 
     * �ε��� ��Ÿ�� ����� ������ �̿��ϵ��� ����Ǿ� �ִ�.
     * �ε��� �α� ���������� ��Ÿ�� ��� ������ �˼� ���� 
     * ������ Ű ���̸� ����� �� ����.���� �ε��� Ű ����
     * �� ����ϱ� ���� ColLenInfo�� �����ؾ� �Ѵ�. */
    sdnbColLenInfoList   mColLenInfoList;
    
    sdnbColumn         * mColumnFence;
    sdnbColumn         * mColumns; //fix BUG-22840

    /* BUG-22946 
     * index key�� insert�Ϸ��� row�κ��� index key column value��
     * fetch�ؿ;� �Ѵ�.
     *   row�κ��� column ������ fetch�Ϸ��� fetch column list�� ����
     * �־�� �ϴµ�, insert key insert �Ҷ����� fetch column list��
     * �����ϸ� �������ϰ� �߻��Ѵ�.
     *   �׷��� create index�ÿ� fetch column list�� �����ϰ� sdnb-
     * Header�� �Ŵ޾Ƶε��� �����Ѵ�. */
    smiFetchColumnList * mFetchColumnListToMakeKey;

    /* CommonPersistentHeader�� Alter���� ���� ��� �ٲ�� ������,
     * Pointer�� ��ũ�� �ɾ�� �� ����. */

    // TODO: ���� SdnHeader�� �ֱ�
    UInt                 mSmoNoAtomicA;
    UInt                 mSmoNoAtomicB;
} sdnbHeader;

typedef struct sdnbCallbackContext
{
    sdnbStatistic * mStatistics;
    sdnbHeader    * mIndex;
    sdnbLKey      * mLeafKey;
} sdnbCallbackContext;

/* PROJ-1628 */
typedef struct sdnbKeyArray
{
    UShort           mLength;
    UShort           mPage;
    UShort           mSeq;
} sdnbKeyArray;

/* PROJ-1628���� mNextNode�� mSlotSize�� �߰���. ������ �׻� ������
   Optimistic������ ������ ������, Pessimistic������ ����.
*/
typedef struct sdnbStackSlot
{
    scPageID         mNode;
    scPageID         mNextNode;
    ULong            mSmoNo;
    SShort           mKeyMapSeq;
    UShort           mNextSlotSize;
} sdnbStackSlot;

typedef struct sdnbStack
{
    SInt            mIndexDepth;
    SInt            mCurrentDepth;
    idBool          mIsSameValueInSiblingNodes;
    sdnbStackSlot   mStack[SDNB_STACK_DEPTH];
} sdnbStack;

typedef struct sdnbRowCache
{
    scPageID   mRowPID;
    scSlotNum  mRowSlotNum;
    scOffset   mOffset;  // leaf node�������� slot offset
} sdnbRowCache;

/* BUG-31845 [sm-disk-index] Debugging information is needed for 
 * PBT when fail to check visibility using DRDB Index.
 * �� �ڷᱸ���� ����ɰ��, sdnbBTree::dumpIterator
 * ���� �����Ǿ�� �մϴ�. */
typedef struct sdnbIterator
{
    smSCN       mSCN;
    smSCN       mInfinite;
    void*       mTrans;
    void*       mTable;
    SChar*      mCurRecPtr;  // MRDB scan module������ �����ؼ� ������ ����?
    SChar*      mLstFetchRecPtr;
    scGRID      mRowGRID;
    smTID       mTID;
    UInt        mFlag;

    smiCursorProperties  * mProperties;
    smiStatement         * mStatement;
    /* smiIterator ���� ���� �� */

    void*               mIndex;
    const smiRange *    mKeyRange;
    const smiRange *    mKeyFilter;
    const smiCallBack * mRowFilter;

    /* BUG-32017 [sm-disk-index] Full index scan in DRDB */
    idBool              mFullIndexScan;
    scPageID            mCurLeafNode;
    scPageID            mNextLeafNode;
    scPageID            mScanBackNode;
    ULong               mCurNodeSmoNo;
    ULong               mIndexSmoNo;
    idBool              mIsLastNodeInRange;
    smLSN               mCurNodeLSN;

    sdnbRowCache *      mCurRowPtr;
    sdnbRowCache *      mCacheFence;
    sdnbRowCache        mRowCache[SDNB_MAX_ROW_CACHE];
    sdnbLKey *          mPrevKey;
    // BUG-18557
    ULong               mAlignedPage[SD_PAGE_SIZE/ID_SIZEOF(ULong)];
    UChar *             mPage;
} sdnbIterator;

/*  TASK-4990 changing the method of collecting index statistics
 * ���� ������� ���� ��� */
typedef struct sdnbStatArgument
{
    idvSQL                        * mStatistics;
    sdbMPRFilter4SamplingAndPScan   mFilter4Scan;
    sdnbHeader                    * mRuntimeIndexHeader;

    /* OutParameter */
    IDE_RC  mResult;
    SLong   mClusteringFactor;  /* ClusteringFactor */
    SLong   mNumDist;           /* NumberOfDistinctValue(Cardinality) */
    SLong   mKeyCount;          /* �м��� Ű ���� */
    UInt    mAnalyzedPageCount; /* ������ �м��� ������ ���� */
    UInt    mSampledPageCount;  /* Sample�� ������ ������ ���� */

    SLong   mMetaSpace;     /* PageHeader, ExtDir�� Meta ���� */
    SLong   mUsedSpace;     /* ���� ������� ���� */
    SLong   mAgableSpace;   /* ���߿� Aging������ ���� */
    SLong   mFreeSpace;     /* Data������ ������ �� ���� */
} sdnbStatArgument;


/* BUG-44973 TBK STAMPING LOGGING�� */
typedef struct sdnbTBKStamping
{
    scOffset mSlotOffset; /* TBK Stamping�� key�� slot offset */
    UChar    mCSCN;       /* CreateCSCN�� ���� TBK Stamping ��(1) */
    UChar    mLSCN;       /* LimitCSCN�� ���� TBK Stamping ��(1) */
} sdnbTBKStamping;

#define SDNB_IS_TBK_STAMPING_WITH_CSCN( aTBKStamping )  ( ( (UChar)((aTBKStamping)->mCSCN) == ((UChar)1) ) ? ID_TRUE : ID_FALSE )
#define SDNB_IS_TBK_STAMPING_WITH_LSCN( aTBKStamping )  ( ( (UChar)((aTBKStamping)->mLSCN) == ((UChar)1) ) ? ID_TRUE : ID_FALSE )
#define SDNB_SET_TBK_STAMPING( aTBKStamping, aSlotOffset, aIsCSCN, aIsLSCN )            \
{                                                                                       \
    (aTBKStamping)->mSlotOffset = (scOffset)aSlotOffset;                                \
    (aTBKStamping)->mCSCN       = ( ( aIsCSCN == ID_TRUE ) ? ((UChar)1) : ((UChar)0) ); \
    (aTBKStamping)->mLSCN       = ( ( aIsLSCN == ID_TRUE ) ? ((UChar)1) : ((UChar)0) ); \
}

#endif /* _O_SDNB_DEF_H_ */
