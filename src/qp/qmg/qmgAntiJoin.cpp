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
 * $Id$
 *
 * Description :
 *     Anti join graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgAntiJoin.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoTwoNonPlan.h>
#include <qmoSelectivity.h>
#include <qmgJoin.h>
#include <qmgSemiJoin.h>
#include <qmoCostDef.h>
#include <qmo.h>

IDE_RC
qmgAntiJoin::init( qcStatement * aStatement,
                   qmgGraph    * aLeftGraph,
                   qmgGraph    * aRightGraph,
                   qmgGraph    * aGraph )
{
/***********************************************************************
 *
 * Description : qmgAntiJoin Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1)  qmgAntiJoin�� ���� ���� �Ҵ�
 *    (2)  graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3)  graph.type ����
 *    (4)  graph.myQuerySet�� aQuerySet���� ����
 *    (5)  graph.myFrom�� aFrom���� ����
 *    (6)  graph.dependencies ����
 *    (7)  qmgJoin�� onConditonCNF ó��
 *    (8)  ����graph�� ���� �� �ʱ�ȭ
 *    (9)  graph.optimize�� graph.makePlan ����
 *    (10) out ����
 *
 ***********************************************************************/

    qmgJOIN   * sMyGraph;
    qcDepInfo   sOrDependencies;

    IDU_FIT_POINT_FATAL( "qmgAntiJoin::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftGraph != NULL );
    IDE_DASSERT( aRightGraph != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // Anti Join Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgJOIN *)aGraph;

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    // Graph�� ���� ǥ��
    sMyGraph->graph.type = QMG_ANTI_JOIN;
    sMyGraph->graph.left  = aLeftGraph;
    sMyGraph->graph.right = aRightGraph;

    sMyGraph->graph.myFrom = NULL;
    sMyGraph->graph.myQuerySet = aLeftGraph->myQuerySet;

    IDE_TEST( qtc::dependencyOr( & aLeftGraph->depInfo,
                                    & aRightGraph->depInfo,
                                    & sOrDependencies )
                 != IDE_SUCCESS );

    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & sOrDependencies );

    // Graph�� �Լ� �����͸� ����
    sMyGraph->graph.optimize = qmgAntiJoin::optimize;
    sMyGraph->graph.makePlan = qmgJoin::makePlan;
    sMyGraph->graph.printGraph = qmgJoin::printGraph;

    // Disk/Memory ���� ����
    switch(  sMyGraph->graph.myQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            // �߰� ��� Type Hint�� ���� ���,
            // left�� disk�̸� disk
            if ( ( sMyGraph->graph.left->flag & QMG_GRAPH_TYPE_MASK )
                   == QMG_GRAPH_TYPE_DISK )
            {
                sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
                sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
            }
            else
            {
                sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
                sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
            }
            break;
        case QMO_INTER_RESULT_TYPE_DISK :
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
            break;
        case QMO_INTER_RESULT_TYPE_MEMORY :
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
            break;
        default :
            IDE_DASSERT( 0 );
            break;
    }

    //---------------------------------------------------
    // Anti Join Graph ���� ���� �ڷ� ���� �ʱ�ȭ
    //---------------------------------------------------

    // Anti join�� ����ڰ� ����� �� �����Ƿ� ON���� ������� �ʴ´�.
    sMyGraph->onConditionCNF = NULL;

    sMyGraph->joinMethods = NULL;
    sMyGraph->joinablePredicate = NULL;
    sMyGraph->nonJoinablePredicate = NULL;
    sMyGraph->hashBucketCnt = 0;
    sMyGraph->hashTmpTblCnt = 0;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgAntiJoin::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgAntiJoin Graph�� ����ȭ
 *
 * Implementation :
 *    (0) left graph�� ����ȭ ����
 *    (1) right graph�� ����ȭ ����
 *    (2) subquery�� ó��
 *    (3) Join Method�� �ʱ�ȭ
 *    (4) Join Method�� ����
 *    (5) Join Method ���� �� ó��
 *    (6) ���� ��� ������ ����
 *    (7) Preserved Order, DISK/MEMORY ����
 *
 ***********************************************************************/

    qmgJOIN      * sMyGraph;
    qmoPredicate * sPredicate;

    IDU_FIT_POINT_FATAL( "qmgAntiJoin::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph = (qmgJOIN*) aGraph;

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        /* TASK-7219 Non-shard DML */
        IDE_TEST( qmo::removeOutRefPredPushedForce( & sMyGraph->graph.myPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // On condition�� �������� �����Ƿ� left�� right�� optimization�� �ʿ����.

    // BUG-32703
    // ����ȭ �ÿ� view�� graph�� �����ȴ�
    // left outer join�� ���� graph�� view�� �ִٸ�
    // ����ȭ ���Ŀ� ( ��, view graph�� ����ȭ �� ���Ŀ� )
    // left outer join�� ���� type ( disk or memory )�� ���� �����ؾ� �Ѵ�.
    // BUG-40191 __OPTIMIZER_DEFAULT_TEMP_TBS_TYPE ��Ʈ�� ����ؾ� �Ѵ�.
    if ( aGraph->myQuerySet->SFWGH->hints->interResultType == QMO_INTER_RESULT_TYPE_NOT_DEFINED )
    {
        if ( ( sMyGraph->graph.left->flag & QMG_GRAPH_TYPE_MASK )
               == QMG_GRAPH_TYPE_DISK )
        {
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
        }
        else
        {
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
        }
    }
    else
    {
        // nothing to do
    }

    //------------------------------------------
    // Subquery�� Graph ����
    // - To Fix BUG-10577
    //   Left, Right ����ȭ �Ŀ� subquery graph�� �����ؾ� ������ view�϶�
    //   view ��� ���� �̱������� ������ ����ϴ� ������ �߻����� �ʴ´�.
    //   ( = BUG-9736 )
    //   Predicate �з��� dependencies�� �����ϱ� ������ predicate��
    //   Subquery graph ���� ���� �����ص� �ȴ�.
    //------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                                  sMyGraph->graph.myPredicate,
                                                  ID_FALSE )  // No KeyRange Tip
                     != IDE_SUCCESS );

        IDE_TEST(
            qmoPred::classifyJoinPredicate( aStatement,
                                            sMyGraph->graph.myPredicate,
                                            sMyGraph->graph.left,
                                            sMyGraph->graph.right,
                                            & sMyGraph->graph.depInfo )
            != IDE_SUCCESS );

        // To fix BUG-26885
        // DNF�� join�� �Ǵ� ��� materialize�� ���� predicate
        // �� ����Ʃ�� ��ġ�� �ٲ� �� �����Ƿ�
        // join predicate�� �����Ѵ�.
        if( aGraph->myQuerySet->SFWGH->crtPath->currentNormalType
            == QMO_NORMAL_TYPE_DNF )
        {   
            IDE_TEST(qmoPred::copyPredicate4Partition(
                            aStatement,
                            sMyGraph->graph.myPredicate,
                            &sPredicate,
                            0,
                            0,
                            ID_TRUE )
                        != IDE_SUCCESS );
        
            sMyGraph->graph.myPredicate = sPredicate;
        }
        else
        {
            // CNF�̰ų� NNF�� ��� ������ �ʿ䰡 ����.
            // Nothing To Do
        }
    }
    else
    {
        // Join Graph�� �ش��ϴ� Predicate�� ���� �����
        // Nothing To Do
    }

    //------------------------------------------
    // Selectivity ����
    //------------------------------------------

    // WHERE clause, ON clause ��� ����� ����
    IDE_TEST( qmoSelectivity::setJoinSelectivity(
                     aStatement,
                     & sMyGraph->graph,
                     sMyGraph->graph.myPredicate,
                     & sMyGraph->graph.costInfo.selectivity )
                 != IDE_SUCCESS );

    //------------------------------------------
    // Join Method�� �ʱ�ȭ
    //------------------------------------------

    // Join Method ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinMethod ) * QMG_ANTI_JOIN_METHOD_COUNT,
                                                (void **) &sMyGraph->joinMethods )
                 != IDE_SUCCESS );

    // nested loop join method�� �ʱ�ȭ
    IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                         & sMyGraph->graph,
                                         sMyGraph->firstRowsFactor,
                                         sMyGraph->graph.myPredicate,
                                         QMO_JOIN_METHOD_NL,
                                         &sMyGraph->joinMethods[QMG_ANTI_JOIN_METHOD_NESTED] )
                 != IDE_SUCCESS );

    // hash based join method�� �ʱ�ȭ
    IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                         & sMyGraph->graph,
                                         sMyGraph->firstRowsFactor,
                                         sMyGraph->graph.myPredicate,
                                         QMO_JOIN_METHOD_HASH,
                                         &sMyGraph->joinMethods[QMG_ANTI_JOIN_METHOD_HASH] )
                 != IDE_SUCCESS );

    // sort based join method�� �ʱ�ȭ
    IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                         & sMyGraph->graph,
                                         sMyGraph->firstRowsFactor,
                                         sMyGraph->graph.myPredicate,
                                         QMO_JOIN_METHOD_SORT,
                                         &sMyGraph->joinMethods[QMG_ANTI_JOIN_METHOD_SORT] )
                 != IDE_SUCCESS );

    // merge join method�� �ʱ�ȭ
    IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                         & sMyGraph->graph,
                                         sMyGraph->firstRowsFactor,
                                         sMyGraph->graph.myPredicate,
                                         QMO_JOIN_METHOD_MERGE,
                                         &sMyGraph->joinMethods[QMG_ANTI_JOIN_METHOD_MERGE] )
                 != IDE_SUCCESS );

    // Anti join�� ���� join method selectivity ����(����� semi join�� �����ϴ�.)
    qmgSemiJoin::setJoinMethodsSelectivity( &sMyGraph->graph );

    //------------------------------------------
    // �� Join Method �� ���� ���� cost�� Join Method�� ����
    //------------------------------------------

    IDE_TEST(
        qmgJoin::selectJoinMethodCost( aStatement,
                                       & sMyGraph->graph,
                                       sMyGraph->graph.myPredicate,
                                       sMyGraph->joinMethods,
                                       & sMyGraph->selectedJoinMethod )
        != IDE_SUCCESS );

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // record size ����
    // Anti join�� left�� record���� ����� ����Ѵ�.
    sMyGraph->graph.costInfo.recordSize =
        sMyGraph->graph.left->costInfo.recordSize;

    // input record count ����
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // BUG-37134 semi join, anti join �� setJoinOrderFactor �� ȣ���ؾ��մϴ�.
    // �� qmgJoin �� joinOrderFactor, joinSize ���
    IDE_TEST( qmoSelectivity::setJoinOrderFactor(
                     aStatement,
                     & sMyGraph->graph,
                     sMyGraph->graph.myPredicate,
                     &sMyGraph->joinOrderFactor,
                     &sMyGraph->joinSize )
                 != IDE_SUCCESS );

    // BUG-37407 semi, anti join cost
    // output record count ����

    // BUG-37918 anti join �� selectivity �� 0 �� ���ü� �ִ�.
    // ������ output record�� ����Ҷ��� 0���� ����ϸ� �ȵȴ�.
    if( QMO_COST_IS_EQUAL( sMyGraph->graph.costInfo.selectivity,
                           0.0 ) == ID_TRUE )
    {
        // 0.1 �� tpch Q21�� ���ǰ� �߳����� ���̴�.
        sMyGraph->graph.costInfo.outputRecordCnt =
            sMyGraph->graph.left->costInfo.outputRecordCnt * 0.1;
    }
    else
    {
        sMyGraph->graph.costInfo.outputRecordCnt =
            sMyGraph->graph.left->costInfo.outputRecordCnt *
            sMyGraph->graph.costInfo.selectivity;
    }

    sMyGraph->graph.costInfo.outputRecordCnt =
        IDL_MAX( sMyGraph->graph.costInfo.outputRecordCnt, 1.0 );

    // My Cost ���
    sMyGraph->graph.costInfo.myAccessCost =
        sMyGraph->selectedJoinMethod->accessCost;
    sMyGraph->graph.costInfo.myDiskCost =
        sMyGraph->selectedJoinMethod->diskCost;
    sMyGraph->graph.costInfo.myAllCost =
        sMyGraph->selectedJoinMethod->totalCost;

    // Total Cost ���
    // Join Graph ��ü�� Cost�� �̹� Child�� Cost�� ��� �����ϰ� �ִ�.
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->graph.costInfo.myAccessCost;
    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->graph.costInfo.myDiskCost;
    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->graph.costInfo.myAllCost;

    //------------------------------------------
    // Join Method�� ���� �� ó��
    //------------------------------------------

    // PROJ-2179
    sMyGraph->pushedDownPredicate = sMyGraph->graph.myPredicate;

    IDE_TEST(
        qmgJoin::afterJoinMethodDecision(
            aStatement,
            & sMyGraph->graph,
            sMyGraph->selectedJoinMethod,
            & sMyGraph->graph.myPredicate,
            & sMyGraph->joinablePredicate,
            & sMyGraph->nonJoinablePredicate)
        != IDE_SUCCESS );

    //------------------------------------------
    // Preserved Order
    //------------------------------------------

    IDE_TEST( qmgJoin::makePreservedOrder( aStatement,
                                              & sMyGraph->graph,
                                              sMyGraph->selectedJoinMethod,
                                              sMyGraph->joinablePredicate )
                 != IDE_SUCCESS );
 
    // BUG-38279 Applying parallel query
    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
    aGraph->flag |= ((aGraph->left->flag & QMG_PARALLEL_EXEC_MASK) |
                     (aGraph->right->flag & QMG_PARALLEL_EXEC_MASK));
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

