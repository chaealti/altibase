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
 * $Id: mtcDef.h 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#ifndef _O_MTC_DEF_H_
# define _O_MTC_DEF_H_ 1

# include <smiDef.h>
# include <iduMemory.h>
# include <iduVarMemList.h>
# include <mtlTerritory.h>

/* PROJ-2429 Dictionary based data compress for on-disk DB */
# define MTD_COLUMN_COPY_FUNC_NORMAL                (0)
# define MTD_COLUMN_COPY_FUNC_COMPRESSION           (1)
# define MTD_COLUMN_COPY_FUNC_MAX_COUNT             (2)

/* Partial Key                                       */
# define MTD_PARTIAL_KEY_MINIMUM                    (0)
# define MTD_PARTIAL_KEY_MAXIMUM         (ID_ULONG_MAX)

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

// PROJ-2204 Join Update, Delete
/* mtcColumn.flag                                    */
# define MTC_COLUMN_KEY_PRESERVED_MASK     (0x00200000)
# define MTC_COLUMN_KEY_PRESERVED_TRUE     (0x00200000)
# define MTC_COLUMN_KEY_PRESERVED_FALSE    (0x00000000)

// PROJ-1784 DML Without Retry
/* mtcColumn.flag                                    */
# define MTC_COLUMN_USE_WHERE_MASK         (0x00400000)
# define MTC_COLUMN_USE_WHERE_TRUE         (0x00400000)
# define MTC_COLUMN_USE_WHERE_FALSE        (0x00000000)

// PROJ-1784 DML Without Retry
/* mtcColumn.flag                                    */
# define MTC_COLUMN_USE_SET_MASK           (0x00800000)
# define MTC_COLUMN_USE_SET_TRUE           (0x00800000)
# define MTC_COLUMN_USE_SET_FALSE          (0x00000000)

/* PROJ-2586 PSM Parameters and return without precision
   mtcColumn.flag
   PSM ��ü ���� �� parameter �� return�� precision�� default ������ ����� column���� �ǹ� */
# define MTC_COLUMN_SP_SET_PRECISION_MASK      (0x01000000)
# define MTC_COLUMN_SP_SET_PRECISION_TRUE      (0x01000000)
# define MTC_COLUMN_SP_SET_PRECISION_FALSE     (0x00000000)

/* PROJ-2586 PSM Parameters and return without precision
   mtcColumn.flag
   PSM ���� �� parameter �� return�� precision�� argument �� result value�� ���� column ������ ������� �ǹ� */
# define MTC_COLUMN_SP_ADJUST_PRECISION_MASK      (0x02000000)
# define MTC_COLUMN_SP_ADJUST_PRECISION_TRUE      (0x02000000)
# define MTC_COLUMN_SP_ADJUST_PRECISION_FALSE     (0x00000000)

/* TASK-7219 */
# define MTC_COLUMN_NULL_TYPE_MASK         (0x04000000)
# define MTC_COLUMN_NULL_TYPE_TRUE         (0x04000000)
# define MTC_COLUMN_NULL_TYPE_FALSE        (0x00000000)

//====================================================================
// mtcNode.flag ����
//====================================================================

// BUG-34127 Increase maximum arguement count(4095(0x0FFF) -> 12287(0x2FFF))
/* mtcNode.flag                                      */
# define MTC_NODE_ARGUMENT_COUNT_MASK           ID_ULONG(0x000000000000FFFF)
# define MTC_NODE_ARGUMENT_COUNT_MAXIMUM                             (12287)

/* mtfModule.flag                                    */
// �����ڰ� �����ϴ� Column�� ������ AVG()�� ���� ������
// SUM()�� COUNT()�������� �����ϱ� ���� ��������
// Column �� �����ؾ� �Ѵ�.
# define MTC_NODE_COLUMN_COUNT_MASK             ID_ULONG(0x00000000000000FF)

/* mtcNode.flag                                      */
# define MTC_NODE_MASK        ( MTC_NODE_INDEX_MASK | \
                                MTC_NODE_BIND_MASK  | \
                                MTC_NODE_DML_MASK   | \
                                MTC_NODE_EAT_NULL_MASK | \
                                MTC_NODE_FUNCTION_CONNECT_BY_MASK | \
                                MTC_NODE_HAVE_SUBQUERY_MASK )

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_OPERATOR_MASK                 ID_ULONG(0x00000000000F0000)
# define MTC_NODE_OPERATOR_MISC                 ID_ULONG(0x0000000000000000)
# define MTC_NODE_OPERATOR_AND                  ID_ULONG(0x0000000000010000)
# define MTC_NODE_OPERATOR_OR                   ID_ULONG(0x0000000000020000)
# define MTC_NODE_OPERATOR_NOT                  ID_ULONG(0x0000000000030000)
# define MTC_NODE_OPERATOR_EQUAL                ID_ULONG(0x0000000000040000)
# define MTC_NODE_OPERATOR_NOT_EQUAL            ID_ULONG(0x0000000000050000)
# define MTC_NODE_OPERATOR_GREATER              ID_ULONG(0x0000000000060000)
# define MTC_NODE_OPERATOR_GREATER_EQUAL        ID_ULONG(0x0000000000070000)
# define MTC_NODE_OPERATOR_LESS                 ID_ULONG(0x0000000000080000)
# define MTC_NODE_OPERATOR_LESS_EQUAL           ID_ULONG(0x0000000000090000)
# define MTC_NODE_OPERATOR_RANGED               ID_ULONG(0x00000000000A0000)
# define MTC_NODE_OPERATOR_NOT_RANGED           ID_ULONG(0x00000000000B0000)
# define MTC_NODE_OPERATOR_LIST                 ID_ULONG(0x00000000000C0000)
# define MTC_NODE_OPERATOR_FUNCTION             ID_ULONG(0x00000000000D0000)
# define MTC_NODE_OPERATOR_AGGREGATION          ID_ULONG(0x00000000000E0000)
# define MTC_NODE_OPERATOR_SUBQUERY             ID_ULONG(0x00000000000F0000)

/* mtcNode.flag                                      */
# define MTC_NODE_INDIRECT_MASK                 ID_ULONG(0x0000000000100000)
# define MTC_NODE_INDIRECT_TRUE                 ID_ULONG(0x0000000000100000)
# define MTC_NODE_INDIRECT_FALSE                ID_ULONG(0x0000000000000000)
                                            
/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_COMPARISON_MASK               ID_ULONG(0x0000000000200000)
# define MTC_NODE_COMPARISON_TRUE               ID_ULONG(0x0000000000200000)
# define MTC_NODE_COMPARISON_FALSE              ID_ULONG(0x0000000000000000)

/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_LOGICAL_CONDITION_MASK        ID_ULONG(0x0000000000400000)
# define MTC_NODE_LOGICAL_CONDITION_TRUE        ID_ULONG(0x0000000000400000)
# define MTC_NODE_LOGICAL_CONDITION_FALSE       ID_ULONG(0x0000000000000000)

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
# define MTC_NODE_INDEX_MASK                    ID_ULONG(0x0000000000800000)
# define MTC_NODE_INDEX_USABLE                  ID_ULONG(0x0000000000800000)
# define MTC_NODE_INDEX_UNUSABLE                ID_ULONG(0x0000000000000000)

/* mtcNode.flag                                      */
// ����� ���� ��忡 ȣ��Ʈ ������ �������� ��Ÿ����.
# define MTC_NODE_BIND_MASK                     ID_ULONG(0x0000000001000000)
# define MTC_NODE_BIND_EXIST                    ID_ULONG(0x0000000001000000)
# define MTC_NODE_BIND_ABSENT                   ID_ULONG(0x0000000000000000)
                                            
/* mtcNode.flag                                      */
# define MTC_NODE_QUANTIFIER_MASK               ID_ULONG(0x0000000002000000)
# define MTC_NODE_QUANTIFIER_TRUE               ID_ULONG(0x0000000002000000)
# define MTC_NODE_QUANTIFIER_FALSE              ID_ULONG(0x0000000000000000)
                                            
/* mtcNode.flag                                      */
# define MTC_NODE_DISTINCT_MASK                 ID_ULONG(0x0000000004000000)
# define MTC_NODE_DISTINCT_TRUE                 ID_ULONG(0x0000000004000000)
# define MTC_NODE_DISTINCT_FALSE                ID_ULONG(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_GROUP_COMPARISON_MASK         ID_ULONG(0x0000000008000000)
# define MTC_NODE_GROUP_COMPARISON_TRUE         ID_ULONG(0x0000000008000000)
# define MTC_NODE_GROUP_COMPARISON_FALSE        ID_ULONG(0x0000000000000000)

/* mtcNode.flag                                      */
# define MTC_NODE_GROUP_MASK                    ID_ULONG(0x0000000010000000)
# define MTC_NODE_GROUP_ANY                     ID_ULONG(0x0000000010000000)
# define MTC_NODE_GROUP_ALL                     ID_ULONG(0x0000000000000000)
                                            
/* mtcNode.flag                                      */
# define MTC_NODE_FILTER_MASK                   ID_ULONG(0x0000000020000000)
# define MTC_NODE_FILTER_NEED                   ID_ULONG(0x0000000020000000)
# define MTC_NODE_FILTER_NEEDLESS               ID_ULONG(0x0000000000000000)
                                            
/* mtcNode.flag                                      */
# define MTC_NODE_DML_MASK                      ID_ULONG(0x0000000040000000)
# define MTC_NODE_DML_UNUSABLE                  ID_ULONG(0x0000000040000000)
# define MTC_NODE_DML_USABLE                    ID_ULONG(0x0000000000000000)

/* mtcNode.flag                                      */
// fix BUG-13830
# define MTC_NODE_REESTIMATE_MASK               ID_ULONG(0x0000000080000000)
# define MTC_NODE_REESTIMATE_TRUE               ID_ULONG(0x0000000080000000)
# define MTC_NODE_REESTIMATE_FALSE              ID_ULONG(0x0000000000000000)

/* mtfModule.flag                                    */
// To Fix PR-15434 INDEX JOIN�� ������ ������������ ����
# define MTC_NODE_INDEX_JOINABLE_MASK           ID_ULONG(0x0000000100000000)
# define MTC_NODE_INDEX_JOINABLE_TRUE           ID_ULONG(0x0000000100000000)
# define MTC_NODE_INDEX_JOINABLE_FALSE          ID_ULONG(0x0000000000000000)
// To Fix PR-15291                          
# define MTC_NODE_INDEX_ARGUMENT_MASK           ID_ULONG(0x0000000200000000)
# define MTC_NODE_INDEX_ARGUMENT_BOTH           ID_ULONG(0x0000000200000000)
# define MTC_NODE_INDEX_ARGUMENT_LEFT           ID_ULONG(0x0000000000000000)

// BUG-15995
/* mtfModule.flag                                    */
# define MTC_NODE_VARIABLE_MASK                 ID_ULONG(0x0000000400000000)
# define MTC_NODE_VARIABLE_TRUE                 ID_ULONG(0x0000000400000000)
# define MTC_NODE_VARIABLE_FALSE                ID_ULONG(0x0000000000000000)

// PROJ-1492
// �������� Ÿ���� validation�ÿ� �����Ǿ����� ��Ÿ����.
/* mtcNode.flag                                      */
# define MTC_NODE_BIND_TYPE_MASK                ID_ULONG(0x0000000800000000)
# define MTC_NODE_BIND_TYPE_TRUE                ID_ULONG(0x0000000800000000)
# define MTC_NODE_BIND_TYPE_FALSE               ID_ULONG(0x0000000000000000)

/* BUG-44762 case when subquery */
# define MTC_NODE_HAVE_SUBQUERY_MASK            ID_ULONG(0x0000001000000000)
# define MTC_NODE_HAVE_SUBQUERY_TRUE            ID_ULONG(0x0000001000000000)
# define MTC_NODE_HAVE_SUBQUERY_FALSE           ID_ULONG(0x0000000000000000)

