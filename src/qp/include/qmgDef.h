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
 * $Id: qmgDef.h 90785 2021-05-06 07:26:22Z hykim $
 *
 * Description :
 *     Graph ���� ������ ���� �ڷ� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_DEF_H_
#define _O_QMG_DEF_H_ 1

#include <qc.h>
#include <qmn.h>
#include <qmoStatMgr.h>
#include <smiDef.h>

/***********************************************************************
 * [Graph�� flag�� ���� ���]
 ***********************************************************************/

// qmgGraph.flag
#define QMG_FLAG_CLEAR                     (0x00000000)

// Graph Type�� DISK���� MEMORY ������ ���� ����
// qmgGraph.flag
#define QMG_GRAPH_TYPE_MASK                (0x00000001)
#define QMG_GRAPH_TYPE_MEMORY              (0x00000000)
#define QMG_GRAPH_TYPE_DISK                (0x00000001)

// Grouping, Distinction ���� Method ��� :  Sort Based or Hash Based
// qmgGraph.flag
#define QMG_SORT_HASH_METHOD_MASK          (0x00000002)
#define QMG_SORT_HASH_METHOD_SORT          (0x00000000)
#define QMG_SORT_HASH_METHOD_HASH          (0x00000002)

// Indexable Min Max ���� ����
// qmgGraph.flag
#define QMG_INDEXABLE_MIN_MAX_MASK         (0x00000004)
#define QMG_INDEXABLE_MIN_MAX_FALSE        (0x00000000)
#define QMG_INDEXABLE_MIN_MAX_TRUE         (0x00000004)

// Join Group ���� ����
// qmgGraph.flag
#define QMG_INCLUDE_JOIN_GROUP_MASK        (0x00000008)
#define QMG_INCLUDE_JOIN_GROUP_FALSE       (0x00000000)
#define QMG_INCLUDE_JOIN_GROUP_TRUE        (0x00000008)

// Preserved Order ����
// qmgGraph.flag
#define QMG_PRESERVED_ORDER_MASK               (0x00000030)
#define QMG_PRESERVED_ORDER_NOT_DEFINED        (0x00000000)
#define QMG_PRESERVED_ORDER_DEFINED_FIXED      (0x00000010)
#define QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED  (0x00000020)
#define QMG_PRESERVED_ORDER_NEVER              (0x00000030)

// Join Order ������ Graph���� �ƴ����� ���� ����
// qmgGraph.flag
#define QMG_JOIN_ORDER_DECIDED_MASK        (0x00000100)
#define QMG_JOIN_ORDER_DECIDED_FALSE       (0x00000000)
#define QMG_JOIN_ORDER_DECIDED_TRUE        (0x00000100)

// qmgGraph.flag
// ���� preserved order ��� ���� ���� 
#define QMG_CHILD_PRESERVED_ORDER_USE_MASK   (0x00000200)
#define QMG_CHILD_PRESERVED_ORDER_CAN_USE    (0x00000000)
#define QMG_CHILD_PRESERVED_ORDER_CANNOT_USE (0x00000200)

// PROJ-1353 GROUPBY_EXTENSION��뿩
#define QMG_GROUPBY_EXTENSION_MASK           (0x00000400)
#define QMG_GROUPBY_EXTENSION_FALSE          (0x00000000)
#define QMG_GROUPBY_EXTENSION_TRUE           (0x00000400)

// PROJ-1353 VALUE��  TEMP�� ����ؾ��ϴ� �� ����
#define QMG_VALUE_TEMP_MASK                  (0x00000800)
#define QMG_VALUE_TEMP_FALSE                 (0x00000000)
#define QMG_VALUE_TEMP_TRUE                  (0x00000800)

/* PROJ-1805 Window Clause */
#define QMG_GROUPBY_NONE_MASK                (0x00001000)
#define QMG_GROUPBY_NONE_TRUE                (0x00001000)
#define QMG_GROUPBY_NONE_FALSE               (0x00000000)

/* BUG-40354 pushed rank */
/* pushed rank�Լ��� �������� ǥ�� */
#define QMG_WINDOWING_PUSHED_RANK_MASK       (0x00002000)
#define QMG_WINDOWING_PUSHED_RANK_TRUE       (0x00002000)
#define QMG_WINDOWING_PUSHED_RANK_FALSE      (0x00000000)

// BUG-45296 rownum Pred �� left outer �� ���������� ������ �ȵ˴ϴ�.
#define QMG_ROWNUM_PUSHED_MASK               (0x00004000)
#define QMG_ROWNUM_PUSHED_TRUE               (0x00004000)
#define QMG_ROWNUM_PUSHED_FALSE              (0x00000000)

/*
 * qmgGraph.flag
 * PROJ-1071 Parallel Query
 */

/* bottom-up setting */
#define QMG_PARALLEL_EXEC_MASK               (0x00002000)
#define QMG_PARALLEL_EXEC_TRUE               (0x00002000)
#define QMG_PARALLEL_EXEC_FALSE              (0x00000000)

/*
 * top-down setting
 * PARALLEL_EXEC_MASK ���� �켱��
 */
