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
 * $Id: sdiTypes.h 91046 2021-06-23 06:06:13Z jake.jang $
 *
 * Description : SD �� Ÿ ��⿡���� ����ϴ� �ڷᱸ�� ����
 *
 **********************************************************************/

#ifndef _O_SDI_TYPES_H_
#define _O_SDI_TYPES_H_ 1

#include <idTypes.h>

#define SDI_NULL_SMN                           ID_ULONG(0)
#define SDI_HASH_MAX_VALUE                     (1000)
#define SDI_HASH_MAX_VALUE_FOR_TEST            (100)

// range, list sharding�� varchar shard key column�� max precision
#define SDI_RANGE_VARCHAR_MAX_PRECISION        (100)
#define SDI_RANGE_VARCHAR_MAX_PRECISION_STR    "100"
#define SDI_RANGE_VARCHAR_MAX_SIZE             (MTD_CHAR_TYPE_STRUCT_SIZE(SDI_RANGE_VARCHAR_MAX_PRECISION))

#define SDI_NODE_MAX_COUNT                     (128)
#define SDI_RANGE_MAX_COUNT                    (1000)
#define SDI_VALUE_MAX_COUNT                    (1000)

#define SDI_SERVER_IP_SIZE                     (64) /* IDL_IP_ADDR_MAX_LEN : 64*/
#define SDI_SERVER_IP_SIZE_STR                 "64"

#define SDI_REPLICATION_NAME_MAX_SIZE          QCI_MAX_NAME_LEN
#define SDI_REPLICATION_NAME_MAX_SIZE_STR      QCI_MAX_NAME_LEN_STR
#define REPL_NAME_PREFIX                       "REPL_SET"
#define REPL_NAME_POSTFIX                      "BACKUP"

#define SDI_NODE_NAME_MAX_SIZE                 (40)
#define SDI_NODE_NAME_MAX_SIZE_STR             "40"
#define SDI_SHARDED_DB_NAME_MAX_SIZE           (40)
#define SDI_SHARDED_DB_NAME_MAX_SIZE_STR       "40"
#define SDI_REPLICATION_MODE_MAX_SIZE          (10)
#define SDI_REPLICATION_MODE_MAX_SIZE_STR      "10"
#define SDI_CHECK_NODE_NAME_MAX_SIZE           (10)

#define SDI_BACKUP_TABLE_PREFIX                "_BAK_"

/* Shard Replication Mode */
#define SDI_REP_MODE_NULL_STR                  "NULL"
#define SDI_REP_MODE_NULL_CODE                 (0)
#define SDI_REP_MODE_CONSISTENT_STR            "CONSISTENT"
#define SDI_REP_MODE_CONSISTENT_CODE           (12)
#define SDI_REP_MODE_NETWAIT_STR               "NETWAIT"
#define SDI_REP_MODE_NETWAIT_CODE              (11)
#define SDI_REP_MODE_NOWAIT_STR                "NOWAIT"
#define SDI_REP_MODE_NOWAIT_CODE               (10)

/* Shard replica count, k-safety max */
#define SDI_KSAFETY_MAX                        (2)
#define SDI_KSAFETY_STR_MAX_SIZE               (1)
#define SDI_PARALLEL_COUNT_STR_MAX_SIZE        (9)

/* Full address: NODE1:xxx.xxx.xxx.xxx:nnnn */
#define SDI_PORT_NUM_BUFFER_SIZE               (10)
#define SDI_FULL_SERVER_ADDRESS_SIZE           ( SDI_NODE_NAME_MAX_SIZE   \
                                               + SDI_SERVER_IP_SIZE       \
                                               + SDI_PORT_NUM_BUFFER_SIZE )

/* PROJ-2661 */
#define SDI_XA_RECOVER_RMID                    (0)
#define SDI_XA_RECOVER_COUNT                   (5)

/*               01234567890123456789
 * Max ShardPIN: 255-65535-4294967295
 */