//fix BUG-17610
# define MTC_NODE_EQUI_VALID_SKIP_MASK          ID_ULONG(0x0000002000000000)
# define MTC_NODE_EQUI_VALID_SKIP_TRUE          ID_ULONG(0x0000002000000000)
# define MTC_NODE_EQUI_VALID_SKIP_FALSE         ID_ULONG(0x0000000000000000)

// fix BUG-22512 Outer Join �迭 Push Down Predicate �� ���, ��� ����
// CASE2, DECODE, DIGEST, DUMP, IS NULL, IS NOT NULL, NVL, NVL2, UserDefined Function �Լ���
// ���ڷ� ���� NULL���� �ٸ� ������ �ٲ� �� �ִ�.
// CASE2, DECODE, DIGEST, DUMP, IS NULL, IS NOT NULL, NVL, NVL2, UserDefined Function �Լ��� 
// outer join ���� ���� push down �� �� ���� function���� ǥ��.
# define MTC_NODE_EAT_NULL_MASK                 ID_ULONG(0x0000004000000000)
# define MTC_NODE_EAT_NULL_TRUE                 ID_ULONG(0x0000004000000000)
# define MTC_NODE_EAT_NULL_FALSE                ID_ULONG(0x0000000000000000)

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
# define MTC_NODE_PRINT_FMT_MASK                ID_ULONG(0x00000F0000000000)
# define MTC_NODE_PRINT_FMT_PREFIX_PA           ID_ULONG(0x0000000000000000)
# define MTC_NODE_PRINT_FMT_PREFIX_SP           ID_ULONG(0x0000010000000000)
# define MTC_NODE_PRINT_FMT_INFIX               ID_ULONG(0x0000020000000000)
# define MTC_NODE_PRINT_FMT_INFIX_SP            ID_ULONG(0x0000030000000000)
# define MTC_NODE_PRINT_FMT_POSTFIX_SP          ID_ULONG(0x0000040000000000)
# define MTC_NODE_PRINT_FMT_MISC                ID_ULONG(0x0000050000000000)

// PROJ-1473
# define MTC_NODE_COLUMN_LOCATE_CHANGE_MASK     ID_ULONG(0x0000100000000000)
# define MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE    ID_ULONG(0x0000100000000000)
# define MTC_NODE_COLUMN_LOCATE_CHANGE_TRUE     ID_ULONG(0x0000000000000000)

// PROJ-1473
# define MTC_NODE_REMOVE_ARGUMENTS_MASK         ID_ULONG(0x0000200000000000)
# define MTC_NODE_REMOVE_ARGUMENTS_TRUE         ID_ULONG(0x0000200000000000)
# define MTC_NODE_REMOVE_ARGUMENTS_FALSE        ID_ULONG(0x0000000000000000)

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
// qtc::estimateInternal()�Լ��� �ּ� ����.
# define MTC_NODE_CASE_TYPE_MASK                ID_ULONG(0x0000400000000000) 
# define MTC_NODE_CASE_TYPE_SIMPLE              ID_ULONG(0x0000400000000000) 
# define MTC_NODE_CASE_TYPE_SEARCHED            ID_ULONG(0x0000000000000000)

// BUG-38133
// BUG-28223 CASE expr WHEN .. THEN .. ������ expr�� subquery ���� �����߻� 
// qtc::estimateInternal()�Լ��� �ּ� ����.
#define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_MASK  ID_ULONG(0x0000800000000000)
#define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_TRUE  ID_ULONG(0x0000800000000000)
#define MTC_NODE_CASE_EXPRESSION_MAKE_PASSNODE_FALSE ID_ULONG(0x0000000000000000)

// BUG-28446 [valgrind], BUG-38133
// qtcCalculate_Pass(mtcNode*, mtcStack*, int, void*, mtcTemplate*)
#define MTC_NODE_CASE_EXPRESSION_PASSNODE_MASK  ID_ULONG(0x0001000000000000)
#define MTC_NODE_CASE_EXPRESSION_PASSNODE_TRUE  ID_ULONG(0x0001000000000000)
#define MTC_NODE_CASE_EXPRESSION_PASSNODE_FALSE ID_ULONG(0x0000000000000000)

// BUG-31777 ������ ���� �����ڿ� ���ؼ� ��ȯ��Ģ�� ����Ѵ�.
/* mtcNode.flag, mtfModule.flag */
#define MTC_NODE_COMMUTATIVE_MASK               ID_ULONG(0x0002000000000000)
#define MTC_NODE_COMMUTATIVE_TRUE               ID_ULONG(0x0002000000000000)
#define MTC_NODE_COMMUTATIVE_FALSE              ID_ULONG(0x0000000000000000)

// BUG-33663 Ranking Function
/* mtcNode.flag, mtfModule.flag */
#define MTC_NODE_FUNCTION_RANKING_MASK          ID_ULONG(0x0004000000000000)
#define MTC_NODE_FUNCTION_RANKING_TRUE          ID_ULONG(0x0004000000000000)
#define MTC_NODE_FUNCTION_RANKING_FALSE         ID_ULONG(0x0000000000000000)

/* PROJ-1353 Rollup, Cube */
#define MTC_NODE_FUNCTON_GROUPING_MASK          ID_ULONG(0x0008000000000000)
#define MTC_NODE_FUNCTON_GROUPING_TRUE          ID_ULONG(0x0008000000000000)
#define MTC_NODE_FUNCTON_GROUPING_FALSE         ID_ULONG(0x0000000000000000)

/* BUG-35252 , BUG-39611*/
#define MTC_NODE_FUNCTION_CONNECT_BY_MASK      ID_ULONG(0x0010000000000000)
#define MTC_NODE_FUNCTION_CONNECT_BY_TRUE      ID_ULONG(0x0010000000000000)
#define MTC_NODE_FUNCTION_CONNECT_BY_FALSE     ID_ULONG(0x0000000000000000)

// BUG-35155 Partial CNF
// PARTIAL_NORMALIZE_CNF_TRUE  : use predicate when make CNF
// PARTIAL_NORMALIZE_CNF_FALSE : don't use predicate when make CNF
// PARTIAL_NORMALIZE_DNF_TRUE  : use predicate when make DNF (�̱���)
// PARTIAL_NORMALIZE_DNF_FALSE : don't use predicate when make DNF
/* mtcNode.flag, mtfModule.flag                      */
# define MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK     ID_ULONG(0x0020000000000000)
# define MTC_NODE_PARTIAL_NORMALIZE_CNF_USABLE   ID_ULONG(0x0000000000000000)
# define MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE ID_ULONG(0x0020000000000000)

# define MTC_NODE_PARTIAL_NORMALIZE_DNF_MASK     ID_ULONG(0x0040000000000000)
# define MTC_NODE_PARTIAL_NORMALIZE_DNF_USABLE   ID_ULONG(0x0000000000000000)
# define MTC_NODE_PARTIAL_NORMALIZE_DNF_UNUSABLE ID_ULONG(0x0040000000000000)

/* PROJ-1805 Window Clause */
# define MTC_NODE_FUNCTION_WINDOWING_MASK        ID_ULONG(0x0080000000000000)
# define MTC_NODE_FUNCTION_WINDOWING_TRUE        ID_ULONG(0x0080000000000000)
# define MTC_NODE_FUNCTION_WINDOWING_FALSE       ID_ULONG(0x0000000000000000)

/* PROJ-1805 Window Clause */
# define MTC_NODE_FUNCTION_ANALYTIC_MASK         ID_ULONG(0x0100000000000000)
# define MTC_NODE_FUNCTION_ANALYTIC_TRUE         ID_ULONG(0x0100000000000000)
# define MTC_NODE_FUNCTION_ANALYTIC_FALSE        ID_ULONG(0x0000000000000000)

// PROJ-2527 WITHIN GROUP AGGR
# define MTC_NODE_WITHIN_GROUP_ORDER_MASK        ID_ULONG(0x0200000000000000)
# define MTC_NODE_WITHIN_GROUP_ORDER_DESC        ID_ULONG(0x0200000000000000)
# define MTC_NODE_WITHIN_GROUP_ORDER_ASC         ID_ULONG(0x0000000000000000)

// BUG-41631 NULLS FIRST/LAST
# define MTC_NODE_WITHIN_GROUP_NULLS_MASK        ID_ULONG(0x0400000000000000)
# define MTC_NODE_WITHIN_GROUP_NULLS_FIRST       ID_ULONG(0x0400000000000000)
# define MTC_NODE_WITHIN_GROUP_NULLS_LAST        ID_ULONG(0x0000000000000000)

// BUG-43858 ���ڰ� Undef Ÿ���� ��� aNode�� ǥ���Ѵ�.
# define MTC_NODE_UNDEF_TYPE_MASK                ID_ULONG(0x0800000000000000)
# define MTC_NODE_UNDEF_TYPE_EXIST               ID_ULONG(0x0800000000000000)
# define MTC_NODE_UNDEF_TYPE_ABSENT              ID_ULONG(0x0000000000000000)

// PROJ-2528 Keep Aggregation
# define MTC_NODE_NULLS_OPT_EXIST_MASK           ID_ULONG(0x1000000000000000)
# define MTC_NODE_NULLS_OPT_EXIST_TRUE           ID_ULONG(0x1000000000000000)
# define MTC_NODE_NULLS_OPT_EXIST_FALSE          ID_ULONG(0x0000000000000000)

// TASK-7219 Non-shard DML
# define MTC_NODE_PUSHED_PRED_FORCE_MASK         ID_ULONG(0x2000000000000000)
# define MTC_NODE_PUSHED_PRED_FORCE_TRUE         ID_ULONG(0x2000000000000000)
# define MTC_NODE_PUSHED_PRED_FORCE_FALSE        ID_ULONG(0x0000000000000000)

// PROJ-1492
# define MTC_NODE_IS_DEFINED_TYPE( aNode )                     \
    (                                                          \
       (  ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )            \
            == MTC_NODE_BIND_ABSENT )                          \
          ||                                                   \
          ( ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )          \
              == MTC_NODE_BIND_EXIST )                         \
            &&                                                 \
            ( ( (aNode)->lflag & MTC_NODE_BIND_TYPE_MASK )     \
              == MTC_NODE_BIND_TYPE_TRUE ) )                   \
       )                                                       \
       ? ID_TRUE : ID_FALSE                                    \
    )

# define MTC_NODE_IS_DEFINED_VALUE( aNode )                    \
    (                                                          \
       ( ( (aNode)->lflag & MTC_NODE_BIND_MASK )               \
            == MTC_NODE_BIND_ABSENT )                          \
       ? ID_TRUE : ID_FALSE                                    \
    )

/* mtfExtractRange Von likeExtractRange              */
# define MTC_LIKE_KEY_SIZE      ( idlOS::align(ID_SIZEOF(UShort) + 32, 8) )
# define MTC_LIKE_KEY_PRECISION ( MTC_LIKE_KEY_SIZE - ID_SIZEOF(UShort) )

/* mtvModule.cost                                    */
# define MTV_COST_INFINITE                 ID_ULONG(0x4000000000000000)
# define MTV_COST_GROUP_PENALTY            ID_ULONG(0x0001000000000000)
# define MTV_COST_ERROR_PENALTY            ID_ULONG(0x0000000100000000)
# define MTV_COST_LOSS_PENALTY             ID_ULONG(0x0000000000010000)
# define MTV_COST_DEFAULT                  ID_ULONG(0x0000000000000001)

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


// PROJ-1358
// mtcTemplate.rows ��
// 16������ �����Ͽ� 65536 ������ �������� �����Ѵ�.
#define MTC_TUPLE_ROW_INIT_CNT                     (16)
#define MTC_TUPLE_ROW_MAX_CNT           (ID_USHORT_MAX)   // BUG-17154

