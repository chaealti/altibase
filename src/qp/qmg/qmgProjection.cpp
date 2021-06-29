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
 * $Id: qmgProjection.cpp 90832 2021-05-13 23:43:20Z donovan.seo $
 *
 * Description :
 *     Projection Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgProjection.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoSelectivity.h>
#include <qmoCost.h>
#include <qmgSelection.h>
#include <qcgPlan.h>
#include <qmv.h>

IDE_RC
qmgProjection::init( qcStatement * aStatement,
                     qmsQuerySet * aQuerySet,
                     qmgGraph    * aChildGraph,
                     qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgProjection Graph�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgProjection�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ����) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgPROJ      * sMyGraph;
    qmsParseTree * sParseTree;
    qmsQuerySet  * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmgProjection::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Projection Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgProjection�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPROJ ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_PROJECTION;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    if ( ( aChildGraph->flag & QMG_INDEXABLE_MIN_MAX_MASK )
         == QMG_INDEXABLE_MIN_MAX_TRUE )
    {
        sMyGraph->graph.flag &= ~QMG_INDEXABLE_MIN_MAX_MASK;
        sMyGraph->graph.flag |= QMG_INDEXABLE_MIN_MAX_TRUE;
    }
    else
    {
        // nothing to do
    }

    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgProjection::optimize;
    sMyGraph->graph.makePlan = qmgProjection::makePlan;
    sMyGraph->graph.printGraph = qmgProjection::printGraph;

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
    // Projection Graph ���� ���� �ʱ�ȭ
    //---------------------------------------------------

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    // To Fix BUG-9060
    // �ֻ��� projection���� limit�� �����ؾ���
    // set�� ���, �� set�� projection�� limit�� �����Ǵ� ������ �־���
    if ( sParseTree->querySet == aQuerySet )
    {
        // �ֻ��� projection���� limit�� ����
        sMyGraph->limit = sParseTree->limit;
        sMyGraph->loopNode = sParseTree->loopNode;
    }
    else
    {
        sMyGraph->limit = NULL;
        sMyGraph->loopNode = NULL;
    }
    
    /* BUG-36580 supported TOP */
    if ( aQuerySet->SFWGH != NULL )
    {
        if( aQuerySet->SFWGH->top != NULL )
        {
            IDE_DASSERT( sMyGraph->limit == NULL );

            sMyGraph->limit = aQuerySet->SFWGH->top;
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
    
    sMyGraph->target = aQuerySet->target;
    sMyGraph->hashBucketCnt = 0;

    // To Fix PR-7989
    sMyGraph->subqueryTipFlag = 0;
    sMyGraph->storeNSearchPred = NULL;
    sMyGraph->hashBucketCnt = 0;

    // out ����
    *aGraph = (qmgGraph *)sMyGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgProjection�� ����ȭ
 *
 * Implementation :
 *    (1) target�� Subquery Graph ����
 *    (2) Indexable MIN MAX Tip�� ����� ���, limit 1�� �����Ѵ�.
 *    (3) SCAN Limit ����ȭ
 *    (4) ���� ��� ���� ����
 *    (5) Preserved Order
 *
 ***********************************************************************/

    qmgPROJ           * sMyGraph;
    qmgGraph          * sChildGraph;
    qtcNode           * sNode;
    qmsLimit          * sLimit;
    idBool              sIsScanLimit;
    qmsQuerySet       * sQuerySet;
    qmsTarget         * sTarget;
    mtcColumn         * sMtcColumn;

    SDouble             sRecordSize;
    SDouble             sOutputRecordCnt;

    // BUG-48779
    qmgChildren       * sCurrChildGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmgProjection::optimize::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph = (qmgPROJ*) aGraph;
    sChildGraph = aGraph->left;
    sRecordSize = 0;

    //---------------------------------------------------
    // target�� Subquery Graph ����
    //---------------------------------------------------

    for ( sTarget = sMyGraph->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sNode = sTarget->targetColumn;

        IDE_TEST( qmoSubquery::optimize( aStatement,
                                         sNode,
                                         ID_FALSE ) // No KeyRange Tip
                  != IDE_SUCCESS );

        sMtcColumn = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
        sRecordSize += sMtcColumn->column.size;
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    //---------------------------------------------------
    // SCAN Limit ����ȭ
    //---------------------------------------------------

    sIsScanLimit = ID_FALSE;
    if ( sMyGraph->limit != NULL )
    {
        if ( sChildGraph->type == QMG_SELECTION )
        {
            canPushDownLimitIntoScan( sChildGraph,
                                      &sIsScanLimit );
        }
        else if ( sChildGraph->type == QMG_PARTITION )
        {
            /* BUG-48779
             * �Ʒ��� ���� ���·� PROJECTION�� SCAN���̿� PARTITION-COORDINATOR�� �� ���
             * ���������� LIMIT�� SCAN���� �����ֵ��� �Ѵ�.
             *
             * PROJECT
             *  PARTITION-COODINATOR
             *   SCAN ( PARTITION : P3 )
             *   SCAN ( PARTITION : P2 )
             *   SCAN ( PARTITION : P1 )
             * ...
             */
            if ( sMyGraph->limit->start.hostBindNode == NULL )
            {
                // limit is constant value
                if ( sMyGraph->limit->start.constant == ID_ULONG(1) )
                {
                    // limit start value is 1
                    for ( sCurrChildGraph = sChildGraph->children;
                          sCurrChildGraph != NULL;
                          sCurrChildGraph = sCurrChildGraph->next )
                    {
                        canPushDownLimitIntoScan( sCurrChildGraph->childGraph,
                                                  &sIsScanLimit );

                        if ( sIsScanLimit == ID_FALSE )
                        {
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
                else
                {
                    // start value �� 1 �� �ƴ� ��쿡�� scan limit���� �������� �ʴ´�.
                    // Nothing to do.
                }
            }
            else
            {
                // limit�� value�� bind param �� ��쿡�� scan limit���� �������� �ʴ´�.
                // Nothing to do
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
        if ( sChildGraph->type == QMG_SELECTION )
        {
            ((qmgSELT *)sChildGraph)->limit = sMyGraph->limit;
        }
        else
        {
            for ( sCurrChildGraph = sChildGraph->children;
                  sCurrChildGraph != NULL;
                  sCurrChildGraph = sCurrChildGraph->next )
            {
                ((qmgSELT *)(sCurrChildGraph->childGraph))->limit = sMyGraph->limit;
            }
        }

        // To Fix BUG-9560
        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmsLimit ),
                                           ( void**) & sLimit )
            != IDE_SUCCESS );

        // fix BUG-13482
        // parse tree�� limit������ ���� ������,
        // PROJ ��� ������,
        // ���� SCAN ��忡�� SCAN Limit ������ Ȯ���Ǹ�,
        // PROJ ����� limit start�� 1�� �����Ѵ�.

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
    // Indexable Min Max Tip�� ����� ���, Limit 1 ����
    //---------------------------------------------------

    if ( ( sMyGraph->graph.flag & QMG_INDEXABLE_MIN_MAX_MASK )
         == QMG_INDEXABLE_MIN_MAX_TRUE )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmsLimit ),
                                                 (void**) & sLimit )
                  != IDE_SUCCESS );

        qmsLimitI::setStartValue( sLimit, 1 );
        qmsLimitI::setCountValue( sLimit, 1 );
        SET_EMPTY_POSITION( sLimit->limitPos );
        sMyGraph->limit = sLimit;
    }
    else
    {
        // nothing to do
    }

    //---------------------------------------------------
    // hash bucket count ����
    //---------------------------------------------------

    for ( sQuerySet = sMyGraph->graph.myQuerySet;
          sQuerySet->left != NULL;
          sQuerySet = sQuerySet->left ) ;

    IDE_TEST(
        qmg::getBucketCntWithTarget( aStatement,
                                     & sMyGraph->graph,
                                     sMyGraph->target,
                                     sQuerySet->SFWGH->hints->hashBucketCnt,
                                     & sMyGraph->hashBucketCnt )
        != IDE_SUCCESS );

    //---------------------------------------------------
    // ���� ��� ���� ����
    //---------------------------------------------------

    // recordSize
    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // selectivity
    IDE_TEST( qmoSelectivity::setProjectionSelectivity(
                  sMyGraph->limit,
                  sChildGraph->costInfo.outputRecordCnt,
                  & sMyGraph->graph.costInfo.selectivity )
              != IDE_SUCCESS );

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sChildGraph->costInfo.outputRecordCnt;

    // outputRecordCnt
    sOutputRecordCnt = sChildGraph->costInfo.outputRecordCnt *
        sMyGraph->graph.costInfo.selectivity;
    sOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;
    sMyGraph->graph.costInfo.outputRecordCnt = sOutputRecordCnt;

    sMyGraph->graph.costInfo.myAccessCost = qmoCost::getTargetCost( aStatement->mSysStat,
                                                                    sMyGraph->target,
                                                                    sOutputRecordCnt );

    // myCost
    sMyGraph->graph.costInfo.myDiskCost = 0;
    sMyGraph->graph.costInfo.myAllCost = sMyGraph->graph.costInfo.myAccessCost;

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
    // Preserved Order
    //    ������ Preserved Order�� �����ϴ� ���,
    //    Preserved Order�� �� Į���� ���Ͽ� �ش� Į����
    //    target�� ������ ��쿡��  Preserved Order�� ����
    //---------------------------------------------------

    if ( sChildGraph->preservedOrder != NULL )
    {
        IDE_TEST( makePreservedOrder( aStatement,
                                      sMyGraph->target,
                                      sMyGraph->graph.left->preservedOrder,
                                      & sMyGraph->graph.preservedOrder )
                  != IDE_SUCCESS );
        
        if ( sMyGraph->graph.preservedOrder != NULL )
        {
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
        }
        else
        {
            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
        }

        //---------------------------------------------------
        // BUG-32258
        // limit ���� �����ϰ�,
        // child�� preserved order direction�� not defined �̸�
        // direction�� �����ϵ��� ��Ų��.
        //---------------------------------------------------

        if ( sMyGraph->limit != NULL )
        {
            if ( sMyGraph->graph.preservedOrder != NULL )
            {
                IDE_TEST( qmg::finalizePreservedOrder( &sMyGraph->graph )
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
    }
    else
    {
        sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
        sMyGraph->graph.flag |=
            ( sChildGraph->flag & QMG_PRESERVED_ORDER_MASK );
    }

    sMyGraph->graph.flag &= ~QMG_PARALLEL_EXEC_MASK;
    sMyGraph->graph.flag |= (sChildGraph->flag & QMG_PARALLEL_EXEC_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgProjection::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgProjection���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *    - qmgProjection���� ���������� Plan
 *
 *        - Top �� ���
 *
 *           [PROJ]
 *
 *        - View Materialization �� ����� ���
 *
 *           [VMTR]
 *             |
 *           [VIEW]
 *             |
 *           [PROJ]
 *
 *        - Transform NJ�� ����� ���
 *
 *           [PROJ]
 *
 *        - Store And Search ����ȭ�� ����� ���
 *
 *           1.  Hash ���� ����� ���
 *
 *
 *               [PROJ]
 *                 |
 *               [HASH] : Filter�� ���Ե�, NOTNULLCHECK option ����.
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *           2.  LMST ���� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [LMST] : Filter�� �������� ����
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *           3.  STORE Only�� ���
 *
 *               [PROJ]
 *                 |
 *               [SORT]
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *        - SUBQUERY IN KEYRANGE ����ȭ�� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [HSDS]
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *        - SUBQUERY KEYRANGE ����ȭ�� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [SORT] : ���常 ��.
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 ***********************************************************************/

    qmgPROJ         * sMyGraph;
    UInt              sPROJFlag         = 0;
    qmnPlan         * sPROJ             = NULL;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgPROJ*) aGraph;

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
    if (aParent != NULL)
    {
        aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
        aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

        aGraph->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aGraph->flag |= (aParent->flag & QMG_PLAN_EXEC_REPEATED_MASK);
    }
    else
    {
        /*
         * QMG_PARALLEL_IMPOSSIBLE_MASK flag �� ��ġ�� �ʴ´�.
         * (�������� setting �Ѱ����� ����)
         * parent �� ���ٰ� �׻� �ֻ����ΰ� �ƴϴ�.
         */

        /* BUG-38410
         * �ֻ��� plan �̹Ƿ� �⺻���� �����Ѵ�.
         */
        aGraph->flag &= ~QMG_PLAN_EXEC_REPEATED_MASK;
        aGraph->flag |= QMG_PLAN_EXEC_REPEATED_FALSE;
    }

    // BUG-38410
    // SCAN parallel flag �� �ڽ� ���� �����ش�.
    aGraph->left->flag  |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);

    // INDEXABLE Min-Max�� ����
    if( (sMyGraph->graph.flag & QMG_INDEXABLE_MIN_MAX_MASK ) ==
        QMG_INDEXABLE_MIN_MAX_TRUE )
    {
        sPROJFlag &= ~QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK;
        sPROJFlag |= QMO_MAKEPROJ_INDEXABLE_MINMAX_TRUE;
    }
    else
    {
        sPROJFlag &= ~QMO_MAKEPROJ_INDEXABLE_MINMAX_MASK;
        sPROJFlag |= QMO_MAKEPROJ_INDEXABLE_MINMAX_FALSE;
    }

    if( (sMyGraph->graph.flag & QMG_PROJ_COMMUNICATION_TOP_PROJ_MASK) ==
        QMG_PROJ_COMMUNICATION_TOP_PROJ_TRUE )
    {
        // PROJ-2462 Top result Cache�� ��� ViewMTR�� �����Ŀ�
        // Projection�� �����ȴ�.
        if ( ( sMyGraph->graph.flag & QMG_PROJ_TOP_RESULT_CACHE_MASK )
             == QMG_PROJ_TOP_RESULT_CACHE_TRUE )
        {
            IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                               sMyGraph->graph.myQuerySet,
                                               sMyGraph->graph.myPlan,
                                               &sPROJ )
                      != IDE_SUCCESS );
            sMyGraph->graph.myPlan = sPROJ;

            sPROJFlag &= ~QMO_MAKEPROJ_TOP_MASK;
            sPROJFlag |= QMO_MAKEPROJ_TOP_FALSE;

            /* ViewMtr�� �����ϵ��� Mask ���� */
            sMyGraph->graph.flag &= ~QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK;
            sMyGraph->graph.flag |= QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE;
            /* VMTR�� ��ȯ�� Project Graph�� flag�� Memory ���� */
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |= QMG_GRAPH_TYPE_MEMORY;

            IDE_TEST( makePlan4ViewMtr( aStatement, sMyGraph, sPROJFlag )
                      != IDE_SUCCESS );

            sPROJFlag &= ~QMO_MAKEPROJ_TOP_MASK;
            sPROJFlag |= QMO_MAKEPROJ_TOP_TRUE;

            sPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
            sPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

            sPROJFlag &= ~QMO_MAKEPROJ_TOP_RESULT_CACHE_MASK;
            sPROJFlag |= QMO_MAKEPROJ_TOP_RESULT_CACHE_TRUE;

            IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                               sMyGraph->graph.myQuerySet ,
                                               sPROJFlag ,
                                               sMyGraph->limit ,
                                               sMyGraph->loopNode ,
                                               sMyGraph->graph.myPlan ,
                                               sPROJ )
                      != IDE_SUCCESS);
            sMyGraph->graph.myPlan = sPROJ;

            qmg::setPlanInfo( aStatement, sPROJ, &(sMyGraph->graph) );
        }
        else
        {
            //-----------------------------------------------------
            // Top�� ���
            //-----------------------------------------------------

            sPROJFlag &= ~QMO_MAKEPROJ_TOP_MASK;
            sPROJFlag |= QMO_MAKEPROJ_TOP_TRUE;

            sPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
            sPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

            //----------------------------
            // PROJ�� ����
            //----------------------------

            IDE_TEST( makePlan4Proj( aStatement,
                                     sMyGraph,
                                     sPROJFlag ) != IDE_SUCCESS );
        }
    }
    else
    {
        //-----------------------------------------------------
        // Top�� �ƴ� ���
        //-----------------------------------------------------

        if( aParent != NULL )
        {
            sMyGraph->graph.myPlan = aParent->myPlan;
        }
        else
        {
            sMyGraph->graph.myPlan = NULL;
        }

        // non-TOP [PROJ] �� ����� flag setting
        sPROJFlag &= ~QMO_MAKEPROJ_TOP_MASK;
        sPROJFlag |= QMO_MAKEPROJ_TOP_FALSE;

        // View Materialization�� ���� ����
        if( ( ( sMyGraph->graph.flag & QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK) ==
              QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE ) ||
            (  ( sMyGraph->graph.flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK) ==
               QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE ) )
        {
            IDE_TEST( makePlan4ViewMtr( aStatement, sMyGraph, sPROJFlag )
                      != IDE_SUCCESS );
        }
        else
        {
            //-----------------------------------------------------
            // View Optimization Type�� View Materialization�� �ƴѰ��
            //-----------------------------------------------------

            // Subquery Tip�� ����
            switch( sMyGraph->subqueryTipFlag & QMG_PROJ_SUBQUERY_TIP_MASK )
            {
                case QMG_PROJ_SUBQUERY_TIP_NONE:
                    //-----------------------------------------------------
                    // subquery tip�� ���� ���
                    //-----------------------------------------------------
                case QMG_PROJ_SUBQUERY_TIP_TRANSFORMNJ:
                    //-----------------------------------------------------
                    // Transform NJ�� ����� ���
                    //-----------------------------------------------------

                    //----------------------------
                    // [PROJ]�� ����
                    //----------------------------

                    IDE_TEST( makePlan4Proj( aStatement,
                                             sMyGraph,
                                             sPROJFlag ) != IDE_SUCCESS );

                    break;

                case QMG_PROJ_SUBQUERY_TIP_STORENSEARCH:
                    //-----------------------------------------------------
                    // Store And Search ����ȭ�� ����� ���
                    //-----------------------------------------------------

                    IDE_TEST ( makePlan4StoreSearch( aStatement,
                                                     sMyGraph,
                                                     sPROJFlag )
                               != IDE_SUCCESS );

                    break;

                case QMG_PROJ_SUBQUERY_TIP_IN_KEYRANGE:

                    //-----------------------------------------------------
                    // Subquery IN Keyrange ����ȭ�� ����� ���
                    //-----------------------------------------------------

                    IDE_TEST ( makePlan4InKeyRange( aStatement,
                                                    sMyGraph,
                                                    sPROJFlag )
                               != IDE_SUCCESS );

                    break;

                case QMG_PROJ_SUBQUERY_TIP_KEYRANGE:

                    //-----------------------------------------------------
                    // Subquery Keyrange ����ȭ�� ����� ���
                    //-----------------------------------------------------

                    IDE_TEST ( makePlan4KeyRange( aStatement,
                                                  sMyGraph,
                                                  sPROJFlag )
                               != IDE_SUCCESS );

                    break;

                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::printGraph( qcStatement  * aStatement,
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

    UInt        i;
    qmsTarget * sTarget;
    qmgPROJ   * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgProjection::printGraph::__FT__" );

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

    /* BUG-48969 target subquery graph display */
    sMyGraph = (qmgPROJ *)aGraph;
    for ( sTarget = sMyGraph->target; sTarget != NULL ; sTarget = sTarget->next )
    {
        IDE_TEST( qmg::printSubquery( aStatement,
                                      sTarget->targetColumn,
                                      aDepth,
                                      aString )
                  != IDE_SUCCESS );
    }

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


IDE_RC
qmgProjection::makePreservedOrder( qcStatement        * aStatement,
                                   qmsTarget          * aTarget,
                                   qmgPreservedOrder  * aChildPreservedOrder,
                                   qmgPreservedOrder ** aMyPreservedOrder )
{
/***********************************************************************
 *
 * Description : qmgProjection�� preserved order�� ����
 *
 * Implementation :
 *
 *    (1) Preserved Order�� �� Į���� ���Ͽ� �ش� Į���� target�� �����ϸ�
 *        preserved order ����,
 *        ������� preserved order�� �����ϴٰ� �ϳ��� �������� ������ �ߴ�
 *        ex ) target : i2, i1, i1 + i3
 *             child preserved order : i1, i2, i3
 *             my preserved order : i1, i2
 *
 *    (2) limit ���� �����ϰ� preserved order�� direction�� not defined �� ���,
 *        selection�� index ���� ���⿡ �°� direction�� �����ϵ��� �Ѵ�.
 *        ( BUG-32258 ) 
 *        ex ) SELECT t1.i1
 *             FROM t1, ( SELECT i2 FROM t2 WHERE i1 > 0 limit 3 ) v1
 *             WHERE t1.i1 = v1.i1;
 ************************************************************************/
    
    qmgPreservedOrder * sPreservedOrder;
    qmgPreservedOrder * sFirstPresOrder;
    qmgPreservedOrder * sCurPresOrder;
    qmgPreservedOrder * sCur;
    qmsTarget         * sTarget;
    qtcNode           * sTargetNode;
    idBool              sExistCol;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePreservedOrder::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aChildPreservedOrder != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sFirstPresOrder   = NULL;
    sCurPresOrder     = NULL;
    sExistCol         = ID_FALSE;

    //---------------------------------------------------
    // Child Graph�� Preserved Order�� �̿��� Order����
    //---------------------------------------------------

    for ( sCur = aChildPreservedOrder; sCur != NULL; sCur = sCur->next )
    {
        sExistCol = ID_FALSE;
        for ( sTarget = aTarget; sTarget != NULL; sTarget = sTarget->next )
        {
            // BUG-38193 target�� pass node �� ����ؾ� �մϴ�.
            if ( sTarget->targetColumn->node.module == &qtc::passModule )
            {
                sTargetNode = (qtcNode*)(sTarget->targetColumn->node.arguments);
            }
            else
            {
                sTargetNode = sTarget->targetColumn;
            }

            if ( QTC_IS_COLUMN( aStatement, sTargetNode )
                 == ID_TRUE )
            {
                // target Į���� ������ Į���� ���
                if ( ( sCur->table == sTargetNode->node.table) &&
                     ( sCur->column == sTargetNode->node.column ) )
                {
                    // ���� Į���� ���
                    IDE_TEST(
                        QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder ),
                                                       (void**)&sPreservedOrder )
                        != IDE_SUCCESS );

                    sPreservedOrder->table = sCur->table;
                    sPreservedOrder->column = sCur->column;
                    sPreservedOrder->direction = sCur->direction;
                    sPreservedOrder->next = NULL;

                    if ( sFirstPresOrder == NULL )
                    {
                        sFirstPresOrder = sCurPresOrder = sPreservedOrder;
                    }
                    else
                    {
                        sCurPresOrder->next = sPreservedOrder;
                        sCurPresOrder = sCurPresOrder->next;
                    }

                    sExistCol = ID_TRUE;
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

        if ( sExistCol == ID_FALSE )
        {
            // preserved order ���� �ߴ�
            break;
        }
        else
        {
            // ���� preserved order�� ��� ����
            // nothing to do
        }
    }

    *aMyPreservedOrder = sFirstPresOrder;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::makeChildPlan( qcStatement * aStatement,
                              qmgPROJ     * aGraph )
{

    IDU_FIT_POINT_FATAL( "qmgProjection::makeChildPlan::__FT__" );

    IDE_TEST( aGraph->graph.left->makePlan( aStatement ,
                                            &aGraph->graph,
                                            aGraph->graph.left )
              != IDE_SUCCESS);

    // fix BUG-13482
    // SCAN Limit ����ȭ ���뿡 ���� PROJ plan�� start value ��������
    if( aGraph->graph.left->type == QMG_SELECTION )
    {
        // projection ������ SCAN�̰�,
        // SCAN limit ����ȭ�� ����� ����̸�,
        // PROJ�� limit start value�� 1�� �����Ѵ�.
        if( ((qmgSELT*)(aGraph->graph.left))->limit != NULL )
        {
            qmsLimitI::setStartValue( aGraph->limit, 1 );
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

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_PROJECT;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::makePlan4Proj( qcStatement * aStatement,
                              qmgPROJ     * aGraph,
                              UInt          aPROJFlag )
{
    qmnPlan  * sPROJ        = NULL;
    qmnPlan  * sDLAY        = NULL;
    UInt       sDelayedExec = 0;
    idBool     sFound       = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan4Proj::__FT__" );

    //----------------------------
    // PROJ�� ����
    //----------------------------
    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       NULL,
                                       &sPROJ )
              != IDE_SUCCESS);
    aGraph->graph.myPlan = sPROJ;

    // BUG-43493 delay operator�� �߰��� execution time�� ���δ�.
    if ( ( aPROJFlag & QMO_MAKEPROJ_TOP_MASK ) == QMO_MAKEPROJ_TOP_TRUE )
    {
        // hint�� ����ϴ� ��� ��� materialize������ operator�� ���� delay�� �����Ѵ�.
        if ( aGraph->graph.myQuerySet->SFWGH != NULL )
        {
            switch ( aGraph->graph.myQuerySet->SFWGH->hints->delayedExec )
            {
                case QMS_DELAY_NONE:
                    qcgPlan::registerPlanProperty( aStatement,
                                                   PLAN_PROPERTY_OPTIMIZER_DELAYED_EXECUTION );
                    sDelayedExec = QCU_OPTIMIZER_DELAYED_EXECUTION;
                    break;

                case QMS_DELAY_TRUE:
                    sDelayedExec = 1;
                    break;

                case QMS_DELAY_FALSE:
                    sDelayedExec = 0;
                    break;

                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_OPTIMIZER_DELAYED_EXECUTION );
            sDelayedExec = QCU_OPTIMIZER_DELAYED_EXECUTION;
        }
        
        if ( sDelayedExec == 1 )
        {
            IDE_TEST( qmg::lookUpMaterializeGraph( &aGraph->graph, &sFound )
                      != IDE_SUCCESS );
            
            if ( sFound == ID_TRUE )
            {
                IDE_TEST( qmoOneNonPlan::initDLAY( aStatement ,
                                                   aGraph->graph.myPlan ,
                                                   &sDLAY )
                          != IDE_SUCCESS );
                // �̸� �����ϸ� �Ʒ� makeChildPlan�� parent�� QMN_PROJ�� �ƴϾ
                // ���������� ��������
                //aGraph->graph.myPlan = sDLAY;
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
    
    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( makeChildPlan( aStatement, aGraph ) != IDE_SUCCESS );

    //----------------------------
    // PROJ�� ����
    //----------------------------

    if ( sDLAY != NULL )
    {
        IDE_TEST( qmoOneNonPlan::makeDLAY( aStatement ,
                                           aGraph->graph.myQuerySet ,
                                           aGraph->graph.left->myPlan ,
                                           sDLAY )
                  != IDE_SUCCESS );
        aGraph->graph.left->myPlan = sDLAY;

        qmg::setPlanInfo( aStatement, sDLAY, aGraph->graph.left );
    }
    else
    {
        // Nothing to do.
    }
    
    aPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
    aPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       aGraph->limit ,
                                       aGraph->loopNode ,
                                       aGraph->graph.left->myPlan ,
                                       sPROJ )
              != IDE_SUCCESS);
    aGraph->graph.myPlan = sPROJ;

    qmg::setPlanInfo( aStatement, sPROJ, &(aGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::makePlan4ViewMtr( qcStatement    * aStatement,
                                 qmgPROJ        * aGraph,
                                 UInt             aPROJFlag )
{
/***********************************************************************
 *
 * Description : qmgProjection���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *
 *        - View Materialization �� ����� ���
 *
 *           [VMTR]
 *             |
 *           [VIEW]
 *             |
 *           [PROJ]
 *
 ***********************************************************************/

    qmnPlan         * sPROJ = NULL;
    qmnPlan         * sVIEW = NULL;
    qmnPlan         * sVMTR = NULL;
    qmnPlan         * sCMTR = NULL;
    UInt              sFlag;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan4ViewMtr::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //-----------------------------------------------------
    // - View Optimization Type�� View Materialization�� ���
    //   [VMTR] - [VIEW] - [PROJ]�� ����
    // - View Optimization Type�� ConnectBy Materialization�� ���
    //   [CMTR] - [VIEW] - [PROJ]�� ����
    //-----------------------------------------------------

    if ( ( aGraph->graph.flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK ) ==
         QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE )
    {
        IDE_TEST( qmoOneMtrPlan::initCMTR( aStatement,
                                           aGraph->graph.myQuerySet,
                                           &sCMTR )
                  != IDE_SUCCESS);
        aGraph->graph.myPlan = sCMTR;
    }
    else
    {
        IDE_TEST( qmoOneMtrPlan::initVMTR( aStatement,
                                           aGraph->graph.myQuerySet,
                                           aGraph->graph.myPlan,
                                           &sVMTR )
                  != IDE_SUCCESS );
        aGraph->graph.myPlan = sVMTR;
    }

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aGraph->graph.myQuerySet,
                                       aGraph->graph.myPlan,
                                       &sVIEW )
              != IDE_SUCCESS );
    aGraph->graph.myPlan = sVIEW;

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet,
                                       aGraph->graph.myPlan,
                                       &sPROJ )
              != IDE_SUCCESS );
    aGraph->graph.myPlan = sPROJ;

    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( makeChildPlan( aStatement, aGraph ) != IDE_SUCCESS );

    //----------------------------
    // [PROJ]�� ����
    //----------------------------

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       aGraph->limit ,
                                       aGraph->loopNode ,
                                       aGraph->graph.left->myPlan ,
                                       sPROJ )
              != IDE_SUCCESS);
    aGraph->graph.myPlan = sPROJ;

    qmg::setPlanInfo( aStatement, sPROJ, &(aGraph->graph) );

    //----------------------------
    // [VIEW]�� ����
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
    sFlag |= QMO_MAKEVIEW_FROM_PROJECTION;

    IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       NULL ,
                                       sFlag ,
                                       aGraph->graph.myPlan ,
                                       sVIEW )
              != IDE_SUCCESS);
    aGraph->graph.myPlan = sVIEW;



    //----------------------------
    // [VMTR/CMTR]�� ����
    //----------------------------

    sFlag = 0;

    if( ( aGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKEVMTR_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKEVMTR_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKEVMTR_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKEVMTR_DISK_TEMP_TABLE;
    }

    // BUG-44288 recursive with ���� ������ TEMP_TBS_DISK + TOP QUERY�� ������
    // recursive query name join ��� �Ǵ� ���
    if ( aGraph->graph.myQuerySet->setOp == QMS_UNION_ALL )
    {
        if ( ( ( aGraph->graph.myQuerySet->left->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) &&
             ( ( aGraph->graph.myQuerySet->right->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
               == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
        {
            sFlag &= ~QMO_MAKEVMTR_TEMP_TABLE_MASK;
            sFlag |= QMO_MAKEVMTR_MEMORY_TEMP_TABLE;
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

    if ( ( aGraph->graph.flag & QMG_PROJ_VIEW_OPT_TIP_CMTR_MASK ) ==
         QMG_PROJ_VIEW_OPT_TIP_CMTR_TRUE )
    {
        IDE_TEST( qmoOneMtrPlan::makeCMTR( aStatement,
                                           aGraph->graph.myQuerySet,
                                           sFlag,
                                           aGraph->graph.myPlan,
                                           sCMTR )
                  != IDE_SUCCESS);
        aGraph->graph.myPlan = sCMTR;

        qmg::setPlanInfo( aStatement, sCMTR, &(aGraph->graph) );
    }
    else
    {
        IDE_DASSERT( ( aGraph->graph.flag & QMG_PROJ_VIEW_OPT_TIP_VMTR_MASK ) ==
                     QMG_PROJ_VIEW_OPT_TIP_VMTR_TRUE );

        if ( ( aGraph->graph.flag & QMG_PROJ_TOP_RESULT_CACHE_MASK )
             == QMG_PROJ_TOP_RESULT_CACHE_TRUE )
        {
            sFlag &= ~QMO_MAKEVMTR_TOP_RESULT_CACHE_MASK;
            sFlag |= QMO_MAKEVMTR_TOP_RESULT_CACHE_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
        IDE_TEST( qmoOneMtrPlan::makeVMTR( aStatement ,
                                           aGraph->graph.myQuerySet ,
                                           sFlag ,
                                           aGraph->graph.myPlan,
                                           sVMTR )
                  != IDE_SUCCESS);
        aGraph->graph.myPlan = sVMTR;

        
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgProjection::makePlan4StoreSearch( qcStatement    * aStatement,
                                     qmgPROJ        * aGraph,
                                     UInt             aPROJFlag )
{
/***********************************************************************
 *
 * Description : qmgProjection���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *
 *        - Store And Search ����ȭ�� ����� ���
 *
 *           1.  Hash ���� ����� ���
 *
 *
 *               [PROJ]
 *                 |
 *               [HASH] : Filter�� ���Ե�, NOTNULLCHECK option ����.
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *           2.  LMST ���� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [LMST] : Filter�� �������� ����
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 *           3.  STORE Only�� ���
 *
 *               [PROJ]
 *                 |
 *               [SORT]
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 ***********************************************************************/

    qmnPlan         * sVIEW;
    qmnPlan         * sInnerPROJ;
    qmnPlan         * sOuterPROJ;

    qmnPlan         * sPlan = NULL;
    UInt              sFlag;
    qtcNode         * sHashFilter;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan4StoreSearch::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //----------------------------
    // Top-down ����
    //----------------------------

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet,
                                       NULL,
                                       &sOuterPROJ )
              != IDE_SUCCESS );
    aGraph->graph.myPlan = sOuterPROJ;

    switch( aGraph->subqueryTipFlag & QMG_PROJ_SUBQUERY_STORENSEARCH_MASK )
    {
        case QMG_PROJ_SUBQUERY_STORENSEARCH_HASH:
            IDE_TEST( qmoOneMtrPlan::initHASH( aStatement,
                                               aGraph->graph.myQuerySet ,
                                               aGraph->graph.myPlan,
                                               &sPlan ) != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;

            break;

        case QMG_PROJ_SUBQUERY_STORENSEARCH_LMST:
            IDE_TEST( qmoOneMtrPlan::initLMST( aStatement ,
                                               aGraph->graph.myQuerySet ,
                                               aGraph->graph.myPlan ,
                                               &sPlan ) != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;

            break;

        case QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY:
            IDE_TEST( qmoOneMtrPlan::initSORT( aStatement ,
                                               aGraph->graph.myQuerySet ,
                                               aGraph->graph.myPlan ,
                                               &sPlan ) != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;

            break;

        default:
            break;
    }

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement,
                                       aGraph->graph.myQuerySet,
                                       aGraph->graph.myPlan,
                                       &sVIEW )
              != IDE_SUCCESS );
    aGraph->graph.myPlan = sVIEW;

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet,
                                       aGraph->graph.myPlan,
                                       &sInnerPROJ )
              != IDE_SUCCESS );
    aGraph->graph.myPlan = sInnerPROJ;

    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( makeChildPlan( aStatement, aGraph ) != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //----------------------------
    // [PROJ]�� ����
    // limit�� ����Ѵ�.
    //----------------------------

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag,
                                       aGraph->limit,
                                       aGraph->loopNode ,
                                       aGraph->graph.left->myPlan,
                                       sInnerPROJ ) != IDE_SUCCESS);

    aGraph->graph.myPlan = sInnerPROJ;

    qmg::setPlanInfo( aStatement, sInnerPROJ, &(aGraph->graph) );

    //----------------------------
    // [VIEW]�� ����
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
    sFlag |= QMO_MAKEVIEW_FROM_PROJECTION;

    IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       NULL ,
                                       sFlag ,
                                       aGraph->graph.myPlan ,
                                       sVIEW ) != IDE_SUCCESS);

    aGraph->graph.myPlan = sVIEW;



    //-----------------------------------------------------
    // ���� ����� ����
    //-----------------------------------------------------
    switch( aGraph->subqueryTipFlag & QMG_PROJ_SUBQUERY_STORENSEARCH_MASK )
    {
        case QMG_PROJ_SUBQUERY_STORENSEARCH_HASH:

            //-----------------------------
            // Hash ���� ����� ���
            //-----------------------------

            if( aGraph->storeNSearchPred != NULL )
            {
                // To Fix PR-8019
                // Key Range ������ ���ؼ��� DNF���·� ��ȯ�Ͽ��� �Ѵ�.
                IDE_TEST(
                    qmoNormalForm::normalizeDNF( aStatement ,
                                                 aGraph->storeNSearchPred ,
                                                 & sHashFilter ) != IDE_SUCCESS );

                IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                                   aGraph->graph.myQuerySet ,
                                                   sHashFilter ) != IDE_SUCCESS );
            }
            else
            {
                sHashFilter = NULL;
            }

            //----------------------------
            // [HASH]�� ����
            // Temp Table�� ������ 1��
            //----------------------------

            sFlag = 0;
            sFlag &= ~QMO_MAKEHASH_METHOD_MASK;
            sFlag |= QMO_MAKEHASH_STORE_AND_SEARCH;

            //----------------------------
            // Temp Table�� ���� ���� ����
            // - Graph�� Hint�� ���� Table Type (memory or disk) �� ���� �ִ�.
            //   Plan ���� Table Type������ �����ϰ� ������,
            //   �̴� ���� ������ OR�Ͽ� ������ �ȴ�.
            //   (disk�� �켱����) ����, �������� Graph��
            //   ������ ǥ�����ֵ��� �Ѵ�.
            //----------------------------

            if( ( aGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
                QMG_GRAPH_TYPE_MEMORY )
            {
                sFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
                sFlag |= QMO_MAKEHASH_MEMORY_TEMP_TABLE;
            }
            else
            {
                sFlag &= ~QMO_MAKEHASH_TEMP_TABLE_MASK;
                sFlag |= QMO_MAKEHASH_DISK_TEMP_TABLE;
            }

            // Not null check�� ���� flag
            // fix BUG-8936
            if ( ( aGraph->subqueryTipFlag &
                   QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_MASK) ==
                 QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_TRUE )
            {
                sFlag &= ~QMO_MAKEHASH_NOTNULLCHECK_MASK;
                sFlag |= QMO_MAKEHASH_NOTNULLCHECK_TRUE;
            }
            else
            {
                sFlag &= ~QMO_MAKEHASH_NOTNULLCHECK_MASK;
                sFlag |= QMO_MAKEHASH_NOTNULLCHECK_FALSE;
            }

            IDE_TEST( qmoOneMtrPlan::makeHASH( aStatement,
                                               aGraph->graph.myQuerySet ,
                                               sFlag ,
                                               1 ,
                                               aGraph->hashBucketCnt ,
                                               sHashFilter ,
                                               aGraph->graph.myPlan ,
                                               sPlan,
                                               ID_FALSE ) // aAllAttrToKey
                      != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;



            break;

        case QMG_PROJ_SUBQUERY_STORENSEARCH_LMST:
            //-----------------------------
            // LMST ���� ����� ���
            //-----------------------------

            //----------------------------
            // [LMST]�� ����
            // limiRowCount�� 2�� �ȴ�
            //----------------------------
            sFlag = 0;
            sFlag &= ~QMO_MAKELMST_METHOD_MASK;
            sFlag |= QMO_MAKELMST_STORE_AND_SEARCH;

            IDE_TEST( qmoOneMtrPlan::makeLMST( aStatement ,
                                               aGraph->graph.myQuerySet ,
                                               (ULong)2 ,
                                               aGraph->graph.myPlan ,
                                               sPlan ) != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;



            break;

        case QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY:

            //-----------------------------
            // Store Only ���� ����� ���
            // SORT����� ����
            //-----------------------------

            //----------------------------
            // [SORT]�� ����
            //----------------------------

            sFlag = 0;
            sFlag &= ~QMO_MAKESORT_METHOD_MASK;
            sFlag |= QMO_MAKESORT_STORE_AND_SEARCH;

            //Temp Table�� ���� ���� ����
            if( ( aGraph->graph.flag & QMG_GRAPH_TYPE_MASK ) ==
                QMG_GRAPH_TYPE_MEMORY )
            {
                sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
                sFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
            }
            else
            {
                sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
                sFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
            }

            IDE_TEST( qmoOneMtrPlan::makeSORT( aStatement ,
                                               aGraph->graph.myQuerySet ,
                                               sFlag ,
                                               NULL ,
                                               aGraph->graph.myPlan ,
                                               aGraph->graph.costInfo.inputRecordCnt,
                                               sPlan ) != IDE_SUCCESS);
            aGraph->graph.myPlan = sPlan;

            qmg::setPlanInfo( aStatement, sPlan, &(aGraph->graph) );

            break;

        default:
            IDE_DASSERT(0);
            break;
    }

    //----------------------------
    // [PROJ]�� ����
    //----------------------------

    aPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
    aPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       NULL ,
                                       NULL ,
                                       aGraph->graph.myPlan ,
                                       sOuterPROJ ) != IDE_SUCCESS);

    aGraph->graph.myPlan = sOuterPROJ;

    qmg::setPlanInfo( aStatement, sOuterPROJ, &(aGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::makePlan4InKeyRange( qcStatement * aStatement,
                                    qmgPROJ     * aGraph,
                                    UInt          aPROJFlag )
{
/***********************************************************************
 *
 * Description : qmgProjection���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *
 *        - SUBQUERY IN KEYRANGE ����ȭ�� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [HSDS]
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 ***********************************************************************/

    qmnPlan         * sInnerPROJ;
    qmnPlan         * sOuterPROJ;
    qmnPlan         * sVIEW;
    qmnPlan         * sHSDS;
    UInt              sFlag;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan4InKeyRange::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //----------------------------
    // Top-down ����
    //----------------------------

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       NULL,
                                       &sOuterPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sOuterPROJ;

    IDE_TEST( qmoOneMtrPlan::initHSDS( aStatement ,
                                       aGraph->graph.myQuerySet,
                                       ID_FALSE,
                                       aGraph->graph.myPlan,
                                       & sHSDS ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sHSDS;

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aGraph->graph.myPlan,
                                       &sVIEW ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sVIEW;

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       aGraph->graph.myPlan,
                                       &sInnerPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sInnerPROJ;

    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( makeChildPlan( aStatement, aGraph ) != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------------------------------------
    // Subquery IN Keyrange ����ȭ�� ����� ���
    // [PROJ] - [HSDS] - [VIEW] - [PROJ]
    //-----------------------------------------------------

    //----------------------------
    // [PROJ]�� ����
    //----------------------------

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       aGraph->limit ,
                                       aGraph->loopNode ,
                                       aGraph->graph.left->myPlan ,
                                       sInnerPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sInnerPROJ;

    qmg::setPlanInfo( aStatement, sInnerPROJ, &(aGraph->graph) );

    //----------------------------
    // [VIEW]�� ����
    //----------------------------
    sFlag = 0;
    sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
    sFlag |= QMO_MAKEVIEW_FROM_PROJECTION;

    IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       NULL ,
                                       sFlag ,
                                       aGraph->graph.myPlan ,
                                       sVIEW ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aGraph->graph) );

    //----------------------------
    // [HSDS]�� ����
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEHSDS_METHOD_MASK;
    sFlag |= QMO_MAKEHSDS_IN_SUBQUERY_KEYRANGE;

    //Temp Table�� ���� ���� ����
    if( (aGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
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
                                       aGraph->graph.myQuerySet,
                                       sFlag ,
                                       aGraph->hashBucketCnt ,
                                       aGraph->graph.myPlan ,
                                       sHSDS ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sHSDS;

    qmg::setPlanInfo( aStatement, sHSDS, &(aGraph->graph) );

    //----------------------------
    // [PROJ]�� ����
    //----------------------------

    aPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
    aPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       NULL ,
                                       NULL ,
                                       aGraph->graph.myPlan,
                                       sOuterPROJ ) != IDE_SUCCESS);

    aGraph->graph.myPlan = sOuterPROJ;

    qmg::setPlanInfo( aStatement, sOuterPROJ, &(aGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgProjection::makePlan4KeyRange( qcStatement * aStatement,
                                  qmgPROJ     * aGraph,
                                  UInt          aPROJFlag )
{
/***********************************************************************
 *
 * Description : qmgProjection���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *
 *        - SUBQUERY KEYRANGE ����ȭ�� ����� ���
 *
 *               [PROJ]
 *                 |
 *               [SORT] : ���常 ��.
 *                 |
 *               [VIEW]
 *                 |
 *               [PROJ]
 *
 ***********************************************************************/

    qmnPlan         * sInnerPROJ;
    qmnPlan         * sOuterPROJ;
    qmnPlan         * sVIEW;
    qmnPlan         * sSORT;
    UInt              sFlag;

    IDU_FIT_POINT_FATAL( "qmgProjection::makePlan4KeyRange::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //----------------------------
    // Top-down ����
    //----------------------------

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       NULL,
                                       &sOuterPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sOuterPROJ;

    IDE_TEST( qmoOneMtrPlan::initSORT( aStatement ,
                                       aGraph->graph.myQuerySet,
                                       sOuterPROJ,
                                       & sSORT ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sSORT;

    IDE_TEST( qmoOneNonPlan::initVIEW( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       sSORT,
                                       &sVIEW ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sVIEW;

    IDE_TEST( qmoOneNonPlan::initPROJ( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       aGraph->graph.myPlan,
                                       &sInnerPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sInnerPROJ;

    //---------------------------
    // ���� Plan�� ����
    //---------------------------

    IDE_TEST( makeChildPlan( aStatement, aGraph ) != IDE_SUCCESS );

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------------------------------------
    // Subquery Keyrange ����ȭ�� ����� ���
    // [PROJ] - [SORT] - [VIEW] - [PROJ]
    //-----------------------------------------------------

    //----------------------------
    // [PROJ]�� ����
    //----------------------------
    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement,
                                       aGraph->graph.myQuerySet,
                                       aPROJFlag,
                                       aGraph->limit ,
                                       aGraph->loopNode ,
                                       aGraph->graph.left->myPlan,
                                       sInnerPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sInnerPROJ;

    qmg::setPlanInfo( aStatement, sInnerPROJ, &(aGraph->graph) );

    //----------------------------
    // [VIEW]�� ����
    //----------------------------
    sFlag = 0;
    sFlag &= ~QMO_MAKEVIEW_FROM_MASK;
    sFlag |= QMO_MAKEVIEW_FROM_PROJECTION;

    IDE_TEST( qmoOneNonPlan::makeVIEW( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       NULL ,
                                       sFlag ,
                                       aGraph->graph.myPlan ,
                                       sVIEW ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sVIEW;

    qmg::setPlanInfo( aStatement, sVIEW, &(aGraph->graph) );

    //----------------------------
    // [SORT]�� ����
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKESORT_METHOD_MASK;
    sFlag |= QMO_MAKESORT_STORE_AND_SEARCH;

    //Temp Table�� ���� ���� ����
    if( ( aGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
        QMG_GRAPH_TYPE_MEMORY )
    {
        sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESORT_MEMORY_TEMP_TABLE;
    }
    else
    {
        sFlag &= ~QMO_MAKESORT_TEMP_TABLE_MASK;
        sFlag |= QMO_MAKESORT_DISK_TEMP_TABLE;
    }

    IDE_TEST( qmoOneMtrPlan::makeSORT( aStatement,
                                       aGraph->graph.myQuerySet ,
                                       sFlag ,
                                       NULL ,
                                       aGraph->graph.myPlan ,
                                       aGraph->graph.costInfo.inputRecordCnt,
                                       sSORT ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sSORT;

    qmg::setPlanInfo( aStatement, sSORT, &(aGraph->graph) );

    //----------------------------
    // [PROJ]�� ����
    //----------------------------

    aPROJFlag &= ~QMO_MAKEPROJ_QUERYSET_TOP_MASK;
    aPROJFlag |= QMO_MAKEPROJ_QUERYSET_TOP_TRUE;

    IDE_TEST( qmoOneNonPlan::makePROJ( aStatement ,
                                       aGraph->graph.myQuerySet ,
                                       aPROJFlag ,
                                       NULL ,
                                       NULL ,
                                       aGraph->graph.myPlan ,
                                       sOuterPROJ ) != IDE_SUCCESS);
    aGraph->graph.myPlan = sOuterPROJ;

    qmg::setPlanInfo( aStatement, sOuterPROJ, &(aGraph->graph) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgProjection::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Projection graph�� child graph�� Preserved order�� �״�� ������.
 *                direction�� child graph�� ���� �����Ѵ�.
 *
 *  Implementation :
 *     Child graph�� Preserved order direction�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool sIsSamePrevOrderWithChild;

    IDU_FIT_POINT_FATAL( "qmgProjection::finalizePreservedOrder::__FT__" );

    // BUG-36803 Projection Graph must have a left child graph
    IDE_DASSERT( aGraph->left != NULL );

    sIsSamePrevOrderWithChild =
        qmg::isSamePreservedOrder( aGraph->preservedOrder,
                                   aGraph->left->preservedOrder );

    if ( sIsSamePrevOrderWithChild == ID_TRUE )
    {
        // Child graph�� Preserved order direction�� �����Ѵ�.
        IDE_TEST( qmg::copyPreservedOrderDirection(
                      aGraph->preservedOrder,
                      aGraph->left->preservedOrder )
                  != IDE_SUCCESS );    
    }
    else
    {
        // Projection�� preserved order�� ������ �� ���� graph�̹Ƿ�
        // ������ preserved order�� �ٸ��� ����.
        IDE_DASSERT(0);
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// BUG-48779
void qmgProjection::canPushDownLimitIntoScan( qmgGraph * aGraph,
                                              idBool   * aIsScanLimit )
{
    qtcNode * sNode = NULL;

    *aIsScanLimit = ID_FALSE;

    if ( aGraph->type == QMG_SELECTION )
    {
        //---------------------------------------------------
        // ������ �Ϲ� qmgSelection�� ���
        // ��, Set, Order By, Group By, Aggregation, Distinct, Join��
        //  ���� ���
        //---------------------------------------------------
        if ( aGraph->myFrom->tableRef->view == NULL )
        {
            // View �� �ƴ� ���, SCAN Limit ����
            sNode = (qtcNode *)aGraph->myQuerySet->SFWGH->where;

            if ( sNode != NULL )
            {
                // where�� �����ϴ� ���, subquery ���� ���� �˻�
                if ( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK )
                     != QTC_NODE_SUBQUERY_EXIST )
                {
                    *aIsScanLimit = ID_TRUE;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // where�� �������� �ʴ� ���,SCAN Limit ����
                *aIsScanLimit = ID_TRUE;
            }
        }
        else
        {
            // nothing to do
        }
    }
}


