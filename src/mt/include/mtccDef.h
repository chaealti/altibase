/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: mtcDef.h 38046 2010-02-03 02:34:16Z peh $
 **********************************************************************/

#ifndef _O_MTC_DEF_H_
# define _O_MTC_DEF_H_ 1

#include <acp.h>
#include <acl.h>
#include <aciTypes.h>
#include <aciConv.h>

/* Partial Key                                       */
# define MTD_PARTIAL_KEY_MINIMUM                    (0)
# define MTD_PARTIAL_KEY_MAXIMUM         (ACP_ULONG_MAX)

//====================================================================
// mtcColumn.flag ����
//====================================================================

/* mtcColumn.flag                                    */
# define MTC_COLUMN_ARGUMENT_COUNT_MASK    (0x00000003)
# define MTC_COLUMN_ARGUMENT_COUNT_MAXIMUM          (2)

/* mtcColumn.flag                                    */
# define MTC_COLUMN_NOTNULL_MASK           (0x00000004)
# define MTC_COLUMN_NOTNULL_TRUE           (0x00000004)
# define MTC_COLUMN_NOTNULL_FALSE          (0x00000000)

/* mtcColumn.flag                                    */
/* Į���� ����Ǵ� ��ġ ����                      */
# define MTC_COLUMN_USE_POSITION_MASK      (0x000003F0)
# define MTC_COLUMN_USE_FROM               (0x00000010)
# define MTC_COLUMN_USE_WHERE              (0x00000020)
# define MTC_COLUMN_USE_GROUPBY            (0x00000040)
# define MTC_COLUMN_USE_HAVING             (0x00000080)
# define MTC_COLUMN_USE_ORDERBY            (0x00000100)
# define MTC_COLUMN_USE_WINDOW             (0x00000200)
# define MTC_COLUMN_USE_TARGET             (0x00000300)

// PROJ-1579 NCHAR
/* mtcColumn.flag                                    */
# define MTC_COLUMN_LITERAL_TYPE_MASK      (0x00003000)
# define MTC_COLUMN_LITERAL_TYPE_NCHAR     (0x00001000)
# define MTC_COLUMN_LITERAL_TYPE_UNICODE   (0x00002000)
# define MTC_COLUMN_LITERAL_TYPE_NORMAL    (0x00000000)

// PROJ-1473
/* mtcColumn.flag                                    */
# define MTC_COLUMN_USE_COLUMN_MASK        (0x00004000)
# define MTC_COLUMN_USE_COLUMN_TRUE        (0x00004000)
# define MTC_COLUMN_USE_COLUMN_FALSE       (0x00000000)

// PROJ-1473
/* mtcColumn.flag                                    */
# define MTC_COLUMN_USE_TARGET_MASK        (0x00008000)
# define MTC_COLUMN_USE_TARGET_TRUE        (0x00008000)
# define MTC_COLUMN_USE_TARGET_FALSE       (0x00000000)

// PROJ-1473
/* mtcColumn.flag                                    */
# define MTC_COLUMN_USE_NOT_TARGET_MASK    (0x00010000)
# define MTC_COLUMN_USE_NOT_TARGET_TRUE    (0x00010000)
# define MTC_COLUMN_USE_NOT_TARGET_FALSE   (0x00000000)

// PROJ-1473
/* mtcColumn.flag                                    */
# define MTC_COLUMN_VIEW_COLUMN_PUSH_MASK  (0x00020000)
# define MTC_COLUMN_VIEW_COLUMN_PUSH_TRUE  (0x00020000)
# define MTC_COLUMN_VIEW_COLUMN_PUSH_FALSE (0x00000000)

/* mtcColumn.flag                                    */
# define MTC_COLUMN_TIMESTAMP_MASK         (0x00040000)
# define MTC_COLUMN_TIMESTAMP_TRUE         (0x00040000)
# define MTC_COLUMN_TIMESTAMP_FALSE        (0x00000000)

/* mtcColumn.flag                                    */
# define MTC_COLUMN_QUEUE_MSGID_MASK       (0x00080000)
# define MTC_COLUMN_QUEUE_MSGID_TRUE       (0x00080000)
# define MTC_COLUMN_QUEUE_MSGID_FALSE      (0x00000000)

// BUG-25470
/* mtcColumn.flag                                    */
# define MTC_COLUMN_OUTER_REFERENCE_MASK   (0x00100000)
# define MTC_COLUMN_OUTER_REFERENCE_TRUE   (0x00100000)
# define MTC_COLUMN_OUTER_REFERENCE_FALSE  (0x00000000)

//====================================================================
// mtcNode.flag ����
//====================================================================

// BUG-34127 Increase maximum arguement count(4095(0x0FFF) -> 12287(0x2FFF))
/* mtcNode.flag                                      */
# define MTC_NODE_ARGUMENT_COUNT_MASK           ACP_UINT64_LITERAL(0x000000000000FFFF)
# define MTC_NODE_ARGUMENT_COUNT_MAXIMUM                                       (12287)

/* mtfModule.flag                                    */
// �����ڰ� �����ϴ� Column�� ������ AVG()�� ���� ������
// SUM()�� COUNT()�������� �����ϱ� ���� ��������
// Column �� �����ؾ� �Ѵ�.
# define MTC_NODE_COLUMN_COUNT_MASK             ACP_UINT64_LITERAL(0x00000000000000FF)

/* mtcNode.flag                                      */
# define MTC_NODE_MASK        ( MTC_NODE_INDEX_MASK |       \
                                MTC_NODE_BIND_MASK  |       \
                                MTC_NODE_DML_MASK   |       \
                                MTC_NODE_EAT_NULL_MASK |    \
                                MTC_NODE_HAVE_SUBQUERY_MASK )

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_OPERATOR_MASK                 ACP_UINT64_LITERAL(0x00000000000F0000)
# define MTC_NODE_OPERATOR_MISC                 ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_NODE_OPERATOR_AND                  ACP_UINT64_LITERAL(0x0000000000010000)
# define MTC_NODE_OPERATOR_OR                   ACP_UINT64_LITERAL(0x0000000000020000)
# define MTC_NODE_OPERATOR_NOT                  ACP_UINT64_LITERAL(0x0000000000030000)
# define MTC_NODE_OPERATOR_EQUAL                ACP_UINT64_LITERAL(0x0000000000040000)
# define MTC_NODE_OPERATOR_NOT_EQUAL            ACP_UINT64_LITERAL(0x0000000000050000)
# define MTC_NODE_OPERATOR_GREATER              ACP_UINT64_LITERAL(0x0000000000060000)
# define MTC_NODE_OPERATOR_GREATER_EQUAL        ACP_UINT64_LITERAL(0x0000000000070000)
# define MTC_NODE_OPERATOR_LESS                 ACP_UINT64_LITERAL(0x0000000000080000)
# define MTC_NODE_OPERATOR_LESS_EQUAL           ACP_UINT64_LITERAL(0x0000000000090000)
# define MTC_NODE_OPERATOR_RANGED               ACP_UINT64_LITERAL(0x00000000000A0000)
# define MTC_NODE_OPERATOR_NOT_RANGED           ACP_UINT64_LITERAL(0x00000000000B0000)
# define MTC_NODE_OPERATOR_LIST                 ACP_UINT64_LITERAL(0x00000000000C0000)
# define MTC_NODE_OPERATOR_FUNCTION             ACP_UINT64_LITERAL(0x00000000000D0000)
# define MTC_NODE_OPERATOR_AGGREGATION          ACP_UINT64_LITERAL(0x00000000000E0000)
# define MTC_NODE_OPERATOR_SUBQUERY             ACP_UINT64_LITERAL(0x00000000000F0000)

/* mtcNode.flag                                      */
# define MTC_NODE_INDIRECT_MASK                 ACP_UINT64_LITERAL(0x0000000000100000)
# define MTC_NODE_INDIRECT_TRUE                 ACP_UINT64_LITERAL(0x0000000000100000)
# define MTC_NODE_INDIRECT_FALSE                ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_COMPARISON_MASK               ACP_UINT64_LITERAL(0x0000000000200000)
# define MTC_NODE_COMPARISON_TRUE               ACP_UINT64_LITERAL(0x0000000000200000)
# define MTC_NODE_COMPARISON_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_LOGICAL_CONDITION_MASK        ACP_UINT64_LITERAL(0x0000000000400000)
# define MTC_NODE_LOGICAL_CONDITION_TRUE        ACP_UINT64_LITERAL(0x0000000000400000)
# define MTC_NODE_LOGICAL_CONDITION_FALSE       ACP_UINT64_LITERAL(0x0000000000000000)

