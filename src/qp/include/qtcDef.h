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
 * $Id: qtcDef.h 90192 2021-03-12 02:01:03Z jayce.park $
 **********************************************************************/

#include <qc.h>
#include <qmsParseTree.h>

#ifndef _O_QTC_DEF_H_
# define _O_QTC_DEF_H_ 1

/* qtcNode.node.flag                                */
/**********************************************************************
 [QTC_NODE_MASK]
 Node�� �����ϴ� ���� Node�� ������ ��
 ������ ���� ������ �����ϴ� ���� �Ǻ��� �� �ִ� Mask�̴�.
 ��, ������ ���� ������ ������ �� �ִ�.
     - MTC_NODE_MASK : �ε���, Binding, DML ����
        - MTC_NODE_INDEX_MASK : �ε����� ����� �� �ִ� Node�� ������.
        - MTC_NODE_BIND_MASK : Binding�� �ʿ��� Node�� ������.
        - MTC_NODE_DML_MASK : DML�� ������ �� ���� Node�� ������.
     - QTC_NODE_AGGREGATE_MASK : Aggregation Node�� ������.
     - QTC_NODE_AGGREGATE2_MASK : Nested Aggregation Node�� ������.
     - QTC_NODE_SUBQUERY_MASK : Subquery Node�� ������
     - QTC_NODE_SEQUNECE_MASK : Sequence Node�� ������
     - QTC_NODE_PRIOR_MASK : PRIOR Column�� ������
     - QTC_NODE_PROC_VAR_MASK : Procedure Variable�� ������.
     - QTC_NODE_JOIN_OPERATOR_MASK : (+) Outer Join Operator �� ������. PROJ-1653
     - QTC_NODE_COLUMN_RID_MASK : RID Node�� ������ (PROJ-1789)
     - QTC_NODE_SP_SYNONYM_FUNC_MASK : Synonym�� ���� Function�� ����
************************************************************************/

# define QTC_NODE_MASK  ( QTC_NODE_AGGREGATE_MASK     | \
                          QTC_NODE_AGGREGATE2_MASK    | \
                          QTC_NODE_SUBQUERY_MASK      | \
                          QTC_NODE_SEQUENCE_MASK      | \
                          QTC_NODE_PRIOR_MASK         | \
                          QTC_NODE_LEVEL_MASK         | \
                          QTC_NODE_ISLEAF_MASK        | \
                          QTC_NODE_ROWNUM_MASK        | \
                          QTC_NODE_SYSDATE_MASK       | \
                          QTC_NODE_PROC_VAR_MASK      | \
                          QTC_NODE_PROC_FUNCTION_MASK | \
                          QTC_NODE_BINARY_MASK        | \
                          QTC_NODE_TRANS_PRED_MASK    | \
                          QTC_NODE_VAR_FUNCTION_MASK  | \
                          QTC_NODE_LOB_COLUMN_MASK    | \
                          QTC_NODE_ANAL_FUNC_MASK     | \
                          QTC_NODE_JOIN_OPERATOR_MASK | \
                          QTC_NODE_COLUMN_RID_MASK    | \
                          QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK | \
                          QTC_NODE_PKG_MEMBER_MASK    | \
                          QTC_NODE_LOOP_LEVEL_MASK    | \
                          QTC_NODE_LOOP_VALUE_MASK    | \
                          QTC_NODE_SP_SYNONYM_FUNC_MASK )