#define SDI_MAX_SHARD_PIN_STR_LEN              (20) /* Without Null terminated */
#define SDI_SHARD_PIN_FORMAT_STR   "%"ID_UINT32_FMT"-%"ID_UINT32_FMT"-%"ID_UINT32_FMT
#define SDI_SHARD_PIN_FORMAT_ARG( __SHARD_PIN ) \
    ( __SHARD_PIN & ( (sdiShardPin)0xff       << SDI_OFFSET_VERSION      ) ) >> SDI_OFFSET_VERSION, \
    ( __SHARD_PIN & ( (sdiShardPin)0xffff     << SDI_OFFSET_SHARD_NODE_ID ) ) >> SDI_OFFSET_SHARD_NODE_ID, \
    ( __SHARD_PIN & ( (sdiShardPin)0xffffffff << SDI_OFFSET_SEQEUNCE     ) ) >> SDI_OFFSET_SEQEUNCE

#define SDI_ODBC_CONN_ATTR_RETRY_COUNT_DEFAULT   (IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_COUNT_DEFAULT)
#define SDI_ODBC_CONN_ATTR_RETRY_DELAY_DEFAULT   (IDP_SHARD_INTERNAL_CONN_ATTR_RETRY_DELAY_DEFAULT)
#define SDI_ODBC_CONN_ATTR_CONN_TIMEOUT_DEFAULT  (IDP_SHARD_INTERNAL_CONN_ATTR_CONNECTION_TIMEOUT_DEFAULT)
#define SDI_ODBC_CONN_ATTR_LOGIN_TIMEOUT_DEFAULT (IDP_SHARD_INTERNAL_CONN_ATTR_LOGIN_TIMEOUT_DEFAULT)

/* TASK-7219 Non-shard DML
 *     Server-side shard execution sequence
 *     2000000000 ~ ( 4000000000 - 1 )
 *
 *     Client-side shardexecution sequence
 *     0          ~ ( 2000000000 - 1 )
 */
#define SDI_STMT_EXEC_SEQ_INIT (2000000000)
#define SDI_STMT_EXEC_SEQ_MAX  (4000000000)

