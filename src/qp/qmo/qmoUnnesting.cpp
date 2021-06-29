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
 * $Id$
 **********************************************************************/

#include <idl.h>
#include <qmv.h>
#include <qmvQTC.h>
#include <qcg.h>
#include <qcgPlan.h>
#include <qtc.h>
#include <qcuProperty.h>
#include <qmoUnnesting.h>
#include <qmoViewMerging.h>
#include <qmoNormalForm.h>
#include <qmvQuerySet.h>
#include <qtcCache.h>
#include <sdi.h> /* TASK-7219 Shard Transformer Refactoring */

// Subquery unnesting �� �����Ǵ� view�� �̸�
/* BUG-48052 */
#define VIEW_NAME_PREFIX        "$VIEW"
#define VIEW_NAME_LENGTH        11

// Subquery unnesting �� �����Ǵ� view�� column �̸�
#define COLUMN_NAME_PREFIX      "COL"
#define COLUMN_NAME_LENGTH      8

extern mtfModule mtfAnd;
extern mtfModule mtfOr;

extern mtfModule mtfExists;
extern mtfModule mtfNotExists;
extern mtfModule mtfUnique;
extern mtfModule mtfNotUnique;

extern mtfModule mtfList;

extern mtfModule mtfEqual;
extern mtfModule mtfNotEqual;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;

extern mtfModule mtfEqualAny;
extern mtfModule mtfNotEqualAny;
extern mtfModule mtfGreaterThanAny;
extern mtfModule mtfGreaterEqualAny;
extern mtfModule mtfLessThanAny;
extern mtfModule mtfLessEqualAny;

extern mtfModule mtfEqualAll;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfGreaterThanAll;
extern mtfModule mtfGreaterEqualAll;
extern mtfModule mtfLessThanAll;
extern mtfModule mtfLessEqualAll;

extern mtfModule mtfLnnvl;
extern mtfModule mtfCount;
extern mtfModule mtfCountKeep;

extern mtfModule mtfCase2;
extern mtfModule mtfIsNotNull;

extern mtfModule mtfGetBlobLocator;
extern mtfModule mtfGetClobLocator;

