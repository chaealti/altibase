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
 * $Id: qmgDistinction.cpp 90409 2021-04-01 04:19:46Z donovan.seo $
 *
 * Description :
 *     Distinction Graph�� ���� ���� �Լ�
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgDistinction.h>
#include <qmoCost.h>
#include <qmoOneNonPlan.h>
#include <qmoOneMtrPlan.h>
#include <qmoSelectivity.h>
#include <qcgPlan.h>

extern mtfModule mtfDecrypt;

IDE_RC
qmgDistinction::init( qcStatement * aStatement,
                      qmsQuerySet * aQuerySet,
                      qmgGraph    * aChildGraph,
                      qmgGraph   ** aGraph )
{
/***********************************************************************
 *
 * Description : qmgDistinction�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmgDistinction�� ���� ���� �Ҵ�
 *    (2) graph( ��� Graph�� ���� ���� �ڷ� ���� ) �ʱ�ȭ
 *    (3) out ����
 *
 ***********************************************************************/

    qmgDIST * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgDistinction::init::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aChildGraph != NULL );

    //---------------------------------------------------
    // Distinction Graph�� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    // qmgDistinction�� ���� ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgDIST ),
                                             (void**) &sMyGraph )
              != IDE_SUCCESS );

    // Graph ���� ������ �ʱ�ȭ
    IDE_TEST( qmg::initGraph( & sMyGraph->graph ) != IDE_SUCCESS );

    sMyGraph->graph.type = QMG_DISTINCTION;
    sMyGraph->graph.left = aChildGraph;
    qtc::dependencySetWithDep( & sMyGraph->graph.depInfo,
                               & aChildGraph->depInfo );

    sMyGraph->graph.myFrom = NULL;
    sMyGraph->graph.myQuerySet = aQuerySet;

    sMyGraph->graph.optimize = qmgDistinction::optimize;
    sMyGraph->graph.makePlan = qmgDistinction::makePlan;
    sMyGraph->graph.printGraph = qmgDistinction::printGraph;

    // Disk/Memory ���� ����
    switch(  aQuerySet->SFWGH->hints->interResultType )
    {
        case QMO_INTER_RESULT_TYPE_NOT_DEFINED :
            // �߰� ��� Type Hint�� ���� ���, ������ Type�� ������.
            sMyGraph->graph.flag &= ~QMG_GRAPH_TYPE_MASK;
            sMyGraph->graph.flag |=
                ( aChildGraph->flag & QMG_GRAPH_TYPE_MASK );
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
    // Distinction Graph ���� ���� �⺻ �ʱ�ȭ
    //---------------------------------------------------

    sMyGraph->hashBucketCnt = 0;

    // out ����
    *aGraph = (qmgGraph*)sMyGraph;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDistinction::optimize( qcStatement * aStatement, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgDistinction�� ����ȭ
 *
 * Implementation :
 *    (1) indexable Distinct ����ȭ
 *    (2) distinction Method ���� �� Preserved order flag ����
 *    (3) hash based distinction���� ������ ���, hashBucketCnt ����
 *    (4) ���� ��� ���� ����
 *
 ***********************************************************************/

    qmgDIST             * sMyGraph;
    qmsTarget           * sTarget;
    qtcNode             * sNode;
    mtcColumn           * sMtcColumn;
    idBool                sSuccess;
    qmoDistinctMethodType sDistinctMethodHint;

    SDouble               sRecordSize;

    SDouble               sTotalCost;
    SDouble               sDiskCost;
    SDouble               sAccessCost;
                          
    SDouble               sSelAccessCost;
    SDouble               sSelDiskCost;
    SDouble               sSelTotalCost;
                          
    idBool                sIsDisk;
    UInt                  sSelectedMethod = 0;

    IDU_FIT_POINT_FATAL( "qmgDistinction::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sMyGraph = (qmgDIST*) aGraph;
    sSuccess = ID_FALSE;
    sRecordSize    = 0;
    sSelAccessCost = 0;
    sSelDiskCost   = 0;
    sSelTotalCost  = 0;

    //------------------------------------------
    // Record Size ���
    //------------------------------------------

    for ( sTarget = aGraph->myQuerySet->SFWGH->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sNode = sTarget->targetColumn;

        sMtcColumn   = QTC_TMPL_COLUMN(QC_SHARED_TMPLATE(aStatement), sNode);
        sRecordSize += sMtcColumn->column.size;
    }
    // BUG-36463 sRecordSize �� 0�� �Ǿ�� �ȵȴ�.
    sRecordSize = IDL_MAX( sRecordSize, 1 );

    //------------------------------------------
    // Distinction Method ����
    //------------------------------------------
    IDE_TEST( qmg::isDiskTempTable( aGraph, & sIsDisk ) != IDE_SUCCESS );

    /* TASK-6744 */
    if ( (QCU_PLAN_RANDOM_SEED != 0) &&
         (aStatement->mRandomPlanInfo != NULL) &&
         (smiGetStartupPhase() == SMI_STARTUP_SERVICE) )
    {
        aStatement->mRandomPlanInfo->mTotalNumOfCases = aStatement->mRandomPlanInfo->mTotalNumOfCases + 2;
        sSelectedMethod = QCU_PLAN_RANDOM_SEED % (aStatement->mRandomPlanInfo->mTotalNumOfCases - aStatement->mRandomPlanInfo->mWeightedValue);

        if ( sSelectedMethod >= 2 )
        {
            sSelectedMethod = sSelectedMethod % 2;
        }
        else
        {
            // Nothing to do.
        }

        IDE_DASSERT( sSelectedMethod < 2 );

        switch ( sSelectedMethod )
        {
            case 0 :
                sDistinctMethodHint = QMO_DISTINCT_METHOD_TYPE_SORT;
                break;
            case 1 :
                sDistinctMethodHint = QMO_DISTINCT_METHOD_TYPE_HASH;
                break;
            default :
                IDE_DASSERT( 0 );
                break;
        }

        aStatement->mRandomPlanInfo->mWeightedValue++;

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED );
    }
    else
    {
        sDistinctMethodHint =
            sMyGraph->graph.myQuerySet->SFWGH->hints->distinctMethodType;
    }

    switch( sDistinctMethodHint )
    {
        case QMO_DISTINCT_METHOD_TYPE_NOT_DEFINED :

            // To Fix PR-12394
            // DISTINCTION ���� ��Ʈ�� ���� ��쿡��
            // ����ȭ Tip�� �����Ѵ�.

            //------------------------------------------
            // To Fix PR-12396
            // ��� ����� ���� ���� ����� �����Ѵ�.
            //------------------------------------------

            //------------------------------------------
            // Sorting �� �̿��� ����� ��� ���
            //------------------------------------------
            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                             &(aGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                              &(aGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              sRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }

            sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
            sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_SORT;

            //------------------------------------------
            // Hashing �� �̿��� ����� �����
            //------------------------------------------
            if( sIsDisk == ID_FALSE )
            {
                sTotalCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                          &(aGraph->left->costInfo),
                                                          QMO_COST_DEFAULT_NODE_CNT );
                sAccessCost = sTotalCost;
                sDiskCost   = 0;
            }
            else
            {
                sTotalCost = qmoCost::getDiskHashTempCost( aStatement->mSysStat,
                                                           &(aGraph->left->costInfo),
                                                           QMO_COST_DEFAULT_NODE_CNT,
                                                           sRecordSize );
                sAccessCost = 0;
                sDiskCost   = sTotalCost;
            }

            if (QMO_COST_IS_EQUAL(sTotalCost, QMO_COST_INVALID_COST) == ID_TRUE)
            {
                // Hashing ����� ������ �� ���� �����
            }
            else
            {
                if (QMO_COST_IS_GREATER(sTotalCost, sSelTotalCost) == ID_TRUE)
                {
                    // Nothing to do
                }
                else
                {
                    // Hashing ����� ���� ����
                    sSelTotalCost  = sTotalCost;
                    sSelDiskCost   = sDiskCost;
                    sSelAccessCost = sAccessCost;

                    sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
                    sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_HASH;

                    sMyGraph->graph.preservedOrder = NULL;

                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;
                }
            }

            //------------------------------------------
            // Preserved Order �� �̿��� ����� �����
            //------------------------------------------

            IDE_TEST( getCostByPrevOrder( aStatement,
                                          sMyGraph,
                                          & sAccessCost,
                                          & sDiskCost,
                                          & sTotalCost )
                      != IDE_SUCCESS );

            if (QMO_COST_IS_EQUAL(sTotalCost, QMO_COST_INVALID_COST) == ID_TRUE)
            {
                // Preserved Order ����� ������ �� ���� �����
            }
            else
            {
                if (QMO_COST_IS_GREATER(sTotalCost, sSelTotalCost) == ID_TRUE)
                {
                    // Nothing to do
                }
                else
                {
                    // Preserved Order ����� ���� ����
                    sSelTotalCost  = sTotalCost;
                    sSelDiskCost   = sDiskCost;
                    sSelAccessCost = sAccessCost;

                    //------------------------------------------
                    // Indexable Distinct ����ȭ ����
                    //------------------------------------------

                    IDE_TEST( indexableDistinct( aStatement,
                                                 aGraph,
                                                 aGraph->myQuerySet->SFWGH->target,
                                                 & sSuccess )
                              != IDE_SUCCESS );

                    IDE_DASSERT( sSuccess == ID_TRUE );

                    sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
                    sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_SORT;

                    // Preserved Order�� �̹� ������
                    sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                    sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;

                    sMyGraph->graph.flag &= ~QMG_DIST_OPT_TIP_MASK;
                    sMyGraph->graph.flag |= QMG_DIST_OPT_TIP_INDEXABLE_DISINCT;
                }
            }

            //------------------------------------------
            // ������
            //------------------------------------------

            // Sorting ����� ���õ� ��� Preserved Order ����
            if ( ( ( sMyGraph->graph.flag & QMG_SORT_HASH_METHOD_MASK )
                   == QMG_SORT_HASH_METHOD_SORT )
                 &&
                 ( ( sMyGraph->graph.flag & QMG_DIST_OPT_TIP_MASK )
                   != QMG_DIST_OPT_TIP_INDEXABLE_DISINCT ) )
            {
                sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
                sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_SORT;

                // Sort-based Distinction�� ��� Preserved Order�� �����.
                IDE_TEST(
                    makeTargetOrder( aStatement,
                                     sMyGraph->graph.myQuerySet->SFWGH->target,
                                     & sMyGraph->graph.preservedOrder )
                    != IDE_SUCCESS );

                sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
                sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            }
            else
            {
                // �ٸ� Method�� ���õ�
            }
            break;

        case QMO_DISTINCT_METHOD_TYPE_HASH :

            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemHashTempCost( aStatement->mSysStat,
                                                             &(aGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskHashTempCost( aStatement->mSysStat,
                                                              &(aGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              sRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }


            sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
            sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_HASH;

            sMyGraph->graph.preservedOrder = NULL;

            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_NEVER;

            break;
        case QMO_DISTINCT_METHOD_TYPE_SORT :

            if( sIsDisk == ID_FALSE )
            {
                sSelTotalCost = qmoCost::getMemSortTempCost( aStatement->mSysStat,
                                                             &(aGraph->left->costInfo),
                                                             QMO_COST_DEFAULT_NODE_CNT );
                sSelAccessCost = sSelTotalCost;
                sSelDiskCost   = 0;
            }
            else
            {
                sSelTotalCost = qmoCost::getDiskSortTempCost( aStatement->mSysStat,
                                                              &(aGraph->left->costInfo),
                                                              QMO_COST_DEFAULT_NODE_CNT,
                                                              sRecordSize );
                sSelAccessCost = 0;
                sSelDiskCost   = sSelTotalCost;
            }

            sMyGraph->graph.flag &= ~QMG_SORT_HASH_METHOD_MASK;
            sMyGraph->graph.flag |= QMG_SORT_HASH_METHOD_SORT;

            // Sort-based Distinction�� ��� Preserved Order�� �����.
            IDE_TEST(
                makeTargetOrder( aStatement,
                                 sMyGraph->graph.myQuerySet->SFWGH->target,
                                 & sMyGraph->graph.preservedOrder )
                != IDE_SUCCESS );


            sMyGraph->graph.flag &= ~QMG_PRESERVED_ORDER_MASK;
            sMyGraph->graph.flag |= QMG_PRESERVED_ORDER_DEFINED_FIXED;
            break;
        default :
            IDE_DASSERT( 0 );
            break;
    }

    //------------------------------------------
    // Hash Bucket Count�� ����
    //------------------------------------------

    IDE_TEST(
        qmg::getBucketCntWithTarget(
            aStatement,
            & sMyGraph->graph,
            sMyGraph->graph.myQuerySet->SFWGH->target,
            sMyGraph->graph.myQuerySet->SFWGH->hints->hashBucketCnt,
            & sMyGraph->hashBucketCnt )
        != IDE_SUCCESS );

    if ( ( sMyGraph->graph.flag & QMG_SORT_HASH_METHOD_MASK ) ==
         QMG_SORT_HASH_METHOD_HASH )
    {
        /* BUG-48792 Distinct Cube wrong result */
        if ( ( ( aStatement->mFlag & QC_STMT_VIEW_MASK )
               == QC_STMT_VIEW_TRUE ) &&
             ( sIsDisk == ID_FALSE ) )
        {
            /* PROJ-1353 Rollup, Cube�� ���� ���� �� */
            if ( ( sMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
                 == QMG_GROUPBY_EXTENSION_TRUE )
            {
                /* Row�� Value�� �ױ⸦ Rollup, Cube�� �����Ѵ�. */
                sMyGraph->graph.left->flag &= ~QMG_VALUE_TEMP_MASK;
                sMyGraph->graph.left->flag |= QMG_VALUE_TEMP_TRUE;
            }
        }
    }
    else
    {
        sMyGraph->hashBucketCnt = 0;

        /* PROJ-1353 Rollup, Cube�� ���� ���� �� */
        if ( ( sMyGraph->graph.left->flag & QMG_GROUPBY_EXTENSION_MASK )
             == QMG_GROUPBY_EXTENSION_TRUE )
        {
            /* Row�� Value�� �ױ⸦ Rollup, Cube�� �����Ѵ�. */
            sMyGraph->graph.left->flag &= ~QMG_VALUE_TEMP_MASK;
            sMyGraph->graph.left->flag |= QMG_VALUE_TEMP_TRUE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    //------------------------------------------
    // ���� ��� ������ ����
    //------------------------------------------

    // recordSize = group by column size + aggregation column size
    sMyGraph->graph.costInfo.recordSize = sRecordSize;

    // selectivity
    sMyGraph->graph.costInfo.selectivity = 1;

    // inputRecordCnt
    sMyGraph->graph.costInfo.inputRecordCnt =
        sMyGraph->graph.left->costInfo.outputRecordCnt;

    // To Fix BUG-8355
    // outputRecordCnt
    IDE_TEST( qmoSelectivity::setDistinctionOutputCnt(
                     aStatement,
                     sMyGraph->graph.myQuerySet->SFWGH->target,
                     sMyGraph->graph.costInfo.inputRecordCnt,
                     & sMyGraph->graph.costInfo.outputRecordCnt )
                 != IDE_SUCCESS );

    //----------------------------------
    // �ش� Graph�� ��� ���� ����
    //----------------------------------
    sMyGraph->graph.costInfo.myAccessCost = sSelAccessCost;
    sMyGraph->graph.costInfo.myDiskCost   = sSelDiskCost;
    sMyGraph->graph.costInfo.myAllCost    = sSelTotalCost;

    // totalCost
    sMyGraph->graph.costInfo.totalAccessCost =
        sMyGraph->graph.left->costInfo.totalAccessCost +
        sMyGraph->graph.costInfo.myAccessCost;
    sMyGraph->graph.costInfo.totalDiskCost =
        sMyGraph->graph.left->costInfo.totalDiskCost +
        sMyGraph->graph.costInfo.myDiskCost;
    sMyGraph->graph.costInfo.totalAllCost =
        sMyGraph->graph.left->costInfo.totalAllCost +
        sMyGraph->graph.costInfo.myAllCost;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmgDistinction::makePlan( qcStatement * aStatement, const qmgGraph * aParent, qmgGraph * aGraph )
{
/***********************************************************************
 *
 * Description : qmgDistinction���� ���� Plan�� �����Ѵ�.
 *
 * Implementation :
 *     - qmgDistinction�� ���� ���� ������ Plan
 *
 *         - Sort-based ó��
 *
 *             [GRBY] : distinct option ���
 *               |
 *           ( [SORT] ) : Indexable Distinct�� ��� �������� ����.
 *
 *         - Hash-based ó��
 *
 *             [HSDS]
 *
 ***********************************************************************/

    qmgDIST         * sMyGraph;

    IDU_FIT_POINT_FATAL( "qmgDistinction::makePlan::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    sMyGraph = (qmgDIST*) aGraph;

    /* PROJ-1071 Parallel Query */
    aGraph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
    aGraph->flag |= (aParent->flag & QMG_PARALLEL_IMPOSSIBLE_MASK);

    // BUG-38410
    // SCAN parallel flag �� �ڽ� ���� �����ش�.
    aGraph->left->flag  |= (aGraph->flag & QMG_PLAN_EXEC_REPEATED_MASK);

    sMyGraph->graph.myPlan = aParent->myPlan;

    switch( sMyGraph->graph.flag & QMG_SORT_HASH_METHOD_MASK )
    {
        case QMG_SORT_HASH_METHOD_SORT:
        {
            IDE_TEST( makeSortDistinction( aStatement,
                                           sMyGraph )
                      != IDE_SUCCESS );
            break;
        }

        case QMG_SORT_HASH_METHOD_HASH:
        {
            IDE_TEST( makeHashDistinction( aStatement,
                                           sMyGraph )
                      != IDE_SUCCESS );
            break;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDistinction::makeSortDistinction( qcStatement * aStatement,
                                     qmgDIST     * aMyGraph )
{
    UInt               sFlag;
    qmnPlan          * sSORT = NULL;
    qmnPlan          * sGRBY = NULL;

    IDU_FIT_POINT_FATAL( "qmgDistinction::makeSortDistinction::__FT__" );

    //-----------------------------------------------------
    //        [GRBY]
    //          |
    //        [SORT]
    //          |
    //        [LEFT]
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init GRBY
    //-----------------------

    IDE_TEST( qmoOneNonPlan::initGRBY( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       aMyGraph->graph.myPlan ,
                                       &sGRBY )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRBY;

    //-----------------------
    // init SORT
    //-----------------------

    if( (aMyGraph->graph.flag & QMG_DIST_OPT_TIP_MASK) !=
        QMG_DIST_OPT_TIP_INDEXABLE_DISINCT )
    {
        IDE_TEST( qmoOneMtrPlan::initSORT(
                         aStatement ,
                         aMyGraph->graph.myQuerySet ,
                         aMyGraph->graph.myPlan ,
                         &sSORT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sSORT;
    }
    else
    {
        // Nothing to do.
    }

    //---------------------------------------------------
    // ���� Plan�� ����
    //---------------------------------------------------

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = aMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_DISTINCT;

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make SORT
    //-----------------------

    if( (aMyGraph->graph.flag & QMG_DIST_OPT_TIP_MASK) !=
        QMG_DIST_OPT_TIP_INDEXABLE_DISINCT )
    {
        //----------------------------
        // SORT�� ����
        //----------------------------
        sFlag = 0;
        sFlag &= ~QMO_MAKESORT_METHOD_MASK;
        sFlag |= QMO_MAKESORT_SORT_BASED_DISTINCTION;
        sFlag &= ~QMO_MAKESORT_POSITION_MASK;
        sFlag |= QMO_MAKESORT_POSITION_LEFT;
        sFlag &= ~QMO_MAKESORT_PRESERVED_ORDER_MASK;
        sFlag |= QMO_MAKESORT_PRESERVED_FALSE;

        //���� ��ü�� ����
        if( (aMyGraph->graph.flag & QMG_GRAPH_TYPE_MASK) ==
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

        IDE_TEST( qmoOneMtrPlan::makeSORT(
                         aStatement ,
                         aMyGraph->graph.myQuerySet ,
                         sFlag ,
                         aMyGraph->graph.preservedOrder ,
                         aMyGraph->graph.myPlan ,
                         aMyGraph->graph.costInfo.inputRecordCnt,
                         sSORT ) != IDE_SUCCESS);
        aMyGraph->graph.myPlan = sSORT;

        qmg::setPlanInfo( aStatement, sSORT, &(aMyGraph->graph) );
    }

    //----------------------------
    // make GRBY
    //----------------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEGRBY_SORT_BASED_METHOD_MASK;
    sFlag |= QMO_MAKEGRBY_SORT_BASED_DISTINCTION;
    
    IDE_TEST( qmoOneNonPlan::makeGRBY( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       sFlag ,
                                       aMyGraph->graph.myPlan ,
                                       sGRBY )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sGRBY;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgDistinction::makeHashDistinction( qcStatement * aStatement,
                                     qmgDIST     * aMyGraph )

{
    UInt              sFlag;
    qmnPlan         * sHSDS;

  
    IDU_FIT_POINT_FATAL( "qmgDistinction::makeHashDistinction::__FT__" );

    //-----------------------------------------------------
    //        [HSDS]
    //          |
    //        [LEFT]
    //-----------------------------------------------------

    //----------------------------
    // Top-down �ʱ�ȭ
    //----------------------------

    //-----------------------
    // init HSDS
    //-----------------------

    IDE_TEST( qmoOneMtrPlan::initHSDS( aStatement ,
                                       aMyGraph->graph.myQuerySet ,
                                       ID_TRUE,
                                       aMyGraph->graph.myPlan ,
                                       &sHSDS )
                 != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sHSDS;

    //---------------------------------------------------
    // ���� Plan�� ����
    //---------------------------------------------------

    IDE_TEST( aMyGraph->graph.left->makePlan( aStatement ,
                                              &aMyGraph->graph,
                                              aMyGraph->graph.left )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = aMyGraph->graph.left->myPlan;

    //---------------------------------------------------
    // Process ���� ����
    //---------------------------------------------------
    aMyGraph->graph.myQuerySet->processPhase = QMS_MAKEPLAN_DISTINCT;

    //----------------------------
    // Bottom-up ����
    //----------------------------

    //-----------------------
    // make HSDS
    //-----------------------

    sFlag = 0;
    sFlag &= ~QMO_MAKEHSDS_METHOD_MASK;
    sFlag |= QMO_MAKEHSDS_HASH_BASED_DISTINCTION;

    //���� ��ü�� ����
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
                                       sHSDS )
              != IDE_SUCCESS);
    aMyGraph->graph.myPlan = sHSDS;

    qmg::setPlanInfo( aStatement, sHSDS, &(aMyGraph->graph) );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmgDistinction::printGraph( qcStatement  * aStatement,
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

    IDU_FIT_POINT_FATAL( "qmgDistinction::printGraph::__FT__" );

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
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDistinction::makeTargetOrder( qcStatement        * aStatement,
                                 qmsTarget          * aDistTarget,
                                 qmgPreservedOrder ** aNewTargetOrder )
{
/***********************************************************************
 *
 * Description : DISTINCT Target �÷��� �̿��Ͽ�
 *               Preserved Order �ڷ� ������ ������.
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsTarget           * sTarget;
    qmgPreservedOrder   * sNewOrder;
    qmgPreservedOrder   * sWantOrder;
    qmgPreservedOrder   * sCurOrder;
    qtcNode             * sNode;

    IDU_FIT_POINT_FATAL( "qmgDistinction::makeTargetOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDistTarget != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sWantOrder = NULL;
    sCurOrder = NULL;

    //------------------------------------------
    // Target �÷��� ���� Want Order�� ����
    //------------------------------------------

    for ( sTarget = aDistTarget;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        sNode = sTarget->targetColumn;

        // To Fix PR-11568
        // ������ Target Column�� ȹ���Ͽ��� �Ѵ�.
        // ORDER BY indicator��� �Բ� ����� Pass Node��
        // �߰������� ������ �� �����Ƿ� �̸� ����Ͽ��� �Ѵ�.
        // qmgSorting::optimize() ����
        //
        // BUG-20272
        if ( (sNode->node.module == &qtc::passModule) ||
             (sNode->node.module == &mtfDecrypt) )
        {
            sNode = (qtcNode*) sNode->node.arguments;
        }
        else
        {
            // nothing to do
        }
        
        //------------------------------------------
        // Target Į���� ���� want order�� ����
        //------------------------------------------

        IDE_TEST(
            QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgPreservedOrder),
                                           (void**)&sNewOrder )
            != IDE_SUCCESS );

        sNewOrder->table = sNode->node.table;
        sNewOrder->column = sNode->node.column;
        sNewOrder->direction = QMG_DIRECTION_NOT_DEFINED;
        sNewOrder->next = NULL;

        if ( sWantOrder == NULL )
        {
            sWantOrder = sCurOrder = sNewOrder;
        }
        else
        {
            sCurOrder->next = sNewOrder;
            sCurOrder = sCurOrder->next;
        }
    }

    *aNewTargetOrder = sWantOrder;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC
qmgDistinction::indexableDistinct( qcStatement      * aStatement,
                                   qmgGraph         * aGraph,
                                   qmsTarget        * aDistTarget,
                                   idBool           * aIndexableDistinct )
{
/***********************************************************************
 *
 * Description : Indexable Distinct ����ȭ ������ ���, ����
 *
 * Implementation :
 *    Preserved Order ��� ������ ���, ����
 *
 ***********************************************************************/

    qmgPreservedOrder * sWantOrder;

    IDU_FIT_POINT_FATAL( "qmgDistinction::indexableDistinct::__FT__" );

    // Target�� �̿��� Order �ڷ� ���� ����
    IDE_TEST( makeTargetOrder( aStatement, aDistTarget, & sWantOrder )
                 != IDE_SUCCESS );

    // Preserved Order ��밡�� ���θ� �˻�
    IDE_TEST( qmg::tryPreservedOrder( aStatement,
                                      aGraph,
                                      sWantOrder,
                                      aIndexableDistinct )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmgDistinction::getCostByPrevOrder( qcStatement      * aStatement,
                                    qmgDIST          * aDistGraph,
                                    SDouble          * aAccessCost,
                                    SDouble          * aDiskCost,
                                    SDouble          * aTotalCost )
{
/***********************************************************************
 *
 * Description :
 *
 *    Preserved Order ����� ����� Distinction ����� ����Ѵ�.
 *
 * Implementation :
 *
 *    �̹� Child�� ���ϴ� Preserved Order�� ������ �ִٸ�
 *    ������ ��� ���� Distinction�� �����ϴ�.
 *
 *    �ݸ� Child�� Ư�� �ε����� �����ϴ� �����,
 *    Child�� �ε����� �̿��� ����� ���Եǰ� �ȴ�.
 *
 ***********************************************************************/

    SDouble             sTotalCost;
    SDouble             sAccessCost;
    SDouble             sDiskCost;

    idBool              sUsable;

    qmgPreservedOrder * sWantOrder;

    qmoAccessMethod   * sOrgMethod;
    qmoAccessMethod   * sSelMethod;

    IDU_FIT_POINT_FATAL( "qmgDistinction::getCostByPrevOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDistGraph != NULL );

    //------------------------------------------
    // Preserved Order�� ����� �� �ִ� ���� �˻�
    //------------------------------------------

    // Target Į���� ���� want order�� ����
    sWantOrder = NULL;
    IDE_TEST( makeTargetOrder( aStatement,
                                  aDistGraph->graph.myQuerySet->SFWGH->target,
                                  & sWantOrder )
                 != IDE_SUCCESS );

    // preserved order ���� ���� �˻�
    IDE_TEST( qmg::checkUsableOrder( aStatement,
                                     sWantOrder,
                                     aDistGraph->graph.left,
                                     & sOrgMethod,
                                     & sSelMethod,
                                     & sUsable )
              != IDE_SUCCESS );

    //------------------------------------------
    // ��� ���
    //------------------------------------------

    if ( sUsable == ID_TRUE )
    {
        if ( aDistGraph->graph.left->preservedOrder == NULL )
        {
            if ( (sOrgMethod == NULL) || (sSelMethod == NULL) )
            {
                // BUG-43824 sorting ����� ����� �� access method�� NULL�� �� �ֽ��ϴ�
                // ������ ���� �̿��ϴ� ����̹Ƿ� 0�� �����Ѵ�.
                sAccessCost = 0;
                sDiskCost   = 0;
            }
            else
            {
                // ���õ� Access Method�� ������ AccessMethod ���̸�ŭ
                // �߰� ����� �߻��Ѵ�.
                sAccessCost = IDL_MAX( ( sSelMethod->accessCost - sOrgMethod->accessCost ), 0 );
                sDiskCost   = IDL_MAX( ( sSelMethod->diskCost   - sOrgMethod->diskCost   ), 0 );
            }
        }
        else
        {
            // �̹� Child�� Ordering�� �ϰ� ����.
            // ���ڵ� �Ǽ���ŭ�� �� ��븸�� �ҿ��.
            // BUG-41237 compare ��븸 �߰��Ѵ�.
            sAccessCost = aDistGraph->graph.left->costInfo.outputRecordCnt *
                          aStatement->mSysStat->mCompareTime;
            sDiskCost   = 0;
        }
        sTotalCost  = sAccessCost + sDiskCost;
    }
    else
    {
        // Preserved Order�� ����� �� ���� �����.
        sAccessCost = QMO_COST_INVALID_COST;
        sDiskCost   = QMO_COST_INVALID_COST;
        sTotalCost  = QMO_COST_INVALID_COST;
    }

    *aTotalCost  = sTotalCost;
    *aDiskCost   = sDiskCost;
    *aAccessCost = sAccessCost;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC
qmgDistinction::finalizePreservedOrder( qmgGraph * aGraph )
{
/***********************************************************************
 *
 *  Description : Preserved Order�� direction�� �����Ѵ�.
 *                direction�� NOT_DEFINED �� ��쿡�� ȣ���Ͽ��� �Ѵ�.
 *
 *  Implementation :
 *     1. Child graph�� Preserved order�� �������� �˻�
 *     2-1. �����ϴٸ� direction ����
 *     2-2. �ٸ��ٸ� direction�� ascending���� ����
 *
 ***********************************************************************/

    idBool              sIsSamePrevOrderWithChild;
    qmgPreservedOrder * sPreservedOrder;
    qmgDirectionType    sPrevDirection;

    IDU_FIT_POINT_FATAL( "qmgDistinction::finalizePreservedOrder::__FT__" );

    // BUG-36803 Distinction Graph must have a left child graph
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
        // ���� preserved order�� ������ �ʰ�
        // ���� preserved order�� ������ ���,
        // Preserved Order�� direction�� acsending���� ����
        sPreservedOrder = aGraph->preservedOrder;

        // ù��° Į���� ascending���� ����
        sPreservedOrder->direction = QMG_DIRECTION_ASC;
        sPrevDirection = QMG_DIRECTION_ASC;

        // �ι�° Į���� ���� Į���� direction ������ ���� ������
        for ( sPreservedOrder = sPreservedOrder->next;
              sPreservedOrder != NULL;
              sPreservedOrder = sPreservedOrder->next )
        {
            switch( sPreservedOrder->direction )
            {
                case QMG_DIRECTION_NOT_DEFINED :
                    sPreservedOrder->direction = QMG_DIRECTION_ASC;
                    break;
                case QMG_DIRECTION_SAME_WITH_PREV :
                    sPreservedOrder->direction = sPrevDirection;
                    break;
                case QMG_DIRECTION_DIFF_WITH_PREV :
                    // direction�� ���� Į���� direction�� �ٸ� ���
                    if ( sPrevDirection == QMG_DIRECTION_ASC )
                    {
                        sPreservedOrder->direction = QMG_DIRECTION_DESC;
                    }
                    else
                    {
                        // sPrevDirection == QMG_DIRECTION_DESC
                        sPreservedOrder->direction = QMG_DIRECTION_ASC;
                    }
                    break;
                case QMG_DIRECTION_ASC :
                    IDE_DASSERT(0);
                    break;
                case QMG_DIRECTION_DESC :
                    IDE_DASSERT(0);
                    break;
                case QMG_DIRECTION_ASC_NULLS_FIRST :
                    IDE_DASSERT(0);
                    break;
                case QMG_DIRECTION_ASC_NULLS_LAST :
                    IDE_DASSERT(0);
                    break;
                case QMG_DIRECTION_DESC_NULLS_FIRST :
                    IDE_DASSERT(0);
                    break;
                case QMG_DIRECTION_DESC_NULLS_LAST :
                    IDE_DASSERT(0);
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
            
            sPrevDirection = sPreservedOrder->direction;
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
