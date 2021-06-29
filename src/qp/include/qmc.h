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
 * $Id: qmc.h 90785 2021-05-06 07:26:22Z hykim $
 *
 * Description :
 *     Execution���� ����ϴ� ���� ����
 *     Materialized Column�� ���� ó���� �ϴ� �۾��� �ָ� �̷��.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMC_H_
#define _O_QMC_H_ 1

#include <mtcDef.h>
#include <qtcDef.h>
#include <qmoDef.h>
#include <qcmTableInfo.h>

// information about plan node's row

typedef SInt qmcRowFlag;

// qmcRowFlag
#define QMC_ROW_INITIALIZE        (0x00000000)

// qmcRowFlag
#define QMC_ROW_DATA_MASK         (0x00000001)
#define QMC_ROW_DATA_EXIST        (0x00000000)
#define QMC_ROW_DATA_NONE         (0x00000001)

// qmcRowFlag
#define QMC_ROW_VALUE_MASK        (0x00000002)
#define QMC_ROW_VALUE_EXIST       (0x00000000)
#define QMC_ROW_VALUE_NULL        (0x00000002)

// qmcRowFlag
#define QMC_ROW_GROUP_MASK        (0x00000004)
#define QMC_ROW_GROUP_NULL        (0x00000000)
#define QMC_ROW_GROUP_SAME        (0x00000004)

// qmcRowFlag
#define QMC_ROW_HIT_MASK          (0x00000008)
#define QMC_ROW_HIT_FALSE         (0x00000000)
#define QMC_ROW_HIT_TRUE          (0x00000008)

// qmcRowFlag, doit retry
#define QMC_ROW_RETRY_MASK        (0x00000010)
#define QMC_ROW_RETRY_FALSE       (0x00000000)
#define QMC_ROW_RETRY_TRUE        (0x00000010)

// BUG-33663 Ranking Function
// qmcRowFlag
#define QMC_ROW_COMPARE_MASK      (0x00000020)
#define QMC_ROW_COMPARE_SAME      (0x00000000)
#define QMC_ROW_COMPARE_DIFF      (0x00000020)

// PROJ-1071
#define QMC_ROW_QUEUE_EMPTY_MASK  (0x00000040)
#define QMC_ROW_QUEUE_EMPTY_FALSE (0x00000000)
#define QMC_ROW_QUEUE_EMPTY_TRUE  (0x00000040)

// PROJ-2750
#define QMC_ROW_NULL_PADDING_MASK     (0x00000080)
#define QMC_ROW_NULL_PADDING_FALSE    (0x00000000)
#define QMC_ROW_NULL_PADDING_TRUE     (0x00000080)

// information about materialization row

// qmcMtrNode.flag
#define QMC_MTR_INITIALIZE             (0x00000000)

//----------------------------------------------------------------------
// PROJ-2179
// 
// Materialized Node�� �����ϴ� Column�� ����
//
// 1. Base table�� ������ �� surrogate key�� �����ϴ� �뵵�� ����Ѵ�.
// QMC_MTR_TYPE_MEMORY_TABLE             : Memory table�� base table(pointer)
// QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE : Partitioned memory table��
//                                         base table(pointer & tuple ID)
// QMC_MTR_TYPE_DISK_TABLE               : Disk table�� base table(RID)
// QMC_MTR_TYPE_DISK_PARTITIONED_TABLE   : Partitioned disk table��
//                                         base table(RID & tuple ID)
// QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE : Hybrid Partitioned Table�� Base Table(Pointer, RID and Tuple ID)
//
// 2. Memory column�� key�� ����ϴ� ���
// QMC_MTR_TYPE_MEMORY_KEY_COLUMN        : Memory table�� column (sorting/hashing key)
//                                         Memory table�� base table(pointer)
// QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN : Memory Partitioned table�� column (sorting/hashing key)
//                                            base table(pointer & tuple ID)
//
// 3. ���� �����ؾ� �ϴ� ���
// QMC_MTR_TYPE_COPY_VALUE               : Disk table�� column, pseudo column
// QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN : Hybrid Partitioned Table�� Column
//
// 4. Expression�� ��� calculate�� ȣ���ϴ� �뵵�� ����Ѵ�.
// QMC_MTR_TYPE_CALCULATE                : �ܼ� column�� �ƴ� expression
// QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE : Subquery ��
//
// 5. MTRNode�� ���� Plan���� ����(���)���� �ʴ� ���
//    ���ʿ��� MTRNode�� Size�� �ּ�ȭ �ϱ� ���� �뵵�� ���ȴ�.
// QMC_MTR_TYPE_USELESS_COLUMN           : Dummy MTRNode
//----------------------------------------------------------------------
#define QMC_MTR_TYPE_MASK                           (0x000000FF)
#define QMC_MTR_TYPE_MEMORY_TABLE                   (0x00000001)
#define QMC_MTR_TYPE_DISK_TABLE                     (0x00000002)
// PROJ-1502 PARTITIONED DISK TABLE
#define QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE       (0x00000003)
#define QMC_MTR_TYPE_DISK_PARTITIONED_TABLE         (0x00000004)
#define QMC_MTR_TYPE_MEMORY_KEY_COLUMN              (0x00000005)
#define QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN    (0x00000006)
#define QMC_MTR_TYPE_COPY_VALUE                     (0x00000007)
#define QMC_MTR_TYPE_CALCULATE                      (0x00000008)
#define QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE       (0x00000009)
// PROJ-2469 Optimize View Materialization
#define QMC_MTR_TYPE_USELESS_COLUMN                 (0x0000000A)
/* PROJ-2464 hybrid partitioned table ���� */
#define QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE       (0x0000000B)
#define QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN    (0x0000000C)