//====================================================================
// mtcTuple.flag ����
//====================================================================

/* mtcTuple.flag                                     */
# define MTC_TUPLE_TYPE_MAXIMUM                     (4)
/* BUG-44490 mtcTuple flag Ȯ�� �ؾ� �մϴ�. */
/* 32bit flag ���� ��� �Ҹ��� 64bit�� ���� */
/* type�� UInt���� ULong���� ���� */
# define MTC_TUPLE_TYPE_MASK               ID_ULONG(0x0000000000000003)
# define MTC_TUPLE_TYPE_CONSTANT           ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_TYPE_VARIABLE           ID_ULONG(0x0000000000000001)
# define MTC_TUPLE_TYPE_INTERMEDIATE       ID_ULONG(0x0000000000000002)
# define MTC_TUPLE_TYPE_TABLE              ID_ULONG(0x0000000000000003)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_SET_MASK         ID_ULONG(0x0000000000000010)
# define MTC_TUPLE_COLUMN_SET_TRUE         ID_ULONG(0x0000000000000010)
# define MTC_TUPLE_COLUMN_SET_FALSE        ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_ALLOCATE_MASK    ID_ULONG(0x0000000000000020)
# define MTC_TUPLE_COLUMN_ALLOCATE_TRUE    ID_ULONG(0x0000000000000020)
# define MTC_TUPLE_COLUMN_ALLOCATE_FALSE   ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_COLUMN_COPY_MASK        ID_ULONG(0x0000000000000040)
# define MTC_TUPLE_COLUMN_COPY_TRUE        ID_ULONG(0x0000000000000040)
# define MTC_TUPLE_COLUMN_COPY_FALSE       ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_SET_MASK        ID_ULONG(0x0000000000000080)
# define MTC_TUPLE_EXECUTE_SET_TRUE        ID_ULONG(0x0000000000000080)
# define MTC_TUPLE_EXECUTE_SET_FALSE       ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_ALLOCATE_MASK   ID_ULONG(0x0000000000000100)
# define MTC_TUPLE_EXECUTE_ALLOCATE_TRUE   ID_ULONG(0x0000000000000100)
# define MTC_TUPLE_EXECUTE_ALLOCATE_FALSE  ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_EXECUTE_COPY_MASK       ID_ULONG(0x0000000000000200)
# define MTC_TUPLE_EXECUTE_COPY_TRUE       ID_ULONG(0x0000000000000200)
# define MTC_TUPLE_EXECUTE_COPY_FALSE      ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_SET_MASK            ID_ULONG(0x0000000000000400)
# define MTC_TUPLE_ROW_SET_TRUE            ID_ULONG(0x0000000000000400)
# define MTC_TUPLE_ROW_SET_FALSE           ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_ALLOCATE_MASK       ID_ULONG(0x0000000000000800)
# define MTC_TUPLE_ROW_ALLOCATE_TRUE       ID_ULONG(0x0000000000000800)
# define MTC_TUPLE_ROW_ALLOCATE_FALSE      ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_COPY_MASK           ID_ULONG(0x0000000000001000)
# define MTC_TUPLE_ROW_COPY_TRUE           ID_ULONG(0x0000000000001000)
# define MTC_TUPLE_ROW_COPY_FALSE          ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_SKIP_MASK           ID_ULONG(0x0000000000002000)
# define MTC_TUPLE_ROW_SKIP_TRUE           ID_ULONG(0x0000000000002000)
# define MTC_TUPLE_ROW_SKIP_FALSE          ID_ULONG(0x0000000000000000)

/* PROJ-2160 row �߿� GEOMETRY Ÿ���� �ִ��� üũ */
/* mtcTuple.flag                                     */
# define MTC_TUPLE_ROW_GEOMETRY_MASK       ID_ULONG(0x0000000000004000)
# define MTC_TUPLE_ROW_GEOMETRY_TRUE       ID_ULONG(0x0000000000004000)
# define MTC_TUPLE_ROW_GEOMETRY_FALSE      ID_ULONG(0x0000000000000000)

/* BUG-43705 lateral view�� simple view merging�� ���������� ����� �ٸ��ϴ�. */
/* mtcTuple.flag */
# define MTC_TUPLE_LATERAL_VIEW_REF_MASK   ID_ULONG(0x0000000000008000)
# define MTC_TUPLE_LATERAL_VIEW_REF_TRUE   ID_ULONG(0x0000000000008000)
# define MTC_TUPLE_LATERAL_VIEW_REF_FALSE  ID_ULONG(0x0000000000000000)

/* mtcTuple.flag                                     */
// Tuple�� ���� ��ü�� ���� ����
# define MTC_TUPLE_STORAGE_MASK            ID_ULONG(0x0000000000010000)
# define MTC_TUPLE_STORAGE_MEMORY          ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_STORAGE_DISK            ID_ULONG(0x0000000000010000)

/* mtcTuple.flag                                     */
// �ش� Tuple�� VIEW�� ���� Tuple������ ����
# define MTC_TUPLE_VIEW_MASK               ID_ULONG(0x0000000000020000)
# define MTC_TUPLE_VIEW_FALSE              ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_VIEW_TRUE               ID_ULONG(0x0000000000020000)

/* mtcTuple.flag */
// �ش� Tuple�� Plan�� ���� ������ Tuple������ ����
# define MTC_TUPLE_PLAN_MASK               ID_ULONG(0x0000000000040000)
# define MTC_TUPLE_PLAN_FALSE              ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_PLAN_TRUE               ID_ULONG(0x0000000000040000)

/* mtcTuple.flag */
// �ش� Tuple�� Materialization Plan�� ���� ������ Tuple
# define MTC_TUPLE_PLAN_MTR_MASK           ID_ULONG(0x0000000000080000)
# define MTC_TUPLE_PLAN_MTR_FALSE          ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_PLAN_MTR_TRUE           ID_ULONG(0x0000000000080000)

/* mtcTuple.flag */
// PROJ-1502 PARTITIONED DISK TABLE
// �ش� Tuple�� Partitioned Table�� ���� ������ Tuple
# define MTC_TUPLE_PARTITIONED_TABLE_MASK  ID_ULONG(0x0000000000100000)
# define MTC_TUPLE_PARTITIONED_TABLE_FALSE ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_PARTITIONED_TABLE_TRUE  ID_ULONG(0x0000000000100000)

/* mtcTuple.flag */
// PROJ-1502 PARTITIONED DISK TABLE
// �ش� Tuple�� Partition�� ���� ������ Tuple
# define MTC_TUPLE_PARTITION_MASK          ID_ULONG(0x0000000000200000)
# define MTC_TUPLE_PARTITION_FALSE         ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_PARTITION_TRUE          ID_ULONG(0x0000000000200000)

/* mtcTuple.flag */
/* BUG-36468 Tuple�� �����ü ��ġ(Location)�� ���� ���� */
# define MTC_TUPLE_STORAGE_LOCATION_MASK   ID_ULONG(0x0000000000400000)
# define MTC_TUPLE_STORAGE_LOCATION_LOCAL  ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_STORAGE_LOCATION_REMOTE ID_ULONG(0x0000000000400000)

/* mtcTuple.flag */
/* PROJ-2464 hybrid partitioned table ���� */
// �ش� Tuple�� Hybrid�� ���� ������ Tuple
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK    ID_ULONG(0x0000000000800000)
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_FALSE   ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE    ID_ULONG(0x0000000000800000)

/* mtcTuple.flag */
// PROJ-1473
// tuple �� ���� ó����� ����.
// (1) tuple set ������� ó��
// (2) ���ڵ����������� ó��
# define MTC_TUPLE_MATERIALIZE_MASK        ID_ULONG(0x0000000001000000)
# define MTC_TUPLE_MATERIALIZE_RID         ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_MATERIALIZE_VALUE       ID_ULONG(0x0000000001000000)

# define MTC_TUPLE_BINARY_COLUMN_MASK      ID_ULONG(0x0000000002000000)
# define MTC_TUPLE_BINARY_COLUMN_ABSENT    ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_BINARY_COLUMN_EXIST     ID_ULONG(0x0000000002000000)

# define MTC_TUPLE_VIEW_PUSH_PROJ_MASK     ID_ULONG(0x0000000004000000)
# define MTC_TUPLE_VIEW_PUSH_PROJ_FALSE    ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_VIEW_PUSH_PROJ_TRUE     ID_ULONG(0x0000000004000000)

// PROJ-1705
/* mtcTuple.flag */
# define MTC_TUPLE_VSCN_MASK               ID_ULONG(0x0000000008000000)
# define MTC_TUPLE_VSCN_FALSE              ID_ULONG(0x0000000000000000)
# define MTC_TUPLE_VSCN_TRUE               ID_ULONG(0x0000000008000000)

/*
 * PROJ-1789 PROWID
 * Target list �Ǵ� where ���� RID �� �ִ���
 */
#define MTC_TUPLE_TARGET_RID_MASK          ID_ULONG(0x0000000010000000)
#define MTC_TUPLE_TARGET_RID_EXIST         ID_ULONG(0x0000000010000000)
#define MTC_TUPLE_TARGET_RID_ABSENT        ID_ULONG(0x0000000000000000)

// PROJ-2204 Join Update Delete
/* mtcTuple.flag */
#define MTC_TUPLE_KEY_PRESERVED_MASK       ID_ULONG(0x0000000020000000)
#define MTC_TUPLE_KEY_PRESERVED_TRUE       ID_ULONG(0x0000000020000000)
#define MTC_TUPLE_KEY_PRESERVED_FALSE      ID_ULONG(0x0000000000000000)

/* BUG-39399 remove search key preserved table */
/* mtcTuple.flag */
#define MTC_TUPLE_TARGET_UPDATE_DELETE_MASK  ID_ULONG(0x0000000040000000)
#define MTC_TUPLE_TARGET_UPDATE_DELETE_TRUE  ID_ULONG(0x0000000040000000)
#define MTC_TUPLE_TARGET_UPDATE_DELETE_FALSE ID_ULONG(0x0000000000000000)

/* BUG-44382 clone tuple ���ɰ��� */
/* mtcTuple.flag */
# define MTC_TUPLE_ROW_MEMSET_MASK         ID_ULONG(0x0000000080000000)
# define MTC_TUPLE_ROW_MEMSET_TRUE         ID_ULONG(0x0000000080000000)
# define MTC_TUPLE_ROW_MEMSET_FALSE        ID_ULONG(0x0000000000000000)

/* PROJ-2719 Multi table update/delete */
# define MTC_TUPLE_VIEW_PADNULL_MASK       ID_ULONG(0x0000000100000000)
# define MTC_TUPLE_VIEW_PADNULL_TRUE       ID_ULONG(0x0000000100000000)
# define MTC_TUPLE_VIEW_PADNULL_FALSE      ID_ULONG(0x0000000000000000)

# define MTC_TUPLE_COLUMN_ID_MAXIMUM            (1024)
/* to_char�� ���� date -> char ��ȯ�� precision      */
# define MTC_TO_CHAR_MAX_PRECISION                 (64)

/* PROJ-2446 ONE SOURCE XDB REPLACTION
 * XSN_TO_LSN �Լ� SN�� LSN���� ��ȯ�� �� max precision */
# define MTC_XSN_TO_LSN_MAX_PRECISION              (40)


/* mtdModule.compare                                 */
/* mtdModule.partialKey                              */
/* aFlag VON mtk::setPartialKey                      */
# define MTD_COMPARE_ASCENDING                      (0)
# define MTD_COMPARE_DESCENDING                     (1)

/* PROJ-2180
   MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL add */
