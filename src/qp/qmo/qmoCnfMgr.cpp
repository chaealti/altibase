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
 * $Id: qmoCnfMgr.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
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

#include <ide.h>
#include <idl.h>
#include <qcuSqlSourceInfo.h>
#include <qmoCnfMgr.h>
#include <qmoPredicate.h>
#include <qmgHierarchy.h>
#include <qmgSelection.h>
#include <qmgPartition.h>
#include <qmgJoin.h>
#include <qmgSemiJoin.h>
#include <qmgAntiJoin.h>
#include <qmgLeftOuter.h>
#include <qmgFullOuter.h>
#include <qmgProjection.h>
#include <qmoSelectivity.h>
#include <qmo.h>
#include <qmoHint.h>
#include <qmoCrtPathMgr.h>
#include <qmoTransitivity.h>
#include <qmoAnsiJoinOrder.h>
#include <qmoCostDef.h>
#include <qmvQTC.h>
#include <qcgPlan.h>
#include <qmgShardSelect.h>
#include <qmv.h>

extern mtfModule mtfEqual;

IDE_RC
qmoCnfMgr::init( qcStatement * aStatement,
                 qmoCNF      * aCNF,
                 qmsQuerySet * aQuerySet,
                 qtcNode     * aNormalCNF,
                 qtcNode     * aNnfFilter )
{
/***********************************************************************
 *
 * Description : qmoCnf ���� �� �ʱ�ȭ
 *
 * Implementation :
 *    (1) aNormalCNF�� qmoCNF::normalCNF�� �����Ѵ�.
 *       ( aNormalCNF�� where���� CNF�� normalize �� ���̴�. )
 *    (2) dependencies ����
 *    (3) base graph count ����
 *    (4) base graph�� ���� �� �ʱ�ȭ
 *    (5) joinGroup �迭 ���� Ȯ��
 *
 ***********************************************************************/

    qmoCNF      * sCNF;
    qmsQuerySet * sQuerySet;
    qmsFrom     * sFrom;
    qmsFrom     * sCurFrom;
    UInt          sBaseGraphCnt;
    UInt          sMaxJoinGroupCnt;
    UInt          sTableCnt;
    UInt          i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sQuerySet = aQuerySet;
    sFrom     = sQuerySet->SFWGH->from;

    //------------------------------------------
    // CNF �ʱ�ȭ
    //------------------------------------------

    sCNF = aCNF;
    sCNF->normalCNF = aNormalCNF;
    sCNF->nnfFilter = NULL;
    sCNF->myQuerySet = sQuerySet;
    sCNF->myGraph = NULL;
    sCNF->cost = 0;
    sCNF->constantPredicate = NULL;
    sCNF->oneTablePredicate = NULL;
    sCNF->joinPredicate = NULL;
    sCNF->levelPredicate = NULL;
    sCNF->connectByRownumPred = NULL;
    sCNF->graphCnt4BaseTable = 0;
    sCNF->baseGraph = NULL;
    sCNF->maxJoinGroupCnt = 0;
    sCNF->joinGroupCnt = 0;
    sCNF->joinGroup = NULL;
    sCNF->tableCnt = 0;
    sCNF->tableOrder = NULL;
    sCNF->outerJoinGraph = NULL;
    sCNF->mIsOnlyNL = ID_FALSE;

    //------------------------------------------
    // dependencies ����
    //------------------------------------------

    qtc::dependencySetWithDep( & sCNF->depInfo,
                               & sQuerySet->SFWGH->depInfo );

    //------------------------------------------
    // PROJ-2418
    // Lateral View�� �����ϴ� Relation �� Left/Full-Outer Join ����
    //------------------------------------------

    for ( sCurFrom = sFrom; sCurFrom != NULL; sCurFrom = sCurFrom->next )
    {
        IDE_TEST( validateLateralViewJoin( aStatement, sCurFrom )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // PROJ-1405
    // rownum predicate ó��
    //------------------------------------------

    if ( aNnfFilter != NULL )
    {
        if ( ( aNnfFilter->lflag & QTC_NODE_ROWNUM_MASK )
             == QTC_NODE_ROWNUM_EXIST )
        {
            // NNF filter�� rownum predicate�� ��� critical path�� �ø���.
            IDE_TEST( qmoCrtPathMgr::addRownumPredicateForNode( aStatement,
                                                                aQuerySet,
                                                                aNnfFilter,
                                                                ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            sCNF->nnfFilter = aNnfFilter;
        }
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // baseGraph count, table count ���
    //------------------------------------------

    sBaseGraphCnt = 0;
    sTableCnt = 0;
    for ( sCurFrom = sFrom; sCurFrom != NULL; sCurFrom = sCurFrom->next )
    {
        sBaseGraphCnt++;
        if ( sCurFrom->joinType != QMS_NO_JOIN )
        {
            // outer join�� �����ϴ� table ������ ��
            sTableCnt += qtc::getCountBitSet( & sCurFrom->depInfo );
        }
        else
        {
            sTableCnt++;
        }
    }

    sCNF->graphCnt4BaseTable = sBaseGraphCnt;
    sCNF->maxJoinGroupCnt    = sMaxJoinGroupCnt = sBaseGraphCnt;
    sCNF->tableCnt           = sTableCnt;

    //------------------------------------------
    // baseGraph�� ���� �� �ʱ�ȭ
    //------------------------------------------

    // baseGraph pointer ������ ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgGraph*) * sBaseGraphCnt,
                                             (void **) & sCNF->baseGraph )
              != IDE_SUCCESS );

    if( aQuerySet->SFWGH->outerJoinTree != NULL )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgGraph*),
                                                 (void **) & sCNF->outerJoinGraph )
                  != IDE_SUCCESS );
    }

    // baseGraph���� �ʱ�ȭ
    for ( sFrom = aQuerySet->SFWGH->from, i = 0;
          sFrom != NULL;
          sFrom = sFrom->next, i++ )
    {
        IDE_TEST( initBaseGraph( aStatement,
                                 & sCNF->baseGraph[i],
                                 sFrom,
                                 aQuerySet )
                  != IDE_SUCCESS );
    }

    if( sCNF->outerJoinGraph != NULL )
    {
        IDE_TEST( initBaseGraph( aStatement,
                                 & sCNF->outerJoinGraph,
                                 aQuerySet->SFWGH->outerJoinTree,
                                 aQuerySet )
                  != IDE_SUCCESS );
    }

    if ( sMaxJoinGroupCnt > 0 )
    {
        //------------------------------------------
        // joinGroup �迭 ���� Ȯ�� �� �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoJoinGroup) *
                                                 sMaxJoinGroupCnt,
                                                 (void **) &sCNF->joinGroup )
                  != IDE_SUCCESS );

        // joinGroup���� �ʱ�ȭ
        IDE_TEST( initJoinGroup( aStatement, sCNF->joinGroup,
                                 sMaxJoinGroupCnt, sBaseGraphCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // Join�� �ƴ�
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::init( qcStatement * aStatement,
                 qmoCNF      * aCNF,
                 qmsQuerySet * aQuerySet,
                 qmsFrom     * aFrom,
                 qtcNode     * aNormalCNF,
                 qtcNode     * aNnfFilter )
{
/***********************************************************************
 *
 * Description : on Condition CNF�� ����  qmoCnf ���� �� �ʱ�ȭ
 *
 * Implementation :
 *    (1) aNormalCNF�� qmoCNF::normalCNF�� �����Ѵ�.
 *       (aNormalCNF�� where���� CNF�� normalize �� ���̴�.)
 *    (2) dependencies ����
 *    (3) base graph count ����
 *    (4) base graph�� ���� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmoCNF      * sCNF;
    qmgGraph   ** sBaseGraph;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );

    //------------------------------------------
    // CNF �ʱ�ȭ
    //------------------------------------------

    sCNF = aCNF;
    sCNF->normalCNF = aNormalCNF;
    sCNF->nnfFilter = NULL;
    sCNF->myQuerySet = aQuerySet;
    sCNF->myGraph = NULL;
    sCNF->cost = 0;
    sCNF->constantPredicate = NULL;
    sCNF->oneTablePredicate = NULL;
    sCNF->joinPredicate = NULL;
    sCNF->levelPredicate = NULL;
    sCNF->connectByRownumPred = NULL;
    sCNF->graphCnt4BaseTable = 0;
    sCNF->baseGraph = NULL;
    sCNF->maxJoinGroupCnt = 0;
    sCNF->joinGroupCnt = 0;
    sCNF->joinGroup = NULL;
    sCNF->tableCnt = qtc::getCountBitSet( & aFrom->depInfo );
    sCNF->tableOrder = NULL;

    //------------------------------------------
    // dependencies ����
    //------------------------------------------

    qtc::dependencySetWithDep( & sCNF->depInfo, & aFrom->depInfo );

    //------------------------------------------
    // PROJ-1405
    // rownum predicate ó��
    //------------------------------------------

    if ( aNnfFilter != NULL )
    {
        if ( ( aNnfFilter->lflag & QTC_NODE_ROWNUM_MASK )
             == QTC_NODE_ROWNUM_EXIST )
        {
            // NNF filter�� rownum predicate�� ��� critical path�� �ø���.
            IDE_TEST( qmoCrtPathMgr::addRownumPredicateForNode( aStatement,
                                                                aQuerySet,
                                                                aNnfFilter,
                                                                ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            sCNF->nnfFilter = aNnfFilter;
        }
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // baseGraph�� ���� �� �ʱ�ȭ
    //------------------------------------------

    sCNF->graphCnt4BaseTable = 2;
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgGraph *) * 2,
                                             (void**) &sBaseGraph )
              != IDE_SUCCESS );

    if ( aFrom->left != NULL )
    {
        IDE_TEST( qmoCnfMgr::initBaseGraph( aStatement,
                                            & sBaseGraph[0],
                                            aFrom->left,
                                            aQuerySet )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    if ( aFrom->right != NULL )
    {
        IDE_TEST( qmoCnfMgr::initBaseGraph( aStatement,
                                            & sBaseGraph[1],
                                            aFrom->right,
                                            aQuerySet )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    sCNF->baseGraph = sBaseGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::optimize( qcStatement * aStatement,
                     qmoCNF      * aCNF )
{
/***********************************************************************
 *
 * Description : CNF Processor�� ����ȭ( ��, qmoCNF�� ����ȭ)
 *
 * Implementation :
 *    (1) Predicate�� �з�
 *    (2) �� base graph�� ����ȭ
 *    (3) Join�� ó��
 *
 ***********************************************************************/

    qmsQuerySet * sQuerySet;
    qmoCNF      * sCNF;
    UInt          sBaseGraphCnt;
    UInt          i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF = aCNF;
    sQuerySet = sCNF->myQuerySet;
    sBaseGraphCnt = sCNF->graphCnt4BaseTable;

    //------------------------------------------
    // PROJ-1404 Transitive Predicate ����
    //------------------------------------------

    IDE_TEST( generateTransitivePredicate( aStatement,
                                           sCNF,
                                           ID_FALSE )
              != IDE_SUCCESS );

    //------------------------------------------
    // Predicate�� �з�
    //------------------------------------------
    IDE_TEST( classifyPred4Where( aStatement, sCNF, sQuerySet )
              != IDE_SUCCESS );

    //------------------------------------------
    // �� Base Graph�� ����ȭ
    //------------------------------------------

    for ( i = 0; i < sBaseGraphCnt; i++ )
    {
        if ( sCNF->mIsOnlyNL == ID_TRUE )
        {
            sCNF->baseGraph[i]->flag &= ~QMG_JOIN_ONLY_NL_MASK;
            sCNF->baseGraph[i]->flag |= QMG_JOIN_ONLY_NL_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
        IDE_TEST( sCNF->baseGraph[i]->optimize( aStatement,
                                                sCNF->baseGraph[i] )
                  != IDE_SUCCESS );
    }

    if( sCNF->outerJoinGraph != NULL )
    {
        IDE_TEST( sCNF->outerJoinGraph->optimize( aStatement,
                                                  sCNF->outerJoinGraph )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // fix BUG-19203
    // subquery�� ���� optimize�� �Ѵ�.
    // selection graph�� optimize���� view�� ���� ��� ������ ����Ǳ� ������
    // �� ���Ŀ� �ϴ� ���� �´�.
    //------------------------------------------

    // To Fix BUG-8067, BUG-8742
    // constant predicate�� subquery ó��
    if ( sCNF->constantPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->constantPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    // To Fix PR-12743
    // NNF Filter ����
    if ( sCNF->nnfFilter != NULL )
    {
        IDE_TEST( qmoPred
                  ::optimizeSubqueryInNode( aStatement,
                                            sCNF->nnfFilter,
                                            ID_FALSE,  //No Range
                                            ID_FALSE ) //No ConstantFilter
                  != IDE_SUCCESS );

    }
    else
    {
        // nothing to do
    }

    //------------------------------------------
    // Join�� ó��
    //------------------------------------------

    if ( sBaseGraphCnt == 1 )
    {
        sCNF->myGraph = sCNF->baseGraph[0];
    }
    else
    {
        IDE_TEST( joinOrdering( aStatement, sCNF ) != IDE_SUCCESS );
        IDE_FT_ASSERT( sCNF->myGraph != NULL );
    }

    // To Fix PR-9700
    // Cost ���� �� Join Ordering ������ �ƴ�
    // CNF�� ���� Graph�� ������ �Ŀ� �����Ͽ��� ��.
    sCNF->cost    = sCNF->myGraph->costInfo.totalAllCost;

    //------------------------------------------
    // Constant Predicate�� ó��
    //------------------------------------------

    // fix BUG-9791, BUG-10419, BUG-29551
    // constant filter�� ó�������� ������ left graph�� ������ (X)
    // constant filter�� ó�������� �ֻ��� graph�� �����Ѵ� (O)
    IDE_TEST( pushSelection4ConstantFilter( aStatement,
                                            sCNF->myGraph,
                                            sCNF )
              != IDE_SUCCESS );

    //------------------------------------------
    // NNF Filter�� ����
    //------------------------------------------

    sCNF->myGraph->nnfFilter = sCNF->nnfFilter;

    //------------------------------------------
    // Table Order �� ����
    //------------------------------------------

    IDE_TEST( getTableOrder( aStatement, sCNF->myGraph, & sCNF->tableOrder )
              != IDE_SUCCESS );

    // dependency ������ ���� CNF�� �ֻ��� graph�� �ش� CNF�� �����ϰ�
    // �־�� �Ѵ�.
    sCNF->myGraph->myCNF = sCNF;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::removeOptimizationInfo( qcStatement * aStatement,
                                   qmoCNF      * aCNF )
{
    UInt i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::removeOptimizationInfo::__FT__" );

    for( i = 0; i < aCNF->graphCnt4BaseTable; i++ )
    {
        IDE_TEST( qmg::removeSDF( aStatement,
                                  aCNF->baseGraph[i] )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::classifyPred4Where( qcStatement       * aStatement,
                               qmoCNF            * aCNF,
                               qmsQuerySet       * aQuerySet)
{
/***********************************************************************
 *
 * Description : Where�� ó���� ���� Predicate�� �з�
 *               ( �Ϲ� CNF�� Predicate �з� )
 *
 * Implementation :
 *    qmoCNF::normalCNF�� �� Predicate�� ���Ͽ� ������ ����
 *    (1) Rownum Predicate �з�
 *        rownum predicate�� where���� ó������ �ʰ� critical path���� ó���Ѵ�.
 *    (2) Constant Predicate �з�
 *        constant predicate, level predicate, prior predicate�� ���õȴ�.
 *        �̴� addConstPred4Where() �Լ����� hierarhcy graph ���� ������ ����
 *        �и��Ͽ� ó���Ѵ�.
 *    (3) One Table Predicate�� �з�
 *    (4) Join Predicate�� �з�
 *
 ***********************************************************************/

    qmoCNF         * sCNF;
    qtcNode        * sNode;
    qmoPredicate   * sNewPred = NULL;
    qcDepInfo      * sFromDependencies;
    qcDepInfo      * sGraphDependencies;
    qmgGraph      ** sBaseGraph;

    idBool           sExistHierarchy;
    idBool           sIsRownum;
    idBool           sIsConstant;
    idBool           sIsOneTable = ID_FALSE;    // TASK-3876 Code Sonar
    idBool           sIsHierJoin = ID_FALSE;

    UInt             sBaseGraphCnt;
    UInt             i;

    idBool           sIsPush = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::classifyPred4Where::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF              = aCNF;
    sFromDependencies = & sCNF->depInfo;
    sBaseGraphCnt     = sCNF->graphCnt4BaseTable;
    sBaseGraph        = sCNF->baseGraph;

    // Hierarchy ���� ���� Ȯ��
    if ( sCNF->baseGraph[0]->type == QMG_HIERARCHY )
    {
        sExistHierarchy = ID_TRUE;
    }
    else
    {
        sExistHierarchy = ID_FALSE;
    }

    if ( ( aQuerySet->SFWGH->hierarchy != NULL ) &&
         ( ( aQuerySet->SFWGH->from->joinType != QMS_NO_JOIN ) ||
           ( aQuerySet->SFWGH->from->next != NULL ) ) )
    {
        sIsHierJoin = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    // Predicate �з� ��� Node
    if ( sCNF->normalCNF != NULL )
    {
        sNode = (qtcNode *)sCNF->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    while( sNode != NULL )
    {
        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            &sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Rownum Predicate �˻�
        //-------------------------------------------------

        IDE_TEST( qmoPred::isRownumPredicate( sNewPred,
                                              & sIsRownum )
                  != IDE_SUCCESS );

        if ( ( sIsRownum == ID_TRUE ) &&
             ( sIsHierJoin == ID_FALSE ) )
        {
            //---------------------------------------------
            // Rownum Predicate �з�
            //    Rownum Predicate�� critical path�� rownumPredicate�� ����
            //---------------------------------------------

            IDE_TEST( qmoCrtPathMgr::addRownumPredicate( aQuerySet,
                                                         sNewPred )
                      != IDE_SUCCESS );
        }
        else
        {
            //-------------------------------------------------
            // Constant Predicate �˻�
            //-------------------------------------------------

            IDE_TEST( qmoPred::isConstantPredicate( sNewPred,
                                                    sFromDependencies,
                                                    & sIsConstant )
                      != IDE_SUCCESS );

            if ( ( sIsConstant == ID_TRUE ) &&
                 ( sIsHierJoin == ID_FALSE ) )
            {
                sNewPred->flag &= ~QMO_PRED_CONSTANT_FILTER_MASK;
                sNewPred->flag |= QMO_PRED_CONSTANT_FILTER_TRUE;

                //-------------------------------------------------
                // Constant Predicate �з�
                //   Hierarhcy ���� ������ ���� constant predicate,
                //   level predicate, prior predicat�� �з��Ͽ� �ش� ��ġ�� �߰�
                //-------------------------------------------------

                IDE_TEST( addConstPred4Where( sNewPred,
                                              sCNF,
                                              sExistHierarchy )
                          != IDE_SUCCESS );
            }
            else
            {
                //---------------------------------------------
                // One Table Predicate �˻�
                //---------------------------------------------

                for ( i = 0 ; i < sBaseGraphCnt; i++ )
                {
                    sGraphDependencies = & sBaseGraph[i]->myFrom->depInfo;
                    IDE_TEST( qmoPred::isOneTablePredicate( sNewPred,
                                                            sFromDependencies,
                                                            sGraphDependencies,
                                                            & sIsOneTable )
                              != IDE_SUCCESS );

                    if ( sIsOneTable == ID_TRUE )
                    {
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                if ( ( sIsOneTable == ID_TRUE ) &&
                     ( sIsHierJoin == ID_FALSE ) )
                {
                    //---------------------------------------------
                    // One Table Predicate �з�
                    //    oneTablePredicate�� �ش� graph�� myPredicate�� ����
                    //    rid predicate �̸� �ش� graph�� ridPredicate�� ����
                    //---------------------------------------------
                    if ((sNewPred->node->lflag & QTC_NODE_COLUMN_RID_MASK) ==
                        QTC_NODE_COLUMN_RID_EXIST)
                    {
                        sNewPred->next = sBaseGraph[i]->ridPredicate;
                        sBaseGraph[i]->ridPredicate = sNewPred;
                    }
                    else
                    {
                        sNewPred->next = sBaseGraph[i]->myPredicate;
                        sBaseGraph[i]->myPredicate = sNewPred;
                    }
                }
                else
                {
                    if ( ( sIsOneTable == ID_TRUE ) &&
                         ( sIsHierJoin == ID_TRUE ) )
                    {
                        /** PROJ-2509 HierarchyQuery Join
                         * OneTable�̸鼭 HierarchyJoin�̸� Hierarchy Graph
                         * ���� ó���Ǿ���Ѵ�.
                         */
                    }
                    else
                    {
                        //---------------------------------------------
                        // Join Predicate �з�
                        //  constantPredicate, oneTablePredicate�� �ƴϸ�
                        //  1. PUSH_PRED hint�� �����ϸ�,
                        //     view ���η� ������,
                        //  2. PUSH_PRED hint�� �������� ������,
                        //     joinPredicate�̹Ƿ� �̸� sCNF->joinPredicate�� ����
                        //---------------------------------------------

                        sIsPush = ID_FALSE;

                        if ( aQuerySet->SFWGH->hints->pushPredHint != NULL )
                        {
                            //---------------------------------------------
                            // PROJ-1495
                            // PUSH_PRED hint�� �����ϴ� ���
                            // VIEW ���η� join predicate�� ������.
                            //---------------------------------------------

                            IDE_TEST( pushJoinPredInView( sNewPred,
                                                          aQuerySet->SFWGH->hints->pushPredHint,
                                                          sBaseGraphCnt,
                                                          QMS_NO_JOIN, // aIsPushPredInnerJoin
                                                          sBaseGraph,
                                                          & sIsPush )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // Nothing To Do
                        }

                        if ( sIsPush == ID_TRUE )
                        {
                            // Nothing To Do
                        }
                        else
                        {
                            //---------------------------------------------
                            // constantPredicate, oneTablePredicate�� �ƴϰ�,
                            // PUSH_PRED hint�� ���� ��� �̸�
                            // sCNF->joinPredicate�� ����
                            //---------------------------------------------

                            sNewPred->flag &= ~QMO_PRED_JOIN_PRED_MASK;
                            sNewPred->flag  |= QMO_PRED_JOIN_PRED_TRUE;

                            sNewPred->next = sCNF->joinPredicate;
                            sCNF->joinPredicate = sNewPred;

                        }
                    }
                }
            }
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    //---------------------------------------------------------------------
    // BUG-34295 Join ordering ANSI style query
    //     Where ���� predicate �� outerJoinGraph �� ������
    //     one table predicate �� ã�� �̵���Ų��.
    //     outerJoinGraph �� one table predicate �� baseGraph ��
    //     dependency �� ��ġ�� �ʾƼ� predicate �з� ��������
    //     constant predicate ���� �߸� �з��ȴ�.
    //     �̸� �ٷ���� ���� sCNF->constantPredicate �� predicate �鿡��
    //     outerJoinGraph �� ���õ� one table predicate ���� ã�Ƴ���
    //     outerJoinGraph �� �̵���Ų��.
    //---------------------------------------------------------------------
    if( sCNF->outerJoinGraph != NULL )
    {
        IDE_TEST( qmoAnsiJoinOrder::fixOuterJoinGraphPredicate( aStatement,
                                                                sCNF )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::classifyPred4OnCondition( qcStatement     * aStatement,
                                     qmoCNF          * aCNF,
                                     qmoPredicate   ** aUpperPred,
                                     qmoPredicate    * aLowerPred,
                                     qmsJoinType       aJoinType )
{
/***********************************************************************
 *
 * Description : onConditionCNF�� Predicate �з�
 *
 * Implementation :
 *    (1) onConditionCNF Predicate�� �з�
 *        �� onConditionCNF�� predicate�� ���Ͽ� ������ ����
 *        A. Rownum Predicate�� �з�
 *        B. Constant Predicate�� �з�
 *        C. One Table Predicate�� �з�
 *        D. Join Predicate�� �з�
 *    (2) Join �迭 graph�� myPredicate�� ���� push selection ����
 *        �� UpperPredicate�� ���Ͽ� ������ ����
 *        ( UpperPredicate : onConditionCNF�� ������ Join Graph�� myPredicate )
 *        �� UpperPredicate�� ���Ͽ� ������ ����
 *        A. One Table Predicate �˻�
 *        B. One Table Predicate�̸�, Push Selection ����
 *        ����. One Table Predicate ���� push selection ����� ����
 *        - constant predicate : ���� predicate�� �������� ����
 *        - one table predicate : push selection
 *        - join predicate : ���� filter�� ����ؾ� ������ �״�� ��
 *
 ***********************************************************************/

    qmoCNF         * sCNF;
    qmsQuerySet    * sQuerySet;
    qtcNode        * sNode;
    qmoPredicate   * sNewPred;
    qmoPredicate   * sUpperPred;
    qmoPredicate   * sLowerPred;
    qcDepInfo      * sFromDependencies;
    qcDepInfo      * sGraphDependencies;
    qmgGraph      ** sBaseGraph;
    idBool           sIsRownum;
    idBool           sIsConstant;
    idBool           sIsOneTable;
    idBool           sIsLeft;
    UInt             i;
    idBool           sPushDownAble;
    idBool           sIsPush;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::classifyPred4OnCondition::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF              = aCNF;
    sQuerySet         = sCNF->myQuerySet;
    sUpperPred        = *aUpperPred;
    sLowerPred        = aLowerPred;
    sFromDependencies = & sCNF->depInfo;
    sBaseGraph        = sCNF->baseGraph;

    //------------------------------------------
    // onCondition�� ���� Predicate�� �з�
    //------------------------------------------

    // Predicate �з� ��� Node
    if ( sCNF->normalCNF != NULL )
    {
        sNode = (qtcNode *)sCNF->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    // BUG-42968 ansi ��Ÿ�� �����϶� push_pred ��Ʈ ����
    if ( sQuerySet->SFWGH->hints->pushPredHint != NULL )
    {
        sPushDownAble = checkPushPredHint( sCNF,
                                           sNode,
                                           aJoinType );
    }
    else
    {
        sPushDownAble = ID_FALSE;
    }

    while( sNode != NULL )
    {

        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            &sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Rownum Predicate �˻�
        //-------------------------------------------------

        IDE_TEST( qmoPred::isRownumPredicate( sNewPred,
                                              & sIsRownum )
                  != IDE_SUCCESS );

        if ( sIsRownum == ID_TRUE )
        {
            //---------------------------------------------
            // Rownum Predicate �з�
            //    Rownum Predicate�� critical path�� rownumPredicate�� ����
            //---------------------------------------------

            IDE_TEST( qmoCrtPathMgr::addRownumPredicate( sQuerySet,
                                                         sNewPred )
                      != IDE_SUCCESS );
        }
        else
        {
            //-------------------------------------------------
            // Constant Predicate �˻�
            //-------------------------------------------------

            IDE_TEST( qmoPred::isConstantPredicate( sNewPred,
                                                    sFromDependencies,
                                                    & sIsConstant )
                      != IDE_SUCCESS );

            if ( sIsConstant == ID_TRUE )
            {
                //-------------------------------------------------
                // Constant Predicate �з�
                //    onConditionCNF�� constantPredicate�� ����
                //-------------------------------------------------

                sNewPred->next = sCNF->constantPredicate;
                sCNF->constantPredicate = sNewPred;
            }
            else
            {
                //---------------------------------------------
                // One Table Predicate �˻�
                //---------------------------------------------

                for ( i = 0 ; i < 2 ; i++ )
                {
                    // BUG-42944 ansi ��Ÿ�� �����϶� push_pred ��Ʈ�� ����ص� 1���� ������Ŷ�� �������ϴ�.
                    sGraphDependencies = & sBaseGraph[i]->myFrom->depInfo;
                    IDE_TEST( qmoPred::isOneTablePredicate( sNewPred,
                                                            sFromDependencies,
                                                            sGraphDependencies,
                                                            & sIsOneTable )
                              != IDE_SUCCESS );

                    if ( sIsOneTable == ID_TRUE )
                    {
                        if ( i == 0 )
                        {
                            sIsLeft = ID_TRUE;
                        }
                        else
                        {
                            sIsLeft = ID_FALSE;
                        }

                        /* BUG-48405 */
                        if ( ( sQuerySet->lflag & QMV_QUERYSET_ANSI_JOIN_ORDERING_MASK )
                             == QMV_QUERYSET_ANSI_JOIN_ORDERING_TRUE )
                        {
                            if ( sNewPred->node->depInfo.depCount > sGraphDependencies->depCount )
                            {
                                sIsOneTable = ID_FALSE;
                            }
                        }
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                }

                if ( sIsOneTable == ID_TRUE )
                {
                    //---------------------------------------------
                    // One Table Predicate �з�
                    //    joinType�� left/right graph ���⿡ ����
                    //    push selection ���� �����Ͽ� �ش� ��ġ�� �߰�
                    //    - push selection ������ ���
                    //      �ش� graph�� myPredicate�� ����
                    //    - push selection �� �� ���� ���
                    //      onConditionCNF�� oneTablePredicate�� ����
                    //---------------------------------------------

                    IDE_TEST( addOneTblPredOnJoinType( sCNF,
                                                       sBaseGraph[i],
                                                       sNewPred,
                                                       aJoinType,
                                                       sIsLeft )
                              != IDE_SUCCESS );

                }
                else
                {
                    //---------------------------------------------
                    // Join Predicate �з�
                    //    constantPredicate, oneTablePredicate�� �ƴϸ�
                    //    joinPredicate�̹Ƿ� �̸� sCNF->joinPredicate�� ����
                    //---------------------------------------------

                    sIsPush = ID_FALSE;

                    if ( sPushDownAble == ID_TRUE )
                    {
                        //---------------------------------------------
                        // PROJ-1495
                        // PUSH_PRED hint�� �����ϴ� ���
                        // VIEW ���η� join predicate�� ������.
                        //---------------------------------------------
                        IDE_TEST( pushJoinPredInView( sNewPred,
                                                      sQuerySet->SFWGH->hints->pushPredHint,
                                                      sCNF->graphCnt4BaseTable,
                                                      aJoinType,
                                                      sCNF->baseGraph,
                                                      & sIsPush )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    if( sIsPush == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sNewPred->flag &= ~QMO_PRED_JOIN_PRED_MASK;
                        sNewPred->flag |= QMO_PRED_JOIN_PRED_TRUE;

                        sNewPred->next = sCNF->joinPredicate;
                        sCNF->joinPredicate = sNewPred;
                    }
                }
            }
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    //---------------------------------------------
    //  PROJ-1404
    //  Transitive Predicate Generation���� ���� graph���� upper predicate��
    //  on���� transitive predicate�� �����Ҽ� �ִ�.
    //  upper predicate�� joinType�� ���� �����ؼ� �������� �ٷ� ��������
    //  �ᱹ���� �ش� graph�� ��������. (IS NULL, IS NOT NULL�� ����)
    //
    //  upper predicate�� on���� ���� �����Ǵ� transitive predicate��
    //  IS NULL, IS NOT NULL predicate�� �ƴϸ� joinType�� ������� �ش�
    //  graph�� �ٷ� ���� �� �ִµ�, �̴� upper predicate�� on��������
    //  ������ lower predicate�̱� �����̴�. (upper predicate�� ��Ե�
    //  ������ predicate�̹Ƿ� ó������ lower predicate�̶�� �� �� �ִ�.)
    //
    //  lower predicate�� graph�� left�� right�� ���� one table predicate�̰ų�
    //  join predicate�� �� �ִ�. join predicate�� ������ ��� �ش� graph��
    //  ���� �� ���� ������ ���� graph���� ó���ؾ� �ϳ�,
    //  left outer join�̳� full outer join���� filter�� ó���ϰ� �ǹǷ�
    //  bad transitive predicate�� �Ǿ� �����Ѵ�.
    //
    //  lower predicate�� ó���� one table predicate�� �� �ش� graph��
    //  ������ �ȴ�. �̰��� upper predicate�� ó���ϴ� pushSelectionOnJoinType
    //  �Լ��� INNER_JOIN type�϶��� ó������� ���� �� �Լ��� �״��
    //  �̿��Ѵ�.
    //---------------------------------------------

    if ( sLowerPred != NULL )
    {
        IDE_TEST( pushSelectionOnJoinType(aStatement, & sLowerPred, sBaseGraph,
                                          sFromDependencies, QMS_INNER_JOIN )
                  != IDE_SUCCESS );

        if ( aJoinType == QMS_INNER_JOIN )
        {
            // INNER JOIN�� ��� join predicate�� upper predicate�� �����Ѵ�.
            IDE_TEST( qmoTransMgr::linkPredicate( sLowerPred,
                                                  aUpperPred )
                      != IDE_SUCCESS );

            sUpperPred = *aUpperPred;
        }
        else
        {
            // INNER JOIN�� �ƴ� ��� ���� join predicate�� bad transitive
            // predicate�̹Ƿ� �����Ѵ�.(������.)

            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------
    // Join �迭 Graph�� myPredicate�� ���� Push Selection
    //    Upper Predicate�� One Table Predicate�̸� Join Type�� �°�
    //    push selection ����
    //---------------------------------------------


    if ( sUpperPred != NULL )
    {
        IDE_TEST( pushSelectionOnJoinType(aStatement, aUpperPred, sBaseGraph,
                                          sFromDependencies, aJoinType )
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::classifyPred4StartWith( qcStatement   * aStatement ,
                                   qmoCNF        * aCNF,
                                   qcDepInfo     * aDepInfo )
{
/***********************************************************************
 *
 * Description : startWithCNF�� Predicate�� �з�
 *
 * Implementation :
 *    (1) Constant Predicate�� �з�
 *    (2) One Table Predicate�� �з�
 *    ���� : Hierarchy Query�� join�� ����� �� ���� ������
 *           startWithCNF���� joinPredicate�� �� �� ����
 *
 ***********************************************************************/

    qmoCNF         * sCNF;
    qtcNode        * sNode;
    qmoPredicate   * sNewPred;
    qcDepInfo      * sFromDependencies;

    idBool           sIsConstant;
    idBool           sIsOneTable;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::classifyPred4StartWith::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF              = aCNF;
    sFromDependencies = aDepInfo;

    // Predicate �з� ��� Node
    if ( sCNF->normalCNF != NULL )
    {
        sNode = (qtcNode *)sCNF->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    while( sNode != NULL )
    {

        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            &sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Constant Predicate �˻�
        //-------------------------------------------------

        IDE_TEST( qmoPred::isConstantPredicate( sNewPred,
                                                sFromDependencies,
                                                & sIsConstant )
                  != IDE_SUCCESS );

        if ( sIsConstant == ID_TRUE )
        {
            //-------------------------------------------------
            // Constant Predicate �з�
            //    onConditionCNF�� constantPredicate�� ����
            //-------------------------------------------------

            if ( ( sNewPred->node->lflag & QTC_NODE_LEVEL_MASK )
                 == QTC_NODE_LEVEL_EXIST )
            {
                // level predicate�� ���, oneTablePredicate�� ����
                sNewPred->next = sCNF->oneTablePredicate;
                sCNF->oneTablePredicate = sNewPred;
            }
            else
            {
                sNewPred->next = sCNF->constantPredicate;
                sCNF->constantPredicate = sNewPred;
            }
        }
        else
        {
            //---------------------------------------------
            // One Table Predicate �з�
            //---------------------------------------------

            // To fix BUG-14370
            IDE_TEST( qmoPred::isOneTablePredicate( sNewPred,
                                                    sFromDependencies,
                                                    sFromDependencies,
                                                    &sIsOneTable )
                      != IDE_SUCCESS );

            IDE_DASSERT( sIsOneTable == ID_TRUE );

            sNewPred->next = sCNF->oneTablePredicate;
            sCNF->oneTablePredicate = sNewPred;
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    //---------------------------------------------
    // start with CNF�� �� Predicate�� subuqery ó��
    //    statr with CNF�� �� Predicate�� �ش� graph�� myPredicate�� �������
    //    �ʾ� �ش� graph�� myPredicateó�� �ÿ� subquery�� �������� �ʴ´�.
    //---------------------------------------------

    if ( sCNF->constantPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->constantPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    // To Fix PR-12743
    // NNF Filter ����
    if ( sCNF->nnfFilter != NULL )
    {
        IDE_TEST( qmoPred
                  ::optimizeSubqueryInNode( aStatement,
                                            sCNF->nnfFilter,
                                            ID_FALSE,  //No Range
                                            ID_FALSE ) //No ConstantFilter
                  != IDE_SUCCESS );

    }
    else
    {
        // nothing to do
    }

    if ( sCNF->oneTablePredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->oneTablePredicate,
                                               ID_TRUE ) // Use KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::classifyPred4ConnectBy( qcStatement     * aStatement,
                                   qmoCNF          * aCNF,
                                   qcDepInfo       * aDepInfo )
{
/***********************************************************************
 *
 * Description : connectBy ó���� Predicate�� �з�
 *               ( connectByCNF�� Predicate �з� )
 *
 * Implementation :
 *    (1) Constant Predicate�� �з�
 *        constant predicate, level predicate, prior predicate�� ���õȴ�.
 *        �̴� addConstPred4ConnectBy() �Լ����� hierarhcy graph ���� ������
 *        ���� �и��Ͽ� ó���Ѵ�.
 *    (2) One Table Predicate�� �з�
 *    ���� : Hierarchy Query�� join�� ����� �� ���� ������
 *           connectByCNF���� joinPredicate�� �� �� ����
 *
 ***********************************************************************/

    qmoCNF         * sCNF;
    qtcNode        * sNode;
    qmoPredicate   * sNewPred;
    qcDepInfo      * sFromDependencies;

    idBool           sIsConstant;
    idBool           sIsOneTable;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::classifyPred4ConnectBy::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF              = aCNF;

    if ( sCNF->normalCNF != NULL )
    {
        sNode = (qtcNode *)sCNF->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    sFromDependencies = aDepInfo;

    while( sNode != NULL )
    {

        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            &sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Constant Predicate �з�
        //-------------------------------------------------

        IDE_TEST( qmoPred::isConstantPredicate( sNewPred,
                                                sFromDependencies,
                                                & sIsConstant )
                  != IDE_SUCCESS );

        if ( sIsConstant == ID_TRUE )
        {
            // Rownum/Level/Prior/Constant Predicate �з��Ͽ� predicate �߰�
            IDE_TEST( addConstPred4ConnectBy( sCNF,
                                              sNewPred )
                      != IDE_SUCCESS );
        }
        else
        {
            //-------------------------------------------------
            // One Table Predicate �з�
            //-------------------------------------------------

            // BUG-38675
            if ( ( sNewPred->node->lflag & QTC_NODE_LEVEL_MASK )
                 == QTC_NODE_LEVEL_EXIST )
            {
                sNewPred->next = sCNF->levelPredicate;
                sCNF->levelPredicate = sNewPred;
            }
            else
            {
                // To fix BUG-14370
                IDE_TEST( qmoPred::isOneTablePredicate( sNewPred,
                                                        sFromDependencies,
                                                        sFromDependencies,
                                                        &sIsOneTable )
                          != IDE_SUCCESS );

                IDE_DASSERT( sIsOneTable == ID_TRUE );

                sNewPred->next = sCNF->oneTablePredicate;
                sCNF->oneTablePredicate = sNewPred;
            }
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    //---------------------------------------------
    // connect by CNF�� �� Predicate�� subuqery ó��
    //    statr with CNF�� �� Predicate�� �ش� graph�� myPredicate�� �������
    //    �ʾ� �ش� graph�� myPredicateó�� �ÿ� subquery�� �������� �ʴ´�.
    //---------------------------------------------

    if ( sCNF->constantPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->constantPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    // To Fix PR-12743
    // NNF Filter ����
    if ( sCNF->nnfFilter != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueryInNode( aStatement,
                                                   sCNF->nnfFilter,
                                                   ID_FALSE,  //No Range
                                                   ID_FALSE ) //No ConstantFilter
                  != IDE_SUCCESS );

    }
    else
    {
        // nothing to do
    }

    if ( sCNF->oneTablePredicate != NULL )
    {
        // BUG-47522 Connect by ������ subquery keyrange tip�� ������� �ʽ��ϴ�.
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->oneTablePredicate,
                                               ID_FALSE ) // Use KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    if ( sCNF->levelPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->levelPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    /* BUG-39434 The connect by need rownum pseudo column. */
    if ( sCNF->connectByRownumPred != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sCNF->connectByRownumPred,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::initBaseGraph( qcStatement * aStatement,
                          qmgGraph   ** aBaseGraph,
                          qmsFrom     * aFrom,
                          qmsQuerySet * aQuerySet )
{
/***********************************************************************
 *
 * Description : Base Graph�� ���� �� �ʱ�ȭ
 *
 * Implementation :
 *    aFrom�� ���󰡸鼭 �ش� Table�� �����ϴ� base graph�� �ʱ�ȭ �Լ� ȣ��
 *
 ***********************************************************************/

    qmsQuerySet     * sQuerySet;
    qmsHierarchy    * sHierarchy;
    qcShardStmtType   sShardStmtType;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::initBaseGraph::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sQuerySet      = aQuerySet;
    sHierarchy     = sQuerySet->SFWGH->hierarchy;

    if ( ( sHierarchy != NULL ) &&
         ( sQuerySet->SFWGH->from->next == NULL ) &&
         ( sQuerySet->SFWGH->from->joinType == QMS_NO_JOIN ) )
    {
        //------------------------------------------
        // Hierarchy Graph �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( qmgHierarchy::init( aStatement,
                                      sQuerySet,
                                      NULL,
                                      aFrom,
                                      sHierarchy,
                                      aBaseGraph )
                  != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------------
        // �� ���� base Graph �ʱ�ȭ
        //------------------------------------------

        switch( aFrom->joinType )
        {
            case QMS_NO_JOIN :
                if( (aFrom->tableRef->view == NULL) &&
                    (aFrom->tableRef->tableInfo->tablePartitionType
                     == QCM_PARTITIONED_TABLE ) )
                {
                    // PROJ-1502 PARTITIONED DISK TABLE
                    // base table�̰� partitioned table�̸�
                    // partition graph�� �����Ѵ�.
                    IDE_TEST( qmgPartition::init( aStatement,
                                                  sQuerySet,
                                                  aFrom,
                                                  aBaseGraph )
                              != IDE_SUCCESS );
                }
                else
                {
                    // PROJ-2638
                    if ( aFrom->tableRef->view != NULL )
                    {
                        sShardStmtType = aFrom->tableRef->view->myPlan->parseTree->stmtShard;
                    }
                    else
                    {
                        sShardStmtType = QC_STMT_SHARD_NONE;
                    }

                    if ( ( sShardStmtType != QC_STMT_SHARD_NONE ) &&
                         ( sShardStmtType != QC_STMT_SHARD_META ) )
                    {
                        IDE_TEST( qmgShardSelect::init( aStatement,
                                                        sQuerySet,
                                                        aFrom,
                                                        aBaseGraph )
                                     != IDE_SUCCESS );
                    }
                    else
                    {
                        // PROJ-1502 PARTITIONED DISK TABLE
                        // base table�̰� non-partitioned table�̰ų�,
                        // view�̸� selection graph�� �����Ѵ�.
                        IDE_TEST( qmgSelection::init( aStatement,
                                                      sQuerySet,
                                                      aFrom,
                                                      aBaseGraph )
                                  != IDE_SUCCESS );
                    }
                }

                break;

            case QMS_INNER_JOIN :
                IDE_TEST( qmgJoin::init( aStatement,
                                         sQuerySet,
                                         aFrom,
                                         aBaseGraph )
                          != IDE_SUCCESS );
                break;

            case QMS_LEFT_OUTER_JOIN :
                IDE_TEST( qmgLeftOuter::init( aStatement,
                                              sQuerySet,
                                              aFrom,
                                              aBaseGraph )
                          != IDE_SUCCESS );
                break;

            case QMS_FULL_OUTER_JOIN :
                IDE_TEST( qmgFullOuter::init( aStatement,
                                              sQuerySet,
                                              aFrom,
                                              aBaseGraph )
                          != IDE_SUCCESS );
                break;

            default :
                IDE_DASSERT( 0 );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::initJoinGroup( qcStatement  * aStatement,
                          qmoJoinGroup * aJoinGroup,
                          UInt           aJoinGroupCnt,
                          UInt           aBaseGraphCnt )
{
/***********************************************************************
 *
 * Description : Join Group�� �ʱ�ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoJoinGroup * sCurJoinGroup;
    UInt         i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::initJoinGroup::__FT__" );

    // �� Join Group�� �ʱ�ȭ
    for ( i = 0; i < aJoinGroupCnt; i++ )
    {
        sCurJoinGroup = & aJoinGroup[i];

        sCurJoinGroup->joinPredicate = NULL;
        sCurJoinGroup->joinRelation = NULL;
        sCurJoinGroup->joinRelationCnt = 0;
        qtc::dependencyClear( & sCurJoinGroup->depInfo );
        sCurJoinGroup->topGraph = NULL;
        sCurJoinGroup->baseGraphCnt = 0;
        sCurJoinGroup->baseGraph = NULL;
        sCurJoinGroup->mIsOnlyNL = ID_FALSE;
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgGraph* ) * aBaseGraphCnt,
                                                 (void**)&sCurJoinGroup->baseGraph )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::addConstPred4Where( qmoPredicate * aPredicate,
                               qmoCNF       * aCNF,
                               idBool         aExistHierarchy )
{
/***********************************************************************
 *
 * Description : Where�� Level/Prior/Constant Predicate�� �з�
 *
 * Implementation :
 *    (1) Hierarchy�� �����ϴ� ���
 *        A. Level Predicate�� ��� : qmgHierarchy::myPredicate�� ����
 *        B. Prior Predicate�� ��� : qmgHierarchy::myPredicate�� ����
 *        C. �� �� �Ϲ� Constant Predicate : qmoCNF::constantPredicate�� ����
 *        ���� : qmgHierarhcy::myPredicate�� PLAN NODE ���� ��,
 *               filter�� ó����
 *    (2) Hierarchy�� �������� �ʴ� ���
 *        qmoCnf::constantPredicate�� ����
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoCNF       * sCNF;
    qmgGraph     * sHierGraph;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::addConstPred4Where::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sPredicate = aPredicate;
    sCNF = aCNF;

    if ( aExistHierarchy == ID_TRUE )
    {
        //------------------------------------------
        // Hierarchy�� �����ϴ� ���,
        //     - Level Predicate,Prior Predicate  :
        //       ( PLAN NODE ���� �� )filter�� ó���� �� �ֵ���
        //         Hierarchy Graph�� myPredicate�� ����
        //     - Prior Predicate : Hierarchy Graph�� myPredicate�� ����
        //     - Constant Predicate : CNF�� constantPredicate�� ����
        //------------------------------------------

        sHierGraph = sCNF->baseGraph[0];


        /* Level, isLeaf, Predicate�� ó�� */
        if ( ( (sPredicate->node->lflag & QTC_NODE_LEVEL_MASK )
             == QTC_NODE_LEVEL_EXIST) ||
             ( (sPredicate->node->lflag & QTC_NODE_ISLEAF_MASK )
             == QTC_NODE_ISLEAF_EXIST) )
        {
            sPredicate->next = sHierGraph->myPredicate;
            sHierGraph->myPredicate = sPredicate;
        }
        else
        {
            // Prior Predicate ó��
            if ( ( sPredicate->node->lflag & QTC_NODE_PRIOR_MASK )
                 == QTC_NODE_PRIOR_EXIST )
            {
                sPredicate->next = sHierGraph->myPredicate;
                sHierGraph->myPredicate = sPredicate;
            }
            else
            {
                // Constant Predicate ó��
                sPredicate->next = sCNF->constantPredicate;
                sCNF->constantPredicate = sPredicate;
            }
        }
    }
    else
    {
        //------------------------------------------
        // Hierarchy�� �������� �ʴ� ���,
        // Level Predicate, Prior Predicate�� �з����� ����
        // ��� constant predicate���� �з��Ͽ� ó��
        //------------------------------------------

        sPredicate->next = sCNF->constantPredicate;
        sCNF->constantPredicate = sPredicate;
    }

    return IDE_SUCCESS;
}

idBool qmoCnfMgr::checkPushPredHint( qmoCNF           * aCNF,
                                     qtcNode          * aNode,
                                     qmsJoinType        aJoinType )
{
/***********************************************************************
 *
 * Description : BUG-42968 ansi ��Ÿ�� �����϶� push_pred ��Ʈ ���뿩�θ� �Ǵ��Ѵ�.
 *
 * Implementation :
 *                  1. inner join : ��� ����
 *                  2. left outer : ON ���� ���� ������Ŷ�� �����Ҷ� ����
                                    ������ view�θ� ������ ����
 *                  3. full outer : ��� �Ұ���
 *
 ***********************************************************************/
    qtcNode          * sNode;
    qmsPushPredHints * sPushPredHint;

    // BUG-26800
    // inner join on���� join���ǿ� push_pred ��Ʈ ����
    if ( aJoinType == QMS_INNER_JOIN )
    {
        // nothing to do
    }
    else if ( aJoinType == QMS_LEFT_OUTER_JOIN )
    {
        sNode         = aNode;

        //------------------------------------------
        // ��Ʈ�� ������ view ���� �˻�
        //------------------------------------------

        for( sPushPredHint = aCNF->myQuerySet->SFWGH->hints->pushPredHint;
             sPushPredHint != NULL;
             sPushPredHint = sPushPredHint->next )
        {
            if ( qtc::dependencyEqual( &(sPushPredHint->table->table->depInfo),
                                       &(aCNF->baseGraph[1]->depInfo) ) == ID_TRUE )
            {
                break;
            }
            else
            {
                // nothing to do
            }
        }

        // ��Ʈ�߿� 1���� ��밡���ϸ� push down �Ѵ�.
        if ( sPushPredHint == NULL )
        {
            IDE_CONT( DISABLE );
        }
        else
        {
            // nothing to do
        }

        //------------------------------------------
        // ON�� �˻�
        // 1. ���� ������Ŷ���θ� �����Ǿ����� ����
        // 2. ���������� ������ ����
        //------------------------------------------

        while( sNode != NULL )
        {
            if ( qtc::dependencyEqual( &(aCNF->depInfo),
                                       &(sNode->depInfo) ) == ID_FALSE )
            {
                IDE_CONT( DISABLE );
            }
            else
            {
                // nothing to do
            }

            if ( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_CONT( DISABLE );
            }
            else
            {
                // nothing to do
            }

            sNode = (qtcNode*)sNode->node.next;
        }
    }
    else
    {
        IDE_CONT( DISABLE );
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( DISABLE );

    return ID_FALSE;
}

IDE_RC
qmoCnfMgr::pushJoinPredInView( qmoPredicate     * aPredicate,
                               qmsPushPredHints * aPushPredHint,
                               UInt               aBaseGraphCnt,
                               qmsJoinType        aJoinType,
                               qmgGraph        ** aBaseGraph,
                               idBool           * aIsPush )
{
/***********************************************************************
 *
 * Description : join predicate�� VIEW ���η� push
 *
 * Implementation : PROJ-1495
 *
 *    1. PUSH_PRED(view)�� �ش� view�� base graph�� ã�´�.
 *    2. ã������,
 *       (1) predicate
 *           . view�� base graph�� ����
 *           . predicate�� PUSH_PRED hint�� ���� �������ٴ� ���� ����
 *       (2) view�� tableRef
 *           . PUSH_PRED hint view�̸�,
 *             �̿� ���� predicate�� �������ٴ� ���� ǥ��
 *           . tableRef�� sameViewRef�� ������, �̸� ����
 *           . view tableRef�� view opt type ���� (QMO_VIEW_OPT_TYPE_PUSH)
 *           . view base graph�� dependency oring
 *
 ***********************************************************************/

    qmsPushPredHints     * sPushPredHint;
    qcDepInfo              sAndDependencies;
    qcDepInfo            * sDependencies;
    qmgGraph            ** sBaseGraph;

    UInt                   sBaseGraphCnt;
    UInt                   sCnt;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::pushJoinPredInView::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aPushPredHint != NULL );
    IDE_DASSERT( aBaseGraph != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sBaseGraph = aBaseGraph;
    sBaseGraphCnt = aBaseGraphCnt;

    //------------------------------------------
    // PUSH_PRED�� �ش� VIEW�� ã�Ƽ� join predicate�� ������.
    //------------------------------------------

    // subquery ����
    if( ( aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
        == QTC_NODE_SUBQUERY_EXIST )
    {
        *aIsPush = ID_FALSE;
    }
    else
    {
        //------------------------------------------
        // PUSH_PRED�� �ش� VIEW�� ã�Ƽ�
        // join predicate�� ������.
        //------------------------------------------

        for( sPushPredHint = aPushPredHint;
             sPushPredHint != NULL;
             sPushPredHint = sPushPredHint->next )
        {
            if ( aJoinType == QMS_LEFT_OUTER_JOIN )
            {
                if( qtc::dependencyEqual( &sBaseGraph[1]->myFrom->depInfo,
                                          &sPushPredHint->table->table->depInfo ) == ID_TRUE )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                qtc::dependencyAnd( &aPredicate->node->depInfo,
                                    &sPushPredHint->table->table->depInfo,
                                    &sAndDependencies );

                if( qtc::dependencyEqual( &sAndDependencies,
                                          &sPushPredHint->table->table->depInfo )
                    == ID_TRUE )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }

        if( sPushPredHint != NULL )
        {
            // push join predicate
            for( sCnt = 0; sCnt < sBaseGraphCnt; sCnt++ )
            {
                sDependencies = & sBaseGraph[sCnt]->myFrom->depInfo;

                if( qtc::dependencyEqual( sDependencies,
                                          & sPushPredHint->table->table->depInfo )
                    == ID_TRUE )
                {
                    aPredicate->next = sBaseGraph[sCnt]->myPredicate;
                    sBaseGraph[sCnt]->myPredicate = aPredicate;

                    // predicate flag ����
                    aPredicate->flag &= ~QMO_PRED_PUSH_PRED_HINT_MASK;
                    aPredicate->flag |= QMO_PRED_PUSH_PRED_HINT_TRUE;

                    // sameViewRef�� �����ϴ� ���, �̸� �����Ѵ�.
                    if( sBaseGraph[sCnt]->myFrom->tableRef->sameViewRef
                        != NULL )
                    {
                        sBaseGraph[sCnt]->myFrom->tableRef->sameViewRef = NULL;
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    // tableRef flag ����
                    sBaseGraph[sCnt]->myFrom->tableRef->flag
                        &= ~ QMS_TABLE_REF_PUSH_PRED_HINT_MASK;
                    sBaseGraph[sCnt]->myFrom->tableRef->flag
                        |= QMS_TABLE_REF_PUSH_PRED_HINT_TRUE;

                    if ( aJoinType == QMS_INNER_JOIN )
                    {
                        sBaseGraph[sCnt]->myFrom->tableRef->flag
                            &= ~QMS_TABLE_REF_PUSH_PRED_HINT_INNER_JOIN_MASK;
                        sBaseGraph[sCnt]->myFrom->tableRef->flag
                            |= QMS_TABLE_REF_PUSH_PRED_HINT_INNER_JOIN_TRUE;
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    // tableRef viewOpt flag ����
                    sBaseGraph[sCnt]->myFrom->tableRef->viewOptType
                        = QMO_VIEW_OPT_TYPE_PUSH;

                    // dependency ����
                    IDE_TEST( qtc::dependencyOr( &(sBaseGraph[sCnt]->depInfo),
                                                 & aPredicate->node->depInfo,
                                                 &(sBaseGraph[sCnt]->depInfo) )
                              != IDE_SUCCESS );

                    sPushPredHint->table->table->tableRef->view->disableLeftStore = ID_TRUE;

                    *aIsPush = ID_TRUE;

                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }
        else
        {
            *aIsPush = ID_FALSE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::addOneTblPredOnJoinType( qmoCNF       * aCNF,
                                    qmgGraph     * aBaseGraph,
                                    qmoPredicate * aPredicate,
                                    qmsJoinType    aJoinType,
                                    idBool         aIsLeft )
{
/***********************************************************************
 *
 * Description : Join Type�� ���� One Table Predicate�� ó��
 *
 * Implementation :
 *    (1) INNER_JOIN
 *        left, right ������� push selection ���� ����
 *        one table predicate�� �ش� graph�� myPredicate�� ������
 *    (2) FULL_OUTER_JOIN
 *        left, right ������� push selection ������ �� ����
 *        onConditionCNF->oneTablePredicate�� ����
 *    (3) LEFT_OUTER_JOIN
 *        - left : push selection ������ �� ����,
 *                 ���� onConditionCNF->oneTablePredicate�� ����
 *        - right : push selection ���� ����
 *                  one table predicate�� �ش� graph�� myPredicate�� ������
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoCNF       * sCNF;
    qmgGraph     * sBaseGraph;
    qmoPushApplicableType sPushApplicable;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::addOneTblPredOnJoinType::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aBaseGraph != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sPredicate = aPredicate;
    sBaseGraph = aBaseGraph;
    sCNF = aCNF;

    //------------------------------------------
    // To Fix BUG-10806
    // Push Selection ���� Predicate ���� �˻�
    //------------------------------------------

    sPushApplicable = isPushApplicable(aPredicate, aJoinType);

    // BUG-44805 QMO_PUSH_APPLICABLE_LEFT �� ����ؾ� �Ѵ�.
    // aIsLeft ������ ���� left �δ� Push Selection�� �ȵ�
    if ( sPushApplicable != QMO_PUSH_APPLICABLE_FALSE )
    {
        switch( aJoinType )
        {
            case QMS_INNER_JOIN :
                // push selection ����
                sPredicate->next = sBaseGraph->myPredicate;
                sBaseGraph->myPredicate = sPredicate;
                break;

            case QMS_LEFT_OUTER_JOIN :
                if ( aIsLeft == ID_TRUE )
                {
                    // left conceptual table�� ���� predicate
                    // push selection �� �� ����
                    sPredicate->next = sCNF->oneTablePredicate;
                    sCNF->oneTablePredicate = sPredicate;
                }
                else
                {
                    // right conceptual table�� ���� predicate
                    // push selection ����
                    sPredicate->next = sBaseGraph->myPredicate;
                    sBaseGraph->myPredicate = sPredicate;
                }
                break;

            case QMS_FULL_OUTER_JOIN :
                // push selection �� �� ����
                sPredicate->next = sCNF->oneTablePredicate;
                sCNF->oneTablePredicate = sPredicate;
                break;
            default:
                IDE_FT_ASSERT( 0 );
                break;
        }
    }
    else
    {
        // push selection �� �� ����
        sPredicate->next = sCNF->oneTablePredicate;
        sCNF->oneTablePredicate = sPredicate;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoCnfMgr::pushSelectionOnJoinType( qcStatement   * aStatement,
                                    qmoPredicate ** aUpperPred,
                                    qmgGraph     ** aBaseGraph,
                                    qcDepInfo     * aFromDependencies,
                                    qmsJoinType     aJoinType )
{
/***********************************************************************
 *
 * Description : Join Type�� ���� push selection ����ȭ ����
 *
 * Implementation :
 *    upper predicate( where������ ������ predicate ) �� IS NULL, IS NOT NULL
 *    predicate�� �����ϰ� �Ʒ� ������ �����Ѵ�.
 *    (1) INNER_JOIN
 *        upper predicate���� �ش� graph�� myPredicate�� ������
 *    (2) FULL_OUTER_JOIN
 *        upper predicate���� one table predicate�� �����Ͽ�
 *        �ش� graph�� myPredicate�� ������
 *    (3) LEFT_OUTER_JOIN
 *        - left  : upper predicate���� �ش� graph�� myPredicate�� �����Ͽ� ������
 *        - right : upper predicate���� one table predicate�� �����Ͽ�
 *                  �ش� graph�� myPredicate�� ������
 *
 *    Ex)
 *       < ������ Predicate���� ������ Predicate�� �и��Ͽ� ������ Graph��
 *         �޾��ִ� ��� >
 *
 *        [p2] Predicate�� �ش� Join Graph�� �����ϴ� ���
 *
 *                             sPrevUpperPred
 *                               |
 *                               |       sAddPred
 *                               |       sCurUpperPred
 *                               |        |
 *                               V        V
 *              [UpperPred] --> [p1] --> [p2] --> [p3]
 *
 *        [�ش� ���� Graph] --> [q1] --> [q2] --> [q3]
 *
 *
 *         ==>   UpperPred���� ������ ����, sCurUpperPred�� �������� ����
 *
 *                             sPrevJoinPred
 *                               |
 *                               |     sAddPred
 *                               |       |
 *                               |       V
 *                               |      [p2]  sCurUpperPred
 *                               V         \   |
 *                                          V  V
 *              [UpperPred] --> [p1] -------> [p3]
 *
 *        [�ش� ���� Graph] --> [q1] --> [q2] --> [q3]
 *
 *
 *         ==>  [p2] Predicate ����
 *
 *                             sPrevUpperPred
 *                               |
 *                               |       sCurUpperPred
 *                               |        |
 *                               V        V
 *              [UpperPred] --> [p1] --> [p3]
 *
 *                           +[p2]
 *                          /      \
 *                         /        V
 *        [�ش� ���� Graph] --X--> [q1] --> [q2] --> [q3]
 *
 *
 *         ==>  [p2] Predicate ���� ��
 *
 *                             sPrevUpperPred
 *                               |
 *                               |       sCurUpperPred
 *                               |        |
 *                               V        V
 *              [UpperPred] --> [p1] --> [p3]
 *
 *        [�ش� ���� Graph] --> [p2]-->[q1] --> [q2] --> [q3]
 *
 *
 *        < ������ Predicate���� ������ Predicate�� �����Ͽ� ������ Graph��
 *         �޾��ִ� ��� >
 *
 *         [p2] Predicate�� �ش� Join Graph�� �����ϴ� ���
 *
 *                                  sPrevUpperPred
 *                                        |
 *                                        |    sCurUpperPred
 *                                        |        |
 *                                        V        V
 *              [UpperPred] --> [p1] --> [p2] --> [p3]
 *
 *        [�ش� ���� Graph] -X-> [q1] --> [q2] --> [q3]
 *                        \    +
 *                         V  /
 *                         [p2]
 *                          +
 *                          |
 *                        sAddNode ( ������ Node )
 *
 ***********************************************************************/

    qmgGraph     ** sBaseGraph;
    qmoPredicate  * sCurUpperPred;
    qmoPredicate  * sPrevUpperPred;
    qmoPredicate  * sAddPred;
    UInt            sPredPos;

    //------------------------------------------------------
    //  aUpperPred : �������� ������ Predicate
    //  sCurUpperPred : �������� ������ predicate�� Traverse ����
    //  sPrevUpperPred : Push Selection �� �� ���� Predicate�� ���� ����
    //  sAddPred : Push Selection ��� Predicate�� ���� ����
    //  sPredPos : Base Graph���� Predicate�� push selection�� Graph�� position
    //------------------------------------------------------

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::pushSelectionOnJoinType::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aUpperPred != NULL );
    IDE_DASSERT( aBaseGraph != NULL );
    IDE_DASSERT( aFromDependencies != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sCurUpperPred     = *aUpperPred;
    sPrevUpperPred    = NULL;
    sBaseGraph        = aBaseGraph;
    sPredPos          = ID_UINT_MAX;

    while ( sCurUpperPred != NULL )
    {
        //------------------------------------------
        // Push Selection ������ Predicate�� ���,
        // Push Selection ��� graph�� ��ġ�� ã�´�.
        // ( Push Selection �� �� ���� ���, ID_UINT_MAX ���� ��ȯ )
        //------------------------------------------

        IDE_TEST( getPushPredPosition( sCurUpperPred,
                                       sBaseGraph,
                                       aFromDependencies,
                                       aJoinType,
                                       & sPredPos )
                  != IDE_SUCCESS );

        //------------------------------------------
        // �ش� graph�� Predicate ����
        //------------------------------------------

        if ( sPredPos != ID_UINT_MAX )
        {
            switch( aJoinType )
            {
                case QMS_INNER_JOIN :
                    // Upper Predicate���� ������ ����
                    if ( sPrevUpperPred == NULL )
                    {
                        *aUpperPred = sCurUpperPred->next;
                    }
                    else
                    {
                        sPrevUpperPred->next = sCurUpperPred->next;
                    }

                    sAddPred = sCurUpperPred;

                    // �������� ����
                    sCurUpperPred = sCurUpperPred->next;

                    // �ش� graph�� myPredicate�� ����
                    sAddPred->next = sBaseGraph[sPredPos]->myPredicate;
                    sBaseGraph[sPredPos]->myPredicate = sAddPred;

                    break;
                case QMS_LEFT_OUTER_JOIN :
                    if ( sPredPos == 0 )
                    {
                        // Upper Predicate���� ������ ����
                        if ( sPrevUpperPred == NULL )
                        {
                            *aUpperPred = sCurUpperPred->next;
                        }
                        else
                        {
                            sPrevUpperPred->next = sCurUpperPred->next;
                        }

                        // BUG-40682 left outer join �� ��ø���� ���ɶ�
                        // filter �� �߸��� ��ġ�� ���� �� �� �ִ�.
                        // predicate �� �������ش�.
                        IDE_TEST( qmoPred::copyOnePredicate( QC_QMP_MEM(aStatement),
                                                             sCurUpperPred,
                                                             & sAddPred )
                                  != IDE_SUCCESS );

                        // �������� ����
                        sCurUpperPred = sCurUpperPred->next;

                        // �ش� graph�� myPredicate�� ����
                        sAddPred->next = sBaseGraph[sPredPos]->myPredicate;
                        sBaseGraph[sPredPos]->myPredicate = sAddPred;
                    }
                    else
                    {
                        // Predicate ����
                        IDE_TEST( qmoPred::copyOnePredicate( QC_QMP_MEM(aStatement),
                                                             sCurUpperPred,
                                                             & sAddPred )
                                  != IDE_SUCCESS );


                        // ������ Predicate�� graph�� myPredicate�� ����
                        sAddPred->next = sBaseGraph[sPredPos]->myPredicate;
                        sBaseGraph[sPredPos]->myPredicate = sAddPred;

                        // �������� ����
                        sPrevUpperPred = sCurUpperPred;
                        sCurUpperPred = sCurUpperPred->next;
                    }

                    break;
                case QMS_FULL_OUTER_JOIN :
                    // Predicate ����
                    IDE_TEST( qmoPred::copyOnePredicate( QC_QMP_MEM(aStatement),
                                                         sCurUpperPred,
                                                         & sAddPred )
                              != IDE_SUCCESS );

                    // ������ Predicate�� �ش� graph�� myPredicate�� ����
                    sAddPred->next = sBaseGraph[sPredPos]->myPredicate;
                    sBaseGraph[sPredPos]->myPredicate = sAddPred;

                    // �������� ����
                    sPrevUpperPred = sCurUpperPred;
                    sCurUpperPred = sCurUpperPred->next;
                    break;
                default :
                    IDE_FT_ASSERT( 0 );
                    break;
            }

        }
        else
        {
            // subquery�� �����ϴ� Predicate�̰ų�,
            // one table predicate�� �ƴ� ���
            sPrevUpperPred = sCurUpperPred;
            sCurUpperPred = sCurUpperPred->next;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::addConstPred4ConnectBy( qmoCNF       * aCNF,
                                   qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description : Connect By�� Level/Prior/Constant Predicate�� �з�
 *
 * Implementation :
 *    (1) Connect by Rownum Predicate�� ��� : qmoCNF::connectByRownumPredicate�� ����
 *    (2) Level Predicate�� ��� : qmoCNF::levelPredicate�� ����
 *    (3) Prior Predicate�� ��� : qmoCNF::oneTablePredicate�� ����
 *    (4) Constant Predicate�� ��� : qmoCNF::constantPredicate�� ����
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoCNF       * sCNF;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::addConstPred4ConnectBy::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCNF != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sPredicate = aPredicate;
    sCNF = aCNF;

    /* BUG-39434 The connect by need rownum pseudo column. */
    if ( ( sPredicate->node->lflag & QTC_NODE_ROWNUM_MASK )
         == QTC_NODE_ROWNUM_EXIST )
    {
        sPredicate->next = sCNF->connectByRownumPred;
        sCNF->connectByRownumPred = sPredicate;
    }
    else if ( ( sPredicate->node->lflag & QTC_NODE_LEVEL_MASK )
         == QTC_NODE_LEVEL_EXIST )
    {
        sPredicate->next = sCNF->levelPredicate;
        sCNF->levelPredicate = sPredicate;
    }
    else
    {
        // To Fix PR-9050
        // Prior Node�� ���� ���θ� �˻��ϱ� ���ؼ���
        // qtcNode->flag�� �˻��Ͽ��� ��.
        // Prior Predicate ó��
        if ( ( sPredicate->node->lflag & QTC_NODE_PRIOR_MASK )
             == QTC_NODE_PRIOR_EXIST )
        {
            sPredicate->next = sCNF->oneTablePredicate;
            sCNF->oneTablePredicate = sPredicate;
        }
        else
        {
            // Constant Predicate ó��
            sPredicate->next = sCNF->constantPredicate;
            sCNF->constantPredicate = sPredicate;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoCnfMgr::joinOrdering( qcStatement * aStatement,
                         qmoCNF      * aCNF )
{
/***********************************************************************
 *
 * Description : Join Order�� ����
 *
 * Implemenation :
 *    (1) Join Group�� �з�
 *    (2) �� Join Group �� Join Relation�� ǥ��
 *    (3) �� Join Group �� joinPredicate���� selectivity ���
 *    (4) �� Join Group �� Join Order�� ����
 *       ���󰡸鼭 base graph�� �ΰ� �̻��̸� ������ ����
 *        A. JoinGroup �� qmgJoin�� ���� �� �ʱ�ȭ
 *        B. Join Order ���� �� qmgJoin�� ����ȭ
 *    (5) Join Group�� Join Order�� ����
 *
 ***********************************************************************/

    qmoCNF       * sCNF;
    UInt           sJoinGroupCnt;
    qmoJoinGroup * sJoinGroup;
    qmgJOIN      * sResultJoinGraph;
    qmgGraph     * sLeftGraph;
    qmsQuerySet  * sQuerySet;
    UInt           i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::joinOrdering::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF             = aCNF;
    sResultJoinGraph = NULL;

    // fix BUG-33880
    sQuerySet        = sCNF->myQuerySet;

    // fix BUG-11166
    // ordered hint����� join group�� predicate������ ���� �ʱ� ������
    // JoinPredicate�� selectivity��
    // join grouping�ϱ� ���� ����� ���ƾ� ��
    if ( sCNF->joinPredicate != NULL )
    {
        //------------------------------------------
        // joinPredicate���� selectivity ���
        //------------------------------------------

        IDE_TEST( qmoSelectivity::setMySelectivity4Join( aStatement,
                                                         NULL,
                                                         sCNF->joinPredicate,
                                                         ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }


    //------------------------------------------
    // Join Grouping�� �з�
    //------------------------------------------

    // BUG-42447 leading hint support
    if( sQuerySet->SFWGH->hints->leading != NULL )
    {
        makeLeadingGroup( sCNF,
                          sQuerySet->SFWGH->hints->leading );
    }
    else
    {
        // Nothing to do
    }

    IDE_TEST( joinGrouping( sCNF ) != IDE_SUCCESS );

    sJoinGroup       = sCNF->joinGroup;
    sJoinGroupCnt    = sCNF->joinGroupCnt;

    //------------------------------------------
    // �� Join Group�� ó��
    //    (1) JoinRelation�� ǥ��
    //    (2) Join Predicate�� selectivity ���
    //    (3) Join Order�� ����
    //------------------------------------------

    for ( i = 0; i < sJoinGroupCnt; i++ )
    {
        //------------------------------------------
        // �ش� Join Group�� JoinRelation�� ǥ��
        //------------------------------------------

        IDE_TEST( joinRelationing( aStatement, & sJoinGroup[i] )
                  != IDE_SUCCESS );
        sJoinGroup[i].mIsOnlyNL = aCNF->mIsOnlyNL;
        //------------------------------------------
        // �ش� Join Group �� Join Order�� ����
        //------------------------------------------

        if ( sJoinGroup[i].baseGraphCnt > 1 )
        {
            IDE_TEST( joinOrderingInJoinGroup( aStatement, & sJoinGroup[i] )
                      != IDE_SUCCESS );
        }
        else
        {
            // To Fix PR-7993
            sJoinGroup[i].topGraph = sJoinGroup[i].baseGraph[0];
        }
    }


    //------------------------------------------
    // �� Group �� Join Order�� ����
    // base graph ������ ó�� : base graph ������ SFWGH::from�� ������ ����
    //                          ���� Ordered ������ ����
    //------------------------------------------

    // PROJ-1495
    // PUSH_PRED hint�� ����� ���, join group ���ġ
    if( sQuerySet->setOp == QMS_NONE )
    {
        // hint�� SFWGH ������ �� �� �ִ�.

        if( sQuerySet->SFWGH->hints->pushPredHint != NULL )
        {
            IDE_TEST( relocateJoinGroup4PushPredHint( aStatement,
                                                      sQuerySet->SFWGH->hints->pushPredHint,
                                                      & sJoinGroup,
                                                      sJoinGroupCnt )
                      != IDE_SUCCESS );
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

    for ( i = 0; i < sJoinGroupCnt; i++ )
    {
        if ( i == 0 )
        {
            sLeftGraph = sJoinGroup[0].topGraph;

            // To Fix PR-7989
            sResultJoinGraph = (qmgJOIN*) sLeftGraph;
        }
        else
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgJOIN ),
                                                     (void**)&sResultJoinGraph )
                != IDE_SUCCESS );

            IDE_TEST( qmgJoin::init( aStatement,
                                     sLeftGraph,
                                     sJoinGroup[i].topGraph,
                                     & sResultJoinGraph->graph,
                                     ID_FALSE )
                      != IDE_SUCCESS );

            IDE_TEST( connectJoinPredToJoinGraph( aStatement,
                                                  & sCNF->joinPredicate,
                                                  & sResultJoinGraph->graph )
                != IDE_SUCCESS );

            // To Fix PR-8266
            // Cartesian Product���� ��� ������ �����ؾ� ��.
            // �� qmgJoin �� joinOrderFactor, joinSize ���
            IDE_TEST( qmoSelectivity::setJoinOrderFactor( aStatement,
                                                          & sResultJoinGraph->graph,
                                                          sResultJoinGraph->graph.myPredicate,
                                                          & sResultJoinGraph->joinOrderFactor,
                                                          & sResultJoinGraph->joinSize )
                      != IDE_SUCCESS );

            if ( sCNF->mIsOnlyNL == ID_TRUE )
            {
                sResultJoinGraph->graph.flag &= ~QMG_JOIN_ONLY_NL_MASK;
                sResultJoinGraph->graph.flag |= QMG_JOIN_ONLY_NL_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( sResultJoinGraph->graph.optimize( aStatement,
                                                        & sResultJoinGraph->graph )
                      != IDE_SUCCESS );

            sLeftGraph = (qmgGraph*)sResultJoinGraph;
        }

    }

    sCNF->myGraph = & sResultJoinGraph->graph;

    //------------------------------------------
    // BUG-34295 Join ordering ANSI style query
    //     outerJoinGraph �� myGraph �� �����Ѵ�.
    //------------------------------------------
    if( sCNF->outerJoinGraph != NULL )
    {
        IDE_TEST( qmoAnsiJoinOrder::mergeOuterJoinGraph2myGraph( aCNF ) != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmoCnfMgr::makeLeadingGroup( qmoCNF         * aCNF,
                                  qmsLeadingHints* aLeadingHints )
{
/***********************************************************************
 *
 * Description : BUG-42447 leading hint support
 *
 * Implemenation : leading ��Ʈ�� ���� ���̺��� ������� ���� �׷��� �����Ѵ�.
 *
 ***********************************************************************/

    qmsHintTables    * sLeadingTables;
    qmgGraph        ** sBaseGraph;
    qmoJoinGroup     * sJoinGroup;
    UInt               sBaseGraphCnt;
    UInt               sJoinGroupCnt;
    UInt               i;

    sBaseGraph      = aCNF->baseGraph;
    sJoinGroup      = aCNF->joinGroup;
    sBaseGraphCnt   = aCNF->graphCnt4BaseTable;
    sJoinGroupCnt   = aCNF->joinGroupCnt;

    for ( sLeadingTables = aLeadingHints->mLeadingTables;
          sLeadingTables != NULL;
          sLeadingTables = sLeadingTables->next )
    {
        for( i = 0; i < sBaseGraphCnt; i++ )
        {
            // ���� ���̺��� ���������
            if ( sLeadingTables->table == NULL )
            {
                continue;
            }

            // �̹� ���� �׷��� ������ ���̺�
            if ( (sBaseGraph[i]->flag & QMG_INCLUDE_JOIN_GROUP_MASK) ==
                 QMG_INCLUDE_JOIN_GROUP_TRUE )
            {
                continue;
            }

            if ( qtc::dependencyContains( &(sBaseGraph[i]->depInfo),
                                          &(sLeadingTables->table->depInfo) ) == ID_TRUE )
            {
                break;
            }
        }

        if ( i < sBaseGraphCnt )
        {
            sBaseGraph[i]->flag &= ~QMG_INCLUDE_JOIN_GROUP_MASK;
            sBaseGraph[i]->flag |= QMG_INCLUDE_JOIN_GROUP_TRUE;

            sJoinGroup[sJoinGroupCnt].baseGraph[0]  = sBaseGraph[i];
            sJoinGroup[sJoinGroupCnt].baseGraphCnt  = 1;
            sJoinGroup[sJoinGroupCnt].depInfo       = sBaseGraph[i]->depInfo;

            sJoinGroupCnt++;

            IDE_DASSERT( sJoinGroupCnt <= aCNF->maxJoinGroupCnt );
        }
        else
        {
            // Nothing to do
        }
    }

    aCNF->joinGroupCnt = sJoinGroupCnt;
}

IDE_RC
qmoCnfMgr::joinGrouping( qmoCNF      * aCNF )
{
/***********************************************************************
 *
 * Description : Join Group�� �з�
 *
 * Implemenation :
 *    (1) Ordered Hint�� �����鼭, Join Predicate�� �����ϴ� ���
 *        A. Join Predicate�� �̿��� Join Grouping
 *        B. Join Group�� ���ϴ� base graph ����
 *    (2) Join Group�� ���Ե��� ���� base graph���� �ϳ��� Join Group����
 *        ����
 *        < Join Group�� ���Ե��� ���� base graph >
 *        A. Ordered Hint�� �����ϴ� ���, ��� base graph
 *        B. Join Predicate�� ���� ���, ��� base graph
 *        C. Join Predicate�� ����������, �ش� base graph�� ������
 *           Join Predicate�� ���� Join Grouping ���� ���� base graph
 *
 ***********************************************************************/

    qmoCNF           * sCNF;
    qmoJoinOrderType   sJoinOrderHint;
    UInt               sJoinGroupCnt;
    qmoJoinGroup     * sJoinGroup;
    qmoJoinGroup     * sCurJoinGroup;
    UInt               sBaseGraphCnt;
    qmgGraph        ** sBaseGraph;
    qmoPredicate     * sJoinPred;
    qmoPredicate     * sLateralJoinPred;
    UInt               i;     // for loop

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::joinGrouping::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF             = aCNF;
    sJoinGroup       = sCNF->joinGroup;
    sJoinOrderHint   = sCNF->myQuerySet->SFWGH->hints->joinOrderType;
    sBaseGraphCnt    = sCNF->graphCnt4BaseTable;
    sBaseGraph       = sCNF->baseGraph;
    sJoinGroupCnt    = sCNF->joinGroupCnt;
    sLateralJoinPred = NULL;
    sJoinPred        = NULL;


    if ( sJoinOrderHint == QMO_JOIN_ORDER_TYPE_OPTIMIZED )
    {
        //------------------------------------------
        // Join Order Hint �� �������� �ʴ� ����� ó��
        //------------------------------------------

        // PROJ-2418
        // Lateral View�� Join Predicate�� Grouping���� ������� �ʴ´�.
        // ��� ���ܽ��״ٰ�, Grouping ���� �ٽ� �����Ѵ�.
        IDE_TEST( discardLateralViewJoinPred( sCNF, & sLateralJoinPred )
                  != IDE_SUCCESS );

        if ( sCNF->joinPredicate != NULL )
        {
            //------------------------------------------
            // Join Predicate���� �̿��Ͽ� Join Grouping�� �����ϰ�,
            // �ش� Join Group�� JoinPredicate�� ����
            //------------------------------------------

            IDE_TEST( joinGroupingWithJoinPred( sJoinGroup,
                                                sCNF,
                                                & sJoinGroupCnt )
                      != IDE_SUCCESS );

            //------------------------------------------
            // JoinGroup�� base graph ����
            //    qmoCNF�� baseGraph�� �� �ش� Join Group�� ���ϴ� baseGraph��
            //    Join Group�� baseGraph�� ������
            //------------------------------------------

            for ( i = 0; i < sJoinGroupCnt; i++ )
            {
                IDE_TEST( connectBaseGraphToJoinGroup( & sJoinGroup[i],
                                                       sBaseGraph,
                                                       sBaseGraphCnt )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            //------------------------------------------
            // Join Predicate�� ���� ����� ó��
            //    �� base graph�� �ϳ��� Join Group���� ����
            //------------------------------------------
        }

        // �����ߴ� Lateral View�� Join Predicate�� �ٽ� ����
        if ( sLateralJoinPred != NULL )
        {
            for ( sJoinPred = sLateralJoinPred; 
                  sJoinPred->next != NULL; 
                  sJoinPred = sJoinPred->next ) ;

            sJoinPred->next = sCNF->joinPredicate;
            sCNF->joinPredicate = sLateralJoinPred;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        //------------------------------------------
        // Join Order Hint �� �����ϴ� ����� ó��
        //    �� base graph�� �ϳ��� Join Group���� ����
        //------------------------------------------
    }

    //------------------------------------------
    // Join Grouping ���� ���� base graph�� �ϳ��� Join Group���� �з�
    //    (1) Join Predicate�� ���� ���
    //    (2) Join Predicate�� ����������, �ش� BaseGraph��
    //        ������ Join Predicate�� ���� Join Grouping ���� ���� ���
    //    (3) Join Order Hint �� �����ϴ� ���
    //------------------------------------------

    for ( i = 0; i < sBaseGraphCnt; i++ )
    {

        if ( ( sBaseGraph[i]->flag & QMG_INCLUDE_JOIN_GROUP_MASK )
             == QMG_INCLUDE_JOIN_GROUP_FALSE )
        {
            sBaseGraph[i]->flag &= ~QMG_INCLUDE_JOIN_GROUP_MASK;
            sBaseGraph[i]->flag |= QMG_INCLUDE_JOIN_GROUP_TRUE;

            sCurJoinGroup = & sJoinGroup[sJoinGroupCnt++];
            sCurJoinGroup->baseGraph[0] = sBaseGraph[i];
            sCurJoinGroup->baseGraphCnt = 1;
            qtc::dependencySetWithDep( & sCurJoinGroup->depInfo,
                                       & sBaseGraph[i]->depInfo );
        }
        else
        {
            // nothing to do
        }
    }

    // ���� Join Group ���� ����
    sCNF->joinGroupCnt = sJoinGroupCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::joinRelationing( qcStatement      * aStatement,
                            qmoJoinGroup     * aJoinGroup )
{
/***********************************************************************
 *
 * Description : �� Join Group ���� Join Relation ���� ����
 *
 * Implementation :
 *    (1) �� ���� conceptual table�θ� ������ Join Predicate��
 *        Join ������ ����
 *    (2) operand�� �� ���� conceptual table �θ� ������ Join ������ ����
 *
 ***********************************************************************/

    qmoJoinGroup     * sJoinGroup;
    qmoPredicate     * sCurPred;
    UInt               sBaseGraphCnt;
    qmgGraph        ** sBaseGraph;
    qtcNode          * sNode;
    qmoJoinRelation  * sJoinRelationList;
    UInt               sJoinRelationCnt;
    idBool             sIsTwoConceptTblNode;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::joinRelationing::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aJoinGroup != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinGroup           = aJoinGroup;
    sJoinRelationList    = NULL;
    sBaseGraph           = sJoinGroup->baseGraph;
    sBaseGraphCnt        = sJoinGroup->baseGraphCnt;
    sJoinRelationCnt     = 0;
    sIsTwoConceptTblNode = ID_FALSE;

    //------------------------------------------
    // join relation ���� ����
    //------------------------------------------

    for ( sCurPred = aJoinGroup->joinPredicate;
          sCurPred != NULL;
          sCurPred = sCurPred->next )
    {
        if ( ( sCurPred->node->node.lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_OR )
        {
            sNode = (qtcNode *)sCurPred->node->node.arguments;
        }
        else
        {
            sNode = sCurPred->node;
        }

        if( ( sNode->lflag & QTC_NODE_JOIN_TYPE_MASK ) != QTC_NODE_JOIN_TYPE_INNER )
        {
            // PROJ-1718 Subquery unnesting
            // Semi/anti join�� ��� ������ join relation ���� �Լ��� �̿��Ѵ�.

            IDE_TEST( makeSemiAntiJoinRelationList( aStatement,
                                                    sNode,
                                                    & sJoinRelationList,
                                                    & sJoinRelationCnt )
                      != IDE_SUCCESS );
        }
        else
        {
            // BUG-37963 inner join �϶��� or ��嵵 üũ�ؾ� �Ѵ�.
            // ����� OR ��带 ��ó üũ�ϸ� semi, anti ������ ������ �ʴ´�.
            if( sCurPred->node != sNode )
            {
                IDE_TEST( isTwoConceptTblNode( sCurPred->node,
                                               sBaseGraph,
                                               sBaseGraphCnt,
                                               & sIsTwoConceptTblNode )
                          != IDE_SUCCESS );

                if ( sIsTwoConceptTblNode == ID_TRUE )
                {
                    IDE_TEST( makeJoinRelationList( aStatement,
                                                    & sCurPred->node->depInfo,
                                                    & sJoinRelationList,
                                                    & sJoinRelationCnt )
                              != IDE_SUCCESS );

                    continue;
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

            // �� ���� conceptual table�θ� ������ predicate���� �˻�
            IDE_TEST( isTwoConceptTblNode( sNode,
                                           sBaseGraph,
                                           sBaseGraphCnt,
                                           & sIsTwoConceptTblNode )
                         != IDE_SUCCESS );

            if ( sIsTwoConceptTblNode == ID_TRUE )
            {
                //------------------------------------------
                // �� ���� conceptual table�θ� ������ Join Predicate
                //------------------------------------------

                IDE_TEST( makeJoinRelationList( aStatement,
                                                & sNode->depInfo,
                                                & sJoinRelationList,
                                                & sJoinRelationCnt )
                          != IDE_SUCCESS );
            }
            else
            {
                sNode = (qtcNode *)sNode->node.arguments;

                if ( sNode->node.next == NULL )
                {
                    sNode = (qtcNode *)sNode->node.arguments;
                }
                else
                {
                    sNode = NULL;
                }

                // To Fix BUG-8789
                // �� ���� conceptual table�θ� �����Ǿ����� �˻�
                for ( sNode = (qtcNode *) sNode;
                      sNode != NULL;
                      sNode = (qtcNode *) sNode->node.next )
                {
                    IDE_TEST( isTwoConceptTblNode( sNode,
                                                   sBaseGraph,
                                                   sBaseGraphCnt,
                                                   & sIsTwoConceptTblNode )
                              != IDE_SUCCESS );

                    if ( sIsTwoConceptTblNode == ID_TRUE )
                    {
                        //------------------------------------------
                        // Operand�� �� ���� conceptual table�θ� ������ ���
                        // ex ) T1.I1 + T2.I1 = T3.I1
                        //------------------------------------------

                        IDE_TEST( makeJoinRelationList( aStatement,
                                                        & sNode->depInfo,
                                                        & sJoinRelationList,
                                                        & sJoinRelationCnt )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            }
        }
    }

    // PROJ-1718 Subquery Unnesting
    IDE_TEST( makeCrossJoinRelationList( aStatement,
                                         aJoinGroup,
                                         & sJoinRelationList,
                                         & sJoinRelationCnt )
              != IDE_SUCCESS );

    // Join Group�� Join Relation ����
    sJoinGroup->joinRelation = sJoinRelationList;
    sJoinGroup->joinRelationCnt = sJoinRelationCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::joinOrderingInJoinGroup( qcStatement  * aStatement,
                                    qmoJoinGroup * aJoinGroup )
{
/***********************************************************************
 *
 * Description : Join Group �������� Join Order ����
 *               Join ordering �� ����� ��� inner join �̴�
 *
 * Implementation :
 *    (1) Join Graph�� ���� �� �ʱ�ȭ
 *    (2) ���� Join Graph ����
 *    (3) Join Graph���� join Ordering ����
 *
 ***********************************************************************/

    UInt               sJoinRelationCnt;
    qmoJoinRelation  * sJoinRelation;
    UInt               sJoinGraphCnt;
    qmgJOIN          * sJoinGraph;
    qmgJOIN          * sSelectedJoinGraph = NULL;
    UInt               sBaseGraphCnt;
    qmgGraph        ** sBaseGraph;
    qmgGraph         * sLeftGraph;
    qmgGraph         * sRightGraph;
    idBool             sSelectedJoinGraphIsTop = ID_FALSE;
    UInt               sTop;
    UInt               sEnd;
    UInt               i;

    SDouble            sCurJoinOrderFactor;
    SDouble            sCurJoinSize;
    SDouble            sJoinOrderFactor         = 0.0;
    SDouble            sJoinSize                = 0.0;

    SDouble            sFirstRowsFactor;
    SDouble            sJoinGroupSelectivity;
    SDouble            sJoinGroupOutputRecord;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::joinOrderingInJoinGroup::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aJoinGroup != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sBaseGraphCnt    = aJoinGroup->baseGraphCnt;
    sBaseGraph       = aJoinGroup->baseGraph;
    sJoinRelation    = aJoinGroup->joinRelation;
    sJoinRelationCnt = aJoinGroup->joinRelationCnt;

    // Join Graph ����
    if ( sJoinRelationCnt > sBaseGraphCnt - 1 )
    {
        sJoinGraphCnt = sJoinRelationCnt;
    }
    else
    {
        sJoinGraphCnt = sBaseGraphCnt - 1;
    }

    // sTop, sEnd ����
    if ( sJoinRelation != NULL )
    {
        sEnd = sJoinRelationCnt;
    }
    else
    {
        sEnd = 1;
    }

    //------------------------------------------
    // Join Graph ���� �� �ʱ�ȭ
    //------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgJOIN ) * sJoinGraphCnt,
                                             (void**) & sJoinGraph )
              != IDE_SUCCESS );

    if ( sJoinRelation != NULL )
    {
        i = 0;

        // Join Relation�� �����Ǵ� qmgJoin �ʱ�ȭ
        while ( sJoinRelation != NULL )
        {
            IDE_TEST( getTwoRelatedGraph( & sJoinRelation->depInfo,
                                          sBaseGraph,
                                          sBaseGraphCnt,
                                          & sLeftGraph,
                                          & sRightGraph )
                      != IDE_SUCCESS );

            IDE_TEST( initJoinGraph( aStatement,
                                     sJoinRelation,
                                     sLeftGraph,
                                     sRightGraph,
                                     &sJoinGraph[i].graph,
                                     ID_FALSE )
                      != IDE_SUCCESS );

            sJoinRelation = sJoinRelation->next;

            // To Fix PR-7994
            i++;
        }
    }
    else
    {
        // Join Relation�� �������� �ʴ� ���, ������ qmgJoin �ʱ�ȭ
        IDE_TEST( qmgJoin::init( aStatement,
                                 sBaseGraph[0],
                                 sBaseGraph[1],
                                 & sJoinGraph[0].graph,
                                 ID_FALSE )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // FirstRowsFactor
    //------------------------------------------

    if( (sBaseGraph[0])->myQuerySet->SFWGH->hints->optGoalType
         == QMO_OPT_GOAL_TYPE_FIRST_ROWS_N )
    {
        // baseGraph Selectivity ����
        for( sJoinGroupSelectivity  = 1,
             sJoinGroupOutputRecord = 1,
             i = 0; i < sBaseGraphCnt; i++ )
        {
            // BUG-37228
            sJoinGroupSelectivity  *= sBaseGraph[i]->costInfo.selectivity;
            sJoinGroupOutputRecord *= sBaseGraph[i]->costInfo.outputRecordCnt;
        }

        // Join predicate list �� selectivity ���
        IDE_TEST( qmoSelectivity::setMySelectivity4Join( aStatement,
                                                         NULL,
                                                         aJoinGroup->joinPredicate,
                                                         ID_TRUE )
                  != IDE_SUCCESS );

        sJoinGroupSelectivity *= aJoinGroup->joinPredicate->mySelectivity;

        IDE_DASSERT_MSG( sJoinGroupSelectivity >= 0 && sJoinGroupSelectivity <= 1,
                         "sJoinGroupSelectivity : %"ID_DOUBLE_G_FMT"\n",
                         sJoinGroupSelectivity );

        sJoinGroupOutputRecord *= sJoinGroupSelectivity;

        sFirstRowsFactor = IDL_MIN( (sBaseGraph[0])->myQuerySet->SFWGH->hints->firstRowsN /
                                     sJoinGroupOutputRecord,
                                     QMO_COST_FIRST_ROWS_FACTOR_DEFAULT);
    }
    else
    {
        sFirstRowsFactor = QMO_COST_FIRST_ROWS_FACTOR_DEFAULT;
    }

    //------------------------------------------
    // joinOrderFactor, joinSize �� �̿��� Join Ordering
    //------------------------------------------

    for ( sTop = 0; sTop < sBaseGraphCnt - 1; sTop++ )
    {
        sSelectedJoinGraph = NULL;

        for ( i = sTop; i < sEnd; i ++ )
        {
            // �� qmgJoin �� joinOrderFactor, joinSize ���
            IDE_TEST( qmoSelectivity::setJoinOrderFactor( aStatement,
                                                          (qmgGraph *) & sJoinGraph[i],
                                                          aJoinGroup->joinPredicate,
                                                          & sCurJoinOrderFactor,
                                                          & sCurJoinSize )
                      != IDE_SUCCESS );

            if( isFeasibleJoinOrder( &sJoinGraph[i].graph,
                                     aJoinGroup->joinRelation ) == ID_FALSE )
            {
                continue;
            }
            else
            {
                // Nothing to do.
            }

            sJoinGraph[i].joinOrderFactor = sCurJoinOrderFactor;
            sJoinGraph[i].joinSize        = sCurJoinSize;

            // JoinOrderFactor �� ���� ���� qmgJoin ����
            if ( sSelectedJoinGraph == NULL )
            {
                sJoinOrderFactor = sCurJoinOrderFactor;
                sJoinSize = sCurJoinSize;
                sSelectedJoinGraph = & sJoinGraph[i];
                sSelectedJoinGraphIsTop = ID_TRUE;
            }
            else
            {
                //--------------------------------------------------
                // PROJ-1352
                // Join Selectivity Threshold �� Join Size �� ����Ͽ�
                // Join Order�� �����Ѵ�.
                //--------------------------------------------------

                if ( sJoinOrderFactor > QMO_JOIN_SELECTIVITY_THRESHOLD )
                {
                    // ���� JoinOrderFactor �� THRESHOLD���� ū ���
                    if ( sJoinOrderFactor > sCurJoinOrderFactor )
                    {
                        sJoinOrderFactor = sCurJoinOrderFactor;
                        sJoinSize = sCurJoinSize;
                        sSelectedJoinGraph = & sJoinGraph[i];
                        sSelectedJoinGraphIsTop = ID_FALSE;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    // ���� JoinOrderFactor �� THRESHOLD���� ���� ���

                    if ( sCurJoinOrderFactor > QMO_JOIN_SELECTIVITY_THRESHOLD )
                    {
                        // nothing to do
                    }
                    else
                    {
                        // ���� selectivity�� ���� selectivity��
                        // ��� THRESHOLD���� ���� ��� Join Size��
                        // ����Ͽ� ���Ѵ�.
                        if ( sJoinOrderFactor * sJoinSize >
                             sCurJoinOrderFactor * sCurJoinSize )
                        {
                            sJoinOrderFactor = sCurJoinOrderFactor;
                            sJoinSize = sCurJoinSize;
                            sSelectedJoinGraph = & sJoinGraph[i];
                            sSelectedJoinGraphIsTop = ID_FALSE;
                        }
                        else
                        {
                            // nothing to do
                        }
                    }
                }
            }
        }

        IDE_TEST_RAISE( sSelectedJoinGraph == NULL,
                        ERR_INVALID_JOIN_GRAPH );

        // ���õ� Join Graph�� left�� right graph��
        // Join Order �����Ǿ����� ����
        sSelectedJoinGraph->graph.left->flag &= ~QMG_JOIN_ORDER_DECIDED_MASK;
        sSelectedJoinGraph->graph.left->flag |= QMG_JOIN_ORDER_DECIDED_TRUE;

        sSelectedJoinGraph->graph.right->flag &= ~QMG_JOIN_ORDER_DECIDED_MASK;
        sSelectedJoinGraph->graph.right->flag |= QMG_JOIN_ORDER_DECIDED_TRUE;

        // top ����
        IDE_TEST( initJoinGraph( aStatement,
                                 (UInt)sSelectedJoinGraph->graph.type,
                                 sSelectedJoinGraph->graph.left,
                                 sSelectedJoinGraph->graph.right,
                                 & sJoinGraph[sTop].graph,
                                 ID_TRUE )
                  != IDE_SUCCESS );

        // To Fix BUG-10357
        if ( sSelectedJoinGraphIsTop == ID_FALSE )
        {
            //fix for overlap memcpy
            if( &sJoinGraph[sTop].graph.costInfo !=
                               &sSelectedJoinGraph->graph.costInfo )
            {
                // To Fix PR-8266
                // �����ϴ� Join Graph�� Cost������ �����Ͽ��� ��.
                idlOS::memcpy( & sJoinGraph[sTop].graph.costInfo,
                               & sSelectedJoinGraph->graph.costInfo,
                               ID_SIZEOF(qmgCostInfo) );
            }
        }


        // Join Predicate�� �з� :
        // Join Group::joinPredicate���� �ش� predicate�� Join Graph��
        // myPredicate�� �����Ѵ�.
        IDE_TEST( connectJoinPredToJoinGraph( aStatement,
                                              & aJoinGroup->joinPredicate,
                                              (qmgGraph *) & sJoinGraph[sTop] )
                  != IDE_SUCCESS );

        if( sTop == 0 )
        {
            // FirstRows set
            sJoinGraph[sTop].firstRowsFactor = sFirstRowsFactor;
        }
        else
        {
            sJoinGraph[sTop].firstRowsFactor = QMO_COST_FIRST_ROWS_FACTOR_DEFAULT;
        }

        if ( aJoinGroup->mIsOnlyNL == ID_TRUE )
        {
            sJoinGraph[sTop].graph.flag &= ~QMG_JOIN_ONLY_NL_MASK;
            sJoinGraph[sTop].graph.flag |= QMG_JOIN_ONLY_NL_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
        // ���õ� qmgJoin�� ����ȭ
        IDE_TEST( sJoinGraph[sTop].graph.optimize( aStatement,
                                                   &sJoinGraph[sTop].graph )
                  != IDE_SUCCESS );

        //Join Order�� �������� ���� base graph��� qmgJoin�� �ʱ�ȭ
        sEnd = sTop + 1;

        for ( i = 0; i < sBaseGraphCnt; i++ )
        {
            if ( ( sBaseGraph[i]->flag & QMG_JOIN_ORDER_DECIDED_MASK )
                 == QMG_JOIN_ORDER_DECIDED_FALSE )
            {
                IDE_TEST( initJoinGraph( aStatement,
                                         aJoinGroup->joinRelation,
                                         &sJoinGraph[sTop].graph,
                                         sBaseGraph[i],
                                         &sJoinGraph[sEnd].graph,
                                         ID_TRUE )
                          != IDE_SUCCESS );
                sEnd++;
            }
            else
            {
                // nothing to do
            }
        }
    }

    // To Fix PR-7989
    aJoinGroup->topGraph = & sJoinGraph[sTop - 1].graph;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_JOIN_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoCnfMgr::joinOrderingInJoinGroup",
                                  "Invalid join graph" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoCnfMgr::isSemiAntiJoinable( qmgGraph        * aJoinGraph,
                               qmoJoinRelation * aJoinRelation )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     Join ordering �� semi/anti join�� ������ �������� Ȯ���Ѵ�.
 *     Semi/anti join ����� relation�� ���� outer relation�� join�� ���
 *     outer relation���� ��� ���� join�Ǿ��־�� �Ѵ�.
 *     ex) SELECT R.*, S.* FROM R, S, T WHERE R.A (semi)= T.A AND S.B (semi)= T.B;
 *         �� ��� R�� S�� ���� join �Ǳ� ������ R�̳� S�� T�� join�� �� ����.
 *
 * Implementation :
 *     Join relation�� �� inner relation�� semi/anti join ��� �ش��ϴ�
 *     �͵鸸 ã�� outer relation�鿡�� ��� �����ϰ� �ִ��� Ȯ���Ѵ�.
 *
 ***********************************************************************/

    qmoJoinRelation * sJoinRelation;
    qcDepInfo         sOuterDepInfo;
    qcDepInfo         sInnerDepInfo;

    sJoinRelation = aJoinRelation;

    while( sJoinRelation != NULL )
    {
        qtc::dependencySet( sJoinRelation->innerRelation,
                            &sInnerDepInfo );
        qtc::dependencyRemove( sJoinRelation->innerRelation,
                               &sJoinRelation->depInfo,
                               &sOuterDepInfo );

        if( qtc::dependencyEqual( &aJoinGraph->right->depInfo,
                                  &sInnerDepInfo ) == ID_TRUE )
        {
            if( qtc::dependencyContains( &aJoinGraph->left->depInfo,
                                         &sOuterDepInfo ) == ID_FALSE )
            {

                return ID_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }

        sJoinRelation = sJoinRelation->next;
    }

    return ID_TRUE;
}

idBool
qmoCnfMgr::isFeasibleJoinOrder( qmgGraph        * aJoinGraph,
                                qmoJoinRelation * aJoinRelation )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     �־��� type�� join�� ������ �������� Ȯ���Ѵ�.
 *
 * Implementation :
 *     1) Semi/anti join�� ��� isSemiAntiJoinable�� ȣ��
 *     2) inner join �Ϸ��� table ��
 *        semi/anti join ������ table �� �ƴϾ���� (BUG-39249)
 *
 ***********************************************************************/

    qmoJoinRelation* sJoinRelation;
    qcDepInfo        sInnerDepInfo;
    idBool           sResult;

    sResult = ID_TRUE;

    switch (aJoinGraph->type)
    {
        case QMG_INNER_JOIN:
            sJoinRelation = aJoinRelation;
            while (sJoinRelation != NULL)
            {
                if (sJoinRelation->joinType != QMO_JOIN_RELATION_TYPE_INNER)
                {
                    qtc::dependencySet(sJoinRelation->innerRelation,
                                       &sInnerDepInfo);

                   if (qtc::dependencyEqual(&aJoinGraph->right->depInfo,
                                            &sInnerDepInfo) == ID_TRUE)
                    {
                        sResult = ID_FALSE;
                        break;
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }
                else
                {
                    /* nothing to do */
                }
                sJoinRelation = sJoinRelation->next;
            }
            break;

        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
            sResult = isSemiAntiJoinable(aJoinGraph, aJoinRelation);
            break;

        default:
            sResult = ID_TRUE;
            break;
    }

    return sResult;
}

IDE_RC
qmoCnfMgr::initJoinGraph( qcStatement     * aStatement,
                          qmoJoinRelation * aJoinRelation,
                          qmgGraph        * aLeft,
                          qmgGraph        * aRight,
                          qmgGraph        * aJoinGraph,
                          idBool            aExistOrderFactor )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     qmoJoinRelation �ڷᱸ���� �ľ��Ͽ� �˸´� type�� join graph��
 *     �������ش�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoJoinRelation * sJoinRelation;
    qcDepInfo         sOuterDepInfo;
    qcDepInfo         sInnerDepInfo;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::initJoinGraph::__FT__" );

    IDE_FT_ERROR( aLeft      != NULL );
    IDE_FT_ERROR( aRight     != NULL );
    IDE_FT_ERROR( aJoinGraph != NULL );

    sJoinRelation = aJoinRelation;

    while( sJoinRelation != NULL )
    {
        if( sJoinRelation->joinType != QMO_JOIN_RELATION_TYPE_INNER )
        {
            // Semi/anti join�� join�� ���⿡ ���� graph�� �ʱ�ȭ�Ѵ�.
            qtc::dependencySet( sJoinRelation->innerRelation,
                                &sInnerDepInfo );
            qtc::dependencyRemove( sJoinRelation->innerRelation,
                                   &sJoinRelation->depInfo,
                                   &sOuterDepInfo );

            // Left�� outer, right�� inner���� Ȯ���Ѵ�.
            if( ( qtc::dependencyContains( &aLeft->depInfo, &sOuterDepInfo ) == ID_TRUE ) &&
                ( qtc::dependencyContains( &aRight->depInfo, &sInnerDepInfo ) == ID_TRUE ) )
            {
                if( sJoinRelation->joinType == QMO_JOIN_RELATION_TYPE_SEMI )
                {
                    IDE_TEST( qmgSemiJoin::init( aStatement,
                                                 aLeft,
                                                 aRight,
                                                 aJoinGraph )
                              != IDE_SUCCESS );
                    break;
                }
                else if( sJoinRelation->joinType == QMO_JOIN_RELATION_TYPE_ANTI )
                {
                    IDE_TEST( qmgAntiJoin::init( aStatement,
                                                 aLeft,
                                                 aRight,
                                                 aJoinGraph )
                              != IDE_SUCCESS );
                    break;
                }
                else
                {
                    IDE_FT_ERROR( 0 );
                }
            }
            else
            {
                // Nothing to do.
            }

            // Right�� outer, left�� inner���� Ȯ���Ѵ�.
            if( ( qtc::dependencyContains( &aRight->depInfo, &sOuterDepInfo ) == ID_TRUE ) &&
                ( qtc::dependencyContains( &aLeft->depInfo, &sInnerDepInfo ) == ID_TRUE ) )
            {
                if( sJoinRelation->joinType == QMO_JOIN_RELATION_TYPE_SEMI )
                {
                    IDE_TEST( qmgSemiJoin::init( aStatement,
                                                 aRight,
                                                 aLeft,
                                                 aJoinGraph )
                              != IDE_SUCCESS );
                    break;
                }
                else if( sJoinRelation->joinType == QMO_JOIN_RELATION_TYPE_ANTI )
                {
                    IDE_TEST( qmgAntiJoin::init( aStatement,
                                                 aRight,
                                                 aLeft,
                                                 aJoinGraph )
                              != IDE_SUCCESS );
                    break;
                }
                else
                {
                    IDE_FT_ERROR( 0 );
                }
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }

        sJoinRelation = sJoinRelation->next;
    }

    if( sJoinRelation == NULL )
    {
        IDE_TEST( qmgJoin::init( aStatement,
                                 aLeft,
                                 aRight,
                                 aJoinGraph,
                                 aExistOrderFactor )
                  != IDE_SUCCESS );
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
qmoCnfMgr::initJoinGraph( qcStatement     * aStatement,
                          UInt              aJoinGraphType,
                          qmgGraph        * aLeft,
                          qmgGraph        * aRight,
                          qmgGraph        * aJoinGraph,
                          idBool            aExistOrderFactor )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     �־��� join graph�� type�� ���� join graph�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::initJoinGraph::__FT__" );

    switch( aJoinGraphType )
    {
        case QMG_INNER_JOIN:
            IDE_TEST( qmgJoin::init( aStatement,
                                     aLeft,
                                     aRight,
                                     aJoinGraph,
                                     aExistOrderFactor )
                      != IDE_SUCCESS );
            break;
        case QMG_SEMI_JOIN:
            IDE_TEST( qmgSemiJoin::init( aStatement,
                                         aLeft,
                                         aRight,
                                         aJoinGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_ANTI_JOIN:
            IDE_TEST( qmgAntiJoin::init( aStatement,
                                         aLeft,
                                         aRight,
                                         aJoinGraph )
                      != IDE_SUCCESS );
            break;
        default:
            IDE_FT_ERROR( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::relocateJoinGroup4PushPredHint( qcStatement      * aStatement,
                                           qmsPushPredHints * aPushPredHint,
                                           qmoJoinGroup    ** aJoinGroup,
                                           UInt               aJoinGroupCnt )
{
/***********************************************************************
 *
 * Description : PUSH_PRED hint�� ����Ǿ��� ����� join group ���ġ
 *
 * Implementation :
 *
 *   PUSH_PRED(view) hint�� ����Ǿ��� ���,
 *   join ������ �ٸ� ���̺��� view���� ���� ����Ǿ�� �Ѵ�.
 *
 ***********************************************************************/

    UInt                sCnt;
    UInt                i;
    qmoJoinGroup      * sNewJoinGroup;
    qmoJoinGroup      * sJoinGroup;
    qmsPushPredHints  * sPushPredHint;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::relocateJoinGroup4PushPredHint::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPushPredHint != NULL );
    IDE_DASSERT( *aJoinGroup != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinGroup = *aJoinGroup;

    //------------------------------------------
    // PUSH_PRED(view)�� outer dependency�� ã�´�.
    //------------------------------------------

    for( sPushPredHint = aPushPredHint;
         sPushPredHint != NULL;
         sPushPredHint = sPushPredHint->next )
    {
        if( ( ( sPushPredHint->table->table->tableRef->flag &
                QMS_TABLE_REF_PUSH_PRED_HINT_MASK )
              == QMS_TABLE_REF_PUSH_PRED_HINT_TRUE )
            &&
            ( ( sPushPredHint->table->table->tableRef->flag &
                QMS_TABLE_REF_PUSH_PRED_HINT_INNER_JOIN_MASK )
              == QMS_TABLE_REF_PUSH_PRED_HINT_INNER_JOIN_FALSE ) )
        {
            if( aJoinGroupCnt == 1 )
            {
                sNewJoinGroup = sJoinGroup;
            }
            else
            {
                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoJoinGroup) * aJoinGroupCnt,
                                                         (void **)&sNewJoinGroup )
                          != IDE_SUCCESS );

                for( sCnt = 0, i = 0; sCnt < aJoinGroupCnt; sCnt++ )
                {
                    // PROJ-1495 BUG-
                    // PUSH_PRED view�� ������ table�� ���� ����ǵ��� ��ġ
                    if( qtc::dependencyContains( &(sJoinGroup[sCnt].depInfo),
                                                 &(sPushPredHint->table->table->depInfo) )
                        != ID_TRUE )
                    {
                        idlOS::memcpy( &sNewJoinGroup[i],
                                       &sJoinGroup[sCnt],
                                       ID_SIZEOF(qmoJoinGroup) );
                        i++;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }

                // PUSH_PRED view�� �ڿ� ����ǵ��� ��ġ
                for( sCnt = 0; sCnt < aJoinGroupCnt; sCnt++ )
                {
                    // PROJ-1495 BUG-

                    if( qtc::dependencyContains( &(sJoinGroup[sCnt].depInfo),
                                                 &(sPushPredHint->table->table->depInfo) )
                        == ID_TRUE )
                    {
                        idlOS::memcpy( &sNewJoinGroup[i],
                                       &sJoinGroup[sCnt],
                                       ID_SIZEOF(qmoJoinGroup) );
                        i++;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }
            }

            sJoinGroup = sNewJoinGroup;
        }
        else
        {
            // Nothing To Do
        }
    }

    *aJoinGroup = sJoinGroup;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::getTableOrder( qcStatement    * aStatement,
                          qmgGraph       * aGraph,
                          qmoTableOrder ** aTableOrder )
{
/***********************************************************************
 *
 * Description : Table Order ���� ���� �� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgGraph      * sGraph;
    qmoTableOrder * sTableOrder;
    qmoTableOrder * sFirstOrder;
    qmoTableOrder * sTableOrderList;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::getTableOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sGraph = aGraph;
    sFirstOrder = *aTableOrder;

    if ( ( aGraph->type == QMG_SELECTION ) ||
         ( aGraph->type == QMG_SHARD_SELECT ) || // PROJ-2638
         ( aGraph->type == QMG_HIERARCHY ) ||
         ( aGraph->type == QMG_PARTITION ) )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoTableOrder),
                                                 (void **) & sTableOrder )
                  != IDE_SUCCESS );

        qtc::dependencySetWithDep( & sTableOrder->depInfo,
                                   & sGraph->depInfo );
        sTableOrder->next = NULL;

        if ( sFirstOrder == NULL )
        {
            sFirstOrder = sTableOrder;
        }
        else
        {
            for ( sTableOrderList = *aTableOrder;
                  sTableOrderList->next != NULL;
                  sTableOrderList = sTableOrderList->next ) ;

            sTableOrderList->next = sTableOrder;
        }

    }
    else
    {
        if ( aGraph->left != NULL )
        {
            IDE_TEST( getTableOrder( aStatement,
                                     aGraph->left,
                                     & sFirstOrder )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }

        if ( aGraph->right != NULL )
        {
            IDE_TEST( getTableOrder( aStatement,
                                     aGraph->right,
                                     & sFirstOrder )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothingt to do
        }
    }

    *aTableOrder = sFirstOrder;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::connectJoinPredToJoinGraph( qcStatement   * aStatement,
                                       qmoPredicate ** aJoinPred,
                                       qmgGraph      * aJoinGraph )
{
/***********************************************************************
 *
 * Description : Join Group�� join predicate���� ���� Join Graph�� �ش��ϴ�
 *               join predicate�� �и��Ͽ� Join Graph�� �����Ѵ�.
 *
 * Implementaion :
 *
 *     Ex)  [p2] Predicate�� Join Graph�� �����ϴ� ���
 *
 *                             sPrevJoinPred
 *                               |
 *                               |       sCurJoinPred
 *                               |        |
 *                               V        V
 *              [JoinGroup] --> [p1] --> [p2] --> [p3]
 *
 *              [JoinGraph] --> [q1] --> [q2] --> [q3]
 *                               +                  +
 *                               |                  |
 *                             sJoinGraphPred      sCurJoinGraphPred
 *
 *         ==>  [p2] Predicate ���� ��
 *
 *                             sPrevJoinPred
 *                               |
 *                               |       sCurJoinPred
 *                               |        |
 *                               V        V
 *              [JoinGroup] --> [p1] --> [p3]
 *
 *              [JoinGraph] --> [q1] --> [q2] --> [q3] --> [p2]
 *                               +                           +
 *                               |                           |
 *                             sJoinGraphPred         sCurJoinGraphPred
 *
 ***********************************************************************/

    qmoPredicate * sJoinPred;
    qmoPredicate * sPrevJoinPred;
    qmoPredicate * sCurJoinPred;
    qmoPredicate * sJoinGraphPred;
    qmoPredicate * sCurJoinGraphPred;
    qmoPredicate * sCopiedJoinPred;
    qcDepInfo    * sJoinDependencies;
    qcDepInfo    * sFromDependencies;
    qcDepInfo      sAndDependencies;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::connectJoinPredToJoinGraph::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    // join predicate ( Join Group �Ǵ� CNF�� joinPredicate )
    sJoinPred            = *aJoinPred;
    sCurJoinPred         = *aJoinPred;
    sPrevJoinPred        = NULL;

    // Join Graph�� join predicate
    sJoinGraphPred       = NULL;
    sCurJoinGraphPred    = NULL;
    sJoinDependencies    = & aJoinGraph->depInfo;
    sFromDependencies    = & aJoinGraph->myQuerySet->SFWGH->depInfo;

    while( sCurJoinPred != NULL )
    {
        //------------------------------------------------
        //  Join Graph�� ���ԵǴ� Join Predicate���� �Ǵ�
        //  ~(Graph Dep) & (From & Pred Dep) == 0 : ���Ե�
        //------------------------------------------------

        qtc::dependencyAnd( sFromDependencies,
                            & sCurJoinPred->node->depInfo,
                            & sAndDependencies );

        if ( qtc::dependencyContains( sJoinDependencies,
                                      & sAndDependencies ) == ID_TRUE )
        {
            //------------------------------------------
            // Join Graph�� join predicate�� ���
            //------------------------------------------

            // Join Group�Ǵ� CNF���� ���� Join Predicate�� �и�
            if ( sPrevJoinPred == NULL )
            {
                sJoinPred = sCurJoinPred->next;
            }
            else
            {
                sPrevJoinPred->next = sCurJoinPred->next;
            }

            IDE_TEST( qmoPred::copyOnePredicate( QC_QMP_MEM( aStatement ),
                                                 sCurJoinPred,
                                                 &sCopiedJoinPred )
                      != IDE_SUCCESS );

            // �и��� Join Predicate�� Join Graph�� myPredicate�� ����
            if ( sJoinGraphPred == NULL )
            {
                sJoinGraphPred = sCurJoinGraphPred = sCopiedJoinPred;
                sCurJoinPred = sCurJoinPred->next;
                sCurJoinGraphPred->next = NULL;
            }
            else
            {
                sCurJoinGraphPred->next = sCopiedJoinPred;
                sCurJoinPred = sCurJoinPred->next;

                // To Fix BUG-8219
                sCurJoinGraphPred = sCurJoinGraphPred->next;

                sCurJoinGraphPred->next = NULL;
            }
        }
        else
        {
            //------------------------------------------
            // Join Graph�� join predicate�� �ƴ� ���
            //------------------------------------------

            sPrevJoinPred = sCurJoinPred;
            sCurJoinPred = sCurJoinPred->next;
        }
    }

    *aJoinPred = sJoinPred;
    aJoinGraph->myPredicate = sJoinGraphPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::joinGroupingWithJoinPred( qmoJoinGroup  * aJoinGroup,
                                     qmoCNF        * aCNF,
                                     UInt          * aJoinGroupCnt )
{
/***********************************************************************
 *
 * Description : JoinPredicate�� �̿��Ͽ� Join Grouping�� ����
 *
 * Implementation :
 *       Join Group�� ���Ե��� ���� Join Predicate���������� ����������
 *       ������ �����Ѵ�.
 *       (1) ���ο� Join Group ����
 *       (2) Join Group�� ���ԵǴ� ��� Join Predicate�� ���
 *
 *        < ���� >
 *        ��� �ÿ� ���� Join Predicate List���� �ش� Predicate�� ������
 *        ���´�. ���� Join Predicate List�� �������� ������
 *        Join Group�� ���Ե��� ���� Join Predicate�� ������ �ǹ��Ѵ�.
 *
 *
 *    Ex)
 *        Join Group�� ���ԵǴ� ��� Join Predicate�� ���
 *
 *        [p2] Predicate�� �ش� Join Group�� �����ϴ� ���
 *
 *                             sFirstPred
 *                               |
 *                               |   sPrevPred
 *                               |        |    sAddPred
 *                               |        |    sCurPred
 *                               |        |        |
 *                               V        V        V
 *     [CNF::joinPredicate] --> [p1] --> [p2] --> [p3] --> [p4]
 *
 *             [Join Group] --> [q1] --> [q2] --> [q3]
 *
 *
 *      ==> CNF�� joinPredicate���� ������ ����, sCurUpperPred�� �������� ����
 *
 *                             sFirstPred
 *                               |    sPrevJoinPred
 *                               |         | sAddPred
 *                               |         |   |
 *                               |         |   V
 *                               |         |  [p3]  sCurPred
 *                               |         |     \    |
 *                               V         V      V   V
 *     [CNF::joinPredicate] --> [p1] --> [p2] -----> [p4]
 *
 *             [Join Group] --> [q1] --> [q2] --> [q3]
 *
 *
 *     ==>  [p3] Predicate ����
 *
 *                             sFirstPred
 *                               |   sPrevUpperPred
 *                               |        |     sCurUpperPred
 *                               |        |        |
 *                               V        V        V
 *     [CNF::joinPredicate] --> [p1] --> [p2] --> [p4]
 *
 *                           +[p3]
 *                          /      \
 *                         /        V
 *             [Join Group] --X--> [q1] --> [q2] --> [q3]
 *
 *
 *    ==>  [p3] Predicate ���� ��
 *
 *                             sFirstPred
 *                               |   sPrevUpperPred
 *                               |        |     sCurUpperPred
 *                               |        |        |
 *                               V        V        V
 *     [CNF::joinPredicate] --> [p1] --> [p2] --> [p4]
 *
 *             [Join Group] --> [p3]-->[q1] --> [q2] --> [q3]
 *
 *
 ***********************************************************************/


    UInt           sJoinGroupCnt;
    qmoJoinGroup * sJoinGroup;
    qmoJoinGroup * sCurJoinGroup;
    qmoPredicate * sFirstPred;
    qmoPredicate * sAddPred = NULL;
    qmoPredicate * sCurPred;
    qmoPredicate * sPrevPred;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::joinGroupingWithJoinPred::__FT__" );

    //------------------------------------------------------
    //  sFirstPred : ���ο� Join Group�� �ʿ��� Predicate
    //  sAddPred : Join Group�� ���ԵǴ� Join Predicate�� ���� ����
    //  sCurPred : Join Predicate�� Traverse ����
    //  sPrevPred : Join Group�� ���Ե��� ���� Join Predicate�� ���� ����
    //------------------------------------------------------

    qcDepInfo      sAndDependencies;

    idBool         sIsInsert;

    UInt               i;
    qcDepInfo          sGraphDepInfo;

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinGroup != NULL );
    IDE_DASSERT( aCNF->joinPredicate != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinGroup    = aJoinGroup;
    sJoinGroupCnt = aCNF->joinGroupCnt;
    sFirstPred    = aCNF->joinPredicate;

    //------------------------------------------
    // JoinGroup�� dependencies ���� �� ���� JoinPredicate ����
    //------------------------------------------

    while ( sFirstPred != NULL )
    {

        //------------------------------------------
        // Join Group�� ������ �ʴ� ù��° Predicate ���� :
        //    Join Group�� ������ ���� ù��° Predicate��
        //    dependencies�� ���ο� Join Group�� dependencies�� ����
        //------------------------------------------

        sCurJoinGroup = & sJoinGroup[sJoinGroupCnt++];

        IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                     & sFirstPred->node->depInfo,
                                     & sCurJoinGroup->depInfo )
                  != IDE_SUCCESS );

        //------------------------------------------
        // BUG-19371
        // base graph�� selection�� �ƴ� join�� ���
        // join predicate�� ��� �ۼ��ϴ��Ŀ�����
        // join grouping�� �ߺ� ���Ե� �� �ִ�.
        // �ߺ� ���Ե��� �ʵ��� �ؾ� ��.
        //------------------------------------------

        for( i = 0;
             i < aCNF->graphCnt4BaseTable;
             i++ )
        {
            if( ( aCNF->baseGraph[i]->type == QMG_INNER_JOIN ) ||
                ( aCNF->baseGraph[i]->type == QMG_LEFT_OUTER_JOIN ) ||
                ( aCNF->baseGraph[i]->type == QMG_FULL_OUTER_JOIN ) )
            {
                sGraphDepInfo = aCNF->baseGraph[i]->depInfo;

                qtc::dependencyAnd( & sGraphDepInfo,
                                    & sFirstPred->node->depInfo,
                                    & sAndDependencies );

                if( qtc::dependencyEqual( & sAndDependencies,
                                          & qtc::zeroDependencies ) == ID_FALSE )
                {
                    IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                                 & sGraphDepInfo,
                                                 & sCurJoinGroup->depInfo )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing To Do
                }
            }
        }

        //------------------------------------------
        // ���� JoinGroup�� ������ ��� Join Predicate�� ã��
        // JoinGroup�� ���
        //------------------------------------------

        sCurPred  = sFirstPred;
        sPrevPred = NULL;
        sIsInsert = ID_FALSE;
        while( sCurPred != NULL )
        {

            qtc::dependencyAnd( & sCurJoinGroup->depInfo,
                                & sCurPred->node->depInfo,
                                & sAndDependencies );

            if ( qtc::dependencyEqual( & sAndDependencies,
                                       & qtc::zeroDependencies )== ID_FALSE )
            {
                //------------------------------------------
                // Join Group�� ������ Predicate
                //------------------------------------------

                // Join Group�� ���Ӱ� ����ϴ� Predicate�� ����
                sIsInsert = ID_TRUE;

                // Join Group�� ���
                IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                             & sCurPred->node->depInfo,
                                             & sCurJoinGroup->depInfo )
                          != IDE_SUCCESS );

                // Join Group�� ���
                //------------------------------------------
                // BUG-19371
                // base graph�� selection�� �ƴ� join�� ���
                // join predicate�� ��� �ۼ��ϴ��Ŀ�����
                // join grouping�� �ߺ� ���Ե� �� �ִ�.
                // �ߺ� ���Ե��� �ʵ��� �ؾ� ��.
                //------------------------------------------

                for( i = 0;
                     i < aCNF->graphCnt4BaseTable;
                     i++ )
                {
                    if( ( aCNF->baseGraph[i]->type == QMG_INNER_JOIN ) ||
                        ( aCNF->baseGraph[i]->type == QMG_LEFT_OUTER_JOIN ) ||
                        ( aCNF->baseGraph[i]->type == QMG_FULL_OUTER_JOIN ) )
                    {
                        sGraphDepInfo = aCNF->baseGraph[i]->depInfo;

                        qtc::dependencyAnd( & sGraphDepInfo,
                                            & sCurPred->node->depInfo,
                                            & sAndDependencies );

                        if( qtc::dependencyEqual( & sAndDependencies,
                                                  & qtc::zeroDependencies ) == ID_FALSE )
                        {
                            IDE_TEST( qtc::dependencyOr( & sCurJoinGroup->depInfo,
                                                         & sGraphDepInfo,
                                                         & sCurJoinGroup->depInfo )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // Nothing To Do
                        }
                    }
                }

                // Predicate�� Join Group�� ���ԵǾ����� ����
                sCurPred->flag &= ~QMO_PRED_INCLUDE_JOINGROUP_MASK;
                sCurPred->flag |= QMO_PRED_INCLUDE_JOINGROUP_TRUE;

                // Predicate�� Join Predicate���� ������ ����,
                // JoinGroup�� joinPredicate�� �����Ŵ

                if ( sCurJoinGroup->joinPredicate == NULL )
                {
                    sCurJoinGroup->joinPredicate = sCurPred;
                    sAddPred = sCurPred;
                }
                else
                {
                    IDE_FT_ASSERT( sAddPred != NULL );
                    sAddPred->next = sCurPred;
                    sAddPred = sAddPred->next;
                }
                sCurPred = sCurPred->next;
                sAddPred->next = NULL;

                if ( sPrevPred == NULL )
                {
                    sFirstPred = sCurPred;
                }
                else
                {
                    sPrevPred->next = sCurPred;
                }
            }
            else
            {
                // Join Group�� �������� ���� Predicate : nothing to do
                sPrevPred = sCurPred;
                sCurPred = sCurPred->next;
            }

            if ( ( sCurPred == NULL ) && ( sIsInsert == ID_TRUE ) )
            {
                //------------------------------------------
                // Join Group�� �߰� ����� Predicate�� �����ϴ� ���,
                // �߰��� Predicate�� ������ Join Predicate�� ������ �� ����
                //------------------------------------------

                sCurPred = sFirstPred;
                sPrevPred = NULL;
                sIsInsert = ID_FALSE;
            }
            else
            {
                // �� �̻� ������ Predicate ���� : nothing to do
            }
        }
    }

    *aJoinGroupCnt = sJoinGroupCnt;
    // CNF�� joinPredicate�� joinGroup�� joinPredicate���� ��� �з���Ŵ
//    *aJoinPredicate = NULL;
    aCNF->joinPredicate = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::connectBaseGraphToJoinGroup( qmoJoinGroup   * aJoinGroup,
                                        qmgGraph      ** aCNFBaseGraph,
                                        UInt             aCNFBaseGraphCnt )
{
/***********************************************************************
 *
 * Description : Join Group�� ���ϴ� base graph ����
 *
 * Implementaion :
 *
 ***********************************************************************/

    qmoJoinGroup     * sJoinGroup;
    UInt               sCNFBaseGraphCnt;
    qmgGraph        ** sCNFBaseGraph;
    UInt               sJoinGroupBaseGraphCnt;
    UInt               i;

    qcDepInfo          sAndDependencies;
    qcDepInfo          sGraphDepInfo;

    qmsPushPredHints * sPushPredHint;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::connectBaseGraphToJoinGroup::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinGroup != NULL );
    IDE_DASSERT( aCNFBaseGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinGroup             = aJoinGroup;
    sCNFBaseGraphCnt       = aCNFBaseGraphCnt;
    sCNFBaseGraph          = aCNFBaseGraph;
    sJoinGroupBaseGraphCnt = 0;

    for ( i = 0; i < sCNFBaseGraphCnt; i++ )
    {
        // PROJ-1495 BUG-
        for( sPushPredHint =
                 sCNFBaseGraph[i]->myQuerySet->SFWGH->hints->pushPredHint;
             sPushPredHint != NULL;
             sPushPredHint = sPushPredHint->next )
        {
            if( ( ( sPushPredHint->table->table->tableRef->flag &
                    QMS_TABLE_REF_PUSH_PRED_HINT_MASK )
                  == QMS_TABLE_REF_PUSH_PRED_HINT_TRUE ) &&
                ( sPushPredHint->table->table == sCNFBaseGraph[i]->myFrom ) )
            {
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if( sPushPredHint != NULL )
        {
            sGraphDepInfo = sCNFBaseGraph[i]->myFrom->depInfo;
        }
        else
        {
            sGraphDepInfo = sCNFBaseGraph[i]->depInfo;
        }

        qtc::dependencyAnd( & sJoinGroup->depInfo,
                            & sGraphDepInfo,
                            & sAndDependencies );

        if ( qtc::dependencyEqual( & sAndDependencies,
                                   & qtc::zeroDependencies ) == ID_FALSE )
        {
            //------------------------------------------
            // ( Join Group�� dependencies & BaseGraph�� dependencies )
            //  != 0 �̸�, Join Group�� ���ϴ� Base Graph
            //------------------------------------------

            sJoinGroup->baseGraph[sJoinGroupBaseGraphCnt++] =
                sCNFBaseGraph[i];

            sCNFBaseGraph[i]->flag &= ~QMG_INCLUDE_JOIN_GROUP_MASK;
            sCNFBaseGraph[i]->flag |= QMG_INCLUDE_JOIN_GROUP_TRUE;
        }
        else
        {
            // nothing to do
        }
    }
    sJoinGroup->baseGraphCnt = sJoinGroupBaseGraphCnt;

    return IDE_SUCCESS;
}

idBool
qmoCnfMgr::existJoinRelation( qmoJoinRelation * aJoinRelationList,
                              qcDepInfo       * aDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery Unnesting
 *     aDepInfo�� ������ dependency�� ���� join relation�� �����ϴ���
 *     Ȯ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoJoinRelation * sCurJoinRelation;

    sCurJoinRelation = aJoinRelationList;

    while ( sCurJoinRelation != NULL )
    {
        if ( qtc::dependencyEqual( & sCurJoinRelation->depInfo,
                                   aDepInfo )
             == ID_TRUE )
        {
            return ID_TRUE;
        }
        sCurJoinRelation = sCurJoinRelation->next;
    }

    return ID_FALSE;
}

IDE_RC
qmoCnfMgr::getInnerTable( qcStatement * aStatement,
                          qtcNode     * aNode,
                          SInt        * aTableID )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery Unnesting
 *     Semi/anti join�� predicate���� inner table�� ID�� ���´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt      sTable;
    qmsFrom * sFrom;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::getInnerTable::__FT__" );

    IDE_FT_ERROR( ( ( aNode->lflag & QTC_NODE_JOIN_TYPE_MASK )
                    == QTC_NODE_JOIN_TYPE_SEMI ) ||
                  ( ( aNode->lflag & QTC_NODE_JOIN_TYPE_MASK )
                    == QTC_NODE_JOIN_TYPE_ANTI ) );

    sTable = qtc::getPosFirstBitSet( &aNode->depInfo );
    while( sTable != QTC_DEPENDENCIES_END )
    {
        sFrom = QC_SHARED_TMPLATE( aStatement )->tableMap[sTable].from;

        if( qtc::dependencyContains( &sFrom->semiAntiJoinDepInfo, &aNode->depInfo ) == ID_TRUE )
        {
            *aTableID = sTable;
            break;
        }
        else
        {
            // Nothing to do.
        }
        sTable = qtc::getPosNextBitSet( &aNode->depInfo, sTable );
    }

    IDE_FT_ERROR( sTable != QTC_DEPENDENCIES_END );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::makeSemiAntiJoinRelationList( qcStatement      * aStatement,
                                         qtcNode          * aNode,
                                         qmoJoinRelation ** aJoinRelationList,
                                         UInt             * aJoinRelationCnt )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery Unnesting
 *     makeJoinRelationList�� ���� ������ �ϴ� semi/anti join�� �Լ��̴�.
 *     makeJoinRelationList�� ���� ���������� ������ ���� ��Ȳ����
 *     �ٸ��� ������ �� �ִ�.
 *     ex) T1.I1 + T3.I1 = T2.I1, �� �� inner table�� T3�� ���
 *         T1 -> T3, T2 -> T3 �� �� ���� join relation�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoJoinRelation     * sLastJoinRelation;
    qmoJoinRelation     * sNewJoinRelation;
    qcDepInfo             sDepInfo;
    qcDepInfo             sInnerDepInfo;
    qmoJoinRelationType   sJoinType;
    UInt                  sJoinRelationCnt;
    SInt                  sInnerTable;
    SInt                  sTable;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::makeSemiAntiJoinRelationList::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinRelationCnt  = *aJoinRelationCnt;
    sLastJoinRelation = *aJoinRelationList;

    // aJoinRelationList�� ������ node�� ã�´�.
    if( sLastJoinRelation != NULL )
    {
        while( sLastJoinRelation->next != NULL )
        {
            sLastJoinRelation = sLastJoinRelation->next;
        }
    }
    else
    {
        // Nothing to do.
    }

    switch( aNode->lflag & QTC_NODE_JOIN_TYPE_MASK )
    {
        case QTC_NODE_JOIN_TYPE_SEMI:
            sJoinType = QMO_JOIN_RELATION_TYPE_SEMI;
            break;
        case QTC_NODE_JOIN_TYPE_ANTI:
            sJoinType = QMO_JOIN_RELATION_TYPE_ANTI;
            break;
        default:
            // Semi/anti join �� �� �� ����.
            IDE_FT_ERROR( 0 );
    }

    IDE_TEST( getInnerTable( aStatement, aNode, &sInnerTable )
              != IDE_SUCCESS );

    qtc::dependencySet( (UShort)sInnerTable, &sInnerDepInfo );

    sTable = qtc::getPosFirstBitSet( &aNode->depInfo );
    while( sTable != QTC_DEPENDENCIES_END )
    {
        if( sTable != sInnerTable )
        {
            qtc::dependencySet( (UShort)sTable, &sDepInfo );

            qtc::dependencyOr( &sDepInfo, &sInnerDepInfo, &sDepInfo );

            if( existJoinRelation( *aJoinRelationList, &sDepInfo ) == ID_FALSE )
            {
                // ������ join relation�� �������� ������ ���� ����

                sJoinRelationCnt++;

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinRelation ),
                                                         (void**) & sNewJoinRelation )
                          != IDE_SUCCESS );

                qtc::dependencySetWithDep( &sNewJoinRelation->depInfo,
                                           &sDepInfo );

                sNewJoinRelation->joinType      = sJoinType;
                sNewJoinRelation->innerRelation = (UShort)sInnerTable;
                sNewJoinRelation->next          = NULL;

                // Join Relation ����
                if ( *aJoinRelationList == NULL )
                {
                    *aJoinRelationList = sNewJoinRelation;
                }
                else
                {
                    sLastJoinRelation->next = sNewJoinRelation;
                }
                sLastJoinRelation = sNewJoinRelation;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }

        sTable = qtc::getPosNextBitSet( &aNode->depInfo, sTable );
    }

    *aJoinRelationCnt  = sJoinRelationCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::makeCrossJoinRelationList( qcStatement      * aStatement,
                                      qmoJoinGroup     * aJoinGroup,
                                      qmoJoinRelation ** aJoinRelationList,
                                      UInt             * aJoinRelationCnt )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery unnesting
 *     Semi/anti join�� outer relation�� ������ ��� �� outer relation��
 *     cartesian product�� �����ϵ��� join�� �����Ѵ�.
 *
 * Implementation :
 *     Semi/anti join�� inner relation�� ã�� outer relation���� depInfo��
 *     ���� �� join�� �����Ѵ�.
 *     �� �� cartesian product�� ����� �Ǵ� �� relation�� �̹�
 *     ANSI style�� join�Ǿ� �ִ� ��쿡�� �������� �ʵ��� �Ѵ�.
 *
 ***********************************************************************/

    qmsFrom   * sFrom;
    qcDepInfo   sDepInfo;
    SInt        sJoinGroupTable;
    SInt        sOuterTable;
    SInt        sLastTable;
    UInt        i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::makeCrossJoinRelationList::__FT__" );

    sJoinGroupTable = qtc::getPosFirstBitSet( &aJoinGroup->depInfo );
    while( sJoinGroupTable != QTC_DEPENDENCIES_END )
    {
        sFrom = QC_SHARED_TMPLATE( aStatement )->tableMap[sJoinGroupTable].from;

        if( qtc::haveDependencies( &sFrom->semiAntiJoinDepInfo ) == ID_TRUE )
        {
            // Semi/anti join�� ���

            sLastTable = -1;
            sOuterTable = qtc::getPosFirstBitSet( &sFrom->semiAntiJoinDepInfo );
            while( sOuterTable != QTC_DEPENDENCIES_END )
            {
                if( sOuterTable != sFrom->tableRef->table )
                {
                    if( sLastTable != -1 )
                    {
                        sDepInfo.depCount = 2;
                        sDepInfo.depend[0] = sLastTable;
                        sDepInfo.depend[1] = sOuterTable;

                        // Cartesian product ���� ����� �� relation�� ansi style�� join�Ǿ��ִٸ�
                        // �������� �ʰ� �����ؾ� �Ѵ�.
                        for( i = 0; i < aJoinGroup->baseGraphCnt; i++ )
                        {
                            // Cartesian product ����� �� relation�� ��� �ϳ��� base graph��
                            // �����ִٸ� �̹� ANSI style�� join�� ����̴�.
                            if( qtc::dependencyContains( &aJoinGroup->baseGraph[i]->depInfo,
                                                         &sDepInfo ) == ID_TRUE )
                            {
                                break;
                            }
                            else
                            {
                                // Nothing to do.
                            }
                        }

                        if( i == aJoinGroup->baseGraphCnt )
                        {
                            IDE_TEST( makeJoinRelationList( aStatement,
                                                            &sDepInfo,
                                                            aJoinRelationList,
                                                            aJoinRelationCnt )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // ANSI style�� join�� ����̹Ƿ� ����
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    sLastTable = sOuterTable;
                }
                else
                {
                    // Nothing to do.
                }
                sOuterTable = qtc::getPosNextBitSet( &sFrom->semiAntiJoinDepInfo, sOuterTable );
            }
        }
        else
        {
            // Inner join�� ���
        }
        sJoinGroupTable = qtc::getPosNextBitSet( &aJoinGroup->depInfo, sJoinGroupTable );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::makeJoinRelationList( qcStatement      * aStatement,
                                 qcDepInfo        * aDependencies,
                                 qmoJoinRelation ** aJoinRelationList,
                                 UInt             * aJoinRelationCnt )
{
/***********************************************************************
 *
 * Description : qmoJoinRelation�� ���� �� �ʱ�ȭ
 *
 ***********************************************************************/

    qmoJoinRelation * sCurJoinRelation  = NULL;  // for traverse
    qmoJoinRelation * sLastJoinRelation = NULL;  // for traverse
    qmoJoinRelation * sNewJoinRelation  = NULL;  // ���� ������ join relation
    idBool            sExistCommon;
    UInt              sJoinRelationCnt;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::makeJoinRelationList::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDependencies != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sExistCommon      = ID_FALSE;
    sJoinRelationCnt  = *aJoinRelationCnt;

    //------------------------------------------
    // ���� Join Relation ���� ���� �˻�
    // ( Join Relation List�� �������� sLastJoinRelation�� ����Ű���� ���� )
    //------------------------------------------

    sCurJoinRelation = *aJoinRelationList;

    while ( sCurJoinRelation != NULL )
    {
        if ( qtc::dependencyEqual( & sCurJoinRelation->depInfo,
                                   aDependencies )
             == ID_TRUE )
        {
            sExistCommon = ID_TRUE;
        }
        sLastJoinRelation = sCurJoinRelation;
        sCurJoinRelation = sCurJoinRelation->next;
    }

    if ( sExistCommon == ID_FALSE )
    {
        //------------------------------------------
        // ������ Join Relation�� �������� ������,
        // Join Relation ����
        //------------------------------------------

        sJoinRelationCnt++;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinRelation ),
                                                 (void**) & sNewJoinRelation )
                     != IDE_SUCCESS );

        qtc::dependencyClear( & sNewJoinRelation->depInfo );
        IDE_TEST( qtc::dependencyOr( & sNewJoinRelation->depInfo,
                                     aDependencies,
                                     & sNewJoinRelation->depInfo )
                  != IDE_SUCCESS );

        sNewJoinRelation->joinType = QMO_JOIN_RELATION_TYPE_INNER;
        sNewJoinRelation->innerRelation = ID_USHORT_MAX;
        sNewJoinRelation->next = NULL;

        // Join Relation ����
        if ( *aJoinRelationList == NULL )
        {
            *aJoinRelationList = sNewJoinRelation;
        }
        else
        {
            sLastJoinRelation->next = sNewJoinRelation;
        }
    }
    else
    {
        // ������ Join Relation�� �����ϴ� ���
        // nothing to do
    }

    *aJoinRelationCnt  = sJoinRelationCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::isTwoConceptTblNode( qtcNode       * aNode,
                                qmgGraph     ** aBaseGraph,
                                UInt            aBaseGraphCnt,
                                idBool        * aIsTwoConceptTblNode)
{
/***********************************************************************
 *
 * Description : �� ���� conceptual table�θ� ������ Node ����
 *
 * Implementation :
 *     (1) ù��° ���� Graph�� ã�´�.
 *     (2) �ι�° ���� Graph�� ã�´�.
 *     (3) �� ���� Concpetual Table���� ���õ� Node���� �˻��Ѵ�.
 *         Node & ~( ù��° Graph | �� ��° Graph ) == 0
 *
 ***********************************************************************/

    qtcNode   * sNode;
    qmgGraph  * sFirstGraph;
    qmgGraph  * sSecondGraph;
    qcDepInfo   sResultDependencies;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::isTwoConceptTblNode::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aBaseGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNode         = aNode;

    // sNode�� ������ �� ���� Graph�� ã�´�.
    IDE_TEST( getTwoRelatedGraph( & sNode->depInfo,
                                  aBaseGraph,
                                  aBaseGraphCnt,
                                  & sFirstGraph,
                                  & sSecondGraph )
              != IDE_SUCCESS );

    //------------------------------------------
    // �� ���� Concpetual Table���� ���õ� Node���� �˻�
    //------------------------------------------

    if ( sFirstGraph != NULL && sSecondGraph != NULL )
    {
        // Predicate & ~( First Graph | Seconde Graph )
        IDE_TEST( qtc::dependencyOr( & sFirstGraph->depInfo,
                                     & sSecondGraph->depInfo,
                                     & sResultDependencies )
                  != IDE_SUCCESS );

        if ( qtc::dependencyContains( & sResultDependencies,
                                      & sNode->depInfo ) == ID_TRUE)
        {
            // Predicate & ~( First Graph | Seconde Graph ) == 0 �̸�,
            // �ΰ��� conceptual table�� ���õ� Predicate
            *aIsTwoConceptTblNode = ID_TRUE;
        }
        else
        {
            *aIsTwoConceptTblNode = ID_FALSE;
        }
    }
    else
    {
        *aIsTwoConceptTblNode = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCnfMgr::getTwoRelatedGraph( qcDepInfo * aDependencies,
                               qmgGraph ** aBaseGraph,
                               UInt        aBaseGraphCnt,
                               qmgGraph ** aFirstGraph,
                               qmgGraph ** aSecondGraph )
{
/***********************************************************************
 *
 * Description : base graph�� �߿��� ������ �� ���� graph�� ��ȯ
 *
 * Implementation :
 *
 ***********************************************************************/

    qcDepInfo        * sDependencies;
    UInt               sBaseGraphCnt;
    qmgGraph        ** sBaseGraph;
    qmgGraph         * sCurGraph;
    qmgGraph         * sFirstGraph;
    qmgGraph         * sSecondGraph;
    UInt               i;
    qcDepInfo          sAndDependencies;
    qcDepInfo          sGraphDepInfo;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::getTwoRelatedGraph::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aDependencies != NULL );
    IDE_DASSERT( aBaseGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sDependencies = aDependencies;
    sBaseGraph    = aBaseGraph;
    sBaseGraphCnt = aBaseGraphCnt;
    sFirstGraph   = NULL;
    sSecondGraph  = NULL;


    for ( i = 0; i < sBaseGraphCnt; i++ )
    {
        sCurGraph = sBaseGraph[i];

        sGraphDepInfo = sCurGraph->depInfo;

        qtc::dependencyAnd( & sGraphDepInfo,
                            sDependencies,
                            & sAndDependencies );

        if ( qtc::dependencyEqual( & sAndDependencies,
                                   & qtc::zeroDependencies ) == ID_FALSE )
        {
            //------------------------------------------
            // ( Node�� dependencies & Graph�� dependencies ) != 0 �̸�,
            // ���� Graph�� Node�� �����ִ� Graph�̴�.
            //------------------------------------------

            if ( sFirstGraph == NULL )
            {
                sFirstGraph = sCurGraph;
            }
            else
            {
                sSecondGraph = sCurGraph;
                break;
            }
        }
        else
        {
            // nothing to do
        }
    }

    *aFirstGraph = sFirstGraph;
    *aSecondGraph = sSecondGraph;

    return IDE_SUCCESS;
}

IDE_RC qmoCnfMgr::pushSelection4ConstantFilter(qcStatement *aStatement,
                                               qmgGraph    *aGraph,
                                               qmoCNF      *aCNF)
{
    /*
     * BUG-29551: aggregation�� �ִ� view�� constant filter��
     *            pushdown�ϸ� ���� ����� Ʋ���ϴ�
     *
     * �Ϲ� filter �� push selection ���� �ٸ���
     * const filter �� ���ؼ��� ������ �ֻ��� ��忡 filter �� �����Ѵ�
     * �׷��� �ϸ� ���� plan �� �����ϱ� ���� �̸� ���� ���� ������ �� �� �ִ�
     */
    qmgGraph     * sCurGraph = NULL;
    qmoPredicate * sLastConstantPred = NULL;
    qmoPredicate * sTempConstantPred = NULL;
    qmoPredicate * sSrcConstantPred = NULL;
    qmgChildren  * sChildren = NULL;
    idBool         sFind     = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::pushSelection4ConstantFilter::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT(aGraph != NULL);
    IDE_DASSERT(aCNF   != NULL);

    sCurGraph = aGraph;

    if (aCNF->constantPredicate == NULL)
    {
        IDE_CONT(NORMAL_EXIT);
    }
    else
    {
    }

    while (sCurGraph != NULL )
    {
        // fix BUG-9791, BUG-10419
        // fix BUG-19093

        if (sCurGraph->left == NULL)
        {
            break;
        }
        else
        {
            // Nothing to do
        }

        if (sCurGraph->type == QMG_SET)
        {
            // fix BUG-19093
            // constant filter�� SET�� ������ ������.
            // BUG-36803 sCurGraph->left must not be null!
            IDE_TEST( pushSelection4ConstantFilter( aStatement,
                                                    sCurGraph->left,
                                                    aCNF )
                      != IDE_SUCCESS );

            if( sCurGraph->right != NULL )
            {
                IDE_TEST( pushSelection4ConstantFilter( aStatement,
                                                        sCurGraph->right,
                                                        aCNF )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do
            }

            for (sChildren = sCurGraph->children;
                 sChildren != NULL;
                 sChildren = sChildren->next)
            {
                IDE_TEST( pushSelection4ConstantFilter( aStatement,
                                                        sChildren->childGraph,
                                                        aCNF )
                          != IDE_SUCCESS );
            }
            break;
        }
        else
        {
            /*
             * ���� graph �� selection �� ���
             * left graph �� view materialization �� ���,
             * ���� graph �� full, left outer join �� ���,
             * constant filter�� �����Ѵ�.
             */
            if ( ( sCurGraph->type == QMG_SELECTION ) ||
                 ( sCurGraph->type == QMG_PARTITION ) )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }

            if (sCurGraph->myFrom != NULL)
            {
                // BUG-47772 sCurGraph->left�� PROJ �׷������� �Ѵ�.
                if ((((sCurGraph->left->flag & QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK)
                      == QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE ) &&
                     (sCurGraph->left->type == QMG_PROJECTION)) ||
                    (sCurGraph->type == QMG_FULL_OUTER_JOIN) ||
                    (sCurGraph->type == QMG_LEFT_OUTER_JOIN))
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }

                /* PROJ-1715
                 * Hierarchy�� ��� �ܺ� ������ ���� constantPredicate�� ���
                 * SCAN���� ������ �ʵǰ� �������� ó���ؾ��Ѵ�.
                 */
                if ( ( ( sCurGraph->left->flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK )
                       == QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE ) &&
                     ( sCurGraph->left->type == QMG_PROJECTION ) )
                {
                    sFind = ID_FALSE;
                    for ( sSrcConstantPred = aCNF->constantPredicate;
                          sSrcConstantPred != NULL;
                          sSrcConstantPred = sSrcConstantPred->next )
                    {
                        if ( qtc::dependencyEqual( &sSrcConstantPred->node->depInfo,
                                                   &qtc::zeroDependencies )
                             != ID_TRUE )
                        {
                            sFind = ID_TRUE;
                            break;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }

                    if ( sFind == ID_TRUE )
                    {
                        break;
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
            }
        }

        sCurGraph = sCurGraph->left;
    }

    // constant predicate ����
    if( sCurGraph != NULL )
    {
        // fix BUG-19212
        // subquery�� ������ �������� �ʰ�, �ּҸ� �޾Ƴ��´�.
        // SET���� �������� (outer column reference�� �ִ� subquery)��
        // ������ ���� ����.
        // outer column ref.�� �ִ� subquery�� view��
        // ������ ���ϵ��� ���� while loop���� ó���߱� �����̴�.

        if( ( aCNF->constantPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
            == QTC_NODE_SUBQUERY_EXIST )
        {
            if( sCurGraph->constantPredicate == NULL )
            {
                sCurGraph->constantPredicate = aCNF->constantPredicate;
            }
            else
            {
                // ���� graph�� �Ǵٸ� constant filter�� �ִ� ���
                // constant filter�� ������豸��
                for( sLastConstantPred = sCurGraph->constantPredicate;
                     sLastConstantPred->next != NULL;
                     sLastConstantPred = sLastConstantPred->next ) ;

                sLastConstantPred->next = aCNF->constantPredicate;
            }
        }
        else
        {
            // constant predicate�� �����ؼ� ������.

            for( sSrcConstantPred = aCNF->constantPredicate;
                 sSrcConstantPred != NULL;
                 sSrcConstantPred = sSrcConstantPred->next )
            {
                // Predicate ����
                IDE_TEST( qmoPred::copyOneConstPredicate( QC_QMP_MEM( aStatement ),
                                                          sSrcConstantPred,
                                                          & sTempConstantPred )
                          != IDE_SUCCESS );

                if( sCurGraph->constantPredicate == NULL )
                {
                    sCurGraph->constantPredicate = sTempConstantPred;
                }
                else
                {
                    for( sLastConstantPred = sCurGraph->constantPredicate;
                         sLastConstantPred->next != NULL;
                         sLastConstantPred = sLastConstantPred->next ) ;

                    sLastConstantPred->next = sTempConstantPred;
                }
            }
        }
    }
    else
    {
        IDE_DASSERT(0);
    }

    IDE_EXCEPTION_CONT(NORMAL_EXIT);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::getPushPredPosition( qmoPredicate  * aPredicate,
                                qmgGraph     ** aBaseGraph,
                                qcDepInfo     * aFromDependencies,
                                qmsJoinType     aJoinType,
                                UInt          * aPredPos )
{
/***********************************************************************
 *
 * Description : Push Selection�� Predicate�� ���� Base Graph�� ��ġ�� ã��
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;
    qmgGraph     ** sBaseGraph;
    UInt            sPredPos;
    qcDepInfo     * sGraphDependencies;
    idBool          sIsOneTablePred;
    UInt            i;
    qmoPushApplicableType sPushApplicable;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::getPushPredPosition::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aBaseGraph != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sPredicate      = aPredicate;
    sBaseGraph      = aBaseGraph;
    sPredPos        = ID_UINT_MAX;
    sIsOneTablePred = ID_FALSE;

    //------------------------------------------
    // To Fix BUG-10806
    // Push Selection ���� Predicate ���� �˻�
    //------------------------------------------

    sPushApplicable = isPushApplicable(aPredicate, aJoinType);

    if ( sPushApplicable != QMO_PUSH_APPLICABLE_FALSE )
    {
        for ( i = 0 ; i < 2; i++ )
        {
            sGraphDependencies = & sBaseGraph[i]->depInfo;

            IDE_TEST( qmoPred::isOneTablePredicate( sPredicate,
                                                    aFromDependencies,
                                                    sGraphDependencies,
                                                    & sIsOneTablePred )
                      != IDE_SUCCESS );

            if ( sIsOneTablePred == ID_TRUE )
            {
                if ((sPushApplicable == QMO_PUSH_APPLICABLE_TRUE) ||
                    ((sPushApplicable == QMO_PUSH_APPLICABLE_LEFT) && (i == 0)) ||
                    ((sPushApplicable == QMO_PUSH_APPLICABLE_RIGHT) && (i == 1)))
                {
                    sPredPos = i;
                    break;
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

    *aPredPos = sPredPos;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


qmoPushApplicableType qmoCnfMgr::isPushApplicable( qmoPredicate * aPredicate,
                                                   qmsJoinType    aJoinType )
{
/***********************************************************************
 *
 * Description : Push Selection ������ predicate���� �˻�
 *
 * Implementation :
 *    (1) Function Predicate�� Push Down ���� ���� �˻�
 *
 ***********************************************************************/

    qmoPushApplicableType sResult;

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    /* --------------------------------------------------------------------
        * fix BUG-22512 Outer Join �迭 Push Down Predicate �� ���, ��� ����
        * Node�� Push Down �� �� ���� Function��
        * �����ϴ� ���, Predicate�� ������ �ʴ´�.
        * Left/Full Outer Join�� Null Padding������
        * Push Selection �ϸ� Ʋ�� ����� �����
        *
        * BUG-41343
        * EAT_NULL�� �־ null padding�� ���� �ʴ� relation�� predicate �̶��
        * push selection�� �����ϴ�
        * --------------------------------------------------------------------
        */
    // BUG-44805 �������� Push Selection �� �߸��ϰ� �ֽ��ϴ�.
    // ���������� EAT_NULL �Ӽ��� ������ �����Ƿ� ���� ó���ؾ� �Ѵ�.
    if ( ((aPredicate->node->node.lflag & MTC_NODE_EAT_NULL_MASK) == MTC_NODE_EAT_NULL_TRUE) ||
         ((aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK) == QTC_NODE_SUBQUERY_EXIST) )
    {
        switch (aJoinType)
        {
            case QMS_INNER_JOIN:
                sResult = QMO_PUSH_APPLICABLE_TRUE;
                break;
            case QMS_LEFT_OUTER_JOIN:
                sResult = QMO_PUSH_APPLICABLE_LEFT;
                break;
            case QMS_RIGHT_OUTER_JOIN:
                sResult = QMO_PUSH_APPLICABLE_RIGHT;
                break;
            case QMS_FULL_OUTER_JOIN:
                sResult = QMO_PUSH_APPLICABLE_FALSE;
                break;
            default:
                IDE_DASSERT(0);
                sResult = QMO_PUSH_APPLICABLE_FALSE;
                break;
        }
    }
    else
    {
        sResult = QMO_PUSH_APPLICABLE_TRUE;
    }

    return sResult;
}

IDE_RC
qmoCnfMgr::generateTransitivePredicate( qcStatement * aStatement,
                                        qmoCNF      * aCNF,
                                        idBool        aIsOnCondition )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     where���� ���� Transitive Predicate ����
 *
 ***********************************************************************/

    qmsQuerySet * sQuerySet;
    qtcNode     * sNode;
    qtcNode     * sTransitiveNode = NULL;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::generateTransitivePredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sQuerySet = aCNF->myQuerySet;

    //------------------------------------------
    // where���� ���� Transitive Predicate ����
    //------------------------------------------

    if ( ( sQuerySet->SFWGH->hints->transitivePredType
           == QMO_TRANSITIVE_PRED_TYPE_ENABLE ) &&
         ( QCU_OPTIMIZER_TRANSITIVITY_DISABLE == 0 ) )
    {
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_TRANSITIVITY_OLD_RULE );
        if ( aCNF->normalCNF != NULL )
        {
            sNode = (qtcNode*) aCNF->normalCNF->node.arguments;

            IDE_TEST( qmoTransMgr::processTransitivePredicate( aStatement,
                                                               sQuerySet,
                                                               sNode,
                                                               aIsOnCondition,
                                                               & sTransitiveNode )
                      != IDE_SUCCESS );

            if ( sTransitiveNode != NULL )
            {
                // ������ transitive predicate�� where���� �����Ѵ�.
                IDE_TEST( qmoTransMgr::linkPredicate( sTransitiveNode,
                                                      & sNode )
                          != IDE_SUCCESS );

                // estimate
                IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                            aCNF->normalCNF )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
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
qmoCnfMgr::generateTransitivePred4OnCondition( qcStatement   * aStatement,
                                               qmoCNF        * aCNF,
                                               qmoPredicate  * aUpperPred,
                                               qmoPredicate ** aLowerPred )
{
/***********************************************************************
 *
 * Description : PROJ-1404 Transitive Predicate Generation
 *
 * Implementation :
 *     (1) on���� ���� Transitive Predicate ����
 *     (2) upper predicate�� on�� predicate���� lower predicate ����
 *
 ***********************************************************************/

    qmsQuerySet  * sQuerySet;
    qtcNode      * sNode;
    qtcNode      * sTransitiveNode = NULL;
    qmoPredicate * sLowerPred = NULL;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::generateTransitivePred4OnCondition::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );
    IDE_DASSERT( aLowerPred != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sQuerySet = aCNF->myQuerySet;

    //------------------------------------------
    // onCondition�� ���� Transitive Predicate ����
    //------------------------------------------

    // where���� �����ϰ� ����
    IDE_TEST( generateTransitivePredicate( aStatement,
                                           aCNF,
                                           ID_TRUE )
              != IDE_SUCCESS );

    //------------------------------------------
    // upper predicate(where������ ������ predicate)�� on���� ����
    // Transitive Predicate ����
    //------------------------------------------

    if ( ( sQuerySet->SFWGH->hints->transitivePredType
           == QMO_TRANSITIVE_PRED_TYPE_ENABLE ) &&
         ( QCU_OPTIMIZER_TRANSITIVITY_DISABLE == 0 ) )
    {
        if ( aCNF->normalCNF != NULL )
        {
            sNode = (qtcNode*) aCNF->normalCNF->node.arguments;

            IDE_TEST( qmoTransMgr::processTransitivePredicate( aStatement,
                                                               sQuerySet,
                                                               sNode,
                                                               aUpperPred,
                                                               & sTransitiveNode )
                      != IDE_SUCCESS );

            if ( sTransitiveNode != NULL )
            {
                // ������ transitive predicate�� qmoPredicate ���·� �����Ѵ�.
                IDE_TEST( qmoTransMgr::linkPredicate( aStatement,
                                                      sTransitiveNode,
                                                      & sLowerPred )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    *aLowerPred = sLowerPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCnfMgr::optimizeSubQ4OnCondition( qcStatement * aStatement,
                                     qmoCNF      * aCNF )
{
/***********************************************************************
 *
 * Description :
 *
 * ------------------------------------------
 * fix BUG-19203
 * �� predicate ���� subquery�� ���� optimize�� �Ѵ�.
 * selection graph�� optimize���� view�� ���� ��� ������ ����Ǳ� ������
 * left, right graph�� ����ȭ�� ����� ���Ŀ� �ϴ� ���� �´�.
 * ------------------------------------------
 *
 * ---------------------------------------------
 * on Condition CNF�� �� Predicate�� subuqery ó��
 * on Condition CNF�� �� Predicate�� �ش� graph�� myPredicate�� �������
 * �ʾ� �ش� graph�� myPredicateó�� �ÿ� subquery�� �������� �ʴ´�.
 * ���� on Condition CNF�� predicate �з� �Ŀ� subquery�� graph��
 * �������ش�.
 * ( To Fix BUG-8742 )
 * ---------------------------------------------
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::optimizeSubQ4OnCondition::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // Display ������ ����
    //------------------------------------------

    if ( aCNF->constantPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               aCNF->constantPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    // To Fix PR-12743
    // NNF Filter ����
    if ( aCNF->nnfFilter != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueryInNode( aStatement,
                                                   aCNF->nnfFilter,
                                                   ID_FALSE,  //No Range
                                                   ID_FALSE ) //No ConstantFilter
                  != IDE_SUCCESS );

    }
    else
    {
        // nothing to do
    }

    if ( aCNF->oneTablePredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               aCNF->oneTablePredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    if ( aCNF->joinPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               aCNF->joinPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoCnfMgr::discardLateralViewJoinPred( qmoCNF        * aCNF,
                                              qmoPredicate ** aDiscardPredList )
{
/***********************************************************************
 *
 * Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 *  
 *   Lateral View�� Join Predicate�� ��� ���ܽ��Ѽ�,
 *   Join Grouping ������ Lateral View�� �������� ���ϵ��� �Ѵ�.
 *
 * Implementation :
 *  
 *   1) Predicate�� ���������� Ž���ϸ鼭, Predicate�� ���õ� Base Graph��
 *      lateralDepInfo�� ���Ѵ�.
 *   2) lateralDepInfo�� �����ϸ�, �ش� Predicate�� Lateral View�� ���õ�
 *      Predicate �̹Ƿ�, joinPredicateList���� ���� discardPredList�� �ִ´�.
 *   3) ��� Predicate�� ���� (1)-(2) ������ �ݺ��Ѵ�.
 *   4) discardPredList�� ��ȯ�Ѵ�.
 *
***********************************************************************/

    qmoPredicate * sCurrPred        = NULL;
    qmoPredicate * sJoinPredHead    = NULL;
    qmoPredicate * sJoinPredTail    = NULL;
    qmoPredicate * sDiscardPredHead = NULL;
    qmoPredicate * sDiscardPredTail = NULL;
    idBool         sInLateralView   = ID_FALSE;
    qcDepInfo      sBaseLateralDepInfo;
    UInt           sBaseGraphCnt;
    qmgGraph    ** sBaseGraph;
    UInt           i;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::discardLateralViewJoinPred::__FT__" );

    sBaseGraphCnt    = aCNF->graphCnt4BaseTable;
    sBaseGraph       = aCNF->baseGraph;

    // �ʱ�ȭ
    sCurrPred        = aCNF->joinPredicate;
    sJoinPredHead    = aCNF->joinPredicate;

    // Predicate depInfo�� �����ϴ� Base Graph��
    // lateralDepInfo�� ������ �ִ��� �˻��Ѵ�.
    while ( sCurrPred != NULL )
    {
        for ( i = 0; i < sBaseGraphCnt; i++ )
        {
            if ( qtc::dependencyContains( & sCurrPred->node->depInfo,
                                          & sBaseGraph[i]->depInfo ) == ID_TRUE )
            {
                IDE_TEST( qmg::getGraphLateralDepInfo( sBaseGraph[i],
                                                       & sBaseLateralDepInfo )
                          != IDE_SUCCESS );

                if ( qtc::haveDependencies( & sBaseLateralDepInfo ) == ID_TRUE )
                {
                    sInLateralView = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sInLateralView == ID_TRUE )
        {
            // Predicate�� Lateral View�� �����ִ�.
            if ( sJoinPredTail == NULL )
            {
                // Lateral View�� ���� ���� Predicate�� ���� ������ ���� ���
                sJoinPredHead = sCurrPred->next;
            }
            else
            {
                // Lateral View�� ���� ���� Predicate�� ������ ���� ���
                sJoinPredTail->next = sCurrPred->next;
            }

            if ( sDiscardPredTail == NULL )
            {
                // Lateral View Predicate�� ó�� ���� ���
                sDiscardPredHead = sCurrPred;
                sDiscardPredTail = sCurrPred;
            }
            else
            {
                // Lateral View Predicate�� ó���� �ƴ� ���
                sDiscardPredTail->next = sCurrPred;
                sDiscardPredTail       = sDiscardPredTail->next;
            }
        }
        else
        {
            // Lateral View�� ���þ��� Predicate�� ���
            sJoinPredTail = sCurrPred;
        }

        // �ʱ�ȭ
        sInLateralView  = ID_FALSE;
        sCurrPred       = sCurrPred->next;
    }

    // ����Ʈ ��ȯ
    aCNF->joinPredicate = sJoinPredHead;
    *aDiscardPredList   = sDiscardPredHead;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoCnfMgr::validateLateralViewJoin( qcStatement   * aStatement,
                                           qmsFrom       * aFrom )
{
/***********************************************************************
 *
 * Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 *
 *   ������ ���� ������ �� ����. �����޽����� ����ؾ� �Ѵ�.
 *   - Left-Outer Join�� ������ LView, �������� LView�� �����ϴ� Relation
 *   - Full-Outer Join�� �� ���� LView, �ٸ� ���� LView�� �����ϴ� Relation
 *
 *   qmv�� �ƴ� qmo���� validation�� �����ϴ� ������,
 *   qmo���� Oracle-style Outer Operator�� ANSI-Join���� ��ȯ�ϱ� �����̸�
 *   ���� ANSI-Join�� �Բ� �ѹ��� validation �ϱ� ���ؼ��̴�.
 *
 * Implementation :
 *
 *   - Left��  Join Tree�� �ִٸ�, Left��  ���ؼ� ���ȣ��
 *   - Right�� Join Tree�� �ִٸ�, Right�� ���ؼ� ���ȣ��
 *   - Left / Right���� ���� lateralDepInfo�� ���� Right/Full Outer ����
 *
***********************************************************************/

    qcDepInfo          sLateralDepInfo;
    qcDepInfo          sLeftLateralDepInfo;
    qcDepInfo          sRightLateralDepInfo;

    IDU_FIT_POINT_FATAL( "qmoCnfMgr::validateLateralViewJoin::__FT__" );

    qtc::dependencyClear( & sLeftLateralDepInfo  );
    qtc::dependencyClear( & sRightLateralDepInfo );

    if ( aFrom->joinType != QMS_NO_JOIN )
    {
        IDE_DASSERT( aFrom->left  != NULL );
        IDE_DASSERT( aFrom->right != NULL );

        if ( aFrom->left->joinType != QMS_NO_JOIN )
        {
            // LEFT�� ���� Validation
            IDE_TEST( validateLateralViewJoin( aStatement,
                                               aFrom->left )
                      != IDE_SUCCESS );
        }
        else
        {
            // LEFT�� lateralDepInfo ���
            IDE_TEST( qmvQTC::getFromLateralDepInfo( aFrom->left,
                                                     & sLeftLateralDepInfo )
                      != IDE_SUCCESS );
        }

        if ( aFrom->right->joinType != QMS_NO_JOIN )
        {
            // RIGHT�� ���� Validation
            IDE_TEST( validateLateralViewJoin( aStatement,
                                               aFrom->right )
                      != IDE_SUCCESS );
        }
        else
        {
            // RIGHT�� lateralDepInfo ���
            IDE_TEST( qmvQTC::getFromLateralDepInfo( aFrom->right,
                                                     & sRightLateralDepInfo )
                      != IDE_SUCCESS );
        }

        // Left/Full-Outer Join�� ���, Left->Right ���� ���� ����
        if ( ( aFrom->joinType == QMS_LEFT_OUTER_JOIN ) ||
             ( aFrom->joinType == QMS_FULL_OUTER_JOIN ) )
        {
            qtc::dependencyAnd( & sLeftLateralDepInfo,
                                & aFrom->right->depInfo,
                                & sLateralDepInfo );

            // �ܺ� �����ϴ� ���, Left/Full-Outer Join�� �� �� ����.
            IDE_TEST_RAISE( qtc::haveDependencies( & sLateralDepInfo ) == ID_TRUE,
                            ERR_LVIEW_LEFT_FULL_JOIN_WITH_REF );
        }
        else
        {
            // Nothing to do.
        }

        // Full-Outer Join�� ���, Right->Left ���� ���ε� ����
        if ( aFrom->joinType == QMS_FULL_OUTER_JOIN )
        {
            qtc::dependencyAnd( & sRightLateralDepInfo,
                                & aFrom->left->depInfo,
                                & sLateralDepInfo );

            // �ܺ� �����ϴ� ���, Full-Outer Join�� �� �� ����.
            IDE_TEST_RAISE( qtc::haveDependencies( & sLateralDepInfo ) == ID_TRUE,
                            ERR_LVIEW_LEFT_FULL_JOIN_WITH_REF );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LVIEW_LEFT_FULL_JOIN_WITH_REF )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMO_NOT_ALLOWED_LVIEW_LEFT_FULL_JOIN_WITH_REF ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoCnfMgr::classifyPred4WhereHierachyJoin( qcStatement * aStatement,
                                                  qmoCrtPath  * aCrtPath,
                                                  qmgGraph    * aGraph )
{
    qmoCNF         * sCNF               = NULL;
    qtcNode        * sNode              = NULL;
    qmoPredicate   * sNewPred           = NULL;
    qcDepInfo      * sFromDependencies  = NULL;
    qcDepInfo      * sGraphDependencies = NULL;
    qmgGraph      ** sBaseGraph         = NULL;

    idBool           sIsRownum       = ID_FALSE;
    idBool           sIsConstant     = ID_FALSE;
    idBool           sIsOneTable     = ID_FALSE;    // TASK-3876 Code Sonar
    UInt             sBaseGraphCnt   = 0;
    UInt             i               = 0;

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF              = aCrtPath->crtCNF;
    sFromDependencies = &sCNF->depInfo;
    sBaseGraphCnt     = sCNF->graphCnt4BaseTable;
    sBaseGraph        = sCNF->baseGraph;

    // Predicate �з� ��� Node
    if ( sCNF->normalCNF != NULL )
    {
        sNode = (qtcNode *)sCNF->normalCNF->node.arguments;
    }
    else
    {
        sNode = NULL;
    }

    while ( sNode != NULL )
    {
        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            &sNewPred )
                  != IDE_SUCCESS );

        //-------------------------------------------------
        // Rownum Predicate �˻�
        //-------------------------------------------------

        IDE_TEST( qmoPred::isRownumPredicate( sNewPred,
                                              &sIsRownum )
                  != IDE_SUCCESS );

        if ( sIsRownum == ID_TRUE )
        {
            sNewPred->next = aCrtPath->rownumPredicate;
            aCrtPath->rownumPredicate = sNewPred;
        }
        else
        {
            //-------------------------------------------------
            // Constant Predicate �˻�
            //-------------------------------------------------

            IDE_TEST( qmoPred::isConstantPredicate( sNewPred,
                                                    sFromDependencies,
                                                    & sIsConstant )
                      != IDE_SUCCESS );

            if ( sIsConstant == ID_TRUE )
            {
                sNewPred->flag &= ~QMO_PRED_CONSTANT_FILTER_MASK;
                sNewPred->flag |= QMO_PRED_CONSTANT_FILTER_TRUE;

                //-------------------------------------------------
                // Constant Predicate �з�
                //   Hierarhcy ���� ������ ���� constant predicate,
                //   level predicate, prior predicat�� �з��Ͽ� �ش� ��ġ�� �߰�
                //-------------------------------------------------
                /* Level, isLeaf, Predicate�� ó�� */
                if ( ( (sNewPred->node->lflag & QTC_NODE_LEVEL_MASK )
                     == QTC_NODE_LEVEL_EXIST) ||
                     ( (sNewPred->node->lflag & QTC_NODE_ISLEAF_MASK )
                     == QTC_NODE_ISLEAF_EXIST) )
                {
                    sNewPred->next = aGraph->myPredicate;
                    aGraph->myPredicate = sNewPred;
                }
                else
                {
                    // Prior Predicate ó��
                    if ( ( sNewPred->node->lflag & QTC_NODE_PRIOR_MASK )
                         == QTC_NODE_PRIOR_EXIST )
                    {
                        sNewPred->next = aGraph->myPredicate;
                        aGraph->myPredicate = sNewPred;
                    }
                    else
                    {
                        // Constant Predicate ó��
                        sNewPred->next = aGraph->constantPredicate;
                        aGraph->constantPredicate = sNewPred;
                    }
                }
            }
            else
            {
                for ( i = 0 ; i < sBaseGraphCnt; i++ )
                {
                    sGraphDependencies = & sBaseGraph[i]->myFrom->depInfo;
                    IDE_TEST( qmoPred::isOneTablePredicate( sNewPred,
                                                            sFromDependencies,
                                                            sGraphDependencies,
                                                            & sIsOneTable )
                              != IDE_SUCCESS );

                    if ( sIsOneTable == ID_TRUE )
                    {
                        break;
                    }
                    else
                    {
                        // nothing to do
                    }
                }

                if ( sIsOneTable == ID_TRUE )
                {
                    //---------------------------------------------
                    // One Table Predicate �з�
                    //    oneTablePredicate�� �ش� graph�� myPredicate�� ����
                    //    rid predicate �̸� �ش� graph�� ridPredicate�� ����
                    //---------------------------------------------
                    if ( ( sNewPred->node->lflag & QTC_NODE_COLUMN_RID_MASK ) ==
                            QTC_NODE_COLUMN_RID_EXIST )
                    {
                        sNewPred->next = aGraph->ridPredicate;
                        aGraph->ridPredicate = sNewPred;
                    }
                    else
                    {
                        sNewPred->next = aGraph->myPredicate;
                        aGraph->myPredicate = sNewPred;
                    }
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
        sNode = (qtcNode*)sNode->node.next;
    }

    //---------------------------------------------------------------------
    // BUG-34295 Join ordering ANSI style query
    //     Where ���� predicate �� outerJoinGraph �� ������
    //     one table predicate �� ã�� �̵���Ų��.
    //     outerJoinGraph �� one table predicate �� baseGraph ��
    //     dependency �� ��ġ�� �ʾƼ� predicate �з� ��������
    //     constant predicate ���� �߸� �з��ȴ�.
    //     �̸� �ٷ���� ���� sCNF->constantPredicate �� predicate �鿡��
    //     outerJoinGraph �� ���õ� one table predicate ���� ã�Ƴ���
    //     outerJoinGraph �� �̵���Ų��.
    //---------------------------------------------------------------------
    if ( sCNF->outerJoinGraph != NULL )
    {
        IDE_TEST( qmoAnsiJoinOrder::fixOuterJoinGraphPredicate( aStatement,
                                                                sCNF )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

