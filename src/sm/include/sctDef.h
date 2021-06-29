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
 * $Id: sctDef.h 17773 2006-08-28 01:18:54Z raysiasd $
 *
 * Description :
 *
 * TableSpace Manager �ڷ� ����
 *
 *
 **********************************************************************/

#ifndef _O_SCT_DEF_H_
#define _O_SCT_DEF_H_ 1

#include <smDef.h>
#include <smriDef.h>


// LOGANCHOR�� ������� ���� TBS/DBF Attribute��
// �� ���� ����
# define  SCT_UNSAVED_ATTRIBUTE_OFFSET (0)


#define SCT_INIT_LOCKHIER( aLockHier ) \
{                                      \
    ((sctLockHier*)(aLockHier))->mTBSListSlot = NULL;    \
    ((sctLockHier*)(aLockHier))->mTBSNodeSlot = NULL;    \
}

/*
  Memory/ Disk Tablespace�� �������� ������ ����

[ PROJ-1548 User Memory Tablespace ]

- mStatus : TBS�� ���� ���¸� ����
  - Disk/Memory Tablespace��� ���� ������� ó���Ԥ�
  - Values
    - Creating :
      - TBS�� CREATE�ϴ� TX�� ���� COMMIT���� ���� ����
    - Online :
      - TBS�� CREATE�ϴ� TX�� COMMIT�� ����
    - Offline :
    - Dropping :
      - TBS�� DROP�ϴ� TX�� ���� COMMIT���� ���� ����
    - Dropped :
      - TBS�� DROP�ϴ� TX�� ���� COMMIT�� ����
  - ��������
      - �ϳ��� Tablespace�� ���´� ������ ���� ���̵ȴ�.
      (  ==>�� Commit Pending �۾��� ���� ���̵��� ���� )

      Creating ==> Online <-> Offline
                     \         |
                      \        +----> Offline | Dropping ==> Dropped
                       \
                        \-> Online | Dropping ==> Dropped

- SyncMutex
  - �ش� Tablespace���� Dirty Page�� Disk�� Sync(Flush)�� ��
    ��� Mutex�̴�.
  - Checkpoint�� Drop Tablespace���� ���ü���� ����Ѵ�.
    - Checkpoint�� �� Mutex�� ��� Tablespace�� Status�� �˻��Ѵ�.
    - Drop Tablespace�� �����ϴ� Tx�� �� Mutex�� ���
      Tablespace�� Status�� �����Ѵ�.
      - ���� Tablespace���� Memory������ ���ϻ����� �� Mutex��
        ���� ���� ä�� �����Ѵ�.
        ( Checkpoint�� Mutex��� Tablespace�� ���¸� �˻��ϱ� ����)
    - ����
      - Checkpoint���� Tablespace�� ���� Drop Tablespace Tx��
        ��ٸ����� �Ѵ�.
      - Drop���� Tablespace�� ���� Checkpoint�� ��ٸ����� �Ѵ�.
*/

typedef struct sctTableSpaceNode
{
    // ���̺����̽� ���̵�
    scSpaceID            mID;
    // ���̺����̽� ����
    // User || System, Disk || Memory, Data || Temp || Undo
    smiTableSpaceType    mType;

    // ���̺����̽� �̸�
    SChar               *mName;
    // ���̺����̽� ����(Creating, Droppping, Online, Offline...)
    UInt                 mState;
    // ���̺����̽��� Page�� Disk�� Sync�ϱ� ���� ��ƾ� �ϴ� Mutex
    iduMutex             mSyncMutex;
    // ���̺����̽��� ���� Commit-Duration lock item
    void                *mLockItem4TBS;

    /* BUG-18279: Drop Table Space�ÿ� ������ Table�� ���߸���
     *            Drop�� ����˴ϴ�.
     *
     * ���̺��� ������ Transaction�� CommitSCN�� �����ȴ�.
     * �̰��� Server ���۽ÿ� 0���� �ʱ�ȭ�ȴ�.
     * (�� Tablespace�� DDL�� �ϴ� Transaction�� ViewSCN��
     * �׻� �� SCN���� ���� �ʾƾ� �Ѵ�. ������ Rebuild������
     * �ø���. */
    smSCN                mMaxTblDDLCommitSCN;

    iduMutex             mMutex;    // Table Space�� ���º���, FileNode�� ���º��� ��ȣ
    iduCond              mBackupCV; // Backup ���  CV
} sctTableSpaceNode;


