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
 * $Id: qmoCnfMgr.h 86122 2019-09-04 07:20:21Z donovan.seo $
 *
 * Description :
 *     CNF Critical Path Manager
 *
 *     CNF Normalized Form�� ���� ����ȭ�� �����ϰ�
 *     �ش� Graph�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMO_CNF_MGR_H_
#define _O_QMO_CNF_MGR_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoPredicate.h>

//---------------------------------------------------------
// [PROJ-1352] JOIN SELECTIVITY THRESHOLD
//
// Join Selectivity�� ���� �۾�����, join selectivity������
// ���ϴ� ���� ������ �ִ� (TPC-H 21).
// �̸� �����ϱ� ���Ͽ� Ư�� threshold ������ ��쿡 ���ؼ���
// Output record count�� �ݿ��Ͽ��� �Ѵ�.
//
// Join Selectivity�� �ǹ� �ľ�
//    [0.5]  : 100 JOIN 100 ==> 100
//             WHERE T1.i1 = T2.i1 �� ����
//             T1.i1 �� T2.i1�� �ߺ��� ����, 1 : 1 �� ���εǴ� �����
//             ���� ȿ������ ���� ������ �ִ� �����
//    [0.1]  : 100 JOIN 100 ==> 20
//             WHERE T1.i1 = T2.i1 AND T1.i2 > ? �� ����
//             [0.5] �� ����� ���ǿ� ���� �κ�(1/3~1/5)
//             �ɷ� �ִ� ������ �߰��� �����.
//    [0.05] : 100 JOIN 100 ==> 10
//             WHERE T1.i1 = T2.i1 AND T1.I2 = ? �� ����
//             [0.5] �� ����� ���ǿ� ��� �κ�(1/10)��
//             �ɷ� �ִ� ������ �߰��� �����
//---------------------------------------------------------
// PROJ-1718 Semi/anti join�� �߰��� ��� 0.1�� �� ������ �� ����.
#define QMO_JOIN_SELECTIVITY_THRESHOLD           (0.1)

// PROJ-1718 Subquery unnesting
enum qmoJoinRelationType
{
    QMO_JOIN_RELATION_TYPE_INNER = 0,
    QMO_JOIN_RELATION_TYPE_SEMI,
    QMO_JOIN_RELATION_TYPE_ANTI
};

//------------------------------------------------------
// Table Order ������ ǥ���ϱ� ���� �ڷ� ����
//------------------------------------------------------
typedef struct qmoTableOrder
{
    qcDepInfo       depInfo;
    qmoTableOrder * next;
} qmoTableOrder;

/* BUG-41343 */
typedef enum qmoPushApplicableType
{
    QMO_PUSH_APPLICABLE_FALSE = 0,
    QMO_PUSH_APPLICABLE_TRUE,
    QMO_PUSH_APPLICABLE_LEFT,
    QMO_PUSH_APPLICABLE_RIGHT
} qmoPushApplicableType;

//-------------------------------------------------------
// Join Relation�� ��Ÿ���� ���� �ڷ� ����
//-------------------------------------------------------

typedef struct qmoJoinRelation
{
    qcDepInfo             depInfo;
    qmoJoinRelation     * next;

    // PROJ-1718 Subquery unnesting
    // Semi/anti join�� ��� join type�� ������ ��Ÿ����.
    qmoJoinRelationType   joinType;
    UShort                innerRelation;
} qmoJoinRelation;

//----------------------------------------------------
// Join Group�� �����ϱ� ���� �ڷ� ����
//----------------------------------------------------

typedef struct qmoJoinGroup
{
    qmoPredicate        * joinPredicate;    // join predicate

    // join ���迡 �ִ� table���� dependencies�� ORing �� ��
    qmoJoinRelation     * joinRelation;     // linked list
    UInt                  joinRelationCnt;  // join graph ���� �� �ʿ�

    // join group ���� ��� table���� dependencies�� ORing �� ��
    qcDepInfo             depInfo;

    // join ordering�� ���� ��, top graph�� pointing
    qmgGraph            * topGraph;

    //--------------------------------------------------------
    // Base Graph ���� �ڷ� ����
    //-------------------------------------------------------
    UInt                  baseGraphCnt;    // baseGraph ����
    qmgGraph           ** baseGraph;       // baseGraph pointer ������ �迭
    idBool                mIsOnlyNL;
} qmoJoinGroup;


