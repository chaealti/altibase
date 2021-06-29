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
 * $Id: qmg.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     ��� Graph���� ���������� ����ϴ� ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qtc.h>
#include <qcg.h>
#include <qmgDef.h>
#include <qmgJoin.h>
#include <qmgLeftOuter.h>
#include <qmgFullOuter.h>
#include <qmgSelection.h>
#include <qmgPartition.h>
#include <qmgWindowing.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoJoinMethod.h>
#include <qmgSorting.h>
#include <qmgGrouping.h>
#include <qmgDistinction.h>
#include <qmgCounting.h>
#include <qmoCostDef.h>
#include <qmoViewMerging.h>
#include <qcgPlan.h>
#include <qmvQTC.h>
#include <qmv.h>

extern mtfModule mtfDecrypt;
extern mtdModule mtdClob;
extern mtdModule mtdBlob;
extern mtdModule mtdSmallint;

IDE_RC
qmg::initGraph( qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description :
 *    Graph�� �����ϴ� ���� ������ �ʱ�ȭ�Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::initGraph::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // Graph�� �⺻ ���� �ʱ�ȭ
    //---------------------------------------------------

    aGraph->flag = QMG_FLAG_CLEAR;
    qtc::dependencyClear( & aGraph->depInfo );

    //---------------------------------------------------
    // Graph�� Child Graph �ʱ�ȭ
    //---------------------------------------------------

    aGraph->left     = NULL;
    aGraph->right    = NULL;
    aGraph->children = NULL;

    //---------------------------------------------------
    // Graph�� �ΰ� ���� �ʱ�ȭ
    //---------------------------------------------------

    aGraph->myPlan            = NULL;
    aGraph->myPredicate       = NULL;
    aGraph->constantPredicate = NULL;
    aGraph->ridPredicate      = NULL;
    aGraph->nnfFilter         = NULL;
    aGraph->myFrom            = NULL;
    aGraph->myQuerySet        = NULL;
    aGraph->myCNF             = NULL;
    aGraph->preservedOrder    = NULL;
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmg::printGraph::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aString != NULL );

    //-----------------------------------
    // Graph ������ ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "-------------------------------------------------" );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    switch ( aGraph->type )
    {
        case QMG_SELECTION :
            iduVarStringAppend( aString,
                                "[[ SELECTION GRAPH ]]" );
            break;
        case QMG_PROJECTION :
            iduVarStringAppend( aString,
                                "[[ PROJECTION GRAPH ]]" );
            break;
        case QMG_DISTINCTION :
            iduVarStringAppend( aString,
                                "[[ DISTINCTION GRAPH ]]" );
            break;
        case QMG_GROUPING :
            iduVarStringAppend( aString,
                                "[[ GROUPING GRAPH ]]" );
            break;
        case QMG_SORTING :
            iduVarStringAppend( aString,
                                "[[ SORTING GRAPH ]]" );
            break;
        case QMG_INNER_JOIN :
            iduVarStringAppend( aString,
                                "[[ INNER JOIN GRAPH ]]" );
            break;
        case QMG_SEMI_JOIN :
            iduVarStringAppend( aString,
                                "[[ SEMI JOIN GRAPH ]]" );
            break;
        case QMG_ANTI_JOIN :
            iduVarStringAppend( aString,
                                "[[ ANTI JOIN GRAPH ]]" );
            break;
        case QMG_LEFT_OUTER_JOIN :
            iduVarStringAppend( aString,
                                "[[ LEFT OUTER JOIN GRAPH ]]" );
            break;
        case QMG_FULL_OUTER_JOIN :
            iduVarStringAppend( aString,
                                "[[ FULL OUTER JOIN GRAPH ]]" );
            break;
        case QMG_SET :
            iduVarStringAppend( aString,
                                "[[ SET GRAPH ]]" );
            break;
        case QMG_HIERARCHY :
            iduVarStringAppend( aString,
                                "[[ HIERARCHY GRAPH ]]" );
            break;
        case QMG_DNF :
            iduVarStringAppend( aString,
                                "[[ DNF GRAPH ]]" );
            break;
        case QMG_PARTITION :
            iduVarStringAppend( aString,
                                "[[ PARTITION GRAPH ]]" );
            break;
        case QMG_COUNTING :
            iduVarStringAppend( aString,
                                "[[ COUNTING GRAPH ]]" );
            break;
        case QMG_WINDOWING :
            iduVarStringAppend( aString,
                                "[[ WINDOWING GRAPH ]]" );
            break;         
        case QMG_INSERT :
            iduVarStringAppend( aString,
                                "[[ INSERT GRAPH ]]" );
            break;         
        case QMG_MULTI_INSERT :
            iduVarStringAppend( aString,
                                "[[ MULTIPLE INSERT GRAPH ]]" );
            break;         
        case QMG_UPDATE :
            iduVarStringAppend( aString,
                                "[[ UPDATE GRAPH ]]" );
            break;         
        case QMG_DELETE :
            iduVarStringAppend( aString,
                                "[[ DELETE GRAPH ]]" );
            break;         
        case QMG_MOVE :
            iduVarStringAppend( aString,
                                "[[ MOVE GRAPH ]]" );
            break;         
        case QMG_MERGE :
            iduVarStringAppend( aString,
                                "[[ MERGE GRAPH ]]" );
            break;         
        case QMG_RECURSIVE_UNION_ALL :
            iduVarStringAppend( aString,
                                "[[ RECURSIVE UNION ALL GRAPH ]]" );
            break;         
        case QMG_SHARD_SELECT:
            iduVarStringAppend( aString,
                                "[[ SHARD SELECT GRAPH ]]" );
            break;
        case QMG_SHARD_DML:
            iduVarStringAppend( aString,
                                "[[ SHARD DML GRAPH ]]" );
            break;
        case QMG_SHARD_INSERT:
            iduVarStringAppend( aString,
                                "[[ SHARD INSERT GRAPH ]]" );
            break;
        default :
            IDE_DASSERT(0);
            break;
    }

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "-------------------------------------------------" );

    //-----------------------------------
    // Graph ���� ��� ������ ���
    //-----------------------------------

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppend( aString,
                        "== Cost Information ==" );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "INPUT_RECORD_COUNT : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.inputRecordCnt );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "OUTPUT_RECORD_COUNT: %"ID_PRINT_G_FMT,
                              aGraph->costInfo.outputRecordCnt );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "RECORD_SIZE        : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.recordSize );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "SELECTIVITY        : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.selectivity );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "GRAPH_ACCESS_COST  : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.myAccessCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "GRAPH_DISK_COST    : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.myDiskCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "GRAPH_TOTAL_COST   : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.myAllCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "TOTAL_ACCESS_COST  : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.totalAccessCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "TOTAL_DISK_COST    : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.totalDiskCost );

    QMG_PRINT_LINE_FEED( i, aDepth, aString );
    iduVarStringAppendFormat( aString,
                              "TOTAL_ALL_COST     : %"ID_PRINT_G_FMT,
                              aGraph->costInfo.totalAllCost );
    
    return IDE_SUCCESS;

}

