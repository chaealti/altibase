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
 * $Id: qmgMove.cpp 53774 2012-06-15 04:53:31Z eerien $
 *
 * Description :
 *     Move Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgMove.h>
#include <qmoOneNonPlan.h>
#include <qmgSelection.h>
#include <qmoPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE
#include <qcmPartition.h>   // PROJ-1502 PARTITIONED DISK TABLE

IDE_RC
qmgMove::init( qcStatement * aStatement,
               qmsQuerySet * aQuerySet,
               qmgGraph    * aChildGraph,
               qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgMove Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgMove�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgMOVE          * sMyGraph;
    qmmMoveParseTree * sParseTree;
    qmsQuerySet      * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgMove::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Move Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgMove�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgMOVE ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_MOVE;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgMove::optimize;
    sMyGraph->graph.makePlan = qmgMove::makePlan;
    sMyGraph->graph.printGraph = qmgMove::printGraph;

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
    // Move Graph ���� ���� �ʱ�ȭ
    //---------------------------------------------------

    sParseTree = (qmmMoveParseTree *)aStatement->myPlan->parseTree;
    
    // �ֻ��� graph�� move graph�� target table ������ ����
    sMyGraph->targetTableRef = sParseTree->targetTableRef;

    // �ֻ��� graph�� move graph�� column ������ ����
    sMyGraph->columns = sParseTree->columns;

    // �ֻ��� graph�� move graph�� value ������ ����
    sMyGraph->values         = sParseTree->values;
    sMyGraph->valueIdx       = sParseTree->valueIdx;
    sMyGraph->canonizedTuple = sParseTree->canonizedTuple;
    sMyGraph->compressedTuple= sParseTree->compressedTuple;     // PROJ-2264
    
    // �ֻ��� graph�� move graph�� sequence ������ ����
    sMyGraph->nextValSeqs = sParseTree->common.nextValSeqs;
    
    // �ֻ��� graph�� move graph�� limit�� ����
    sMyGraph->limit = sParseTree->limit;

    // �ֻ��� graph�� move graph�� constraint�� ����
    sMyGraph->parentConstraints = sParseTree->parentConstraints;
    sMyGraph->childConstraints  = sParseTree->childConstraints;
    sMyGraph->checkConstrList   = sParseTree->checkConstrList;

    // �ֻ��� graph�� insert graph�� Default Expr�� ����
    sMyGraph->defaultExprTableRef = sParseTree->defaultTableRef;
    sMyGraph->defaultExprColumns  = sParseTree->defaultExprColumns;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgMove::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgMove�� ����ȭ
 *
 * Implementation :
 *    (1) SCAN Limit ����ȭ
 *    (2) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgMOVE           * sMyGraph;
    qmgGraph          * sChildGraph;
    qtcNode           * sNode;
    qmsLimit          * sLimit;
    idBool              sIsScanLimit;

    SDouble             sOutputRecordCnt;
    SDouble             sMoveSelectivity = 1;    // BUG-17166

    IDU_FIT_POINT_FATAL( "qmgMove::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgMOVE*) aGraph;
    sChildGraph = aGraph->left;

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
        // MOVE ��� ������,
        // ���� SCAN ��忡�� SCAN Limit ������ Ȯ���Ǹ�,
        // MOVE ����� limit start�� 1�� �����Ѵ�.

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
    sMyGraph->graph.costInfo.selectivity = sMoveSelectivity;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sChildGraph->costInfo.outputRecordCnt;

    // outputRecordCnt
    sOutputRecordCnt = sChildGraph->costInfo.outputRecordCnt * sMoveSelectivity;
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
qmgMove::makePlan( qcStatement     * aStatement,
                   const qmgGraph  * aParent,
                   qmgGraph        * aGraph )
{
/***********************************************************************
 *
 * Description : qmgMove���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgMove���� ���������� Plan
 *
 *           [MOVE]
 *
 ***********************************************************************/

    qmgMOVE         * sMyGraph;
    qmnPlan         * sPlan;
    qmnPlan         * sChildPlan;
    qmmValueNode    * sValueNode;
    qmoMOVEInfo       sMOVEInfo;

    IDU_FIT_POINT_FATAL( "qmgMove::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgMOVE*) aGraph;

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
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;

    //---------------------------
    // Plan�� ����
    //---------------------------

    // �ֻ��� plan�̴�.
    IDE_DASSERT( aParent == NULL );
    
    IDE_TEST( qmoOneNonPlan::initMOVE( aStatement ,
                                       &sPlan )
              != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sPlan;
    
    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( sMyGraph->graph.left->makePlan( aStatement ,
                                              &sMyGraph->graph,
                                              sMyGraph->graph.left )
              != IDE_SUCCESS);

    // fix BUG-13482
    // SCAN Limit ����ȭ ���뿡 ���� MOVE plan�� start value ��������
    if( sMyGraph->graph.left->type == QMG_SELECTION )
    {
        // projection ������ SCAN�̰�,
        // SCAN limit ����ȭ�� ����� ����̸�,
        // MOVE�� limit start value�� 1�� �����Ѵ�.
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
    sMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_MOVE;

    //----------------------------
    // MOVE�� ����
    //----------------------------

    sMOVEInfo.targetTableRef    = sMyGraph->targetTableRef;
    sMOVEInfo.columns           = sMyGraph->columns;
    sMOVEInfo.values            = sMyGraph->values;
    sMOVEInfo.valueIdx          = sMyGraph->valueIdx;
    sMOVEInfo.canonizedTuple    = sMyGraph->canonizedTuple;
    sMOVEInfo.compressedTuple   = sMyGraph->compressedTuple;    // PROJ-2264
    sMOVEInfo.nextValSeqs       = sMyGraph->nextValSeqs;
    sMOVEInfo.limit             = sMyGraph->limit;
    sMOVEInfo.parentConstraints = sMyGraph->parentConstraints;
    sMOVEInfo.childConstraints  = sMyGraph->childConstraints;
    sMOVEInfo.checkConstrList   = sMyGraph->checkConstrList;

    /* PROJ-1090 Function-based Index */
    sMOVEInfo.defaultExprTableRef = sMyGraph->defaultExprTableRef;
    sMOVEInfo.defaultExprColumns  = sMyGraph->defaultExprColumns;

    IDE_TEST( qmoOneNonPlan::makeMOVE( aStatement ,
                                       sMyGraph->graph.myQuerySet ,
                                       & sMOVEInfo ,
                                       sChildPlan ,
                                       sPlan )
              != IDE_SUCCESS);
    sMyGraph->graph.myPlan = sPlan;

    qmg::setPlanInfo( aStatement, sPlan, &(sMyGraph->graph) );

    //------------------------------------------
    // INSERT ... VALUES ���� ���� Subquery ����ȭ
    //------------------------------------------

    // BUG-32584
    // ��� ���������� ���ؼ� MakeGraph ���Ŀ� MakePlan�� �ؾ� �Ѵ�.
    for ( sValueNode = sMyGraph->values;
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

    for ( sValueNode = sMyGraph->values;
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

    //----------------------------
    // into���� ���� ��Ƽ�ǵ� ���̺� ����ȭ
    //----------------------------
        
    // PROJ-1502 PARTITIONED DISK TABLE
    IDE_TEST( qmoPartition::optimizeInto( aStatement,
                                          sMyGraph->targetTableRef )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmgMove::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgMove::printGraph::__FT__" );

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