//---------------------------------------------------
// CNF Critical Path�� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmoCNF
{
    qtcNode       * normalCNF;   // where���� CNF�� normalize�� ���
    qtcNode       * nnfFilter;   // NNF Filter�� ����, PR-12743

    qmsQuerySet   * myQuerySet;  // �ش� query set�� ����Ŵ
    qmgGraph      * myGraph;     // CNF Form ��� graph�� top
    SDouble         cost;        // CNF Total Cost

    // from�� dependencies ���� �����ؼ� ������.
    qcDepInfo       depInfo;

    //-------------------------------------------------------------------
    // Predicate ���� �ڷ� ����
    //
    // constantPredicate : FROM ���� ������� predicate ����
    //                     ex) 1 = 1
    // oneTablePredicate : FROM ���� �����ϴ� ����� table �� ���� �ϳ���
    //                     table�� ���õ� predicate
    //                     ex) T1.I1 = 1
    // joinPredicate     : FROM ���� �����ϴ� ����� table �� �ΰ� �̻���
    //                     table�� ���õ� predicate
    //                     ex) T1.I1 = T2.I1
    // levelPredicate    : level predicate
    //                     ex) level = 1
    //-------------------------------------------------------------------

    qmoPredicate  * constantPredicate;
    qmoPredicate  * oneTablePredicate;
    qmoPredicate  * joinPredicate;
    qmoPredicate  * levelPredicate;
    qmoPredicate  * connectByRownumPred;

    //-------------------------------------------------------
    // Base Graph ���� �ڷ� ����
    //-------------------------------------------------------

    UInt            graphCnt4BaseTable;    // baseGraph ����
    qmgGraph     ** baseGraph;             // baseGraph pointer�� ������ �迭

    //------------------------------------------------------
    // Join Group ���� �ڷ� ����
    //
    //   - maxJoinGroupCnt : �ִ� joinGroupCnt = graphCnt4BaseTable
    //   - joinGroupCnt    : ���� joinGroupCnt
    //   - joinGroup       : joinGroup �迭
    //------------------------------------------------------

    UInt            maxJoinGroupCnt;
    UInt            joinGroupCnt;
    qmoJoinGroup  * joinGroup;

    //-------------------------------------------------------------------
    // Table Order ����
    //    - tableCnt : table ����
    //      outer join�� ���� ��� : graphCnt4BaseTable�� ������ ���� ����
    //      outer join�� �ִ� ��� : outer join�� �����ϴ� ��� table ����
    //                               �����ϹǷ� graphCnt4BaseTable���� ŭ
    //   - tableOrder : table ����
    //-------------------------------------------------------------------

    UInt            tableCnt;
    qmoTableOrder * tableOrder;

    // BUG-34295 Join ordering ANSI style query
    qmgGraph      * outerJoinGraph;
    idBool          mIsOnlyNL;
} qmoCNF;

//----------------------------------------------------------
// CNF Critical Path�� �����ϱ� ���� �Լ�
//----------------------------------------------------------

class qmoCnfMgr
{
public:

    //-----------------------------------------------------
    // CNF Critical Path ���� �� �ʱ�ȭ
    //-----------------------------------------------------

    // �Ϲ� qmoCNF�� �ʱ�ȭ
    static IDE_RC    init( qcStatement * aStatement,
                           qmoCNF      * aCNF,
                           qmsQuerySet * aQuerySet,
                           qtcNode     * aNormalCNF,
                           qtcNode     * aNnfFilter );

    // on Condition�� ���� qmoCNF�� �ʱ�ȭ
    static IDE_RC    init( qcStatement * aStatement,
                           qmoCNF      * aCNF,
                           qmsQuerySet * aQuerySet,
                           qmsFrom     * aFrom,
                           qtcNode     * aNormalCNF,
                           qtcNode     * aNnfFilter );

    //-----------------------------------------------------
    // CNF Critical Path�� ���� ����ȭ
    //     - Predicate �з�, �� base graph����ȭ, Join�� ó�� ����
    //     - qmgHierarcy�� left outer graph �迭�� ȣ������ ����
    //-----------------------------------------------------

    static IDE_RC   optimize( qcStatement * aStatement,
                              qmoCNF      * aCNF );


    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // optimization�� ���� ������ ���� �ʿ䰡 ���� ��
    // �� �Լ��� �߰��ϸ� �ȴ�.
    static IDE_RC    removeOptimizationInfo( qcStatement * aStatement,
                                             qmoCNF      * aCNF );

