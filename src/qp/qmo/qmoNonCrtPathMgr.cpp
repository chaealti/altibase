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
 * $Id: qmoNonCrtPathMgr.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 *
 * Description :
 *     Non Critical Path Manager
 *
 *     Critical Path �̿��� �κп� ���� ����ȭ �� �׷����� �����Ѵ�.
 *     ��, ������ ���� �κп� ���� �׷��� ����ȭ�� �����Ѵ�.
 *         - Projection Graph
 *         - Set Graph
 *         - Sorting Graph
 *         - Distinction Graph
 *         - Grouping Graph
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qmoNonCrtPathMgr.h>
#include <qmoConstExpr.h>
#include <qmgProjection.h>
#include <qmgSet.h>
#include <qmgSorting.h>
#include <qmgDistinction.h>
#include <qmgGrouping.h>
#include <qmgWindowing.h>
#include <qmgSetRecursive.h>
#include <qmv.h>

IDE_RC
qmoNonCrtPathMgr::init( qcStatement    * aStatement,
                        qmsQuerySet    * aQuerySet,
                        idBool           aIsTop,
                        qmoNonCrtPath ** aNonCrtPath )
{
/***********************************************************************
 *
 * Description : qmoNonCrtPath ���� �� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmoNonCrtPath ��ŭ �޸� �Ҵ�
 *    (2) qmoNonCrtPath �ʱ�ȭ
 *    (2) crtPath ���� �� �ʱ�ȭ
 *    (3) left, right ���� �� �ʱ�ȭ �Լ� ȣ��
 *
 ***********************************************************************/

    qmsParseTree  * sParseTree;
    qmsQuerySet   * sQuerySet;
    qmoNonCrtPath * sNonCrtPath;
    qmoCrtPath    * sCrtPath;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sParseTree  = (qmsParseTree *)aStatement->myPlan->parseTree;
    sQuerySet   = aQuerySet;
    sNonCrtPath = NULL;
    sCrtPath    = NULL;

    //------------------------------------------
    // qmoNonCrtPath �ʱ�ȭ
    //------------------------------------------

    // qmoNonCrtPath �޸� �Ҵ� ����
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoNonCrtPath),
                                             (void**) & sNonCrtPath )
              != IDE_SUCCESS);

    sNonCrtPath->flag = QMO_NONCRT_PATH_INITIALIZE;

    // flag ���� ����
    if ( aIsTop == ID_TRUE )
    {
        sNonCrtPath->flag &= ~QMO_NONCRT_PATH_TOP_MASK;
        sNonCrtPath->flag |= QMO_NONCRT_PATH_TOP_TRUE;
        sQuerySet->nonCrtPath = sNonCrtPath;
    }
    else
    {
        sNonCrtPath->flag &= ~QMO_NONCRT_PATH_TOP_MASK;
        sNonCrtPath->flag |= QMO_NONCRT_PATH_TOP_FALSE;
    }

    sNonCrtPath->myGraph = NULL;
    sNonCrtPath->myQuerySet = sQuerySet;

    //------------------------------------------
    // crtPath ���� �� �ʱ�ȭ : Leaf Non Critical Path �� ��쿡��
    //------------------------------------------

    if ( sQuerySet->setOp == QMS_NONE )
    {
        //------------------------------------------
        // PROJ-1413 constant exprssion�� ��� ��ȯ
        //------------------------------------------

        IDE_TEST( qmoConstExpr::processConstExpr( aStatement,
                                                  sQuerySet->SFWGH )
                  != IDE_SUCCESS );

        //------------------------------------------
        // Leaf Non Critical Path : crtPath ���� �� �ʱ�ȭ
        //------------------------------------------

        IDE_TEST( qmoCrtPathMgr::init( aStatement, sQuerySet, &sCrtPath )
                  != IDE_SUCCESS );
        sNonCrtPath->crtPath = sCrtPath;
    }
    else
    {
        //------------------------------------------
        // Non-Leaf Non Critical Path : left, right ���� �� �ʱ�ȭ �Լ� ȣ��
        //------------------------------------------

        IDE_TEST( init( aStatement, sQuerySet->left, ID_FALSE,
                        &(sNonCrtPath->left) )
                  != IDE_SUCCESS );

        IDE_TEST( init( aStatement, sQuerySet->right, ID_FALSE,
                        &(sNonCrtPath->right) )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // PROJ-1413 constant exprssion�� ��� ��ȯ
    //------------------------------------------

    IDE_TEST( qmoConstExpr::processConstExprForOrderBy( aStatement,
                                                        sParseTree )
              != IDE_SUCCESS );

    // out ����
    *aNonCrtPath = sNonCrtPath;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNonCrtPathMgr::optimize( qcStatement    * aStatement,
                            qmoNonCrtPath  * aNonCrtPath )
{
/***********************************************************************
 *
 * Description : Non Critical Path�� ����ȭ( ��, qmoNonCrtPath�� ����ȭ)
 *
 * Implementation :
 *    (1) Leaf Non Critical Path�� ����ȭ ( myQuerySet->setOp == QMS_NONE )
 *        a. Critical Path�� ����ȭ ����
 *        b. Grouping�� ó��
 *        c. Nested Grouping�� ó��
 *        d. Distinction�� ó��
 *        e. ��� graph�� qmoNonCrtPath::myGraph�� ����
 *    (2) Non Leaf Non Critical Path�� ����ȭ ( myQuerySet->setOp != QMS_NONE )
 *        a. left, right ����ȭ �Լ� ȣ��
 *        b. qmgSet ���� �� �ʱ�ȭ
 *        c. qmgSet ����ȭ
 *        d. ����ȭ�� qmgSet�� qmoNonCrtPath::myGraph�� ����
 *    (4) Top Non Critical Path�� ó��
 *        a. Order By�� ó��
 *        b. Projection ó��
 *        c. ��� graph�� qmoNonCrtPath::myGraph�� ����
 *
 ***********************************************************************/

    qmoNonCrtPath * sNonCrtPath;
    qmsParseTree  * sParseTree;
    qmsQuerySet   * sQuerySet;
    qmgGraph      * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNonCrtPath != NULL );

    //------------------------------------------
    // Leaf�� ���, Non-Leat ����� ó��
    //------------------------------------------

    sNonCrtPath = aNonCrtPath;
    sParseTree  = (qmsParseTree *)aStatement->myPlan->parseTree;
    sQuerySet   = sNonCrtPath->myQuerySet;

    if ( sQuerySet->setOp == QMS_NONE )
    {
        //------------------------------------------
        // Leaf Non Critical Path�� ���
        //------------------------------------------

        IDE_TEST( optimizeLeaf( aStatement, sNonCrtPath ) != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------------
        // Non-Leaf Non Critical Path�� ���
        //------------------------------------------
        IDE_TEST( optimizeNonLeaf( aStatement, sNonCrtPath ) != IDE_SUCCESS );
    }
    sMyGraph = sNonCrtPath->myGraph;

    //------------------------------------------
    // Top ���ο� ���� ó��
    //------------------------------------------

    if ( ( sNonCrtPath->flag & QMO_NONCRT_PATH_TOP_MASK )
         == QMO_NONCRT_PATH_TOP_TRUE )
    {
        //------------------------------------------
        // ORDER BY ó��
        //------------------------------------------

        if ( sParseTree->orderBy != NULL )
        {
            IDE_TEST( qmgSorting::init( aStatement,
                                        sQuerySet,
                                        sMyGraph,           // child
                                        &sMyGraph )         // result graph
                      != IDE_SUCCESS );

            IDE_TEST( qmgSorting::optimize( aStatement, sMyGraph )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }

        //------------------------------------------
        // Projection ó��
        //------------------------------------------

        IDE_TEST( qmgProjection::init( aStatement,
                                       sQuerySet,
                                       sMyGraph,        // child
                                       &sMyGraph )      // result graph
                  != IDE_SUCCESS );

        IDE_TEST( qmgProjection::optimize( aStatement,  sMyGraph )
                  != IDE_SUCCESS );

        sNonCrtPath->myGraph = sMyGraph;
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
qmoNonCrtPathMgr::optimizeLeaf( qcStatement   * aStatement,
                                qmoNonCrtPath * aNonCrtPath )
{
/***********************************************************************
 *
 * Description : Leaf Non Critical Path�� ����ȭ
 *
 * Implementation :
 *    (1) Critical Path �� ����ȭ ����
 *    (2) Grouping�� ����
 *    (3) Nested Grouping�� ����
 *    (4) Distinction�� ����
 *    (5) ��� graph�� qmoNonCrtPath::myGraph�� ����
 *
 ***********************************************************************/

    qmoNonCrtPath * sNonCrtPath;
    qmsQuerySet   * sQuerySet;
    qmgGraph      * sMyGraph;
    // dummy node
    qmsAggNode    * sAggr;
    qmsAggNode    * sPrev;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::optimizeLeaf::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNonCrtPath != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNonCrtPath = aNonCrtPath;
    sQuerySet   = sNonCrtPath->myQuerySet;

    //------------------------------------------
    // Critical Path ����ȭ
    //------------------------------------------

    IDE_TEST( qmoCrtPathMgr::optimize( aStatement,sNonCrtPath->crtPath )
              != IDE_SUCCESS );

    sMyGraph = sNonCrtPath->crtPath->myGraph;

    //------------------------------------------
    // Grouping�� ó��
    //------------------------------------------

    // dummyNode
    if( (sQuerySet->SFWGH->group != NULL) &&
        (sQuerySet->SFWGH->aggsDepth1 != NULL) &&
        (sQuerySet->SFWGH->aggsDepth2 != NULL) )
    {
        // aggr1 -> aggr2 ( for grouping column, aggr1 )

        sPrev = NULL;
        sAggr = sQuerySet->SFWGH->aggsDepth1;
        while( sAggr != NULL )
        {
            if( sAggr->aggr->indexArgument == 1 )
            {
                sPrev = sAggr;
                sAggr = sAggr->next;
                continue;
            }

            // BUG-37496
            // nested aggregation �� ����� ������ ����� �ùٸ��� �ʽ��ϴ�.
            if( sPrev == NULL )
            {
                sQuerySet->SFWGH->aggsDepth1 = sAggr->next;
            }
            else
            {
                sPrev->next = sAggr->next;
            }

            sAggr->next = sQuerySet->SFWGH->aggsDepth2;
            sQuerySet->SFWGH->aggsDepth2 = sAggr;

            if( sPrev == NULL )
            {
                sAggr = sQuerySet->SFWGH->aggsDepth1;
            }
            else
            {
                sAggr = sPrev->next;
            }
        }
    }

    if ( (sQuerySet->SFWGH->group != NULL) ||
         (sQuerySet->SFWGH->aggsDepth1 != NULL) )
    {
        IDE_TEST( qmgGrouping::init( aStatement,
                                     sQuerySet,
                                     sMyGraph,      // child
                                     ID_FALSE,      // is nested
                                     &sMyGraph )    // result graph
                  != IDE_SUCCESS );

        IDE_TEST ( qmgGrouping::optimize( aStatement, sMyGraph )
                   != IDE_SUCCESS);
    }
    else
    {
        // Nohting To Do
    }

    //------------------------------------------
    // Nested Grouping�� ó��
    //------------------------------------------

    if ( sQuerySet->SFWGH->aggsDepth2 != NULL )
    {
        IDE_TEST( qmgGrouping::init( aStatement,
                                     sQuerySet,
                                     sMyGraph,         // child
                                     ID_TRUE,          // is nested
                                     &sMyGraph )       // result graph
                  != IDE_SUCCESS );

        IDE_TEST( qmgGrouping::optimize( aStatement, sMyGraph )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nohting To Do
    }

    //------------------------------------------
    // Windowing�� ó��
    //------------------------------------------

    if ( sQuerySet->analyticFuncList != NULL )
    {
        IDE_TEST( qmgWindowing::init( aStatement,
                                      sQuerySet,
                                      sMyGraph,
                                      & sMyGraph )
                  != IDE_SUCCESS );

        IDE_TEST( qmgWindowing::optimize( aStatement, sMyGraph )
                  != IDE_SUCCESS);
    }
    else
    {
        // nothing to do
    }

    //------------------------------------------
    // Distinction�� ó��
    //------------------------------------------

    if ( sQuerySet->SFWGH->selectType == QMS_DISTINCT )
    {
        IDE_TEST( qmgDistinction::init( aStatement,
                                        sQuerySet,
                                        sMyGraph,         // child
                                        &sMyGraph )       // result graph
                  != IDE_SUCCESS );

        IDE_TEST( qmgDistinction::optimize( aStatement, sMyGraph )
                  != IDE_SUCCESS);
    }
    else
    {
        // Nohting To Do
    }

    // ��� Graph ����
    sNonCrtPath->myGraph = sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoNonCrtPathMgr::optimizeNonLeaf( qcStatement   * aStatement,
                                   qmoNonCrtPath * aNonCrtPath )
{
/***********************************************************************
 *
 * Description : Non-Leaf Non Critical Path�� ����ȭ
 *
 * Implementation :
 *    (1) left, right ����ȭ �Լ� ȣ��
 *    (2) qmgSet �ʱ�ȭ
 *    (3) qmgSet ����ȭ
 *    (4) ��� graph�� qmoNonCrtPath::myGraph�� ����
 *
 ***********************************************************************/

    qmoNonCrtPath * sNonCrtPath = NULL;
    qmsQuerySet   * sQuerySet = NULL;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::optimizeNonLeaf::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNonCrtPath != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNonCrtPath = aNonCrtPath;
    sQuerySet   = sNonCrtPath->myQuerySet;

    // PROJ-2582 recursive with
    if ( ( sQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
         == QMV_QUERYSET_RECURSIVE_VIEW_TOP )
    {
        IDE_TEST( optimizeSetRecursive( aStatement,
                                        aNonCrtPath )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( optimizeSet( aStatement,
                               aNonCrtPath )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoNonCrtPathMgr::optimizeSet( qcStatement   * aStatement,
                                      qmoNonCrtPath * aNonCrtPath )
{
/***********************************************************************
 *
 * Description : Non-Leaf Non Critical Path�� ����ȭ
 *
 * Implementation :
 *    (1) left, right ����ȭ �Լ� ȣ��
 *    (2) qmgSet �ʱ�ȭ
 *    (3) qmgSet ����ȭ
 *    (4) ��� graph�� qmoNonCrtPath::myGraph�� ����
 *
 ***********************************************************************/

    qmoNonCrtPath * sNonCrtPath;
    qmsQuerySet   * sQuerySet;
    qmgGraph      * sMyGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::optimizeSet::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNonCrtPath != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNonCrtPath = aNonCrtPath;
    sQuerySet   = sNonCrtPath->myQuerySet;

    //------------------------------------------
    // left, right ����ȭ �Լ� ȣ��
    //------------------------------------------

    IDE_TEST( optimize( aStatement, sNonCrtPath->left ) != IDE_SUCCESS );
    IDE_TEST( optimize( aStatement, sNonCrtPath->right ) != IDE_SUCCESS );

    // To Fix PR-9567
    // Non Leaf Query Set�� ���
    // Child Query Set�� ������ ���� �پ��� ���·� ������ �� �ִ�.
    // ----------------------------------------------------
    // Left   :  Leaf   | Leaf     | Non-Leaf | Non-Leaf
    // Right  :  Leaf   | Non-Leaf | Leaf     | Non-Leaf
    // ----------------------------------------------------
    // ���� ���� �پ��� ó���� ���Ͽ�
    // Left Query Set�� Right Query Set�� ������ ó���Ͽ��� �Ѵ�.

    if ( sNonCrtPath->left->myQuerySet->setOp == QMS_NONE )
    {
        //------------------------------------------
        // left projection graph�� ����
        //------------------------------------------

        IDE_TEST( qmgProjection::init( aStatement,
                                       aNonCrtPath->left->myQuerySet,
                                       aNonCrtPath->left->myGraph,
                                       & aNonCrtPath->left->myGraph )
                  != IDE_SUCCESS );

        IDE_TEST( qmgProjection::optimize( aStatement,
                                           aNonCrtPath->left->myGraph )
                  != IDE_SUCCESS );
    }
    else
    {
        // To Fix PR-9567
        // ���� Query Set�� SET�� �ǹ��� ���
        // Parent�� SET���� ����Ͽ��� ��.
        aNonCrtPath->left->myGraph->flag &= ~QMG_SET_PARENT_TYPE_SET_MASK;
        aNonCrtPath->left->myGraph->flag |= QMG_SET_PARENT_TYPE_SET_TRUE;
    }

    if ( sNonCrtPath->right->myQuerySet->setOp == QMS_NONE )
    {
        //------------------------------------------
        // right projection graph�� ����
        //------------------------------------------

        IDE_TEST( qmgProjection::init( aStatement,
                                       aNonCrtPath->right->myQuerySet,
                                       aNonCrtPath->right->myGraph,
                                       & aNonCrtPath->right->myGraph )
                  != IDE_SUCCESS );

        IDE_TEST( qmgProjection::optimize( aStatement,
                                           aNonCrtPath->right->myGraph )
                  != IDE_SUCCESS );
    }
    else
    {
        // To Fix PR-9567
        // ���� Query Set�� SET�� �ǹ��� ���
        // Parent�� SET���� ����Ͽ��� ��.
        aNonCrtPath->right->myGraph->flag &= ~QMG_SET_PARENT_TYPE_SET_MASK;
        aNonCrtPath->right->myGraph->flag |= QMG_SET_PARENT_TYPE_SET_TRUE;
    }

    // qmgSet �ʱ�ȭ
    IDE_TEST( qmgSet::init( aStatement,
                            sQuerySet,
                            sNonCrtPath->left->myGraph, // left child
                            sNonCrtPath->right->myGraph,// right child
                            &sMyGraph )                 // result graph
              != IDE_SUCCESS );

    // qmgSet ����ȭ
    IDE_TEST( qmgSet::optimize( aStatement, sMyGraph ) != IDE_SUCCESS );

    // ��� Graph ����
    sNonCrtPath->myGraph = sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoNonCrtPathMgr::optimizeSetRecursive( qcStatement   * aStatement,
                                               qmoNonCrtPath * aNonCrtPath )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *    Non-Leaf Non Critical Path ( recrusvie union all)�� ����ȭ
 *
 * Implementation :
 *    (1) left, right ����ȭ �Լ� ȣ��
 *    (2) qmgSetRecursive �ʱ�ȭ
 *    (3) qmgSetRecursive ����ȭ
 *    (4) ��� graph�� qmoNonCrtPath::myGraph�� ����
 *
 ***********************************************************************/

    qmoNonCrtPath * sNonCrtPath = NULL;
    qmsQuerySet   * sQuerySet = NULL;
    qmgGraph      * sMyGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmoNonCrtPathMgr::optimizeSetRecursive::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNonCrtPath != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNonCrtPath = aNonCrtPath;
    sQuerySet   = sNonCrtPath->myQuerySet;

    //------------------------------------------
    // left ����ȭ �Լ� ȣ��
    //------------------------------------------

    IDE_TEST( optimize( aStatement, sNonCrtPath->left )
              != IDE_SUCCESS );

    //------------------------------------------
    // left projection graph�� ����
    //------------------------------------------

    // Recursive Query Set�� ���
    // Child Query Set�� Leaf�� �����ϴ�.
    IDE_TEST_RAISE( ( sNonCrtPath->left->myQuerySet->setOp != QMS_NONE ) ||
                    ( sNonCrtPath->right->myQuerySet->setOp != QMS_NONE ),
                    ERR_INVALID_CHILD );

    IDE_TEST( qmgProjection::init( aStatement,
                                   aNonCrtPath->left->myQuerySet,
                                   aNonCrtPath->left->myGraph,
                                   & aNonCrtPath->left->myGraph )
              != IDE_SUCCESS );

    IDE_TEST( qmgProjection::optimize( aStatement,
                                       aNonCrtPath->left->myGraph )
              != IDE_SUCCESS );

    //------------------------------------------
    // right ����ȭ �Լ� ȣ��
    //------------------------------------------

    // �ӽ÷� recursive view�� left�� ����ϰ�
    // right�� graph�� ������ �� ����Ѵ�.
    aStatement->myPlan->graph = aNonCrtPath->left->myGraph;

    IDE_TEST( optimize( aStatement, sNonCrtPath->right )
              != IDE_SUCCESS );

    //------------------------------------------
    // right projection graph�� ����
    //------------------------------------------

    IDE_TEST( qmgProjection::init( aStatement,
                                   aNonCrtPath->right->myQuerySet,
                                   aNonCrtPath->right->myGraph,
                                   & aNonCrtPath->right->myGraph )
              != IDE_SUCCESS );

    IDE_TEST( qmgProjection::optimize( aStatement,
                                       aNonCrtPath->right->myGraph )
              != IDE_SUCCESS );

    //------------------------------------------
    // set recursive graph�� ����
    //------------------------------------------

    // right query�� ���� recursive view graph�� �޷��־�� �Ѵ�.
    IDE_DASSERT( aStatement->myPlan->graph != NULL );

    // qmgSet �ʱ�ȭ
    IDE_TEST( qmgSetRecursive::init( aStatement,
                                     sQuerySet,
                                     sNonCrtPath->left->myGraph, // left child
                                     sNonCrtPath->right->myGraph,// right child
                                     aStatement->myPlan->graph,  // right query�� ���� recursive view
                                     & sMyGraph )                 // result graph
              != IDE_SUCCESS );

    // qmgSet ����ȭ
    IDE_TEST( qmgSetRecursive::optimize( aStatement, sMyGraph )
              != IDE_SUCCESS );

    // ��� Graph ����
    sNonCrtPath->myGraph = sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_CHILD )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoNonCrtPathMgr::optimizeSetRecursive",
                                  "invalid children" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