IDE_RC
qmoUnnesting::doTransform( qcStatement  * aStatement,
                           idBool       * aChanged )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1718 Subquery Unnesting ���
 *     SQL������ ���Ե� subquery���� ��� ã�� unnesting �õ��Ѵ�.
 *
 * Implementation :
 *     Unnesting�Ǿ� view merging�� ����Ǿ�� �ϴ� ��� aChange��
 *     ���� true�� ��ȯ�ȴ�.
 *
 ***********************************************************************/

    qmsParseTree * sParseTree = NULL; /* TASK-7219 */
    idBool         sChanged = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::doTransform::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------
    // Subqury Unnesting ����
    //------------------------------------------

    if ( QCU_OPTIMIZER_UNNEST_SUBQUERY == 0 )
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_SUBQUERY_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_SUBQUERY_FALSE;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_SUBQUERY_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_SUBQUERY_TRUE;
    }

    /* BUG-46544 unnest hit */
    if ( (QCU_OPTIMIZER_UNNEST_COMPATIBILITY & QCU_UNNEST_COMPATIBILITY_MASK_MODE1)
         == QCU_UNNEST_COMPATIBILITY_MASK_MODE1 )
    {
        // ������ unnest�� ��� hint���� ������Ƽ �� �켱�̿��� ȣȯ���� ����
        // ������Ƽ�� �켱�ϵ��� �Ѵ�.
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_COMPATIBILITY_1_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_COMPATIBILITY_1_TRUE;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_COMPATIBILITY_1_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_COMPATIBILITY_1_FALSE;
    }

    /* TASK-7219 ���������� ������ Shard View���� Unnesting�� �������� �ʴ´�. */
    sParseTree = (qmsParseTree*)aStatement->myPlan->parseTree;

    IDE_TEST_CONT( ( ( sParseTree->common.stmtShard != QC_STMT_SHARD_NONE )
                     &&
                     ( sParseTree->common.stmtShard != QC_STMT_SHARD_META ) ),
                   NORMAL_EXIT );

    // BUG-43059 Target subquery unnest/removal disable
    if ( ( sParseTree->querySet->lflag & QMV_QUERYSET_TARGET_SUBQUERY_UNNEST_MASK )
         == QMV_QUERYSET_TARGET_SUBQUERY_UNNEST_TRUE )
    {
        IDE_TEST( doTransformQuerySet( aStatement,
                                       sParseTree->querySet,
                                       & sChanged )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* TASK-7219 */
    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_UNNEST_SUBQUERY );

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPATIBILITY );

    *aChanged = sChanged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::doTransformSubqueries( qcStatement * aStatement,
                                     qtcNode     * aPredicate,
                                     idBool      * aChanged )
{
/***********************************************************************
 *
 * Description :
 *     �־��� predicate���� ���Ե� subquery���� ��� ã�� unnesting
 *     �õ��Ѵ�.
 *     SELECT �� ������ ���Ե� predicate���� ���� ����Ѵ�.
 *
 * Implementation :
 *     Unnesting�Ǿ� view merging�� ����Ǿ�� �ϴ� ��� aChange��
 *     ���� true�� ��ȯ�ȴ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoUnnesting::doTransformSubqueries::__FT__" );

    if ( QCU_OPTIMIZER_UNNEST_SUBQUERY == 0 )
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_SUBQUERY_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_SUBQUERY_FALSE;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_SUBQUERY_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_SUBQUERY_TRUE;
    }

    /* BUG-46544 unnest hit */
    if ( (QCU_OPTIMIZER_UNNEST_COMPATIBILITY & QCU_UNNEST_COMPATIBILITY_MASK_MODE1) 
         == QCU_UNNEST_COMPATIBILITY_MASK_MODE1 )
    {
        // ������ unnest�� ��� hint���� ������Ƽ �� �켱�̿��� ȣȯ���� ����
        // ������Ƽ�� �켱�ϵ��� �Ѵ�.
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_COMPATIBILITY_1_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_COMPATIBILITY_1_TRUE;
    }
    else
    {
        QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_COMPATIBILITY_1_MASK;
        QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_COMPATIBILITY_1_FALSE;
    }

    IDE_TEST( findAndUnnestSubquery( aStatement,
                                     NULL,
                                     ID_FALSE,
                                     aPredicate,
                                     aChanged )
              != IDE_SUCCESS );

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_UNNEST_SUBQUERY );

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPATIBILITY );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::doTransformQuerySet( qcStatement * aStatement,
                                   qmsQuerySet * aQuerySet,
                                   idBool      * aChanged )
{
/***********************************************************************
 *
 * Description :
 *     Query-set�� ���Ե� subquery���� ã�� unnesting �õ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom * sFrom;
    idBool    sUnnestSubquery;
    idBool    sChanged = ID_FALSE;
    idBool    sRemoved;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::doTransformQuerySet::__FT__" );

    if( aQuerySet->setOp == QMS_NONE )
    {
        if( aStatement->myPlan->parseTree->stmtKind == QCI_STMT_SELECT )
        {
            if( aQuerySet->SFWGH->hints->joinOrderType == QMO_JOIN_ORDER_TYPE_ORDERED )
            {
                sUnnestSubquery = ID_FALSE;
            }
            else
            {
                if( ( aQuerySet->SFWGH->hierarchy == NULL ) &&
                    ( QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement ) > 0 ) )
                {
                    sUnnestSubquery = ID_TRUE;
                }
                else
                {
                    sUnnestSubquery = ID_FALSE;
                }
            }
        }
        else
        {
            sUnnestSubquery = ID_FALSE;
        }

        if ( sUnnestSubquery == ID_TRUE )
        {
            for( sFrom = aQuerySet->SFWGH->from;
                 sFrom != NULL;
                 sFrom = sFrom->next )
            {
                IDE_TEST( doTransformFrom( aStatement,
                                           aQuerySet->SFWGH,
                                           sFrom,
                                           aChanged )
                          != IDE_SUCCESS );
            }

            if( ( aQuerySet->SFWGH->hierarchy == NULL ) &&
                ( aQuerySet->SFWGH->rownum    == NULL ) )
            {
                IDE_TEST( findAndRemoveSubquery( aStatement,
                                                 aQuerySet->SFWGH,
                                                 aQuerySet->SFWGH->where,
                                                 &sRemoved )
                          != IDE_SUCCESS );

                // BUG-38288 RemoveSubquery �� �ϰ� �Ǹ� target �� ����ȴ�
                // �̸� ���� QuerySet ���� �˾ƾ� �Ѵ�.
                if( sRemoved == ID_TRUE )
                {
                    *aChanged = ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // Hierarchy query ��� �� subquery removal �������
            }

            // SET ������ �ƴ� ��� WHERE���� subquery���� unnesting�� �õ� �Ѵ�.
            IDE_TEST( findAndUnnestSubquery( aStatement,
                                             aQuerySet->SFWGH,
                                             sUnnestSubquery,
                                             aQuerySet->SFWGH->where,
                                             &sChanged )
                      != IDE_SUCCESS );

            if( sChanged == ID_TRUE )
            {
                IDE_TEST( qmoViewMerging::validateNode( aStatement,
                                                        aQuerySet->SFWGH->where )
                          != IDE_SUCCESS );

                *aChanged = ID_TRUE;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        // SET ������ ��� �� query block �� transformation�� �õ� �Ѵ�.
        IDE_TEST( doTransformQuerySet( aStatement,
                                       aQuerySet->left,
                                       aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformQuerySet( aStatement,
                                       aQuerySet->right,
                                       aChanged )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoUnnesting::doTransformFrom( qcStatement * aStatement,
                               qmsSFWGH    * aSFWGH,
                               qmsFrom     * aFrom,
                               idBool      * aChanged )
{
/***********************************************************************
 *
 * Description :
 *     FROM���� relation�� view�� ��� view���� subquery�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoUnnesting::doTransformFrom::__FT__" );

    if( aFrom->joinType == QMS_NO_JOIN )
    {
        if( aFrom->tableRef->view != NULL )
        {
            IDE_TEST( doTransform( aFrom->tableRef->view,
                                   aChanged )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        IDE_TEST( findAndUnnestSubquery( aStatement,
                                         NULL,
                                         ID_FALSE,
                                         aFrom->onCondition,
                                         aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->left,
                                   aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->right,
                                   aChanged )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::findAndUnnestSubquery( qcStatement * aStatement,
                                     qmsSFWGH    * aSFWGH,
                                     idBool        aUnnestSubquery,
                                     qtcNode     * aPredicate,
                                     idBool      * aChanged )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� ���Ե� subquery�� ã�� unnesting�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode       * sNode;
    idBool          sChanged = ID_FALSE;
    idBool          sUnnestSubquery = aUnnestSubquery;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::findAndUnnestSubquery::__FT__" );

    if( aPredicate != NULL )
    {
        if( ( aPredicate->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
        {
            if( isSubqueryPredicate( aPredicate ) == ID_TRUE )
            {
                // EXISTS/NOT EXISTS�� ��ȯ �õ�
                if( isExistsTransformable( aStatement, aSFWGH, aPredicate, aUnnestSubquery ) == ID_TRUE )
                {
                    IDE_TEST( transformToExists( aStatement, aPredicate )
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

            if( ( aPredicate->node.module == &mtfExists ) ||
                ( aPredicate->node.module == &mtfNotExists ) ||
                ( aPredicate->node.module == &mtfUnique ) ||
                ( aPredicate->node.module == &mtfNotUnique ) )
            {
                // EXISTS/NOT EXISTS/UNIQUE/NOT UNIQUE�� ��� SELECT���� �ܼ��ϰ� ����
                IDE_TEST( setDummySelect( aStatement, aPredicate, ID_TRUE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }

            if( ( aUnnestSubquery == ID_TRUE ) &&
                ( ( aPredicate->node.module == &mtfExists ) ||
                  ( aPredicate->node.module == &mtfNotExists ) ) )
            {
                // Subquery�� ���Ե� subquery�鿡 ���� ���� unnesting �õ�
                IDE_TEST( doTransform( ((qtcNode *)aPredicate->node.arguments)->subquery,
                                       aChanged )
                          != IDE_SUCCESS );

                if( isSimpleSubquery( ((qtcNode *)aPredicate->node.arguments)->subquery ) == ID_TRUE )
                {
                    // Nothing to do.
                }
                else
                {
                    // �� �� cost-based query transformation���� �����Ǿ�� ��
                    if( QCU_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY == 0 )
                    {
                        sUnnestSubquery = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    qcgPlan::registerPlanProperty( aStatement,
                                                   PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY );
                }

                if( ( sUnnestSubquery == ID_TRUE ) &&
                    ( isUnnestableSubquery( aStatement, aSFWGH, aPredicate ) == ID_TRUE ) )
                {
                    // Unnesting �õ�
                    IDE_TEST( unnestSubquery( aStatement,
                                              aSFWGH,
                                              aPredicate )
                              != IDE_SUCCESS );

                    *aChanged = ID_TRUE;
                }
                else
                {
                    // Unnesting �Ұ����� subquery
                }
            }
            else
            {
                if( aPredicate->node.module == &qtc::subqueryModule )
                {
                    IDE_TEST( doTransform( aPredicate->subquery, &sChanged )
                              != IDE_SUCCESS );

                    if( sChanged == ID_TRUE )
                    {
                        *aChanged = ID_TRUE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    if( ( aPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
                        != MTC_NODE_OPERATOR_AND )
                    {
                        // AND�� �ƴ� �������� ���� subquery��
                        // Unnesting���� �ʴ´�.
                        aUnnestSubquery = ID_FALSE;

                        /* BUG-47786 Unnesting ��� ���� */
                        if ( ( ( aPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
                               == MTC_NODE_OPERATOR_OR ) &&
                             ( aSFWGH != NULL ) )
                        {
                            aSFWGH->lflag &= ~QMV_SFWGH_UNNEST_OR_STOP_MASK;
                            aSFWGH->lflag |= QMV_SFWGH_UNNEST_OR_STOP_TRUE;
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    for( sNode = (qtcNode *)aPredicate->node.arguments;
                         sNode != NULL;
                         sNode = (qtcNode *)sNode->node.next )
                    {
                        IDE_TEST( findAndUnnestSubquery( aStatement,
                                                         aSFWGH,
                                                         aUnnestSubquery,
                                                         sNode,
                                                         aChanged )
                                  != IDE_SUCCESS );
                    }
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

    /* BUG-47786 Unnesting ��� ���� */
    if ( aSFWGH != NULL )
    {
        if ( ( ( aSFWGH->lflag & QMV_SFWGH_UNNEST_LEFT_DISK_MASK )
               == QMV_SFWGH_UNNEST_LEFT_DISK_TRUE ) &&
             ( ( aSFWGH->lflag & QMV_SFWGH_UNNEST_OR_STOP_MASK )
               == QMV_SFWGH_UNNEST_OR_STOP_TRUE ) )
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_TRUE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoUnnesting::isExistsTransformable( qcStatement * aStatement,
                                     qmsSFWGH    * aSFWGH,
                                     qtcNode     * aNode,
                                     idBool        aUnnestSubquery )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� EXISTS/NOT EXISTS ���·� ��ȯ �������� Ȯ���Ѵ�.
 *
 * Implementation :
 *     1. Subquery���� LIMIT��, ROWNUM column, window function ���Կ��� Ȯ��
 *     2. Oracle style outer join�̳� GROUP BY������ ���� Ȯ��
 *     3. EXISTS/NOT EXISTS ���·� ��ȯ�� �� �������� Ȯ��
 *        ������ unnesting �Ұ����� ��� �ұ������� transformation�ϰ�
 *        ������ subquery optimization tip�� �ִ��� Ȱ���Ѵ�.
 *
 ***********************************************************************/

    qtcNode        * sSQNode;
    qtcNode        * sArg;
    qcStatement    * sSQStatement;
    qmsParseTree   * sSQParseTree;
    qmsSFWGH       * sSQSFWGH;
    qmsTarget      * sTarget;
    idBool           sIsTrue;
    idBool           sIsCheck; /* BUG-48336 */

    sSQNode      = (qtcNode *)aNode->node.arguments->next;
    sSQStatement = sSQNode->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    if ( (aNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK) == QTC_NODE_JOIN_OPERATOR_EXIST )
    {
        // Subquery�� outer join �� �Ұ�
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }
    
    if( sSQParseTree->limit != NULL )
    {
        // LIMIT�� ��� �� �Ұ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH == NULL )
    {
        // UNION, UNION ALL, MINUS, INTERSECT �� ����ϴ� ��� SFWGH�� NULL�̴�.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-36580 supported TOP */
    if( sSQSFWGH->top != NULL )
    {

        // top�� ��� �� �Ұ���
        return ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
    
    if( sSQSFWGH->where != NULL )
    {
        if ( (sSQSFWGH->where->lflag & QTC_NODE_JOIN_OPERATOR_MASK) == QTC_NODE_JOIN_OPERATOR_EXIST )
        {
            // Oracle style�� outer join ��� �� unnesting���� �ʴ´�.
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        /* BUG-47576 aggregation +  �ܺ����� �÷��� unnest ��� ���� */
        if ( ( sSQSFWGH->aggsDepth1 != NULL ) &&
             ( sSQSFWGH->outerDepInfo.depCount > 0 ) )
        {
            sIsTrue = ID_TRUE; 
            isAggrExistTransformable( sSQSFWGH->where,
                                      &sSQSFWGH->outerDepInfo,
                                      &sIsTrue );

            if ( sIsTrue == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
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

    if( isQuantifiedSubquery( aNode ) == ID_FALSE )
    {
        // BUG-45250 �� �������� ��, left�� list type�̸� �ȵ˴ϴ�.
        if( ( ( aNode->node.module == &mtfGreaterThan ) ||
              ( aNode->node.module == &mtfGreaterEqual ) ||
              ( aNode->node.module == &mtfLessThan ) ||
              ( aNode->node.module == &mtfLessEqual ) ) &&
            ( aNode->node.arguments->module == &mtfList ) )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        // Quantified predicate�� �ƴϴ��� single row subquery�� ���
        if( isSingleRowSubquery( sSQStatement ) == ID_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            if( isUnnestablePredicate( aStatement, aSFWGH, aNode, sSQSFWGH ) == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
            }
            else
            {
                // Single row�̸鼭 unnesting ������ ����
            }

            if( aSFWGH != NULL )
            {
                if( qtc::dependencyContains( &aSFWGH->depInfo,
                                             &sSQSFWGH->outerDepInfo ) == ID_FALSE )
                {
                    // Subquery�� correlation�� parent query block�� �����Ǿ�� �Ѵ�.
                    IDE_CONT( INVALID_FORM );
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
        if( aSFWGH != NULL )
        {
            if( qtc::dependencyContains( &aSFWGH->depInfo,
                                         &sSQSFWGH->outerDepInfo ) == ID_FALSE )
            {
                // Subquery�� correlation�� parent query block�� �����Ǿ�� �Ѵ�.
                IDE_CONT( INVALID_FORM );
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

    if( ( sSQSFWGH->group      == NULL ) && 
        ( sSQSFWGH->aggsDepth1 != NULL ) &&
        ( qtc::haveDependencies( &sSQSFWGH->outerDepInfo ) == ID_FALSE ) )
    {
        // GROUP BY���� correlation ���� aggregate function�� ����� ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-38996
    // OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY �� ���� ����� Ʋ����
    // aggr �Լ��� count �̸鼭 group by �� ������� �ʴ� ��쿡�� ����� �ٸ��� �ִ�.
    // �� �����϶��� ���ϴ� ���� 0,1 �϶��� unnset �� �����ϴ�.
    if( ( sSQSFWGH->group      == NULL ) &&
        ( sSQSFWGH->aggsDepth1 != NULL ) )
    {
        // BUG-45238
        for( sTarget = sSQSFWGH->target;
             sTarget != NULL;
             sTarget = sTarget->next )
        {
            if ( findCountAggr4Target( sTarget->targetColumn ) == ID_TRUE )
            {
                //�� count(*)���� ������ �� target column�̸� �ȵȴ�.
                if ( sTarget->targetColumn->node.module != &mtfCount )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // Nothing to do.
                    // subquery�� target���� count �÷� �Ѱ��� �ִ� ���
                }

                // subquery�� target���� count �÷��� �ִٸ�, list�� �ƴϿ��� �Ѵ�.
                if ( aNode->node.arguments->module == &mtfList )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // Nothing to do.
                    // subquery�� target���� count �÷� �Ѱ��� �ִ� ���
                }

                // count�÷� �� �ִٸ�, �ٸ� aggregate function�� �Բ� �� �� ����
                if ( sSQSFWGH->aggsDepth1->next != NULL )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // �ٸ� aggregate function�� ���Ǹ� EXISTS��ȯ�Ǹ鼭 ����� �޶��� �� �ִ�.
                    // Nothing to do.
                }

                if ( sSQSFWGH->having != NULL )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // Nothing to do.
                }

                // EXISTS �Ǵ� NOT EXISTS�� ��ȯ �����ؾ��Ѵ�.
                if( toExistsModule4CountAggr( aStatement,aNode ) == NULL )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // COUNT �� ������ ����
                // Nothing to do.
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->aggsDepth2 != NULL )
    {
        // Nested aggregate function�� ����� ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->rownum != NULL )
    {
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    // SELECT�� �˻�
    for( sTarget = sSQSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        if ( (sTarget->targetColumn->lflag & QTC_NODE_ANAL_FUNC_MASK ) == QTC_NODE_ANAL_FUNC_EXIST )
        {
            // Window function�� ����� ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        if( sTarget->targetColumn->depInfo.depCount > 1 )
        {
            // Subquery�� SELECT������ �� �̻��� table�� �����ϴ� ���
            // ex) t1.i1 IN (SELECT t2.i1 + t3.i1 FROM t2, t3 ... );
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    if( aNode->node.arguments->module == &mtfList )
    {
        for( sArg = (qtcNode *)aNode->node.arguments->arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next )
        {
            if( sArg->depInfo.depCount > 1 )
            {
                // Outer query�� ���ǿ��� �� �̻��� table�� �����ϴ� ���
                // ex) (t1.i1 + t2.i1, ...) IN (SELECT i1, ...);

                IDE_CONT( INVALID_FORM );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        if( ((qtcNode*)aNode->node.arguments)->depInfo.depCount > 1 )
        {
            // Outer query�� ���ǿ��� �� �̻��� table�� �����ϴ� ���
            // ex) t1.i1 + t2.i1 IN (SELECT i1 FROM ...);

            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    // Subquery�� correlation�� ���� ��� ������ subquery optimization tip��
    // �̿��ϴ°��� �� ȿ������ ������ �����Ѵ�.
    if( qtc::haveDependencies( &sSQSFWGH->outerDepInfo ) == ID_FALSE )
    {
        // ������ unnesting ���ϴ� ���(ON�� ��) EXISTS�� ��ȯ���� �ʴ´�.
        if( aUnnestSubquery == ID_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Notihng to do.
        }

        // Nullable column�� ���Ե� ��� ������ anti join�� ������ �� �����Ƿ�,
        // NOT EXISTS predicate�� ��ȯ�� �����Ѵ�.
        if( ( ( aNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) == MTC_NODE_GROUP_COMPARISON_TRUE ) &&
            ( ( aNode->node.lflag & MTC_NODE_GROUP_MASK ) == MTC_NODE_GROUP_ALL ) )
        {
            if( aNode->node.arguments->module == &mtfList )
            {
                for( sArg = (qtcNode *)aNode->node.arguments->arguments, sTarget = sSQSFWGH->target;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next, sTarget = sTarget->next )
                {
                    if( ( isNullable( aStatement, sArg ) == ID_TRUE ) ||
                        ( isNullable( aStatement, sTarget->targetColumn )  == ID_TRUE ) )
                    {
                        IDE_CONT( INVALID_FORM );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                if( ( isNullable( aStatement, (qtcNode*)aNode->node.arguments ) == ID_TRUE ) ||
                    ( isNullable( aStatement, sSQSFWGH->target->targetColumn )  == ID_TRUE ) )
                {
                    IDE_CONT( INVALID_FORM );
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_EQUAL )
            {
                // =ALL�� ��� NOT EXISTS�� <>���� correlation�� �����Ƿ�
                // �ᱹ anti join�� �� ����.
                IDE_CONT( INVALID_FORM );
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

        // List type�� <>, <>ANY�� ��� predicate���� OR�� ����Ǿ� semi join���� ���Ѵ�.
        if( ( ( aNode->node.module == &mtfNotEqual ) || ( aNode->node.module == &mtfNotEqualAny ) ) &&
            ( aNode->node.arguments->module == &mtfList ) )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    // BUG-42637 subquery unnesting�� lob ���� ����
    // lob �÷��� group by �� �� ���� ����.
    // subquery unnesting�� group by �� AGGR �Լ��� �ִ� ��쿡��
    // group by �� �߰��� �ǹǷ� ���ƾ� �Ѵ�.
    // BUG-47751 unnesting�� ���� �ʴ´ٸ� exists��ȯ�� ���´�.
    if ( (sSQSFWGH->group != NULL) ||
         (sSQSFWGH->aggsDepth1 != NULL) )
    {
        if( sSQSFWGH->where != NULL )
        {
            if ( (sSQSFWGH->where->lflag & QTC_NODE_LOB_COLUMN_MASK)
                 == QTC_NODE_LOB_COLUMN_EXIST )
            {
                IDE_CONT( INVALID_FORM );
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

        if ( sSQSFWGH->having != NULL )
        {
            if ( (sSQSFWGH->having->lflag & QTC_NODE_LOB_COLUMN_MASK)
                 == QTC_NODE_LOB_COLUMN_EXIST )
            {
                IDE_CONT( INVALID_FORM );
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

    // BUG-48336 compatibiltiy�� 2�̸� ���׹ݿ� ������ Ȯ������ �ʽ��ϴ�. 
    if ( (QCU_OPTIMIZER_UNNEST_COMPATIBILITY & QCU_UNNEST_COMPATIBILITY_MASK_MODE2)
         != QCU_UNNEST_COMPATIBILITY_MASK_MODE2 )
    { 
        sIsCheck = ID_TRUE;
    }
    else
    {
        sIsCheck = ID_FALSE;
    }

    // BUG-43300 no_unnest ��Ʈ ���� exists ��ȯ�� �����ϴ�.
    if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NO_UNNEST )
    {
        IDE_CONT( INVALID_FORM );
    }
    else if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NOT_DEFINED )
    {
        if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
             == QC_TMP_UNNEST_SUBQUERY_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* BUG-46544 Unnest hit */
        // Hint QMO_SUBQUERY_UNNEST_TYPE_UNNEST
        // hint�� �����ϰ� Property�� 0�̰� Compatibility�� 1 �̸� Property
        // �켱���� unnest�� ���� �ʴ´�.
        if ( ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
               == QC_TMP_UNNEST_SUBQUERY_FALSE ) &&
             ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_COMPATIBILITY_1_MASK )
               == QC_TMP_UNNEST_COMPATIBILITY_1_TRUE ) )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // BUG-48336
            // hint�� �����ϰ� Compatibility�� 2�� �ƴϸ� unnest�� �մϴ�.
            sIsCheck = ID_FALSE;
        }
    }

    /*********************************
     *   ���� �����������̿����մϴ�.    
     *********************************/
    if ( sIsCheck == ID_TRUE )
    {
        /**************************************
         *  BUG-48336 �Ʒ� ������ ��� �����ϴ� ���
         *            subquery filter�÷��� �����Ǵ� ��� �߰��մϴ�.
         *   1. subquery��outer dep�� 1�̰�,
         *               ������ ���� �䰡 �ƴ� ���̺��� ���
         *   2. parent query block���� 1�� �̻��� left outer join�� �ִ� ���
         **************************************/
        // 1. Subquery ����
        if ( ( qtc::haveDependencies( &sSQSFWGH->outerDepInfo ) == ID_FALSE ) &&
             ( ( sSQSFWGH->from->next == NULL ) &&
               ( sSQSFWGH->from->joinType == QMS_NO_JOIN ) ) )
        { 
            // - view( view, inline view, with) recursive with�ΰ�� ����
            if ( ( sSQSFWGH->from->tableRef->view == NULL ) &&
                 ( (sSQSFWGH->from->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK)
                   == QMS_TABLE_REF_RECURSIVE_VIEW_FALSE ) )
            {
                // 2. parent query block ����
                if ( ( aSFWGH->from->next != NULL ) ||
                     ( aSFWGH->from->joinType != QMS_NO_JOIN ) )
                {
                    // parent query block�� Oracle style�� outer join ��� ��
                    if ( aSFWGH->where != NULL )
                    {
                        if ( ( aSFWGH->where->lflag & QTC_NODE_JOIN_OPERATOR_MASK)
                             == QTC_NODE_JOIN_OPERATOR_EXIST )
                        {
                            IDE_CONT( INVALID_FORM );
                        }
                    } 
                    else
                    {
                        // Nothing to do.
                    }

                    // parent query block�� Ansi style outer join�� �� �ϳ��� ����� ���
                    if ( ( (aSFWGH->lflag & QMV_SFWGH_JOIN_LEFT_OUTER)  == QMV_SFWGH_JOIN_LEFT_OUTER ) ||
                         ( (aSFWGH->lflag & QMV_SFWGH_JOIN_RIGHT_OUTER) == QMV_SFWGH_JOIN_RIGHT_OUTER ) )
                    {
                        IDE_CONT( INVALID_FORM );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    // main query�� join�� �ƴѰ��
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

        // ���� ���ǿ��� EXISTS��ȯ�� �Ͼ ��� SU ��ȯ�� �Ͼ���� �Ѵ�.
        sSQNode->lflag &= ~QTC_NODE_TRANS_IN_TO_EXISTS_MASK;
        sSQNode->lflag |= QTC_NODE_TRANS_IN_TO_EXISTS_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( INVALID_FORM );

    return ID_FALSE;
}

IDE_RC
qmoUnnesting::transformToExists( qcStatement * aStatement,
                                 qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� quantified predicate�� �Բ� ���� ���
 *     EXISTS/NOT EXISTS ���·� ��ȯ�Ѵ�.
 *
 * Implementation :
 *     1. Subquery���� LIMIT��, ROWNUM column, window function ���Կ��� Ȯ��
 *        (Window function�� WHERE������ ����� �� ����.)
 *     2. Correlation predicate ����
 *     3. ������ correlation predicate�� WHERE�� �Ǵ� HAVING���� ����
 *     4. Subquery predicate�� EXISTS/NOT EXISTS�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode        * sPredicate[2];
    qtcNode        * sSQNode;
    qtcNode        * sCorrPreds;
    qtcNode        * sConcatPred;
    mtcNode        * sNext;
    qcStatement    * sSQStatement;
    qmsParseTree   * sSQParseTree;
    qmsSFWGH       * sSQSFWGH;
    qcStatement    * sTempStatement;
    qmsParseTree   * sTempParseTree;
    qmsSFWGH       * sTempSFWGH;
    qmsOuterNode   * sOuter;
    mtfModule      * sTransModule;
    qcNamePosition   sEmptyPosition;
    // BUG-45238
    qtcNode        * sIsNotNull[2];

    IDU_FIT_POINT_FATAL( "qmoUnnesting::transformToExists::__FT__" );

    SET_EMPTY_POSITION( sEmptyPosition );

    sSQNode      = (qtcNode *)aNode->node.arguments->next;
    sSQStatement = sSQNode->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    /* TASK-7219 Shard Transformer Refactoring */
    if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                        SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
         == ID_TRUE )
    {
        IDE_TEST( sdi::preAnalyzeQuerySet( sSQStatement,
                                           sSQParseTree->querySet,
                                           QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    // Correlation predicate�� ����
    IDE_TEST( genCorrPredicates( aStatement,
                                 aNode,
                                 ID_TRUE, /* aExistsTrans */
                                 &sCorrPreds )
              != IDE_SUCCESS );

    // Correlation predicate �߰��� ���� ���� ���� ����
    IDE_TEST( qmvQTC::setOuterColumns( sSQStatement,
                                       NULL,
                                       sSQSFWGH,
                                       sCorrPreds )
              != IDE_SUCCESS );

    // BUG-45668 ���ʿ� ���������� ������ exists ��ȯ�� ����� Ʋ���ϴ�.
    // ���� ���������� outerColumns �� ���������� �Ѱ��־�� �Ѵ�.
    if ( sCorrPreds->node.arguments->module == &qtc::subqueryModule )
    {
        sTempStatement = ((qtcNode*)(sCorrPreds->node.arguments))->subquery;
        sTempParseTree = (qmsParseTree *)sTempStatement->myPlan->parseTree;
        sTempSFWGH     = sTempParseTree->querySet->SFWGH;

        for ( sOuter = sTempSFWGH->outerColumns;
              sOuter != NULL;
              sOuter = sOuter->next )
        {
            IDE_TEST( qmvQTC::addOuterColumn( sSQStatement,
                                              sSQSFWGH,
                                              sOuter->column )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // nothing to do.
    }

    // BUG-40753 aggsDepth1 ������ �߸��Ǿ� ����� Ʋ��
    // aggsDepth1�� ����� �������ֵ��� �Ѵ�.
    IDE_TEST( setAggrNode( sSQStatement,
                           sSQSFWGH,
                           sCorrPreds )
              != IDE_SUCCESS );

    IDE_TEST( qmvQTC::setOuterDependencies( sSQSFWGH,
                                            &sSQSFWGH->outerDepInfo )
              != IDE_SUCCESS );

    qtc::dependencySetWithDep( &sSQSFWGH->thisQuerySet->outerDepInfo,
                               &sSQSFWGH->outerDepInfo );

    qtc::dependencySetWithDep( &sSQNode->depInfo,
                               &sSQSFWGH->outerDepInfo );

    if( (sSQSFWGH->group      == NULL) &&
        (sSQSFWGH->aggsDepth1 == NULL) )
    {
        // GROUP BY��, aggregate function�� ������� ���� ���
        // Correlation predicate�� WHERE���� �߰�
        IDE_TEST( concatPredicate( aStatement,
                                   sSQSFWGH->where,
                                   sCorrPreds,
                                   &sConcatPred )
                  != IDE_SUCCESS );
        sSQSFWGH->where = sConcatPred;
    }
    else
    {
        // BUG-38996
        // OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY �� ���� ����� Ʋ����
        // aggr �Լ��� count(*) �̸鼭 group by �� ������� �ʴ� ��쿡�� ����� �ٸ��� �ִ�.
        // �� �����϶��� ���ϴ� ���� 0,1 �϶��� unnset �� �����ϴ�.
        // ��ȯ�� �����Ҷ��� having ��, aggr �Լ��� ���� �Ѵ�.
        if( (sSQSFWGH->group      == NULL) &&
            (sSQSFWGH->aggsDepth1 != NULL) )
        {
            if (sSQSFWGH->aggsDepth1->aggr->node.module == &mtfCount )
            {
                // Nothing to do.
            }
            else
            {
                // Correlation predicate�� HAVING���� �߰�
                IDE_TEST( concatPredicate( aStatement,
                                           sSQSFWGH->having,
                                           sCorrPreds,
                                           &sConcatPred )
                          != IDE_SUCCESS );
                sSQSFWGH->having = sConcatPred;
            }

        }
        else
        {
            // Correlation predicate�� HAVING���� �߰�
            IDE_TEST( concatPredicate( aStatement,
                                       sSQSFWGH->having,
                                       sCorrPreds,
                                       &sConcatPred )
                      != IDE_SUCCESS );
            sSQSFWGH->having = sConcatPred;
        }
    }

    // BUG-38996
    // OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY �� ���� ����� Ʋ����
    // aggr �Լ��� count(*) �̸鼭 group by �� ������� �ʴ� ��쿡�� ����� �ٸ��� �ִ�.
    // �� �����϶��� ���ϴ� ���� 0,1 �϶��� unnset �� �����ϴ�.
    // ��ȯ�� �����Ҷ��� having ��, aggr �Լ��� ���� �Ѵ�.
    if( (sSQSFWGH->group      == NULL) &&
        (sSQSFWGH->aggsDepth1 != NULL) )
    {
        if( sSQSFWGH->aggsDepth1->aggr->node.module == &mtfCount )
        {
            // BUG-45238
            // isExistsTransformable�� �����ϴٸ�, count�� �ִ� ��쿡��
            // �ٸ� aggregation�� �Բ� �� �� ��� count�÷��� �ִ�.
            // COUNT( arguments ) �÷��� �ִ� ���, (arguments) IS NOT NULL�� where���� �߰��Ѵ�.
            if ( sSQSFWGH->aggsDepth1->aggr->node.arguments != NULL )
            {
                IDE_TEST( qtc::makeNode( aStatement,
                                            sIsNotNull,
                                            &sEmptyPosition,
                                            &mtfIsNotNull )
                             != IDE_SUCCESS );

                sIsNotNull[0]->node.arguments = sSQSFWGH->aggsDepth1->aggr->node.arguments;

                IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                            sIsNotNull[0] )
                             != IDE_SUCCESS );

                IDE_TEST( concatPredicate( aStatement,
                                              sSQSFWGH->where,
                                              sIsNotNull[0],
                                              &sConcatPred )
                             != IDE_SUCCESS );

                sSQSFWGH->where = sConcatPred;
            }
            else
            {
                // Nothing to do.
            }

            sSQSFWGH->aggsDepth1 = NULL;

            sTransModule = toExistsModule4CountAggr( aStatement, aNode );

            IDE_DASSERT( sTransModule != NULL);

        }
        else
        {
            sTransModule = toExistsModule( aNode->node.module );
        }
    }
    else
    {
        sTransModule = toExistsModule( aNode->node.module );
    }

    // Subquery predicate�� EXISTS/NOT EXISTS�� ����
    IDE_TEST( qtc::makeNode( aStatement,
                             sPredicate,
                             &sEmptyPosition,
                             sTransModule )
              != IDE_SUCCESS );

    sNext = aNode->node.next;
    idlOS::memcpy( aNode, sPredicate[0], ID_SIZEOF( qtcNode ) );
    aNode->node.arguments = (mtcNode *)sSQNode;
    aNode->node.next = sNext;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                aNode )
              != IDE_SUCCESS );

    IDE_TEST( setDummySelect( aStatement, aNode, ID_FALSE )
              != IDE_SUCCESS );

    // BUG-41917
    IDE_TEST( qtcCache::validateSubqueryCache( QC_SHARED_TMPLATE(aStatement),
                                               (qtcNode *)aNode->node.arguments )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoUnnesting::isUnnestableSubquery( qcStatement * aStatement,
                                    qmsSFWGH    * aSFWGH,
                                    qtcNode     * aSubqueryPredicate )
{
/***********************************************************************
 *
 * Description :
 *     Unnesting ������ subquery���� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcStatement      * sSQStatement;
    qmsParseTree     * sSQParseTree;
    qmsSFWGH         * sSQSFWGH;
    qmsConcatElement * sGroup;
    qmsAggNode       * sAggr;
    qmsTarget        * sTarget;
    qtcNode          * sSQNode;
    qcDepInfo          sDepInfo;
    qmsFrom          * sSQFrom; 
    idBool             sIsCheck; /* BUG-48336 */

    sSQNode      = (qtcNode *)aSubqueryPredicate->node.arguments;
    sSQStatement = sSQNode->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    if( sSQParseTree->querySet->setOp != QMS_NONE )
    {
        // SET ������ ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-48336 compatibiltiy�� 2�̸� ���׹ݿ� ������ Ȯ������ �ʽ��ϴ�. 
    if ( (QCU_OPTIMIZER_UNNEST_COMPATIBILITY & QCU_UNNEST_COMPATIBILITY_MASK_MODE2)
         != QCU_UNNEST_COMPATIBILITY_MASK_MODE2 )
    { 
        sIsCheck = ID_TRUE;
    }
    else
    {
        sIsCheck = ID_FALSE;
    }

    if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NO_UNNEST )
    {
        IDE_CONT( INVALID_FORM );
    }
    else if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NOT_DEFINED )
    {
        if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
             == QC_TMP_UNNEST_SUBQUERY_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* BUG-46544 Unnest hit */
        // Hint QMO_SUBQUERY_UNNEST_TYPE_UNNEST
        // hint�� �����ϰ� Property�� 0�̰� Compatibility�� 1 �̸� Property
        // �켱���� unnest�� ���� �ʴ´�.
        if ( ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
               == QC_TMP_UNNEST_SUBQUERY_FALSE ) &&
             ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_COMPATIBILITY_1_MASK )
               == QC_TMP_UNNEST_COMPATIBILITY_1_TRUE ) )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // BUG-48336
            // hint�� �����ϰ� Compatibility�� 2�� �ƴϸ� unnest�� �մϴ�.
            sIsCheck = ID_FALSE;
        }
    }

    if( sSQParseTree->limit != NULL )
    {
        // LIMIT���� ����� ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-36580 supported TOP */
    if( sSQSFWGH->top    != NULL )
    {
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-37314 ���������� rownum �� �ִ� ��쿡�� unnest �� �����ؾ� �Ѵ�. */
    if( sSQSFWGH->rownum != NULL )
    {
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->hierarchy != NULL )
    {
        // Hierarchy������ correlation�� ������ ���

        if( sSQSFWGH->hierarchy->startWith != NULL )
        {
            if( qtc::dependencyContains( &sSQSFWGH->depInfo,
                                         &sSQSFWGH->hierarchy->startWith->depInfo )
                == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
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

        if( sSQSFWGH->hierarchy->connectBy != NULL )
        {
            if( qtc::dependencyContains( &sSQSFWGH->depInfo,
                                         &sSQSFWGH->hierarchy->connectBy->depInfo )
                == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
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

    // PROJ-2418
    // sSQSFWGH�� From���� Lateral View�� �����ϸ� Unnesting �� �� ����.
    // ��, Lateral View�� ������ ��� Merging �Ǿ��ٸ� Unnesting�� �����ϴ�.
    for ( sSQFrom = sSQSFWGH->from; sSQFrom != NULL; sSQFrom = sSQFrom->next )
    {
        IDE_TEST( qmvQTC::getFromLateralDepInfo( sSQFrom, & sDepInfo )
                  != IDE_SUCCESS );

        if ( qtc::haveDependencies( & sDepInfo ) == ID_TRUE )
        {
            // �ش� From�� Lateral View�� �����ϸ� Unnesting �Ұ�
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    if( ( sSQSFWGH->where  != NULL ) ||
        ( sSQSFWGH->having != NULL ) )
    {
        // WHERE���� HAVING���� ������ Ȯ���Ѵ�.
        if( isUnnestablePredicate( aStatement,
                                   aSFWGH,
                                   aSubqueryPredicate,
                                   sSQSFWGH ) == ID_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        IDE_CONT( INVALID_FORM );
    }

    if( ( sSQSFWGH->group      == NULL ) &&
        ( sSQSFWGH->aggsDepth1 != NULL ) )
    {
        if( qtc::haveDependencies( &sSQSFWGH->outerDepInfo ) == ID_FALSE )
        {
            // GROUP BY���� correlation ���� aggregate function�� ����� ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Cost-based query transformation�� �ʿ��� ����
            // ex) SELECT * FROM T1 WHERE I1 = (SELECT SUM(I1) FROM T2 WHERE T1.I2 = T2.I2);
            //     ���� T2.I2�� index�� �����ϰ� T1�� cardinality�� ���� �ʴٸ�
            //     unnesting���� �ʴ� ���� �����ϰ� �׷��� �ʴٸ� ������ ���� �����ϴ°��� �����ϴ�.
            //     SELECT * FROM T1, (SELECT SUM(I1) COL1, I2 COL2 FROM T2 GROUP BY I2) V1
            //       WHERE T1.I1 = V1.COL1 AND T1.I2 = V1.COL2;

            qcgPlan::registerPlanProperty( aStatement,
                                           PLAN_PROPERTY_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY );

            if( QCU_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY == 0 )
            {
                IDE_CONT( INVALID_FORM );
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

    if( sSQSFWGH->aggsDepth2 != NULL )
    {
        // Nested aggregate function�� ����� ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( qtc::dependencyContains( &aSFWGH->depInfo,
                                 &sSQSFWGH->outerDepInfo ) == ID_FALSE )
    {
        // Subquery�� correlation�� parent query block�� �����Ǿ�� �Ѵ�.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    for( sTarget = sSQSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        if ( (sTarget->targetColumn->lflag & QTC_NODE_AGGREGATE_MASK ) == QTC_NODE_AGGREGATE_EXIST )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        // BUG-41564
        // Target Subquery�� ���� Subquery ���� �����Ѵٸ� Unnesting �Ұ�
        if( isOuterRefSubquery( sTarget->targetColumn, &sSQSFWGH->depInfo ) == ID_TRUE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    for( sGroup = sSQSFWGH->group;
         sGroup != NULL;
         sGroup = sGroup->next )
    {
        // BUG-45151 ROLL-UP, CUBE ���� ����ϸ� sGroup->arithmeticOrList�� NULL�̶� �׽��ϴ�.
        if( sGroup->type != QMS_GROUPBY_NORMAL )
        {
            // ROLL-UP, CUBE ���� ��� �ϴ� ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        qtc::dependencyAnd( &sGroup->arithmeticOrList->depInfo,
                            &sSQSFWGH->outerDepInfo,
                            &sDepInfo );

        if( sDepInfo.depCount != 0 )
        {
            // SELECT, WHERE, HAVING �� clause���� outer query�� column�� ������ ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    for( sAggr = sSQSFWGH->aggsDepth1;
         sAggr != NULL;
         sAggr = sAggr->next )
    {
        qtc::dependencyAnd( &sAggr->aggr->depInfo,
                            &sSQSFWGH->outerDepInfo,
                            &sDepInfo );

        if( sDepInfo.depCount != 0 )
        {
            // SELECT, WHERE, HAVING �� clause���� outer query�� column�� ������ ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    for( sAggr = sSQSFWGH->aggsDepth2;
         sAggr != NULL;
         sAggr = sAggr->next )
    {
        qtc::dependencyAnd( &sAggr->aggr->depInfo,
                            &sSQSFWGH->outerDepInfo,
                            &sDepInfo );

        if( sDepInfo.depCount != 0 )
        {
            // SELECT, WHERE, HAVING �� clause���� outer query�� column�� ������ ���
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }

    // BUG-42637 subquery unnesting�� lob ���� ����
    // lob �÷��� group by �� �� ���� ����.
    // subquery unnesting�� group by �� AGGR �Լ��� �ִ� ��쿡��
    // group by �� �߰��� �ǹǷ� ���ƾ� �Ѵ�.
    if ( (sSQSFWGH->group != NULL) ||
         (sSQSFWGH->aggsDepth1 != NULL) )
    {
        if( sSQSFWGH->where != NULL )
        {
            if ( (sSQSFWGH->where->lflag & QTC_NODE_LOB_COLUMN_MASK)
                 == QTC_NODE_LOB_COLUMN_EXIST )
            {
                IDE_CONT( INVALID_FORM );
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

        if ( sSQSFWGH->having != NULL )
        {
            if ( (sSQSFWGH->having->lflag & QTC_NODE_LOB_COLUMN_MASK)
                 == QTC_NODE_LOB_COLUMN_EXIST )
            {
                IDE_CONT( INVALID_FORM );
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

    /********************************
     * ���� �����������̿����մϴ�.                
     ********************************/
    if ( sIsCheck == ID_TRUE )
    { 
        /**************************************
         *  BUG-48336 �Ʒ� ������ ��� �����ϴ� ���
         *            subquery filter�÷��� �����Ǵ� ��� �߰��մϴ�.
         *   1. subquery��IN->EXISTS ��ȯ�� ���߰�,
         *               outer dep�� 1�̰�,
         *               ������ ���� �䰡 �ƴ� ���̺��� ���
         *   2. parent query block���� 1�� �̻��� left outer join�� �ִ� ���
         *************************/
        // 1. Subquery ����
        //    IN->EXISTS ������ȯ�� �� ��� UNNEST
        if ( (sSQNode->lflag & QTC_NODE_TRANS_IN_TO_EXISTS_MASK)
             == QTC_NODE_TRANS_IN_TO_EXISTS_FALSE )
        {
            // JOIN�� �ְų� outer dependency�� 1 �ƴѰ�� UNNEST
            if ( ( sSQSFWGH->from->next == NULL ) &&
                 ( sSQSFWGH->from->joinType == QMS_NO_JOIN ) &&
                 ( sSQSFWGH->outerDepInfo.depCount == 1 ) )
            {
                // view( view, inline view, with) recursive with�� ��� UNNEST
                if ( ( sSQSFWGH->from->tableRef->view == NULL ) &&
                     ( (sSQSFWGH->from->tableRef->flag & QMS_TABLE_REF_RECURSIVE_VIEW_MASK)
                       == QMS_TABLE_REF_RECURSIVE_VIEW_FALSE ) )
                {
                    // 2. parent query block ���� 
                    if ( ( aSFWGH->from->next != NULL ) ||
                         ( aSFWGH->from->joinType != QMS_NO_JOIN ) )
                    {
                        // Oracle style�� outer join 
                        if ( aSFWGH->where != NULL )
                        {
                            if ( ( aSFWGH->where->lflag & QTC_NODE_JOIN_OPERATOR_MASK)
                                 == QTC_NODE_JOIN_OPERATOR_EXIST )
                            {
                                IDE_CONT( INVALID_FORM );
                            }
                        } 
                        else
                        {
                            // Nothing to do.
                        }

                        // Ansi style outer join�� �� �ϳ��� ����� ���
                        if ( ( (aSFWGH->lflag & QMV_SFWGH_JOIN_LEFT_OUTER) == QMV_SFWGH_JOIN_LEFT_OUTER ) ||
                             ( (aSFWGH->lflag & QMV_SFWGH_JOIN_RIGHT_OUTER) == QMV_SFWGH_JOIN_RIGHT_OUTER ) )
                        {
                            IDE_CONT( INVALID_FORM );
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
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( INVALID_FORM );

    IDE_EXCEPTION_END;

    return ID_FALSE;
}

IDE_RC
qmoUnnesting::unnestSubquery( qcStatement * aStatement,
                              qmsSFWGH    * aSFWGH,
                              qtcNode     * aSQPredicate )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� unnesting �õ� �Ѵ�.
 *
 * Implementation :
 *     1. Simple/complex subquery�� �����Ѵ�.
 *     2. Subquery�� ����� single/multiple row���� ���ο� ����
 *        join�� ����(semi/inner)�� �����Ѵ�.
 *
 ***********************************************************************/

    qcStatement  * sSQStatement;
    qmsParseTree * sSQParseTree;
    qmsSFWGH     * sSQSFWGH;
    qmsFrom      * sViewFrom;
    qtcNode      * sCorrPred = NULL;
    mtcNode      * sNext;
    qcDepInfo      sDepInfo;
    idBool         sIsSingleRow;
    idBool         sIsRemoveSemi;
    UInt           sTuple;
    
    IDU_FIT_POINT_FATAL( "qmoUnnesting::unnestSubquery::__FT__" );

    sSQStatement = ((qtcNode *)aSQPredicate->node.arguments)->subquery;

    sIsSingleRow = isSingleRowSubquery( sSQStatement );

    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    /* TASK-7219 Shard Transformer Refactoring */
    if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                        SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
         == ID_TRUE )
    {
        IDE_TEST( sdi::preAnalyzeQuerySet( sSQStatement,
                                           sSQParseTree->querySet,
                                           QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if( sSQSFWGH->where != NULL )
    {
        // WHERE���� correlation predicate ����
        IDE_TEST( removeCorrPredicate( aStatement,
                                       &sSQSFWGH->where,
                                       &sSQParseTree->querySet->outerDepInfo,
                                       &sCorrPred )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->having != NULL )
    {
        // HAVING���� correlation predicate ����
        IDE_TEST( removeCorrPredicate( aStatement,
                                       &sSQSFWGH->having,
                                       &sSQParseTree->querySet->outerDepInfo,
                                       &sCorrPred )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // Subquery�� SELECT���� ��� ����
    sSQSFWGH->target               = NULL;
    sSQParseTree->querySet->target = NULL;

    // Correlation predicate���� ����ϴ� column���� SELECT���� ����
    IDE_TEST( genViewSelect( sSQStatement,
                             sCorrPred,
                             ID_FALSE )
              != IDE_SUCCESS );

    // BUG-42637
    // unnest �������� ������ view�� target�� lob �÷��� ������� LobLocatorFunc�� �������ش�.
    // view�� �ݵ�� lobLocator Ÿ������ ��ȯ�Ǿ�� �Ѵ�.
    IDE_TEST( qmvQuerySet::addLobLocatorFunc( sSQStatement, sSQSFWGH->target )
              != IDE_SUCCESS );

    IDE_TEST( transformSubqueryToView( aStatement,
                                       (qtcNode *)aSQPredicate->node.arguments,
                                       &sViewFrom )
              != IDE_SUCCESS );

    // Subquery���� ���ŵ� correlation predicate�� view���� join predicate���� ��ȯ
    IDE_TEST( toViewColumns( sSQStatement,
                             sViewFrom->tableRef,
                             &sCorrPred,
                             ID_FALSE )
              != IDE_SUCCESS );

    qtc::dependencyClear( &sViewFrom->semiAntiJoinDepInfo );

    if( aSQPredicate->node.module == &mtfExists )
    {
        qtc::dependencyAnd( &sCorrPred->depInfo,
                            &aSFWGH->depInfo,
                            &sDepInfo );
        if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
        {
            if( sIsSingleRow == ID_TRUE )
            {
                // Inner join���� ó���Ѵ�.
            }
            else
            {
                qcgPlan::registerPlanProperty(
                            aStatement,
                            PLAN_PROPERTY_OPTIMIZER_SEMI_JOIN_REMOVE );

                if ( QCU_OPTIMIZER_SEMI_JOIN_REMOVE == 1 )
                {
                    // BUG-45172 semi ������ ������ ������ �˻��Ͽ� flag�� ������ �д�.
                    // ������ ���������� semi ������ ��� flag �� ���� semi ������ ������
                    sIsRemoveSemi = isRemoveSemiJoin( sSQStatement, sSQParseTree );
                }
                else
                {
                    sIsRemoveSemi = ID_FALSE;
                }

                // Semi-join
                IDE_TEST( setJoinType( sCorrPred, ID_FALSE, sIsRemoveSemi, sViewFrom )
                          != IDE_SUCCESS );

                removeDownSemiJoinFlag( sSQSFWGH );

                setNoMergeHint( sViewFrom );
            }
            setJoinMethodHint( aStatement, aSFWGH, sCorrPred, sViewFrom, ID_FALSE );
        }
        else
        {
            // Nothing to do.
            // Outer query�� outer query�� �����ϴ� predicate�� ���
        }
    }
    else
    {
        // Anti-join
        IDE_TEST( setJoinType( sCorrPred, ID_TRUE, ID_FALSE, sViewFrom )
                  != IDE_SUCCESS );

        setNoMergeHint( sViewFrom );
        setJoinMethodHint( aStatement, aSFWGH, sCorrPred, sViewFrom, ID_TRUE );
    }

    /* BUG-47786 Unnesting ��� ���� */
    if ( aSQPredicate->depInfo.depCount == 1 )
    {
        sTuple = aSQPredicate->depInfo.depend[0];

        if ( ( QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTuple].lflag & MTC_TUPLE_STORAGE_MASK )
             == MTC_TUPLE_STORAGE_DISK )
        {
            aSFWGH->lflag &= ~QMV_SFWGH_UNNEST_LEFT_DISK_MASK;
            aSFWGH->lflag |= QMV_SFWGH_UNNEST_LEFT_DISK_TRUE;
        }
    }

    // EXISTS/NOT EIXSTS�� �ִ� �ڸ��� view join predicate�� �����Ѵ�.
    sNext = aSQPredicate->node.next;
    idlOS::memcpy( aSQPredicate, sCorrPred, ID_SIZEOF( qtcNode ) );
    aSQPredicate->node.next = sNext;

    // FROM���� ù ��°�� �����Ѵ�.
    sViewFrom->next = aSFWGH->from;
    aSFWGH->from = sViewFrom;

    // Dependency ����
    IDE_TEST( qtc::dependencyOr( &aSFWGH->depInfo,
                                 &sViewFrom->depInfo,
                                 &aSFWGH->depInfo )
              != IDE_SUCCESS );
    IDE_TEST( qtc::dependencyOr( &aSFWGH->thisQuerySet->depInfo,
                                 &sViewFrom->depInfo,
                                 &aSFWGH->thisQuerySet->depInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::setJoinMethodHint( qcStatement * aStatement,
                                 qmsSFWGH    * aSFWGH,
                                 qtcNode     * aJoinPred,
                                 qmsFrom     * aViewFrom,
                                 idBool        aIsAntiJoin )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� ����� hint�� ���� outer query���� join method hint��
 *     �������ش�.
 *
 * Implementation :
 *     | Subquery�� hint    | Outer query�� hint |
 *     | NL_SJ, NL_AJ       | USE_NL             |
 *     | HASH_SJ, HASH_AJ   | USE_HASH           |
 *     | MERGE_SJ, MERGE_AJ | USE_MERGE          |
 *     | SORT_SJ, SORT_AJ   | USE_SORT           |
 * 
 *   - PROJ-2385 //
 *     NL_AJ, MERGE_SJ/_AJ�� �����ϰ�� 
 *     ��� Inverse Join Method Hint ������ �����ϴ�.
 *
 ***********************************************************************/

    SInt                 sTable;
    idBool               sExistHint   = ID_TRUE;
    UInt                 sFlag        = 0;
    qmsJoinMethodHints * sJoinMethodHint;
    qmsParseTree       * sViewParseTree;
    qmsSFWGH           * sViewSFWGH;
    qmsFrom            * sFrom;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::setJoinMethodHint::__FT__" );

    sViewParseTree = (qmsParseTree *)aViewFrom->tableRef->view->myPlan->parseTree;
    sViewSFWGH     = sViewParseTree->querySet->SFWGH;

    if( aIsAntiJoin == ID_FALSE )
    {
        // Inner/semi join�� ���
        switch( sViewSFWGH->hints->semiJoinMethod )
        {
            case QMO_SEMI_JOIN_METHOD_NOT_DEFINED:
                sExistHint = ID_FALSE;
                break;
            case QMO_SEMI_JOIN_METHOD_NL:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_NL;
                break;
            case QMO_SEMI_JOIN_METHOD_HASH:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_HASH;
                break;
            case QMO_SEMI_JOIN_METHOD_MERGE:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_MERGE;
                break;
            case QMO_SEMI_JOIN_METHOD_SORT:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_SORT;
                break;
            default:
                IDE_FT_ASSERT( 0 );
        }
    }
    else
    {
        // Anti join�� ���
        switch( sViewSFWGH->hints->antiJoinMethod )
        {
            case QMO_ANTI_JOIN_METHOD_NOT_DEFINED:
                sExistHint = ID_FALSE;
                break;
            case QMO_ANTI_JOIN_METHOD_NL:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_NL;
                break;
            case QMO_ANTI_JOIN_METHOD_HASH:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_HASH;
                break;
            case QMO_ANTI_JOIN_METHOD_MERGE:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_MERGE;
                break;
            case QMO_ANTI_JOIN_METHOD_SORT:
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_SORT;
                break;
            default:
                IDE_FT_ASSERT( 0 );
        }
    }

    // PROJ-2385
    // PROJ-2339���� �߰��� ��Ʈ�� �����ϰ�, �ΰ� ��Ʈ�� �����Ѵ�.
    // ������ ��Ʈ�δ�, INVERSE�� ������ ������ Method�� ������ �� ���� �����̴�.
    switch ( sViewSFWGH->hints->inverseJoinOption )
    {
        case QMO_INVERSE_JOIN_METHOD_DENIED: // NO_INVERSE_JOIN
        {
            if ( sExistHint == ID_FALSE )
            {
                /* Join Method Hint�� �������� �ʴ� ���,
                 * ��� Inverse Join Method�� ������ ������ Method�� ����Ѵ�.  */
                sExistHint = ID_TRUE;
                sFlag |= QMO_JOIN_METHOD_MASK;
                sFlag &= ~QMO_JOIN_METHOD_INVERSE;
            }
            else
            {
                /* Join Method Hint�� �����ϴ� ���,
                 * �ش� Method �߿��� Inverse Join Method�� �����Ѵ�. */
                sFlag &= ~QMO_JOIN_METHOD_INVERSE;
            }
            break;
        }
        case QMO_INVERSE_JOIN_METHOD_ONLY: // INVERSE_JOIN
        {
            if ( sExistHint == ID_FALSE )
            {
                /* Join Method Hint�� �������� �ʴ� ���,
                 * ��� Inverse Join Method�� ����Ѵ�. */
                sExistHint = ID_TRUE;
                sFlag &= ~QMO_JOIN_METHOD_MASK;
                sFlag |= QMO_JOIN_METHOD_INVERSE;
            }
            else
            {
                /* Join Method Hint�� �����ϴ� ���,
                 * �ش� Method �߿��� Inverse Join Method�� ����Ѵ�.
                 * ��, �ش� Method�� Inverse Join Method�� �ƿ� ���õ� �� ���� ���
                 * ( ��, NL Join (Anti), MERGE Join(Semi/Anti) )
                 * INVERSE ��Ʈ�� �Ϲ������� �����Ѵ�. */

                if ( ( sViewSFWGH->hints->antiJoinMethod == QMO_ANTI_JOIN_METHOD_NL    ) ||
                     ( sViewSFWGH->hints->semiJoinMethod == QMO_SEMI_JOIN_METHOD_MERGE ) ||
                     ( sViewSFWGH->hints->antiJoinMethod == QMO_ANTI_JOIN_METHOD_MERGE ) )
                {
                    // Nothing to do.
                }
                else
                {
                    sFlag &= ( ~QMO_JOIN_METHOD_MASK | QMO_JOIN_METHOD_INVERSE );
                }
            }
            break;
        }
        case QMO_INVERSE_JOIN_METHOD_ALLOWED: // (default)
            // Nothing to do.  
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    if( sExistHint == ID_TRUE )
    {
        sTable = qtc::getPosFirstBitSet( &aJoinPred->depInfo );

        while( sTable != QTC_DEPENDENCIES_END )
        {
            if( sTable != aViewFrom->tableRef->table )
            {
                IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsJoinMethodHints ),
                                                           (void**)&sJoinMethodHint )
                          != IDE_SUCCESS );

                QCP_SET_INIT_JOIN_METHOD_HINTS( sJoinMethodHint );

                // PROJ-2339, 2385
                // ���� Join Method�� Inverse Join Method�� ����ϴ� ����� (ALLOWED)
                // ���ݴ� dependency�� ���� Inverse Join Method�� ���� ����ؾ� �Ѵ�.
                if( sViewSFWGH->hints->inverseJoinOption == QMO_INVERSE_JOIN_METHOD_ALLOWED )
                {
                    sJoinMethodHint->isUndirected = ID_TRUE;
                }
                else
                {
                    // Default : ID_FALSE
                    sJoinMethodHint->isUndirected = ID_FALSE;
                }

                IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsHintTables ),
                                                           (void**)&sJoinMethodHint->joinTables )
                          != IDE_SUCCESS );

                IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsHintTables ),
                                                           (void**)&sJoinMethodHint->joinTables->next )
                          != IDE_SUCCESS );

                sFrom = QC_SHARED_TMPLATE( aStatement )->tableMap[sTable].from;

                SET_POSITION( sJoinMethodHint->joinTables->userName,  sFrom->tableRef->userName );
                SET_POSITION( sJoinMethodHint->joinTables->tableName, sFrom->tableRef->tableName );
                sJoinMethodHint->joinTables->table = sFrom;

                SET_POSITION( sJoinMethodHint->joinTables->next->userName,  aViewFrom->tableRef->userName );
                SET_POSITION( sJoinMethodHint->joinTables->next->tableName, aViewFrom->tableRef->tableName );
                sJoinMethodHint->joinTables->next->table = aViewFrom;
                sJoinMethodHint->joinTables->next->next  = NULL;

                sJoinMethodHint->flag = sFlag;
                qtc::dependencySet( sTable,
                                    &sJoinMethodHint->depInfo );
                qtc::dependencyOr( &aViewFrom->depInfo,
                                   &sJoinMethodHint->depInfo,
                                   &sJoinMethodHint->depInfo );
                sJoinMethodHint->next = aSFWGH->hints->joinMethod;
                aSFWGH->hints->joinMethod = sJoinMethodHint;
            }
            else
            {
                // Nothing to do.
            }

            sTable = qtc::getPosNextBitSet( &aJoinPred->depInfo, sTable );
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

void
qmoUnnesting::setNoMergeHint( qmsFrom * aViewFrom )
{
/***********************************************************************
 *
 * Description :
 *     View�� ���Ե� relation�� �� �̻��� ��� NO_MERGE hint��
 *     �����Ͽ� view merging�� ������� �ʵ��� �Ѵ�.
 *     View�� ������� semi/anti join�� �õ��ϴ� ��쿡�� �ʿ��ϴ�.
 *     ���� view ������ join���� semi/anti join�� ���� ����Ǹ�
 *     ����� �޶��� �� �ִ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcStatement     * sViewStatement;
    qmsParseTree    * sViewParseTree;
    qmsSFWGH        * sViewSFWGH;

    sViewStatement = aViewFrom->tableRef->view;
    sViewParseTree = (qmsParseTree *)sViewStatement->myPlan->parseTree;
    sViewSFWGH     = sViewParseTree->querySet->SFWGH;

    // View�� �� �̻��� relation�� ���Ե� ��쿡�� NO_MERGE hint�� �����Ѵ�.
    if( ( sViewSFWGH->from->next != NULL ) ||
        ( sViewSFWGH->from->joinType != QMS_NO_JOIN ) )
    {
        aViewFrom->tableRef->noMergeHint = ID_TRUE;
    }
    else
    {
        // Nothing to do.
    }
}

IDE_RC
qmoUnnesting::setJoinType( qtcNode * aPredicate,
                           idBool    aType,
                           idBool    aIsRemoveSemi,
                           qmsFrom * aViewFrom )
{
/***********************************************************************
 *
 * Description :
 *     Join predicate�� semi/anti join�� ���� ������ flag�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qcDepInfo   sDepInfo;
    
    IDU_FIT_POINT_FATAL( "qmoUnnesting::setJoinType::__FT__" );

    if( ( aPredicate->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        for( sArg = (qtcNode *)aPredicate->node.arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next )
        {
            IDE_TEST( setJoinType( sArg, aType, aIsRemoveSemi, aViewFrom ) != IDE_SUCCESS );
        }
    }
    else
    {
        qtc::dependencySet( aViewFrom->tableRef->table, &sDepInfo );
        qtc::dependencyAnd( &aPredicate->depInfo, &sDepInfo, &sDepInfo );
        if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
        {
            // Type ����
            if( aType == ID_FALSE )
            {
                // Semi-join
                aPredicate->lflag &= ~QTC_NODE_JOIN_TYPE_MASK;
                aPredicate->lflag |= QTC_NODE_JOIN_TYPE_SEMI;

                // BUG-45172 semi ������ ������ ������ �˻��Ͽ� flag�� ������ �д�.
                // ������ ���������� semi ������ ��� flag �� ���� ���� semi ������ ������
                if ( aIsRemoveSemi == ID_TRUE )
                {
                    aPredicate->lflag &= ~QTC_NODE_REMOVABLE_SEMI_JOIN_MASK;
                    aPredicate->lflag |= QTC_NODE_REMOVABLE_SEMI_JOIN_TRUE;
                }
                else
                {
                    // nothing to do.
                }
            }
            else
            {
                // Anti-join
                aPredicate->lflag &= ~QTC_NODE_JOIN_TYPE_MASK;
                aPredicate->lflag |= QTC_NODE_JOIN_TYPE_ANTI;
            }

            // BUG-45167 ���������� �θ������� ���̺��� �����ϴ� one table predicate�� ����ϴ� ��� fatal�� �߻��մϴ�.
            // ���������� �����ϴ� predicate �� ������ø� �߰��ؾ� �Ѵ�.
            qtc::dependencyOr( &aPredicate->depInfo,
                               &aViewFrom->semiAntiJoinDepInfo,
                               &aViewFrom->semiAntiJoinDepInfo );
        }
        else
        {
            // Nothing to do.
            // SELECT * FROM T1 WHERE EXISTS (SELECT 0 FROM T2 WHERE T1.I1 = T2.I1 AND T1.I2 > 0);
            // ���� I1.I2�� correlation predicate������ join ����� �����Ƿ� ���⿡ �ش��Ѵ�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmoUnnesting::isRemoveSemiJoin( qcStatement  * aSQStatement,
                                       qmsParseTree * aSQParseTree )
{
/***********************************************************************
 *
 * Description : BUG-45172
 *     semi ������ ������ ������ �˻��Ͽ� flag�� ������ �д�.
 *     ���� ���������� semi �����̸�
 *     ���� semi ���ο� flag�� �����Ǿ��� ��� ���� semi ������ �����Ѵ�.
 *
 * Implementation :
 *              1. union x, target 1
 *              2. view x, group x, aggr x, having x, ansi x
 *              3. ���̺��� 1���� ��� ���� ����
 *              4. ���̺��� 2���� ���
 *                  2���� ���̺��� 1���� ���̺��� 1row �� ����ɶ�
 ***********************************************************************/

    idBool         sResult = ID_FALSE;
    qmsSFWGH     * sSFWGH;
    qmsFrom      * sFrom;
    qcmTableInfo * sTableInfo;
    UInt           i;
    UShort         sTableCount;

    if ( aSQParseTree->querySet->setOp != QMS_NONE )
    {
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        sSFWGH     = aSQParseTree->querySet->SFWGH;
    }

    // ���� ���� üũ
    if ( ( sSFWGH->from->joinType        == QMS_NO_JOIN ) &&
         ( sSFWGH->from->tableRef        != NULL ) &&
         ( sSFWGH->from->tableRef->view  == NULL ) &&
         ( sSFWGH->group                 == NULL ) &&
         ( sSFWGH->having                == NULL ) &&
         ( sSFWGH->aggsDepth1            == NULL ) )
    {
        if ( sSFWGH->from->next == NULL )
        {
            // ���̺��� 1���� ��� ���� ����
            sTableCount = 1;

            sResult = ID_TRUE;
        }
        else if ( ( sSFWGH->from->next                   != NULL ) &&
                  ( sSFWGH->from->next->joinType         == QMS_NO_JOIN ) &&
                  ( sSFWGH->from->next->tableRef         != NULL ) &&
                  ( sSFWGH->from->next->tableRef->view   == NULL ) &&
                  ( sSFWGH->from->next->next             == NULL ) )
        {
            // ���̺��� 2���� ���
            sTableCount = 2;
        }
        else
        {
            IDE_CONT( INVALID_FORM );
        }
    }
    else
    {
        IDE_CONT( INVALID_FORM );
    }

    // target �� 1�� �� ��츸
    if ( ( sSFWGH->target       != NULL ) &&
         ( sSFWGH->target->next == NULL ) )
    {
        // value or �÷�
        if ( ( QTC_IS_COLUMN( aSQStatement, sSFWGH->target->targetColumn ) == ID_TRUE ) ||
             ( sSFWGH->target->targetColumn->node.module == &qtc::valueModule ) )
        {
            // nothing to do.
        }
        else
        {
            IDE_CONT( INVALID_FORM );
        }
    }
    else
    {
        IDE_CONT( INVALID_FORM );
    }

    if ( ( sTableCount   == 2    ) &&
         ( sSFWGH->where != NULL ) )
    {
        for ( sFrom = sSFWGH->from;
              sFrom != NULL;
              sFrom = sFrom->next )
        {
            sTableInfo = sFrom->tableRef->tableInfo;
            
            for( i = 0; i < sTableInfo->indexCount; i++ )
            {
                if ( ( sTableInfo->indices[i].isUnique    == ID_TRUE ) &&
                     ( sTableInfo->indices[i].keyColCount == 1 ) )
                {
                    // if B.i1 is unique
                    // find A.i1 = B.i1 and A.i1 = ���
                    // find B.i1 = ���
                    if ( findUniquePredicate(
                                aSQStatement,
                                sSFWGH,
                                sFrom->tableRef->table,
                                sTableInfo->indices[i].keyColumns[0].column.id,
                                sSFWGH->where ) == ID_TRUE )
                    {
                        sResult = ID_TRUE;

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
            }
        }
    }
    else
    {
        // nothing to do.
    }

    IDE_EXCEPTION_CONT( INVALID_FORM );

    return sResult;
}

idBool qmoUnnesting::findUniquePredicate( qcStatement * aStatement,
                                          qmsSFWGH    * aSFWGH,
                                          UInt          aTableID,
                                          UInt          aUniqueID,
                                          qtcNode     * aNode )
{
    idBool    sResult = ID_FALSE;
    mtcNode * sFindColumn = NULL;
    mtcNode * sNode;
    mtcNode * sTemp;

    if ( aNode->node.module == &mtfAnd )
    {
        sNode = aNode->node.arguments;
    }
    else
    {
        sNode = (mtcNode*)aNode;
    }

    // unique = ���
    for ( sTemp = sNode; sTemp != NULL; sTemp = sTemp->next )
    {
        if( sTemp->module == &mtfEqual )
        {
            if ( ( sTemp->arguments->table == aTableID ) &&
                 ( QTC_STMT_COLUMN( aStatement, (qtcNode*)sTemp->arguments )->column.id == aUniqueID ) &&
                 ( sTemp->arguments->next->module == &qtc::valueModule ) )
            {
                sResult = ID_TRUE;
            }
            else if ( ( sTemp->arguments->next->table == aTableID ) &&
                      ( QTC_STMT_COLUMN( aStatement, (qtcNode*)sTemp->arguments->next )->column.id == aUniqueID ) &&
                      ( sTemp->arguments->module == &qtc::valueModule ) )
            {
                sResult = ID_TRUE;
            }
            else
            {
                // nothing to do.
            }
        }
        else
        {
            // nothing to do.
        }
    }

    // A.i1 = B.i1 and A.i1 = ���
    if ( sResult == ID_FALSE )
    {
        for ( sTemp = sNode; sTemp != NULL; sTemp = sTemp->next )
        {
            if( sTemp->module == &mtfEqual )
            {
                if ( ( qtc::dependencyEqual( &((qtcNode*)sTemp)->depInfo, &aSFWGH->depInfo ) == ID_TRUE ) &&
                     ( QTC_IS_COLUMN( aStatement, (qtcNode*)sTemp->arguments )               == ID_TRUE ) &&
                     ( QTC_IS_COLUMN( aStatement, (qtcNode*)sTemp->arguments->next )         == ID_TRUE ) &&
                     ( sTemp->arguments->table  != sTemp->arguments->next->table ) )
                {
                    if ( QTC_STMT_COLUMN( aStatement, (qtcNode*)sTemp->arguments )->column.id == aUniqueID )
                    {
                        sFindColumn = sTemp->arguments->next;
                    }
                    else if ( QTC_STMT_COLUMN( aStatement, (qtcNode*)sTemp->arguments->next )->column.id == aUniqueID )
                    {
                        sFindColumn = sTemp->arguments;
                    }
                    else
                    {
                        // nothing to do.
                    }
                }
                else
                {
                    // nothing to do.
                }
            }
            else
            {
                // nothing to do.
            }
        }
    }
    else
    {
        // nothing to do.
    }

    if ( sFindColumn != NULL )
    {
        for ( sTemp = sNode; sTemp != NULL; sTemp = sTemp->next )
        {
            if( sTemp->module == &mtfEqual )
            {
                if ( qtc::dependencyEqual( &((qtcNode*)sTemp)->depInfo,
                                           &((qtcNode*)sFindColumn)->depInfo ) == ID_TRUE )
                {
                    if ( ( sFindColumn->table  == sTemp->arguments->table  ) &&
                         ( sFindColumn->column == sTemp->arguments->column ) &&
                         ( sTemp->arguments->next->module == &qtc::valueModule ) )
                    {
                        sResult = ID_TRUE;
                    }
                    else if ( ( sFindColumn->table  == sTemp->arguments->next->table  ) &&
                              ( sFindColumn->column == sTemp->arguments->next->column ) &&
                              ( sTemp->arguments->module == &qtc::valueModule ) )
                    {
                        sResult = ID_TRUE;
                    }
                    else
                    {
                        // nothing to do.
                    }
                }
                else
                {
                    // nothing to do.
                }
            }
            else
            {
                // nothing to do.
            }
        }
    }
    else
    {
        // nothing to do.
    }

    return sResult;
}

void qmoUnnesting::removeDownSemiJoinFlag( qmsSFWGH * aSFWGH )
{
    qmsTarget   * sTarget   = aSFWGH->target;
    qtcNode     * sNode     = aSFWGH->where;
    qtcNode     * sTemp;

    if ( sNode != NULL )
    {
        if ( sNode->node.module == &mtfAnd )
        {
            sNode = (qtcNode*)sNode->node.arguments;
        }
        else
        {
            sNode = sNode;
        }

        // target = ��� ������ �����ϸ� semi ������ �������� �ʴ´�.
        for ( ; sTarget != NULL; sTarget = sTarget->next )
        {
            for ( sTemp = sNode; sTemp != NULL; sTemp = (qtcNode*)sTemp->node.next )
            {
                if ( sTemp->node.module == &mtfEqual )
                {
                    if ( ( sTarget->targetColumn->node.table == sTemp->node.arguments->table ) &&
                         ( sTarget->targetColumn->node.column == sTemp->node.arguments->column ) &&
                         ( sTemp->node.arguments->next->module == &qtc::valueModule ) )
                    {
                        break;
                    }
                    else if ( ( sTarget->targetColumn->node.table == sTemp->node.arguments->next->table ) &&
                              ( sTarget->targetColumn->node.column == sTemp->node.arguments->next->column ) &&
                              ( sTemp->node.arguments->module == &qtc::valueModule ) )
                    {
                        break;
                    }
                    else
                    {
                        // nothing to do.
                    }
                }
                else
                {
                    // nothing to do.
                }
            }
            
            if ( sTemp != NULL )
            {
                break;
            }
            else
            {
                // nothing to do.
            }
        }

        if ( sTarget == NULL )
        {
            for ( sTemp = sNode; sTemp != NULL; sTemp = (qtcNode*)sTemp->node.next )
            {
                if ( ((sTemp->lflag & QTC_NODE_REMOVABLE_SEMI_JOIN_MASK) == QTC_NODE_REMOVABLE_SEMI_JOIN_TRUE) &&
                     ((sTemp->lflag & QTC_NODE_JOIN_TYPE_MASK) == QTC_NODE_JOIN_TYPE_SEMI) )
                {
                    sTemp->lflag &= ~QTC_NODE_JOIN_TYPE_MASK;
                }
                else
                {
                    // nothing to do.
                }
            }
        }
        else
        {
            // nothing to do.
        }
    }
    else
    {
        // nothing to do.
    }
}

IDE_RC
qmoUnnesting::makeDummyConstant( qcStatement  * aStatement,
                                 qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description :
 *     CHAR type�� '0'���� ���� constant node�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcNamePosition   sPosition;
    qtcNode        * sConstNode[2];

    IDU_FIT_POINT_FATAL( "qmoUnnesting::makeDummyConstant::__FT__" );

    sPosition.stmtText = (SChar*)"'0'";
    sPosition.offset   = 0;
    sPosition.size     = 3;

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

    *aResult = sConstNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::removePassNode( qcStatement * aStatement,
                              qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     HAVING���� predicate�� WHERE���� �ű�� ���� HAVING���� ���Ե�
 *     pass node���� ��� �����Ѵ�.
 *
 * Implementation :
 *     Pass node�� child�� pass node�� �ִ� ��ġ�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sArg;
    qtcNode * sNode;
    mtcNode * sNext;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::removePassNode::__FT__" );

    if( aNode->node.module == &qtc::passModule )
    {
        sNext = aNode->node.next;

        if( QTC_IS_TERMINAL( (qtcNode *)aNode->node.arguments ) == ID_TRUE )
        {
            sNode = (qtcNode *)aNode->node.arguments;
        }
        else
        {
            IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                             (qtcNode *)aNode->node.arguments,
                                             &sNode,
                                             ID_FALSE,
                                             ID_FALSE,
                                             ID_FALSE,
                                             ID_FALSE )
                      != IDE_SUCCESS );
        }
        idlOS::memcpy( aNode, sNode, ID_SIZEOF( qtcNode ) );
        aNode->node.next = sNext;
    }
    else
    {
        for( sArg = (qtcNode *)aNode->node.arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next )
        {
            IDE_TEST( removePassNode( aStatement,
                                      sArg )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::setDummySelect( qcStatement * aStatement,
                              qtcNode     * aNode,
                              idBool        aCheckAggregation )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� SELECT���� ������ ���� �����Ѵ�.
 *     SELECT DISTINCT i1, i2 FROM t1 WHERE ...
 *     => SELECT '0' FROM t1 WHERE ...
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode        * sConstNode = NULL;
    qmsTarget      * sTarget;
    qmsParseTree   * sParseTree;
    qcStatement    * sSQStatement;
    idBool           sChange = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::setDummySelect::__FT__" );

    IDE_FT_ERROR( aNode->node.arguments->module == &qtc::subqueryModule );

    sSQStatement = ((qtcNode *)aNode->node.arguments)->subquery;
    sParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;

    /* TASK-7219 Shard Transformer Refactoring */
    if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                        SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
         == ID_TRUE )
    {
        IDE_TEST( sdi::preAnalyzeQuerySet( sSQStatement,
                                           sParseTree->querySet,
                                           QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if( sParseTree->querySet->setOp == QMS_NONE )
    {
        if( ( aNode->node.module == &mtfExists ) ||
            ( aNode->node.module == &mtfNotExists ) )
        {
            // DISTINCT�� �ʿ� ����.
            sParseTree->querySet->SFWGH->selectType = QMS_ALL;
            sChange = ID_TRUE;

            if( aCheckAggregation == ID_TRUE )
            {
                for( sTarget = sParseTree->querySet->target;
                     sTarget != NULL;
                     sTarget = sTarget->next )
                {
                    // EXISTS(SELECT COUNT(...), ...) �� ��� ����� ��ȯ �� ����� �޶��� �� �ִ�.
                    if( ( sTarget->targetColumn->lflag & QTC_NODE_AGGREGATE_MASK )
                        == QTC_NODE_AGGREGATE_EXIST )
                    {
                        sChange = ID_FALSE;
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
                // Nothing to do.
            }
        }
        else
        {
            // UNIQUE�ÿ��� DISTINCT���� ���ܾ� �Ѵ�.
            if( sParseTree->querySet->SFWGH->selectType == QMS_ALL )
            {
                sChange = ID_TRUE;
            }
            else
            {
                sChange = ID_FALSE;
            }
        }

        if( sChange == ID_TRUE )
        {
            // BUG-45591 aggsDepth1 ����
            // target �� ������ �϶� ��� ���� �������� üũ�ؾ� �Ѵ�.
            for ( sTarget = sParseTree->querySet->target;
                  sTarget != NULL;
                  sTarget = sTarget->next )
            {
                delAggrNode( sParseTree->querySet->SFWGH, sTarget->targetColumn );
            }

            // BUG-45271 exists ��ȯ�� ������ �߻��մϴ�.
            // exists ��ȯ�� target �� ���縦 ������ ������ ���� �������� �ʴ´�.
            // �߸��� ������ �����ϴ� ������ �߻���ŵ�ϴ�.
            // exists ��ȯ�� target �� �ǹ̰� �����Ƿ� ������ �����ϰ� ���� �����մϴ�.
            IDE_TEST( makeDummyConstant( sSQStatement,
                                         & sConstNode )
                      != IDE_SUCCESS );

            sTarget = sParseTree->querySet->target;

            QMS_TARGET_INIT( sTarget );

            sTarget->targetColumn = sConstNode;
            sTarget->flag        &= ~QMS_TARGET_IS_NULLABLE_MASK;
            sTarget->flag        |= QMS_TARGET_IS_NULLABLE_TRUE;
            sTarget->next         = NULL;

            aNode->node.arguments->arguments = (mtcNode *)sTarget->targetColumn;
        }
        else
        {
            // Nothing to do.
        }

        if( ( sParseTree->querySet->SFWGH->aggsDepth1 == NULL ) &&
            ( sParseTree->querySet->SFWGH->aggsDepth2 == NULL ) &&
            ( sParseTree->querySet->SFWGH->group != NULL ) )
        {
            // GROUP BY���� �����Ѵ�.
            sParseTree->querySet->SFWGH->group = NULL;

            if( sParseTree->querySet->SFWGH->having != NULL )
            {
                IDE_TEST( removePassNode( aStatement,
                                          sParseTree->querySet->SFWGH->having )
                          != IDE_SUCCESS );

                // HAVING���� WHERE���� �� ���δ�.
                IDE_TEST( concatPredicate( sSQStatement,
                                           sParseTree->querySet->SFWGH->where,
                                           sParseTree->querySet->SFWGH->having,
                                           &sParseTree->querySet->SFWGH->where )
                          != IDE_SUCCESS );

                sParseTree->querySet->SFWGH->having = NULL;
            }
            else
            {
                // Nothin to do.
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // SET ������ ���
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::concatPredicate( qcStatement  * aStatement,
                               qtcNode      * aPredicate1,
                               qtcNode      * aPredicate2,
                               qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description :
 *     �ΰ��� predicate�� conjunctive form���� �����Ѵ�.
 *
 * Implementation :
 *     �� �� �Ѱ��� AND�� �����ϴ� ��� �̸� Ȱ���Ѵ�.
 *     ��� ���ʿ��� AND�� �������� ������ ���� �����Ѵ�.
 *
 ***********************************************************************/

    qcNamePosition   sEmptyPosition;
    qtcNode        * sANDNode[2];
    qtcNode        * sArg;
    UInt             sArgCount1 = 0;
    UInt             sArgCount2 = 0;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::concatPredicate::__FT__" );

    IDE_DASSERT( aPredicate2 != NULL );

    if( aPredicate1 == NULL )
    {
        *aResult = aPredicate2;
    }
    else
    {
        sArgCount1 = ( aPredicate1->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );
        sArgCount2 = ( aPredicate2->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );

        if( ( aPredicate1->node.module == &mtfAnd ) &&
            ( sArgCount1 < MTC_NODE_ARGUMENT_COUNT_MAXIMUM ) )
        {
            // aPredicate1�� AND�� ���

            // AND�� ������ argument�� ã�´�.
            sArg = (qtcNode *)aPredicate1->node.arguments;
            while( sArg->node.next != NULL )
            {
                sArg = (qtcNode *)sArg->node.next;
            }

            if( ( aPredicate2->node.module == &mtfAnd ) &&
                ( sArgCount1 + sArgCount2 <= MTC_NODE_ARGUMENT_COUNT_MAXIMUM ) )
            {
                // aPredicate2�� AND�� ���� ��� argument�鳢�� �����Ѵ�.
                sArg->node.next = aPredicate2->node.arguments;
                aPredicate1->node.lflag += sArgCount2;
            }
            else
            {
                // aPredicate2�� AND�� �ƴ� ��� ���� �����Ѵ�.
                sArg->node.next = (mtcNode *)aPredicate2;
                aPredicate1->node.lflag++;
            }

            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        aPredicate1 )
                      != IDE_SUCCESS );

            *aResult = aPredicate1;
        }
        else
        {
            if( ( aPredicate2->node.module == &mtfAnd ) &&
                ( sArgCount2 < MTC_NODE_ARGUMENT_COUNT_MAXIMUM ) )
            {
                // aPredicate2�� AND�� ���

                // AND�� ������ argument�� ã�´�.
                sArg = (qtcNode *)aPredicate2->node.arguments;
                while( sArg->node.next != NULL )
                {
                    sArg = (qtcNode *)sArg->node.next;
                }

                sArg->node.next = (mtcNode *)aPredicate1;
                aPredicate2->node.lflag++;

                IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                            aPredicate2 )
                          != IDE_SUCCESS );

                *aResult = aPredicate2;
            }
            else
            {
                SET_EMPTY_POSITION( sEmptyPosition );

                // ��� ���ʵ� AND�� �������� �ʴ� ���
                IDE_TEST( qtc::makeNode( aStatement,
                                         sANDNode,
                                         &sEmptyPosition,
                                         &mtfAnd )
                          != IDE_SUCCESS );

                sANDNode[0]->node.arguments       = (mtcNode *)aPredicate1;
                sANDNode[0]->node.arguments->next = (mtcNode *)aPredicate2;

                IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                            sANDNode[0] )
                          != IDE_SUCCESS );

                sANDNode[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
                sANDNode[0]->node.lflag |= 2;

                // AND node�� ���� ����� ��ȯ�Ѵ�.
                *aResult = sANDNode[0];
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoUnnesting::isSubqueryPredicate( qtcNode * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� =ANY, =ALL ���� quantified predicate�̸�
 *     �ι�° ���ڰ� subquery�� ��쿡�� true�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    if( ( aPredicate->node.lflag & MTC_NODE_COMPARISON_MASK )
        == MTC_NODE_COMPARISON_TRUE )
    {
        switch( aPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
        {
            case MTC_NODE_OPERATOR_EQUAL:
            case MTC_NODE_OPERATOR_NOT_EQUAL:
            case MTC_NODE_OPERATOR_LESS:
            case MTC_NODE_OPERATOR_LESS_EQUAL:
            case MTC_NODE_OPERATOR_GREATER:
            case MTC_NODE_OPERATOR_GREATER_EQUAL:
                IDE_DASSERT( aPredicate->node.arguments       != NULL );
                IDE_DASSERT( aPredicate->node.arguments->next != NULL );

                if( aPredicate->node.arguments->next->module == &qtc::subqueryModule )
                {
                    return ID_TRUE;
                }
                else
                {
                    // Nothing to do.
                }
                break;
            default:
                break;
        }
    }
    else
    {
        // Nothing to do.
    }

    return ID_FALSE;
}

idBool
qmoUnnesting::isQuantifiedSubquery( qtcNode * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� =ANY, =ALL ���� quantified predicate�̸�
 *     �ι�° ���ڰ� subquery�� ��쿡�� true�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool sResult = ID_FALSE;

    if( ( aPredicate->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK )
        == MTC_NODE_GROUP_COMPARISON_TRUE )
    {
        IDE_DASSERT( aPredicate->node.arguments       != NULL );
        IDE_DASSERT( aPredicate->node.arguments->next != NULL );

        if( aPredicate->node.arguments->next->module == &qtc::subqueryModule )
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

    return sResult;
}

idBool
qmoUnnesting::isNullable( qcStatement * aStatement,
                          qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     Nullable�� expression���� Ȯ���Ѵ�.
 *     ���� �ΰ��� ��츦 �����ϰ� ��� nullable�̴�.
 *     1. Column�̸鼭 not nullable constraint�� ������ ���
 *     2. ����̸鼭 null�� �ƴ� ���
 *
 * Implementation :
 *
 ***********************************************************************/

    mtcColumn * sColumn;
    qtcNode   * sNode;
    UChar     * sValue;
    idBool      sResult = ID_TRUE;

    if( aNode->node.module == &qtc::passModule )
    {
        sNode = (qtcNode *)aNode->node.arguments;
    }
    else
    {
        sNode = aNode;
    }

    if( sNode->node.module == &qtc::valueModule )
    {
        if( ( QTC_STMT_TUPLE( aStatement, sNode )->lflag & MTC_TUPLE_TYPE_MASK )
            == MTC_TUPLE_TYPE_CONSTANT )
        {
            // ����� ���
            sColumn = QTC_STMT_COLUMN( aStatement, sNode );
            sValue  = (UChar *)QTC_STMT_TUPLE( aStatement, sNode )->row + sColumn->column.offset;
            if( sColumn->module->isNull( sColumn, 
                                         sValue )
                == ID_FALSE )
            {
                // ��� ���� null�� �ƴ� ���
                sResult = ID_FALSE;
            }
            else
            {
                // ��� ���� null�� ���
            }
        }
        else
        {
            // Bind ������ ���
        }
    }
    else
    {
        if( ( QTC_STMT_COLUMN( aStatement, sNode )->flag & MTC_COLUMN_NOTNULL_MASK )
            == MTC_COLUMN_NOTNULL_FALSE )
        {
            // Nullable column�� ���
        }
        else
        {
            // Not nullable column�� ���
            sResult = ID_FALSE;
        }
    }

    return sResult;
}

IDE_RC
qmoUnnesting::genCorrPredicate( qcStatement  * aStatement,
                                qtcNode      * aPredicate,
                                qtcNode      * aOperand1,
                                qtcNode      * aOperand2,
                                idBool         aExistsTrans,
                                qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description : 
 *     Subquery predicate�� ������ ���� correlation predicate�� �����Ѵ�.
 *
 * Implementation :
 *     ������ table�� ���� predicate�� ��ȯ�Ѵ�.
 *     | Input | Not nullable | Nullable      |
 *     | =ANY  | a = b        | a = b         |
 *     | <>ANY | a <> b       | a <> b        |
 *     | >ANY  | a > b        | a > b         |
 *     | >=ANY | a >= b       | a >= b        |
 *     | <ANY  | a < b        | a < b         |
 *     | <=ANY | a <= b       | a <= b        |
 *     | =ALL  | a <> b       | LNNVL(a = b)  |
 *     | <>ALL | a = b        | LNNVL(a <> b) |
 *     | >ALL  | a <= b       | LNNVL(a > b)  |
 *     | >=ALL | a < b        | LNNVL(a >= b) |
 *     | <ALL  | a >= b       | LNNVL(a < b)  |
 *     | <=ALL | a > b        | LNNVL(a <= b) |
 *
 *     subquery removal�� ��� ���� tabel�� ���� predicate�� ��ȯ�Ѵ�.
 *     | Input         || correlation predicate
 *     | =ANY  | =ALL  || a = b        
 *     | <>ANY | <>ALL || a <> b       
 *     | >ANY  | >ALL  || a > b        
 *     | >=ANY | >=ALL || a >= b       
 *     | <ANY  | <ALL  || a < b        
 *     | <=ANY | <=ALL || a <= b       
 *
 ***********************************************************************/

    qtcNode        * sLnnvlNode[2];
    qtcNode        * sPredicate[2];
    qtcNode        * sOperand1;
    qtcNode        * sOperand2;
    qmsTableRef    * sTableRef;
    qcNamePosition   sEmptyPosition;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genCorrPredicate::__FT__" );

    SET_EMPTY_POSITION( sEmptyPosition );

    // Predicate�� operand ����
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                               (void **)&sOperand1 )
              != IDE_SUCCESS );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                               (void **)&sOperand2 )
              != IDE_SUCCESS );

    idlOS::memcpy( sOperand1, aOperand1, ID_SIZEOF( qtcNode ) );
    idlOS::memcpy( sOperand2, aOperand2, ID_SIZEOF( qtcNode ) );

    sOperand1->node.next = (mtcNode *)sOperand2;
    sOperand2->node.next = NULL;

    if( QTC_IS_COLUMN( aStatement, sOperand1 ) == ID_TRUE )
    {
        // Outer query�� column�� ���Ͽ� table alias�� �׻� �������ش�.
        sTableRef = QC_SHARED_TMPLATE( aStatement )->tableMap[sOperand1->node.table].from->tableRef;
        if( QC_IS_NULL_NAME( sOperand1->tableName ) == ID_TRUE )
        {
            SET_POSITION( sOperand1->tableName, sTableRef->aliasName );
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

    if( ( ( aPredicate->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) == MTC_NODE_GROUP_COMPARISON_TRUE ) &&
        ( ( aPredicate->node.lflag & MTC_NODE_GROUP_MASK ) == MTC_NODE_GROUP_ALL ) &&
        ( aExistsTrans == ID_TRUE ) )
    {
        // Subquery predicate�� ALL�迭�� ���
        if( ( isNullable( aStatement, sOperand1 ) == ID_TRUE ) ||
            ( isNullable( aStatement, sOperand2 ) == ID_TRUE ) )
        {
            // Operand�� nullable�� ��� LNNVL�� �ʿ��ϴ�.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sPredicate,
                                     &sEmptyPosition,
                                     toNonQuantifierModule( aPredicate->node.module ) )
                      != IDE_SUCCESS );

            sPredicate[0]->node.arguments = (mtcNode *)sOperand1;
            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        sPredicate[0] )
                      != IDE_SUCCESS );

            IDE_TEST( qtc::makeNode( aStatement,
                                     sLnnvlNode,
                                     &sEmptyPosition,
                                     &mtfLnnvl )
                      != IDE_SUCCESS );

            sLnnvlNode[0]->node.arguments = (mtcNode *)sPredicate[0];
            *aResult = sLnnvlNode[0];
        }
        else
        {
            // Operand�� ��� not nullable�� ��� counter operator�� �����Ѵ�.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sPredicate,
                                     &sEmptyPosition,
                                     (mtfModule *)toNonQuantifierModule( aPredicate->node.module )->counter )
                      != IDE_SUCCESS );

            sPredicate[0]->node.arguments = (mtcNode *)sOperand1;
            *aResult = sPredicate[0];
        }
    }
    else
    {
        // Subquery predicate�� ANY�迭�� ���
        IDE_TEST( qtc::makeNode( aStatement,
                                 sPredicate,
                                 &sEmptyPosition,
                                 toNonQuantifierModule( aPredicate->node.module ) )
                  != IDE_SUCCESS );

        sPredicate[0]->node.arguments = (mtcNode *)sOperand1;
        *aResult = sPredicate[0];
    }

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                *aResult )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::genCorrPredicates( qcStatement  * aStatement,
                                 qtcNode      * aNode,
                                 idBool         aExistsTrans,
                                 qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description :
 *     Quantified subquery predicate���κ��� correlation predicate��
 *     �����Ѵ�.
 *     ex) (t1.i1, t1.i2) IN (SELECT t2.i1, t2.i2 FROM t2 ...)
 *         => t1.i1 = t2.i1 AND t1.i2 = t2.i2
 *
 * Implementation :
 *     1. Predicate�� ù ��° operator�� list�� ���
 *        List�� �� element�鿡 ���� correlation predicate�� ���� ��
 *        AND�� ���� �Ͽ� ��ȯ�Ѵ�.
 *     2. Predicate�� ù ��° operator�� single value�� ���
 *        Correlation predicate �ϳ��� �����Ͽ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qtcNode        * sConnectorNode[2];
    qtcNode        * sFirst = NULL;
    qtcNode        * sLast = NULL;
    qtcNode        * sResult;
    qtcNode        * sArg;
    mtfModule      * sConnectorModule;
    UInt             sArgCount = 0;
    qmsTarget      * sSQTarget;
    qcStatement    * sSQStatement;
    qcNamePosition   sEmptyPosition;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genCorrPredicates::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );
    IDE_DASSERT( aNode->node.arguments != NULL );
    IDE_DASSERT( aNode->node.arguments->next->module == &qtc::subqueryModule );

    sSQStatement = ((qtcNode *)aNode->node.arguments->next)->subquery;
    sSQTarget    = ((qmsParseTree*)sSQStatement->myPlan->parseTree)->querySet->SFWGH->target;

    if( aNode->node.arguments->module == &mtfList )
    {
        // List�� ���
        for( sArg = (qtcNode *)aNode->node.arguments->arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next, sSQTarget = sSQTarget->next )
        {
            IDE_TEST( genCorrPredicate( aStatement,
                                        aNode,
                                        sArg,
                                        sSQTarget->targetColumn,
                                        aExistsTrans,
                                        &sResult )
                      != IDE_SUCCESS );

            // ������ predicate�� �����Ѵ�.
            if( sFirst == NULL )
            {
                sFirst = sLast = sResult;
            }
            else
            {
                sLast->node.next = (mtcNode *)sResult;
            }

            while( sLast->node.next != NULL )
            {
                sLast = (qtcNode *)sLast->node.next;
            }

            sArgCount++;
        }

        SET_EMPTY_POSITION( sEmptyPosition );

        if( ( aNode->node.module == &mtfNotEqual ) ||
            ( aNode->node.module == &mtfNotEqualAny ) ||
            ( aNode->node.module == &mtfEqualAll ) )
        {
            sConnectorModule = &mtfOr;
        }
        else
        {
            sConnectorModule = &mtfAnd;
        }

        IDE_TEST( qtc::makeNode( aStatement,
                                 sConnectorNode,
                                 &sEmptyPosition,
                                 sConnectorModule )
                  != IDE_SUCCESS );

        sConnectorNode[0]->node.arguments = (mtcNode *)sFirst;

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sConnectorNode[0] )
                  != IDE_SUCCESS );

        sConnectorNode[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
        sConnectorNode[0]->node.lflag |= (aNode->node.arguments->lflag & MTC_NODE_ARGUMENT_COUNT_MASK);

        *aResult = sConnectorNode[0];
    }
    else
    {
        // Single value ���
        IDE_TEST( genCorrPredicate( aStatement,
                                    aNode,
                                    (qtcNode *)aNode->node.arguments,
                                    sSQTarget->targetColumn,
                                    aExistsTrans,
                                    &sResult )
                  != IDE_SUCCESS );

        *aResult = sResult;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

mtfModule *
qmoUnnesting::toExistsModule( const mtfModule * aQuantifier )
{
/***********************************************************************
 *
 * Description :
 *     Subquery predicate�� ���� �������� ������ ���� predicate��
 *     ��ȯ �� ����� �����ڸ� ��ȯ�Ѵ�.
 *
 * Implementation :
 *     ANY �迭 ������: EXISTS
 *     ALL �迭 ������: NOT EXISTS
 *
 ***********************************************************************/

    mtfModule * sResult;

    if( ( ( aQuantifier->lflag & MTC_NODE_GROUP_COMPARISON_MASK ) == MTC_NODE_GROUP_COMPARISON_TRUE ) &&
        ( ( aQuantifier->lflag & MTC_NODE_GROUP_MASK ) == MTC_NODE_GROUP_ALL ) )
    {
        sResult = &mtfNotExists;
    }
    else
    {
        sResult = &mtfExists;
    }

    return sResult;
}

mtfModule * qmoUnnesting::toExistsModule4CountAggr( qcStatement * aStatement,
                                                    qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : aggr �Լ��� count �̸鼭 group by �� ������� �ʴ� ���
 *               ���ϴ� ���� 0,1 �϶��� unnset �� �����ϴ�.
 *               �� �Լ������� ���� 0,1 ���� üũ�ϰ�
 *               ��츶�� ��밡���� exists ����� ��ȯ���ش�.
 *
 ***********************************************************************/

    qcTemplate  * sTemplate;
    mtcNode     * sNode = (mtcNode*)aNode;
    mtcNode     * sValueNode;
    mtcColumn   * sColumn;
    mtcTuple    * sTuple;
    SLong         sValue  = -1;
    SChar       * sValueTemp;
    mtfModule   * sResult = NULL;

    sTemplate = QC_SHARED_TMPLATE(aStatement);

    // ���ε� ������ ���� ����ɼ� �ֱ� ������ ��ȯ�ؼ��� �ȵȴ�.
    if ( (sNode->arguments->module == &qtc::valueModule) &&
         (sNode->arguments->lflag & MTC_NODE_BIND_MASK) == MTC_NODE_BIND_ABSENT )
    {
        sValueNode = mtf::convertedNode(
            sNode->arguments,
            (mtcTemplate*)sTemplate );

        sTuple     = QTC_TMPL_TUPLE(sTemplate, (qtcNode*)sValueNode);
        sColumn    = QTC_TUPLE_COLUMN( sTuple, (qtcNode*)sValueNode);

        sValueTemp = (SChar*)mtc::value( sColumn,
                                         sTuple->row,
                                         MTD_OFFSET_USE );

        if( sColumn->type.dataTypeId == MTD_BIGINT_ID )
        {
            sValue  = *((SLong*)sValueTemp);
        }
        else if( (sColumn->type.dataTypeId == MTD_NUMERIC_ID) ||
                 (sColumn->type.dataTypeId == MTD_FLOAT_ID) )
        {
            if ( mtv::numeric2NativeN( (mtdNumericType*)sValueTemp,
                                       &sValue) != IDE_SUCCESS )
            {
                sValue  = -1;
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

    switch( sValue )
    {
        case 0:
            if ( (sNode->module == &mtfEqual)    ||
                 (sNode->module == &mtfEqualAny) ||
                 (sNode->module == &mtfEqualAll) )
            {
                sResult = &mtfNotExists;
            }
            else if ( (sNode->module == &mtfGreaterEqual)    ||
                      (sNode->module == &mtfGreaterEqualAny) ||
                      (sNode->module == &mtfGreaterEqualAll) )
            {
                sResult = &mtfNotExists;
            }
            else if ( (sNode->module == &mtfLessThan)    ||
                      (sNode->module == &mtfLessThanAny) ||
                      (sNode->module == &mtfLessThanAll) )
            {
                sResult = &mtfExists;
            }
            else if ( (sNode->module == &mtfNotEqual)    ||
                      (sNode->module == &mtfNotEqualAny) ||
                      (sNode->module == &mtfNotEqualAll) )
            {
                sResult = &mtfExists;
            }
            else
            {
                sResult = NULL;
            }
            break;

        case 1:
            if ( (sNode->module == &mtfLessEqual)    ||
                 (sNode->module == &mtfLessEqualAny) ||
                 (sNode->module == &mtfLessEqualAll) )
            {
                sResult = &mtfExists;
            }
            else if ( (sNode->module == &mtfGreaterThan)    ||
                      (sNode->module == &mtfGreaterThanAny) ||
                      (sNode->module == &mtfGreaterThanAll) )
            {
                sResult = &mtfNotExists;
            }
            else
            {
                sResult = NULL;
            }
            break;

        default :
            sResult = NULL;
            break;
    }

    return sResult;
}

mtfModule *
qmoUnnesting::toNonQuantifierModule( const mtfModule * aQuantifier )
{
/***********************************************************************
 *
 * Description :
 *     Subquery predicate�� ���� �������� ������ ����
 *     correlation predicate���� ����� �����ڸ� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    mtfModule * sResult = NULL;

    switch( aQuantifier->lflag & MTC_NODE_OPERATOR_MASK )
    {
        case MTC_NODE_OPERATOR_EQUAL:
            sResult = &mtfEqual;
            break;

        case MTC_NODE_OPERATOR_NOT_EQUAL:
            sResult = &mtfNotEqual;
            break;

        case MTC_NODE_OPERATOR_GREATER:
            sResult = &mtfGreaterThan;
            break;

        case MTC_NODE_OPERATOR_GREATER_EQUAL:
            sResult = &mtfGreaterEqual;
            break;

        case MTC_NODE_OPERATOR_LESS:
            sResult = &mtfLessThan;
            break;

        case MTC_NODE_OPERATOR_LESS_EQUAL:
            sResult = &mtfLessEqual;
            break;

        default:
            IDE_FT_ASSERT( 0 );
    }

    return sResult;
}

idBool
qmoUnnesting::isSingleRowSubquery( qcStatement * aSQStatement )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� ����� 1�� ���ϸ� �����ϴ��� Ȯ���Ѵ�.
 *
 * Implementation :
 *     Subquery�� ���� ���� �� �Ѱ����� �����ϸ� single row���� ������ �� �ִ�.
 *     1. GROUP BY���� ���� aggregate function�� ����� ���
 *     2. Nested aggregate function�� ����� ���
 *     3. WHERE���� unique key�� equal predicate�� ���Ե� ���
 *     4. HAVING���� GROUP BY���� expression��� equal predicate��
 *        ���Ե� ���
 *
 ***********************************************************************/

    qmsParseTree * sParseTree;
    qmsSFWGH     * sSFWGH;
    qcmTableInfo * sTableInfo;
    UInt           i;

    sParseTree = (qmsParseTree *)aSQStatement->myPlan->parseTree;

    if( sParseTree->querySet->setOp == QMS_NONE )
    {
        sSFWGH     = sParseTree->querySet->SFWGH;

        // 1. GROUP BY���� ���� aggregate function�� ����� ���
        if( ( sSFWGH->aggsDepth1 != NULL ) &&
            ( sSFWGH->group == NULL ) )
        {
            return ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        // 2. Nested aggregate function�� ����� ���
        if( sSFWGH->aggsDepth2 != NULL )
        {
            return ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        // 3. WHERE���� unique key�� equal predicate�� ���Ե� ���
        // Unique key constraint ������ 1�� table�� ������ Ȯ���Ѵ�.
        if( ( sSFWGH->from->joinType == QMS_NO_JOIN ) &&
            ( sSFWGH->from->next     == NULL ) &&
            ( sSFWGH->where          != NULL ) )
        {
            // View�� �ƴϾ�� �Ѵ�.
            if( sSFWGH->from->tableRef->view == NULL )
            {
                sTableInfo = sSFWGH->from->tableRef->tableInfo;

                // BUG-45168 unique index�� ����ؼ� subquery�� INNER JOIN���� ��ȯ�� �� �־�� �մϴ�.
                // unique �������� ��ſ� unique index�� ����ؼ� üũ�ϵ��� ����
                for( i = 0; i < sTableInfo->indexCount; i++ )
                {
                    if ( sTableInfo->indices[i].isUnique == ID_TRUE )
                    {
                        if( containsUniqueKeyPredicate( aSQStatement,
                                                        sSFWGH->where,
                                                        sSFWGH->from->tableRef->table,
                                                        &sTableInfo->indices[i] )
                            == ID_TRUE )
                        {
                            return ID_TRUE;
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
                // Nothing to do.
            }
        }
        else
        {
            // Nothing to do.
        }

        // 4. HAVING���� GROUP BY���� expression��� equal predicate�� ���Ե� ���
        if( ( sSFWGH->group != NULL ) &&
            ( sSFWGH->having != NULL ) )
        {
            if( containsGroupByPredicate( sSFWGH ) == ID_TRUE )
            {
                return ID_TRUE;
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
        // SET ���� �ÿ��� �Ǵ� �Ұ�
    }

    return ID_FALSE;
}

idBool
qmoUnnesting::containsUniqueKeyPredicate( qcStatement * aStatement,
                                          qtcNode     * aPredicate,
                                          UInt          aJoinTable,
                                          qcmIndex    * aUniqueIndex )
{
/***********************************************************************
 *
 * Description :
 *     �־��� unique key constraint�� column��� equal predicate��
 *     �����ϴ��� Ȯ���Ѵ�.
 *
 * Implementation :
 *     findUniqueKeyPredicate()�� ȣ���� ��, unique key�� column����
 *     ��� ���ԵǾ����� Ȯ���Ѵ�.
 *
 ***********************************************************************/

    UChar sRefVector[QC_MAX_KEY_COLUMN_COUNT];
    UInt  i;

    // Vector�� �ʱ�ȭ
    idlOS::memset( sRefVector, 0, ID_SIZEOF(sRefVector) );

    findUniqueKeyPredicate( aStatement, aPredicate, aJoinTable, aUniqueIndex, sRefVector );

    // Unique key constraint�� column���� ��� equi-join�Ǿ����� Ȯ��
    for( i = 0; i < aUniqueIndex->keyColCount; i++ )
    {
        if( sRefVector[i] == 0 )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if( i == aUniqueIndex->keyColCount )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

void
qmoUnnesting::findUniqueKeyPredicate( qcStatement * aStatement,
                                      qtcNode     * aPredicate,
                                      UInt          aJoinTable,
                                      qcmIndex    * aUniqueIndex,
                                      UChar       * aRefVector )
{
/***********************************************************************
 *
 * Description :
 *     Predicate�� unique key constraint�� column��� equalitiy predicate
 *     ���� ���θ� Ȯ���Ͽ� vector�� ǥ���Ѵ�.
 *
 * Implementation :
 *     AND ������ �������� = �����ڸ� ����ϴ� ��츸 �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qcDepInfo   sDepInfo;
    UInt        sUniqueKeyColumn;

    qtc::dependencySet( aJoinTable, &sDepInfo );

    if( qtc::dependencyContains( &aPredicate->depInfo, &sDepInfo ) == ID_TRUE )
    {
        // BUG-44988 SingleRowSubquery �� �߸� �Ǵ��ϰ� ����
        if( aPredicate->node.module == &mtfAnd )
        {
            for( sArg = (qtcNode *)aPredicate->node.arguments;
                 sArg != NULL;
                 sArg = (qtcNode *)sArg->node.next )
            {
                findUniqueKeyPredicate( aStatement,
                                        sArg,
                                        aJoinTable,
                                        aUniqueIndex,
                                        aRefVector );
            }
        }
        else if( aPredicate->node.module == &mtfEqual )
        {
            // Equal���� Ȯ���Ѵ�.
            if( (qtc::dependencyEqual( &sDepInfo, &(((qtcNode*)aPredicate->node.arguments)->depInfo) ) == ID_TRUE) &&
                (qtc::dependencyEqual( &sDepInfo, &(((qtcNode*)aPredicate->node.arguments->next)->depInfo) ) == ID_FALSE) )
            {
                // i1 = 1
                sUniqueKeyColumn =
                    findUniqueKeyColumn( aStatement,
                                         (qtcNode *)aPredicate->node.arguments,
                                         aJoinTable,
                                         aUniqueIndex );
                if( sUniqueKeyColumn != ID_UINT_MAX )
                {
                    aRefVector[sUniqueKeyColumn] = 1;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else if ( (qtc::dependencyEqual( &sDepInfo, &(((qtcNode*)aPredicate->node.arguments)->depInfo) ) == ID_FALSE) &&
                      (qtc::dependencyEqual( &sDepInfo, &(((qtcNode*)aPredicate->node.arguments->next)->depInfo) ) == ID_TRUE) )
            {
                // 1 = i1
                sUniqueKeyColumn =
                    findUniqueKeyColumn( aStatement,
                                         (qtcNode *)aPredicate->node.arguments->next,
                                         aJoinTable,
                                         aUniqueIndex );
                if( sUniqueKeyColumn != ID_UINT_MAX )
                {
                    aRefVector[sUniqueKeyColumn] = 1;
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // i1 = i2
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

UInt
qmoUnnesting::findUniqueKeyColumn( qcStatement * aStatement,
                                   qtcNode     * aNode,
                                   UInt          aTable,
                                   qcmIndex    * aUniqueIndex )
{
/***********************************************************************
 *
 * Description :
 *     Column�� unique key constraint�� � column�� �����ϴ��� ã�´�.
 *
 * Implementation :
 *     Unique key constraint�� column��� id�� ������ ���Ѵ�.
 *     ���� �� �ش� column�� index��, ���� �� UINT_MAX�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    UInt i;

    if( aNode->node.table == aTable )
    {
        for( i = 0; i < aUniqueIndex->keyColCount; i++ )
        {
            // BUG-45168 unique index�� ����ؼ� subquery�� INNER JOIN���� ��ȯ�� �� �־�� �մϴ�.
            // unique �������� ��ſ� unique index�� ����ؼ� üũ�ϵ��� ����
            if( QTC_STMT_COLUMN( aStatement, aNode )->column.id
                == aUniqueIndex->keyColumns[i].column.id )
            {
                return i;
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

    return ID_UINT_MAX;
}

idBool
qmoUnnesting::containsGroupByPredicate( qmsSFWGH * aSFWGH )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY���� ��� expression����� equal predicate�� HAVING����
 *     ���ԵǾ��ִ��� Ȯ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsConcatElement * sGroup;
    UChar sRefVector[128];
    UInt  sCount;
    UInt  i;

    for( sGroup = aSFWGH->group, sCount = 0;
         sGroup != NULL;
         sGroup = sGroup->next )
    {
        sCount++;
    }

    if( sCount > ID_SIZEOF( sRefVector ) )
    {
        // Vector ������� GROUP BY expression�� ������ �� ���� ���
        // Ȯ�� �Ұ�
    }
    else
    {
        // Vector�� �ʱ�ȭ
        idlOS::memset( sRefVector, 0, ID_SIZEOF(sRefVector) );

        findGroupByPredicate( aSFWGH, aSFWGH->group, aSFWGH->having, sRefVector );

        for( i = 0; i < sCount; i++ )
        {
            if( sRefVector[i] == 0 )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if( i == sCount )
        {
            IDE_CONT( APPLICABLE_EXIT );
        }
        else
        {
            // Nothing to do.
        }
    }

    return ID_FALSE;

    IDE_EXCEPTION_CONT( APPLICABLE_EXIT );

    return ID_TRUE;
}

void
qmoUnnesting::findGroupByPredicate( qmsSFWGH         * aSFWGH,
                                    qmsConcatElement * aGroup,
                                    qtcNode          * aPredicate,
                                    UChar            * aRefVector )
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY���� expression�� �����ϴ� predicate�� ã�� vector��
 *     marking �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode * sArg;
    UInt      sIndex;

    if( aPredicate->node.module == &mtfAnd )
    {
        for( sArg = (qtcNode *)aPredicate->node.arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next )
        {
            findGroupByPredicate( aSFWGH,
                                  aGroup,
                                  sArg,
                                  aRefVector );
        }
    }
    else if( aPredicate->node.module == &mtfEqual )
    {
        // BUG-44988 SingleRowSubquery �� �߸� �Ǵ��ϰ� ����
        if ( (qtc::haveDependencies( &(((qtcNode*)aPredicate->node.arguments)->depInfo) ) == ID_TRUE ) &&
             (qtc::dependencyContains( &aSFWGH->depInfo, &(((qtcNode*)aPredicate->node.arguments)->depInfo) ) == ID_TRUE) &&
             (qtc::dependencyContains( &aSFWGH->outerDepInfo, &(((qtcNode*)aPredicate->node.arguments->next)->depInfo) ) == ID_TRUE) )
        {
            // i1 = 1
            // i1 = t1.i1 ( outer column )
            sIndex = findGroupByExpression( aGroup, (qtcNode *)aPredicate->node.arguments );

            if( sIndex != ID_UINT_MAX )
            {
                aRefVector[sIndex] = 1;
            }
            else
            {
                // Nothing to do.
            }            
        }
        else if ( (qtc::haveDependencies( &(((qtcNode*)aPredicate->node.arguments->next)->depInfo) ) == ID_TRUE ) &&
                  (qtc::dependencyContains( &aSFWGH->outerDepInfo, &(((qtcNode*)aPredicate->node.arguments)->depInfo) ) == ID_TRUE) &&
                  (qtc::dependencyContains( &aSFWGH->depInfo, &(((qtcNode*)aPredicate->node.arguments->next)->depInfo) ) == ID_TRUE) )
        {
            // 1 = i1 
            // t1.i1(outer column) = i1
            sIndex = findGroupByExpression( aGroup,
                                            (qtcNode *)aPredicate->node.arguments->next );
            if( sIndex != ID_UINT_MAX )
            {
                aRefVector[sIndex] = 1;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // i1 = i2
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }
}

UInt
qmoUnnesting::findGroupByExpression( qmsConcatElement * aGroup,
                                     qtcNode          * aExpression )
{
/***********************************************************************
 *
 * Description :
 *     �־��� expression�� GROUP BY���� �� ��° expression���� �˻��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsConcatElement * sGroup;
    UInt sResult = ID_UINT_MAX;
    UInt i;

    if( aExpression->node.module == &qtc::passModule )
    {
        for( sGroup = aGroup, i = 0;
             sGroup != NULL;
             sGroup = sGroup->next, i++ )
        {
            if( sGroup->arithmeticOrList == (qtcNode *)aExpression->node.arguments )
            {
                sResult = i;
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
        // GROUP BY expression�� �ƴ� ���
    }

    return sResult;
}

idBool
qmoUnnesting::isSimpleSubquery( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *     �־��� statement�� simple subquery���� Ȯ���Ѵ�.
 *
 * Implementation :
 *     ������ ���ǵ��� ��� �����ؾ� �Ѵ�.
 *     1) FROM���� �ϳ��� relation�� �����Ѵ�.
 *     2) FROM���� relation�� view�� �ƴϴ�.
 *     3) Correlation�� 1�� table�� ���ؼ��� �����Ѵ�.
 *     4) Hierarchy, GROUP BY, LIMIT �Ǵ� HAVING ���� ������� �ʴ´�.
 *     5) ROWNUM, LEVEL, ISLEAF, PRIOR column�� ������� �ʴ´�.
 *
 ***********************************************************************/

    qmsParseTree * sParseTree;
    qmsSFWGH     * sSFWGH;
    ULong          sMask;
    ULong          sCondition;

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    sSFWGH     = sParseTree->querySet->SFWGH;

    if( sSFWGH == NULL )
    {
        // Set ������ ���Ե� ���
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSFWGH->depInfo.depCount != 1 )
    {
        // FROM������ �ϳ��� relation���� �����ؾ� �Ѵ�.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSFWGH->from->tableRef->view != NULL )
    {
        // FROM���� view�� �� �� ����.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( sSFWGH->outerDepInfo.depCount != 1 )
    {
        // 1���� outer query table���� �����ؾ� �Ѵ�.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( ( sSFWGH->hierarchy  != NULL ) ||
        ( sSFWGH->group      != NULL ) ||
        ( sSFWGH->aggsDepth1 != NULL ) ||
        ( sSFWGH->aggsDepth2 != NULL ) ||
        ( sSFWGH->having     != NULL ) ||
        ( sParseTree->limit  != NULL ) ||
        ( sSFWGH->top        != NULL ) ||   /* BUG-36580 supported TOP */        
        ( sSFWGH->rownum     != NULL ) ||
        ( sSFWGH->isLeaf     != NULL ) ||
        ( sSFWGH->level      != NULL ) )
    {
        // START WITH, CONNECT BY, GROUP BY, HAVING, LIMIT clause,
        // ROWNUM, ISLEAF, LEVEL pseudo column, aggregate function�� ����� �� ����.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // EXISTS/NOT EXISTS predicate�� ���Ե� subquery�� ���
        // SELECT������ ������̹Ƿ� ������� �Դٸ�
        // Aggregate/window function�� �������� �ʴ´�.
    }

    if( sSFWGH->where != NULL )
    {
        sMask = QTC_NODE_PRIOR_MASK;
        sCondition = QTC_NODE_PRIOR_ABSENT;

        if( ( sSFWGH->where->lflag & sMask ) != sCondition )
        {
            // PRIOR column�� ����� �� ����.
            IDE_CONT( INVALID_FORM );
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

    return ID_TRUE;

    IDE_EXCEPTION_CONT( INVALID_FORM );

    return ID_FALSE;
}

idBool
qmoUnnesting::isUnnestablePredicate( qcStatement * aStatement,
                                     qmsSFWGH    * aSFWGH,
                                     qtcNode     * aSubqueryPredicate,
                                     qmsSFWGH    * aSQSFWGH )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� WHERE���� HAVING���� ���Ե� correlation predicate����
 *     unnesting ������ ������ predicate���� Ȯ���Ѵ�.
 *
 * Implementation :
 *     ���� �ΰ��� ������ �����ؾ� �Ѵ�.
 *     1) �׻� conjunctive(AND�� ����)�� ���·� �����ؾ� �Ѵ�.
 *     2) WHERE�� �Ǵ� HAVING��, �ּ��� �� ������ outer query�� table����
 *        join predicate�� �����ؾ� �Ѵ�.
 *        �� �� subquery�� predicate�� EXISTS/NOT EXISTS���ο� ����
 *        join predicate���� �����ϴ� �����ڰ� �ٸ���.
 *
 ***********************************************************************/

    idBool sIsConjunctive = ID_TRUE;
    UInt   sJoinPredCount = 0;
    UInt   sCnfCount;
    UInt   sEstimated;

    if( aSQSFWGH->where != NULL )
    {
        existConjunctiveJoin( aSubqueryPredicate,
                              aSQSFWGH->where,
                              &aSQSFWGH->depInfo,
                              &aSQSFWGH->outerDepInfo,
                              &sIsConjunctive,
                              &sJoinPredCount );

        if( sIsConjunctive == ID_FALSE )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }

        if( existOuterJoinCorrelation( aSQSFWGH->where,
                                       &aSQSFWGH->outerDepInfo ) == ID_TRUE )
        {
            // Correlation���� outer join�� ���Ե� ���
            // ex) SELECT * FROM t1 WHERE EXISTS (SELECT 0 FROM t2 WHERE t1.i1 (+) = t2.i1);
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
        
        // BUG-41564
        // Correlated Predicate���� Subquery�� Argument�� ������ ���,
        // Subquery Argument�� ���� Subquery ���� �����Ѵٸ� Unnesting �Ұ�
        if( existOuterSubQueryArgument( aSQSFWGH->where,
                                        &aSQSFWGH->depInfo ) == ID_TRUE )
        {
            IDE_CONT( INVALID_FORM );
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

    if( aSQSFWGH->having != NULL )
    {
        existConjunctiveJoin( aSubqueryPredicate,
                              aSQSFWGH->having,
                              &aSQSFWGH->depInfo,
                              &aSQSFWGH->outerDepInfo,
                              &sIsConjunctive,
                              &sJoinPredCount );

        if( existOuterJoinCorrelation( aSQSFWGH->having,
                                       &aSQSFWGH->outerDepInfo ) == ID_TRUE )
        {
            // Correlation���� outer join�� ���Ե� ���
            // ex) SELECT * FROM t1 WHERE EXISTS (SELECT 0 FROM t2 WHERE t1.i1 (+) = t2.i1);
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
        
        // BUG-41564
        // Correlated Predicate���� Subquery�� Argument�� ������ ���,
        // Subquery Argument�� ���� Subquery ���� �����Ѵٸ� Unnesting �Ұ�
        if( existOuterSubQueryArgument( aSQSFWGH->having,
                                        &aSQSFWGH->depInfo ) == ID_TRUE )
        {
            IDE_CONT( INVALID_FORM );
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

    if( ( sIsConjunctive == ID_FALSE ) ||
        ( sJoinPredCount == 0 ) )
    {
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( aSubqueryPredicate->node.module == &mtfNotExists )
    {
        // NOT EXISTS�� ��� subquery �� table�� �������� �ʴ� predicate�� �����ϴ� ���
        // (constant predicate �Ǵ� outer table�� column���θ� ������ predicate)
        // anti join���� transform�� �� ����.
        if( aSQSFWGH->where != NULL )
        {
            if( isAntiJoinablePredicate( aSQSFWGH->where, &aSQSFWGH->depInfo ) == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
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

        if( aSQSFWGH->having != NULL )
        {
            if( isAntiJoinablePredicate( aSQSFWGH->having, &aSQSFWGH->depInfo ) == ID_FALSE )
            {
                IDE_CONT( INVALID_FORM );
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

    // BUG-38827
    // Subquery block ���� view target(inline view �� target ��) �����
    // ���� ���� unnest ���� �ʴ´�.
    if ( ( existViewTarget( aSQSFWGH->where,
                            &aSQSFWGH->depInfo ) != ID_TRUE ) &&
         ( existViewTarget( aSQSFWGH->having,
                            &aSQSFWGH->depInfo ) != ID_TRUE ) )
    {
        // Inline view �� target ���� ���ٸ� unnest �ؼ� �ȵȴ�.
        IDE_CONT( INVALID_FORM );
    }
    else
    {
        // Nothing to do.
    }

    if( aSFWGH != NULL )
    {
        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_NORMAL_FORM_MAXIMUM );

        (void)qmoNormalForm::estimateCNF( aSFWGH->where,
                                          &sCnfCount );

        // sEstimated = WHERE���� CNF ��ȯ �� ���� predicate �� + subquery�� correlation predicate�� �� - 1
        // * EXISTS/NOT EXISTS�� ��ġ�� correlation predicate�� �����ǹǷ� 1���� �����Ѵ�.
        sEstimated = sCnfCount + sJoinPredCount - 1;

        if( sEstimated > QCG_GET_SESSION_NORMALFORM_MAXIMUM( aStatement ) )
        {
            IDE_CONT( INVALID_FORM );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // SELECT������ �ƴϰų� WHERE�� �� ON �� ���� ���
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( INVALID_FORM );

    return ID_FALSE;
}

idBool
qmoUnnesting::existOuterJoinCorrelation( qtcNode   * aNode,
                                         qcDepInfo * aOuterDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     Subquery���� correlation���� outer join�� ���ԵǾ��ִ��� Ȯ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qcDepInfo   sDepInfo;

    qtc::dependencyAnd( &aNode->depInfo,
                        aOuterDepInfo,
                        &sDepInfo );

    if( ( qtc::haveDependencies( &sDepInfo ) == ID_TRUE ) &&
        ( aNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK ) == QTC_NODE_JOIN_OPERATOR_EXIST )
    {
        if( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE ) 
        {
            if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_AND )
            {
                for( sArg = (qtcNode *)aNode->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    if( existOuterJoinCorrelation( sArg, aOuterDepInfo ) == ID_TRUE )
                    {
                        IDE_CONT( APPLICABLE_EXIT );
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
        }
        else
        {
            // �� �����ڰ� �ƴϸ鼭 outer join operator�� correlation�� �����ϴ� ���
            IDE_CONT( APPLICABLE_EXIT );
        }
    }
    else
    {
        // Nothing to do.
    }

    return ID_FALSE;

    IDE_EXCEPTION_CONT( APPLICABLE_EXIT );

    return ID_TRUE;
}

idBool
qmoUnnesting::isAntiJoinablePredicate( qtcNode   * aNode,
                                       qcDepInfo * aDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     Anti join�� �������� subquery�� ���Ե� predicate�� Ȯ���Ѵ�.
 *     Subquery�� ���Ե� predicate �� subquery�� column���� ��������
 *     �ʴ� predicate�� ���Ե� ��� anti join�� �Ұ����ϴ�.
 *     ex) Constant predicate, correlation���θ� ������ predicate ��
 *
 * Implementation :
 *     aNode�� depInfo�� aDepInfo�� ��ġ�� ������ �Ұ������� �Ǵ��Ѵ�.
 *     �� �� aDepInfo�� subquery�� depInfo�� �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qcDepInfo   sDepInfo;

    qtc::dependencyAnd( &aNode->depInfo,
                        aDepInfo,
                        &sDepInfo );

    if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
    {
        if( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE ) 
        {
            if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_AND )
            {
                for( sArg = (qtcNode *)aNode->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    if( isAntiJoinablePredicate( sArg, aDepInfo ) == ID_FALSE )
                    {
                        IDE_CONT( NOT_APPLICABLE_EXIT );
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
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        IDE_CONT( NOT_APPLICABLE_EXIT );
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( NOT_APPLICABLE_EXIT );

    return ID_FALSE;
}

void
qmoUnnesting::existConjunctiveJoin( qtcNode   * aSubqueryPredicate,
                                    qtcNode   * aNode,
                                    qcDepInfo * aInnerDepInfo,
                                    qcDepInfo * aOuterDepInfo,
                                    idBool    * aIsConjunctive,
                                    UInt      * aJoinPredCount )
{
/***********************************************************************
 *
 * Description :
 *     �־��� predicate�� ���Ե� correlation predicate���� conjunctive����,
 *     �׸��� subquery�� table�� join ������ �ϳ� �̻� �����ϴ��� Ȯ���Ѵ�.
 *     aIsConjunctive�� �ݵ�� ID_FALSE, aContainsJoin�� ID_TRUE��
 *     �ʱⰪ�� ������ �� �� �Լ��� ȣ���ؾ� �Ѵ�.
 *
 * Implementation :
 *     Correlation predicate�� �����ϴ� �� �����ڰ� AND�� �����ϸ�
 *     true��, �� �� �ٸ� �� �����ڰ� correlation predicate��
 *     �����ϸ� false�� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qtcNode   * sFirstArg;
    qtcNode   * sSecondArg;
    qcDepInfo   sDepInfo;
    idBool      sJoinableOperator;

    qtc::dependencyAnd( &aNode->depInfo,
                        aOuterDepInfo,
                        &sDepInfo );

    if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
    {
        if( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE ) 
        {
            if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_AND )
            {
                for( sArg = (qtcNode *)aNode->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    existConjunctiveJoin( aSubqueryPredicate,
                                          sArg,
                                          aInnerDepInfo,
                                          aOuterDepInfo,
                                          aIsConjunctive,
                                          aJoinPredCount );
                    if( *aIsConjunctive == ID_FALSE )
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
                // OR, NOT ���� ���Ե� �� ����.
                *aIsConjunctive = ID_FALSE;
            }
        }
        else
        {
            IDE_FT_ASSERT( ( aNode->node.lflag & MTC_NODE_COMPARISON_MASK ) == MTC_NODE_COMPARISON_TRUE );

            // Quantifier ������, EXISTS/NOT EXISTS, BETWEEN, not equal(<>)�� �������� �ʴ´�.
            if( ( ( aNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK ) == MTC_NODE_GROUP_COMPARISON_TRUE ) ||
                ( aNode->node.module == &mtfExists ) ||
                ( aNode->node.module == &mtfNotExists ) )
            {
                // Nothing to do.
                *aIsConjunctive = ID_FALSE;
            }
            else
            {
                // Quantifier ������, BETWEEN ���� �� �����ڷ� ��ȯ�Ѵٸ� �����ϴ�.
                switch( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                {
                    case MTC_NODE_OPERATOR_EQUAL:
                    case MTC_NODE_OPERATOR_GREATER:
                    case MTC_NODE_OPERATOR_GREATER_EQUAL:
                    case MTC_NODE_OPERATOR_LESS:
                    case MTC_NODE_OPERATOR_LESS_EQUAL:
                        sJoinableOperator = ID_TRUE;
                        break;
                    case MTC_NODE_OPERATOR_NOT_EQUAL:
                        if( aSubqueryPredicate->node.module == &mtfNotExists )
                        {
                            sJoinableOperator = ID_FALSE;
                        }
                        else
                        {
                            sJoinableOperator = ID_TRUE;
                        }
                        break;
                    default:
                        sJoinableOperator = ID_FALSE;
                        break;
                }

                if( sJoinableOperator == ID_TRUE )
                {
                    sFirstArg = (qtcNode *)aNode->node.arguments;
                    sSecondArg = (qtcNode *)sFirstArg->node.next;

                    if( ( sFirstArg->depInfo.depCount  == 1 ) &&
                        ( sSecondArg->depInfo.depCount == 1 ) )
                    {
                        // �� argument�� ���� inner query�� outer query�� dependency�� ������ Ȯ���Ѵ�.
                        if( ( ( qtc::dependencyContains( aOuterDepInfo, &sFirstArg->depInfo ) == ID_TRUE ) &&
                              ( qtc::dependencyContains( aInnerDepInfo, &sSecondArg->depInfo ) == ID_TRUE ) ) ||
                            ( ( qtc::dependencyContains( aInnerDepInfo, &sFirstArg->depInfo ) == ID_TRUE ) &&
                              ( qtc::dependencyContains( aOuterDepInfo, &sSecondArg->depInfo ) == ID_TRUE ) ) )
                        {
                            (*aJoinPredCount)++;
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
        }
    }
    else
    {
        // Nothing to do.
    }
}

IDE_RC
qmoUnnesting::transformSubqueryToView( qcStatement  * aStatement,
                                       qtcNode      * aSubquery,
                                       qmsFrom     ** aView )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� view�� ��ȯ�Ͽ� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsSFWGH     * sSFWGH;
    qmsFrom      * sFrom;
    qmsTableRef  * sTableRef;
    qcStatement  * sSQStatement;
    qmsParseTree * sSQParseTree;
    qmsOuterNode * sOuterNode;
    qmsOuterNode * sPrevOuterNode = NULL;
    mtcTuple     * sMtcTuple;
    UInt           i;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::transformSubqueryToView::__FT__" );

    sSFWGH = ((qmsParseTree *)aStatement->myPlan->parseTree)->querySet->SFWGH;
    sSQStatement = aSubquery->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsFrom ),
                                               (void **)&sFrom )
              != IDE_SUCCESS );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsTableRef ),
                                               (void **)&sTableRef )
              != IDE_SUCCESS );

    QCP_SET_INIT_QMS_FROM( sFrom );
    QCP_SET_INIT_QMS_TABLE_REF( sTableRef );

    sTableRef->view = sSQStatement;
    sFrom->tableRef = sTableRef;

    // Unique�� view name ����
    IDE_TEST( genUniqueViewName( aStatement, (UChar **)&sTableRef->aliasName.stmtText )
              != IDE_SUCCESS );
    sTableRef->aliasName.offset = 0;
    sTableRef->aliasName.size   = idlOS::strlen( sTableRef->aliasName.stmtText );

    sSQParseTree->querySet->SFWGH->selectType = QMS_ALL;

    // View���� outer column�� �����Ƿ� ���� ����
    qtc::dependencyClear( &sSQParseTree->querySet->outerDepInfo );
    qtc::dependencyClear( &sSQParseTree->querySet->SFWGH->outerDepInfo );

    // PROJ-2418 Unnesting �� ��Ȳ������ Lateral View�� �����Ƿ�
    // lateralDepInfo ������ ���� �����Ѵ�.
    qtc::dependencyClear( &sSQParseTree->querySet->lateralDepInfo );

    for( sOuterNode = sSQParseTree->querySet->SFWGH->outerColumns;
         sOuterNode != NULL;
         sOuterNode = sOuterNode->next )
    {
        if( sPrevOuterNode != NULL )
        {
            IDE_TEST( QC_QMP_MEM( aStatement )->free( sPrevOuterNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        sPrevOuterNode = sOuterNode;
    }

    if( sPrevOuterNode != NULL )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->free( sPrevOuterNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }
    sSQParseTree->querySet->SFWGH->outerColumns = NULL;

    // ������ inline view�� validation�Ѵ�.
    IDE_TEST( qmvQuerySet::validateInlineView( aStatement,
                                               sSFWGH,
                                               sTableRef,
                                               MTC_COLUMN_NOTNULL_TRUE )
              != IDE_SUCCESS );

    sMtcTuple = &(QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sTableRef->table]);

    // BUG-43708 unnest ���� ������� view�� ���ؼ�
    // MTC_COLUMN_USE_COLUMN_TRUE �� �����ؾ���
    for( i = 0;
         i < sMtcTuple->columnCount;
         i++ )
    {
        sMtcTuple->columns[i].flag &= ~MTC_COLUMN_USE_COLUMN_MASK;
        sMtcTuple->columns[i].flag |=  MTC_COLUMN_USE_COLUMN_TRUE;
    }

    // View�� column name���� qmsTableRef�� �����Ѵ�.
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( (QC_MAX_OBJECT_NAME_LEN + 1) *
                                             (sTableRef->tableInfo->columnCount),
                                             (void**)&( sTableRef->columnsName ) )
              != IDE_SUCCESS );

    for( i = 0;
         i < sMtcTuple->columnCount;
         i++ )
    {
        idlOS::memcpy( sTableRef->columnsName[i],
                       sTableRef->tableInfo->columns[i].name,
                       (QC_MAX_OBJECT_NAME_LEN + 1) );
    }

    // Table map�� view�� ����Ѵ�.
    QC_SHARED_TMPLATE( aStatement )->tableMap[sTableRef->table].from = sFrom;

    // Dependency ����
    qtc::dependencyClear( &sFrom->depInfo );
    qtc::dependencySet( sTableRef->table, &sFrom->depInfo );

    qtc::dependencyClear( &sFrom->semiAntiJoinDepInfo );

    *aView          = sFrom;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::genUniqueViewName( qcStatement * aStatement, UChar ** aViewName )
{
/***********************************************************************
 *
 * Description :
 *     Unique�� view�� �̸��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UChar   * sViewName;
    UInt      sIdx = 0;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genUniqueViewName::__FT__" );

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( VIEW_NAME_LENGTH, (void**)&sViewName )
              != IDE_SUCCESS );

    sIdx = ++QC_SHARED_TMPLATE( aStatement )->mUnnestViewNameIdx;

    IDE_TEST_RAISE ( sIdx > 99999, ERR_INDEX );

    /* BUG-48052
     * PREFIX(5)  $VIEW  + Number(5) - max (99999)
     * = 10 + 1 = 11
     */
    idlOS::snprintf( (char*)sViewName, VIEW_NAME_LENGTH, VIEW_NAME_PREFIX"%"ID_UINT32_FMT, sIdx );

    *aViewName = sViewName;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INDEX )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoUnnesting::genUniqueViewName",
                                  "Invalid View Name" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::removeCorrPredicate( qcStatement  * aStatement,
                                   qtcNode     ** aPredicate,
                                   qcDepInfo    * aOuterDepInfo,
                                   qtcNode     ** aRemoved )
{
/***********************************************************************
 *
 * Description :
 *     �־��� predicate���� correlation predicate���� �����Ѵ�.
 *
 * Implementation :
 *     ��������� Ž���ϸ鼭 �� �����ڰ� �ƴ� ��� ����� ��Ƽ�
 *     ��ȯ�Ѵ�.
 *     Correlation predicate�� �����ϱ� ���ؼ�, parent node�� ������
 *     ���ڷ� �Ѱܹ޴� ��� double pointer�� ����ϵ��� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode    * sConcatPred;
    qtcNode    * sNode;
    qtcNode   ** sDoublePointer;
    qtcNode    * sOld;
    qcDepInfo    sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::removeCorrPredicate::__FT__" );

    sNode = *aPredicate;

    qtc::dependencyAnd( &sNode->depInfo,
                        aOuterDepInfo,
                        &sDepInfo );

    if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
    {
        if( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            IDE_FT_ERROR( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                          == MTC_NODE_OPERATOR_AND );

            sDoublePointer = (qtcNode **)&sNode->node.arguments;

            while( *sDoublePointer != NULL )
            {
                sOld = *sDoublePointer;

                IDE_TEST( removeCorrPredicate( aStatement,
                                               sDoublePointer,
                                               aOuterDepInfo,
                                               aRemoved )
                          != IDE_SUCCESS );
                if( sOld == *sDoublePointer )
                {
                    sDoublePointer = (qtcNode **)&(*sDoublePointer)->node.next;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( sNode->node.arguments == NULL )
            {
                // AND�� operand�� ��� ���ŵ� ���
                *aPredicate = NULL;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            *aPredicate = (qtcNode *)sNode->node.next;
            sNode->node.next = NULL;

            // �� �����ڰ� �ƴϸ� predicate�̹Ƿ� ����
            IDE_TEST( concatPredicate( aStatement,
                                       *aRemoved,
                                       sNode,
                                       &sConcatPred )
                      != IDE_SUCCESS );
            *aRemoved = sConcatPred;
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
qmoUnnesting::genViewSelect( qcStatement * aStatement,
                             qtcNode     * aNode,
                             idBool        aIsOuterExpr )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�κ��� ���ŵ� correlation predicate���κ��� VIEW��
 *     SELECT list�� �����Ѵ�.
 *
 * Implementation :
 *     Predicate�� subquery�� column���� ã�� append�Ѵ�.
 *
 ***********************************************************************/

    qmsParseTree * sParseTree;
    qmsQuerySet  * sQuerySet;
    qtcNode      * sArg;
    qmsQuerySet  * sTempQuerySet;
    qcDepInfo      sDepInfo;
    qtcOverColumn* sOverColumn;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genViewSelect::__FT__" );

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    sQuerySet  = sParseTree->querySet;

    // BUG-36803 aNode may be null
    if ( aNode != NULL )
    {
        qtc::dependencyAnd( &sQuerySet->depInfo, &aNode->depInfo, &sDepInfo );

        // BUG-45279 ������ð� ���� aggr �Լ��� target�� �־�� �Ѵ�.
        if( (qtc::haveDependencies( &sDepInfo ) == ID_TRUE) ||
            (QTC_HAVE_AGGREGATE( aNode ) == ID_TRUE) )
        {
            // Column�̳� pass node�� ã�� ������ ��������� ��ȸ�Ѵ�.
            // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
            // mtfGetBlobLocator, mtfGetClobLocator�� pass nodeó�� ����ؾ� �Ѵ�.
            if( ( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE ) || 
                ( aNode->node.module == &qtc::passModule ) ||
                ( aNode->node.module == &mtfGetBlobLocator ) ||
                ( aNode->node.module == &mtfGetClobLocator ) ||
                ( ( ( aIsOuterExpr == ID_FALSE ) && 
                    ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                      == MTC_NODE_OPERATOR_AGGREGATION ) ) ) )
            {
                IDE_TEST( appendViewSelect( aStatement,
                                            aNode )
                          != IDE_SUCCESS );
            }
            else if( aNode->node.module == &qtc::subqueryModule )
            {
                // BUG-45226 ���������� target �� ���������� ������ ������ �߻��մϴ�.
                sTempQuerySet = ((qmsParseTree*)(aNode->subquery->myPlan->parseTree))->querySet;

                IDE_TEST( genViewSetOp( aStatement,
                                        sTempQuerySet,
                                        aIsOuterExpr )
                          != IDE_SUCCESS );
            }
            else
            {
                for ( sArg = (qtcNode *)aNode->node.arguments;
                      sArg != NULL;
                      sArg = (qtcNode *)sArg->node.next )
                {
                    IDE_TEST( genViewSelect( aStatement,
                                             sArg,
                                             aIsOuterExpr )
                              != IDE_SUCCESS );
                }
            }
        }
        else
        {
            // Nothing to do.
        }

        /* BUG-40914 */
        if (aNode->overClause != NULL)
        {
            for (sOverColumn = aNode->overClause->overColumn;
                 sOverColumn != NULL;
                 sOverColumn = sOverColumn->next)
            {
                IDE_TEST(genViewSelect(aStatement,
                                       sOverColumn->node,
                                       aIsOuterExpr)
                         != IDE_SUCCESS);
            }
        }
        else
        {
            /* nothing to do */
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

IDE_RC qmoUnnesting::genViewSetOp( qcStatement * aStatement,
                                   qmsQuerySet * aSetQuerySet,
                                   idBool        aIsOuterExpr )
{
/***********************************************************************
 *
 * Description :
 *     BUG-45226
 *     ���������� outerColumns �� ã�Ƽ� genViewSelect ȣ���Ѵ�.
 *
 ***********************************************************************/
    qmsOuterNode * sOuter;

    if ( aSetQuerySet->setOp == QMS_NONE )
    {
        for ( sOuter = aSetQuerySet->SFWGH->outerColumns;
              sOuter != NULL;
              sOuter = sOuter->next )
        {
            IDE_TEST( genViewSelect( aStatement,
                                     sOuter->column,
                                     aIsOuterExpr)
                      != IDE_SUCCESS);
        }
    }
    else
    {
        IDE_TEST( genViewSetOp( aStatement,
                                aSetQuerySet->left,
                                aIsOuterExpr )
                  != IDE_SUCCESS );

        IDE_TEST( genViewSetOp( aStatement,
                                aSetQuerySet->right,
                                aIsOuterExpr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoUnnesting::appendViewSelect( qcStatement * aStatement,
                                       qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     View�� SELECT���� column�� append�Ѵ�.
 *     ���� �̹� SELECT���� �����ϴ� column�� ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsConcatElement * sGroup;
    qmsParseTree     * sParseTree;
    qmsQuerySet      * sQuerySet;
    qmsTarget        * sTarget;
    qmsTarget        * sLastTarget = NULL;
    qtcNode          * sNode;
    qtcNode          * sArgNode;  /* BUG-39287 */
    qtcNode          * sPassNode; /* BUG-39287 */
    SChar            * sColumnName;
    UInt               sIdx = 0;
    idBool             sIsEquivalent = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::appendViewSelect::__FT__" );

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    sQuerySet  = sParseTree->querySet;

    for( sTarget = sQuerySet->target;
         sTarget != NULL;
         sTarget = sTarget->next, sIdx++ )
    {
        sLastTarget = sTarget;

        sNode = sTarget->targetColumn;
        while( sNode->node.module == &qtc::passModule )
        {
            sNode = (qtcNode *)sNode->node.arguments;
        }

        if( ( sNode->node.table  == aNode->node.table ) &&
            ( sNode->node.column == aNode->node.column ) )
        {
            // ������ column�� �̹� �����ϴ� ���
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if( sTarget == NULL )
    {
        // ���� �߰��ؾ� �ϴ� ���

        // Unique�� column �̸��� �������ش�.
        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( COLUMN_NAME_LENGTH,
                                                   (void**)&sColumnName )
                  != IDE_SUCCESS );

        idlOS::snprintf( (char*)sColumnName,
                         COLUMN_NAME_LENGTH,
                         COLUMN_NAME_PREFIX"%"ID_UINT32_FMT,
                         sIdx + 1 );

        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsTarget ),
                                                   (void**)&sTarget )
                  != IDE_SUCCESS );

        QMS_TARGET_INIT( sTarget );

        /*
         * BUG-39287
         * ���� node �� �ű��� �ʰ� node �� ���� �����Ͽ� �����Ѵ�.
         * pass node �� ��쿡�� pass node �� argument �� ���� �����Ѵ�.
         */
        IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qtcNode),
                                               (void**)&sNode)
                 != IDE_SUCCESS);

        idlOS::memcpy(sNode, aNode, ID_SIZEOF(qtcNode));
        sNode->node.next = NULL;

        sTarget->aliasColumnName.name = sColumnName;
        sTarget->aliasColumnName.size = idlOS::strlen(sColumnName);
        sNode->node.conversion        = NULL;
        sNode->node.leftConversion    = NULL;
        sTarget->targetColumn         = sNode;

        if (aNode->node.module == &qtc::passModule)
        {
            IDE_TEST(QC_QMP_MEM(aStatement)->alloc(ID_SIZEOF(qtcNode),
                                                   (void**)&sArgNode)
                     != IDE_SUCCESS);

            idlOS::memcpy(sArgNode, aNode->node.arguments, ID_SIZEOF(qtcNode));
            sNode->node.arguments = (mtcNode*)sArgNode;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST(qtc::estimateNodeWithArgument(aStatement,
                                               sNode)
                 != IDE_SUCCESS);

        // Aggregate function�� �ƴϸ� pass node�� �ƴ� ���
        // (�̹� GROUP BY���� column�̾��� ��쿡�� pass node�� �����ȴ�.)
        if( ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK ) != MTC_NODE_OPERATOR_AGGREGATION ) &&
            ( aNode->node.module != &qtc::passModule ) )
        {
            // GROUP BY�� ó��
            for( sGroup = sQuerySet->SFWGH->group;
                 sGroup != NULL;
                 sGroup = sGroup->next )
            {
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       aNode,
                                                       sGroup->arithmeticOrList,
                                                       &sIsEquivalent )
                          != IDE_SUCCESS );

                if( sIsEquivalent == ID_TRUE )
                {
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( ( sQuerySet->SFWGH->aggsDepth1 != NULL ) ||
                ( sQuerySet->SFWGH->group != NULL ) )
            {
                if( sIsEquivalent == ID_FALSE )
                {
                    // GROUP BY���� �� expression �߰�
                    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmsConcatElement ),
                                                               (void**)&sGroup )
                              != IDE_SUCCESS );

                    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qtcNode ),
                                                               (void**)&sGroup->arithmeticOrList )
                              != IDE_SUCCESS );

                    idlOS::memcpy( sGroup->arithmeticOrList, aNode, ID_SIZEOF( qtcNode ) );

                    // BUG-41018 taget �÷��� �������� �����ϰ� �߰������Ƿ�
                    // group by ���� �������� �������� �߰��ؾ� �Ѵ�.
                    sGroup->arithmeticOrList->node.conversion        = NULL;
                    sGroup->arithmeticOrList->node.leftConversion    = NULL;

                    // BUG-38011
                    // target ���� ���������� dependency �� ��ġ�ϴ� ��常 �����´�.
                    sGroup->arithmeticOrList->node.next = NULL;

                    sGroup->type = QMS_GROUPBY_NORMAL;
                    sGroup->next = sQuerySet->SFWGH->group;

                    /* TASK-7219 Shard Transformer Refactoring */
                    sGroup->arguments = NULL;
                    sQuerySet->SFWGH->group = sGroup;
                }
                else
                {
                    // Nothing to do.
                }

                // GROUP BY�� expression�� ����Ű���� pass node ����
                IDE_TEST( qtc::makePassNode( aStatement,
                                             sNode,
                                             sGroup->arithmeticOrList,
                                             &sPassNode )
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

        if( sLastTarget == NULL )
        {
            sQuerySet->target = sTarget;
            sQuerySet->SFWGH->target = sTarget;
        }
        else
        {
            sLastTarget->next = sTarget;
        }
    }
    else
    {
        // ������ column�� �̹� �����ϹǷ� �߰����� �ʴ´�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoUnnesting::toViewColumns( qcStatement  * aViewStatement,
                                    qmsTableRef  * aViewTableRef,
                                    qtcNode     ** aNode,
                                    idBool         aIsOuterExpr )
{
/***********************************************************************
 *
 * Description :
 *     Subquery �� table�� �����ϴ� column�� view�� �����ϴ� table��
 *     �����Ѵ�.
 *     Subquery�� view�� �����Ҷ� subquery�� ���Ե� correlation predicate��
 *     outer query�� �Ű� view���� join predicate���� ��ȯ�ϱ� ���� ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsParseTree * sParseTree;
    qmsQuerySet  * sQuerySet;
    qtcNode      * sNode[2];
    qtcNode     ** sDoublePointer;
    qmsTarget    * sTarget;
    qmsQuerySet  * sTempQuerySet;
    qcDepInfo      sDepInfo;
    qcDepInfo      sViewDefInfo;
    UInt           sIdx = 0;
    idBool         sFind = ID_FALSE;
    mtcNode      * sNextNode;           /* BUG-39287 */
    mtcNode      * sConversionNode;     /* BUG-39287 */
    mtcNode      * sLeftConversionNode; /* BUG-39287 */
    qtcOverColumn* sOverColumn;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::toViewColumns::__FT__" );

    IDE_DASSERT( aNode != NULL );

    sParseTree = (qmsParseTree *)aViewStatement->myPlan->parseTree;
    sQuerySet  = sParseTree->querySet;

    qtc::dependencySet( aViewTableRef->table, &sViewDefInfo );
    qtc::dependencyAnd( &sQuerySet->depInfo, &(*aNode)->depInfo, &sDepInfo );

    // BUG-45279 ������ð� ���� aggr �Լ��� target�� �־�� �Ѵ�.
    // view�� target �� �����Ƿ� ��ȯ�� �����ؾ� �Ѵ�.
    if( (qtc::haveDependencies( &sDepInfo ) == ID_TRUE) ||
        (QTC_HAVE_AGGREGATE( *aNode ) == ID_TRUE) )
    {
        // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
        // mtfGetBlobLocator, mtfGetClobLocator�� pass nodeó�� ����ؾ� �Ѵ�.
        if( ( QTC_IS_COLUMN( aViewStatement, *aNode ) == ID_TRUE ) ||
            ( (*aNode)->node.module == &qtc::passModule ) ||
            ( (*aNode)->node.module == &mtfGetBlobLocator ) ||
            ( (*aNode)->node.module == &mtfGetClobLocator ) ||
            ( ( ( aIsOuterExpr == ID_FALSE  ) &&
                ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK) == MTC_NODE_OPERATOR_AGGREGATION ) ) )
        {
            // Column, �Ǵ� pass node�� ���
            for( sTarget = sQuerySet->target;
                 sTarget != NULL;
                 sTarget = sTarget->next )
            {
                // View�� SELECT������ predicate�� column�� ã�� view�� �����ϵ��� �����Ѵ�.
                // ex) SELECT ... FROM t2, (SELECT t1.c1 COL1, t1.c2 COL2 ... FROM t1) view1
                //     aNode�� "t1.c1 = t2.c1" �̾��� �� "view1.col1 = t2.c1" �� �����Ѵ�.

                // BUG-38228
                // group by �� �������� target �� pass node�� ������ �ִ�.
                // group by �� �ִ� �÷��� aNode �� ���ö� ������ ���� ��Ȳ�� �߻��Ѵ�.
                // select i1 from t1 where i4 in ( select i4 from t1 ) group by i1;
                //        1�� ��Ȳ                                               2�� ��Ȳ
                // 1. aNode �� �ܺ� ������ taget�϶��� pass node�� �ִ�.
                //    �̶��� 1��° if ������ ó���� �ȴ�.
                // 2. aNode �� �ܺ� ������ group by ���϶��� pass node�� ����.
                //    �̶��� 2��° if ������ ó���� �ȴ�.
                if( ( sTarget->targetColumn->node.table  == (*aNode)->node.table ) &&
                    ( sTarget->targetColumn->node.column == (*aNode)->node.column ) )
                {
                    sFind = ID_TRUE;
                }
                // mtfGetBlobLocator, mtfGetClobLocator�� pass nodeó�� ����ؾ� �Ѵ�.
                else if ( (sTarget->targetColumn->node.module == &qtc::passModule)   ||
                          (sTarget->targetColumn->node.module == &mtfGetBlobLocator) ||
                          (sTarget->targetColumn->node.module == &mtfGetClobLocator) )
                {
                    if( ( sTarget->targetColumn->node.arguments->table  == (*aNode)->node.table ) &&
                        ( sTarget->targetColumn->node.arguments->column == (*aNode)->node.column ) )
                    {
                        sFind = ID_TRUE;
                    }
                    else
                    {
                        sFind = ID_FALSE;
                    }
                }
                else
                {
                    sFind = ID_FALSE;
                }

                if ( sFind == ID_TRUE )
                {
                    /*
                     * BUG-39287
                     * appendViewSelect �������� view target �� �����ؼ� ��������Ƿ�
                     * ���⼭�� ���� node �� view target ���� ����Ű���� �����Ѵ�.
                     * ��, target �� aggr node �� ��� ���� �����Ѵ�.
                     * �׷��� ������ qmsSFWGH->aggsDepth1 �� �߸��� node �� ����Ű�� �ȴ�.
                     */
                    if (((*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK) ==
                        MTC_NODE_OPERATOR_AGGREGATION)
                    {
                        /* aggr node �� ��� ���ο� node ����
                         * subquery unnesting �� ��츸 �ش�� */

                        IDE_TEST( qtc::makeColumn( aViewStatement,
                                                   sNode,
                                                   NULL,
                                                   NULL,
                                                   NULL,
                                                   NULL )
                                  != IDE_SUCCESS );

                        sNode[0]->node.module         = &qtc::columnModule;
                        sNode[0]->node.table          = aViewTableRef->table;
                        sNode[0]->node.column         = sIdx;
                        sNode[0]->node.baseTable      = sNode[0]->node.table;
                        sNode[0]->node.baseColumn     = sNode[0]->node.column;
                        sNode[0]->node.next           = (*aNode)->node.next;
                        sNode[0]->node.conversion     = (*aNode)->node.conversion;
                        sNode[0]->node.leftConversion = (*aNode)->node.leftConversion;

                        (*aNode)->node.next = NULL;

                        SET_POSITION( sNode[0]->tableName, aViewTableRef->aliasName );
                        sNode[0]->columnName.stmtText = sTarget->aliasColumnName.name;
                        sNode[0]->columnName.size     = sTarget->aliasColumnName.size;
                        sNode[0]->columnName.offset   = 0;

                        IDE_TEST( qtc::estimateNodeWithoutArgument( aViewStatement,
                                                                    sNode[0] )
                                  != IDE_SUCCESS );

                        IDE_TEST( qmvQTC::addViewColumnRefList( aViewStatement,
                                                                sNode[0] )
                                  != IDE_SUCCESS );

                        *aNode = sNode[0];
                    }
                    else
                    {
                        /* aggr node �ƴѰ�� ���� node �� ���� ���� */

                        sNextNode           = aNode[0]->node.next;
                        sConversionNode     = aNode[0]->node.conversion;
                        sLeftConversionNode = aNode[0]->node.leftConversion;

                        QTC_NODE_INIT(aNode[0]);

                        aNode[0]->node.module         = &qtc::columnModule;
                        aNode[0]->node.table          = aViewTableRef->table;
                        aNode[0]->node.column         = sIdx;
                        aNode[0]->node.baseTable      = aNode[0]->node.table;
                        aNode[0]->node.baseColumn     = aNode[0]->node.column;
                        aNode[0]->node.next           = sNextNode;

                        // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
                        // unnest �������� ������ view ������ ������ lobLocator Ÿ������ �Ѱ��ش�.
                        // lob Ÿ���� �ɷ��� lobLocator ������ ��尡 �޷������Ƿ� ������ ��带 �������־�� �Ѵ�.
                        // ex: SELECT SUBSTR(i2,0,LENGTH(i2)) FROM t1 WHERE i1 = (SELECT MAX(i1) FROM t1);
                        if ( (sTarget->targetColumn->node.module == &mtfGetBlobLocator) ||
                             (sTarget->targetColumn->node.module == &mtfGetClobLocator) )
                        {
                            aNode[0]->node.conversion     = NULL;
                            aNode[0]->node.leftConversion = NULL;
                        }
                        else
                        {
                            aNode[0]->node.conversion     = sConversionNode;
                            aNode[0]->node.leftConversion = sLeftConversionNode;
                        }

                        SET_POSITION( aNode[0]->tableName, aViewTableRef->aliasName );
                        aNode[0]->columnName.stmtText = sTarget->aliasColumnName.name;
                        aNode[0]->columnName.size     = sTarget->aliasColumnName.size;
                        aNode[0]->columnName.offset   = 0;

                        IDE_TEST( qtc::estimateNodeWithoutArgument( aViewStatement,
                                                                    aNode[0] )
                                  != IDE_SUCCESS );

                        IDE_TEST( qmvQTC::addViewColumnRefList( aViewStatement,
                                                                aNode[0] )
                                  != IDE_SUCCESS );
                    }

                    break;
                }
                else
                {
                    // Nothing to do.
                }
                sIdx++;
            }
            // View�� SELECT���� �ݵ�� �����ؾ� �Ѵ�.
            IDE_FT_ERROR( sTarget != NULL );
        }
        else
        {
            // Terminal node�� �ƴ� ��� ��������� ��ȸ�Ѵ�.
            if( (*aNode)->node.module != &qtc::subqueryModule )
            {
                sDoublePointer = (qtcNode **)&(*aNode)->node.arguments;
                while( *sDoublePointer != NULL )
                {
                    IDE_TEST( toViewColumns( aViewStatement,
                                             aViewTableRef,
                                             sDoublePointer,
                                             aIsOuterExpr )
                              != IDE_SUCCESS );
                    sDoublePointer = (qtcNode **)&(*sDoublePointer)->node.next;
                }

                IDE_TEST( qtc::estimateNodeWithArgument( aViewStatement,
                                                         *aNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // BUG-45226 ���������� target �� ���������� ������ ������ �߻��մϴ�.
                sTempQuerySet = ((qmsParseTree*)((*aNode)->subquery->myPlan->parseTree))->querySet;

                IDE_TEST( toViewSetOp( aViewStatement,
                                       aViewTableRef,
                                       sTempQuerySet,
                                       aIsOuterExpr )
                          != IDE_SUCCESS );

                qtc::dependencyOr( &sViewDefInfo,
                                   &((*aNode)->depInfo),
                                   &((*aNode)->depInfo) );
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    /* BUG-40914 */
    if ((*aNode)->overClause != NULL)
    {
        for (sOverColumn = (*aNode)->overClause->overColumn;
             sOverColumn != NULL;
             sOverColumn = sOverColumn->next)
        {
            IDE_TEST(toViewColumns(aViewStatement,
                                   aViewTableRef,
                                   &sOverColumn->node,
                                   aIsOuterExpr)
                     != IDE_SUCCESS);
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


IDE_RC qmoUnnesting::toViewSetOp( qcStatement  * aViewStatement,
                                  qmsTableRef  * aViewTableRef,
                                  qmsQuerySet  * aSetQuerySet,
                                  idBool         aIsOuterExpr )
{
/***********************************************************************
 *
 * Description :
 *     BUG-45226
 *     ���������� outerColumns �� ã�Ƽ� toViewColumns ȣ���Ѵ�.
 *
 ***********************************************************************/

    qmsOuterNode * sOuter;

    if ( aSetQuerySet->setOp == QMS_NONE )
    {
        for ( sOuter  = aSetQuerySet->SFWGH->outerColumns;
              sOuter != NULL;
              sOuter  = sOuter->next )
        {
            IDE_TEST( toViewColumns( aViewStatement,
                                     aViewTableRef,
                                     &( sOuter->column ),
                                     aIsOuterExpr )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_TEST( toViewSetOp( aViewStatement,
                               aViewTableRef,
                               aSetQuerySet->left,
                               aIsOuterExpr )
                  != IDE_SUCCESS );

        IDE_TEST( toViewSetOp( aViewStatement,
                               aViewTableRef,
                               aSetQuerySet->right,
                               aIsOuterExpr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::findAndRemoveSubquery( qcStatement * aStatement,
                                     qmsSFWGH    * aSFWGH,
                                     qtcNode     * aPredicate,
                                     idBool      * aResult )
{
/***********************************************************************
 *
 * Description :
 *     aPredicate���� aggregation subquery�� ã�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode * sArg;
    UShort  * sRelationMap;
    idBool    sRemovable;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::findAndRemoveSubquery::__FT__" );

    *aResult = ID_FALSE;

    if( aPredicate != NULL )
    {
        if( ( aPredicate->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
        {
            IDE_TEST( isRemovableSubquery( aStatement,
                                           aSFWGH,
                                           aPredicate,
                                           &sRemovable,
                                           &sRelationMap )
                      != IDE_SUCCESS );

            if( sRemovable == ID_TRUE )
            {
                IDE_TEST( removeSubquery( aStatement,
                                          aSFWGH,
                                          aPredicate,
                                          sRelationMap )
                          != IDE_SUCCESS );

                *aResult = ID_TRUE;

                // Removable subquery�� ������ �������ָ� �ȴ�.
                IDE_TEST( QC_QMP_MEM( aStatement )->free( sRelationMap )
                          != IDE_SUCCESS );
            }
            else
            {
                for( sArg = (qtcNode *)aPredicate->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    if( sArg->node.module != &qtc::subqueryModule )
                    {
                        IDE_TEST( findAndRemoveSubquery( aStatement,
                                                         aSFWGH,
                                                         sArg,
                                                         aResult )
                                  != IDE_SUCCESS );

                        if( *aResult == ID_TRUE )
                        {
                            // �� query�� �� ���ۿ� ������ �� �����Ƿ�
                            // �� �̻� �õ� ���� ����
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
qmoUnnesting::isRemovableSubquery( qcStatement  * aStatement,
                                   qmsSFWGH     * aSFWGH,
                                   qtcNode      * aSubqueryPredicate,
                                   idBool       * aResult,
                                   UShort      ** aRelationMap )
{
/***********************************************************************
 *
 * Description :
 *     Aggregation�� �����Ͽ� windowing view�� �����ϰ� ���� ������
 *     subquery���� �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcStatement  * sSQStatement;
    qmsParseTree * sSQParseTree;
    qmsSFWGH     * sSQSFWGH;
    qmsTarget    * sTarget;
    qmsAggNode   * sAggrNode;
    qmsFrom      * sFrom;
    qtcNode      * sSQNode;
    qtcNode      * sOuterNode;
    qcDepInfo      sDepInfo;
    qcDepInfo      sOuterCommonDepInfo;
    idBool         sResult;
    idBool         sIsEquivalent;
    qmsConcatElement * sGroup;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::isRemovableSubquery::__FT__" );

    *aRelationMap = NULL;

    // BUG-47616
    if ( SDU_SHARD_ENABLE == 1 )
    {
        IDE_CONT( UNREMOVABLE );
    }

    // BUG-43059 Target subquery unnest/removal disable
    if ( ( aSFWGH->thisQuerySet->lflag & QMV_QUERYSET_TARGET_SUBQUERY_REMOVAL_MASK )
         == QMV_QUERYSET_TARGET_SUBQUERY_REMOVAL_FALSE )
    {
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if( ( aSubqueryPredicate->node.lflag & MTC_NODE_COMPARISON_MASK )
        == MTC_NODE_COMPARISON_FALSE )
    {
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        switch( aSubqueryPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
        {
            case MTC_NODE_OPERATOR_EQUAL:
            case MTC_NODE_OPERATOR_NOT_EQUAL:
            case MTC_NODE_OPERATOR_GREATER:
            case MTC_NODE_OPERATOR_GREATER_EQUAL:
            case MTC_NODE_OPERATOR_LESS:
            case MTC_NODE_OPERATOR_LESS_EQUAL:
                break;
            default:
                // =, <>, >, >=, <, <= �� �� �����ڴ� �Ұ�
                IDE_CONT( UNREMOVABLE );
        }

        if( aSubqueryPredicate->node.arguments->next->module != &qtc::subqueryModule )
        {
            // �� �������� �� ��° ���ڰ� subquery�� �ƴ�
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            sSQNode = (qtcNode *)aSubqueryPredicate->node.arguments->next;
        }

        // BUG-46952 left�� list type�̸� �ȵ˴ϴ�.
        if( aSubqueryPredicate->node.arguments->module == &mtfList )
        {
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }

    }

    // Subquery�� ���� Ȯ��
    sSQStatement = sSQNode->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    if( sSQParseTree->querySet->setOp != QMS_NONE )
    {
        // SET ������ ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NO_UNNEST )
    {
        IDE_CONT( UNREMOVABLE );
    }
    else if ( sSQSFWGH->hints->subqueryUnnestType == QMO_SUBQUERY_UNNEST_TYPE_NOT_DEFINED )
    {
        if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
             == QC_TMP_UNNEST_SUBQUERY_FALSE )
        {
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            /* Noting to do */
        }
    }
    else
    {
        /* BUG-46544 Unnest hit */
        // Hint QMO_SUBQUERY_UNNEST_TYPE_UNNEST
        // hint�� �����ϰ� Property�� 0�̰� Compatibility�� 1 �̸� Property
        // �켱���� unnest�� ���� �ʴ´�.
        if ( ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_SUBQUERY_MASK )
               == QC_TMP_UNNEST_SUBQUERY_FALSE ) &&
             ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_COMPATIBILITY_1_MASK )
               == QC_TMP_UNNEST_COMPATIBILITY_1_TRUE ) )
        {
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            /* Nothing to do */
        }
    }

    if( ( aSFWGH->hierarchy != NULL ) ||
        ( sSQSFWGH->hierarchy != NULL ) )
    {
        // Hierarchy ������ ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQParseTree->limit != NULL )
    {
        // Subquery���� LIMIT�� ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }
    
    /* BUG-36580 supported TOP */
    if ( aSFWGH->top != NULL )
    {
        // Subquery���� TOP�� ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }
    
    if( ( sSQSFWGH->rownum != NULL ) ||
        ( sSQSFWGH->level  != NULL ) ||
        ( sSQSFWGH->isLeaf != NULL ) )
    {
        // Subquery���� ROWNUM, LEVEL, ISLEAF�� ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    for( sFrom = aSFWGH->from;
         sFrom != NULL;
         sFrom = sFrom->next )
    {
        if( sFrom->joinType != QMS_NO_JOIN )
        {
            // Ansi style join�� ����� ��� �Ұ�
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }
    }

    for( sFrom = sSQSFWGH->from;
         sFrom != NULL;
         sFrom = sFrom->next )
    {
        if( sFrom->joinType != QMS_NO_JOIN )
        {
            // Ansi style join�� ����� ��� �Ұ�
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-2418
        // sSQSFWGH�� From���� Lateral View�� �����ϸ� Removal �� �� ����.
        // ��, Lateral View�� ������ ��� Merging �Ǿ��ٸ� Removal�� �����ϴ�.
        IDE_TEST( qmvQTC::getFromLateralDepInfo( sFrom, & sDepInfo )
                  != IDE_SUCCESS );

        if ( qtc::haveDependencies( & sDepInfo ) == ID_TRUE )
        {
            // �ش� tableRef�� Lateral View��� Removal �Ұ�
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }
    }

    if( sSQSFWGH->group != NULL )
    {
        // GROUP BY���� �����ϴ� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->aggsDepth1 == NULL )
    {
        // Aggregate function�� ������� ���� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        for( sAggrNode = sSQSFWGH->aggsDepth1;
             sAggrNode != NULL;
             sAggrNode = sAggrNode->next )
        {
            if( ( sAggrNode->aggr->node.lflag & MTC_NODE_DISTINCT_MASK )
                == MTC_NODE_DISTINCT_TRUE )
            {
                // Aggregate function�� DISTINCT�� ��� �� �Ұ�
                IDE_CONT( UNREMOVABLE );
            }
            else
            {
                // Nothing to do.
            }

            if( sAggrNode->aggr->node.arguments == NULL )
            {
                // COUNT(*) ��� �� �Ұ�
                IDE_CONT( UNREMOVABLE );
            }
            else
            {
                // Nothing to do.
            }

            /* BUG-43703 WITHIN GROUP ���� ���� Subquery Unnesting�� ���� �ʵ��� �մϴ�. */
            IDE_TEST_CONT( sAggrNode->aggr->node.funcArguments != NULL, UNREMOVABLE );
        }
    }

    if( sSQSFWGH->aggsDepth2 != NULL )
    {
        // Nested aggregate function�� ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if( sSQSFWGH->having != NULL )
    {
        // BUG-41170 having ���� ����� ���
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    // Subsumption property Ȯ��
    IDE_TEST( isSubsumed( aStatement,
                          aSFWGH,
                          sSQSFWGH,
                          &sResult,
                          aRelationMap,
                          &sOuterCommonDepInfo )
              != IDE_SUCCESS );

    if( sResult == ID_FALSE )
    {
        // Subsumption property�� �������� ����
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    // Subquery�� aggregate function ���ڿ� outer query�� column�� ����
    // expression���� ��
    if( aSubqueryPredicate->node.arguments->module == &mtfList )
    {
        // List type�� ��� list�� argument
        sOuterNode = (qtcNode *)aSubqueryPredicate->node.arguments->arguments;
    }
    else
    {
        // List type�� �ƴ� ��� �ش� column
        sOuterNode = (qtcNode *)aSubqueryPredicate->node.arguments;
    }

    for( sTarget = sSQSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next, sOuterNode = (qtcNode *)sOuterNode->node.next )
    {
        if ( ( (sTarget->targetColumn->lflag & QTC_NODE_AGGREGATE_MASK ) != QTC_NODE_AGGREGATE_EXIST ) )
        {
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Aggregate function�� ���ڸ� outer query�� relation���� �ٲ�
            IDE_TEST( changeRelation( aStatement,
                                      sTarget->targetColumn,
                                      &sSQSFWGH->depInfo,
                                      *aRelationMap )
                      != IDE_SUCCESS );

            if( ( qtc::dependencyContains( &sOuterCommonDepInfo,
                                           &sOuterNode->depInfo ) == ID_TRUE ) &&
                ( qtc::dependencyContains( &sOuterCommonDepInfo,
                                           &sTarget->targetColumn->depInfo ) == ID_TRUE ) )
            {
                sIsEquivalent = ID_TRUE;
            }
            else
            {
                sIsEquivalent = ID_FALSE;
            }

            // �ٲ���� relation�� �ٽ� ����
            IDE_TEST( changeRelation( aStatement,
                                      sTarget->targetColumn,
                                      &sOuterCommonDepInfo,
                                      *aRelationMap )
                      != IDE_SUCCESS );

            if( sIsEquivalent == ID_FALSE )
            {
                IDE_CONT( UNREMOVABLE );
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    // BUG-45226 ���������� target �� ���������� ������ ������ �߻��մϴ�.
    // remove �϶��� ���´�.
    for( sTarget = aSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        if ( QTC_HAVE_SUBQUERY( sTarget->targetColumn ) == ID_TRUE )
        {
            if ( isRemovableTarget( sTarget->targetColumn, &sOuterCommonDepInfo ) == ID_FALSE )
            {
                IDE_CONT( UNREMOVABLE );
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

    // BUG-45760
    for ( sGroup = aSFWGH->group;
         sGroup != NULL;
         sGroup = sGroup->next )
    {
        if ( ( sGroup->type == QMS_GROUPBY_ROLLUP ) ||
             ( sGroup->type == QMS_GROUPBY_CUBE ) )
        {
            IDE_RAISE( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }
    }
            
    *aResult = ID_TRUE;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( UNREMOVABLE );

    *aResult = ID_FALSE;

    if( *aRelationMap != NULL )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->free( *aRelationMap )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    *aRelationMap = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmoUnnesting::isRemovableTarget( qtcNode   * aNode,
                                        qcDepInfo * aOuterCommonDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     BUG-45226
 *     1. target �� ���������� �ִ��� �˻��Ѵ�.
 *     2. ���������� Removable ���̺��� �����ϴ��� �˻��Ѵ�.
 *     3. �����Ѵٸ� Removable �� �ȵǵ��� �Ѵ�.
 *        ������ ������ ���� �߻��ؼ� �̴�.
 *
 ***********************************************************************/
    idBool      sIsRemovable = ID_TRUE;
    mtcNode   * sNode;
    qcDepInfo   sAndDepInfo;

    if( aNode->node.module != &qtc::subqueryModule )
    {
        for ( sNode = aNode->node.arguments;
              (sNode != NULL) && (sIsRemovable == ID_TRUE);
              sNode = sNode->next )
        {
            if ( QTC_HAVE_SUBQUERY( (qtcNode*)sNode ) == ID_TRUE )
            {
                sIsRemovable = isRemovableTarget( (qtcNode*)sNode, aOuterCommonDepInfo );
            }
            else
            {
                // Nothing to do.
            }
        }
    }
    else
    {
        qtc::dependencyAnd( aOuterCommonDepInfo,
                            &aNode->depInfo,
                            &sAndDepInfo );
        
        if ( qtc::haveDependencies( &sAndDepInfo ) == ID_TRUE )
        {
            sIsRemovable = ID_FALSE;
        }
        else
        {
            sIsRemovable = ID_TRUE;
        }
    }
    
    return sIsRemovable;
}

IDE_RC
qmoUnnesting::isSubsumed( qcStatement  * aStatement,
                          qmsSFWGH     * aOQSFWGH,
                          qmsSFWGH     * aSQSFWGH,
                          idBool       * aResult,
                          UShort      ** aRelationMap,
                          qcDepInfo    * aOuterCommonDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     Outer query�� subquery�� relation���� ��� �����ϰ� �ִ���
 *     Ȯ���Ѵ�.
 *
 * Implementation :
 *     FROM���� relation, �׸��� predicate���� ������ Ȯ���Ѵ�.
 *
 ***********************************************************************/

    qcDepInfo      sDepInfo;
    UShort       * sRelationMap = NULL;
    SInt           sTable;
    idBool         sIsEquivalent;
    idBool         sExistSubquery = ID_FALSE;
    qmoPredList  * sOQPredList = NULL;
    qmoPredList  * sSQPredList = NULL;
    qmoPredList  * sPredNode1;
    qmoPredList  * sPredNode2;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::isSubsumed::__FT__" );

    // �������� relation�� ������ Ȯ��
    // Outer query�� subquery�� �����ϴ� relation���� ��� �����ؾ� ��
    IDE_TEST( createRelationMap( aStatement, aOQSFWGH, aSQSFWGH, &sRelationMap )
              != IDE_SUCCESS );

    if( sRelationMap == NULL )
    {
        // Outer query���� subquery�� relation�� ��� �������� ����
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    // Outer query�� subquery ��� predicate�� AND�θ� �����Ǿ�� ��
    if( isConjunctiveForm( aSQSFWGH->where ) == ID_FALSE )
    {
        // Subquery���� AND �� �������ڸ� ������ �� ����
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    if( isConjunctiveForm( aOQSFWGH->where ) == ID_FALSE )
    {
        // Outer query������ AND �� �������ڸ� ������ �� ����
        IDE_CONT( UNREMOVABLE );
    }
    else
    {
        // Nothing to do.
    }

    // �� query�� ���� relation�� subquery���� ������ �� ����
    sTable = qtc::getPosFirstBitSet( &aSQSFWGH->outerDepInfo );

    while( sTable != QTC_DEPENDENCIES_END )
    {
        if( sRelationMap[sTable] != ID_USHORT_MAX )
        {
            // Outer query���� ���� relation�� subquery���� �����ϸ� �ȵ�
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }

        sTable = qtc::getPosNextBitSet( &aSQSFWGH->outerDepInfo, sTable );
    }

    // ���� relation�� �� outer query�� relation ������ ����
    qtc::dependencyClear( aOuterCommonDepInfo );

    sTable = qtc::getPosFirstBitSet( &aOQSFWGH->depInfo );

    while( sTable != QTC_DEPENDENCIES_END )
    {
        if( sRelationMap[sTable] != ID_USHORT_MAX )
        {
            qtc::dependencySet( sTable, &sDepInfo );
            IDE_TEST( qtc::dependencyOr( aOuterCommonDepInfo,
                                         &sDepInfo,
                                         aOuterCommonDepInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
        sTable = qtc::getPosNextBitSet( &aOQSFWGH->depInfo, sTable );
    }

    IDE_TEST( genPredicateList( aStatement,
                                aOQSFWGH->where,
                                &sOQPredList )
              != IDE_SUCCESS );

    IDE_TEST( genPredicateList( aStatement,
                                aSQSFWGH->where,
                                &sSQPredList )
              != IDE_SUCCESS );

    // ���� relation�鿡 ���Ͽ� ������ predicate���� ������ Ȯ��
    for( sPredNode1 = sOQPredList;
         sPredNode1 != NULL;
         sPredNode1 = sPredNode1->next )
    {
        qtc::dependencyAnd( aOuterCommonDepInfo,
                            &sPredNode1->predicate->depInfo,
                            &sDepInfo );

        if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
        {
            if( ( sPredNode1->predicate->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
            {
                if( sExistSubquery == ID_FALSE )
                {
                    // Subquery�� �� 1��(removing ���)�� ������ �� �ִ�.
                    sExistSubquery = ID_TRUE;
                    continue;
                }
                else
                {
                    // �� �� �̻� �����ϴ� ���
                    IDE_CONT( UNREMOVABLE );
                }
            }
            else
            {
                // Nothing to do.
            }

            if( ( qtc::haveDependencies( &aSQSFWGH->outerDepInfo ) == ID_FALSE ) &&
                ( qtc::dependencyContains( aOuterCommonDepInfo, &sPredNode1->predicate->depInfo ) == ID_FALSE ) )
            {
                // Subquery�� correlation�� �����鼭 ���� relation������
                // predicate�� �ƴ� ��� Ȯ������ �ʴ´�.
                continue;
            }
            else
            {
                // Nothing to do.
            }

            for( sPredNode2 = sSQPredList;
                 sPredNode2 != NULL;
                 sPredNode2 = sPredNode2->next )
            {
                sIsEquivalent = ID_TRUE;
                
                // Subquery�� predicate�� outer query�� ���� relation����
                // �����ϴ� predicate���� �Ͻ������� ����
                IDE_TEST( changeRelation( aStatement,
                                          sPredNode2->predicate,
                                          &aSQSFWGH->depInfo,
                                          sRelationMap )
                          != IDE_SUCCESS );

                // ��
                IDE_TEST( qtc::isEquivalentPredicate( aStatement,
                                                      sPredNode1->predicate,
                                                      sPredNode2->predicate,
                                                      &sIsEquivalent )
                          != IDE_SUCCESS );

                // �����Ͽ��� predicate���� �ٽ� ���󺹱�
                IDE_TEST( changeRelation( aStatement,
                                          sPredNode2->predicate,
                                          aOuterCommonDepInfo,
                                          sRelationMap )
                          != IDE_SUCCESS );

                if( sIsEquivalent == ID_TRUE )
                {
                    // �����ϴٰ� �Ǵܵ� predicate�� hit flag ����
                    sPredNode2->hit = ID_TRUE;

                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( sPredNode2 == NULL )
            {
                // ������ predicate�� ã�� ���� ���
                IDE_CONT( UNREMOVABLE );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // ���� relation��� ������� predicate
        }
    }

    // Subquery�� ��� predicate�� hit flag�� �����Ǿ����� Ȯ��
    for( sPredNode2 = sSQPredList;
         sPredNode2 != NULL;
         sPredNode2 = sPredNode2->next )
    {
        if( sPredNode2->hit == ID_FALSE )
        {
            IDE_CONT( UNREMOVABLE );
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_TEST( freePredicateList( aStatement, sOQPredList )
              != IDE_SUCCESS );
    IDE_TEST( freePredicateList( aStatement, sSQPredList )
              != IDE_SUCCESS );

    *aRelationMap = sRelationMap;
    *aResult      = ID_TRUE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( UNREMOVABLE );

    if( sRelationMap != NULL )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->free( sRelationMap )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( freePredicateList( aStatement, sOQPredList )
              != IDE_SUCCESS );
    IDE_TEST( freePredicateList( aStatement, sSQPredList )
              != IDE_SUCCESS );

    *aRelationMap = NULL;
    *aResult      = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::changeRelation( qcStatement * aStatement,
                              qtcNode     * aPredicate,
                              qcDepInfo   * aDepInfo,
                              UShort      * aRelationMap )
{
/***********************************************************************
 *
 * Description :
 *     aPredicate�� ���Ե� node�� �� aDepInfo�� ���ԵǴ� node����
 *     table���� aRelationMap�� ���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode   * sArg;
    mtcNode   * sConversion;
    qcDepInfo   sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::changeRelation::__FT__" );

    qtc::dependencyAnd( aDepInfo, &aPredicate->depInfo, &sDepInfo );

    if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
    {
        if( aPredicate->node.module == &qtc::columnModule )
        {
            aPredicate->node.table = aRelationMap[aPredicate->node.table];

            // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
            // lob�� ���� ������ �Լ������� baseTable �� ����Ѵ�.
            // ���� ���� �������־�� �Ѵ�.
            sConversion = aPredicate->node.conversion;
            while ( sConversion != NULL )
            {
                sConversion->baseTable = aPredicate->node.table;

                sConversion = sConversion->next;
            }

            qtc::dependencySet( aPredicate->node.table,
                                &aPredicate->depInfo );

            // BUG-41141 estimate �� �Ǿ��ٰ� �����Ҽ� �����Ƿ�
            // estimate �� ���־�� �Ѵ�.
            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        aPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            for( sArg = (qtcNode *)aPredicate->node.arguments;
                 sArg != NULL;
                 sArg = (qtcNode *)sArg->node.next )
            {
                changeRelation( aStatement,
                                sArg,
                                aDepInfo,
                                aRelationMap );
            }

            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     aPredicate )
                      != IDE_SUCCESS );
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
qmoUnnesting::createRelationMap( qcStatement  * aStatement,
                                 qmsSFWGH     * aOQSFWGH,
                                 qmsSFWGH     * aSQSFWGH,
                                 UShort      ** aRelationMap )
{
/***********************************************************************
 *
 * Description :
 *     Relation map �ڷᱸ���� �����Ѵ�.
 *     ex) SELECT * FROM t1, t2, t3 WHERE ... AND t1.c1 IN
 *             (SELECT AVG(t1.c1) FROM t1, t2 ... )
 *         �� �� tuple-set�� ������ relation map�� ������ ����.
 *         | Idx. | Description        | Map |
 *         | 0    | Intermediate tuple | N/A |
 *         | 1    | T1(outer query)    | 4   |
 *         | 2    | T2(outer query)    | 5   |
 *         | 3    | T3(outer query)    | N/A |
 *         | 4    | T1(subquery)       | 1   |
 *         | 5    | T2(subquery)       | 2   |
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom * sOQFrom;
    qmsFrom * sSQFrom;
    UShort  * sRelationMap = NULL;
    UShort    sRowCount;
    UShort    i;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::createRelationMap::__FT__" );

    sRowCount = QC_SHARED_TMPLATE( aStatement )->tmplate.rowCount;

    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( sRowCount * ID_SIZEOF( UShort ),
                                               (void**)&sRelationMap )
              != IDE_SUCCESS );

    for( i = 0; i < sRowCount; i++ )
    {
        sRelationMap[i] = ID_USHORT_MAX;
    }

    for( sSQFrom = aSQSFWGH->from;
         sSQFrom != NULL;
         sSQFrom = sSQFrom->next )
    {
        if( sSQFrom->tableRef->tableInfo->tableID == 0 )
        {
            // Inline view�� ���� �� ����.
            IDE_CONT( UNABLE );
        }
        else
        {
            // Nothing to do.
        }

        for( sOQFrom = aOQSFWGH->from;
             sOQFrom != NULL;
             sOQFrom = sOQFrom->next )
        {
            if( sSQFrom->tableRef->tableInfo->tableID
                == sOQFrom->tableRef->tableInfo->tableID )
            {
                if( sRelationMap[sOQFrom->tableRef->table] != ID_USHORT_MAX )
                {
                    // �̹� mapping �� relation�� ���
                }
                else
                {
                    sRelationMap[sSQFrom->tableRef->table] = sOQFrom->tableRef->table;
                    sRelationMap[sOQFrom->tableRef->table] = sSQFrom->tableRef->table;
                    break;
                }
            }
            else
            {
                // Nothing to do.
            }
        }

        if( sOQFrom == NULL )
        {
            // sSQFrom�� ������ relation�� outer query���� ã�� ����
            IDE_CONT( UNABLE );
        }
        else
        {
            // Nothing to do.
        }
    }

    *aRelationMap = sRelationMap;

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( UNABLE );

    if( sRelationMap != NULL )
    {
        IDE_TEST( QC_QMP_MEM( aStatement )->free( sRelationMap )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    *aRelationMap = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoUnnesting::isConjunctiveForm( qtcNode * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     ���ڷ� ���� predicate�� conjunctive form���� Ȯ���Ѵ�.
 *
 * Implementation :
 *     AND �� �������ڰ� ���Ե��� �ʾҴ��� ��������� Ȯ���Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sArg;

    if( aPredicate != NULL )
    {
        if( ( aPredicate->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            if( ( aPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_AND )
            {
                for( sArg = (qtcNode *)aPredicate->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    if( isConjunctiveForm( sArg ) == ID_FALSE )
                    {
                        IDE_CONT( NOT_APPLICABLE_EXIT );
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                IDE_CONT( NOT_APPLICABLE_EXIT );
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

    return ID_TRUE;

    IDE_EXCEPTION_CONT( NOT_APPLICABLE_EXIT );

    return ID_FALSE;
}

IDE_RC
qmoUnnesting::genPredicateList( qcStatement  * aStatement,
                                qtcNode      * aPredicate,
                                qmoPredList ** aPredList )
{
/***********************************************************************
 *
 * Description :
 *     �� �����ڰ� �ƴ� predicate���� ��� ã�� list�� �����Ѵ�.
 *     ex) A AND (B AND C), A AND B AND C
 *         => A, B, C
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredList * sFirst = NULL;
    qmoPredList * sLast  = NULL;
    qmoPredList * sPredNode;
    qtcNode     * sArg;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genPredicateList::__FT__" );

    if( aPredicate != NULL )
    {
        if( ( aPredicate->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            for( sArg = (qtcNode *)aPredicate->node.arguments;
                 sArg != NULL;
                 sArg = (qtcNode *)sArg->node.next )
            {
                IDE_TEST( genPredicateList( aStatement,
                                            sArg,
                                            &sPredNode )
                          != IDE_SUCCESS );
                if( sFirst == NULL )
                {
                    sFirst = sLast = sPredNode;
                }
                else
                {
                    sLast->next = sPredNode;
                }

                while( sLast->next != NULL )
                {
                    sLast = sLast->next;
                }
            }

            *aPredList = sFirst;
        }
        else
        {
            IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoPredList ),
                                                       (void **)&sPredNode )
                      != IDE_SUCCESS );

            sPredNode->predicate = aPredicate;
            sPredNode->next      = NULL;
            sPredNode->hit       = ID_FALSE;

            *aPredList = sPredNode;
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
qmoUnnesting::freePredicateList( qcStatement * aStatement,
                                 qmoPredList * aPredList )
{
/***********************************************************************
 *
 * Description :
 *     aPredList�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredList * sPredNode = aPredList;
    qmoPredList * sPrevPredNode;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::freePredicateList::__FT__" );

    while( sPredNode != NULL )
    {
        sPrevPredNode = sPredNode;
        sPredNode = sPredNode->next;

        IDE_TEST( QC_QMP_MEM( aStatement )->free( sPrevPredNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::changeSemiJoinInnerTable( qmsSFWGH * aSFWGH,
                                        qmsSFWGH * aViewSFWGH,
                                        SInt       aViewID )
{
/***********************************************************************
 *
 * Description :
 *     Subquery ���Ž� remove ��� subquery�� table�� semi/anti join��
 *     inner table�� ��� ��ȯ�� view�� inner table�� ����Ű���� �Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsFrom   * sFrom;
    qcDepInfo   sDepInfo;
    SInt        sTable;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::changeSemiJoinInnerTable::__FT__" );

    for( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        if( qtc::haveDependencies( &sFrom->semiAntiJoinDepInfo ) == ID_TRUE )
        {
            qtc::dependencyAnd( &sFrom->semiAntiJoinDepInfo,
                                &aViewSFWGH->depInfo,
                                &sDepInfo );

            if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
            {
                sTable = qtc::getPosFirstBitSet( &aViewSFWGH->depInfo );
                while( sTable != QTC_DEPENDENCIES_END )
                {
                    qtc::dependencyChange( sTable,
                                           aViewID,
                                           &sFrom->semiAntiJoinDepInfo,
                                           &sFrom->semiAntiJoinDepInfo );
                    sTable = qtc::getPosNextBitSet( &aViewSFWGH->depInfo, sTable );
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

IDE_RC
qmoUnnesting::removeSubquery( qcStatement * aStatement,
                              qmsSFWGH    * aSFWGH,
                              qtcNode     * aPredicate,
                              UShort      * aRelationMap )
{
/***********************************************************************
 *
 * Description :
 *     TPC-H�� 2, 15, 17�� query�� ���� subquery�� outer query��
 *     ���� relation���� ���� subquery���� aggregate function�� ����ϴ�
 *     ����� transformation�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcStatement    * sSQStatement;
    qtcNode        * sIsNotNull[2];
    qtcNode        * sCol1Node[2];
    qtcNode        * sSQNode;
    mtcNode        * sNext;
    qcNamePosition   sEmptyPosition;
    qmsTarget      * sTarget;
    qmsParseTree   * sParseTree = NULL;
    qmsParseTree   * sSQParseTree;
    qmsSFWGH       * sSQSFWGH;
    qmsFrom        * sViewFrom;
    qmsFrom        * sFrom;
    qmsFrom       ** sDoublePointer;
    qmsSortColumns * sSort;
    idBool           sIsCorrelatedSQ;

    qmsConcatElement * sGroup;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::removeSubquery::__FT__" );

    sSQNode      = (qtcNode *)aPredicate->node.arguments->next;
    sSQStatement = sSQNode->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    /* TASK-7219 Shard Transformer Refactoring */
    if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                        SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
         == ID_TRUE )
    {
        IDE_TEST( sdi::preAnalyzeQuerySet( sSQStatement,
                                           sSQParseTree->querySet,
                                           QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if( qtc::haveDependencies( &sSQSFWGH->outerDepInfo ) == ID_TRUE )
    {
        sIsCorrelatedSQ = ID_TRUE;
    }
    else
    {
        sIsCorrelatedSQ = ID_FALSE;
    }

    IDE_TEST( transformToCase2Expression( aPredicate )
              != IDE_SUCCESS );

    IDE_TEST( transformAggr2Window( sSQStatement, sSQSFWGH->target->targetColumn, aRelationMap )
              != IDE_SUCCESS );

    sSQSFWGH->aggsDepth1 = NULL;

    // Outer query�� FROM���� relation���� subquery�� �̵��Ѵ�.
    sSQSFWGH->where = NULL;
    if( sIsCorrelatedSQ == ID_TRUE )
    {
        // Correlated subquery�� ��� ��� relation���� �ű��.
        sSQSFWGH->from = aSFWGH->from;

        // Subquery�� dependency ����
        qtc::dependencySetWithDep( &sSQSFWGH->depInfo, &aSFWGH->depInfo );
        qtc::dependencySetWithDep( &sSQSFWGH->thisQuerySet->depInfo, &aSFWGH->thisQuerySet->depInfo );
    }
    else
    {
        // Uncorrelated subquery�� ��� ���� relation�鸸 �ű��.
        sSQSFWGH->from = NULL;
        sDoublePointer = &aSFWGH->from;

        while( *sDoublePointer != NULL )
        {
            sFrom = *sDoublePointer;

            if( aRelationMap[sFrom->tableRef->table] != ID_USHORT_MAX )
            {
                *sDoublePointer = sFrom->next;

                sFrom->next = sSQSFWGH->from;
                sSQSFWGH->from = sFrom;

                // Subquery�� dependency ����
                qtc::dependencyChange( aRelationMap[sFrom->tableRef->table],
                                       sFrom->tableRef->table,
                                       &sSQSFWGH->depInfo,
                                       &sSQSFWGH->depInfo );

                qtc::dependencyChange( aRelationMap[sFrom->tableRef->table],
                                       sFrom->tableRef->table,
                                       &sSQSFWGH->thisQuerySet->depInfo,
                                       &sSQSFWGH->thisQuerySet->depInfo );
            }
            else
            {
                sDoublePointer = &sFrom->next;
            }
        }
    }

    // View�� ��ȯ�� ���̹Ƿ� outer dependency�� ����� �Ѵ�.
    qtc::dependencyClear( &sSQSFWGH->outerDepInfo );

    // PROJ-2418 View�� ��ȯ�� ��Ȳ������ Lateral View�� �����Ƿ�
    // lateralDepInfo ������ ���� �����Ѵ�.
    qtc::dependencyClear( &sSQParseTree->querySet->lateralDepInfo );

    // Outer query�� WHERE�� ������ view(������ subquery)�� �ű��.
    IDE_TEST( movePredicates( sSQStatement, &aSFWGH->where, sSQSFWGH )
              != IDE_SUCCESS );

    // WHERE���� �����ϴ� column���� view���� ��ȯ�Ѵ�.
    for( sTarget = aSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        IDE_TEST( genViewSelect( sSQStatement, sTarget->targetColumn, ID_TRUE )
                  != IDE_SUCCESS );
    }

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;

    if( sParseTree->querySet == aSFWGH->thisQuerySet )
    {
        for( sSort = sParseTree->orderBy;
             sSort != NULL;
             sSort = sSort->next )
        {
            IDE_TEST( genViewSelect( sSQStatement, sSort->sortColumn, ID_TRUE )
                      != IDE_SUCCESS );
        }

        // BUGBUG
        /*
        for ( sGroup = sParseTree->querySet->SFWGH->group;
             sGroup != NULL;
             sGroup = sGroup->next )
        {
            IDE_TEST( genViewSelect( sSQStatement, sGroup->arithmeticOrList, ID_TRUE )
                      != IDE_SUCCESS );
        } 
        */
    }
    else
    {
        // Nothing to do.
    }

    // View�� SELECT���� �����Ѵ�.
    IDE_TEST( genViewSelect( sSQStatement, aSFWGH->where, ID_TRUE )
              != IDE_SUCCESS );

    // BUG-42113 LOB type �� ���� subquery ��ȯ�� ����Ǿ�� �մϴ�.
    // unnest �������� ������ view�� target�� lob �÷��� ������� LobLocatorFunc�� �������ش�.
    // view�� �ݵ�� lobLocator Ÿ������ ��ȯ�Ǿ�� �Ѵ�.
    IDE_TEST( qmvQuerySet::addLobLocatorFunc( sSQStatement, sSQSFWGH->target )
              != IDE_SUCCESS );

    // Subquery�� view�� ��ȯ
    IDE_TEST( transformSubqueryToView( aStatement,
                                       sSQNode,
                                       &sViewFrom )
              != IDE_SUCCESS );

    // View�� ����� table�� semi/anti join�� inner table�̾��� ���
    // inner table�� view�� ����Ű���� �Ѵ�.
    IDE_TEST( changeSemiJoinInnerTable( aSFWGH,
                                        sSQSFWGH,
                                        sViewFrom->tableRef->table )
              != IDE_SUCCESS );

    // Subquery predicate�� �ִ� �ڸ��� COL1 IS NOT NULL�� �����Ѵ�.
    SET_EMPTY_POSITION( sEmptyPosition );

    IDE_TEST( qtc::makeColumn( aStatement,
                               sCol1Node,
                               NULL,
                               NULL,
                               NULL,
                               NULL )
              != IDE_SUCCESS );

    sCol1Node[0]->node.module     = &qtc::columnModule;
    sCol1Node[0]->node.table      = sViewFrom->tableRef->table;
    sCol1Node[0]->node.column     = 0;
    sCol1Node[0]->node.baseTable  = sCol1Node[0]->node.table;
    sCol1Node[0]->node.baseColumn = sCol1Node[0]->node.column;
    sCol1Node[0]->node.next       = NULL;

    SET_POSITION( sCol1Node[0]->tableName, sViewFrom->tableRef->aliasName );
    sCol1Node[0]->columnName.stmtText = sSQSFWGH->target->aliasColumnName.name;
    sCol1Node[0]->columnName.size     = sSQSFWGH->target->aliasColumnName.size;
    sCol1Node[0]->columnName.offset   = 0;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sCol1Node[0] )
              != IDE_SUCCESS );

    IDE_TEST( qmvQTC::addViewColumnRefList( aStatement,
                                            sCol1Node[0] )
              != IDE_SUCCESS );

    IDE_TEST( qtc::makeNode( aStatement,
                             sIsNotNull,
                             &sEmptyPosition,
                             &mtfIsNotNull )
              != IDE_SUCCESS );

    sIsNotNull[0]->node.arguments = (mtcNode *)sCol1Node[0];

    IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                             sIsNotNull[0] )
              != IDE_SUCCESS );

    sNext = aPredicate->node.next;
    idlOS::memcpy( aPredicate, sIsNotNull[0], ID_SIZEOF( qtcNode ) );
    aPredicate->node.next = sNext;

    // Outer query�� �� clause���� �����ϴ� column���� view column�� �����ϵ��� �����Ѵ�.
    if( sParseTree->querySet == aSFWGH->thisQuerySet )
    {
        for( sSort = sParseTree->orderBy;
             sSort != NULL;
             sSort = sSort->next )
        {
            IDE_TEST( toViewColumns( sSQStatement,
                                     sViewFrom->tableRef,
                                     &sSort->sortColumn,
                                     ID_TRUE )
                      != IDE_SUCCESS );
        }

        // BUG-38228
        // Outer query�� group by ���� view�� �����Ѿ� �Ѵ�.
        for( sGroup = sParseTree->querySet->SFWGH->group;
             sGroup != NULL;
             sGroup = sGroup->next )
        {
            IDE_TEST( toViewColumns( sSQStatement,
                                     sViewFrom->tableRef,
                                     &sGroup->arithmeticOrList,
                                     ID_TRUE )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    for( sTarget = aSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next )
    {
        IDE_TEST( toViewColumns( sSQStatement,
                                 sViewFrom->tableRef,
                                 &sTarget->targetColumn,
                                 ID_TRUE )
                  != IDE_SUCCESS );
    }

    IDE_TEST( toViewColumns( sSQStatement,
                             sViewFrom->tableRef,
                             &aSFWGH->where,
                             ID_TRUE )
              != IDE_SUCCESS );

    if( sIsCorrelatedSQ == ID_TRUE )
    {
        aSFWGH->from = sViewFrom;
    }
    else
    {
        sViewFrom->next = aSFWGH->from;
        aSFWGH->from = sViewFrom;
    }

    // Dependency ����
    qtc::dependencyClear( &aSFWGH->depInfo );
    qtc::dependencyClear( &aSFWGH->thisQuerySet->depInfo );

    for( sFrom = aSFWGH->from;
         sFrom != NULL;
         sFrom = sFrom->next )
    {
        IDE_TEST( qtc::dependencyOr( &aSFWGH->depInfo,
                                     &sFrom->depInfo,
                                     &aSFWGH->depInfo )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::dependencyOr( &aSFWGH->thisQuerySet->depInfo,
                                     &sFrom->depInfo,
                                     &aSFWGH->thisQuerySet->depInfo )
                  != IDE_SUCCESS );
    }

    IDE_TEST( qmoViewMerging::validateNode( aStatement,
                                            aSFWGH->where )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::transformToCase2Expression( qtcNode * aSubqueryPredicate )
{
/***********************************************************************
 *
 * Description :
 *     Subquery predicate�� �̿��Ͽ� SELECT���� CASE2 expression����
 *     ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcStatement    * sSQStatement;
    qmsParseTree   * sSQParseTree;
    qmsSFWGH       * sSQSFWGH;
    qtcNode        * sCase2[2];
    qtcNode        * sCorrPred;
    qtcNode        * sConstNode = NULL;
    qcNamePosition   sEmptyPosition;
    SChar          * sColumnName;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::transformToCase2Expression::__FT__" );

    sSQStatement = ((qtcNode *)aSubqueryPredicate->node.arguments->next)->subquery;
    sSQParseTree = (qmsParseTree *)sSQStatement->myPlan->parseTree;
    sSQSFWGH     = sSQParseTree->querySet->SFWGH;

    IDE_TEST( genCorrPredicates( sSQStatement,
                                 aSubqueryPredicate,
                                 ID_FALSE /* aExistsTrans */,
                                 &sCorrPred )
              != IDE_SUCCESS );

    IDE_TEST( makeDummyConstant( sSQStatement,
                                 &sConstNode )
              != IDE_SUCCESS );

    SET_EMPTY_POSITION( sEmptyPosition );

    IDE_TEST( qtc::makeNode( sSQStatement,
                             sCase2,
                             &sEmptyPosition,
                             &mtfCase2 )
              != IDE_SUCCESS );

    // Argument���� �����Ѵ�.
    // ex) CASE2( T1.C1 = AVG(T1.C1) OVER (PARTITION BY ...), '0' )
    sCase2[0]->node.arguments = (mtcNode *)sCorrPred;
    sCorrPred->node.next      = (mtcNode *)sConstNode;

    IDE_TEST( qtc::estimateNodeWithArgument( sSQStatement,
                                             sCase2[0] )
              != IDE_SUCCESS );

    // COL1�� alias�� �����Ѵ�.
    IDE_TEST( QC_QMP_MEM( sSQStatement )->alloc( COLUMN_NAME_LENGTH,
                                                 (void**)&sColumnName )
              != IDE_SUCCESS );

    idlOS::strncpy( (SChar*)sColumnName,
                    COLUMN_NAME_PREFIX"1",
                    COLUMN_NAME_LENGTH );

    sSQSFWGH->target->targetColumn         = sCase2[0];
    sSQSFWGH->target->aliasColumnName.name = sColumnName;
    sSQSFWGH->target->aliasColumnName.size = idlOS::strlen( sColumnName );
    sSQSFWGH->target->next                 = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::transformAggr2Window( qcStatement * aStatement,
                                    qtcNode     * aNode,
                                    UShort      * aRelationMap )
{
/***********************************************************************
 *
 * Description :
 *     SELECT���� aggregate function�� window function���� ��ȯ�Ѵ�.
 *     �� �� PARTITION BY���� expression�� WHERE���� correlation��� �Ѵ�.
 *
 * Implementation :
 *     Correlation predicate���� ã�� PARTITION BY���� �����Ѵ�.
 *
 ***********************************************************************/

    qmsParseTree  * sParseTree;
    qmsSFWGH      * sSFWGH;
    qtcOver       * sOver;
    qtcNode       * sArg;
    qtcOverColumn * sPartitions;
    qtcOverColumn * sPartition;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::transformAggr2Window::__FT__" );

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    sSFWGH     = sParseTree->querySet->SFWGH;

    if( ( aNode->lflag & QTC_NODE_AGGREGATE_MASK )
        == QTC_NODE_AGGREGATE_EXIST )
    {
        if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_AGGREGATION )
        {
            IDE_TEST( QC_QMP_MEM( aStatement )->cralloc( ID_SIZEOF( qtcOver ),
                                                         (void **)&sOver )
                      != IDE_SUCCESS );

            // Aggregate function�� argument�� outer query�� relation���� �����Ѵ�.
            for( sArg = (qtcNode *)aNode->node.arguments;
                 sArg != NULL;
                 sArg = (qtcNode *)sArg->node.next )
            {
                IDE_TEST( changeRelation( aStatement,
                                          (qtcNode *)sArg,
                                          &sSFWGH->depInfo,
                                          aRelationMap )
                          != IDE_SUCCESS );
            }

            sPartitions = NULL;

            IDE_TEST( genPartitions( aStatement,
                                     sSFWGH,
                                     sSFWGH->where,
                                     &sPartitions )
                      != IDE_SUCCESS );

            // PARTITION BY���� column���� outer query�� relation���� �����Ѵ�.
            for( sPartition = sPartitions;
                 sPartition != NULL;
                 sPartition = sPartition->next )
            {
                IDE_TEST( changeRelation( aStatement,
                                          sPartition->node,
                                          &sSFWGH->depInfo,
                                          aRelationMap )
                          != IDE_SUCCESS );
            }

            sOver->overColumn        = sPartitions;
            sOver->partitionByColumn = sPartitions;
            SET_EMPTY_POSITION( sOver->endPos );
            aNode->overClause = sOver;

            IDE_TEST( qtc::estimateWindowFunction( aStatement,
                                                   sSFWGH,
                                                   aNode )
                      != IDE_SUCCESS );
        }
        else
        {
            for( sArg = (qtcNode *)aNode->node.arguments;
                 sArg != NULL;
                 sArg = (qtcNode *)sArg->node.next )
            {
                IDE_TEST( transformAggr2Window( aStatement,
                                                sArg,
                                                aRelationMap )
                          != IDE_SUCCESS );
            }

            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     aNode )
                      != IDE_SUCCESS );
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
qmoUnnesting::movePredicates( qcStatement  * aStatement,
                              qtcNode     ** aPredicate,
                              qmsSFWGH     * aSFWGH )
{
/***********************************************************************
 *
 * Description :
 *     aPredicate�� aSFWGH�� WHERE���� �ű��.
 *     �Ű��� predicate�� ���� ��ġ���� ���ŵǾ�� �ϹǷ� double pointer��
 *     �Ѱ� �޴´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode    * sNode;
    qtcNode    * sOld;
    qtcNode    * sConcatPred;
    qtcNode   ** sDoublePointer;
    qcDepInfo    sDepInfo;
    ULong        sMask;
    ULong        sCondition;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::movePredicates::__FT__" );

    sNode = *aPredicate;

    qtc::dependencyAnd( &sNode->depInfo,
                        &aSFWGH->depInfo,
                        &sDepInfo );

    if( qtc::haveDependencies( &sDepInfo ) )
    {
        if( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            IDE_FT_ERROR( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                          == MTC_NODE_OPERATOR_AND );

            sDoublePointer = (qtcNode **)&sNode->node.arguments;

            while( *sDoublePointer != NULL )
            {
                sOld = *sDoublePointer;

                IDE_TEST( movePredicates( aStatement,
                                          sDoublePointer,
                                          aSFWGH )
                          != IDE_SUCCESS );
                if( sOld == *sDoublePointer )
                {
                    sDoublePointer = (qtcNode **)&(*sDoublePointer)->node.next;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if( sNode->node.arguments == NULL )
            {
                // AND�� operand�� ��� ���ŵ� ���
                *aPredicate = NULL;
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            sMask      = ( QTC_NODE_ROWNUM_MASK | QTC_NODE_SUBQUERY_MASK );
            sCondition = ( QTC_NODE_ROWNUM_ABSENT | QTC_NODE_SUBQUERY_ABSENT );
            if( ( qtc::dependencyContains( &aSFWGH->depInfo,
                                           &sNode->depInfo ) == ID_TRUE ) &&
                ( ( sNode->lflag & sMask ) == sCondition ) )
            {
                // Correlation�̰ų� ROWNUM �Ǵ� subquery�� �����ϴ� predicate�� �����Ѵ�.

                *aPredicate = (qtcNode *)sNode->node.next;
                sNode->node.next = NULL;

                IDE_TEST( concatPredicate( aStatement,
                                           aSFWGH->where,
                                           sNode,
                                           &sConcatPred )
                          != IDE_SUCCESS );
                aSFWGH->where = sConcatPred;
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

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoUnnesting::addPartition( qcStatement    * aStatement,
                            qtcNode        * aExpression,
                            qtcOverColumn ** aPartitions )
{
/***********************************************************************
 *
 * Description :
 *     PARTITION BY���� expression�� �߰��Ѵ�.
 *     ���� ������ expression�� �̹� �����ϸ� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcOverColumn * sPartition;
    idBool          sIsEquivalent = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::addPartition::__FT__" );

    // �̹� ������ expression�� partition�� �����ϴ��� ã�´�.
    for( sPartition = *aPartitions;
         sPartition != NULL;
         sPartition = sPartition->next )

    {
        IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                               sPartition->node,
                                               aExpression,
                                               &sIsEquivalent )
                  != IDE_SUCCESS );

        if ( sIsEquivalent == ID_TRUE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    // BUG-37781
    if ( sIsEquivalent == ID_FALSE )
    {
        // �������� �ʴ� ��� �߰��Ѵ�.
        IDE_TEST( QC_QMP_MEM( aStatement )->cralloc( ID_SIZEOF( qtcOverColumn ),
                                                     (void **)&sPartition )
                  != IDE_SUCCESS );

        sPartition->node = aExpression;
        sPartition->next = *aPartitions;

        *aPartitions = sPartition;
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
qmoUnnesting::genPartitions( qcStatement    * aStatement,
                             qmsSFWGH       * aSFWGH,
                             qtcNode        * aPredicate,
                             qtcOverColumn ** aPartitions )
{
/***********************************************************************
 *
 * Description :
 *     WHERE���� ���� �� correlation predicate�� ã�� window function��
 *     PARTITION BY���� ������ expression���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsParseTree  * sParseTree;
    qmsQuerySet   * sQuerySet;
    qtcNode       * sArg;
    qcDepInfo       sDepInfo;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::genPartitions::__FT__" );

    sParseTree = (qmsParseTree *)aStatement->myPlan->parseTree;
    sQuerySet  = sParseTree->querySet;

    if( aPredicate != NULL )
    {
        qtc::dependencyAnd( &sQuerySet->depInfo, &aPredicate->depInfo, &sDepInfo );

        if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
        {
            if( ( aPredicate->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                IDE_FT_ERROR( ( aPredicate->node.lflag & MTC_NODE_OPERATOR_MASK )
                              == MTC_NODE_OPERATOR_AND );

                for( sArg = (qtcNode *)aPredicate->node.arguments;
                     sArg != NULL;
                     sArg = (qtcNode *)sArg->node.next )
                {
                    IDE_TEST( genPartitions( aStatement,
                                             aSFWGH,
                                             sArg,
                                             aPartitions )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_FT_ERROR( ( aPredicate->node.lflag & MTC_NODE_COMPARISON_MASK )
                              == MTC_NODE_COMPARISON_TRUE );

                qtc::dependencyAnd( &aPredicate->depInfo,
                                    &aSFWGH->outerDepInfo,
                                    &sDepInfo );

                if( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
                {
                    // Correlation predicate�� ���
                    for( sArg = (qtcNode *)aPredicate->node.arguments;
                         sArg != NULL;
                         sArg = (qtcNode *)sArg->node.next )
                    {
                        if( qtc::dependencyContains( &aSFWGH->depInfo,
                                                     &sArg->depInfo )
                            == ID_TRUE )
                        {
                            IDE_TEST( addPartition( aStatement,
                                                    sArg,
                                                    aPartitions )
                                      != IDE_SUCCESS );
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
                    // Nothing to do.
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

idBool qmoUnnesting::existViewTarget( qtcNode   * aNode,
                                      qcDepInfo * aDepInfo )
{
/***********************************************************************
 *
 * Description :
 *     Subquery�� ���Ե� predicate �߿���
 *     unnest �� ������ view �� target ���� �� column �� �ִ��� �˻��Ѵ�.
 *
 * Implementation :
 *     Corelation predicate ���� column �̳� pass node,
 *     aggragation node �� �ִ��� �˻��Ѵ�.
 *
 *     removeCorrPredicate �Լ����� corelation predicate �� �з� ������ �ٲ�ų�,
 *     genViewSelect �Լ����� target �� ���� ������ �ٲ�� �� �Լ��� �ٲ��� �Ѵ�.
 *
 ***********************************************************************/

    qtcNode   * sArg;
    qcDepInfo   sDepInfo;
    idBool      sExist = ID_FALSE;

    if ( aNode != NULL )
    {
        qtc::dependencyAnd( &aNode->depInfo,
                            aDepInfo,
                            &sDepInfo );

        if ( qtc::haveDependencies( &sDepInfo ) == ID_TRUE )
        {
            // Corelation predicate
            if ( ( aNode->node.module == &qtc::columnModule ) ||
                 ( aNode->node.module == &qtc::passModule ) ||
                 ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_AGGREGATION ) )
            {
                // genViewSelect �Լ��� ������ ������ �����ϸ� 
                // target ���� ���õ� ���̴�.
                sExist = ID_TRUE;
            }
            else
            {
                if ( aNode->node.module == &qtc::subqueryModule )
                {
                    // Subquery�� Target���� ������� �ʴ´�.
                    // Nothing to do.
                }
                else
                {
                    // Arguments �� ��������� �˻��Ѵ�.
                    for ( sArg = (qtcNode *)aNode->node.arguments;
                          sArg != NULL;
                          sArg = (qtcNode *)sArg->node.next )
                    {
                        if ( existViewTarget( sArg, aDepInfo ) == ID_TRUE )
                        {
                            sExist = ID_TRUE;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                }
            }
        }
        else
        {
            // Dependency �� ���� �ʴ� node
            // Nothing to do.
        }
    }
    else
    {
        // Node �� null �� ���
        // Nothing to do.
    }

    return sExist;
}


IDE_RC qmoUnnesting::setAggrNode( qcStatement * aSQStatement,
                                  qmsSFWGH    * aSQSFWGH,
                                  qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     BUG-40753 aggsDepth1 ����
 *
 * Implementation :
 *     aggr Node �� ã�Ƽ� aggsDepth1 �� �߰��Ѵ�.
 *     subQuery �� ���δ� ã�� �ʴ´�.
 *     �Ʒ��� ���� ��Ȳ���� sum ��带 ���縦 �߱⶧���� �߰��� �ȴ�.
 *         ex) i1 in ( select sum( c1 ) from ...
 *     �Ʒ��� ���� ���� �߰��� �ȵȴ�.
 *         ex) i1 in ( select sum( c1 )+1 from ...
 ***********************************************************************/

    idBool        sIsAdd = ID_TRUE;
    qtcNode     * sArg;
    qmsAggNode  * sAggNode;

    IDU_FIT_POINT_FATAL( "qmoUnnesting::setAggrNode::__FT__" );

    if ( QTC_IS_AGGREGATE( aNode ) == ID_TRUE )
    {
        for( sAggNode = aSQSFWGH->aggsDepth1;
             sAggNode != NULL;
             sAggNode = sAggNode->next )
        {
            if ( sAggNode->aggr == aNode )
            {
                sIsAdd = ID_FALSE;
                break;
            }
            else
            {
                // nothing to do.
            }
        }

        if ( sIsAdd == ID_TRUE )
        {
            IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aSQStatement ),
                                    qmsAggNode,
                                    (void**)&sAggNode )
                      != IDE_SUCCESS );

            sAggNode->aggr = aNode;
            sAggNode->next = aSQSFWGH->aggsDepth1;

            aSQSFWGH->aggsDepth1 = sAggNode;
        }
        else
        {
            // nothing to do.
        }
    }
    else
    {
        for( sArg = (qtcNode *)aNode->node.arguments;
             sArg != NULL;
             sArg = (qtcNode *)sArg->node.next )
        {
            if( QTC_IS_SUBQUERY( sArg ) == ID_FALSE )
            {
                IDE_TEST( setAggrNode( aSQStatement,
                                       aSQSFWGH,
                                       sArg )
                          != IDE_SUCCESS );
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

void qmoUnnesting::delAggrNode( qmsSFWGH    * aSQSFWGH,
                                qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description :
 *     BUG-45591 aggsDepth1 ����
 *               ���ʿ����� aggsDepth1 �� �����.
 *
 * Implementation :
 *     aggr Node �� ã�Ƽ� aggsDepth1 �� �����Ѵ�.
 *     �Ʒ��� ���� ��Ȳ���� sum ��带 ���縦 �߱⶧���� ���� ��尡 �����ȴ�.
 *         ex) i1 in ( select sum( c1 ) from ...
 *     �Ʒ��� ���� ���� ������ �ȵȴ�.
 *         ex) i1 in ( select sum( c1 )+1 from ...
 ***********************************************************************/

    qmsAggNode  * sAggNode;
    qmsAggNode ** sPrev;

    if ( QTC_IS_AGGREGATE( aNode ) == ID_TRUE )
    {
        for( sPrev    = &aSQSFWGH->aggsDepth1,
             sAggNode = aSQSFWGH->aggsDepth1;
             sAggNode != NULL;
             sAggNode = sAggNode->next )
        {
            if ( sAggNode->aggr == aNode )
            {
                (*sPrev) = sAggNode->next;
            }
            else
            {
                sPrev = &sAggNode->next;
            }
        }
    }
    else
    {
        // Nothing to do.
    }
}

idBool qmoUnnesting::existOuterSubQueryArgument( qtcNode   * aNode,
                                                 qcDepInfo * aInnerDepInfo )
{
/*****************************************************************************
 *
 * Description : BUG-41564
 *               Subquery Argument�� ���� Predicate�� ������ ��,
 *               �� Subquery Argument�� ���� Unnesting ���� Subquery �ܿ�
 *               Outer Query Block�� �����ϴ��� �˻��Ѵ�.
 *               �׷��ٸ�, Unnesting�� �� �� ����.
 *
 * (e.g.) SELECT T1 FROM T1 
 *        WHERE  T1.I1 < ( SELECT SUM(I1) FROM T1 A
 *                         WHERE  A.I1 < ( SELECT SUM(B.I1) FROM T2 B 
 *                                         WHERE B.I2 = T1.I1 ) );
 * 
 *    ù ��° Subquery�� Correlated Predicate ( A.I1 < (..) )�� ������ �ְ�
 *    GROUP BY���� SUM(I1)�̹Ƿ� Single Row Subquery �̴�.
 *    �׷���, Predicate ���ο� �ִ� �� ��° Subquery�� Main Query�� T1�� �����Ѵ�.
 *
 *    Inner Join���� ��ȯ�Ǿ� T1�� RIGHT�� ��ġ�ϰ� �Ǹ�,
 *    �� ��° Subquery�� ����� ���� T1�� ����� �ߺ��Ǿ� ��ȯ�ȴ�.
 *
 *
 *  Note : Lateral View�� ������ �� Unnesting ��Ű�� �ʴ� �Ͱ� ���� �ƶ��̴�.
 *
 *****************************************************************************/

    qtcNode   * sArg              = NULL;
    idBool      sExistCorrSubQArg = ID_FALSE;

    if ( QTC_HAVE_SUBQUERY( aNode ) == ID_TRUE )
    {
        if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
             == MTC_NODE_LOGICAL_CONDITION_TRUE ) 
        {
            for ( sArg = (qtcNode *)aNode->node.arguments;
                  sArg != NULL;
                  sArg = (qtcNode *)sArg->node.next )
            {
                if ( existOuterSubQueryArgument( sArg, aInnerDepInfo ) == ID_TRUE )
                {
                    // �ϳ��� �����ϸ�, Ž���� �����Ѵ�.
                    sExistCorrSubQArg = ID_TRUE;
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
            for ( sArg = (qtcNode *)aNode->node.arguments;
                  sArg != NULL;
                  sArg = (qtcNode *)sArg->node.next )
            {
                if ( isOuterRefSubquery( sArg, aInnerDepInfo ) == ID_TRUE )
                {
                    // �ϳ��� �����ϸ�, Ž���� �����Ѵ�.
                    sExistCorrSubQArg = ID_TRUE;
                    break;
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

    return sExistCorrSubQArg;
}

idBool qmoUnnesting::isOuterRefSubquery( qtcNode   * aArg,
                                         qcDepInfo * aInnerDepInfo )
{
/*****************************************************************************
 *
 * Description : BUG-41564
 *
 *    Subquery Node�� �־��� Dependency ���� �����ϴ��� Ȯ���Ѵ�.
 *    �̷� ��쿡�� TRUE, �������� FALSE�� ��ȯ�Ѵ�.
 *
 *    Predicate�� Subquery Argument, Target Subquery�� ���� ȣ���Ѵ�.
 *
 *****************************************************************************/

    idBool sOuterSubQArg = ID_FALSE;

    if ( aArg != NULL )
    {
        if ( QTC_IS_SUBQUERY( aArg ) == ID_TRUE )
        {
            if ( qtc::dependencyContains( aInnerDepInfo, &aArg->depInfo ) == ID_FALSE )
            {
                sOuterSubQArg = ID_TRUE;
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

    return sOuterSubQArg;
}

idBool
qmoUnnesting::findCountAggr4Target( qtcNode  * aTarget )
{
/*****************************************************************************
 *
 * Description : BUG-45238
 *
 *    target column���� count aggregation�� ã�´�. 
 *
 *****************************************************************************/

    qtcNode   * sArg    = NULL;
    idBool      sFind   = ID_FALSE;

    /* BUG-45316 CountKeep Not unnesting */
    if ( ( aTarget->node.module == &mtfCount ) ||
         ( aTarget->node.module == &mtfCountKeep ) )
    {
        sFind = ID_TRUE;
    }
    else
    {
        if ( QTC_HAVE_AGGREGATE( aTarget ) == ID_TRUE )
        {
            for ( sArg = (qtcNode *)aTarget->node.arguments;
                  sArg != NULL;
                  sArg = (qtcNode *)sArg->node.next )
            {
                if ( findCountAggr4Target( sArg ) == ID_TRUE )
                {
                    // �ϳ��� �����ϸ�, Ž���� �����Ѵ�.
                    sFind = ID_TRUE;
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
            // Nothing to do.
        }
    }

    return sFind;
}

void qmoUnnesting::isAggrExistTransformable( qtcNode   * aNode,
                                             qcDepInfo * aOuterDep,
                                             idBool    * aIsTrue )
{
    if ( *aIsTrue == ID_TRUE )
    {
        if ( ( aNode->node.lflag & MTC_NODE_COMPARISON_MASK )
             == MTC_NODE_COMPARISON_TRUE )
        {
            if ( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                  != MTC_NODE_OPERATOR_EQUAL )
            {
                if ( qtc::dependencyContains( &aNode->depInfo, aOuterDep )
                     == ID_TRUE )
                {
                    *aIsTrue = ID_FALSE;
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
            /* Nothing to do */
        }

        if ( aNode->node.arguments != NULL )
        {
            isAggrExistTransformable( ( qtcNode * )aNode->node.arguments, 
                                      aOuterDep,
                                      aIsTrue );
        }
        else
        {
            /* Nothing to do */
        }

        if ( aNode->node.next != NULL )
        {
            isAggrExistTransformable( ( qtcNode * )aNode->node.next,
                                      aOuterDep,
                                      aIsTrue );
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

