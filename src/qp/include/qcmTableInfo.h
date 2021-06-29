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
 * $Id: qcmTableInfo.h 90270 2021-03-21 23:20:18Z bethy $
 **********************************************************************/

#ifndef _O_QCM_TABLE_INFO_H_
#define _O_QCM_TABLE_INFO_H_ 1

#include    <qc.h>

// BUG-16980
// ����� Table Type�� Sequence Type�� �и��Ǿ�� ������
// SYS_TABLES_�� TABLE_TYPE �÷����� ����Ͽ�,
// �ߺ����� ���� �ٸ� ���� �������� �Ѵ�.
enum qcmTableType
{
    QCM_USER_TABLE,        // 'T' (SYS_TABLES_.TABLE_TYPE)
    QCM_META_TABLE,        // 'T'
    QCM_SEQUENCE,          // 'S'
    QCM_VIEW,              // 'V'
    QCM_FIXED_TABLE,
    QCM_DUMP_TABLE,        // PROJ-1618 Online Dump
    QCM_PERFORMANCE_VIEW,
    QCM_QUEUE_TABLE,       // 'Q'
    QCM_QUEUE_SEQUENCE,    // 'W'
    QCM_MVIEW_TABLE,       /* 'M' */
    QCM_MVIEW_VIEW,        /* 'A' */
    QCM_INDEX_TABLE,       /* 'G' BUG-35460 Add TABLE_TYPE G in SYS_TABLES_ */
    QCM_SEQUENCE_TABLE,    /* 'E' PROJ-2365 sequence table */
    QCM_RECYCLEBIN_TABLE,  /* 'R' PROJ-2441 flashback R recyclebin */
    QCM_DICTIONARY_TABLE   // 'D'
};

enum qcmViewStatusType
{
    QCM_VIEW_VALID = 0,
    QCM_VIEW_INVALID = 1
};

/* BUG-36350 Updatable Join DML WITH READ ONLY*/
enum qcmViewReadOnly
{
    QCM_VIEW_READ_ONLY = 0,
    QCM_VIEW_NON_READ_ONLY = 1
};

// PROJ-1502 PARTITIONED DISK TABLE - BEGIN -

enum qcmTablePartitionType
{
    QCM_PARTITIONED_TABLE     = 0,
    QCM_TABLE_PARTITION       = 1,
    QCM_NONE_PARTITIONED_TABLE = 100
};

enum qcmPartitionMethod
{
    QCM_PARTITION_METHOD_RANGE = 0,
    QCM_PARTITION_METHOD_HASH  = 1,
    QCM_PARTITION_METHOD_LIST  = 2,
    QCM_PARTITION_METHOD_RANGE_USING_HASH = 3,
    QCM_PARTITION_METHOD_NONE  = 100
};

enum qcmIndexPartitionType
{
    QCM_LOCAL_PREFIXED_PARTITIONED_INDEX       = 0,
    QCM_LOCAL_NONE_PREFIXED_PARTITIONED_INDEX  = 1,
    QCM_GLOBAL_PREFIXED_PARTITIONED_INDEX      = 2,
    QCM_LOCAL_INDEX_PARTITION                  = 3,
    QCM_GLOBAL_INDEX_PARTITION                 = 4,
    QCM_NONE_PARTITIONED_INDEX                 = 100
};

enum qcmPartitionObjectType
{
    QCM_TABLE_OBJECT_TYPE = 0,
    QCM_INDEX_OBJECT_TYPE = 1
};
// PROJ-1502 PARTITIONED DISK TABLE - END -

// PROJ-1407 Temporary Table
enum qcmTemporaryType
{
    QCM_TEMPORARY_ON_COMMIT_NONE = 0,
    QCM_TEMPORARY_ON_COMMIT_DELETE_ROWS,
    QCM_TEMPORARY_ON_COMMIT_PRESERVE_ROWS
};

// PROJ-1624 Global Non-partitioned Index
enum qcmHiddenType
{
    QCM_HIDDEN_NONE = 0,
    QCM_HIDDEN_INDEX_TABLE
};

/* PROJ-2359 Table/Partition Access Option */
enum qcmAccessOption
{
    QCM_ACCESS_OPTION_NONE = 0,
    QCM_ACCESS_OPTION_READ_ONLY,    /* 'R' */
    QCM_ACCESS_OPTION_READ_WRITE,   /* 'W' */
    QCM_ACCESS_OPTION_READ_APPEND   /* 'A' */
};

/* TASK-7307 DML Data Consistency in Shard */
enum qcmShardFlag
{
    QCM_SHARD_FLAG_TABLE_NONE = 0,
    QCM_SHARD_FLAG_TABLE_META,
    QCM_SHARD_FLAG_TABLE_BACKUP,
    QCM_SHARD_FLAG_TABLE_SPLIT,
    QCM_SHARD_FLAG_TABLE_CLONE,
    QCM_SHARD_FLAG_TABLE_SOLO
};