// qmcMtrNode.flag
// ��� Materialized Column�� Sorting�� �ʿ��� ���� ����
#define QMC_MTR_SORT_NEED_MASK                      (0x00000100)
#define QMC_MTR_SORT_NEED_FALSE                     (0x00000000)
#define QMC_MTR_SORT_NEED_TRUE                      (0x00000100)

// qmcMtrNode.flag
// ��� Materialized Column�� Sorting�� �ʿ��� ���, �� Order
#define QMC_MTR_SORT_ORDER_MASK                     (0x00000200)
#define QMC_MTR_SORT_ASCENDING                      (0x00000000)
#define QMC_MTR_SORT_DESCENDING                     (0x00000200)

// qmcMtrNode.flag
// ��� Materialized Column�� Hashing�� �ʿ��� ���� ����
#define QMC_MTR_HASH_NEED_MASK                      (0x00000400)
#define QMC_MTR_HASH_NEED_FALSE                     (0x00000000)
#define QMC_MTR_HASH_NEED_TRUE                      (0x00000400)

// qmcMtrNode.flag
// ��� Materialized Column�� GROUP BY �� ���� �������� ����
#define QMC_MTR_GROUPING_MASK          ( QMC_MTR_HASH_NEED_MASK)
#define QMC_MTR_GROUPING_FALSE         (QMC_MTR_HASH_NEED_FALSE)
#define QMC_MTR_GROUPING_TRUE          ( QMC_MTR_HASH_NEED_TRUE)

// qmcMtrNode.flag
// �ش� Aggregation�� Distinct Column�� ���������� ����
#define QMC_MTR_DIST_AGGR_MASK                      (0x00000800)
#define QMC_MTR_DIST_AGGR_FALSE                     (0x00000000)
#define QMC_MTR_DIST_AGGR_TRUE                      (0x00000800)

// PROJ-1473
// �ش� ����� column Locate�� ����Ǿ�� �Ѵ�.
#define QMC_MTR_CHANGE_COLUMN_LOCATE_MASK           (0x00001000)
#define QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE          (0x00000000)
#define QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE           (0x00001000)

// PROJ-1705
// mtrNode ������, 
#define QMC_MTR_BASETABLE_TYPE_MASK                 (0x00004000)
#define QMC_MTR_BASETABLE_TYPE_DISKTABLE            (0x00000000)
#define QMC_MTR_BASETABLE_TYPE_DISKTEMPTABLE        (0x00004000)

// BUG-31210
// wndNode�� analytic function result
#define QMC_MTR_ANAL_FUNC_RESULT_OF_WND_NODE_MASK   (0x00008000)
#define QMC_MTR_ANAL_FUNC_RESULT_OF_WND_NODE_FALSE  (0x00000000)
#define QMC_MTR_ANAL_FUNC_RESULT_OF_WND_NODE_TRUE   (0x00008000)

// BUG-31210
// materialization plan���� mtrNode�� �����ϴ� ��� 
#define QMC_MTR_MTR_PLAN_MASK                       (0x00010000)
#define QMC_MTR_MTR_PLAN_FALSE                      (0x00000000)
#define QMC_MTR_MTR_PLAN_TRUE                       (0x00010000)

// BUG-33663 Ranking Function
// mtrNode�� order by expr�� ���Ǿ� order�� �����Ǿ������� ����
#define QMC_MTR_SORT_ORDER_FIXED_MASK               (0x00020000)
#define QMC_MTR_SORT_ORDER_FIXED_FALSE              (0x00000000)
#define QMC_MTR_SORT_ORDER_FIXED_TRUE               (0x00020000)

// PROJ-1890 PROWID
// select target �� prowid �� �ִ°�� rid ������ �ʿ���
#define QMC_MTR_RECOVER_RID_MASK                    (0x00040000)
#define QMC_MTR_RECOVER_RID_FALSE                   (0x00000000)
#define QMC_MTR_RECOVER_RID_TRUE                    (0x00040000)

// PROJ-1353 Dist Mtr Node�� �ߺ��ؼ� �߰��ϱ� ���� �ɼ�
#define QMC_MTR_DIST_DUP_MASK                       (0x00080000)
#define QMC_MTR_DIST_DUP_FALSE                      (0x00000000)
#define QMC_MTR_DIST_DUP_TRUE                       (0x00080000)