// ���̺����̽�����Ʈ, ���̺����̽���忡
// ���� ��ݽ��� ���� ����ü
typedef struct sctLockHier
{
    void *mTBSListSlot;
    void *mTBSNodeSlot;
} sctLockHier;

// X$TABLESPACES_HEADER���� ����ڿ��� ������ ����
typedef struct sctTbsHeaderInfo
{
    scSpaceID          mSpaceID;
    SChar             *mName;
    smiTableSpaceType  mType;
    // Tablespace ���������� bitset
    UInt               mStateBitset;
    // �������� ��� ���ڿ�
    UChar              mStateName[SM_DUMP_VALUE_LENGTH + 1];
    // Tablespace���� �����Ϳ� ���� Log�� ���� ����
    UInt               mAttrLogCompress;
} sctTbsHeaderInfo;

typedef struct sctTbsInfo
{
    scSpaceID          mSpaceID;
    sdFileID           mNewFileID;
    UChar              mExtMgmtName[20];
    UChar              mSegMgmtName[20];
    UInt               mDataFileCount;
    ULong              mTotalPageCount;
    UInt               mExtPageCount;
    // BUG-15564
    // mUsedPageLimit���� mAllocPageCount�� �̸��� �ٲ۴�.
    // ������� �������� �ƴ϶� �Ҵ�� ������ ���� ��Ÿ����
    // �����̴�.
    ULong              mAllocPageCount;

    /* BUG-18115 v$tablespaces�� page_size�� �޸� ��쵵 8192�� ���ɴϴ�.
     *
     * TableSpace���� PageSize�� �������� �����Ͽ����ϴ�.*/
    UInt               mPageSize;

    /* Free�� Extent�� Free Extent Pool�� �����ϴ� Extent���� */
    ULong              mCachedFreeExtCount;
} sctTbsInfo;

// To implement TASK-1842.
typedef enum sctPendingOpType
{
    SCT_POP_CREATE_TBS = 0,
    SCT_POP_DROP_TBS,
    SCT_POP_ALTER_TBS_ONLINE,
    SCT_POP_ALTER_TBS_OFFLINE,
    SCT_POP_CREATE_DBF,
    SCT_POP_DROP_DBF,
    SCT_POP_ALTER_DBF_RESIZE,
    SCT_POP_ALTER_DBF_ONLINE,
    SCT_POP_ALTER_DBF_OFFLINE,
    SCT_POP_UPDATE_SPACECACHE
} sctPendingOpType;

struct sctPendingOp;

// Disk �� Memory TablespaceƯ���� ���� �ٸ��� ó���ؾ� �ϴ�
// �۾��� ������ Pening Function Pointer
// ���� : sctTableSpaceMgr::executePendingOperation - �ּ�
typedef IDE_RC (*sctPendingOpFunc) ( idvSQL            * aStatistics,
                                     sctTableSpaceNode * aTBSNode,
                                     sctPendingOp      * aPendingOp );


/*
   To implement TASK-1842.

   PROJ-1548 User Memory Tablespace ---------------------------------------
   Disk/Memory Tablespace�� Disk DBF�� ���õ� Pending�۾��� ����
   ������ ���Ѵ�.
   Transaction�� �� ����ü�� ���� �� Linked List�� ������ �ִٰ�
   Commit�̳� Rollback�ÿ�
   sctTableSpaceMgr::executePendingOperation�� ȣ���Ͽ�
   Pending�� �۾��� �����Ѵ�.

   Ex> DROP TABLESPACE TBS1 �����
       Tablespace�� ���� ������ �����ϴ� Operation�� UNDO�� �Ұ��ϹǷ�
       Commit�ÿ� Pending���� ó���Ͽ��� �Ѵ�.
       "Tablespace�� ���� ������ ����"�ϴ� Pending Operation��
       sctPendingOp����ü�� �����Ͽ� Transaction�� �޾Ƴ��� �ִٰ�
       Transaction Commit�ÿ� sctPendingOp����ü�� ����
       Tablespace�� ���� ������ �����ϴ� �۾��� �����Ѵ�.
*/
typedef struct sctPendingOp
{
    scSpaceID        mSpaceID;
    sdFileID         mFileID;
    idBool           mIsCommit;        /* ���� �ñ�(commit or abort)�� ���� */
    smiTouchMode     mTouchMode;       /* ������ ������ ���� ���� ���� */
    SLong            mResizePageSize;  /* ALTER RESIZE ����� ���淮   */
    ULong            mResizeSizeWanted;/* ALTER RESIZE �����
                                        * ������ ���� ����ũ��   */
    UInt             mNewTBSState;     /* pending�� ������ TBS ���°�  */
    UInt             mNewDBFState;     /* pending�� ������ DBF ���°�  */
    smLSN            mOnlineTBSLSN;    /* online TBS �α��� Begin LSN */
    sctPendingOpType mPendingOpType;
    sctPendingOpFunc mPendingOpFunc;
    void*            mPendingOpParam;  /* Pending Parameter */
} sctPendingOp;

