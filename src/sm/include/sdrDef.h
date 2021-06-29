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
 * $Id: sdrDef.h 88414 2020-08-25 04:45:02Z justin.kwon $
 *
 * Description :
 *
 * �� ������ recovery layer�� disk ���� ��������̴�.
 *
 **********************************************************************/
#ifndef _O_SDR_DEF_H_
#define _O_SDR_DEF_H_ 1

#include <sddDef.h>
#include <sdbDef.h>
#include <smuHash.h>
#include <smuDynArray.h>

// BUG-13972
// sdrMtxStackInfo�� size�� 128�� ���� �� ������ �Ѵ�.
// �� ���� �ܺο��� ���ϵ��� �Ǿ� �־�����
// sdrMtxStack ���η� �����´�.
#define SDR_MAX_MTX_STACK_SIZE   (128)

// PRJ-1867 sdbDef.h���� �ŰܿԴ�.
// Corrupt Page�� �о��� ���� ��ó ���
typedef enum
{
    SDR_CORRUPTED_PAGE_READ_FATAL = 0,
    SDR_CORRUPTED_PAGE_READ_ABORT,
    SDR_CORRUPTED_PAGE_READ_PASS
} sdrCorruptPageReadPolicy;

/*-------------------------------------------
 * Description : DRDB �α� Ÿ��
 *-------------------------------------------*/