/* qtcNode.flag                                */
// Aggregation�� ���� ����
# define QTC_NODE_AGGREGATE_MASK          ID_ULONG(0x0000000000000001)
# define QTC_NODE_AGGREGATE_EXIST         ID_ULONG(0x0000000000000001)
# define QTC_NODE_AGGREGATE_ABSENT        ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// Nested Aggregation�� ���� ����
# define QTC_NODE_AGGREGATE2_MASK         ID_ULONG(0x0000000000000002)
# define QTC_NODE_AGGREGATE2_EXIST        ID_ULONG(0x0000000000000002)
# define QTC_NODE_AGGREGATE2_ABSENT       ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// Subquery�� ���� ����
# define QTC_NODE_SUBQUERY_MASK           ID_ULONG(0x0000000000000004)
# define QTC_NODE_SUBQUERY_EXIST          ID_ULONG(0x0000000000000004)
# define QTC_NODE_SUBQUERY_ABSENT         ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// Sequence�� ���� ����
# define QTC_NODE_SEQUENCE_MASK           ID_ULONG(0x0000000000000008)
# define QTC_NODE_SEQUENCE_EXIST          ID_ULONG(0x0000000000000008)
# define QTC_NODE_SEQUENCE_ABSENT         ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PRIOR Pseudo Column�� ���� ����
# define QTC_NODE_PRIOR_MASK              ID_ULONG(0x0000000000000010)
# define QTC_NODE_PRIOR_EXIST             ID_ULONG(0x0000000000000010)
# define QTC_NODE_PRIOR_ABSENT            ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// LEVEL Pseudo Column�� ���� ����
# define QTC_NODE_LEVEL_MASK              ID_ULONG(0x0000000000000020)
# define QTC_NODE_LEVEL_EXIST             ID_ULONG(0x0000000000000020)
# define QTC_NODE_LEVEL_ABSENT            ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// fix BUG-10524
// SYSDATE pseudo Column�� ���� ����
# define QTC_NODE_SYSDATE_MASK            ID_ULONG(0x0000000000000040)
# define QTC_NODE_SYSDATE_EXIST           ID_ULONG(0x0000000000000040)
# define QTC_NODE_SYSDATE_ABSENT          ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// Procedure Variable �� ���� ����
# define QTC_NODE_PROC_VAR_MASK           ID_ULONG(0x0000000000000080)
# define QTC_NODE_PROC_VAR_EXIST          ID_ULONG(0x0000000000000080)
# define QTC_NODE_PROC_VAR_ABSENT         ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1364
// indexable predicate �Ǵܽ�,
// qmoPred::checkSameGroupType() �Լ�������
// ���ϰ迭�� ���� �ٸ� data type�� ���� index ��밡�������� �Ǵ��ϸ�,
// �̶�, column node�� �� flag�� �����ϰ� �ȴ�.
// ������,
// qmoKeyRange::isIndexable() �Լ�������
//  (1) host ������ binding �� ��,
//  (2) sort temp table�� ���� keyRange ������,
// ���ϰ迭�� index ��밡�������� �Ǵ��ϰ� �Ǹ�,
// �̶�, prepare �ܰ迡�� �̹� �Ǵܵ� predicate�� ����, �ߺ� �˻����� �ʱ� ����
# define QTC_NODE_CHECK_SAMEGROUP_MASK   ID_ULONG(0x0000000000000100)
# define QTC_NODE_CHECK_SAMEGROUP_TRUE   ID_ULONG(0x0000000000000100)
# define QTC_NODE_CHECK_SAMEGROUP_FALSE  ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// To Fix PR-11391
// Internal Procedure Variable�� table�� column����
// procedure�� variable���� üũ�� �ʿ䰡 ����
#define QTC_NODE_INTERNAL_PROC_VAR_MASK   ID_ULONG(0x0000000000000200)
#define QTC_NODE_INTERNAL_PROC_VAR_EXIST  ID_ULONG(0x0000000000000200)
#define QTC_NODE_INTERNAL_PROC_VAR_ABSENT ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// to Fix BUG-12934
// constant filter�� ���ؼ���
// subquery�� store and search ����ȭ���� �������� �ʴ´�.
// ��) ? in subquery, 1 in subquery
#define QTC_NODE_CONSTANT_FILTER_MASK     ID_ULONG(0x0000000000000400)
#define QTC_NODE_CONSTANT_FILTER_TRUE     ID_ULONG(0x0000000000000400)
#define QTC_NODE_CONSTANT_FILTER_FALSE    ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// To Fix PR-12743
// NNF Filter ���θ� ����
#define QTC_NODE_NNF_FILTER_MASK          ID_ULONG(0x0000000000000800)
#define QTC_NODE_NNF_FILTER_TRUE          ID_ULONG(0x0000000000000800)
#define QTC_NODE_NNF_FILTER_FALSE         ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// To Fix BUG-13939
// keyRange ������, In subquery or subquery keyRange�� ���,
// subquery�� ������ �� �ֵ��� �ϱ� ����
// ��庯ȯ��, subquery node�� ����� �񱳿����ڳ�忡 �� ������ ����
#define QTC_NODE_SUBQUERY_RANGE_MASK      ID_ULONG(0x0000000000001000)
#define QTC_NODE_SUBQUERY_RANGE_TRUE      ID_ULONG(0x0000000000001000)
#define QTC_NODE_SUBQUERY_RANGE_FALSE     ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1075 PSM������ OUTBINDING ����
#define QTC_NODE_OUTBINDING_MASK          ID_ULONG(0x0000000000002000)
#define QTC_NODE_OUTBINDING_ENABLE        ID_ULONG(0x0000000000002000)
#define QTC_NODE_OUTBINDING_DISABLE       ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1075 PSM������ LVALUE ����
#define QTC_NODE_LVALUE_MASK              ID_ULONG(0x0000000000004000)
#define QTC_NODE_LVALUE_ENABLE            ID_ULONG(0x0000000000004000)
#define QTC_NODE_LVALUE_DISABLE           ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// fix BUG-14257
// node�� function������ ����
#define QTC_NODE_PROC_FUNCTION_MASK       ID_ULONG(0x0000000000008000)
#define QTC_NODE_PROC_FUNCTION_TRUE       ID_ULONG(0x0000000000008000)
#define QTC_NODE_PROC_FUNCTION_FALSE      ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// BUG-16000
// Equal������ �Ұ����� TYPE������ ����
// (Lob or Binary Type�� ���)
#define QTC_NODE_BINARY_MASK              ID_ULONG(0x0000000000010000)
#define QTC_NODE_BINARY_EXIST             ID_ULONG(0x0000000000010000)
#define QTC_NODE_BINARY_ABSENT            ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// qtcColumn�� estimate ���� ����.
#define QTC_NODE_COLUMN_ESTIMATE_MASK     ID_ULONG(0x0000000000020000)
#define QTC_NODE_COLUMN_ESTIMATE_TRUE     ID_ULONG(0x0000000000020000)
#define QTC_NODE_COLUMN_ESTIMATE_FALSE    ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// procedure variable�� estimate�Ǿ����� ����
#define QTC_NODE_PROC_VAR_ESTIMATE_MASK   ID_ULONG(0x0000000000040000)
#define QTC_NODE_PROC_VAR_ESTIMATE_TRUE   ID_ULONG(0x0000000000040000)
#define QTC_NODE_PROC_VAR_ESTIMATE_FALSE  ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// ROWNUM Pseudo Column�� ���� ����
# define QTC_NODE_ROWNUM_MASK             ID_ULONG(0x0000000000080000)
# define QTC_NODE_ROWNUM_EXIST            ID_ULONG(0x0000000000080000)
# define QTC_NODE_ROWNUM_ABSENT           ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// ��� ��ȯ�� �߻��ߴ��� ����
#define QTC_NODE_CONVERSION_MASK          ID_ULONG(0x0000000000100000)
#define QTC_NODE_CONVERSION_TRUE          ID_ULONG(0x0000000000100000)
#define QTC_NODE_CONVERSION_FALSE         ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1404
// transformation���� ������ transitive predicate���� ����
# define QTC_NODE_TRANS_PRED_MASK         ID_ULONG(0x0000000000200000)
# define QTC_NODE_TRANS_PRED_EXIST        ID_ULONG(0x0000000000200000)
# define QTC_NODE_TRANS_PRED_ABSENT       ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1404
// random, sendmsg ���� variable build-in �Լ��� ���Ǿ����� ����
# define QTC_NODE_VAR_FUNCTION_MASK       ID_ULONG(0x0000000000400000)
# define QTC_NODE_VAR_FUNCTION_EXIST      ID_ULONG(0x0000000000400000)
# define QTC_NODE_VAR_FUNCTION_ABSENT     ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1413
// view merging�� ���� ��ȯ�� ������� ����
# define QTC_NODE_MERGED_COLUMN_MASK      ID_ULONG(0x0000000000800000)
# define QTC_NODE_MERGED_COLUMN_TRUE      ID_ULONG(0x0000000000800000)
# define QTC_NODE_MERGED_COLUMN_FALSE     ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// PROJ-1762
// analytic function�� ���ϴ� node���� ���� 
# define QTC_NODE_ANAL_FUNC_COLUMN_MASK   ID_ULONG(0x0000000001000000)
# define QTC_NODE_ANAL_FUNC_COLUMN_FALSE  ID_ULONG(0x0000000000000000)
# define QTC_NODE_ANAL_FUNC_COLUMN_TRUE   ID_ULONG(0x0000000001000000)

/* qtcNode.flag                                */
// BUG-25916
// LOB Column�� ���� ����
# define QTC_NODE_LOB_COLUMN_MASK         ID_ULONG(0x0000000002000000)
# define QTC_NODE_LOB_COLUMN_EXIST        ID_ULONG(0x0000000002000000)
# define QTC_NODE_LOB_COLUMN_ABSENT       ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// BUG-27457
// Analytic function�� ���� ����
# define QTC_NODE_ANAL_FUNC_MASK          ID_ULONG(0x0000000004000000)
# define QTC_NODE_ANAL_FUNC_EXIST         ID_ULONG(0x0000000004000000)
# define QTC_NODE_ANAL_FUNC_ABSENT        ID_ULONG(0x0000000000000000)