    //-----------------------------------------------------
    // Predicate �з� �Լ�
    //-----------------------------------------------------

    // where�� Predicate �з�
    static IDE_RC    classifyPred4Where( qcStatement       * aStatement,
                                         qmoCNF            * aCNF,
                                         qmsQuerySet       * aQuerySet );

    // where�� Predicate �з�
    static IDE_RC    classifyPred4WhereHierachyJoin( qcStatement  * aStatement,
                                                     qmoCrtPath   * aCrtPath,
                                                     qmgGraph     * aGraph );
    // on Condition Predicate �з�
    static IDE_RC    classifyPred4OnCondition( qcStatement       * aStatement,
                                               qmoCNF            * aCNF,
                                               qmoPredicate     ** aUpperPred,
                                               qmoPredicate      * aLowerPred,
                                               qmsJoinType         aJoinType );

    // startWith�� Predicate �з�
    static IDE_RC    classifyPred4StartWith( qcStatement       * aStatement,
                                             qmoCNF            * aCNF,
                                             qcDepInfo         * aDepInfo );

    // connectBy�� Predicate �з�
    static IDE_RC    classifyPred4ConnectBy( qcStatement       * aStatement,
                                             qmoCNF            * aCNF,
                                             qcDepInfo         * aDepInfo );

    // baseGraph���� �ʱ�ȭ �Լ� ȣ���Ͽ� baseGraph�� ���� �� �ʱ�ȭ��
    static IDE_RC    initBaseGraph( qcStatement   * aStatement,
                                    qmgGraph     ** aBaseGraph,
                                    qmsFrom       * aFrom,
                                    qmsQuerySet   * aQuerySet );

    // Predicate�� �����ϴ� �Լ�
    static IDE_RC    copyPredicate( qcStatement   * aStatement,
                                    qmoPredicate ** aDstPredicate,
                                    qmoPredicate  * aSrcPredicate );

    // Join Graph�� selectivity�� ���ϴ� �Լ�
    static IDE_RC    getJoinGraphSelectivity( qcStatement  * aStatement,
                                              qmgGraph     * aJoinGraph,
                                              qmoPredicate * aJoinPredicate,
                                              SDouble      * aJoinSelectivity,
                                              SDouble      * aJoinSize );

    // fix BUG-9791, BUG-10419
    // constant filter�� ó�������� ������ left graph�� �����ִ� �Լ�
    static IDE_RC    pushSelection4ConstantFilter( qcStatement * aStatement,
                                                   qmgGraph    * aGraph,
                                                   qmoCNF      * aCNF );

    // PROJ-1404
    // where���� predicate���� transitive predicate�� �����ϰ� ������
    static IDE_RC    generateTransitivePredicate( qcStatement * aStatement,
                                                  qmoCNF      * aCNF,
                                                  idBool        aIsOnCondition );

    // PROJ-1404
    // on���� predicate���� transitive predicate�� �����ϰ� ������
    // on���� upper predicate���� lower predicate�� �����ϰ� ��ȯ��
    static IDE_RC    generateTransitivePred4OnCondition( qcStatement   * aStatement,
                                                         qmoCNF        * aCNF,
                                                         qmoPredicate  * aUpperPred,
                                                         qmoPredicate ** aLowerPred );

    // fix BUG-19203
    // on Condition CNF�� �� Predicate�� subuqery ó��
    static IDE_RC   optimizeSubQ4OnCondition( qcStatement * aStatement,
                                              qmoCNF      * aCNF );

private:
    //------------------------------------------------------
    // �ʱ�ȭ �Լ����� ���
    //------------------------------------------------------

    // joinGroup���� �ʱ�ȭ
    static IDE_RC    initJoinGroup( qcStatement   * aStatement,
                                    qmoJoinGroup  * aJoinGroup,
                                    UInt            aJoinGroupCnt,
                                    UInt            aBaseGraphCnt );

    //-----------------------------------------------------
    // Predicate �з� �Լ����� ���
    //-----------------------------------------------------

    // classifyPred4Where �Լ����� ȣ�� :
    //    constant predicate�� level/prior/constant �з��Ͽ� �ش� ��ġ�� �߰�
    static IDE_RC    addConstPred4Where( qmoPredicate * aPredicate,
                                         qmoCNF       * aCNF,
                                         idBool         aExistHierarchy );

