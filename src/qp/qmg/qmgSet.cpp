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
 * $Id: qmgSet.cpp 89426 2020-12-04 00:47:19Z donovan.seo $
 *
 * Description :
 *     SET Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgSet.h>
#include <qmoCost.h>
#include <qmoSelectivity.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoTwoMtrPlan.h>
#include <qmoMultiNonPlan.h>

IDE_RC
qmgSet::init( qcStatement * aStatement,
              qmsQuerySet * aQuerySet,
              qmgGraph    * aLeftGraph,
              qmgGraph    * aRightGraph,
              qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgSet�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgSet�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgSET      * sMyGraph;
    qmsQuerySet * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgSet::init::__FT__" );

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

    // qmgSet�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgSET ),
                                             (void**) & sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_SET;

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->setOp = aQuerySet->setOp;

    if ( sMyGraph->graph.myQuerySet->setOp == QMS_INTERSECT )
    {
        if ( aLeftGraph->costInfo.outputRecordCnt >
             aRightGraph->costInfo.outputRecordCnt )
        {
            sMyGraph->graph.left = aRightGraph;
            sMyGraph->graph.right = aLeftGraph;
        }
        else
        {
            sMyGraph->graph.left = aLeftGraph;
            sMyGraph->graph.right = aRightGraph;
        }
    }
    else
    {
        sMyGraph->graph.left = aLeftGraph;
        sMyGraph->graph.right = aRightGraph;
    }

    // PROJ-1358
    // SET�� ��� Child�� Dependency�� �������� �ʰ�,
    // �ڽ��� VIEW�� ���� dependency�� �����Ѵ�.

    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aQuerySet->depInfo );

    sMyGraph->graph.optimize = qmgSet::optimize;
    sMyGraph->graph.makePlan = qmgSet::makePlan;
    sMyGraph->graph.printGraph = qmgSet::printGraph;

    // DISK/MEMORY ���� ����
    for ( sQuerySet = aQuerySet;
          sQuerySet->left != NULL;
          sQuerySet = sQuerySet->left ) ;

    switch( sQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            if ( sMyGraph->graph.myQuerySet->setOp == QMS_MINUS ||
                 sMyGraph->graph.myQuerySet->setOp == QMS_INTERSECT )
            {
                // left �� ������.
                sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
                sMyGraph->graph.flag |=
                    (aLeftGraph->flag & QMG_GRAPH_TYPE_MASK );
            }
            else
            {
                // left �Ǵ� right�� disk�̸� disk,
                // �׷��� ���� ��� memory
                if ( ( ( aLeftGraph->flag & QMG_GRAPH_TYPE_MASK )
                       == QMG_GRAPH_TYPE_DISK ) ||
                     ( ( aRightGraph->flag & QMG_GRAPH_TYPE_MASK )
                       == QMG_GRAPH_TYPE_DISK ) )
                {
                    sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
                    sMyGraph->graph.flag |= QMG_GRAPH_TYPE_DISK;
                }
                else
                {
                    sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
                    sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;
                }
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

    // out ����
    *aGraph = (qmgGraph*)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSet::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSet�� ����ȭ
 *
 * Implementation :
 *    (1) setOp�� UNION ALL�� �ƴ� ���, ������ ����
 *        A. bucketCnt ����
 *        B. setOp�� intersect �� ���, ���� query ����ȭ
 *        C. interResultType ����
 *    (2) ���� ��� ���� ����
 *    (3) Preserved Order, DISK/MEMORY ����
 *    (4) Multple Bag Union ����ȭ ����
 *
 ***********************************************************************/

    qmgSET      * sMyGraph;
    qmsQuerySet * sQuerySet;
    SDouble       sCost;

    IDU_FIT_POINT_FATAL( "qmgSet::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph = (qmgSET*) aGraph;

    // To Fix BUG-8355
    // bucket count ����
    if ( sMyGraph->graph.myQuerySet->setOp == QMS_UNION_ALL )
    {
        // bucket count �ʿ����
    }
    else
    {
        for ( sQuerySet = sMyGraph->graph.myQuerySet;
              sQuerySet->left != NULL;
              sQuerySet = sQuerySet->left ) ;

        IDE_TEST(
            qmg::getBucketCntWithTarget(
                aStatement,
                & sMyGraph->graph,
                sMyGraph->graph.myQuerySet->target,
                sQuerySet->SFWGH->hints->hashBucketCnt,
                & sMyGraph->hashBucketCnt )
            != IDE_SUCCESS );
    }

    //------------------------------------------
    // cost ���
    //------------------------------------------
    if ( sMyGraph->graph.myQuerySet->setOp != QMS_UNION_ALL )
    {
        if( (sMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
            QMG_GRAPH_TYPE_MEMORY )
        {
            sCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                 &(aGraph->left->costInfo),
                                                 1 );

            sMyGraph->graph.costInfo.myAccessCost = sCost;
            sMyGraph->graph.costInfo.myDiskCost   = 0;
        }
        else
        {
            sCost = qmoCost::getDiskHashTempCost( aStatement->mSysStat,
                                                  &(aGraph->left->costInfo),
                                                  1,
                                                  sMyGraph->graph.left->costInfo.recordSize );

            sMyGraph->graph.costInfo.myAccessCost = 0;
            sMyGraph->graph.costInfo.myDiskCost   = sCost;
        }
    }
    else
    {
        sMyGraph->graph.costInfo.myAccessCost = 0;
        sMyGraph->graph.costInfo.myDiskCost   = 0;
    }

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
    IDE_TEST( qmoSelectivity::setSetOutputCnt(
                  sMyGraph->graph.myQuerySet->setOp,
                  sMyGraph->graph.left->costInfo.outputRecordCnt,
                  sMyGraph->graph.right->costInfo.outputRecordCnt,
                  & sMyGraph->graph.costInfo.outputRecordCnt )
              != IDE_SUCCESS );

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
    // To fix BUG-15296
    // Set������ hashtemptable�� ����ϱ� ������
    // Intersect, Minus�� ���� order�� ������ �� ����.
    // union all�� �������� order���� �ȵ�.
    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;

    //-----------------------------------------
    // PROJ-1486 Multiple Bag Union ����ȭ
    //-----------------------------------------

    // BUG-42333
    if ( ( sMyGraph->graph.myQuerySet->setOp == QMS_UNION ) ||
         ( sMyGraph->graph.myQuerySet->setOp == QMS_UNION_ALL ) )
    {
        IDE_TEST( optMultiBagUnion( aStatement, sMyGraph ) != IDE_SUCCESS );
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
qmgSet::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgSet���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *     - qmgSet���� ���� ���������� Plan
 *
 *         - UNION ALL �� ���
 *
 *               ( [PROJ] ) : parent�� SET�� ��� ������
 *                   |
 *                 [VIEW]
 *                   |
 *                 [BUNI]
 *                 |    |
 *
 *         - UNION�� ���
 *
 *               ( [PROJ] ) : parent�� SET�� ��� ������
 *                   |
 *                 [HSDS]
 *                   |
 *                 [VIEW]
 *                   |
 *                 [BUNI]
 *                 |    |
 *
 *         - INTERSECT�� ���
 *
 *               ( [PROJ] ) : parent�� SET�� ��� ������
 *                   |
 *                 [VIEW]
 *                   |
 *                 [SITS]
 *                 |    |
 *
 *         - MINUS�� ���
 *
 *               ( [PROJ] ) : parent�� SET�� ��� ������
 *                   |
 *                 [VIEW]
 *                   |
 *                 [SDIF]
 *                 |    |
 *
 ***********************************************************************/

    qmgSET          * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgSet::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgSET*) aGraph;

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    sMyGraph->graph.myPlan = aParent->myPlan;

    switch( sMyGraph->setOp )
    {
        //---------------------------------------------------
        // UNION������ ���� BUNI����� ����
        //---------------------------------------------------
        case QMS_UNION:
            IDE_TEST( makeUnion( aStatement,
                                 sMyGraph,
                                 ID_FALSE )
                      != IDE_SUCCESS );
            break;
        case QMS_UNION_ALL:
            IDE_TEST( makeUnion( aStatement,
                                 sMyGraph,
                                 ID_TRUE )
                      != IDE_SUCCESS );
            break;
        case QMS_MINUS:
            IDE_TEST( makeMinus( aStatement,
                                 sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMS_INTERSECT:
            IDE_TEST( makeIntersect( aStatement,
                                     sMyGraph )
                      != IDE_SUCCESS );
            break;
        case QMS_NONE :
        default:
            IDE_DASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSet::makeChild( qcStatement * aStatement,
                   qmgSET      * aMyGraph )
{
    qmgChildren * sChildren;

    IDU_FIT_POINT_FATAL( "qmgSet::makeChild::__FT__" );

    if ( aMyGraph->graph.children == NULL )
    {
        // BUG-38410
        // SCAN parallel flag �� �ڽ� ���� �����ش�.
        aMyGraph->graph.left->flag  |= (aMyGraph->graph.flag &
                                        QMG_PLAN_EXEC_REPEATED_MASK);
        aMyGraph->graph.right->flag |= (aMyGraph->graph.flag &
                                        QMG_PLAN_EXEC_REPEATED_MASK);

        IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                                  &aMyGraph->graph ,
                                                  aMyGraph->graph.left )
                  != IDE_SUCCESS);
        IDE_TEST( aMyGraph->graph.right->makePlan( aStatement ,
                                                   &aMyGraph->graph ,
                                                   aMyGraph->graph.right )
                  != IDE_SUCCESS);
    }
    else
    {
        // PROJ-1486
        // Multi Graph�� children���� plan tree�� �����Ѵ�.
        for( sChildren = aMyGraph->graph.children;
             sChildren != NULL;
             sChildren = sChildren->next )
        {
            // BUG-38410
            // SCAN parallel flag �� �ڽ� ���� �����ش�.
            sChildren->childGraph->flag |= (aMyGraph->graph.flag &
                                            QMG_PLAN_EXEC_REPEATED_MASK);

            IDE_TEST(
                sChildren->childGraph->makePlan( aStatement ,
                                                 &aMyGraph->graph ,
                                                 sChildren->childGraph )
                != IDE_SUCCESS);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgSet::makeUnion( qcStatement * aStatement,
                   qmgSET      * aMyGraph,
                   idBool        aIsUnionAll )
{
    qmnPlan * sPROJ = NULL;
    qmnPlan * sHSDS = NULL;
    qmnPlan * sVIEW = NULL;
    qmnPlan * sBUNI = NULL;

    UInt      sFlag;

    IDU_FIT_POINT_FATAL( "qmgSet::makeUnion::__FT__" );

    //-----------------------------------------------------
    //        [*PROJ]
    //           |
    //        [*HSDS]
    //           |
    //        [VIEW]
    //           |
    //        [MultiBUNI]
    //           | 
    //        [Children] - [Children]
    //           |           |
    //        [LEFT]       [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //---------------------------------------------------
    // Parent�� SET�� ��� PROJ�� �����Ѵ�.
    //---------------------------------------------------
    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // init PROJ 
        //-----------------------

        IDE_TEST( qmoOneNonPlan::initPROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           NULL,
                                           &sPROJ ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sPROJ;
    }
    else
    {
        // nothing to do
    }

    if( aIsUnionAll == ID_FALSE )
    {
        //-----------------------
        // init HSDS
        //-----------------------

        IDE_TEST( qmoOneMtrPlan::initHSDS( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           ID_FALSE,
                                           aMyGraph->graph.myPlan ,
                                           &sHSDS ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sHSDS;
    }
    else
    {
        // nothing to do
    }

    //-----------------------
    // init VIEW
    //-----------------------

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan ,
                                       &sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    //-----------------------
    // init MultiBUNI
    //-----------------------
    IDE_DASSERT( aMyGraph->graph.children != NULL );

    IDE_TEST(
        qmoMultiNonPlan::initMultiBUNI( aStatement,
                                        aMyGraph->graph.myQuerySet,
                                        aMyGraph->graph.myPlan,
                                        & sBUNI )
        != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sBUNI;

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
    // make MultiBUNI
    //-----------------------

    IDE_TEST(
        qmoMultiNonPlan::makeMultiBUNI( aStatement,
                                        aMyGraph->graph.myQuerySet,
                                        aMyGraph->graph.children,
                                        sBUNI )
        != IDE_SUCCESS );

    aMyGraph->graph.myPlan = sBUNI;

    qmg::setPlanInfo( aStatement, sBUNI, &(aMyGraph->graph) );

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
                                       sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aMyGraph->graph) );

    if( aIsUnionAll == ID_FALSE )
    {
        //-----------------------
        // make HSDS
        //-----------------------

        sFlag = 0;
        sFlag &= ~QMO_MAKEHSDS_METHOD_MASK;
        sFlag |= QMO_MAKEHSDS_SET_UNION;

        //Temp Table���� ��ü ����
        if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
            QMG_GRAPH_TYPE_MEMORY )
        {
            sFlag &= ~QMO_MAKEHSDS_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEHSDS_MEMORY_TEMP_TABLE;
        }
        else
        {
            sFlag &= ~QMO_MAKEHSDS_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEHSDS_DISK_TEMP_TABLE;
        }

        IDE_TEST( qmoOneMtrPlan::makeHSDS( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           sFlag ,
                                           aMyGraph->hashBucketCnt ,
                                           aMyGraph->graph.myPlan ,
                                           sHSDS ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sHSDS;

        qmg::setPlanInfo( aStatement, sHSDS, &(aMyGraph->graph) );
    }
    else
    {
        // Nothing to do.
    }

    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // init PROJ 
        //-----------------------

        sFlag = 0;
        sFlag &= ~QMO_MAKEPROJ_TOP_MASK;
        sFlag |= QMO_MAKEPROJ_TOP_FALSE;

        sFlag &= ~QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK;
        sFlag |= QMO_MAKEPROJ_INDEXABLE_MINMAX_FALSE;

        IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           sFlag ,
                                           NULL ,
                                           NULL ,
                                           aMyGraph->graph.myPlan ,
                                           sPROJ ) != IDE_SUCCESS);

        aMyGraph->graph.myPlan = sPROJ;

        qmg::setPlanInfo( aStatement, sPROJ, &(aMyGraph->graph) );
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
qmgSet::makeIntersect( qcStatement * aStatement,
                       qmgSET      * aMyGraph )
{
    qmnPlan * sPROJ = NULL;
    qmnPlan * sVIEW = NULL;
    qmnPlan * sSITS = NULL;

    UInt      sFlag;

    IDU_FIT_POINT_FATAL( "qmgSet::makeIntersect::__FT__" );

    //-----------------------------------------------------
    //        [*PROJ]
    //           |
    //        [VIEW]
    //           |
    //        [SITS]
    //         /  `.
    //    [LEFT]  [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //---------------------------------------------------
    // Parent�� SET�� ��� PROJ�� �����Ѵ�.
    //---------------------------------------------------
    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // init PROJ 
        //-----------------------

        IDE_TEST( qmoOneNonPlan::initPROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           NULL,
                                           &sPROJ ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sPROJ;
    }
    else
    {
        // nothing to do
    }

    //-----------------------
    // init VIEW
    //-----------------------

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan ,
                                       &sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;
    //-----------------------
    // init SITS
    //-----------------------

    IDE_TEST( qmoTwoMtrPlan::initSITS( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan,
                                       &sSITS )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sSITS;

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
    // make SITS
    //-----------------------

    sFlag = 0;
    if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKESITS_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESITS_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKESITS_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESITS_DISK_TEMP_TABLE;
    }

    IDE_TEST( qmoTwoMtrPlan::makeSITS( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       aMyGraph->hashBucketCnt ,
                                       aMyGraph->graph.left->myPlan ,
                                       aMyGraph->graph.right->myPlan ,
                                       sSITS ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sSITS;

    qmg::setPlanInfo( aStatement, sSITS, &(aMyGraph->graph) );

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
                                       sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aMyGraph->graph) );

    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // make PROJ 
        //-----------------------

        sFlag = 0;
        sFlag &= ~QMO_MAKEPROJ_TOP_MASK;
        sFlag |= QMO_MAKEPROJ_TOP_FALSE;

        sFlag &= ~QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK;
        sFlag |= QMO_MAKEPROJ_INDEXABLE_MINMAX_FALSE;

        IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           sFlag ,
                                           NULL ,
                                           NULL ,
                                           aMyGraph->graph.myPlan ,
                                           sPROJ ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sPROJ;

        qmg::setPlanInfo( aStatement, sPROJ, &(aMyGraph->graph) );
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
qmgSet::makeMinus( qcStatement * aStatement,
                   qmgSET      * aMyGraph )
{
    qmnPlan * sPROJ = NULL;
    qmnPlan * sVIEW = NULL;
    qmnPlan * sSDIF = NULL;

    UInt      sFlag;

    IDU_FIT_POINT_FATAL( "qmgSet::makeMinus::__FT__" );

    //-----------------------------------------------------
    //        [*PROJ]
    //           |
    //        [VIEW]
    //           |
    //        [SDIF]
    //         /  `.
    //    [LEFT]  [RIGHT]
    // * Option
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //---------------------------------------------------
    // Parent�� SET�� ��� PROJ�� �����Ѵ�.
    //---------------------------------------------------
    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // init PROJ 
        //-----------------------

        IDE_TEST( qmoOneNonPlan::initPROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           NULL,
                                           &sPROJ ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sPROJ;
    }
    else
    {
        // nothing to do
    }

    //-----------------------
    // init VIEW
    //-----------------------

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan ,
                                       &sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    //-----------------------
    // init SDIF
    //-----------------------

    IDE_TEST( qmoTwoMtrPlan::initSDIF( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan,
                                       &sSDIF )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sSDIF;

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
    // make SDIF
    //-----------------------

    sFlag = 0;
    if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKESDIF_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESDIF_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKESDIF_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESDIF_DISK_TEMP_TABLE;
    }

    IDE_TEST( qmoTwoMtrPlan::makeSDIF( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       aMyGraph->hashBucketCnt ,
                                       aMyGraph->graph.left->myPlan,
                                       aMyGraph->graph.right->myPlan,
                                       sSDIF ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sSDIF;

    qmg::setPlanInfo( aStatement, sSDIF, &(aMyGraph->graph) );

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
                                       sVIEW ) != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aMyGraph->graph) );

    if ( (aMyGraph->graph.flag & QMG_SET_PARENT_TYPE_SET_MASK) ==
         QMG_SET_PARENT_TYPE_SET_TRUE )
    {
        //-----------------------
        // make PROJ 
        //-----------------------

        sFlag = 0;
        sFlag &= ~QMO_MAKEPROJ_TOP_MASK;
        sFlag |= QMO_MAKEPROJ_TOP_FALSE;

        sFlag &= ~QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK;
        sFlag |= QMO_MAKEPROJ_INDEXABLE_MINMAX_FALSE;

        IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                           aMyGraph->graph.myQuerySet ,
                                           sFlag ,
                                           NULL ,
                                           NULL ,
                                           aMyGraph->graph.myPlan ,
                                           sPROJ ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sPROJ;

        qmg::setPlanInfo( aStatement, sPROJ, &(aMyGraph->graph) );
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
qmgSet::printGraph( qcStatement  * aStatement,
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

    qmgChildren * sChildren;

    IDU_FIT_POINT_FATAL( "qmgSet::printGraph::__FT__" );

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

    if ( aGraph->children == NULL )
    {
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
    }
    else
    {
        // Multiple Children�� ���� Graph ���� ���
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgSet::optMultiBagUnion( qcStatement * aStatement,
                          qmgSET      * aSETGraph )
{
/***********************************************************************
 *
 * Description : PROJ-1486
 *    Multiple Bag Union ����ȭ�� ������.
 *
 * Implementation :
 *
 *    Left�� Right Graph�� ���Ͽ� ���� ������ �����Ѵ�.
 *
 *    - 1. ������ ���� ���踦 ���´�.
 *    - 2. �� Child�� Graph������ �����Ѵ�.
 *    - 3. �� Child�� Data Type�� ��ġ��Ų��.
 *
 ***********************************************************************/

    qmgGraph    * sChildGraph;
    qmgChildren * sChildren;
    qmsTarget   * sCurTarget;
    qmsTarget   * sChildTarget;

    IDU_FIT_POINT_FATAL( "qmgSet::optMultiBagUnion::__FT__" );

    // BUG-42333
    IDE_DASSERT( ( aSETGraph->graph.myQuerySet->setOp == QMS_UNION_ALL ) ||
                 ( aSETGraph->graph.myQuerySet->setOp == QMS_UNION ) );

    //-------------------------------------------
    // Left Child�� ���� ó��
    //-------------------------------------------

    // 1. ������ ���� ���� ����
    sChildGraph = aSETGraph->graph.left;
    aSETGraph->graph.left = NULL;

    // 2. �� Child�� Graph������ �����Ѵ�.
    IDE_TEST( linkChildGraph( aStatement,
                              sChildGraph,
                              & aSETGraph->graph.children )
              != IDE_SUCCESS );

    //-------------------------------------------
    // Right Child�� ���� ó��
    //-------------------------------------------

    // 0. ���� ������ ���� ��ġ�� �̵�
    for ( sChildren = aSETGraph->graph.children;
          sChildren->next != NULL;
          sChildren = sChildren->next );

    // 1. ������ ���� ���� ����
    sChildGraph = aSETGraph->graph.right;
    aSETGraph->graph.right = NULL;

    // 2. �� Child�� Graph������ �����Ѵ�.
    IDE_TEST( linkChildGraph( aStatement,
                              sChildGraph,
                              & sChildren->next )
              != IDE_SUCCESS );

    //-------------------------------------------
    // 3. �� Child�� Target�� �����
    //    ���� Target�� ������ Data Type���� ��ġ��Ŵ.
    //-------------------------------------------

    for ( sChildren = aSETGraph->graph.children;
          sChildren != NULL;
          sChildren = sChildren->next )
    {
        for ( sCurTarget = aSETGraph->graph.myQuerySet->target,
                  sChildTarget = sChildren->childGraph->myQuerySet->target;
              ( sCurTarget != NULL ) && ( sChildTarget != NULL );
              sCurTarget = sCurTarget->next,
                  sChildTarget = sChildTarget->next )
        {
            // ������ Child Target�� conversion�� ����
            sChildTarget->targetColumn->node.conversion = NULL;

            // ���� Target�� ������ Data Type���� ��ġ��Ŵ
            // ���� Target�� ���� ������ Data Type���� ������� �ʴ´�.
            // ��, child target�� ���Ͽ� ������ ���� �ʴ´�.
            IDE_TEST(
                qtc::makeSameDataType4TwoNode( aStatement,
                                               sCurTarget->targetColumn,
                                               sChildTarget->targetColumn )
                != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgSet::linkChildGraph( qcStatement  * aStatement,
                        qmgGraph     * aChildGraph,
                        qmgChildren ** aChildren )
{
/***********************************************************************
 *
 * Description : PROJ-1486
 *     �ϳ��� Child Graph�� ���Ͽ� ������ �δ´�.
 *
 * Implementation :
 *
 *    - 2-1. Child�� PROJECTION or UNION_ALL �� �ƴ� SET�� ���
 *           �ش� Graph�� children�� ����
 *    - 2-2. Child�� UNOIN_ALL �� ���
 *    - 2-2-1. children�� ���� ��� �̸� �����Ѵ�.
 *    - 2-2-2. children�� ���� ��� �ش� Graph�� children�� ����
 *
 ***********************************************************************/

    qmgChildren * sChildren;
    qmgChildren * sNextChildren;

    IDU_FIT_POINT_FATAL( "qmgSet::linkChildGraph::__FT__" );

    if ( ( aChildGraph->type == QMG_PROJECTION ) ||
         ( ( aChildGraph->type == QMG_SET ) &&
           ( ((qmgSET*)aChildGraph)->setOp != QMS_UNION_ALL ) ) )
    {
        // BUG-42333
        //---------------------------------------
        // 2-1. Projection or UNION_ALL �� �ƴ� SET �� ����� ����
        //---------------------------------------

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgChildren) ,
                                                 (void **) & sChildren )
                  != IDE_SUCCESS );

        sChildren->childGraph = aChildGraph;
        sChildren->next = NULL;

        *aChildren = sChildren;
    }
    else
    {
        //---------------------------------------
        // 2-2. UNION_ALL �� ����� ����
        //---------------------------------------

        IDE_DASSERT( aChildGraph->type == QMG_SET );
        IDE_DASSERT( ((qmgSET*)aChildGraph)->setOp == QMS_UNION_ALL );

        if ( aChildGraph->children != NULL )
        {
            //---------------------------------------
            // 2-2-1. children�� ���� ��� �̸� �����Ѵ�.
            //---------------------------------------

            *aChildren = aChildGraph->children;
        }
        else
        {
            //---------------------------------------
            // 2-2-2. children�� ���� ��� �ش� Graph�� children�� ����
            //---------------------------------------

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgChildren) ,
                                                     (void **) & sChildren )
                      != IDE_SUCCESS );

            sChildren->childGraph = aChildGraph->left;
            sChildren->next = NULL;

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgChildren) ,
                                                     (void **) & sNextChildren )
                      != IDE_SUCCESS );

            sNextChildren->childGraph = aChildGraph->right;
            sNextChildren->next = NULL;

            sChildren->next = sNextChildren;
            *aChildren = sChildren;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