/* TASK-7219 Shard Transformer Refactoring */
typedef enum
{
    /*
     * PROJ-2646 shard analyzer enhancement
     * boolean array�� shard query false�� ������ üũ�Ѵ�.
     *
     * BUG-45718
     * Reason�� ������ �� ��
     * Enum�� ������ ��������
     * setNonShardQueryReasonErr()���� error message������ �켱������ ����.
     * SDI_NON_SHARD_QUERY_REASONS �� ������ �����ؾ� �Ѵ�.
     */

    // �����Ǻ� non-shard reason
    SDI_SHARD_KEYWORD_EXISTS                   =  0,
    SDI_NON_SHARD_OBJECT_EXISTS                =  1, // SHARD META�� ��ϵ��� ���� TABLE�� ��� ��
    SDI_MULTI_SHARD_INFO_EXISTS                =  2, // �л� ���ǰ� �ٸ� SHARD TABLE���� ��� ��
    SDI_HIERARCHY_EXISTS                       =  3, // CONNECT BY�� ��� ��
    SDI_LATERAL_VIEW_EXISTS                    =  4,
    SDI_RECURSIVE_VIEW_EXISTS                  =  5,
    SDI_PIVOT_EXISTS                           =  6,
    SDI_CNF_NORMALIZATION_FAIL                 =  7,

    // ���Ǻ� non-shard reason
    SDI_NODE_TO_NODE_JOIN_EXISTS               =  8, // ��� �� JOIN�� �ʿ���
    SDI_NODE_TO_NODE_DISTINCT_EXISTS           =  9, // ��� �� ������ �ʿ��� DISTINCT�� ��� ��
    SDI_NODE_TO_NODE_GROUP_AGGREGATION_EXISTS  = 10, // ��� �� ������ �ʿ��� GROUP BY�� ��� ��
    SDI_NODE_TO_NODE_NESTED_AGGREGATION_EXISTS = 11, // ��� �� ������ �ʿ��� GROUP BY�� ��� ��
    SDI_NODE_TO_NODE_GROUP_BY_EXTENSION_EXISTS = 12, // Group by extension(ROLLUP,CUBE,GROUPING SETS)�� �����Ѵ�.
    SDI_NODE_TO_NODE_OUTER_JOIN_EXISTS         = 13, // CLONE table�� left-side����, HASH,RANGE,LIST table�� right-side�� ���� outer join�� �����Ѵ�.
    SDI_NODE_TO_NODE_SEMI_ANTI_JOIN_EXISTS     = 14, // HASH,RANGE,LIST table�� inner(subquery table)�� ���� semi/anti-join�� �����Ѵ�.
    SDI_NODE_TO_NODE_WINDOW_FUNCTION_EXISTS    = 15, // BUG-47642 ��� �� ������ �ʿ��� window function �� �����Ѵ�.
    SDI_NODE_TO_NODE_SET_OP_EXISTS             = 16, // ��� �� ������ �ʿ��� SET operator�� ��� ��
    SDI_NODE_TO_NODE_ORDER_BY_EXISTS           = 17, // ��� �� ������ �ʿ��� ORDER BY�� ��� ��
    SDI_NODE_TO_NODE_LIMIT_EXISTS              = 18, // ��� �� ������ �ʿ��� LIMIT�� ��� ��
    SDI_NODE_TO_NODE_LOOP_EXISTS               = 19, // ��� �� ������ �ʿ��� LOOP�� ��� ��
    SDI_UNSPECIFIED_SHARD_KEY_VALUE            = 20,
    SDI_NON_SHARD_SUBQUERY_EXISTS              = 21, // SHARD SUBQUERY�� ��� ��
    SDI_NON_SHARD_GROUPING_SET_EXISTS          = 22, // Grouping Set ���  ��
    SDI_NON_SHARD_PSM_BIND_EXISTS              = 23, // PSM ������ ���  ��
    SDI_NON_SHARD_PSM_LOB_EXISTS               = 24, // PSM LOB ������ ���  ��
    SDI_UNKNOWN_REASON                         = 25,
    SDI_NON_SHARD_QUERY_REASON_MAX             = 26
} sdiNonShardQueryReason;

#define SDI_NON_SHARD_QUERY_REASONS            \
    (SChar*)"Shard keyword exists.",\
    (SChar*)"Non-shard object exists.",\
    (SChar*)"The shard distribution information of the objects is different.",\
    (SChar*)"CONNECT BY clause exists.",\
    (SChar*)"LATERAL view exists.",\
    (SChar*)"RECURSIVE WITH clause exists.",\
    (SChar*)"PIVOT or UNPIVOT clause exists.",\
    (SChar*)"CNF normalization was failed.",\
    (SChar*)"Node to node join operation exists.",\
    (SChar*)"Node to node distinction exists.",\
    (SChar*)"Node to node aggregate operation exists.",\
    (SChar*)"Node to node nested aggregate operation exists.",\
    (SChar*)"Node to node grouping extension exists.",\
    (SChar*)"Node to node outer join operation exists.",\
    (SChar*)"Node to node semi join or anti join operation exists.",\
    (SChar*)"Node to node windowing operation exists.", \
    (SChar*)"Node to node set operation exists.",\
    (SChar*)"Node to node sorting operation exists.",\
    (SChar*)"Node to node limit operation exists.",\
    (SChar*)"Node to node loop operation exists.",\
    (SChar*)"Unspecified shard key value",\
    (SChar*)"Non-shard subquery exists.",\
    (SChar*)"Non-shard grouping set exists.",\
    (SChar*)"Non-shard psm bind exists.",\
    (SChar*)"Non-shard psm lob bind exists.",\
    (SChar*)"Unsupported statement type",\
    (SChar*)"Unexpected reason : reason max"\