/* qtcNode.flag                                */
// SP_arrayIndex_variable�� ���� ����
# define QTC_NODE_SP_ARRAY_INDEX_VAR_MASK    ID_ULONG(0x0000000008000000)
# define QTC_NODE_SP_ARRAY_INDEX_VAR_EXIST   ID_ULONG(0x0000000008000000)
# define QTC_NODE_SP_ARRAY_INDEX_VAR_ABSENT  ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2586 PSM Parameters and return without precision
 * PSM ��ü�� parameter node �Ǵ� return node�� ��Ÿ����.
 */
#define QTC_NODE_SP_PARAM_OR_RETURN_MASK                  ID_ULONG(0x0000000010000000)
#define QTC_NODE_SP_PARAM_OR_RETURN_TRUE                  ID_ULONG(0x0000000010000000)
#define QTC_NODE_SP_PARAM_OR_RETURN_FALSE                 ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2586 PSM Parameters and return without precision
 * PSM ��ü�� parameter�� precision�� ǥ�⿩�θ� ��Ÿ����.
 *     ex) char    => QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT
 *         char(3) => QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_EXIST
 */
#define QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_MASK        ID_ULONG(0x0000000020000000)
#define QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_ABSENT      ID_ULONG(0x0000000020000000)
#define QTC_NODE_SP_PARAM_OR_RETURN_PRECISION_EXIST       ID_ULONG(0x0000000000000000)

// qtcNode.flag
// PROJ-1653 Outer Join Operator (+)
# define QTC_NODE_JOIN_OPERATOR_MASK      ID_ULONG(0x0000000040000000)
# define QTC_NODE_JOIN_OPERATOR_EXIST     ID_ULONG(0x0000000040000000)
# define QTC_NODE_JOIN_OPERATOR_ABSENT    ID_ULONG(0x0000000000000000)

/* PROJ-1715 CONNECT_BY_ISLEAF Pseudo Column�� ���� ���� */
# define QTC_NODE_ISLEAF_MASK             ID_ULONG(0x0000000080000000)
# define QTC_NODE_ISLEAF_EXIST            ID_ULONG(0x0000000080000000)
# define QTC_NODE_ISLEAF_ABSENT           ID_ULONG(0x0000000000000000)

// qtcNode.flag
// PROJ-1789 PROIWD
#define QTC_NODE_COLUMN_RID_MASK          ID_ULONG(0x0000000100000000)
#define QTC_NODE_COLUMN_RID_EXIST         ID_ULONG(0x0000000100000000)
#define QTC_NODE_COLUMN_RID_ABSENT        ID_ULONG(0x0000000000000000)

// qtcNode.flag
// PROJ-1090 Function-based Index
// deterministic user defined function���� ����
#define QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK  ID_ULONG(0x0000000200000000)
#define QTC_NODE_PROC_FUNC_DETERMINISTIC_FALSE ID_ULONG(0x0000000200000000)
#define QTC_NODE_PROC_FUNC_DETERMINISTIC_TRUE  ID_ULONG(0x0000000000000000)

/* BUG-48336 */
#define QTC_NODE_TRANS_IN_TO_EXISTS_MASK       ID_ULONG(0x0000000400000000)
#define QTC_NODE_TRANS_IN_TO_EXISTS_TRUE       ID_ULONG(0x0000000400000000)
#define QTC_NODE_TRANS_IN_TO_EXISTS_FALSE      ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * BUG-35674
 */
#define QTC_NODE_AGGR_ESTIMATE_MASK             ID_ULONG(0x0000000800000000)
#define QTC_NODE_AGGR_ESTIMATE_TRUE             ID_ULONG(0x0000000800000000)
#define QTC_NODE_AGGR_ESTIMATE_FALSE            ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2197 PSM Renewal
 */
#define QTC_NODE_COLUMN_CONVERT_MASK            ID_ULONG(0x0000001000000000)
#define QTC_NODE_COLUMN_CONVERT_TRUE            ID_ULONG(0x0000001000000000)
#define QTC_NODE_COLUMN_CONVERT_FALSE           ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-1718 Subquery unnesting
 */
#define QTC_NODE_JOIN_TYPE_MASK                 ID_ULONG(0x0000006000000000)
#define QTC_NODE_JOIN_TYPE_INNER                ID_ULONG(0x0000000000000000)
#define QTC_NODE_JOIN_TYPE_SEMI                 ID_ULONG(0x0000002000000000)
#define QTC_NODE_JOIN_TYPE_ANTI                 ID_ULONG(0x0000004000000000)
#define QTC_NODE_JOIN_TYPE_UNDEFINED            ID_ULONG(0x0000006000000000)

/* qtcNode.lflag
 * BUG-37253
 */
#define QTC_NODE_PKG_VARIABLE_MASK              ID_ULONG(0x0000010000000000)
#define QTC_NODE_PKG_VARIABLE_TRUE              ID_ULONG(0x0000010000000000)
#define QTC_NODE_PKG_VARIABLE_FALSE             ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2219 Row-level before update trigger
 */
#define QTC_NODE_VALIDATE_MASK                  ID_ULONG(0x0000020000000000)
#define QTC_NODE_VALIDATE_TRUE                  ID_ULONG(0x0000020000000000)
#define QTC_NODE_VALIDATE_FALSE                 ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2415 Grouping Sets Clause
 */
#define QTC_NODE_EMPTY_GROUP_MASK               ID_ULONG(0x0000040000000000)
#define QTC_NODE_EMPTY_GROUP_TRUE               ID_ULONG(0x0000040000000000)
#define QTC_NODE_EMPTY_GROUP_FALSE              ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2415 Grouping Sets Clause
 * Grouping Sets Transform���� Target�� �߰��� OrderBy�� Node�� ��Ÿ����. 
 */
#define QTC_NODE_GBGS_ORDER_BY_NODE_MASK        ID_ULONG(0x0000080000000000)
#define QTC_NODE_GBGS_ORDER_BY_NODE_TRUE        ID_ULONG(0x0000080000000000)
#define QTC_NODE_GBGS_ORDER_BY_NODE_FALSE       ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * PROJ-2210 Autoincrement column
 */
#define QTC_NODE_DEFAULT_VALUE_MASK             ID_ULONG(0x0000100000000000)
#define QTC_NODE_DEFAULT_VALUE_TRUE             ID_ULONG(0x0000100000000000)
#define QTC_NODE_DEFAULT_VALUE_FALSE            ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * BUG-39770
 * package�� ���Ե� ����, subprogram�� ���� Node�� ��Ÿ����.
 */
#define QTC_NODE_PKG_MEMBER_MASK                ID_ULONG(0x0000200000000000)
#define QTC_NODE_PKG_MEMBER_EXIST               ID_ULONG(0x0000200000000000)
#define QTC_NODE_PKG_MEMBER_ABSENT              ID_ULONG(0x0000000000000000)