// PROJ-2362 memory temp ���� ȿ���� ����
#define QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK           (0x00100000)
#define QMC_MTR_TEMP_VAR_TYPE_ENABLE_FALSE          (0x00000000)
#define QMC_MTR_TEMP_VAR_TYPE_ENABLE_TRUE           (0x00100000)

// PROJ-2362 memory temp ���� ȿ���� ����
#define QMC_MTR_BASETABLE_MASK                      (0x00200000)
#define QMC_MTR_BASETABLE_FALSE                     (0x00000000)
#define QMC_MTR_BASETABLE_TRUE                      (0x00200000)

/* PROJ-2435 order by nulls first/last */
#define QMC_MTR_SORT_NULLS_ORDER_MASK               (0x00C00000)
#define QMC_MTR_SORT_NULLS_ORDER_NONE               (0x00000000)
#define QMC_MTR_SORT_NULLS_ORDER_FIRST              (0x00400000)
#define QMC_MTR_SORT_NULLS_ORDER_LAST               (0x00800000)

/* BUG-39837 */
#define QMC_MTR_REMOTE_TABLE_MASK                   (0x01000000)
#define QMC_MTR_REMOTE_TABLE_TRUE                   (0x01000000)
#define QMC_MTR_REMOTE_TABLE_FALSE                  (0x00000000)

/* PROJ-2462 */
#define QMC_MTR_LOB_EXIST_MASK                      (0x02000000)
#define QMC_MTR_LOB_EXIST_TRUE                      (0x02000000)
#define QMC_MTR_LOB_EXIST_FALSE                     (0x00000000)

/* PROJ-2462 */
#define QMC_MTR_PRIOR_EXIST_MASK                    (0x04000000)
#define QMC_MTR_PRIOR_EXIST_TRUE                    (0x04000000)
#define QMC_MTR_PRIOR_EXIST_FALSE                   (0x00000000)

// PROJ-2362 memory temp ���� ȿ���� ����
// 1. �������� �÷�
// 2. non-padding type�� ���
// 3. 10 byte�̻��� ���
// (10 byte char�� ��� precision 8���� ��� 4���ڸ� �����Ѵٰ� �ϸ�
// 6 byte�̰� 2 byte header�� �߰��ϸ� 8 byte�� �Ǿ� 2 byte�� �̵��� �߻��Ѵ�.)
#if defined(DEBUG)
#define QMC_IS_MTR_TEMP_VAR_COLUMN( aMtcColumn )                        \
    ( ( ( ( (aMtcColumn).module->flag & MTD_VARIABLE_LENGTH_TYPE_MASK ) \
          == MTD_VARIABLE_LENGTH_TYPE_TRUE ) )                          \
      ? ID_TRUE : ID_FALSE )
#else
#define QMC_IS_MTR_TEMP_VAR_COLUMN( aMtcColumn )                        \
    ( ( ( ( (aMtcColumn).module->flag & MTD_VARIABLE_LENGTH_TYPE_MASK ) \
          == MTD_VARIABLE_LENGTH_TYPE_TRUE ) &&                         \
        ( (aMtcColumn).module->id != MTD_NCHAR_ID ) &&                  \
        ( (aMtcColumn).module->id != MTD_CHAR_ID ) &&                   \
        ( (aMtcColumn).module->id != MTD_BIT_ID ) &&                    \
        ( (aMtcColumn).column.size >= 10 ) )                            \
      ? ID_TRUE : ID_FALSE )
#endif

class qmcMemory;

//
//  qmc : only in Code Plan
//  qmd : only in Data Plan
//

typedef struct qmcMemSortElement
{
    qmcRowFlag flag;
} qmcMemSortElement;

typedef struct qmcMemHashElement
{
    qmcRowFlag        flag;
    UInt              key;
    qmcMemHashElement  * next;
} qmcMemHashElement;

//-------------------------------------------------
// �� Temp Table�� Record Header Size
//-------------------------------------------------

#define QMC_MEMSORT_TEMPHEADER_SIZE                                     \
    ( idlOS::align(ID_SIZEOF(qmcMemSortElement), ID_SIZEOF(SDouble)) )
#define QMC_MEMHASH_TEMPHEADER_SIZE                                     \
    ( idlOS::align(ID_SIZEOF(qmcMemHashElement), ID_SIZEOF(SDouble)) )

/* PROJ-2201 Innovation in sorting and hashing(temp)
 * DiskTemp�� RowHeader�� ���� */
#define QMC_DISKSORT_TEMPHEADER_SIZE (0)
#define QMC_DISKHASH_TEMPHEADER_SIZE ( QMC_DISKSORT_TEMPHEADER_SIZE )

typedef struct qmcViewElement
{
    qmcViewElement * next;
} qmcViewElement;

typedef struct qmdNode
{
    qtcNode      * dstNode;
    mtcTuple     * dstTuple;     // means my Tuple,  just named with qmdMtrNode
    mtcColumn    * dstColumn;    // means my Column, just named with qmdMtrNode
    qmdNode      * next;
} qmdNode;