# define MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL       (0)
# define MTD_COMPARE_MTDVAL_MTDVAL                   (1)
# define MTD_COMPARE_STOREDVAL_MTDVAL                (2)
# define MTD_COMPARE_STOREDVAL_STOREDVAL             (3)
# define MTD_COMPARE_INDEX_KEY_FIXED_MTDVAL          (4)
# define MTD_COMPARE_INDEX_KEY_MTDVAL                (5)
# define MTD_COMPARE_FUNC_MAX_CNT                    (6)

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
// PROJ-1904 Extend UDT
# define MTD_PSM_TYPE_MASK                 (0x00000080)
# define MTD_PSM_TYPE_ENABLE               (0x00000080)
# define MTD_PSM_TYPE_DISABLE              (0x00000000)

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

// PROJ-2163 Bind revise
// ���ο����� ���̴� Ÿ��
// Typed literal �̳� CAST ���� �� �ܺο��� 
// mtd::moduleByName �� ���� Ÿ���� ã�� ���
// ������ �߻��Ѵ�.
# define MTD_INTERNAL_TYPE_MASK            (0x20000000)
# define MTD_INTERNAL_TYPE_FALSE           (0x00000000)
# define MTD_INTERNAL_TYPE_TRUE            (0x20000000)

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

/* PROJ-2433 Direct Key Index
 * ���� direct key�� partial direct key���� ���� 
 * aFlag VON mtdFunctions */
# define MTD_PARTIAL_KEY_MASK              (0x00000002)
# define MTD_PARTIAL_KEY_ON                (0x00000002)
# define MTD_PARTIAL_KEY_OFF               (0x00000000)

/* mtcCallBack.flag                                  */
// Child Node�� ���� Estimation ���θ� ǥ����
// ENABLE�� ���, Child�� ���� Estimate�� �����ϸ�,
// DISABLE�� ���, �ڽſ� ���� Estimate���� �����Ѵ�.
# define MTC_ESTIMATE_ARGUMENTS_MASK       (0x00000001)
# define MTC_ESTIMATE_ARGUMENTS_DISABLE    (0x00000001)
# define MTC_ESTIMATE_ARGUMENTS_ENABLE     (0x00000000)

/* mtcCallBack.flag                                  */
// PROJ-2527 WITHIN GROUP AGGR
// initialize���� ����
# define MTC_ESTIMATE_INITIALIZE_MASK      (0x00000002)
# define MTC_ESTIMATE_INITIALIZE_TRUE      (0x00000002)
# define MTC_ESTIMATE_INITIALIZE_FALSE     (0x00000000)

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
# define MTL_NCHAR_PRECISION( aMtlModule )              \
    (                                                   \
        ( (aMtlModule)->id == MTL_UTF8_ID )             \
        ? MTL_UTF8_PRECISION : MTL_UTF16_PRECISION      \
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
//PROJ-2002 Column Security
# define MTC_POLICY_NAME_SIZE  (16-1)

/* BUG-39237 add sys_guid() function */
#define MTF_SYS_GUID_LENGTH   ((UShort) (32))
#define MTF_SYS_HOSTID_LENGTH ((UShort) ( 8))
#define MTF_SYS_HOSTID_FORMAT "%08"

/* BUG-25198 UNIX */
#define MTF_BASE_UNIX_DATE  ID_LONG(62135769600)
#define MTF_MAX_UNIX_DATE   ID_LONG(64060588799)

typedef struct mtcId            mtcId;
typedef struct mtcCallBack      mtcCallBack;
typedef struct mtcCallBackInfo  mtcCallBackInfo;
typedef struct mtcColumn        mtcColumn;
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

typedef struct mtdValueInfo     mtdValueInfo;

// PROJ-2002 Column Security
typedef struct mtcEncryptInfo   mtcEncryptInfo;

/* PROJ-2632 */
typedef struct mtxModule        mtxModule;
typedef union  mtxEntry         mtxEntry;

typedef enum
{
    NC_VALID = 0,         /* �������� 1���� */
    NC_INVALID,           /* firstbyte�� �ڵ忡 �ȸ´� 1���� */
    NC_MB_INVALID,        /* secondbyte���İ� �ڵ忡 �ȸ´� 1���� */
    NC_MB_INCOMPLETED     /* ��Ƽ����Ʈ �����ε� byte�� ���ڸ� 0���� */
} mtlNCRet;

#define MTC_CURSOR_PTR  void*

typedef IDE_RC (*mtcInitSubqueryFunc)( mtcNode*     aNode,
                                       mtcTemplate* aTemplate );

typedef IDE_RC (*mtcFetchSubqueryFunc)( mtcNode*     aNode,
                                        mtcTemplate* aTemplate,
                                        idBool*      aRowExist );

typedef IDE_RC (*mtcFiniSubqueryFunc)( mtcNode*     aNode,
                                       mtcTemplate* aTemplate );

/* PROJ-2448 Subquery caching */
typedef IDE_RC (*mtcSetCalcSubqueryFunc)( mtcNode     * aNode,
                                          mtcTemplate * aTemplate );

//PROJ-1583 large geometry
typedef UInt   (*mtcGetSTObjBufSizeFunc)( mtcCallBack * aCallBack );

typedef IDE_RC (*mtcGetOpenedCursorFunc)( mtcTemplate     * aTemplate,
                                          UShort            aTableID,
                                          MTC_CURSOR_PTR  * aCursor,
                                          UShort          * aOrgTableID,
                                          idBool          * aFound );

// BUG-40427
typedef IDE_RC (*mtcAddOpenedLobCursorFunc)( mtcTemplate   * aTemplate,
                                             smLobLocator    aLocator );

/* PROJ-1530 PSM/Trigger���� LOB ����Ÿ Ÿ�� ���� */
typedef idBool (*mtcIsBaseTable)( mtcTemplate * aTemplate,
                                  UShort        aTable );

typedef IDE_RC (*mtcCloseLobLocator)( smLobLocator   aLocator );

typedef IDE_RC (*mtdInitializeFunc)( UInt sNo );

typedef IDE_RC (*mtdEstimateFunc)(  UInt * aColumnSize,
                                    UInt * aArguments,
                                    SInt * aPrecision,
                                    SInt * aScale );

typedef IDE_RC (*mtdChangeFunc)( mtcColumn* aColumn,
                                 UInt       aFlag );

typedef IDE_RC (*mtdValueFunc)( mtcTemplate* aTemplate,  // fix BUG-15947
                                mtcColumn*   aColumn,
                                void*        aValue,
                                UInt*        aValueOffset,
                                UInt         aValueSize,
                                const void*  aToken,
                                UInt         aTokenLength,
                                IDE_RC*      aResult );

typedef UInt (*mtdActualSizeFunc)( const mtcColumn* aColumn,
                                   const void*      aRow );

typedef IDE_RC (*mtdGetPrecisionFunc)( const mtcColumn * aColumn,
                                       const void      * aRow,
                                       SInt            * aPrecision,
                                       SInt            * aScale );

typedef void (*mtdNullFunc)( const mtcColumn* aColumn,
                             void*            aRow );

typedef UInt (*mtdHashFunc)( UInt             aHash,
                             const mtcColumn* aColumn,
                             const void*      aRow );

typedef idBool (*mtdIsNullFunc)( const mtcColumn* aColumn,
                                 const void*      aRow );

typedef IDE_RC (*mtdIsTrueFunc)( idBool*          aResult,
                                 const mtcColumn* aColumn,
                                 const void*      aRow );

typedef SInt (*mtdCompareFunc)( mtdValueInfo * aValueInfo1,
                                mtdValueInfo * aValueInfo2 );

typedef UChar (*mtsRelationFunc)( const mtcColumn* aColumn1,
                                  const void*      aRow1,
                                  UInt             aFlag1,
                                  const mtcColumn* aColumn2,
                                  const void*      aRow2,
                                  UInt             aFlag2,
                                  const void*      aInfo );

typedef IDE_RC (*mtdCanonizeFunc)( const mtcColumn  *  aCanon,
                                   void             ** aCanonized,
                                   mtcEncryptInfo   *  aCanonInfo,
                                   const mtcColumn  *  aColumn,
                                   void             *  aValue,
                                   mtcEncryptInfo   *  aColumnInfo,
                                   mtcTemplate      *  aTemplate );

typedef void (*mtdEndianFunc)( void* aValue );

typedef IDE_RC (*mtdValidateFunc)( mtcColumn * aColumn,
                                   void      * aValue,
                                   UInt        aValueSize);

typedef IDE_RC (*mtdEncodeFunc) ( mtcColumn  * aColumn,
                                  void       * aValue,
                                  UInt         aValueSize,
                                  UChar      * aCompileFmt,
                                  UInt         aCompileFmtLen,
                                  UChar      * aText,
                                  UInt       * aTextLen,
                                  IDE_RC     * aRet );

typedef IDE_RC (*mtdDecodeFunc) ( mtcTemplate* aTemplate,  // fix BUG-15947
                                  mtcColumn  * aColumn,
                                  void       * aValue,
                                  UInt       * aValueSize,
                                  UChar      * aCompileFmt,
                                  UInt         aCompileFmtLen,
                                  UChar      * aText,
                                  UInt         aTextLen,
                                  IDE_RC     * aRet );

typedef IDE_RC (*mtdCompileFmtFunc)	( mtcColumn  * aColumn,
                                      UChar      * aCompiledFmt,
                                      UInt       * aCompiledFmtLen,
                                      UChar      * aFormatString,
                                      UInt         aFormatStringLength,
                                      IDE_RC     * aRet );

typedef IDE_RC (*mtdValueFromOracleFunc)( mtcColumn*  aColumn,
                                          void*       aValue,
                                          UInt*       aValueOffset,
                                          UInt        aValueSize,
                                          const void* aOracleValue,
                                          SInt        aOracleLength,
                                          IDE_RC*     aResult );

typedef IDE_RC (*mtdMakeColumnInfo) ( void * aStmt,
                                      void * aTableInfo,
                                      UInt   aIdx );

// BUG-28934
// key range�� AND merge�ϴ� �Լ�
typedef IDE_RC (*mtcMergeAndRange)( smiRange* aMerged,
                                    smiRange* aRange1,
                                    smiRange* aRange2 );

// key range�� OR merge�ϴ� �Լ�
typedef IDE_RC (*mtcMergeOrRangeList)( smiRange  * aMerged,
                                       smiRange ** aRangeList,
                                       SInt        aRangeCount );

//-----------------------------------------------------------------------
// Data Type�� Selectivity ���� �Լ�
// Selectivity ����
//     Selectivity = (aValueMax - aValueMin) / (aColumnMax - aColumnMin)
//-----------------------------------------------------------------------

typedef SDouble (*mtdSelectivityFunc) ( void     * aColumnMax,  // Column�� MAX��
                                        void     * aColumnMin,  // Column�� MIN��
                                        void     * aValueMax,   // MAX Value ��
                                        void     * aValueMin,   // MIN Value ��
                                        SDouble    aBoundFactor,
                                        SDouble    aTotalRecordCnt );

typedef IDE_RC (*mtfInitializeFunc)( void );

typedef IDE_RC (*mtfFinalizeFunc)( void );

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

typedef IDE_RC (*mtdStoredValue2MtdValueFunc) ( UInt              aColumnSize,
                                                void            * aDestValue,
                                                UInt              aDestValueOffset,
                                                UInt              aLength,
                                                const void      * aValue );

/***********************************************************************
 * PROJ-1705
 * �� ����ŸŸ���� null Value�� ũ�� ��ȯ
 * replication���� ���.
 **********************************************************************/

typedef UInt (*mtdNullValueSizeFunc) ();

/***********************************************************************
 * PROJ-1705
 * length�� ������ ����ŸŸ���� length ������ �����ϴ� ������ ũ�� ��ȯ
 * ��) mtdCharType( UShort length; UChar value[1] ) ����
 *      lengthŸ���� UShort�� ũ�⸦ ��ȯ
 *  integer�� ���� �������� ����ŸŸ���� 0 ��ȯ
 **********************************************************************/
typedef UInt (*mtdHeaderSizeFunc) ();


/***********************************************************************
* PROJ-2399 row tmaplate
* sm�� ����Ǵ� �������� ũ�⸦ ��ȯ�Ѵ�.( mtheader ũ�⸦ ����.)
* varchar�� ���� variable Ÿ���� ������ Ÿ���� 0�� ��ȯ
 **********************************************************************/
typedef UInt (*mtdStoreSizeFunc) ( const smiColumn * aColumn );

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

typedef IDE_RC (*mtfEstimateFunc)( mtcNode*     aNode,
                                   mtcTemplate* aTemplate,
                                   mtcStack*    aStack,
                                   SInt         aRemain,
                                   mtcCallBack* aCallBack );

//----------------------------------------------------------------
// PROJ-1358
// ����) mtfCalculateFunc, mtfEstimateRangeFunc ����
//       Internal Tuple Set�� Ȯ������ ���� ������ �߻����� ������,
//       mtcTuple* �� ���ڷ� ���ϴ� �Լ��� ��� �����Ͽ�
//       ������ ������ �����Ѵ�.
//----------------------------------------------------------------

typedef IDE_RC (*mtfCalculateFunc)( mtcNode*     aNode,
                                    mtcStack*    aStack,
                                    SInt         aRemain,
                                    void*        aInfo,
                                    mtcTemplate* aTemplate );

typedef IDE_RC (*mtfEstimateRangeFunc)( mtcNode*      aNode,
                                        mtcTemplate* aTemplate,
                                        UInt         aArgument,
                                        UInt*        aSize );

/* PROJ-2632 */
typedef IDE_RC (*mtxSerialExecuteFunc)( mtxEntry ** aEntry );

typedef mtxSerialExecuteFunc (*mtxGetSerialExecute)( UInt aMtd1Type,
                                                     UInt aMtd2Type );

//----------------------------------------------------------------
// PROJ-1358
// ����) mtfCalculateFunc, mtfEstimateRangeFunc ����
//       Internal Tuple Set�� Ȯ������ ���� ������ �߻����� ������,
//       mtcTuple* �� ���ڷ� ���ϴ� �Լ��� ��� �����Ͽ�
//       ������ ������ �����Ѵ�.
//----------------------------------------------------------------

typedef IDE_RC (*mtfExtractRangeFunc)( mtcNode*       aNode,
                                       mtcTemplate*   aTemplate,
                                       mtkRangeInfo * aInfo,
                                       smiRange*      aRange );

typedef IDE_RC (*mtfInitConversionNodeFunc)( mtcNode** aConversionNode,
                                             mtcNode*  aNode,
                                             void*     aInfo );

typedef void (*mtfSetIndirectNodeFunc)( mtcNode* aNode,
                                        mtcNode* aConversionNode,
                                        void*    aInfo );

typedef IDE_RC (*mtfAllocFunc)( void* aInfo,
                                UInt  aSize,
                                void**);

typedef const void* (*mtcGetColumnFunc)( const void*      aRow,
                                         const smiColumn* aColumn,
                                         UInt*            aLength );

typedef const void* (*mtcGetCompressionColumnFunc)( const void*      aRow,
                                                    const smiColumn* aColumn,
                                                    idBool           aUseColumnOffset,
                                                    UInt*            aLength );

typedef IDE_RC (*mtcOpenLobCursorWithRow)( MTC_CURSOR_PTR    aTableCursor,
                                           void*             aRow,
                                           const smiColumn*  aLobColumn,
                                           UInt              aInfo,
                                           smiLobCursorMode  aMode,
                                           smLobLocator*     aLobLocator );

typedef IDE_RC (*mtcOpenLobCursorWithGRID)(MTC_CURSOR_PTR    aTableCursor,
                                           scGRID            aGRID,
                                           smiColumn*        aLobColumn,
                                           UInt              aInfo,
                                           smiLobCursorMode  aMode,
                                           smLobLocator*     aLobLocator);

typedef IDE_RC (*mtcReadLob)( idvSQL*      aStatistics,
                              smLobLocator aLobLoc,
                              UInt         aOffset,
                              UInt         aMount,
                              UChar*       aPiece,
                              UInt*        aReadLength );

typedef IDE_RC (*mtcGetLobLengthWithLocator)( idvSQL*      aStatistics,
                                              smLobLocator aLobLoc,
                                              SLong*       aLobLen,
                                              idBool*      aIsNullLob );

typedef idBool (*mtcIsNullLobColumnWithRow)( const void      * aRow,
                                             const smiColumn * aColumn );

/* PROJ-2446 ONE SOURCE */
typedef idvSQL* (*mtcGetStatistics)( mtcTemplate * aTemplate );

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
    UInt     sessionID;
    SChar  * ipAddr;
    SChar  * userName;

    // statement info
    SChar  * stmtType;   // select/insert/update/delete

    // column info
    SChar  * tableOwnerName;
    SChar  * tableName;
    SChar  * columnName;
};

