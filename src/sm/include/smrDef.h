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
 * $Id: smrDef.h 90444 2021-04-02 10:15:58Z minku.kang $
 *
 * Description :
 *
 * Recovery Layer Common Header ����
 *
 *
 **********************************************************************/

#ifndef _O_SMR_DEF_H_
#define _O_SMR_DEF_H_ 1

#include <smDef.h>
#include <smu.h>
#include <sdrDef.h>
#include <smriDef.h>
#include <sdsDef.h>

/* --------------------------------------------------------------------
 * Memory Redo �Լ�
 * ----------------------------------------------------------------- */

typedef IDE_RC (*smrRecFunction)(smTID,
                                 scSpaceID,
                                 scPageID,
                                 scOffset,
                                 vULong,
                                 SChar*,
                                 SInt,
                                 UInt aFlag);

// BUG-9640
typedef IDE_RC (*smrTBSUptRecFunction)( idvSQL*            aStatistics,
                                        void*              aTrans,
                                        smLSN              aCurLSN,
                                        scSpaceID          aSpaceID,
                                        UInt               aFileID,
                                        UInt               aValueSize,
                                        SChar*             aValuePtr,
                                        idBool             aIsRestart );

#define SMR_LOG_FULL_SIZE               (500)

#define SMR_LOG_FILE_NAME               "logfile"
#define SMR_TEMP_LOG_FILE_NAME          "logfile.tmp"  /* BUG-48409 */

#define SMR_BACKUP_VER_NAME             "backup_ver"
#define SMR_LOGANCHOR_RESERV_SIZE       (4)

/* ------------------------------------------------
 * for multi-loganchor
 * ----------------------------------------------*/
#define SMR_LOGANCHOR_FILE_COUNT       (3)
#define SMR_LOGANCHOR_NAME              "loganchor"

#define SMR_BACKUP_NONE                (0x00000000)
#define SMR_BACKUP_MEMTBS              (0x00000001)
#define SMR_BACKUP_DISKTBS             (0x00000002)

typedef UInt    smrBackupState;


/*
 PRJ-1548 �̵�� ���� ��ü ����
 �̵�� ������ �ʿ��� ����Ÿ������
 �޸����� ��ũ���� ��δ� ������ �����Ͽ�
 �̵�� ������ ������ �α׷��ڵ带
 ������ϱ� ���ؼ��̴�
 */
# define SMR_FAILURE_MEDIA_NONE      (0x00000000) // �̵���������
# define SMR_FAILURE_MEDIA_MRDB      (0x00000001) // �޸�DB ����
# define SMR_FAILURE_MEDIA_DRDB      (0x00000010) // ��ũDB ����
# define SMR_FAILURE_MEDIA_BOTH      (0x00000011) // ��� ����

/* ------------------------------------------------
 * drdb�� �αװ� ���� �α׿� ���� ������ �Ǿ�����
 * ���Ϸα������� �Ǵ� �� �� �ִ� type
 * ----------------------------------------------*/
typedef enum
{
    SMR_CT_END = 0,
    SMR_CT_CONTINUE
} smrContType;

typedef enum
{
    SMR_CHKPT_TYPE_MRDB = 0,
    SMR_CHKPT_TYPE_DRDB,
    SMR_CHKPT_TYPE_BOTH
} smrChkptType;

/* ------------------------------------------------
 * redo ����� runtime memory�� ���Ͽ� �Բ� �����
 * �� ���ΰ��� ���θ� ǥ��
 * ----------------------------------------------*/
typedef enum
{
    SMR_RT_DISKONLY = 0,
    SMR_RT_WITHMEM
} smrRedoType;

typedef enum
{
    SMR_SERVER_SHUTDOWN,
    SMR_SERVER_STARTED
} smrServerStatus;

/*  PROJ-1362   replication for LOB */
/*  LogType  SMR_LT_LOB_FOR_REPL�� sub type */
typedef enum
{
    SMR_MEM_LOB_CURSOR_OPEN = 0,
    SMR_DISK_LOB_CURSOR_OPEN,
    SMR_LOB_CURSOR_CLOSE,
    SMR_PREPARE4WRITE,
    SMR_FINISH2WRITE,
    SMR_LOB_ERASE,
    SMR_LOB_TRIM,
    SMR_LOB_OP_MAX
} smrLobOpType;


/* --------------------------------------
   �α�Ÿ�� �߰�, ����, ������
   smrLogFileDump���� ������ �ִ� LogType
   �̸��� ���� ������ �ʿ��մϴ�.

   ���� logType ������ �þ UChar���� ��
   �̻��� Ÿ������ �þ ���, �̸��� ����
   �ϴ� ���ڿ� �迭�� ũ�⵵ �����ؾ� �մ�
   ��.
   -------------------------------------- */
typedef UChar                       smrLogType;

/* � undo, redo�� �������� �ʴ� �α׷μ�
   1. Repliction Sender ���۽� Log�� ���ؼ� ����Ѵ�.
   �ڼ��� ������ smiReadLogByOrder.cpp�� �����ϱ� �ٶ���.
   2. Checkpoint�� smrLogMgr :: getRestartRedoLSN
   ȣ��� Log�� ����Ѵ�.
*/

#define SMR_LT_NULL                  (0)

#define SMR_LT_DUMMY                 (1)

    /* Checkpoint */
#define SMR_LT_CHKPT_BEGIN          (20)
#define SMR_LT_DIRTY_PAGE           (21)
#define SMR_LT_CHKPT_END            (22)

/* Transaction */
/* BUG-47525 */
#define SMR_LT_MEMTRANS_GROUPCOMMIT (37)
/*
 * BUG-24906 [valgrind] sdcUpdate::redo_SDR_SDC_UNBIND_TSS()���� 
 *           valgrind ������ �߻��մϴ�.
 * ��ũ Ʈ����ǿ� ���̺긮�� Ʈ������� Commit/Abort�α׿�
 * �޸� Ʈ������� Commit/Abort�α׸� �з��Ѵ�.
 */
#define SMR_LT_MEMTRANS_COMMIT      (38)
#define SMR_LT_MEMTRANS_ABORT       (39)
#define SMR_LT_DSKTRANS_COMMIT      (40)
#define SMR_LT_DSKTRANS_ABORT       (41)

