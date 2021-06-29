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
 * Description : PROJ-2242 Common Subexpression Elimination Transformation
 *
 *       - QTC_NODE_JOIN_OPERATOR_EXIST �� ��� ���� ����
 *       - subquery, host variable, GEOMETRY type arguments ����
 *       - __OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION property �� ����
 *
 * ��� ���� :
 *
 *            1. Idempotent law
 *             - A and A = A
 *             - A or A = A
 *            2. Absorption law
 *             - A and (A or B) = A
 *             - A or (A and B) = A
 *
 *
 * ��� : CSE (Common Subexpression Elimination)
 *        NNF (Not Normal Form)
 *
 *****************************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qcgPlan.h>
#include <qmoCSETransform.h>
#include <qcg.h>

IDE_RC
qmoCSETransform::doTransform4NNF( qcStatement  * aStatement,
                                  qmsQuerySet  * aQuerySet,
                                  idBool         aIsNNF )
{
/******************************************************************************
 *
 * PROJ-2242 : Normalize ���� ���� NNF ������ ��� �������� ����
 *             CSE (common subexpression elimination) ����
 *
 * Implementation : 1. CSE for where clause predicate
 *                  2. CSE for from tree
 *                  3. CSE for having clause predicate
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCSETransform::doTransform4NNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    //--------------------------------------
    // �� �������� ���� CSE �Լ� ȣ��
    //--------------------------------------

    if ( aQuerySet->setOp == QMS_NONE )
    {
        // From on clause predicate ó��
        IDE_TEST( doTransform4From( aStatement, aQuerySet->SFWGH->from, aIsNNF )
                  != IDE_SUCCESS );

        // Where clause predicate ó��
        IDE_TEST( doTransform( aStatement, &aQuerySet->SFWGH->where, aIsNNF )
                  != IDE_SUCCESS );

        // Having clause predicate ó��
        IDE_TEST( doTransform( aStatement, &aQuerySet->SFWGH->having, aIsNNF )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( doTransform4NNF( aStatement, aQuerySet->left, aIsNNF )
                  != IDE_SUCCESS );

        IDE_TEST( doTransform4NNF( aStatement, aQuerySet->right, aIsNNF )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCSETransform::doTransform4From( qcStatement  * aStatement,
                                   qmsFrom      * aFrom,
                                   idBool         aIsNNF )
{
/******************************************************************************
 *
 * PROJ-2242 : From ���� ���� CSE (common subexpression elimination) ����
 *
 * Implementation :
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCSETransform::doTransform4From::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom      != NULL );

    //--------------------------------------
    // CSE �Լ� ȣ��
    //--------------------------------------

    IDE_TEST( doTransform( aStatement, &aFrom->onCondition, aIsNNF )
              != IDE_SUCCESS );

    if( aFrom->joinType != QMS_NO_JOIN ) // INNER, OUTER JOIN
    {
        IDE_TEST( doTransform4From( aStatement, aFrom->left, aIsNNF )
                  != IDE_SUCCESS);
        IDE_TEST( doTransform4From( aStatement, aFrom->right, aIsNNF )
                  != IDE_SUCCESS);
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
qmoCSETransform::doTransform( qcStatement  * aStatement,
                              qtcNode     ** aNode,
                              idBool         aIsNNF,
                              idBool         aIsWhere,
                              qmsHints     * aHints )
{
/******************************************************************************
 *
 * PROJ-2242 : CSE (common subexpression elimination) ����
 *             ��, �������� oracle style outer mask '(+)' �� ������ ��� ����
 *
 * Implementation :
 *
 *          1. ORACLE style outer mask ���� �˻�
 *          2. NNF
 *          2.1. Depth ���̱�
 *          2.2. Idempotent law or absorption law ����
 *          2.3. �ϳ��� ���ڸ� ���� logical operator node ����
 *          3. Normal form
 *          3.1. Idempotent law or absorption law ����
 *
 ******************************************************************************/

    idBool sExistOuter;

    idBool              sIsTransform    = ID_TRUE;
    qmsHints          * sHints;

    IDU_FIT_POINT_FATAL( "qmoCSETransform::doTransform::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    sHints = aHints;

    //--------------------------------------
    // CSE ����
    //--------------------------------------
    if ( *aNode == NULL )
    {
        sIsTransform = ID_FALSE;
    }
    else
    {
        // hint �켱
        if ( sHints != NULL )
        {
            if ( ( sHints->partialCSE == ID_TRUE ) &&
                 ( aIsWhere == ID_TRUE ) )
            {
                sIsTransform = ID_FALSE;
            }
        }

        if( sIsTransform == ID_TRUE )
        {
            switch ( QCG_GET_SESSION_ELIMINATE_COMMON_SUBEXPRESSION( aStatement ) )
            {
                case 0:
                    sIsTransform = ID_FALSE;
                    break;
                case 1:
                    break;
                case 2:
                    if ( aIsWhere == ID_TRUE )
                    {
                        sIsTransform = ID_FALSE;
                    }
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }

    if ( sIsTransform == ID_TRUE )
    {
        IDE_TEST( doCheckOuter( *aNode, &sExistOuter ) != IDE_SUCCESS );

        if( sExistOuter == ID_FALSE )
        {
            if( aIsNNF == ID_TRUE )
            {
                IDE_TEST( unnestingAndOr4NNF( aStatement, *aNode )
                          != IDE_SUCCESS );
                IDE_TEST( idempotentAndAbsorption( aStatement, *aNode )
                          != IDE_SUCCESS );
                IDE_TEST( removeLogicalNode4NNF( aStatement, aNode )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( idempotentAndAbsorption( aStatement, *aNode )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // �������� oracle style outer mask '(+)' �� ������ ���
            // Nothing to do.
        }
    }
    else
    {
        // Nothing to do.
    }

    // environment�� ���
    qcgPlan::registerPlanProperty(
            aStatement,
            PLAN_PROPERTY_OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCSETransform::doCheckOuter( qtcNode  * aNode,
                               idBool   * aExistOuter )
{
/******************************************************************************
 *
 * PROJ-2242 : ORACLE style outer mask �˻�
 *
 * Implementation : ��带 ��ȸ�ϸ� QTC_NODE_JOIN_OPERATOR_MASK �˻�
 *                  ( parsing �������� setting )
 *
 ******************************************************************************/

    qtcNode * sNode;
    qtcNode * sNext;
    idBool    sExistOuter;

    IDU_FIT_POINT_FATAL( "qmoCSETransform::doCheckOuter::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode       != NULL );
    IDE_DASSERT( aExistOuter != NULL );

    //--------------------------------------
    // Outer mask �˻�
    //--------------------------------------

    sNode = aNode;

    do
    {
        sNext = (qtcNode *)(sNode->node.next);

        if( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_OR ||
            ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_AND )
        {
            sNode = (qtcNode *)(sNode->node.arguments);

            IDE_TEST( doCheckOuter( sNode, & sExistOuter )
                      != IDE_SUCCESS );
        }
        else
        {
            if( ( sNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                sExistOuter = ID_TRUE;
            }
            else
            {
                sExistOuter = ID_FALSE;
            }
        }

        if( sExistOuter == ID_TRUE )
        {
            break;
        }
        else
        {
            sNode = sNext;
        }

    } while( sNode != NULL );

    *aExistOuter = sExistOuter;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoCSETransform::unnestingAndOr4NNF( qcStatement * aStatement,
                                     qtcNode     * aNode )
{
/******************************************************************************
 *
 * PROJ-2242 : NNF �� ���� ��ø�� AND,OR ��� ����
 *
 * Implementation : Parent level, child level ��尡
 *                  AND �Ǵ� OR ���� ������ ���
 *                  child level �� logical ��带 ����
 *
 * Parent level :   OR (<- aNode)
 *                   |
 *  Child level :    P --- OR (<- sNode : ����) --- P (<-sNext) --- ...
 *                          |
 *                          P1 (<- sArg) --- ... --- Pn (<-sArgTail)
 *
 ******************************************************************************/

    qtcNode * sNode;
    qtcNode * sArg;
    qtcNode * sPrev;
    qtcNode * sArgTail = NULL;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDU_FIT_POINT_FATAL( "qmoCSETransform::unnestingAndOr4NNF::__FT__" );

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // AND, OR ��� unnesting ����
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_OR ||
        ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_AND )
    {
        sPrev = NULL;
        sNode = (qtcNode *)(aNode->node.arguments);

        while( sNode != NULL )
        {
            IDE_TEST( unnestingAndOr4NNF( aStatement, sNode )
                      != IDE_SUCCESS );

            if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK ) ==
                ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK ) )
            {
                // Tail ����
                sArg = (qtcNode *)(sNode->node.arguments);
                while( sArg != NULL )
                {
                    sArgTail = sArg;
                    sArg = (qtcNode *)(sArg->node.next);
                }

                if( sArgTail != NULL )
                {
                    sArgTail->node.next = sNode->node.next;
                }
                else
                {
                    // Nothing to do.
                }

                // Head ����
                if( sPrev == NULL )
                {
                    aNode->node.arguments = sNode->node.arguments;
                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sNode )
                              != IDE_SUCCESS );
                    sNode = (qtcNode *)(aNode->node.arguments);
                }
                else
                {
                    sPrev->node.next = sNode->node.arguments;
                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sNode )
                              != IDE_SUCCESS );
                    sNode = (qtcNode *)(sPrev->node.next);
                }
            }
            else
            {
                sPrev = sNode;
                sNode = (qtcNode *)(sNode->node.next);
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
qmoCSETransform::idempotentAndAbsorption( qcStatement * aStatement,
                                          qtcNode     * aNode )
{
/******************************************************************************
 *
 * PROJ-2242 : �� ��带 ���Ͽ� �Ʒ� �� ��Ģ�� �����Ѵ�.
 *             NNF �� ��� ������ AND, OR ��尡 �ϳ��� ���ڸ� ������ �ִ�.
 *             (���� �� �Լ� �������� �ݵ�� removeLogicalNode4NNF �� �����ؾ� ��)
 *
 *          1. Idempotent law : A and A = A, A or A = A
 *          2. Absorption law : A and (A or B) = A, A or (A and B) = A
 *
 * ex) NNF : OR (<-aNode)
 *           |
 *           P1 (<-sTarget) -- AND (<-sCompare:���) -- P2 -- AND --...
 *                             |                              |
 *                             Pn ( ��� : �� �� ���� ����)   ...
 *
 * ex) CNF : AND (<-aNode)
 *           |
 *           OR (<-sTarget) -- OR (<-sCompare) -- OR --...
 *           |                      |             |
 *           Pn                     Pm            ...
 *
 *      cf) �� ���(sResult)�� ������ ���� �����Ѵ�.
 *        - QMO_CFS_COMPARE_RESULT_BOTH : sTarget, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_WIN  : sTarget ����, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_LOSE : sTarget ����, sCompare ����
 *
 *****************************************************************************/

    qtcNode   * sTarget;
    qtcNode   * sCompare;
    qtcNode   * sTargetPrev;
    qtcNode   * sComparePrev;

    qmoCSECompareResult sResult = QMO_CSE_COMPARE_RESULT_BOTH;

    IDU_FIT_POINT_FATAL( "qmoCSETransform::idempotentAndAbsorption::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // Idempotent, absorption law ����
    //--------------------------------------

    if( ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_OR ||
        ( aNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_AND )
    {
        sTargetPrev = NULL;
        sTarget = (qtcNode *)(aNode->node.arguments);

        // sTarget�� ���� �켱 ����, ���� next ��ȸ�ϸ� sCompare ���� ����
        if( sTarget != NULL )
        {
            IDE_TEST( idempotentAndAbsorption( aStatement, sTarget )
                      != IDE_SUCCESS );
        }

        while( sTarget != NULL )
        {
            sComparePrev = sTarget;
            sCompare = (qtcNode *)(sTarget->node.next);

            while( sCompare != NULL )
            {
                sResult = QMO_CSE_COMPARE_RESULT_BOTH;
                IDE_TEST( idempotentAndAbsorption( aStatement, sCompare )
                          != IDE_SUCCESS );

                // �� ����� ��
                IDE_TEST( compareNode( aStatement,
                                       sTarget,
                                       sCompare,
                                       &sResult )
                          != IDE_SUCCESS );

                if( sResult == QMO_CSE_COMPARE_RESULT_WIN )
                {
                    sComparePrev->node.next = sCompare->node.next;

                    // BUGBUG : sCompare argument list free
                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sCompare )
                              != IDE_SUCCESS );
                    sCompare = (qtcNode *)(sComparePrev->node.next);
                }
                else if( sResult == QMO_CSE_COMPARE_RESULT_LOSE )
                {
                    break;
                }
                else
                {
                    sComparePrev = sCompare;
                    sCompare = (qtcNode *)(sCompare->node.next);
                }
            }

            if( sResult == QMO_CSE_COMPARE_RESULT_LOSE )
            {
                if( sTargetPrev == NULL )
                {
                    aNode->node.arguments = sTarget->node.next;

                    // BUGBUG : sTarget argument list free
                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sTarget )
                              != IDE_SUCCESS );
                    sTarget = (qtcNode *)(aNode->node.arguments);
                }
                else
                {
                    sTargetPrev->node.next = sTarget->node.next;

                    // BUGBUG : sTarget argument list free
                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sTarget )
                              != IDE_SUCCESS );
                    sTarget = (qtcNode *)(sTargetPrev->node.next);
                }
            }
            else
            {
                sTargetPrev = sTarget;
                sTarget = (qtcNode *)(sTarget->node.next);
            }

            sResult = QMO_CSE_COMPARE_RESULT_BOTH;
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
qmoCSETransform::compareNode( qcStatement         * aStatement,
                              qtcNode             * aTarget,
                              qtcNode             * aCompare,
                              qmoCSECompareResult * aResult )
{
/******************************************************************************
 *
 * PROJ-2242 : �� ��带 ���Ͽ� ���� ������ ��带 ����� ��ȯ�Ѵ�.
 *
 * Implementation : ����� ������ �����¿� ���� ������ ���� ����ȴ�.
 *                  ( LO : AND, OR ���, OP: �� �� ��� )
 *
 *               1. OP vs OP -> Idempotent law ����
 *               2. LO vs OP -> Absorption law ����
 *               3. OP vs LO -> Absorption law ����
 *               4. LO vs LO -> Absorption law ����
 *
 *      cf) �� ���(sResult)�� ������ ���� �����Ѵ�.
 *        - QMO_CFS_COMPARE_RESULT_BOTH : sTarget, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_WIN  : sTarget ����, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_LOSE : sTarget ����, sCompare ����
 *
 *****************************************************************************/

    qtcNode   * sTarget;
    qtcNode   * sTargetArg;
    qtcNode   * sCompare;
    qtcNode   * sCompareArg;
    UInt        sTargetArgCnt;
    UInt        sCompareArgCnt;
    UInt        sArgMatchCnt;
    idBool      sIsEqual;

    qmoCSECompareResult sResult;

    IDU_FIT_POINT_FATAL( "qmoCSETransform::compareNode::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTarget    != NULL );
    IDE_DASSERT( aCompare   != NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sTargetArgCnt = 0;
    sCompareArgCnt = 0;
    sArgMatchCnt = 0;
    sResult = QMO_CSE_COMPARE_RESULT_BOTH;

    sTarget  = aTarget;
    sCompare = aCompare;

    //--------------------------------------
    // target node �� compare node �� �� 
    //--------------------------------------

    // BUG-35040 : fix �� if ���� ����
    if( ( sTarget->lflag  & QTC_NODE_SUBQUERY_MASK )
        == QTC_NODE_SUBQUERY_EXIST ||
        ( sCompare->lflag & QTC_NODE_SUBQUERY_MASK )
        == QTC_NODE_SUBQUERY_EXIST )
    {
        // �� ��� �� �ϳ��� subquery ��带 ���ڷ� ���� ���
        sResult = QMO_CSE_COMPARE_RESULT_BOTH;
    }
    else
    {
        if( ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
            != MTC_NODE_OPERATOR_OR &&
            ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
            != MTC_NODE_OPERATOR_AND &&
            ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
            != MTC_NODE_OPERATOR_OR &&
            ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
            != MTC_NODE_OPERATOR_AND )
        {
            // sTarget, sCompare ��� �� ������(OR, AND)�� �ƴ�
            // target �� compare ��
            sIsEqual = ID_FALSE;

            IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                   sTarget,
                                                   sCompare,
                                                   & sIsEqual )
                      != IDE_SUCCESS )

            sResult = ( sIsEqual == ID_TRUE ) ?
                      QMO_CSE_COMPARE_RESULT_WIN: sResult;
        }
        else if( ( ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_OR ||
                   ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_AND )
                 &&
                 ( ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
                   != MTC_NODE_OPERATOR_OR &&
                   ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
                   != MTC_NODE_OPERATOR_AND ) )
        {
            // sTarget �� �� ������(OR, AND)
            sTargetArg = (qtcNode *)(sTarget->node.arguments);

            while( sTargetArg != NULL )
            {
                // target arguments �� compare ��
                sIsEqual = ID_FALSE;
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sTargetArg,
                                                       sCompare,
                                                       & sIsEqual )
                          != IDE_SUCCESS )
                    if( sIsEqual == ID_TRUE )
                    {
                        sResult = QMO_CSE_COMPARE_RESULT_LOSE;
                        break;
                    }
                sTargetArg = (qtcNode *)(sTargetArg->node.next);
            }
        }
        else if( ( ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
                   != MTC_NODE_OPERATOR_OR &&
                   ( sTarget->node.lflag & MTC_NODE_OPERATOR_MASK )
                   != MTC_NODE_OPERATOR_AND )
                 &&
                 ( ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_OR ||
                   ( sCompare->node.lflag & MTC_NODE_OPERATOR_MASK )
                   == MTC_NODE_OPERATOR_AND ) )
        {
            // sCompare �� �� ������(OR, AND)
            sCompareArg = (qtcNode *)(sCompare->node.arguments);

            while( sCompareArg != NULL )
            {
                // target �� compare arguments ��
                sIsEqual = ID_FALSE;
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sTarget,
                                                       sCompareArg,
                                                       & sIsEqual )
                          != IDE_SUCCESS )
                if( sIsEqual == ID_TRUE )
                {
                    sResult = QMO_CSE_COMPARE_RESULT_WIN;
                    break;
                }
                sCompareArg = (qtcNode *)(sCompareArg->node.next);
            }
        }
        else
        {   // sTarget, sCompare ��� �� ������(OR, AND)

            // sTarget, sCompare arguments ���� ȹ��
            sTargetArg = (qtcNode *)(sTarget->node.arguments);
            sCompareArg = (qtcNode *)(sCompare->node.arguments);

            while( sTargetArg != NULL )
            {
                sTargetArgCnt++;
                sTargetArg = (qtcNode *)(sTargetArg->node.next);
            }

            while( sCompareArg != NULL )
            {
                sCompareArgCnt++;
                sCompareArg = (qtcNode *)(sCompareArg->node.next);
            }

            // sTargetArg, sCompareArg ��
            sTargetArg = (qtcNode *)(sTarget->node.arguments);

            while( sTargetArg != NULL )
            {
                sCompareArg = (qtcNode *)(sCompare->node.arguments);

                while( sCompareArg != NULL )
                {
                    sIsEqual = ID_FALSE;

                    IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                           sTargetArg,
                                                           sCompareArg,
                                                           & sIsEqual )
                              != IDE_SUCCESS )
                    if( sIsEqual == ID_TRUE )
                    {
                        sArgMatchCnt++;
                        break;
                    }
                    sCompareArg = (qtcNode *)(sCompareArg->node.next);
                }
                sTargetArg = (qtcNode *)(sTargetArg->node.next);
            }

            sResult =
                ( sArgMatchCnt == sTargetArgCnt )  ? QMO_CSE_COMPARE_RESULT_WIN:
                ( sArgMatchCnt == sCompareArgCnt ) ? QMO_CSE_COMPARE_RESULT_LOSE:
                QMO_CSE_COMPARE_RESULT_BOTH;
        }
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCSETransform::removeLogicalNode4NNF( qcStatement * aStatement,
                                        qtcNode    ** aNode )
{
/******************************************************************************
 *
 * PROJ-2242 : NNF �� ���� �ϳ��� argument �� ���� logical operator ����
 *
 * Implementation : �ϳ��� argument �� ���� logical ����� ���ڸ� level up
 *
 * Predicate list : where (<- aNode)
 *                    |
 *                   OR (<- sNode)
 *                    |
 *                   P1 --- AND (<- sArg : ����) --- P2 (<- sNext)-- ...
 *                           |
 *                           P (���� level �� ����ø�)
 *
 ******************************************************************************/

    qtcNode  * sNode;
    qtcNode  * sNext;
    qtcNode ** sArg;

    IDU_FIT_POINT_FATAL( "qmoCSETransform::removeLogicalNode4NNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    sNode = *aNode;

    //--------------------------------------
    // ��ø�� logical node �� ����
    //--------------------------------------

    if( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_OR ||
        ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_AND )
    {
        sArg = (qtcNode **)(&sNode->node.arguments);

        while( *sArg != NULL )
        {
            sNext = (qtcNode *)((*sArg)->node.next);

            IDE_TEST( removeLogicalNode4NNF( aStatement, sArg )
                      != IDE_SUCCESS );

            (*sArg)->node.next = &(sNext->node);
            sArg = (qtcNode **)(&(*sArg)->node.next);
        }

        if( sNode->node.arguments->next == NULL )
        {
            *aNode = (qtcNode *)(sNode->node.arguments);
            IDE_TEST( QC_QMP_MEM( aStatement )->free( sNode )
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