// ���� : ������ reason ���� ',' �� ������ ��!

#define SDI_INIT_NON_SHARD_QUERY_REASON(_dst_)                     \
{                                                                  \
    _dst_[SDI_SHARD_KEYWORD_EXISTS]                   = ID_FALSE;  \
    _dst_[SDI_NON_SHARD_OBJECT_EXISTS]                = ID_FALSE;  \
    _dst_[SDI_MULTI_SHARD_INFO_EXISTS]                = ID_FALSE;  \
    _dst_[SDI_HIERARCHY_EXISTS]                       = ID_FALSE;  \
    _dst_[SDI_LATERAL_VIEW_EXISTS]                    = ID_FALSE;  \
    _dst_[SDI_RECURSIVE_VIEW_EXISTS]                  = ID_FALSE;  \
    _dst_[SDI_PIVOT_EXISTS]                           = ID_FALSE;  \
    _dst_[SDI_CNF_NORMALIZATION_FAIL]                 = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_JOIN_EXISTS]               = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_DISTINCT_EXISTS]           = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_GROUP_AGGREGATION_EXISTS]  = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_NESTED_AGGREGATION_EXISTS] = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_GROUP_BY_EXTENSION_EXISTS] = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_OUTER_JOIN_EXISTS]         = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_SEMI_ANTI_JOIN_EXISTS]     = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_WINDOW_FUNCTION_EXISTS]    = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_SET_OP_EXISTS]             = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_ORDER_BY_EXISTS]           = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_LIMIT_EXISTS]              = ID_FALSE;  \
    _dst_[SDI_NODE_TO_NODE_LOOP_EXISTS]               = ID_FALSE;  \
    _dst_[SDI_UNSPECIFIED_SHARD_KEY_VALUE]            = ID_FALSE;  \
    _dst_[SDI_NON_SHARD_SUBQUERY_EXISTS]              = ID_FALSE;  \
    _dst_[SDI_NON_SHARD_GROUPING_SET_EXISTS]          = ID_FALSE;  \
    _dst_[SDI_NON_SHARD_PSM_BIND_EXISTS]              = ID_FALSE;  \
    _dst_[SDI_NON_SHARD_PSM_LOB_EXISTS]               = ID_FALSE;  \
    _dst_[SDI_UNKNOWN_REASON]                         = ID_FALSE;  \
}

typedef enum
{
    SDI_SHARD_CLIENT_FALSE = 0,
    SDI_SHARD_CLIENT_TRUE  = 1,
} sdiShardClient;

typedef enum
{
    SDI_SESSION_TYPE_USER  = 0,
    SDI_SESSION_TYPE_COORD = 1,
    SDI_SESSION_TYPE_LIB   = 2,
} sdiSessionType;

typedef enum
{
    SDI_QUERY_TYPE_NONE     = 0,
    SDI_QUERY_TYPE_SHARD    = 1,
    SDI_QUERY_TYPE_NONSHARD = 2,
    SDI_QUERY_TYPE_MAX      = 3 
} sdiQueryType;

typedef enum
{
    SDI_SPLIT_NONE  = 0,
    SDI_SPLIT_HASH  = 1,
    SDI_SPLIT_RANGE = 2,
    SDI_SPLIT_LIST  = 3,
    SDI_SPLIT_CLONE = 4,
    SDI_SPLIT_SOLO  = 5,
    SDI_SPLIT_NODES = 100
} sdiSplitMethod;

typedef enum
{
    SDI_SPLIT_TYPE_NONE         = 0,
    SDI_SPLIT_TYPE_DIST         = 1, // HASH, RANGE, LIST, (and so on...)
    SDI_SPLIT_TYPE_NO_DIST      = 2 // CLONE, SOLO, (and so on...)
} sdiSplitType;

