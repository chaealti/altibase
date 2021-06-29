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
 * $Id: qmoTwoNonPlan.cpp 86088 2019-08-30 00:33:15Z ahra.cho $
 *
 * Description :
 *     Plan Generator
 *
 *     Two-child Non-Materialized Plan�� �����ϱ� ���� �������̴�.
 *
 *     ������ ���� Plan Node�� ������ �����Ѵ�.
 *         - JOIN ���
 *         - MGJN ���
 *         - LOJN ���
 *         - FOJN ���
 *         - AOJN ���
 *         - CONC ���
 *         - BUNI ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmsParseTree.h>
#include <qmo.h>
#include <qmoTwoNonPlan.h>
#include <qci.h>

extern mtfModule mtfEqual;

IDE_RC
qmoTwoNonPlan::initJOIN( qcStatement   * aStatement,
                         qmsQuerySet   * aQuerySet,
                         qmsQuerySet   * aViewQuerySet,
                         qtcNode       * aJoinPredicate,
                         qtcNode       * aFilterPredicate,
                         qmoPredicate  * aPredicate,
                         qmnPlan       * aParent,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : JOIN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncJOIN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncJOIN          * sJOIN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initJOIN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aParent != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncJOIN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncJOIN) , (void **)&sJOIN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sJOIN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_JOIN ,
                        qmnJOIN ,
                        qmndJOIN,
                        sDataNodeOffset );

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sJOIN->plan.resultDesc )
              != IDE_SUCCESS );

    if( aJoinPredicate != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sJOIN->plan.resultDesc,
                                        aJoinPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if( aFilterPredicate != NULL )
    {
        sJOIN->filter = aFilterPredicate;

        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sJOIN->plan.resultDesc,
                                        aFilterPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        sJOIN->filter = NULL;
    }

    // BUG-43077
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if( aViewQuerySet != NULL )
    {
        IDE_TEST( qmc::appendViewPredicate( aStatement,
                                            aViewQuerySet,
                                            &sJOIN->plan.resultDesc,
                                            0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }


    // TODO: aFilterPredicate���� ��ü�ؾ� �Ѵ�.
    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sJOIN->plan.resultDesc,
                                    aPredicate )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sJOIN;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::makeJOIN( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aGraphType,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
    qmncJOIN          * sJOIN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeJOIN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    sJOIN = (qmncJOIN *)aPlan;

    sJOIN->plan.left    = aLeftChild;
    sJOIN->plan.right   = aRightChild;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sJOIN->flag = QMN_PLAN_FLAG_CLEAR;
    sJOIN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sJOIN->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sJOIN->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndJOIN));

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    switch( aGraphType )
    {
        case QMG_INNER_JOIN:
            sJOIN->flag &= ~QMNC_JOIN_TYPE_MASK;
            sJOIN->flag |= QMNC_JOIN_TYPE_INNER;
            break;
        case QMG_SEMI_JOIN:
            sJOIN->flag &= ~QMNC_JOIN_TYPE_MASK;
            sJOIN->flag |= QMNC_JOIN_TYPE_SEMI;
            break;
        case QMG_ANTI_JOIN:
            sJOIN->flag &= ~QMNC_JOIN_TYPE_MASK;
            sJOIN->flag |= QMNC_JOIN_TYPE_ANTI;
            break;
        default:
            IDE_DASSERT( 0 );
    }

    if( sJOIN->filter != NULL )
    {
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sJOIN->filter )
                  != IDE_SUCCESS );

        IDE_TEST( qmoNormalForm::optimizeForm( sJOIN->filter,
                                               &sJOIN->filter )
                  != IDE_SUCCESS );

        IDE_TEST( qmg::changeColumnLocate( aStatement,
                                           aQuerySet,
                                           sJOIN->filter,
                                           ID_USHORT_MAX,
                                           ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sJOIN->plan ,
                                            QMO_JOIN_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            &sJOIN->filter ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sJOIN->plan.resultDesc,
                                     &sJOIN->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sJOIN->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sJOIN->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sJOIN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sJOIN->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);
    
    // PROJ-2551 simple query ����ȭ
    // index nl join�� ��� fast execute�� �����Ѵ�.
    IDE_TEST( checkSimpleJOIN( aStatement, sJOIN ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::initMGJN( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmoPredicate  * aJoinPredicate ,
                         qmoPredicate  * aFilterPredicate ,
                         qmnPlan       * aParent,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : MGJN��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncMGJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - ���� ��忡 �´� flag����
 *         - MGJN ����� ���� ( �Է� Predicate�� ����)
 *         - compareLetfRight ���� ( ��� �񱳸� ���� predicate )
 *         - storedJoinCondition ���� ( ����� ����� �� )
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncMGJN          * sMGJN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initMGJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------
    // qmncMGJN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncMGJN) , (void **)&sMGJN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sMGJN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_MGJN ,
                        qmnMGJN ,
                        qmndMGJN,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sMGJN;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sMGJN->plan.resultDesc )
              != IDE_SUCCESS );

    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sMGJN->plan.resultDesc,
                                    aJoinPredicate )
              != IDE_SUCCESS );

    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sMGJN->plan.resultDesc,
                                    aFilterPredicate )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoTwoNonPlan::makeMGJN( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         UInt           aGraphType,
                         qmoPredicate * aJoinPredicate ,
                         qmoPredicate * aFilterPredicate ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
    qmncMGJN          * sMGJN = (qmncMGJN *)aPlan;
    UInt                sDataNodeOffset;

    UShort              sTupleID;
    UShort              sColumnCount = 0;
    qmcMtrNode        * sMtrNode;

    const mtfModule   * sModule;

    qtcNode           * sCompareNode;
    qtcNode           * sColumnNode;

    qtcNode           * sPredicate; // fix BUG-12282
    qtcNode           * sNode;
    qtcNode           * sOptimizedNode = NULL;

    qtcNode           * sNewNode[2];
    qcNamePosition      sPosition;

    mtcTemplate       * sMtcTemplate;
    qtcNode             sCopiedNode;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeMGJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------
    sMtcTemplate = & QC_SHARED_TMPLATE(aStatement)->tmplate;

    sMGJN->plan.left    = aLeftChild;
    sMGJN->plan.right   = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndMGJN));

    sMGJN->mtrNodeOffset = sDataNodeOffset;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sMGJN->flag = QMN_PLAN_FLAG_CLEAR;
    sMGJN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sMGJN->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sMGJN->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    switch( aGraphType )
    {
        case QMG_INNER_JOIN:
            sMGJN->flag &= ~QMNC_MGJN_TYPE_MASK;
            sMGJN->flag |= QMNC_MGJN_TYPE_INNER;
            break;
        case QMG_SEMI_JOIN:
            sMGJN->flag &= ~QMNC_MGJN_TYPE_MASK;
            sMGJN->flag |= QMNC_MGJN_TYPE_SEMI;
            break;
        case QMG_ANTI_JOIN:
            sMGJN->flag &= ~QMNC_MGJN_TYPE_MASK;
            sMGJN->flag |= QMNC_MGJN_TYPE_ANTI;
            break;
        default:
            IDE_DASSERT( 0 );
    }

    // To Fix PR-8062
    // Child ������ ���� Flag ����
    switch ( aLeftChild->type )
    {
        case QMN_SCAN :
            sMGJN->flag &= ~QMNC_MGJN_LEFT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_LEFT_CHILD_SCAN;
            break;
        case QMN_PCRD :
            sMGJN->flag &= ~QMNC_MGJN_LEFT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_LEFT_CHILD_PCRD;
            break;
        case QMN_SORT :
            sMGJN->flag &= ~QMNC_MGJN_LEFT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_LEFT_CHILD_SORT;
            break;
        case QMN_MGJN :
            sMGJN->flag &= ~QMNC_MGJN_LEFT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_LEFT_CHILD_MGJN;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    switch ( aRightChild->type )
    {
        case QMN_SCAN :
            sMGJN->flag &= ~QMNC_MGJN_RIGHT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_RIGHT_CHILD_SCAN;
            break;
        case QMN_PCRD :
            sMGJN->flag &= ~QMNC_MGJN_RIGHT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_RIGHT_CHILD_PCRD;
            break;
        case QMN_SORT :
            sMGJN->flag &= ~QMNC_MGJN_RIGHT_CHILD_MASK;
            sMGJN->flag |= QMNC_MGJN_RIGHT_CHILD_SORT;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    //------------------------------------------------------------
    // ���� �۾�
    //------------------------------------------------------------

    //----------------------------------
    // Flag ����
    //----------------------------------

    IDE_TEST( qtc::nextTable( &sTupleID , aStatement, NULL, ID_TRUE, MTC_COLUMN_NOTNULL_TRUE ) // PR-13597
              != IDE_SUCCESS );

    sMtcTemplate->rows[sTupleID].lflag      &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag      |= MTC_TUPLE_STORAGE_MEMORY;

    //----------------------------------
    // Join Predicate�� ����
    //----------------------------------

    IDE_TEST( qmoPred::linkPredicate( aStatement ,
                                      aJoinPredicate ,
                                      & sNode )
              != IDE_SUCCESS );
    IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                       aQuerySet ,
                                       sNode ) != IDE_SUCCESS );

    // BUG-17921
    IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                           & sOptimizedNode )
              != IDE_SUCCESS );

    sMGJN->mergeJoinPred = sOptimizedNode;

    // To Fix PR-8062
    if ( aFilterPredicate != NULL )
    {
        IDE_TEST( qmoPred::linkFilterPredicate( aStatement ,
                                                aFilterPredicate ,
                                                & sNode )
                  != IDE_SUCCESS );
        IDE_TEST( qmoPred::setPriorNodeID( aStatement ,
                                           aQuerySet ,
                                           sNode )
                  != IDE_SUCCESS );

        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( sNode,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sMGJN->joinFilter = sOptimizedNode;
    }
    else
    {
        sMGJN->joinFilter = NULL;
    }

    // To Fix PR-8062
    // Store Join Predicate, Compare Join Predicate ����
    // ������ ����ȭ��.

    //----------------------------------
    // Compare Node �� Column Node ����
    //----------------------------------

    sCompareNode = sMGJN->mergeJoinPred;
    while ( 1 )
    {
        if( ( sCompareNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sCompareNode->node.arguments);
        }
        else
        {
            break;
        }
    }

    if ( sCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode*) sCompareNode->node.arguments;
    }
    else
    {
        sColumnNode = (qtcNode*) sCompareNode->node.arguments->next;
    }

    sCopiedNode = *sColumnNode;

    //----------------------------------
    // ���� Column ����
    //----------------------------------
    IDE_TEST( qmg::makeColumnMtrNode( aStatement,
                                      aQuerySet,
                                      &sCopiedNode,
                                      ID_TRUE,
                                      sTupleID,
                                      0,
                                      & sColumnCount,
                                      & sMGJN->myNode )
              != IDE_SUCCESS );

    // To Fix PR-8062
    // ���� Column�� ������ ���� ���� calculate �� �����ϵ��� �����Ѵ�.
    for ( sMtrNode = sMGJN->myNode;
          sMtrNode != NULL;
          sMtrNode = sMtrNode->next )
    {
        sMtrNode->flag &= ~QMC_MTR_TYPE_MASK;
        sMtrNode->flag |= QMC_MTR_TYPE_CALCULATE_AND_COPY_VALUE;
    }

    //----------------------------------
    // Stored Join Condition ����
    //----------------------------------

    // fix BUG-23279
    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sMGJN->mergeJoinPred,
                                       sTupleID,
                                       ID_TRUE )
              != IDE_SUCCESS );

    // Join Predicate�� ����
    IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                     sMGJN->mergeJoinPred,
                                     &sMGJN->storedMergeJoinPred,
                                     ID_TRUE,
                                     ID_FALSE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    // CompareNode ����
    sCompareNode = sMGJN->storedMergeJoinPred;
    while ( 1 )
    {
        if( ( sCompareNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sCompareNode->node.arguments);
        }
        else
        {
            break;
        }
    }

    // Column ��带 ���� Column���� ��ü
    if ( sCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode*) sCompareNode->node.arguments;
        sCompareNode->node.arguments = (mtcNode*) sMGJN->myNode->dstNode;
        sCompareNode->node.arguments->next = sColumnNode->node.next;
    }
    else
    {
        sCompareNode->node.arguments->next =
            (mtcNode*) sMGJN->myNode->dstNode;
    }

    //----------------------------------
    // ��ȣ �������� ����� ��� �� �Լ� ����
    //----------------------------------

    if ( sCompareNode->node.module == & mtfEqual )
    {
        // Join Predicate�� ����
        IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                         sMGJN->mergeJoinPred,
                                         &sMGJN->compareLeftRight,
                                         ID_TRUE,
                                         ID_FALSE,
                                         ID_FALSE,
                                         ID_FALSE )
                  != IDE_SUCCESS );

        // CompareNode ����
        sCompareNode = sMGJN->compareLeftRight;
        while ( 1 )
        {
            if( ( sCompareNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                sCompareNode = (qtcNode *)(sCompareNode->node.arguments);
            }
            else
            {
                break;
            }
        }

        SET_EMPTY_POSITION( sPosition );

        if ( sCompareNode->indexArgument == 0 )
        {
            // T1.i1 = T2.i1
            //      MGJN
            //     |    |
            //    T2    T1 �� �����
            // T1.i1 <= T2.i1 �� ������.

            // To Fix PR-8062
            // ���� ��带 �����Ͽ� ���� ������ �����Ŵ.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sNewNode,
                                     & sPosition,
                                     (const UChar*) "<=",
                                     2 )
                      != IDE_SUCCESS );

        }
        else
        {
            // T1.i1 = T2.i1
            //      MGJN
            //     |    |
            //    T1    T2 �� �����
            // T1.i1 >= T2.i1 �� ������.

            // ���� ��带 �����Ͽ� ���� ������ �����Ŵ.
            IDE_TEST( qtc::makeNode( aStatement,
                                     sNewNode,
                                     & sPosition,
                                     (const UChar*) ">=",
                                     2 )
                      != IDE_SUCCESS );

        }

        sNewNode[0]->node.arguments = sCompareNode->node.arguments;

        IDE_TEST( qtc::estimateNodeWithoutArgument ( aStatement,
                                                     sNewNode[0] )
                  != IDE_SUCCESS );

        idlOS::memcpy( sCompareNode, sNewNode[0], ID_SIZEOF(qtcNode) );
    }
    else
    {
        // ��� �� �Լ��� ���� �ʿ䰡 ����.
        sMGJN->compareLeftRight = NULL;
    }

    //----------------------------------
    // Tuple column�� �Ҵ�
    //----------------------------------
    sColumnCount = 0;
    for (sMtrNode = sMGJN->myNode ;
         sMtrNode != NULL ;
         sMtrNode = sMtrNode->next)
    {
        sModule      = sMtrNode->srcNode->node.module;
        sColumnCount += sModule->lflag & MTC_NODE_COLUMN_COUNT_MASK;
    }

    IDE_TEST( qtc::allocIntermediateTuple( aStatement,
                                           & QC_SHARED_TMPLATE(aStatement)->tmplate,
                                           sTupleID ,
                                           sColumnCount ) != IDE_SUCCESS);

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_TRUE;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_STORAGE_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_STORAGE_MEMORY;

    sMtcTemplate->rows[sTupleID].lflag &= ~MTC_TUPLE_PLAN_MTR_MASK;
    sMtcTemplate->rows[sTupleID].lflag |= MTC_TUPLE_PLAN_MTR_FALSE;

    //----------------------------------
    // mtcColumn , mtcExecute ������ ����
    //----------------------------------
    IDE_TEST( qmg::copyMtcColumnExecute( aStatement ,
                                         sMGJN->myNode )
              != IDE_SUCCESS);

    //----------------------------------
    // PROJ-2179 calculate�� �ʿ��� node���� ��� ��ġ�� ����
    //----------------------------------

    IDE_TEST( qmg::setCalcLocate( aStatement,
                                  sMGJN->myNode )
              != IDE_SUCCESS );

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------
    //data ������ ũ�� ���
    for (sMtrNode = sMGJN->myNode , sColumnCount = 0 ;
         sMtrNode != NULL;
         sMtrNode = sMtrNode->next , sColumnCount++ ) ;

    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset +
        sColumnCount * idlOS::align8( ID_SIZEOF(qmdMtrNode) );

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    // fix BUG-12282
    sPredicate = sMGJN->joinFilter;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����. 
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate,
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sMGJN->plan ,
                                            QMO_MGJN_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            1 , // fix BUG-12282
                                            &sPredicate ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sMGJN->plan.resultDesc,
                                     &sMGJN->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sMGJN->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sMGJN->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sMGJN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sMGJN->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::initLOJN( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qmsQuerySet   * aViewQuerySet,
                         qtcNode       * aJoinPredicate,
                         qtcNode       * aFilter,
                         qmoPredicate  * aPredicate,
                         qmnPlan       * aParent ,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : LOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncLOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncLOJN          * sLOJN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initLOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncLOJN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncLOJN) , (void **)&sLOJN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sLOJN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_LOJN ,
                        qmnLOJN ,
                        qmndLOJN,
                        sDataNodeOffset );

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sLOJN->plan.resultDesc )
              != IDE_SUCCESS );

    // BUG-38513 join �������� result desc�� �߰����־����
    if( aJoinPredicate != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sLOJN->plan.resultDesc,
                                        aJoinPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if( aFilter != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sLOJN->plan.resultDesc,
                                        aFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    // BUG-47414
    // view�ȿ��� �����ϴ� �ܺ� ���� �÷����� Result descriptor�� �߰��ؾ� �Ѵ�.
    if( aViewQuerySet != NULL )
    {
        IDE_TEST( qmc::appendViewPredicate( aStatement,
                                            aViewQuerySet,
                                            &sLOJN->plan.resultDesc,
                                            0 )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sLOJN->plan.resultDesc,
                                    aPredicate )
              != IDE_SUCCESS );

    *aPlan = (qmnPlan *)sLOJN;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoTwoNonPlan::makeLOJN( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aFilter ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : LOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncLOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncLOJN          * sLOJN = (qmncLOJN *)aPlan;
    UInt                sDataNodeOffset;
    qtcNode           * sPredicate[2];
    qtcNode           * sOptimizedNode;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeLOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    sLOJN->plan.left    = aLeftChild;
    sLOJN->plan.right   = aRightChild;

    //----------------------------------
    // Flag ����
    //----------------------------------

    sLOJN->flag      = QMN_PLAN_FLAG_CLEAR;
    sLOJN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sLOJN->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sLOJN->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    //------------------------------------------------------------
    // ���� �۾�
    //------------------------------------------------------------

    if ( aFilter != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( aFilter,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sLOJN->filter = sOptimizedNode;
    }
    else
    {
        sLOJN->filter = NULL;
    }

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndLOJN));

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] =  sLOJN->filter;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����. 
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );    

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sLOJN->plan ,
                                            QMO_LOJN_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sPredicate,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sLOJN->plan.resultDesc,
                                     &sLOJN->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sLOJN->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sLOJN->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sLOJN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sLOJN->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::initFOJN( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qtcNode       * aJoinPredicate,
                         qtcNode       * aFilter ,
                         qmoPredicate  * aPredicate,
                         qmnPlan       * aParent,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : FOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncFOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncFOJN          * sFOJN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initFOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncFOJN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncFOJN) , (void **)&sFOJN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sFOJN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_FOJN ,
                        qmnFOJN ,
                        qmndFOJN,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sFOJN;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sFOJN->plan.resultDesc )
              != IDE_SUCCESS );

    // BUG-38513 join �������� result desc�� �߰����־���� 
    if( aJoinPredicate != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sFOJN->plan.resultDesc,
                                        aJoinPredicate )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    if( aFilter != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sFOJN->plan.resultDesc,
                                        aFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sFOJN->plan.resultDesc,
                                    aPredicate )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::makeFOJN( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qtcNode      * aFilter ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : FOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncFOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncFOJN          * sFOJN = (qmncFOJN *)aPlan;
    UInt                sDataNodeOffset;
    qtcNode           * sPredicate[2];
    qtcNode           * sOptimizedNode;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeFOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aLeftChild != NULL );
    IDE_FT_ASSERT( aRightChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    sFOJN->plan.left    = aLeftChild;
    sFOJN->plan.right   = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndFOJN));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sFOJN->flag      = QMN_PLAN_FLAG_CLEAR;
    sFOJN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sFOJN->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sFOJN->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    //------------------------------------------------------------
    // ���� �۾�
    //------------------------------------------------------------

    //flag�� ����
    if (aRightChild->type == QMN_HASH)
    {
        sFOJN->flag &= ~QMNC_FOJN_RIGHT_CHILD_MASK;
        sFOJN->flag |= QMNC_FOJN_RIGHT_CHILD_HASH;
    }
    else if (aRightChild->type == QMN_SORT)
    {
        sFOJN->flag &= ~QMNC_FOJN_RIGHT_CHILD_MASK;
        sFOJN->flag |= QMNC_FOJN_RIGHT_CHILD_SORT;
    }
    else
    {
        IDE_DASSERT(0);
    }

    if ( aFilter != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( aFilter,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sFOJN->filter = sOptimizedNode;
    }
    else
    {
        sFOJN->filter = NULL;
    }

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] =  sFOJN->filter;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����. 
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );    

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sFOJN->plan ,
                                            QMO_FOJN_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sPredicate,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sFOJN->plan.resultDesc,
                                     &sFOJN->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sFOJN->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sFOJN->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sFOJN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sFOJN->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::initAOJN( qcStatement   * aStatement ,
                         qmsQuerySet   * aQuerySet ,
                         qtcNode       * aFilter ,
                         qmoPredicate  * aPredicate,
                         qmnPlan       * aParent,
                         qmnPlan      ** aPlan )
{
/***********************************************************************
 *
 * Description : AOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncAOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncAOJN          * sAOJN;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initAOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncAOJN�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncAOJN) , (void **)&sAOJN )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sAOJN ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_AOJN ,
                        qmnAOJN ,
                        qmndAOJN,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sAOJN;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sAOJN->plan.resultDesc )
              != IDE_SUCCESS );

    if( aFilter != NULL )
    {
        IDE_TEST( qmc::appendPredicate( aStatement,
                                        aQuerySet,
                                        &sAOJN->plan.resultDesc,
                                        aFilter )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    IDE_TEST( qmc::appendPredicate( aStatement,
                                    aQuerySet,
                                    &sAOJN->plan.resultDesc,
                                    aPredicate )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::makeAOJN( qcStatement * aStatement ,
                         qmsQuerySet * aQuerySet ,
                         qtcNode     * aFilter ,
                         qmnPlan     * aLeftChild ,
                         qmnPlan     * aRightChild ,
                         qmnPlan     * aPlan )
{
/***********************************************************************
 *
 * Description : AOJN ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncAOJN�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *         - aFilterPredicate�� Filter�� �����Ѵ�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncAOJN          * sAOJN = (qmncAOJN *)aPlan;
    UInt                sDataNodeOffset;
    qtcNode           * sPredicate[2];
    qtcNode           * sOptimizedNode;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeAOJN::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    sAOJN->plan.left    = aLeftChild;
    sAOJN->plan.right   = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndAOJN));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sAOJN->flag      = QMN_PLAN_FLAG_CLEAR;
    sAOJN->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sAOJN->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sAOJN->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    //------------------------------------------------------------
    // ���� �۾�
    //------------------------------------------------------------

    if ( aFilter != NULL )
    {
        // BUG-17921
        IDE_TEST( qmoNormalForm::optimizeForm( aFilter,
                                               & sOptimizedNode )
                  != IDE_SUCCESS );

        sAOJN->filter = sOptimizedNode;
    }
    else
    {
        sAOJN->filter = NULL;
    }

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    sPredicate[0] =  sAOJN->filter;

    //----------------------------------
    // PROJ-1437
    // dependency �������� predicate���� ��ġ���� ����. 
    //----------------------------------

    IDE_TEST( qmg::changeColumnLocate( aStatement,
                                       aQuerySet,
                                       sPredicate[0],
                                       ID_USHORT_MAX,
                                       ID_TRUE )
              != IDE_SUCCESS );
    
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            &sAOJN->plan ,
                                            QMO_AOJN_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            1 ,
                                            sPredicate,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sAOJN->plan.resultDesc,
                                     &sAOJN->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sAOJN->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sAOJN->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sAOJN->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sAOJN->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::initCONC( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aParent,
                         qcDepInfo    * aDepInfo,
                         qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : CONC ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncCONC�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncCONC    * sCONC;
    UInt          sDataNodeOffset;
    qmcAttrDesc * sItrAttr;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initCONC::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncCONC�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncCONC) , (void **)&sCONC )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sCONC ,
                        QC_SHARED_TMPLATE(aStatement) ,
                        QMN_CONC ,
                        qmnCONC ,
                        qmndCONC,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sCONC;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sCONC->plan.resultDesc )
              != IDE_SUCCESS );

    // BUG-38285
    // CONCATENATION ������ �÷��� ����
    // ������ ID�� TABLE�� ������ �����ϹǷ� �߰��� materialize�Ǵ� ������ ��ġ�� �����ؼ��� �ȵȴ�.

    for( sItrAttr = sCONC->plan.resultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        if ( qtc::dependencyContains( aDepInfo, &sItrAttr->expr->depInfo )
             == ID_TRUE )
        {
            sItrAttr->expr->node.lflag &= ~MTC_NODE_COLUMN_LOCATE_CHANGE_MASK;
            sItrAttr->expr->node.lflag |= MTC_NODE_COLUMN_LOCATE_CHANGE_FALSE;
        }
        else
        {
            // Nothing to do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::makeCONC( qcStatement  * aStatement ,
                         qmsQuerySet  * aQuerySet ,
                         qmnPlan      * aLeftChild ,
                         qmnPlan      * aRightChild ,
                         qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : CONC ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncCONC�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncCONC          *     sCONC = (qmncCONC *)aPlan;
    UInt                sDataNodeOffset;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeCONC::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    sCONC->plan.left    = aLeftChild;
    sCONC->plan.right   = aRightChild;

    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndCONC));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sCONC->flag      = QMN_PLAN_FLAG_CLEAR;
    sCONC->plan.flag = QMN_PLAN_FLAG_CLEAR;

    sCONC->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sCONC->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sCONC->plan ,
                                            QMO_CONC_DEPENDENCY,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sCONC->plan.resultDesc,
                                     &sCONC->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sCONC->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sCONC->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sCONC->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sCONC->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoTwoNonPlan::checkSimpleJOIN( qcStatement  * aStatement,
                                qmncJOIN     * aJOIN )
{
/***********************************************************************
 *
 * Description : simple join ������� �˻��Ѵ�.
 *
 * Implementation :
 *     simple join�� ��� fast execute�� �����Ѵ�.
 *
 ***********************************************************************/

    qmncJOIN  * sJOIN = aJOIN;
    idBool      sIsSimple = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::checkSimpleJOIN::__FT__" );

    IDE_TEST_CONT( qciMisc::checkExecFast( aStatement, NULL ) == ID_FALSE,
                   NORMAL_EXIT );

    sIsSimple = ID_TRUE;
    
    if ( ( ( sJOIN->flag & QMNC_JOIN_TYPE_MASK )
           != QMNC_JOIN_TYPE_INNER ) ||
         ( sJOIN->filter != NULL ) )
    {
        sIsSimple = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }

    IDE_EXCEPTION_CONT( NORMAL_EXIT );
    
    sJOIN->isSimple = sIsSimple;
    
    return IDE_SUCCESS;
}