//----------------------------------------------------
// [MTC_NODE_INDEX_USABLE]�� �ǹ�
// mtfModule.mask�� ���� Node���� flag������ �̿��Ͽ�
// �ش� Node�� flag������ Setting�Ͽ� �����ȴ�.
// MTC_NODE_INDEX_USABLE�� �ش� Predicate��
// ������ ���� ������ �����Ѵ�.
//     1. Indexable Operator�� ����
//         - A3�� �޸� A4������ system level��
//           indexable operator�Ӹ� �ƴ϶�,
//           Node Transform���� �̿��Ͽ� �ε�����
//           ����� �� �ִ� user level�� indexable
//           operator���� �ǹ��Ѵ�.
//     2. Column�� ������.
//     3. Column�� Expression�� ���ԵǾ� ���� ����.
//----------------------------------------------------

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_INDEX_MASK                    ACP_UINT64_LITERAL(0x0000000000800000)
# define MTC_NODE_INDEX_USABLE                  ACP_UINT64_LITERAL(0x0000000000800000)
# define MTC_NODE_INDEX_UNUSABLE                ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
// ����� ���� ��忡 ȣ��Ʈ ������ �������� ��Ÿ����.
# define MTC_NODE_BIND_MASK                     ACP_UINT64_LITERAL(0x0000000001000000)
# define MTC_NODE_BIND_EXIST                    ACP_UINT64_LITERAL(0x0000000001000000)
# define MTC_NODE_BIND_ABSENT                   ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_QUANTIFIER_MASK               ACP_UINT64_LITERAL(0x0000000002000000)
# define MTC_NODE_QUANTIFIER_TRUE               ACP_UINT64_LITERAL(0x0000000002000000)
# define MTC_NODE_QUANTIFIER_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_DISTINCT_MASK                 ACP_UINT64_LITERAL(0x0000000004000000)
# define MTC_NODE_DISTINCT_TRUE                 ACP_UINT64_LITERAL(0x0000000004000000)
# define MTC_NODE_DISTINCT_FALSE                ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_GROUP_COMPARISON_MASK         ACP_UINT64_LITERAL(0x0000000008000000)
# define MTC_NODE_GROUP_COMPARISON_TRUE         ACP_UINT64_LITERAL(0x0000000008000000)
# define MTC_NODE_GROUP_COMPARISON_FALSE        ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_GROUP_MASK                    ACP_UINT64_LITERAL(0x0000000010000000)
# define MTC_NODE_GROUP_ANY                     ACP_UINT64_LITERAL(0x0000000010000000)
# define MTC_NODE_GROUP_ALL                     ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_FILTER_MASK                   ACP_UINT64_LITERAL(0x0000000020000000)
# define MTC_NODE_FILTER_NEED                   ACP_UINT64_LITERAL(0x0000000020000000)
# define MTC_NODE_FILTER_NEEDLESS               ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_DML_MASK                      ACP_UINT64_LITERAL(0x0000000040000000)
# define MTC_NODE_DML_UNUSABLE                  ACP_UINT64_LITERAL(0x0000000040000000)
# define MTC_NODE_DML_USABLE                    ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcNode.flag                                      */
// fix BUG-13830
# define MTC_NODE_REESTIMATE_MASK               ACP_UINT64_LITERAL(0x0000000080000000)
# define MTC_NODE_REESTIMATE_TRUE               ACP_UINT64_LITERAL(0x0000000080000000)
# define MTC_NODE_REESTIMATE_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)

/* mtfModule.flag                                    */
// To Fix PR-15434 INDEX JOIN�� ������ ������������ ����
# define MTC_NODE_INDEX_JOINABLE_MASK           ACP_UINT64_LITERAL(0x0000000100000000)
# define MTC_NODE_INDEX_JOINABLE_TRUE           ACP_UINT64_LITERAL(0x0000000100000000)
# define MTC_NODE_INDEX_JOINABLE_FALSE          ACP_UINT64_LITERAL(0x0000000000000000)
// To Fix PR-15291
# define MTC_NODE_INDEX_ARGUMENT_MASK           ACP_UINT64_LITERAL(0x0000000200000000)
# define MTC_NODE_INDEX_ARGUMENT_BOTH           ACP_UINT64_LITERAL(0x0000000200000000)
# define MTC_NODE_INDEX_ARGUMENT_LEFT           ACP_UINT64_LITERAL(0x0000000000000000)

// BUG-15995
/* mtfModule.flag                                    */
# define MTC_NODE_VARIABLE_MASK                 ACP_UINT64_LITERAL(0x0000000400000000)
# define MTC_NODE_VARIABLE_TRUE                 ACP_UINT64_LITERAL(0x0000000400000000)
# define MTC_NODE_VARIABLE_FALSE                ACP_UINT64_LITERAL(0x0000000000000000)

// PROJ-1492
// �������� Ÿ���� validation�ÿ� �����Ǿ����� ��Ÿ����.
/* mtcNode.flag                                      */
# define MTC_NODE_BIND_TYPE_MASK                ACP_UINT64_LITERAL(0x0000000800000000)
# define MTC_NODE_BIND_TYPE_TRUE                ACP_UINT64_LITERAL(0x0000000800000000)
# define MTC_NODE_BIND_TYPE_FALSE               ACP_UINT64_LITERAL(0x0000000000000000)

/* BUG-44762 case when subquery */
# define MTC_NODE_HAVE_SUBQUERY_MASK            ACP_UINT64_LITERAL(0x0000001000000000)
# define MTC_NODE_HAVE_SUBQUERY_TRUE            ACP_UINT64_LITERAL(0x0000001000000000)
# define MTC_NODE_HAVE_SUBQUERY_FALSE           ACP_UINT64_LITERAL(0x0000000000000000)

//fix BUG-17610
# define MTC_NODE_EQUI_VALID_SKIP_MASK          ACP_UINT64_LITERAL(0x0000002000000000)
# define MTC_NODE_EQUI_VALID_SKIP_TRUE          ACP_UINT64_LITERAL(0x0000002000000000)
# define MTC_NODE_EQUI_VALID_SKIP_FALSE         ACP_UINT64_LITERAL(0x0000000000000000)

// fix BUG-22512 Outer Join �迭 Push Down Predicate �� ���, ��� ����
// CASE2, DECODE, DIGEST, DUMP, IS NULL, IS NOT NULL, NVL, NVL2, UserDefined Function �Լ���
// ���ڷ� ���� NULL���� �ٸ� ������ �ٲ� �� �ִ�.
// CASE2, DECODE, DIGEST, DUMP, IS NULL, IS NOT NULL, NVL, NVL2, UserDefined Function �Լ��� 
// outer join ���� ���� push down �� �� ���� function���� ǥ��.
# define MTC_NODE_EAT_NULL_MASK                 ACP_UINT64_LITERAL(0x0000004000000000)
# define MTC_NODE_EAT_NULL_TRUE                 ACP_UINT64_LITERAL(0x0000004000000000)
# define MTC_NODE_EAT_NULL_FALSE                ACP_UINT64_LITERAL(0x0000000000000000)

//----------------------------------------------------
// BUG-19180
// ������� predicate ����� ���� ������ ����
// built-in function���� ��¹���� �����Ѵ�.
// 1. prefix( �������� ��ȣ
//    ��) ABS(...), SUB(...), FUNC1(...)
// 2. prefix_ �������� ����
//    ��) EXISTS @, UNIQUE @, NOT @
// 3. infix �߼���
//    ��) @+@, @=ANY@
// 4. infix_ �߼����� ����
//    ��) @ AND @, @ IN @
// 5. _postfix �ļ����� ����
//    ��) @ IS NULL
// 6. ��Ÿ
//    ��) between, like, cast, concat, count, minus, list ��
//----------------------------------------------------
# define MTC_NODE_PRINT_FMT_MASK                ACP_UINT64_LITERAL(0x00000F0000000000)
# define MTC_NODE_PRINT_FMT_PREFIX_PA           ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_NODE_PRINT_FMT_PREFIX_SP           ACP_UINT64_LITERAL(0x0000010000000000)
# define MTC_NODE_PRINT_FMT_INFIX               ACP_UINT64_LITERAL(0x0000020000000000)
# define MTC_NODE_PRINT_FMT_INFIX_SP            ACP_UINT64_LITERAL(0x0000030000000000)
# define MTC_NODE_PRINT_FMT_POSTFIX_SP          ACP_UINT64_LITERAL(0x0000040000000000)
# define MTC_NODE_PRINT_FMT_MISC                ACP_UINT64_LITERAL(0x0000050000000000)

// PROJ-1473
# define MTC_NODE_COLUMN_LOCATE_CHANGE_MASK     ACP_UINT64_LITERAL(0x0000100000000000)
# define MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE    ACP_UINT64_LITERAL(0x0000100000000000)
# define MTC_NODE_COLUMN_LOCATE_CHANGE_TRUE     ACP_UINT64_LITERAL(0x0000000000000000)

// PROJ-1473
# define MTC_NODE_REMOVE_ARGUMENTS_MASK         ACP_UINT64_LITERAL(0x0000200000000000)
# define MTC_NODE_REMOVE_ARGUMENTS_TRUE         ACP_UINT64_LITERAL(0x0000200000000000)
# define MTC_NODE_REMOVE_ARGUMENTS_FALSE        ACP_UINT64_LITERAL(0x0000000000000000)

// BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻� 
// .SIMPLE :
//          select case i1 when 1 then 777
//                         when 2 then 666
//                         when 3 then 555
//                 else 888
//                 end
//            from t1;
// .SEARCHED:
//           select case when i1=1 then 777
//                       when i1=2 then 666
//                       when i1=3 then 555
//                  else 888
//                  end
//             from t1;
# define MTC_NODE_CASE_TYPE_MASK                ACP_UINT64_LITERAL(0x0000400000000000) 
# define MTC_NODE_CASE_TYPE_SIMPLE              ACP_UINT64_LITERAL(0x0000400000000000) 
# define MTC_NODE_CASE_TYPE_SEARCHED            ACP_UINT64_LITERAL(0x0000000000000000)

// BUG-38133
// BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻� 
// qtc::estimateInternal()�Լ��� �ּ� ����.
# define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_MASK  ACP_UINT64_LITERAL(0x0000800000000000)
# define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_TRUE  ACP_UINT64_LITERAL(0x0000800000000000)
# define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_FALSE ACP_UINT64_LITERAL(0x0000000000000000)

// BUG-28446 [valgrind], BUG-38133
// qtcCalculate_Pass(mtcNode*, mtcStack*, int, void*, mtcTemplate*)
# define MTC_NODE_CASE_EXPRESSION_PASSNODE_MASK  ACP_UINT64_LITERAL(0x0001000000000000)
# define MTC_NODE_CASE_EXPRESSION_PASSNODE_TRUE  ACP_UINT64_LITERAL(0x0001000000000000)
# define MTC_NODE_CASE_EXPRESSION_PASSNODE_FALSE ACP_UINT64_LITERAL(0x0000000000000000)

// PROJ-1492
# define MTC_NODE_IS_DEFINED_TYPE( aNode )                  \
    (                                                       \
        (  ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )        \
             == MTC_NODE_BIND_ABSENT )                      \
           ||                                               \
           ( ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )      \
               == MTC_NODE_BIND_EXIST )                     \
             &&                                             \
             ( ( (aNode)->lflag & MTC_NODE_BIND_TYPE_MASK ) \
               == MTC_NODE_BIND_TYPE_TRUE ) )               \
           )                                                \
        ? ACP_TRUE : ACP_FALSE                              \
        )

# define MTC_NODE_IS_DEFINED_VALUE( aNode )         \
    (                                               \
        ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )   \
          == MTC_NODE_BIND_ABSENT )                 \
        ? ACP_TRUE : ACP_FALSE                      \
        )