// PRJ-1548 User Memory Tablespace
// ���̺����̽� DDL���� ���̺����̽� �α��� Update Ÿ��
// SDD ��⿡ ���ǵǾ� �־�����, �޸� ���̺����̽���
// �����Ǿ�� �ϴ� Ÿ���̱� ������ SCT ��⿡ ������ �Ѵ�
typedef enum sctUpdateType
{
    SCT_UPDATE_MRDB_CREATE_TBS = 0,      // DDL
    SCT_UPDATE_MRDB_CREATE_CIMAGE_FILE,  // DDL
    SCT_UPDATE_MRDB_DROP_TBS,            // DDL
    SCT_UPDATE_MRDB_ALTER_AUTOEXTEND,    // DDL
    SCT_UPDATE_MRDB_ALTER_TBS_ONLINE,    // DDL
    SCT_UPDATE_MRDB_ALTER_TBS_OFFLINE,   // DDL
    SCT_UPDATE_DRDB_CREATE_TBS,          // DDL
    SCT_UPDATE_DRDB_DROP_TBS,            // DDL
    SCT_UPDATE_DRDB_ALTER_TBS_ONLINE,    // DDL
    SCT_UPDATE_DRDB_ALTER_TBS_OFFLINE,   // DDL
    SCT_UPDATE_DRDB_CREATE_DBF,          // DDL
    SCT_UPDATE_DRDB_DROP_DBF,            // DDL
    SCT_UPDATE_DRDB_EXTEND_DBF,          // DDL
    SCT_UPDATE_DRDB_SHRINK_DBF,          // DDL
    SCT_UPDATE_DRDB_AUTOEXTEND_DBF,      // DDL
    SCT_UPDATE_DRDB_ALTER_DBF_ONLINE,    // DDL
    SCT_UPDATE_DRDB_ALTER_DBF_OFFLINE,   // DDL
    SCT_UPDATE_VRDB_CREATE_TBS,          // DDL
    SCT_UPDATE_VRDB_DROP_TBS,            // DDL
    SCT_UPDATE_VRDB_ALTER_AUTOEXTEND,    // DDL
    SCT_UPDATE_COMMON_ALTER_ATTR_FLAG,   // DDL
    SCT_UPDATE_MAXMAX_TYPE
} sctUpdateType;

// PRJ-1548 User Memory Tablespace
// BACKUP ����������� ���̺����̽� doAction �Լ���
// ������ ���ڵ��� �����Ѵ�. [IN] ����
typedef struct sctActBackupArgs
{
    void          * mTrans;
    SChar         * mBackupDir;
    //PROJ-2133 incremental Backup
    smriBISlot    * mCommonBackupInfo;
    idBool          mIsIncrementalBackup;
} sctActBackupArgs;

// PRJ-1548 User Memory Tablespace
// ������������������ ����Ÿ���̽� ������ üũ����Ʈ
// ���������� ����Ÿ���̽� ������ doAction �Լ���
// ������ ���ڵ��� �����Ѵ�. [OUT] ����
typedef struct sctActIdentifyDBArgs
{
    idBool    mIsValidDBF;    // �̵�� ���� �ʿ俩��
    idBool    mIsFileExist;     // ����Ÿ������ ������
} sctActIdentifyDBArgs;