// �Ϻ�ȣȭ�� ���ȸ�⿡ �ʿ��� ������ �����ϱ� ���� �ڷᱸ��
struct mtcECCInfo
{
    // session info
    UInt     sessionID;
};

// Echar, Evarchar canonize�� ����
typedef IDE_RC (*mtcEncrypt) ( mtcEncryptInfo  * aEncryptInfo,
                               SChar           * aPolicyName,
                               UChar           * aPlain,
                               UShort            aPlainSize,
                               UChar           * aCipher,
                               UShort          * aCipherSize );

// Echar->Char, Evarchar->Varchar conversion�� ����
typedef IDE_RC (*mtcDecrypt) ( mtcEncryptInfo  * aEncryptInfo,
                               SChar           * aPolicyName,
                               UChar           * aCipher,
                               UShort            aCipherSize,
                               UChar           * aPlain,
                               UShort          * aPlainSize );

// Char->Echar, Varchar->Evarchar conversion�� ����
typedef IDE_RC (*mtcEncodeECC) ( mtcECCInfo   * aInfo,
                                 UChar        * aPlain,
                                 UShort         aPlainSize,
                                 UChar        * aCipher,
                                 UShort       * aCipherSize );

// decrypt�� ���� decryptInfo�� ������
typedef IDE_RC (*mtcGetDecryptInfo) ( mtcTemplate     * aTemplate,
                                      UShort            aTable,
                                      UShort            aColumn,
                                      mtcEncryptInfo  * aDecryptInfo );

// ECC�� ���� ECCInfo�� ������
typedef IDE_RC (*mtcGetECCInfo) ( mtcTemplate * aTemplate,
                                  mtcECCInfo  * aECCInfo );

// Echar, Evarchar�� initializeColumn�� ����
typedef IDE_RC (*mtcGetECCSize) ( UInt    aSize,
                                  UInt  * aECCSize );

// PROJ-1579 NCHAR
typedef SChar * (*mtcGetDBCharSet)();
typedef SChar * (*mtcGetNationalCharSet)();

struct mtcExtCallback
{
    mtcGetColumnFunc            getColumn;
    mtcOpenLobCursorWithRow     openLobCursorWithRow;
    mtcOpenLobCursorWithGRID    openLobCursorWithGRID;
    mtcReadLob                  readLob;
    mtcGetLobLengthWithLocator  getLobLengthWithLocator;
    mtcIsNullLobColumnWithRow   isNullLobColumnWithRow;
    /* PROJ-2446 ONE SOURCE */
    mtcGetStatistics            getStatistics;
    // PROJ-1579 NCHAR
    mtcGetDBCharSet             getDBCharSet;
    mtcGetNationalCharSet       getNationalCharSet;

    // PROJ-2264 Dictionary table
    mtcGetCompressionColumnFunc getCompressionColumn;
};

// PROJ-1361 : language�� ���� ���� ���ڷ� �̵�
// PROJ-1755 : nextChar �Լ� ����ȭ
// PROJ-1579,PROJ-1681, BUG-21117
// ��Ȯ�� ���ڿ� üũ�� ���� fence�� nextchar�ȿ��� üũ�ؾ� ��.
typedef mtlNCRet (*mtlNextCharFunc)( UChar ** aSource, UChar * aFence );

// PROJ-1361 : language�� ���� �׿� �´� chr() �Լ��� ���� ���
typedef IDE_RC (*mtlChrFunc)( UChar      * aResult,
                              const SInt   aSource );

// PROJ-1361 : mtlExtractFuncSet �Ǵ� mtlNextDayFuncSet���� ���
typedef SInt (*mtlMatchFunc)( UChar* aSource,
                              UInt   aSourceLen );

// BUG-13830 ���ڰ����� ���� language�� �ִ� precision ���
typedef SInt (*mtlMaxPrecisionFunc)( SInt aLength );

/* PROJ-2208 : Multi Currency */
typedef IDE_RC ( *mtcGetCurrencyFunc )( mtcTemplate * aTemplate,
                                        mtlCurrency * aCurrency );

struct mtcId {
    UInt dataTypeId;
    UInt languageId;
};

// Estimate �� ���� �ΰ� ����
struct mtcCallBack {
    void*                       info;     // (mtcCallBackInfo or qtcCallBackInfo)

    UInt                        flag;     // Child�� ���� estimate ����
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

// mtcCallBack.info�� ���Ǹ�, ����� ���� �����ÿ��� ���ȴ�.
struct mtcCallBackInfo {
    iduMemory* memory;            // ����ϴ� Memory Mgr
    UShort     columnCount;       // ���� Column�� ����
    UShort     columnMaximum;     // Column�� �ִ� ����
};

// PROJ-2002 Column Security
struct mtcColumnEncryptAttr {
    SInt       mEncPrecision;                      // ��ȣ ����Ÿ Ÿ���� precision
    SChar      mPolicy[MTC_POLICY_NAME_SIZE+1];    // ���� ��å�� �̸� (Null Terminated String)
};

// PROJ-2422 srid
struct mtcColumnSRIDAttr {
    SInt       mSrid;
};

// column�� �ΰ�����
union mtcColumnAttr {
    mtcColumnEncryptAttr  mEncAttr;
    mtcColumnSRIDAttr     mSridAttr;
};

struct mtcColumn {
    smiColumn         column;
    mtcId             type;
    UInt              flag;
    SInt              precision;
    SInt              scale;

    // column�� �ΰ�����
    mtcColumnAttr     mColumnAttr;
    