#define QMG_PARALLEL_IMPOSSIBLE_MASK         (0x00004000)
#define QMG_PARALLEL_IMPOSSIBLE_TRUE         (0x00004000)
#define QMG_PARALLEL_IMPOSSIBLE_FALSE        (0x00000000)

/*
 * BUG-38410
 * top-down setting
 * make plan �������� ���� graph �� ���� parallel ��� ����
 * qmgPartition::makePlan, qmgSelection::makePlan ����
 * flag �� ���� parallel plan / non parallel plan �� �����.
 * NL join �� right �� ���� �ݺ�����Ǵ� plan �� ����
 * parallel ������ �����ϱ����� �������
 */
#define QMG_PLAN_EXEC_REPEATED_MASK          (0x00008000)
#define QMG_PLAN_EXEC_REPEATED_TRUE          (0x00008000)
#define QMG_PLAN_EXEC_REPEATED_FALSE         (0x00000000)

/* PROJ-2509 Hierarchy Query Join */
#define QMG_HIERARCHY_JOIN_MASK              (0x00010000)
#define QMG_HIERARCHY_JOIN_TRUE              (0x00010000)
#define QMG_HIERARCHY_JOIN_FALSE             (0x00000000)

/* PROJ-2714 Multiple table Update,Delete support */
#define QMG_JOIN_ONLY_NL_MASK                (0x00020000)
#define QMG_JOIN_ONLY_NL_TRUE                (0x00020000)
#define QMG_JOIN_ONLY_NL_FALSE               (0x00000000)

/* BUG-47752 left outer join�� on���� OR �������� �����ǰ��ְ�
 * ������ view�� aggregaion�� �����Ұ�� fatal
 */
#define QMG_JOIN_RIGHT_MASK                  (0x00040000)
#define QMG_JOIN_RIGHT_TRUE                  (0x00040000)
#define QMG_JOIN_RIGHT_FALSE                 (0x00000000)

#define QMG_VIEW_PUSH_MASK                   (0x00080000)
#define QMG_VIEW_PUSH_TRUE                   (0x00080000)
#define QMG_VIEW_PUSH_FALSE                  (0x00000000)

// PROJ-2750
#define QMG_LOJN_SKIP_RIGHT_COND_MASK        (0x00100000)
#define QMG_LOJN_SKIP_RIGHT_COND_TRUE        (0x00100000)
#define QMG_LOJN_SKIP_RIGHT_COND_FALSE       (0x00000000)

// Reserved Area
// qmgGraph.flag
#define QMG_RESERVED_AREA_MASK               (0xFF000000)
#define QMG_RESERVED_AREA_CLEAR              (0x00000000)


//------------------------------
//makeColumnMtrNode()�Լ��� �ʿ��� flag
//------------------------------
#define QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_MASK  (0x00000001)
#define QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_FALSE (0x00000000)
#define QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_TRUE  (0x00000001)

/* BUG-48132 */
#define QMG_HASH_OR_SORT_METHOD_MASK          (0x0000000F)
#define QMG_HASH_OR_SORT_METHOD_NOT_DEFINED   (0x00000000)
#define QMG_HASH_OR_SORT_METHOD_GROUPING_MASK (0x00000003)
#define QMG_HASH_OR_SORT_METHOD_GROUPING_HASH (0x00000001)
#define QMG_HASH_OR_SORT_METHOD_GROUPING_SORT (0x00000002)

/* BUG-48120 __OPTIMIER_INDEX_COST_MODE property */
#define QMG_OPTI_INDEX_COST_MODE_1_MASK    (0x00000001)

/***********************************************************************
 * [Graph�� ����]
 **********************************************************************/

typedef enum
{
    QMG_NONE = 0,

    // One Tuple ���� Graph
    QMG_SELECTION,       // Selection Graph
    QMG_PROJECTION,      // Projection Graph

    // Multi Tuple ���� Graph
    QMG_DISTINCTION,     // Duplicate Elimination Graph
    QMG_GROUPING,        // Grouping Graph
    QMG_SORTING,         // Sorting Graph
    QMG_WINDOWING,       // Windowing Graph

    // Product ���� Graph
    QMG_INNER_JOIN,      // Inner Join Graph
    QMG_SEMI_JOIN,       // Semi Join Graph
    QMG_ANTI_JOIN,       // Anti Join Graph
    QMG_LEFT_OUTER_JOIN, // Left Outer Join Graph
    QMG_FULL_OUTER_JOIN, // Full Outer Join Graph

    // SET ���� Graph
    QMG_SET,             // SET Graph

    // Ư�� ���� ���� Graph
    QMG_HIERARCHY,       // Hierarchy Graph
    QMG_DNF,             // DNF Graph
    QMG_PARTITION,       // PROJ-1502 Partition Graph
    QMG_COUNTING,        // PROJ-1405 Counting Graph

    // PROJ-2205 DML ���� Graph
    QMG_INSERT,          // insert graph
    QMG_MULTI_INSERT,    // BUG-36596 multi-table insert
    QMG_UPDATE,          // update graph
    QMG_DELETE,          // delete graph
    QMG_MOVE,            // move graph
    QMG_MERGE,           // merge graph    
    QMG_RECURSIVE_UNION_ALL, // PROJ-2582 recursive with

    // PROJ-2638 Shard Graph
    QMG_SHARD_SELECT,    // Shard Select Graph
    QMG_SHARD_DML,       // Shard DML Graph
    QMG_SHARD_INSERT,    // Shard Insert Graph

    QMG_MAX_MAX
} qmgType;

