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
 * $Id: qmoDependency.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     Plan Dependency Manager
 *
 *     �� Plan Node ���� �� Plan ���� ���踦 ����Ͽ�,
 *     �ش� Plan�� Dependency�� �����ϴ� ������ �����Ѵ�.
 *
 *     �Ʒ��� ���� �� 6�ܰ��� ������ ���� Plan�� Depenendency�� �����ȴ�.
 *
 *     - 1�ܰ� : Set Tuple ID
 *     - 2�ܰ� : Dependencies ����
 *     - 3�ܰ� : Table Map ����
 *     - 4�ܰ� : Dependency ����
 *     - 5�ܰ� : Dependency ����
 *     - 6�ܰ� : Dependencies ����
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmoSubquery.h>
#include <qmoDependency.h>

IDE_RC
qmoDependency::setDependency( qcStatement * aStatement,
                              qmsQuerySet * aQuerySet,
                              qmnPlan     * aPlan,
                              UInt          aDependencyFlag,
                              UShort        aTupleID,
                              qmsTarget   * aTarget,
                              UInt          aPredCount,
                              qtcNode    ** aPredExpr,
                              UInt          aMtrCount,
                              qmcMtrNode ** aMtrNode )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Plan Node�� Dependency �� Depedencies�� �����Ѵ�.
 *    �� �ܿ��� Predicate�̳� Expression�� �����ϴ�
 *    Subquery�� Plan Tree�� �����Ѵ�.
 *
 *        - aDependencyFlag : �÷��� Dependency���õ� Flag �� ����
 *        - aTupleID : Tuple�� ������ �ִ� ��� �� ���� ����
 *        - aTarget  : Target�� �ִ� ��� ����
 *        - aPredCount : Predicate �Ǵ� Expression�� �����ϴ� ������ ����
 *        - aPredExpr  : ��� Predicate�̳� Expression�� ���� ��ġ
 *            Ex) SCAN ����� ���, ������ ���� Predicate�� ������.
 *                - constantFilter, filter, subqueryFilter
 *                - fixKeyRangeOrg, varKeyRange,
 *                - fixKeyFilterOrg, varKeyRange,
 *                - ���� ��� ������ ���� ���� ������
 *                    - aPredCount : 7
 *                    - aPredExpr  : �ش� predicate�� ���� ��ġ�� ����Ű�� �迭
 *            Cf) �������� �ʴ� ��쿡�� ������.
 *
 *        - aMtrCount : ���� ����� ������ ����
 *        - aMtrNode  : ��� ���� ����� ���� ��ġ
 *            Ex) AGGR ����� ���, ������ ���� �� ������ ���� ��尡 ������.
 *                - myNode, distNode
 *
 * Implementation :
 *     - Tuple ID�� �ڱ�� NODE���� Table Map�� ����Ѵ�.
 *     - Dependency ������ �� 6 �ܰ踦 �����Ѵ�.
 *          - 1 �ܰ� : Tuple ID ���� �ܰ�
 *          - 2 �ܰ� : Dependencies ���� �ܰ�
 *          - 3 �ܰ� : Table Map ���� �ܰ�
 *          - 4 �ܰ� : Dependency ���� �ܰ�
 *          - 5 �ܰ� : Dependecny ���� �ܰ� : 4�ܰ迡 ���Ե�
 *          - 6 �ܰ� : Dependencies ���� �ܰ�
 *
 ***********************************************************************/

    qmsTarget  * sTarget;
    qmcMtrNode * sMtrNode;
    qtcNode    * sTargetNode;
    UInt         i;

    qcDepInfo sDependencies;
    qcDepInfo sTotalDependencies;
    qcDepInfo sMtrDependencies;

    IDU_FIT_POINT_FATAL( "qmoDependency::setDependency::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aPlan != NULL );

    //---------------------------------------------------------
    // 1 �ܰ� : Tuple ID ���� �ܰ�
    //---------------------------------------------------------

    qtc::dependencyClear( & aPlan->depInfo );

    if( ( aDependencyFlag & QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_MASK ) ==
        QMO_DEPENDENCY_STEP1_SET_TABLE_MAP_TRUE )
    {
        IDE_TEST( qmoDependency::step1setTupleID( aStatement,
                                                  aTupleID )
                  != IDE_SUCCESS );
    }
    else
    {
        //nothing to do
    }

    //---------------------------------------------------------
    // 2 �ܰ� : Dependencies ���� �ܰ�
    //---------------------------------------------------------

    qtc::dependencyClear( & sTotalDependencies );
    qtc::dependencyClear( & sMtrDependencies );

    //-------------------------------------------
    // Predicate/Expression�� ��� Dependencies����
    //     - 2�ܰ� �Ǵ� 6�ܰ迡�� ����
    //-------------------------------------------

    // Target���κ��� Dependencies����
    for ( sTarget = aTarget; sTarget != NULL; sTarget = sTarget->next )
    {
        qtc::dependencySetWithDep( & sDependencies ,
                                   & sTotalDependencies );

        // BUG-38228 group by �� �������� target �� pass node�� ������ �ִ�.
        if ( sTarget->targetColumn->node.module == &qtc::passModule )
        {
            sTargetNode = (qtcNode*)(sTarget->targetColumn->node.arguments);
        }
        else
        {
            sTargetNode = (qtcNode*)(sTarget->targetColumn);
        }

        IDE_TEST( qtc::dependencyOr( & sTargetNode->depInfo,
                                     & sDependencies,
                                     & sTotalDependencies )
                  != IDE_SUCCESS );
    }

    // Predicate�̳� Expression���κ��� ����
    for ( i = 0; i < aPredCount; i++ )
    {
        if ( aPredExpr[i] != NULL )
        {
            qtc::dependencySetWithDep( & sDependencies ,
                                       & sTotalDependencies );
            IDE_TEST( qtc::dependencyOr( & aPredExpr[i]->depInfo,
                                         & sDependencies,
                                         & sTotalDependencies )
                      != IDE_SUCCESS );

        }
        else
        {
            // Nothing To Do
        }
    }

    //-------------------------------------------
    // ������ ���� 2�ܰ� ����
    //-------------------------------------------

    if ( ( aDependencyFlag & QMO_DEPENDENCY_STEP2_DEP_MASK ) ==
         QMO_DEPENDENCY_STEP2_DEP_WITH_PREDICATE )
    {
        //------------------------------------
        // Predicate�̳� Expresssion���κ���
        // dependencies�� �����ϴ� ���
        //------------------------------------

        IDE_TEST( qmoDependency::step2makeDependencies( aPlan,
                                                        aDependencyFlag,
                                                        aTupleID,
                                                        & sTotalDependencies )
                  != IDE_SUCCESS );
    }
    else
    {
        //------------------------------------
        // Materialization Node��κ���
        // dependencies�� �����ϴ� ���
        // Join�� �����ϴ� HASH, SORT ��尡 �̿� �ش���.
        //------------------------------------

        IDE_DASSERT( aMtrNode != NULL && aMtrCount > 0 );

        for ( i = 0; i < aMtrCount; i++ )
        {
            for ( sMtrNode = aMtrNode[i];
                  sMtrNode != NULL;
                  sMtrNode = sMtrNode->next )
            {
                qtc::dependencySetWithDep( & sDependencies ,
                                           & sMtrDependencies );

                IDE_TEST( qtc::dependencyOr( & sMtrNode->srcNode->depInfo,
                                             & sDependencies,
                                             & sMtrDependencies )
                          != IDE_SUCCESS );
            }
        }

        IDE_TEST( qmoDependency::step2makeDependencies( aPlan,
                                                        aDependencyFlag,
                                                        aTupleID,
                                                        & sMtrDependencies )
                  != IDE_SUCCESS );

        // Materialized Column�� ���� Plan Tree�� ������.
        for ( i = 0; i < aMtrCount; i++ )
        {
            for ( sMtrNode = aMtrNode[i];
                  sMtrNode != NULL;
                  sMtrNode = sMtrNode->next )
            {
                IDE_TEST( qmoSubquery::makePlan( aStatement,
                                                 aTupleID,
                                                 sMtrNode->srcNode )
                          != IDE_SUCCESS );
            }
        }
    }

    //---------------------------------------------------------
    // 3 �ܰ� : Table Map ���� �ܰ�
    //---------------------------------------------------------

    if ( ( aDependencyFlag & QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_MASK )
         == QMO_DEPENDENCY_STEP3_TABLEMAP_REFINE_TRUE )
    {
        IDE_TEST( qmoDependency::step3refineTableMap( aStatement,
                                                      aPlan,
                                                      aQuerySet,
                                                      aTupleID )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing To Do
    }

    // Target Column�� ���� plan tree�� �����Ѵ�.
    // �̴� 3�ܰ谡 ���� �Ŀ� ó���� ���ֵ��� �Ѵ�.
    for ( sTarget = aTarget; sTarget != NULL; sTarget = sTarget->next )
    {
        // BUG-38228 group by �� �������� target �� pass node�� ������ �ִ�.
        if ( sTarget->targetColumn->node.module == &qtc::passModule )
        {
            sTargetNode = (qtcNode*)(sTarget->targetColumn->node.arguments);
        }
        else
        {
            sTargetNode = (qtcNode*)(sTarget->targetColumn);
        }

        IDE_TEST( qmoSubquery::makePlan( aStatement,
                                         aTupleID,
                                         sTargetNode )
                  != IDE_SUCCESS );
    }

    //--------------------------------------
    // Plan Tree ���� �� Host ���� ���
    //--------------------------------------

    for ( i = 0; i < aPredCount; i++ )
    {
        if ( aPredExpr[i] != NULL )
        {
            // Plan Tree ����
            IDE_TEST( qmoSubquery::makePlan( aStatement,
                                             aTupleID,
                                             aPredExpr[i] )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }

    //---------------------------------------------------------
    // 4 �ܰ� : Dependency ���� �ܰ�
    // 5 �ܰ� : Dependecny ���� �ܰ� : 4�ܰ迡 ���Ե�
    //---------------------------------------------------------

    IDE_TEST( qmoDependency::step4decideDependency( aStatement ,
                                                    aPlan,
                                                    aQuerySet)
              != IDE_SUCCESS );

    //---------------------------------------------------------
    // 6 �ܰ� : Dependencies ���� �ܰ�
    //---------------------------------------------------------

    if( ( aDependencyFlag & QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_MASK ) ==
        QMO_DEPENDENCY_STEP6_DEPENDENCIES_REFINE_TRUE )
    {
        IDE_TEST( qmoDependency::step6refineDependencies( aPlan,
                                                          & sTotalDependencies)
                  != IDE_SUCCESS );
    }
    else
    {
        //nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDependency::step1setTupleID( qcStatement * aStatement ,
                                UShort        aTupleID )
{
/***********************************************************************
 *
 * Description : Dependency������ 1�ܰ��� �ڽ��� Tuple ID�� Table Map��
 *               ����Ѵ�.
 *
 * Implementation :
 *     - Tuple ID�� �ڱ�� NODE���� Table Map�� ����Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoDependency::step1setTupleID::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //----------------------------------
    //Table Map�� ���
    //----------------------------------

    QC_SHARED_TMPLATE(aStatement)->tableMap[aTupleID].dependency = (ULong)aTupleID;

    return IDE_SUCCESS;
}

IDE_RC
qmoDependency::step2makeDependencies( qmnPlan     * aPlan ,
                                      UInt          aDependencyFlag,
                                      UShort        aTupleID ,
                                      qcDepInfo   * aDependencies )
{
/***********************************************************************
 *
 * Description : Dependency������ 2�ܰ��� ��ǥ Dependency�� ����
 *               dependencies�� �����Ѵ�.
 *
 * Implementation :
 *     - 4������ ������ �Ѵ�.
 *         - ����� ���� : ����� Base Table�� �ǹ� �ϴ� NODE
 *         - ����� ���� : ���� child�� dependencies�� �����ϴ� NODE
 *         - ������ ���� : ��� Predicate �� Expression�� dependencies
 *                         ����
 *         - ������ ���� : Materialization�� Predicate�� �ٸ�
 *                         dependencies�� ������ ���
 *
 ***********************************************************************/

    qcDepInfo     sChildDependencies;
    qmnChildren * sChildren;

    IDU_FIT_POINT_FATAL( "qmoDependency::step2makeDependencies::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aPlan != NULL );
    IDE_DASSERT( aDependencies != NULL );

    //----------------------------------
    //��忡 ���� ����
    //----------------------------------

    if( ( aDependencyFlag & QMO_DEPENDENCY_STEP2_BASE_TABLE_MASK )
        == QMO_DEPENDENCY_STEP2_BASE_TABLE_TRUE )
    {
        //����� Base Table�� �ǹ��ϴ� NODE
        //Tuple ID�� ���� dependencies ǥ��
        qtc::dependencySet( aTupleID , & aPlan->depInfo );
    }
    else
    {
        if ( ( aDependencyFlag & QMO_DEPENDENCY_STEP2_SETNODE_MASK )
             == QMO_DEPENDENCY_STEP2_SETNODE_TRUE )
        {
            // PROJ-1358
            // SET �� ��� dependency�� �������� �ʰ�,
            // Child�� ��ǥ outer dependency���� �����Ѵ�.
            qtc::dependencySet( aTupleID , & aPlan->depInfo );

            if ( aPlan->children == NULL )
            {
                // Left Child�� ��ǥ dependency ����
                if( aPlan->left != NULL )
                {
                    if ( (aPlan->left->flag & QMN_PLAN_OUTER_REF_MASK)
                         == QMN_PLAN_OUTER_REF_TRUE )
                    {
                        qtc::dependencySet( (UShort)aPlan->left->outerDependency,
                                            & sChildDependencies );
                        IDE_TEST( qtc::dependencyOr( & aPlan->depInfo,
                                                     & sChildDependencies,
                                                     & aPlan->depInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // dependency�� �������� �ʴ´�.
                    }
                }
                else
                {
                    // Nothing to do.
                }

                // Right Child�� ��ǥ dependency ����
                if( aPlan->right != NULL )
                {
                    if ( (aPlan->right->flag & QMN_PLAN_OUTER_REF_MASK)
                         == QMN_PLAN_OUTER_REF_TRUE )
                    {
                        qtc::dependencySet( (UShort)aPlan->right->outerDependency,
                                            & sChildDependencies );
                        IDE_TEST( qtc::dependencyOr( & aPlan->depInfo,
                                                     & sChildDependencies,
                                                     & aPlan->depInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // dependency�� �������� �ʴ´�.
                    }
                }
                else
                {
                    // Nothing to do.
                }
            }
            else
            {
                // PROJ-1486
                // Multiple Children Set �� ���
                for ( sChildren = aPlan->children;
                      sChildren != NULL;
                      sChildren = sChildren->next )
                {
                    if ( (sChildren->childPlan->flag & QMN_PLAN_OUTER_REF_MASK)
                         == QMN_PLAN_OUTER_REF_TRUE )
                    {
                        qtc::dependencySet( (UShort)sChildren->childPlan->outerDependency,
                                            & sChildDependencies );
                        IDE_TEST( qtc::dependencyOr( & aPlan->depInfo,
                                                     & sChildDependencies,
                                                     & aPlan->depInfo )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // dependency�� �������� �ʴ´�.
                    }
                }
            }
        }
        else
        {
            //Base Table�� �ƴ϶�, ������ Dependencies�� �����ϴ� NODE
            if( aPlan->left != NULL )
            {
                if( aPlan->right == NULL )
                {
                    qtc::dependencySetWithDep( & aPlan->depInfo,
                                               & aPlan->left->depInfo );
                }
                else
                {
                    IDE_TEST( qtc::dependencyOr( & aPlan->left->depInfo,
                                                 & aPlan->right->depInfo,
                                                 & aPlan->depInfo )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                // Nothing to do.
            }
        }
    }

    //----------------------------------
    //���꿡 ���� ����
    //----------------------------------

    //����� materialize column���� dependencies�� �߰��Ѵ�.
    //NODE�� ���� dependencies�� �̸� ���Ǿ �Էµȴ�.
    IDE_TEST( qtc::dependencyOr( & aPlan->depInfo,
                                 aDependencies,
                                 & aPlan->depInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDependency::step3refineTableMap( qcStatement * aStatement ,
                                    qmnPlan     * aPlan ,
                                    qmsQuerySet * aQuerySet ,
                                    UShort        aTupleID )
{
/***********************************************************************
 *
 * Description : Dependency������ 3�ܰ��� Table Map�� �����Ѵ�.
 *
 * Implementation :
 *     - Materialization NODE���� Table Map�� dependency�� �����Ѵ�.
 *     - �ش� �����
 ***********************************************************************/

    qcDepInfo sDependencies;
    SInt      sTableMapIndex;

    IDU_FIT_POINT_FATAL( "qmoDependency::step3refineTableMap::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPlan != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    qtc::dependencyClear( & sDependencies );

    //----------------------------------
    //dependencies�� �ش��ϴ� ���̺� ����
    //----------------------------------

    //�ش� ����� dependencies�� ���� �ش� querySet�� �ش��ϴ�
    //dependencies����
    qtc::dependencyAnd( & aPlan->depInfo,
                        & aQuerySet->depInfo,
                        & sDependencies );

    //����� dependencies�� �ش��ϴ� ���̺� ����
    sTableMapIndex = qtc::getPosFirstBitSet( & sDependencies );

    //----------------------------------
    //Table Map�� ����
    //----------------------------------
    while( sTableMapIndex != QTC_DEPENDENCIES_END )
    {
        //����� Table�� dependency�� TupleID ����
        QC_SHARED_TMPLATE(aStatement)->tableMap[sTableMapIndex].dependency = (ULong)aTupleID;
        sTableMapIndex = qtc::getPosNextBitSet( & sDependencies ,
                                                sTableMapIndex );
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoDependency::step4decideDependency( qcStatement * aStatement ,
                                      qmnPlan     * aPlan ,
                                      qmsQuerySet * aQuerySet )
{
/***********************************************************************
 *
 * Description : Dependency������ 4�ܰ��� ��ǥ Dependency�� �����Ѵ�.
 *
 * Implementation :
 *     - Outer Column Reference�� �����ϴ��� �˻��Ѵ�.
 *         - plan�� dependencies & NOT(��query�� dependencies)
 *             - Reference�� ���� ��� :
 *                      �ش� querySet�� join Order�� ����
 *             - Reference�� �ִ� ��� :
 *                      ���� querySet�� join Order�� ����
 *     - Dependencies�� Join Order�� ���� ���� ������ Table�� ã�´�.
 *     - �Ʒ��� 5�ܰ������ Outer Column Reference�� ������ �˾ƾ�
 *       �ϹǷ� 4�ܰ迡�� ���� ó�� �Ѵ�.
 *
 ***********************************************************************/

    qcDepInfo      sDependencies;
    qcDepInfo      sBaseDepInfo;
    idBool         sHaveDependencies;
    qmsQuerySet  * sQuerySet;
    qmsSFWGH     * sSFWGH;

    IDU_FIT_POINT_FATAL( "qmoDependency::step4decideDependency::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPlan != NULL );
    IDE_DASSERT( aQuerySet != NULL );

    //----------------------------------
    //��ǥ Dependency�� ����
    //----------------------------------

    //----------------------------------
    //outer column reference ã��
    //----------------------------------

    // PROJ-2418
    // Lateral Dependency�� �����ϴ� ��쿣, ���� dependency�� ��ü�Ѵ�
    qtc::dependencyClear( & sBaseDepInfo );

    if ( qtc::haveDependencies ( & aQuerySet->lateralDepInfo ) == ID_TRUE )
    {
        // QuerySet���� �ܺ� ������ �ؾ� �ϴ� ��Ȳ�̶��
        // QuerySet�� Lateral Dependencies�� ����
        qtc::dependencySetWithDep( & sBaseDepInfo, & aQuerySet->lateralDepInfo );

        // �� ���, ù ��° ��� (dependencies�� ���� ���) ��
        // ������ ����ϰ� �ȴ�. �� ��° ���� �̵��� ��ǥ dependency�� ã�´�.
    }
    else
    {
        // Lateral View�� �������� �ʴ� QuerySet�� �����
        // ������ ���� Plan�� Dependencies�� ����
        qtc::dependencySetWithDep( & sBaseDepInfo, & aPlan->depInfo );
    }

    if( qtc::dependencyContains( & aQuerySet->depInfo,
                                 & sBaseDepInfo ) == ID_TRUE )
    {
        //---------------------------------
        // dependencies�� ���� ���.
        // ��, outer column reference�� ���� ���
        //---------------------------------

        if( aQuerySet->SFWGH != NULL )
        {
            //���� querySet�� join order�� aPlan�� dependencies�� ����
            //���� ������ order�� ã�´�.
            sSFWGH = aQuerySet->SFWGH;
            IDE_TEST( findRightJoinOrder( sSFWGH,
                                          &sBaseDepInfo,
                                          &sDependencies )
                      != IDE_SUCCESS );

            sHaveDependencies = qtc::haveDependencies( & sDependencies );
            if( sHaveDependencies == ID_TRUE )
            {
                //ã�� dependencies�� ��ǥ dependencies�� ��´�
                aPlan->dependency = qtc::getPosFirstBitSet( & sDependencies );
            }
            else
            {
                //ã�� ���� ���
                IDE_DASSERT( 0 );
            }
        }
        else
        {
            //��, SET�� ��쿡�� SFWGH�� NULL�̰� right, left�� �޷�
            //�ִµ� �� ��� VIEW�� �����Ǵµ� �̴� ���ο� Relation��
            //�����̹Ƿ�, leaf��� ó�� ó���� �Ǿ�� ���̴�.
            //(��, �ڽ��� ��ǥdependency�� ������ �ִ´�.)
            aPlan->dependency = qtc::getPosFirstBitSet( & sBaseDepInfo );
        }

        // PROJ-1358
        // Outer Reference ���翩�θ� ����
        aPlan->flag &= ~QMN_PLAN_OUTER_REF_MASK;
        aPlan->flag |= QMN_PLAN_OUTER_REF_FALSE;
    }
    else
    {
        //---------------------------------
        // dependencies�� �ִ� ���.
        // ��, outer column reference�� �ִ� ���
        //---------------------------------

        // SET���� ǥ���ϴ� Query Set�� ���,
        // ���� Query�� ã�� ���� Right-most Query Set���� �̵��Ѵ�.
        sQuerySet = aQuerySet;
        while ( sQuerySet->right != NULL )
        {
            sQuerySet = sQuerySet->right;
        }
        sSFWGH = sQuerySet->SFWGH;

        IDE_TEST_RAISE( sSFWGH == NULL, ERR_INVALID_SFWGH );

        while( sSFWGH->outerQuery != NULL )
        {
            //���� querySet�� join order���� ã�´�.
            //����� ������ �ٽ� ������ �ö󰣴�.
            sSFWGH = sSFWGH->outerQuery;

            // PROJ-1413
            // outerQuery�� view merging�� ���� ���̻� ��ȿ���� ���� �� �ִ�.
            // merge�� SFWGH�� ã�ư��� �Ѵ�.
            while ( sSFWGH->mergedSFWGH != NULL )
            {
                sSFWGH = sSFWGH->mergedSFWGH;
            }

            if ( sSFWGH->crtPath != NULL )
            {
                IDE_TEST( findRightJoinOrder( sSFWGH,
                                              &sBaseDepInfo,
                                              &sDependencies )
                          != IDE_SUCCESS );

                sHaveDependencies = qtc::haveDependencies( & sDependencies );

                if( sHaveDependencies == ID_TRUE )
                {
                    //ã�� dependencies�� ��ǥ dependencies�� ��´�
                    aPlan->dependency = qtc::getPosFirstBitSet( & sDependencies );

                    // PROJ-1358
                    // ���� ���� ��ǥ outer dependency�� ����Ѵ�.
                    aPlan->outerDependency = aPlan->dependency;

                    // 5�ܰ� ó��
                    // Table Map���κ��� ������ dependency���� �����Ѵ�.
                    aPlan->dependency =
                        QC_SHARED_TMPLATE(aStatement)->tableMap[aPlan->
                                                      dependency].dependency;
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
        }

        // PROJ-1358
        // Outer Reference ���翩�θ� ����
        aPlan->flag &= ~QMN_PLAN_OUTER_REF_MASK;
        aPlan->flag |= QMN_PLAN_OUTER_REF_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_SFWGH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoDependency::step4decideDependency",
                                  "SFWGH is null" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDependency::step6refineDependencies( qmnPlan     * aPlan ,
                                        qcDepInfo   * aDependencies )
{
/***********************************************************************
 *
 * Description : Dependency ������ 6�ܰ��� ���� NODE�� ���� Dependencies
 *               �� �����Ѵ�.
 *
 * Implementation :
 *     - 2�ܰ迡�� HASH , SORT�� Materialize column�� ���ؼ���
 *       dependencies�� ���������Ƿ� Predicate �� Expression�� ���ؼ���
 *       dependencies�� �������ش�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoDependency::step6refineDependencies::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aPlan != NULL );
    IDE_DASSERT( aDependencies != NULL );

    //----------------------------------
    //dependencies�� ����
    //----------------------------------

    //Predicate �� Expression���� dependencies �߰�
    //depdencies�� �̸� ���Ǿ �Էµȴ�.
    IDE_TEST( qtc::dependencyOr( & aPlan->depInfo,
                                 aDependencies ,
                                 & aPlan->depInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoDependency::findRightJoinOrder( qmsSFWGH   * aSFWGH ,
                                   qcDepInfo  * aInputDependencies ,
                                   qcDepInfo  * aOutputDependencies )
{
/***********************************************************************
 *
 * Description : ���� Dependencies�� Join Order�� ���� ���� ������
 *               order�� Table�� ã�´�.
 *
 * Implementation :
 *     - qmoTableOrder�� ��ȸ �ϸ鼭 input dependencies�� AND�����Ͽ�
 *       dependencies�� ������ ��� ��� �صд�.
 *
 *     - dependencies�� ���� ���
 *          -
 ***********************************************************************/

    qmoTableOrder * sTableOrder;
    qcDepInfo       sDependencies;
    qcDepInfo       sRecursiveViewDep;
    idBool          sHaveDependencies;

    IDU_FIT_POINT_FATAL( "qmoDependency::findRightJoinOrder::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aSFWGH != NULL );
    IDE_DASSERT( aInputDependencies != NULL );
    IDE_DASSERT( aOutputDependencies != NULL );

    //----------------------------------
    // �ʱ�ȭ
    //----------------------------------

    sTableOrder = aSFWGH->crtPath->currentCNF->tableOrder;

    //��� dependencies�� ���� ��� clear�Ǿ� ���ϵȴ�.
    qtc::dependencyClear( aOutputDependencies );

    // PROJ-2582 recursive with
    // recursive view�� dependency ������ �����Ѵ�.
    if ( aSFWGH->recursiveViewID != ID_USHORT_MAX )
    {
        qtc::dependencySet( aSFWGH->recursiveViewID,
                            & sRecursiveViewDep );
    }
    else
    {
        qtc::dependencyClear( & sRecursiveViewDep );
    }

    //----------------------------------
    // Join Order���� ������ Table �˻�
    //----------------------------------

    //Join Order�� ��ȸ �Ѵ�.
    while( sTableOrder != NULL )
    {
        //join order�� �Էµ� dependencies�� AND�����Ͽ�
        //�ش� join order�� �ִ��� �˻��Ѵ�.
        qtc::dependencyAnd( & sTableOrder->depInfo,
                            aInputDependencies ,
                            & sDependencies );

        sHaveDependencies = qtc::haveDependencies( & sDependencies );

        //join order�� ���� �������� �ִ� ���� dependencies�� ����
        //�������� �ȴ�.
        if ( sHaveDependencies == ID_TRUE )
        {
            qtc::dependencySetWithDep( aOutputDependencies ,
                                       & sDependencies );

            // PROJ-2582 recursive with
            // recursive view�� �ִ� ��� �����ϰ�, recursive view��
            // dependency�� �����ϰ� �Ѵ�.
            if ( ( qtc::haveDependencies( & sRecursiveViewDep )
                   == ID_TRUE ) &&
                 ( qtc::dependencyContains( & sDependencies,
                                            & sRecursiveViewDep )
                   == ID_TRUE ) )
            {
                qtc::dependencySetWithDep( aOutputDependencies ,
                                           & sRecursiveViewDep );
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

        sTableOrder = sTableOrder->next;
    }

    return IDE_SUCCESS;
}