// BUG-41311
// loop_level pseudo column
#define QTC_NODE_LOOP_LEVEL_MASK                ID_ULONG(0x0000400000000000)
#define QTC_NODE_LOOP_LEVEL_EXIST               ID_ULONG(0x0000400000000000)
#define QTC_NODE_LOOP_LEVEL_ABSENT              ID_ULONG(0x0000000000000000)

// loop_value pseudo column
#define QTC_NODE_LOOP_VALUE_MASK                ID_ULONG(0x0000800000000000)
#define QTC_NODE_LOOP_VALUE_EXIST               ID_ULONG(0x0000800000000000)
#define QTC_NODE_LOOP_VALUE_ABSENT              ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * BUG-41243
 * PSM Named-based Argument Passing����, Parameter �̸��� ������ Node�� ��Ÿ����.
 */
#define QTC_NODE_SP_PARAM_NAME_MASK             ID_ULONG(0x0001000000000000)
#define QTC_NODE_SP_PARAM_NAME_TRUE             ID_ULONG(0x0001000000000000)
#define QTC_NODE_SP_PARAM_NAME_FALSE            ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * BUG-41228
 * PSM parameter�� default�� ���� validation ���� ������ ��Ÿ����..
 */
#define QTC_NODE_SP_PARAM_DEFAULT_VALUE_MASK    ID_ULONG(0x0002000000000000)
#define QTC_NODE_SP_PARAM_DEFAULT_VALUE_TRUE    ID_ULONG(0x0002000000000000)
#define QTC_NODE_SP_PARAM_DEFAULT_VALUE_FALSE   ID_ULONG(0x0000000000000000)

/* qtcNode.lflag
 * BUG-36728 Check Constraint, Function-Based Index���� Synonym�� ����� �� ����� �մϴ�.
 */
#define QTC_NODE_SP_SYNONYM_FUNC_MASK           ID_ULONG(0x0004000000000000)
#define QTC_NODE_SP_SYNONYM_FUNC_TRUE           ID_ULONG(0x0004000000000000)
#define QTC_NODE_SP_SYNONYM_FUNC_FALSE          ID_ULONG(0x0000000000000000)

// BUG-44518 order by ������ ESTIMATE �ߺ� �����ϸ� �ȵ˴ϴ�.
#define QTC_NODE_ORDER_BY_ESTIMATE_MASK         ID_ULONG(0x0008000000000000)
#define QTC_NODE_ORDER_BY_ESTIMATE_TRUE         ID_ULONG(0x0008000000000000)
#define QTC_NODE_ORDER_BY_ESTIMATE_FALSE        ID_ULONG(0x0000000000000000)

// BUG-45172 semi ������ ������ ������ �˻��Ͽ� flag�� ������ �д�.
// ������ ���������� semi ������ ��� flag �� ���� ���� semi ������ ������
#define QTC_NODE_REMOVABLE_SEMI_JOIN_MASK       ID_ULONG(0x0010000000000000)
#define QTC_NODE_REMOVABLE_SEMI_JOIN_TRUE       ID_ULONG(0x0010000000000000)
#define QTC_NODE_REMOVABLE_SEMI_JOIN_FALSE      ID_ULONG(0x0000000000000000)

/* PROJ-2687 Shard aggregation transform */
#define QTC_NODE_SHARD_VIEW_TARGET_REF_MASK     ID_ULONG(0x0020000000000000)
#define QTC_NODE_SHARD_VIEW_TARGET_REF_TRUE     ID_ULONG(0x0020000000000000)
#define QTC_NODE_SHARD_VIEW_TARGET_REF_FALSE    ID_ULONG(0x0000000000000000)

/* BUG-46032 */
#define QTC_NODE_SP_NESTED_ARRAY_MASK          ID_ULONG(0x0040000000000000)
#define QTC_NODE_SP_NESTED_ARRAY_TRUE          ID_ULONG(0x0040000000000000)
#define QTC_NODE_SP_NESTED_ARRAY_FALSE         ID_ULONG(0x0000000000000000)

/* BUG-46174 */
#define QTC_NODE_SP_INS_UPT_VALUE_REC_MASK     ID_ULONG(0x0080000000000000)
#define QTC_NODE_SP_INS_UPT_VALUE_REC_TRUE     ID_ULONG(0x0080000000000000)
#define QTC_NODE_SP_INS_UPT_VALUE_REC_FALSE    ID_ULONG(0x0000000000000000)

// /* TASK-7219 Non-shard DML */
#define QTC_NODE_OUT_REF_COLUMN_MASK           ID_ULONG(0x0100000000000000)
#define QTC_NODE_OUT_REF_COLUMN_TRUE           ID_ULONG(0x0100000000000000)
#define QTC_NODE_OUT_REF_COLUMN_FALSE          ID_ULONG(0x0000000000000000)

# define QTC_HAVE_AGGREGATE( aNode ) \
    ( ( (aNode)->lflag & QTC_NODE_AGGREGATE_MASK )           \
            == QTC_NODE_AGGREGATE_EXIST ? ID_TRUE : ID_FALSE )

# define QTC_HAVE_AGGREGATE2( aNode ) \
    ( ( aNode->lflag & QTC_NODE_AGGREGATE2_MASK )           \
            == QTC_NODE_AGGREGATE2_EXIST ? ID_TRUE : ID_FALSE )

# define QTC_IS_AGGREGATE( aNode ) \
    ( ( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK )           \
            == MTC_NODE_OPERATOR_AGGREGATION ? ID_TRUE : ID_FALSE )

# define QTC_HAVE_SUBQUERY( aNode ) \
    ( ( (aNode)->lflag & QTC_NODE_SUBQUERY_MASK )           \
            == QTC_NODE_SUBQUERY_EXIST ? ID_TRUE : ID_FALSE )

# define QTC_IS_SUBQUERY( aNode ) \
    ( ( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK )           \
            == MTC_NODE_OPERATOR_SUBQUERY ? ID_TRUE : ID_FALSE )

# define QTC_INDEXABLE_COLUMN( aNode ) \
    ( ( ( (aNode)->node.lflag & MTC_NODE_INDEX_MASK ) == MTC_NODE_INDEX_USABLE ) \
      ? ID_TRUE : ID_FALSE )

// BUG-16730
// Subquery�� target count�� 2�̻��̸� List��
# define QTC_IS_LIST( aNode ) \
    ( ( ( (aNode)->node.module == &mtfList ) || \
        ( ( (aNode)->node.module == &qtc::subqueryModule ) && \
          ( ((aNode)->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK) > 1 ) ) ) \
      ? ID_TRUE : ID_FALSE )