/* BUG-45646 */
typedef enum
{
    QCM_PV_TYPE_NONE   = 0,
    QCM_PV_TYPE_NORMAL = 1,
    QCM_PV_TYPE_SHARD  = 2
} qcmPVType;

// PROJ-1407 Temporary Table
typedef struct qcmTemporaryInfo
{
    qcmTemporaryType    type;
} qcmTemporaryInfo;

typedef struct qcmTableInfo
{
    // BASIC TABLE information
    UInt                      tableOwnerID;
    SChar                     tableOwnerName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                      tableID;     // This is META ID, not tableOID
    void                    * tableHandle;
    SChar                     name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcmTableType              tableType;
    qcmPVType                 mPVType;

    // BUG-13725
    UInt                      operatableFlag;

    // PROJ-1382, jhseong, global performance View index
    SInt                      viewArrayNo;

    UInt                      replicationCount;
    UInt                      replicationRecoveryCount;
    ULong                     maxrows;
    qcmViewStatusType         status;       // status of VIEW
    SChar                     TBSName[ SMI_MAX_TABLESPACE_NAME_LEN + 1 ];
    scSpaceID                 TBSID;
    smiTableSpaceType         TBSType;
    smiSegAttr                segAttr;
    smiSegStorageAttr         segStoAttr;

    // COLUMN information
    UInt                      columnCount;
    struct qcmColumn        * columns;

    // INDEX information
    UInt                      indexCount;
    struct qcmIndex         * indices;

    // PRIMARY KEY information
    // replication�Ӹ� �ƴ϶� referential constraint�� �����
    // predicate selectivity���� ���ϱ� ���ؼ� optimizer������ �ʿ���.
    struct qcmIndex         * primaryKey; // for replication or optimizer

    // UNIQUE KEY information
    UInt                      uniqueKeyCount;
    struct qcmUnique        * uniqueKeys;

    // FOREIGN KEY information
    UInt                      foreignKeyCount;
    struct qcmForeignKey    * foreignKeys;

    // NOT NULL information
    UInt                      notNullCount;
    struct qcmNotNull       * notNulls;

    /* PROJ-1107 Check Constraint ���� */
    UInt                      checkCount;
    struct qcmCheck         * checks;

    // TIMESTAMP information
    struct qcmTimeStamp     * timestamp;

    // PRIVILEGE
    // number of users who have object privileges
    UInt                      privilegeCount;
    struct qcmPrivilege     * privilegeInfo;

    // PROJ-1359 Trigger Information
    UInt                      triggerCount;
    struct qcmTriggerInfo   * triggerInfo;

    // PROJ-1362 LOB column information
    UInt                      lobColumnCount;

    // PROJ-1502 PARTITIONED DISK TABLE
    UInt                      partitionID;
    qcmTablePartitionType     tablePartitionType;
    qcmPartitionMethod        partitionMethod;
    UInt                      partKeyColCount;
    struct qcmColumn *        partKeyColumns;
    mtcColumn               * partKeyColBasicInfo;
    UInt                    * partKeyColsFlag;
    idBool                    rowMovement;

    // PROJ-1407 Temporary Table
    qcmTemporaryInfo          temporaryInfo;

    // PROJ-1624 Global Non-partitioned Index
    qcmHiddenType             hiddenType;

    /* PROJ-2359 Table/Partition Access Option */
    qcmAccessOption           accessOption;

    /* PROJ-1071 Parallel query */
    UInt                      parallelDegree;

    /* ��Ÿ ���� ����ϴ� ���� */
    smOID                     tableOID;
    UInt                      tableFlag;
    idBool                    isDictionary;
    qcmViewReadOnly           viewReadOnly;
    
    /* TASK-7307 DML Data Consistency in Shard */
    idBool                    mIsUsable;
    UInt                      mShardFlag;

} qcmTableInfo;

/* TASK-7307 DML Data Consistency in Shard */
#define QCM_TABLE_SHARD_FLAG_TABLE_MASK   (0x00000007)
#define QCM_TABLE_SHARD_FLAG_TABLE_NONE   (0x00000000)
#define QCM_TABLE_SHARD_FLAG_TABLE_META   (0x00000001)
#define QCM_TABLE_SHARD_FLAG_TABLE_BACKUP (0x00000002)
#define QCM_TABLE_SHARD_FLAG_TABLE_SPLIT  (0x00000003)
#define QCM_TABLE_SHARD_FLAG_TABLE_CLONE  (0x00000004)
#define QCM_TABLE_SHARD_FLAG_TABLE_SOLO   (0x00000005)

#define QCM_TABLE_IS_SHARD_META( _flag_ )                         \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_META ) ? ID_TRUE : ID_FALSE )

#define QCM_TABLE_IS_SHARD_BACKUP( _flag_ )                       \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_BACKUP ) ? ID_TRUE : ID_FALSE )

#define QCM_TABLE_IS_SHARD_SPLIT( _flag_ )                        \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_SPLIT ) ? ID_TRUE : ID_FALSE )

#define QCM_TABLE_IS_SHARD_CLONE( _flag_ )                        \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_CLONE ) ? ID_TRUE : ID_FALSE )

