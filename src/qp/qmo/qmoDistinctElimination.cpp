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
 *****************************************************************************/

#include <ide.h>
#include <qcuProperty.h>
#include <qcgPlan.h>
#include <qmv.h>
#include <qmoDistinctElimination.h>

IDE_RC qmoDistinctElimination::doTransform( qcStatement * aStatement,
                                            qmsQuerySet * aQuerySet )
{
/***********************************************************************
 * Description : DISTINCT Keyword ����
 ***********************************************************************/

    idBool sChanged = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoDistinctElimination::doTransform::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aQuerySet != NULL );

    //------------------------------------------
    // ���� �˻�
    //------------------------------------------

    if ( QCU_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE == 1 )
    {
        if ( aQuerySet->setOp == QMS_NONE )
        {
            IDE_TEST( doTransformSFWGH( aStatement,
                                        aQuerySet->SFWGH,
                                        & sChanged )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( doTransform( aStatement, aQuerySet->left )
                      != IDE_SUCCESS );

            IDE_TEST( doTransform( aStatement, aQuerySet->right )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // environment�� ���
    //------------------------------------------

    qcgPlan::registerPlanProperty(
        aStatement,
        PLAN_PROPERTY_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoDistinctElimination::doTransformSFWGH( qcStatement * aStatement,
                                                 qmsSFWGH    * aSFWGH,
                                                 idBool      * aChanged )
{
/****************************************************************************************
 *
 *  Description : BUG-39522 / BUG-39665
 *                Target ��ü�� �̹� DISTINCT �ϴٸ� ���� DISTINCT Ű���带 �����Ѵ�.
 *
 *   DISTINCT Keyword�� Target �� �����ϸ�, �Ʒ� 2���� �Լ��� ���ʴ�� ȣ���Ѵ�.
 *
 *   - isDistTargetByGroup()
 *   - isDistTargetByUniqueIdx()
 *
 ****************************************************************************************/

    qmsFrom      * sFrom              = NULL;
    qmsParseTree * sParseTree         = NULL;
    idBool         sIsDistTarget      = ID_FALSE;
    idBool         sIsDistFromTarget  = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoDistinctElimination::doTransformSFWGH::__FT__" );

    /****************************************************************************
     * Bottom-up Distinct Elimination
     * FROM ���� �ִ� Inline View / Lateral View�� ���� ���� ó���Ѵ�.
     ****************************************************************************/

    for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
    {
        if ( sFrom->tableRef != NULL )
        {
            if ( sFrom->tableRef->view != NULL )
            {
                sParseTree = (qmsParseTree*)sFrom->tableRef->view->myPlan->parseTree;

                IDE_TEST( doTransform( sFrom->tableRef->view, sParseTree->querySet )
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
    }

    /****************************************************************************
     * DISTINCT ���� �Ұ����� ȯ������ Ȯ��
     ****************************************************************************/

    IDE_TEST_CONT( ( canTransform( aSFWGH ) == ID_FALSE ), NO_TRANSFORMATION );  

    /****************************************************************************
     * Group By ���� Target DISTINCT ���� ���� Ȯ��
     ****************************************************************************/

    IDE_TEST( isDistTargetByGroup( aStatement,
                                   aSFWGH,
                                   & sIsDistTarget )
              != IDE_SUCCESS );

    /****************************************************************************
     * Unique NOT NULL Index�� Target DISTINCT ���� ���� Ȯ��
     ****************************************************************************/

    if ( sIsDistTarget == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        // ANSI-Join���� ���� �� From ���� ȣ���ؾ� �Ѵ�.
        for ( sFrom = aSFWGH->from; sFrom != NULL; sFrom = sFrom->next )
        {
            IDE_TEST( isDistTargetByUniqueIdx( aStatement,
                                               aSFWGH,
                                               sFrom,
                                               & sIsDistFromTarget )
                      != IDE_SUCCESS );

            // ��� From�� DISTINCT �ؾ�, TARGET�� DISTINCT�� ������ �� �����Ƿ�,
            // ��� �� From�̶� DISTINCT���� ������ ������ �����Ѵ�.
            if ( sIsDistFromTarget == ID_FALSE )
            {
                break;
            }
            else
            {
                // ���� From�� DISTINCT�� ����
                // Nothing to do.
            }
        }

        if ( sIsDistFromTarget == ID_TRUE )
        {
            // ��� From���� Target DISTINCT�� �����Ѵ�.
            sIsDistTarget = ID_TRUE;
        }
        else
        {
            sIsDistTarget = ID_FALSE;
        }
    }

    /****************************************************************************
     * (3-3) Target DISTINCT ����
     ****************************************************************************/

    if ( sIsDistTarget == ID_TRUE )
    {
        aSFWGH->selectType = QMS_ALL;
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT( NO_TRANSFORMATION );

    *aChanged = sIsDistTarget;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmoDistinctElimination::canTransform( qmsSFWGH * aSFWGH )
{
/****************************************************************************************
 *
 *  Description : Distinct Elimination �Լ��� ������ �ʿ䰡 �ִ��� Ȯ���Ѵ�.
 *
 *  Implementation : ������ �����Ѵ�.
 *
 *    (1) Target�� DISTINCT Keyword�� �ִ���
 *    (2) GROUP BY ���� ROLLUP / CUBE / GROUPING SETS�� �����ϴ���
 *
 ****************************************************************************************/

    qmsConcatElement * sConcatElement   = NULL;
    idBool             sCanTransform    = ID_FALSE;

    // DISTINCT Keyword�� SFWGH�� �����ؾ� �Ѵ�.
    IDE_TEST_CONT( aSFWGH->selectType != QMS_DISTINCT, SKIP_TRANSFORMATION );

    // GROUPING SETS�� ������ ���� �ʾƾ� �Ѵ�.
    IDE_TEST_CONT( ( aSFWGH->lflag & QMV_SFWGH_GBGS_TRANSFORM_MASK ) 
                     != QMV_SFWGH_GBGS_TRANSFORM_NONE, 
                   SKIP_TRANSFORMATION );

    // ROLLUP / CUBE�� ������ ���� �ʾƾ� �Ѵ�.
    for ( sConcatElement = aSFWGH->group;
          sConcatElement != NULL;
          sConcatElement = sConcatElement->next )
    {
        IDE_TEST_CONT( ( sConcatElement->type == QMS_GROUPBY_ROLLUP ) ||
                       ( sConcatElement->type == QMS_GROUPBY_CUBE ),
                       SKIP_TRANSFORMATION );
    }

    // ���� �˻� �Ϸ�. ���� ����.
    sCanTransform = ID_TRUE;

    IDE_EXCEPTION_CONT( SKIP_TRANSFORMATION );

    return sCanTransform;
}

IDE_RC qmoDistinctElimination::isDistTargetByGroup( qcStatement  * aStatement,
                                                    qmsSFWGH     * aSFWGH,
                                                    idBool       * aIsDistTarget )
{
/****************************************************************************************
 *
 *  Description : BUG-39665
 *                Grouping Expression (���� Exp.) ���� Target�� �����ϴ� ���
 *                Target�� �̹� DISTINCT �ϸ�, DISTINCT Ű���带 ������ �� �ִ�.
 *
 *   (��) SELECT DISTINCT i1          FROM T1 GROUP BY i1;     -- ����
 *        SELECT DISTINCT i1, SUM(i2) FROM T1 GROUP BY i1;     -- ����
 *        SELECT DISTINCT i1*2        FROM T1 GROUP BY i1*2;   -- ����   (���� Expression)
 *        SELECT DISTINCT i1, i2      FROM T1 GROUP BY i1, i2; -- ����   (���� Target�� ����)
 *        SELECT DISTINCT i1          FROM T1 GROUP BY i1, i2; -- �Ұ��� (i2�� Target�� ����)
 *        SELECT DISTINCT i1*2        FROM T1 GROUP BY i1;     -- �Ұ��� (��Ȯ�� ��ġ���� ����)
 *                                    [*������ �����ص�, Expression�� ���� �׷��� ���� �� �ִ�.]
 *
 *   - �Ϲ� Grouping�� ���ؼ��� ó���Ѵ�.
 *
 *   - ROLLUP / CUBE / GROUPING SETS�� ó������ �ʴ´�.
 *     �̵� Grouping Exp. �� �ϳ���, �Ϲ� Grouping Exp. �� Base Column�� �ߺ��Ǵ� ��쿣
 *     DISTINCT ������ �� �� ���� �����̴�.
 *
 *     (��) SELECT DISTINCT i1, i2, SUM(i3) FROM T1 GROUP BY i1, ROLLUP( i1, i2 );
 *          >> ��� Target�� ����������, �Ϲ� Grouping Column i1�� ROLLUP���� �����Ƿ�
 *             i1�� �ߺ��Ǿ� ���´�.
 *
 ****************************************************************************************/

    qmsTarget        * sTarget         = NULL;
    qmsConcatElement * sGroup          = NULL;
    idBool             sGroupNodeFound = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoDistinctElimination::isDistTargetByGroup::__FT__" );

    // GROUP BY�� ���� ���, �ƹ� �ϵ� ���� �ʴ´�.
    IDE_TEST_CONT( aSFWGH->group == NULL, NO_GROUP_BY );

    /**********************************************************************************
     * Grouping Exp. �� Target DISTINCT ����
     *********************************************************************************/

    for ( sGroup = aSFWGH->group;
          sGroup != NULL;
          sGroup = sGroup->next )
    {
        // ���� Group�� ������ ���� �ʱ�ȭ
        sGroupNodeFound = ID_FALSE;

        switch ( sGroup->type )
        {
            case QMS_GROUPBY_NORMAL:

                // �Ϲ� Grouping Expression�� LIST ���·� �� �� ����.
                IDE_DASSERT( ( sGroup->arithmeticOrList->node.lflag & MTC_NODE_OPERATOR_MASK )
                               != MTC_NODE_OPERATOR_LIST );

                for ( sTarget = aSFWGH->target;
                      sTarget != NULL;
                      sTarget = sTarget->next )
                {
                    // Target�� Grouping Expression�� ��� passModule�� ������ �ִ�.
                    if ( sTarget->targetColumn->node.module == &qtc::passModule )
                    {
                        IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                               (qtcNode *)sGroup->arithmeticOrList,
                                                               (qtcNode *)sTarget->targetColumn->node.arguments,
                                                               & sGroupNodeFound )
                                  != IDE_SUCCESS );

                        if ( sGroupNodeFound == ID_TRUE )
                        {
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                    }
                    else
                    {
                        // Target�� Grouping Expression�� �ƴ� ���
                        // Nothing to do.
                    }
                }

                break;
            case QMS_GROUPBY_ROLLUP:
            case QMS_GROUPBY_CUBE:
            case QMS_GROUPBY_NULL:
                // ROLLUP / CUBE / GROUPING SETS �� ���ؼ� ó������ �ʴ´�.
                break;
            default:
                IDE_DASSERT(0);
                break;
        }

        if ( sGroupNodeFound == ID_FALSE )
        {
            // ���� �׷��� Target�� �������� �ʴ� ���
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    IDE_EXCEPTION_CONT( NO_GROUP_BY );

    // Target�� DISTINCT ���� ���� ����
    *aIsDistTarget = sGroupNodeFound;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoDistinctElimination::isDistTargetByUniqueIdx( qcStatement  * aStatement,
                                                        qmsSFWGH     * aSFWGH,
                                                        qmsFrom      * aFrom,
                                                        idBool       * aIsDistTarget )
{
/****************************************************************************************
 *
 *  Description : BUG-39522
 *
 *   FROM ���� qmsFrom �ϳ���, ���� ������ �����ϴ��� Ȯ���Ѵ�.
 *
 *     (1) �ش� qmsFrom���� ���� Target(��)�� ����
 *     (2) (1)�� Target(��) �߿���, �ش� qmsFrom�� Unique Index�� ���ϴ� Target(��)�� ����
 *     (3) (2)�� Target(��) ��� NOT NULL
 *
 *   �� �۾��� FROM ���� ��� qmsFrom�� ���� �����Ѵ�.
 *   ��� qmsFrom�� �� ������ �����ؾ�, Target�� DISTINCT�� ������ �� �ִ�.
 *
 *
 *   (��) CREATE TABLE T1 ( i1 INT NOT NULL, i2 INT, i3 INT,
 *                          PRIMARY KEY(i3) );
 *        CREATE UNIQUE INDEX T1_UIDX1 ON T1(i1);
 *        CREATE UNIQUE INDEX T1_UIDX2 ON T1(i2);
 *
 *        -- [T1.i1 : NOT NULL UNIQUE, T1.i2 : UNIQUE, T1.i3 : PRIMARY KEY]
 *
 *        SELECT DISTINCT i1     FROM T1; -- ����
 *        SELECT DISTINCT i2     FROM T1; -- �Ұ��� (NULL �ߺ� ����)
 *        SELECT DISTINCT i3     FROM T1; -- ����
 *        SELECT DISTINCT i1, i2 FROM T1; -- ����
 *                                           (i1�� DISTINCT �ϹǷ� (i1, i2) ������ DISTINCT)
 *
 *        SELECT DISTINCT i1     FROM T1 GROUP BY i1, i2; -- Group By�δ� �Ұ���, ���⼱ ����
 *        SELECT DISTINCT i2     FROM T1 GROUP BY i1, i2; -- Group By�δ� �Ұ���, ���⼭�� �Ұ���
 *        SELECT DISTINCT i1, i2 FROM T1 GROUP BY i1, i2; -- Group By�� �̹� ���� (Skipped)
 *
 *        CREATE TABLE T2 ( i1 INT NOT NULL, i2 INT NOT NULL,
 *                          FOREIGN KEY (i2) REFERENCES T1(i2) );
 *        CREATE UNIQUE INDEX T1_UIDX1 ON T1(i1);
 *
 *        -- [T2.i1 : NOT NULL UNIQUE, T2.i2 : NOT NULL FOREIGN KEY]
 *
 *        SELECT DISTINCT T1.i1               FROM T1, T2; -- �Ұ��� (T2�� Target�� ����)
 *        SELECT DISTINCT T1.i1, T2.i1        FROM T1, T2; -- ����
 *        SELECT DISTINCT T1.i1, T1.i2, T2.i1 FROM T1, T2; -- ����
 *        SELECT DISTINCT T1.i1, T2.i2        FROM T1, T2; -- �Ұ���
 *                                                            (T2.i2�� FK�� �ߺ��� �� ����)
 *
 *
 *   Note : ������ ���� DISTINCT�� ������ �� ����.
 *
 *   - qmsFrom�� ANSI Join Tree�̰�, �����̶� DISTINCT�� ������� ���ϴ� ���
 *
 *   - Composite Unique Index�� �ִ� Key Column '�Ϻ�'�� Target�� �����ϴ� ���
 *     (���ǿ��� ���ߵ�, Key Column ��� Target �����ؾ� ����)
 *
 *   - From �� Inline View, Lateral View, Subquery Factoring (WITH)�� ���� ���
 *
 *   - Partitioned Table qmsFrom ����, LOCALUNIQUE Index�� Ȯ������ �ʴ´�.
 *     >> qmsFrom�� Ư�� Partition�� ��쿡�� Ȯ���Ѵ�.
 *
****************************************************************************************/

    qmsTarget     * sTarget          = NULL;
    qcmColumn     * sFromColumn      = NULL;
    qcmTableInfo  * sTableInfo       = NULL;
    qcmIndex      * sIndex           = NULL;
    qtcNode       * sTargetNode      = NULL;
    mtcColumn     * sTargetColumn    = NULL;
    mtcColumn     * sKeyColumn       = NULL;
    idBool          sHasTarget       = ID_FALSE;
    idBool          sIsParttition    = ID_FALSE;
    idBool          sIsDistTarget    = ID_FALSE;
    idBool          sKeyColFound     = ID_FALSE;
    idBool          sIsNotNullUnique = ID_FALSE;
    UInt            i, j;

    IDU_FIT_POINT_FATAL( "qmoDistinctElimination::isDistTargetByGroup::__FT__" );

    // ���� From Object�� �ƴ� ���, LEFT/RIGHT�� ���� ��� ȣ��
    if ( aFrom->joinType != QMS_NO_JOIN )
    {
        IDE_TEST( isDistTargetByUniqueIdx( aStatement,
                                           aSFWGH,
                                           aFrom->left,
                                           & sIsDistTarget )
                  != IDE_SUCCESS );

        // LEFT���� DISTINCT�� ������ �� ����, DistIdx�� �������� �ʾƵ� �Ǹ�
        // �ش� From Object�� DISTINCT�� ������ �� �����Ƿ� ��ٷ� ���� (��ȯ�� : FALSE)
        IDE_TEST_CONT( sIsDistTarget == ID_FALSE, NORMAL_EXIT );

        IDE_TEST( isDistTargetByUniqueIdx( aStatement,
                                           aSFWGH,
                                           aFrom->right,
                                           & sIsDistTarget )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( aFrom->tableRef != NULL );

        /**********************************************************************************
         * Partition�� ���� Ž���ϴ� ���, LOCALUNIQUE Index�� ����
         *********************************************************************************/

        if ( aFrom->tableRef->partitionRef != NULL )
        {
            sTableInfo    = aFrom->tableRef->partitionRef->partitionInfo;
            sIsParttition = ID_TRUE;
        }
        else
        {
            sTableInfo    = aFrom->tableRef->tableInfo;
            sIsParttition = ID_FALSE;
        }


        /**********************************************************************************
         * ���� From�� ���� Target�� �����ϴ��� Ȯ��
         *********************************************************************************/

        for ( sFromColumn = sTableInfo->columns;
              sFromColumn != NULL;
              sFromColumn = sFromColumn->next )
        {
            for ( sTarget = aSFWGH->target;
                  sTarget != NULL;
                  sTarget = sTarget->next )
            {
                // Target�� Grouping Expression�� ��� passModule�� ������ �ִ�.
                if ( sTarget->targetColumn->node.module == &qtc::passModule )
                {
                    sTargetNode = (qtcNode *)sTarget->targetColumn->node.arguments;
                }
                else
                {
                    sTargetNode = sTarget->targetColumn;
                }

                if ( QTC_IS_COLUMN( aStatement, sTargetNode ) == ID_TRUE )
                {
                    sTargetColumn = QTC_STMT_COLUMN( aStatement, sTargetNode );

                    if ( sFromColumn->basicInfo->column.id == sTargetColumn->column.id )
                    {
                        sHasTarget = ID_TRUE;
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

            } /* End of loop : ��� Target Ž�� �Ϸ� */

            if ( sHasTarget == ID_TRUE )
            {
               break;
            }
            else
            {
                // Nothing to do.
            }

        } /* End of loop : ���� From Object�� ��� Column Ž�� �Ϸ� */

        // ���� From�� �����ִ� Target�� �������� ������
        // DISTINCT�� ������ �� �����Ƿ� ��ٷ� ���� (��ȯ�� : FALSE)
        IDE_TEST_CONT( sHasTarget == ID_FALSE, NORMAL_EXIT );

        /**********************************************************************************
         * Unique Index�� DISTINCT ����
         *********************************************************************************/

        for ( i = 0; i < sTableInfo->indexCount; i++ )
        {
            sIndex = & sTableInfo->indices[i];

            // Unique Index?
            if ( sIndex->isUnique == ID_FALSE )
            {
                // Partition�� ���� Ž���ϴ� ���, ���� Index�� LocalUnique ��� Unique ����
                if ( ( sIsParttition == ID_TRUE ) && ( sIndex->isLocalUnique == ID_TRUE ) )
                {
                    // Nothing to do.
                }
                else
                {
                    // Unique���� ���� Index��� �ٸ� Index�� Ž��
                    continue;
                }
            }
            else
            {
                // Nothing to do.
            }

            // Not Null Unique Index?
            sIsNotNullUnique = ID_TRUE;

            for ( j = 0; j < sIndex->keyColCount; j++ )
            {
                sKeyColumn = & sIndex->keyColumns[j];

                if ( ( sKeyColumn->flag & MTC_COLUMN_NOTNULL_MASK ) ==
                       MTC_COLUMN_NOTNULL_FALSE )
                {
                    sIsNotNullUnique = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if ( sIsNotNullUnique == ID_FALSE )
            {
                continue;
            }
            else
            {
                // Nothing to do.
            }

            /**********************************************************************************
             * Unique Index�� ��� Key�� Target�� �ִ��� �˻�
             *********************************************************************************/

            // ������ ���� �ʱ�ȭ
            sIsDistTarget = ID_TRUE;

            for ( j = 0; j < sIndex->keyColCount; j++ )
            {
                sKeyColumn   = & sIndex->keyColumns[j];
                sKeyColFound = ID_FALSE;

                for ( sTarget = aSFWGH->target;
                      sTarget != NULL;
                      sTarget = sTarget->next )
                {
                    // Target�� Grouping Expression�� ��� passModule�� ������ �ִ�.
                    if ( sTarget->targetColumn->node.module == &qtc::passModule )
                    {
                        sTargetNode = (qtcNode *)sTarget->targetColumn->node.arguments;
                    }
                    else
                    {
                        sTargetNode = sTarget->targetColumn;
                    }

                    if ( QTC_IS_COLUMN( aStatement, sTargetNode ) == ID_TRUE )
                    {
                        sTargetColumn = QTC_STMT_COLUMN( aStatement, sTargetNode );

                        // ���� Key Column�� ��ġ�ϴ� Target�� ã�´�.
                        // ��ġ�ϴ� Key Column�� NOT Null�� �ƴ϶��, Key Column�� ��ġ�ص� �����Ѵ�.

                        // Key Column�� NOT NULL�̶� Target�� NULL�� �� �ִµ� (Outer Join)
                        // �� ��쿡�� Key Column�� NOT NULL�̸� DISTINCT�� ����ȴ�.
                        if ( ( sKeyColumn->column.id == sTargetColumn->column.id ) &&
                             ( sKeyColumn->flag & MTC_COLUMN_NOTNULL_MASK ) ==
                             MTC_COLUMN_NOTNULL_TRUE )
                        {
                            sKeyColFound = ID_TRUE;
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

                // KeyColumn �� �ϳ��� Target�� �����Ƿ�, �ش� Index�� DISTINCT�� �� �� ����.
                if ( sKeyColFound == ID_FALSE )
                {
                    sIsDistTarget = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }

            } /* End of loop : ���� Unique Index�� ��� Key Column Ž�� �Ϸ� */

            // �ش� Unique Index�� DISTINCT�� ������ �� �ִٸ� ��ٷ� ���� (��ȯ�� : TRUE)
            IDE_TEST_CONT( sIsDistTarget == ID_TRUE, NORMAL_EXIT );

        } /* End of loop : ��� Index Ž�� �Ϸ� */
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    *aIsDistTarget = sIsDistTarget;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

