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
 * $Id: qmgInsert.cpp 53774 2012-06-15 04:53:31Z eerien $
 *
 * Description :
 *     Insert Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgInsert.h>
#include <qmo.h>
#include <qmoOneNonPlan.h>
#include <qmoPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE
#include <qcmPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE

IDE_RC
qmgInsert::init( qcStatement      * aStatement,
                 qmmInsParseTree  * aParseTree,
                 qmgGraph         * aChildGraph,
                 qmgGraph        ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgInsert Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgInsert�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgINST         * sMyGraph;
    qmsTableRef     * sInsertTableRef;

    IDU_FIT_POINT_FATAL( "qmgInsert::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );

    //---------------------------------------------------
    // Insert Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------
    
    // qmgInsert�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgINST ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_INSERT;
    sMyGraph->graph.left = aChildGraph;
    if ( aChildGraph != NULL )
    {
        qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                                   & aChildGraph->depInfo );
    }
    else
    {
        // Nothing to do.
    }

    // insert�� querySet�� ����
    sMyGraph->graph.myQuerySet = NULL;

    sMyGraph->graph.optimize = qmgInsert::optimize;
    sMyGraph->graph.makePlan = qmgInsert::makePlan;
    sMyGraph->graph.printGraph = qmgInsert::printGraph;

    sInsertTableRef = aParseTree->insertTableRef;
    
    // Disk/Memory ���� ����
    if ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sInsertTableRef->table].lflag
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
    // Insert Graph ���� ���� �ʱ�ȭ
    //---------------------------------------------------

    // �ֻ��� graph�� insert graph�� insert table ������ ����
    sMyGraph->tableRef = sInsertTableRef;
    
    // �ֻ��� graph�� insert graph�� insert columns ������ ����
    sMyGraph->columns          = aParseTree->insertColumns;
    sMyGraph->columnsForValues = aParseTree->columnsForValues;
    
    // �ֻ��� graph�� insert graph�� insert values ������ ����
    sMyGraph->rows           = aParseTree->rows;
    sMyGraph->valueIdx       = aParseTree->valueIdx;
    sMyGraph->canonizedTuple = aParseTree->canonizedTuple;
    sMyGraph->compressedTuple= aParseTree->compressedTuple;     // PROJ-2264

    // �ֻ��� graph�� insert graph�� queue ������ ����
    sMyGraph->queueMsgIDSeq = aParseTree->queueMsgIDSeq;

    // �ֻ��� graph�� insert graph�� hint ������ ����
    sMyGraph->hints = aParseTree->hints;

    // �ֻ��� graph�� insert graph�� sequence ������ ����
    sMyGraph->nextValSeqs = aParseTree->common.nextValSeqs;
    
    // �ֻ��� graph�� insert graph�� multi-insert ������ ����
    if ( (aParseTree->flag & QMM_MULTI_INSERT_MASK) == QMM_MULTI_INSERT_TRUE )
    {
        sMyGraph->multiInsertSelect = ID_TRUE;
    }
    else
    {
        sMyGraph->multiInsertSelect = ID_FALSE;
    }
    
    // �ֻ��� graph�� insert graph�� instead of trigger������ ����
    sMyGraph->insteadOfTrigger = aParseTree->insteadOfTrigger;

    // �ֻ��� graph�� insert graph�� constraint�� ����
    sMyGraph->parentConstraints = aParseTree->parentConstraints;
    sMyGraph->checkConstrList   = aParseTree->checkConstrList;

    // �ֻ��� graph�� insert graph�� return into�� ����
    sMyGraph->returnInto = aParseTree->returnInto;

    // �ֻ��� graph�� insert graph�� Default Expr�� ����
    sMyGraph->defaultExprTableRef = aParseTree->defaultTableRef;
    sMyGraph->defaultExprColumns  = aParseTree->defaultExprColumns;

    // BUG-43063 insert nowait
    sMyGraph->lockWaitMicroSec = aParseTree->lockWaitMicroSec;
    
    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgInsert::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgInsert�� ����ȭ
 *
 * Implementation :
 *    (1) CASE 1 : INSERT...VALUE(...(subquery)...)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (2) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgINST         * sMyGraph;
    qcmTableInfo    * sTableInfo;

    IDU_FIT_POINT_FATAL( "qmgInsert::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgINST*) aGraph;
    sTableInfo = sMyGraph->tableRef->tableInfo;
    
    //-----------------------------------------
    // PROJ-1566
    // INSERT APPEND Hint ó��
    // �Ʒ� ���� ������ �������� �ʴ� ���, append hint�� ����
    //-----------------------------------------

    if ( sMyGraph->hints != NULL )
    {
        if ( ( sMyGraph->hints->directPathInsHintFlag &
               SMI_INSERT_METHOD_MASK )
             == SMI_INSERT_METHOD_APPEND )
        {
            // APPEND Hint�� �־��� ���

            while(1)
            {
                if ( sTableInfo->indexCount != 0 )
                {
                    // (2) ��� table�� index�� ���� �� ����.
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }

                if ( sTableInfo->triggerCount != 0 )
                {
                    // (3) ��� table�� trigger�� ���� �� ����.
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }

                if ( sTableInfo->foreignKeyCount != 0 )
                {
                    // (4) ��� table�� foreign key�� ���� �� ����.
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }

                /* BUG-29753 Direct-Path INSERT ���� ���ǿ���
                   NOT NULL constraint ���� ����
                if ( sTableInfo->notNullCount != 0 )
                {
                    // (5) ��� table�� not null constraint�� ���� �� ����.
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }
                */

                /* PROJ-1107 Check Constraint ���� */
                if ( sTableInfo->checkCount != 0 )
                {
                    /* (6) ��� table�� check constraint�� ���� �� ����. */
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }
                else
                {
                    /* Nothing to do */
                }

                if ( sTableInfo->replicationCount != 0 )
                {
                    // (7) replication �� �� ����.
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }

                /* PROJ-2464 hybrid partitioned table ����
                 *  APPEND Hint�� Partitioned Table�� ������� Disk Partition�� �ִ� ��쿡�� �����Ѵ�.
                 *  ���⿡�� Partition ������ �� �� ����. qmgInsert::makePlan()���� ó���Ѵ�.
                 */
                // if ( sTableFlag != SMI_TABLE_DISK )
                // {
                //     // (8) ��� table�� disk table�̾�� �Ѵ�.
                //     //     memory table�� APPEND ������� insert �� �� ����
                //     sMyGraph->hints->directPathInsHintFlag
                //         = SMI_INSERT_METHOD_NORMAL;
                //     break;
                // }
                // else
                // {
                //     /* Nothing to do */
                // }

                if ( sTableInfo->lobColumnCount != 0 )
                {
                    // (9) ��� table�� lob column�� �����ϸ� �ȵ�
                    sMyGraph->hints->directPathInsHintFlag
                        = SMI_INSERT_METHOD_NORMAL;
                    break;
                }

                break;
            }
        }
        else
        {
            // APPEND Hint�� �־����� ���� ���
            // nothing to do
        }
    }

    //---------------------------------------------------
    // ���� ��� ���� ����
    //---------------------------------------------------

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt = 0;

    // recordSize
    sMyGraph->graph.costInfo.recordSize = 1;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = 1;

    // outputRecordCnt
    sMyGraph->graph.costInfo.outputRecordCnt = 0;

    // myCost
    sMyGraph->graph.costInfo.myAccessCost = 0;
    sMyGraph->graph.costInfo.myDiskCost = 0;
    sMyGraph->graph.costInfo.myAllCost = 0;

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost = 0;
    sMyGraph->graph.costInfo.totalDiskCost = 0;
    sMyGraph->graph.costInfo.totalAllCost = 0;

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;

    return IDE_SUCCESS;
}

