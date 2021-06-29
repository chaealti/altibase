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
 * $Id: sdnDef.h 29386 2008-11-18 06:52:08Z upinel9 $
 **********************************************************************/

#ifndef _O_SDN_DEF_H_
# define _O_SDN_DEF_H_ 1

# include <idl.h>
# include <idpULong.h>
# include <idpUInt.h>
# include <smDef.h>
# include <smnDef.h>

#define SDN_CTS_MAX_KEY_CACHE  (3) /* BUG-48064 : CTS�� KEY CACHE ���� 4 -> 3 �� ���� */
#define SDN_CTS_KEY_CACHE_NULL (0)


#define SDN_RUNTIME_PARAMETERS                                           \
    SMN_RUNTIME_PARAMETERS                                               \
                                                                         \
    iduLatch       mLatch; /* FOR SMO Operation */                       \
                                                                         \
    scSpaceID      mTableTSID;                                           \
    scSpaceID      mIndexTSID;                                           \
    sdRID          mMetaRID;                                             \
    smOID          mTableOID;                                            \
    UInt           mIndexID;                                             \
                                                                         \
    /*  PROJ-1671 Bitmap-based Tablespace And Segment Space Management*/ \
    /* Segment Handle : Segment RID �� Semgnet Cache */                  \
    sdpSegmentDesc mSegmentDesc;                                         \
                                                                         \
    ULong          mSmoNo; /* �ý��� startup�ÿ� 1�� ����(0 �ƴ�)  */    \
                           /* 0���� ���� startup�ÿ� node�� ��ϵ� */    \
                           /* SmoNo�� reset�ϴµ� ����           */    \
                                                                         \
    idBool         mIsUnique;                                            \
    idBool         mIsNotNull; /*PK�� NULL�� ������ ����(BUG-17762).*/   \
    idBool         mLogging;                                             \
    /* BUG-17957 */                                                      \
    /* index run-time header�� creation option(logging, force) �߰�*/    \
    idBool              mIsCreatedWithLogging;                           \
    idBool              mIsCreatedWithForce;                             \
                                                                         \
    smLSN               mCompletionLSN;

 /* BUG-25279     Btree for spatial�� Disk Btree�� �ڷᱸ�� �� �α� �и� 
  * Btree For Spatial�� �Ϲ� Disk Btree�� ���屸���� �и��ȴ�. ������ Disk Index�ν�
  * ���������� ������ �ϴ� ��Ÿ�� ��� �׸��� �����Ѵ�. �̸� ������ ���� �����Ѵ�. */
typedef struct sdnRuntimeHeader
{
    SDN_RUNTIME_PARAMETERS
} sdnRuntimeHeader;

/* BUG-48064
   CTS CHANING ������ŷ� ���ú��� �����ϰ� CTS KEY CACHE�� 4->3���� �ٿ���,
   CTS ����ü ũ�� �۾���. (40->24bytes) */
typedef struct sdnCTS
{
    smSCN       mCommitSCN;     // Commit SCN or Begin SCN
    scPageID    mTSSlotPID;     // TSS page id
    scSlotNum   mTSSlotNum;     // TSS slotnum 
    UChar       mState;         // CTS State
    UChar       mDummy;
    UShort      mRefKey[ SDN_CTS_MAX_KEY_CACHE ];
    UShort      mRefCnt;        /* �� CTS�� �����ϴ� key�� ���� */
} sdnCTS;

typedef struct sdnCTLayerHdr
{
    UChar       mCount;
    UChar       mUsedCount;
    UChar       mDummy[6];
} sdnCTLayerHdr;

typedef struct sdnCTL
{
    UChar       mCount;
    UChar       mUsedCount;
    UChar       mDummy[6];
    sdnCTS      mArrCTS[1];
} sdnCTL;

#define SDN_CTS_NONE          (0)  // 'N': �ʱ� ����
#define SDN_CTS_UNCOMMITTED   (1)  // 'U': �������� ���� ���� ����(Ŀ�� ��Ȯ��)
#define SDN_CTS_STAMPED       (2)  // 'S': ���������� ����(Ŀ�� ����)
#define SDN_CTS_DEAD          (3)  // 'D': ���̻� ������ �ʴ� ����

#define SDN_CTS_INFINITE     ((UChar)0x3F) /* 63 */
#define SDN_CTS_IN_KEY       ((UChar)0x3E) /* 62 */

#define SDN_IS_VALID_CTS( aCTS ) ( (aCTS != SDN_CTS_INFINITE) && (aCTS != SDN_CTS_IN_KEY) )

/* BUG-24091
 * [SD-����߰�] vrow column ���鶧 ���ϴ� ũ�⸸ŭ�� �����ϴ� ��� �߰� */
#define SDN_FETCH_SIZE_UNLIMITED    ID_UINT_MAX

typedef IDE_RC (*sdnSoftKeyStamping)( sdrMtx        * aMtx,
                                      sdpPhyPageHdr * aPage,
                                      UChar           aCTSlotNum,
                                      UChar         * aContxt);

typedef IDE_RC (*sdnHardKeyStamping)( idvSQL        * aStatistics,
                                      sdrMtx        * aMtx,
                                      sdpPhyPageHdr * aPage,
                                      UChar           aCTSlotNum,
                                      UChar         * aContxt,
                                      idBool        * aSuccess );

typedef struct sdnCallbackFuncs
{
    sdnSoftKeyStamping          mSoftKeyStamping;
    sdnHardKeyStamping          mHardKeyStamping;
} sdnCallbackFuncs;

#endif /* _O_SDN_DEF_H_ */
