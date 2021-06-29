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
 * $Id: qmoHint.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *
 *    [Hints �� ���� ����]
 *
 * ��� ���� :
 *
 * ��� :
 *
 ***********************************************************************/

#ifndef  _O_QMO_HINT_H_
#define  _O_QMO_HINT_H_ 1

//-------------------------------------------
// [Hints : Optimization ��Ģ]
//     - COST
//        : Hint�� �������� �ʴ� ��쿡��
//          Cost Based Optimization�� �����Ѵ�.
//     - RULE
//-------------------------------------------

typedef enum qmoOptGoalType
{
    QMO_OPT_GOAL_TYPE_UNKNOWN = 0,
    QMO_OPT_GOAL_TYPE_RULE,
    QMO_OPT_GOAL_TYPE_ALL_ROWS,
    QMO_OPT_GOAL_TYPE_FIRST_ROWS_N // PROJ-2242
} qmoOptGoalType;

//-------------------------------------------
// [Hints : WHERE���� Normalization Form]
//     - NNF : Hint�� �������� ���� ��쿡��
//             Not Normal Type���� Cost�� ���Ͽ�
//             CNF �Ǵ� DNF normal form�� �����Ѵ�.
//     - CNF : Conjunctive normal form���� ����ȭ
//     - DNF : Disjunctive normal form���� ����ȭ
//-------------------------------------------

typedef enum qmoNormalType
{
    QMO_NORMAL_TYPE_NOT_DEFINED = 0, // Not Defined Hint
    QMO_NORMAL_TYPE_DNF,     // Disjunctive Normal Form
    QMO_NORMAL_TYPE_CNF,     // Conjunctive Normal Form
    QMO_NORMAL_TYPE_NNF      // Not Normal Form  To Fix PR-12743
} qmosNormalType;

//-------------------------------------------
// [Hints : ���� ������ ����]
//      - TUPLESET : tuple set ó�����
//      - RECORD   : push projection�� ������ ���ڵ�������
//-------------------------------------------

typedef enum qmoMaterializeType
{
    QMO_MATERIALIZE_TYPE_NOT_DEFINED = 0,
    QMO_MATERIALIZE_TYPE_RID,
    QMO_MATERIALIZE_TYPE_VALUE 
} qmoProcesType;

//-------------------------------------------
// [Hints : Join Order Hints�� ����]
//      - OPTIMIZED : Join Order Hint�� �������� ���� ���
//      - ORDERED : Join Order Hint�� ������ ���
//-------------------------------------------

typedef enum qmoJoinOrderType
{
    QMO_JOIN_ORDER_TYPE_OPTIMIZED = 0,
    QMO_JOIN_ORDER_TYPE_ORDERED
} qmoJoinOrderType;

//-------------------------------------------
// [Hints : Subquery unnesting hint�� ����]
//      - UNDEFINED : Optimizer�� �Ǵ�(����� �׻� ����)
//      - UNNEST    : ������ ��� �׻� unnesting ����
//      - NO_UNNEST : Unnesting �������� ����
//-------------------------------------------

typedef enum qmoSubqueryUnnestType
{
    QMO_SUBQUERY_UNNEST_TYPE_NOT_DEFINED = 0,
    QMO_SUBQUERY_UNNEST_TYPE_UNNEST,
    QMO_SUBQUERY_UNNEST_TYPE_NO_UNNEST
} qmoSubqueryUnnestType;

typedef enum qmoInverseJoinOption
{
    QMO_INVERSE_JOIN_METHOD_ALLOWED = 0,  // (default)
    QMO_INVERSE_JOIN_METHOD_ONLY,         // INVERSE_JOIN
    QMO_INVERSE_JOIN_METHOD_DENIED,       // NO_INVERSE_JOIN
} qmoInverseJoinOption;