IDE_RC qmoTwoNonPlan::initSREC( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmnPlan      * aParent,
                                qmnPlan     ** aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *     SREC ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSREC�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSREC * sSREC = NULL;
    UInt       sDataNodeOffset = 0;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::initSREC::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    // qmncSREC�� �Ҵ�� �ʱ�ȭ
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmncSREC),
                                             (void **)&sSREC )
              != IDE_SUCCESS );

    QMO_INIT_PLAN_NODE( sSREC,
                        QC_SHARED_TMPLATE(aStatement),
                        QMN_SREC,
                        qmnSREC,
                        qmndSREC,
                        sDataNodeOffset );

    *aPlan = (qmnPlan *)sSREC;

    IDE_TEST( qmc::pushResultDesc( aStatement,
                                   aQuerySet,
                                   ID_FALSE,
                                   aParent->resultDesc,
                                   &sSREC->plan.resultDesc )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoTwoNonPlan::makeSREC( qcStatement  * aStatement,
                                qmsQuerySet  * aQuerySet,
                                qmnPlan      * aLeftChild,
                                qmnPlan      * aRightChild,
                                qmnPlan      * aRecursiveChild,
                                qmnPlan      * aPlan )
{
/***********************************************************************
 *
 * Description : PROJ-2582 recursive with
 *      SREC ��带 �����Ѵ�
 *
 * Implementation :
 *     + �ʱ�ȭ �۾�
 *         - qmncSREC�� �Ҵ� �� �ʱ�ȭ
 *     + ���� �۾�
 *     + ������ �۾�
 *         - data ������ ũ�� ���
 *         - dependency�� ó��
 *         - subquery�� ó��
 *
 * TO DO
 *
 ***********************************************************************/

    qmncSREC    * sSREC = (qmncSREC *)aPlan;
    UInt          sDataNodeOffset = 0;

    IDU_FIT_POINT_FATAL( "qmoTwoNonPlan::makeSREC::__FT__" );

    //----------------------------------
    // ���ռ� �˻�
    //----------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aLeftChild != NULL );
    IDE_DASSERT( aRightChild != NULL );
    IDE_DASSERT( aRecursiveChild != NULL );

    //------------------------------------------------------------
    // �ʱ�ȭ �۾�
    //------------------------------------------------------------

    sSREC->plan.left    = aLeftChild;
    sSREC->plan.right   = aRightChild;
    
    if ( aRecursiveChild->type == QMN_VSCN )
    {
        sSREC->recursiveChild = aRecursiveChild;
    }
    else if ( aRecursiveChild->type == QMN_FILT )
    {
        IDE_TEST_RAISE( aRecursiveChild->left->type != QMN_VSCN,
                        ERR_INVALID_VSCN );
        
        sSREC->recursiveChild = aRecursiveChild->left;
    }
    else
    {
        IDE_RAISE( ERR_INVALID_VSCN );
    }
    
    aPlan->offset = aStatement->myPlan->sTmplate->tmplate.dataSize;
    sDataNodeOffset  = idlOS::align8(aPlan->offset +
                                     ID_SIZEOF(qmndSREC));

    //----------------------------------
    // Flag ����
    //----------------------------------

    sSREC->flag      = QMN_PLAN_FLAG_CLEAR;
    sSREC->plan.flag = QMN_PLAN_FLAG_CLEAR;
    
    sSREC->plan.flag       |= (aLeftChild->flag & QMN_PLAN_STORAGE_MASK);
    sSREC->plan.flag       |= (aRightChild->flag & QMN_PLAN_STORAGE_MASK);

    //------------------------------------------------------------
    // ������ �۾�
    //------------------------------------------------------------

    //data ������ ũ�� ���
    QC_SHARED_TMPLATE(aStatement)->tmplate.dataSize = sDataNodeOffset;

    //----------------------------------
    // dependency ó�� �� subquery�� ó��
    //----------------------------------

    // To Fix PR-12791
    // PROJ-1358 �� ���� dependency ���� �˰����� �����Ǿ�����
    // Query Set���� ������ dependency ������ �����ؾ� ��.
    IDE_TEST( qmoDependency::setDependency( aStatement ,
                                            aQuerySet ,
                                            & sSREC->plan ,
                                            QMO_SREC_DEPENDENCY,
                                            (UShort)qtc::getPosFirstBitSet( & aQuerySet->depInfo ),
                                            NULL ,
                                            0 ,
                                            NULL ,
                                            0 ,
                                            NULL )
              != IDE_SUCCESS );

    IDE_TEST( qmc::filterResultDesc( aStatement,
                                     &sSREC->plan.resultDesc,
                                     &sSREC->plan.depInfo,
                                     &(aQuerySet->depInfo) )
              != IDE_SUCCESS );

    /*
     * PROJ-1071 Parallel Query
     * parallel degree
     */
    if ( aLeftChild->mParallelDegree > aRightChild->mParallelDegree )
    {
        sSREC->plan.mParallelDegree = aLeftChild->mParallelDegree;
    }
    else
    {
        sSREC->plan.mParallelDegree = aRightChild->mParallelDegree;
    }
    sSREC->plan.flag &= ~QMN_PLAN_NODE_EXIST_MASK;
    sSREC->plan.flag |= ((aRightChild->flag | aLeftChild->flag) &
                         QMN_PLAN_NODE_EXIST_MASK);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_VSCN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoTwoNonPlan::makeSREC",
                                  "Invalid VSCN" ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