// BUG-17506
# define QTC_IS_VARIABLE( aNode )                               \
    (                                                           \
       (  ( ( (aNode)->lflag & QTC_NODE_SUBQUERY_MASK )         \
            == QTC_NODE_SUBQUERY_EXIST )                        \
          ||                                                    \
          ( ( (aNode)->lflag & QTC_NODE_PRIOR_MASK )            \
            == QTC_NODE_PRIOR_EXIST )                           \
          ||                                                    \
          ( ( (aNode)->lflag & QTC_NODE_LEVEL_MASK )            \
            == QTC_NODE_LEVEL_EXIST )                           \
          ||                                                    \
          ( ( (aNode)->lflag & QTC_NODE_ROWNUM_MASK )           \
            == QTC_NODE_ROWNUM_EXIST )                          \
          ||                                                    \
          ( ( (aNode)->lflag & QTC_NODE_SYSDATE_MASK )          \
            == QTC_NODE_SYSDATE_EXIST )                         \
          ||                                                    \
          ( ( (aNode)->lflag & QTC_NODE_PROC_VAR_MASK )         \
            == QTC_NODE_PROC_VAR_EXIST )                        \
          ||                                                    \
           ( ( (aNode)->node.lflag & MTC_NODE_BIND_MASK )       \
            == MTC_NODE_BIND_EXIST )                            \
          ||                                                    \
           ( ( ( (aNode)->lflag & QTC_NODE_PROC_FUNCTION_MASK )           \
               == QTC_NODE_PROC_FUNCTION_TRUE ) &&                        \
             ( ( (aNode)->lflag & QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK ) \
               == QTC_NODE_PROC_FUNC_DETERMINISTIC_FALSE ) )              \
          ||                                                    \
           ( ( (aNode)->lflag & QTC_NODE_VAR_FUNCTION_MASK )    \
            == QTC_NODE_VAR_FUNCTION_EXIST )                    \
          ||                                                    \
           ( ( (aNode)->lflag & QTC_NODE_ISLEAF_MASK )          \
            == QTC_NODE_ISLEAF_EXIST )                          \
          ||                                                    \
           ( ( (aNode)->lflag & QTC_NODE_LOOP_LEVEL_MASK )      \
            == QTC_NODE_LOOP_LEVEL_EXIST )                      \
          ||                                                    \
           ( ( (aNode)->lflag & QTC_NODE_LOOP_VALUE_MASK )      \
            == QTC_NODE_LOOP_VALUE_EXIST )                      \
       )                                                        \
       ? ID_TRUE : ID_FALSE                                     \
    )

// BUG-17506
#define QTC_IS_DYNAMIC_CONSTANT( aNode )                       \
    (                                                          \
       (                                                       \
          (  ( ( (aNode)->lflag & QTC_NODE_SYSDATE_MASK )      \
               == QTC_NODE_SYSDATE_EXIST )                     \
             ||                                                \
             ( ( (aNode)->lflag & QTC_NODE_PROC_VAR_MASK )     \
               == QTC_NODE_PROC_VAR_EXIST )                    \
             ||                                                \
              ( ( (aNode)->node.lflag & MTC_NODE_BIND_MASK )   \
               == MTC_NODE_BIND_EXIST )                        \
          )                                                    \
          &&                                                   \
          (  ( ( (aNode)->lflag & QTC_NODE_PRIOR_MASK )        \
               == QTC_NODE_PRIOR_ABSENT )                      \
          )                                                    \
          &&                                                   \
          (  ( ( (aNode)->lflag & QTC_NODE_LEVEL_MASK )        \
               == QTC_NODE_LEVEL_ABSENT )                      \
          )                                                    \
          &&                                                   \
          (  ( ( (aNode)->lflag & QTC_NODE_ROWNUM_MASK )       \
               == QTC_NODE_ROWNUM_ABSENT )                     \
          )                                                    \
       )                                                       \
       ? ID_TRUE : ID_FALSE                                    \
    )

// BUG-17949
# define QTC_IS_PSEUDO( aNode )                                \
    (                                                          \
       (  ( ( (aNode)->lflag & QTC_NODE_LEVEL_MASK )           \
            == QTC_NODE_LEVEL_EXIST )                          \
          ||                                                   \
          ( ( (aNode)->lflag & QTC_NODE_ROWNUM_MASK )          \
            == QTC_NODE_ROWNUM_EXIST )                         \
          ||                                                   \
          ( ( (aNode)->lflag & QTC_NODE_ISLEAF_MASK )          \
            == QTC_NODE_ISLEAF_EXIST )                         \
       )                                                       \
       ? ID_TRUE : ID_FALSE                                    \
    )


/******************************************************************
 [Column ������ �Ǵ�]
 ���� A3������ �Ʒ��� ���� �Ǵ��Ͽ�����,
 �̴� ���� Table���� Column������ �Ǵ��ϴ� ��Ȯ��
 ������ ���� �ʴ´�.
 �̷��� ó���� Predicate�� Index��� ���� ���θ�
 ���� ���� �Ǵ��� �� �ִ� ������ ������, Conversion��
 �߻��ϴ� Column�鿡 ���Ͽ� Column������ �Ǵ����� ���ϰ�
 �־� �پ��� ����ȭ Tip ���뿡 ������ �ǰ� �ִ�.(Ex, PR-7960)
 ����, ���� Column���� �ƴ������� �Ǵ��� �� �ֵ���
 �ش� Macro�� �����Ѵ�.  ��, FROM���� �����ϴ� Table��
 Column������ ���� �Ǵ��Ѵ�.
******************************************************************/

/******************************************************************
# define QTC_IS_COLUMN( aNode )                                 \
    ( ( ( aNode->node.lflag & MTC_NODE_INDEX_MASK )             \
        == MTC_NODE_INDEX_USABLE )                              \
      && aNode->node.arguments == NULL ? ID_TRUE : ID_FALSE )
******************************************************************/
# define QTC_IS_COLUMN( aStatement, aNode ) \
    ( QC_SHARED_TMPLATE(aStatement)->tableMap[(aNode)->node.table].from \
      != NULL ? ID_TRUE : ID_FALSE )

# define QTC_TEMPLATE_IS_COLUMN( aTemplate, aNode ) \
    ( (aTemplate)->tableMap[(aNode)->node.table].from   \
      != NULL ? ID_TRUE : ID_FALSE )

# define QTC_IS_TERMINAL( aNode ) \
    ( (aNode)->node.arguments == NULL ? ID_TRUE : ID_FALSE )

# define QTC_COMPOSITE_EQUAL_CLASSIFIED( aNode ) \
    ( \
      (( (aNode)->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) \
       == MTC_NODE_GROUP_COMPARISON_FALSE) \
     && \
     ( \
      (( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) \
       == MTC_NODE_OPERATOR_EQUAL) \
       )\
    )

# define QTC_COMPOSITE_INEQUAL_EQUAL_CLASSIFIED( aNode ) \
    ( \
      (( (aNode)->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) \
       == MTC_NODE_GROUP_COMPARISON_FALSE) \
     && \
     ( \
      (( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) \
       == MTC_NODE_OPERATOR_GREATER_EQUAL) \
      || \
      (( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) \
       == MTC_NODE_OPERATOR_LESS_EQUAL) \
      ) \
     )

