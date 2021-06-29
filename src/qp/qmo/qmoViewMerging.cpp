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
 * $Id: qmoViewMerging.cpp 23857 2008-03-19 02:36:53Z sungminee $
 **********************************************************************/

#include <idl.h>
#include <qcg.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qmvOrderBy.h>
#include <qmoNormalForm.h>
#include <qmoViewMerging.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmv.h>
#include <qmsDefaultExpr.h>
#include <qmv.h>
#include <sdi.h> /* TASK-7219 Shard Transformer Refactoring */

IDE_RC
qmoViewMerging::doTransform( qcStatement  * aStatement,
                             qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ��ü query�� parseTree�� ���Ͽ� View Merging�� �����ϰ�
 *     transformed parseTree�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoViewMerging::doTransform::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    //------------------------------------------
    // Simple View Merging ����
    //------------------------------------------

    if ( QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE == 0 )
    {
        // Simple View Merge�� �����Ѵ�.
        IDE_TEST( processTransform( aStatement,
                                    aStatement->myPlan->parseTree,
                                    aQuerySet )
                  != IDE_SUCCESS );

        // merge�� ���� view reference�� �����Ѵ�.
        IDE_TEST( modifySameViewRef( aStatement )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_SIMPLE_VIEW_MERGE_DISABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::doTransformSubqueries( qcStatement * aStatement,
                                       qtcNode     * aNode )
{
    IDU_FIT_POINT_FATAL( "qmoViewMerging::doTransformSubqueries::__FT__" );

    if ( QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE == 0 )
    {
        IDE_TEST( processTransformForExpression( aStatement,
                                                 aNode )
                  != IDE_SUCCESS );
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
qmoViewMerging::processTransform( qcStatement  * aStatement,
                                  qcParseTree  * aParseTree,
                                  qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     query block�� ���Ͽ� subquery�� ������ ��� query set��
 *     bottom-up���� ��ȸ�ϸ� Simple View Merging�� �����Ѵ�.
 *     (query block�̶� qmsParseTree�� �ǹ��Ѵ�.)
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool         sIsTransformed;
    qmsParseTree * sParseTree;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::processTransform::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    /* TASK-7219 ���������� ������ Shard View���� View Mergingg�� �������� �ʴ´�. */
    IDE_TEST_CONT( ( ( aParseTree->stmtShard != QC_STMT_SHARD_NONE )
                     &&
                     ( aParseTree->stmtShard != QC_STMT_SHARD_META ) ),
                   NORMAL_EXIT );

    //------------------------------------------
    // Simple View Merging�� ����
    //------------------------------------------

    IDE_TEST( processTransformForQuerySet( aStatement,
                                           aQuerySet,
                                           & sIsTransformed )
              != IDE_SUCCESS );

    /* TASK-7219 */
    //------------------------------------------
    // ORDER-BY ���� validation ����
    //------------------------------------------
    if ( sIsTransformed == ID_TRUE )
    {
        sParseTree = (qmsParseTree *)aParseTree;

        /* parseTree�� ��庯ȭ�� �߻��Ǿ����� ����Ѵ�. */
        sParseTree->isTransformed = ID_TRUE;

        /* SET������ �ִ� ��� ORDER-BY ���� ������� �ʴ´�. */
        if ( aQuerySet->setOp == QMS_NONE )
        {
            IDE_TEST( validateOrderBy( aStatement,
                                       sParseTree )
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

    /* TASK-7219 */
    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::processTransformForQuerySet( qcStatement  * aStatement,
                                             qmsQuerySet  * aQuerySet,
                                             idBool       * aIsTransformed )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     query set�� ���Ͽ� subquery�� ������ ��� query set��
 *     bottom-up���� ��ȸ�ϸ� Simple View Merging�� �����Ѵ�.
 *
 * Implementation :
 *     ���� query set�� ���� ���� view�� ��ȸ�ϸ�
 *     (1) view�� simple view����, merge �������� �˻��Ѵ�.
 *     (2) view�� merge�Ѵ�.
 *     (3) merge�� view�� �����Ѵ�.
 *     (4) ���� query set�� ���Ͽ� �ٽ� validation�� �����Ѵ�.
 *
 ***********************************************************************/

    qmsQuerySet  * sCurrentQuerySet;
    qmsSFWGH     * sCurrentSFWGH;
    qmsParseTree * sUnderBlock;
    qmsQuerySet  * sUnderQuerySet;
    qmsFrom      * sFrom;
    qmsTarget    * sTarget;
    idBool         sIsTransformed;
    idBool         sIsSimpleQuery;
    idBool         sCanMergeView;
    idBool         sIsMerged;
    idBool         sIsMergedForShard = ID_FALSE;  /* TASK-7219 Shard Transformer Refactoring */

    IDU_FIT_POINT_FATAL( "qmoViewMerging::processTransformForQuerySet::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aIsTransformed != NULL );
    
    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sCurrentQuerySet = aQuerySet;
    sIsTransformed = ID_FALSE;
    
    //------------------------------------------
    // Simple View Merging�� ����
    //------------------------------------------
    
    if ( sCurrentQuerySet->setOp == QMS_NONE )
    {
        sCurrentSFWGH = (qmsSFWGH *)sCurrentQuerySet->SFWGH;

        // Subquery���� ��� ã�� view merging�� ���� �õ��Ѵ�.

        // SELECT���� subquery�� ã�� view merging �õ�
        for( sTarget = sCurrentSFWGH->target;
             sTarget != NULL;
             sTarget = sTarget->next )
        {
            IDE_TEST( processTransformForExpression( aStatement, sTarget->targetColumn )
                      != IDE_SUCCESS );
        }

        // WHERE���� subquery�� ã�� view merging �õ�
        IDE_TEST( processTransformForExpression( aStatement, sCurrentSFWGH->where )
                  != IDE_SUCCESS );

        // HAVING���� subquery�� ã�� view merging �õ�
        IDE_TEST( processTransformForExpression( aStatement, sCurrentSFWGH->having )
                  != IDE_SUCCESS );

        if( sCurrentSFWGH->hierarchy != NULL )
        {
            // START WITH ���� subquery�� ã�� view merging �õ�
            IDE_TEST( processTransformForExpression( aStatement, sCurrentSFWGH->hierarchy->startWith )
                      != IDE_SUCCESS );

            // CONNECT BY ���� subquery�� ã�� view merging �õ�
            IDE_TEST( processTransformForExpression( aStatement, sCurrentSFWGH->hierarchy->connectBy )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        // FROM���� �� view�� merging �õ�
        for ( sFrom = sCurrentSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            if ( sFrom->joinType == QMS_NO_JOIN )
            {
                if ( sFrom->tableRef->view != NULL )
                {
                    sUnderBlock = (qmsParseTree *) sFrom->tableRef->view->myPlan->parseTree;
                    sUnderQuerySet = sUnderBlock->querySet;

                    // ���� view�� ���Ͽ� bottom-up���� ��ȸ�ϸ�
                    // Simple View Merging�� �����Ѵ�.
                    IDE_TEST( processTransform( aStatement,
                                                &(sUnderBlock->common),
                                                sUnderQuerySet )
                              != IDE_SUCCESS );

                    //------------------------------------------
                    // (1) Simple View & Merge ���� �˻�
                    //------------------------------------------

                    // �̹� view�̹Ƿ� simple query������ �˻��Ѵ�.
                    IDE_TEST( isSimpleQuery( sUnderBlock,
                                             & sIsSimpleQuery )
                              != IDE_SUCCESS );
                    
                    if ( sIsSimpleQuery  == ID_TRUE )
                    {
                        // merge �������� �˻��Ѵ�.
                        // ���� querySet�� ���� querySet�� SET�� ����.
                        IDE_TEST( canMergeView( aStatement,
                                                sCurrentQuerySet->SFWGH,
                                                sUnderQuerySet->SFWGH,
                                                sFrom->tableRef,
                                                & sCanMergeView )
                                  != IDE_SUCCESS );
                        
                        if ( sCanMergeView == ID_TRUE )
                        {
                            /* TASK-7219 Shard Transformer Refactoring */
                            if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                                                SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
                                 == ID_TRUE )
                            {
                                sIsMergedForShard = sUnderQuerySet->SFWGH->isTransformed;

                                sUnderQuerySet->SFWGH->isTransformed = ID_FALSE;

                                IDE_TEST( sdi::preAnalyzeQuerySet( aStatement,
                                                                   sUnderQuerySet,
                                                                   QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                                          != IDE_SUCCESS );

                                sUnderQuerySet->SFWGH->isTransformed = sIsMergedForShard;
                            }
                            else
                            {
                                /* Nothing to do */
                            }

                            //------------------------------------------
                            // (2) Merge�� �����Ѵ�.
                            //------------------------------------------
                            
                            IDE_TEST( processMerging( aStatement,
                                                      sCurrentQuerySet->SFWGH,
                                                      sUnderQuerySet->SFWGH,
                                                      sFrom,
                                                      & sIsMerged )
                                      != IDE_SUCCESS );
                            
                            if ( sIsMerged == ID_TRUE )
                            {
                                // ���� SFWGH�� ���� SFWGH��
                                // merge�Ǿ����� ǥ���Ѵ�.
                                sUnderQuerySet->SFWGH->mergedSFWGH =
                                    sCurrentQuerySet->SFWGH;
                                
                                sFrom->tableRef->isMerged = ID_TRUE;

                                sIsTransformed = ID_TRUE;

                                // PROJ-2462 Result Cache
                                sCurrentQuerySet->lflag |= sUnderQuerySet->lflag & QMV_QUERYSET_RESULT_CACHE_INVALID_MASK;
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
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                IDE_TEST( processTransformForJoinedTable( aStatement,
                                                          sFrom )
                          != IDE_SUCCESS );
            }
        }

        if ( sIsTransformed == ID_TRUE )
        {
            //------------------------------------------
            // (3) Merge�� View�� �����Ѵ�.
            //------------------------------------------
            
            IDE_TEST( removeMergedView( sCurrentQuerySet->SFWGH )
                      != IDE_SUCCESS );

            // SFWGH�� ��庯ȭ�� �߻��Ǿ����� ����Ѵ�.
            sCurrentQuerySet->SFWGH->isTransformed = ID_TRUE;

            //------------------------------------------
            // (4) validation�� �����Ѵ�.
            //------------------------------------------
            
            IDE_TEST( validateSFWGH( aStatement,
                                     sCurrentQuerySet->SFWGH )
                      != IDE_SUCCESS );

            // dependency�� �缳���Ѵ�.
            qtc::dependencySetWithDep( & sCurrentQuerySet->depInfo,
                                       & sCurrentQuerySet->SFWGH->depInfo );

            // set outer column dependencies
            IDE_TEST( qmvQTC::setOuterDependencies( sCurrentQuerySet->SFWGH,
                                                    & sCurrentQuerySet->SFWGH->outerDepInfo )
                      != IDE_SUCCESS );
            
            qtc::dependencySetWithDep( & sCurrentQuerySet->outerDepInfo,
                                       & sCurrentQuerySet->SFWGH->outerDepInfo );
            
            IDE_TEST( checkViewDependency( aStatement,
                                           & sCurrentQuerySet->outerDepInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // left subquery�� simple view merging ����
        IDE_TEST( processTransformForQuerySet( aStatement,
                                               sCurrentQuerySet->left,
                                               & sIsMerged )
                  != IDE_SUCCESS );

        /* TASK-7219 */
        if ( sIsMerged == ID_TRUE )
        {
            sIsTransformed = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        // right subquery�� simple view merging ����
        IDE_TEST( processTransformForQuerySet( aStatement,
                                               sCurrentQuerySet->right,
                                               & sIsMerged )
                  != IDE_SUCCESS );

        /* TASK-7219 */
        if ( sIsMerged == ID_TRUE )
        {
            sIsTransformed = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        // outer column dependency�� ���� dependency�� OR-ing�Ѵ�.
        qtc::dependencyClear( & sCurrentQuerySet->outerDepInfo );

        IDE_TEST( qtc::dependencyOr( & sCurrentQuerySet->left->outerDepInfo,
                                     & sCurrentQuerySet->outerDepInfo,
                                     & sCurrentQuerySet->outerDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & sCurrentQuerySet->right->outerDepInfo,
                                     & sCurrentQuerySet->outerDepInfo,
                                     & sCurrentQuerySet->outerDepInfo )
                  != IDE_SUCCESS );
    }

    *aIsTransformed = sIsTransformed;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::processTransformForJoinedTable( qcStatement  * aStatement,
                                                qmsFrom      * aFrom )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     joined table�� ���� query set�� ��ȸ�ϸ� simple view merging��
 *     �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsParseTree * sViewParseTree;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::processTransformForJoinedTable::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom != NULL );

    //------------------------------------------
    // Joined Table�� bottom-up ��ȸ
    //------------------------------------------

    if ( aFrom->joinType == QMS_NO_JOIN )
    {
        if ( aFrom->tableRef->view != NULL )
        {
            sViewParseTree = (qmsParseTree *) aFrom->tableRef->view->myPlan->parseTree;

            //------------------------------------------
            // Simple View Merging ����
            //------------------------------------------

            IDE_TEST( processTransform( aStatement,
                                        &(sViewParseTree->common),
                                        sViewParseTree->querySet )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        IDE_TEST( processTransformForExpression( aStatement, aFrom->onCondition )
                  != IDE_SUCCESS );

        IDE_TEST( processTransformForJoinedTable( aStatement,
                                                  aFrom->left )
                  != IDE_SUCCESS );

        IDE_TEST( processTransformForJoinedTable( aStatement,
                                                  aFrom->right )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::processTransformForExpression( qcStatement * aStatement,
                                               qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : Predicate�̳� expression�� ���Ե� subquery�� ã��
 *               view merging�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::processTransformForExpression::__FT__" );

    if( aNode != NULL )
    {
        if( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
        {
            if( aNode->node.module == &qtc::subqueryModule )
            {
                IDE_TEST( doTransform( aNode->subquery,
                                       ( (qmsParseTree *)( aNode->subquery->myPlan->parseTree ) )->querySet )
                          != IDE_SUCCESS );
            }
            else
            {
                for( sNode = (qtcNode *)aNode->node.arguments;
                     sNode != NULL;
                     sNode = (qtcNode *)sNode->node.next )
                {
                    IDE_TEST( processTransformForExpression( aStatement,
                                                             sNode )
                              != IDE_SUCCESS );
                }
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::isSimpleQuery( qmsParseTree * aParseTree,
                               idBool       * aIsSimpleQuery )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     Simple Query���� �˻��Ѵ�.
 *
 * Implementation :
 *     (1) SELECT, FROM, WHERE ���� �ְ�
 *     (2) AGGREGATION�� ����.
 *     (3) target�� DISTINCT�� ����, Analytic Function�� ����.
 *     (4) target�� DISTINCT�� ����.
 *     (5) START WITH, CONNECT BY ���� ����.
 *     (6) GROUP BY, HAVING ���� ����.
 *     (7) ORDER BY, LIMIT �Ǵ� LOOP ���� ����.
 *     (8) SHARD ������ �ƴϴ�.
 *
 ***********************************************************************/

    qmsQuerySet * sQuerySet;
    qmsSFWGH    * sSFWGH;
    idBool        sIsSimpleQuery = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::isSimpleQuery::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aParseTree != NULL );
    IDE_DASSERT( aIsSimpleQuery != NULL );

    //------------------------------------------
    // Simple Query �˻�
    //------------------------------------------

    if ( ( aParseTree->orderBy == NULL ) &&
         ( aParseTree->limit == NULL ) &&
         ( aParseTree->loopNode == NULL ) &&
         ( aParseTree->common.stmtShard == QC_STMT_SHARD_NONE ) ) // PROJ-2638
    {
        sQuerySet = aParseTree->querySet;

        if ( sQuerySet->setOp == QMS_NONE )
        {
            sSFWGH = sQuerySet->SFWGH;

            if ( ( sSFWGH->selectType == QMS_ALL ) &&
                 ( sSFWGH->hierarchy == NULL ) &&
                 ( sSFWGH->top == NULL ) && /* BUG-36580 supported TOP */
                 ( sSFWGH->group == NULL ) &&
                 ( sSFWGH->having == NULL ) &&
                 ( sSFWGH->aggsDepth1 == NULL ) &&
                 ( sSFWGH->aggsDepth2 == NULL ) &&
                 ( sQuerySet->analyticFuncList == NULL ) )
            {
                sIsSimpleQuery = ID_TRUE;
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

    *aIsSimpleQuery = sIsSimpleQuery;

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::canMergeView( qcStatement  * aStatement,
                              qmsSFWGH     * aCurrentSFWGH,
                              qmsSFWGH     * aUnderSFWGH,
                              qmsTableRef  * aUnderTableRef,
                              idBool       * aCanMergeView )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ���� SFWGH�� ���� SFWGH�� merge �������� �˻��Ѵ�.
 *
 * Implementation :
 *     (1) Environment ���� �˻�
 *     (2) ���� SFWGH ���� �˻�
 *     (3) ���� SFWGH ���� �˻�
 *     (4) Dependency �˻�
 *     (5) NormalForm �˻�
 *
 ***********************************************************************/

    idBool  sCanMergeView = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::canMergeView::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aUnderTableRef != NULL );
    IDE_DASSERT( aCanMergeView != NULL );

    //------------------------------------------
    // Merge ���� ���� �˻�
    //------------------------------------------

    while ( 1 )
    {
        //------------------------------------------
        // (1) Environment ���� �˻�
        //------------------------------------------

        // �̹� ����
        
        //------------------------------------------
        // (2) ���� SFWGH ���� �˻�
        //------------------------------------------
        
        IDE_TEST( checkCurrentSFWGH( aCurrentSFWGH,
                                     & sCanMergeView )
                  != IDE_SUCCESS );
        
        if ( sCanMergeView == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // (3) ���� SFWGH ���� �˻�
        //------------------------------------------
        
        IDE_TEST( checkUnderSFWGH( aStatement,
                                   aUnderSFWGH,
                                   aUnderTableRef,
                                   & sCanMergeView )
                  != IDE_SUCCESS );
        
        if ( sCanMergeView == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // (4) Dependency �˻�
        //------------------------------------------
        
        IDE_TEST( checkDependency( aCurrentSFWGH,
                                   aUnderSFWGH,
                                   & sCanMergeView )
                  != IDE_SUCCESS );
            
        if ( sCanMergeView == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // (5) NormalForm �˻�
        //------------------------------------------
        
        IDE_TEST( checkNormalForm( aStatement,
                                   aCurrentSFWGH,
                                   aUnderSFWGH,
                                   & sCanMergeView )
                  != IDE_SUCCESS );
            
        if ( sCanMergeView == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }

        break;
    }
    
    *aCanMergeView = sCanMergeView;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::checkCurrentSFWGH( qmsSFWGH     * aSFWGH,
                                   idBool       * aCanMerge )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ���� query block�� merge ������ �˻��Ѵ�.
 *
 * Implementation :
 *     (1) hint �˻�
 *     (2) pseudo column �˻�
 *
 ***********************************************************************/

    idBool  sCanMerge = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::checkCurrentSFWGH::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aSFWGH != NULL );
    IDE_DASSERT( aCanMerge != NULL );

    //------------------------------------------
    // ���� SFWGH �˻�
    //------------------------------------------
    
    while ( 1 )
    {
        //------------------------------------------
        // hint �˻�
        //------------------------------------------
        
        // dnf
        if ( aSFWGH->hints->normalType == QMO_NORMAL_TYPE_DNF )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    
        // ordered
        if ( aSFWGH->hints->joinOrderType == QMO_JOIN_ORDER_TYPE_ORDERED )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // pseudo column �˻�
        //------------------------------------------

        // BUG-37314 ���������� rownum �� �ִ� ��쿡�� unnest �� �����ؾ� �Ѵ�.
        if( ( aSFWGH->outerQuery != NULL ) &&
            ( aSFWGH->rownum     != NULL ) )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        //------------------------------------------
        // PROJ-1653 Outer Join Operator (+)
        //------------------------------------------

        if ( aSFWGH->where != NULL )
        {
            if( ( aSFWGH->where->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                sCanMerge = ID_FALSE;
                break;
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

        /*
         * PROJ-1715 Hierarchy Query Exstension
         */
        if ( aSFWGH->hierarchy != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            /* Nothing to do */
        }
        break;
    }
    
    *aCanMerge = sCanMerge;

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::checkUnderSFWGH( qcStatement  * aStatement,
                                 qmsSFWGH     * aSFWGH,
                                 qmsTableRef  * aTableRef,
                                 idBool       * aCanMerge )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ���� query block�� merge ������ �˻��Ѵ�.
 *
 *     [Enhancement]
 *     view target�� subquery, user-defined function �� merge�� �� ����
 *     simple view�� �ϴ��� �ش� target�� �������� �ʴ� ��쿡��
 *     merge�� �����ϵ��� �����Ѵ�.
 *
 *     ex) select count(*) from ( select func1(i1) from t1 ) v1;
 *         -> select count(*) from t1;
 *
 * Implementation :
 *     (1) hint �˻�
 *     (2) pseudo column �˻�
 *     (3) performance view �˻�
 *     (4) target list �˻�
 *     (5) disk table �˻�
 *
 ***********************************************************************/

    qmsTarget         * sViewTarget = NULL;
    idBool              sCanMerge   = ID_TRUE;
    qmsColumnRefList  * sColumnRef  = NULL;
    mtcColumn         * sViewColumn = NULL;
    UShort              sViewColumnOrder;
    UShort              sTargetOrder;
    qmsFrom           * sViewFrom   = NULL;
    qmsNoMergeHints   * sNoMergeHint;
    qcDepInfo           sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::checkUnderSFWGH::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSFWGH != NULL );
    IDE_DASSERT( aTableRef != NULL );
    IDE_DASSERT( aCanMerge != NULL );

    //------------------------------------------
    // ���� SFWGH �˻�
    //------------------------------------------

    while ( 1 )
    {
        //------------------------------------------
        // hint �˻�
        //------------------------------------------
        
        // currentSFWGH���� ���� view�� ���Ͽ� no_merge,
        // push_selection_view, push_pred�� ����� ���
        if ( aTableRef->noMergeHint == ID_TRUE )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        // underSFWGH���� push_selection_view�� ����� ���
        if ( aSFWGH->hints->viewOptHint != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        // underSFWGH���� push_pred�� ����� ���
        if ( aSFWGH->hints->pushPredHint != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }    
        
        // dnf
        if ( aSFWGH->hints->normalType == QMO_NORMAL_TYPE_DNF )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        // ordered
        if ( aSFWGH->hints->joinOrderType == QMO_JOIN_ORDER_TYPE_ORDERED )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        // BUG-43536 no_merge() ��Ʈ ����
        for( sNoMergeHint = aSFWGH->hints->noMergeHint;
             sNoMergeHint != NULL;
             sNoMergeHint = sNoMergeHint->next )
        {
            if ( sNoMergeHint->table == NULL )
            {
                sCanMerge = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        //------------------------------------------
        // pseudo column �˻�
        //------------------------------------------
        
        if ( aSFWGH->rownum != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        if ( aSFWGH->level != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        /* PROJ-1715 */
        if ( aSFWGH->isLeaf != NULL )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            /* Nothing to do */
        }

        //------------------------------------------
        // performance view �˻�
        //------------------------------------------
        
        // performance view�� merge�� �� ����.
        if ( aTableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW )
        {
            sCanMerge = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        /* TASK-7219 Shard Transformer Refactoring */
        if ( ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                              SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
               == ID_TRUE )
             &&
             ( aTableRef->withStmt != NULL ) )
        {
            sCanMerge = ID_FALSE;

            break;
        }
        else
        {
            /* Nothing to do */
        }


        //------------------------------------------
        // PROJ-1653 Outer Join Operator (+)
        //------------------------------------------

        if ( aSFWGH->where != NULL )
        {
            if( ( aSFWGH->where->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                sCanMerge = ID_FALSE;
                break;
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
        
        // PROJ-2418
        // UnderSFWGH�� From���� Lateral View�� �����ϸ� Merging �� �� ����.
        // ��, Lateral View�� ������ ��� Merging �Ǿ��ٸ� Merging�� �����ϴ�.
        for ( sViewFrom = aSFWGH->from; sViewFrom != NULL; sViewFrom = sViewFrom->next )
        {
            IDE_TEST( qmvQTC::getFromLateralDepInfo( sViewFrom, & sDepInfo )
                      != IDE_SUCCESS );

            if ( qtc::haveDependencies( & sDepInfo ) == ID_TRUE )
            {
                // �ش� From�� Lateral View�� �����ϸ� Merging �Ұ�
                sCanMerge = ID_FALSE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        //------------------------------------------
        // ���� query block���� �����ϴ� target column  �˻�
        //------------------------------------------

        // BUGBUG ������ view �÷��� �ߺ��˻��� �� �ִ�.
        for ( sColumnRef = aTableRef->viewColumnRefList;
              sColumnRef != NULL;
              sColumnRef = sColumnRef->next )
        {
            if ( sColumnRef->column->node.module == & qtc::passModule )
            {
                // view ���� �÷��̾��ٰ� passNode�� �ٲ���
                
                // Nothing to do.
            }
            else
            {
                IDE_DASSERT( sColumnRef->column->node.module == & qtc::columnModule );
                
                sViewColumn = QTC_STMT_COLUMN( aStatement, sColumnRef->column );
                sViewColumnOrder = sViewColumn->column.id & SMI_COLUMN_ID_MASK;
                
                sTargetOrder = 0;
                for ( sViewTarget = aSFWGH->target;
                      sViewTarget != NULL;
                      sViewTarget = sViewTarget->next )
                {
                    if ( sTargetOrder == sViewColumnOrder )
                    {
                        break;
                    }
                    else
                    {
                        sTargetOrder++;
                    }
                }
            
                IDE_TEST_RAISE( sViewTarget == NULL, ERR_COLUMN_NOT_FOUND );

                // (1) subquery �������� �ʴ´�.
                if ((sViewTarget->targetColumn->lflag & QTC_NODE_SUBQUERY_MASK)
                    == QTC_NODE_SUBQUERY_EXIST)
                {
                    sCanMerge = ID_FALSE;
                    break;
                }

                // (2) user-defined function�� �������� �ʴ´�.
                if ( ( sViewTarget->targetColumn->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                     == QTC_NODE_PROC_FUNCTION_TRUE )
                {
                    sCanMerge = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            
                // (3) variable build-in function�� �������� �ʴ´�.
                if ( ( sViewTarget->targetColumn->lflag & QTC_NODE_VAR_FUNCTION_MASK )
                     == QTC_NODE_VAR_FUNCTION_EXIST )
                {
                    sCanMerge = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }

                /* (4) _prowid �� �������� �ʴ´�. (BUG-41218) */
                if ( ( sViewTarget->targetColumn->lflag & QTC_NODE_COLUMN_RID_MASK )
                     == QTC_NODE_COLUMN_RID_EXIST)
                {
                    sCanMerge = ID_FALSE;
                    break;
                }
                else
                {
                    /* nothing to do */
                }

                /* TASK-7219 Shard Transformer Refactoring
                 *  - (5) Bind
                 */
                if ( ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                                      SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
                       == ID_TRUE )
                     &&
                     ( MTC_NODE_IS_DEFINED_TYPE( &( sViewTarget->targetColumn->node ) )
                       == ID_FALSE ) )
                {
                    sCanMerge = ID_FALSE;

                    break;
                }
                else
                {
                    /* nothing to do */
                }
            }
        }

        break;
    }
        
    *aCanMerge = sCanMerge;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoViewMerging::checkUnderSFWGH",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::checkDependency( qmsSFWGH     * aCurrentSFWGH,
                                 qmsSFWGH     * aUnderSFWGH,
                                 idBool       * aCanMerge )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     merge�� �ִ� ���� relation�� ���� ���� �ʴ��� �˻��Ѵ�.
 *
 * Implementation :
 *     �󼼼���� �� SFWGHO�� ���Ͽ� ���� dependency ������ �˻��Ϸ���
 *     ������ �ڵ� ���⵵�� ��������, �˻��������� ��Ȯ�ϰ� �˻���
 *     �ʿ䰡 ����, ũ�� SFWGH ��ü�� ���� dependency ����������
 *     ����Ѵ�. SFWGH�� dependency�� SFWGH ���� inner dependency��
 *     outer dependency�� ������ ����Ѵ�.
 *
 *     �׸���, order-by ���� ���ؼ��� outer dependency�� ���� �� ����,
 *     SFWGH dependency�� ���ϹǷ� ������ ����� �ʿ䰡 ����.
 *
 ***********************************************************************/

    UInt       sDepCount;
    idBool     sCanMerge = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::checkDependency::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aCanMerge != NULL );

    //------------------------------------------
    // dependency �˻�
    //------------------------------------------

    // SFWGH�� dependency
    sDepCount = aCurrentSFWGH->depInfo.depCount +
        aUnderSFWGH->depInfo.depCount;

    // SFWGH�� outer dependency
    sDepCount += aCurrentSFWGH->outerDepInfo.depCount +
        aUnderSFWGH->outerDepInfo.depCount;

    IDE_DASSERT( sDepCount > 0 );
    
    // merge�� ���ŵ� view�� dependency�� �ϳ� ����.
    sDepCount -= 1;
    
    // merge�� ���� �ִ� dependency�� �˻��Ѵ�.
    if ( sDepCount > QC_MAX_REF_TABLE_CNT )
    {
        sCanMerge = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    *aCanMerge = sCanMerge;

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::checkNormalForm( qcStatement  * aStatement,
                                 qmsSFWGH     * aCurrentSFWGH,
                                 qmsSFWGH     * aUnderSFWGH,
                                 idBool       * aCanMerge )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     merge�� predicate�� normal form maximum�� �˻��Ѵ�.
 *
 * Implementation :
 *     �� where���� AND�� ����� ���̹Ƿ� ���� ���� ���Ͽ� ���Ѵ�.
 *
 ***********************************************************************/

    UInt       sCurrentEstimateCnfCnt = 0;
    UInt       sUnderEstimateCnfCnt = 0;
    UInt       sNormalFormMaximum;
    idBool     sCanMerge = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::checkNormalForm::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aCanMerge != NULL );

    //------------------------------------------
    // normal form �˻�
    //------------------------------------------

    if ( aCurrentSFWGH->where != NULL )
    {
        IDE_TEST( qmoNormalForm::estimateCNF( aCurrentSFWGH->where,
                                              & sCurrentEstimateCnfCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( aUnderSFWGH->where != NULL )
    {
        IDE_TEST( qmoNormalForm::estimateCNF( aUnderSFWGH->where,
                                              & sUnderEstimateCnfCnt )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    sNormalFormMaximum = QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement );

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );
    
    // and�� ����� �����Ƿ� ���ϸ� �ȴ�.
    // Ȥ�� ��� �ϳ��� normalFormMaxinum�� ������ merge���� �ʴ´�.
    if ( sCurrentEstimateCnfCnt + sUnderEstimateCnfCnt > sNormalFormMaximum )
    {
        sCanMerge = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    *aCanMerge = sCanMerge;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::processMerging( qcStatement  * aStatement,
                                qmsSFWGH     * aCurrentSFWGH,
                                qmsSFWGH     * aUnderSFWGH,
                                qmsFrom      * aUnderFrom,
                                idBool       * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     �� ���� ���Ͽ� merge�� �����Ѵ�.
 *
 * Implementation :
 *     (1) hint ���� merge�Ѵ�.
 *     (2) from ���� merge�Ѵ�.
 *     (3) target list�� merge�Ѵ�.
 *     (4) where ���� merge�Ѵ�.
 *
 ***********************************************************************/

    qmoViewRollbackInfo   sRollbackInfo;
    qmsTarget           * sTarget;
    idBool                sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::processMerging::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aUnderFrom != NULL );
    IDE_DASSERT( aIsMerged != NULL );

    //------------------------------------------
    // merge ����
    //------------------------------------------

    sRollbackInfo.hintMerged   = ID_FALSE;
    sRollbackInfo.targetMerged = ID_FALSE;
    sRollbackInfo.fromMerged   = ID_FALSE;
    sRollbackInfo.whereMerged  = ID_FALSE;

    while ( 1 )
    {
        //------------------------------------------
        // hint ���� merge ����
        //------------------------------------------

        IDE_TEST( mergeForHint( aCurrentSFWGH,
                                aUnderSFWGH,
                                aUnderFrom,
                                & sRollbackInfo,
                                & sIsMerged )
                  != IDE_SUCCESS );
        
        if ( sIsMerged == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    
        //------------------------------------------
        // from ���� merge ����
        //------------------------------------------
        
        IDE_TEST( mergeForFrom( aStatement,
                                aCurrentSFWGH,
                                aUnderSFWGH,
                                aUnderFrom,
                                & sRollbackInfo,
                                & sIsMerged )
                  != IDE_SUCCESS );
        
        if ( sIsMerged == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    
        //------------------------------------------
        // target list�� merge ����
        //------------------------------------------
        
        IDE_TEST( mergeForTargetList( aStatement,
                                      aUnderSFWGH,
                                      aUnderFrom->tableRef,
                                      & sRollbackInfo,
                                      & sIsMerged )
                  != IDE_SUCCESS );
        
        if ( sIsMerged == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    
        //------------------------------------------
        // where ���� merge ����
        //------------------------------------------
        
        IDE_TEST( mergeForWhere( aStatement,
                                 aCurrentSFWGH,
                                 aUnderSFWGH,
                                 & sRollbackInfo,
                                 & sIsMerged )
                  != IDE_SUCCESS );
        
        if ( sIsMerged == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        //------------------------------------------
        // ���� merge�� ���� dependency ���� ����
        //------------------------------------------
        
        // view dependency ����
        qtc::dependencyRemove( aUnderFrom->tableRef->table,
                               & aCurrentSFWGH->depInfo,
                               & aCurrentSFWGH->depInfo );
        
        IDE_TEST( qtc::dependencyOr( & aUnderSFWGH->depInfo,
                                     & aCurrentSFWGH->depInfo,
                                     & aCurrentSFWGH->depInfo )
                  != IDE_SUCCESS );
        
        IDE_TEST( qtc::dependencyOr( & aUnderSFWGH->outerDepInfo,
                                     & aCurrentSFWGH->outerDepInfo,
                                     & aCurrentSFWGH->outerDepInfo )
                  != IDE_SUCCESS );

        //------------------------------------------
        // PROJ-2418
        // LATERAL_VIEW Flag�� Unmask �Ѵ�.
        //------------------------------------------
        aUnderFrom->tableRef->flag &= ~QMS_TABLE_REF_LATERAL_VIEW_MASK;
        aUnderFrom->tableRef->flag |= QMS_TABLE_REF_LATERAL_VIEW_FALSE;
        
        break;
    }
    
    //------------------------------------------
    // merge�� ������ ��� rollback ����
    //------------------------------------------

    if ( sIsMerged == ID_TRUE )
    {
        // PROJ-2179
        // Merge�� ��� ORDER BY�� SELECT���� attribute ���� ���谡 �缳���Ǿ�� �Ѵ�.
        for( sTarget = aCurrentSFWGH->target;
             sTarget != NULL;
             sTarget = sTarget->next )
        {
            sTarget->flag &= ~QMS_TARGET_ORDER_BY_MASK;
            sTarget->flag |= QMS_TARGET_ORDER_BY_FALSE;
        }
    }
    else
    {
        IDE_TEST( rollbackForWhere( aCurrentSFWGH,
                                    aUnderSFWGH,
                                    & sRollbackInfo )
                  != IDE_SUCCESS );
            
        IDE_TEST( rollbackForTargetList( aUnderFrom->tableRef,
                                         & sRollbackInfo )
                  != IDE_SUCCESS );
            
        IDE_TEST( rollbackForFrom( aCurrentSFWGH,
                                   aUnderSFWGH,
                                   & sRollbackInfo )
                  != IDE_SUCCESS );
            
        IDE_TEST( rollbackForHint( aCurrentSFWGH,
                                   & sRollbackInfo )
                  != IDE_SUCCESS );
    }
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::mergeForHint( qmsSFWGH            * aCurrentSFWGH,
                              qmsSFWGH            * aUnderSFWGH,
                              qmsFrom             * aUnderFrom,
                              qmoViewRollbackInfo * aRollbackInfo,
                              idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsJoinMethodHints   * sJoinMethodHint;
    qmsTableAccessHints  * sTableAccessHint;
    qmsHintTables        * sHintTable;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForHint::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    IDE_DASSERT( aIsMerged != NULL );

    //------------------------------------------
    // rollbackInfo �ʱ�ȭ
    //------------------------------------------

    aRollbackInfo->hintMerged = ID_FALSE;
    aRollbackInfo->lastJoinMethod = NULL;
    aRollbackInfo->lastTableAccess = NULL;
    
    //------------------------------------------
    // hint ���� rollback ���� ����
    //------------------------------------------

    // join method hint
    for ( sJoinMethodHint = aCurrentSFWGH->hints->joinMethod;
          sJoinMethodHint != NULL;
          sJoinMethodHint = sJoinMethodHint->next )
    {
        if ( sJoinMethodHint->next == NULL )
        {
            aRollbackInfo->lastJoinMethod = sJoinMethodHint;
        }
        else
        {
            // Nothing to do.
        }
    }

    // table access hint
    for ( sTableAccessHint = aCurrentSFWGH->hints->tableAccess;
          sTableAccessHint != NULL;
          sTableAccessHint = sTableAccessHint->next )
    {
        if ( sTableAccessHint->next == NULL )
        {
            aRollbackInfo->lastTableAccess = sTableAccessHint;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    //------------------------------------------
    // hint ���� merge ����
    //------------------------------------------

    // join method hint
    if ( aRollbackInfo->lastJoinMethod != NULL )
    {
        aRollbackInfo->lastJoinMethod->next =
            aUnderSFWGH->hints->joinMethod;
    }
    else
    {
        aCurrentSFWGH->hints->joinMethod =
            aUnderSFWGH->hints->joinMethod;
    }

    // PROJ-1718 Subquery unnesting
    // View�� ���Ե� relation�� 1���� ��� outer query���� ������ join method hint��
    // view merging ���Ŀ��� ��ȿ�ϵ��� �����Ѵ�.
    if( aUnderSFWGH->from->next == NULL )
    {
        for( sJoinMethodHint = aCurrentSFWGH->hints->joinMethod;
             sJoinMethodHint != NULL;
             sJoinMethodHint = sJoinMethodHint->next )
        {
            qtc::dependencyClear( &sJoinMethodHint->depInfo );

            // BUG-43923 NO_USE_SORT(a) ��Ʈ�� ����ϸ� FATAL�� �߻��մϴ�.
            // joinTables �� 1�� �̻��϶��� ó�������ϰ� �Ѵ�.
            for ( sHintTable = sJoinMethodHint->joinTables;
                  sHintTable != NULL;
                  sHintTable = sHintTable->next )
            {
                if ( sHintTable->table != NULL )
                {
                    if ( sHintTable->table == aUnderFrom )
                    {
                        sHintTable->table = aUnderSFWGH->from;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    qtc::dependencyOr( &sHintTable->table->depInfo,
                                       &sJoinMethodHint->depInfo,
                                       &sJoinMethodHint->depInfo );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }
    
    // table access hint
    if ( aRollbackInfo->lastTableAccess != NULL )
    {
        aRollbackInfo->lastTableAccess->next =
            aUnderSFWGH->hints->tableAccess;
    }
    else
    {
        aCurrentSFWGH->hints->tableAccess =
            aUnderSFWGH->hints->tableAccess;
    }

    // BUG-22236
    if ( aCurrentSFWGH->hints->interResultType
         == QMO_INTER_RESULT_TYPE_NOT_DEFINED )
    {
        // ���� query block�� �߰� ��� Ÿ�� Hint�� �־����� ���� ���,
        // view�� �߰� ��� Ÿ�� Hint�� �����Ѵ�.
        aRollbackInfo->interResultType = QMO_INTER_RESULT_TYPE_NOT_DEFINED;
        
        aCurrentSFWGH->hints->interResultType =
            aUnderSFWGH->hints->interResultType;
    }
    else
    {
        aRollbackInfo->interResultType = aCurrentSFWGH->hints->interResultType;
    }

    aRollbackInfo->hintMerged = ID_TRUE;

    // BUG-48419 for BUG-48336
    aCurrentSFWGH->lflag |= ( aUnderSFWGH->lflag & QMV_SFWGH_JOIN_MASK );

    *aIsMerged = ID_TRUE;

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::rollbackForHint( qmsSFWGH            * aCurrentSFWGH,
                                 qmoViewRollbackInfo * aRollbackInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoViewMerging::rollbackForHint::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );

    //------------------------------------------
    // hint ���� rollback ����
    //------------------------------------------

    if ( aRollbackInfo->hintMerged == ID_TRUE )
    {
        // join method hint
        if ( aRollbackInfo->lastJoinMethod != NULL )
        {
            aRollbackInfo->lastJoinMethod->next = NULL;
        }
        else
        {
            aCurrentSFWGH->hints->joinMethod = NULL;
        }
        
        // table access hint
        if ( aRollbackInfo->lastTableAccess != NULL )
        {
            aRollbackInfo->lastTableAccess->next = NULL;
        }
        else
        {
            aCurrentSFWGH->hints->tableAccess = NULL;
        }

        // BUG-22236
        // �߰� ��� ���� Ÿ�� hint
        if ( aRollbackInfo->interResultType ==
             QMO_INTER_RESULT_TYPE_NOT_DEFINED )
        {
            aCurrentSFWGH->hints->interResultType =
                QMO_INTER_RESULT_TYPE_NOT_DEFINED;
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::mergeForTargetList( qcStatement         * aStatement,
                                    qmsSFWGH            * aUnderSFWGH,
                                    qmsTableRef         * aUnderTableRef,
                                    qmoViewRollbackInfo * aRollbackInfo,
                                    idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsColumnRefList  * sColumnRef;
    qmsTarget         * sViewTarget;
    mtcColumn         * sViewColumn;
    UShort              sViewColumnOrder;
    UShort              sTargetOrder;
    idBool              sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForTargetList::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aUnderTableRef != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    IDE_DASSERT( aIsMerged != NULL );

    //------------------------------------------
    // rollbackInfo �ʱ�ȭ
    //------------------------------------------
    
    aRollbackInfo->targetMerged = ID_FALSE;

    //------------------------------------------
    // target list�� merge ����
    //------------------------------------------

    for ( sColumnRef = aUnderTableRef->viewColumnRefList;
          sColumnRef != NULL;
          sColumnRef = sColumnRef->next )
    {
        if ( sColumnRef->column->node.module == & qtc::passModule )
        {
            // view ���� �÷��̾��ٰ� passNode�� �ٲ���

            // Nothing to do.
        }
        else
        {
            if ( ( sColumnRef->column->lflag & QTC_NODE_MERGED_COLUMN_MASK )
                 == QTC_NODE_MERGED_COLUMN_TRUE )
            {
                // BUG-23467
                // case when ���� �ϳ��� ��带 �����ؼ� ����ϴ� �������� ���
                // �̹� merge�� �������� �� �� �ִ�.
                
                // Nothing to do.
            }
            else
            {
                IDE_DASSERT( sColumnRef->column->node.module == & qtc::columnModule );

                sViewColumn = QTC_STMT_COLUMN( aStatement, sColumnRef->column );
                sViewColumnOrder = sViewColumn->column.id & SMI_COLUMN_ID_MASK;
            
                sTargetOrder = 0;
                for ( sViewTarget = aUnderSFWGH->target;
                      sViewTarget != NULL;
                      sViewTarget = sViewTarget->next )
                {
                    if ( sTargetOrder == sViewColumnOrder )
                    {
                        break;
                    }
                    else
                    {
                        sTargetOrder++;
                    }
                }
            
                IDE_TEST_RAISE( sViewTarget == NULL, ERR_COLUMN_NOT_FOUND );

                if (sViewTarget->targetColumn->node.module == & qtc::columnModule)
                {
                    IDE_TEST( mergeForTargetColumn( aStatement,
                                                    sColumnRef,
                                                    sViewTarget->targetColumn,
                                                    & sIsMerged )
                              != IDE_SUCCESS );
                }
                else if ( sViewTarget->targetColumn->node.module == & qtc::valueModule )
                {
                    // view target�� ����� ���
                
                    IDE_TEST( mergeForTargetValue( aStatement,
                                                   sColumnRef,
                                                   sViewTarget->targetColumn,
                                                   & sIsMerged )
                              != IDE_SUCCESS );
                }
                else
                {
                    // view target�� expression�� ���
                
                    IDE_TEST( mergeForTargetExpression( aStatement,
                                                        aUnderSFWGH,
                                                        sColumnRef,
                                                        sViewTarget->targetColumn,
                                                        & sIsMerged )
                              != IDE_SUCCESS );
                }
            
                if ( sIsMerged == ID_FALSE )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }
        }
    }

    aRollbackInfo->targetMerged = ID_TRUE;
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoViewMerging::mergeForTargetList",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::mergeForTargetColumn( qcStatement         * aStatement,
                                      qmsColumnRefList    * aColumnRef,
                                      qtcNode             * aTargetColumn,
                                      idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     target�� ���� �÷��� �� merge�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcTuple  * sTargetTuple;
    mtcColumn * sTargetColumn;
    mtcColumn * sOrgColumn;
    idBool      sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForTargetColumn::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aColumnRef != NULL );
    IDE_DASSERT( aTargetColumn != NULL );
    IDE_DASSERT( aIsMerged != NULL );
    
    //------------------------------------------
    // target�� rollback ���� ����
    //------------------------------------------
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                             (void **) & aColumnRef->orgColumn )
              != IDE_SUCCESS );

    idlOS::memcpy( aColumnRef->orgColumn, aColumnRef->column,
                   ID_SIZEOF( qtcNode ) );

    //------------------------------------------
    // target�� merge ����
    //------------------------------------------

    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aColumnRef->column, aTargetColumn,
                   ID_SIZEOF( qtcNode ) );

    // To fix BUG-21405
    // ���� �÷��� ��� ���� estimate �� ���� �����Ƿ�
    // push projection���� flag�� set���� �ʴ´�.
    // �÷� ġȯ�� �� �� �÷��� flag�� ORing�Ѵ�.
    // To fix BUG-21425
    // disk table�� ���� flag�� ORing�Ѵ�.

    sTargetTuple  = QTC_STMT_TUPLE( aStatement, aColumnRef->column );
    sTargetColumn = QTC_TUPLE_COLUMN( sTargetTuple, aColumnRef->column );

    if( ( ( sTargetTuple->lflag & MTC_TUPLE_TYPE_MASK ) ==
          MTC_TUPLE_TYPE_TABLE ) &&
        ( ( sTargetTuple->lflag & MTC_TUPLE_STORAGE_MASK ) ==
          MTC_TUPLE_STORAGE_DISK ) )
    {
        sOrgColumn = QTC_STMT_COLUMN( aStatement, aColumnRef->orgColumn );

        sTargetColumn->flag |= sOrgColumn->flag;
    }
    else
    {
        // Nothing to do.
    }
        
    // conversion ��带 �ű��.
    aColumnRef->column->node.conversion = aColumnRef->orgColumn->node.conversion;
    aColumnRef->column->node.leftConversion = aColumnRef->orgColumn->node.leftConversion;

    // next�� �ű��.
    aColumnRef->column->node.next = aColumnRef->orgColumn->node.next;
    
    // name�� �����Ѵ�.
    SET_POSITION( aColumnRef->column->userName, aColumnRef->orgColumn->userName );
    SET_POSITION( aColumnRef->column->tableName, aColumnRef->orgColumn->tableName );
    SET_POSITION( aColumnRef->column->columnName, aColumnRef->orgColumn->columnName );

    // flag�� �����Ѵ�.
    aColumnRef->column->lflag &= ~QTC_NODE_MERGED_COLUMN_MASK;
    aColumnRef->column->lflag |= QTC_NODE_MERGED_COLUMN_TRUE;
        
    aColumnRef->isMerged = ID_TRUE;
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::mergeForTargetValue( qcStatement         * aStatement,
                                     qmsColumnRefList    * aColumnRef,
                                     qtcNode             * aTargetColumn,
                                     idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     target�� ����� �� merge�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool      sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForTargetValue::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aColumnRef != NULL );
    IDE_DASSERT( aTargetColumn != NULL );
    IDE_DASSERT( aIsMerged != NULL );
    
    //------------------------------------------
    // target�� rollback ���� ����
    //------------------------------------------
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                             (void **) & aColumnRef->orgColumn )
              != IDE_SUCCESS );

    idlOS::memcpy( aColumnRef->orgColumn, aColumnRef->column,
                   ID_SIZEOF( qtcNode ) );

    //------------------------------------------
    // target�� merge ����
    //------------------------------------------

    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aColumnRef->column, aTargetColumn,
                   ID_SIZEOF( qtcNode ) );
 
    // conversion ��带 �ű��.
    aColumnRef->column->node.conversion = aColumnRef->orgColumn->node.conversion;
    aColumnRef->column->node.leftConversion = aColumnRef->orgColumn->node.leftConversion;
    
    // next�� �ű��.
    aColumnRef->column->node.next = aColumnRef->orgColumn->node.next;
    
    // flag�� �����Ѵ�.
    aColumnRef->column->lflag &= ~QTC_NODE_MERGED_COLUMN_MASK;
    aColumnRef->column->lflag |= QTC_NODE_MERGED_COLUMN_TRUE;
    
    aColumnRef->isMerged = ID_TRUE;
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::mergeForTargetExpression( qcStatement         * aStatement,
                                          qmsSFWGH            * aUnderSFWGH,
                                          qmsColumnRefList    * aColumnRef,
                                          qtcNode             * aTargetColumn,
                                          idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     target�� expression�� �� merge�� �����Ѵ�.
 *
 * Implementation :
 *     (1) view �÷� ��带 ���� �����Ͽ� �����Ѵ�.
 *     (2) expr ��� ����� ������ template ������ �Ҵ��Ѵ�.
 *     (3) expr ��� Ʈ���� ���� �����Ѵ�.
 *     (4) ���� ������ expr�� �ֻ��� ����� ���� ������ �����Ѵ�.
 *     (5) view �÷��� conversion ��带 expr�� �ֻ��� ���� �ű��.
 *     (6) view �÷��� expr�� �ֻ��� ��带 ġȯ�Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sNode[2];
    qtcNode   * sNewNode;
    idBool      sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForTargetExpression::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aColumnRef != NULL );
    IDE_DASSERT( aTargetColumn != NULL );
    IDE_DASSERT( aIsMerged != NULL );
    
    //------------------------------------------
    // target�� rollback ���� ����
    //------------------------------------------
    
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                             (void **) & aColumnRef->orgColumn )
              != IDE_SUCCESS );

    idlOS::memcpy( aColumnRef->orgColumn, aColumnRef->column,
                   ID_SIZEOF( qtcNode ) );

    //------------------------------------------
    // target�� merge ����
    //------------------------------------------

    // expr�� ����� ������ template ������ �����Ѵ�.
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aTargetColumn->position,
                             (mtfModule*) aTargetColumn->node.module )
              != IDE_SUCCESS );

    // expr ��� Ʈ���� ���� �����Ѵ�.
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     (qtcNode*) aTargetColumn,
                                     & sNewNode,
                                     ID_FALSE,  // root�� next�� �������� �ʴ´�.
                                     ID_TRUE,   // conversion�� ���´�.
                                     ID_TRUE,   // constant node���� �����Ѵ�.
                                     ID_TRUE )  // constant node�� �����Ѵ�.
              != IDE_SUCCESS );

    // template ��ġ�� �����Ѵ�.
    sNewNode->node.table = sNode[0]->node.table;
    sNewNode->node.column = sNode[0]->node.column;

    // BUG-45187 view merge �ÿ� baseTable �����ؾ� �մϴ�.
    sNewNode->node.baseTable = sNode[0]->node.baseTable;
    sNewNode->node.baseColumn = sNode[0]->node.baseColumn;

    // estimate�� �����Ѵ�. (�ʱ�ȭ�Ѵ�.)
    IDE_TEST( qtc::estimate( sNewNode,
                             QC_SHARED_TMPLATE(aStatement),
                             aStatement,
                             NULL,
                             aUnderSFWGH,
                             NULL )
              != IDE_SUCCESS );

    // conversion ��带 �ű��.
    sNewNode->node.conversion = aColumnRef->column->node.conversion;
    sNewNode->node.leftConversion = aColumnRef->column->node.leftConversion;

    // next�� �ű��.
    sNewNode->node.next = aColumnRef->column->node.next;
    
    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aColumnRef->column, sNewNode,
                   ID_SIZEOF( qtcNode ) );

    // flag�� �����Ѵ�.
    aColumnRef->column->lflag &= ~QTC_NODE_MERGED_COLUMN_MASK;
    aColumnRef->column->lflag |= QTC_NODE_MERGED_COLUMN_TRUE;
    
    aColumnRef->isMerged = ID_TRUE;

    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::rollbackForTargetList( qmsTableRef         * aUnderTableRef,
                                       qmoViewRollbackInfo * aRollbackInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsColumnRefList  * sColumnRef;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::rollbackForTargetList::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aUnderTableRef != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    
    //------------------------------------------
    // target list�� rollback ����
    //------------------------------------------

    if ( aRollbackInfo->targetMerged == ID_TRUE )
    {
        for ( sColumnRef = aUnderTableRef->viewColumnRefList;
              sColumnRef != NULL;
              sColumnRef = sColumnRef->next )
        {
            if ( sColumnRef->isMerged == ID_TRUE )
            {
                idlOS::memcpy( sColumnRef->column, sColumnRef->orgColumn,
                               ID_SIZEOF( qtcNode ) );
                
                sColumnRef->isMerged = ID_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::mergeForFrom( qcStatement         * aStatement,
                              qmsSFWGH            * aCurrentSFWGH,
                              qmsSFWGH            * aUnderSFWGH,
                              qmsFrom             * aUnderFrom,
                              qmoViewRollbackInfo * aRollbackInfo,
                              idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom        * sFrom;
    qcNamePosition * sAliasName;
    UInt             sAliasCount = 0;
    idBool           sIsCreated = ID_TRUE;
    idBool           sIsMerged = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForFrom::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aUnderFrom != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    IDE_DASSERT( aIsMerged != NULL );

    //------------------------------------------
    // rollbackInfo �ʱ�ȭ
    //------------------------------------------

    aRollbackInfo->fromMerged = ID_FALSE;
    aRollbackInfo->firstFrom = NULL;
    aRollbackInfo->lastFrom = NULL;
    aRollbackInfo->oldAliasName = NULL;
    aRollbackInfo->newAliasName = NULL;
    
    //------------------------------------------
    // from ���� rollback ���� ����
    //------------------------------------------

    // currentSFWGH
    aRollbackInfo->firstFrom = aCurrentSFWGH->from;

    // underSFWGH
    for ( sFrom = aUnderSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        if ( sFrom->joinType == QMS_NO_JOIN )
        {
            sAliasCount++;
        }
        else
        {
            // Nothing to do.
        }
        
        if ( sFrom->next == NULL )
        {
            aRollbackInfo->lastFrom = sFrom;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_FT_ASSERT( aRollbackInfo->firstFrom != NULL );
    IDE_FT_ASSERT( aRollbackInfo->lastFrom != NULL );

    if ( sAliasCount > 0 )
    {
        // old alias name�� ����
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcNamePosition) * sAliasCount,
                                                 (void **) & aRollbackInfo->oldAliasName )
                  != IDE_SUCCESS );
        
        sAliasName = aRollbackInfo->oldAliasName;
        
        for ( sFrom = aUnderSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            if ( sFrom->joinType == QMS_NO_JOIN )
            {
                // tuple variable ����
                SET_POSITION( (*sAliasName), sFrom->tableRef->aliasName );
                
                sAliasName++;
            }
            else
            {
                // joined table�� tuple variable�� ���� �ʴ´�.
                
                // Nothing to do.
            }
        }
    }
    else
    {
        // from ���� joined table�θ� �̷���� ��� aliasCount�� 0�̴�.
        
        // Nothing to do.
    }

    //------------------------------------------
    // tuple variable ����
    //------------------------------------------

    if ( sAliasCount > 0 )
    {
        // new alias name�� ����
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcNamePosition) * sAliasCount,
                                                 (void **) & aRollbackInfo->newAliasName )
                  != IDE_SUCCESS );

        sAliasName = aRollbackInfo->newAliasName;
    
        // underSFWGH
        for ( sFrom = aUnderSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            if ( sFrom->joinType == QMS_NO_JOIN )
            {
                IDE_TEST( makeTupleVariable( aStatement,
                                             & aUnderFrom->tableRef->aliasName,
                                             & sFrom->tableRef->aliasName,
                                             sFrom->tableRef->isNewAliasName,
                                             sAliasName,
                                             & sIsCreated )
                          != IDE_SUCCESS );
                
                if ( sIsCreated == ID_FALSE )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
                
                sAliasName++;
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }
        
    //------------------------------------------
    // from ���� merge ����
    //------------------------------------------

    if ( sIsCreated == ID_TRUE )
    {
        if ( sAliasCount > 0 )
        {
            sAliasName = aRollbackInfo->newAliasName;

            // new alias�� �����Ѵ�.
            for ( sFrom = aUnderSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
            {
                if ( sFrom->joinType == QMS_NO_JOIN )
                {
                    SET_POSITION( sFrom->tableRef->aliasName, (*sAliasName) );
                    
                    // merge�� ���� ������ alias���� ����Ѵ�.
                    sFrom->tableRef->isNewAliasName = ID_TRUE;
                    
                    sAliasName++;
                }
                else
                {
                    // Nothing to do.
                }
            }

            // PROJ-1718 Subquery unnesting
            // View�� �����ִ� semi/anti join�� dependency ������ table�� ������ ����
            if( ( aUnderSFWGH->from->next == NULL ) &&
                ( qtc::haveDependencies( &aUnderFrom->semiAntiJoinDepInfo ) == ID_TRUE ) )
            {
                qtc::dependencyOr( &aUnderSFWGH->from->depInfo,
                                   &aUnderFrom->semiAntiJoinDepInfo,
                                   &aUnderSFWGH->from->semiAntiJoinDepInfo );

                qtc::dependencyRemove( aUnderFrom->tableRef->table,
                                       &aUnderSFWGH->from->semiAntiJoinDepInfo,
                                       &aUnderSFWGH->from->semiAntiJoinDepInfo );
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

        aRollbackInfo->lastFrom->next = aCurrentSFWGH->from;
        aCurrentSFWGH->from = aUnderSFWGH->from;
    }
    else
    {
        // merge�� �����ߴ�.
        sIsMerged = ID_FALSE;
    }

    aRollbackInfo->fromMerged = ID_TRUE;
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::makeTupleVariable( qcStatement    * aStatement,
                                   qcNamePosition * aViewName,
                                   qcNamePosition * aTableName,
                                   idBool           aIsNewTableName,
                                   qcNamePosition * aNewTableName,
                                   idBool         * aIsCreated )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     from���� merge�� ����� tuple variable�� �����Ѵ�.
 *
 *     [Enhancement]
 *     �󼼼���� 'SYS_ALIAS_n_'��� prefix�� ����Ϸ������� �ʹ� ���
 *     ������ �������� ������, ���� ª�� '$$n_'�� �����Ѵ�.
 *     �׸��� view�� table�� �̸� _�� ���� ���ǹǷ� �����ڷ�
 *     '_$'�� ����Ͽ� �� �� ���еǵ��� �����Ѵ�.
 *
 *     ex) $$1_$view_$table
 *         $$2_$view_v2_$view_v1_$table
 *
 * Implementation :
 *
 ***********************************************************************/

    qcNamePosition  * sViewName;
    qcNamePosition  * sTableName;
    qcNamePosition    sDefaultName;
    qcTemplate      * sTemplate;
    qcTupleVarList  * sVarList;
    SChar           * sNameBuffer;
    UInt              sNameLen;
    SChar           * sRealTableName;
    UInt              sRealTableNameLen;
    idBool            sIsCreated = ID_TRUE;
    idBool            sFound;
    UInt              i;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::makeTupleVariable::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aViewName != NULL );
    IDE_DASSERT( aTableName != NULL );
    IDE_DASSERT( aNewTableName != NULL );
    IDE_DASSERT( aIsCreated != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------

    sTemplate = QC_SHARED_TMPLATE(aStatement);
    
    // $$1_ ���� �����Ѵ�.
    if ( sTemplate->tupleVarGenNumber == 0 )
    {
        sTemplate->tupleVarGenNumber = 1;
    }
    else
    {
        // Nothing to do.
    }

    // default name�� �ʱ�ȭ
    sDefaultName.stmtText = DEFAULT_VIEW_NAME;
    sDefaultName.offset   = 0;
    sDefaultName.size     = DEFAULT_VIEW_NAME_LEN;
    
    // �̸� ���� view�� �̸��� default�� �ٲ۴�.
    if ( QC_IS_NULL_NAME( (*aViewName) ) == ID_TRUE )
    {
        sViewName = & sDefaultName;
    }
    else
    {
        sViewName = aViewName;
    }

    // �̸� ���� table�� �̸��� default�� �ٲ۴�.
    // (merge���� ���� �̸����� view�� ���)
    if ( QC_IS_NULL_NAME( (*aTableName) ) == ID_TRUE )
    {
        sTableName = & sDefaultName;
    }
    else
    {
        sTableName = aTableName;
    }

    //------------------------------------------
    // name buffer ����
    //------------------------------------------

    // name buffer length ����
    if ( aIsNewTableName == ID_FALSE )
    {
        // ���� merge�Ǵ� ���
        // ex) view V1�� T1 -> $$1_$V1_$T1
        //                     ~~~~ ~~  ~~
        sNameLen = QC_TUPLE_VAR_HEADER_SIZE +
            sViewName->size +
            sTableName->size +
            4 +   // 2���� '_$'
            10 +  // ������ ���� �ִ� 10
            1;    // '\0'
    }
    else
    {
        // �̹� �ѹ� merge�Ǿ� tule variable header�� ���� ���
        // ex) view V2�� $$1_$V1_$T1 -> $$2_$V2_$V1_$T1
        //                              ~~~~ ~~ ~~~~~~~
        sNameLen = sTableName->size +
            sViewName->size +
            2 +   // 1���� '_$'
            10 +  // ������ ���� �ִ� 10
            1;    // '\0'
    }
    
    // name buffer ����
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(SChar) * sNameLen,
                                             (void **) & sNameBuffer )
              != IDE_SUCCESS );

    //------------------------------------------
    // tuple variable ����
    //------------------------------------------
    
    for ( i = 0; i < MAKE_TUPLE_RETRY_COUNT; i++ )
    {
        idlOS::memset( sNameBuffer, 0x00, sNameLen );

        // $$1_$
        idlOS::snprintf( sNameBuffer, sNameLen, "%s%"ID_INT32_FMT"_$",
                         QC_TUPLE_VAR_HEADER,
                         sTemplate->tupleVarGenNumber );
        sNameLen = idlOS::strlen( sNameBuffer );
        
        // $$1_$V1
        idlOS::memcpy( sNameBuffer + sNameLen,
                       sViewName->stmtText + sViewName->offset,
                       sViewName->size );
        sNameLen += sViewName->size;
        
        // $$1_$V1_
        sNameBuffer[sNameLen] = '_';
        sNameLen += 1;
        
        if ( aIsNewTableName == ID_FALSE )
        {
            // $$1_$V1_$
            sNameBuffer[sNameLen] = '$';
            sNameLen += 1;
        
            // $$1_$V1_$T1
            idlOS::memcpy( sNameBuffer + sNameLen,
                           sTableName->stmtText + sTableName->offset,
                           sTableName->size );
            sNameLen += sTableName->size;
        }
        else
        {
            // �̹� merge�Ǿ� alias�� ����Ǿ����Ƿ�
            // $$1_$V1_$T1���� $V1_$T1�� �̾Ƴ���.
            sRealTableName = sTableName->stmtText + sTableName->offset;
            sRealTableNameLen = sTableName->size;
            
            sRealTableName += QC_TUPLE_VAR_HEADER_SIZE;
            sRealTableNameLen -= QC_TUPLE_VAR_HEADER_SIZE;
            
            while ( sRealTableNameLen > 0 )
            {
                if ( *sRealTableName == '_' )
                {
                    // '_'�� �����Ѵ�.
                    sRealTableName++;
                    sRealTableNameLen--;
                    
                    break;
                }
                else
                {
                    sRealTableName++;
                    sRealTableNameLen--;
                }
            }

            // $$2_$V2_ -> $$2_$V2_$V1_$T1
            idlOS::memcpy( sNameBuffer + sNameLen,
                           sRealTableName,
                           sRealTableNameLen );
            sNameLen += sRealTableNameLen;
        }
        
        sNameBuffer[sNameLen] = '\0';
        
        // generated number ����
        sTemplate->tupleVarGenNumber++;

        //------------------------------------------
        // �ߺ� tuple variable �˻�
        //------------------------------------------

        sFound = ID_FALSE;
        
        for ( sVarList = sTemplate->tupleVarList;
              sVarList != NULL;
              sVarList = sVarList->next )
        {
            // BUG-37032
            if ( idlOS::strMatch( sVarList->name.stmtText + sVarList->name.offset,
                                  sVarList->name.size,
                                  sNameBuffer,
                                  sNameLen ) == 0 )
            {
                sFound = ID_TRUE;
                break;
            }
        }

        if ( sFound == ID_FALSE )
        {
            break;
        }
        else
        {
            // ������ tuple variable ������ �浹���� �ʴ´�.
            
            // Nothing to do.
        }
    }

    if ( i == MAKE_TUPLE_RETRY_COUNT )
    {
        sIsCreated = ID_FALSE;
    }
    else
    {
        aNewTableName->stmtText = sNameBuffer;
        aNewTableName->offset   = 0;
        aNewTableName->size     = sNameLen;
    }

    *aIsCreated = sIsCreated;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::rollbackForFrom( qmsSFWGH            * aCurrentSFWGH,
                                 qmsSFWGH            * aUnderSFWGH,
                                 qmoViewRollbackInfo * aRollbackInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom        * sFrom;
    qcNamePosition * sAliasName;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::rollbackForFrom::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    
    //------------------------------------------
    // from ���� rollback ����
    //------------------------------------------

    if ( aRollbackInfo->fromMerged == ID_TRUE )
    {
        // tuple variable ����
        sAliasName = aRollbackInfo->oldAliasName;
        
        for ( sFrom = aUnderSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            if ( sFrom->joinType == QMS_NO_JOIN )
            {
                SET_POSITION( sFrom->tableRef->aliasName, (*sAliasName) );
                
                sAliasName++;
            }
            else
            {
                // Nothing to do.
            }
        }

        // from ���� ����
        aRollbackInfo->lastFrom->next = NULL;
        aCurrentSFWGH->from = aRollbackInfo->firstFrom;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::mergeForWhere( qcStatement         * aStatement,
                               qmsSFWGH            * aCurrentSFWGH,
                               qmsSFWGH            * aUnderSFWGH,
                               qmoViewRollbackInfo * aRollbackInfo,
                               idBool              * aIsMerged )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode         * sAndNode[2];
    qcNamePosition    sNullPosition;
    idBool            sIsMerged = ID_TRUE;
    mtcNode         * sNode = NULL;
    mtcNode         * sPrev = NULL;
    
    IDU_FIT_POINT_FATAL( "qmoViewMerging::mergeForWhere::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    IDE_DASSERT( aIsMerged != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------
    
    SET_EMPTY_POSITION( sNullPosition );
    
    //------------------------------------------
    // rollbackInfo �ʱ�ȭ
    //------------------------------------------
    
    aRollbackInfo->currentWhere = NULL;
    aRollbackInfo->underWhere = NULL;
    aRollbackInfo->whereMerged = ID_FALSE;
    
    //------------------------------------------
    // where ���� rollback ���� ����
    //------------------------------------------

    if ( aCurrentSFWGH->where != NULL )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                 (void **) & aRollbackInfo->currentWhere )
                  != IDE_SUCCESS );

        idlOS::memcpy( aRollbackInfo->currentWhere, aCurrentSFWGH->where,
                       ID_SIZEOF( qtcNode ) );
    }
    else
    {
        // Nothing to do.
    }
    
    if ( aUnderSFWGH->where != NULL )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                 (void **) & aRollbackInfo->underWhere )
                  != IDE_SUCCESS );

        idlOS::memcpy( aRollbackInfo->underWhere, aUnderSFWGH->where,
                       ID_SIZEOF( qtcNode ) );
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // where ���� merge ����
    //------------------------------------------

    if ( aCurrentSFWGH->where != NULL )
    {
        if ( aUnderSFWGH->where != NULL )
        {
            // ���� SFWGH�� where���� �ְ�
            // ���� SFWGH���� where ���� �ִ� ���

            // ���ο� AND ��带 �ϳ� �����Ѵ�.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sAndNode,
                                     & sNullPosition,
                                     (const UChar*)"AND",
                                     3 )
                      != IDE_SUCCESS );
            
            IDE_DASSERT( aCurrentSFWGH->where->node.next == NULL );
            
            // arguments�� �����Ѵ�.
            // BUG-43017
            if ( ( ( aCurrentSFWGH->where->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                   == MTC_NODE_LOGICAL_CONDITION_TRUE ) &&
                 ( ( aCurrentSFWGH->where->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_AND ) )
            {
                sAndNode[0]->node.arguments = aCurrentSFWGH->where->node.arguments;
            }
            else
            {
                sAndNode[0]->node.arguments = (mtcNode*) aCurrentSFWGH->where;
            }

            for ( sNode = sAndNode[0]->node.arguments;
                  sNode != NULL;
                  sPrev = sNode, sNode = sNode->next );

            if ( ( ( aUnderSFWGH->where->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                   == MTC_NODE_LOGICAL_CONDITION_TRUE ) &&
                 ( ( aUnderSFWGH->where->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_AND ) )
            {
                sPrev->next = aUnderSFWGH->where->node.arguments;
            }
            else
            {
                sPrev->next = (mtcNode*) aUnderSFWGH->where;
            }

            sAndNode[0]->node.lflag |= 2;

            // estimate�� �����Ѵ�.
            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        sAndNode[0] )
                      != IDE_SUCCESS );

            // where���� �����Ѵ�.
            aCurrentSFWGH->where = sAndNode[0];
        }
        else
        {
            // ���� SFWGH���� where���� ������
            // ���� SFWGH���� where���� ���� ���

            // Nothing to do.
        }

        /* BUG-42661 A function base index is not wokring view */
        if ( QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) == 1 )
        {
            IDE_TEST( qmsDefaultExpr::applyFunctionBasedIndex( aStatement,
                                                               aCurrentSFWGH->where,
                                                               aCurrentSFWGH->from,
                                                               &( aCurrentSFWGH->where ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        if ( aUnderSFWGH->where != NULL )
        {
            // ���� SFWGH���� where���� ������
            // ���� SFWGH���� where���� �ִ� ���

            // where���� �����Ѵ�.
            aCurrentSFWGH->where = aUnderSFWGH->where;
        }
        else
        {
            // ���� SFWGH���� where���� ����
            // ���� SFWGH���� where ���� ���� ���

            // Nothing to do.
        }
    }

    aRollbackInfo->whereMerged = ID_TRUE;
    
    *aIsMerged = sIsMerged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::rollbackForWhere( qmsSFWGH            * aCurrentSFWGH,
                                  qmsSFWGH            * aUnderSFWGH,
                                  qmoViewRollbackInfo * aRollbackInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoViewMerging::rollbackForWhere::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aCurrentSFWGH != NULL );
    IDE_DASSERT( aUnderSFWGH != NULL );
    IDE_DASSERT( aRollbackInfo != NULL );
    
    //------------------------------------------
    // where ���� rollback ����
    //------------------------------------------

    if ( aRollbackInfo->whereMerged == ID_TRUE )
    {
        // ���� where���� ����
        aCurrentSFWGH->where = aRollbackInfo->currentWhere;
        
        // ���� where���� ����
        aUnderSFWGH->where = aRollbackInfo->underWhere;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::removeMergedView( qmsSFWGH     * aSFWGH )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     merge�� view�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom  * sFirstFrom = NULL;
    qmsFrom  * sCurFrom;
    qmsFrom  * sFrom;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::removeMergedView::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aSFWGH != NULL );

    //------------------------------------------
    // merge�� view�� ����
    //------------------------------------------

    for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        if ( sFrom->joinType == QMS_NO_JOIN )
        {
            // merge���� ���� from�� �����Ѵ�.
            if ( sFrom->tableRef->isMerged == ID_FALSE )
            {
                sFirstFrom = sFrom;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // joined table�� merge����� �ƴϹǷ� �ٷ� �����Ѵ�.
            sFirstFrom = sFrom;
            break;
        }
    }

    IDE_FT_ASSERT( sFirstFrom != NULL );

    sCurFrom = sFirstFrom;

    for ( sFrom = sCurFrom->next; sFrom != NULL; sFrom = sFrom->next )
    {
        if ( sFrom->joinType == QMS_NO_JOIN )
        {
            // merge���� ���� from�� �����Ѵ�.
            if ( sFrom->tableRef->isMerged == ID_FALSE )
            {
                sCurFrom->next = sFrom;
                sCurFrom = sFrom;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // joined table�� merge����� �ƴϹǷ� �ٷ� �����Ѵ�.
            sCurFrom->next = sFrom;
            sCurFrom = sFrom;
        }
    }

    sCurFrom->next = NULL;
    
    aSFWGH->from = sFirstFrom;

    // BUG-45177 view merge ���Ŀ� ���� �ִ� view �� noMerge�ϰ� �Ѵ�.
    for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        if ( sFrom->joinType == QMS_NO_JOIN )
        {
            sFrom->tableRef->noMergeHint = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::validateQuerySet( qcStatement  * aStatement,
                                  qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ���� view�� merge�Ǿ� ���� query set�� validation�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateQuerySet::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // validation ����
    //------------------------------------------

    if ( aQuerySet->setOp == QMS_NONE )
    {
        IDE_TEST( validateSFWGH( aStatement,
                                 aQuerySet->SFWGH )
                  != IDE_SUCCESS );
        
        // set dependencies
        qtc::dependencySetWithDep( & aQuerySet->depInfo,
                                   & aQuerySet->SFWGH->depInfo );

        // set outer column dependencies
        IDE_TEST( qmvQTC::setOuterDependencies( aQuerySet->SFWGH,
                                                & aQuerySet->SFWGH->outerDepInfo )
                  != IDE_SUCCESS );
        
        qtc::dependencySetWithDep( & aQuerySet->outerDepInfo,
                                   & aQuerySet->SFWGH->outerDepInfo );

        // PROJ-2418
        // Lateral View�� outerDepInfo ����
        IDE_TEST( qmvQTC::setLateralDependencies( aQuerySet->SFWGH,
                                                  & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( checkViewDependency( aStatement,
                                       & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( validateQuerySet( aStatement,
                                    aQuerySet->left )
                  != IDE_SUCCESS );
        
        IDE_TEST( validateQuerySet( aStatement,
                                    aQuerySet->right )
                  != IDE_SUCCESS );
        
        // outer column dependency�� ���� dependency�� OR-ing�Ѵ�.
        qtc::dependencyClear( & aQuerySet->outerDepInfo );
        
        IDE_TEST( qtc::dependencyOr( & aQuerySet->left->outerDepInfo,
                                     & aQuerySet->outerDepInfo,
                                     & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );
        
        IDE_TEST( qtc::dependencyOr( & aQuerySet->right->outerDepInfo,
                                     & aQuerySet->outerDepInfo,
                                     & aQuerySet->outerDepInfo )
                  != IDE_SUCCESS );

        // PROJ-2418
        // lateral view�� dependency�� ���� dependency�� OR-ing�Ѵ�.
        IDE_TEST( qtc::dependencyOr( & aQuerySet->left->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & aQuerySet->right->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo,
                                     & aQuerySet->lateralDepInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::validateSFWGH( qcStatement  * aStatement,
                               qmsSFWGH     * aSFWGH )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ���� view�� merge�Ǿ� ���� SFWGH�� validation�� �����Ѵ�.
 *     ����, ���� SFWGH�� ���Ե� subquery�� �ܺ� ���� �÷��� ���� ��
 *     �����Ƿ� subquery���� validation�� �����Ѵ�.
 *
 * Implementation :
 *     view merge�� �����ϴ� validation�� �� clause�� expression Ȥ��
 *     predicate�� ��� Ʈ���� ��ȸ�ϸ�
 *     (1) ����� dependency ����, �ΰ� �������� ó���Ѵ�.
 *     (2) ���ŵ� view�� ���Ͽ� dependency ������ �����ִ��� �˻��Ѵ�.
 *
 ***********************************************************************/

    qmsTarget         * sTarget;
    qmsFrom           * sFrom;
    qmsConcatElement  * sElement;
    qmsConcatElement  * sSubElement;
    qmsAggNode        * sAggNode;
    qtcNode           * sList;
    qmsQuerySet       * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateSFWGH::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSFWGH != NULL );

    //---------------------------------------------------
    // validation of FROM clause
    //---------------------------------------------------

    qtc::dependencyClear( & aSFWGH->depInfo );
    
    for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        // PROJ-2418
        // View Merging ���� Lateral View�� �ٽ� Validation �ؾ� �Ѵ�
        IDE_TEST( validateFrom( sFrom ) != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( & sFrom->depInfo,
                                     & aSFWGH->depInfo,
                                     & aSFWGH->depInfo )
                  != IDE_SUCCESS );

        // PROJ-2415 Grouping Sets Clause
        // View�� Dependency ó�� �߰��� ���� ���� View�� Outer Dependency�� Or���ش�.
        if ( sFrom->tableRef != NULL )
        {
            if ( sFrom->tableRef->view != NULL )
            {
                sQuerySet = ( ( qmsParseTree* )( sFrom->tableRef->view->myPlan->parseTree ) )->querySet;
                
                IDE_TEST( qtc::dependencyOr( & sQuerySet->outerDepInfo,
                                             & aSFWGH->outerDepInfo,
                                             & aSFWGH->outerDepInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // Nohting to do.
        }
    }
    
    IDE_TEST( checkViewDependency( aStatement,
                                   & aSFWGH->depInfo )
              != IDE_SUCCESS );
    
    //---------------------------------------------------
    // validation of WHERE clause
    //---------------------------------------------------

    if ( aSFWGH->where != NULL )
    {
        IDE_TEST( validateNode( aStatement,
                                aSFWGH->where )
                  != IDE_SUCCESS );
        
        IDE_TEST( checkViewDependency( aStatement,
                                       & aSFWGH->where->depInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    
    //---------------------------------------------------
    // validation of GROUP clause
    //---------------------------------------------------

    for ( sElement = aSFWGH->group; sElement != NULL; sElement = sElement->next )
    {
        if ( sElement->type == QMS_GROUPBY_NORMAL )
        {
            IDE_TEST( validateNode( aStatement,
                                    sElement->arithmeticOrList )
                      != IDE_SUCCESS );

            IDE_TEST( checkViewDependency( aStatement,
                                           & sElement->arithmeticOrList->depInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            /* PROJ-1353 */
            for ( sSubElement = sElement->arguments;
                  sSubElement != NULL;
                  sSubElement = sSubElement->next )
            {
                if ( ( sSubElement->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    for ( sList = ( qtcNode * )sSubElement->arithmeticOrList->node.arguments;
                          sList != NULL;
                          sList = ( qtcNode * )sList->node.next )
                    {
                        IDE_TEST( validateNode( aStatement,
                                                sList )
                                  != IDE_SUCCESS );

                        IDE_TEST( checkViewDependency( aStatement,
                                                       &sList->depInfo )
                                  != IDE_SUCCESS );
                    }
                }
                else
                {
                    IDE_TEST( validateNode( aStatement,
                                            sSubElement->arithmeticOrList )
                              != IDE_SUCCESS );

                    IDE_TEST( checkViewDependency( aStatement,
                                                   &sSubElement->arithmeticOrList->depInfo )
                              != IDE_SUCCESS );
                }
            }
        }
    }

    //---------------------------------------------------
    // validation of target list
    //---------------------------------------------------

    for ( sTarget = aSFWGH->target; sTarget != NULL; sTarget = sTarget->next )
    {
        IDE_TEST( validateNode( aStatement,
                                sTarget->targetColumn )
                  != IDE_SUCCESS );
        
        IDE_TEST( checkViewDependency( aStatement,
                                       & sTarget->targetColumn->depInfo )
                  != IDE_SUCCESS );
    }

    //---------------------------------------------------
    // validation of HAVING clause
    //---------------------------------------------------
    
    if ( aSFWGH->having != NULL )
    {
        IDE_TEST( validateNode( aStatement,
                                aSFWGH->having )
                  != IDE_SUCCESS );
        
        IDE_TEST( checkViewDependency( aStatement,
                                       & aSFWGH->having->depInfo )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
 
    //---------------------------------------------------
    // validation of aggregate functions
    //---------------------------------------------------
    // PROJ-2179 Aggregate function�� ���ؼ��� validation�� �ٽ�
    // �������־�� ������ ���� SQL������ �������� �����Ѵ�.
    // SELECT /*+NO_PLAN_CACHE*/ MAX(c1) + MIN(c1) FROM (SELECT c1 FROM t1) ORDER BY MAX(c1) + MIN(c1);
    for( sAggNode = aSFWGH->aggsDepth1;
         sAggNode != NULL;
         sAggNode = sAggNode->next )
    {
        IDE_TEST( validateNode( aStatement,
                                sAggNode->aggr )
                  != IDE_SUCCESS );
        
        IDE_TEST( checkViewDependency( aStatement,
                                       & sAggNode->aggr->depInfo )
                  != IDE_SUCCESS );
    }

    for( sAggNode = aSFWGH->aggsDepth2;
         sAggNode != NULL;
         sAggNode = sAggNode->next )
    {
        IDE_TEST( validateNode( aStatement,
                                sAggNode->aggr )
                  != IDE_SUCCESS );
        
        IDE_TEST( checkViewDependency( aStatement,
                                       & sAggNode->aggr->depInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::validateOrderBy( qcStatement  * aStatement,
                                 qmsParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     order by ���� validation�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsSortColumns  * sSortColumn;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateOrderBy::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParseTree != NULL );

    //------------------------------------------
    // validation ����
    //------------------------------------------

    if ( aParseTree->orderBy != NULL )
    {
        for ( sSortColumn = aParseTree->orderBy;
              sSortColumn != NULL;
              sSortColumn = sSortColumn->next )
        {
            IDE_TEST( validateNode( aStatement,
                                    sSortColumn->sortColumn )
                      != IDE_SUCCESS );
            
            IDE_TEST( checkViewDependency( aStatement,
                                           & sSortColumn->sortColumn->depInfo )
                      != IDE_SUCCESS );
        }

        IDE_TEST( qmvOrderBy::disconnectConstantNode( aParseTree )
                  != IDE_SUCCESS );
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
qmoViewMerging::validateNode( qcStatement  * aStatement,
                              qtcNode      * aNode )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     ����� validation�� �����Ѵ�.
 *
 * Implementation :
 *     (1) dependency ������ �缳���Ѵ�.
 *     (2) �ΰ� ������ �����Ѵ�.
 *
 ***********************************************************************/

    qcStatement       * sStatement;
    qmsParseTree      * sParseTree;
    qtcNode           * sNode;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateNode::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    
    //------------------------------------------
    // dependency ���� �缳��
    //------------------------------------------
    
    if ( aNode->node.module == & qtc::subqueryModule )
    {
        // subquery ����� ���
        sStatement = ((qtcNode*) aNode)->subquery;
        sParseTree = (qmsParseTree*) sStatement->myPlan->parseTree;

        // outer dependency�� �ִ� subquery�� ���Ͽ� validation�� �����Ѵ�.
        if ( qtc::haveDependencies( & sParseTree->querySet->outerDepInfo ) == ID_TRUE )
        {
            IDE_TEST( validateQuerySet( aStatement,
                                        sParseTree->querySet )
                      != IDE_SUCCESS );
        
            // dependency ���� �ʱ�ȭ
            qtc::dependencySetWithDep( & aNode->depInfo,
                                       & sParseTree->querySet->outerDepInfo );
        }
        else
        {
            // dependency ���� �ʱ�ȭ
            qtc::dependencyClear( & aNode->depInfo );
        }
    }
    else if ( aNode->node.module == & qtc::passModule )
    {
        // pass ����� ���
        sNode = (qtcNode*) aNode->node.arguments;
        
        // dependency ���� ����
        qtc::dependencySetWithDep( & aNode->depInfo,
                                   & sNode->depInfo );

        // flag ���� ����
        aNode->node.lflag |= sNode->node.lflag & MTC_NODE_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

        // variable built-in function ���� ����
        aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_VAR_FUNCTION_MASK;
        
        // Lob or Binary Type ���� ����
        aNode->lflag &= ~QTC_NODE_BINARY_MASK;
        aNode->lflag |= sNode->lflag & QTC_NODE_BINARY_MASK;
    }
    else
    {
        if ( ( QTC_IS_TERMINAL( aNode ) == ID_TRUE )         // (1)
             ||
             ( ( aNode->lflag & QTC_NODE_CONVERSION_MASK )    // (2)
               == QTC_NODE_CONVERSION_TRUE )
             )
        {
            //------------------------------------------------------
            // (1) ���� ����̰ų�
            // (2) �̹� ���ȭ�� ������ ����̰ų�
            //
            // ��� ���� ����� �� �� �ִ�. ���� ��忡 ���ؼ���
            // �̹� ������ dependency ������ flag ������ �̿��ϹǷ�
            // validation�� ������ �ʿ䰡 ����.
            //------------------------------------------------------
            
            //------------------------------------------------------
            // BUG-30115
            // ���� ����, �� ��尡 analytic function�� ���,
            // over���� ���� dependency ������ dependency�� �缳�� �ؾ���
            // ex) �Ʒ��� ���� ���ǰ� view merging �ɶ�
            //     SELECT COUNT(*) OVER ( PARTITION BY v1.i1 )
            //     FROM ( SELECT i1 FROM t1 )v1
            //     -> view merging 
            //     SELECT COUNT(*) OVER ( PARTITION BY t1.i1 )
            //     FROM t1
            //------------------------------------------------------

            if( aNode->overClause != NULL )
            {
                // anayltic function ����� dependency ���� �ʱ�ȭ
                qtc::dependencyClear( & aNode->depInfo );

                // over���� dependency ���� �缳��
                IDE_TEST( validateNode4OverClause( aStatement,
                                                   aNode )
                          != IDE_SUCCESS );
                
                // analytic function�� ������ ����
                aNode->lflag &= ~QTC_NODE_ANAL_FUNC_MASK;
                aNode->lflag |= QTC_NODE_ANAL_FUNC_EXIST;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // �߰� ����� dependency ���� �ʱ�ȭ
            qtc::dependencyClear( & aNode->depInfo );

            //------------------------------------------
            // validation ����
            //------------------------------------------
        
            for( sNode  = (qtcNode*) aNode->node.arguments;
                 sNode != NULL;
                 sNode  = (qtcNode*) sNode->node.next )
            {
                IDE_TEST( validateNode( aStatement, sNode )
                          != IDE_SUCCESS );
                
                //------------------------------------------------------
                // Argument�� ���� �� �ʿ��� ������ ��� �����Ѵ�.
                //    [Index ��� ���� ����]
                //     aNode->module->mask : ���� Node�� column�� ���� ���,
                //     ���� ����� flag�� index�� ����� �� ������ Setting�Ǿ� ����.
                //     �� ��, ������ ����� Ư���� �ǹ��ϴ� mask�� �̿��� flag��
                //     ����������μ� index�� Ż �� ������ ǥ���� �� �ִ�.
                //------------------------------------------------------
            
                aNode->node.lflag |=
                    sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
                aNode->lflag |= sNode->lflag & QTC_NODE_MASK;
            
                // Argument�� dependencies�� ��� �����Ѵ�.
                IDE_TEST( qtc::dependencyOr( & aNode->depInfo,
                                             & sNode->depInfo,
                                             & aNode->depInfo )
                          != IDE_SUCCESS );
            }
            
            //------------------------------------------------------
            // BUG-27526
            // over���� ���� validation�� �����Ѵ�.
            //------------------------------------------------------
            
            if( aNode->overClause != NULL )
            {
                IDE_TEST( validateNode4OverClause( aStatement,
                                                   aNode )
                          != IDE_SUCCESS );
                
                // BUG-27457
                // analytic function�� ������ ����
                aNode->lflag &= ~QTC_NODE_ANAL_FUNC_MASK;
                aNode->lflag |= QTC_NODE_ANAL_FUNC_EXIST;
            }
            else
            {
                // Nothing to do.
            }
            
            //------------------------------------------------------
            // BUG-16000
            // Column�̳� Function�� Type�� Lob or Binary Type�̸� flag����
            //------------------------------------------------------
            
            aNode->lflag &= ~QTC_NODE_BINARY_MASK;
            
            if ( qtc::isEquiValidType( aNode, & QC_SHARED_TMPLATE(aStatement)->tmplate )
                 == ID_FALSE )
            {
                aNode->lflag |= QTC_NODE_BINARY_EXIST;
            }
            else
            {
                // Nothing to do.
            }

            //------------------------------------------------------
            // PROJ-1404
            // variable built-in function�� ����� ��� �����Ѵ�.
            //------------------------------------------------------
            
            if ( ( aNode->node.lflag & MTC_NODE_VARIABLE_MASK )
                 == MTC_NODE_VARIABLE_TRUE )
            {
                aNode->lflag &= ~QTC_NODE_VAR_FUNCTION_MASK;
                aNode->lflag |= QTC_NODE_VAR_FUNCTION_EXIST;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::validateNode4OverClause( qcStatement  * aStatement,
                                         qtcNode      * aNode )
{
/***********************************************************************
 *
 * Description : BUG-27526
 *     over�� ����� validation�� �����Ѵ�.
 *
 * Implementation :
 *     (1) dependency ������ �缳���Ѵ�.
 *     (2) �ΰ� ������ �����Ѵ�.
 *
 ***********************************************************************/

    qtcOverColumn   * sCurOverColumn;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateNode4OverClause::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    
    //------------------------------------------
    // dependency ���� �缳��
    //------------------------------------------
    
    // Partition By column �鿡 ���� estimate
    for ( sCurOverColumn = aNode->overClause->overColumn;
          sCurOverColumn != NULL;
          sCurOverColumn = sCurOverColumn->next )
    {
        IDE_TEST( validateNode( aStatement,
                                sCurOverColumn->node )
                  != IDE_SUCCESS );
        
        // partition by column�� dependencies�� ��� �����Ѵ�.
        IDE_TEST( qtc::dependencyOr( & aNode->depInfo,
                                     & sCurOverColumn->node->depInfo,
                                     & aNode->depInfo )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoViewMerging::checkViewDependency( qcStatement  * aStatement,
                                     qcDepInfo    * aDepInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     merge�� ���ŵ� view�� dependency�� �����ִ��� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcTableMap   * sTableMap;
    qmsFrom      * sFrom;
    SInt           sTable;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::checkViewDependency::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDepInfo != NULL );

    //------------------------------------------
    // �ʱ�ȭ
    //------------------------------------------
    
    sTableMap = QC_SHARED_TMPLATE(aStatement)->tableMap;
    
    //------------------------------------------
    // dependency �˻�
    //------------------------------------------

    sTable = qtc::getPosFirstBitSet( aDepInfo );
    
    while ( sTable != QTC_DEPENDENCIES_END )
    {
        if ( sTableMap[sTable].from != NULL )
        {
            sFrom = sTableMap[sTable].from;
            
            // ���⼭ �����ϰ� �˻����� ������
            // optimize�ó� Ȥ�� execution�� �м��ϱ� ����
            // ������ �߻��ϰ� �ȴ�.
            IDE_FT_ASSERT( sFrom->tableRef->isMerged!= ID_TRUE );
        }
        else
        {
            // Nothing to do.
        }

        sTable = qtc::getPosNextBitSet( aDepInfo, sTable );
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoViewMerging::modifySameViewRef( qcStatement  * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1413 Simple View Merging
 *     merge�� view�� ���� ���� view reference�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcTableMap   * sTableMap;
    qmsFrom      * sFrom;
    qmsTableRef  * sTableRef;
    UShort         i;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::modifySameViewRef::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    
    //------------------------------------------
    // tableMap ȹ��
    //------------------------------------------

    sTableMap = QC_SHARED_TMPLATE(aStatement)->tableMap;
    
    //------------------------------------------
    // same view reference ����
    //------------------------------------------

    for ( i = 0; i < QC_SHARED_TMPLATE(aStatement)->tmplate.rowCount; i++ )
    {
        if ( sTableMap[i].from != NULL )
        {
            sFrom = sTableMap[i].from;
            
            if ( sFrom->tableRef->sameViewRef != NULL )
            {
                sTableRef = sFrom->tableRef->sameViewRef;

                if ( sTableRef->isMerged == ID_TRUE )
                {
                    // sameViewRef�� merge�Ǿ��ٸ� NULL�� �ٲ۴�.
                    sFrom->tableRef->sameViewRef = NULL;
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
    }

    return IDE_SUCCESS;
}

IDE_RC qmoViewMerging::validateFrom( qmsFrom * aFrom )
{
/************************************************************************
 *
 *  Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 *
 *  Merge�� Lateral View�� �ܺ� �����ϴ� �ٸ� Lateral View�� ���� �� �����Ƿ�
 *  Lateral View�� ������ querySet�� �ٽ� Validation �Ѵ�.
 *
 *  Shard View �� �ܺ� ���� Push Predicate �� ����� �� �����Ƿ� �ٽ� Validation �Ѵ�.
 *
 *  Merge�� View�� �����ϴ� Subquery�� �ٽ� Validation �ϴ� �Ͱ� ���� �����̴�.
 *
 ************************************************************************/

    qcStatement  * sStatement;
    qmsParseTree * sParseTree;

    IDU_FIT_POINT_FATAL( "qmoViewMerging::validateFrom::__FT__" );

    if ( aFrom->joinType == QMS_NO_JOIN )
    {
        if ( aFrom->tableRef != NULL )
        {
            if ( aFrom->tableRef->view != NULL )
            {
                sStatement = aFrom->tableRef->view;

                if ( ( aFrom->tableRef->flag & QMS_TABLE_REF_LATERAL_VIEW_MASK )
                     == QMS_TABLE_REF_LATERAL_VIEW_TRUE )
                {
                    sParseTree = (qmsParseTree *) sStatement->myPlan->parseTree;

                    // Lateral View�� �ٽ� Validation�� �ʿ��ϴ�.
                    IDE_TEST( validateQuerySet( sStatement,
                                                sParseTree->querySet )
                              != IDE_SUCCESS );

                    // View QuerySet�� outerDepInfo�� �����ϴ� ��쿡��
                    // lateralDepInfo�� outerDepInfo�� ORing �Ѵ�.
                    IDE_TEST( qmvQTC::setLateralDependenciesLast( sParseTree->querySet )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* BUG-48880 */
                    if ( ( sStatement->mFlag & QC_STMT_SHARD_OBJ_MASK )
                         == QC_STMT_SHARD_OBJ_EXIST )
                    {
                        sParseTree = (qmsParseTree *) sStatement->myPlan->parseTree;
                        
                        /* Shard View�� �ܺ� ���� Push Predicate �� ����� �� �ְ�,
                         * �ٽ� Validation �� �ʿ��ϴ�.
                         */
                        IDE_TEST( validateQuerySet( sStatement,
                                                    sParseTree->querySet )
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
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        // JOIN�� ���, LEFT/RIGHT�� ���� ���� ȣ���Ѵ�.
        IDE_TEST( validateFrom( aFrom->left  ) != IDE_SUCCESS );
        IDE_TEST( validateFrom( aFrom->right ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoViewMerging::doTransformForMultiDML( qcStatement  * aStatement,
                                               qmsQuerySet  * aQuerySet )
{
    idBool sIsTransformed;

    //------------------------------------------
    // Simple View Merging ����
    //------------------------------------------
    if ( QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE == 0 )
    {
        IDE_TEST( processTransformForQuerySet( aStatement,
                                               aQuerySet,
                                               & sIsTransformed )
                  != IDE_SUCCESS );
        // merge�� ���� view reference�� �����Ѵ�.
        IDE_TEST( modifySameViewRef( aStatement )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_SIMPLE_VIEW_MERGE_DISABLE );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