/* mtfExtractRange Von likeExtractRange              */
# define MTC_LIKE_KEY_SIZE      ( ACP_ALIGN8( sizeof(acp_uint16_t) + 32 )
# define MTC_LIKE_KEY_PRECISION ( MTC_LIKE_KEY_SIZE - sizeof(acp_uint16_t) )

/* mtvModule.cost                                    */
# define MTV_COST_INFINITE                 ACP_UINT64_LITERAL(0x4000000000000000)
# define MTV_COST_GROUP_PENALTY            ACP_UINT64_LITERAL(0x0001000000000000)
# define MTV_COST_ERROR_PENALTY            ACP_UINT64_LITERAL(0x0000000100000000)
# define MTV_COST_LOSS_PENALTY             ACP_UINT64_LITERAL(0x0000000000010000)
# define MTV_COST_DEFAULT                  ACP_UINT64_LITERAL(0x0000000000000001)

//----------------------------------------------------
// [NATVIE2NUMERIC_PENALTY]
//     [���� ������ ����] ������ ���� �Լ�����
//     Native Type�� �켱������ ����� �� �ְ� �ӽ÷�
//     Setting�ϴ� Penalty ������ mtvTable.cost�� �����
//----------------------------------------------------
# define MTV_COST_NATIVE2NUMERIC_PENALTY ( MTV_COST_LOSS_PENALTY * 3 )


//--------------------------------------------------------------------
// PROJ-2002 Column Security
// [ MTV_COST_SMALL_LOSS_PENALTY ]
//     ��ȣ ����Ÿ Ÿ�Կ��� ���� ����Ÿ Ÿ������ ��ȯ�� �߰��Ǵ� ���
//
// ECHAR_TYPE, EVARCHAR_TYPE => CHAR_TYPE, VARCHAR_TYPE
//--------------------------------------------------------------------
# define MTV_COST_SMALL_LOSS_PENALTY      ( MTV_COST_DEFAULT * 2 )


/* mtcTemplate.rows                                  */
// fix BUG-12500
// # define MTC_TUPLE_ROW_MAXIMUM                   (1024)

// PROJ-1358
// mtcTemplate.rows ��
// 16������ �����Ͽ� 65536 ������ �������� �����Ѵ�.
#define MTC_TUPLE_ROW_INIT_CNT                     (16)
//#define MTC_TUPLE_ROW_INIT_CNT                      (2)
#define MTC_TUPLE_ROW_MAX_CNT           (ACP_USHORT_MAX)   // BUG-17154

//====================================================================
// mtcTuple.flag ����
//====================================================================

/* mtcTuple.flag                                     */
# define MTC_TUPLE_TYPE_MAXIMUM                     (4)
/* BUG-44490 mtcTuple flag Ȯ�� �ؾ� �մϴ�. */
/* 32bit flag ���� ��� �Ҹ��� 64bit�� ���� */
/* type�� UInt���� ULong���� ���� */
# define MTC_TUPLE_TYPE_MASK               ACP_UINT64_LITERAL(0x0000000000000003)
# define MTC_TUPLE_TYPE_CONSTANT           ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_TYPE_VARIABLE           ACP_UINT64_LITERAL(0x0000000000000001)
# define MTC_TUPLE_TYPE_INTERMEDIATE       ACP_UINT64_LITERAL(0x0000000000000002)
# define MTC_TUPLE_TYPE_TABLE              ACP_UINT64_LITERAL(0x0000000000000003)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_SET_MASK         ACP_UINT64_LITERAL(0x0000000000000010)
# define MTC_TUPLE_COLUMN_SET_TRUE         ACP_UINT64_LITERAL(0x0000000000000010)
# define MTC_TUPLE_COLUMN_SET_FALSE        ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_ALLOCATE_MASK    ACP_UINT64_LITERAL(0x0000000000000020)
# define MTC_TUPLE_COLUMN_ALLOCATE_TRUE    ACP_UINT64_LITERAL(0x0000000000000020)
# define MTC_TUPLE_COLUMN_ALLOCATE_FALSE   ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_COPY_MASK        ACP_UINT64_LITERAL(0x0000000000000040)
# define MTC_TUPLE_COLUMN_COPY_TRUE        ACP_UINT64_LITERAL(0x0000000000000040)
# define MTC_TUPLE_COLUMN_COPY_FALSE       ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_SET_MASK        ACP_UINT64_LITERAL(0x0000000000000080)
# define MTC_TUPLE_EXECUTE_SET_TRUE        ACP_UINT64_LITERAL(0x0000000000000080)
# define MTC_TUPLE_EXECUTE_SET_FALSE       ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_ALLOCATE_MASK   ACP_UINT64_LITERAL(0x0000000000000100)
# define MTC_TUPLE_EXECUTE_ALLOCATE_TRUE   ACP_UINT64_LITERAL(0x0000000000000100)
# define MTC_TUPLE_EXECUTE_ALLOCATE_FALSE  ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_COPY_MASK       ACP_UINT64_LITERAL(0x0000000000000200)
# define MTC_TUPLE_EXECUTE_COPY_TRUE       ACP_UINT64_LITERAL(0x0000000000000200)
# define MTC_TUPLE_EXECUTE_COPY_FALSE      ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_SET_MASK            ACP_UINT64_LITERAL(0x0000000000000400)
# define MTC_TUPLE_ROW_SET_TRUE            ACP_UINT64_LITERAL(0x0000000000000400)
# define MTC_TUPLE_ROW_SET_FALSE           ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_ALLOCATE_MASK       ACP_UINT64_LITERAL(0x0000000000000800)
# define MTC_TUPLE_ROW_ALLOCATE_TRUE       ACP_UINT64_LITERAL(0x0000000000000800)
# define MTC_TUPLE_ROW_ALLOCATE_FALSE      ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_COPY_MASK           ACP_UINT64_LITERAL(0x0000000000001000)
# define MTC_TUPLE_ROW_COPY_TRUE           ACP_UINT64_LITERAL(0x0000000000001000)
# define MTC_TUPLE_ROW_COPY_FALSE          ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_SKIP_MASK           ACP_UINT64_LITERAL(0x0000000000002000)
# define MTC_TUPLE_ROW_SKIP_TRUE           ACP_UINT64_LITERAL(0x0000000000002000)
# define MTC_TUPLE_ROW_SKIP_FALSE          ACP_UINT64_LITERAL(0x0000000000000000)

/* mtcTuple.flag                                     */
// Tuple�� ���� ��ü�� ���� ����
# define MTC_TUPLE_STORAGE_MASK            ACP_UINT64_LITERAL(0x0000000000010000)
# define MTC_TUPLE_STORAGE_MEMORY          ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_STORAGE_DISK            ACP_UINT64_LITERAL(0x0000000000010000)

/* mtcTuple.flag                                     */
// �ش� Tuple�� VIEW�� ���� Tuple������ ����
# define MTC_TUPLE_VIEW_MASK               ACP_UINT64_LITERAL(0x0000000000020000)
# define MTC_TUPLE_VIEW_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_VIEW_TRUE               ACP_UINT64_LITERAL(0x0000000000020000)

/* mtcTuple.flag */
// �ش� Tuple�� Plan�� ���� ������ Tuple������ ����
# define MTC_TUPLE_PLAN_MASK               ACP_UINT64_LITERAL(0x0000000000040000)
# define MTC_TUPLE_PLAN_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_PLAN_TRUE               ACP_UINT64_LITERAL(0x0000000000040000)

/* mtcTuple.flag */
// �ش� Tuple�� Materialization Plan�� ���� ������ Tuple
# define MTC_TUPLE_PLAN_MTR_MASK           ACP_UINT64_LITERAL(0x0000000000080000)
# define MTC_TUPLE_PLAN_MTR_FALSE          ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_PLAN_MTR_TRUE           ACP_UINT64_LITERAL(0x0000000000080000)

/* mtcTuple.flag */
// PROJ-1502 PARTITIONED DISK TABLE
// �ش� Tuple�� Partitioned Table�� ���� ������ Tuple
# define MTC_TUPLE_PARTITIONED_TABLE_MASK  ACP_UINT64_LITERAL(0x0000000000100000)
# define MTC_TUPLE_PARTITIONED_TABLE_FALSE ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_PARTITIONED_TABLE_TRUE  ACP_UINT64_LITERAL(0x0000000000100000)

/* mtcTuple.flag */
// PROJ-1502 PARTITIONED DISK TABLE
// �ش� Tuple�� Partition�� ���� ������ Tuple
# define MTC_TUPLE_PARTITION_MASK          ACP_UINT64_LITERAL(0x0000000000200000)
# define MTC_TUPLE_PARTITION_FALSE         ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_PARTITION_TRUE          ACP_UINT64_LITERAL(0x0000000000200000)

/* mtcTuple.flag */
/* BUG-36468 Tuple�� �����ü ��ġ(Location)�� ���� ���� */
# define MTC_TUPLE_STORAGE_LOCATION_MASK   ACP_UINT64_LITERAL(0x0000000000400000)
# define MTC_TUPLE_STORAGE_LOCATION_LOCAL  ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_STORAGE_LOCATION_REMOTE ACP_UINT64_LITERAL(0x0000000000400000)

/* mtcTuple.flag */
/* PROJ-2464 hybrid partitioned table ���� */
// �ش� Tuple�� Hybrid�� ���� ������ Tuple
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK    ACP_UINT64_LITERAL(0x0000000000800000)
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_FALSE   ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE    ACP_UINT64_LITERAL(0x0000000000800000)

/* mtcTuple.flag */
// PROJ-1473
// tuple �� ���� ó����� ����.
// (1) tuple set ������� ó��
// (2) ���ڵ����������� ó��
# define MTC_TUPLE_MATERIALIZE_MASK        ACP_UINT64_LITERAL(0x0000000001000000)
# define MTC_TUPLE_MATERIALIZE_RID         ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_MATERIALIZE_VALUE       ACP_UINT64_LITERAL(0x0000000001000000)