/***********************************************************************
 * [���� ��� ���� �ڷ� ����]
 *
 **********************************************************************/
typedef struct qmgCostInfo
{
    SDouble             recordSize;
    SDouble             selectivity;
    SDouble             inputRecordCnt;
    SDouble             outputRecordCnt;
    SDouble             myAccessCost;
    SDouble             myDiskCost;
    SDouble             myAllCost;
    SDouble             totalAccessCost;
    SDouble             totalDiskCost;
    SDouble             totalAllCost;
}qmgCostInfo;

/***********************************************************************
 * [Preserved Order �ڷ� ����]
 *
 **********************************************************************/

//--------------------------------------------------------
// Preserved Order�� Direction  ����
//--------------------------------------------------------
typedef enum
{
    QMG_DIRECTION_NOT_DEFINED = 0, // direction �������� ����
    QMG_DIRECTION_SAME_WITH_PREV,  // direction �������� �ʾ����� ���� Į����
                                   // ������ ������ ���Ե�
    QMG_DIRECTION_DIFF_WITH_PREV,  // direction �������� �ʾ����� ���� Į����
                                   // �ٸ� ������ ���Ե�
    QMG_DIRECTION_ASC,             // Ascending Order�� ����
    QMG_DIRECTION_DESC,            // Descending Order�� ����
    // BUG-40361 supporting to indexable analytic function
    QMG_DIRECTION_ASC_NULLS_FIRST, // Ascending Order�� Nulls First  ����
    QMG_DIRECTION_ASC_NULLS_LAST,  // Ascending Order�� Nulls Last  ����
    QMG_DIRECTION_DESC_NULLS_FIRST,// Descending Order�� Nulls First ����
    QMG_DIRECTION_DESC_NULLS_LAST  // Descending Order�� Nulls Last ����
} qmgDirectionType;

//--------------------------------------------------------
// Preserved Order�� �ڷ� ����
//    - mtcColumn���κ��� ȹ��
//      column = (mtcColumn.column.id % SMI_COLUMN_ID_MAXIMUM)
//    - qtcNode�κ��� ȹ��
//      column = qtcNode.node.column
//--------------------------------------------------------
typedef struct qmgPreservedOrder
{
    UShort              table;        // tuple set���� �ش� table�� position
    UShort              column;       // tuple set���� �ش� column�� position
    qmgDirectionType    direction;    // preserved order direction ����
    qmgPreservedOrder * next;
} qmgPreservedOrder;

/***********************************************************************
 * [��� Graph�� ���� �Լ� ������]
 * �� Graph�� ����ȭ �Ǵ� Plan Tree������ ���� �Լ� ������
 * initialize �������� Graph���� �Լ��� Setting�ϰ� �ȴ�.
 * �̷μ�, ���� Graph������ ���� Graph�� ������ ���� ����
 * ������ �Լ� �����͸� ȣ�������μ� ó���� �� �ְ� �ȴ�.
 ***********************************************************************/

struct qmgGraph;
struct qmgJOIN;

// �� Graph�� ����ȭ�� �����ϴ� �Լ� ������
typedef IDE_RC (* optimizeFunc ) ( qcStatement * aStatement,
                                   qmgGraph    * aGraph );

// �� Graph�� Execution Plan�� �����ϴ� �Լ� ������
typedef IDE_RC (* makePlanFunc ) ( qcStatement    * aStatement,
                                   const qmgGraph * aParent,
                                   qmgGraph       * aGraph );

// �� Graph�� Graph ��� ������ �����ϴ� �Լ� ������
typedef IDE_RC (* printGraphFunc) ( qcStatement  * aTemplate,
                                    qmgGraph     * aGraph,
                                    ULong          aDepth,
                                    iduVarString * aString );

#define QMG_PRINT_LINE_FEED( i, aDepth, aString )       \
    {                                                   \
        iduVarStringAppend( aString, "\n" );            \
        iduVarStringAppend( aString, "|" );             \
        for ( i = 0; i < aDepth; i++ )                  \
        {                                               \
            iduVarStringAppend( aString, "  " );        \
        }                                               \
        iduVarStringAppend( aString, "|" );             \
    }

/***********************************************************************
 *  [ Graph �� ���� ]
 *
 *  ---------------------------------------------
 *    Graph     |  left   |  right  | children
 *  ---------------------------------------------
 *  unary graph  |    O    |  null   |   null
 *  ---------------------------------------------
 *  binary graph |    O    |    O    |   null
 *  ---------------------------------------------
 *  multi  graph |   null  |  null   |    O
 *  ----------------------------------------------
 *
 *  Multi Graph : Parsing, Validation �ܰ迡���� �������� ������,
 *                Optimization �ܰ迡�� ����ȭ�� ���� �����ȴ�.
 *
 **********************************************************************/