    const mtdModule * module;
    const mtlModule * language; // PROJ-1361 : add language module
};

// mtcColumn���� �ʿ��� �κи� �����Ѵ�.
#define MTC_COPY_COLUMN_DESC(_dst_, _src_)               \
{                                                        \
    (_dst_)->column.id       = (_src_)->column.id;       \
    (_dst_)->column.flag     = (_src_)->column.flag;     \
    (_dst_)->column.offset   = (_src_)->column.offset;   \
    (_dst_)->column.varOrder = (_src_)->column.varOrder; \
    (_dst_)->column.vcInOutBaseSize = (_src_)->column.vcInOutBaseSize; \
    (_dst_)->column.size     = (_src_)->column.size;     \
    (_dst_)->column.align    = (_src_)->column.align;    \
    (_dst_)->column.maxAlign = (_src_)->column.maxAlign; \
    (_dst_)->column.value    = (_src_)->column.value;    \
    (_dst_)->column.colSpace = (_src_)->column.colSpace; \
    (_dst_)->column.colSeg   = (_src_)->column.colSeg;   \
    (_dst_)->column.colType  = (_src_)->column.colType;  \
    (_dst_)->column.descSeg  = (_src_)->column.descSeg;  \
    (_dst_)->column.mDictionaryTableOID = (_src_)->column.mDictionaryTableOID; \
    (_dst_)->type            = (_src_)->type;            \
    (_dst_)->flag            = (_src_)->flag;            \
    (_dst_)->precision       = (_src_)->precision;       \
    (_dst_)->scale           = (_src_)->scale;           \
    (_dst_)->mColumnAttr.mEncAttr.mEncPrecision    = (_src_)->mColumnAttr.mEncAttr.mEncPrecision;    \
    idlOS::memcpy((_dst_)->mColumnAttr.mEncAttr.mPolicy, (_src_)->mColumnAttr.mEncAttr.mPolicy, MTC_POLICY_NAME_SIZE+1); \
    (_dst_)->module          = (_src_)->module;          \
    (_dst_)->language        = (_src_)->language;        \
}

// ������ ������ ���� ���� �Լ� ����
struct mtcExecute {
    mtfCalculateFunc     initialize;    // Aggregation �ʱ�ȭ �Լ�
    mtfCalculateFunc     aggregate;     // Aggregation ���� �Լ�
    mtfCalculateFunc     merge;         // Aggregation �ӽð����� ������Ű�� �Լ�
    mtfCalculateFunc     finalize;      // Aggregation ���� �Լ�
    mtfCalculateFunc     calculate;     // ���� �Լ�
    void*                calculateInfo; // ������ ���� �ΰ� ����, ���� �̻��
    mtxSerialExecuteFunc mSerialExecute;/* PROJ-2632 */
    mtfEstimateRangeFunc estimateRange; // Key Range ũ�� ���� �Լ�
    mtfExtractRangeFunc  extractRange;  // Key Range ���� �Լ�
};

// ������ Ÿ��, ���� ���� �̸��� ����
struct mtcName {
    mtcName*    next;   // �ٸ� �̸�
    UInt        length; // �̸��� ����, Bytes
    void*       string; // �̸�, UTF-8
};

// PROJ-1473
struct mtcColumnLocate {
    UShort      table;
    UShort      column;
};

struct mtcNode {
    const mtfModule* module;
    mtcNode*         conversion;
    mtcNode*         leftConversion;
    mtcNode*         funcArguments;   // PROJ-1492 CAST( operand AS target )
    mtcNode*         orgNode;         // PROJ-1404 original const expr node
    mtcNode*         arguments;
    mtcNode*         next;
    UShort           table;
    UShort           column;
    UShort           baseTable;       // PROJ-2002 column�� ��� base table
    UShort           baseColumn;      // PROJ-2002 column�� ��� base column
    ULong            lflag;
    ULong            cost;
    UInt             info;
    smOID            objectID;        // PROJ-1073 Package
};

#define MTC_NODE_INIT(_node_)               \
{                                           \
    (_node_)->module         = NULL;        \
    (_node_)->conversion     = NULL;        \
    (_node_)->leftConversion = NULL;        \
    (_node_)->funcArguments  = NULL;        \
    (_node_)->orgNode        = NULL;        \
    (_node_)->arguments      = NULL;        \
    (_node_)->next           = NULL;        \
    (_node_)->table          = 0;           \
    (_node_)->column         = 0;           \
    (_node_)->baseTable      = 0;           \
    (_node_)->baseColumn     = 0;           \
    (_node_)->lflag          = 0;           \
    (_node_)->cost           = 0;           \
    (_node_)->info           = ID_UINT_MAX; \
    (_node_)->objectID       = 0;           \
}

struct mtkRangeCallBack
{
    UInt              flag; // range ���� �� �ʿ��� �������� ����
                            // (1) Asc/Desc 
                            // (2) ���� �迭 �� ���� �ٸ� data type����
                            //     compare �Լ����� ����
                            // (3) Operator ���� ( <=, >=, <, > )
    UInt              columnIdx; // STOREDVAL�� �񱳿� ���Ǵ� Į�� �ε���
                                 // (Į�� ��ġ ����)
    
    mtdCompareFunc    compare;

    // PROJ-1436
    // columnDesc, valueDesc�� shared template�� column��
    // ����ؼ��� �ȵȴ�. variable type column�� ���
    // ������ ������ �߻��� �� �ִ�. ���� column ������
    // �����ؼ� ����Ѵ�.
    // ���� private template�� column ������ ����� ��
    // �ֵ��� �����ؾ� �Ѵ�.
    mtcColumn         columnDesc;
    mtcColumn         valueDesc;
    const void*       value;


    mtkRangeCallBack* next;
};

// PROJ-2527 WITHIN GROUP AGGR
// mt���� ���Ǵ� memory pool.
typedef struct mtfFuncDataBasicInfo
{
    iduMemory            * memoryMgr;

    /* BUG-46906 */
    iduMemoryStatus        mMemStatus;
} mtfFuncDataBasicInfo;

// BUG-41944 high precision/performance hint ����
typedef enum
{
    MTC_ARITHMETIC_OPERATION_PRECISION = 0,           // �������� ���е��켱
    MTC_ARITHMETIC_OPERATION_PERFORMANCE_LEVEL1 = 1,  // �������� ���ɿ켱
    MTC_ARITHMETIC_OPERATION_PERFORMANCE_LEVEL2 = 2,  // �������� ���ɿ켱 SUM, AVG
    MTC_ARITHMETIC_OPERATION_MAX_PRECISION = 3,       /* BUG-46195 */
    MTC_ARITHMETIC_OPERATION_DEFAULT = MTC_ARITHMETIC_OPERATION_PERFORMANCE_LEVEL1
} mtcArithmeticOpMode;

//----------------------------------------------------------
// Key Range ������ ���� ����
// - column : Index Column ����, Memory/Disk Index�� ����
// - argument : Index Column�� ��ġ (0: left, 1: right)
// - compValueType : ���ϴ� ������ type
//                   MTD_COMPARE_FIXED_MTDVAL_FIXED_MTDVAL
//                   MTD_COMPARE_MTDVAL_MTDVAL
//                   MTD_COMPARE_STOREDVAL_MTDVAL
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
    mtcColumn * column;    
    UInt        argument;   
    UInt        compValueType; 
    UInt        direction;  
    idBool      isSameGroupType;
    UInt        columnIdx;
    UInt        useOffset; /* BUG-47690 Inlist ���� Index fatal */
};

struct mtkRangeFuncIndex
{
    smiCallBackFunc  rangeFunc;
    UInt             idx;
};

struct mtkCompareFuncIndex
{
    mtdCompareFunc   compareFunc;
    UInt             idx;
};

struct mtcStack {
    mtcColumn* column;
    void*      value;
};

struct mtcTuple {
    UInt                    modify;
    ULong                   lflag;
    UShort                  columnCount;
    UShort                  columnMaximum;
    UShort                  partitionTupleID;
    mtcColumn             * columns;
    mtcColumnLocate       * columnLocate; // PROJ-1473    
    mtcExecute            * execute;
    mtcExecute            * ridExecute;   // PROJ-1789 PROWID
    UInt                    rowOffset;
    UInt                    rowMaximum;
    void                  * tableHandle;
    scGRID                  rid;          // Disk Table�� ���� Record IDentifier
    void                  * row;
    void                  * cursorInfo;   // PROJ-2205 rownum in DML
};

struct mtcTemplate {
    mtcStack              * stackBuffer;
    SInt                    stackCount;
    mtcStack              * stack;
    SInt                    stackRemain;
    UChar                 * data;         // execution������ data����
    UInt                    dataSize;
    UChar                 * execInfo;     // ���� ���� �Ǵ��� ���� ����
    UInt                    execInfoCnt;  // ���� ���� ������ ����
    mtcInitSubqueryFunc     initSubquery;
    mtcFetchSubqueryFunc    fetchSubquery;
    mtcFiniSubqueryFunc     finiSubquery;
    mtcSetCalcSubqueryFunc  setCalcSubquery; // PROJ-2448

    mtcGetOpenedCursorFunc  getOpenedCursor; // PROJ-1362
    mtcAddOpenedLobCursorFunc  addOpenedLobCursor; // BUG-40427
    mtcIsBaseTable          isBaseTable;     // PROJ-1362
    mtcCloseLobLocator      closeLobLocator; // PROJ-1362
    mtcGetSTObjBufSizeFunc  getSTObjBufSize; // PROJ-1583 large geometry

    // PROJ-2002 Column Security
    mtcEncrypt              encrypt;
    mtcDecrypt              decrypt;
    mtcEncodeECC            encodeECC;
    mtcGetDecryptInfo       getDecryptInfo;
    mtcGetECCInfo           getECCInfo;
    mtcGetECCSize           getECCSize;

    // PROJ-1358 Internal Tuple�� �������� �����Ѵ�.
    UShort                  currentRow[MTC_TUPLE_TYPE_MAXIMUM];
    UShort                  rowArrayCount;    // �Ҵ�� internal tuple�� ����
    UShort                  rowCount;         // ������� internal tuple�� ����
    UShort                  rowCountBeforeBinding;
    UShort                  variableRow;
    mtcTuple              * rows;

    SChar                 * dateFormat;
    idBool                  dateFormatRef;
    
    /* PROJ-2208 Multi Currency */
    mtcGetCurrencyFunc      nlsCurrency;
    idBool                  nlsCurrencyRef;

    // BUG-37247
    idBool                  groupConcatPrecisionRef;

    // BUG-41944
    mtcArithmeticOpMode     arithmeticOpMode;
    idBool                  arithmeticOpModeRef;
    
    // PROJ-1579 NCHAR
    const mtlModule       * nlsUse;

    // PROJ-2527 WITHIN GROUP AGGR
    mtfFuncDataBasicInfo ** funcData;
    UInt                    funcDataCnt;
};

struct mtvConvert{
    UInt              count;
    mtfCalculateFunc* convert;
    mtcColumn*        columns;
    mtcStack*         stack;
};

struct mtvTable {
    ULong             cost;
    UInt              count;
    const mtvModule** modules;
    const mtvModule*  module;
};

struct mtfSubModule {
    mtfSubModule*   next;
    mtfEstimateFunc estimate;
};

typedef struct mtfNameIndex {
    const mtcName*   name;
    const mtfModule* module;
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

    UInt                   id;
    UInt                   no;
    UChar                  idxType[MTD_MAX_USABLE_INDEX_CNT];
    UInt                   align;
    UInt                   flag;
    UInt                   maxStorePrecision;   // BUG-28921 store precision maximum
    SInt                   minScale;     // PROJ-1362
    SInt                   maxScale;     // PROJ-1362
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
    mtdCompareFunc         logicalCompare[2];   // Type�� ���� ��
    mtdCompareFunc         keyCompare[MTD_COMPARE_FUNC_MAX_CNT][2]; // �ε��� Key ��
    mtdCanonizeFunc        canonize;
    mtdEndianFunc          endian;
    mtdValidateFunc        validate;
    mtdSelectivityFunc     selectivity;  // A4 : Selectivity ��� �Լ�
    mtdEncodeFunc          encode;
    mtdDecodeFunc          decode;
    mtdCompileFmtFunc      compileFmt;
    mtdValueFromOracleFunc valueFromOracle;
    mtdMakeColumnInfo      makeColumnInfo;

    // BUG-28934
    // data type�� Ư���� �°� key range�� ������ ��, �̸� merge�� ����
    // key range�� Ư���� �°� merge�ϴ� ����� �ʿ��ϴ�. merge�� �ᱹ
    // compare�� �����̹Ƿ� compare�Լ��� ������ mtdModule�� �����Ѵ�.
    mtcMergeAndRange       mergeAndRange;
    mtcMergeOrRangeList    mergeOrRangeList;
    
    //-------------------------------------------------------------------
    // PROJ-1705
    // ��ũ���̺��� ���ڵ� ���屸�� �������� ����
    // ��ũ ���̺� ���ڵ� ��ġ�� 
    // �÷������� qp ���ڵ�ó�������� �ش� �÷���ġ�� ���� 
    // PROJ-2429 Dictionary based data compress for on-disk DB
    // disk�� dictionary column�� fetch�Ҷ� ����ϴ� �Լ� �����͸� �߰���
    // ���� �Ѵ�.(mtd::mtdStoredValue2MtdValue4CompressColumn)
    // char, varchar, nchar nvarchar, byte, nibble, bit, varbit, data Ÿ����
    // �شܵǸ� ������ data type�� null�� ����ȴ�.
    //-------------------------------------------------------------------
    mtdStoredValue2MtdValueFunc  storedValue2MtdValue[MTD_COLUMN_COPY_FUNC_MAX_COUNT];    

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

    //-------------------------------------------------------------------
    // PROJ-2399 row tmaplate
    // varchar�� ���� variable Ÿ���� ������ Ÿ���� ID_UINT_MAX�� ��ȯ
    // mtheader�� sm�� ����Ȱ�찡 �ƴϸ� mtheaderũ�⸦ ���� ��ȯ
    //-------------------------------------------------------------------
    mtdStoreSizeFunc             storeSize;
};  