typedef enum qmoSemiJoinMethod
{
    QMO_SEMI_JOIN_METHOD_NOT_DEFINED = 0,
    QMO_SEMI_JOIN_METHOD_NL,
    QMO_SEMI_JOIN_METHOD_HASH,
    QMO_SEMI_JOIN_METHOD_MERGE,
    QMO_SEMI_JOIN_METHOD_SORT
} qmoSemiJoinMethod;

typedef enum qmoAntiJoinMethod
{
    QMO_ANTI_JOIN_METHOD_NOT_DEFINED = 0,
    QMO_ANTI_JOIN_METHOD_NL,
    QMO_ANTI_JOIN_METHOD_HASH,
    QMO_ANTI_JOIN_METHOD_MERGE,
    QMO_ANTI_JOIN_METHOD_SORT
} qmoAntiJoinMethod;

//---------------------------------------------------------
// [Hints : Join Method�� ����]
//
// ����� Hint : ����ڰ� ��������� Join Method��
//               ������ �� �ִ� Hint�� ���� �װ����� ������.
//    - Nested Loop Join
//    - Hash-based Join
//    - Sort-based Join
//    - Merge Join
//
// ���� Hint : ���������� ����� ���������� �����ϴ���, �Ǵ�
//             ��ɿ� ���� ���� ���̴� ����� �˾ƺ��� ����
//             �߰��� Hint�̴�.
//    - Nested Loop Join �迭�� ���� Hint
//      Full Nested Loop Join
//      Full Store Nested Loop Join
//      Index Nested Loop Jon
//      Anti Outer Join
//    - Sort-based Join �迭�� ���� Hint
//      One Pass Sort Join
//      Two Pass Sort Join
//    - Hash-based Join �迭�� ���� Hint
//      One Pass Hash Join
//      Two Pass Hash Join
//------------------------------------------------------------

#define QMO_JOIN_METHOD_MASK                (0x0000FFFF)
#define QMO_JOIN_METHOD_NONE                (0x00000000)

// Nested Loop Join �迭�� Hints
// Nested Loop
//    SQL: USE_NL( table_name, table_name )
// Full Nested Loop Join :
//    SQL: USE_FULL_NL( table_name, table_name )
// Full Store Nested Loop Join :
//    SQL: USE_FULL_STORE_NL( table_name, table_name )
// Index Nested Loop Join :
//    SQL: USE_INDEX_NL( table_name, table_name )
// Anti Outer Join :
//    SQL: USE_ANTI( table_name, table_name )
// Inverse Index Nested Loop Join (PROJ-2385, Semi-Join) :
//    SQL: NL_SJ INVERSE_JOIN
#define QMO_JOIN_METHOD_NL                  (0x0000001F)
#define QMO_JOIN_METHOD_FULL_NL             (0x00000001)
#define QMO_JOIN_METHOD_INDEX               (0x00000002)
#define QMO_JOIN_METHOD_FULL_STORE_NL       (0x00000004)
#define QMO_JOIN_METHOD_ANTI                (0x00000008)
#define QMO_JOIN_METHOD_INVERSE_INDEX       (0x00000010)

// Hash-based Join �迭�� Hints
// Hash-based
//    SQL: USE_HASH( table_name, table_name )
// One Pass Hash Join :
//    SQL: USE_ONE_PASS_HASH( table_name, table_name )
// Two Pass Hash Join :
//    SQL: USE_TWO_PASS_HASH( table_name, table_name [,temp_table_count] )
// Inverse Hash Join (PROJ-2339) :
//    SQL: USE_INVERSE_HASH( table_name, table_name )
#define QMO_JOIN_METHOD_HASH                (0x000000E0)
#define QMO_JOIN_METHOD_ONE_PASS_HASH       (0x00000020)
#define QMO_JOIN_METHOD_TWO_PASS_HASH       (0x00000040)
#define QMO_JOIN_METHOD_INVERSE_HASH        (0x00000080)