typedef struct qmcMemPartRowInfo
{
    scGRID         grid;            // BUG-38309 �޸� ��Ƽ���϶��� rid �� �����ؾ� �Ѵ�.
    UShort         partitionTupleID;

    SChar        * position;
} qmcMemPartRowInfo;

typedef struct qmcDiskPartRowInfo
{
    scGRID         grid;
    UShort         partitionTupleID;

    // index table scan�� ��� index table rid�� ������ �ʿ䰡 �ִ�.
    scGRID         indexGrid;
} qmcDiskPartRowInfo;

/* PROJ-2464 hybrid partitioned table ���� */
typedef struct qmcPartRowInfo
{
    union
    {
        qmcMemPartRowInfo   memory;
        qmcDiskPartRowInfo  disk;
    } unionData;

    idBool         isDisk;
} qmcPartRowInfo;

struct qmcMtrNode;
struct qmdMtrNode;
struct qmdAggrNode;
struct qmdDistNode;

typedef IDE_RC (* qmcSetMtrFunc ) ( qcTemplate  * aTemplate,
                                    qmdMtrNode  * aNode,
                                    void        * aRow );

typedef IDE_RC (* qmcSetTupleFunc ) ( qcTemplate * aTemplate,
                                      qmdMtrNode * aNode,
                                      void       * aRow );

typedef UInt (* qmcGetHashFunc ) ( UInt         aValue,
                                   qmdMtrNode * aNode,
                                   void       * aRow );

typedef idBool (* qmcIsNullFunc ) ( qmdMtrNode * aNode,
                                    void       * aRow );

typedef void (* qmcMakeNullFunc ) ( qmdMtrNode * aNode,
                                    void       * aRow );

typedef void * (* qmcGetRowFunc) ( qmdMtrNode * aNode, const void * aRow );

typedef struct qmdMtrFunction
{
    qmcSetMtrFunc     setMtr;        // Materialized Column ���� ���
    qmcGetHashFunc    getHash;       // Column�� Hash�� ȹ�� ���
    qmcIsNullFunc     isNull;        // Column�� NULL ���� �Ǵ� ���
    qmcMakeNullFunc   makeNull;      // Column�� NULL Value ���� ���
    qmcGetRowFunc     getRow;        // Column�� �������� �ش� Row ȹ�� ���
    qmcSetTupleFunc   setTuple;      // Column�� ������Ű�� ���
    mtdCompareFunc    compare;       // Column�� ��� �� ���
    mtcColumn       * compareColumn; // Compare�� ������ �Ǵ� Column ����
} qmdMtrFunction;

//---------------------------------------------
// Code ������ ���� Column�� ����
//---------------------------------------------

typedef struct qmcMtrNode
{
    //  materialization dstNode from srcNode
    //  if POINTER materialization, srcNode and dstNode are different
    //  if VALUE materialization, srcNode and dstNode are same
    qtcNode         * srcNode;
    qtcNode         * dstNode;
    qmcMtrNode      * next;

    UInt              flag;

    // only for AGGR
    qmcMtrNode      * myDist;    // only for Distinct Aggregation, or NULL
    UInt              bucketCnt; // only for Distinct Aggregation

    // only for GRST
    // qtcNode         * origNode; // TODO : A4 - Grouping Set
} qmcMtrNode;

//---------------------------------------------
// Data ������ ���� Column�� ����
//---------------------------------------------

typedef struct qmdMtrNode
{
    qtcNode               * dstNode;
    mtcTuple              * dstTuple;
    mtcColumn             * dstColumn;
    qmdMtrNode            * next;
    qtcNode               * srcNode;
    mtcTuple              * srcTuple;
    mtcColumn             * srcColumn;    

    qmcMtrNode            * myNode;
    qmdMtrFunction          func;
    UInt                    flag;

    smiFetchColumnList    * fetchColumnList; // PROJ-1705
    mtcTemplate           * tmplate;  // BUG-39896
    
} qmdMtrNode;

// attribute���� flag

// Expression�� ����� �������� �״�� ���޹����� ����
#define QMC_ATTR_SEALED_MASK            (0x00000001)
#define QMC_ATTR_SEALED_FALSE           (0x00000000)
#define QMC_ATTR_SEALED_TRUE            (0x00000001)

// Hashing/sorting�� �ʿ����� ����
#define QMC_ATTR_KEY_MASK               (0x00000002)
#define QMC_ATTR_KEY_FALSE              (0x00000000)
#define QMC_ATTR_KEY_TRUE               (0x00000002)

// Sorting�� �ʿ��� ��� ����
#define QMC_ATTR_SORT_ORDER_MASK        (0x00000004)
#define QMC_ATTR_SORT_ORDER_ASC         (0x00000000)
#define QMC_ATTR_SORT_ORDER_DESC        (0x00000004)

// Conversion�� ������ ä �߰����� ����
#define QMC_ATTR_CONVERSION_MASK        (0x00000008)
#define QMC_ATTR_CONVERSION_FALSE       (0x00000000)
#define QMC_ATTR_CONVERSION_TRUE        (0x00000008)

