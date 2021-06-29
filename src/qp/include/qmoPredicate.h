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
 * $Id: qmoPredicate.h 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Predicate Manager
 *
 *     �� �����ڸ� �����ϴ� Predicate�鿡 ���� �������� ������
 *     ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_PREDICATE_H_
#define _O_QMO_PREDICATE_H_ 1

#include <qc.h>
#include <qmsParseTree.h>
#include <qmoStatMgr.h>


//---------------------------------------------------
// Predicate�� columnID ����
//---------------------------------------------------

/* qmoPredicate.id                                 */
// QMO_COLUMNID_NOT_FOUND : �ʱⰪ columnID, getColumnID() ���� ���
// QMO_COLUMNID_LIST : LIST�� ���� ���ǵ� columnID
// QMO_COLUMNID_NON_INDEXABLE : non-indexable�� ���� ���ǵ� columnID
#define QMO_COLUMNID_NOT_FOUND             (ID_UINT_MAX-2)
#define QMO_COLUMNID_LIST                  (ID_UINT_MAX-1)
#define QMO_COLUMNID_NON_INDEXABLE         (  ID_UINT_MAX)

//---------------------------------------------------
// Predicate�� flag ����
//---------------------------------------------------

/* qmoPredicate.flag�� �ʱ�ȭ ��Ŵ                    */
# define QMO_PRED_CLEAR                       (0x00000000)

/* qmoPredicate.flag                                  */
// Key Range�� �������� ��� ���� ����
// keyRange ����, ������ �ʿ��� ����
# define QMO_PRED_NEXT_KEY_USABLE_MASK        (0x00000001)
# define QMO_PRED_NEXT_KEY_UNUSABLE           (0x00000000)
# define QMO_PRED_NEXT_KEY_USABLE             (0x00000001)

/* qmoPredicate.flag                                  */
// join predicate������ ���� ����
# define QMO_PRED_JOIN_PRED_MASK              (0x00000002)
# define QMO_PRED_JOIN_PRED_FALSE             (0x00000000)
# define QMO_PRED_JOIN_PRED_TRUE              (0x00000002)

/* qmoPredicate.flag                                  */
// fixed or variable������ ����
# define QMO_PRED_VALUE_MASK                  (0x00000004)
# define QMO_PRED_FIXED                       (0x00000000)
# define QMO_PRED_VARIABLE                    (0x00000004)

/* qmoPredicate.flag                                  */
// joinable predicate ����
# define QMO_PRED_JOINABLE_PRED_MASK          (0x00000008)
# define QMO_PRED_JOINABLE_PRED_FALSE         (0x00000000)
# define QMO_PRED_JOINABLE_PRED_TRUE          (0x00000008)

/* qmoPredicate.flag                                  */
// ���డ�� join method ����
# define QMO_PRED_JOINABLE_MASK ( QMO_PRED_INDEX_JOINABLE_MASK |        \
                                  QMO_PRED_HASH_JOINABLE_MASK  |        \
                                  QMO_PRED_SORT_JOINABLE_MASK  |        \
                                  QMO_PRED_MERGE_JOINABLE_MASK )
# define QMO_PRED_JOINABLE_FALSE              (0x00000000)

/* qmoPredicate.flag                                  */
// index nested loop join method ��밡�� ���� ����
# define QMO_PRED_INDEX_JOINABLE_MASK         (0x00000010)
# define QMO_PRED_INDEX_JOINABLE_FALSE        (0x00000000)
# define QMO_PRED_INDEX_JOINABLE_TRUE         (0x00000010)

/* qmoPredicate.flag                                  */
// hash join method ��밡�� ���� ����
# define QMO_PRED_HASH_JOINABLE_MASK          (0x00000020)
# define QMO_PRED_HASH_JOINABLE_FALSE         (0x00000000)
# define QMO_PRED_HASH_JOINABLE_TRUE          (0x00000020)

/* qmoPredicate.flag                                  */
// sort join method ��밡�� ���� ����
# define QMO_PRED_SORT_JOINABLE_MASK          (0x00000040)
# define QMO_PRED_SORT_JOINABLE_FALSE         (0x00000000)
# define QMO_PRED_SORT_JOINABLE_TRUE          (0x00000040)

/* qmoPredicate.flag                                  */
// merge join method ��밡�� ���� ����
# define QMO_PRED_MERGE_JOINABLE_MASK         (0x00000080)
# define QMO_PRED_MERGE_JOINABLE_FALSE        (0x00000000)
# define QMO_PRED_MERGE_JOINABLE_TRUE         (0x00000080)

/* qmoPredicate.flag                                  */
// index nested loop join ���� ���� ���� ����
# define QMO_PRED_INDEX_DIRECTION_MASK        (0x00000300)
# define QMO_PRED_INDEX_NON_DIRECTION         (0x00000000)
# define QMO_PRED_INDEX_LEFT_RIGHT            (0x00000100)
# define QMO_PRED_INDEX_RIGHT_LEFT            (0x00000200)
# define QMO_PRED_INDEX_BIDIRECTION           (0x00000300)

/* BUG-47509 */
#define QMO_PRED_OR_VALUE_INDEX_MASK          (0x00000400)
#define QMO_PRED_OR_VALUE_INDEX_TRUE          (0x00000400)
#define QMO_PRED_OR_VALUE_INDEX_FALSE         (0x00000000)

/* BUG-47986 */
#define QMO_PRED_JOIN_OR_VALUE_INDEX_MASK     (0x00000800)
#define QMO_PRED_JOIN_OR_VALUE_INDEX_TRUE     (0x00000800)
#define QMO_PRED_JOIN_OR_VALUE_INDEX_FALSE    (0x00000000)

/* qmoPredicate.flag                                  */
// merge join ���� ���� ���� ����
# define QMO_PRED_MERGE_DIRECTION_MASK        (0x00003000)
# define QMO_PRED_MERGE_NON_DIRECTION         (0x00000000)
# define QMO_PRED_MERGE_LEFT_RIGHT            (0x00001000)
# define QMO_PRED_MERGE_RIGHT_LEFT            (0x00002000)

/* qmoPredicate.flag                                  */
// joinGroup�� ���ԵǾ����� ����
# define QMO_PRED_INCLUDE_JOINGROUP_MASK      (0x00010000)
# define QMO_PRED_INCLUDE_JOINGROUP_FALSE     (0x00000000)
# define QMO_PRED_INCLUDE_JOINGROUP_TRUE      (0x00010000)

/* qmoPredicate.flag                                  */
// Index Nested Loop Join�� joinable predicate���� ǥ���ϴ� ����
// join predicate�� selection graph�� ������ �ǰ�,
// keyRange �����, �� ������ �����Ǿ� �ִ� predicate�� �켱������ ó����.
# define QMO_PRED_INDEX_NESTED_JOINABLE_MASK  (0x00020000)
# define QMO_PRED_INDEX_NESTED_JOINABLE_FALSE (0x00000000)
# define QMO_PRED_INDEX_NESTED_JOINABLE_TRUE  (0x00020000)