#define SMR_LT_SAVEPOINT_SET        (42)
#define SMR_LT_SAVEPOINT_ABORT      (43)
#define SMR_LT_XA_PREPARE           (44)
#define SMR_LT_TRANS_PREABORT       (45)
// PROJ-1442 Replication Online �� DDL ���
#define SMR_LT_DDL                  (46)
// PROJ-1705 Disk MVCC ������
/* Prepare Ʈ������� ����ϴ� Ʈ����� ���׸�Ʈ ������ ���� */
#define SMR_LT_XA_SEGS              (47)


  /*  PROJ-1362   replication for LOB */
#define SMR_LT_LOB_FOR_REPL         (50)

/*  PROJ-1723 [MDW/INTEGRATOR] Altibase Plugin ����
   Table/Index/Sequence��
   Create/Alter/Drop DDL�� ���� Query String�� �α��Ѵ�.
*/
#define SMR_LT_DDL_QUERY_STRING     (51)


    /* Update */
#define SMR_LT_UPDATE               (60)

    /* Rollback And NTA(Nested Top Action)  And File And */
#define SMR_LT_NTA                  (61)
#define SMR_LT_COMPENSATION         (62)
#define SMR_LT_DUMMY_COMPENSATION   (63)
#define SMR_LT_FILE_BEGIN           (64)

    /* PRJ-1548 Tablespace Update */
#define SMR_LT_TBS_UPDATE           (65)

#define SMR_LT_FILE_END             (66)

/*-------------------------------------------
* DRDB �α� Ÿ�� �߰�
*
* - Ư�� page�� ���� physical or logical redo �α�Ÿ��
*   : SMR_DLT_REDOONLY
* - undo record ��Ͽ� ���� redo-undo �α� Ÿ��
*   : SMR_DLT_UNDOABLE
* - ���꿡 ���� NTA�α�
*   : SMR_DLT_NTA
*-------------------------------------------*/

/*
 * PRJ-1548 User Memory Tablespace
 * ��ũ�α�Ÿ���� �߰��Ǹ� �����Լ����� �߰����־�� �Ѵ�.
 * smrRecoveryMgr::isDiskLogType()
 */
#define SMR_DLT_REDOONLY            (80)
#define SMR_DLT_UNDOABLE            (81)
#define SMR_DLT_NTA                 (82)
#define SMR_DLT_COMPENSATION        (83)
#define SMR_DLT_REF_NTA             (85)

/* PROJ-2569 DK XA 2PC */
#define SMR_LT_XA_PREPARE_REQ       (86)
#define SMR_LT_XA_END               (87)

/* PROJ-2747 GlobalTx Consistency */
#define SMR_LT_XA_START_REQ         (88)

/*-------------------------------------------
 * Meta �α� Ÿ�� �߰�
 *------------------------------------------*/
// PROJ-1442 Replication Online �� DDL ���
#define SMR_LT_TABLE_META           (100)

/*-------------------------------------------*/
// Log Type Name �������
// SMR + Manager Name + Update ��ġ + Action
//
// ��) SMR + SMM + MEMBASE + "Update Link"
//     = SMR_SMM_MEMBASE_UPDATE_LINK
/*-------------------------------------------*/

//========================================================
// smrUpdateLog �� ���� �з� Ÿ��
typedef UShort smrUpdateType;

//========================================================
// smrUpdateType �� ������ ��ϵ� ��
#define SMR_PHYSICAL                            (0)

//SMM
#define SMR_SMM_MEMBASE_SET_SYSTEM_SCN          (1)
#define SMR_SMM_MEMBASE_ALLOC_PERS_LIST         (2)
#define SMR_SMM_MEMBASE_ALLOC_EXPAND_CHUNK      (3)
#define SMR_SMM_PERS_UPDATE_LINK                (4)
#define SMR_SMM_PERS_UPDATE_NEXT_FREE_PAGE_LINK (5)
#define SMR_SMM_MEMBASE_INFO                    (6)

//SMC
// 1) Table Header
#define SMR_SMC_TABLEHEADER_INIT                       (20)
#define SMR_SMC_TABLEHEADER_UPDATE_INDEX               (21)
#define SMR_SMC_TABLEHEADER_UPDATE_COLUMNS             (22)
#define SMR_SMC_TABLEHEADER_UPDATE_INFO                (23)
#define SMR_SMC_TABLEHEADER_SET_NULLROW                (24)
#define SMR_SMC_TABLEHEADER_UPDATE_ALL                 (25)
#define SMR_SMC_TABLEHEADER_UPDATE_ALLOCINFO           (26)
#define SMR_SMC_TABLEHEADER_UPDATE_FLAG                (27)
#define SMR_SMC_TABLEHEADER_SET_SEQUENCE               (28)
#define SMR_SMC_TABLEHEADER_UPDATE_COLUMN_COUNT        (29)
#define SMR_SMC_TABLEHEADER_UPDATE_TABLE_SEGMENT       (30)
#define SMR_SMC_TABLEHEADER_SET_SEGSTOATTR             (31)
#define SMR_SMC_TABLEHEADER_SET_INSERTLIMIT            (32)
#define SMR_SMC_TABLEHEADER_SET_INCONSISTENCY          (33) /* PROJ-2162 */

// 2) Index Header
#define SMR_SMC_INDEX_SET_FLAG                   (40)
#define SMR_SMC_INDEX_SET_SEGATTR                (41)
#define SMR_SMC_INDEX_SET_SEGSTOATTR             (42)
#define SMR_SMC_INDEX_SET_DROP_FLAG              (43)

// 3) Pers Page
#define SMR_SMC_PERS_INIT_FIXED_PAGE             (60)
#define SMR_SMC_PERS_INIT_FIXED_ROW              (61)
#define SMR_SMC_PERS_UPDATE_FIXED_ROW            (62)
#define SMR_SMC_PERS_UPDATE_FIXED_ROW_NEXT_VERSION  (64)
#define SMR_SMC_PERS_SET_FIX_ROW_DROP_FLAG       (66)
#define SMR_SMC_PERS_SET_FIX_ROW_DELETE_BIT      (67)
#define SMR_SMC_PERS_INIT_VAR_PAGE               (68)
#define SMR_SMC_PERS_UPDATE_VAR_ROW_HEAD         (69)
#define SMR_SMC_PERS_UPDATE_VAR_ROW              (70)
#define SMR_SMC_PERS_SET_VAR_ROW_FLAG            (71)
#define SMR_SMC_PERS_SET_VAR_ROW_NXT_OID         (72)
#define SMR_SMC_PERS_WRITE_LOB_PIECE             (73)
#define SMR_SMC_PERS_SET_INCONSISTENCY           (74) /* PROJ-2162 */