// Distinct ���� ���� expression���� ����
#define QMC_ATTR_DISTINCT_MASK          (0x00000010)
#define QMC_ATTR_DISTINCT_FALSE         (0x00000000)
#define QMC_ATTR_DISTINCT_TRUE          (0x00000010)

// Terminal���� ����(push down���� �ʴ´�)
#define QMC_ATTR_TERMINAL_MASK          (0x00000020)
#define QMC_ATTR_TERMINAL_FALSE         (0x00000000)
#define QMC_ATTR_TERMINAL_TRUE          (0x00000020)

// Analytic function���� ����
#define QMC_ATTR_ANALYTIC_FUNC_MASK     (0x00000040)
#define QMC_ATTR_ANALYTIC_FUNC_FALSE    (0x00000000)
#define QMC_ATTR_ANALYTIC_FUNC_TRUE     (0x00000040)

// Analytic function�� order by���� ����
#define QMC_ATTR_ANALYTIC_SORT_MASK     (0x00000080)
#define QMC_ATTR_ANALYTIC_SORT_FALSE    (0x00000000)
#define QMC_ATTR_ANALYTIC_SORT_TRUE     (0x00000080)

// ORDER BY������ �����Ǿ����� ����
#define QMC_ATTR_ORDER_BY_MASK          (0x00000100)
#define QMC_ATTR_ORDER_BY_FALSE         (0x00000000)
#define QMC_ATTR_ORDER_BY_TRUE          (0x00000100)

// PROJ-1353 ROLLUP, CUBE�� ���ڷ� ���� �÷��� Group by�� ���� �÷��� ���п뵵
#define QMC_ATTR_GROUP_BY_EXT_MASK      (0x00000200)
#define QMC_ATTR_GROUP_BY_EXT_FALSE     (0x00000000)
#define QMC_ATTR_GROUP_BY_EXT_TRUE      (0x00000200)

/* PROJ-2435 order by nulls first/last */
#define QMC_ATTR_SORT_NULLS_ORDER_MASK  (0x00000C00)
#define QMC_ATTR_SORT_NULLS_ORDER_NONE  (0x00000000)
#define QMC_ATTR_SORT_NULLS_ORDER_FIRST (0x00000400)
#define QMC_ATTR_SORT_NULLS_ORDER_LAST  (0x00000800)

/* PROJ-2469 Optimize View Materialization */
#define QMC_ATTR_USELESS_RESULT_MASK    (0x00001000)
#define QMC_ATTR_USELESS_RESULT_FALSE   (0x00000000)
#define QMC_ATTR_USELESS_RESULT_TRUE    (0x00001000)

// qmc::appendAttribute�� �ɼ�

// �ߺ��� ����� ������ ����
#define QMC_APPEND_ALLOW_DUP_MASK       (0x00000001)
#define QMC_APPEND_ALLOW_DUP_FALSE      (0x00000000)
#define QMC_APPEND_ALLOW_DUP_TRUE       (0x00000001)

// ��� �Ǵ� bind ������ ����� ������ ����
#define QMC_APPEND_ALLOW_CONST_MASK     (0x00000002)
#define QMC_APPEND_ALLOW_CONST_FALSE    (0x00000000)
#define QMC_APPEND_ALLOW_CONST_TRUE     (0x00000002)

// Expression�� ��� �״�� �߰��� ������ ���� ������ҵ��� �߰��Ұ����� ����
#define QMC_APPEND_EXPRESSION_MASK      (0x00000004)
#define QMC_APPEND_EXPRESSION_FALSE     (0x00000000)
#define QMC_APPEND_EXPRESSION_TRUE      (0x00000004)

// Analytic function�� analytic��(OVER�� ����) �˻� ����
#define QMC_APPEND_CHECK_ANALYTIC_MASK  (0x00000008)
#define QMC_APPEND_CHECK_ANALYTIC_FALSE (0x00000000)
#define QMC_APPEND_CHECK_ANALYTIC_TRUE  (0x00000008)

#define QMC_NEED_CALC( aQtcNode )                               \
    ( ( ( (aQtcNode)->node.module != &qtc::columnModule ) &&    \
        ( (aQtcNode)->node.module != &gQtcRidModule     ) &&    \
        ( (aQtcNode)->node.module != &qtc::valueModule  ) &&    \
        ( QTC_IS_AGGREGATE(aQtcNode) == ID_FALSE )              \
      ) ? ID_TRUE : ID_FALSE )

typedef struct qmcAttrDesc
{
    UInt                 flag;
    qtcNode            * expr;
    struct qmcAttrDesc * next;
} qmcAttrDesc;

class qmc
{
public:

    //-----------------------------------------------------
    // Default Execute �Լ� Pointer
    //-----------------------------------------------------

    static mtcExecute      valueExecute;

