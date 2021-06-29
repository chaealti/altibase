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
 * $Id: qmoCheckViewColumnRef.cpp 89448 2020-12-07 04:24:14Z cory.chae $
 *
 * PROJ-2469 Optimize View Materialization
 *
 * View Column�� ���� ���� ����( qmsTableRef->viewColumnRefList )��
 * ���� Query Block���� ���� Top-Down���� ���� �ϸ鼭
 * ���� ���� ������ �ʾ�, ���� �Ǿ ����� ������ ��ġ�� �ʴ� View�� Target�� ����
 * ������� ����( QMS_TARGET_IS_USELESS_TRUE )���� flagó���� �Ѵ�.
 *
 * ex ) SELECT i1 << i1�� ���ȴ�.
 *        FROM (
 *               SELECT i1, i2
 *                 FROM (   ^ ������ �ʴ´ٰ� ǥ��
 *                        SELECT i1, i2, i3
 *                          FROM T1  ^   ^ ������ �ʴ´ٰ� ǥ��
 *                      )
 *              );
 *
 * ������ �ʴ´ٰ� ǥ�� �� View�� Target Column��
 * qmoOneNonPlan::initPROJ()
 * qmoOneMtrPlan::initVMTR()
 * qmoOneMtrPlan::initCMTR() �Լ����� Result Descriptor�� ���� �� �� �ݿ��Ǿ�,
 *                                  ( createResultFromQuerySet() )
 * �ش� Node�� calculate ���� �ʰų�, Materialized Node�� Minimize �ؼ�
 * ��������� Dummyȭ ��Ų��.
 *
 * <<<<<<<<<< View�� Target�� ���࿡�� �������� �ʴ� ���( ���ܻ��� ) >>>>>>>>>>
 *
 * 1. SELECT Clause���� �����Ѵ�. - DML( INSERT/UPDATE/DELETE )����. ( Subquery�� �����Ѵ�. )
 *
 * 2. Set Operator Type�� NONE �̰ų� UNION_ALL( BAG OPERATION ) �� ���� �����Ѵ�.
 *    ( Set ������ �� ��ü�� Target�� ��� ���ǹ��ϴ�. )
 *
 * 3. DISTINCT �� �ִ°�� �������� �ʴ´�. ( ��� Target�� ���ǹ��ϴ�. )
 *
 **********************************************************************/

#include <qmoCheckViewColumnRef.h>
#include <qmsParseTree.h>
#include <qcuProperty.h>
#include <qmv.h>