IDE_RC
qmg::printSubquery( qcStatement  * aStatement,
                    qtcNode      * aSubQuery,
                    ULong          aDepth,
                    iduVarString * aString )
{
/***********************************************************************
 *
 * Description :
 *    Subquery ���� Graph ������ ����Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt  i;
    UInt  sArguNo;
    UInt  sArguCount;

    qtcNode  * sNode;
    qmgGraph * sGraph;

    IDU_FIT_POINT_FATAL( "qmg::printSubquery::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSubQuery != NULL );
    IDE_DASSERT( aString != NULL );

    //-----------------------------------
    // Graph ������ ���
    //-----------------------------------

    if ( ( aSubQuery->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_SUBQUERY )
    {
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "::::SUB-QUERY BEGIN" );
        QMG_PRINT_LINE_FEED( i, aDepth, aString );

        sGraph = aSubQuery->subquery->myPlan->graph;
        IDE_TEST( sGraph->printGraph( aStatement,
                                      sGraph,
                                      aDepth + 1,
                                      aString ) != IDE_SUCCESS );

        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
        iduVarStringAppend( aString,
                            "::::SUB-QUERY END" );
        QMG_PRINT_LINE_FEED( i, aDepth, aString );
    }
    else
    {
        sArguCount = ( aSubQuery->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );

        if ( sArguCount > 0 ) // This node has arguments.
        {
            if ( (aSubQuery->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                 == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                for (sNode = (qtcNode *)aSubQuery->node.arguments;
                     sNode != NULL;
                     sNode = (qtcNode *)sNode->node.next)
                {
                    IDE_TEST( printSubquery( aStatement,
                                             sNode,
                                             aDepth,
                                             aString ) != IDE_SUCCESS );
                }
            }
            else
            {
                for (sArguNo = 0,
                         sNode = (qtcNode *)aSubQuery->node.arguments;
                     sArguNo < sArguCount && sNode != NULL;
                     sArguNo++,
                         sNode = (qtcNode *)sNode->node.next)
                {
                    IDE_TEST( printSubquery( aStatement,
                                             sNode,
                                             aDepth,
                                             aString ) != IDE_SUCCESS );
                }
            }
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
qmg::checkUsableOrder( qcStatement       * aStatement,
                       qmgPreservedOrder * aWantOrder,
                       qmgGraph          * aLeftGraph,
                       qmoAccessMethod  ** aOriginalMethod,
                       qmoAccessMethod  ** aSelectMethod,
                       idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Graph�� ���ϴ� Order�� ����� �� �ִ� ���� �˻��Ѵ�.
 *    [����]
 *       - �Էµ� aWantOrder�� ó�� �������� ���Ƿ�
 *         ���� ���谡 ����� �� �����Ƿ� ������ �� ����.
 *
 *    [���ϰ�]
 *        ��� ���� ����                     : aUsable
 *        ��� ���ɽ� ���õǴ� Access Method : aSelectMethod
 *        ������ ���Ǵ� Access Method      : aOriginalMethod
 *
 * Implementation :
 *
 *    [aWantOrder�� ����]
 *
 *       - Graph�� ���ϴ� Order
 *
 *       - Order�� �߿��� ��쿡�� ���ϴ� Order��
 *         ASC, DESC���� ����Ѵ�.
 *           - Ex) Sorting������ Order
 *                 SELECT * FROM T1 ORDER BY I1 ASC, I2 DESC;
 *                 (1,1,ASC) --> (1,2,DESC)
 *
 *           - Ex) Merge Join������ Order (��� Ascending)
 *                 SELECT * FROM T1, T2 WHERE T1.i1 = T2.i1;
 *                 (1,1,ASC) --> (2,1, DESC)
 *
 *       - Order�� �߿����� ���� ��쿡�� ���ϴ� Order��
 *         ���� ������ ������� �ʴ´�.
 *           - Ex) Distinction������ Order
 *                 SELECT DISTINCT t1.i1, t2.i2, t1.i2 from t1, t2;
 *                 (1,1,NOT) --> (2,2,NOT) --> (1,2,NOT)
 *
 *    [ ó�� ���� ]
 *
 *    1. Child Graph�� �Ҽӵ� Want Order�� ����
 *        - ���ϴ� Order�� �¿� Child �� ���ϴ� ���� �˻��ϰ�,
 *          �̸� �¿�� �����Ѵ�.
 *
 *    2. Child Graph���� �ش� Order�� ó���� �� �ִ� ���� �˻�.
 *
 ***********************************************************************/

    idBool sUsable;

    IDU_FIT_POINT_FATAL( "qmg::checkUsableOrder::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aLeftGraph != NULL );
    IDE_DASSERT( aUsable != NULL );
    IDE_DASSERT( aOriginalMethod != NULL );
    IDE_DASSERT( aSelectMethod != NULL );

    IDE_TEST( checkOrderInGraph( aStatement,
                                 aLeftGraph,
                                 aWantOrder,
                                 aOriginalMethod,
                                 aSelectMethod,
                                 & sUsable )
              != IDE_SUCCESS );

    *aUsable = sUsable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmg::setOrder4Child( qcStatement       * aStatement,
                     qmgPreservedOrder * aWantOrder,
                     qmgGraph          * aLeftGraph )
{
/***********************************************************************
 *
 * Description :
 *
 *    ���ϴ� Order�� Child Graph�� �����Ų��.
 *
 *    [���� 1]
 *       - �Էµ� aWantOrder�� ó�� �������� ���Ƿ�
 *         ���� ���谡 ����� �� �����Ƿ� ������ �� ����.
 *
 *    [���� 2]
 *       - �ݵ�� Preserved Order�� ����� �� �ִ� ��쿡 ���Ͽ� ����
 *       - ��, qmg::checkUsableOrder() �� ���� ����� �� �ִ� Order��
 *         ��쿡 ����.
 *
 *    [���� 3]
 *       - �ش� Graph�� Preserved Order�� �ܺο��� ���� �����ؾ� ��.
 *
 * Implementation :
 *
 *    1. Want Order�� �� Child Graph�� Order�� �и�
 *
 *    2. �ش� Graph�� ������ ���� ó��
 *        - Selection Graph�� ���
 *            - Base Table �� ��� :
 *                �ش� Index�� ã�� �� Preserved Order Build
 *            - View�� ���
 *                ���� Target�� ID�� �����Ͽ� Child Graph�� ���� ó��
 *                �Էµ� Want Order�� Preserved Order Build
 *        - Set, Dnf, Hierarchy�� ���
 *            - �ش� ���� ����.
 *        - �� ���� Graph�� ���
 *            - �Էµ� Want Order�� Preserved Order Build
 *            - Recursive�� ����
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::setOrder4Child::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aLeftGraph != NULL );

    //---------------------------------------------------
    // Left Graph�� ���� Preserved Order ����
    //---------------------------------------------------

    IDE_TEST( makeOrder4Graph( aStatement,
                               aLeftGraph,
                               aWantOrder )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
    
}

IDE_RC
qmg::tryPreservedOrder( qcStatement       * aStatement,
                        qmgGraph          * aGraph,
                        qmgPreservedOrder * aWantOrder,
                        idBool            * aSuccess )
{
/***********************************************************************
 *
 * Description : Preserved Order ��� ������ ���, ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmgPreservedOrder * sCur;
    qmgPreservedOrder * sNewOrder;

    qmgPreservedOrder * sCheckOrder;
    qmgPreservedOrder * sCheckCurOrder;
    qmgPreservedOrder * sPushOrder;
    qmgPreservedOrder * sPushCurOrder;
    qmgPreservedOrder * sChildCur;

    qmoAccessMethod   * sOriginalMethod;
    qmoAccessMethod   * sSelectMethod;
    idBool              sIsDefined = ID_FALSE;
    idBool              sUsable;

    IDU_FIT_POINT_FATAL( "qmg::tryPreservedOrder::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( aWantOrder != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sUsable        = ID_FALSE;
    sCheckOrder    = NULL;
    sCheckCurOrder = NULL;
    sPushOrder     = NULL;
    sPushCurOrder  = NULL;


    for ( sCur = aWantOrder; sCur != NULL; sCur = sCur->next )
    {
        // �˻��� Order, Push�� Order�� ���� ���� �Ҵ�
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder) * 2,
                                                 (void**)&sNewOrder )
                  != IDE_SUCCESS );

        sNewOrder[0].table = sCur->table;
        sNewOrder[0].column = sCur->column;
        sNewOrder[0].direction = sCur->direction;
        sNewOrder[0].next = NULL;

        idlOS::memcpy( & sNewOrder[1],
                       & sNewOrder[0],
                       ID_SIZEOF( qmgPreservedOrder ) );

        if ( sCheckOrder == NULL )
        {
            sCheckOrder = sCheckCurOrder = & sNewOrder[0];
            sPushOrder = sPushCurOrder = & sNewOrder[1];
        }
        else
        {
            sCheckCurOrder->next = & sNewOrder[0];
            sCheckCurOrder = sCheckCurOrder->next;

            sPushCurOrder->next = & sNewOrder[1];
            sPushCurOrder = sPushCurOrder->next;
        }

        //BUG-40361 supporting to indexable analytic function
        if ( sCur->direction != QMG_DIRECTION_NOT_DEFINED )
        {
            sIsDefined = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    // preserved order ���� ���� �˻�
    IDE_TEST( qmg::checkUsableOrder( aStatement,
                                     sCheckOrder,
                                     aGraph->left,
                                     & sOriginalMethod,
                                     & sSelectMethod,
                                     & sUsable )
              != IDE_SUCCESS );

    if ( sUsable == ID_TRUE )
    {
        //---------------------------------------------------
        // preserved order ���� ������ ���
        //---------------------------------------------------

        // To Fix PR-11945
        // Child Graph�� preserved order�� �����ϰ�,
        // preserved order�� ���⵵ �����ϴ� �����
        // ���� preserved order�� �籸���ؼ��� �ȵȴ�.
        // �������� Merger Join�� ���� ASC order�� ������ ���,
        // ������ Group By���� ���� �� ������ ������ ������Ѽ��� �ȵȴ�.
        
        if ( aGraph->left->preservedOrder != NULL )
        {
            if ( ( aGraph->left->flag & QMG_PRESERVED_ORDER_MASK )
                 == QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED )
            {
                // ������ Window Graph�� �����ϴ� ���
                IDE_TEST( qmg::setOrder4Child( aStatement,
                                               sPushOrder,
                                               aGraph->left )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( aGraph->left->preservedOrder->direction !=
                     QMG_DIRECTION_NOT_DEFINED )
                {
                    // ������ Preserved Order�� �����ϰ�
                    // ���⵵ �̹� ������ ���
                    // Nothing To Do
                }
                else
                {
                    // ������ Preserved Order�� ������,
                    // ������ �������� ���� ���
                    IDE_TEST( qmg::setOrder4Child( aStatement,
                                                   sPushOrder,
                                                   aGraph->left )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // ������ Preserved Order�� ���� ���
            // ������ preserved order ����
            IDE_TEST( qmg::setOrder4Child( aStatement,
                                           sPushOrder,
                                           aGraph->left )
                      != IDE_SUCCESS );
        }

        // �ڽ��� preserved order ����
        if ( sIsDefined == ID_FALSE )
        {
            // To Fix BUG-8710
            // ex ) want order ( not defined, not defined )
            //      set  order ( not defined, same ( diff ) with prev, ... )
            // want order�� not defined�� �ϴ��� direction�� ������Ѿ���
            // To Fix BUG-11373
            // child�� order�� indexable group by �� ���
            // index�� ������� �ٲ�� ��찡 �ֱ� ������
            // child�� order�� �״�� �����ؾ���.
            // ex ) index            : i1->i2->i3
            //      group by         : i2->i1->i3
            //      selection graph  : i2->i1->i3�� �����ָ� index�������
            //                         i1->i2->i3���� �����Ͽ� �����
            for ( sCur=aWantOrder,sChildCur = aGraph->left->preservedOrder;
                  (sCur != NULL) && (sChildCur != NULL);
                  sCur = sCur->next, sChildCur = sChildCur->next )
            {
                sCur->direction = sChildCur->direction;
                sCur->table     = sChildCur->table;
                sCur->column    = sChildCur->column;
            }
        }
        else
        {
            // nothing to do
        }

        aGraph->preservedOrder = aWantOrder;
    }
    else
    {
        aGraph->preservedOrder = NULL;
    }

    *aSuccess = sUsable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * ----------------------------------------------------------------------------
 * BUG-39298 improve preserved order
 *
 * ex)
 * create index .. on t1 (i1, i2, i3)
 * select .. from .. where t1.i1 = 1 and t1.i2 = 2 order by i3;
 * (i1, i2) �� ������ ���̹Ƿ� preserved order �� ����� �� �ֵ��� �Ѵ�.
 *
 * ������ ���� ��Ȳ������ ó���Ѵ�.
 * - SORT �� ���� preserved order (grouping, distinct ����)
 * - qmgSort �ؿ� qmgSelection �� ���
 * - index �� ���õ� ��� (preserved order �� ���� index ���� ���� X)
 * - index hint �� ASC, DESC �� ���°��
 * ----------------------------------------------------------------------------
 */
IDE_RC qmg::retryPreservedOrder(qcStatement       * aStatement,
                                qmgGraph          * aGraph,
                                qmgPreservedOrder * aWantOrder,
                                idBool            * aUsable)
{
    qmgGraph         * sChildGraph;
    qmgSELT          * sSELTGraph;
    qmgPreservedOrder* sIter;
    qmgPreservedOrder* sIter2;
    qmgPreservedOrder* sStart;
    qmgPreservedOrder* sNewOrder;
    qmoKeyRangeColumn* sKeyRangeCol;
    idBool             sUsable;
    UInt               sOrderCount;
    UInt               sStartIdx;
    UInt               i;

    IDU_FIT_POINT_FATAL( "qmg::retryPreservedOrder::__FT__" );

    sUsable = ID_FALSE;

    IDE_DASSERT(aWantOrder   != NULL);
    IDE_DASSERT(aGraph->left != NULL);

    sChildGraph = aGraph->left;

    if (aWantOrder->direction == QMG_DIRECTION_NOT_DEFINED)
    {
        IDE_CONT(LABEL_EXIT);
    }

    if (sChildGraph->preservedOrder == NULL)
    {
        IDE_CONT(LABEL_EXIT);
    }

    if (sChildGraph->preservedOrder->direction != QMG_DIRECTION_NOT_DEFINED)
    {
        IDE_CONT(LABEL_EXIT);
    }

    if (sChildGraph->type != QMG_SELECTION)
    {
        IDE_CONT(LABEL_EXIT);
    }

    sSELTGraph = (qmgSELT*)sChildGraph;

    if (sSELTGraph->selectedMethod == NULL)
    {
        IDE_CONT(LABEL_EXIT);
    }

    if (sSELTGraph->selectedMethod->method == NULL)
    {
        IDE_CONT(LABEL_EXIT);
    }

    if (sChildGraph->myFrom->tableRef->table != aWantOrder->table)
    {
        IDE_CONT(LABEL_EXIT);
    }

    sKeyRangeCol  = &sSELTGraph->selectedMethod->method->mKeyRangeColumn;

    for (sOrderCount = 0, sIter = sChildGraph->preservedOrder;
         sIter != NULL;
         sIter = sIter->next, sOrderCount++);

    for (sIter = sChildGraph->preservedOrder, sStartIdx = 0;
         sIter != NULL;
         sIter = sIter->next, sStartIdx++)
    {
        if (sIter->column != aWantOrder->column)
        {
            for (i = 0; i < sKeyRangeCol->mColumnCount; i++)
            {
                if (sKeyRangeCol->mColumnArray[i] == sIter->column)
                {
                    break;
                }
            }
            if (i == sKeyRangeCol->mColumnCount)
            {
                /* no way match */
                IDE_CONT(LABEL_EXIT);
            }
            else
            {
                /* try next column */
            }
        }
        else
        {
            /* start position found */
            break;
        }
    }

    if (sIter == NULL)
    {
        IDE_CONT(LABEL_EXIT);
    }

    sStart = sIter;

    for (sIter = aWantOrder, sIter2 = sStart;
         sIter != NULL && sIter2 != NULL;
         sIter = sIter->next, sIter2 = sIter2->next)
    {
        if ((sIter->table != sIter2->table) ||
            (sIter->column != sIter2->column))
        {
            break;
        }
    }

    if (sIter != NULL)
    {
        /* do not cover all want orders */
        IDE_CONT(LABEL_EXIT);
    }

    IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qmgPreservedOrder) *
                                           sOrderCount,
                                           (void**)&sNewOrder)
             != IDE_SUCCESS);

    /*
     * forward �� �������� ��
     */
    sNewOrder[0].table     = sChildGraph->preservedOrder->table;
    sNewOrder[0].column    = sChildGraph->preservedOrder->column;
    sNewOrder[0].direction = QMG_DIRECTION_ASC;

    for (i = 1, sIter = sChildGraph->preservedOrder->next;
         i < sOrderCount;
         i++, sIter = sIter->next)
    {
        sNewOrder[i].table  = sIter->table;
        sNewOrder[i].column = sIter->column;
        switch (sIter->direction)
        {
            case QMG_DIRECTION_SAME_WITH_PREV:
                if (sNewOrder[i-1].direction == QMG_DIRECTION_ASC)
                {
                    sNewOrder[i].direction = QMG_DIRECTION_ASC;
                }
                else
                {
                    sNewOrder[i].direction = QMG_DIRECTION_DESC;
                }
                break;
            case QMG_DIRECTION_DIFF_WITH_PREV:
                if (sNewOrder[i-1].direction == QMG_DIRECTION_ASC)
                {
                    sNewOrder[i].direction = QMG_DIRECTION_DESC;
                }
                else
                {
                    sNewOrder[i].direction = QMG_DIRECTION_ASC;
                }
                break;
            default:
                IDE_CONT(LABEL_EXIT);
        }
        sNewOrder[i-1].next = &sNewOrder[i];
    }

    for (sIter = aWantOrder, sIter2 = &sNewOrder[sStartIdx];
         sIter != NULL;
         sIter = sIter->next, sIter2 = sIter2->next)
    {
        if (sIter->direction != sIter2->direction)
        {
            break;
        }
    }

    if (sIter == NULL)
    {
        sUsable = ID_TRUE;
        IDE_CONT(LABEL_EXIT);
    }

    /*
     * forward �� ������ ��� backward �� �������� ��
     */
    for (i = 0; i < sOrderCount; i++)
    {
        if (sNewOrder[i].direction == QMG_DIRECTION_ASC)
        {
            sNewOrder[i].direction = QMG_DIRECTION_DESC;
        }
        else
        {
            sNewOrder[i].direction = QMG_DIRECTION_ASC;
        }
    }

    for (sIter = aWantOrder, sIter2 = &sNewOrder[sStartIdx];
         sIter != NULL;
         sIter = sIter->next, sIter2 = sIter2->next)
    {
        if (sIter->direction != sIter2->direction)
        {
            break;
        }
    }

    if (sIter == NULL)
    {
        sUsable = ID_TRUE;
    }

    IDE_EXCEPTION_CONT(LABEL_EXIT);

    *aUsable = sUsable;

    if (sUsable == ID_TRUE)
    {
        /*
         * match �� ��� child �� preserved order �� fix
         */
        for (sIter = sChildGraph->preservedOrder, i = 0;
             i < sOrderCount;
             sIter = sIter->next, i++)
        {
            sIter->direction = sNewOrder[i].direction;
        }
    }
    else
    {
        /* nothing to do */
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getBucketCntWithTarget(qcStatement* aStatement,
                                   qmgGraph   * aGraph,
                                   qmsTarget  * aTarget,
                                   UInt         aHintBucketCnt,
                                   UInt       * aBucketCnt )
{
/***********************************************************************
 *
 * Description : target�� Į������ cardinality�� �̿��Ͽ� bucket count ���ϴ�
 *               �Լ�
 *
 * Implementation :
 *    - hash bucket count hint�� �������� ���� ���
 *      hash bucket count = MIN( ���� graph�� outputRecordCnt / 2,
 *                               Target Column���� cardinality �� )
 *    - hash bucket count hint�� ������ ���
 *      hash bucket count = hash bucket count hint ��
 *
 ***********************************************************************/

    qmsTarget      * sTarget;
    qtcNode        * sNode;
    qmoColCardInfo * sColCardInfo;
    SDouble          sCardinality;
    SDouble          sBucketCnt;
    ULong            sBucketCntOutput;
    idBool           sAllColumn;

    IDU_FIT_POINT_FATAL( "qmg::getBucketCntWithTarget::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aTarget != NULL );

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

        //------------------------------------------
        // �⺻ bucket count ����
        // bucket count = ���� graph�� ouput record count / 2
        //------------------------------------------

        if ( ( aGraph->type == QMG_SET ) &&
             ( aGraph->myQuerySet->setOp == QMS_UNION ) )
        {
            sBucketCnt = ( aGraph->left->costInfo.outputRecordCnt +
                           aGraph->right->costInfo.outputRecordCnt ) / 2.0;
        }
        else
        {
            sBucketCnt = aGraph->left->costInfo.outputRecordCnt / 2.0;
        }
        sBucketCnt = (sBucketCnt < 1) ? 1 : sBucketCnt;

        //------------------------------------------
        // target column���� cardinality ���� ����
        // ( ��, target column���� ��� ������ Į���̾�� �� )
        //------------------------------------------

        for ( sTarget = aTarget; sTarget != NULL; sTarget = sTarget->next )
        {
            sNode = sTarget->targetColumn;

            // BUG-38193 target�� pass node �� ����ؾ� �մϴ�.
            if ( sNode->node.module == &qtc::passModule )
            {
                sNode = (qtcNode*)(sNode->node.arguments);
            }
            else
            {
                // Nothing to do.
            }

            // BUG-20272
            if ( sNode->node.module == &mtfDecrypt )
            {
                sNode = (qtcNode*) sNode->node.arguments;
            }
            else
            {
                // Nothing to do.
            }

            if ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
            {
                // QMG_SET �� ���
                // validation �������� tuple �� �Ҵ�޾� target �� ���� ����
                // tablemap[table].from �� NULL �� �Ǿ�
                // one column list �� statInfo ȹ�� �Ұ�
                IDE_DASSERT( aGraph->type != QMG_SET );

                if( sNode->node.column == MTC_RID_COLUMN_ID )
                {
                    /*
                     * prowid pseudo column �� ���� ��������� ��� �ȵǾ��ִ�
                     * �׳� default ������ ó��
                     */
                    sCardinality *= QMO_STAT_COLUMN_NDV;
                }
                else
                {
                    /* target ����� ������ Į���� ��� */
                    sColCardInfo = QC_SHARED_TMPLATE(aStatement)->
                        tableMap[sNode->node.table].
                        from->tableRef->statInfo->colCardInfo;

                    sCardinality *= sColCardInfo[sNode->node.column].columnNDV;
                }
            }
            else
            {
                // target ����� �ϳ��� Į���� �ƴ� ���, �ߴ�
                sAllColumn = ID_FALSE;
                break;
            }

        }

        if ( sAllColumn == ID_TRUE )
        {
            //------------------------------------------
            // MIN( ���� graph�� outputRecordCnt / 2,
            //      Target Column���� cardinality �� )
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

        // hash bucket count ����
        if ( sBucketCnt < QCU_OPTIMIZER_BUCKET_COUNT_MIN )
        {
            sBucketCnt = QCU_OPTIMIZER_BUCKET_COUNT_MIN;
        }
        else
        {
            /* BUG-48161 */
            if ( sBucketCnt > QCG_GET_BUCKET_COUNT_MAX( aStatement ) )
            {
                sBucketCnt = QCG_GET_BUCKET_COUNT_MAX( aStatement );
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        //------------------------------------------
        // hash bucket count hint�� �����ϴ� ���
        //------------------------------------------

        sBucketCnt = aHintBucketCnt;
    }

    // BUG-36403 �÷������� BucketCnt �� �޶����� ��찡 �ֽ��ϴ�.
    sBucketCntOutput = DOUBLE_TO_UINT64( sBucketCnt );
    *aBucketCnt      = (UInt)sBucketCntOutput;
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::getBucketCnt4DistAggr( qcStatement * aStatement,
                            SDouble       aChildOutputRecordCnt,
                            UInt          aGroupBucketCnt,
                            qtcNode     * aNode,
                            UInt          aHintBucketCnt,
                            UInt        * aBucketCnt )
{
/***********************************************************************
 *
 * Description : hash bucket count�� ����
 *
 * Implementation :
 *    - hash bucket count hint�� �������� ���� ���
 *      - distinct ����� �÷��� ���
 *         sDistBucketCnt = Distinct Aggregation Column�� cardinality
 *      - �÷��� �ƴѰ��
 *        sDistBucketCnt = ���� graph�� outputRecordCnt / 2
 *
 *      sBucketCnt = MAX( ChildOutputRecordCnt / GroupBucketCnt, 1.0 )
 *      sBucketCnt = MIN( sBucketCnt, sDistBucketCnt )
 *
 *    - hash bucket count hint�� ������ ���
 *      hash bucket count = hash bucket count hint ��
 *
 ***********************************************************************/

    qmoColCardInfo * sColCardInfo;
    SDouble          sBucketCnt;
    SDouble          sDistBucketCnt;
    ULong            sBucketCntOutput;

    IDU_FIT_POINT_FATAL( "qmg::getBucketCnt4DistAggr::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );


    if ( aHintBucketCnt == QMS_NOT_DEFINED_BUCKET_CNT )
    {
        // hash bucket count hint�� �������� �ʴ� ���

        if ( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE )
        {
            sColCardInfo = QC_SHARED_TMPLATE(aStatement)->
                tableMap[aNode->node.table].
                from->tableRef->statInfo->colCardInfo;

            sDistBucketCnt = sColCardInfo[aNode->node.column].columnNDV;
        }
        else
        {
            sDistBucketCnt = IDL_MAX( aChildOutputRecordCnt / 2.0, 1.0 );
        }

        sBucketCnt = IDL_MAX( aChildOutputRecordCnt / aGroupBucketCnt, 1.0 );
        sBucketCnt = IDL_MIN( sBucketCnt, sDistBucketCnt );

        if ( sBucketCnt > QMC_MEM_HASH_MAX_BUCKET_CNT )
        {
            sBucketCnt = QMC_MEM_HASH_MAX_BUCKET_CNT;
        }
        else
        {
            // nothing to do
        }

        // BUG-36403 �÷������� BucketCnt �� �޶����� ��찡 �ֽ��ϴ�.
        sBucketCntOutput = DOUBLE_TO_UINT64( sBucketCnt );
        *aBucketCnt      = (UInt)sBucketCntOutput;
    }
    else
    {
        // bucket count hint�� �����ϴ� ���
        *aBucketCnt      = aHintBucketCnt;
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::isDiskTempTable( qmgGraph    * aGraph,
                      idBool      * aIsDisk )
{
/***********************************************************************
 *
 * Description :
 *     Graph �� ����� ���� ��ü�� �Ǵ��Ѵ�.
 *     Join Graph�� ��� ������ Child Graph�� ���ڷ� �޾ƾ� �Ѵ�.
 *
 * Implementation :
 *
 *     �Ǵܹ��
 *         - Hint �� ������ ��� �ش� Hint�� ������.
 *         - Hint�� ���� ��� �ش� Graph�� ���� ��ü�� ������.
 *
 ***********************************************************************/

    qmgGraph    * sHintGraph;
    idBool        sIsDisk = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmg::isDiskTempTable::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aIsDisk != NULL );

    // SET �� ���� ��Ʈ�� ������ �������� �ʴ� Graph�� ����
    // ��Ʈ�� ȹ���� �� �ִ� Graph�� �̵�
    for ( sHintGraph = aGraph;
          ;
          sHintGraph = sHintGraph->left )
    {
        IDE_DASSERT( sHintGraph != NULL );
        IDE_DASSERT( sHintGraph->myQuerySet != NULL );

        if( sHintGraph->myQuerySet->SFWGH != NULL )
        {
            break;
        }
    }

    //------------------------------------------
    // ���� ��ü �Ǵ�
    //------------------------------------------

    switch ( sHintGraph->myQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED:
            // Hint �� ���� ���
            // ���� Graph�� ���� ��ü�� �״�� ����Ѵ�.
            if ( ( aGraph->flag & QMG_GRAPH_TYPE_MASK )
                 == QMG_GRAPH_TYPE_MEMORY )
            {
                sIsDisk = ID_FALSE;
            }
            else
            {
                sIsDisk = ID_TRUE;
            }

            break;

        case QMO_INTER_RESULT_TYPE_MEMORY :
            // Memory Temp Table ��� Hint

            sIsDisk = ID_FALSE;
            break;

        case QMO_INTER_RESULT_TYPE_DISK :
            // Disk Temp Table ��� Hint

            sIsDisk = ID_TRUE;
            break;

        default:
            IDE_DASSERT(0);
            break;
    }

    *aIsDisk = sIsDisk;
    
    return IDE_SUCCESS;

}

// Plan Tree ������ ���� �Լ�
IDE_RC
qmg::makeColumnMtrNode( qcStatement * aStatement ,
                        qmsQuerySet * aQuerySet ,
                        qtcNode     * aSrcNode ,
                        idBool        aConverted,
                        UShort        aNewTupleID ,
                        UInt          aFlag,
                        UShort      * aColumnCount ,
                        qmcMtrNode ** aMtrNode)
{
/***********************************************************************
 *
 * Description : ��ɿ� �°� Materialize�� �÷����� �����Ѵ�
 *
 * Implementation :
 *     - srNode�� ����
 *     - �ķ��� ������ �´� identifier�� �����Ͽ� flag�� �����Ѵ�.
 *     - aStartColumnID�� ���� �����Ͽ�, �߰��� ������ �÷��� ������
 *       ���Ͽ� aColumnCount�� ����Ѵ�.
 *
 ***********************************************************************/

    qmcMtrNode        * sNewMtrNode;
    const mtfModule   * sModule;
    qtcNode           * sSrcNode;
    mtcNode           * sArgs;
    mtcTemplate       * sMtcTemplate;
    idBool              sNeedConversion;
    idBool              sNeedEvaluation;
    ULong               sFlag;

    IDU_FIT_POINT_FATAL( "qmg::makeColumnMtrNode::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSrcNode   != NULL );

    //----------------------------------
    // Source ��� ����
    //----------------------------------

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement), qmcMtrNode, &sNewMtrNode)
              != IDE_SUCCESS);

    // Node�� �����Ͽ� ����� node�� srcNode�� �����Ѵ�.
    IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sSrcNode)
              != IDE_SUCCESS);

    idlOS::memcpy( sSrcNode,
                   aSrcNode,
                   ID_SIZEOF( qtcNode ) );

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sSrcNode,
                                       aNewTupleID,
                                       ID_FALSE )
              != IDE_SUCCESS );

    sFlag = sMtcTemplate->rows[sSrcNode->node.table].lflag;

    if( ( QTC_IS_AGGREGATE( aSrcNode ) == ID_TRUE ) &&
        ( ( sFlag & MTC_TUPLE_PLAN_MASK ) == MTC_TUPLE_PLAN_TRUE ) )
    {
        // PROJ-2179
        // Aggregate function�� tuple-set ������ �������� ������ �����ϴµ�,
        // ���� ���� ���Ŀ��� ���� ����� ����ִ� 1�� �������� �����ص� �ȴ�.
        // ���� value/column module�� �����Ͽ� ������ ���δ�.
        aSrcNode->node.module = &qtc::valueModule;
        sSrcNode->node.module = &qtc::valueModule;
    }
    else
    {
        // Nothing to do.
    }

    // source node ����
    sNewMtrNode->srcNode = sSrcNode;

    // To Fix PR-10182
    // GROUP BY, SUM() ��� PRIOR Column�� ������ �� �ִ�.
    IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                       aQuerySet ,
                                       sNewMtrNode->srcNode )
              != IDE_SUCCESS );

    //----------------------------------
    // To Fix PR-12093
    //     - Destine Node�� ��� ���� �籸���� ���ؼ���
    //     - Mtr Node�� flag ���� ���� ���� ���� �̷������ �Ѵ�.
    // Destine ��� ����
    //----------------------------------

    //dstNode�� ����
    IDE_TEST( qtc::makeInternalColumn( aStatement,
                                       aNewTupleID,
                                       *aColumnCount,
                                       &(sNewMtrNode->dstNode) )
              != IDE_SUCCESS);

    // To Fix PR-9208
    // Destine Node�� ��κ��� Source Node ������ �����Ͽ��� �Ѵ�.
    // �Ʒ��� ���� �� ���� �������� ���� ���, flag �������� ������� �ȴ�.

    // sNewMtrNode->dstNode->node.arguments
    //     = sNewMtrNode->srcNode->node.arguments;
    // sNewMtrNode->dstNode->subquery
    //     = sNewMtrNode->srcNode->subquery;

    idlOS::memcpy( sNewMtrNode->dstNode,
                   sNewMtrNode->srcNode,
                   ID_SIZEOF(qtcNode) );

    // To Fix PR-9208
    // Destine Node�� �ʿ��� �������� �缳���Ѵ�.
    sNewMtrNode->dstNode->node.table = aNewTupleID;
    sNewMtrNode->dstNode->node.column = *aColumnCount;

    // To Fix PR-9208
    // Destine Node���� ���ʿ��� ������ �ʱ�ȭ�Ѵ�.

    sNewMtrNode->dstNode->node.conversion = NULL;
    sNewMtrNode->dstNode->node.leftConversion = NULL;
    sNewMtrNode->dstNode->node.next = NULL;

    // To Fix PR-9208
    // Desinte Node�� �׻� ���� ���� ������.
    // ����, Indirection�� �� ����.
    sNewMtrNode->dstNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
    sNewMtrNode->dstNode->node.lflag |= MTC_NODE_INDIRECT_FALSE;

    //----------------------------------
    // flag ����
    //----------------------------------

    sNewMtrNode->flag = 0;

    // Column locate ������ �⺻���̴�.
    sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
    sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

    // Evaluation�� �ʿ��� expression���� Ȯ��
    // (expression�̶� ������ view�� �ٸ� operator�� tuple�� ����� �����Ѵٸ�
    //  ������ �̹� evaluation�� �� ���̹Ƿ� �ٽ� evaluation���� �ʴ´�.)
    if( ( ( sFlag & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_INTERMEDIATE ) &&
        ( ( sFlag & MTC_TUPLE_PLAN_MASK ) == MTC_TUPLE_PLAN_FALSE ) &&
        ( ( sFlag & MTC_TUPLE_VIEW_MASK ) == MTC_TUPLE_VIEW_FALSE ) &&
        ( QMC_NEED_CALC( aSrcNode ) == ID_TRUE ) )
    {
        sNeedEvaluation = ID_TRUE;
    }
    else
    {
        sNeedEvaluation = ID_FALSE;
    }

    // Conversion�� �ʿ����� Ȯ��
    if( ( aConverted == ID_TRUE ) &&
        ( aSrcNode->node.conversion != NULL ) )
    {
        sNeedConversion = ID_TRUE;
    }
    else
    {
        sNeedConversion = ID_FALSE;
        sSrcNode->node.conversion = NULL;
    }

    // ���� ������ �Ѱ����� �ش��ϸ� �ݵ�� calculate�Լ��� �����Ѵ�.
    // 1. Evaluation�� �ʿ��� expression�� ���
    // 2. Conversion�� �ʿ��� ���
    if( ( sNeedEvaluation == ID_TRUE ) ||
        ( sNeedConversion == ID_TRUE ) )
    {
        if( ( aSrcNode->node.lflag & MTC_NODE_INDIRECT_MASK ) == MTC_NODE_INDIRECT_FALSE )
        {
            // Expression�� ��� calculate�Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE;

            aSrcNode->node.lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
            aSrcNode->node.lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE;
        }
        else
        {
            // Calculate �� copy�� �ؾ��ϴ� ���
            // (Pass node, subquery ���� ����� stack���κ��� ����)
            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
            sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE;

            if( aSrcNode->node.module == &qtc::subqueryModule )
            {
                // Subquery�� ����� materialize�� ���ĺ��ʹ� �Ϲ� column�� �����ϰ� �����Ѵ�.
                aSrcNode->node.module = &qtc::valueModule;
                aSrcNode->node.lflag &= ~MTC_NODE_INDIRECT_MASK;
                aSrcNode->node.lflag |= MTC_NODE_INDIRECT_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        switch ( sFlag & MTC_TUPLE_TYPE_MASK )
        {
            case MTC_TUPLE_TYPE_CONSTANT:
            case MTC_TUPLE_TYPE_VARIABLE:
            case MTC_TUPLE_TYPE_INTERMEDIATE:
                if( ( sFlag & MTC_TUPLE_PLAN_MTR_MASK ) == MTC_TUPLE_PLAN_MTR_FALSE )
                {
                    // Temp table�� �ƴ� ��� ������ �����Ѵ�.
                    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                    sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

                    // BUG-28212 ����
                    if( isDatePseudoColumn( aStatement, sSrcNode ) == ID_TRUE )
                    {
                        // SYSDATE���� pseudo column�� materialize�Ǵ��� ��ġ�� �������� �ʴ´�.
                        sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
                        sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    break;
                }
                else
                {
                    // Nothing to do.
                    // Temp table�� ��� table�� ������ ���� materialize ����� �����Ѵ�.
                }
                /* fall through */
            case MTC_TUPLE_TYPE_TABLE:
                /* PROJ-2464 hybrid partitioned table ���� */
                if ( ( sFlag & MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK ) == MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                    sNewMtrNode->flag |= QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN;
                }
                else
                {
                    if ( ( sFlag & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_MEMORY )
                    {
                        // Src�� memory table�� ��
                        if ( ( ( sFlag & MTC_TUPLE_PARTITIONED_TABLE_MASK )
                               == MTC_TUPLE_PARTITIONED_TABLE_TRUE ) ||
                             ( ( sFlag & MTC_TUPLE_PARTITION_MASK )
                               == MTC_TUPLE_PARTITION_TRUE ) )
                        {
                            // BUG-39896
                            // key column�� partitioned table�� �÷��� ���
                            // row pointer�Ӹ��ƴ϶� column�������� �ʿ��ϱ� ������
                            // ������ mtrNode�� �����Ͽ� ó���Ѵ�.
                            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                            sNewMtrNode->flag |= QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN;
                        }
                        else
                        {
                            sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                            sNewMtrNode->flag |= QMC_MTR_TYPE_MEMORY_KEY_COLUMN;
                        }

                        if ( ( sMtcTemplate->rows[sNewMtrNode->dstNode->node.table].lflag & MTC_TUPLE_STORAGE_MASK )
                             == MTC_TUPLE_STORAGE_MEMORY )
                        {
                            // Dst�� memory table�� ���, pointer�� �����Ѵ�.
                            sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
                            sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE;
                        }
                        else
                        {
                            // Dst�� disk table�� ���, ������ �����Ѵ�.
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        // Src�� disk table�� ���, ������ �����Ѵ�.
                        sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                        sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

                        if ( ( aFlag & QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_MASK )
                             == QMG_MAKECOLUMN_MTR_NODE_NOT_CHANGE_COLUMN_TRUE )
                        {
                            sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
                            sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }
                }
                break;
        }
    }

    sNewMtrNode->next = NULL;
    sNewMtrNode->myDist = NULL;
    sNewMtrNode->bucketCnt = 0;

    // To Fix PR-12093
    // Destine Node�� ����Ͽ� mtcColumgn�� Count�� ���ϴ� ���� ��Ģ�� ����
    // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
    //     - Memory ���� ����
    //     - offset ���� ���� (PR-12093)
    sModule = sNewMtrNode->dstNode->node.module;
    (*aColumnCount) += ( sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK );

    // Argument���� ��ġ�� �� �̻� ������� �ʵ��� �����Ѵ�.
    for( sArgs = aSrcNode->node.arguments;
         sArgs != NULL;
         sArgs = sArgs->next )
    {
        // BUG-37355
        // argument node tree�� �����ϴ� passNode�� qtcNode�� �����ϴ� ���
        // flag�� �����ϴ��� column locate�� ����ǹǷ�
        // �̸� ��������Ͽ� ������Ų��.
        IDE_TEST( isolatePassNode( aStatement, (qtcNode*) sArgs )
                  != IDE_SUCCESS );
        
        sArgs->lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
        sArgs->lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE;
    }

    if( ( sNewMtrNode->flag & QMC_MTR_CHANGE_COLUMN_LOCATE_MASK )
        == QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE )
    {
        // �������� temp table�� ���� �����ϵ��� ����� ��ġ�� �����Ѵ�.
        aSrcNode->node.table  = sNewMtrNode->dstNode->node.table;
        aSrcNode->node.column = sNewMtrNode->dstNode->node.column;

        if( ( aSrcNode->node.conversion != NULL ) || ( sSrcNode->node.conversion != NULL ) )
        {
            // Conversion�� ����� materialize�ϴ� ��� ��ġ ������ ����Ѵ�.
            // ���� qmg::chagneColumnLocate ȣ�� �� conversion���� ���� column �����ÿ���
            // conversion�� ����� ������ �� �ֱ� �����̴�.
            // Ex) SELECT /*+USE_SORT(t1, t2)*/ * FROM t1, t2 WHERE t1.c1 = t2.c2;
            //     * �� �� t1.c1�� t2.c1�� type�� �޶� conversion�� �߻��ϴ� ���
            //       PROJECTION������ SORT�� t1.c1�̳� t2.c2�� �ƴ� SCAN�� �����
            //       �����ؾ� �Ѵ�.
            sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
            sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_FALSE;
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

    // PROJ-2362 memory temp ���� ȿ���� ����
    // aggregation�� �ƴϰ�, variable type�̸�, memory temp�� ����ϴ� ���
    if( ( QTC_IS_AGGREGATE( sNewMtrNode->srcNode ) != ID_TRUE )
        &&
        ( ( ( sFlag & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_TABLE ) ||
          ( ( sFlag & MTC_TUPLE_VIEW_MASK ) == MTC_TUPLE_VIEW_TRUE )  ||
          ( isTempTable( sFlag ) == ID_TRUE ) )
        &&
        ( ( QTC_STMT_TUPLE( aStatement, sNewMtrNode->dstNode )->lflag
            & MTC_TUPLE_STORAGE_MASK )
          == MTC_TUPLE_STORAGE_MEMORY )
        &&
        ( QCU_REDUCE_TEMP_MEMORY_ENABLE == 1 ) )
    {
        if ( QTC_STMT_TUPLE( aStatement, sNewMtrNode->srcNode )->columns != NULL )
        {
            if ( QTC_STMT_COLUMN( aStatement, sNewMtrNode->srcNode )->module->id
                 != MTD_LIST_ID )
            {
                sNewMtrNode->flag &= ~QMC_MTR_TEMP_VAR_TYPE_ENABLE_MASK;
                sNewMtrNode->flag |= QMC_MTR_TEMP_VAR_TYPE_ENABLE_TRUE;
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
    else
    {
        // Nothing to do.
    }
    
    // BUG-34341
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_USE_TEMP_TYPE );

    /* PROJ-2462 Result Cache */
    if ( ( aSrcNode->lflag & QTC_NODE_LOB_COLUMN_MASK )
         == QTC_NODE_LOB_COLUMN_EXIST )
    {
        sNewMtrNode->flag &= ~QMC_MTR_LOB_EXIST_MASK;
        sNewMtrNode->flag |= QMC_MTR_LOB_EXIST_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2462 Result Cache */
    if ( ( aSrcNode->lflag & QTC_NODE_PRIOR_MASK )
         == QTC_NODE_PRIOR_EXIST )
    {
        sNewMtrNode->flag &= ~QMC_MTR_PRIOR_EXIST_MASK;
        sNewMtrNode->flag |= QMC_MTR_PRIOR_EXIST_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    *aMtrNode = sNewMtrNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::makeBaseTableMtrNode( qcStatement * aStatement ,
                           qtcNode     * aSrcNode ,
                           UShort        aDstTupleID ,
                           UShort      * aColumnCount ,
                           qmcMtrNode ** aMtrNode )
{
    qmcMtrNode  * sNewMtrNode;
    ULong         sFlag;
    idBool        sIsRecoverRid = ID_FALSE;
    UShort        sSrcTupleID   = 0;

    IDU_FIT_POINT_FATAL( "qmg::makeBaseTableMtrNode::__FT__" );

    sSrcTupleID = aSrcNode->node.table;
    sFlag = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sSrcTupleID].lflag;

    if( ( sFlag & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_TABLE )
    {
        // PROJ-1789 PROWID
        // need to recover rid when setTuple
        if( (sFlag & MTC_TUPLE_TARGET_RID_MASK) ==
            MTC_TUPLE_TARGET_RID_EXIST)
        {
            sIsRecoverRid = ID_TRUE;
        }
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement),
                           qmcMtrNode,
                           &sNewMtrNode) != IDE_SUCCESS);

    IDE_TEST( qtc::makeInternalColumn( aStatement,
                                       sSrcTupleID,
                                       0,
                                       &(sNewMtrNode->srcNode) )
              != IDE_SUCCESS );

    sNewMtrNode->flag = 0;
    sNewMtrNode->next = NULL;
    sNewMtrNode->myDist = NULL;
    sNewMtrNode->bucketCnt = 0;

    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
    sNewMtrNode->flag |= (getBaseTableType( sFlag ) & QMC_MTR_TYPE_MASK);

    // PROJ-2362 memory temp ���� ȿ���� ����
    // baseTable�� ���� mtrNode���� ����Ѵ�.
    sNewMtrNode->flag &= ~QMC_MTR_BASETABLE_MASK;
    sNewMtrNode->flag |= QMC_MTR_BASETABLE_TRUE;
    
    if( isTempTable( sFlag ) == ID_TRUE )
    {
        sNewMtrNode->flag &= ~QMC_MTR_BASETABLE_TYPE_MASK;
        sNewMtrNode->flag |= QMC_MTR_BASETABLE_TYPE_DISKTEMPTABLE;
    }
    else
    {
        sNewMtrNode->flag &= ~QMC_MTR_BASETABLE_TYPE_MASK;
        sNewMtrNode->flag |= QMC_MTR_BASETABLE_TYPE_DISKTABLE;
    }

    /* BUG-39830 */
    if ((sFlag & MTC_TUPLE_STORAGE_MASK) == MTC_TUPLE_STORAGE_LOCATION_REMOTE)
    {
        sNewMtrNode->flag &= ~QMC_MTR_REMOTE_TABLE_MASK;
        sNewMtrNode->flag |= QMC_MTR_REMOTE_TABLE_TRUE;
    }
    else
    {
        sNewMtrNode->flag &= ~QMC_MTR_REMOTE_TABLE_MASK;
        sNewMtrNode->flag |= QMC_MTR_REMOTE_TABLE_FALSE;
    }

    IDE_TEST( qtc::makeInternalColumn( aStatement,
                                       aDstTupleID,
                                       *aColumnCount,
                                       &(sNewMtrNode->dstNode) )
              != IDE_SUCCESS );

    (*aColumnCount)++;

    if (sIsRecoverRid == ID_TRUE)
    {
        /*
         * PROJ-1789 PROWID
         *
         * SELECT _PROWID FROM T1 ORDER BY c1;
         * Pointer or RID ����̰� select target �� RID �� �ִ� ���
         * ���߿� setTupleXX �������� rid ���� �����ϵ���
         *
         * ��ǻ� recover �� �ʿ��� ���� memory table ����
         * base table ptr �� �����Ҷ��̴�.
         * �̶� �׻� alCoccount = 1 �̹Ƿ� sFirstMtrNode ���� ó���Ͽ���.
         */
        sNewMtrNode->flag &= ~QMC_MTR_RECOVER_RID_MASK;
        sNewMtrNode->flag |= QMC_MTR_RECOVER_RID_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2462 Result Cache */
    if ( ( aSrcNode->lflag & QTC_NODE_LOB_COLUMN_MASK )
         == QTC_NODE_LOB_COLUMN_EXIST )
    {
        sNewMtrNode->flag &= ~QMC_MTR_LOB_EXIST_MASK;
        sNewMtrNode->flag |= QMC_MTR_LOB_EXIST_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    *aMtrNode = sNewMtrNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::setDisplayInfo( qmsFrom          * aFrom ,
                     qmsNamePosition  * aTableOwnerName ,
                     qmsNamePosition  * aTableName ,
                     qmsNamePosition  * aAliasName )
{
/***********************************************************************
 *
 * Description : display ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcNamePosition     sTableOwnerNamePos;
    qcNamePosition     sTableNamePos;
    qcNamePosition     sAliasNamePos;

    IDU_FIT_POINT_FATAL( "qmg::setDisplayInfo::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aFrom != NULL );

    sTableOwnerNamePos = aFrom->tableRef->userName;
    sTableNamePos = aFrom->tableRef->tableName;
    sAliasNamePos = aFrom->tableRef->aliasName;

    //table owner name ����
    if ( ( aFrom->tableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
         ( aFrom->tableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
         ( aFrom->tableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) )
    {
        // performance view�� tableInfo�� ����.
        aTableOwnerName->name = NULL;
        aTableOwnerName->size = QC_POS_EMPTY_SIZE;
    }
    else
    {
        aTableOwnerName->name = sTableOwnerNamePos.stmtText + sTableOwnerNamePos.offset;
        aTableOwnerName->size = sTableOwnerNamePos.size;
    }

    if ( ( sTableNamePos.stmtText != NULL ) && ( sTableNamePos.size > 0 ) )
    {
        //table name ����
        aTableName->name = sTableNamePos.stmtText + sTableNamePos.offset;
        aTableName->size = sTableNamePos.size;
    }
    else
    {
        aTableName->name = NULL;
        aTableName->size = QC_POS_EMPTY_SIZE;
    }


    //table name �� alias name�� ���� ���
    if( sTableNamePos.offset == sAliasNamePos.offset )
    {
        aAliasName->name = NULL;
        aAliasName->size = QC_POS_EMPTY_SIZE;
    }
    else
    {
        aAliasName->name = sAliasNamePos.stmtText + sAliasNamePos.offset;
        aAliasName->size = sAliasNamePos.size;
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::copyMtcColumnExecute( qcStatement      * aStatement ,
                           qmcMtrNode       * aMtrNode )
{
/***********************************************************************
 *
 * Description : mtcColumn������ Execute������ �����Ѵ�.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcMtrNode       * sMtrNode;
    mtcNode          * sConvertedNode;

    UShort             sTable1;
    UShort             sColumn1;
    UShort             sTable2;
    UShort             sColumn2;

    UShort             sColumnCount;
    UShort             i;

    IDU_FIT_POINT_FATAL( "qmg::copyMtcColumnExecute::__FT__" );
    
    for( sMtrNode = aMtrNode ; sMtrNode != NULL ; sMtrNode = sMtrNode->next )
    {
        // To Fix PR-12093
        // Destine Node�� ����Ͽ� mtcColumn�� Count�� ���ϴ� ���� ��Ģ�� ����
        // ��������� ���� mtcColumn ������ �����ϴ� ���� ���ո���.
        //     - Memory ���� ����
        //     - offset ���� ���� (PR-12093)

        sColumnCount = sMtrNode->dstNode->node.module->lflag &
            MTC_NODE_COLUMN_COUNT_MASK;

        // fix BUG-20659
        if( ( sMtrNode->srcNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_AGGREGATION )
        {
            sConvertedNode = (mtcNode*)&(sMtrNode->srcNode->node);
        }
        else
        {
            sConvertedNode =
                mtf::convertedNode( &( sMtrNode->srcNode->node ),
                                    &( QC_SHARED_TMPLATE(aStatement)->tmplate ) );
        }

        for ( i = 0; i < sColumnCount ; i++ )
        {
            sTable1 = sMtrNode->dstNode->node.table;
            sColumn1 = sMtrNode->dstNode->node.column + i;

            //BUG-8785
            //Aggregation�� ó�� result���� conversion�� �޸����ִ�.
            if( i == 0 )
            {
                sTable2 = sConvertedNode->table;
                sColumn2 = sConvertedNode->column;
            }
            else
            {
                sTable2 = sMtrNode->srcNode->node.table;
                sColumn2 = sMtrNode->srcNode->node.column + i;
            }

            if (sColumn2 != MTC_RID_COLUMN_ID)
            {
                idlOS::memcpy(&(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable1].columns[sColumn1]) ,
                              &(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable2].columns[sColumn2]) ,
                              ID_SIZEOF(mtcColumn));

                idlOS::memcpy(&(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable1].execute[sColumn1]) ,
                              &(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable2].execute[sColumn2]) ,
                              ID_SIZEOF(mtcExecute));

                /* BUG-44047
                 * CLOB�� ������ ���̺�� HASH ���� ��Ʈ�� ����ϸ�, ������ �߻�
                 * Intermediate Tuple���� Pointer �� ���� ��쵵 ù ��° Table��
                 * Column Size ��ŭ �Ҵ��ϵ��� �Ǿ��ִ�.
                 * �� ���״� �ϴ� CLob �� ��츸 column size�� �����Ѵ�.
                 * ���� partition pointer size�� 26 ���� mtdBigintType * 5 = 40
                 * ������ ����ϸ��� ����. execute�� �ٽ� ������ �Ͼ�Ƿ� ��
                 * ���� �ʿ�� ����.
                 */
                if ( (( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable1].columns[sColumn1].module == &mtdClob ) ||
                      ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable1].columns[sColumn1].module == &mtdBlob )) &&
                     ( ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_MEMORY_TABLE ) ||
                       ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_DISK_TABLE ) ||
                       ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE ) ||
                       ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_DISK_PARTITIONED_TABLE ) ||
                       ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE ) ) )
                {
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable1].
                        columns[sColumn1].column.size = ID_SIZEOF(mtdBigintType) * 5;

                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                idlOS::memcpy(&(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable1].columns[sColumn1]),
                              &gQtcRidColumn,
                              ID_SIZEOF(mtcColumn));

                idlOS::memcpy(&(QC_SHARED_TMPLATE(aStatement)->
                                tmplate.rows[sTable1].execute[sColumn1]),
                              &gQtcRidExecute,
                              ID_SIZEOF(mtcExecute));
            }

            //--------------------------------------------------------
            // BUG-21020
            // Ex)  SELECT DISTINCT A.S1,
            //            (SELECT S1(Variable Column) FROM T2) S4,
            //            A.S2,
            //            A.S3
            //      FROM T1 A
            //      ORDER BY A.S1;
            // ���� ���� SubQuery�ȿ� Variable Column�� ���� ���, Fixed�� ������ ����Ѵ�.
            // SubQuery�� �ƴ� �ٸ� Column���� �������� ó�������� Subquery�� ���� ó���� ����.
            // SubQuery���� Column�� Variable �� ���, Fixed�� �����ϴ� ó���� ����.
            // ���� �� ó���� ���ٸ�, SORT�� Fixed�� ����� Column�� Variable Column����
            // �νĵǰ� ���� ������ ���Ḧ �߱��Ѵ�.
            // �� �ڵ尡 ������ ������ Test�� �ݵ�� 4���� ������ ��� �����ϵ��� �Ͽ��� �Ѵ�.
            // ù°, Memory Variable Column�� Memory Temptable�� ����� ��,
            // ��°, Memory Variable Column�� Disk Temptable�� ����� ��,
            // ��°, Disk Variable Column�� Memory Temptable�� ����� ��,
            // ��°, Disk Variable column�� Disk Temptable�� ����ɶ�.
            // Subquery�� ���� FIXED ���� �ڵ尡 ���� ���, ��°�� ��° ���ǿ��� ������ �����Ѵ�.
            //--------------------------------------------------------
            if ( QTC_IS_SUBQUERY( sMtrNode->srcNode ) == ID_TRUE )
            {
                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag &=
                    ~SMI_COLUMN_TYPE_MASK;

                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag |=
                    SMI_COLUMN_TYPE_FIXED;        

                // BUG-38494
                // Compressed Column ���� �� ��ü�� ����ǹǷ�
                // Compressed �Ӽ��� �����Ѵ�
                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag &=
                    ~SMI_COLUMN_COMPRESSION_MASK;

                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag |=
                    SMI_COLUMN_COMPRESSION_FALSE;        
            }

            // fix BUG-9494
            // memory variable column�� disk temp table�� ����ɶ���
            // pointer�� �ƴ� ���� ����ȴ�.
            // ����, �� ����� smiColumn.flag�� variable���� fixed��
            // ��������־�� �Ѵ�.
            // ex) SELECT /*+ TEMP_TBS_DISK */ DISTINCT *
            //     FROM M1 ORDER BY M1.I2(variable column);
            // �Ʒ��� ���� HSDS���ʹ� disk temp table�� ����Ǹ�
            // variable column�� pointer�� �ƴ� ���� ����ȴ�.
            // �̶�, HSDS���� �� variable column�� fixed�� �������� ������,
            // SORT������ disk variable column���� �ν��ؼ�
            // disk variable column header�� ���� ���� ã������ �õ��ϰ� �ǹǷ�
            // ������ �����������ϰ� ��.
            // HSDS���� variable column�� fixed�� ���������ν�,
            // SORT������ disk fixed column���� �ν��ؼ�,
            // ����� ���� �����ϰ� �Ǿ�, �ùٸ� ���ǰ���� �����ϰ� ��.
            //     [PROJ]
            //       |
            //     [SORT] -- disk temp table
            //       |
            //     [HSDS] --  disk temp table
            //       |
            //     [SCAN] -- memory table
            //     
            
            if( ( ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK )
                    == QMC_MTR_TYPE_MEMORY_KEY_COLUMN ) ||
                  ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK )
                    == QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN ) ||
                  ( ( sMtrNode->flag & QMC_MTR_TYPE_MASK )
                    == QMC_MTR_TYPE_HYBRID_PARTITION_KEY_COLUMN ) ) /* PROJ-2464 hybrid partitioned table ���� */
                &&
                ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTable1].lflag
                    & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_DISK ) )
            {
                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag &=
                    ~SMI_COLUMN_TYPE_MASK;

                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag |=
                    SMI_COLUMN_TYPE_FIXED;

                // BUG-38494
                // Compressed Column ���� �� ��ü�� ����ǹǷ�
                // Compressed �Ӽ��� �����Ѵ�
                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag &=
                    ~SMI_COLUMN_COMPRESSION_MASK;

                QC_SHARED_TMPLATE(aStatement)->tmplate.
                    rows[sTable1].columns[sColumn1].column.flag |=
                    SMI_COLUMN_COMPRESSION_FALSE;        
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::setCalcLocate( qcStatement * aStatement,
                    qmcMtrNode  * aMtrNode )
{
/***********************************************************************
 *
 * Description :
 *    Calculation������ materialization�� �Ϸ�Ǵ� node�鿡 ����
 *    src node�� ���� ��ġ�� dest node�� �������ش�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcMtrNode * sMtrNode;
    mtcNode    * sConverted;

    IDU_FIT_POINT_FATAL( "qmg::setCalcLocate::__FT__" );

    for (sMtrNode = aMtrNode;
         sMtrNode != NULL;
         sMtrNode = sMtrNode->next )
    {
        if( ( sMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_CALCULATE )
        {
            sConverted = mtf::convertedNode( &sMtrNode->srcNode->node,
                                             &QC_SHARED_TMPLATE( aStatement )->tmplate );
            sConverted->table  = sMtrNode->dstNode->node.table;
            sConverted->column = sMtrNode->dstNode->node.column;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::checkOrderInGraph( qcStatement        * aStatement,
                        qmgGraph           * aGraph,
                        qmgPreservedOrder  * aWantOrder,
                        qmoAccessMethod   ** aOriginalMethod,
                        qmoAccessMethod   ** aSelectMethod,
                        idBool             * aUsable )
{
/***********************************************************************
 *
 * Description :
 *
 *    �Էµ� Order�� �ش� Graph������ ��� ������ ���� �˻�
 *
 * Implementation :
 *
 *    Graph�� Preserved Order�� ���ǵ��� ���� ���
 *        - �ش� Graph�� ������ ���� ó��
 *            - �Ϲ� Table�� Selection Graph�� ���
 *                - Index ��� ���� ���� �˻�
 *            - View�� Selection Graph�� ���
 *                - Child Graph�� ���� Order ���·� ���� ��
 *                - Child Graph�� �̿��� ó��
 *            - Set, Hierarchy, Dnf Graph�� ���
 *                - ó���� �� ������,
 *                - Preserved Order�� ���� ������ ������.
 *            - �� ���� Graph�� ���
 *                - Child Graph�� �̿��� ó��
 *    Graph�� Preserved Order�� ������ �ִ� ���
 *        - ���ϴ� Preserved Order�� Graph Preserved Order�� �˻�.
 *    Graph�� NEVER Defined�� ���
 *        - ���ϴ� Order�� ó���� �� ����.
 *
 ***********************************************************************/

    idBool            sUsable = ID_FALSE;
    idBool            sOrderImportant;
    qmoAccessMethod * sOrgMethod;
    qmoAccessMethod * sSelMethod;

    qmgDirectionType  sPrevDirection;
    qmsParseTree    * sParseTree;

    qmgPreservedOrder * sWantOrder;
    qmgPreservedOrder * sPreservedOrder;

    UInt              i;
    UInt              sOrderCnt;
    UInt              sDummy;
    UInt              sJoinMethod;

    IDU_FIT_POINT_FATAL( "qmg::checkOrderInGraph::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aWantOrder != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( aUsable != NULL );
    IDE_FT_ASSERT( aOriginalMethod != NULL );
    IDE_FT_ASSERT( aSelectMethod != NULL );

    sOrgMethod = NULL;
    sSelMethod = NULL;

    // Order�� �߿������� �Ǵ�.
    // BUG-40361 supporting to indexable analytic function
    // aWantOrder�� ��� Node�� NOT DEFINED ���� Ȯ���Ѵ�
    sOrderImportant = ID_FALSE;
    for ( sWantOrder = aWantOrder; sWantOrder != NULL; sWantOrder = sWantOrder->next )
    {
        if ( sWantOrder->direction != QMG_DIRECTION_NOT_DEFINED )
        {
            sOrderImportant = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    //---------------------------------------------------
    // Graph�� ������ ���� �˻�
    //---------------------------------------------------

    // Graph�� ���� Preserved Order�� ���� �˻�
    switch ( aGraph->flag & QMG_PRESERVED_ORDER_MASK )
    {
        case QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED :
            //---------------------------------------------------
            // Graph�� Preserved Order�� �����ϳ� �������� ���� ���
            // ���ϴ� order�� ���� ������ ���
            //---------------------------------------------------

            if ( aGraph->type == QMG_WINDOWING )
            {
                IDE_TEST( qmgWindowing::existWantOrderInSortingKey( aGraph,
                                                                    aWantOrder,
                                                                    & sUsable,
                                                                    & sDummy )
                          != IDE_SUCCESS );
            }
            else
            {
                // Child Graph�κ��� Order�� ��� �������� �˻�.
                IDE_TEST(
                    checkUsableOrder( aStatement,
                                      aWantOrder,
                                      aGraph->left,
                                      & sOrgMethod,
                                      & sSelMethod,
                                      & sUsable )
                    != IDE_SUCCESS );
            }
            break;
            
        case QMG_PRESERVED_ORDER_NOT_DEFINED :

            //---------------------------------------------------
            // Graph�� Preserved Order�� ���ǵ��� �ʴ� ���
            //---------------------------------------------------

            // Graph ������ ���� �Ǵ�
            switch ( aGraph->type )
            {
                case QMG_SELECTION :
                    if ( aGraph->myFrom->tableRef->view == NULL )
                    {
                        // �Ϲ� Table�� ���� Selection�� ���

                        // ���ϴ� Order�� �����ϴ� Index�� �����ϴ� �� �˻�
                        IDE_TEST( checkUsableIndex4Selection( aGraph,
                                                              aWantOrder,
                                                              sOrderImportant,
                                                              & sOrgMethod,
                                                              & sSelMethod,
                                                              & sUsable )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // View �� ���� Selection�� ���
                        // Ex) SELECT * FROM ( SELECT i1, i2 FROM T1 ) V1
                        //     ORDER BY V1.i1;

                        sParseTree = (qmsParseTree *)
                            aGraph->myFrom->tableRef->view->myPlan->parseTree;

                        // ���ϴ� Order�� ID��
                        // View�� Target�� �����ϴ� ID�� ����
                        IDE_TEST(
                            refineID4Target( aWantOrder,
                                             sParseTree->querySet->target )
                            != IDE_SUCCESS );

                        // Child Graph�κ��� Order�� ��� �������� �˻�.
                        IDE_TEST(
                            checkUsableOrder( aGraph->myFrom->tableRef->view,
                                              aWantOrder,
                                              aGraph->left,
                                              & sOrgMethod,
                                              & sSelMethod,
                                              & sUsable )
                            != IDE_SUCCESS );
                    }

                    break;
                case QMG_PARTITION :
                    // ���ϴ� Order�� �����ϴ� Index�� �����ϴ� �� �˻�
                    IDE_TEST( checkUsableIndex4Partition( aGraph,
                                                          aWantOrder,
                                                          sOrderImportant,
                                                          & sOrgMethod,
                                                          & sSelMethod,
                                                          & sUsable )
                              != IDE_SUCCESS );
                    break;

                case QMG_WINDOWING :
                    // BUG-35001
                    // disk temp table �� ����ϴ� ��쿡��
                    // insert ������ fetch �ϴ� ������ ���� �ٸ��� �ִ�.
                    if( ( aGraph->flag & QMG_GRAPH_TYPE_MASK )
                        == QMG_GRAPH_TYPE_MEMORY )
                    {
                        // Child Graph�κ��� Order�� ��� �������� �˻�.
                        IDE_TEST(
                            checkUsableOrder( aStatement,
                                              aWantOrder,
                                              aGraph->left,
                                              & sOrgMethod,
                                              & sSelMethod,
                                              & sUsable )
                            != IDE_SUCCESS );
                    }
                    else
                    {
                        // nothing todo.
                    }
                    break;

                case QMG_SET :
                case QMG_HIERARCHY :
                case QMG_DNF :
                case QMG_SHARD_SELECT:    // PROJ-2638

                    // Set�� ��쿡�� Index�� ����Ѵ� �ϴ���
                    // �ùٸ� ����� ������ �� ����.
                    // Ex) SELECT * FROM ( SELECT i1, i2 FROM T1
                    //                     UNOIN ALL
                    //                     SELECT i1, i2 FROM T2 ) V1
                    //     ORDER BY V1.i1;
                    // Ex) SELECT DISTINCT V1.i1
                    //       FROM ( SELECT i1, i2 FROM T1
                    //              UNOIN ALL
                    //              SELECT i1, i2 FROM T2 ) V1;

                    // NOT_DEFINED�� �� ����.
                    IDE_DASSERT(0);

                    sUsable = ID_FALSE;

                    break;

                default :

                    // Child Graph�κ��� Order�� ��� �������� �˻�.
                    IDE_TEST(
                        checkUsableOrder( aStatement,
                                          aWantOrder,
                                          aGraph->left,
                                          & sOrgMethod,
                                          & sSelMethod,
                                          & sUsable )
                        != IDE_SUCCESS );
                    break;
            }

            break;

        case QMG_PRESERVED_ORDER_DEFINED_FIXED :

            //---------------------------------------------------
            // Graph�� Preserved Order�� ������ ���
            //---------------------------------------------------

            IDE_DASSERT( aGraph->preservedOrder != NULL );

            if ( sOrderImportant == ID_TRUE )
            {
                //------------------------------------
                // Order�� �߿��� ���
                //------------------------------------

                // Preserved Order�� Order�� ���� �� ������ ��ġ�ؾ� ��
                // Ex) Preserved Order i1(asc) -> i2(asc) -> i3(asc)
                //     ORDER BY i1, i2 :  O
                //     ORDER BY i1, i2, i3 desc : X
                //     ORDER BY i3, i2, i1 :  X
                //     ORDER BY i1, i3 : X
                //     ORDER BY i2, i3 : X
                //     ORDER BY i1, i2, i3, i4 : X

                sPrevDirection = QMG_DIRECTION_NOT_DEFINED;

                for ( sWantOrder = aWantOrder,
                          sPreservedOrder = aGraph->preservedOrder;
                      sWantOrder != NULL && sPreservedOrder != NULL;
                      sWantOrder = sWantOrder->next,
                          sPreservedOrder = sPreservedOrder->next )
                {
                    if ( sWantOrder->table == sPreservedOrder->table &&
                         sWantOrder->column == sPreservedOrder->column )
                    {
                        // ������ Column�� ���� Preserved Order��
                        /* BUG-42145
                         * Table�̳� Partition�� Index�� �ִ� �����
                         * Nulls Option �� ����� Direction Check�� �Ѵ�.
                         */
                        if ( ( ( aGraph->type == QMG_SELECTION ) ||
                               ( aGraph->type == QMG_PARTITION ) ) &&
                             ( aGraph->myFrom->tableRef->view == NULL ) )
                        {
                            IDE_TEST( checkSameDirection4Index( sWantOrder,
                                                                sPreservedOrder,
                                                                sPrevDirection,
                                                                & sPrevDirection,
                                                                & sUsable )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // ���⼺ �˻�
                            IDE_TEST( checkSameDirection( sWantOrder,
                                                          sPreservedOrder,
                                                          sPrevDirection,
                                                          & sPrevDirection,
                                                          & sUsable )
                                      != IDE_SUCCESS );
                        }

                        if ( sUsable == ID_FALSE )
                        {
                            // ���⼺�� �޶� Preserved Order��
                            // ����� �� ����.
                            break;
                        }
                        else
                        {
                            // Go Go
                        }
                    }
                    else
                    {
                        // ���� �ٸ� Column�� ���� Order�� ������ ����.
                        break;
                    }
                }

                if ( sWantOrder != NULL )
                {
                    // ���ϴ� Order�� ��� ������Ű�� ����
                    sUsable = ID_FALSE;
                }
                else
                {
                    // ���ϴ� Order�� ��� ������Ŵ
                    sUsable = ID_TRUE;
                }
            }
            else
            {
                //------------------------------------
                // Order�� �߿����� ���� ���
                //------------------------------------

                // Preserved Order�� ������ �ش��ϴ� Order�� �����ؾ� ��.
                // Ex) Preserved Order : i1 -> i2 -> i3 �϶�,
                //     DISTINCT i1, i2 :  O
                //     DISTINCT i3, i2, i1 :  O
                //     DISTINCT i1, i3 : X
                //     DISTINCT i2, i3 : X
                //     DISTINCT i1, i2, i3, i4 : X

                // Order�� ���� ���
                for ( sWantOrder = aWantOrder, sOrderCnt = 0;
                      sWantOrder != NULL;
                      sWantOrder = sWantOrder->next, sOrderCnt++ ) ;

                for ( sPreservedOrder = aGraph->preservedOrder, i = 0;
                      sPreservedOrder != NULL;
                      sPreservedOrder = sPreservedOrder->next, i++ )
                {
                    for ( sWantOrder = aWantOrder;
                          sWantOrder != NULL;
                          sWantOrder = sWantOrder->next )
                    {
                        if ( sWantOrder->table == sPreservedOrder->table &&
                             sWantOrder->column == sPreservedOrder->column )
                        {
                            // Order�� ������.
                            break;
                        }
                        else
                        {
                            // Nothing To Do
                        }
                    }

                    if ( sWantOrder != NULL )
                    {
                        // ���ϴ� Order�� Preserved Order���� ã�� ���
                        // Nothing To Do
                    }
                    else
                    {
                        // ���ϴ� Order�� Preserved Order�� �������� ����
                        break;
                    }
                }

                if ( i == sOrderCnt )
                {
                    /* BUG-44261 roup By, View Join �� ���� Order �˻� ���׷� ������ ���� �մϴ�.
                     *  - ��� Want Order�� Preserved Order�� ������ ���ԵǴ� �� �˻��մϴ�.
                     *     - Want Order�� Preserved Order�� �����ϴ� �� �˻��մϴ�.
                     *     - Want Order�� Preserved Order�� �����ϴ�.
                     *     - ��� Want Order�� Preserved Order�� ���Ե˴ϴ�.
                     */
                    for ( sWantOrder  = aWantOrder;
                          sWantOrder != NULL;
                          sWantOrder  = sWantOrder->next )
                    {
                        for ( sPreservedOrder  = aGraph->preservedOrder;
                              sPreservedOrder != NULL;
                              sPreservedOrder  = sPreservedOrder->next )
                        {
                            /* Want Order�� Preserved Order�� �����ϴ� �� �˻��մϴ�. */
                            if ( sWantOrder->table  == sPreservedOrder->table &&
                                 sWantOrder->column == sPreservedOrder->column )
                            {
                                break;
                            }
                            else
                            {
                                /* Nothing to do */
                            }
                        }

                        if ( sPreservedOrder == NULL )
                        {
                            /* Want Order�� Preserved Order�� �����ϴ�. */
                            break;
                        }
                        else
                        {
                            /* Nothing to do */
                        }
                    }

                    if ( sPreservedOrder != NULL )
                    {
                        /* ��� Want Order�� Preserved Order�� ���Ե˴ϴ�. */
                        sUsable = ID_TRUE;
                    }
                    else
                    {
                        sUsable = ID_FALSE;
                    }
                }
                else
                {
                    // ���ϴ� Order�� Preserved Order�� ���ų�
                    // �����ϴ��� Preserved Order�� �������
                    // ���Ե��� ����

                    sUsable = ID_FALSE;
                }
            }

            if ( sUsable == ID_TRUE )
            {
                switch ( aGraph->type )
                {
                    case QMG_SELECTION :

                        if ( aGraph->myFrom->tableRef->view == NULL )
                        {
                            sOrgMethod = ((qmgSELT*) aGraph)->selectedMethod;
                            sSelMethod = ((qmgSELT*) aGraph)->selectedMethod;
                        }
                        else
                        {
                            // Nothing To Do
                        }

                        break;

                    case QMG_PARTITION :

                        sOrgMethod = ((qmgPARTITION*) aGraph)->selectedMethod;
                        sSelMethod = ((qmgPARTITION*) aGraph)->selectedMethod;

                        break;

                    default:
                        // Nothing To Do
                        break;
                }
            }
            else
            {
                qcgPlan::registerPlanProperty(
                    aStatement,
                    PLAN_PROPERTY_OPTIMIZER_ORDER_PUSH_DOWN );

                if ( QCU_OPTIMIZER_ORDER_PUSH_DOWN != 1 )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }

                // BUG-43068 Indexable order by ����
                switch ( aGraph->type )
                {
                    case QMG_SELECTION :
                        if ( aGraph->myFrom->tableRef->view == NULL )
                        {
                            IDE_TEST( checkUsableIndex4Selection( aGraph,
                                                                  aWantOrder,
                                                                  sOrderImportant,
                                                                  &sOrgMethod,
                                                                  &sSelMethod,
                                                                  &sUsable )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            // Nothing To Do
                        }
                        break;

                    case QMG_INNER_JOIN :
                    case QMG_LEFT_OUTER_JOIN :

                        if ( aGraph->type == QMG_INNER_JOIN )
                        {
                            sJoinMethod = (((qmgJOIN*)aGraph)->selectedJoinMethod->flag & QMO_JOIN_METHOD_MASK);
                        }
                        else
                        {
                            sJoinMethod = (((qmgLOJN*)aGraph)->selectedJoinMethod->flag & QMO_JOIN_METHOD_MASK);
                        }

                        // �Ϻ� ���� �޼ҵ常 ����Ѵ�.
                        switch ( sJoinMethod )
                        {
                            case QMO_JOIN_METHOD_FULL_NL :
                            case QMO_JOIN_METHOD_FULL_STORE_NL :
                            case QMO_JOIN_METHOD_INDEX :
                            case QMO_JOIN_METHOD_ONE_PASS_HASH :

                                IDE_TEST( checkUsableOrder( aStatement,
                                                            aWantOrder,
                                                            aGraph->left,
                                                            &sOrgMethod,
                                                            &sSelMethod,
                                                            &sUsable )
                                          != IDE_SUCCESS );
                                break;
                            default :
                                break;
                        } // end switch
                        break;

                    default :
                        break;
                } // end switch
            }

            break;

        case QMG_PRESERVED_ORDER_NEVER :

            //---------------------------------------------------
            // Graph�� Preserved Order�� ����� �� ���� ���
            //---------------------------------------------------

            sUsable = ID_FALSE;
            break;

        default:
            IDE_DASSERT(0);
            break;
    }

    *aOriginalMethod = sOrgMethod;
    *aSelectMethod = sSelMethod;
    *aUsable = sUsable;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmg::checkUsableIndex4Selection( qmgGraph          * aGraph,
                                 qmgPreservedOrder * aWantOrder,
                                 idBool              aOrderNeed,
                                 qmoAccessMethod  ** aOriginalMethod,
                                 qmoAccessMethod  ** aSelectMethod,
                                 idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� ����� �� �ִ� Index�� �����ϴ� �� �˻�.
 *
 * Implementation :
 *     Index�� Key Column�� ���ϴ� Order�� ��ġ�ϴ��� �˻�
 *         - Order�� �߿��� ��� : ��Ȯ�� ��ġ�ؾ� ��.
 *         - Order�� �߿����� ���� ��� : �ش� Column�� ���� ����
 *     �ش� Index�� �����Ѵ� �ϴ��� Index�� �������� ����
 *     ����� Selection ó�� �� ���� ó���ϴ� ��뺸�� Ŭ ����
 *     Index�� �������� �ʴ´�.
 *
 ***********************************************************************/

    UInt    i;
    idBool  sUsable;
    UShort  sTableID;

    qmgSELT         * sSELTgraph;

    qmoAccessMethod * sMethod = NULL;
    qmoAccessMethod * sSelectedMethod = NULL;

    IDU_FIT_POINT_FATAL( "qmg::checkUsableIndex4Selection::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aGraph->type == QMG_SELECTION );
    IDE_DASSERT( aGraph->myFrom->tableRef->view == NULL );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aUsable != NULL );

    //---------------------------------------------------
    // ������ index �˻�
    //---------------------------------------------------

    // �⺻ ���� ����
    sSELTgraph = (qmgSELT *) aGraph;
    sTableID = aGraph->myFrom->tableRef->table;

    sMethod = sSELTgraph->accessMethod;

    // ���ϴ� Order�� ������ Index�� �����ϴ� �� �˻�.
    for ( i = 0; i < sSELTgraph->accessMethodCnt; i++ )
    {
        // fix BUG-12307
        // IN subquery keyRange�� range scan�� �ϴ� ���,
        // order�� ������ �� �����Ƿ�
        // access method�� in subquery�� �����ϴ� ���� ����
        if ( sMethod[i].method != NULL )
        {
            if ( ( sMethod[i].method->flag & QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK )
                 == QMO_STAT_CARD_IDX_IN_SUBQUERY_FALSE )
            {
                // Index Scan�� ���

                // ��� ������ Index���� �˻�

                if ( aOrderNeed == ID_TRUE )
                {
                    // Order�� �߿��� ����
                    // Order�� Index�� ���⼺�� ��ġ�ϴ� ���� �˻�

                    IDE_TEST( checkIndexOrder( & sMethod[i],
                                               sTableID,
                                               aWantOrder,
                                               & sUsable )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Order�� �߿����� ���� ����
                    // Order�� �ش��ϴ� ��� Column�� Index���� �����ϴ��� �˻�

                    IDE_TEST( checkIndexColumn( sMethod[i].method->index,
                                                sTableID,
                                                aWantOrder,
                                                & sUsable )
                              != IDE_SUCCESS );
                }

                if ( sUsable == ID_TRUE )
                {
                    // To Fix BUG-8336
                    // ��� ������ Index�� ���
                    // �ش� Index�� ����ϴ� ���� �������� �˻�
                    if ( sSelectedMethod == NULL )
                    {
                        sSelectedMethod = & sMethod[i];
                    }
                    else
                    {
                        if (QMO_COST_IS_LESS(sMethod[i].totalCost,
                                             sSelectedMethod->totalCost) == ID_TRUE)
                        {
                            sSelectedMethod = & sMethod[i];
                        }
                        else if (QMO_COST_IS_EQUAL(sMethod[i].totalCost,
                                                   sSelectedMethod->totalCost) == ID_TRUE)
                        {
                            // ����� �����ϴٸ� Primary Index�� ����ϵ��� �Ѵ�.
                            if ( ( sMethod[i].method->flag
                                   & QMO_STAT_CARD_IDX_PRIMARY_MASK )
                                 == QMO_STAT_CARD_IDX_PRIMARY_TRUE )
                            {
                                sSelectedMethod = & sMethod[i];
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
                    }
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
        }
        else
        {
            // Full Scan�� ���� �� ����� �ƴ�
            // Nothing To Do
        }
    }

    // ���õ� Index�� ������ ���
    // ���ϴ� Order�� Index�� ó���� �� ����.
    if ( sSelectedMethod != NULL )
    {
        *aSelectMethod = sSelectedMethod;
        *aUsable = ID_TRUE;
    }
    else
    {
        *aSelectMethod = NULL;
        *aUsable = ID_FALSE;
    }

    // ������ ���Ǵ� Method
    *aOriginalMethod = sSELTgraph->selectedMethod;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::checkUsableIndex4Partition( qmgGraph          * aGraph,
                                 qmgPreservedOrder * aWantOrder,
                                 idBool              aOrderNeed,
                                 qmoAccessMethod  ** aOriginalMethod,
                                 qmoAccessMethod  ** aSelectMethod,
                                 idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� ����� �� �ִ� Index�� �����ϴ� �� �˻�.
 *
 * Implementation :
 *     Index�� Key Column�� ���ϴ� Order�� ��ġ�ϴ��� �˻�
 *         - Order�� �߿��� ��� : ��Ȯ�� ��ġ�ؾ� ��.
 *         - Order�� �߿����� ���� ��� : �ش� Column�� ���� ����
 *     �ش� Index�� �����Ѵ� �ϴ��� Index�� �������� ����
 *     ����� Selection ó�� �� ���� ó���ϴ� ��뺸�� Ŭ ����
 *     Index�� �������� �ʴ´�.
 *
 ***********************************************************************/

    UInt    i;
    idBool  sUsable;
    UShort  sTableID;

    qmgPARTITION    * sPARTgraph;

    qmoAccessMethod * sMethod = NULL;
    qmoAccessMethod * sSelectedMethod = NULL;

    IDU_FIT_POINT_FATAL( "qmg::checkUsableIndex4Partition::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aGraph->type == QMG_PARTITION );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aUsable != NULL );

    //---------------------------------------------------
    // ������ index �˻�
    //---------------------------------------------------

    // �⺻ ���� ����
    sPARTgraph = (qmgPARTITION *) aGraph;
    sTableID = aGraph->myFrom->tableRef->table;

    sMethod = sPARTgraph->accessMethod;

    // ���ϴ� Order�� ������ Index�� �����ϴ� �� �˻�.
    for ( i = 0; i < sPARTgraph->accessMethodCnt; i++ )
    {
        // fix BUG-12307
        // IN subquery keyRange�� range scan�� �ϴ� ���,
        // order�� ������ �� �����Ƿ�
        // access method�� in subquery�� �����ϴ� ���� ����
        if ( sMethod[i].method != NULL )
        {
            // index table scan�� ���
            if ( ( sMethod[i].method->index->indexPartitionType ==
                   QCM_NONE_PARTITIONED_INDEX )
                 &&
                 ( ( sMethod[i].method->flag & QMO_STAT_CARD_IDX_IN_SUBQUERY_MASK )
                   == QMO_STAT_CARD_IDX_IN_SUBQUERY_FALSE ) )
            {
                // ��� ������ Index���� �˻�

                if ( aOrderNeed == ID_TRUE )
                {
                    // Order�� �߿��� ����
                    // Order�� Index�� ���⼺�� ��ġ�ϴ� ���� �˻�

                    IDE_TEST( checkIndexOrder( & sMethod[i],
                                               sTableID,
                                               aWantOrder,
                                               & sUsable )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Order�� �߿����� ���� ����
                    // Order�� �ش��ϴ� ��� Column�� Index���� �����ϴ��� �˻�

                    IDE_TEST( checkIndexColumn( sMethod[i].method->index,
                                                sTableID,
                                                aWantOrder,
                                                & sUsable )
                              != IDE_SUCCESS );
                }

                if ( sUsable == ID_TRUE )
                {
                    // To Fix BUG-8336
                    // ��� ������ Index�� ���
                    // �ش� Index�� ����ϴ� ���� �������� �˻�
                    if ( sSelectedMethod == NULL )
                    {
                        sSelectedMethod = & sMethod[i];
                    }
                    else
                    {
                        if (QMO_COST_IS_LESS(sMethod[i].totalCost,
                                             sSelectedMethod->totalCost) == ID_TRUE)
                        {
                            sSelectedMethod = & sMethod[i];
                        }
                        else if (QMO_COST_IS_EQUAL(sMethod[i].totalCost,
                                                   sSelectedMethod->totalCost) == ID_TRUE)
                        {
                            // ����� �����ϴٸ� Primary Index�� ����ϵ��� �Ѵ�.
                            if ( ( sMethod[i].method->flag
                                   & QMO_STAT_CARD_IDX_PRIMARY_MASK )
                                 == QMO_STAT_CARD_IDX_PRIMARY_TRUE )
                            {
                                sSelectedMethod = & sMethod[i];
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
                    }
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
        }
        else
        {
            // Full Scan�� ���� �� ����� �ƴ�
            // Nothing To Do
        }
    }

    // ���õ� Index�� ������ ���
    // ���ϴ� Order�� Index�� ó���� �� ����.
    if ( sSelectedMethod != NULL )
    {
        *aSelectMethod = sSelectedMethod;
        *aUsable = ID_TRUE;
    }
    else
    {
        *aSelectMethod = NULL;
        *aUsable = ID_FALSE;
    }

    // ������ ���Ǵ� Method
    *aOriginalMethod = sPARTgraph->selectedMethod;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::checkIndexOrder( qmoAccessMethod   * aMethod,
                      UShort              aTableID,
                      qmgPreservedOrder * aWantOrder,
                      idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� �ش� Index�� Order�� ������ �� �˻�
 *
 *     ���ϴ� Order�� ���⼺ �� Order�� �߿��� ��쿡��
 *     ȣ���Ͽ� �˻��Ѵ�.
 *
 * Implementation :
 *     ���ϴ� Order�� ������ Order�� �ش� Index�� ��밡���� ����
 *     �˻��Ѵ�.
 *
 *                          <���⼺ �˻�ǥ>
 *     -------------------------------------------------------------
 *      Index ����  |  Column Order | ���ϴ� Order | ���� ����
 *     -------------------------------------------------------------
 *      Foward       |     ASC       |    ASC        |     O
 *                   |               |------------------------------
 *                   |               |    DESC       |     X
 *                   -----------------------------------------------
 *                   |     DESC      |    ASC        |     X
 *                   |               |------------------------------
 *                   |               |    DESC       |     O
 *     -------------------------------------------------------------
 *      Backward     |     ASC       |    ASC        |     X
 *                   |               |------------------------------
 *                   |               |    DESC       |     O
 *                   -----------------------------------------------
 *                   |     DESC      |    ASC        |     O
 *                   |               |------------------------------
 *                   |               |    DESC       |     X
 *     -------------------------------------------------------------
 *
 ***********************************************************************/

    UInt   i;
    idBool sUsable;

    UShort              sColumnPosition;
    idBool              sIsForward;
    qmgPreservedOrder * sOrder;

    qcmIndex          * sIndex;

    IDU_FIT_POINT_FATAL( "qmg::checkIndexOrder::__FT__" );
    
    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aMethod != NULL );
    IDE_DASSERT( aMethod->method != NULL );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aUsable != NULL );

    //---------------------------------------------------
    // �ش� index�� ���ϴ� order�� ���������� �˻�
    //---------------------------------------------------

    //----------------------------------
    // ù��° Column�� ������ �� �˻�.
    //----------------------------------

    sIndex = aMethod->method->index;

    sColumnPosition = (UShort)(sIndex->keyColumns[0].column.id % SMI_COLUMN_ID_MAXIMUM);

    sOrder = aWantOrder;

    if ( ( aTableID == sOrder->table ) &&
         ( sColumnPosition == sOrder->column ) )
    {
        // ù��° Column ������ ������ ���
        // ���ϴ� Order�� ����� Index�� ������ ��ġ��Ų��.

        switch ( aMethod->method->flag & QMO_STAT_CARD_IDX_HINT_MASK )
        {
            case QMO_STAT_CARD_IDX_INDEX_ASC :

                // Index�� ���Ͽ� Ascending Hint�� �ִ� ���

                if ( sOrder->direction == QMG_DIRECTION_ASC )
                {
                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                         == SMI_COLUMN_ORDER_ASCENDING )
                    {
                        sIsForward = ID_TRUE;
                    }
                    else
                    {
                        sIsForward = ID_FALSE;
                    }
                    
                    sUsable = ID_TRUE;
                }
                else
                {
                    sUsable = ID_FALSE;
                }
                break;

            case QMO_STAT_CARD_IDX_INDEX_DESC :

                // Index�� ���Ͽ� Descending Hint�� �ִ� ���

                if ( sOrder->direction == QMG_DIRECTION_DESC )
                {
                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                         == SMI_COLUMN_ORDER_ASCENDING )
                    {
                        sIsForward = ID_FALSE;
                    }
                    else
                    {
                        sIsForward = ID_TRUE;
                    }
                    sUsable = ID_TRUE;
                }
                else
                {
                    sUsable = ID_FALSE;
                }
                break;

            default :

                // ������ Hint �� ���� ���

                switch ( sOrder->direction )
                {
                    case QMG_DIRECTION_ASC:

                        // ���ϴ� Order�� Ascending�� ���

                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sIsForward = ID_TRUE;
                        }
                        else
                        {
                            sIsForward = ID_FALSE;
                        }

                        sUsable = ID_TRUE;

                        break;

                    case QMG_DIRECTION_DESC:

                        // ���ϴ� Order�� Descending�� ���

                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sIsForward = ID_FALSE;
                        }
                        else
                        {
                            sIsForward = ID_TRUE;
                        }
                        sUsable = ID_TRUE;
                        break;
                    case QMG_DIRECTION_NOT_DEFINED:
                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sIsForward = ID_TRUE;
                        }
                        else
                        {
                            sIsForward = ID_FALSE;
                        }
                        sUsable = ID_TRUE;
                        break;
                    case QMG_DIRECTION_ASC_NULLS_FIRST:
                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sUsable = ID_FALSE;
                        }
                        else
                        {
                            sIsForward = ID_FALSE;
                            sUsable    = ID_TRUE;
                        }
                        break;
                    case QMG_DIRECTION_ASC_NULLS_LAST:
                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sIsForward = ID_TRUE;
                            sUsable    = ID_TRUE;
                        }
                        else
                        {
                            sUsable = ID_FALSE;
                        }
                        break;
                    case QMG_DIRECTION_DESC_NULLS_FIRST:
                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sIsForward = ID_FALSE;
                            sUsable    = ID_TRUE;
                        }
                        else
                        {
                            sUsable = ID_FALSE;
                        }
                        break;
                    case QMG_DIRECTION_DESC_NULLS_LAST:
                        if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                             == SMI_COLUMN_ORDER_ASCENDING )
                        {
                            sUsable = ID_FALSE;
                        }
                        else
                        {
                            sIsForward = ID_TRUE;
                            sUsable    = ID_TRUE;
                        }
                        break;
                    default:
                        IDE_DASSERT(0);
                        sUsable = ID_FALSE;
                        break;
                }
                break;
        } // end of switch
    }
    else
    {
        // ù��° Column ������ �ٸ�
        sUsable = ID_FALSE;
    }

    //----------------------------------
    // �ι�° ������ Column�� ������ �� �˻�.
    //----------------------------------

    if ( sUsable == ID_TRUE )
    {
        // ���ϴ� �� ��° Order���� �����Ͽ�
        // Index���� ������ Column�� �����ϰ�
        // ���⼺�� ������ �� �ִ� �� �˻��Ѵ�.

        for ( i = 1, sOrder = sOrder->next;
              i < sIndex->keyColCount && sOrder != NULL;
              i++, sOrder = sOrder->next )
        {
            sColumnPosition = (UShort)(sIndex->keyColumns[i].column.id % SMI_COLUMN_ID_MAXIMUM);

            if ( (sOrder->table == aTableID) &&
                 (sOrder->column == sColumnPosition) )
            {
                // �ε��� Column�� ���⼺�� ���ϴ� Order�� ���⼺ �˻�
                // ���� : <���⼺ �˻�ǥ>

                if ( sIsForward == ID_TRUE )
                {
                    // Forward Index�� ���

                    switch ( sIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                    {
                        case SMI_COLUMN_ORDER_ASCENDING :
                            switch ( sOrder->direction )
                            {
                                case QMG_DIRECTION_ASC:
                                    sUsable = ID_TRUE;
                                    break;
                                case QMG_DIRECTION_ASC_NULLS_LAST:
                                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                                         == SMI_COLUMN_ORDER_ASCENDING )
                                    {
                                        sUsable = ID_TRUE;
                                    }
                                    else
                                    {
                                        sUsable = ID_FALSE;
                                    }
                                    break;
                                default:
                                    sUsable = ID_FALSE;
                                    break;
                            }
                            break;
                        case SMI_COLUMN_ORDER_DESCENDING :
                            switch ( sOrder->direction )
                            {
                                case QMG_DIRECTION_DESC:
                                    sUsable = ID_TRUE;
                                    break;
                                case QMG_DIRECTION_DESC_NULLS_LAST:
                                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                                         == SMI_COLUMN_ORDER_ASCENDING )
                                    {
                                        sUsable = ID_FALSE;
                                    }
                                    else
                                    {
                                        sUsable = ID_TRUE;
                                    }
                                    break;
                                default:
                                    sUsable = ID_FALSE;
                                    break;
                            }
                            break;
                        default:
                            IDE_DASSERT(0);
                            sUsable = ID_FALSE;
                            break;
                    }
                }
                else
                {
                    // Backward Index�� ���

                    switch ( sIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                    {
                        case SMI_COLUMN_ORDER_ASCENDING :
                            switch ( sOrder->direction )
                            {
                                case QMG_DIRECTION_DESC:
                                    sUsable = ID_TRUE;
                                    break;
                                case QMG_DIRECTION_DESC_NULLS_FIRST:
                                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                                         == SMI_COLUMN_ORDER_ASCENDING )
                                    {
                                        sUsable = ID_TRUE;
                                    }
                                    else
                                    {
                                        sUsable = ID_FALSE;
                                    }
                                    break;
                                default:
                                    sUsable = ID_FALSE;
                                    break;
                            }
                            break;
                        case SMI_COLUMN_ORDER_DESCENDING :
                            switch ( sOrder->direction )
                            {
                                case QMG_DIRECTION_ASC:
                                    sUsable = ID_TRUE;
                                    break;
                                case QMG_DIRECTION_ASC_NULLS_FIRST:
                                    if ( ( sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK )
                                         == SMI_COLUMN_ORDER_ASCENDING )
                                    {
                                        sUsable = ID_FALSE;
                                    }
                                    else
                                    {
                                        sUsable = ID_TRUE;
                                    }
                                    break;
                                default:
                                    sUsable = ID_FALSE;
                                    break;
                            }
                            break;
                        default:
                            IDE_DASSERT(0);
                            sUsable = ID_FALSE;
                            break;
                    }
                }

                if ( sUsable == ID_FALSE )
                {
                    // ���⼺�� �޶� Index�� ����� �� ���� ���
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // �÷� ������ �޶� Index�� ����� �� ���� ���
                break;
            }
        } // end of for()

        if ( sOrder != NULL )
        {
            // ���ϴ� Order�� Index�� ��� �������� ����.
            sUsable = ID_FALSE;
        }
        else
        {
            // ���ϴ� Order�� ��� ������.
            sUsable = ID_TRUE;
        }
    }
    else
    {
        // ù��° Key Column�� �������� ����
        sUsable = ID_FALSE;
    }

    *aUsable = sUsable;
    
    return IDE_SUCCESS;
    
}


IDE_RC
qmg::checkIndexColumn( qcmIndex          * aIndex,
                       UShort              aTableID,
                       qmgPreservedOrder * aWantOrder,
                       idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� �ش� Index�� Column�� ��� ���ԵǴ� �� �˻�
 *
 *     ���ϴ� Order�� ���⼺ �� Order�� �߿����� ���� ��쿡��
 *     ȣ���Ͽ� �˻��Ѵ�.
 *
 * Implementation :
 *
 *     ���ϴ� Order�� Column�� �ε��� Column�� ��� ���ԵǴ� ����
 *     �˻��Ѵ�.
 *
 *     ��� Order�� ��� Index Key Column�� ������ ���յǾ�� �Ѵ�.
 *         T1(i1,i2,i3) Index
 *         SELECT DISTINCT i1, i2 FROM T1;         ---- O
 *         SELECT DISTINCT i2, i1, i3 FROM T1;     ---- O
 *         SELECT DISTINCT i1, i3, i4 FROM T1;     ---- X
 *         SELECT DISTINCT i2, i3 FROM T1;         ---- X
 *
 ***********************************************************************/

    UInt                i;
    UInt                sOrderCnt;

    idBool              sUsable = ID_FALSE;
    UShort              sColumnPosition;

    qmgPreservedOrder * sOrder;

    IDU_FIT_POINT_FATAL( "qmg::checkIndexColumn::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aUsable != NULL );

    //---------------------------------------------------
    // �ش� index�� ���ϴ� order�� ��� column�� ���������� �˻�
    // ��� Index Key Column�� �ش��ϴ� Order�� �����ϴ� �� �˻�
    //---------------------------------------------------

    // Order�� ���� ���
    for ( sOrder = aWantOrder, sOrderCnt = 0;
          sOrder != NULL;
          sOrder = sOrder->next, sOrderCnt++ ) ;

    if ( sOrderCnt <= aIndex->keyColCount )
    {
        // Index Key Column�� �������
        // Order�� �����ϰ� �ִ� �� �˻�.

        for ( i = 0; i < sOrderCnt; i++ )
        {
            sColumnPosition = (UShort)(aIndex->keyColumns[i].column.id % SMI_COLUMN_ID_MAXIMUM);

            sUsable = ID_FALSE;

            for ( sOrder = aWantOrder;
                  sOrder != NULL;
                  sOrder = sOrder->next )
            {
                if ( ( sOrder->table == aTableID ) &&
                     ( sOrder->column == sColumnPosition ) )
                {
                    // ������ Key Column�� ����
                    sUsable = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            if ( sUsable == ID_FALSE )
            {
                // Key Column�� �ش��ϴ� Order�� ����
                break;
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else
    {
        // Order ������ Index Key Column�� �������� ����
        sUsable = ID_FALSE;
    }

    *aUsable = sUsable;
    
    return IDE_SUCCESS;
    
}


IDE_RC
qmg::refineID4Target( qmgPreservedOrder * aWantOrder,
                      qmsTarget         * aTarget )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� Target�� ID�� ��ü�Ѵ�.
 *
 * Implementation :
 *     ���ϴ� Order�� Column Position���κ��� Target�� ��ġ��
 *     ���ϰ� Target�� ID�� Order�� ID�� ��ü�Ѵ�.
 *
 ***********************************************************************/

    UShort  i;

    qmgPreservedOrder * sOrder;
    qmsTarget         * sTarget;
    qtcNode           * sTargetNode;

    IDU_FIT_POINT_FATAL( "qmg::refineID4Target::__FT__" );
    
    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aTarget != NULL );

    //---------------------------------------------------
    // Order�� ID�� ����
    //---------------------------------------------------

    for ( sOrder = aWantOrder;
          sOrder != NULL;
          sOrder = sOrder->next )
    {
        // Column Position�� �ش��ϴ� Target�� ����
        for ( i = 0, sTarget = aTarget;
              i < sOrder->column && sTarget != NULL;
              i++, sTarget = sTarget->next ) ;

        // �ݵ�� Target�� �����ؾ� ��.
        IDE_FT_ASSERT( sTarget != NULL );

        // BUG-38193 target�� pass node �� ����ؾ� �մϴ�.
        if ( sTarget->targetColumn->node.module == &qtc::passModule )
        {
            sTargetNode = (qtcNode*)(sTarget->targetColumn->node.arguments);
        }
        else
        {
            sTargetNode = sTarget->targetColumn;
        }

        // Order�� ID ����
        sOrder->table  = sTargetNode->node.table;
        sOrder->column = sTargetNode->node.column;
    }
    
    return IDE_SUCCESS;
    
}

/**
 * Bug-42145
 *
 * Index�� �����ϴ� ��Ȳ���� Nulls Option�� ǥ�Ե� ���� üũ���Ѵ�.
 *
 * Index���� ���� Direction�� Not Defined�� ��� ASC NULLS FIRST�� DESC NULLS
 * LAST �� ���� ó���� �� ����.
 * Index���� ASC = ASC NULLS LAST,
 *           DESC = DESC NULLS FIRST �� ����.
 */
IDE_RC qmg::checkSameDirection4Index( qmgPreservedOrder * aWantOrder,
                                      qmgPreservedOrder * aPresOrder,
                                      qmgDirectionType    aPrevDirection,
                                      qmgDirectionType  * aNowDirection,
                                      idBool            * aUsable )
{
    idBool sUsable = ID_FALSE;
    qmgDirectionType sDirection = QMG_DIRECTION_NOT_DEFINED;

    IDU_FIT_POINT_FATAL( "qmg::checkSameDirection4Index::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aPresOrder != NULL );
    IDE_DASSERT( aNowDirection != NULL );
    IDE_DASSERT( aUsable    != NULL );

    //---------------------------------------------------
    // ���⼺ �˻�
    //---------------------------------------------------

    switch ( aPresOrder->direction )
    {
        case QMG_DIRECTION_NOT_DEFINED :

            if ( ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_FIRST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_LAST )  || 
                 ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_FIRST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_LAST ) )
            {
                sUsable = ID_FALSE;
            }
            else
            {
                // Preserved Order�� ��� ���⼺�� ���� ���
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            break;

        case QMG_DIRECTION_SAME_WITH_PREV :

            // Preserved Order�� ���� ����� ������ ���

            if ( aPrevDirection == aWantOrder->direction )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_DIFF_WITH_PREV :

            // Preserved Order�� ���� ����� �ٸ� ���

            if ( aPrevDirection == aWantOrder->direction )
            {
                sUsable = ID_FALSE;
            }
            else
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            break;

        case QMG_DIRECTION_ASC :
            if ( ( aWantOrder->direction == QMG_DIRECTION_ASC ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_LAST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_NOT_DEFINED ) )
            {
                sDirection = QMG_DIRECTION_ASC;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_DESC :
            if ( ( aWantOrder->direction == QMG_DIRECTION_DESC ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_FIRST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_NOT_DEFINED ) )
            {
                sDirection = QMG_DIRECTION_DESC;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_ASC_NULLS_FIRST :
            if ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_FIRST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_ASC_NULLS_LAST :
            if ( ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_LAST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_ASC ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_NOT_DEFINED ) )
            {
                sDirection = QMG_DIRECTION_ASC;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_DESC_NULLS_FIRST :
            if ( ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_FIRST ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_DESC ) ||
                 ( aWantOrder->direction == QMG_DIRECTION_NOT_DEFINED ) )
            {
                sDirection = QMG_DIRECTION_DESC;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_DESC_NULLS_LAST :
            if ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_LAST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        default :
            IDE_DASSERT(0);
            sUsable = ID_FALSE;
            break;
    }

    *aUsable = sUsable;
    *aNowDirection = sDirection;
    
    return IDE_SUCCESS;
    
}

IDE_RC
qmg::checkSameDirection( qmgPreservedOrder * aWantOrder,
                         qmgPreservedOrder * aPresOrder,
                         qmgDirectionType    aPrevDirection,
                         qmgDirectionType  * aNowDirection,
                         idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *     ���ϴ� Order�� Preserved Order�� ������(��� ������)
 *     ���⼺�� ���� �ִ� �� �˻��Ѵ�.
 *
 * Implementation :
 *     Preserved Order�� ���� ���⼺�� ����,
 *     ���ϴ� Order�� ����� �� �ִ����� �Ǵ�
 *
 ***********************************************************************/

    idBool sUsable = ID_FALSE;
    qmgDirectionType sDirection = QMG_DIRECTION_NOT_DEFINED;

    IDU_FIT_POINT_FATAL( "qmg::checkSameDirection::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aWantOrder != NULL );
    IDE_DASSERT( aPresOrder != NULL );
    IDE_DASSERT( aNowDirection != NULL );
    IDE_DASSERT( aUsable    != NULL );

    //---------------------------------------------------
    // ���⼺ �˻�
    //---------------------------------------------------

    switch ( aPresOrder->direction )
    {
        case QMG_DIRECTION_NOT_DEFINED :

            // Preserved Order�� ��� ���⼺�� ���� ���
            sDirection = aWantOrder->direction;
            sUsable = ID_TRUE;
            break;

        case QMG_DIRECTION_SAME_WITH_PREV :

            // Preserved Order�� ���� ����� ������ ���

            if ( aPrevDirection == aWantOrder->direction )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_DIFF_WITH_PREV :

            // Preserved Order�� ���� ����� �ٸ� ���

            if ( aPrevDirection == aWantOrder->direction )
            {
                sUsable = ID_FALSE;
            }
            else
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            break;

        case QMG_DIRECTION_ASC :
            if ( aWantOrder->direction == QMG_DIRECTION_ASC )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_DESC :
            if ( aWantOrder->direction == QMG_DIRECTION_DESC )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;

        case QMG_DIRECTION_ASC_NULLS_FIRST :
            if ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_FIRST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_ASC_NULLS_LAST :
            if ( aWantOrder->direction == QMG_DIRECTION_ASC_NULLS_LAST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_DESC_NULLS_FIRST :
            if ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_FIRST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        case QMG_DIRECTION_DESC_NULLS_LAST :
            if ( aWantOrder->direction == QMG_DIRECTION_DESC_NULLS_LAST )
            {
                sDirection = aWantOrder->direction;
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
            break;
        default :
            IDE_DASSERT(0);
            sUsable = ID_FALSE;
            break;
    }

    *aUsable = sUsable;
    *aNowDirection = sDirection;
    
    return IDE_SUCCESS;

}

IDE_RC
qmg::makeOrder4Graph( qcStatement       * aStatement,
                      qmgGraph          * aGraph,
                      qmgPreservedOrder * aWantOrder )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Order�� �̿��Ͽ� Graph��
 *    Preserved Order�� �����Ѵ�.
 *
 * Implementation :
 *
 *    �ش� Graph�� ������ ���� ó��
 *        - Selection Graph�� ���
 *            - Base Table �� ��� :
 *                �ش� Index�� ã�� �� Preserved Order Build
 *            - View�� ���
 *                ���� Target�� ID�� �����Ͽ� Child Graph�� ���� ó��
 *                �Էµ� Want Order�� Preserved Order Build
 *        - Set, Dnf, Hierarchy�� ���
 *            - �ش� ���� ����.
 *        - �� ���� Graph�� ���
 *            - Non-Leaf Graph�� ���� Preserved Order Build
 *
 ***********************************************************************/

    idBool            sOrderImportant;
    idBool            sUsable;
    idBool            sIsSorted;

    qmoAccessMethod * sOrgMethod;
    qmoAccessMethod * sSelMethod;

    qmsParseTree      * sParseTree;

    qmgPreservedOrder * sOrder;
    qmgPreservedOrder * sWantOrder;

    // To Fix BUG-8838
    qmgPreservedOrder * sFirstOrder = NULL;
    qmgPreservedOrder * sLastOrder = NULL;
    qmgPreservedOrder * sNewOrder = NULL;

    IDU_FIT_POINT_FATAL( "qmg::makeOrder4Graph::__FT__" );

    sIsSorted = ID_FALSE;

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( aWantOrder != NULL );

    // Order�� �߿������� �Ǵ�.
    // BUG-40361 supporting to indexable analytic function
    // aWantOrder�� ��� Node�� NOT DEFINED ���� Ȯ���Ѵ�
    sOrderImportant = ID_FALSE;
    for ( sWantOrder = aWantOrder; sWantOrder != NULL; sWantOrder = sWantOrder->next )
    {
        if ( sWantOrder->direction != QMG_DIRECTION_NOT_DEFINED )
        {
            sOrderImportant = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }
    // Graph ������ ���� Preserved Order ����
    switch ( aGraph->type )
    {
        case QMG_SET :
        case QMG_DNF :
        case QMG_HIERARCHY :

            //--------------------------------
            // Order�� ������ �� ���� Graph�� ���
            //--------------------------------

            IDE_DASSERT( 0 );
            break;
            
        case QMG_WINDOWING :
            
            if ( ( aGraph->flag & QMG_PRESERVED_ORDER_MASK )
                 == QMG_PRESERVED_ORDER_DEFINED_NOT_FIXED )
            {
                // Windowing Graph�� preserved order�� �����ϴ� ���,
                // Want Order�� ������ Sorting Key�� Sorting ������
                // ���������� �����Ѵ�.
                IDE_TEST( qmgWindowing::alterSortingOrder( aStatement,
                                                           aGraph,
                                                           aWantOrder,
                                                           ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( ( aGraph->flag & QMG_CHILD_PRESERVED_ORDER_USE_MASK )
                     == QMG_CHILD_PRESERVED_ORDER_CAN_USE )
                {
                    IDE_TEST( checkUsableOrder( aStatement,
                                                aWantOrder,
                                                aGraph->left,
                                                & sOrgMethod,
                                                & sSelMethod,
                                                & sUsable )
                              != IDE_SUCCESS );
                    if ( sUsable == ID_TRUE )
                    {
                        // BUG-21812
                        // Windowing Graph�� preserved order�� �������� �ʴ� ���
                        // ( �� over���� ���� analytic function�鸸�� �����ϴ�
                        //   ��� )
                        IDE_TEST( makeOrder4NonLeafGraph( aStatement,
                                                          aGraph,
                                                          aWantOrder )
                                  != IDE_SUCCESS );
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
            
            break;

        case QMG_SELECTION :

            if ( aGraph->myFrom->tableRef->view == NULL )
            {
                // �Ϲ� Table�� ���� Selection Graph�� ���

                // Order�� �����ϴ� Index ����
                IDE_TEST( checkUsableIndex4Selection( aGraph,
                                                      aWantOrder,
                                                      sOrderImportant,
                                                      & sOrgMethod,
                                                      & sSelMethod,
                                                      & sUsable )
                          != IDE_SUCCESS );

                // BUG-45062 sUsable ���� üũ�ؾ� �մϴ�.
                if ( sUsable == ID_TRUE )
                {
                    // Selection Graph�� ���� Order ����
                    IDE_TEST( makeOrder4Index( aStatement,
                                               aGraph,
                                               sSelMethod,
                                               aWantOrder )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                // View�� ���� Selection Graph�� ���

                // �ش� Graph�� Preserved Order�� ���� ����
                for ( sOrder = aWantOrder;
                      sOrder != NULL;
                      sOrder = sOrder->next )
                {
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                                  ID_SIZEOF(qmgPreservedOrder),
                                  (void**) & sNewOrder )
                              != IDE_SUCCESS );

                    idlOS::memcpy( sNewOrder,
                                   sOrder,
                                   ID_SIZEOF(qmgPreservedOrder) );
                    sNewOrder->next = NULL;

                    if ( sFirstOrder == NULL )
                    {
                        sFirstOrder = sNewOrder;
                        sLastOrder = sFirstOrder;
                    }
                    else
                    {
                        sLastOrder->next = sNewOrder;
                        sLastOrder = sLastOrder->next;
                    }
                }

                // Child Graph�� ���� ID ����
                sParseTree = (qmsParseTree *)
                    aGraph->myFrom->tableRef->view->myPlan->parseTree;

                // ���ϴ� Order�� ID��
                // View�� Target�� �����ϴ� ID�� ����
                IDE_TEST(
                    refineID4Target( aWantOrder,
                                     sParseTree->querySet->target )
                    != IDE_SUCCESS );

                // Child Graph�� ���� Preserved Order ����
                IDE_TEST( makeOrder4NonLeafGraph(
                              aGraph->myFrom->tableRef->view,
                              aGraph->left,
                              aWantOrder )
                          != IDE_SUCCESS );

                IDE_DASSERT( aGraph->left->preservedOrder != NULL );

                // View�� Preserved Order�� ���� direction ���� ����
                for ( sNewOrder = sFirstOrder,
                          sOrder = aGraph->left->preservedOrder;
                      sNewOrder != NULL && sOrder != NULL;
                      sNewOrder = sNewOrder->next, sOrder = sOrder->next )
                {
                    sNewOrder->direction = sOrder->direction;
                }

                // View�� Selection Graph�� ���� ���� ����
                aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
                aGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
                aGraph->preservedOrder = sFirstOrder;
            }

            break;

        case QMG_PARTITION :

            // Order�� �����ϴ� Index ����
            IDE_TEST( checkUsableIndex4Partition( aGraph,
                                                  aWantOrder,
                                                  sOrderImportant,
                                                  & sOrgMethod,
                                                  & sSelMethod,
                                                  & sUsable )
                      != IDE_SUCCESS );

            // BUG-45062 sUsable ���� üũ�ؾ� �մϴ�.
            if ( sUsable == ID_TRUE )
            {
                // Selection Graph�� ���� Order ����
                IDE_TEST( makeOrder4Index( aStatement,
                                           aGraph,
                                           sSelMethod,
                                           aWantOrder )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
            break;
            
            // To fix BUG-14336
            // join�� preserved order�� child�� ���õ� order��.
        case QMG_PROJECTION :
        case QMG_INNER_JOIN :
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
        case QMG_LEFT_OUTER_JOIN :
        case QMG_FULL_OUTER_JOIN :
            // BUG-22034
        case QMG_COUNTING :

            IDE_TEST( makeOrder4NonLeafGraph( aStatement,
                                              aGraph,
                                              aWantOrder )
                      != IDE_SUCCESS );
            break;

            // To Fix BUG-8344
        case QMG_GROUPING :
        case QMG_SORTING :
        case QMG_DISTINCTION :
            if ( aGraph->preservedOrder != NULL )
            {
                // To fix BUG-14336
                // grouping, sorting, distinction graph�� sorting�� ����
                // preserved order�� ������ ���� ������ ����.
                switch( aGraph->type )
                {
                    case QMG_GROUPING:
                        // grouping ���� none�� ��� sort�� ���� preserved order ����
                        if( ( aGraph->flag & QMG_GROP_OPT_TIP_MASK ) == QMG_GROP_OPT_TIP_NONE )
                        {
                            sIsSorted = ID_TRUE;
                        }
                        else
                        {
                            sIsSorted = ID_FALSE;
                        }
                        break;

                    case QMG_SORTING:
                        // sort ���� none �̰ų� lmst�� ��� sort�� ���� preserved order ����
                        if( ( ( aGraph->flag & QMG_SORT_OPT_TIP_MASK ) == QMG_SORT_OPT_TIP_NONE ) ||
                            ( ( aGraph->flag & QMG_SORT_OPT_TIP_MASK ) == QMG_SORT_OPT_TIP_LMST ) )
                        {
                            sIsSorted = ID_TRUE;
                        }
                        else
                        {
                            sIsSorted = ID_FALSE;
                        }
                        break;

                    case QMG_DISTINCTION:
                        // distinct ���� none�� ��� sort�� ���� preserved order ����
                        if( ( aGraph->flag & QMG_DIST_OPT_TIP_MASK ) == QMG_DIST_OPT_TIP_NONE )
                        {
                            sIsSorted = ID_TRUE;
                        }
                        else
                        {
                            sIsSorted = ID_FALSE;
                        }
                        break;

                    default:
                        IDE_DASSERT(0);
                        break;
                }

                if ( sIsSorted == ID_TRUE )
                {
                    //---------------------------------------------------
                    // sorting�� ���Ͽ� ���� preserved order�� ������ ���
                    //---------------------------------------------------

                    if ( aGraph->preservedOrder->direction ==
                         QMG_DIRECTION_NOT_DEFINED )
                    {
                        for ( sOrder = aGraph->preservedOrder,
                                  sWantOrder = aWantOrder;
                              sOrder != NULL && sWantOrder != NULL;
                              sOrder = sOrder->next,
                                  sWantOrder = sWantOrder->next )
                        {
                            sOrder->direction = sWantOrder->direction;
                        }
                    }
                    else
                    {
                        // nothing to do
                    }
                }
                else
                {
                    IDE_TEST( makeOrder4NonLeafGraph( aStatement,
                                                      aGraph,
                                                      aWantOrder )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_TEST( makeOrder4NonLeafGraph( aStatement,
                                                  aGraph,
                                                  aWantOrder )
                          != IDE_SUCCESS );
            }
            break;

        default :
            IDE_DASSERT( 0 );
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::makeOrder4Index( qcStatement       * aStatement,
                      qmgGraph          * aGraph,
                      qmoAccessMethod   * aMethod,
                      qmgPreservedOrder * aWantOrder )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Order�� index�� �̿��Ͽ� Selection Graph��
 *    Preserved Order�� �����Ѵ�.
 *
 * Implementation :
 *
 *    ���� Graph������ Merge�� ���� Index�� ��� Presreved Order��
 *    �������� �ʰ�, �־��� Order�� �ش��ϴ� Preserved Order��
 *    �����Ѵ�.
 *
 ***********************************************************************/

    UInt                i;
    idBool              sOrderImportant;
    qmgPreservedOrder * sOrder;

    qcmIndex          * sIndex;
    idBool              sHintExist;
    UInt                sPrevOrder;
    UShort              sColumnPosition;

    qmgPreservedOrder * sFirstOrder = NULL;
    qmgPreservedOrder * sLastOrder = NULL;
    qmgPreservedOrder * sNewOrder = NULL;

    qmgSELT           * sSELTgraph;
    qmgPARTITION      * sPARTgraph;

    IDU_FIT_POINT_FATAL( "qmg::makeOrder4Index::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aGraph != NULL );
    IDE_FT_ASSERT( (aGraph->type == QMG_SELECTION) ||
                   (aGraph->type == QMG_PARTITION) );
    IDE_FT_ASSERT( aMethod != NULL );
    IDE_FT_ASSERT( aWantOrder != NULL );

    //---------------------------------------------------
    // Index�� �̿��� Preserved Order ����
    //---------------------------------------------------

    // Order�� �߿������� �Ǵ�.
    // BUG-40361 supporting to indexable analytic function
    // aWantOrder�� ��� Node�� NOT DEFINED ���� Ȯ���Ѵ�
    sOrderImportant = ID_FALSE;
    for ( sOrder = aWantOrder; sOrder != NULL; sOrder = sOrder->next )
    {
        if ( sOrder->direction != QMG_DIRECTION_NOT_DEFINED )
        {
            sOrderImportant = ID_TRUE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sOrderImportant == ID_TRUE )
    {
        // Order�� �߿��� ���
        // Want Order�� ������ Preserved Order�� �����Ѵ�.

        // Key Column Order��Want Order�� ������ ������ �����Ǿ� ������,
        // �̹� �ش� index�� �̿��� ���ռ� �˻�� ��� �̷���� ������

        for ( sOrder = aWantOrder;
              sOrder != NULL;
              sOrder = sOrder->next )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                     (void**) & sNewOrder )
                      != IDE_SUCCESS );

            idlOS::memcpy( sNewOrder, sOrder, ID_SIZEOF(qmgPreservedOrder) );
            sNewOrder->next = NULL;

            if ( sFirstOrder == NULL )
            {
                sFirstOrder = sNewOrder;
                sLastOrder = sFirstOrder;
            }
            else
            {
                sLastOrder->next = sNewOrder;
                sLastOrder = sLastOrder->next;
            }
        }
    }
    else
    {
        // Order�� �߿����� ���� ���
        // �� Preserved Order�� ���⼺�� ������.

        // Key Column ������ �ش��ϴ� Want Order�� �����ϳ�
        // �� ������ ����ġ ����

        // �ε��� ���� ȹ��
        sIndex = aMethod->method->index;

        // ù��° Column�� ���� Order ���� ����
        // �ݵ�� ù��° Column�� Want Order�� ���ԵǾ� ������
        // �����Ѵ�.

        sOrder = aWantOrder;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                 (void**) & sNewOrder )
                  != IDE_SUCCESS );

        // To Fix PR-8572
        // ������ �߿����� �ʴٸ�, Index�� Key Column�� �������
        // Selection Graph�� Preserved Order�� �����Ͽ��� �Ѵ�.
        sNewOrder->table = sOrder->table;
        sNewOrder->column =
            sIndex->keyColumns[0].column.id % SMI_COLUMN_ID_MAXIMUM;
        sNewOrder->next = NULL;

        // Order�� ���� ����
        switch ( aMethod->method->flag & QMO_STAT_CARD_IDX_HINT_MASK )
        {
            case QMO_STAT_CARD_IDX_INDEX_ASC :

                // Index�� ���Ͽ� Ascending Hint�� �ִ� ���
                sNewOrder->direction = QMG_DIRECTION_ASC;
                sHintExist = ID_TRUE;
                break;

            case QMO_STAT_CARD_IDX_INDEX_DESC :

                // Index�� ���Ͽ� Descending Hint�� �ִ� ���
                sNewOrder->direction = QMG_DIRECTION_DESC;
                sHintExist = ID_TRUE;
                break;

            default:
                // ������ Hint �� ���� ���
                sNewOrder->direction = QMG_DIRECTION_NOT_DEFINED;
                sHintExist = ID_FALSE;
                break;
        }

        sFirstOrder = sNewOrder;
        sLastOrder = sFirstOrder;

        sPrevOrder = sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK;

        for ( i = 1; i < sIndex->keyColCount; i++ )
        {
            // Index Key Column�� ������ Want Order�� �˻��Ѵ�.
            // �������� ���� ������ �ݺ��Ѵ�.

            for ( sOrder = aWantOrder; sOrder != NULL; sOrder = sOrder->next )
            {
                sColumnPosition = (UShort)(sIndex->keyColumns[i].column.id % SMI_COLUMN_ID_MAXIMUM);
                if ( sColumnPosition == sOrder->column )
                {
                    // ������ Column�� �����ϴ� ���
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            if ( sOrder != NULL )
            {
                // Key Column�� ������ Order�� ������

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                         (void**) & sNewOrder )
                          != IDE_SUCCESS );

                idlOS::memcpy( sNewOrder, sOrder, ID_SIZEOF(qmgPreservedOrder) );

                // Direction ����
                if ( ( sIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK )
                     == sPrevOrder )
                {
                    // ������ ������ ������ ���

                    if ( sHintExist == ID_TRUE )
                    {
                        sNewOrder->direction = sLastOrder->direction;
                    }
                    else
                    {
                        sNewOrder->direction = QMG_DIRECTION_SAME_WITH_PREV;
                    }
                }
                else
                {
                    // ������ ������ �ٸ� ���

                    if ( sHintExist == ID_TRUE )
                    {
                        switch ( sLastOrder->direction )
                        {
                            case QMG_DIRECTION_ASC:
                                sNewOrder->direction = QMG_DIRECTION_DESC;
                                break;
                            case QMG_DIRECTION_DESC:
                                sNewOrder->direction = QMG_DIRECTION_ASC;
                                break;
                            default:
                                IDE_DASSERT(0);
                                break;
                        }
                    }
                    else
                    {
                        sNewOrder->direction = QMG_DIRECTION_DIFF_WITH_PREV;
                    }

                    sPrevOrder =
                        sIndex->keyColsFlag[i] & SMI_COLUMN_ORDER_MASK;
                }
                sNewOrder->next = NULL;

                sLastOrder->next = sNewOrder;
                sLastOrder = sLastOrder->next;
            }
            else
            {
                // Key Column�� ������ Order�� �������� ����
                // �� �̻� ������ �ʿ䰡 ����
                break;
            }
        }
    }

    //---------------------------------------------------
    // Selection Graph�� ���� ����
    //---------------------------------------------------

    switch ( aGraph->type )
    {
        case QMG_SELECTION :
            
            sSELTgraph = (qmgSELT *) aGraph;

            // ���� Index ����
            IDE_TEST( qmgSelection::alterSelectedIndex( aStatement,
                                                        sSELTgraph,
                                                        aMethod->method->index )
                      != IDE_SUCCESS );

            // Preserved Order ����
            sSELTgraph->graph.preservedOrder = sFirstOrder;

            // Preserved Order Mask ����
            sSELTgraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sSELTgraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            
            break;
            
        case QMG_PARTITION :

            sPARTgraph = (qmgPARTITION *) aGraph;

            // ���� Index ����
            IDE_TEST( qmgPartition::alterSelectedIndex( aStatement,
                                                        sPARTgraph,
                                                        aMethod->method->index )
                      != IDE_SUCCESS );

            // Preserved Order ����
            sPARTgraph->graph.preservedOrder = sFirstOrder;

            // Preserved Order Mask ����
            sPARTgraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sPARTgraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            
            break;
            
        default :
            IDE_DASSERT( 0 );
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::makeOrder4NonLeafGraph( qcStatement       * aStatement,
                             qmgGraph          * aGraph,
                             qmgPreservedOrder * aWantOrder )
{
/***********************************************************************
 *
 * Description :
 *
 *    �־��� Order�� �̿��Ͽ� Non Leaf Graph��
 *    Preserved Order�� �����Ѵ�.
 *
 * Implementation :
 *
 *    Child Graph�� ���� Preserved Order Build
 *    Child Graph�� Preserved Order ������ �����Ͽ� Preserverd Order Build
 *
 ***********************************************************************/

    qmgPreservedOrder * sOrder;

    qmgPreservedOrder * sFirstOrder = NULL;
    qmgPreservedOrder * sLastOrder = NULL;
    qmgPreservedOrder * sNewOrder = NULL;

    IDU_FIT_POINT_FATAL( "qmg::makeOrder4NonLeafGraph::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aWantOrder != NULL );

    //---------------------------------------------------
    // Child Graph�� ���� Preserved Order ����
    //---------------------------------------------------

    // Child Graph �� ���� Order���� ����
    IDE_TEST( setOrder4Child( aStatement,
                              aWantOrder,
                              aGraph->left )
              != IDE_SUCCESS );

    //---------------------------------------------------
    // Non-Leaf Graph�� ���� Preserved Order ����
    //---------------------------------------------------

    // ������ Preserved Order ������ �����ϸ�,
    // �־��� Preserved Order�� �̿��Ͽ� ���� �����Ѵ�.

    aGraph->flag &= ~QMG_PRESERVED_ORDER_MASK;
    aGraph->flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;

    // Child�� Order ������ �̿��Ͽ� Order������ �����Ѵ�.
    // Child Graph�� �־��� Order������ Preserved Order��
    // �����Ǿ� �ִ�.

    // Left Graph�� �ݵ�� Preserved Order�� �����Ѵ�.
    IDE_DASSERT( aGraph->left->preservedOrder != NULL );

    //------------------------------------
    // Left Graph�κ��� Order ����
    //------------------------------------

    for ( sOrder = aGraph->left->preservedOrder;
          sOrder != NULL;
          sOrder = sOrder->next )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                                 (void **) & sNewOrder )
                  != IDE_SUCCESS );

        idlOS::memcpy( sNewOrder, sOrder, ID_SIZEOF(qmgPreservedOrder) );
        sNewOrder->next = NULL;

        if ( sLastOrder == NULL )
        {
            sFirstOrder = sNewOrder;
            sLastOrder = sFirstOrder;
        }
        else
        {
            sLastOrder->next = sNewOrder;
            sLastOrder = sLastOrder->next;
        }
    }

    //------------------------------------
    // Right Graph�κ��� Preserved Order ����
    //------------------------------------

    if ( aGraph->right != NULL )
    {
        // Right Graph���� Preserved Order�� �������� ���� �� ����
        for ( sOrder = aGraph->right->preservedOrder;
              sOrder != NULL;
              sOrder = sOrder->next )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                          ID_SIZEOF(qmgPreservedOrder),
                          (void **) & sNewOrder )
                      != IDE_SUCCESS );

            idlOS::memcpy( sNewOrder,
                           sOrder,
                           ID_SIZEOF(qmgPreservedOrder) );
            sNewOrder->next = NULL;

            IDE_FT_ASSERT( sFirstOrder != NULL );
            IDE_FT_ASSERT( sLastOrder != NULL );

            sLastOrder->next = sNewOrder;
            sLastOrder = sLastOrder->next;
        }
    }
    else
    {
        // Nothing To Do
    }

    aGraph->preservedOrder = sFirstOrder;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::makeAnalFuncResultMtrNode( qcStatement       * aStatement,
                                qmsAnalyticFunc   * aAnalyticFunc,
                                UShort              aTupleID,
                                UShort            * aColumnCount,
                                qmcMtrNode       ** aMtrNode,
                                qmcMtrNode       ** aLastMtrNode )
{
/***********************************************************************
 *
 * Description : Analytic Function ����� ����� materialize node��
 *               ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmcMtrNode        * sLastMtrNode;
    qmcMtrNode        * sNewMtrNode;

    IDU_FIT_POINT_FATAL( "qmg::makeAnalFuncResultMtrNode::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aAnalyticFunc != NULL );
    IDE_DASSERT( aColumnCount != NULL );

    //----------------------------------
    // �⺻ �ʱ�ȭ
    //----------------------------------
    
    sLastMtrNode = *aLastMtrNode;
    
    //----------------------------------
    //  1. Analytic Function�� Result�� ����� materialize node ����
    //----------------------------------

    IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement), qmcMtrNode, &sNewMtrNode)
              != IDE_SUCCESS);

    // �ʱ�ȭ
    sNewMtrNode->srcNode = NULL;
    sNewMtrNode->dstNode = NULL;
    sNewMtrNode->flag = 0;
    sNewMtrNode->next = NULL;
    sNewMtrNode->myDist = NULL;
    sNewMtrNode->bucketCnt = 0;

    // flag ����
    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
    sNewMtrNode->flag |= QMC_MTR_TYPE_COPY_VALUE;

    sNewMtrNode->flag &= ~QMC_MTR_CHANGE_COLUMN_LOCATE_MASK;
    sNewMtrNode->flag |= QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE;

    //--------------
    // srcNode ����
    //--------------
    
    IDE_TEST(
        qtc::makeInternalColumn( aStatement,
                                 aAnalyticFunc->analyticFuncNode->node.table,
                                 aAnalyticFunc->analyticFuncNode->node.column,
                                 &(sNewMtrNode->srcNode) )
        != IDE_SUCCESS);

    idlOS::memcpy( sNewMtrNode->srcNode,
                   aAnalyticFunc->analyticFuncNode,
                   ID_SIZEOF( qtcNode ) );

    //--------------
    // dstNode ����
    //--------------
    
    IDE_TEST( qtc::makeInternalColumn( aStatement,
                                       aTupleID,
                                       *aColumnCount,
                                       &(sNewMtrNode->dstNode) )
              != IDE_SUCCESS);

    idlOS::memcpy( sNewMtrNode->dstNode,
                   sNewMtrNode->srcNode,
                   ID_SIZEOF( qtcNode ) );

    sNewMtrNode->dstNode->node.table  = aTupleID;
    sNewMtrNode->dstNode->node.column = *aColumnCount;
    // Aggregate function�� ����� �����ϸ� �ǹǷ� value/column node�� �����Ѵ�.
    sNewMtrNode->dstNode->node.module = &qtc::valueModule;

    *aColumnCount += (sNewMtrNode->dstNode->node.module->lflag & MTC_NODE_COLUMN_COUNT_MASK);
    
    //----------------------------------
    // ���ο� materialize node ������ ���, �̸� ����
    //----------------------------------
    if( sLastMtrNode == NULL )
    {
        *aMtrNode    = sNewMtrNode;
        sLastMtrNode = sNewMtrNode;
    }
    else
    {
        sLastMtrNode->next = sNewMtrNode;
        sLastMtrNode       = sNewMtrNode;
    }

    *aLastMtrNode = sLastMtrNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmg::makeSortMtrNode( qcStatement        * aStatement,
                      UShort               aTupleID,
                      qmgPreservedOrder  * aSortKey,
                      qmsAnalyticFunc    * aAnalFuncList,
                      qmcMtrNode         * aMtrNode,
                      qmcMtrNode        ** aSortMtrNode,
                      UInt               * aSortMtrNodeCount )
{
/***********************************************************************
 *
 * Description : sort key�� ���� materialize node�� ������
 *
 * Implementation :
 *
 ***********************************************************************/

    UShort               sTable;
    UShort               sColumn;
    qmgPreservedOrder  * sCurSortKeyCol;
    qmsAnalyticFunc    * sAnalyticFunc;
    qmsAnalyticFunc    * sCurAnalyticFunc;
    qtcOverColumn      * sOverColumn;
    UInt                 sOverColumnCount;
    UInt                 sCurOverColumnCount;
    qmcMtrNode         * sCurNode;
    qmcMtrNode         * sNewMtrNode;
    qmcMtrNode         * sFirstSortMtrNode;
    qmcMtrNode         * sLastSortMtrNode       = NULL;
    idBool               sExist;

    IDU_FIT_POINT_FATAL( "qmg::makeSortMtrNode::__FT__" );

    // �ʱ�ȭ
    sFirstSortMtrNode = NULL;
    sAnalyticFunc     = NULL;
    sOverColumnCount  = 0;

    // BUG-33663 Ranking Function
    // sort key���� �������� analytic function���� �޷��ְ�
    // sort node�� ������ �� ���߿��� over column�� ���� ���� ���� �������� �����Ѵ�.
    // �������� preserved order������ �����߾�����
    // order�� ����Ͽ� over column�� ���� �����ؾ� �Ѵ�.
    for ( sCurAnalyticFunc = aAnalFuncList;
          sCurAnalyticFunc != NULL;
          sCurAnalyticFunc = sCurAnalyticFunc->next )
    {
        sCurOverColumnCount = 0;
        
        for ( sOverColumn = sCurAnalyticFunc->analyticFuncNode->overClause->overColumn;
              sOverColumn != NULL;
              sOverColumn = sOverColumn->next )
        {
            sCurOverColumnCount++;
        }

        if ( sCurOverColumnCount >= sOverColumnCount )
        {
            sAnalyticFunc = sCurAnalyticFunc;
            sOverColumnCount = sCurOverColumnCount;
        }
        else
        {
            // Nothing to do.
        }
    }

    // sort key�� �ش��ϴ� analytic function�� �ݵ�� �����Ѵ�.
    IDE_TEST_RAISE( sAnalyticFunc == NULL, ERR_INVALID_ANALYTIC_FUNCTION );
    
    // Sort Key�� �ش��ϴ� materialize node�� ã��
    for ( sCurSortKeyCol = aSortKey,
              sOverColumn = sAnalyticFunc->analyticFuncNode->overClause->overColumn;
          sCurSortKeyCol != NULL;
          sCurSortKeyCol = sCurSortKeyCol->next,
              sOverColumn = sOverColumn->next )
    {
        // sort key�� �����Ǵ� over column�� �ݵ�� �����ؾ� �Ѵ�.
        IDE_TEST_RAISE( sOverColumn == NULL, ERR_INVALID_OVER_COLUMN );
        
        // Į���� ���� (table, column) ������ ã��
        IDE_TEST( qmg::findColumnLocate( aStatement,
                                         aTupleID,
                                         sCurSortKeyCol->table,
                                         sCurSortKeyCol->column,
                                         & sTable,
                                         & sColumn )
                  != IDE_SUCCESS );
    
        // ���� sort key column�� mtr node�� ã��
        sExist = ID_FALSE;
        
        for ( sCurNode = aMtrNode;
              sCurNode != NULL;
              sCurNode = sCurNode->next )
        {
            if ( ( sCurNode->srcNode->node.table == sTable )
                 &&
                 ( sCurNode->srcNode->node.column == sColumn ) )
            {
                if ( ( (sCurNode->flag & QMC_MTR_TYPE_MASK)
                       != QMC_MTR_TYPE_MEMORY_TABLE )
                     &&
                     ( (sCurNode->flag & QMC_MTR_TYPE_MASK)
                       != QMC_MTR_TYPE_DISK_TABLE ) )
                {
                    if ( ( sCurNode->flag & QMC_MTR_SORT_NEED_MASK )
                         == QMC_MTR_SORT_NEED_TRUE )
                    {
                        // partition by column�̾�� ��
                        // partition by column�� sorting �ؾ� �ϹǷ�
                        // QMC_MTR_SORT_NEED_TRUE�� �����Ǿ� ����
                        
                        // BUG-33663 Ranking Function
                        // mtr node�� order column�� ��쿡 ���ؼ� ������ order�� ���� �÷��� ã�´�.
                        if ( (sOverColumn->flag & QTC_OVER_COLUMN_MASK)
                             == QTC_OVER_COLUMN_NORMAL )
                        {
                            if ( (sCurNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                                 == QMC_MTR_SORT_ORDER_FIXED_FALSE )
                            {
                                sExist = ID_TRUE;
                                break;
                            }
                            else
                            {
                                // Noting to do.
                            }
                        }
                        else
                        {
                            IDE_DASSERT( (sOverColumn->flag & QTC_OVER_COLUMN_MASK)
                                         == QTC_OVER_COLUMN_ORDER_BY );

                            if ( (sCurNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
                                 == QMC_MTR_SORT_ORDER_FIXED_TRUE )
                            {
                                if ( ( (sCurNode->flag & QMC_MTR_SORT_ORDER_MASK)
                                       == QMC_MTR_SORT_ASCENDING )
                                     &&
                                     ( (sOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                                       == QTC_OVER_COLUMN_ORDER_ASC ) )
                                {
                                    // BUG-42145 Nulls Option �� �ٸ� ��쵵
                                    // üũ�ؾ��Ѵ�.
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_NONE ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_FIRST ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_LAST ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                                    
                                if ( ( (sCurNode->flag & QMC_MTR_SORT_ORDER_MASK)
                                       == QMC_MTR_SORT_DESCENDING )
                                     &&
                                     ( (sOverColumn->flag & QTC_OVER_COLUMN_ORDER_MASK)
                                       == QTC_OVER_COLUMN_ORDER_DESC ) )
                                {
                                    // BUG-42145 Nulls Option �� �ٸ� ��쵵
                                    // üũ�ؾ��Ѵ�.
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_NONE ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_NONE ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_FIRST ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_FIRST ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
                                    }
                                    if ( ( ( sCurNode->flag & QMC_MTR_SORT_NULLS_ORDER_MASK )
                                           == QMC_MTR_SORT_NULLS_ORDER_LAST ) &&
                                         ( ( sOverColumn->flag & QTC_OVER_COLUMN_NULLS_ORDER_MASK )
                                           == QTC_OVER_COLUMN_NULLS_ORDER_LAST ) )
                                    {
                                        // order by column
                                        sExist = ID_TRUE;
                                        break;
                                    }
                                    else
                                    {
                                        /* Nothing to do */
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
                        }
                    }
                    else
                    {
                        // nothing to do 
                    }
                }
                else
                {
                    // table�� ǥ���ϱ� ���� column�� ���
                    // �ٸ� Į���ӿ��� �ұ��ϰ� ���� �� ����
                }
            }
            else
            {
                // nothing to do 
            }
        }

        // Partition By Column�� ���Ͽ� materialize node�� �̹�
        // �����������Ƿ�, ������ �����ؾ���
        IDE_TEST_RAISE( sExist == ID_FALSE, ERR_MTR_NODE_NOT_EXISTS );

        IDE_TEST( STRUCT_ALLOC(QC_QMP_MEM(aStatement), qmcMtrNode, &sNewMtrNode)
                  != IDE_SUCCESS);

        // BUG-28507
        // Partition By Column�� sorting direction�� ������
        if ( (sCurNode->flag & QMC_MTR_SORT_ORDER_FIXED_MASK)
             == QMC_MTR_SORT_ORDER_FIXED_FALSE )
        {
            if ( sCurSortKeyCol->direction == QMG_DIRECTION_DESC )
            {
                sCurNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sCurNode->flag |= QMC_MTR_SORT_DESCENDING;
            }
            else
            {
                sCurNode->flag &= ~QMC_MTR_SORT_ORDER_MASK;
                sCurNode->flag |= QMC_MTR_SORT_ASCENDING;
            }
        }
        else
        {
            // Nothing to do.
        }

        *sNewMtrNode = *sCurNode;
        sNewMtrNode->next = NULL;

        *aSortMtrNodeCount = *aSortMtrNodeCount + 1;

        if ( sFirstSortMtrNode == NULL )
        {
            sFirstSortMtrNode = sNewMtrNode;
            sLastSortMtrNode  = sNewMtrNode;
        }
        else
        {
            sLastSortMtrNode->next = sNewMtrNode;
            sLastSortMtrNode = sLastSortMtrNode->next;
        }
    }

    *aSortMtrNode = sFirstSortMtrNode;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MTR_NODE_NOT_EXISTS )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeSortMtrNode",
                                  "Materialized Node is not exists" ));
    }
    IDE_EXCEPTION( ERR_INVALID_ANALYTIC_FUNCTION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeSortMtrNode",
                                  "analytic function is null" ));
    }
    IDE_EXCEPTION( ERR_INVALID_OVER_COLUMN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeSortMtrNode",
                                  "over column is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::makeDistNode( qcStatement        * aStatement,
                   qmsQuerySet        * aQuerySet,
                   UInt                 aFlag,
                   qmnPlan            * aChildPlan,
                   UShort               aTupleID ,
                   qmoDistAggArg      * aDistAggArg,
                   qtcNode            * aAggrNode,
                   qmcMtrNode         * aMtrNode,
                   qmcMtrNode        ** aPlanDistNode,
                   UShort             * aDistNodeCount )
{
/***********************************************************************
 *
 * Description : Materialize Node�� distNode�� �����ϰ�,
 *               Plan�� distNode�� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    
    qmcMtrNode        * sDistNode;
    qmcMtrNode        * sNewMtrNode;
    idBool              sExistSameDistNode;
    UShort              sDistTupleID;
    UShort              sDistNodeCount;
    qmoDistAggArg     * sDistAggArg;
    mtcTemplate       * sMtcTemplate;
    UShort              sDistColumnCount;
    qtcNode           * sPassNode;
    qtcNode             sCopiedNode;
    mtcNode           * sMtcNode;

    IDU_FIT_POINT_FATAL( "qmg::makeDistNode::__FT__" );

    // �⺻ �ʱ�ȭ
    sExistSameDistNode = ID_FALSE;
    sDistNodeCount = *aDistNodeCount;
    sDistAggArg = NULL;
    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    /* PROJ-1353 dist node�� �ߺ��ؼ� ����� ���� ��� */
    if ( ( aMtrNode->flag & QMC_MTR_DIST_DUP_MASK )
         == QMC_MTR_DIST_DUP_FALSE )
    {
        // Plan�� ��ϵ� distinct node�� �߿���
        // ������ distinct node �����ϴ��� �˻�
        for ( sDistNode = *aPlanDistNode;
              sDistNode != NULL;
              sDistNode = sDistNode->next )
        {
            IDE_TEST(
                qtc::isEquivalentExpression(
                    aStatement,
                    (qtcNode *)aAggrNode->node.arguments,
                    sDistNode->srcNode,
                    & sExistSameDistNode )
                != IDE_SUCCESS );

            if ( sExistSameDistNode == ID_TRUE )
            {
                break;
            }
            else
            {
                // nothing to do
            }
        }
    }
    else
    {
        /* Nothing to do */
    }
    if ( sExistSameDistNode == ID_TRUE )
    {
        // ������ Distinct Argument�� ���� ���,
        // ���ο� distNode �������� �ʰ� ������ distNode�� ����Ŵ
        aMtrNode->myDist = sDistNode;
        aMtrNode->bucketCnt = sDistNode->bucketCnt;
    }
    else
    {
        //�ش� bucketCount�� ã�� ���ؼ� ���� aggregation���� ã��
        for( sDistAggArg = aDistAggArg ;
             sDistAggArg != NULL ;
             sDistAggArg = sDistAggArg->next )
        {
            //���� aggregation�̶��
            if( aAggrNode->node.arguments == (mtcNode*)sDistAggArg->aggArg )
            {
                sDistNodeCount++;
                sDistColumnCount = 0;

                //----------------------------------
                // Distinct�� ���� Ʃ���� �Ҵ�
                //----------------------------------
                IDE_TEST( qtc::nextTable( &sDistTupleID ,
                                          aStatement ,
                                          NULL,
                                          ID_TRUE,
                                          MTC_COLUMN_NOTNULL_TRUE )
                          != IDE_SUCCESS );

                if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
                {
                    if( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
                        == QMN_PLAN_STORAGE_DISK )
                    {            
                        sMtcTemplate->rows[sDistTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
                        sMtcTemplate->rows[sDistTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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

                // PROJ-1358
                // Next Table �Ŀ��� Internal Tuple Set��
                // �޸� Ȯ������ ���� ���� ������� ���ϴ�
                // ��찡 �ִ�.
                // mtcTuple * ���������� ����ϸ� �ȵ�
                // �÷��� ��ü ���θ� �����ϱ� ���ؼ���
                // Tuple�� ���� ��ü ������ �̸� ����ϰ� �־�� �Ѵ�.
                if( (aFlag & QMN_PLAN_STORAGE_MASK) ==
                    QMN_PLAN_STORAGE_MEMORY )
                {
                    sMtcTemplate->rows[sDistTupleID].lflag
                        &= ~MTC_TUPLE_STORAGE_MASK;
                    sMtcTemplate->rows[sDistTupleID].lflag
                        |= MTC_TUPLE_STORAGE_MEMORY;
                }
                else
                {
                    sMtcTemplate->rows[sDistTupleID].lflag
                        &= ~MTC_TUPLE_STORAGE_MASK;
                    sMtcTemplate->rows[sDistTupleID].lflag
                        |= MTC_TUPLE_STORAGE_DISK;
                }

                sCopiedNode = *sDistAggArg->aggArg;

                //distNode ����
                IDE_TEST( qmg::makeColumnMtrNode( aStatement ,
                                                  aQuerySet ,
                                                  &sCopiedNode ,
                                                  ID_TRUE,
                                                  sDistTupleID ,
                                                  0,
                                                  &sDistColumnCount ,
                                                  &sNewMtrNode )
                          != IDE_SUCCESS );

                sNewMtrNode->flag &= ~QMC_MTR_HASH_NEED_MASK;
                sNewMtrNode->flag |= QMC_MTR_HASH_NEED_TRUE;

                sNewMtrNode->bucketCnt = sDistAggArg->bucketCnt;

                //connect
                if( (*aPlanDistNode) == NULL )
                {
                    *aPlanDistNode    = sNewMtrNode;
                }
                else
                {
                    for ( sDistNode = *aPlanDistNode;
                          sDistNode->next != NULL;
                          sDistNode = sDistNode->next ) ;
                                        
                    sDistNode->next = sNewMtrNode;
                }
                
                aMtrNode->myDist = sNewMtrNode;

                //�� Tuple�� �Ҵ�
                IDE_TEST(
                    qtc::allocIntermediateTuple(
                        aStatement ,
                        & QC_SHARED_TMPLATE(aStatement)->tmplate,
                        sDistTupleID ,
                        sDistColumnCount )
                    != IDE_SUCCESS);
                
                sMtcTemplate->rows[sDistTupleID].lflag
                    &= ~MTC_TUPLE_PLAN_MASK;
                sMtcTemplate->rows[sDistTupleID].lflag
                    |= MTC_TUPLE_PLAN_TRUE;

                sMtcTemplate->rows[aTupleID].lflag
                    &= ~MTC_TUPLE_PLAN_MTR_MASK;
                sMtcTemplate->rows[aTupleID].lflag
                    |= MTC_TUPLE_PLAN_MTR_TRUE;

                if( aQuerySet->materializeType == QMO_MATERIALIZE_TYPE_VALUE )
                {
                    if( ( aChildPlan->flag & QMN_PLAN_STORAGE_MASK )
                        == QMN_PLAN_STORAGE_DISK )
                    {            
                        sMtcTemplate->rows[sDistTupleID].lflag &= ~MTC_TUPLE_MATERIALIZE_MASK;
                        sMtcTemplate->rows[sDistTupleID].lflag |= MTC_TUPLE_MATERIALIZE_VALUE;
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
                        
                //GRAPH���� ������ �����ü�� ����Ѵ�.
                if( (aFlag & QMN_PLAN_STORAGE_MASK) ==
                    QMN_PLAN_STORAGE_MEMORY )                    
                {
                    sMtcTemplate->rows[sDistTupleID].lflag
                        &= ~MTC_TUPLE_STORAGE_MASK;
                    sMtcTemplate->rows[sDistTupleID].lflag
                        |= MTC_TUPLE_STORAGE_MEMORY;
                }
                else
                {
                    sMtcTemplate->rows[sDistTupleID].lflag
                        &= ~MTC_TUPLE_STORAGE_MASK;
                    sMtcTemplate->rows[sDistTupleID].lflag
                        |= MTC_TUPLE_STORAGE_DISK;
                }

                IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                                     sNewMtrNode )
                          != IDE_SUCCESS);

                if( ( sNewMtrNode->flag & QMC_MTR_TYPE_MASK ) == QMC_MTR_TYPE_CALCULATE )
                {
                    sNewMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
                    sNewMtrNode->flag |= QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE;
                }
                else
                {
                    // Nothing to do.
                }

                //passNode�� ����
                // qtc::columnModule�̰ų�
                // memory temp table�� ���� pass node�� ��������
                // �ʴ´�. �̶�, conversion�� NULL�̾�� �Ѵ�.
                if( ( ( sNewMtrNode->srcNode->node.module
                        == &(qtc::columnModule ) ) ||
                      ( ( sNewMtrNode->flag & QMC_MTR_TYPE_MASK )
                        == QMC_MTR_TYPE_MEMORY_KEY_COLUMN ) ||
                      ( ( sNewMtrNode->flag & QMC_MTR_TYPE_MASK )
                        == QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN ) )
                    &&
                    ( sNewMtrNode->srcNode->node.conversion == NULL ) )
                {
                    //memory Temp Table�� ����ǰ� Memory Column��
                    //�����ϰ�� myNode->dstNoode->node.argument��
                    //���� ��Ų��
                    if( ( ( sMtcTemplate
                            ->rows[sDistAggArg->aggArg->node.table]
                            .lflag
                            & MTC_TUPLE_STORAGE_MASK ) ==
                          MTC_TUPLE_STORAGE_MEMORY ) &&
                        ( ( sMtcTemplate->rows[sDistTupleID].lflag
                            & MTC_TUPLE_STORAGE_MASK ) ==
                          MTC_TUPLE_STORAGE_MEMORY ) )
                    {
                        //nothing to do
                    }
                    else
                    {
                        /* BUG-43698 GROUP_CONCAT()���� DISTINCT()�� next�� ����Ѵ�. */
                        sNewMtrNode->dstNode->node.next = aMtrNode->dstNode->node.arguments->next;

                        aMtrNode->dstNode->node.arguments =
                            (mtcNode *)sNewMtrNode->dstNode;
                        aAggrNode->node.arguments =
                            (mtcNode *)sNewMtrNode->dstNode;
                    }
                }
                else
                {
                    if( ( aQuerySet->materializeType
                          == QMO_MATERIALIZE_TYPE_VALUE )
                        &&
                        ( sMtcTemplate->rows[sNewMtrNode->dstNode->node.table].
                          lflag & MTC_TUPLE_STORAGE_MASK )
                        == MTC_TUPLE_STORAGE_DISK ) 
                    {
                        // Nothing To Do 
                    }
                    else
                    {
                        IDE_TEST( qtc::makePassNode( aStatement ,
                                                     NULL ,
                                                     sNewMtrNode->dstNode ,
                                                     &sPassNode )
                                  != IDE_SUCCESS );
                        /* BUG-45387 GROUP_CONCAT()���� DISTINCT()�� next�� ����Ѵ�. */
                        sMtcNode = aMtrNode->dstNode->node.arguments->next;

                        aMtrNode->dstNode->node.arguments =
                            (mtcNode *)sPassNode;

                        aAggrNode->node.arguments = (mtcNode *)sPassNode;

                        /* BUG-45387 GROUP_CONCAT()���� DISTINCT()�� next�� ����Ѵ�. */
                        sPassNode->node.next = sMtcNode;
                    }
                }
                break;
            }
            else
            {
                // nothing to do
            }
        }
    }

    *aDistNodeCount = sDistNodeCount;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmg::setDirection4SortColumn( qmgPreservedOrder  * aPreservedOrder,
                              UShort               aColumnID,
                              UInt               * aFlag )
{
/***********************************************************************
 *
 * Description : sort �÷������ÿ� ���� ������ �����Ѵ�.
 *
 *
 * Implementation :
 *
 *     - NOT DEFINE : ASC
 *     - ACS        : ASC
 *     - DESC       : DESC
 *     - SAME_PREV  : �հ� ������ ����.
 *     - DIFF_PREV  : �հ� ������ �ݴ�.
 *
 ***********************************************************************/

    qmgPreservedOrder * sPreservedOrder;
    UShort              sColumnCount;
    idBool              sIsASC = ID_TRUE;
    idBool              sIsNullsFirst = ID_FALSE;
    idBool              sIsNullsLast = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmg::setDirection4SortColumn::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------
    IDE_DASSERT( aPreservedOrder != NULL );

    for( sPreservedOrder = aPreservedOrder , sColumnCount = 0 ;
         sPreservedOrder != NULL && sColumnCount <= aColumnID ;
         sPreservedOrder = sPreservedOrder->next , sColumnCount++ )
    {
        switch( sPreservedOrder->direction )
        {
            case QMG_DIRECTION_NOT_DEFINED:
            case QMG_DIRECTION_ASC:
                sIsASC = ID_TRUE;
                break;
            case QMG_DIRECTION_DESC:
                sIsASC = ID_FALSE;
                break;
            case QMG_DIRECTION_SAME_WITH_PREV:
                //������ ����
                break;
            case QMG_DIRECTION_DIFF_WITH_PREV:
                //�ݴ� �����̴�.
                if( sIsASC == ID_TRUE )
                {
                    sIsASC = ID_FALSE;
                }
                else
                {
                    sIsASC = ID_TRUE;
                }
                break;
            case QMG_DIRECTION_ASC_NULLS_FIRST:
                sIsNullsFirst = ID_TRUE;
                break;
            case QMG_DIRECTION_ASC_NULLS_LAST:
                sIsNullsLast = ID_TRUE;
                break;
            case QMG_DIRECTION_DESC_NULLS_FIRST:
                sIsNullsFirst = ID_TRUE;
                break;
            case QMG_DIRECTION_DESC_NULLS_LAST:
                sIsNullsLast = ID_TRUE;
                break;
            default:
                IDE_DASSERT(0);
                break;
        }

    }

    if( sIsASC == ID_TRUE )
    {
        *aFlag &= ~QMC_MTR_SORT_ORDER_MASK;
        *aFlag |= QMC_MTR_SORT_ASCENDING;
    }
    else
    {
        *aFlag &= ~QMC_MTR_SORT_ORDER_MASK;
        *aFlag |= QMC_MTR_SORT_DESCENDING;
    }

    if ( sIsNullsFirst == ID_TRUE )
    {
        *aFlag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
        *aFlag |= QMC_MTR_SORT_NULLS_ORDER_FIRST;
    }
    else
    {
        /* Nothing to do */
    }

    if ( sIsNullsLast == ID_TRUE )
    {
        *aFlag &= ~QMC_MTR_SORT_NULLS_ORDER_MASK;
        *aFlag |= QMC_MTR_SORT_NULLS_ORDER_LAST;
    }
    else
    {
        /* Nothing to do */
    }
    
    return IDE_SUCCESS;

}

IDE_RC
qmg::makeOuterJoinFilter(qcStatement   * aStatement,
                         qmsQuerySet   * aQuerySet,
                         qmoPredicate ** aPredicate ,
                         qtcNode       * aNnfFilter,
                         qtcNode      ** aFilter)
{
/***********************************************************************
 *
 * Description : Outer Join���� ���� Filter�� �����Ѵ�
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate    * sOnFirstPredicate = NULL;
    qmoPredicate    * sOnLastPredicate = NULL;
    UInt              sIndex;

    IDU_FIT_POINT_FATAL( "qmg::makeOuterJoinFilter::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //---------------------------------------------------
    // To Fix BUG-10988
    // on condition CNF�� constant predicate, one table predicate,
    // nonJoinable Predicate�� ����
    //
    // - Input : aPredicate
    //   aPredicate[0] : constant predicate�� pointer
    //   aPredicate[1] : one table predicate�� pointer
    //   aPredicate[2] : join predicate�� pointer
    //   -------------------
    //  |     |      |      |
    //   ---|-----|------|--
    //      |     |       -->p1->p2
    //      |     ---> q1->q2
    //      ----> r1->r2
    //
    // - Output : r1->r2->q1->q2->p1->p2
    //---------------------------------------------------

    for( sIndex = 0 ; sIndex < 3 ; sIndex++ )
    {
        if ( sOnLastPredicate == NULL )
        {
            sOnFirstPredicate = sOnLastPredicate = aPredicate[sIndex];
        }
        else
        {
            sOnLastPredicate->next = aPredicate[sIndex];
        }

        if ( sOnLastPredicate != NULL )
        {
            // constant predicate, one table predicate, nonjoinable predicate
            // ������ �ΰ� �̻��� predicate ���� ���,
            // last predicate���� �̵��� ���� ������ �ι�° �̻��� predicate��
            // �Ҿ������ ��
            for ( ; sOnLastPredicate->next != NULL;
                  sOnLastPredicate = sOnLastPredicate->next ) ;
        }
        else
        {
            // nothing to do
        }
    }

    if( sOnFirstPredicate != NULL )
    {
        IDE_TEST( qmoPred::linkFilterPredicate( aStatement ,
                                                sOnFirstPredicate ,
                                                aFilter ) != IDE_SUCCESS );

        // BUG-35155 Partial CNF
        if ( aNnfFilter != NULL )
        {
            IDE_TEST( qmoPred::addNNFFilter4linkedFilter(
                          aStatement,
                          aNnfFilter,
                          aFilter )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        if ( aNnfFilter != NULL )
        {
            *aFilter = aNnfFilter;
        }
        else
        {
            *aFilter = NULL;
        }
    }

    if ( *aFilter != NULL )
    {
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           *aFilter ) != IDE_SUCCESS );
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
qmg::removeSDF( qcStatement * aStatement, qmgGraph * aGraph )
{
    qmgChildren * sChildren;

    IDU_FIT_POINT_FATAL( "qmg::removeSDF::__FT__" );

    if( aGraph->left != NULL )
    {
        IDE_TEST( removeSDF( aStatement, aGraph->left )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do...
    }

    if( aGraph->right != NULL )
    {
        IDE_TEST( removeSDF( aStatement, aGraph->right )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do...
    }

    // BUG-33132
    if ( aGraph->type == QMG_FULL_OUTER_JOIN )
    {
        if ( ((qmgFOJN*)aGraph)->antiLeftGraph != NULL )
        {
            IDE_TEST( removeSDF( aStatement, ((qmgFOJN*)aGraph)->antiLeftGraph )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do 
        }

        if ( ((qmgFOJN*)aGraph)->antiRightGraph != NULL )
        {
            IDE_TEST( removeSDF( aStatement, ((qmgFOJN*)aGraph)->antiRightGraph )
                      != IDE_SUCCESS );
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

    // fix BUG-19147
    // aGraph�� QMG_PARTITION�� ���, aGraph->children�� �޸���.
    for( sChildren = aGraph->children;
         sChildren != NULL;
         sChildren = sChildren->next )
    {
        IDE_TEST( removeSDF( aStatement, sChildren->childGraph )
                  != IDE_SUCCESS );
    }

    if( (aGraph->left == NULL) && (aGraph->right == NULL) && 
        (aGraph->children == NULL) && (aGraph->type == QMG_SELECTION) )
    {
        if( ((qmgSELT*)aGraph)->sdf != NULL )
        {
            IDE_TEST( qtc::removeSDF( aStatement,
                                      ((qmgSELT*)aGraph)->sdf )
                      != IDE_SUCCESS );
            ((qmgSELT*)aGraph)->sdf = NULL;
        }
        else
        {
            // Nothing to do...
        }
    }
    else
    {
        // Nothing to do...
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::makeLeftHASH4Join( qcStatement  * aStatement,
                        qmgGraph     * aGraph,
                        UInt           aMtrFlag,
                        UInt           aJoinFlag,
                        qtcNode      * aFilter,
                        qmnPlan      * aChild,
                        qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::makeLeftHASH4Join::__FT__" );

    if ( ( aJoinFlag &
           QMO_JOIN_METHOD_LEFT_STORAGE_MASK )
         == QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY )
    {
        aMtrFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKEHASH_MEMORY_TEMP_TABLE;
    }
    else
    {
        aMtrFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKEHASH_DISK_TEMP_TABLE;
    }

    switch( aGraph->type )
    {
        case QMG_INNER_JOIN:
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgJOIN*)aGraph)->hashTmpTblCnt ,
                          ((qmgJOIN*)aGraph)->hashBucketCnt ,
                          aFilter ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        case QMG_LEFT_OUTER_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgLOJN*)aGraph)->hashTmpTblCnt ,
                          ((qmgLOJN*)aGraph)->hashBucketCnt ,
                          aFilter ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        case QMG_FULL_OUTER_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgFOJN*)aGraph)->hashTmpTblCnt ,
                          ((qmgFOJN*)aGraph)->hashBucketCnt ,
                          aFilter ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        default:
            IDE_FT_ASSERT( 0 );
    }

    qmg::setPlanInfo( aStatement, aPlan, aGraph );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::makeRightHASH4Join( qcStatement  * aStatement,
                         qmgGraph     * aGraph,
                         UInt           aMtrFlag,
                         UInt           aJoinFlag,
                         qtcNode      * aRange,
                         qmnPlan      * aChild,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description :
 *
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::makeRightHASH4Join::__FT__" );

    if ( ( aJoinFlag &
           QMO_JOIN_METHOD_RIGHT_STORAGE_MASK )
         == QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY )
    {
        aMtrFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKEHASH_MEMORY_TEMP_TABLE;
    }
    else
    {
        aMtrFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKEHASH_DISK_TEMP_TABLE;
    }

    switch( aGraph->type )
    {
        case QMG_INNER_JOIN:
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgJOIN*)aGraph)->hashTmpTblCnt ,
                          ((qmgJOIN*)aGraph)->hashBucketCnt ,
                          aRange ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        case QMG_LEFT_OUTER_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgLOJN*)aGraph)->hashTmpTblCnt ,
                          ((qmgLOJN*)aGraph)->hashBucketCnt ,
                          aRange ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        case QMG_FULL_OUTER_JOIN:
            IDE_TEST( qmoOneMtrPlan::makeHASH(
                          aStatement ,
                          aGraph->myQuerySet ,
                          aMtrFlag ,
                          ((qmgFOJN*)aGraph)->hashTmpTblCnt ,
                          ((qmgFOJN*)aGraph)->hashBucketCnt ,
                          aRange ,
                          aChild ,
                          aPlan,
                          ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            break;
        default:
            IDE_FT_ASSERT( 0 );
    }

    qmg::setPlanInfo( aStatement, aPlan, aGraph );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::initLeftSORT4Join( qcStatement  * aStatement,
                        qmgGraph     * aGraph,
                        UInt           aJoinFlag,
                        qtcNode      * aRange,
                        qmnPlan      * aParent,
                        qmnPlan     ** aPlan )
{

    IDU_FIT_POINT_FATAL( "qmg::initLeftSORT4Join::__FT__" );

    switch( aJoinFlag & QMO_JOIN_LEFT_NODE_MASK )
    {
        case QMO_JOIN_LEFT_NODE_NONE:
            *aPlan = aParent;
            break;
        case QMO_JOIN_LEFT_NODE_STORE:
        case QMO_JOIN_LEFT_NODE_SORTING:
            if ( ( ( aGraph->myQuerySet->SFWGH->lflag & QMV_SFWGH_JOIN_MASK )
                   == QMV_SFWGH_JOIN_RIGHT_OUTER ) ||
                 ( ( aGraph->myQuerySet->SFWGH->lflag & QMV_SFWGH_JOIN_MASK )
                   == QMV_SFWGH_JOIN_FULL_OUTER ) )
            {
                qmc::disableSealTrueFlag( aParent->resultDesc );
            }
            else
            {
                /* Nothing to do */
            }
            IDE_TEST( qmoOneMtrPlan::initSORT(
                          aStatement,
                          aGraph->myQuerySet,
                          aRange,
                          ID_TRUE,
                          ID_FALSE,
                          aParent,
                          aPlan )
                      != IDE_SUCCESS );
            break;
        default:
            IDE_DASSERT(0);
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::makeLeftSORT4Join( qcStatement  * aStatement,
                        qmgGraph     * aGraph,
                        UInt           aMtrFlag,
                        UInt           aJoinFlag,
                        qmnPlan      * aChild,
                        qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : Sort Join��, Join�� ���ʿ� ��ġ�� SORT plan node ����
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::makeLeftSORT4Join::__FT__" );

    if ( ( aJoinFlag & QMO_JOIN_METHOD_LEFT_STORAGE_MASK )
         == QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY )
    {
        aMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
    }
    else
    {
        aMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
    }

    switch ( aJoinFlag & QMO_JOIN_LEFT_NODE_MASK )
    {
        case QMO_JOIN_LEFT_NODE_NONE :
            // �ƹ��� ��嵵 �������� �ʴ´�.
            break;
        case QMO_JOIN_LEFT_NODE_STORE :

            aMtrFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
            aMtrFlag |= QMO_MAKESORT_PRESERVED_TRUE;

            IDE_TEST(
                qmoOneMtrPlan::makeSORT( aStatement ,
                                         aGraph->myQuerySet ,
                                         aMtrFlag ,
                                         aGraph->left->preservedOrder ,
                                         aChild ,
                                         aGraph->costInfo.inputRecordCnt,
                                         aPlan ) != IDE_SUCCESS);

            aGraph->myPlan = aPlan;

            qmg::setPlanInfo( aStatement, aPlan, aGraph );

            break;
        case QMO_JOIN_LEFT_NODE_SORTING :

            aMtrFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
            aMtrFlag |= QMO_MAKESORT_PRESERVED_FALSE;

            IDE_TEST(
                qmoOneMtrPlan::makeSORT( aStatement ,
                                         aGraph->myQuerySet ,
                                         aMtrFlag ,
                                         aGraph->left->preservedOrder ,
                                         aChild ,
                                         aGraph->costInfo.inputRecordCnt,
                                         aPlan ) != IDE_SUCCESS);

            aGraph->myPlan = aPlan;

            qmg::setPlanInfo( aStatement, aPlan, aGraph );

            break;
        default:
            IDE_DASSERT(0);
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::initRightSORT4Join( qcStatement  * aStatement,
                         qmgGraph     * aGraph,
                         UInt           aJoinFlag,
                         idBool         aOrderCheckNeed,
                         qtcNode      * aRange,
                         idBool         aIsRangeSearch,
                         qmnPlan      * aParent,
                         qmnPlan     ** aPlan )
{

    IDU_FIT_POINT_FATAL( "qmg::initRightSORT4Join::__FT__" );

    if( aOrderCheckNeed == ID_TRUE )
    {
        switch( aJoinFlag & QMO_JOIN_RIGHT_NODE_MASK )
        {
            case QMO_JOIN_RIGHT_NODE_NONE :
                if( ( aGraph->type == QMG_INNER_JOIN ) ||
                    ( aGraph->type == QMG_SEMI_JOIN ) ||
                    ( aGraph->type == QMG_ANTI_JOIN ) )
                {
                    IDE_TEST_RAISE(
                        ((qmgJOIN*)aGraph)->selectedJoinMethod == NULL,
                        ERR_INVALID_INNER_JOIN_METHOD );
                    IDE_TEST_RAISE(
                        ((((qmgJOIN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_ONE_PASS_SORT) ||
                        ((((qmgJOIN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_TWO_PASS_SORT),
                        ERR_INVALID_INNER_JOIN_METHOD_SORT );
                }
                else if( aGraph->type == QMG_LEFT_OUTER_JOIN )
                {
                    IDE_TEST_RAISE(
                        ((qmgLOJN*)aGraph)->selectedJoinMethod == NULL,
                        ERR_INVALID_LEFT_OUTER_JOIN_METHOD );
                    IDE_TEST_RAISE(
                        ((((qmgLOJN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_ONE_PASS_SORT) ||
                        ((((qmgLOJN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_TWO_PASS_SORT),
                        ERR_INVALID_LEFT_OUTER_JOIN_METHOD_SORT );
                }
                else if( aGraph->type == QMG_FULL_OUTER_JOIN )
                {
                    IDE_TEST_RAISE(
                        ((qmgFOJN*)aGraph)->selectedJoinMethod == NULL,
                        ERR_INVALID_FULL_OUTER_JOIN_METHOD );
                    IDE_TEST_RAISE(
                        ((((qmgFOJN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_ONE_PASS_SORT) ||
                        ((((qmgFOJN*)aGraph)->selectedJoinMethod->flag &
                          QMO_JOIN_METHOD_MASK)
                         == QMO_JOIN_METHOD_TWO_PASS_SORT),
                        ERR_INVALID_FULL_OUTER_JOIN_METHOD_SORT );
                }
                *aPlan = aParent;
                break;
            case QMO_JOIN_RIGHT_NODE_STORE:
            case QMO_JOIN_RIGHT_NODE_SORTING:
                qmc::disableSealTrueFlag( aParent->resultDesc );
                IDE_TEST( qmoOneMtrPlan::initSORT(
                              aStatement,
                              aGraph->myQuerySet,
                              aRange,
                              ID_FALSE,
                              aIsRangeSearch,
                              aParent,
                              aPlan )
                          != IDE_SUCCESS );
                break;
            default:
                IDE_DASSERT(0);
                break;
        }
    }
    else
    {
        IDE_TEST( qmoOneMtrPlan::initSORT(
                      aStatement,
                      aGraph->myQuerySet,
                      aRange,
                      ID_FALSE,
                      aIsRangeSearch,
                      aParent,
                      aPlan )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_INNER_JOIN_METHOD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "INNER JOIN: selectedJoinMethod is NULL" ));
    }
    IDE_EXCEPTION( ERR_INVALID_INNER_JOIN_METHOD_SORT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "INNER JOIN: selectedJoinMethod is SORT" ));
    }
    IDE_EXCEPTION( ERR_INVALID_LEFT_OUTER_JOIN_METHOD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "LEFT OUTER JOIN: selectedJoinMethod is NULL" ));
    }
    IDE_EXCEPTION( ERR_INVALID_LEFT_OUTER_JOIN_METHOD_SORT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "LEFT OUTER JOIN: selectedJoinMethod is SORT" ));
    }
    IDE_EXCEPTION( ERR_INVALID_FULL_OUTER_JOIN_METHOD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "FULL OUTER JOIN: selectedJoinMethod is NULL" ));
    }
    IDE_EXCEPTION( ERR_INVALID_FULL_OUTER_JOIN_METHOD_SORT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeRightSORT4Join",
                                  "FULL OUTER JOIN: selectedJoinMethod is SORT" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::makeRightSORT4Join( qcStatement  * aStatement,
                         qmgGraph     * aGraph,
                         UInt           aMtrFlag,
                         UInt           aJoinFlag,
                         idBool         aOrderCheckNeed,
                         qmnPlan      * aChild,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : Sort Join��, Join�� �����ʿ� ��ġ�� SORT plan node ����
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::makeRightSORT4Join::__FT__" );
    
    //���� ��ü�� ����
    if ( ( aJoinFlag &
           QMO_JOIN_METHOD_RIGHT_STORAGE_MASK )
         == QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY )
    {
        aMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
    }
    else
    {
        aMtrFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        aMtrFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
    }

    if( aOrderCheckNeed == ID_TRUE )
    {
        switch ( aJoinFlag & QMO_JOIN_RIGHT_NODE_MASK )
        {
            case QMO_JOIN_RIGHT_NODE_NONE :
                break;
            case QMO_JOIN_RIGHT_NODE_STORE :

                aMtrFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
                aMtrFlag |= QMO_MAKESORT_PRESERVED_TRUE;

                IDE_TEST(
                    qmoOneMtrPlan::makeSORT( aStatement ,
                                             aGraph->myQuerySet ,
                                             aMtrFlag ,
                                             aGraph->right->preservedOrder ,
                                             aChild ,
                                             aGraph->costInfo.inputRecordCnt,
                                             aPlan )
                    != IDE_SUCCESS);
                aGraph->myPlan = aPlan;

                qmg::setPlanInfo( aStatement, aPlan, aGraph );

                break;
            case QMO_JOIN_RIGHT_NODE_SORTING :

                aMtrFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
                aMtrFlag |= QMO_MAKESORT_PRESERVED_FALSE;

                IDE_TEST(
                    qmoOneMtrPlan::makeSORT( aStatement ,
                                             aGraph->myQuerySet ,
                                             aMtrFlag ,
                                             NULL ,
                                             aChild ,
                                             aGraph->costInfo.inputRecordCnt,
                                             aPlan )
                    != IDE_SUCCESS);

                aGraph->myPlan = aPlan;

                qmg::setPlanInfo( aStatement, aPlan, aGraph );

                break;
            default:
                IDE_DASSERT(0);
                break;
        }
    }
    else
    {
        IDE_TEST( qmoOneMtrPlan::makeSORT( aStatement ,
                                           aGraph->myQuerySet ,
                                           aMtrFlag ,
                                           NULL ,
                                           aChild ,
                                           aGraph->costInfo.inputRecordCnt,
                                           aPlan ) != IDE_SUCCESS);

        qmg::setPlanInfo( aStatement, aPlan, aGraph );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmg::usableIndexScanHint( qcmIndex            * aHintIndex,
                          qmoTableAccessType    aHintAccessType,
                          qmoIdxCardInfo      * aIdxCardInfo,
                          UInt                  aIdxCnt,
                          qmoIdxCardInfo     ** aSelectedIdxInfo,
                          idBool              * aUsableHint )
{
/***********************************************************************
 *
 * Description : ��� ������ INDEX SCAN Hint���� �˻�
 *
 * Implementation :
 *    (1) Hint Index�� �����ϴ��� �˻�
 *    (2) �ش� index�� �̹� �ٸ� Hint�� ����Ǿ����� �˻�
 *
 ***********************************************************************/

    qmoIdxCardInfo * sSelected = NULL;
    idBool           sUsableIndexHint = ID_FALSE;
    UInt             i;

    IDU_FIT_POINT_FATAL( "qmg::usableIndexScanHint::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    //---------------------------------------------------
    // �ش� Index�� �����ϴ��� �˻�
    //---------------------------------------------------

    if( aHintIndex != NULL )
    {
        for ( i = 0; i < aIdxCnt; i++ )
        {
            // PROJ-1502 PARTITIONED DISK TABLE
            // index �����ͷ� ������ �ʰ� index id�� ���� ������ ���Ѵ�.
            // �� ������, partitioned table�� index�� hint�� �־��� ��
            // partition�� index�� ���õǾ�� �ϱ� �����̴�.
            if ( aIdxCardInfo[i].index->indexId == aHintIndex->indexId )
            {
                // Index Hint�� ������ index�� �����ϴ� ���
                if ( aIdxCardInfo[i].index->isOnlineTBS == ID_TRUE )
                {
                    sSelected = & aIdxCardInfo[i];
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
        }
    }

    if ( sSelected != NULL )
    {
        //---------------------------------------------------
        // �ش� Index�� ������ ���
        //---------------------------------------------------

        if ( ( sSelected->flag & QMO_STAT_CARD_IDX_HINT_MASK) ==
             QMO_STAT_CARD_IDX_HINT_NONE )
        {
            //---------------------------------------------------
            // �ش� Index�� ���� ������ �ٸ� Hint�� ������ ���
            //---------------------------------------------------

            switch ( aHintAccessType )
            {
                case QMO_ACCESS_METHOD_TYPE_NO_INDEX_SCAN :
                    sSelected->flag &= ~QMO_STAT_CARD_IDX_HINT_MASK;
                    sSelected->flag |= QMO_STAT_CARD_IDX_NO_INDEX;

                    // �ǹ̻����� ��� ������ Hint���� No Index Hint�� ���,
                    // �ش� index�� access method ������ �������� �ʱ� ����
                    sUsableIndexHint = ID_FALSE;
                    break;
                case QMO_ACCESS_METHOD_TYPE_INDEXACCESS_SCAN :
                    sSelected->flag &= ~QMO_STAT_CARD_IDX_HINT_MASK;
                    sSelected->flag |= QMO_STAT_CARD_IDX_INDEX;
                    sUsableIndexHint = ID_TRUE;
                    break;
                case QMO_ACCESS_METHOD_TYPE_INDEX_ASC_SCAN :
                    sSelected->flag &= ~QMO_STAT_CARD_IDX_HINT_MASK;
                    sSelected->flag |= QMO_STAT_CARD_IDX_INDEX_ASC;
                    sUsableIndexHint = ID_TRUE;
                    break;
                case QMO_ACCESS_METHOD_TYPE_INDEX_DESC_SCAN :
                    sSelected->flag &= ~QMO_STAT_CARD_IDX_HINT_MASK;
                    sSelected->flag |= QMO_STAT_CARD_IDX_INDEX_DESC;
                    sUsableIndexHint = ID_TRUE;
                    break;
                default :
                    IDE_DASSERT( 0 );
                    break;
            }
        }
        else
        {
            // ������ �ٸ� Hint�� �������� ���
            sUsableIndexHint = ID_FALSE;
        }
    }
    else
    {
        // �ش� Index�� �������� ���� ���
        sUsableIndexHint = ID_FALSE;
    }

    *aSelectedIdxInfo = sSelected;
    *aUsableHint = sUsableIndexHint;

    return IDE_SUCCESS;
}

IDE_RC qmg::resetColumnLocate( qcStatement * aStatement, UShort aTupleID )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-2179
 *    tuple-set�� columnLocate�� ������ �ʱ���·� �ǵ�����.
 *    Disjunctive query ��� �� CONCATENATOR�� child���� ���� ��
 *    left ������ right ���� �� ȣ��ȴ�.
 *    Left���� HASH���� �����Ǵ� ��� ���� table�� SCAN ID�� �ƴ� HASH��
 *    ID�� right���� �� �߸� ������ �� �ֱ� �����̴�.
 *
 *    BUG-37324
 *    �ܺ� ���� �÷��� ���ؼ��� ������ø� reset �ϸ� �ȵ�
 * Implementation :
 *    �Է¹��� aTupleID �� columnLocate�� �Ҵ�Ǿ��ִ� ���
 *    �ʱⰪ���� �ǵ�����.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::resetColumnLocate::__FT__" );

    mtcTemplate * sMtcTemplate;
    UShort        i;

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    for( i = 0; i < sMtcTemplate->rows[aTupleID].columnCount; i++ )
    {
        if( sMtcTemplate->rows[aTupleID].columnLocate != NULL )
        {
            sMtcTemplate->rows[aTupleID].columnLocate[i].table  = aTupleID;
            sMtcTemplate->rows[aTupleID].columnLocate[i].column = i;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC qmg::setColumnLocate( qcStatement * aStatement,
                             qmcMtrNode  * aMtrNode )
{
    qmcMtrNode        * sLastMtrNode;
    qmcMtrNode        * sMtrNode;
    mtcTemplate       * sMtcTemplate;
    qcTemplate        * sQcTemplate;
    qtcNode           * sConversionNode;
    idBool              sPushProject;

    IDU_FIT_POINT_FATAL( "qmg::setColumnLocate::__FT__" );

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;
    sQcTemplate  = QC_SHARED_TMPLATE(aStatement);
    
    IDE_FT_ASSERT( aMtrNode != NULL );

    for( sLastMtrNode = aMtrNode;
         sLastMtrNode != NULL;
         sLastMtrNode = sLastMtrNode->next )
    {
        for( sMtrNode = aMtrNode;
             sMtrNode != NULL;
             sMtrNode = sMtrNode->next )
        {
            if( ( sMtrNode->flag & QMC_MTR_CHANGE_COLUMN_LOCATE_MASK )
                == QMC_MTR_CHANGE_COLUMN_LOCATE_TRUE )
            {
                if( ( sMtrNode->srcNode->node.table ==
                      sLastMtrNode->srcNode->node.table )
                    &&
                    ( sMtrNode->srcNode->node.column ==
                      sLastMtrNode->srcNode->node.column ) )
                {
                    break;            
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
        }

        sConversionNode =
            (qtcNode*)mtf::convertedNode(
                &(sLastMtrNode->srcNode->node),
                &(QC_SHARED_TMPLATE(aStatement)->tmplate) );

        if( sLastMtrNode->srcNode == sConversionNode )
        {
            if( sLastMtrNode != sMtrNode ) 
            {
                // Nothing to do.
            }
            else
            {
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].
                    columnLocate[sLastMtrNode->srcNode->node.column].table
                    = sLastMtrNode->dstNode->node.table;                
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].      
                    columnLocate[sLastMtrNode->srcNode->node.column].column
                    = sLastMtrNode->dstNode->node.column;
            }
        }
        else
        {
            //------------------------------------------------------
            // srcNode�� conversion node�� �־� �� ���� ����
            // dstNode�� �����ϴ� ����,
            // ���� ��忡�� �� ����� ���� �̿��ϵ��� �ؾ� ��.
            // base column�� column locate�� �����ϸ� �ȵ�.
            // �� ) TC/Server/qp4/Bugs/PR-13286/PR-13286.sql
            //      SELECT /*+ USE_ONE_PASS_SORT( D2, D1 ) */
            //      * FROM D1, D2
            //      WHERE D1.I1 > D2.I1 AND D1.I1 < D2.I1 + '10';
            //------------------------------------------------------

            if( sLastMtrNode != sMtrNode )
            {
                // Nothing To Do 
            }
            else
            {
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].
                    columnLocate[sLastMtrNode->srcNode->node.column].table
                    = sLastMtrNode->dstNode->node.table;                
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].
                    columnLocate[sLastMtrNode->srcNode->node.column].column
                    = sLastMtrNode->dstNode->node.column;
            }                

            sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].
                columns[sLastMtrNode->dstNode->node.column].column.flag
                &= ~SMI_COLUMN_TYPE_MASK;
            sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].
                columns[sLastMtrNode->dstNode->node.column].column.flag
                |= SMI_COLUMN_TYPE_FIXED;

            // BUG-38494
            // Compressed Column ���� �� ��ü�� ����ǹǷ�
            // Compressed �Ӽ��� �����Ѵ�
            sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].
                columns[sLastMtrNode->dstNode->node.column].column.flag
                &= ~SMI_COLUMN_COMPRESSION_MASK;

            sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].
                columns[sLastMtrNode->dstNode->node.column].column.flag
                |= SMI_COLUMN_COMPRESSION_FALSE;        
        }        

        // fix BUG-22068
        if( ( ( sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].lflag
                & MTC_TUPLE_TYPE_MASK ) == MTC_TUPLE_TYPE_TABLE ) ||
            ( ( sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].lflag
                & MTC_TUPLE_VIEW_MASK ) == MTC_TUPLE_VIEW_TRUE ) )
        {
            // TABLE type
            // RID ��İ� Push Projection ����� �Բ� ������
            // ���� ���̺��� ���� ����� �ٲ��� �ʴ´�. ( BUG-22068, BUG-31873 )
            // ex ) RID ��� t1, Push Projection ��� t2
            //      SELECT t1.i1, t2.i1 FROM t1, t2 ORDER BY t1.i1, t2.i1;
            //      ORDER BY ó���� ���Ͽ� �߰� ����� �����Ҷ�,
            //      t1 ���̺��� Į������ RID �������
            //      t2 ���̺��� Į������ Push Projection ������� ����ȴ�.
        }
        else
        {
            // CONSTANT, VARIABLE, INTERMEDIATE type
            // ���� tuple���� RID Ÿ������ �ʱ�ȭ�ȴ�.
            // ������ Push Projection ����϶��� �� tuple�� Ÿ�Ե� Push Projection
            // Ÿ������ �����Ѵ�.
            
            // BUG-28212
            // sysdate, unix_date, current_date Ÿ���� statement�� tmplate�� �����ȴ�.
            // �׸��� �� ���� RID Ÿ������ �����ؾ� �Ѵ�.

            sPushProject = ID_TRUE;

            if( sQcTemplate->unixdate != NULL )
            {
                if( sQcTemplate->unixdate->node.table ==
                    sLastMtrNode->srcNode->node.table )
                {
                    sPushProject = ID_FALSE;
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

            if( sQcTemplate->sysdate != NULL )
            {
                if( sQcTemplate->sysdate->node.table ==
                    sLastMtrNode->srcNode->node.table )
                {
                    sPushProject = ID_FALSE;
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

            if( sQcTemplate->currentdate != NULL )
            {
                if( sQcTemplate->currentdate->node.table ==
                    sLastMtrNode->srcNode->node.table )
                {
                    sPushProject = ID_FALSE;
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

            if ( sPushProject == ID_TRUE )
            {
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].lflag
                    &= ~MTC_TUPLE_MATERIALIZE_MASK;
                sMtcTemplate->rows[sLastMtrNode->srcNode->node.table].lflag
                    |= MTC_TUPLE_MATERIALIZE_VALUE;
            }
            else
            {
                /* Nothing to do. */
            }
        }

        sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].lflag
            &= ~MTC_TUPLE_MATERIALIZE_MASK;
        sMtcTemplate->rows[sLastMtrNode->dstNode->node.table].lflag
            |= MTC_TUPLE_MATERIALIZE_VALUE;
    }
    
    return IDE_SUCCESS;
    
}

IDE_RC qmg::changeColumnLocate( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qtcNode      * aNode,
                                UShort         aTupleID,
                                idBool         aNext )
{
/***********************************************************************
 *
 * Description : validation�� ������ ����� �⺻��ġ������
 *               ������ �����ؾ� �� ����� ��ġ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode        * sNode;
    UShort           sOrgTable;
    UShort           sOrgColumn;
    UShort           sChangeTable;
    UShort           sChangeColumn;

    IDU_FIT_POINT_FATAL( "qmg::changeColumnLocate::__FT__" );

    if( aNode == NULL )        
    {
        // Nothing To Do
    }
    else
    {
        if( ( aNode->node.lflag & MTC_NODE_COLUMN_LOCATE_CHANGE_MASK )
            == MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE )
        {
            // Nothing To Do
        }
        else
        {
            if( aNext == ID_FALSE )        
            {
                //---------------------------------------------------
                // target column���� node ó����
                // node->next �� ������ �ʵ��� �Ѵ�.
                //---------------------------------------------------
        
                if( aNode->node.arguments != NULL )
                {
                    sNode = (qtcNode*)(aNode->node.arguments);
                }
                else
                {
                    sNode = NULL;
                }        
            }
            else
            {
                sNode = aNode;        
            }

            for( ; sNode != NULL; sNode = (qtcNode*)(sNode->node.next) )
            {
                if( ( sNode->node.lflag & MTC_NODE_COLUMN_LOCATE_CHANGE_MASK )
                    == MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE )
                {
                    // BUG-43723 disk table���� merge join ���� ����� �ٸ��ϴ�.
                    // MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE ���� next�� Ȯ���ؾ� �Ѵ�.
                }
                else
                {
                    IDE_TEST( changeColumnLocateInternal( aStatement,
                                                          aQuerySet,
                                                          sNode,
                                                          aTupleID )
                              != IDE_SUCCESS );
                }
            }

            if( aNext == ID_FALSE )        
            {
                /*
                 * BUG-39605
                 * procedure variable �� ��쿡�� changeColumnLocate ���� �ʴ´�.
                 */
                if ((aNode->node.column != MTC_RID_COLUMN_ID) &&
                    (aNode->node.table != QC_SHARED_TMPLATE(aStatement)->tmplate.variableRow))
                {
                    sOrgTable = aNode->node.table;
                    sOrgColumn = aNode->node.column;

                    IDE_TEST(findColumnLocate(aStatement,
                                              aQuerySet->parentTupleID,
                                              aTupleID,
                                              sOrgTable,
                                              sOrgColumn,
                                              &sChangeTable,
                                              &sChangeColumn)
                             != IDE_SUCCESS);        

                    aNode->node.table = sChangeTable;
                    aNode->node.column = sChangeColumn;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Nothing To Do 
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}


IDE_RC qmg::changeColumnLocateInternal( qcStatement  * aStatement,
                                        qmsQuerySet  * aQuerySet,
                                        qtcNode      * aNode,
                                        UShort         aTupleID )
{
/***********************************************************************
 *
 * Description : validation�� ������ ����� �⺻��ġ������
 *               ������ �����ؾ� �� ����� ��ġ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode     * sNode;
    UShort        sOrgTable;
    UShort        sOrgColumn;
    UShort        sChangeTable;
    UShort        sChangeColumn;

    IDU_FIT_POINT_FATAL( "qmg::changeColumnLocateInternal::__FT__" );

    for( sNode = (qtcNode*)(aNode->node.arguments);
         sNode != NULL;
         sNode = (qtcNode*)(sNode->node.next) )
    {
        if( ( sNode->node.lflag & MTC_NODE_COLUMN_LOCATE_CHANGE_MASK )
            == MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE )
        {
            // BUG-43723 disk table���� merge join ���� ����� �ٸ��ϴ�.
            // MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE ���� next�� Ȯ���ؾ� �Ѵ�.
        }
        else
        {
            IDE_TEST( changeColumnLocateInternal( aStatement,
                                                  aQuerySet,
                                                  sNode,
                                                  aTupleID )
                      != IDE_SUCCESS );
        }
    }

    /*
     * BUG-39605
     * procedure variable �� ��쿡�� changeColumnLocate ���� �ʴ´�.
     */
    if ((aNode->node.column != MTC_RID_COLUMN_ID) &&
        (aNode->node.table != QC_SHARED_TMPLATE(aStatement)->tmplate.variableRow))
    {
        sOrgTable = aNode->node.table;
        sOrgColumn = aNode->node.column;

        IDE_TEST( findColumnLocate( aStatement,
                                    aQuerySet->parentTupleID,
                                    aTupleID,
                                    sOrgTable,
                                    sOrgColumn,
                                    & sChangeTable,
                                    & sChangeColumn )
                  != IDE_SUCCESS );

        aNode->node.table = sChangeTable;
        aNode->node.column = sChangeColumn;
    }
    else
    {
        // Nothing to do.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

IDE_RC qmg::findColumnLocate( qcStatement  * aStatement,
                              UInt           aParentTupleID,
                              UShort         aTableID,
                              UShort         aOrgTable,
                              UShort         aOrgColumn,
                              UShort       * aChangeTable,
                              UShort       * aChangeColumn )
{
/***********************************************************************
 *
 * Description : 
 *    PROJ-2179
 *    ������ findColumnLocate�� �޸� �߰� ���ڷ� aParentTupleID�� ���´�.
 *    Subquery�� execution plan���� ���� operator�� materialize�� ����
 *    �����ϴ°��� �����ϵ��� �Ѵ�
 *
 *    Ex) SELECT i1, (SELECT COUNT(*) FROM t2 WHERE t2.i1 = t1.i1)
 *            FROM t1 ORDER BY 2, 1;
 *        * t1, t2�� ��� disk table
 *
 *        �� SQL������ ��� outerquery���� SORT�� �����ϰ� sort����
 *        t3.i1�� subquery�� ����� materialize�Ѵ�.
 *        �� �� subquery�� WHERE������ t1.i1�� outerquery�� SORT����
 *        materialize�� ��ġ�� ����Ű�� �ʵ��� SORT�� ID�� aParentTupleID
 *        �� �������ش�.
 *
 * Implementation :
 *    aParentTupleID�� columnLocate���� ����Ű�� ��� �̸� �����ϰ�
 *    �������� ã�� ��ġ�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    UShort        sTable;
    UShort        sColumn;

    IDU_FIT_POINT_FATAL( "qmg::findColumnLocate::__FT__" );

    if( aOrgTable != aTableID )
    {
        sTable = QC_SHARED_TMPLATE(aStatement)->tmplate.
            rows[aOrgTable].columnLocate[aOrgColumn].table;
        sColumn = QC_SHARED_TMPLATE(aStatement)->tmplate.
            rows[aOrgTable].columnLocate[aOrgColumn].column;

        if( ( sTable < aTableID ) &&
            ( aParentTupleID != sTable ) )
        {
            if( ( sTable == aOrgTable ) &&
                ( sColumn == aOrgColumn ) )
            {
                *aChangeTable = sTable;
                *aChangeColumn = sColumn;
            }
            else
            {
                IDE_TEST( findColumnLocate( aStatement,
                                            aTableID,
                                            sTable,
                                            sColumn,
                                            aChangeTable,
                                            aChangeColumn )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // tuple�� ���� �Ҵ���� ���� ����.
            // mtrNode ������, ���ǿ� ���� �÷����� �����ϴµ�.....
            *aChangeTable = aOrgTable;
            *aChangeColumn = aOrgColumn;
        }
    }
    else
    {
        // tuple�� ���� �Ҵ���� ���� ����.
        // mtrNode ������, ���ǿ� ���� �÷����� �����ϴµ�.....
        *aChangeTable = aOrgTable;
        *aChangeColumn = aOrgColumn;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
        
    return IDE_FAILURE;    
}


IDE_RC qmg::findColumnLocate( qcStatement  * aStatement,
                              UShort         aTableID,
                              UShort         aOrgTable,
                              UShort         aOrgColumn,
                              UShort       * aChangeTable,
                              UShort       * aChangeColumn )
{
    UShort sTable;
    UShort sColumn;

    IDU_FIT_POINT_FATAL( "qmg::findColumnLocate::__FT__" );

    if( aOrgTable != aTableID )
    {
        IDE_DASSERT( aOrgColumn != MTC_RID_COLUMN_ID );

        sTable = QC_SHARED_TMPLATE(aStatement)->tmplate.
            rows[aOrgTable].columnLocate[aOrgColumn].table;
        sColumn = QC_SHARED_TMPLATE(aStatement)->tmplate.
            rows[aOrgTable].columnLocate[aOrgColumn].column;

        if( sTable != aTableID )
        {
            if( ( sTable == aOrgTable ) &&
                ( sColumn == aOrgColumn ) )
            {

                *aChangeTable = sTable;
                *aChangeColumn = sColumn;
            }
            else
            {
                IDE_TEST( findColumnLocate( aStatement,
                                            aTableID,
                                            sTable,
                                            sColumn,
                                            aChangeTable,
                                            aChangeColumn )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // tuple�� ���� �Ҵ���� ���� ����.
            // mtrNode ������, ���ǿ� ���� �÷����� �����ϴµ�.....
            *aChangeTable = aOrgTable;
            *aChangeColumn = aOrgColumn;
        }
    }
    else
    {
        // tuple�� ���� �Ҵ���� ���� ����.
        // mtrNode ������, ���ǿ� ���� �÷����� �����ϴµ�.....
        *aChangeTable = aOrgTable;
        *aChangeColumn = aOrgColumn;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
        
    return IDE_FAILURE;    
}

IDE_RC
qmg::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description :
 *    BUG-32258 
 *    When Preserved Order is NOT-DEFINED state, Sort-Join may misjudge
 *    sorting order. (ASC/DESC)
 *
 * Implementation :
 *    (1) Finalize left child graph's Preserved Order (Recursive)
 *    (2) Finalize right child graph's Preserved Order (Recursive)
 *    (3) If this graph's Preserved Order is NOT-DEFINED, finalize it.
 *    (3-1) Selection Graph : Set order to selected index's one.
 *    (3-2) Join Graph : 
 *    (3-3) Group Graph : 
 *    (3-4) Distinct Graph :
 *    (3-5) Windowing Graph : In this case, Preserved Order maybe DEFINED-NOT-
 *    (4) Return this graph's Preserved Order
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::finalizePreservedOrder::__FT__" );

    // (1) Finalize Left
    if( aGraph->left != NULL )
    {
        IDE_TEST( finalizePreservedOrder( aGraph->left ) != IDE_SUCCESS );
    }

    // (2) Finalize Right
    if( aGraph->right != NULL )
    {
        IDE_TEST( finalizePreservedOrder( aGraph->right ) != IDE_SUCCESS );
    }

    // (3) When Preserved Order's Direction is Not defined
    if ( aGraph->preservedOrder != NULL )
    {
        // Preserved Order has created
        if ( aGraph->preservedOrder->direction == QMG_DIRECTION_NOT_DEFINED )
        {
            // Direction is not fixed
            switch( aGraph->type )
            {
                case QMG_SELECTION :       // Selection Graph
                    IDE_TEST( qmgSelection::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_PARTITION :       // PROJ-1502 Partition Graph
                    IDE_TEST( qmgPartition::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;
                    
                case QMG_PROJECTION :      // Projection Graph
                    IDE_TEST( qmgProjection::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_DISTINCTION :     // Duplicate Elimination Graph
                    IDE_TEST( qmgDistinction::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_GROUPING :        // Grouping Graph
                    IDE_TEST( qmgGrouping::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_SORTING :         // Sorting Graph
                    // Sort graph always set direction
                    break;

                case QMG_WINDOWING :       // Windowing Graph
                    IDE_TEST( qmgWindowing::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_INNER_JOIN :      // Inner Join Graph
                case QMG_SEMI_JOIN :       // Semi Join Graph
                case QMG_ANTI_JOIN :       // Anti Join Graph
                case QMG_LEFT_OUTER_JOIN : // Left Outer Join Graph
                case QMG_FULL_OUTER_JOIN : // Full Outer Join Graph
                    // Join graph uses common function
                    IDE_TEST( qmgJoin::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                case QMG_SET :             // SET Graph
                case QMG_HIERARCHY :       // Hierarchy Graph
                case QMG_DNF :             // DNF Graph
                case QMG_SHARD_SELECT:    // Shard Select Graph
                    // These graphs does not use Preserved order
                    break;

                case QMG_COUNTING :        // PROJ-1405 Counting Graph
                    IDE_TEST( qmgCounting::finalizePreservedOrder( aGraph )
                              != IDE_SUCCESS );
                    break;

                default :
                    // All kinds of node must be considered.
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            // Direction has defined.
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
qmg::copyPreservedOrderDirection( qmgPreservedOrder * aDstOrder,
                                  qmgPreservedOrder * aSrcOrder )
{
/***********************************************************************
 *
 * Description : Copy direction info.
 *               Source must compatible to Dest.
 *
 * Implementation :
 *     Copy Direction
 *
 ***********************************************************************/
    
    qmgPreservedOrder * sSrcOrder;
    qmgPreservedOrder * sDstOrder;

    IDU_FIT_POINT_FATAL( "qmg::copyPreservedOrderDirection::__FT__" );
    
    // Source Preserved order�� direction�� copy �Ѵ�.
    for ( sDstOrder = aDstOrder,
              sSrcOrder = aSrcOrder;
          sDstOrder != NULL && sSrcOrder != NULL;
          sDstOrder = sDstOrder->next,
              sSrcOrder = sSrcOrder->next )
    {
        // Direction copy
        sDstOrder->direction = sSrcOrder->direction;
    }
    
    return IDE_SUCCESS;
    
}

idBool
qmg::isSamePreservedOrder( qmgPreservedOrder * aDstOrder,
                           qmgPreservedOrder * aSrcOrder )
{
/***********************************************************************
 *
 * Description : �� preserved order�� ������ �˻�
 *
 * Implementation :
 *         1. Compatability Check
 *         2. Check Preserved order size
 *
 ***********************************************************************/
    
    qmgPreservedOrder * sSrcOrder;
    qmgPreservedOrder * sDstOrder;
    idBool              sIsSame;

    sIsSame = ID_TRUE;

    // 1. Source�� Destination Preserved order�� ������ �÷��� �ǹ��ϴ��� �˻��Ѵ�.
    for ( sDstOrder = aDstOrder,
              sSrcOrder = aSrcOrder;
          sDstOrder != NULL && sSrcOrder != NULL;
          sDstOrder = sDstOrder->next,
              sSrcOrder = sSrcOrder->next )
    {
        if ( ( sDstOrder->table  == sSrcOrder->table ) &&
             ( sDstOrder->column == sSrcOrder->column ) )
        {
            // sIsCompat is ID_TRUE
        }
        else
        {
            sIsSame = ID_FALSE;
            break;
        }
    }

    // 2. Check Preserved order size
    if ( sDstOrder != NULL )
    {
        // Source�� Destination Preserved order�� ��� ������Ű�� ����
        sIsSame = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    return sIsSame;
}

IDE_RC
qmg::makeDummyMtrNode( qcStatement  * aStatement ,
                       qmsQuerySet  * aQuerySet,
                       UShort         aTupleID,
                       UShort       * aColumnCount,
                       qmcMtrNode  ** aMtrNode )
{
/***********************************************************************
 *
 * Description :
 *    PROJ-2179 SORT���� result descriptor�� ����־��ִ� ��쿡 ����Ͽ�
 *    dummy ���� materialize �ϵ��� �Ѵ�.
 *
 * Implementation :
 *    CHAR���� '1'�� materlialize�ϵ��� node�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode        * sConstNode[2];
    qcNamePosition   sPosition;

    IDU_FIT_POINT_FATAL( "qmg::makeDummyMtrNode::__FT__" );

    SET_EMPTY_POSITION( sPosition );

    IDE_TEST( qtc::makeValue( aStatement,
                              sConstNode,
                              (const UChar*)"CHAR",
                              4,
                              &sPosition,
                              (const UChar*)"1",
                              1,
                              MTC_COLUMN_NORMAL_LITERAL ) 
              != IDE_SUCCESS );

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sConstNode[0] )
              != IDE_SUCCESS );

    IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                      aQuerySet,
                                      sConstNode[0],
                                      ID_FALSE,
                                      aTupleID,
                                      0,
                                      aColumnCount,
                                      aMtrNode ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

idBool
qmg::getMtrMethod( qcStatement * aStatement,
                   UShort        aSrcTupleID,
                   UShort        aDstTupleID )
{
/***********************************************************************
 *
 * Description :
 *    � ������� materialize �ؾ��ϴ��� �����Ͽ� ��ȭ�Ѵ�.
 *    - Complete record : TRUE�� ��ȯ
 *    - Surrogate key   : FALSE�� ��ȯ
 *
 * Implementation :
 *    Source�� destination tuple�� flag�� ������ ���� �Ǵ��Ѵ�.
 *
 ***********************************************************************/

    mtcTemplate * sMtcTemplate;

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    if( ( ( sMtcTemplate->rows[aSrcTupleID].lflag & MTC_TUPLE_MATERIALIZE_MASK )
          == MTC_TUPLE_MATERIALIZE_VALUE ) &&
        ( ( sMtcTemplate->rows[aDstTupleID].lflag & MTC_TUPLE_MATERIALIZE_MASK )
          == MTC_TUPLE_MATERIALIZE_VALUE ) )
    {
        return ID_TRUE;
    }

    // View�� surrogate-key�� �������� �ʴ´�.
    if( ( sMtcTemplate->rows[aSrcTupleID].lflag & MTC_TUPLE_VIEW_MASK )
        == MTC_TUPLE_VIEW_TRUE )
    {
        return ID_TRUE;
    }

    // Expression�� ���� intermediate tuple�� ���
    if( ( ( sMtcTemplate->rows[aSrcTupleID].lflag & MTC_TUPLE_TYPE_MASK )
          == MTC_TUPLE_TYPE_INTERMEDIATE ) &&
        ( ( sMtcTemplate->rows[aSrcTupleID].lflag & MTC_TUPLE_PLAN_MTR_MASK )
          == MTC_TUPLE_PLAN_MTR_FALSE ) )
    {
        return ID_TRUE;
    }

    /* BUG-36468 for DB Link's Remote Table */
    if ( ( sMtcTemplate->rows[aSrcTupleID].lflag & MTC_TUPLE_STORAGE_LOCATION_MASK )
         == MTC_TUPLE_STORAGE_LOCATION_REMOTE )
    {
        return ID_TRUE;
    }

    return ID_FALSE;
}

idBool
qmg::existBaseTable( qmcMtrNode * aMtrNode,
                     UInt         aFlag,
                     UShort       aTable )
{
    // aMtrNode���� aFlag�� aTable�� ���� node�� �����ϴ��� Ȯ���Ѵ�.

    qmcMtrNode * sMtrNode;

    for( sMtrNode = aMtrNode;
         sMtrNode != NULL;
         sMtrNode = sMtrNode->next )
    {
        if( ( sMtrNode->srcNode->node.table == aTable ) &&
            ( ( sMtrNode->flag & aFlag ) != 0 ) )
        {
            return ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    return ID_FALSE;
}

UInt
qmg::getBaseTableType( ULong aTupleFlag )
{
    // Tuple�� flag�� ���� base table�� materialize type�� ��ȯ�Ѵ�.

    /* PROJ-2464 hybrid partitioned table ���� */
    if ( ( aTupleFlag & MTC_TUPLE_HYBRID_PARTITIONED_TABLE_MASK ) == MTC_TUPLE_HYBRID_PARTITIONED_TABLE_TRUE )
    {
        return QMC_MTR_TYPE_HYBRID_PARTITIONED_TABLE;
    }
    else
    {
        if ( ( aTupleFlag & MTC_TUPLE_STORAGE_MASK ) == MTC_TUPLE_STORAGE_MEMORY )
        {
            if ( ( aTupleFlag & MTC_TUPLE_PARTITIONED_TABLE_MASK ) == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                return QMC_MTR_TYPE_MEMORY_PARTITIONED_TABLE;
            }
            else
            {
                return QMC_MTR_TYPE_MEMORY_TABLE;
            }
        }
        else
        {
            if ( ( aTupleFlag & MTC_TUPLE_PARTITIONED_TABLE_MASK ) == MTC_TUPLE_PARTITIONED_TABLE_TRUE )
            {
                return QMC_MTR_TYPE_DISK_PARTITIONED_TABLE;
            }
            else
            {
                return QMC_MTR_TYPE_DISK_TABLE;
            }
        }
    }
}

idBool
qmg::isTempTable( ULong aTupleFlag )
{
    // �ش� tuple�� temp table�� tuple���� ���θ� ��ȯ�Ѵ�.

    if( ( ( aTupleFlag & MTC_TUPLE_PLAN_MTR_MASK ) == MTC_TUPLE_PLAN_MTR_TRUE ) ||
        ( ( aTupleFlag & MTC_TUPLE_VSCN_MASK ) == MTC_TUPLE_VSCN_TRUE ) )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

idBool
qmg::isDatePseudoColumn( qcStatement * aStatement,
                         qtcNode     * aNode )
{
    qtcNode * sDateNode;
    idBool    sResult = ID_FALSE;

    if( ( aNode->lflag & QTC_NODE_SYSDATE_MASK ) == QTC_NODE_SYSDATE_EXIST )
    {
        if( QC_SHARED_TMPLATE( aStatement )->sysdate != NULL )
        {
            sDateNode = QC_SHARED_TMPLATE( aStatement )->sysdate;

            if( ( sDateNode->node.table  == aNode->node.table  ) &&
                ( sDateNode->node.column == aNode->node.column ) )
            {
                sResult = ID_TRUE;
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

        if( QC_SHARED_TMPLATE( aStatement )->unixdate != NULL )
        {
            sDateNode = QC_SHARED_TMPLATE( aStatement )->unixdate;

            if( ( sDateNode->node.table  == aNode->node.table  ) &&
                ( sDateNode->node.column == aNode->node.column ) )
            {
                sResult = ID_TRUE;
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

        if( QC_SHARED_TMPLATE( aStatement )->currentdate != NULL )
        {
            sDateNode = QC_SHARED_TMPLATE( aStatement )->currentdate;

            if( ( sDateNode->node.table  == aNode->node.table  ) &&
                ( sDateNode->node.column == aNode->node.column ) )
            {
                sResult = ID_TRUE;
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
    else
    {
        // Nothing to do.
    }
    
    return sResult;
}

// PROJ-2242 qmgGraph�� cost ������ qmnPlan ���� ������
void qmg::setPlanInfo( qcStatement  * aStatement,
                       qmnPlan      * aPlan,
                       qmgGraph     * aGraph )
{
    aPlan->qmgOuput   = aGraph->costInfo.outputRecordCnt;

    // insert, delete ���������� mSysStat�� null �̵ȴ�.
    if( aStatement->mSysStat != NULL )
    {
        aPlan->qmgAllCost = aGraph->costInfo.totalAllCost /
            aStatement->mSysStat->singleIoScanTime;
    }
    else
    {
        aPlan->qmgAllCost = aGraph->costInfo.totalAllCost;
    }    
}

IDE_RC qmg::isolatePassNode( qcStatement * aStatement,
                             qtcNode     * aSource )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 *    Source Node Tree�� Traverse�ϸ鼭 passNode�� ���ڸ� ������Ų��.
 *
 ***********************************************************************/

    qtcNode * sNewNode;

    IDU_FIT_POINT_FATAL( "qmg::isolatePassNode::__FT__" );

    IDE_FT_ASSERT( aSource != NULL );
    
    if( aSource->node.arguments != NULL )
    {
        if( ( aSource->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_SUBQUERY )
        {
            // Subquery����� ��쿣 arguments�� �������� �ʴ´�.
        }
        else
        {
            if( qtc::dependencyEqual( & aSource->depInfo,
                                      & qtc::zeroDependencies ) == ID_FALSE )
            {
                IDE_TEST( isolatePassNode( aStatement,
                                           (qtcNode *)aSource->node.arguments )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do...
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // BUG-37355
    // argument�� passNode��� qtcNode�� �����ؼ� �� �̻� ������� �ʵ��� �Ѵ�.
    if( aSource->node.module == &qtc::passModule )
    {
        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM(aStatement), qtcNode, &sNewNode )
                  != IDE_SUCCESS );
        
        idlOS::memcpy( sNewNode, aSource->node.arguments, ID_SIZEOF(qtcNode) );

        aSource->node.arguments = (mtcNode*) sNewNode;
    }
    else
    {
        // Nothing to do.
    }

    if( aSource->node.next != NULL )
    {
        if( qtc::dependencyEqual( &((qtcNode*)aSource->node.next)->depInfo,
                                  & qtc::zeroDependencies ) == ID_FALSE )
        {
            IDE_TEST( isolatePassNode( aStatement,
                                       (qtcNode *)aSource->node.next )
                      != IDE_SUCCESS );
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

IDE_RC qmg::getGraphLateralDepInfo( qmgGraph      * aGraph,
                                    qcDepInfo     * aGraphLateralDepInfo )
{
/*********************************************************************
 * 
 *  Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 *
 *   Graph�� lateralDepInfo�� ��ȯ�Ѵ�.
 *  
 *  Implementation :
 *  - Graph�� SELECTION / PARTITION�̶�� ������ lateralDepInfo�� ��ȯ
 *  - �� ���� Graph�� ���, �� depInfo�� ��ȯ
 * 
 ********************************************************************/

    IDU_FIT_POINT_FATAL( "qmg::getGraphLateralDepInfo::__FT__" );

    qtc::dependencyClear( aGraphLateralDepInfo );

    switch ( aGraph->type )
    {
        case QMG_SELECTION:
        case QMG_PARTITION:
        {
            IDE_DASSERT( aGraph->myFrom != NULL );
            IDE_TEST( qmvQTC::getFromLateralDepInfo( aGraph->myFrom, 
                                                     aGraphLateralDepInfo )
                      != IDE_SUCCESS );
            break;
        }
        default:
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::resetDupNodeToMtrNode( qcStatement * aStatement,
                                   UShort        aOrgTable,
                                   UShort        aOrgColumn,
                                   qtcNode     * aChangeNode,
                                   qtcNode     * aNode )
{
/*********************************************************************
 * 
 *  Description : BUG-43088 (PR-13286 diff)
 *                mtrNode �� original ���� �ߺ��� ��带 ã��
 *                mtrNode ������ �����ϱ� ����
 *  
 *  Implementation :
 *
 *          aNode tree �� ��ȸ�ϸ�
 *          aOrgTable, aOrgColumn �� ������ table, column �� ã��
 *          aChageNode �� table, column ���� �����Ѵ�.
 * 
 ********************************************************************/

    qtcNode * sNode = NULL;

    IDU_FIT_POINT_FATAL( "qmg::resetDupNodeToMtrNode::__FT__" );

    IDE_DASSERT( aStatement  != NULL );
    IDE_DASSERT( aChangeNode != NULL );
    IDE_DASSERT( aNode       != NULL );

    if ( QTC_IS_COLUMN( aStatement, aNode ) &&
         ( aNode->node.table  == aOrgTable ) &&
         ( aNode->node.column == aOrgColumn ) )
    {
        aNode->node.table  = aChangeNode->node.table;
        aNode->node.column = aChangeNode->node.column;
    }
    else
    {
        sNode = (qtcNode *)(aNode->node.arguments);

        while ( sNode != NULL )
        {
            IDE_TEST( resetDupNodeToMtrNode( aStatement,
                                             aOrgTable,
                                             aOrgColumn,
                                             aChangeNode,
                                             sNode )
                      != IDE_SUCCESS );

            sNode = (qtcNode *)(sNode->node.next);
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::lookUpMaterializeGraph( qmgGraph * aGraph, idBool * aFound )
{
/***********************************************************************
 *
 * Description :
 *    BUG-43493 delay operator�� �߰��� execution time�� ���δ�.
 *
 * Implementation :
 *    graph�� left�� ��ȸ�ϸ� materialize�Ҹ���(���ɼ� �ִ�)
 *    operator�� �ִ��� ã�ƺ���.
 *
 ***********************************************************************/

    qmgGraph * sGraph = aGraph;
    idBool     sFound = ID_FALSE;

    while ( sGraph != NULL )
    {
        switch( sGraph->type )
        {
            // materialize ���ɼ� �ִ� graph
            case QMG_DISTINCTION :
            case QMG_GROUPING :
            case QMG_SORTING :
            case QMG_WINDOWING :
            case QMG_SET :
            case QMG_HIERARCHY :
            case QMG_RECURSIVE_UNION_ALL :
                sFound = ID_TRUE;
                break;

            default :
                break;
        }

        if ( sFound == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }

        sGraph = sGraph->left;
    }

    *aFound = sFound;
    
    return IDE_SUCCESS;
}

/* TASK-6744 */
void qmg::initializeRandomPlanInfo( qmgRandomPlanInfo * aRandomPlanInfo )
{
    aRandomPlanInfo->mWeightedValue   = 0;
    aRandomPlanInfo->mTotalNumOfCases = 0;
}

/* TASK-7219 */
IDE_RC qmg::getNodeOffset( qtcNode * aNode,
                           idBool    aIsRecursive,
                           SInt    * aStart,
                           SInt    * aEnd )
{
/****************************************************************************************
 *
 * Description : Node�� Query Text���� ���� Offset�� ������ Offset�� ��ȯ�Ѵ�.
 *               Target, Order By, Where �� ���� �� Shard ����ȭ�ÿ� ����Ѵ�. ������
 *               Offset�� ������ ���� Subquery �� ���, ���̻� argument�� �������� �ʰ�,
 *               ������ Offset�� ��ȯ�Ѵ�. Target�� ���, �ֻ��� Node ������ ����ϸ�,
 *               argument�� ������ �ʿ䰡 ����.
 *
 * Implementation : 1.1. ��ȯ�� �ʿ��� ���� Offset�� ����Ѵ�.
 *                  1.2. ���� Offset�� ����Ѵ�.
 *                  1.3. " " ��. ���ؼ� ������ �ʿ��� ��� �����Ѵ�.
 *                  2.1. ��ȯ�� �ʿ��� ������ Offset�� ����Ѵ�.
 *                  2.2. ������ Offset�� ����Ѵ�.
 *                  2.3. " " ��. ���ؼ� ������ �ʿ��� ��� �����Ѵ�.
 *                  2.4. argument�� �ִٸ� ����Ѵ�.
 *
 ****************************************************************************************/

    qtcNode * sNode  = NULL;
    SInt      sStart = 0;
    SInt      sEnd   = 0;

    IDE_TEST_RAISE( aNode == NULL, ERR_NULL_NODE );

    /* 1.1. ��ȯ�� �ʿ��� ���� Offset�� ����Ѵ�. */
    if ( aStart != NULL )
    {
        /* 1.2. ���� Offset�� ����Ѵ�. */
        sStart = aNode->position.offset;

        /* 1.3. " " ��. ���ؼ� ������ �ʿ��� ��� �����Ѵ�. */
        if ( aNode->position.stmtText[ sStart - 1 ] == '"' )
        {
            sStart--;
        }
        else
        {
            /* Nothing to do */
        }

        *aStart = sStart;
    }
    else
    {
        /* Nothing to do */
    }

    /* 2.1. ��ȯ�� �ʿ��� ������ Offset�� ����Ѵ�. */
    if ( aEnd != NULL )
    {
        /* 2.2. ������ Offset�� ����Ѵ�. */
        sEnd = aNode->position.offset + aNode->position.size;

        /* 2.3. " " ��. ���ؼ� ������ �ʿ��� ��� �����Ѵ�. */
        if ( aNode->position.stmtText[ sEnd ] == '"' )
        {
            sEnd++;
        }
        else
        {
            /* Nothing to do */
        }

        /* 2.4. argument�� �ִٸ� ����Ѵ�. */
        if ( aIsRecursive == ID_TRUE )
        {
            for ( sNode  = (qtcNode *)(aNode->node.arguments);
                  sNode != NULL;
                  sNode  = (qtcNode *)(sNode->node.next) )
            {
                if ( ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                       != MTC_NODE_OPERATOR_SUBQUERY ) &&
                     ( sNode->node.next == NULL ) )
                {
                    IDE_TEST( getNodeOffset( sNode,
                                             aIsRecursive,
                                             NULL,
                                             &( sEnd ) )
                              != IDE_SUCCESS );
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

        *aEnd = sEnd;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getNodeOffset",
                                  "node is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getFromEnd( qmsFrom * aFrom,
                        SInt    * aFromWhereEnd )
{
/***********************************************************************
 *
 * Description : PROJ-2687 Shard aggregation transform
 *
 *               From tree�� ��ȸ�ϸ� query string�� ������ ��ġ�� �ش��ϴ� from��
 *               End position�� ã�� ��ȯ�Ѵ�.
 *
 *               TASK-7219 ���� qmvShardTransform -> qmg �� �Լ��� �Ű��.
 *
 * Implementation :
 *
 ***********************************************************************/

    SInt sThisIsTheEnd = 0;

    if ( aFrom != NULL )
    {
        /* ON clause�� �����ϸ� on clause�� end position�� ����Ѵ�. */
        if ( aFrom->onCondition != NULL )
        {
            sThisIsTheEnd = aFrom->onCondition->position.offset + aFrom->onCondition->position.size;

            if ( aFrom->onCondition->position.stmtText[aFrom->onCondition->position.offset +
                                                       aFrom->onCondition->position.size] == '"' )
            {
                sThisIsTheEnd++;
            }
            else
            {
                /* Nothing to do */
            }
            if ( *aFromWhereEnd < sThisIsTheEnd )
            {
                /*
                 * From tree�� ��ȸ�ϴ� ���� ��ϵ� from�� end position����
                 * �� ū(�� �ڿ� �����ϴ�) end position�� ��� ���� ����
                 */
                *aFromWhereEnd = sThisIsTheEnd;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* ON clause�� �������� ������ tableRef�� end position�� ã�´�. */
            IDE_DASSERT( aFrom->tableRef != NULL );

            if ( QC_IS_NULL_NAME( aFrom->tableRef->aliasName ) == ID_FALSE )
            {
                if ( aFrom->tableRef->aliasName.
                     stmtText[aFrom->tableRef->aliasName.offset +
                              aFrom->tableRef->aliasName.size] == '"' )
                {
                    sThisIsTheEnd = aFrom->tableRef->aliasName.offset +
                        aFrom->tableRef->aliasName.size + 1;
                }
                else if ( aFrom->tableRef->aliasName.
                          stmtText[aFrom->tableRef->aliasName.offset +
                                   aFrom->tableRef->aliasName.size] == '\'' )
                {
                    sThisIsTheEnd = aFrom->tableRef->aliasName.offset +
                        aFrom->tableRef->aliasName.size + 1;
                }
                else
                {
                    sThisIsTheEnd = aFrom->tableRef->aliasName.offset +
                        aFrom->tableRef->aliasName.size;
                }
            }
            else
            {
                sThisIsTheEnd = aFrom->tableRef->position.offset +
                    aFrom->tableRef->position.size;

                if ( aFrom->tableRef->position.
                     stmtText[aFrom->tableRef->position.offset +
                              aFrom->tableRef->position.size] == '"' )
                {
                    sThisIsTheEnd++;
                }
                else
                {
                    /* Nothing to do */
                }
            }

            if ( *aFromWhereEnd < sThisIsTheEnd )
            {
                *aFromWhereEnd = sThisIsTheEnd;
            }
            else
            {
                /* Nothing to do */
            }
        }

        /* Traverse */
        IDE_TEST( getFromEnd( aFrom->left,
                              aFromWhereEnd )
                  != IDE_SUCCESS );

        IDE_TEST( getFromEnd( aFrom->right,
                              aFromWhereEnd )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getFromStart( qmsFrom * aFrom,
                          SInt    * aFromWhereStart )
{
/****************************************************************************************
 *
 * Description : Shard Query�� From�� ���� Offset�� �����Ѵ�. From���� ������ ����
 *               ����ϸ�, Join ����� From�� ����ȭ�� ParseTree�� �ٸ� ������ From ����Ʈȭ
 *               �� ���� ����ؼ� �ּҰ��� ã�´�.
 *
 * Implementation : 1. Join ������ ���ٸ�, �ش� TableRef Position Offset�� �����Ѵ�.
 *                  2. Join ������ �ִٸ�, Left ���� ����Ѵ�.
 *                  3. Join ������ �ִٸ�, Right ���� ����Ѵ�.
 *
 *****************************************************************************************/

    SInt sThisIsTheStart = ID_SINT_MAX;

    if ( aFrom != NULL )
    {
        /* 1. Join ������ ���ٸ�, �ش� TableRef Position Offset�� �����Ѵ�. */
        if ( aFrom->joinType == QMS_NO_JOIN )
        {
            IDE_TEST_RAISE( aFrom->tableRef == NULL, ERR_NULL_TABLEREF );

            sThisIsTheStart = aFrom->tableRef->position.offset;

            if ( *aFromWhereStart > sThisIsTheStart )
            {
                *aFromWhereStart = sThisIsTheStart;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* 2. Join ������ �ִٸ�, Left ���� ����Ѵ�. */
            IDE_TEST( getFromStart( aFrom->left,
                                    aFromWhereStart )
                      != IDE_SUCCESS );

            /* 3. Join ������ �ִٸ�, Right ���� ����Ѵ�.
             *   - Join �� From Parsing ������ ���� 3�� �̻�, Ansi Join �� ����, Comma ������
             *     Right �� ù��° From �� �����Ǿ� �ִ� ��찡 �ִ�.
             */
            IDE_TEST( getFromStart( aFrom->right,
                                    aFromWhereStart )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_TABLEREF )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getFromStart",
                                  "tableref is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getFromOffset( qmsFrom * aFrom,
                           SInt    * aStart,
                           SInt    * aEnd )
{
/****************************************************************************************
 *
 * Description : Shard Query�� From�� Offset�� �����Ѵ�. From���� ������ ���� ����Ѵ�.
 *
 * Implementation : 1. ��ȯ�� �ʿ��� ���� Offset�� ����Ѵ�.
 *                  2. ��ȯ�� �ʿ��� ������ Offset�� ����Ѵ�.
 *
 ****************************************************************************************/

    qmsFrom * sFrom  = NULL;
    SInt      sStart = ID_SINT_MAX;
    SInt      sEnd   = 0;

    IDE_TEST_RAISE( aFrom == NULL, ERR_NULL_FROM );

    /* 1. ��ȯ�� �ʿ��� ���� Offset�� ����Ѵ�. */
    if ( aStart != NULL )
    {
        for ( sFrom  = aFrom;
              sFrom != NULL;
              sFrom  = sFrom->next )
        {
            IDE_TEST( getFromStart( sFrom,
                                    & sStart )
                      != IDE_SUCCESS );
        }

        *aStart = sStart;
    }
    else
    {
        /* Nothing to do */
    }

    /* 2. ��ȯ�� �ʿ��� ������ Offset�� ����Ѵ�. */
    if ( aEnd != NULL )
    {
        for ( sFrom  = aFrom;
              sFrom != NULL;
              sFrom  = sFrom->next )
        {
            IDE_TEST( getFromEnd( sFrom,
                                  & sEnd )
                      != IDE_SUCCESS );
        }

        *aEnd = sEnd;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_FROM )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getFromOffset",
                                  "from is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmg::makeShardParamOffsetArrayForGraph( qcStatement       * aStatement,
                                               qcParamOffsetInfo * aParamOffsetInfo,
                                               UShort            * aOutParamCount,
                                               qcShardParamInfo ** aOutParamInfo )
{
/****************************************************************************************
 *
 * Description : Shard Bind Offset Array�� �����Ѵ�.
 *               ���⼭ Bind Offset�� Variable Tuple �� Column ��ȣ�̴�.
 *               Shard View Transform���� ���Ͽ� Bind ������ ��ġ�� ����� �� �ֱ� ������,
 *               Shard View���� ����ϴ� ������� Bind �������ֱ� ���ؼ�, Offset�� Array��
 *               �����Ѵ�.
 *
 *               ���ο� Bind Count�� �����ؼ�, ������ ��ġ�� �����ؼ� Array�� �����ؾ�
 *               �Ѵ�. ����, �ش� Bind Node�� Column�� �������־��, Node Estimate����
 *               ã�� �� �ִ�. ���� Graph �ܰ��� Bind Transform���� �̿��ϱ� ����
 *               Offset�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ****************************************************************************************/

    UShort    sIdx        = 0;
    UShort    sParamCount = 0;
    qcShardParamInfo * sOutParamInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_INFO );

    sParamCount = aParamOffsetInfo->mCount;

    if ( sParamCount > 0 )
    {
        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMP_MEM( aStatement ),
                                           qcShardParamInfo,
                                           sParamCount,
                                           &( sOutParamInfo ) )
                  != IDE_SUCCESS );

        for ( sIdx = 0;
              sIdx < sParamCount;
              sIdx++ )
        {
            ( sOutParamInfo + sIdx )->mIsOutRefColumnBind = aParamOffsetInfo->mShardParamInfo[ sIdx ].mIsOutRefColumnBind;
            ( sOutParamInfo + sIdx )->mOffset = aParamOffsetInfo->mShardParamInfo[ sIdx ].mOffset;
            ( sOutParamInfo + sIdx )->mOutRefTuple = aParamOffsetInfo->mShardParamInfo[ sIdx ].mOutRefTuple;
        }
    }
    else
    {
        /* Nothing to do */
    }

    if ( aOutParamCount != NULL )
    {
        *aOutParamCount = sParamCount;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aOutParamInfo != NULL )
    {
        *aOutParamInfo = sOutParamInfo;
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayForGraph",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayForGraph",
                                  "info NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::makeShardParamOffsetArrayWithInfo( qcStatement       * aStatement,
                                               sdiAnalyzeInfo    * aAnalyzeInfo,
                                               qcParamOffsetInfo * aParamOffsetInfo,
                                               UShort            * aOutParamCount,
                                               UShort            * aOutParamOffset,
                                               qcShardParamInfo ** aOutShardParamInfo )
{
/****************************************************************************************
 *
 * Description : Shard Bind Offset Array�� �����Ѵ�.
 *               ���⼭ Bind Offset�� Variable Tuple �� Column ��ȣ�̴�.
 *               Shard View Transform���� ���Ͽ� Bind ������ ��ġ�� ����� �� �ֱ� ������,
 *               Shard View���� ����ϴ� ������� Bind �������ֱ� ���ؼ�, Offset�� Array��
 *               �����Ѵ�.
 *
 *               ���ο� Bind Count�� �����ؼ�, ������ ��ġ�� �����ؼ� Array�� �����ؾ�
 *               �Ѵ�. ����, �ش� Bind Node�� Column�� �������־��, Node Estimate����
 *               ã�� �� �ִ�. ���� Graph �ܰ��� Bind Transform���� �̿��ϱ� ����
 *               Offset�� ��ȯ�Ѵ�.
 *
 * Implementation : 1. ����� Bind Count���� ������ Bind Count�� �����Ѵ�.
 *                  2. ���õ� Bind�� �ִٸ�, ���� ��ġ�� �����ؼ� Array�� �����Ѵ�.
 *                  3. Analyze Info ���� Key ���õ� Bind�� �ִٸ�, Array ������ �����Ѵ�
 *
 ****************************************************************************************/

    UShort             sIdx               = 0;
    UShort             sParamOffset       = 0;
    UShort             sParamCount        = 0;
    qcShardParamInfo * sOutShardParamInfo = NULL;
    qtcNode          * sNode              = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aAnalyzeInfo == NULL, ERR_NULL_ANALYZE );
    IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_INFO );
    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    /* 1. ����� Bind Count���� ������ Bind Count�� �����Ѵ�. */
    IDE_TEST( getHostVarOffset( aStatement,
                                &( sParamOffset ) )
              != IDE_SUCCESS );

    sParamCount = qcg::getBindCount( aStatement ) - sParamOffset;

    /* 2. ���õ� Bind�� �ִٸ�, ���� ��ġ�� �����ؼ� Array�� �����Ѵ�. */
    if ( sParamCount > 0 )
    {
        IDE_TEST_RAISE( aParamOffsetInfo->mCount != sParamCount, ERR_INVALID_BIND_INFO );

        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMP_MEM( aStatement ),
                                           qcShardParamInfo,
                                           sParamCount,
                                           &( sOutShardParamInfo ) )
                  != IDE_SUCCESS );

        for ( sIdx = 0;
              sIdx < aParamOffsetInfo->mCount;
              sIdx++ )
        {
            sNode = aStatement->myPlan->stmtListMgr->mHostVarNode[ sParamOffset + sIdx ];

            sNode->node.column = aParamOffsetInfo->mShardParamInfo[sIdx].mOffset;
            ( sOutShardParamInfo + sIdx )->mOffset = aParamOffsetInfo->mShardParamInfo[sIdx].mOffset;
            ( sOutShardParamInfo + sIdx )->mIsOutRefColumnBind = aParamOffsetInfo->mShardParamInfo[sIdx].mIsOutRefColumnBind;
            ( sOutShardParamInfo + sIdx )->mOutRefTuple = aParamOffsetInfo->mShardParamInfo[sIdx].mOutRefTuple;

        }
    }
    else
    {
        /* Nothing to do */
    }

    /* 3. Analyze Info ���� Key ���õ� Bind�� �ִٸ�, Array ������ �����Ѵ�. */
    IDE_TEST( adjustParamOffsetForAnalyzeInfo( aAnalyzeInfo,
                                               sParamCount,
                                               &sOutShardParamInfo )
              != IDE_SUCCESS );

    if ( aOutParamOffset != NULL )
    {
        *aOutParamOffset = sParamOffset;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aOutParamCount != NULL )
    {
        *aOutParamCount = sParamCount;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aOutShardParamInfo != NULL )
    {
        *aOutShardParamInfo = sOutShardParamInfo;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayWithInfo",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayWithInfo",
                                  "info is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ANALYZE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayWithInfo",
                                  "analyze info is NULL" ) );
    }
    IDE_EXCEPTION( ERR_INVALID_BIND_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayWithInfo",
                                  "invalid bind info" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArrayWithInfo",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::makeShardParamOffsetArray( qcStatement       * aStatement,
                                       qcNamePosition    * aParsePosition,
                                       UShort            * aOutParamCount,
                                       UShort            * aOutParamOffset,
                                       qcShardParamInfo ** aOutShardParamInfo )
{
/****************************************************************************************
 *
 * Description : Shard Bind Offset Array�� �����Ѵ�.
 *               ���⼭ Bind Offset�� Variable Tuple �� Column ��ȣ�̴�.
 *               Shard View Transform���� ���Ͽ� Bind ������ ��ġ�� ����� �� �ֱ� ������,
 *               Shard View���� ����ϴ� ������� Bind �������ֱ� ���ؼ�, Offset�� Array��
 *               �����Ѵ�.
 *
 *               ������ ������ Bind ������ Array�� �����Ѵ�. ���� Graph �ܰ���
 *               Bind Transform���� �̿��ϱ� ���� Offset�� ��ȯ�Ѵ�.
 *
 * Implementation : 1. ���� ��ġ�� Ȯ���Ѵ�.
 *                  2. ���� ��ġ�� �״�� ����Ѵ�.
 *
 ****************************************************************************************/

    UShort             sIdx               = 0;
    UShort             sParamOffset       = ID_USHORT_MAX;
    UShort             sParamCount        = ID_USHORT_MAX;
    qcShardParamInfo * sOutShardParamInfo = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParsePosition == NULL, ERR_NULL_POSITION );

    /* 1. ���� ��ġ�� Ȯ���Ѵ�. */
    IDE_TEST( getParamOffsetAndCount( aStatement,
                                      aParsePosition->offset,
                                      aParsePosition->offset + aParsePosition->size,
                                      0,
                                      qcg::getBindCount( aStatement ),
                                      &( sParamOffset ),
                                      &( sParamCount ) )
              != IDE_SUCCESS );

    /* 2. ���� ��ġ�� �״�� ����Ѵ�. */
    if ( sParamCount > 0 )
    {
        IDE_TEST( STRUCT_ALLOC_WITH_COUNT( QC_QMP_MEM( aStatement ),
                                           qcShardParamInfo,
                                           sParamCount,
                                           aOutShardParamInfo )
                  != IDE_SUCCESS );

        sOutShardParamInfo = *aOutShardParamInfo;

        for ( sIdx = 0;
              sIdx < sParamCount;
              sIdx ++ )
        {
            ( sOutShardParamInfo + sIdx )->mOffset = sParamOffset + sIdx;
            ( sOutShardParamInfo + sIdx )->mIsOutRefColumnBind = ID_FALSE;
            ( sOutShardParamInfo + sIdx )->mOutRefTuple = ID_USHORT_MAX;
        }
    }
    else
    {
        *aOutShardParamInfo = NULL;
    }

    if ( aOutParamOffset != NULL )
    {
        *aOutParamOffset = sParamOffset;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aOutParamCount != NULL )
    {
        *aOutParamCount = sParamCount;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArray",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_POSITION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::makeShardParamOffsetArray",
                                  "position is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::findAndCollectParamOffset( qcStatement       * aStatement,
                                       qtcNode           * aNode,
                                       qcParamOffsetInfo * aParamOffsetInfo )
{
/****************************************************************************************
 *
 * Description : Parse �ܰ迡�� Shard Transform���� Bind �߰�, ��ġ ������ �߻��ϸ� Bind
 *               ������ ������ �ֱ����ؼ�, ������ Bind ���� ��ġ�� �����Ѵ�.
 *               Bind�� ���� �߰��ǰų�, �ٸ� ��ġ�� Bind�� �̵��� �� ���� ȣ���ؾ� �Ѵ�.
 *
 *
 *  BEFORE / SELECT SUM( C1 + ? ) AS A FROM T1 GROUP BY C2;
 *                  ************* OLD
 *
 *  AFTER  / SELECT SUM( B ) AS A
 *            FROM SHARD( SELECT C2, SUM( C1 + ? ) B
 *                         FROM T1   ************* NEW
 *                          GROUP BY C2 )
 *             GROUP BY C2;
 *
 *
 *  BEFORE / SELECT AVG( C1 + ? ) AS A FROM T1 GROUP BY C2;
 *                  ************* OLD
 *
 *  AFTER  / SELECT SUM( B ) / SUM( C ) AS A
 *            FROM SHARD( SELECT C2, SUM( C1 + ? ) AS B, COUNT( C1 + ? ) AS C
 *                          FROM T1  *************       *************** NEW 2ȸ
 *                           GROUP BY C2 )
 *             GROUP BY C2;
 *
 *
 *  BEFORE / SELECT SUM( COL ) AS A FROM T1 GROUP BY C2 HAVING COUNT( C1 + ? ) > ?
 *                                                             ******************* OLD
 *
 *  AFTER  / SELECT SUM( B ) AS A
 *            FROM SHARD( SELECT C2, SUM( COL ) AS B, COUNT( C1 + ? ) AS C
 *                         FROM T1                    *************** NEW
 *                          GROUP BY C2 )
 *             GROUP BY C2
 *              HAVING C > ?
 *
 *
 * Implementation : 1. BIND ����� �� �˻��Ѵ�.
 *                  2. �ִٸ�, BIND ��ġ�� ��Ͻ�Ų��.
 *                  3. ���ٸ�, ��� ������ BIND ����� ã�´�.
 *
 ****************************************************************************************/

    qtcNode * sArgNode = NULL;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aNode == NULL, ERR_NULL_ARGUMENT );

    /* 1. BIND ����� �� �˻��Ѵ�.*/
    if ( qtc::isHostVariable( QC_SHARED_TMPLATE( aStatement ), aNode ) == ID_TRUE )
    {
        IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_BIND_INFO );
        IDE_TEST_RAISE( aParamOffsetInfo->mCount >= MTC_TUPLE_COLUMN_ID_MAXIMUM, ERR_HOST_VAR_LIMIT );

        /* 2. �ִٸ�, BIND ��ġ�� ��Ͻ�Ų�� */
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mIsOutRefColumnBind = ID_FALSE;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOffset = aNode->node.column;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOutRefTuple = ID_USHORT_MAX;
        aParamOffsetInfo->mCount++;
    }
    else if ( ( aNode->lflag & QTC_NODE_OUT_REF_COLUMN_MASK )
              == QTC_NODE_OUT_REF_COLUMN_TRUE ) 
    {
        IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_BIND_INFO );
        IDE_TEST_RAISE( aParamOffsetInfo->mCount >= MTC_TUPLE_COLUMN_ID_MAXIMUM, ERR_HOST_VAR_LIMIT );

        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mIsOutRefColumnBind = ID_TRUE;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOutRefTuple = aNode->node.table;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOffset = aNode->node.column;

        aParamOffsetInfo->mCount++;
    }   
    else
    {
        /* 3. ���ٸ�, ��� ������ BIND ����� ã�´�. */
        for ( sArgNode  = (qtcNode *)aNode->node.arguments;
              sArgNode != NULL;
              sArgNode  = (qtcNode *)sArgNode->node.next )
        {
            IDE_TEST( findAndCollectParamOffset( aStatement,
                                                 sArgNode,
                                                 aParamOffsetInfo )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::findAndCollectParamOffset",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_ARGUMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::findAndCollectParamOffset",
                                  "node is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_BIND_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::findAndCollectParamOffset",
                                  "bind info is null" ) );
    }
    IDE_EXCEPTION( ERR_HOST_VAR_LIMIT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QTC_HOST_VAR_LIMIT_EXCEED,
                                  MTC_TUPLE_COLUMN_ID_MAXIMUM ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::collectReaminParamOffset( qcStatement       * aStatement,
                                      SInt                aStartOffset,
                                      SInt                aEndOffset,
                                      qcParamOffsetInfo * aParamOffsetInfo )
{
/****************************************************************************************
 *
 * Description : Shard Transform���� ������ �ڷ� �и� Bind�� �����Ѵ�.
 *               findAndCollectParamOffset ���Ŀ� ȣ���ϸ�, From ������ Where��������
 *               Bind�� aParamOffsetInfo�� ������ Bind ���� ������, ������ Bind ��ġ��
 *               ����Ѵ�.
 *
 *               aStartOffset�� From �� ����, aEndOffset�� Where�� ���� ����Ѵ�.
 *
 *
 *  BEFORE / SELECT SUM( C1 + ? ) AS A FROM T1 WHERE C3 != ? GROUP BY C2;
 *                                     *********************
 *
 *  AFTER  / SELECT SUM( B ) AS A
 *            FROM SHARD( SELECT C2, SUM( C1 + ? ) AS B
 *                         FROM T1 WHERE C3 != ?
 *                          GROUP BY C2 )
 *             GROUP BY C2;
 *
 *
 *  BEFORE / SELECT SUM( C1 + ? ) A
 *            FROM ( SELECT * FROM T1 WHERE C4 = ? ) WHERE C3 != ? GROUP BY C2;
 *            ****************************************************
 *
 *  AFTER  / SELECT SUM( B ) / SUM( C ) AS A
 *            FROM SHARD( SELECT C2, SUM( C1 + ? ) AS B, COUNT( C1 + ? ) AS C
 *                         FROM ( SELECT * FROM T1 WHERE C4 = ? ) WHERE C3 != ?
 *                          GROUP BY C2 )
 *             GROUP BY C2;
 *
 *
 * Implementation : 1. Bind Offset, Bind Count�� ��´�.
 *                  2. ���� Bind ���� ��ū ������ Bind Offset�� ����Ѵ�.
 *
 ****************************************************************************************/

    UShort sParamOffset = 0;
    UShort sParamCount  = 0;
    UInt   sIdx;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_BIND_INFO );
    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    /* 1. Bind Offset, Bind Count�� ��´�. */
    IDE_TEST( getParamOffsetAndCount( aStatement,
                                      aStartOffset,
                                      aEndOffset,
                                      0,
                                      qcg::getBindCount( aStatement ),
                                      &( sParamOffset ),
                                      &( sParamCount ) )
              != IDE_SUCCESS );

    /* 2.   ���� Bind ���� ��ū ������ Bind Offset�� ����Ѵ�. */
    for ( sIdx = 0;
          sIdx < sParamCount;
          sIdx ++ )
    {
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mIsOutRefColumnBind = ID_FALSE ;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOffset = sParamOffset + sIdx;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOutRefTuple = ID_USHORT_MAX;

        aParamOffsetInfo->mCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::collectReaminParamOffset",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_BIND_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::collectReaminParamOffset",
                                  "bind info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::collectReaminParamOffset",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::copyAndCollectParamOffset( qcStatement       * aStatement,
                                       SInt                aStartOffset,
                                       SInt                aEndOffset,
                                       UShort              aParamOffset,
                                       UShort              aParamCount,
                                       qcShardParamInfo  * aShardParamInfo,
                                       qcParamOffsetInfo * aParamOffsetInfo )
{
/****************************************************************************************
 *
 * Description : Graph ������ Shard Transform���� ������ �ڷ� �и� Bind�� �����Ѵ�.
 *               findAndCollectParamOffset ���Ŀ� ȣ���ϸ�, Transform ����� �ƴ�
 *               ������ �ִ� Bind ������ �����Ѵ�.
 *
 *
 *  BEFORE / SELECT CAST( ? AS INTEGER )
 *            FROM ( SELECT CAST( ? AS INTEGER ), C1
 *                    FROM T1 WHERE C3 != ?
 *                    *********************
 *                     ORDER BY C2 )
 *             WHERE C1 != ?;
 *
 *  BIND       / COUNT 4 / OFFSET 0 1 2 3
 *                                    *
 *                                      \_____________________
 *                                                            |
 *  AFTER1 / SELECT CAST( ? AS INTEGER )                      |
 *            FROM ( SELECT *                                 |
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1  |
 *                            FROM T1 WHERE C3 != ?           |
 *                             ORDER BY C2 ) )                |
 *             WHERE C1 != ?;          _______________________|
 *                                    /                        collectReaminParamOffset
 *  SHARD BIND / COUNT 2 / OFFSET 1 2
 *                                N N
 *                                    \_______________________
 *                                                            |
 *  AFTER2 / SELECT CAST( ? AS INTEGER )                      |
 *            FROM ( SELECT *                                 |
 *                    FROM ( SELECT CAST( ? AS INTEGER ), C1  |
 *                            FROM T1 WHERE C3 != ?           |
 *                             AND C1 != ?                    |
 *                              ORDER BY C2 ) )               |
 *             WHERE C1 != ?;         ________________________|
 *                                   /                         copyAndCollectParamOffset
 *  SHARD BIND / COUNT 3 / OFFSET 1 2 3
 *                                C C N
 *
 *
 * Implementation : 1. Bind Offset, Bind Count�� ��´�.
 *                  2. ���� Bind ���� ��ū ������ Bind Offset�� �����Ѵ�.
 *
 ****************************************************************************************/

    UShort   sShardParamOffset = aParamOffset;
    UShort   sShardParamCount  = aParamOffset + aParamCount;
    UShort   sParamOffset      = 0;
    UShort   sParamCount       = 0;
    UInt     sIdx;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aParamOffsetInfo == NULL, ERR_NULL_BIND_INFO );
    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    /* 1. Bind Offset, Bind Count�� ��´�. */
    IDE_TEST( getParamOffsetAndCount( aStatement,
                                      aStartOffset,
                                      aEndOffset,
                                      sShardParamOffset,
                                      sShardParamCount,
                                      &( sParamOffset ),
                                      &( sParamCount ) )
              != IDE_SUCCESS );

    /* 2. ���� Bind ���� ��ū ������ Bind Offset�� ����Ѵ�. */
    for ( sIdx = 0;
          sIdx < sParamCount;
          sIdx ++ )
    {
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mIsOutRefColumnBind = (aShardParamInfo + sParamOffset + sIdx)->mIsOutRefColumnBind ;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOffset = (aShardParamInfo + sParamOffset + sIdx)->mOffset;
        aParamOffsetInfo->mShardParamInfo[aParamOffsetInfo->mCount].mOutRefTuple = (aShardParamInfo + sParamOffset + sIdx)->mOffset;

        aParamOffsetInfo->mCount++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::copyAndCollectParamOffset",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_BIND_INFO )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::copyAndCollectParamOffset",
                                  "bind info is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::copyAndCollectParamOffset",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::setHostVarOffset( qcStatement * aStatement )
{
 /****************************************************************************************
 *
 * Description : Shard Transform���� Bind ������ �߻��� ���� ����ؿ�, ���� Bind ������
 *               �����ؾ� �մϴ�. ���� ���ο� Bind ������ ���� Bind �������Ŀ� �����ϱ�
 *               ���ؼ�, ���� Bind ������ mHostVarOffset�� ����մϴ�.
 *
 *               �׸��� Shard Transfrom �Ϸ� �Ŀ� mHostVarOffset�� 0���� �����ؾ� �մϴ�.
 *
 * Implementation : 1. mHostVarOffset�� ���� Bind ������ �����Ѵ�.
 *                  2. �ٽ� ȣ��� ���� �����Ѵ�.
 *                  3. ���� Bind ������ ����Ѵ�.
 *
 ****************************************************************************************/

    qcTemplate * sTemplate   = NULL;
    UShort       sParamCount = ID_USHORT_MAX;
    UShort       sBindTuple  = ID_USHORT_MAX;

    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    sTemplate  = QC_SHARED_TMPLATE( aStatement );
    sBindTuple = sTemplate->tmplate.variableRow;

    /* 1. mHostVarOffset�� ���� Bind ������ �����Ѵ�. */
    if ( aStatement->myPlan->stmtListMgr->mHostVarOffset == 0 )
    {
        sParamCount = qcg::getBindCount( aStatement );

        aStatement->myPlan->stmtListMgr->mHostVarOffset = sParamCount;
    }
    else
    {
        /* 2. �ٽ� ȣ��� ���� �����Ѵ�. */
        sParamCount = aStatement->myPlan->stmtListMgr->mHostVarOffset;

        aStatement->myPlan->stmtListMgr->mHostVarOffset = 0;
    }

    /* 3. ���� Bind ������ ����Ѵ�. */
    if ( sBindTuple != ID_USHORT_MAX )
    {
        sTemplate->tmplate.rows[sBindTuple].columnCount = sParamCount;
    }
    else
    {
        /* Nothing do do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::setParamCountAndOffset",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::checkStackOverflow( mtcNode * aNode,
                                SInt      aRemain,
                                idBool  * aIsOverflow )
{
 /****************************************************************************************
 *
 * Description : �־��� Node�� Mtc Stack ������ �˻��ϴ� �Լ��̴�.
 *
 * Implementation : 1. ���� ����� Stack ������ �˻��Ѵ�.
 *                  2. ���ѿ� �����ϸ�, ���� ����� Argument�� ����Ѵ�.
 *                  3. Overflow ���θ� ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    mtcNode * sNode        = NULL;
    SInt      sRemain      = 0;
    idBool    sIskOverflow = ID_FALSE;

    IDE_TEST_RAISE( aNode == NULL, ERR_NULL_NODE );

    /* 1. ���� ����� Stack ������ �˻��Ѵ�. */
    sRemain = aRemain - (SInt)( aNode->lflag & MTC_NODE_ARGUMENT_COUNT_MASK );

    if ( sRemain < 1 )
    {
        sIskOverflow = ID_TRUE;
    }
    else
    {
        /* 2. ���ѿ� �����ϸ�, ���� ����� Argument�� ����Ѵ�. */
        for ( sNode  = aNode->arguments, sRemain = aRemain - 1;
              sNode != NULL;
              sNode  = sNode->next, sRemain-- )
        {
            IDE_TEST( checkStackOverflow( sNode,
                                          sRemain,
                                          &( sIskOverflow ) )
                      != IDE_SUCCESS );

            if ( sIskOverflow == ID_TRUE )
            {
                break;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    /* 3. Overflow ���θ� ��ȯ�Ѵ�. */
    if ( aIsOverflow != NULL )
    {
        *aIsOverflow = sIskOverflow;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::checkStackOverflow",
                                  "node is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getParamOffsetAndCount( qcStatement * aStatement,
                                    SInt          aStartOffset,
                                    SInt          aEndOffset,
                                    UShort        aStartParamOffset,
                                    UShort        aEndParamCount,
                                    UShort      * aParamOffset,
                                    UShort      * aParamCount )
{
/****************************************************************************************
 *
 * Description :
 *
 * Implementation : 1. aStartParamOffset ���� aEndParamCount ���� Bind Offset, Bind Count �� ����Ѵ�.
 *                  2. aStartOffset ������ Bind ������ Ȯ��, Bind Offset�� ��´�.
 *                  3. aEndOffset ������ Bind ������ Ȯ��, Bind Count�� ��´�.
 *
 ****************************************************************************************/

    UShort   sParamOffset = 0;
    UShort   sParamCount  = 0;
    UInt     sIdx;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    /* 1. From ������ Where������ Bind Offset, Bind Count �� ����Ѵ�. */
    for ( sIdx = aStartParamOffset;
          sIdx < aEndParamCount;
          sIdx++ )
    {
        /* 2. aStartOffset ������ Bind ������ Ȯ��, Bind Offset�� ��´�. */
        if ( aStatement->myPlan->stmtListMgr->hostVarOffset[ sIdx ] < aStartOffset )
        {
            sParamOffset++;
        }
        else
        {
            /* 3. aEndOffset ������ Bind ������ Ȯ��, Bind Count�� ��´�.. */
            if ( aStatement->myPlan->stmtListMgr->hostVarOffset[ sIdx ] <= aEndOffset )
            {
                sParamCount++;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    if ( aParamOffset != NULL )
    {
        *aParamOffset = sParamOffset;
    }
    else
    {
        /* Nothing to do */
    }

    if ( aParamCount != NULL )
    {
        *aParamCount = sParamCount;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getParamOffsetAndCount",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getParamOffsetAndCount",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::adjustParamOffsetForAnalyzeInfo( sdiAnalyzeInfo    * aAnalyzeInfo,
                                             UShort              aParamCount,
                                             qcShardParamInfo ** aOutShardParamInfo )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                PROJ-2646 New shard analyzer �� ���� ������ �Լ�ȭ
 *
 *                 Analyze �ݺ� ���� ���������� ���ึ�� Bind Offset ��ġ�� ����Ǿ�,
 *                  �Ź� ȣ���Ͽ� �����Ͽ�����,
 *                   ������ Analyze �ּ� ���� ���������� Shard Query �� ����Ǵ�
 *                    TransformAble Query �� Optimize Shard Query ������ ȣ���Ѵ�.
 *
 *                     - qmg::makeShardParamOffsetArrayWithInfo
 *                      <- qmvShardTransform::makeShardForAggr
 *                       <- qmvShardTransform::processTransformForAggr
 *
 *                     - qmgShardSelect::setShardParameter
 *                      <- qmgShardSelect::pushSeriesOptimize
 *
 * Implementation :
 *
 ***********************************************************************/
    UShort             sIndex = 0;
    qcShardParamInfo * sShardParamInfo = *aOutShardParamInfo;
    UShort             sOrgBindId = 0;

    IDE_TEST_RAISE( aAnalyzeInfo == NULL, ERR_NULL_ANALYZE );

    if ( aParamCount > 0 )
    {
        for ( sIndex = 0;
              sIndex < aAnalyzeInfo->mValuePtrCount;
              sIndex++ )
        {
            if ( aAnalyzeInfo->mValuePtrArray[ sIndex ]->mType == 0 )
            {
                if ( ( sShardParamInfo + aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mBindParamId )->mIsOutRefColumnBind
                     == ID_TRUE )
                {
                    /* TASK-7219 Non-shard DML */
                    sOrgBindId = aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mBindParamId;

                    aAnalyzeInfo->mValuePtrArray[ sIndex ]->mType = SDI_VALUE_INFO_OUT_REF_COL;

                    aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mOutRefInfo.mTuple =
                        &(( sShardParamInfo + sOrgBindId )->mOutRefTuple);

                    aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mOutRefInfo.mColumn =
                        &(( sShardParamInfo + sOrgBindId )->mOffset);
                }
                else
                {
                    aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mBindParamId =
                        ( sShardParamInfo + aAnalyzeInfo->mValuePtrArray[ sIndex ]->mValue.mBindParamId )->mOffset;
                }
            }
            else
            {
                /* Nothing to do */
            }
        }

        if ( aAnalyzeInfo->mSubKeyExists == ID_TRUE )
        {
            for ( sIndex = 0;
                  sIndex < aAnalyzeInfo->mSubValuePtrCount;
                  sIndex++ )
            {
                if ( aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mType == 0 )
                {
                    if ( ( sShardParamInfo + aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mBindParamId )->mIsOutRefColumnBind
                         == ID_TRUE )
                    {
                        /* TASK-7219 Non-shard DML */
                        sOrgBindId = aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mBindParamId;

                        aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mType = SDI_VALUE_INFO_OUT_REF_COL;

                        aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mOutRefInfo.mTuple =
                            &(( sShardParamInfo + sOrgBindId )->mOutRefTuple);

                        aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mOutRefInfo.mColumn =
                            &(( sShardParamInfo + sOrgBindId )->mOffset);
                    }
                    else
                    {
                        aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mBindParamId =
                            ( sShardParamInfo + aAnalyzeInfo->mSubValuePtrArray[ sIndex ]->mValue.mBindParamId )->mOffset;
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
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_ANALYZE )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "qmg::adjustParamOffsetForAnalyzeInfo",
                                  "analyze is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmg::getHostVarOffset( qcStatement * aStatement,
                              UShort      * aParamOffset )
{
/***********************************************************************
 *
 * Description : TASK-7219 Shard Transformer Refactoring
 *                Shard Transform ���� Bind ������ �߻��� ���� ����ؿ�,
 *                 ������ ���� Bind ������ �����´�.
 *                  ������ ���� ���ٸ�, ���� Bind ������ �����´�.
 *
 *                   ���� �Լ�
 *                    - qmg::setHostVarOffset
 *
 * Implementation : 1. ������ ���� ���ٸ�, ���� Bind ������ �����´�.
 *                  2. ������ ���� Bind ������ �����´�.
 *
 ***********************************************************************/

    UShort sParamOffset = ID_USHORT_MAX;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aStatement->myPlan->stmtListMgr == NULL, ERR_NULL_STMTLISTMGR );

    /* 1. ������ ���� ���ٸ�, ���� Bind ������ �����´�. */
    if ( aStatement->myPlan->stmtListMgr->mHostVarOffset == 0 )
    {
        sParamOffset = qcg::getBindCount( aStatement );
    }
    else
    {
        /* 2. ������ ���� Bind ������ �����´�. */
        sParamOffset = aStatement->myPlan->stmtListMgr->mHostVarOffset;
    }


    if ( aParamOffset != NULL )
    {
        *aParamOffset = sParamOffset;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getHostVarOffset",
                                  "statement is NULL" ) );
    }
    IDE_EXCEPTION( ERR_NULL_STMTLISTMGR )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmg::getHostVarOffset",
                                  "stmtListMgr is NULL" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