typedef enum
{
    /* page �Ӽ� update ���� */
    SDR_SDP_1BYTE  = 1,                /* 1byte  �Ӽ����� ���� */
    SDR_SDP_2BYTE  = 2,                /* 2bytes �Ӽ����� ���� */
    SDR_SDP_4BYTE  = 4,                /* 4bytes �Ӽ����� ���� */
    SDR_SDP_8BYTE  = 8,                /* 8bytes �Ӽ����� ���� */
    SDR_SDP_BINARY,                    /* Nbytes �Ӽ����� ���� */

    /* page ���� Ÿ�� */
    // PROJ-1665 : page�� consistent ������ �����ϴ� log
    // media recovery �ÿ� nologging���� Direct-Path INSERT�� �����
    // Page�� ���, in-consistent ���� �˾Ƴ��� ����
    SDR_SDP_PAGE_CONSISTENT = 32,

    SDR_SDP_INIT_PHYSICAL_PAGE,         /* extent desc���� freepage �Ҵ� �Ǵ�
                                           ���������� �Ҵ�, value ���� */
    SDR_SDP_INIT_LOGICAL_HDR,           /* logical header �ʱ�ȭ */
    SDR_SDP_INIT_SLOT_DIRECTORY,        /* slot directory �ʱ�ȭ */
    SDR_SDP_FREE_SLOT,                  /* page���� ������ �ϳ��� slto�� free */
    SDR_SDP_FREE_SLOT_FOR_SID,
    SDR_SDP_RESTORE_FREESPACE_CREDIT,

    // BUG-17615
    SDR_SDP_RESET_PAGE,                 /* index bottom-up build�� page reset */

    // PRJ-1149
    SDR_SDP_WRITE_PAGEIMG,             /* ��������߿� write�߻��� ����
                                           ����Ÿ���Ͽ� write�� �����ʰ�
                                           �α׷� ���� */
    // sdb
    // PROJ-1665 : Direct-Path Buffer���� Direct-Path INSERT�� �����
    //             Page�� Flush�Ҷ� Page ��ü�� ���Ͽ� Logging
    SDR_SDP_WRITE_DPATH_INS_PAGE,      /* PROJ-1665 */

    // PROJ-1671 Bitmap-based Tablespace And Segment Space Management
    SDR_SDPST_INIT_SEGHDR,              /* Segment Header �ʱ�ȭ */
    SDR_SDPST_INIT_BMP,                 /* Root/Internal Bitmap Page �ʱ�ȭ */
    SDR_SDPST_INIT_LFBMP,               /* Leaf Bitmap Page �ʱ�ȭ */
    SDR_SDPST_INIT_EXTDIR,              /* Extent Map Page �ʱ�ȭ */
    SDR_SDPST_ADD_RANGESLOT,            /* leaf bmp page�� rangeslot�� ��� */
    SDR_SDPST_ADD_SLOTS,                /* rt/it-bmp page�� slot���� ��� */
    SDR_SDPST_ADD_EXTDESC,              /* extent map page�� extslot ��� */
    SDR_SDPST_ADD_EXT_TO_SEGHDR,        /* segment�� extent ���� */
    SDR_SDPST_UPDATE_WM,                /* HWM ���� */
    SDR_SDPST_UPDATE_MFNL,              /* BMP MFNL ���� */
    SDR_SDPST_UPDATE_PBS,               /* PBS ���� */
    SDR_SDPST_UPDATE_LFBMP_4DPATH,      /* DirectPath�Ϸ�� lfbmp bitset���� */

    // PROJ-1705 Disk MVCC Renewal
    /* XXX: migration issue ������ �Ʒ� ���� 58�̾�� �Ѵ�. */
    SDR_SDPSC_INIT_SEGHDR = 58,         /* Segment Header �ʱ�ȭ */
    SDR_SDPSC_INIT_EXTDIR,              /* ExtDir Page �ʱ�ȭ */
    SDR_SDPSC_ADD_EXTDESC_TO_EXTDIR,    /* ExtDir Page�� ExtDesc ��� */

    SDR_SDPTB_INIT_LGHDR_PAGE,         /* local group header initialize */
    SDR_SDPTB_ALLOC_IN_LG,             /* alloc by bitmap index */
    SDR_SDPTB_FREE_IN_LG,              /* free by bitmap index */

                                       /* DML - insert, update, delete */
    SDR_SDC_INSERT_ROW_PIECE = 64,     /* page�� row piece�� insert�� �Ŀ� ��� */
    SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE,
    SDR_SDC_INSERT_ROW_PIECE_FOR_DELETEUNDO, /* delete rollback�� ������ �Ŀ� ��� */
    SDR_SDC_UPDATE_ROW_PIECE,
    SDR_SDC_OVERWRITE_ROW_PIECE,
    SDR_SDC_CHANGE_ROW_PIECE_LINK,
    SDR_SDC_DELETE_FIRST_COLUMN_PIECE,
    SDR_SDC_ADD_FIRST_COLUMN_PIECE,
    SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE,
    SDR_SDC_DELETE_ROW_PIECE,          /* row piece�� delete flag�� ������ �Ŀ� ��� */
    SDR_SDC_LOCK_ROW,
    SDR_SDC_PK_LOG,

    // PROJ-1705 Disk MVCC Renewal
    SDR_SDC_INIT_CTL,                   /* CTL ���� �� ��� �ʱ�ȭ */
    SDR_SDC_EXTEND_CTL,                 /* CTL ���� */
    SDR_SDC_BIND_CTS,                   /* Binding CTS */
    SDR_SDC_UNBIND_CTS,                 /* unbinding CTS */
    SDR_SDC_BIND_ROW,                   /* Binding In-Row CTS */
    SDR_SDC_UNBIND_ROW,                 /* unbinding In-Row CTS */
    SDR_SDC_ROW_TIMESTAMPING,           /* Row TimeStamping */
    SDR_SDC_DATA_SELFAGING,             /* SELF AGING */

    SDR_SDC_BIND_TSS,                  /* bind TSS */
    SDR_SDC_UNBIND_TSS,                /* unbind TSS */
    SDR_SDC_SET_INITSCN_TO_TSS,        /* Set INITSCN to CTS */
    SDR_SDC_INIT_TSS_PAGE,             /* TSS ������ �ʱ�ȭ */
    SDR_SDC_INIT_UNDO_PAGE,            /* Undo ������ �ʱ�ȭ */
    SDR_SDC_INSERT_UNDO_REC,           /* undo segment�� undo log��
                                          ������ �Ŀ� ��� */

    SDR_SDN_INSERT_INDEX_KEY = 96,     /* internal key�� ���� insert */
    SDR_SDN_FREE_INDEX_KEY,            /* internal key�� ���� delete */
    SDR_SDN_INSERT_UNIQUE_KEY,         /* insert index unique key�� ����
                                        * physical redo and logical undo */
    SDR_SDN_INSERT_DUP_KEY,            /* insert index duplicate key�� ����
                                        * physical redo and logical undo */
    SDR_SDN_DELETE_KEY_WITH_NTA,       /* delete index key�� ����
                                        * physical redo and logical undo */
    SDR_SDN_FREE_KEYS,                 /* BUG-19637 SMO�� ���� split�ɶ�
                                           index key�� ���� �� free */
    SDR_SDN_COMPACT_INDEX_PAGE,        /* index page�� ���� compact */
    SDR_SDN_KEY_STAMPING,
    SDR_SDN_INIT_CTL,                  /* Index CTL ���� �� ��� �ʱ�ȭ */
    SDR_SDN_EXTEND_CTL,                /* Index CTL Ȯ�� */
    SDR_SDN_FREE_CTS,

    /*
     * PROJ-2047 Strengthening LOB
     */
    SDR_SDC_LOB_UPDATE_LOBDESC,
    
    SDR_SDC_LOB_INSERT_INTERNAL_KEY,
    
    SDR_SDC_LOB_INSERT_LEAF_KEY,
    SDR_SDC_LOB_UPDATE_LEAF_KEY,
    SDR_SDC_LOB_OVERWRITE_LEAF_KEY,

    SDR_SDC_LOB_FREE_INTERNAL_KEY,
    SDR_SDC_LOB_FREE_LEAF_KEY,
    
    SDR_SDC_LOB_WRITE_PIECE,           /* PROJ-1362 lob piece write,
                                        * replication������ �߰�
                                        * SM���忡���� SDR_SDP_BINARY��
                                        * ����ص� ����. */
    SDR_SDC_LOB_WRITE_PIECE4DML,
    
    SDR_SDC_LOB_WRITE_PIECE_PREV,      /* replication������ �߰�,
                                        * replication�� �� �α׸� �����Ѵ�. */

    SDR_SDC_LOB_ADD_PAGE_TO_AGINGLIST,

    //external recovery function

    /*
     * PROJ-1591 Disk Spatial Index
     */
    SDR_STNDR_INSERT_INDEX_KEY,
    SDR_STNDR_UPDATE_INDEX_KEY,
    SDR_STNDR_FREE_INDEX_KEY,
    
    SDR_STNDR_INSERT_KEY,
    SDR_STNDR_DELETE_KEY_WITH_NTA,
    SDR_STNDR_FREE_KEYS,

    SDR_STNDR_COMPACT_INDEX_PAGE,
    SDR_STNDR_KEY_STAMPING,
    
    SDR_LOG_TYPE_MAX
} sdrLogType;

