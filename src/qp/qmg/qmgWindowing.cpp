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
 * $Id: qmgWindowing.cpp 29304 2008-11-14 08:17:42Z jakim $
 *
 * Description :
 *     Windowing Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgWindowing.h>
#include <qmoCost.h>
#include <qmoOneMtrPlan.h>
#include <qmgSet.h>
#include <qmgGrouping.h>
#include <qcg.h>

extern mtfModule mtfRowNumberLimit;

IDE_RC
qmgWindowing::init( qcStatement * aStatement,
                    qmsQuerySet * aQuerySet,
                    qmgGraph    * aChildGraph,
                    qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgWindowing�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgWindowing�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgWINDOW   * sMyGraph;
    qmsQuerySet * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgWindowing::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Windowing Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgWindowing�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgWINDOW ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_WINDOWING;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myFrom = NULL;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgWindowing::optimize;
    sMyGraph->graph.makePlan = qmgWindowing::makePlan;
    sMyGraph->graph.printGraph = qmgWindowing::printGraph;

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
    // Windowing Graph ���� ������ ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->analyticFuncList = aQuerySet->analyticFuncList;
    sMyGraph->analyticFuncListPtrArr = NULL;
    sMyGraph->sortingKeyCount = 0;
    sMyGraph->sortingKeyPtrArr = NULL;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgWindowing::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgWindowing�� ����ȭ
 *
 * Implementation :
 *    (1) Analytic Function ���� ���ϰ�, sorting key �з��� ����
 *        �ڷ� ���� ����
 *    (2) Sorting Key �з�
 *    (3) Analytic Function �з�
 *    (4) Sorting ���� ���� ����
 *    (5) Preserved Order ����
 *    (6) ���� ��� ������ ����
 *
 ***********************************************************************/

    qmgWINDOW           * sMyGraph;
    qmsAnalyticFunc     * sCurAnalFunc;
    qmoDistAggArg       * sNewDistAggr;
    qmoDistAggArg       * sFirstDistAggr;
    qmoDistAggArg       * sCurDistAggr          = NULL;
    qtcNode             * sNode;
    mtcColumn           * sMtcColumn;
    UInt                  sAnalFuncCount;
    UInt                  sRowNumberFuncCount;
    qmgPreservedOrder  ** sSortingKeyPtrArr;
    qmsAnalyticFunc    ** sAnalyticFuncListPtrArr;
    qmsTarget           * sTarget;
    SDouble               sRecordSize;
    SDouble               sTempNodeCount;
    SDouble               sCost;
    qtcOverColumn       * sOverColumn;
    UInt                  sHashBucketCnt;
    UInt                  i;
    qmgPreservedOrder   * sWantOrder = NULL;
    idBool                sSuccess = ID_FALSE;
    idBool                sIsDisk;

    IDU_FIT_POINT_FATAL( "qmgWindowing::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph            = (qmgWINDOW*) aGraph;
    sAnalFuncCount      = 0;
    sRowNumberFuncCount = 0;
    sFirstDistAggr      = NULL;

    // analytic function ������ sorting key ������ �ʱ�ȭ�Ѵ�.
    // sorting key �з� �� analytic function �з� �ÿ� ��Ȯ�� ������
    // �����ȴ�.
    // distinct aggregation argument ����
    for ( sCurAnalFunc = sMyGraph->analyticFuncList;
          sCurAnalFunc != NULL;
          sCurAnalFunc = sCurAnalFunc->next )
    {
        sAnalFuncCount++;

        // aggregation�� subquery ����ȭ
        IDE_TEST(
            qmoPred::optimizeSubqueryInNode(aStatement,
                                            sCurAnalFunc->analyticFuncNode,
                                            ID_FALSE,
                                            ID_FALSE )
            != IDE_SUCCESS );

        for ( sOverColumn =
                  sCurAnalFunc->analyticFuncNode->overClause->overColumn;
              sOverColumn != NULL;
              sOverColumn = sOverColumn->next )
        {
            // partition by �� subquery ����ȭ
            IDE_TEST(
                qmoPred::optimizeSubqueryInNode(aStatement,
                                                sOverColumn->node,
                                                ID_FALSE,
                                                ID_FALSE )
                != IDE_SUCCESS );
        }

        // distinct aggreagtion argument ����
        if( ( sCurAnalFunc->analyticFuncNode->node.lflag &
              MTC_NODE_DISTINCT_MASK )
            == MTC_NODE_DISTINCT_TRUE )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoDistAggArg ),
                                                     (void**) & sNewDistAggr )
                      != IDE_SUCCESS );

            sNode = (qtcNode*)sCurAnalFunc->analyticFuncNode->node.arguments;

            sNewDistAggr->aggArg = sNode;
            sNewDistAggr->next   = NULL;

            if( sCurAnalFunc->analyticFuncNode->overClause->partitionByColumn != NULL )
            {
                // distinct �� bucket ������ ���ϱ� ���ؼ�
                // partition by �� bucket ������ ���Ѵ�.
                IDE_TEST(
                    getBucketCnt4Windowing(
                        aStatement,
                        sMyGraph,
                        sCurAnalFunc->analyticFuncNode->overClause->partitionByColumn,
                        sMyGraph->graph.myQuerySet->SFWGH->hints->groupBucketCnt,
                        &sHashBucketCnt )
                    != IDE_SUCCESS );
            }
            else
            {
                sHashBucketCnt = 1;
            }

            IDE_TEST(
                qmg::getBucketCnt4DistAggr(
                    aStatement,
                    sMyGraph->graph.left->costInfo.outputRecordCnt,
                    sHashBucketCnt,
                    sNode,
                    sMyGraph->graph.myQuerySet->SFWGH->hints->groupBucketCnt,
                    & sNewDistAggr->bucketCnt )
                != IDE_SUCCESS );

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
            /* BUG-40354 pushed rank */
            if ( ( sCurAnalFunc->analyticFuncNode->overClause->partitionByColumn == NULL ) &&
                 ( sCurAnalFunc->analyticFuncNode->node.module == &mtfRowNumberLimit ) )
            {
                sRowNumberFuncCount++;
            }
            else
            {
                /* Nothing to do. */
            }
        }
    }
    sMyGraph->distAggArg      = sFirstDistAggr;
    sMyGraph->sortingKeyCount = sAnalFuncCount;

    /* BUG-40354 pushed rank */
    if ( sAnalFuncCount == sRowNumberFuncCount )
    {
        sMyGraph->graph.flag &= ~QMG_WINDOWING_PUSHED_RANK_MASK;
        sMyGraph->graph.flag |= QMG_WINDOWING_PUSHED_RANK_TRUE;
    }
    else
    {
        /* Nothing to do. */
    }
    
    //------------------------------------------
    // Sorting Key Pointer Array��
    // Analytic Function List�� ����Ű�� Pointer Array�� ���� �� �ʱ�ȭ
    //-----------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc((ID_SIZEOF(qmgPreservedOrder*) *
                                             sAnalFuncCount),
                                            (void**) & sSortingKeyPtrArr )
              != IDE_SUCCESS );

    for ( i = 0; i < sAnalFuncCount; i++ )
    {
        sSortingKeyPtrArr[i] = NULL;
    }
    
    sMyGraph->sortingKeyPtrArr = sSortingKeyPtrArr;

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc((ID_SIZEOF(qmsAnalyticFunc*) *
                                             sAnalFuncCount),
                                            (void**) & sAnalyticFuncListPtrArr )
              != IDE_SUCCESS );

    for ( i = 0; i < sAnalFuncCount; i++ )
    {
        sAnalyticFuncListPtrArr[i] = NULL;
    }

    sMyGraph->analyticFuncListPtrArr = sAnalyticFuncListPtrArr;

    //------------------------------------------
    // Sorting Key �� Analytic Function  �з�
    //-----------------------------------------

    IDE_TEST( classifySortingKeyNAnalFunc( aStatement, sMyGraph )
              != IDE_SUCCESS );
                                           

    // ���� sorting key count ����
    for ( i = 0 ; i < sMyGraph->sortingKeyCount; i ++ )
    {
        if ( sSortingKeyPtrArr[i] == NULL )
        {
            break;
        }
        else
        {
            // nothing to do
        }
    }
    sMyGraph->sortingKeyCount = i;
    
    //------------------------------------------
    // ù��° Sorting ���� ���� ���� ��
    // Preserved Order ����
    //-----------------------------------------
    
    if ( sMyGraph->graph.left->preservedOrder != NULL )
    {
        // child�� preserved order�� ������ sorting key�� ������
        // �̸� ù��°�� �Ű� preserved order�� �̿��ϵ��� �Ѵ�.
        IDE_TEST( alterSortingOrder( aStatement,
                                     aGraph,
                                     aGraph->left->preservedOrder,
                                     ID_TRUE )// ù��° sorting key ���� ����
                  != IDE_SUCCESS );
    }
    else
    {
        sMyGraph->graph.flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
        sMyGraph->graph.flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;

        if ( sMyGraph->sortingKeyCount > 0 )
        {
            /* BUG-40361 supporting to indexable analytic function */
            sWantOrder = sMyGraph->sortingKeyPtrArr[0];
            IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                              &sMyGraph->graph,
                                              sWantOrder,
                                              & sSuccess )
                      != IDE_SUCCESS );

            /* BUG-40361 supporting to indexable analytic function */
            if ( sSuccess == ID_TRUE )
            {
                sMyGraph->graph.flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
                sMyGraph->graph.flag |= QMG_CHILD_PRESERVED_ORDER_CAN_USE;
            }
            else
            {
                /* Nothing to do */
            }

            // BUG-21812
            // child plan�� preserved order�� ���� ���,
            // ���� sorting order�� ������ sorting key��
            // preserved order�� ��
            sMyGraph->graph.preservedOrder =
                sMyGraph->sortingKeyPtrArr[sMyGraph->sortingKeyCount - 1];
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED;
        }
        else
        {
            // child plan�� preserved order ����,
            // window graph�� sorting key�� ���� ��쿡��
            // preserved order�� not defined ��
            sMyGraph->graph.preservedOrder = NULL;

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NOT_DEFINED;
        }
    }

    /* PROJ-1805 */
    IDE_TEST_RAISE( ( sMyGraph->graph.left->flag & QMG_GROUPBY_NONE_MASK )
                    == QMG_GROUPBY_NONE_TRUE, ERR_NO_GROUP_EXPRESSION );
    /* PROJ-1353 */
    IDE_TEST_RAISE( ( sMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
                    == QMG_GROUPBY_EXTENSION_TRUE, ERR_NOT_WINDOWING );

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // recordSize ���
    sRecordSize = 0;
    for ( sTarget = aGraph->myQuerySet->SFWGH->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sNode = sTarget->targetColumn;

        sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
        sRecordSize += sMtcColumn->column.size;
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = 1;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // outputRecordCnt
    sMyGraph->graph.costInfo.outputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // myCost
    IDE_TEST( qmg::isDiskTempTable( aGraph, & sIsDisk ) != IDE_SUCCESS );

    // BUG-36463 sortingKeyCount �� 0�� ���� sort �� ���ϰ� ���常 ��
    sTempNodeCount = IDL_MAX( sMyGraph->sortingKeyCount, 1 );

    if( sIsDisk == ID_FALSE )
    {
        sCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                             &(aGraph->left->costInfo),
                                             sTempNodeCount );

        // analytic function ���Ҷ� �ι� �˻���?
        sMyGraph->graph.costInfo.myAccessCost = sCost;
        sMyGraph->graph.costInfo.myDiskCost   = 0;
    }
    else
    {
        sCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                              &(aGraph->left->costInfo),
                                              sTempNodeCount,
                                              sRecordSize );

        // analytic function ���Ҷ� �ι� �˻���?
        sMyGraph->graph.costInfo.myAccessCost = 0;
        sMyGraph->graph.costInfo.myDiskCost   = sCost;
    }

    // My Access Cost�� My Disk Cost�� �̹� ����.
    sMyGraph->graph.costInfo.myAllCost =
        sMyGraph->graph.costInfo.myAccessCost +
        sMyGraph->graph.costInfo.myDiskCost;

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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_WINDOWING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_INVALID_WINDOW_FUNCTION,
                                  "ROLLUP, CUBE, GROUPING SETS"));
    }
    IDE_EXCEPTION( ERR_NO_GROUP_EXPRESSION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NO_GROUP_EXPRESSION ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgWindowing::classifySortingKeyNAnalFunc( qcStatement     * aStatement,
                                           qmgWINDOW       * aMyGraph )
{
    /***********************************************************************
     *
     * Description : sorting key�� analytic function �з�
     *
     * Implementation :
     *    (1) �̹� ��ϵ� sorting key���� �湮�Ͽ�
     *        ���� Partition By Column �̿� �������� �˻�
     *    (2) ��� ������ ���,
     *        (i)   sorting key Ȯ�� ���� ����
     *        (ii) Ȯ���ؾ� �ϴ� ���, sorting key Ȯ��
     *        ��� �Ұ����� ���, sorting key ����
     *    (3) analytic function�� �����Ǵ� sorting key ��ġ�� ���
     *
     ***********************************************************************/

    qmsAnalyticFunc     * sCurAnalFunc;
    qmsAnalyticFunc     * sNewAnalFunc;
    qtcOverColumn       * sOverColumn;
    qtcOverColumn       * sOverColumn2 = NULL;
    qtcOverColumn       * sOverColumn3 = NULL;
    qmsAnalyticFunc     * sInnerAnalyticFunc;
    idBool                sIsSame;
    idBool                sFoundSameSortingKey;
    idBool                sReplaceSortingKey;
    UInt                  sSortingKeyPosition;
    qmgPreservedOrder  ** sSortingKeyPtrArr;
    qmsAnalyticFunc    ** sAnalyticFuncPtrArr;
    qtcOverColumn       * sExpandOverColumn;
    qtcOverColumn       * sCurOverColumn;
    qmgPreservedOrder   * sNewSortingKeyCol;
    qmgPreservedOrder   * sLastSortingKeyCol;
    mtcNode             * sNode;

    IDU_FIT_POINT_FATAL( "qmgWindowing::classifySortingKeyNAnalFunc::__FT__" );

    //--------------
    // �⺻ �ʱ�ȭ
    //--------------
    sSortingKeyPosition  = 0;
    sSortingKeyPtrArr    = aMyGraph->sortingKeyPtrArr;
    sAnalyticFuncPtrArr  = aMyGraph->analyticFuncListPtrArr;

    /* BUG-46264 window sort�� order by ���� ���� �÷��� ���ð�� �ߺ� ���� */
    for ( sCurAnalFunc = aMyGraph->analyticFuncList;
          sCurAnalFunc != NULL;
          sCurAnalFunc = sCurAnalFunc->next )
    {
        for ( sOverColumn = sCurAnalFunc->analyticFuncNode->overClause->orderByColumn;
              sOverColumn != NULL;
              sOverColumn = sOverColumn->next )
        {
            sIsSame = ID_FALSE;

            for ( sOverColumn2 = sCurAnalFunc->analyticFuncNode->overClause->orderByColumn;
                  sOverColumn2 != NULL;
                  sOverColumn2 = sOverColumn2->next )
            {
                if ( sOverColumn == sOverColumn2 )
                {
                    break;
                }
                else
                {
                    if ( ( sOverColumn->node->node.table == sOverColumn2->node->node.table ) &&
                         ( sOverColumn->node->node.column == sOverColumn2->node->node.column ) )
                    {
                        sIsSame = ID_TRUE;
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
            }

            if ( sIsSame == ID_TRUE )
            {
                sOverColumn3->next = sOverColumn->next;
            }
            else
            {
                sOverColumn3 = sOverColumn;
            }
        }
    }

    sCurAnalFunc = aMyGraph->analyticFuncList;
    while ( sCurAnalFunc != NULL )
    {
        for( sInnerAnalyticFunc = aMyGraph->analyticFuncList;
             sInnerAnalyticFunc != sCurAnalFunc;
             sInnerAnalyticFunc = sInnerAnalyticFunc->next )
        {
            /* PROJ-1805 window Caluse */
            if ( sInnerAnalyticFunc->analyticFuncNode->overClause->window == NULL )
            {
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sCurAnalFunc->analyticFuncNode,
                                                       sInnerAnalyticFunc->analyticFuncNode,
                                                       &sIsSame )
                          != IDE_SUCCESS );
                if( sIsSame == ID_TRUE )
                {
                    goto loop_end;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }

        sOverColumn = sCurAnalFunc->analyticFuncNode->overClause->overColumn;

        if ( sOverColumn == NULL )
        {
            // Partition By ���� �������� �����Ƿ�
            // ������ sorting�� �ʿ����
            sFoundSameSortingKey = ID_TRUE;
            sReplaceSortingKey   = ID_FALSE;
            sSortingKeyPosition  = 0;
        }
        else
        {
            sFoundSameSortingKey = ID_FALSE;
            sReplaceSortingKey   = ID_FALSE;

            // partition by�� ������ sorting key�� ��ϵǾ��ִ��� �˻�
            IDE_TEST( existSameSortingKeyAndDirection(
                          aMyGraph->sortingKeyCount,
                          sSortingKeyPtrArr,
                          sOverColumn,
                          & sFoundSameSortingKey,
                          & sReplaceSortingKey,
                          & sSortingKeyPosition )
                      != IDE_SUCCESS );
        }

        if ( sFoundSameSortingKey == ID_TRUE )
        {
            if ( sReplaceSortingKey == ID_TRUE )
            {
                //----------------------------------------
                // ���� ��ϵ� sorting key�� Ȯ��
                //----------------------------------------

                // (1) ������ sorting ã��
                // (2) �߰��� sorting key col ������ ������ ù��°
                //     partition column ã��
                for ( sLastSortingKeyCol =
                          sSortingKeyPtrArr[sSortingKeyPosition],
                          sCurOverColumn = sOverColumn;
                      sLastSortingKeyCol->next != NULL;
                      sLastSortingKeyCol = sLastSortingKeyCol->next,
                          sCurOverColumn = sCurOverColumn->next ) ;
                
                sExpandOverColumn = sCurOverColumn->next;
                
                // (3) �����ִ� partition key column����
                //     sorting key column�� �ڿ� �߰��Ѵ�.
                while ( sExpandOverColumn != NULL )
                {
                    IDE_TEST(
                        QC_QMP_MEM(aStatement)->alloc(
                            ID_SIZEOF(qmgPreservedOrder),
                            (void**) &sNewSortingKeyCol )
                        != IDE_SUCCESS );

                    // BUG-34966 Pass node �� �� �����Ƿ� ���� ���� ��ġ�� �����Ѵ�.
                    sNode = &sExpandOverColumn->node->node;

                    while( sNode->module == &qtc::passModule )
                    {
                        sNode = sNode->arguments;
                    }

                    sNewSortingKeyCol->table  = sNode->table;
                    sNewSortingKeyCol->column = sNode->column;

                    // To Fix BUG-21966
                    if ( (sExpandOverColumn->flag & QTC_OVER_COLUMN_MASK)
                         == QTC_OVER_COLUMN_NORMAL )
                    {
                        // others column
                        sNewSortingKeyCol->direction = QMG_DIRECTION_NOT_DEFINED;
                    }
                    else
                    {   
                        IDE_DASSERT( (sExpandOverColumn->flag & QTC_OVER_COLUMN_MASK)
                                     == QTC_OVER_COLUMN_ORDER_BY );
                            
                        // BUG-33663 Ranking Function
                        // order by column
                        if ( (sExpandOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                             == QTC_OVER_COLUMN_ORDER_ASC )
                        {
                            // BUG-40361 supporting to indexable analytic function
                            if ( ( sExpandOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                 == QTC_OVER_COLUMN_NULLS_ORDER_NONE )
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_ASC;
                            }
                            else if ( ( sExpandOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                      == QTC_OVER_COLUMN_NULLS_ORDER_FIRST )
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_ASC_NULLS_FIRST;
                            }
                            else
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_ASC_NULLS_LAST;
                            }
                        }
                        else
                        {
                            // BUG-40361 supporting to indexable analytic function
                            if ( ( sExpandOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                 == QTC_OVER_COLUMN_NULLS_ORDER_NONE )
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_DESC;
                            }
                            else if ( ( sExpandOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                      == QTC_OVER_COLUMN_NULLS_ORDER_FIRST )
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_DESC_NULLS_FIRST;
                            }
                            else
                            {
                                sNewSortingKeyCol->direction = QMG_DIRECTION_DESC_NULLS_LAST;
                            }
                        }
                    }
                    
                    sNewSortingKeyCol->next = NULL;
                    
                    sLastSortingKeyCol->next = sNewSortingKeyCol;
                    sLastSortingKeyCol       = sNewSortingKeyCol;
                    
                    sExpandOverColumn = sExpandOverColumn->next;
                }
            }
            else
            {
                // �̹� sorting key�� ��ϵǾ�����,
                // sorting key�� Ȯ���� �ʿ䵵 ���� ���
                // nothing to do 
            }
        }
        else
        {
            //----------------------------------------
            // ���ο� sorting key�� ���
            //----------------------------------------
            sLastSortingKeyCol = NULL;
            
            for ( sCurOverColumn = sOverColumn;
                  sCurOverColumn != NULL;
                  sCurOverColumn = sCurOverColumn->next )
            {
                IDE_TEST(
                    QC_QMP_MEM(aStatement)->alloc(
                        ID_SIZEOF(qmgPreservedOrder),
                        (void**) & sNewSortingKeyCol )
                    != IDE_SUCCESS );

                // BUG-34966 Pass node �� �� �����Ƿ� ���� ���� ��ġ�� �����Ѵ�.
                sNode = &sCurOverColumn->node->node;

                while( sNode->module == &qtc::passModule )
                {
                    sNode = sNode->arguments;
                }

                sNewSortingKeyCol->table  = sNode->table;
                sNewSortingKeyCol->column = sNode->column;

                // To Fix BUG-21966
                if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                     == QTC_OVER_COLUMN_NORMAL )
                {
                    sNewSortingKeyCol->direction = QMG_DIRECTION_NOT_DEFINED;
                }
                else
                {                    
                    IDE_DASSERT( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                                 == QTC_OVER_COLUMN_ORDER_BY );
                        
                    // BUG-33663 Ranking Function
                    // order by column
                    if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                         == QTC_OVER_COLUMN_ORDER_ASC )
                    {
                        // BUG-40361 supporting to indexable analytic function
                        if ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                             == QTC_OVER_COLUMN_NULLS_ORDER_NONE )
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_ASC;
                        }
                        else if ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                  == QTC_OVER_COLUMN_NULLS_ORDER_FIRST )
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_ASC_NULLS_FIRST;
                        }
                        else
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_ASC_NULLS_LAST;
                        }
                    }
                    else
                    {
                        // BUG-40361 supporting to indexable analytic function
                        if ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                             == QTC_OVER_COLUMN_NULLS_ORDER_NONE )
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_DESC;
                        }
                        else if ( ( sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                  == QTC_OVER_COLUMN_NULLS_ORDER_FIRST )
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_DESC_NULLS_FIRST;
                        }
                        else
                        {
                            sNewSortingKeyCol->direction = QMG_DIRECTION_DESC_NULLS_LAST;
                        }
                    }
                }
                
                sNewSortingKeyCol->next = NULL;
                
                if ( sLastSortingKeyCol == NULL )
                {
                    // ù��° column�� ���
                    sSortingKeyPtrArr[sSortingKeyPosition] = sNewSortingKeyCol;
                    sLastSortingKeyCol = sNewSortingKeyCol;
                }
                else
                {
                    sLastSortingKeyCol->next = sNewSortingKeyCol;
                    sLastSortingKeyCol       = sNewSortingKeyCol;
                }
            }
        }

        //----------------------------------------
        // ���� analytic function��
        // Sorting key�� �����Ǵ� ��ġ��
        // analytic function list�� �����Ѵ�.
        //----------------------------------------

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmsAnalyticFunc),
                                                (void**) & sNewAnalFunc )
                  != IDE_SUCCESS );

        idlOS::memcpy( sNewAnalFunc,
                       sCurAnalFunc,
                       ID_SIZEOF(qmsAnalyticFunc) );
        sNewAnalFunc->next = NULL;
        
        if( sAnalyticFuncPtrArr[sSortingKeyPosition] != NULL )
        {
            sNewAnalFunc->next =
                sAnalyticFuncPtrArr[sSortingKeyPosition];
            sAnalyticFuncPtrArr[sSortingKeyPosition] = sNewAnalFunc;
        }
        else
        {
            // analytic function ù ����� ���
            sAnalyticFuncPtrArr[sSortingKeyPosition] = sNewAnalFunc;
        }

