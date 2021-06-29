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
 * $Id: sdpsfDef.h 27220 2008-07-23 14:56:22Z newdaily $
 ***********************************************************************/

# ifndef _O_SDPSF_DEF_H_
# define _O_SDPSF_DEF_H_ 1

#define SDPSF_SEGHDR_OFFSET (idlOS::align8((UInt)ID_SIZEOF(sdpPhyPageHdr)))

/* --------------------------------------------------------------------
 * �������� physical header�� ����ȴ�.
 * �������� ���¸� ǥ���Ѵ�.
 * extent desc�� ǥ�õǴ� �������� ���¿� physical header��
 * ǥ�õǴ� �������� ���°� ���� ���·� ������ ���̴�.
 * �̰��� �ٸ��ٸ� �߰��� ������ �׾��� ������ �� �����Ƿ�,
 * ������ ó���ؾ� �Ѵ�.
 * ----------------------------------------------------------------- */
typedef enum
{
    // page�� used�̰� insert ������ ����
    // insertable ���θ� ������� �ʴ� ��������� used�� �ǹ�
    SDPSF_PAGE_USED_INSERTABLE = 0,

    // page�� used�̰� insert �Ұ����� ����
    // ���� update�� �����ϴ�.
    SDPSF_PAGE_USED_UPDATE_ONLY,

    // page�� free�� ����
    SDPSF_PAGE_FREE

} sdpsfPageState;

/* Segment Cache �ڷᱸ�� (Runtime ����) */
typedef struct sdpsfSegCache
{
    sdpSegCCache  mCommon;
} sdpsfSegCache;

//XXX Name�� �����ؾ� ��.
#define SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_MASK  (0x00000001)
#define SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_TRUE  (0x00000001)
#define SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_FALSE (0x00000000)

#define SDP_SF_IS_FST_EXTDIRPAGE_AT_EXT( aFlag ) \
    ( ( aFlag & SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_MASK ) == SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_TRUE )

/* --------------------------------------------------------------------
 * Extent Descriptor�� ����
 * ----------------------------------------------------------------- */
typedef struct sdpsfExtDesc
{
    // �� extent�� ���� ù��° �������� ID
    scPageID           mFstPID;
    // Extent�� ��������: ex) ù��° �������� ExtDirPage�ΰ�?
    UInt               mFlag;
} sdpsfExtDesc;

// Extent Direct Control Header ����
typedef struct sdpsfExtDirCntlHdr
{
    UShort       mExtDescCnt;         // ���������� ExtentDesc ����
    UShort       mMaxExtDescCnt;      // ���������� �ִ� ExtentDesc ����
    scOffset     mFstExtDescOffset;   // ������ ���� ExtentDesc�� ù��° Offset

    /* PROJ-1497 DB Migration�� �����ϱ� ���� Reserved ������ �߰� */
    ULong        mReserveArea[SDP_EXTDIR_PAGEHDR_RESERV_SIZE];
} sdpsfExtDirCntlHdr;

/* --------------------------------------------------------------------
 * Segement Descriptor�� ����
 *
 * extent�� ����Ʈ�� �׻� seg desc�� ���� �ؾ� �Ѵ�.
 * table seg hdr�� ���� �� �� ����.
 * �ֳ��ϸ� table seg hdr�� ���ʿ� �Ҵ�ޱ� ���ؼ���
 * ���� extent�� �Ҵ���� �� �־�� �ϴµ� table seg hdr��
 * extent list�� �ξ��� ��� ���ʿ� extent�� �Ҵ���� ��
 * �ִ� ����� ����.
 * ----------------------------------------------------------------- */
typedef struct sdpsfSegHdr
{
    /* Segment Hdr PageID */
    scPageID           mSegHdrPID;

    /* table or index or undo or tss or temp */
    UShort             mType;

    /* �Ҵ� ���� */
    UShort             mState;

    /* PageCnt In Ext */
    UInt               mPageCntInExt;

    /* Max Extent Cnt In Segment Header */
    UShort             mMaxExtCntInSegHdrPage;
    /* Max Extent Cnt In Extent Directory Page */
    UShort             mMaxExtCntInExtDirPage;

    /* Direct Path Insert�� Segment Merge�� HWM�� �ڷ� �̵��ϴµ�
     * �̶� �߰��� �ִ� Extent�� �ִ� UFmt Page�� �� ����Ʈ�� �߰��Ѵ�.
     * �ֳ��ϸ� DPath Insert�� Merge�� Rollback�� Add�� Free Page��
     * Free�ؾ� �ϴµ� Ager�� ���ÿ� �� ��� UFmtPageList�� ���� ������
     * �߻��Ͽ� Add�� �������� rollback�� ������ �� ����. Commit������
     * �ٸ� Trasnsaction�� �������� �ʴ� Private FreePage List�� �д�.*/
    sdpSglPIDListBase  mPvtFreePIDList;

    /* ������ �� Page List( External, LOB, Multiple Column�� Page��
     * �ѹ��Ҵ� �ǰ� Row�� Free�Ǿ� ������ ��� FreePage List�� �ƴ�
     * Unformated Page List�� ��ȯ�ȴ�. */
    sdpSglPIDListBase  mUFmtPIDList;

    /* Insert�� ������ �ִ� PageList */
    sdpSglPIDListBase  mFreePIDList;

    /* Segment���� �ѹ��̶� �Ҵ�� Page�� ���� */
    ULong              mFmtPageCnt;

    /* Segment���� Format�� ������ �������� ����Ų��. */
    scPageID           mHWMPID;

    /* Meta Page ID Array: Index Seg�� �� PageID Array �� Root
     * Node PID�� ����Ѵ�. */
    scPageID           mArrMetaPID[ SDP_MAX_SEG_PID_CNT ];

    /* Segment���� ���� Page Alloc�� ���� ������� Ext RID */
    sdRID              mAllocExtRID;

    /* Segment���� ���� Page Alloc�� ���� ������� Extent��
     * ù��° PID */
    scPageID           mFstPIDOfAllocExt;


    /** Extent Management�� ���õ� ������ ����Ѵ�. **/
    /* Ext Desc�� Free ����Ʈ */
    sdpDblPIDListBase  mExtDirPIDList;

    /* Segment�� ������ �ִ� Extent�� Total Count */
    ULong              mTotExtCnt;

    /* Segment Header�� ���� ������ ExtDesc�� ����ϱ�
     * ���� ����Ѵ�. */
    sdpsfExtDirCntlHdr mExtDirCntlHdr;
} sdpsfSegHdr;

#endif // _O_SDPSF_DEF_H_