# define MTC_TUPLE_BINARY_COLUMN_MASK      ACP_UINT64_LITERAL(0x0000000002000000)
# define MTC_TUPLE_BINARY_COLUMN_ABSENT    ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_BINARY_COLUMN_EXIST     ACP_UINT64_LITERAL(0x0000000002000000)

# define MTC_TUPLE_VIEW_PUSH_PROJ_MASK     ACP_UINT64_LITERAL(0x0000000004000000)
# define MTC_TUPLE_VIEW_PUSH_PROJ_FALSE    ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_VIEW_PUSH_PROJ_TRUE     ACP_UINT64_LITERAL(0x0000000004000000)

// PROJ-1705
/* mtcTuple.flag */
# define MTC_TUPLE_VSCN_MASK               ACP_UINT64_LITERAL(0x0000000008000000)
# define MTC_TUPLE_VSCN_FALSE              ACP_UINT64_LITERAL(0x0000000000000000)
# define MTC_TUPLE_VSCN_TRUE               ACP_UINT64_LITERAL(0x0000000008000000)

# define MTC_TUPLE_COLUMN_ID_MAXIMUM            (1024)
/* to_char�� ���� date -> char ��ȯ�� precision      */
# define MTC_TO_CHAR_MAX_PRECISION                 (64)

/* mtdModule.compare                                 */
/* mtdModule.partialKey                              */
/* aFlag VON mtk::setPartialKey                      */
# define MTD_COMPARE_ASCENDING                      (0)
# define MTD_COMPARE_DESCENDING                     (1)

# define MTD_COMPARE_MTDVAL_MTDVAL                  (0)
# define MTD_COMPARE_STOREDVAL_MTDVAL               (1)
# define MTD_COMPARE_STOREDVAL_STOREDVAL            (2)

//====================================================================
// mtdModule.flag ����
//====================================================================

/* mtdModule.flag                                    */
# define MTD_GROUP_MASK                    (0x0000000F)
# define MTD_GROUP_MAXIMUM                 (0x00000005)
# define MTD_GROUP_MISC                    (0x00000000)
# define MTD_GROUP_TEXT                    (0x00000001)
# define MTD_GROUP_NUMBER                  (0x00000002)
# define MTD_GROUP_DATE                    (0x00000003)
# define MTD_GROUP_INTERVAL                (0x00000004)

/* mtdModule.flag                                    */
# define MTD_CANON_MASK                    (0x00000030)
# define MTD_CANON_NEEDLESS                (0x00000000)
# define MTD_CANON_NEED                    (0x00000010)
# define MTD_CANON_NEED_WITH_ALLOCATION    (0x00000020)

/* mtdModule.flag                                    */
# define MTD_CREATE_MASK                   (0x00000040)
# define MTD_CREATE_ENABLE                 (0x00000040)
# define MTD_CREATE_DISABLE                (0x00000000)

/* mtdModule.flag                                    */
# define MTD_COLUMN_TYPE_MASK              (0x00000300)
# define MTD_COLUMN_TYPE_FIXED             (0x00000100)
# define MTD_COLUMN_TYPE_VARIABLE          (0x00000000)
# define MTD_COLUMN_TYPE_LOB               (0x00000200)

/* mtdModule.flag                                    */
// Selectivity�� ������ �� �ִ� Data Type������ ����
# define MTD_SELECTIVITY_MASK              (0x00000400)
# define MTD_SELECTIVITY_ENABLE            (0x00000400)
# define MTD_SELECTIVITY_DISABLE           (0x00000000)

/* mtdModule.flag                                    */
// byte Ÿ�Կ� ����
// Greatest, Least, Nvl, Nvl2, Case2, Decode����
// �Լ��� Precision, Scale�� ������ ��쿡��
// �����ϵ��� �ϱ� ���� flag
// byte�� �����ϰ� length �ʵ尡 ����.
# define MTD_NON_LENGTH_MASK               (0x00000800)
# define MTD_NON_LENGTH_TYPE               (0x00000800)
# define MTD_LENGTH_TYPE                   (0x00000000)

// PROJ-1364
/* mtdModule.flag                                    */
// �������迭�� ��ǥ data type�� ���� flag
//-----------------------------------------------------------------------
//            |                                                | ��ǥtype
// ----------------------------------------------------------------------
// �������迭 | CHAR, VARCHAR                                  | VARCHAR
// ----------------------------------------------------------------------
// �������迭 | Native | ������ | BIGINT, INTEGER, SMALLINT    | BIGINT
//            |        |-------------------------------------------------
//            |        | �Ǽ��� | DOUBLE, REAL                 | DOUBLE
//            -----------------------------------------------------------
//            | Non-   | �����Ҽ����� | NUMERIC, DECIMAL,      |
//            | Native |              | NUMBER(p), NUMBER(p,s) | NUMERIC
//            |        |----------------------------------------
//            |        | �����Ҽ����� | FLOAT, NUMBER          |
// ----------------------------------------------------------------------
// "���� "
// �� ���� mtd::comparisonNumberType matrix�� �迭���ڸ� ���ϱ� ����
// MTD_NUMBER_GROUP_TYPE_SHIFT ���� �̿��ؼ� right shift�� �����ϰ� �ȴ�.
// ����, mtdModule.flag���� �� flag�� ��ġ�� ����Ǹ�,
// MTD_NUMBER_GROUP_TYPE_SHIFT�� �׿� �°� ����Ǿ�� ��.
# define MTD_NUMBER_GROUP_TYPE_MASK        (0x00003000)
# define MTD_NUMBER_GROUP_TYPE_NONE        (0x00000000)
# define MTD_NUMBER_GROUP_TYPE_BIGINT      (0x00001000)
# define MTD_NUMBER_GROUP_TYPE_DOUBLE      (0x00002000)
# define MTD_NUMBER_GROUP_TYPE_NUMERIC     (0x00003000)

// PROJ-1362
// ODBC API, SQLGetTypeInfo ������ ���� mtdModule Ȯ��
// �������� Ÿ�������� client�� �ϵ��ڵ��Ǿ� �ִ�����
// �������� ��ȸ�ϵ��� ������
// mtdModule.flag
# define MTD_CREATE_PARAM_MASK             (0x00030000)
# define MTD_CREATE_PARAM_NONE             (0x00000000)
# define MTD_CREATE_PARAM_PRECISION        (0x00010000)
# define MTD_CREATE_PARAM_PRECISIONSCALE   (0x00020000)

# define MTD_CASE_SENS_MASK                (0x00040000)
# define MTD_CASE_SENS_FALSE               (0x00000000)
# define MTD_CASE_SENS_TRUE                (0x00040000)

# define MTD_UNSIGNED_ATTR_MASK            (0x00080000)
# define MTD_UNSIGNED_ATTR_FALSE           (0x00000000)
# define MTD_UNSIGNED_ATTR_TRUE            (0x00080000)

# define MTD_SEARCHABLE_MASK               (0x00300000)
# define MTD_SEARCHABLE_PRED_NONE          (0x00000000)
# define MTD_SEARCHABLE_PRED_CHAR          (0x00100000)
# define MTD_SEARCHABLE_PRED_BASIC         (0x00200000)
# define MTD_SEARCHABLE_SEARCHABLE         (0x00300000)

# define MTD_SQL_DATETIME_SUB_MASK         (0x00400000)
# define MTD_SQL_DATETIME_SUB_FALSE        (0x00000000)
# define MTD_SQL_DATETIME_SUB_TRUE         (0x00400000)

# define MTD_NUM_PREC_RADIX_MASK           (0x00800000)
# define MTD_NUM_PREC_RADIX_FALSE          (0x00000000)
# define MTD_NUM_PREC_RADIX_TRUE           (0x00800000)

# define MTD_LITERAL_MASK                  (0x01000000)
# define MTD_LITERAL_FALSE                 (0x00000000)
# define MTD_LITERAL_TRUE                  (0x01000000)

// PROJ-1705
// define�� mtdDataType�� 
// length ������ ������ ����ŸŸ�������� ���θ� ��Ÿ����.
# define MTD_VARIABLE_LENGTH_TYPE_MASK     (0x02000000)
# define MTD_VARIABLE_LENGTH_TYPE_FALSE    (0x00000000)
# define MTD_VARIABLE_LENGTH_TYPE_TRUE     (0x02000000)

// PROJ-1705
// ��ũ���̺��� ���, 
// SM���� �÷��� ���� row piece��
// ������ �����ص� �Ǵ��� ���θ� ��Ÿ����. 
# define MTD_DATA_STORE_DIVISIBLE_MASK     (0x04000000)
# define MTD_DATA_STORE_DIVISIBLE_FALSE    (0x00000000)
# define MTD_DATA_STORE_DIVISIBLE_TRUE     (0x04000000)

// PROJ-1705
// ��ũ���̺��� ���,
// SM�� mtdValue ���·� ����Ǵ����� ����
// nibble, bit, varbit �� ��쿡 �ش��.
// �� ������ Ÿ���� ����Ǵ� length ������ 
// ����������� ���� length ������ ��Ȯ�ϰ� ���Ҽ� ����
// mtdValueType���� �����Ѵ�. 
# define MTD_DATA_STORE_MTDVALUE_MASK      (0x08000000)
# define MTD_DATA_STORE_MTDVALUE_FALSE     (0x00000000)
# define MTD_DATA_STORE_MTDVALUE_TRUE      (0x08000000)

// PROJ-2002 Column Security
// echar, evarchar ���� ���� Ÿ�� ����
// mtdModule.flag
# define MTD_ENCRYPT_TYPE_MASK             (0x10000000)
# define MTD_ENCRYPT_TYPE_FALSE            (0x00000000)
# define MTD_ENCRYPT_TYPE_TRUE             (0x10000000)

// PROJ-1364
// ������ �迭�� �񱳽�,
// mtd::comparisonNumberType  matrix�� ����
// �񱳱��� data type�� ��� �Ǵµ�,
// �� �迭���ڸ� ��� ����
// MTD_NUMBER_GROUP_TYPE_XXX�� �Ʒ� ����ŭ shift
// "����"
// MTD_NUMBER_GROUP_TYPE_MASK�� ��ġ�� ����Ǹ�,
// �� define ����� �׿� �°� ����Ǿ�� ��.
# define MTD_NUMBER_GROUP_TYPE_SHIFT       (12)

