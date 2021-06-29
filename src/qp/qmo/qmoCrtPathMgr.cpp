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
 * $Id: qmoCrtPathMgr.cpp 89925 2021-02-03 04:40:48Z ahra.cho $
 *
 * Description :
 *     Critical Path Manager
 *
 *     FROM, WHERE�� �����Ǵ� Critical Path�� ���� ����ȭ�� �����ϰ�
 *     �̿� ���� Graph�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qcg.h>
#include <qmoCrtPathMgr.h>
#include <qmoNormalForm.h>
#include <qmgCounting.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmoOuterJoinOper.h>
#include <qmoAnsiJoinOrder.h>
#include <qmoOuterJoinElimination.h>
#include <qmoInnerJoinPushDown.h>
#include <qmv.h>
#include <qmoCostDef.h>
#include <qmgHierarchy.h>

IDE_RC
qmoCrtPathMgr::init( qcStatement * aStatement,
                     qmsQuerySet * aQuerySet,
                     qmoCrtPath ** aCrtPath )
{
/***********************************************************************
 *
 * Description : qmoCrtPath ���� �� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmoCrtPath ��ŭ �޸� �Ҵ�
 *    (2) Normalization Type ����
 *    (3) Normalization Type�� ���� qmoCNF, qmoDNF �ʱ�ȭ
 *        A. qmoCNF �Ǵ� qmoDNF �޸� �Ҵ�
 *        B. where ���� CNF �Ǵ� DNF Normalize
 *        C. qmoCnfMgr�Ǵ� qmoDnfMgr�� �ʱ�ȭ �Լ� ȣ��
 *
 ***********************************************************************/

    qmsQuerySet        * sQuerySet;
    qmoCrtPath         * sCrtPath;
    qmoNormalType        sNormalType;
    qtcNode            * sWhere;
    qmsFrom            * sFrom;
    qtcNode            * sNormalForm;
    qmoCNF             * sCNF;
    qmoDNF             * sDNF;
    // BUG-34295 Join ordering ANSI style query
    qmsFrom            * sFromArr;
    qmsFrom            * sFromTree;
    idBool               sMakeFail;
    qmsFrom            * sIter;
    qcDepInfo            sFromArrDep;
    idBool               sCrossProduct;
    qtcNode            * sNode;
    qtcNode            * sNNFFilter;
    idBool               sCNFOnly;
    idBool               sChanged;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    sQuerySet   = aQuerySet;
    sWhere      = sQuerySet->SFWGH->where;
    sFrom       = sQuerySet->SFWGH->from;

    sCrtPath    = NULL;
    sNormalForm = NULL;
    sNNFFilter  = NULL;

    // qmoCrtPath �޸� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoCrtPath) ,
                                             (void **) & sCrtPath)
              != IDE_SUCCESS);

    // qmoCrtPath �ʱ�ȭ
    sCrtPath->crtDNF     = NULL;
    sCrtPath->crtCNF     = NULL;
    sCrtPath->currentCNF = NULL;
    sCrtPath->myGraph    = NULL;

    sCrtPath->currentNormalType = QMO_NORMAL_TYPE_NOT_DEFINED;

    sCrtPath->rownumPredicateForCNF = NULL;
    sCrtPath->rownumPredicateForDNF = NULL;
    sCrtPath->rownumPredicate       = NULL;

    sCrtPath->myQuerySet = aQuerySet;
    sCrtPath->mIsOnlyNL = ID_FALSE;
    // SFWGH�� Critical Path �� ���
    sQuerySet->SFWGH->crtPath = sCrtPath;

    // BUG-36926
    // updatable view�� CNF�θ� plan�� �����Ǿ�� �Ѵ�.
    if ( ( sQuerySet->SFWGH->lflag & QMV_SFWGH_UPDATABLE_VIEW_MASK )
         == QMV_SFWGH_UPDATABLE_VIEW_TRUE )
    {
        sCNFOnly = ID_TRUE;
    }
    else
    {
        // BUG-43894
        // join�� CNF�θ� plan�� �����Ǿ�� �Ѵ�.
        if ( ( aQuerySet->SFWGH->from->next == NULL ) &&
             ( aQuerySet->SFWGH->from->joinType == QMS_NO_JOIN ) )
        {
            sCNFOnly = ID_FALSE;
        }
        else
        {
            sCNFOnly = ID_TRUE;
        }
    }

    // Normalization Type ����
    // BUG-35155 Partial CNF
    // Where ���� partial CNF ó���� ���� ������ �Լ��� ȣ���Ѵ�.
    IDE_TEST( decideNormalType4Where( aStatement,
                                      sFrom,
                                      sWhere,
                                      sQuerySet->SFWGH->hints,
                                      sCNFOnly,
                                      &sNormalType )
              != IDE_SUCCESS );

    sCrtPath->normalType = sNormalType;

    //------------------------------------------------
    // Normalization Type�� ���� qmoCNF, qmoDNF �ʱ�ȭ
    //------------------------------------------------

    switch ( sNormalType )
    {
        case QMO_NORMAL_TYPE_NNF:

            //-----------------------------------------
            // NNF �� ���
            //-----------------------------------------

            // qmoCNF �޸� �Ҵ�
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoCNF ),
                                                     (void **) & sCNF )
                      != IDE_SUCCESS);

            if ( sWhere != NULL )
            {
                sWhere->lflag &= ~QTC_NODE_NNF_FILTER_MASK;
                sWhere->lflag |= QTC_NODE_NNF_FILTER_TRUE;
            }
            else
            {
                // Nothing To Do
            }

            // critical path�� ���� NNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_NNF;

            // qmoCNF �ʱ�ȭ
            IDE_TEST( qmoCnfMgr::init( aStatement,
                                       sCNF,
                                       sQuerySet,
                                       NULL,
                                       sWhere )
                      != IDE_SUCCESS );

            sCrtPath->crtCNF = sCNF;

            break;
        case QMO_NORMAL_TYPE_CNF:

            //-----------------------------------------
            // CNF �� ���
            //-----------------------------------------

            // qmoCNF �޸� �Ҵ�
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoCNF ),
                                                     (void **) & sCNF )
                      != IDE_SUCCESS);

            // where ���� CNF Normalization
            if ( sWhere == NULL )
            {
                sNormalForm = NULL;
            }
            else
            {
                // where ���� CNF Normalization
                IDE_TEST( qmoNormalForm::normalizeCNF( aStatement,
                                                       sWhere,
                                                       &sNormalForm,
                                                       ID_TRUE, /* aIsWhere */
                                                       sQuerySet->SFWGH->hints )
                          != IDE_SUCCESS );

                // BUG-35155 Partial CNF
                // Partial CNF ���� ���ܵ� qtcNode �� NNF ���ͷ� �����.
                IDE_TEST( qmoNormalForm::extractNNFFilter4CNF( aStatement,
                                                               sWhere,
                                                               &sNNFFilter )
                          != IDE_SUCCESS );

                //-----------------------------------------
                // PROJ-1653 Outer Join Operator (+)
                //
                // Outer Join Operator �� ����ϴ� normalizedCNF �� ���
                // ANSI Join ������ �ڷᱸ���� ����
                //-----------------------------------------
                if ( ( sNormalForm->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                        == QTC_NODE_JOIN_OPERATOR_EXIST )
                {
                    // BUGBUG PROJ-1718 Subquery unnesting
                    // LEFT OUTER JOIN�� ������ WHERE���� ON���� ���ÿ� �����־� unparsing ��
                    // �ߺ��Ǿ� ��µȴ�.
                    // WHERE���� ���Ե� LEFT OUTER JOIN�� ������ ���ŵǾ�� �Ѵ�.
                    IDE_TEST( qmoOuterJoinOper::transform2ANSIJoin ( aStatement,
                                                                     sQuerySet,
                                                                     &sNormalForm )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }

            //------------------------------------------
            // BUG-38375 Outer Join Elimination OracleStyle Join
            //------------------------------------------

            IDE_TEST( qmoOuterJoinElimination::doTransform( aStatement,
                                                            aQuerySet,
                                                            & sChanged )
                      != IDE_SUCCESS );

            //------------------------------------------
            // BUG-43039 inner join push down
            //------------------------------------------

            if ( QCU_OPTIMIZER_INNER_JOIN_PUSH_DOWN == 1 )
            {
                IDE_TEST( qmoInnerJoinPushDown::doTransform( aStatement,
                                                             sQuerySet )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do
            }

            qcgPlan::registerPlanProperty(
                aStatement,
                PLAN_PROPERTY_OPTIMIZER_INNER_JOIN_PUSH_DOWN );

            // BUG-34295 Join ordering ANSI style query
            // �������
            // 1. Property �� ���� �־�� ��
            // 2. from ���� ANSI style join 1���� �����ؾ� ��
            // 3. table 1���� ���ΰ��� ó������ ����
            // 4. Right or full outer join �� ������ ó������ ����
            if( ( QCU_OPTIMIZER_ANSI_JOIN_ORDERING == 1 ) &&
                ( aQuerySet->SFWGH->from->next == NULL ) &&
                ( aQuerySet->SFWGH->from->joinType != QMS_NO_JOIN ) &&
                ( ( ( aQuerySet->SFWGH->lflag & QMV_SFWGH_JOIN_FULL_OUTER ) != QMV_SFWGH_JOIN_FULL_OUTER ) &&
                  ( ( aQuerySet->SFWGH->lflag & QMV_SFWGH_JOIN_RIGHT_OUTER ) != QMV_SFWGH_JOIN_RIGHT_OUTER ) ) )
            {
                sCrossProduct = ID_FALSE;

                qtc::dependencyClear( &sFromArrDep );

                sFromArr  = NULL;
                sFromTree = NULL;
                sMakeFail = ID_FALSE;

                IDE_TEST( qmoAnsiJoinOrder::traverseFroms( aStatement,
                                                           aQuerySet->SFWGH->from,
                                                           & sFromTree,
                                                           & sFromArr,
                                                           & sFromArrDep,
                                                           & sMakeFail )
                          != IDE_SUCCESS );

                // BUG-40028
                // qmsFrom �� tree �� �������� ġ��ģ(skewed)���°� �ƴ� ����
                // ANSI_JOIN_ORDERING �ؼ��� �ȵȴ�.
                if ( sMakeFail == ID_TRUE )
                {
                    sFromArr  = NULL;
                    sFromTree = NULL;
                }
                else
                {
                    // Nothing to do.
                }

                // sFromArr�� NULL�� ���: left outer join�� �����ϴ� ���
                // sFromTree�� NULL�� ���: inner join�� �����ϴ� ���
                // sFromArr�� NULL�� �ƴ� ���: inner join�� �ּ��� �ϳ��� �����ϴ� ���
                if( sFromArr != NULL )
                {
                    for( sIter = sFromArr; 
                         sIter != NULL; 
                         sIter = sIter->next )
                    {
                        if( sIter->onCondition != NULL )
                        {
                            if( qtc::dependencyContains( &sFromArrDep,
                                                         &sIter->onCondition->depInfo ) != ID_TRUE )
                            {
                                sCrossProduct = ID_TRUE;

                                break;
                            }
                        }

                        // fix INC000000004933
                        if( sIter->tableRef->view != NULL )
                        {
                            sCrossProduct = ID_TRUE;

                            break;
                        }
                    }

                    // check where clause
                    if ( sNormalForm != NULL )
                    {
                        sNode = (qtcNode *)sNormalForm->node.arguments;
                    }
                    else
                    {
                        sNode = NULL;
                    }

                    while( sNode != NULL )
                    {
                        // check FROM ARRAY contains sNode
                        if( sNode->depInfo.depCount > 1 )
                        {
                            sCrossProduct = ID_TRUE;

                            break;
                        }

                        sNode = (qtcNode *)sNode->node.next;
                    }

                    if( sCrossProduct != ID_TRUE )
                    {
                        qtc::dependencyClear( &aQuerySet->SFWGH->depInfo );

                        for( sIter = sFromArr;
                             sIter != NULL;
                             sIter = sIter->next )
                        {
                            qtc::dependencyOr( &aQuerySet->SFWGH->depInfo,
                                               &sIter->depInfo,
                                               &aQuerySet->SFWGH->depInfo );

                            if( sIter->onCondition != NULL )
                            {
                                if( sNormalForm == NULL )
                                {
                                    sNormalForm = sIter->onCondition;
                                }
                                else
                                {
                                    IDE_TEST( qmoNormalForm::addToMerge( sNormalForm,
                                                                         sIter->onCondition,
                                                                         & sNormalForm )
                                              != IDE_SUCCESS );
                                }
                            }
                        }

                        // ANSI style join �� from tree �� ���� �����Ѵ�.
                        aQuerySet->SFWGH->outerJoinTree = sFromTree;

                        // From ���� ����� from array �� ��ü�Ѵ�.
                        aQuerySet->SFWGH->from = sFromArr;
                        
                        /* BUG-48405 */
                        aQuerySet->lflag &= QMV_QUERYSET_ANSI_JOIN_ORDERING_MASK;
                        aQuerySet->lflag |= QMV_QUERYSET_ANSI_JOIN_ORDERING_TRUE;
                    }
                }
                else
                {
                    // pass
                }
            }
            else
            {
                // ANSI join ordering �� �� �� ����
                // Nothing to do.
            }

            // critical path�� ���� CNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_CNF;

            // qmoCNF �ʱ�ȭ
            IDE_TEST( qmoCnfMgr::init( aStatement,
                                       sCNF,
                                       sQuerySet,
                                       sNormalForm,
                                       sNNFFilter )  // BUG-35155
                      != IDE_SUCCESS );

            sCrtPath->crtCNF = sCNF;
            break;

        case QMO_NORMAL_TYPE_DNF:

            //-----------------------------------------
            // DNF �� ���
            //-----------------------------------------

            // qmoDNF �޸� �Ҵ�
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoDNF ),
                                                     (void **) & sDNF )
                      != IDE_SUCCESS);

            // where ���� DNF Normalization
            IDE_TEST( qmoNormalForm::normalizeDNF( aStatement,
                                                   sWhere,
                                                   &sNormalForm )
                      != IDE_SUCCESS );

            // critical path�� ���� DNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_DNF;

            // qmoDNF �ʱ�ȭ
            IDE_TEST( qmoDnfMgr::init( aStatement,
                                       sDNF,
                                       sQuerySet,
                                       sNormalForm )
                      != IDE_SUCCESS );

            sCrtPath->crtDNF = sDNF;
            break;

        case QMO_NORMAL_TYPE_NOT_DEFINED:

            //-----------------------------------------
            // Normal Form�� ���ǵ��� ���� ���
            //-----------------------------------------

            // qmoCNF �ʱ�ȭ
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoCNF ) ,
                                                     (void**) & sCNF )
                      != IDE_SUCCESS);

            // where ���� CNF Normalization
            IDE_TEST( qmoNormalForm::normalizeCNF( aStatement,
                                                   sWhere,
                                                   &sNormalForm,
                                                   ID_TRUE, /* aIsWhere */
                                                   sQuerySet->SFWGH->hints )
                      != IDE_SUCCESS );

            // BUG-35155 Partial CNF
            // Partial CNF ���� ���ܵ� qtcNode �� NNF ���ͷ� �����.
            IDE_TEST( qmoNormalForm::extractNNFFilter4CNF( aStatement,
                                                           sWhere,
                                                           &sNNFFilter )
                      != IDE_SUCCESS );

            // critical path�� ���� CNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_CNF;

            IDE_TEST( qmoCnfMgr::init( aStatement,
                                       sCNF,
                                       sQuerySet,
                                       sNormalForm,
                                       sNNFFilter )  // BUG-35155
                      != IDE_SUCCESS );

            sCrtPath->crtCNF = sCNF;

            // qmoDNF �ʱ�ȭ
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoDNF ) ,
                                                     (void**) & sDNF )
                      != IDE_SUCCESS);

            IDE_TEST( qmoNormalForm::normalizeDNF( aStatement,
                                                   sWhere,
                                                   &sNormalForm )
                      != IDE_SUCCESS );

            // critical path�� ���� DNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_DNF;

            IDE_TEST( qmoDnfMgr::init( aStatement,
                                       sDNF,
                                       sQuerySet,
                                       sNormalForm )
                      != IDE_SUCCESS );

            sCrtPath->crtDNF = sDNF;
            break;

        default:
            IDE_FT_ASSERT( 0 );
            break;
    }

    // critical path�� ó���� ������.
    sCrtPath->currentNormalType = QMO_NORMAL_TYPE_NOT_DEFINED;

    // out ����
    *aCrtPath = sCrtPath;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCrtPathMgr::optimize( qcStatement * aStatement,
                         qmoCrtPath  * aCrtPath )
{
/***********************************************************************
 *
 * Description : Critical Path�� ����ȭ( ��, qmoCrtPathMgr�� ����ȭ)
 *
 * Implementation :
 *    Normalization Type�� ���� qmoCnfMgf�Ǵ� qmoDnfMgr�� ����ȭ �Լ� ȣ��
 *
 ***********************************************************************/

    SDouble        sCnfCost;
    SDouble        sDnfCost;
    qmoNormalType  sNormalType;
    qmoCrtPath   * sCrtPath;
    qmsQuerySet  * sQuerySet;
    qmgGraph     * sMyGraph;
    qmsQuerySet  * sTmp;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCrtPath != NULL );

    sCrtPath = aCrtPath;
    sNormalType = sCrtPath->normalType;
    sQuerySet = sCrtPath->myQuerySet;

    //------------------------------------------
    // Normalization Form�� ���� ����ȭ
    //------------------------------------------

    switch ( sNormalType )
    {
        case QMO_NORMAL_TYPE_NNF:

            //------------------------------------------
            // NNF ����ȭ
            //------------------------------------------

            // critical path�� ���� NNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_NNF;
            sCrtPath->crtCNF->mIsOnlyNL = sCrtPath->mIsOnlyNL;
            IDE_TEST( qmoCnfMgr::optimize( aStatement,
                                           sCrtPath->crtCNF )
                      != IDE_SUCCESS );
            break;

        case QMO_NORMAL_TYPE_CNF:

            //------------------------------------------
            // CNF ����ȭ
            //------------------------------------------

            // critical path�� ���� CNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_CNF;
            sCrtPath->crtCNF->mIsOnlyNL = sCrtPath->mIsOnlyNL;
            IDE_TEST( qmoCnfMgr::optimize( aStatement,
                                           sCrtPath->crtCNF )
                      != IDE_SUCCESS );
            break;

        case QMO_NORMAL_TYPE_DNF:

            //------------------------------------------
            // DNF ����ȭ
            //------------------------------------------

            // critical path�� ���� DNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_DNF;

            IDE_TEST( qmoDnfMgr::optimize( aStatement,
                                           sCrtPath->crtDNF,
                                           0 )
                      != IDE_SUCCESS );
            break;

        case QMO_NORMAL_TYPE_NOT_DEFINED:
            //------------------------------------------
            // ���ǵ��� ���� ���
            //------------------------------------------

            // critical path�� ���� CNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_CNF;
            sCrtPath->crtCNF->mIsOnlyNL = sCrtPath->mIsOnlyNL;
            // CNF ����ȭ
            IDE_TEST( qmoCnfMgr::optimize( aStatement,
                                           sCrtPath->crtCNF )
                      != IDE_SUCCESS );

            sCnfCost = sCrtPath->crtCNF->cost;

            /* BUG-47769  recursive with ������ 2���̻� ���ǰ� distinct�� �Բ�
             * ���Ǿ��� ��� FATAL �߻�
             */
            if ( sCrtPath->crtCNF->myQuerySet != NULL )
            {
                sTmp = sCrtPath->crtCNF->myQuerySet;
                if ( sTmp->SFWGH != NULL )
                {
                    if ( sTmp->SFWGH->from != NULL )
                    {
                        if ( sTmp->SFWGH->from->tableRef != NULL )
                        {
                            if ( sTmp->SFWGH->from->tableRef->recursiveView != NULL )
                            {
                                sNormalType = QMO_NORMAL_TYPE_CNF;
                                break;
                            }
                        }
                    }
                }
            }

            // critical path�� ���� DNF�� ó���Ѵ�.
            sCrtPath->currentNormalType = QMO_NORMAL_TYPE_DNF;

            // DNF ����ȭ
            IDE_TEST( qmoDnfMgr::optimize( aStatement,
                                           sCrtPath->crtDNF,
                                           sCnfCost )
                      != IDE_SUCCESS );

            sDnfCost = sCrtPath->crtDNF->cost;

            // CNF�� DNF�� cost ��
            // BUG-42400 cost �񱳽� ��ũ�θ� ����ؾ� �մϴ�.
            if ( QMO_COST_IS_GREATER(sCnfCost, sDnfCost) == ID_TRUE )
            {
                sNormalType = QMO_NORMAL_TYPE_DNF;
                IDE_TEST( qmoCnfMgr::removeOptimizationInfo( aStatement,
                                                             sCrtPath->crtCNF )
                          != IDE_SUCCESS );
            }
            else // sCnfCost <= sDnfCost
            {
                sNormalType = QMO_NORMAL_TYPE_CNF;
                IDE_TEST( qmoDnfMgr::removeOptimizationInfo( aStatement,
                                                             sCrtPath->crtDNF )
                          != IDE_SUCCESS );
            }
            break;

        default:
            IDE_FT_ASSERT( 0 );
            break;
    }

    // critical path�� ó���� ������.
    sCrtPath->currentNormalType = QMO_NORMAL_TYPE_NOT_DEFINED;

    //------------------------------------------
    // ������ normalType ���� ��  ��� Graph ����
    //------------------------------------------

    switch ( sNormalType )
    {
        case QMO_NORMAL_TYPE_NNF:
            sCrtPath->normalType      = QMO_NORMAL_TYPE_NNF;
            sCrtPath->myGraph         = sCrtPath->crtCNF->myGraph;
            sCrtPath->rownumPredicate = sCrtPath->rownumPredicateForCNF;
            break;
        case QMO_NORMAL_TYPE_CNF:
            sCrtPath->normalType      = QMO_NORMAL_TYPE_CNF;
            sCrtPath->myGraph         = sCrtPath->crtCNF->myGraph;
            sCrtPath->rownumPredicate = sCrtPath->rownumPredicateForCNF;
            break;
        case QMO_NORMAL_TYPE_DNF:
            sCrtPath->normalType      = QMO_NORMAL_TYPE_DNF;
            sCrtPath->myGraph         = sCrtPath->crtDNF->myGraph;
            sCrtPath->rownumPredicate = sCrtPath->rownumPredicateForDNF;

            // fix BUG-21478
            // push projection ó���� dnf�� ���� ����� ������.
            // dnf�� ���� ����� �ʿ���.
            sQuerySet->materializeType = QMO_MATERIALIZE_TYPE_RID;
            sQuerySet->SFWGH->hints->materializeType = QMO_MATERIALIZE_TYPE_RID;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    //------------------------------------------
    // PROJ-2509 Join������ Hierarhcy ó��
    //------------------------------------------
    if ( ( sQuerySet->SFWGH->hierarchy != NULL ) &&
         ( ( sQuerySet->SFWGH->from->joinType != QMS_NO_JOIN ) ||
           ( sQuerySet->SFWGH->from->next != NULL ) ) )
    {
        sMyGraph = sCrtPath->myGraph;

        IDE_TEST( qmgHierarchy::init( aStatement,
                                      sQuerySet,
                                      sMyGraph,
                                      sQuerySet->SFWGH->from,
                                      sQuerySet->SFWGH->hierarchy,
                                      &sMyGraph )
                  != IDE_SUCCESS );

        IDE_TEST( qmoCnfMgr::classifyPred4WhereHierachyJoin( aStatement,
                                                             sCrtPath,
                                                             sMyGraph )
                  != IDE_SUCCESS );

        IDE_TEST( qmgHierarchy::optimize( aStatement,
                                          sMyGraph )
                  != IDE_SUCCESS );

        sCrtPath->myGraph = sMyGraph;
    }
    else
    {
        /* Nothing to do */
    }

    //------------------------------------------
    // Counting�� ó��
    //------------------------------------------

    if ( sQuerySet->SFWGH->rownum != NULL )
    {
        sMyGraph = sCrtPath->myGraph;

        IDE_TEST( qmgCounting::init( aStatement,
                                     sQuerySet,
                                     sCrtPath->rownumPredicate,
                                     sMyGraph,         // child
                                     & sMyGraph )      // result graph
                  != IDE_SUCCESS );

        IDE_TEST( qmgCounting::optimize( aStatement, sMyGraph )
                  != IDE_SUCCESS);

        // ��� Graph ����
        sCrtPath->myGraph = sMyGraph;
    }
    else
    {
        // Nohting To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCrtPathMgr::decideNormalType( qcStatement   * aStatement ,
                                 qmsFrom       * aFrom,
                                 qtcNode       * aWhere,
                                 qmsHints      * aHint,
                                 idBool          aIsCNFOnly,
                                 qmoNormalType * aNormalType)
{
/***********************************************************************
 *
 * Description : Normalization Type ����
 *
 *    To Fix PR-12743
 *    Normal Form���� ������ �� ���ٰ� �Ͽ� ���� ó������ �ʴ´�.
 *
 *    ���� �װ��� Type�߿� �ϳ��� �����ȴ�.
 *       : NOT_DEFINED ( ���� ��� ��꿡 ���Ͽ� CNF, DNF�� ������ )
 *       : CNF
 *       : DNF
 *       : NNF
 *
 * Implementation :
 *
 *    (0) CNF Only ���� �˻�
 *        - QCU_OPTIMIZER_DNF_DISABLE == 1,
 *        - No Where, View, DML, Subquery, Outer Join Operator
 *
 *    (1) Normalize ���� ���� �˻�
 *        - CNF �Ұ�, DNF �Ұ� : NNF ���
 *        - CNF ����, DNF �Ұ� : CNF ���
 *        - CNF �Ұ�, DNF ����
 *           - CNF Only ������ ���       : NNF ���
 *           - DNF ���� ������ ���� ���  : DNF ���
 *        - CNF ����, DNF ����
 *           - CNF Only ������ ���       : CNF ���
 *           - ���� ���                  : ���� �ܰ��
 *
 *    (2) ��Ʈ �˻�
 *        - ��Ʈ�� �����ϸ� ��Ʈ ���
 *        - ���ٸ�, ���� �ܰ��
 *
 *    (3) NOT DEFINED
 *
 ***********************************************************************/

    qtcNode     * sWhere;
    qmsFrom     * sFrom;
    qmsHints    * sHint;
    idBool        sIsCNFOnly;
    idBool        sIsExistView;
    idBool        sExistsJoinOper = ID_FALSE;
    qmoNormalType sNormalType;
    UInt          sNormalFormMaximum;
    UInt          sEstimateCnfCnt = 0;
    UInt          sEstimateDnfCnt = 0;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::decideNormalType::__FT__" );

    sWhere       = aWhere;
    sFrom        = aFrom;
    sHint        = aHint;
    sIsExistView = ID_FALSE;

    // NormalType �ʱ�ȭ
    sNormalType = QMO_NORMAL_TYPE_NOT_DEFINED;

    //----------------------------------------------
    // (0) CNF Only ���θ� �Ǵ�
    //----------------------------------------------

    sIsCNFOnly = ID_FALSE;

    while ( 1 )
    {
        //----------------------------------------
        // BUG-38434
        // __OPTIMIZER_DNF_DISABLE property
        //----------------------------------------

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_DNF_DISABLE );

        if ( ( QCU_OPTIMIZER_DNF_DISABLE == 1 ) &&
             ( sHint->normalType != QMO_NORMAL_TYPE_DNF ) )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // CNF Only�� �Է� ���ڸ� ���� ���
        // ON ��, START WITH��, CONNECT BY��, HAVING�� ��
        //----------------------------------------

        if ( aIsCNFOnly == ID_TRUE )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // DML �� ���
        //----------------------------------------

        // To Fix BUG-10576
        // To Fix BUG-12320 MOVE DML �� CNF�θ� ó���Ǿ�� ��
        // BUG-45357 Select for update�� DNF �÷��� �������� �ʵ��� �մϴ�.
        if ( ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_UPDATE ) ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DELETE ) ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_MOVE )   ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE ) )
        {
            // Update, Delete, MOVE ���� CNF�θ� ó���Ǿ�� ��
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // VIEW �� �����ϴ� ���
        //----------------------------------------

        // from���� view�� ������ CNF only
        IDE_TEST( existsViewinFrom( sFrom, &sIsExistView )
                  != IDE_SUCCESS );

        if( sIsExistView == ID_TRUE )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // WHERE ���� ���ų� Outer Join Operator (+) �� �ִ� ���
        //----------------------------------------

        // CNF Only Predicate �˻�
        if ( sWhere == NULL )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            //----------------------------------------
            // PROJ-1653 Outer Join Operator (+)
            //----------------------------------------

            if ( ( sWhere->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                   == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                sIsCNFOnly = ID_TRUE;
                sExistsJoinOper = ID_TRUE;
                break;
            }
            else
            {
                // GO GO
            }
        }

        //----------------------------------------
        // Subquery ������ �ִ� ���
        //----------------------------------------

        IDE_TEST( qmoNormalForm::normalizeCheckCNFOnly( sWhere,
                                                        & sIsCNFOnly )
                  != IDE_SUCCESS );

        break;
    }

    //----------------------------------------------
    // (1) Normalization�� ���������� �Ǵ�
    //----------------------------------------------

    // fix BUG-8806
    // CNF/DNF normalForm �����뿹��
    if ( sWhere != NULL )
    {
        IDE_TEST( qmoNormalForm::estimateCNF( sWhere, &sEstimateCnfCnt )
                  != IDE_SUCCESS );
        IDE_TEST( qmoNormalForm::estimateDNF( sWhere, &sEstimateDnfCnt )
                  != IDE_SUCCESS);
    }
    else
    {
        sEstimateCnfCnt = 0;
        sEstimateDnfCnt = 0;
    }

    sNormalFormMaximum = QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement );

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );

    if ( ( sEstimateCnfCnt > sNormalFormMaximum ) &&
         ( sEstimateDnfCnt > sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF �Ұ� DNF �Ұ�
        //------------------------------

        sNormalType = QMO_NORMAL_TYPE_NNF;
    }
    else if ( ( sEstimateCnfCnt <= sNormalFormMaximum ) &&
              ( sEstimateDnfCnt > sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF ���� DNF �Ұ�
        //------------------------------

        sNormalType = QMO_NORMAL_TYPE_CNF;
    }
    else if ( ( sEstimateCnfCnt > sNormalFormMaximum ) &&
              ( sEstimateDnfCnt <= sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF �Ұ� DNF ����
        //------------------------------

        if ( sIsCNFOnly == ID_TRUE )
        {
            sNormalType = QMO_NORMAL_TYPE_NNF;
        }
        else
        {
            sNormalType = QMO_NORMAL_TYPE_DNF;
        }
    }
    else
    {
        //------------------------------
        // CNF ���� DNF ����
        //------------------------------

        if ( sIsCNFOnly == ID_TRUE )
        {
            sNormalType = QMO_NORMAL_TYPE_CNF;
        }
        else
        {
            // ���� �ܰ�� ����
        }
    }

    //----------------------------------------------
    // ��Ʈ�� �̿��� �˻�
    //----------------------------------------------

    if ( sNormalType == QMO_NORMAL_TYPE_NOT_DEFINED )
    {
        // Normal Form�� �������� ���� ���
        if ( sHint->normalType != QMO_NORMAL_TYPE_NOT_DEFINED )
        {
            //---------------------------------------------------------------
            // Normalization Form Hint �˻� :
            //    Hint�� �����ϸ� Hint�� NormalType�� ������.
            //    QMO_NORMAL_TYPE_CNF �Ǵ� QMO_NORMAL_TYPE_DNF
            //---------------------------------------------------------------

            sNormalType = sHint->normalType;
        }
        else
        {
            // ��Ʈ�� ����
        }
    }
    else
    {
        // �̹� ������
    }

    //----------------------------------------------
    // PROC-1653 Outer Join Operator (+)
    //----------------------------------------------

    IDE_TEST_RAISE( sExistsJoinOper == ID_TRUE && sNormalType != QMO_NORMAL_TYPE_CNF,
                    ERR_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF );


    // out ����
    *aNormalType = sNormalType;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCrtPathMgr::addRownumPredicate( qmsQuerySet  * aQuerySet,
                                   qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1405
 *     Rownum Predicate�� critical path�� rownumPredicate���� �����Ѵ�.
 *
 * Implementation :
 *     1. where���� normalization type�� CNF/NNF�� ���
 *        1.1 where,on���� Rownum Predicate�� rownumPrecicateForCNF�� �����Ѵ�.
 *     2. where���� normalization type�� DNF�� ���
 *        2.1 where,on���� Rownum Predicate�� rownumPrecicateForDNF�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoCrtPath     * sCrtPath;
    qmoNormalType    sCurrentNormalType;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::addRownumPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sCrtPath = aQuerySet->SFWGH->crtPath;
    sCurrentNormalType = sCrtPath->currentNormalType;

    //------------------------------------------
    // Rownum Predicate ����
    //------------------------------------------
    if ( aPredicate != NULL )
    {
        switch ( sCurrentNormalType )
        {
            case QMO_NORMAL_TYPE_NNF:
            case QMO_NORMAL_TYPE_CNF:
                aPredicate->next = sCrtPath->rownumPredicateForCNF;
                sCrtPath->rownumPredicateForCNF = aPredicate;
                break;

            case QMO_NORMAL_TYPE_DNF:
                aPredicate->next = sCrtPath->rownumPredicateForDNF;
                sCrtPath->rownumPredicateForDNF = aPredicate;
                break;

            default:
                IDE_DASSERT( 0 );
                break;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoCrtPathMgr::addRownumPredicateForNode( qcStatement  * aStatement,
                                          qmsQuerySet  * aQuerySet,
                                          qtcNode      * aNode,
                                          idBool         aNeedCopy )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1405
 *     qtcNode������ Rownum Predicate�� critical path�� rownumPredicate����
 *     �����Ѵ�.
 *
 * Implementation :
 *     1. where���� normalization type�� CNF/NNF�� ���
 *        1.1 ���簡 �ʿ��� ��� node�� �����Ѵ�.
 *        1.2 node�� qmoPredicate���·� ���Ѵ�.
 *        1.3 where,on���� Rownum Predicate�� rownumPrecicateForCNF�� �����Ѵ�.
 *     2. where���� normalization type�� DNF�� ���
 *        2.1 ���簡 �ʿ��� ��� node�� �����Ѵ�.
 *        2.2 node�� qmoPredicate���·� ���Ѵ�.
 *        2.3 where,on���� Rownum Predicate�� rownumPrecicateForDNF�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoCrtPath     * sCrtPath;
    qmoNormalType    sCurrentNormalType;
    qmoPredicate   * sNewPred = NULL;
    qtcNode        * sNode;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::addRownumPredicateForNode::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // �⺻ ����
    //------------------------------------------

    sCrtPath = aQuerySet->SFWGH->crtPath;
    sCurrentNormalType = sCrtPath->currentNormalType;

    //------------------------------------------
    // NNF Predicate ����
    //------------------------------------------

    // NNF filter�� ��� rownumPredicate�� �����ϱ� ����
    // qmoPredicate���� ���Ѵ�.
    if ( aNode != NULL )
    {
        if ( aNeedCopy == ID_TRUE )
        {
            // NNF filter ����
            IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                             aNode,
                                             & sNode,
                                             ID_FALSE,
                                             ID_FALSE,
                                             ID_TRUE,
                                             ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            sNode = aNode;
        }

        // qmoPredicate ����
        IDE_TEST( qmoPred::createPredicate( QC_QMP_MEM(aStatement),
                                            sNode,
                                            & sNewPred )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // Rownum Predicate ����
    //------------------------------------------

    if ( sNewPred != NULL )
    {
        switch ( sCurrentNormalType )
        {
            case QMO_NORMAL_TYPE_NNF:
            case QMO_NORMAL_TYPE_CNF:
                sNewPred->next = sCrtPath->rownumPredicateForCNF;
                sCrtPath->rownumPredicateForCNF = sNewPred;
                break;

            case QMO_NORMAL_TYPE_DNF:
                sNewPred->next = sCrtPath->rownumPredicateForDNF;
                sCrtPath->rownumPredicateForDNF = sNewPred;
                break;

            default:
                IDE_DASSERT( 0 );
                break;
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

IDE_RC qmoCrtPathMgr::existsViewinFrom( qmsFrom * aFrom,
                                        idBool  * aIsExistView )
{
/***********************************************************************
 *
 * Description : From�� ���� view�� �����ϴ��� �˻�
 *
 * Implementation :
 *    (1) from->left, right���� : traverse
 *    (2) from->left, right�� �������� ���� : ������ from����,
 *                        view�� ������ ���ɼ��� ����.
 *
 ***********************************************************************/

    qmsFrom * sFrom;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::existsViewinFrom::__FT__" );

    // To Fix PR-11530, PR-11536
    // FROM�� ��θ� �˻��Ͽ��� ��.
    for ( sFrom = aFrom; sFrom != NULL; sFrom = sFrom->next )
    {
        if( sFrom->left == NULL && sFrom->right == NULL )
        {
            if( sFrom->tableRef->view != NULL )
            {
                *aIsExistView = ID_TRUE;
                break;
            }
        }
        else
        {
            IDE_TEST( existsViewinFrom( sFrom->left, aIsExistView )
                      != IDE_SUCCESS );

            IDE_TEST( existsViewinFrom( sFrom->right, aIsExistView )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCrtPathMgr::decideNormalType4Where( qcStatement   * aStatement,
                                       qmsFrom       * aFrom,
                                       qtcNode       * aWhere,
                                       qmsHints      * aHint,
                                       idBool          aIsCNFOnly,
                                       qmoNormalType * aNormalType )
{
/***********************************************************************
 *
 * Description : Normalization Type ����
 *
 *    BUG-35155 Partial CNF
 *    CNF estimate �� NORMALFORM_MAXIMUM ���� Ŀ���� ���
 *    qtcNode �Ϻκ��� CNF ���� �����Ѵ�.
 *
 *    ���� �װ��� Type�߿� �ϳ��� �����ȴ�.
 *       : NOT_DEFINED ( ���� ��� ��꿡 ���Ͽ� CNF, DNF�� ������ )
 *       : CNF
 *       : DNF
 *       : NNF
 *
 * Implementation :
 *
 *    (0) CNF Only ���� �˻�
 *        - QCU_OPTIMIZER_DNF_DISABLE == 1,
 *        - No Where, View, DML, Subquery, Outer Join Operator
 *
 *    (1) Normalize ���� ���� �˻�
 *        - CNF �Ұ�, DNF �Ұ� : NNF ���
 *        - CNF ����, DNF �Ұ� : CNF ���
 *        - CNF �Ұ�, DNF ����
 *           - CNF Only ������ ���       : NNF ���
 *           - DNF ���� ������ ���� ���  : DNF ���
 *        - CNF ����, DNF ����
 *           - CNF Only ������ ���       : CNF ���
 *           - ���� ���                  : ���� �ܰ��
 *
 *    (2) ��Ʈ �˻�
 *        - ��Ʈ�� �����ϸ� ��Ʈ ���
 *        - ���ٸ�, ���� �ܰ��
 *
 *    (3) NOT DEFINED
 *
 ***********************************************************************/

    qtcNode     * sWhere;
    qmsFrom     * sFrom;
    qmsHints    * sHint;
    idBool        sIsCNFOnly;
    idBool        sIsExistView;
    idBool        sExistsJoinOper = ID_FALSE;
    qmoNormalType sNormalType;
    UInt          sNormalFormMaximum;
    UInt          sEstimateCnfCnt = 0;
    UInt          sEstimateDnfCnt = 0;

    IDU_FIT_POINT_FATAL( "qmoCrtPathMgr::decideNormalType4Where::__FT__" );

    sWhere       = aWhere;
    sFrom        = aFrom;
    sHint        = aHint;
    sIsExistView = ID_FALSE;

    // NormalType �ʱ�ȭ
    sNormalType = QMO_NORMAL_TYPE_NOT_DEFINED;

    //----------------------------------------------
    // (0) CNF Only ���θ� �Ǵ�
    //----------------------------------------------

    sIsCNFOnly = ID_FALSE;

    while ( 1 )
    {
        //----------------------------------------
        // BUG-38434
        // __OPTIMIZER_DNF_DISABLE property
        //----------------------------------------

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_DNF_DISABLE );

        if ( ( QCU_OPTIMIZER_DNF_DISABLE == 1 ) &&
             ( sHint->normalType != QMO_NORMAL_TYPE_DNF ) )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // CNF Only�� �Է� ���ڸ� ���� ���
        // ON ��, START WITH��, CONNECT BY��, HAVING�� ��
        //----------------------------------------

        if ( aIsCNFOnly == ID_TRUE )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // DML �� ���
        //----------------------------------------

        // To Fix BUG-10576
        // To Fix BUG-12320 MOVE DML �� CNF�θ� ó���Ǿ�� ��
        // BUG-45357 Select for update�� DNF �÷��� �������� �ʵ��� �մϴ�.
        if ( ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_UPDATE ) ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DELETE ) ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_MOVE )   ||
             ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE ) )
        {
            // Update, Delete, MOVE ���� CNF�θ� ó���Ǿ�� ��
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // VIEW �� �����ϴ� ���
        //----------------------------------------

        // from���� view�� ������ CNF only
        IDE_TEST( existsViewinFrom( sFrom, &sIsExistView ) != IDE_SUCCESS );

        if( sIsExistView == ID_TRUE )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            // Go Go
        }

        //----------------------------------------
        // WHERE ���� ���ų� Outer Join Operator (+) �� �ִ� ���
        //----------------------------------------

        // CNF Only Predicate �˻�
        if ( sWhere == NULL )
        {
            sIsCNFOnly = ID_TRUE;
            break;
        }
        else
        {
            //----------------------------------------
            // PROJ-1653 Outer Join Operator (+)
            //----------------------------------------

            if ( ( sWhere->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                 == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                sIsCNFOnly = ID_TRUE;
                sExistsJoinOper = ID_TRUE;
                break;
            }
            else
            {
                // GO GO
            }
        }

        //----------------------------------------
        // Subquery ������ �ִ� ���
        //----------------------------------------

        IDE_TEST( qmoNormalForm::normalizeCheckCNFOnly( sWhere, &sIsCNFOnly )
                     != IDE_SUCCESS );

        break;
    }

    //----------------------------------------------
    // (1) Normalization�� ���������� �Ǵ�
    //----------------------------------------------

    sNormalFormMaximum = QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement );

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );

    // CNF/DNF normalForm �����뿹��
    if ( sWhere != NULL )
    {
        if ( sNormalFormMaximum > 0 )
        {
            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_OPTIMIZER_PARTIAL_NORMALIZE );

            if ( QCU_OPTIMIZER_PARTIAL_NORMALIZE == 0 )
            {
                // Estimate CNF and DNF
                IDE_TEST( qmoNormalForm::estimateCNF( sWhere, &sEstimateCnfCnt )
                          != IDE_SUCCESS );
                IDE_TEST( qmoNormalForm::estimateDNF( sWhere, &sEstimateDnfCnt )
                          != IDE_SUCCESS);
            }
            else
            {
                // Estimate partial CNF
                qmoNormalForm::estimatePartialCNF( sWhere,
                                                   &sEstimateCnfCnt,
                                                   NULL,
                                                   sNormalFormMaximum );

                // �ֻ��� qtcNode �� CNF UNUSABLE �̸� ������ ��ü�� NNF �����̹Ƿ�
                // CNF �� �Ұ����ϴ�.
                if ( ( sWhere->node.lflag &  MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK )
                     == MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE )
                {
                    sEstimateCnfCnt = UINT_MAX;
                }

                // Estimate DNF
                IDE_TEST( qmoNormalForm::estimateDNF( sWhere, &sEstimateDnfCnt )
                          != IDE_SUCCESS);
            }
        }
        else
        {
            sEstimateCnfCnt = UINT_MAX;
            sEstimateDnfCnt = UINT_MAX;
        }
    }
    else
    {
        sEstimateCnfCnt = 0;
        sEstimateDnfCnt = 0;
    }

    if ( ( sEstimateCnfCnt > sNormalFormMaximum ) &&
         ( sEstimateDnfCnt > sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF �Ұ� DNF �Ұ�
        //------------------------------

        sNormalType = QMO_NORMAL_TYPE_NNF;
    }
    else if ( ( sEstimateCnfCnt <= sNormalFormMaximum ) &&
              ( sEstimateDnfCnt > sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF ���� DNF �Ұ�
        //------------------------------

        sNormalType = QMO_NORMAL_TYPE_CNF;
    }
    else if ( ( sEstimateCnfCnt > sNormalFormMaximum ) &&
              ( sEstimateDnfCnt <= sNormalFormMaximum ) )
    {
        //------------------------------
        // CNF �Ұ� DNF ����
        //------------------------------

        if ( sIsCNFOnly == ID_TRUE )
        {
            sNormalType = QMO_NORMAL_TYPE_NNF;
        }
        else
        {
            sNormalType = QMO_NORMAL_TYPE_DNF;
        }
    }
    else
    {
        //------------------------------
        // CNF ���� DNF ����
        //------------------------------

        if ( sIsCNFOnly == ID_TRUE )
        {
            sNormalType = QMO_NORMAL_TYPE_CNF;
        }
        else
        {
            // ���� �ܰ�� ����
        }
    }

    //----------------------------------------------
    // ��Ʈ�� �̿��� �˻�
    //----------------------------------------------

    if ( sNormalType == QMO_NORMAL_TYPE_NOT_DEFINED )
    {
        // Normal Form�� �������� ���� ���
        if ( sHint->normalType != QMO_NORMAL_TYPE_NOT_DEFINED )
        {
            //---------------------------------------------------------------
            // Normalization Form Hint �˻� :
            //    Hint�� �����ϸ� Hint�� NormalType�� ������.
            //    QMO_NORMAL_TYPE_CNF �Ǵ� QMO_NORMAL_TYPE_DNF
            //---------------------------------------------------------------

            sNormalType = sHint->normalType;
        }
        else
        {
            // ��Ʈ�� ����
        }
    }
    else
    {
        // �̹� ������
    }

    //----------------------------------------------
    // PROC-1653 Outer Join Operator (+)
    //----------------------------------------------

    IDE_TEST_RAISE( sExistsJoinOper == ID_TRUE && sNormalType != QMO_NORMAL_TYPE_CNF,
                    ERR_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF );


    // out ����
    *aNormalType = sNormalType;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_OUTER_JOIN_OPERATOR_NOT_ALLOW_NOCNF ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