/* qmoPredicate.flag                                  */
// keyRange�� ����� predicate��
// IN���� subqueryKeyRange���� ǥ���ϴ� ����
# define QMO_PRED_INSUBQ_KEYRANGE_MASK        (0x00040000)
# define QMO_PRED_INSUBQ_KEYRANGE_FALSE       (0x00000000)
# define QMO_PRED_INSUBQ_KEYRANGE_TRUE        (0x00040000)

/* qmoPredicate.flag                                  */
// selectivity ��� �ÿ�
// composite index ��� �����Ͽ� selectivity ������ �ʿ��� ���
# define QMO_PRED_USABLE_COMP_IDX_MASK        (0x00080000)
# define QMO_PRED_USABLE_COMP_IDX_FALSE       (0x00000000)
# define QMO_PRED_USABLE_COMP_IDX_TRUE        (0x00080000)

/* qmoPredicate.flag                                  */
// ���� indexable predicate�� ���ؼ� IN(subquery)�� �����ϴ����� ����
// IN(subquery)�� ���ؼ��� keyRange�� �ܵ����� �����ؾ��ϹǷ�,
// �� ������ keyRange ����� ���ȴ�.
# define QMO_PRED_INSUBQUERY_MASK             (0x00100000)
# define QMO_PRED_INSUBQUERY_ABSENT           (0x00000000)
# define QMO_PRED_INSUBQUERY_EXIST            (0x00100000)

/* qmoPredicate.flag                                  */
// joinable selectivity ����,
// �̹� ó���� predicate�� ���� �ߺ� ������ �����ϱ� ���� ����
# define QMO_PRED_SEL_PROCESS_MASK            (0x00200000)
# define QMO_PRED_SEL_PROCESS_FALSE           (0x00000000)
# define QMO_PRED_SEL_PROCESS_TRUE            (0x00200000)

/* qmoPredicate.flag                                  */
// ���� �÷�����Ʈ�� equal, in �񱳿����� ���翩��
# define QMO_PRED_EQUAL_IN_MASK               (0x00400000)
# define QMO_PRED_EQUAL_IN_ABSENT             (0x00000000)
# define QMO_PRED_EQUAL_IN_EXIST              (0x00400000)

/* qmoPredicate.flag                                  */

// view�� ���� push selection�ǰ�, where���� ���� �ִ� predicate������ ����
# define QMO_PRED_PUSH_REMAIN_MASK            (0x00800000)
# define QMO_PRED_PUSH_REMAIN_FALSE           (0x00000000)
# define QMO_PRED_PUSH_REMAIN_TRUE            (0x00800000)

/* qmoPredicate.flag                                  */
// Fix BUG-12934
// constant filter�� ���ؼ���
// subquery�� store and search ����ȭ���� �������� �ʴ´�.
// ��) 1 in subquery ...
#define QMO_PRED_CONSTANT_FILTER_MASK         (0x01000000)
#define QMO_PRED_CONSTANT_FILTER_TRUE         (0x01000000)
#define QMO_PRED_CONSTANT_FILTER_FALSE        (0x00000000)

/* PROJ-1446 Host variable�� ������ ���� ����ȭ */
// ������ host ������ ������ ���� ����ȭ ������ predicate�� ����
// ������ �����Ѵ�.
// �� ������ predicate�� selectivity�� execution�ÿ� �ٽ�
// ���� ��, ����ȭ ���ΰ� ���� predicate�鿡 ���ؼ���
// ���ʿ��� �۾��� ���� �����̴�.
#define QMO_PRED_HOST_OPTIMIZE_MASK           (0x02000000)
#define QMO_PRED_HOST_OPTIMIZE_TRUE           (0x02000000)
#define QMO_PRED_HOST_OPTIMIZE_FALSE          (0x00000000)

/* PROJ-1446 Host variable�� ������ ���� ����ȭ */
// more ����Ʈ �߿� �ϳ��� QMO_PRED_HOST_OPTIMIZE_TRUE��
// ���õǾ� ������ more list�� �� ó�� predicate��
// QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE�� ���õȴ�.
#define QMO_PRED_HEAD_HOST_OPTIMIZE_MASK      (0x04000000)
#define QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE      (0x04000000)
#define QMO_PRED_HEAD_HOST_OPTIMIZE_FALSE     (0x00000000)

// QMO_PRED_HEAD_HOST_OPT_TOTAL_TRUE�� more�� �ΰ���
// predicate�� �ְ� ���� selectivity�� ���� �� �ִ�
// ��츦 �ǹ��Ѵ�. �� ���� setTotal���� ���õȴ�.
#define QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK     (0x08000000)
#define QMO_PRED_HEAD_HOST_OPT_TOTAL_TRUE     (0x08000000)
#define QMO_PRED_HEAD_HOST_OPT_TOTAL_FALSE    (0x00000000)

/* PROJ-1446 Host variable�� ������ ���� ����ȭ */
// qmoPredicate�� mySelectivityOffset�� totalSelectivityOffset�� ����.
#define QMO_SELECTIVITY_OFFSET_NOT_USED           UINT_MAX

// PROJ-1446 Host variable�� ������ ���� ����ȭ
// qmoPredWrapperPool�� pool�� �ʱ� �迭 ũ��
#define QMO_DEFAULT_WRAPPER_POOL_SIZE                 (32)

/* qmoPredicate.flag                                  */
// PROJ-1495
// push_pred hint�� ���� VIEW selection graph ���� push �� predicate
#define QMO_PRED_PUSH_PRED_HINT_MASK          (0x10000000)
#define QMO_PRED_PUSH_PRED_HINT_FALSE         (0x00000000)
#define QMO_PRED_PUSH_PRED_HINT_TRUE          (0x10000000)

/* qmoPredicate.flag                                  */
// PROJ-1404
// transformation���� ������ �� transitive predicate
#define QMO_PRED_TRANS_PRED_MASK              (0x20000000)
#define QMO_PRED_TRANS_PRED_FALSE             (0x00000000)
#define QMO_PRED_TRANS_PRED_TRUE              (0x20000000)

/* BUG-39298 improve preserved order */
#define QMO_PRED_INDEXABLE_EQUAL_MASK          (0x40000000)
#define QMO_PRED_INDEXABLE_EQUAL_TRUE          (0x40000000)
#define QMO_PRED_INDEXABLE_EQUAL_FALSE         (0x00000000)

/* TASK-7219 Non-shard DML */
#define QMO_PRED_PUSHED_FORCE_PRED_MASK        (0x80000000)
#define QMO_PRED_PUSHED_FORCE_PRED_TRUE        (0x80000000)
#define QMO_PRED_PUSHED_FORCE_PRED_FALSE       (0x00000000)

