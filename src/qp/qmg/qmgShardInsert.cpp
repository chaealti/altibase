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
 * $Id$
 *
 * Description :
 *     Shard Insert Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgShardInsert.h>
#include <qmgShardDML.h>
#include <qmo.h>
#include <qmoOneNonPlan.h>
#include <qcg.h>

IDE_RC
qmgShardInsert::init( qcStatement      * aStatement,
                      qmmInsParseTree  * aParseTree,
                      qmgGraph         * aChildGraph,
                      qmgGraph        ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgShardInsert Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgShardInsert�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgShardINST    * sMyGraph;
    qmsTableRef     * sInsertTableRef;

    IDU_FIT_POINT_FATAL( "qmgShardInsert::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );

    //---------------------------------------------------
    // Insert Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgShardInsert�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgShardINST ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SHARD_INSERT;
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

    sMyGraph->graph.optimize = qmgShardInsert::optimize;
    sMyGraph->graph.makePlan = qmgShardInsert::makePlan;
    sMyGraph->graph.printGraph = qmgShardInsert::printGraph;

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

    // �ֻ��� graph�� insert graph�� queue ������ ����
    sMyGraph->queueMsgIDSeq = aParseTree->queueMsgIDSeq;

    // �ֻ��� graph�� insert graph�� sequence ������ ����
    sMyGraph->nextValSeqs = aParseTree->common.nextValSeqs;

    // insert position
    IDE_FT_ASSERT( QC_IS_NULL_NAME( sInsertTableRef->position ) != ID_TRUE );

    SET_POSITION( sMyGraph->insertPos, aParseTree->common.stmtPos );
    sMyGraph->insertPos.size =
        sInsertTableRef->position.offset +
        sInsertTableRef->position.size -
        sMyGraph->insertPos.offset;

    /* BUG-45865 */
    if ( sInsertTableRef->position.stmtText[sInsertTableRef->position.offset +
                                            sInsertTableRef->position.size] == '\"' )
    {
        sMyGraph->insertPos.size += 1;
    }

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgShardInsert::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgShardInsert�� ����ȭ
 *
 * Implementation :
 *    (1) CASE 1 : INSERT...VALUE(...(subquery)...)
 *        qmoSubquery::optimizeExpr()�� ����
 *    (2) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgShardINST    * sMyGraph;
    qcmTableInfo    * sTableInfo;
    sdiObjectInfo   * sShardObjInfo = NULL;
    sdiAnalyzeInfo  * sAnalyzeInfo;
    UInt              sParamCount = 1;
    UInt              sLen;
    UInt              i;
    ULong             sSessionSMN = ID_LONG(0);

    IDU_FIT_POINT_FATAL( "qmgShardInsert::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgShardINST*) aGraph;
    sTableInfo = sMyGraph->tableRef->tableInfo;

    /* PROJ-2701 Sharding online data rebuild */
    sSessionSMN = QCG_GET_SESSION_SHARD_META_NUMBER( aStatement );

    sdi::getShardObjInfoForSMN( sSessionSMN,
                                sMyGraph->tableRef->mShardObjInfo,
                                &sShardObjInfo );

    IDE_TEST_RAISE( sShardObjInfo == NULL, ERR_NO_SHARD_OBJECT );

    sAnalyzeInfo = &(sMyGraph->shardAnalysis);

    //---------------------------------------------------
    // insert�� ���� analyzeInfo ����
    //---------------------------------------------------

    IDE_FT_ASSERT( sShardObjInfo != NULL );

    // analyzer�� ������ �ʰ� ���� analyze ������ �����Ѵ�.
    IDE_TEST( sdi::setAnalysisResultForInsert( aStatement,
                                               sAnalyzeInfo,
                                               sShardObjInfo )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // insert query ����
    //---------------------------------------------------

    idlOS::snprintf( sMyGraph->shardQueryBuf,
                     ID_SIZEOF(sMyGraph->shardQueryBuf),
                     "%.*s VALUES (?",
                     sMyGraph->insertPos.size,
                     sMyGraph->insertPos.stmtText + sMyGraph->insertPos.offset );
    sLen = idlOS::strlen( sMyGraph->shardQueryBuf );

    // ù��° �÷��� hidden column�� �ƴϴ�.
    IDE_FT_ASSERT( sTableInfo->columnCount > 0 );
    IDE_FT_ASSERT( (sTableInfo->columns[0].flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
                 == QCM_COLUMN_HIDDEN_COLUMN_FALSE );

    for ( i = 1; i < sTableInfo->columnCount; i++ )
    {
        if ( (sTableInfo->columns[i].flag & QCM_COLUMN_HIDDEN_COLUMN_MASK)
             == QCM_COLUMN_HIDDEN_COLUMN_FALSE )
        {
            sParamCount++;
            sLen += 2;

            IDE_TEST_RAISE( sLen + 2 >= ID_SIZEOF(sMyGraph->shardQueryBuf),
                            ERR_QUERY_BUFFER_OVERFLOW );
            idlOS::strcat( sMyGraph->shardQueryBuf, ",?" );
        }
        else
        {
            // Nothing to do.
        }
    }

    sLen += 1;
    IDE_TEST_RAISE( sLen + 1 >= ID_SIZEOF(sMyGraph->shardQueryBuf),
                    ERR_QUERY_BUFFER_OVERFLOW );
    idlOS::strcat( sMyGraph->shardQueryBuf, ")" );

    // name position���� ����Ѵ�.
    sMyGraph->shardQuery.stmtText = sMyGraph->shardQueryBuf;
    sMyGraph->shardQuery.offset   = 0;
    sMyGraph->shardQuery.size     = sLen;

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

    IDE_EXCEPTION( ERR_QUERY_BUFFER_OVERFLOW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardInsert::optimize",
                                  "query buffer overflow" ) );
    }
    IDE_EXCEPTION( ERR_NO_SHARD_OBJECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardInsert::optimize",
                                  "no shard object information" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgShardInsert::makePlan( qcStatement     * aStatement,
                          const qmgGraph  * aParent,
                          qmgGraph        * aGraph )
{
/***********************************************************************
 *
 * Description : qmgShardInsert���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgShardInsert���� ���������� Plan
 *
 *           [INST]
 *
 ***********************************************************************/

    qmmInsParseTree * sInsParseTree;
    qcStatement     * sSubQStatement;
    qmgShardINST    * sMyGraph;
    qmnPlan         * sPlan;
    qmnPlan         * sChildPlan;
    qmmValueNode    * sValueNode;
    qmmMultiRows    * sMultiRows;
    qmoINSTInfo       sINSTInfo;

    IDU_FIT_POINT_FATAL( "qmgShardInsert::makePlan::__FT__" );

    sInsParseTree = (qmmInsParseTree *)aStatement->myPlan->parseTree;

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );

    sMyGraph = (qmgShardINST*) aGraph;

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
    IDE_FT_ASSERT( aParent == NULL );

    IDE_TEST( qmoOneNonPlan::initSDIN( aStatement ,
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
    sINSTInfo.queueMsgIDSeq     = sMyGraph->queueMsgIDSeq;
    sINSTInfo.nextValSeqs       = sMyGraph->nextValSeqs;

    IDE_TEST( qmoOneNonPlan::makeSDIN( aStatement ,
                                       &sINSTInfo ,
                                       &(sMyGraph->shardQuery),
                                       &(sMyGraph->shardAnalysis),
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
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgShardInsert::printGraph( qcStatement  * aStatement,
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

    qmgShardINST    * sMyGraph;
    UInt              i;

    IDU_FIT_POINT_FATAL( "qmgShardInsert::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( aString != NULL );

    sMyGraph = (qmgShardINST*)aGraph;

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
    // shard ���� ���
    //-----------------------------------

    IDE_TEST( qmgShardDML::printShardInfo( aStatement,
                                           &(sMyGraph->shardAnalysis),
                                           &(sMyGraph->shardQuery),
                                           aDepth,
                                           aString )
              != IDE_SUCCESS );

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