IDE_RC
qmgInsert::makePlan( qcStatement     * aStatement,
                     const qmgGraph  * aParent,
                     qmgGraph        * aGraph )
{
/***********************************************************************
 *
 * Description : qmgInsert���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgInsert���� ���������� Plan
 *
 *           [INST]
 *
 ***********************************************************************/

    qmmInsParseTree * sInsParseTree;
    qcStatement     * sSubQStatement;
    qmgINST         * sMyGraph;
    qmnPlan         * sPlan;
    qmnPlan         * sChildPlan;
    qmmValueNode    * sValueNode;
    qmmMultiRows    * sMultiRows;
    qmoINSTInfo       sINSTInfo;
    qmncINST        * sINST;
    idBool            sIsConst = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgInsert::makePlan::__FT__" );

    sInsParseTree = (qmmInsParseTree *)aStatement->myPlan->parseTree;
    
    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgINST*) aGraph;

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= QMG_PARALLEL_IMPOSSIBLE_FALSE;

    // BUG-38410
    // �ֻ��� plan �̹Ƿ� �⺻���� �����Ѵ�.
    aGraph->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
    aGraph->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;

    //---------------------------
    // Plan�� ����
    //---------------------------

    // �ֻ��� plan�̴�.
    IDE_DASSERT( aParent == NULL );
    
    IDE_TEST( qmoOneNonPlan::initINST( aStatement ,
                                       &sPlan )
              != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sPlan;
    
    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    if ( sMyGraph->graph.left != NULL )
    {
        // BUG-38410
        // SCAN parallel flag �� �ڽ� ���� �����ش�.
        aGraph->left->flag  |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);

        // subquery�� statement�� makePlan�Ѵ�.
        sSubQStatement = sInsParseTree->select;
        
        IDE_TEST( sMyGraph->graph.left->makePlan( sSubQStatement ,
                                                  &sMyGraph->graph,
                                                  sMyGraph->graph.left )
                  != IDE_SUCCESS);

        // child��������
        sChildPlan = sMyGraph->graph.left->myPlan;
    }
    else
    {
        sChildPlan = NULL;
    }

    //----------------------------
    // INST�� ����
    //----------------------------

    sINSTInfo.tableRef          = sMyGraph->tableRef;
    sINSTInfo.columns           = sMyGraph->columns;
    sINSTInfo.columnsForValues  = sMyGraph->columnsForValues;
    sINSTInfo.rows              = sMyGraph->rows;
    sINSTInfo.valueIdx          = sMyGraph->valueIdx;
    sINSTInfo.canonizedTuple    = sMyGraph->canonizedTuple;
    sINSTInfo.compressedTuple   = sMyGraph->compressedTuple;    // PROJ-2264
    sINSTInfo.queueMsgIDSeq     = sMyGraph->queueMsgIDSeq;
    sINSTInfo.hints             = sMyGraph->hints;
    sINSTInfo.nextValSeqs       = sMyGraph->nextValSeqs;
    sINSTInfo.multiInsertSelect = sMyGraph->multiInsertSelect;
    sINSTInfo.insteadOfTrigger  = sMyGraph->insteadOfTrigger;
    sINSTInfo.parentConstraints = sMyGraph->parentConstraints;
    sINSTInfo.checkConstrList   = sMyGraph->checkConstrList;
    sINSTInfo.returnInto        = sMyGraph->returnInto;
    sINSTInfo.lockWaitMicroSec  = sMyGraph->lockWaitMicroSec;

    /* PROJ-1090 Function-based Index */
    sINSTInfo.defaultExprTableRef = sMyGraph->defaultExprTableRef;
    sINSTInfo.defaultExprColumns  = sMyGraph->defaultExprColumns;

    IDE_TEST( qmoOneNonPlan::makeINST( aStatement ,
                                       & sINSTInfo ,
                                       sChildPlan ,
                                       sPlan )
                 != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sPlan;

    if ( sMyGraph->graph.left == NULL )
    {
        /* BUG-42764 Multi Row */
        for ( sMultiRows = sMyGraph->rows;
              sMultiRows != NULL;
              sMultiRows = sMultiRows->next )
        {
            //------------------------------------------
            // INSERT ... VALUES ���� ���� Subquery ����ȭ
            //------------------------------------------
            // BUG-32584
            // ��� ���������� ���ؼ� MakeGraph ���Ŀ� MakePlan�� �ؾ� �Ѵ�.
            for ( sValueNode = sMultiRows->values;
                  sValueNode != NULL;
                  sValueNode = sValueNode->next )
            {
                // Subquery ������ ��� Subquery ����ȭ
                if ( (sValueNode->value->lflag & QTC_NODE_SUBQUERY_MASK )
                     == QTC_NODE_SUBQUERY_EXIST )
                {
                    IDE_TEST( qmoSubquery::optimizeExprMakeGraph( aStatement,
                                                                  ID_UINT_MAX,
                                                                  sValueNode->value )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nohting To Do
                }
            }

            for ( sValueNode = sMultiRows->values;
                  sValueNode != NULL;
                  sValueNode = sValueNode->next )
            {
                // Subquery ������ ��� Subquery ����ȭ
                if ( (sValueNode->value->lflag & QTC_NODE_SUBQUERY_MASK )
                     == QTC_NODE_SUBQUERY_EXIST )
                {
                    IDE_TEST( qmoSubquery::optimizeExprMakePlan( aStatement,
                                                                 ID_UINT_MAX,
                                                                 sValueNode->value )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nohting To Do
                }
            }
        }
    }
    else
    {
        // insert select�� ��� validation�� partitionRef�� �����ߴ�.

        // Nothing to do.
    }

    //----------------------------
    // into���� ���� ��Ƽ�ǵ� ���̺� ����ȭ
    //----------------------------

    /* BUG-47614 partition table insert�� optimize ���� ���� */
    if ( sMyGraph->parentConstraints != NULL )
    {
        sIsConst = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }
    // PROJ-1502 PARTITIONED DISK TABLE
    /* PROJ-2464  hybrid partitioned table ����
     *  - Insert Select�� Partition�� Tuple�� �������� �ʾҴ�.
     *  - HPT�� �߰��� ���� Insert�� �Ǵ� Partition�� Tuple�� ����ؾ� �ϹǷ� ������ ȣ���Ѵ�.
     *  - Move DML�� ȣ�� ������ �����ϰ� �����Ѵ�.
     * BUG-47614 partition table insert�� optimize ���� ����
     */
    IDE_TEST( qmoPartition::optimizeIntoForInsert( aStatement,
                                                   sMyGraph->tableRef,
                                                   sIsConst )
              != IDE_SUCCESS );

    sINST = (qmncINST *)sPlan;

    /* BUG-47625 Insert execution ������� */
    if ( ( sINST->isMultiInsertSelect == ID_TRUE ) ||
         ( sMyGraph->tableRef->tableInfo->lobColumnCount > 0 ) ||
         ( sMyGraph->tableRef->tableInfo->triggerCount > 0 ) ||
         ( sINST->returnInto != NULL ) ||
         ( sINST->checkConstrList != NULL ) ||
         ( sINST->rows->next != NULL ) ||
         ( sINST->compressedTuple != UINT_MAX ) ||
         ( sINST->isAppend == ID_TRUE ) ||
         ( sINST->insteadOfTrigger == ID_TRUE ) ||
         ( sINST->defaultExprColumns != NULL ) ||
         ( ( sINST->flag & QMNC_INST_EXIST_ENCRYPT_COLUMN_MASK )
           == QMNC_INST_EXIST_ENCRYPT_COLUMN_TRUE ) ||
         ( ( sINST->flag & QMNC_INST_EXIST_TIMESTAMP_MASK )
           == QMNC_INST_EXIST_TIMESTAMP_TRUE ) ||
         ( ( sINST->flag & QMNC_INST_EXIST_QUEUE_MASK )
           == QMNC_INST_EXIST_QUEUE_TRUE ) )
    {
        sINST->flag &= ~QMNC_INST_COMPLEX_MASK;
        sINST->flag |= QMNC_INST_COMPLEX_TRUE;
    }
    else
    {
        sINST->flag &= ~QMNC_INST_COMPLEX_MASK;
        sINST->flag |= QMNC_INST_COMPLEX_FALSE;
    }

    if ( sMyGraph->tableRef->partitionSummary != NULL )
    {
        if ( sMyGraph->tableRef->partitionSummary->diskPartitionCount > 0 )
        {
            sINST->isSimple = ID_FALSE;
        }
        else
        {
            /* Nothing to do */
        }

        /* BUG-47625 Insert execution ������� */
        if ( sMyGraph->tableRef->partitionSummary->isHybridPartitionedTable == ID_TRUE )
        {
            sINST->flag &= ~QMNC_INST_COMPLEX_MASK;
            sINST->flag |= QMNC_INST_COMPLEX_TRUE;
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

    /* PROJ-2464 hybrid partitioned table ����
     *  APPEND Hint�� Partitioned Table�� ������� Disk Partition�� �ִ� ��쿡�� �����Ѵ�.
     */
    fixHint( sPlan );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgInsert::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgInsert::printGraph::__FT__" );

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

    if ( aGraph->left != NULL )
    {
        IDE_TEST( aGraph->left->printGraph( aStatement,
                                            aGraph->left,
                                            aDepth + 1,
                                            aString )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

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

/***********************************************************************
 *
 * Description : Disk Partition�� ����Ͽ� INSERT ������ HInt�� ó���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
void qmgInsert::fixHint( qmnPlan * aPlan )
{
    qmncINST    * sINST           = (qmncINST *)aPlan;
    qmsTableRef * sTableRef       = NULL;

    sINST->isAppend = ID_FALSE;

    if ( sINST->hints != NULL )
    {
        if ( ( sINST->hints->directPathInsHintFlag & SMI_INSERT_METHOD_MASK )
                                                  == SMI_INSERT_METHOD_APPEND )
        {
            sTableRef = sINST->tableRef;

            if ( sTableRef->tableInfo->tablePartitionType == QCM_PARTITIONED_TABLE )
            {
                if ( sTableRef->partitionSummary->diskPartitionCount > 0 )
                {
                    sINST->isAppend = ID_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                if ( QCM_TABLE_TYPE_IS_DISK( sTableRef->tableInfo->tableFlag ) == ID_TRUE )
                {
                    sINST->isAppend = ID_TRUE;
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
    }
    else
    {
        /* Nothing to do */
    }

    return;
}