/* qmoPredicate.flag                                  */
// variable predicate�� �Ǵ�
// variable �Ǵ� ����
//   (1) join predicate ����, (2) subquery ����
//   (3) prior column ����  ,
//   fix BUG-10524 (4) SYSDATE column ����
//   (5) procedure variable�� ����
//   (6) host ���� ����
//   To Fix PR-10209
//   (7) Value�� Variable������ �Ǵ�

# define QMO_PRED_IS_VARIABLE( aPredicate )                                \
    (                                                                      \
       (  ( ( (aPredicate)->flag & QMO_PRED_JOIN_PRED_MASK )               \
             == QMO_PRED_JOIN_PRED_TRUE )                                  \
          ||                                                               \
          ( ( (aPredicate)->flag & QMO_PRED_VALUE_MASK )                   \
             == QMO_PRED_VARIABLE )                                        \
          ||                                                               \
          ( QTC_IS_VARIABLE( (aPredicate)->node )                          \
             == ID_TRUE )                                                  \
       )                                                                   \
       ? ID_TRUE : ID_FALSE                                                \
    )

#define QMO_PRED_IS_DYNAMIC_OPTIMIZABLE( aPredicate )                      \
    QTC_IS_DYNAMIC_CONSTANT( (aPredicate)->node )

//---------------------------------------------------
// [subquery�� ���� graph �������θ� �Ǵ�.]
//  1. subquery�� ���� graph �������θ� �Ǵ��ϴ� ������ ������ ����.
//     (1) subquery�����ڰ� subquery�� graph�� �ߺ��������� �ʵ���
//     (2) view ����ȭ �� ����� graph �������� Ȯ��
//  2. subquery�� ���� qcStatement�ڷᱸ���� �����ؼ�,
//     �� �ڷᱸ���� graph�� NULL�� �ƴ����� �˻��Ѵ�.
//---------------------------------------------------

//# define IS_EXIST_GRAPH_IN_SUBQUERY( aSubStatement )
//    ( ( aSubStatement->graph != NULL ) ? ID_TRUE : ID_FALSE )

//---------------------------------------------------
// [subquery�� ���� plan �������� �Ǵ�]
//  1. subquery �����ڰ� subquery�� ���� plan�� �ߺ��������� �ʱ� ����.
//  2. subquery�� ���� qcStatement�ڷᱸ���� �����ؼ�,
//     �� �ڷᱸ���� plan�� NULL�� �ƴ����� �˻��Ѵ�.
//---------------------------------------------------

//# define IS_EXIST_PLAN_IN_SUBQUERY( aSubStatement )
//    ( ( aSubStatement->plan != NULL ) ? ID_TRUE : ID_FALSE )

//---------------------------------------------------
// Predicate�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoPredicate
{

    // filter ordering�� �÷������� diff ������ ����
    // qsort�� compare�Լ������� ����.
    UInt       idx;

    // predicate�� ���� ���� ����
    UInt       flag;

    //-----------------------------------------------
    // [ id ] columnID
    // (1)�� �÷��� ���: smiColum.id
    // (2)LIST: ���ǵ� columnID
    // (3)N/A : ���ǵ� columnID  (N/A=non-indexable)
    //-----------------------------------------------
    UInt       id;

    qtcNode  * node;        // OR�� �񱳿����� ������ predicate


    //-----------------------------------------------
    // [ selectivity ]
    // mySelectivity    : predicate�� ���� selectivity
    // totalSelectivity : qmoPredicate.id�� ���� ��� predicate�� ����
    //                    ��ǥ selectivity.
    //                    qmoPredicate->more�� ���Ḯ��Ʈ ��
    //                    ù��° qmoPredicate���� �� ������ �����ȴ�.
    //-----------------------------------------------
    SDouble    mySelectivity;    // predicate�� ���� selectivity
    SDouble    totalSelectivity; // �÷��� ��ǥ selectivity

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    UInt       mySelectivityOffset;   // ���ε� �� my sel.�� ���ϱ� ����
    UInt       totalSelectivityOffset;// ���ε� �� total sel.�� ���ϱ� ����

    //-----------------------------------------------
    // qmoPredicate ������� �ڷᱸ��
    // next : qmoPredicate.id�� �ٸ� predicate�� ����
    // more : qmoPredicate.id�� ���� predicate�� ����
    //-----------------------------------------------
    qmoPredicate  * next;        // qmoPredicate.id�� �ٸ� predicate�� ����
    qmoPredicate  * more;        // qmoPredicate.id�� ���� predicate�� ����

} qmoPredicate;

//---------------------------------------------------
// qmoPredicate ���������� �����ϱ� ���� �ڷᱸ��
//---------------------------------------------------

// �� join selectivity ��꿡 ���Ե� predicate�� ���������� ���� �ڷᱸ��.
// �̴�, ���õ� join method�� predicate�� ���� ã�� �����̴�.
typedef struct qmoPredInfo
{
    qmoPredicate  * predicate;

    //-----------------------------------------------
    // ������� �ڷᱸ��
    // next : qmoPredicate.id�� �ٸ� predicate�� ����
    // more : qmoPredicate.id�� ���� predicate�� ����
    //-----------------------------------------------
    qmoPredInfo   * next     ;
    qmoPredInfo   * more     ;
} qmoPredInfo;

// PROJ-1446 Host variable�� ������ ���� ����ȭ
// predicate���� ���� ������ �����Ѵ�.
// filter/key range/key filter/subquery filter�� �з��� ��� Ÿ��
typedef struct qmoPredWrapper
{
    qmoPredicate       * pred;
    qmoPredWrapper     * prev;
    qmoPredWrapper     * next;
} qmoPredWrapper;

typedef struct qmoPredWrapperPool
{
    iduVarMemList  * prepareMemory; //fix PROJ-1596
    iduMemory      * executeMemory; //fix PROJ-1436
    qmoPredWrapper   base[QMO_DEFAULT_WRAPPER_POOL_SIZE];
    qmoPredWrapper * current;
    UInt             used;
} qmoPredWrapperPool;

//---------------------------------------------------
// predicate type
//---------------------------------------------------

typedef enum
{
    QMO_KEYRANGE = 0,
    QMO_KEYFILTER,
    QMO_FILTER
} qmoPredType;

// nodeTransform �迭 �Լ����� ����ϴ� ����ü
typedef struct qmoPredTrans
{
    // qmoPredTrans�� ������ �� �ʿ��� �����
    qcStatement  * statement;

    // qmoPredTransI���� ���������� ����ϴ� �����
    SChar          condition[3];
    SChar          logical[4];
    idBool         isGroupComparison;   // BUG-15816

    qmsQuerySet  * myQuerySet; // BUG-18242
} qmoPredTrans;

