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
 * $Id: qmgDelete.cpp 53774 2012-06-15 04:53:31Z eerien $
 *
 * Description :
 *     Delete Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgDelete.h>
#include <qmoOneNonPlan.h>
#include <qmgSelection.h>
#include <qmgPartition.h>

IDE_RC
qmgDelete::init( qcStatement * aStatement,
                 qmsQuerySet * aQuerySet,
                 qmgGraph    * aChildGraph,
                 qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgDelete Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgDelete�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgDETE         * sMyGraph;
    qmmDelParseTree * sParseTree;
    qmsQuerySet     * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgDelete::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Delete Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgDelete�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgDETE ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_DELETE;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgDelete::optimize;
    sMyGraph->graph.makePlan = qmgDelete::makePlan;
    sMyGraph->graph.printGraph = qmgDelete::printGraph;

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
    // Delete Graph ���� ���� �ʱ�ȭ
    //---------------------------------------------------

    sParseTree = (qmmDelParseTree *)aStatement->myPlan->parseTree;

    /* PROJ-2204 Join Update, Delete */
    sMyGraph->deleteTableRef = sParseTree->deleteTableRef;

    // �ֻ��� graph�� delete graph�� instead of trigger������ ����
    sMyGraph->insteadOfTrigger = sParseTree->insteadOfTrigger;

    // �ֻ��� graph�� delete graph�� limit�� ����
    sMyGraph->limit = sParseTree->limit;

    // �ֻ��� graph�� delete graph�� child constraint�� ����
    sMyGraph->childConstraints = sParseTree->childConstraints;

    // �ֻ��� graph�� delete graph�� return into�� ����
    sMyGraph->returnInto = sParseTree->returnInto;

    /* PROJ-2714 Multiple update, delete support */
    sMyGraph->mTableList = sParseTree->mTableList;
    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDelete::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgDelete�� ����ȭ
 *
 * Implementation :
 *    (1) SCAN Limit ����ȭ
 *    (2) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgDETE           * sMyGraph;
    qmgGraph          * sChildGraph;
    qtcNode           * sNode;
    qmsLimit          * sLimit;
    idBool              sIsScanLimit;

    SDouble             sOutputRecordCnt;
    SDouble             sDeleteSelectivity = 1;    // BUG-17166

    IDU_FIT_POINT_FATAL( "qmgDelete::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgDETE*) aGraph;
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
        // DETE ��� ������,
        // ���� SCAN ��忡�� SCAN Limit ������ Ȯ���Ǹ�,
        // DETE ����� limit start�� 1�� �����Ѵ�.

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
    sMyGraph->graph.costInfo.selectivity = sDeleteSelectivity;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sChildGraph->costInfo.outputRecordCnt;

    // outputRecordCnt
    sOutputRecordCnt = sChildGraph->costInfo.outputRecordCnt * sDeleteSelectivity;
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
qmgDelete::makePlan( qcStatement     * aStatement,
                     const qmgGraph  * aParent,
                     qmgGraph        * aGraph )
{
/***********************************************************************
 *
 * Description : qmgDelete���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgDelete���� ���������� Plan
 *
 *           [DETE]
 *
 ***********************************************************************/

    qmgDETE         * sMyGraph;
    qmnPlan         * sPlan;
    qmnPlan         * sChildPlan;
    qmoDETEInfo       sDETEInfo;

    IDU_FIT_POINT_FATAL( "qmgDelete::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgDETE*) aGraph;

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
        IDE_TEST( qmoOneNonPlan::initDETE( aStatement ,
                                           &sPlan )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qmoOneNonPlan::initMultiDETE( aStatement ,
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
    // SCAN Limit ����ȭ ���뿡 ���� DETE plan�� start value ��������
    if( sMyGraph->graph.left->type == QMG_SELECTION )
    {
        // projection ������ SCAN�̰�,
        // SCAN limit ����ȭ�� ����� ����̸�,
        // DETE�� limit start value�� 1�� �����Ѵ�.
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
    sMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_DELETE;

    //----------------------------
    // DETE�� ����
    //----------------------------

    sDETEInfo.deleteTableRef   = sMyGraph->deleteTableRef;
    sDETEInfo.insteadOfTrigger = sMyGraph->insteadOfTrigger;
    sDETEInfo.limit            = sMyGraph->limit;
    sDETEInfo.childConstraints = sMyGraph->childConstraints;
    sDETEInfo.returnInto       = sMyGraph->returnInto;
    sDETEInfo.mTableList       = sMyGraph->mTableList;

    if ( sMyGraph->mTableList == NULL )
    {
        IDE_TEST( qmoOneNonPlan::makeDETE( aStatement ,
                                           sMyGraph->graph.myQuerySet ,
                                           & sDETEInfo ,
                                           sChildPlan ,
                                           sPlan )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qmoOneNonPlan::makeMultiDETE( aStatement,
                                                sMyGraph->graph.myQuerySet,
                                                &sDETEInfo,
                                                sChildPlan,
                                                sPlan )
                  != IDE_SUCCESS );
    }

    sMyGraph->graph.myPlan = sPlan;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDelete::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgDelete::printGraph::__FT__" );

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