// �������� Ư¡�� ���
struct mtfModule {
    ULong             lflag; // �Ҵ��ϴ� Column�� ����, �������� ����
    ULong             lmask; // ���� Node�� flag�� �����Ͽ� �ش� ��������
                             // Index ��� ���� ���θ� flag�� Setting
    SDouble           selectivity;      // �������� default selectivity
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
    UInt                  id;
    
    mtlNextCharFunc       nextCharPtr;    // Language�� �°� ���� ���ڷ� �̵�
    mtlMaxPrecisionFunc   maxPrecision;   // Language�� �ִ� precision���
    mtlExtractFuncSet   * extractSet;     // Language�� �´� Date Type �̸�
    mtlNextDayFuncSet   * nextDaySet;     // Language�� �´� ���� �̸�
    
    UChar              ** specialCharSet;
    UChar                 specialCharSize;
};

struct mtvModule {
    const mtdModule* to;
    const mtdModule* from;
    ULong            cost;
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
    SChar    * mTypeName;
    SShort     mDataType;
    SShort     mODBCDataType; // mDataType�� ODBC SPEC���� ����
    SInt       mColumnSize;
    SChar    * mLiteralPrefix;
    SChar    * mLiteralSuffix;
    SChar    * mCreateParam;
    SShort     mNullable;
    SShort     mCaseSens;
    SShort     mSearchable;
    SShort     mUnsignedAttr;
    SShort     mFixedPrecScale;
    SShort     mAutoUniqueValue;
    SChar    * mLocalTypeName;
    SShort     mMinScale;
    SShort     mMaxScale;
    SShort     mSQLDataType;
    SShort     mODBCSQLDataType; // mSQLDataType�� ODBC SPEC���� ����
    SShort     mSQLDateTimeSub;
    SInt       mNumPrecRadix;
    SShort     mIntervalPrecision;
};

// PROJ-1362
// LobCursor.Info
#define MTC_LOB_COLUMN_NOTNULL_MASK        (0x00000001)
#define MTC_LOB_COLUMN_NOTNULL_TRUE        (0x00000001)
#define MTC_LOB_COLUMN_NOTNULL_FALSE       (0x00000000)

#define MTC_LOB_LOCATOR_CLOSE_MASK         (0x00000002)
#define MTC_LOB_LOCATOR_CLOSE_TRUE         (0x00000002)
#define MTC_LOB_LOCATOR_CLOSE_FALSE        (0x00000000)

// BUG-40427
// LobCursor.Info
// - ���ο����� ����ϴ� ���   : FALSE
// - Client������ ����ϴ� ��� : TRUE
#define MTC_LOB_LOCATOR_CLIENT_MASK        (0x00000004)
#define MTC_LOB_LOCATOR_CLIENT_TRUE        (0x00000004)
#define MTC_LOB_LOCATOR_CLIENT_FALSE       (0x00000000)

// PROJ-1362
// Lob�� ���� LENGTH, SUBSTR�� ������ �� Lob�����͸� �д� ����
#define MTC_LOB_BUFFER_SIZE     (32000)

// PROJ-1753 One Pass Like Algorithm
#define MTC_LIKE_PATTERN_MAX_SIZE (4000)
#define MTC_LIKE_HASH_KEY         (0x000FFFFF)
#define MTC_LIKE_SHIFT            (8)

#define MTC_FORMAT_BLOCK_STRING   (1)
#define MTC_FORMAT_BLOCK_PERCENT  (2)
#define MTC_FORMAT_BLOCK_UNDER    (3)

// 16byte�� ���߾���
struct mtcLikeBlockInfo
{    
    UChar   * start;

    // MTC_FORMAT_BLOCK_STRING�� ��� string byte size
    // MTC_FORMAT_BLOCK_PERCENT�� ��� ����
    // MTC_FORMAT_BLOCK_UNDER�� ��� ����
    UShort    sizeOrCnt;
    
    UChar     type;
};

// PROJ-1755
struct mtcLobCursor
{
    UInt     offset;
    UChar  * index;
};

// PROJ-1755
struct mtcLobBuffer
{
    smLobLocator   locator;
    UInt           length;
    
    UChar        * buf;
    UChar        * fence;
    UInt           offset;
    UInt           size;
};

// PROJ-1755
typedef enum
{                                   // format string�� ���Ͽ�
    MTC_FORMAT_NULL = 1,            // case 1) null Ȥ�� ''�� ����
    MTC_FORMAT_NORMAL,              // case 2) �Ϲݹ��ڷθ� ����
    MTC_FORMAT_UNDER,               // case 3) '_'�θ� ����
    MTC_FORMAT_PERCENT,             // case 4) '%'�θ� ����
    MTC_FORMAT_NORMAL_UNDER,        // case 5) �Ϲݹ��ڿ� '_'�� ����
    MTC_FORMAT_NORMAL_ONE_PERCENT,  // case 6-1) �Ϲݹ��ڿ� '%' 1�� �� ����
    MTC_FORMAT_NORMAL_MANY_PERCENT, // case 6-2) �Ϲݹ��ڿ� '%' 2���̻����� ����
    MTC_FORMAT_UNDER_PERCENT,       // case 7) '_'�� '%'�� ����
    MTC_FORMAT_ALL                  // case 8) �Ϲݹ��ڿ� '_', '%'�� ����
} mtcLikeFormatType;

// PROJ-1755
struct mtcLikeFormatInfo
{
    // escape���ڸ� ������ format pattern
    UChar             * pattern;
    UShort              patternSize;

    // format pattern�� �з�����
    mtcLikeFormatType   type;
    UShort              underCnt;
    UShort              percentCnt;
    UShort              charCnt;

    // '%'�� 1���� ����� �ΰ�����
    // [head]%[tail]
    UChar             * firstPercent;
    UChar             * head;     // pattern������ head ��ġ
    UShort              headSize;
    UChar             * tail;     // pattern������ tail ��ġ
    UShort              tailSize;

    // PROJ-1753 one pass like
    // case 5,6-2,8�� ���� ����ȭ
    //
    // i1 like '%abc%%__\_%de_' escape '\'
    // => 8 blocks
    //   1. percent '%',1
    //   2. string  'abc',3
    //   3. percent '%%',2
    //   4. under   '__',2
    //   5. string  '_',1
    //   6. percent '%',1
    //   7. string  'de',2
    //   8. under   '_',1
    //
    UInt                blockCnt;
    mtcLikeBlockInfo  * blockInfo;
    UChar             * refinePattern;    // escaped pattern
};

/* BUG-32622 inlist operator */
#define MTC_INLIST_ELEMENT_COUNT_MAX  (1000)
#define MTC_INLIST_KEYRANGE_COUNT_MAX (MTC_INLIST_ELEMENT_COUNT_MAX)
#define MTC_INLIST_ELEMRNT_LENGTH_MAX (4000)
/* ���ڿ� �ִ���� + (char_align-1)*MTC_INLIST_ELEMENT_COUNT_MAX + (char_header_size)*MTC_INLIST_ELEMENT_COUNT_MAX */
#define MTC_INLIST_VALUE_BUFFER_MAX   (32000 + 1*MTC_INLIST_ELEMENT_COUNT_MAX + 2*MTC_INLIST_ELEMENT_COUNT_MAX)