/* aFlag VON mtdFunctions                            */
# define MTD_OFFSET_MASK                   (0x00000001)
# define MTD_OFFSET_USELESS                (0x00000001)
# define MTD_OFFSET_USE                    (0x00000000)

/* mtcCallBack.flag                                  */
// Child Node�� ���� Estimation ���θ� ǥ����
// ENABLE�� ���, Child�� ���� Estimate�� �����ϸ�,
// DISABLE�� ���, �ڽſ� ���� Estimate���� �����Ѵ�.
# define MTC_ESTIMATE_ARGUMENTS_MASK       (0x00000001)
# define MTC_ESTIMATE_ARGUMENTS_DISABLE    (0x00000001)
# define MTC_ESTIMATE_ARGUMENTS_ENABLE     (0x00000000)

// Selectivity ��� �� ��Ȯ�� ����� �Ұ����� ��쿡
// ���õǴ� ��
# define MTD_DEFAULT_SELECTIVITY           (   1.0/3.0)

// PROJ-1484
// Selectivity ��� �� 10������ ��ȯ ������ ���ڿ��� �ִ� ����
# define MTD_CHAR_DIGIT_MAX                (15)

//====================================================================
// mtkRangeCallBack.info ����
//====================================================================

// Ascending/Descending ����
#define MTK_COMPARE_DIRECTION_MASK  (0x00000001)
#define MTK_COMPARE_DIRECTION_ASC   (0x00000000)
#define MTK_COMPARE_DIRECTION_DESC  (0x00000001)

// ���� �迭�� ���� �ٸ� data type���� �� �Լ� ���� 
#define MTK_COMPARE_SAMEGROUP_MASK  (0x00000002)
#define MTK_COMPARE_SAMEGROUP_FALSE (0x00000000)
#define MTK_COMPARE_SAMEGROUP_TRUE  (0x00000002)

//====================================================================
// mtl ���� ����
//====================================================================

// fix BUG-13830
// �� language�� �� ������ �ִ� precision
# define MTL_UTF8_PRECISION      3
# define MTL_UTF16_PRECISION     2  // PROJ-1579 NCHAR
# define MTL_ASCII_PRECISION     1
# define MTL_KSC5601_PRECISION   2
# define MTL_MS949_PRECISION     2
# define MTL_SHIFTJIS_PRECISION  2
# define MTL_MS932_PRECISION     2 /* PROJ-2590 support CP932 character set */
# define MTL_GB231280_PRECISION  2
/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
# define MTL_MS936_PRECISION     2
# define MTL_BIG5_PRECISION      2
# define MTL_EUCJP_PRECISION     3
# define MTL_MAX_PRECISION       3

// PROJ-1579 NCHAR
# define MTL_NCHAR_PRECISION( aMtlModule )          \
    (                                               \
        ( (aMtlModule)->id == MTL_UTF8_ID )         \
        ? MTL_UTF8_PRECISION : MTL_UTF16_PRECISION  \
        )

// PROJ-1361 : language type�� ���� id
// mtlModule.id
# define MTL_DEFAULT          0
# define MTL_UTF8_ID      10000
# define MTL_UTF16_ID     11000     // PROJ-1579 NCHAR
# define MTL_ASCII_ID     20000
# define MTL_KSC5601_ID   30000
# define MTL_MS949_ID     31000
# define MTL_SHIFTJIS_ID  40000
# define MTL_MS932_ID     42000 /* PROJ-2590 support CP932 character set */
# define MTL_EUCJP_ID     41000
# define MTL_GB231280_ID  50000
# define MTL_BIG5_ID      51000
/* PROJ-2414 [��ɼ�] GBK, CP936 character set �߰� */
# define MTL_MS936_ID     52000

typedef struct smiCColumn       smiCColumn;

typedef struct mtcId            mtcId;
typedef struct mtcCallBack      mtcCallBack;
typedef struct mtcCallBackInfo  mtcCallBackInfo;
typedef struct mtcColumn        mtcColumn;
typedef struct mtcColumnLocate  mtcColumnLocate;
typedef struct mtcExecute       mtcExecute;
typedef struct mtcName          mtcName;
typedef struct mtcNode          mtcNode;
typedef struct mtcStack         mtcStack;
typedef struct mtcTuple         mtcTuple;
typedef struct mtcTemplate      mtcTemplate;

typedef struct mtkRangeCallBack mtkRangeCallBack;
typedef struct mtvConvert       mtvConvert;
typedef struct mtvTable         mtvTable;

typedef struct mtfSubModule     mtfSubModule;

typedef struct mtdModule        mtdModule;
typedef struct mtfModule        mtfModule;
typedef struct mtlModule        mtlModule;
typedef struct mtvModule        mtvModule;

typedef struct mtkRangeInfo     mtkRangeInfo;

typedef struct mtdValueInfo 
{
    const mtcColumn * column;
    const void      * value;
    acp_uint32_t      length;
    acp_uint32_t      flag;
} mtdValueInfo;

// PROJ-2002 Column Security
typedef struct mtcEncryptInfo   mtcEncryptInfo;

typedef enum
{
    NC_VALID = 0,         /* �������� 1���� */
    NC_INVALID,           /* firstbyte�� �ڵ忡 �ȸ´� 1���� */
    NC_MB_INVALID,        /* secondbyte���İ� �ڵ忡 �ȸ´� 1���� */
    NC_MB_INCOMPLETED     /* ��Ƽ����Ʈ �����ε� byte�� ���ڸ� 0���� */
} mtlNCRet;

#define MTC_CURSOR_PTR  void*

typedef ACI_RC (*mtcInitSubqueryFunc)( mtcNode     * aNode,
                                       mtcTemplate * aTemplate );

typedef ACI_RC (*mtcFetchSubqueryFunc)( mtcNode     * aNode,
                                        mtcTemplate * aTemplate,
                                        acp_bool_t  * aRowExist );

typedef ACI_RC (*mtcFiniSubqueryFunc)( mtcNode     * aNode,
                                       mtcTemplate * aTemplate );

/* PROJ-2448 Subquery caching */
typedef ACI_RC (*mtcSetCalcSubqueryFunc)( mtcNode     * aNode,
                                          mtcTemplate * aTemplate );

//PROJ-1583 large geometry
typedef acp_uint32_t   (*mtcGetSTObjBufSizeFunc)( mtcCallBack * aCallBack );

typedef ACI_RC (*mtcGetOpenedCursorFunc)( mtcTemplate    * aTemplate,
                                          acp_sint16_t     aTableID,
                                          MTC_CURSOR_PTR * aCursor,
                                          acp_sint16_t   * aOrgTableID,
                                          acp_bool_t     * aFound );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
typedef acp_bool_t (*mtcIsBaseTable)( mtcTemplate * aTemplate,
                                      acp_uint16_t  aTable );

typedef ACI_RC (*mtdInitializeFunc)( acp_uint32_t sNo );

typedef ACI_RC (*mtdEstimateFunc)(  acp_uint32_t * aColumnSize,
                                    acp_uint32_t * aArguments,
                                    acp_sint32_t * aPrecision,
                                    acp_sint32_t * aScale );

typedef ACI_RC (*mtdChangeFunc)( mtcColumn    * aColumn,
                                 acp_uint32_t   aFlag );

typedef ACI_RC (*mtdValueFunc)( mtcTemplate  * aTemplate,  // fix BUG-15947
                                mtcColumn    * aColumn,
                                void         * aValue,
                                acp_uint32_t * aValueOffset,
                                acp_uint32_t   aValueSize,
                                const void   * aToken,
                                acp_uint32_t   aTokenLength,
                                ACI_RC     * aResult );

typedef acp_uint32_t (*mtdActualSizeFunc)( const mtcColumn * aColumn,
                                           const void      * aRow,
                                           acp_uint32_t      aFlag );

typedef ACI_RC (*mtdGetPrecisionFunc)( const mtcColumn * aColumn,
                                       const void      * aRow,
                                       acp_uint32_t      aFlag,
                                       acp_sint32_t    * aPrecision,
                                       acp_sint32_t    * aScale );

typedef void (*mtdNullFunc)( const mtcColumn * aColumn,
                             void            * aRow,
                             acp_uint32_t      aFlag );

typedef acp_uint32_t (*mtdHashFunc)( acp_uint32_t      aHash,
                                     const mtcColumn * aColumn,
                                     const void      * aRow,
                                     acp_uint32_t      aFlag );

typedef acp_bool_t (*mtdIsNullFunc)( const mtcColumn * aColumn,
                                     const void      * aRow,
                                     acp_uint32_t      aFlag );

typedef ACI_RC (*mtdIsTrueFunc)( acp_bool_t      * aResult,
                                 const mtcColumn * aColumn,
                                 const void      * aRow,
                                 acp_uint32_t      aFlag );

typedef acp_sint32_t (*mtdCompareFunc)( mtdValueInfo * aValueInfo1,
                                        mtdValueInfo * aValueInfo2 );

typedef acp_uint8_t (*mtsRelationFunc)( const mtcColumn * aColumn1,
                                        const void      * aRow1,
                                        acp_uint32_t      aFlag1,
                                        const mtcColumn * aColumn2,
                                        const void      * aRow2,
                                        acp_uint32_t      aFlag2,
                                        const void      * aInfo );

typedef ACI_RC (*mtdCanonizeFunc)( const mtcColumn  *  aCanon,
                                   void             ** aCanonized,
                                   mtcEncryptInfo   *  aCanonInfo,
                                   const mtcColumn  *  aColumn,
                                   void             *  aValue,
                                   mtcEncryptInfo   *  aColumnInfo,
                                   mtcTemplate      *  aTemplate );

typedef void (*mtdEndianFunc)( void* aValue );

typedef ACI_RC (*mtdValidateFunc)( mtcColumn  * aColumn,
                                   void       * aValue,
                                   acp_uint32_t aValueSize);

typedef ACI_RC (*mtdEncodeFunc) ( mtcColumn    * aColumn,
                                  void         * aValue,
                                  acp_uint32_t   aValueSize,
                                  acp_uint8_t  * aCompileFmt,
                                  acp_uint32_t   aCompileFmtLen,
                                  acp_uint8_t  * aText,
                                  acp_uint32_t * aTextLen,
                                  ACI_RC     * aRet );