IDE_RC
qmoCheckViewColumnRef::checkViewColumnRef( qcStatement      * aStatement,
                                           qmsColumnRefList * aParentColumnRef,
                                           idBool             aAllColumnUsed )
{
/***********************************************************************
 *
 * Description :
 *     SELECT Statement�� Transform�� �߻� ��ų �� �ִ�
 *     ��� Validation�� ������ ����Ǿ�, �ֻ��� Query Block ����
 *     ������ Query Block ���� ��ȸ�ϸ�, ������ ������ �ʴ�
 *     View Target Column�� ã�Ƴ� flag ó���Ѵ�.
 *
 * Implementation :
 *     1. SELECT Statement �� Parse Tree�� �������
 *        Query Set�� ���Ͽ� ���ʿ���( ����Query Block���� ������� �ʴ� )
 *        View Target Column�� ã�� �Լ��� ȣ���Ѵ�.
 *
 * Arguments :
 *     aStatement       ( �ʱ� Statement )
 *     aParentColumnRef ( ���� Query Block�� View Column ���� ����Ʈ )
 *     aAllColumnUsed   ( ��� Target Column�� ��ȿ�ؾ� �ϴ��� ���� )
 *
 ***********************************************************************/
    qmsParseTree   * sParseTree;
    idBool           sAllColumnUsed = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoCheckViewColumnRef::checkViewColumnRef::__FT__" );

    IDE_DASSERT( aStatement != NULL );

    // PLAN PROPERTY : __OPTIMIZER_VIEW_TARGET_ENABLE = 1 �϶��� �����Ѵ�.
    if ( QCU_OPTIMIZER_VIEW_TARGET_ENABLE == 1 )
    {
        sParseTree = ( qmsParseTree * )aStatement->myPlan->parseTree;

        // BUG-43669
        // View Merging�� ���� �� ���� view target�� ��� ����ϴ� ������ ó���Ѵ�.
        // ���� ���������� tableRef->isNewAliasName���� viewMerging ���θ� Ȯ�� �ߴµ�,
        // From�� type�� (OUTER)JOIN�� ��� tableRef�� ��� viewMerging�� �ɷ����� �ʾҴ�.
        // �̸� ParseTree�� isTransformed�� ���� �Ǵ��ϴ� �� ���� �����Ѵ�.
        if ( sParseTree->isTransformed == ID_TRUE )
        {
            sAllColumnUsed = ID_TRUE;
        }
        else
        {
            sAllColumnUsed = aAllColumnUsed;
        }

        /**********************************************************************
         * ������ ����( aAllTargetUsed )��
         * Top Query Block �̰ų�, ������ Set ������ �����Ͽ�
         * ��� Target�� ���Ǵ� ��� TRUE �̰�,
         * ������ Set������ ���� View Query Block �� ��� FALSE �̴�.
         **********************************************************************/
        IDE_TEST( checkQuerySet( sParseTree->querySet,
                                 aParentColumnRef,
                                 sParseTree->orderBy,
                                 sAllColumnUsed )
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
qmoCheckViewColumnRef::checkQuerySet( qmsQuerySet      * aQuerySet,
                                      qmsColumnRefList * aParentColumnRef,
                                      qmsSortColumns   * aOrderBy,
                                      idBool             aAllColumnUsed )
/***********************************************************************
 *
 * Description :
 *     Query Set�� Target�� ����, ���� Query Block�� ��������( aParentColumnRef )��
 *     ���ʷ�, ���ŵǾ ����� ������ �����ʴ� Column�� ���� flag ó���Ѵ�.
 *
 * Implementation :
 *     1. ��� Target�� ��ȿ���� Ȯ��
 *     2. �ʿ���� View Target�� ã�Ƴ��� flagó��
 *     3. ���� View�� ���� ó���� ���� �Լ� ȣ��
 *     4. SET�� ���ؼ� LEFT, RIGHT ���ȣ��
 *
 * Arguments :
 *     aQuerySet
 *     aParentColumnRef ( ���� Query Block�� View Column ���� ����Ʈ )
 *     aOrderBy
 *     aAllColumnUsed   ( ��� Target Column�� ��ȿ�ؾ� �ϴ��� ���� )
 *
 ***********************************************************************/
{
    qmsTarget        * sTarget;
    qmsFrom          * sFrom;
    idBool             sAllColumnUsed;

    IDU_FIT_POINT_FATAL( "qmoCheckViewColumnRef::checkQuerySet::__FT__" );

    /********************************************
     * 1. ��� Column�� ����ؾ� �ϴ��� Ȯ��
     ********************************************/
    switch ( aQuerySet->setOp )
    {
        case QMS_NONE :
            if ( aQuerySet->SFWGH->selectType == QMS_DISTINCT )
            {
                /**********************************************************************************
                 *
                 * BUG-40893
                 * TARGET�� DISTINCT�� �־ ��� Column�� �ʿ� �� ���
                 *
                 * ex ) SELECT i1
                 *        FROM (
                 *               SELECT DISTINCT i1, i2<<< DISTINCTION�� ���� ��� Column�� �ʿ��ϴ�.
                 *                 FROM ( SELECT i1, i2, i3
                 *                        FROM T1
                 *                        LIMIT 10 ) );
                 *
                 ***********************************************************************************/
                sAllColumnUsed = ID_TRUE;
            }
            else
            {
                sAllColumnUsed = aAllColumnUsed;

                // BUG-48090 ���� QuerySet ���� Ž���Ͽ� With View �÷��װ� �����Ǿ� ������, 
                // ��� �÷��� ��ȿ�ϵ��� �����Ѵ�.
                if ( sAllColumnUsed == ID_FALSE )
                {
                    IDE_TEST( checkWithViewFlagFromQuerySet( aQuerySet,
                                                             &sAllColumnUsed /* sIsWithView */ )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothing to do */
                }
            }
            break;

        case QMS_UNION_ALL :
            sAllColumnUsed = aAllColumnUsed;
            break;

        default :
            /**********************************************************************************
             *
             * Set �������� ��� Column�� �ʿ� �� ���
             *
             * ex ) SELECT i1
             *        FROM (
             *               SELECT i1, i2, i3
             *                 FROM T1  ^   ^ ���� Query block���� ������ ������,
             *               INTERSECT        INTERSECT�� ����� ������ �ֱ� ������ �ʿ��ϴ�.
             *               SELECT i1, i2, i3
             *                 FROM T1  ^   ^
             *              );
             *
             ***********************************************************************************/
            sAllColumnUsed = ID_TRUE;
            break;
    }

    /**************************************************
     * 2. �ʿ���� View Target�� ã�Ƴ��� flag ó��
     **************************************************/
    // ��� Column�� ����ϴ� ��찡 �ƴҶ�
    if ( sAllColumnUsed == ID_FALSE )
    {
        // ���� Query Block �Ǵ� Order By���� Target�� �ϳ��� �����Ǵ� ���
        if ( ( aParentColumnRef != NULL ) || ( aOrderBy != NULL ) )
        {
            IDE_TEST( checkUselessViewTarget( aQuerySet->target,
                                              aParentColumnRef,
                                              aOrderBy ) != IDE_SUCCESS );
        }
        else
        {
            /**********************************************************
             * �������� View�� Target�� �ϳ��� �������� ���� ���
             **********************************************************/
            for ( sTarget  = aQuerySet->target;
                  sTarget != NULL;
                  sTarget  = sTarget->next )
            {
                if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) == QMS_TARGET_IS_USELESS_UNKNOWN )
                {
                    sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
                    sTarget->flag |=  QMS_TARGET_IS_USELESS_TRUE;
                }
                else
                {
                    // Nohting to do.
                }
            }
        }
    }
    else
    {
        /**********************************************************
         * View Target�� ��� ��ȿ�� ���
         **********************************************************/
        for ( sTarget  = aQuerySet->target;
              sTarget != NULL;
              sTarget  = sTarget->next )
        {
            // UNKNOWN->FALSE, TRUE->FALSE
            if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) != QMS_TARGET_IS_USELESS_FALSE )
            {
                sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
                sTarget->flag |=  QMS_TARGET_IS_USELESS_FALSE;
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    /**************************************************
     * 3. From�� ���� ó��
     **************************************************/
    if ( aQuerySet->setOp == QMS_NONE )
    {
        sFrom = aQuerySet->SFWGH->from;

        for ( ; sFrom != NULL; sFrom = sFrom->next )
        {
            IDE_TEST( checkFromTree( sFrom,
                                     aParentColumnRef,
                                     aOrderBy,
                                     sAllColumnUsed ) != IDE_SUCCESS );
        }
    }
    else // SET OPERATORS
    {
        // Recursive Call
        IDE_TEST( checkQuerySet( aQuerySet->left,
                                 aParentColumnRef,
                                 aOrderBy,
                                 sAllColumnUsed )
                  != IDE_SUCCESS );

        IDE_TEST( checkQuerySet( aQuerySet->right,
                                 aParentColumnRef,
                                 aOrderBy,
                                 sAllColumnUsed )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCheckViewColumnRef::checkFromTree( qmsFrom          * aFrom,
                                      qmsColumnRefList * aParentColumnRef,
                                      qmsSortColumns   * aOrderBy,
                                      idBool             aAllColumnUsed )
{
/***********************************************************************
 *
 * Description :
 *     ������ View Column ��������( aParentColumnRef )�� ���Ͽ�
 *     �ڽ��� View Column ������������ ���ʿ��� Target Column�� ǥ���ϰ�,
 *     ���� View�� ���ؼ� �ʱ��Լ��� checkViewColumnRef() �� ��ͼ����Ѵ�.
 *
 * Implementation :
 *     1. ���� ���������� ������� ����, ���� ���������� Target Column�� ������ ǥ��
 *     2. ���� View�� ���ؼ� �ʱ�����Լ� qmoCheckViewColumnRef() ȣ��
 *     3. SameView�� ���ؼ� �ʱ�����Լ� qmoCheckViewColumnRef() ȣ��
 *     4. JOIN Tree��ȸ
 *
 * Arguments :
 *     aFrom
 *     aParentColumnRef ( ���� Query Block�� View Column ���� ����Ʈ )
 *     aOrderBy
 *     aAllColumnUsed   ( ��� Target Column�� ��ȿ�ؾ� �ϴ��� ���� )
 *
 ***********************************************************************/
    qmsTableRef      * sTableRef;

    IDU_FIT_POINT_FATAL( "qmoCheckViewColumnRef::checkFromTree::__FT__" );

    if ( aFrom->joinType == QMS_NO_JOIN )
    {
        sTableRef = aFrom->tableRef;

        // ������ View�� ���� ��� �������� �ʴ´�.
        if ( sTableRef->view != NULL )
        {
            /*
             * Top Query Block, Merged View �Ǵ� Set Operation���� ���� ��� Column�� ��ȿ�ϸ� �������� �ʴ´�.
             */
            if ( aAllColumnUsed == ID_FALSE )
            {
                IDE_TEST( checkUselessViewColumnRef( sTableRef,
                                                     aParentColumnRef,
                                                     aOrderBy )
                          != IDE_SUCCESS );

            }
            else
            {
                // Nothing to do.
            }

            // BUG-48090 sTableRef�� With View �÷��װ� �����Ǿ� ������, 
            // sTableRef�� ���� View�� ����ȭ�� ��, ��� �÷��� ��ȿ�ϵ��� �����Ѵ�.
            if ( ( sTableRef->flag & QMS_TABLE_REF_WITH_VIEW_MASK ) 
                 == QMS_TABLE_REF_WITH_VIEW_TRUE )
            {
                IDE_TEST( checkViewColumnRef( sTableRef->view,
                                              NULL,
                                              ID_TRUE )
                          != IDE_SUCCESS );                
            }
            else
            {
                IDE_TEST( checkViewColumnRef( sTableRef->view,
                                              sTableRef->viewColumnRefList,
                                              ID_FALSE )
                          != IDE_SUCCESS );
            }

            // Same View Reference �� ���� ��� ó��
            if ( sTableRef->sameViewRef != NULL )
            {
                /* BUG-47787 recursive with ������ ��ø�ǰ� CASE WHEN��
                 * Subquery�� ���� ��� FATAL
                 */
                if ( sTableRef->sameViewRef->view != NULL )
                {
                    IDE_TEST( checkViewColumnRef( sTableRef->sameViewRef->view,
                                                  sTableRef->viewColumnRefList,
                                                  ID_FALSE )
                              != IDE_SUCCESS );
                }
                else
                {
                    /* Nothingt to do */
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
        // Recursive Call For From Tree
        IDE_TEST( checkFromTree( aFrom->left,
                                 aParentColumnRef,
                                 aOrderBy,
                                 aAllColumnUsed )
                  != IDE_SUCCESS );

        IDE_TEST( checkFromTree( aFrom->right,
                                 aParentColumnRef,
                                 aOrderBy,
                                 aAllColumnUsed )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoCheckViewColumnRef::checkUselessViewTarget( qmsTarget        * aTarget,
                                               qmsColumnRefList * aParentColumnRef,
                                               qmsSortColumns   * aOrderBy )
{
/***********************************************************************
 *
 * Description :
 *     �ʿ���� View Target�� ã�Ƴ��� flagó���Ѵ�.
 *
 * Implementation :
 *
 * Arguments :
 *
 ***********************************************************************/
    qmsTarget        * sTarget;
    qmsSortColumns   * sOrderBy;
    qmsColumnRefList * sParentColumnRef;
    UShort             sTargetOrder;
    idBool             sIsFound;

    IDU_FIT_POINT_FATAL( "qmoCheckViewColumnRef::checkUselessViewTarget::__FT__" );

    for ( sTarget  = aTarget, sTargetOrder = 0;
          sTarget != NULL;
          sTarget  = sTarget->next, sTargetOrder++ )
    {
        sIsFound = ID_FALSE;

        for ( sParentColumnRef  = aParentColumnRef;
              sParentColumnRef != NULL;
              sParentColumnRef  = sParentColumnRef->next )
        {
            if ( ( sParentColumnRef->viewTargetOrder == sTargetOrder ) &&
                 ( sParentColumnRef->isUsed == ID_TRUE ) )
            {
                // �������� ��� ��
                sIsFound = ID_TRUE;

                // UNKNOWN->FALSE, TRUE->FALSE
                if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) != QMS_TARGET_IS_USELESS_FALSE )
                {
                    sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
                    sTarget->flag |=  QMS_TARGET_IS_USELESS_FALSE;
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

        if ( sIsFound == ID_FALSE )
        {
            for ( sOrderBy  = aOrderBy;
                  sOrderBy != NULL;
                  sOrderBy  = sOrderBy->next )
            {
                // ����� Indicator�� ��ϵ� SortNode�� targetPosition�� ���õȴ�.
                // OrderBy���� ��� �� Target�� ���Ŵ�󿡼� �����Ѵ�.
                if ( sOrderBy->targetPosition == ( sTargetOrder + 1 ) )
                {
                    /********************************
                     *
                     * CASE :
                     *
                     * SELECT I1, I2
                     *   FROM ( SELECT I1, I2, I3
                     *            FROM T1
                     *        ORDER BY 3 )
                     *
                     *********************************/
                    sIsFound = ID_TRUE;

                    // UNKNOWN->FALSE, TRUE->FALSE
                    if ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) != QMS_TARGET_IS_USELESS_FALSE )
                    {
                        sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
                        sTarget->flag |=  QMS_TARGET_IS_USELESS_FALSE;
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

        // ���� Query Block���� ������ �ʴ� Column
        if ( ( sIsFound == ID_FALSE ) &&
             ( ( sTarget->flag & QMS_TARGET_IS_USELESS_MASK ) == QMS_TARGET_IS_USELESS_UNKNOWN ) )
        {
            sTarget->flag &= ~QMS_TARGET_IS_USELESS_MASK;
            sTarget->flag |=  QMS_TARGET_IS_USELESS_TRUE;
        }
        else
        {
            // Nothing to do.
        }

    } // End for loop

    return IDE_SUCCESS;
}

IDE_RC
qmoCheckViewColumnRef::checkUselessViewColumnRef( qmsTableRef      * aTableRef,
                                                  qmsColumnRefList * aParentColumnRef,
                                                  qmsSortColumns   * aOrderBy )
{
/***********************************************************************
 *
 * Description :
 *     �ʿ���� View Column Ref�� ã�Ƴ��� flagó���Ѵ�.
 *
 * Implementation :
 *
 * Arguments :
 *
 ***********************************************************************/

    qmsColumnRefList * sColumnRef;
    qmsColumnRefList * sParentColumnRef;
    qmsSortColumns   * sOrderBy;
    idBool             sIsFound;

    IDU_FIT_POINT_FATAL( "qmoCheckViewColumnRef::checkUselessViewColumnRef::__FT__" );

    for ( sColumnRef  = aTableRef->viewColumnRefList;
          sColumnRef != NULL;
          sColumnRef  = sColumnRef->next )
    {
        // Target ���� ������ Column���� ������� �Ѵ�.
        if ( sColumnRef->usedInTarget == ID_TRUE )
        {
            sIsFound = ID_FALSE;

            for ( sParentColumnRef  = aParentColumnRef;
                  sParentColumnRef != NULL;
                  sParentColumnRef  = sParentColumnRef->next )
            {
                if ( ( sColumnRef->targetOrder  == sParentColumnRef->viewTargetOrder ) &&
                     ( sParentColumnRef->isUsed == ID_TRUE ) )
                {
                    // ���� Query Block���� ��� �ȴ�.
                    sIsFound = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing to do.
                }
            }

            if ( sIsFound == ID_FALSE )
            {
                for ( sOrderBy  = aOrderBy;
                      sOrderBy != NULL;
                      sOrderBy  = sOrderBy->next )
                {
                    if ( ( sColumnRef->targetOrder + 1 ) == sOrderBy->targetPosition )
                    {
                        // Order By���� ��� �ȴ�.
                        sIsFound = ID_TRUE;
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

            if ( sIsFound == ID_FALSE )
            {
                /****************************************************************************
                 *
                 * ���� Query Block���� ������ �ʴ�,
                 * Target���� ��ϵ� Column�� ���ؼ�
                 * ������� �������� ǥ���Ѵ�.
                 *
                 ****************************************************************************/
                sColumnRef->isUsed = ID_FALSE;
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

    } // End loop

    return IDE_SUCCESS;
}

IDE_RC qmoCheckViewColumnRef::checkWithViewFlagFromQuerySet( qmsQuerySet  * aQuerySet,
                                                             idBool       * aIsWithView )
{
    qmsFrom  * sFrom;
 
    if ( *aIsWithView == ID_FALSE )
    {
        if ( aQuerySet->setOp == QMS_NONE )
        {
            sFrom = aQuerySet->SFWGH->from;

            for ( ; sFrom != NULL; sFrom = sFrom->next )
            {
                IDE_TEST( checkWithViewFlagFromFromTree( sFrom, aIsWithView )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // Recursive Call
            IDE_TEST( checkWithViewFlagFromQuerySet( aQuerySet->left,
                                                     aIsWithView )
                      != IDE_SUCCESS );
                      
            IDE_TEST( checkWithViewFlagFromQuerySet( aQuerySet->right,
                                                     aIsWithView )
                      != IDE_SUCCESS );
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

IDE_RC qmoCheckViewColumnRef::checkWithViewFlagFromFromTree( qmsFrom      * aFrom,
                                                             idBool       * aIsWithView )
{
    qmsTableRef  * sTableRef;
 
    if ( *aIsWithView == ID_FALSE )
    {
        if ( aFrom->joinType == QMS_NO_JOIN )
        {
            if ( aFrom->tableRef != NULL )
            {
                sTableRef = aFrom->tableRef;

                if ( ( sTableRef->flag & QMS_TABLE_REF_WITH_VIEW_MASK ) 
                     == QMS_TABLE_REF_WITH_VIEW_TRUE )
                {
                    *aIsWithView = ID_TRUE;
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
            // Recursive Call For From Tree
            IDE_TEST( checkWithViewFlagFromFromTree( aFrom->left,
                                                     aIsWithView )
                      != IDE_SUCCESS );
                      
            IDE_TEST( checkWithViewFlagFromFromTree( aFrom->right,
                                                     aIsWithView )
                      != IDE_SUCCESS );
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
