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
 * $Id: qmgSelection.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Selection Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcg.h>
#include <qmgSelection.h>
#include <qmgProjection.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoParallelPlan.h>
#include <qmo.h>
#include <qmoCost.h>
#include <qmoSelectivity.h>
#include <qmoPushPred.h>
#include <qmoRownumPredToLimit.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmgSetRecursive.h>

IDE_RC
qmgSelection::init( qcStatement * aStatement,
                    qmsQuerySet * aQuerySet,
                    qmsFrom     * aFrom,
                    qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgSelection Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgSelection�� ���� ���� �Ҵ� ����
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgSELT       * sMyGraph;
    qmsTableRef   * sTableRef;

    IDU_FIT_POINT_FATAL( "qmgSelection::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );

    //---------------------------------------------------
    // Selection Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgSelection�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgSELT ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );


    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SELECTION;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aFrom->depInfo );
    sMyGraph->graph.myFrom = aFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgSelection::optimize;
    sMyGraph->graph.makePlan = qmgSelection::makePlan;
    sMyGraph->graph.printGraph = qmgSelection::printGraph;

    // Disk/Memory ���� ����
    sTableRef =  sMyGraph->graph.myFrom->tableRef;
    if ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTableRef->table].lflag
           & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_DISK )
    {
        sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
    }
    else
    {
        sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
    }

    sMyGraph->graph.flag &= ~QMG_ROWNUM_PUSHED_MASK;
    sMyGraph->graph.flag |= QMG_ROWNUM_PUSHED_TRUE;

    //---------------------------------------------------
    // Selection ���� ������ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->limit = NULL;
    sMyGraph->selectedIndex = NULL;
    sMyGraph->selectedMethod = NULL;
    sMyGraph->accessMethodCnt = 0;
    sMyGraph->accessMethod = NULL;
    sMyGraph->sdf = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition�� ���� selection graph���� ���
    sMyGraph->partitionRef = NULL;

    sMyGraph->mFlag = QMG_SELT_FLAG_CLEAR;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::init( qcStatement     * aStatement,
                    qmsQuerySet     * aQuerySet,
                    qmsFrom         * aFrom,
                    qmsPartitionRef * aPartitionRef,
                    qmgGraph       ** aGraph )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *               qmgSelection Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgSelection�� ���� ���� �Ҵ� ����
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) reference partition�� ����
 *    (3) out ����
 *
 ***********************************************************************/

    qmgSELT       * sMyGraph;
    qmsTableRef   * sTableRef;

    IDU_FIT_POINT_FATAL( "qmgSelection::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aPartitionRef != NULL );

    //---------------------------------------------------
    // Selection Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------
    sTableRef = aFrom->tableRef;

    // qmgSelection�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgSELT ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );


    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SELECTION;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aFrom->depInfo );

    // partition�� dependency�� ����.
    qtc::dependencyChange( sTableRef->table,
                           aPartitionRef->table,
                           &sMyGraph->graph.depInfo,
                           &sMyGraph->graph.depInfo );

    sMyGraph->graph.myFrom = aFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgSelection::optimizePartition;
    sMyGraph->graph.makePlan = qmgSelection::makePlanPartition;
    sMyGraph->graph.printGraph = qmgSelection::printGraphPartition;

    // Disk/Memory ���� ����
    if ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aPartitionRef->table].lflag
           & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_DISK )
    {
        sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
    }
    else
    {
        sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
    }

    //---------------------------------------------------
    // Selection ���� ������ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->limit = NULL;
    sMyGraph->selectedIndex = NULL;
    sMyGraph->selectedMethod = NULL;
    sMyGraph->accessMethodCnt = 0;
    sMyGraph->accessMethod = NULL;
    sMyGraph->sdf = NULL;

    // PROJ-1502 PARTITIONED DISK TABLE
    // partition�� ���� selection graph���� ���
    sMyGraph->partitionRef = aPartitionRef;

    /* BUG-44659 �̻�� Partition�� ��� ������ ����ϴٰ�,
     *           Graph�� Partition/Column/Index Name �κп��� ������ ������ �� �ֽ��ϴ�.
     *  Lock�� ���� �ʰ� Meta Cache�� ����ϸ�, ������ ������ �� �ֽ��ϴ�.
     *  qmgSELT���� Partition Name�� �����ϵ��� �����մϴ�.
     */
    (void)idlOS::memcpy( sMyGraph->partitionName,
                         aPartitionRef->partitionInfo->name,
                         idlOS::strlen( aPartitionRef->partitionInfo->name ) + 1 );

    sMyGraph->graph.flag &= ~QMG_SELT_PARTITION_MASK;
    sMyGraph->graph.flag |= QMG_SELT_PARTITION_TRUE;

    sMyGraph->mFlag = QMG_SELT_FLAG_CLEAR;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSelection::optimize(qcStatement * aStatement, qmgGraph * aGraph)
{
/***********************************************************************
 *
 * Description : qmgSelection�� ����ȭ
 *
 * Implementation :
 *    (1) View Graph ����
 *        - BUG-9736 ���� �ÿ� View Graph ���������� init �������� �Ű�����
 *          init �������� View Graph�� ������ ���, Push Selection�� ������ ��
 *          ����. BUG-9736�� left outer, full outer join�� subquery graph ����
 *          ��ġ �������� �ذ��ؾ� ��
 *    (2) bad transitive predicate�� ����
 *    (3) Subquery Graph ����
 *    (4) ���� ��� ���� ����( recordSize, inputRecordCnt, pageCnt )
 *    (5) Predicate ���ġ �� ���� Predicate�� selectivity ���
 *    (6) ��ü selectivity ��� �� ���� ��� ������ selectivity�� ����
 *    (7) Access Method ����
 *    (8) Preserved Order ����
 *    (9) ���� ��� ���� ����( outputRecordCnt, myCost, totalCost )
 *
 ***********************************************************************/

    UInt              sColumnCnt;
    UInt              sIndexCnt;
    UInt              sSelectedScanHint;
    UInt              sParallelDegree;

    SDouble           sOutputRecordCnt;
    SDouble           sRecordSize;

    UInt              i;

    qmgSELT         * sMyGraph;
    qmsTarget       * sTarget;
    qmsTableRef     * sTableRef;
    qcmColumn       * sColumns;
    mtcColumn       * sTargetColumn;

    IDU_FIT_POINT_FATAL( "qmgSelection::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgSELT*) aGraph;
    sTableRef = sMyGraph->graph.myFrom->tableRef;
    sRecordSize = 0;

    //---------------------------------------------------
    // View Graph�� ���� �� ��� ���� ����
    //   ( �Ϲ� Table�� ��� ������ Validation �������� ������ )
    //---------------------------------------------------

    // PROJ-2582 recursive with
    // recursive view�� view ó�� ó�� �ϱ� ���� view ����..
    if ( sTableRef->recursiveView != NULL )
    {
        sTableRef->view = sTableRef->recursiveView;
    }
    else
    {
        // Nothing To Do
    }

    if( sTableRef->view != NULL )
    {
        // View Graph�� ����
        IDE_TEST( makeViewGraph( aStatement, & sMyGraph->graph )
                  != IDE_SUCCESS );

        // ��� ������ ����
        IDE_TEST( qmoStat::getStatInfo4View( aStatement,
                                             & sMyGraph->graph,
                                             & sTableRef->statInfo )
                  != IDE_SUCCESS );

        // fix BUG-11209
        // selection graph�� ������ view�� �� �� �����Ƿ�
        // view�� ���� ���� view�� projection graph�� Ÿ������ flag�� �����Ѵ�.
        sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
        sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MASK &
            sMyGraph->graph.left->flag;
    }
    else
    {
        //---------------------------------------------------
        // ::init���� �̹� flag �ʱ�ȭ�� �̷�� �����Ƿ�, View�� �ƴѰ�쿡��
        // �ƹ�ó���� ���� ������ �ȴ�.
        // PROJ-2582 recursive with
        // recursive view�� ��� ������ ����
        // validate �ܰ迡�� view�� statInfo ���� ���� �ʱ� ������
        // right�� recursive view�� tableRef�� statInfo�� ����.
        // right child�� left�� VMTR�� ��������� ����
        //---------------------------------------------------

        if ( ( sTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
             == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
        {
            // right query�� recursive view�� �ӽ÷� ������ left�� graph����
            sMyGraph->graph.left = aStatement->myPlan->graph;

            // �ٽ� right query�� recursive view graph�� ��ȯ
            aStatement->myPlan->graph = & sMyGraph->graph;
            
            // ��� ������ ����
            IDE_TEST( qmoStat::getStatInfo4View( aStatement,
                                                 & sMyGraph->graph,
                                                 & sTableRef->statInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }
    }

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

    //---------------------------------------------------
    // PROJ-1404
    // ���ʿ��� bad transitive predicate�� �����Ѵ�.
    //---------------------------------------------------
    
    // ������ transitive predicate�� ����ڰ� �Է��� predicate�� �ߺ��Ǵ�
    // ��� ������ �̸� �����ϴ� ���� ����.
    // (������ �����ϴ� transitive predicate�� one table predicate�̶�
    // selection graph���� ó���� �� �ִ�.)
    //
    // ��1)
    // select * from ( select * from t1 where t1.i1=1 ) v1, t2
    // where v1.i1=t2.i1 and t2.i1=1;
    // transitive predicate {t1.i1=1}�� ����������
    // v1 where���� predicate�� �ߺ��ǹǷ� �����ϴ� ���� ����.
    //
    // ��2)
    // select * from t1 left join t2 on t1.i1=t2.i1 and t1.i1=1
    // where t2.i1=1;
    // on������ transitive predicate {t2.i1=1}�� ����������
    // �̴� right�� �������� �ǰ� where������ ������
    // predicate�� �ߺ��ǹǷ� �����ϴ� ���� ����.
    
    if ( ( sMyGraph->graph.myPredicate != NULL ) &&
         ( sTableRef->view == NULL ) )
    {
        IDE_TEST( qmoPred::removeEquivalentTransitivePredicate(
                      aStatement,
                      & sMyGraph->graph.myPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------------------
    // Subquery�� Graph ����
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        // To Fix PR-11461, PR-11460
        // �Ϲ� Table�� ��쿡�� Subquery KeyRange ����ȭ�� ������ �� �ִ�.
        if ( sTableRef->view == NULL )
        {
            // �Ϲ� ���̺��� ���
            IDE_TEST(
                qmoPred::optimizeSubqueries( aStatement,
                                             sMyGraph->graph.myPredicate,
                                             ID_TRUE ) // Use KeyRange Tip
                != IDE_SUCCESS );
        }
        else
        {
            // View�� ���
            IDE_TEST(
                qmoPred::optimizeSubqueries( aStatement,
                                             sMyGraph->graph.myPredicate,
                                             ID_FALSE ) // No KeyRange Tip
                != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------
    // ���� ��� ������ ����
    //---------------------------------------------------
    
    // recordSize ����
    if ( sTableRef->view == NULL )
    {
        // �Ϲ� Table�� ���
        sColumns = sTableRef->tableInfo->columns;
        sColumnCnt = sTableRef->tableInfo->columnCount;

        for ( i = 0; i < sColumnCnt; i++ )
        {
            sRecordSize += sColumns[i].basicInfo->column.size;
        }
    }
    else
    {
        // View�� ���,
        // To Fix BUG-8241
        for ( sTarget = ((qmgPROJ*)(sMyGraph->graph.left))->target;
              sTarget != NULL;
              sTarget = sTarget->next )
        {
            sTargetColumn = QTC_STMT_COLUMN( aStatement, sTarget->targetColumn );

            sRecordSize += sTargetColumn->column.size;
        }
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // inputRecordCnt ����
    sMyGraph->graph.costInfo.inputRecordCnt =
        sTableRef->statInfo->totalRecordCnt;

    //---------------------------------------------------
    // Predicate�� ���ġ �� ���� Predicate�� Selectivity ���
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        IDE_TEST(
            qmoPred::relocatePredicate(
                aStatement,
                sMyGraph->graph.myPredicate,
                & sMyGraph->graph.depInfo,
                & sMyGraph->graph.myQuerySet->outerDepInfo,
                sMyGraph->graph.myFrom->tableRef->statInfo,
                & sMyGraph->graph.myPredicate )
            != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    /* BUG-43006 FixedTable Indexing Filter
     * Fixed Table�� ����optimizer_formance_view ������Ƽ�� �����ִٸ� index�� ���ٰ�
     * ����������Ѵ�.
     */
    if ( ( ( sTableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
           ( sTableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
           ( sTableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) ) &&
         ( QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) == 0 ) )
    {
        sIndexCnt = 0;
    }
    else
    {
        sIndexCnt = sTableRef->tableInfo->indexCount;
    }

    /* PROJ-2402 Parallel Table Scan */
    setParallelScanFlag(aStatement, aGraph);

    //---------------------------------------------------------------
    // accessMethod ����
    // 
    // rid predicate �� �ִ� ��� ������ rid scan �� �õ��Ѵ�.
    // rid predicate �� �ִ���  rid scan �� �Ҽ� ���� ��쵵 �ִ�.
    // �� ��쿡�� index scan �� ���� �ʰ� full scan �� �ϰ� �ȴ�.
    //---------------------------------------------------------------

    if ( sMyGraph->graph.ridPredicate != NULL )
    {
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmoAccessMethod),
                                               (void**)&sMyGraph->accessMethod)
                 != IDE_SUCCESS);

        sMyGraph->accessMethod->method               = NULL;
        sMyGraph->accessMethod->keyRangeSelectivity  = 0;
        sMyGraph->accessMethod->keyFilterSelectivity = 0;
        sMyGraph->accessMethod->filterSelectivity    = 0;
        sMyGraph->accessMethod->methodSelectivity    = 0;

        sMyGraph->accessMethod->totalCost = qmoCost::getTableRIDScanCost(
            sTableRef->statInfo,
            &sMyGraph->accessMethod->accessCost,
            &sMyGraph->accessMethod->diskCost );

        sMyGraph->selectedIndex   = NULL;
        sMyGraph->accessMethodCnt = 1;
        sMyGraph->selectedMethod  = &sMyGraph->accessMethod[0];
        sMyGraph->forceIndexScan  = ID_FALSE;
        sMyGraph->forceRidScan    = ID_FALSE;

        sSelectedScanHint = QMG_NOT_USED_SCAN_HINT;
    }
    else
    {
        // TASK-6699 TPC-H ���� ����
        // Cost ��꿡 �ʿ��� ParallelDegree�� ����Ѵ�.
        // �� ParallelDegree ���δٰ��ؼ� ����ؼ� ������ �������� �����Ƿ� �ִ밪�� 4
        if ( (sMyGraph->mFlag & QMG_SELT_PARALLEL_SCAN_MASK) == QMG_SELT_PARALLEL_SCAN_TRUE )
        {
            if ( sMyGraph->graph.myFrom->tableRef->mParallelDegree > 1 )
            {
                sParallelDegree = IDL_MIN( sMyGraph->graph.myFrom->tableRef->mParallelDegree, 4 );
            }
            else
            {
                sParallelDegree = 1;
            }
        }
        else
        {
            sParallelDegree = 1;
        }

        sMyGraph->accessMethodCnt = sIndexCnt + 1;

        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmoAccessMethod) *
                                               (sIndexCnt + 1),
                                               (void**)&sMyGraph->accessMethod)
                 != IDE_SUCCESS );

        IDE_TEST( getBestAccessMethod( aStatement,
                                       & sMyGraph->graph,
                                       sTableRef->statInfo,
                                       sMyGraph->graph.myPredicate,
                                       sMyGraph->accessMethod,
                                       & sMyGraph->selectedMethod,
                                       & sMyGraph->accessMethodCnt,
                                       & sMyGraph->selectedIndex,
                                       & sSelectedScanHint,
                                       sParallelDegree,
                                       0 )
                  != IDE_SUCCESS);

        // TASK-6699 TPC-H ���� ����
        // Index �� ���õǸ� Parallel ������ �ȵ�
        if (sMyGraph->selectedIndex != NULL)
        {
            sMyGraph->mFlag &= ~QMG_SELT_PARALLEL_SCAN_MASK;
            sMyGraph->mFlag |= QMG_SELT_PARALLEL_SCAN_FALSE;
        }
        else
        {
            // nothing to do
        }

        // To fix BUG-12742
        // hint�� ����� ���� index�� ������ �� ����.
        if( (sSelectedScanHint == QMG_USED_SCAN_HINT) ||
            (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_DEQUEUE) )
        {
            sMyGraph->forceIndexScan = ID_TRUE;
        }
        else
        {
            sMyGraph->forceIndexScan = ID_FALSE;
        }

        sMyGraph->forceRidScan = ID_FALSE;
    }

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    // preserved order �ʱ�ȭ
    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NOT_DEFINED;

    if ( sMyGraph->selectedMethod->method == NULL )
    {
        //---------------------------------------------------
        // FULL SCAN�� ���õ� ���
        //---------------------------------------------------

        if ( sMyGraph->graph.left != NULL )
        {
            //---------------------------------------------------
            // View�� ���, ������ Preserved Order�� ����
            //---------------------------------------------------

            if ( sMyGraph->graph.left->preservedOrder != NULL )
            {
                // BUG-43692,BUG-44004
                // recursive with (recursive view) �� preserved order������ ����.
                if ( ( sTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                     == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
                {
                    sMyGraph->graph.preservedOrder = NULL;
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                }
                else
                {
                    // ���� view�� preserved order ���� ��,
                    // table ID ����
                    IDE_TEST( copyPreservedOrderFromView(
                                  aStatement,
                                  sMyGraph->graph.left,
                                  sTableRef->table,
                                  & sMyGraph->graph.preservedOrder )
                              != IDE_SUCCESS );

                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= ( sMyGraph->graph.left->flag &
                                              QMG_PRESERVED_ORDER_MASK );
                }
            }
            else
            {
                sMyGraph->graph.preservedOrder = NULL;
                sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;

                if ( ( ( sMyGraph->graph.left->flag & QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK ) ==
                       QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE ) ||
                     ( ( sMyGraph->graph.left->flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK ) ==
                       QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE ) )
                {
                    // view ����ȭ type�� View Materialization�� ���
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                    sMyGraph->graph.left->flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.left->flag |= QMG_PRESERVED_ORDER_NEVER;
                }
                else
                {
                    //view ����ȭ type�� Push Selection�� ���
                    sMyGraph->graph.flag |= ( sMyGraph->graph.left->flag &
                                              QMG_PRESERVED_ORDER_MASK );
                }
            }
        }
        else
        {
            // nothing to do
        }

        if ( ( sMyGraph->graph.flag & QMG_PRESERVED_ORDER_MASK )
             == QMG_PRESERVED_ORDER_NOT_DEFINED )
        {
            if( sSelectedScanHint == QMG_USED_ONLY_FULL_SCAN_HINT )
            {
                //---------------------------------------------------
                // FULL SCAN Hint�� ���õ� ���
                //---------------------------------------------------

                sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
            }
            else
            {
                //---------------------------------------------------
                // cost�� ���� FULL SCAN�� ���õ� ���
                //---------------------------------------------------
                if ( sMyGraph->accessMethodCnt > 1 )
                {
                    // index�� �����ϴ� ���
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |=
                        QMG_PRESERVED_ORDER_NOT_DEFINED;
                }
                else
                {
                    // index�� ���� ���
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                }
            }
        }
    }
    else
    {
        //---------------------------------------------------
        // INDEX SCAN�� ���õ� ���
        //---------------------------------------------------

        // To Fix PR-9181
        // Index Scan�̶� �� ����
        // IN SUBQUERY KEY RANGE�� ���� ���
        // Order�� ������� �ʴ´�.
        if ( ( sMyGraph->selectedMethod->method->flag &
               QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK )
             == QMO_STAT_CARD_IDX_IN_SUBQUERY_TRUE )
        {
            sMyGraph->graph.preservedOrder = NULL;

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
        }
        else
        {
            IDE_TEST( makePreservedOrder( aStatement,
                                          sMyGraph->selectedMethod->method,
                                          sTableRef->table,
                                          & sMyGraph->graph.preservedOrder )
                      != IDE_SUCCESS );

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
        }
    }

    /* BUG-43006 ���� Index�� �ƴϰ� FixedTable�� Filter ��Ȱ��
     * �����ϱ� ������ ordering�� ����. ���� presevered order ��
     * ���ٰ� ����������Ѵ�.
     */
    /* BUG-43498 fixed table���� internal index ���� ������ �ʵ� */
    if ( ( sTableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
         ( sTableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
         ( sTableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) )
    {
        sMyGraph->graph.preservedOrder = NULL;
        sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
        sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------------------
    // ���� ��� ������ ����
    //---------------------------------------------------

    sMyGraph->graph.costInfo.selectivity = 
        sMyGraph->selectedMethod->methodSelectivity;

    // output record count ����
    sOutputRecordCnt = sMyGraph->graph.costInfo.selectivity *
        sMyGraph->graph.costInfo.inputRecordCnt;

    sMyGraph->graph.costInfo.outputRecordCnt =
        ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    // myCost, totalCost ����
    sMyGraph->graph.costInfo.myAccessCost =
        sMyGraph->selectedMethod->accessCost;

    sMyGraph->graph.costInfo.myDiskCost =
        sMyGraph->selectedMethod->diskCost;

    sMyGraph->graph.costInfo.myAllCost =
        sMyGraph->selectedMethod->totalCost;

    //---------------------------------------
    // �� ��� ���� ����
    //---------------------------------------

    // 0 ( Child�� Total Cost) + My Cost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->selectedMethod->accessCost;

    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->selectedMethod->diskCost;

    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->selectedMethod->totalCost;

    if ( sMyGraph->graph.left != NULL )
    {
        // Child�� �����ϴ� ���

        if ( ( (sMyGraph->graph.left->flag & QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK)
               == QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE ) ||
             ( (sMyGraph->graph.left->flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK)
               == QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE ) )
        {
            // View Materialization �� �����
            // Child�� Cost�� �������� �ʴ´�.
        }
        else
        {
            // Child�� ��������� �����Ѵ�.
            sMyGraph->graph.costInfo.totalAccessCost +=
                sMyGraph->graph.left->costInfo.totalAccessCost;

            sMyGraph->graph.costInfo.totalDiskCost +=
                sMyGraph->graph.left->costInfo.totalDiskCost;

            sMyGraph->graph.costInfo.totalAllCost +=
                sMyGraph->graph.left->costInfo.totalAllCost;
        }
    }
    else
    {
        // ������ Selection �׷�����.
        // ������ ������ ����
    }

    //---------------------------------------------------
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // host variable�� ���� ����ȭ�� ���� �غ����
    //---------------------------------------------------
    if ((sSelectedScanHint == QMG_NOT_USED_SCAN_HINT) &&
        (sMyGraph->accessMethodCnt > 1))
    {
        /*
         * BUG-38863 result wrong: host variable
         * table scan parallel or vertical parallel ������
         * host ���� ����ȭ�� �������� �ʴ´�.
         */
        if ((sTableRef->mParallelDegree == 1) ||
            ((aGraph->flag & QMG_PARALLEL_IMPOSSIBLE_MASK) ==
             QMG_PARALLEL_IMPOSSIBLE_TRUE))
        {
            if ( (QCU_HOST_OPTIMIZE_ENABLE == 1) &&
                 (QCU_PLAN_RANDOM_SEED == 0 ) )
            {
                IDE_TEST( prepareScanDecisionFactor( aStatement,
                                                     sMyGraph )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            // environment�� ���
            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_HOST_OPTIMIZE_ENABLE );
        }
        else
        {
            /* nothing to do */
        }
    }
    else
    {
        // scan hint�� ���Ǹ� ȣ��Ʈ ������ ���� ����ȭ�� �������� �ʴ´�.
        // index�� ���� ��쿡�� ȣ��Ʈ ������ ���� ����ȭ�� �������� �ʴ´�.
        // rid predicate �� �������� index scan ���� �����Ƿ� �������� �ʴ´�.
        // table scan parallel �Ǵ� vertical parallel ���� �������� �ʴ´�.
        // Nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ------------------------------------------------------------------
 * PROJ-2402 Parallel Table Scan
 * ------------------------------------------------------------------
 * �Ϲ� table �� parallel �� ���� ������ �����ϴ� flag
 *
 * 1. parallel degree > 1
 * 2. select for update �� ��� �Ұ���
 * 3. index scan �� �ƴϾ�� ��
 * 4. subquery filter �� ����� ��
 * 5. predicate �� outer column �� ����� ��
 * ------------------------------------------------------------------
 */
void qmgSelection::setParallelScanFlag(qcStatement* aStatement,
                                       qmgGraph   * aGraph)
{
    qmgSELT     * sMyGraph;
    qmoPredicate* sPredicate;
    qmoPredicate* sNextIter;
    qmoPredicate* sMoreIter;
    idBool        sIsParallel;

    sMyGraph   = (qmgSELT*)aGraph;
    sPredicate = sMyGraph->graph.myPredicate;

    sIsParallel = ID_TRUE;

    if ( aGraph->myFrom->tableRef->mParallelDegree > 1 )
    {
        if ( aGraph->myFrom->tableRef->tableAccessHints != NULL )
        {
            /* BUG-43757 Partial Full Scan Hint ���� Parallel Plan
             * �� ������ �ʾƾ��Ѵ�
             */
            if ( aGraph->myFrom->tableRef->tableAccessHints->count > 1 )
            {
                aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                sIsParallel = ID_FALSE;
                IDE_CONT(LABEL_EXIT);
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
        aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
        aGraph->flag |= QMG_PARALLEL_EXEC_TRUE;
    }
    else
    {
        aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
        aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
        sIsParallel = ID_FALSE;
        IDE_CONT(LABEL_EXIT);
    }

    // BUG-42317
    // ���� qmgGraph �� parallel ���࿡ ������ ��ġ��
    // QMG_PARALLEL_EXEC_MASK �� �������� �˻縦 �켱 check �ؾ� �Ѵ�.
    if ( sPredicate != NULL )
    {
        for (sNextIter = sPredicate;
             sNextIter != NULL;
             sNextIter = sNextIter->next)
        {
            for (sMoreIter = sNextIter;
                 sMoreIter != NULL;
                 sMoreIter = sMoreIter->more)
            {
                if ((sMoreIter->node->lflag & QTC_NODE_SUBQUERY_MASK) ==
                    QTC_NODE_SUBQUERY_EXIST)
                {
                    /* ���� graph �� parallel �� �� ���� */
                    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                    aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                    sIsParallel = ID_FALSE;
                    IDE_CONT(LABEL_EXIT);
                }
                else
                {
                    /* nothing to do */
                }

                /* BUG-39769
                   node�� function�� ���, parallel scan�ϸ� �� �ȴ�. */
                if ( (sMoreIter->node->lflag & QTC_NODE_PROC_FUNCTION_MASK) ==
                     QTC_NODE_PROC_FUNCTION_TRUE )
                {
                    /* ���� graph �� parallel �� �� ���� */
                    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                    aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                    sIsParallel = ID_FALSE;
                    IDE_CONT(LABEL_EXIT);
                }
                else
                {
                    /* Nothing to do */
                }

                /* BUG-39770
                   package�� ������ ���, parallel scan�ϸ� �� �ȴ�. */
                if ( (sMoreIter->node->lflag & QTC_NODE_PKG_MEMBER_MASK) ==
                     QTC_NODE_PKG_MEMBER_EXIST )
                {
                    /* ���� graph �� parallel �� �� ���� */
                    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                    aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                    sIsParallel = ID_FALSE;
                    IDE_CONT(LABEL_EXIT);
                }
                else
                {
                    /* Nothing to do. */
                }

                /* BUG-41932
                   lob�� ������ ���, parallel scan�ϸ� �� �ȴ�. */
                if ( (sMoreIter->node->lflag & QTC_NODE_LOB_COLUMN_MASK) ==
                     QTC_NODE_LOB_COLUMN_EXIST )
                {
                    /* ���� graph �� parallel �� �� ���� */
                    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                    aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                    sIsParallel = ID_FALSE;
                    IDE_CONT(LABEL_EXIT);
                }
                else
                {
                    /* Nothing to do. */
                }

                /*
                 * BUG-38763
                 * push_pred �� ���� t1.i1 = t2.i1 �� ����
                 * predicate �� ���� ���
                 * t1.i1 �� t2.i1 �� ���� �ٸ� template �� ������
                 * filter �� ����� �������� �ʴ´�.
                 * �̸� �����ϱ����� parallel plan �� �������� �ʴ´�.
                 */
                if (qtc::dependencyContains(&aGraph->depInfo,
                                            &sMoreIter->node->depInfo) == ID_FALSE)
                {
                    aGraph->flag &= ~QMG_PARALLEL_EXEC_MASK;
                    aGraph->flag |= QMG_PARALLEL_EXEC_FALSE;
                    sIsParallel = ID_FALSE;
                    IDE_CONT(LABEL_EXIT);
                }
                else
                {
                    /* nothing to do */
                } 
            }
        }
    }
    else
    {
        /* nothing to do */
    }

    // BUG-39567 Lateral View ���ο����� Parallel Scan�� �����Ѵ�.
    if ( qtc::haveDependencies( & sMyGraph->graph.myQuerySet->lateralDepInfo )
         == ID_TRUE )
    {
        sIsParallel = ID_FALSE;
        IDE_CONT(LABEL_EXIT);
    }
    else
    {
        /* nothing to do */
    }

    if (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT_FOR_UPDATE)
    {
        /*
         * BUG-38803
         * select for update ������ SCAN->init ���� cursor open �ϹǷ�
         * parallel �Ұ���
         */
        sIsParallel = ID_FALSE;
        IDE_CONT(LABEL_EXIT);
    }
    else
    {
        /* nothing to do */
    }

    if ( sMyGraph->graph.ridPredicate != NULL )
    {
        // BUG-43756 _prowid ���� parallel �÷��� �������� �ʽ��ϴ�.
        sIsParallel = ID_FALSE;
        IDE_CONT(LABEL_EXIT);
    }
    else
    {
        /* nothing to do */
    }

    if ( aGraph->myFrom->tableRef->remoteTable != NULL )
    {
        // BUG-43819 remoteTable ���� parallel �÷��� �������� �ʽ��ϴ�.
        sIsParallel = ID_FALSE;
        IDE_CONT(LABEL_EXIT);
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT(LABEL_EXIT);

    if (sIsParallel == ID_TRUE)
    {
        sMyGraph->mFlag &= ~QMG_SELT_PARALLEL_SCAN_MASK;
        sMyGraph->mFlag |= QMG_SELT_PARALLEL_SCAN_TRUE;
    }
    else
    {
        sMyGraph->mFlag &= ~QMG_SELT_PARALLEL_SCAN_MASK;
        sMyGraph->mFlag |= QMG_SELT_PARALLEL_SCAN_FALSE;
    }

}

IDE_RC
qmgSelection::prepareScanDecisionFactor( qcStatement * aStatement,
                                         qmgSELT     * aGraph )
{
/***********************************************************************
 *
 * Description : ȣ��Ʈ ������ �ִ� ���ǹ��� ����ȭ�ϱ� ����
 *               qmoScanDecisionFactor �ڷ� ������ �����Ѵ�.
 *               predicate �߿��� ȣ��Ʈ ������ �����ϸ�,
 *               qmoScanDecisionFactor�� �ڷḦ �����Ѵ�.
 *               �� �ڷ�� data�� ���ε��� ��, selectivity�� �ٽ�
 *               ����ϰ� access method�� �缳���� �� ���ȴ�.
 *               �ϳ��� selection graph�� ����
 *               �ϳ��� qmoScanDecisionFactor�� �����ǰų� Ȥ��
 *               �������� �ʴ´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoScanDecisionFactor * sSDF;

    IDU_FIT_POINT_FATAL( "qmgSelection::prepareScanDecisionFactor::__FT__" );

    if( qmoPred::checkPredicateForHostOpt( aGraph->graph.myPredicate )
        == ID_TRUE )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoScanDecisionFactor ),
                                                 (void **)&sSDF )
                  != IDE_SUCCESS );

        sSDF->next = NULL;
        sSDF->baseGraph = aGraph;
        sSDF->basePlan = NULL;
        sSDF->candidateCount = 0;

        // sSDF->predicate ����
        IDE_TEST( qmoPred::deepCopyPredicate( QC_QMP_MEM(aStatement),
                                              aGraph->graph.myPredicate,
                                              &sSDF->predicate )
                  != IDE_SUCCESS );

        // sSDF->predicate->mySelectivityOffset, totalSelectivityOffset ����
        IDE_TEST( setSelectivityOffset( aStatement,
                                        sSDF->predicate )
                  != IDE_SUCCESS );

        // sSDF->accessMethodsOffset ����
        IDE_TEST( qtc::getDataOffset( aStatement,
                                      ID_SIZEOF(qmoAccessMethod)
                                      * aGraph->accessMethodCnt,
                                      &sSDF->accessMethodsOffset )
                  != IDE_SUCCESS );

        // sSDF->selectedMethodOffset ����
        IDE_TEST( qtc::getDataOffset( aStatement,
                                      ID_SIZEOF(UInt),
                                      &sSDF->selectedMethodOffset )
                  != IDE_SUCCESS );

        // sSDF ����
        IDE_TEST( qtc::addSDF( aStatement, sSDF ) != IDE_SUCCESS );

        aGraph->sdf = sSDF;
    }
    else
    {
        // Nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::setSelectivityOffset( qcStatement  * aStatement,
                                    qmoPredicate * aPredicate )
{
/********************************************************************
 *
 * Description : predicate�� mySelectivity�� totalSelectivity��
 *               �����Ѵ�.
 *
 *               �� �Լ��� checkPredicateForHostOpt() �Լ��� ȣ��� �Ŀ�
 *               �ҷ��� �Ѵ�.
 *
 * Implementation :
 *
 *               ��� predicate�鿡 ���� host ����ȭ�� �����ϸ�
 *               mySelectivityOffset�� data ��ġ�� �����Ѵ�.
 *               �׷��� ������ QMO_SELECTIVITY_OFFSET_NOT_USED��
 *               �����Ѵ�.
 *
 *               more list�� ù��° predicate�� ����
 *               host ����ȭ�� �����ϸ� totalSelectivity��
 *               data ��ġ�� �����ϰ� �׷��� ������
 *               QMO_SELECTIVITY_OFFSET_NOT_USED�� �����Ѵ�.
 *
 ********************************************************************/

    qmoPredicate * sNextIter;
    qmoPredicate * sMoreIter;

    IDU_FIT_POINT_FATAL( "qmgSelection::setSelectivityOffset::__FT__" );

    for( sNextIter = aPredicate;
         sNextIter != NULL;
         sNextIter = sNextIter->next )
    {
        IDE_TEST( setMySelectivityOffset( aStatement, sNextIter )
                  != IDE_SUCCESS );

        IDE_TEST( setTotalSelectivityOffset( aStatement, sNextIter )
                  != IDE_SUCCESS );

        for( sMoreIter = sNextIter->more;
             sMoreIter != NULL;
             sMoreIter = sMoreIter->more )
        {
            IDE_TEST( setMySelectivityOffset( aStatement, sMoreIter )
                      != IDE_SUCCESS );

            sMoreIter->totalSelectivityOffset =
                QMO_SELECTIVITY_OFFSET_NOT_USED;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::setMySelectivityOffset( qcStatement  * aStatement,
                                      qmoPredicate * aPredicate )
{

    IDU_FIT_POINT_FATAL( "qmgSelection::setMySelectivityOffset::__FT__" );

    if( ( aPredicate->flag & QMO_PRED_HOST_OPTIMIZE_MASK )
        == QMO_PRED_HOST_OPTIMIZE_TRUE )
    {
        IDE_TEST( qtc::getDataOffset( aStatement,
                                      ID_SIZEOF(aPredicate->mySelectivity),
                                      &aPredicate->mySelectivityOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        aPredicate->mySelectivityOffset = QMO_SELECTIVITY_OFFSET_NOT_USED;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::setTotalSelectivityOffset( qcStatement  * aStatement,
                                         qmoPredicate * aPredicate )
{

    IDU_FIT_POINT_FATAL( "qmgSelection::setTotalSelectivityOffset::__FT__" );

    if( ( aPredicate->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
        == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE )
    {
        IDE_TEST( qtc::getDataOffset( aStatement,
                                      ID_SIZEOF(aPredicate->totalSelectivity),
                                      &aPredicate->totalSelectivityOffset )
                  != IDE_SUCCESS );
    }
    else
    {
        aPredicate->totalSelectivityOffset = QMO_SELECTIVITY_OFFSET_NOT_USED;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSelection�� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgSelection���� ���� ���������� plan
 *
 *        1. Base Table�� ���� Selection Graph�� ���
 *
 *           - ��� Predicate ������ [SCAN]��忡 ���Եȴ�.
 *
 *                 [SCAN]
 *
 *        2. View�� ���� Selection Graph�� ���
 *
 *           - �� ���� qmgSelection�� Child�� �ݵ�� qmgProjection�̴�.
 *           - Predicate�� ���� ������ ���� [FILT] ��尡 �����ȴ�.
 *
 *           2.1 Child Graph qmgProjection�� View Materialization�� ���
 *
 *               ( [FILT] )
 *                   |
 *                 [VSCN]
 *
 *           2.2 Child Graph qmgProjection�� View Materialization�� �ƴ� ���
 *
 *               ( [FILT] )
 *                   |
 *                 [VIEW]
 *
 *        3. recursive View�� ���� Selection Graph�� ���
 *
 *           - �� ���� qmgSelection�� Child�� �ݵ�� qmgProjection�̴�.
 *
 *                 [VSCN]
 *
 *
 ***********************************************************************/

    qmgSELT         * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSelection::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph       = (qmgSELT *) aGraph;

    //---------------------------------------------------
    // Current CNF�� ���
    //---------------------------------------------------

    if( sMyGraph->graph.myCNF != NULL )
    {
        sMyGraph->graph.myQuerySet->SFWGH->crtPath->currentCNF =
            sMyGraph->graph.myCNF;
    }
    else
    {
        // Nothing To Do
    }

    // PROJ-2179
    // UPDATE ������ ��� parent�� NULL�� �� �ִ�.
    if( aParent != NULL )
    {
        sMyGraph->graph.myPlan = aParent->myPlan;

        /* PROJ-1071 Parallel Query */
        aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
        aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);
    }
    else
    {
        sMyGraph->graph.myPlan = NULL;

        /* PROJ-1071 Parallel Query */
        aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
        aGraph->flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;
    }

    if( (sMyGraph->graph.left == NULL) )
    {
        /*
         * 1. DML �� �ƴϾ�� ��
         * 2. keyrange �� ����� ��
         * 3. subquery �ƴϾ�� ��
         * 4. parallel table scan �� ������ graph
         * 5. �ݺ�����Ǵ� join �������� �ƴϾ�� ��
         */
        if (((aGraph->flag & QMG_PARALLEL_IMPOSSIBLE_MASK) ==
             QMG_PARALLEL_IMPOSSIBLE_FALSE) &&
            ((aGraph->flag & QMG_SELT_NOTNULL_KEYRANGE_MASK) ==
             QMG_SELT_NOTNULL_KEYRANGE_FALSE) &&
            (aGraph->myQuerySet->parentTupleID == ID_USHORT_MAX) &&
            ((sMyGraph->mFlag & QMG_SELT_PARALLEL_SCAN_MASK) ==
             QMG_SELT_PARALLEL_SCAN_TRUE) &&
            ((aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK) ==
             QMG_PLAN_EXEC_REPEATED_FALSE))
        {
            IDE_TEST(makeParallelScan(aStatement, sMyGraph) != IDE_SUCCESS);
        }
        else
        {
            // PROJ-2582 recursive with
            if( ( sMyGraph->graph.myFrom->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
            {
                IDE_TEST( makeRecursiveViewScan( aStatement,
                                                 sMyGraph )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( makeTableScan( aStatement,
                                         sMyGraph )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        IDE_TEST( makeViewScan( aStatement,
                                sMyGraph )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::makeTableScan( qcStatement * aStatement,
                             qmgSELT     * aMyGraph )
{
    qmsTableRef     * sTableRef;
    qmoSCANInfo       sSCANInfo;
    qtcNode         * sLobFilter; // BUG-25916
    qmnPlan         * sFILT;
    qmnPlan         * sSCAN;

    IDU_FIT_POINT_FATAL( "qmgSelection::makeTableScan::__FT__" );

    sSCANInfo.flag = QMO_SCAN_INFO_INITIALIZE;

    //-----------------------------------------------------
    //        [*FILT]
    //           |
    //        [SCAN]
    // * Option
    //-----------------------------------------------------

    //-----------------------------------------------------
    // 1. Base Table�� ���� Selection Graph�� ���
    //-----------------------------------------------------

    // ���� ��尡 ���� leaf�� base�� ��� SCAN�� �����Ѵ�.

    // To Fix PR-11562
    // Indexable MIN-MAX ����ȭ�� ����� ���
    // Preserved Order�� ���⼺�� ����, ���� �ش� ������
    // �������� �ʿ䰡 ����.
    // INDEXABLE Min-Max�� ����
    // ���� �ڵ� ����

    //-----------------------------------------------------
    // To Fix BUG-8747
    // Selection Graph�� Not Null Key Range�� �����϶�� Flag��
    // �ִ� ���, Leaf Info�� �� ������ �����Ѵ�.
    // - Selection Graph���� Not Null Key Range ���� Flag ����Ǵ� ����
    //   (1) indexable Min Max�� ����� Selection Graph
    //   (2) Merge Join ������ Selection Graph
    //-----------------------------------------------------

    if( (aMyGraph->graph.flag & QMG_SELT_NOTNULL_KEYRANGE_MASK ) ==
        QMG_SELT_NOTNULL_KEYRANGE_TRUE )
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_NOTNULL_KEYRANGE_TRUE;
    }
    else
    {
        // To Fix PR-10288
        // NOTNULL KEY RANGE�� �ƴ� ���� �ݵ�� ������ �־�� ��.
        sSCANInfo.flag &= ~QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_NOTNULL_KEYRANGE_FALSE;
    }

    if (aMyGraph->forceIndexScan == ID_TRUE)
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_INDEX_SCAN_TRUE;
    }
    else
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_INDEX_SCAN_FALSE;
    }

    sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_RID_SCAN_MASK;
    sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_RID_SCAN_FALSE;

    sSCANInfo.predicate         = aMyGraph->graph.myPredicate;
    sSCANInfo.constantPredicate = aMyGraph->graph.constantPredicate;
    sSCANInfo.ridPredicate      = aMyGraph->graph.ridPredicate;
    sSCANInfo.limit             = aMyGraph->limit;
    sSCANInfo.index             = aMyGraph->selectedIndex;
    sSCANInfo.preservedOrder    = aMyGraph->graph.preservedOrder;
    sSCANInfo.sdf               = aMyGraph->sdf;
    sSCANInfo.nnfFilter         = aMyGraph->graph.nnfFilter;

    /* BUG-39306 partial scan */
    sTableRef = aMyGraph->graph.myFrom->tableRef;
    if ( sTableRef->tableAccessHints != NULL )
    {
        sSCANInfo.mParallelInfo.mDegree = sTableRef->tableAccessHints->count;
        sSCANInfo.mParallelInfo.mSeqNo  = sTableRef->tableAccessHints->id;
    }
    else
    {
        sSCANInfo.mParallelInfo.mDegree = 1;
        sSCANInfo.mParallelInfo.mSeqNo  = 1;
    }

    //SCAN����
    //������ ����� ��ġ�� �ݵ�� graph.myPlan�� ������ �ϵ��� �Ѵ�.
    //�� ������ �ٽ� child�� ��� ���� ��� ���鶧 �����ϵ��� �Ѵ�.
    IDE_TEST( qmoOneNonPlan::makeSCAN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myFrom ,
                                       &sSCANInfo,
                                       aMyGraph->graph.myPlan,
                                       &sSCAN )
              != IDE_SUCCESS);

    qmg::setPlanInfo( aStatement, sSCAN, &(aMyGraph->graph) );

    // BUG-25916 : clob�� select for update �ϴ� ���� assert �߻�
    // clob locator�� �������� lobfilter�� �з��� ���� �����ϸ�
    // SCAN ��� ������ FILTER ���� ó���ؾ� �Ѵ�.
    sLobFilter = ((qmncSCAN*)sSCAN)->method.lobFilter;

    if ( sLobFilter != NULL )
    {
        // BUGBUG
        // Lob filter�� ��� SCAN�� ������ �Ŀ� ������ �� �� �ִ�.
        // ���� FILT�� ���� ���ε� SCAN�� ���� �Ŀ� �����ȴ�.
        // BUG-25916�� ���� �ذ� ����� �����ؾ� �Ѵ�.
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sLobFilter ,
                      aMyGraph->graph.myPlan ,
                      &sFILT) != IDE_SUCCESS);

        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sLobFilter ,
                      NULL,
                      sSCAN ,
                      sFILT) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo( aStatement, sFILT, &(aMyGraph->graph) );
    }
    else
    {
        aMyGraph->graph.myPlan = sSCAN;
    }

    // fix BUG-13482
    // SCAN ��� ������,
    // filter��������� SCAN Limit ����ȭ�� �������� ���� ���,
    // selection graph�� limit�� NULL�� �����Ѵ�.
    // �̴� ���� PROJ ��� ������, limit start value ������ ������ ��.
    if( sSCANInfo.limit == NULL )
    {
        aMyGraph->limit = NULL;
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
qmgSelection::makeViewScan( qcStatement * aStatement,
                            qmgSELT     * aMyGraph )
{
    UInt              sFlag   = 0;
    qtcNode         * sFilter = NULL;
    qtcNode         * sTmp    = NULL;
    qmnPlan         * sFILT   = NULL;
    qmnPlan         * sVSCN   = NULL;
    qmnPlan         * sVIEW   = NULL;

    IDU_FIT_POINT_FATAL( "qmgSelection::makeViewScan::__FT__" );

    //-----------------------------------------------------
    // 1. Non-materialized view
    // 2. Materialized view
    // 
    //     1. [*FILT]        2. [*FILT]
    //           |       OR        |
    //        [VIEW]            [VSCN]
    //           |                 |
    //        [LEFT]            [LEFT]
    // * Option
    //-----------------------------------------------------


    //---------------------------------------
    // predicate�� ���� ������ ����
    // FILT ����
    //---------------------------------------

    if( ( aMyGraph->graph.myPredicate != NULL ) ||
        ( aMyGraph->graph.constantPredicate != NULL ) ||
        ( aMyGraph->graph.nnfFilter != NULL ) )
    {
        if( aMyGraph->graph.myPredicate != NULL )
        {
            IDE_TEST( qmoPred::linkFilterPredicate(
                          aStatement ,
                          aMyGraph->graph.myPredicate ,
                          &sFilter ) != IDE_SUCCESS );

            // BUG-35155 Partial CNF
            if ( aMyGraph->graph.nnfFilter != NULL )
            {
                IDE_TEST( qmoPred::addNNFFilter4linkedFilter(
                              aStatement,
                              aMyGraph->graph.nnfFilter,
                              & sFilter ) != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if ( aMyGraph->graph.nnfFilter != NULL )
            {
                sFilter = aMyGraph->graph.nnfFilter;
            }
            else
            {
                sFilter = NULL;
            }
        }

        if ( sFilter != NULL )
        {
            IDE_TEST( qmoPred::setPriorNodeID(
                          aStatement ,
                          aMyGraph->graph.myQuerySet ,
                          sFilter ) != IDE_SUCCESS );

            /* BUG-47752 left outer join�� on���� OR �������� �����ǰ��ְ�
             * ������ view�� aggregaion�� �����Ұ�� fatal
             */
            if ( ( ( aMyGraph->graph.flag & QMG_JOIN_RIGHT_MASK )
                   == QMG_JOIN_RIGHT_TRUE ) &&
                 ( ( aMyGraph->graph.flag & QMG_VIEW_PUSH_MASK )
                   == QMG_VIEW_PUSH_TRUE ) )
            {
                sTmp = sFilter;

                IDE_TEST( qtc::copyNodeTree( aStatement, 
                                             sTmp,
                                             &sFilter,
                                             ID_FALSE,
                                             ID_TRUE )
                           != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            // Nothing To Do
        }

        //make FILT
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFilter ,
                      aMyGraph->graph.myPlan ,
                      &sFILT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;
    }
    else
    {
        // Nothing To Do
    }

    if( (aMyGraph->graph.left->flag &
         QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK)
        == QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE )
    {
        IDE_TEST( qmoOneNonPlan::initVSCN( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           aMyGraph->graph.myFrom ,
                                           aMyGraph->graph.myPlan ,
                                           &sVSCN ) != IDE_SUCCESS );
        aMyGraph->graph.myPlan = sVSCN;
    }
    else
    {
        IDE_TEST( qmoOneNonPlan::initVIEW( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           aMyGraph->graph.myPlan ,
                                           &sVIEW ) != IDE_SUCCESS );
        aMyGraph->graph.myPlan = sVIEW;
    }

    //-----------------------------------------------------
    // 2. View�� ���� Selection Graph�� ���
    //-----------------------------------------------------

    IDE_DASSERT( aMyGraph->graph.left->type == QMG_PROJECTION );

    //----------------------
    // ���� Plan�� ����
    //----------------------

    // To Fix BUG-8241
    if( aMyGraph->graph.left->myPlan == NULL )
    {
        // To Fix PR-8470
        // View�� ���� Plan Tree ���� �ÿ��� View�� Statement��
        // ����Ͽ��� ��
        IDE_TEST( aMyGraph->graph.left->makePlan(
                      aMyGraph->graph.myFrom->tableRef->view,
                      &aMyGraph->graph,
                      aMyGraph->graph.left )
                  != IDE_SUCCESS);
    }
    else
    {
        // nothing to do
    }

    if( (aMyGraph->graph.left->flag &
         QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK)
        == QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE )
    {
        //---------------------------------------
        // VSCN����
        //     - Materialization�� �ִ� ���
        //---------------------------------------

        IDE_TEST( qmoOneNonPlan::makeVSCN( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           aMyGraph->graph.myFrom ,
                                           aMyGraph->graph.left->myPlan ,
                                           sVSCN ) != IDE_SUCCESS );
        aMyGraph->graph.myPlan = sVSCN;

        qmg::setPlanInfo( aStatement, sVSCN, &(aMyGraph->graph) );
    }
    else
    {
        //---------------------------------------
        // VIEW ����
        //     - Materialization�� ���� ���
        //---------------------------------------

        sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
        sFlag |= QMO_MAKEVIEW_FROM_SELECTION;

        //VIEW����
        IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           aMyGraph->graph.myFrom ,
                                           sFlag ,
                                           aMyGraph->graph.left->myPlan ,
                                           sVIEW ) != IDE_SUCCESS );
        aMyGraph->graph.myPlan = sVIEW;

        qmg::setPlanInfo( aStatement, sVIEW, &(aMyGraph->graph) );
    }

    //---------------------------------------
    // predicate�� ���� ������ ����
    // FILT ����
    //---------------------------------------

    if( ( aMyGraph->graph.myPredicate != NULL ) ||
        ( aMyGraph->graph.constantPredicate != NULL ) ||
        ( aMyGraph->graph.nnfFilter != NULL ) )
    {
        //make FILT
        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFilter ,
                      aMyGraph->graph.constantPredicate ,
                      aMyGraph->graph.myPlan ,
                      sFILT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo( aStatement, sFILT, &(aMyGraph->graph) );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
  
/*
 * ------------------------------------------------------------------
 * PROJ-2402 Parallel Table Scan
 * ------------------------------------------------------------------
 *     [*FILT]
 *        |
 *     [PSCRD]
 *        |   
 *     [PRLQ]-[PRLQ]-[PRLQ]
 *       |      |      |
 *     [SCAN] [SCAN] [SCAN]
 * ------------------------------------------------------------------
 */
IDE_RC qmgSelection::makeParallelScan(qcStatement* aStatement,
                                      qmgSELT    * aMyGraph)
{
    qmnPlan     * sPSCRD        = NULL;
    qmnPlan     * sPRLQ         = NULL;
    qmnPlan     * sSCAN         = NULL;
    qmnPlan     * sFILT         = NULL;
    qmnPlan     * sParentPlan   = NULL;
    qmnChildren * sChildrenPRLQ = NULL;
    qmnChildren * sLastChildren = NULL;
    qtcNode     * sLobFilter    = NULL;
    qmoSCANInfo   sSCANInfo;
    qmoPSCRDInfo  sPSCRDInfo;
    UInt          sDegree;
    UInt          sPRLQCount;
    UInt          i;

    IDU_FIT_POINT_FATAL( "qmgSelection::makeParallelScan::__FT__" );

    sDegree = aMyGraph->graph.myFrom->tableRef->mParallelDegree;
    IDE_DASSERT(sDegree > 1);

    sParentPlan = aMyGraph->graph.myPlan;

    sPSCRD = NULL;
    IDE_TEST(qmoParallelPlan::initPSCRD(aStatement,
                                        aMyGraph->graph.myQuerySet,
                                        sParentPlan,
                                        &sPSCRD)
             != IDE_SUCCESS);

    // �ʱ�ȭ
    sPRLQCount      = 0;
    sSCANInfo.limit = NULL;

    for (i = 0; i < sDegree; i++)
    {
        sPRLQ = NULL;

        IDE_TEST(qmoParallelPlan::initPRLQ(aStatement,
                                           sPSCRD,
                                           &sPRLQ)
                 != IDE_SUCCESS);
        sPRLQ->left = NULL;

        IDU_FIT_POINT("qmgSelection::makeParallelScan::alloc", 
                      idERR_ABORT_InsufficientMemory);
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmnChildren),
                                               (void**)&sChildrenPRLQ)
                 != IDE_SUCCESS);

        sChildrenPRLQ->childPlan = sPRLQ;
        sChildrenPRLQ->next = NULL;

        if (sLastChildren == NULL)
        {
            sPSCRD->childrenPRLQ = sChildrenPRLQ;
        }
        else
        {
            sLastChildren->next = sChildrenPRLQ;
        }
        sLastChildren = sChildrenPRLQ;
        sPRLQCount++;

        /*
         * ----------------------------------------------------------
         * make PRLQ
         * ----------------------------------------------------------
         */

        IDE_DASSERT((aMyGraph->graph.flag & QMG_SELT_NOTNULL_KEYRANGE_MASK) ==
                    QMG_SELT_NOTNULL_KEYRANGE_FALSE);
        IDE_DASSERT(aMyGraph->selectedIndex == NULL);
        IDE_DASSERT(aMyGraph->forceIndexScan == ID_FALSE);

        sSCANInfo.flag = QMO_SCAN_INFO_INITIALIZE;
        sSCANInfo.flag &= ~QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_NOTNULL_KEYRANGE_FALSE;

        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_INDEX_SCAN_FALSE;

        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_RID_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_RID_SCAN_FALSE;

        IDE_TEST(qmoPred::deepCopyPredicate(QC_QMP_MEM(aStatement),
                                            aMyGraph->graph.myPredicate,
                                            &sSCANInfo.predicate)
                 != IDE_SUCCESS);

        IDE_TEST(qmoPred::deepCopyPredicate(QC_QMP_MEM(aStatement),
                                            aMyGraph->graph.ridPredicate,
                                            &sSCANInfo.ridPredicate)
                 != IDE_SUCCESS);

        /*
         * BUG-38823
         * constant filter �� PSCRD �� ���´�
         */
        sSCANInfo.constantPredicate = NULL;

        sSCANInfo.limit                 = aMyGraph->limit;
        sSCANInfo.index                 = aMyGraph->selectedIndex;
        sSCANInfo.preservedOrder        = aMyGraph->graph.preservedOrder;
        sSCANInfo.sdf                   = aMyGraph->sdf;
        sSCANInfo.nnfFilter             = aMyGraph->graph.nnfFilter;
        sSCANInfo.mParallelInfo.mDegree = sDegree;
        sSCANInfo.mParallelInfo.mSeqNo  = i + 1;

        sSCAN = NULL;
        IDE_TEST(qmoOneNonPlan::makeSCAN(aStatement,
                                         aMyGraph->graph.myQuerySet,
                                         aMyGraph->graph.myFrom,
                                         &sSCANInfo,
                                         sPRLQ,
                                         &sSCAN)
                 != IDE_SUCCESS);
        qmg::setPlanInfo(aStatement, sSCAN, &aMyGraph->graph);

        /*
         * ----------------------------------------------------------
         * make PRLQ
         * ----------------------------------------------------------
         */
        IDE_TEST(qmoParallelPlan::makePRLQ(aStatement,
                                           aMyGraph->graph.myQuerySet,
                                           QMO_MAKEPRLQ_PARALLEL_TYPE_SCAN,
                                           sSCAN,
                                           sPRLQ)
                 != IDE_SUCCESS);
    }

    /*
     * ----------------------------------------------------------
     * make PSCRD 
     * ----------------------------------------------------------
     */
    sPSCRDInfo.mConstantPredicate = aMyGraph->graph.constantPredicate;

    IDE_TEST(qmoParallelPlan::makePSCRD(aStatement,
                                        aMyGraph->graph.myQuerySet,
                                        aMyGraph->graph.myFrom,
                                        &sPSCRDInfo,
                                        sPSCRD)
             != IDE_SUCCESS);
    qmg::setPlanInfo(aStatement, sPSCRD, &aMyGraph->graph);

    aMyGraph->graph.myPlan = sPSCRD;

    sLobFilter = ((qmncSCAN*)sSCAN)->method.lobFilter;
    if (sLobFilter != NULL)
    {
        sFILT = NULL;
        IDE_TEST(qmoOneNonPlan::initFILT(aStatement,
                                         aMyGraph->graph.myQuerySet,
                                         sLobFilter,
                                         sParentPlan,
                                         &sFILT)
                 != IDE_SUCCESS);

        IDE_TEST(qmoOneNonPlan::makeFILT(aStatement,
                                         aMyGraph->graph.myQuerySet,
                                         sLobFilter,
                                         NULL,
                                         sPSCRD,
                                         sFILT)
                 != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo(aStatement, sFILT, &aMyGraph->graph);
    }
    else
    {
        /* nothing to do */
    }

    if (sSCANInfo.limit == NULL)
    {
        aMyGraph->limit = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::alterSelectedIndex( qcStatement * aStatement,
                                  qmgSELT     * aGraph,
                                  qcmIndex    * aNewSelectedIndex )
{
    qcmIndex * sNewSelectedIndex = NULL;
    UInt       i;

    IDU_FIT_POINT_FATAL( "qmgSelection::alterSelectedIndex::__FT__" );

    IDE_DASSERT( aGraph != NULL );

    if( aNewSelectedIndex != NULL )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        // ���õ� index�� local index��� �� ��Ƽ�ǿ� �´�
        // local index partition�� ã�ƾ� �Ѵ�.
        if( ( aNewSelectedIndex->indexPartitionType ==
              QCM_LOCAL_PREFIXED_PARTITIONED_INDEX ) ||
            ( aNewSelectedIndex->indexPartitionType ==
              QCM_LOCAL_NONE_PREFIXED_PARTITIONED_INDEX ) )
        {
            IDE_DASSERT( aGraph->partitionRef != NULL );

            for( i = 0;
                 i < aGraph->partitionRef->partitionInfo->indexCount;
                 i++ )
            {
                if( aNewSelectedIndex->indexId ==
                    aGraph->partitionRef->partitionInfo->indices[i].indexId )
                {
                    sNewSelectedIndex = &aGraph->partitionRef->partitionInfo->indices[i];
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            IDE_FT_ASSERT( sNewSelectedIndex != NULL );
        }
        else
        {
            sNewSelectedIndex = aNewSelectedIndex;
        }
    }
    else
    {
        // Nothing to do.
    }

    aGraph->selectedIndex = sNewSelectedIndex;

    // To fix BUG-12742
    // ���� graph�� ���� ������ ���
    // index�� ���� �� ����.
    aGraph->forceIndexScan = ID_TRUE;
    
    if( aGraph->sdf != NULL )
    {
        // ���� selection graph�� selectedIndex��
        // ���� graph�� ���� �ٽ� ������ ���
        // host optimization�� �ؼ��� �ȵȴ�.
        // �� ��� sdf�� disable�Ѵ�.
        IDE_TEST( qtc::removeSDF( aStatement, aGraph->sdf ) != IDE_SUCCESS );

        aGraph->sdf = NULL;
    }
    else
    {
        // Nothing to do...
    }

    /* PROJ-2402 Parallel Table Scan */
    aGraph->mFlag &= ~QMG_SELT_PARALLEL_SCAN_MASK;
    aGraph->mFlag |= QMG_SELT_PARALLEL_SCAN_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::printGraph( qcStatement  * aStatement,
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

    qmoPredicate * sPredicate;
    qmoPredicate * sMorePred;

    qmgSELT * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSelection::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

    sMyGraph = (qmgSELT*)aGraph;

    //-----------------------------------
    // Graph�� ���� ���
    //-----------------------------------

    if (aDepth == 0)
    {
        QMG_PRINT_LINE_FEED(i, aDepth, aString);
        iduVarStringAppend(aString,
                           "----------------------------------------------------------");
    }
    else
    {
    }

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

    IDE_TEST( qmoStat::printStat( aGraph->myFrom,
                                  aDepth,
                                  aString )
              != IDE_SUCCESS );

    //-----------------------------------
    // Access method �� ���� ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "== Access Method Information ==" );

    for ( i = 0; i < sMyGraph->accessMethodCnt; i++ )
    {
        IDE_TEST( printAccessMethod( aStatement,
                                     &sMyGraph->accessMethod[i],
                                     aDepth,
                                     aString )
                  != IDE_SUCCESS );
    }

    //-----------------------------------
    // Subquery Graph ������ ���
    //-----------------------------------

    for ( sPredicate = aGraph->myPredicate;
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

    for ( sPredicate = aGraph->constantPredicate;
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

    if ( aGraph->myFrom->tableRef->view != NULL )
    {
        // View�� ���� �׷��� ���� ���
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "::VIEW STATEMENT GRAPH BEGIN" );

        // To Fix BUG-9586
        // view�� ���� graph ��½� qcStatement��
        // aGraph->myFrom->tableRef->view �� �Ǿ�� ��
        IDE_TEST( aGraph->left->printGraph( aGraph->myFrom->tableRef->view,
                                            aGraph->left,
                                            aDepth + 1,
                                            aString )
                  != IDE_SUCCESS );

        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "::VIEW STATEMENT GRAPH END" );

    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------
    // Graph�� ������ ���
    //-----------------------------------

    if (aDepth == 0)
    {
        QMG_PRINT_LINE_FEED(i, aDepth, aString);
        iduVarStringAppend(aString,
                           "----------------------------------------------------------\n\n");
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
qmgSelection::printAccessMethod( qcStatement     *,
                                 qmoAccessMethod * aMethod,
                                 ULong             aDepth,
                                 iduVarString    * aString )
{
    UInt    i;

    IDU_FIT_POINT_FATAL( "qmgSelection::printAccessMethod::__FT__" );

    if( aMethod->method == NULL )
    {
        // FULL SCAN
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "FULL SCAN" );
    }
    else
    {
        // INDEX SCAN
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "INDEX SCAN[" );
        iduVarStringAppend( aString,
                            aMethod->method->index->name );
        iduVarStringAppend( aString,
                            "]" );
    }

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "  ACCESS_COST : %"ID_PRINT_G_FMT,
                              aMethod->accessCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "  DISK_COST   : %"ID_PRINT_G_FMT,
                              aMethod->diskCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "  TOTAL_COST  : %"ID_PRINT_G_FMT,
                              aMethod->totalCost );

    return IDE_SUCCESS;
}

IDE_RC
qmgSelection::makeViewGraph( qcStatement * aStatement,
                             qmgGraph    * aGraph )
{
/***********************************************************************
 *
 * Description : View Graph�� ����
 *
 * Implementation :
 *    (1) ù��° View�̰ų�, ���� View�� �������� �ʴ� ���
 *        A. View ����ȭ Hint�� ���� ���
 *           i    Push Selection ����
 *           ii   View Graph ����
 *           iii. View Graph ����
 *           iv.  View ����ȭ type ����
 *           v.   View Materialization ���θ� ���� Projection Graph�� ����
 *        B. View ����ȭ Hint�� VMTR�� ���
 *           ii   View Graph ����
 *           iii. View Graph ����
 *           iv.  View ����ȭ type ���� :  view ����ȭ type�� Not Defined
 *                ( �ι�° view���� View Materialization���� ������ )
 *           v.   View Materialization ���θ� ���� Projection Graph�� ����
 *        C. View ����ȭ Hint�� Push Selection�� ���
 *          i    Push Selection ����
 *           ii   View Graph ����
 *           iii. View Graph ����
 *           iv.  View ����ȭ type ���� :
 *                ���� ������ ������� view ����ȭ type�� push selection
 *               ( view materialization���� �������� �ʵ��� �ϱ� ���� )
 *           v.   View Materialization ���θ� ���� Projection Graph�� ����
 *    (2) �ι�° �̻��� View�� ���
 *        A. ù��° View ����ȭ type�� �������� ���� ���,
 *           View ����ȭ type�� View Materialization���� ����
 *        B. ù��° View ����ȭ type�� ������ ���,
 *           ù��° View ����ȭ type�� ����
 *
 ***********************************************************************/

    qmsTableRef  * sTableRef;
    qmoPredicate * sPredicate;
    idBool         sIsPushed;

    IDU_FIT_POINT_FATAL( "qmgSelection::makeViewGraph::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sTableRef          = aGraph->myFrom->tableRef;
    sIsPushed          = ID_FALSE;

    //---------------------------------------------------
    // BUG-40355 rownum predicate to limit
    //---------------------------------------------------

    // BUG-45296 rownum Pred �� left outer �� ���������� ������ �ȵ˴ϴ�.
    if ( (aGraph->flag & QMG_ROWNUM_PUSHED_MASK) == QMG_ROWNUM_PUSHED_TRUE )
    {
        IDE_TEST ( qmoRownumPredToLimit::rownumPredToLimitTransform(
                    aStatement,
                    aGraph->myQuerySet,
                    sTableRef,
                    &(aGraph->myPredicate) )
                != IDE_SUCCESS );
    }
    else
    {
        // nothing to do.
    }

    // doRownumPredToLimit �Լ����� myPredicate ���� ����ɼ� �ִ�.
    sPredicate         = aGraph->myPredicate;

    //---------------------------------------------------
    // View�� index hint ���� PROJ-1495
    //---------------------------------------------------

    if( sTableRef->tableAccessHints != NULL )
    {
        IDE_TEST( setViewIndexHints( aStatement,
                                     sTableRef )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------
    // PROJ-1473 VIEW ���η��� push projection ����
    //---------------------------------------------------

    if( aGraph->myQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
    {
        IDE_TEST( doViewPushProjection(
                      aStatement,
                      sTableRef,
                      ((qmsParseTree*)(sTableRef->view->myPlan->parseTree))->querySet )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do 
    }

    //---------------------------------------------------
    // View ����ȭ Type ����
    //---------------------------------------------------

    if ( sTableRef->sameViewRef == NULL )
    {
        //---------------------------------------------------
        // ù��° View �̰ų�, ���� View�� �������� �ʴ� ���
        //---------------------------------------------------

        switch( sTableRef->viewOptType )
        {
            case QMO_VIEW_OPT_TYPE_NOT_DEFINED :

                //---------------------------------------------------
                // Hint �� ���� ���
                //---------------------------------------------------

                // push selection �������� �õ�
                IDE_TEST( doViewPushSelection( aStatement,
                                               sTableRef,
                                               sPredicate,
                                               aGraph,
                                               & sIsPushed )
                          != IDE_SUCCESS );

                // View Graph ����
                IDE_TEST( qmo::makeGraph( sTableRef->view ) != IDE_SUCCESS );

                // View Graph ����
                aGraph->left = sTableRef->view->myPlan->graph;

                // View ����ȭ type ����
                if ( sIsPushed == ID_TRUE )
                {
                    // push selection�� ������ ���
                    sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_PUSH;
                }
                else
                {
                    // push selection ������ ���
                    sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_NOT_DEFINED;
                }

                // View Materialization ���� ����
                aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE;
                break;
            case QMO_VIEW_OPT_TYPE_VMTR :

                //---------------------------------------------------
                // View NO_PUSH_SELECT_VIEW Hint�� �ִ� ���
                //    - ���� View : Not Defined �� ����
                //    - �� ���� : ���� View�� �����Ҷ�, Materialization ����
                //               ���� View�� ���� ���, Materialization ����
                //---------------------------------------------------

                // To Fix BUG-8400
                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    // View Graph ����
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );

                    // View ����ȭ type ����
                    sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_NOT_DEFINED;

                    // To Fix PR-11558
                    // ������ View�� �ֻ��� Graph�� ������Ѿ� ��.
                    aGraph->left = sTableRef->view->myPlan->graph;

                    // View Materialization ���� ����
                    aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                    aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE;
                }
                else
                {
                    // View Graph ����
                    aGraph->left = sTableRef->view->myPlan->graph;
                }

                break;
            case QMO_VIEW_OPT_TYPE_POTENTIAL_VMTR :

                //---------------------------------------------------
                // view�� ���ݱ��� �ѹ��� ����ؼ� VMTR�� �����ߴ� ���
                //---------------------------------------------------
                
                IDE_DASSERT( sTableRef->view->myPlan->graph != NULL );

                // View Graph ����
                aGraph->left = sTableRef->view->myPlan->graph;

                // View Materialization ���� ����
                aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;

                // View ����ȭ type ����
                sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_VMTR;

                break;
            case QMO_VIEW_OPT_TYPE_PUSH :

                //---------------------------------------------------
                // Push Selection Hint�� �ִ� ���
                //    - Push Selection ���� ���ο� ������� ViewOptType��
                //      Push Selection���� �����Ͽ� ���� View ������
                //      View Materialization ���� ���ϵ��� �Ѵ�.
                //---------------------------------------------------

                // push selection �������� �õ�
                IDE_TEST( doViewPushSelection( aStatement,
                                               sTableRef,
                                               sPredicate,
                                               aGraph,
                                               & sIsPushed )
                          != IDE_SUCCESS );

                // View Graph ����
                IDE_TEST( qmo::makeGraph( sTableRef->view ) != IDE_SUCCESS );

                // View Graph ����
                aGraph->left = sTableRef->view->myPlan->graph;

                // View ����ȭ type ����
                sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_PUSH;

                // View Materialization ���� ����
                aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE;
                break;
            case QMO_VIEW_OPT_TYPE_FORCE_VMTR :

                //---------------------------------------------------
                // View materialze Hint�� �ִ� ���
                //    Materialization ����
                //---------------------------------------------------

                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    // View Graph ����
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );

                    // View ����ȭ type ����
                    sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_FORCE_VMTR;

                    // ������ View�� �ֻ��� Graph�� ������Ѿ� ��.
                    aGraph->left = sTableRef->view->myPlan->graph;

                    // View Materialization ���� ����
                    aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                    aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;
                }
                else
                {
                    // View Graph ����
                    aGraph->left = sTableRef->view->myPlan->graph;
                }

                break;
            case QMO_VIEW_OPT_TYPE_CMTR :
                // To Fix BUG-8400
                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    // View Graph ����
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );

                    // To Fix PR-11558
                    // ������ View�� �ֻ��� Graph�� ������Ѿ� ��.
                    aGraph->left = sTableRef->view->myPlan->graph;

                    // View Materialization ���� ����
                    aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK;
                    aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE;
                }
                else
                {
                    // View Graph ����
                    aGraph->left = sTableRef->view->myPlan->graph;
                }
                break;
            default :
                IDE_DASSERT( 0 );
                break;

        }
    }
    else
    {
        //---------------------------------------------------
        // �� ��° �̻��� View�� ���
        //---------------------------------------------------

        switch ( sTableRef->sameViewRef->viewOptType )
        {
            case QMO_VIEW_OPT_TYPE_NOT_DEFINED:

                // PROJ-2582 recursive with
                // ù��° view�� Statement�� ����
                if ( ( sTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                     == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
                {
                    sTableRef->view = sTableRef->sameViewRef->recursiveView;
                }
                else
                {
                    // BUG-43659 view ����ȭ�� �������
                    IDE_TEST( mergeViewTargetFlag( sTableRef->sameViewRef->view,
                                                   sTableRef->view ) != IDE_SUCCESS );

                    sTableRef->view = sTableRef->sameViewRef->view;
                }

                // BUG-38022
                // sameViewRef�� ������ ������ �������� �ѹ��� ����ϴ� ����
                // VMTR�� �������� �ʵ��� �Ѵ�.
                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );

                    // Graph�� �����Ѵ�.
                    aGraph->left = sTableRef->view->myPlan->graph;

                    // View Materialization ���� ����
                    aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                    aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE;

                    // ù��° view�� ����ȭ type�� POTENTIAL_VMTR�� ����
                    sTableRef->sameViewRef->viewOptType = QMO_VIEW_OPT_TYPE_POTENTIAL_VMTR;
                    break;
                }
                else
                {
                    // Nothing to do.
                }

                // ������ �������� �ι� �̻� ����ϴ� ���
                // ���� view�� ù��° view�� ����ȭ type�� ��� VMTR�� ����
                sTableRef->sameViewRef->viewOptType = QMO_VIEW_OPT_TYPE_VMTR;
                /* fall through */
            case QMO_VIEW_OPT_TYPE_VMTR :
            case QMO_VIEW_OPT_TYPE_FORCE_VMTR :
            case QMO_VIEW_OPT_TYPE_POTENTIAL_VMTR :
                sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_VMTR;

                // PROJ-2582 recursive with
                // ù��° view�� Statement�� ����
                if ( ( sTableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK )
                     == QMS_TABLE_REF_RECURSIVE_VIEW_TRUE )
                {
                    sTableRef->view = sTableRef->sameViewRef->recursiveView;
                }
                else
                {
                    // BUG-43659 view ����ȭ�� �������
                    IDE_TEST( mergeViewTargetFlag( sTableRef->sameViewRef->view,
                                                   sTableRef->view ) != IDE_SUCCESS );

                    sTableRef->view = sTableRef->sameViewRef->view;
                }

                // To Fix BUG-8400
                // View Graph�� �������� ���� ���,
                // View Graph�� �����Ѵ�.
                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do
                }

                // Graph�� �����Ѵ�.
                aGraph->left = sTableRef->view->myPlan->graph;

                // ������ Projection Graph�� VMTR ������ ����
                aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;

                // To Fix PR-11562
                // View Materialization�� preserverd order�� ������ ASC ����
                // �����ϸ� �ȵ�.
                // ���� �ڵ� ����.
                break;

            case QMO_VIEW_OPT_TYPE_PUSH :
                // ù��° view�� ����ȭ type�� ������.
                sTableRef->viewOptType = QMO_VIEW_OPT_TYPE_PUSH;

                // push selection �������� �õ��� �����ϸ�
                // push selection ����

                IDE_TEST( doViewPushSelection( aStatement,
                                               sTableRef,
                                               sPredicate,
                                               aGraph,
                                               & sIsPushed )
                          != IDE_SUCCESS );

                // View Graph ����
                IDE_TEST( qmo::makeGraph( sTableRef->view ) != IDE_SUCCESS );

                // View Graph ����
                aGraph->left = sTableRef->view->myPlan->graph;

                // ������ Projection Graph�� VMTR ������ ����
                aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
                aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_FALSE;
                break;
            case QMO_VIEW_OPT_TYPE_CMTR :
                // BUG-37237 hierarchy query�� same view�� ó������ �ʴ´�.
                if ( sTableRef->view->myPlan->graph == NULL )
                {
                    // View Graph ����
                    IDE_TEST( qmo::makeGraph( sTableRef->view )
                              != IDE_SUCCESS );

                    aGraph->left = sTableRef->view->myPlan->graph;

                    // View Materialization ���� ����
                    aGraph->left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK;
                    aGraph->left->flag |= QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE;
                }
                else
                {
                    // View Graph ����
                    aGraph->left = sTableRef->view->myPlan->graph;
                }
                break;
            default:
                IDE_DASSERT(0);
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSelection::mergeViewTargetFlag( qcStatement * aDesView,
                                          qcStatement * aSrcView )
{
    qmsTarget  * sDesViewTarget;
    qmsTarget  * sSrcViewTarget;
    mtcColumn  * sDesColumn;
    mtcColumn  * sSrcColumn;

    sDesViewTarget = ((qmsParseTree*)(aDesView->myPlan->parseTree))->querySet->target;
    sSrcViewTarget = ((qmsParseTree*)(aSrcView->myPlan->parseTree))->querySet->target;

    for (  ;
           sSrcViewTarget != NULL;
           sDesViewTarget = sDesViewTarget->next,
               sSrcViewTarget = sSrcViewTarget->next )
    {
        sDesColumn = QTC_STMT_COLUMN( aDesView, sDesViewTarget->targetColumn );
        sSrcColumn = QTC_STMT_COLUMN( aSrcView, sSrcViewTarget->targetColumn );

        // makeSCAN ���� ���Ǵ� flag �� ������Ŵ
        sDesColumn->flag |= sSrcColumn->flag & (
            MTC_COLUMN_USE_COLUMN_MASK |
            MTC_COLUMN_USE_TARGET_MASK |
            MTC_COLUMN_USE_NOT_TARGET_MASK |
            MTC_COLUMN_VIEW_COLUMN_PUSH_MASK |
            MTC_COLUMN_OUTER_REFERENCE_MASK );
    }

    return IDE_SUCCESS;
}

IDE_RC
qmgSelection::setViewIndexHints( qcStatement         * aStatement,
                                 qmsTableRef         * aTableRef )
{
//----------------------------------------------------------------------
//
// Description : VIEW index hint�� VIEW�� �����ϴ� �ش� base table�� ������.
//
// Implementation : PROJ-1495
//
//
//   ��) create table t1( i1 integer, i2 integer );
//       create index t1_i1 on t1( i1 );
//
//       create table t2( i1 integer, i2 integer );
//       create index t2_i1 on t2( i1 );
//
//       create view v1 ( a1, a2 ) as
//       select * from t1
//       union all
//       select * from t2;
//
//       select /*+ index( v1, t1_i1, t2_i1 ) */
//              *
//       from v1;
//
//----------------------------------------------------------------------

    qmsTableAccessHints  * sAccessHint;
    qmsQuerySet          * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgSelection::setViewIndexHints::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableRef != NULL );
    IDE_DASSERT( aTableRef->tableAccessHints != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sQuerySet = ((qmsParseTree *)aTableRef->view->myPlan->parseTree)->querySet;

    //---------------------------------------------------
    // �� table Access hint�� VIEW ���� �ش� base table�� ������.
    //---------------------------------------------------

    for( sAccessHint = aTableRef->tableAccessHints;
         sAccessHint != NULL;
         sAccessHint = sAccessHint->next )
    {
        IDE_TEST( findQuerySet4ViewIndexHints( aStatement,
                                               sQuerySet,
                                               sAccessHint )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgSelection::findQuerySet4ViewIndexHints( qcStatement         * aStatement,
                                           qmsQuerySet         * aQuerySet,
                                           qmsTableAccessHints * aAccessHint )
{
/***********************************************************************
 *
 * Description : VIEW index hint�� VIEW�� �����ϴ� �ش� base table�� ������.
 *
 * Implementation : PROJ-1495
 *
 *  view ���� �ش� querySet�� ã�´�.
 *
 ***********************************************************************/

    qmsFrom     * sFrom;

    IDU_FIT_POINT_FATAL( "qmgSelection::findQuerySet4ViewIndexHints::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aAccessHint != NULL );

    //---------------------------------------------------
    // set�� �ƴ� querySet�� ã�� view index hint�� ����
    //---------------------------------------------------

    if( aQuerySet->setOp == QMS_NONE )
    {
        for( sFrom = aQuerySet->SFWGH->from;
             sFrom != NULL;
             sFrom = sFrom->next )
        {
            //----------------------------------
            // view index hint�� �ش� base table�� ���� ����
            //----------------------------------
            if( sFrom->joinType == QMS_NO_JOIN )
            {
                if( sFrom->tableRef->view == NULL )
                {
                    //---------------------------------------
                    // ���� ���̺��� base table�� index hint ����
                    //---------------------------------------

                    IDE_TEST( setViewIndexHintInBaseTable( aStatement,
                                                           sFrom,
                                                           aAccessHint )
                              != IDE_SUCCESS );
                }
                else
                {
                    // VIEW �� ����,
                    // index hint �������� �ƴ�.
                }
            }
            else
            {
                //-----------------------------------------
                // ���� ���̺��� base table�� �ƴϹǷ�,
                // left, right from�� ��ȸ�ϸ鼭
                // base table�� ã�Ƽ� index hint ���� ����
                //-----------------------------------------

                IDE_TEST(
                    findBaseTableNSetIndexHint( aStatement,
                                                sFrom,
                                                aAccessHint )
                    != IDE_SUCCESS );
            }
        }
    }
    else
    {
        if( aQuerySet->left != NULL )
        {
            IDE_TEST( findQuerySet4ViewIndexHints( aStatement,
                                                   aQuerySet->left,
                                                   aAccessHint )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        if( aQuerySet->right != NULL )
        {
            IDE_TEST( findQuerySet4ViewIndexHints( aStatement,
                                                   aQuerySet->right,
                                                   aAccessHint )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgSelection::findBaseTableNSetIndexHint( qcStatement         * aStatement,
                                          qmsFrom             * aFrom,
                                          qmsTableAccessHints * aAccessHint )
{
/***********************************************************************
 *
 * Description : VIEW�� ���� index hint ����
 *
 * Implementation : PROJ-1495
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgSelection::findBaseTableNSetIndexHint::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aAccessHint != NULL );

    //---------------------------------------------------
    // left From�� ���� ó��
    //---------------------------------------------------

    if( aFrom->left->joinType == QMS_NO_JOIN )
    {
        if( aFrom->left->tableRef->view == NULL )
        {
            IDE_TEST( setViewIndexHintInBaseTable( aStatement,
                                                   aFrom->left,
                                                   aAccessHint )
                      != IDE_SUCCESS );
        }
        else
        {
            // VIEW�� ����,
            // index hint �������� �ƴ�.
        }
    }
    else
    {
        IDE_TEST( findBaseTableNSetIndexHint( aStatement,
                                              aFrom->left,
                                              aAccessHint )
                  != IDE_SUCCESS );
    }

    //---------------------------------------------------
    // right From�� ���� ó��
    //---------------------------------------------------

    if( aFrom->right->joinType == QMS_NO_JOIN )
    {
        if( aFrom->right->tableRef->view == NULL )
        {
            IDE_TEST( setViewIndexHintInBaseTable( aStatement,
                                                   aFrom->right,
                                                   aAccessHint )
                      != IDE_SUCCESS );
        }
        else
        {
            // VIEW�� ����,
            // index hint �������� �ƴ�.
        }
    }
    else
    {
        IDE_TEST( findBaseTableNSetIndexHint( aStatement,
                                              aFrom->right,
                                              aAccessHint )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgSelection::setViewIndexHintInBaseTable( qcStatement * aStatement,
                                           qmsFrom     * aFrom,
                                           qmsTableAccessHints * aAccessHint )
{
/***********************************************************************
 *
 * Description : VIEW�� ���� index hint ����
 *
 * Implementation : PROJ-1495
 *
 *  view������ �� querySet�� base table��
 *  view index hint�� �ش��ϴ� index�� ã�Ƽ� TableAccess hint�� �����.
 *
 *     1. hint index name�� �ش��ϴ� index�� �����ϴ��� �˻�
 *     2. �����ϸ�,
 *        indexHint������ ���� base table�� �����Ѵ�.
 *
 ***********************************************************************/

    qmsTableAccessHints   * sTableAccessHint;
    qmsTableAccessHints   * sCurAccess;
    qmsHintTables         * sHintTable;
    qmsHintIndices        * sHintIndex;
    qmsHintIndices        * sHintIndices = NULL;
    qmsHintIndices        * sLastHintIndex;
    qmsHintIndices        * sNewHintIndex;
    qcmIndex              * sIndex;
    UInt                    sCnt;

    IDU_FIT_POINT_FATAL( "qmgSelection::setViewIndexHintInBaseTable::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom != NULL );
    IDE_DASSERT( aAccessHint != NULL );

    //---------------------------------------------------
    // hint index name�� �ش��ϴ� index�� �����ϴ����� �˻��ؼ�
    // �����ϸ�, base table�� ������ indexHint������ �����.
    //---------------------------------------------------

    for( sHintIndex = aAccessHint->indices;
         sHintIndex != NULL;
         sHintIndex = sHintIndex->next )
    {
        for( sCnt = 0; sCnt < aFrom->tableRef->tableInfo->indexCount; sCnt++ )
        {
            sIndex = &(aFrom->tableRef->tableInfo->indices[sCnt]);

            if( idlOS::strMatch(
                    sHintIndex->indexName.stmtText + sHintIndex->indexName.offset,
                    sHintIndex->indexName.size,
                    sIndex->name,
                    idlOS::strlen(sIndex->name) ) == 0 )
            {
                IDE_TEST(
                    QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsHintIndices),
                                                   (void **)&sNewHintIndex )
                    != IDE_SUCCESS );

                idlOS::memcpy( &(sNewHintIndex->indexName),
                               &(sHintIndex->indexName),
                               ID_SIZEOF(qcNamePosition) );

                sNewHintIndex->indexPtr = sIndex;
                sNewHintIndex->next = NULL;

                if( sHintIndices == NULL )
                {
                    sHintIndices = sNewHintIndex;
                    sLastHintIndex = sHintIndices;
                }
                else
                {
                    sLastHintIndex->next = sNewHintIndex;
                    sLastHintIndex = sLastHintIndex->next;
                }

                break;
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    //----------------------------------------------
    // VIEW index hint �� �ش� base table�� �ε����� �����ϴ� ���
    // base table�� tableRef�� �� ���� ����
    //----------------------------------------------

    if( sHintIndices != NULL )
    {
        //--------------------------------------
        // qmsHintTable ���� ����
        //--------------------------------------

        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsHintTables),
                                           (void **)&sHintTable )
            != IDE_SUCCESS );

        idlOS::memcpy( &(sHintTable->userName),
                       &(aFrom->tableRef->userName),
                       ID_SIZEOF( qcNamePosition ) );
        idlOS::memcpy( &(sHintTable->tableName),
                       &(aFrom->tableRef->tableName),
                       ID_SIZEOF( qcNamePosition ) );
        sHintTable->table = aFrom;
        sHintTable->next  = NULL;

        //--------------------------------------
        // qmsTableAccessHints ���� ����
        //--------------------------------------

        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsTableAccessHints),
                                           (void **)&sTableAccessHint )
            != IDE_SUCCESS );

        idlOS::memcpy( sTableAccessHint,
                       aAccessHint,
                       ID_SIZEOF( qmsTableAccessHints ) );

        sTableAccessHint->table   = sHintTable;
        sTableAccessHint->indices = sHintIndices;
        sTableAccessHint->next    = NULL;

        //--------------------------------------
        // �ش� table�� tableRef�� tableAccessHint ���� ����
        //--------------------------------------

        if( aFrom->tableRef->tableAccessHints == NULL )
        {
            aFrom->tableRef->tableAccessHints = sTableAccessHint;
        }
        else
        {
            // �̹� �ش� table�� tableAccessHint�� �ִ� ���,
            // tableAccessHint���Ḯ��Ʈ�� �Ǹ������� ����.
            for( sCurAccess = aFrom->tableRef->tableAccessHints;
                 sCurAccess->next != NULL;
                 sCurAccess = sCurAccess->next ) ;

            sCurAccess->next = sTableAccessHint;
        }
    }
    else
    {
        // �ش� base table�� ����� index�� ����.
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::doViewPushProjection( qcStatement  * aViewStatement,
                                    qmsTableRef  * aViewTableRef,
                                    qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1473 
 *     view�� ���� push projection�� �����Ѵ�.
 *
 * Implementation :
 *
 *     1. SET���� union all�� ����
 *     2. distinct (X)
 *     3. group by (X)
 *     4. target Column�� aggr (X)
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmgSelection::doViewPushProjection::__FT__" );

    if( aQuerySet->setOp == QMS_NONE )
    {
        if( ( aQuerySet->SFWGH->selectType == QMS_ALL ) &&
            ( aQuerySet->SFWGH->group == NULL ) &&
            ( aQuerySet->SFWGH->aggsDepth1 == NULL ) )
        {
            IDE_TEST( doPushProjection( aViewStatement,
                                        aViewTableRef,
                                        aQuerySet )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do 
        }
    }
    else
    {
        if( aQuerySet->setOp == QMS_UNION_ALL )
        {
            IDE_TEST( doViewPushProjection( aViewStatement,
                                            aViewTableRef,
                                            aQuerySet->left )
                      != IDE_SUCCESS );

            IDE_TEST( doViewPushProjection( aViewStatement,
                                            aViewTableRef,
                                            aQuerySet->right )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do 
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::doPushProjection( qcStatement  * aViewStatement,
                                qmsTableRef  * aViewTableRef,
                                qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1473
 *     view�� ���� push projection�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcTemplate    * sMtcTemplate;
    mtcTuple       * sViewTuple;
    qmsTarget      * sTarget;
    UShort           sColumn;

    IDU_FIT_POINT_FATAL( "qmgSelection::doPushProjection::__FT__" );

    sMtcTemplate = &(QC_SHARED_TMPLATE(aViewStatement)->tmplate);
    sViewTuple   = &(sMtcTemplate->rows[aViewTableRef->table]);

    for( sColumn = 0, sTarget = aQuerySet->target;
         ( sColumn < sViewTuple->columnCount ) && ( sTarget != NULL );
         sColumn++, sTarget = sTarget->next )
    {
        if( ( sViewTuple->columns[sColumn].flag
              & MTC_COLUMN_USE_COLUMN_MASK )
            == MTC_COLUMN_USE_COLUMN_TRUE )
        {
            // fix BUG-20939
            if( ( QTC_IS_COLUMN( aViewStatement, sTarget->targetColumn ) 
                  == ID_TRUE ) )
            {
                if( ( sMtcTemplate->rows[sTarget->targetColumn->node.table].lflag
                      & MTC_TUPLE_MATERIALIZE_MASK )
                    == MTC_TUPLE_MATERIALIZE_VALUE )
                {
                    // view���η� push projection
                    sMtcTemplate->rows[sTarget->targetColumn->node.table].lflag
                        &= ~MTC_TUPLE_VIEW_PUSH_PROJ_MASK;
                    sMtcTemplate->rows[sTarget->targetColumn->node.table].lflag
                        |= MTC_TUPLE_VIEW_PUSH_PROJ_TRUE;

                    sMtcTemplate->rows[sTarget->targetColumn->node.table].
                        columns[sTarget->targetColumn->node.column].flag
                        &= ~MTC_COLUMN_VIEW_COLUMN_PUSH_MASK;
                    sMtcTemplate->rows[sTarget->targetColumn->node.table].
                        columns[sTarget->targetColumn->node.column].flag
                        |= MTC_COLUMN_VIEW_COLUMN_PUSH_TRUE;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // fix BUG-20939
                setViewPushProjMask( aViewStatement,
                                     sTarget->targetColumn );
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    return IDE_SUCCESS;
}

void
qmgSelection::setViewPushProjMask( qcStatement * aStatement,
                                   qtcNode     * aNode )
{
    qtcNode * sNode;

    for( sNode = (qtcNode*)aNode->node.arguments;
         sNode != NULL;
         sNode = (qtcNode*)sNode->node.next )
    {
        if( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
        {
            if( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].lflag
                  & MTC_TUPLE_MATERIALIZE_MASK )
                == MTC_TUPLE_MATERIALIZE_VALUE )
            {
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].lflag
                    &= ~MTC_TUPLE_VIEW_PUSH_PROJ_MASK;
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].lflag
                    |= MTC_TUPLE_VIEW_PUSH_PROJ_TRUE;

                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].
                    columns[sNode->node.column].flag
                    &= ~MTC_COLUMN_VIEW_COLUMN_PUSH_MASK;
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].
                    columns[sNode->node.column].flag
                    |= MTC_COLUMN_VIEW_COLUMN_PUSH_TRUE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            setViewPushProjMask( aStatement, sNode );
        }
    }
}

IDE_RC
qmgSelection::doViewPushSelection( qcStatement  * aStatement,
                                   qmsTableRef  * aTableRef,
                                   qmoPredicate * aPredicate,
                                   qmgGraph     * aGraph,
                                   idBool       * aIsPushed )
{
/***********************************************************************
 *
 * Description :
 *     BUG-18367 view push selection
 *     view�� ���� push selection�� �����Ѵ�.
 *
 * Implementation :
 *     view�� ���� ���� �� ���� ���¸� ó���Ѵ�.
 *     (1) view�� ���� one table predicate
 *     (2) PUSH_PRED ��Ʈ�� ������ view�� ���� join predicate
 *
 ***********************************************************************/

    qcStatement   * sView;
    qmsParseTree  * sViewParseTree;
    qmoPredicate  * sPredicate;
    qmoPredicate  * sRemainPredicate = NULL;
    qmoPredicate  * sLast;
    UShort          sViewTupleId;
    UInt            sColumnID;
    idBool          sIsIndexable;
    idBool          sIsPushed;         // view�� ���� set���� �ϳ��� ������
    idBool          sIsPushedAll;      // view�� ���� set���� ��� ������
    idBool          sRemainPushedPredicate;
    idBool          sRemovePredicate;

    /* TASK-7219 Non-shard DML */
    idBool          sIsPushedForce  = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgSelection::doViewPushSelection::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTableRef != NULL );
    IDE_DASSERT( aIsPushed != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sView = aTableRef->view;
    sViewTupleId = aTableRef->table;
    sViewParseTree = (qmsParseTree*) sView->myPlan->parseTree;

    for ( sPredicate = aPredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        sIsPushed              = ID_FALSE;
        sIsPushedAll           = ID_TRUE;
        sRemainPushedPredicate = ID_FALSE;

        /* TASK-7219 Non-shard DML */
        sIsPushedForce         = ID_FALSE;

        if ( (sPredicate->flag & QMO_PRED_PUSH_PRED_HINT_MASK)
             == QMO_PRED_PUSH_PRED_HINT_TRUE )
        {
            //---------------------------------------------------
            // PUSH_PRED ��Ʈ�� ���� ������ join predicate�� ���
            //---------------------------------------------------

            if ( ( sPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 != QTC_NODE_SUBQUERY_EXIST )
            {
                //---------------------------------------------------
                // �� Predicate�� Subquery�� �ƴ� ���
                //---------------------------------------------------

                IDE_TEST( qmoPred::isIndexable( aStatement,
                                                sPredicate,
                                                & aGraph->myFrom->depInfo,
                                                & qtc::zeroDependencies,
                                                & sIsIndexable )
                          != IDE_SUCCESS );

                if ( sIsIndexable == ID_TRUE )
                {
                    //---------------------------------------------------
                    // Indexable �� ���
                    //---------------------------------------------------

                    IDE_TEST( qmoPred::getColumnID( aStatement,
                                                    sPredicate->node,
                                                    ID_TRUE,
                                                    & sColumnID )
                              != IDE_SUCCESS );

                    if ( sColumnID != QMO_COLUMNID_LIST )
                    {
                        //---------------------------------------------------
                        // Indexable Predicate�� List�� �ƴ� ���
                        //---------------------------------------------------

                        IDE_TEST( qmoPushPred::doPushDownViewPredicate(
                                      aStatement,
                                      sViewParseTree,
                                      sViewParseTree->querySet,
                                      sViewTupleId,
                                      aGraph->myQuerySet->SFWGH,
                                      aGraph->myFrom,
                                      sPredicate,
                                      & sIsPushed,
                                      & sIsPushedAll,
                                      & sRemainPushedPredicate )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // List�� ���, Push Selection ���� ���� : ���� Ȯ��
                    }
                }
                else
                {
                    // indexable predicate�� �ƴ� ���
                    // nothing to do
                }
            }
            else
            {
                // Subquery�� ���
                // nothing to do
            }
        }
        else
        {
            //---------------------------------------------------
            // view�� one table predicate�� ���
            //---------------------------------------------------

            if ( ( sPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 != QTC_NODE_SUBQUERY_EXIST )
            {
                //---------------------------------------------------
                // �� Predicate�� Subquery�� �ƴ� ���
                //---------------------------------------------------

                if ( qtc::getPosNextBitSet( & sPredicate->node->depInfo,
                                            qtc::getPosFirstBitSet(
                                                & sPredicate->node->depInfo ) )
                     == QTC_DEPENDENCIES_END )
                {
                    //---------------------------------------------------
                    // �� Predicate�� �ܺ����� �÷��� ���� ���
                    //---------------------------------------------------

                    IDE_TEST( qmoPushPred::doPushDownViewPredicate(
                                  aStatement,
                                  sViewParseTree,
                                  sViewParseTree->querySet,
                                  sViewTupleId,
                                  aGraph->myQuerySet->SFWGH,
                                  aGraph->myFrom,
                                  sPredicate,
                                  & sIsPushed,
                                  & sIsPushedAll,
                                  & sRemainPushedPredicate )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* TASK-7219 Non-shard DML */
                    // �ܺ����� �÷��� ������ ���, property �� ���� ����
                    if ( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_MASK  )
                         == SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_ENABLE )
                    {
                        IDE_TEST( qmoPred::getColumnID( aStatement,
                                                        sPredicate->node,
                                                        ID_TRUE,
                                                        & sColumnID )
                                  != IDE_SUCCESS );

                        if ( sColumnID != QMO_COLUMNID_LIST )
                        {
                            IDE_TEST( qmoPushPred::doPushDownViewPredicate(
                                          aStatement,
                                          sViewParseTree,
                                          sViewParseTree->querySet,
                                          sViewTupleId,
                                          aGraph->myQuerySet->SFWGH,
                                          aGraph->myFrom,
                                          sPredicate,
                                          & sIsPushed,
                                          & sIsPushedAll,
                                          & sRemainPushedPredicate )
                                      != IDE_SUCCESS );
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                // Subquery�� ���
                // nothing to do
            }
        }

        /* TASK-7219 Non-shard DML */
        if ( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_MASK  )
             == SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_ENABLE )
        {
            /* TASK-7219 Non-shard DML */
            isForcePushedPredForShardView( &sPredicate->node->node,
                                           &sIsPushedForce );

            /* TASK-7219 Non-shard DML */
            if ( ( sPredicate->flag & QMO_PRED_PUSHED_FORCE_PRED_MASK )
                 == QMO_PRED_PUSHED_FORCE_PRED_TRUE )
            {
                sIsPushed = ID_FALSE;
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

        // fix BUG-12515
        // view�� ���ڵ尹���� push selection��
        // predicate�� selectivity�� ���ǹǷ�
        // where���� ���� �ִ� predicate�� selectivity��
        // �߰������� ������ �ʵ��� �ϱ� ����
        if( sIsPushed == ID_TRUE )
        {
            *aIsPushed = ID_TRUE;

            sPredicate->flag &= ~QMO_PRED_PUSH_REMAIN_MASK;
            sPredicate->flag |= QMO_PRED_PUSH_REMAIN_TRUE;
        }
        else
        {
            sPredicate->flag &= ~QMO_PRED_PUSH_REMAIN_MASK;
            sPredicate->flag |= QMO_PRED_PUSH_REMAIN_FALSE;
        }

        if ( ( ( sPredicate->flag & QMO_PRED_TRANS_PRED_MASK )
               == QMO_PRED_TRANS_PRED_TRUE ) ||
             ( sIsPushedForce == ID_TRUE ) )
        {
            //---------------------------------------------------
            // TRANSITIVE PREDICATE ����ȭ�� ���� ������ predicate��
            // predicate list���� �����Ѵ�.
            // PROJ-1404
            // ( ���ܵ� ��� filter�� ó���Ǹ� �̴� bad transitive predicate �̹Ƿ� )
            //---------------------------------------------------
            sRemovePredicate = ID_TRUE;
        }
        else
        {
            //---------------------------------------------------
            // Pushed Predicate��
            // �ش� predicate�� predicate list���� �����Ѵ�. ( BUG-18367 )
            // 
            // < ���� ��Ȳ : ��, predicate�� ���ܵξ�� �ϴ� ��� >
            // (1) Ư�� ���� Ư�� ������ pushed predicate�� �����ϸ�
            //     ����� Ʋ�� ���
            // (2) view�� set�� �� �Ϻο��� push predicate�� ���
            //---------------------------------------------------
            
            if ( sIsPushed == ID_TRUE )
            {
                if ( sIsPushedAll == ID_TRUE )
                {
                    if ( sRemainPushedPredicate == ID_TRUE )
                    {
                        // Ư�� ������ �� Ư�� ������ ��� push �Ǿ
                        // predicate�� �����ϸ� �� ����� Ʋ��
                        // �� ������ doPushDownViewPredicate()���� ��������
                        sRemovePredicate = ID_FALSE;
                    }
                    else
                    {
                        // view�� ��� queryset�� push predicate �����Ƿ�
                        // predicate�� �������ѵ���
                        sRemovePredicate = ID_TRUE;
                    }
                }
                else
                {
                    // �Ϻθ� push �Ǿ����Ƿ� predicate�� ���ܵξ�� ��
                    sRemovePredicate = ID_FALSE;
                }
            }
            else
            {
                // Push ���� �ʾ����Ƿ� predicate�� ���ܵξ�� ��
                sRemovePredicate = ID_FALSE;
            }
        }

        if ( sRemovePredicate == ID_TRUE )
        {
            // ������� ����
        }
        else
        {
            // ������� ����
            if( sRemainPredicate == NULL )
            {
                sRemainPredicate = sPredicate;
                sLast = sRemainPredicate;
            }
            else
            {
                sLast->next = sPredicate;
                sLast = sLast->next;
            }
        }
    } // for
    
    if( sRemainPredicate == NULL )
    {
        // Nothing To Do
    }
    else
    {
        sLast->next = NULL;

        /* BUG-47752 left outer join�� on���� OR �������� �����ǰ��ְ�
         * ������ view�� aggregaion�� �����Ұ�� fatal
         */
        if ( ( ( aGraph->flag & QMG_JOIN_RIGHT_MASK )
               == QMG_JOIN_RIGHT_TRUE ) &&
             ( sIsPushed == ID_TRUE ) )
        {
            aGraph->flag &= ~QMG_VIEW_PUSH_MASK;
            aGraph->flag |= QMG_VIEW_PUSH_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    aGraph->myPredicate = sRemainPredicate;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::makePreservedOrder( qcStatement        * aStatement,
                                  qmoIdxCardInfo     * aIdxCardInfo,
                                  UShort               aTable,
                                  qmgPreservedOrder ** aPreservedOrder)
{
/***********************************************************************
 *
 * Description : Preserved Order�� ����
 *
 * Implementation :
 *    (1) direction hint�� ���� ù��° Į�� direction ���� �� forward ���� ����
 *        - direction hint�� ���� ��� : ù��° Į�� direction�� not defined
 *        - direction hint�� �ִ� ��� : ù��° Į�� direction�� hint�� ����
 *                                     & forward �Ǵ� backward�� ������ ����
 *    (2) Index�� �� Į���� ���Ͽ� preserved order ���� �� ���� ����
 *
 ***********************************************************************/

    mtcColumn         * sKeyCols;
    UInt                sKeyColCnt;
    UInt              * sKeyColsFlag;
    qmgPreservedOrder * sPreservedOrder;
    qmgPreservedOrder * sFirst;
    qmgPreservedOrder * sCurrent;
    qmgDirectionType    sFirstDirection;
    idBool              sIsForward;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmgSelection::makePreservedOrder::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aIdxCardInfo != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sFirst       = NULL;
    sCurrent     = NULL;
    sKeyCols     = aIdxCardInfo->index->keyColumns;
    sKeyColCnt   = aIdxCardInfo->index->keyColCount;
    sKeyColsFlag = aIdxCardInfo->index->keyColsFlag;

    //---------------------------------------------------
    //  direction hint�� ���� ù��° Į�� direction �� Forward ���� ����
    //---------------------------------------------------

    switch( aIdxCardInfo->flag & QMO_STAT_CARD_IDX_HINT_MASK )
    {
        case  QMO_STAT_CARD_IDX_HINT_NONE :
        case  QMO_STAT_CARD_IDX_INDEX :
            sFirstDirection = QMG_DIRECTION_NOT_DEFINED;
            break;
        case  QMO_STAT_CARD_IDX_INDEX_ASC :
            if ( ( sKeyColsFlag[0] & SMI_COLUMN_ORDER_MASK ) ==
                 SMI_COLUMN_ORDER_ASCENDING )
            {
                sIsForward = ID_TRUE;
            }
            else
            {
                sIsForward = ID_FALSE;
            }
            sFirstDirection = QMG_DIRECTION_ASC;
            break;
        case  QMO_STAT_CARD_IDX_INDEX_DESC :
            if ( ( sKeyColsFlag[0] & SMI_COLUMN_ORDER_MASK ) ==
                 SMI_COLUMN_ORDER_ASCENDING )
            {
                sIsForward = ID_FALSE;
            }
            else
            {
                sIsForward = ID_TRUE;
            }
            sFirstDirection = QMG_DIRECTION_DESC;
            break;
        default :
            IDE_DASSERT( 0 );
            sFirstDirection = QMG_DIRECTION_NOT_DEFINED;
            break;
    }

    //---------------------------------------------------
    // Index�� �� Į���� ���Ͽ� preserved order ���� �� ���� ����
    //---------------------------------------------------

    for ( i = 0; i < sKeyColCnt; i++ )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                 (void **) & sPreservedOrder )
                  != IDE_SUCCESS );

        sPreservedOrder->table = aTable;
        sPreservedOrder->column =
            sKeyCols[i].column.id % SMI_COLUMN_ID_MAXIMUM;
        sPreservedOrder->next = NULL;

        if ( sFirst == NULL )
        {
            // direction ����
            sPreservedOrder->direction = sFirstDirection;
            // preserved order ����
            sFirst = sCurrent = sPreservedOrder;
        }
        else
        {
            //---------------------------------------------------
            // ���� ����
            //---------------------------------------------------

            if ( sFirstDirection == QMG_DIRECTION_NOT_DEFINED )
            {
                //---------------------------------------------------
                // ������ Not Defined �� ���
                //---------------------------------------------------

                if ( ( sKeyColsFlag[i-1] & SMI_COLUMN_ORDER_MASK ) ==
                     ( sKeyColsFlag[i] & SMI_COLUMN_ORDER_MASK ) )
                {
                    sPreservedOrder->direction = QMG_DIRECTION_SAME_WITH_PREV;
                }
                else
                {
                    sPreservedOrder->direction = QMG_DIRECTION_DIFF_WITH_PREV;
                }
            }
            else
            {
                //---------------------------------------------------
                // ������ ������ ���
                //---------------------------------------------------

                if ( sIsForward == ID_TRUE )
                {
                    if ( ( sKeyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                         == SMI_COLUMN_ORDER_ASCENDING )
                    {
                        sPreservedOrder->direction = QMG_DIRECTION_ASC;
                    }
                    else
                    {
                        sPreservedOrder->direction = QMG_DIRECTION_DESC;
                    }
                }
                else
                {
                    if ( ( sKeyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                         == SMI_COLUMN_ORDER_ASCENDING )
                    {
                        sPreservedOrder->direction = QMG_DIRECTION_DESC;
                    }
                    else
                    {
                        sPreservedOrder->direction = QMG_DIRECTION_ASC;
                    }
                }
            }

            sCurrent->next = sPreservedOrder;
            sCurrent = sCurrent->next;
        }
    }

    *aPreservedOrder = sFirst;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::getBestAccessMethodInExecutionTime(
    qcStatement      * aStatement,
    qmgGraph         * aGraph,
    qmoStatistics    * aStatInfo,
    qmoPredicate     * aPredicate,
    qmoAccessMethod  * aAccessMethod,
    qmoAccessMethod ** aSelectedAccessMethod )
{
    qmoAccessMethod     * sAccessMethod;
    qmoIdxCardInfo      * sIdxCardInfo;
    UInt                  sIdxCnt;
    UInt                  i;
    UInt                  sAccessMethodCnt;
    qmoIdxCardInfo      * sCurIdxCardInfo;
    idBool                sIsMemory = ID_FALSE;
    qmoSystemStatistics * sSysStat; // BUG-36958

    IDU_FIT_POINT_FATAL( "qmgSelection::getBestAccessMethodInExecutionTime::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aAccessMethod != NULL );

    sAccessMethod    = aAccessMethod;
    sAccessMethodCnt = 0;

    // PROJ-1436
    // execution time���� index card info�� �����ؼ� ����Ͽ�
    // shared plan�� �����Ű�� �ʰ� �Ѵ�.
    sIdxCnt = aStatInfo->indexCnt;

    // execution time�̹Ƿ� qmxMem�� ����Ѵ�.
    IDE_TEST( QC_QMX_MEM(aStatement)->alloc(
                  ID_SIZEOF(qmoIdxCardInfo) * sIdxCnt,
                  (void**) & sIdxCardInfo )
              != IDE_SUCCESS );

    idlOS::memcpy( (void*) sIdxCardInfo,
                   (void*) aStatInfo->idxCardInfo,
                   ID_SIZEOF(qmoIdxCardInfo) * sIdxCnt );
    
    // BUG-36958
    // reprepare �� qmeMem �� �����Ǹ鼭 cost ��꿡 �ʿ��� system ��������� ���� �� ����.
    // ���� execution time �� qmxMem �� ����Ͽ� system ��������� �ٽ� ���´�.

    IDE_TEST( QC_QMX_MEM(aStatement)->alloc(
                  ID_SIZEOF(qmoSystemStatistics),
                  (void**) & sSysStat)
              != IDE_SUCCESS );

    aStatement->mSysStat = sSysStat;

    if( aGraph->myQuerySet->SFWGH->hints->optGoalType != QMO_OPT_GOAL_TYPE_RULE )
    {
        IDE_TEST( qmoStat::getSystemStatistics( aStatement->mSysStat ) != IDE_SUCCESS );
    }
    else
    {
        qmoStat::getSystemStatistics4Rule( aStatement->mSysStat );
    }

    //---------------------------------------------------
    // Access Method�� ���� �ܰ�
    //---------------------------------------------------

    if( ( aGraph->flag & QMG_GRAPH_TYPE_MASK ) == QMG_GRAPH_TYPE_DISK )
    {
        // Nothing To Do
    }
    else
    {
        sIsMemory = ID_TRUE;
    }

    // FULL SCAN Access Method ����
    setFullScanMethod( aStatement,
                       aStatInfo,
                       aPredicate,
                       &sAccessMethod[sAccessMethodCnt],
                       1,
                       ID_TRUE ); // execution time

    sAccessMethodCnt++;

    // INDEX SCAN Access Method ����
    for ( i = 0; i < sIdxCnt; i++ )
    {
        sCurIdxCardInfo = & sIdxCardInfo[i];

        if ( ( sCurIdxCardInfo->flag & QMO_STAT_CARD_IDX_HINT_MASK )
             != QMO_STAT_CARD_IDX_NO_INDEX )
        {
            if ( sCurIdxCardInfo->index->isOnlineTBS == ID_TRUE )
            {
                IDE_TEST( setIndexScanMethod( aStatement,
                                              aGraph,
                                              aStatInfo,
                                              sCurIdxCardInfo,
                                              aPredicate,
                                              &sAccessMethod[sAccessMethodCnt],
                                              sIsMemory,
                                              ID_TRUE ) // execution time
                          != IDE_SUCCESS );

                sAccessMethodCnt++;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // NO INDEX Hint�� �ִ� ���, �������� ����
        }
    }

    IDE_TEST( selectBestMethod( aStatement,
                                aGraph->myFrom->tableRef->tableInfo,
                                &sAccessMethod[0],
                                sAccessMethodCnt,
                                aSelectedAccessMethod )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSelection::getBestAccessMethod(qcStatement      *aStatement,
                                         qmgGraph         *aGraph,
                                         qmoStatistics    *aStatInfo,
                                         qmoPredicate     *aPredicate,
                                         qmoAccessMethod  *aAccessMethod,
                                         qmoAccessMethod **aSelectedAccessMethod,
                                         UInt             *aAccessMethodCnt,
                                         qcmIndex        **aSelectedIndex,
                                         UInt             *aSelectedScanHint,
                                         UInt              aParallelDegree,
                                         UInt              aFlag )
{
/***********************************************************************
 *
 * Description : AccessMethod �� ���� �� cost �� �� ���� ����
 *               AccessMethod�� ����
 *
 * Implementation :
 *    (1) Access Method ���� ����
 *    (2) ����� Access Method���� ���� cost�� ���� access method ����
 *
 ***********************************************************************/

    qmoAccessMethod     * sAccessMethod;
    qmsTableAccessHints * sAccessHint;
    qmoIdxCardInfo      * sIdxCardInfo;
    UInt                  sIdxCnt;
    qmoIdxCardInfo      * sSelectedIdxInfo = NULL;
    qmoIdxCardInfo      * sCurIdxCardInfo;
    qmsHintIndices      * sHintIndex;
    idBool                sUsableHint = ID_FALSE;
    idBool                sIsMemory = ID_FALSE;
    idBool                sIsFullScan = ID_FALSE;
    UInt                  i;

    IDU_FIT_POINT_FATAL( "qmgSelection::getBestAccessMethod::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aAccessMethod != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sAccessMethod        = aAccessMethod;
    sAccessHint          = aGraph->myFrom->tableRef->tableAccessHints;
    sIdxCardInfo         = aStatInfo->idxCardInfo;
    sIdxCnt              = aStatInfo->indexCnt;
    *aAccessMethodCnt    = 0;

    for( i = 0; i < sIdxCnt; i++ )
    {
        // fix BUG-12888
        // �� flag�� graph�����ø� �����ϱ⶧����,
        // clear�ص� �̹� ������ graph���� ������ ��ġ�� ����.
        sIdxCardInfo[i].flag = QMO_STAT_CLEAR;
    }

    //---------------------------------------------------
    // Access Method�� ���� �ܰ�
    //---------------------------------------------------

    if( ( aGraph->flag & QMG_GRAPH_TYPE_MASK ) == QMG_GRAPH_TYPE_DISK )
    {
        // Nothing To Do
    }
    else
    {
        sIsMemory = ID_TRUE;
    }

    for ( ; sAccessHint != NULL; sAccessHint = sAccessHint->next )
    {
        //---------------------------------------------------
        // Hint�� �����ϴ� ���, Hint�� ���� Access Method ����
        //---------------------------------------------------

        if ( sAccessHint->accessType ==
             QMO_ACCESS_METHOD_TYPE_FULLACCESS_SCAN )
        {
            setFullScanMethod( aStatement,
                               aStatInfo,
                               aPredicate,
                               &sAccessMethod[*aAccessMethodCnt],
                               aParallelDegree,
                               ID_FALSE ); // prepare time
            sIsFullScan = ID_TRUE;
            (*aAccessMethodCnt)++;

            // To Fix PR-11937
            // Index Nested Loop Join �� ��� ���ɼ� ���θ� �Ǵ��ϱ� ���Ͽ�
            // Full Scan Hint�� ����Ǿ����� �����Ѵ�.
            // Join Graph���� �̰��� ���� ���θ� ���� �Ǵ��ϰ� �ȴ�.
            aGraph->flag &= ~QMG_SELT_FULL_SCAN_HINT_MASK;
            aGraph->flag |= QMG_SELT_FULL_SCAN_HINT_TRUE;
        }
        else
        {
            //---------------------------------------------------
            // INDEX SCAN Hint �Ǵ� NO INDEX SCAN Hint �� ���,
            // ������ Index�� ���󰡸鼭 ��� ������ index hint�� ���
            // access method�� �����Ѵ�.
            //---------------------------------------------------

            // BUG-43534 index ( table ) ��Ʈ�� �����ؾ� �մϴ�.
            if ( sAccessHint->indices == NULL )
            {
                for ( i = 0; i < sIdxCnt; i++ )
                {
                    IDE_TEST( qmg::usableIndexScanHint(
                                  sIdxCardInfo[i].index,
                                  sAccessHint->accessType,
                                  sIdxCardInfo,
                                  sIdxCnt,
                                  & sSelectedIdxInfo,
                                  & sUsableHint )
                              != IDE_SUCCESS );

                    if ( sUsableHint == ID_TRUE )
                    {
                        IDE_TEST( setIndexScanMethod(
                                      aStatement,
                                      aGraph,
                                      aStatInfo,
                                      sSelectedIdxInfo,
                                      aPredicate,
                                      &sAccessMethod[*aAccessMethodCnt],
                                      sIsMemory,
                                      ID_FALSE ) // prepare time
                                  != IDE_SUCCESS );

                        (*aAccessMethodCnt)++;
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

            for ( sHintIndex = sAccessHint->indices;
                  sHintIndex != NULL;
                  sHintIndex = sHintIndex->next )
            {
                IDE_TEST(
                    qmg::usableIndexScanHint( sHintIndex->indexPtr,
                                              sAccessHint->accessType,
                                              sIdxCardInfo,
                                              sIdxCnt,
                                              & sSelectedIdxInfo,
                                              & sUsableHint )
                    != IDE_SUCCESS );

                if ( sUsableHint == ID_TRUE )
                {
                    IDE_TEST( setIndexScanMethod( aStatement,
                                                  aGraph,
                                                  aStatInfo,
                                                  sSelectedIdxInfo,
                                                  aPredicate,
                                                  &sAccessMethod[*aAccessMethodCnt],
                                                  sIsMemory,
                                                  ID_FALSE ) // prepare time
                              != IDE_SUCCESS );

                    (*aAccessMethodCnt)++;
                }
                else
                {
                    // nothing to do
                }
            }
        }
    }

    if( *aAccessMethodCnt == 0 )
    {
        *aSelectedScanHint = QMG_NOT_USED_SCAN_HINT;
    }
    else if( *aAccessMethodCnt == 1 )
    {
        if( sAccessMethod[0].method == NULL )
        {
            //---------------------------------------------------
            // FULL SCAN Hint ���� �����ϴ� ���,
            // - Preserved Order�� Never Preserved Order�� �����ϱ� ����
            //---------------------------------------------------
            *aSelectedScanHint = QMG_USED_ONLY_FULL_SCAN_HINT;
        }
        else
        {
            // hint�� ���ؼ� index scan�� ������ ���
            // ���߿� host ������ ����ȭ�� �õ����� �ʱ� ����
            *aSelectedScanHint = QMG_USED_SCAN_HINT;
        }
    }
    else
    {
        // BUG-13800
        // Scan hint�� ������ �־����� ���
        // ���߿� �ϳ��� ���õ� ���̱� ������
        // ���������� host ����ȭ�� ����ؾ� �Ѵ�.
        *aSelectedScanHint = QMG_USED_SCAN_HINT;
    }

    if ( *aAccessMethodCnt == 0 )
    {
        //---------------------------------------------------
        // (1) Hint�� �������� �ʴ� ���
        // (2) Index Hint�� �־������� �ش� Hint�� ��� ����� �� ���� ���
        // (3) No Index Hint ���� ������ ���
        //---------------------------------------------------

        // FULL SCAN Access Method ����
        setFullScanMethod( aStatement,
                           aStatInfo,
                           aPredicate,
                           &sAccessMethod[*aAccessMethodCnt],
                           aParallelDegree,
                           ID_FALSE ); // prepare time
        sIsFullScan = ID_TRUE;
        (*aAccessMethodCnt)++;

        // INDEX SCAN Access Method ����
        for ( i = 0; i < sIdxCnt; i++ )
        {
            sCurIdxCardInfo = & sIdxCardInfo[i];

            if ( ( sCurIdxCardInfo->flag & QMO_STAT_CARD_IDX_HINT_MASK )
                 != QMO_STAT_CARD_IDX_NO_INDEX )
            {
                if ( sCurIdxCardInfo->index->isOnlineTBS == ID_TRUE )
                {
                    IDE_TEST( setIndexScanMethod( aStatement,
                                                  aGraph,
                                                  aStatInfo,
                                                  sCurIdxCardInfo,
                                                  aPredicate,
                                                  &sAccessMethod[*aAccessMethodCnt],
                                                  sIsMemory,
                                                  ID_FALSE ) // prepare time
                              != IDE_SUCCESS );

                    (*aAccessMethodCnt)++;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // NO INDEX Hint�� �ִ� ���, �������� ����
            }
        }
    }
    else
    {
        // nothing to do
    }

    /**
     * PROJ-2641 Hierarchy Query Index
     * Hierarchy query �� ��� full scan���� index scan�� �����ϰ�
     * ����� index scan�� index �� memory�� load�Ǹ� disk io��
     * ���ٰ� ���� full scan�ÿ��� level�� ���� disk io�� ����������
     * ũ�� �����ϹǷ� full scan�� disk io ���� �������� �ش�.
     */
    if ( ( ( aFlag & QMG_BEST_ACCESS_METHOD_HIERARCHY_MASK )
           == QMG_BEST_ACCESS_METHOD_HIERARCHY_TRUE ) &&
         ( sIsFullScan == ID_TRUE ) )
    {
        sAccessMethod[0].diskCost   = sAccessMethod[0].diskCost * QMG_HIERARCHY_QUERY_DISK_IO_ADJUST_VALUE;
        sAccessMethod[0].totalCost  = sAccessMethod[0].accessCost + sAccessMethod[0].diskCost;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( selectBestMethod( aStatement,
                                aGraph->myFrom->tableRef->tableInfo,
                                &sAccessMethod[0],
                                *aAccessMethodCnt,
                                aSelectedAccessMethod )
              != IDE_SUCCESS );

    if ((*aSelectedAccessMethod)->method == NULL )
    {
        *aSelectedIndex = NULL;
    }
    else
    {
        *aSelectedIndex = (*aSelectedAccessMethod)->method->index;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmgSelection::setFullScanMethod( qcStatement      * aStatement,
                                      qmoStatistics    * aStatInfo,
                                      qmoPredicate     * aPredicate,
                                      qmoAccessMethod  * aAccessMethod,
                                      UInt               aParallelDegree,
                                      idBool             aInExecutionTime )
{
    SDouble sCost;
    SDouble sSelectivity;

    // filter selectivity
    qmoSelectivity::getTotalSelectivity4PredList( aStatement,
                                                  aPredicate,
                                                  &sSelectivity,
                                                  aInExecutionTime );

    // filter cost
    aAccessMethod->filterCost = qmoCost::getFilterCost(
        aStatement->mSysStat,
        aPredicate,
        aStatInfo->totalRecordCnt );

    sCost = qmoCost::getTableFullScanCost( aStatement->mSysStat,
                                           aStatInfo,
                                           aAccessMethod->filterCost,
                                           &(aAccessMethod->accessCost),
                                           &(aAccessMethod->diskCost) );

    aAccessMethod->method               = NULL;
    aAccessMethod->keyRangeSelectivity  = 1;
    aAccessMethod->keyFilterSelectivity = 1;
    aAccessMethod->filterSelectivity    = sSelectivity;
    aAccessMethod->methodSelectivity    = sSelectivity;
    aAccessMethod->keyRangeCost         = 0;
    aAccessMethod->keyFilterCost        = 0;

    // TASK-6699 TPC-H ���� ����
    // ParallelDegree �� ����Ͽ� Full scan cost �� ����Ѵ�.
    aAccessMethod->accessCost = aAccessMethod->accessCost / aParallelDegree;
    aAccessMethod->diskCost   = aAccessMethod->diskCost / aParallelDegree;
    aAccessMethod->totalCost  = sCost / aParallelDegree;

}

IDE_RC qmgSelection::setIndexScanMethod( qcStatement      * aStatement,
                                         qmgGraph         * aGraph,
                                         qmoStatistics    * aStatInfo,
                                         qmoIdxCardInfo   * aIdxCardInfo,
                                         qmoPredicate     * aPredicate,
                                         qmoAccessMethod  * aAccessMethod,
                                         idBool             aIsMemory,
                                         idBool             aInExecutionTime )
{
    SDouble            sCost;
    SDouble            sKeyRangeCost;
    SDouble            sKeyFilterCost;
    SDouble            sFilterCost;
    SDouble            sLoopCount;
    qcTemplate       * sTemplate;
    mtcColumn        * sColumns;
    qmoAccessMethod  * sAccessMethod = aAccessMethod;

    qmoPredWrapper   * sKeyRange     = NULL;
    qmoPredWrapper   * sKeyFilter    = NULL;
    qmoPredWrapper   * sFilter       = NULL;
    qmoPredWrapper   * sLobFilter    = NULL;
    qmoPredWrapper   * sSubQFilter   = NULL;
    qmoPredWrapper   * sWrapperIter;
    qmoPredWrapperPool sWrapperPool;

    SDouble            sKeyRangeSelectivity;
    SDouble            sKeyFilterSelectivity;
    SDouble            sFilterSelectivity;
    SDouble            sMethodSelectivity;
    UShort             sTempArr[SMI_COLUMN_ID_MAXIMUM];
    UInt               i;

    IDU_FIT_POINT_FATAL( "qmgSelection::setIndexScanMethod::__FT__" );

    if( aInExecutionTime == ID_TRUE )
    {
        sTemplate = QC_PRIVATE_TMPLATE(aStatement);
    }
    else
    {
        sTemplate = QC_SHARED_TMPLATE(aStatement);
    }
    sColumns = sTemplate->tmplate.rows[aGraph->myFrom->tableRef->table].columns;

    if( aPredicate != NULL )
    {
        //--------------------------------------
        // range, filter ����
        //--------------------------------------
        IDE_TEST( qmoPred::extractRangeAndFilter( aStatement,
                                                  sTemplate,
                                                  aIsMemory,
                                                  aInExecutionTime,
                                                  aIdxCardInfo->index,
                                                  aPredicate,
                                                  &sKeyRange,
                                                  &sKeyFilter,
                                                  &sFilter,
                                                  &sLobFilter,
                                                  &sSubQFilter,
                                                  &sWrapperPool )
                  != IDE_SUCCESS );

        //--------------------------------------
        // range, filter selectivity
        //--------------------------------------
        IDE_TEST( qmoSelectivity::getSelectivity4KeyRange( sTemplate,
                                                           aStatInfo,
                                                           aIdxCardInfo,
                                                           sKeyRange,
                                                           &sKeyRangeSelectivity,
                                                           aInExecutionTime )
                  != IDE_SUCCESS );

        // fix BUG-42752
        if ( QCU_OPTIMIZER_ESTIMATE_KEY_FILTER_SELECTIVITY == 1 )
        {
            IDE_TEST( qmoSelectivity::getSelectivity4PredWrapper( sTemplate,
                                                                  sKeyFilter,
                                                                  &sKeyFilterSelectivity,
                                                                  aInExecutionTime )
                      != IDE_SUCCESS );
        }
        else
        {
            sKeyFilterSelectivity = 1;
        }

        IDE_TEST( qmoSelectivity::getSelectivity4PredWrapper( sTemplate,
                                                              sFilter,
                                                              &sFilterSelectivity,
                                                              aInExecutionTime )
                  != IDE_SUCCESS );

        //--------------------------------------
        // range, filter cost
        //--------------------------------------
        sKeyRangeCost  = qmoCost::getKeyRangeCost(  aStatement->mSysStat,
                                                    aStatInfo,
                                                    aIdxCardInfo,
                                                    sKeyRange,
                                                    sKeyRangeSelectivity );


        sLoopCount     = aStatInfo->totalRecordCnt * sKeyRangeSelectivity;

        sKeyFilterCost = qmoCost::getKeyFilterCost( aStatement->mSysStat,
                                                    sKeyFilter,
                                                    sLoopCount );


        sLoopCount     = sLoopCount * sKeyFilterSelectivity;

        sFilterCost    = qmoCost::getFilterCost4PredWrapper(
            aStatement->mSysStat,
            sFilter,
            sLoopCount );

        sFilterCost   += qmoCost::getFilterCost4PredWrapper(
            aStatement->mSysStat,
            sSubQFilter,
            sLoopCount );

        sMethodSelectivity = sKeyRangeSelectivity  *
            sKeyFilterSelectivity *
            sFilterSelectivity;
    }
    else
    {
        sKeyRangeCost          = 0;
        sKeyFilterCost         = 0;
        sFilterCost            = 0;

        sKeyRangeSelectivity   = QMO_SELECTIVITY_NOT_EXIST;
        sKeyFilterSelectivity  = QMO_SELECTIVITY_NOT_EXIST;
        sFilterSelectivity     = QMO_SELECTIVITY_NOT_EXIST;
        sMethodSelectivity     = QMO_SELECTIVITY_NOT_EXIST;
    }

    //--------------------------------------
    // Index cost
    //--------------------------------------
    sCost = qmoCost::getIndexScanCost( aStatement,
                                       sColumns,
                                       aPredicate,
                                       aStatInfo,
                                       aIdxCardInfo,
                                       sKeyRangeSelectivity,
                                       sKeyFilterSelectivity,
                                       sKeyRangeCost,
                                       sKeyFilterCost,
                                       sFilterCost,
                                       &(aAccessMethod->accessCost),
                                       &(aAccessMethod->diskCost) );

    //--------------------------------------
    // qmoAccessMethod ����
    //--------------------------------------
    sAccessMethod->method               = aIdxCardInfo;

    // To Fix PR-9181
    // Index�� Key Range�� ���� Predicate�� ������ �ִ� ��
    // ������ ���� ��� IN SUBQUERY KEY RANGE�� ����ϴ� ����
    // ������.
    if( sKeyRange == NULL )
    {
        sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_EXIST_PRED_MASK;
        sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_EXIST_PRED_FALSE;

        sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK;
        sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_IN_SUBQUERY_FALSE;

        /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
        if ( (( QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY == 0) ||
              ( QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY == 2) ) &&
             (aGraph->myFrom->tableRef->tableInfo->primaryKey != NULL) )
        {
            if ( sAccessMethod->method->index->indexId
                 == aGraph->myFrom->tableRef->tableInfo->primaryKey->indexId )
            {
                // Primary Index��
                sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_PRIMARY_MASK;
                sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_PRIMARY_TRUE;
            }
            else
            {
                // Nothing to do.
            }

            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_EXIST_PRED_MASK;
        sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_EXIST_PRED_TRUE;

        // To Fix PR-9181
        // IN SUBQUERY KEYRANGE�� �����Ǿ� �ִ� ���� ������
        if ( ( sKeyRange->pred->flag & QMO_PRED_INSUBQUERY_MASK )
             == QMO_PRED_INSUBQUERY_ABSENT )
        {
            sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK;
            sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_IN_SUBQUERY_FALSE;
        }
        else
        {
            sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK;
            sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_IN_SUBQUERY_TRUE;
        }

        if ( aGraph->myFrom->tableRef->tableInfo->primaryKey != NULL )
        {
            if( sAccessMethod->method->index->indexId
                == aGraph->myFrom->tableRef->tableInfo->primaryKey->indexId )
            {
                // Primary Index��
                sAccessMethod->method->flag &= ~QMO_STAT_CARD_IDX_PRIMARY_MASK;
                sAccessMethod->method->flag |= QMO_STAT_CARD_IDX_PRIMARY_TRUE;
            }
        }
    }

    sAccessMethod->keyRangeSelectivity  = sKeyRangeSelectivity;
    sAccessMethod->keyFilterSelectivity = sKeyFilterSelectivity;
    sAccessMethod->filterSelectivity    = sFilterSelectivity;

    // BUG-48120 
    if ( sAccessMethod->method != NULL )
    {  
        if ( (sAccessMethod->method->flag & QMO_STAT_CARD_ALL_EQUAL_IDX_MASK)
             == QMO_STAT_CARD_ALL_EQUAL_IDX_TRUE )
        {
            // BUG-48120 output record�� ����� �� ����ϴ� 
            // methodSelectivity��index�� keyNDV �� ����ؾ��Ѵ�.
            // (__optimizer_scan_cost_mode 1�� ���Ե� ���)
            // �� ��� key filter�� ����.
            sAccessMethod->methodSelectivity    = (1/aIdxCardInfo->KeyNDV) * sFilterSelectivity;
        }
        else
        {
            // BUG-48120 �������� ����
            sAccessMethod->methodSelectivity    = sMethodSelectivity;
        }
    }
    else
    {
        // full scan �� ��� ������ ����
        sAccessMethod->methodSelectivity    = sMethodSelectivity;
    }

    sAccessMethod->keyRangeCost         = sKeyRangeCost;
    sAccessMethod->keyFilterCost        = sKeyFilterCost;
    sAccessMethod->filterCost           = sFilterCost;
    sAccessMethod->totalCost            = sCost;

    /*
     * BUG-39298 improve preserved order
     *
     * create index .. on t1 (i1, i2, i3)
     * select .. from .. where t1.i1 = 1 and t1.i2 = 2 order by i3;
     *
     * ������ ���� ���� column (i1, i2) �� �����صд�.
     * �Ŀ� preserved order �񱳽� i1, i2 �� �����ϰ� ���Ѵ�.
     */
    if ((sKeyRange != NULL) && (aInExecutionTime == ID_FALSE))
    {
        i = 0;
        for (sWrapperIter = sKeyRange;
             sWrapperIter != NULL;
             sWrapperIter = sWrapperIter->next)
        {
            if ((sWrapperIter->pred->flag & QMO_PRED_INDEXABLE_EQUAL_MASK) ==
                QMO_PRED_INDEXABLE_EQUAL_TRUE)
            {
                sTempArr[i] = (sWrapperIter->pred->id % SMI_COLUMN_ID_MAXIMUM);
                i++;
            }
        }

        if (i > 0)
        {
            IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                         ID_SIZEOF(UShort) * i,
                         (void**)&sAccessMethod->method->mKeyRangeColumn.mColumnArray)
                     != IDE_SUCCESS);

            sAccessMethod->method->mKeyRangeColumn.mColumnCount = i;

            for (i = 0; i < sAccessMethod->method->mKeyRangeColumn.mColumnCount; i++)
            {
                sAccessMethod->method->mKeyRangeColumn.mColumnArray[i] = sTempArr[i];
            }
        }
        else
        {
            sAccessMethod->method->mKeyRangeColumn.mColumnCount = 0;
            sAccessMethod->method->mKeyRangeColumn.mColumnArray = NULL;
        }
    }
    else
    {
        sAccessMethod->method->mKeyRangeColumn.mColumnCount = 0;
        sAccessMethod->method->mKeyRangeColumn.mColumnArray = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmgSelection::selectBestMethod( qcStatement      * aStatement,
                                       qcmTableInfo     * aTableInfo,
                                       qmoAccessMethod  * aAccessMethod,
                                       UInt               aAccessMethodCnt,
                                       qmoAccessMethod ** aSelected )
{
    UInt   i;
    idBool sIndexChange;
    UInt   sSelectedMethod = 0;

    IDU_FIT_POINT_FATAL( "qmgSelection::selectBestMethod::__FT__" );

    //---------------------------------------------------
    // Accees Method ���� �ܰ�
    //   - cost�� ���� ���� table access method�� ����
    //---------------------------------------------------

    // sSelected �ʱ�ȭ
    *aSelected = aAccessMethod;

    for ( i = 0; i < aAccessMethodCnt; i++ )
    {
        if (QMO_COST_IS_GREATER((*aSelected)->totalCost,
                                aAccessMethod[i].totalCost) == ID_TRUE)
        {
            //---------------------------------------------------
            // cost�� ���� access method ����
            //---------------------------------------------------

            // To Fix PR-8134
            // Cost �� ���� �ε����� ���� �ϴ��� ���� ����ϴ�
            // Predicate�� �����Ͽ��� ��.
            if ( aAccessMethod[i].method != NULL )
            {
                if ( ( aAccessMethod[i].method->index->indexTypeId == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID ) && 
                     ( ( aAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_EXIST_PRED_MASK )
                       == QMO_STAT_CARD_IDX_EXIST_PRED_FALSE ) )
                {
                    // BUG-38082, BUG-40269
                    // predicate �� �����ϴ��� R-tree�� KeyRange�� ������ ���� ���� predicate�̶��
                    // index(R-tree) ������ �����Ѵ�. �׷��� predicate ������ ������� flag�� �˻��ϵ��� �Ѵ�.

                    sIndexChange = ID_FALSE;
                }
                else
                {
                    // BUG-37861 / BUG-39666 �޸� ���̺����� Predicate�� �˻���
                    /* PROJ-2464 hybrid partitioned table ���� */
                    if ( ( aTableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) != SMI_TABLE_DISK )
                    {
                        if( (aAccessMethod[i].method->flag &
                             QMO_STAT_CARD_IDX_EXIST_PRED_MASK)
                            ==  QMO_STAT_CARD_IDX_EXIST_PRED_TRUE )
                        {
                            sIndexChange = ID_TRUE;
                        }
                        else
                        {
                            sIndexChange = ID_FALSE;
                        }
                    }
                    else
                    {
                        sIndexChange = ID_TRUE;
                    }
                }
            }
            else
            {
                sIndexChange = ID_FALSE;
            }
        }
        else if (QMO_COST_IS_EQUAL((*aSelected)->totalCost,
                                   aAccessMethod[i].totalCost) == ID_TRUE)
        {
            //---------------------------------------------------
            // cost�� ������ ���
            //---------------------------------------------------

            sIndexChange = ID_FALSE;

            // Access Method�� ������ �������� ����
            while (1)
            {
                //-------------------------------------------------
                // ���� Method�� Full Scan�̶�� �������� ����
                //-------------------------------------------------

                if (aAccessMethod[i].method == NULL)
                {
                    sIndexChange = ID_FALSE;
                    break;
                }
                else
                {
                    // Go Go
                }

                if ((aAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_PRIMARY_MASK) ==
                    QMO_STAT_CARD_IDX_PRIMARY_TRUE)
                {
                    sIndexChange = ID_TRUE;
                    break;
                }
                else
                {
                    // Go Go
                }

                //----------------------------------------------------------------------------------------
                // BUG-38082, BUG-40269
                // predicate �� �����ϴ��� R-tree�� KeyRange�� ������ ���� ���� predicate�̶��
                // index(R-tree) ������ �����Ѵ�. �׷��� predicate ������ ������� flag�� �˻��ϵ��� �Ѵ�.
                //----------------------------------------------------------------------------------------

                if ( ( aAccessMethod[i].method->index->indexTypeId == SMI_ADDITIONAL_RTREE_INDEXTYPE_ID ) && 
                     ( ( aAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_EXIST_PRED_MASK )
                       == QMO_STAT_CARD_IDX_EXIST_PRED_FALSE ) )
                {
                    sIndexChange = ID_FALSE;
                    break;
                }
                else
                {
                    // Go Go
                }

                //-------------------------------------------------
                // ���� Method�� Predicate�� ����
                // Index��� �������� ����
                //-------------------------------------------------

                if ((aAccessMethod[i].method->flag &
                     QMO_STAT_CARD_IDX_EXIST_PRED_MASK) !=
                    QMO_STAT_CARD_IDX_EXIST_PRED_TRUE)
                {
                    sIndexChange = ID_FALSE;
                    break;
                }
                else
                {
                    // Go Go
                }

                //-------------------------------------------------
                // ���õ� Method�� Full Scan�̰�
                // ���� Method�� Predicate�� ���� Index���
                // Method�� ������.
                //-------------------------------------------------

                if ((*aSelected)->method == NULL)
                {
                    sIndexChange = ID_TRUE;
                    break;
                }
                else
                {
                    // Go Go
                }

                //-------------------------------------------------
                // ���� Method�� Predicate�� �ִ� index�̰�
                // ���õ� Method�� Predicate�� ���� index�� ��
                // Method ������.
                //------------------------------------------------

                if (((*aSelected)->method->flag &
                     QMO_STAT_CARD_IDX_EXIST_PRED_MASK)
                    == QMO_STAT_CARD_IDX_EXIST_PRED_FALSE)
                {
                    sIndexChange = ID_TRUE;
                    break;
                }
                else
                {
                    // Go Go
                }

                //-------------------------------------------------
                // �� ��� �ش� index�� ����ϴ� predicate�� �ִ� ���,
                // keyFilterSelecitivity�� ���� method�� ����
                //------------------------------------------------

                if ((*aSelected)->keyFilterSelectivity >
                    aAccessMethod[i].keyFilterSelectivity)
                {
                    sIndexChange = ID_TRUE;
                    break;
                }
                else
                {
                }

                /* BUG-48120 : index�� keyRangeSelectivity�� ���ϱ� ���� 
                 * ���� Method��ALL EQUAL INDEX �̰�,
                 * ���õ� Method�� ALL EQUAL INDEX�� �� Method ������
                 * (__optimizer_scan_cost_mode 1�� ���Ե� ���) */
                if ( ( ((*aSelected)->method->flag & QMO_STAT_CARD_ALL_EQUAL_IDX_MASK)
                       == QMO_STAT_CARD_ALL_EQUAL_IDX_FALSE ) &&
                     ( (aAccessMethod[i].method->flag & QMO_STAT_CARD_ALL_EQUAL_IDX_MASK)
                       == QMO_STAT_CARD_ALL_EQUAL_IDX_TRUE ) )
                {
                    sIndexChange = ID_TRUE;
                    break;
                }

                /*
                 * BUG-30307: index ���ý� �������� ������
                 *            ��ȿ������ index �� Ÿ�� ��찡 �ֽ��ϴ�
                 *
                 * �ش� index �� selectivity �� ���غ���
                 *
                 */
                if ((*aSelected)->keyRangeSelectivity > aAccessMethod[i].keyRangeSelectivity)
                {
                    sIndexChange = ID_TRUE;
                    break;
                }
                else
                {
                }

                break;
            } // while
        }
        else
        {
            sIndexChange = ID_FALSE;
        }

        // Access Method ����
        if ( sIndexChange == ID_TRUE )
        {
            *aSelected = & aAccessMethod[i];
        }
        else
        {
            // Nothing to do.
        }
    } // for

    /* TASK-6744 */
    if ( (QCU_PLAN_RANDOM_SEED != 0) &&
         (aStatement->mRandomPlanInfo != NULL) &&
         (smiGetStartupPhase() == SMI_STARTUP_SERVICE) )
    {
        aStatement->mRandomPlanInfo->mTotalNumOfCases
            = aStatement->mRandomPlanInfo->mTotalNumOfCases + aAccessMethodCnt;
        sSelectedMethod =
            QCU_PLAN_RANDOM_SEED % (aStatement->mRandomPlanInfo->mTotalNumOfCases - aStatement->mRandomPlanInfo->mWeightedValue);

        if ( sSelectedMethod >= aAccessMethodCnt )
        {
            sSelectedMethod = (QCU_PLAN_RANDOM_SEED + sSelectedMethod) % aAccessMethodCnt;
        }
        else
        {
            // Nothing to do.
        }

        IDE_DASSERT( sSelectedMethod < aAccessMethodCnt ); 

        aStatement->mRandomPlanInfo->mWeightedValue++;

        *aSelected = & aAccessMethod[sSelectedMethod];

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmgSelection::copyPreservedOrderFromView( qcStatement        * aStatement,
                                          qmgGraph           * aChildGraph,
                                          UShort               aTable,
                                          qmgPreservedOrder ** aNewOrder )
{
/***********************************************************************
 *
 * Description : View�� preserved order�� Copy�Ͽ� table, column ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPreservedOrder * sPreservedOrder;
    qmgPreservedOrder * sViewOrder;
    qmgPreservedOrder * sCurOrder;
    qmgPreservedOrder * sFirstOrder;
    qmgPROJ           * sPROJ;
    qmsTarget         * sTarget;
    qtcNode           * sTargetNode;
    UShort              sPredPos;

    IDU_FIT_POINT_FATAL( "qmgSelection::copyPreservedOrderFromView::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sCurOrder   = NULL;
    sFirstOrder = NULL;
    sPROJ       = (qmgPROJ *)aChildGraph;
    sPredPos    = 0;

    sViewOrder  = aChildGraph->preservedOrder;
    for ( ; sViewOrder != NULL; sViewOrder = sViewOrder->next )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                 (void**)&sPreservedOrder )
                  != IDE_SUCCESS );

        // table ����
        sPreservedOrder->table = aTable;

        // column ����
        for ( sTarget = sPROJ->target, sPredPos = 0;
              sTarget != NULL;
              sTarget = sTarget->next, sPredPos++ )
        {
            // BUG-38193 target�� pass node �� ����ؾ� �մϴ�.
            if ( sTarget->targetColumn->node.module == &qtc::passModule )
            {
                sTargetNode = (qtcNode*)(sTarget->targetColumn->node.arguments);
            }
            else
            {
                sTargetNode = sTarget->targetColumn;
            }

            if ( ( sViewOrder->table  == sTargetNode->node.table  ) &&
                 ( sViewOrder->column == sTargetNode->node.column ) )
            {
                break;
            }
            else
            {
                // nothing to do
            }
        }
        sPreservedOrder->column = sPredPos;

        sPreservedOrder->direction = sViewOrder->direction;
        sPreservedOrder->next = NULL;

        if ( sFirstOrder == NULL )
        {
            sFirstOrder = sCurOrder = sPreservedOrder;
        }
        else
        {
            sCurOrder->next = sPreservedOrder;
            sCurOrder = sCurOrder->next;
        }
    }

    *aNewOrder = sFirstOrder;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::copySELTAndAlterSelectedIndex( qcStatement * aStatement,
                                             qmgSELT     * aSource,
                                             qmgSELT    ** aTarget,
                                             qcmIndex    * aNewSelectedIndex,
                                             UInt          aWhichOne )
{
/****************************************************************************
 *
 * Description : qmgJoin���� ANTI�� ���õ� ���
 *               ���� SELT graph�� �����ؾ� �Ѵ�.
 *               ������ ���� �������� predicate�� ���� ������,
 *               ������ �� � graph�� access method�� �ٲ����� ���� ó����,
 *               scanDecisionFactor�� ó�� �� ������ ��Ҹ� ó���ϱ� ���ؼ�
 *               ���� �˰����� qmgSelection���� �����Ѵ�.
 *               SELT graph�� �����ؾ��� �ʿ䰡 ���� ���
 *               �ݵ�� qmgSelection���� �����ؼ� ȣ���ϵ��� �Ѵ�.
 *
 *               aWhich = 0 : target�� access method�� �ٲ۴�.
 *               aWhich = 1 : source�� access method�� �ٲ۴�.
 *
 * Implementation : aSource�� aTarget�� ������ ��,
 *                  selectedIndex�� �ٲ� graph�� ���ؼ���
 *                  sdf�� ��ȿȭ �Ѵ�.
 *
 *****************************************************************************/

    IDU_FIT_POINT_FATAL( "qmgSelection::copySELTAndAlterSelectedIndex::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgSELT ), (void**)aTarget )
              != IDE_SUCCESS );

    idlOS::memcpy( *aTarget, aSource, ID_SIZEOF( qmgSELT ) );

    if( aWhichOne == 0 )
    {
        (*aTarget)->sdf = NULL;
        IDE_TEST( alterSelectedIndex( aStatement,
                                      *aTarget,
                                      aNewSelectedIndex )
                  != IDE_SUCCESS );
    }
    else if( aWhichOne == 1 )
    {
        if( aSource->sdf != NULL )
        {
            aSource->sdf->baseGraph = *aTarget;
            aSource->sdf = NULL;
        }
        else
        {
            // Nothing to do...
        }
        IDE_TEST( alterSelectedIndex( aStatement,
                                      aSource,
                                      aNewSelectedIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::optimizePartition( qcStatement * aStatement, qmgGraph * aGraph )
{
    qmgSELT         * sMyGraph;
    qmsPartitionRef * sPartitionRef;
    qcmColumn       * sColumns;
    UInt              sColumnCnt;
    qmoPredicate    * sRidPredicate;

    UInt              sIndexCnt;
    UInt              sSelectedScanHint;

    SDouble           sOutputRecordCnt;
    SDouble           sRecordSize;

    UInt              i;

    IDU_FIT_POINT_FATAL( "qmgSelection::optimizePartition::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgSELT*) aGraph;
    sPartitionRef = sMyGraph->partitionRef;
    sRecordSize = 0;

    //---------------------------------------------------
    // ���� ��� ������ ����
    //---------------------------------------------------

    // recordSize ����
    sColumns = sPartitionRef->partitionInfo->columns;
    sColumnCnt = sPartitionRef->partitionInfo->columnCount;

    for ( i = 0; i < sColumnCnt; i++ )
    {
        sRecordSize += sColumns[i].basicInfo->column.size;
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // inputRecordCnt ����
    sMyGraph->graph.costInfo.inputRecordCnt =
        sPartitionRef->statInfo->totalRecordCnt;

    //---------------------------------------------------
    // Predicate�� ���ġ �� ���� Predicate�� Selectivity ���
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        IDE_TEST(
            qmoPred::relocatePredicate(
                aStatement,
                sMyGraph->graph.myPredicate,
                & sMyGraph->graph.depInfo,
                & sMyGraph->graph.myQuerySet->outerDepInfo,
                sPartitionRef->statInfo,
                & sMyGraph->graph.myPredicate )
            != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sIndexCnt = sPartitionRef->partitionInfo->indexCount;
    sRidPredicate = sMyGraph->graph.ridPredicate;

    //---------------------------------------------------
    // accessMethod ����
    //---------------------------------------------------

    if (sRidPredicate != NULL)
    {
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                     ID_SIZEOF(qmoAccessMethod),
                     (void**)&sMyGraph->accessMethod)
                 != IDE_SUCCESS);

        sMyGraph->accessMethod->method               = NULL;
        sMyGraph->accessMethod->keyRangeSelectivity  = 0;
        sMyGraph->accessMethod->keyFilterSelectivity = 0;
        sMyGraph->accessMethod->filterSelectivity    = 0;
        sMyGraph->accessMethod->methodSelectivity    = 0;

        sMyGraph->accessMethod->totalCost = qmoCost::getTableRIDScanCost(
            sPartitionRef->statInfo,
            &sMyGraph->accessMethod->accessCost,
            &sMyGraph->accessMethod->diskCost );

        sMyGraph->selectedIndex   = NULL;
        sMyGraph->accessMethodCnt = 1;
        sMyGraph->selectedMethod  = &sMyGraph->accessMethod[0];
        sMyGraph->forceIndexScan  = ID_FALSE;
        sMyGraph->forceRidScan    = ID_FALSE;

        sSelectedScanHint = QMG_NOT_USED_SCAN_HINT;
    }
    else
    {
        sMyGraph->accessMethodCnt = sIndexCnt + 1;

        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(
                     ID_SIZEOF(qmoAccessMethod) * (sIndexCnt + 1),
                     (void**) & sMyGraph->accessMethod)
                 != IDE_SUCCESS);

        IDE_TEST(getBestAccessMethod(aStatement,
                                     &sMyGraph->graph,
                                     sPartitionRef->statInfo,
                                     sMyGraph->graph.myPredicate,
                                     sMyGraph->accessMethod,
                                     &sMyGraph->selectedMethod,
                                     &sMyGraph->accessMethodCnt,
                                     &sMyGraph->selectedIndex,
                                     &sSelectedScanHint,
                                     1,
                                     0)
                 != IDE_SUCCESS);

        // To fix BUG-12742
        // hint�� ����� ���� index�� ������ �� ����.
        if( sSelectedScanHint == QMG_USED_SCAN_HINT )
        {
            sMyGraph->forceIndexScan = ID_TRUE;
        }
        else
        {
            sMyGraph->forceIndexScan = ID_FALSE;
        }
        
        sMyGraph->forceRidScan = ID_FALSE;
    }

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    // preserved order �ʱ�ȭ
    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NOT_DEFINED;

    if ( sMyGraph->selectedMethod->method == NULL )
    {
        //---------------------------------------------------
        // FULL SCAN�� ���õ� ���
        //---------------------------------------------------

        if ( ( sMyGraph->graph.flag & QMG_PRESERVED_ORDER_MASK )
             == QMG_PRESERVED_ORDER_NOT_DEFINED )
        {
            if( sSelectedScanHint == QMG_USED_ONLY_FULL_SCAN_HINT )
            {
                //---------------------------------------------------
                // FULL SCAN Hint�� ���õ� ���
                //---------------------------------------------------

                sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
            }
            else
            {
                //---------------------------------------------------
                // cost�� ���� FULL SCAN�� ���õ� ��� :
                //---------------------------------------------------

                if ( sMyGraph->accessMethodCnt > 1)
                {
                    // index�� �����ϴ� ���
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |=
                        QMG_PRESERVED_ORDER_NOT_DEFINED;
                }
                else
                {
                    // index�� ���� ���
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                }
            }
        }
    }
    else
    {
        //---------------------------------------------------
        // INDEX SCAN�� ���õ� ���
        //---------------------------------------------------

        IDE_TEST( makePreservedOrder( aStatement,
                                      sMyGraph->selectedMethod->method,
                                      sPartitionRef->table,
                                      & sMyGraph->graph.preservedOrder )
                  != IDE_SUCCESS );

        sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
        sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
    }

    //---------------------------------------------------
    // ���� ��� ������ ����
    //---------------------------------------------------

    sMyGraph->graph.costInfo.selectivity = 
        sMyGraph->selectedMethod->methodSelectivity;

    // output record count ����
    sOutputRecordCnt =  sMyGraph->accessMethod->methodSelectivity *
        sMyGraph->graph.costInfo.inputRecordCnt;

    sMyGraph->graph.costInfo.outputRecordCnt =
        ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    // myCost, totalCost ����
    sMyGraph->graph.costInfo.myAccessCost =
        sMyGraph->selectedMethod->accessCost;

    sMyGraph->graph.costInfo.myDiskCost =
        sMyGraph->selectedMethod->diskCost;

    sMyGraph->graph.costInfo.myAllCost =
        sMyGraph->selectedMethod->totalCost;

    //---------------------------------------
    // �� ��� ���� ����
    //---------------------------------------

    // 0 ( Child�� Total Cost) + My Cost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->selectedMethod->accessCost;

    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->selectedMethod->diskCost;

    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->selectedMethod->totalCost;

    //---------------------------------------------------
    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // host variable�� ���� ����ȭ�� ���� �غ����
    //---------------------------------------------------
    if( (QCU_HOST_OPTIMIZE_ENABLE == 1) &&
        (sSelectedScanHint == QMG_NOT_USED_SCAN_HINT) &&
        (sMyGraph->accessMethodCnt > 1) &&
        (QCU_PLAN_RANDOM_SEED == 0) )
    {
        IDE_TEST( prepareScanDecisionFactor( aStatement,
                                             sMyGraph )
                  != IDE_SUCCESS );
    }
    else
    {
        // scan hint�� ���Ǹ� ȣ��Ʈ ������ ���� ����ȭ�� �������� �ʴ´�.
        // index�� ���� ��쿡�� ȣ��Ʈ ������ ���� ����ȭ�� �������� �ʴ´�.
        // Nothing to do...
    }

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_HOST_OPTIMIZE_ENABLE );
    
    /* BUG-41134 */
    setParallelScanFlag(aStatement, aGraph);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::makePlanPartition( qcStatement * aStatement, const qmgGraph * /*aParent*/, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *               qmgSelection�� ���� Plan�� �����Ѵ�.
 *               partition�ϳ��� ���� SCAN�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgSelection���� ���� ���������� plan
 *
 *           - ��� Predicate ������ [SCAN]��忡 ���Եȴ�.
 *
 *                 [SCAN]
 *
 ***********************************************************************/

    qmgSELT         * sMyGraph;
    qmnPlan         * sPlan;
    qmsTableRef     * sTableRef;
    qmoSCANInfo       sSCANInfo;

    IDU_FIT_POINT_FATAL( "qmgSelection::makePlanPartition::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sSCANInfo.flag = QMO_SCAN_INFO_INITIALIZE;
    sMyGraph       = (qmgSELT *) aGraph;

    //---------------------------------------------------
    // Current CNF�� ���
    //---------------------------------------------------

    if( sMyGraph->graph.myCNF != NULL )
    {
        sMyGraph->graph.myQuerySet->SFWGH->crtPath->currentCNF =
            sMyGraph->graph.myCNF;
    }
    else
    {
        // Nothing To Do
    }

    //-----------------------------------------------------
    // 1. Base Table�� ���� Selection Graph�� ���
    //-----------------------------------------------------

    // ���� ��尡 ���� leaf�� base�� ��� SCAN�� �����Ѵ�.

    // To Fix PR-11562
    // Indexable MIN-MAX ����ȭ�� ����� ���
    // Preserved Order�� ���⼺�� ����, ���� �ش� ������
    // �������� �ʿ䰡 ����.
    // INDEXABLE Min-Max�� ����
    // ���� �ڵ� ����

    //-----------------------------------------------------
    // To Fix BUG-8747
    // Selection Graph�� Not Null Key Range�� �����϶�� Flag��
    // �ִ� ���, Leaf Info�� �� ������ �����Ѵ�.
    // - Selection Graph���� Not Null Key Range ���� Flag ����Ǵ� ����
    //   (1) indexable Min Max�� ����� Selection Graph
    //   (2) Merge Join ������ Selection Graph
    //-----------------------------------------------------

    if( (sMyGraph->graph.flag & QMG_SELT_NOTNULL_KEYRANGE_MASK ) ==
        QMG_SELT_NOTNULL_KEYRANGE_TRUE )
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_NOTNULL_KEYRANGE_TRUE;
    }
    else
    {
        // To Fix PR-10288
        // NOTNULL KEY RANGE�� �ƴ� ���� �ݵ�� ������ �־�� ��.
        sSCANInfo.flag &= ~QMO_SCAN_INFO_NOTNULL_KEYRANGE_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_NOTNULL_KEYRANGE_FALSE;
    }

    if (sMyGraph->forceIndexScan == ID_TRUE)
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_INDEX_SCAN_TRUE;
    }
    else
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_INDEX_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_INDEX_SCAN_FALSE;
    }

    if (sMyGraph->forceRidScan == ID_TRUE)
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_RID_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_RID_SCAN_TRUE;
    }
    else
    {
        sSCANInfo.flag &= ~QMO_SCAN_INFO_FORCE_RID_SCAN_MASK;
        sSCANInfo.flag |= QMO_SCAN_INFO_FORCE_RID_SCAN_FALSE;
    }

    sSCANInfo.predicate         = sMyGraph->graph.myPredicate;
    sSCANInfo.constantPredicate = sMyGraph->graph.constantPredicate;
    sSCANInfo.ridPredicate      = sMyGraph->graph.ridPredicate;
    sSCANInfo.limit             = sMyGraph->limit;
    sSCANInfo.index             = sMyGraph->selectedIndex;
    sSCANInfo.preservedOrder    = sMyGraph->graph.preservedOrder;
    sSCANInfo.sdf               = sMyGraph->sdf;
    sSCANInfo.nnfFilter         = sMyGraph->graph.nnfFilter;

    /* BUG-39306 partial scan */
    sTableRef = sMyGraph->graph.myFrom->tableRef;
    if ( sTableRef->tableAccessHints != NULL )
    {
        sSCANInfo.mParallelInfo.mDegree = sTableRef->tableAccessHints->count;
        sSCANInfo.mParallelInfo.mSeqNo  = sTableRef->tableAccessHints->id;
    }
    else
    {
        sSCANInfo.mParallelInfo.mDegree = 1;
        sSCANInfo.mParallelInfo.mSeqNo  = 1;
    }
    
    //SCAN����
    //������ ����� ��ġ�� �ݵ�� graph.myPlan�� ������ �ϵ��� �Ѵ�.
    //�� ������ �ٽ� child�� ��� ���� ��� ���鶧 �����ϵ��� �Ѵ�.
    // partition�� ���� scan
    // partitionRef�� ���ڷ� �Ѱ��־�� �Ѵ�.
    IDE_TEST( qmoOneNonPlan::makeSCAN4Partition(
                  aStatement,
                  sMyGraph->graph.myQuerySet,
                  sMyGraph->graph.myFrom,
                  &sSCANInfo,
                  sMyGraph->partitionRef,
                  &sPlan )
              != IDE_SUCCESS);

    // fix BUG-13482
    // SCAN ��� ������,
    // filter��������� SCAN Limit ����ȭ�� �������� ���� ���,
    // selection graph�� limit�� NULL�� �����Ѵ�.
    // �̴� ���� PROJ ��� ������, limit start value ������ ������ ��.
    if( sSCANInfo.limit == NULL )
    {
        sMyGraph->limit = NULL;
    }
    else
    {
        // Nothing To Do
    }

    sMyGraph->graph.myPlan = sPlan;

    qmg::setPlanInfo( aStatement, sPlan, &(sMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::printGraphPartition( qcStatement  * aStatement,
                                   qmgGraph     * aGraph,
                                   ULong          aDepth,
                                   iduVarString * aString )
{
    qmgSELT * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSelection::printGraphPartition::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );
    IDE_DASSERT( aGraph->myFrom->tableRef->view == NULL );
    IDE_DASSERT( aGraph->type == QMG_SELECTION );

    sMyGraph = (qmgSELT*) aGraph;

    IDE_DASSERT( sMyGraph->partitionRef != NULL );

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

    IDE_TEST( qmoStat::printStat4Partition( sMyGraph->graph.myFrom->tableRef,
                                            sMyGraph->partitionRef,
                                            sMyGraph->partitionName,
                                            aDepth,
                                            aString )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::setJoinPushDownPredicate( qmgSELT       * aGraph,
                                        qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 *  Description : push-down join predicate�� �� �տ� �����Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sJoinPredicate;

    IDU_FIT_POINT_FATAL( "qmgSelection::setJoinPushDownPredicate::__FT__" );

    IDE_FT_ASSERT( aPredicate != NULL );

    IDE_TEST_RAISE( *aPredicate == NULL,
                    ERR_NOT_EXIST_PREDICATE );

    //--------------------------------------
    // join predicate�� ���Ḯ��Ʈ��
    // selection graph�� predicate ���Ḯ��Ʈ �� ó���� �����Ų��.
    //--------------------------------------

    for( sJoinPredicate       = *aPredicate;
         sJoinPredicate->next != NULL;
         sJoinPredicate       = sJoinPredicate->next ) ;

    sJoinPredicate->next = aGraph->graph.myPredicate;
    aGraph->graph.myPredicate  = *aPredicate;
    *aPredicate       = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_PREDICATE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgSelection::setJoinPushDownPredicate",
                                  "not exist predicate" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::setNonJoinPushDownPredicate( qmgSELT       * aGraph,
                                           qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 *  Description : non-join push predicate�� selection graph�� ����.
 *
 *  Implementation :
 *     ���ڷ� ���� non-joinable predicate�� keyFilter or filter��
 *     ó���Ǿ�� �Ѵ�.
 *     ����, keyRange�� ����� joinable predicate���� ������� �ʵ��� �Ѵ�.
 *
 *     1. joinable predicate�� ����
 *        joinable predicate�� selection graph predicate�� ó���κп�
 *        ����Ǿ� �ֱ� ������, masking ������ �̿��ؼ�, joinable predicate��
 *        �����Ѵ�.
 *     2. non-joinable predicate�� ������ �÷��� ã�Ƽ� ����.
 ***********************************************************************/

    qmoPredicate * sJoinPredicate;
    qmoPredicate * sPredicate;
    qmoPredicate * sNextPredicate;

    IDU_FIT_POINT_FATAL( "qmgSelection::setNonJoinPushDownPredicate::__FT__" );

    //--------------------------------------
    // selection graph predicate�� ���Ḯ��Ʈ ��
    // non-joinable predicate�� columnID�� ������ predicate�� ���Ḯ��Ʈ��
    // non-joinable predicate�� �����Ѵ�.
    //--------------------------------------

    // sPredicate : index joinable predicate�� ������
    //              ù��° predicate�� ����Ű�� �ȴ�.

    sJoinPredicate = *aPredicate;

    if( sJoinPredicate == NULL )
    {
        // PROJ-1502 PARTITIONED DISK TABLE
        // subquery�� ���Ե� predicate�� partition graph���� ó���ϹǷ�,
        // non-join push down predicate�� NULL�� �� �ִ�.
        // Nothing to do.
    }
    else
    {
        if( aGraph->graph.myPredicate == NULL )
        {
            // selection graph�� predicate�� �ϳ��� ���� ����,
            // ù��° non-joinable predicate �����ϰ�,
            // ����� predicate�� next ������ ���´�.
            aGraph->graph.myPredicate = sJoinPredicate;
            sJoinPredicate = sJoinPredicate->next;
            aGraph->graph.myPredicate->next = NULL;
            sPredicate = aGraph->graph.myPredicate;
        }
        else
        {
            // selection graph�� predicate�� �ִ� ����,
            // Index Nested Loop Joinable Predicate���� �����Ѵ�.
            // non-joinable predicate�� keyFilter or filter�� ó���Ǿ�� �ϹǷ�,
            // keyRange�� ����� Index Nested Loop Join Predicate���� ������ ����.

            for( sPredicate = aGraph->graph.myPredicate;
                 sPredicate->next != NULL;
                 sPredicate = sPredicate->next )
            {
                if( ( sPredicate->flag & QMO_PRED_INDEX_NESTED_JOINABLE_MASK )
                    != QMO_PRED_INDEX_NESTED_JOINABLE_TRUE )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            // �� ó�� ��������, selection graph�� joinable predicate�� �ִ� ���,
            // ������ joinable predicate�� next�� non-joinable predicate�� ����,
            // ����� non-joinable predicate�� next ������ ���´�.
            if( ( sPredicate->flag & QMO_PRED_INDEX_NESTED_JOINABLE_MASK )
                == QMO_PRED_INDEX_NESTED_JOINABLE_TRUE )
            {
                sPredicate->next = sJoinPredicate;
                sJoinPredicate = sJoinPredicate->next;
                sPredicate = sPredicate->next;
                sPredicate->next = NULL;
            }
            else
            {
                // Nothing To Do
            }
        }

        // Index Nested Loop Joinable Predicate�� ������
        // selection graph predicate ����Ʈ���� non-joinable predicate��
        // columnID�� ���� predicate�� non-joinable predicate�� �����ϰ�,
        // ����� non-joinable�� next�� ������ ���´�.

        while( sJoinPredicate != NULL )
        {
            // joinable predicate�� ������ predicate�߿��� join predicate��
            // ���� �÷��� �����ϴ����� �˻�.
            // sPredicate : index joinable predicate�� ������
            //              ù��° predicate�� ����Ų��.

            // �÷����� ������� �����
            // ���� �÷��� �ִ� ���, ���� �÷��� ������ predicate.more��
            // ���� �÷��� ���� ���, sPredicate�� ������ predicate.next��
            // (1) ���ο� predicate(sJoinPredicate)�� �����ϰ�,
            // (2) sJoinPredicate = sJoinPredicate->next
            // (3) ����� predicate�� next ������踦 ����.

            sNextPredicate = sJoinPredicate->next;

            IDE_TEST( qmoPred::linkPred4ColumnID( sPredicate,
                                                  sJoinPredicate ) != IDE_SUCCESS );
            sJoinPredicate = sNextPredicate;
        }

        //---------------------------------------------------
        // To Fix BUG-13292
        // Subquery�� �ٸ� Predicate�� �Բ� key range�� ������ �� ����.
        // �׷��� ���� ������ ���� ���, �Բ� �����ǰ� �ȴ�.
        // ex) ���õ� Join Method : Full Nested Loop Join
        //     right Graph        : selection graph
        //     ==> full nested loop join�� join predicate�� right�� �����ٶ�
        //         in subquery�� key range�� ���õǸ� �ܵ����� key range��
        //         ������ �� �ֵ��� ó�����־�� �Ѵ�.
        //         �̿� ���� �۾��� ���ִ� �Լ��� processIndexableSubQ()�̴�.
        //---------------------------------------------------

        IDE_TEST( qmoPred::processIndexableInSubQ( &aGraph->graph.myPredicate )
                  != IDE_SUCCESS );


        *aPredicate = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::alterForceRidScan( qcStatement * aStatement,
                                 qmgGraph    * aGraph )
{
/***********************************************************************
 *
 * Description : PROJ-1624 global non-partitioned index
 *               partition graph�� ����ȭ�Ϸ� �� global index scan��
 *               ���õǸ� global index scan�� graph�� �����Ѵ�.
 *
 * Implementation :
 *      - predicate �� ��� ���� �ʱ�ȭ
 *      - sdf ����
 *
 ***********************************************************************/

    qmgSELT         * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSelection::alterForceRidScan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgSELT *) aGraph;
    
    //---------------------------------------------------
    // �ʱ�ȭ
    //---------------------------------------------------
    
    // Selection Graph�� Not Null Key Range ���� Flag �ʱ�ȭ
    sMyGraph->graph.flag &= ~QMG_SELT_NOTNULL_KEYRANGE_MASK;
    sMyGraph->graph.flag |= QMG_SELT_NOTNULL_KEYRANGE_FALSE;

    sMyGraph->graph.myPredicate       = NULL;
    sMyGraph->graph.constantPredicate = NULL;
    sMyGraph->graph.ridPredicate      = NULL;
    sMyGraph->limit                   = NULL;
    sMyGraph->selectedIndex           = NULL;
    sMyGraph->graph.preservedOrder    = NULL;
    sMyGraph->graph.nnfFilter         = NULL;
    sMyGraph->forceIndexScan          = ID_FALSE;
    sMyGraph->forceRidScan            = ID_TRUE;   // ���� rid scan ����
    
    if( sMyGraph->sdf != NULL )
    {
        // ���� selection graph�� selectedIndex��
        // ���� graph�� ���� �ٽ� ������ ���
        // host optimization�� �ؼ��� �ȵȴ�.
        // �� ��� sdf�� disable�Ѵ�.
        IDE_TEST( qtc::removeSDF( aStatement, sMyGraph->sdf ) != IDE_SUCCESS );

        sMyGraph->sdf = NULL;
    }
    else
    {
        // Nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSelection::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Preserved Order�� direction�� �����Ѵ�.
 *                direction�� NOT_DEFINED �� ��쿡�� ȣ���Ͽ��� �Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qmgSELT           * sMyGraph;
    qmgPreservedOrder * sPreservedOrder;
    qmgPreservedOrder * sChildOrder;
    UInt              * sKeyColsFlag;
    UInt                sKeyColCount;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmgSelection::finalizePreservedOrder::__FT__" );

    sMyGraph = (qmgSELT*) aGraph;

    if ( aGraph->left != NULL )
    {
        // View�� ���, ������ Preserved Order�� ����
        // View�� table, column���� ��ȯ�ϱ� ������
        //  �� Preserved order�� table�� column�� �ٸ� �� �ִ�.
        sPreservedOrder = aGraph->preservedOrder;
        sChildOrder     = aGraph->left->preservedOrder;
        for ( ; sPreservedOrder != NULL && sChildOrder != NULL;
              sPreservedOrder = sPreservedOrder->next,
                  sChildOrder = sChildOrder->next )
        {
            sPreservedOrder->direction= sChildOrder->direction;
        }
    }
    else
    {
        // View�� �ƴϰ� Preserved order�� �����ϴ� ���� Index scan���̴�.
        sKeyColCount = sMyGraph->selectedIndex->keyColCount;
        sKeyColsFlag = sMyGraph->selectedIndex->keyColsFlag;
    
        // Selected index�� order�� direction�� �����Ѵ�.
        for ( sPreservedOrder = aGraph->preservedOrder,
                  i = 0;
              sPreservedOrder != NULL &&
                  i < sKeyColCount;
              sPreservedOrder = sPreservedOrder->next,
                  i++ )
        {
            // Direction copy
            if ( ( sKeyColsFlag[i] & SMI_COLUMN_ORDER_MASK ) ==
                 SMI_COLUMN_ORDER_ASCENDING )
            {
                sPreservedOrder->direction = QMG_DIRECTION_ASC;
            }
            else
            {
                sPreservedOrder->direction = QMG_DIRECTION_DESC;
            }
        }
    }

    return IDE_SUCCESS;
}

IDE_RC qmgSelection::makeRecursiveViewScan( qcStatement * aStatement,
                                            qmgSELT     * aMyGraph )
{
    /***********************************************************************
     *
     * Description : PROJ-2582 recursive with
     *
     * Implementation :
     *   VSCAN ����.
     *
     *   right query
     *     [FILT]
     *       |
     *     [VSCN]
     *
     ***********************************************************************/

    qtcNode  * sFilter = NULL;
    qmnPlan  * sFILT   = NULL;
    qmnPlan  * sVSCN   = NULL;

    IDU_FIT_POINT_FATAL( "qmgSelection::makeRecursiveViewScan::__FT__" );

    //---------------------------------------
    // predicate�� ���� ������ ����
    // FILT ����
    //---------------------------------------

    if( ( aMyGraph->graph.myPredicate != NULL ) ||
        ( aMyGraph->graph.constantPredicate != NULL ) ||
        ( aMyGraph->graph.nnfFilter != NULL ) )
    {
        if( aMyGraph->graph.myPredicate != NULL )
        {
            IDE_TEST( qmoPred::linkFilterPredicate(
                          aStatement ,
                          aMyGraph->graph.myPredicate ,
                          &sFilter ) != IDE_SUCCESS );

            // BUG-35155 Partial CNF
            if ( aMyGraph->graph.nnfFilter != NULL )
            {
                IDE_TEST( qmoPred::addNNFFilter4linkedFilter(
                              aStatement,
                              aMyGraph->graph.nnfFilter,
                              & sFilter ) != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            if ( aMyGraph->graph.nnfFilter != NULL )
            {
                sFilter = aMyGraph->graph.nnfFilter;
            }
            else
            {
                sFilter = NULL;
            }
        }

        //make FILT
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFilter ,
                      aMyGraph->graph.myPlan ,
                      &sFILT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;
    }
    else
    {
        // Nothing To Do
    }

    IDE_TEST( qmoOneNonPlan::initVSCN( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myFrom ,
                                       aMyGraph->graph.myPlan ,
                                       &sVSCN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sVSCN;    

    //---------------------------------------
    // VSCN����
    //---------------------------------------

    // codesonar::Null Pointer Dereference
    IDE_FT_ERROR( aMyGraph->graph.left != NULL );
    
    // right VSCN�� LEFT �� �ֻ��� VMTR�� child�� ������.
    IDE_TEST( qmoOneNonPlan::makeVSCN( aStatement,
                                       aMyGraph->graph.myQuerySet,
                                       aMyGraph->graph.myFrom,
                                       aMyGraph->graph.left->myPlan, // child
                                       sVSCN ) != IDE_SUCCESS );
    aMyGraph->graph.myPlan = sVSCN;

    qmg::setPlanInfo( aStatement, sVSCN, &(aMyGraph->graph) );
    
    //---------------------------------------
    // predicate�� ���� ������ ����
    // FILT ����
    //---------------------------------------

    if( ( aMyGraph->graph.myPredicate != NULL ) ||
        ( aMyGraph->graph.constantPredicate != NULL ) ||
        ( aMyGraph->graph.nnfFilter != NULL ) )
    {
        //make FILT
        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      aMyGraph->graph.myQuerySet ,
                      sFilter ,
                      aMyGraph->graph.constantPredicate ,
                      aMyGraph->graph.myPlan ,
                      sFILT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo( aStatement, sFILT, &(aMyGraph->graph) );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* TASK-7219 Non-shard DML */
void qmgSelection::isForcePushedPredForShardView( mtcNode * aNode,
                                                  idBool  * aIsFound )
{
    if ( ( aNode != NULL ) &&
         ( *aIsFound == ID_FALSE ) )
    {
        if ( ( aNode->lflag  & MTC_NODE_PUSHED_PRED_FORCE_MASK )
             == MTC_NODE_PUSHED_PRED_FORCE_TRUE )
        {
            *aIsFound = ID_TRUE;
        }
        else
        {
            isForcePushedPredForShardView( aNode->arguments,
                                           aIsFound );

            isForcePushedPredForShardView( aNode->next,
                                           aIsFound );
        }
    }
}
