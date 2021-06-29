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
 * $Id: qmvOrderBy.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qtcDef.h>
#include <qmvOrderBy.h>
#include <qmvQuerySet.h>
#include <qmvQTC.h>
#include <qmn.h>
#include <qcuSqlSourceInfo.h>
#include <qmv.h>

extern mtfModule mtfDecrypt;

IDE_RC qmvOrderBy::searchSiblingColumn( qtcNode  * aExpression,
                                        qtcNode ** aColumn,
                                        idBool   * aFind )
{
    IDU_FIT_POINT_FATAL( "qmvOrderBy::searchSiblingColumn::__FT__" );

    if ( QTC_IS_SUBQUERY( aExpression ) == ID_FALSE )
    {
        if ( ( aExpression->node.module == &(qtc::columnModule ) ) &&
             ( *aFind == ID_FALSE ) )
        {
            *aColumn = aExpression;
            *aFind = ID_TRUE;
        }
        else
        {
            /* Nothing to do */
        }

        if ( *aFind == ID_FALSE )
        {
            if ( aExpression->node.next != NULL )
            {
                IDE_TEST( searchSiblingColumn( (qtcNode *)aExpression->node.next,
                                               aColumn,
                                               aFind )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }

            if ( aExpression->node.arguments != NULL )
            {
                IDE_TEST( searchSiblingColumn( (qtcNode *)aExpression->node.arguments,
                                               aColumn,
                                               aFind )
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
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvOrderBy::validate(qcStatement * aStatement)
{

    qmsParseTree    * sParseTree;
    qmsSortColumns  * sCurrSort;
    qmsQuerySet     * sQuerySet;
    qmsSFWGH        * sSFWGH;
    qtcNode         * sSortColumn;
    qtcOverColumn   * sOverColumn;
    mtcColumn       * sColumn;
    idBool            sFind          = ID_FALSE;
    qtcNode         * sFindColumn    = NULL;
    qtcNode         * sSiblingColumn = NULL;
    qmsSortColumns  * sSibling       = NULL;
    qmsSortColumns  * sSort          = NULL;
    qmsSortColumns    sTemp;
    qcuSqlSourceInfo  sqlInfo;
    qtcNode         * sNewNode = NULL;
    qtcNode         * sPrevNode = NULL;

    IDU_FIT_POINT_FATAL( "qmvOrderBy::validate::__FT__" );

    //---------------
    // �⺻ �ʱ�ȭ
    //---------------
    sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;

    //---------------
    // validate ó�� �ܰ� ����
    //---------------
    sParseTree->querySet->processPhase = QMS_VALIDATE_ORDERBY;

    /* BUG-46728 */
    if ( QCU_COERCE_HOST_VAR_TO_VARCHAR > 0 )
    {
        for ( sCurrSort = sParseTree->orderBy;
              sCurrSort != NULL;
              sCurrSort = sCurrSort->next)
        {
            IDE_TEST( qmvQuerySet::searchHostVarAndAddCastOper( aStatement,
                                                                sCurrSort->sortColumn,
                                                                &sNewNode,
                                                                ID_FALSE )
                      != IDE_SUCCESS );

            if ( sNewNode != NULL )
            {
                sCurrSort->sortColumn = sNewNode;

                if ( sPrevNode != NULL )
                {
                    sPrevNode->node.next = (mtcNode*)sNewNode;
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

            sPrevNode = sCurrSort->sortColumn;
        }
    }
    else
    {
        /* Nothing to do */
    }

    if (sParseTree->querySet->setOp == QMS_NONE)
    {
        // This Query doesn't have SET(UNION, INTERSECT, MINUS) operations.
        sSFWGH  = sParseTree->querySet->SFWGH;

        if (sSFWGH->group != NULL)
        {
            IDE_TEST(validateSortWithGroup(aStatement) != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(validateSortWithoutGroup(aStatement) != IDE_SUCCESS);
        }

        if (sSFWGH->selectType == QMS_DISTINCT)
        {
            for (sCurrSort = sParseTree->orderBy;
                 sCurrSort != NULL;
                 sCurrSort = sCurrSort->next)
            {
                if (sCurrSort->targetPosition <= QMV_EMPTY_TARGET_POSITION)
                {
                    IDE_TEST(qmvQTC::isSelectedExpression(
                                 aStatement,
                                 sCurrSort->sortColumn,
                                 sSFWGH->target)
                             != IDE_SUCCESS);
                }
            }
        }
    }
    else 
    {
        // UNION, UNION ALL, INTERSECT, MINUS
        IDE_TEST(validateSortWithSet(aStatement) != IDE_SUCCESS);
    }

    // check host variable
    for (sCurrSort = sParseTree->orderBy;
         sCurrSort != NULL;
         sCurrSort = sCurrSort->next)
    {
        IDE_FT_ASSERT( sCurrSort->sortColumn != NULL );

        // BUG-27208
        if ( sCurrSort->sortColumn->node.module == &qtc::assignModule )
        {
            sSortColumn = (qtcNode *)
                sCurrSort->sortColumn->node.arguments;
        }
        else
        {
            sSortColumn = sCurrSort->sortColumn;
        }

        // PROJ-1492
        // BUG-42041
        if ( ( MTC_NODE_IS_DEFINED_TYPE( & sSortColumn->node ) == ID_FALSE ) &&
             ( aStatement->calledByPSMFlag == ID_FALSE ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_ALLOW_HOSTVAR );
        }

        // BUG-14094
        if ( ( sSortColumn->node.lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_LIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
        }
        else
        {
            // Nothing to do.
        }

        // BUG-14094
        if ( ( sSortColumn->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
        }
        else
        {
            // Nothing to do.
        }

        // BUG-14094
        if ( ( sSortColumn->node.lflag & MTC_NODE_COMPARISON_MASK )
             == MTC_NODE_COMPARISON_TRUE )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
        }
        else
        {
            // Nothing to do.
        }

        // BUG-16083
        if ( ( sSortColumn->node.lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_SUBQUERY )
        {
            if ( (sSortColumn->node.lflag &
                  MTC_NODE_ARGUMENT_COUNT_MASK) > 1 )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sSortColumn->position );
                IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
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

        // BUG-15896
        // BUG-24133
        sColumn = QTC_TMPL_COLUMN( QC_SHARED_TMPLATE(aStatement), sSortColumn );

        if ( ( sColumn->module->id == MTD_BOOLEAN_ID ) ||
             ( sColumn->module->id == MTD_ROWTYPE_ID ) ||
             ( sColumn->module->id == MTD_RECORDTYPE_ID ) ||
             ( sColumn->module->id == MTD_ASSOCIATIVE_ARRAY_ID ) ||
             ( sColumn->module->id == MTD_REF_CURSOR_ID ) )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-1362
        // Indicator�� ����Ű�� Į���� Equal������ �Ұ�����
        // Ÿ��(Lob or Binary Type)�� ���, ���� ��ȯ
        // BUG-22817 : ������� ����  �Ͻ����� ��� ��� �˻��ؾ���
        if ( ( sSortColumn->lflag & QTC_NODE_BINARY_MASK )
             == QTC_NODE_BINARY_EXIST )
        {
            sqlInfo.setSourceInfo( aStatement,
                                   & sSortColumn->position );
            IDE_RAISE( ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE );
        }
        else
        {
            // Nothing to do.
        }

        /*
         * PROJ-1789 PROWID
         * ordery by ���� _prowid �������� �ʴ´�.
         */
        if ((sSortColumn->lflag & QTC_NODE_COLUMN_RID_MASK) ==
            QTC_NODE_COLUMN_RID_EXIST)
        {
            sqlInfo.setSourceInfo(aStatement, &sSortColumn->position);
            IDE_RAISE( ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE );
        }
        else
        {
        }

        if ( sSortColumn->overClause != NULL )
        {
            for ( sOverColumn = sSortColumn->overClause->overColumn;
                  sOverColumn != NULL;
                  sOverColumn = sOverColumn->next )
            {
                // ����Ʈ Ÿ���� ��� ���� Ȯ��
                if ( ( sOverColumn->node->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           & sOverColumn->node->position );
                    IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
                }
                else
                {
                    // Nothing to do.
                }

                // ���������� ��� �Ǿ����� Ÿ�� �÷��� �ΰ��̻����� Ȯ��
                if ( ( sOverColumn->node->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_SUBQUERY )
                {
                    if ( ( sOverColumn->node->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK ) > 1 )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               & sOverColumn->node->position );
                        IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
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

                // BUG-35670 over ���� lob, geometry type ��� �Ұ�
                if ((sOverColumn->node->lflag & QTC_NODE_BINARY_MASK) ==
                    QTC_NODE_BINARY_EXIST)
                {
                    sqlInfo.setSourceInfo( aStatement,
                                           &sOverColumn->node->position );
                    IDE_RAISE( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
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

    // disconnect constant node
    if (sParseTree->querySet->setOp == QMS_NONE)
    {
        // PROJ-1413
        IDE_TEST( disconnectConstantNode( sParseTree )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-1715 */
    if ( sParseTree->querySet->SFWGH != NULL )
    {
        if ( sParseTree->querySet->SFWGH->hierarchy != NULL )
        {
            if ( sParseTree->isSiblings == ID_TRUE )
            {
                /* ORDER SIBLING BY �� �÷��߿� Hierarhy�� ���õ� �͸� �̾Ƽ�
                 * ORDER SIBLING BY�� �� �����Ѵ�.
                 */
                sSibling   = &sTemp;
                sTemp.next = NULL;
                for (sCurrSort = sParseTree->orderBy;
                     sCurrSort != NULL;
                     sCurrSort = sCurrSort->next)
                {
                    /* BUG-36707 wrong result using many siblings by column */
                    sFind = ID_FALSE;

                    if ( QTC_IS_PSEUDO( sCurrSort->sortColumn ) == ID_TRUE )
                    {
                        sqlInfo.setSourceInfo( aStatement,
                                               &sCurrSort->sortColumn->position );
                        IDE_RAISE( ERR_NOT_ALLOW_PSEUDO_ORDER_SIBLINGS_BY );
                    }
                    else
                    {
                        /* Nothing to do */
                    }

                    IDE_TEST( searchSiblingColumn( sCurrSort->sortColumn,
                                                   &sFindColumn,
                                                   &sFind )
                              != IDE_SUCCESS );

                    if ( sFind == ID_FALSE )
                    {
                        continue;
                    }
                    else
                    {
                        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                                qmsSortColumns,
                                                &sSort )
                                  != IDE_SUCCESS );
                        idlOS::memcpy( sSort, sCurrSort, sizeof( qmsSortColumns ) );
                        sSort->targetPosition = QMV_EMPTY_TARGET_POSITION;
                        sSort->next           = NULL; /* BUG-46184 */
                        IDE_TEST( STRUCT_ALLOC( QC_QMP_MEM( aStatement ),
                                                qtcNode,
                                                &sSiblingColumn )
                                  != IDE_SUCCESS );

                        idlOS::memcpy( sSiblingColumn, sFindColumn, sizeof( qtcNode ) );
                        sSiblingColumn->node.next = NULL;
                        sSiblingColumn->node.arguments = NULL;

                        sSort->sortColumn = sSiblingColumn;
                        sSibling->next    = sSort;
                        sSibling          = sSibling->next;
                    }
                }
                sParseTree->querySet->SFWGH->hierarchy->siblings = sTemp.next;
                sParseTree->orderBy = NULL;
            }
            else
            {
                sParseTree->querySet->SFWGH->hierarchy->siblings = NULL;
            }
        }
        else
        {
            IDE_TEST_RAISE( sParseTree->isSiblings == ID_TRUE,
                            ERR_NOT_ALLOW_ORDER_SIBLINGS_BY );
        }
    }
    else
    {
        IDE_TEST_RAISE( sParseTree->isSiblings == ID_TRUE,
                        ERR_NOT_ALLOW_ORDER_SIBLINGS_BY );
    }

    // BUG-41221 Lateral View������ Order By �� �ܺ�����
    // Sort Column�� depInfo�� QuerySet�� depInfo �ۿ� �ִٸ�
    // Sort Column�� depInfo�� QuerySet�� lateralDepInfo�� �߰��Ѵ�.
    sQuerySet = sParseTree->querySet;

    for ( sCurrSort = sParseTree->orderBy;
          sCurrSort != NULL;
          sCurrSort = sCurrSort->next )
    {
        if ( qtc::dependencyContains( & sQuerySet->depInfo,
                                      & sCurrSort->sortColumn->depInfo )
             == ID_FALSE )
        {
            // QuerySet�� depInfo �ۿ� ������, �ܺ� ����
            // �켱 SortColumn�� depInfo�� lateralDepInfo�� ����
            IDE_TEST( qtc::dependencyOr( & sQuerySet->lateralDepInfo,
                                         & sCurrSort->sortColumn->depInfo,
                                         & sQuerySet->lateralDepInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // QuerySet�� depInfo �ȿ� ������, ���� ����
        }
    }

    // BUG-41967 lateralDepInfo���� ���� dependency�� ����
    if ( qtc::haveDependencies( & sQuerySet->lateralDepInfo ) == ID_TRUE )
    {
        qtc::dependencyMinus( & sQuerySet->lateralDepInfo,
                              & sQuerySet->depInfo,
                              & sQuerySet->lateralDepInfo );
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_ALLOW_HOSTVAR );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_HOSTVAR,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_APPLICABLE_TYPE_IN_ORDER_BY );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_APPLICABLE_TYPE_IN_ORDER_BY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION(ERR_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE)
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOWED_INCOMPARABLE_DATA_TYPE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_ORDER_SIBLINGS_BY )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMV_NOT_ALLOW_ORDER_SIBLINGS_BY) );
    }
    IDE_EXCEPTION( ERR_NOT_ALLOW_PSEUDO_ORDER_SIBLINGS_BY )
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_UNSUPPORTED_PSEUDO_COLUMN_IN_ORDER_SIBLINGS_BY,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmvOrderBy::validateSortWithGroup(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *     GROUP BY�� �Բ� �����ϴ� ORDER BY�� ���� Vaildation ����
 *
 * Implementation :
 *     - ORDER BY�� �����ϴ� Column�� Target�� ������ ���� �ִ� ����
 *       �˻��Ͽ� Indicator�� �ۼ��Ѵ�.
 *     - ORDER BY�� �����ϴ� Column�� GROUP BY�� �����ϴ� Column������
 *       �˻��Ѵ�.
 *
 ***********************************************************************/

    qmsParseTree    * sParseTree;
    qmsSortColumns  * sCurrSort;
    qmsSFWGH        * sSFWGH;
    qmsTarget       * sTarget;
    SInt              sTargetMaxPos = 0;
    SInt              sCurrTargetPos;
    qmsAggNode      * sAggsDepth1;
    qmsAggNode      * sAggsDepth2;
    qcuSqlSourceInfo  sqlInfo;

    qtcNode         * sTargetNode1;
    qtcNode         * sTargetNode2;
    idBool            sIsTrue = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmvOrderBy::validateSortWithGroup::__FT__" );

    sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;
    sSFWGH  = sParseTree->querySet->SFWGH;
    sAggsDepth1 = sSFWGH->aggsDepth1;
    sAggsDepth2 = sSFWGH->aggsDepth2;

    for (sTarget = sSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next)
    {
        sTargetMaxPos++;
    }

    for (sCurrSort = sParseTree->orderBy;
         sCurrSort != NULL;
         sCurrSort = sCurrSort->next)
    {
        //-----------------------------------------------
        // ORDER BY�� indicator������ �˻�
        // ����� indicator�� ���
        //     Ex) ORDER BY 1;
        // Target�� ������ ���(�Ͻ��� Indicator)
        //     Ex) SELECT t1.i1 FROM T1 GROUP BY I1 ORDER BY I1;
        //     Ex) SELECT t1.i1 A FROM T1 GROUP BY i1 ORDER BY A;
        //-----------------------------------------------

        //-----------------------------------------------
        // 1. ����� indicator������ �˻�
        //-----------------------------------------------

        IDE_TEST(qtc::getSortColumnPosition(sCurrSort, QC_SHARED_TMPLATE(aStatement))
                 != IDE_SUCCESS);

        //-----------------------------------------------
        // 2. �Ͻ��� indicator������ �˻�
        //-----------------------------------------------

        if (sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION)
        {
            /* PROJ-2197 PSM Renewal */
            sCurrSort->sortColumn->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
            IDE_TEST(qtc::estimate(
                         sCurrSort->sortColumn,
                         QC_SHARED_TMPLATE(aStatement),
                         aStatement,
                         sParseTree->querySet,
                         sSFWGH,
                         NULL )
                     != IDE_SUCCESS);

            if ( ( sCurrSort->sortColumn->node.lflag & MTC_NODE_DML_MASK )
                 == MTC_NODE_DML_UNUSABLE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrSort->sortColumn->position );
                IDE_RAISE( ERR_USE_CURSOR_ATTR );
            }
            else
            {
                // �Ͻ��� Indicator���� �˻��Ѵ�.
                // Target�� ID�� ORDER BY�� ID�� �����ϴٸ�
                // Indicator�� ��Ȱ�� �ϰ� �ȴ�.

                for (sTarget = sSFWGH->target, sCurrTargetPos = 1;
                     sTarget != NULL;
                     sTarget = sTarget->next, sCurrTargetPos++)
                {
                    // PROJ-2002 Column Security
                    // target���� ���� �÷��� �ִ� ��� decrypt�Լ��� ������ �� �ִ�.
                    if ( sTarget->targetColumn->node.module == &mtfDecrypt )
                    {
                        sTargetNode1 = (qtcNode *)
                            sTarget->targetColumn->node.arguments;
                    }
                    else
                    {
                        sTargetNode1 = sTarget->targetColumn;
                    }

                    // To Fix PR-8615, PR-8820, PR-9143
                    // GROUP BY�� �Բ� ���� Target��
                    // estimation �������� Pass Node�� ��ü�� �� �ִ�.
                    // �� ��, Target�� Naming�� ����
                    // ORDER BY�� ID�� ������ ���� �پ��� ���·�
                    // �����Ǿ� ���� �� �ִ�.
                    // Ex) ID�� ������ Pass Node�� ���
                    //     SELECT T1.i1 A FROM T1, T2 GROUP BY i1 ORDER BY i1;
                    // Ex) ID�� �ٸ��� Pass Node�� ���
                    //     SELECT i1 FROM T1 GROUP BY i1 ORDER BY T1.i1;
                    // ��, ���� �� ��� ��� �Ͻ��� Indicator��
                    // Target Position�� ������ �� �ִ�.
                    // ����, Target ��ü�� ID �� Pass Node�� ���
                    // Argument�� ID�� ������ ORDER BY ID���,
                    // �Ͻ��� Indicator�� ��ü�� �� �ִ�.

                    if ( sTargetNode1->node.module == &qtc::passModule )
                    {
                        sTargetNode2 = (qtcNode *)
                            sTargetNode1->node.arguments;
                    }
                    else
                    {
                        sTargetNode2 = sTargetNode1;
                    }

                    /* BUG-32102
                     * target �� orderby ���� �߸� ���Ͽ� ����� �޶���
                     */
                    IDE_TEST(qtc::isEquivalentExpression( aStatement,
                                                          sTargetNode2,
                                                          sCurrSort->sortColumn,
                                                          &sIsTrue)
                             != IDE_SUCCESS);

                    if ( sIsTrue == ID_TRUE )
                    {
                        sCurrSort->targetPosition = sCurrTargetPos;

                        IDE_TEST(transposePosValIntoTargetPtr( sCurrSort,
                                                               sSFWGH->target )
                                 != IDE_SUCCESS);

                        break;
                    }
                }
            }
        }
        else
        {
            // ����� Indicator��.
            IDE_TEST(transposePosValIntoTargetPtr( sCurrSort,
                                                   sSFWGH->target )
                     != IDE_SUCCESS);
            
            // PROJ-1413
            // view �÷� ���� ��带 ����Ѵ�.
            IDE_TEST( qmvQTC::addViewColumnRefList( aStatement,
                                                    sCurrSort->sortColumn )
                      != IDE_SUCCESS );
        }

        if ( sCurrSort->targetPosition > QMV_EMPTY_TARGET_POSITION )
        {
            //-----------------------------------------------
            // 3. Indicator�� ���� Validation
            //-----------------------------------------------

            // Indicator�� �����ϴ� ���
            if ( sCurrSort->targetPosition > sTargetMaxPos)
            {
                IDE_RAISE( ERR_INVALID_ORDERBY_POS );
            }

            // To Fix PR-8115
            // TARGET POSITION�� �ִ� ORDER BY��
            // Order By Indicator ����ȭ�� ����ȴ�.
            // �Ʒ��� ���� ������ ��� Target Position�� ���� ���� �ϴ���,
            // ������ Target�� �ִ� ��� Target Position�� �����ϰ� �Ǵµ�,
            // Ex) SELECT i1, MAX(i2) FROM T1 GROUP BY i1 ORDER BY i1;
            //  --> SELECT i1, MAX(i2) FROM T1 GROUP BY i1 ORDER BY 1;
            //                                                     ^^
            // Order By Indicator ����ȭ�� Group By Expression�� Pass Node
            // ��� ����� ���ÿ� ���� �� ����.
            // ��, Target�� �����ϴ� Order By �÷��� ���
            // Target�� Group By �� �˻翡 ���Ͽ� Order By����
            // Group By �÷� �̿�(Aggregation����)�� �÷��� ���� �� ���ٴ�
            // ���� ������ �������� ������ �� �ִ�.

            // Nothing To Do
        }
        else
        {
            // Indicator�� �������� �ʴ� ���

            // Nothing To Do
        }
    }

    //--------------------------------------
    // 4. check group expression
    //--------------------------------------
    
    if (sSFWGH->aggsDepth2 != NULL)
    {
        // order by������ �߰��� aggsDepth2�� �˻��ؾ��Ѵ�.
        for ( sCurrSort = sParseTree->orderBy;
              sCurrSort != NULL;
              sCurrSort = sCurrSort->next)
        {
            if ( sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION )
            {
                if ( QTC_HAVE_AGGREGATE2( sCurrSort->sortColumn ) == ID_TRUE )
                {
                    IDE_TEST( qmvQTC::haveNotNestedAggregate( aStatement, 
                                                              sSFWGH, 
                                                              sCurrSort->sortColumn )
                              != IDE_SUCCESS);
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
        
        /* BUG-39332 group by�� �ְ� aggsDepth2�� �ִ� ���� ����� 1���̹Ƿ�
         * order by�� �����Ѵ�.
         * �׸��� order by������ �߰��� aggregation�� �����Ѵ�.
         */
        sParseTree->orderBy = NULL;
        
        sSFWGH->aggsDepth1 = sAggsDepth1;
        sSFWGH->aggsDepth2 = sAggsDepth2;
    }
    else
    {
        // GROUP BY Expression�� �����ϴ� Expression������ �˻�.
        for ( sCurrSort = sParseTree->orderBy;
              sCurrSort != NULL;
              sCurrSort = sCurrSort->next )
        {
            if ( sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION )
            {
                IDE_TEST( qmvQTC::isGroupExpression( aStatement,
                                                     sSFWGH,
                                                     sCurrSort->sortColumn,
                                                     ID_TRUE ) // make pass node
                          != IDE_SUCCESS );

                // BUG-48128 order by���� ����
                sSFWGH->thisQuerySet->lflag &= ~QMV_QUERYSET_SCALAR_SUBQ_OUTER_COL_MASK;
                sSFWGH->thisQuerySet->lflag |= QMV_QUERYSET_SCALAR_SUBQ_OUTER_COL_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_INVALID_ORDERBY_POS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_INVALID_POSITION_IN_ORDERBY));
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvOrderBy::validateSortWithoutGroup(qcStatement * aStatement)
{

    qmsParseTree    * sParseTree;
    qmsSortColumns  * sCurrSort;
    qmsSFWGH        * sSFWGH;
    qmsTarget       * sTarget;
    qtcNode         * sTargetNode;
    SInt              sTargetMaxPos = 0;
    SInt              sCurrTargetPos;
    qmsAggNode      * sAggsDepth1;
    qcuSqlSourceInfo  sqlInfo;
    idBool            sIsTrue;

    IDU_FIT_POINT_FATAL( "qmvOrderBy::validateSortWithoutGroup::__FT__" );

    sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;
    sSFWGH  = sParseTree->querySet->SFWGH;
    sAggsDepth1 = sSFWGH->aggsDepth1;
    
    for (sTarget = sSFWGH->target;
         sTarget != NULL;
         sTarget = sTarget->next)
    {
        sTargetMaxPos++;
    }

    for (sCurrSort = sParseTree->orderBy;
         sCurrSort != NULL;
         sCurrSort = sCurrSort->next)
    {
        IDE_TEST(qtc::getSortColumnPosition(sCurrSort, QC_SHARED_TMPLATE(aStatement))
                 != IDE_SUCCESS);
        
        if (sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION)
        {
            /* PROJ-2197 PSM Renewal */
            sCurrSort->sortColumn->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
            IDE_TEST(qtc::estimate(
                         sCurrSort->sortColumn,
                         QC_SHARED_TMPLATE(aStatement),
                         aStatement,
                         sParseTree->querySet,
                         sSFWGH,
                         NULL )
                     != IDE_SUCCESS);

            if ( ( sCurrSort->sortColumn->node.lflag & MTC_NODE_DML_MASK )
                 == MTC_NODE_DML_UNUSABLE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrSort->sortColumn->position );
                IDE_RAISE( ERR_USE_CURSOR_ATTR );
            }

            // If column name is alias name,
            // then get position of target
            for (sTarget = sSFWGH->target, sCurrTargetPos = 1;
                 sTarget != NULL;
                 sTarget = sTarget->next, sCurrTargetPos++)
            {
                // PROJ-2002 Column Security
                // target���� ���� �÷��� �ִ� ��� decrypt�Լ��� ������ �� �ִ�.
                if ( sTarget->targetColumn->node.module == &mtfDecrypt )
                {
                    sTargetNode = (qtcNode *)
                        sTarget->targetColumn->node.arguments;
                }
                else
                {
                    sTargetNode = sTarget->targetColumn;
                }

                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       sTargetNode,
                                                       sCurrSort->sortColumn,
                                                       &sIsTrue )
                          != IDE_SUCCESS );

                if( sIsTrue == ID_TRUE )
                {
                    sCurrSort->targetPosition = sCurrTargetPos;

                    IDE_TEST(transposePosValIntoTargetPtr( sCurrSort,
                                                           sSFWGH->target )
                             != IDE_SUCCESS);

                    break;
                }
            }
        }
        else if (sCurrSort->targetPosition > QMV_EMPTY_TARGET_POSITION &&
                 sCurrSort->targetPosition <= sTargetMaxPos)
        {
            IDE_TEST(transposePosValIntoTargetPtr( sCurrSort,
                                                   sSFWGH->target )
                     != IDE_SUCCESS);
            
            // PROJ-1413
            // view �÷� ���� ��带 ����Ѵ�.
            IDE_TEST( qmvQTC::addViewColumnRefList( aStatement,
                                                    sCurrSort->sortColumn )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ORDERBY_POS );
        }
    }

    if (sSFWGH->aggsDepth1 == NULL)
    {
        for (sCurrSort = sParseTree->orderBy;
             sCurrSort != NULL;
             sCurrSort = sCurrSort->next)
        {
            if (sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION)
            {
                // BUG-27597
                // order by������ analytic func�� �ƴ� aggregation�� ��� ����
                // aggregation�� argument�� analytic func�� �̹� �ɷ�����
                if ( ( sCurrSort->sortColumn->lflag & QTC_NODE_ANAL_FUNC_MASK )
                     == QTC_NODE_ANAL_FUNC_ABSENT )
                {
                    IDE_TEST_RAISE(QTC_HAVE_AGGREGATE(sCurrSort->sortColumn)
                                   == ID_TRUE, ERR_CANNOT_HAVE_AGGREGATE);
                }
                else
                {
                    // nothing to do 
                }
            }
        }
    }
    else // if (sSFWGH->aggsDepth1 != NULL)
    {
        if (sSFWGH->aggsDepth2 == NULL)
        {
            // If aggregation exists only in order by clause,
            // select clause is not checked in previous validation function.
            // If aggregation exists in SFWGH,
            // the following check of select clause is double check.
            for (sTarget = sSFWGH->target;
                 sTarget != NULL;
                 sTarget = sTarget->next)
            {
                IDE_TEST(qmvQTC::isAggregateExpression(
                             aStatement, sSFWGH, sTarget->targetColumn)
                         != IDE_SUCCESS);
            }

            // order by������ �߰��� aggsDepth1�� �˻��ؾ��Ѵ�.
            for ( sCurrSort = sParseTree->orderBy;
                  sCurrSort != NULL;
                  sCurrSort = sCurrSort->next)
            {
                if ( sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION )
                {
                    if ( QTC_HAVE_AGGREGATE( sCurrSort->sortColumn ) == ID_TRUE )
                    {
                        IDE_TEST( qmvQTC::isAggregateExpression( aStatement, 
                                                                 sSFWGH, 
                                                                 sCurrSort->sortColumn )
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
            }
            
            /* BUG-39332 group by�� ���� aggsDepth1�� �ִ� ���� ����� 1���̹Ƿ�
             * order by�� �����Ѵ�.
             * �׸��� order by������ �߰��� aggregation�� �����Ѵ�.
             */
            sParseTree->orderBy = NULL;
            
            sSFWGH->aggsDepth1 = sAggsDepth1;
        }
        else // if (sSFWGH->aggsDepth2 != NULL)
        {
            IDE_RAISE( ERR_NESTED_AGGR_WITHOUT_GROUP );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_CANNOT_HAVE_AGGREGATE)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_CANNOT_USE_AGG));
    }
    IDE_EXCEPTION(ERR_INVALID_ORDERBY_POS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_INVALID_POSITION_IN_ORDERBY));
    }
    IDE_EXCEPTION(ERR_NESTED_AGGR_WITHOUT_GROUP)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NESTED_AGGR_WITHOUT_GROUP));
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvOrderBy::validateSortWithSet(qcStatement * aStatement)
{

    qmsParseTree    * sParseTree;
    qmsSortColumns  * sCurrSort;
    qmsTarget       * sTarget;
    qtcNode         * sTargetNode;
    SInt              sTargetMaxPos = 0;
    SInt              sCurrTargetPos;
    qcuSqlSourceInfo  sqlInfo;

    IDU_FIT_POINT_FATAL( "qmvOrderBy::validateSortWithSet::__FT__" );

    sParseTree = (qmsParseTree*) aStatement->myPlan->parseTree;

    for (sTarget = sParseTree->querySet->target;
         sTarget != NULL;
         sTarget = sTarget->next)
    {
        sTargetMaxPos++;
    }

    for (sCurrSort = sParseTree->orderBy;
         sCurrSort != NULL;
         sCurrSort = sCurrSort->next)
    {
        IDE_TEST(qtc::getSortColumnPosition(sCurrSort, QC_SHARED_TMPLATE(aStatement))
                 != IDE_SUCCESS);

        if (sCurrSort->targetPosition < QMV_EMPTY_TARGET_POSITION)
        {
            // The possible format is only "ORDER BY col"
            // ORDER BY tbl.col => error
            // alias name -> search target -> set sortColumn
            // ERR_INVALID_ORDERBY_EXP_WITH_SET
            /* PROJ-2197 PSM Renewal */
            sCurrSort->sortColumn->lflag |= QTC_NODE_COLUMN_CONVERT_TRUE;
            IDE_TEST(qtc::estimate(
                         sCurrSort->sortColumn,
                         QC_SHARED_TMPLATE(aStatement),
                         aStatement,
                         sParseTree->querySet,
                         NULL,
                         NULL )
                     != IDE_SUCCESS);

            if ( ( sCurrSort->sortColumn->node.lflag & MTC_NODE_DML_MASK )
                 == MTC_NODE_DML_UNUSABLE )
            {
                sqlInfo.setSourceInfo( aStatement,
                                       & sCurrSort->sortColumn->position );
                IDE_RAISE( ERR_USE_CURSOR_ATTR );
            }

            // To Fix PR-9032
            // SET�� ������ ��� ORDER BY�� PRIOR �� ����� �� ����.
            if ( (sCurrSort->sortColumn->lflag & QTC_NODE_PRIOR_MASK)
                 == QTC_NODE_PRIOR_EXIST )
            {
                IDE_RAISE( ERR_INVALID_ORDERBY_POS );
            }
            else
            {
                // go go
            }

            // get position of target
            for (sTarget = sParseTree->querySet->target, sCurrTargetPos = 1;
                 sTarget != NULL;
                 sTarget = sTarget->next, sCurrTargetPos++)
            {
                // PROJ-2002 Column Security
                // target���� ���� �÷��� �ִ� ��� decrypt�Լ��� ������ �� �ִ�.
                if ( sTarget->targetColumn->node.module == &mtfDecrypt )
                {
                    sTargetNode = (qtcNode *)
                        sTarget->targetColumn->node.arguments;
                }
                else
                {
                    sTargetNode = sTarget->targetColumn;
                }

                if (sTargetNode->node.table ==
                    sCurrSort->sortColumn->node.table &&
                    sTargetNode->node.column ==
                    sCurrSort->sortColumn->node.column)
                {
                    sCurrSort->targetPosition = sCurrTargetPos;
                    break;
                }
            }

            if (sTarget == NULL)
            {
                if ( sCurrSort->targetPosition < 0 )
                {
                    // BUG-21807 
                    // position ������ �־����� ���� ���,
                    // order by Į���� target list���� ã�� ��������
                    // ���� �޽����� �˷��־�� ��
                    IDE_RAISE( ERR_NOT_EXIST_SELECT_LIST );
                }
                else
                {
                    // position ������ �־��� ���,
                    // position ������ �߸� �Ǿ�����
                    // ���� �޽����� �˷��־�� ��
                    IDE_RAISE( ERR_INVALID_ORDERBY_POS );
                }
            }
        }
        else if (sCurrSort->targetPosition > QMV_EMPTY_TARGET_POSITION &&
                 sCurrSort->targetPosition <= sTargetMaxPos)
        {
            IDE_TEST(transposePosValIntoTargetPtr(
                         sCurrSort,
                         sParseTree->querySet->target )
                     != IDE_SUCCESS);
        }
        else
        {
            IDE_RAISE( ERR_INVALID_ORDERBY_POS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NOT_EXIST_SELECT_LIST)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_NO_SELECTED_EXPRESSION));
    }
    IDE_EXCEPTION(ERR_INVALID_ORDERBY_POS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_INVALID_POSITION_IN_ORDERBY));
    }
    IDE_EXCEPTION(ERR_USE_CURSOR_ATTR);
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(
            ideSetErrorCode(qpERR_ABORT_QSV_USING_ATTR_IN_INAPPROPRIATE_CLAUSE,
                            sqlInfo.getErrMessage() ));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvOrderBy::transposePosValIntoTargetPtr(
    qmsSortColumns  * aSortColumn,
    qmsTarget       * aTarget )
{

    qmsTarget   * sTarget;
    qtcNode     * sTargetNode;
    SInt          sPosition = 0;
    qcNamePosition sOrgPosition; /* TASK-7219 */

    IDU_FIT_POINT_FATAL( "qmvOrderBy::transposePosValIntoTargetPtr::__FT__" );

    for (sTarget = aTarget; sTarget != NULL; sTarget = sTarget->next)
    {
        sPosition++;

        if (aSortColumn->targetPosition == sPosition)
        {
            break;
        }
    }

    IDE_TEST_RAISE(aSortColumn->targetPosition != sPosition || sTarget == NULL,
                   ERR_ORDERBY_WITH_INVALID_TARGET_POS);

    // PROJ-2415 Grouping Sets Clause
    // Grouping Sets Transform�� ���� Target�� �߰��� OrderBy�� Node��
    // Target�� �߰����� ���� OrderBy�� Position�� �ٶ� �� ����.
    IDE_TEST_RAISE( ( ( sTarget->targetColumn->lflag & QTC_NODE_GBGS_ORDER_BY_NODE_MASK ) ==
                      QTC_NODE_GBGS_ORDER_BY_NODE_TRUE ) &&
                    ( ( aSortColumn->sortColumn->lflag & QTC_NODE_GBGS_ORDER_BY_NODE_MASK ) !=
                      QTC_NODE_GBGS_ORDER_BY_NODE_TRUE ),
                    ERR_ORDERBY_WITH_INVALID_TARGET_POS );

    // PROJ-2002 Column Security
    // target���� ���� �÷��� �ִ� ��� decrypt�Լ��� ������ �� �ִ�.
    if ( sTarget->targetColumn->node.module == &mtfDecrypt )
    {
        sTargetNode = (qtcNode *)
            sTarget->targetColumn->node.arguments;
    }
    else
    {
        sTargetNode = sTarget->targetColumn;

        // PROJ-2179 ORDER BY������ �����Ǿ����� ǥ��
        sTarget->flag &= ~QMS_TARGET_ORDER_BY_MASK;
        sTarget->flag |= QMS_TARGET_ORDER_BY_TRUE;
    }

    /* TASK-7219 */
    SET_POSITION( sOrgPosition, aSortColumn->sortColumn->position );

    // set target expression
    idlOS::memcpy( aSortColumn->sortColumn,
                   sTargetNode,
                   ID_SIZEOF(qtcNode) );

    /* TASK-7219 */
    SET_POSITION( aSortColumn->sortColumn->position, sOrgPosition );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ERR_ORDERBY_WITH_INVALID_TARGET_POS)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QMV_INVALID_POSITION_IN_ORDERBY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmvOrderBy::disconnectConstantNode(
    qmsParseTree    * aParseTree )
{

    qmsSortColumns  * sPrevSort = NULL;
    qmsSortColumns  * sCurrSort;

    IDU_FIT_POINT_FATAL( "qmvOrderBy::disconnectConstantNode::__FT__" );

    // disconnect constant
    for (sCurrSort = aParseTree->orderBy;
         sCurrSort != NULL;
         sCurrSort = sCurrSort->next)
    {
        IDE_FT_ASSERT( sCurrSort->sortColumn != NULL );
        
        // PROJ-1413
        // constant�� �˻��ϴ� ��� ����
        if ( qtc::isConstNode4OrderBy( sCurrSort->sortColumn ) == ID_TRUE )
        {
            if (sPrevSort == NULL)
            {
                aParseTree->orderBy = sCurrSort->next;
            }
            else
            {
                sPrevSort->next = sCurrSort->next;
            }
        }
        else
        {
            sPrevSort = sCurrSort;
        }
    }

    return IDE_SUCCESS;
}
