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
 * $Id: qmoSubquery.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Subquery Manager
 *
 *     ���� ó�� �߿� �����ϴ� Subquery�� ���� ����ȭ�� �����Ѵ�.
 *     Subquery�� ���Ͽ� ����ȭ�� ���� Graph ����,
 *     Graph�� �̿��� Plan Tree ������ ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qtc.h>
#include <qtcCache.h>
#include <qmsParseTree.h>
#include <qmgDef.h>
#include <qmo.h>
#include <qmoSubquery.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmoCheckViewColumnRef.h>
#include <qmvQTC.h>

extern mtfModule mtfExists;
extern mtfModule mtfNotExists;
extern mtfModule mtfUnique;
extern mtfModule mtfNotUnique;
extern mtfModule mtfEqualAny;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfList;
extern mtfModule mtfAnd;
extern mtfModule mtfEqual;
extern mtfModule mtfEqualAll;
extern mtfModule mtfNotEqualAny;
extern mtfModule mtfIsNull;
extern mtfModule mtfIsNotNull;
extern mtfModule mtfNotEqual;
extern mtfModule mtfLike;
extern mtfModule mtfNotLike;
extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;
extern mtfModule mtfDecrypt;


IDE_RC
qmoSubquery::makePlan( qcStatement  * aStatement,
                       UShort         aTupleID,
                       qtcNode      * aNode )
{
/***********************************************************************
 *
 * Description : subquery�� ���� plan����
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode     * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::makePlan::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // subquery�� ���� plan tree ����
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_SUBQUERY )
    {
        // subquery�� ���� plan �������� �Ǵ���,
        // plan tree�� �������� �ʾ�����, plan tree ����.
        if( aNode->subquery->myPlan->plan == NULL )
        {
            aNode->subquery->myPlan->graph->myQuerySet->parentTupleID = aTupleID;

            /*
             * PROJ-2402
             * subquery �� ��� parallel ���� ���� �ʴ´�.
             */
            aNode->subquery->myPlan->graph->flag &= ~QMG_PARALLEL_IMPOSSIBLE_MASK;
            aNode->subquery->myPlan->graph->flag |= QMG_PARALLEL_IMPOSSIBLE_TRUE;

            IDE_TEST( aNode->subquery->myPlan->graph->makePlan( aNode->subquery,
                                                                NULL,
                                                                aNode->subquery->myPlan->graph )
                      != IDE_SUCCESS );

            aNode->subquery->myPlan->plan =
                aNode->subquery->myPlan->graph->myPlan;
        }
        else
        {
            // Nothing To Do
        }

        /* PROJ-2448 Subquery caching */
        IDE_TEST( qtcCache::preprocessSubqueryCache( aStatement, aNode )
                  != IDE_SUCCESS );
    }
    else
    {
        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( makePlan( aStatement, aTupleID, sNode ) != IDE_SUCCESS );

            sNode = (qtcNode *)(sNode->node.next);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::optimize( qcStatement  * aStatement,
                       qtcNode      * aNode,
                       idBool         aTryKeyRange)
{
/***********************************************************************
 *
 * Description : Subquery�� ������ qtcNode�� ���� ����ȭ ����
 *
 *   1. predicate �� subquery :
 *     (1) �Ϲ� query�� ����ȭ�� ������ ����ȭ ���� ����
 *     (2) predicate subquery�� ���� optimization tip�� �߰������� ���� ����.
 *     ��) select * from t1 where i1 > (select sum(i2) from t2);
 *                                ------------------------------
 *         update t1 set i1=1 where i2 in (select i1 from t2);
 *                                 ---------------------------
 *         delete from t1 where i1 in (select i1 from t2);
 *                             ---------------------------
 *         select * from t1 where ( subquery ) = ( subquery );
 *                                ----------------------------
 *
 *   2. expression �� subquery :
 *      ��) select (select sum(i1) from t2 ) from t1;
 *                 -------------------------
 *      ��) select * from t1 where i1 > 1 + (select sum(i1) from t2);
 *                                 --------------------------------
 *         �̿� ���� ����, constantSubquery()�� ó���Ǿ�� �Ѵ�.
 *
 * Implementation :
 *
 *     0. Predicate/Expression���� �Ǵ�
 *        (1) �񱳿������� ���
 *            - subquery�� predicate �ٷ� ������ �����ϴ� ���, predicate��
 *            - �׷��� ���� ���,  expression������ ó��.
 *        (2) �� �����ڰ� �ƴ� ���
 *            - Expression��
 *
 *     1. predicate �� subquery�̸�, �Ʒ��� ������ ����.
 *
 *        (1). graph ���� ��, predicate�� subquery ����ȭ ����
 *            1) no calculate (not)EXISTS/(not)UNIQUE subquery
 *            2) transform NJ
 *
 *        (2). graph ����
 *
 *        (3). graph ���� ��, predicate�� subquery ����ȭ ����
 *            1) store and search
 *            2) IN���� subquery keyRange
 *            3) subquery keyRange
 *
 *     2. expression �� subquery�̸�,
 *        (1) graph ����
 *        (2) Tip ���� : constantSubquery()�� ȣ���Ѵ�.
 *
 ***********************************************************************/

    idBool     sIsPredicate;
    qtcNode  * sNode = NULL;
    qtcNode  * sLeftNode = NULL;
    qtcNode  * sRightNode = NULL;
    
    qmsQuerySet * sQuerySet;

    IDU_FIT_POINT_FATAL( "qmoSubquery::optimize::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    // aNode : (1) where�� ó���� ȣ�� : �ֻ��� ��尡 �񱳿�����
    //             selection graph�� myPredicate
    //             CNF�� constantPredicate
    //         (2) projection�� target�� ó��
    //         (3) grouping graph�� aggr, group, having

    //--------------------------------------
    // subquery�� predicate/expression �Ǵ�
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_COMPARISON_MASK )
        == MTC_NODE_COMPARISON_TRUE )
    {
        // where���� predicate �Ǵ�
        // grouping graph�� having �� ���� ó��

        // where���� ������ ��� subquery�� ���� �����Ƿ�,
        // �񱳿����� ���� �ΰ� ��� ��� subquery�� ���� ó���� �ؾ� ��.

        // �񱳿����ڰ�
        // (1) IS NULL, IS NOT NULL, (NOT)EXISTS, (NOT)UNIQUE �� ���,
        //     �񱳿����� ���� ���� �ϳ�
        // (2) BETWEEN, NOT BETWEEN �� ���,
        //     �񱳿����� ���� ���� ����
        //     LIKE �� ���, �񱳿����� ���� ��尡 �ΰ� �Ǵ� ����
        // (3) (1),(2)������ �񱳿����� ���� ���� �ΰ�
        // �̹Ƿ�, �̿� ���� ��� ����� �̷������ �Ѵ�.

        for( sNode = (qtcNode *)aNode->node.arguments,
                 sIsPredicate = ID_TRUE;
             sNode != NULL;
             sNode = (qtcNode *)(sNode->node.next) )
        {
            if( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
            {
                if( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                    == MTC_NODE_OPERATOR_SUBQUERY )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsPredicate = ID_FALSE;
                    break;
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
        // projection�� target
        // grouping graph�� aggr, group�� ���� ó��

        sIsPredicate = ID_FALSE;
    }

    //--------------------------------------
    // subquery ó��
    //--------------------------------------

    if( sIsPredicate == ID_TRUE )
    {
        // predicate�� subquery �� ���

        sLeftNode = (qtcNode *)(aNode->node.arguments);
        sRightNode = (qtcNode *)(aNode->node.arguments->next);

        if( ( sLeftNode->lflag & QTC_NODE_SUBQUERY_MASK )
            == QTC_NODE_SUBQUERY_EXIST )
        {
            if( ( sLeftNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_SUBQUERY )
            {
                // �񱳿����� ���� ��尡 subquery�� ����,
                // predicate�� subquery�̴�.

                if ( sLeftNode->subquery->myPlan->graph == NULL )
                {
                    // BUG-23362
                    // subquery�� ����ȭ ���� ���� ���
                    // ��, ù optimize�� ��쿡�� ����
                    IDE_TEST( optimizePredicate( aStatement,
                                                 aNode,
                                                 sRightNode,
                                                 sLeftNode,
                                                 aTryKeyRange )
                              != IDE_SUCCESS );
                }
                else
                {
                    // �̹� ����ȭ ������
                }
            }
            else
            {
                // �񱳿����� ���� ��尡 subquery�� �ƴ� ����,
                // expression�� subquery�� ó����.
            }
        }
        else
        {
            // Nothing To Do
        }

        for( sNode = sRightNode;
             sNode != NULL;
             sNode = (qtcNode *)(sNode->node.next) )
        {
            if( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
            {
                if( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                    == MTC_NODE_OPERATOR_SUBQUERY )
                {
                    // �񱳿����� ���� ��尡 subquery�� ����,
                    // predicate�� subquery�̴�.

                    if ( sNode->subquery->myPlan->graph == NULL )
                    {
                        // BUG-23362
                        // subquery�� ����ȭ ���� ���� ���
                        // ��, ù optimize�� ��쿡�� ����
                        IDE_TEST( optimizePredicate( aStatement,
                                                     aNode,
                                                     sLeftNode,
                                                     sNode,
                                                     aTryKeyRange )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // �̹� ����ȭ ������
                    }
                }
                else
                {
                    // �񱳿����� ���� ��尡 subquery�� �ƴ� ����,
                    // expression�� subquery�� ó����.
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
        // expression �� subquery�� ���,

        // projection�� target
        // grouping graph�� aggr, group
        // where���� predicate �� �񱳿����� ���� ��尡 subquery�� �ƴ� ���,
        //  (��: i1 = 1 + subquery)
        IDE_TEST( optimizeExpr4Select( aStatement,
                                       aNode ) != IDE_SUCCESS );

    }

    // BUG-45212 ���������� �ܺ� ���� �÷��� �����϶� ����� Ʋ���ϴ�.
    if ( QTC_IS_SUBQUERY(aNode) == ID_TRUE )
    {
        sQuerySet = ((qmsParseTree*)aNode->subquery->myPlan->parseTree)->querySet;

        if ( sQuerySet->setOp == QMS_NONE )
        {
            IDE_TEST( qmvQTC::extendOuterColumns( aNode->subquery,
                                                  NULL,
                                                  sQuerySet->SFWGH )
                      != IDE_SUCCESS );
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::optimizeExprMakeGraph( qcStatement * aStatement,
                                    UInt          aTupleID,
                                    qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : expression�� subquery�� ���� ó��
 *
 *  ��) INSERT...VALUES(...(subquery)...)
 *      : INSERT INTO T1 VALUES ( (SELECT SUM(I2) FROM T2) );
 *      UPDATE...SET column = (subquery)
 *      : UPDATE SET I1=(SELECT SUM(I1) FROM T2) WHERE I1>1;
 *      UPDATE...SET (column list) = (subquery)
 *      : UPDATE SET (I1,I2)=(SELECT I1,I2 FROM T2 WHERE I1=1);
 *
 * Implementation :
 *
 *     1. graph ����
 *     2. constant subquery ����ȭ ���� (store and search)
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::optimizeExprMakeGraph::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // subquery node�� ã�´�.
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_SUBQUERY )
    {
        if ( aNode->subquery->myPlan->graph == NULL )
        {
            // BUG-23362
            // subquery�� ����ȭ ���� ���� ���
            // ��, ù optimize�� ��쿡�� ����
            IDE_TEST( makeGraph( aNode->subquery ) != IDE_SUCCESS );

            IDE_TEST( constantSubquery( aStatement,
                                        aNode ) != IDE_SUCCESS );
        }
        else
        {
            // nothing to do 
        }
    }
    else
    {
        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( optimizeExprMakeGraph( aStatement,
                                             aTupleID,
                                             sNode ) != IDE_SUCCESS );

            sNode = (qtcNode *)(sNode->node.next);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoSubquery::optimizeExprMakePlan( qcStatement * aStatement,
                                   UInt          aTupleID,
                                   qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : expression�� subquery�� ���� ó��
 *              ��� ���������� ���ؼ� optimizeExprMakePlan ���Ŀ� ȣ���ؾ��Ѵ�.
 *              ( BUG-32854 )
 *
 * Implementation :
 *
 *     1. plan ����
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::optimizeExprMakePlan::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // subquery node�� ã�´�.
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_SUBQUERY )
    {
        if ( aNode->subquery->myPlan->plan == NULL )
        {
            // BUG-23362
            // plan�� �������� ���� ��쿡�� ����
            IDE_TEST( makePlan( aStatement,
                                aTupleID,
                                aNode ) != IDE_SUCCESS );
        }
        else
        {
            // nothing to do 
        }
    }
    else
    {
        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( optimizeExprMakePlan( aStatement,
                                            aTupleID,
                                            sNode ) != IDE_SUCCESS );

            sNode = (qtcNode *)(sNode->node.next);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoSubquery::makeGraph( qcStatement  * aSubQStatement )
{
/***********************************************************************
 *
 * Description : subquery�� ���� graph ����
 *
 * Implementation :
 *
 *     qmo::makeGraph()ȣ��
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoSubquery::makeGraph::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aSubQStatement != NULL );

    //------------------------------------------
    // PROJ-1413
    // subquery�� ���� Query Transformation ����
    //------------------------------------------

    IDE_TEST( qmo::doTransform( aSubQStatement ) != IDE_SUCCESS );

    //------------------------------------------
    // PROJ-2469 Optimize View Materialization
    // subquery ������ ���ʿ��� View Column�� ã�Ƽ� flagó��
    //------------------------------------------

    IDE_TEST( qmoCheckViewColumnRef::checkViewColumnRef( aSubQStatement,
                                                         NULL,
                                                         ID_TRUE ) != IDE_SUCCESS );

    //--------------------------------------
    // subquery�� ���� graph ����
    //--------------------------------------

    IDE_TEST( qmo::makeGraph( aSubQStatement ) != IDE_SUCCESS );

    IDE_DASSERT( aSubQStatement->myPlan->graph != NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoSubquery::checkSubqueryType( qcStatement     * aStatement,
                                qtcNode         * aSubqueryNode,
                                qmoSubqueryType * aType )
{
/***********************************************************************
 *
 * Description : Subquery type�� �Ǵ��Ѵ�.
 *
 *     < Subquery Type >
 *      -----------------------------------------------
 *     |         | outerColumn reference | aggregation |
 *      -----------------------------------------------
 *     | Type  A |           X           |      O      |
 *      -----------------------------------------------
 *     | Type  N |           X           |      X      |
 *      -----------------------------------------------
 *     | Type  J |           O           |      X      |
 *      -----------------------------------------------
 *     | Type JA |           O           |      O      |
 *      -----------------------------------------------
 *
 * Implementation :
 *
 *     outer column�� aggregation ���������� �����ؼ�, subquery type�� ��ȯ.
 *
 *     SET���� ���, �Ϲ� query�� �����ϰ� ó���Ǹ�, type N or J�� �����ȴ�.
 *
 *     1. outer column �������� �Ǵ�
 *        dependencies�� �Ǵ�
 *        subqueryNode.dependencies != 0
 *
 *     2. aggregation �������� �Ǵ�
 *        qmsSFWGH���� �����ϰ� �ִ�
 *        (1) aggregation���� (2) group ������ �Ǵ�.
 *
 ***********************************************************************/

    qmsSFWGH        * sSFWGH = NULL;
    qmsQuerySet     * sQuerySet = NULL;
    qmoSubqueryType   sType;
    idBool            sExistOuterColumn;

    IDU_FIT_POINT_FATAL( "qmoSubquery::checkSubqueryType::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aSubqueryNode != NULL );
    IDE_DASSERT( aType != NULL );

    //--------------------------------------
    // subquery type �Ǵ�
    //--------------------------------------

    sQuerySet =
        ((qmsParseTree *)(aSubqueryNode->subquery->myPlan->parseTree))->querySet;

    sSFWGH = sQuerySet->SFWGH;

    if( qtc::dependencyEqual( & aSubqueryNode->depInfo,
                              & qtc::zeroDependencies ) == ID_TRUE )
    {
        if( sSFWGH == NULL )
        {
            sExistOuterColumn = ID_FALSE;
        }
        else
        {
            sExistOuterColumn = sSFWGH->outerHavingCase;
        }
    }
    else
    {
        checkSubqueryDependency( aStatement,
                                 sQuerySet,
                                 aSubqueryNode,
                                 & sExistOuterColumn );
    }

    if ( sExistOuterColumn == ID_FALSE )
    {
        //--------------------------------------
        // outer column�� �������� �ʴ� ���
        //--------------------------------------

        if( sQuerySet->setOp == QMS_NONE )
        {
            // SET���� �ƴ� ���,

            IDE_FT_ASSERT( sSFWGH != NULL );
            
            if( sSFWGH->group == NULL )
            {
                if( sSFWGH->aggsDepth1 == NULL )
                {
                    // aggregation�� �������� ����.
                    sType = QMO_TYPE_N;
                }
                else
                {
                    // aggregation�� ������.
                    sType = QMO_TYPE_A;
                }
            }
            else
            {
                // aggregation�� ������.
                sType = QMO_TYPE_A;
            }
        }
        else
        {
            // SET���� ���,
            sType = QMO_TYPE_N;
        }
    }
    else
    {
        //--------------------------------------
        // outer column�� �����ϴ� ���
        //--------------------------------------

        if( sQuerySet->setOp == QMS_NONE )
        {
            // SET ���� �ƴ� ���,

            if( sSFWGH->group == NULL )
            {
                if( sSFWGH->aggsDepth1 == NULL )
                {
                    // aggregation�� �������� ����.
                    sType = QMO_TYPE_J;
                }
                else
                {
                    // aggregation�� ������.
                    sType = QMO_TYPE_JA;
                }
            }
            else
            {
                // aggregation�� ������.
                sType = QMO_TYPE_JA;
            }
        }
        else
        {
            // SET ���� ���,

            sType = QMO_TYPE_J;
        }
    }

    *aType = sType;

    return IDE_SUCCESS;
}

void qmoSubquery::checkSubqueryDependency( qcStatement * aStatement,
                                           qmsQuerySet * aQuerySet,
                                           qtcNode     * aSubQNode,
                                           idBool      * aExist )
{
/***********************************************************************
 *
 * Description : BUG-36575
 *               Subquery�� parent querySet�� �������� �ִ��� �˻��Ѵ�.
 *
 *   ��1) ���� �������� t3.i1�� parent querySet���� �����ϴ�.
 *        select 
 *            (select count(*) from t1 where i1 in (select i1 from t2 where i1=t3.i1))
 *        from t3;                                                             ^^^^^
 *
 *   ��2) ���� �������� t3.i1�� parent querySet���� �����ϴ�.
 *        select 
 *            (select count(*) from t1 where i1 in
 *                (select i1 from t2 where i1=t3.i1 union select i1 from t2 where i1=t4.i1))
 *        from t3, t4;                        ^^^^^                                  ^^^^^
 *
 *   ��3) ���� �������� t1.i1�� parent querySet�� �������� �ִ�.
 *        select 
 *            (select count(*) from t1 where i1 in (select i1 from t2 where i1=t1.i1))
 *        from t3;                                                             ^^^^^
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsSFWGH  * sSFWGH;
    qcDepInfo   sDepInfo;
    idBool      sExist;

    sExist = ID_FALSE;

    if ( aQuerySet->setOp == QMS_NONE )
    {
        sSFWGH = aQuerySet->SFWGH;

        qtc::dependencyAnd( & aSubQNode->depInfo,
                            & sSFWGH->outerQuery->depInfo,
                            & sDepInfo );
    
        if( qtc::dependencyEqual( & sDepInfo,
                                  & qtc::zeroDependencies ) != ID_TRUE )
        {
            sExist = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        checkSubqueryDependency( aStatement,
                                 aQuerySet->left,
                                 aSubQNode,
                                 & sExist );

        if ( sExist == ID_FALSE )
        {
            checkSubqueryDependency( aStatement,
                                     aQuerySet->right,
                                     aSubQNode,
                                     & sExist );
        }
        else
        {
            // Nothing to do.
        }
    }

    *aExist = sExist;
}

IDE_RC qmoSubquery::optimizeExpr4Select( qcStatement * aStatement,
                                         qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : select���� ���� expression�� subquery ó��
 *
 *      ��) select (select sum(i1) from t2 ) from t1;
 *                 -------------------------
 *      ��) select * from t1 where i1 > 1 + (select sum(i1) from t2);
 *                                 --------------------------------
 *
 *   qmoSubquery::optimizeExpr()
 *        : update, delete���� ���� subquery ó��
 *          graph����->constant subqueryTip����->plan����
 *   qmoSubquery::optimizeExpr4Select()
 *        : select���� expression�� subqueryó��
 *          �� �Լ������� plan�� �������� �ʴ´�.
 *
 * Implementation :
 *
 *     1. graph ����
 *     2. constant subquery ����ȭ ���� (store and search)
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::optimizeExpr4Select::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // subquery node�� ã�´�.
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_SUBQUERY )
    {
        // subquery node�� ã��
        if ( aNode->subquery->myPlan->graph == NULL )
        {
            // BUG-23362
            // subquery�� ����ȭ ���� ���� ���
            // ��, ù optimize�� ��쿡�� ����
            IDE_TEST( makeGraph( aNode->subquery ) != IDE_SUCCESS );
            
            IDE_TEST( constantSubquery( aStatement,
                                        aNode ) != IDE_SUCCESS );
        }
        else
        {
            // �̹� ����ȭ ������
        }
    }
    else
    {
        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( optimizeExpr4Select( aStatement,
                                           sNode )
                      != IDE_SUCCESS );

            sNode = (qtcNode *)(sNode->node.next);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC qmoSubquery::optimizePredicate( qcStatement * aStatement,
                                       qtcNode     * aCompareNode,
                                       qtcNode     * aNode,
                                       qtcNode     * aSubQNode,
                                       idBool        aTryKeyRange )
{
/***********************************************************************
 *
 * Description : predicate�� subquery�� ���� subquery ����ȭ
 *
 * Implementation :
 *
 *     ��) select * from t1 where i1 > (select sum(i2) from t2);
 *                                ------------------------------
 *         update t1 set i1=1 where i2 in (select i1 from t2);
 *                                 ---------------------------
 *         delete from t1 where i1 in (select i1 from t2);
 *                             ---------------------------
 *         select * from t1 where ( subquery ) = ( subquery );
 *                                ----------------------------
 *
 *    1. graph ���� ��, predicate�� subquery ����ȭ ����
 *       1) no calculate (not)EXISTS/(not)UNIQUE subquery
 *       2) transform NJ
 *
 *    2. graph ����
 *
 *    3. graph ���� ��, predicate�� subquery ����ȭ ����
 *       1) store and search
 *       2) IN���� subquery keyRange
 *       3) subquery keyRange
 *
 ***********************************************************************/

    idBool            sColumnNodeIsColumn = ID_TRUE;
    idBool            sIsTransformNJ = ID_FALSE;
    idBool            sIsStoreAndSearch = ID_TRUE;
    idBool            sIsInSubqueryKeyRangePossible;
    SDouble           sColumnCardinality;
    SDouble           sTargetCardinality;
    SDouble           sSubqueryResultCnt;
    idBool            sSubqueryIsSet;
    qmoSubqueryType   sSubQType;
    qmgPROJ         * sPROJ;

    IDU_FIT_POINT_FATAL( "qmoSubquery::optimizePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    if( ( aCompareNode->node.module == & mtfExists )    ||
        ( aCompareNode->node.module == & mtfNotExists ) ||
        ( aCompareNode->node.module == & mtfUnique )    ||
        ( aCompareNode->node.module == & mtfNotUnique ) ||
        ( aCompareNode->node.module == & mtfIsNull )    ||
        ( aCompareNode->node.module == & mtfIsNotNull ) )
    {
        IDE_DASSERT( aNode == NULL ); // subquery node�� �ƴ� ���� node
    }
    else
    {
        IDE_DASSERT( aNode != NULL ); // subquery node�� �ƴ� ���� node
    }
    IDE_DASSERT( aSubQNode != NULL ); // subquery node

    // BUG-23362
    // �̹� ����ȭ�� subquery�� ���� �� ����
    IDE_DASSERT( aSubQNode->subquery->myPlan->graph == NULL ); 

    //--------------------------------------
    // subquery node�� ���� subquery type �Ǵ�
    //--------------------------------------

    IDE_TEST( checkSubqueryType( aStatement,
                                 aSubQNode,
                                 &sSubQType ) != IDE_SUCCESS );

    if( ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet->setOp
        == QMS_NONE )
    {
        sSubqueryIsSet = ID_FALSE;
    }
    else
    {
        sSubqueryIsSet = ID_TRUE;
    }

    //--------------------------------------
    // transform NJ, store and search, IN subquery keyRange
    // ���뿩�θ� �Ǵ��ϱ� ����
    // predicate column�� subquery target column�� cardinality�� ���Ѵ�.
    //--------------------------------------

    if( aNode != NULL )
    {
        if( qtcNodeI::isColumnArgument( aStatement, aNode ) == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            sColumnNodeIsColumn = ID_FALSE;
        }

        if( sColumnNodeIsColumn == ID_TRUE )
        {
            // predicate column�� cardinality�� ���Ѵ�.
            IDE_TEST( getColumnCardinality( aStatement,
                                            aNode,
                                            &sColumnCardinality )
                      != IDE_SUCCESS );

            // subquery node�� subquery target column�� cardinality�� ���Ѵ�.
            IDE_TEST( getSubQTargetCardinality( aStatement,
                                                aSubQNode,
                                                &sTargetCardinality )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // (NOT)EXISTS, (NOT)UNIQUE, IS NULL, IS NOT NULL �� ���,
        sColumnNodeIsColumn = ID_FALSE;
    }

    if ((aTryKeyRange == ID_TRUE) && (sColumnNodeIsColumn == ID_TRUE))
    {
        sIsInSubqueryKeyRangePossible = ID_TRUE;
    }
    else
    {
        sIsInSubqueryKeyRangePossible = ID_FALSE;
    }

    //--------------------------------------
    // graph ���� ��, predicate�� subquery ����ȭ �� ����
    //--------------------------------------

    if( sColumnNodeIsColumn == ID_FALSE )
    {
        // ��: select * from t1 where ( subquery ) = ( subquery );
        // transforNJ ����ȭ���� �������� �ʴ´�.
    }
    else
    {
        if ((sIsInSubqueryKeyRangePossible == ID_TRUE) &&
            (QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD == 1))
        {
            /*
             * BUG-34235
             * in subquery key range tip �켱 ����
             * => transform NJ ���� �ʴ´�.
             */
        }
        else
        {
            if ((sColumnCardinality < sTargetCardinality) ||
                (QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD == 2))
            {
                //----------------------------------------------------------
                // column cardinality < subquery target column�� cardinality
                // �̸�, transform NJ ����ȭ �� ����
                //----------------------------------------------------------
                IDE_TEST(transformNJ(aStatement,
                                     aCompareNode,
                                     aNode,
                                     aSubQNode,
                                     sSubQType,
                                     sSubqueryIsSet,
                                     &sIsTransformNJ)
                         != IDE_SUCCESS);
            }
            else
            {
            }
        }
    }

    //--------------------------------------
    // graph ����
    //--------------------------------------

    IDE_TEST( makeGraph( aSubQNode->subquery ) != IDE_SUCCESS );

    //--------------------------------------
    // graph ���� ��, transform NJ ����ȭ �� ���뿩�� ����
    //--------------------------------------

    //--------------------------------------
    // graph ���� ��, predicate�� subquery ����ȭ ����
    //--------------------------------------

    if( sIsTransformNJ == ID_TRUE )
    {
        // transform NJ�� ����Ǵ� ���
        sPROJ = (qmgPROJ *)(aSubQNode->subquery->myPlan->graph);

        sPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
        sPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_TRANSFORMNJ;
    }
    else
    {
        // transform NJ�� ������� ���� ���,
        // store and search ����ȭ ���� ������ �� �ִ�.
        // �ٸ�, left�� subquery�̰� �� �����ڰ� group �������̸�
        // storeAndSearch�� ���� �ʴ´�.
        // BUG-10328 fix, by kumdory
        // �׿ܿ���
        // fix BUG-12934
        // column�� �;� �� �ڸ��� host ������ ���ų�, ����� �� ����
        // constant filter�� �з��� ���̹Ƿ�
        // store and search ȿ���� ����, store and search�� ���� �ʴ´�.
        // ��) (subquery) in (subquery)
        //     ? in (subquery), ?=(subquery)
        //     1 in (subquery), 1=(subquery) ...
        
        // �߰���,
        // BUG-28929 ? between subquery�� ����
        // where���� ȣ��Ʈ���� �񱳿����� store and search�� �Ǵ� subquery�� ���� ���
        // ���� ����������.
        // ��) i1 between ? and (subquery)
        if( aNode != NULL )
        {
            if ( ( aSubQNode == (qtcNode*)aCompareNode->node.arguments ) &&
                 ( ( aCompareNode->node.module->lflag &
                     MTC_NODE_GROUP_COMPARISON_MASK )
                   == MTC_NODE_GROUP_COMPARISON_TRUE ) )
            {
                sIsStoreAndSearch = ID_FALSE;
            }
            else
            {
                // �� �������� �ٽ�
                // constantFilter������ �Ǵ��ϱⰡ �������� �ʾ�
                // predicate�з���, constant filter�� �Ǵܵ� ������ �̿�
                // �� ������ qtcNode.flag�� �ӽ÷� ������ �ΰ�, �̸� �̿��Ѵ�.
                if( ( aCompareNode->lflag & QTC_NODE_CONSTANT_FILTER_MASK )
                    == QTC_NODE_CONSTANT_FILTER_TRUE )
                {
                    sIsStoreAndSearch = ID_FALSE;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }

        if( sIsStoreAndSearch == ID_FALSE )
        {
            // Nothing to do
        }
        else
        {
            IDE_TEST( storeAndSearch( aStatement,
                                      aCompareNode,
                                      aNode,
                                      aSubQNode,
                                      sSubQType,
                                      sSubqueryIsSet ) != IDE_SUCCESS );
        }
    }

    // To Fix PR-11460
    // Transform NJ�� ����Ǿ��ٸ�,
    // In Subquery KeyRange�Ǵ� Store And Search�� ����� �� ����.
    // To Fix PR-11461
    // In Subquery KeyRange �Ǵ� Subquery KeyRange ����ȭ��
    // �Ϲ� Table�� WHERE�� ���ؼ��� �����ϴ�.
    if ((sIsInSubqueryKeyRangePossible == ID_FALSE) ||
        (sIsTransformNJ == ID_TRUE))
    {
        // ��: select * from t1 where ( subquery ) = ( subquery );
        // IN subquery keyRange, subquery keyRange ����ȭ ����
        // �������� �ʴ´�.
    }
    else
    {
        // To Fix PR-11460
        // In Subquery Key Range ���� �˻��
        // Target Cardinality �� �ƴ϶�,
        // Subquery�� ��� ������ ����Ͽ��� �Ѵ�.
        // �� �߿� ���� ������ �˻��Ѵ�.

        sSubqueryResultCnt =
            aSubQNode->subquery->myPlan->graph->costInfo.outputRecordCnt;

        sSubqueryResultCnt = ( sSubqueryResultCnt < sTargetCardinality ) ?
            sSubqueryResultCnt : sTargetCardinality;

        if ((QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD == 1) ||
            ((QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD == 0) &&
             (sColumnCardinality >= sSubqueryResultCnt)))
        {
            //-----------------------------------------------------------
            // column cardinality >= subquery target column�� cardinality
            // �̸�, IN subquery keyRange ����ȭ �� ����
            //
            // BUG-34235
            // cardinality�� ������� in subquery key range �׻� ����
            //-----------------------------------------------------------

            IDE_TEST( inSubqueryKeyRange( aCompareNode,
                                          aSubQNode,
                                          sSubQType ) != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        // subquery keyRange
        IDE_TEST( subqueryKeyRange( aCompareNode,
                                    aSubQNode,
                                    sSubQType ) != IDE_SUCCESS );
    }

    qcgPlan::registerPlanProperty(aStatement,
                                  PLAN_PROPERTY_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoSubquery::transformNJ( qcStatement     * aStatement,
                          qtcNode         * aCompareNode,
                          qtcNode         * aNode,
                          qtcNode         * aSubQNode,
                          qmoSubqueryType   aSubQType,
                          idBool            aSubqueryIsSet,
                          idBool          * aIsTransform )
{
/***********************************************************************
 *
 * Description : transform NJ ����ȭ �� ����
 *
 *  < transform NJ ����ȭ �� >
 *
 *  subquery�� type N�� => type J��, type J�� => type J������ �����Ų��.
 *
 *  ��) select * from t1 where i1 in ( select i1 from t2 );
 *  ==> select * from t1 where i1 in ( select i1 from t2 where t2.i1=t1.i1;
 *
 *  �̷��� ���� ������ subquery���� predicate�� �߰��Ͽ� ���� ����
 *  selection�� �����ϴµ� �� ������ �ִ�.
 *
 *  < transform NJ �� �������� >
 *  0. predicate�� IN, NOT IN�� ���.
 *  1. type N, type J ��  subquery�� ���
 *  2. subquery�� SET���� �ƴϾ�� �Ѵ�.
 *  3. subquery�� LIMIT���� ����� �Ѵ�.
 *  4. predicate column�� NOT NULL�̰�, subquery target�� NOT NULL�� ���
 *  5. subquery target column�� �ε����� �ִ� ���(type N���� �ش�)
 *  6. PR-11632) Type N �� ��� Subquery�� ����� �پ�� �� �ִ� ����
 *     Transform NJ�� �������� ����.
 *         - Subquery�� DISTINCT�� �ִ� ���
 *         - Subquery�� WHERE ���� �ִ� ���
 *
 * Implementation :
 *
 *     1. transform NJ �� �������� �˻�
 *     2. subquery���� �߰��� predicate�� ����, ���� where���� ����.
 *     3. subquery node�� dependencies�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool         sIsTemp = ID_TRUE;
    idBool         sIsNotNull;
    idBool         sIsExistIndex;
    idBool         sIsTransform = ID_FALSE;
    qmsParseTree * sSubQParseTree;

    qmsQuerySet  * sSubQuerySet;
    qmsSFWGH     * sSubSFWGH;

    IDU_FIT_POINT_FATAL( "qmoSubquery::transformNJ::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aIsTransform != NULL );

    //--------------------------------------
    // transform NJ ����ȭ ��
    //--------------------------------------

    while( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        //--------------------------------------
        // ���� �˻�
        //--------------------------------------

        // �񱳿����� �˻� ( in, not in )
        if( ( aCompareNode->node.module == &mtfEqualAny ) ||
            ( aCompareNode->node.module == &mtfNotEqualAll ) )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        sSubQParseTree = (qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree);

        // type N, type J ��  subquery�� ����̰�,
        // subquery�� set���� �ƴϰ�, subquery�� LIMIT���� ����� �Ѵ�.

        if( ( aSubQType == QMO_TYPE_N ) || ( aSubQType == QMO_TYPE_J ) )
        {
            if( aSubqueryIsSet == ID_FALSE )
            {
                if( sSubQParseTree->limit == NULL )
                {
                    // Nothing To Do
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }

        // predicate column�� NOT NULL�̰�,
        // subquery target�� NOT NULL�� ���
        IDE_TEST( checkNotNull( aStatement,
                                aNode,
                                aSubQNode,
                                &sIsNotNull ) != IDE_SUCCESS );

        if( sIsNotNull == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        // subquery target column�� �ε����� �ִ� ���(type N���� �ش�)
        if( aSubQType == QMO_TYPE_N )
        {
            IDE_TEST( checkIndex4SubQTarget( aStatement,
                                             aSubQNode,
                                             &sIsExistIndex )
                      != IDE_SUCCESS );

            if( sIsExistIndex == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }

            //----------------------------------------------------
            // To Fix PR-11632
            // �ڼ��� ������ Bug Description ����
            // Type N �� ��� Subquery�� ����� �پ�� �� �ִ� ����
            // Transform NJ�� �������� ����.
            //   - Subquery�� DISTINCT�� �ִ� ���
            //   - Subquery�� WHERE ���� �ִ� ���
            //----------------------------------------------------

            sSubQuerySet =
                ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet;

            // �ݵ�� SFWGH�� �����Ѵ�.
            IDE_DASSERT( sSubQuerySet->SFWGH != NULL );
            sSubSFWGH = sSubQuerySet->SFWGH;

            // Distinct�� �����ϴ� �� �˻�
            if ( sSubSFWGH->selectType == QMS_DISTINCT )
            {
                break;
            }
            else
            {
                // Nothing To Do
            }

            // WHERE ���� �����ϴ� �� �˻�
            if ( sSubSFWGH->where != NULL )
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

        //--------------------------------------
        // subquery���� �߰��� predicate�� ����, ���� where���� ����.
        //--------------------------------------

        IDE_TEST( makeNewPredAndLink( aStatement,
                                      aNode,
                                      aSubQNode ) != IDE_SUCCESS );

        sIsTransform = ID_TRUE;


        //--------------------------------------
        // transform NJ ����ȭ ���� ����Ǿ�����
        // subquery projection graph�� subquery ����ȭ �� flag��
        // �����ؾ� �ϳ�, ���� graph ���� ���̹Ƿ�,
        // graph ���� ����, transform NJ ����ȭ �� �������� ����.
        //--------------------------------------

    }

    *aIsTransform = sIsTransform;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::checkNotNull( qcStatement * aStatement,
                           qtcNode     * aNode,
                           qtcNode     * aSubQNode,
                           idBool      * aIsNotNull )
{
/***********************************************************************
 *
 * Description : transform NJ ����ȭ �� �����
 *               predicate column�� subquery target column��
 *               not null �������� �˻�
 *
 * Implementation :
 *
 *     1. predicate column�� subquery target column��
 *        base table�� column���� �˻�
 *     2. 1�� ������ �����ϸ�, not null ���ǰ˻�.
 *
 ***********************************************************************/

    idBool       sIsTemp = ID_TRUE;
    idBool       sIsNotNull;
    qtcNode    * sNode;

    mtcTemplate       * sMtcTemplate;

    IDU_FIT_POINT_FATAL( "qmoSubquery::checkNotNull::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aNode != NULL );
    IDE_FT_ASSERT( aSubQNode != NULL );
    IDE_FT_ASSERT( aIsNotNull != NULL );

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    //--------------------------------------
    //  not null �˻�
    //--------------------------------------

    while( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        sIsNotNull = ID_TRUE;

        //--------------------------------------
        // predicate column�� not null �˻�
        //--------------------------------------

        if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_LIST )
        {
            sNode = (qtcNode *)(aNode->node.arguments);

            while( sNode != NULL )
            {
                if( ( sMtcTemplate->rows[sNode->node.table].lflag
                      & MTC_TUPLE_TYPE_MASK )
                    == MTC_TUPLE_TYPE_TABLE )
                {
                    if( ( sMtcTemplate->rows[sNode->node.table]
                          .columns[sNode->node.column].flag
                          & MTC_COLUMN_NOTNULL_MASK )
                        == MTC_COLUMN_NOTNULL_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsNotNull = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsNotNull = ID_FALSE;
                    break;
                }

                sNode = (qtcNode *)(sNode->node.next);
            }
        }
        else
        {
            sNode = aNode;

            if( ( sMtcTemplate->rows[sNode->node.table].lflag
                  & MTC_TUPLE_TYPE_MASK )
                == MTC_TUPLE_TYPE_TABLE )
            {
                if( ( sMtcTemplate->rows[sNode->node.table]
                      .columns[sNode->node.column].flag
                      & MTC_COLUMN_NOTNULL_MASK )
                    == MTC_COLUMN_NOTNULL_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsNotNull = ID_FALSE;
                }
            }
            else
            {
                sIsNotNull = ID_FALSE;
            }
        }

        if( sIsNotNull == ID_TRUE )
        {
            //--------------------------------------
            // subquery target column�� not null �˻�
            //--------------------------------------

            IDE_TEST( checkNotNullSubQTarget( aStatement,
                                              aSubQNode,
                                              &sIsNotNull ) != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    *aIsNotNull = sIsNotNull;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::checkNotNullSubQTarget( qcStatement * aStatement,
                                     qtcNode     * aSubQNode,
                                     idBool      * aIsNotNull )
{
/***********************************************************************
 *
 * Description : transform NJ, store and search ����ȭ �� �����,
 *               subquery target column�� not null �������� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool       sIsTemp = ID_TRUE;
    idBool       sIsNotNull;
    qmsTarget  * sTarget;
    qtcNode    * sNode;

    mtcTemplate * sMtcTemplate;

    IDU_FIT_POINT_FATAL( "qmoSubquery::checkNotNullSubQTarget::__FT__" );


    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aIsNotNull != NULL );

    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    //--------------------------------------
    // subquery target column�� not null �˻�
    //--------------------------------------

    while( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        sIsNotNull = ID_TRUE;

        sTarget =
            ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->
            querySet->target;

        while( sTarget != NULL )
        {
            sNode = sTarget->targetColumn;

            // fix BUG-8936
            // ��) create table t1( i1 not null );
            //     create table t2( i1 not null );
            //     select i1 from t1
            //     where i1 in ( select i1 from t2 group by i1);
            //     �� ���ǹ��� ���, subquery target�� passNode�� ����ǹǷ�
            //     subquery target�� ���� not null �˻��
            //     passNode->node.argument���� �����Ѵ�.
 
            // BUG-20272
            if( (sNode->node.module == & qtc::passModule) ||
                (sNode->node.module == & mtfDecrypt) )
            {
                sNode = (qtcNode *)(sNode->node.arguments);
            }
            else
            {
                // Nothing To Do
            }

            if( ( sMtcTemplate->rows[sNode->node.table].lflag
                  & MTC_TUPLE_TYPE_MASK )
                == MTC_TUPLE_TYPE_TABLE )
            {
                if( ( sMtcTemplate->rows[sNode->node.table]
                      .columns[sNode->node.column].flag
                      & MTC_COLUMN_NOTNULL_MASK )
                    == MTC_COLUMN_NOTNULL_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsNotNull = ID_FALSE;
                    break;
                }
            }
            else
            {
                sIsNotNull = ID_FALSE;
                break;
            }

            sTarget = sTarget->next;
        }
    }

    *aIsNotNull = sIsNotNull;

    return IDE_SUCCESS;
}


IDE_RC
qmoSubquery::checkIndex4SubQTarget( qcStatement * aStatement,
                                    qtcNode     * aSubQNode,
                                    idBool      * aIsExistIndex )
{
/***********************************************************************
 *
 * Description : transform NJ ����ȭ �� �����
 *               subquery target column�� �ε����� �����ϴ��� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool          sIsTemp = ID_TRUE;
    idBool          sIsExistIndex = ID_TRUE;
    UShort          sTable;
    UInt            i;
    UInt            sCnt;
    UInt            sTargetCnt = 0;
    UInt            sIdxColumnID;
    UInt            sID;
    UInt            sCount;
    qmsQuerySet   * sQuerySet;
    qmsTarget     * sTarget;
    qcmTableInfo  * sTableInfo;
    qcmIndex      * sIndices = NULL;
    qtcNode       * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::checkIndex4SubQTarget::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aIsExistIndex != NULL );

    //--------------------------------------
    // subquery target column�� �ε��� ���� ���� �Ǵ�
    //--------------------------------------

    while( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        sQuerySet =
            ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet;

        //--------------------------------------
        // subquery target column�� ���� ���̺��� �÷����� �˻�
        // target column�� outer column�� ����,
        // subquery type J������ �ǴܵǱ⶧����, �� �Լ��� ������ ����.
        // ����, subuqery from���� table������ ���� �˻����� �ʴ´�.
        //--------------------------------------

        sTarget = sQuerySet->target;
        
        sNode = sTarget->targetColumn;

        // BUG-20272
        if ( sNode->node.module == &mtfDecrypt )
        {
            sNode = (qtcNode*) sNode->node.arguments;
        }
        else
        {
            // Nothing to do.
        }
        
        sTable = sNode->node.table;
        
        for( ;
             sTarget != NULL;
             sTarget = sTarget->next )
        {
            sTargetCnt++;

            sNode = sTarget->targetColumn;
            
            // BUG-20272
            if ( sNode->node.module == &mtfDecrypt )
            {
                sNode = (qtcNode*) sNode->node.arguments;
            }
            else
            {
                // Nothing to do.
            }
        
            if( sTable == sNode->node.table )
            {
                // Nothing To Do
            }
            else
            {
                sIsExistIndex = ID_FALSE;
                break;
            }
        }

        if( sIsExistIndex == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        //--------------------------------------
        // target column�� �ε����� �����ϴ��� �˻�.
        //--------------------------------------
        sTableInfo =
            QC_SHARED_TMPLATE(aStatement)->tableMap[sTable].
            from->tableRef->tableInfo;
        sIndices = sTableInfo->indices;

        if( sTableInfo->indexCount == 0 )
        {
            sIsExistIndex = ID_FALSE;
            break;
        }
        else
        {
            // Nothing To Do
        }

        for( sCnt = 0; sCnt < sTableInfo->indexCount; sCnt++ )
        {
            if( sIndices[sCnt].keyColCount >= sTargetCnt )
            {
                // Nothint To Do
            }
            else
            {
                continue;
            }

            for( i = 0, sCount = 0; i < sTargetCnt; i++ )
            {
                sIdxColumnID = sIndices[sCnt].keyColumns[i].column.id;

                for( sTarget = sQuerySet->target;
                     sTarget != NULL;
                     sTarget = sTarget->next )
                {
                    sNode = sTarget->targetColumn;

                    if( sNode->node.conversion == NULL )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsExistIndex = ID_FALSE;
                        break;
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
                    
                    sID =
                        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sNode->node.table].
                        columns[sNode->node.column].column.id;

                    if( sIdxColumnID == sID )
                    {
                        sCount++;
                        break;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }

                if( sIsExistIndex == ID_TRUE )
                {
                    if( sNode != NULL )
                    {
                        // �������� �ε��� ���
                        // Nothing To Do
                    }
                    else
                    {
                        // �������� �ε��� ����� �ƴ�.
                        sIsExistIndex = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    break;
                }
            } // target column�� ���ϴ� �ε����� �����ϴ��� �˻�

            if( sIsExistIndex == ID_TRUE )
            {
                if( sTargetCnt == sCount )
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
                break;
            }
        }
    }

    *aIsExistIndex = sIsExistIndex;

    return IDE_SUCCESS;
}


IDE_RC
qmoSubquery::makeNewPredAndLink( qcStatement * aStatement,
                                 qtcNode     * aNode,
                                 qtcNode     * aSubQNode )
{
/***********************************************************************
 *
 * Description : transform NJ ����ȭ �� �����
 *          ���ο� predicate�� ����, subquery�� ���� where���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qcNamePosition      sPosition;
    qmsSFWGH          * sSFWGH;
    qmsTarget         * sTarget;
    qtcNode           * sNode[2];
    qtcNode           * sListNode[2];
    qtcNode           * sLastNode;
    qtcNode           * sEqualNode;
    qtcNode           * sAndNode;
    qtcNode           * sTargetColumn;
    qtcNode           * sTargetNode;
    qtcNode           * sPredColumn;
    qtcNode           * sTempTargetColumn;

    IDU_FIT_POINT_FATAL( "qmoSubquery::makeNewPredAndLink::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );

    //--------------------------------------
    // subquery target column�� ���� ��� ����
    //--------------------------------------

    sSFWGH =
        ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet->SFWGH;

    if( sSFWGH->target->next == NULL )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                 (void **)&sTargetColumn )
                  != IDE_SUCCESS );

        sTempTargetColumn = sSFWGH->target->targetColumn;

        // BUG-20272
        if ( sTempTargetColumn->node.module == &mtfDecrypt )
        {
            sTempTargetColumn = (qtcNode*) sTempTargetColumn->node.arguments;
        }
        else
        {
            // Nothing to do.
        }

        idlOS::memcpy(
            sTargetColumn, sTempTargetColumn, ID_SIZEOF(qtcNode) );
    }
    else
    {
        //--------------------------------------
        // target column�� �������� ���, LIST���·� ��� ����
        //--------------------------------------

        // LIST ��� ����
        SET_EMPTY_POSITION( sPosition );

        IDE_TEST( qtc::makeNode( aStatement,
                                 sListNode,
                                 &sPosition,
                                 &mtfList ) != IDE_SUCCESS );

        // subquery�� target column���� �����ؼ� �����Ѵ�.

        sTarget = sSFWGH->target;

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                 (void **)&sTargetNode )
                  != IDE_SUCCESS );
        
        sTempTargetColumn = sTarget->targetColumn;

        // BUG-20272
        if ( sTempTargetColumn->node.module == &mtfDecrypt )
        {
            sTempTargetColumn = (qtcNode*) sTempTargetColumn->node.arguments;
        }
        else
        {
            // Nothing to do.
        }

        idlOS::memcpy(
            sTargetNode, sTempTargetColumn, ID_SIZEOF(qtcNode) );

        sListNode[0]->node.arguments = (mtcNode *)sTargetNode;
        sLastNode = (qtcNode *)(sListNode[0]->node.arguments);

        sTarget = sTarget->next;

        while( sTarget != NULL )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                     (void **)&sTargetNode )
                      != IDE_SUCCESS );
            
            sTempTargetColumn = sTarget->targetColumn;

            // BUG-20272
            if ( sTempTargetColumn->node.module == &mtfDecrypt )
            {
                sTempTargetColumn = (qtcNode*) sTempTargetColumn->node.arguments;
            }
            else
            {
                // Nothing to do.
            }

            idlOS::memcpy(
                sTargetNode, sTempTargetColumn, ID_SIZEOF(qtcNode) );

            sLastNode->node.next = (mtcNode*)sTargetNode;
            sLastNode = (qtcNode *)(sLastNode->node.next);

            sTarget = sTarget->next;
        }

        sTargetColumn = sListNode[0];

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sTargetColumn )
                  != IDE_SUCCESS );
    }

    //--------------------------------------
    // predicate column�� ���� ��� ����
    //--------------------------------------

    if( aNode->node.module == &mtfList )
    {
        IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                         aNode,
                                         &sPredColumn,
                                         ID_FALSE,
                                         ID_FALSE,
                                         ID_FALSE,
                                         ID_FALSE ) // column node�̹Ƿ�
                  // constant node�� ����.
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                 (void **)&sPredColumn )
                  != IDE_SUCCESS );

        idlOS::memcpy( sPredColumn, aNode, ID_SIZEOF(qtcNode) );
    }

    sPredColumn->node.next = NULL;

    //--------------------------------------
    // equal �񱳿����� ����
    //--------------------------------------

    SET_EMPTY_POSITION(sPosition);

    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             &sPosition,
                             &mtfEqual ) != IDE_SUCCESS );
    sEqualNode = sNode[0];

    sEqualNode->node.arguments = (mtcNode *)sTargetColumn;
    sEqualNode->node.arguments->next = (mtcNode *)sPredColumn;
    sEqualNode->indexArgument = 0;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sEqualNode )
              != IDE_SUCCESS );

    //--------------------------------------
    // subquery�� where���� ���� ������ predicate ����
    //--------------------------------------

    if( sSFWGH->where == NULL )
    {
        sSFWGH->where = sEqualNode;
    }
    else
    {
        if( ( sSFWGH->where->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_AND )
        {
            sEqualNode->node.next = sSFWGH->where->node.arguments;
            sSFWGH->where->node.arguments = (mtcNode *)sEqualNode;
        }
        else
        {
            IDE_TEST( qtc::makeNode( aStatement,
                                     sNode,
                                     &sPosition,
                                     &mtfAnd ) != IDE_SUCCESS );

            sAndNode = sNode[0];

            sAndNode->node.arguments = (mtcNode *)sEqualNode;
            sAndNode->node.arguments->next = (mtcNode *)(sSFWGH->where);
            sSFWGH->where = sAndNode;
        }

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sSFWGH->where )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::storeAndSearch( qcStatement    * aStatement,
                             qtcNode        * aCompareNode,
                             qtcNode        * aNode,
                             qtcNode        * aSubQNode,
                             qmoSubqueryType  aSubQType,
                             idBool           aSubqueryIsSet )
{
/***********************************************************************
 *
 * Description : store and search ����ȭ �� ����
 *
 *  < store and search ����ȭ �� >
 *
 *  type A��, type N�� subquery�� ���, outer query�� ���ǿ� ���� ����
 *  ������ ����� �����Ѵ�. ����, subquery�� �ݺ������� �������� �ʰ�
 *  ���� ����� ������ �Ŀ� search�� �� �ֵ��� �Ͽ� ���� ����� ���� �� �ִ�.
 *
 *  < store and search �� �������� >
 *  1. type A�� �Ǵ� type N�� subquery ( transformNJ�� ����Ǹ� �ȵ�)
 *  2. predicate��
 *     (1) IN(=ANY), {>,>=,<,<=}ANY, {>,>=,<,<=}ALL
 *     (2) =,>,>=,<,<= : subquery filter�� ���,
 *                       subquery�� �Ź� �����Ǵ� ���� �����ϱ� ����
 *                       �� ���� �ο�� store and search�� ó���Ѵ�.
 *
 * Implementation :
 *
 *     subquery graph���� ��, subquery type A, N���� ����
 *     qmgProjection graph�� store and search tip�� �������� �����ϰ�,
 *     HASH�� ���, qmgProjection graph�� storeNSearchPred �� �޾��ش�.
 *
 *    [����������]
 *
 *    1. IN(=ANY), NOT IN(!=ALL)
 *       (1) �� �÷��� ���, hash�� ����.
 *       (2) �� �÷� �̻��� ���,
 *          predicate ���� ��� not null constraint�� �����ϴ����� �˻�.
 *          .not null constraint�� ���� : hash�� ����
 *          .not null constraint�� �������� ����: SORT��忡 storeOnly�� ����.
 *
 *    2. {>,>=,<,<=}ANY, {>,>=,<,<=}ALL
 *       LMST�� ����
 *
 *    3. =ALL, !=ANY
 *       (1) �� �÷��� ���, LMST�� ����.
 *       (2) �� �÷� �̻��� ���,
 *           predicate ���� ��� not null constraint�� �����ϴ����� �˻�.
 *           .not null constraint�� ���� : LMST�� ����
 *           .not null constraint�� �������� ����: SORT��忡 storeOnly�� ����
 *
 *    4. =, >, >=, <, <=
 *       SORT��忡 store only�� ����.
 *
 *    [ IN(=ANY), NOT IN(!=ALL), =ALL, !=ANY �������� ������� ]
 *    ------------------------------------------------------------
 *      �� ���, �� �÷��̻��� ���, not null constraint�� �������� ������,
 *      SORT ��忡 store only�� �����ϰ� �ȴ�.
 *      �̶�, subquery ����� ����� �پ��� ��츸, �����ϵ��� �Ѵ�.
 *      ( SORT ��忡 store only�� �����ϰ� �Ǹ�, full scan���� ó���ǹǷ�)
 *
 *      1. SET ���� ���, �������� �ʴ´�.
 *         UNION ALL�� �ƴ� ����, �̹� ����� ����Ǿ� �ְ�,
 *         UNION ALL�� ����, ��� subquery�� ���� �˻翡 ���� ���ϰ� ũ�Ƿ�.
 *      2. SET ���� �ƴ� ���,
 *         (1) where���� �����ϰų�,
 *         (2) group by�� �����ϰų�,
 *         (3) aggregation�� �����ϰų�,
 *         (4) distinct�� �����ϴ� ��쿡 �����Ѵ�.
 *
 *   [����]
 *   �� �÷��̻��� ���,
 *   predicate ���� �÷��� ��� not null constraint�� �����ؾ� �Ѵ�.
 *   �Ʒ��� ��ó��, ���� ����� Ʋ���� �ֱ� �����̴�.
 *   ��) (2,3,5) IN (4,7,NULL)       => FALSE
 *       (2,3,5) IN (NULL,3,NULL)    => UNKOWN
 *       (2,3,5) NOT IN (4,7,NULL)   => TRUE
 *       (2,3,5) NOT IN (NULL,3,NULL)=> UNKOWN
 *
 ***********************************************************************/

    idBool         sIsSortNodeStoreOnly;
    qmsQuerySet  * sQuerySet;
    qmgPROJ      * sPROJ;

    IDU_FIT_POINT_FATAL( "qmoSubquery::storeAndSearch::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );

    //--------------------------------------
    // store and search ����ȭ �� ����
    //--------------------------------------

    //--------------------------------------
    // ���ǰ˻�
    //--------------------------------------

    // subquery type �� A, N ���̾�� �Ѵ�.
    if( ( aSubQType == QMO_TYPE_A ) || ( aSubQType == QMO_TYPE_N ) )
    {
        sQuerySet =
            ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet;
        sPROJ  = (qmgPROJ *)(aSubQNode->subquery->myPlan->graph);

        //--------------------------------------
        // ������ ����
        //--------------------------------------

        if( ( aCompareNode->node.module == & mtfEqualAny ) ||
            ( aCompareNode->node.module == & mtfNotEqualAll ) )
        {
            //--------------------------------------
            // IN(=ANY), NOT IN(!=ALL)
            //--------------------------------------
            //-----------------------
            // store and search ���뿩�ο� ����������
            //-----------------------

            IDE_TEST( setStoreFlagIN( aStatement,
                                      aCompareNode,
                                      aNode,
                                      aSubQNode,
                                      sQuerySet,
                                      sPROJ,
                                      aSubqueryIsSet ) != IDE_SUCCESS );
        }
        else if( ( aCompareNode->node.module == & mtfEqualAll ) ||
                 ( aCompareNode->node.module == & mtfNotEqualAny ) )
        {
            //---------------------------------------
            // =ALL, !=ANY
            //--------------------------------------
            //------------------------
            // store and search ���뿩�ο� ����������
            //------------------------

            IDE_TEST( setStoreFlagEqualAll( aStatement,
                                            aNode,
                                            aSubQNode,
                                            sQuerySet,
                                            sPROJ,
                                            aSubqueryIsSet ) != IDE_SUCCESS );
        }
        else
        {
            switch( aCompareNode->node.module->lflag
                    & MTC_NODE_GROUP_COMPARISON_MASK )
            {
                case ( MTC_NODE_GROUP_COMPARISON_TRUE ) :
                {
                    //--------------------------------------
                    // {>,>=,<,<=}ANY, {>,>=,<,<=}ALL �� ���,
                    //  : LMST�� ����
                    //--------------------------------------

                    switch( aCompareNode->node.module->lflag
                            & MTC_NODE_OPERATOR_MASK )
                    {
                        case MTC_NODE_OPERATOR_GREATER :
                        case MTC_NODE_OPERATOR_GREATER_EQUAL :
                        case MTC_NODE_OPERATOR_LESS :
                        case MTC_NODE_OPERATOR_LESS_EQUAL :
                            sPROJ->subqueryTipFlag
                                &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
                            sPROJ->subqueryTipFlag
                                |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

                            sPROJ->subqueryTipFlag
                                &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
                            sPROJ->subqueryTipFlag
                                |= QMG_PROJ_SUBQUERY_STORENSEARCH_LMST;
                            break;
                        default :
                            // Nothint To Do
                            break;
                    }
                    break;
                }
                case ( MTC_NODE_GROUP_COMPARISON_FALSE ) :
                {
                    //--------------------------------------
                    //  =, !=, >, >=, <, <=,
                    //  IS NULL, IS NOT NULL, LIKE, NOT LIKE �� ���,
                    //  : SORT ��忡 store only�� ����.
                    //--------------------------------------

                    sIsSortNodeStoreOnly = ID_FALSE;

                    switch( aCompareNode->node.module->lflag
                            & MTC_NODE_OPERATOR_MASK )
                    {
                        case MTC_NODE_OPERATOR_EQUAL :
                        case MTC_NODE_OPERATOR_NOT_EQUAL :
                        case MTC_NODE_OPERATOR_GREATER :
                        case MTC_NODE_OPERATOR_GREATER_EQUAL :
                        case MTC_NODE_OPERATOR_LESS :
                        case MTC_NODE_OPERATOR_LESS_EQUAL :
                            sIsSortNodeStoreOnly = ID_TRUE;
                            break;
                        default :
                            // Nothing To Do
                            break;
                    }

                    if( ( aCompareNode->node.module == &mtfIsNull )    ||
                        ( aCompareNode->node.module == &mtfIsNotNull ) ||
                        ( aCompareNode->node.module == &mtfLike )      ||
                        ( aCompareNode->node.module == &mtfNotLike )   ||
                        ( aCompareNode->node.module == &mtfBetween )   ||
                        ( aCompareNode->node.module == &mtfNotBetween ) )
                    {
                        sIsSortNodeStoreOnly = ID_TRUE;
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    if( sIsSortNodeStoreOnly == ID_TRUE )
                    {
                        sPROJ->subqueryTipFlag
                            &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
                        sPROJ->subqueryTipFlag
                            |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

                        sPROJ->subqueryTipFlag
                            &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
                        sPROJ->subqueryTipFlag
                            |= QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                    break;
                }
                default :
                    // Nothing To Do
                    break;
            }
        }
    }
    else
    {
        // subquery type J, JA ������,
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::setStoreFlagIN( qcStatement * aStatement,
                             qtcNode     * aCompareNode,
                             qtcNode     * aNode,
                             qtcNode     * aSubQNode,
                             qmsQuerySet * aQuerySet,
                             qmgPROJ     * aPROJ,
                             idBool        aSubqueryIsSet )
{
/***********************************************************************
 *
 * Description : store and search ����ȭ �� �����
 *               IN(=ANY), NOT IN(!=ALL)�� ���� ������ ����
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool    sIsNotNull;
    idBool    sIsHash = ID_FALSE;
    idBool    sIsStoreOnly = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoSubquery::setStoreFlagIN::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aPROJ != NULL );

    if( aQuerySet->target->next == NULL )
    {
        //--------------------------------------
        // �� �÷��� ���,
        // ��忡�� subquery target column�� ���� not null �˻翩�ο�
        // ���� flag ����.
        // ��: i1 IN ( select a1 from ... )
        //     i1�� ��忡�� null �˻縦 �����ϹǷ�,
        //     subquery target�� a1�� ���ؼ��� �˻�.
        //     1) a1�� not null �̸�,
        //        ��忡�� not null �˻縦 ���� �ʾƵ� �ȴٴ� ��������.
        //     2) a1�� not null�� �ƴϸ�,
        //        ��忡�� not null �˻縦 �ؾ� �Ѵٴ� ���� ����.
        //--------------------------------------

        IDE_TEST( checkNotNullSubQTarget( aStatement,
                                          aSubQNode,
                                          &sIsNotNull )
                  != IDE_SUCCESS );


        // �Ʒ����� HASH�� ������ ������ ����,
        sIsHash = ID_TRUE;
    }
    else
    {
        IDE_TEST( checkNotNull( aStatement,
                                aNode,
                                aSubQNode,
                                &sIsNotNull ) != IDE_SUCCESS );

        if( sIsNotNull == ID_TRUE )
        {
            sIsHash = ID_TRUE;
        }
        else
        {
            // multi column�� ���, not null���������� �������� �ʴ� ���,
            // In(=ANY), NOT IN(!=ALL), =ALL, !=ANY �������� ������� ����

            if( aSubqueryIsSet == ID_TRUE )
            {
                // store and search ����ȭ ���� �������� �ʴ´�.
                // Nothing To Do
            }
            else
            {
                
                IDE_DASSERT( aQuerySet->SFWGH != NULL );

                if( ( aQuerySet->SFWGH->where != NULL )      ||
                    ( aQuerySet->SFWGH->group != NULL )      ||
                    ( aQuerySet->SFWGH->aggsDepth1 != NULL ) ||
                    ( aQuerySet->SFWGH->selectType == QMS_DISTINCT ) )
                {
                    // fix BUG-8936
                    // sort node�� store only�� ����
                    sIsStoreOnly = ID_TRUE;
                }
                else
                {
                    // store and search ����ȭ ���� �������� �ʴ´�.
                    // Nothing To Do
                }
            }
        }
    }

    //--------------------------------------
    // ����������.
    // (1) �� �÷��� ���, hash�� ����.
    // (2) �� �÷� �̻��� ���,
    //     predicate ���� ��� not null constraint�� �����ϴ����� �˻�.
    //     .not null constraint�� ���� : hash�� ����
    //     .not null constraint�� �������� ����: SORT��忡 storeOnly�� ����.
    //
    // �������� HASH�� ����, projection graph�� �ش� predicate�� �޾��ش�.
    //--------------------------------------
    if( sIsHash == ID_TRUE )
    {
        aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
        aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

        aPROJ->subqueryTipFlag
            &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
        aPROJ->subqueryTipFlag
            |= QMG_PROJ_SUBQUERY_STORENSEARCH_HASH;

        //--------------------------------------
        // store and search predicate�� qmgProjection graph�� ����.
        //--------------------------------------

        aPROJ->storeNSearchPred = aCompareNode;

        //--------------------------------------
        // not null �˻翩�� ����
        // 1. �� �÷��� ���,
        //    ��忡�� subquery target column�� ���� not null �˻翩�ο�
        //    ���� flag ����.
        //    ��: i1 IN ( select a1 from ... )
        //        i1�� ��忡�� null �˻縦 �����ϹǷ�,
        //        subquery target�� a1�� ���ؼ��� �˻�.
        //        1) a1�� not null �̸�,
        //           ��忡�� not null �˻縦 ���� �ʾƵ� �ȴٴ� ��������.
        //        2) a1�� not null�� �ƴϸ�,
        //           ��忡�� not null �˻縦 �ؾ� �Ѵٴ� ���� ����.
        // 2. ���� �÷��� ���,
        //    ��忡�� ���� �÷��� ����
        //    null �˻縦 �������� �ʾƵ� �ȴٴ� ���� ����.
        //--------------------------------------

        if( sIsNotNull == ID_TRUE )
        {
            aPROJ->subqueryTipFlag
                &= ~QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_MASK;
            aPROJ->subqueryTipFlag
                |= QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_FALSE;
        }
        else
        {
            aPROJ->subqueryTipFlag
                &= ~QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_MASK;
            aPROJ->subqueryTipFlag
                |= QMG_PROJ_SUBQUERY_HASH_NOTNULLCHECK_TRUE;
        }
    }
    else
    {
        if( sIsStoreOnly == ID_TRUE )
        {
            aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
            aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

            aPROJ->subqueryTipFlag
                &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
            aPROJ->subqueryTipFlag
                |= QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY;
        }
        else
        {
            // store and search ����ȭ�� �������� �ʴ´�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::setStoreFlagEqualAll( qcStatement  * aStatement,
                                   qtcNode      * aNode,
                                   qtcNode      * aSubQNode,
                                   qmsQuerySet  * aQuerySet,
                                   qmgPROJ      * aPROJ,
                                   idBool         aSubqueryIsSet )
{
/***********************************************************************
 *
 * Description : store and search ����ȭ �� �����
 *               =ALL, !=ANY�� ���� ������ ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool    sIsNotNull;
    idBool    sIsLMST = ID_FALSE;
    idBool    sIsStoreOnly = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoSubquery::setStoreFlagEqualAll::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aPROJ != NULL );

    //--------------------------------------
    // ����������
    //--------------------------------------

    if( aQuerySet->target->next == NULL )
    {
        //--------------------------------------
        // �� �÷��� ���, LMST�� ����.
        //--------------------------------------

        sIsLMST = ID_TRUE;
    }
    else
    {
        //--------------------------------------
        // ���� �÷��� ���,
        // predicate ���� ��� not null constraint�� �����ϴ��� �˻�.
        //--------------------------------------

        IDE_TEST( checkNotNull( aStatement,
                                aNode,
                                aSubQNode,
                                &sIsNotNull ) != IDE_SUCCESS );

        if( sIsNotNull == ID_TRUE )
        {
            sIsLMST = ID_TRUE;
        }
        else
        {
            // multi column�� ���,
            // not null constrant���������� �������� �ʴ� ���
            // IN(=ANY), NOT IN(!=ALL), =ALL, !=ANY �������� ������� ����

            if( aSubqueryIsSet == ID_TRUE )
            {
                // store and search ����ȭ ���� �������� �ʴ´�.
                // Nothing To Do
            }
            else
            {
                IDE_DASSERT( aQuerySet->SFWGH != NULL );

                if( ( aQuerySet->SFWGH->where != NULL ) ||
                    ( aQuerySet->SFWGH->group != NULL ) ||
                    ( aQuerySet->SFWGH->aggsDepth1 != NULL ) ||
                    ( aQuerySet->SFWGH->selectType == QMS_DISTINCT ) )
                {
                    // fix BUG-8936
                    // sort node�� store only�� ����
                    sIsStoreOnly = ID_TRUE;
                }
                else
                {
                    // store and search ����ȭ ���� �������� �ʴ´�.
                    // Nothing To Do
                }
            }
        }
    }

    //--------------------------------------
    // =ALL, !=ANY
    // (1) �� �÷��� ���, LMST�� ����.
    // (2) �� �÷� �̻��� ���,
    //     predicate ���� ��� not null constraint��
    //     �����ϴ����� �˻�.
    //    .not null constraint�� ���� : LMST�� ����
    //    .not null constraint�� �������� ����
    //        : SORT��忡 storeOnly�� ����
    //--------------------------------------

    if( sIsLMST == ID_TRUE )
    {
        aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
        aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

        aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
        aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_STORENSEARCH_LMST;
    }
    else
    {
        if( sIsStoreOnly == ID_TRUE )
        {
            aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
            aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

            aPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
            aPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY;
        }
        else
        {
            // store and search ����ȭ�� �������� �ʴ´�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::inSubqueryKeyRange( qtcNode        * aCompareNode,
                                 qtcNode        * aSubQNode,
                                 qmoSubqueryType  aSubQType )
{
/***********************************************************************
 *
 * Description : IN���� subquery keyRange ����ȭ �� ����
 *
 *   < IN���� subquery keyRange ����ȭ �� >
 *
 *   FROM T1 WHERE I1 IN ( SELECT A1 FROM T2 );
 *   �� ���� quantify �� �����ڿ� subquery�� �Բ� ����ϴ� ���,
 *   transformNJ ����ȭ ���� ����� �� ������, �̴� ��������� ������ �־�,
 *   T1�� ���ڵ� ���� subquery�� ���ڵ� ������ �ſ� ���� ���, �� ȿ����
 *   �������� ������ �ִ�. �̿� ���� IN subquery keyRange ����ȭ�� T1��
 *   ���ڵ� ���� subquery�� ���ڵ� ������ �ſ� ���� ��쿡 �����Ͽ� ȿ����
 *   ���̰� �ȴ�.
 *
 * Implementation :
 *
 *    subquery�� ���� graph�� �����ϰ� ����, subquery type A, N���� ���,
 *    column cardinality >= subquery target cardinality �̸�,
 *    qmgProjection graph�� IN subquery keyRange����ȭ ���� ����
 *
 ***********************************************************************/

    qmgPROJ       * sPROJ;

    IDU_FIT_POINT_FATAL( "qmoSubquery::inSubqueryKeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );

    //--------------------------------------
    // IN subquery keyRange ����ȭ �� ����
    //--------------------------------------

    //--------------------------------------
    // ���ǰ˻�
    // 1. subquery type A, N
    // 2. IN �񱳿�����
    //--------------------------------------

    if( ( aSubQType == QMO_TYPE_A ) || ( aSubQType == QMO_TYPE_N ) )
    {
        if( aCompareNode->node.module == &mtfEqualAny )
        {
            sPROJ = (qmgPROJ *)(aSubQNode->subquery->myPlan->graph);

            // IN subquery ����ȭ �� ���� ����
            sPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
            sPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_IN_KEYRANGE;
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

    return IDE_SUCCESS;
}


IDE_RC
qmoSubquery::subqueryKeyRange( qtcNode        * aCompareNode,
                               qtcNode        * aSubQNode,
                               qmoSubqueryType  aSubQType )
{
/***********************************************************************
 *
 * Description : subquery keyRange ����ȭ �� ����
 *
 *   < subquery keyRange ����ȭ �� >
 *
 *   one-row one-column ������ subquery�� ���,
 *   subquery�� ���� ������� ���� ������ keyRange�� �����Ѵ�.
 *
 * Implementation :
 *
 *   subquery�� ���� graph�� �����ϰ� ����, subquery type A, N���� ���,
 *   =, >, >=, <, <= �� ���ؼ� subquery keyRange ����ȭ ���� �����ϴµ�,
 *   �� ������, store and search ����ȭ �� flag�� oring �ǵ��� �Ѵ�.
 *   (������, inSubqueryKeyRange()�� ����)
 *
 ***********************************************************************/

    qmgPROJ     * sPROJ;

    IDU_FIT_POINT_FATAL( "qmoSubquery::subqueryKeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSubQNode != NULL );

    //--------------------------------------
    // subquery keyRange ����ȭ �� ����
    //--------------------------------------

    //--------------------------------------
    // ���ǰ˻�
    // 1. subquery type A, N
    // 2. �񱳿����� =, >, >=, <, <=
    //--------------------------------------

    if( ( aSubQType == QMO_TYPE_A ) || ( aSubQType == QMO_TYPE_N ) )
    {
        switch( aCompareNode->node.module->lflag &
                ( MTC_NODE_OPERATOR_MASK | MTC_NODE_GROUP_COMPARISON_MASK ) )
        {
            case ( MTC_NODE_OPERATOR_EQUAL |
                   MTC_NODE_GROUP_COMPARISON_FALSE ) :
            case ( MTC_NODE_OPERATOR_GREATER |
                   MTC_NODE_GROUP_COMPARISON_FALSE ) :
            case ( MTC_NODE_OPERATOR_GREATER_EQUAL |
                   MTC_NODE_GROUP_COMPARISON_FALSE ) :
            case ( MTC_NODE_OPERATOR_LESS |
                   MTC_NODE_GROUP_COMPARISON_FALSE ) :
            case ( MTC_NODE_OPERATOR_LESS_EQUAL |
                   MTC_NODE_GROUP_COMPARISON_FALSE ):
                sPROJ = (qmgPROJ *)(aSubQNode->subquery->myPlan->graph);

            sPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
            sPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_KEYRANGE;
            break;
            default :
                // Nothing To Do
                break;
        }
    }
    else
    {
        // subquery type�� J, JA ���� ���,
        // Nothing To Do
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoSubquery::constantSubquery( qcStatement * aStatement,
                               qtcNode     * aSubQNode )
{
/***********************************************************************
 *
 * Description : Expression�� subquery�� ����
 *               constant subquery ����ȭ �� ����
 *
 *   < constant subquery ����ȭ �� >
 *
 *   one-row one-column ���� subquery�� ���,
 *   subquery�� �Ź� �������� �ʵ���, �ѹ� ����� ����� ������ �ΰ�,
 *   �̸� �̿��Ѵ�.
 *
 * Implementation :
 *
 *     subquery type A, N�� ���,
 *     qmgProjection graph�� constant subquery ����ȭ �� ������ �����Ѵ�.
 *     ���� ����� SORT��忡 store only�� ����.
 *
 ***********************************************************************/

    qmoSubqueryType    sSubQType;
    qmgPROJ          * sPROJ;

    IDU_FIT_POINT_FATAL( "qmoSubquery::constantSubquery::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSubQNode != NULL );

    //--------------------------------------
    // expression�� subquery�� ���� constant subquery ����ȭ �� ����
    //--------------------------------------

    //--------------------------------------
    // subquery node�� ���� subquery type �Ǵ�
    //--------------------------------------

    IDE_TEST( checkSubqueryType( aStatement,
                                 aSubQNode,
                                 &sSubQType ) != IDE_SUCCESS );

    if( ( sSubQType == QMO_TYPE_A ) || ( sSubQType == QMO_TYPE_N ) )
    {

        sPROJ = (qmgPROJ *)(aSubQNode->subquery->myPlan->graph);

        sPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_TIP_MASK;
        sPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;

        sPROJ->subqueryTipFlag &= ~QMG_PROJ_SUBQUERY_STORENSEARCH_MASK;
        sPROJ->subqueryTipFlag |= QMG_PROJ_SUBQUERY_STORENSEARCH_STOREONLY;
    }
    else
    {
        // subquery type�� J, JA���� ���
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSubquery::getColumnCardinality( qcStatement * aStatement,
                                   qtcNode     * aColumnNode,
                                   SDouble     * aCardinality )
{
/***********************************************************************
 *
 * Description :  predicate column�� cardinality�� ���Ѵ�.
 *
 *    store and search, IN subquery keyRange, transform NJ �Ǵܽ�
 *    �ʿ��� predicate column�� cardinality�� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SDouble         sCardinality;
    SDouble         sOneCardinality;
    qtcNode       * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::getColumnCardinality::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aColumnNode != NULL );
    IDE_DASSERT( aCardinality != NULL );

    //--------------------------------------
    // predicate column�� cardinality�� ���Ѵ�.
    //--------------------------------------

    if( ( aColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_LIST )
    {
        sCardinality = 1;
        sNode = (qtcNode *)(aColumnNode->node.arguments);

        while( sNode != NULL )
        {
            if( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
            {
                sOneCardinality =
                    QC_SHARED_TMPLATE(aStatement)->tableMap[sNode->node.table].
                    from->tableRef->statInfo->\
                    colCardInfo[sNode->node.column].columnNDV;
            }
            else
            {
                sOneCardinality = QMO_STAT_COLUMN_NDV;
            }

            sCardinality *= sOneCardinality;

            sNode = (qtcNode *)(sNode->node.next);
        }
    }
    else
    {
        sNode = aColumnNode;

        if( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
        {
            sCardinality =
                QC_SHARED_TMPLATE(aStatement)->tableMap[sNode->node.table].
                from->tableRef->statInfo->\
                colCardInfo[sNode->node.column].columnNDV;
        }
        else
        {
            sCardinality = QMO_STAT_COLUMN_NDV;
        }
    }

    *aCardinality = sCardinality;

    return IDE_SUCCESS;
}

IDE_RC
qmoSubquery::getSubQTargetCardinality( qcStatement * aStatement,
                                       qtcNode     * aSubQNode,
                                       SDouble     * aCardinality )
{
/***********************************************************************
 *
 * Description :  subquery Target column�� cardinality�� ���Ѵ�.
 *
 *    store and search, IN subquery keyRange, transform NJ �Ǵܽ�
 *    �ʿ��� subquery Target cardinality�� ���Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    SDouble         sCardinality;
    SDouble         sOneCardinality;
    qmsQuerySet   * sQuerySet;
    qmsTarget     * sTarget;
    qtcNode       * sNode;

    IDU_FIT_POINT_FATAL( "qmoSubquery::getSubQTargetCardinality::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aSubQNode != NULL );
    IDE_DASSERT( aCardinality != NULL );

    //--------------------------------------
    // subquery target column�� ���� cardinality�� ���Ѵ�.
    //--------------------------------------

    sQuerySet =  ((qmsParseTree *)(aSubQNode->subquery->myPlan->parseTree))->querySet;

    sCardinality = 1;
    sTarget = sQuerySet->target;

    while( sTarget != NULL )
    {
        sNode = sTarget->targetColumn;
        
        // BUG-20272
        if ( sNode->node.module == &mtfDecrypt )
        {
            sNode = (qtcNode*) sNode->node.arguments;
        }
        else
        {
            // nothing to do
        }

        // BUG-43645 dml where clause subquery recursive with
        if ( ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE ) &&
             ( ( QC_SHARED_TMPLATE(aStatement)->tableMap[sNode->node.table].
                 from->tableRef->view == NULL ) &&
               ( QC_SHARED_TMPLATE(aStatement)->tableMap[sNode->node.table].
                 from->tableRef->recursiveView == NULL ) ) )
        {
            // target column�� column�̰�,
            // base table�� ���, ��������� ���� cardinality�� ��´�.
            sOneCardinality =
                QC_SHARED_TMPLATE(aStatement)->tableMap[sNode->node.table].
                from->tableRef->statInfo->\
                colCardInfo[sNode->node.column].columnNDV;
        }
        else
        {
            // target column�� column�� �ƴϰų�,
            // subquery�� view�� ���
            sOneCardinality = QMO_STAT_COLUMN_NDV;
        }

        sCardinality *= sOneCardinality;
        sTarget = sTarget->next;
    }

    *aCardinality = sCardinality;

    return IDE_SUCCESS;
}