typedef struct sdiPrintInfo
{
    UInt           mAnalyzeCount;
    sdiQueryType   mQueryType;
    UShort         mNonShardQueryReason;
    idBool         mTransformable;
} sdiPrintInfo;

#define SDI_INIT_PRINT_INFO( info )                              \
{                                                                \
    (info)->mAnalyzeCount        = 0;                            \
    (info)->mQueryType           = SDI_QUERY_TYPE_NONE;          \
    (info)->mTransformable       = ID_FALSE;                     \
    (info)->mNonShardQueryReason = SDI_NON_SHARD_QUERY_REASON_MAX; \
}

/* PROJ-2733-DistTxInfo */
typedef enum
{
    SDI_DIST_LEVEL_INIT     = 0,
    SDI_DIST_LEVEL_SINGLE   = 1,
    SDI_DIST_LEVEL_MULTI    = 2,
    SDI_DIST_LEVEL_PARALLEL = 3
} sdiDistLevel;

typedef enum {
    SDI_NON_SHARD_OBJECT                = 0,
    SDI_SINGLE_SHARD_KEY_DIST_OBJECT    = 1,
    SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT = 2,
    SDI_CLONE_DIST_OBJECT               = 3,
    SDI_SOLO_DIST_OBJECT                = 4,
} sdiShardObjectType;

typedef enum
{
    SDI_INTERNAL_OP_NOT      = 0,
    SDI_INTERNAL_OP_NORMAL   = 1,
    SDI_INTERNAL_OP_FAILOVER = 2,
    SDI_INTERNAL_OP_FAILBACK = 3,
    SDI_INTERNAL_OP_MAX      = 4,
} sdiInternalOperation;

/* call function after transaction end */
typedef void (*sdiZKPendingJobFunc)(ULong, ULong) ;

/* TASK-7219 Shard Transformer Refactoring */
#define SDI_ANALYSIS_FLAG_TYPE_MASK (0x0000000F)
#define SDI_ANALYSIS_FLAG_NON_MASK  (0x00000001)
#define SDI_ANALYSIS_FLAG_NON_FALSE (0x00000000)
#define SDI_ANALYSIS_FLAG_NON_TRUE  (0x00000001)
#define SDI_ANALYSIS_FLAG_TOP_MASK  (0x00000002)
#define SDI_ANALYSIS_FLAG_TOP_FALSE (0x00000000)
#define SDI_ANALYSIS_FLAG_TOP_TRUE  (0x00000002)
#define SDI_ANALYSIS_FLAG_SET_MASK  (0x00000004)
#define SDI_ANALYSIS_FLAG_SET_FALSE (0x00000000)
#define SDI_ANALYSIS_FLAG_SET_TRUE  (0x00000004)
#define SDI_ANALYSIS_FLAG_CUR_MASK  (0x00000008)
#define SDI_ANALYSIS_FLAG_CUR_FALSE (0x00000000)
#define SDI_ANALYSIS_FLAG_CUR_TRUE  (0x00000008)

typedef enum
{
    SDI_CQ_AGGR_TRANSFORMABLE       = 0, /* Query Set �м� �� SFWGH �� TransformAble �� ���, ���θ� ǥ�� */
    SDI_CQ_LOCAL_TABLE              = 1, /* INSERT, DELETE, UPDATE ��� Table �� Local Table �� ���      */
    SDI_ANALYSIS_CUR_QUERY_FLAG_MAX = 2  /* CURRENT QUERY SET ���� ������ �ʿ��� ���, �� ���� �߰��Ѵ�.  */
} sdiCurQueryFlag;