// qmoPredTrans ����ü�� manage�ϴ� Ŭ����
// ��� static �Լ���� �����Ǿ� �ִ�.
class qmoPredTransI
{
public:

    static void initPredTrans( qmoPredTrans  * aPredTrans,
                               qcStatement   * aStatement,
                               qtcNode       * aCompareNode,
                               qmsQuerySet   * aQuerySet );

    static IDE_RC makeConditionNode( qmoPredTrans * aPredTrans,
                                     qtcNode      * aArgumentNode,
                                     qtcNode     ** aResultNode );

    static IDE_RC makeLogicalNode( qmoPredTrans * aPredTrans,
                                   qtcNode      * aArgumentNode,
                                   qtcNode     ** aResultNode );

    static IDE_RC makeAndNode( qmoPredTrans * aPredTrans,
                               qtcNode      * aArgumentNode,
                               qtcNode     ** aResultNode );

    static IDE_RC copyNode( qmoPredTrans * aPredTrans,
                            qtcNode      * aNode,
                            qtcNode     ** aResultNode );

    static IDE_RC makeSubQWrapperNode( qmoPredTrans * aPredTrans,
                                       qtcNode      * aValueNode,
                                       qtcNode     ** aResultNode );

    static IDE_RC makeIndirectNode( qmoPredTrans * aPredTrans,
                                    qtcNode      * aValueNode,
                                    qtcNode     ** aResultNode );

private:
    static IDE_RC makePredNode( qcStatement  * aStatement,
                                qtcNode      * aArgumentNode,
                                SChar        * aOperator,
                                qtcNode     ** aResultNode );

    static IDE_RC estimateConditionNode( qmoPredTrans * aPredTrans,
                                         qtcNode      * aConditionNode );

    static IDE_RC estimateLogicalNode( qmoPredTrans * aPredTrans,
                                       qtcNode      * aLogicalNode );

    static void setLogicalNCondition( qmoPredTrans * aPredTrans,
                                      qtcNode      * aCompareNode );
};

class qmoPredWrapperI
{
public:


    static IDE_RC initializeWrapperPool( iduVarMemList      * aMemory, //fix PROJ-1596
                                         qmoPredWrapperPool * aWrapperPool );

    static IDE_RC initializeWrapperPool( iduMemory          * aMemory,
                                         qmoPredWrapperPool * aWrapperPool );

    static IDE_RC createWrapperList( qmoPredicate       * aPredicate,
                                     qmoPredWrapperPool * aWrapperPool,
                                     qmoPredWrapper    ** aResult );

    static IDE_RC extractWrapper( qmoPredWrapper  * aTarget,
                                  qmoPredWrapper ** aFrom );

    static IDE_RC moveAll( qmoPredWrapper ** aFrom,
                           qmoPredWrapper ** aTo );

    static IDE_RC moveWrapper( qmoPredWrapper  * aTarget,
                               qmoPredWrapper ** aFrom,
                               qmoPredWrapper ** aTo );

    static IDE_RC addTo( qmoPredWrapper  * aTarget,
                         qmoPredWrapper ** aTo );

    static IDE_RC addPred( qmoPredicate       * aPredicate,
                           qmoPredWrapper    ** aWrapperList,
                           qmoPredWrapperPool * aWrapperPool );

private:

    static IDE_RC newWrapper( qmoPredWrapperPool * aWrapperPool,
                              qmoPredWrapper    ** aNewWrapper );
};

//---------------------------------------------------
// Predicate�� �����ϱ� ���� �Լ�
//---------------------------------------------------
class qmoPred
{
public:

    //-----------------------------------------------
    // Graph ���� ȣ���ϴ� �Լ�
    //-----------------------------------------------

    // PROJ-1502 PARTITIONED DISK TABLE
    // composite key�� ���õ� flag�� predicate�� ������.
    // �����÷�����Ʈ�� equal(IN) ������ ���翩�� ����
    // �������� Ű �÷� ��밡�ɿ��� ����
    static void setCompositeKeyUsableFlag( qmoPredicate * aPredicate );

    // selection graph�� predicate ���ġ
    static IDE_RC  relocatePredicate( qcStatement     * aStatement,
                                      qmoPredicate    * aInPredicate,
                                      qcDepInfo       * aTableDependencies,
                                      qcDepInfo       * aOuterDependencies,
                                      qmoStatistics   * aStatiscalData,
                                      qmoPredicate   ** aOutPredicate );

    // partition graph�� predicate���ġ
    static IDE_RC  relocatePredicate4PartTable(
        qcStatement        * aStatement,
        qmoPredicate       * aInPredicate,
        qcmPartitionMethod   aPartitionMethod,
        qcDepInfo          * aTableDependencies,
        qcDepInfo          * aOuterDependencies,
        qmoPredicate      ** aOutPredicate );

    // Join Predicate�� ���� �з�
    static IDE_RC  classifyJoinPredicate( qcStatement  * aStatement,
                                          qmoPredicate * aPredicate,
                                          qmgGraph     * aLeftChildGraph,
                                          qmgGraph     * aRightChildGraph,
                                          qcDepInfo    * aFromDependencies );

    // Index Nested Loop Join�� push down joinable predicate�� ����.
    static IDE_RC  makeJoinPushDownPredicate( qcStatement    * aStatement,
                                              qmoPredicate   * aNewPredicate,
                                              qcDepInfo      * aRightDependencies );

    // Index Nested Loop Join�� push down non-joinable prediate
    static IDE_RC  makeNonJoinPushDownPredicate( qcStatement   * aStatement,
                                                 qmoPredicate  * aNewPredicate,
                                                 qcDepInfo     * aRightDependencies,
                                                 UInt            aDirection );

    // PROJ-1405
    // rownum predicate�� �Ǵ�
    static IDE_RC  isRownumPredicate( qmoPredicate  * aPredicate,
                                      idBool        * aIsTrue );

    // constant predicate�� �Ǵ�
    static IDE_RC  isConstantPredicate( qmoPredicate  * aPredicate,
                                        qcDepInfo     * aFromDependencies,
                                        idBool        * aIsTrue );

    // one table predicate�� �Ǵ�
    static IDE_RC  isOneTablePredicate( qmoPredicate  * aPredicate,
                                        qcDepInfo     * aFromDependencies,
                                        qcDepInfo     * aTableDependencies,
                                        idBool        * aIsTrue );

    // To Fix PR-11460, PR-11461
    // Subquery Predicate�� ���� ����ȭ �� �Ϲ� Table����
    // In Subquery KeyRange �� Subquery KeyRange ����ȭ�� �����ϴ�.
    // �̸� �����Ѵ� [aTryKeyRange]