#define QCM_TABLE_IS_SHARD_SOLO( _flag_ )                         \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_SOLO ) ? ID_TRUE : ID_FALSE )

#define QCM_TABLE_IS_FOR_SHARD( _flag_ )                          \
    ( ( (_flag_ & QCM_TABLE_SHARD_FLAG_TABLE_MASK ) ==            \
        QCM_TABLE_SHARD_FLAG_TABLE_NONE ) ? ID_FALSE : ID_TRUE )

/* qcmColumn.flag�� ������ ����                    */
/* qcply.y���� qcmColumn�� type�� ������             */
# define QCM_COLUMN_TYPE_MASK                    (0x00000007)
# define QCM_COLUMN_TYPE_FIXED                   (0x00000000)
# define QCM_COLUMN_TYPE_VARIABLE                (0x00000001)
# define QCM_COLUMN_TYPE_VARIABLE_LARGE            (0x00000002)
# define QCM_COLUMN_TYPE_DEFAULT                 (0x00000003)

// PROJ-1877
// alter table modify column�� column�� length ��ȯ����
# define QCM_COLUMN_MODIFY_LENGTH_MASK           (0x00000008)
# define QCM_COLUMN_MODIFY_LENGTH_FALSE          (0x00000000)
# define QCM_COLUMN_MODIFY_LENGTH_TRUE           (0x00000008)

// PROJ-1877
// alter table modify column�� column�� (not) null ��������
# define QCM_COLUMN_MODIFY_NULLABLE_MASK         (0x00000030)
# define QCM_COLUMN_MODIFY_NULLABLE_NONE         (0x00000000)
# define QCM_COLUMN_MODIFY_NULLABLE_NULL         (0x00000010)
# define QCM_COLUMN_MODIFY_NULLABLE_NOTNULL      (0x00000020)

// PROJ-1877
// alter table modify column�� column�� default value ��������
# define QCM_COLUMN_MODIFY_DEFAULT_MASK          (0x00000040)
# define QCM_COLUMN_MODIFY_DEFAULT_FALSE         (0x00000000)
# define QCM_COLUMN_MODIFY_DEFAULT_TRUE          (0x00000040)

// PROJ-1877
// alter table modify column�� column�� type ��ȯ���� data loss ���� ����
# define QCM_COLUMN_MODIFY_DATA_LOSS_MASK        (0x00000080)
# define QCM_COLUMN_MODIFY_DATA_LOSS_FALSE       (0x00000000)
# define QCM_COLUMN_MODIFY_DATA_LOSS_TRUE        (0x00000080)

// PROJ-1502 PARTITIONED DISK TABLE
// lob column�� tablespace�� ����ڰ� �����ߴ��� ����
/* qcmColumn.flag�� ������ ����*/
# define QCM_COLUMN_LOB_DEFAULT_TBS_MASK         (0x00000100)
# define QCM_COLUMN_LOB_DEFAULT_TBS_FALSE        (0x00000000)
# define QCM_COLUMN_LOB_DEFAULT_TBS_TRUE         (0x00000100)

// PROJ-1877
// alter table modify column�� column�� column option ��������
# define QCM_COLUMN_MODIFY_DATA_TYPE_MASK        (0x00000200)
# define QCM_COLUMN_MODIFY_DATA_TYPE_FALSE       (0x00000000)
# define QCM_COLUMN_MODIFY_DATA_TYPE_TRUE        (0x00000200)

// PROJ-1877
// alter table modify column�� column�� column option ��������
# define QCM_COLUMN_MODIFY_COLUMN_OPTION_MASK    (0x00000400)
# define QCM_COLUMN_MODIFY_COLUMN_OPTION_FALSE   (0x00000000)
# define QCM_COLUMN_MODIFY_COLUMN_OPTION_TRUE    (0x00000400)

// PROJ-2002 Column Security
// alter table modify column�� encryption ��������
# define QCM_COLUMN_MODIFY_ENCRYPT_COLUMN_MASK   (0x00000800)
# define QCM_COLUMN_MODIFY_ENCRYPT_COLUMN_FALSE  (0x00000000)
# define QCM_COLUMN_MODIFY_ENCRYPT_COLUMN_TRUE   (0x00000800)

// PROJ-2002 Column Security
// alter table modify column�� decryption ��������
# define QCM_COLUMN_MODIFY_DECRYPT_COLUMN_MASK   (0x00001000)
# define QCM_COLUMN_MODIFY_DECRYPT_COLUMN_FALSE  (0x00000000)
# define QCM_COLUMN_MODIFY_DECRYPT_COLUMN_TRUE   (0x00001000)

/* PROJ-1090 Function-based Index
 *  Hidden Column���� ����
 */
#define QCM_COLUMN_HIDDEN_COLUMN_MASK            (0x00002000)
#define QCM_COLUMN_HIDDEN_COLUMN_FALSE           (0x00000000)
#define QCM_COLUMN_HIDDEN_COLUMN_TRUE            (0x00002000)

