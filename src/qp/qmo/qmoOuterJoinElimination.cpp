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
 * $Id: qmoOuterJoinElimination.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description : BUG-38339
 *     Outer Join Elimination
 * outer ������ on ���� ������ �����϶� ��� outer ���̺��� ���� null �� �ȴ�.
 * where ���̳� inner ���� ���� outer ���� �� ���̺��� �����Ѵٸ� outer ���� ������ ������ �ʿ䰡 ���� �ȴ�.
 *
 * is null �� ���� predicate ���� null ���� �ʿ��Ҷ��� �ִ�.
 * ���� or �����ڰ� ������ outer ���� �� ���̺��� �����Ҽ��� �ְ� ���Ҽ��� �ֱ� ������ ��쿡 ���� ����� Ʋ������.
 *
 * Implementation :
 *     1.  from �� Ʈ���� ��ȸ�� �ϸ鼭 ������ ���� �۾��� �����Ѵ�.
 *     2.  where ���� inner ������ on���� ������ø� ��� �����Ѵ�.
       2.1 Oracle Style Join �� ��쿡�� �������� �ʴ´�.
 *     2.2 ���� ������ ���( is null �� )���� ������ ������ÿ��� ���Ÿ� �Ѵ�.
 *     3.  left outer �����϶��� right �� ������ð� ������ ������ÿ� ���ԵǴ��� �˻��Ѵ�.
 *     3.1 ���Եȴٸ� inner join ���� �����Ѵ�.
 *     3.2 inner join �� �Ǿ����Ƿ� on ���� ������õ� �����Ѵ�.
 *
 **********************************************************************/

#include <ide.h>
#include <qcgPlan.h>
#include <qmoOuterJoinElimination.h>