typedef enum sdrOPType
{
    SDR_OP_INVALID = 0,
    SDR_OP_NULL,
    SDR_OP_SDP_CREATE_TABLE_SEGMENT,
    SDR_OP_SDP_CREATE_LOB_SEGMENT,
    SDR_OP_SDP_CREATE_INDEX_SEGMENT,

    /* LOB Segment */
    SDR_OP_SDC_LOB_ADD_PAGE_TO_AGINGLIST,
    SDR_OP_SDC_LOB_APPEND_LEAFNODE,

    /* Undo Segment */
    SDR_OP_SDC_ALLOC_UNDO_PAGE,

    /* Treelist Segment : PROJ-1671 */
    SDR_OP_SDPTB_ALLOCATE_AN_EXTENT_FROM_TBS,

    /* Extent Dir.Free List for CMS */
    SDR_OP_SDPTB_ALLOCATE_AN_EXTDIR_FROM_LIST,

    SDR_OP_SDPTB_RESIZE_GG,
    SDR_OP_SDPST_ALLOC_PAGE,
    SDR_OP_SDPST_UPDATE_WMINFO_4DPATH,
    SDR_OP_SDPST_UPDATE_MFNL_4DPATH,
    SDR_OP_SDPST_UPDATE_BMP_4DPATH,

    /* Free List TableSpace: PROJ-1671 */
    SDR_OP_SDPSF_ALLOC_PAGE,
    SDR_OP_SDPSF_ADD_PIDLIST_PVTFREEPIDLIST_4DPATH,
    SDR_OP_SDPSF_MERGE_SEG_4DPATH,
    SDR_OP_SDPSF_UPDATE_HWMINFO_4DPATH,

    /* MVCC Renewal: PROJ-1704 */
    SDR_OP_SDN_INSERT_KEY_WITH_NTA,
    SDR_OP_SDN_DELETE_KEY_WITH_NTA,

    /* Direct Path Insert Rollback: PROJ-2068 */
    SDR_OP_SDP_DPATH_ADD_SEGINFO,

    /* Disk Spatial Index: PROJ-1591 */
    SDR_OP_STNDR_INSERT_KEY_WITH_NTA,
    SDR_OP_STNDR_DELETE_KEY_WITH_NTA,

    SDR_OP_MAX
} sdrOPType;

/* ------------------------------------------------
 * Description : sdrHashNode ó�� ���� ����
 *
 * hash ��� ���� hashed log�� ��� ��������
 * �ݿ� ���θ� ��Ÿ���� ���°��� �����Ѵ�.
 * ----------------------------------------------*/
typedef enum
{
    SDR_RECV_NOT_START = 0,   /* ����������� ��� ���� */
    SDR_RECV_START,           /* �������� ��� ���� */
    SDR_RECV_FINISHED,        /* ����Ϸ�� ��� ���� */
    SDR_RECV_INVALID          /* ��ȿ���� ���� hash ��� */
} sdrHashNodeState;

typedef struct sdrInitPageInfo
{
    // Page ID
    scPageID     mPageID;
    // Parent Page ID
    scPageID     mParentPID;
    // TableOID   BUG-32091
    smOID        mTableOID;
    // IndexID  PROJ-2281
    UInt         mIndexID;
    // Page Type
    UShort       mPageType;
    // Page State
    UShort       mPageState;
    // Parent���� ���� �������� ���° �������ΰ�
    SShort       mIdxInParent;
    UShort       mAlign;
} sdrInitPageInfo;

/* ------------------------------------------------
 * Description : media recovery ��� ����Ÿ���ϵ��� �ڷᱸ��
 * ----------------------------------------------*/