typedef ACI_RC (*mtdDecodeFunc) ( mtcTemplate  * aTemplate,  // fix BUG-15947
                                  mtcColumn    * aColumn,
                                  void         * aValue,
                                  acp_uint32_t * aValueSize,
                                  acp_uint8_t  * aCompileFmt,
                                  acp_uint32_t   aCompileFmtLen,
                                  acp_uint8_t  * aText,
                                  acp_uint32_t   aTextLen,
                                  ACI_RC     * aRet );

typedef ACI_RC (*mtdCompileFmtFunc)	( mtcColumn    * aColumn,
                                      acp_uint8_t  * aCompiledFmt,
                                      acp_uint32_t * aCompiledFmtLen,
                                      acp_uint8_t  * aFormatString,
                                      acp_uint32_t   aFormatStringLength,
                                      ACI_RC     * aRet );

typedef ACI_RC (*mtdValueFromOracleFunc)( mtcColumn    * aColumn,
                                          void         * aValue,
                                          acp_uint32_t * aValueOffset,
                                          acp_uint32_t   aValueSize,
                                          const void   * aOracleValue,
                                          acp_sint32_t   aOracleLength,
                                          ACI_RC     * aResult );

typedef ACI_RC (*mtdMakeColumnInfo) ( void         * aStmt,
                                      void         * aTableInfo,
                                      acp_uint32_t   aIdx );

//-----------------------------------------------------------------------
// Data Type�� Selectivity ���� �Լ�
// Selectivity ����
//     Selectivity = (aValueMax - aValueMin) / (aColumnMax - aColumnMin)
//-----------------------------------------------------------------------

typedef acp_double_t (*mtdSelectivityFunc) ( void * aColumnMax,  // Column�� MAX��
                                             void * aColumnMin,  // Column�� MIN��
                                             void * aValueMax,   // MAX Value ��
                                             void * aValueMin ); // MIN Value ��

typedef ACI_RC (*mtfInitializeFunc)( void );

typedef ACI_RC (*mtfFinalizeFunc)( void );

/***********************************************************************
 * PROJ-1705
 * ��ũ���̺��÷��� ����Ÿ��
 * qp ���ڵ�ó�������� �ش� �÷���ġ�� ����
 *
 *     aColumnSize : ���̺������ ���ǵ� column size
 *     aDestValue : �÷��� value�� ����Ǿ�� �� �޸� �ּ�
 *                  qp : mtcTuple.row�� �Ҵ��
 *                       ��ũ���ڵ带 ���� �޸��� �ش� �÷��� ��ġ
 *                       mtcTuple.row + column.offset    
 *     aDestValueOffset: �÷��� value�� ����Ǿ�� �� �޸��ּҷκ����� offset
 *                       aDestValue�� aOffset 
 *     aLength    : sm�� ����� ���ڵ��� value�� ����� �÷� value�� length
 *     aValue     : sm�� ����� ���ڵ��� value�� ����� �÷� value 
 **********************************************************************/

typedef ACI_RC (*mtdStoredValue2MtdValueFunc) ( acp_uint32_t   aColumnSize,
                                                void         * aDestValue,
                                                acp_uint32_t   aDestValueOffset,
                                                acp_uint32_t   aLength,
                                                const void   * aValue );

/***********************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ
 * replication���� ���.
 **********************************************************************/

typedef acp_uint32_t (*mtdNullValueSizeFunc) ();

/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * ��) mtdCharType( UShort length; UChar value[1] ) ����
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/
typedef acp_uint32_t (*mtdHeaderSizeFunc) ();

//----------------------------------------------------------------
// PROJ-1358
// Estimation �������� SEQUENCE, SYSDATE, LEVEL, Pre-Processing,
// Conversion ���� ���Ͽ� Internal Tuple�� �Ҵ���� �� �ִ�.
// �� �� Internal Tuple Set�� Ȯ��Ǹ�, �Է� ���ڷ� ���Ǵ�
// mtcTuple * �� ��ȿ���� �ʰ� �ȴ�.
// ��, Estimatation�� ���ڴ� Tuple Set�� Ȯ���� ����Ͽ�
// mtcTemplate * �� ���ڷ� �޾� mtcTemplate->rows �� �̿��Ͽ��� �Ѵ�.
// ����) mtfCalculateFunc, mtfEstimateRangeFunc ����
//       Internal Tuple Set�� Ȯ������ ���� ������ �߻����� ������,
//       mtcTuple* �� ���ڷ� ���ϴ� �Լ��� ��� �����Ͽ�
//       ������ ������ �����Ѵ�.
//----------------------------------------------------------------

//----------------------------------------------------------------
// PROJ-1358
// ���� �Ұ����� ��찡 �ƴ϶��, mtcTuple * �� �Լ�����
// ���������ε� ������� ���ƾ� �Ѵ�.
//----------------------------------------------------------------

typedef ACI_RC (*mtfEstimateFunc)( mtcNode      * aNode,
                                   mtcTemplate  * aTemplate,
                                   mtcStack     * aStack,
                                   acp_sint32_t   aRemain,
                                   mtcCallBack  * aCallBack );

//----------------------------------------------------------------
// PROJ-1358
// ����) mtfCalculateFunc, mtfEstimateRangeFunc ����
//       Internal Tuple Set�� Ȯ������ ���� ������ �߻����� ������,
//       mtcTuple* �� ���ڷ� ���ϴ� �Լ��� ��� �����Ͽ�
//       ������ ������ �����Ѵ�.
//----------------------------------------------------------------

typedef ACI_RC (*mtfCalculateFunc)( mtcNode      * aNode,
                                    mtcStack     * aStack,
                                    acp_sint32_t   aRemain,
                                    void         * aInfo,
                                    mtcTemplate  * aTemplate );

typedef ACI_RC (*mtfEstimateRangeFunc)( mtcNode      * aNode,
                                        mtcTemplate  * aTemplate,
                                        acp_uint32_t   aArgument,
                                        acp_uint32_t * aSize );

//----------------------------------------------------------------
// PROJ-1358
// ����) mtfCalculateFunc, mtfEstimateRangeFunc ����
//       Internal Tuple Set�� Ȯ������ ���� ������ �߻����� ������,
//       mtcTuple* �� ���ڷ� ���ϴ� �Լ��� ��� �����Ͽ�
//       ������ ������ �����Ѵ�.
//----------------------------------------------------------------

typedef ACI_RC (*mtfExtractRangeFunc)( mtcNode      * aNode,
                                       mtcTemplate  * aTemplate,
                                       mtkRangeInfo * aInfo,
                                       void         * );

typedef ACI_RC (*mtfInitConversionNodeFunc)( mtcNode ** aConversionNode,
                                             mtcNode *  aNode,
                                             void    *  aInfo );

typedef void (*mtfSetIndirectNodeFunc)( mtcNode * aNode,
                                        mtcNode * aConversionNode,
                                        void    * aInfo );

typedef ACI_RC (*mtfAllocFunc)( void         *  aInfo,
                                acp_uint32_t    aSize,
                                void         **);

//----------------------------------------------------------------
// PROJ-2002 Column Security
// mtcEncrypt        : ��ȣȭ �Լ�
// mtcDecrypt        : ��ȣȭ �Լ�
// mtcGetDecryptInfo : ��ȣȭ�� �ʿ��� ������ ��� �Լ�
// mtcEncodeECC      : ECC(Encrypted Comparison Code) encode �Լ�
//----------------------------------------------------------------

// �Ϻ�ȣȭ�� ���ȸ�⿡ �ʿ��� ������ �����ϱ� ���� �ڷᱸ��
struct mtcEncryptInfo
{
    // session info
    acp_char_t  * ipAddr;
    acp_char_t  * userName;

    // statement info
    acp_char_t  * stmtType;   // select/insert/update/delete

    // column info
    acp_char_t  * tableOwnerName;
    acp_char_t  * tableName;
    acp_char_t  * columnName;
};

// Echar, Evarchar canonize�� ����
typedef ACI_RC (*mtcEncrypt) ( mtcEncryptInfo * aEncryptInfo,
                               acp_char_t     * aPolicyName,
                               acp_uint8_t    * aPlain,
                               acp_uint16_t     aPlainSize,
                               acp_uint8_t    * aCipher,
                               acp_uint16_t   * aCipherSize );

// Echar->Char, Evarchar->Varchar conversion�� ����
typedef ACI_RC (*mtcDecrypt) ( mtcEncryptInfo * aEncryptInfo,
                               acp_char_t     * aPolicyName,
                               acp_uint8_t    * aCipher,
                               acp_uint16_t     aCipherSize,
                               acp_uint8_t    * aPlain,
                               acp_uint16_t   * aPlainSize );

// Char->Echar, Varchar->Evarchar conversion�� ����
typedef ACI_RC (*mtcEncodeECC) ( acp_uint8_t  * aPlain,
                                 acp_uint16_t   aPlainSize,
                                 acp_uint8_t  * aCipher,
                                 acp_uint16_t * aCipherSize );

// decrypt�� ���� decryptInfo�� ������
typedef ACI_RC (*mtcGetDecryptInfo) ( mtcTemplate    * aTemplate,
                                      acp_uint16_t     aTable,
                                      acp_sint16_t     aColumn,
                                      mtcEncryptInfo * aDecryptInfo );

// Echar, Evarchar�� initializeColumn�� ����
typedef ACI_RC (*mtcGetECCSize) ( acp_uint32_t   aSize,
                                  acp_uint32_t * aECCSize );

// PROJ-1579 NCHAR
typedef acp_char_t * (*mtcGetDBCharSet)();
typedef acp_char_t * (*mtcGetNationalCharSet)();

// PROJ-1361 : language�� ���� ���� ���ڷ� �̵�
// PROJ-1755 : nextChar �Լ� ����ȭ
// PROJ-1579,PROJ-1681, BUG-21117
// ��Ȯ�� ���ڿ� üũ�� ���� fence�� nextchar�ȿ��� üũ�ؾ� ��.
typedef mtlNCRet (*mtlNextCharFunc)( acp_uint8_t ** aSource, acp_uint8_t * aFence );