    // predicate�� �����ϴ� ��� subquery�� ���� ó��
    static IDE_RC  optimizeSubqueries( qcStatement  * aStatement,
                                       qmoPredicate * aPredicate,
                                       idBool         aTryKeyRange );

    // qtcNode�� �����ϴ� ��� subquery�� ���� ó��
    static IDE_RC  optimizeSubqueryInNode( qcStatement * aStatement,
                                           qtcNode     * aNode,
                                           idBool        aTryKeyRange,
                                           idBool        aConstantPred );

    // joinable predicate�� non joinable predicate�� �и��Ѵ�.
    static IDE_RC  separateJoinPred( qmoPredicate  * aPredicate,
                                     qmoPredInfo   * aJoinablePredInfo,
                                     qmoPredicate ** aJoinPred,
                                     qmoPredicate ** aNonJoinPred );

    // Join predicate�� ���� columnID�� qmoPredicate.id�� ����.
    static IDE_RC  setColumnIDToJoinPred( qcStatement  * aStatement,
                                          qmoPredicate * aPredicate,
                                          qcDepInfo    * aDependencies );

    // Join predicate�� ���� column node�� ��ȯ.
    static qtcNode* getColumnNodeOfJoinPred( qcStatement  * aStatement,
                                             qmoPredicate * aPredicate,
                                             qcDepInfo    * aDependencies );
    
    // Predicate�� Indexable ���� �Ǵ�
    static IDE_RC  isIndexable( qcStatement   * aStatement,
                                qmoPredicate  * aPredicate,
                                qcDepInfo     * aTableDependencies,
                                qcDepInfo     * aOuterDependencies,
                                idBool        * aIsIndexable );

    static IDE_RC createPredicate( iduVarMemList * aMemory, //fix PROJ-1596
                                   qtcNode       * aNode,
                                   qmoPredicate ** aNewPredicate );

    // PROJ-1502 PARTITIONED DISK TABLE
    // Predicate�� Partition Prunable ���� �Ǵ�
    static IDE_RC  isPartitionPrunable(
        qcStatement        * aStatement,
        qmoPredicate       * aPredicate,
        qcmPartitionMethod   aPartitionMethod,
        qcDepInfo          * aTableDependencies,
        qcDepInfo          * aOuterDependencies,
        idBool             * aIsPartitionPrunable );

    // PROJ-1405 ROWNUM
    // rownum predicate�� stopkey predicate�� filter predicate�� �и��Ѵ�.
    static IDE_RC  separateRownumPred( qcStatement   * aStatement,
                                       qmsQuerySet   * aQuerySet,
                                       qmoPredicate  * aPredicate,
                                       qmoPredicate ** aStopkeyPred,
                                       qmoPredicate ** aFilterPred,
                                       SLong         * aStopRecordCount );

    //-----------------------------------------------
    // Plan ���� ȣ���ϴ� �Լ�
    //-----------------------------------------------

    // ROWNUM column�� �Ǵ�
    static idBool  isROWNUMColumn( qmsQuerySet  * aQuerySet,
                                   qtcNode      * aNode );

    // prior column�� ���� �Ǵ�
    static IDE_RC  setPriorNodeID( qcStatement  * aStatement,
                                   qmsQuerySet  * aQuerySet ,
                                   qtcNode      * aNode       );

    // keyRange, keyFilter�� ����� qmoPredicate�� ����������,
    // �ϳ��� qtcNode�� �׷����� ���������� �����.
    static IDE_RC  linkPredicate( qcStatement   * aStatement,
                                  qmoPredicate  * aPredicate,
                                  qtcNode      ** aNode       );

    // filter�� ����� qmoPredicate�� ����������,
    // �ϳ��� qtcNode�� �׷����� ���������� �����.
    static IDE_RC  linkFilterPredicate( qcStatement   * aStatement,
                                        qmoPredicate  * aPredicate,
                                        qtcNode      ** aNode      );

    // BUG-38971 subQuery filter �� ������ �ʿ䰡 �ֽ��ϴ�.
    static IDE_RC  sortORNodeBySubQ( qcStatement   * aStatement,
                                     qtcNode       * aNode );

    // BUG-35155 Partial CNF
    // linkFilterPredicate ���� ������� qtcNode �׷쿡 NNF ���͸� �߰��Ѵ�.
    static IDE_RC addNNFFilter4linkedFilter( qcStatement   * aStatement,
                                             qtcNode       * aNNFFilter,
                                             qtcNode      ** aNode      );

    static IDE_RC makeRangeAndFilterPredicate( qcStatement   * aStatement,
                                               qmsQuerySet   * aQuerySet,
                                               idBool          aIsMemory,
                                               qmoPredicate  * aPredicate,
                                               qcmIndex      * aIndex,
                                               qmoPredicate ** aKeyRange,
                                               qmoPredicate ** aKeyFilter,
                                               qmoPredicate ** aFilter,
                                               qmoPredicate ** aLobFilter,
                                               qmoPredicate ** aSubqueryFilter );

    // keyRange, keyFilter, filter,
    // outerFilter, outerMtrFilter,
    // subqueryFilter ���� �Լ�
    static IDE_RC  extractRangeAndFilter( qcStatement        * aStatement,
                                          qcTemplate         * aTemplate,
                                          idBool               aIsMemory,
                                          idBool               aInExecutionTime,
                                          qcmIndex           * aIndex,
                                          qmoPredicate       * aPredicate,
                                          qmoPredWrapper    ** aKeyRange,
                                          qmoPredWrapper    ** aKeyFilter,
                                          qmoPredWrapper    ** aFilter,
                                          qmoPredWrapper    ** aLobFilter,
                                          qmoPredWrapper    ** aSubqueryFilter,
                                          qmoPredWrapperPool * aWrapperPool );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition key range predicate�� �����ϰ�,
    // subquery filter predicate�� �����Ѵ�.
    // ������ predicate�鵵 ���� �з��Ѵ�.
    static IDE_RC makePartKeyRangePredicate(
        qcStatement        * aStatement,
        qmsQuerySet        * aQuerySet,
        qmoPredicate       * aPredicate,
        qcmColumn          * aPartKeyColumns,
        qcmPartitionMethod   aPartitionMethod,
        qmoPredicate      ** aPartKeyRange );

    static IDE_RC makePartFilterPredicate(
        qcStatement        * aStatement,
        qmsQuerySet        * aQuerySet,
        qmoPredicate       * aPredicate,
        qcmColumn          * aPartKeyColumns,
        qcmPartitionMethod   aPartitionMethod,
        qmoPredicate      ** aPartFilter,
        qmoPredicate      ** aRemain,
        qmoPredicate      ** aSubqueryFilter );

