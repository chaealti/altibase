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
 * $Id: qmgJoin.h 88830 2020-10-08 02:14:27Z donovan.seo $
 *
 * Description :
 *     Join Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_JOIN_H_
#define _O_QMG_JOIN_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoJoinMethod.h>
#include <qmoPredicate.h>

// To Fix BUG-20259
#define QMG_JOIN_MAX_TMPTABLE_COUNT        (10)

// PROJ-1718
enum qmgInnerJoinMethod
{
    QMG_INNER_JOIN_METHOD_NESTED = 0,
    QMG_INNER_JOIN_METHOD_HASH,
    QMG_INNER_JOIN_METHOD_SORT,
    QMG_INNER_JOIN_METHOD_MERGE,
    QMG_INNER_JOIN_METHOD_COUNT
};

//---------------------------------------------------
// Join Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgJOIN
{
    qmgGraph graph;  // ���� Graph ����

    // Join Graph�� ���� ����

    qmoCNF            * onConditionCNF;

    //-----------------------------------------------
    // Join Method ����
    //-----------------------------------------------

    qmoJoinMethod     * joinMethods;

    qmoJoinMethodCost * selectedJoinMethod; // ���� cost�� ���� Join Method

    //----------------------------------------------
    // Join Predicate ����:
    //    ���õ� Join Method Type�� ���� ������ ���� Join Predicate�� �з��ȴ�.
    //
    //    - joinablePredicate
    //      Index Nested Loop or Anti Outer      : indexablePredicate
    //      One Pass/Two Pass Sort Join or Merge : sortJoinablePredicate
    //      One Pass/Two Pass Hash Join          : hashJoinablePredicate
    //    - nonJoinablePredicate
    //      Index Nested Loop or Anti Outer      : nonIndexablePredicate
    //      One Pass/Two Pass Sort Join or Merge : nonSortJoinablePredicate
    //      One Pass/Two Pass Hash Join          : nonHashJoinablePredicate
    //
    //----------------------------------------------

    qmoPredicate      * joinablePredicate;
    qmoPredicate      * nonJoinablePredicate;

    // PROJ-2179/BUG-35484
    // Full/index nested loop join�� push down�� predicate�� ��Ƶд�.
    qmoPredicate      * pushedDownPredicate;

    //---------------------------------------------
    // Join Method Type�� Hash Based Join�� ���, ���
    //---------------------------------------------

    UInt            hashBucketCnt;        // hash bucket count
    UInt            hashTmpTblCnt;        // hash temp table ����

    // PROJ-2242
    SDouble         firstRowsFactor;      // FIRST_ROWS_N
    SDouble         joinOrderFactor;      // join ordering factor
    SDouble         joinSize;             // join ordering size
} qmgJOIN;

//---------------------------------------------------
// Join Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgJoin
{
public:
    // Inner Join�� �����ϴ� qmgJoin Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmsFrom     * aFrom,
                         qmgGraph   ** aGraph );

    // Join Relation�� �����ϴ� qmgJoin Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmgGraph    * aLeftGraph,
                         qmgGraph    * aRightGraph,
                         qmgGraph    * aGraph,
                         idBool        aExistOrderFactor );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    static IDE_RC  makeChildPlan( qcStatement * aStatement,
                                  qmgJOIN     * aMyGraph,
                                  qmnPlan     * aLeftPlan,
                                  qmnPlan     * aRightPlan );

    static IDE_RC  makeSortRange( qcStatement  * aStatement,
                                  qmgJOIN      * aMyGraph,
                                  qtcNode     ** aRange );

    static IDE_RC  makeHashFilter( qcStatement  * aStatement,
                                   qmgJOIN      * aMyGraph,
                                   qtcNode     ** aFilter );

    static IDE_RC  makeFullNLJoin( qcStatement    * aStatement,
                                   qmgJOIN        * aMyGraph,
                                   idBool           aIsStored );

    static IDE_RC  makeIndexNLJoin( qcStatement    * aStatement,
                                    qmgJOIN        * aMyGraph );

    static IDE_RC  makeInverseIndexNLJoin( qcStatement    * aStatement,
                                           qmgJOIN        * aMyGraph );

    static IDE_RC  makeHashJoin( qcStatement    * aStatement,
                                 qmgJOIN        * aMyGraph,
                                 idBool           aIsTwoPass,
                                 idBool           aIsInverse );

    static IDE_RC  makeSortJoin( qcStatement     * aStatement,
                                 qmgJOIN         * aMyGraph,
                                 idBool            aIsTwoPass,
                                 idBool            aIsInverse );

    static IDE_RC  makeMergeJoin( qcStatement     * aStatement,
                                  qmgJOIN         * aMyGraph );

    // Join method�� ���� joinable predicate���� �и����� ����
    // predicate���� ��� filter�� ����
    static IDE_RC  extractFilter( qcStatement   * aStatement,
                                  qmgJOIN       * aMyGraph,
                                  qmoPredicate  * aPredicate,
                                  qtcNode      ** aFilter );

    // Constant predicate�� ���� ��쿡�� ����
    static IDE_RC  initFILT( qcStatement   * aStatement,
                             qmgJOIN       * aMyGraph,
                             qmnPlan      ** aFILT );

    static IDE_RC  makeFILT( qcStatement  * aStatement,
                             qmgJOIN      * aMyGraph,
                             qmnPlan      * aFILT );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

    // ���� cost�� ���� Join Method Cost�� ����
    static IDE_RC selectJoinMethodCost( qcStatement        * aStatement,
                                        qmgGraph           * aGraph,
                                        qmoPredicate       * aJoinPredicate,
                                        qmoJoinMethod      * aJoinMethods,
                                        qmoJoinMethodCost ** aSelected );

    // Join Method ���� �� ó��
    static
        IDE_RC afterJoinMethodDecision( qcStatement       * aStatement,
                                        qmgGraph          * aGraph,
                                        qmoJoinMethodCost * aSelectedMethod,
                                        qmoPredicate     ** aPredicate,
                                        qmoPredicate     ** aJoinablePredicate,
                                        qmoPredicate     ** aNonJoinablePredicate);


    // Preserved Order ����
    static IDE_RC makePreservedOrder( qcStatement       * aStatement,
                                      qmgGraph          * aGraph,
                                      qmoJoinMethodCost * aSelectedMethod,
                                      qmoPredicate      * aJoinablePredicate );

    static IDE_RC finalizePreservedOrder( qmgGraph * aGraph );