typedef struct sdrRecvFileHashNode
{
    sdrHashNodeState   mState;       /* ���Ե� redo �α׵��� �ݿ��� ���� */

    scSpaceID          mSpaceID;     /* tablespace id */
    sdFileID           mFileID;      /* file id */
    scPageID           mFstPageID;   /* ����Ÿ������ ���� PAGE ID*/
    scPageID           mLstPageID;   /* ����Ÿ������ ������ PAGE ID*/
    smLSN              mFromLSN;     /* ������ ���� redo �α��� LSN */
    smLSN              mToLSN;       /* ������ ������ redo �α��� LSN */
    time_t             mToTime;      /* ������ ������ redo �α���
                                        commit �α��� LSN (time) */

    UInt               mApplyRedoLogCnt; /* ������ �α� ���� */
    sddDataFileNode*   mFileNode;        /* ���� �Ϸ����� ���������
                                            �α׾�Ŀ sync�� ���� */
    SChar*             mSpaceName;    /* tablespace ��*/
} sdrRecvFileHashNode;

/* ------------------------------------------------
 * Description : hash ������ �����ϴ� hash ��� ����ü
 * ----------------------------------------------*/
typedef struct sdrRedoHashNode
{
    sdrHashNodeState     mState;       /* ���Ե� redo �α׵��� �ݿ��� ���¸�
                                      ��Ÿ���� �÷��� */
    scSpaceID            mSpaceID;     /* tablespace id */
    scPageID             mPageID;      /* page id */
    UInt                 mRedoLogCount; /* �����ϴ� redo �α� ��� count */
    smuList              mRedoLogList; /* �����ϴ� redo �α� list�� base node */

    sdrRecvFileHashNode* mRecvItem;    /* media recovery�� ���Ǵ� 2�� hash ���
                                          ������ */
} sdrRedoHashNode;

/* ------------------------------------------------
 * PROJ-1665 : corrupted pages hash node ����ü
 * ----------------------------------------------*/
typedef struct sdrCorruptPageHashNode
{
    scSpaceID            mSpaceID;     /* tablespace id */
    scPageID             mPageID;      /* page id */

} sdrCorruptPageHashNode;

/* ------------------------------------------------
 * Description : ���� redo log�� ����� ����ü
 * ----------------------------------------------*/
typedef struct sdrRedoLogData
{
    sdrLogType    mType;         /* �α� Ÿ�� */
    scSpaceID     mSpaceID;      /* SpaceID  - for PROJ-2162 */
    scOffset      mOffset;       /* offset */
    scSlotNum     mSlotNum;      /* slot num */
    scPageID      mPageID;       /* PID      - for PROJ-2162 */
    void        * mSourceLog;    /* �����α� - for PROJ-2162 */
    SChar       * mValue;        /* value */
    UInt          mValueLength;  /* value ���� */
    smTID         mTransID;      /* tx id */
    smLSN         mBeginLSN;     /* �α��� start lsn */
    smLSN         mEndLSN;       /* �α��� end lsn */
    smuList       mNode4RedoLog; /* redo �α��� ���� ����Ʈ */
} sdrRedoLogData;


/* --------------------------------------------------------------------
 * ������� mini transaction ����
 * ----------------------------------------------------------------- */
/* --------------------------------------------------------------------
 * mtx�� �α� ���
 * �Ϲ������δ� �α� ����̳�
 * �� �α� ���� �����ϴ� ���۷��̼��� �����Ѵ�.
 * ----------------------------------------------------------------- */

typedef enum
{
    // read �����θ� Ȥ�� log �ʿ���� �۾��Ѵ�.
    SDR_MTX_NOLOGGING = 0,

    // redo �α׸� write�Ѵ�. �Ϲ������� ���ȴ�.
    SDR_MTX_LOGGING
} sdrMtxLogMode;


/* --------------------------------------------------------------------
 * �α��� redo�� �� ���̳� redo�� ���� before�̹����� ����� ���̳�.
 * before image�� ���߿� �� ������ undo�� �� ���ȴ�.
 * �̴� mtx begin - commit ���̿� abort�� �߻����� ��,
 * abort�� ó���ϱ� ���Ͽ� ������ ������ ������ �ǵ����� ���� ���ȴ�.
 * mtx logging ����϶� ������ 2���� type���� ���еȴ�.
 * ----------------------------------------------------------------- */
typedef enum
{
    // redo �α׸� write�Ѵ�. �Ϲ������� ���ȴ�.
    SDR_MTX_LOGGING_REDO_ONLY = 0 ,

    // n byte redo�α׿� ���� undo log�� write�Ѵ�.
    SDR_MTX_LOGGING_WITH_UNDO

} sdrMtxLoggingType;


/* --------------------------------------------------------------------
 * mtx���� ����ϴ� latch�� ����.
 * commit�ÿ� �� �������� release�� ���ְ� �ȴ�.
 * ----------------------------------------------------------------- */
