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
 * $Id: qmgSetRecursive.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     RECURSIVE UNION ALL Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgSetRecursive.h>
#include <qmoCost.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoTwoNonPlan.h>
#include <qmoMultiNonPlan.h>
#include <qmoSelectivity.h>

IDE_RC qmgSetRecursive::init( qcStatement * aStatement,
                              qmsQuerySet * aQuerySet,
                              qmgGraph    * aLeftGraph,
                              qmgGraph    * aRightGraph,
                              qmgGraph    * aRecursiveViewGraph,
                              qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgSetRecursive�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgSetRecursive�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgRUNION   * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aLeftGraph != NULL );
    IDE_DASSERT( aRightGraph != NULL );

    //---------------------------------------------------
    // Set Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgSetRecursive�� ���� ���� �Ҵ�
    IDU_FIT_POINT( "qmgSetRecursive::init::alloc::sMyGraph" );
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgRUNION ),
                                             (void**) & sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_RECURSIVE_UNION_ALL;

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.left = aLeftGraph;
    sMyGraph->graph.right = aRightGraph;

    sMyGraph->recursiveViewGraph = aRecursiveViewGraph;

    // PROJ-1358
    // SET�� ��� Child�� Dependency�� �������� �ʰ�,
    // �ڽ��� VIEW�� ���� dependency�� �����Ѵ�.
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aQuerySet->depInfo );

    sMyGraph->graph.optimize = qmgSetRecursive::optimize;
    sMyGraph->graph.makePlan = qmgSetRecursive::makePlan;
    sMyGraph->graph.printGraph = qmgSetRecursive::printGraph;

    // MEMORY �� �����Ѵ�.
    sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
    sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;

    // out ����
    *aGraph = (qmgGraph*)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSetRecursive::optimize( qcStatement * aStatement,
                                  qmgGraph    * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSetRecursive�� ����ȭ
 *
 * Implementation :
 *    (1) �ʱ�ȭ
 *    (2) ���� ��� ���� ����
 *    (3) Preserved Order
 *
 ***********************************************************************/

    qmgRUNION  * sMyGraph;
    SDouble      sCost;

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph = (qmgRUNION*) aGraph;

    //------------------------------------------
    // cost ���
    //------------------------------------------

    sMyGraph->graph.costInfo.myAccessCost = 0;
    sMyGraph->graph.costInfo.myDiskCost   = 0;

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // record size
    sMyGraph->graph.costInfo.recordSize =
        sMyGraph->graph.left->costInfo.recordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = 1;

    // input record count
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt +
        sMyGraph->graph.right->costInfo.outputRecordCnt;

    // output record count
    IDE_TEST( qmoSelectivity::setSetRecursiveOutputCnt(
                  sMyGraph->graph.left->costInfo.outputRecordCnt,
                  sMyGraph->graph.right->costInfo.outputRecordCnt,
                  & sMyGraph->graph.costInfo.outputRecordCnt )
              != IDE_SUCCESS );

    sCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                         &(aGraph->left->costInfo),
                                         QMO_COST_DEFAULT_NODE_CNT );
        
    sMyGraph->graph.costInfo.myAccessCost = sCost;
    sMyGraph->graph.costInfo.myDiskCost   = 0;   

    // myCost
    // My Access Cost�� My Disk Cost�� �̹� ����.
    sMyGraph->graph.costInfo.myAllCost =
        sMyGraph->graph.costInfo.myAccessCost +
        sMyGraph->graph.costInfo.myDiskCost;

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->graph.left->costInfo.totalAccessCost +
        sMyGraph->graph.right->costInfo.totalAccessCost;
    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->graph.left->costInfo.totalDiskCost +
        sMyGraph->graph.right->costInfo.totalDiskCost;
    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->graph.left->costInfo.totalAllCost +
        sMyGraph->graph.right->costInfo.totalAllCost;

    // preserved order ����
    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmgSetRecursive::makePlan( qcStatement    * aStatement,
                                  const qmgGraph * aParent,
                                  qmgGraph       * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSetRecursive���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *     - qmgSetRecursive���� ���� ���������� Plan
 *
 *               ( [PROJ] ) : parent�� SET�� ��� ������
 *                   |
 *                 [VIEW]
 *                   |
 *           [UNION ALL RECURSIVE]
 *                 |    |
 *                left right
 *
 ***********************************************************************/

    qmgRUNION * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgRUNION*) aGraph;

    // parallel ���� ���� ����.
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;

    sMyGraph->graph.myPlan = aParent->myPlan;

    IDE_TEST( makeUnionAllRecursive( aStatement, sMyGraph )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qmgSetRecursive::makeChild( qcStatement * aStatement,
                                   qmgRUNION   * aMyGraph )
{

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::makeChild::__FT__" );

    // BUG-38410
    // SCAN parallel flag �� �ڽ� ���� �����ش�.
    aMyGraph->graph.left->flag  |= (aMyGraph->graph.flag &
                                    QMG_PLAN_EXEC_REPEATED_MASK);
    aMyGraph->graph.right->flag |= (aMyGraph->graph.flag &
                                    QMG_PLAN_EXEC_REPEATED_MASK);

    /* ViewMtr�� �����ϵ��� Mask ���� */
    aMyGraph->graph.left->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
    aMyGraph->graph.left->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;

    aMyGraph->graph.right->flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
    aMyGraph->graph.right->flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;

    // �׻� Memeory Temp Table �� �̿��Ѵ�.
    aMyGraph->graph.left->flag &= ~QMG_GRAPH_TYPE_MASK;
    aMyGraph->graph.left->flag |= QMG_GRAPH_TYPE_MEMORY;

    aMyGraph->graph.right->flag &= ~QMG_GRAPH_TYPE_MASK;
    aMyGraph->graph.right->flag |= QMG_GRAPH_TYPE_MEMORY;

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph ,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);
    
    
    IDE_TEST( aMyGraph->graph.right->makePlan( aStatement ,
                                               &aMyGraph->graph ,
                                               aMyGraph->graph.right )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSetRecursive::makeUnionAllRecursive( qcStatement * aStatement,
                                               qmgRUNION   * aMyGraph )
{
/***********************************************************************
 *
 * Description : qmgSetRecursive���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *
 *                 [PROJ]
 *                   |
 *                 [VIEW]
 *                   |
 *           [UNION ALL RECURSIVE]
 *                 |    |
 *               left  right
 *
 ***********************************************************************/
    
    qmnPlan * sVIEW = NULL;
    qmnPlan * sSREC = NULL;

    UInt      sFlag;

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::makeUnionAllRecursive::__FT__" );

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init VIEW
    //-----------------------

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan ,
                                       &sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    //-----------------------
    // init RECWITH
    //-----------------------
    IDE_TEST( qmoTwoNonPlan::initSREC( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan,
                                       &sSREC )
              != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sSREC;

    //-----------------------
    // ���� plan ����
    //-----------------------

    IDE_TEST( makeChild( aStatement,
                         aMyGraph )
              != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make SREC
    //-----------------------

    IDE_TEST( qmoTwoNonPlan::makeSREC( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.left->myPlan,
                                       aMyGraph->graph.right->myPlan,
                                       aMyGraph->recursiveViewGraph->myPlan,
                                       sSREC )
              != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sSREC;

    qmg::setPlanInfo( aStatement, sSREC, &(aMyGraph->graph) );

    //-----------------------
    // make VIEW
    //-----------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
    sFlag |= QMO_MAKEVIEW_FROM_SET;

    IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myFrom ,
                                       sFlag ,
                                       aMyGraph->graph.myPlan ,
                                       sVIEW )
              != IDE_SUCCESS);

    aMyGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aMyGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmgSetRecursive::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgSetRecursive::printGraph::__FT__" );

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

    IDE_TEST( aGraph->right->printGraph( aStatement,
                                         aGraph->right,
                                         aDepth + 1,
                                         aString )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
