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
 * $Id: qmgPartition.cpp 20233 2007-02-01 01:58:21Z shsuh $
 *
 * Description :
 *     Partition Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgPartition.h>
#include <qcg.h>
#include <qmgSelection.h>
#include <qmoMultiNonPlan.h>
#include <qmo.h>
#include <qmoCost.h>
#include <qcuProperty.h>
#include <qmoPartition.h>
#include <qcmPartition.h>
#include <qtc.h>
#include <qcgPlan.h>
#include <qmoParallelPlan.h>

/* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
#define QMG_REDUCE_PART_PREPARE_MEM_DISK_TUPLE_MASK   ( 0x00000001 )
#define QMG_REDUCE_PART_PREPARE_MEM_DISK_TUPLE_FALSE  ( 0x00000000 )
#define QMG_REDUCE_PART_PREPARE_MEM_DISK_TUPLE_TRUE   ( 0x00000001 )

#define QMG_REDUCE_PART_PREPARE_MEM_STAT_COLUMN_MASK  ( 0x00000002 )
#define QMG_REDUCE_PART_PREPARE_MEM_STAT_COLUMN_FALSE ( 0x00000000 )
#define QMG_REDUCE_PART_PREPARE_MEM_STAT_COLUMN_TRUE  ( 0x00000002 )

IDE_RC
qmgPartition::init( qcStatement * aStatement,
                    qmsQuerySet * aQuerySet,
                    qmsFrom     * aFrom,
                    qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgPartition Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgPartition�� ���� ���� �Ҵ� ����
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgPARTITION  * sMyGraph;
    qmsTableRef   * sTableRef;

    IDU_FIT_POINT_FATAL( "qmgPartition::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aFrom != NULL );

    //---------------------------------------------------
    // Partition Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgPartition�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPARTITION ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );


    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_PARTITION;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aFrom->depInfo );
    sMyGraph->graph.myFrom = aFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgPartition::optimize;
    sMyGraph->graph.makePlan = qmgPartition::makePlan;
    sMyGraph->graph.printGraph = qmgPartition::printGraph;

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

    //---------------------------------------------------
    // Partition ���� ������ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->limit = NULL;

    sMyGraph->partKeyRange = NULL;
    sMyGraph->partFilterPredicate = NULL;
    
    sMyGraph->selectedIndex = NULL;
    sMyGraph->selectedMethod = NULL;
    sMyGraph->accessMethodCnt = 0;
    sMyGraph->accessMethod = NULL;

    sMyGraph->forceIndexScan = ID_FALSE;

    // BUG-48800
    sMyGraph->mPrePruningPartRef = NULL;    

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgPartition�� ����ȭ
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPARTITION    * sMyGraph;
    qmsTableRef     * sTableRef;
    qmsPartitionRef * sPartitionRef;
    qmoPredicate    * sPredicate;
    qmoPredicate    * sMyPredicate;
    qmoPredicate    * sPartKeyRangePred;
    qtcNode         * sPartKeyRangeNode;
    smiRange        * sPartKeyRange;
    qmgGraph       ** sChildGraph;
    UInt              sPartitionCount;
    UInt              i;
    UInt              sFlag;    
    UShort            sColumnCnt;
    mtcTuple        * sMtcTuple;    
    UInt              sSelectedScanHint;
    idBool            sIsIndexTableScan = ID_FALSE;
    idBool            sExistIndexTable = ID_FALSE;
    idBool            sIsLocked = ID_FALSE;
    SDouble           sOutputRecordCnt;
    mtcColumn       * sColumns = NULL;
    qmoStatistics   * sStatInfo = NULL;

    IDU_FIT_POINT_FATAL( "qmgPartition::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph          = (qmgPARTITION*) aGraph;
    sTableRef         = sMyGraph->graph.myFrom->tableRef;
    sPredicate        = NULL;
    sMyPredicate      = NULL;
    sPartKeyRangePred = NULL;
    sPartKeyRange     = NULL;
    sPartitionCount   = 0;
    i                 = 0;

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
    
    if ( sMyGraph->graph.myPredicate != NULL )
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
        IDE_TEST(
            qmoPred::optimizeSubqueries( aStatement,
                                         sMyGraph->graph.myPredicate,
                                         ID_TRUE ) // Use KeyRange Tip
            != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------
    // partition keyrange���� �� �˻�
    //---------------------------------------------------

    // �ϴ� ��� ��Ƽ�ǿ� ���� ������ �����Ѵ�.
    IDE_TEST(
        qmoPartition::makePartitions(
            aStatement,
            sTableRef )
        != IDE_SUCCESS );

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        // partition pruning�� ���� predicate�� ����.
        // fixed predicate�� ���ؼ��� partition keyrange�� ������
        // �� �����Ƿ�,
        // subquery, variable predicate�� �����ϰ� �����Ѵ�.

        // partition key range ��¿�
        IDE_TEST(qmoPred::copyPredicate4Partition(
                     aStatement,
                     sMyGraph->graph.myPredicate,
                     &sPredicate,
                     sTableRef->table,
                     sTableRef->table,
                     ID_FALSE )
                 != IDE_SUCCESS );

        // predicate�� �����Ͽ��� ������ intermediate tuple�� ���� �Ҵ���.
        IDE_TEST( qtc::fixAfterValidation( QC_QMP_MEM(aStatement),
                                           QC_SHARED_TMPLATE(aStatement) )
                  != IDE_SUCCESS );
        
        // subquery predicate�� �����Ͽ��� ������ null�� �� ����.
        if( sPredicate != NULL )
        {
            // predicate�� ���ġ(������� �����ϰ� �׳� ���ġ�� �Ѵ�.)
            IDE_TEST( qmoPred::relocatePredicate4PartTable(
                          aStatement,
                          sPredicate,
                          sTableRef->tableInfo->partitionMethod,
                          & sMyGraph->graph.depInfo,
                          & sMyGraph->graph.myQuerySet->outerDepInfo,
                          & sPredicate )
                      != IDE_SUCCESS );
            
            // partition keyrange predicate ����.
            IDE_TEST( qmoPred::makePartKeyRangePredicate(
                          aStatement,
                          sMyGraph->graph.myQuerySet,
                          sPredicate,
                          sTableRef->tableInfo->partKeyColumns,
                          sTableRef->tableInfo->partitionMethod,
                          &sPartKeyRangePred )
                      != IDE_SUCCESS );

            // partition keyrange predicate�κ��� partition keyrange����.
            IDE_TEST( extractPartKeyRange(
                          aStatement,
                          sMyGraph->graph.myQuerySet,
                          &sPartKeyRangePred,
                          sTableRef->tableInfo->partKeyColCount,
                          sTableRef->tableInfo->partKeyColBasicInfo,
                          sTableRef->tableInfo->partKeyColsFlag,
                          &sPartKeyRangeNode,
                          &sPartKeyRange )
                      != IDE_SUCCESS );

            // sPartKeyRangeNode�� ��¿��̴�.
            // sPartKeyRange�� ����׿�.
        }
        else
        {
            // Nothing to do.
        }

        if( sPartKeyRange != NULL )
        {
            // BUG-48800 ����� ��Ƽ���� ��Ƽ�� ���͸��Ǹ� �÷��� rebuild���� �ʽ��ϴ�.
            if ( (sTableRef->flag & QMS_TABLE_REF_PRE_PRUNING_MASK)
                 == QMS_TABLE_REF_PRE_PRUNING_TRUE ) 
            {
                sMyGraph->mPrePruningPartRef = sTableRef->partitionRef;
            }

            /* BUG-47521 hybrid partition foreignkey bug */
            if ( ( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_UPDATE ) &&
                 ( sTableRef->tableInfo->rowMovement == ID_TRUE ) &&
                 ( ( sTableRef->tableInfo->foreignKeyCount > 0 ) ||
                   ( sTableRef->tableInfo->triggerCount > 0 ) ) )
            {
                IDE_TEST( qcmPartition::validateAndLockPartitions(
                              aStatement,
                              sTableRef,
                              SMI_TABLE_LOCK_IS )
                          != IDE_SUCCESS );
                sIsLocked = ID_TRUE;

                /* PROJ-2464 hybrid partitioned table ���� */
                IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                          != IDE_SUCCESS );

                if ( sTableRef->partitionSummary->isHybridPartitionedTable == ID_FALSE )
                {
                    IDE_TEST( qmoPartition::partitionPruningWithKeyRange(
                                  aStatement,
                                  sTableRef,
                                  sPartKeyRange )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                IDE_TEST( qmoPartition::partitionPruningWithKeyRange(
                              aStatement,
                              sTableRef,
                              sPartKeyRange )
                          != IDE_SUCCESS );
            }

            sMyGraph->partKeyRange = sPartKeyRangeNode;
        }
        else
        {
            // partition keyrange�� ����.
            // pruning�� �� �ʿ䰡 ����.
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // partition�� ��� ���� ����
    //---------------------------------------------------
    if ( sIsLocked == ID_FALSE )
    {
        IDE_TEST( qcmPartition::validateAndLockPartitions(
                      aStatement,
                      sTableRef,
                      SMI_TABLE_LOCK_IS )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2464 hybrid partitioned table ���� */
    IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
              != IDE_SUCCESS );

    // ���õ� ��Ƽ���� ������ ���ϸ鼭
    // ��������� ���Ѵ�.
    // Ʃ�� ���� �Ҵ��Ѵ�.
    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    if ( ( sTableRef->partitionSummary->isHybridPartitionedTable != ID_TRUE ) &&
         ( sTableRef->tableInfo->lobColumnCount == 0 ) &&
         ( QCM_TABLE_TYPE_IS_DISK(sTableRef->tableInfo->tableFlag) == ID_TRUE ) &&
         ( ( QCG_GET_REDUCE_PART_PREPARE_MEMORY( aStatement ) & QMG_REDUCE_PART_PREPARE_MEM_DISK_TUPLE_MASK )
            == QMG_REDUCE_PART_PREPARE_MEM_DISK_TUPLE_TRUE ) &&
         ( ( aStatement->myPlan->parseTree->stmtKind & QCI_STMT_MASK_MASK )
           == QCI_STMT_MASK_DML ) &&
         ( ( aStatement->mFlag & QC_STMT_NO_PART_COLUMN_REDUCE_MASK )
           != QC_STMT_NO_PART_COLUMN_REDUCE_TRUE ) )
    {
        sColumns = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTableRef->table].columns;
    }
    else
    {
        sColumns = NULL;
    }

    for ( sPartitionCount = 0, sPartitionRef = sTableRef->partitionRef;
          sPartitionRef != NULL;
          sPartitionCount++, sPartitionRef = sPartitionRef->next )
    {
        IDE_TEST( qmoStat::getStatInfo4BaseTable( aStatement,
                                                  sMyGraph->graph.myQuerySet->SFWGH->hints->optGoalType,
                                                  sPartitionRef->partitionInfo,
                                                  &(sPartitionRef->statInfo) )
                  != IDE_SUCCESS );

        IDE_TEST(qtc::nextTable( &(sPartitionRef->table),
                                 aStatement,
                                 sPartitionRef->partitionInfo,
                                 QCM_TABLE_TYPE_IS_DISK(sPartitionRef->partitionInfo->tableFlag),
                                 MTC_COLUMN_NOTNULL_TRUE )
                 != IDE_SUCCESS);

        if ( sColumns != NULL )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->free( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].columns)
                      != IDE_SUCCESS );

            QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].columns = sColumns;
        }
        else
        {
            /* Nothing to do */
        }

        QC_SHARED_TMPLATE(aStatement)->tableMap[sPartitionRef->table].from = sMyGraph->graph.myFrom;
    }

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_REDUCE_PART_PREPARE_MEMORY );

    // index table�� ��������� ���Ѵ�.
    for ( i = 0; i < sTableRef->indexTableCount; i++ )
    {
        IDE_TEST( qmoStat::getStatInfo4BaseTable(
                      aStatement,
                      sMyGraph->graph.myQuerySet->SFWGH->hints->optGoalType,
                      sTableRef->indexTableRef[i].tableInfo,
                      &(sTableRef->indexTableRef[i].statInfo) )
                  != IDE_SUCCESS );
    }

    // ������ partition�� ���� ��� ������ ���� ��,
    // �̸� �̿��Ͽ� partitioned table�� ��� ������ ���Ѵ�.
    IDE_TEST( qmoStat::getStatInfo4PartitionedTable(
                  aStatement,
                  sMyGraph->graph.myQuerySet->SFWGH->hints->optGoalType,
                  sTableRef,
                  sTableRef->statInfo )
              != IDE_SUCCESS );

    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    if ( ( QCG_GET_REDUCE_PART_PREPARE_MEMORY( aStatement ) & QMG_REDUCE_PART_PREPARE_MEM_STAT_COLUMN_MASK )
         == QMG_REDUCE_PART_PREPARE_MEM_STAT_COLUMN_TRUE )
    {
        sStatInfo = sTableRef->statInfo;

        /* Partitioned�� ������ column ������ �ʱ�ȭ�ϰ� �� partition��
         * �ִ� column �� �����Ѵ�
         */
        for ( sColumnCnt = 0; sColumnCnt < sStatInfo->columnCnt ; sColumnCnt++ )
        {
            sStatInfo->colCardInfo[sColumnCnt].isValidStat = ID_FALSE;
            sStatInfo->colCardInfo[sColumnCnt].flag &= ~QMO_STAT_MINMAX_COLUMN_SET_MASK;
            sStatInfo->colCardInfo[sColumnCnt].flag |= QMO_STAT_MINMAX_COLUMN_SET_FALSE;
        }
        for ( sPartitionCount = 0, sPartitionRef = sTableRef->partitionRef;
              sPartitionRef != NULL;
              sPartitionCount++, sPartitionRef = sPartitionRef->next )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->free( sPartitionRef->statInfo->colCardInfo )
                      != IDE_SUCCESS );

            sPartitionRef->statInfo->colCardInfo = sStatInfo->colCardInfo;
        }
    }
    //---------------------------------------------------
    // global index�� ���� predicate ����ȭ
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        // selectivity �������� ����
        IDE_TEST(qmoPred::deepCopyPredicate(
                     QC_QMP_MEM(aStatement),
                     sMyGraph->graph.myPredicate,
                     & sMyPredicate )
                 != IDE_SUCCESS );

        // Predicate�� ���ġ �� ���� Predicate�� Selectivity ���
        // selectivity ���� access method �Ի�����θ� ����Ѵ�.
        IDE_TEST( qmoPred::relocatePredicate(
                      aStatement,
                      sMyPredicate,
                      & sMyGraph->graph.depInfo,
                      & sMyGraph->graph.myQuerySet->outerDepInfo,
                      sTableRef->statInfo,
                      & sMyPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    //---------------------------------------------------------------
    // child ����ȭ
    //---------------------------------------------------------------

    if( sPartitionCount >= 1 )
    {
        //���õ� ��Ƽ���� �ּ� �Ѱ� �̻��� �����.

        //---------------------------------------------------------------
        // child graph�� ����
        //---------------------------------------------------------------

        // graph->children����ü�� �޸� �Ҵ�.
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                      ID_SIZEOF(qmgChildren) * sPartitionCount,
                      (void**) &sMyGraph->graph.children )
                  != IDE_SUCCESS );

        // child graph pointer�� �޸� �Ҵ�.
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                      ID_SIZEOF(qmgGraph *) * sPartitionCount,
                      (void**) &sChildGraph )
                  != IDE_SUCCESS );

        for( i = 0, sPartitionRef = sTableRef->partitionRef;
             (sPartitionRef != NULL) && (i < sPartitionCount);
             i++, sPartitionRef = sPartitionRef->next )
        {
            IDE_TEST( qmgSelection::init( aStatement,
                                          sMyGraph->graph.myQuerySet,
                                          sMyGraph->graph.myFrom,
                                          sPartitionRef,
                                          &sChildGraph[i] )
                      != IDE_SUCCESS );

            // child graph�� predicate�� ����.
            // join graph���� �̹� ������ ���� �� ������ ����.
            // subquery predicate�� ���� ������ predicate�� ��� ������.
            // ������ : �����ؼ� ������ ��.
            // TODO1502: in subquery keyrange�� �������°���
            // �� ������ �ȵǴ°��� ö���� ���� �ʿ�.
            IDE_TEST(qmoPred::copyPredicate4Partition(
                         aStatement,
                         sMyGraph->graph.myPredicate,
                         &sChildGraph[i]->myPredicate,
                         sTableRef->table,
                         sPartitionRef->table,
                         ID_TRUE )
                     != IDE_SUCCESS );

            // PROJ-1789 PROWID
            IDE_TEST(qmoPred::copyPredicate4Partition(
                         aStatement,
                         sMyGraph->graph.ridPredicate,
                         &sChildGraph[i]->ridPredicate,
                         sTableRef->table,
                         sPartitionRef->table,
                         ID_TRUE )
                     != IDE_SUCCESS );
        }

        // predicate�� �����Ͽ��� ������ intermediate tuple�� ���� �Ҵ���.
        IDE_TEST( qtc::fixAfterValidation( QC_QMP_MEM(aStatement),
                                           QC_SHARED_TMPLATE(aStatement) )
                  != IDE_SUCCESS );

        //---------------------------------------------------------------
        // child graph�� optimize
        //---------------------------------------------------------------

        for( i = 0, sPartitionRef = sTableRef->partitionRef;
             (sPartitionRef != NULL) && (i < sPartitionCount);
             i++, sPartitionRef = sPartitionRef->next )
        {
            // PROJ-1705
            // validation�� ������ ���ǿ� ���� �÷����� ����

            sMtcTuple = &(QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table]);

            for( sColumnCnt = 0;
                 sColumnCnt <
                     QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTableRef->table].columnCount;
                 sColumnCnt++ )
            {
                sFlag = QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTableRef->table].columns[sColumnCnt].flag;

                /* PROJ-2464 hybrid partitioned table ����
                 *  Partition���� DML Without Retry�� �����ϱ� ����,
                 *  MTC_COLUMN_USE_WHERE_MASK�� MTC_COLUMN_USE_SET_MASK�� Partition Tuple�� �����Ѵ�.
                 */
                sMtcTuple->columns[sColumnCnt].flag &= ~MTC_COLUMN_USE_COLUMN_MASK;
                sMtcTuple->columns[sColumnCnt].flag &= ~MTC_COLUMN_USE_TARGET_MASK;
                sMtcTuple->columns[sColumnCnt].flag &= ~MTC_COLUMN_USE_WHERE_MASK;
                sMtcTuple->columns[sColumnCnt].flag &= ~MTC_COLUMN_USE_SET_MASK;

                sMtcTuple->columns[sColumnCnt].flag |= sFlag & MTC_COLUMN_USE_COLUMN_MASK;
                sMtcTuple->columns[sColumnCnt].flag |= sFlag & MTC_COLUMN_USE_TARGET_MASK;
                sMtcTuple->columns[sColumnCnt].flag |= sFlag & MTC_COLUMN_USE_WHERE_MASK;
                sMtcTuple->columns[sColumnCnt].flag |= sFlag & MTC_COLUMN_USE_SET_MASK;
            }

            // ������ ��Ƽ�� ���� optimize.
            IDE_TEST( sChildGraph[i]->optimize( aStatement,
                                                sChildGraph[i] )
                      != IDE_SUCCESS );

            sMyGraph->graph.children[i].childGraph = sChildGraph[i];

            if( i == sPartitionCount - 1 )
            {
                sMyGraph->graph.children[i].next = NULL;
            }
            else
            {
                sMyGraph->graph.children[i].next =
                    &sMyGraph->graph.children[i+1];
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // accessMethod ����
    //---------------------------------------------------

    // rid predicate �� �ִ� ��� ������ rid scan �� �õ��Ѵ�.
    // rid predicate �� �ִ���  rid scan �� �Ҽ� ���� ��쵵 �ִ�.
    // �� ��쿡�� index scan �� ���� �ʰ� full scan �� �ϰ� �ȴ�.
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

        sSelectedScanHint = QMG_NOT_USED_SCAN_HINT;
    }
    else
    {
        // ��� ������ access method���� �����Ѵ�.
        sMyGraph->accessMethodCnt = sTableRef->tableInfo->indexCount + 1;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoAccessMethod) *
                                                 ( sMyGraph->accessMethodCnt ),
                                                 (void**) & sMyGraph->accessMethod )
                  != IDE_SUCCESS );

        IDE_TEST( qmgSelection::getBestAccessMethod( aStatement,
                                                     & sMyGraph->graph,
                                                     sTableRef->statInfo,
                                                     sMyPredicate,
                                                     sMyGraph->accessMethod,
                                                     & sMyGraph->selectedMethod,
                                                     & sMyGraph->accessMethodCnt,
                                                     & sMyGraph->selectedIndex,
                                                     & sSelectedScanHint,
                                                     1,
                                                     0 )
                  != IDE_SUCCESS);

        // BUG-37125 tpch plan optimization
        // ��Ƽ�� ������� �Ǿ����� ����Ȯ�� ��������� scan method �� ���ϰ� �ִ�.
        // ������ ��Ƽ���� cost ���� ���� ������ scan method �� �����ؾ� �Ѵ�.
        IDE_TEST( reviseAccessMethodsCost( sMyGraph,
                                           sPartitionCount )
                  != IDE_SUCCESS);

        for( i = 0; i < sMyGraph->accessMethodCnt; i++ )
        {
            if( QMO_COST_IS_GREATER(sMyGraph->selectedMethod->totalCost,
                                    sMyGraph->accessMethod[i].totalCost) )
            {
                sMyGraph->selectedMethod = &(sMyGraph->accessMethod[i]);
            }
            else
            {
                // Nothing to do.
            }
        }

        if( sMyGraph->selectedMethod->method == NULL )
        {
            sMyGraph->selectedIndex = NULL;
        }
        else
        {
            sMyGraph->selectedIndex = sMyGraph->selectedMethod->method->index;
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
            // Nothing to do.
        }

        //---------------------------------------------------
        // selected index�� global index�� ���
        // hint�� global index�� ���
        //---------------------------------------------------

        if ( sMyGraph->selectedIndex != NULL )
        {
            if ( sMyGraph->selectedIndex->indexPartitionType ==
                 QCM_NONE_PARTITIONED_INDEX )
            {
                // (global) index table scan
                sIsIndexTableScan = ID_TRUE;
            }
            else
            {
                // local index scan
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    // partition�� 1���� ���
    // global index�� ���õ� ���
    // full scan hint�� �ִ� ���
    // index scan hint�� �ִ� ���

    if ( sIsIndexTableScan == ID_TRUE )
    {
        //---------------------------------------------------
        // INDEX TABLE SCAN�� ���õ� ���
        //---------------------------------------------------

        // To Fix PR-9181
        // Index Scan�̶� �� ����
        // IN SUBQUERY KEY RANGE�� ���� ���
        // Order�� ������� �ʴ´�.
        if ( ( sMyGraph->selectedMethod->method->flag &
               QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK )
             == QMO_STAT_CARD_IDX_IN_SUBQUERY_TRUE )
        {
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
        }
        else
        {
            IDE_TEST( qmgSelection::makePreservedOrder(
                          aStatement,
                          sMyGraph->selectedMethod->method,
                          sTableRef->table,
                          & sMyGraph->graph.preservedOrder )
                      != IDE_SUCCESS );

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
        }
    }
    else
    {
        if ( sMyGraph->selectedMethod->method == NULL )
        {
            //---------------------------------------------------
            // FULL SCAN�� ���õ� ���
            //---------------------------------------------------

            if( sSelectedScanHint == QMG_USED_ONLY_FULL_SCAN_HINT )
            {
                //---------------------------------------------------
                // FULL SCAN Hint�� ���õ� ���
                //---------------------------------------------------

                sMyGraph->graph.preservedOrder = NULL;

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
                    for ( i = 1; i < sMyGraph->accessMethodCnt; i++ )
                    {
                        if ( sMyGraph->accessMethod[i].method->index->indexPartitionType ==
                             QCM_NONE_PARTITIONED_INDEX )
                        {
                            sExistIndexTable = ID_TRUE;
                            break;
                        }
                        else
                        {
                            // nothing to do
                        }
                    }

                    if ( sExistIndexTable == ID_TRUE )
                    {
                        // global index�� �����ϴ� ���
                        sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                        sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NOT_DEFINED;
                    }
                    else
                    {
                        // global index�� ���� ���
                        sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                        sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                    }
                }
                else
                {
                    // global index�� ���� ���
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                }
            }
        }
        else
        {
            //---------------------------------------------------
            // LOCAL INDEX SCAN�� ���õ� ���
            //---------------------------------------------------

            // local index�δ� order�� ���� �� ����.
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
        }
    }

    //---------------------------------------------------
    // Parallel query ������ ����
    //---------------------------------------------------
    if (aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT)
    {
        if (sTableRef->mParallelDegree > 1)
        {
            // parallel degree �� 2 �̻��̴���
            // access method �� ������ ���� ����
            // parallel execution �� �Ұ����ϴ�.
            //
            // - GLOBAL INDEX SCAN
            if ( sMyGraph->selectedIndex != NULL )
            {
                if ( sMyGraph->selectedIndex->indexPartitionType ==
                     QCM_NONE_PARTITIONED_INDEX )
                {
                    // GLOBAL INDEX SCAN
                    sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
                    sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_FALSE;
                }
                else
                {
                    // LOCAL INDEX SCAN
                    sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
                    sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_TRUE;
                }
            }
            else
            {
                // FULL SCAN or RID SCAN
                sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
                sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_TRUE;
            }
        }
        else
        {
            // Parallel degree = 1
            sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
            sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_FALSE;
        }
    }
    else
    {
        // Not SELECT statement
        sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
        sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_FALSE;
    }

    /*
     * BUG-41134
     * child �� parallel ���� �� �� �ִ��� �˻�
     */
    if ((sMyGraph->graph.flag & QMG_PARALLEL_EXEC_MASK) == QMG_PARALLEL_EXEC_TRUE)
    {
        for (i = 0; i < sPartitionCount; i++)
        {
            if ((sMyGraph->graph.children[i].childGraph->flag & QMG_PARALLEL_EXEC_MASK) ==
                QMG_PARALLEL_EXEC_FALSE)
            {
                sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
                sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_FALSE;
                break;
            }
        }
    }
    else
    {
        /* nothing to do */
    }

    //---------------------------------------------------
    // ���� ��� ������ ����
    //---------------------------------------------------

    if( sPartitionCount >= 1 )
    {
        // recordSize ����
        // �ƹ� child graph���� �������� �ȴ�.
        sMyGraph->graph.costInfo.recordSize =
            sMyGraph->graph.children[0].childGraph->costInfo.recordSize;

        sMyGraph->graph.costInfo.selectivity =
            sMyGraph->selectedMethod->methodSelectivity;

        // inputRecordCnt�� child�κ��� ���Ѵ�.
        sMyGraph->graph.costInfo.inputRecordCnt = 0;

        for( i = 0, sPartitionRef = sTableRef->partitionRef;
             (sPartitionRef != NULL) && (i < sPartitionCount);
             i++, sPartitionRef = sPartitionRef->next )
        {
            sMyGraph->graph.costInfo.inputRecordCnt +=
                sPartitionRef->statInfo->totalRecordCnt;
        }
        // ���ڵ� ������ �ּ� �Ѱ��̻����� �����Ѵ�.
        if( sMyGraph->graph.costInfo.inputRecordCnt < 1 )
        {
            sMyGraph->graph.costInfo.inputRecordCnt = 1;
        }
        else
        {
            // Nothing to do.
        }

        // output record count ����
        sOutputRecordCnt =
            sMyGraph->graph.costInfo.selectivity *
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

        // �ڽ��� cost�� total�� ����. ��� child�� cost�� �����ϰ� �ִ�.
        sMyGraph->graph.costInfo.totalAccessCost =
            sMyGraph->graph.costInfo.myAccessCost;
        sMyGraph->graph.costInfo.totalDiskCost =
            sMyGraph->graph.costInfo.myDiskCost;
        sMyGraph->graph.costInfo.totalAllCost =
            sMyGraph->graph.costInfo.myAllCost;
    }
    else
    {
        // TODO1502: ��������� �� �������־�� �Ѵ�.
        // ������ ������ ����
        // ���õ� ��Ƽ���� ����!
        sMyGraph->graph.costInfo.recordSize         = 1;
        sMyGraph->graph.costInfo.selectivity        = 0;
        sMyGraph->graph.costInfo.inputRecordCnt     = 1;
        sMyGraph->graph.costInfo.outputRecordCnt    = 1;
        sMyGraph->graph.costInfo.myAccessCost       = 1;
        sMyGraph->graph.costInfo.myDiskCost         = 0;
        sMyGraph->graph.costInfo.myAllCost          = 1;
        sMyGraph->graph.costInfo.totalAccessCost    = 1;
        sMyGraph->graph.costInfo.totalDiskCost      = 0;
        sMyGraph->graph.costInfo.totalAllCost       = 1;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::reviseAccessMethodsCost( qmgPARTITION   * aPartitionGraph,
                                       UInt             aPartitionCount )
{
    UInt             i, j;
    qmgSELT        * sChildGrpah;
    qmgChildren    * sChildList;
    qmoIdxCardInfo * sOrgMethod;
    qmsTableRef    * sTableRef;
    SDouble          sTotalInputCount;
    SDouble          sChildPercent;
    SDouble          sOutputCount;
    qmoAccessMethod  sMerge;

    IDU_FIT_POINT_FATAL( "qmgPartition::reviseAccessMethodsCost::__FT__" );

    if( aPartitionCount >= 1 )
    {
        sChildList  = aPartitionGraph->graph.children;
        sChildGrpah = (qmgSELT*)sChildList->childGraph;

        //--------------------------------------
        // Sum Input record count
        //--------------------------------------
        for( sTotalInputCount = 0, i = 0,
                 sChildList = aPartitionGraph->graph.children;
             sChildList != NULL;
             i++, sChildList = sChildList->next )
        {
            sTotalInputCount += sChildList->childGraph->costInfo.inputRecordCnt;
        }

        //--------------------------------------
        // Add child cost
        //--------------------------------------
        for( i = 0; i < aPartitionGraph->accessMethodCnt ; i++ )
        {
            idlOS::memset( &sMerge, 0, ID_SIZEOF(qmoAccessMethod));

            //--------------------------------------
            // Full scan
            //--------------------------------------
            if( aPartitionGraph->accessMethod[i].method == NULL )
            {
                for( sChildList = aPartitionGraph->graph.children;
                     sChildList != NULL;
                     sChildList = sChildList->next )
                {
                    sChildGrpah = (qmgSELT*) sChildList->childGraph;

                    // �ڽ� �׷����� ���ڵ� ���� ������� selectivity �� �ݿ��Ѵ�.
                    sChildPercent = sChildList->childGraph->costInfo.inputRecordCnt /
                        sTotalInputCount;

                    sMerge.keyRangeSelectivity  += sChildGrpah->accessMethod[0].keyRangeSelectivity  * sChildPercent;
                    sMerge.keyFilterSelectivity += sChildGrpah->accessMethod[0].keyFilterSelectivity * sChildPercent;
                    sMerge.filterSelectivity    += sChildGrpah->accessMethod[0].filterSelectivity    * sChildPercent;
                    sMerge.methodSelectivity    += sChildGrpah->accessMethod[0].methodSelectivity    * sChildPercent;
                    sMerge.keyRangeCost         += sChildGrpah->accessMethod[0].keyRangeCost;
                    sMerge.keyFilterCost        += sChildGrpah->accessMethod[0].keyFilterCost;
                    sMerge.filterCost           += sChildGrpah->accessMethod[0].filterCost;
                    sMerge.accessCost           += sChildGrpah->accessMethod[0].accessCost;
                    sMerge.diskCost             += sChildGrpah->accessMethod[0].diskCost;
                    sMerge.totalCost            += sChildGrpah->accessMethod[0].totalCost;
                }

                sOrgMethod = aPartitionGraph->accessMethod[i].method;

                idlOS::memcpy( &(aPartitionGraph->accessMethod[i]),
                               &sMerge,
                               ID_SIZEOF(qmoAccessMethod) );

                aPartitionGraph->accessMethod[i].method         = sOrgMethod;
            }
            else if( aPartitionGraph->accessMethod[i].method->index->indexPartitionType
                     != QCM_NONE_PARTITIONED_INDEX )
            {
                //--------------------------------------
                // non Global Index scan
                //--------------------------------------
                for( sChildList = aPartitionGraph->graph.children;
                     sChildList != NULL;
                     sChildList = sChildList->next )
                {
                    sChildGrpah = (qmgSELT*) sChildList->childGraph;

                    for( j = 1; j < sChildGrpah->accessMethodCnt; j++ )
                    {
                        if( aPartitionGraph->accessMethod[i].method->index->indexId ==
                            sChildGrpah->accessMethod[j].method->index->indexId )
                        {
                            // �ڽ� �׷����� ���ڵ� ���� ������� selectivity �� �ݿ��Ѵ�.
                            sChildPercent = sChildList->childGraph->costInfo.inputRecordCnt /
                                sTotalInputCount;

                            sMerge.keyRangeSelectivity  += sChildGrpah->accessMethod[j].keyRangeSelectivity  * sChildPercent;
                            sMerge.keyFilterSelectivity += sChildGrpah->accessMethod[j].keyFilterSelectivity * sChildPercent;
                            sMerge.filterSelectivity    += sChildGrpah->accessMethod[j].filterSelectivity    * sChildPercent;
                            sMerge.methodSelectivity    += sChildGrpah->accessMethod[j].methodSelectivity    * sChildPercent;
                            sMerge.keyRangeCost         += sChildGrpah->accessMethod[j].keyRangeCost;
                            sMerge.keyFilterCost        += sChildGrpah->accessMethod[j].keyFilterCost;
                            sMerge.filterCost           += sChildGrpah->accessMethod[j].filterCost;
                            sMerge.accessCost           += sChildGrpah->accessMethod[j].accessCost;
                            sMerge.diskCost             += sChildGrpah->accessMethod[j].diskCost;
                            sMerge.totalCost            += sChildGrpah->accessMethod[j].totalCost;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }   // end for j
                }   // end for child

                sOrgMethod = aPartitionGraph->accessMethod[i].method;

                idlOS::memcpy( &(aPartitionGraph->accessMethod[i]),
                               &sMerge,
                               ID_SIZEOF(qmoAccessMethod) );

                aPartitionGraph->accessMethod[i].method         = sOrgMethod;
            }
            else
            {
                //--------------------------------------
                // Global Index scan
                //--------------------------------------
                sTableRef = aPartitionGraph->graph.myFrom->tableRef;

                // �ε��� ���̺��� �����ϴ� ����� �߰��Ѵ�.
                sMerge.totalCost = qmoCost::getTableRIDScanCost(
                    sTableRef->statInfo,
                    &sMerge.accessCost,
                    &sMerge.diskCost );

                // output count
                sOutputCount = sTotalInputCount * aPartitionGraph->accessMethod[i].methodSelectivity;

                aPartitionGraph->accessMethod[i].accessCost += sMerge.accessCost * sOutputCount;
                aPartitionGraph->accessMethod[i].diskCost   += sMerge.diskCost   * sOutputCount;
                aPartitionGraph->accessMethod[i].totalCost  += sMerge.totalCost  * sOutputCount;
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmgPartition::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgPartition�� ���� Plan�� �����Ѵ�.
 *               partition filter ������ ���� ���⼭ predicate�� ���ġ�Ѵ�.
 * Implementation :
 *    - qmgPartition���� ���� ���������� plan
 *
 *        1. �Ϲ� scan�� ���
 *
 *                 [PSCN]
 *                   |
 *                 [SCAN]-[SCAN]-[SCAN]-...-[SCAN]
 *
 *        2. Parallel scan�� ���
 *
 *                 [PPCRD]
 *                   |  |
 *                   | [PRLQ]-[PRLQ]-...-[PRLQ]
 *                   |
 *                 [SCAN]-[SCAN]-[SCAN]-...-[SCAN]
 *
 ***********************************************************************/

    qmgPARTITION  * sMyGraph;
    qmnPlan       * sPCRD;
    qmnPlan       * sPRLQ;
    qmgChildren   * sChildren;
    qmnChildren   * sChildrenPRLQ;
    qmnChildren   * sCurPlanChildren;
    qmsTableRef   * sTableRef;
    UInt            sPartitionCount;
    UInt            sPRLQCount;
    qmoPCRDInfo     sPCRDInfo;
    idBool          sMakePPCRD;
    UInt            i;

    IDU_FIT_POINT_FATAL( "qmgPartition::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph     != NULL );

    sMyGraph  = (qmgPARTITION *) aGraph;
    sTableRef =  sMyGraph->graph.myFrom->tableRef;

    sPartitionCount  = 0;
    sPRLQCount       = 0;
    sChildrenPRLQ    = NULL;
    sCurPlanChildren = NULL;

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

    if( aParent != NULL )
    {
        sMyGraph->graph.myPlan = aParent->myPlan;

        /* PROJ-1071 Parallel Query */
        aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
        aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);
    }
    else
    {
        aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
        aGraph->flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;
    }

    //---------------------------------------------------
    // Index Table Scan�� ��� RID SCAN���� ����
    //---------------------------------------------------
    
    // PROJ-1624 global non-partitioned index
    // selected index�� global index��� scan�� index table scan������ �����Ѵ�.
    // ������ makeGraph �߿� �����ϴ� ���� ������
    // bottom-up���� ����ȭ�Ǹ鼭 ���� graph������ graph�� �����Ű�Ƿ�
    // makePlan ������ �����Ѵ�.
    if ( sMyGraph->selectedIndex != NULL )
    {
        if ( sMyGraph->selectedIndex->indexPartitionType ==
             QCM_NONE_PARTITIONED_INDEX )
        {
            IDE_TEST( alterForceRidScan( aStatement,
                                         sMyGraph )
                      != IDE_SUCCESS );

            /* BUG-38024 */
            sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
            sMyGraph->graph.flag |= QMG_PARALLEL_EXEC_FALSE;

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

    // PROJ-1071
    // 1. Parallel degree �� 1 ���� ū ���
    // 2. Subquery �� �ƴϿ��� ��
    // 3. Parallel ���� ������ graph
    // 4. Parallel SCAN �� ���Ǿ�� �� (BUG-38410)
    if ( ( (sMyGraph->graph.flag & QMG_PARALLEL_EXEC_MASK)
           == QMG_PARALLEL_EXEC_TRUE ) &&
         ( aGraph->myQuerySet->parentTupleID
           == ID_USHORT_MAX ) &&
         ( (aGraph->flag & QMG_PARALLEL_IMPOSSIBLE_MASK)
           == QMG_PARALLEL_IMPOSSIBLE_FALSE ) &&
         ( (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK)
           == QMG_PLAN_EXEC_REPEATED_FALSE ) )
    {
        sMakePPCRD = ID_TRUE;
    }
    else
    {
        sMakePPCRD = ID_FALSE;
    }
    
    //----------------------------------------------
    // PCRD Ȥ�� PPCRD �� ����
    //----------------------------------------------
    if (sMakePPCRD == ID_FALSE)
    {
        // Parallel query �� �ƴ�
        IDE_TEST( qmoMultiNonPlan::initPCRD( aStatement,
                                             sMyGraph->graph.myQuerySet,
                                             sMyGraph->graph.myPlan,
                                             &sPCRD )
                  != IDE_SUCCESS );

    }
    else
    {
        // Parallel query
        IDE_TEST( qmoMultiNonPlan::initPPCRD( aStatement,
                                              sMyGraph->graph.myQuerySet,
                                              sMyGraph->graph.myPlan,
                                              &sPCRD )
                  != IDE_SUCCESS );
    }

    sMyGraph->graph.myPlan = sPCRD;

    // child plan�� ����
    sTableRef->partitionRef = NULL;

    for( sChildren = sMyGraph->graph.children;
         sChildren != NULL;
         sChildren = sChildren->next )
    {
        // plan�� �����ϸ鼭 partition reference�� merge�Ѵ�.
        // DNF�� �� ��� ������ plan�� ���� �ٸ� partition reference��
        // �����Ƿ� merge�� �־�� ��.
        IDE_DASSERT( sChildren->childGraph->type == QMG_SELECTION );

        IDE_TEST( qmoPartition::mergePartitionRef(
                      sTableRef,
                      ((qmgSELT*)sChildren->childGraph)->partitionRef )
                  != IDE_SUCCESS );

        /* PROJ-2464 hybrid partitioned table ���� */
        IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTableRef )
                  != IDE_SUCCESS );

        IDE_TEST( sChildren->childGraph->makePlan( aStatement ,
                                                   &sMyGraph->graph ,
                                                   sChildren->childGraph )
                  != IDE_SUCCESS);
        sPartitionCount++;
    }

    //----------------------------------------------
    // child PRLQ plan�� ����
    //----------------------------------------------
    sPCRD->childrenPRLQ = NULL;

    if (sMakePPCRD == ID_FALSE)
    {
        // Parallel disabled
        // Nothing to do.
    }
    else
    {
        // PRLQ �� parallel degree ��ŭ �����
        // partition ������ ���� �ʾƾ� �Ѵ�.
        for( i = 0, sChildren = sMyGraph->graph.children;
             (i < sTableRef->mParallelDegree) && (i < sPartitionCount)
                 && (sChildren != NULL);
             i++, sChildren = sChildren->next )
     
        {
            IDE_TEST( qmoParallelPlan::initPRLQ( aStatement,
                                                 sPCRD,
                                                 &sPRLQ )
                      != IDE_SUCCESS );

            IDE_TEST( qmoParallelPlan::makePRLQ( aStatement,
                                                 sMyGraph->graph.myQuerySet,
                                                 QMO_MAKEPRLQ_PARALLEL_TYPE_PARTITION,
                                                 sChildren->childGraph->myPlan,
                                                 sPRLQ )
                      != IDE_SUCCESS );

            // PRLQ �� SCAN �� �������� ��-���� ���踦 ������ �ʴ´�.
            sPRLQ->left = NULL;

            //----------------------------------------------
            // Children PRLQ �� plan �� ����
            //----------------------------------------------
            IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmnChildren),
                                                   (void**)&sChildrenPRLQ)
                     != IDE_SUCCESS);

            sChildrenPRLQ->childPlan = sPRLQ;
            sChildrenPRLQ->next = NULL;

            //----------------------------------------------
            // Children PRLQ Plan�� ���� ���踦 ����
            //----------------------------------------------
            if ( sCurPlanChildren == NULL )
            {
                sPCRD->childrenPRLQ = sChildrenPRLQ;
            }
            else
            {
                sCurPlanChildren->next = sChildrenPRLQ;
            }
            sCurPlanChildren = sChildrenPRLQ;

            sPRLQCount++;
        }
        ((qmncPPCRD *)sPCRD)->PRLQCount = sPRLQCount;
    }

    sPCRDInfo.flag = QMO_PCRD_INFO_INITIALIZE;

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
        sPCRDInfo.flag &= ~QMO_PCRD_INFO_NOTNULL_KEYRANGE_MASK;
        sPCRDInfo.flag |= QMO_PCRD_INFO_NOTNULL_KEYRANGE_TRUE;
    }
    else
    {
        // To Fix PR-10288
        // NOTNULL KEY RANGE�� �ƴ� ���� �ݵ�� ������ �־�� ��.
        sPCRDInfo.flag &= ~QMO_PCRD_INFO_NOTNULL_KEYRANGE_MASK;
        sPCRDInfo.flag |= QMO_PCRD_INFO_NOTNULL_KEYRANGE_FALSE;
    }
    
    //---------------------------------------------------
    // partition filter ����� predicate ó��
    //---------------------------------------------------
    
    // non-joinable predicate�� pushdown�� �� �ִ�.
    if ( sMyGraph->graph.myPredicate != NULL )
    {
        // partition pruning�� ���� predicate�� ����
        
        // partition filter �����
        IDE_TEST(qmoPred::deepCopyPredicate(
                     QC_QMP_MEM(aStatement),
                     sMyGraph->graph.myPredicate,
                     &sMyGraph->partFilterPredicate )
                 != IDE_SUCCESS );
        
        // predicate�� ���ġ�Ѵ�.(partition filter�����)
        IDE_TEST( qmoPred::relocatePredicate4PartTable(
                      aStatement,
                      sMyGraph->partFilterPredicate,
                      sTableRef->tableInfo->partitionMethod,
                      & sMyGraph->graph.depInfo,
                      & sMyGraph->graph.myQuerySet->outerDepInfo,
                      & sMyGraph->partFilterPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // global index�� ���� predicate ����ȭ
    //---------------------------------------------------

    // non-joinable predicate�� pushdown�� �� �ִ�.
    if ( sMyGraph->graph.myPredicate != NULL )
    {
        // predicate�� ���ġ�Ѵ�.
        IDE_TEST( qmoPred::relocatePredicate(
                      aStatement,
                      sMyGraph->graph.myPredicate,
                      & sMyGraph->graph.depInfo,
                      & sMyGraph->graph.myQuerySet->outerDepInfo,
                      sTableRef->statInfo,
                      & sMyGraph->graph.myPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    sPCRDInfo.predicate              = sMyGraph->graph.myPredicate;
    sPCRDInfo.constantPredicate      = sMyGraph->graph.constantPredicate;
    sPCRDInfo.index                  = sMyGraph->selectedIndex;
    sPCRDInfo.preservedOrder         = sMyGraph->graph.preservedOrder;
    sPCRDInfo.forceIndexScan         = sMyGraph->forceIndexScan;
    sPCRDInfo.nnfFilter              = sMyGraph->graph.nnfFilter;
    sPCRDInfo.partKeyRange           = sMyGraph->partKeyRange;
    sPCRDInfo.partFilterPredicate    = sMyGraph->partFilterPredicate;
    sPCRDInfo.selectedPartitionCount = sPartitionCount;
    sPCRDInfo.childrenGraph          = sMyGraph->graph.children;

    // BUG-48800
    if ( sMyGraph->mPrePruningPartRef != NULL )
    {
        sPCRDInfo.mPrePruningPartHandle  = sMyGraph->mPrePruningPartRef->partitionHandle;
        sPCRDInfo.mPrePruningPartSCN     = sMyGraph->mPrePruningPartRef->partitionSCN;
    }
    else
    {
        sPCRDInfo.mPrePruningPartHandle  = NULL; 
        sPCRDInfo.mPrePruningPartSCN     = SM_SCN_INIT; 
    }   

    // make PCRD or PPCRD
    if (sMakePPCRD == ID_FALSE)
    {
        IDE_TEST( qmoMultiNonPlan::makePCRD( aStatement,
                                             sMyGraph->graph.myQuerySet,
                                             sMyGraph->graph.myFrom,
                                             &sPCRDInfo,
                                             sPCRD )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qmoMultiNonPlan::makePPCRD( aStatement,
                                              sMyGraph->graph.myQuerySet,
                                              sMyGraph->graph.myFrom,
                                              &sPCRDInfo,
                                              sPCRD )
                  != IDE_SUCCESS );
    }

    sMyGraph->graph.myPlan = sPCRD;

    qmg::setPlanInfo( aStatement, sPCRD, &(sMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::alterForceRidScan( qcStatement  * aStatement,
                                 qmgPARTITION * aGraph )
{
    qmgChildren  * sChild;

    IDU_FIT_POINT_FATAL( "qmgPartition::alterForceRidScan::__FT__" );

    for( sChild = aGraph->graph.children;
         sChild != NULL;
         sChild = sChild->next )
    {
        IDE_TEST( qmgSelection::alterForceRidScan( aStatement,
                                                   sChild->childGraph )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::alterSelectedIndex( qcStatement  * aStatement,
                                  qmgPARTITION * aGraph,
                                  qcmIndex     * aNewSelectedIndex )
{
    qmgChildren  * sChild;
    qmgSELT      * sChildSeltGraph;

    IDU_FIT_POINT_FATAL( "qmgPartition::alterSelectedIndex::__FT__" );

    // PROJ-1624 Global Index
    // global index�� partitioned table���� �����Ǿ� �ִ�.
    if ( aNewSelectedIndex->indexPartitionType ==
         QCM_NONE_PARTITIONED_INDEX )
    {
        aGraph->selectedIndex = aNewSelectedIndex;

        // To fix BUG-12742
        // ���� graph�� ���� ������ ���
        // index�� ���� �� ����.
        aGraph->forceIndexScan = ID_TRUE;
    }
    else
    {   
        for( sChild = aGraph->graph.children;
             sChild != NULL;
             sChild = sChild->next )
        {
            sChildSeltGraph = (qmgSELT*)sChild->childGraph;
            
            IDE_TEST( qmgSelection::alterSelectedIndex(
                          aStatement,
                          sChildSeltGraph,
                          aNewSelectedIndex )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::printGraph( qcStatement  * aStatement,
                          qmgGraph     * aGraph,
                          ULong          aDepth,
                          iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *    Graph�� �����ϴ� ���� ������ ����Ѵ�.
 *    TODO1502: partition graph�� ���� ������ �������� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoPredicate * sMorePred;
    qmgChildren  * sChildren;
    qmgPARTITION * sMyGraph;
    UInt  i;

    IDU_FIT_POINT_FATAL( "qmgPartition::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

    sMyGraph = (qmgPARTITION*)aGraph;

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
    // BUG-38192
    // Access method �� ���� ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "== Access Method Information ==" );

    for ( i = 0; i < sMyGraph->accessMethodCnt; i++ )
    {
        IDE_TEST( qmgSelection::printAccessMethod(
                      aStatement,
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

    // BUG-38192

    if ( QCU_TRCLOG_DISPLAY_CHILDREN == 1 )
    {
        if ( aGraph->children != NULL )
        {
            for( sChildren = aGraph->children;
                 sChildren != NULL;
                 sChildren = sChildren->next )
            {
                IDE_TEST( sChildren->childGraph->printGraph( aStatement,
                                                             sChildren->childGraph,
                                                             aDepth + 1,
                                                             aString )
                          != IDE_SUCCESS );
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
qmgPartition::extractPartKeyRange(
    qcStatement        * aStatement,
    qmsQuerySet        * aQuerySet,
    qmoPredicate      ** aPartKeyPred,
    UInt                 aPartKeyCount,
    mtcColumn          * aPartKeyColumns,
    UInt               * aPartKeyColsFlag,
    qtcNode           ** aPartKeyRangeOrg,
    smiRange          ** aPartKeyRange )
{
    qtcNode  * sKey;
    qtcNode  * sDNFKey;
    smiRange * sRange;
    smiRange * sRangeArea;
    UInt       sEstimateSize;
    UInt       sEstDNFCnt;
    UInt       sCompareType;

    IDU_FIT_POINT_FATAL( "qmgPartition::extractPartKeyRange::__FT__" );

    sKey = NULL;
    sDNFKey = NULL;
    sRange = NULL;
    sRangeArea = NULL;
    sEstimateSize = 0;
    sEstDNFCnt = 0;

    *aPartKeyRangeOrg = NULL;
    *aPartKeyRange = NULL;

    if( *aPartKeyPred == NULL )
    {
        // Nothing to do.
    }
    else
    {
        // qtcNode���� ��ȯ
        IDE_TEST( qmoPred::linkPredicate( aStatement ,
                                          *aPartKeyPred ,
                                          &sKey ) != IDE_SUCCESS );

        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sKey ) != IDE_SUCCESS );

        if( sKey != NULL )
        {
            // To Fix PR-9481
            // CNF�� ������ Key Range Predicate�� DNF normalize�� ���
            // ��û���� ���� Node�� ��ȯ�� �� �ִ�.
            // �̸� �˻��Ͽ� ����ġ�� ���� ��ȭ�� �ʿ��� ��쿡��
            // Default Key Range���� �����ϰ� �Ѵ�.
            IDE_TEST( qmoNormalForm::estimateDNF( sKey, &sEstDNFCnt )
                      != IDE_SUCCESS );

            if ( sEstDNFCnt > QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement ) )
            {
                sDNFKey = NULL;
            }
            else
            {
                // DNF���·� ��ȯ
                IDE_TEST( qmoNormalForm::normalizeDNF( aStatement ,
                                                       sKey ,
                                                       &sDNFKey )
                          != IDE_SUCCESS );
            }

            // environment�� ���
            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );
        }
        else
        {
            //nothing to do
        }

        if( sDNFKey != NULL )
        {
            IDE_DASSERT( ( (*aPartKeyPred)->flag  & QMO_PRED_VALUE_MASK )
                         == QMO_PRED_FIXED );

            // KeyRange�� ���� DNF�� ���� size estimation�� �Ѵ�.
            IDE_TEST( qmoKeyRange::estimateKeyRange( QC_SHARED_TMPLATE(aStatement) ,
                                                     sDNFKey ,
                                                     &sEstimateSize )
                      != IDE_SUCCESS );

            IDE_TEST( QC_QMP_MEM(aStatement)->cralloc( sEstimateSize ,
                                                       (void**)&sRangeArea )
                      != IDE_SUCCESS );

            // partition keyrange�� �����Ѵ�.

            // PROJ-1872
            // Partition Key�� Partition Table �����Ҷ� ����
            // (Partition�� ������ ���ذ�)�� (Predicate�� value)��
            // ���ϹǷ� MTD Value���� compare�� �����
            // ex) i1 < 10 => Partition P1
            //     i1 < 20 => Partition P2
            //     i1 < 30 => Partition P3
            // SELECT ... FROM ... WHERE i1 = 5
            // P1, P2, P3���� P1 Partition�� �����ϱ� ����
            // Partition Key Range�� �����
            sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;

            IDE_TEST(
                qmoKeyRange::makePartKeyRange( QC_SHARED_TMPLATE(aStatement),
                                               sDNFKey,
                                               aPartKeyCount,
                                               aPartKeyColumns,
                                               aPartKeyColsFlag,
                                               sCompareType,
                                               sRangeArea,
                                               sEstimateSize,
                                               &sRange )
                != IDE_SUCCESS );

            *aPartKeyRangeOrg = sKey;
            *aPartKeyRange = sRange;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::copyPARTITIONAndAlterSelectedIndex( qcStatement   * aStatement,
                                                  qmgPARTITION  * aSource,
                                                  qmgPARTITION ** aTarget,
                                                  qcmIndex      * aNewSelectedIndex,
                                                  UInt            aWhichOne )
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

    qmgPARTITION * sTarget;
    qmgChildren  * sChild;
    qmgGraph     * sNewChild;
    UInt           sChildCnt = 0;
    UInt           i;

    IDU_FIT_POINT_FATAL( "qmgPartition::copyPARTITIONAndAlterSelectedIndex::__FT__" );

    for( sChild = aSource->graph.children;
         sChild != NULL;
         sChild = sChild->next )
    {
        sChildCnt++;
    }
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPARTITION ), (void**) &sTarget )
              != IDE_SUCCESS );

    idlOS::memcpy( sTarget, aSource, ID_SIZEOF( qmgPARTITION ) );
    
    // graph->children����ü�� �޸� �Ҵ�.
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                  ID_SIZEOF(qmgChildren) * sChildCnt,
                  (void**) &sTarget->graph.children )
              != IDE_SUCCESS );
    
    for( sChild = aSource->graph.children, i = 0;
         sChild != NULL;
         sChild = sChild->next, i++ )
    {
        IDE_TEST( qmgSelection::copySELTAndAlterSelectedIndex(
                      aStatement,
                      (qmgSELT*)sChild->childGraph,
                      (qmgSELT**)&sNewChild,
                      aNewSelectedIndex,
                      aWhichOne )
                  != IDE_SUCCESS );

        sTarget->graph.children[i].childGraph = sNewChild;

        if ( i + 1 < sChildCnt )
        {
            sTarget->graph.children[i].next = &(sTarget->graph.children[i + 1]);
        }
        else
        {
            sTarget->graph.children[i].next = NULL;
        }        
    }
    
    if( aWhichOne == 0 )
    {
        IDE_TEST( alterSelectedIndex( aStatement,
                                      sTarget,
                                      aNewSelectedIndex )
                  != IDE_SUCCESS );
    }
    else if( aWhichOne == 1 )
    {
        IDE_TEST( alterSelectedIndex( aStatement,
                                      aSource,
                                      aNewSelectedIndex )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( 0 );
    }

    *aTarget = sTarget;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::setJoinPushDownPredicate( qcStatement   * aStatement,
                                        qmgPARTITION  * aGraph,
                                        qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                push-down join predicate�� partition graph�� �����Ѵ�.
 *
 *  Implementation :
 *                 1) partition graph�� ���� child graph(selection)��
 *                    join predicate�� �����Ͽ� �����Ų��.
 *                 2) partition graph���� join predicate�� ���� �����Ų��.
 *                 3) parameter�� ���� join predicate�� null�� ����.
 *
 ***********************************************************************/

    qmgChildren  * sChild;
    qmgSELT      * sChildSeltGraph;
    qmoPredicate * sCopiedPred = NULL;
    qmoPredicate * sJoinPredicate;

    IDU_FIT_POINT_FATAL( "qmgPartition::setJoinPushDownPredicate::__FT__" );

    // partition graph�� child graph�� �����Ͽ� ����.
    for( sChild = aGraph->graph.children;
         sChild != NULL;
         sChild = sChild->next )
    {
        sChildSeltGraph = (qmgSELT*)sChild->childGraph;

        IDE_TEST(qmoPred::copyPredicate4Partition(
                     aStatement,
                     *aPredicate,
                     &sCopiedPred,
                     aGraph->graph.myFrom->tableRef->table,
                     sChildSeltGraph->partitionRef->table,
                     ID_TRUE )
                 != IDE_SUCCESS );

        IDE_TEST( qmgSelection::setJoinPushDownPredicate(
                      sChildSeltGraph,
                      &sCopiedPred )
                  != IDE_SUCCESS );
    }

    // partition graph�� ����.
    for( sJoinPredicate       = *aPredicate;
         sJoinPredicate->next != NULL;
         sJoinPredicate       = sJoinPredicate->next ) ;

    sJoinPredicate->next = aGraph->graph.myPredicate;
    aGraph->graph.myPredicate  = *aPredicate;
    *aPredicate       = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::setNonJoinPushDownPredicate( qcStatement   * aStatement,
                                           qmgPARTITION  * aGraph,
                                           qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                push-down join predicate�� partition graph�� �����Ѵ�.
 *
 *  Implementation :
 *                 1) partition graph�� ���� child graph(selection)��
 *                    join predicate�� �����Ͽ� �����Ų��.
 *                 2) partition graph���� join predicate�� ���� �����Ų��.
 *                 3) parameter�� ���� join predicate�� null�� ����.
 *
 ***********************************************************************/

    qmgChildren  * sChild;
    qmgSELT      * sChildSeltGraph;
    qmoPredicate * sCopiedPred;
    qmoPredicate * sJoinPredicate;
    qmoPredicate * sPredicate;
    qmoPredicate * sNextPredicate;

    IDU_FIT_POINT_FATAL( "qmgPartition::setNonJoinPushDownPredicate::__FT__" );

    // partition graph�� child graph�� �����Ͽ� ����.
    for( sChild = aGraph->graph.children;
         sChild != NULL;
         sChild = sChild->next )
    {
        sChildSeltGraph = (qmgSELT*)sChild->childGraph;

        IDE_TEST(qmoPred::copyPredicate4Partition(
                     aStatement,
                     *aPredicate,
                     &sCopiedPred,
                     aGraph->graph.myFrom->tableRef->table,
                     sChildSeltGraph->partitionRef->table,
                     ID_TRUE )
                 != IDE_SUCCESS );

        IDE_TEST( qmgSelection::setNonJoinPushDownPredicate(
                      sChildSeltGraph,
                      &sCopiedPred )
                  != IDE_SUCCESS );
    }


    sJoinPredicate = *aPredicate;

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

    *aPredicate = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgPartition::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Preserved Order�� direction�� �����Ѵ�.
 *                direction�� NOT_DEFINED �� ��쿡�� ȣ���Ͽ��� �Ѵ�.
 *
 *  Implementation :
 *
 ***********************************************************************/

    qmgPARTITION      * sMyGraph;
    qmgPreservedOrder * sPreservedOrder;
    UInt              * sKeyColsFlag;
    UInt                sKeyColCount;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmgPartition::finalizePreservedOrder::__FT__" );

    sMyGraph = (qmgPARTITION*) aGraph;

    // Preserved order�� �����ϴ� ���� global index scan���̴�.
    IDE_DASSERT( sMyGraph->selectedIndex->indexPartitionType ==
                 QCM_NONE_PARTITIONED_INDEX );
    
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

    return IDE_SUCCESS;
}
