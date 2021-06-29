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
 * $Id: qmoListTransform.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description : BUG-36438 LIST Transformation
 *
 * ex) (t1.i1, t1.i2) = (t2.i1, t2.i2)  => t1.i1=t2.i1 AND t1.i2=t2.i2
 *     (t1.i1, t2.i1) = (1,1)           => t1.i1=1 AND t1.i2=1
 *     (t1.i1, t1.i2) != (t2.i1, t2.i2) => t1.i1!=t2.i1 OR t2.i1!=t2.i2
 *     (t1.i1, t2.i1) != (1,1)          => t1.i1!=1 OR t2.i1!=1
 *
 * ��� ���� :
 *
 *****************************************************************************/

#include <idl.h>
#include <qtc.h>
#include <qcgPlan.h>
#include <qcuProperty.h>
#include <qmoCSETransform.h>
#include <qmoListTransform.h>

extern mtfModule mtfAnd;
extern mtfModule mtfOr;
extern mtfModule mtfEqual;
extern mtfModule mtfNotEqual;
extern mtfModule mtfList;

IDE_RC qmoListTransform::doTransform( qcStatement * aStatement,
                                      qmsQuerySet * aQuerySet )
{
/******************************************************************************
 *
 * Description : qmsQuerySet �� ��� �������� ���� ����
 *
 * Implementation : QCU_OPTIMIZER_LIST_TRANSFORMATION property �� �����ϸ�
 *                  ���� �׸� ���� ��ȯ
 *
 *               - for from tree
 *               - for where clause predicate
 *               - for having clause predicate
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoListTransform::doTransform::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    //--------------------------------------
    // �������� ���� ��ȯ �Լ� ȣ��
    //--------------------------------------

    if ( QCU_OPTIMIZER_LIST_TRANSFORMATION == 1 )
    {
        if ( aQuerySet->setOp == QMS_NONE )
        {
            IDE_TEST( doTransform4From( aStatement,
                                        aQuerySet->SFWGH->from )
                      != IDE_SUCCESS );

            IDE_TEST( listTransform( aStatement,
                                     & aQuerySet->SFWGH->where )
                      != IDE_SUCCESS );

            IDE_TEST( listTransform( aStatement,
                                     & aQuerySet->SFWGH->having )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( doTransform( aStatement,
                                   aQuerySet->left )
                      != IDE_SUCCESS );

            IDE_TEST( doTransform( aStatement,
                                   aQuerySet->right )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    // environment�� ���
    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_LIST_TRANSFORMATION );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoListTransform::doTransform4From( qcStatement * aStatement,
                                           qmsFrom     * aFrom )
{
/******************************************************************************
 *
 * Description : From ���� ���� ����
 *
 * Implementation : From tree ��ȸ
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoListTransform::doTransform4From::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom      != NULL );

    //--------------------------------------
    // FROM tree ��ȸ
    //--------------------------------------

    if ( aFrom->joinType != QMS_NO_JOIN ) // INNER, OUTER JOIN
    {
        IDE_TEST( listTransform( aStatement,
                                 & aFrom->onCondition )
                  != IDE_SUCCESS );

        IDE_TEST( doTransform4From( aStatement,
                                    aFrom->left  )
                  != IDE_SUCCESS );

        IDE_TEST( doTransform4From( aStatement,
                                    aFrom->right )
                  != IDE_SUCCESS );
    }
    else
    {
        // QMS_NO_JOIN �� ��� onCondition �� �������� �ʴ´�.
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoListTransform::listTransform( qcStatement  * aStatement,
                                        qtcNode     ** aNode )
{
/******************************************************************************
 *
 * Description : LIST ��ȯ�� �����Ѵ�.
 *
 * Implementation :
 *
 *     Logical operator ������ [NOT] EQUAL �����ڿ� ���� ����ȴ�.
 *
 *     [Before] AND/OR(sParent)
 *               |
 *               ... --- =(sCompare) --- ...
 *                       |
 *                       LIST(sLeftList) ------ LIST (sRightList)
 *                       |                      |
 *                       t1.i1(sLeftArg) - ...  t2.i1(sRightArg) - ...
 *
 *     [After]  AND/OR
 *               |
 *               ... --- AND --- ...
 *                       |
 *                       = ------------- = ------------- ...
 *                       |               |
 *                       t1.i1 - t2.i1   ...
 *
 ******************************************************************************/

    qtcNode ** sTarget  = NULL;
    qtcNode  * sPrev    = NULL;
    qtcNode  * sNewPred = NULL;

    IDU_FIT_POINT_FATAL( "qmoListTransform::listTransform::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // ��� ��ȸ�ϸ鼭 ��ȯ
    //--------------------------------------

    if ( *aNode != NULL )
    {
        if ( ( ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_OR ) ||
             ( ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_AND ) )
        {
            // Logical operator �̸� argument �� next ��ȸ
            for ( sPrev = NULL, sTarget = (qtcNode **)&((*aNode)->node.arguments);
                  *sTarget != NULL;
                  sPrev = *sTarget, sTarget = (qtcNode **)&((*sTarget)->node.next) )
            {
                IDE_TEST( listTransform( aStatement, sTarget ) != IDE_SUCCESS );

                // Target �� ����Ǿ��� �� �����Ƿ� ������� ����
                if ( sPrev == NULL )
                {
                    (*aNode)->node.arguments = (mtcNode *)(*sTarget);
                }
                else
                {
                    sPrev->node.next = (mtcNode *)(*sTarget);
                }
            }
        }
        else
        {
            // Logical operator �ƴϸ� ��ȯ
            sTarget = aNode;

            IDE_TEST( makePredicateList( aStatement,
                                         *sTarget,
                                         &sNewPred )
                      != IDE_SUCCESS );

            // ������� ����
            if ( sNewPred != NULL )
            {
                *aNode = sNewPred;
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

IDE_RC qmoListTransform::makePredicateList( qcStatement  * aStatement,
                                            qtcNode      * aCompareNode,
                                            qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description : Compare predicate ���κ��� predicate list �� �����Ѵ�.
 *
 * Implementation :
 *
 * ex) (t1.i1, t1.i2) = (t2.i1, t2.i2) => t1.i1 = t2.i1 AND t1.i2 = t2.i2
 *
 *     [Before] =(sCompare)
 *              |
 *              LIST(sLeftList) ------ LIST (sRightList)
 *              |                      |
 *              t1.i1(sLeftArg) - ...  t2.i1(sRightArg) - ...
 *
 *     [After]  AND
 *              |
 *              = ------------- = ------------- ...
 *              |               |
 *              t1.i1 - t2.i1   ...
 *
 ***********************************************************************/

    qtcNode * sLogicalNode[2];
    qtcNode * sFirst    = NULL;
    qtcNode * sLast     = NULL;
    qtcNode * sNewNode  = NULL;
    mtcNode * sLeftArg  = NULL;
    mtcNode * sRightArg = NULL;
    mtcNode * sLeftArgNext  = NULL;
    mtcNode * sRightArgNext = NULL;
    mtcNode * sCompareNext  = NULL;
    idBool    sResult = ID_FALSE;
    UInt      sCount = 0;
    qcNamePosition   sEmptyPosition;

    IDU_FIT_POINT_FATAL( "qmoListTransform::makePredicateList::__FT__" );

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aCompareNode != NULL );

    IDE_TEST( checkCondition( aCompareNode,
                              &sResult )
              != IDE_SUCCESS );

    if ( sResult == ID_TRUE )
    {
        sCompareNext = aCompareNode->node.next;

        // Predicate list ����
        for ( sLeftArg  = aCompareNode->node.arguments->arguments,
              sRightArg = aCompareNode->node.arguments->next->arguments;
              ( sLeftArg != NULL ) && ( sRightArg != NULL );
              sLeftArg = sLeftArgNext, sRightArg = sRightArgNext, sCount++ )
        {
            sLeftArgNext  = sLeftArg->next;
            sRightArgNext = sRightArg->next;

            // Predicate ����
            IDE_TEST( makePredicate( aStatement,
                                     aCompareNode,
                                     (qtcNode*)sLeftArg,
                                     (qtcNode*)sRightArg,
                                     &sNewNode )
                      != IDE_SUCCESS );

            // ������ predicate�� �����Ѵ�.
            if ( sFirst == NULL )
            {
                sFirst = sLast = sNewNode;
            }
            else
            {
                sLast->node.next = (mtcNode *)sNewNode;
                sLast = (qtcNode*)sLast->node.next;
            }
        }

        // Logical operator ����
        SET_EMPTY_POSITION( sEmptyPosition );

        IDE_TEST( qtc::makeNode( aStatement,
                                 sLogicalNode,
                                 &sEmptyPosition,
                                 ( aCompareNode->node.module == &mtfEqual ) ? &mtfAnd: &mtfOr )
                  != IDE_SUCCESS );

        sLogicalNode[0]->node.arguments = (mtcNode *)sFirst;

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sLogicalNode[0] )
                  != IDE_SUCCESS );

        sLogicalNode[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
        sLogicalNode[0]->node.lflag |= sCount;
        sLogicalNode[0]->node.next = sCompareNext;

        *aResult = sLogicalNode[0];
    }
    else
    {
        *aResult = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoListTransform::checkCondition( qtcNode     * aNode,
                                         idBool      * aResult )
{
/******************************************************************************
 *
 * Description : ��ȯ ������ ���ǿ� ���� ����� ��ȯ�Ѵ�.
 *
 * Implementation : ��ȯ ������ ������ ������ ����.
 *
 *             - ORACLE style outer mask �������� �ʾƾ� ��
 *             - Subquery �� �������� �ʾƾ� ��
 *             - [NOT] EQUAL ������
 *             - ���ڴ� ��� LIST ������
 *             - Predicate dependency �� QMO_LIST_TRANSFORM_DEPENDENCY_COUNT �̻�
 *             - LIST ������ ������ QMO_LIST_TRANSFORM_ARGUMENTS_COUNT ������ ���
 *
 *             ex) (t1.i1, t1.i2) = (t2.i1, t2.i2)
 *                 (t1.i1, t2.i2) = (t3.i1, t3.i2)
 *                 (t1.i1, t2.i1) = (1, 1)
 *
 *****************************************************************************/

    qtcNode * sListNode   = NULL;
    qtcNode * sNode       = NULL;
    UInt      sArgCnt     = 0;
    idBool    sExistOuter = ID_FALSE;
    idBool    sIsResult   = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoListTransform::checkCondition::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // ���� �˻�
    //--------------------------------------

    IDE_TEST( qmoCSETransform::doCheckOuter( aNode,
                                             & sExistOuter )
              != IDE_SUCCESS );

    if ( ( sExistOuter == ID_FALSE ) &&
         ( QTC_HAVE_SUBQUERY( aNode ) == ID_FALSE ) &&
         ( ( aNode->node.module == &mtfEqual ) || ( aNode->node.module == &mtfNotEqual ) ) &&
         ( aNode->node.arguments->module == &mtfList ) &&
         ( aNode->node.arguments->next->module == &mtfList ) &&
         ( aNode->depInfo.depCount >= QMO_LIST_TRANSFORM_DEPENDENCY_COUNT ) )
    {
        sListNode = (qtcNode*)aNode->node.arguments;

        for ( sNode = (qtcNode*)sListNode->node.arguments, sArgCnt = 0;
              sNode != NULL;
              sNode = (qtcNode*)sNode->node.next, sArgCnt++ );

        if ( sArgCnt > QMO_LIST_TRANSFORM_ARGUMENTS_COUNT )
        {
            sIsResult = ID_FALSE;
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        sIsResult = ID_FALSE;
    }

    *aResult = sIsResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoListTransform::makePredicate( qcStatement  * aStatement,
                                        qtcNode      * aPredicate,
                                        qtcNode      * aOperand1,
                                        qtcNode      * aOperand2,
                                        qtcNode     ** aResult )
{
/***********************************************************************
 *
 * Description : �ϳ��� predicate �� �����Ѵ�.
 *
 * Implementation :
 *
 *                =
 *                |
 *                t1.i1 - t2.i1
 *
 ***********************************************************************/

    qtcNode        * sPredicate[2];
    qcNamePosition   sEmptyPosition;

    IDU_FIT_POINT_FATAL( "qmoListTransform::makePredicate::__FT__" );

    aOperand1->node.next = (mtcNode *)aOperand2;
    aOperand2->node.next = NULL;

    SET_EMPTY_POSITION( sEmptyPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sPredicate,
                             &sEmptyPosition,
                             (mtfModule*)aPredicate->node.module )
              != IDE_SUCCESS );

    sPredicate[0]->node.arguments = (mtcNode *)aOperand1;
    sPredicate[0]->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
    sPredicate[0]->node.lflag |= 2;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sPredicate[0] )
              != IDE_SUCCESS );

    *aResult = sPredicate[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