    static IDE_RC          calculateValue( mtcNode*     aNode,
                                           mtcStack*    aStack,
                                           SInt         aRemain,
                                           void*        aInfo,
                                           mtcTemplate* aTemplate );

    //-----------------------------------------------------
    // [setMtr �迭 �Լ�]
    // Column�� Materialized Row�� �����ϴ� ���
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static IDE_RC setMtrNA( qcTemplate  * aTemplate,
                            qmdMtrNode  * aNode,
                            void        * aRow );

    // Pointer�� Materialized Row�� ����.
    static IDE_RC setMtrByPointer( qcTemplate  * aTemplate,
                                   qmdMtrNode  * aNode,
                                   void        * aRow );

    // RID�� Materialized Row�� ����
    static IDE_RC setMtrByRID( qcTemplate  * aTemplate,
                               qmdMtrNode  * aNode,
                               void        * aRow );

    // ���� ������ Value ��ü�� Materialized Row�� ����
    static IDE_RC setMtrByValue( qcTemplate  * aTemplate,
                                 qmdMtrNode  * aNode,
                                 void        * aRow);

    // Column�� �����Ͽ� Materialized Row�� ����
    static IDE_RC setMtrByCopy( qcTemplate  * aTemplate,
                                qmdMtrNode  * aNode,
                                void        * aRow);

    // Source Column�� ���� ����� Materialized Row�� ����
    static IDE_RC setMtrByConvert( qcTemplate  * aTemplate,
                                   qmdMtrNode  * aNode,
                                   void        * aRow);

    /* PROJ-2464 hybrid partitioned table ���� */
    static IDE_RC setMtrByCopyOrConvert( qcTemplate  * aTemplate,
                                         qmdMtrNode  * aNode,
                                         void        * aRow );

    // PROJ-2469 Optimize View Materialization
    static IDE_RC setMtrByNull( qcTemplate * aTemplate,
                                qmdMtrNode * aNode,
                                void       * aRow );

    // PROJ-1502 PARTITIONED DISK TABLE
    // Partitioned table�� ��� tuple id�� �����ؾ� ��.
    static IDE_RC setMtrByPointerAndTupleID( qcTemplate  * aTemplate,
                                             qmdMtrNode  * aNode,
                                             void        * aRow);

    static IDE_RC setMtrByRIDAndTupleID( qcTemplate  * aTemplate,
                                         qmdMtrNode  * aNode,
                                         void        * aRow);

    /* PROJ-2464 hybrid partitioned table ���� */
    static IDE_RC setMtrByPointerOrRIDAndTupleID( qcTemplate  * aTemplate,
                                                  qmdMtrNode  * aNode,
                                                  void        * aRow );

    // Pointer�� ����� Column���κ��� row pointerȹ��
    static void * getRowByPointerAndTupleID( qmdMtrNode * aNode,
                                             const void * aRow );

    // Pointer�� ����� Column���κ��� NULL ���� �Ǵ�
    static idBool   isNullByPointerAndTupleID( qmdMtrNode * aNode,
                                               void       * aRow );
    
    // Pointer�� ����� Column���κ��� Hash �� ȹ��
    static UInt   getHashByPointerAndTupleID( UInt         aValue,
                                              qmdMtrNode * aNode,
                                              void       * aRow );
    
    //-----------------------------------------------------
    // [setTuple �迭 �Լ�]
    // Materialized Row�� ����� Column�� �����ϴ� ���
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static IDE_RC   setTupleNA( qcTemplate * aTemplate,
                                qmdMtrNode * aNode,
                                void       * aRow );

    // Materialized Row�� Pointer�� ����� Column�� ����
    static IDE_RC   setTupleByPointer( qcTemplate * aTemplate,
                                       qmdMtrNode * aNode,
                                       void       * aRow );

    // Materialized Row�� Disk Table�� ���� RID�� ����� Column�� ����
    static IDE_RC   setTupleByRID( qcTemplate * aTemplate,
                                   qmdMtrNode * aNode,
                                   void       * aRow );

    // Materialized Row�� Value��ü�� ����� Column�� ����
    static IDE_RC   setTupleByValue( qcTemplate * aTemplate,
                                     qmdMtrNode *  aNode,
                                     void       * aRow );

    // PROJ-2469 Optmize View Materialization
    static IDE_RC   setTupleByNull( qcTemplate * aTemplate,
                                    qmdMtrNode * aNode,
                                    void       * aRow );    

    // PROJ-1502 PARTITIONED DISK TABLE
    // Partitioned table�� ��� tuple id�� �����ؾ� ��.
    static IDE_RC   setTupleByRIDAndTupleID( qcTemplate * aTemplate,
                                             qmdMtrNode * aNode,
                                             void       * aRow );

    static IDE_RC   setTupleByPointerAndTupleID( qcTemplate * aTemplate,
                                                 qmdMtrNode * aNode,
                                                 void       * aRow );

    /* PROJ-2464 hybrid partitioned table ���� */
    static IDE_RC   setTupleByPointerOrRIDAndTupleID( qcTemplate * aTemplate,
                                                      qmdMtrNode * aNode,
                                                      void       * aRow );