    // extractRangeAndFilter ȣ���� �� ���ĸ� ȣ���ϸ�
    // ���� predicate���� ���ġ�ȴ�.
    static IDE_RC fixPredToRangeAndFilter( qcStatement        * aStatement,
                                           qmsQuerySet        * aQuerySet,
                                           qmoPredWrapper    ** aKeyRange,
                                           qmoPredWrapper    ** aKeyFilter,
                                           qmoPredWrapper    ** aFilter,
                                           qmoPredWrapper    ** aLobFilter,
                                           qmoPredWrapper    ** aSubqueryFilter,
                                           qmoPredWrapperPool * aWrapperPool );

    // columnID�� ����
    static IDE_RC    getColumnID( qcStatement  * aStatement,
                                  qtcNode      * aNode,
                                  idBool         aIsIndexable,
                                  UInt         * aColumnID );

    // LIST �÷�����Ʈ�� �ε��� ��뿩�� �˻�
    static IDE_RC    checkUsableIndex4List( qcTemplate   * aTemplate,
                                            qcmIndex     * aIndex,
                                            qmoPredicate * aPredicate,
                                            qmoPredType  * aPredType );

    // PROJ-1502 PARTITIONED DISK TABLE
    // LIST �÷�����Ʈ�� partition key ��뿩�� �˻�
    static IDE_RC    checkUsablePartitionKey4List( qcStatement  * aStatement,
                                                   qcmColumn    * aPartKeyColumns,
                                                   qmoPredicate * aPredicate,
                                                   qmoPredType  * aPredType );


    // �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
    static IDE_RC findChildGraph( qtcNode   * aCompareNode,
                                  qcDepInfo * aFromDependencies,
                                  qmgGraph  * aGraph1,
                                  qmgGraph  * aGraph2,
                                  qmgGraph ** aLeftColumnGraph,
                                  qmgGraph ** aRightColumnGraph );

    // column�� value�� ���� �迭�� data type������ �Ǵ�
    static idBool isSameGroupType( qcTemplate  * aTemplate,
                                   qtcNode     * aColumn,
                                   qtcNode     * aValue );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // filter, key filter, key range�� �����ϱ� ���ؼ�
    // predicate�� �ʿ��ѵ�, �̵��� ��������� ��������
    // predicate ���ᱸ���� qtcNode���� �������� �ٲ�� ������
    // filter���� �ι� ������ �� ���� ������ �Ǿ� �ִ�.
    // ȣ��Ʈ ������ ���� filter���� �ĺ��� ������ ���� �ʿ䰡 �ִµ�
    // �̸� ���� graph�� predicate�� ���縦 �ؼ� ����ؾ� �Ѵ�.
    static IDE_RC deepCopyPredicate( iduVarMemList * aMemory, //fix PROJ-1596
                                     qmoPredicate  * aSource,
                                     qmoPredicate ** aResult );

    // PROJ-1502 PARTITIONED DISK TABLE
    // �� ��Ƽ�� ���� ����ȭ�� �����ϱ� ���� predicate�� �����Ѵ�.
    // �� ��, subquery�� �����ϰ� �����Ѵ�.
    // partitioned table�� ��带 partition�� ���� ��ȯ�Ѵ�.
    static IDE_RC copyPredicate4Partition(
        qcStatement   * aStatement,
        qmoPredicate  * aSource,
        qmoPredicate ** aResult,
        UShort          aSourceTable,
        UShort          aDestTable,
        idBool          aCopyVariablePred );

    // PROJ-1502 PARTITIONED DISK TABLE
    // �ϳ��� predicate�� �����Ѵ�.
    static IDE_RC copyOnePredicate4Partition(
        qcStatement   * aStatement,
        qmoPredicate  * aSource,
        qmoPredicate ** aResult,
        UShort          aSourceTable,
        UShort          aDestTable );

    // �������� qmoPredicate ���� �۾��� �����Ѵ�.
    // ������ qtcNode�� �����Ѵ�.
    static IDE_RC copyOnePredicate( iduVarMemList * aMemory, //fix PROJ-1596
                                    qmoPredicate  * aSource,
                                    qmoPredicate ** aResult );

    // fix BUG-19211
    static IDE_RC copyOneConstPredicate( iduVarMemList * aMemory,
                                         qmoPredicate  * aSource,
                                         qmoPredicate ** aResult );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // �־��� predicate�� next, more���� Ž���ϸ鼭,
    // host ����ȭ�� �������� �˻��Ѵ�.
    static idBool checkPredicateForHostOpt( qmoPredicate * aPredicate );

    // �÷����� ������� �����
    static IDE_RC    linkPred4ColumnID( qmoPredicate * aDestPred,
                                        qmoPredicate * aSourcePred );

    // ���ġ�� �Ϸ�� �� indexable column�� ���� IN(subquery)�� ���� ó��
    static IDE_RC  processIndexableInSubQ( qmoPredicate ** aPredicate );

    // PROJ-1404
    // predicate���� ������ transitive predicate�� �����Ѵ�.
    static IDE_RC  removeTransitivePredicate( qmoPredicate ** aPredicate,
                                              idBool          aOnlyJoinPred );

    // PROJ-1404
    // ������ transitive predicate�� �������� �ߺ��� predicate�� �ִ� ���
    // �����Ѵ�.
    static IDE_RC  removeEquivalentTransitivePredicate( qcStatement   * aStatement,
                                                        qmoPredicate ** aPredicate );

    // indexable predicate �Ǵܽ�, �÷��� ���ʿ��� �����ϴ����� �Ǵ�
    // (����4�� �Ǵ�)
    static IDE_RC isExistColumnOneSide( qcStatement * aStatement,
                                        qtcNode     * aNode,
                                        qcDepInfo   * aTableDependencies,
                                        idBool      * aIsIndexable );

    // BUG-39403
    static idBool hasOnlyColumnCompInPredList( qmoPredInfo * aJoinPredList );

    /* TASK-7219 */
    static IDE_RC isValidPushDownPredicate( qcStatement  * aStatement,
                                            qmgGraph     * aGraph,
                                            qmoPredicate * aPredicate,
                                            idBool       * aIsValid );

    /* TASK-7219 Non-shard DML */
    static void setOutRefColumnForQtcNode( qtcNode   * aNode,
                                           qcDepInfo * aPredicateDepInfo );
private:

    //-----------------------------------------------
    // Base Table�� ���� ó��
    //-----------------------------------------------

    // Base Table�� Predicate�� ���� �з�
    static IDE_RC  classifyTablePredicate( qcStatement   * aStatement,
                                           qmoPredicate  * aPredicate,
                                           qcDepInfo     * aTableDependencies,
                                           qcDepInfo     * aOuterDependencies,
                                           qmoStatistics * aStatiscalData );

    static IDE_RC  classifyPartTablePredicate( qcStatement      * aStatement,
                                               qmoPredicate     * aPredicate,
                                               qcmPartitionMethod aPartitionMethod,
                                               qcDepInfo        * aTableDependencies,
                                               qcDepInfo        * aOuterDependencies );