// PRJ-1548 User Memory Tablespace
// �޸� ���̺����̽� �̵�� �����Ϸ��������
// ������� �����Ҷ� �����ϴ� ����
typedef struct sctActRepairArgs
{
    smLSN*     mResetLogsLSN;
} sctActRepairArgs;

/*
   ������ TBS�� ���� Action�� �����ϴ� lockAndRun4EachTBS ���� ����ϴ�
   Function Type��
*/

// Action Function
typedef IDE_RC (*sctAction4TBS)( idvSQL            * aStatistics,
                                 sctTableSpaceNode * aTBSNode,                                                             void              * aActionArg);

typedef enum sctActionExecMode {
    SCT_ACT_MODE_NONE  = 0x0000,
    // Action�����ϴ� ���� lock() �� unlock() ȣ��
    SCT_ACT_MODE_LATCH = 0x0001
} sctActionExecMode ;


typedef enum sctStateSet {
    SCT_SS_INVALID         = 0x0000, /* �� ���� ���� ���� �� ���� �� */
    // Tablespace�� �ܰ躰 �ʱ�ȭ�� �����ؾ��ϴ� Tablespace�� ����
    // ( �ڼ��� ������ smmTBSMultiState.h�� ���� )
    //
    // ex> DROPPED, DISCARDED, ONLINE, OFFLINE ������ Tablespace��
    //     STATE�ܰ���� Tablespace�� �ʱ�ȭ�ؾ���
    SCT_SS_NEED_STATE_PHASE = (SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_ONLINE | SMI_TBS_OFFLINE ),
    SCT_SS_NEED_MEDIA_PHASE = ( SMI_TBS_DISCARDED | SMI_TBS_OFFLINE | SMI_TBS_ONLINE),
    SCT_SS_NEED_PAGE_PHASE  = ( SMI_TBS_ONLINE ),

    // �� ��⺰ ������ SKIP�� Tablespace�� ����
    // Tablespace�� �������� ����� ��� SMI_TBS_INCONSISTENT���°� �ȴ�.
    SCT_SS_SKIP_LOAD_FROM_ANCHOR = (SMI_TBS_DROPPED),
    SCT_SS_SKIP_REFINE     = ( SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE),
    // Drop Pending���൵�� ����� ���,
    // ��¥�� Drop �� Tablespace�̹Ƿ� Redo, Undo Skip��.
    // ��, Tablespace���� DDL�� Redo,Undo��.
    SCT_SS_SKIP_MEDIA_REDO = (SMI_TBS_INCONSISTENT | SMI_TBS_DROP_PENDING | SMI_TBS_DROPPED | SMI_TBS_DISCARDED ),
    SCT_SS_SKIP_REDO       = (SMI_TBS_INCONSISTENT | SMI_TBS_DROP_PENDING | SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),
    SCT_SS_SKIP_UNDO       = (SMI_TBS_INCONSISTENT | SMI_TBS_DROP_PENDING | SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),
    // Drop Pending���൵�� ����� ���,
    // Checkpoint Image�Ϻΰ� ���� �� �����Ƿ�, Prepare,Restore����
    SCT_SS_SKIP_PREPARE    = (SMI_TBS_INCONSISTENT | SMI_TBS_DROP_PENDING | SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),
    SCT_SS_SKIP_RESTORE    = (SMI_TBS_INCONSISTENT | SMI_TBS_DROP_PENDING | SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),
    // Drop Tablespace Pending�� �������� ��쿡��
    // Checkpoint�� Skip�Ѵ�.
    SCT_SS_SKIP_CHECKPOINT = (SMI_TBS_DROPPED | SMI_TBS_DROP_PENDING | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE),
    SCT_SS_SKIP_INDEXBUILD = (SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE),
    SCT_SS_SKIP_DROP_TABLE_CONTENT = ( SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE),
    /* PAGE�ܰ� destroy�� �����޸� Page�� Pool�� ������ Tablespace�� ����
     * BUG-30547 SMI_TBS_DROP_PENDING ���¿����� �����޸� ���� */
    SCT_SS_FREE_SHM_PAGE_ON_DESTROY = ( SMI_TBS_DROP_PENDING
                                        | SMI_TBS_DROPPED
                                        | SMI_TBS_OFFLINE ),
    // Drop ������ TableSpace ����
    SCT_SS_HAS_DROP_TABLESPACE = ( SMI_TBS_DISCARDED | SMI_TBS_OFFLINE | SMI_TBS_ONLINE ),
    // Media Recovery �Ұ����� TableSpace ����
    SCT_SS_UNABLE_MEDIA_RECOVERY = ( SMI_TBS_INCONSISTENT |  SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_DROP_PENDING ),
    // Checkpoint�� DBF ����� �������� �ʴ� ���
    SCT_SS_SKIP_UPDATE_DBFHDR = ( SMI_TBS_DROPPED | SMI_TBS_DROP_PENDING | SMI_TBS_DISCARDED ),
    // Identify Database �������� Check ���� ���� ���
    SCT_SS_SKIP_IDENTIFY_DB = ( SMI_TBS_INCONSISTENT | SMI_TBS_DROPPED | SMI_TBS_DROP_PENDING | SMI_TBS_DISCARDED ),
    SCT_SS_SKIP_SYNC_DISK_TBS = ( SMI_TBS_DROPPED | SMI_TBS_DISCARDED ),
    SCT_SS_SKIP_SHMUTIL_OPER = ( SMI_TBS_DROPPED | SMI_TBS_DROP_PENDING | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),
    SCT_SS_SKIP_COUNTING_TOTAL_PAGES = ( SMI_TBS_DROPPED | SMI_TBS_DROP_PENDING | SMI_TBS_DISCARDED ),
    // ������ ���̻� Valid ���� ���� Disk TBS ����
    SCT_SS_INVALID_DISK_TBS = ( SMI_TBS_DROPPED | SMI_TBS_DISCARDED ),
    // DISCARDED �� DISK TBS�� ���� Row Aging�� Skip�ϴ� ���
    SCT_SS_SKIP_AGING_DISK_TBS = ( SMI_TBS_DISCARDED ),
    // BUG-48500
    SCT_SS_SKIP_BUILD_FIXED_TABLE = ( SMI_TBS_DROPPED | SMI_TBS_DISCARDED | SMI_TBS_OFFLINE ),

    // fix BUG-8132
    // table/index segment free�� skip �Ѵ�.
    SCT_SS_SKIP_FREE_SEG_DISK_TBS = ( SMI_TBS_OFFLINE )
} sctStateSet;