    static idBool    checkPushPredHint( qmoCNF         * aCNF,
                                        qtcNode        * aNode,
                                        qmsJoinType      aJoinType );

    // PROJ-1495
    // classifyPred4Where �Լ����� ȣ�� :
    //    PUSH_PRED hint�� ���� hint�� view table�� ���õ�
    //    join predicate�� view���η� ����
    static IDE_RC  pushJoinPredInView( qmoPredicate     * aPredicate,
                                       qmsPushPredHints * aPushPredHint,
                                       UInt               aBaseGraphCnt,
                                       qmsJoinType        aJoinType,
                                       qmgGraph        ** aBaseGraph,
                                       idBool           * aIsPush );

    // classifyPred4OnCondition �Լ����� ȣ�� :
    //    onConditionCNF�� one table predicate�� �ش� ��ġ�� �߰�
    static IDE_RC    addOneTblPredOnJoinType( qmoCNF       * aCNF,
                                              qmgGraph     * aGraph,
                                              qmoPredicate * aPredicate,
                                              qmsJoinType    aJoinType,
                                              idBool         aIsLeft );

    // classifyPred4OnCondition �Լ����� ȣ��
    //    onConditionCNF�� ������ Join Graph�� myPredicate�� push selection
    static IDE_RC   pushSelectionOnJoinType( qcStatement   * aStatement,
                                             qmoPredicate ** aUpperPred,
                                             qmgGraph     ** aBaseGraph,
                                             qcDepInfo     * aFromDependencies,
                                             qmsJoinType     aJoinType );


    // classifyPred4ConnectBy �Լ����� ȣ�� :
    //    constant predicate�� level/prior/constant �з��Ͽ� �ش� ��ġ�� �߰�
    static IDE_RC    addConstPred4ConnectBy( qmoCNF       * aCNF,
                                             qmoPredicate * aPredicate );

    //-----------------------------------------------------
    // ����ȭ �Լ����� ���
    //-----------------------------------------------------

    // Join Order�� �����ϴ� �Լ�
    static IDE_RC    joinOrdering( qcStatement    * aStatement,
                                   qmoCNF         * aCNF );

    //-----------------------------------------------------
    // Join Ordering �Լ����� ���
    //-----------------------------------------------------

    // Join Group�� �з��ϴ� �Լ�
    static IDE_RC    joinGrouping( qmoCNF         * aCNF );

    // Join Group �� Join Relation
    static IDE_RC    joinRelationing( qcStatement    * aStatement,
                                      qmoJoinGroup   * aJoinGroup );

    // Join Group ������ Join Order�� �����ϴ� �Լ�
    static IDE_RC    joinOrderingInJoinGroup( qcStatement    * aStatement,
                                              qmoJoinGroup   * aJoinGroup );

    static idBool    isSemiAntiJoinable( qmgGraph        * aJoinGraph,
                                         qmoJoinRelation * aJoinRelation );

    static idBool    isFeasibleJoinOrder( qmgGraph        * aGraph,
                                          qmoJoinRelation * aJoinRelation );

    static IDE_RC    initJoinGraph( qcStatement     * aStatement,
                                    qmoJoinRelation * aJoinRelation,
                                    qmgGraph        * aLeft,
                                    qmgGraph        * aRight,
                                    qmgGraph        * aJoinGraph,
                                    idBool            aExistOrderFactor );

    static IDE_RC    initJoinGraph( qcStatement     * aStatement,
                                    UInt              aJoinGraphType,
                                    qmgGraph        * aLeft,
                                    qmgGraph        * aRight,
                                    qmgGraph        * aJoinGraph,
                                    idBool            aExistOrderFactor );

    // PROJ-1495
    // join group���� ���μ��� ���ġ
    static IDE_RC relocateJoinGroup4PushPredHint(
        qcStatement      * aStatement,
        qmsPushPredHints * aPushPredHint,
        qmoJoinGroup    ** aJoinGroup,
        UInt               aJoinGroupCnt );

    // Table Order ������ �����ϴ� �Լ�
    static IDE_RC    getTableOrder( qcStatement    * aStatement,
                                    qmgGraph       * aGraph,
                                    qmoTableOrder ** aTableOrder);

    // Join Ordering, Join Groupint���� ȣ��
    // Join Predicate���� �ش� Join Graph�� predicate�� �и��Ͽ� ����
    static IDE_RC connectJoinPredToJoinGraph(qcStatement   * aStatement,
                                             qmoPredicate ** aJoinGroupPred,
                                             qmgGraph     * aJoinGraph );