loop_end:
        sCurAnalFunc = sCurAnalFunc->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgWindowing::existSameSortingKeyAndDirection( UInt                 aSortingKeyCount,
                                               qmgPreservedOrder ** aSortingKeyPtrArr,
                                               qtcOverColumn      * aOverColumn,
                                               idBool             * aFoundSameSortingKey,
                                               idBool             * aReplaceSortingKey,
                                               UInt               * aSortingKeyPosition )
{
/***********************************************************************
 *
 * Description : ������ sorting key, direction ���� ���� ��ȯ
 *
 * Implementation :
 *    (1) �̹� ��ϵ� sorting key���� �湮�Ͽ�
 *        ���� Partition By Column �̿� �������� �˻�
 *    (2) ��� ������ ���,
 *        (i)   sorting key Ȯ�� ���� ����
 *        (ii) Ȯ���ؾ� �ϴ� ���, sorting key Ȯ��
 *        ��� �Ұ����� ���, sorting key ����
 *    (3) analytic function�� �����Ǵ� sorting key ��ġ�� ���
 *
 ***********************************************************************/

    qmgPreservedOrder   * sCurSortingKeyCol;
    qtcOverColumn       * sCurOverColumn;
    UInt                  i;

    IDU_FIT_POINT_FATAL( "qmgWindowing::existSameSortingKeyAndDirection::__FT__" );

    //----------------------------------------------------
    // �̹� ��ϵǾ� �ִ� sorting key�� ���󰡸�
    // ���� partition by column list�� ������ sorting key��
    // �����ϴ��� �˻�
    //----------------------------------------------------
    
    for ( i = 0; i < aSortingKeyCount; i ++ )
    {
        // ���� Sorting Key ����
        if ( aSortingKeyPtrArr[i] == NULL )
        {
            // ���̻� ��ϵ� sorting key�� ����
            *aSortingKeyPosition = i;
            break;
        }
        else
        {
            // nothing to do 
        }
        
        //----------------------------------------
        // ��ϵ� sorting key column���
        // partition by, order by column���� �������� �˻�
        //----------------------------------------
        *aFoundSameSortingKey = ID_TRUE;

        for ( sCurOverColumn = aOverColumn, sCurSortingKeyCol = aSortingKeyPtrArr[i];
              ( sCurOverColumn != NULL ) && ( sCurSortingKeyCol != NULL );
              sCurOverColumn = sCurOverColumn->next, sCurSortingKeyCol = sCurSortingKeyCol->next )
        {
            if ((sCurOverColumn->node->node.table == sCurSortingKeyCol->table) &&
                (sCurOverColumn->node->node.column == sCurSortingKeyCol->column))
            {
                // BUG-33663 Ranking Function
                // order by column�� ��� order���� ���ƾ� �Ѵ�.
                switch ( sCurSortingKeyCol->direction )
                {
                    case QMG_DIRECTION_NOT_DEFINED:
                    {
                        if ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                             == QTC_OVER_COLUMN_NORMAL )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_ASC:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_ASC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_DESC:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_DESC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_ASC_NULLS_FIRST:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_ASC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_ASC_NULLS_LAST:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_ASC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_DESC_NULLS_FIRST:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_DESC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    case QMG_DIRECTION_DESC_NULLS_LAST:
                    {
                        if ( ( (sCurOverColumn->flag & QTC_OVER_COLUMN_MASK)
                               == QTC_OVER_COLUMN_ORDER_BY )
                             &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                               == QTC_OVER_COLUMN_ORDER_DESC ) &&
                             ( (sCurOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK)
                               == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) )
                        {
                            // ���� Į���� ����
                        }
                        else
                        {
                            *aFoundSameSortingKey = ID_FALSE;
                        }
                        break;
                    }
                    default:
                    {
                        IDE_DASSERT(0);
                        *aFoundSameSortingKey = ID_FALSE;
                        break;
                    }
                }

                if ( *aFoundSameSortingKey == ID_FALSE )
                {
                    break;
                }
                else
                {
                    // ���� Į�� �˻� ����
                }
            }
            else
            {
                *aFoundSameSortingKey = ID_FALSE;
                break;
            }
        }

        if ( *aFoundSameSortingKey == ID_TRUE )
        {
            // ���� sorting key ã�� ���
            if ( ( sCurOverColumn != NULL ) &&
                 ( sCurSortingKeyCol == NULL ) )
            {
                // Partition Key�� Sorting Key���� Į���� �� ���� ���,
                // ��ϵ� Sorting Key�� Ȯ���Ѵ�.
                // ex) SELECT sum(i1) over ( partition by i1 ),
                //            sum(i2) over ( partition by i1, i2 )
                //     FROM t1;
                // (i1)�� sorting key�� ��ϵ� ���,
                // (i1, i2)�� sorting key�� Ȯ���ؾ� �Ѵ�.
                *aReplaceSortingKey = ID_TRUE;
            }
            else
            {
                // ���� sorting key�� �״�� �̿� ������
                *aReplaceSortingKey = ID_FALSE;
            }
            
            *aSortingKeyPosition = i;
            break;
        }
        else
        {
            // nothing to do 
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmgWindowing::existWantOrderInSortingKey( qmgGraph          * aGraph,
                                          qmgPreservedOrder * aWantOrder,
                                          idBool            * aFind,
                                          UInt              * aFindSortingKeyPosition )
{
/***********************************************************************
 *
 * Description : ���ϴ� order�� sorting key�� �߿� �����ϴ��� �˻�
 *
 * Implementation :
 *    (1) ���ϴ� preserved order�� ������ sorting key�� �����ϴ���
 *        �˻�
 *    (2) �����ϴ� ���, ù��° �Ǵ� ������ sorting ������ ����
 *        - ù��° sorting key�� ����
 *        - ������ sorting key�� ����
 *    (3) ���� preserved order�� ������ sorting key�� ����
 ***********************************************************************/

    qmgPreservedOrder  ** sSortingKeyPtrArr;
    UInt                  sSortingKeyCount;
    qmgPreservedOrder   * sCurSortingKeyCol;
    qmgPreservedOrder   * sCurOrderCol;
    idBool                sFind;
    qmgDirectionType      sPrevDirection;
    idBool                sSameDirection;
    UInt                  i;

    IDU_FIT_POINT_FATAL( "qmgWindowing::existWantOrderInSortingKey::__FT__" );

    //--------------
    // �⺻ �ʱ�ȭ
    //--------------

    sFind                   = ID_FALSE;
    sSortingKeyPtrArr       = ((qmgWINDOW*)aGraph)->sortingKeyPtrArr;
    sSortingKeyCount        = ((qmgWINDOW*)aGraph)->sortingKeyCount;
    sSameDirection          = ID_FALSE;
    
    for ( i = 0 ; i < sSortingKeyCount; i ++ )
    {
        //------------------------------
        // ������ sorting key�� ã��
        //------------------------------

        sPrevDirection = QMG_DIRECTION_NOT_DEFINED;
        
        for ( sCurSortingKeyCol = sSortingKeyPtrArr[i],
                  sCurOrderCol = aWantOrder ;
              sCurSortingKeyCol != NULL &&
                  sCurOrderCol != NULL;
              sCurSortingKeyCol = sCurSortingKeyCol->next,
                  sCurOrderCol = sCurOrderCol->next )
        {
            if ( ( sCurSortingKeyCol->table == sCurOrderCol->table ) &&
                 ( sCurSortingKeyCol->column == sCurOrderCol->column ) )
            {
                /* BUG-42145
                 * Table�̳� Partition�� Index�� �ִ� �����
                 * Nulls Option �� ����� Direction Check�� �Ѵ�.
                 */
                if ( ( ( aGraph->left->type == QMG_SELECTION ) ||
                       ( aGraph->left->type == QMG_PARTITION ) ) &&
                     ( aGraph->left->myFrom->tableRef->view == NULL ) )
                {
                    IDE_TEST( qmg::checkSameDirection4Index( sCurOrderCol,
                                                             sCurSortingKeyCol,
                                                             sPrevDirection,
                                                             & sPrevDirection,
                                                             & sSameDirection )
                              != IDE_SUCCESS );
                }
                else
                {
                    // BUG-28507
                    // sorting key�� ������ ���,
                    // soring�� ���⼺(ASC, DESC) �˻�
                    IDE_TEST( qmg::checkSameDirection( sCurOrderCol,
                                                       sCurSortingKeyCol,
                                                       sPrevDirection,
                                                       & sPrevDirection,
                                                       & sSameDirection )
                              != IDE_SUCCESS );
                }

                if ( sSameDirection == ID_TRUE )
                {
                    // ���� Į�� ��
                    sFind = ID_TRUE;
                }
                else
                {
                    // ���⼺ �޶� ���� order�� �� �� ����
                    sFind = ID_FALSE;
                    break;
                }
            }
            else
            {
                sFind = ID_FALSE;
                break;
            }

            sPrevDirection = sCurSortingKeyCol->direction;
        }
        
        if ( sFind == ID_TRUE )
        {
            if ( ( sCurOrderCol != NULL ) || ( sCurSortingKeyCol != NULL ) )
            {
                // Order Column�� ���� ���,
                // ���� order�� ������ sorting key ã�°��� ������ ����
                sFind = ID_FALSE;
            }
            else
            {
                // order�� ������ sorting key ã��
                *aFindSortingKeyPosition = i;
                break;
            }
        }
        else
        {
            // nothing to do 
        }
    }

    *aFind = sFind;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgWindowing::alterSortingOrder( qcStatement       * aStatement,
                                 qmgGraph          * aGraph,
                                 qmgPreservedOrder * aWantOrder,
                                 idBool              aAlterFirstOrder )
{
/***********************************************************************
 *
 * Description : first sorting key ����
 *
 * Implementation :
 *    (1) ���ϴ� preserved order�� ������ sorting key�� �����ϴ���
 *        �˻�
 *    (2) �����ϴ� ���, ù��° �Ǵ� ������ sorting ������ ����
 *        - ù��° sorting key�� ����
 *        - ������ sorting key�� ����
 *    (3) ���� preserved order�� ������ sorting key�� ����
 ***********************************************************************/

    qmgWINDOW           * sMyGraph;
    qmgPreservedOrder  ** sSortingKeyPtrArr;
    qmsAnalyticFunc    ** sAnalFuncListPtrArr;
    idBool                sFind;
    UInt                  sFindSortingKeyPosition;
    qmgPreservedOrder   * sFindSortingKeyPtr        = NULL;
    qmgPreservedOrder   * sCurOrder;
    qmgPreservedOrder   * sCurSortingKey            = NULL;
    qmsAnalyticFunc     * sFindAnalFuncListPtr;
    idBool                sSuccess = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgWindowing::alterSortingOrder::__FT__" );

    //--------------
    // �⺻ �ʱ�ȭ
    //--------------

    sMyGraph                = (qmgWINDOW*)aGraph;
    sFind                   = ID_FALSE;
    sSortingKeyPtrArr       = sMyGraph->sortingKeyPtrArr;
    sAnalFuncListPtrArr     = sMyGraph->analyticFuncListPtrArr;
    sFindSortingKeyPosition = 0;
    
    //------------------------------
    // want order�� ������ sorting ã��
    //------------------------------
    
    IDE_TEST( existWantOrderInSortingKey( aGraph,
                                          aWantOrder,
                                          & sFind, 
                                          & sFindSortingKeyPosition )
              != IDE_SUCCESS );

    if ( aAlterFirstOrder == ID_TRUE )
    {
        //------------------------------
        // ã�� sorting key�� ù��°�� �ű�
        // ���� graph���� �ö���� preserved order ��� ������ ���,
        // �̸� ù��° sorting ��ġ�� �ξ� ������ sorting��
        // ���� �ʵ��� ��
        //------------------------------
        if ( sFind == ID_TRUE )
        {
            sFindSortingKeyPtr = sSortingKeyPtrArr[sFindSortingKeyPosition];

            sSortingKeyPtrArr[sFindSortingKeyPosition] = sSortingKeyPtrArr[0];
            sSortingKeyPtrArr[0] = sFindSortingKeyPtr;

            sFindAnalFuncListPtr =
                sAnalFuncListPtrArr[sFindSortingKeyPosition];

            sAnalFuncListPtrArr[sFindSortingKeyPosition] =
                sAnalFuncListPtrArr[0];
            sAnalFuncListPtrArr[0] = sFindAnalFuncListPtr;

            /* BUG-43690 ���� preserved order�� direction�� �����Ѵ�. */
            if ( ( aGraph->left->preservedOrder->direction == QMG_DIRECTION_NOT_DEFINED ) &&
                 ( aGraph->left->preservedOrder->table == sFindSortingKeyPtr->table ) &&
                 ( aGraph->left->preservedOrder->column == sFindSortingKeyPtr->column ) )
            {
                aGraph->left->preservedOrder->direction = sFindSortingKeyPtr->direction;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            // ��� ������ preserved order�� ���� ���,
            // child preserved order ����� �� ����
            aGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
            aGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;
        }

        if ( sMyGraph->sortingKeyCount > 0 )
        {
            //------------------------------
            // ������ sorting key�� Preserved Order ����
            // ( ���� Graph���� ���ϴ� preserved order�� ���� �����ϹǷ�
            //    Defined Not Fixed �̸�, ���� order ���� ���ο� �������
            //    ������ sorting key�� ���� preserved order�� ���� )
            //------------------------------
            sMyGraph->graph.preservedOrder =
                sSortingKeyPtrArr[sMyGraph->sortingKeyCount - 1];
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED;
        }
        else
        {
            // sorting key�� ���� ���
            // ������ preserved order�� ���� order�� ������
            sMyGraph->graph.preservedOrder = aWantOrder;

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= ( aGraph->left->flag &
                                      QMG_PRESERVED_ORDER_MASK );
        }
    }
    else
    {
        //------------------------------
        // ã�� sorting key�� ���������� �ű�
        // qmgWindowing ���� Graph (��, qmgSorting)����
        // �ʿ��� want order�� ã�� ���,
        // �������� sorting �ϵ��� �Ͽ� ���� graph����
        // ������ sorting�� ���� �ʵ��� ��
        //------------------------------
        
        if ( sFind == ID_TRUE )
        {
            if ( ( sFindSortingKeyPosition == 0 ) &&
                 ( sMyGraph->sortingKeyCount > 1 ) )
            {
                // BUG-28507
                // ã�� sorting order�� ù��° �� ���,
                // ���� preserved order�� ����� �� ���ٰ� �����ؾ���
                aGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
                aGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;
            }
            else
            {
                // BUG-40361
                // Sort Key�� 1���̰� ���� Order ������ ���� �ʴٸ�
                // �������� �������� ������ Sort Key�� �����ֵ��� �Ѵ�.
                if ( sMyGraph->sortingKeyCount == 1 )
                {
                    IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                                      &sMyGraph->graph,
                                                      aWantOrder,
                                                      & sSuccess )
                              != IDE_SUCCESS );
                    if ( sSuccess == ID_FALSE )
                    {
                        aGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
                        aGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;
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

            sFindSortingKeyPtr = sSortingKeyPtrArr[sFindSortingKeyPosition];

            sSortingKeyPtrArr[sFindSortingKeyPosition] =
                sSortingKeyPtrArr[sMyGraph->sortingKeyCount - 1];
            sSortingKeyPtrArr[sMyGraph->sortingKeyCount - 1]
                = sFindSortingKeyPtr;

            sFindAnalFuncListPtr = sAnalFuncListPtrArr[sFindSortingKeyPosition];

            sAnalFuncListPtrArr[sFindSortingKeyPosition] =
                sAnalFuncListPtrArr[sMyGraph->sortingKeyCount - 1];
            sAnalFuncListPtrArr[sMyGraph->sortingKeyCount - 1] =
                sFindAnalFuncListPtr;

            //------------------------------
            // ������ sorting key�� Preserved Order ����
            // ( �������� �ʿ��� prserved order�� ������ ����̹Ƿ�
            //    Defined Fixed �̸�, ������ sorting key�� ����Ǿ���
            //    ������ ���� preserved order ���� )
            //------------------------------
            sMyGraph->graph.preservedOrder =
                sSortingKeyPtrArr[sMyGraph->sortingKeyCount - 1];
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
        }
        else
        {
            // ��� ������ preserved order�� ���� ���,
            // child preserved order ����� �� ����
            aGraph->flag &= ~QMG_CHILD_PRESERVED_ORDER_USE_MASK;
            aGraph->flag |= QMG_CHILD_PRESERVED_ORDER_CANNOT_USE;
        }
    }

    if ( sFind == ID_TRUE )
    {
        // To Fix BUG-21966
        for ( sCurSortingKey = sFindSortingKeyPtr,
                  sCurOrder = aWantOrder;
              sCurSortingKey != NULL &&
                  sCurOrder != NULL;
              sCurSortingKey = sCurSortingKey->next,
                  sCurOrder = sCurOrder->next )
        {
            if ( sCurOrder->direction != QMG_DIRECTION_NOT_DEFINED )
            {
                sCurSortingKey->direction = sCurOrder->direction;
            }
            else
            {
                /* Nothing to do */
            }
        }
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
qmgWindowing::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgWindowing���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    (1) WNST�� materialize node ���� ��� ����
 *        Base Table + Column + Analytic Functions
 *       < RID ��� >
 *       - �Ϲ�
 *         Base Table         : ���� table���� RID
 *         Column             : Partition By Columns + Arguments
 *         Analytic Functions 
 *       - ������ Hash Based Grouping ó���� ���� ���
 *         Base Table         : ���� GRAG�� RID
 *         Column             : Partition By Columns + Arguments
 *         Analytic Functions 
 *       - ������ Sort Based Grouping ó���� ���� ���
 *         Base Table         : target list�� ���� column��
 *         Column             : Partition By Columns + Arguments
 *         Analytic Functions 
 *       < Push Projection ��� >
 *       Base Table        : ���� plan ���࿡ �ʿ��� Į��
 *       Column            : Partition By Columns + Arguments
 *       Analytic Functions 
 *    (2) WNST plan ����
 ***********************************************************************/

    qmgWINDOW       * sMyGraph;
    qmnPlan         * sWNST;
    UInt              sFlag;

    IDU_FIT_POINT_FATAL( "qmgWindowing::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------
    sMyGraph = (qmgWINDOW*) aGraph;
    sFlag    = 0;
    
    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    // BUG-38410
    // SCAN parallel flag �� �ڽ� ���� �����ش�.
    aGraph->left->flag  |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);

    // BUG-37277
    if ( ( aParent->type == QMG_SORTING ) &&
         ( sMyGraph->graph.left->type == QMG_GROUPING ) )
    {
        sFlag &= ~QMO_MAKEWNST_ORDERBY_GROUPBY_MASK;
        sFlag |= QMO_MAKEWNST_ORDERBY_GROUPBY_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( qmoOneMtrPlan::initWNST( aStatement,
                                       sMyGraph->graph.myQuerySet,
                                       sMyGraph->sortingKeyCount,
                                       sMyGraph->analyticFuncListPtrArr,
                                       sFlag,
                                       aParent->myPlan,
                                       &sWNST ) != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sWNST;

    //---------------------------------------------------
    // ���� Plan�� ����
    //---------------------------------------------------

    IDE_TEST( sMyGraph->graph.left->makePlan( aStatement ,
                                              &sMyGraph->graph ,
                                              sMyGraph->graph.left )
              != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    sMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_WINDOW;

    //-----------------------------------------------------
    // WNST ��� ������ Base Table�� �����ϴ� ���
    //
    // 1. ���� Graph�� �Ϲ� Graph�� ���
    //    Child�� dependencies�� �̿��� Base Table�� ����
    //
    // 2. ������ Grouping�� ���
    //    - Sort-Based�� ��� : Target�� Order By�� �������
    //                          (�Ŀ� target�� sort�� dst�� ����)
    //    - Hash-Based�� ��� : child���� GRAG�� ã�� Base Table�� ����
    //                          (�߰��� FILT�� �ü��ִ� )
    //
    // 3. ������ Set�� ���
    //    child�� HSDS�� Base Table�� ����
    //-----------------------------------------------------

    sFlag = 0;
    if( sMyGraph->graph.left->type == QMG_GROUPING )
    {
        if( (sMyGraph->graph.left->flag & QMG_SORT_HASH_METHOD_MASK )
            == QMG_SORT_HASH_METHOD_SORT )
        {
            sFlag &= ~QMO_MAKEWNST_BASETABLE_MASK;
            sFlag |= QMO_MAKEWNST_BASETABLE_TARGET;
        }
        else
        {
            sFlag &= ~QMO_MAKEWNST_BASETABLE_MASK;
            sFlag |= QMO_MAKEWNST_BASETABLE_GRAG;
        }
    }
    else if( sMyGraph->graph.left->type == QMG_SET )
    {
        //----------------------------------------------------------
        // PR-8369
        // ������ SET�ΰ�� VIEW�� �����ǹǷ� DEPENDENCY�� ����
        // BASE TABLE�� �����Ѵ�.
        // �ٸ�, UNION�� ��� , HSDS�� ���� BASE TABLE�� �����Ѵ�.
        //----------------------------------------------------------
        if( ((qmgSET*)sMyGraph->graph.left)->setOp == QMS_UNION )
        {
            sFlag &= ~QMO_MAKEWNST_BASETABLE_MASK;
            sFlag |= QMO_MAKEWNST_BASETABLE_HSDS;
        }
        else
        {
            sFlag &= ~QMO_MAKEWNST_BASETABLE_MASK;
            sFlag |= QMO_MAKEWNST_BASETABLE_DEPENDENCY;
        }
    }
    else
    {
        sFlag &= ~QMO_MAKEWNST_BASETABLE_MASK;
        sFlag |= QMO_MAKEWNST_BASETABLE_DEPENDENCY;
    }

    //----------------------------
    // WNST ����� ����
    //----------------------------
    
    if ( ( sMyGraph->graph.flag & QMG_CHILD_PRESERVED_ORDER_USE_MASK )
         == QMG_CHILD_PRESERVED_ORDER_CANNOT_USE )
    {
        sFlag &= ~QMO_MAKEWNST_PRESERVED_ORDER_MASK;
        sFlag |= QMO_MAKEWNST_PRESERVED_ORDER_FALSE;
    }
    else
    {
        sFlag &= ~QMO_MAKEWNST_PRESERVED_ORDER_MASK;
        sFlag |= QMO_MAKEWNST_PRESERVED_ORDER_TRUE;
    }

    //���� ��ü�� ����
    if( (sMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKEWNST_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKEWNST_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKEWNST_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKEWNST_DISK_TEMP_TABLE;
    }
    
    // sorting key�� analytic function�� partition by column����
    // (table, column) ���� �����ϴ�.
    // �׷��� partition by column �� (table, column)����
    // ���� plan ���� �ÿ� ���� ���� �ִ�.
    // ���� �̿� ���� sorting key �� �����Ͽ��� �Ѵ�.
    IDE_TEST( resetSortingKey( sMyGraph->sortingKeyPtrArr,
                               sMyGraph->sortingKeyCount,
                               sMyGraph->analyticFuncListPtrArr )
              != IDE_SUCCESS );

    // BUG-33663 Ranking Function
    // partition by expr�� order�� ������ �� sorting key array����
    // ���Ű����� sorting key�� ����
    IDE_TEST( compactSortingKeyArr( sMyGraph->graph.flag,
                                    sMyGraph->sortingKeyPtrArr,
                                    & sMyGraph->sortingKeyCount,
                                    sMyGraph->analyticFuncListPtrArr )
              != IDE_SUCCESS );

    /* BUG-40354 pushed rank */
    if ( (sMyGraph->graph.flag & QMG_WINDOWING_PUSHED_RANK_MASK) ==
         QMG_WINDOWING_PUSHED_RANK_TRUE )
    {
        if ( sMyGraph->sortingKeyCount == 1 )
        {
            sFlag &= ~QMO_MAKEWNST_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEWNST_MEMORY_TEMP_TABLE;
            
            sFlag &= ~QMO_MAKEWNST_PUSHED_RANK_MASK;
            sFlag |= QMO_MAKEWNST_PUSHED_RANK_TRUE;
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        /* Nothing to do. */
    }
    
    IDE_TEST( qmoOneMtrPlan::makeWNST( aStatement,
                                       sMyGraph->graph.myQuerySet,
                                       sFlag,
                                       sMyGraph->distAggArg,
                                       sMyGraph->sortingKeyCount,
                                       sMyGraph->sortingKeyPtrArr,
                                       sMyGraph->analyticFuncListPtrArr,
                                       sMyGraph->graph.myPlan ,
                                       sMyGraph->graph.costInfo.inputRecordCnt,
                                       sWNST ) != IDE_SUCCESS);

    sMyGraph->graph.myPlan = sWNST;

    qmg::setPlanInfo( aStatement, sWNST, &(sMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgWindowing::resetSortingKey( qmgPreservedOrder ** aSortingKeyPtrArr,
                               UInt                 aSortingKeyCount,
                               qmsAnalyticFunc   ** aAnalyticFuncListPtrArr)
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *    Child Plan ���� �ÿ� Sorting Key (table, column) ������
 *    ����Ǵ� ��찡 ����
 *    ���� Child Plan ���� ��, �̸� ��������
 *
 ***********************************************************************/

    qmgPreservedOrder   * sCurSortingKeyCol;
    UInt                  sSortingKeyColCnt;
    UInt                  sOverColumnCnt;
    qmsAnalyticFunc     * sAnalFunc;
    qtcOverColumn       * sCurPartAndOrderByCol;
    UInt                  i;
    mtcNode             * sNode;

    IDU_FIT_POINT_FATAL( "qmgWindowing::resetSortingKey::__FT__" );

    for ( i = 0; i < aSortingKeyCount; i++ )
    {
        //--------------------------------------------------
        // Sorting Key�� ������ Partition By Column�� ������
        // Analytic Function�� ã��
        //--------------------------------------------------
        sSortingKeyColCnt = 0;
        for ( sCurSortingKeyCol = aSortingKeyPtrArr[i];
              sCurSortingKeyCol != NULL;
              sCurSortingKeyCol = sCurSortingKeyCol->next )
        {
            sSortingKeyColCnt++;
        }

        // sorting key array���� i��° sorting key��
        // analytic function list pointer array���� i��°�� �ִ�
        // anlaytic function list���� sorting key �̹Ƿ�
        // ���߿��� ���� key count�� ������ partition by column�� ã����
        // �ȴ�.
        for ( sAnalFunc = aAnalyticFuncListPtrArr[i];
              sAnalFunc != NULL;
              sAnalFunc = sAnalFunc->next )
        {
            sOverColumnCnt = 0;
            for ( sCurPartAndOrderByCol =
                      sAnalFunc->analyticFuncNode->overClause->overColumn;
                  sCurPartAndOrderByCol != NULL;
                  sCurPartAndOrderByCol = sCurPartAndOrderByCol->next )
            {
                sOverColumnCnt++;
            }

            if ( sSortingKeyColCnt == sOverColumnCnt )
            {
                break;
            }
            else
            {
                // ���ο� analytic function ã��
            }
        }

        IDE_TEST_RAISE( sAnalFunc == NULL,
                        ERR_INVALID_ANAL_FUNC );

        //--------------------------------------------------
        // Sorting Key ������ ����� ���, �� ����
        //--------------------------------------------------
        
        for ( sCurSortingKeyCol = aSortingKeyPtrArr[i],
                  sCurPartAndOrderByCol =
                  sAnalFunc->analyticFuncNode->overClause->overColumn;
              sCurSortingKeyCol != NULL &&
                  sCurPartAndOrderByCol != NULL;
              sCurSortingKeyCol = sCurSortingKeyCol->next,
                  sCurPartAndOrderByCol = sCurPartAndOrderByCol->next )
        {
            sNode = &sCurPartAndOrderByCol->node->node;

            while( sNode->module == &qtc::passModule )
            {
                sNode = sNode->arguments;
            }

            sCurSortingKeyCol->table  = sNode->table;
            sCurSortingKeyCol->column = sNode->column;
        }

        // sorting key�� partition by col�� ������ ������ �־�� ��
        // ���� ��� ����
        IDE_DASSERT( sCurSortingKeyCol == NULL );
        IDE_DASSERT( sCurPartAndOrderByCol == NULL );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ANAL_FUNC )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgWindowing::resetSortingKey",
                                  "sAnalFunc is NULL" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgWindowing::compactSortingKeyArr( UInt                 aFlag,
                                    qmgPreservedOrder ** aSortingKeyPtrArr,
                                    UInt               * aSortingKeyCount,
                                    qmsAnalyticFunc   ** aAnalyticFuncListPtrArr )
{
/***********************************************************************
 *
 * Description :
 *    order by expr�� order�� �����ǰ� ������ sorting key�� ��ϵǾ���.
 *    ���� partition by expr�� order�� �����Ǹ� ������ order��
 *    ���� �� �����Ƿ� sorting key�� ������ �� �ִ�.
 *
 * Implementation :
 *    first order(child preserved order)��
 *    last order(parent preserved order)�� ����Ͽ� sorting key array����
 *    ������ order�� ���� sorting key�� �����Ѵ�.
 *
 ***********************************************************************/

    qmsAnalyticFunc  * sAnalyticFuncList;
    UInt               sSortingKeyCount;
    UInt               sStartKey;
    UInt               sEndKey;
    idBool             sIsFound;
    UInt               i;
    UInt               j;

    IDU_FIT_POINT_FATAL( "qmgWindowing::resetSortingKey::__FT__" );

    sSortingKeyCount = *aSortingKeyCount;

    if ( sSortingKeyCount > 1 )
    {
        //----------------------------------
        // last order�� ������ �κ� �ߺ� ����
        //----------------------------------

        sStartKey = 0;

        if ( (aFlag & QMG_PRESERVED_ORDER_MASK)
             == QMG_PRESERVED_ORDER_DEFINED_FIXED )
        {
            sEndKey = sSortingKeyCount - 1;
        }
        else
        {
            sEndKey = sSortingKeyCount;
        }

        for ( i = sStartKey; i < sEndKey; i++ )
        {
            if ( aSortingKeyPtrArr[i] == NULL )
            {
                continue;
            }
            else
            {
                // Nothing to do.
            }

            for ( j = i + 1; j < sEndKey; j++ )
            {
                if ( aSortingKeyPtrArr[j] == NULL )
                {
                    continue;
                }
                else
                {
                    // Nothing to do.
                }

                if ( isSameSortingKey( aSortingKeyPtrArr[i],
                                       aSortingKeyPtrArr[j] ) == ID_TRUE )
                {
                    // merge sorting key
                    mergeSortingKey( aSortingKeyPtrArr[i],
                                     aSortingKeyPtrArr[j] );
                    aSortingKeyPtrArr[j] = NULL;

                    // merge analytic function
                    for ( sAnalyticFuncList = aAnalyticFuncListPtrArr[i];
                          sAnalyticFuncList->next != NULL;
                          sAnalyticFuncList = sAnalyticFuncList->next ) ;

                    sAnalyticFuncList->next = aAnalyticFuncListPtrArr[j];
                    aAnalyticFuncListPtrArr[j] = NULL;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        //----------------------------------
        // last order �ߺ� ����
        //----------------------------------

        if ( (aFlag & QMG_PRESERVED_ORDER_MASK)
             == QMG_PRESERVED_ORDER_DEFINED_FIXED )
        {
            if ( (aFlag & QMG_CHILD_PRESERVED_ORDER_USE_MASK)
                 == QMG_CHILD_PRESERVED_ORDER_CAN_USE )
            {
                sStartKey = 1;
            }
            else
            {
                sStartKey = 0;
            }

            sEndKey = sSortingKeyCount - 1;

            i = sSortingKeyCount - 1;

            for ( j = sStartKey; j < sEndKey; j++ )
            {
                if ( aSortingKeyPtrArr[j] == NULL )
                {
                    continue;
                }
                else
                {
                    // Nothing to do.
                }

                if ( isSameSortingKey( aSortingKeyPtrArr[i],
                                       aSortingKeyPtrArr[j] ) == ID_TRUE )
                {
                    // merge sorting key
                    mergeSortingKey( aSortingKeyPtrArr[i],
                                     aSortingKeyPtrArr[j] );
                    aSortingKeyPtrArr[j] = NULL;

                    // merge analytic function
                    for ( sAnalyticFuncList = aAnalyticFuncListPtrArr[i];
                          sAnalyticFuncList->next != NULL;
                          sAnalyticFuncList = sAnalyticFuncList->next ) ;

                    sAnalyticFuncList->next = aAnalyticFuncListPtrArr[j];
                    aAnalyticFuncListPtrArr[j] = NULL;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
        else
        {
            // Nothing to do.
        }

        //----------------------------------
        // ������ sort key ����
        //----------------------------------

        sStartKey = 0;
        sEndKey = sSortingKeyCount;

        for ( i = sStartKey; i < sEndKey; i++ )
        {
            if ( aSortingKeyPtrArr[i] == NULL )
            {
                sIsFound = ID_FALSE;

                for ( j = i + 1; j < sEndKey; j++ )
                {
                    if ( aSortingKeyPtrArr[j] != NULL )
                    {
                        sIsFound = ID_TRUE;
                        break;
                    }
                }

                if ( sIsFound == ID_TRUE )
                {
                    aSortingKeyPtrArr[i] = aSortingKeyPtrArr[j];
                    aSortingKeyPtrArr[j] = NULL;

                    aAnalyticFuncListPtrArr[i] = aAnalyticFuncListPtrArr[j];
                    aAnalyticFuncListPtrArr[j] = NULL;
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

        sSortingKeyCount = 0;

        for ( i = sStartKey; i < sEndKey; i++ )
        {
            if ( aSortingKeyPtrArr[i] != NULL )
            {
                sSortingKeyCount++;
            }
            else
            {
                break;
            }
        }

        *aSortingKeyCount = sSortingKeyCount;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

idBool
qmgWindowing::isSameSortingKey( qmgPreservedOrder * aSortingKey1,
                                qmgPreservedOrder * aSortingKey2 )
{
/***********************************************************************
 *
 * Description :
 *     aSortingKey1�� aSortingKey2�� ���� order�� ������ ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPreservedOrder  * sOrder1;
    qmgPreservedOrder  * sOrder2;
    idBool               sIsSame;

    sIsSame = ID_TRUE;

    for ( sOrder1 = aSortingKey1, sOrder2 = aSortingKey2;
          (sOrder1 != NULL) && (sOrder2 != NULL);
          sOrder1 = sOrder1->next, sOrder2 = sOrder2->next )
    {
        if ( (sOrder1->table == sOrder2->table ) &&
             (sOrder1->column == sOrder2->column ) )
        {
            switch ( sOrder1->direction )
            {
                case QMG_DIRECTION_NOT_DEFINED:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_ASC:
                        case QMG_DIRECTION_DESC:
                        case QMG_DIRECTION_ASC_NULLS_FIRST:
                        case QMG_DIRECTION_ASC_NULLS_LAST:
                        case QMG_DIRECTION_DESC_NULLS_FIRST:
                        case QMG_DIRECTION_DESC_NULLS_LAST:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }
                    
                    break;
                }
                case QMG_DIRECTION_ASC:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_ASC:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }
                    
                    break;
                }
                case QMG_DIRECTION_DESC:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_DESC:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }
                    
                    break;
                }
                case QMG_DIRECTION_ASC_NULLS_FIRST:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_ASC_NULLS_FIRST:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }

                    break;
                }
                case QMG_DIRECTION_ASC_NULLS_LAST:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_ASC_NULLS_LAST:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }

                    break;
                }
                case QMG_DIRECTION_DESC_NULLS_FIRST:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_DESC_NULLS_FIRST:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }

                    break;
                }
                case QMG_DIRECTION_DESC_NULLS_LAST:
                {
                    switch ( sOrder2->direction )
                    {
                        case QMG_DIRECTION_NOT_DEFINED:
                        case QMG_DIRECTION_DESC_NULLS_LAST:
                        {
                            break;
                        }
                        default:
                        {
                            sIsSame = ID_FALSE;
                            break;
                        }
                    }

                    break;
                }
                default:
                {
                    sIsSame = ID_FALSE;
                    break;
                }
            }
        }
        else
        {
            sIsSame = ID_FALSE;
        }
    }

    if ( sIsSame == ID_TRUE )
    {
        if ( (sOrder1 == NULL) && (sOrder2 == NULL) )
        {
            // Nothing to do.
        }
        else
        {
            sIsSame = ID_FALSE;
        }
    }
    else
    {
        // Nothing to do.
    }

    return sIsSame;
}

void
qmgWindowing::mergeSortingKey( qmgPreservedOrder * aSortingKey1,
                               qmgPreservedOrder * aSortingKey2 )
{
/***********************************************************************
 *
 * Description :
 *     aSortingKey1�� aSortingKey2�� order�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPreservedOrder  * sOrder1;
    qmgPreservedOrder  * sOrder2;

    for ( sOrder1 = aSortingKey1, sOrder2 = aSortingKey2;
          (sOrder1 != NULL) && (sOrder2 != NULL);
          sOrder1 = sOrder1->next, sOrder2 = sOrder2->next )
    {
        switch ( sOrder1->direction )
        {
            case QMG_DIRECTION_NOT_DEFINED:
            {
                switch ( sOrder2->direction )
                {
                    case QMG_DIRECTION_NOT_DEFINED:
                    case QMG_DIRECTION_ASC:
                    case QMG_DIRECTION_DESC:
                    case QMG_DIRECTION_ASC_NULLS_FIRST:
                    case QMG_DIRECTION_ASC_NULLS_LAST:
                    case QMG_DIRECTION_DESC_NULLS_FIRST:
                    case QMG_DIRECTION_DESC_NULLS_LAST:
                    {
                        sOrder1->direction = sOrder2->direction;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                
                break;
            }
            default:
            {
                break;
            }
        }
    }

}

IDE_RC
qmgWindowing::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgWindowing::printGraph::__FT__" );

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
qmgWindowing::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Windowing Graph�� Preserved order Finalizing��
 *                �ٸ� graph�� �ٸ��� Sorting key�� ó���Ѵ�.
 *                �迭�� ù��° key�� child graph�� direction�� ������.
 *  (��, graph�� flag�� QMG_CHILD_PRESERVED_ORDER_CANNOT_USE�� ���õǾ� ������ �ȵȴ�.)
 *                �迭�� �ι�°���� ������ key������ direction�� ascending���� ���� �����Ѵ�.
 *
 *  Implementation :
 *     1. Child graph�� Preserved order ��� ���ο� ���� ù��° Sorting Key�� �����Ѵ�.
 *     2. �������� Ascending���� �����Ѵ�.
 *
 ***********************************************************************/

    qmgWINDOW         *  sMyGraph;
    qmgPreservedOrder ** sSortingKeyPtrArr;
    qmgPreservedOrder *  sPreservedOrder;
    qmgDirectionType     sPrevDirection;
    qmgPreservedOrder *  sChildOrder;

    UInt                 i;

    IDU_FIT_POINT_FATAL( "qmgWindowing::finalizePreservedOrder::__FT__" );

    // BUG-36803 Windowing Graph must have a left child graph
    IDE_DASSERT( aGraph->left != NULL );

    sMyGraph = (qmgWINDOW *)aGraph;

    sSortingKeyPtrArr = sMyGraph->sortingKeyPtrArr;
    sPreservedOrder   = aGraph->preservedOrder;
    sChildOrder       = aGraph->left->preservedOrder;

    /* BUG-43380 windowsort Optimize ���߿� fatal */
    /* ������ index�� ������ sorting�� �ʿ䰡 ������ ���� */
    if ( sMyGraph->sortingKeyCount > 0 )
    {
        // 1. Child graph�� Preserved order ��� ���ο� ���� ù��° Sorting Key�� �����Ѵ�.
        if ( ( aGraph->flag & QMG_CHILD_PRESERVED_ORDER_USE_MASK )
             == QMG_CHILD_PRESERVED_ORDER_CAN_USE )
        {
            sPreservedOrder = sSortingKeyPtrArr[0];

            if ( sPreservedOrder->direction == QMG_DIRECTION_NOT_DEFINED )
            {
                sPreservedOrder->direction = sChildOrder->direction;
                sPrevDirection = sChildOrder->direction;
            }
            else
            {
                IDE_DASSERT( sPreservedOrder->direction == sChildOrder->direction );
                sPrevDirection = sPreservedOrder->direction;
            }
        }
        else
        {
            sPreservedOrder = sSortingKeyPtrArr[0];

            if ( sPreservedOrder->direction == QMG_DIRECTION_NOT_DEFINED )
            {
                sPreservedOrder->direction = QMG_DIRECTION_ASC;
                sPrevDirection = QMG_DIRECTION_ASC;
            }
            else
            {
                sPrevDirection = sPreservedOrder->direction;
            }
        }

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
                    break;
                case QMG_DIRECTION_DESC :
                    break;
                case QMG_DIRECTION_ASC_NULLS_FIRST :
                    break;
                case QMG_DIRECTION_ASC_NULLS_LAST :
                    break;
                case QMG_DIRECTION_DESC_NULLS_FIRST :
                    break;
                case QMG_DIRECTION_DESC_NULLS_LAST :
                    break;
                default:
                    break;
            }
                
            sPrevDirection = sPreservedOrder->direction;
        }
    }
    else
    {
        /* Nothing to do */
    }

    // 2. �������� Ascending���� �����Ѵ�.
    for ( i = 1; i < sMyGraph->sortingKeyCount; i++ )
    {
        sPreservedOrder = sSortingKeyPtrArr[i];

        if ( sPreservedOrder->direction == QMG_DIRECTION_NOT_DEFINED )
        {
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
                        break;
                    case QMG_DIRECTION_DESC :
                        break;
                    case QMG_DIRECTION_ASC_NULLS_FIRST :
                        break;
                    case QMG_DIRECTION_ASC_NULLS_LAST :
                        break;
                    case QMG_DIRECTION_DESC_NULLS_FIRST :
                        break;
                    case QMG_DIRECTION_DESC_NULLS_LAST :
                        break;
                    default:
                        break;
                }
            
                sPrevDirection = sPreservedOrder->direction;
            }
        }
        else
        {
            // nothing to do 
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmgWindowing::getBucketCnt4Windowing( qcStatement  * aStatement,
                                      qmgWINDOW    * aMyGraph,
                                      qtcOverColumn* aGroupBy,
                                      UInt           aHintBucketCnt,
                                      UInt         * aBucketCnt )
{
/***********************************************************************
 *
 *  Description : qmgGrouping::getBucketCnt4Group��
 *                ������ �Լ� �������̽��� ���� �޶� ���縦 ����
 *
 ***********************************************************************/

    qmoColCardInfo   * sColCardInfo;
    qtcOverColumn    * sGroup;
    qtcNode          * sNode;

    SDouble            sCardinality;
    SDouble            sBucketCnt;
    ULong              sBucketCntOutput;

    idBool             sAllColumn;

    IDU_FIT_POINT_FATAL( "qmgWindowing::getBucketCnt4Windowing::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aMyGraph   != NULL );

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

        sBucketCnt = aMyGraph->graph.left->costInfo.outputRecordCnt / 2.0;
        sBucketCnt = ( sBucketCnt < 1 ) ? 1 : sBucketCnt;

        //------------------------------------------
        // group column���� cardinality ���� ���Ѵ�.
        //------------------------------------------

        for ( sGroup = aGroupBy;
              sGroup != NULL;
              sGroup = sGroup->next )
        {
            sNode = sGroup->node;

            if ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
            {
                // group ����� ������ Į���� ���
                sColCardInfo = QC_SHARED_TMPLATE(aStatement)->
                    tableMap[sNode->node.table].
                    from->tableRef->statInfo->colCardInfo;

                sCardinality *= sColCardInfo[sNode->node.column].columnNDV;

                if ( sCardinality > QMC_MEM_HASH_MAX_BUCKET_CNT )
                {
                    // max bucket count���� Ŭ ���
                    sCardinality = QMC_MEM_HASH_MAX_BUCKET_CNT;
                    break;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                sAllColumn = ID_FALSE;
                break;
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
            /* BUG-48161 */
            if ( sBucketCnt > QCG_GET_BUCKET_COUNT_MAX( aStatement ) )
            {
                // fix BUG-14070
                // bucketCnt�� QMC_MEM_HASH_MAX_BUCKET_CNT(10240000)�� ������
                // QMC_MEM_HASH_MAX_BUCKET_CNT ������ �������ش�.
                sBucketCnt = QCG_GET_BUCKET_COUNT_MAX( aStatement );
            }
            else
            {
                // nothing to do
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
