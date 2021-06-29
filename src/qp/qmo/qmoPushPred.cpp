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
 * $Id: qmoPushPred.cpp 23857 2008-03-19 02:36:53Z sungminee $
 **********************************************************************/

#include <idl.h>
#include <qtc.h>
#include <qmoPushPred.h>
#include <qmsParseTree.h>
#include <qmv.h>
#include <qmvQTC.h>
#include <qmsDefaultExpr.h>
#include <qcg.h>

extern mtfModule mtfRowNumber;
extern mtfModule mtfRowNumberLimit;

IDE_RC
qmoPushPred::doPushDownViewPredicate( qcStatement  * aStatement,
                                      qmsParseTree * aViewParseTree,
                                      qmsQuerySet  * aViewQuerySet,
                                      UShort         aViewTupleId,
                                      qmsSFWGH     * aSFWGH,
                                      qmsFrom      * aFrom,
                                      qmoPredicate * aPredicate,
                                      idBool       * aIsPushed,
                                      idBool       * aIsPushedAll,
                                      idBool       * aRemainPushedPredicate )
{
/***********************************************************************
 *
 * Description :
 *     BUG-18367 view push selection
 *     view�� ���� one table predicate�� push selection�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool    sCanPushDown;
    UInt      sPushedRankTargetOrder;
    qtcNode * sPushedRankLimit;

    IDU_FIT_POINT_FATAL( "qmoPushPred::doPushDownViewPredicate::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aViewQuerySet != NULL );
    IDE_DASSERT( aSFWGH != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aIsPushed != NULL );
    IDE_DASSERT( aIsPushedAll != NULL );

    // recursive view�� pushdown�� �� ����.
    if ( ( aViewQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
         == QMV_QUERYSET_RECURSIVE_VIEW_TOP )
    {
        *aIsPushed    = ID_FALSE;
        *aIsPushedAll = ID_FALSE;

        IDE_CONT( NORMAL_EXIT );
    }
    else
    {
        // Nothing to do.
    }

    if ( aViewQuerySet->setOp == QMS_NONE )
    {
        sCanPushDown = ID_TRUE;

        //---------------------------------------------------
        // Push Selection �����ص� �Ǵ� �������� �˻�
        // ( query set ������ �˻� )
        //---------------------------------------------------
        IDE_TEST( canPushSelectionQuerySet( aViewParseTree,
                                            aViewQuerySet,
                                            & sCanPushDown,
                                            aRemainPushedPredicate )
                  != IDE_SUCCESS );

        //---------------------------------------------------
        // Push selection �ص� �Ǵ� predicate���� �˻�
        //---------------------------------------------------
        if ( sCanPushDown == ID_TRUE )
        {
            IDE_TEST( canPushDownPredicate( aStatement,
                                            aViewQuerySet,
                                            aViewQuerySet->target,
                                            aViewTupleId,
                                            NULL, /* aOuterQuery */
                                            aPredicate->node,
                                            ID_FALSE,  // next�� �˻����� �ʴ´�.
                                            & sCanPushDown )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }

        if ( sCanPushDown == ID_TRUE )
        {
            //---------------------------------------------------
            // Pushdown �ص� �Ǵ� ���, ������ predicate�� pushdown
            //---------------------------------------------------
            IDE_TEST( pushDownPredicate( aStatement,
                                         aViewQuerySet,
                                         aViewTupleId,
                                         aSFWGH,
                                         aFrom,
                                         aPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. */
        }

        /******************************************************
         * BUG-40354 pushed rank
         ******************************************************/

        if ( sCanPushDown == ID_FALSE )
        {
            sCanPushDown = ID_TRUE;

            /* push������ predicate����, view���� Ȯ���Ѵ�. */
            IDE_TEST( isPushableRankPred( aStatement,
                                          aViewParseTree,
                                          aViewQuerySet,
                                          aViewTupleId,
                                          aPredicate->node,
                                          & sCanPushDown,
                                          & sPushedRankTargetOrder,
                                          & sPushedRankLimit )
                      != IDE_SUCCESS );

            if ( sCanPushDown == ID_TRUE )
            {
                IDE_TEST( pushDownRankPredicate( aStatement,
                                                 aViewQuerySet,
                                                 sPushedRankTargetOrder,
                                                 sPushedRankLimit )
                          != IDE_SUCCESS );

                /* rank pred�� ���ܳ��ƾ� �Ѵ�. */
                *aRemainPushedPredicate = ID_TRUE;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* Nothing to do. */
        }

        if ( sCanPushDown == ID_TRUE )
        {
            // �ϳ��� �����ϸ� ����
            *aIsPushed  = ID_TRUE;
        }
        else
        {
            // �ϳ��� �����ϸ� ����
            *aIsPushedAll = ID_FALSE;
        }
    }
    else
    {
        if ( aViewQuerySet->left != NULL )
        {
            IDE_TEST( doPushDownViewPredicate( aStatement,
                                               aViewParseTree,
                                               aViewQuerySet->left,
                                               aViewTupleId,
                                               aSFWGH,
                                               aFrom,
                                               aPredicate,
                                               aIsPushed,
                                               aIsPushedAll,
                                               aRemainPushedPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }

        if ( aViewQuerySet->right != NULL )
        {
            IDE_TEST( doPushDownViewPredicate( aStatement,
                                               aViewParseTree,
                                               aViewQuerySet->right,
                                               aViewTupleId,
                                               aSFWGH,
                                               aFrom,
                                               aPredicate,
                                               aIsPushed,
                                               aIsPushedAll,
                                               aRemainPushedPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // nothing to do
        }
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::canPushSelectionQuerySet( qmsParseTree * aViewParseTree,
                                       qmsQuerySet  * aViewQuerySet,
                                       idBool       * aCanPushDown,
                                       idBool       * aRemainPushedPred )
{
/***********************************************************************
 *
 * Description :
 *     Push selection �ص� �Ǵ� �������� query set ������ �˻�
 *
 * Implementation :
 *    (1) View�� limit ���� �����ϴ� ���,
 *        Predicate Pushdown ���� ( ��� Ʋ�� )
 *    (2) View�� analytic function�� �����ϴ� ���,
 *        Predicate Pushdown ���� ( Pushdown �ϸ� ��� Ʋ�� )
 *    (3) View�� row num�� �����ϴ� ���,
 *        Predicate Pushdown ���� ( BUG-20953 : Pushdown �ϸ� ��� Ʋ�� )
 *    (4) View�� target list�� aggregate function�� �����ϴ� ���,
 *        Predicate�� pushdown �ص� ������ ������ predicate�� �״�� ���ܵξ�� ��
 *       ( BUG-31399 : Predicate pushdown ��, �������� predicate�� �����ϸ�
 *         ��� Ʋ�� )
 *    (5) View�� group by extension�� �ִ� ���
 *        Predicate Pushdown ���� ( ��� Ʋ�� )
 *    (6) View�� Grouping Sets Transformed View�� ���
 *        Predicate Pushdown ����
 *    (7) View�� loop ���� �����ϴ� ���
 *        Predicate Pushdown ����
 *
 ***********************************************************************/

    qmsTarget         * sTarget;
    qmsConcatElement  * sElement;

    IDU_FIT_POINT_FATAL( "qmoPushPred::canPushSelectionQuerySet::__FT__" );

    if ( aViewParseTree->limit != NULL )
    {
        //---------------------------------------
        // (1) View�� limit ���� �����ϴ� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    /* BUG-36580 supported TOP */
    if ( aViewQuerySet->SFWGH->top != NULL )
    {
        //---------------------------------------
        // (1-1) View�� top�� �����ϴ� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    if ( aViewQuerySet->analyticFuncList != NULL )
    {
        //---------------------------------------
        // (2) View�� analytic function�� �����ϴ� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    if( aViewQuerySet->SFWGH->rownum != NULL )
    {
        //---------------------------------------
        // (3) view�� row num�� �����ϴ� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    for ( sTarget  = aViewQuerySet->target;
          sTarget != NULL;
          sTarget  = sTarget->next )
    {
        if ( QTC_HAVE_AGGREGATE( sTarget->targetColumn) == ID_TRUE )
        {
            //---------------------------------------
            // (4) View�� target list�� aggregate function�� �����ϴ� ���
            //---------------------------------------
            *aRemainPushedPred = ID_TRUE;
            break;
        }
        else
        {
            // nothing to do
        }
    }

    // BUG-37047 group by extension�� row�� �����ϴ� ��찡 �־�
    // predicate�� group by �Ʒ��� ���� �� ����.
    for ( sElement  = aViewQuerySet->SFWGH->group;
          sElement != NULL;
          sElement  = sElement->next )
    {
        if ( sElement->type != QMS_GROUPBY_NORMAL )
        {
            //---------------------------------------
            // (5) View�� group by extension�� �ִ� ���
            //---------------------------------------
            *aCanPushDown = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    // PROJ-2415 Grouping Sets Clause
    if ( ( aViewQuerySet->SFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK ) !=
         QMV_SFWGH_GBGS_TRANSFORM_NONE )
    {
        //---------------------------------------
        // (6) view�� Grouping Sets Transformed View �� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    if ( aViewParseTree->loopNode != NULL )
    {
        //---------------------------------------
        // (7) View�� loop ���� �����ϴ� ���
        //---------------------------------------
        *aCanPushDown = ID_FALSE;
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPushPred::canPushDownPredicate( qcStatement  * aStatement,
                                   qmsQuerySet  * aViewQuerySet,
                                   qmsTarget    * aViewTarget,
                                   UShort         aViewTupleId,
                                   qmsSFWGH     * aOuterQuery,
                                   qtcNode      * aNode,
                                   idBool         aContainRootsNext,
                                   idBool       * aCanPushDown )
{
/***********************************************************************
 *
 * Description :
 *     Push selection �ص� �Ǵ� predicate���� �˻�
 *     BUG-18367 view push selection
 *
 * Implementation :
 *     predicate�� ���� view column�� �����Ǵ� view�� target column��
 *     ���� ���� �÷��̾�� �Ѵ�.
 *
 *     aCanPushSelection�� �ʱⰪ�� ID_TRUE�� �����Ǿ��ִ�.
 *
 ***********************************************************************/

    qmsTarget  * sViewTarget;
    qtcNode    * sTargetColumn;
    UInt         sColumnId;
    UInt         sColumnOrder;
    UInt         sTargetOrder;
    UInt         sOrgDataTypeId;
    UInt         sViewDataTypeId;

    /* TASK-7219 Non-shard DML */
    idBool       sIsOutRefTupleFound = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoPushPred::canPushDownPredicate::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aViewTarget != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aCanPushDown != NULL );

    /* TASK-7219 Non-shard DML */
    if ( qtc::getPosNextBitSet( & aNode->depInfo,
                                qtc::getPosFirstBitSet(
                                    & aNode->depInfo ) )
         != QTC_DEPENDENCIES_END )
    {
        if ( ( aNode->depInfo.depCount + aViewQuerySet->depInfo.depCount ) > QC_MAX_REF_TABLE_CNT )
        {
            *aCanPushDown = ID_FALSE;
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        /* Nothing to do. */
    }

    if ( *aCanPushDown == ID_TRUE )
    {
        if ( qtc::dependencyEqual( & aNode->depInfo,
                                   & qtc::zeroDependencies ) == ID_FALSE )
        {
            if ( aNode->node.table == aViewTupleId )
            {
                //---------------------------------------------------
                // PROJ-1653 Outer Join Operator (+)
                //
                // Predicate �� Outer Join Operator �� ���Ǿ�����
                // PushDown Predicate �� �������� �ʴ´�.
                //---------------------------------------------------
                if ( ( aNode->lflag & QTC_NODE_JOIN_OPERATOR_MASK )
                        == QTC_NODE_JOIN_OPERATOR_EXIST )
                {
                    *aCanPushDown = ID_FALSE;
                }
                else
                {
                    sOrgDataTypeId =
                        QTC_STMT_COLUMN(aStatement, aNode)->type.dataTypeId;
                    sColumnId =
                        QTC_STMT_COLUMN(aStatement, aNode)->column.id;
                    sColumnOrder = sColumnId & SMI_COLUMN_ID_MASK;

                    sTargetOrder = 0;
                    for ( sViewTarget  = aViewTarget;
                          sViewTarget != NULL;
                          sViewTarget  = sViewTarget->next )
                    {
                        if ( sTargetOrder == sColumnOrder )
                        {
                            break;
                        }
                        else
                        {
                            sTargetOrder++;
                        }
                    }

                    IDE_TEST_RAISE( sViewTarget == NULL, ERR_COLUMN_NOT_FOUND );

                    sTargetColumn = sViewTarget->targetColumn;

                    sViewDataTypeId =
                        QTC_STMT_COLUMN(aStatement, sTargetColumn)->type.dataTypeId;

                    /* BUG-33843
                    PushDownPredicate �� �Ҷ����� Ÿ�� �������� ���� �ٸ��� �����Ǹ� �ȵȴ�.
                    �÷��� Ÿ���� ���� �ٸ��� �ٸ� �������� ������ �ȴ�.
                    ���� �÷��� Ÿ���� ���Ͽ� Ȯ���Ѵ�. */
                    if ( sOrgDataTypeId != sViewDataTypeId )
                    {
                        *aCanPushDown = ID_FALSE;
                    }
                    else
                    {
                        // nothing to do
                    }

                    // BUG-19179
                    if ( sTargetColumn->node.module == & qtc::passModule )
                    {
                        sTargetColumn = (qtcNode*) sTargetColumn->node.arguments;
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    // BUG-19756
                    if ( ( ( sTargetColumn->lflag & QTC_NODE_AGGREGATE_MASK )
                           == QTC_NODE_AGGREGATE_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_AGGREGATE2_MASK )
                           == QTC_NODE_AGGREGATE2_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_SUBQUERY_MASK )
                           == QTC_NODE_SUBQUERY_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                           == QTC_NODE_PROC_FUNCTION_TRUE )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_VAR_FUNCTION_MASK )
                           == QTC_NODE_VAR_FUNCTION_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_PRIOR_MASK )
                           == QTC_NODE_PRIOR_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_LEVEL_MASK )
                           == QTC_NODE_LEVEL_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_ROWNUM_MASK )
                           == QTC_NODE_ROWNUM_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_ISLEAF_MASK )
                           == QTC_NODE_ISLEAF_EXIST )
                         ||
                         ( ( sTargetColumn->lflag & QTC_NODE_COLUMN_RID_MASK )
                           == QTC_NODE_COLUMN_RID_EXIST ) /* BUG-41218 */
                         ||
                         ( ( sTargetColumn->node.lflag & MTC_NODE_BIND_MASK )
                           == MTC_NODE_BIND_EXIST )
                         )
                    {
                        *aCanPushDown = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            else
            {
                /* TASK-7219 Non-shard DML
                 * Shard view���� column to bind push down �� LOB type�� column�� push �Ǵ� ���� �����Ѵ�.
                 */
                if ( aOuterQuery != NULL )
                {
                    if ( aNode->node.module == &qtc::columnModule )
                    {
                        /* TASK-7219 Non-shard DML */
                        IDE_TEST ( findOutRefTuple( aNode->node.table,
                                                    aOuterQuery->from,
                                                    &sIsOutRefTupleFound )
                                   != IDE_SUCCESS );

                        if ( sIsOutRefTupleFound == ID_TRUE )
                        {
                            if ( ( aNode->lflag & QTC_NODE_LOB_COLUMN_MASK )
                                 == QTC_NODE_LOB_COLUMN_EXIST )
                            {
                                *aCanPushDown = ID_FALSE;
                            }
                            else
                            {
                                /* Nothing to do. */
                            }
                        }
                        else
                        {
                            // raise unexpected error
                            /* Nothing to do. */
                        }
                    }
                    else
                    {
                        /* Nothing to do. */
                    }
                }
                else
                {
                    /* Nothing to do. */
                }
            }

            if ( aNode->node.arguments != NULL )
            {
                IDE_TEST( canPushDownPredicate( aStatement,
                                                aViewQuerySet,
                                                aViewTarget,
                                                aViewTupleId,
                                                aOuterQuery,
                                                (qtcNode*) aNode->node.arguments,
                                                ID_TRUE,
                                                aCanPushDown )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // ����϶�

            // Nothing to do.
        }

        if ( (aNode->node.next != NULL) &&
             (aContainRootsNext == ID_TRUE) )
        {
            IDE_TEST( canPushDownPredicate( aStatement,
                                            aViewQuerySet,
                                            aViewTarget,
                                            aViewTupleId,
                                            aOuterQuery,
                                            (qtcNode*) aNode->node.next,
                                            ID_TRUE,
                                            aCanPushDown )
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

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::canPushDownPredicate",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoPushPred::findOutRefTuple( UShort    aTupleId,
                                     qmsFrom * aOutRefFrom,
                                     idBool  * aIsFound )
{
    qmsFrom * sFrom = NULL;

    *aIsFound = ID_FALSE;

    for ( sFrom  = aOutRefFrom;
          sFrom != NULL;
          sFrom  = sFrom->next )
    {
        IDE_TEST(findOutRefTupleForFromTree( aTupleId,
                                             sFrom,
                                             aIsFound )
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoPushPred::findOutRefTupleForFromTree( UShort    aTupleId,
                                                qmsFrom * aFrom,
                                                idBool  * aIsFound )
{
    if ( aFrom != NULL )
    {
        if ( aFrom->joinType == QMS_NO_JOIN )
        {
            IDE_TEST_RAISE( aFrom->tableRef == NULL, ERR_NULL_TABLEREF );

            if ( aTupleId == aFrom->tableRef->table  )
            {
                *aIsFound = ID_TRUE;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            findOutRefTupleForFromTree( aTupleId,
                                        aFrom->left,
                                        aIsFound );

            findOutRefTupleForFromTree( aTupleId,
                                        aFrom->right,
                                        aIsFound );
        }
    }
    else
    {
        /* Nothing to do. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_TABLEREF )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::findOutRefTupleForFromTree",
                                  "tableRef is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::pushDownPredicate( qcStatement  * aStatement,
                                qmsQuerySet  * aViewQuerySet,
                                UShort         aViewTupleId,
                                qmsSFWGH     * aSFWGH,
                                qmsFrom      * aFrom,
                                qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     BUG-18367 view push selection
 *     push predicate�� �����Ͽ� view�� where���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcNamePosition   sNullPosition;
    qtcNode        * sNode;
    qtcNode        * sCompareNode;
    qtcNode        * sAndNode[2];
    qtcNode        * sResultNode[2];
    qtcNode        * sArgNode1[2];
    qtcNode        * sArgNode2[2];

    IDU_FIT_POINT_FATAL( "qmoPushPred::pushDownPredicate::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aViewQuerySet != NULL );
    IDE_DASSERT( aSFWGH != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //---------------------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------------------

    SET_EMPTY_POSITION( sNullPosition );

    //---------------------------------------------------
    // Node ����
    //---------------------------------------------------

    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     aPredicate->node,
                                     & sNode,
                                     ID_FALSE,  // root�� next�� �������� �ʴ´�.
                                     ID_TRUE,   // conversion�� ���´�.
                                     ID_TRUE,   // constant node���� �����Ѵ�.
                                     ID_FALSE ) // constant node�� �������� �ʴ´�.
              != IDE_SUCCESS );

    //---------------------------------------------------
    // view�� predicate�� view ������ table predicate���� ��ȯ
    //---------------------------------------------------

    IDE_TEST( changeViewPredIntoTablePred( aStatement,
                                           aViewQuerySet->target,
                                           aViewTupleId,
                                           sNode,
                                           QMO_CHANGE_COLUMN_NAME_DISABLE, /* TASK-7219 */
                                           ID_FALSE ) // next�� �ٲ��� �ʴ´�.
             != IDE_SUCCESS );

    /* TASK-7219 Non-shard DML */
    if ( (aPredicate->flag & QMO_PRED_PUSH_PRED_HINT_MASK)
         == QMO_PRED_PUSH_PRED_HINT_FALSE )
    {
        // Hint�� ���� push�� predicate�� �ƴϸ鼭
        if ( qtc::getPosNextBitSet( & aPredicate->node->depInfo,
                                    qtc::getPosFirstBitSet(
                                        & aPredicate->node->depInfo ) )
             != QTC_DEPENDENCIES_END )
        {
            // �ܺ� ������ �����ϴ� predicate �� ���

            // ������(�纻-view's where node) predicate node ���� ǥ��
            setForcePushedPredForShardView( &sNode->node );

            // ������(����-my relation own predicate) predicate ���� ǥ��
            aPredicate->flag  &= QMO_PRED_PUSHED_FORCE_PRED_MASK;
            aPredicate->flag  |= QMO_PRED_PUSHED_FORCE_PRED_TRUE;
        }
        else
        {
            /* Nothing to do. */
        }
    }
    else
    {
        /* Nothing to do. */
    }

    //---------------------------------------------------
    // Node Estimate
    //---------------------------------------------------

    aViewQuerySet->processPhase = QMS_OPTIMIZE_PUSH_DOWN_PRED;

    IDE_TEST( qtc::estimate( sNode,
                             QC_SHARED_TMPLATE(aStatement),
                             NULL,
                             aViewQuerySet,
                             aViewQuerySet->SFWGH,
                             NULL)
              != IDE_SUCCESS);

    //---------------------------------------------------
    // view�� where ���� ����
    //---------------------------------------------------

    // To Fix BUG-9645
    if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_OR )
    {
        if ( sNode->node.arguments->next == NULL )
        {
            sCompareNode = (qtcNode *)sNode->node.arguments;
        }
        else
        {
            sCompareNode = NULL;
        }
    }
    else
    {
        sCompareNode = sNode;
    }

    // PR-12955
    // �̹� where���� AND�� ����� ��� ��ܽ��� AND���� ��ſ�
    // �ϳ��� AND node�� arguments->next...->next�θ� �������
    // CNF only�� �Ǻ��� �� �ֵ��� ��.
    // CompareNode�� NULL �� ��� OR ��忡 2�� �̻��� argument�� ����.
    if ( aViewQuerySet->SFWGH->where != NULL )
    {
        if ( (aViewQuerySet->SFWGH->where->node.lflag &
              ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ))
             == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) &&
             ( sCompareNode != NULL ) )
        {
            IDE_TEST( qtc::makeNode( aStatement,
                                     sAndNode,
                                     & sNullPosition,
                                     (const UChar*)"AND",
                                     3 )
                      != IDE_SUCCESS );

            sAndNode[0]->node.arguments       = (mtcNode *)sCompareNode;
            sAndNode[0]->node.arguments->next = NULL;

            sArgNode1[0] = sAndNode[0];
            sArgNode1[1] = (qtcNode*)(sAndNode[0]->node.arguments);

            sArgNode2[0] = aViewQuerySet->SFWGH->where;

            IDE_TEST( qtc::addAndArgument( aStatement,
                                           sResultNode,
                                           sArgNode1,
                                           sArgNode2 )
                      != IDE_SUCCESS );
            aViewQuerySet->SFWGH->where = sResultNode[0];
        }
        else
        {
            // �Ʒ� else���� ������ ����.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sAndNode,
                                     & sNullPosition,
                                     (const UChar*)"AND",
                                     3 )
                      != IDE_SUCCESS );

            sAndNode[0]->node.arguments = (mtcNode *)sNode;
            sAndNode[0]->node.arguments->next =
                (mtcNode *) aViewQuerySet->SFWGH->where;
            aViewQuerySet->SFWGH->where = sAndNode[0];
        }
    }
    else
    {
        // �� else���� ������ ����
        IDE_TEST( qtc::makeNode( aStatement,
                                 sAndNode,
                                 & sNullPosition,
                                 (const UChar*)"AND",
                                 3 )
                  != IDE_SUCCESS );

        sAndNode[0]->node.arguments = (mtcNode *)sNode;
        sAndNode[0]->node.arguments->next =
            (mtcNode *) aViewQuerySet->SFWGH->where;
        aViewQuerySet->SFWGH->where = sAndNode[0];
    }

    //---------------------------------------------------
    // To Fix BUG-10577
    // ���� ������ AND ����� estimate �� Push Selection Predicate��
    // column ����� table ID �������� ���� dependencies�� �ݿ��Ǿ�� ��
    //     - column node�� ���� ��� : column node�� �� ���� node����
    //                                 dependencies���� ORing ��
    //---------------------------------------------------

    // PR-12955, AND ��尡 �ֻ����� �ƴ� �� ����. sAndNode[0]�� where���� ��ġ
    IDE_TEST(qtc::estimateNodeWithoutArgument( aStatement,
                                               aViewQuerySet->SFWGH->where )
             != IDE_SUCCESS);

    /* BUG-42661 A function base index is not wokring view */
    if ( QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) == 1 )
    {
        IDE_TEST( qmsDefaultExpr::applyFunctionBasedIndex( aStatement,
                                                           aViewQuerySet->SFWGH->where,
                                                           aViewQuerySet->SFWGH->from,
                                                           &( aViewQuerySet->SFWGH->where ) )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    //---------------------------------------------------
    // outer column�� ������ predicate�� ���� ���,
    // outer query�� ������ �ش�.
    // PROJ-1495
    //---------------------------------------------------

    if( qtc::getPosNextBitSet(
            & aPredicate->node->depInfo,
            qtc::getPosFirstBitSet( & aPredicate->node->depInfo ) )
        == QTC_DEPENDENCIES_END )
    {
        // outer column�� ���Ե��� ���� ���,
        // Nothing To Do
    }
    else
    {
        // outer column�� ���Ե� ���,
        aViewQuerySet->SFWGH->outerQuery = aSFWGH;
        aViewQuerySet->SFWGH->outerFrom = aFrom;
    }

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    // push_pred �� ����� ��쿡�� outerColumns�� �߰��� �־�� �Ѵ�.
    IDE_TEST( qmvQTC::setOuterColumns( aStatement,
                                       & aFrom->depInfo,
                                       aViewQuerySet->SFWGH,
                                       aPredicate->node )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::changeViewPredIntoTablePred( qcStatement  * aStatement,
                                          qmsTarget    * aViewTarget,
                                          UShort         aViewTupleId,
                                          qtcNode      * aNode,
                                          UShort         aChangeName, /* TASK-7219 */
                                          idBool         aContainRootsNext )
{
/***********************************************************************
 *
 * Description :
 *     BUG-19756 view predicate pushdown
 *     view�� predicate�� view ������ table predicate���� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmsTarget  * sViewTarget;
    qtcNode    * sTargetColumn;
    UInt         sColumnId;
    UInt         sColumnOrder;
    UInt         sTargetOrder;

    IDU_FIT_POINT_FATAL( "qmoPushPred::changeViewPredIntoTablePred::__FT__" );

    //---------------------------------------------------
    // ���ռ� �˻�
    //---------------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aViewTarget != NULL );
    IDE_DASSERT( aNode != NULL );

    //---------------------------------------------------
    // push selection �˻�
    //---------------------------------------------------

    if ( qtc::dependencyEqual( & aNode->depInfo,
                               & qtc::zeroDependencies ) == ID_FALSE )
    {
        if ( aNode->node.table == aViewTupleId )
        {
            sColumnId =
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aNode->node.table].
                columns[aNode->node.column].column.id;
            sColumnOrder = sColumnId & SMI_COLUMN_ID_MASK;

            sTargetOrder = 0;
            for ( sViewTarget = aViewTarget;
                  sViewTarget != NULL;
                  sViewTarget = sViewTarget->next )
            {
                if ( sTargetOrder == sColumnOrder )
                {
                    break;
                }
                else
                {
                    sTargetOrder++;
                }
            }

            IDE_TEST_RAISE( sViewTarget == NULL, ERR_COLUMN_NOT_FOUND );

            sTargetColumn = sViewTarget->targetColumn;

            // BUG-19179
            if ( sTargetColumn->node.module == & qtc::passModule )
            {
                sTargetColumn = (qtcNode*) sTargetColumn->node.arguments;
            }
            else
            {
                // Nothing to do.
            }

            // BUG-19756
            if ( sTargetColumn->node.module == & qtc::columnModule )
            {
                // column�� ���
                IDE_TEST( transformToTargetColumn( aNode,
                                                   aChangeName,
                                                   sTargetColumn )
                          != IDE_SUCCESS );
            }
            else if ( sTargetColumn->node.module == & qtc::valueModule )
            {
                // value�� ���
                IDE_TEST( transformToTargetValue( aNode,
                                                  sTargetColumn )
                             != IDE_SUCCESS );
            }
            else
            {
                // expression�� ���
                IDE_TEST( transformToTargetExpression( aStatement,
                                                       aNode,
                                                       sTargetColumn )
                             != IDE_SUCCESS );
            }
        }
        else
        {
            // Nothing to do.
        }

        if ( aNode->node.arguments != NULL )
        {
            IDE_TEST( changeViewPredIntoTablePred( aStatement,
                                                   aViewTarget,
                                                   aViewTupleId,
                                                   (qtcNode*) aNode->node.arguments,
                                                   aChangeName,
                                                   ID_TRUE )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // ����϶�

        // Nothing to do.
    }

    if ( ( aNode->node.next != NULL ) &&
         ( aContainRootsNext == ID_TRUE ) )
    {
        IDE_TEST( changeViewPredIntoTablePred( aStatement,
                                               aViewTarget,
                                               aViewTupleId,
                                               (qtcNode*) aNode->node.next,
                                               aChangeName,
                                               ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COLUMN_NOT_FOUND )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::changeViewPredIntoTablePred",
                                  "Column not found" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::transformToTargetColumn( qtcNode      * aNode,
                                      UShort         aChangeName, /* TASK-7219 */
                                      qtcNode      * aTargetColumn )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode  sOrgNode;

    IDU_FIT_POINT_FATAL( "qmoPushPred::transformToTargetColumn::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aTargetColumn != NULL );

    //------------------------------------------
    // view �÷��� transform ����
    //------------------------------------------

    // ��带 ����Ѵ�.
    idlOS::memcpy( & sOrgNode, aNode, ID_SIZEOF( qtcNode ) );

    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aNode, aTargetColumn, ID_SIZEOF( qtcNode ) );

    // conversion ��带 �ű��.
    aNode->node.conversion = sOrgNode.node.conversion;
    aNode->node.leftConversion = sOrgNode.node.leftConversion;

    // next�� �ű��.
    aNode->node.next = sOrgNode.node.next;

    /* TASK-7219 */
    if ( aChangeName == QMO_CHANGE_COLUMN_NAME_DISABLE )
    {
        SET_POSITION( aNode->userName, sOrgNode.userName );
        SET_POSITION( aNode->tableName, sOrgNode.tableName );
        SET_POSITION( aNode->columnName, sOrgNode.columnName );
    }
    else if ( aChangeName == QMO_CHANGE_COLUMN_NAME_ONLY )
    {
        SET_EMPTY_POSITION( aNode->userName );
        SET_EMPTY_POSITION( aNode->tableName );
    }
    else
    {
        /* Nothing to do */
    }

    if ( aNode->columnName.offset == QC_POS_EMPTY_OFFSET )
    {
        SET_POSITION( aNode->columnName, sOrgNode.columnName );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoPushPred::transformToTargetValue( qtcNode      * aNode,
                                     qtcNode      * aTargetColumn )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode  sOrgNode;

    IDU_FIT_POINT_FATAL( "qmoPushPred::transformToTargetValue::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aTargetColumn != NULL );

    //------------------------------------------
    // view �÷��� transform ����
    //------------------------------------------

    // ��带 ����Ѵ�.
    idlOS::memcpy( & sOrgNode, aNode, ID_SIZEOF( qtcNode ) );

    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aNode, aTargetColumn, ID_SIZEOF( qtcNode ) );

    // conversion ��带 �ű��.
    aNode->node.conversion = sOrgNode.node.conversion;
    aNode->node.leftConversion = sOrgNode.node.leftConversion;

    // next�� �ű��.
    aNode->node.next = sOrgNode.node.next;

    return IDE_SUCCESS;
}

IDE_RC
qmoPushPred::transformToTargetExpression( qcStatement  * aStatement,
                                          qtcNode      * aNode,
                                          qtcNode      * aTargetColumn )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode  * sNode[2];
    qtcNode  * sNewNode;

    IDU_FIT_POINT_FATAL( "qmoPushPred::transformToTargetExpression::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aTargetColumn != NULL );

    //------------------------------------------
    // view �÷��� transform ����
    //------------------------------------------

    // expr�� ����� ������ template ������ �����Ѵ�.
    IDE_TEST( qtc::makeNode( aStatement,
                             sNode,
                             & aTargetColumn->position,
                             (mtfModule*) aTargetColumn->node.module )
              != IDE_SUCCESS );

    // expr ��� Ʈ���� ���� �����Ѵ�.
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     aTargetColumn,
                                     & sNewNode,
                                     ID_FALSE,  // root�� next�� �������� �ʴ´�.
                                     ID_TRUE,   // conversion�� ���´�.
                                     ID_TRUE,   // constant node���� �����Ѵ�.
                                     ID_TRUE )  // constant node�� �����Ѵ�.
              != IDE_SUCCESS );

    // template ��ġ�� �����Ѵ�.
    sNewNode->node.table = sNode[0]->node.table;
    sNewNode->node.column = sNode[0]->node.column;

    // conversion ��带 �ű��.
    sNewNode->node.conversion = aNode->node.conversion;
    sNewNode->node.leftConversion = aNode->node.leftConversion;

    // BUG-43017
    sNewNode->node.lflag &= ~MTC_NODE_REESTIMATE_MASK;
    sNewNode->node.lflag |= MTC_NODE_REESTIMATE_FALSE;

    // next�� �ű��.
    sNewNode->node.next = aNode->node.next;

    // ��带 ġȯ�Ѵ�.
    idlOS::memcpy( aNode, sNewNode, ID_SIZEOF( qtcNode ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::isPushableRankPred( qcStatement  * aStatement,
                                 qmsParseTree * aViewParseTree,
                                 qmsQuerySet  * aViewQuerySet,
                                 UShort         aViewTupleId,
                                 qtcNode      * aNode,
                                 idBool       * aCanPushDown,
                                 UInt         * aPushedRankTargetOrder,
                                 qtcNode     ** aPushedRankLimit )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     1. push ������ rank predicate���� Ȯ���Ѵ�.
 *        <, <=, >, >=, =�� �񱳿��길 �����ϴ�.
 *     2. push ������ view���� Ȯ���Ѵ�.
 *        rank predicate�� �÷��� view���� row_number �Լ��̾�� �Ѵ�.
 *
 ***********************************************************************/

    qmsTarget        * sTarget;
    qtcNode          * sOrNode;
    qtcNode          * sCompareNode;
    qtcNode          * sColumnNode;
    qtcNode          * sValueNode;
    qtcNode          * sNode;
    idBool             sIsPushable;
    UShort             sTargetOrder;

    IDU_FIT_POINT_FATAL( "qmoPushPred::isPushableRankPred::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aViewParseTree != NULL );
    IDE_DASSERT( aViewQuerySet  != NULL );
    IDE_DASSERT( aNode          != NULL );

    //--------------------------------------
    // �ʱ�ȭ �۾�
    //--------------------------------------

    sOrNode = aNode;
    sIsPushable = ID_FALSE;

    //--------------------------------------
    // pushable rank predicate �˻�
    //--------------------------------------

    // �ֻ��� ���� OR ��忩�� �Ѵ�.
    if ( ( sOrNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_OR )
    {
        sCompareNode = (qtcNode*) sOrNode->node.arguments;

        // �񱳿������� next�� NULL�̾�� �Ѵ�.
        if ( sCompareNode->node.next == NULL )
        {
            switch ( sCompareNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            {
                case MTC_NODE_OPERATOR_LESS:
                    // COLUMN < ���/ȣ��Ʈ ����
                case MTC_NODE_OPERATOR_LESS_EQUAL:
                    // COLUMN <= ���/ȣ��Ʈ ����

                    sColumnNode = (qtcNode*) sCompareNode->node.arguments;
                    sValueNode = (qtcNode*) sColumnNode->node.next;

                    IDE_TEST( isStopKeyPred( aViewTupleId,
                                             sColumnNode,
                                             sValueNode,
                                             & sIsPushable )
                              != IDE_SUCCESS );
                    break;

                case MTC_NODE_OPERATOR_GREATER:
                    // ���/ȣ��Ʈ ���� > COLUMN
                case MTC_NODE_OPERATOR_GREATER_EQUAL:
                    // ���/ȣ��Ʈ ���� >= COLUMN

                    sValueNode = (qtcNode*) sCompareNode->node.arguments;
                    sColumnNode = (qtcNode*) sValueNode->node.next;

                    IDE_TEST( isStopKeyPred( aViewTupleId,
                                             sColumnNode,
                                             sValueNode,
                                             & sIsPushable )
                              != IDE_SUCCESS );
                    break;

                case MTC_NODE_OPERATOR_EQUAL:

                    // COLUMN = ���/ȣ��Ʈ ����
                    sColumnNode = (qtcNode*) sCompareNode->node.arguments;
                    sValueNode = (qtcNode*) sColumnNode->node.next;

                    IDE_TEST( isStopKeyPred( aViewTupleId,
                                             sColumnNode,
                                             sValueNode,
                                             & sIsPushable )
                              != IDE_SUCCESS );

                    if ( sIsPushable == ID_FALSE )
                    {
                        // ���/ȣ��Ʈ ���� = COLUMN
                        sValueNode = (qtcNode*) sCompareNode->node.arguments;
                        sColumnNode = (qtcNode*) sValueNode->node.next;

                        IDE_TEST( isStopKeyPred( aViewTupleId,
                                                 sColumnNode,
                                                 sValueNode,
                                                 & sIsPushable )
                                  != IDE_SUCCESS );
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
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------
    // view �˻�
    //--------------------------------------

    if ( sIsPushable == ID_TRUE )
    {
        // order by�� ������ �ȵȴ�.
        if ( aViewParseTree->orderBy != NULL )
        {
            sIsPushable = ID_FALSE;
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

    if ( sIsPushable == ID_TRUE )
    {
        // root ��尡 row_number�̾�� �Ѵ�.
        for ( sTarget = aViewQuerySet->target, sTargetOrder = 0;
              sTarget != NULL;
              sTarget = sTarget->next, sTargetOrder++ )
        {
            if ( sTargetOrder == sColumnNode->node.column )
            {
                if ( sTarget->targetColumn->node.module != &mtfRowNumber )
                {
                    sIsPushable = ID_FALSE;
                }
                else
                {
                    // Nothing to do.
                }

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

    //---------------------------------------------------
    // Node ����
    //---------------------------------------------------

    if ( sIsPushable == ID_TRUE )
    {
        IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                         sValueNode,
                                         & sNode,
                                         ID_FALSE,  // root�� next�� �������� �ʴ´�.
                                         ID_TRUE,   // conversion�� ���´�.
                                         ID_TRUE,   // constant node���� �����Ѵ�.
                                         ID_FALSE ) // constant node�� �������� �ʴ´�.
                  != IDE_SUCCESS );

        // Node Estimate
        aViewQuerySet->processPhase = QMS_OPTIMIZE_PUSH_DOWN_PRED;

        IDE_TEST( qtc::estimate( sNode,
                                 QC_SHARED_TMPLATE(aStatement),
                                 NULL,
                                 aViewQuerySet,
                                 aViewQuerySet->SFWGH,
                                 NULL )
                  != IDE_SUCCESS);

        *aPushedRankTargetOrder = (UInt) sColumnNode->node.column;
        *aPushedRankLimit = sNode;
    }
    else
    {
        *aCanPushDown = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPushPred::isStopKeyPred( UShort         aViewTupleId,
                            qtcNode      * aColumn,
                            qtcNode      * aValue,
                            idBool       * aIsStopKey )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     stop key�� ������ �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

    *aIsStopKey = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoPushPred::isStopKeyPred::__FT__" );

    if ( ( aColumn->node.module == &qtc::columnModule ) &&
         ( aColumn->node.table == aViewTupleId ) )
    {
        if ( qtc::dependencyEqual( & aValue->depInfo,
                                   & qtc::zeroDependencies )
             == ID_TRUE )
        {
            *aIsStopKey = ID_TRUE;
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
}


IDE_RC
qmoPushPred::pushDownRankPredicate( qcStatement  * aStatement,
                                    qmsQuerySet  * aViewQuerySet,
                                    UInt           aRankTargetOrder,
                                    qtcNode      * aRankLimit )
{
/***********************************************************************
 *
 * Description :
 *
 * Implementation :
 *     target �÷��� row_number�� row_number_limit���� �����Ѵ�.
 *
 ***********************************************************************/

    qmsTarget  * sViewTarget;
    qtcNode    * sTargetColumn;
    UInt         sTargetOrder;

    IDU_FIT_POINT_FATAL( "qmoPushPred::pushDownRankPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aViewQuerySet != NULL );

    //------------------------------------------
    // push rank filter
    //------------------------------------------

    for ( sViewTarget = aViewQuerySet->target, sTargetOrder = 0;
          sViewTarget != NULL;
          sViewTarget = sViewTarget->next, sTargetOrder++ )
    {
        if ( sTargetOrder == aRankTargetOrder )
        {
            sTargetColumn = sViewTarget->targetColumn;

            /* node transform */
            sTargetColumn->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
            sTargetColumn->node.lflag |= 1;

            sTargetColumn->node.arguments = (mtcNode*) aRankLimit;

            /* row_number_limit �Լ��� �����Ѵ�. */
            sTargetColumn->node.module = &mtfRowNumberLimit;

            /* �Լ��� estimate�� �����Ѵ�. */
            IDE_TEST( qtc::estimateNodeWithArgument( aStatement,
                                                     sTargetColumn )
                      != IDE_SUCCESS );

            break;
        }
        else
        {
            /* Nothing to do. */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7219 */
IDE_RC qmoPushPred::checkPushDownPredicate( qcStatement  * aStatement,
                                            qmsParseTree * aViewParseTree,
                                            qmsQuerySet  * aViewQuerySet,
                                            qmsSFWGH     * aOuterQuery,
                                            UShort         aViewTupleId,
                                            qmoPredicate * aPredicate,
                                            idBool       * aIsPushed )
{
/****************************************************************************************
 *
 * Description : qmoPushPred::doPushDownViewPredicate �� �����Ͽ� �ۼ��� Push Selection
 *               ���θ� �˻��ϴ� �Լ��̴�. Set �����ڰ� ���� Query Set�� ����ϰ� �ִ�.
 *
 * Implementation : 1. Recursive View�� Push Down�� �� ����.
 *                  2. Push Selection �����ص� �Ǵ� Query Set���� �˻��Ѵ�.
 *                  3. Push Selection �ص� �Ǵ� Predicate���� �˻��Ѵ�.
 *                  4. Set �����ڸ� ����ߴٸ�, ��� ȣ���Ѵ�.
 *                  5. Push Selection�� �����ϸ� ���θ� ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    qtcNode * sPushedRankLimit       = NULL;
    UInt      sPushedRankTargetOrder = 0;
    idBool    sCanPushDown           = ID_FALSE;
    idBool    sIsRemain              = ID_FALSE;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aViewQuerySet == NULL, ERR_NULL_QUERYSET );
    IDE_TEST_RAISE( aPredicate == NULL, ERR_NULL_PREDICATE );

    /* 1. Recursive View�� Push Down�� �� ����. */
    IDE_TEST_CONT( ( aViewQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                   == QMV_QUERYSET_RECURSIVE_VIEW_TOP,
                   NORMAL_EXIT );

    if ( aViewQuerySet->setOp == QMS_NONE )
    {
        sCanPushDown = ID_TRUE;

        /* 2. Push Selection �����ص� �Ǵ� Query Set���� �˻��Ѵ�. */
        IDE_TEST( canPushSelectionQuerySet( aViewParseTree,
                                            aViewQuerySet,
                                            &( sCanPushDown ),
                                            &( sIsRemain ) )
                  != IDE_SUCCESS );

        if ( sCanPushDown == ID_TRUE )
        {
            /* 3. Push Selection �ص� �Ǵ� Predicate���� �˻��Ѵ�. */
            IDE_TEST( canPushDownPredicate( aStatement,
                                            aViewQuerySet,
                                            aViewQuerySet->target,
                                            aViewTupleId,
                                            aOuterQuery,
                                            aPredicate->node,
                                            ID_FALSE, /* Next�� �˻����� �ʴ´�. */
                                            &( sCanPushDown ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        if ( sCanPushDown == ID_FALSE )
        {
            sCanPushDown = ID_TRUE;

            /* BUG-40354 pushed rank - push������ predicate����, view���� Ȯ���Ѵ�. */
            IDE_TEST( isPushableRankPred( aStatement,
                                          aViewParseTree,
                                          aViewQuerySet,
                                          aViewTupleId,
                                          aPredicate->node,
                                          &( sCanPushDown ),
                                          &( sPushedRankTargetOrder ),
                                          &( sPushedRankLimit ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* 4. Set �����ڸ� ����ߴٸ�, ��� ȣ���Ѵ�. */
        if ( aViewQuerySet->left != NULL )
        {
            IDE_TEST( checkPushDownPredicate( aStatement,
                                              aViewParseTree,
                                              aViewQuerySet->left,
                                              aOuterQuery,
                                              aViewTupleId,
                                              aPredicate,
                                              &( sCanPushDown ) )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do */
        }

        /* 5. Set �������� ���, ��� �����ؾ� �����Ѵ�. * /
        if ( sCanPushDown == ID_TRUE )
        {
            if ( aViewQuerySet->right != NULL )
            {
                IDE_TEST( checkPushDownPredicate( aStatement,
                                                  aViewParseTree,
                                                  aViewQuerySet->right,
                                                  aViewTupleId,
                                                  aPredicate,
                                                  &( sCanPushDown ) )
                          != IDE_SUCCESS );
            }
            else
            {
                / * Nothing to do * /
            }
        }
        else
        {
            / * Nothing to do * /
        }*/
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    /* 5. Push Selection�� �����ϸ� ���θ� ��ȯ�Ѵ�. */
    if ( aIsPushed != NULL )
    {
        *aIsPushed = sCanPushDown;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::checkPushDownPredicate",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::checkPushDownPredicate",
                                  "query set is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PREDICATE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::checkPushDownPredicate",
                                  "predicate is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7219 */
IDE_RC qmoPushPred::changePredicateNodeName( qcStatement * aStatement,
                                             qmsQuerySet * aViewQuerySet,
                                             UShort        aViewTupleId,
                                             qtcNode     * aSrcNode,
                                             qtcNode    ** aDstNode )
{
/****************************************************************************************
 *
 * Description : Shard Push Selection�� ������ �ٸ��� Push Selection�� �˻��ϰ� �ٷ�
 *               Where�� ������ �ʰ�, ���ο� Text�� ������ ���� ������ ���·� ��ȯ�ؼ�
 *               ����Ѵ�.
 *
 * Implementation : 1. CNF, DNF Tree ������ �����Ѵ�.
 *                  2. Predicate Node�� ã�Ҵ�.
 *                  3. Predicate Node�� �����Ѵ�.
 *                  4. Set�� ��� Left �������� ��ȯ�ؾ� �Ѵ�.
 *                  5. ������ Predicate�� Table Predicate���� ��ȯ�Ѵ�.
 *                  6. ��ȯ�� Predicate ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    qmsQuerySet * sQuerySet   = NULL;
    qtcNode     * sSrcNode    = NULL;
    qtcNode     * sDstNode    = NULL;
    UShort        sChangeName = QMO_CHANGE_COLUMN_NAME_ENABLE;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aViewQuerySet == NULL, ERR_NULL_QUERYSET );
    IDE_TEST_RAISE( aSrcNode == NULL, ERR_NULL_NODE );

    for ( sSrcNode  = (qtcNode *)aSrcNode;
          sSrcNode != NULL;
          sSrcNode  = (qtcNode *)sSrcNode->node.arguments )
    {
        /* 1. CNF, DNF Tree ������ �����Ѵ�. */
        if ( ( ( sSrcNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
               == MTC_NODE_LOGICAL_CONDITION_TRUE ) &&
             ( sSrcNode->node.arguments->next == NULL ) )
        {
            continue;
        }
        else
        {
            /* 2. Predicate Node�� ã�Ҵ�. */
            break;
        }
    }

    /* 3. Predicate Node�� �����Ѵ�. */
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     sSrcNode,
                                     &( sDstNode ),
                                     ID_FALSE,  /* root�� next�� �������� �ʴ´�. */
                                     ID_TRUE,   /* conversion�� ���´�. */
                                     ID_TRUE,   /* constant node���� �����Ѵ�. */
                                     ID_FALSE ) /* constant node�� �������� �ʴ´�. */
              != IDE_SUCCESS );

    /* 4. Set�� ��� Left �������� ��ȯ�ؾ� �Ѵ�. */
    sQuerySet   = aViewQuerySet;

    while ( sQuerySet->setOp != QMS_NONE )
    {
        sQuerySet   = sQuerySet->left;
        sChangeName = QMO_CHANGE_COLUMN_NAME_ONLY;
    }

    /* 5. ������ Predicate�� Table Predicate���� ��ȯ�Ѵ�. */
    IDE_TEST( changeViewPredIntoTablePred( aStatement,
                                           sQuerySet->target,
                                           aViewTupleId,
                                           sDstNode,
                                           sChangeName, /* Shard Push Selection�� ��� */
                                           ID_FALSE )   /* Next�� �ٲ��� �ʴ´�. */
             != IDE_SUCCESS );

    /* 6. ��ȯ�� Predicate ��ȯ�Ѵ�. */
    if ( aDstNode != NULL )
    {
        *aDstNode = sDstNode;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::changePushDownPredicate",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_QUERYSET )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::changePushDownPredicate",
                                  "query set is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_NODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPushPred::changePushDownPredicate",
                                  "node is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7219 Non-shard DML */
void qmoPushPred::setForcePushedPredForShardView( mtcNode * aNode )
{
    if ( aNode != NULL )
    {
        aNode->lflag &= ~MTC_NODE_PUSHED_PRED_FORCE_MASK;
        aNode->lflag |= MTC_NODE_PUSHED_PRED_FORCE_TRUE;

        setForcePushedPredForShardView( aNode->arguments );

        setForcePushedPredForShardView( aNode->next );
    }
}