typedef enum
{
    SDI_SQ_OUT_REF_NOT_EXISTS       = 0, /* Sub-query �м� �� out reference column���� shard key joining ���θ� ǥ�� */
    SDI_SQ_OUT_DEP_INFO             = 1, /* Query Set �м� �� Outer Column �� ���� ���θ� ǥ��                       */
    SDI_SQ_NON_SHARD                = 2, /* Query Set �м� �� SFWGH �� Non-shard �� ���, ���θ� ǥ��                */
    SDI_SQ_UNSUPPORTED              = 3, /* Shard Keyword, With Alias ���� ó��                                      */
    SDI_ANALYSIS_SET_QUERY_FLAG_MAX = 4, /* QUERY SET BLOCK ���� ������ �ʿ��� ���, �� ���� �߰��Ѵ�.               */
} sdiSetQueryFlag;

typedef enum
{
    SDI_TQ_SUB_KEY_EXISTS           = 0, /* Sub-shard key�� ���� table�� ���� ��                     */
    SDI_PARTIAL_COORD_EXEC_NEEDED   = 1, /* TASK-7219 Non-shard DML */
    SDI_ANALYSIS_TOP_QUERY_FLAG_MAX = 2, /* TOP QUERY SET ���� ������ �ʿ��� ���, �� ���� �߰��Ѵ�. */
} sdiTopQueryFlag;

#define SDI_INIT_ANALYSIS_CUR_QUERY_FLAG( _dst_ )  \
{                                                  \
    _dst_[ SDI_CQ_AGGR_TRANSFORMABLE ] = ID_FALSE; \
    _dst_[ SDI_CQ_LOCAL_TABLE   ]      = ID_FALSE; \
}

#define SDI_INIT_ANALYSIS_SET_QUERY_FLAG( _dst_ )  \
{                                                  \
    _dst_[ SDI_SQ_OUT_REF_NOT_EXISTS ] = ID_FALSE; \
    _dst_[ SDI_SQ_OUT_DEP_INFO ]       = ID_FALSE; \
    _dst_[ SDI_SQ_NON_SHARD ]          = ID_FALSE; \
    _dst_[ SDI_SQ_UNSUPPORTED ]        = ID_FALSE; \
}

#define SDI_INIT_ANALYSIS_TOP_QUERY_FLAG( _dst_ )  \
{                                                  \
    _dst_[ SDI_TQ_SUB_KEY_EXISTS ]     = ID_FALSE; \
    _dst_[ SDI_PARTIAL_COORD_EXEC_NEEDED ]  = ID_FALSE; \
}

#define SDI_INIT_ANALYSIS_FLAG( _dst_ )                          \
{                                                                \
    SDI_INIT_ANALYSIS_CUR_QUERY_FLAG( ( _dst_ ).mCurQueryFlag ); \
    SDI_INIT_ANALYSIS_SET_QUERY_FLAG( ( _dst_ ).mSetQueryFlag ); \
    SDI_INIT_ANALYSIS_TOP_QUERY_FLAG( ( _dst_ ).mTopQueryFlag ); \
    SDI_INIT_NON_SHARD_QUERY_REASON( ( _dst_ ).mNonShardFlag );  \
}

typedef struct sdiAnalysisFlag
{
    idBool mCurQueryFlag[ SDI_ANALYSIS_CUR_QUERY_FLAG_MAX ];
    idBool mSetQueryFlag[ SDI_ANALYSIS_SET_QUERY_FLAG_MAX ];
    idBool mTopQueryFlag[ SDI_ANALYSIS_TOP_QUERY_FLAG_MAX ];
    idBool mNonShardFlag[ SDI_NON_SHARD_QUERY_REASON_MAX ];
} sdiAnalysisFlag;

/* TASK-7219 Non-shard DML */
typedef enum {
    SDI_SHARD_PARTIAL_EXEC_TYPE_NONE  = 0,
    SDI_SHARD_PARTIAL_EXEC_TYPE_COORD = 1,
    SDI_SHARD_PARTIAL_EXEC_TYPE_QUERY = 2
} sdiShardPartialExecType;

#endif /* _O_SDI_TYPES_H_ */