    // �ϳ��� �񱳿����� ��忡 ���� indexable ���� �Ǵ�
    static IDE_RC  isIndexableUnitPred( qcStatement * aStatement,
                                        qtcNode     * aNode,
                                        qcDepInfo   * aTableDependencies,
                                        qcDepInfo   * aOuterDependencies,
                                        idBool      * aIsIndexable );

    // PROJ-1502 PARTITIONED DISK TABLE
    // �ϳ��� �񱳿����� ��忡 ���� Partition Prunable ���� �Ǵ�
    static IDE_RC  isPartitionPrunableOnePred(
        qcStatement        * aStatement,
        qtcNode            * aNode,
        qcmPartitionMethod   aPartitionMethod,
        qcDepInfo          * aTableDependencies,
        qcDepInfo          * aOuterDependencies,
        idBool             * aIsPartitionPrunable );

    // indexable predicate �Ǵܽ�,
    // column�� value�� ���ϰ迭�� ���ϴ� data type������ �˻�
    // (����5�� �Ǵ�)
    static IDE_RC checkSameGroupType( qcStatement * aStatement,
                                      qtcNode     * aNode,
                                      idBool      * aIsIndexable );

    // indexable predicate �Ǵܽ�, value�� ���� ���� �˻� (����6�� �Ǵ�)
    static IDE_RC isIndexableValue( qcStatement * aStatement,
                                    qtcNode     * aNode,
                                    qcDepInfo   * aOuterDependencies,
                                    idBool      * aIsIndexable );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition prunable predicate �Ǵܽ�, value�� ���� ���� �˻� (����6�� �Ǵ�)
    static IDE_RC isPartitionPrunableValue( qcStatement * aStatement,
                                            qtcNode     * aNode,
                                            qcDepInfo   * aOuterDependencies,
                                            idBool      * aIsPartitionPrunable );

    // LIKE �������� index ��� ���� ���� �Ǵ�
    static IDE_RC  isIndexableLIKE( qcStatement  * aStatement,
                                    qtcNode      * aNode,
                                    idBool       * aIsIndexable );

    //-----------------------------------------------
    // Join �� ���� ó��
    //-----------------------------------------------

    // joinable predicate�� �Ǵ�
    static IDE_RC isJoinablePredicate( qmoPredicate * aPredicate,
                                       qcDepInfo    * aFromDependencies,
                                       qcDepInfo    * aLeftChildDependencies,
                                       qcDepInfo    * aRightChildDependencies,
                                       idBool       * aIsOnlyIndexNestedLoop );

    // �ϳ��� �񱳿����ڿ� ���� joinable predicate �Ǵ�
    static IDE_RC isJoinableOnePred( qtcNode     * aNode,
                                     qcDepInfo   * aFromDependencies,
                                     qcDepInfo   * aLeftChildDependencies,
                                     qcDepInfo   * aRightChildDependencies,
                                     idBool      * aIsJoinable );

    // indexable join predicate�� �Ǵ�
    static IDE_RC  isIndexableJoinPredicate( qcStatement  * aStatement,
                                             qmoPredicate * aPredicate,
                                             qmgGraph     * aLeftChildGraph,
                                             qmgGraph     * aRightChildGraph );

    // sort joinable predicate�� �Ǵ� ( key size limit �˻� )
    static IDE_RC  isSortJoinablePredicate( qmoPredicate * aPredicate,
                                            qtcNode      * aNode,
                                            qcDepInfo    * aFromDependencies,
                                            qmgGraph     * aLeftChildGraph,
                                            qmgGraph     * aRightChildGraph );

    // hash joinable predicate�� �Ǵ� ( key size limit �˻� )
    static IDE_RC isHashJoinablePredicate( qmoPredicate * aPredicate,
                                           qtcNode      * aNode,
                                           qcDepInfo    * aFromDependencies,
                                           qmgGraph     * aLeftChildGraph,
                                           qmgGraph     * aRightChildGraph );

    // merge joinable predicate�� �Ǵ� ( ���⼺ �˻� )
    static IDE_RC isMergeJoinablePredicate( qmoPredicate * aPredicate,
                                            qtcNode      * aNode,
                                            qcDepInfo    * aFromDependencies,
                                            qmgGraph     * aLeftChildGraph,
                                            qmgGraph     * aRightChildGraph );

    //-----------------------------------------------
    // ��Ÿ
    //-----------------------------------------------

    // keyRange ����
    static IDE_RC  extractKeyRange( qcmIndex        * aIndex,
                                    qmoPredWrapper ** aSource,
                                    qmoPredWrapper ** aKeyRange,
                                    qmoPredWrapper ** aKeyFilter,
                                    qmoPredWrapper ** aSubqueryFilter );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition keyrange ����
    static IDE_RC  extractPartKeyRangePredicate(
        qcStatement         * aStatement,
        qmoPredicate        * aPredicate,
        qcmColumn           * aPartKeyColumns,
        qcmPartitionMethod    aPartitionMethod,
        qmoPredWrapper     ** aPartKeyRange,
        qmoPredWrapper     ** aRemain,
        qmoPredWrapper     ** aLobFilter,
        qmoPredWrapper     ** aSubqueryFilter,
        qmoPredWrapperPool  * aWrapperPool );

    // one column�� ���� keyRange ���� �Լ�
    static IDE_RC  extractKeyRange4Column( qcmIndex        * aIndex,
                                           qmoPredWrapper ** aSource,
                                           qmoPredWrapper ** aKeyRange );

    // PROJ-1502 PARTITIONED DISK TABLE
    // one column�� ���� partition keyRange ���� �Լ�
    static IDE_RC  extractPartKeyRange4Column( qcmColumn        * aPartKeyColumns,
                                               qcmPartitionMethod aPartitionMethod,
                                               qmoPredWrapper  ** aSource,
                                               qmoPredWrapper  ** aPartKeyRange );


    // keyFilter ����
    static IDE_RC extractKeyFilter( idBool            aIsMemory,
                                    qcmIndex        * aIndex,
                                    qmoPredWrapper ** aSource,
                                    qmoPredWrapper ** aKeyRange,
                                    qmoPredWrapper ** aKeyFilter,
                                    qmoPredWrapper ** aFilter );

    // one column�� ���� keyRange, keyFilter, filter, subqueryFilter ���� �Լ�
    static IDE_RC  extractKeyFilter4Column( qcmIndex        * aIndex,
                                            qmoPredWrapper ** aSource,
                                            qmoPredWrapper ** aKeyFilter );

