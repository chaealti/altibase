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
 * $Id: qmgFullOuter.h 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     FullOuter Graph�� ���� ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#ifndef _O_QMG_FULL_OUTER_H_
#define _O_QMG_FULL_OUTER_H_ 1

#include <qc.h>
#include <qmgDef.h>
#include <qmoJoinMethod.h>
#include <qmoPredicate.h>

//---------------------------------------------------
// Full Outer Join Graph �� �����ϱ� ���� �ڷ� ����
//---------------------------------------------------

typedef struct qmgFOJN
{
    qmgGraph graph;  // ���� Graph ����

    // Full Outer Join Graph�� ���� ����

    qmoCNF            * onConditionCNF;

    //-----------------------------------------------
    // Join Method ����
    //-----------------------------------------------

    qmoJoinMethod     * nestedLoopJoinMethod;
    qmoJoinMethod     * sortBasedJoinMethod;
    qmoJoinMethod     * hashBasedJoinMethod;

    qmoJoinMethodCost * selectedJoinMethod; // ���� cost�� ���� Join Method

    //----------------------------------------------
    // Join Predicate ����:
    //    ���õ� Join Method Type�� ���� ������ ���� Join Predicate�� �з��ȴ�.
    //
    //    - joinablePredicate
    //      Index Nested Loop or Anti Outer      : indexablePredicate
    //      One Pass/Two Pass Sort Join          : sortJoinablePredicate
    //      One Pass/Two Pass Hash Join          : hashJoinablePredicate
    //    - nonJoinablePredicate
    //      Index Nested Loop or Anti Outer      : nonIndexablePredicate
    //      One Pass/Two Pass Sort Join or Merge : nonSortJoinablePredicate
    //      One Pass/Two Pass Hash Join          : nonHashJoinablePredicate
    //
    //----------------------------------------------

    qmoPredicate      * joinablePredicate;
    qmoPredicate      * nonJoinablePredicate;

    //----------------------------------------------
    // Join Method�� Anti Outer Join���� ������ ���, �����Ǵ� ����
    //    - antiLeftGraph  : right graph�� �����Ͽ� ����
    //    - antiRightGraph : left graph�� �����ؼ� ����
    //                      joinable predicate�� �����Ͽ� myPredicate�� ����
    //----------------------------------------------

    qmgGraph          * antiLeftGraph;
    qmgGraph          * antiRightGraph;

    //---------------------------------------------
    // Join Method Type�� Hash Based Join�� ���, ���
    //---------------------------------------------

    UInt            hashBucketCnt;        // hash bucket count
    UInt            hashTmpTblCnt;        // hash temp table ����

    // PROJ-2242
    SDouble         firstRowsFactor;      // FIRST_ROWS_N
    SDouble         joinOrderFactor;      // join ordering factor
    SDouble         joinSize;             // join ordering size
} qmgFOJN;

//---------------------------------------------------
// Full Outer Join Graph �� �����ϱ� ���� �Լ�
//---------------------------------------------------

class qmgFullOuter
{
public:
    // Graph �� �ʱ�ȭ
    static IDE_RC  init( qcStatement * aStatement,
                         qmsQuerySet * aQuerySet,
                         qmsFrom     * aFrom,
                         qmgGraph   ** aGraph );

    // Graph�� ����ȭ ����
    static IDE_RC  optimize( qcStatement * aStatement, qmgGraph * aGraph );

    // Graph�� Plan Tree ����
    static IDE_RC  makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph );

    // Graph�� ���� ������ �����.
    static IDE_RC  printGraph( qcStatement  * aStatement,
                               qmgGraph     * aGraph,
                               ULong          aDepth,
                               iduVarString * aString );

private:
    static IDE_RC  makeChildPlan( qcStatement * aStatement,
                                  qmgFOJN     * aMyGraph,
                                  qmnPlan     * aLeftPlan,
                                  qmnPlan     * aRightPlan );

    static IDE_RC  makeSortRange( qcStatement  * aStatement,
                                  qmgFOJN      * aMyGraph,
                                  qtcNode     ** aRange );

    static IDE_RC  makeHashFilter( qcStatement  * aStatement,
                                   qmgFOJN      * aMyGraph,
                                   qtcNode     ** aFilter );

    static IDE_RC  makeFullStoreNLJoin( qcStatement * aStatement,
                                        qmgFOJN     * aMyGraph );

    static IDE_RC  makeAntiOuterJoin( qcStatement * aStatement,
                                      qmgFOJN     * aMyGraph );

    static IDE_RC  makeHashJoin( qcStatement * aStatement,
                                 qmgFOJN     * aMyGraph,
                                 idBool        aIsTwoPass );

    static IDE_RC  makeSortJoin( qcStatement * aStatement,
                                 qmgFOJN     * aMyGraph,
                                 idBool        aIsTwoPass );

    static IDE_RC  initFILT( qcStatement  * aStatement,
                             qmgFOJN      * aMyGraph,
                             qtcNode     ** aFilter,
                             qmnPlan     ** aFILT );

    static IDE_RC  makeFILT( qcStatement * aStatement,
                             qmgFOJN     * aMyGraph,
                             qtcNode     * aFilter,
                             qmnPlan     * aFILT );
};

#endif /* _O_QMG_FULL_OUTER_H_ */