private:

    // bucket count�� temp table count�� ���ϴ� �Լ�
    static
        IDE_RC getBucketCntNTmpTblCnt( qcStatement       * aStatement,
                                       qmoJoinMethodCost * aSelectedMethod,
                                       qmgGraph          * aGraph,
                                       idBool              aIsLeftGraph,
                                       UInt              * aBucketCnt,
                                       UInt              * aTmpTblCnt );

    // BUG-13257
    // Temporary_Table�� ������ Temporary_TableSpace�� ���� ũ�⸦ ����Ͽ� ����Ѵ�
    static IDE_RC decideTmpTblCnt(  qcStatement       * aStatement,
                                    qmgGraph          * aGraph,
                                    SDouble             aMemoryBufCnt,
                                    UInt              * aTmpTblCnt );

    // Child�� Graph�κ��� Preserved Order�� �����Ѵ�.
    static IDE_RC makeOrderFromChild( qcStatement * aStatement,
                                      qmgGraph    * aGraph,
                                      idBool        aIsRightGraph );

    // Two Pass Sort, Merge Join�� Join Predicate���κ��� Preserved Order ����
    static IDE_RC makeOrderFromJoin( qcStatement        * aStatement,
                                     qmgGraph           * aGraph,
                                     qmoJoinMethodCost  * aSelectedMethod,
                                     qmgDirectionType     aDirection,
                                     qmoPredicate       * aJoinablePredicate );

    // ���ο� Preserved Order ���� �� ����
    static IDE_RC makeNewOrder( qcStatement       * aStatement,
                                qmgGraph          * aGraph,
                                qcmIndex          * aSelectedIndex );
    
    // ���ο� Preserved Order ���� �� ����
    static IDE_RC makeNewOrder4Selection( qcStatement       * aStatement,
                                          qmgGraph          * aGraph,
                                          qcmIndex          * aSelectedIndex );

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition graph�� ���� ���ο� preserved order ���� �� ����.
    static IDE_RC makeNewOrder4Partition( qcStatement       * aStatement,
                                          qmgGraph          * aGraph,
                                          qcmIndex          * aSelectedIndex );

    // push-down join predicate�� �޾Ƽ� �׷����� ����.
    static IDE_RC setJoinPushDownPredicate( qcStatement   * aStatement,
                                            qmgGraph      * aGraph,
                                            qmoPredicate ** aPredicate );
    
    // push-down non-join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
    static IDE_RC setNonJoinPushDownPredicate( qcStatement   * aStatement,
                                               qmgGraph      * aGraph,
                                               qmoPredicate ** aPredicate );
    
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� graph�� ���� access method�� �ٲ� ���
    // selection graph�� sdf�� disable ��Ų��.
    static IDE_RC alterSelectedIndex( qcStatement * aStatement,
                                      qmgGraph    * aGraph,
                                      qcmIndex    * aNewIndex );

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // ���� JOIN graph���� ANTI�� ó���� ��
    // ���� SELT graph�� �����ϴµ� �̶� �� �Լ���
    // ���ؼ� �����ϵ��� �ؾ� �����ϴ�.
    static IDE_RC copyGraphAndAlterSelectedIndex( qcStatement * aStatement,
                                                  qmgGraph    * aSource,
                                                  qmgGraph   ** aTarget,
                                                  qcmIndex    * aNewSelectedIndex,
                                                  UInt          aWhichOne );
    
    static IDE_RC randomSelectJoinMethod( qcStatement        * aStatement,
                                          UInt                 aJoinMethodCnt,
                                          qmoJoinMethod      * aJoinMethods,
                                          qmoJoinMethodCost ** aSelected );

    static void moveConstantPred( qmgJOIN * aMyGraph, idBool aIsInverse );

    static void checkOrValueIndex( qmgGraph * aGraph );

#endif /* _O_QMG_JOIN_H_ */
};