IDE_RC
qmoOuterJoinElimination::doTransform( qcStatement * aStatement,
                                      qmsQuerySet * aQuerySet,
                                      idBool      * aChanged )
{
    idBool          sChanged = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinElimination::doTransform::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );

    //------------------------------------------
    // Outer Join Elimination ����
    //------------------------------------------

    if ( QCU_OPTIMIZER_OUTERJOIN_ELIMINATION == 1 )
    {
        IDE_TEST( doTransformQuerySet( aStatement,
                                       aQuerySet,
                                       & sChanged )
                  != IDE_SUCCESS );

        if ( sChanged == ID_TRUE )
        {
            qcgPlan::registerPlanProperty(
                aStatement,
                PLAN_PROPERTY_OPTIMIZER_OUTERJOIN_ELIMINATION );
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

    *aChanged = sChanged;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOuterJoinElimination::doTransformQuerySet( qcStatement * aStatement,
                                              qmsQuerySet * aQuerySet,
                                              idBool      * aChanged )
{
    qmsFrom * sFrom;
    qcDepInfo sFindDependencies;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinElimination::doTransformQuerySet::__FT__" );

    if ( aQuerySet->setOp == QMS_NONE )
    {
        for ( sFrom  = aQuerySet->SFWGH->from;
              sFrom != NULL;
              sFrom  = sFrom->next )
        {
            qtc::dependencyClear( &sFindDependencies );

            IDE_TEST( addWhereDep( (mtcNode*)aQuerySet->SFWGH->where,
                                   & sFindDependencies )
                      != IDE_SUCCESS );

            removeDep( (mtcNode*)aQuerySet->SFWGH->where, &sFindDependencies );

            IDE_TEST( doTransformFrom( aStatement,
                                       aQuerySet->SFWGH,
                                       sFrom,
                                       & sFindDependencies,
                                       aChanged )
                      != IDE_SUCCESS );
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
qmoOuterJoinElimination::doTransformFrom( qcStatement * aStatement,
                                          qmsSFWGH    * aSFWGH,
                                          qmsFrom     * aFrom,
                                          qcDepInfo   * aFindDependencies,
                                          idBool      * aChanged )
{
    IDU_FIT_POINT_FATAL( "qmoOuterJoinElimination::doTransformFrom::__FT__" );

    if ( aFrom->joinType == QMS_NO_JOIN )
    {
        // Nothing to do.
    }
    else if ( aFrom->joinType == QMS_INNER_JOIN )
    {
        IDE_TEST( addOnConditionDep( aFrom,
                                     aFrom->onCondition,
                                     aFindDependencies )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->left,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->right,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );
    }
    else if ( aFrom->joinType == QMS_LEFT_OUTER_JOIN )
    {
        if ( qtc::dependencyContains( aFindDependencies,
                                      &(aFrom->right->depInfo) ) == ID_TRUE )
        {
            aFrom->joinType = QMS_INNER_JOIN;

            // BUG-38375
            // ���� ���ǿ��� ��� inner join ���� ��ȯ�� �����ϴ�.
            // select * from t1 left outer join t2 on t1.i1=t2.i1
            //                  left outer join t3 on t2.i2=t3.i2
            //                  where t3.i3=1;
            IDE_TEST( addOnConditionDep( aFrom,
                                         aFrom->onCondition,
                                         aFindDependencies )
                      != IDE_SUCCESS );

            *aChanged = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->left,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->right,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->left,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );

        IDE_TEST( doTransformFrom( aStatement,
                                   aSFWGH,
                                   aFrom->right,
                                   aFindDependencies,
                                   aChanged )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOuterJoinElimination::addWhereDep( mtcNode   * aNode,
                                      qcDepInfo * aDependencies )
{
    mtcNode   * sNode;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinElimination::addWhereDep::__FT__" );

    for ( sNode  = aNode;
          sNode != NULL;
          sNode  = sNode->next )
    {
        if ( ( sNode->lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            IDE_TEST( addWhereDep( sNode->arguments,
                                   aDependencies )
                      != IDE_SUCCESS );
        }
        else
        {
            // BUG-38375 Oracle Style Join �� ��쿡�� �������� �ʴ´�.
            if ( ( ((qtcNode*)sNode)->lflag  & QTC_NODE_JOIN_OPERATOR_MASK )
                 == QTC_NODE_JOIN_OPERATOR_EXIST )
            {
                // Nothing to do.
            }
            else
            {
                IDE_TEST( qtc::dependencyOr( &( ((qtcNode*)sNode)->depInfo ),
                                             aDependencies,
                                             aDependencies )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoOuterJoinElimination::addOnConditionDep( qmsFrom   * aFrom,
                                            qtcNode   * aNode,
                                            qcDepInfo * aFindDependencies )
{
    qcDepInfo   sFindDependencies;

    IDU_FIT_POINT_FATAL( "qmoOuterJoinElimination::addOnConditionDep::__FT__" );

    IDE_DASSERT( aFrom->joinType == QMS_INNER_JOIN );

    if ( aNode != NULL )
    {
        qtc::dependencyClear( & sFindDependencies );

        qtc::dependencyAnd( & aFrom->depInfo,
                            & aNode->depInfo,
                            & sFindDependencies );

        IDE_TEST( qtc::dependencyOr( & sFindDependencies,
                                     aFindDependencies,
                                     aFindDependencies )
                  != IDE_SUCCESS );

        removeDep( (mtcNode*)aNode, aFindDependencies );
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
qmoOuterJoinElimination::removeDep( mtcNode     * aNode,
                                    qcDepInfo   * aFindDependencies )
{
    UInt        sCount;
    mtcNode   * sNode;
    qcDepInfo * sDependencies;

    for( sNode   = aNode;
         sNode  != NULL;
         sNode   = sNode->next )
    {
        // BUG-44692 subquery�� ������ OJE �� �����ϸ� �ȵ˴ϴ�.
        // OJE ����� ���� �����ϱ� ���ؼ��� ���� null �϶� true �� ���ϵǸ� �ȵ˴ϴ�.
        // ���������� ��� ���� ���ǿ� ���� true �� ���ϵɼ� �ֽ��ϴ�.
        // BUG-44781 anti join �� ������ OJE �� �����ϸ� �ȵ˴ϴ�.
        if ( ((sNode->lflag & MTC_NODE_EAT_NULL_MASK) == MTC_NODE_EAT_NULL_TRUE) ||
             ((sNode->lflag & MTC_NODE_OPERATOR_MASK) == MTC_NODE_OPERATOR_OR) ||
             (((sNode->lflag & MTC_NODE_OPERATOR_MASK) == MTC_NODE_OPERATOR_SUBQUERY) && (QCU_OPTIMIZER_BUG_44692 == 1)) ||
             (((((qtcNode*)sNode)->lflag & QTC_NODE_JOIN_TYPE_MASK) == QTC_NODE_JOIN_TYPE_ANTI) && (QCU_OPTIMIZER_BUG_44692 == 1)) )
        {
            sDependencies = &(((qtcNode*)sNode)->depInfo);

            for ( sCount = 0;
                  sCount < sDependencies->depCount;
                  sCount++ )
            {
                qtc::dependencyRemove( sDependencies->depend[sCount],
                                       aFindDependencies,
                                       aFindDependencies );
            }
        }
        else
        {
            removeDep( sNode->arguments, aFindDependencies );
        }
    }
}