typedef enum
{
    SDR_MTX_PAGE_X = SDB_X_LATCH,  // page X-latch

    SDR_MTX_PAGE_S = SDB_S_LATCH,  // page S-latch

    SDR_MTX_PAGE_NO = SDB_NO_LATCH,// no latch

    SDR_MTX_LATCH_X,               // latch�� X�� ��´�.

    SDR_MTX_LATCH_S,               // latch�� S�� ��´�.

    SDR_MTX_MUTEX_X                // mutex�� ��´�.

} sdrMtxLatchMode;

/* --------------------------------------------------------------------
 * mtx���� ����ϴ� latch�� ������ ���� relase function����
 * sdrMtxLatchMode�� ������ ����.
 * ----------------------------------------------------------------- */
#define SDR_MTX_RELEASE_FUNC_NUM  (SDR_MTX_MUTEX_X+1)


/* ----------------------------------------------------------------------------
 *   PROJ-1867
 *   CorruptPageErrPolicy - Corrupt Page�� �߰��Ͽ��� �� ��ó���
 *   SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL - CorruptPage �߽߰� ���� ����
 *   SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE
 *                              - CorruptPage �߽߰� ImgLog�� Overwrite �õ�
 *   CORRUPT_PAGE_ERR_POLICY  0(00)  1(01)  2(10)  3(11)
 *   SERVERFATAL  (01)          X      O      X      O
 *   OVERWRITE    (10)          X      X      O      O
 * --------------------------------------------------------------------------*/
#define SDR_CORRUPT_PAGE_ERR_POLICY_SERVERFATAL  (0x00000001)
#define SDR_CORRUPT_PAGE_ERR_POLICY_OVERWRITE    (0x00000002)


/* --------------------------------------------------------------------
 *
 * latch item�� Ÿ���� ��Ÿ����.
 * mtx stack�� Ư�� �������� �ֳ� Ȯ���ϰ��� �Ҷ�
 * ���ȴ�.
 *
 * ----------------------------------------------------------------- */
typedef enum
{
    SDR_MTX_BCB = 0,
    SDR_MTX_LATCH,
    SDR_MTX_MUTEX
} sdrMtxItemSourceType;

/* ---------------------------------------------
 * latch release�� ���� function prototype
 * --------------------------------------------- */
typedef IDE_RC (*smrMtxReleaseFunc)( void *aObject,
                                     UInt  aLatchMode,
                                     void *aMtx );

/* ---------------------------------------------
 * stack�� ����� Object�� �񱳸� ���� �Լ�
 * --------------------------------------------- */
typedef idBool (*smrMtxCompareFunc)( void *aLhs,
                                     void *aRhs );

/* ---------------------------------------------
 * stack�� ����� Object�� dump �ϱ� ���� �Լ�
 * --------------------------------------------- */
typedef IDE_RC (*smrMtxDumpFunc)( void *aObject );


/* --------------------------------------------------------------------
 * enum type�� latch mode�̸��� �ִ� ����
 * dump�� ���� ���̹Ƿ� ū �ǹ̴� ����.
 * ----------------------------------------------------------------- */
#define LATCH_MODE_NAME_MAX_LEN (50)

/* --------------------------------------------------------------------
 * mtx stack�� �� latch item�� ����Ǵ� �Լ����� ���հ�
 * latch ��� ���� ����
 * ----------------------------------------------------------------- */
typedef struct sdrMtxLatchItemApplyInfo
{
    smrMtxReleaseFunc mReleaseFunc;
    smrMtxReleaseFunc mReleaseFunc4Rollback;
    smrMtxCompareFunc mCompareFunc;
    smrMtxDumpFunc    mDumpFunc;
    sdrMtxItemSourceType mSourceType;

    // dump�� ���� latch mode�� ���ڿ�
    SChar             mLatchModeName[LATCH_MODE_NAME_MAX_LEN];

} sdrMtxLatchItemApplyInfo;

/* --------------------------------------------------------------------
 * Description mtx ���ÿ� ����Ǵ� �ڷᱸ��
 *
 * latch ���ÿ� push�ϰų� pop�ϴ� �����ڷ��̴�.
 * ----------------------------------------------------------------- */
typedef struct sdrMtxLatchItem
{
    void*             mObject;
    sdrMtxLatchMode   mLatchMode;
    smuAlignBuf       mPageCopy;  /*PROJ-2162 RedoValidation�� ���� ����*/
} sdrMtxLatchItem;

/* --------------------------------------------------------------------
 * Description : DRDB �α��� Header
 *
 * �α��� Ÿ���� SDR_1/2/4/8_BYTES�� ��� body�� length��
 * ������� ������, �� ���� ���� length�� ����Ѵ�.
 * ----------------------------------------------------------------- */
typedef struct sdrLogHdr
{
    scGRID         mGRID;
    sdrLogType     mType;
    UInt           mLength;
} sdrLogHdr;

/* --------------------------------------------------------------------
 * 2���� stack item�� �޾� ���ϴ� function�̴�.
 * ----------------------------------------------------------------- */