    // LIST�� ���� keyRange, keyFilter, filter, subqueryFilter ���� �Լ�
    static IDE_RC  extractRange4LIST( qcTemplate         * aTemplate,
                                      qcmIndex           * aIndex,
                                      qmoPredWrapper    ** aSouece,
                                      qmoPredWrapper    ** aKeyRange,
                                      qmoPredWrapper    ** aKeyFilter,
                                      qmoPredWrapper    ** aFilter,
                                      qmoPredWrapper    ** aSubQFilterList,
                                      qmoPredWrapperPool * aWrapperPool );

    // PROJ-1502 PARTITIONED DISK TABLE
    // LIST�� ���� keyRange, keyFilter, filter, subqueryFilter ���� �Լ�
    static IDE_RC  extractPartKeyRange4LIST( qcStatement        * aStatement,
                                             qcmColumn          * aPartKeyColumns,
                                             qmoPredWrapper    ** aSouece,
                                             qmoPredWrapper    ** aKeyRange,
                                             qmoPredWrapperPool * aWrapperPool );


    // keyRange�� keyFilter�� �з��� predicate�� ���ؼ�
    // quantify �񱳿����ڿ� ���� ��庯ȯ, like�񱳿������� filter����
    static IDE_RC  process4Range( qcStatement        * aStatement,
                                  qmsQuerySet        * aQuerySet,
                                  qmoPredicate       * aRange,
                                  qmoPredWrapper    ** aFilter,
                                  qmoPredWrapperPool * aWrapperPool );

    // qmoPredicate ����Ʈ���� filter subqueryFilter�� �и��Ѵ�.
    static IDE_RC separateFilters( qcTemplate         * aTemplate,
                                   qmoPredWrapper     * aSource,
                                   qmoPredWrapper    ** aFilter,
                                   qmoPredWrapper    ** aLobFilter,
                                   qmoPredWrapper    ** aSubqueryFilter,
                                   qmoPredWrapperPool * aWrapperPool );

    // keyRange/keyFilter�� ����� preidcate�� ���Ḯ��Ʈ �� �����ֻ�ܿ�
    // fixed/variable/InSubquery-keyRange�� ���� ������ �����Ѵ�.
    static IDE_RC setRangeInfo( qmsQuerySet  * aQuerySet,
                                qmoPredicate * aPredicate );

    // To Fix PR-9679
    // LIKE predicate�� ���� Range�� �����ϴ���
    // Filter�� �ʿ��� ��쿡 ���� filter�� �����.
    static IDE_RC makeFilterNeedPred( qcStatement   * aStatement,
                                      qmoPredicate  * aPredicate,
                                      qmoPredicate ** aLikeFilterPred );



    // keyRange ������ ���� ��� ��ȯ(quantify �񱳿����ڿ� LIST)
    static IDE_RC nodeTransform( qcStatement  * aStatement,
                                 qtcNode      * aSourceNode,
                                 qtcNode     ** aTransformNode,
                                 qmsQuerySet  * aQuerySet );

    // OR ��� ���� �ϳ��� �񱳿����� ��� ������ ��� ��ȯ
    static IDE_RC nodeTransform4OneCond( qcStatement * aStatement,
                                         qtcNode     * aCompareNode,
                                         qtcNode    ** aTransformNode,
                                         qmsQuerySet * aQuerySet );

    // �ϳ��� list value node ��ȯ
    static IDE_RC nodeTransform4List( qmoPredTrans * aPredTrans,
                                      qtcNode      * aColumnNode,
                                      qtcNode      * sValueNode,
                                      qtcNode     ** aTransformNode );

    // (i1, i2) in (1,2) ���� ���
    static IDE_RC nodeTransformListColumnListValue( qmoPredTrans * aPredTrans,
                                                    qtcNode      * aColumnNode,
                                                    qtcNode      * aValueNode,
                                                    qtcNode     ** aTransformNode );

    // (i1, i2) in (1,2) ���� ���
    static IDE_RC nodeTransformOneColumnListValue( qmoPredTrans * aPredTrans,
                                                   qtcNode      * aColumnNode,
                                                   qtcNode      * aValueNode,
                                                   qtcNode     ** aTransformNode );

    // �ϳ��� node�� ���� ��ȯ
    static IDE_RC nodeTransformOneNode( qmoPredTrans * aPredTrans,
                                        qtcNode      * aColumnNode,
                                        qtcNode      * aValueNode,
                                        qtcNode     ** aTransfromNode );

    // subquery list�� ���� ��ȯ
    static IDE_RC nodeTransform4OneRowSubQ( qmoPredTrans * aPredTrans,
                                            qtcNode      * aColumnNode,
                                            qtcNode      * aValueNode,
                                            qtcNode     ** aTransformNode );

    // value node�� SUBQUERY �� ����� ��� ��ȯ
    static IDE_RC nodeTransform4SubQ( qmoPredTrans * aPredTrans,
                                      qtcNode      * aCompareNode,
                                      qtcNode      * aColumnNode,
                                      qtcNode      * aValueNode,
                                      qtcNode     ** aTransfromNode );

    // indexArgument ����
    static IDE_RC    setIndexArgument( qtcNode      * aNode,
                                       qcDepInfo    * aDependencies );

    // IN(subquery) �Ǵ� subquery keyRange ����ȭ �� ���� predicate��
    // keyRange�� ������� ���ϰ�, filter�� ������ ���,
    // �����Ǿ� �ִ� IN(subquery)����ȭ �� ������ ����
    static IDE_RC   removeIndexableSubQTip( qtcNode     * aNode );

    // Filter�� ���� indexable ������ �����Ѵ�.
    static IDE_RC removeIndexableFromFilter( qmoPredWrapper * aFilter );

    // qmoPredWrapper�� ���� ������� qmoPredicate�� ���� ������ �缳���Ѵ�.
    static IDE_RC relinkPredicate( qmoPredWrapper * aWrapper );

    // more�� ������� ���´�.
    static IDE_RC removeMoreConnection( qmoPredWrapper * aWrapper,
                                        idBool aIfOnlyList );

    // �Ʒ� �� �Լ��� deepCopyPredicate()�κ��� �Ļ��� ���� �Լ����̴�.
    // linkToMore�� aLast�� more�� aNew�� �ް�,
    // linkToNext�� aLast�� next�� aNew�� �ܴ�.
    static void linkToMore( qmoPredicate **aHead,
                            qmoPredicate **aLast,
                            qmoPredicate *aNew );

    static void linkToNext( qmoPredicate **aHead,
                            qmoPredicate **aLast,
                            qmoPredicate *aNew );

    // PROJ-1405 ROWNUM
    static IDE_RC isStopkeyPredicate( qcStatement  * aStatement,
                                      qmsQuerySet  * aQuerySet,
                                      qtcNode      * aNode,
                                      idBool       * aIsStopkey,
                                      SLong        * aStopRecordCount );

    // BUG-39403
    static idBool hasOnlyColumnCompInPredNode( qtcNode * aNode );
};

#endif /* _O_QMO_PREDICATE_H_ */