// PROJ-1361 : language�� ���� �׿� �´� chr() �Լ��� ���� ���
typedef ACI_RC (*mtlChrFunc)( acp_uint8_t        * aResult,
                              const acp_sint32_t   aSource );

// PROJ-1361 : mtlExtractFuncSet �Ǵ� mtlNextDayFuncSet���� ���
typedef acp_sint32_t (*mtlMatchFunc)( acp_uint8_t  * aSource,
                                      acp_uint32_t   aSourceLen );

// BUG-13830 ���ڰ����� ���� language�� �ִ� precision ���
typedef acp_sint32_t (*mtlMaxPrecisionFunc)( acp_sint32_t aLength );

struct mtcId {
    acp_uint32_t dataTypeId;
    acp_uint32_t languageId;
};

// Estimate ���� ���� �ΰ� ����
struct mtcCallBack {
    void*                       info;     // (mtcCallBackInfo or qtcCallBackInfo)

    acp_uint32_t                flag;     // Child�� ���� estimate ����
    mtfAllocFunc                alloc;    // Memory �Ҵ� �Լ�

    //----------------------------------------------------------
    // [Conversion�� ó���ϱ� ���� �Լ� ������]
    //
    // initConversionNode
    //    : Conversion Node�� ���� �� �ʱ�ȭ
    //    : Estimate ������ ȣ��
    //----------------------------------------------------------

    mtfInitConversionNodeFunc   initConversionNode;
};
// PROJ-2002 Column Security
#define MTC_POLICY_NAME_SIZE  (16-1)

// BUGBUG
// smiColumn�� client porting�� ���� �������Ѵ�.
struct smiCColumn {
    acp_uint32_t      id;
    acp_uint32_t      flag;
    acp_uint32_t      offset;
    acp_uint32_t      size;
    void            * value;
};

// PROJ-2002 Column Security
struct mtcColumnEncryptAttr {
    acp_sint32_t      mEncPrecision;                      // ��ȣ ����Ÿ Ÿ���� precision
    acp_char_t        mPolicy[MTC_POLICY_NAME_SIZE+1];    // ���� ��å�� �̸� (Null Terminated String)
};

// PROJ-2422 srid
struct mtcColumnSRIDAttr {
    acp_sint32_t      mSrid;
};

// column�� �ΰ�����
union mtcColumnAttr {
    struct mtcColumnEncryptAttr  mEncAttr;
    struct mtcColumnSRIDAttr     mSridAttr;
};

struct mtcColumn {
    smiCColumn        column;
    mtcId             type;
    acp_uint32_t      flag;
    acp_sint32_t      precision;
    acp_sint32_t      scale;

    // column�� �ΰ�����
    union mtcColumnAttr  mColumnAttr;
    
    const mtdModule * module;
    const mtlModule * language; // PROJ-1361 : add language module
};

// ������ ������ ���� ���� �Լ� ����
struct mtcExecute {
    mtfCalculateFunc     initialize;    // Aggregation �ʱ�ȭ �Լ�
    mtfCalculateFunc     aggregate;     // Aggregation ���� �Լ�
    mtfCalculateFunc     finalize;      // Aggregation ���� �Լ�
    mtfCalculateFunc     calculate;     // ���� �Լ�
    void*                calculateInfo; // ������ ���� �ΰ� ����, ���� �̻��
    mtfEstimateRangeFunc estimateRange; // Key Range ũ�� ���� �Լ�
    mtfExtractRangeFunc  extractRange;  // Key Range ���� �Լ�
};

// ������ Ÿ��, ���� ���� �̸��� ����
struct mtcName {
    mtcName*         next;   // �ٸ� �̸�
    acp_uint32_t     length; // �̸��� ����, Bytes
    void*            string; // �̸�, UTF-8
};

// PROJ-1473
struct mtcColumnLocate {
    acp_uint16_t     table;
    acp_uint16_t     column;
};

struct mtcNode {
    const mtfModule* module;
    mtcNode*         conversion;
    mtcNode*         leftConversion;
    mtcNode*         funcArguments;   // PROJ-1492 CAST( operand AS target )
    mtcNode*         orgNode;         // PROJ-1404 original const expr node
    mtcNode*         arguments;
    mtcNode*         next;
    acp_uint16_t     table;
    acp_uint16_t     column;
    acp_uint16_t     baseTable;   // PROJ-2002 column�� ��� base table
    acp_uint16_t     baseColumn;  // PROJ-2002 column�� ��� base column
    acp_uint64_t     lflag;
    acp_uint64_t     cost;
    acp_uint32_t     info;
    acp_ulong_t      objectID;    // PROJ-1073 Package
};

#define MTC_NODE_INIT(_node_)                       \
    {                                               \
        (_node_)->module         = NULL;            \
        (_node_)->conversion     = NULL;            \
        (_node_)->leftConversion = NULL;            \
        (_node_)->funcArguments  = NULL;            \
        (_node_)->orgNode        = NULL;            \
        (_node_)->arguments      = NULL;            \
        (_node_)->next           = NULL;            \
        (_node_)->table          = 0;               \
        (_node_)->column         = 0;               \
        (_node_)->baseTable      = 0;               \
        (_node_)->baseColumn     = 0;               \
        (_node_)->lflag          = 0;               \
        (_node_)->cost           = 0;               \
        (_node_)->info           = ACP_UINT32_MAX;  \
        (_node_)->objectID       = 0;               \
    }

//----------------------------------------------------------
// Key Range ������ ���� ����
// - column : Index Column ����, Memory/Disk Index�� ����
// - argument : Index Column�� ��ġ (0: left, 1: right)
// - compValueType : ���ϴ� ������ type
//                   MTD_COMPARE_MTDVAL_MTDVAL
//                   MTD_COMPARE_MTDVAL_STOREDVAL
//                   MTD_COMPARE_STOREDVAL_STOREDVAL
// - direction : MTD_COMPARE_ASCENDING, MTD_COMPARE_DESCENDING
// - isSameGroupType : sm�� callback���� ������ compare �Լ� ����
//                     TRUE : mtd::compareSameGroupAscending
//                            mtd::compareSameGroupDescending
//                     FALSE: index column�� mtdModule.compare
// - columnIdx : STOREDVAL Ÿ���� Column�� ���Ҷ� ���Ǵ�
//               Į�� �ε���
//----------------------------------------------------------
struct mtkRangeInfo
{
    mtcColumn    * column;    
    acp_uint32_t   argument;   
    acp_uint32_t   compValueType; 
    acp_uint32_t   direction;  
    acp_bool_t     isSameGroupType;
    acp_uint32_t   columnIdx;
    acp_uint32_t   useOffset; /* BUG-47690 Inlist ���� Index fatal */
};

struct mtcStack {
    mtcColumn * column;
    void      * value;
};

struct mtcTuple {
    acp_uint32_t      modify;
    acp_uint64_t      lflag;
    acp_uint16_t      columnCount;
    acp_uint16_t      columnMaximum;
    mtcColumn       * columns;
    mtcColumnLocate * columnLocate; // PROJ-1473    
    mtcExecute      * execute;
    acp_uint32_t      rowOffset;
    acp_uint32_t      rowMaximum;
    void*             tableHandle;
    void*             row;
    acp_uint16_t      partitionTupleID;
};

struct mtcTemplate {
    mtcStack*               stackBuffer;
    acp_sint32_t            stackCount;
    mtcStack*               stack;
    acp_sint32_t            stackRemain;
    acp_uint8_t*            data;         // execution������ data����
    acp_uint32_t            dataSize;
    acp_uint8_t*            execInfo;     // ���� ���� �Ǵ��� ���� ����
    acp_uint32_t            execInfoCnt;  // ���� ���� ������ ����
    mtcInitSubqueryFunc     initSubquery;
    mtcFetchSubqueryFunc    fetchSubquery;
    mtcFiniSubqueryFunc     finiSubquery;
    mtcSetCalcSubqueryFunc  setCalcSubquery; // PROJ-2448

    mtcGetOpenedCursorFunc  getOpenedCursor; // PROJ-1362
    mtcIsBaseTable          isBaseTable;     // PROJ-1362
    mtcGetSTObjBufSizeFunc  getSTObjBufSize; // PROJ-1583 large geometry

    // PROJ-2002 Column Security
    mtcEncrypt              encrypt;
    mtcDecrypt              decrypt;
    mtcEncodeECC            encodeECC;
    mtcGetDecryptInfo       getDecryptInfo;
    mtcGetECCSize           getECCSize;

    // PROJ-1358 Internal Tuple�� �������� �����Ѵ�.
    acp_uint16_t                    currentRow[MTC_TUPLE_TYPE_MAXIMUM];
    acp_uint16_t                    rowArrayCount;    // �Ҵ�� internal tuple�� ����
    acp_uint16_t                    rowCount;         // ������� internal tuple�� ����
    acp_uint16_t                    rowCountBeforeBinding;
    acp_uint16_t                    variableRow;
    mtcTuple                      * rows;

    acp_char_t                    * dateFormat;
    acp_bool_t                      dateFormatRef;
    
    acp_sint32_t                    id; //fix PROJ-1596

    // PROJ-1579 NCHAR
    const mtlModule       * nlsUse;
};

struct mtvConvert{
    acp_uint32_t       count;
    mtfCalculateFunc * convert;
    mtcColumn        * columns;
    mtcStack         * stack;
};

struct mtvTable {
    acp_uint64_t       cost;
    acp_uint32_t       count;
    const mtvModule ** modules;
    const mtvModule *  module;
};

struct mtfSubModule {
    mtfSubModule    * next;
    mtfEstimateFunc   estimate;
};

typedef struct mtfNameIndex {
    const mtcName   * name;
    const mtfModule * module;
} mtfNameIndex;

//--------------------------------------------------------
// mtdModule
//    - name       : data type ��
//    - column     : default column ����
//    - id         : data type�� ���� id
//    - no         : data type�� ���� number
//    - idxType    : data type�� ����� �� �ִ� �ε����� ����(�ִ�8��)
//                 : idxType[0] �� Default Index Type
//    - initialize : no, column �ʱ�ȭ
//    - estimate   : mtcColumn�� data type ���� ���� �� semantic �˻�
//--------------------------------------------------------

