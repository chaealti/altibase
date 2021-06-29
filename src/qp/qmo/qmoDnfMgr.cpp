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
 * $Id: qmoDnfMgr.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * Description :
 *     DNF Critical Path Manager
 *
 *     DNF Normalized Form�� ���� ����ȭ�� �����ϰ�
 *     �ش� Graph�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qmoDnfMgr.h>
#include <qmgSelection.h>
#include <qmoCrtPathMgr.h>
#include <qmoCostDef.h>
#include <qmoNormalForm.h>

IDE_RC
qmoDnfMgr::init( qcStatement * aStatement,
                 qmoDNF      * aDNF,
                 qmsQuerySet * aQuerySet,
                 qtcNode     * aNormalDNF )
{
/***********************************************************************
 *
 * Description : qmoDnf�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) aNormalDNF�� qmoDNF::normalDNF�� �����Ѵ�.
 *    (2) AND ������ŭ cnfCnt�� �����Ѵ�.
 *    (3) cnfCnt ������ŭ qmoCnf�� ���� �迭 ������ �Ҵ�޴´�.
 *
 ***********************************************************************/

    qmsQuerySet * sQuerySet;
    qmoDNF      * sDNF;
    qtcNode     * sAndNode;
    UInt          sCnfCnt;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDNF != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aNormalDNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sQuerySet = aQuerySet;
    sCnfCnt   = 0;

    //------------------------------------------
    // DNF �ʱ�ȭ
    //------------------------------------------

    sDNF = aDNF;

    sDNF->normalDNF = aNormalDNF;
    sDNF->myGraph = NULL;
    sDNF->myQuerySet = sQuerySet;
    sDNF->cost = 0;

    //------------------------------------------
    // PROJ-1405
    // Rownum Predicate ó��
    //------------------------------------------

    if ( ( aNormalDNF->lflag & QTC_NODE_ROWNUM_MASK )
         == QTC_NODE_ROWNUM_EXIST )
    {
        // rownum predicate�� ��� critical path�� �����ؼ� �ø���.
        IDE_TEST( qmoCrtPathMgr::addRownumPredicateForNode( aStatement,
                                                            sQuerySet,
                                                            aNormalDNF,
                                                            ID_TRUE )
                  != IDE_SUCCESS );

        // rownum predicate�� �����Ѵ�.
        IDE_TEST( removeRownumPredicate( aStatement, sDNF )
                  != IDE_SUCCESS );
    }
    else
    {
        // Noting to do.
    }

    //------------------------------------------
    // CNF ���� ���
    //------------------------------------------

    if ( aNormalDNF->node.arguments != NULL )
    {
        for ( sAndNode = (qtcNode *) aNormalDNF->node.arguments, sCnfCnt = 0;
              sAndNode != NULL;
              sAndNode = (qtcNode *)sAndNode->node.next, sCnfCnt++ ) ;
    }
    else
    {
        // BUG-17950
        // �� AND�׷��� �߻��Ͽ� full(?) selection�� �����ϴ� AND �׷���
        // �Ѱ� �����Ѵ�.
        sCnfCnt = 1;
    }

    sDNF->cnfCnt = sCnfCnt;
    sDNF->dnfGraphCnt = sCnfCnt - 1;

    //------------------------------------------
    // CNF ������ŭ qmoCNF�� ���� �迭 ���� �Ҵ� ����
    //------------------------------------------

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoCNF ) * sCnfCnt,
                                             (void **)& sDNF->myCNF )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDnfMgr::optimize( qcStatement * aStatement,
                     qmoDNF      * aDNF,
                     SDouble       aCnfCost )
{
/***********************************************************************
 *
 * Description : DNF Processor�� ����ȭ( ��, qmoDNF�� ����ȭ)
 *
 * Implementation :
 *    (1) �� qmoCNF(��)�� �ʱ�ȭ, ����ȭ �Լ� ȣ��
 *        - aCnfCost != 0 : DNF Prunning ����
 *        - aCnfCost == 0 : DNF Prunning �������� ����
 *    (2)(cnfCnt - 1)������ŭ dnfGraph�� ���� �迭 ������ �Ҵ����
 *    (3) dnfNotNormalForm�� ����
 *    (4) �� dnf�� �ʱ�ȭ
 *    (5) �� dnf�� ����ȭ
 *
 ***********************************************************************/

    qmoCNF   * sCNF;
    qmoDNF   * sDNF;
    qtcNode  * sAndNode;
    SDouble    sDnfCost;
    idBool     sIsPrunning;
    qmgGraph * sResultGraph = NULL;
    UInt       i;
    UInt       sCnfCnt;
    UInt       sDnfGraphCnt;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::optimize::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sDNF         = aDNF;
    sDnfCost     = 0;
    sIsPrunning  = ID_FALSE;
    sCnfCnt      = sDNF->cnfCnt;
    sDnfGraphCnt = sDNF->dnfGraphCnt;

    // PROJ-1446 Host variable�� ������ ���� ����ȭ
    // dnf optimization�� ������ cnf�� ������ �ʱ�ȭ�Ѵ�.
    sDNF->madeCnfCnt = 0;

    //------------------------------------------
    // �� CNF���� �ʱ�ȭ, ����ȭ
    //------------------------------------------

    sAndNode = (qtcNode *)sDNF->normalDNF->node.arguments;

    for ( i = 0; i < sCnfCnt; i++ )
    {
        sCNF = & sDNF->myCNF[i];

        IDE_TEST( qmoCnfMgr::init( aStatement,
                                   sCNF,
                                   sDNF->myQuerySet,
                                   sAndNode,
                                   NULL )
                  != IDE_SUCCESS );

        IDE_TEST( qmoCnfMgr::optimize( aStatement, sCNF ) != IDE_SUCCESS );

        // PROJ-1446 Host variable�� ������ ���� ����ȭ
        // dnf optimization�� ������ cnf�� ������ ������Ų��.
        sDNF->madeCnfCnt++;

        sDnfCost += sCNF->cost;

        // Join�� DNF�� ó���� ��쿡 ���� Penalty �ο�
        switch ( sCNF->myGraph->type )
        {
            case QMG_INNER_JOIN:
            case QMG_LEFT_OUTER_JOIN:
            case QMG_FULL_OUTER_JOIN:
                sDnfCost += ( sCNF->cost * QMO_COST_DNF_JOIN_PENALTY );
                break;
            default:
                // Nothing To Do
                break;
        }

        if (QMO_COST_IS_EQUAL(aCnfCost, 0.0) == ID_FALSE)
        {
            //------------------------------------------
            // DNF Prunning ����
            //------------------------------------------

            // BUG-42400 cost �񱳽� ��ũ�θ� ����ؾ� �մϴ�.
            if ( QMO_COST_IS_GREATER(aCnfCost, sDnfCost) == ID_TRUE )
            {
                // nothing to do
            }
            else
            {
                // DNF Prunning
                sIsPrunning = ID_TRUE;
                break;
            }
        }
        else
        {
            //------------------------------------------
            // DNF Prunning �������� ����
            //------------------------------------------

            // nothing to do
        }

        if ( sAndNode != NULL )
        {
            sAndNode = (qtcNode *)sAndNode->node.next;
        }
        else
        {
            // Nothing to do.
        }
    }

    // To Fix PR-8237
    sDNF->cost = sDnfCost;

    if ( sIsPrunning == ID_FALSE )
    {
        // To Fix PR-8727
        // ����� Hint�� CNF Only Predicate���� �Ǵܵ��� �ʴ�
        // CNF Only Predicate�� ��� DNF Graph�� ������ 0�� �� ����.
        // ����, 0���� Ŭ ��쿡�� DNF Graph�� �����ؾ� ��.

        if ( sDnfGraphCnt > 0 )
        {
            //------------------------------------------
            // dnf Graph�� ���� �迭 ������ �Ҵ� ����
            //------------------------------------------

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmgDNF) * sDnfGraphCnt,
                                                     (void**) & sDNF->dnfGraph )
                != IDE_SUCCESS );

            //------------------------------------------
            // dnfNotNormalForm �迭 ����
            //------------------------------------------

            IDE_TEST( makeNotNormal( aStatement, sDNF ) != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
            // ����� Hint�� CNF Only Predicate���� �Ǵܵ��� �ʴ�
            // CNF Only Predicate�� ��� DNF Graph�� �ʿ����� �ʴ�.
        }

        for ( i = 0; i < sCnfCnt ; i++ )
        {
            if ( i == 0 )
            {
                sResultGraph = sDNF->myCNF[i].myGraph;
            }
            else
            {
                //------------------------------------------
                // �� Dnf Graph�� �ʱ�ȭ
                //------------------------------------------

                IDE_TEST( qmgDnf::init( aStatement,
                                        sDNF->notNormal[i-1],
                                        sResultGraph,
                                        sDNF->myCNF[i].myGraph,
                                        & sDNF->dnfGraph[i-1].graph )
                          != IDE_SUCCESS );

                sResultGraph = & sDNF->dnfGraph[i-1].graph;

                //------------------------------------------
                // �� Dnf Graph�� ����ȭ
                //------------------------------------------

                IDE_TEST( qmgDnf::optimize( aStatement,
                                            sResultGraph )
                          != IDE_SUCCESS );
            }
        }
    }
    else
    {
        // Prunning �Ǿ����Ƿ� Dnf Graph ������ �ʿ� ����
        // Nothing to do...
    }

    // To Fix BUG-8247
    sDNF->myGraph = sResultGraph;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDnfMgr::removeOptimizationInfo( qcStatement * aStatement,
                                   qmoDNF      * aDNF )
{
    UInt i;
    UInt j;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::removeOptimizationInfo::__FT__" );

    for( i = 0; i < aDNF->madeCnfCnt; i++ )
    {
        for( j = 0; j < aDNF->myCNF[i].graphCnt4BaseTable; j++ )
        {
            IDE_TEST( qmg::removeSDF( aStatement,
                                      aDNF->myCNF[i].baseGraph[j] )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDnfMgr::makeNotNormal( qcStatement * aStatement,
                          qmoDNF      * aDNF )
{
/***********************************************************************
 *
 * Description : Dnf Not Normal Form���� �����.
 *
 * Implementation :
 *
 *    ������ ���� DNF Normal Form�� �ִٰ� ��������
 *
 *           [OR]
 *            |
 *           [AND1]-->[AND2]-->[AND3]
 *
 *   ù��° DNF Not Normal Form�� �ۼ�
 *
 *       1.  makeDnfNotNormal()              2.  AND ���� ����
 *
 *                                              [AND]
 *                                                 |
 *           [LNNVL]                            [LNNVL]
 *              |                   ==>            |
 *           [AND1]'                            [AND1]'
 *
 *    �ι�° DNF Not Normal Form�� �ۼ�
 *
 *       3. qtc::copyAndForDnfFilter() �� 2�� ����
 *
 *           [AND]'
 *              |
 *           [LNNVL]' <-- sLastNode
 *              |
 *           [AND1]''
 *
 *       4.  [AND2]�� ���� DNF Not Normal Form�� �ۼ�
 *
 *           [LNNVL]
 *              |
 *           [AND2]'
 *
 *       5.  3�� 4�� ����
 *
 *           [AND]'
 *              |
 *           [LNNVL]' --->   [LNNVL]
 *              |               |
 *           [AND1]''        [AND2]'
 *
 *
 *
 ***********************************************************************/

    qtcNode      * sNormalForm;
    qtcNode     ** sNotNormalForm;
    qtcNode      * sDnfNotNode;
    qtcNode      * sAndNode[2];
    qtcNode      * sLastNode;
    qcNamePosition sNullPosition;
    UInt           sNotNormalFormCnt;
    UInt           i;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::makeNotNormal::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNotNormalFormCnt = aDNF->dnfGraphCnt;
    SET_EMPTY_POSITION(sNullPosition);

    // ���� �Ҵ�
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode *) * sNotNormalFormCnt,
                                             (void **) & sNotNormalForm )
              != IDE_SUCCESS );

    // �� not normal form ����
    for ( i = 0, sNormalForm = (qtcNode *)aDNF->normalDNF->node.arguments;
          i < sNotNormalFormCnt;
          i++, sNormalForm = (qtcNode *)sNormalForm->node.next )
    {
        if ( i == 0 )
        {
            IDE_TEST( makeDnfNotNormal( aStatement,
                                        sNormalForm,
                                        & sDnfNotNode )
                      != IDE_SUCCESS );

            // And Node ����
            IDE_TEST( qtc::makeNode( aStatement,
                                     sAndNode,
                                     & sNullPosition,
                                     (const UChar*)"AND",
                                     3 )
                      != IDE_SUCCESS );

            sAndNode[0]->node.arguments = (mtcNode *)sDnfNotNode;

            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        sAndNode[0] )
                      != IDE_SUCCESS );

            sNotNormalForm[i] = sAndNode[0];
        }
        else
        {
            IDE_TEST( qtc::copyAndForDnfFilter( aStatement,
                                                sNotNormalForm[i-1],
                                                & sNotNormalForm[i],
                                                & sLastNode )
                      != IDE_SUCCESS );

            // TASK-3876 codesonar
            IDE_TEST_RAISE( sLastNode == NULL,
                            ERR_DNF_FILETER );

            IDE_TEST( makeDnfNotNormal( aStatement,
                                        sNormalForm,
                                        & sDnfNotNode )
                      != IDE_SUCCESS );

            sLastNode->node.next = (mtcNode *)sDnfNotNode;
        }
    }

    aDNF->notNormal = sNotNormalForm;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DNF_FILETER )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoDnfMgr::makeNotNormal",
                                  "fail to copy dnf filter" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDnfMgr::makeDnfNotNormal( qcStatement * aStatement,
                             qtcNode     * aNormalForm,
                             qtcNode    ** aDnfNotNormal )
{
/***********************************************************************
 *
 * Description : Dnf Not Normal Form�� �����.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode      * sNode;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::makeDnfNotNormal::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNormalForm != NULL );

    //------------------------------------------
    // Normal Form ����
    //------------------------------------------
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                     aNormalForm,
                                     & sNode,
                                     ID_FALSE,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                           &sNode )
              != IDE_SUCCESS );

    IDE_TEST( qtc::lnnvlNode( aStatement,
                              sNode )
              != IDE_SUCCESS );

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sNode )
              != IDE_SUCCESS );

    *aDnfNotNormal = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoDnfMgr::removeRownumPredicate( qcStatement * aStatement,
                                  qmoDNF      * aDNF )
{
/***********************************************************************
 *
 * Description :
 *     �� AND �׷쿡�� rownum predicate�� �����Ѵ�.
 *
 *     rownum predicate�� �����ϰ� �Ǹ� predicate�� ���� AND �׷���
 *     ������� �� �ִµ�, �� AND �׷��� ��� ���ڵ尡 �ö�;�
 *     �ϴ� selection�� �����Ѵ�.
 *
 *     �׷���, �̷� �׷��� �ϳ� �̻��� �� �־� predicate�� ����
 *     AND �׷��� ��� �����ϰ�, �Ŀ� �� AND �׷��� �ϳ� �߰��� ��
 *     �ֵ��� flag�� ��ȯ�Ѵ�.
 *
 *
 *     ��1) where ROWNUM < 1 and ( i2 = 2 or ROWNUM < 3 );
 *
 *     [where]
 *
 *       OR
 *       |
 *      AND ----------------- AND
 *       |                     |
 *       = ------ =            = -------- <
 *       |        |            |          |
 *    ROWNUM - 1  i2 - 2    ROWNUM - 1  ROWNUM - 3
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]            [AND�׷� 2]
 *
 *      AND                   AND
 *       |                     |
 *       = ------ =            = -------- <
 *       |        |            |          |
 *    ROWNUM - 1  i2 - 2    ROWNUM - 1  ROWNUM - 3
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]            [AND�׷� 2]
 *
 *      AND                   AND
 *       |
 *       =
 *       |
 *      i2 - 2
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]            [AND�׷� 2]
 *
 *      AND                   �� AND �׷�
 *       |
 *       =
 *       |
 *      i2 - 2
 *
 *
 *    ��2) where ROWNUM < 1 or ROWNUM < 3;
 *
 *     [where]
 *
 *       OR
 *       |
 *      AND ----------------- AND
 *       |                     |
 *       =                     =
 *       |                     |
 *    ROWNUM - 1            ROWNUM - 3
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]            [AND�׷� 2]
 *
 *      AND                   AND
 *       |                     |
 *       =                     =
 *       |                     |
 *    ROWNUM - 1            ROWNUM - 3
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]            [AND�׷� 2]
 *
 *      AND                   AND
 *
 *                      |
 *                      V
 *
 *     [where]
 *
 *     [AND�׷� 1]
 *
 *     �� AND �׷�
 *
 *
 * Implementation :
 *     1. AND �׷쳻�� rownum predicate�� �����Ѵ�.
 *     2. predicate�� ���� AND �׷��� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode  * sNormalDNF;
    qtcNode  * sAndNode;
    qtcNode  * sCompareNode;
    qtcNode  * sFirstNode;
    qtcNode  * sPrevNode;
    qtcNode  * sNode;
    idBool     sEmptyAndGroup;

    IDU_FIT_POINT_FATAL( "qmoDnfMgr::removeRownumPredicate::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aDNF != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sNormalDNF = aDNF->normalDNF;
    sEmptyAndGroup = ID_FALSE;

    //------------------------------------------
    // AND �׷� ���� rownum predicate�� �����Ѵ�.
    //------------------------------------------

    for ( sAndNode = (qtcNode *)sNormalDNF->node.arguments;
          sAndNode != NULL;
          sAndNode = (qtcNode *)sAndNode->node.next )
    {
        sFirstNode = NULL;
        sPrevNode = NULL;

        for ( sCompareNode = (qtcNode *) sAndNode->node.arguments;
              sCompareNode != NULL;
              sCompareNode = (qtcNode *) sCompareNode->node.next )
        {
            if ( ( sCompareNode->lflag & QTC_NODE_ROWNUM_MASK )
                 == QTC_NODE_ROWNUM_EXIST )
            {
                // rownum predicate�� ���ŵȴ�.

                // Nothing to do.
            }
            else
            {
                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qtcNode),
                                                         (void **)&sNode )
                             != IDE_SUCCESS );

                idlOS::memcpy( sNode, sCompareNode, ID_SIZEOF(qtcNode) );

                if ( sPrevNode == NULL )
                {
                    sFirstNode = sNode;
                    sPrevNode = sNode;
                    sNode->node.next = NULL;
                }
                else
                {
                    sPrevNode->node.next = (mtcNode*) sNode;
                    sPrevNode = sNode;
                    sNode->node.next = NULL;
                }
            }
        }

        sAndNode->node.arguments = (mtcNode*) sFirstNode;
    }

    //------------------------------------------
    // predicate�� ���� AND �׷��� �����Ѵ�.
    //------------------------------------------

    for ( sAndNode = (qtcNode *)sNormalDNF->node.arguments;
          sAndNode != NULL;
          sAndNode = (qtcNode *)sAndNode->node.next )
    {
        if ( sAndNode->node.arguments == NULL )
        {
            // BUG-17950
            // predicate�� ���� �� AND �׷��� �ִ� ���
            // ������ AND �׷��� ������ �� �ִ�.
            sEmptyAndGroup = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    if ( sEmptyAndGroup == ID_TRUE )
    {
        // �� AND�׷��� �����Ǿ�� ���� normalDNF�� arguments��
        // NULL�ΰ����� �˸���.
        aDNF->normalDNF->node.arguments = NULL;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