    //-----------------------------------------------------
    // [getHash �迭 �Լ�]
    // Materialized Row�� ����� Column�� Hash���� ��� ���
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static UInt   getHashNA( UInt         aValue,
                             qmdMtrNode * aNode,
                             void       * aRow );

    // Pointer�� ����� Column���κ��� Hash �� ȹ��
    static UInt   getHashByPointer( UInt         aValue,
                                    qmdMtrNode * aNode,
                                    void       * aRow );

    // Value�� ����� Column���κ��� Hash �� ȹ��
    static UInt   getHashByValue( UInt         aValue,
                                  qmdMtrNode * aNode,
                                  void       * aRow );

    //-----------------------------------------------------
    // [isNull �迭 �Լ�]
    // Materialized Row�� ����� Column�� NULL���� �Ǵ� ���
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static idBool   isNullNA( qmdMtrNode * aNode,
                              void       * aRow );

    // Pointer�� ����� Column���κ��� NULL ���� �Ǵ�
    static idBool   isNullByPointer( qmdMtrNode * aNode,
                                     void       * aRow );

    // Value�� ����� Column���κ��� NULL ���� �Ǵ�
    static idBool   isNullByValue( qmdMtrNode * aNode,
                                   void       * aRow );

    //-----------------------------------------------------
    // [makeNull �迭 �Լ�]
    // Materialized Row�� ����� Column�� NULL���� �Ǵ� ���
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static void   makeNullNA( qmdMtrNode * aNode,
                              void       * aRow );

    // Null Value�� �������� ����
    static void   makeNullNothing( qmdMtrNode * aNode,
                                   void       * aRow );

    // Null Value�� ����
    static void   makeNullValue( qmdMtrNode * aNode,
                                 void       * aRow );

    //-----------------------------------------------------
    // [getRow �迭 �Լ�]
    // Materialized Row�� ����� Column�� ���� row pointerȹ�� �Լ�
    //-----------------------------------------------------

    // ������ �ʴ� �Լ�
    static void * getRowNA( qmdMtrNode * aNode,
                            const void * aRow );

    // Pointer�� ����� Column���κ��� row pointerȹ��
    static void * getRowByPointer ( qmdMtrNode * aNode,
                                    const void * aRow );

    // Value�� ����� Column���κ��� row pointerȹ��
    static void * getRowByValue ( qmdMtrNode * aNode,
                                  const void * aRow );

    //-----------------------------------------------------
    // [��Ÿ ���� �Լ�]
    //-----------------------------------------------------

    //---------------------------------------------
    // ���� Node���� �ùٸ� ����� ���ؼ��� ������ ����
    // ������ �����ϸ� ó���Ͽ��� �Ѵ�.
    //     ::linkMtrNode()
    //     ::initMtrNode()
    //     ::refineOffsets()
    //     ::setRowSize()
    //     ::getRowSize()
    //---------------------------------------------

    // 1. Data ������ ���� Node�� ����
    static IDE_RC linkMtrNode( const qmcMtrNode * aCodeNode,
                               qmdMtrNode       * aDataNode );

    // 2. Data ������ ���� Node�� �ʱ�ȭ
    static IDE_RC initMtrNode( qcTemplate * aTemplate,
                               qmdMtrNode * aDataNode,
                               UShort       aBaseTableCount );

    // 3. ����Ǵ� Column���� offset�� ��������.
    static IDE_RC refineOffsets( qmdMtrNode * aNode, UInt aStartOffset );

    // 4. Tuple ������ Size�� ����ϰ�, Memory�� �Ҵ���.
    static IDE_RC setRowSize( iduMemory  * aMemory,
                              mtcTemplate* aTemplate,
                              UShort       aTupleID );

    // 5. ���� Row�� Size�� ����
    static UInt   getMtrRowSize( qmdMtrNode * aNode );

    // Materialized Column�� ó���� �Լ� �����͸� ����
    static IDE_RC setFunctionPointer( qmdMtrNode * aDataNode );

    // Materialized Column�� �� �Լ� �����͸� ����
    static IDE_RC setCompareFunction( qmdMtrNode * aDataNode );

    /* PROJ-2464 hybrid partitioned table ���� */
    static UInt getRowOffsetForTuple( mtcTemplate * aTemplate,
                                      UShort        aTupleID );

    // PROJ-2179
    // Result descriptor�� �ٷ�� ���� �Լ���
    static IDE_RC findAttribute( qcStatement  * aStatement,
                                 qmcAttrDesc  * aResult,
                                 qtcNode      * aExpr,
                                 idBool         aMakePassNode,
                                 qmcAttrDesc ** aAttr );

    static IDE_RC makeReference( qcStatement  * aStatement,
                                 idBool         aMakePassNode,
                                 qtcNode      * aRefNode,
                                 qtcNode     ** aDepNode );

    static IDE_RC makeReferenceResult( qcStatement * aStatement,
                                       idBool        aMakePassNode,
                                       qmcAttrDesc * aParent,
                                       qmcAttrDesc * aChild );

