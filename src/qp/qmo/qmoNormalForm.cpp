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
 * $Id: qmoNormalForm.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Normal Form Manager
 *
 *     ������ȭ�� Predicate���� ����ȭ�� ���·� �����Ű�� ��Ȱ�� �Ѵ�.
 *     ������ ���� ����ȭ�� �����Ѵ�.
 *         - CNF (Conjunctive Normal Form)
 *         - DNF (Disjunctive Normal Form)
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qtc.h>
#include <qmvQTC.h>
#include <qmoNormalForm.h>
#include <qmoCSETransform.h>

extern mtfModule mtfAnd;
extern mtfModule mtfOr;
extern mtfModule mtfNot;

IDE_RC
qmoNormalForm::normalizeCheckCNFOnly( qtcNode  * aNode,
                                      idBool   * aCNFonly )
{
/***********************************************************************
 *
 * Description : CNF �θ� ����Ǵ� ��츦 �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 *    CNF�θ� ����Ǵ� ���� ������ ����.
 *
 *    1. SFWGH�� subquery�� �ִ� ���
 *       normalization �ܰ迡�� node�� �����
 *       subquery�� �������� �ʴ´�.
 *       ����, DNF�� ó���� ������ subquery�� dependency ������
 *       �߸��Ǿ� ��� ������ �߻��Ѵ�.(���� BUG-6365)
 *       ��) where (t1.i1 >= t2.i1 or t1.i2 >= t2.i2 )
 *           and 1 = ( select count(*) from t3 where t1.i1 != t2.i2 );
 *
 *    2. SFWGH�� AND���길 �ִ� ���
 *       ��) i1=1 and i2=1 and i3=1
 *       ����: i1=1 and ( i2=1 and i3=1 ) and i4=1
 *       [ AND ���길 �ִ� ���� �ϴ��� ��ȣ�� ���� �ִ� ����
 *         CNF only �� �Ǵ����� �ʴ´�. ]
 *
 *    3. SFWGH�� �ϳ��� predicate�� �ִ� ���
 *       ��) i1=1
 *
 ***********************************************************************/

    qtcNode * sNode;
    qtcNode * sNodeTraverse;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::normalizeCheckCNFOnly::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode    != NULL );
    IDE_DASSERT( aCNFonly != NULL );

    sNode = aNode;

    //--------------------------------------
    // CNF only�� ���� ���� �˻�
    // ���ڷ� �Ѿ�� qtcNode�� �ֻ��� ��忡 ���ؼ� ���ǰ˻縦 �����Ѵ�.
    //--------------------------------------

    if( ( sNode->lflag & QTC_NODE_SUBQUERY_MASK )
        == QTC_NODE_SUBQUERY_EXIST )
    {
        // 1. SFWGH�� subquery�� �ִ� ���
        //    ( �ֻ��� ��忡�� subquery ���� ���� �˻� )
        *aCNFonly = ID_TRUE;
    }
    else if ((sNode->lflag & QTC_NODE_COLUMN_RID_MASK) ==
             QTC_NODE_COLUMN_RID_EXIST)
    {
        *aCNFonly = ID_TRUE;
    }
    else if( ( sNode->node.lflag &
             ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
             == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        // 2. SFWGH�� AND ���길 �ִ� ���.
        //    �ֻ��� ��尡 AND�̰�, AND ���� ��忡�� �񱳿����ڸ� ����
        //    �ϴ����� �˻�. ( ��, �������ڰ� �����ϸ�, CNFonly�� �ƴ�. )
        *aCNFonly = ID_TRUE;

        for( sNodeTraverse = (qtcNode *)(sNode->node.arguments);
             sNodeTraverse != NULL;
             sNodeTraverse = (qtcNode *)(sNodeTraverse->node.next) )
        {
            if( ( sNodeTraverse->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                // AND ���� ��忡 �������ڰ� �����ϸ�, CNFonly�� �ƴ�.
                *aCNFonly = ID_FALSE;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else if( ( sNode->node.lflag & MTC_NODE_COMPARISON_MASK )
               == MTC_NODE_COMPARISON_TRUE )
    {
        // 3. SFWGH�� �ϳ��� predicate�� �ִ� ���
        //    ( �ֻ��� ��尡 �񱳿����������� �˻� )
        *aCNFonly = ID_TRUE;
    }
    else
    {
        *aCNFonly = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoNormalForm::normalizeDNF( qcStatement   * aStatement,
                             qtcNode       * aNode,
                             qtcNode      ** aDNF )
{
/***********************************************************************
 *
 * Description : DNF �� ����ȭ
 *     DNF�� �ۼ��� predicate�� �������ڸ� �������� (OR of AND's)��
 *     ���·� ǥ���ϴ� ����̴�.
 *
 * Implementation :
 *     DNF normalization�� where���� ó���Ѵ�.
 *     1. DNF ����ȭ���·� ��ȯ
 *     2. DNF form ���������� flag�� dependency �缳��
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoNormalForm::normalizeDNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );
    IDE_DASSERT( aDNF       != NULL );

    //--------------------------------------
    // DNF normal form���� ��ȯ
    //--------------------------------------

    IDE_TEST( makeDNF( aStatement, aNode, aDNF ) != IDE_SUCCESS );

    //--------------------------------------
    // PROJ-2242 : CSE transformation
    //--------------------------------------

    IDE_TEST( qmoCSETransform::doTransform( aStatement, aDNF, ID_FALSE )
              != IDE_SUCCESS );

    //--------------------------------------
    // DNF form ���������� flag�� dependencies �缳��
    //--------------------------------------

    IDE_TEST( setFlagAndDependencies( *aDNF ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoNormalForm::normalizeCNF( qcStatement   * aStatement,
                             qtcNode       * aNode,
                             qtcNode      ** aCNF,
                             idBool          aIsWhere,
                             qmsHints      * aHints )
{
/***********************************************************************
 *
 * Description : CNF �� ����ȭ
 *     CNF�� �ۼ��� predicate�� �� �����ڸ� �������� (AND of OR's)��
 *     ���·� ǥ���ϴ� ����̴�.
 *
 * Implementation :
 *     DNF�� �޸� �پ��� ������ �������� ó���Ѵ�.
 *     1. CNF ����ȭ ���·� ��ȯ
 *     2. CNF form ���������� flag�� dependency�� �缳��
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoNormalForm::normalizeCNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );
    IDE_DASSERT( aCNF       != NULL );

    //--------------------------------------
    // CNF normal form���� ��ȯ
    //--------------------------------------

    IDE_TEST( makeCNF( aStatement, aNode, aCNF ) != IDE_SUCCESS );

    //--------------------------------------
    // PROJ-2242 : CSE transformation
    //--------------------------------------

    IDE_TEST( qmoCSETransform::doTransform( aStatement,
                                            aCNF,
                                            ID_FALSE,
                                            aIsWhere,
                                            aHints )
              != IDE_SUCCESS );

    //--------------------------------------
    // CNF form ���������� flag�� dependencies �缳��
    //--------------------------------------

    IDE_TEST( setFlagAndDependencies( *aCNF ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNormalForm::estimateDNF( qtcNode  * aNode,
                            UInt     * aCount )
{
/***********************************************************************
 *
 * Description : DNF�� ����ȭ�Ǿ����� ���������
 *               �񱳿����� ����� ���� ����.
 *     ��) where i1=1 and (i2=1 or i3=1 or (i4=1 and i5=1))
 *         DNF ��� ���� = 2 * ( 2 + ( (1*1) + (1*1) ) ) = 8
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode           * sNNFArg;
    UInt                sNNFCount;
    SDouble             sCount;
    UInt                sChildCount;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::estimateDNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode  != NULL );
    IDE_DASSERT( aCount != NULL );

    //--------------------------------------
    // DNF�� ġȯ�� ����� Node���� ����
    //--------------------------------------

    if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        sCount = 1;
        sNNFCount = 0;

        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            sNNFCount++;

            estimateDNF( sNNFArg, & sChildCount );
            if( sNNFCount != 0 && sCount >= (UINT_MAX/2) )
            {
                *aCount = UINT_MAX;

                return IDE_SUCCESS;
            }
            sCount *= sChildCount;
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
        sCount *= sNNFCount;
    }
    else if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        sCount = 0;
        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            estimateDNF( sNNFArg, & sNNFCount );
            if( sNNFCount != 0 && sCount >= (UINT_MAX/2) )
            {
                *aCount = UINT_MAX;

                return IDE_SUCCESS;
            }
            sCount += sNNFCount;
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
    }
    else
    {
        sCount = 1;
    }

    // To fix BUG-14846
    // UInt overflow����
    if( sCount >= UINT_MAX )
    {
        *aCount = UINT_MAX;
    }
    else
    {
        *aCount = (UInt)sCount;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoNormalForm::estimateCNF( qtcNode  * aNode,
                            UInt     * aCount )
{
/***********************************************************************
 *
 * Description : CNF�� ����ȭ�Ǿ����� ���������
 *               �񱳿����� ����� ���� ����
 *     ��) where i1=1 and (i2=1 or i3=1 or (i4=1 and i5=1))
 *         CNF ��� ���� = 1 + ( 3 * ( 1 + 1 ) ) = 7
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode           * sNNFArg;
    UInt                sNNFCount;
    SDouble             sCount;
    UInt                sChildCount;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::estimateCNF::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode  != NULL );
    IDE_DASSERT( aCount != NULL );

    //--------------------------------------
    // CNF�� ġȯ�� ����� Node���� ����
    //--------------------------------------

    if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        sCount = 1;
        sNNFCount = 0;

        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            sNNFCount++;

            estimateCNF( sNNFArg, & sChildCount );
            if( sNNFCount != 0 && sCount >= (UINT_MAX/2) )
            {
                *aCount = UINT_MAX;

                return IDE_SUCCESS;
            }
            sCount *= sChildCount;
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
        sCount *= sNNFCount;
    }
    else if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        sCount = 0;
        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            estimateCNF( sNNFArg, & sNNFCount );
            if( sNNFCount != 0 && sCount >= (UINT_MAX/2) )
            {
                *aCount = UINT_MAX;

                return IDE_SUCCESS;
            }
            sCount += sNNFCount;
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
    }
    else
    {
        sCount = 1;
    }

    // To fix BUG-14846
    // UInt overflow����
    if( sCount >= UINT_MAX )
    {
        *aCount = UINT_MAX;
    }
    else
    {
        *aCount = (UInt)sCount;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoNormalForm::makeDNF( qcStatement  * aStatement,
                        qtcNode      * aNode,
                        qtcNode     ** aDNF)
{
/***********************************************************************
 *
 * Description : DNF ���·� predicate ��ȯ
 *
 * Implementation :
 *     1. OR  ���� ���� addToMerge
 *     1. AND ���� ���� productToMerge
 *     2. �񱳿����� ���
 *        (1) OR ��� ����
 *        (2) AND ��� ����
 *        (3) ���ο� ��忡 predicate ����
 *        (4) OR->AND->predicate ������ ����
 *
 ***********************************************************************/

    qtcNode           * sNode;
    qtcNode           * sTmpNode[2];
    qtcNode           * sDNFNode;
    qtcNode           * sPrevDNF = NULL;
    qtcNode           * sCurrDNF = NULL;
    qtcNode           * sNNFArg = NULL;
    qcNamePosition      sNullPosition;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::makeDNF::__FT__" );

    IDE_FT_ASSERT( aNode != NULL );

    if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        IDE_FT_ASSERT( aNode->node.arguments != NULL );

        sNNFArg = (qtcNode *)(aNode->node.arguments);
        IDE_TEST(makeDNF(aStatement, sNNFArg, &sPrevDNF) != IDE_SUCCESS);
        sDNFNode = sPrevDNF;

        sNNFArg = (qtcNode *)(sNNFArg->node.next);

        while (sNNFArg != NULL)
        {
            IDE_TEST(makeDNF(aStatement, sNNFArg, &sCurrDNF) != IDE_SUCCESS);

            IDE_TEST(productToMerge(aStatement, sPrevDNF, sCurrDNF, &sDNFNode)
                     != IDE_SUCCESS);

            sPrevDNF = sDNFNode;
            sNNFArg  = (qtcNode *)(sNNFArg->node.next);
        }
    }
    else if ( ( aNode->node.lflag &
            ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
            == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        IDE_FT_ASSERT( aNode->node.arguments != NULL );

        sNNFArg = (qtcNode *)(aNode->node.arguments);
        IDE_TEST(makeDNF(aStatement, sNNFArg, &sPrevDNF) != IDE_SUCCESS);
        sDNFNode = sPrevDNF;

        sNNFArg = (qtcNode *)(sNNFArg->node.next);
        while (sNNFArg != NULL)
        {
            IDE_TEST(makeDNF(aStatement, sNNFArg, &sCurrDNF) != IDE_SUCCESS);

            IDE_TEST(addToMerge(sPrevDNF, sCurrDNF, &sDNFNode)
                     != IDE_SUCCESS);

            sPrevDNF = sDNFNode;
            sNNFArg  = (qtcNode *)(sNNFArg->node.next);
        }
    }
    else
    { // terminal predicate node

        SET_EMPTY_POSITION(sNullPosition);

        IDE_TEST( qtc::makeNode( aStatement,
                                 sTmpNode,
                                 & sNullPosition,
                                 (const UChar*)"OR",
                                 2 )
                  != IDE_SUCCESS );
        sDNFNode = sTmpNode[0];

        sDNFNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
        sDNFNode->node.lflag |= 1;

        IDE_TEST( qtc::makeNode( aStatement,
                                 sTmpNode,
                                 & sNullPosition,
                                 (const UChar*)"AND",
                                 3 )
                  != IDE_SUCCESS );
        sNode = sTmpNode[0];

        sNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
        sNode->node.lflag |= 1;

        sDNFNode->node.arguments = (mtcNode *)sNode;

        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
                 != IDE_SUCCESS);

        idlOS::memcpy(sNode, aNode, ID_SIZEOF(qtcNode));
        sNode->node.next = NULL;

        sDNFNode->node.arguments->arguments = (mtcNode *)sNode;

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    (qtcNode *)( sDNFNode->node.arguments ) )
                  != IDE_SUCCESS );

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sDNFNode )
                  != IDE_SUCCESS );
    }

    *aDNF = sDNFNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoNormalForm::makeCNF( qcStatement  * aStatement,
                        qtcNode      * aNode,
                        qtcNode     ** aCNF )
{
/***********************************************************************
 *
 * Description : CNF ���·� predicate ��ȯ
 *
 *     BUG-35155 Partial CNF
 *     mtcNode �� MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE flag ��
 *     �����Ǿ� ������ normalize ���� �ʴ´�.
 *
 * Implementation :
 *     1. AND  ���� ���� addToMerge
 *     1. OR   ���� ���� productToMerge
 *     2. �񱳿����� ���
 *        (1) AND ��� ����
 *        (2) OR ��� ����
 *        (3) ���ο� ��忡 predicate ����
 *        (4) AND->OR->predicate ������ ����
 *
 ***********************************************************************/

    qtcNode           * sNode;
    qtcNode           * sCNFNode;
    qtcNode           * sTmpNode[2];
    qtcNode           * sPrevCNF = NULL;
    qtcNode           * sCurrCNF = NULL;
    qtcNode           * sNNFArg = NULL;
    qcNamePosition      sNullPosition;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::makeCNF::__FT__" );

    if ( ( aNode->node.lflag & MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK )
           != MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE )
    {
        if ( ( aNode->node.lflag &
                ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
                == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
        {
            /* TASK-7219 Non-shard DML */
             if ( ( aNode->node.lflag & MTC_NODE_PUSHED_PRED_FORCE_MASK )
                   == MTC_NODE_PUSHED_PRED_FORCE_TRUE )
            {
                aNode->node.arguments->lflag &= ~MTC_NODE_PUSHED_PRED_FORCE_MASK;
                aNode->node.arguments->lflag |= MTC_NODE_PUSHED_PRED_FORCE_TRUE;
            }

            sNNFArg = (qtcNode *)(aNode->node.arguments);
            IDE_TEST(makeCNF(aStatement, sNNFArg, &sPrevCNF) != IDE_SUCCESS);
            sCNFNode = sPrevCNF;

            sNNFArg = (qtcNode *)(sNNFArg->node.next);

            while (sNNFArg != NULL)
            {
                IDE_TEST( makeCNF( aStatement,
                                   sNNFArg,
                                   & sCurrCNF )
                          != IDE_SUCCESS );

                IDE_TEST(addToMerge2(sPrevCNF, sCurrCNF, &sCNFNode)
                            != IDE_SUCCESS);

                sPrevCNF = sCNFNode;
                sNNFArg  = (qtcNode *)(sNNFArg->node.next);
            }
        }
        else if ( ( aNode->node.lflag &
                ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
                == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
        {
            /* TASK-7219 Non-shard DML */
            if ( ( aNode->node.lflag & MTC_NODE_PUSHED_PRED_FORCE_MASK )
                   == MTC_NODE_PUSHED_PRED_FORCE_TRUE )
            {
                aNode->node.arguments->lflag &= ~MTC_NODE_PUSHED_PRED_FORCE_MASK;
                aNode->node.arguments->lflag |= MTC_NODE_PUSHED_PRED_FORCE_TRUE;
            }

            sNNFArg = (qtcNode *)(aNode->node.arguments);
            IDE_TEST(makeCNF(aStatement, sNNFArg, &sPrevCNF) != IDE_SUCCESS);
            sCNFNode = sPrevCNF;

            sNNFArg = (qtcNode *)(sNNFArg->node.next);

            while (sNNFArg != NULL)
            {
                IDE_TEST( makeCNF( aStatement,
                                   sNNFArg,
                                   & sCurrCNF )
                          != IDE_SUCCESS );

                IDE_TEST( productToMerge( aStatement,
                                          sPrevCNF,
                                          sCurrCNF,
                                          & sCNFNode )
                          != IDE_SUCCESS );

                sPrevCNF = sCNFNode;
                sNNFArg  = (qtcNode *)(sNNFArg->node.next);
            }
        }
        else
        { // terminal predicate node
            SET_EMPTY_POSITION(sNullPosition);

            IDE_TEST( qtc::makeNode( aStatement,
                                     sTmpNode,
                                     & sNullPosition,
                                     (const UChar*)"AND",
                                     3 )
                      != IDE_SUCCESS );
            sCNFNode = sTmpNode[0];

            sCNFNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
            sCNFNode->node.lflag |= 1;

            IDE_TEST( qtc::makeNode( aStatement,
                                     sTmpNode,
                                     & sNullPosition,
                                     (const UChar*)"OR",
                                     2 )
                      != IDE_SUCCESS );
            sNode = sTmpNode[0];

            sNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
            sNode->node.lflag |= 1;

            /* TASK-7219 Non-shard DML */
            if ( ( aNode->node.lflag & MTC_NODE_PUSHED_PRED_FORCE_MASK )
                   == MTC_NODE_PUSHED_PRED_FORCE_TRUE )
            {
                sNode->node.lflag &= ~MTC_NODE_PUSHED_PRED_FORCE_MASK;
                sNode->node.lflag |= MTC_NODE_PUSHED_PRED_FORCE_TRUE;
            }

            sCNFNode->node.arguments = (mtcNode *)sNode;

            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
                     != IDE_SUCCESS);

            idlOS::memcpy(sNode, aNode, ID_SIZEOF(qtcNode));
            sNode->node.next = NULL;

            sCNFNode->node.arguments->arguments = (mtcNode *)sNode;

            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        (qtcNode *)( sCNFNode->node.arguments ) )
                      != IDE_SUCCESS );

            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        sCNFNode )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // CNF UNUSABLE �̸� CNF �� ����ȭ���� �ʴ´�.
        sCNFNode = NULL;
    }
    *aCNF = sCNFNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNormalForm::addToMerge( qtcNode     * aPrevNF,
                           qtcNode     * aCurrNF,
                           qtcNode    ** aNFNode)
{
/***********************************************************************
 *
 * Description : ����ȭ ���·� ��ȯ�ϴ� �������� predicate�� ����.
 *
 * Implementation :
 *     1. DNF
 *         OR�� ���� ��� ó�� ��, �� AND ��带 �����Ѵ�.
 *     2. CNF
 *         AND�� ���� ��� ó�� ��, �� OR ��带 �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode     * sNode;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::addToMerge::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPrevNF != NULL );
    IDE_DASSERT( aCurrNF != NULL );
    IDE_DASSERT( aNFNode != NULL );

    //--------------------------------------
    // add to merge
    //--------------------------------------

    sNode = (qtcNode *)(aPrevNF->node.arguments);
    while (sNode->node.next != NULL)
    {
        sNode = (qtcNode *)(sNode->node.next);
    }
    sNode->node.next = aCurrNF->node.arguments;

    *aNFNode = aPrevNF;

    return IDE_SUCCESS;
}

IDE_RC
qmoNormalForm::productToMerge( qcStatement * aStatement,
                               qtcNode     * aPrevNF,
                               qtcNode     * aCurrNF,
                               qtcNode    ** aNFNode)
{
/***********************************************************************
 *
 * Description : ����ȭ ���·� ��ȯ�ϴ� ��������
 *               predicate�� ���� ��й�Ģ����
 *
 * Implementation :
 *     1. DNF
 *         AND ���� ��� ó����, ��й�Ģ ����
 *     2. CNF
 *         OR  ���� ��� ó����, ��й�Ģ ����
 *
 ***********************************************************************/

    qtcNode     * sNode;
    qtcNode     * sArg;
    qtcNode     * sLastNode;
    qtcNode     * sLastArg;
    qtcNode     * sNewNode;
    qtcNode     * sNewArg;
    qtcNode     * sPrevNewArg = NULL;
    SInt          sPrevCnt;
    SInt          sCurrCnt;
    SInt          i;
    SInt          j;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::productToMerge::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPrevNF    != NULL );
    IDE_DASSERT( aCurrNF    != NULL );
    IDE_DASSERT( aNFNode    != NULL );

    //--------------------------------------
    // product to merge
    //--------------------------------------

    for (sPrevCnt = 0,
             sNode = (qtcNode *)(aPrevNF->node.arguments);
         sNode != NULL;
         sNode = (qtcNode *)(sNode->node.next))
    {
        sPrevCnt++;
    }

    for (sCurrCnt = 0,
             sNode = (qtcNode *)(aCurrNF->node.arguments);
         sNode != NULL;
         sNode = (qtcNode *)(sNode->node.next))
    {
        sCurrCnt++;
    }

    // memory alloc for saving product result
    sLastNode = (qtcNode *)(aPrevNF->node.arguments);
    for (i = sCurrCnt - 1; i > 0; i--)
    {
        while(sLastNode->node.next != NULL)
        {
            sLastNode = (qtcNode *)(sLastNode->node.next);
        }

        for (j = 0,
                 sNode = (qtcNode *)(aPrevNF->node.arguments);
             j < sPrevCnt;
             j++,
                 sNode = (qtcNode *)(sNode->node.next))
        {
            IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNewNode)
                     != IDE_SUCCESS);

            idlOS::memcpy(sNewNode, sNode, ID_SIZEOF(qtcNode));
            sNewNode->node.arguments = NULL;
            sNewNode->node.next      = NULL;

            for (sArg = (qtcNode *)(sNode->node.arguments);
                 sArg != NULL;
                 sArg = (qtcNode *)(sArg->node.next))
            {
                IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNewArg)
                         != IDE_SUCCESS);

                idlOS::memcpy(sNewArg, sArg, ID_SIZEOF(qtcNode));
                sNewArg->node.next = NULL;

                // link
                if (sNewNode->node.arguments == NULL)
                {
                    sNewNode->node.arguments = (mtcNode *)sNewArg;
                    sPrevNewArg                = sNewArg;
                }
                else
                {
                    sPrevNewArg->node.next = (mtcNode *)sNewArg;
                    sPrevNewArg            = sNewArg;
                }
            }

            // link
            sLastNode->node.next = (mtcNode *)sNewNode;
            sLastNode            = sNewNode;
        }
    }

    // product
    sLastNode = (qtcNode *)(aPrevNF->node.arguments);
    for (i = 0,
             sNode = (qtcNode *)(aCurrNF->node.arguments);
         i < sCurrCnt;
         i++,
             sNode = (qtcNode *)(sNode->node.next))
    {
        for (j = 0;
             j < sPrevCnt;
             j++,
                 sLastNode = (qtcNode *)(sLastNode->node.next))
        {
            sLastArg = (qtcNode *)(sLastNode->node.arguments);
            while (sLastArg->node.next != NULL)
            {
                sLastArg = (qtcNode *)(sLastArg->node.next);
            }

            for (sArg = (qtcNode *)(sNode->node.arguments);
                 sArg != NULL;
                 sArg = (qtcNode *)(sArg->node.next))
            {
                IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNewArg)
                         != IDE_SUCCESS);

                idlOS::memcpy(sNewArg, sArg, ID_SIZEOF(qtcNode));
                sNewArg->node.next = NULL;

                // link
                sLastArg->node.next = (mtcNode *)sNewArg;
                sLastArg            = sNewArg;
            }
        }
    }

    // return product result
    *aNFNode = aPrevNF;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoNormalForm::setFlagAndDependencies(qtcNode * aNode)
{
/***********************************************************************
 *
 * Description : ����ȭ ���·� ��ȯ�ϸ鼭 ���� ���ο� �������� ��忡
 *               ���� flag�� dependency ����
 *
 * Implementation :
 *     ����ȭ ���·� ��ȯ�ϴ� �������� �� �����ڵ��� ��� ����
 *     ���� �����ϰ� �ȴ�. ����, �� ������ ��忡 ���ؼ���
 *     parsing & validation�������� ������ flag�� dependency������
 *     ��� ������� �ǹǷ�, �̿� ���� ������ �� �־�� �ȴ�.
 *
 *     ���� ����� flag�� dependency ������ merge �Ѵ�.
 *     [���� ����� flag�� merge�ؼ� ��� ������ QTC_NODE_MASK ����]
 *
 ***********************************************************************/

    qtcNode     * sNode;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::setFlagAndDependencies::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // Flag�� Dependency ����
    //--------------------------------------

    if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        qtc::dependencyClear( & aNode->depInfo );

        for (sNode = (qtcNode *)(aNode->node.arguments);
             sNode != NULL;
             sNode = (qtcNode *)(sNode->node.next))
        {
            IDE_TEST( setFlagAndDependencies(sNode) != IDE_SUCCESS );

            aNode->node.lflag |=
                sNode->node.lflag & aNode->node.module->lmask & MTC_NODE_MASK;
            aNode->lflag |= sNode->lflag & QTC_NODE_MASK;

            IDE_TEST( qtc::dependencyOr( & aNode->depInfo,
                                         & sNode->depInfo,
                                         & aNode->depInfo )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoNormalForm::optimizeForm( qtcNode  * aInputNode,
                             qtcNode ** aOutputNode )
{
/***********************************************************************
 *
 * Description : ����ȭ ���·� ��ȯ�� predicate�� ����ȭ �Ѵ�.
 *
 * Implementation :
 *     CNF�� ��ȯ�� predicate�� Filter�� ó���Ǳ� ������
 *     ���ʿ���  AND, OR�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sNode;
    qtcNode * sNewNode;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::optimizeForm::__FT__" );

    //-------------------------------------
    // ���ռ� �˻�
    //-------------------------------------

    IDE_DASSERT( aInputNode != NULL );

    if( ( aInputNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        switch( aInputNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        {
            case MTC_NODE_OPERATOR_AND:
            case MTC_NODE_OPERATOR_OR:
                // AND�� OR ����� argument�� �ϳ��� ���
                // �� AND �Ǵ� OR ���� ���ŵ� �� ����.
                IDE_FT_ASSERT( aInputNode->node.arguments != NULL );

                if( aInputNode->node.arguments->next == NULL )
                {
                    IDE_TEST( optimizeForm( (qtcNode*) aInputNode->node.arguments,
                                            aOutputNode )
                              != IDE_SUCCESS );
                }
                else
                {
                    sNewNode = NULL;

                    for (sNode  = (qtcNode *)(aInputNode->node.arguments);
                         sNode != NULL;
                         sNode  = (qtcNode *)(sNode->node.next))
                    {
                        if( sNewNode == NULL )
                        {
                            IDE_TEST( optimizeForm( sNode,
                                                    &sNewNode )
                                      != IDE_SUCCESS );

                            aInputNode->node.arguments = (mtcNode*)sNewNode;
                        }
                        else
                        {
                            IDE_TEST( optimizeForm( sNode,
                                                    (qtcNode**)& sNewNode->node.next )
                                      != IDE_SUCCESS );

                            sNewNode = (qtcNode*)sNewNode->node.next;
                        }
                    }
                    *aOutputNode = aInputNode;
                }
                break;
            case MTC_NODE_OPERATOR_NOT:
                // NOT ���� ���ŵǾ�� �ȵǸ� ���� ��忡 ����
                // optimizeForm�� �����ϴ�.
                IDE_TEST( optimizeForm( (qtcNode*)aInputNode->node.arguments,
                                        & sNewNode )
                          != IDE_SUCCESS );
                aInputNode->node.arguments = (mtcNode*)sNewNode;

                *aOutputNode = aInputNode;
                break;
            default:
                IDE_FT_ASSERT( 0 );
                break;
        }
    }
    else
    {
        *aOutputNode = aInputNode;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void
qmoNormalForm::estimatePartialCNF( qtcNode  * aNode,
                                   UInt     * aCount,
                                   qtcNode  * aRoot,
                                   UInt       aNFMaximum )
{
/***********************************************************************
 *
 * Description :
 *         BUG-35155 Partial CNF
 *         CNF�� ����ȭ�Ǿ����� ��������� �񱳿����� ����� ���� ����
 *         ���� aNFMaximum �� ���� ��� �ش� ����� �ֻ��� OR ���(aRoot)��
 *         partial normalize ���� �����ϱ� ���� CNF_UNUSABLE flag �� �����Ѵ�.
 *    ��1) where i1=1 and (i2=1 or i3=1 or (i4=1 and i5=1))
 *         CNF ��� ���� = 1 + ( 3 * ( 1 + 1 ) ) = 7
 *    ��2) where i1=1 and (i2=1 or i3=1 or ((i4=1 and (i5=1 or (i6=1 and ...))
 *         CNF ��� ���� = 1 + ( 3 * (1) ) = 6  (�Ϻ� ��尡 CNF ���� ���ܵ�)
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode           * sNNFArg;
    UInt                sNNFCount;
    SDouble             sCount;
    UInt                sChildCount;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode  != NULL );
    IDE_DASSERT( aCount != NULL );

    //--------------------------------------
    // CNF�� ġȯ�� ����� Node���� ����
    //--------------------------------------
    if ( ( aNode->node.lflag &
           ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
           == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_OR ) )
    {
        // Check aRoot
        if ( aRoot != NULL )
        {
            // aRoot is OR node
            // Nothing to do.
        }
        else
        {
            // Change aRoot to first OR node. (this node)
            aRoot = aNode;
        }

        sCount = 1;
        sNNFCount = 0;

        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            sNNFCount++;

            estimatePartialCNF( sNNFArg, & sChildCount, aRoot, aNFMaximum );
            sCount *= sChildCount;

            if( sCount*sNNFCount > aNFMaximum )
            {
                // �� ���� ��������� count �� ����� ���� NORMALFORM_MAXIMUM �� �Ѿ��.
                // ���� ��� �������� �ֻ��� OR ���(aRoot)�� UNUSABLE flag �� �����Ѵ�.
                aRoot->node.lflag &= ~MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK;
                aRoot->node.lflag |=  MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE;
                break;
            }
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
        sCount *= sNNFCount;
    }
    else if ( ( aNode->node.lflag &
                ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
                == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
    {
        sCount = 0;
        sNNFArg = (qtcNode *)(aNode->node.arguments);
        while( sNNFArg )
        {
            estimatePartialCNF( sNNFArg, & sNNFCount, aRoot, aNFMaximum );
            sCount += sNNFCount;

            if( sCount > aNFMaximum )
            {
                // �� ���� ��������� count �� ����� ���� NORMALFORM_MAXIMUM �� �Ѿ��.
                // ���� ��� �������� �ֻ��� OR ���(aRoot)�� UNUSABLE flag �� �����Ѵ�.
                if( aRoot == NULL )
                {
                    // aRoot �� NULL �̸� ������ OR ��尡 ���� ���̴�.
                    // �� ������ �ڱ� �ڽ��� CNF ��󿡼� �����Ѵ�.
                    aRoot = aNode;
                }
                aRoot->node.lflag &= ~MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK;
                aRoot->node.lflag |=  MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE;
                break;
            }
            sNNFArg = (qtcNode *)(sNNFArg->node.next);
        }
    }
    else
    {
        sCount = 1;
    }

    // ���� ��尡 UNUSABLE �̸� CNF ��󿡼� ���� �ǹǷ� count �� 0 ���� �ٲ��ش�.
    if ( ( aNode->node.lflag & MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK )
           == MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE )
    {
        // count �� ���ϹǷ� 0�� ��ȯ�Ѵ�.
        sCount = 0;
    }

    // UInt overflow����
    if( sCount >= UINT_MAX )
    {
        *aCount = UINT_MAX;
    }
    else
    {
        *aCount = (UInt)sCount;
    }

    return;
}

IDE_RC
qmoNormalForm::extractNNFFilter4CNF( qcStatement  * aStatement,
                                     qtcNode      * aNode,
                                     qtcNode     ** aNNF )
{
/***********************************************************************
 *
 * Description :
 *         BUG-35155 Partial CNF
 *         CNF �� ��ȯ �� ���ܵ� predicate ���� NNF filter �� �����.
 *
 * Implementation :
 *         �ֻ��� qtcNode �� CNF UNUSABLE �� ���� ��� ��ü�� NNF ���ͷ� ��ȯ�Ѵ�.
 *         �� �ܿ��� qtcNode �� �����ؼ� NNF ���͸� ���� ��
 *         flag �� dependency ������ �����Ͽ� ��ȯ�Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoNormalForm::extractNNFFilter4CNF::__FT__" );

    if ( ( aNode->node.lflag & MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK )
           == MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE )
    {
        // �ֻ��� node �� UNUSABLE �̸� where ���� ��ü�� nnf filter �� �ȴ�.
        *aNNF = aNode;
    }
    else
    {
        //--------------------------------------
        // NNF filter �� �����Ѵ�.
        //--------------------------------------
        IDE_TEST( makeNNF4CNFByCopyNodeTree( aStatement,
                                             aNode,
                                             aNNF )
                  != IDE_SUCCESS );

        if ( *aNNF != NULL )
        {
            //--------------------------------------
            // NNF form ���������� flag�� dependencies �缳��
            //--------------------------------------
            IDE_TEST( setFlagAndDependencies( *aNNF ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNormalForm::makeNNF4CNFByCopyNodeTree( qcStatement  * aStatement,
                                          qtcNode      * aNode,
                                          qtcNode     ** aNNF )
{
/***********************************************************************
 *
 * Description :
 *         BUG-35155 Partial CNF
 *         CNF �� ��ȯ �� ���ܵ� predicate ���� NNF filter �� �����.
 *
 * Implementation :
 *         MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE �� ����
 *         ���� ��带 �����ؼ� �����Ͽ� NNF ���ͷ� �����.
 *
 ***********************************************************************/

    qtcNode           * sNode;
    qtcNode           * sNNFNode = NULL;
    qtcNode           * sTmpNode[2];
    qtcNode           * sNNFArg = NULL;
    qcNamePosition      sNullPosition;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::makeNNF4CNFByCopyNodeTree::__FT__" );

    if ( ( aNode->node.lflag & MTC_NODE_PARTIAL_NORMALIZE_CNF_MASK )
           == MTC_NODE_PARTIAL_NORMALIZE_CNF_UNUSABLE )
    {
        // make AND node
        SET_EMPTY_POSITION(sNullPosition);

        IDE_TEST( qtc::makeNode( aStatement,
                                 sTmpNode,
                                 &sNullPosition,
                                 (const UChar*)"AND",
                                 3 )
                  != IDE_SUCCESS );
        sNNFNode = sTmpNode[0];

        sNNFNode->node.lflag &= ~MTC_NODE_ARGUMENT_COUNT_MASK;
        sNNFNode->node.lflag |= 1;

        // copy node tree
        IDE_TEST( copyNodeTree( aStatement, aNode, &sNode ) != IDE_SUCCESS );

        sNNFNode->node.arguments = (mtcNode *)sNode;

        IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                    sNNFNode )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( ( aNode->node.lflag &
               ( MTC_NODE_LOGICAL_CONDITION_MASK | MTC_NODE_OPERATOR_MASK ) )
               == ( MTC_NODE_LOGICAL_CONDITION_TRUE | MTC_NODE_OPERATOR_AND ) )
        {
            sNNFArg = (qtcNode *)(aNode->node.arguments);

            IDE_TEST( makeNNF4CNFByCopyNodeTree( aStatement,
                                                 sNNFArg,
                                                 & sNode )
                      != IDE_SUCCESS );

            sNNFNode = sNode;

            sNNFArg = (qtcNode *)(sNNFArg->node.next);

            while ( sNNFArg != NULL )
            {
                IDE_TEST(  makeNNF4CNFByCopyNodeTree( aStatement,
                                                      sNNFArg,
                                                      & sNode )
                           != IDE_SUCCESS );

                IDE_TEST( addToMerge2( sNNFNode, sNode, &sNNFNode )
                          != IDE_SUCCESS );

                sNNFArg  = (qtcNode *)(sNNFArg->node.next);
            }
        }
        else
        {
            // OR node or terminal predicate node
            // Nothing to do.
        }
    }

    *aNNF = sNNFNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNormalForm::copyNodeTree( qcStatement  * aStatement,
                             qtcNode      * aNode,
                             qtcNode     ** aCopy )
{
/***********************************************************************
 *
 * Description :
 *         BUG-35155 Partial CNF
 *         NNF ���� ������ ���� qtcNode �� ���� ��带 �����Ͽ� �����Ѵ�.
 *
 * Implementation :
 *         �� �������� ���� �ڱ� �ڽ��� �����ϰ� argument ��带 ���ȣ���Ͽ� �����Ѵ�.
 *         �� �������� ���� �ڱ� �ڽ��� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode           * sNode;
    qtcNode           * sTree;
    qtcNode           * sCursor;
    qtcNode           * sNNFArg = NULL;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::copyNodeTree::__FT__" );

    // copy qtcNodes recursivly
    if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
           == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        //---------------------------------
        // 1. Copy this node
        //---------------------------------
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
                 != IDE_SUCCESS);

        idlOS::memcpy(sNode, aNode, ID_SIZEOF(qtcNode));
        sNode->node.next = NULL;

        sTree = sNode;

        //---------------------------------
        // 2. copy argument
        //---------------------------------
        sNNFArg = (qtcNode *)(aNode->node.arguments);
        IDE_TEST( copyNodeTree( aStatement, sNNFArg, &sNode ) != IDE_SUCCESS );

        // Attach to tree
        sTree->node.arguments = (mtcNode *)sNode;

        sCursor = sNode;

        //---------------------------------
        // 3. copy next nodes of argument
        //---------------------------------
        for ( sNNFArg  = (qtcNode *)(sNNFArg->node.next);
              sNNFArg != NULL;
              sNNFArg  = (qtcNode *)(sNNFArg->node.next),
              sCursor  = (qtcNode *)(sCursor->node.next) )
        {
            IDE_TEST( copyNodeTree( aStatement,
                                    sNNFArg,
                                    & sNode )
                      != IDE_SUCCESS );

            // Attach to previous node
            sCursor->node.next = (mtcNode *)sNode;
        }
    }
    else
    { // terminal predicate node
        //---------------------------------
        // 1. Copy this node
        //---------------------------------
        IDE_TEST(STRUCT_ALLOC(QC_QMP_MEM(aStatement), qtcNode, &sNode)
                 != IDE_SUCCESS);

        idlOS::memcpy(sNode, aNode, ID_SIZEOF(qtcNode));
        sNode->node.next = NULL;

        sTree = sNode;
    }

    *aCopy = sTree;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoNormalForm::addToMerge2( qtcNode     * aPrevNF,
                            qtcNode     * aCurrNF,
                            qtcNode    ** aNFNode)
{
/***********************************************************************
 *
 * Description : ����ȭ ���·� ��ȯ�ϴ� �������� predicate�� ����.
 *
 *       BUG-35155 Partial CNF
 *       NNF filter ��� ��带 �����ϰ� ����ȭ ���·� ��ȯ�ϴ� ����� �߰��Ǿ���.
 *       NNF filter ��� ���(CNF_USUSABLE flag)�� ��ȯ ��󿡼� �����ϱ� ������
 *       normal form(aPrefNF �� aCurrNF)�� NULL �� �� �ִ�.
 *
 * Implementation :
 *     1. DNF
 *         OR�� ���� ��� ó�� ��, �� AND ��带 �����Ѵ�.
 *     2. CNF
 *         AND�� ���� ��� ó�� ��, �� OR ��带 �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode     * sNode;

    IDU_FIT_POINT_FATAL( "qmoNormalForm::addToMerge2::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNFNode  != NULL );

    //--------------------------------------
    // add to merge
    //--------------------------------------

    if ( aPrevNF == NULL )
    {
        *aNFNode = aCurrNF;
    }
    else
    {
        if ( aCurrNF == NULL )
        {
            // Nothing to do.
        }
        else
        {
            // merge
            sNode = (qtcNode *)(aPrevNF->node.arguments);
            while (sNode->node.next != NULL)
            {
                sNode = (qtcNode *)(sNode->node.next);
            }
            sNode->node.next = aCurrNF->node.arguments;
        }

        *aNFNode = aPrevNF;
    }

    return IDE_SUCCESS;
}
