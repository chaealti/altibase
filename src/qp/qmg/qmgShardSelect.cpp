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
 * $Id: qmgShardSELT.cpp 77650 2016-10-17 02:19:47Z timmy.kim $
 *
 * Description : Shard Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qcg.h>
#include <qmo.h>
#include <qcgPlan.h>
#include <qmgShardSelect.h>
#include <qmgShardDML.h>
#include <qmgSelection.h>
#include <qmoOneNonPlan.h>
#include <qmgSorting.h>            /* TASK-7219 */
#include <qmoUtil.h>               /* TASK-7219 */
#include <qmoPushPred.h>           /* TASK-7219 */
#include <qmoCheckViewColumnRef.h> /* TASK-7219 */
#include <qmvShardTransform.h>     /* TASK-7219 Shard Transformer Refactoring */

IDE_RC qmgShardSelect::init( qcStatement  * aStatement,
                             qmsQuerySet  * aQuerySet,
                             qmsFrom      * aFrom,
                             qmgGraph    ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgShardSELT Graph�� �ʱ�ȭ
 *
 * Implementation :
 *
 *            - alloc qmgShardSELT
 *            - init qmgShardSELT, qmgGraph
 *
 ***********************************************************************/

    qmgShardSELT * sMyGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmgShardSelect::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aQuerySet != NULL );
    IDE_FT_ASSERT( aFrom != NULL );

    //---------------------------------------------------
    // Shard Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgShardSELT�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgShardSELT ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SHARD_SELECT;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aFrom->depInfo );
    sMyGraph->graph.myFrom = aFrom;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgShardSelect::optimize;
    sMyGraph->graph.makePlan = qmgShardSelect::makePlan;
    sMyGraph->graph.printGraph = qmgShardSelect::printGraph;

    // BUGBUG
    // SDSE ��ü�� disk �� �����ϳ�
    // SDSE �� ���� plan �� interResultType �� memory temp �� ����Ǿ�� �Ѵ�.
    sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
    sMyGraph->graph.flag |=  QMG_GRAPH_TYPE_DISK;

    //---------------------------------------------------
    // Shard ���� ������ �ʱ�ȭ
    //---------------------------------------------------

    SET_POSITION( sMyGraph->shardQuery,
                  aFrom->tableRef->view->myPlan->parseTree->stmtPos );
    sMyGraph->shardAnalysis = aFrom->tableRef->view->myPlan->mShardAnalysis;
    sMyGraph->shardParamInfo = aFrom->tableRef->view->myPlan->mShardParamInfo; /* TASK-7219 Non-shard DML */
    sMyGraph->shardParamOffset = aFrom->tableRef->view->myPlan->mShardParamOffset;
    sMyGraph->shardParamCount = aFrom->tableRef->view->myPlan->mShardParamCount;

    sMyGraph->limit = NULL;
    sMyGraph->accessMethod = NULL;
    sMyGraph->accessMethodCnt = 0;
    sMyGraph->flag = QMG_SHARD_FLAG_CLEAR;

    /* TASK-7219 */
    sMyGraph->mShardQueryBuf = NULL;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::optimize( qcStatement * aStatement,
                                 qmgGraph    * aGraph )
{
/***********************************************************************
 *
 * Description : qmgShardSELT �� ����ȭ
 *
 * Implementation :
 *
 *      - ������� ����
 *      - Subquery Graph ����
 *      - ���� ��� ���� ���� (recordSize, inputRecordCnt)
 *      - Predicate ���ġ �� ���� Predicate�� selectivity ���
 *      - ��ü selectivity ��� �� ���� ��� ������ selectivity�� ����
 *      - Access Method ����
 *      - Preserved Order ����
 *      - ���� ��� ���� ���� (outputRecordCnt, myCost, totalCost)
 *
 ***********************************************************************/

    qmgShardSELT   * sMyGraph  = NULL;
    qmsTarget      * sTarget   = NULL;
    qmsTableRef    * sTableRef = NULL;
    qmsQuerySet    * sQuerySet = NULL;
    qmsParseTree   * sParseTree = NULL;
    mtcColumn      * sTargetColumn = NULL;
    SDouble          sOutputRecordCnt = 0;
    SDouble          sRecordSize = 0;

    IDU_FIT_POINT_FATAL( "qmgShardSelect::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgShardSELT *)aGraph;
    sTableRef = sMyGraph->graph.myFrom->tableRef;

    IDE_FT_ASSERT( sTableRef != NULL );
    IDE_FT_ASSERT( sTableRef->view != NULL );

    //---------------------------------------------------
    // ��� ���� ����
    //---------------------------------------------------

    IDE_TEST( qmoStat::getStatInfo4RemoteTable( aStatement,
                                                sTableRef->tableInfo,
                                                &(sTableRef->statInfo) )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // Subquery�� Graph ����
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        IDE_TEST( qmoPred::optimizeSubqueries( aStatement,
                                               sMyGraph->graph.myPredicate,
                                               ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // ���� ��� ���� ���� (recordSize, inputRecordCnt)
    //---------------------------------------------------

    sParseTree = (qmsParseTree *)sTableRef->view->myPlan->parseTree;

    IDE_FT_ASSERT( sParseTree != NULL );

    for ( sQuerySet = sParseTree->querySet;
          sQuerySet != NULL;
          sQuerySet = sQuerySet->left )
    {
        if ( sQuerySet->setOp == QMS_NONE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    // recordSize ����
    // To Fix BUG-8241
    IDE_FT_ASSERT( sQuerySet != NULL );
    IDE_FT_ASSERT( sQuerySet->SFWGH != NULL );

    for ( sTarget = sQuerySet->SFWGH->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sTargetColumn = QTC_STMT_COLUMN( aStatement, sTarget->targetColumn );

        IDE_FT_ASSERT( sTargetColumn != NULL );

        IDE_TEST_RAISE(
            ( sTargetColumn->module->id != MTD_BIGINT_ID   ) &&
            ( sTargetColumn->module->id != MTD_SMALLINT_ID ) &&
            ( sTargetColumn->module->id != MTD_INTEGER_ID  ) &&
            ( sTargetColumn->module->id != MTD_DOUBLE_ID   ) &&
            ( sTargetColumn->module->id != MTD_FLOAT_ID    ) &&
            ( sTargetColumn->module->id != MTD_REAL_ID     ) &&
            ( sTargetColumn->module->id != MTD_NUMBER_ID   ) &&
            ( sTargetColumn->module->id != MTD_NUMERIC_ID  ) &&
            ( sTargetColumn->module->id != MTD_BINARY_ID   ) &&
            ( sTargetColumn->module->id != MTD_BIT_ID      ) &&
            ( sTargetColumn->module->id != MTD_VARBIT_ID   ) &&
            ( sTargetColumn->module->id != MTD_BYTE_ID     ) &&
            ( sTargetColumn->module->id != MTD_VARBYTE_ID  ) &&
            ( sTargetColumn->module->id != MTD_NIBBLE_ID   ) &&
            ( sTargetColumn->module->id != MTD_DATE_ID     ) &&
            ( sTargetColumn->module->id != MTD_INTERVAL_ID ) &&
            ( sTargetColumn->module->id != MTD_CHAR_ID     ) &&
            ( sTargetColumn->module->id != MTD_VARCHAR_ID  ) &&
            ( sTargetColumn->module->id != MTD_NCHAR_ID    ) &&
            ( sTargetColumn->module->id != MTD_NVARCHAR_ID ) &&
            ( sTargetColumn->module->id != MTD_NULL_ID     ) &&
            ( sTargetColumn->module->id != MTD_GEOMETRY_ID ) &&
            ( sTargetColumn->module->id != MTD_BLOB_LOCATOR_ID ) &&
            ( sTargetColumn->module->id != MTD_CLOB_LOCATOR_ID ),
            ERR_INVALID_SHARD_QUERY );

        sRecordSize += sTargetColumn->column.size;
    }

    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );
    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // inputRecordCnt ����
    sMyGraph->graph.costInfo.inputRecordCnt = sTableRef->statInfo->totalRecordCnt;

    //---------------------------------------------------
    // Predicate�� ���ġ �� ���� Predicate�� Selectivity ���
    //---------------------------------------------------

    if ( sMyGraph->graph.myPredicate != NULL )
    {
        IDE_TEST( qmoPred::relocatePredicate( aStatement,
                                              sMyGraph->graph.myPredicate,
                                              & sMyGraph->graph.depInfo,
                                              & sMyGraph->graph.myQuerySet->outerDepInfo,
                                              sMyGraph->graph.myFrom->tableRef->statInfo,
                                              & sMyGraph->graph.myPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // accessMethod (full scan)
    //---------------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoAccessMethod),
                                             (void**)&sMyGraph->accessMethod)
              != IDE_SUCCESS );

    qmgSelection::setFullScanMethod( aStatement,
                                     sTableRef->statInfo,
                                     sMyGraph->graph.myPredicate,
                                     sMyGraph->accessMethod,
                                     1,           // parallel degree
                                     ID_FALSE );  // execute time

    sMyGraph->accessMethodCnt = 1;

    //---------------------------------------------------
    // Preserved Order ����
    //---------------------------------------------------

    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |=  QMG_PRESERVED_ORDER_NEVER;

    //---------------------------------------------------
    // ���� ��� ���� ���� (outputRecordCnt, myCost, totalCost)
    //---------------------------------------------------

    sMyGraph->graph.costInfo.selectivity = sMyGraph->accessMethod->methodSelectivity;

    // output record count ����
    sOutputRecordCnt = sMyGraph->graph.costInfo.selectivity *
        sMyGraph->graph.costInfo.inputRecordCnt;

    sMyGraph->graph.costInfo.outputRecordCnt =
        ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    // my cost
    sMyGraph->graph.costInfo.myAccessCost = sMyGraph->accessMethod->accessCost;
    sMyGraph->graph.costInfo.myDiskCost = sMyGraph->accessMethod->diskCost;
    sMyGraph->graph.costInfo.myAllCost = sMyGraph->accessMethod->totalCost;

    // total cost
    sMyGraph->graph.costInfo.totalAccessCost = sMyGraph->accessMethod->accessCost;
    sMyGraph->graph.costInfo.totalDiskCost = sMyGraph->accessMethod->diskCost;
    sMyGraph->graph.costInfo.totalAllCost = sMyGraph->accessMethod->totalCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_SHARD_QUERY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_TARGET,
                                  sTargetColumn->module->names->string ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::makePlan( qcStatement    * aStatement,
                                 const qmgGraph * aParent,
                                 qmgGraph       * aGraph )
{
/***********************************************************************
 *
 *  Description : qmgShardSELT �� ���� Plan�� �����Ѵ�.
 *
 *  Implementation : SHARD�� ���� ��� �Ʒ��� ���� plan�� �����Ѵ�.
 *
 *          [PARENT]
 *            |
 *          [SDSE] (SharD SElect)
 *        ----|------------------------------------------
 *          [...]    [...]    [...] : shard data node 
 *    
 ************************************************************************/

    qmgShardSELT * sMyGraph = NULL;
    qmnPlan      * sSDSE    = NULL;
    qtcNode      * sLobFilter; // BUG-25916
    qmnPlan      * sFILT;

    IDU_FIT_POINT_FATAL( "qmgShardSelect::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aParent    != NULL );
    IDE_FT_ASSERT( aGraph     != NULL );

    sMyGraph = (qmgShardSELT*)aGraph;

    /* TASK-7219 */
    IDE_TEST( qmgShardSelect::pushSeriesOptimize( aStatement,
                                                  aParent,
                                                  sMyGraph )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // Current CNF�� ���
    //---------------------------------------------------

    if ( sMyGraph->graph.myCNF != NULL )
    {
        sMyGraph->graph.myQuerySet->SFWGH->crtPath->currentCNF = sMyGraph->graph.myCNF;
    }
    else
    {
        // Nothing to do.
    }

    sMyGraph->graph.myPlan = aParent->myPlan;

    // PROJ-1071 Parallel Query
    sMyGraph->graph.flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    sMyGraph->graph.flag |=  QMG_PARALLEL_IMPOSSIBLE_TRUE;

    //-----------------------
    // init SDSE
    //-----------------------
    IDE_TEST( qmoOneNonPlan::initSDSE( aStatement,
                                       aParent->myPlan,
                                       &sSDSE )
              != IDE_SUCCESS );

    //-----------------------
    // make SDSE
    //-----------------------
    IDE_TEST( qmoOneNonPlan::makeSDSE( aStatement,
                                       aParent->myPlan,
                                       &sMyGraph->shardQuery,
                                       sMyGraph->shardAnalysis,
                                       sMyGraph->shardParamInfo, /* TASK-7219 Non-shard DML */
                                       sMyGraph->shardParamCount,
                                       &sMyGraph->graph,
                                       sSDSE )
              != IDE_SUCCESS );

    qmg::setPlanInfo( aStatement, sSDSE, &(sMyGraph->graph) );

    /* PROJ-2728 Sharding LOB
         BUG-47751 �ݿ����� ���� �Ʒ� BUG-25916 �ڵ� ���� */
    // BUG-25916 : clob�� select for update �ϴ� ���� assert �߻�
    // clob locator�� �������� lobfilter�� �з��� ���� �����ϸ�
    // SCAN ��� ������ FILTER ���� ó���ؾ� �Ѵ�.
    sLobFilter = ((qmncSDSE*)sSDSE)->lobFilter;

    if ( sLobFilter != NULL )
    {
        // BUGBUG
        // Lob filter�� ��� SCAN�� ������ �Ŀ� ������ �� �� �ִ�.
        // ���� FILT�� ���� ���ε� SCAN�� ���� �Ŀ� �����ȴ�.
        // BUG-25916�� ���� �ذ� ����� �����ؾ� �Ѵ�.
        IDE_TEST( qmoOneNonPlan::initFILT(
                      aStatement ,
                      sMyGraph->graph.myQuerySet ,
                      sLobFilter ,
                      sMyGraph->graph.myPlan ,
                      &sFILT) != IDE_SUCCESS);

        IDE_TEST( qmoOneNonPlan::makeFILT(
                      aStatement ,
                      sMyGraph->graph.myQuerySet ,
                      sLobFilter ,
                      NULL,
                      sSDSE ,
                      sFILT) != IDE_SUCCESS);
        sMyGraph->graph.myPlan = sFILT;

        qmg::setPlanInfo( aStatement, sFILT, &(sMyGraph->graph) );
    }
    else
    {
        sMyGraph->graph.myPlan = sSDSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::printGraph( qcStatement  * aStatement,
                                   qmgGraph     * aGraph,
                                   ULong          aDepth,
                                   iduVarString * aString )
{
/***********************************************************************
 *
 * Description : Graph�� �����ϴ� ���� ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sPredicate = NULL;
    qmoPredicate * sMorePred  = NULL;
    qmgShardSELT * sMyGraph = NULL;
    UInt  i;

    IDU_FIT_POINT_FATAL( "qmgShardSelect::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph     != NULL );
    IDE_FT_ASSERT( aString    != NULL );

    sMyGraph = (qmgShardSELT *)aGraph;

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
        // Nothing to do.
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
    // Access method ���� ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "== Access Method Information ==" );

    IDE_TEST( printAccessMethod( aStatement,
                                 sMyGraph->accessMethod,
                                 aDepth,
                                 aString )
              != IDE_SUCCESS );

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
    // shard ���� ���
    //-----------------------------------

    IDE_TEST( qmgShardDML::printShardInfo( aStatement,
                                           sMyGraph->shardAnalysis,
                                           &(sMyGraph->shardQuery),
                                           aDepth,
                                           aString )
              != IDE_SUCCESS );

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
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::printAccessMethod( qcStatement     * /* aStatement */ ,
                                          qmoAccessMethod * aMethod,
                                          ULong             aDepth,
                                          iduVarString    * aString )
{
    UInt i;

    IDU_FIT_POINT_FATAL( "qmgShardSelect::printAccessMethod::__FT__" );

    IDE_FT_ASSERT( aMethod->method == NULL );

    // FULL SCAN
    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "FULL SCAN" );

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

/* TASK-7219 */
IDE_RC qmgShardSelect::copySelect( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Select�� �����Ѵ�. Set ������ ����ؼ� SELECT * ��
 *               �տ� �߰��� �ްų�, ������ Query Text�� �״�� �̿��Ѵ�.
 *
 *  QUERY   / SELECT C1 FROM ( SELECT * FROM T1 ORDER BY C1 )
 *  ANALYZE / SELECT C1 FROM ( SELECT * FROM SHARD( SELECT * FROM T1 ) ORDER BY C1 )
 *
 *  SHARD   /                                       SELECT * FROM T1
 *  COPY    /                                       SELECT
 *
 *  SET     /                 ( SELECT * FROM T1 ) UNION ALL ( SELECT * FROM T1 )
 *  WRAP    / SELECT *
 *
 *  SET     /                 ( SELECT * FROM T1 ) UNION ALL ( SELECT * FROM T1 )
 *  SKIP    /
 *
 *  HALFSET /                 ( SELECT * FROM T1 )
 *  COPY    /                 ( SELECT *
 *
 * Implementation : 1.   SELECT or ( SELECT
 *                  2.1. Set ���δ� Transform�� �����Ѵ�.
 *                  2.2. �⺻ Set ������ ��� copyFrom���� �۾��Ѵ�.
 *                  2.3. ������ Query Text�� �׷��� �����Ѵ�.
 *                  2.4. Bind ������ �����Ѵ�
 *
 ****************************************************************************************/

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mQuerySet == NULL, ERR_NULL_QUERYSET );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    if ( aInfo->mTarget != NULL )
    {
        /* 1.   SELECT or ( SELECT */
        if ( aInfo->mQuerySet->setOp != QMS_NONE )
        {
            /*  Set ���꿡 Push Projection�� Target�� �����Ƿ�,
             *  aInfo->mNeedWrapSet == ID_TRUE�� ��Ȳ.
             *  From���� Set�� ���Ѵ�.
             */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "SELECT " )
                      != IDE_SUCCESS );
        }
        else
        {
            /*  Set ������ �ƴ� ���, �Ʒ��� �� �����̴�.
             *  |  SELECT ...  |  ( SELECT ...  )  |
             *     ^              ^
             *      \______________\____ aInfo->mPosition.offset
             */
            if ( aInfo->mPosition.stmtText[ aInfo->mPosition.offset ] == '(' )
            {
                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              "( SELECT " )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              "SELECT " )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        if ( aInfo->mQuerySet->setOp != QMS_NONE )
        {
            /* 2.1. Set ���δ� Transform�� �����Ѵ�. */
            if ( aInfo->mNeedWrapSet == ID_TRUE )
            {
                /*  Push Projection�� �������� ������, Push Selection�� �����ؼ�,
                 *  aInfo->mNeedWrapSet == ID_TRUE�� ��Ȳ����
                 *  From���� Set�� ���Ѵ�.
                 */
                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              "SELECT * " )
                          != IDE_SUCCESS );
            }
            else
            {
                /* 2.2. �⺻ Set ������ ��� copyFrom���� �۾��Ѵ�.
                 *
                 *  Push Projection�� �������� �ʰ�,
                 *  Set�� �������� �ʱ� ������,
                 *  ������ �ʿ䰡 ����.
                 */
            }
        }
        else
        {
            /* 2.3. ������ Query Text�� �״�� �����Ѵ�.
             *
             *  Set ������ �ƴ� ���, �Ʒ��� �� �����̴�.
             *  |  SELECT ...  |  ( SELECT ...  )  |
             *     ^              ^
             *      \______________\____ aInfo->mPosition.offset
             *
             */
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aInfo->mPosition.stmtText
                                                + aInfo->mPosition.offset,
                                                aInfo->mQuerySet->SFWGH->from->fromPosition.offset
                                                - aInfo->mPosition.offset )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( aInfo->mGraph == NULL, ERR_NULL_GRAPH );

            /* 2.4.   Bind ������ �����Ѵ�. */
            IDE_TEST( qmg::copyAndCollectParamOffset( aInfo->mStatement,
                                                      aInfo->mPosition.offset,
                                                      aInfo->mQuerySet->SFWGH->from->fromPosition.offset,
                                                      aInfo->mGraph->shardParamOffset,
                                                      aInfo->mGraph->shardParamCount,
                                                      aInfo->mGraph->shardParamInfo,
                                                      aInfo->mParamOffsetInfo )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copySelect",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copySelect",
                                  "query set is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copySelect",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copySelect",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::copyFrom( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� From�� Text�� �����Ѵ�.
 *               Set �����ڸ� ����ؼ� �����ϸ�, Push Selection ���ο� ����, Set ������
 *               Query Set�� View �� �ѹ� ���δ� Transform���� �����ϱ⵵ �Ѵ�.
 *
 *  SET    /        ( SELECT * FROM T1 ) UNION ALL ( SELECT * FROM T1 )
 *  WRAP   / FROM ( ( SELECT * FROM T1 ) UNION ALL ( SELECT * FROM T1 ) )  <-- 3.2
 *  COPY   /        ( SELECT * FROM T1 ) UNION ALL ( SELECT * FROM T1 )    <-- 4.1
 *
 *  COMMON / SELECT * FROM T1
 *  WRAP   /        X
 *  COPY   /          FROM T1  <-- 4.2
 *
 * Implementation : 1.   Set ������ ������ ���� ���� Offset�� �����Ѵ�.
 *                  2.1. Set �����ڰ� �ִٸ�, ������ Right ���� �����Ѵ�.
 *                  2.2. �ƴ϶��, ���� From ���� ������ From ���� ã�´�.
 *                  3.1. Set ���δ� Transform�� �����Ѵ�.
 *                  3.2. FROM (
 *                  3.3. ���� Offset���� ������ Offset���� �����Ѵ�.
 *                  3.4. )
 *                  4.1. �ƴϸ� Query Set ���ο� ���� �����Ѵ�.
 *                  4.2. FROM
 *                  4.3. ���� Offset���� ������ Offset���� �����Ѵ�.
 *                  5.   Bind ������ �����Ѵ�.
 *
 ****************************************************************************************/

    qmsQuerySet * sLeft   = NULL;
    qmsQuerySet * sRight  = NULL;
    qmsFrom     * sFrom   = NULL;
    SInt          sStart  = 0;
    SInt          sEnd    = 0;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mQuerySet == NULL, ERR_NULL_QUERYSET );

    /* 1.   Set ������ ������ ���� ���� Offset�� �����Ѵ�. */
    if ( aInfo->mQuerySet->setOp != QMS_NONE )
    {
        sLeft = aInfo->mQuerySet->left;

        while ( sLeft->setOp != QMS_NONE )
        {
            sLeft = sLeft->left;
        }

        sRight = aInfo->mQuerySet->right;

        while ( sRight->setOp != QMS_NONE )
        {
            sRight = sRight->right;
        }
    }
    else
    {
        sFrom = aInfo->mQuerySet->SFWGH->from;
    }

    /* 2.1. Set �����ڰ� �ִٸ�, ������ Right ���� �����Ѵ�. */
    if ( sRight != NULL )
    {
        IDE_TEST_RAISE( sLeft->startPos.stmtText != aInfo->mPosition.stmtText, ERR_DIFF_LEFT );
        IDE_TEST_RAISE( sRight->startPos.stmtText != aInfo->mPosition.stmtText, ERR_DIFF_RIGHT );

        sStart = sLeft->startPos.offset;
        sEnd   = sRight->endPos.offset + sRight->endPos.size;
    }
    else
    {
        if ( sFrom->onCondition != NULL )
        {
            IDE_TEST_RAISE( sFrom->onCondition->position.stmtText != aInfo->mPosition.stmtText, ERR_DIFF_TABLE );
        }
        else
        {
            IDE_TEST_RAISE( sFrom->tableRef->position.stmtText != aInfo->mPosition.stmtText, ERR_DIFF_TABLE );
        }

        /* 2.2. �ƴ϶��, ���� From ���� ������ From ���� ã�´�.*/
        IDE_TEST( qmg::getFromOffset( sFrom,
                                      &( sStart ),
                                      &( sEnd ) )
                  != IDE_SUCCESS );
    }

    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /* 3.1. Set ���δ� Transform�� �����Ѵ�. */
    if ( aInfo->mNeedWrapSet == ID_TRUE )
    {
        /* 3.2. FROM ( */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      "FROM ( " )
                  != IDE_SUCCESS );


        /* 3.3. ���� Offset���� ������ Offset���� �����Ѵ�. */
        IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                            aInfo->mPosition.stmtText
                                            + sStart,
                                            sEnd
                                            - sStart )
                  != IDE_SUCCESS );

        /* 3.4. ) */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " )" )
                  != IDE_SUCCESS );

        if ( ( sEnd < aInfo->mPosition.offset + aInfo->mPosition.size ) &&
             ( aInfo->mPosition.stmtText[ aInfo->mPosition.offset
                                          + aInfo->mPosition.size
                                          - 1 ] == ')' ) )
        {
            aInfo->mPosition.size--;
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( aInfo->mQuerySet->setOp != QMS_NONE )
        {
            /* 4.1. �ƴϸ� Query Set ���ο� ���� �����Ѵ�. */
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aInfo->mPosition.stmtText
                                                + sStart,
                                                sEnd
                                                - sStart )
                  != IDE_SUCCESS );
        }
        else
        {
            /* 4.2. FROM */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "FROM " )
                      != IDE_SUCCESS );

            /* 4.3. ���� Offset���� ������ Offset���� �����Ѵ�. */
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aInfo->mPosition.stmtText
                                                + sStart,
                                                sEnd
                                                - sStart )
                      != IDE_SUCCESS );
        }

    }

    IDE_TEST_RAISE( aInfo->mGraph == NULL, ERR_NULL_GRAPH );

    /* 5.   Bind ������ �����Ѵ�. */
    IDE_TEST( qmg::copyAndCollectParamOffset( aInfo->mStatement,
                                              sStart,
                                              sEnd,
                                              aInfo->mGraph->shardParamOffset,
                                              aInfo->mGraph->shardParamCount,
                                              aInfo->mGraph->shardParamInfo,
                                              aInfo->mParamOffsetInfo )
              != IDE_SUCCESS );

    aInfo->mPosition.size  -= sEnd - aInfo->mPosition.offset;
    aInfo->mPosition.offset = sEnd;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "query set is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_DIFF_LEFT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "left position is diff" ) );
    }
    IDE_EXCEPTION( ERR_DIFF_RIGHT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "right position is diff" ) );
    }
    IDE_EXCEPTION( ERR_DIFF_TABLE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "table position is diff" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyFrom",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::copyWhere( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Where�� Text�� �����Ѵ�.
 *               Push Selection ���ο� ���� Where Text ������ ����� �� �ִ�.
 *
 *  Where          /   / WHEHE C2 = 1 AND C3 > 2
 *                             *****************
 *  Push Selection / O / WHERE ( PUSH SELECTION ) AND ( C2 = 1 AND C3 > 2 )
 *                                                *************************
 *  Push Selection / X / WHEHE C2 = 1 AND C3 > 2
 *                       ***********************
 *
 * Implementation : 1.1. AND (
 *                  1.2. WHERE
 *                  2.   ���� Offset�� ����Ѵ�.
 *                  3.   ������ Offset�� ����Ѵ�.
 *                  4.   ���ۺ��� ���������� �����Ѵ�.
 *                  5.   )
 *                  6.   Bind ������ �����Ѵ�.
 *
 ****************************************************************************************/

    qtcNode * sNode  = NULL;
    qtcNode * sLast  = NULL;
    SInt      sStart = 0;
    SInt      sEnd   = 0;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );

    if ( aInfo->mWhere != NULL )
    {
        IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

        if ( aInfo->mPushPred != NULL )
        {
            /* 1.1. AND ( */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          " AND ( " )
                          != IDE_SUCCESS );
        }
        else
        {
            /* 1.2. WHERE */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          " WHERE " )
                      != IDE_SUCCESS );
        }

        /* 2.   ���� Offset�� ����Ѵ�. */
        IDE_TEST( qmg::getNodeOffset( aInfo->mWhere,
                                      ID_TRUE,
                                      &( sStart ),
                                      NULL )
                  != IDE_SUCCESS );

        /* 3.   ������ Offset�� ����Ѵ�. */
        for ( sNode  = aInfo->mWhere;
              sNode != NULL;
              sNode  = (qtcNode *)sNode->node.next )
        {
            sLast = sNode;
        }

        IDE_TEST( qmg::getNodeOffset( sLast,
                                      ID_TRUE,
                                      NULL,
                                      &( sEnd ) )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sLast->position.stmtText != aInfo->mPosition.stmtText, ERR_DIFF_QUERY );

        /* 4.   ���ۺ��� ���������� �����Ѵ�. */
        IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                            sLast->position.stmtText
                                            + sStart,
                                            sEnd
                                            - sStart )
                  != IDE_SUCCESS );

        if ( aInfo->mPushPred != NULL )
        {
            /* 5.   ) */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              " )" )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST_RAISE( aInfo->mGraph == NULL, ERR_NULL_GRAPH );

        /* 6.   Bind ������ �����Ѵ�. */
        IDE_TEST( qmg::copyAndCollectParamOffset( aInfo->mStatement,
                                                  sStart,
                                                  sEnd,
                                                  aInfo->mGraph->shardParamOffset,
                                                  aInfo->mGraph->shardParamCount,
                                                  aInfo->mGraph->shardParamInfo,
                                                  aInfo->mParamOffsetInfo )
                  != IDE_SUCCESS );

        aInfo->mPosition.size  -= sEnd - aInfo->mPosition.offset;
        aInfo->mPosition.offset = sEnd;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyWhere",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyWhere",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_DIFF_QUERY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyWhere",
                                  "query position is diff" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::copyWhere",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::generateTargetText( qmgTransformInfo * aInfo,
                                           qmsTarget        * aTarget )
{
/****************************************************************************************
 *
 * Description : Asterisk ( * ) �� ����� Query������ qmvQuerySet::expandAllTarget �Լ���
 *               Target�� Ȯ���Ͽ� Target Node�� �����ϱ� ������, Target Position�� ����.
 *
 *               �̶�, tableInfo->column ������ ������� �ۼ��� userName, tableName,
 *               aliasTableName, columnName, aliasColumnName�� ������
 *               Ȯ��� Target�� �̸��� �����Ѵ�.
 *
 * Implementation : 1.1. Set ������ �ִٸ�, column �����Ѵ�.
 *                  1.2. AliasColumnName�� �ִٸ� Text�� �����Ѵ�.
 *                  1.3. Column�� �ִٸ� Text�� �����Ѵ�.
 *                  2.1. UserName�� �ִٸ� Text�� �����Ѵ�.
 *                  2.2. AliasTableName�� �ִٸ� Text�� �����Ѵ�.
 *                  2.3. TableName�� �ִٸ� Text�� �����Ѵ�.
 *                  2.4. Column�� �ִٸ� Text�� �����Ѵ�.
 *                  2.5. AliasColumnName�� �ִٸ� Text�� �����Ѵ�.
 *
 ****************************************************************************************/

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aTarget == NULL, ERR_NULL_NODE );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /* 1.1. Set ������ �ִٸ�, column�� �����Ѵ�. */
    if ( aInfo->mQuerySet->setOp != QMS_NONE )
    {
        /* 1.2. AliasColumnName�� �ִٸ� Text�� �����Ѵ�. */
        if ( QC_IS_NULL_NAME( aTarget->aliasColumnName ) != ID_TRUE )
        {
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aTarget->aliasColumnName.name,
                                                aTarget->aliasColumnName.size )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST_RAISE( QC_IS_NULL_NAME( aTarget->columnName ) == ID_TRUE, ERR_NULL_COLUMNNAME );

            /* 1.3. Column�� �ִٸ� Text�� �����Ѵ�. */
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aTarget->columnName.name,
                                                aTarget->columnName.size )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* 2.1. UserName�� �ִٸ� Text�� �����Ѵ�. */
        if ( QC_IS_NULL_NAME( aTarget->userName ) != ID_TRUE )
        {
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aTarget->userName.name,
                                                aTarget->userName.size )
                      != IDE_SUCCESS );

            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "." )
                      != IDE_SUCCESS );

        }
        else
        {
            /* Nothing to do */
        }

        /* 2.2. AliasTableName�� �ִٸ� Text�� �����Ѵ�. */
        if ( QC_IS_NULL_NAME( aTarget->aliasTableName ) != ID_TRUE )
        {
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aTarget->aliasTableName.name,
                                                aTarget->aliasTableName.size )
                      != IDE_SUCCESS );

            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "." )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 2.3. TableName�� �ִٸ� Text�� �����Ѵ�. */
            if ( QC_IS_NULL_NAME( aTarget->tableName ) != ID_TRUE )
            {
                IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                    aTarget->tableName.name,
                                                    aTarget->tableName.size )
                          != IDE_SUCCESS );

                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              "." )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }

        IDE_TEST_RAISE( QC_IS_NULL_NAME( aTarget->columnName ) == ID_TRUE, ERR_NULL_COLUMNNAME );

        /* 2.4. Column�� �ִٸ� Text�� �����Ѵ�. */
        IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                            aTarget->columnName.name,
                                            aTarget->columnName.size )
                  != IDE_SUCCESS );

        /* 2.5. AliasColumnName�� �ִٸ� Text�� �����Ѵ�. */
        if ( QC_IS_NULL_NAME( aTarget->aliasColumnName ) != ID_TRUE )
        {
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          " AS " )
                      != IDE_SUCCESS );

            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aTarget->aliasColumnName.name,
                                                aTarget->aliasColumnName.size )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generateTargetText",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generateTargetText",
                                  "node is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generateTargetText",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_COLUMNNAME )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generateTargetText",
                                  "column name is null" ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::generatePredText( qmgTransformInfo * aInfo,
                                         qtcNode          * aNode )
{
/****************************************************************************************
 *
 * Description : qmoUtil::printPredInPlan �� �����Ͽ� �ۼ��� Query Text ������ Predicate
 *               Node Text ���� �Լ��̴�. ���� ��ȸ�� ��ȸ�ϸ鼭 ���ٷ� �����س���.
 *
 * Implementation : 1. �� �����ڶ�� ��� ȣ���Ѵ�.
 *                  2. ���� ��ȸ�� �� �����ڸ� �߰��� �ۼ��Ѵ�.
 *                  3. �� �����ڰ� �ƴ϶��, Node Text�� �����Ѵ�.
 *
 ****************************************************************************************/

    qtcNode * sNode = NULL;

    IDE_TEST_RAISE( aNode == NULL, ERR_NULL_NODE );
    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /* 1. �� �����ڶ�� ��� ȣ���Ѵ�. */
    if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        for ( sNode  = (qtcNode *)( aNode->node.arguments );
              sNode != NULL;
              sNode  = (qtcNode *)( sNode->node.next ) )
        {
            IDE_TEST( generatePredText( aInfo,
                                        sNode )
                     != IDE_SUCCESS );

            /* 2. ���� ��ȸ�� �� �����ڸ� �߰��� �ۼ��Ѵ�. */
            if ( sNode->node.next != NULL )
            {
                if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_AND )
                {
                    IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                        " AND ",
                                                        5 )
                              != IDE_SUCCESS );
                }
                else if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                          == MTC_NODE_OPERATOR_OR )
                {
                    IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                        " OR ",
                                                        4 )
                              != IDE_SUCCESS );
                }
                else if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                          == MTC_NODE_OPERATOR_NOT )
                {
                    IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                        " NOT ",
                                                        6 )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_RAISE( ERR_UNEXPECT );
                }
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        QC_SHARED_TMPLATE( aInfo->mStatement )->flag &= ~QC_TMP_SHARD_OUT_REF_COL_TO_BIND_MASK;
        QC_SHARED_TMPLATE( aInfo->mStatement )->flag |= QC_TMP_SHARD_OUT_REF_COL_TO_BIND_TRUE;
        
        /* 3. �� �����ڰ� �ƴ϶��, Node Text�� �����Ѵ�. */
        IDE_TEST( qmoUtil::printExpressionInPlan( QC_SHARED_TMPLATE( aInfo->mStatement ),
                                                  aInfo->mString,
                                                  aNode,
                                                  QMO_PRINT_UPPER_NODE_NORMAL )
                  != IDE_SUCCESS );

        QC_SHARED_TMPLATE( aInfo->mStatement )->flag &= ~QC_TMP_SHARD_OUT_REF_COL_TO_BIND_MASK;
        QC_SHARED_TMPLATE( aInfo->mStatement )->flag |= QC_TMP_SHARD_OUT_REF_COL_TO_BIND_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generatePredText",
                                  "info is null" ) );
    }
        IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generatePredText",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generatePredText",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generatePredText",
                                  "node is null" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::generatePredText",
                                  "unexpected error" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::pushProjectOptimize( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Target�� Push, Text�� �����Ѵ�.
 *               QMS_TARGET_IS_USELESS_TRUE �� Target�� �����Ѵ�. Target ���ŷ� Push��
 *               Target�� �������� Null Target�� �߰��Ѵ�.
 *
 *  QUERY   / SELECT C1 FROM ( SELECT * FROM T1 ORDER BY C1 )
 *  ANALYZE / SELECT C1 FROM ( SELECT * FROM SHARD( SELECT * FROM T1 ) ORDER BY C1 )
 *
 *  SHARD   /                                       SELECT * FROM T1
 *  EXPAND  /                                       SELECT C1, C2 FROM T1
 *  SELECT  /                                       SELECT
 *  USELESS /                                              F   T
 *  PROJECT /                                              C1               <-- 2.1, 2.3
 *  FROM    /                                                     FROM T1
 *
 *
 * Implementation : 1.   Push Projection ����� �ִ� ��쿡 ����ȭ�Ѵ�.
 *                  2.1. ��� Target�� �����ϸ�, ����ȭ�Ѵ�.
 *                  2.2. QMS_TARGET_IS_USELESS_TRUE�� Target�� �����Ѵ�.
 *                  2.3. ���� Target�� ,�� �����Ѵ�.
 *                  3.1. Query Text���� �ִ� Target�� �����Ѵ�.
 *                  3.2. ���ۺ��� ���������� �����Ѵ�.
 *                  3.3. aliasColumnName�� Position�� ���� ���� Text�� �����Ѵ�.
 *                  3.4. ������ Target�� ���� Text�� �����Ѵ�.
 *                  4.   Push�� Target�� Bind ������ �����Ѵ�.
 *                  5.   Push�� Target�� ���ٸ�, NULL Target �� �߰��Ѵ�.
 *                  6.   Tuple Column�� �����Ѵ�.
 *
 ****************************************************************************************/

    qmsTarget * sTarget      = NULL;
    SInt        sStart       = 0;
    SInt        sEnd         = 0;
    idBool      sIsTransform = ID_FALSE;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mQuerySet == NULL, ERR_NULL_QUERYSET );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /* 1.   Push Projection ����� �ִ� ��쿡 ����ȭ�Ѵ�. */
    if ( aInfo->mTarget != NULL )
    {
        /* 2.1. ��� Target�� �����ϸ�, ����ȭ�Ѵ�. */
        for ( sTarget  = aInfo->mTarget;
              sTarget != NULL;
              sTarget  = sTarget->next )
        {
            /* 2.2.   QMS_TARGET_IS_USELESS_TRUE �� Target�� �����Ѵ�. */
            if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) == QMS_TARGET_IS_USELESS_TRUE )
            {
                continue;
            }
            else
            {
                /* Nothing to do */
            }

            /* 2.3.   ���� Target�� ,�� �����Ѵ�. */
            if ( sIsTransform == ID_TRUE )
            {
                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              ", " )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsTransform = ID_TRUE;
            }

            /* 3.1. Query Text���� �ִ� Target�� �����Ѵ�. */
            if ( ( QC_IS_NULL_NAME( sTarget->targetColumn->position ) != ID_TRUE ) &&
                 ( sTarget->targetColumn->position.stmtText == aInfo->mPosition.stmtText ) )
            {
                IDE_TEST( qmg::getNodeOffset( sTarget->targetColumn,
                                              ID_FALSE,
                                              &( sStart ),
                                              &( sEnd ) )
                          != IDE_SUCCESS );

                /* 3.2. ���ۺ��� ���������� �����Ѵ�. */
                IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                    sTarget->targetColumn->position.stmtText
                                                    + sStart,
                                                    sEnd
                                                    - sStart )
                          != IDE_SUCCESS );

                /* 3.3. aliasColumnName�� Position�� ���� ���� Text�� �����Ѵ�. */
                if ( QC_IS_NULL_NAME( sTarget->aliasColumnName ) != ID_TRUE )
                {
                    IDE_TEST( iduVarStringAppend( aInfo->mString,
                                                  " AS " )
                              != IDE_SUCCESS );

                    IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                        sTarget->aliasColumnName.name,
                                                        sTarget->aliasColumnName.size )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* 3.4. ������ Target�� ���� Text�� �����Ѵ�.
                 *  - qmvQuerySet::expandAllTarget
                 *  - qmvQuerySet::expandTarget
                 *  - qmvShardTransform::appendSortNode
                 *  - qmvShardTransform::adjustTargetAndSort
                 */
                IDE_TEST( generateTargetText( aInfo,
                                              sTarget )
                          != IDE_SUCCESS );
            }

            /* 4.   Push�� Target�� Bind ������ �����Ѵ�. */
            IDE_TEST( qmg::findAndCollectParamOffset( aInfo->mStatement,
                                                      sTarget->targetColumn,
                                                      aInfo->mParamOffsetInfo )
                      != IDE_SUCCESS );
        }

        if ( sIsTransform == ID_TRUE )
        {
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          " " )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 5.   Push�� Target�� ���ٸ�, NULL Target �� �߰��Ѵ�. */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "NULL " )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushProjectOptimize",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushProjectOptimize",
                                  "query set is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushProjectOptimize",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::pushSelectOptimize( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Where���� Push, Text�� �����Ѵ�.
 *               Predicate ������ ( )�� ���ΰ�, Push Selection ��ü�� ( )�� ���Ѵ�.
 *
 *  TARGET PREDICATE            |  Push Selection
 *                              |
 *    OR                        |   WHERE (
 *     AND                      |          (
 *      A.C2 = 1 ---  A.C3 = 2  |            T1.C2 = 1
 *                              |                      )
 *                              |                        AND
 *                              |                            (
 *                              |                              T2.C2 = 1
 *                              |                                        )
 *                              |                                          )
 *                              |
 *                              |   WHERE ( ( T1.C2 = 1 ) AND ( T2.C2 = 1 ) )
 *
 * Implementation : 1.  Push�� Predicate�� ���� ���� �����Ѵ�.
 *                  2.  WHERE (
 *                  3.  ���� Predicade�� AND�� �����Ѵ�.
 *                  4.  Shard View�� �����ϰ� Predicate Node �̸��� �����Ѵ�.
 *                  5.  (
 *                  6.  Predicate Node�� Textȭ �Ѵ�.
 *                  7.  )
 *                  8.  Push�� Predicate�� Bind ������ �����Ѵ�.
 *                  9.  Shard Key Columnn�̶�� Shard Value�� �����.
 *                  10. )
 *
 ****************************************************************************************/

    qmgPushPred * sPushPred    = NULL;
    idBool        sIsTransform = ID_FALSE;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );

    /* 1.  Push�� Predicate�� ���� ���� �����Ѵ�. */
    if ( aInfo->mPushPred != NULL )
    {
        IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

        /* 2.  WHERE ( */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " WHERE ( " )
                  != IDE_SUCCESS );

        for ( sPushPred  = aInfo->mPushPred;
              sPushPred != NULL;
              sPushPred  = sPushPred->mNext )
        {
            if ( sIsTransform == ID_TRUE )
            {
                /* 3.  ���� Predicade�� AND�� �����Ѵ�. */
                IDE_TEST( iduVarStringAppend( aInfo->mString,
                                              " AND " )
                          != IDE_SUCCESS );
            }
            else
            {
                sIsTransform = ID_TRUE;
            }

            /* 5.  ( */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "( " )
                      != IDE_SUCCESS );

            /* 6.  Predicate Node�� Textȭ �Ѵ�. */
            IDE_TEST( generatePredText( aInfo,
                                        sPushPred->mNode )
                      != IDE_SUCCESS );

            /* 7.  ) */
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          " )" )
                      != IDE_SUCCESS );

            /* 8.  Push�� Predicate�� Bind ������ �����Ѵ�. */
            IDE_TEST( qmg::findAndCollectParamOffset( aInfo->mStatement,
                                                      sPushPred->mNode,
                                                      aInfo->mParamOffsetInfo )
                      != IDE_SUCCESS );
        }

        IDE_TEST_RAISE( sIsTransform == ID_FALSE, ERR_UNEXPECTED );

        /* 10. ) */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " )" )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSelectOptimize",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSelectOptimize",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSelectOptimize",
                                  "target predicate is none" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::pushOrderByOptimize( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Order By���� Push, Text�� �����Ѵ�.
 *               Limit Sort�� ��츸 �����Ѵ�. �׸��� Order By���� ���� Push, �����ؼ�
 *               pushLimitOptimize�� ȣ���ؼ� �ؾ� �Ѵ�. ���� Order By���� Push�� ����
 *               Limit���� Push�Ѵ�.
 *
 *  ORDER BY C2, C3 ASC  |  ORDER BY C2, C3 NULL LAST
 *  *******************  |  *************************
 *
 * Implementation : 1. Push�� Order By�� ���� ���� �����Ѵ�.
 *                  2. ���� Offset�� ����Ѵ�.
 *                  3. ������ Offset�� ����Ѵ�.
 *                  4. Push�� Order By�� Bind ������ �����Ѵ�.
 *                  5. ���ۺ��� ���������� �����Ѵ�.
 *
 ****************************************************************************************/

    qmsSortColumns * sOrderBy = NULL;
    qmsSortColumns * sLast    = NULL;
    SInt             sStart   = 0;
    SInt             sEnd     = 0;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /*  1. Push�� Order By�� ���� ���� �����Ѵ�. */
    if ( aInfo->mOrderBy != NULL )
    {
        /* 2. ���� Offset�� ����Ѵ�. */
        IDE_TEST( qmg::getNodeOffset( aInfo->mOrderBy->sortColumn,
                                      ID_TRUE,
                                      &( sStart ),
                                      NULL )
                  != IDE_SUCCESS );

        /* 3. ������ Offset�� ����Ѵ�. */
        for ( sOrderBy  = aInfo->mOrderBy;
              sOrderBy != NULL;
              sOrderBy  = sOrderBy->next )
        {
            /* 4.Push�� Order By�� Bind ������ �����Ѵ�. */
            IDE_TEST( qmg::findAndCollectParamOffset( aInfo->mStatement,
                                                      sOrderBy->sortColumn,
                                                      aInfo->mParamOffsetInfo )
                      != IDE_SUCCESS );

            sLast = sOrderBy;
        }

        if ( QC_IS_NULL_NAME( sLast->mNullsOptPos ) != ID_TRUE )
        {
            sEnd = sLast->mNullsOptPos.offset + sLast->mNullsOptPos.size;
        }
        else if ( QC_IS_NULL_NAME( sLast->mSortModePos ) != ID_TRUE )
        {
            sEnd = sLast->mSortModePos.offset + sLast->mSortModePos.size;
        }
        else
        {
            IDE_TEST( qmg::getNodeOffset( sLast->sortColumn,
                                          ID_TRUE,
                                          NULL,
                                          &( sEnd ) )
                      != IDE_SUCCESS );
        }

        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " ORDER BY " )
                  != IDE_SUCCESS );

        /* 5. ���ۺ��� ���������� �����Ѵ�. */
        IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                            sLast->sortColumn->position.stmtText
                                            + sStart,
                                            sEnd
                                            - sStart )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushOrderByOptimize",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushOrderByOptimize",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::pushLimitOptimize( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Plan String Buffer�� Limit���� Push, Query Set Text�� �����Ѵ�.
 *               Push Limit ����ȭ�� Shard Select Plan���� Node ���� Limit�� �����Ѵ�.
 *
 *  BEFORE                       |  AFTER
 *  - LIMIT 2 OFFSET 2 ( COORD ) |  -- LIMIT CAST( 2 AS BIGINT ) + CAST( 2 AS BIGINT ) - 1 ( NODE1 )
 *                               |  -- LIMIT CAST( 2 AS BIGINT ) + CAST( 2 AS BIGINT ) - 1 ( NODE2 )
 *                               |  - LIMIT 2 OFFSET 2 ( COORD )
 *
 * Implementation : 1.   Push�� Limit�� ���� ���� �����Ѵ�.
 *                  2.   LIMIT FOR SHARD CAST(
 *                  3.1. START
 *                  3.2. Push�� Limit�� Bind ������ �����Ѵ�.
 *                  4.   AS BIGINT ) + CAST(
 *                  5.1  COUNT
 *                  5.2. Push�� Limit�� Bind ������ �����Ѵ�.
 *                  6.   AS BIGINT ) - 1
 *
 ****************************************************************************************/

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mString == NULL, ERR_NULL_PLAN_STRING );

    /*  1.   Push�� Limit�� ���� ���� �����Ѵ�. */
    if ( aInfo->mLimit != NULL )
    {
        /* 2.   LIMIT ( */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " LIMIT FOR SHARD CAST( " )
                  != IDE_SUCCESS );

        /* 3.1. START */
        if ( QC_IS_NULL_NAME( aInfo->mLimit->start.mPosition ) != ID_TRUE )
        {
            IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                                aInfo->mLimit->start.mPosition.stmtText
                                                + aInfo->mLimit->start.mPosition.offset,
                                                aInfo->mLimit->start.mPosition.size )
                      != IDE_SUCCESS );

            if ( qmsLimitI::hasHostBind( qmsLimitI::getStart( aInfo->mLimit ) ) == ID_TRUE )
            {
                /* 3.2. Push�� Limit�� Bind ������ �����Ѵ�. */
                IDE_TEST( qmg::findAndCollectParamOffset( aInfo->mStatement,
                                                          qmsLimitI::getHostNode(
                                                              qmsLimitI::getStart( aInfo->mLimit ) ),
                                                          aInfo->mParamOffsetInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            IDE_TEST( iduVarStringAppend( aInfo->mString,
                                          "1" )
                      != IDE_SUCCESS );
        }

        /* 4.   AS BIGINT ) + ( */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " AS BIGINT ) + CAST( " )
                  != IDE_SUCCESS );

        /* 5.1. COUNT */
        IDE_TEST( iduVarStringAppendLength( aInfo->mString,
                                            aInfo->mLimit->count.mPosition.stmtText
                                            + aInfo->mLimit->count.mPosition.offset,
                                            aInfo->mLimit->count.mPosition.size )
                  != IDE_SUCCESS );

        if ( qmsLimitI::hasHostBind( qmsLimitI::getCount( aInfo->mLimit ) ) == ID_TRUE )
        {
            /* 5.2. Push�� Limit�� Bind ������ �����Ѵ�. */
            IDE_TEST( qmg::findAndCollectParamOffset( aInfo->mStatement,
                                                      qmsLimitI::getHostNode(
                                                          qmsLimitI::getCount( aInfo->mLimit ) ),
                                                      aInfo->mParamOffsetInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* 6.   AS BIGINT ) - 1 SHARD */
        IDE_TEST( iduVarStringAppend( aInfo->mString,
                                      " AS BIGINT ) - 1" )
                  != IDE_SUCCESS );

    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushLimitOptimize",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PLAN_STRING )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushLimitOptimize",
                                  "plan string is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::pushSeriesOptimize( qcStatement    * aStatement,
                                           const qmgGraph * aParent,
                                           qmgShardSELT   * aMyGraph )
{
/****************************************************************************************
 *
 * Description : Shard Query�� Push Projection, Push Selection, Push Limit ����ȭ��
 *               �����Ѵ�.
 *
 *               Ư��, Push Limit�� Limit�� �ϳ��� Push�ϰų�, Limit Sort�� ���
 *               Order By�� ������ ���� Push�ؾ� �Ѵ�.
 *
 * Implementation : 1.  ��ȯ�� �ʿ��� �� �˻��Ѵ�.
 *                  2.  ��ȯ�� �ʿ��� ���� �����Ѵ�.
 *                  3.  Select�� �����Ѵ�.
 *                  4.  Push Projection�� �����Ѵ�.
 *                  5.  From ���� �����Ѵ�.
 *                  6.  Push Selection�� �����Ѵ�.
 *                  7.  Where ���� �����Ѵ�.
 *                  8.  Group By Having ���� �����Ѵ�
 *                  9.  Bind ������ �����Ѵ�.
 *                  10. Push Limit�� �����Ѵ�.
 *                  11. ��ȯ�� Query�� Buffer�� ����Ѵ�.
 *                  12. ��ȯ�� Query�� Tuple Column�� �����Ѵ�.
 *                  13. ��ȯ�� Query�� Shard Key Value�� �����Ѵ�.
 *                  14. ��ȯ�� Query�� Shard Parameter�� ����
 *                  15. ��ȯ�� Query�� Shard Query�� �����Ѵ�.
 *
 ****************************************************************************************/

    qmgTransformInfo sTransformInfo;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParent == NULL, ERR_NULL_PARENT_GRAPH );
    IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );

    /* 1.  ��ȯ�� �ʿ��� �� �˻��Ѵ�. */
    IDE_TEST( checkAndGetPushSeries( aStatement,
                                     aParent,
                                     aMyGraph,
                                     &( sTransformInfo ) )
              != IDE_SUCCESS );

    /* 2.  ��ȯ�� �ʿ��� ���� �����Ѵ�. */
    if ( sTransformInfo.mIsTransform == ID_TRUE )
    {
        IDE_TEST( iduVarStringTruncate( sTransformInfo.mString,
                                        ID_TRUE )
                  != IDE_SUCCESS );

        /* 3. Select�� �����Ѵ�. */
        IDE_TEST( copySelect( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 4.  Push Projection�� �����Ѵ�. */
        IDE_TEST( pushProjectOptimize( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 5.  From ���� �����Ѵ�. */
        IDE_TEST( copyFrom( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 6.  Push Selection�� �����Ѵ�. */
        IDE_TEST( pushSelectOptimize( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 7.  Where ���� �����Ѵ�. */
        IDE_TEST( copyWhere( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 8.  Group By Having ���� �����Ѵ�. */
        IDE_TEST( iduVarStringAppendLength( sTransformInfo.mString,
                                            sTransformInfo.mPosition.stmtText
                                            + sTransformInfo.mPosition.offset,
                                            sTransformInfo.mPosition.size )
                  != IDE_SUCCESS );

        /* 9.  Bind ������ �����Ѵ�. */
        IDE_TEST( qmg::copyAndCollectParamOffset( aStatement,
                                                  sTransformInfo.mPosition.offset,
                                                  sTransformInfo.mPosition.offset
                                                  + sTransformInfo.mPosition.size,
                                                  aMyGraph->shardParamOffset,
                                                  aMyGraph->shardParamCount,
                                                  aMyGraph->shardParamInfo,
                                                  sTransformInfo.mParamOffsetInfo )
                  != IDE_SUCCESS );

        /* 10. Push Limit�� �����Ѵ�. */
        IDE_TEST( pushOrderByOptimize( &( sTransformInfo ) ) != IDE_SUCCESS );

        IDE_TEST( pushLimitOptimize( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 11. ��ȯ�� Query�� Buffer�� ����Ѵ�. */
        aMyGraph->mShardQueryLength = iduVarStringGetLength( sTransformInfo.mString );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( aMyGraph->mShardQueryLength + 1,
                                                   (void**)&( aMyGraph->mShardQueryBuf ) )
                  != IDE_SUCCESS );

        IDE_TEST( iduVarStringConvToCString( sTransformInfo.mString,
                                             aMyGraph->mShardQueryLength + 1,
                                             aMyGraph->mShardQueryBuf )
                  != IDE_SUCCESS );

        IDE_TEST( iduVarStringTruncate( sTransformInfo.mString,
                                        ID_TRUE )
                  != IDE_SUCCESS );

        /* 12. ��ȯ�� Query�� Tuple Column�� t���Ѵ�. */
        IDE_TEST( setShardTupleColumn( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 13. ��ȯ�� Query�� Shard Key Value�� �����Ѵ�. */
        IDE_TEST( setShardKeyValue( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 14. ��ȯ�� Query�� Shard Parameter�� �����Ѵ�. */
        IDE_TEST( setShardParameter( &( sTransformInfo ) ) != IDE_SUCCESS );

        /* 15. ��ȯ�� Query�� Shard Query�� �����Ѵ�. */
        aMyGraph->shardQuery.stmtText = aMyGraph->mShardQueryBuf;
        aMyGraph->shardQuery.offset   = 0;
        aMyGraph->shardQuery.size     = aMyGraph->mShardQueryLength;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSeriesOptimize",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARENT_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSeriesOptimize",
                                  "parent graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::pushSeriesOptimize",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::setShardKeyValue( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Push Selection���� Shard Key�� ������ ����, Shard Key Value�� �����ϴ�
 *               �Լ��̴�. qmvShardTransform::isShardQuery �Լ� ������ �����Ͽ���.
 *
 *               Dummy Statement�� �����ϰ�, Push Selection�� ����� Query�� �ٽ� PVO,
 *               Analyze �Ѵ�. Analyze ����� ��ȯ�Ǵ� Shard Key Value�� ���ϰ�
 *               �������� ������.
 *
 *
 *  BEFORE / SELECT CAST( ? AS INTEGER )
 *            FROM ( SELECT CAST( ? AS INTEGER ), C1
 *                    FROM T1 WHERE C3 != ?
 *                     ORDER BY C2 )
 *             WHERE C1 != ?;
 *
 *  AFTER1 / SELECT CAST( ? AS INTEGER )
 *            FROM ( SELECT *
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1
 *                            FROM T1 WHERE C3 != ?
 *                             ORDER BY C2 ) )
 *             WHERE C1 != 1;
 *
 *  SHARD VALUE / COUNT 0 / NONE
 *
 *
 *  AFTER2 / SELECT CAST( ? AS INTEGER )
 *            FROM ( SELECT *
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1
 *                            FROM T1 WHERE C3 != ?
 *                             AND C1 != 1
 *                                 *******
 *                              ORDER BY C2 ) )
 *             WHERE C1 != 1;
 *
 *  SHARD VALUE / COUNT 1 / VALUE 1
 *
 *
 * Implementation : 1.  Push Selection�� ����� ��츸 �����Ѵ�.
 *                  2.  Dummy Statement�� �����Ѵ�.
 *                  3.  Transform�� Query�� �����Ѵ�.
 *                  4.  ���� Bind ������ �����Ϸ���, Shard ���п� Bind Offset�� �����Ѵ�.
 *                  5.  prasePartial�� �����Ѵ�.
 *                  6.  �м� ������� ���� Ȯ���Ѵ�.
 *                  7.  Dummy Bind ������ �����Ѵ�.
 *                  8.  Validate�� �����Ѵ�.
 *                  9.  Analyze�� �����Ѵ�.
 *                  10. Shard ���п� Bind Offset�� �����Ѵ�.
 *                  11. Shard Key Value�� �����Ѵ�.
 *                  12. Dummy Statement�� �����Ѵ�.
 *
 ****************************************************************************************/

    qcStatement    * sStatement        = NULL;
    qmsQuerySet    * sQuerySet         = NULL;
    sdiAnalyzeInfo * sAnalyzeInfo      = NULL;
    ULong            sSMN              = ID_ULONG(0);
    idBool           sIsShardParseTree = ID_FALSE;
    idBool           sIsShardQuerySet  = ID_FALSE;
    idBool           sIsTransformAble  = ID_FALSE;
    qcNamePosition * sStmtPos          = NULL;
    qcNamePosition   sStmtBuffer;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aInfo->mGraph == NULL, ERR_NULL_GRAPH );

    /* 1.  Push Selection�� ����� ��츸 �����Ѵ�. */
    IDE_TEST_CONT( aInfo->mPushPred == NULL, NORMAL_EXIT );
    IDE_TEST_CONT( aInfo->mGraph->shardAnalysis == NULL, NORMAL_EXIT );

    /* 2. */
    sStatement  = aInfo->mStatement;
    sQuerySet   = aInfo->mQuerySet;
    sStmtPos    = &( sStmtBuffer );

    if ( sdi::detectShardMetaChange( sStatement ) == ID_TRUE )
    {
        sSMN = sdi::getSMNForDataNode();
    }
    else
    {
        sSMN = QCG_GET_SESSION_SHARD_META_NUMBER( sStatement );
    }

    /* TASK-7219 Non-shard DML */
    sStatement->mShardPartialExecType = aInfo->mStatement->mShardPartialExecType;

    /* 3.  Transform�� Query�� �����Ѵ�. */
    sStmtPos->stmtText = aInfo->mGraph->mShardQueryBuf;
    sStmtPos->offset   = 0;
    sStmtPos->size     = aInfo->mGraph->mShardQueryLength;

    /* 4. */
    SDI_SET_QUERYSET_LIST_STATE( sStatement->mShardQuerySetList,
                                 SDI_QUERYSET_LIST_STATE_MAIN_PARTIAL );

    IDE_TEST( qmvShardTransform::doShardAnalyze( sStatement,
                                                 sStmtPos,
                                                 sSMN,
                                                 sQuerySet )
              != IDE_SUCCESS );

    IDE_TEST( sdi::isShardParseTree( sStatement->myPlan->parseTree,
                                     &( sIsShardParseTree ) )
              != IDE_SUCCESS );

    if ( sIsShardParseTree == ID_FALSE )
    {
        IDE_TEST( sdi::isShardQuerySet( sQuerySet,
                                        &( sIsShardQuerySet ),
                                        &( sIsTransformAble ) )
                  != IDE_SUCCESS );

        if ( ( sIsShardQuerySet == ID_FALSE )
             &&
             ( sIsTransformAble == ID_FALSE ) )
        {
            IDE_TEST_RAISE( aInfo->mUseShardKwd == ID_FALSE, ERR_UNEXPECTED );
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

    IDE_TEST( sdi::makeAndSetAnalyzeInfoFromQuerySet( sStatement,
                                                      sQuerySet,
                                                      &( sAnalyzeInfo ) )
              != IDE_SUCCESS );

    aInfo->mGraph->shardAnalysis = sAnalyzeInfo;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardKeyValue",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardKeyValue",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardKeyValue",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION( ERR_UNEXPECTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardKeyValue",
                                  "transformed query is non shard" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::setShardParameter( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Push Selection���� Shard Key Bind�� ������ ����, Shard Key Bind
 *               ������ �����ϴ� �Լ��̴�. qmvShardTransform::isShardQuery �Լ� ������
 *               �����Ͽ���.
 *
 *               Transform �� Bind ������ Transform ���� �߿� ������ ������ ������, Shard
 *               Bind�� ���� Query�� ������ Bind Offset���� Array�� �����Ѵ�.
 *
 *               ���� qmgShardSelect::setShardKeyValue �� ����Ǿ��ٸ�, Transform��
 *               Query ���� Bind Offset���� Shard Key Bind�� �����ϱ� ������,
 *               Shard Param Offset Array ������ �����ϴ�.
 *
 *               Shard Param Offset Array�� Transform�� Query ���� Shard Bind ������
 *               ��ġ �ش��ϴ� ������ Offset�� ��ϵǾ� �����Ƿ�, �ش� ������ �����ؾ� �Ѵ�.
 *
 *
 *  BEFORE / SELECT CAST( ? AS INTEGER )
 *            FROM ( SELECT CAST( ? AS INTEGER ), C1
 *                    FROM T1 WHERE C3 != ?
 *                     ORDER BY C2 )
 *             WHERE C1 != ?;
 *
 *  BIND       / COUNT 4 / OFFSET 0 1 2 3
 *                                       \______________________________
 *                                                                      |
 *  AFTER1 / SELECT CAST( ? AS INTEGER )                                |
 *            FROM ( SELECT *                                           |
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1            |
 *                            FROM T1 WHERE C3 != ?                     |
 *                             ORDER BY C2 ) )                          |
 *             WHERE C1 != ?;                                           |
 *                                                                      |
 *  SHARD BIND  / COUNT 2 / OFFSET 1 2                                  |
 *  SHARD VALUE / COUNT 0 / NONE                                        |
 *                                                                      |
 *                                                                      |
 *  AFTER2 / SELECT CAST( ? AS INTEGER )                                |
 *            FROM ( SELECT *                                           |
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1 -- BIND 0  |
 *                            FROM T1 WHERE C3 != ?          -- BIND 1  |
 *                             AND C1 != ?                   -- BIND 2  |
 *                              ORDER BY C2 ) )                         |
 *             WHERE C1 != ?;            _______________________________|
 *                                      /                               |
 *  SHARD BIND  / COUNT 2 / OFFSET 1 2 3                                |
 *  SHARD VALUE / COUNT 1 / BIND 2    /                                 |
 *                                \__/                                  |
 *                                DIFF                                  |
 *                                                                      |
 *  qmgShardSelect::setShardParameter()                                 |
 *                                                                      |
 *  SHARD VALUE / COUNT 1 / BIND 3                                      |
 *                                \_____________________________________|
 *
 *
 * Implementation : 1.  Shard Param Offset Array�� �����Ѵ�.
 *                  2.  Push Selection�� ����� ��츸 �����Ѵ�.
 *                  3.  Shard Key Value�� �����Ѵ�.
 *                  4.  Shard SubKey Value�� �����Ѵ�.
 *
 ****************************************************************************************/

    UShort             sShardParamCount = ID_USHORT_MAX;
    qcShardParamInfo * sShardParamInfo  = NULL;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aInfo->mStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aInfo->mParamOffsetInfo == NULL, ERR_NULL_PARAM_INFO );
    IDE_TEST_RAISE( aInfo->mGraph == NULL, ERR_NULL_GRAPH );

    /* 1.  Shard Param Offset Array�� �����Ѵ�. */
    IDE_TEST( qmg::makeShardParamOffsetArrayForGraph( aInfo->mStatement,
                                                      aInfo->mParamOffsetInfo,
                                                      &( sShardParamCount ),
                                                      &( sShardParamInfo ) )
              != IDE_SUCCESS );

    aInfo->mGraph->shardParamCount       = sShardParamCount;
    aInfo->mGraph->shardParamInfo = sShardParamInfo;

    /* 2.  Push Selection�� ����� ��츸 �����Ѵ�. */
    IDE_TEST_CONT( aInfo->mPushPred == NULL, NORMAL_EXIT );
    IDE_TEST_CONT( aInfo->mGraph->shardAnalysis == NULL, NORMAL_EXIT );

    IDE_TEST( qmg::adjustParamOffsetForAnalyzeInfo( aInfo->mGraph->shardAnalysis,
                                                    sShardParamCount,
                                                    &sShardParamInfo )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardParameter",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardParameter",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardParameter",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARAM_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardParameter",
                                  "param info is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::setShardTupleColumn( qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Push Projecion���� Column�� ���ŵǴ� ��Ȳ�� ����ؼ� Tuple Column��
 *               �����ϴ� �Լ��̴�.
 *
 *               Target�� ���ŵ� ������ Shard Query�� ���ؼ�, DATA NODE ���� �޾ƿ�
 *               Tuple �� �ʿ��� Target Column �� ���̾ ����Ѵ�.
 *
 *               COORD NODE ���� �� ���¿� �����ϰ�, �ʿ��� Target Column�� Offset��
 *               Tuple ������ �����Ѵ�.
 *
 *               �ݴ�� ���ʿ��� Target Column�� Tuple �ڷ� �����ϰ�, ���Ŀ�
 *               Column Type Null ó���� �����ؾ� �Ѵ�.
 *
 *
 *  QUERY     / SELECT C2 FROM      ( SELECT C1, C2, C3, C4 FROM T1 ) ORDER BY C4
 *  ANALYZE   / SELECT C2 FROM SHARD( SELECT C1, C2, C3, C4 FROM T1 ) ORDER BY C4
 *  TRANSFROM /                SHARD( SELECT     C2,     C4 FROM T1 )
 *
 *  DATA      /                SHARD( SELECT     C2,     C4 FROM T1 )
 *   RESULT   /                     |- C2 -|- C4 -|
 *                                      \______\___________________________________
 *                                                                                 |
 *  COORD     / SELECT C2 FROM      ( SELECT C1, C2, C3, C4 FROM T1 ) ORDER BY C4  |
 *   BEFORE   /                     |- C1 -|- C2 -|- C3 -|- C4 -|                  |
 *   AFTER    /                     |- C2 -|- C4 -|- C1 -|- C3 -|                  |
 *                                      \______\___________________________________|
 *
 *
 * Implementation : 1. Push Projection �� �����ϴ� ��츸, �����Ѵ�.
 *                  2. ������� Target�� Tuple�� �պκ��� ����ϵ��� Offset�� �����Ѵ�.
 *                  3. ������� �ʴ� Target�� Tuple�� �޺κ��� ����ϵ��� Offset�� �����Ѵ�.
 *
 ****************************************************************************************/

    qmsTarget * sTarget      = NULL;
    mtcTuple  * sTuple       = NULL;
    mtcColumn * sColumn      = NULL;
    UShort      sIdx         = 0;
    UShort      sTable       = 0;
    UShort      sColumnCount = 0;
    UInt        sOffset      = 0;

    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );

    /* 1. Push Projection �� �����ϴ� ��츸, �����Ѵ�. */
    IDE_TEST_CONT( aInfo->mTarget == NULL, NORMAL_EXIT );

    sTarget      = aInfo->mTarget;
    sTable       = aInfo->mGraph->graph.myFrom->tableRef->table;
    sTuple       = & QC_SHARED_TMPLATE( aInfo->mStatement )->tmplate.rows[ sTable ];    
    sColumnCount = sTuple->columnCount;
    sColumn      = sTuple->columns;
    sOffset      = sColumn->column.offset;

    /* 2. ������� Target�� Tuple�� �պκ��� ����ϵ��� Offset�� �����Ѵ�. */
    for ( sIdx = 0, sColumn = sTuple->columns, sTarget = aInfo->mTarget;
          sIdx < sColumnCount;
          sIdx++, sColumn++, sTarget = sTarget->next )
    {
        if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) == QMS_TARGET_IS_USELESS_TRUE )
        {
            sColumn->flag &= ~MTC_COLUMN_NULL_TYPE_MASK;
            sColumn->flag |= MTC_COLUMN_NULL_TYPE_TRUE;
        }
        else
        {
            sOffset = idlOS::align( sOffset,
                                    sColumn->module->align );

            sColumn->column.offset = sOffset;

            sOffset += sColumn->column.size;
        }
    }

    /* 3. ������� �ʴ� Target�� Tuple�� �޺κ��� ����ϵ��� Offset�� �����Ѵ�. */
    for ( sIdx = 0, sColumn = sTuple->columns;
          sIdx < sColumnCount;
          sIdx++, sColumn++ )
    {
        if ( ( sColumn->flag & MTC_COLUMN_NULL_TYPE_MASK ) == MTC_COLUMN_NULL_TYPE_TRUE )
        {
            sOffset = idlOS::align( sOffset,
                                    sColumn->module->align );

            sColumn->column.offset = sOffset;

            sOffset += sColumn->column.size;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::setShardTupleColumn",
                                  "info is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::makePushPredicate( qcStatement  * aStatement,
                                          qmgShardSELT * aMyGraph,
                                          qmsParseTree * aViewParseTree,
                                          qmsSFWGH     * aOuterQuery,
                                          SInt         * aRemain,
                                          qmoPredicate * aPredicate,
                                          qmgPushPred ** aHead,
                                          qmgPushPred ** aTail )
{
/****************************************************************************************
 *
 * Description : qmgSelection::doViewPushSelection �� �����Ͽ� Push Selection�� ������
 *               ��쿡 �ش� Predicate�� �����Ѵ�.
 *
 * Implementation : 1.   Predicate �˻��Ѵ�.
 *                  2.   Predicate�� Push Down�ϱ� Valid�� ����̾�� �Ѵ�.
 *                  3.1. Push Selection�� �˻��Ѵ�.
 *                  3.2. ������ Table Predicate�� �����Ѵ�.
 *                  3.3. �ش� Predicate�� �����ؼ� Mtc Stack ������ �˻��Ѵ�.
 *                  3.4. ������ Predicate�� �����Ѵ�.
 *                  4.   aPredicate->next ���� �˻��Ѵ�.
 *                  5.   aPredicate->more ���� �˻��Ѵ�.
 *                  6.   Stack ��뷮�� �����Ѵ�.
 *
 ****************************************************************************************/

    qmgPushPred  * sNewPred    = NULL;
    qtcNode      * sNode       = NULL;
    UShort         sTupleId    = ID_USHORT_MAX;
    idBool         sIsValid    = ID_FALSE;
    idBool         sIsPushPred = ID_FALSE;
    idBool         sIsOverflow = ID_FALSE;
    SInt           sRemain     = 0;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );
    IDE_TEST_RAISE( aViewParseTree == NULL, ERR_NULL_PARSETREE );
    IDE_TEST_RAISE( aPredicate == NULL, ERR_NULL_PREDICATE );

    sRemain = *aRemain;

    IDE_TEST_CONT( sRemain < 1, NORMAL_EXIT );

    /* 1.   Predicate �˻��Ѵ�. */
    IDE_TEST( qmoPred::isValidPushDownPredicate( aStatement,
                                                 &( aMyGraph->graph ),
                                                 aPredicate,
                                                 &( sIsValid ) )
              != IDE_SUCCESS );

    /* 2.   Predicate�� Push Down�ϱ� Valid�� ����̾�� �Ѵ�. */
    if ( sIsValid == ID_FALSE )
    {
        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* 3.1. Push Selection�� �˻��Ѵ�. */
    sTupleId = aMyGraph->graph.myFrom->tableRef->table;

    IDE_TEST( qmoPushPred::checkPushDownPredicate( aStatement,
                                                   aViewParseTree,
                                                   aViewParseTree->querySet,
                                                   aOuterQuery,
                                                   sTupleId,
                                                   aPredicate,
                                                   &( sIsPushPred ) )
              != IDE_SUCCESS );

    if ( sIsPushPred == ID_TRUE )
    {
        /* 3.2. ������ Table Predicate�� �����Ѵ�. */
        IDE_TEST( qmoPushPred::changePredicateNodeName( aStatement,
                                                        aViewParseTree->querySet,
                                                        sTupleId,
                                                        aPredicate->node,
                                                        &( sNode ) )
                  != IDE_SUCCESS );

        /* 3.3. �ش� Predicate�� �����ؼ� Mtc Stack ������ �˻��Ѵ�.
         *   - AND�� Argument�� ����Ǿ�, 1�� �����Ѵ�.
         */
        sRemain -= 1;

        IDE_TEST( qmg::checkStackOverflow( &( sNode->node ),
                                           sRemain,
                                           &( sIsOverflow ) )
                  != IDE_SUCCESS );

        IDE_TEST_CONT( sIsOverflow == ID_TRUE, NORMAL_EXIT );

        /* 3.4. ������ Predicate�� �����Ѵ�. */
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qmgPushPred,
                                &( sNewPred ) )
                  != IDE_SUCCESS );

        sNewPred->mNode = sNode;
        sNewPred->mNext = NULL;

        if ( *aHead == NULL )
        {
            sNewPred->mId = 1;

            *aHead = sNewPred;
            *aTail = sNewPred;
        }
        else
        {
            sNewPred->mId = (*aTail)->mId + 1;

            (*aTail)->mNext = sNewPred;
            *aTail          = sNewPred;
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* 4.   aPredicate->next ���� �˻��Ѵ�. */
    if ( aPredicate->next != NULL )
    {
        IDE_TEST( makePushPredicate( aStatement,
                                     aMyGraph,
                                     aViewParseTree,
                                     aOuterQuery,
                                     &( sRemain ),
                                     aPredicate->next,
                                     aHead,
                                     aTail )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* 5.   aPredicate->more ���� �˻��Ѵ�. */
    if ( aPredicate->more != NULL )
    {
        IDE_TEST( makePushPredicate( aStatement,
                                     aMyGraph,
                                     aViewParseTree,
                                     aOuterQuery,
                                     &( sRemain ),
                                     aPredicate->more,
                                     aHead,
                                     aTail )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    /* 6.   Stack ��뷮�� �����Ѵ�. */
    if ( aRemain != NULL )
    {
        *aRemain = sRemain;
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::makePushPredicate",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::makePushPredicate",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::makePushPredicate",
                                  "view parse tree is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PREDICATE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::makePushPredicate",
                                  "predicate tree is nul" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::checkPushProject( qcStatement  * aStatement,
                                         qmgShardSELT * aMyGraph,
                                         qmsParseTree * aParseTree,
                                         qmsTarget   ** aTarget,
                                         idBool       * aNeedWrapSet )
{
/****************************************************************************************
 *
 * Description : PROJ-2469 Optimize View Materialization�� View Target ����ȭ�� �̿��ؼ�
 *               Push Projection ���θ� �˻��ϴ� �Լ��̴�.
 *
 *               Shard Graph �Ʒ��� Graph ������ ���� �ʾƼ�, View Target ����ȭ��
 *               Shard Graph ������ ����Ǿ� �ִ�. ���� Shard Graph�� View Target
 *               ������ viewColumnRefList �� �����ϰ�, �� ������ Shard View Target ����
 *               �����ϵ��� qmoCheckViewColumnRef::checkUselessViewTarget ȣ���Ѵ�.
 *               �� Implicit Order By Transform ������ �����Ѵ�.
 *
 *               �� �Լ��� ���ʿ��� Target�� ã�Ƴ���, Shard Graph��
 *               qmgShardSelect::pushProjectOptimize ���� �� Target�� ������
 *               Shard Query���� ��ȯ�Ѵ�.
 *
 *               qmgShardSelect::pushProjectOptimize ���� ��ȯ�Ǵ� Shard Query�� �����ϰ�
 *               Tuple Column�� �����Ѵ�.
 *
 * Implementation : 1. ������Ƽ�� �˻��Ѵ�.
 *                  2. viewColumnRefList�� �����Ѵ�.
 *                  3. Shard View�� Push Projection ������ �����Ѵ�.
 *                  4. Shard View�� Implicit Order By Transform ������ �����Ѵ�.
 *
 ****************************************************************************************/

    qmsTarget        * sTarget       = NULL;
    qmsColumnRefList * sColumnRef    = NULL;
    qmsSortColumns   * sOrderBy      = NULL;
    idBool             sNeedPushProj = ID_FALSE;
    UInt               sTargetOrder  = 0;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );
    IDE_TEST_RAISE( aParseTree == NULL, ERR_NULL_PARSETREE );

    /* 1. ������Ƽ�� �˻��Ѵ�. */
    IDE_TEST_CONT( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_PROJECT_MASK )
                   != SDU_SHARD_TRANSFORM_PUSH_PROJECT_ENABLE,
                   NORMAL_EXIT );

    /* 2. viewColumnRefList�� �����Ѵ�. */
    for ( sTarget  = aMyGraph->graph.myQuerySet->target, sTargetOrder = 0;
          sTarget != NULL;
          sTarget  = sTarget->next, sTargetOrder++ )
    {
        if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK )
             == QMS_TARGET_IS_USELESS_FALSE )
        {
            for ( sColumnRef  = aMyGraph->graph.myFrom->tableRef->viewColumnRefList;
                  sColumnRef != NULL;
                  sColumnRef  = sColumnRef->next )
            {
                if ( sColumnRef->targetOrder == sTargetOrder )
                {
                    sColumnRef->isUsed = ID_TRUE;

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
    }

    sTarget    = aParseTree->querySet->target;
    sColumnRef = aMyGraph->graph.myFrom->tableRef->viewColumnRefList;
    sOrderBy   = ( (qmsParseTree *)( aStatement->myPlan->parseTree ) )->orderBy;

    /* 3. Shard View�� Push Projection ������ �����Ѵ�. */
    IDE_TEST( qmoCheckViewColumnRef::checkUselessViewTarget( sTarget,
                                                             sColumnRef,
                                                             sOrderBy )
              != IDE_SUCCESS );

    /* 4. Shard View�� Implicit Order By Transform ������ �����Ѵ�. */
    for ( sTarget  = aParseTree->querySet->target;
          sTarget != NULL;
          sTarget  = sTarget->next )
    {
        if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK )
             == QMS_TARGET_IS_USELESS_TRUE )
        {
            sNeedPushProj = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        if ( ( sTarget->flag & QMS_TARGET_SHARD_ORDER_BY_TRANS_MASK )
             != QMS_TARGET_SHARD_ORDER_BY_TRANS_NONE )
        {
            sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
            sTarget->flag |= QMS_TARGET_IS_USELESS_FALSE;

            sNeedPushProj = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    if ( sNeedPushProj == ID_TRUE )
    {
        if ( aParseTree->querySet->setOp != QMS_NONE )
        {
            *aNeedWrapSet = ID_TRUE;
            *aTarget      = aParseTree->querySet->target;
        }
        else
        {
            *aTarget = aParseTree->querySet->target;
        }
    }
    else
    {
        *aTarget = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushProject",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushProject",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushProject",
                                  "parse tree is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::checkPushSelect( qcStatement  * aStatement,
                                        qmgShardSELT * aMyGraph,
                                        qmsParseTree * aViewParseTree,
                                        qmsSFWGH     * aOuterQuery,
                                        qmgPushPred ** aPushPred,
                                        qtcNode     ** aWhere,
                                        idBool       * aNeedWrapSet )
{
/****************************************************************************************
 *
 * Description : ���� Where�� Mtc Stack ���Ѹ� ����ؼ� �ش� Predicate�� �����Ѵ�.
 *
 * Implementation : 1.   ������Ƽ�� �˻��Ѵ�.
 *                  2.1. Mtc Stack ��뷮�� ����Ѵ�.
 *                  2.2. ���� Where�� Mtc Stack ��뷮�� �����Ѵ�
 *                  2.   Predicate�� �����Ѵ�.
 *                  3.   Push Selection ���θ� ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    qmgPushPred * sPushPred = NULL;
    qmgPushPred * sTail     = NULL;
    qtcNode     * sWhere    = NULL;
    SInt          sRemain   = 0;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );
    IDE_TEST_RAISE( aViewParseTree == NULL, ERR_NULL_PARSETREE );

    /* 1.   ������Ƽ�� �˻��Ѵ�. */
    IDE_TEST_CONT( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_SELECT_MASK )
                   != SDU_SHARD_TRANSFORM_PUSH_SELECT_ENABLE,
                   NORMAL_EXIT );

    if ( aMyGraph->graph.myPredicate != NULL )
    {
        /* 2.1. Mtc Stack ��뷮�� ����Ѵ�. */
        sRemain = QCG_GET_SESSION_STACK_SIZE( aStatement );

        if ( aViewParseTree->querySet->setOp != QMS_NONE )
        {
            /* Nothing to do */
        }
        else
        {
            /* 2.2. ���� Where�� Mtc Stack ��뷮�� �����Ѵ�.
             *
             *   - �ֻ��� AND ������ �߰��� ����ؼ�, 1�� �����Ѵ�.
             *   - �ֻ��� Node�� AND�� ��쿡 Argument ������ŭ �����Ѵ�.
             *
             *   - Push Selection�� AND ����� �����ϴµ�,
             *   - ���� AND �����ڴ� �ֻ��� AND �������� �ټ� Argument�� ���δ�.
             */
            sWhere = aViewParseTree->querySet->SFWGH->where;

            if ( sWhere != NULL )
            {
                sRemain -= 1;

                if ( ( sWhere->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_AND )
                {
                    sRemain -= (SInt)( sWhere->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );
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

        /* 3.   Predicate�� �����Ѵ�. */
        IDE_TEST( makePushPredicate( aStatement,
                                     aMyGraph,
                                     aViewParseTree,
                                     aOuterQuery,
                                     &( sRemain ),
                                     aMyGraph->graph.myPredicate,
                                     &( sPushPred ),
                                     &( sTail ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    /* 4.   Push Selection ���θ� ��ȯ�Ѵ�. */
    if ( aViewParseTree->querySet->setOp != QMS_NONE )
    {
        if ( sPushPred != NULL )
        {
            *aNeedWrapSet = ID_TRUE;
            *aPushPred    = sPushPred;
            *aWhere       = NULL;
        }
        else
        {
            *aPushPred = NULL;
            *aWhere    = NULL;
        }
    }
    else
    {
        *aPushPred = sPushPred;
        *aWhere    = aViewParseTree->querySet->SFWGH->where;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushSelect",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushSelect",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushSelect",
                                  "parse tree is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::checkPushLimit( qcStatement     * aStatement,
                                       const qmgGraph  * aParent,
                                       qmgShardSELT    * aMyGraph,
                                       qmsParseTree    * aParseTree,
                                       qmsSortColumns ** aOrderBy,
                                       qmsLimit       ** aLimit )
{
/****************************************************************************************
 *
 * Description : Shard View �ٷ� �� Graph���� Limit�� �ִ��� �˻��ϴ� �Լ��̴�.
 *               �̷��� ��� �������� ��Ģ�� �°� Limit���� Query�� ��ȯ�ؼ�,
 *               �����ϸ� ����� �޶����� �ʴ´�.
 *
 * Implementation : 1. Limit�� �ִٸ�, ����ȭ���� �ʴ´�.
 *                  2. ������Ƽ�� �˻��Ѵ�.
 *                  3. Where ���� �ִٸ�, ����ȭ���� �ʴ´�
 *                  4. Push Selection�� �����ϸ� Flag�� ����Ѵ�.
 *                  5. Push Limit�� �����ϸ� ���θ� ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    qmsLimit       * sLimit     = NULL;
    qmsSortColumns * sOrderBy   = NULL;

    IDE_TEST_RAISE( aParent == NULL, ERR_NULL_PARENT_GRAPH );
    IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );
    IDE_TEST_RAISE( aParseTree == NULL, ERR_NULL_PARSETREE );

    /* 1. Limit�� �ִٸ�, ����ȭ���� �ʴ´�. */
    IDE_TEST_CONT( aParseTree->limit != NULL, NORMAL_EXIT );

    /* 2. ������Ƽ�� �˻��Ѵ�. */
    IDE_TEST_CONT( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_LIMIT_MASK )
                   != SDU_SHARD_TRANSFORM_PUSH_LIMIT_ENABLE,
                   NORMAL_EXIT );

    /* 3. Where ���� �ִٸ�, ����ȭ���� �ʴ´�. */
    IDE_TEST_CONT( aMyGraph->graph.myPredicate != NULL, NORMAL_EXIT );

    /* 4. Parent Graph���� Push�� ����� �˻��Ѵ�. */
    if ( aParent->type == QMG_PROJECTION )
    {
        sLimit = ( (qmgPROJ *)aParent )->limit;
    }
    else if ( aParent->type == QMG_SORTING )
    {
        if ( ( aParent->flag & QMG_SORT_OPT_TIP_MASK ) == QMG_SORT_OPT_TIP_LMST )
        {
            sOrderBy = ( (qmgSORT *)aParent )->orderBy;
            sLimit   = ( (qmsParseTree *)( aStatement->myPlan->parseTree ) )->limit;

            IDE_TEST_RAISE( sOrderBy == NULL, ERR_NULL_ORDER_BY );
            IDE_TEST_RAISE( sLimit   == NULL, ERR_NULL_LIMIT );
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

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    /* 5. Push Limit�� �����ϸ� ���θ� ��ȯ�Ѵ�. */
    if ( aOrderBy != NULL )
    {
        *aOrderBy = sOrderBy;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aLimit != NULL )
    {
        *aLimit = sLimit;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_PARENT_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushLimit",
                                  "parent graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushLimit",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PARSETREE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushLimit",
                                  "parse tree is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ORDER_BY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushLimit",
                                  "order by is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_LIMIT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkPushLimit",
                                  "limit is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgShardSelect::checkAndGetPushSeries( qcStatement      * aStatement,
                                              const qmgGraph   * aParent,
                                              qmgShardSELT     * aMyGraph,
                                              qmgTransformInfo * aInfo )
{
/****************************************************************************************
 *
 * Description : Push Projection, Push Selection, Push Limit ����ȭ�� ���� ��������
 *               ����ȭ ��Ҹ� �˻��Ѵ�.
 *
 * Implementation : 1. Plan String�� ���� ���� �����Ѵ�.
 *                  2. Ű���带 ����ߴٸ�, ����ȭ���� �ʴ´�.
 *                  3. Parent Graph ���� Push�� ����� ã�ƿ´�.
 *                  4. My Graph ���� Push�� ����� �˻��Ѵ�.
 *
 ****************************************************************************************/

    qcParamOffsetInfo * sParamOffsetInfo = NULL;
    iduVarString      * sString          = NULL;
    qmsParseTree      * sViewParseTree   = NULL;
    qmsQuerySet       * sViewQuerySet    = NULL;

    /* TASK-7219 Non-shard DML */
    qmsSFWGH          * sOuterQuery      = NULL;

    qmsTarget         * sTarget          = NULL;
    qmgPushPred       * sPushPred        = NULL;
    qtcNode           * sWhere           = NULL;
    qmsSortColumns    * sOrderBy         = NULL;
    qmsLimit          * sLimit           = NULL;
    idBool              sNeedWrapSet     = ID_FALSE;
    idBool              sUseShardKwd     = ID_FALSE; /* TASK-7219 Shard Transformer Refactoring */
    idBool              sIsTransform     = ID_FALSE;
    idBool              sIsSupported     = ID_FALSE; /* TASK-7219 Shard Transformer Refactoring */

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aInfo == NULL, ERR_NULL_INFO );

    sString = (iduVarString *)qci::mSessionCallback.mGetPlanString( QC_MM_STMT( aStatement ) );

    /* 1. Plan String�� ���� ���� �����Ѵ�. */
    if ( sString != NULL )
    {
        IDE_TEST_RAISE( aMyGraph == NULL, ERR_NULL_MY_GRAPH );
        IDE_TEST_RAISE( aMyGraph->graph.myFrom == NULL, ERR_NULL_FROM );
        IDE_TEST_RAISE( aMyGraph->graph.myFrom->tableRef->view == NULL, ERR_NULL_VIEW );
        IDE_TEST_RAISE( aMyGraph->graph.myQuerySet->SFWGH == NULL, ERR_NULL_SFWGH );

        sViewParseTree = (qmsParseTree *)aMyGraph->graph.myFrom->tableRef->view->myPlan->parseTree;
        sViewQuerySet  = sViewParseTree->querySet;

        sOuterQuery = aMyGraph->graph.myQuerySet->SFWGH->outerQuery;

        /* 2. Ű���带 ����ߴٸ�, ����ȭ���� �ʴ´�. */
        IDE_TEST_CONT( sViewParseTree->common.stmtShard != QC_STMT_SHARD_ANALYZE, NORMAL_EXIT );

        /* TASK-7219 Shard Transformer Refactoring */
        if ( sViewParseTree->isShardView == ID_TRUE )
        {
            sUseShardKwd = ID_TRUE;

            IDE_TEST( sdi::isSupported( sViewQuerySet,
                                        &( sIsSupported ) )
                      != IDE_SUCCESS );

            IDE_TEST_CONT( sIsSupported == ID_FALSE, NORMAL_EXIT );
        }
        else
        {
            /* Nothing to do */
        }

        /* 3. My Graph ���� Push�� ����� �˻��Ѵ�. */
        IDE_TEST( checkPushProject( aStatement,
                                    aMyGraph,
                                    sViewParseTree,
                                    &( sTarget ),
                                    &( sNeedWrapSet ) )
                  != IDE_SUCCESS );

        if ( sTarget != NULL )
        {
            sIsTransform = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        IDE_TEST( checkPushSelect( aStatement,
                                   aMyGraph,
                                   sViewParseTree,
                                   sOuterQuery,
                                   &( sPushPred ),
                                   &( sWhere ),
                                   &( sNeedWrapSet ) )
                  != IDE_SUCCESS );

        if ( sPushPred != NULL )
        {
            sIsTransform = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        /* 4. Parent Graph ���� Push�� ����� ã�ƿ´�. */
        IDE_TEST( checkPushLimit( aStatement,
                                  aParent,
                                  aMyGraph,
                                  sViewParseTree,
                                  &( sOrderBy ),
                                  &( sLimit ) )
                  != IDE_SUCCESS );

        if ( sLimit != NULL )
        {
            sIsTransform = ID_TRUE;
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

    if ( sIsTransform == ID_TRUE )
    {
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                qcParamOffsetInfo,
                                &( sParamOffsetInfo ) )
                  != IDE_SUCCESS );

        QC_INIT_PARAM_OFFSET_INFO( sParamOffsetInfo );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    aInfo->mString          = sString;
    aInfo->mGraph           = aMyGraph;
    aInfo->mStatement       = aStatement;
    aInfo->mQuerySet        = sViewQuerySet;
    aInfo->mTarget          = sTarget;
    aInfo->mPushPred        = sPushPred;
    aInfo->mWhere           = sWhere;
    aInfo->mOrderBy         = sOrderBy;
    aInfo->mLimit           = sLimit;
    aInfo->mParamOffsetInfo = sParamOffsetInfo;
    aInfo->mNeedWrapSet     = sNeedWrapSet;
    aInfo->mUseShardKwd     = sUseShardKwd; /* TASK-7219 Shard Transformer Refactoring */
    aInfo->mIsTransform     = sIsTransform;

    SET_POSITION( aInfo->mPosition, aMyGraph->shardQuery );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_MY_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "my graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_FROM )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "from is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_VIEW )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "shard viewis null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_SFWGH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmgShardSelect::checkAndGetPushSeries",
                                  "SFWGH is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