    //-----------------------------------------------------
    // Join Grouping ���� ����ϴ� �Լ�
    //-----------------------------------------------------

    // BUG-42447 leading hint support
    static void     makeLeadingGroup( qmoCNF         * aCNF,
                                      qmsLeadingHints* aLeadingHints );

    static IDE_RC    joinGroupingWithJoinPred( qmoJoinGroup  * aJoinGroup,
                                               qmoCNF        * aCNF,
                                               UInt          * aJoinGroupCnt );

    // base graph�� �ش� Join Group�� ��������ִ� �Լ�
    static
        IDE_RC    connectBaseGraphToJoinGroup( qmoJoinGroup  * aJoinGroup,
                                               qmgGraph     ** aCNFBaseGraph,
                                               UInt            aCNFBaseGraphCnt );

    //-----------------------------------------------------
    // Join Relation���� ����ϴ� �Լ�
    //-----------------------------------------------------

    // �̹� ������ Join Relation�� �ִ��� Ȯ��
    static idBool    existJoinRelation( qmoJoinRelation * aJoinRelationList,
                                        qcDepInfo       * aDepInfo );

    // Semi/anti join�� inner table�� ID�� ��� �Լ�
    static IDE_RC    getInnerTable( qcStatement * aStatement,
                                    qtcNode     * aNode,
                                    SInt        * aTableID );

    // Semi/anti join�� Join Relation List�� ������ִ� �Լ�
    static IDE_RC    makeSemiAntiJoinRelationList( qcStatement      * aStatement,
                                                   qtcNode          * aNode,
                                                   qmoJoinRelation ** aJoinRelationList,
                                                   UInt             * aJoinRelationCnt );

    // Semi/anti join�� outer relation�� Join Relation List�� ������ִ� �Լ�
    static IDE_RC    makeCrossJoinRelationList( qcStatement      * aStatement,
                                                qmoJoinGroup     * aJoinGroup,
                                                qmoJoinRelation ** aJoinRelationList,
                                                UInt             * aJoinRelationCnt );

    // Join Relation List�� ������ִ� �Լ�
    static IDE_RC    makeJoinRelationList( qcStatement      * aStatement,
                                           qcDepInfo        * aDependencies,
                                           qmoJoinRelation ** aJoinRelationList,
                                           UInt             * aJoinRelationCnt );

    // �� ���� conceptual table�θ� ������ Node ����
    static IDE_RC   isTwoConceptTblNode( qtcNode      * aNode,
                                         qmgGraph    ** aBaseGraph,
                                         UInt           aBaseGraphCnt,
                                         idBool       * aIsTwoConceptTbl);

    // aDependencis�� ������ �� ���� Graph�� ã���ִ� �Լ�
    static IDE_RC    getTwoRelatedGraph( qcDepInfo * aDependencies,
                                         qmgGraph ** aBaseGraph,
                                         UInt        aBaseGraphCnt,
                                         qmgGraph ** aFirstGraph,
                                         qmgGraph ** aSecondGraph );

    //-----------------------------------------------------
    // �� �� �Լ����� ȣ��
    //-----------------------------------------------------

    // pushSelectionOnJoinType() �Լ����� ���
    // push selection�� predicate�� ��ġ
    static IDE_RC    getPushPredPosition( qmoPredicate  * aPredicate,
                                          qmgGraph     ** aBaseGraph,
                                          qcDepInfo     * aFromDependencies,
                                          qmsJoinType     aJoinType,
                                          UInt          * aPredPos );

    // addOneTblPredOnJoinType(), getPushPredPosition() �Լ����� ���
    // Push Selection ������ Predicate���� �˻�
    static qmoPushApplicableType isPushApplicable( qmoPredicate * aPredicate,
                                                   qmsJoinType    aJoinType );

    // PROJ-2418 
    // Lateral View�� ������ Predicate�� CNF���� ������ ��ȯ
    static IDE_RC    discardLateralViewJoinPred( qmoCNF        * aCNF,
                                                 qmoPredicate ** aDiscardPredList );

    // PROJ-2418
    // Lateral View�� �ܺ� �����ϴ� Relation���� Left/Full-Outer Join ����
    static IDE_RC    validateLateralViewJoin( qcStatement * aStatement,
                                              qmsFrom     * aFrom );
};


#endif /* _O_QMO_CRT_CNF_MGR_H_ */