// Sort-based Join �迭�� Hints
// Sort- based :
//     SQL: USE_SORT( table_name, table_name )
// One Pass Sort Join :
//     SQL: USE_ONE_PASS_SORT( table_name, table_name )
// Two Pass Sort Join :
//     SQL: USE_TWO_PASS_SORT( table_name, table_name )
// Inverse Sort Join (PROJ-2385, Semi/Anti-Join) :
//    SQL: SORT_SJ/AJ INVERSE_JOIN 
#define QMO_JOIN_METHOD_SORT                (0x00000F00)
#define QMO_JOIN_METHOD_ONE_PASS_SORT       (0x00000100)
#define QMO_JOIN_METHOD_TWO_PASS_SORT       (0x00000200)
#define QMO_JOIN_METHOD_INVERSE_SORT        (0x00000400)

// Merge Join Hints
// Merge :
//     SQL: USE_MERGE( table_name, table_name)

#define QMO_JOIN_METHOD_MERGE               (0x00001000)

// Inverse Join Hints (PROJ-2385)
#define QMO_JOIN_METHOD_INVERSE         \
    ( QMO_JOIN_METHOD_INVERSE_INDEX     \
      | QMO_JOIN_METHOD_INVERSE_HASH    \
      | QMO_JOIN_METHOD_INVERSE_SORT )

//-------------------------------------------
// [Hints : Table Access ���]
// ���̺� ���� ����� �����ϴ� Hint �̴�.
//    - OPTIMIZED SCAN : Table Access Hint�� �־����� ���� ���,
//                       optimizer�� Table Access Cost�� ���� ����
//    - FULL SCAN : �ε��� ��� �����ϴ���� ���̺� ��ü�� �˻�
//    - INDEX     : ������ �ε����� �ش� ���̺� �˻�
//    - INDEX ASC : ������ �ε����� �ش� ���̺��� �����������θ� �˻�
//    - INDEX DESC: ������ �ε����� �ش� ���̺��� �����������θ� �˻�
//    - NO INDEX  : �˻� �ÿ� ������ �ε����� ������� ����
//-------------------------------------------

typedef enum qmoTableAccessType
{
    QMO_ACCESS_METHOD_TYPE_OPTIMIZED_SCAN = 0,
    QMO_ACCESS_METHOD_TYPE_FULLACCESS_SCAN,
    QMO_ACCESS_METHOD_TYPE_NO_INDEX_SCAN,
    QMO_ACCESS_METHOD_TYPE_INDEXACCESS_SCAN,
    QMO_ACCESS_METHOD_TYPE_INDEX_ASC_SCAN,
    QMO_ACCESS_METHOD_TYPE_INDEX_DESC_SCAN
} qmoTableAccessType;

//-------------------------------------------
// [Hint : �߰� ����� ����]
// �߰� ����� �����Ǵ� ���, �߰� ����� disk temp table�� ������ ��
// memory temp table �� ������ �� �����ϴ� Hint
//    - NOT DEFIENED : �߰� ��� Hint�� �־����� ���� ���
//    - MEMORY       : �߰� ����� �����Ǹ�, Memory Temp Table�� ����
//        SQL : TEMP_TBS_MEMORY
//    - DISK         : �߰� ����� �����Ǹ�, Disk Temp Table�� ����
//        SQL : TEMP_TBS_DISK
//-------------------------------------------

typedef enum qmoInterResultType
{
    QMO_INTER_RESULT_TYPE_NOT_DEFINED = 0,
    QMO_INTER_RESULT_TYPE_MEMORY,
    QMO_INTER_RESULT_TYPE_DISK
} qmoInterResultType;

//-------------------------------------------
// [Hint : Grouping Method ����]
// Grouping�� Sort-based�� ������ ��, Hash-based�� ������ ����
// ������ �� �ִ� ���� Hint
//    - NOT DEFINED : Hint�� �־����� ���� ���
//    - HASH        : Hash-based�� Grouping�� ����
//        SQL : GROUP_HASH
//    - SORT        : Sort-based�� Grouping�� ����
//        SQL : GROUP_SORT
//-------------------------------------------

