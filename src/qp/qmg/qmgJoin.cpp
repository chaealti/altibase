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
 * $Id: qmgJoin.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Join Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <smDef.h>
#include <smiTableSpace.h>
#include <qcg.h>
#include <qmgJoin.h>
#include <qmgFullOuter.h>
#include <qmgLeftOuter.h>
#include <qmoCostDef.h>
#include <qmoCnfMgr.h>
#include <qmoNormalForm.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoTwoNonPlan.h>
#include <qmoParallelPlan.h>
#include <qmoJoinMethod.h>
#include <qmoSelectivity.h>
#include <qmgSelection.h>
#include <qmgPartition.h>
#include <qcgPlan.h>
#include <qmo.h>

IDE_RC
qmgJoin::init( qcStatement * aStatement,
               qmsQuerySet * aQuerySet,
               qmsFrom     * aFrom,
               qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : Inner Join�� �����ϴ� qmgJoin Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgJoin�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) qmgJoin�� onCondition ó��
 *    (4) ���� graph�� ���� �� �ʱ�ȭ
 *    (5) graph.optimize�� graph.makePlan ����
 *    (6) out ����
 *
 ***********************************************************************/

    qmgJOIN   * sMyGraph;
    qtcNode   * sNormalCNF;

    qmoNormalType sNormalType;

    IDU_FIT_POINT_FATAL( "qmgJoin::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );

    //---------------------------------------------------
    // Join Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgJoin�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgJOIN),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_INNER_JOIN;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aFrom->depInfo );

    sMyGraph->graph.myFrom = aFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgJoin::optimize;
    sMyGraph->graph.makePlan = qmgJoin::makePlan;
    sMyGraph->graph.printGraph = qmgJoin::printGraph;

    //---------------------------------------------------
    // Join Graph ���� ���� �ڷ� ���� �ʱ�ȭ
    //---------------------------------------------------

    // on condition CNF�� ���� ���� �Ҵ� �� �ڷ� ���� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoCNF ),
                                             (void**) & sMyGraph->onConditionCNF )
              != IDE_SUCCESS );

    // To Fix PR-12743 NNF Filter����
    IDE_TEST( qmoCrtPathMgr::decideNormalType( aStatement,
                                               aFrom,
                                               aFrom->onCondition,
                                               aQuerySet->SFWGH->hints,
                                               ID_TRUE, // CNF Only��
                                               & sNormalType )
              != IDE_SUCCESS );

    switch ( sNormalType )
    {
        case QMO_NORMAL_TYPE_NNF:

            if ( aFrom->onCondition != NULL )
            {
                aFrom->onCondition->lflag &= ~QTC_NODE_NNF_FILTER_MASK;
                aFrom->onCondition->lflag |= QTC_NODE_NNF_FILTER_TRUE;
            }
            else
            {
                // Nothing To Do
            }

            IDE_TEST( qmoCnfMgr::init( aStatement,
                                       sMyGraph->onConditionCNF,
                                       aQuerySet,
                                       aFrom,
                                       NULL,
                                       aFrom->onCondition )
                      != IDE_SUCCESS );
            break;

        case QMO_NORMAL_TYPE_CNF:
            IDE_TEST( qmoNormalForm::normalizeCNF( aStatement,
                                                   aFrom->onCondition,
                                                   & sNormalCNF )
                      != IDE_SUCCESS );

            IDE_TEST( qmoCnfMgr::init( aStatement,
                                       sMyGraph->onConditionCNF,
                                       aQuerySet,
                                       aFrom,
                                       sNormalCNF,
                                       NULL )
                      != IDE_SUCCESS );
            break;

        case QMO_NORMAL_TYPE_DNF:
        case QMO_NORMAL_TYPE_NOT_DEFINED:
        default:
            IDE_FT_ASSERT( 0 );
            break;
    }

    sMyGraph->graph.left = sMyGraph->onConditionCNF->baseGraph[0];
    sMyGraph->graph.right = sMyGraph->onConditionCNF->baseGraph[1];

    // BUG-45296 rownum Pred �� left outer �� ���������� ������ �ȵ˴ϴ�.
    sMyGraph->graph.left->flag &= ~QMG_ROWNUM_PUSHED_MASK;
    sMyGraph->graph.left->flag |= QMG_ROWNUM_PUSHED_FALSE;
    
    sMyGraph->graph.right->flag &= ~QMG_ROWNUM_PUSHED_MASK;
    sMyGraph->graph.right->flag |= QMG_ROWNUM_PUSHED_FALSE;

    // To Fix BUG-8439
    // Disk/Memory ���� ����
    switch(  aQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            // �߰� ��� Type Hint�� ���� ���,
            // left �Ǵ� right�� disk�̸� disk
            if ( ( ( sMyGraph->graph.left->flag & QMG_GRAPH_TYPE_MASK )
                   == QMG_GRAPH_TYPE_DISK ) ||
                 ( ( sMyGraph->graph.right->flag & QMG_GRAPH_TYPE_MASK )
                   == QMG_GRAPH_TYPE_DISK ) )
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

    // join method �ʱ�ȭ
    sMyGraph->joinMethods = NULL;

    // joinable predicate / non joinable predicate �ʱ�ȭ
    sMyGraph->joinablePredicate = NULL;
    sMyGraph->nonJoinablePredicate = NULL;
    sMyGraph->pushedDownPredicate = NULL;

    // bucket count, hash temp table count �ʱ�ȭ
    sMyGraph->hashBucketCnt = 0;
    sMyGraph->hashTmpTblCnt = 0;

    // PROJ-2242 joinOrderFactor, joinSize �ʱ�ȭ
    sMyGraph->firstRowsFactor = QMO_COST_FIRST_ROWS_FACTOR_DEFAULT;
    sMyGraph->joinOrderFactor = 0;
    sMyGraph->joinSize        = 0;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::init( qcStatement * /* aStatement*/,
               qmgGraph    * aLeftGraph,
               qmgGraph    * aRightGraph,
               qmgGraph    * aGraph,
               idBool        aExistOrderFactor )
{
/***********************************************************************
 *
 * Description : Join Relation�� �����ϴ� qmgJoin Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (2) graph.type ����
 *    (3) graph.left�� aLeftGraph��, graph.right�� aRightGraph�� ����
 *    (4) graph.dependencies ����
 *    (5) graph.optimize�� graph.makePlan ����
 *
 ***********************************************************************/

    qmgJOIN * sMyGraph;
    qcDepInfo sOrDependencies;

    IDU_FIT_POINT_FATAL( "qmgJoin::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_FT_ASSERT( aLeftGraph != NULL );
    IDE_FT_ASSERT( aRightGraph != NULL );
    IDE_FT_ASSERT( aGraph != NULL );

    //---------------------------------------------------
    // Join Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgJOIN *) aGraph;

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );
    sMyGraph->graph.type = QMG_INNER_JOIN;
    sMyGraph->graph.left = aLeftGraph;
    sMyGraph->graph.right = aRightGraph;

    // BUG-45296 rownum Pred �� left outer �� ���������� ������ �ȵ˴ϴ�.
    sMyGraph->graph.left->flag &= ~QMG_ROWNUM_PUSHED_MASK;
    sMyGraph->graph.left->flag |= QMG_ROWNUM_PUSHED_FALSE;
    
    sMyGraph->graph.right->flag &= ~QMG_ROWNUM_PUSHED_MASK;
    sMyGraph->graph.right->flag |= QMG_ROWNUM_PUSHED_FALSE;

    sMyGraph->graph.myFrom = NULL;
    sMyGraph->graph.myQuerySet = aLeftGraph->myQuerySet;

    IDE_TEST( qtc::dependencyOr( & aLeftGraph->depInfo,
                                 & aRightGraph->depInfo,
                                 & sOrDependencies )
              != IDE_SUCCESS );

    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & sOrDependencies );

    // Graph�� �Լ� �����͸� ����
    sMyGraph->graph.optimize = qmgJoin::optimize;
    sMyGraph->graph.makePlan = qmgJoin::makePlan;
    sMyGraph->graph.printGraph = qmgJoin::printGraph;

    // Disk/Memory ���� ����
    switch(  sMyGraph->graph.myQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            // �߰� ��� Type Hint�� ���� ���,
            // left �Ǵ� right�� disk�̸� disk
            if ( ( ( aLeftGraph->flag & QMG_GRAPH_TYPE_MASK )
                   == QMG_GRAPH_TYPE_DISK ) ||
                 ( ( aRightGraph->flag & QMG_GRAPH_TYPE_MASK )
                   == QMG_GRAPH_TYPE_DISK ) )
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
    // Join Graph ���� ���� �ڷ� ���� �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->onConditionCNF = NULL;

    sMyGraph->joinMethods = NULL;

    sMyGraph->joinablePredicate = NULL;
    sMyGraph->nonJoinablePredicate = NULL;
    sMyGraph->pushedDownPredicate = NULL;

    sMyGraph->hashBucketCnt = 0;
    sMyGraph->hashTmpTblCnt = 0;

    // PROJ-2242
    if( aExistOrderFactor == ID_FALSE )
    {
        sMyGraph->joinOrderFactor = 0;
        sMyGraph->joinSize = 0;
    }
    else
    {
        // join ordering �������� ���� ����
    }
    sMyGraph->firstRowsFactor = QMO_COST_FIRST_ROWS_FACTOR_DEFAULT;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgJoin Graph�� ����ȭ
 *
 * Implementation :
 *    (1) Inner Join�� ���, ������ ����
 *        A. subquery�� ó��
 *        B. on Condition���� Transitive Predicate ����
 *        C. on Condition Predicate�� �з�
 *        D. left graph�� ����ȭ ����
 *        E. right graph�� ����ȭ ����
 *    (2) Join Method �� �ʱ�ȭ
 *    (3) Join Method ����
 *    (4) Join Method ���� �� ó��
 *    (5) ���� ��� ������ ����
 *    (6) Preserved Order, DISK/MEMORY ����
 *
 ***********************************************************************/

    qmgJOIN           * sMyGraph;
    qmoPredicate      * sPredicate;
    qmoPredicate      * sLowerPredicate;

    IDU_FIT_POINT_FATAL( "qmgJoin::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph = (qmgJOIN*)aGraph;

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

    if ( sMyGraph->onConditionCNF != NULL )
    {
        //------------------------------------------
        // Inner Join �� ���
        //    - Transitive Predicate ����
        //    - onCondition Predicate�� �з�
        //    - left graph�� ����ȭ ����
        //    - right graph�� ����ȭ ����
        //------------------------------------------

        IDE_TEST( qmoCnfMgr::generateTransitivePred4OnCondition(
                      aStatement,
                      sMyGraph->onConditionCNF,
                      sMyGraph->graph.myPredicate,
                      & sLowerPredicate )
                  != IDE_SUCCESS );
        
        IDE_TEST(
            qmoCnfMgr::classifyPred4OnCondition(
                aStatement,
                sMyGraph->onConditionCNF,
                & sMyGraph->graph.myPredicate,
                sLowerPredicate,
                sMyGraph->graph.myFrom->joinType )
            != IDE_SUCCESS );

        if ( ( sMyGraph->graph.flag & QMG_JOIN_ONLY_NL_MASK )
               == QMG_JOIN_ONLY_NL_TRUE )
        {
            sMyGraph->graph.left->flag &= ~QMG_JOIN_ONLY_NL_MASK;
            sMyGraph->graph.left->flag |= QMG_JOIN_ONLY_NL_TRUE;
            sMyGraph->graph.right->flag &= ~QMG_JOIN_ONLY_NL_MASK;
            sMyGraph->graph.right->flag |= QMG_JOIN_ONLY_NL_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( sMyGraph->graph.left->optimize( aStatement,
                                                  sMyGraph->graph.left )
                  != IDE_SUCCESS );

        IDE_TEST( sMyGraph->graph.right->optimize( aStatement,
                                                   sMyGraph->graph.right )
                  != IDE_SUCCESS );

        // fix BUG-19203
        IDE_TEST( qmoCnfMgr::optimizeSubQ4OnCondition(aStatement,
                                                      sMyGraph->onConditionCNF)
                  != IDE_SUCCESS );

        if( sMyGraph->onConditionCNF->joinPredicate != NULL )
        {
            IDE_TEST(
                qmoPred::classifyJoinPredicate(
                    aStatement,
                    sMyGraph->onConditionCNF->joinPredicate,
                    sMyGraph->graph.left,
                    sMyGraph->graph.right,
                    & sMyGraph->graph.depInfo )
                != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        //------------------------------------------
        // Subquery�� Graph ����
        //------------------------------------------
        // To fix BUG-19813
        // on condition �� �����ִ� predicate�� mypredicate�� �ٱ� ����
        // ���� subquery graph�� �����ؾ� ��
        // :on condition predicate��
        // qmoCnfMgr::optimizeSubQ4OnCondition�Լ��� ���� �̹� subquery graph��
        // �����Ͽ��� ����
        if ( sMyGraph->graph.myPredicate != NULL )
        {
            IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                                   sMyGraph->graph.myPredicate,
                                                   ID_FALSE ) // No KeyRange Tip
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
        
        //------------------------------------------
        // fix BUG-9972
        // on Condition CNF�� predicate �з� �� sMyGraph->graph.myPredicate ��
        // one table predicate�� left �Ǵ� right�� push selection �� ��
        // subquery�� ������ predicate�� ��� push selection���� �ʾ� �������� �� ����
        // on Condition CNF���� joinPredicate���� �з��� predicate �� oneTablePredicate
        // �� sMyGraph->graph.myPredicate�� ������� �Ŀ� �����ϰ� ó���ϵ��� ����
        //------------------------------------------
        if( sMyGraph->graph.myPredicate == NULL )
        {
            sMyGraph->graph.myPredicate = sMyGraph->onConditionCNF->oneTablePredicate;
        }
        else
        {
            for ( sPredicate = sMyGraph->graph.myPredicate;
                  sPredicate->next != NULL;
                  sPredicate = sPredicate->next ) ;
            sPredicate->next = sMyGraph->onConditionCNF->oneTablePredicate;
        }


        if ( sMyGraph->graph.myPredicate == NULL )
        {
            sMyGraph->graph.myPredicate =
                sMyGraph->onConditionCNF->joinPredicate;
        }
        else
        {
            for ( sPredicate = sMyGraph->graph.myPredicate;
                  sPredicate->next != NULL;
                  sPredicate = sPredicate->next ) ;
            sPredicate->next = sMyGraph->onConditionCNF->joinPredicate;
        }

        //------------------------------------------
        // Constant Predicate�� ó��
        //------------------------------------------

        // fix BUG-9791
        // ��) SELECT * FROM T1 INNER JOIN T2 ON 1 = 2;
        // inner join�� ����, on���� constant filter��
        // ó�������� ������ left graph�� ������.
        // ( left outer, full outer join�� ����,
        //   ���ǿ� ���� �ʴ� ���ڵ忡 ���ؼ��� null padding�ؾ� �ϹǷ�,
        //   on���� constant filter�� ���� �� ����,
        //   left, full outer join node���� filteró���ؾ� �Ѵ�. )
        IDE_TEST(
            qmoCnfMgr::pushSelection4ConstantFilter( aStatement,
                                                     (qmgGraph*)sMyGraph,
                                                     sMyGraph->onConditionCNF )
            != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------------
        // Subquery�� Graph ����
        //------------------------------------------
        
        // To Fix BUG-8219
        if ( sMyGraph->graph.myPredicate != NULL )
        {
            IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                                   sMyGraph->graph.myPredicate,
                                                   ID_FALSE ) // No KeyRange Tip
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    if ( sMyGraph->graph.myPredicate != NULL )
    {

        //------------------------------------------
        // Join Predicate�� �з�
        //------------------------------------------

        IDE_TEST( qmoPred::classifyJoinPredicate( aStatement,
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
            IDE_TEST( qmoPred::copyPredicate4Partition(
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
    if ( ( sMyGraph->graph.flag & QMG_JOIN_ONLY_NL_MASK )
         == QMG_JOIN_ONLY_NL_FALSE )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoJoinMethod ) * QMG_INNER_JOIN_METHOD_COUNT,
                                                   (void **)&sMyGraph->joinMethods )
                  != IDE_SUCCESS );

        // nested loop join method�� �ʱ�ȭ
        IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->firstRowsFactor,
                                          sMyGraph->graph.myPredicate,
                                          QMO_JOIN_METHOD_NL,
                                          &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_NESTED] )
                  != IDE_SUCCESS );

        // hash based join method�� �ʱ�ȭ
        IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->firstRowsFactor,
                                          sMyGraph->graph.myPredicate,
                                          QMO_JOIN_METHOD_HASH,
                                          &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_HASH] )
                  != IDE_SUCCESS );

        // sort based join method�� �ʱ�ȭ
        IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->firstRowsFactor,
                                          sMyGraph->graph.myPredicate,
                                          QMO_JOIN_METHOD_SORT,
                                          &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_SORT] )
                  != IDE_SUCCESS );

        // merge join method�� �ʱ�ȭ
        IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->firstRowsFactor,
                                          sMyGraph->graph.myPredicate,
                                          QMO_JOIN_METHOD_MERGE,
                                          &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_MERGE] )
                  != IDE_SUCCESS );
    }
    else
    {
        IDU_FIT_POINT("qmgJoin::optimize::alloc::joinMethods",
                      idERR_ABORT_InsufficientMemory);
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoJoinMethod ),
                                                   (void **)&sMyGraph->joinMethods )
                  != IDE_SUCCESS );

        IDE_TEST( qmoJoinMethodMgr::init( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->firstRowsFactor,
                                          sMyGraph->graph.myPredicate,
                                          QMO_JOIN_METHOD_NL,
                                          &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_NESTED] )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // �� Join Method �� ���� ���� cost�� Join Method�� ����
    //------------------------------------------

    IDE_TEST( qmgJoin::selectJoinMethodCost( aStatement,
                                             & sMyGraph->graph,
                                             sMyGraph->graph.myPredicate,
                                             sMyGraph->joinMethods,
                                             & sMyGraph->selectedJoinMethod )
              != IDE_SUCCESS );

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // record size ����
    sMyGraph->graph.costInfo.recordSize =
        sMyGraph->graph.left->costInfo.recordSize +
        sMyGraph->graph.right->costInfo.recordSize;

    // �� qmgJoin �� joinOrderFactor, joinSize ���
    IDE_TEST( qmoSelectivity::setJoinOrderFactor(
                  aStatement,
                  & sMyGraph->graph,
                  sMyGraph->graph.myPredicate,
                  &sMyGraph->joinOrderFactor,
                  &sMyGraph->joinSize )
              != IDE_SUCCESS );

    // input record count ����
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt *
        sMyGraph->graph.right->costInfo.outputRecordCnt;

    // output record count ����
    sMyGraph->graph.costInfo.outputRecordCnt =
        sMyGraph->graph.costInfo.inputRecordCnt *
        sMyGraph->graph.costInfo.selectivity;

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

    // To Fix PR-8032
    // joinPredicate�� nonJoinablePredicate�� Output���� �޾ƾ� ��.
    IDE_TEST(
        qmgJoin::afterJoinMethodDecision( aStatement,
                                          & sMyGraph->graph,
                                          sMyGraph->selectedJoinMethod,
                                          & sMyGraph->graph.myPredicate,
                                          & sMyGraph->joinablePredicate,
                                          & sMyGraph->nonJoinablePredicate )
        != IDE_SUCCESS );

    //------------------------------------------
    // Preserved Order
    //------------------------------------------

    IDE_TEST( makePreservedOrder( aStatement,
                                  & sMyGraph->graph,
                                  sMyGraph->selectedJoinMethod,
                                  sMyGraph->joinablePredicate )
              != IDE_SUCCESS );

    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
    aGraph->flag |= ((aGraph->left->flag & QMG_PARALLEL_EXEC_MASK) |
                     (aGraph->right->flag & QMG_PARALLEL_EXEC_MASK));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgJoin���� ���� Plan �� �����Ѵ�.
 *
 * Implementation :
 *     - qmgJoin���� ���� �������� �� Plan
 *
 *     1.  Nested Loop �迭
 *         1.1  Full Nested Loop Join�� ���
 *              - Filter�� ���� ���
 *
 *                   [JOIN]
 *
 *              - Filter�� �ִ� ��� (Right�� SCAN)
 *
 *                   [JOIN]
 *                   |    |
 *                        [SCAN]  : SCAN�� ���Ե�
 *
 *              - Filter�� �ִ� ��� (Right�� SCAN�� �ƴ� ���)
 *
 *                   [FILT]
 *                     |
 *                   [JOIN]
 * --> Filter�� �ִ� ��� (right�� SCAN�϶�) Join���� Selection���� �����ֱ�
 *     ������, Right�� scan���� �ƴ��� ���������ʿ䰡 ����. ���� Filter��
 *     ���� �ϴ��� ���ϴ����� ���и� ���ش�.
 *
 *         1.2  Full Store Nested Loop Join�� ���
 *
 *                 ( [FILT] )
 *                     |
 *                   [JOIN]
 *                   |    |
 *                        [SORT]
 *
 *         1.3  Index Nested Loop Join�� ���
 *
 *                   [JOIN]
 *                   |    |
 *                        [SCAN]
 *
 *         1.4  Anti-Outer Nested Loop Join�� ���
 *              - qmgFullOuter�κ��͸� ������.
 *
 *     2.  Sort-based �迭
 *
 *                      |
 *                  ( [FILT] )
 *                      |
 *                    [JOIN]
 *                    |    |
 *              ([SORT])   [SORT]
 *
 *         - Two-Pass Sort Join�� ���, Left�� SORT�� �����ǳ�
 *           Preserved Order�� �ִٸ� Two-Pass Sort Join�̶� �ϴ���
 *           Left�� SORT�� �������� �ʴ´�.
 *
 *     3.  Hash-based �迭
 *
 *                      |
 *                  ( [FILT] )
 *                      |
 *                    [JOIN]
 *                    |    |
 *              ([HASH])   [HASH]
 *
 *         - Two-Pass Hash Join�� ���, Left�� HASH�� ������
 *
 *     4.  Merge Join �迭
 *
 *                      |
 *                    [MGJN]
 *                    |    |
 *          [SORT,MGJN]    [SORT,MGJN]
 *
 *         - SCAN����� ��� ���� Graph�κ��� ������.
 *
 ***********************************************************************/

    qmgJOIN         * sMyGraph;
    qmnPlan         * sOnFILT    = NULL;
    qmnPlan         * sWhereFILT = NULL;

    IDU_FIT_POINT_FATAL( "qmgJoin::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgJOIN*) aGraph;

    //---------------------------------------------------
    // Current CNF�� ���
    //---------------------------------------------------
    if ( sMyGraph->graph.myCNF != NULL )
    {
        sMyGraph->graph.myQuerySet->SFWGH->crtPath->currentCNF =
            sMyGraph->graph.myCNF;
    }
    else
    {
        // Nothing To Do
    }

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    // BUG-38410
    // SCAN parallel flag �� �ڽ� ���� �����ش�.
    aGraph->left->flag  |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);
    aGraph->right->flag |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);

    // Parent plan�� myPlan���� �ϴ� �����Ѵ�.
    sMyGraph->graph.myPlan = aParent->myPlan;

    //-----------------------------------------------
    // NNF Filter �� ó��
    //-----------------------------------------------

    // TODO: FILTER operator�� ������ �ʰ� JOIN operator��
    //       filter�� ó���Ǿ�� �Ѵ�.

    // ON ���� �����ϴ� NNF Filter ó��
    if ( sMyGraph->onConditionCNF != NULL )
    {
        if ( sMyGraph->onConditionCNF->nnfFilter != NULL )
        {
            //make FILT
            IDE_TEST( qmoOneNonPlan::initFILT(
                          aStatement ,
                          sMyGraph->graph.myQuerySet ,
                          sMyGraph->onConditionCNF->nnfFilter,
                          sMyGraph->graph.myPlan,
                          &sOnFILT) != IDE_SUCCESS);
            sMyGraph->graph.myPlan = sOnFILT;
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    // WHERE ���� �����ϴ� NNF Filter ó��
    if ( sMyGraph->graph.nnfFilter != NULL )
    {
        //make FILT
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement ,
                      sMyGraph->graph.myQuerySet ,
                      sMyGraph->graph.nnfFilter,
                      sMyGraph->graph.myPlan,
                      &sWhereFILT) != IDE_SUCCESS);
        sMyGraph->graph.myPlan = sWhereFILT;
    }
    else
    {
        // Nothing To Do
    }

    switch( sMyGraph->selectedJoinMethod->flag & QMO_JOIN_METHOD_MASK )
    {
        //---------------------------------------------------
        // Index Nested Loop Join�� ���
        //---------------------------------------------------

        case QMO_JOIN_METHOD_INDEX:
            IDE_TEST( makeIndexNLJoin( aStatement,
                                       sMyGraph )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_INVERSE_INDEX:
            IDE_TEST( makeInverseIndexNLJoin( aStatement,
                                              sMyGraph )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_FULL_NL:
            IDE_TEST( makeFullNLJoin( aStatement,
                                      sMyGraph,
                                      ID_FALSE )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_FULL_STORE_NL:
            IDE_TEST( makeFullNLJoin( aStatement,
                                      sMyGraph,
                                      ID_TRUE )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_TWO_PASS_HASH:
            IDE_TEST( makeHashJoin( aStatement,
                                    sMyGraph,
                                    ID_TRUE,    // TwoPass?
                                    ID_FALSE )  // Inverse?
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_ONE_PASS_HASH:
            IDE_TEST( makeHashJoin( aStatement,
                                    sMyGraph,
                                    ID_FALSE,
                                    ID_FALSE )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_INVERSE_HASH:
            IDE_TEST( makeHashJoin( aStatement,
                                    sMyGraph,
                                    ID_FALSE,   // TwoPass?
                                    ID_TRUE )   // Inverse?
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_TWO_PASS_SORT:
            IDE_TEST( makeSortJoin( aStatement,
                                    sMyGraph,
                                    ID_TRUE,   // TwoPass?
                                    ID_FALSE ) // Inverse?
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_ONE_PASS_SORT:
            IDE_TEST( makeSortJoin( aStatement,
                                    sMyGraph,
                                    ID_FALSE,  // TwoPass?
                                    ID_FALSE ) // Inverse?
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_INVERSE_SORT:
            IDE_TEST( makeSortJoin( aStatement,
                                    sMyGraph,
                                    ID_FALSE,  // TwoPass?
                                    ID_TRUE )  // Inverse?
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_MERGE:
            IDE_TEST( makeMergeJoin( aStatement,
                                     sMyGraph )
                      != IDE_SUCCESS );
            break;
    }

    //-----------------------------------------------
    // NNF Filter �� ó��
    //-----------------------------------------------

    // WHERE ���� �����ϴ� NNF Filter ó��
    if ( sMyGraph->graph.nnfFilter != NULL )
    {
        //make FILT
        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      sMyGraph->graph.myQuerySet ,
                      sMyGraph->graph.nnfFilter,
                      NULL,
                      sMyGraph->graph.myPlan ,
                      sWhereFILT) != IDE_SUCCESS);

        sMyGraph->graph.myPlan = sWhereFILT;

        qmg::setPlanInfo( aStatement, sWhereFILT, &(sMyGraph->graph) );
    }
    else
    {
        // Nothing To Do
    }

    // ON ���� �����ϴ� NNF Filter ó��
    if ( sMyGraph->onConditionCNF != NULL )
    {
        if ( sMyGraph->onConditionCNF->nnfFilter != NULL )
        {
            //make FILT
            IDE_TEST( qmoOneNonPlan::makeFILT(
                          aStatement ,
                          sMyGraph->graph.myQuerySet ,
                          sMyGraph->onConditionCNF->nnfFilter,
                          NULL,
                          sMyGraph->graph.myPlan ,
                          sOnFILT) != IDE_SUCCESS);

            sMyGraph->graph.myPlan = sOnFILT;

            qmg::setPlanInfo( aStatement, sOnFILT, &(sMyGraph->graph) );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeChildPlan( qcStatement     * aStatement,
                        qmgJOIN         * aMyGraph,
                        qmnPlan         * aLeftPlan,
                        qmnPlan         * aRightPlan )
{

    IDU_FIT_POINT_FATAL( "qmgJoin::makeChildPlan::__FT__" );

    //---------------------------------------------------
    // ���� Plan�� ���� , Join�̹Ƿ� ���� ��� ����
    //---------------------------------------------------

    aMyGraph->graph.myPlan = aLeftPlan;

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph ,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);

    aMyGraph->graph.myPlan = aRightPlan;

    IDE_TEST( aMyGraph->graph.right->makePlan( aStatement ,
                                               &aMyGraph->graph ,
                                               aMyGraph->graph.right )
              != IDE_SUCCESS);

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_JOIN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeSortRange( qcStatement  * aStatement,
                        qmgJOIN      * aMyGraph,
                        qtcNode     ** aRange )
{
    qtcNode         * sCNFRange;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeSortRange::__FT__" );

    //---------------------------------------------------
    // SORT�� ���� range , HASH�� ���� filter ����
    //---------------------------------------------------
    if( aMyGraph->joinablePredicate != NULL )
    {
        IDE_TEST( qmoPred::linkPredicate( aStatement ,
                                          aMyGraph->joinablePredicate ,
                                          &sCNFRange ) != IDE_SUCCESS );

        // To Fix PR-8019
        // Key Range ������ ���ؼ��� DNF���·� ��ȯ�Ͽ��� �Ѵ�.
        IDE_TEST( qmoNormalForm::normalizeDNF( aStatement ,
                                               sCNFRange ,
                                               aRange ) != IDE_SUCCESS );

        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           *aRange ) != IDE_SUCCESS );
    }
    else
    {
        *aRange = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeHashFilter( qcStatement  * aStatement,
                         qmgJOIN      * aMyGraph,
                         qtcNode     ** aFilter )
{

    IDU_FIT_POINT_FATAL( "qmgJoin::makeHashFilter::__FT__" );

    IDE_TEST( makeSortRange( aStatement, aMyGraph, aFilter ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeIndexNLJoin( qcStatement    * aStatement,
                          qmgJOIN        * aMyGraph )
{
    qmnPlan         * sJOIN = NULL;
    qmsQuerySet     * sViewQuerySet;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeIndexNLJoin::__FT__" );

    //-----------------------------------------------------
    //        [JOIN]
    //         /  `.
    //    [LEFT]  [RIGHT]
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init JOIN
    //-----------------------

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if ( (aMyGraph->graph.right->type == QMG_SELECTION ) &&
         (aMyGraph->graph.right->myFrom->tableRef->view != NULL) )
    {
        sViewQuerySet = aMyGraph->graph.right->left->myQuerySet;
    }
    else
    {
        sViewQuerySet = NULL;
    }

    IDE_TEST( qmoTwoNonPlan::initJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sViewQuerySet,
                                       NULL,
                                       NULL,
                                       aMyGraph->pushedDownPredicate ,
                                       aMyGraph->graph.myPlan,
                                       &sJOIN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sJOIN;

    // BUG-38410
    // �ݺ� ���� �� ��� SCAN Parallel �� �����Ѵ�.
    aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_TRUE;

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph,
                             sJOIN,
                             sJOIN )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make JOIN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.type,
                                       aMyGraph->graph.left->myPlan ,
                                       aMyGraph->graph.right->myPlan ,
                                       sJOIN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sJOIN;

    qmg::setPlanInfo( aStatement, sJOIN, &(aMyGraph->graph) );

    sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
    sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_INDEX;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmgJoin::makeInverseIndexNLJoin( qcStatement    * aStatement,
                                        qmgJOIN        * aMyGraph )
{
    //-----------------------------------------------------
    //        [JOIN]
    //         /  `.
    //    [HASH]  [RIGHT] (+index)
    //       |
    //    [LEFT]  
    //-----------------------------------------------------

    UInt              sMtrFlag    = 0;
    qmnPlan         * sJOIN       = NULL;
    qmnPlan         * sFILT       = NULL;
    qmnPlan         * sLeftHASH   = NULL;
    qtcNode         * sHashFilter = NULL;
    qtcNode         * sFilter     = NULL;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeInverseIndexNLJoin::__FT__" );

    IDE_TEST( makeHashFilter( aStatement,
                              aMyGraph,
                              &sHashFilter )
              != IDE_SUCCESS );

    IDE_TEST( extractFilter( aStatement,
                             aMyGraph,
                             aMyGraph->nonJoinablePredicate,
                             &sFilter )
              != IDE_SUCCESS );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init FILT if filter exist
    //-----------------------

    IDE_TEST( initFILT( aStatement,
                        aMyGraph,
                        &sFILT )
              != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sFILT;

    //-----------------------
    // init JOIN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::initJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       NULL,
                                       NULL,
                                       NULL,
                                       aMyGraph->pushedDownPredicate ,
                                       aMyGraph->graph.myPlan,
                                       &sJOIN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sJOIN;

    //-----------------------------
    // init LEFT HASH (Distinct Sequential Search)
    //-----------------------------

    IDE_TEST( qmoOneMtrPlan::initHASH( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       sHashFilter,
                                       ID_TRUE, // isLeftHash
                                       ID_TRUE, // isDistinct
                                       sJOIN,
                                       &sLeftHASH ) != IDE_SUCCESS );

    // BUG-38410
    // Hash �ǹǷ� left �� �ѹ��� ����ǰ� hash �� ������ �����Ѵ�.
    // ���� left �� SCAN �� ���� parallel �� ����Ѵ�.
    aMyGraph->graph.left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_TRUE;

    //-----------------------
    // ���� plan ���� (LEFT)
    //-----------------------

    aMyGraph->graph.myPlan = sJOIN;

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph ,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    sMtrFlag = 0;

    sMtrFlag &= ~QMO_MAKEHASH_METHOD_MASK;
    sMtrFlag |= QMO_MAKEHASH_HASH_BASED_JOIN;

    sMtrFlag &= ~QMO_MAKEHASH_POSITION_MASK;
    sMtrFlag |= QMO_MAKEHASH_POSITION_LEFT;

    sMtrFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;

    if ( ( aMyGraph->selectedJoinMethod->flag & 
           QMO_JOIN_METHOD_LEFT_STORAGE_MASK )
         == QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY )
    {
        sMtrFlag |= QMO_MAKEHASH_MEMORY_TEMP_TABLE;  
    }
    else
    {
        sMtrFlag |= QMO_MAKEHASH_DISK_TEMP_TABLE;
    }

    // isAllAttrKey�� TRUE�� �α� ����, makeHASH ���� ȣ���Ѵ�.
    IDE_TEST( qmoOneMtrPlan::makeHASH( 
                  aStatement,
                  aMyGraph->graph.myQuerySet,
                  sMtrFlag,
                  ((qmgJOIN*)&aMyGraph->graph)->hashTmpTblCnt,
                  ((qmgJOIN*)&aMyGraph->graph)->hashBucketCnt,
                  sHashFilter,
                  aMyGraph->graph.left->myPlan,
                  sLeftHASH,
                  ID_TRUE ) // aAllAttrToKey
              != IDE_SUCCESS );

    qmg::setPlanInfo( aStatement, sLeftHASH, &aMyGraph->graph );

    //-----------------------
    // ���� plan ���� (RIGHT)
    //-----------------------

    aMyGraph->graph.myPlan = sJOIN;

    IDE_TEST( aMyGraph->graph.right->makePlan( aStatement ,
                                               &aMyGraph->graph ,
                                               aMyGraph->graph.right )
              != IDE_SUCCESS );

    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_JOIN;

    //-----------------------
    // make JOIN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.type,
                                       sLeftHASH ,
                                       aMyGraph->graph.right->myPlan ,
                                       sJOIN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sJOIN;

    qmg::setPlanInfo( aStatement, sJOIN, &(aMyGraph->graph) );

    //-----------------------
    // Join Type ����
    //-----------------------

    sJOIN->flag &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
    sJOIN->flag |= QMN_PLAN_JOIN_METHOD_INVERSE_INDEX;

    //-----------------------
    // make FILT if filter exist
    //-----------------------

    IDE_TEST( makeFILT( aStatement,
                        aMyGraph,
                        sFILT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeFullNLJoin( qcStatement    * aStatement,
                         qmgJOIN        * aMyGraph,
                         idBool           aIsStored )
{
    UInt              sMtrFlag;
    UInt              sJoinFlag;
    qmnPlan         * sJOIN   = NULL;
    qmnPlan         * sSORT   = NULL;
    qmnPlan         * sFILT   = NULL;
    qtcNode         * sFilter = NULL;
    qmsQuerySet     * sViewQuerySet;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeFullNLJoin::__FT__" );

    //-----------------------------------------------------
    //        [*FILT]
    //           |
    //        [JOIN]
    //         /  `.
    //    [LEFT]  [*SORT]
    //               |
    //            [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init FILT if filter exist
    //-----------------------

    IDE_TEST( extractFilter( aStatement,
                             aMyGraph,
                             aMyGraph->graph.myPredicate,
                             &sFilter )
              != IDE_SUCCESS );

    IDE_TEST( initFILT( aStatement,
                        aMyGraph,
                        &sFILT )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sFILT;

    //-----------------------
    // init JOIN
    //-----------------------

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if ( (aMyGraph->graph.right->type == QMG_SELECTION ) &&
         (aMyGraph->graph.right->myFrom->tableRef->view != NULL) )
    {
        sViewQuerySet = aMyGraph->graph.right->left->myQuerySet;
    }
    else
    {
        sViewQuerySet = NULL;
    }

    IDE_TEST( qmoTwoNonPlan::initJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sViewQuerySet,
                                       NULL,
                                       sFilter,
                                       aMyGraph->pushedDownPredicate,
                                       aMyGraph->graph.myPlan ,
                                       &sJOIN ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sJOIN;

    if( aIsStored == ID_TRUE )
    {
        // Full store nested loop join�� ���

        //-----------------------
        // init right SORT
        //-----------------------

        IDE_TEST( qmoOneMtrPlan::initSORT( aStatement,
                                           aMyGraph->graph.myQuerySet,
                                           aMyGraph->graph.myPlan,
                                           &sSORT )
                  != IDE_SUCCESS );

        // BUG-38410
        // Sort �ǹǷ� right �� �ѹ��� ����ǰ� sort �� ������ �����Ѵ�.
        // ���� right �� SCAN �� ���� parallel �� ����Ѵ�.
        aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;
    }
    else
    {
        // Full nested loop join�� ���
        // Right child�� parent�� join���� ����
        sSORT = aMyGraph->graph.myPlan;

        // BUG-38410
        // �ݺ� ���� �� ��� SCAN Parallel �� �����Ѵ�.
        aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_TRUE;
    }

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph,
                             sJOIN,
                             sSORT )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make JOIN
    //-----------------------

    if( aIsStored == ID_TRUE )
    {
        // Full store nested loop join�� ���

        //----------------------------
        // SORT�� ����
        //----------------------------

        sMtrFlag = 0;

        sMtrFlag &= ~QMO_MAKESORT_METHOD_MASK;
        sMtrFlag |= QMO_MAKESORT_STORE;

        sMtrFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
        sMtrFlag |= QMO_MAKESORT_PRESERVED_FALSE;

        //���� ��ü�� ����
        sJoinFlag = 0;

        sJoinFlag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

        if ( ( sJoinFlag &
               QMO_JOIN_METHOD_RIGHT_STORAGE_MASK )
             == QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY )
        {
            sMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
            sMtrFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
        }
        else
        {
            sMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
            sMtrFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
        }

        //-----------------------
        // make right SORT
        //-----------------------

        IDE_TEST( qmoOneMtrPlan::makeSORT( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           sMtrFlag ,
                                           NULL ,
                                           aMyGraph->graph.right->myPlan ,
                                           aMyGraph->graph.costInfo.inputRecordCnt,
                                           sSORT ) != IDE_SUCCESS);

        qmg::setPlanInfo( aStatement, sSORT, &(aMyGraph->graph) );
    }
    else
    {
        // Full nested loop join�� ���
        // Right child graph�� operator�� right child�� ����
        sSORT = aMyGraph->graph.right->myPlan;
    }

    IDE_TEST( qmoTwoNonPlan::makeJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.type,
                                       aMyGraph->graph.left->myPlan ,
                                       sSORT ,
                                       sJOIN ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sJOIN;

    qmg::setPlanInfo( aStatement, sJOIN, &(aMyGraph->graph) );

    if( aIsStored == ID_TRUE )
    {
        sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
        sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_FULL_STORE_NL;
    }
    else
    {
        sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
        sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_FULL_NL;
    }

    //-----------------------
    // make FILT if filter exist
    //-----------------------

    IDE_TEST( makeFILT( aStatement,
                        aMyGraph,
                        sFILT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeHashJoin( qcStatement     * aStatement,
                       qmgJOIN         * aMyGraph,
                       idBool            aIsTwoPass,
                       idBool            aIsInverse )
{
    UInt              sMtrFlag    = 0;
    UInt              sJoinFlag;
    qmnPlan         * sFILT       = NULL;
    qmnPlan         * sJOIN       = NULL;
    qmnPlan         * sLeftHASH   = NULL;
    qmnPlan         * sRightHASH  = NULL;
    qtcNode         * sHashFilter = NULL;
    qtcNode         * sFilter     = NULL;
    qmsQuerySet     * sViewQuerySet;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeHashJoin::__FT__" );

    //-----------------------------------------------------
    //           [JOIN]
    //            /  `.
    //       [*HASH]  [HASH]
    //          |        |
    //       [LEFT]   [RIGHT]
    // * Option
    //-----------------------------------------------------

    IDE_TEST( makeHashFilter( aStatement,
                              aMyGraph,
                              &sHashFilter )
              != IDE_SUCCESS );

    IDE_TEST( extractFilter( aStatement,
                             aMyGraph,
                             aMyGraph->nonJoinablePredicate,
                             &sFilter )
              != IDE_SUCCESS );

    /* BUG-46800 */
    moveConstantPred( aMyGraph, aIsInverse );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init FILT if filter exist
    //-----------------------

    IDE_TEST( initFILT( aStatement,
                        aMyGraph,
                        &sFILT )
              != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sFILT;

    //-----------------------
    // init JOIN
    //-----------------------

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if ( (aMyGraph->graph.right->type == QMG_SELECTION ) &&
         (aMyGraph->graph.right->myFrom->tableRef->view != NULL) )
    {
        sViewQuerySet = aMyGraph->graph.right->left->myQuerySet;
    }
    else
    {
        sViewQuerySet = NULL;
    }

    IDE_TEST( qmoTwoNonPlan::initJOIN(
                  aStatement,
                  aMyGraph->graph.myQuerySet,
                  sViewQuerySet,
                  sHashFilter,
                  sFilter,
                  NULL,
                  sFILT,
                  &sJOIN )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sJOIN;

    //-----------------------
    // init right HASH
    //-----------------------

    qmc::disableSealTrueFlag( sJOIN->resultDesc );
    IDE_TEST( qmoOneMtrPlan::initHASH(
                  aStatement,
                  aMyGraph->graph.myQuerySet,
                  sHashFilter,
                  ID_FALSE,     // isLeftHash
                  ID_FALSE,     // isDistinct
                  sJOIN,
                  &sRightHASH )
              != IDE_SUCCESS );

    if ( aIsTwoPass == ID_TRUE )
    {
        // Two pass hash join�� ���

        //-----------------------
        // init left HASH
        //-----------------------

        IDE_TEST( qmoOneMtrPlan::initHASH(
                      aStatement,
                      aMyGraph->graph.myQuerySet,
                      sHashFilter,
                      ID_TRUE,  // isLeftHash
                      ID_FALSE, // isDistinct
                      sJOIN,
                      &sLeftHASH )
                  != IDE_SUCCESS );

        // BUG-38410
        // Hash �ǹǷ� left �� �ѹ��� ����ǰ� hash �� ���븸 �����Ѵ�.
        // ���� left �� SCAN �� ���� parallel �� ����Ѵ�.
        aMyGraph->graph.left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aMyGraph->graph.left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;
    }
    else
    {
        // One pass hash join�� ���
        // Left child�� parent�� join�� �����Ѵ�.
        sLeftHASH = sJOIN;
    }

    // BUG-38410
    // Hash �ǹǷ� right �� �ѹ��� ����ǰ� sort �� ���븸 �����Ѵ�.
    // ���� right �� SCAN �� ���� parallel �� ����Ѵ�.
    aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph,
                             sLeftHASH,   // left child�� parent�� hash �Ǵ� join
                             sRightHASH ) // right child�� parent�� hash
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    if( aIsTwoPass == ID_TRUE )
    {
        // Two pass hash join�� ���

        //-----------------------
        // make left HASH
        //-----------------------

        sMtrFlag = 0;

        sMtrFlag &= ~QMO_MAKEHASH_METHOD_MASK;
        sMtrFlag |= QMO_MAKEHASH_HASH_BASED_JOIN;

        // To Fix PR-8032
        // HASH�� ���Ǵ� ��ġ�� ǥ���ؾ���.
        sMtrFlag &= ~QMO_MAKEHASH_POSITION_MASK;
        sMtrFlag |= QMO_MAKEHASH_POSITION_LEFT;

        //���� ��ü�� ����
        sJoinFlag = 0;

        sJoinFlag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

        IDE_TEST( qmg::makeLeftHASH4Join(
                      aStatement,
                      &aMyGraph->graph,
                      sMtrFlag,
                      sJoinFlag,
                      sHashFilter,
                      aMyGraph->graph.left->myPlan,
                      sLeftHASH )
                  != IDE_SUCCESS );
    }
    else
    {
        // One pass hash join�� ���
        // Join�� left child�� left graph�� plan�� �����Ѵ�.
        sLeftHASH = aMyGraph->graph.left->myPlan;
    }

    //-----------------------
    // make right HASH
    //-----------------------

    sMtrFlag = 0;

    sMtrFlag &= ~QMO_MAKEHASH_METHOD_MASK;
    sMtrFlag |= QMO_MAKEHASH_HASH_BASED_JOIN;

    // To Fix PR-8032
    // HASH�� ���Ǵ� ��ġ�� ǥ���ؾ���.
    sMtrFlag &= ~QMO_MAKEHASH_POSITION_MASK;
    sMtrFlag |= QMO_MAKEHASH_POSITION_RIGHT;

    //���� ��ü�� ����
    sJoinFlag = 0;

    sJoinFlag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    IDE_TEST( qmg::makeRightHASH4Join(
                  aStatement,
                  &aMyGraph->graph,
                  sMtrFlag,
                  sJoinFlag,
                  sHashFilter,
                  aMyGraph->graph.right->myPlan,
                  sRightHASH )
              != IDE_SUCCESS );

    //-----------------------
    // make JOIN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.type,
                                       sLeftHASH ,
                                       sRightHASH ,
                                       sJOIN ) != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sJOIN;

    qmg::setPlanInfo( aStatement, sJOIN, &(aMyGraph->graph) );

    if( aIsTwoPass == ID_TRUE )
    {
        sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
        sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_TWO_PASS_HASH;
    }
    else
    {
        /* PROJ-2339 */
        if ( aIsInverse == ID_TRUE )
        {
            sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
            sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_INVERSE_HASH;
        }
        else
        {
            sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
            sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_ONE_PASS_HASH;
        }
    }

    //-----------------------
    // make FILT if filter exist
    //-----------------------

    IDE_TEST( makeFILT( aStatement,
                        aMyGraph,
                        sFILT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeSortJoin( qcStatement     * aStatement,
                       qmgJOIN         * aMyGraph,
                       idBool            aIsTwoPass,
                       idBool            aIsInverse )
{
    UInt              sMtrFlag   = 0;
    UInt              sJoinFlag  = 0;
    qmnPlan         * sFILT      = NULL;
    qmnPlan         * sJOIN      = NULL;
    qmnPlan         * sLeftSORT  = NULL;
    qmnPlan         * sRightSORT = NULL;
    qtcNode         * sSortRange = NULL;
    qtcNode         * sFilter    = NULL;
    qmsQuerySet     * sViewQuerySet;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeSortJoin::__FT__" );

    IDE_TEST( makeSortRange( aStatement,
                             aMyGraph,
                             &sSortRange )
              != IDE_SUCCESS );

    IDE_TEST( extractFilter( aStatement,
                             aMyGraph,
                             aMyGraph->nonJoinablePredicate,
                             &sFilter )
              != IDE_SUCCESS );

    /* BUG-46800 */
    moveConstantPred( aMyGraph, aIsInverse );

    //-----------------------------------------------------
    //          [JOIN]
    //           /  `.
    //      [*SORT]  [SORT]
    //         |        |
    //      [LEFT]   [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init FILT if filter exist
    //-----------------------

    IDE_TEST( initFILT( aStatement,
                        aMyGraph,
                        &sFILT )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sFILT;

    //-----------------------
    // init JOIN
    //-----------------------

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if ( (aMyGraph->graph.right->type == QMG_SELECTION ) &&
         (aMyGraph->graph.right->myFrom->tableRef->view != NULL) )
    {
        sViewQuerySet = aMyGraph->graph.right->left->myQuerySet;
    }
    else
    {
        sViewQuerySet = NULL;
    }

    // BUG-38049 sSortRange�� ���ڷ� �Ѱ��־����
    IDE_TEST( qmoTwoNonPlan::initJOIN(
                  aStatement,
                  aMyGraph->graph.myQuerySet,
                  sViewQuerySet,
                  sSortRange,
                  sFilter,
                  aMyGraph->graph.myPredicate,
                  sFILT,
                  &sJOIN )
              != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sJOIN;

    //-----------------------
    // init right SORT
    //-----------------------

    sJoinFlag = 0;
    sJoinFlag &= ~QMO_JOIN_RIGHT_NODE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_RIGHT_NODE_MASK);

    IDE_TEST( qmg::initRightSORT4Join( aStatement,
                                       &aMyGraph->graph,
                                       sJoinFlag,
                                       ID_TRUE,
                                       sSortRange,
                                       ID_TRUE,
                                       sJOIN,
                                       &sRightSORT )
              != IDE_SUCCESS);

    // BUG-38410
    // Sort �ǹǷ� right �� �ѹ��� ����ǰ� sort �� ���븸 �����Ѵ�.
    // ���� right �� SCAN �� ���� parallel �� ����Ѵ�.
    aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    if( aIsTwoPass == ID_TRUE )
    {
        // Two pass sort join�� ���

        //-----------------------
        // init left SORT
        //-----------------------

        sJoinFlag = 0;
        sJoinFlag &= ~QMO_JOIN_LEFT_NODE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_LEFT_NODE_MASK);

        IDE_TEST( qmg::initLeftSORT4Join( aStatement,
                                          &aMyGraph->graph,
                                          sJoinFlag,
                                          sSortRange,
                                          sJOIN,
                                          &sLeftSORT )
                  != IDE_SUCCESS);

        // BUG-38410
        // Sort �ǹǷ� left �� �ѹ��� ����ǰ� sort �� ���븸 �����Ѵ�.
        // ���� left �� SCAN �� ���� parallel �� ����Ѵ�.
        aMyGraph->graph.left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aMyGraph->graph.left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;
    }
    else
    {
        // One pass sort join�� ���
        // Left child�� parent�� join�� �����Ѵ�.
        sLeftSORT = sJOIN;
    }

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph,
                             sLeftSORT,   // left child�� parent�� sort �Ǵ� join
                             sRightSORT ) // right child�� parent�� sort
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    if( aIsTwoPass == ID_TRUE )
    {
        // Two pass sort join�� ���

        //-----------------------
        // make left SORT
        //-----------------------

        sMtrFlag = 0;

        sMtrFlag &= ~QMO_MAKESORT_METHOD_MASK;
        sMtrFlag |= QMO_MAKESORT_SORT_BASED_JOIN;

        sMtrFlag &= ~QMO_MAKESORT_POSITION_MASK;
        sMtrFlag |= QMO_MAKESORT_POSITION_LEFT;

        //���� ��ü�� ����
        sJoinFlag = 0;

        sJoinFlag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

        sJoinFlag &= ~QMO_JOIN_LEFT_NODE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_LEFT_NODE_MASK);

        IDE_TEST( qmg::makeLeftSORT4Join( aStatement,
                                          &aMyGraph->graph,
                                          sMtrFlag,
                                          sJoinFlag,
                                          aMyGraph->graph.left->myPlan,
                                          sLeftSORT )
                  != IDE_SUCCESS);
    }
    else
    {
        // One pass sort join�� ���
        // Join�� left child�� left graph�� plan�� �����Ѵ�.
        sLeftSORT = aMyGraph->graph.left->myPlan;
    }

    //-----------------------
    // make right SORT
    //-----------------------

    sMtrFlag = 0;

    sMtrFlag &= ~QMO_MAKESORT_METHOD_MASK;
    sMtrFlag |= QMO_MAKESORT_SORT_BASED_JOIN;

    sMtrFlag &= ~QMO_MAKESORT_POSITION_MASK;
    sMtrFlag |= QMO_MAKESORT_POSITION_RIGHT;

    //���� ��ü�� ����
    sJoinFlag = 0;

    sJoinFlag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    sJoinFlag &= ~QMO_JOIN_RIGHT_NODE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_RIGHT_NODE_MASK);

    IDE_TEST( qmg::makeRightSORT4Join( aStatement,
                                       &aMyGraph->graph,
                                       sMtrFlag,
                                       sJoinFlag,
                                       ID_TRUE,
                                       aMyGraph->graph.right->myPlan,
                                       sRightSORT )
              != IDE_SUCCESS);

    //-----------------------
    // make JOIN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeJOIN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.type,
                                       sLeftSORT ,
                                       sRightSORT ,
                                       sJOIN ) != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sJOIN;

    qmg::setPlanInfo( aStatement, sJOIN, &(aMyGraph->graph) );

    if( aIsTwoPass == ID_TRUE )
    {
        sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
        sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_TWO_PASS_SORT;
    }
    else
    {
        if ( aIsInverse == ID_TRUE )
        {
            sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
            sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_INVERSE_SORT;
        }
        else
        {
            sJOIN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
            sJOIN->flag       |= QMN_PLAN_JOIN_METHOD_ONE_PASS_SORT;
        }
    }

    //-----------------------
    // make FILT if filter exist
    //-----------------------

    IDE_TEST( makeFILT( aStatement,
                        aMyGraph,
                        sFILT )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeMergeJoin( qcStatement     * aStatement,
                        qmgJOIN         * aMyGraph )
{
    UInt              sJoinFlag;
    UInt              sMtrFlag;
    qmnPlan         * sLeftSORT  = NULL;
    qmnPlan         * sRightSORT = NULL;
    qmnPlan         * sMGJN      = NULL;
    qtcNode         * sSortRange = NULL;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeMergeJoin::__FT__" );

    IDE_TEST( makeSortRange( aStatement,
                             aMyGraph,
                             &sSortRange )
              != IDE_SUCCESS );

    //-----------------------------------------------------
    //          [MGJN]
    //           /  `.
    //      [*SORT]  [*SORT]
    //         |        |
    //      [LEFT]   [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init MGJN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::initMGJN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->joinablePredicate ,
                                       aMyGraph->nonJoinablePredicate ,
//                                       aMyGraph->graph.myPredicate,
                                       aMyGraph->graph.myPlan,
                                       &sMGJN) != IDE_SUCCESS);

    //-----------------------
    // init left SORT
    //-----------------------

    sJoinFlag = 0;

    sJoinFlag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

    sJoinFlag &= ~QMO_JOIN_LEFT_NODE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_LEFT_NODE_MASK);

    IDE_TEST( qmg::initLeftSORT4Join( aStatement,
                                      &aMyGraph->graph,
                                      sJoinFlag,
                                      sSortRange,
                                      sMGJN,
                                      &sLeftSORT )
              != IDE_SUCCESS);

    // BUG-38410
    // Sort �ǹǷ� left �� �ѹ��� ����ǰ� sort �� ���븸 �����Ѵ�.
    // ���� left �� SCAN �� ���� parallel �� ����Ѵ�.
    aMyGraph->graph.left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    //-----------------------
    // init right SORT
    //-----------------------

    sJoinFlag = 0;

    sJoinFlag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

    sJoinFlag &= ~QMO_JOIN_RIGHT_NODE_MASK;
    sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                  QMO_JOIN_RIGHT_NODE_MASK);

    IDE_TEST( qmg::initRightSORT4Join( aStatement,
                                       &aMyGraph->graph,
                                       sJoinFlag,
                                       ID_TRUE,
                                       sSortRange,
                                       ID_FALSE,
                                       sMGJN,
                                       &sRightSORT )
              != IDE_SUCCESS);

    // BUG-38410
    // Sort �ǹǷ� right �� �ѹ��� ����ǰ� sort �� ���븸 �����Ѵ�.
    // ���� right �� SCAN �� ���� parallel �� ����Ѵ�.
    aMyGraph->graph.right->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aMyGraph->graph.right->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph,
                             sLeftSORT,
                             sRightSORT )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    if ( sRightSORT != sMGJN )
    {
        // Right child�� SORT�� ������ ���

        //-----------------------
        // make right SORT
        //-----------------------

        sMtrFlag = 0;

        sMtrFlag &= ~QMO_MAKESORT_METHOD_MASK;
        sMtrFlag |= QMO_MAKESORT_SORT_MERGE_JOIN;

        sMtrFlag &= ~QMO_MAKESORT_POSITION_MASK;
        sMtrFlag |= QMO_MAKESORT_POSITION_RIGHT;

        //���� ��ü�� ����

        sJoinFlag = 0;

        sJoinFlag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_METHOD_RIGHT_STORAGE_MASK);

        sJoinFlag &= ~QMO_JOIN_RIGHT_NODE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_RIGHT_NODE_MASK);

        IDE_TEST( qmg::makeRightSORT4Join( aStatement,
                                           &aMyGraph->graph,
                                           sMtrFlag,
                                           sJoinFlag,
                                           ID_TRUE,
                                           aMyGraph->graph.right->myPlan,
                                           sRightSORT )
                  != IDE_SUCCESS);
    }
    else
    {
        // Right child�� SORT�� �ʿ���� ���

        sRightSORT = aMyGraph->graph.right->myPlan;
    }

    if ( sLeftSORT != sMGJN )
    {
        // Left child�� SORT�� ������ ���

        //-----------------------
        // make left SORT
        //-----------------------

        sMtrFlag = 0;

        sMtrFlag &= ~QMO_MAKESORT_METHOD_MASK;
        sMtrFlag |= QMO_MAKESORT_SORT_MERGE_JOIN;

        sMtrFlag &= ~QMO_MAKESORT_POSITION_MASK;
        sMtrFlag |= QMO_MAKESORT_POSITION_LEFT;

        //���� ��ü�� ����

        sJoinFlag = 0;

        sJoinFlag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_METHOD_LEFT_STORAGE_MASK);

        sJoinFlag &= ~QMO_JOIN_LEFT_NODE_MASK;
        sJoinFlag |= (aMyGraph->selectedJoinMethod->flag &
                      QMO_JOIN_LEFT_NODE_MASK);

        IDE_TEST( qmg::makeLeftSORT4Join( aStatement,
                                          &aMyGraph->graph,
                                          sMtrFlag,
                                          sJoinFlag,
                                          aMyGraph->graph.left->myPlan,
                                          sLeftSORT )
                  != IDE_SUCCESS);
    }
    else
    {
        // Left child�� SORT�� �ʿ���� ���

        sLeftSORT = aMyGraph->graph.left->myPlan;
    }

    //-----------------------
    // make MGJN
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeMGJN( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       aMyGraph->graph.type,
                                       aMyGraph->joinablePredicate,
                                       aMyGraph->nonJoinablePredicate,
                                       sLeftSORT,
                                       sRightSORT,
                                       sMGJN )
              != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sMGJN;

    qmg::setPlanInfo( aStatement, sMGJN, &(aMyGraph->graph) );

    sMGJN->flag       &= ~QMN_PLAN_JOIN_METHOD_TYPE_MASK;
    sMGJN->flag       |= QMN_PLAN_JOIN_METHOD_MERGE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::extractFilter( qcStatement   * aStatement,
                        qmgJOIN       * aMyGraph,
                        qmoPredicate  * aPredicate,
                        qtcNode      ** aFilter )
{
    qmoPredicate * sCopiedPred;

    IDU_FIT_POINT_FATAL( "qmgJoin::extractFilter::__FT__" );

    if( aPredicate != NULL )
    {
        if( aMyGraph->graph.myQuerySet->materializeType ==
            QMO_MATERIALIZE_TYPE_VALUE )
        {
            // To fix BUG-24919
            // normalform���� �ٲ� predicate��
            // ���� ������ ��带 ������ �� �����Ƿ�
            // �߰��� materialized node �� �ִ� ���
            // push projection�� ������ �ȵ� �� �ִ�.
            // ���� predicate�� �����ؼ� ����Ѵ�.
            IDE_TEST( qmoPred::deepCopyPredicate(
                          QC_QMP_MEM(aStatement),
                          aPredicate,
                          &sCopiedPred )
                      != IDE_SUCCESS );
        
            IDE_TEST( qmoPred::linkFilterPredicate(
                          aStatement ,
                          sCopiedPred,
                          aFilter ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( qmoPred::linkFilterPredicate(
                          aStatement ,
                          aPredicate,
                          aFilter ) != IDE_SUCCESS );
        }
        
        IDE_TEST( qmoPred::setPriorNodeID(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      *aFilter ) != IDE_SUCCESS );
    }
    else
    {
        *aFilter = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::initFILT( qcStatement   * aStatement,
                   qmgJOIN       * aMyGraph,
                   qmnPlan      ** aFILT )
{
    IDU_FIT_POINT_FATAL( "qmgJoin::initFILT::__FT__" );

    if( aMyGraph->graph.constantPredicate != NULL )
    {
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement,
                      aMyGraph->graph.myQuerySet,
                      NULL,
                      aMyGraph->graph.myPlan,
                      aFILT )
                  != IDE_SUCCESS );
    }
    else
    {
        *aFILT = aMyGraph->graph.myPlan;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeFILT( qcStatement  * aStatement,
                   qmgJOIN      * aMyGraph,
                   qmnPlan      * aFILT )
{
    IDU_FIT_POINT_FATAL( "qmgJoin::makeFILT::__FT__" );

    if( aMyGraph->graph.constantPredicate != NULL )
    {
        //make FILT
        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      NULL,
                      aMyGraph->graph.constantPredicate ,
                      aMyGraph->graph.myPlan ,
                      aFILT) != IDE_SUCCESS);

        aMyGraph->graph.myPlan = aFILT;

        qmg::setPlanInfo( aStatement, aFILT, &(aMyGraph->graph) );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::printGraph( qcStatement  * aStatement,
                     qmgGraph     * aGraph,
                     ULong          aDepth,
                     iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *    Graph�� �����ϴ� ���� ������ ����Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt  i;

    qmgJOIN * sMyGraph;

    qmoPredicate * sPredicate;
    qmoPredicate * sMorePred;

    IDU_FIT_POINT_FATAL( "qmgJoin::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

    sMyGraph = (qmgJOIN*) aGraph;

    //-----------------------------------
    // Graph ���� ������ ���
    //-----------------------------------

    IDE_TEST( qmg::printGraph( aStatement,
                               aGraph,
                               aDepth,
                               aString )
              != IDE_SUCCESS );

    //-----------------------------------
    // Graph ���� ������ ���
    //-----------------------------------

    //-----------------------------------
    // Join Method ������ ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "== Join Method Information ==" );

    if ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
         == QMG_JOIN_ONLY_NL_FALSE )
    {
        IDE_TEST(
            qmoJoinMethodMgr::printJoinMethod( &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_NESTED],
                                               aDepth,
                                               aString )
            != IDE_SUCCESS );

        IDE_TEST(
            qmoJoinMethodMgr::printJoinMethod( &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_SORT],
                                               aDepth,
                                               aString )
            != IDE_SUCCESS );

        IDE_TEST(
            qmoJoinMethodMgr::printJoinMethod( &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_HASH],
                                               aDepth,
                                               aString )
            != IDE_SUCCESS );

        IDE_TEST(
            qmoJoinMethodMgr::printJoinMethod( &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_MERGE],
                                               aDepth,
                                               aString )
            != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST(
            qmoJoinMethodMgr::printJoinMethod( &sMyGraph->joinMethods[QMG_INNER_JOIN_METHOD_NESTED],
                                               aDepth,
                                               aString )
            != IDE_SUCCESS );
    }

    //-----------------------------------
    // Subquery Graph ������ ���
    //-----------------------------------

    for ( sPredicate = sMyGraph->joinablePredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        for ( sMorePred = sPredicate;
              sMorePred != NULL;
              sMorePred = sMorePred->more )
        {
            IDE_TEST( qmg::printSubquery( aStatement,
                                          sMorePred->node,
                                          aDepth,
                                          aString )
                      != IDE_SUCCESS );
        }
    }

    for ( sPredicate = sMyGraph->joinablePredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        for ( sMorePred = sPredicate;
              sMorePred != NULL;
              sMorePred = sMorePred->more )
        {
            IDE_TEST( qmg::printSubquery( aStatement,
                                          sMorePred->node,
                                          aDepth,
                                          aString )
                      != IDE_SUCCESS );
        }
    }

    for ( sPredicate = sMyGraph->graph.myPredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        for ( sMorePred = sPredicate;
              sMorePred != NULL;
              sMorePred = sMorePred->more )
        {
            IDE_TEST( qmg::printSubquery( aStatement,
                                          sMorePred->node,
                                          aDepth,
                                          aString )
                      != IDE_SUCCESS );
        }
    }

    //-----------------------------------
    // Child Graph ���� ������ ���
    //-----------------------------------

    IDE_TEST( aGraph->left->printGraph( aStatement,
                                        aGraph->left,
                                        aDepth + 1,
                                        aString )
              != IDE_SUCCESS );

    IDE_TEST( aGraph->right->printGraph( aStatement,
                                         aGraph->right,
                                         aDepth + 1,
                                         aString )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::selectJoinMethodCost( qcStatement        * aStatement,
                               qmgGraph           * aGraph,
                               qmoPredicate       * aJoinPredicate,
                               qmoJoinMethod      * aJoinMethods,
                               qmoJoinMethodCost ** aSelected )
{
/***********************************************************************
 *
 * Description : ���� cost�� ���� Join Method�� ����
 *
 * Implementation :
 *    (1) �� Join Method���� cost ���
 *    (2) ���� cost�� ���� Join Method ����
 *        - Hint�� ������ ���
 *          A. table order���� �����ϴ� Join Method �߿��� ���� cost�� ���� ��
 *             ����
 *          B. table order���� �����ϴ� Join Method�� ���� ���
 *             Join Method Hint���� �����ϴ� Join Method �߿��� ���� cost��
 *             ���� �� ����
 *        - Hint�� �������� ���� ���
 *          ���� cost�� ���� Join Method ����
 *
 ***********************************************************************/

    qmsJoinMethodHints * sJoinMethodHints;
    UInt                 sJoinMethodCnt;
    UInt                 i;

    // To Fix PR-7989
    qmoJoinMethodCost *  sSelected;

    IDU_FIT_POINT_FATAL( "qmgJoin::selectJoinMethodCost::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethods != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sSelected = NULL;
    sJoinMethodHints = aGraph->myQuerySet->SFWGH->hints->joinMethod;

    if ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
         == QMG_JOIN_ONLY_NL_FALSE )
    {
        switch( aGraph->type )
        {
            case QMG_INNER_JOIN:
            case QMG_SEMI_JOIN:
            case QMG_ANTI_JOIN:
                sJoinMethodCnt = 4;
                break;
            case QMG_LEFT_OUTER_JOIN:
            case QMG_FULL_OUTER_JOIN:
                sJoinMethodCnt = 3;
                break;
            default:
                IDE_FT_ASSERT( 0 );
        }
    }
    else
    {
        sJoinMethodCnt = 1;
    }

    //------------------------------------------
    // �� Join Method���� cost ��� �� ���� cost�� ���� Join Method Cost ����
    //------------------------------------------

    for ( i = 0; i < sJoinMethodCnt; i++ )
    {
        IDE_TEST( qmoJoinMethodMgr::getBestJoinMethodCost( aStatement,
                                                           aGraph,
                                                           aJoinPredicate,
                                                           & aJoinMethods[i] )
                  != IDE_SUCCESS );
    }

    if ( sJoinMethodHints != NULL )
    {
        //------------------------------------------
        // Join Method Hint�� table order���� �����ϴ� Join Method�� �߿���
        // ���� cost�� ���� ���� ����
        //------------------------------------------

        for ( i = 0; i < sJoinMethodCnt; i++ )
        {
            if ( aJoinMethods[i].hintJoinMethod != NULL )
            {
                if ( sSelected == NULL )
                {
                    sSelected = aJoinMethods[i].hintJoinMethod;
                }
                else
                {
                    if ( sSelected->totalCost >
                         aJoinMethods[i].hintJoinMethod->totalCost )
                    {
                        sSelected = aJoinMethods[i].hintJoinMethod;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            }
            else
            {
                // nothing to do
            }
        }

        //------------------------------------------
        // Join Method Hint�� table order���� �����ϴ� Join Method�� ���� ���,
        // Join Method ���� �����ϴ� Join Method�� �߿��� ���� cost�� ���� ��
        // ����
        //------------------------------------------

        if ( sSelected == NULL )
        {
            // To Fix BUG-10410
            for ( i = 0; i < sJoinMethodCnt; i++ )
            {
                if ( aJoinMethods[i].hintJoinMethod2 != NULL )
                {
                    if ( sSelected == NULL )
                    {
                        sSelected = aJoinMethods[i].hintJoinMethod2;
                    }
                    else
                    {
                        if ( sSelected->totalCost >
                             aJoinMethods[i].hintJoinMethod2->totalCost )
                        {
                            sSelected = aJoinMethods[i].hintJoinMethod2;
                        }
                        else
                        {
                            // nothing to do
                        }
                    }
                }
                else
                {
                    // nothing to do
                }
            }
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // nothing to do
    }

    if ( sSelected == NULL )
    {
        //------------------------------------------
        // Hint�� �������� �ʰų�,
        // Hint�� �����ϰ� feasibility�� �ִ� Join Method�� ���� ���
        //------------------------------------------

        for ( i = 0; i < sJoinMethodCnt; i++ )
        {
            if ( sSelected == NULL )
            {
                sSelected = aJoinMethods[i].selectedJoinMethod;
            }
            else
            {
                if ( aJoinMethods[i].selectedJoinMethod != NULL )
                {
                    if ( sSelected->totalCost >
                         aJoinMethods[i].selectedJoinMethod->totalCost )
                    {
                        sSelected = aJoinMethods[i].selectedJoinMethod;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    // nothing to do
                }
            }
        }
    }
    else
    {
        // nothing to do
    }

    /* TASK-6744 */
    if ( (QCU_PLAN_RANDOM_SEED != 0) &&
         (aStatement->mRandomPlanInfo != NULL) &&
         (smiGetStartupPhase() == SMI_STARTUP_SERVICE) )
    {
        IDE_TEST( randomSelectJoinMethod( aStatement,
                                          sJoinMethodCnt,
                                          aJoinMethods,
                                          &sSelected )
                  !=IDE_SUCCESS );

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST_RAISE( sSelected == NULL, ERR_NOT_FOUND );

    *aSelected = sSelected;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_FOUND )
    {
        IDE_SET(ideSetErrorCode( idERR_ABORT_InternalServerError ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::afterJoinMethodDecision( qcStatement       * aStatement,
                                  qmgGraph          * aGraph,
                                  qmoJoinMethodCost * aSelectedMethod,
                                  qmoPredicate     ** aPredicate,
                                  qmoPredicate     ** aJoinablePredicate,
                                  qmoPredicate     ** aNonJoinablePredicate )
{
/***********************************************************************
 *
 * Description : Join Method ���� �� ó��
 *
 * Implementation :
 *    (1) direction�� ���� left, right �缳��
 *    (2) Join Method�� ���� Predicate�� ó��
 *    (3) index argument�� ����
 *
 ***********************************************************************/

    qmgGraph      * sTmpGraph;
    qmgGraph      * sAntiRight;
    qmgGraph      * sAntiLeft;
    qmoPredicate  * sJoinablePredicate;
    qmoPredicate  * sNonJoinablePredicate;
    qmoPredicate  * sMorePredicate;
    qmoPredicate  * sPredicate;
    qtcNode       * sNode;
    qmgGraph      * sLeftNodeGraph  = NULL;
    qmgGraph      * sRightNodeGraph = NULL;
    UInt            sBucketCnt;
    UInt            sTmpTblCnt;

    // To Fix PR-8032
    // �Լ��� ���ڸ� Input�� Output ��θ� ���� �뵵��
    // ����ϸ� �ȵ�.
    qmoPredicate  * sPred = NULL;
    qmoPredicate  * sFirstJoinPred = NULL;
    qmoPredicate  * sCurJoinPred = NULL;
    qmoPredicate  * sJoinPred = NULL;
    qmoPredicate  * sNonJoinPred = NULL;

    IDU_FIT_POINT_FATAL( "qmgJoin::afterJoinMethodDecision::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aSelectedMethod != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sPredicate = * aPredicate;

    //------------------------------------------
    // Direction�� ���� left, right graph ��ġ ����
    //------------------------------------------

    if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
         == QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT )
    {
        sTmpGraph = aGraph->left;
        aGraph->left = aGraph->right;
        aGraph->right = sTmpGraph;
    }
    else
    {
        // nothing to do
    }

    //------------------------------------------
    // Join Method�� ���� Predicate�� ó��
    //------------------------------------------

    switch ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK )
    {
        case QMO_JOIN_METHOD_FULL_NL :
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sPredicate,
                                                          ID_TRUE )
                      != IDE_SUCCESS );

            // ������ predicate�� �ݿ��Ѵ�.
            *aPredicate = sPredicate;

            if ( ( ( aGraph->type == QMG_INNER_JOIN ) ||
                   ( aGraph->type == QMG_SEMI_JOIN ) ||
                   ( aGraph->type == QMG_ANTI_JOIN ) ) &&
                 ( ( aGraph->right->type == QMG_SELECTION ) ||
                   ( aGraph->right->type == QMG_PARTITION ) ) )
            {
                // �Ϲ� Join�̸鼭 ���� graph�� base table�� ���� Graph�� ���,
                // predicate�� ������ ����
                if ( sPredicate != NULL )
                {
                    // BUG-43929 FULL NL join�� pushedDownPredicate�� deep copy �ؾ� �մϴ�.
                    // deep copy ���� ������ setNonJoinPushDownPredicate���� �Ѽ��� �˴ϴ�.
                    IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM(aStatement),
                                                          sPredicate,
                                                          &((qmgJOIN*)aGraph)->pushedDownPredicate )
                              != IDE_SUCCESS );

                    IDE_TEST( qmoPred::makeNonJoinPushDownPredicate(
                                  aStatement,
                                  sPredicate,
                                  &aGraph->right->depInfo,
                                  aSelectedMethod->flag )
                              != IDE_SUCCESS );

                    IDE_TEST( setNonJoinPushDownPredicate(
                                  aStatement,
                                  aGraph->right,
                                  &sPredicate )
                              != IDE_SUCCESS );

                    // fix BUG-12766
                    // aGraph->graph.myPredicate�� right�� �������Ƿ�
                    // aGraph->graph.myPredicate = NULL
                    *aPredicate = sPredicate;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // Nothing To Do
            }
            break;
        case QMO_JOIN_METHOD_INDEX :
            // indexable / non indexable predicate�� �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );
            
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );

            if ( ( aGraph->type == QMG_INNER_JOIN ) ||
                 ( aGraph->type == QMG_SEMI_JOIN ) ||
                 ( aGraph->type == QMG_ANTI_JOIN ) )
            {
                // joinable predicate�� right�� ����
                if ( sJoinPred != NULL )
                {
                    ((qmgJOIN*)aGraph)->pushedDownPredicate = sJoinPred;

                    // PROJ-1502 PARTITIONED DISK TABLE
                    // push down joinable predicate�� �����.
                    IDE_TEST( qmoPred::makeJoinPushDownPredicate(
                                  aStatement,
                                  sJoinPred,
                                  & aGraph->right->depInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( setJoinPushDownPredicate(
                                  aStatement,
                                  aGraph->right,
                                  &sJoinPred )
                              != IDE_SUCCESS );

                    IDE_TEST( alterSelectedIndex(
                                  aStatement,
                                  aGraph->right,
                                  aSelectedMethod->rightIdxInfo->index )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do
                }

                // non joinable predicate�� right�� ����
                if ( sNonJoinPred != NULL )
                {
                    IDE_TEST( qmoPred::makeNonJoinPushDownPredicate(
                                  aStatement,
                                  sNonJoinPred,
                                  & aGraph->right->depInfo,
                                  aSelectedMethod->flag )
                              != IDE_SUCCESS );

                    IDE_TEST( setNonJoinPushDownPredicate(
                                  aStatement,
                                  aGraph->right,
                                  &sNonJoinPred )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do
                }
            }
            else if ( aGraph->type == QMG_LEFT_OUTER_JOIN )
            {
                // joinable predicate�� right�� ����
                if ( sJoinPred != NULL )
                {
                    ((qmgLOJN*)aGraph)->pushedDownPredicate = sJoinPred;

                    // PROJ-1502 PARTITIONED DISK TABLE
                    // push down joinable predicate�� �����.
                    IDE_TEST( qmoPred::makeJoinPushDownPredicate(
                                  aStatement,
                                  sJoinPred,
                                  & aGraph->right->depInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( setJoinPushDownPredicate(
                                  aStatement,
                                  aGraph->right,
                                  &sJoinPred )
                              != IDE_SUCCESS );

                    IDE_TEST( alterSelectedIndex(
                                  aStatement,
                                  aGraph->right,
                                  aSelectedMethod->rightIdxInfo->index )
                              != IDE_SUCCESS );
                        
                    *aPredicate = sNonJoinPred;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }

            break;
        case QMO_JOIN_METHOD_INVERSE_INDEX :
            // indexable / non indexable predicate�� �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );
            
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );

            // Inverse Index NL�� Semi Join������ �����Ѵ�.
            IDE_DASSERT( aGraph->type == QMG_SEMI_JOIN );

            if ( sJoinPred != NULL )
            {
                ((qmgJOIN*)aGraph)->pushedDownPredicate = sJoinPred;

                // PROJ-1502 PARTITIONED DISK TABLE
                // push down joinable predicate�� �����.
                IDE_TEST( qmoPred::makeJoinPushDownPredicate(
                              aStatement,
                              sJoinPred,
                              & aGraph->right->depInfo )
                          != IDE_SUCCESS );

                IDE_TEST( setJoinPushDownPredicate(
                              aStatement,
                              aGraph->right,
                              &sJoinPred )
                          != IDE_SUCCESS );

                IDE_TEST( alterSelectedIndex(
                              aStatement,
                              aGraph->right,
                              aSelectedMethod->rightIdxInfo->index )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            // PROJ-2385 
            // non-joinable Predicate�� ���� ������ �� Right�� Pushdown�Ѵ�.
            // Join Node Filter �뵵�� �� �� �� �ʿ��ϱ� �����̴�.
            IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM(aStatement),
                                                  sNonJoinPred,
                                                  &sNonJoinablePredicate )
                      != IDE_SUCCESS );

            if ( sNonJoinablePredicate != NULL )
            {
                IDE_TEST( qmoPred::makeNonJoinPushDownPredicate(
                              aStatement,
                              sNonJoinablePredicate,
                              & aGraph->right->depInfo,
                              aSelectedMethod->flag )
                          != IDE_SUCCESS );

                IDE_TEST( setNonJoinPushDownPredicate(
                              aStatement,
                              aGraph->right,
                              &sNonJoinablePredicate )
                          != IDE_SUCCESS );
            }
            else
            {
                // nothing to do
            }
            break;
        case QMO_JOIN_METHOD_FULL_STORE_NL :
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sPredicate,
                                                          ID_TRUE )
                      != IDE_SUCCESS );

            // ������ predicate�� �ݿ��Ѵ�.
            *aPredicate = sPredicate;
            
            break;
        case QMO_JOIN_METHOD_ANTI :
            // indexable / non indexable predicate�� �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );
            
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );
            
            // left�� �����Ͽ� antiRightGraph�� ����
            // PROJ-1446 Host variable�� ������ ���� ����ȭ
            // left�� sAntiRight�� �����ϴ� ���ÿ�
            // sAntiRight�� selectedIndex�� �ٲ۴�.
            // �� ó���� qmgSelection�� ���ؼ� �ϴ� ������
            // selectedIndex�� �ٲ� �� scan decision factor��
            // ó���ؾ� �ϱ� �����̴�.
            IDE_TEST( copyGraphAndAlterSelectedIndex(
                          aStatement,
                          aGraph->left,
                          &sAntiRight,
                          aSelectedMethod->leftIdxInfo->index,
                          0 ) // sAntiRight�� selectedIndex�� �ٲ۴�.
                      != IDE_SUCCESS );

            // To Fix BUG-10191
            // left�� myPredicate�� �����ؼ� right�� myPredicate�� ����
            if ( aGraph->left->myPredicate != NULL )
            {
                IDE_TEST(
                    qmoPred::copyOnePredicate( QC_QMP_MEM(aStatement),
                                               aGraph->left->myPredicate,
                                               &sAntiRight->myPredicate )
                    != IDE_SUCCESS );
            }
            else
            {
                // nothing to do
            }

            // joinable Predicate�� �����ؼ� AntiRight�� ����
            sPred = sJoinPred;
            while ( sPred != NULL )
            {
                IDE_TEST( qmoPred::copyOnePredicate( QC_QMP_MEM(aStatement),
                                                     sPred,
                                                     &sJoinablePredicate )
                          != IDE_SUCCESS );

                sJoinablePredicate->next = NULL;

                if ( sFirstJoinPred == NULL )
                {
                    sFirstJoinPred = sCurJoinPred = sJoinablePredicate;
                }
                else
                {
                    sCurJoinPred->next = sJoinablePredicate;
                    sCurJoinPred = sCurJoinPred->next;
                }

                sPred = sPred->next;
            }

            IDE_DASSERT( sFirstJoinPred != NULL );

            IDE_TEST( qmoPred::makeJoinPushDownPredicate(
                          aStatement,
                          sFirstJoinPred,
                          & sAntiRight->depInfo )
                      != IDE_SUCCESS );

            IDE_TEST( setJoinPushDownPredicate(
                          aStatement,
                          sAntiRight,
                          &sFirstJoinPred )
                      != IDE_SUCCESS );

            // To Fix BUG-8384
            ((qmgFOJN*)aGraph)->antiRightGraph = sAntiRight;

            // right�� �����Ͽ� antiLeftGraph�� ����
            // PROJ-1446 Host variable�� ������ ���� ����ȭ
            // left�� sAntiRight�� �����ϴ� ���ÿ�
            // sAntiRight�� selectedIndex�� �ٲ۴�.
            // �� ó���� qmgSelection�� ���ؼ� �ϴ� ������
            // selectedIndex�� �ٲ� �� scan decision factor��
            // ó���ؾ� �ϱ� �����̴�.
            IDE_TEST( copyGraphAndAlterSelectedIndex(
                          aStatement,
                          aGraph->right,
                          &sAntiLeft,
                          aSelectedMethod->rightIdxInfo->index,
                          1 ) // right�� selectedIndex�� �ٲ۴�.
                      != IDE_SUCCESS );

            // To Fix BUG-8384
            ((qmgFOJN*)aGraph)->antiLeftGraph = sAntiLeft;

            // joinable Predicate�� Right�� ����
            IDE_TEST( qmoPred::makeJoinPushDownPredicate(
                          aStatement,
                          sJoinPred,
                          & aGraph->right->depInfo )
                      != IDE_SUCCESS );

            IDE_TEST( setJoinPushDownPredicate(
                          aStatement,
                          aGraph->right,
                          &sJoinPred )
                      != IDE_SUCCESS );
            
            break;
        case QMO_JOIN_METHOD_ONE_PASS_SORT : // To Fix PR-8032
        case QMO_JOIN_METHOD_TWO_PASS_SORT : // One-Pass �Ǵ� Two Pass�� ������
        case QMO_JOIN_METHOD_INVERSE_SORT  : // PROJ-2385
            // sort joinable predicate/ non sort joinable predicate �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );
            
            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_MERGE :
            // sort joinable predicate/ non sort joinable predicate �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );

            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_ONE_PASS_HASH : // To Fix PR-8032
        case QMO_JOIN_METHOD_TWO_PASS_HASH : // One-Pass �Ǵ� Two Pass�� ������
        case QMO_JOIN_METHOD_INVERSE_HASH :  // PROJ-2339

            // hash joinable predicate/ non hash joinable predicate �з�
            IDE_TEST(
                qmoPred::separateJoinPred( sPredicate,
                                           aSelectedMethod->joinPredicate,
                                           & sJoinPred,
                                           & sNonJoinPred )
                != IDE_SUCCESS );

            // PROJ-1404
            // ������ transitive predicate�� non-join predicate����
            // ���õ� ���� bad transitive predicate�̹Ƿ� �����Ѵ�.
            IDE_TEST( qmoPred::removeTransitivePredicate( & sNonJoinPred,
                                                          ID_TRUE )
                      != IDE_SUCCESS );            
            break;
        default :
            IDE_DASSERT( 0 );
            break;
    }

    //------------------------------------------
    // Index Argument ����
    //------------------------------------------

    if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) 
         == QMO_JOIN_METHOD_INDEX )
    {
        // indexable join predicate�� �����ڿ� ���� ������
    }
    else
    {
        for ( sJoinablePredicate = sJoinPred;
              sJoinablePredicate != NULL;
              sJoinablePredicate = sJoinablePredicate->next )
        {
            sNode = (qtcNode*)sJoinablePredicate->node;

            if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_OR )
            {
                sNode = (qtcNode *)sNode->node.arguments;
            }
            else
            {
                // nothing to do
            }

            // �� ������ �� ���� ��忡 �ش��ϴ� graph�� ã��
            IDE_TEST( qmoPred::findChildGraph( sNode,
                                               & aGraph->depInfo,
                                               aGraph->left,
                                               aGraph->right,
                                               & sLeftNodeGraph,
                                               & sRightNodeGraph )
                      != IDE_SUCCESS );

            if ( aGraph->right == sLeftNodeGraph )
            {
                sNode->indexArgument = 0;
            }
            else // aGraph->right == sRightNodeGraph
            {
                sNode->indexArgument = 1;
            }

            for ( sMorePredicate = sJoinablePredicate->more;
                  sMorePredicate != NULL;
                  sMorePredicate = sMorePredicate->more )
            {
                sNode = (qtcNode*)sMorePredicate->node;

                if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_OR )
                {
                    sNode = (qtcNode *)sNode->node.arguments;
                }
                else
                {
                    // nothing to do
                }

                // �� ������ �� ���� ��忡 �ش��ϴ� graph�� ã��
                IDE_TEST( qmoPred::findChildGraph( sNode,
                                                   & aGraph->depInfo,
                                                   aGraph->left,
                                                   aGraph->right,
                                                   & sLeftNodeGraph,
                                                   & sRightNodeGraph )
                          != IDE_SUCCESS );

                if ( aGraph->right == sLeftNodeGraph )
                {
                    sNode->indexArgument = 0;
                }
                else // aGraph->right == sRightNodeGraph
                {
                    sNode->indexArgument = 1;
                }
            }
        }
    }

    checkOrValueIndex( aGraph->left );
    checkOrValueIndex( aGraph->right );

    //------------------------------------------
    // hash bucket count�� hash temp table ������ ���Ѵ�.
    //------------------------------------------

    // To-Fix 8062
    // Index Argument �����Ŀ� ����Ͽ��� ��.
    // hash bucket count,  hash temp table ���� ���

    // PROJ-2385
    // Inverse Index NL
    if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) == QMO_JOIN_METHOD_INVERSE_INDEX )
    {
        IDE_TEST( getBucketCntNTmpTblCnt( aStatement,
                                          aSelectedMethod,
                                          aGraph,
                                          ID_TRUE, // aIsLeftGraph
                                          & sBucketCnt,
                                          & sTmpTblCnt )
                  != IDE_SUCCESS );

        // Semi Join������ Inverse Index NL ����
        IDE_DASSERT( aGraph->type == QMG_SEMI_JOIN );

        ((qmgJOIN *)aGraph)->hashBucketCnt = sBucketCnt;
        ((qmgJOIN *)aGraph)->hashTmpTblCnt = sTmpTblCnt;
    }
    else
    {
        if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_HASH ) 
             != QMO_JOIN_METHOD_NONE )
        {
            IDE_TEST( getBucketCntNTmpTblCnt( aStatement,
                                              aSelectedMethod,
                                              aGraph,
                                              ID_FALSE, // aIsLeftGraph
                                              & sBucketCnt,
                                              & sTmpTblCnt )
                      != IDE_SUCCESS );
    
            switch ( aGraph->type )
            {
                case QMG_INNER_JOIN :
                case QMG_SEMI_JOIN :
                case QMG_ANTI_JOIN :
                    ((qmgJOIN *)aGraph)->hashBucketCnt = sBucketCnt;
                    ((qmgJOIN *)aGraph)->hashTmpTblCnt = sTmpTblCnt;
                    break;
                case QMG_FULL_OUTER_JOIN :
                    ((qmgFOJN *)aGraph)->hashBucketCnt = sBucketCnt;
                    ((qmgFOJN *)aGraph)->hashTmpTblCnt = sTmpTblCnt;
                    break;
                case QMG_LEFT_OUTER_JOIN :
                    ((qmgLOJN *)aGraph)->hashBucketCnt = sBucketCnt;
                    ((qmgLOJN *)aGraph)->hashTmpTblCnt = sTmpTblCnt;
                    break;
                default :
                    IDE_DASSERT( 0 );
                    break;
            }
        }
        else
        {
            // Inverse Index NL�̳� Hash Join�� �ƴ� ���� Bucket Count ������ �ʿ����.
            // Nothing To Do
        }
    }

    *aJoinablePredicate = sJoinPred;
    *aNonJoinablePredicate = sNonJoinPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makePreservedOrder( qcStatement        * aStatement,
                             qmgGraph           * aGraph,
                             qmoJoinMethodCost  * aSelectedMethod,
                             qmoPredicate       * aJoinablePredicate )
{
/***********************************************************************
 *
 * Description : Preserved Order�� ����
 *
 * Implementation :
 *    To Fix PR-11556
 *       Join Predicate�� ���� Preserved Order�� ������� �ʴ´�.
 *       ȿ�뼺�� �������� �ݸ�, ���⵵�� �����ϰ� �ȴ�.
 *       ���� ������ L1.i1 �� R1.a1�� ������ �� �̰� ����ұ�?
 *           - SELECT DISTINCT L1.i1, R1.a1 FROM L1, R1 WHERE L1.i1 = R1.a1;
 *           - SELECT L1.i1, R1.a1 FROM L1, R1 WHERE L1.i1 = R1.a1
 *             GROUP BY L1.i1, R1.a1;
 *           - SELECT * FROM L1, R1 WHERE L1.i1 = R1.a1
 *             GROUP BY L1.i1, R1.a1;
 *       BUGBUG
 *       sort-based join�� �ε�ȣ�� �����ϸ� �̶����� preserved order�� ����ϸ�
 *       ���� ������ ���� �ִ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::makePreservedOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aSelectedMethod != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    switch( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK )
    {
        case QMO_JOIN_METHOD_FULL_NL :
        case QMO_JOIN_METHOD_FULL_STORE_NL :

            // Left Child Graph�� Preserved Order�� �̿��Ͽ�
            // Join Graph�� preserved order�� �����Ѵ�.
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph,
                                          ID_FALSE ) // aIsRightGraph 
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_INDEX :

            // Left Child Graph�� Preserved Order�� �̿��Ͽ�
            // Join Graph�� preserved order�� �����Ѵ�.
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph,
                                          ID_FALSE ) // aIsRightGraph 
                      != IDE_SUCCESS );

            //------------------------------------------
            // right�� index�� ���� preserved order ����
            //    < right�� preserved order�� �����ϴ� ��� >
            //    - right�� preserved order�� ���õ� join method��
            //      right index�� ���� ���, right preserved order �״�� ���
            //    - right�� preserved order�� ���õ� join method��
            //      right index�� �ٸ� ���, right preserved order ���� ����
            //    < right�� preserved order�� �������� �ʴ� ��� >
            //      right�� index�� ���� preserved order �����Ͽ�
            //      right�� �����ش�.
            //------------------------------------------

            IDE_TEST( makeNewOrder( aStatement,
                                    aGraph->right,
                                    aSelectedMethod->rightIdxInfo->index )
                      != IDE_SUCCESS );

            break;
        case QMO_JOIN_METHOD_INVERSE_INDEX :
            
            // Left Child Graph�� Preserved Order�� ������ �� ����.
            // ���, right�� index�� ���� �� Order�� ������ �Ѵ�.
            aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
            aGraph->flag |= QMG_PRESERVED_ORDER_NEVER;

            IDE_TEST( makeNewOrder( aStatement,
                                    aGraph->right,
                                    aSelectedMethod->rightIdxInfo->index )
                      != IDE_SUCCESS );

            break;
        case QMO_JOIN_METHOD_ANTI :

            // Left Child Graph�� Preserved Order�� �̿��Ͽ�
            // Join Graph�� preserved order�� �����Ѵ�.
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph,
                                          ID_FALSE ) // aIsRightGraph 
                      != IDE_SUCCESS );

            IDE_TEST( makeNewOrder( aStatement,
                                    aGraph->right,
                                    aSelectedMethod->rightIdxInfo->index )
                      != IDE_SUCCESS );

            IDE_TEST( makeNewOrder( aStatement,
                                    ((qmgFOJN*)aGraph)->antiRightGraph,
                                    aSelectedMethod->leftIdxInfo->index )
                      != IDE_SUCCESS );

            break;
        case QMO_JOIN_METHOD_ONE_PASS_HASH :
        case QMO_JOIN_METHOD_ONE_PASS_SORT :

            // Left Child Graph�� Preserved Order�� �̿��Ͽ�
            // Join Graph�� preserved order�� �����Ѵ�.
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph,
                                          ID_FALSE ) // aIsRightGraph 
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_TWO_PASS_HASH :
        case QMO_JOIN_METHOD_INVERSE_HASH  : // PROJ-2339 

            // Two Pass, Inverse Hash Join�� preserverd order�� �������� �ʴ´�.
            aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
            aGraph->flag |= QMG_PRESERVED_ORDER_NEVER;
            break;

        case QMO_JOIN_METHOD_TWO_PASS_SORT :
        case QMO_JOIN_METHOD_INVERSE_SORT :
        case QMO_JOIN_METHOD_MERGE :

            // Join Predicate�� �̿��Ͽ� Preserved Order�� �����Ѵ�.
            // Merge Join�� ��� ASCENDING���� �����ϴ�.
            IDE_TEST( makeOrderFromJoin( aStatement,
                                         aGraph,
                                         aSelectedMethod,
                                         QMG_DIRECTION_ASC,
                                         aJoinablePredicate )
                      != IDE_SUCCESS );
            break;
        default :
            // nothing to do
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmgJoin::getBucketCntNTmpTblCnt( qcStatement       * aStatement,
                                        qmoJoinMethodCost * aSelectedMethod,
                                        qmgGraph          * aGraph,
                                        idBool              aIsLeftGraph,
                                        UInt              * aBucketCnt,
                                        UInt              * aTmpTblCnt )
{
/***********************************************************************
 *
 * Description : hash bucket count�� hash temp table count�� ���ϴ� �Լ�
 *
 * Implementation :
 *    (1) hash bucket count�� ����
 *    (2) hash temp table count�� ����
 *
 ***********************************************************************/

    UInt             sBucketCnt;
    SDouble          sBucketCntTemp;
    SDouble          sMaxBucketCnt;
    SDouble          sTmpTblCnt;

    IDU_FIT_POINT_FATAL( "qmgJoin::getBucketCntNTmpTblCnt::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aSelectedMethod != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // hash bucket count ����
    //------------------------------------------

    if ( aGraph->myQuerySet->SFWGH->hints->hashBucketCnt !=
         QMS_NOT_DEFINED_BUCKET_CNT )
    {
        //------------------------------------------
        // hash bucket count hint�� �����ϴ� ���
        //------------------------------------------

        sBucketCnt = aGraph->myQuerySet->SFWGH->hints->hashBucketCnt;
    }
    else
    {
        //------------------------------------------
        // hash bucket count hint�� �������� �ʴ� ���
        //------------------------------------------

        //------------------------------------------
        // �⺻ bucket count ����
        // bucket count = ���� graph�� ouput record count / 2
        //------------------------------------------

        // BUG-37778 disk hash temp table size estimate
        // Hash Join �� bucket ������ ditinct �ϰ� �������� �����Ƿ�
        // �÷��� ndv �� �������� �ʾƵ� �ȴ�.

        // PROJ-2385
        // BUG-40561
        if ( aIsLeftGraph == ID_TRUE )
        {
            sBucketCntTemp = ( aGraph->left->costInfo.outputRecordCnt / 2 );

            if ((aGraph->left->flag & QMG_GRAPH_TYPE_MASK) == QMG_GRAPH_TYPE_MEMORY)
            {
                // bucketCnt�� QMC_MEM_HASH_MAX_BUCKET_CNT(10240000)�� ������
                // QMC_MEM_HASH_MAX_BUCKET_CNT ������ �������ش�.
                /* BUG-48161 */
                sBucketCnt = (UInt)IDL_MIN(sBucketCntTemp, QCG_GET_BUCKET_COUNT_MAX( aStatement ) );
            }
            else
            {
                sBucketCnt = (UInt)IDL_MIN(sBucketCntTemp, ID_UINT_MAX);
            }
        }
        else
        {
            sBucketCntTemp = ( aGraph->right->costInfo.outputRecordCnt / 2 );

            if ((aGraph->right->flag & QMG_GRAPH_TYPE_MASK) == QMG_GRAPH_TYPE_MEMORY)
            {
                // bucketCnt�� QMC_MEM_HASH_MAX_BUCKET_CNT(10240000)�� ������
                // QMC_MEM_HASH_MAX_BUCKET_CNT ������ �������ش�.
                /* BUG-48161 */
                sBucketCnt = (UInt)IDL_MIN(sBucketCntTemp, QCG_GET_BUCKET_COUNT_MAX( aStatement ) );
            }
            else
            {
                sBucketCnt = (UInt)IDL_MIN(sBucketCntTemp, ID_UINT_MAX);
            }
        }

        //  hash bucket count�� ����
        if ( sBucketCnt < QCU_OPTIMIZER_BUCKET_COUNT_MIN )
        {
            sBucketCnt = QCU_OPTIMIZER_BUCKET_COUNT_MIN;
        }
        else
        {
            /* nothing to do */
        }
    }

    *aBucketCnt = sBucketCnt;

    //------------------------------------------
    // hash temp table count ����
    //------------------------------------------

    *aTmpTblCnt = 0;
    if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) ==
         QMO_JOIN_METHOD_TWO_PASS_HASH )
    {
        // two pass hash join method�� ���õ� ���
        if ( aSelectedMethod->hashTmpTblCntHint
             != QMS_NOT_DEFINED_TEMP_TABLE_CNT )
        {
            // hash temp table count hint�� �־��� ���, �̸� ����
            *aTmpTblCnt = aSelectedMethod->hashTmpTblCntHint;
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // nothing to do
    }

    if ( *aTmpTblCnt == 0 )
    {
        // BUG-12581 fix
        // PROJ-2339, 2385
        if ( ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) ==
               QMO_JOIN_METHOD_ONE_PASS_HASH ) ||
             ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) ==
               QMO_JOIN_METHOD_INVERSE_HASH ) ||
             ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK ) ==
               QMO_JOIN_METHOD_INVERSE_INDEX ) )
        {
            *aTmpTblCnt = 1;
        }
        else
        {
            // PROJ-2242
            // hash temp table�� �޸𸮰����� ���� ������ BucketCnt �� �ִ�ġ�� �������
            // �� BucketCnt ���� �������� ����Ѵ�.
            sMaxBucketCnt = smuProperty::getHashAreaSize() *
                (smuProperty::getTempHashGroupRatio() / 100.0 ) / 8.0;

            sTmpTblCnt = *aBucketCnt / sMaxBucketCnt;

            if ( sTmpTblCnt > 1.0 )
            {
                *aTmpTblCnt = (UInt)idlOS::ceil( sTmpTblCnt );
            }
            else
            {
                *aTmpTblCnt = 1;
            }
        }
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeOrderFromChild( qcStatement * aStatement,
                             qmgGraph    * aGraph,
                             idBool        aIsRightGraph )
{
/***********************************************************************
 *
 * Description :
 *    Left �Ǵ� Right Child Graph�� Preserved Order�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPreservedOrder * sGraphOrder = NULL;
    qmgPreservedOrder * sCurrOrder  = NULL;
    qmgPreservedOrder * sNewOrder   = NULL;
    qmgPreservedOrder * sChildOrder = NULL;
    qmgGraph          * sChildGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeOrderFromChild::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sGraphOrder = NULL;

    if ( aIsRightGraph == ID_FALSE )
    {
        // Right Graph�� Order�� ����ϴ� ���
        sChildGraph = aGraph->left;
    }
    else
    {
        // Left Graph�� Order�� ����ϴ� ���
        sChildGraph = aGraph->right;
    }

    //------------------------------------------
    // Preserved Order�� �����Ѵ�.
    //------------------------------------------

    for ( sChildOrder = sChildGraph->preservedOrder;
          sChildOrder != NULL;
          sChildOrder = sChildOrder->next )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                 (void**) & sNewOrder )
                  != IDE_SUCCESS );

        idlOS::memcpy( sNewOrder,
                       sChildOrder,
                       ID_SIZEOF(qmgPreservedOrder ) );

        sNewOrder->next = NULL;

        if ( sGraphOrder == NULL )
        {
            sGraphOrder = sNewOrder;
            sCurrOrder = sNewOrder;
        }
        else
        {
            sCurrOrder->next = sNewOrder;
            sCurrOrder = sCurrOrder->next;
        }
    }

    //------------------------------------------
    // Join Graph�� Preserved Order���� ����
    //------------------------------------------

    aGraph->preservedOrder = sGraphOrder;

    aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
    aGraph->flag |= ( sChildGraph->flag & QMG_PRESERVED_ORDER_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeOrderFromJoin( qcStatement        * aStatement,
                            qmgGraph           * aGraph,
                            qmoJoinMethodCost  * aSelectedMethod,
                            qmgDirectionType     aDirection,
                            qmoPredicate       * aJoinablePredicate )
{
/***********************************************************************
 *
 * Description :
 *    Join Predicate���κ��� Preserved Order ����
 *    �Ʒ��� ���� Left�� ���� ������ ����Ǵ� Join Method�� ���Ͽ�
 *    Preserverd Order�� �����Ѵ�.
 *       - Two Pass Sort Join
 *       - Inverse Sort Join  (PROJ-2385)
 *       - Merge Join
 *
 * Implementation :
 *
 *    Left Graph�� ���� Presererved Order��
 *    Right Graph�� ���� Preserverd Order�� �����Ѵ�.
 *
 *    �ڽſ� ���� Preserverd Order�� ���� �� �ϳ��� �����Ѵ�.
 *       - Left Graph�κ��� ���� �� �ִ� ���(�ִ��� Ȱ��)
 *       - ���� ���, �ڽ��� Graph���� ����
 *
 ***********************************************************************/

    qmgPreservedOrder * sNewOrder               = NULL;
    qmgPreservedOrder * sLeftOrder              = NULL;
    qmgPreservedOrder * sRightOrder             = NULL;
    qtcNode           * sNode                   = NULL;
    qtcNode           * sRightNode              = NULL;
    qtcNode           * sLeftNode               = NULL;

    idBool              sCanUseFromLeftGraph    = ID_FALSE;
    idBool              sCanUseFromRightGraph   = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeOrderFromJoin::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( aSelectedMethod != NULL );
    IDE_FT_ASSERT( aJoinablePredicate != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNode = aJoinablePredicate->node;

    if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_OR )
    {
        sNode = (qtcNode *)sNode->node.arguments;
    }
    else
    {
        // nothing to do
    }

    if ( sNode->indexArgument == 1 )
    {
        sLeftNode  = (qtcNode*)sNode->node.arguments;
        sRightNode = (qtcNode*)sLeftNode->node.next;
    }
    else
    {
        sRightNode = (qtcNode*)sNode->node.arguments;
        sLeftNode  = (qtcNode*)sRightNode->node.next;
    }

    //--------------------------------------------
    // Join Predicate�� ���� Preserved Order ����
    //--------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                             (void**)&sNewOrder )
              != IDE_SUCCESS );
    sNewOrder->table = sLeftNode->node.table;
    sNewOrder->column = sLeftNode->node.column;
    sNewOrder->direction = aDirection;
    sNewOrder->next = NULL;

    sLeftOrder = sNewOrder;

    //--------------------------------------------
    // Left Graph�� ���� Preserved Order ����
    //--------------------------------------------

    // fix BUG-9737
    // merge join�� ��� left child�� right child�� �и�����
    // preserved order����

    if ( ( aSelectedMethod->flag &
           QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_MASK ) ==
         QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_TRUE )
    {
        // To Fix PR-8062
        if ( aGraph->left->preservedOrder == NULL )
        {
            // To Fix PR-8110
            // ���ϴ� Child���� �Է� ���ڷ� ����ؾ� ��.
            IDE_TEST( qmg::setOrder4Child( aStatement,
                                           sNewOrder,
                                           aGraph->left )
                      != IDE_SUCCESS );
            sCanUseFromLeftGraph = ID_TRUE;
        }
        else
        {
            if ( aGraph->left->preservedOrder->direction ==
                 QMG_DIRECTION_NOT_DEFINED )
            {
                // To Fix PR-8110
                // ���ϴ� Child���� �Է� ���ڷ� ����ؾ� ��.
                IDE_TEST( qmg::setOrder4Child( aStatement,
                                               sNewOrder,
                                               aGraph->left )
                          != IDE_SUCCESS );
                sCanUseFromLeftGraph = ID_TRUE;
            }
            else
            {
                sCanUseFromLeftGraph = ID_FALSE;
            }
        }
    }
    else
    {
        sCanUseFromLeftGraph = ID_FALSE;
    }

    //--------------------------------------------
    // Right Graph�� ���� Preserved Order ����
    //--------------------------------------------

    // fix BUG-9737
    // merge join�� ��� left child�� right child�� �и�����
    // preserved order����
    if( QTC_IS_COLUMN( aStatement, sRightNode ) == ID_TRUE )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                 (void**)&sNewOrder )
                  != IDE_SUCCESS );

        sNewOrder->table = sRightNode->node.table;
        sNewOrder->column = sRightNode->node.column;
        sNewOrder->direction = QMG_DIRECTION_ASC;
        sNewOrder->next = NULL;

        sRightOrder = sNewOrder;

        if ( ( aSelectedMethod->flag &
               QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_MASK ) ==
             QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_TRUE )
        {
            // To Fix PR-8062
            if ( aGraph->right->preservedOrder == NULL )
            {
                // To Fix PR-8110
                // ���ϴ� Child���� �Է� ���ڷ� ����ؾ� ��.
                IDE_TEST( qmg::setOrder4Child( aStatement,
                                               sNewOrder,
                                               aGraph->right )
                          != IDE_SUCCESS );
                sCanUseFromRightGraph = ID_TRUE;
            }
            else
            {
                // To Fix PR-8110
                // ���ϴ� Child���� �Է� ���ڷ� ����ؾ� ��.
                if ( aGraph->right->preservedOrder->direction ==
                     QMG_DIRECTION_NOT_DEFINED )
                {
                    IDE_TEST( qmg::setOrder4Child( aStatement,
                                                   sNewOrder,
                                                   aGraph->right )
                              != IDE_SUCCESS );
                    sCanUseFromRightGraph = ID_TRUE;
                }
                else
                {
                    sCanUseFromRightGraph = ID_FALSE;
                }
            }
        }
        else
        {
            sCanUseFromRightGraph = ID_FALSE;
        }
    }
    else
    {
        // nothing to do
    }

    //--------------------------------------------
    // Join Graph�� ���� Preserved Order ����
    //--------------------------------------------

    // PROJ-2385 Inverse Sort
    if ( ( aSelectedMethod->flag & QMO_JOIN_METHOD_MASK )
         == QMO_JOIN_METHOD_INVERSE_SORT )
    {
        if ( sCanUseFromRightGraph == ID_TRUE )
        {
            // Right Graph�� Preserved Order�� �ִ��� Ȱ���� �� �ִ� ���
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph, 
                                          ID_TRUE ) // aIsRightGraph
                      != IDE_SUCCESS );
        }
        else
        {
            // Right Graph���� ������ Preserved Order�� ���� ���
            aGraph->preservedOrder = sRightOrder;
        }
    }
    else
    {
        // Two-Pass Sort, Merge
        if ( sCanUseFromLeftGraph == ID_TRUE )
        {
            // Left Graph�� Preserved Order�� �ִ��� Ȱ���� �� �ִ� ���
            IDE_TEST( makeOrderFromChild( aStatement, 
                                          aGraph,
                                          ID_FALSE ) // aIsRightGraph 
                      != IDE_SUCCESS );
        }
        else
        {
            // Left Graph���� ������ Preserved Order�� ���� ���
            aGraph->preservedOrder = sLeftOrder;
        }
    }

    if ( aGraph->preservedOrder != NULL )
    {
        aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
        aGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
    }
    else
    {
        /* BUG-43697 Inverse Sort�̸�, Right Graph�� Preserved Order�� NULL�� �� �� �ִ�.
         * �� ��쿡�� Preserved Order�� ������� �ʴ´�. 
         */
        aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
        aGraph->flag |= QMG_PRESERVED_ORDER_NEVER;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeNewOrder( qcStatement       * aStatement,
                       qmgGraph          * aGraph,
                       qcmIndex          * aSelectedIndex )
{
/***********************************************************************
 *
 * Description : ���ο� Preserved Order�� ���� �� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::makeNewOrder::__FT__" );

    if( aGraph->type == QMG_SELECTION )
    {
        IDE_TEST( makeNewOrder4Selection( aStatement,
                                          aGraph,
                                          aSelectedIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aGraph->type == QMG_PARTITION )
        {
            IDE_TEST( makeNewOrder4Partition( aStatement,
                                              aGraph,
                                              aSelectedIndex )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2638
            IDE_FT_ASSERT( aGraph->type == QMG_SHARD_SELECT );
        }
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::makeNewOrder4Selection( qcStatement        * aStatement,
                                 qmgGraph           * aGraph,
                                 qcmIndex           * aSelectedIndex )
{
/***********************************************************************
 *
 * Description : ���ο� Preserved Order�� ���� �� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool              sNeedNewChildOrder;
    qmgPreservedOrder * sNewOrder;
    qmgPreservedOrder * sFirstOrder = NULL;
    qmgPreservedOrder * sCurOrder = NULL;
    UInt                sKeyColCnt;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeNewOrder4Selection::__FT__" );

    IDE_DASSERT( (aGraph->type == QMG_SELECTION) ||
                 (aGraph->type == QMG_PARTITION) );

    if ( aGraph->preservedOrder != NULL )
    {
        if ( ( aGraph->preservedOrder->table ==
               aGraph->myFrom->tableRef->table ) &&
             ( (UInt) aGraph->preservedOrder->column ==
               ( aSelectedIndex->keyColumns[0].column.id %
                 SMI_COLUMN_ID_MAXIMUM ) ) )
        {
            //------------------------------------------
            // preserved order�� ���õ� join method�� right index�� ���� ���
            //------------------------------------------

            sNeedNewChildOrder = ID_FALSE;
        }
        else
        {
            sNeedNewChildOrder = ID_TRUE;
        }
    }
    else
    {
        sNeedNewChildOrder = ID_TRUE;
    }

    if ( sNeedNewChildOrder == ID_TRUE )
    {
        //------------------------------------------
        // - preserved order�� ���� ���
        // - preserved order�� �ְ�
        //   ���õ� method�� ����ϴ� index�� �ٸ� ���
        //------------------------------------------

        // To Fix BUG-9631
        sKeyColCnt = aSelectedIndex->keyColCount;

        for ( i = 0; i < sKeyColCnt; i++ )
        {
            IDE_TEST(
                QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                               (void**)&sNewOrder )
                != IDE_SUCCESS );

            if( ( aGraph->flag & QMG_SELT_PARTITION_MASK ) ==
                QMG_SELT_PARTITION_TRUE )
            {
                sNewOrder->table = ((qmgSELT*)aGraph)->partitionRef->table;
            }
            else
            {
                sNewOrder->table = aGraph->myFrom->tableRef->table;
            }

            sNewOrder->column = aSelectedIndex->keyColumns[i].column.id
                % SMI_COLUMN_ID_MAXIMUM;
            sNewOrder->next = NULL;

            if ( i == 0 )
            {
                sNewOrder->direction = QMG_DIRECTION_NOT_DEFINED;
                sFirstOrder = sCurOrder = sNewOrder;
            }
            else
            {
                sNewOrder->direction = QMG_DIRECTION_SAME_WITH_PREV;
                sCurOrder->next = sNewOrder;
                sCurOrder = sCurOrder->next;
            }
        }

        aGraph->preservedOrder = sFirstOrder;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmgJoin::makeNewOrder4Partition( qcStatement        * aStatement,
                                        qmgGraph           * aGraph,
                                        qcmIndex           * aSelectedIndex )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                partitioned table�� preserved order�� ����.
 *                TODO1502: ���� preserved order�� ���� ������ ����.
 *                ������, children(selection) graph�� ���ؼ���
 *                preserved order�� �����Ƿ�, �̸� �ٲ��ش�.
 *
 *                PROJ-1624 global non-partitioned index
 *                index table scan�� ��� preserved order�� �����Ƿ�,
 *                �̸� �ٲ��ش�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qmgChildren  * sChild;

    IDU_FIT_POINT_FATAL( "qmgJoin::makeNewOrder4Partition::__FT__" );

    IDE_DASSERT( aGraph->type == QMG_PARTITION );

    // PROJ-1624 global non-partitioned index
    // global index�� partitioned table���� �����Ǿ� �ִ�.
    if ( aSelectedIndex->indexPartitionType == QCM_NONE_PARTITIONED_INDEX )
    {
        IDE_TEST( makeNewOrder4Selection( aStatement,
                                          aGraph,
                                          aSelectedIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        for( sChild = aGraph->children;
             sChild != NULL;
             sChild = sChild->next )
        {
            IDE_TEST( makeNewOrder4Selection( aStatement,
                                              sChild->childGraph,
                                              aSelectedIndex )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Preserved Order�� direction�� �����Ѵ�.
 *                direction�� NOT_DEFINED �� ��쿡�� ȣ���Ͽ��� �Ѵ�.
 *
 *  Implementation :
 *    
 ***********************************************************************/

    UInt                sSelectedJoinMethodFlag = 0;
    qmgPreservedOrder * sPreservedOrder;
    idBool              sIsSamePrevOrderWithChild;
    qmgDirectionType    sPrevDirection;

    IDU_FIT_POINT_FATAL( "qmgJoin::finalizePreservedOrder::__FT__" );

    // BUG-36803 Join Graph must have a left child graph
    IDE_DASSERT( aGraph->left != NULL );

    switch( aGraph->type )
    {
        case QMG_INNER_JOIN :
        case QMG_SEMI_JOIN :
        case QMG_ANTI_JOIN :
            sSelectedJoinMethodFlag = ((qmgJOIN *)aGraph)->selectedJoinMethod->flag;
            break;
        case QMG_FULL_OUTER_JOIN :
            sSelectedJoinMethodFlag = ((qmgFOJN *)aGraph)->selectedJoinMethod->flag;
            break;
        case QMG_LEFT_OUTER_JOIN :
            sSelectedJoinMethodFlag = ((qmgLOJN *)aGraph)->selectedJoinMethod->flag;
            break;
        default :
            IDE_DASSERT(0);
            break;
    }
    
    switch( sSelectedJoinMethodFlag & QMO_JOIN_METHOD_MASK )
    {
        case QMO_JOIN_METHOD_FULL_NL :
        case QMO_JOIN_METHOD_FULL_STORE_NL :
        case QMO_JOIN_METHOD_ONE_PASS_HASH :
        case QMO_JOIN_METHOD_ONE_PASS_SORT :
        case QMO_JOIN_METHOD_INDEX :
        case QMO_JOIN_METHOD_ANTI :
            // Copy direction from Left child graph's Preserved order
            IDE_TEST( qmg::copyPreservedOrderDirection(
                          aGraph->preservedOrder,
                          aGraph->left->preservedOrder )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_INVERSE_SORT :
            IDE_TEST( qmg::copyPreservedOrderDirection(
                          aGraph->preservedOrder,
                          aGraph->right->preservedOrder )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_MERGE :
        case QMO_JOIN_METHOD_TWO_PASS_SORT :

            sIsSamePrevOrderWithChild =
                qmg::isSamePreservedOrder( aGraph->preservedOrder,
                                           aGraph->left->preservedOrder );

            if ( sIsSamePrevOrderWithChild == ID_TRUE )
            {
                // Child graph�� Preserved order direction�� �����Ѵ�.
                IDE_TEST( qmg::copyPreservedOrderDirection(
                              aGraph->preservedOrder,
                              aGraph->left->preservedOrder )
                          != IDE_SUCCESS );
            }
            else
            {
                // ���� preserved order�� ������ �ʰ�
                // ���� preserved order�� ������ ���,
                // Preserved Order�� direction�� acsending���� ����
                
                sPreservedOrder = aGraph->preservedOrder;

                // ù��° Į���� ascending���� ����
                sPreservedOrder->direction = QMG_DIRECTION_ASC;
                sPrevDirection = QMG_DIRECTION_ASC;

                // �ι�° Į���� ���� Į���� direction ������ ���� ������
                for ( sPreservedOrder = sPreservedOrder->next;
                      sPreservedOrder != NULL;
                      sPreservedOrder = sPreservedOrder->next )
                {
                    switch( sPreservedOrder->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED :
                            sPreservedOrder->direction = QMG_DIRECTION_ASC;
                            break;
                        case QMG_DIRECTION_SAME_WITH_PREV :
                            sPreservedOrder->direction = sPrevDirection;
                            break;
                        case QMG_DIRECTION_DIFF_WITH_PREV :
                            // direction�� ���� Į���� direction�� �ٸ� ���
                            if ( sPrevDirection == QMG_DIRECTION_ASC )
                            {
                                sPreservedOrder->direction = QMG_DIRECTION_DESC;
                            }
                            else
                            {
                                // sPrevDirection == QMG_DIRECTION_DESC
                                sPreservedOrder->direction = QMG_DIRECTION_ASC;
                            }
                            break;
                        case QMG_DIRECTION_ASC :
                            IDE_DASSERT(0);
                            break;
                        case QMG_DIRECTION_DESC :
                            IDE_DASSERT(0);
                            break;
                        case QMG_DIRECTION_ASC_NULLS_FIRST :
                            IDE_DASSERT(0);
                            break;
                        case QMG_DIRECTION_ASC_NULLS_LAST :
                            IDE_DASSERT(0);
                            break;
                        case QMG_DIRECTION_DESC_NULLS_FIRST :
                            IDE_DASSERT(0);
                            break;
                        case QMG_DIRECTION_DESC_NULLS_LAST :
                            IDE_DASSERT(0);
                            break;
                        default:
                            IDE_DASSERT(0);
                            break;
                    }
            
                    sPrevDirection = sPreservedOrder->direction;
                }
            }
            break;

        case QMO_JOIN_METHOD_TWO_PASS_HASH :
        case QMO_JOIN_METHOD_INVERSE_HASH  : // PROJ-2339
        case QMO_JOIN_METHOD_INVERSE_INDEX : // PROJ-2385
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::setJoinPushDownPredicate( qcStatement   * aStatement,
                                   qmgGraph      * aGraph,
                                   qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description : push-down join predicate�� �޾Ƽ� �׷����� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::setJoinPushDownPredicate::__FT__" );

    if ( aGraph->type == QMG_SELECTION )
    {
        IDE_TEST( qmgSelection::setJoinPushDownPredicate(
                      (qmgSELT*)aGraph,
                      aPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aGraph->type == QMG_PARTITION )
        {
            IDE_TEST( qmgPartition::setJoinPushDownPredicate(
                          aStatement,
                          (qmgPARTITION*)aGraph,
                          aPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2638
            IDE_FT_ASSERT( aGraph->type == QMG_SHARD_SELECT );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::setNonJoinPushDownPredicate( qcStatement   * aStatement,
                                      qmgGraph      * aGraph,
                                      qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description : push-down non-join predicate�� �޾Ƽ� �ڽ��� �׷����� ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::setNonJoinPushDownPredicate::__FT__" );

    if( aGraph->type == QMG_SELECTION )
    {
        IDE_TEST( qmgSelection::setNonJoinPushDownPredicate(
                      (qmgSELT*)aGraph,
                      aPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aGraph->type == QMG_PARTITION )
        {
            IDE_TEST( qmgPartition::setNonJoinPushDownPredicate(
                          aStatement,
                          (qmgPARTITION*)aGraph,
                          aPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2638
            IDE_FT_ASSERT( aGraph->type == QMG_SHARD_SELECT );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::alterSelectedIndex( qcStatement * aStatement,
                             qmgGraph    * aGraph,
                             qcmIndex    * aNewIndex )
{
/***********************************************************************
 *
 * Description : ���� graph�� ���� access method�� �ٲ� ���
 *               selection graph�� sdf�� disable ��Ų��.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::alterSelectedIndex::__FT__" );

    if( aGraph->type == QMG_SELECTION )
    {
        IDE_TEST( qmgSelection::alterSelectedIndex(
                      aStatement,
                      (qmgSELT*)aGraph,
                      aNewIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aGraph->type == QMG_PARTITION )
        {
            IDE_TEST( qmgPartition::alterSelectedIndex(
                          aStatement,
                          (qmgPARTITION*)aGraph,
                          aNewIndex )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2638
            IDE_FT_ASSERT( aGraph->type == QMG_SHARD_SELECT );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgJoin::copyGraphAndAlterSelectedIndex( qcStatement * aStatement,
                                         qmgGraph    * aSource,
                                         qmgGraph   ** aTarget,
                                         qcmIndex    * aNewSelectedIndex,
                                         UInt          aWhichOne )
{
/***********************************************************************
 *
 * Description : ���� JOIN graph���� ANTI�� ó���� ��
 *               ���� SELT graph�� �����ϴµ� �̶� �� �Լ���
 *               ���ؼ� �����ϵ��� �ؾ� �����ϴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgJoin::copyGraphAndAlterSelectedIndex::__FT__" );

    if ( aSource->type == QMG_SELECTION )
    {
        IDE_TEST( qmgSelection::copySELTAndAlterSelectedIndex(
                      aStatement,
                      (qmgSELT*)aSource,
                      (qmgSELT**)aTarget,
                      aNewSelectedIndex,
                      aWhichOne )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aSource->type == QMG_PARTITION )
        {
            IDE_TEST( qmgPartition::copyPARTITIONAndAlterSelectedIndex(
                          aStatement,
                          (qmgPARTITION*)aSource,
                          (qmgPARTITION**)aTarget,
                          aNewSelectedIndex,
                          aWhichOne )
                      != IDE_SUCCESS );
        }
        else
        {
            // PROJ-2638
            IDE_FT_ASSERT( aSource->type == QMG_SHARD_SELECT );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qmgJoin::randomSelectJoinMethod ( qcStatement        * aStatement,
                                         UInt                 aJoinMethodCnt,
                                         qmoJoinMethod      * aJoinMethods,
                                         qmoJoinMethodCost ** aSelected )
{
/****************************************************************************************
 *
 * Description : TASK-6744
 *               �� Join Type���� ���� ������ join method�� �߿��� 
 *               random�ϰ� �ϳ� ���� 
 *               
 *     << join method�� �ִ� ���� >>
 *         - Inner Join      : 16
 *         - Semi Join       : 11
 *         - Anti Join       : 10
 *         - Left Outer Join : 8
 *         - Full Outer Join : 12 
 *               
 * Implementation :
 *     1. ��� join method�� �߿��� ���� ������ join method�� ã�Ƽ� ������ ���Ѵ�.
 *     2. ��ⷯ ������ ���ؼ� join method�� random�ϰ� ����
 *
 ****************************************************************************************/

    UInt                i = 0;
    UInt                j = 0;
    UInt                sSelectedMethod = 0;
    UInt                sFeasibilityJoinMethodCnt = 0;
    qmoJoinMethodCost * sFeasibilityJoinMethod[16];
    qmoJoinMethodCost * sJoinMethodCost = NULL;
    qmoJoinMethod     * sJoinMethod     = NULL;
    // using well random generator
    ULong sStage1 = 0;
    ULong sStage2 = 0;
    ULong sStage3 = 0;
    ULong sStage4 = 0;
    ULong sStage5 = 0;

    // �迭 �ʱ�ȭ
    for ( i = 0 ; i < 16 ; i++ )
    {
        sFeasibilityJoinMethod[i] = NULL;
    }

    // ��� join method �߿��� ���� ������ join method �� join method�� ������ ���Ѵ�. 
    for ( i = 0 ; i < aJoinMethodCnt ; i++ )
    {
        sJoinMethod = &aJoinMethods[i];

        for ( j = 0 ; j < sJoinMethod->joinMethodCnt ; j++ )
        {
            sJoinMethodCost = &sJoinMethod->joinMethodCost[j];

            if ( (sJoinMethodCost->flag & QMO_JOIN_METHOD_FEASIBILITY_MASK) == QMO_JOIN_METHOD_FEASIBILITY_TRUE )
            {
                sFeasibilityJoinMethod[sFeasibilityJoinMethodCnt] = sJoinMethodCost;
                sFeasibilityJoinMethodCnt++;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    // join method ����
    aStatement->mRandomPlanInfo->mTotalNumOfCases = aStatement->mRandomPlanInfo->mTotalNumOfCases + sFeasibilityJoinMethodCnt;

    // using well random number generator
    sStage1 = QCU_PLAN_RANDOM_SEED ^ aStatement->mRandomPlanInfo->mWeightedValue ^ (QCU_PLAN_RANDOM_SEED << 16) ^ (aStatement->mRandomPlanInfo->mWeightedValue << 15);
    sStage2 = aStatement->mRandomPlanInfo->mTotalNumOfCases ^ (aStatement->mRandomPlanInfo->mTotalNumOfCases >> 11);
    sStage3 = sStage1 ^ sStage2;
    sStage4 = sStage3 ^ ( (sStage3 << 5) & 0xDA442D20UL );
    sStage5 = sFeasibilityJoinMethodCnt ^ sStage1 ^ sStage4 ^ (sFeasibilityJoinMethodCnt << 2) ^ (sStage1 >> 18) ^ (sStage2 << 28);

    sSelectedMethod = sStage5 % sFeasibilityJoinMethodCnt; 

    IDE_DASSERT( sSelectedMethod < sFeasibilityJoinMethodCnt );

    aStatement->mRandomPlanInfo->mWeightedValue++;

    *aSelected = sFeasibilityJoinMethod[sSelectedMethod];

    return IDE_SUCCESS;
}

void qmgJoin::moveConstantPred( qmgJOIN * aMyGraph, idBool aIsInverse )
{
    qmoPredicate * sPredicate;
    qmoPredicate * sPrev;

    IDE_TEST_RAISE( ( aMyGraph->graph.left == NULL ) ||
                    ( aMyGraph->graph.right == NULL ) ||
                    ( aIsInverse == ID_FALSE ), normal_exit );

    if ( aMyGraph->graph.left->type == QMG_SELECTION )
    {
        if ( aMyGraph->graph.left->left == NULL )
        {
            if ( aMyGraph->graph.left->constantPredicate != NULL )
            {
                if ( aMyGraph->graph.right->constantPredicate == NULL )
                {
                    aMyGraph->graph.right->constantPredicate = aMyGraph->graph.left->constantPredicate;
                    aMyGraph->graph.left->constantPredicate = NULL;
                }
                else
                {
                    for ( sPredicate = aMyGraph->graph.right->constantPredicate;
                          sPredicate != NULL;
                          sPredicate = sPredicate->next )
                    {
                        sPrev = sPredicate;
                    }
                    sPrev->next = aMyGraph->graph.left->constantPredicate;
                    aMyGraph->graph.left->constantPredicate = NULL;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( normal_exit );
}

void qmgJoin::checkOrValueIndex( qmgGraph * aGraph )
{
    qmoPredicate  * sTemp = NULL;
    qmgChildren   * sChild = NULL;

    if ( aGraph != NULL )
    {
        if ( aGraph->type == QMG_SELECTION )
        {
            for ( sTemp = aGraph->myPredicate; sTemp != NULL; sTemp = sTemp->next )
            {
                if ( ( sTemp->flag & QMO_PRED_OR_VALUE_INDEX_MASK )
                     == QMO_PRED_OR_VALUE_INDEX_TRUE )
                {
                    sTemp->flag &= ~QMO_PRED_JOIN_OR_VALUE_INDEX_MASK;
                    sTemp->flag |= QMO_PRED_JOIN_OR_VALUE_INDEX_TRUE;
                }
            }
        }
        else if ( aGraph->type == QMG_PARTITION )
        {
            for ( sChild = ((qmgPARTITION *)aGraph)->graph.children;
                  sChild != NULL;
                  sChild = sChild->next )
            {
                for ( sTemp = sChild->childGraph->myPredicate; sTemp != NULL; sTemp = sTemp->next )
                {
                    if ( ( sTemp->flag & QMO_PRED_OR_VALUE_INDEX_MASK )
                         == QMO_PRED_OR_VALUE_INDEX_TRUE )
                    {
                        sTemp->flag &= ~QMO_PRED_JOIN_OR_VALUE_INDEX_MASK;
                        sTemp->flag |= QMO_PRED_JOIN_OR_VALUE_INDEX_TRUE;
                    }
                }
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
}