# define QTC_COMPOSITE_INEQUAL_CLASSIFIED( aNode ) \
    ( \
      (( (aNode)->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) \
       == MTC_NODE_GROUP_COMPARISON_FALSE) \
     && \
     ( \
      (( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) \
       == MTC_NODE_OPERATOR_GREATER ) \
      || \
      (( (aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) \
       == MTC_NODE_OPERATOR_LESS ) \
      ) \
     )

// dependencies constants
# define QTC_DEPENDENCIES_END      (-1)

/******************************************************************
 macros for readability of source code.
 naming convension is shown below.
******************************************************************/
#define QTC_NODE_TABLE(_QtcNode_)  ( (_QtcNode_)->node.table )
#define QTC_NODE_COLUMN(_QtcNode_) ( (_QtcNode_)->node.column )
#define QTC_NODE_TMPLATE(_QtcNode_) ( (_QtcNode_)->node.tmplate)

/***********************************************************************
 QTC_XXXX_YYYY : get YYYY contained in XXXX pointed by qtcNode.table,column
                 argument 1 ==> (XXXX *)
                 argument 2 ==> (qtcNode *)
                 return     ==> (YYYY *)
***********************************************************************/
// return mtcTuple *
#define QTC_TMPL_TUPLE( _QcTmpl_, _QtcNode_ )   \
    (  ( (_QcTmpl_)-> tmplate. rows ) +         \
       ( (_QtcNode_)-> node.table )  )

// return mtcTuple *
#define QTC_STMT_TUPLE( _QcStmt_, _QtcNode_ )           \
    (  ( QC_SHARED_TMPLATE(_QcStmt_)-> tmplate. rows) +        \
       ( (_QtcNode_)-> node.table )  )

// return mtcColumn *
#define QTC_TUPLE_COLUMN(_MtcTuple_, _QtcNode_)           \
    (((_QtcNode_)->node.column != MTC_RID_COLUMN_ID)  ?   \
    ((_MtcTuple_)->columns)+(_QtcNode_)->node.column  :   \
    (&gQtcRidColumn))

// if you are accessing columns more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_COLUMN.
// return mtcColumn *
#define QTC_TMPL_COLUMN( _QcTmpl_, _QtcNode_ )                          \
    ( QTC_TUPLE_COLUMN( QTC_TMPL_TUPLE( (_QcTmpl_), (_QtcNode_) ),      \
                        (_QtcNode_) ) )
// if you are accessing columns more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_COLUMN.
// return mtcColumn *
#define QTC_STMT_COLUMN( _QcStmt_, _QtcNode_ )                          \
    ( QTC_TUPLE_COLUMN( QTC_STMT_TUPLE( (_QcStmt_), (_QtcNode_) ),      \
                        (_QtcNode_) ) )

// return mtcExecute *
#define QTC_TUPLE_EXECUTE( _MtcTuple_, _QtcNode_ )       \
    (((_QtcNode_)->node.column != MTC_RID_COLUMN_ID)   ? \
    ((_MtcTuple_)->execute) + (_QtcNode_)->node.column : \
    (&gQtcRidExecute))

// if you are accessing execute pointers more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_EXECUTE.
// return mtcExecute *
#define QTC_TMPL_EXECUTE( _QcTmpl_, _QtcNode_ )                         \
    ( QTC_TUPLE_EXECUTE( QTC_TMPL_TUPLE( (_QcTmpl_), (_QtcNode_) ),     \
                         (_QtcNode_) ) )

// if you are accessing execute pointers more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_EXECUTE.
// return mtcExecute *
#define QTC_STMT_EXECUTE( _QcStmt_, _QtcNode_ )                         \
    ( QTC_TUPLE_EXECUTE( QTC_STMT_TUPLE( (_QcStmt_), (_QtcNode_) ),     \
                         (_QtcNode_) ) )


#define QTC_TUPLE_FIXEDDATA( _MtcTuple_, _QtcNode_ )            \
    ( ( (SChar*) (_MtcTuple_)-> row ) +                         \
      QTC_TUPLE_COLUMN(_MtcTuple_, _QtcNode_)->column.offset )

// if you are accessing execute pointers more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_EXECUTE.
// return SChar *
#define QTC_TMPL_FIXEDDATA( _QcTmpl_, _QtcNode_ )                       \
    ( QTC_TUPLE_FIXEDDATA( QTC_TMPL_TUPLE( (_QcTmpl_), (_QtcNode_) ),   \
                           (_QtcNode_) ) )

// if you are accessing execute pointers more than once in a function,
// use QTC_XXXX_TUPLE and QTC_TUPLE_EXECUTE.
// return SChar *
#define QTC_STMT_FIXEDDATA( _QcStmt_, _QtcNode_ )                       \
    ( QTC_TUPLE_FIXEDDATA( QTC_STMT_TUPLE( (_QcStmt_), (_QtcNode_) ),   \
                           (_QtcNode_) ) )


// operation code to qtc::makeProcVariable
#define QTC_PROC_VAR_OP_NONE           (0x00000000)
#define QTC_PROC_VAR_OP_NEXT_COLUMN    (0x00000001)
#define QTC_PROC_VAR_OP_SKIP_MODULE    (0x00000002)

/******************************************************************
 Subquery Wrapper Node, Constant Wrapper Node����
 ���� ���� ���θ� �Ǵ��ϱ� ���� mask�̴�.
 mtcTemplate.execInfo �� ������ �� ���� Setting�Ѵ�.
******************************************************************/

#define QTC_WRAPPER_NODE_EXECUTE_MASK          (0x11)
#define QTC_WRAPPER_NODE_EXECUTE_FALSE         (0x00)
#define QTC_WRAPPER_NODE_EXECUTE_TRUE          (0x01)

/***********************************************************************
   structures.
***********************************************************************/

/******************************************************************
 [qtcNode]

   - indexArgument :
       - used in comparison predicate( for detecting indexing column )
       - expand function for ( qmsTarget, outer reference column flag )
       - expand function for dummyAggs
                aggs1 aggregation used by aggs2. ( distinguish from pure aggs1  )
******************************************************************/

struct qtcOver;
struct qtcWithinGroup;

typedef struct qtcNode {
    mtcNode            node;
    ULong              lflag;
    qcStatement*       subquery;
    UInt               indexArgument;
    UInt               dependency;
    qcDepInfo          depInfo;
    qcNamePosition     position;
    qcNamePosition     userName;
    qcNamePosition     tableName;
    qcNamePosition     columnName;
    qcNamePosition     pkgName;             // PROJ-1073 Package
    qtcOver          * overClause;          // PROJ-1762 : for analytic function
    UShort             shardViewTargetPos;  // PROJ-2687 Shard aggregation transform
} qtcNode;


#define QTC_NODE_INIT(_node_)                  \
{                                              \
    MTC_NODE_INIT( & (_node_)->node );         \
    (_node_)->lflag         = 0;               \
    (_node_)->subquery      = NULL;            \
    (_node_)->indexArgument = 0;               \
    (_node_)->dependency    = 0;               \
    qtc::dependencyClear( &(_node_)->depInfo );\
    SET_EMPTY_POSITION((_node_)->position);    \
    SET_EMPTY_POSITION((_node_)->userName);    \
    SET_EMPTY_POSITION((_node_)->tableName);   \
    SET_EMPTY_POSITION((_node_)->columnName);  \
    SET_EMPTY_POSITION((_node_)->pkgName);     \
    (_node_)->overClause    = NULL;            \
    (_node_)->shardViewTargetPos = ID_USHORT_MAX; \
}

//---------------------------------
// PROJ-1762 : Analytic Function 
//---------------------------------

// for qtcOverColumn.flag
// BUG-33663 Ranking Function
#define QTC_OVER_COLUMN_MASK                    (0x00000001)
#define QTC_OVER_COLUMN_NORMAL                  (0x00000000)
#define QTC_OVER_COLUMN_ORDER_BY                (0x00000001)

// for qtcOverColumn.flag
// BUG-33663 Ranking Function
#define QTC_OVER_COLUMN_ORDER_MASK              (0x00000002)
#define QTC_OVER_COLUMN_ORDER_ASC               (0x00000000)
#define QTC_OVER_COLUMN_ORDER_DESC              (0x00000002)

/* PROJ-2435 order by nulls first/ last */
#define QTC_OVER_COLUMN_NULLS_ORDER_MASK        (0x0000000C)
#define QTC_OVER_COLUMN_NULLS_ORDER_NONE        (0x00000000)
#define QTC_OVER_COLUMN_NULLS_ORDER_FIRST       (0x00000004)
#define QTC_OVER_COLUMN_NULLS_ORDER_LAST        (0x00000008)

typedef struct qtcOverColumn
{
    qtcNode         * node;
    UInt              flag;
    
    qtcOverColumn   * next;
} qtcOverColumn;

/* PROJ-1805 Support window clause */
typedef enum qtcOverWindow
{
    QTC_OVER_WINDOW_NONE = 0,
    QTC_OVER_WINODW_ROWS,
    QTC_OVER_WINODW_RANGE
} qtcOverWindow;

typedef enum qtcOverWindowOpt
{
    QTC_OVER_WINODW_OPT_UNBOUNDED_PRECEDING = 0,
    QTC_OVER_WINODW_OPT_UNBOUNDED_FOLLOWING,
    QTC_OVER_WINODW_OPT_CURRENT_ROW,
    QTC_OVER_WINODW_OPT_N_PRECEDING,
    QTC_OVER_WINODW_OPT_N_FOLLOWING,
    QTC_OVER_WINDOW_OPT_MAX
} qtcOverWindowOpt;

typedef enum qtcOverWindowValueType
{
    QTC_OVER_WINDOW_VALUE_TYPE_NUMBER = 0,
    QTC_OVER_WINDOW_VALUE_TYPE_YEAR,
    QTC_OVER_WINDOW_VALUE_TYPE_MONTH,
    QTC_OVER_WINDOW_VALUE_TYPE_DAY,
    QTC_OVER_WINDOW_VALUE_TYPE_HOUR,
    QTC_OVER_WINDOW_VALUE_TYPE_MINUTE,
    QTC_OVER_WINDOW_VALUE_TYPE_SECOND,
    QTC_OVER_WINDOW_VALUE_TYPE_MAX
} qtcOverWindowValueType;

typedef struct qtcWindowValue
{
    SLong                  number;
    qtcOverWindowValueType type;
} qtcWindowValue;

typedef struct qtcWindowPoint
{
    qtcWindowValue   * value;
    qtcOverWindowOpt   option;
} qtcWindowPoint;

typedef struct qtcWindow
{
    qtcOverWindow     rowsOrRange;
    idBool            isBetween;
    qtcWindowPoint  * start;
    qtcWindowPoint  * end;
} qtcWindow;

//---------------------------------
// BUG-33663 Ranking Function
// rank() over (partition by i1, i2 order by i3)�� ���
// qtcOver�� �� member�� ������ ���� �����Ѵ�.
//
// overColumn ---------->i1->i2->i3->null
//                       /       /
// partitionByColumn ----       /
// orderByColumn ---------------
//
// ��� partitionByColumn�̳� oderByColumn�� �ܼ��� �� ����
// �����ϴ����� ǥ���� ���̴�. validation�̿ܿ��� ������ �ʴ´�.
//---------------------------------
typedef struct qtcOver
{
    qcNamePosition    overPosition;       // BUG-42337
    qtcOverColumn   * overColumn;         // partition by, order by column list
    
    qtcOverColumn   * partitionByColumn;  // partition by column list
    qtcOverColumn   * orderByColumn;      // order by column list
    qtcWindow       * window;
    qcNamePosition    endPos;
} qtcOver;

// PROJ-2527 WITHIN GROUP AGGR
typedef struct qtcWithinGroup
{
    qtcNode        * expression[2];
    qcNamePosition   withinPosition;    // PROJ-2533
    qcNamePosition   endPos;
} qtcWithinGroup;

// PROJ-2528 Keep Aggregation
typedef struct qtcKeepAggr
{
    qtcNode        * mExpression[2];
    qcNamePosition   mKeepPosition;
    qcNamePosition   mEndPos;
    UChar            mOption;
} qtcKeepAggr;

//---------------------------------
// Filter�� CallBack ȣ���� ���� Data
//---------------------------------
typedef struct qtcSmiCallBackData {
    mtcNode*         node;           // Node
    mtcTemplate*     tmplate;        // Template
    mtcTuple*        table;          // ������ �������� Tuple
    mtcTuple*        tuple;          // Node�� Tuple
    mtfCalculateFunc calculate;      // Filter ���� �Լ�
    void*            calculateInfo;  // Filter ���� �Լ��� ���� ����
    mtcColumn*       column;         // Node�� Column ����
    mtdIsTrueFunc    isTrue;         // TRUE �Ǵ� �Լ�
} qtcSmiCallBackData;

typedef struct qtcSmiCallBackDataAnd {
    qtcSmiCallBackData  * argu1;
    qtcSmiCallBackData  * argu2;
    qtcSmiCallBackData  * argu3;
} qtcSmiCallBackDataAnd;

/******************************************************************
 [qtcCallBackInfo]

 estimate �� ó�� ��
 mtcCallBack�� ó���� ���� ���Ǵ� �ΰ� ������
 mtcCallBack.info �� �����ȴ�.
******************************************************************/


typedef struct qtcCallBackInfo {
    qcTemplate*         tmplate;
    iduVarMemList*      memory;         // alloc�� ����� Memory Mgr
                                        //fix PROJ-1596
    qcStatement*        statement;
    struct qmsQuerySet* querySet;       // for search column in order by
    struct qmsSFWGH*    SFWGH;          // for search column
    struct qmsFrom*     from;           // for search column
} qtcCallBackInfo;

typedef struct qtcMetaRangeColumn {
    qtcMetaRangeColumn* next;
    mtdCompareFunc      compare;
    
    // PROJ-1436
    // columnDesc, valueDesc�� shared template�� column��
    // ����ؼ��� �ȵȴ�. variable type column�� ���
    // ������ ������ �߻��� �� �ִ�. ���� column ������
    // �����ؼ� ����Ѵ�.
    // ���� private template�� column ������ ����� ��
    // �ֵ��� �����ؾ� �Ѵ�.
    mtcColumn           columnDesc;
    mtcColumn           valueDesc;
    const void*         value;
    // Proj-1872 DiskIndex ���屸�� ����ȭ
    // StoredŸ���� Column�� ���ϱ� ���� Column �ε����� �����ؾ� �Ѵ�.
    UInt                columnIdx;
} qtcMetaRangeColumn;

/******************************************************************
 [qtcModule] PROJ-1075 PSM structured type ����

 row type, record type, collection type�� ó�� ��
 mtdModule�̿��� qp���� ����ϴ� �ΰ������� �̿��ϱ� ����
 ����Ѵ�.
******************************************************************/

// PROJ-1904 Extend UDT
#define QTC_UD_TYPE_HAS_ARRAY_MASK  (0x00000001)
#define QTC_UD_TYPE_HAS_ARRAY_TRUE  (0x00000001)
#define QTC_UD_TYPE_HAS_ARRAY_FALSE (0x00000000)

struct qsTypes;

typedef struct qtcModule
{
    mtdModule       module;
    qsTypes       * typeInfo;
} qtcModule;

typedef struct qtcColumnInfo
{
    UShort           table;
    UShort           column;
    smOID            objectId;         // PROJ-1073 Package
} qtcColumnInfo;

/* PROJ-2207 Password policy support */
#define QTC_SYSDATE_FOR_NATC_LEN (20)

extern IDE_RC qtcSubqueryCalculateExists( mtcNode     * aNode,
                                          mtcStack    * aStack,
                                          SInt          aRemain,
                                          void        * aInfo,
                                          mtcTemplate * aTemplate );

extern IDE_RC qtcSubqueryCalculateNotExists( mtcNode     * aNode,
                                             mtcStack    * aStack,
                                             SInt          aRemain,
                                             void        * aInfo,
                                             mtcTemplate * aTemplate );

/* PROJ-2632 */
#define QTC_ENTRY_TYPE_NONE          (MTX_ENTRY_TYPE_NONE)
#define QTC_ENTRY_TYPE_VALUE         (MTX_ENTRY_TYPE_VALUE)
#define QTC_ENTRY_TYPE_RID           (MTX_ENTRY_TYPE_RID)
#define QTC_ENTRY_TYPE_COLUMN        (MTX_ENTRY_TYPE_COLUMN)
#define QTC_ENTRY_TYPE_FUNCTION      (MTX_ENTRY_TYPE_FUNCTION)
#define QTC_ENTRY_TYPE_SINGLE        (MTX_ENTRY_TYPE_SINGLE)
#define QTC_ENTRY_TYPE_CONVERT       (MTX_ENTRY_TYPE_CONVERT)
#define QTC_ENTRY_TYPE_CONVERT_CHAR  (MTX_ENTRY_TYPE_CONVERT_CHAR)
#define QTC_ENTRY_TYPE_CONVERT_DATE  (MTX_ENTRY_TYPE_CONVERT_DATE)
#define QTC_ENTRY_TYPE_CHECK         (MTX_ENTRY_TYPE_CHECK)
#define QTC_ENTRY_TYPE_AND           (MTX_ENTRY_TYPE_AND)
#define QTC_ENTRY_TYPE_OR            (MTX_ENTRY_TYPE_OR)
#define QTC_ENTRY_TYPE_AND_SINGLE    (MTX_ENTRY_TYPE_AND_SINGLE)
#define QTC_ENTRY_TYPE_OR_SINGLE     (MTX_ENTRY_TYPE_OR_SINGLE)
#define QTC_ENTRY_TYPE_NOT           (MTX_ENTRY_TYPE_NOT)
#define QTC_ENTRY_TYPE_LNNVL         (MTX_ENTRY_TYPE_LNNVL)

#define QTC_ENTRY_SIZE_ZERO          (MTX_ENTRY_SIZE_ZERO)
#define QTC_ENTRY_SIZE_ONE           (MTX_ENTRY_SIZE_ONE)
#define QTC_ENTRY_SIZE_TWO           (MTX_ENTRY_SIZE_TWO)
#define QTC_ENTRY_SIZE_THREE         (MTX_ENTRY_SIZE_THREE)

#define QTC_ENTRY_COUNT_ZERO         (MTX_ENTRY_COUNT_ZERO)
#define QTC_ENTRY_COUNT_ONE          (MTX_ENTRY_COUNT_ONE)
#define QTC_ENTRY_COUNT_TWO          (MTX_ENTRY_COUNT_TWO)
#define QTC_ENTRY_COUNT_THREE        (MTX_ENTRY_COUNT_THREE)

#define QTC_SET_SERIAL_FILTER_INFO( _info_ ,     \
                                    _node_ ,     \
                                    _execute_ ,  \
                                    _offset_,    \
                                    _left_,      \
                                    _right_)     \
    MTX_SET_SERIAL_FILTER_INFO( ( _info_ ),      \
                                ( _node_ ),      \
                                ( _execute_ ),   \
                                ( _offset_ ),    \
                                ( _left_ ),      \
                                ( _right_ ) )

#define QTC_SET_ENTRY_HEADER( _header_,  \
                              _id_,      \
                              _type_,    \
                              _size_,    \
                              _count_ )  \
    MTX_SET_ENTRY_HEADER( ( _header_ ),  \
                          ( _id_ ),      \
                          ( _type_ ),    \
                          ( _size_ ),    \
                          ( _count_ ) )

#define QTC_SERIAL_EXEUCTE_DATA_INITIALIZE( _data_ ,     \
                                            _template_,  \
                                            _count_,     \
                                            _setcount_,  \
                                            _offset_ )   \
    MTX_SERIAL_EXEUCTE_DATA_INITIALIZE( ( _data_ ),      \
                                        ( _template_ ),  \
                                        ( _count_ ),     \
                                        ( _setcount_ ),  \
                                        ( _offset_ ) )

#define QTC_GET_SERIAL_EXECUTE_DATA_SIZE( _count_ ) \
    MTX_GET_SERIAL_EXECUTE_DATA_SIZE( ( _count_ ) )

#endif /* _O_QTC_DEF_H_ */