// 4) Replication
#define SMR_SMC_PERS_INSERT_ROW                  (80)
#define SMR_SMC_PERS_UPDATE_INPLACE_ROW          (81)
#define SMR_SMC_PERS_UPDATE_VERSION_ROW          (82)
#define SMR_SMC_PERS_DELETE_VERSION_ROW          (83)
/* PROJ-2429 Dictionary based data compress for on-disk DB */
#define SMR_SMC_SET_CREATE_SCN                   (84)

/* ------------------------------------------------
 * LOG HEADER
 * BUG-35392 ������ ���� UChar -> UInt�� ���� ��
 * ----------------------------------------------*/
#define SMR_LOG_TYPE_MASK               (0x00000003)
#define SMR_LOG_TYPE_NORMAL             (0x00000000)
#define SMR_LOG_TYPE_REPLICATED         (0x00000001)
#define SMR_LOG_TYPE_REPL_RECOVERY      (0x00000002)

#define SMR_LOG_SAVEPOINT_MASK          (0x00000004)
#define SMR_LOG_SAVEPOINT_NO            (0x00000000)
#define SMR_LOG_SAVEPOINT_OK            (0x00000004)

#define SMR_LOG_BEGINTRANS_MASK         (0x00000008)
#define SMR_LOG_BEGINTRANS_NO           (0x00000000)
#define SMR_LOG_BEGINTRANS_OK           (0x00000008)

/* BUG-14513 : Insert, update, delete�� allot slot�� alloc Slot log����.
 * DML Log(insert, update, delete�αװ� Alloc Slot�� ����
 * �α׵� �����ϰ� �ִ��� ǥ�� */
#define SMR_LOG_ALLOC_FIXEDSLOT_MASK    (0x00000010)
#define SMR_LOG_ALLOC_FIXEDSLOT_NO      (0x00000000)
#define SMR_LOG_ALLOC_FIXEDSLOT_OK      (0x00000010)

/* TASK-2398 Log Compress
 * Log ������ ������ �ʴ� ���,
 * smrLogHead(����� Log Head)�� Flag�� �� �÷��׸� �����Ѵ�
 *
 * Disk Log�� ��� Mini Transaction�� ������ ���� Tablespace ��
 * Log ������ ���� �ʵ��� ������ Tablespace�� �ϳ��� ������
 * �� Flag�� �����Ͽ� �α׸� �������� �ʵ��� �Ѵ�. */
#define SMR_LOG_FORBID_COMPRESS_MASK    (0x00000020)
#define SMR_LOG_FORBID_COMPRESS_NO      (0x00000000)
#define SMR_LOG_FORBID_COMPRESS_OK      (0x00000020)

/* TASK-5030 Full XLog */
#define SMR_LOG_FULL_XLOG_MASK          (0x00000040)
#define SMR_LOG_FULL_XLOG_NO            (0x00000000)
#define SMR_LOG_FULL_XLOG_OK            (0x00000040)

/* �ش� �α� ���ڵ尡 ����Ǿ����� ����
 * �� Flag�� ����α�, �����α��� ù��° ����Ʈ�� ��ϵȴ�.
 * �� Flag�� ���� �α׸� ����α׷� �ؼ�����,
 * ����� �α׷� �ؼ����� ���θ� �Ǵٸ��Ѵ�. */
#define SMR_LOG_COMPRESSED_MASK         (0x00000080)
#define SMR_LOG_COMPRESSED_NO           (0x00000000) // ������� �α�
#define SMR_LOG_COMPRESSED_OK           (0x00000080) // ������ �α�

/* BUG-35392
 * dummy log ���� �Ǵ��ϱ� ���� ��� */
#define SMR_LOG_DUMMY_LOG_MASK          (0x00000100)
#define SMR_LOG_DUMMY_LOG_NO            (0x00000000) // ���� �α�
#define SMR_LOG_DUMMY_LOG_OK            (0x00000100) // ���� �α�

/* BUG-46854: RP ����(�����̸Ӹ� Ű) �α� �ۼ� ����  */
#define SMR_LOG_RP_INFO_LOG_MASK        (0x00000200)
#define SMR_LOG_RP_INFO_LOG_NO          (0x00000000) // �α� �� ��
#define SMR_LOG_RP_INFO_LOG_OK          (0x00000200) // �α� �� 

/* BUG-46854: CMPS �α� ����
 * delete�� �ѹ����� ��, redo �������� CMPS�α�(CLR)���� undo��.
 * �� ��� before flag�� �α׿� �ֱ� ������ delete version�� undo
 * �Լ����� CLR�� ���� delete undo ���� �ƴ��� �˾ƾ� �Ѵ�. */
#define SMR_LOG_CMPS_LOG_MASK           (0x00000400)
#define SMR_LOG_CMPS_LOG_NO             (0x00000000) // CMPS �α� �ƴ� 
#define SMR_LOG_CMPS_LOG_OK             (0x00000400) // CMPS �α�


/* BUG-48059 �������� 
 * smrLogHead.mFlag �� �����ϴ� flag �ƴ�
 * GCTX Group CommitSCN �� �����ߴ��� �����ϱ� ���� �뵵 
 * SMR_LOG_TYPE_XXX �� �����Ǹ� �� */
#define SMR_LOG_COMMITSCN_MASK          (0x00000004)
#define SMR_LOG_COMMITSCN_NO            (0x00000000) // CommitSCN �� ���ų� INIT �� ��� 
#define SMR_LOG_COMMITSCN_OK            (0x00000004) // GCTX �鼭 CommitSCN �� ����Ѱ��

/* PROJ-1527              */
/* smrLogHead ����ü ��� */
/*
   Tail �� Log Record ����ü �ȿ� ������ �α� ���ڵ��� ũ�� ���
   ======================================================================
   PROJ-1527�� ���� log�� 8����Ʈ align�� �ʿ䰡 ���ԵǾ�����
   ���� smrXXXLog�� ������ �� align�� ����� �ʿ䰡 ����.
   ������ 64��Ʈ ȯ�濡�� 8����Ʈ ����� ���� ����ü�� ũ���
   8�� ����� �������� ������ log�� ����� �� ID_SIZEOF��
   ����ϸ� �ȵȴ�. (���� �ʷ���)
   ���� �α׸� ����� �� smrXXXLog�� ������ ��������� ����ϸ�
   �Ǵµ� �� ������ ��������� ���̸� ���ϱ� ���� ��� smrXXXLog����
   mLogRecFence��� ���� ����� �����Ѵ�.
   SMR_LOGREC_SIZE�� smrXXXLog ����ü���� mLogRecFence �ձ����� ũ�⸦ ���Ѵ�.

   +----------+--------------------+----------+-------+-------+
   | Log Head | Log Body                      | Fence | Dummy |
   +----------+--------------------+----------+-------+-------+
                                              ^
                                              |
                                             size
   ���� head�� setSize�� �ϱ����ؼ��� ID_SIZEOF��� SMR_LOGREC_SIZE��
   ����Ѵ�.
   �׸��� �α׾��� ���̱� ���ؼ��� 8����Ʈ ����� ���ʿ�, ���� �������� �����
   ���ʿ� �δ°� �����ϴ�.
*/
#define SMR_LOGREC_SIZE(STRUCT_NAME) (offsetof(STRUCT_NAME,mLogRecFence))