// Tablespace�� Lock�� ȹ���� �� ������ Validation
typedef enum sctTBSLockValidOpt
{
    SCT_VAL_INVALID         = 0x0000, /* �� ���� ���� ���� �� ���� �� */
    SCT_VAL_CHECK_DROPPED   = 0x0001, /* TBS�� DROPPED�̸� ���� */
    SCT_VAL_CHECK_DISCARDED = 0x0002, /* TBS�� DROPPED�̸� ���� */
    SCT_VAL_CHECK_OFFLINE   = 0x0004, /* TBS�� DROPPED�̸� ���� */

    // Tablespace��ü�� DDL�� �����ϴ� ��� (ex> ALTER TABLESPACE)
    // Tablespace���� Table/Index�� Insert/Update/Delete/Select�ϴ� ���
    // => DROPPED, DISCARDED, OFFLINE�̸� �����߻�
    SCT_VAL_DDL_DML   =
        (SCT_VAL_CHECK_DROPPED|SCT_VAL_CHECK_DISCARDED|SCT_VAL_CHECK_OFFLINE),
    // DROP TBS�� ���
    // => DROPPED �̸� �����߻�
    // => Tablespace��  DISCARDED���¿��� ������ �߻���Ű�� �ʴ´�.
    SCT_VAL_DROP_TBS =
        (SCT_VAL_CHECK_DROPPED),

    // ALTER TBS ONLINE/OFFLINE�� ���
    // => DROPPED, DISCARDED�̸� �����߻�
    // => Tablespace�� OFFLINE���¿��� ������ �߻���Ű�� �ʴ´�.
    SCT_VAL_ALTER_TBS_ONOFF =
        (SCT_VAL_CHECK_DROPPED|SCT_VAL_CHECK_DISCARDED)
} sctTBSLockValidOpt;

#endif /* _O_SCT_DEF_H_ */

