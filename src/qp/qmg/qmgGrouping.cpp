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
 * $Id: qmgGrouping.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 *
 * Description :
 *     Grouping Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgGrouping.h>
#include <qmgSelection.h>
#include <qmoCost.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoNormalForm.h>
#include <qmoSelectivity.h>
#include <qcgPlan.h>
#include <qmoParallelPlan.h>
#include <qmv.h>
#include <qcg.h>

/* BUG-46906 */
extern mtfModule mtfListagg;
extern mtfModule mtfPercentileDisc;
extern mtfModule mtfPercentileCont;

IDE_RC
qmgGrouping::init( qcStatement * aStatement,
                   qmsQuerySet * aQuerySet,
                   qmgGraph    * aChildGraph,
                   idBool        aIsNested,
                   qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgGrouping Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgGrouping�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgGROP      * sMyGraph;
    qtcNode      * sNormalForm;
    qtcNode      * sNode;
    qmoPredicate * sNewPred = NULL;
    qmoPredicate * sHavingPred = NULL;

    qmoNormalType sNormalType;

    IDU_FIT_POINT_FATAL( "qmgGrouping::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Grouping Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgGrouping�� ���� ���� �Ҵ�
    IDU_FIT_POINT( "qmgGrouping::init::alloc::Graph" );
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgGROP),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( &sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_GROUPING;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myFrom = aChildGraph->myFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgGrouping::optimize;
    sMyGraph->graph.makePlan = qmgGrouping::makePlan;
    sMyGraph->graph.printGraph = qmgGrouping::printGraph;

    // Disk/Memory ���� ����
    switch(  aQuerySet->SFWGH->hints->interResultType )
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
    // Grouping Graph ���� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    if ( aIsNested == ID_TRUE )
    {
        // Nested Aggregation
        sMyGraph->graph.flag &= ~QMG_GROP_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GROP_TYPE_NESTED;
        sMyGraph->aggregation = aQuerySet->SFWGH->aggsDepth2;
        sMyGraph->groupBy = NULL;
        sMyGraph->having = NULL;
        sMyGraph->havingPred = NULL;
        sMyGraph->distAggArg = NULL;
    }
    else
    {
        // �Ϲ� Aggregation
        sMyGraph->graph.flag &= ~QMG_GROP_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GROP_TYPE_GENERAL;
        sMyGraph->aggregation = aQuerySet->SFWGH->aggsDepth1;
        sMyGraph->groupBy = aQuerySet->SFWGH->group;

        // To Fix BUG-8287
        if ( aQuerySet->SFWGH->having != NULL )
        {
            // To Fix PR-12743 NNF Filter����
            IDE_TEST( qmoCrtPathMgr
                      ::decideNormalType( aStatement,
                                          sMyGraph->graph.myFrom,
                                          aQuerySet->SFWGH->having,
                                          aQuerySet->SFWGH->hints,
                                          ID_TRUE, // CNF Only��
                                          & sNormalType )
                      != IDE_SUCCESS );

            switch ( sNormalType )
            {
                case QMO_NORMAL_TYPE_NNF:

                    if ( aQuerySet->SFWGH->having != NULL )
                    {
                        aQuerySet->SFWGH->having->lflag
                            &= ~QTC_NODE_NNF_FILTER_MASK;
                        aQuerySet->SFWGH->having->lflag
                            |= QTC_NODE_NNF_FILTER_TRUE;
                    }
                    else
                    {
                        // nothing to do
                    }

                    sMyGraph->having = aQuerySet->SFWGH->having;
                    sMyGraph->havingPred = NULL;
                    break;

                case QMO_NORMAL_TYPE_CNF:

                    IDE_TEST( qmoNormalForm
                              ::normalizeCNF( aStatement,
                                              aQuerySet->SFWGH->having,
                                              & sNormalForm )
                              != IDE_SUCCESS );

                    sMyGraph->having = sNormalForm;

                    for ( sNode = (qtcNode*)sNormalForm->node.arguments;
                          sNode != NULL;
                          sNode = (qtcNode*)sNode->node.next )
                    {
                        IDU_FIT_POINT( "qmgGrouping::init::alloc::NewPred" );
                        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                                      ID_SIZEOF( qmoPredicate ),
                                      (void**) & sNewPred )
                                  != IDE_SUCCESS );

                        sNewPred->node = sNode;
                        sNewPred->flag = QMO_PRED_CLEAR;
                        sNewPred->mySelectivity = 1;
                        sNewPred->totalSelectivity = 1;
                        sNewPred->next = NULL;
                        sNewPred->more = NULL;

                        if ( sHavingPred == NULL )
                        {
                            sMyGraph->havingPred = sHavingPred = sNewPred;
                        }
                        else
                        {
                            sHavingPred->next = sNewPred;
                            sHavingPred = sHavingPred->next;
                        }
                    }
                    break;

                case QMO_NORMAL_TYPE_DNF:
                case QMO_NORMAL_TYPE_NOT_DEFINED:
                default:
                    IDE_FT_ASSERT( 0 );
                    break;
            }
        }
        else
        {
            sMyGraph->having = NULL;
            sMyGraph->havingPred = NULL;
        }
    }


    sMyGraph->hashBucketCnt = 0;
    sMyGraph->distAggArg = NULL;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgGrouping::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgGrouping�� ����ȭ
 *
 * Implementation :
 *    (1) �Ϲ� Grouping�� ��� ������ ����
 *       A. Subquery graph ����
 *         - �Ϲ� Grouping : having��,aggsDepth1�� subquery ó��
 *         - nested Grouping :  aggsDepth2�� subquery ó��
 *       B. Grouping ����ȭ ����
 *         ( ����ȭ ���� �� Preserved Order ���� �� ������ )
 *    (2) groupingMethod ���� �� Preserved Order flag ����
 *        - Hash Based Grouping : Never Preserved Order
 *        - Sort Based Grouping : Preserved Order Defined
 *    (3) hash based grouping�� ���, hashBucketCnt ����
 *    (4) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgGROP            * sMyGraph;
    qmsAggNode         * sAggr;
    qmsConcatElement   * sGroup;
    qmsConcatElement   * sSubGroup;
    qmoDistAggArg      * sNewDistAggr;
    qmoDistAggArg      * sFirstDistAggr;
    qmoDistAggArg      * sCurDistAggr;
    qtcNode            * sNode;
    mtcColumn          * sMtcColumn;
    qtcNode            * sList;

    idBool               sIsGroupExt;
    idBool               sEnableParallel = ID_TRUE;
    SDouble              sRecordSize;
    SDouble              sAggrCost;
    idBool               sIsFuncData = ID_FALSE; /* BUG-46906 */
    qtcNode            * sArgument   = NULL;     /* BUG-46922 */

    IDU_FIT_POINT_FATAL( "qmgGrouping::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph       = (qmgGROP*)aGraph;
    sAggr          = sMyGraph->aggregation;
    sFirstDistAggr = NULL;
    sCurDistAggr   = NULL;
    sRecordSize    = 0;
    sAggrCost      = 0;
    sIsGroupExt    = ID_FALSE;

    // BUG-38416 count(column) to count(*)
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_COUNT_COLUMN_TO_COUNT_ASTAR );

    //------------------------------------------
    //    - recordSize ���
    //    - Subquery Graph�� ó�� �� distAggArg ����
    //------------------------------------------

    if ( sAggr != NULL )
    {
        for ( ; sAggr != NULL; sAggr = sAggr->next )
        {
            sNode = sAggr->aggr;

            sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
            sRecordSize += sMtcColumn->column.size;

            /* BUG-46906 BUG-46922 */
            if ( ( sNode->node.module == & mtfListagg ) ||
                 ( sNode->node.module == & mtfPercentileDisc ) ||
                 ( sNode->node.module == & mtfPercentileCont ) )
            {
                sIsFuncData = ID_TRUE;
            }
            else
            {
                /* BUG-46906 */
                sArgument = (qtcNode*)sAggr->aggr->node.arguments;

                if ( sArgument != NULL )
                {
                    if ( ( sArgument->node.module == & mtfListagg ) ||
                         ( sArgument->node.module == & mtfPercentileDisc ) ||
                         ( sArgument->node.module == & mtfPercentileCont ) )
                    {
                        sIsFuncData = ID_TRUE;
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

            // aggregation�� subquery ����ȭ
            IDE_TEST(
                qmoPred::optimizeSubqueryInNode( aStatement,
                                                 sAggr->aggr,
                                                 ID_FALSE,  //No Range
                                                 ID_FALSE ) //No ConstantFilter
                != IDE_SUCCESS );

            // distAggArg ����
            if ( ( sAggr->aggr->node.lflag & MTC_NODE_DISTINCT_MASK )
                 == MTC_NODE_DISTINCT_TRUE )
            {
                IDU_FIT_POINT( "qmgGrouping::optimize::alloc::NewDistAggr" );
                IDE_TEST(
                    QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoDistAggArg ),
                                                   (void**)&sNewDistAggr )
                    != IDE_SUCCESS );

                sNode = (qtcNode*)sAggr->aggr->node.arguments;

                sNewDistAggr->aggArg  = sNode;
                sNewDistAggr->next    = NULL;

                if ( sFirstDistAggr == NULL )
                {
                    sFirstDistAggr = sCurDistAggr = sNewDistAggr;
                }
                else
                {
                    sCurDistAggr->next = sNewDistAggr;
                    sCurDistAggr = sCurDistAggr->next;
                }
            }
            else
            {
                // nothing to do
            }
        }
        sMyGraph->distAggArg = sFirstDistAggr;
    }
    else
    {
        // nothing to do
    }

    if ( ( sMyGraph->graph.flag & QMG_GROP_TYPE_MASK )
         == QMG_GROP_TYPE_GENERAL )
    {
        //------------------------------------------
        // �Ϲ� Grouping �� ���
        //    - group by Į���� subquery ó��
        //    - having�� subquery ó��
        //    - Grouping Optimization Tip ����
        //------------------------------------------

        // group�� subquery ó��
        for ( sGroup = sMyGraph->groupBy;
              sGroup != NULL;
              sGroup = sGroup->next )
        {
            if ( sGroup->type == QMS_GROUPBY_NORMAL )
            {
                sNode = sGroup->arithmeticOrList;

                sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
                sRecordSize += sMtcColumn->column.size;

                IDE_TEST(
                    qmoPred::optimizeSubqueryInNode( aStatement,
                                                     sGroup->arithmeticOrList,
                                                     ID_FALSE, // No Range
                                                     ID_FALSE )// No ConstantFilter
                    != IDE_SUCCESS );
            }
            else
            {
                /* PROJ-1353 */
                sMyGraph->graph.flag &= ~QMG_GROUPBY_EXTENSION_MASK;
                sMyGraph->graph.flag |= QMG_GROUPBY_EXTENSION_TRUE;

                switch ( sGroup->type )
                {
                    case QMS_GROUPBY_ROLLUP:
                        sMyGraph->graph.flag &= ~QMG_GROP_OPT_TIP_MASK;
                        sMyGraph->graph.flag |= QMG_GROP_OPT_TIP_ROLLUP;
                        break;
                    case QMS_GROUPBY_CUBE:
                        sMyGraph->graph.flag &= ~QMG_GROP_OPT_TIP_MASK;
                        sMyGraph->graph.flag |= QMG_GROP_OPT_TIP_CUBE;
                        break;
                    case QMS_GROUPBY_GROUPING_SETS:
                        sMyGraph->graph.flag &= ~QMG_GROP_OPT_TIP_MASK;
                        sMyGraph->graph.flag |= QMG_GROP_OPT_TIP_GROUPING_SETS;
                        break;
                    default:
                        break;
                }
                sIsGroupExt = ID_TRUE;
                for ( sSubGroup = sGroup->arguments;
                      sSubGroup != NULL;
                      sSubGroup = sSubGroup->next )
                {
                    if ( ( sSubGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                         == MTC_NODE_OPERATOR_LIST )
                    {
                        for ( sList = (qtcNode *)sSubGroup->arithmeticOrList->node.arguments;
                              sList != NULL;
                              sList = (qtcNode *)sList->node.next )
                        {
                            sNode = sList;

                            sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
                            sRecordSize += sMtcColumn->column.size;
                        }
                    }
                    else
                    {
                        sNode = sSubGroup->arithmeticOrList;

                        sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
                        sRecordSize += sMtcColumn->column.size;
                    }
                }
            }
        }

        if ( sMyGraph->having != NULL )
        {
            // having�� subquery ó��
            IDE_TEST(
                qmoPred::optimizeSubqueryInNode( aStatement,
                                                 sMyGraph->having,
                                                 ID_FALSE, // No Range
                                                 ID_FALSE )// No ConstantFilter
                != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // Nested Grouping �� ���
        // nothing to do
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    //------------------------------------------
    // Grouping Method ���� �� Preserved Order ����
    //------------------------------------------

    // TASK-6699 TPC-H ���� ����
    // AGGR �Լ��� �þ���� ����ð��� ������
    sAggrCost = qmoCost::getAggrCost( aStatement->mSysStat,
                                      sMyGraph->aggregation,
                                      sMyGraph->graph.left->costInfo.outputRecordCnt );

    if ( sIsGroupExt == ID_FALSE )
    {
        //------------------------------------------
        // Hash Bucket Count ����
        //------------------------------------------

        // BUG-38132 group by�� temp table �� �޸𸮷� �����ϴ� ������Ƽ
        if ( (QCU_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP == 1) &&
             (sMyGraph->distAggArg == NULL) )
        {
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;

            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP );
        }
        else
        {
            // nothing todo.
        }

        if ( sMyGraph->groupBy != NULL )
        {
            IDE_TEST(
                getBucketCnt4Group(
                    aStatement,
                    sMyGraph,
                    sMyGraph->graph.myQuerySet->SFWGH->hints->groupBucketCnt,
                    & sMyGraph->hashBucketCnt )
                != IDE_SUCCESS );
        }
        else
        {
            // BUG-37074
            // group by �� �������� bucket ������ 1
            sMyGraph->hashBucketCnt = 1;
        }

        if ( sMyGraph->groupBy != NULL )
        {
            // GROUP BY �� �����ϴ� ��� Grouping Method�� ������.
            IDE_TEST( setGroupingMethod( aStatement,
                                         sMyGraph,
                                         sRecordSize,
                                         sAggrCost ) != IDE_SUCCESS );
        }
        else
        {
            // GROUP BY�� �������� �ʴ� ���
            sMyGraph->graph.flag &= ~QMG_GROUPBY_NONE_MASK;
            sMyGraph->graph.flag |= QMG_GROUPBY_NONE_TRUE;

            // �پ��� ����ȭ Tip�� ������ ������ �� �˻�
            IDE_TEST( nonGroupOptTip( aStatement,
                                      sMyGraph,
                                      sRecordSize,
                                      sAggrCost ) != IDE_SUCCESS );
        }
    }
    else
    {
        /* PROJ-1353 */
        IDE_TEST( setPreOrderGroupExtension( aStatement, sMyGraph, sRecordSize )
                  != IDE_SUCCESS );
    }

    /* BUG-46906 */
    if ( sIsFuncData == ID_TRUE )
    {
        sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
        sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_SORT;
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-37074
    if( sMyGraph->distAggArg != NULL )
    {
        for( sCurDistAggr = sMyGraph->distAggArg;
             sCurDistAggr != NULL;
             sCurDistAggr = sCurDistAggr->next )
        {
            // group by �� ����Ͽ� distinct �� bucket ������ ����.
            IDE_TEST( qmg::getBucketCnt4DistAggr(
                          aStatement,
                          sMyGraph->graph.left->costInfo.outputRecordCnt,
                          sMyGraph->hashBucketCnt,
                          sCurDistAggr->aggArg,
                          sMyGraph->graph.myQuerySet->SFWGH->hints->groupBucketCnt,
                          &sCurDistAggr->bucketCnt )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // nothing todo.
    }

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // recordSize = group by column size + aggregation column size
    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // selectivity
    IDE_TEST( qmoSelectivity::setGroupingSelectivity(
                  & sMyGraph->graph,
                  sMyGraph->havingPred,
                  sMyGraph->graph.left->costInfo.outputRecordCnt,
                  & sMyGraph->graph.costInfo.selectivity )
              != IDE_SUCCESS );

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // outputRecordCnt
    IDE_TEST( qmoSelectivity::setGroupingOutputCnt(
                  aStatement,
                  & sMyGraph->graph,
                  sMyGraph->groupBy,
                  sMyGraph->graph.costInfo.inputRecordCnt,
                  sMyGraph->graph.costInfo.selectivity,
                  & sMyGraph->graph.costInfo.outputRecordCnt )
              != IDE_SUCCESS );

    //----------------------------------
    // �ش� Graph�� ��� ���� ����
    //----------------------------------

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->graph.left->costInfo.totalAccessCost +
        sMyGraph->graph.costInfo.myAccessCost;
    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->graph.left->costInfo.totalDiskCost +
        sMyGraph->graph.costInfo.myDiskCost;
    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->graph.left->costInfo.totalAllCost +
        sMyGraph->graph.costInfo.myAllCost;

    // PROJ-2444 Parallel Aggreagtion
    for ( sAggr = sMyGraph->aggregation; sAggr != NULL; sAggr = sAggr->next )
    {
        // psm not support
        if( ( sAggr->aggr->lflag & QTC_NODE_PROC_FUNCTION_MASK )
            == QTC_NODE_PROC_FUNCTION_FALSE )
        {
            //nothing to do
        }
        else
        {
            sEnableParallel = ID_FALSE;
            break;
        }

        // subQ not support
        if( ( sAggr->aggr->lflag & QTC_NODE_SUBQUERY_MASK )
            == QTC_NODE_SUBQUERY_ABSENT )
        {
            //nothing to do
        }
        else
        {
            sEnableParallel = ID_FALSE;
            break;
        }

        // BUG-43830 parallel aggregation
        if ( ( QTC_STMT_EXECUTE( aStatement, sAggr->aggr )->merge != mtf::calculateNA ) ||
             ( QTC_STMT_EXECUTE( aStatement, sAggr->aggr )->merge == NULL ) )
        {
            //nothing to do
        }
        else
        {
            sEnableParallel = ID_FALSE;
            break;
        }
    }

    for ( sGroup = sMyGraph->groupBy; sGroup != NULL; sGroup = sGroup->next )
    {
        // psm not support
        if ( sGroup->arithmeticOrList != NULL )
        {
            if ( ( sGroup->arithmeticOrList->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                 == QTC_NODE_PROC_FUNCTION_FALSE )
            {
                //nothing to do
            }
            else
            {
                sEnableParallel = ID_FALSE;
                break;
            }
        }
        else
        {
            //nothing to do
        }
    }

    // PROJ-2444 Parallel Aggreagtion
    if ( sEnableParallel == ID_TRUE )
    {
        aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
        aGraph->flag |= (aGraph->left->flag & QMG_PARALLEL_EXEC_MASK);
    }
    else
    {
        aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
        aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmgGrouping::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgGrouping���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *     - qmgGrouping���� ���� ���������� Plan
 *         1.  ����ȭ Tip �� ����� ���
 *
 *             A.  Indexable Distinct Aggregation�� ���
 *                 ��) SELECT SUM(DISTINCT I1) FROM T1;
 *
 *                       [GRAG]        : SUM(I1)�� ó��
 *                         |
 *                       [GRBY]        : DISTINCT I1�� ó��
 *                                     : distinct option�� ���
 *
 *             B.  COUNT ASTERISK ����ȭ�� ����� ���
 *
 *                    [CUNT]
 *
 *             C.  Indexable Min-Max ����ȭ�� ����� ���
 *
 *                    ����
 *
 *         2.  GROUP BY�� ó��
 *
 *             - Sort-based�� ���
 *
 *                    [GRBY]
 *                      |
 *                  ( [SORT] ) : Indexable Group By�� ��� �������� ����
 *
 *             - Hash-based�� ���
 *
 *                    ����
 *
 *             - Group By �� ���� ���
 *
 *                    ����
 *
 *         3.  Aggregation�� ó��
 *             - Sort-based�� ��� ( Distinct Aggregation�� ���� )
 *
 *                    [AGGR]
 *                      |
 *                    Group By�� ó��
 *
 *             - Hash-based�� ���
 *
 *                    [GRAG]
 *
 ***********************************************************************/

    qmgGROP          * sMyGraph;
    qmnPlan          * sFILT;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgGROP*) aGraph;

    sMyGraph->graph.myPlan = aParent->myPlan;

    //----------------------------
    // Having���� ó��
    //----------------------------

    if( sMyGraph->having != NULL )
    {
        IDE_TEST( qmoOneNonPlan::initFILT( aStatement ,
                                           sMyGraph->graph.myQuerySet ,
                                           sMyGraph->having ,
                                           sMyGraph->graph.myPlan,
                                           &sFILT ) != IDE_SUCCESS);
        sMyGraph->graph.myPlan = sFILT;
    }
    else
    {
        //nothing to do
    }

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    // BUG-38410
    // Materialize �� ��� ���� ������ �ѹ��� ����ǰ�
    // materialized �� ���븸 �����Ѵ�.
    // ���� �ڽ� ��忡�� SCAN �� ���� parallel �� ����Ѵ�.
    aGraph->left->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aGraph->left->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    switch( sMyGraph->graph.flag & QMG_GROP_OPT_TIP_MASK )
    {
        case QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY:
        case QMG_GROP_OPT_TIP_NONE:
            switch( sMyGraph->graph.flag & QMG_SORT_HASH_METHOD_MASK )
            {
                case QMG_SORT_HASH_METHOD_SORT:
                    IDE_TEST( makeSortGroup( aStatement, sMyGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_SORT_HASH_METHOD_HASH:
                    IDE_TEST( makeHashGroup( aStatement, sMyGraph )
                              != IDE_SUCCESS );
                    break;
            }
            break;
        case QMG_GROP_OPT_TIP_COUNT_STAR:
            IDE_TEST( makeCountAll( aStatement,
                                    sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_GROP_OPT_TIP_INDEXABLE_MINMAX:
            IDE_TEST( makeIndexMinMax( aStatement,
                                       sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG:
            IDE_TEST( makeIndexDistAggr( aStatement,
                                         sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_GROP_OPT_TIP_ROLLUP:
            IDE_TEST( makeRollup( aStatement, sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_GROP_OPT_TIP_CUBE:
            IDE_TEST( makeCube( aStatement, sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMG_GROP_OPT_TIP_GROUPING_SETS:
            break;
        default:
            break;
    }

    //----------------------------
    // Having���� ó��
    //----------------------------

    if( sMyGraph->having != NULL )
    {
        IDE_TEST( qmoOneNonPlan::makeFILT( aStatement ,
                                           sMyGraph->graph.myQuerySet ,
                                           sMyGraph->having ,
                                           NULL ,
                                           sMyGraph->graph.myPlan ,
                                           sFILT ) != IDE_SUCCESS);
        sMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo( aStatement, sFILT, &(sMyGraph->graph) );
    }
    else
    {
        //nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeChildPlan( qcStatement * aStatement,
                            qmgGROP     * aMyGraph )
{
    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeChildPlan::__FT__" );

    //---------------------------------------------------
    // ���� plan�� ����
    //---------------------------------------------------
    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = aMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_GROUPBY;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeHashGroup( qcStatement * aStatement,
                            qmgGROP     * aMyGraph )
{
    UInt                sFlag;
    qmnPlan           * sGRAG         = NULL;
    qmnPlan           * sParallelGRAG = NULL;
    qmnChildren       * sChildren;
    qmcAttrDesc       * sResultDescOrg;
    qmcAttrDesc       * sResultDesc;
    UShort              sSourceTable;
    UShort              sDestTable;
    idBool              sMakeParallel;

    //-----------------------------------------------------
    //        [GRAG]
    //           |
    //        [LEFT]
    //-----------------------------------------------------

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeHashGroup::__FT__" );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //----------------------------
    // init GRAG
    //----------------------------

    IDE_TEST( qmoOneMtrPlan::initGRAG(
                  aStatement ,
                  aMyGraph->graph.myQuerySet ,
                  aMyGraph->aggregation ,
                  aMyGraph->groupBy ,
                  aMyGraph->graph.myPlan,
                  &sGRAG )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRAG;

    //----------------------------
    // ���� plan ����
    //----------------------------
    IDE_TEST( makeChildPlan( aStatement, aMyGraph ) != IDE_SUCCESS );

    // PROJ-2444
    // Parallel �÷��� �������θ� �����Ѵ�.
    sMakeParallel = checkParallelEnable( aMyGraph );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //----------------------------
    // make GRAG
    //----------------------------

    sFlag = 0;

    //���� ��ü�� ����
    if( aMyGraph->groupBy == NULL )
    {
        //Group by �÷��� ���ٸ� ���� ��ü�� memory�� ����
        sFlag &= ~QMO_MAKEGRAG_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKEGRAG_MEMORY_TEMP_TABLE;
    }
    else
    {
        //Graph���� ����
        if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
            QMG_GRAPH_TYPE_MEMORY )
        {
            sFlag &= ~QMO_MAKEGRAG_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEGRAG_MEMORY_TEMP_TABLE;
        }
        else
        {
            sFlag &= ~QMO_MAKEGRAG_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEGRAG_DISK_TEMP_TABLE;
        }
    }

    // PROJ-2444 Parallel Aggreagtion
    if ( sMakeParallel == ID_TRUE )
    {
        //-----------------------------------------------------
        //        [GRAG ( merge ) ]
        //           |
        //        [GRAG ( aggr ) ]
        //           |
        //        [LEFT]
        //-----------------------------------------------------

        if ( aMyGraph->graph.left->myPlan->type == QMN_PSCRD )
        {
            sFlag &= ~QMO_MAKEGRAG_PARALLEL_STEP_MASK;
            sFlag |= QMO_MAKEGRAG_PARALLEL_STEP_AGGR;

            sDestTable   = aMyGraph->graph.left->myFrom->tableRef->table;

            for ( sChildren  = aMyGraph->graph.myPlan->childrenPRLQ;
                  sChildren != NULL;
                  sChildren  = sChildren->next )
            {
                // ���� GRAG ��带 �����Ѵ�.
                IDE_TEST( qmoParallelPlan::copyGRAG( aStatement,
                                                     aMyGraph->graph.myQuerySet,
                                                     sDestTable,
                                                     sGRAG,
                                                     &sParallelGRAG )
                          != IDE_SUCCESS );

                // initGRAG ���� ResultDesc->expr Node �� ���� ����ȴ�.
                // ���� Node�� ���縦 �ؾ��Ѵ�.
                for( sResultDescOrg  = sGRAG->resultDesc,
                         sResultDesc     = sParallelGRAG->resultDesc;
                     sResultDescOrg != NULL;
                     sResultDescOrg  = sResultDescOrg->next,
                         sResultDesc     = sResultDesc->next )
                {
                    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                                     sResultDescOrg->expr,
                                                     &sResultDesc->expr,
                                                     ID_FALSE,
                                                     ID_FALSE,
                                                     ID_FALSE,
                                                     ID_FALSE )
                              != IDE_SUCCESS );
                }
                IDE_DASSERT( sResultDesc == NULL );

                IDE_TEST( qmoOneMtrPlan::makeGRAG(
                              aStatement,
                              aMyGraph->graph.myQuerySet,
                              sFlag,
                              aMyGraph->groupBy,
                              aMyGraph->hashBucketCnt,
                              aMyGraph->graph.myPlan,
                              sParallelGRAG )
                          != IDE_SUCCESS);

                qmg::setPlanInfo( aStatement, sParallelGRAG, &(aMyGraph->graph) );

                // PSCRD �� ���� PRLQ-SCAN ������ �޷��ִ�.
                IDE_DASSERT( sChildren->childPlan->type       == QMN_PRLQ );
                IDE_DASSERT( sChildren->childPlan->left->type == QMN_SCAN );

                sParallelGRAG->left        = sChildren->childPlan->left;
                sChildren->childPlan->left = sParallelGRAG;
            }

            sFlag &= ~QMO_MAKEGRAG_PARALLEL_STEP_MASK;
            sFlag |= QMO_MAKEGRAG_PARALLEL_STEP_MERGE;

            IDE_TEST( qmoOneMtrPlan::makeGRAG(
                          aStatement,
                          aMyGraph->graph.myQuerySet,
                          sFlag,
                          aMyGraph->groupBy,
                          aMyGraph->hashBucketCnt,
                          aMyGraph->graph.myPlan,
                          sGRAG )
                      != IDE_SUCCESS);
        }
        else if ( aMyGraph->graph.left->myPlan->type == QMN_PPCRD )
        {
            sFlag &= ~QMO_MAKEGRAG_PARALLEL_STEP_MASK;
            sFlag |= QMO_MAKEGRAG_PARALLEL_STEP_AGGR;

            for ( sChildren  = aMyGraph->graph.myPlan->children;
                  sChildren != NULL;
                  sChildren  = sChildren->next )
            {
                sSourceTable = aMyGraph->graph.left->myFrom->tableRef->table;
                sDestTable   = sChildren->childPlan->depInfo.depend[0];

                IDE_TEST( qmoParallelPlan::copyGRAG( aStatement,
                                                     aMyGraph->graph.myQuerySet,
                                                     sDestTable,
                                                     sGRAG,
                                                     &sParallelGRAG )
                          != IDE_SUCCESS );

                for( sResultDescOrg  = sGRAG->resultDesc,
                         sResultDesc     = sParallelGRAG->resultDesc;
                     sResultDescOrg != NULL;
                     sResultDescOrg  = sResultDescOrg->next,
                         sResultDesc     = sResultDesc->next )
                {
                    // ��Ƽ���� ��� SCAN ����� tuple ���� ���� �ٸ���.
                    // ���� ���� ������ Node �� �ش� ��Ƽ���� tuple�� �ٶ󺸵��� �������־�� �Ѵ�.
                    IDE_TEST( qtc::cloneQTCNodeTree4Partition(
                                  QC_QMP_MEM(aStatement),
                                  sResultDescOrg->expr,
                                  &sResultDesc->expr,
                                  sSourceTable,
                                  sDestTable,
                                  ID_FALSE )
                              != IDE_SUCCESS );

                    // cloneQTCNodeTree4Partition �Լ����� ������ ��带 �����Ѵ�.
                    // �� �����ؾ� �Ѵ�.
                    IDE_TEST( qtc::estimate(
                                  sResultDesc->expr,
                                  QC_SHARED_TMPLATE(aStatement),
                                  aStatement,
                                  aMyGraph->graph.myQuerySet,
                                  aMyGraph->graph.myQuerySet->SFWGH,
                                  NULL )
                              != IDE_SUCCESS );
                }
                IDE_DASSERT( sResultDesc == NULL );

                IDE_TEST( qmoOneMtrPlan::makeGRAG(
                              aStatement,
                              aMyGraph->graph.myQuerySet,
                              sFlag,
                              aMyGraph->groupBy,
                              aMyGraph->hashBucketCnt,
                              aMyGraph->graph.myPlan,
                              sParallelGRAG )
                          != IDE_SUCCESS);

                qmg::setPlanInfo( aStatement, sParallelGRAG, &(aMyGraph->graph) );

                // PPCRD �� ���� SCAN �� �޷��ִ�.
                IDE_DASSERT( sChildren->childPlan->type       == QMN_SCAN );

                sParallelGRAG->left     = sChildren->childPlan;
                sChildren->childPlan    = sParallelGRAG;
            }

            sFlag &= ~QMO_MAKEGRAG_PARALLEL_STEP_MASK;
            sFlag |=  QMO_MAKEGRAG_PARALLEL_STEP_MERGE;

            IDE_TEST( qmoOneMtrPlan::makeGRAG(
                          aStatement,
                          aMyGraph->graph.myQuerySet,
                          sFlag,
                          aMyGraph->groupBy,
                          aMyGraph->hashBucketCnt,
                          aMyGraph->graph.myPlan,
                          sGRAG )
                      != IDE_SUCCESS);
        }
        else
        {
            IDE_DASSERT( 0 );
        }
    }
    else
    {
        IDE_TEST( qmoOneMtrPlan::makeGRAG(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFlag ,
                      aMyGraph->groupBy ,
                      aMyGraph->hashBucketCnt ,
                      aMyGraph->graph.myPlan ,
                      sGRAG )
                  != IDE_SUCCESS);
    }

    aMyGraph->graph.myPlan = sGRAG;

    qmg::setPlanInfo( aStatement, sGRAG, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeSortGroup( qcStatement * aStatement,
                            qmgGROP     * aMyGraph )
{
    UInt          sFlag;
    qmnPlan     * sAGGR = NULL;
    qmnPlan     * sGRBY = NULL;
    qmnPlan     * sSORT = NULL;

    //-----------------------------------------------------
    //        [*AGGR]
    //           |
    //        [*GRBY]
    //           |
    //        [*SORT]
    //           |
    //        [LEFT]
    // * Option
    //-----------------------------------------------------

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeSortGroup::__FT__" );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    if( aMyGraph->aggregation != NULL )
    {
        //-----------------------
        // init AGGR
        //-----------------------

        IDE_TEST( qmoOneNonPlan::initAGGR(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      aMyGraph->aggregation ,
                      aMyGraph->groupBy ,
                      aMyGraph->graph.myPlan,
                      &sAGGR ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sAGGR;
    }
    else
    {
        //nothing to od
    }

    if( aMyGraph->groupBy != NULL )
    {
        //-----------------------
        // init GRBY
        //-----------------------

        IDE_TEST( qmoOneNonPlan::initGRBY(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      aMyGraph->aggregation ,
                      aMyGraph->groupBy ,
                      &sGRBY ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sGRBY;

        if( (aMyGraph->graph.flag & QMG_GROP_OPT_TIP_MASK) !=
            QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY )
        {
            //-----------------------
            // init SORT
            //-----------------------

            IDE_TEST( qmoOneMtrPlan::initSORT(
                          aStatement ,
                          aMyGraph->graph.myQuerySet ,
                          aMyGraph->graph.myPlan,
                          &sSORT ) != IDE_SUCCESS);
            aMyGraph->graph.myPlan = sSORT;
        }
        else
        {
            //nothing to do
        }
    }
    else
    {
        //nothing to do
    }

    //----------------------------
    // ���� plan ����
    //----------------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    if( aMyGraph->groupBy != NULL )
    {
        if( (aMyGraph->graph.flag & QMG_GROP_OPT_TIP_MASK) !=
            QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY )
        {
            //-----------------------
            // make SORT
            //-----------------------
            sFlag = 0;
            sFlag &= ~QMO_MAKESORT_METHOD_MASK;
            sFlag |= QMO_MAKESORT_SORT_BASED_GROUPING;
            sFlag &= ~QMO_MAKESORT_POSITION_MASK;
            sFlag |= QMO_MAKESORT_POSITION_LEFT;
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

            IDE_TEST( qmoOneMtrPlan::makeSORT(
                          aStatement ,
                          aMyGraph->graph.myQuerySet ,
                          sFlag ,
                          aMyGraph->graph.preservedOrder ,
                          aMyGraph->graph.myPlan ,
                          aMyGraph->graph.costInfo.inputRecordCnt,
                          sSORT ) != IDE_SUCCESS);
            aMyGraph->graph.myPlan = sSORT;

            qmg::setPlanInfo( aStatement, sSORT, &(aMyGraph->graph) );
        }
        else
        {
            //nothing to do
        }

        //-----------------------
        // make GRBY
        //-----------------------
        sFlag = 0;
        sFlag &= ~QMO_MAKEGRBY_SORT_BASED_METHOD_MASK;

        if( aMyGraph->aggregation != NULL )
        {
            sFlag |= QMO_MAKEGRBY_SORT_BASED_GROUPING;
        }
        else
        {
            sFlag |= QMO_MAKEGRBY_SORT_BASED_DISTAGGR;
        }

        IDE_TEST( qmoOneNonPlan::makeGRBY(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFlag ,
                      aMyGraph->graph.myPlan ,
                      sGRBY ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sGRBY;

        qmg::setPlanInfo( aStatement, sGRBY, &(aMyGraph->graph) );
    }
    else
    {
        //nothing to do
    }

    if( aMyGraph->aggregation != NULL )
    {
        //-----------------------
        // make AGGR
        //-----------------------
        sFlag = 0;
        //���� ��ü�� ����
        if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
            QMG_GRAPH_TYPE_MEMORY )
        {
            sFlag &= ~QMO_MAKEAGGR_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEAGGR_MEMORY_TEMP_TABLE;
        }
        else
        {
            sFlag &= ~QMO_MAKEAGGR_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEAGGR_DISK_TEMP_TABLE;
        }

        IDE_TEST( qmoOneNonPlan::makeAGGR(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFlag ,
                      aMyGraph->distAggArg ,
                      aMyGraph->graph.myPlan ,
                      sAGGR ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sAGGR;

        qmg::setPlanInfo( aStatement, sAGGR, &(aMyGraph->graph) );
    }
    else
    {
        //nothing to od
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeCountAll( qcStatement * aStatement,
                           qmgGROP     * aMyGraph )
{
    qmoLeafInfo       sLeafInfo;
    qmnPlan         * sCUNT;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeCountAll::__FT__" );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //----------------------------
    // init CUNT
    //----------------------------

    IDE_TEST( qmoOneNonPlan::initCUNT( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       aMyGraph->graph.myPlan,
                                       &sCUNT )
              != IDE_SUCCESS );

    //----------------------------
    // ���� plan�� �������� �ʴ´�.
    //----------------------------

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //---------------------------------------------------
    // Current CNF�� ���
    //---------------------------------------------------

    if ( aMyGraph->graph.left->myCNF != NULL )
    {
        aMyGraph->graph.myQuerySet->SFWGH->crtPath->currentCNF =
            aMyGraph->graph.left->myCNF;
    }
    else
    {
        // Nothing To Do
    }

    sLeafInfo.predicate         = aMyGraph->graph.left->myPredicate;
    sLeafInfo.levelPredicate    = NULL;
    sLeafInfo.constantPredicate = aMyGraph->graph.left->constantPredicate;
    sLeafInfo.preservedOrder    = aMyGraph->graph.preservedOrder;
    sLeafInfo.ridPredicate      = aMyGraph->graph.left->ridPredicate;

    // BUG-17483 ��Ƽ�� ���̺� count(*) ����
    if( aMyGraph->graph.left->type == QMG_SELECTION )
    {
        sLeafInfo.index             = ((qmgSELT*)aMyGraph->graph.left)->selectedIndex;
        sLeafInfo.sdf               = ((qmgSELT*)aMyGraph->graph.left)->sdf;
        sLeafInfo.forceIndexScan    = ((qmgSELT*)aMyGraph->graph.left)->forceIndexScan;
    }
    else if ( ( aMyGraph->graph.left->type == QMG_PARTITION ) ||
              ( aMyGraph->graph.left->type == QMG_SHARD_SELECT ) ) // PROJ-2638
    {
        sLeafInfo.index             = NULL;
        sLeafInfo.sdf               = NULL;
        sLeafInfo.forceIndexScan    = ID_FALSE;
    }
    else
    {
        IDE_FT_ASSERT( 0 );
    }

    //----------------------------
    // make CUNT
    //----------------------------

    IDE_TEST( qmoOneNonPlan::makeCUNT( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myFrom ,
                                       aMyGraph->aggregation ,
                                       &sLeafInfo ,
                                       sCUNT )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sCUNT;

    qmg::setPlanInfo( aStatement, sCUNT, &(aMyGraph->graph) );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeIndexMinMax( qcStatement * aStatement,
                              qmgGROP     * aMyGraph )
{
    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeIndexMinMax::__FT__" );

    //----------------------------
    // ���� plan ����
    //----------------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph )
              != IDE_SUCCESS );

    //�ش� graph�� �����Ǵ� node�� �����Ƿ� ������ childplan��
    //���� graph�� myPlan���� ����صд�.
    //���� �������� ���� Plan�� ã���� �ִ�.
    aMyGraph->graph.myPlan = aMyGraph->graph.left->myPlan;

    // To Fix PR-9602
    // ������ ���� ���Ǹ� ó���ϱ� ���ؼ���
    // INDEXABLE MINMAX ����ȭ�� ����� ���,
    // Conversion ������ ���� ���� ���� �����ϰ� �Ϸ���,
    // Aggregation Node�� ID��
    // �ش� argument�� ID�� ������� �־�� �Ѵ�.
    //     SELECT MIN(i1) FROM T1;
    //     SELECT MIN(i1) + 0.1 FROM T1;

    IDE_DASSERT( aMyGraph->aggregation != NULL );
    IDE_DASSERT( aMyGraph->aggregation->next == NULL );

    aMyGraph->aggregation->aggr->node.table
        = aMyGraph->aggregation->aggr->node.arguments->table;
    aMyGraph->aggregation->aggr->node.column
        = aMyGraph->aggregation->aggr->node.arguments->column;

    // PROJ-2179
    //aMyGraph->aggregation->aggr->node.arguments = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::makeIndexDistAggr( qcStatement * aStatement,
                                qmgGROP     * aMyGraph )
{
    UInt          sFlag;
    qmnPlan     * sGRBY;
    qmnPlan     * sGRAG;
    qmsConcatElement * sGroupBy;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeIndexDistAggr::__FT__" );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //----------------------------
    // init GRAG
    //----------------------------

    IDE_TEST( qmoOneMtrPlan::initGRAG( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->aggregation ,
                                       NULL,
                                       aMyGraph->graph.myPlan ,
                                       &sGRAG )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRAG;

    //----------------------------
    // init GRBY
    //----------------------------

    // To Fix PR-7960
    // Aggregation�� Argument�� �̿��Ͽ�
    // Grouping ��� Column�� �����Ѵ�.

    IDU_FIT_POINT( "qmgGrouping::makeIndexDistAggr::alloc::Group" );
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsConcatElement),
                                             (void **) & sGroupBy )
              != IDE_SUCCESS );
    sGroupBy->arithmeticOrList =
        (qtcNode*) aMyGraph->aggregation->aggr->node.arguments;
    sGroupBy->next = NULL;

    IDE_TEST( qmoOneNonPlan::initGRBY( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->aggregation,
                                       sGroupBy ,
                                       &sGRBY ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRBY;

    //----------------------------
    // ���� plan ����
    //----------------------------

    IDE_TEST( makeChildPlan( aStatement,
                             aMyGraph )
              != IDE_SUCCESS );

    //----------------------------
    // make GRBY
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEGRBY_SORT_BASED_METHOD_MASK;
    sFlag |= QMO_MAKEGRBY_SORT_BASED_DISTAGGR;

    // GRBY ����� ����
    IDE_TEST( qmoOneNonPlan::makeGRBY( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       aMyGraph->graph.myPlan ,
                                       sGRBY ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRBY;

    qmg::setPlanInfo( aStatement, sGRBY, &(aMyGraph->graph) );

    //----------------------------
    // GRAG ����� ����
    // - Aggregation�� ó�� �Ѵ�.
    //----------------------------

    sFlag = 0;

    //���� ��ü�� ����
    //GROUP BY�� ���� ��� �̴�.(bug-7696)
    sFlag &= ~QMO_MAKEGRAG_TEMP_TABLE_MASK;
    sFlag |= QMO_MAKEGRAG_MEMORY_TEMP_TABLE;

    IDE_DASSERT( aMyGraph->groupBy == NULL );

    IDE_TEST( qmoOneMtrPlan::makeGRAG( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       NULL ,
                                       aMyGraph->hashBucketCnt ,
                                       aMyGraph->graph.myPlan ,
                                       sGRAG )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRAG;

    qmg::setPlanInfo( aStatement, sGRBY, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeRollup
 *
 * - Rollup �� ������ Sort Plan�� �����Ǹ� ���� Index�� ������ �ÿ��� Sort�� �������� �ʴ´�.
 * - Memory Table�� ��� ���� Plan( Sort, Window Sort, Limit Sort )���� �����͸� �׾Ƽ�
 *   ó���� �ʿ䰡 ���� ��� Memory Table�� value�� STORE�ϴ� Temp Table�� �����ؼ�
 *   �� Table�� Row Pointer�� �׾Ƽ� ó���ϵ��� �Ѵ�.
 */
IDE_RC qmgGrouping::makeRollup( qcStatement    * aStatement,
                                qmgGROP        * aMyGraph )
{
    UInt      sFlag        = 0;
    UInt      sRollupCount = 0;
    qmnPlan * sROLL        = NULL;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeRollup::__FT__" );

    IDE_DASSERT( aMyGraph->groupBy != NULL );

    IDE_TEST( qmoOneMtrPlan::initROLL( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       &sRollupCount,
                                       aMyGraph->aggregation,
                                       aMyGraph->groupBy,
                                       &sROLL )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sROLL;

    IDE_TEST( makeChildPlan( aStatement, aMyGraph )
              != IDE_SUCCESS );

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

    if ( ( aMyGraph->graph.flag & QMG_VALUE_TEMP_MASK )
         == QMG_VALUE_TEMP_TRUE )
    {
        sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
        sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    if ( ( aMyGraph->graph.flag & QMG_CHILD_PRESERVED_ORDER_USE_MASK ) ==
         QMG_CHILD_PRESERVED_ORDER_CANNOT_USE )
    {
        sFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
        sFlag |= QMO_MAKESORT_PRESERVED_FALSE;
    }
    else
    {
        sFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
        sFlag |= QMO_MAKESORT_PRESERVED_TRUE;
    }

    IDE_TEST( qmoOneMtrPlan::makeROLL( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       sFlag,
                                       sRollupCount,
                                       aMyGraph->aggregation,
                                       aMyGraph->distAggArg,
                                       aMyGraph->groupBy,
                                       aMyGraph->graph.myPlan,
                                       sROLL )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sROLL;

    qmg::setPlanInfo( aStatement, sROLL, &(aMyGraph->graph) );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * PROJ-1353 makeCube
 *
 * - Memory Table�� ��� ���� Plan( Sort, Window Sort, Limit Sort )���� �����͸� �׾Ƽ�
 *   ó���� �ʿ䰡 ���� ��� Memory Table�� value�� STORE�ϴ� Temp Table�� �����ؼ�
 *   �� Table�� Row Pointer�� �׾Ƽ� ó���ϵ��� �Ѵ�.
 */
IDE_RC qmgGrouping::makeCube( qcStatement    * aStatement,
                              qmgGROP        * aMyGraph )
{
    UInt      sFlag        = 0;
    UInt      sCubeCount   = 0;
    qmnPlan * sCUBE        = NULL;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeCube::__FT__" );

    IDE_DASSERT( aMyGraph->groupBy != NULL );

    IDE_TEST( qmoOneMtrPlan::initCUBE( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       &sCubeCount,
                                       aMyGraph->aggregation,
                                       aMyGraph->groupBy,
                                       &sCUBE )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sCUBE;

    IDE_TEST( makeChildPlan( aStatement, aMyGraph )
              != IDE_SUCCESS );

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

    if ( ( aMyGraph->graph.flag & QMG_VALUE_TEMP_MASK )
         == QMG_VALUE_TEMP_TRUE )
    {
        sFlag &= ~QMO_MAKESORT_VALUE_TEMP_MASK;
        sFlag |= QMO_MAKESORT_VALUE_TEMP_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( qmoOneMtrPlan::makeCUBE( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       sFlag,
                                       sCubeCount,
                                       aMyGraph->aggregation,
                                       aMyGraph->distAggArg,
                                       aMyGraph->groupBy,
                                       aMyGraph->graph.myPlan,
                                       sCUBE )
              != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sCUBE;

    qmg::setPlanInfo( aStatement, sCUBE, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgGrouping::printGraph( qcStatement  * aStatement,
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

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::printGraph::__FT__" );

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

IDE_RC
qmgGrouping::setGroupingMethod( qcStatement * aStatement,
                                qmgGROP     * aGroupGraph,
                                SDouble       aRecordSize,
                                SDouble       aAggrCost )
{
/***********************************************************************
 *
 * Description : GROUP BY �÷��� ���� ����ȭ�� ������.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool               sIndexable;
    qmoGroupMethodType   sGroupMethodHint;

    SDouble              sTotalCost     = 0;
    SDouble              sDiskCost      = 0;
    SDouble              sAccessCost    = 0;

    SDouble              sSelAccessCost = 0;
    SDouble              sSelDiskCost   = 0;
    SDouble              sSelTotalCost  = 0;
    idBool               sIsDisk;
    qmgGraph           * sGraph;
    UInt                 sSelectedMethod = 0;
    qmsAggNode         * sAggr;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::setGroupingMethod::__FT__" );

    //-------------------------------------------
    // ���ռ� �˻�
    //-------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGroupGraph != NULL );
    IDE_DASSERT( aGroupGraph->groupBy != NULL );

    //-------------------------------------------
    // GROUPING METHOD HINT ����
    //-------------------------------------------
    sGraph = &(aGroupGraph->graph);

    IDE_TEST( qmg::isDiskTempTable( sGraph, & sIsDisk ) != IDE_SUCCESS );

    /* TASK-6744 */
    if ( (QCU_PLAN_RANDOM_SEED != 0) &&
         (aStatement->mRandomPlanInfo != NULL) &&
         (smiGetStartupPhase() == SMI_STARTUP_SERVICE) )
    {
        aStatement->mRandomPlanInfo->mTotalNumOfCases = aStatement->mRandomPlanInfo->mTotalNumOfCases + 2;
        sSelectedMethod = QCU_PLAN_RANDOM_SEED % (aStatement->mRandomPlanInfo->mTotalNumOfCases - aStatement->mRandomPlanInfo->mWeightedValue);

        if ( sSelectedMethod >= 2 )
        {
            sSelectedMethod = sSelectedMethod % 2;
        }
        else
        {
            // Nothing to do.
        }

        IDE_DASSERT( sSelectedMethod < 2 );

        switch ( sSelectedMethod )
        {
            case 0 :
                sGroupMethodHint = QMO_GROUP_METHOD_TYPE_SORT;
                break;
            case 1 :
                sGroupMethodHint = QMO_GROUP_METHOD_TYPE_HASH;
                break;
            default :
                IDE_DASSERT( 0 );
                break;
        }

        aStatement->mRandomPlanInfo->mWeightedValue++;

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED );
    }
    else
    {
        // BUG-48132 group method property �߰� / grouping hint �켱
        if ( aGroupGraph->graph.myQuerySet->SFWGH->hints->groupMethodType == QMO_GROUP_METHOD_TYPE_NOT_DEFINED )
        {
            getPropertyGroupingMethod( aStatement,
                                       &sGroupMethodHint ); 
        }
        else
        {
            sGroupMethodHint =
                aGroupGraph->graph.myQuerySet->SFWGH->hints->groupMethodType;
        }
    }

    /* BUG-44250
     * aggregation�� group by �� �ְ� ������ hierarchy query��
     * ���� �ϴµ� aggregation�� ���ڷ� prior�� �����ٸ� Sort��
     * Ǯ���� �ʵ��� �Ѵ�. hint�� ������ ����
     */
    if ( ( aGroupGraph->aggregation != NULL ) &&
         ( aGroupGraph->groupBy != NULL ) &&
         ( aGroupGraph->graph.left->type == QMG_HIERARCHY ) )
    {
        for ( sAggr = aGroupGraph->aggregation;
              sAggr != NULL;
              sAggr = sAggr->next )
        {
            if ( ( sAggr->aggr->lflag & QTC_NODE_PRIOR_MASK )
                 == QTC_NODE_PRIOR_EXIST )
            {
                sGroupMethodHint = QMO_GROUP_METHOD_TYPE_HASH;
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-48128
    // outer query�� aggregation + group by �̰�,
    // target or having ���� ����� subquery ( scalar subquery)���� outer column�� ����� ��� 
    // sort �޼ҵ带 ������� �ʵ��� �մϴ�.
    if ( ( sGroupMethodHint != QMO_GROUP_METHOD_TYPE_HASH ) &&
         ( aGroupGraph->aggregation != NULL ) &&
         ( aGroupGraph->groupBy != NULL ) &&
         ( (aGroupGraph->graph.myQuerySet->lflag & QMV_QUERYSET_SCALAR_SUBQ_OUTER_COL_MASK)
           == QMV_QUERYSET_SCALAR_SUBQ_OUTER_COL_TRUE ) )
    {
        sGroupMethodHint = QMO_GROUP_METHOD_TYPE_HASH;
    }
    else
    {
        /* Nothing to do */
    }

    switch( sGroupMethodHint )
    {
        case QMO_GROUP_METHOD_TYPE_NOT_DEFINED :
            // Group Method Hint�� ���� ���

            // To Fix PR-12394
            // GROUPING ���� ��Ʈ�� ���� ��쿡��
            // ����ȭ Tip�� �����ؾ� ��.

            //------------------------------------------
            // To Fix PR-12396
            // ��� ����� ���� ���� ����� �����Ѵ�.
            //------------------------------------------

            //------------------------------------------
            // Sorting �� �̿��� ����� ��� ���
            //------------------------------------------

            // ��� Grouping�� Sorting����� ����� �� �ִ�.
            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                             &(sGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                              &(sGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              aRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }

            /* BUG-39284 The sys_connect_by_path function with Aggregate
             * function is not correct. 
             */
            if ( ( aGroupGraph->graph.myQuerySet->SFWGH->lflag & QMV_SFWGH_CONNECT_BY_FUNC_MASK )
                 == QMV_SFWGH_CONNECT_BY_FUNC_TRUE ) 
            {
                IDE_TEST_RAISE( aGroupGraph->distAggArg != NULL, ERR_NOT_DIST_AGGR_WITH_CONNECT_BY_FUNC );

                sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                sGraph->flag |= QMG_SORT_HASH_METHOD_HASH;
            }
            else
            {
                sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                sGraph->flag |= QMG_SORT_HASH_METHOD_SORT;
            }

            //------------------------------------------
            // Hashing �� �̿��� ����� �����
            //------------------------------------------
            if ( aGroupGraph->distAggArg == NULL )
            {
                if( sIsDisk == ID_FALSE )
                {
                    sTotalCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                              &(sGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT );
                    sAccessCost = sTotalCost;
                    sDiskCost   = 0;
                }
                else
                {
                    // BUG-37752
                    // DiskHashGroup �϶� ���۹���� �ٸ��Ƿ� ������ cost �� ����Ѵ�.
                    // hashBucketCnt �� ������ �޴´�.
                    sTotalCost = qmoCost::getDiskHashGroupCost( aStatement->mSysStat,
                                                                &(sGraph->left->costInfo),
                                                                aGroupGraph->hashBucketCnt,
                                                                QMO_COST_DEFAULT_NODE_CNT,
                                                                aRecordSize );

                    sAccessCost = 0;
                    sDiskCost   = sTotalCost;
                }
            }
            else
            {
                sTotalCost = QMO_COST_INVALID_COST;
            }

            if (QMO_COST_IS_EQUAL(sTotalCost, QMO_COST_INVALID_COST) == ID_TRUE)
            {
                // Hashing ����� ������ �� ���� �����
            }
            else
            {
                if (QMO_COST_IS_GREATER(sTotalCost, sSelTotalCost) == ID_TRUE)
                {
                    // Nothing to do
                }
                else
                {
                    // Hashing ����� ���� ����
                    sSelTotalCost  = sTotalCost;
                    sSelDiskCost   = sDiskCost;
                    sSelAccessCost = sAccessCost;

                    sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                    sGraph->flag |= QMG_SORT_HASH_METHOD_HASH;

                    sGraph->preservedOrder = NULL;

                    sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sGraph->flag |= QMG_PRESERVED_ORDER_NEVER;
                }
            }

            //------------------------------------------
            // Preserved Order �� �̿��� ����� �����
            //------------------------------------------

            IDE_TEST( getCostByPrevOrder( aStatement,
                                          aGroupGraph,
                                          & sAccessCost,
                                          & sDiskCost,
                                          & sTotalCost )
                      != IDE_SUCCESS );

            if (QMO_COST_IS_EQUAL(sTotalCost, QMO_COST_INVALID_COST) == ID_TRUE)
            {
                // Preserved Order ����� ������ �� ���� �����
            }
            else
            {
                if (QMO_COST_IS_GREATER(sTotalCost, sSelTotalCost) == ID_TRUE)
                {
                    // Nothing to do
                }
                else
                {
                    // Preserved Order ����� ���� ����
                    sSelTotalCost  = sTotalCost;
                    sSelDiskCost   = sDiskCost;
                    sSelAccessCost = sAccessCost;

                    // To Fix PR-12394
                    // GROUPING ���� ��Ʈ�� ���� ��쿡��
                    // ����ȭ Tip�� �����ؾ� ��.
                    IDE_TEST( indexableGroupBy( aStatement,
                                                aGroupGraph,
                                                & sIndexable )
                              != IDE_SUCCESS );

                    IDE_DASSERT( sIndexable == ID_TRUE );

                    // Indexable Group By �� ������ ���
                    sGraph->flag &= ~QMG_GROP_OPT_TIP_MASK;
                    sGraph->flag
                        |= QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY;

                    sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                    sGraph->flag |= QMG_SORT_HASH_METHOD_SORT;

                    // �̹� Preserved Order�� ������

                    sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
                }
            }

            //------------------------------------------
            // ������
            //------------------------------------------

            // Sorting ����� ���õ� ��� Preserved Order ����
            if ( ( ( sGraph->flag & QMG_SORT_HASH_METHOD_MASK )
                   == QMG_SORT_HASH_METHOD_SORT )
                 &&
                 ( ( sGraph->flag & QMG_GROP_OPT_TIP_MASK )
                   != QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY ) )
            {
                sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                sGraph->flag |= QMG_SORT_HASH_METHOD_SORT;

                // Group By �÷��� �̿��Ͽ� Preserved Order�� ������.
                IDE_TEST(
                    makeGroupByOrder( aStatement,
                                      aGroupGraph->groupBy,
                                      & sGraph->preservedOrder )
                    != IDE_SUCCESS );

                sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
                sGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            }
            else
            {
                // �ٸ� Method�� ���õ�
            }

            break;

        case QMO_GROUP_METHOD_TYPE_HASH :
            // Group Method Hint�� Hash based �� ���

            if ( aGroupGraph->distAggArg == NULL )
            {
                if( sIsDisk == ID_FALSE )
                {
                    sSelTotalCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                                 &(sGraph->left->costInfo),
                                                                 QMO_COST_DEFAULT_NODE_CNT );
                    sSelAccessCost = sSelTotalCost;
                    sSelDiskCost   = 0;
                }
                else
                {
                    // BUG-37752
                    // DiskHashGroup �϶� ���۹���� �ٸ��Ƿ� ������ cost �� ����Ѵ�.
                    // hashBucketCnt �� ������ �޴´�.
                    sSelTotalCost = qmoCost::getDiskHashGroupCost( aStatement->mSysStat,
                                                                   &(sGraph->left->costInfo),
                                                                   aGroupGraph->hashBucketCnt,
                                                                   QMO_COST_DEFAULT_NODE_CNT,
                                                                   aRecordSize );

                    sSelAccessCost = 0;
                    sSelDiskCost   = sSelTotalCost;
                }

                sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
                sGraph->flag |= QMG_SORT_HASH_METHOD_HASH;

                sGraph->preservedOrder = NULL;

                sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
                sGraph->flag |= QMG_PRESERVED_ORDER_NEVER;

                break;
            }
            else
            {
                // Distinct Aggregation�� ���
                // Hash-based Grouping�� ����� �� ����.
                // �Ʒ��� Sort-based Grouping�� ����Ѵ�.
            }
            /* fall through  */
        case QMO_GROUP_METHOD_TYPE_SORT :

            // Group Method Hint�� Sort based �� ���
            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                             &(sGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                              &(sGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              aRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }

            sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
            sGraph->flag |= QMG_SORT_HASH_METHOD_SORT;

            // Group By �÷��� �̿��Ͽ� Preserved Order�� ������.
            IDE_TEST( makeGroupByOrder( aStatement,
                                        aGroupGraph->groupBy,
                                        & sGraph->preservedOrder )
                      != IDE_SUCCESS );

            sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
            sGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            break;
        default :
            IDE_DASSERT( 0 );
            break;
    }

    /* BUG-39284 The sys_connect_by_path function with Aggregate
     * function is not correct. 
     */
    IDE_TEST_RAISE( ( ( aGroupGraph->graph.myQuerySet->SFWGH->lflag & QMV_SFWGH_CONNECT_BY_FUNC_MASK )
                      == QMV_SFWGH_CONNECT_BY_FUNC_TRUE ) &&
                    ( ( sGraph->flag & QMG_SORT_HASH_METHOD_MASK )
                      == QMG_SORT_HASH_METHOD_SORT ), 
                    ERR_NOT_DIST_AGGR_WITH_CONNECT_BY_FUNC );

    // ��� ���� ����
    sGraph->costInfo.myAccessCost = sSelAccessCost + aAggrCost;
    sGraph->costInfo.myDiskCost   = sSelDiskCost;
    sGraph->costInfo.myAllCost    = sSelTotalCost  + aAggrCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_DIST_AGGR_WITH_CONNECT_BY_FUNC )
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCP_NOT_SUPPORTED_SYNTAX));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgGrouping::makeGroupByOrder( qcStatement        * aStatement,
                               qmsConcatElement   * aGroupBy,
                               qmgPreservedOrder ** sNewGroupByOrder )
{
/***********************************************************************
 *
 * Description : Group By �÷��� �̿��Ͽ�
 *               Preserved Order �ڷ� ������ ������.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsConcatElement  * sGroupBy;
    qmgPreservedOrder * sWantOrder;
    qmgPreservedOrder * sNewOrder;
    qmgPreservedOrder * sCurOrder;
    qtcNode           * sNode;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::makeGroupByOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sWantOrder = NULL;
    sCurOrder  = NULL;

    //------------------------------------------
    // Group by Į���� ���� want order�� ����
    //------------------------------------------

    for ( sGroupBy = aGroupBy; sGroupBy != NULL; sGroupBy = sGroupBy->next )
    {
        sNode = sGroupBy->arithmeticOrList;

        //------------------------------------------
        // Group by Į���� ���� want order�� ����
        //------------------------------------------
        IDU_FIT_POINT( "qmgGrouping::makeGroupByOrder::alloc::NewOrder" );
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                 (void**)&sNewOrder )
                  != IDE_SUCCESS );

        sNewOrder->table = sNode->node.table;
        sNewOrder->column = sNode->node.column;
        sNewOrder->direction = QMG_DIRECTION_NOT_DEFINED;
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

    *sNewGroupByOrder = sWantOrder;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgGrouping::indexableGroupBy( qcStatement        * aStatement,
                               qmgGROP            * aGroupGraph,
                               idBool             * aIndexableGroupBy )
{
/***********************************************************************
 *
 * Description : Indexable Group By ����ȭ ������ ���, ����
 *
 * Implementation :
 *
 *    Preserved Order ��� ������ ���, ����
 *
 ***********************************************************************/

    qmgPreservedOrder * sWantOrder;

    idBool              sSuccess;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::indexableGroupBy::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGroupGraph != NULL );
    IDE_DASSERT( aGroupGraph->groupBy != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sSuccess   = ID_FALSE;
    sWantOrder = NULL;

    //------------------------------------------
    // Group by Į���� ���� want order�� ����
    //------------------------------------------

    IDE_TEST( makeGroupByOrder( aStatement,
                                aGroupGraph->groupBy,
                                & sWantOrder )
              != IDE_SUCCESS );

    //------------------------------------------
    // Preserved Order�� ��� ���� ���θ� �˻�
    //------------------------------------------

    IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                      & aGroupGraph->graph,
                                      sWantOrder,
                                      & sSuccess )
              != IDE_SUCCESS );

    if ( sSuccess == ID_TRUE )
    {
        aGroupGraph->graph.preservedOrder = sWantOrder;
        *aIndexableGroupBy = ID_TRUE;
    }
    else
    {
        aGroupGraph->graph.preservedOrder = NULL;
        *aIndexableGroupBy = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmgGrouping::countStar( qcStatement      * aStatement,
                        qmgGROP          * aGroupGraph,
                        idBool           * aCountStar )
{
/***********************************************************************
 *
 * Description : Count(*) ����ȭ ������ ���, ����
 *
 * Implementation :
 *    (1) CNF�� ���
 *    (2) �ϳ��� base table�� ���� ����
 *    (3) limit�� ���� ���
 *    (4) hierarchy query�� ���� ���
 *    (5) distinct�� ���� ���
 *    (6) order by�� ���� ���
 *    (7) subquery filter�� ���� ���
 *    (8) rownum�� ������ ���� ���
 *
 ***********************************************************************/

    idBool sTemp;
    idBool sIsCountStar;

    qmsSFWGH    * sMySFWGH;
    qmgChildren * sChild;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::countStar::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGroupGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sTemp = ID_TRUE;
    sIsCountStar = ID_TRUE;
    sMySFWGH = aGroupGraph->graph.myQuerySet->SFWGH;

    while( sTemp == ID_TRUE )
    {
        sTemp = ID_FALSE;

        // CNF�� ���
        if ( sMySFWGH->crtPath->normalType != QMO_NORMAL_TYPE_CNF )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // BUG-35155 Partial CNF
        // NNF ���Ͱ� �����ϸ� count star ����ȭ�� �����ϸ� �ȵȴ�.
        // (���� �� ��� �׻� CNF �̴�)
        if ( sMySFWGH->crtPath->crtCNF->nnfFilter != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // �ϳ��� base table�� ���� ����
        if ( sMySFWGH->from->next != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // To Fix BUG-9072
        // base graph�� type�� join�� �ƴ� ���
        if ( sMySFWGH->from->joinType != QMS_NO_JOIN )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // To Fix BUG-9148, 8753, 8745, 8786
        // view�� �ƴ� ���
        if ( sMySFWGH->from->tableRef->view != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // PROJ-1502 PARTITIONED DISK TABLE
            if( sMySFWGH->from->tableRef->tableInfo->tablePartitionType
                == QCM_PARTITIONED_TABLE )
            {
                sChild = aGroupGraph->graph.left->children;

                // BUG-17483 ��Ƽ�� ���̺� count(*) ����
                // ��Ƽ�� ���̺��� ��� ���Ͱ� �޷��ִ� ��쿡�� count(*) ����ȭ�� ����
                // ��Ƽ�� ���̺��� QMND_CUNT_METHOD_HANDLE ��ĸ� �����Ѵ�.
                if( ((qmsParseTree*)(aStatement->myPlan->parseTree))->querySet->setOp != QMS_NONE )
                {
                    sIsCountStar = ID_FALSE;
                    break;
                }
                else
                {
                    // BUG-41700
                    if( ( aGroupGraph->graph.left->constantPredicate != NULL ) ||
                        ( aGroupGraph->graph.left->myPredicate       != NULL ) ||
                        ( aGroupGraph->graph.left->ridPredicate      != NULL ) )
                    {
                        sIsCountStar = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    while( sChild != NULL )
                    {
                        if( ( sChild->childGraph->constantPredicate != NULL ) ||
                            ( sChild->childGraph->myPredicate       != NULL ) ||
                            ( sChild->childGraph->ridPredicate      != NULL ) )
                        {
                            sIsCountStar = ID_FALSE;
                            break;
                        }
                        else
                        {
                            sChild = sChild->next;
                        }
                    }
                }
            }
            else
            {
                // Nothing to do.
            }
        }

        // To Fix BUG-8711
        // select type�� distinct�� �ƴ� ���
        if ( sMySFWGH->selectType == QMS_DISTINCT )
        {
            sIsCountStar = ID_FALSE;
        }
        else
        {
            // nothing to do
        }

        // limit�� ���� ���
        if ( ((qmsParseTree*)(aStatement->myPlan->parseTree))->limit != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // hierarchy�� ���� ���
        if ( sMySFWGH->hierarchy != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // distinct aggregation�� ���� ���
        if ( aGroupGraph->distAggArg != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // order by�� ���� ���
        if ( ((qmsParseTree*)(aStatement->myPlan->parseTree))->orderBy != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        // subquery filter�� ���� ���
        if ( sMySFWGH->where != NULL )
        {
            if ( ( sMySFWGH->where->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                sIsCountStar = ID_FALSE;
                break;
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

        // rownum�� ������ ���� ���
        if ( sMySFWGH->rownum != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            // nothing to do
        }

        /* PROJ-1832 New database link */
        if ( sMySFWGH->from->tableRef->remoteTable != NULL )
        {
            sIsCountStar = ID_FALSE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    *aCountStar = sIsCountStar;

    return IDE_SUCCESS;

}

IDE_RC
qmgGrouping::nonGroupOptTip( qcStatement * aStatement,
                             qmgGROP     * aGroupGraph,
                             SDouble       aRecordSize,
                             SDouble       aAggrCost )
{
/***********************************************************************
 *
 * Description : GROUP BY�� ���� ����� tip ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode            * sAggNode;
    qtcNode            * sColNode;
    qmgPreservedOrder  * sNewOrder;
    qmgGraph           * sLeftGraph;
    qmgGraph           * sGraph;

    SDouble              sTotalCost     = 0;
    SDouble              sDiskCost      = 0;
    SDouble              sAccessCost    = 0;
    idBool               sIsDisk;
    idBool               sSuccess;

    extern mtfModule     mtfMin;
    extern mtfModule     mtfMax;
    extern mtfModule     mtfCount;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::nonGroupOptTip::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGroupGraph != NULL );
    IDE_FT_ASSERT( aGroupGraph->aggregation != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sSuccess = ID_FALSE;
    sGraph   = &(aGroupGraph->graph);

    //------------------------------------------
    //  aggregation�� �ϳ��� �����ϸ鼭
    //  �� aggregation ��� Į���� ������ Į���� ���
    //------------------------------------------

    if ( aGroupGraph->aggregation->next == NULL )
    {
        sAggNode = (qtcNode*) aGroupGraph->aggregation->aggr;
        sColNode = (qtcNode*)sAggNode->node.arguments;

        if ( sColNode == NULL )
        {
            if ( sAggNode->node.module == & mtfCount )
            {

                //------------------------------------------
                // count(*) ����ȭ
                //------------------------------------------

                IDE_TEST( countStar( aStatement,
                                     aGroupGraph,
                                     & sSuccess )
                          != IDE_SUCCESS );

                if ( sSuccess == ID_TRUE )
                {
                    sGraph->flag &= ~QMG_GROP_OPT_TIP_MASK;
                    sGraph->flag |= QMG_GROP_OPT_TIP_COUNT_STAR;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // BUG-12542
                // COUNT(*) �ӿ��� �ұ��ϰ� �̰����� ���� ��찡 ����.
                // Nothing To Do
            }
        }
        else
        {
            // want order ����
            IDU_FIT_POINT( "qmgGrouping::nonGroupOptTip::alloc::NewOrder" );
            IDE_TEST(
                QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                               (void**)&sNewOrder )
                != IDE_SUCCESS );

            sNewOrder->table = sColNode->node.table;
            sNewOrder->column = sColNode->node.column;
            sNewOrder->direction = QMG_DIRECTION_NOT_DEFINED;
            sNewOrder->next = NULL;

            if ( ( sAggNode->node.lflag & MTC_NODE_DISTINCT_MASK )
                 == MTC_NODE_DISTINCT_TRUE )
            {
                //------------------------------------------
                //  indexable Distinct Aggregation ����ȭ
                //------------------------------------------

                IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                                  sGraph,
                                                  sNewOrder,
                                                  & sSuccess )
                          != IDE_SUCCESS );

                if ( sSuccess == ID_TRUE )
                {
                    sGraph->flag &= ~QMG_GROP_OPT_TIP_MASK;
                    sGraph->flag |=
                        QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG;
                }
                else
                {
                    // nothing to
                }
            }
            else
            {
                if ( ( sAggNode->node.module == & mtfMin ) ||
                     ( sAggNode->node.module == & mtfMax ) )
                {
                    // To Fix PR-11562
                    // Indexable MIN-MAX ����ȭ�� ���,
                    // Direction�� ���ǵǾ�� ��.
                    if ( sAggNode->node.module == & mtfMin )
                    {
                        // MIN() �� ��� ASC Order�� ��밡������ �˻�
                        sNewOrder->direction = QMG_DIRECTION_ASC;
                    }
                    else
                    {
                        // MAX() �� ��� DESC Order�� ��밡������ �˻�
                        sNewOrder->direction = QMG_DIRECTION_DESC;
                    }

                    //------------------------------------------
                    //  indexable Min Max ����ȭ
                    //------------------------------------------

                    IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                                      sGraph,
                                                      sNewOrder,
                                                      & sSuccess )
                              != IDE_SUCCESS );

                    if ( sSuccess == ID_TRUE )
                    {
                        sGraph->flag &= ~QMG_GROP_OPT_TIP_MASK;
                        sGraph->flag |=
                            QMG_GROP_OPT_TIP_INDEXABLE_MINMAX;
                        sGraph->flag &= ~QMG_INDEXABLE_MIN_MAX_MASK;
                        sGraph->flag |= QMG_INDEXABLE_MIN_MAX_TRUE;

                        // To Fix BUG-9919
                        for ( sLeftGraph = sGraph->left;
                              sLeftGraph != NULL;
                              sLeftGraph = sLeftGraph->left )
                        {
                            if ( (sLeftGraph->type == QMG_SELECTION) ||
                                 (sLeftGraph->type == QMG_PARTITION) )
                            {
                                if ( sLeftGraph->myFrom->tableRef->view
                                     != NULL )
                                {
                                    continue;
                                }
                                else
                                {
                                    sLeftGraph->flag &= ~QMG_SELT_NOTNULL_KEYRANGE_MASK;
                                    sLeftGraph->flag |=  QMG_SELT_NOTNULL_KEYRANGE_TRUE;
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
                        // preserved order ������ ���
                        // nothing to
                    }
                }
                else
                {
                    // min �Ǵ� max�� �ƴ� ���
                    // nothing to do
                }
            }
        }
    }
    else
    {
        // aggregation�� �ΰ� �̻��� ���,
        // nothing to do
    }

    IDE_TEST( qmg::isDiskTempTable( sGraph, & sIsDisk ) != IDE_SUCCESS );

    // TASK-6699 TPC-H ���� ����
    // group by �� ���������� cost �� �����
    if ( aGroupGraph->distAggArg != NULL )
    {
        // Distinct Aggregation�� ��� sort-based�� �����ؾ� ��.
        sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
        sGraph->flag |= QMG_SORT_HASH_METHOD_SORT;

        if ( sGraph->preservedOrder != NULL )
        {
            sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
            sGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
        }
        else
        {
            sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
            sGraph->flag |= QMG_PRESERVED_ORDER_NEVER;
        }

        if( sIsDisk == ID_FALSE )
        {
            sTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                      &(sGraph->left->costInfo),
                                                      QMO_COST_DEFAULT_NODE_CNT );
            sAccessCost = sTotalCost;
            sDiskCost   = 0;
        }
        else
        {
            sTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                       &(sGraph->left->costInfo),
                                                       QMO_COST_DEFAULT_NODE_CNT,
                                                       aRecordSize );
            sAccessCost = 0;
            sDiskCost   = sTotalCost;
        }
    }
    else
    {
        // Grouping Method�� hash-based�� ����
        sGraph->flag &= ~QMG_SORT_HASH_METHOD_MASK;
        sGraph->flag |= QMG_SORT_HASH_METHOD_HASH;

        sGraph->preservedOrder = NULL;

        sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
        sGraph->flag |= QMG_PRESERVED_ORDER_NEVER;

        if( sIsDisk == ID_FALSE )
        {
            sTotalCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                      &(sGraph->left->costInfo),
                                                      QMO_COST_DEFAULT_NODE_CNT );
            sAccessCost = sTotalCost;
            sDiskCost   = 0;
        }
        else
        {
            sTotalCost = qmoCost::getDiskHashGroupCost( aStatement->mSysStat,
                                                        &(sGraph->left->costInfo),
                                                        aGroupGraph->hashBucketCnt,
                                                        QMO_COST_DEFAULT_NODE_CNT,
                                                        aRecordSize );

            sAccessCost = 0;
            sDiskCost   = sTotalCost;
        }
    }

    switch ( sGraph->flag & QMG_GROP_OPT_TIP_MASK )
    {
        case QMG_GROP_OPT_TIP_NONE :

            sGraph->costInfo.myAccessCost = aAggrCost + sAccessCost;
            sGraph->costInfo.myDiskCost   = sDiskCost;
            sGraph->costInfo.myAllCost    = aAggrCost + sTotalCost;
            break;

        case QMG_GROP_OPT_TIP_COUNT_STAR :
        case QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG :
        case QMG_GROP_OPT_TIP_INDEXABLE_MINMAX :

            sGraph->costInfo.myAccessCost =
                sGraph->left->costInfo.outputRecordCnt *
                aStatement->mSysStat->mMTCallTime;
            sGraph->costInfo.myDiskCost   = 0;
            sGraph->costInfo.myAllCost    = sGraph->costInfo.myAccessCost;
            break;
        default :
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgGrouping::getBucketCnt4Group( qcStatement  * aStatement,
                                 qmgGROP      * aGroupGraph,
                                 UInt           aHintBucketCnt,
                                 UInt         * aBucketCnt )
{
/***********************************************************************
 *
 * Description : hash bucket count�� ����
 *
 * Implementation :
 *    - hash bucket count hint�� �������� ���� ���
 *      hash bucket count = MIN( ���� graph�� outputRecordCnt / 2,
 *                               Group Column���� cardinality �� )
 *    - hash bucket count hint�� ������ ���
 *      hash bucket count = hash bucket count hint ��
 *
 ***********************************************************************/

    qmoColCardInfo   * sColCardInfo;
    qmsConcatElement * sGroup;
    qtcNode          * sNode;

    SDouble            sCardinality;
    SDouble            sBucketCnt;
    ULong              sBucketCntOutput;

    idBool             sAllColumn;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::getBucketCnt4Group::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGroupGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sAllColumn = ID_TRUE;
    sCardinality = 1;
    sBucketCnt = 1;

    if ( aHintBucketCnt == QMS_NOT_DEFINED_BUCKET_CNT )
    {

        //------------------------------------------
        // hash bucket count hint�� �������� �ʴ� ���
        //------------------------------------------

        sBucketCnt = aGroupGraph->graph.left->costInfo.outputRecordCnt / 2.0;
        sBucketCnt = ( sBucketCnt < 1 ) ? 1 : sBucketCnt;

        //------------------------------------------
        // group column���� cardinality ���� ���Ѵ�.
        //------------------------------------------

        for ( sGroup = aGroupGraph->groupBy;
              sGroup != NULL;
              sGroup = sGroup->next )
        {
            sNode = sGroup->arithmeticOrList;

            if ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
            {
                // group ����� ������ Į���� ���
                sColCardInfo = QC_SHARED_TMPLATE(aStatement)->
                    tableMap[sNode->node.table].
                    from->tableRef->statInfo->colCardInfo;

                sCardinality *= sColCardInfo[sNode->node.column].columnNDV;
            }
            else
            {
                // BUG-37778 disk hash temp table size estimate
                // tpc-H Q9 ���� group by ������ ����� ���� ����
                // EXTRACT(O_ORDERDATE,'year') AS O_YEAR
                // group by O_YEAR
                if( (sNode->node.arguments != NULL) &&
                    QTC_IS_COLUMN( aStatement, (qtcNode*)(sNode->node.arguments) ) == ID_TRUE )
                {
                    sColCardInfo = QC_SHARED_TMPLATE(aStatement)->
                        tableMap[sNode->node.arguments->table].
                        from->tableRef->statInfo->colCardInfo;

                    sCardinality *= sColCardInfo[sNode->node.arguments->column].columnNDV;
                }
                else
                {
                    sAllColumn = ID_FALSE;
                    break;
                }
            }
        }

        if ( sAllColumn == ID_TRUE )
        {
            //------------------------------------------
            // MIN( ���� graph�� outputRecordCnt / 2,
            //      Group Column���� cardinality �� )
            //------------------------------------------

            if ( sBucketCnt > sCardinality )
            {
                sBucketCnt = sCardinality;
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

        //  hash bucket count�� ����
        if ( sBucketCnt < QCU_OPTIMIZER_BUCKET_COUNT_MIN )
        {
            sBucketCnt = QCU_OPTIMIZER_BUCKET_COUNT_MIN;
        }
        else
        {
            if ( (aGroupGraph->graph.flag & QMG_GRAPH_TYPE_MASK) == QMG_GRAPH_TYPE_MEMORY )
            {
                // QMC_MEM_HASH_MAX_BUCKET_CNT ������ �������ش�.
                /* BUG-48161 */
                if ( sBucketCnt > QCG_GET_BUCKET_COUNT_MAX( aStatement )  )
                {
                    sBucketCnt = QCG_GET_BUCKET_COUNT_MAX( aStatement );
                }
                else
                {
                    // nothing todo.
                }
            }
            else
            {
                // nothing todo.
            }
        }
    }
    else
    {
        // bucket count hint�� �����ϴ� ���
        sBucketCnt = aHintBucketCnt;
    }

    // BUG-36403 �÷������� BucketCnt �� �޶����� ��찡 �ֽ��ϴ�.
    sBucketCntOutput = DOUBLE_TO_UINT64( sBucketCnt );
    *aBucketCnt      = (UInt)sBucketCntOutput;

    return IDE_SUCCESS;

}

IDE_RC
qmgGrouping::getCostByPrevOrder( qcStatement      * aStatement,
                                 qmgGROP          * aGroupGraph,
                                 SDouble          * aAccessCost,
                                 SDouble          * aDiskCost,
                                 SDouble          * aTotalCost )
{
/***********************************************************************
 *
 * Description :
 *
 *    Preserved Order ����� ����� Grouping ����� ����Ѵ�.
 *
 * Implementation :
 *
 *    �̹� Child�� ���ϴ� Preserved Order�� ������ �ִٸ�
 *    ������ ��� ���� Grouping�� �����ϴ�.
 *
 *    �ݸ� Child�� Ư�� �ε����� �����ϴ� �����,
 *    Child�� �ε����� �̿��� ����� ���Եǰ� �ȴ�.
 *
 ***********************************************************************/

    SDouble             sTotalCost;
    SDouble             sAccessCost;
    SDouble             sDiskCost;

    idBool              sUsable;

    qmgPreservedOrder * sWantOrder;

    qmoAccessMethod   * sOrgMethod;
    qmoAccessMethod   * sSelMethod;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::getCostByPrevOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGroupGraph != NULL );

    //------------------------------------------
    // Preserved Order�� ����� �� �ִ� ���� �˻�
    //------------------------------------------

    // Group by Į���� ���� want order�� ����
    sWantOrder = NULL;
    IDE_TEST( makeGroupByOrder( aStatement,
                                aGroupGraph->groupBy,
                                & sWantOrder )
              != IDE_SUCCESS );

    // preserved order ���� ���� �˻�
    IDE_TEST( qmg::checkUsableOrder( aStatement,
                                     sWantOrder,
                                     aGroupGraph->graph.left,
                                     & sOrgMethod,
                                     & sSelMethod,
                                     & sUsable )
              != IDE_SUCCESS );

    //------------------------------------------
    // ��� ���
    //------------------------------------------

    if ( sUsable == ID_TRUE )
    {
        if ( aGroupGraph->graph.left->preservedOrder == NULL )
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
            sAccessCost = aGroupGraph->graph.left->costInfo.outputRecordCnt *
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

IDE_RC
qmgGrouping::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Preserved Order�� direction�� �����Ѵ�.
 *                direction�� NOT_DEFINED �� ��쿡�� ȣ���Ͽ��� �Ѵ�.
 *
 *  Implementation :
 *    - ������ preserved order�� ���� ���,
 *      Child graph�� Preserved order direction�� �����Ѵ�.
 *    - ������ preserved order�� �ٸ� ���,
 *      Ascending���� direction�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool              sIsSamePrevOrderWithChild;
    qmgPreservedOrder * sPreservedOrder;
    qmgDirectionType    sPrevDirection;

    
    IDU_FIT_POINT_FATAL( "qmgGrouping::finalizePreservedOrder::__FT__" );

    // BUG-36803 Grouping Graph must have a left child graph
    IDE_DASSERT( aGraph->left != NULL );

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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * setGroupExtensionMethod
 *
 *   Rollup�� ��� Preserved Order�� �����ؼ� Sort Plan�� ���� ���θ� �����ϰ�
 *   Output order �� ������ Cost ����� �����Ѵ�.
 *   Cube�� ���� Cust ��길 �����Ѵ�.
 */
IDE_RC qmgGrouping::setPreOrderGroupExtension( qcStatement * aStatement,
                                               qmgGROP     * aGroupGraph,
                                               SDouble       aRecordSize )
{
    qmgGraph           * sGraph;
    qmsConcatElement  * sGroupBy;
    qmsConcatElement  * sSubGroup;
    qmgPreservedOrder * sWantOrder;
    qmgPreservedOrder * sNewOrder;
    qmgPreservedOrder * sCurOrder;
    idBool              sSuccess;
    qtcNode           * sNode;
    qtcNode           * sList;
    UInt                sCount;

    SDouble             sSelAccessCost = 0;
    SDouble             sSelDiskCost   = 0;
    SDouble             sSelTotalCost  = 0;
    idBool              sIsDisk;
    
    IDU_FIT_POINT_FATAL( "qmgGrouping::setPreOrderGroupExtension::__FT__" );

    sWantOrder = NULL;
    sCurOrder  = NULL;

    sGraph = &(aGroupGraph->graph);

    IDE_TEST( qmg::isDiskTempTable( sGraph, & sIsDisk ) != IDE_SUCCESS );

    if ( ( sGraph->flag & QMG_GROP_OPT_TIP_MASK ) ==
         QMG_GROP_OPT_TIP_ROLLUP )
    {
        for ( sGroupBy = aGroupGraph->groupBy;
              sGroupBy != NULL;
              sGroupBy = sGroupBy->next )
        {
            sNode = sGroupBy->arithmeticOrList;

            if ( sGroupBy->type != QMS_GROUPBY_NORMAL )
            {
                continue;
            }
            else
            {
                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                         (void**)&sNewOrder )
                          != IDE_SUCCESS );
                sNewOrder->table = sNode->node.table;
                sNewOrder->column = sNode->node.column;
                sNewOrder->direction = QMG_DIRECTION_ASC;
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

        for ( sGroupBy = aGroupGraph->groupBy;
              sGroupBy != NULL;
              sGroupBy = sGroupBy->next )
        {
            if ( sGroupBy->type != QMS_GROUPBY_NORMAL )
            {
                for ( sSubGroup = sGroupBy->arguments;
                      sSubGroup != NULL;
                      sSubGroup = sSubGroup->next )
                {
                    sNode = sSubGroup->arithmeticOrList;

                    if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                         == MTC_NODE_OPERATOR_LIST )
                    {
                        for ( sList = ( qtcNode * )sNode->node.arguments;
                              sList != NULL;
                              sList = ( qtcNode * )sList->node.next )
                        {
                            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                                     (void**)&sNewOrder )
                                      != IDE_SUCCESS );
                            sNewOrder->table = sList->node.table;
                            sNewOrder->column = sList->node.column;
                            sNewOrder->direction = QMG_DIRECTION_ASC;
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
                    else
                    {
                        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                                 (void**)&sNewOrder )
                                  != IDE_SUCCESS );
                        sNewOrder->table = sNode->node.table;
                        sNewOrder->column = sNode->node.column;
                        sNewOrder->direction = QMG_DIRECTION_ASC;
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
            }
            else
            {
                /* Nothing to do */
            }
        }

        IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                          sGraph,
                                          sWantOrder,
                                          & sSuccess )
                  != IDE_SUCCESS );

        /* Child Plan�� Preserved Order�� ���� �� �ִ��� �����Ѵ�. */
        if ( sSuccess == ID_TRUE )
        {
            sGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
            sGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CAN_USE;

            sSelAccessCost = sGraph->left->costInfo.outputRecordCnt;
            sSelDiskCost   = 0;
            sSelTotalCost  = sSelAccessCost + sSelDiskCost;
        }
        else
        {
            sGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
            sGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;

            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                             &(sGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                              &(sGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              aRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }
        }
        /* Rollup�� �׻� preservedOrder�� �����ȴ� */
        sGraph->preservedOrder = sWantOrder;
        sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
        sGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
    }
    else
    {
        if( sIsDisk == ID_FALSE )
        {
            sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                         &(sGraph->left->costInfo),
                                                         QMO_COST_DEFAULT_NODE_CNT );
            sSelAccessCost = sSelTotalCost;
            sSelDiskCost   = 0;
        }
        else
        {
            sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                          &(sGraph->left->costInfo),
                                                          QMO_COST_DEFAULT_NODE_CNT,
                                                          aRecordSize );
            sSelAccessCost = 0;
            sSelDiskCost   = sSelTotalCost;
        }

        sGraph->preservedOrder = NULL;

        /* BUG-43835 cube perserved order */
        sGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
        sGraph->flag |= QMG_PRESERVED_ORDER_NEVER;

        sCount = 0;
        for ( sGroupBy = aGroupGraph->groupBy;
              sGroupBy != NULL;
              sGroupBy = sGroupBy->next )
        {
            if ( sGroupBy->type == QMS_GROUPBY_CUBE )
            {
                for ( sSubGroup = sGroupBy->arguments;
                      sSubGroup != NULL;
                      sSubGroup = sSubGroup->next )
                {
                    sCount++;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }

        if (sCount > 0)
        {
            /* Cube�� 2^(n-1) ��ŭ Sorting �� �����Ѵ�. */
            sCount = 0x1 << ( sCount - 1 );
            sSelAccessCost = sSelAccessCost * sCount;
            sSelDiskCost   = sSelDiskCost   * sCount;
            sSelTotalCost  = sSelTotalCost  * sCount;
        }
        else
        {
            // Nothing to do.
        }
    }

    sGraph->costInfo.myAccessCost = sSelAccessCost;
    sGraph->costInfo.myDiskCost   = sSelDiskCost;
    sGraph->costInfo.myAllCost    = sSelTotalCost;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmgGrouping::checkParallelEnable( qmgGROP * aMyGraph )
{
/***********************************************************************
 *
 * Description : PROJ-2444 Parallel Aggreagtion
 *    Group ��尡 parallel �� �������� ���θ� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool  sMakeParallel = ID_TRUE;
    SDouble sCost;
    SDouble sGroupCount;

    // Group ������ ���Ѵ�.
    // output ���� Group ���� * selectivity �� ���ȹǷ� �̸� �����Ѵ�.
    sGroupCount = aMyGraph->graph.costInfo.outputRecordCnt /
        aMyGraph->graph.costInfo.selectivity;

    if ( ((aMyGraph->graph.flag & QMG_PARALLEL_IMPOSSIBLE_MASK)
          == QMG_PARALLEL_IMPOSSIBLE_FALSE) &&
         ((aMyGraph->graph.flag & QMG_PARALLEL_EXEC_MASK)
          == QMG_PARALLEL_EXEC_TRUE) )
    {
        if ( (aMyGraph->graph.left->myPlan->type == QMN_PSCRD) ||
             (aMyGraph->graph.left->myPlan->type == QMN_PPCRD) )
        {
            // MERGE �ܰ��� �۾����� ������ parallel �ؼ��� �ȵȴ�.
            sCost = ( aMyGraph->graph.costInfo.inputRecordCnt /
                      aMyGraph->graph.myPlan->mParallelDegree ) +
                ( aMyGraph->graph.myPlan->mParallelDegree *
                  sGroupCount );

            if ( QMO_COST_IS_GREATER( sCost, aMyGraph->graph.costInfo.inputRecordCnt ) == ID_TRUE )
            {
                sMakeParallel = ID_FALSE;
            }
            else
            {
                //nothing to do
            }

            // PPCRD �� ���Ͱ� ������ parallel �ؼ��� �ȵȴ�.
            if (aMyGraph->graph.left->myPlan->type == QMN_PPCRD)
            {
                if ( (((qmncPPCRD*)aMyGraph->graph.left->myPlan)->subqueryFilter != NULL) ||
                     (((qmncPPCRD*)aMyGraph->graph.left->myPlan)->nnfFilter != NULL) )
                {
                    sMakeParallel = ID_FALSE;
                }
                else
                {
                    //nothing to do
                }
            }
            else
            {
                //nothing to do
            }
        }
        else
        {
            sMakeParallel = ID_FALSE;
        }
    }
    else
    {
        sMakeParallel = ID_FALSE;
    }

    return sMakeParallel;
}

// BUG-48132 grouping method property
void qmgGrouping::getPropertyGroupingMethod( qcStatement        * aStatement,
                                             qmoGroupMethodType * aGroupingMethod )
{
    qmoGroupMethodType sGroupingMethod = QMO_GROUP_METHOD_TYPE_NOT_DEFINED;

    switch ( QCG_GET_PLAN_HASH_OR_SORT_METHOD( aStatement ) & QMG_HASH_OR_SORT_METHOD_GROUPING_MASK )
    {
        case QMG_HASH_OR_SORT_METHOD_GROUPING_HASH:
            sGroupingMethod = QMO_GROUP_METHOD_TYPE_HASH;
            break;
        case QMG_HASH_OR_SORT_METHOD_GROUPING_SORT:
            sGroupingMethod =QMO_GROUP_METHOD_TYPE_SORT;
            break;
        default:
            sGroupingMethod = QMO_GROUP_METHOD_TYPE_NOT_DEFINED;
            break;
    }

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_PLAN_HASH_OR_SORT_METHOD );

    *aGroupingMethod = sGroupingMethod;
}