    static IDE_RC appendPredicate( qcStatement  * aStatement,
                                   qmsQuerySet  * aQuerySet,
                                   qmcAttrDesc ** aResult,
                                   qtcNode      * aPredicate,
                                   UInt           aFlag = 0 );

    static IDE_RC appendPredicate( qcStatement  * aStatement,
                                   qmsQuerySet  * aQuerySet,
                                   qmcAttrDesc ** aResult,
                                   qmoPredicate * aPredicate,
                                   UInt           aFlag = 0 );

    static IDE_RC appendAttribute( qcStatement  * aStatement,
                                   qmsQuerySet  * aQuerySet,
                                   qmcAttrDesc ** aResult,
                                   qtcNode      * aExpr,
                                   UInt           aFlag,
                                   UInt           aOption,
                                   idBool         aMakePassNode );

    static IDE_RC createResultFromQuerySet( qcStatement  * aStatement,
                                            qmsQuerySet  * aQuerySet,
                                            qmcAttrDesc ** aResult );

    static IDE_RC copyResultDesc( qcStatement        * aStatement,
                                  const qmcAttrDesc  * aParent,
                                  qmcAttrDesc       ** aChild );

    static IDE_RC pushResultDesc( qcStatement  * aStatement,
                                  qmsQuerySet  * aQuerySet,
                                  idBool         aMakePassNode,
                                  qmcAttrDesc  * aParent,
                                  qmcAttrDesc ** aChild );

    static IDE_RC filterResultDesc( qcStatement  * aStatement,
                                    qmcAttrDesc ** aPlan,
                                    qcDepInfo    * aDepInfo,
                                    qcDepInfo    * aDepInfo2 );

    static IDE_RC appendViewPredicate( qcStatement  * aStatement,
                                       qmsQuerySet  * aQuerySet,
                                       qmcAttrDesc ** aResult,
                                       UInt           aFlag );

    static IDE_RC duplicateGroupExpression( qcStatement * aStatement,
                                            qtcNode     * aPredicate );

    static void disableSealTrueFlag( qmcAttrDesc * aResult );
private :

    //-----------------------------------------------------
    // ���� Node �ʱ�ȭ ���� �Լ�
    //-----------------------------------------------------

    // ���� Column���� ������ �����Ѵ�.
    static IDE_RC setMtrNode( qcTemplate * aTemplate,
                              qmdMtrNode * aDataNode,
                              qmcMtrNode * aCodeNode,
                              idBool       aBaseTable );

    //---------------------------
    // �Լ� ������ �ʱ�ȭ ���� �Լ�
    //---------------------------

    // Memory Column�� ���� �Լ� ������ ����
    static IDE_RC setFunction4MemoryColumn( qmdMtrNode * aDataNode );

    // Memory Partition Column�� ���� �Լ� ������ ����
    static IDE_RC setFunction4MemoryPartitionColumn( qmdMtrNode * aDataNode );

    // Disk Column�� ���� �Լ� ������ ����
    static IDE_RC setFunction4DiskColumn( qmdMtrNode * aDataNode );

    // Constant�� ���� �Լ� ������ ����
    static IDE_RC setFunction4Constant( qmdMtrNode * aDataNode );

    // PROJ-1502 PARTITIONED DISK TABLE
    // Disk Table�� Key Column�� ���� �Լ� ������ ����
    static IDE_RC setFunction4DiskKeyColumn( qmdMtrNode * aDataNode );

    //-----------------------------------------------------
    // Offset ���� ���� �Լ�
    //-----------------------------------------------------

    // Memory Column�� ����� ���� offset ����
    static IDE_RC refineOffset4MemoryColumn( qmdMtrNode * aListNode,
                                             qmdMtrNode * aColumnNode,
                                             UInt       * aOffset );

    // Memory Partition Column�� ����� ���� offset ����
    static IDE_RC refineOffset4MemoryPartitionColumn( qmdMtrNode * aListNode,
                                                      qmdMtrNode * aColumnNode,
                                                      UInt       * aOffset );

    // Disk Column�� ����� ���� offset ����
    static IDE_RC refineOffset4DiskColumn( qmdMtrNode * aListNode,
                                           qmdMtrNode * aColumnNode,
                                           UInt       * aOffset );

    static IDE_RC setTupleByPointer4Rid(qcTemplate  * aTemplate,
                                        qmdMtrNode  * aNode,
                                        void        * aRow);

    static IDE_RC appendQuerySetCorrelation( qcStatement  * aStatement,
                                             qmsQuerySet  * aQuerySet,
                                             qmcAttrDesc ** aResult,
                                             qmsQuerySet  * aSubquerySet,
                                             qcDepInfo    * aDepInfo );

    static IDE_RC appendSubqueryCorrelation( qcStatement  * aStatement,
                                             qmsQuerySet  * aQuerySet,
                                             qmcAttrDesc ** aResult,
                                             qtcNode      * aSubqueryNode );
};

#endif /* _O_QMC_H_ */