typedef idBool (*sdrMtxStackItemCompFunc)(void *, void *);

/* --------------------------------------------------------------------
 * stack�� ������ ���´�. ����Ʈ�� �̿��Ͽ� ������
 * ----------------------------------------------------------------- */
typedef struct sdrMtxStackInfo
{
    UInt          mTotalSize;      // ������ �� ũ��

    UInt          mCurrSize;      // ���� ������ ũ��

    sdrMtxLatchItem mArray[SDR_MAX_MTX_STACK_SIZE];

} sdrMtxStackInfo;

/* -------------------------------------------------------------------------
 * BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� �մϴ�. 
 * 
 * Mini-transaction Commit ���Ŀ� ��Ȱ�� �ǵ��� �ϱ� ����, Mini-transaction
 * ���� Free Extent ����� ���� �ֽ��ϴ�. FreeExtent�� Mini-Transaction��
 * Commit ���� �� �������� �Ͼ�� �ϴ� Pending ���� �Դϴ�.
 * ---------------------------------------------------------------------- */
typedef IDE_RC (*sdrMtxPendingFunc)( void * aData );

/* --------------------------------------------------------------------------
 *  PROJ-2162 RestartRiskReduction
 *  DISK MiniTransaction ���� Flag���� ������.
 *  �߰������� DiskLogging ���� Flag���� smDef.h�� SM_DLOG Prefix�� ������
 * ------------------------------------------------------------------------*/

/*  For DISK NOLOGGING Attributes */
# define SDR_MTX_NOLOGGING_ATTR_PERSISTENT_MASK   (0x00000001)
# define SDR_MTX_NOLOGGING_ATTR_PERSISTENT_OFF    (0x00000000)
# define SDR_MTX_NOLOGGING_ATTR_PERSISTENT_ON     (0x00000001)

/* TASK-2398 Log Compress
 *   Disk Log�� Log File�� ��ϵ� �� ���� ���θ� �����ϴ� Flag
 *   => Mini Transaction�� ����ϴ� Log��
 *      LOG������ ���� �ʰڴٰ� ������ Tablespace�� ���� �αװ�
 *      �ϳ��� ������, �α׸� �������� �ʴ´�. */
# define SDR_MTX_LOG_SHOULD_COMPRESS_MASK         (0x00000002)
# define SDR_MTX_LOG_SHOULD_COMPRESS_ON           (0x00000000)
# define SDR_MTX_LOG_SHOULD_COMPRESS_OFF          (0x00000002)

/* mtx�� init�Ǿ����� true
 * destroy�Ǹ� false
 * rollback�ÿ� true�϶��� rollback�� �����ϵ��� �ϱ� ���� �߰�
 * mtx begin�ϴ� �Լ������� Stateó�� ���� exception����
 * mtx rollback�� ���ָ� �ȴ�. */
# define SDR_MTX_INITIALIZED_MASK                 (0x00000004)
# define SDR_MTX_INITIALIZED_ON                   (0x00000000)
# define SDR_MTX_INITIALIZED_OFF                  (0x00000004)

/* Logging ���� Flag
 * NoLogging - read �����θ� Ȥ�� log �ʿ���� �۾��Ѵ�.
 * Logging   - redo �α׸� write�Ѵ�. �Ϲ������� ���ȴ�.
 * ( ���� sdrMtxLogMde���� ) */
# define SDR_MTX_LOGGING_MASK                     (0x00000008)
# define SDR_MTX_LOGGING_ON                       (0x00000000)
# define SDR_MTX_LOGGING_OFF                      (0x00000008)

/* Refrence Offset ���� ���� ( RefOffset )
 *
 * - DML���� undo/redo �α� ��ġ ���
 * ref offset�� �ϳ��� smrDiskLog�� ��ϵ� �������� disk �α׵� ��
 * replication sender�� �����Ͽ� �α׸� �ǵ��ϰų� transaction undo�ÿ�
 * �ǵ��ϴ� DML���� redo/undo �αװ� ��ϵ� ��ġ�� �ǹ��Ѵ�. */
# define SDR_MTX_REFOFFSET_MASK                   (0x00000010)
# define SDR_MTX_REFOFFSET_OFF                    (0x00000000)
# define SDR_MTX_REFOFFSET_ON                     (0x00000010)

/* MtxUndo���� ����
 * 
 *  PROJ-2162 RestartRiskReduction
 *  Mtx�� �⺻������ DirtyPage(XLatch������ �ڵ����� DirtyPage�� �ν�)��
 * ���� �����ϴ� ����� ������, ��ϵ��� ���� Runtime��ü�� �������� ����
 * ���ϱ� ������ ���� �Ұ����ϴ�. ���� �̷��� ��쿡 ���� �������� ����
 * �ؾ� �Ѵ�.
 *  �׽����� ���� ������ ��츸 MtxRollback ���� ���θ� �����Ѵ�.
 *
 * �̿� ���� �׽�Ʈ �����
 * TC/Server/sm4/Project3/PROJ-2162/function/mtxtest
 * �� �����Ѵ�. */
