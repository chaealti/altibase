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
 
/******************************************************************************
 * $Id$
 *
 * Description : ORDER BY Elimination Transformation
 *
 * ��� ���� :
 *
 *****************************************************************************/

#include <ide.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmoOBYETransform.h>

extern mtfModule mtfCount;

IDE_RC qmoOBYETransform::doTransform( qcStatement  * aStatement,
                                      qmsParseTree * aParseTree )
{
/***********************************************************************
 *
 * Description : BUG-41183 Inline view �� ���ʿ��� ORDER BY ����
 *               BUG-48941 where/having ���� �������� ������Ŷ�� �ζ��κ信 ORDER BY ����
 * Implementation :
 *
 *       ���� ������ ������ ��� inline view �� order by ���� �����Ѵ�.
 *
 *       Mode1 BUG-41183 
 *        - SELECT count(*) ������ ����
 *        - FROM ���� ��� inline view �� ����
 *        - LIMIT ���� ���� ���
 *       Mode2 BUG-48941 
 *        - WHERE/HAVING���� SUBQUERY�� ����
 *        - SUBQUERY�� LIMIT/TOP/ROWNUM/LEVEL�� ����
 *        - SUBQUERY�� inline view�� ���� ( create view / recursive with view ����)
 *        - inline view �ȿ� �Ʒ��� ������ ��� ���� ���
 *            Set OP ( ������ ������ recursive with�� �ȵ�)
 *            group by/having ��
 *            LIMIT/TOP��
 *            ROWNUM/LEVEL �ǻ��÷�
 *            aggregation/nested aggregation/window function
 *
 ***********************************************************************/
    UInt   sOBYEproperty;

    sOBYEproperty = QCU_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE;

    // environment�� ���
    qcgPlan::registerPlanProperty(
        aStatement,
        PLAN_PROPERTY_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE );

    if ( sOBYEproperty > 0)
    {
        if ( (sOBYEproperty & QCU_OBYE_MASK_MODE1) == QCU_OBYE_MASK_MODE1 )
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_OBYE_1_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_OBYE_1_TRUE;
        }
        else
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_OBYE_1_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_OBYE_1_FALSE;
        }

        if ( (sOBYEproperty & QCU_OBYE_MASK_MODE2)
             == QCU_OBYE_MASK_MODE2 )
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_OBYE_2_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_OBYE_2_TRUE;
        }
        else
        {
            QC_SHARED_TMPLATE(aStatement)->flag &= ~QC_TMP_OBYE_2_MASK;
            QC_SHARED_TMPLATE(aStatement)->flag |= QC_TMP_OBYE_2_FALSE;
        }

        IDE_TEST( doTransformInternal( aStatement,
                                       aParseTree->querySet,
                                       aParseTree->limit,
                                       ID_FALSE )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOBYETransform::doTransformInternal( qcStatement * aStatement,
                                              qmsQuerySet * aQuerySet,
                                              qmsLimit    * aLimit,
                                              idBool        aIsSubQPred )
{
    qmsFrom      * sFrom         = NULL;
    idBool         sIsTransMode2 = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOBYETransform::doTransform::__FT__" );

    //------------------------------------------
    // ���� �˻�
    //------------------------------------------
    if ( aQuerySet->setOp == QMS_NONE )
    {
        if ( ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_OBYE_2_MASK )
               == QC_TMP_OBYE_2_TRUE ) &&
             ( aIsSubQPred == ID_TRUE ) )
        {
            /* BUG-48941
             *  mode2�� �� parent query block�� ���� Ȯ��
             *    line view �� ��� ������Ͽ� �Ʒ� ������ �ִ°��
             *    OBYE�� �� ����. */
            if ( ( aLimit != NULL ) ||
                 ( aQuerySet->SFWGH->rownum != NULL ) ||
                 ( aQuerySet->SFWGH->level  != NULL ) ||
                 ( aQuerySet->SFWGH->top    != NULL ) )
            {
                sIsTransMode2 = ID_FALSE;
            }
            else
            {
                sIsTransMode2 = ID_TRUE; 
            }
        }

        // from
        for ( sFrom = aQuerySet->SFWGH->from;
              sFrom != NULL;
              sFrom = sFrom->next )
        {
            IDE_TEST( doTransform4FromTree( aStatement,
                                            aQuerySet,
                                            sFrom,
                                            sIsTransMode2 )
                      != IDE_SUCCESS );
        }

        // where
        IDE_TEST( doTransform4Predicate( aStatement,
                                         aQuerySet->SFWGH->where ) 
                  != IDE_SUCCESS ); 

        // having
        IDE_TEST( doTransform4Predicate( aStatement,
                                         aQuerySet->SFWGH->having ) 
                  != IDE_SUCCESS ); 
    }
    else
    {
        IDE_TEST( doTransformInternal( aStatement,
                                       aQuerySet->left,
                                       aLimit,
                                       aIsSubQPred )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformInternal( aStatement,
                                       aQuerySet->right,
                                       aLimit,
                                       aIsSubQPred )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmoOBYETransform::doTransform4FromTree( qcStatement * aStatement,
                                               qmsQuerySet * aQuerySet, 
                                               qmsFrom     * aFrom,
                                               idBool        aIsTransMode2 )
{
    qmsParseTree * sParseTree    = NULL;
    qmsQuerySet  * sQuerySet     = NULL;

    if ( aFrom->joinType == QMS_NO_JOIN )
    {
        if ( aFrom->tableRef != NULL )
        {
            // recursive with view ����
            if ( ( aFrom->tableRef->view != NULL ) &&
                 ( aFrom->tableRef->tableInfo->tableType == QCM_USER_TABLE ) )
            {
                // inline view�� parseTree/QuerySet
                sParseTree = (qmsParseTree*)(aFrom->tableRef->view->myPlan->parseTree);
                sQuerySet = sParseTree->querySet;

                IDE_TEST( doTransformInternal( aStatement,
                                               sQuerySet,
                                               sParseTree->limit,
                                               aIsTransMode2 )
                          != IDE_SUCCESS );

                // mode1
                if ( ( sParseTree->orderBy != NULL ) &&
                     ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_OBYE_1_MASK )
                       == QC_TMP_OBYE_1_TRUE ) ) 
                {
                    if ( ( aQuerySet->target->targetColumn->node.module == &mtfCount ) &&
                         ( aQuerySet->target->next == NULL ) &&
                         ( sParseTree->limit == NULL ) &&
                         ( sQuerySet->setOp == QMS_NONE ) )
                    {
                        sParseTree->orderBy = NULL;
                    }
                }

                if ( ( sParseTree->orderBy != NULL ) && 
                     ( aIsTransMode2 == ID_TRUE ) )
                {
                    // mode2�� �� Inline view�� ���� Ȯ��
                    // inlin view�� With view�� ��� ����
                    if ( ( ( aFrom->tableRef->flag & QMS_TABLE_REF_WITH_VIEW_MASK )
                           == QMS_TABLE_REF_WITH_VIEW_FALSE ) &&
                         ( canTranMode2ForInlineView( sParseTree ) == ID_TRUE ) )
                    {
                        sParseTree->orderBy = NULL;
                    }
                }
            }
        }
    }
    else
    {
        IDE_TEST( doTransform4FromTree( aStatement,
                                        aQuerySet,
                                        aFrom->left,
                                        aIsTransMode2)
                  != IDE_SUCCESS );

        IDE_TEST( doTransform4FromTree( aStatement,
                                        aQuerySet,
                                        aFrom->right,
                                        aIsTransMode2 )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoOBYETransform::doTransform4Predicate( qcStatement * aStatement,
                                            qtcNode     * aNode )
{
    qmsParseTree * sSubParseTree = NULL;
    qmsQuerySet  * sSubQuerySet  = NULL;
    qtcNode      * sNode;

    if ( aNode != NULL )
    {
        if ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK ) == QTC_NODE_SUBQUERY_EXIST )
        {
            if ( QTC_IS_SUBQUERY(aNode) == ID_TRUE )
            {
                sSubParseTree = (qmsParseTree*)(aNode->subquery->myPlan->parseTree);
                sSubQuerySet = sSubParseTree->querySet;

                if ( sSubQuerySet->setOp == QMS_NONE )
                { 
                    IDE_TEST( doTransformInternal( aStatement,
                                                   sSubQuerySet,
                                                   sSubParseTree->limit,
                                                   ID_TRUE )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( doTransformInternal( aStatement,
                                                   sSubQuerySet->left,
                                                   sSubParseTree->limit,
                                                   ID_TRUE )
                              != IDE_SUCCESS );

                    IDE_TEST( doTransformInternal( aStatement,
                                                   sSubQuerySet->right,
                                                   sSubParseTree->limit,
                                                   ID_TRUE )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                for( sNode = (qtcNode *)aNode->node.arguments;
                     sNode != NULL;
                     sNode = (qtcNode *)sNode->node.next )
                {
                    IDE_TEST( doTransform4Predicate( aStatement,
                                                     sNode )
                              != IDE_SUCCESS );
                }
            }
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmoOBYETransform::canTranMode2ForInlineView( qmsParseTree * aParseTree )
{
    qmsSFWGH       * sSFWGH;
    qmsTarget      * sTarget;

    if ( aParseTree->querySet->setOp != QMS_NONE )
    {
        IDE_CONT( INVALID_FORM );
    }

    sSFWGH = aParseTree->querySet->SFWGH;

    if ( ( aParseTree->limit  != NULL ) ||
         ( sSFWGH->rownum     != NULL ) ||
         ( sSFWGH->level      != NULL ) ||
         ( sSFWGH->top        != NULL ) ||
         ( sSFWGH->group      != NULL ) ||
         ( sSFWGH->having     != NULL ) ||
         ( sSFWGH->aggsDepth1 != NULL ) ||
         ( sSFWGH->aggsDepth2 != NULL ) )
    {
        IDE_CONT( INVALID_FORM );
    }

    // SELECT�� �˻�
    for ( sTarget = sSFWGH->target;
          sTarget != NULL;
          sTarget = sTarget->next )
    {
        if ( ( sTarget->targetColumn->lflag & QTC_NODE_ANAL_FUNC_MASK )
             == QTC_NODE_ANAL_FUNC_EXIST )
        {
            // Window function�� ����� ���
            IDE_CONT( INVALID_FORM );
        }
    }

    return ID_TRUE;

    IDE_EXCEPTION_CONT( INVALID_FORM );

    return ID_FALSE;
}