struct mtcInlistInfo
{
    UInt          count;                                         /* token ����               */
    mtcColumn     valueDesc;                                     /* value �� column descirpt */
    void        * valueArray[MTC_INLIST_ELEMENT_COUNT_MAX];      /* value pointer array      */
    ULong         valueBuf[MTC_INLIST_VALUE_BUFFER_MAX / 8 + 1]; /* ���� value buffer        */
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
typedef struct mtdValueInfo 
{
    const mtcColumn * column;
    const void      * value;
    UInt              length;
    UInt              flag;
} mtdValueInfo;

//-------------------------------------------------------------------
// BUG-33663
// ranking function�� ����ϴ� define
//-------------------------------------------------------------------
typedef enum
{
    MTC_RANK_VALUE_FIRST = 1,  // ù��° ��
    MTC_RANK_VALUE_SAME,       // ���� ���� ����.
    MTC_RANK_VALUE_DIFF        // ���� ���� �ٸ���.
} mtcRankValueType;

// PROJ-1789 PROWID
#define MTC_RID_COLUMN_ID ID_USHORT_MAX

// PROJ-1861 REGEXP_LIKE

#define MTC_REGEXP_APPLY_FROM_START (0x00000001)
#define MTC_REGEXP_APPLY_AT_END     (0x00000002)
#define MTC_REGEXP_NORMAL           (0x00000000)

typedef enum
{
    R_PC_IDX = 0,   // error
    R_DOLLAR_IDX,   // '$'
    R_CARET_IDX,    // '^'
    R_BSLASH_IDX,   // '\\'
    R_PLUS_IDX,     // '+'
    R_MINUS_IDX,    // '-'
    R_STAR_IDX,     // '*'
    R_QMARK_IDX,    // '?' 
    R_COMMA_IDX,    // ','
    R_PRIOD_IDX,    // '.'
    R_VBAR_IDX,     // '|'
    R_LPAREN_IDX,   // '('
    R_RPAREN_IDX,   // ')'
    R_LCBRACK_IDX,  // '{'
    R_RCBRACK_IDX,  // '}'
    R_LBRACKET_IDX, // '['
    R_RBRACKET_IDX, // '],
    R_ALNUM_IDX,    // :alnum:
    R_ALPHA_IDX,    // :alpha:
    R_BLANK_IDX,    // :blank:
    R_CNTRL_IDX,    // :cntrl:
    R_DIGIT_IDX,    // :digit:
    R_GRAPH_IDX,    // :graph:
    R_LOWER_IDX,    // :lower:
    R_PRINT_IDX,    // :print:
    R_PUNCT_IDX,    // :punct:
    R_SPACE_IDX,    // :space:
    R_UPPER_IDX,    // :upper:
    R_XDIGIT_IDX,   // :xdigit:
    R_MAX_IDX
} mtcRegPatternType;

struct mtcRegPatternTokenInfo
{
    UChar            * ptr;    
    mtcRegPatternType  type;
    UInt               size;
};

struct mtcRegBlockInfo
{
    mtcRegPatternTokenInfo * start;
    mtcRegPatternTokenInfo * end;
    UInt                     minRepeat;
    UInt                     maxRepeat;    
};

struct mtcRegBracketInfo
{
    mtcRegPatternType       type;
    mtcRegPatternTokenInfo *tokenPtr;
    mtcRegBracketInfo      *bracketPair;    
};

struct mtcRegPatternInfo
{
    UInt                     tokenCount;    
    UInt                     blockCount;
    mtcRegBlockInfo        * blockInfo;
    UInt                     flag;    
    mtcRegPatternTokenInfo * patternToken;
};

/* PROJ-2180 */
typedef struct mtfQuantFuncArgData {
	mtfQuantFuncArgData * mSelf;			/* for safe check           */
	UInt                  mElements;		/* # of in_list             */
	idBool *              mIsNullList;      /* idBool[mElements]        */
	mtcStack *            mStackList;       /* mStackList[mElements]    */
} mtfQuantFuncArgData;

typedef enum {
	MTF_QUANTIFY_FUNC_UNKNOWN = 0,
	MTF_QUANTIFY_FUNC_NORMAL_CALC,
	MTF_QUANTIFY_FUNC_FAST_CALC,
} mtfQuantFuncCalcType;

#define MTC_TUPLE_EXECUTE(aTuple, aNode)      \
    ((aNode)->column != MTC_RID_COLUMN_ID) ?  \
    ((aTuple)->execute + (aNode)->column)  :  \
    ((aTuple)->ridExecute)

/* PROJ-2632 */
#define MTX_SERIAL_EXECUTE_DATA_SIZE  ( ID_SIZEOF( mtxSerialExecuteData ) )
#define MTX_SERIAL_ENTRY_SIZE         ( ID_SIZEOF( mtxEntry ) )

#define MTX_ENTRY_TYPE_NONE           ( 0x00000000 )
#define MTX_ENTRY_TYPE_VALUE          ( 0x00000001 )
#define MTX_ENTRY_TYPE_RID            ( 0x00000002 )
#define MTX_ENTRY_TYPE_COLUMN         ( 0x00000003 )
#define MTX_ENTRY_TYPE_FUNCTION       ( 0x00000004 )
#define MTX_ENTRY_TYPE_SINGLE         ( 0x00000005 )
#define MTX_ENTRY_TYPE_CONVERT        ( 0x00000006 )
#define MTX_ENTRY_TYPE_CONVERT_CHAR   ( 0x00000007 )
#define MTX_ENTRY_TYPE_CONVERT_DATE   ( 0x00000008 )
#define MTX_ENTRY_TYPE_CHECK          ( 0x00000009 )
#define MTX_ENTRY_TYPE_AND            ( 0x0000000A )
#define MTX_ENTRY_TYPE_OR             ( 0x0000000B )
#define MTX_ENTRY_TYPE_AND_SINGLE     ( 0x0000000C )
#define MTX_ENTRY_TYPE_OR_SINGLE      ( 0x0000000D )
#define MTX_ENTRY_TYPE_NOT            ( 0x0000000E )
#define MTX_ENTRY_TYPE_LNNVL          ( 0x0000000F )
#define MTX_ENTRY_TYPE_ERROR          ( 0x000000FF )

#define MTX_ENTRY_SIZE_ZERO           ( 0x00000000 )
#define MTX_ENTRY_SIZE_ONE            ( 0x00000001 )
#define MTX_ENTRY_SIZE_TWO            ( 0x00000002 )
#define MTX_ENTRY_SIZE_THREE          ( 0x00000003 )

#define MTX_ENTRY_COUNT_ZERO          ( 0x00000000 )
#define MTX_ENTRY_COUNT_ONE           ( 0x00000001 )
#define MTX_ENTRY_COUNT_TWO           ( 0x00000002 )
#define MTX_ENTRY_COUNT_THREE         ( 0x00000003 )

#define MTX_ENTRY_FLAG_INITIALIZE     ( 0x00000000 )
#define MTX_ENTRY_FLAG_CALCULATE_MASK ( 0x00000001 )
#define MTX_ENTRY_FLAG_CALCULATE      ( 0x00000001 )

typedef enum
{
    MTX_EXECUTOR_ID_DATE,
    MTX_EXECUTOR_ID_SMALLINT,
    MTX_EXECUTOR_ID_INTEGER,
    MTX_EXECUTOR_ID_BIGINT,
    MTX_EXECUTOR_ID_REAL,
    MTX_EXECUTOR_ID_DOUBLE,
    MTX_EXECUTOR_ID_FLOAT,
    MTX_EXECUTOR_ID_NUMERIC,
    MTX_EXECUTOR_ID_CHAR,
    MTX_EXECUTOR_ID_VARCHAR,
    MTX_EXECUTOR_ID_NCHAR,
    MTX_EXECUTOR_ID_NVARCHAR,
    MTX_EXECUTOR_ID_INTERVAL,
    MTX_EXECUTOR_ID_NIBBLE,
    MTX_EXECUTOR_ID_BIT,
    MTX_EXECUTOR_ID_VARBIT,
    MTX_EXECUTOR_ID_BYTE,
    MTX_EXECUTOR_ID_VARBYTE,
    MTX_EXECUTOR_ID_RID,
    MTX_EXECUTOR_ID_COLUMN,
    MTX_EXECUTOR_ID_AND,
    MTX_EXECUTOR_ID_OR,
    MTX_EXECUTOR_ID_NOT,
    MTX_EXECUTOR_ID_EQUAL,
    MTX_EXECUTOR_ID_NOT_EQUAL,
    MTX_EXECUTOR_ID_GREATER_THAN,
    MTX_EXECUTOR_ID_GREATER_EQUAL,
    MTX_EXECUTOR_ID_LESS_THAN,
    MTX_EXECUTOR_ID_LESS_EQUAL,
    MTX_EXECUTOR_ID_ADD,
    MTX_EXECUTOR_ID_SUBTRACT,
    MTX_EXECUTOR_ID_MULTIPLY,
    MTX_EXECUTOR_ID_DIVIDE,
    MTX_EXECUTOR_ID_IS_NOT_NULL,
    MTX_EXECUTOR_ID_LNNVL,
    MTX_EXECUTOR_ID_MAX
} mtxSerialExecuteId;

struct mtxModule
{
    mtxSerialExecuteId   mId;
    mtxSerialExecuteFunc mCommon;
    mtxGetSerialExecute  mGetExecute;
};

typedef struct mtxEntryHeader
{
    UShort mId;     /* 2B : Identify Entry Set*/
    UChar  mType;   /* 3B : Entry Set Type*/
    UChar  mSize;   /* 4B : Entry Size for Entry Set*/
    UChar  mCount;  /* 5B : Arugment Count*/
    UChar  mFlag;   /* 6B : Flag*/
    UChar  mRSV[2]; /* 8B */
} mtxEntryHeader;

union mtxEntry
{
    mtxEntryHeader         mHeader; 
    void                 * mAddress;
    mtxSerialExecuteFunc   mExecute;
};

#define MTX_SET_ENTRY_HEAD( _entry_, _header_ ) \
{                                               \
    ( _entry_ )->mHeader = ( _header_ );        \
    ( _entry_ )++;                              \
}

#define MTX_SET_ENTRY_ADDR( _entry_, _val_ ) \
{                                            \
    ( _entry_ )->mAddress = ( _val_ );       \
    ( _entry_ )++;                           \
}

#define MTX_SET_ENTRY_RETN( _entry_, _val_ ) \
    MTX_SET_ENTRY_ADDR( ( _entry_ ), ( _val_ ) )

#define MTX_SET_ENTRY_EXEC( _entry_, _val_ ) \
{                                            \
    ( _entry_ )->mExecute = ( _val_ );       \
    ( _entry_ )++;                           \
}

#define MTX_SET_ENTRY_FLAG( _entry_, _flag_ ) \
{                                             \
    ( _entry_ )->mHeader.mFlag |= ( _flag_ ); \
}

#define MTX_INITAILZE_ENTRY_FLAG( _entry_ ) \
{                                           \
    ( _entry_ )->mHeader.mFlag &=           \
        ~MTX_ENTRY_FLAG_CALCULATE;          \
}

#define MTX_GET_TUPLE_ADDR( _template_, _node_ )        \
    (void*)&(( _template_ )->rows[( _node_ )->table])

#define MTX_GET_COLUNM_ADDR( _template_, _node_ )             \
    (void*)&((( _template_ )->rows[( _node_ )->table].columns \
              + ( _node_ )->column)->column)

#define MTX_GET_MTC_COLUMN( _template_, _node_ )            \
    (void*)(( _template_ )->rows[( _node_ )->table].columns \
            + ( _node_ )->column)

#define MTX_GET_ROW_ADDR( _template_, _node_ )              \
    (UChar*)(( _template_ )->rows[( _node_ )->table].row)

#define MTX_GET_ROW_OFFSET( _template_, _node_ )      \
    ((( _template_ )->rows[( _node_ )->table].columns \
      + ( _node_ )->column)->column.offset)

#define MTX_GET_RETURN_ADDR( _template_, _node_ )               \
    (void*)(MTX_GET_ROW_ADDR( ( _template_ ), ( _node_ ) )      \
            + MTX_GET_ROW_OFFSET( ( _template_ ), ( _node_ )))

#define MTX_GET_SERIAL_EXECUTE_DATA_SIZE( _count_ ) \
    ( MTX_SERIAL_EXECUTE_DATA_SIZE                  \
      + ( MTX_SERIAL_ENTRY_SIZE * ( _count_ ) ) )

typedef struct mtxSerialExecuteData
{
    mtcTuple             * mTable;
    mtxEntry             * mEntry;
    UInt                   mEntryTotalSize;
    UInt                   mEntrySetCount;
} mtxSerialExecuteData;

#define MTX_SERIAL_EXEUCTE_DATA_INITIALIZE( _data_ ,                \
                                            _template_,             \
                                            _count_,                \
                                            _setcount_,             \
                                            _offset_ )              \
{                                                                   \
    ( _data_ )                  =                                   \
        (mtxSerialExecuteData*)( ( _template_ ) + ( _offset_ ) );   \
    ( _data_ )->mTable          = NULL;                             \
    ( _data_ )->mEntry          =                                   \
        (mtxEntry*)( ( _template_ ) + ( _offset_ )                  \
                     + MTX_SERIAL_EXECUTE_DATA_SIZE );              \
    ( _data_ )->mEntryTotalSize = ( _count_ );                      \
    ( _data_ )->mEntrySetCount  = ( _setcount_ );                   \
}

typedef struct mtxSerialFilterInfo
{
    mtxEntryHeader        mHeader;
    mtcNode             * mNode;
    mtxSerialExecuteFunc  mExecute;
    UInt                  mOffset;
    mtxSerialFilterInfo * mLeft;
    mtxSerialFilterInfo * mRight;
} mtxSerialFilterInfo;

#define MTX_SET_SERIAL_FILTER_INFO( _info_ ,     \
                                    _node_ ,     \
                                    _execute_ ,  \
                                    _offset_,    \
                                    _left_,      \
                                    _right_ )    \
{                                                \
    ( _info_ )->mNode    = ( _node_ );           \
    ( _info_ )->mExecute = ( _execute_ );        \
    ( _info_ )->mOffset  = ( _offset_ );         \
    ( _info_ )->mLeft    = ( _left_ );           \
    ( _info_ )->mRight   = ( _right_ );          \
}

#define MTX_SET_ENTRY_HEADER( _header_,  \
                              _id_,      \
                              _type_,    \
                              _size_,    \
                              _count_ )  \
{                                        \
    ( _header_ ).mId     = ( _id_ );     \
    ( _header_ ).mType   = ( _type_ );   \
    ( _header_ ).mSize   = ( _size_ );   \
    ( _header_ ).mCount  = ( _count_ );  \
    ( _header_ ).mFlag   =               \
        MTX_ENTRY_FLAG_INITIALIZE;       \
    ( _header_ ).mRSV[0] = 0;            \
    ( _header_ ).mRSV[1] = 0;            \
}

#define MTC_COLUMN_IS_NOT_SAME( aColumn, aColumnPtr )                       \
(                                                                           \
    ( ( ( aColumn.column.id )       != ( aColumnPtr->column.id ) ) ||       \
      ( ( aColumn.column.colSpace ) != ( aColumnPtr->column.colSpace ) ) || \
      ( ( aColumn.module )          != ( aColumnPtr->module ) ) ) ?         \
      ID_TRUE : ID_FALSE                                                    \
)

#endif /* _O_MTC_DEF_H_ */
