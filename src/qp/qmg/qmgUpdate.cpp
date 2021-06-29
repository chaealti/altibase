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
 * $Id: qmgUpdate.cpp 53774 2012-06-15 04:53:31Z eerien $
 *
 * Description :
 *     Update Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgUpdate.h>
#include <qmo.h>
#include <qmoOneNonPlan.h>
#include <qmgSelection.h>
#include <qmgPartition.h>
#include <qmoPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE
#include <qcmPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE
#include <qmsDefaultExpr.h>

IDE_RC
qmgUpdate::init( qcStatement * aStatement,
                 qmsQuerySet * aQuerySet,
                 qmgGraph    * aChildGraph,
                 qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgUpdate Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgUpdate�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgUPTE         * sMyGraph;
    qmmUptParseTree * sParseTree;
    qmsQuerySet     * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgUpdate::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Update Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgUpdate�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgUPTE ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_UPDATE;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgUpdate::optimize;
    sMyGraph->graph.makePlan = qmgUpdate::makePlan;
    sMyGraph->graph.printGraph = qmgUpdate::printGraph;

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
    // Update Graph ���� ���� �ʱ�ȭ
    //---------------------------------------------------

    sParseTree = (qmmUptParseTree *)aStatement->myPlan->parseTree;

    /* PROJ-2204 JOIN UPDATE, DELETE */
    sMyGraph->updateTableRef    = sParseTree->updateTableRef;
    
    // �ֻ��� graph�� update graph�� update columns ������ ����
    sMyGraph->columns           = sParseTree->updateColumns;
    sMyGraph->updateColumnList  = sParseTree->uptColumnList;
    sMyGraph->updateColumnCount = sParseTree->uptColCount;
    
    // �ֻ��� graph�� update graph�� update values ������ ����
    sMyGraph->values         = sParseTree->values;
    sMyGraph->subqueries     = sParseTree->subqueries;
    sMyGraph->lists          = sParseTree->lists;
    sMyGraph->valueIdx       = sParseTree->valueIdx;
    sMyGraph->canonizedTuple = sParseTree->canonizedTuple;
    sMyGraph->compressedTuple= sParseTree->compressedTuple;     // PROJ-2264
    sMyGraph->isNull         = sParseTree->isNull;

    // �ֻ��� graph�� update graph�� sequence ������ ����
    sMyGraph->nextValSeqs = sParseTree->common.nextValSeqs;
    
    // �ֻ��� graph�� update graph�� instead of trigger������ ����
    sMyGraph->insteadOfTrigger = sParseTree->insteadOfTrigger;

    // �ֻ��� graph�� update graph�� limit�� ����
    sMyGraph->limit = sParseTree->limit;

    // �ֻ��� graph�� update graph�� constraint�� ����
    sMyGraph->parentConstraints = sParseTree->parentConstraints;
    sMyGraph->childConstraints  = sParseTree->childConstraints;
    sMyGraph->checkConstrList   = sParseTree->checkConstrList;

    // �ֻ��� graph�� update graph�� return into�� ����
    sMyGraph->returnInto = sParseTree->returnInto;

    // �ֻ��� graph�� insert graph�� Default Expr�� ����
    sMyGraph->defaultExprTableRef = sParseTree->defaultTableRef;
    sMyGraph->defaultExprColumns  = sParseTree->defaultExprColumns;

    /* PROJ-2714 Multiple update, delete support */
    sMyGraph->mTableList = sParseTree->mTableList;
    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgUpdate::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgUpdate�� ����ȭ
 *
 * Implementation :
 *    (1) CASE 2 : UPDATE...SET column = (subquery)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (2) CASE 3 : UPDATE...SET (column list) = (subquery)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (3) SCAN Limit ����ȭ
 *    (4) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgUPTE           * sMyGraph;
    qmgGraph          * sChildGraph;
    qmsTableRef       * sTableRef;
    idBool              sIsIntersect = ID_FALSE;
    qtcNode           * sNode;
    qmsLimit          * sLimit;
    idBool              sIsScanLimit;
    idBool              sIsRowMovementUpdate = ID_FALSE;
    idBool              sHasMemory = ID_FALSE;

    SDouble             sOutputRecordCnt;
    SDouble             sUpdateSelectivity = 1;    // BUG-17166

    IDU_FIT_POINT_FATAL( "qmgUpdate::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgUPTE*) aGraph;
    sChildGraph = aGraph->left;

    sTableRef = sMyGraph->updateTableRef;

    //---------------------------------------------------
    // update hidden column ����ȭ
    //---------------------------------------------------

    /* PROJ-1090 Function-based Index */
    IDE_TEST( qmsDefaultExpr::makeBaseColumn(
                  aStatement,
                  sTableRef->tableInfo,
                  sMyGraph->defaultExprColumns,
                  &(sMyGraph->defaultExprBaseColumns) )
              != IDE_SUCCESS );

    // Disk Table�� Default Expr�� ����
    /* PROJ-2464 hybrid partitioned table ����
     *  qmoPartition::optimizeInto()���� Partition�� Tuple�� �����ϹǷ�,
     *  qmoPartition::optimizeInto()�� ȣ���ϱ� ���� �����ؾ� �Ѵ�.
     */
    qmsDefaultExpr::setUsedColumnToTableRef(
        &(QC_SHARED_TMPLATE(aStatement)->tmplate),
        sTableRef,
        sMyGraph->defaultExprBaseColumns,
        ID_TRUE );

    //---------------------------------------------------
    // Row Movement ����ȭ
    //---------------------------------------------------

    // PROJ-1502 PARTITIONED DISK TABLE
    // partitioned table�� ��� set���� partition key�� ���ԵǾ� �ְ�,
    // table�� row movement�� Ȱ��ȭ �Ǿ� �ִٸ�,
    // insert table reference�� ����ؾ� �Ѵ�.
    if ( sTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        IDE_TEST( qmoPartition::isIntersectPartKeyColumn(
                      sTableRef->tableInfo->partKeyColumns,
                      sMyGraph->columns,
                      & sIsIntersect )
                  != IDE_SUCCESS );

        if ( sIsIntersect == ID_TRUE )
        {
            if ( sTableRef->tableInfo->rowMovement == ID_TRUE )
            {
                sIsRowMovementUpdate = ID_TRUE;
            
                IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                              ID_SIZEOF(qmsTableRef),
                              (void**)&sMyGraph->insertTableRef )
                          != IDE_SUCCESS );

                idlOS::memcpy( sMyGraph->insertTableRef,
                               sTableRef,
                               ID_SIZEOF(qmsTableRef) );

                /* PROJ-2464 hybrid partitioned table ����
                 *  Partition Summary�� �������� �ʴ´�.
                 */
                sMyGraph->insertTableRef->partitionSummary = NULL;

                // row movement�� Ȱ��ȭ �Ǿ� �ִٸ�
                // pre pruning�� �����ϰ� �ٸ� ��Ƽ�ǿ���
                // row�� �Űܰ� �� �־�� �Ѵ�.
                sMyGraph->insertTableRef->partitionRef = NULL;

                sMyGraph->insertTableRef->flag &=
                    ~QMS_TABLE_REF_PRE_PRUNING_MASK;
                sMyGraph->insertTableRef->flag |=
                    QMS_TABLE_REF_PRE_PRUNING_FALSE;

                // optimizeInto�� ���.
                // insert(row movement)�� �� �ִ� �÷��� ���Ͽ� �ֱ� ����.
                // values��忡 ���ؼ��� pruning�� �Ѵ�.
                // list���� �ᱹ values���� �����Ǹ�,
                // subquery���� ��� pruning��󿡼� �������� ���ܵȴ�.
                IDE_TEST( qmoPartition::optimizeInto(
                              aStatement,
                              sMyGraph->insertTableRef )
                          != IDE_SUCCESS );
            
                // insert�� �Ͼ�� partition list����
                // update�� �Ͼ�� partition list�� ���� ���� ������ �����Ѵ�.
                // insert/delete/update�� �ϳ��� Ŀ�������� �ؾ� �ϱ� ������.
                IDE_TEST( qmoPartition::minusPartitionRef(
                              sMyGraph->insertTableRef,
                              sTableRef )
                          != IDE_SUCCESS );

                /* PROJ-2464 hybrid partitioned table ���� */
                IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sMyGraph->insertTableRef )
                          != IDE_SUCCESS );

                // row movement�� �߻��ϸ� delete-insert�� ó���ϴ� update
                sMyGraph->updateType = QMO_UPDATE_ROWMOVEMENT;    
                // insert/update/delete�� ��� �߻��� �� �ִ� cursor
                sMyGraph->cursorType = SMI_COMPOSITE_CURSOR;
            }
            else
            {
                sMyGraph->insertTableRef = NULL;
                
                // row movement�� �߻��ϸ� �������� update
                sMyGraph->updateType = QMO_UPDATE_CHECK_ROWMOVEMENT;
                // update cursor
                sMyGraph->cursorType = SMI_UPDATE_CURSOR;
            }
        }
        else
        {
            sMyGraph->insertTableRef = NULL;
            
            // row movement�� �Ͼ�� �ʴ� update
            sMyGraph->updateType = QMO_UPDATE_NO_ROWMOVEMENT;
            // update cursor
            sMyGraph->cursorType = SMI_UPDATE_CURSOR;
        }
    }
    else
    {
        sMyGraph->insertTableRef = NULL;
        
        // �Ϲ� ���̺��� update
        sMyGraph->updateType = QMO_UPDATE_NORMAL;
        // update cursor
        sMyGraph->cursorType = SMI_UPDATE_CURSOR;
    }

    // PROJ-1502 PARTITIONED DISK TABLE
    // rowmovement�� �Ͼ ���ɼ��� �ִ����� parsetree�� ����.
    sMyGraph->isRowMovementUpdate = sIsRowMovementUpdate;

    //---------------------------------------------------
    // Cursor ����
    //---------------------------------------------------
    
    //----------------------------------
    // PROJ-1509
    // INPLACE UPDATE ���� ����
    // MEMORY table������,
    // trigger or foreign key�� �����ϴ� ���,
    // ���� ����/���� ���� �б� ���ؼ���
    // inplace update�� ���� �ʵ��� flag������ sm�� �����ش�.
    //----------------------------------

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( sTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
    {
        if ( ( sTableRef->partitionSummary->memoryPartitionCount +
               sTableRef->partitionSummary->volatilePartitionCount ) > 0 )
        {
            sHasMemory = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( ( ( sTableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_MEMORY ) ||
             ( ( sTableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_VOLATILE ) )
        {
            sHasMemory = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // inplace update
    if ( ( sHasMemory == ID_TRUE ) &&
         ( ( sTableRef->tableInfo->foreignKeys != NULL ) ||
           ( sTableRef->tableInfo->triggerInfo != NULL ) ) )
    {
        sMyGraph->inplaceUpdate = ID_FALSE;
    }
    else
    {
        sMyGraph->inplaceUpdate = ID_TRUE;
    }

    //---------------------------------------------------
    // SCAN Limit ����ȭ
    //---------------------------------------------------

    sIsScanLimit = ID_FALSE;
    if ( sMyGraph->limit != NULL )
    {
        if ( sChildGraph->type == QMG_SELECTION )
        {
            //---------------------------------------------------
            // ������ �Ϲ� qmgSelection�� ���
            // ��, Set, Order By, Group By, Aggregation, Distinct, Join��
            //  ���� ���
            //---------------------------------------------------
            if ( sChildGraph->myFrom->tableRef->view == NULL )
            {
                // View �� �ƴ� ���, SCAN Limit ����
                sNode = (qtcNode *)sChildGraph->myQuerySet->SFWGH->where;

                if ( sNode != NULL )
                {
                    // where�� �����ϴ� ���, subquery ���� ���� �˻�
                    if ( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK )
                         != QTC_NODE_SUBQUERY_EXIST )
                    {
                        sIsScanLimit = ID_TRUE;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    // where�� �������� �ʴ� ���,SCAN Limit ����
                    sIsScanLimit = ID_TRUE;
                }
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // Set, Order By, Group By, Distinct, Aggregation, Join, View��
            // �ִ� ��� :
            // nothing to do
        }
    }
    else
    {
        // nothing to do
    }

    //---------------------------------------------------
    // SCAN Limit Tip�� ����� ���
    //---------------------------------------------------

    if ( sIsScanLimit == ID_TRUE )
    {
        ((qmgSELT *)sChildGraph)->limit = sMyGraph->limit;

        // To Fix BUG-9560
        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmsLimit ),
                                           ( void**) & sLimit )
            != IDE_SUCCESS );

        // fix BUG-13482
        // parse tree�� limit������ ���� ������,
        // UPTE ��� ������,
        // ���� SCAN ��忡�� SCAN Limit ������ Ȯ���Ǹ�,
        // UPTE ����� limit start�� 1�� �����Ѵ�.

        qmsLimitI::setStart( sLimit,
                             qmsLimitI::getStart( sMyGraph->limit ) );

        qmsLimitI::setCount( sLimit,
                             qmsLimitI::getCount( sMyGraph->limit ) );

        SET_EMPTY_POSITION( sLimit->limitPos );

        sMyGraph->limit = sLimit;
    }
    else
    {
        // nothing to do
    }

    //---------------------------------------------------
    // ���� ��� ���� ����
    //---------------------------------------------------

    // recordSize
    sMyGraph->graph.costInfo.recordSize =
        sChildGraph->costInfo.recordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = sUpdateSelectivity;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sChildGraph->costInfo.outputRecordCnt;

    // outputRecordCnt
    sOutputRecordCnt = sChildGraph->costInfo.outputRecordCnt * sUpdateSelectivity;
    sMyGraph->graph.costInfo.outputRecordCnt = sOutputRecordCnt;

    // myCost
    sMyGraph->graph.costInfo.myAccessCost = 0;
    sMyGraph->graph.costInfo.myDiskCost = 0;
    sMyGraph->graph.costInfo.myAllCost = 0;

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost =
        sChildGraph->costInfo.totalAccessCost +
        sMyGraph->graph.costInfo.myAccessCost;

    sMyGraph->graph.costInfo.totalDiskCost =
        sChildGraph->costInfo.totalDiskCost +
        sMyGraph->graph.costInfo.myDiskCost;

    sMyGraph->graph.costInfo.totalAllCost =
        sChildGraph->costInfo.totalAllCost +
        sMyGraph->graph.costInfo.myAllCost;

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |=
        ( sChildGraph->flag & QMG_PRESERVED_ORDER_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgUpdate::makePlan( qcStatement     * aStatement,
                     const qmgGraph  * aParent,
                     qmgGraph        * aGraph )
{
/***********************************************************************
 *
 * Description : qmgUpdate���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgUpdate���� ���������� Plan
 *
 *           [UPTE]
 *
 ***********************************************************************/

    qmgUPTE         * sMyGraph;
    qmnPlan         * sPlan;
    qmnPlan         * sChildPlan;
    qmmSubqueries   * sSubquery;
    qmmValueNode    * sValue;
    qmoUPTEInfo       sUPTEInfo;

    qmsPartitionRef * sPartitionRef = NULL;
    UInt              sRowOffset    = 0;
    qmmMultiTables  * sTmp;

    IDU_FIT_POINT_FATAL( "qmgUpdate::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgUPTE*) aGraph;

    //---------------------------
    // Current CNF�� ���
    //---------------------------

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
    sMyGraph->graph.flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    sMyGraph->graph.flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;

    //---------------------------
    // Plan�� ����
    //---------------------------

    // �ֻ��� plan�̴�.
    IDE_DASSERT( aParent == NULL );

    if ( sMyGraph->mTableList == NULL )
    {
        IDE_TEST( qmoOneNonPlan::initUPTE( aStatement ,
                                           &sPlan )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qmoOneNonPlan::initMultiUPTE( aStatement,
                                                sMyGraph->graph.myQuerySet,
                                                sMyGraph->mTableList,
                                                &sPlan )
                  != IDE_SUCCESS);
    }
    sMyGraph->graph.myPlan = sPlan;

    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( sMyGraph->graph.left->makePlan( aStatement ,
                                              &sMyGraph->graph,
                                              sMyGraph->graph.left )
              != IDE_SUCCESS);

    // fix BUG-13482
    // SCAN Limit ����ȭ ���뿡 ���� UPTE plan�� start value ��������
    if( sMyGraph->graph.left->type == QMG_SELECTION )
    {
        // projection ������ SCAN�̰�,
        // SCAN limit ����ȭ�� ����� ����̸�,
        // UPTE�� limit start value�� 1�� �����Ѵ�.
        if( ((qmgSELT*)(sMyGraph->graph.left))->limit != NULL )
        {
            qmsLimitI::setStartValue( sMyGraph->limit, 1 );
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

    // child��������
    sChildPlan = sMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ���� 
    //---------------------------------------------------
    sMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_UPDATE;

    //----------------------------
    // UPTE�� ����
    //----------------------------
    
    sUPTEInfo.updateTableRef      = sMyGraph->updateTableRef;
    sUPTEInfo.columns             = sMyGraph->columns;
    sUPTEInfo.updateColumnList    = sMyGraph->updateColumnList;
    sUPTEInfo.updateColumnCount   = sMyGraph->updateColumnCount;
    sUPTEInfo.values              = sMyGraph->values;
    sUPTEInfo.subqueries          = sMyGraph->subqueries;
    sUPTEInfo.valueIdx            = sMyGraph->valueIdx;
    sUPTEInfo.canonizedTuple      = sMyGraph->canonizedTuple;
    sUPTEInfo.compressedTuple     = sMyGraph->compressedTuple;  // PROJ-2264
    sUPTEInfo.isNull              = sMyGraph->isNull;
    sUPTEInfo.nextValSeqs         = sMyGraph->nextValSeqs;
    sUPTEInfo.insteadOfTrigger    = sMyGraph->insteadOfTrigger;
    sUPTEInfo.updateType          = sMyGraph->updateType;
    sUPTEInfo.cursorType          = sMyGraph->cursorType;
    sUPTEInfo.inplaceUpdate       = sMyGraph->inplaceUpdate;
    sUPTEInfo.insertTableRef      = sMyGraph->insertTableRef;
    sUPTEInfo.isRowMovementUpdate = sMyGraph->isRowMovementUpdate;
    sUPTEInfo.limit               = sMyGraph->limit;
    sUPTEInfo.parentConstraints   = sMyGraph->parentConstraints;
    sUPTEInfo.childConstraints    = sMyGraph->childConstraints;
    sUPTEInfo.checkConstrList     = sMyGraph->checkConstrList;
    sUPTEInfo.returnInto          = sMyGraph->returnInto;

    /* PROJ-1090 Function-based Index */
    sUPTEInfo.defaultExprTableRef    = sMyGraph->defaultExprTableRef;
    sUPTEInfo.defaultExprColumns     = sMyGraph->defaultExprColumns;
    sUPTEInfo.defaultExprBaseColumns = sMyGraph->defaultExprBaseColumns;
    sUPTEInfo.mTableList             = sMyGraph->mTableList;

    if ( sMyGraph->mTableList == NULL )
    {
        IDE_TEST( qmoOneNonPlan::makeUPTE( aStatement ,
                                           sMyGraph->graph.myQuerySet ,
                                           & sUPTEInfo ,
                                           sChildPlan ,
                                           sPlan )
                  != IDE_SUCCESS);

        sMyGraph->graph.myPlan = sPlan;

        /* PROJ-2464 hybrid partitioned table ���� */
        // qmnUPTE::updateOneRowForRowmovement()���� rowOffset�� ����Ѵ�.
        if ( sMyGraph->insertTableRef != NULL )
        {
            for ( sPartitionRef = sMyGraph->insertTableRef->partitionRef;
                  sPartitionRef != NULL;
                  sPartitionRef = sPartitionRef->next )
            {
                sRowOffset = qmc::getRowOffsetForTuple( &(QC_SHARED_TMPLATE(aStatement)->tmplate),
                                                        sPartitionRef->table );
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].rowOffset  = sRowOffset;
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].rowMaximum = sRowOffset;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        IDE_TEST( qmoOneNonPlan::makeMultiUPTE( aStatement,
                                                sMyGraph->graph.myQuerySet,
                                                &sUPTEInfo,
                                                sChildPlan,
                                                sPlan )
                  != IDE_SUCCESS);

        sMyGraph->graph.myPlan = sPlan;

        for ( sTmp = sMyGraph->mTableList; sTmp != NULL; sTmp = sTmp->mNext )
        {
            if ( sTmp->mInsertTableRef != NULL )
            {
                for ( sPartitionRef = sTmp->mInsertTableRef->partitionRef;
                      sPartitionRef != NULL;
                      sPartitionRef = sPartitionRef->next )
                {
                    sRowOffset = qmc::getRowOffsetForTuple( &(QC_SHARED_TMPLATE(aStatement)->tmplate),
                                                            sPartitionRef->table );
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].rowOffset  = sRowOffset;
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sPartitionRef->table].rowMaximum = sRowOffset;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    //------------------------------------------
    // MAKE GRAPH
    // BUG-32584
    // ��� ���������� ���ؼ� MakeGraph ���Ŀ� MakePlan�� �ؾ� �Ѵ�.
    //------------------------------------------

    //------------------------------------------
    // SET ���� ���� Subquery ����ȭ
    // ex) SET i1 = (subquery)
    //------------------------------------------

    for ( sSubquery = sMyGraph->subqueries;
          sSubquery != NULL;
          sSubquery = sSubquery->next )
    {
        if ( sSubquery->subquery != NULL )
        {
            // Subquery ����ȭ
            IDE_TEST( qmoSubquery::optimizeExprMakeGraph( aStatement,
                                                          ID_UINT_MAX,
                                                          sSubquery->subquery )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // SET ������ LIST ���� Subquery ����ȭ
    // ex) SET (i1,i2) = (SELECT a1, a2 ... )
    //------------------------------------------

    for ( sValue = sMyGraph->lists;
          sValue != NULL;
          sValue = sValue->next )
    {
        if ( sValue->value != NULL )
        {
            // Subquery ������ ��� Subquery ����ȭ
            if ( (sValue->value->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_TEST( qmoSubquery::optimizeExprMakeGraph( aStatement,
                                                              ID_UINT_MAX,
                                                              sValue->value )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nohting To Do
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    //------------------------------------------
    // VALUE ���� Subquery ����ȭ
    // ex) SET i1 = (1 + subquery)
    //------------------------------------------

    for ( sValue = sMyGraph->values;
          sValue != NULL;
          sValue = sValue->next )
    {
        if ( sValue->value != NULL )
        {
            // Subquery ������ ��� Subquery ����ȭ
            if ( (sValue->value->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_TEST( qmoSubquery::optimizeExprMakeGraph( aStatement,
                                                              ID_UINT_MAX,
                                                              sValue->value )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nohting To Do
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    //------------------------------------------
    // MAKE PLAN
    // BUG-32584
    // ��� ���������� ���ؼ� MakeGraph ���Ŀ� MakePlan�� �ؾ� �Ѵ�.
    //------------------------------------------

    //------------------------------------------
    // SET ���� ���� Subquery ����ȭ
    // ex) SET i1 = (subquery)
    //------------------------------------------

    for ( sSubquery = sMyGraph->subqueries;
          sSubquery != NULL;
          sSubquery = sSubquery->next )
    {
        if ( sSubquery->subquery != NULL )
        {
            // Subquery ����ȭ
            IDE_TEST( qmoSubquery::optimizeExprMakePlan( aStatement,
                                                         ID_UINT_MAX,
                                                         sSubquery->subquery )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //------------------------------------------
    // SET ������ LIST ���� Subquery ����ȭ
    // ex) SET (i1,i2) = (SELECT a1, a2 ... )
    //------------------------------------------

    for ( sValue = sMyGraph->lists;
          sValue != NULL;
          sValue = sValue->next )
    {
        if ( sValue->value != NULL )
        {
            // Subquery ������ ��� Subquery ����ȭ
            if ( (sValue->value->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_TEST( qmoSubquery::optimizeExprMakePlan( aStatement,
                                                             ID_UINT_MAX,
                                                             sValue->value )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nohting To Do
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    //------------------------------------------
    // VALUE ���� Subquery ����ȭ
    // ex) SET i1 = (1 + subquery)
    //------------------------------------------

    for ( sValue = sMyGraph->values;
          sValue != NULL;
          sValue = sValue->next )
    {
        if ( sValue->value != NULL )
        {
            // Subquery ������ ��� Subquery ����ȭ
            if ( (sValue->value->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_TEST( qmoSubquery::optimizeExprMakePlan( aStatement,
                                                             ID_UINT_MAX,
                                                             sValue->value )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nohting To Do
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgUpdate::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgUpdate::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

    //-----------------------------------
    // Graph�� ���� ���
    //-----------------------------------

    if ( aDepth == 0 )
    {
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "----------------------------------------------------------" );
    }
    else
    {
        // Nothing To Do
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


    //-----------------------------------
    // Child Graph ���� ������ ���
    //-----------------------------------

    IDE_TEST( aGraph->left->printGraph( aStatement,
                                        aGraph->left,
                                        aDepth + 1,
                                        aString )
              != IDE_SUCCESS );

    //-----------------------------------
    // Graph�� ������ ���
    //-----------------------------------

    if ( aDepth == 0 )
    {
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "----------------------------------------------------------\n\n" );
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**
 * Description : qmgMultiUpdate�� ����ȭ
 *
 * Implementation :
 *    (1) CASE 2 : UPDATE...SET column = (subquery)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (2) CASE 3 : UPDATE...SET (column list) = (subquery)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (3) ���� ��� ���� ����
 */
IDE_RC qmgUpdate::optimizeMultiUpdate( qcStatement * aStatement, qmgGraph * aGraph )
{
    qmgUPTE           * sMyGraph;
    qmgGraph          * sChildGraph;
    qmmMultiTables    * sTmp;
    idBool              sIsIntersect = ID_FALSE;
    idBool              sHasMemory = ID_FALSE;
    SDouble             sOutputRecordCnt;
    SDouble             sUpdateSelectivity = 1;    // BUG-17166

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------
    sMyGraph = (qmgUPTE*) aGraph;
    sChildGraph = aGraph->left;

    //---------------------------------------------------
    // update hidden column ����ȭ
    //---------------------------------------------------
    for ( sTmp = sMyGraph->mTableList; sTmp != NULL; sTmp = sTmp->mNext )
    {
        /* PROJ-1090 Function-based Index */
        IDE_TEST( qmsDefaultExpr::makeBaseColumn( aStatement,
                                                  sTmp->mTableRef->tableInfo,
                                                  sTmp->mDefaultColumns,
                                                  &(sTmp->mDefaultBaseColumns) )
                  != IDE_SUCCESS );

        // Disk Table�� Default Expr�� ����
        /* PROJ-2464 hybrid partitioned table ����
         *  qmoPartition::optimizeInto()���� Partition�� Tuple�� �����ϹǷ�,
         *  qmoPartition::optimizeInto()�� ȣ���ϱ� ���� �����ؾ� �Ѵ�.
         */
        qmsDefaultExpr::setUsedColumnToTableRef( &(QC_SHARED_TMPLATE(aStatement)->tmplate),
                                                 sTmp->mTableRef,
                                                 sTmp->mDefaultBaseColumns,
                                                 ID_TRUE );

        //---------------------------------------------------
        // Row Movement ����ȭ
        //---------------------------------------------------
        if ( sTmp->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            IDE_TEST( qmoPartition::isIntersectPartKeyColumn( sTmp->mTableRef->tableInfo->partKeyColumns,
                                                              sTmp->mColumns,
                                                              &sIsIntersect )
                      != IDE_SUCCESS );

            if ( sIsIntersect == ID_TRUE )
            {
                if ( sTmp->mTableRef->tableInfo->rowMovement == ID_TRUE )
                {
                    IDU_FIT_POINT("qmgUpdate::optimizeMultiUpdate::alloc::mInsertTableRef",
                                  idERR_ABORT_InsufficientMemory);
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsTableRef),
                                                             (void**)&sTmp->mInsertTableRef )
                              != IDE_SUCCESS );
                    idlOS::memcpy( sTmp->mInsertTableRef,
                                   sTmp->mTableRef,
                                   ID_SIZEOF(qmsTableRef) );
                    /* PROJ-2464 hybrid partitioned table ����
                     *  Partition Summary�� �������� �ʴ´�.
                     */
                    sTmp->mInsertTableRef->partitionSummary = NULL;

                    // row movement�� Ȱ��ȭ �Ǿ� �ִٸ�
                    // pre pruning�� �����ϰ� �ٸ� ��Ƽ�ǿ���
                    // row�� �Űܰ� �� �־�� �Ѵ�.
                    sTmp->mInsertTableRef->partitionRef = NULL;

                    sTmp->mInsertTableRef->flag &= ~QMS_TABLE_REF_PRE_PRUNING_MASK;
                    sTmp->mInsertTableRef->flag |= QMS_TABLE_REF_PRE_PRUNING_FALSE;

                    // optimizeInto�� ���.
                    // insert(row movement)�� �� �ִ� �÷��� ���Ͽ� �ֱ� ����.
                    // values��忡 ���ؼ��� pruning�� �Ѵ�.
                    // list���� �ᱹ values���� �����Ǹ�,
                    // subquery���� ��� pruning��󿡼� �������� ���ܵȴ�.
                    IDE_TEST( qmoPartition::optimizeInto( aStatement,
                                                          sTmp->mInsertTableRef )
                              != IDE_SUCCESS );

                    // insert�� �Ͼ�� partition list����
                    // update�� �Ͼ�� partition list�� ���� ���� ������ �����Ѵ�.
                    // insert/delete/update�� �ϳ��� Ŀ�������� �ؾ� �ϱ� ������.
                    IDE_TEST( qmoPartition::minusPartitionRef( sTmp->mInsertTableRef,
                                                               sTmp->mTableRef )
                              != IDE_SUCCESS );

                    /* PROJ-2464 hybrid partitioned table ���� */
                    IDE_TEST( qcmPartition::makePartitionSummary( aStatement, sTmp->mInsertTableRef )
                              != IDE_SUCCESS );

                    // row movement�� �߻��ϸ� delete-insert�� ó���ϴ� update
                    sTmp->mUpdateType = QMO_UPDATE_ROWMOVEMENT;
                    // insert/update/delete�� ��� �߻��� �� �ִ� cursor
                    sTmp->mCursorType = SMI_COMPOSITE_CURSOR;
                }
                else
                {
                    sTmp->mInsertTableRef = NULL;
                    // row movement�� �߻��ϸ� �������� update
                    sTmp->mUpdateType = QMO_UPDATE_CHECK_ROWMOVEMENT;
                    // update cursor
                    sTmp->mCursorType = SMI_UPDATE_CURSOR;
                }
            }
            else
            {
                sTmp->mInsertTableRef = NULL;
                // row movement�� �Ͼ�� �ʴ� update
                sTmp->mUpdateType = QMO_UPDATE_NO_ROWMOVEMENT;
                // update cursor
                sTmp->mCursorType = SMI_UPDATE_CURSOR;
            }
        }
        else
        {
            sTmp->mInsertTableRef = NULL;
            // row movement�� �Ͼ�� �ʴ� update
            sTmp->mUpdateType = QMO_UPDATE_NO_ROWMOVEMENT;
            // update cursor
            sTmp->mCursorType = SMI_UPDATE_CURSOR;
        }
    }
    sMyGraph->insertTableRef = NULL;
    // row movement�� �Ͼ�� �ʴ� update
    sMyGraph->updateType = QMO_UPDATE_NO_ROWMOVEMENT;
    // update cursor
    sMyGraph->cursorType = SMI_UPDATE_CURSOR;
    // PROJ-1502 PARTITIONED DISK TABLE
    // rowmovement�� �Ͼ ���ɼ��� �ִ����� parsetree�� ����.
    sMyGraph->isRowMovementUpdate = ID_FALSE;

    //---------------------------------------------------
    // Cursor ����
    //---------------------------------------------------

    //----------------------------------
    // PROJ-1509
    // INPLACE UPDATE ���� ����
    // MEMORY table������,
    // trigger or foreign key�� �����ϴ� ���,
    // ���� ����/���� ���� �б� ���ؼ���
    // inplace update�� ���� �ʵ��� flag������ sm�� �����ش�.
    //----------------------------------

    for ( sTmp = sMyGraph->mTableList; sTmp != NULL; sTmp = sTmp->mNext )
    {
        /* PROJ-2464 hybrid partitioned table ���� */
        if ( sTmp->mTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
        {
            if ( ( sTmp->mTableRef->partitionSummary->memoryPartitionCount +
                   sTmp->mTableRef->partitionSummary->volatilePartitionCount ) > 0 )
            {
                sHasMemory = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            if ( ( ( sTmp->mTableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_MEMORY ) ||
                 ( ( sTmp->mTableRef->tableInfo->tableFlag & SMI_TABLE_TYPE_MASK ) == SMI_TABLE_VOLATILE ) )
            {
                sHasMemory = ID_TRUE;
            }
            else
            {
                /* Nothing to do */
            }
        }
        // inplace update
        if ( ( sHasMemory == ID_TRUE ) &&
             ( ( sTmp->mTableRef->tableInfo->foreignKeys != NULL ) ||
               ( sTmp->mTableRef->tableInfo->triggerInfo != NULL ) ) )
        {
            sTmp->mInplaceUpdate = ID_FALSE;
        }
        else
        {
            sTmp->mInplaceUpdate = ID_TRUE;
        }
    }

    //---------------------------------------------------
    // ���� ��� ���� ����
    //---------------------------------------------------

    // recordSize
    sMyGraph->graph.costInfo.recordSize = sChildGraph->costInfo.recordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = sUpdateSelectivity;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt = sChildGraph->costInfo.outputRecordCnt;

    // outputRecordCnt
    sOutputRecordCnt = sChildGraph->costInfo.outputRecordCnt * sUpdateSelectivity;
    sMyGraph->graph.costInfo.outputRecordCnt = sOutputRecordCnt;

    // myCost
    sMyGraph->graph.costInfo.myAccessCost = 0;
    sMyGraph->graph.costInfo.myDiskCost = 0;
    sMyGraph->graph.costInfo.myAllCost = 0;

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost = sChildGraph->costInfo.totalAccessCost +
                                                sMyGraph->graph.costInfo.myAccessCost;

    sMyGraph->graph.costInfo.totalDiskCost = sChildGraph->costInfo.totalDiskCost +
                                                sMyGraph->graph.costInfo.myDiskCost;

    sMyGraph->graph.costInfo.totalAllCost = sChildGraph->costInfo.totalAllCost +
                                            sMyGraph->graph.costInfo.myAllCost;

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= ( sChildGraph->flag & QMG_PRESERVED_ORDER_MASK );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