// PROJ-2415 Grouping Sets Clause
// Grouping Sets Transform�� ���� Target�� �߰��� OrderBy Node�� ���� flag��
// tableInfo->columns�� �����ϱ� ���� flag
#define QCM_COLUMN_GBGS_ORDER_BY_NODE_MASK       (0x00004000)
#define QCM_COLUMN_GBGS_ORDER_BY_NODE_FALSE      (0x00000000)
#define QCM_COLUMN_GBGS_ORDER_BY_NODE_TRUE       (0x00004000)

// PROJ-1877
// alter table modify column�� column�� type ��ȯ����
# define QCM_COLUMN_MODIFY_TYPE_MASK             (0x00008000)
# define QCM_COLUMN_MODIFY_TYPE_FALSE            (0x00000000)
# define QCM_COLUMN_MODIFY_TYPE_TRUE             (0x00008000)

// PROJ-2422 srid
// alter table modify column�� srid ��������
# define QCM_COLUMN_MODIFY_SRID_MASK             (0x00010000)
# define QCM_COLUMN_MODIFY_SRID_FALSE            (0x00000000)
# define QCM_COLUMN_MODIFY_SRID_TRUE             (0x00010000)

/* BUG-13725 */
/* qcmTableInfo.operatableFlag ������ ���� */
/****************** Operatable QP Functions ********************************/
# define QCM_OPERATABLE_QP_REPL_MASK             (0x00000001)
# define QCM_OPERATABLE_QP_REPL_DISABLE          (0x00000000)
# define QCM_OPERATABLE_QP_REPL_ENABLE           (0x00000001)
# define QCM_IS_OPERATABLE_QP_REPL( _flag_ )                    \
    ( ( (_flag_ & QCM_OPERATABLE_QP_REPL_MASK) ==               \
        QCM_OPERATABLE_QP_REPL_ENABLE ) ? ID_TRUE : ID_FALSE )