# define SDR_MTX_UNDOABLE_MASK                    (0x00000020)
# define SDR_MTX_UNDOABLE_OFF                     (0x00000000)
# define SDR_MTX_UNDOABLE_ON                      (0x00000020)

/* MtxUndo���� ����
 * 
 *  PROJ-2162 RestartRiskReduction
 *  Mtx�� Dirty�� �������� �ݵ�� �����Ѵ�. �� ������ ���� �ɸ���. ������
 * UniqueViolation, ��������, Update Retry ���� �� Undo�� �ʿ� ���� 
 * �׳� Log���� �����ص� �Ǵ� ��찡 �ִ�.
 *  �̷� ��� Mtx��� ������ �ش� ������ ���������ν� ������� Rollback��
 * ���´�. */
# define SDR_MTX_IGNORE_UNDO_MASK                 (0x00000040)
# define SDR_MTX_IGNORE_UNDO_OFF                  (0x00000000)
# define SDR_MTX_IGNORE_UNDO_ON                   (0x00000040)

/* StartupBugDetector ��� ���� ����
 *
 *  PROJ-2162 RestartRiskReduction
 * __SM_ENABLE_STARTUP_BUG_DETECTOR �� ���� �����صд�. */
# define SDR_MTX_STARTUP_BUG_DETECTOR_MASK        (0x00000080)
# define SDR_MTX_STARTUP_BUG_DETECTOR_OFF         (0x00000000)
# define SDR_MTX_STARTUP_BUG_DETECTOR_ON          (0x00000080)


# define SDR_MTX_DEFAULT_INIT ( SDR_MTX_NOLOGGING_ATTR_PERSISTENT_OFF | \
                                SDR_MTX_LOG_SHOULD_COMPRESS_ON |        \
                                SDR_MTX_INITIALIZED_ON |                \
                                SDR_MTX_LOGGING_ON |                    \
                                SDR_MTX_REFOFFSET_OFF |                 \
                                SDR_MTX_UNDOABLE_OFF |                  \
                                SDR_MTX_IGNORE_UNDO_OFF |               \
                                SDR_MTX_STARTUP_BUG_DETECTOR_OFF )

# define SDR_MTX_DEFAULT_DESTROY ( SDR_MTX_NOLOGGING_ATTR_PERSISTENT_OFF | \
                                   SDR_MTX_LOG_SHOULD_COMPRESS_ON |        \
                                   SDR_MTX_INITIALIZED_OFF |               \
                                   SDR_MTX_LOGGING_ON |                    \
                                   SDR_MTX_REFOFFSET_OFF |                 \
                                   SDR_MTX_UNDOABLE_OFF |                  \
                                   SDR_MTX_IGNORE_UNDO_OFF |               \
                                   SDR_MTX_STARTUP_BUG_DETECTOR_OFF )

#define SDR_MTX_STARTUP_BUG_DETECTOR_IS_ON(f)                              \
                           ( ( ( (f) & SDR_MTX_STARTUP_BUG_DETECTOR_MASK)  \
                                == SDR_MTX_STARTUP_BUG_DETECTOR_ON ) ?     \
                             ID_TRUE : ID_FALSE ) 
 

typedef struct sdrMtxPendingJob
{
    idBool              mIsCommitJob; /* Commit�� ����(T) Rollback�� ����(F)*/
    sdrMtxPendingFunc   mPendingFunc; /* Job�� ������ �Լ� */
    void              * mData;        /* Job�� �����ϴµ� �ʿ��� ������.*/
} sdrMtxPendingJob;



/* --------------------------------------------------------------------
 * Description : mini-transaction �ڷᱸ��
 *
 * mtx�� ������ �����Ѵ�.
 * ----------------------------------------------------------------- */