# define MTD_MAX_USABLE_INDEX_CNT                  (8)

struct mtdModule {
    mtcName*               names;
    mtcColumn*             column;

    acp_uint32_t           id;
    acp_uint32_t           no;
    acp_uint8_t            idxType[MTD_MAX_USABLE_INDEX_CNT];
    acp_uint32_t           align;
    acp_uint32_t           flag;
    acp_uint32_t           columnSize;   // PROJ-1362 column size
    // Ȥ�� max precision
    acp_sint32_t           minScale;     // PROJ-1362
    acp_sint32_t           maxScale;     // PROJ-1362
    void*                  staticNull;
    mtdInitializeFunc      initialize;
    mtdEstimateFunc        estimate;
    mtdValueFunc           value;
    mtdActualSizeFunc      actualSize;
    mtdGetPrecisionFunc    getPrecision;  // PROJ-1877 value�� precision,scale
    mtdNullFunc            null;
    mtdHashFunc            hash;
    mtdIsNullFunc          isNull;
    mtdIsTrueFunc          isTrue;
    mtdCompareFunc         logicalCompare;   // Type�� ���� ��
    mtdCompareFunc         keyCompare[3][2]; // �ε��� Key ��
    mtdCanonizeFunc        canonize;
    mtdEndianFunc          endian;
    mtdValidateFunc        validate;
    mtdSelectivityFunc     selectivity;  // A4 : Selectivity ��� �Լ�
    mtdEncodeFunc          encode;
    mtdDecodeFunc          decode;
    mtdCompileFmtFunc      compileFmt;
    mtdValueFromOracleFunc valueFromOracle;
    mtdMakeColumnInfo      makeColumnInfo;

    //-------------------------------------------------------------------
    // PROJ-1705
    // ��ũ���̺��� ���ڵ� ���屸�� �������� ����
    // ��ũ ���̺� ���ڵ� ��ġ�� 
    // �÷������� qp ���ڵ�ó�������� �ش� �÷���ġ�� ���� 
    //-------------------------------------------------------------------
    mtdStoredValue2MtdValueFunc  storedValue2MtdValue;    

    //-------------------------------------------------------------------
    // PROJ-1705
    // �� ����ŸŸ���� null Value�� ũ�� ��ȯ
    // replication���� ���
    //-------------------------------------------------------------------
    mtdNullValueSizeFunc        nullValueSize;

    //-------------------------------------------------------------------
    // PROJ-1705
    // length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
    // ��) mtdCharType( UShort length; UChar value[1] ) ����
    //      lengthŸ���� UShort�� ũ�⸦ ��ȯ
    // integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
    //-------------------------------------------------------------------
    mtdHeaderSizeFunc           headerSize;
};  

// �������� Ư¡�� ���
struct mtfModule {
    acp_uint64_t             lflag; // �Ҵ��ϴ� Column�� ����, �������� ����
    acp_uint64_t             lmask; // ���� Node�� flag�� �����Ͽ� �ش� ��������
    // Index ��� ���� ���θ� flag�� Setting
    acp_double_t      selectivity;      // �������� default selectivity
    const mtcName*    names;            // �������� �̸� ����
    const mtfModule*  counter;          // �ش� �������� Counter ������
    mtfInitializeFunc initialize;       // ���� ������ �����ڸ� ���� �ʱ�ȭ
    mtfFinalizeFunc   finalize;         // ���� ����� �����ڸ� ���� ���� �Լ�
    mtfEstimateFunc   estimate;         // PROJ-2163 : �������� estimation�� ���� �Լ�
};

// PROJ-1361
//    language�� ���� '��,��,��,��,��,��,����ũ����' ���ڰ� �ٸ���.
//    ���� EXTRACT �Լ� ���� ��,
//    language�� �°� '��,��,��,��,��,��,����ũ����' ��ġ���� �˻�
//    '����, �б�, ��' �߰�
typedef struct mtlExtractFuncSet {
    mtlMatchFunc   matchCentury;
    mtlMatchFunc   matchYear;
    mtlMatchFunc   matchQuarter;
    mtlMatchFunc   matchMonth;
    mtlMatchFunc   matchWeek;
    mtlMatchFunc   matchWeekOfMonth;
    mtlMatchFunc   matchDay;
    mtlMatchFunc   matchDayOfYear;
    mtlMatchFunc   matchDayOfWeek;
    mtlMatchFunc   matchHour;
    mtlMatchFunc   matchMinute;
    mtlMatchFunc   matchSecond;
    mtlMatchFunc   matchMicroSec;
    /* BUG-45730 ROUND, TRUNC �Լ����� DATE ���� IW �߰� ���� */
    mtlMatchFunc   matchISOWeek;
} mtlExtractFuncSet;

// PROJ-1361
// language�� ���� ������ ��Ÿ���� ���ڰ� �ٸ���.
// ���� NEXT_DAY �Լ� ���� ��, language�� �°� ���� ��ġ ���� �˻�
typedef struct mtlNextDayFuncSet {
    mtlMatchFunc   matchSunDay;
    mtlMatchFunc   matchMonDay;
    mtlMatchFunc   matchTueDay;
    mtlMatchFunc   matchWedDay;
    mtlMatchFunc   matchThuDay;
    mtlMatchFunc   matchFriDay;
    mtlMatchFunc   matchSatDay;
} mtlNextDayFuncSet;

struct mtlModule {
    const mtcName*        names;
    acp_uint32_t          id;
    
    mtlNextCharFunc       nextCharPtr;    // Language�� �°� ���� ���ڷ� �̵�
    mtlMaxPrecisionFunc   maxPrecision;   // Language�� �ִ� precision���
    mtlExtractFuncSet   * extractSet;     // Language�� �´� Date Type �̸�
    mtlNextDayFuncSet   * nextDaySet;     // Language�� �´� ���� �̸�
    
    acp_uint8_t        ** specialCharSet;
    acp_uint8_t           specialCharSize;
};

struct mtvModule {
    const mtdModule* to;
    const mtdModule* from;
    acp_uint64_t            cost;
    mtfEstimateFunc  estimate;
};

// PROJ-1362
// ODBC API, SQLGetTypeInfo ������ ���� mtdModule Ȯ��
// �������� Ÿ�������� client�� �ϵ��ڵ��Ǿ� �ִ�����
// �������� ��ȸ�ϵ��� ������
#define MTD_TYPE_NAME_LEN            40
#define MTD_TYPE_LITERAL_LEN         4
#define MTD_TYPE_CREATE_PARAM_LEN    20

// BUG-17202
struct mtdType
{
    acp_char_t    * mTypeName;
    acp_sint16_t    mDataType;
    acp_sint16_t    mODBCDataType; // mDataType�� ODBC SPEC���� ����
    acp_sint32_t    mColumnSize;
    acp_char_t    * mLiteralPrefix;
    acp_char_t    * mLiteralSuffix;
    acp_char_t    * mCreateParam;
    acp_sint16_t    mNullable;
    acp_sint16_t    mCaseSens;
    acp_sint16_t    mSearchable;
    acp_sint16_t    mUnsignedAttr;
    acp_sint16_t    mFixedPrecScale;
    acp_sint16_t    mAutoUniqueValue;
    acp_char_t    * mLocalTypeName;
    acp_sint16_t    mMinScale;
    acp_sint16_t    mMaxScale;
    acp_sint16_t    mSQLDataType;
    acp_sint16_t    mODBCSQLDataType; // mSQLDataType�� ODBC SPEC���� ����
    acp_sint16_t    mSQLDateTimeSub;
    acp_sint32_t    mNumPrecRadix;
    acp_sint16_t    mIntervalPrecision;
};

// PROJ-1755
typedef enum
{                                // format string�� ���Ͽ�
    MTC_FORMAT_NULL = 1,         // null Ȥ�� ''�� ����
    MTC_FORMAT_NORMAL,           // �Ϲݹ��ڷθ� ����
    MTC_FORMAT_UNDER,            // '_'�θ� ����
    MTC_FORMAT_PERCENT,          // '%'�θ� ����
    MTC_FORMAT_NORMAL_UNDER,     // �Ϲݹ��ڿ� '_'�� ����
    MTC_FORMAT_NORMAL_PERCENT,   // �Ϲݹ��ڿ� '%'�� ����
    MTC_FORMAT_UNDER_PERCENT,    // '_'�� '%'�� ����
    MTC_FORMAT_ALL               // �Ϲݹ��ڿ� '_', '%'�� ����
} mtcLikeFormatType;

// PROJ-1755
struct mtcLikeFormatInfo
{
    // escape���ڸ� ������ format pattern
    acp_uint8_t             * pattern;
    acp_uint16_t              patternSize;

    // format pattern�� �з�����
    mtcLikeFormatType   type;
    acp_uint16_t              underCnt;
    acp_uint16_t              percentCnt;
    acp_uint16_t              charCnt;

    // '%'�� 1���� ����� �ΰ�����
    // [head]%[tail]
    acp_uint8_t             * firstPercent;
    acp_uint8_t             * head;     // pattern������ head ��ġ
    acp_uint16_t              headSize;
    acp_uint8_t             * tail;     // pattern������ tail ��ġ
    acp_uint16_t              tailSize;
};

// PROJ-1579 NCHAR
typedef enum
{
    MTC_COLUMN_NCHAR_LITERAL = 1,
    MTC_COLUMN_UNICODE_LITERAL,
    MTC_COLUMN_NORMAL_LITERAL
} mtcLiteralType;

//-------------------------------------------------------------------
// PROJ-1872
// compare �Ҷ� �ʿ��� ����
// - mtd type ���� ���Ҷ�
//   column, value, flag ���� �̿��� 
//   value�� column.offset ��ġ���� �ش� column�� value�� �о�� 
// - stored type���� ���Ҷ�
//   column, value, length ���� �̿���
//   value�� length ��ŭ �о�� 
//-------------------------------------------------------------------



#endif /* _O_MTC_DEF_H_ */