typedef enum qmoGroupMethodType
{
    QMO_GROUP_METHOD_TYPE_NOT_DEFINED = 0,
    QMO_GROUP_METHOD_TYPE_HASH,
    QMO_GROUP_METHOD_TYPE_SORT
}qmoGroupMethodType;

//-------------------------------------------
// [Hint : Distinction ����]
// Distinction�� Sort-based�� ������ ��, Hash-based�� ������ ����
// ������ �� �ִ� ���� Hint
//    - NOT DEFINED : Hint�� �־����� ���� ���
//    - HASH        : Hash-based�� Distinction�� ����
//        SQL : DISTINCT_HASH
//    - SORT        : Sort-based�� Distinction�� ����
//        SQL : DISTINCT_SORT
//-------------------------------------------

typedef enum qmoDistinctMethodType
{
    QMO_DISTINCT_METHOD_TYPE_NOT_DEFINED = 0,
    QMO_DISTINCT_METHOD_TYPE_HASH,
    QMO_DISTINCT_METHOD_TYPE_SORT
}qmoDistinctMethodType;

//-------------------------------------------
// [Hint : View ����ȭ ����]
// View ����ȭ�� View Materialization���� ������ ��,
// Push Selection���� ������ ���� ������ �� �ִ� ���� Hint
//    - NOT DEFINED : Hint�� �־����� ���� ���
//    - VMTR        : View Materialization ������� View ����ȭ
//        SQL : NO_PUSH_SELECT_VIEW
//    - PUSH        : Push Selection ������� View ����ȭ
//        SQL : PUSH_SELECT_VIEW
//-------------------------------------------

typedef enum qmoViewOptType
{
    QMO_VIEW_OPT_TYPE_NOT_DEFINED = 0,
    QMO_VIEW_OPT_TYPE_VMTR,
    QMO_VIEW_OPT_TYPE_PUSH,
    QMO_VIEW_OPT_TYPE_FORCE_VMTR,
    QMO_VIEW_OPT_TYPE_CMTR,
    // BUG-38022
    // view�� �ѹ��� ����ؼ� ���� ��뿩�ο� ���� VMTR�� �ٲ� �� �ִ� ���
    QMO_VIEW_OPT_TYPE_POTENTIAL_VMTR
} qmoViewOptType;

//-------------------------------------------
// PROJ-2206
// [Hint : View ����ȭ ����]
// MATERIALIZE
// NO_MATERIALIZE
// ex)
// select * from (select /*+ materialize */ * from t1);
// select * from (select /*+ no_materialize */ * from t2);
// select /*+ no_push_select_view(v1) */ * from (select /*+ materialize */ * from t1) v1;
// no_push_select_view(view name), push_select_view(view name)�� ������
// �־��� query set�� ������ �ִ� materialize, no_materialize hint�� ���� enum Ÿ���̴�.
//-------------------------------------------
typedef enum qmoViewOptMtrType
{
    QMO_VIEW_OPT_MATERIALIZE_NOT_DEFINED = 0,
    QMO_VIEW_OPT_MATERIALIZE,
    QMO_VIEW_OPT_NO_MATERIALIZE
}qmoViewOptMtrType;

//-------------------------------------------
// PROJ-1404
// [Hints : Transitive Predicate Generation Hints�� ����]
// Transitive Predicate�� ������ ���� ������ �� �ִ� Hint
//      - ENABLE  : Hint�� �־����� ���� ���
//                  Transitive Predicate�� ����
//      - DISABLE : Transitive Predicate�� �������� ����
//          SQL : NO_TRANSITIVE_PRED
//-------------------------------------------

typedef enum qmoTransitivePredType
{
    QMO_TRANSITIVE_PRED_TYPE_ENABLE = 0,
    QMO_TRANSITIVE_PRED_TYPE_DISABLE
} qmoTransitivePredType;

typedef enum qmoResultCacheType
{
    QMO_RESULT_CACHE_NOT_DEFINED = 0,
    QMO_RESULT_CACHE,
    QMO_RESULT_CACHE_NO
} qmoResultCacheType;

#endif /* _O_QMO_HINT_H_ */