typedef struct sdrMtx
{
    smuDynArrayBase     mLogBuffer;  /* mtx �α� ���� */
    idvSQL*             mStatSQL;    /* ������� */
    void*               mTrans;      /* mtx�� �ʱ�ȭ�� Ʈ����� */

    UInt                mLogCnt;
    scSpaceID           mAccSpaceID; /* mtx�� ���� ����Ǵ� TBS�� �ϳ��̴�. */
                                     /* ��, undo tbs�� data tbs��
                                        ���� �Բ� ����� �� �ִ� */

    smLSN               mBeginLSN;    /* Begin LSN */
    smLSN               mEndLSN;      /* End LSN   */
    // disk log�� �Ӽ� mtx begin�ÿ� ���ǵȴ�.
    UInt                mDLogAttr;

    UInt                mFlag;
    UInt                mOpType;      /* operation �α��� ���� Ÿ�� */
    smLSN               mPPrevLSN;    /* NTA ���� ������ �ʿ��� LSN
                                         ������ ���� LSN */
    UInt                mRefOffset;   /* �α׹��ۻ��� DML����
                                         redo/undo �α� ��ġ */

    /* -----------------------------------------------------------------------
     * BUG-24730 [SD] Drop�� Temp Segment�� Extent�� ������ ����Ǿ�� 
     * �մϴ�. 
     * 
     * Mini-transaction Commit ���Ŀ� ��Ȱ�� �ǵ��� �ϱ� ����, MiniTransaction
     * ���� Free Extent ����� ���� �ֽ��ϴ�. FreeExtent�� MiniTransaction��
     * Commit ���� �� �������� �Ͼ�� �ϴ� Pending ���� �Դϴ�.
     * -------------------------------------------------------------------- */
    smuList             mPendingJob;

    UInt                mDataCount;

    // replication�� ���� table oid
    smOID               mTableOID;

    /* PROJ-2162 RestartRiskReduction
     * DRDB Log�� N���� SubLog�� �����Ǹ� �ϳ��� SubLog�� initLogRec�� ����
     * sdrLogHdr�� ���·� Header�� ��ϵȴ�. ���� initLogRec���� ��
     * SubLog�� ũ�⸦ ����������, �� SubLog�� ���� ��߸� Mtx�� ����
     * Commit�� �� �ִ�. */
    /* BUG-32579 The MiniTransaction commit should not be used in
     * exception handling area. */
    UInt                mRemainLogRecSize; 

    //-----------------------------------------------------
    // PROJ-1566
    // < Extent �Ҵ������ >
    // - mData1 : SpaceID
    // - mData2 : segment RID
    // - mData3 : extent RID
    // < Page list�� meta�� Page list�� �߰��� �� >
    // - mData1 : table OID
    // - mData2 : tail page PID
    // - mData3 : ������ page�� ��, table type�� page ����
    // - mData4 : ������ ��� page ���� ( multiple, external ���� )
    //-----------------------------------------------------
    ULong               mData[ SM_DISK_NTALOG_DATA_COUNT ];

    sdrMtxStackInfo     mLatchStack;  /* mtx���� ���Ǵ� stack */
    sdrMtxStackInfo     mXLatchPageStack; /* XLatch�� ���� Page���� Stack */
} sdrMtx;


/* --------------------------------------------------------------------
 * mtx�� begin�ϴµ� �ʿ��� trans, logmode�� �����ϱ� ���� ����ü
 *
 * ----------------------------------------------------------------- */
typedef struct sdrMtxStartInfo
{
    void          *mTrans;
    sdrMtxLogMode  mLogMode;
} sdrMtxStartInfo;


/* --------------------------------------------------------------------
 * stack�� �� item�� ������ dump�� ���ΰ�.
 * ----------------------------------------------------------------- */
typedef enum
{
    // stack�� dump�Ѵ�.
    SDR_MTX_DUMP_NORMAL,
    // stack�� �������� ��� dump�Ѵ�.
    SDR_MTX_DUMP_DETAILED
} sdrMtxDumpMode ;

/* --------------------------------------------------------------------
 * mtx�� save point
 * �� �������� latch ���� ��� object�� ������ �� �ִ�.
 * save point ���ķ� log�� write �Ǿ��ٸ� save point�� ��ȿȭ�ȴ�.
 * ----------------------------------------------------------------- */
typedef struct sdrSavePoint
{
    // stack�� ��ġ
    UInt   mStackIndex;

    // XLatch Stack�� ��ġ
    UInt   mXLatchStackIndex;

    // mtx�� write�� log�� ũ��
    UInt   mLogSize;
} sdrSavePoint;

/* --------------------------------------------------------------------
 * Redo �ÿ� �ʿ��� ����
 * ----------------------------------------------------------------- */
typedef struct sdrRedoInfo
{
    sdrLogType     mLogType; // �α�Ÿ��
    scSlotNum      mSlotNum; // Table�������� Slot Num
} sdrRedoInfo;

typedef IDE_RC (*sdrDiskRedoFunction)( SChar       * aData,
                                       UInt          aLength,
                                       UChar       * aPagePtr,
                                       sdrRedoInfo * aRedoInfo,
                                       sdrMtx      * aMtx );


typedef IDE_RC (*sdrDiskUndoFunction)(idvSQL      * aStatistics,
                                      smTID         aTransID,
                                      smOID         aOID,
                                      scGRID        aRecGRID,
                                      SChar       * aLogPtr,
                                      smLSN       * aPrevLSN );

typedef IDE_RC (*sdrDiskRefNTAUndoFunction)(idvSQL   * aStatistics,
                                            void     * aTrans,
                                            sdrMtx   * aMtx,
                                            scGRID     aGRID,
                                            SChar    * aLogPtr,
                                            UInt       aSize );

#endif // _O_SDR_DEF_H_
