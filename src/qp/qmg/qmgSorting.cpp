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
 * $Id: qmgSorting.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Sorting Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgSorting.h>
#include <qmoCost.h>
#include <qmoOneMtrPlan.h>
#include <qmoParallelPlan.h>
#include <qmgGrouping.h>

IDE_RC
qmgSorting::init( qcStatement * aStatement,
                  qmsQuerySet * aQuerySet,
                  qmgGraph    * aChildGraph,
                  qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgSorting�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgSorting�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgSORT      * sMyGraph;
    qmsQuerySet  * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgSorting::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Sorting Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgSorting�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgSORT ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SORTING;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgSorting::optimize;
    sMyGraph->graph.makePlan = qmgSorting::makePlan;
    sMyGraph->graph.printGraph = qmgSorting::printGraph;

    // Disk/Memory ���� ����
    for ( sQuerySet = aQuerySet;
          sQuerySet->left != NULL;
          sQuerySet = sQuerySet->left ) ;

    switch(  sQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            // �߰� ��� Type Hint�� ���� ���, ������ Type�� ������.
            if ( ( aChildGraph->flag & QMG_GRAPH_TYPE_MASK )
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
    // Sorting Graph ���� ������ ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->orderBy = ((qmsParseTree*)(aStatement->myPlan->parseTree))->orderBy;
    sMyGraph->limitCnt = 0;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSorting::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSorting�� ����ȭ
 *
 * Implementation :
 *    (1) orderBy�� Subquery Graph ����
 *    (2) orderBy�� subquery�� expression�� �������� ������, ����ȭ ó��
 *        A. indexable Order By ����ȭ
 *        B. indexable Order By ����ȭ ������ ���, Limit Sort ����ȭ
 *        C. Limit Sort ����ȭ ���õ� ���, limitCnt ����
 *    (3) ���� ��� ����
 *    (4) Preserved Order
 *
 ***********************************************************************/

    qmgSORT           * sMyGraph;
    qmsSortColumns    * sOrderBy;
    qtcNode           * sSortNode;
    qmgPreservedOrder * sNewOrder;
    qmgPreservedOrder * sWantOrder;
    qmgPreservedOrder * sCurOrder;
    qmsLimit          * sLimit;
    qmgGraph          * sChild;

    SDouble             sTotalCost     = 0.0;
    SDouble             sDiskCost      = 0.0;
    SDouble             sAccessCost    = 0.0;
    SDouble             sSelAccessCost = 0.0;
    SDouble             sSelDiskCost   = 0.0;
    SDouble             sSelTotalCost  = 0.0;
    SDouble             sOutputRecordCnt;

    UInt                sLimitCnt = 0;
    idBool              sExistSubquery   = ID_FALSE;
    idBool              sSuccess         = ID_FALSE;
    idBool              sHasHostVar      = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgSorting::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph       = (qmgSORT*) aGraph;
    sWantOrder     = NULL;

    //------------------------------------------
    // order by�� subquery graph ���� �� want order ����
    //------------------------------------------

    for ( sOrderBy = sMyGraph->orderBy;
          sOrderBy != NULL;
          sOrderBy = sOrderBy->next )
    {
        sSortNode = sOrderBy->sortColumn;

        if ( sSortNode->node.module == &( qtc::passModule ) )
        {
            sSortNode = (qtcNode*)sSortNode->node.arguments;
        }
        else
        {
            // nothing to do
        }

        // BUG-42041 SortColumn���� PSM Variable�� ��������,
        // �� ��� limit sort�� �� �� ����.
        if ( MTC_NODE_IS_DEFINED_TYPE( & sSortNode->node ) == ID_FALSE )
        {
            sHasHostVar = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        if ( ( sSortNode->lflag & QTC_NODE_SUBQUERY_MASK )
             == QTC_NODE_SUBQUERY_EXIST )
        {
            sExistSubquery = ID_TRUE;
            IDE_TEST( qmoSubquery::optimize( aStatement,
                                             sSortNode,
                                             ID_FALSE ) // No KeyRange Tip
                      != IDE_SUCCESS );
        }
        else
        {
            // want order ����
            IDE_TEST(
                QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                               (void**)&sNewOrder )
                != IDE_SUCCESS );

            sNewOrder->table = sSortNode->node.table;
            sNewOrder->column = sSortNode->node.column;

            if ( sOrderBy->nullsOption == QMS_NULLS_NONE )
            {
                // To Fix BUG-8316
                if ( sOrderBy->isDESC == ID_TRUE )
                {
                    sNewOrder->direction = QMG_DIRECTION_DESC;
                }
                else
                {
                    sNewOrder->direction = QMG_DIRECTION_ASC;
                }
            }
            else
            {
                if ( sOrderBy->nullsOption == QMS_NULLS_FIRST )
                {
                    if ( sOrderBy->isDESC == ID_TRUE )
                    {
                        sNewOrder->direction = QMG_DIRECTION_DESC_NULLS_FIRST;
                    }
                    else
                    {
                        sNewOrder->direction = QMG_DIRECTION_ASC_NULLS_FIRST;
                    }
                }
                else
                {
                    if ( sOrderBy->isDESC == ID_TRUE )
                    {
                        sNewOrder->direction = QMG_DIRECTION_DESC_NULLS_LAST;
                    }
                    else
                    {
                        sNewOrder->direction = QMG_DIRECTION_ASC_NULLS_LAST;
                    }
                }
            }
            sNewOrder->next = NULL;

            if ( sWantOrder == NULL )
            {
                sWantOrder = sCurOrder = sNewOrder;
            }
            else
            {
                sCurOrder->next = sNewOrder;
                sCurOrder = sCurOrder->next;
            }
        }
    }

    // BUG-43194 SORT �÷��� cost�� �̿��Ͽ� index ��� ���θ� ����
    if ( (aGraph->flag & QMG_GRAPH_TYPE_MASK) == QMG_GRAPH_TYPE_MEMORY )
    {
        sSelTotalCost = qmoCost::getMemSortTempCost(
            aStatement->mSysStat,
            &(aGraph->left->costInfo),
            QMO_COST_DEFAULT_NODE_CNT ); // node cnt

        sSelAccessCost = sSelTotalCost;
        sSelDiskCost   = 0.0;
    }
    else
    {
        sSelTotalCost   = qmoCost::getDiskSortTempCost(
            aStatement->mSysStat,
            &(aGraph->left->costInfo),
            QMO_COST_DEFAULT_NODE_CNT, // node cnt
            aGraph->left->costInfo.recordSize );

        sSelAccessCost = 0.0;
        sSelDiskCost   = sSelTotalCost;
    }

    if ( sExistSubquery == ID_FALSE )
    {
        //------------------------------------------
        // order by�� subquery�� �������� ������, ����ȭ ���� �õ�
        //------------------------------------------

        //------------------------------------------
        // Indexable Order By ����ȭ
        //------------------------------------------

        IDE_TEST( getCostByPrevOrder( aStatement,
                                      sMyGraph,
                                      sWantOrder,
                                      &sAccessCost,
                                      &sDiskCost,
                                      &sTotalCost ) != IDE_SUCCESS );

        if ( QMO_COST_IS_EQUAL(sTotalCost, QMO_COST_INVALID_COST) == ID_TRUE )
        {
            // retryPreservedOrder �� ������ ��쿡�� ������ ����Ѵ�.
            IDE_TEST(qmg::retryPreservedOrder( aStatement,
                                               aGraph,
                                               sWantOrder,
                                               &sSuccess )
                     != IDE_SUCCESS);

            if ( sSuccess == ID_TRUE )
            {
                sSelAccessCost = 0.0;
                sSelDiskCost   = 0.0;
                sSelTotalCost  = 0.0;

                sMyGraph->graph.flag &= ~QMG_SORT_OPT_TIP_MASK;
                sMyGraph->graph.flag |= QMG_SORT_OPT_TIP_INDEXABLE_ORDERBY;
            }
            else
            {
                // nothing to do
            }
        }
        else if ( QMO_COST_IS_LESS(sTotalCost, sSelTotalCost) == ID_TRUE )
        {
            IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                              aGraph,
                                              sWantOrder,
                                              &sSuccess )
                      != IDE_SUCCESS );

            if ( sSuccess == ID_TRUE )
            {
                sSelAccessCost = sAccessCost;
                sSelDiskCost   = sDiskCost;
                sSelTotalCost  = sTotalCost;

                sMyGraph->graph.flag &= ~QMG_SORT_OPT_TIP_MASK;
                sMyGraph->graph.flag |= QMG_SORT_OPT_TIP_INDEXABLE_ORDERBY;
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

        //------------------------------------------
        // Indexable Min Max ����ȭ PR-19344
        //------------------------------------------

        if ( (aGraph->left->type == QMG_GROUPING) &&
             ((aGraph->left->flag & QMG_GROP_OPT_TIP_MASK)
              == QMG_GROP_OPT_TIP_INDEXABLE_MINMAX) ) // Indexable Min-Max
        {
            sSelAccessCost = 0.0;
            sSelDiskCost   = 0.0;
            sSelTotalCost  = 0.0;

            sMyGraph->graph.flag &= ~QMG_INDEXABLE_MIN_MAX_MASK;
            sMyGraph->graph.flag |= QMG_INDEXABLE_MIN_MAX_TRUE;
            // Indexable order-by�� �����ϰ� sort node�� ���� �ʱ� ����
            sMyGraph->graph.flag &= ~QMG_SORT_OPT_TIP_MASK;
            sMyGraph->graph.flag |= QMG_SORT_OPT_TIP_INDEXABLE_ORDERBY;
        }
        else
        {
            // nothing to do
        }

        //------------------------------------------
        // Limit Sort ����ȭ
        //------------------------------------------

        if ( ( sMyGraph->graph.flag & QMG_SORT_OPT_TIP_MASK ) ==
             QMG_SORT_OPT_TIP_NONE )
        {
            sLimit = ((qmsParseTree*)(aStatement->myPlan->parseTree))->limit;

            // BUG-10146 fix
            // limit ���� host variable binding�� ����Ѵ�.
            // ������ ���� host variable�� ���Ǹ� limit sort�� ������ �� ����.
            // BUG-42041 SortColum���� PSM Variable�� ���� ��쵵 ������ �� ����.
            if ( sLimit != NULL )
            {
                if ( (qmsLimitI::hasHostBind( sLimit ) == ID_FALSE) &&
                     ( sHasHostVar == ID_FALSE ) )
                {
                    // To Fix PR-8017
                    sLimitCnt = qmsLimitI::getStartConstant( sLimit ) +
                        qmsLimitI::getCountConstant( sLimit ) - 1;

                    if ( (sLimitCnt > 0) &&
                         (sLimitCnt <= QMN_LMST_MAXIMUM_LIMIT_CNT) )
                    {
                        // Memory Temp Table���� ����ϰ� �ȴ�.

                        // left �� output record count �� �����Ͽ� ����Ѵ�.
                        sOutputRecordCnt = aGraph->left->costInfo.outputRecordCnt;
                        aGraph->left->costInfo.outputRecordCnt = sLimitCnt;

                        sSelAccessCost = qmoCost::getMemSortTempCost(
                            aStatement->mSysStat,
                            &(aGraph->left->costInfo),
                            QMO_COST_DEFAULT_NODE_CNT ); // node cnt

                        aGraph->left->costInfo.outputRecordCnt = sOutputRecordCnt;

                        sSelDiskCost   = 0.0;
                        sSelTotalCost  = sSelAccessCost;

                        sMyGraph->limitCnt = sLimitCnt;
                        sMyGraph->graph.flag &= ~QMG_SORT_OPT_TIP_MASK;
                        sMyGraph->graph.flag |= QMG_SORT_OPT_TIP_LMST;
                    }
                    else if ( sLimitCnt == 0 )
                    {
                        sSelAccessCost = 0.0;
                        sSelDiskCost   = 0.0;
                        sSelTotalCost  = 0.0;

                        sMyGraph->limitCnt = sLimitCnt;
                        sMyGraph->graph.flag &= ~QMG_SORT_OPT_TIP_MASK;
                        sMyGraph->graph.flag |= QMG_SORT_OPT_TIP_LMST;
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
    else
    {
        // nothing to do
    }

    // preserved order ����
    sMyGraph->graph.preservedOrder = sWantOrder;
    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;

    /* PROJ-1353 ROLLUP, CUBE�� ���� �׷����϶� OPT TIP�� ���� VALUE TEMP ���� ���� */
    if ( ( sMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
         == QMG_GROUPBY_EXTENSION_TRUE )
    {
        switch ( sMyGraph->graph.flag & QMG_SORT_OPT_TIP_MASK )
        {
            case QMG_SORT_OPT_TIP_LMST :
            case QMG_SORT_OPT_TIP_NONE : /* BUG-43727 SORT ��� ��, Disk/Memory�� ������� Value Temp�� ����Ѵ�. */
                /* Row�� Value�� �ױ⸦ Rollup, Cube�� �����Ѵ�. */
                sMyGraph->graph.left->flag &= ~QMG_VALUE_TEMP_MASK;
                sMyGraph->graph.left->flag |= QMG_VALUE_TEMP_TRUE;
                break;
            default:
                break;
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* BUG-39384  A some rollup query result is wrong 
     * with distinct, order by clause.
     */
    if ( sMyGraph->graph.left->type == QMG_DISTINCTION )
    {
        sChild = sMyGraph->graph.left;

        if ( sChild->left != NULL )
        {
            /* PROJ-1353 Rollup, Cube�� ���� ���� �� */
            if ( ( sChild->left->flag & QMG_GROUPBY_EXTENSION_MASK )
                 == QMG_GROUPBY_EXTENSION_TRUE )
            {
                /* Row�� Value�� �ױ⸦ Rollup, Cube�� �����Ѵ�. */
                sChild->left->flag &= ~QMG_VALUE_TEMP_MASK;
                sChild->left->flag |= QMG_VALUE_TEMP_TRUE;
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

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // recordSize
    sMyGraph->graph.costInfo.recordSize =
        sMyGraph->graph.left->costInfo.recordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = 1;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // outputRecordCnt
    if ( (sMyGraph->graph.flag & QMG_SORT_OPT_TIP_MASK) == QMG_SORT_OPT_TIP_LMST )
    {
        if ( sLimitCnt == 0 )
        {
            sMyGraph->graph.costInfo.outputRecordCnt = 1;
        }
        else
        {
            sMyGraph->graph.costInfo.outputRecordCnt = sLimitCnt;
        }
    }
    else
    {
        sMyGraph->graph.costInfo.outputRecordCnt =
            sMyGraph->graph.left->costInfo.outputRecordCnt;
    }

    // myCost
    sMyGraph->graph.costInfo.myAccessCost = sSelAccessCost;
    sMyGraph->graph.costInfo.myDiskCost   = sSelDiskCost;
    sMyGraph->graph.costInfo.myAllCost    = sSelTotalCost;

    // total cost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->graph.left->costInfo.totalAccessCost +
        sMyGraph->graph.costInfo.myAccessCost;
    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->graph.left->costInfo.totalDiskCost +
        sMyGraph->graph.costInfo.myDiskCost;
    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->graph.left->costInfo.totalAllCost +
        sMyGraph->graph.costInfo.myAllCost;

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
    aGraph->flag |= (aGraph->left->flag & QMG_PARALLEL_EXEC_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmgSorting::makePlan( qcStatement   * aStatement,
                             const qmgGraph* aParent,
                             qmgGraph      * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSorting���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *     - qmgSorting���� ���� ���������� Plan
 *
 *         1.  Indexable Order By ����ȭ�� ����� ���
 *
 *                  ����
 *
 *         2.  Limit Sort ����ȭ�� ����� ���
 *
 *                  [LMST]
 *
 *         3.  ����ȭ�� ���� ���
 *
 *                  [SORT]
 *
 ***********************************************************************/

    qmgSORT         * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSorting::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgSORT*) aGraph;

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    // BUG-38410
    // Materialize �� ��� ���� ������ �ѹ��� ����ǰ�
    // materialized �� ���븸 �����Ѵ�.
    // ���� �ڽ� ��忡�� SCAN �� ���� parallel �� ����Ѵ�.
    aGraph->left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aGraph->left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    sMyGraph->graph.myPlan = aParent->myPlan;

    switch( sMyGraph->graph.flag & QMG_SORT_OPT_TIP_MASK )
    {
        case QMG_SORT_OPT_TIP_INDEXABLE_ORDERBY:
            IDE_TEST( makeChildPlan( aStatement,
                                     sMyGraph )
                      != IDE_SUCCESS );

            break;

        case QMG_SORT_OPT_TIP_LMST:
            IDE_TEST( makeLimitSort( aStatement,
                                     sMyGraph )
                      != IDE_SUCCESS );
            break;

        case QMG_SORT_OPT_TIP_NONE:
            IDE_TEST( makeSort( aStatement,
                                sMyGraph )
                      != IDE_SUCCESS );
            break;

        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSorting::makeChildPlan( qcStatement * aStatement,
                           qmgSORT     * aMyGraph )
{

    IDU_FIT_POINT_FATAL( "qmgSorting::makeChildPlan::__FT__" );

    //---------------------------------------------------
    // ���� Plan�� ����
    //---------------------------------------------------

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph ,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = aMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_ORDERBY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSorting::makeSort( qcStatement * aStatement,
                      qmgSORT     * aMyGraph )
{
    UInt              sFlag = 0;
    qmnPlan         * sSORT;
    qmgGraph        * sChild;

    IDU_FIT_POINT_FATAL( "qmgSorting::makeSort::__FT__" );

    //-----------------------------------------------------
    //        [SORT]
    //          |
    //        [LEFT]
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    /* BUG-36826 A rollup or cube occured wrong result using order by count_i3 */
    if ( ( aMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
         == QMG_GROUPBY_EXTENSION_TRUE )
    {
        if ( ( aMyGraph->graph.left->flag & QMG_VALUE_TEMP_MASK )
             == QMG_VALUE_TEMP_TRUE )
        {
            sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
            sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
        }
        else
        {
            if ( aMyGraph->graph.myQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
            {
                sFlag &= ~QMO_MAKESORT_GROUP_EXT_VALUE_MASK;
                sFlag |= QMO_MAKESORT_GROUP_EXT_VALUE_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* BUG-42303 A rollup/cube is wrong result with distinct and order by */
        if ( aMyGraph->graph.left->type == QMG_DISTINCTION )
        {
            sChild = aMyGraph->graph.left;

            if ( sChild->left != NULL )
            {
                /* PROJ-1353 Rollup, Cube�� ���� ���� �� */
                if ( ( sChild->left->flag & QMG_GROUPBY_EXTENSION_MASK )
                     == QMG_GROUPBY_EXTENSION_TRUE )
                {
                    sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                    sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
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

    //-----------------------
    // init SORT
    //-----------------------
    IDE_TEST( qmoOneMtrPlan::initSORT( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       sFlag,
                                       aMyGraph->orderBy,
                                       aMyGraph->graph.myPlan,
                                       &sSORT ) != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sSORT;

    //----------------------------
    // ���� plan ����
    //----------------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make SORT
    //-----------------------

    sFlag &= ~QMO_MAKESORT_METHOD_MASK;
    sFlag |= QMO_MAKESORT_ORDERBY;

    //FALSE�� �Ǵ� ������ SORT_TIP�� ���⶧���̴�. TRUE���Ǵ�
    //��Ȳ�̶�� INDEXABLE ORDER BY TIP�� ����ɰ��̴�.
    sFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
    sFlag |= QMO_MAKESORT_PRESERVED_FALSE;

    //���� ��ü�� ����
    if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
    }

    IDE_TEST( qmoOneMtrPlan::makeSORT( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       NULL ,
                                       aMyGraph->graph.myPlan ,
                                       aMyGraph->graph.costInfo.inputRecordCnt,
                                       sSORT ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sSORT;

    qmg::setPlanInfo( aStatement, sSORT, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSorting::makeLimitSort( qcStatement * aStatement,
                           qmgSORT     * aMyGraph )
{
    UInt              sFlag = 0;
    qmnPlan         * sLMST;
    qmgGraph        * sChild;

    IDU_FIT_POINT_FATAL( "qmgSorting::makeLimitSort::__FT__" );

    //-----------------------------------------------------
    //        [LMST]
    //          |
    //        [LEFT]
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    /* BUG-36826 A rollup or cube occured wrong result using order by count_i3 */
    if ( ( aMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
         == QMG_GROUPBY_EXTENSION_TRUE )
    {
        if ( ( aMyGraph->graph.left->flag & QMG_VALUE_TEMP_MASK )
             == QMG_VALUE_TEMP_TRUE )
        {
            sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
            sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
        }
        else
        {
            if ( aMyGraph->graph.myQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
            {
                sFlag &= ~QMO_MAKESORT_GROUP_EXT_VALUE_MASK;
                sFlag |= QMO_MAKESORT_GROUP_EXT_VALUE_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* BUG-42303 A rollup/cube is wrong result with distinct and order by */
        if ( aMyGraph->graph.left->type == QMG_DISTINCTION )
        {
            sChild = aMyGraph->graph.left;

            if ( sChild->left != NULL )
            {
                /* PROJ-1353 Rollup, Cube�� ���� ���� �� */
                if ( ( sChild->left->flag & QMG_GROUPBY_EXTENSION_MASK )
                     == QMG_GROUPBY_EXTENSION_TRUE )
                {
                    sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
                    sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
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

    //-----------------------
    // init LMST
    //-----------------------

    IDE_TEST( qmoOneMtrPlan::initLMST( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       sFlag,
                                       aMyGraph->orderBy,
                                       aMyGraph->graph.myPlan,
                                       &sLMST ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sLMST;

    //----------------------------
    // ���� plan ����
    //----------------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make LMST
    //-----------------------

    sFlag &= ~QMO_MAKELMST_METHOD_MASK;
    sFlag |= QMO_MAKELMST_LIMIT_ORDERBY;

    IDE_TEST( qmoOneMtrPlan::makeLMST( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->limitCnt ,
                                       aMyGraph->graph.myPlan ,
                                       sLMST )
              != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sLMST;

    qmg::setPlanInfo( aStatement, sLMST, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSorting::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgSorting::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

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
    // Child Graph ���� ������ ���
    //-----------------------------------

    IDE_TEST( aGraph->left->printGraph( aStatement,
                                        aGraph->left,
                                        aDepth + 1,
                                        aString )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSorting::getCostByPrevOrder( qcStatement       * aStatement,
                                       qmgSORT           * aSortGraph,
                                       qmgPreservedOrder * aWantOrder,
                                       SDouble           * aAccessCost,
                                       SDouble           * aDiskCost,
                                       SDouble           * aTotalCost )
{
/***********************************************************************
 *
 * Description :
 *
 *    Preserved Order ����� ����� Sorting ����� ����Ѵ�.
 *
 * Implementation :
 *
 *    �̹� Child�� ���ϴ� Preserved Order�� ������ �ִٸ�
 *    ������ ��� ���� Sorting�� �����ϴ�.
 *
 *    �ݸ� Child�� Ư�� �ε����� �����ϴ� �����,
 *    Child�� �ε����� �̿��� ����� ���Եǰ� �ȴ�.
 *
 ***********************************************************************/

    SDouble             sTotalCost;
    SDouble             sAccessCost;
    SDouble             sDiskCost;

    idBool              sUsable;

    qmoAccessMethod   * sOrgMethod;
    qmoAccessMethod   * sSelMethod;

    IDU_FIT_POINT_FATAL( "qmgSorting::getCostByPrevOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSortGraph != NULL );

    //------------------------------------------
    // Preserved Order�� ����� �� �ִ� ���� �˻�
    //------------------------------------------

    // preserved order ���� ���� �˻�
    IDE_TEST( qmg::checkUsableOrder( aStatement,
                                     aWantOrder,
                                     aSortGraph->graph.left,
                                     & sOrgMethod,
                                     & sSelMethod,
                                     & sUsable )
              != IDE_SUCCESS );

    //------------------------------------------
    // ��� ���
    //------------------------------------------

    if ( sUsable == ID_TRUE )
    {
        if ( aSortGraph->graph.left->preservedOrder == NULL )
        {
            if ( (sOrgMethod == NULL) || (sSelMethod == NULL) )
            {
                // BUG-43824 sorting ����� ����� �� access method�� NULL�� �� �ֽ��ϴ�
                // ������ ���� �̿��ϴ� ����̹Ƿ� 0�� �����Ѵ�.
                sAccessCost = 0;
                sDiskCost   = 0;
            }
            else
            {
                // ���õ� Access Method�� ������ AccessMethod ���̸�ŭ
                // �߰� ����� �߻��Ѵ�.
                sAccessCost = IDL_MAX( ( sSelMethod->accessCost - sOrgMethod->accessCost ), 0 );
                sDiskCost   = IDL_MAX( ( sSelMethod->diskCost   - sOrgMethod->diskCost   ), 0 );
            }
        }
        else
        {
            // �̹� Child�� Ordering�� �ϰ� ����.
            // ���ڵ� �Ǽ���ŭ�� �� ��븸�� �ҿ��.
            // BUG-41237 compare ��븸 �߰��Ѵ�.
            sAccessCost = aSortGraph->graph.left->costInfo.outputRecordCnt *
                aStatement->mSysStat->mCompareTime;
            sDiskCost   = 0;
        }
        sTotalCost  = sAccessCost + sDiskCost;
    }
    else
    {
        // Preserved Order�� ����� �� ���� �����.
        sAccessCost = QMO_COST_INVALID_COST;
        sDiskCost   = QMO_COST_INVALID_COST;
        sTotalCost  = QMO_COST_INVALID_COST;
    }

    *aTotalCost  = sTotalCost;
    *aDiskCost   = sDiskCost;
    *aAccessCost = sAccessCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}