/* BUG-35392
 * mFlag �� �߰� ������ �ֱ� ���� ����� UChar -> UInt��
 * �����ϰ�, ����� ������ �ٲ۴�. */
typedef struct smrLogHead
{
    /* mFlag�� ����α׿� �����α��� ���� Head�̴�. */
    UInt            mFlag;

    UInt            mSize;

    /* For Parallel Logging :
     * �αװ� ��ϵ� �� LSN(Log Sequence Number)
     * ���� ����� */
    smLSN           mLSN;

    /* �α����� ������ memset���� �ʾƼ� garbage�� �ö�͵�
     * Invalid�� Log�� Valid�� �α׷� �Ǻ��ϴ� Ȯ���� ���߱� ����
     * Magic Number .
     * �α����� ��ȣ�� �α׷��ڵ尡 ��ϵǴ� �������� �������� ���������. */
    smMagic         mMagic;

    smrLogType      mType;

    /* BUG-17033: �ֻ��� Statement�� �ƴ� Statment�� ���ؼ���
     * Partial Rollback�� �����ؾ� �մϴ�.
     *
     * Replication�� Statment�� Savepoint�� �����Ҷ� �̿��ؾ���
     * Implicit SVN Name Number. */
    UChar           mReplSvPNumber;

    smTID           mTransID;

    smLSN           mPrevUndoLSN;
} smrLogHead;

/* BUG-35392 */
#define SMR_LOG_FLAG_OFFSET     (offsetof(smrLogHead,mFlag))
#define SMR_LOG_LOGSIZE_OFFSET  (offsetof(smrLogHead,mSize))
#define SMR_LOG_LSN_OFFSET      (offsetof(smrLogHead,mLSN))
#define SMR_LOG_MAGIC_OFFSET    (offsetof(smrLogHead,mMagic))

typedef smrLogType smrLogTail;

#define SMR_DEF_LOG_SIZE (ID_SIZEOF(smrLogHead) + ID_SIZEOF(smrLogTail))

/* ------------------------------------------------
 * Operation Log Type
 * ----------------------------------------------*/
typedef enum
{
    SMR_OP_NULL,
    SMR_OP_SMM_PERS_LIST_ALLOC,
    SMR_OP_SMC_FIXED_SLOT_ALLOC,
    SMR_OP_CREATE_TABLE,
    SMR_OP_CREATE_INDEX,
    SMR_OP_DROP_INDEX,
    SMR_OP_INIT_INDEX,
    SMR_OP_SMC_FIXED_SLOT_FREE, /* BUG-31062 �� ���Ͽ� ���ŵǾ�����*/
    SMR_OP_SMC_VAR_SLOT_FREE,   /* ���� Log Type�� ���� �����ϱ� ���� ���ܵ�*/
    SMR_OP_ALTER_TABLE,
    SMR_OP_SMM_CREATE_TBS,
    SMR_OP_INSTANT_AGING_AT_ALTER_TABLE,
    SMR_OP_SMC_TABLEHEADER_ALLOC,
    SMR_OP_DIRECT_PATH_INSERT,
    SMR_OP_MAX
} smrOPType;


/* ------------------------------------------------
 * FOR A4 : DRDB�� �α� Ÿ��
 *
 * 1) Ư�� page�� ���� physical or logical redo �α�Ÿ��
 * smrLogType���� SMR_DLT_REDO Ÿ���� ������, redo ����
 * Ÿ���̴�.
 *
 * 2) undo record ��Ͽ� ���� redo-undo �α� Ÿ��
 * smrLogType����SMR_DLT_UNDORECŸ���� ������, redo ��
 * undo ������ Ÿ���̴�.
 * ----------------------------------------------*/