/***********************************************************************
 * Multiple Children �� �����ϱ� ���� �ڷ� ����
 *      ( PROJ-1486 Multiple Bag Union )
 **********************************************************************/

typedef struct qmgChildren
{
    struct qmgGraph     * childGraph;     // Child Graph
    struct qmgChildren  * next;
} qmgChildren;

/***********************************************************************
 * [��� Graph�� ���� ���� �ڷ� ����]
 *
 **********************************************************************/
typedef struct qmgGraph
{
    qmgType           type;          // Graph �� ����
    UInt              flag;          // Masking ����, joinGroup ���� ����

    // Graph�� dependencies
    qcDepInfo         depInfo;

    // �ش� Graph�� ���� �Լ� ������
    optimizeFunc      optimize;      // �ش� Graph�� ����ȭ�� �����ϴ� �Լ�
    makePlanFunc      makePlan;      // �ش� Graph�� Plan�� �����ϴ� �Լ�
    printGraphFunc    printGraph;    // �ش� Graph�� ������ ����ϴ� �Լ�

    //-------------------------------------------------------
    // [Child Graph]
    // Graph���� ���� ����� left�� right�� ǥ���ȴ�.
    //-------------------------------------------------------

    qmgGraph        * left;          // left child graph
    qmgGraph        * right;         // right child graph
    qmgChildren     * children;      // multiple child graph

    //-------------------------------------------------------
    // [myPlan]
    // �ش� Graph�κ��� ������ Plan
    // Graph �� ���� ����ȭ ��������� NULL���� ������,
    // makePlan() �Լ� �����͸� ���Ͽ� �����ȴ�.
    //-------------------------------------------------------
    qmnPlan         * myPlan;        // Graph�� ������ Plan Tree

    qmoPredicate    * myPredicate;   // �ش� graph���� ó���Ǿ�� �� predicate
    qmoPredicate    * constantPredicate; // CNF�κ��� ����� Constant Predicate
    qmoPredicate    * ridPredicate;      // PROJ-1789 PROWID

    qtcNode         * nnfFilter;     // To Fix PR-12743, NNF filter ����

    qmsFrom         * myFrom;        // �ش� base graph�� �����Ǵ� qmsFrom
    qmsQuerySet     * myQuerySet;    // �ش� base graph�� ���� querySet

    qmgCostInfo       costInfo;      // ���� ��� ����
    struct qmoCNF   * myCNF;         // top graph�� ���,
                                     // �ش� graph�� ���� CNF�� ����Ŵ
    qmgPreservedOrder * preservedOrder; // �ش� graph�� preserved order ����

} qmgGraph;

/* TASK-6744 */
typedef struct qmgRandomPlanInfo
{
    UInt mWeightedValue;      /* ����ġ */
    UInt mTotalNumOfCases;    /* ��� ����� ���� ��ģ �� */
} qmgRandomPlanInfo;

// ��� Graph�� ���������� ����ϴ� ���

class qmg
{
public:
    //-----------------------------------------
    // Graph ������ ���������� �ʿ��� �Լ��� �߰�
    //-----------------------------------------