# define QCM_OPERATABLE_QP_LOCK_TABLE_MASK       (0x00000004)
# define QCM_OPERATABLE_QP_LOCK_TABLE_DISABLE    (0x00000000)
# define QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE     (0x00000004)
# define QCM_IS_OPERATABLE_QP_LOCK_TABLE( _flag_ )                      \
    ( ( (_flag_ & QCM_OPERATABLE_QP_LOCK_TABLE_MASK) ==                 \
        QCM_OPERATABLE_QP_LOCK_TABLE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_CREATE_TABLE_MASK     (0x00000008)
# define QCM_OPERATABLE_QP_CREATE_TABLE_DISABLE  (0x00000000)
# define QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE   (0x00000008)
# define QCM_IS_OPERATABLE_QP_CREATE_TABLE( _flag_ )                    \
    ( ( (_flag_ & QCM_OPERATABLE_QP_CREATE_TABLE_MASK) ==               \
        QCM_OPERATABLE_QP_CREATE_TABLE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_DROP_TABLE_MASK       (0x00000010)
# define QCM_OPERATABLE_QP_DROP_TABLE_DISABLE    (0x00000000)
# define QCM_OPERATABLE_QP_DROP_TABLE_ENABLE     (0x00000010)
# define QCM_IS_OPERATABLE_QP_DROP_TABLE( _flag_ )                      \
    ( ( (_flag_ & QCM_OPERATABLE_QP_DROP_TABLE_MASK) ==                 \
        QCM_OPERATABLE_QP_DROP_TABLE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_TRUNC_TABLE_MASK      (0x00000020)
# define QCM_OPERATABLE_QP_TRUNC_TABLE_DISABLE   (0x00000000)
# define QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE    (0x00000020)
# define QCM_IS_OPERATABLE_QP_TRUNC_TABLE( _flag_ )                     \
    ( ( (_flag_ & QCM_OPERATABLE_QP_TRUNC_TABLE_MASK) ==                \
        QCM_OPERATABLE_QP_TRUNC_TABLE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_ALTER_TABLE_MASK      (0x00000040)
# define QCM_OPERATABLE_QP_ALTER_TABLE_DISABLE   (0x00000000)
# define QCM_OPERATABLE_QP_ALTER_TABLE_ENABLE    (0x00000040)
# define QCM_IS_OPERATABLE_QP_ALTER_TABLE( _flag_ )                     \
    ( ( (_flag_ & QCM_OPERATABLE_QP_ALTER_TABLE_MASK) ==                \
        QCM_OPERATABLE_QP_ALTER_TABLE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_CREATE_VIEW_MASK      (0x00000080)
# define QCM_OPERATABLE_QP_CREATE_VIEW_DISABLE   (0x00000000)
# define QCM_OPERATABLE_QP_CREATE_VIEW_ENABLE    (0x00000080)
# define QCM_IS_OPERATABLE_QP_CREATE_VIEW( _flag_ )                     \
    ( ( (_flag_ & QCM_OPERATABLE_QP_CREATE_VIEW_MASK) ==                \
        QCM_OPERATABLE_QP_CREATE_VIEW_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_ALTER_VIEW_MASK       (0x00000100)
# define QCM_OPERATABLE_QP_ALTER_VIEW_DISABLE    (0x00000000)
# define QCM_OPERATABLE_QP_ALTER_VIEW_ENABLE     (0x00000100)
# define QCM_IS_OPERATABLE_QP_ALTER_VIEW( _flag_ )                      \
    ( ( (_flag_ & QCM_OPERATABLE_QP_ALTER_VIEW_MASK) ==                 \
        QCM_OPERATABLE_QP_ALTER_VIEW_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_DROP_VIEW_MASK        (0x00000200)
# define QCM_OPERATABLE_QP_DROP_VIEW_DISABLE     (0x00000000)
# define QCM_OPERATABLE_QP_DROP_VIEW_ENABLE      (0x00000200)
# define QCM_IS_OPERATABLE_QP_DROP_VIEW( _flag_ )                       \
    ( ( (_flag_ & QCM_OPERATABLE_QP_DROP_VIEW_MASK) ==                  \
        QCM_OPERATABLE_QP_DROP_VIEW_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_INSERT_MASK           (0x00000400)
# define QCM_OPERATABLE_QP_INSERT_DISABLE        (0x00000000)
# define QCM_OPERATABLE_QP_INSERT_ENABLE         (0x00000400)
# define QCM_IS_OPERATABLE_QP_INSERT( _flag_ )                          \
    ( ( (_flag_ & QCM_OPERATABLE_QP_INSERT_MASK) ==                     \
        QCM_OPERATABLE_QP_INSERT_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_UPDATE_MASK           (0x00000800)
# define QCM_OPERATABLE_QP_UPDATE_DISABLE        (0x00000000)
# define QCM_OPERATABLE_QP_UPDATE_ENABLE         (0x00000800)
# define QCM_IS_OPERATABLE_QP_UPDATE( _flag_ )                          \
    ( ( (_flag_ & QCM_OPERATABLE_QP_UPDATE_MASK) ==                     \
        QCM_OPERATABLE_QP_UPDATE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_DELETE_MASK           (0x00001000)
# define QCM_OPERATABLE_QP_DELETE_DISABLE        (0x00000000)
# define QCM_OPERATABLE_QP_DELETE_ENABLE         (0x00001000)
# define QCM_IS_OPERATABLE_QP_DELETE( _flag_ )                          \
    ( ( (_flag_ & QCM_OPERATABLE_QP_DELETE_MASK) ==                     \
        QCM_OPERATABLE_QP_DELETE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_MOVE_MASK             (0x00002000)
# define QCM_OPERATABLE_QP_MOVE_DISABLE          (0x00000000)
# define QCM_OPERATABLE_QP_MOVE_ENABLE           (0x00002000)
# define QCM_IS_OPERATABLE_QP_MOVE( _flag_ )                    \
    ( ( (_flag_ & QCM_OPERATABLE_QP_MOVE_MASK) ==               \
        QCM_OPERATABLE_QP_MOVE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_SELECT_MASK           (0x00004000)
# define QCM_OPERATABLE_QP_SELECT_DISABLE        (0x00000000)
# define QCM_OPERATABLE_QP_SELECT_ENABLE         (0x00004000)
# define QCM_IS_OPERATABLE_QP_SELECT( _flag_ )                          \
    ( ( (_flag_ & QCM_OPERATABLE_QP_SELECT_MASK) ==                     \
        QCM_OPERATABLE_QP_SELECT_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_COMMENT_MASK          (0x00008000)
# define QCM_OPERATABLE_QP_COMMENT_DISABLE       (0x00000000)
# define QCM_OPERATABLE_QP_COMMENT_ENABLE        (0x00008000)
# define QCM_IS_OPERATABLE_QP_COMMENT( _flag_ )                          \
    ( ( (_flag_ & QCM_OPERATABLE_QP_COMMENT_MASK) ==                     \
        QCM_OPERATABLE_QP_COMMENT_ENABLE ) ? ID_TRUE : ID_FALSE )

/* PROJ-2441 flashback */
# define QCM_OPERATABLE_QP_PURGE_MASK            (0x00010000)
# define QCM_OPERATABLE_QP_PURGE_DISABLE         (0x00000000)
# define QCM_OPERATABLE_QP_PURGE_ENABLE          (0x00010000)
# define QCM_IS_OPERATABLE_QP_PURGE( _flag_ )                           \
( ( (_flag_ & QCM_OPERATABLE_QP_PURGE_MASK) ==                          \
QCM_OPERATABLE_QP_PURGE_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_FLASHBACK_MASK        (0x00020000)
# define QCM_OPERATABLE_QP_FLASHBACK_DISABLE     (0x00000000)
# define QCM_OPERATABLE_QP_FLASHBACK_ENABLE      (0x00020000)
# define QCM_IS_OPERATABLE_QP_FLASHBACK( _flag_ )                       \
( ( (_flag_ & QCM_OPERATABLE_QP_FLASHBACK_MASK) ==                      \
QCM_OPERATABLE_QP_FLASHBACK_ENABLE ) ? ID_TRUE : ID_FALSE )

# define QCM_OPERATABLE_QP_FT_TRANS_MASK          (0x00040000)
# define QCM_OPERATABLE_QP_FT_TRANS_DISABLE       (0x00000000)
# define QCM_OPERATABLE_QP_FT_TRNAS_ENABLE        (0x00040000)
# define QCM_IS_OPERATABLE_QP_FT_TRANS( _flag_ )                       \
( ( (_flag_ & QCM_OPERATABLE_QP_FT_TRANS_MASK ) ==                      \
QCM_OPERATABLE_QP_FT_TRNAS_ENABLE ) ? ID_TRUE : ID_FALSE )

/****************** Operatable QP Functions ********************************/


typedef struct qcmColumn
{
    mtcColumn         * basicInfo;            // id. type
    UInt                flag;
    UInt                inRowLength;          // PROJ-1557
    SChar               name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition      userNamePos;          // PROJ-1075
    qcNamePosition      tableNamePos;
    qcNamePosition      namePos;
    // for making qdTableParseTree->columns in parser
    struct qtcNode    * defaultValue;
    qcmColumn         * next;
    UChar             * defaultValueStr;
    // string representation of default expression (need parse).

    // PROJ-1488 Altibase Spatio-Temporal DBMS
    // stmColumnInfo�� Casting �ؼ� ����ؾ���.
    void              * stColumn;

    // PROJ-1579 NCHAR
    struct qcNamePosList * ncharLiteralPos;

    // PROJ-2002 Column Security
    struct qdExtColumnAttr * mExtColumnAttr;
} qcmColumn;

#define QCM_COLUMN_INIT(_col_)                                  \
{                                                               \
    _col_->basicInfo = NULL;                                    \
    _col_->flag = QCM_COLUMN_TYPE_DEFAULT;                      \
    _col_->inRowLength = 0;                                     \
    idlOS::memset( _col_->name, 0x00, QC_MAX_OBJECT_NAME_LEN + 1 );    \
    SET_EMPTY_POSITION( _col_->userNamePos );                   \
    SET_EMPTY_POSITION( _col_->tableNamePos );                  \
    SET_EMPTY_POSITION( _col_->namePos );                       \
    _col_->defaultValue = NULL;                                 \
    _col_->next = NULL;                                         \
    _col_->defaultValueStr = NULL;                              \
    _col_->stColumn = NULL;                                     \
    _col_->ncharLiteralPos = NULL;                              \
    _col_->mExtColumnAttr = NULL;                               \
}

// PROJ-2264 Dictionary table
typedef struct qcmCompressionColumn
{
    SChar                     name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcNamePosition            namePos;
    ULong                     maxrows;
    UInt                      dictionaryTableID;
    qcmCompressionColumn    * next;
} qcmCompressionColumn;

// PROJ-2264 Dictionary table
#define QCM_COMPRESSION_COLUMN_INIT(_col_)                                  \
{                                                                \
    idlOS::memset( (_col_)->name, 0x00, QC_MAX_OBJECT_NAME_LEN + 1 ); \
    SET_EMPTY_POSITION( (_col_)->namePos );                      \
    (_col_)->maxrows = 0;                                        \
    (_col_)->next = NULL;                                        \
}

typedef struct qcmPrivilege
{
    UInt        userID;             // who has object privileges
    UInt        privilegeBitMap;
} qcmPrivilege;

//-----------------------------------------------------------------
// [qcmIndex]
//
// Index�� ���� Meta Cache ����
// A4������ key column�� ���� ������ mtcColumn���·�
// �����ϰ� �ִ�.
// �� ��, mtcColumn ������ setting�� ������ ����
// ���׿� ���Ͽ� �����Ͽ��� �Ѵ�.
//     - mtcColumn.column.value = NULL;
//        : SM���� Variable Column�� ���� ���� �� ������ ���縦 ���� �ʱ� ����
//     - mtcColumn.module
//        : �ش� module�� ã�� ��Ȯ�� �����Ͽ��� �Ѵ�.
//----------------------------------------------------------------

typedef struct qcmIndex
{
    UInt                  userID;
    SChar                 userName[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                  indexId;
    UInt                  indexTypeId;
    SChar                 name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                  keyColCount;
    mtcColumn           * keyColumns;    // Index�������� Column ����
    UInt                * keyColsFlag;   // ASC, DESC
    idBool                isUnique;      // ID_TRUE : unique, ID_FALSE : non-unique
    idBool                isLocalUnique; // PROJ-1502 ID_TRUE : unique, ID_FALSE : non-unique
    idBool                isRange;       // ID_TRUE : range, ID_FALSE : exact-match
    void                * indexHandle;
    scSpaceID             TBSID;
    idBool                isOnlineTBS;

    // PROJ-1502 partitioned index
    qcmIndexPartitionType indexPartitionType;
    UInt                  indexPartitionID;
    
    // PROJ-1624 non-partitioned index
    // partitioned table�� non-partitioned index�� global index�̴�.
    // ������ index table�� �̿��� index�� �̿��Ѵ�.
    UInt                  indexTableID;

} qcmIndex;

typedef struct qcmForeignKey
{
    SChar                  name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                   constraintID;
    UInt                   constraintColumnCount;
    UInt                   referencingColumn[QC_MAX_KEY_COLUMN_COUNT];
    UInt                   referenceRule;
    UInt                   referencedTableID;
    UInt                   referencedIndexID;
    idBool                 validated;      // PROJ-1874 FK Novalidate
} qcmForeignKey;

typedef struct qcmUnique
{
    SChar                       name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                        constraintID;
    UInt                        constraintColumn[QC_MAX_KEY_COLUMN_COUNT];
    UInt                        constraintType; /*unique, local unique or primary key */
    qcmIndex                  * constraintIndex;
    UInt                        constraintColumnCount;
} qcmUnique;

typedef struct qcmNotNull
{
    SChar                       name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                        constraintID;
    UInt                        constraintColumn[QC_MAX_KEY_COLUMN_COUNT];
    UInt                        constraintType; /*NOT NULL */
    UInt                        constraintColumnCount;
} qcmNotNull;

/* PROJ-1107 Check Constraint ���� */
typedef struct qcmCheck
{
    SChar                       name[QC_MAX_OBJECT_NAME_LEN + 1];
    UInt                        constraintID;
    UInt                        constraintColumn[QC_MAX_KEY_COLUMN_COUNT];
    SChar                     * checkCondition;
    UInt                        constraintColumnCount;
} qcmCheck;

typedef struct qcmTimeStamp
{
    SChar                       name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    UInt                        constraintID;
    UInt                        constraintColumn[QC_MAX_KEY_COLUMN_COUNT];
    UInt                        constraintColumnCount; /* only one column */
} qcmTimeStamp;

typedef struct qcmParentInfo
{
    qcmForeignKey            * foreignKey;
    qcmIndex                 * parentIndex;
    struct qmsIndexTableRef  * parentIndexTable;       // PROJ-1624
    qcmIndex                 * parentIndexTableIndex;  // PROJ-1624
    struct qmsTableRef       * parentTableRef; // PROJ-1502
    struct qcmParentInfo     * next;
} qcmParentInfo;

//----------------------------------------
// BUG-28049
//
// validation �������� �����Ǵ� ������ foreign key ����Ʈ��
// ���� ���¿� ���� ���� ����� �����ϰ� ���װ� �־����ϴ�.
// �������� ������ �����Ǿ��� ������ foreign key ����Ʈ��
// �ϳ��� linked list�� ����ϴ� �������� ������ �����մϴ�.
//
// qcmForeignKeyNode�� qcmChildInfo�� qcmRefChildInfo���� ����
// 
//----------------------------------------

/*
typedef struct qcmForeignKeyNode
{
    qcmForeignKey                * foreignKey;
    void                         * childTableRef; // PROJ-1502
    idBool                         isSelfRef;     // BUG-23370
    struct qcmChildInfo          * child;         // PROJ-1509
    struct qcmForeignKeyNode     * next;
} qcmForeignKeyNode;

typedef struct qcmChildInfo
{
    UInt                parentTableID; // BUG-23370
    qcmIndex          * parentIndex;
    qcmForeignKeyNode * foreignKeys;
    qcmChildInfo      * next;
} qcmChildInfo;
*/

// BUG-35379
// update type
typedef enum qcmRefChildUpdateType
{
    QCM_CHILD_UPDATE_NORMAL = 0,        // �Ϲ� ���̺��� update
    QCM_CHILD_UPDATE_ROWMOVEMENT,       // row movement�� �߻��ϸ� delete-insert�� ó���ϴ� update
    QCM_CHILD_UPDATE_CHECK_ROWMOVEMENT, // row movement�� �߻��ϸ� �������� update
    QCM_CHILD_UPDATE_NO_ROWMOVEMENT     // row movement�� �Ͼ�� �ʴ� update
} qcmRefChildUpdateType;

typedef struct qcmRefChildInfo
{
    idBool                    isCycle;       // BUG-23370
    idBool                    isSelfRef;     // PROJ-2212
    UInt                      parentTableID; // BUG-23370
    qcmIndex                * parentIndex;
    qcmForeignKey           * foreignKey;
    UInt                      referenceRule; // foreignKey->referenceRule�� lock���� �����Ѵ�.
    struct qmsTableRef      * childTableRef; // PROJ-1502

    /* PROJ-1107 Check Constraint ���� */
    struct qdConstraintSpec * childCheckConstrList;

    /* PROJ-1090 Function-based Index */
    struct qmsTableRef      * defaultTableRef;
    qcmColumn               * defaultExprColumns;
    qcmColumn               * defaultExprBaseColumns;

    // on delete set null
    UInt                      updateColCount;
    UInt                    * updateColumnID;
    qcmColumn               * updateColumn;
    smiColumnList           * updateColumnList;
    qcmRefChildUpdateType     updateRowMovement;
    
    struct qcmRefChildInfo  * next;
} qcmRefChildInfo;

//=========================================================
// [PROJ-1359] Trigger�� ���� Meta Cache ����
//=========================================================

//---------------------------------------------------------
// Action Body�� ��ȿ�� ���ο� ���� ����
// �����ؾ� �ϴ� ������ ���� Meta Table�κ��� ��� �����ϸ�,
//     : SYS_TRIGGERS_
//     : SYS_TRIGGER_STRINGS_
//     : SYS_TRIGGER_UPDATE_COLUMNS_
//     : SYS_TRIGGER_DML_TABLES_
// ��� ���� ������ �Ǵ��� Meta Cache�κ��� �����ϸ�,
// When Condition�� Action Body�� ���ุ ���ü� ��� ����
// ����ȴ�.  ��, Cycle Detection ���� �۾� ���� Trigger
// ��ü�� �������� �ʰ� Meta Cache���������� ����ȴ�.
//---------------------------------------------------------

// ALTER TRIGGER ������ ���Ͽ� ENABLE/DISABLE�� �����ȴ�.
typedef enum {
    QCM_TRIGGER_DISABLE = 0,
    QCM_TRIGGER_ENABLE
} qcmTriggerAble;

// [Trigger ���� ����]
/* AFTER, BEFORE, INSTEAD OF */
typedef enum {
    QCM_TRIGGER_NONE = 0,
    QCM_TRIGGER_BEFORE,
    QCM_TRIGGER_AFTER,
    QCM_TRIGGER_INSTEAD_OF
} qcmTriggerEventTime;

// [Trigger Event�� ����]
// INSERT, DELETE, UPDATE Event�� �����ϸ�,
// UPDATE EVENT�� ��� COLUMN ������ ������ �� �ִ�.

#define    QCM_TRIGGER_EVENT_NONE     (0x00000000)
#define    QCM_TRIGGER_EVENT_INSERT   (0x00000001)
#define    QCM_TRIGGER_EVENT_DELETE   (0x00000002)
#define    QCM_TRIGGER_EVENT_UPDATE   (0x00000004)


// [REFERENCING �� ����]
typedef enum {
    QCM_TRIGGER_REF_NONE = 0,
    QCM_TRIGGER_REF_OLD_ROW,
    QCM_TRIGGER_REF_NEW_ROW,
    QCM_TRIGGER_REF_OLD_TABLE,
    QCM_TRIGGER_REF_NEW_TABLE
} qcmTriggerRefType;

// [Trigger Action Granularity]
// EACH_ROW, EACH_STATEMENT �� ������ �����Ѵ�.
typedef enum {
    QCM_TRIGGER_ACTION_NONE = 0,
    QCM_TRIGGER_ACTION_EACH_ROW,
    QCM_TRIGGER_ACTION_EACH_STMT
} qcmTriggerGranularity;

typedef struct qcmTriggerInfo
{
    //----------------------------------------
    // Trigger ����
    //----------------------------------------

    // SYS_TRIGGERS_ �κ��� ȹ��
    UInt            userID;                       // User ID
    SChar           userName[ QC_MAX_OBJECT_NAME_LEN + 1 ];    // User Name
    smOID           triggerOID;                   // Trigger OID
    SChar           triggerName[ QC_MAX_OBJECT_NAME_LEN + 1 ]; // Trigger Name
    UInt            tableID;                      // Subject Table ID

    //----------------------------------------
    // Trigger Event ����
    //----------------------------------------

    qcmTriggerAble              enable;     // DISABLE/ENABLE
    qcmTriggerEventTime         eventTime;  // BEFORE/AFTER
    UInt                        eventType;  // INSERT/DELETE/UPDATE

    SInt                        uptCount;   // Update Column Count on UPDATE EVENT

    //----------------------------------------
    // Action ����
    //----------------------------------------

    qcmTriggerGranularity  granularity; // ROW/STATEMENT
    SInt                   refRowCnt;   // REFERENCING ROW �� ����

    //----------------------------------------
    // Trigger ��ü ����
    // Latch�� �̿��Ͽ� ���ü� ��� �Ѵ�.
    //----------------------------------------

    void          * triggerHandle; // Trigger�� ��ü ������ ������ Handle

} qcmTriggerInfo;

//=========================================================
// [BUG-16980] Sequence�� ���� Meta ���� (Meta Cache�� �ƴ�)
//=========================================================

typedef struct qcmSequenceInfo
{
    UInt                      sequenceOwnerID;
    UInt                      sequenceID;     // This is META ID, not tableOID
    void                    * sequenceHandle;
    smOID                     sequenceOID;
    SChar                     name[ QC_MAX_OBJECT_NAME_LEN + 1 ];
    qcmTableType              sequenceType;   // sequence or queue_sequence

} qcmSequenceInfo;

#endif /* _O_QCM_TABLE_INFO_H_ */
 
