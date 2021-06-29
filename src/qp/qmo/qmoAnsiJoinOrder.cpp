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
 * $Id$
 *
 * Description :
 *     ANSI Join Ordering
 *
 *     BUG-34295 ANSI style ������ join ordering 
 *     �Ϻ� ���ѵ� ���ǿ��� ANSI style join ���� inner join �� �и��Ͽ�
 *     join order �� optimizer �� �����ϵ��� �Ѵ�.
 *
 *     TODO : ��� ���ǿ��� cost �� ����Ͽ� ��� join �� join order ��
 *            �����ϵ��� �ؾ� �Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmoAnsiJoinOrder.h>
#include <qmoCnfMgr.h>
#include <qmoNormalForm.h>
#include <qmgLeftOuter.h>

IDE_RC 
qmoAnsiJoinOrder::traverseFroms( qcStatement * aStatement,
                                 qmsFrom     * aFrom,
                                 qmsFrom    ** aFromTree,
                                 qmsFrom    ** aFromArr,
                                 qcDepInfo   * aFromArrDep,
                                 idBool      * aMakeFail )
{
/***********************************************************************
 *
 * Description : qmsFrom ���� outer join �� driven table
 *               (left outer join �� right) �� aFromTree ��,
 *               �� ���� table �� aFromArr �� �з��ϴ� �Լ�.
 *  
 * ��) select * from t1 left outer join t2 on t1.i1 = t2.i1
 *                      inner join      t3 on t1.i1 = t3.i1
 *                      left outer join t4 on t1.i1 = t4.i1;
 *
 *     Input :        LOJ
 *     (aFrom)       /   \
 *                  IJ    t4
 *                 /  \
 *               LOJ    t3
 *              /   \
 *            t1     t2
 *
 *    Output :        LOJ 
 *    (aFromTree)    /   \
 *                 LOJ    t4
 *                /   \
 *             (t1)    t2
 *
 *    (aFromArr) :   t1 -> t3 
 *
 *    t1 �� left outer join �� left �̹Ƿ� aFromArr �� �з�������,
 *    aFromTree ���� ������ ��� tree ������ �������� �ǹǷ� �����Ѵ�.
 *    �̶����� t1 �� aFromTree �� aFromArr ���ʿ� �����ϰ� �Ǿ� �����
 *    ��������, graph ���� �� optimize �� �Ϸ�� �Ŀ� t1 ��� 
 *    aFromArr �� ������ graph (base graph) �� ġȯ�Ͽ� ������ �ذ��Ѵ�.
 *
 *
 * Implemenation :
 *    qmsFrom �� tree �� �������� ġ��ģ(skewed) ���¶�� �����ϰ� ó���Ѵ�.
 *    Right outer join �� left outer join ���� �����Ǵ� ��� ��������
 *    ġ��ģ ���°� �ƴ� �� ������, �� ��쿡�� �� �Լ��� ȣ��Ǿ�� �ȵȴ�.
 *
 *    ���� qmsFrom �� ��� Ÿ������ ���� ���� qmsFrom �� �з��ϰų�
 *    ��������� ȣ���Ѵ�.
 *
 *      QMS_NO_JOIN (table) :
 *          left outer join �� left �� �̸鼭 �Ϲ� table �� ����̴�.
 *          aFromArr �� aFromTree ���ʿ� �����Ѵ�.
 *      QMS_INNER_JOIN :
 *          right �� �޸� table �� aFromArr �� �����Ѵ�.
 *          left �� ���ȣ���Ͽ� ó���Ѵ�.
 *      QMS_LEFT_OUTER_JOIN :
 *          ���� ���(left outer join)�� right �� �޸� table ��
 *          aFromTree �� �����Ѵ�.
 *          left �� ���ȣ���Ͽ� ó���Ѵ�.
 *
 ***********************************************************************/

    qmsFrom * sFrom;
    qmsFrom * sFromChild;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::traverseFroms::__FT__" );

    if( aFrom->joinType == QMS_NO_JOIN )
    {
        if( *aFromArr != NULL )
        {
            IDE_TEST( cloneFrom( aStatement,
                                 aFrom,
                                 &sFrom )
                      != IDE_SUCCESS );

            IDE_TEST( appendFroms( aStatement,
                                   aFromArr,
                                   sFrom )
                      != IDE_SUCCESS );

            qtc::dependencyOr( aFromArrDep,
                               &sFrom->depInfo,
                               aFromArrDep );
        }

        if( *aFromTree != NULL )
        {
            IDE_TEST( cloneFrom( aStatement,
                                 aFrom,
                                 &sFrom )
                      != IDE_SUCCESS );

            (*aFromTree)->left = sFrom;
        }
    }
    else
    {
        // BUG-40028
        // qmsFrom �� tree �� �������� ġ��ģ(skewed)���°� �ƴ� ����
        // ANSI_JOIN_ORDERING �ؼ��� �ȵȴ�.
        if ( aFrom->right->joinType != QMS_NO_JOIN )
        {
            *aMakeFail = ID_TRUE;
        }
        else
        {
            // Nothing to do.
        }

        if( aFrom->joinType == QMS_INNER_JOIN )
        {
            IDE_TEST( cloneFrom( aStatement,
                                 aFrom->right,
                                 &sFrom )
                      != IDE_SUCCESS );

            IDE_TEST( qmoNormalForm::normalizeCNF( aStatement,
                                                   aFrom->onCondition,
                                                   &sNode )
                      != IDE_SUCCESS );

            sFrom->onCondition = sNode;

            IDE_TEST( appendFroms( aStatement,
                                   aFromArr,
                                   sFrom )
                      != IDE_SUCCESS );

            qtc::dependencyOr( aFromArrDep,
                               &sFrom->depInfo,
                               aFromArrDep );

            IDE_TEST( traverseFroms( aStatement,
                                     aFrom->left,
                                     aFromTree,
                                     aFromArr,
                                     aFromArrDep,
                                     aMakeFail )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( cloneFrom( aStatement,
                                 aFrom,
                                 &sFrom )
                      != IDE_SUCCESS );

            if( *aFromTree == NULL )
            {
                *aFromTree = sFrom;

                IDE_TEST( cloneFrom( aStatement,
                                     aFrom->right,
                                     &sFromChild )
                          != IDE_SUCCESS );

                (*aFromTree)->right = sFromChild;

                IDE_TEST( traverseFroms( aStatement,
                                         aFrom->left,
                                         aFromTree,
                                         aFromArr,
                                         aFromArrDep,
                                         aMakeFail )
                          != IDE_SUCCESS );
            }
            else
            {
                (*aFromTree)->left = sFrom;

                IDE_TEST( cloneFrom( aStatement,
                                     aFrom->right,
                                     & sFromChild )
                          != IDE_SUCCESS );

                (*aFromTree)->left->right = sFromChild;

                IDE_TEST( traverseFroms( aStatement,
                                         aFrom->left,
                                         &((*aFromTree)->left),
                                         aFromArr,
                                         aFromArrDep,
                                         aMakeFail )
                          != IDE_SUCCESS );
            }

            // Reset dependency
            qtc::dependencyClear( &sFrom->depInfo );
            qtc::dependencyOr( &sFrom->left->depInfo,
                               &sFrom->right->depInfo,
                               &sFrom->depInfo );
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoAnsiJoinOrder::appendFroms( qcStatement  * aStatement,
                                      qmsFrom     ** aFromArr,
                                      qmsFrom      * aFrom )
{
    qmsFrom * sFrom;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::appendFroms::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsFrom) ,
                                             (void **)&sFrom )
              != IDE_SUCCESS);

    *sFrom = *aFrom;

    sFrom->next = NULL;

    if( *aFromArr == NULL )
    {
        *aFromArr = sFrom;
    }
    else
    {
        sFrom->next = *aFromArr;

        *aFromArr = sFrom;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoAnsiJoinOrder::cloneFrom( qcStatement * aStatement,
                             qmsFrom     * aFrom1,
                             qmsFrom    ** aFrom2 )
{
    qmsFrom * sFrom;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::cloneFrom::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmsFrom) ,
                                             (void **)&sFrom )
              != IDE_SUCCESS);

    *sFrom = *aFrom1;

    sFrom->next = NULL;

    sFrom->left = NULL;

    sFrom->right = NULL;

    *aFrom2 = sFrom;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoAnsiJoinOrder::clonePredicate2List( qcStatement   * aStatement,
                                              qmoPredicate  * aPred1,
                                              qmoPredicate ** aPredList )
{
    qmoPredicate * sPred;
    qmoPredicate * sCursor;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::clonePredicate2List::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredicate) ,
                                             (void **)&sPred )
              != IDE_SUCCESS);

    *sPred = *aPred1;

    sPred->next = NULL;

    if( *aPredList == NULL )
    {
        *aPredList = sPred;
    }
    else
    {
        sCursor = *aPredList;

        while( 1 )
        {
            if( sCursor->next == NULL )
            {
                break;
            }
            else
            {
                sCursor = sCursor->next;
            }
        }

        sCursor->next = sPred;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoAnsiJoinOrder::mergeOuterJoinGraph2myGraph( qmoCNF * aCNF )
{
/***********************************************************************
 *
 * Description : outerJoinGraph �� myGraph �� ����
 *
 * Implemenation :
 *    BUG-34295 Join ordering ANSI style query
 *    outerJoinGraph �� myGraph �ʹ� ������ �����Ǵ� outer join �� ����
 *    graph �̴�.
 *    �� �Լ��� outerJoinGraph �� ���� ����
 *    (left outer join �� ������ �Ǵ� ��ġ)�� myGraph �� �����Ѵ�.
 *
 ***********************************************************************/

    qmgGraph     * sIter = NULL;
    qmgGraph     * sPrev = NULL;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::mergeOuterJoinGraph2myGraph::__FT__" );

    IDE_DASSERT( aCNF != NULL );
    IDE_DASSERT( aCNF->outerJoinGraph != NULL );

    sIter = aCNF->outerJoinGraph;

    // BUG-39877 OPTIMIZER_ANSI_JOIN_ORDERING ���� left, right �� �ٲ�� ����
    // ���� �˰����� ���� ���ʿ� �ִ� �׷����� ã�Ƽ� �����ϴ� ���̹Ƿ�
    // selectedJoinMethod �̿��Ͽ� left�� �Ǵ��ؾ� �Ѵ�.
    while( sIter->left != NULL )
    {
        IDE_FT_ERROR_MSG( sIter->type == QMG_LEFT_OUTER_JOIN,
                          "Graph type : %u\n", sIter->type );

        sPrev = sIter;

        if ( (((qmgLOJN*)sIter)->selectedJoinMethod->flag & QMO_JOIN_METHOD_DIRECTION_MASK)
                == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
        {
            sIter = sIter->left;
        }
        else
        {
            sIter= sIter->right;
        }
    }

    IDE_FT_ERROR( sPrev != NULL );

    if ( (((qmgLOJN*)sPrev)->selectedJoinMethod->flag & QMO_JOIN_METHOD_DIRECTION_MASK)
            == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
    {
        sPrev->left = aCNF->myGraph;
    }
    else
    {
        sPrev->right = aCNF->myGraph;
    }

    aCNF->myGraph = aCNF->outerJoinGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoAnsiJoinOrder::fixOuterJoinGraphPredicate( qcStatement * aStatement,
                                              qmoCNF      * aCNF )
{
/***********************************************************************
 *
 * Description : BUG-34295 Join ordering ANSI style query
 *    Where ���� predicate �� outerJoinGraph �� ������
 *    one table predicate �� ã�� �̵���Ų��.
 *    outerJoinGraph �� one table predicate �� baseGraph ��
 *    dependency �� ��ġ�� �ʾƼ� predicate �з� ��������
 *    constant predicate ���� �߸� �з��ȴ�.
 *    �̸� �ٷ���� ���� sCNF->constantPredicate �� predicate �鿡��
 *    outerJoinGraph �� ���õ� one table predicate ���� ã�Ƴ���
 *    outerJoinGraph �� �̵���Ų��.
 *
 * Implemenation :
 *
 *
 ***********************************************************************/

    qmoCNF         * sCNF;
    qcDepInfo      * sFromDependencies;
    qcDepInfo      * sGraphDependencies;
    idBool           sIsOneTable = ID_FALSE;

    qmoPredicate   * sPred;
    qmoPredicate   * sNewConstantPred = NULL;

    IDU_FIT_POINT_FATAL( "qmoAnsiJoinOrder::fixOuterJoinGraphPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aCNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sCNF               = aCNF;
    sFromDependencies  = &(sCNF->outerJoinGraph->depInfo);
    sGraphDependencies = &(sCNF->outerJoinGraph->myFrom->depInfo);

    // Extract one table predicate from CNF->constantPredicate (mis-placed pred)
    for( sPred = sCNF->constantPredicate;
         sPred != NULL;
         sPred = sPred->next )
    {
        IDE_TEST( qmoPred::isOneTablePredicate( sPred,
                                                sFromDependencies,
                                                sGraphDependencies,
                                                & sIsOneTable )
                  != IDE_SUCCESS );

        // Dependency �� �ϳ��� ��� sIsOneTable �� ID_TRUE �� �����Ƿ�
        // dependency count �� 1 �̻��̿��� ��¥ one table predicate �̴�.
        if( ( sIsOneTable == ID_TRUE ) &&
            ( sPred->node->depInfo.depCount > 0 ) )
        {
            // flag setting
            sPred->flag &= ~QMO_PRED_CONSTANT_FILTER_MASK;
            sPred->flag |= QMO_PRED_CONSTANT_FILTER_FALSE;

            // add to outerJoinGraph
            IDE_TEST( clonePredicate2List( aStatement,
                                           sPred,
                                           &sCNF->outerJoinGraph->myPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // add to new constant predicate list
            IDE_TEST( clonePredicate2List( aStatement,
                                           sPred,
                                           &sNewConstantPred )
                      != IDE_SUCCESS );
        }
    }

    // constantPredicate ���� ��¥ constant predicate �� ���´�.
    sCNF->constantPredicate = sNewConstantPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