    // Graph�� ���� ������ �ʱ�ȭ��.
    static IDE_RC initGraph( qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC printGraph( qcStatement  * aStatement,
                              qmgGraph     * aGraph,
                              ULong          aDepth,
                              iduVarString * aString );


    // Subquery���� Graph ������ �����.
    static IDE_RC printSubquery( qcStatement  * aStatement,
                                 qtcNode      * aSubQuery,
                                 ULong          aDepth,
                                 iduVarString * aString );

    // ���ϴ� Preserved Order�� �����ϴ� ���� �˻�
    static IDE_RC checkUsableOrder( qcStatement       * aStatement,
                                    qmgPreservedOrder * aWantOrder,
                                    qmgGraph          * aLeftGraph,
                                    qmoAccessMethod  ** aOriginalMethod,
                                    qmoAccessMethod  ** aSelectMethod,
                                    idBool            * aUsable );

    // Child Graph�� ���Ͽ� Preserved Order�� ������.
    static IDE_RC setOrder4Child( qcStatement       * aStatement,
                                  qmgPreservedOrder * aWantOrder,
                                  qmgGraph          * aLeftGraph );

    // Preserved order ���� ������ ���, Preserved Order ����
    static IDE_RC tryPreservedOrder( qcStatement       * aStatement,
                                     qmgGraph          * aGraph,
                                     qmgPreservedOrder * aWantOrder,
                                     idBool            * aSuccess );

    /* BUG-39298 improve preserved order */
    static IDE_RC retryPreservedOrder(qcStatement       * aStatement,
                                      qmgGraph          * aGraph,
                                      qmgPreservedOrder * aWantOrder,
                                      idBool            * aUsable);

    // target�� Į������ cardinality�� �̿��Ͽ� bucket count ���ϴ� �Լ�
    static IDE_RC getBucketCntWithTarget( qcStatement * aStatement,
                                          qmgGraph    * aGraph,
                                          qmsTarget   * aTarget,
                                          UInt          aHintBucketCnt,
                                          UInt        * aBucketCnt );

    // distinct aggregation column�� bucket count�� ����
    static IDE_RC getBucketCnt4DistAggr( qcStatement * aStatement,
                                         SDouble       aChildOutputRecordCnt,
                                         UInt          aGroupBucketCnt,
                                         qtcNode     * aNode,
                                         UInt          aHintBucketCnt,
                                         UInt        * aBucketCnt );

    // Graph�� ����� ���� ��ü�� �Ǵ���
    static IDE_RC isDiskTempTable( qmgGraph    * aGraph,
                                   idBool      * aIsDisk );

    //-----------------------------------------
    // Plan Tree ������ ���������� �ʿ��� �Լ��� �߰�
    //-----------------------------------------

    //-----------------------------------------
    // ���� Column ������ ���� �Լ�
    //-----------------------------------------

    // ���� Column�� ���� qmcMtrNode�� ����
    static IDE_RC makeColumnMtrNode( qcStatement  * aStatement ,
                                     qmsQuerySet  * aQuerySet ,
                                     qtcNode      * aSrcNode,
                                     idBool         aConverted,
                                     UShort         aNewTupleID ,
                                     UInt           aFlag,
                                     UShort       * aColumnCount ,
                                     qmcMtrNode  ** aMtrNode);

    static IDE_RC makeBaseTableMtrNode( qcStatement  * aStatement ,
                                        qtcNode      * aSrcNode ,
                                        UShort         aDstTupleID ,
                                        UShort       * aColumnCount ,
                                        qmcMtrNode  ** aMtrNode );

    // To Fix PR-11562
    // Indexable MIN-MAX ����ȭ�� ����� ���
    // Preserved Order�� ���⼺�� ����, ���� �ش� ������
    // �������� �ʿ䰡 ����.
    // indexable min-max���� �˻� ������ ����
    // static IDE_RC    setDirection4IndexableMinMax( UInt   * aFlag ,
    //                                                UInt     aMinMaxFlag ,
    //                                                UInt     aOrderFlag );

    //display ������ ����
    static IDE_RC setDisplayInfo( qmsFrom          * aFrom ,
                                  qmsNamePosition  * aTableOwnerName ,
                                  qmsNamePosition  * aTableName ,
                                  qmsNamePosition  * aAliasName );

    //remote object�� display ������ ����
    static IDE_RC setDisplayRemoteInfo( qcStatement      * aStatement ,
                                        qmsFrom          * aFrom ,
                                        qmsNamePosition  * aRemoteUserName ,
                                        qmsNamePosition  * aRemoteObjectName ,
                                        qmsNamePosition  * aTableOwnerName ,
                                        qmsNamePosition  * aTableName ,
                                        qmsNamePosition  * aAliasName );

    //mtcColumn ������ mtcExecute������ �����Ѵ�.
    static IDE_RC copyMtcColumnExecute( qcStatement      * aStatement ,
                                        qmcMtrNode       * aMtrNode );

    // PROJ-2179
    static IDE_RC setCalcLocate( qcStatement * aStatement,
                                 qmcMtrNode  * aMtrNode );

    // PROJ-1762
    // Analytic Function ����� ����� qmcMtrNode�� �����Ѵ�.
    static IDE_RC
    makeAnalFuncResultMtrNode( qcStatement      * aStatement,
                               qmsAnalyticFunc  * aAnalFunc,
                               UShort             aTupleID,
                               UShort           * aColumnCount,
                               qmcMtrNode      ** aMtrNode,
                               qmcMtrNode      ** aLastMtrNode );
    
    // PROJ-1762
    // sort mtr node 
    static IDE_RC makeSortMtrNode( qcStatement        * aStatement,
                                   UShort               aTupleID,
                                   qmgPreservedOrder  * aSortKey,
                                   qmsAnalyticFunc    * aAnalFuncList,
                                   qmcMtrNode         * aMtrNode,
                                   qmcMtrNode        ** aSortMtrNode,
                                   UInt               * aSortMtrNodeCount );
    
    // PROJ-1762
    // window mtr node & distinct mtr node 
    static IDE_RC
    makeWndNodeNDistNodeNAggrNode( qcStatement        * aStatement,
                                   qmsQuerySet        * aQuerySet,
                                   UInt                 aFlag,
                                   qmnPlan            * aChildPlan,
                                   qmsAnalyticFunc    * aAnalyticFunc,
                                   qmcMtrNode         * aMtrNode,
                                   qmoDistAggArg      * aDistAggArg,
                                   qmcMtrNode         * aAggrResultNode,
                                   UShort               aAggrTupleID,
                                   qmcWndNode        ** aWndNode,
                                   UInt               * aWndNodeCount,
                                   UInt               * aPtnNodeCount,
                                   qmcMtrNode        ** aDistNode,
                                   UInt               * aDistNodeCount,
                                   qmcMtrNode        ** aAggrNode,
                                   UInt               * aAggrNodeCount );
    
    // PROJ-1762
    static IDE_RC makeDistNode( qcStatement    * aStatement,
                                qmsQuerySet    * aQuerySet,
                                UInt             aFlag,
                                qmnPlan        * aChildPlan,
                                UShort           aTupleID,
                                qmoDistAggArg  * aDistAggArg,
                                qtcNode        * aAggrNode,
                                qmcMtrNode     * aNewMtrNode,
                                qmcMtrNode    ** aPlanDistNode,
                                UShort         * aDistNodeCount );

    // LOJN , FOJN�� Filter�� ������ش�.
    static IDE_RC makeOuterJoinFilter(qcStatement   * aStatement ,
                                      qmsQuerySet   * aQuerySet ,
                                      qmoPredicate ** aPredicate ,
                                      qtcNode       * aNnfFilter,
                                      qtcNode      ** aFilter);

    // CNF, DNF cost �Ǵܽ�, ��ҵ� method�� ���ؼ�
    // optimize�� ������� sdf�� ��� �����Ѵ�.
    static IDE_RC removeSDF( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree �����ÿ� ȣ���ϴ� ���� �Լ�
    //   HASH ����
    static IDE_RC makeLeftHASH4Join( qcStatement  * aStatement,
                                     qmgGraph     * aGraph,
                                     UInt           aMtrFlag,
                                     UInt           aJoinFlag,
                                     qtcNode      * aRange,
                                     qmnPlan      * aChild,
                                     qmnPlan      * aPlan );

    static IDE_RC makeRightHASH4Join( qcStatement  * aStatement,
                                      qmgGraph     * aGraph,
                                      UInt           aMtrFlag,
                                      UInt           aJoinFlag,
                                      qtcNode      * aRange,
                                      qmnPlan      * aChild,
                                      qmnPlan      * aPlan );

    // Graph�� Plan Tree �����ÿ� ȣ���ϴ� ���� �Լ�
    //   SORT ����

    static IDE_RC initLeftSORT4Join( qcStatement  * aStatement,
                                     qmgGraph     * aGraph,
                                     UInt           aJoinFlag,
                                     qtcNode      * aRange,
                                     qmnPlan      * aParent,
                                     qmnPlan     ** aPlan );

    static IDE_RC makeLeftSORT4Join( qcStatement  * aStatement,
                                     qmgGraph     * aGraph,
                                     UInt           aMtrFlag,
                                     UInt           aJoinFlag,
                                     qmnPlan      * aChild,
                                     qmnPlan      * aPlan );

    static IDE_RC initRightSORT4Join( qcStatement  * aStatement,
                                      qmgGraph     * aGraph,
                                      UInt           aJoinFlag,
                                      idBool         aOrderCheckNeed,
                                      qtcNode      * aRange,
                                      idBool         aIsRangeSearch,
                                      qmnPlan      * aParent,
                                      qmnPlan     ** aPlan );

    static IDE_RC makeRightSORT4Join( qcStatement  * aStatement,
                                      qmgGraph     * aGraph,
                                      UInt           aMtrFlag,
                                      UInt           aJoinFlag,
                                      idBool         aOrderCheckNeed,
                                      qmnPlan      * aChild,
                                      qmnPlan      * aPlan );

    // BUG-18525
    static IDE_RC isNeedMtrNode4LeftChild( qcStatement  * aStatement,
                                           qmnPlan      * aChild,
                                           idBool       * aIsNeed );

    static IDE_RC usableIndexScanHint( qcmIndex            * aHintIndex,
                                       qmoTableAccessType    aHintAccessType,
                                       qmoIdxCardInfo      * aIdxCardInfo,
                                       UInt                  aIdxCnt,
                                       qmoIdxCardInfo     ** aSelectedIdxInfo,
                                       idBool              * aUsableHint );

    static IDE_RC resetColumnLocate( qcStatement * aStatement, UShort aTupleID );

    static IDE_RC setColumnLocate( qcStatement * aStatement,
                                   qmcMtrNode  * aMtrNode );

    static IDE_RC changeTargetColumnLocate( qcStatement  * aStatement,
                                            qmsQuerySet  * aQuerySet,
                                            qmsTarget    * aTarget );

    // PROJ-1473
    // validation�� ������ ����� �⺻ ��ġ������
    // ������ �����ؾ� �� ����� ��ġ������ �����Ѵ�.
    static IDE_RC changeColumnLocate( qcStatement  * aStatement,
                                      qmsQuerySet  * aQuerySet,
                                      qtcNode      * aNode,
                                      UShort         aTableID,
                                      idBool         aNext );

    static IDE_RC changeColumnLocateInternal( qcStatement * aStatement,
                                              qmsQuerySet * aQuerySet,
                                              qtcNode     * aNode,
                                              UShort        aTableID );

    static IDE_RC findColumnLocate( qcStatement  * aStatement,
                                    UInt           aParentTupleID,
                                    UShort         aTableID,
                                    UShort         aOrgTable,
                                    UShort         aOrgColumn,
                                    UShort       * aChangeTable,
                                    UShort       * aChangeColumn );

    static IDE_RC findColumnLocate( qcStatement  * aStatement,
                                    UShort         aTableID,
                                    UShort         aOrgTable,
                                    UShort         aOrgColumn,
                                    UShort       * aChangeTable,
                                    UShort       * aChangeColumn );
    
    // ������ ���⼺�� ���� Column������ ��
    static IDE_RC checkSameDirection( qmgPreservedOrder * aWantOrder,
                                      qmgPreservedOrder * aPresOrder,
                                      qmgDirectionType    aPrevDirection,
                                      qmgDirectionType  * aNowDirection,
                                      idBool            * aUsable );

    // BUG-42145 Index������ Nulls Option ó���� �����ؼ�
    // ������ ���⼺�� ���� Column������ ��
    static IDE_RC checkSameDirection4Index( qmgPreservedOrder * aWantOrder,
                                            qmgPreservedOrder * aPresOrder,
                                            qmgDirectionType    aPrevDirection,
                                            qmgDirectionType  * aNowDirection,
                                            idBool            * aUsable );
    //Sort ���� �÷��� ���� ������ �����Ѵ�.
    static IDE_RC setDirection4SortColumn(
        qmgPreservedOrder  * aPreservedOrder,
        UShort               aColumnID,
        UInt               * aFlag );

    // BUG-32303
    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

    // Preserved order�� direction�� �����Ѵ�.
    static IDE_RC copyPreservedOrderDirection(
        qmgPreservedOrder * aDstOrder,
        qmgPreservedOrder * aSrcOrder );

    static idBool isSamePreservedOrder(
            qmgPreservedOrder * aDstOrder,
            qmgPreservedOrder * aSrcOrder );

    static IDE_RC makeDummyMtrNode( qcStatement  * aStatement ,
                                    qmsQuerySet  * aQuerySet,
                                    UShort         aTupleID,
                                    UShort       * aColumnCount,
                                    qmcMtrNode  ** aMtrNode );

    static idBool getMtrMethod( qcStatement * aStatement,
                                UShort        aSrcTupleID,
                                UShort        aDstTupleID );

    static idBool existBaseTable( qmcMtrNode * aMtrNode,
                                  UInt         aFlag,
                                  UShort       aTable );

    static UInt getBaseTableType( ULong aTupleFlag );

    static idBool isTempTable( ULong aTupleFlag );

    // PROJ-2242
    static void setPlanInfo( qcStatement  * aStatement,
                             qmnPlan      * aPlan,
                             qmgGraph     * aGraph );

    // PROJ-2418 
    static IDE_RC getGraphLateralDepInfo( qmgGraph  * aGraph,
                                          qcDepInfo * aGraphLateralDepInfo );

    static IDE_RC resetDupNodeToMtrNode( qcStatement * aStatement,
                                         UShort        aOrgTable,
                                         UShort        aOrgColumn,
                                         qtcNode     * aChangeNode,
                                         qtcNode     * aNode );

    // BUG-43493
    static IDE_RC lookUpMaterializeGraph( qmgGraph * aGraph,
                                          idBool   * aFound );
    
    /* TASK-6744 */
    static void initializeRandomPlanInfo( qmgRandomPlanInfo * aRandomPlanInfo );

    /* TASK-7219 */
    static IDE_RC getNodeOffset( qtcNode * aNode,
                                 idBool    aIsRecursive,
                                 SInt    * aStart,
                                 SInt    * aEnd );

    static IDE_RC getFromOffset( qmsFrom * aFrom,
                                 SInt    * aStart,
                                 SInt    * aEnd );

    static IDE_RC makeShardParamOffsetArrayForGraph( qcStatement       * aStatement,
                                                     qcParamOffsetInfo * aParamOffsetInfo,
                                                     UShort            * aOutParamCount,
                                                     qcShardParamInfo ** aOutParamInfo );

    static IDE_RC makeShardParamOffsetArrayWithInfo( qcStatement        * aStatement,
                                                     sdiAnalyzeInfo     * aAnalyzeInfo,
                                                     qcParamOffsetInfo  * aParamOffsetInfo,
                                                     UShort             * aOutParamCount,
                                                     UShort             * aOutParamOffset,
                                                     qcShardParamInfo  ** aOutShardParamInfo );

    static IDE_RC makeShardParamOffsetArray( qcStatement       * aStatement,
                                             qcNamePosition    * aParsePosition,
                                             UShort            * aOutParamCount,
                                             UShort            * aOutParamOffset,
                                             qcShardParamInfo ** aOutShardParamInfo );

    static IDE_RC findAndCollectParamOffset( qcStatement       * aStatement,
                                             qtcNode           * aNode,
                                             qcParamOffsetInfo * aParamOffsetInfo );

    static IDE_RC collectReaminParamOffset( qcStatement       * aStatement,
                                            SInt                aStartOffset,
                                            SInt                aEndOffset,
                                            qcParamOffsetInfo * aParamOffsetInfo );

    static IDE_RC copyAndCollectParamOffset( qcStatement       * aStatement,
                                             SInt                aStartOffset,
                                             SInt                aEndOffset,
                                             UShort              aParamOffset,
                                             UShort              aParamCount,
                                             qcShardParamInfo  * aShardParamInfo,
                                             qcParamOffsetInfo * aParamOffsetInfo );

    static IDE_RC setHostVarOffset( qcStatement * aStatement );

    static IDE_RC checkStackOverflow( mtcNode * aNode,
                                      SInt      aRemain,
                                      idBool  * aIsOverflow );

    static IDE_RC adjustParamOffsetForAnalyzeInfo( sdiAnalyzeInfo    * aAnalyzeInfo,
                                                   UShort              aParamCount,
                                                   qcShardParamInfo ** aOutShardParamInfo );
private:

    //-----------------------------------------
    // Preserved Order ���� �Լ�
    //-----------------------------------------

    // Graph������ �ش� Order�� ����� �� �ִ� �� �˻�.
    static IDE_RC checkOrderInGraph ( qcStatement       * aStatement,
                                      qmgGraph          * aGraph,
                                      qmgPreservedOrder * aWantOrder,
                                      qmoAccessMethod  ** aOriginalMethod,
                                      qmoAccessMethod  ** aSelectMethod,
                                      idBool            * aUsable );

    // ���ϴ� Order�� �����ϴ� Index�� �����ϴ� �� �˻�
    static IDE_RC    checkUsableIndex4Selection( qmgGraph          * aGraph,
                                                 qmgPreservedOrder * aWantOrder,
                                                 idBool              aOrderNeed,
                                                 qmoAccessMethod  ** aOriginalMethod,
                                                 qmoAccessMethod  ** aSelectMethod,
                                                 idBool            * aUsable );

    // PROJ-2242 qmoJoinMethodMgr �� ���� �Ȱܿ�
    static IDE_RC checkUsableIndex( qcStatement       * aStatement,
                                    qmgGraph          * aGraph,
                                    qmgPreservedOrder * aWantOrder,
                                    idBool              aOrderNeed,
                                    qmoAccessMethod  ** aOriginalMethod,
                                    qmoAccessMethod  ** aSelectMethod,
                                    idBool            * aUsable );

    // ���ϴ� Order�� �����ϴ� Index�� �����ϴ� �� �˻�
    static IDE_RC    checkUsableIndex4Partition( qmgGraph          * aGraph,
                                                 qmgPreservedOrder * aWantOrder,
                                                 idBool              aOrderNeed,
                                                 qmoAccessMethod  ** aOriginalMethod,
                                                 qmoAccessMethod  ** aSelectMethod,
                                                 idBool            * aUsable );

    // ���ϴ� Order�� �ش� Index�� Order�� ������ �� �˻�
    static IDE_RC checkIndexOrder( qmoAccessMethod   * aMethod,
                                   UShort              aTableID,
                                   qmgPreservedOrder * aWantOrder,
                                   idBool            * aUsable );

    // ���ϴ� Order�� �ش� Index�� ��� Key Column�� ���ԵǴ� �� �˻�
    static IDE_RC checkIndexColumn( qcmIndex          * aIndex,
                                    UShort              aTableID,
                                    qmgPreservedOrder * aWantOrder,
                                    idBool            * aUsable );

    // ���ϴ� Order�� ID�� Target�� ID�� ����
    static IDE_RC refineID4Target( qmgPreservedOrder * aWantOrder,
                                   qmsTarget         * aTarget );

    // �ش� Graph�� ���Ͽ� Preserved Order�� ������
    static IDE_RC makeOrder4Graph( qcStatement       * aStatement,
                                   qmgGraph          * aGraph,
                                   qmgPreservedOrder * aWantOrder );

    // Selection Graph�� ���Ͽ� Index�� �̿��� Preserved Order�� ������
    static IDE_RC makeOrder4Index( qcStatement       * aStatement,
                                   qmgGraph          * aGraph,
                                   qmoAccessMethod   * aMethod,
                                   qmgPreservedOrder * aWantOrder );

    // Non Leaf Graph�� ���Ͽ� Preserved Order�� ������
    static IDE_RC makeOrder4NonLeafGraph( qcStatement       * aStatement,
                                          qmgGraph          * aGraph,
                                          qmgPreservedOrder * aWantOrder );

    // SYSDATE ���� ���� ��¥ ����  pseudo column���� Ȯ��
    static idBool isDatePseudoColumn( qcStatement * aStatement,
                                      qtcNode     * aNode );
    
    // BUG-37355 node tree���� pass node�� ������Ų��.
    static IDE_RC isolatePassNode( qcStatement * aStatement,
                                   qtcNode     * aSource );

    /* TASK-7219 */
    static IDE_RC getFromEnd( qmsFrom * aFrom,
                              SInt    * aFromWhereEnd );

    static IDE_RC getFromStart( qmsFrom * aFrom,
                                SInt    * aFromWhereStart );

    static IDE_RC getParamOffsetAndCount( qcStatement * aStatement,
                                          SInt          aStartOffset,
                                          SInt          aEndOffset,
                                          UShort        aStartParamOffset,
                                          UShort        aEndParamCount,
                                          UShort      * aParamOffset,
                                          UShort      * aParamCount );

    static IDE_RC getHostVarOffset( qcStatement * aStatement,
                                    UShort      * aParamOffset );
};

#endif /* _O_QMG_DEF_H_ */
