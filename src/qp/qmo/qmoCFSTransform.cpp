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
 * Description : PROJ-2242 Filter Subsumption Transformation
 *
 *       - AND, OR �� ���� TRUE, FALSE predicate ����
 *       - QTC_NODE_JOIN_OPERATOR_EXIST �� ��� ���� ����
 *       - subquery, host variable, GEOMETRY type arguments ����
 *       - __OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION property �� ����
 *
 * ��� ���� :
 *
 * ��� : CFS (Constant Filter Subsumption)
 *
 *****************************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qcgPlan.h>
#include <qcuProperty.h>
#include <qmoCSETransform.h>
#include <qmoCFSTransform.h>
#include <qcg.h> /* TASK-7219 Shard Transformer Refactoring */
#include <sdi.h> /* TASK-7219 Shard Transformer Refactoring */

IDE_RC
qmoCFSTransform::doTransform4NNF( qcStatement  * aStatement,
                                  qmsQuerySet  * aQuerySet )
{
/******************************************************************************
 *
 * PROJ-2242 : ���� NNF ������ ��� �������� ���� CFS (Filter Subsumption) ����
 *
 * Implementation : 1. CFS for where clause predicate
 *                  2. CFS for from tree
 *                  3. CFS for having clause predicate
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCFSTransform::doTransform4NNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    //--------------------------------------
    // CFS �Լ� ����
    //--------------------------------------

    if ( aQuerySet->setOp == QMS_NONE )
    {
        // From on clause predicate ó��
        IDE_TEST( doTransform4From( aStatement, aQuerySet->SFWGH->from )
                  != IDE_SUCCESS );

        // Where clause predicate ó��
        IDE_TEST( doTransform( aStatement, &aQuerySet->SFWGH->where )
                  != IDE_SUCCESS );

        // Having clause predicate ó��
        IDE_TEST( doTransform( aStatement, &aQuerySet->SFWGH->having )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( doTransform4NNF( aStatement, aQuerySet->left ) != IDE_SUCCESS );

        IDE_TEST( doTransform4NNF( aStatement, aQuerySet->right ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCFSTransform::doTransform4From( qcStatement  * aStatement,
                                   qmsFrom      * aFrom )
{
/******************************************************************************
 *
 * PROJ-2242 : From ���� ���� CFS (Constant Filter Subsumption) ����
 *
 * Implementation : From tree �� ��ȸ�ϸ� CFS ����
 *
 ******************************************************************************/

    IDU_FIT_POINT_FATAL( "qmoCFSTransform::doTransform4From::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aFrom      != NULL );

    //--------------------------------------
    // CFS ����
    //--------------------------------------

    IDE_TEST( doTransform( aStatement, &aFrom->onCondition ) != IDE_SUCCESS );

    if( aFrom->joinType != QMS_NO_JOIN ) // INNER, OUTER JOIN
    {
        IDE_TEST( doTransform4From( aStatement, aFrom->left ) != IDE_SUCCESS );
        IDE_TEST( doTransform4From( aStatement, aFrom->right ) != IDE_SUCCESS );
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
qmoCFSTransform::doTransform( qcStatement  * aStatement,
                              qtcNode     ** aNode )
{
/******************************************************************************
 *
 * PROJ-2242 : NNF �� CFS (Constant Filter subsumption) ����
 *             ��, �������� oracle style outer mask '(+)' �� ������ ��� ����
 *
 * Implementation :
 *
 *       1. ORACLE style outer mask ���� �˻�
 *       2. NNF �� constant filter subsumption ����
 *       3. Envieronment �� ���
 *
 ******************************************************************************/

    idBool sExistOuter;

    IDU_FIT_POINT_FATAL( "qmoCFSTransform::doTransform::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // CFS ����
    //--------------------------------------

    if( *aNode != NULL && QCU_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION == 1 )
    {
        IDE_TEST( qmoCSETransform::doCheckOuter( *aNode, &sExistOuter )
                  != IDE_SUCCESS );

        if( sExistOuter == ID_FALSE )
        {
            IDE_TEST( constantFilterSubsumption( aStatement, aNode, ID_TRUE )
                      != IDE_SUCCESS );
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
            PLAN_PROPERTY_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCFSTransform::constantFilterSubsumption( qcStatement * aStatement,
                                            qtcNode    ** aNode,
                                            idBool        aIsRoot )
{
/******************************************************************************
 *
 * PROJ-2242 : NNF �� ���� constant filter subsumption �� �����Ѵ�.
 *
 * Implementation : AND, OR �� ���� ������ ���� �����Ѵ�.
 *                  ����� CSE �������Ķ� T^T, F^F, TvT, FvF �񱳴� ������ ��
 *                  ������ ������ ������ �����ϵ��� ��� ����ϱ�� �Ѵ�.
 *
 *      1. OR  : T ^ F = F, T ^ P = P, F ^ P = F, F ^ F = F, T ^ T = T
 *      2. AND : T v F = T, T v P = T, F v P = P, F v F = F, T v T = T
 *         (�ֻ��� level �� F ^ P �� ��� subsumption �������� �ʴ´�.)
 *
 *      ex) NNF : Predicate list pointer (<-aNode)
 *                 |
 *                OR
 *                 |
 *                P1 (<-sTarget) -- AND (<-sCompare:���) -- P2 -- AND --...
 *                                   |
 *                                  Pn ( ������ �� ���̸� level up) ...
 *
 *      cf) �� ���(sResult)�� ������ ���� �����Ѵ�.
 *        - QMO_CFS_COMPARE_RESULT_BOTH : sTarget, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_WIN  : sTarget ����, sCompare ����
 *        - QMO_CFS_COMPARE_RESULT_LOSE : sTarget ����, sCompare ����
 *
 ******************************************************************************/

    qtcNode            ** sTarget;
    qtcNode            ** sCompare;
    qtcNode             * sTargetPrev;
    qtcNode             * sComparePrev;
    qtcNode             * sTemp;
    mtdBooleanType        sTargetValue;
    mtdBooleanType        sCompareValue;
    qmoCFSCompareResult   sResult = QMO_CFS_COMPARE_RESULT_BOTH;

    IDU_FIT_POINT_FATAL( "qmoCFSTransform::constantFilterSubsumption::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // CFS ����
    //--------------------------------------

    if( ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_OR ||
        ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_AND )
    {
        sTargetPrev = NULL;
        sTarget = (qtcNode **)(&(*aNode)->node.arguments);

        // sTarget�� ���� �켱 ����, ���� next ��ȸ�ϸ� sCompare ���� ����
        if( *sTarget != NULL )
        {
            sTemp = (qtcNode *)((*sTarget)->node.next);

            IDE_TEST( constantFilterSubsumption( aStatement,
                                                 sTarget,
                                                 ID_FALSE )
                      != IDE_SUCCESS );

            (*sTarget)->node.next = &(sTemp->node);
        }
        else
        {
            // Nothing to do.
        }

        // sTarget ��ȸ
        while( *sTarget != NULL )
        {
            sResult = QMO_CFS_COMPARE_RESULT_BOTH;
            sTargetValue = MTD_BOOLEAN_NULL;

            if( (*sTarget)->node.module == &qtc::valueModule &&
                ( (*sTarget)->node.lflag & QTC_NODE_PROC_VAR_MASK )
                == QTC_NODE_PROC_VAR_ABSENT )
            {
                sTargetValue = 
                    *( (mtdBooleanType *)
                     ( (SChar*)QTC_STMT_TUPLE( aStatement, *sTarget )->row +
                      QTC_STMT_COLUMN( aStatement, *sTarget )->column.offset ) );
            }
            else
            {
                // Nothing to do.
            }

            sComparePrev = (qtcNode *)(&(*sTarget)->node);
            sCompare = (qtcNode **)(&(*sTarget)->node.next);

            // sCompare ��ȸ
            while( *sCompare != NULL )
            {
                sTemp = (qtcNode *)((*sCompare)->node.next);

                IDE_TEST( constantFilterSubsumption( aStatement,
                                                     sCompare,
                                                     ID_FALSE )
                          != IDE_SUCCESS );

                (*sCompare)->node.next = &(sTemp->node);

                sResult = QMO_CFS_COMPARE_RESULT_BOTH;
                sCompareValue = MTD_BOOLEAN_NULL;

                if( (*sCompare)->node.module == &qtc::valueModule &&
                    ( (*sCompare)->node.lflag & QTC_NODE_PROC_VAR_MASK )
                    == QTC_NODE_PROC_VAR_ABSENT )
                {
                    sCompareValue =
                        *( (mtdBooleanType *)
                         ( (SChar*)QTC_STMT_TUPLE( aStatement, *sCompare )->row +
                          QTC_STMT_COLUMN( aStatement, *sCompare )->column.offset ) );
                }
                else
                {
                    // Nothing to do.
                }

                // �� ����� ��
                if( ( (*aNode)->node.lflag & MTC_NODE_OPERATOR_MASK )
                    == MTC_NODE_OPERATOR_AND )
                {
                    sResult = 
                        ( sTargetValue == MTD_BOOLEAN_TRUE && sCompareValue == MTD_BOOLEAN_TRUE) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_TRUE && sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_LOSE:
                        ( sTargetValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_LOSE:
                        ( sTargetValue == MTD_BOOLEAN_FALSE && sCompareValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_FALSE && sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_FALSE && aIsRoot == ID_TRUE ) ? QMO_CFS_COMPARE_RESULT_BOTH:
                        ( sTargetValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sCompareValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sCompareValue == MTD_BOOLEAN_FALSE && aIsRoot == ID_TRUE) ? QMO_CFS_COMPARE_RESULT_BOTH:
                        ( sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_LOSE: QMO_CFS_COMPARE_RESULT_BOTH;
                }
                else // OR
                {
                    sResult = 
                        ( sTargetValue == MTD_BOOLEAN_TRUE && sCompareValue == MTD_BOOLEAN_TRUE) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_TRUE && sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_FALSE && sCompareValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_LOSE:
                        ( sTargetValue == MTD_BOOLEAN_FALSE && sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_WIN:
                        ( sTargetValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_LOSE:
                        ( sCompareValue == MTD_BOOLEAN_TRUE ) ? QMO_CFS_COMPARE_RESULT_LOSE:
                        ( sCompareValue == MTD_BOOLEAN_FALSE ) ? QMO_CFS_COMPARE_RESULT_WIN: QMO_CFS_COMPARE_RESULT_BOTH;
                }

                if( sResult == QMO_CFS_COMPARE_RESULT_WIN )
                {
                    // sCompare ����
                    sTemp = *sCompare;
                    sComparePrev->node.next = (*sCompare)->node.next;
                    sCompare = (qtcNode **)(&sComparePrev->node.next);

                    /* TASK-7219 Shard Transformer Refactoring */
                    if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                                        SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
                         == ID_TRUE )
                    {
                        IDE_TEST( sdi::preAnalyzeSubquery( aStatement,
                                                           sTemp,
                                                           QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    IDE_TEST( QC_QMP_MEM( aStatement )->free( sTemp )
                              != IDE_SUCCESS );
                }
                else if( sResult == QMO_CFS_COMPARE_RESULT_LOSE )
                {
                    // sTarget ����
                    break;
                }
                else
                {
                    // sTarget, sCompare ����
                    sComparePrev = (qtcNode *)(&(*sCompare)->node);
                    sCompare = (qtcNode **)(&(*sCompare)->node.next);
                }
            }

            if( sResult == QMO_CFS_COMPARE_RESULT_LOSE )
            {
                // sTarget ����
                sTemp = *sTarget;
                if( sTargetPrev == NULL )
                {
                    (*aNode)->node.arguments = (*sTarget)->node.next;
                    sTarget = (qtcNode **)(&(*aNode)->node.arguments);
                }
                else
                {
                    sTargetPrev->node.next = (*sTarget)->node.next;
                    sTarget = (qtcNode **)(&sTargetPrev->node.next);
                }

                /* TASK-7219 Shard Transformer Refactoring */
                if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                                    SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
                         == ID_TRUE )
                {
                    IDE_TEST( sdi::preAnalyzeSubquery( aStatement,
                                                       sTemp,
                                                       QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }

                IDE_TEST( QC_QMP_MEM( aStatement )->free( sTemp )
                          != IDE_SUCCESS );
            }
            else
            {
                sTargetPrev = (qtcNode *)(&(*sTarget)->node);
                sTarget = (qtcNode **)(&(*sTarget)->node.next);
            }
        }

        // �ϳ��� ���ڸ� ���� logical operator ó��
        if( (*aNode)->node.arguments->next == NULL )
        {
            sTemp = *aNode;
            *aNode = (qtcNode *)((*aNode)->node.arguments);

            /* TASK-7219 Shard Transformer Refactoring */
            if ( SDI_CHECK_QUERYSET_LIST_STATE( aStatement->mShardQuerySetList,
                                                SDI_QUERYSET_LIST_STATE_DUMMY_ANALYZE )
                         == ID_TRUE )
            {
                IDE_TEST( sdi::preAnalyzeSubquery( aStatement,
                                                   sTemp,
                                                   QCG_GET_SESSION_SHARD_META_NUMBER( aStatement ) )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            IDE_TEST( QC_QMP_MEM( aStatement )->free( sTemp ) != IDE_SUCCESS );
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