typedef struct smrDiskLog
{
    smrLogHead  mHead;        /* �α� header */
    smOID       mTableOID;
    UInt        mRefOffset;   /* disk log buffer���� DML����
                                 redo/undo �α� ��ġ */
    UInt        mContType;    /* �αװ� ������������ �ƴ��� ���� */
    UInt        mRedoLogSize; /* redo log ũ�� */
    UInt        mRedoType;    /* runtime memory ����Ÿ ���Կ��� */
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDiskLog;


/* ------------------------------------------------
 * PROJ-1566 : disk NTA �α�
 * < extent �Ҵ���� �� >
 * - mData1 : segment RID
 * - mData2 : extent RID
 * < page list�� meta�� page list�� �߰��� �� >
 * - mData1 : table OID
 * - mData2 : tail Page PID
 * - mData3 : ������ page�� ��, table type�� page ����
 * - mData4 : ������ ��� page ���� ( multiple, external ���� )
 * ----------------------------------------------*/
typedef struct smrDiskNTALog
{
    smrLogHead  mHead;        /* �α� header */
    ULong       mData[ SM_DISK_NTALOG_DATA_COUNT ] ;
    UInt        mDataCount;   /* Data ���� */
    UInt        mOPType;      /* operation NTA Ÿ�� */
    UInt        mRedoLogSize; /* redo log ũ�� */
    scPageID    mSpaceID;     /* SpaceID */
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDiskNTALog;

/* ------------------------------------------------
 * PROJ-1704 : MVCC Renewal
 * ----------------------------------------------*/
typedef struct smrDiskRefNTALog
{
    smrLogHead  mHead;        /* �α� header */
    UInt        mOPType;      /* operation NTA Ÿ�� */
    UInt        mRefOffset;   /* disk log buffer���� index����
                                 logical undo �α� ��ġ */
    UInt        mRedoLogSize; /* redo log ũ�� */
    scPageID    mSpaceID;     /* SpaceID */
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDiskRefNTALog;

/* ------------------------------------------------
 * disk CLR �α�
 * smrLogType���� SMR_DLT_COMPENSATION
 * ----------------------------------------------*/
typedef struct smrDiskCMPSLog
{
    smrLogHead      mHead;        /* �α� header */
    UInt            mRedoLogSize; /* redo log ũ�� */
    UChar           mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDiskCMPSLog;

/* ------------------------------------------------
 * PROJ-1867 Disk Page Img Log
 * DPath-Insert�� Page Img Log�� �⺻ ������ ����.
 * ----------------------------------------------*/
typedef struct smrDiskPILog
{
    smrLogHead  mHead;        /* �α� header */
    smOID       mTableOID;
    UInt        mRefOffset;   /* disk log buffer���� DML����
                                 redo/undo �α� ��ġ */
    UInt        mContType;    /* �αװ� ������������ �ƴ��� ���� */
    UInt        mRedoLogSize; /* redo log ũ�� */
    UInt        mRedoType;    /* runtime memory ����Ÿ ���Կ��� */

    sdrLogHdr   mDiskLogHdr;

    UChar       mPage[SD_PAGE_SIZE]; /* page */
    smrLogTail  mTail;
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDiskPILog;

/* ------------------------------------------------
 * PROJ-1864 Page consistent log
 * ----------------------------------------------*/
typedef struct smrPageConsistentLog
{
    smrLogHead  mHead;        /* �α� header */
    smOID       mTableOID;
    UInt        mRefOffset;   /* disk log buffer���� DML����
                                 redo/undo �α� ��ġ */
    UInt        mContType;    /* �αװ� ������������ �ƴ��� ���� */
    UInt        mRedoLogSize; /* redo log ũ�� */
    UInt        mRedoType;    /* runtime memory ����Ÿ ���Կ��� */

    sdrLogHdr   mDiskLogHdr;

    UChar       mPageConsistent; /* page consistent */
    smrLogTail  mTail;
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrPageCinsistentLog;

/* ------------------------------------------------
 * ���̺����̽� UPDATE�� ���� �α� BUG-9640
 * ----------------------------------------------*/
typedef struct smrTBSUptLog
{
    smrLogHead  mHead;        /* �α� header */
    scSpaceID   mSpaceID;     /* �ش� tablespace ID */
    UInt        mFileID;
    UInt        mTBSUptType;  /* file ���� Ÿ�� */
    SInt        mAImgSize;    /* after image ũ�� */
    SInt        mBImgSize;    /* before image ũ�� */
    UChar       mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTBSUptLog;

/* ------------------------------------------------
 *  NTA log
 * ----------------------------------------------*/
typedef struct smrNTALog
{
    smrLogHead mHead;
    scSpaceID  mSpaceID;
    ULong      mData1;
    ULong      mData2;
    smrOPType  mOPType;
    smrLogTail mTail;
    UChar      mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrNTALog;

/*---------------------------------------------------------------
 * FOR A4 :  checkpoint log
 * DRDB�� recovery LSN�� ����ؾ���
 *---------------------------------------------------------------*/
typedef struct smrBeginChkptLog
{
    smrLogHead     mHead;
    /* redoall�������� MRDB�� recovery lsn */
    smLSN          mEndLSN;
    /* redoall�������� DRDB�� recovery lsn */
    smLSN          mDiskRedoLSN;
    /* PROJ-2569 �̿Ϸ�л�Ʈ����� �α׿� mDiskRedoLSN �߿��� ���� ������ redo���� */
    smLSN          mDtxMinLSN;
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrBeginChkptLog;

typedef struct smrEndChkptLog
{
    smrLogHead     mHead;
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrEndChkptLog;

/*---------------------------------------------------------------*/
//  transaction log
//
/*---------------------------------------------------------------*/

/* PROJ-1553 Replication self-deadlock */
/* undo log�� abort log�� ��� ���� pre-abort log�� ��´�. */
typedef struct smrTransPreAbortLog
{
    smrLogHead     mHead;
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTransPreAbortLog;

typedef struct smrTransAbortLog
{
    smrLogHead     mHead;
    UInt           mDskRedoSize; /* Abort Log�� Disk �� ��� TSS ������ */
    UInt           mGlobalTxId;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTransAbortLog;

typedef struct smrTransCommitLog // ��ũ/�޸� ����
{
    smrLogHead     mHead;
    UInt           mTxCommitTV;
    UInt           mDskRedoSize; /* Commit Log�� Disk �� ��� TSS ������ */
    UInt           mGlobalTxId;
    smSCN          mCommitSCN;   /* Global Trans�� commitSCN */
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTransCommitLog;

typedef struct smrTransMemCommitLog // �޸𸮿�  // smrTransCommitLog �� ������ ����. �� ��������.
{
    smrLogHead     mHead;
    UInt           mTxCommitTV;
    UInt           mDskRedoSize; /* Commit Log�� Disk �� ��� TSS ������ */
    UInt           mGlobalTxId;
    smSCN          mCommitSCN;   /* Global Trans�� commitSCN */
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTransMemCommitLog;

typedef struct smrTransGroupCommitLog
{
    smrLogHead     mHead;
    UInt           mTxCommitTV;
    UInt           mGroupCnt;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTransGroupCommitLog;

#define SMR_LOG_GROUP_COMMIT_GROUPCNT_OFFSET   (offsetof(smrTransGroupCommitLog,mGroupCnt))

/*--------------------------
    For Global Transaction
  --------------------------*/
typedef struct smrXaPrepareLog
{
    smrLogHead     mHead;
    /* BUG-18981 */
    ID_XID         mXaTransID;
    UInt           mLockCount;
    idBool         mIsGCTx;        /* PROJ-2733 */
    timeval        mPreparedTime;
    smSCN          mFstDskViewSCN; /* XA Trans�� FstDskViewSCN */
    UChar          mLogRecFence;   /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrXaPrepareLog;

/* BUG-2569 */
#define SM_INIT_GTX_ID ( ID_UINT_MAX )

typedef struct smrXaStartReqLog
{
    smrLogHead     mHead;
    ID_XID         mXID;         /* PROJ-2747 Global Tx Consistent */
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrXaStartReqLog;

typedef struct smrXaPrepareReqLog
{
    smrLogHead     mHead;
    ID_XID         mXID;         /* PROJ-2747 Global Tx Consistent */
    UInt           mGlobalTxId;
    UInt           mBranchTxSize;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrXaPrepareReqLog;

typedef struct smrXaEndLog
{
    smrLogHead     mHead;
    UInt           mGlobalTxId;
    smrLogTail     mTail;
    UChar          mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrXaEndLog;

/*
 * PROJ-1704 Disk MVCC ������
 * Prepare Ʈ������� ����ϴ� Ʈ����� ���׸�Ʈ�� ���� �α�
 */
typedef struct smrXaSegsLog
{
    smrLogHead     mHead;
    ID_XID         mXaTransID;
    UInt           mTxSegEntryIdx;  /* ����ߴ� Ʈ����� ���׸�Ʈ ��Ʈ������ */
    sdRID          mExtRID4TSS;     /* TSS�� ������ ExtRID */
    scPageID       mFstPIDOfLstExt4TSS; /* TSS�� �Ҵ��� Ext�� ù��° ������ID */
    sdRID          mFstExtRID4UDS;  /* ó�� ����ߴ� Undo ExtRID */
    sdRID          mLstExtRID4UDS;  /* ������ ����ߴ� Undo ExtRID */
    scPageID       mFstPIDOfLstExt4UDS; /* ������ Undo Ext�� ù��° ������ID */
    scPageID       mFstUndoPID;     /* ������ ����ߴ� TSS PID */
    scPageID       mLstUndoPID;     /* ������ ����ߴ� Undo PID */
    smrLogTail     mTail;
    UChar          mLogRecFence;    /* log record�� ũ�⸦ ���ϱ� ���� ��� */
} smrXaSegsLog;

/*---------------------------------------------------------------*/
//  Log File Begin & End Log
//
/*---------------------------------------------------------------*/

/*
 * To Fix BUG-11450  LOG_DIR, ARCHIVE_DIR �� ������Ƽ ������ ����Ǹ�
 *                   DB�� ����
 *
 * �α�Ÿ�� : SMR_LT_FILE_BEGIN
 */

typedef struct smrFileBeginLog
{
    smrLogHead    mHead;
    UInt          mFileNo;   // �α������� ��ȣ
    smrLogTail    mTail;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrFileBeginLog;

typedef struct smrFileEndLog
{
    smrLogHead    mHead;
    smrLogTail    mTail;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrFileEndLog;

/*---------------------------------------------------------------*/
//  Dummy Log
//  � Undo Redo����� ���� �ʴ´�. �ڼ��� ������
//  1. smiReadLogByOrder::initialize
//  2. smrLogMgr :: getRestartRedoLSN
//  �� �����ϱ� �ٶ���.
/*---------------------------------------------------------------*/
typedef struct smrDummyLog
{
    smrLogHead    mHead;
    smrLogTail    mTail;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDummyLog;

/*---------------------------------------------------------------*/
//  update log
//
/*---------------------------------------------------------------*/
typedef struct smrUpdateLog
{
    smrLogHead         mHead;
    vULong             mData;
    scGRID             mGRID;
    SInt               mAImgSize;
    SInt               mBImgSize;
    smrUpdateType      mType;
    UChar              mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrUpdateLog;

/* ------------------------------------------------
 * compensation log BUG-12399 CASE-3893
 * ----------------------------------------------*/
typedef struct smrCMPSLog
{
    smrLogHead    mHead;
    scGRID        mGRID;
    UInt          mFileID;
    UInt          mTBSUptType;  /* file ���� Ÿ�� */
    vULong        mData;
    smrUpdateType mUpdateType;
    SInt          mBImgSize;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrCMPSLog;

// PROJ-1362.
typedef struct smrLobLog
{
    smrLogHead    mHead;
    smLobLocator  mLocator;
    UChar         mOpType;     // lob operation code
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
}smrLobLog;

/* PROJ-1442 Replication Online �� DDL ��� */
typedef struct smrDDLLog
{
    smrLogHead    mHead;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrDDLLog;

typedef struct smrTableMeta
{
    /* Table Identifier */
    UInt          mTableID;
    vULong        mTableOID;
    vULong        mOldTableOID;
    UInt          mTBSType; 
    SChar         mRepName[SM_MAX_NAME_LEN + 1 + 7];    // 7 Byte�� Dummy
    SChar         mUserName[SM_MAX_NAME_LEN + 1 + 7];   // 7 Byte�� Dummy
    SChar         mTableName[SM_MAX_NAME_LEN + 1 + 7];  // 7 Byte�� Dummy
    SChar         mPartName[SM_MAX_NAME_LEN + 1 + 7];   // 7 Byte�� Dummy
    SChar         mRemoteUserName[SM_MAX_NAME_LEN + 1 + 7];   // 7 Byte�� Dummy
    SChar         mRemoteTableName[SM_MAX_NAME_LEN + 1 + 7];  // 7 Byte�� Dummy
    SChar         mRemotePartName[SM_MAX_NAME_LEN + 1 + 7];   // 7 Byte�� Dummy

    /* Primary Key Index ID */
    UInt          mPKIndexID;
} smrTableMeta;

typedef struct smrTableMetaLog
{
    smrLogHead    mHead;
    smrTableMeta  mTableMeta;
    UChar         mLogRecFence; /* log record�� ũ�⸦ ���ϱ� ���� ���� */
} smrTableMetaLog;

/* ------------------------------------------------
 * PROJ-1723
 * ----------------------------------------------*/
typedef struct smrDDLStmtMeta
{
    SChar          mUserName[SM_MAX_NAME_LEN + 1 + 7];
    SChar          mTableName[SM_MAX_NAME_LEN + 1 + 7];
} smrDDLStmtMeta;

typedef struct smrXaLog
{
    ID_XID mXID;
} smrXaLog;

/*---------------------------------------------------------------
 * - loganchor ����ü
 * ���� Loganchor�� �����ϵ��� ����
 * �߰��� �κ��� checksum�� DRDB�� tablespace ������ ����
 *---------------------------------------------------------------*/
typedef struct smrLogAnchor
{
    UInt             mCheckSum;

    smLSN            mBeginChkptLSN;
    smLSN            mEndChkptLSN;

    /* BEGIN CHKPT �α׿��� ����Ǹ�, ���� üũ����Ʈ(DRDB)�ÿ���
     * ������ BEGIN CHKPT �α׸� ������� �����Ƿ� �ٷ� Loganchor����
     * �����Ѵ�. Restart Recovery �������� BEGIN CHKPT �α׿� ����� �Ͱ�
     * Loganchor�� ����� ���� ���Ͽ� �� ū ���� �����Ѵ�. */
    smLSN            mDiskRedoLSN;

    /*
     * mMemEndLSN            : Restart Redo Point
     * mLstCreatedLogFileNo  : ���������� ������ LogFile No
     * mFstDeleteFileNo      : ������ ù��° LogFile No
     * mLstDeleteFileNo      : ������ ������ LogFile No
     * mResetLSN             : ��Ϸ� Recovery�� mResetLogs�� ����Ű�� ����
     *                         log�� ������.
     */
    smLSN            mMemEndLSN;
    UInt             mLstCreatedLogFileNo;
    UInt             mFstDeleteFileNo;
    UInt             mLstDeleteFileNo;
    smLSN            mMediaRecoveryLSN;

    smLSN            mResetLSN;

    smrServerStatus  mServerStatus;

    smiArchiveMode   mArchiveMode;

    /* PROJ-1704 Disk MVCC ������
     * Ʈ����� ���׸�Ʈ ��Ʈ�� ���� */
    UInt             mTXSEGEntryCnt;

    UInt             mNewTableSpaceID;

    UInt             mSmVersionID;
    /* proj-1608 recovery from replication */
    smLSN            mReplRecoveryLSN;

    /* change in run-time */
    ULong            mUpdateAndFlushCount; /* Loganchor ���Ͽ� Flush�� Ƚ�� */

    /* PROJ-2133 incremental backup */
    smriCTFileAttr   mCTFileAttr;
    smriBIFileAttr   mBIFileAttr;

    /* PROJ-1497 DB Migration�� �����ϱ� ���� Reserved ������ �߰� */
    ULong            mReserveArea[SMR_LOGANCHOR_RESERV_SIZE];
} smrLogAnchor;

/*---------------------------------------------------------------*/
//  For Savepoint Log
//
/*---------------------------------------------------------------*/
#define SMR_IMPLICIT_SVP_NAME           "$$IMPLICIT"
#define SMR_IMPLICIT_SVP_NAME_SIZE      (10)

/* checkpoint reason */
#define SMR_CHECKPOINT_BY_SYSTEM           (0)
#define SMR_CHECKPOINT_BY_LOGFILE_SWITCH   (1)
#define SMR_CHECKPOINT_BY_TIME             (2)
#define SMR_CHECKPOINT_BY_USER             (3)

typedef struct smrArchLogFile
{
    UInt               mFileNo;
    smrArchLogFile    *mArchNxtLogFile;
    smrArchLogFile    *mArchPrvLogFile;
} smrArchLogFile;

/* BUG-14778 Tx�� Log Buffer�� �������̽��� ����ؾ� �մϴ�.
 *
 * Code Refactoring�� ���ؼ� �߰� �Ǿ����ϴ�. After Image��
 * Before Image�� ���� ������ ǥ���ϴµ� log����Լ��� ��������
 * After Image, Before Image�� ���� ������ �ѱ�� ���ؼ�
 * ���˴ϴ�.
 *
 * log����Լ����� image����, smrUptLogImgInfo mArr[image����]
 * �� ������ After , Before Image�� ���� ������ �Ѿ�ϴ�.
 *
 * */
typedef struct smrUptLogImgInfo
{
    SChar *mLogImg; /* Log Image�� ���� Pointer */
    UInt   mSize;   /* Image size(byte) */
} smrUptLogImgInfo;

/* Log Flush�� ��û�ϴ� ��츦 �з� */
typedef enum
{
    /* Transaction */
    SMR_LOG_SYNC_BY_TRX = 0,
    /* Log Flush Thread */
    SMR_LOG_SYNC_BY_LFT,
    /* Buffer Flush Thread */
    SMR_LOG_SYNC_BY_BFT,
    /* Checkpoint Thread */
    SMR_LOG_SYNC_BY_CKP,
    /* Refine */
    SMR_LOG_SYNC_BY_REF,
    /* Other */
    SMR_LOG_SYNC_BY_SYS
} smrSyncByWho;

/* TASK-2398 Log Compress
   Disk Log�� ����ϴ� �Լ��鿡 aWriteOption���ڿ� �Ѿ Flag
*/
// �α� ���࿩��
#define SMR_DISK_LOG_WRITE_OP_COMPRESS_MASK  (0x01)
#define SMR_DISK_LOG_WRITE_OP_COMPRESS_FALSE (0x00)
#define SMR_DISK_LOG_WRITE_OP_COMPRESS_TRUE  (0x01)

#define SMR_DEFAULT_DISK_LOG_WRITE_OPTION SMR_DISK_LOG_WRITE_OP_COMPRESS_TRUE

/* �α� ���࿡ �ʿ��� ���ҽ� */
typedef struct smrCompRes
{
    // ���������� ���� �ð�
    // ���������� ���� �ð��� ����ð��� ���̰� Ư�� �ð��� �����
    // Pool���� �����ϱ� ���� �뵵�� ���ȴ�.
    idvTime                mLastUsedTime;

    // Ʈ����� rollback�� �α� ���� ������ ���� ������ �ڵ�
    iduReusedMemoryHandle  mDecompBufferHandle;

    // �α� ���࿡ ����� ���� ����
    iduReusedMemoryHandle  mCompBufferHandle;

} smrCompRes;

// smrLSN4Union�� atomic �ϰ�  mSync�� ������ mLstLSN.mFileNo �� mLstLSN. mOffset�� �����ö� ���
typedef struct smrLSN4Union
{
    union
    {
        ULong mSync; // For 8 Byte Align
        smLSN mLSN;
    } ;
} smrLSN4Union;

/********** BUG-35392 **********/
// FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1 �� ��� dummyLog �� �߻��Ҽ� �ִµ� 
// ��쿡 ���� dummy �� �������� �ʴ� �α׸� ���ؾ� �Ѵ�. 
// �α׸� ����� ���� LstLSN �� LstWriteLSN �� �����ϰ� setFstCheckLSN()  
// �ֱ������� LstLSN �� LstWriteLSN �� ���� ���� ���� �����ϸ� rebuildMinUCSN()
// �ش簪�� �������� Log�� �ƴ� (Ȥ�� ���̰� �ƴ�) ���Ⱑ ����� LogLSN ��.
//
//  -----------------------------------------------------
//   LSN    | 100 | 101 | 102   | 103 | 104   | 105 | 106 |
//   Status | ok  | ok  | dummy | ok  | dummy | ok  | ok  |
//  ------------- A --- B-------------------------- C --- D --
// 
// smrUncompletedLogInfo.mLstWriteLSN (A) : ���̸� �������� �ʴ� ������ �α� ���ڵ��� LSN
// smrUncompletedLogInfo.mLstLSN (B)      : ���̸� �������� �ʴ� ������ �α� ���ڵ��� Offset
// mLstWriteLSN (C) : ������ �α� ���ڵ��� LSN, FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���� ���� 
// mLstLSN (D)      : ������ �α� ���ڵ��� Offset, FAST_UNLOCK_LOG_ALLOC_MUTEXT = 1�� ��� ���� ����
// 
// A,B,C,D ��.. 
// A �� B , C�� D �� ���� ���� �α��� ���۰� ��.
// A <= C , B <= D �� �����ȴ�. 

typedef struct smrUncompletedLogInfo
{
    smrLSN4Union mLstLSN;
    smrLSN4Union mLstWriteLSN;  
} smrUncompletedLogInfo;

#define SM_SYNC_LSN_MAX( mSync )     (mSync = ID_ULONG_MAX)

#define SM_IS_SYNC_LSN_MAX( mSync )  (mSync == ID_ULONG_MAX)

/********** END **********/


/* TASK-4007 [SM] PBT�� ���� ��� �߰�
 * Log�� Dump�Ҷ�, � �α׸� ��� ��������� ������ �ƴ� Dump�ϰ��� �ϴ� 
 * Util���� �����ϵ���, �о�帰 �α׿� ���� ó���� Callback�Լ��� �д�.*/

typedef IDE_RC (*smrLogDump)(UInt         aOffsetInFile,
                             smrLogHead * aLogHead,
                             idBool       aIsCompressed,
                             SChar      * aLogPtr );

/************************************************************************
 * PROJ-2162 RestartRiskReduction
 *
 *  - Recovery Target Object Info (RTOI) ����
 * Recovery ��� ��ü�� Inconsistent�� ��Ȳ���� Recovery�� �õ��� ��� �
 * ������ �߻����� �𸥴�. ���� �̸� Log�� �������� Targetobject��
 * Consistency�� �˻��Ѵ�.
 *  ���� ���� Recovery ������ �������� ���, Recovery ��� ��ü�� Inconsistent
 * Flag�� �����Ͽ� ���� Recovery�� �����ϴ� ���� �����ش�.
 * Recovery ��� ��ü�� Table, Index, MemPage, DiskPage, �װ����̴�. 
 * ��� TBSID, PID, OFFSET, �� GRID�� ������ ǥ���Ѵ�. 
 *
 * - InconsistentObjectList (IOL)
 *  RTOI�� Inconsistent�ϴٴ� ���� �����Ǿ��� ���, RecoveryMgr�� IOL��
 * ���̰� �ȴ�.
 *
 ***********************************************************************/

/* TOI ����:
 * <INIT> �ʱ�ȭ��
 *    |
 * [prepareRTOI               Log�� ���� ��� Object�� ������ ]
 *    |                       
 * <PREPARED>                      
 *    |                       
 * [checkObjectConsistency    Object�˻���]
 *    |                       
 * <CHECKED>                  
 *    |                       
 * [setObjectInconsistency    Object�� ���� ������ ����]
 *    |
 * <DONE>
 *
 * ��, prepareRTOI �Լ����� Log�� ���� CHECKED, �Ǵ� DONE ���·� �ٷ� 
 * ������ ��찡 ����. (�ش� �Լ� ���� ) */

typedef enum
{
    SMR_RTOI_STATE_INIT,
    SMR_RTOI_STATE_PREPARED,/* prepareRTOI�� ���� Log�� �м���*/
    SMR_RTOI_STATE_CHECKED, /* checkObjectConsistency�� ���� ��ü���� ����*/
    SMR_RTOI_STATE_DONE     /* ó�� ����*/
} smrRTOIState;

/* TOI�� ����ϰ� �� ���� */
typedef enum
{
    SMR_RTOI_CAUSE_INIT,
    SMR_RTOI_CAUSE_OBJECT,   /* ��ü(Table,index,Page)���� �̻���*/
    SMR_RTOI_CAUSE_REDO,     /* RedoRecovery���� �������� */
    SMR_RTOI_CAUSE_UNDO,     /* UndoRecovery���� �������� */
    SMR_RTOI_CAUSE_REFINE,   /* Refine���� �������� */
    SMR_RTOI_CAUSE_PROPERTY  /* Property�� ���� ������ ���ܵ�  */
} smrRTOICause;

/* TOI�� ��ϵ� ��� Object�� ����  */
typedef enum
{
    SMR_RTOI_TYPE_INIT,
    SMR_RTOI_TYPE_TABLE,
    SMR_RTOI_TYPE_INDEX,
    SMR_RTOI_TYPE_MEMPAGE,
    SMR_RTOI_TYPE_DISKPAGE
} smrRTOIType;

/* PROJ-2162 RestartRiskReduction
 * Recovery ��� ��ü���Դϴ�. Log�κ���, �Ǵ� DRDBPage�κ��� �����մϴ�.
 * �� �ڷᱸ���� �������� �߸��� ��ü���� List�� ��Ƶξ� �������� �����
 * �ݴϴ�.*/
typedef struct smrRTOI /* RecoveryTargetObjectInfo */
{
    /* TOI�� �� ���� ��ü */
    smrRTOICause   mCause;
    smLSN          mCauseLSN;
    UChar        * mCauseDiskPage;

    /* TOI�� ���� */
    smrRTOIState   mState;

    /* TOI�� ���� ���� */
    smrRTOIType    mType;
    scGRID         mGRID;

    /* IndexCommonPersistentHeader���� ��� Alter���� ���� ��ġ�� ����
     * �� �� �ֱ� ������, TableHeader, IndexID�� ������ �־�� ��
     * checkObjectConsistency�� ���� ������*/
    smOID          mTableOID;
    UInt           mIndexID;

    smrRTOI      * mNext;
} smrRTOI;

/*FixedTable����� ���� �� */
typedef struct smrRTOI4FT
{
    SChar      mType[9];  // INIT,TABLE,INDEX,MEMPAGE,DISKPAGE
    SChar      mCause[9]; // INIT,OBJECT,REDO,UNDO,REFINE,PROPERTY
    ULong      mData1;
    ULong      mData2;
} smrRTOI4FT;

typedef enum
{
    SMR_RECOVERY_NORMAL    = 0, // �⺻. ��� ������ �������� �ʽ��ϴ�
    SMR_RECOVERY_EMERGENCY = 1, // �߸��� ��ü, �߸��� Redo�� ���� ������ 
                                // �����鼭 ������ ������ŵ�ϴ�.
    SMR_RECOVERY_SKIP      = 2  // Recovery�� ���� �ʽ��ϴ�. ������ DB�� 
                                // Inconsistency�����ϴ�.
} smrEmergencyRecoveryPolicy;

/* iSQL, TRC �α׷� ���� �޽��� ������ ���� ���� ũ�� */
#define SMR_MESSAGE_BUFFER_SIZE (512)

#endif /* _O_SMR_DEF_H_ */
