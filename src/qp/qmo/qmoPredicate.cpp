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
 * $Id: qmoPredicate.cpp 90192 2021-03-12 02:01:03Z jayce.park $
 *
 * Description :
 *     Predicate Manager
 *
 *     �� �����ڸ� �����ϴ� Predicate�鿡 ���� �������� ������
 *     ����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qtc.h>
#include <qmoPredicate.h>
#include <qmoNormalForm.h>
#include <qmoSubquery.h>
#include <qmoSelectivity.h>
#include <qmoJoinMethod.h>
#include <qmoTransitivity.h>
#include <qmgProjection.h>
#include <qcsModule.h>

extern mtfModule mtfLike;
extern mtfModule mtfEqual;
extern mtfModule mtfEqualAny;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;
extern mtfModule mtfList;
extern mtfModule mtfNotEqual;
extern mtfModule mtfNotEqualAny;
extern mtfModule mtfEqualAll;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;
extern mtfModule mtfIsNull;
extern mtfModule mtfIsNotNull;
extern mtfModule mtfInlist;
extern mtfModule mtfNvlEqual;
extern mtfModule mtfNvlNotEqual;

static inline idBool isMultiColumnSubqueryNode( qtcNode * aNode )
{
    if( QTC_IS_SUBQUERY( aNode ) == ID_TRUE &&
        qtcNodeI::isOneColumnSubqueryNode( aNode ) == ID_FALSE )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

static inline idBool isOneColumnSubqueryNode( qtcNode * aNode )
{
    if( QTC_IS_SUBQUERY( aNode ) == ID_TRUE &&
        qtcNodeI::isOneColumnSubqueryNode( aNode ) == ID_TRUE )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

extern "C" SInt
compareFilter( const void * aElem1,
               const void * aElem2 )
{
/***********************************************************************
 *
 * Description : filter ordering�� ���� compare �Լ�
 *
 * Implementation :
 *
 *     ���ڷ� �Ѿ�� �� predicate->selectivity�� ���Ѵ�.
 *
 ***********************************************************************/

    //--------------------------------------
    // filter ordering�� ���� selectivity ��
    //--------------------------------------

    if( (*(qmoPredicate**)aElem1)->mySelectivity >
        (*(qmoPredicate**)aElem2)->mySelectivity )
    {
        return 1;
    }
    else if( (*(qmoPredicate**)aElem1)->mySelectivity <
             (*(qmoPredicate**)aElem2)->mySelectivity )
    {
        return -1;
    }
    else
    {
        if( (*(qmoPredicate**)aElem1)->idx >
            (*(qmoPredicate**)aElem2)->idx )
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }
}

void
qmoPred::setCompositeKeyUsableFlag( qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 *  Description : PROJ-1502 PARTITIONED DISK TABLE
 *                composite key�� ������ predicate�� flag�� �����Ѵ�.
 *
 *  Implementation :
 *     ���� ���� : composite index, range partition key(composite key)
 *     �������� key column �÷����ο� equal �񱳿����� ���� ���� ����.
 *     �������� key column ����� �� �ִ� predicate��,
 *       (1) equal(=)�� IN�������̸�,
 *       (2) �÷��� predicate�� �ϳ��� �ִ� ����̴�.
 *
 ***********************************************************************/

    idBool         sIsExistEqual;
    idBool         sIsExistOnlyEqual;
    qmoPredicate * sMorePredicate;
    qtcNode      * sCompareNode;

    //--------------------------------------
    // �������� key column ��밡�ɿ��ο�
    // �����÷�����Ʈ�� equal(in) �񱳿����� ���翩�� ����
    // 1. �������� key column ��밡�� ����
    //    keyRange ����� �ʿ��� ����.
    // 2. �����÷�����Ʈ�� equal(in) �񱳿����� ���翩��
    //    selection graph�� composite index�� ���� selectivity ������ �ʿ�.
    // To Fix PR-11731
    //    IS NULL ������ ���� �������� key column ����� �����ϴ�.
    // To Fix PR-11491
    //    =ALL ������ ���� �������� key column ����� �����ϴ�.
    //--------------------------------------

    sIsExistEqual     = ID_FALSE;
    sIsExistOnlyEqual = ID_TRUE;

    for( sMorePredicate = aPredicate;
         sMorePredicate != NULL;
         sMorePredicate = sMorePredicate->more )
    {
        if( ( sMorePredicate->node->node.lflag
              & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sMorePredicate->node->node.arguments);

            while( sCompareNode != NULL )
            {
                // To Fix PR-11731
                // IS NULL ������ ���� �������� key column ����� ������.
                // BUG-11491 =ALL�� �߰���.
                if( ( sCompareNode->node.module == &mtfEqual ) ||
                    ( sCompareNode->node.module == &mtfEqualAny ) ||
                    ( sCompareNode->node.module == &mtfEqualAll ) ||
                    ( sCompareNode->node.module == &mtfIsNull ) )
                {
                    sIsExistEqual = ID_TRUE;
                }
                else
                {
                    sIsExistOnlyEqual = ID_FALSE;
                }

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }
        }
        else
        {
            sCompareNode = sMorePredicate->node;

            // To Fix PR-11731
            // IS NULL ������ ���� �������� key column ����� ������.
            // BUG-11491 =ALL�� �߰���.
            if( ( sCompareNode->node.module == &mtfEqual ) ||
                ( sCompareNode->node.module == &mtfEqualAny ) ||
                ( sCompareNode->node.module == &mtfEqualAll ) ||
                ( sCompareNode->node.module == &mtfIsNull ) )
            {
                sIsExistEqual = ID_TRUE;
            }
            else
            {
                sIsExistOnlyEqual = ID_FALSE;
            }
        }
    }


    //--------------------------------------
    // ù��° qmoPredicate�� flag ���� ����
    //--------------------------------------

    if( sIsExistEqual == ID_TRUE )
    {
        aPredicate->flag &= ~QMO_PRED_EQUAL_IN_MASK;
        aPredicate->flag |= QMO_PRED_EQUAL_IN_EXIST;
    }
    else
    {
        aPredicate->flag &= ~QMO_PRED_EQUAL_IN_MASK;
        aPredicate->flag |= QMO_PRED_EQUAL_IN_ABSENT;
    }

    if( sIsExistOnlyEqual == ID_TRUE )
    {
        aPredicate->flag &= ~QMO_PRED_NEXT_KEY_USABLE_MASK;
        aPredicate->flag |= QMO_PRED_NEXT_KEY_USABLE;
    }
    else
    {
        aPredicate->flag &= ~QMO_PRED_NEXT_KEY_USABLE_MASK;
        aPredicate->flag |= QMO_PRED_NEXT_KEY_UNUSABLE;
    }
}


IDE_RC
qmoPred::relocatePredicate4PartTable(
    qcStatement       * aStatement,
    qmoPredicate      * aInPredicate,
    qcmPartitionMethod  aPartitionMethod,
    qcDepInfo         * aTableDependencies,
    qcDepInfo         * aOuterDependencies,
    qmoPredicate     ** aOutPredicate )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *               - partition graph�� predicate ���ġ
 *
 *     partition graph������ �ش� ���̺� ����
 *     OR �Ǵ� �񱳿����� ������ predicate���������� ������.
 *     predicate�����ڴ� �̸�
 *     (1) ���� �÷��� �����ϴ� predicate�� �з�
 *     (2) �ϳ��� �ε����κ��� �� �� �ִ� predicate�� �з���
 *     �����ϰ� �ϱ� ���� predicate�� ���ġ�Ѵ�.
 *
 *     [predicate�� ���ġ ����]
 *     1. �ε�������� ������ one column predicate���� �÷����� �и���ġ
 *     2. �ε�������� ������ multi column predicate(LIST)���� �и���ġ
 *     3. �ε�������� �Ұ����� predicate���� �и���ġ
 *
 * Implementation :
 *
 *     (1) �� predicate�鿡 ���ؼ� indexable predicate������ �˻��ؼ�,
 *         �÷����� �и���ġ
 *     (2) indexable column�� ���� IN(subquery)�� ���� ó���� ���� ����.
 *     (3) selectivity�� ���ϴ� �۾��� ���� ����.
 *
 *     ��������� �̿����� �ʴ´�. ������ partition keyrange��
 *     �����ϱ� ������.
 *     ���ġ���� �ϹǷ�, selectivity�� ������ �ִ� ������
 *     qmgSelection::optimizePartition���� �ϰ� �ȴ�.
 *
 ***********************************************************************/
    qmoPredicate * sInPredicate;
    qmoPredicate * sRelocatePred;
    qmoPredicate * sNextPredicate;
    qmoPredicate * sPredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::relocatePredicate4PartTable::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement         != NULL );
    IDE_DASSERT( aInPredicate       != NULL );
    IDE_DASSERT( aTableDependencies != NULL );
    IDE_DASSERT( aOutPredicate      != NULL );

    //--------------------------------------
    // predicate ���ġ
    //--------------------------------------

    sInPredicate = aInPredicate;

    // Base Table�� Predicate�� ���� �з� :
    // indexable predicate�� �Ǵ��ϰ�, columnID ����
    IDE_TEST( classifyPartTablePredicate( aStatement,
                                          sInPredicate,
                                          aPartitionMethod,
                                          aTableDependencies,
                                          aOuterDependencies )
              != IDE_SUCCESS );
    sRelocatePred = sInPredicate;
    sInPredicate = sInPredicate->next;
    // ����� sInPredicate�� next ������ ���´�.
    sRelocatePred->next = NULL;

    while ( sInPredicate != NULL )
    {
        IDE_TEST( classifyPartTablePredicate( aStatement,
                                              sInPredicate,
                                              aPartitionMethod,
                                              aTableDependencies,
                                              aOuterDependencies )
                  != IDE_SUCCESS );

        // �÷����� ������� �����
        // ���� �÷��� �ִ� ���, ���� �÷��� ������ predicate.more��
        // ���� �÷��� ���� ���, sRelocate�� ������ predicate.next��
        // (1) ���ο� predicate(sInPredicate)�� �����ϰ�,
        // (2) sInPredicate = sInPredicate->next
        // (3) ����� predicate�� next ������踦 ����.

        sNextPredicate = sInPredicate->next;

        IDE_TEST( linkPred4ColumnID( sRelocatePred,
                                     sInPredicate )
                  != IDE_SUCCESS );

        sInPredicate = sNextPredicate;
    }

    for ( sPredicate  = sRelocatePred;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        setCompositeKeyUsableFlag( sPredicate );
    }

    *aOutPredicate = sRelocatePred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::relocatePredicate( qcStatement      * aStatement,
                            qmoPredicate     * aInPredicate,
                            qcDepInfo        * aTableDependencies,
                            qcDepInfo        * aOuterDependencies,
                            qmoStatistics    * aStatiscalData,
                            qmoPredicate    ** aOutPredicate )
{
/***********************************************************************
 *
 * Description : - selection graph�� predicate ���ġ
 *               - selectivity �� ����
 *               - Host optimization ���� ����
 *
 *     selection graph������ �ش� ���̺� ����
 *     OR �Ǵ� �񱳿����� ������ predicate���������� ������.
 *     predicate�����ڴ� �̸�
 *     (1) ���� �÷��� �����ϴ� predicate�� �з�
 *     (2) �ϳ��� �ε����κ��� �� �� �ִ� predicate�� �з���
 *     �����ϰ� �ϱ� ���� predicate�� ���ġ�Ѵ�.
 *
 *     [predicate�� ���ġ ����]
 *     1. �ε�������� ������ one column predicate���� �÷����� �и���ġ
 *     2. �ε�������� ������ multi column predicate(LIST)���� �и���ġ
 *     3. �ε�������� �Ұ����� predicate���� �и���ġ
 *
 * Implementation :
 *
 *     (1) �� predicate�鿡 ���ؼ� indexable predicate������ �˻��ؼ�,
 *         �÷����� �и���ġ
 *     (2) indexable column�� ���� IN(subquery)�� ���� ó��
 *     (3) one column�� ���� ��ǥ selectivity�� ���ؼ�,
 *         qmoPredicate->more�� ���Ḯ��Ʈ ��,
 *         ù��° qmoPredicate.totalSelectivity�� �� ���� �����Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate * sInPredicate;
    qmoPredicate * sRelocatePred;
    qmoPredicate * sPredicate;
    qmoPredicate * sNextPredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::relocatePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement         != NULL );
    IDE_DASSERT( aInPredicate       != NULL );
    IDE_DASSERT( aTableDependencies != NULL );
    IDE_DASSERT( aStatiscalData     != NULL );
    IDE_DASSERT( aOutPredicate      != NULL );

    //--------------------------------------
    // predicate ���ġ
    //--------------------------------------

    sInPredicate = aInPredicate;

    // Base Table�� Predicate�� ���� �з� :
    // indexable predicate�� �Ǵ��ϰ�, columnID�� selectivity ����
    IDE_TEST( classifyTablePredicate( aStatement,
                                      sInPredicate,
                                      aTableDependencies,
                                      aOuterDependencies,
                                      aStatiscalData )
              != IDE_SUCCESS );
    sRelocatePred = sInPredicate;
    sInPredicate  = sInPredicate->next;
    // ����� sInPredicate�� next ������ ���´�.
    sRelocatePred->next = NULL;

    while ( sInPredicate != NULL )
    {
        IDE_TEST( classifyTablePredicate( aStatement,
                                          sInPredicate,
                                          aTableDependencies,
                                          aOuterDependencies,
                                          aStatiscalData )
                  != IDE_SUCCESS );

        // �÷����� ������� �����
        // ���� �÷��� �ִ� ���, ���� �÷��� ������ predicate.more��
        // ���� �÷��� ���� ���, sRelocate�� ������ predicate.next��
        // (1) ���ο� predicate(sInPredicate)�� �����ϰ�,
        // (2) sInPredicate = sInPredicate->next
        // (3) ����� predicate�� next ������踦 ����.

        sNextPredicate = sInPredicate->next;

        IDE_TEST( linkPred4ColumnID( sRelocatePred,
                                     sInPredicate )
                  != IDE_SUCCESS );

        sInPredicate = sNextPredicate;
    }

    //--------------------------------------
    // ���ġ �Ϸ���, indexable column�� ���� IN(subquery)�� ó��
    // IN(subquery)�� �ٸ� �÷��� ���� keyRange�� �������� ���ϰ�,
    // �ܵ����� �����ؾ��ϹǷ�, keyRange�������� ������ ���� ó���Ѵ�.
    // IN(subquery)�� �����ϴ� �÷��� ���,
    // selectivity�� ���� predicate�� ã�´�.
    // (1) ã�� predicate�� IN(subquery)�̸�,
    //     IN(subquery)�̿��� predicate�� non-indexable column ����Ʈ�� ����
    // (2) ã�� predicate�� IN(subquery)�� �ƴϸ�,
    //     IN(subquery) predicate���� non-indexable column ����Ʈ�� ����
    //--------------------------------------

    IDE_TEST( processIndexableInSubQ( & sRelocatePred )
              != IDE_SUCCESS );

    //--------------------------------------
    // ���ġ �Ϸ���,
    // ��ǥ selectivity��
    // one column�� ����
    //     (1) �����ε��� ��밡�ɿ���
    //     (2) equal(in)������ ���翩�ο� ���� ���� ����
    //--------------------------------------

    for ( sPredicate  = sRelocatePred;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        // PROJ-2242
        IDE_TEST( qmoSelectivity::setTotalSelectivity( aStatement,
                                                       aStatiscalData,
                                                       sPredicate )
                  != IDE_SUCCESS );

        setCompositeKeyUsableFlag( sPredicate );
    }

    *aOutPredicate = sRelocatePred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::classifyJoinPredicate( qcStatement  * aStatement,
                                qmoPredicate * aPredicate,
                                qmgGraph     * aLeftChildGraph,
                                qmgGraph     * aRightChildGraph,
                                qcDepInfo    * aFromDependencies )
{
/***********************************************************************
 *
 * Description : Join predicate�� ���� �з�
 *
 * Implementation :
 *
 *     join predicate���� �з��� predicate�� ����,
 *     � join method�� predicate���� ���� �� �ִ����� ������
 *     ��밡�� join method�� ���� ���� ���⼺�� ���� ������
 *     qmoPredicate.flag�� �����Ѵ�.
 *
 *     1. joinable prediate������ �˻�.
 *        OR ������尡 2�� �̻��� ����, index nested loop join��
 *        �����ϹǷ�, 2��(1)�� ���ؼ��� ó���Ѵ�.
 *        ��) t1.i1=t2.i1 OR t1.i1=t2.i2
 *     2. joinable predicate�̸�,
 *        index nested loop join, sort join, hash join, merge join��
 *        ��밡�ɿ��ο� join ���డ�ɹ��⿡ ���� ������ �����Ѵ�.
 *        (1) index nested loop join ��밡�� �Ǵ�
 *        (2) OR ��� ������ �ϳ��� ��츸 hash/sort/merge join ��밡�� �Ǵ�
 *            I)  sort join ��밡�� �Ǵ� : [=,>,>=,<,<=]��� �����ϰ� ó����
 *            II) hash join ��밡�� �Ǵ� : [=] hash ��밡��
 *            III) merge join ��밡�� �Ǵ�
 *                [ >, >=, <, <= ] : merge join ��� ����
 *                . =     : merge join ����� ��� ���డ��
 *                . >, >= : left->right �� ���డ��
 *                . <, <= : right->left �� ���డ��
 *
 ***********************************************************************/

    idBool         sIsOnlyIndexNestedLoop;
    qtcNode      * sNode;
    qmoPredicate * sCurPredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::classifyJoinPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement        != NULL );
    IDE_DASSERT( aPredicate        != NULL );
    IDE_DASSERT( aLeftChildGraph   != NULL );
    IDE_DASSERT( aRightChildGraph  != NULL );
    IDE_DASSERT( aFromDependencies != NULL );

    //--------------------------------------
    // join predicate �з�
    //--------------------------------------

    sCurPredicate = aPredicate;

    while ( sCurPredicate != NULL )
    {
        //--------------------------------------
        // joinable predicate������ �Ǵ��Ѵ�.
        //--------------------------------------

        IDE_TEST( isJoinablePredicate( sCurPredicate,
                                       aFromDependencies,
                                       & aLeftChildGraph->depInfo,
                                       & aRightChildGraph->depInfo,
                                       &sIsOnlyIndexNestedLoop )
                  != IDE_SUCCESS );

        if ( ( sCurPredicate->flag & QMO_PRED_JOINABLE_PRED_MASK )
             == QMO_PRED_JOINABLE_PRED_TRUE )
        {
            //--------------------------------------
            // joinable predicate�� ����
            // join method�� join ���డ�ɹ��� ����
            //--------------------------------------

            //--------------------------------------
            // indexable join predicate�� ���� ���� ����
            //-------------------------------------
            IDE_TEST( isIndexableJoinPredicate( aStatement,
                                                sCurPredicate,
                                                aLeftChildGraph,
                                                aRightChildGraph )
                      != IDE_SUCCESS );

            if ( sIsOnlyIndexNestedLoop == ID_TRUE )
            {
                // OR ��� ������ �񱳿����ڰ� ������ �ִ� ����,
                // index nested loop join method�� ��밡���ϹǷ� ���� skip

                // Nothing To Do
            }
            else
            {
                //--------------------------------------
                // �񱳿����� ������ sort, hash, merge join�� ���� ���� ����
                //-------------------------------------

                // �񱳿����� ��带 ã�´�.
                sNode = sCurPredicate->node;

                if ( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                     == MTC_NODE_LOGICAL_CONDITION_TRUE )
                {
                    sNode = (qtcNode *)(sNode->node.arguments);
                }
                else
                {
                    // Nothing To Do
                }

                //-------------------------------------
                // sort joinable predicate�� ���� ���� ����
                // sort joinable predicate��
                // [ =, >, >=, <, <= ]�� �����ϰ� �����.
                //-------------------------------------

                IDE_TEST( isSortJoinablePredicate( sCurPredicate,
                                                   sNode,
                                                   aFromDependencies,
                                                   aLeftChildGraph,
                                                   aRightChildGraph )
                          != IDE_SUCCESS );

                //-------------------------------------
                // hash joinable predicate�� ���� ���� ����
                // [ = ] : hash ����
                //-------------------------------------

                IDE_TEST( isHashJoinablePredicate( sCurPredicate,
                                                   sNode,
                                                   aFromDependencies,
                                                   aLeftChildGraph,
                                                   aRightChildGraph )
                          != IDE_SUCCESS );

                //-------------------------------------
                // merge joinable predicate�� ���� ���� ����
                // 1. �ε��� ���� ���� ���� �˻�
                //    sort�� ����. �̹� sort���� �˻������Ƿ�,
                //    QMO_PRED_SORT_JOINABLE_TRUE �̸�, �ε��� ��������
                // 2. �ε��� ���������ϸ�, �Ʒ� ���� ����.
                //    [ =, >, >=, <, <= ] : merge ����
                //    [ = ]    : ����� ���డ��
                //    [ >,>= ] : left->right ���డ��
                //    [ <,<= ] : right->left ���డ��
                //-------------------------------------

                IDE_TEST( isMergeJoinablePredicate( sCurPredicate,
                                                    sNode,
                                                    aFromDependencies,
                                                    aLeftChildGraph,
                                                    aRightChildGraph )
                          != IDE_SUCCESS );
            }

            //---------------------------------------
            // ��밡���� join method�� ���� ���,
            // non-joinable predicate���� �����Ѵ�.
            // ��: t1.i1+1 = t2.i1+1 OR t1.i2+1 = t2.i2+1
            //     OR ��� ������ �������� �񱳿����ڰ� �־�
            //     joinable predicate���� �ǴܵǾ�����,
            //     ����, index nested loop join method�� ������� ���� ���...
            //----------------------------------------
            if ( ( sCurPredicate->flag & QMO_PRED_JOINABLE_MASK )
                 == QMO_PRED_JOINABLE_FALSE )
            {
                sCurPredicate->flag &= ~QMO_PRED_JOINABLE_PRED_MASK;
                sCurPredicate->flag |= QMO_PRED_JOINABLE_PRED_FALSE;
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

        sCurPredicate = sCurPredicate->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::makeJoinPushDownPredicate( qcStatement    * aStatement,
                                    qmoPredicate   * aNewPredicate,
                                    qcDepInfo      * aRightDependencies )
{
/***********************************************************************
 *
 * Description : Index Nested Loop Join�� push down joinable predicate
 *
 *   (1) Inner Join (2) Left Outer Join (3) Anti Outer Join ó����
 *   join graph���� selection graph�� join index Ȱ���� ���� join-push predicate��
 *   �����.
 *
 *   *aNewPredicate : add�� joinable predicate�� ���Ḯ��Ʈ
 *                    joinable predicate�� predicate ���ġ�� �����ϰ�
 *                    �÷����� �и��Ǿ� �ִ�.
 *
 *
 * Implementation :
 *
 *     1. joinable predicate�� indexArgument�� columID ����
 *
 ***********************************************************************/

    UInt           sColumnID;
    qmoPredicate * sJoinPredicate;
    qmoPredicate * sPredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::makeJoinPushDownPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNewPredicate != NULL );
    IDE_DASSERT( aRightDependencies != NULL );

    //--------------------------------------
    // joinable predicate�� indexArgument�� columnID ����.
    //--------------------------------------

    for ( sJoinPredicate  = aNewPredicate;
          sJoinPredicate != NULL;
          sJoinPredicate  = sJoinPredicate->next )
    {
        for ( sPredicate  = sJoinPredicate;
              sPredicate != NULL;
              sPredicate  = sPredicate->more )
        {
            // indexArgument ����
            IDE_TEST( setIndexArgument( sPredicate->node,
                                        aRightDependencies )
                      != IDE_SUCCESS );
            // columnID ����
            IDE_TEST( getColumnID( aStatement,
                                   sPredicate->node,
                                   ID_TRUE,
                                   & sColumnID )
                      != IDE_SUCCESS );

            sPredicate->id = sColumnID;

            // To fix BUG-17575
            sPredicate->flag &= ~QMO_PRED_JOIN_PRED_MASK;
            sPredicate->flag |= QMO_PRED_JOIN_PRED_TRUE;

            sPredicate->flag &= ~QMO_PRED_INDEX_NESTED_JOINABLE_MASK;
            sPredicate->flag |= QMO_PRED_INDEX_NESTED_JOINABLE_TRUE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::makeNonJoinPushDownPredicate( qcStatement   * aStatement,
                                       qmoPredicate  * aNewPredicate,
                                       qcDepInfo     * aRightDependencies,
                                       UInt            aDirection )
{
/***********************************************************************
 *
 * Description : push down non-joinable predicate
 *
 *     Index Nested Loop Join�� ���,
 *     join graph���� selection graph�� non-joinable predicate�� ������ �Ǹ�,
 *     Full Nested Loop Join�� ���,
 *     right graph�� selection graph�̸�, join predicate�� ������ �ȴ�.
 *
 *     *aNewPredicate : add�� join predicate�� ���Ḯ��Ʈ
 *                      non-joinable predicate�� �÷����� �и��Ǿ� ���� �ʴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt           sJoinDirection;
    UInt           sColumnID;
    idBool         sIsIndexable;
    qmoPredicate * sJoinPredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::makeNonJoinPushDownPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNewPredicate != NULL );
    IDE_DASSERT( aRightDependencies != NULL );

    //--------------------------------------
    // non-joinable predicate�� ���� indexArgument�� columnID ����
    //  . indexable     : indexArgument �� columnID(smiColumn.id) ����
    //  . non-indexable : columnID(QMO_COLUMNID_NON_INDEXABLE) ����
    //--------------------------------------

    // non-joinable predicate�� indexable���θ� �Ǵ��ϱ� ����,
    // direction������ ���ڷ� �޴´�.
    sJoinDirection = QMO_PRED_CLEAR;

    if ( ( aDirection & QMO_JOIN_METHOD_DIRECTION_MASK )
         == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
    {
        sJoinDirection = QMO_PRED_INDEX_LEFT_RIGHT;
    }
    else
    {
        sJoinDirection = QMO_PRED_INDEX_RIGHT_LEFT;
    }

    // non-joinable predicate�� �÷����� �и��Ǿ� ���� �ʴ�.

    for ( sJoinPredicate  = aNewPredicate;
          sJoinPredicate != NULL;
          sJoinPredicate  = sJoinPredicate->next )
    {
        // BUG-11519 fix
        // index joinable�� true�̰�
        // join ��������� sJoinDirection�� ���ų� ������̸�
        // indexable�� true�� �����Ѵ�.
        if ( ( sJoinPredicate->flag & QMO_PRED_INDEX_JOINABLE_MASK )
             == QMO_PRED_INDEX_JOINABLE_TRUE &&
             ( ( sJoinPredicate->flag & QMO_PRED_INDEX_DIRECTION_MASK )
               == sJoinDirection ||
               ( sJoinPredicate->flag & QMO_PRED_INDEX_DIRECTION_MASK )
               == QMO_PRED_INDEX_BIDIRECTION ) )
        {
            // indexable predicate�� ���� indexArgument ����
            IDE_TEST( setIndexArgument( sJoinPredicate->node,
                                        aRightDependencies )
                      != IDE_SUCCESS );

            sIsIndexable = ID_TRUE;
        }
        else
        {
            sIsIndexable = ID_FALSE;
        }

        // columnID ����
        IDE_TEST( getColumnID( aStatement,
                               sJoinPredicate->node,
                               sIsIndexable,
                               & sColumnID )
                  != IDE_SUCCESS );

        sJoinPredicate->id = sColumnID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isRownumPredicate( qmoPredicate  * aPredicate,
                            idBool        * aIsTrue )
{
/***********************************************************************
 *
 * Description
 *     PROJ-1405
 *     rownum predicate�� �Ǵ�
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPred::isRownumPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aIsTrue    != NULL );

    //--------------------------------------
    // rownum predicate�� �Ǵ�
    //--------------------------------------

    if ( ( aPredicate->node->lflag & QTC_NODE_ROWNUM_MASK )
         == QTC_NODE_ROWNUM_EXIST )
    {
        *aIsTrue = ID_TRUE;
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::isConstantPredicate( qmoPredicate  * aPredicate       ,
                              qcDepInfo     * aFromDependencies,
                              idBool        * aIsTrue            )
{
/***********************************************************************
 *
 * Description : constant predicate�� �Ǵ�
 *
 *     constant predicate�� FROM���� table�� ������� predicate�̴�.
 *     ��)  1 = 1
 *
 *   <LEVEL, PRIOR column ó����>
 *   LEVEL = 1, PRIOR I1 = 1�� ���� ���� ó���Ǵ� ���� ����,
 *   constant predicate�� �ƴԿ��� �ұ��ϰ�, constant predicate����
 *   �з��� �� �ִ�. ����, graph������ ������ ���� ó���ؾ� �Ѵ�.
 *
 *   1. LEVEL pseudo column
 *      (1) hierarchy query�� ���
 *          . where�� ó���� :
 *            LEVEL = 1 �� ���� ����, filter�� ó���Ǿ�� �Ѵ�.
 *            (constant filter�� ó���Ǹ� �ȵ�.)
 *            ��, constant filter�̸�, LEVEL column�� �����ϴ����� �˻��ؼ�,
 *            LEVEL column�� �����ϸ�, filter�� ó���ǵ��� �ؾ� ��.
 *          .connect by�� ó���� :
 *            LEVEL = 1 �� ���� ����, level filter�� ó���Ǿ�� �Ѵ�.
 *            ��, constant filter�̸�, LEVEL column�� �����ϴ����� �˻��ؼ�,
 *            LEVEL column�� �����ϸ�, level filter�� ó���ǵ��� �ؾ� ��.
 *
 *      (2) hierarchy query�� �ƴ� ���,
 *         .where�� ó����, constant predicate���� ó��.
 *
 *   2. PRIOR column
 *      (1) hierarchy query�� ���
 *          constant filter�̸�, PRIOR column�� �����ϴ����� �˻��ؼ�,
 *          PRIOR column�� �����ϸ�, filter�� ó���ǵ��� �ؾ� ��.
 *          (constant filter�� �ƴ�.)
 *      (2) hierarchy query�� �ƴ� ���, PRIOR column�� ����� �� ����.
 *
 * Implementation :
 *
 *    �ֻ��� ��忡�� �Ʒ��� ������ �˻�.
 *    ( predicate�� dependencies & FROM�� dependencies ) == 0
 *
 ***********************************************************************/

    qcDepInfo  sAndDependencies;

    IDU_FIT_POINT_FATAL( "qmoPred::isConstantPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate        != NULL );
    IDE_DASSERT( aFromDependencies != NULL );
    IDE_DASSERT( aIsTrue           != NULL );

    //--------------------------------------
    // constant predicate�� �Ǵ�
    // dependencies�� �ֻ��� ��忡�� �Ǵ��Ѵ�.
    //--------------------------------------

    // predicate�� dependencies & FROM�� dependencies
    qtc::dependencyAnd ( & aPredicate->node->depInfo,
                         aFromDependencies,
                         & sAndDependencies );

    // (predicate�� dependencies & FROM�� dependencies)�� ����� 0���� �˻�
    if ( qtc::dependencyEqual( & sAndDependencies,
                               & qtc::zeroDependencies ) == ID_TRUE )
    {
        *aIsTrue = ID_TRUE;
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::isOneTablePredicate( qmoPredicate  * aPredicate         ,
                              qcDepInfo     * aFromDependencies  ,
                              qcDepInfo     * aTableDependencies ,
                              idBool        * aIsTrue             )
{
/***********************************************************************
 *
 * Description : one table predicate�� �Ǵ�
 *
 *     one table predicate: FROM���� table �� �ϳ����� ���ϴ� predicate
 *     ��) T1.i1 = 1
 *
 * Implementation :
 *
 *    �ֻ��� ��忡�� �Ʒ��� ������ �˻�.
 *    (  ( predicate�� dependencies & FROM�� dependencies )
 *       & ~(FROM�� �ش� table�� dependencies)    ) == 0
 *
 ***********************************************************************/

    qcDepInfo  sAndDependencies;

    IDU_FIT_POINT_FATAL( "qmoPred::isOneTablePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate          != NULL );
    IDE_DASSERT( aFromDependencies   != NULL );
    IDE_DASSERT( aTableDependencies  != NULL );
    IDE_DASSERT( aIsTrue             != NULL );

    //--------------------------------------
    // one table predicate �Ǵ�
    // dependencies�� �ֻ��� ��忡�� �Ǵ��Ѵ�.
    //--------------------------------------

    // predicate�� dependencies & FROM�� dependencies
    qtc::dependencyAnd( & aPredicate->node->depInfo,
                        aFromDependencies,
                        & sAndDependencies );

    if ( qtc::dependencyContains( aTableDependencies,
                                  & sAndDependencies ) == ID_TRUE )
    {
        // (1) select * from t1
        //     where i1 in ( select * from t2
        //                   where t1.i1=t2.i1 );
        // (2) select * from t1
        //     where exists( select * from t2
        //                   where t1.i1=t2.i1 );
        // (1),(2)�� ���ǹ��� ���,
        // subquery��  t1.i1=t2.i1�� variable predicate���� �з��Ǿ�� �Ѵ�.

        *aIsTrue = ID_TRUE;

        if ( qtc::dependencyContains( aFromDependencies,
                                      & aPredicate->node->depInfo ) == ID_TRUE )
        {
            // BUG-28470
            // Inner Join�� ���̺���� �������� �ϳ��� ���̺�� ����.
            // �� ������ where������ Inner Join�� ���̺���� ����� Predicate��
            // One Table Predicate���� �з������� Selectivity ��� �ÿ�
            // ���ڵ带 ������ �� �����Ƿ� Variable Predicate���� �з��Ǿ�� �Ѵ�.
            // ��) select * from t1 inner join t2 on t1.i1=t2.i1
            //     where  t1.i2 > substr(t2.i2)
            if ( ( aTableDependencies->depCount > 1 )
                 && ( aPredicate->node->depInfo.depCount > 1 ) )
            {
                // variable predicate���� �з��Ǿ�� ��.
                aPredicate->flag &= ~QMO_PRED_VALUE_MASK;
                aPredicate->flag |= QMO_PRED_VARIABLE;
            }
            else
            {
                // Nothing To Do
                // fixed predicate
            }
        }
        else
        {
            // variable predicate���� �з��Ǿ�� ��.
            aPredicate->flag &= ~QMO_PRED_VALUE_MASK;
            aPredicate->flag |= QMO_PRED_VARIABLE;
        }
    }
    else
    {
        *aIsTrue = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::optimizeSubqueries( qcStatement  * aStatement,
                             qmoPredicate * aPredicate,
                             idBool         aTryKeyRange )
{
/***********************************************************************
 *
 * Description : qmoPredicate->node�� �����ϴ� ��� subquery�� ���� ó��
 *
 *  selection graph���� myPredicate�� �޷��ִ� predicate�鿡 ���ؼ�,
 *  subquery�� ���� ó���� ����, �� �Լ��� ȣ���ϰ� �ȴ�.
 *
 * Implementation :
 *
 *     qmoPredicate->node�� �����ϴ� ��� subquery�� ã�Ƽ�,
 *     �̿� ���� ����ȭ �� ���� �� graph�� �����Ѵ�.
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    idBool         sIsConstantPred = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoPred::optimizeSubqueries::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL ); // selection graph�� myPredicate

    //--------------------------------------
    // predicate�� �����ϴ� ��� subquery�� ó��
    //--------------------------------------

    if ( ( aPredicate->flag & QMO_PRED_CONSTANT_FILTER_MASK )
         == QMO_PRED_CONSTANT_FILTER_TRUE )
    {
        sIsConstantPred = ID_TRUE;
    }
    else
    {
        sIsConstantPred = ID_FALSE;
    }

    for ( sPredicate  = aPredicate;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        IDE_TEST( optimizeSubqueryInNode( aStatement,
                                          sPredicate->node,
                                          aTryKeyRange,
                                          sIsConstantPred )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::optimizeSubqueryInNode( qcStatement  * aStatement,
                                 qtcNode      * aNode,
                                 idBool         aTryKeyRange,
                                 idBool         aConstantPred )
{
/***********************************************************************
 *
 * Description : qtcNode�� �����ϴ� ��� subquery�� ���� ó��
 *
 * Implementation :
 *
 *     qtcNode�� �����ϴ� ��� subquery�� ã�Ƽ�,
 *     �̿� ���� ����ȭ �� ���� �� graph�� �����Ѵ�.
 *
 *     �� �Լ��� qmoPred::optimizeSubqueries()
 *               qmgGrouping::optimize() ���� ȣ��ȴ�.
 *
 *     qmgGrouping������ aggr, group, having�� ���� subquery ó���� ����
 *     �� �Լ��� ȣ���Ѵ�.
 *     (1) aggr, group�� �������� �ƴ�. (2) having�� ��������.
 *
 ***********************************************************************/

    qtcNode      * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::optimizeSubqueryInNode::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode != NULL );

    //--------------------------------------
    // qtcNode�� �����ϴ� ��� subquery�� ó��
    //--------------------------------------

    if ( ( aNode->lflag & QTC_NODE_SUBQUERY_MASK )
         == QTC_NODE_SUBQUERY_EXIST )
    {
        if ( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sNode = (qtcNode *)(aNode->node.arguments);

            while ( sNode != NULL )
            {
                IDE_TEST( optimizeSubqueryInNode( aStatement,
                                                  sNode,
                                                  aTryKeyRange,
                                                  aConstantPred )
                          != IDE_SUCCESS );

                sNode = (qtcNode *)(sNode->node.next);
            }
        }
        else
        {
            // grouping graph���� aggr, group�� ���� ó��

            // To Fix BUG-9522
            sNode = aNode;

            // fix BUG-12934
            // constant filter�� ���, store and search�� ���� �ʱ� ����
            // �ӽ����� ����
            if ( aConstantPred == ID_TRUE )
            {
                sNode->lflag &= ~QTC_NODE_CONSTANT_FILTER_MASK;
                sNode->lflag |= QTC_NODE_CONSTANT_FILTER_TRUE;
            }
            else
            {
                // Nothing To Do
            }

            IDE_TEST( qmoSubquery::optimize( aStatement,
                                             sNode,
                                             aTryKeyRange )
                      != IDE_SUCCESS );

            // �ӽ÷� ����� ���� ����
            sNode->lflag &= ~QTC_NODE_CONSTANT_FILTER_MASK;
            sNode->lflag |= QTC_NODE_CONSTANT_FILTER_FALSE;
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
qmoPred::separateJoinPred( qmoPredicate  * aPredicate,
                           qmoPredInfo   * aJoinablePredInfo,
                           qmoPredicate ** aJoinPred,
                           qmoPredicate ** aNonJoinPred )
{
/***********************************************************************
 *
 * Description : joinable predicate�� non-joinable predicate�� �и��Ѵ�.
 *
 * 1. ���ڷ� �Ѿ�� join predicate list�� joinable predicate Info��
 *    �������� ������ ����.
 *                                 aPredicate [p1]-[p2]-[p3]-[p4]-[p5]
 *                      ________________________|    |              |
 *                      |                            |              |
 *                      |                            |              |
 *  aJoinablePredInfo [Info1]-->[Info2]______________|              |
 *                                |                                 |
 *                                |                                 |
 *                               \ /                                |
 *                             [Info2]______________________________|
 *
 * 2. �и���ġ�� ���
 *    (1) joinable Predicate    (2) non-joinable predicate
 *        [p1]-[p2]                 [p3]->[p4]
 *              |
 *             [p5]
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredInfo  * sPredInfo;
    qmoPredInfo  * sMorePredInfo;
    qmoPredicate * sJoinPredList = NULL;
    qmoPredicate * sLastPredList;
    qmoPredicate * sPredicateList;
    qmoPredicate * sPredicate;
    qmoPredicate * sPrevPredicate;
    qmoPredicate * sNextPredicate;
    qmoPredicate * sTempList        = NULL;
    qmoPredicate * sMoreTempList    = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::separateJoinPred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    if ( aJoinablePredInfo != NULL )
    {
        IDE_DASSERT( aPredicate != NULL );
    }
    else
    {
        // Nothing To Do
    }
    IDE_DASSERT( aJoinPred != NULL );
    IDE_DASSERT( aNonJoinPred != NULL );

    //--------------------------------------
    // joinable predicate �и�
    //--------------------------------------

    sPredicateList = aPredicate;  // aPredicate�� join predicate list

    for ( sPredInfo  = aJoinablePredInfo;
          sPredInfo != NULL;
          sPredInfo  = sPredInfo->next )
    {
        sTempList = NULL;

        for ( sMorePredInfo  = sPredInfo;
              sMorePredInfo != NULL;
              sMorePredInfo  = sMorePredInfo->more )
        {
            for ( sPrevPredicate  = NULL,
                      sPredicate  = sPredicateList;
                  sPredicate     != NULL;
                  sPrevPredicate  = sPredicate,
                      sPredicate  = sNextPredicate )
            {
                sNextPredicate = sPredicate->next;

                if ( sMorePredInfo->predicate == sPredicate )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            IDE_TEST_RAISE( sPredicate == NULL, ERR_INVALID_PREDICATE_LIST );

            if ( sTempList == NULL )
            {
                sTempList     = sPredicate;
                sMoreTempList = sTempList;
            }
            else
            {
                sMoreTempList->more = sPredicate;
                sMoreTempList       = sMoreTempList->more;
            }

            // join predicate list������ ������ ���´�.
            if ( sPrevPredicate == NULL )
            {
                sPredicateList = sPredicate->next;
            }
            else
            {
                sPrevPredicate->next = sPredicate->next;
            }

            sPredicate->next = NULL;
        }

        if ( sJoinPredList == NULL )
        {
            sJoinPredList = sTempList;
            sLastPredList = sJoinPredList;
        }
        else
        {
            sLastPredList->next = sTempList;
            sLastPredList       = sLastPredList->next;
        }
    }

    *aJoinPred = sJoinPredList;

    //--------------------------------------
    // non-joinable predicate �� ����
    // sPredicateList�� NULL�ϼ��� �ִ�.
    // �� ���,����������� sPredicate�� NULL�� ���õ�.
    //--------------------------------------

    *aNonJoinPred = sPredicateList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_PREDICATE_LIST );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPred::separateJoinPred",
                                  "invalid predicate list" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

qtcNode*
qmoPred::getColumnNodeOfJoinPred( qcStatement  * aStatement,
                                  qmoPredicate * aPredicate,
                                  qcDepInfo    * aDependencies )
{
/***********************************************************************
 *
 * Description : BUG-24673
 *               Join predicate�� ���� columnNode ��
 *               ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *     join predicate�� �ʿ��� preserved order�˻�� ����Ѵ�.
 *
 ***********************************************************************/

    idBool    sIsFirstNode = ID_TRUE;
    idBool    sIsIndexable = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sCurNode;
    qtcNode * sColumnNode = NULL;
    qtcNode * sFirstColumnNode = NULL;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aDependencies != NULL );

    //--------------------------------------
    // join predicate�� columnID ����
    //--------------------------------------

    // join predicate ������ �������� �񱳿����� ��尡 �� ���� �����Ƿ�,
    // �̿� ���� ����� �ʿ���. ( �� ���� index nested loop join�� ���� )

    if( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        // CNF �� ���

        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);

        while( sCompareNode != NULL )
        {
            sCurNode = (qtcNode*)(sCompareNode->node.arguments);

            // To Fix PR-8025
            // Dependency�� �����ϴ� Argument�� ����.
            // ���ڷ� �Ѿ�� ���� selection graph��
            // dependencies�� �������� �˻�.
            if( qtc::dependencyEqual( aDependencies,
                                      & sCurNode->depInfo ) == ID_TRUE )
            {
                sCompareNode->indexArgument = 0;
            }
            else
            {
                if( ( sCompareNode->node.module == &mtfEqual )
                    || ( sCompareNode->node.module == &mtfNotEqual )
                    || ( sCompareNode->node.module == &mtfGreaterThan )
                    || ( sCompareNode->node.module == &mtfGreaterEqual )
                    || ( sCompareNode->node.module == &mtfLessThan )
                    || ( sCompareNode->node.module == &mtfLessEqual ) )
                {
                    sCompareNode->indexArgument = 1;
                    sCurNode = (qtcNode *) sCurNode->node.next;
                }
                else
                {
                    // =, !=, >, >=, <, <= �� ������ ��� �񱳿����ڴ�
                    // �񱳿������� argument�� column node�� �����Ѵ�.
                    // ����, �� �����ڵ��� ���� ���,
                    // �ش� ���̺��� �÷��� �ƴ� ���,

                    sCurNode = NULL;
                }
            }

            if( sCurNode != NULL )
            {
                if ( QTC_IS_COLUMN( aStatement, sCurNode ) == ID_TRUE )
                {
                    // �񱳿������� argument�� �÷��� ���,
                    // columnNode�� ���Ѵ�.
                    sColumnNode = sCurNode;    
                }
                else
                {
                    // Index�� ����� �� ���� �����.
                    sIsIndexable = ID_FALSE;
                }
            }
            else
            {
                sIsIndexable = ID_FALSE;
            }

            if( sIsIndexable == ID_TRUE )
            {
                if( sIsFirstNode == ID_TRUE )
                {
                    // OR ��� ������ ù��° �񱳿����� ó����,
                    // ���� column �񱳸� ����,
                    // sFirstColumn�� column�� ����.
                    sFirstColumnNode = sColumnNode;
                    sIsFirstNode = ID_FALSE;
                }
                else
                {
                    // Nothing To Do
                }
                
                // �ϴ� first, column node�Ѵ� null�̸� �ȵȴ�.
                // null�̶�� ���� indexable���� ���� ��.
                if( ( sFirstColumnNode != NULL ) &&
                    ( sColumnNode != NULL ) )
                {
                    // ���� �ٸ� �÷��̶�� indexable���� ����
                    if( ( sFirstColumnNode->node.table ==
                          sColumnNode->node.table ) &&
                        ( sFirstColumnNode->node.column ==
                          sColumnNode->node.column ) )
                    {
                        // Nothing to do.
                    }
                    else
                    {
                        sIsIndexable = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsIndexable = ID_FALSE;
                    break;
                }
            }
            else
            {
                break;
            }

            sCompareNode = (qtcNode *)(sCompareNode->node.next);
        }
    }
    else
    {
        // DNF �� ���

        sCompareNode = aPredicate->node;
        sCurNode = (qtcNode*)(sCompareNode->node.arguments);

        // To Fix PR-8025
        // Dependency�� �����ϴ� Argument�� ����.
        // ���ڷ� �Ѿ�� ���� selection graph��
        // dependencies�� �������� �˻�.
        if( qtc::dependencyEqual( aDependencies,
                                  & sCurNode->depInfo ) == ID_TRUE )
        {
            sCompareNode->indexArgument = 0;
        }
        else
        {
            if( ( sCompareNode->node.module == &mtfEqual )
                || ( sCompareNode->node.module == &mtfNotEqual )
                || ( sCompareNode->node.module == &mtfGreaterThan )
                || ( sCompareNode->node.module == &mtfGreaterEqual )
                || ( sCompareNode->node.module == &mtfLessThan )
                || ( sCompareNode->node.module == &mtfLessEqual ) )
            {
                sCompareNode->indexArgument = 1;
                sCurNode = (qtcNode *) sCurNode->node.next;
            }
            else
            {
                // =, !=, >, >=, <, <= �� ������ ��� �񱳿����ڴ�
                // �񱳿������� argument�� column node�� �����Ѵ�.
                // ����, �� �����ڵ��� ���� ���,
                // �ش� ���̺��� �÷��� �ƴ� ���,

                sCurNode = NULL;
            }
        }

        if( sCurNode != NULL )
        {
            if ( QTC_IS_COLUMN( aStatement, sCurNode ) == ID_TRUE )
            {
                // �񱳿������� argument�� �÷��� ���,
                // columnID�� ���Ѵ�.
                sColumnNode = sCurNode;
            }
            else
            {
                // Index�� ����� �� ���� �����.
                sIsIndexable = ID_FALSE;
            }
        }
        else
        {
            sIsIndexable = ID_FALSE;
        }
    }

    if( sIsIndexable == ID_TRUE )
    {
        // Nothing to do.
    }
    else
    {
        sColumnNode = NULL;
    }

    return sColumnNode;
}


IDE_RC
qmoPred::setColumnIDToJoinPred( qcStatement  * aStatement,
                               qmoPredicate * aPredicate,
                               qcDepInfo    * aDependencies )
{
/***********************************************************************
 *
 * Description : Join predicate�� ���� columnID ������
 *               qmoPredicate.id�� �����Ѵ�.
 *
 * Implementation :
 *
 *     join order ������������ join graph�� ������ selection graph�� ���,
 *     composite index�� ���� selectivity ������ �ϰ� �Ǵµ�,
 *     join predicate�� composite index�� �÷����Կ��θ� �˻��ϱ� ����
 *     graph���� ȣ���ϰ� �ȴ�.
 *
 *     columnID ������ qmoPredicate.flag�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool    sIsFirstNode = ID_TRUE;
    idBool    sIsIndexable = ID_TRUE;
    UInt      sFirstColumnID = QMO_COLUMNID_NON_INDEXABLE;
    UInt      sColumnID      = QMO_COLUMNID_NON_INDEXABLE;
    qtcNode * sCompareNode;
    qtcNode * sCurNode;

    IDU_FIT_POINT_FATAL( "qmoPred::setColumnIDToJoinPred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aDependencies != NULL );

    //--------------------------------------
    // join predicate�� columnID ����
    //--------------------------------------

    // join predicate ������ �������� �񱳿����� ��尡 �� ���� �����Ƿ�,
    // �̿� ���� ����� �ʿ���. ( �� ���� index nested loop join�� ���� )

    if ( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        // CNF �� ���

        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);

        while ( sCompareNode != NULL )
        {
            sCurNode = (qtcNode*)(sCompareNode->node.arguments);

            // To Fix PR-8025
            // Dependency�� �����ϴ� Argument�� ����.
            // ���ڷ� �Ѿ�� ���� selection graph��
            // dependencies�� �������� �˻�.
            if ( qtc::dependencyEqual( aDependencies,
                                       & sCurNode->depInfo ) == ID_TRUE )
            {
                sCompareNode->indexArgument = 0;
            }
            else
            {
                if ( ( sCompareNode->node.module == &mtfEqual )
                     || ( sCompareNode->node.module == &mtfNotEqual )
                     || ( sCompareNode->node.module == &mtfGreaterThan )
                     || ( sCompareNode->node.module == &mtfGreaterEqual )
                     || ( sCompareNode->node.module == &mtfLessThan )
                     || ( sCompareNode->node.module == &mtfLessEqual ) )
                {
                    sCompareNode->indexArgument = 1;
                    sCurNode = (qtcNode *) sCurNode->node.next;
                }
                else
                {
                    // =, !=, >, >=, <, <= �� ������ ��� �񱳿����ڴ�
                    // �񱳿������� argument�� column node�� �����Ѵ�.
                    // ����, �� �����ڵ��� ���� ���,
                    // �ش� ���̺��� �÷��� �ƴ� ���,

                    sCurNode = NULL;
                }
            }

            if ( sCurNode != NULL )
            {
                if ( QTC_IS_COLUMN( aStatement, sCurNode ) == ID_TRUE )
                {
                    // �񱳿������� argument�� �÷��� ���,
                    // columnID�� ���Ѵ�.
                    sColumnID =
                        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sCurNode->node.table].
                        columns[sCurNode->node.column].column.id;
                }
                else
                {
                    // Index�� ����� �� ���� �����.
                    sIsIndexable = ID_FALSE;
                }
            }
            else
            {
                sIsIndexable = ID_FALSE;
            }

            if ( sIsIndexable == ID_TRUE )
            {
                if ( sIsFirstNode == ID_TRUE )
                {
                    // OR ��� ������ ù��° �񱳿����� ó����,
                    // ���� columnID �񱳸� ����,
                    // sFirstColumnID�� columnID�� ����.
                    sFirstColumnID = sColumnID;
                    sIsFirstNode   = ID_FALSE;
                }
                else
                {
                    // Nothing To Do
                }

                if ( sFirstColumnID == sColumnID )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsIndexable = ID_FALSE;
                    break;
                }
            }
            else
            {
                break;
            }

            sCompareNode = (qtcNode *)(sCompareNode->node.next);
        }
    }
    else
    {
        // DNF �� ���

        sCompareNode = aPredicate->node;
        sCurNode     = (qtcNode*)(sCompareNode->node.arguments);

        // To Fix PR-8025
        // Dependency�� �����ϴ� Argument�� ����.
        // ���ڷ� �Ѿ�� ���� selection graph��
        // dependencies�� �������� �˻�.
        if ( qtc::dependencyEqual( aDependencies,
                                   & sCurNode->depInfo ) == ID_TRUE )
        {
            sCompareNode->indexArgument = 0;
        }
        else
        {
            if ( ( sCompareNode->node.module == &mtfEqual )
                 || ( sCompareNode->node.module == &mtfNotEqual )
                 || ( sCompareNode->node.module == &mtfGreaterThan )
                 || ( sCompareNode->node.module == &mtfGreaterEqual )
                 || ( sCompareNode->node.module == &mtfLessThan )
                 || ( sCompareNode->node.module == &mtfLessEqual ) )
            {
                sCompareNode->indexArgument = 1;
                sCurNode                    = (qtcNode *) sCurNode->node.next;
            }
            else
            {
                // =, !=, >, >=, <, <= �� ������ ��� �񱳿����ڴ�
                // �񱳿������� argument�� column node�� �����Ѵ�.
                // ����, �� �����ڵ��� ���� ���,
                // �ش� ���̺��� �÷��� �ƴ� ���,

                sCurNode = NULL;
            }
        }

        if ( sCurNode != NULL )
        {
            if ( QTC_IS_COLUMN( aStatement, sCurNode ) == ID_TRUE )
            {
                // �񱳿������� argument�� �÷��� ���,
                // columnID�� ���Ѵ�.
                sColumnID =
                    QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sCurNode->node.table].
                    columns[sCurNode->node.column].column.id;
            }
            else
            {
                // Index�� ����� �� ���� �����.
                sIsIndexable = ID_FALSE;
            }
        }
        else
        {
            sIsIndexable = ID_FALSE;
        }
    }

    if ( sIsIndexable == ID_TRUE )
    {
        aPredicate->id = sColumnID;
    }
    else
    {
        aPredicate->id = QMO_COLUMNID_NON_INDEXABLE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::isIndexable( qcStatement   * aStatement,
                      qmoPredicate  * aPredicate,
                      qcDepInfo     * aTableDependencies,
                      qcDepInfo     * aOuterDependencies,
                      idBool        * aIsIndexable )
{
/***********************************************************************
 *
 * Description : predicate�� indexable ���� �Ǵ�
 *
 *     <indexable predicate�� ����>
 *
 *     ����1. indexable operator �̾�� �Ѵ�.
 *            system level operator �Ӹ� �ƴ϶�,
 *            user level operator(quantify�񱳿�����)�� ���Եȴ�.
 *
 *     ����2. column�� �־�� �Ѵ�.
 *            ��) i1=1(O), i1=i2(O), i1=i1+1(O), i1+1=1(O), 1=1(X),
 *                (i1,i2,i3)=(i1,i2,i3)(O), (i1,i2,i3)=(1,1,1)(O),
 *                (i1,i2,1 )=( 1, 1, 1)(X), (1,subquery)=(1,1)(X)
 *
 *     ����3. column�� ������ ����� �Ѵ�.
 *
 *            ��) i1=1(O), i1=i2(O), i1=i1+1(O), i1+1=1(X)
 *
 *     ����4. column�� ���ʿ��� �����ؾ� �Ѵ�.
 *            ��) i1=1(O), i1=i1+1(X), i1=i2(X)
 *
 *     ����5. column�� conversion�� �߻����� �ʾƾ� �Ѵ�.
 *            ��) i1(integer)=smallint'1'(O), i1(integer)=3.5(X)
 *
 *     ����6. value�� ���� ����üũ
 *            6-1. subquery�� ���� ���, subquery type�� A,N���̾�� �Ѵ�.
 *                 ��) a1 = (select i1 from t2 where i1=1)(O)
 *                     a1 = (select sum(i1) from t2)(O)
 *                     a1 = (select i1 from t2 where i1=al)(X)
 *                     a1 = (select sum(i1) from t2 where i1=a1)(X)
 *            6-2. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
 *                 ��) i1 like 'a%'(O) , i1 like '\_a%' escape'\'(O)
 *                     i1 like '%a%'(X), i1 like '_bc'(X)
 *            6-3. host ������ ���� ���
 *                 ��) i1=?(O), i1=?+1(O)
 *
 *     ����7. OR��� ������ ������ �÷��� �ִ� ����̾�� �Ѵ�.
 *            ��, subquery �� ���� ���� ���ܵȴ�.
 *            ��) i1=1 OR i1=2(O), (i1,i2)=(1,1) OR (i1,i2)=(2,2)(O),
 *                i1=1 OR i2=2(X),
 *                i1 in (subquery) OR i1 in (subquery)(X),
 *                i1=1 OR i1 in (subquery) (X)
 *                (i1=1 and i2=1) or i1=( subquery ) (X)
 *
 *     ����8. index�� �־�� �Ѵ�.
 *            ����1~7������ indexable predicate�� �ǴܵǾ�����,
 *            �̿� �ش��ϴ� index�� �־�� �Ѵ�.
 *
 *     < ���� ������������� indexable predicate�� �Ǵܹ��� >
 *
 *     (1) parsing & validation ���������� �Ǵܹ���
 *         ����1, ����2, ����3, �� ������ ������ �����ϸ�
 *         mtcNode.lflag�� MTC_NODE_INDEX_USABLE�� ������.
 *
 *     (2) graph �������������� �Ǵܹ���
 *         ����4, ����5, ����6, ����7
 *
 *     (3) plan tree �������������� �Ǵܹ���
 *         IN���� subquery keyRange�� ���� ���ǰ� ����8
 *
 *     (4) execution ���������� �Ǵܹ���
 *         host������ ���� binding�� ������,
 *         column�� ���� conversion�߻����ο�
 *         LIKE�������� ���Ϲ��ڰ� �Ϲݹ��ڷ� �����ϴ����� ����
 *
 * Implementation : �� �Լ��� graph �������������� �Ǵܹ����� �˻���.
 *
 *     1. ����1,2,3�� �Ǵ�
 *        mtcNode.flag�� MTC_NODE_INDEX_USABLE ������ �˻�
 *
 *     2. ����4�� �Ǵ�
 *        (1) operand�� dependency�� �ߺ����� �ʴ����� �˻��Ѵ�.
 *            (�� �˻�� �÷��� ���ʿ��� �����Ѵٴ� ���� ������)
 *
 *            dependency �ߺ� �Ǵܹ����,
 *            ( ( �񱳿����������� �ΰ� ����� dependencies�� AND����)
 *              & FROM�� �ش� table�� dependency ) != 0
 *
 *        (2) (1)�� �����Ǹ�,
 *            �÷��� ��� �ش� table�� �÷����� �����Ǿ������� �˻�.
 *
 *     3. ����5�� �Ǵ�
 *        (1) column
 *            value�ʿ� host������ ���µ���,
 *            column�� conversion�� �߻��ߴ����� �˻�.
 *
 *        (2) LIST
 *            value�� LIST ���� ������ ��� �����ؼ�, host������ �ƴϸ鼭,
 *            value�� leftConversion�� �����ϴ��� �˻�.
 *
 *     4. ����6�� �Ǵ�
 *        (1) 6-1���� : QTC_NODE_SUBQUERY_EXIST
 *        (2) 6-2���� : subquery�����ڷκ��� type�� �˾Ƴ���.
 *        (3) 6-3���� :
 *            value node�� tuple�� MTC_TUPLE_TYPE_CONSTANT������ �˻�.
 *
 *     5. ����7�� �Ǵ�
 *        (1) OR��� ������ 1���� �񱳿����ڰ� ������ ���,
 *            .indexable predicate������ �Ǵ�
 *        (2) OR��� ������ 2���̻��� �񱳿����ڰ� ������ ���,
 *            . subquery�� �������� �ʾƾ� �Ѵ�.
 *            . ���� �񱳿����ڵ��� ��� indexable�̾�� �Ѵ�.
 *            . ���� ������ columID�� ��� ���ƾ� �Ѵ�.
 *              (��, �÷��� LIST�� ���� non-indexable�� ó��)
 *
 ***********************************************************************/

    UInt     sColumnID;
    UInt     sFirstColumnID;
    idBool   sIsFirstNode = ID_TRUE;
    idBool   sIsIndexablePred = ID_FALSE;
    idBool   sIsOrValueIndex = ID_FALSE;
    idBool   sIsDisable = ID_FALSE;
    qtcNode *sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isIndexable::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aIsIndexable != NULL );

    /* BUG-47986 */
    if ( QCU_OPTIMIZER_OR_VALUE_INDEX == 0 )
    {
        sIsDisable = ID_TRUE;
    }

    //--------------------------------------
    // ���� 7�� �˻�
    // OR��� ������ ������ �÷��� �ִ� ���,
    // ��, subquery ��尡 �����ϴ� ���� ���ܵȴ�.
    //--------------------------------------

    sNode = aPredicate->node;

    if ( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        //--------------------------------------
        // CNF �� ����,
        // ���ڷ� �Ѿ�� predicate�� �ֻ��� ���� OR����̸�,
        // OR ��� ������ �������� �񱳿����ڰ� �� �� �ִ�.
        // OR ��� ������ �񱳿����ڰ� �ϳ��϶��� �������϶���
        // ���ǰ˻簡 Ʋ���Ƿ�, �̸� �����Ͽ� ó���Ѵ�.
        //--------------------------------------

        // sNode�� �񱳿����� ���
        sNode = (qtcNode *)(sNode->node.arguments);

        if ( aPredicate->node->node.arguments->next == NULL )
        {

            // 1. OR ��� ������ �񱳿����ڰ� �ϳ��� ���� ���,
            //    indexable������ �Ǵ��ϸ� �ȴ�.
            IDE_TEST( isIndexableUnitPred( aStatement,
                                           sNode,
                                           aTableDependencies,
                                           aOuterDependencies,
                                           & sIsIndexablePred )
                      != IDE_SUCCESS );

            if ( ( sIsIndexablePred == ID_TRUE ) &&
                 ( aPredicate->node->node.arguments->module == &mtfEqual ) )
            {
                aPredicate->flag &= ~QMO_PRED_INDEXABLE_EQUAL_MASK;
                aPredicate->flag |= QMO_PRED_INDEXABLE_EQUAL_TRUE;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            // 2. OR ��� ������ �񱳿����ڰ� ������ ���� ���,
            //   (1) subquery�� �������� �ʾƾ� �Ѵ�.
            //   (2) �񱳿����ڰ� ��� indexable predicate�̾�� �Ѵ�.
            //   (3) �񱳿����� ����� columnID�� ��� �����ؾ� �Ѵ�.
            //       (��, column�� LIST�� ���� �����Ѵ�.)

            // subquery�� �������� �ʾƾ� �Ѵ�.
            if ( ( aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                sIsIndexablePred = ID_FALSE;
            }
            else
            {
                while ( sNode != NULL )
                {
                    // indexable predicate���� �˻�
                    IDE_TEST( isIndexableUnitPred( aStatement,
                                                   sNode,
                                                   aTableDependencies,
                                                   aOuterDependencies,
                                                   &sIsIndexablePred )
                              != IDE_SUCCESS );

                    if ( sIsIndexablePred == ID_TRUE )
                    {
                        // columnID�� ��´�.
                        IDE_TEST( getColumnID( aStatement,
                                               sNode,
                                               ID_TRUE,
                                               & sColumnID )
                                  != IDE_SUCCESS );

                        if ( sIsFirstNode == ID_TRUE )
                        {
                            // OR ��� ������ ù��° �񱳿����� ó����,
                            // ������ columnID �񱳸� ����,
                            // sFirstColumnID�� columnID�� ����.
                            sFirstColumnID = sColumnID;
                            sIsFirstNode   = ID_FALSE;
                        }
                        else
                        {
                            // Nothing To Do
                        }

                        // column�� LIST�� �ƴ� one column���� �����Ǿ� �ְ�,
                        // ù��° �񱳿������� columnID�� �������� �˻�.
                        if ( ( sColumnID != QMO_COLUMNID_LIST ) &&
                             ( sColumnID == sFirstColumnID ) )
                        {
                            // Nothing To Do
                        }
                        else
                        {
                            sIsIndexablePred = ID_FALSE;
                            break;
                        }
                    }
                    else
                    {
                        // BUG-39036 select one or all value optimization
                        // keyRange or ����� �� �����϶� keyRange ������ �����ϴ°��� �����ϰ� �Ѵ�.
                        // �� ? = 1 �� ���� ���ε� ������ ��������� 1 = 1 �� ���� ���´� ������� �ʴ´�.
                        // select * from t1 where :emp is null or i1 = :emp; ( o )
                        // SELECT * FROM t1 WHERE i1=1 OR 1=1; ( x )

                        // BUG-40878 koscom case
                        // SELECT count(*) FROM t1
                        // WHERE i2 = to_number('20150209') AND ((1 = 0  and i1  = :a1 ) or ( 0 = 0 and i7 = :a2 ));
                        // �� ���ǿ��� 1 = 0  and i1  = :a1 �� indexable �� �Ǵ�������
                        // i2 = to_number('20150209') �� �����ϸ鼭 ����� Ʋ������.
                        // ���� �������� predicate �� ���� ��쿡�� indexable �� �Ǵ����� �ʴ´�.
                        if ( (qtc::haveDependencies( &sNode->depInfo ) == ID_TRUE) ||
                             (sNode->node.module == &qtc::valueModule) ||
                             (aPredicate->next != NULL) ||
                             (sIsDisable == ID_TRUE) )
                        {
                            sIsIndexablePred = ID_FALSE;
                            break;
                        }
                        else
                        {
                            sIsIndexablePred = ID_TRUE;
                            sIsOrValueIndex = ID_TRUE;
                        }
                    }

                    sNode = (qtcNode *)(sNode->node.next);
                }

                /* BUG-47509 */
                if ( sIsOrValueIndex == ID_TRUE )
                {
                    aPredicate->flag &= ~QMO_PRED_OR_VALUE_INDEX_MASK;
                    aPredicate->flag |= QMO_PRED_OR_VALUE_INDEX_TRUE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }
    }
    else
    {
        //------------------------------------------
        // DNF �� ����,
        // ���ڷ� �Ѿ�� predicate�� �ֻ��� ���� �񱳿����� ����̴�.
        // ���� predicate�� ������谡 �������� ���� �����̹Ƿ�,
        // ���ڷ� �Ѿ�� �񱳿����� ��� �ϳ��� ���ؼ��� ó���ؾ� �Ѵ�.
        //-------------------------------------------

        IDE_TEST( isIndexableUnitPred( aStatement,
                                       sNode,
                                       aTableDependencies,
                                       aOuterDependencies,
                                       & sIsIndexablePred )
                  != IDE_SUCCESS );

    }

    *aIsIndexable = sIsIndexablePred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoPred::isROWNUMColumn( qmsQuerySet * aQuerySet,
                         qtcNode     * aNode )
{
/***********************************************************************
 *
 * Description : ROWNUM column�� �Ǵ�
 *
 *     materialized node���� ROWNUM column ó�� ����� value������
 *     �����ϱ� ���ؼ� �ش� ��尡 ROWNUM column������ �Ǵ��Ѵ�.
 *
 * Implementation :
 *
 *      ROWNUM pseudo column�� qmsSFWGH::rownum �ڷᱸ����
 *      qtcNode���·� �����ȴ�.
 *      �� qmsSFWGH::rownum �ڷᱸ���� tupleID�� columnID�� �̿��Ѵ�.
 *
 *      ���ڷ� �Ѿ�� aNode�� tupleID, columnID��
 *      qmsSFWGH::rownum�� tupleID, columnID�� ������,
 *      ROWNUM column���� �Ǵ��Ѵ�.
 *
 ***********************************************************************/

    qtcNode  * sNode;
    idBool     sResult;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // ROWNUM Column�� �Ǵ�
    // �˻��� Node�� qmsSFWGH->rowum�� tupleID�� columnID�� �������� ��.
    //--------------------------------------

    sNode = aQuerySet->SFWGH->rownum;

    if( sNode != NULL )
    {
        // BUG-17949
        // select rownum ... group by rownum�� ���
        // SFWGH->rownum�� passNode�� �޷��ִ�.
        if( sNode->node.module == & qtc::passModule )
        {
            sNode = (qtcNode*) sNode->node.arguments;
        }
        else
        {
            // Nothing to do.
        }

        if( ( aNode->node.table == sNode->node.table ) &&
            ( aNode->node.column == sNode->node.column ) )
        {
            sResult = ID_TRUE;
        }
        else
        {
            sResult = ID_FALSE;
        }
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}


IDE_RC
qmoPred::setPriorNodeID( qcStatement  * aStatement,
                         qmsQuerySet  * aQuerySet,
                         qtcNode      * aNode       )
{
/***********************************************************************
 *
 * Description : prior column�� �����ϴ��� �˻��ؼ�,
 *               prior column�� �����ϸ�,  prior tuple�� �����Ų��.
 *
 *     hierarchy node�� �����Ǿ�� prior tuple�� �� �� �����Ƿ�,
 *     plan node ������, prior column�� ���� tuple�� prior tuple��
 *     �缳���Ѵ�.
 *     ����, ��� expression�� predicate�� �����ϴ�
 *     ������ �� �۾��� �����ؾ� �Ѵ�. predicate �����ڴ� �̷��� �ڵ尡
 *     �л���� �ʵ��� moduleȭ�ϸ�, ��� expression�� predicate��
 *     �����ϴ� �������� �� �Լ��� ȣ���ϵ��� �Ѵ�.
 *
 * Implementation :
 *
 *     hierarchy�� �����ϸ�,
 *     qtc::priorNodeSetWithNewTuple()ȣ��
 *     [ �� �Լ���, �ش� Node�� Traverse�ϸ鼭,
 *       PRIOR Node�� Table ID�� ���ο� Table ID�� �����Ѵ�. ]
 *
 *     prior column�� �ִٸ�, prior tuple�� �����Ű��,
 *     prior column�� ���ٸ�, �ƹ� ó���� ���� �ʴ´�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPred::setPriorNodeID::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // hierarchy�� �����ϸ�, tuple�� �����Ѵ�.
    //--------------------------------------

    if ( aQuerySet->setOp == QMS_NONE )
    {
        // SET ���� �ƴ� ���,
        IDE_FT_ASSERT( aQuerySet->SFWGH != NULL );

        if ( aQuerySet->SFWGH->hierarchy != NULL )
        {
            IDE_TEST( qtc::priorNodeSetWithNewTuple( aStatement,
                                                     & aNode,
                                                     aQuerySet->SFWGH->hierarchy->originalTable,
                                                     aQuerySet->SFWGH->hierarchy->priorTable )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // SET ���� ���,
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::linkPredicate( qcStatement   * aStatement,
                        qmoPredicate  * aPredicate,
                        qtcNode      ** aNode       )
{
/***********************************************************************
 *
 * Description : keyRange, keyFilter�� ����� qmoPredicate��
 *              ����������, �ϳ��� qtcNode�� �׷����� ���������� �����.
 *
 *     keyRange, keyFilter�� ����� ������ qmoPredicate��
 *     ���������� �����Ǿ� ������, �� qmoPredicate->predicate��
 *     qtcNode������ predicate�� ���� �л�Ǿ� �ִ�.
 *     keyRange ������ ����, �� �л�Ǿ� �ִ� qtcNode�� predicate����
 *     �ϳ��� �׷����� ���������� �����.
 *
 * Implementation :
 *
 *     �л�Ǿ� �ִ� qtcNode�� predicate���� �ܼ� �����Ѵ�.
 *     1. ���ο� AND ��� ����
 *     2. AND ��� ������ predicate���� �����Ѵ�.
 *     3. AND ����� flag�� dependencies�� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode        * sANDNode[2];
    qcNamePosition   sNullPosition;
    qmoPredicate   * sPredicate;
    qmoPredicate   * sMorePredicate;
    qtcNode        * sNode = NULL;
    qtcNode        * sLastNode;

    IDU_FIT_POINT_FATAL( "qmoPred::linkPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // ���ο� AND ��带 �ϳ� �����Ѵ�.
    //--------------------------------------

    SET_EMPTY_POSITION( sNullPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sANDNode,
                             & sNullPosition,
                             (const UChar*)"AND",
                             3 )
              != IDE_SUCCESS );

    //--------------------------------------
    // AND ��� ������
    // qmoPredicate->node�� ����� �ִ� predicate���� �����Ѵ�.
    //--------------------------------------

    for ( sPredicate  = aPredicate;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        for ( sMorePredicate  = sPredicate;
              sMorePredicate != NULL;
              sMorePredicate  = sMorePredicate->more )
        {
            // ���� qtcNode�� next������踦 ���´�.
            sMorePredicate->node->node.next = NULL;

            if ( sNode == NULL )
            {
                sNode     = sMorePredicate->node;
                sLastNode = sNode;
            }
            else
            {
                sLastNode->node.next = (mtcNode *)(sMorePredicate->node);
                sLastNode            = (qtcNode *)(sLastNode->node.next);
            }
        }
    }

    sANDNode[0]->node.arguments = (mtcNode *)&(sNode->node);

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sANDNode[0] )
              != IDE_SUCCESS );

    // ���� ������ AND ����� flag�� dependencies�� �缳������ �ʾƵ� �ȴ�.
    // ���� BUG-7557

    *aNode = sANDNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::linkFilterPredicate( qcStatement   * aStatement,
                              qmoPredicate  * aPredicate,
                              qtcNode      ** aNode       )
{
/***********************************************************************
 *
 * Description : filter�� ����� qmoPredicate�� ����������,
 *               �ϳ��� qtcNode�� �׷����� ���������� �����.
 *
 *     filter�� ����� ������ qmoPredicate�� ���������� �����Ǿ� ������,
 *     �� qmoPredicate->node�� qtcNode������ predicate�� ����
 *     �л�Ǿ� �ִ�.
 *     filter ������ ���� �л�Ǿ� �ִ� qtcNode�� predicate����
 *     �ϳ��� �׷����� ���������� �����.
 *
 *     filter�� ����� predicate���� selectivity�� ���� predicate��
 *     ���� ó���ǵ��� �Ͽ� ���õǴ� ���ڵ��� ���� �������ν�,
 *     ������ ����Ų��. ( filter ordering )
 *
 * Implementation :
 *
 *     1. �� predicate�� selectivity�� ���ؼ�,
 *        selectivity�� ���� ������� qmoPredicate�� �����Ѵ�.
 *     2. ���ο� AND��带 �ϳ� �����ϰ�,
 *        AND��� ������ 1���� ���ĵ� ������ qtcNode�� �����Ѵ�.
 *     4. AND ����� flag�� dependencies�� �����Ѵ�.
 *
 ***********************************************************************/

    UInt            sIndex;
    UInt            sPredCnt = 0;
    qtcNode       * sNode;
    qtcNode       * sANDNode[2];
    qcNamePosition  sNullPosition;
    qmoPredicate  * sPredicate;
    qmoPredicate  * sMorePredicate;
    qmoPredicate ** sPredicateArray;

    IDU_FIT_POINT_FATAL( "qmoPred::linkFilterPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_FT_ASSERT( aStatement != NULL );
    IDE_FT_ASSERT( aPredicate != NULL );
    IDE_FT_ASSERT( aNode      != NULL );

    //--------------------------------------
    // filter ordering ����
    // selectivity�� ���� ������� predicate���� �����Ѵ�.
    //--------------------------------------

    //--------------------------------------
    // selectivity�� ���� ������� ����(qsort)�ϱ� ����,
    // predicate�� �迭�� �����.
    //--------------------------------------

    // ���ڷ� �Ѿ�� filter�� predicate ������ ���Ѵ�.
    for ( sPredicate  = aPredicate;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        for ( sMorePredicate  = sPredicate;
              sMorePredicate != NULL;
              sMorePredicate  = sMorePredicate->more )
        {
            sPredCnt ++;
        }
    }

    // qmoPredicate �迭�� ����� ���� �޸��Ҵ�(qsort�� ����)
    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoPredicate* ) * sPredCnt,
                                               (void **)& sPredicateArray )
              != IDE_SUCCESS );

    // �Ҵ���� �޸𸮿� ���ڷ� ���� qmoPredicate �ּ�����
    for ( sPredicate  = aPredicate, sIndex = 0;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        for ( sMorePredicate  = sPredicate;
              sMorePredicate != NULL;
              sMorePredicate  = sMorePredicate->more )
        {
            sMorePredicate->idx     = sIndex;
            sPredicateArray[sIndex] = sMorePredicate;
            sIndex++;
        }
    }

    //--------------------------------------
    // filter�� ó���� predicate�� ������ 2�̻��̸�,
    // selectivity�� ���� ������ �����Ѵ�.
    //--------------------------------------

    if ( sPredCnt > 1 )
    {
        idlOS::qsort( sPredicateArray,
                      sPredCnt,
                      ID_SIZEOF(qmoPredicate*),
                      compareFilter );
    }
    else
    {
        // Nothing To Do
    }

    //--------------------------------------
    // ���ο� AND ��带 �ϳ� �����Ѵ�.
    //--------------------------------------

    SET_EMPTY_POSITION( sNullPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sANDNode,
                             & sNullPosition,
                             (const UChar*)"AND",
                             3 )
              != IDE_SUCCESS );

    //--------------------------------------
    // AND ��� ������ qsort�� ���ĵ� qtcNode�� �����Ѵ�.
    //--------------------------------------

    sANDNode[0]->node.arguments
        = (mtcNode *)(sPredicateArray[0]->node);
    sNode = (qtcNode *)(sANDNode[0]->node.arguments);

    sPredicateArray[0]->node->node.next = NULL; // ������踦 ���´�.

    for ( sIndex = 1;
          sIndex < sPredCnt;
          sIndex++ )
    {
        // ���� predicate�� next ������踦 ���´�.
        sPredicateArray[sIndex]->node->node.next = NULL;
        // AND�� ���� ���� ����.
        sNode->node.next = (mtcNode *)(sPredicateArray[sIndex]->node);
        sNode            = (qtcNode *)(sNode->node.next);
    }

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sANDNode[0] )
              != IDE_SUCCESS );

    *aNode = sANDNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoPred::sortORNodeBySubQ( qcStatement   * aStatement,
                                  qtcNode       * aNode )
{
/***********************************************************************
 *
 * Description : Or node �� ���ڵ��� SubQ �� NonSubQ �� ������.
 *               NonSubQ �� ������ ��Ƽ� ���� �����ϵ��� �Ѵ�.
 *
 * BUG-38971 subQuery filter �� ������ �ʿ䰡 �ֽ��ϴ�.
 *
 ***********************************************************************/

    UInt        i;
    UInt        sNodeCnt;
    UInt        sSubQ;
    UInt        sNonSubQ;
    mtcNode   * sNode;
    mtcNode   * sOrNode;
    mtcNode  ** sNodePtrArray;

    IDU_FIT_POINT_FATAL( "qmoPred::sortORNodeBySubQ::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aNode      != NULL );

    sOrNode = (mtcNode*)aNode;

    // ���ڷ� �Ѿ�� filter�� Node ������ ���Ѵ�.
    for ( sNode  = sOrNode->arguments, sNodeCnt = 0;
          sNode != NULL;
          sNode  = sNode->next )
    {
        sNodeCnt++;
    }

    // �迭�� ����� ���� �޸��Ҵ�
    IDE_TEST( QC_QME_MEM( aStatement )->cralloc( ID_SIZEOF( mtcNode* ) * sNodeCnt * 2,
                                                 (void **)& sNodePtrArray )
              != IDE_SUCCESS );

    // �Ҵ���� �޸𸮿� ���ڷ� ���� Node �ּ�����
    for ( sNode  = sOrNode->arguments, sNonSubQ = 0, sSubQ = sNodeCnt;
          sNode != NULL;
          sNode  = sNode->next )
    {
        if ( ( ((qtcNode*)sNode)->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
        {
            sNodePtrArray[sSubQ]    = sNode;
            sSubQ++;
        }
        else
        {
            sNodePtrArray[sNonSubQ] = sNode;
            sNonSubQ++;
        }
    }

    // Or ����� arguments �� �������ش�.
    for ( i = 0;
          i < sNodeCnt * 2;
          i++ )
    {
        if ( sNodePtrArray[i] != NULL )
        {
            sNodePtrArray[i]->next = NULL;

            sOrNode->arguments = sNodePtrArray[i];
            sNode              = sNodePtrArray[i];
            break;
        }
        else
        {
            // Nothing To Do.
        }
    }

    // ������ ������ �������ش�.
    for ( i = i+1;
          i < sNodeCnt * 2;
          i++ )
    {
        if ( sNodePtrArray[i] != NULL )
        {
            sNodePtrArray[i]->next = NULL;

            sNode->next = sNodePtrArray[i];
            sNode       = sNodePtrArray[i];
        }
        else
        {
            // Nothing To Do.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::removeIndexableFromFilter( qmoPredWrapper * aFilter )
{
    qmoPredicate   * sPredIter;
    qmoPredWrapper * sWrapperIter;

    IDU_FIT_POINT_FATAL( "qmoPred::removeIndexableFromFilter::__FT__" );

    // filter/subquery filter�� ���� subquery ����ȭ �� �����ؾ� ��.
    for ( sWrapperIter  = aFilter;
          sWrapperIter != NULL;
          sWrapperIter  = sWrapperIter->next )
    {
        for ( sPredIter  = sWrapperIter->pred;
              sPredIter != NULL;
              sPredIter  = sPredIter->more )
        {
            sPredIter->id = QMO_COLUMNID_NON_INDEXABLE;

            if ( ( sPredIter->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                IDE_TEST( removeIndexableSubQTip( sPredIter->node )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::relinkPredicate( qmoPredWrapper * aWrapper )
{
    qmoPredWrapper * sWrapperIter;

    IDU_FIT_POINT_FATAL( "qmoPred::relinkPredicate::__FT__" );

    if ( aWrapper != NULL )
    {
        for ( sWrapperIter        = aWrapper;
              sWrapperIter->next != NULL;
              sWrapperIter        = sWrapperIter->next )
        {
            sWrapperIter->pred->next = sWrapperIter->next->pred;
        }
        sWrapperIter->pred->next = NULL;
    }
    else
    {
        // nothing to do...
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoPred::removeMoreConnection( qmoPredWrapper * aWrapper, idBool aIfOnlyList )
{
    qmoPredWrapper * sWrapperIter;

    IDU_FIT_POINT_FATAL( "qmoPred::removeMoreConnection::__FT__" );

    for ( sWrapperIter  = aWrapper;
          sWrapperIter != NULL;
          sWrapperIter  = sWrapperIter->next )
    {
        if ( aIfOnlyList == ID_TRUE )
        {
            if ( sWrapperIter->pred->id == QMO_COLUMNID_LIST )
            {
                sWrapperIter->pred->more = NULL;
            }
            else
            {
                // Nothing to do...
            }
        }
        else
        {
            sWrapperIter->pred->more = NULL;
        }
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::fixPredToRangeAndFilter( qcStatement        * aStatement,
                                  qmsQuerySet        * aQuerySet,
                                  qmoPredWrapper    ** aKeyRange,
                                  qmoPredWrapper    ** aKeyFilter,
                                  qmoPredWrapper    ** aFilter,
                                  qmoPredWrapper    ** aLobFilter,
                                  qmoPredWrapper    ** aSubqueryFilter,
                                  qmoPredWrapperPool * aWrapperPool )
{
    IDU_FIT_POINT_FATAL( "qmoPred::fixPredToRangeAndFilter::__FT__" );

    IDE_DASSERT( aFilter != NULL );
    IDE_DASSERT( aLobFilter != NULL );
    IDE_DASSERT( aSubqueryFilter != NULL );

    // filter, lobFilter, subqueryFilter ����
    // ������踦 �������Ѵ�.
    IDE_TEST( relinkPredicate( *aFilter )
              != IDE_SUCCESS );
    IDE_TEST( relinkPredicate( *aLobFilter )
              != IDE_SUCCESS );
    IDE_TEST( relinkPredicate( *aSubqueryFilter )
              != IDE_SUCCESS );

    IDE_TEST( removeMoreConnection( *aFilter, ID_FALSE )
              != IDE_SUCCESS );
    IDE_TEST( removeMoreConnection( *aLobFilter, ID_FALSE )
              != IDE_SUCCESS );
    IDE_TEST( removeMoreConnection( *aSubqueryFilter, ID_FALSE )
              != IDE_SUCCESS );

    // filter, lobFilter, subqueryFilter�� ����
    // subquery ����ȭ �� �����ؾ� ��.
    IDE_TEST( removeIndexableFromFilter( *aFilter )
              != IDE_SUCCESS );
    IDE_TEST( removeIndexableFromFilter( *aLobFilter )
              != IDE_SUCCESS );
    IDE_TEST( removeIndexableFromFilter( *aSubqueryFilter )
              != IDE_SUCCESS );

    if ( *aKeyRange != NULL )
    {
        // key range ���� ������踦 �������Ѵ�.
        IDE_TEST( relinkPredicate( *aKeyRange )
                  != IDE_SUCCESS );
        IDE_TEST( removeMoreConnection( *aKeyRange, ID_TRUE )
                  != IDE_SUCCESS );

        IDE_TEST( process4Range( aStatement,
                                 aQuerySet,
                                 (*aKeyRange)->pred,
                                 aFilter,
                                 aWrapperPool )
                  != IDE_SUCCESS );

        if ( *aKeyFilter != NULL )
        {
            // key filter ���� ������踦 �������Ѵ�.
            IDE_TEST( relinkPredicate( *aKeyFilter )
                      != IDE_SUCCESS );
            IDE_TEST( removeMoreConnection( *aKeyFilter, ID_TRUE )
                      != IDE_SUCCESS );

            IDE_TEST( process4Range( aStatement,
                                     aQuerySet,
                                     (*aKeyFilter)->pred,
                                     aFilter,
                                     aWrapperPool )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }

        // To fix BUG-15348
        // process4Range���� like�� filter�� �з����� ���
        // filter�� ���� ������ �־�� ��.
        IDE_TEST( relinkPredicate( *aFilter )
                  != IDE_SUCCESS );
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
qmoPred::makePartKeyRangePredicate( qcStatement        * aStatement,
                                    qmsQuerySet        * aQuerySet,
                                    qmoPredicate       * aPredicate,
                                    qcmColumn          * aPartKeyColumns,
                                    qcmPartitionMethod   aPartitionMethod,
                                    qmoPredicate      ** aPartKeyRange )
{
    qmoPredWrapperPool  sWrapperPool;
    qmoPredWrapper    * sPartKeyRange;
    qmoPredWrapper    * sTempWrapper;
    qmoPredWrapper    * sRemain;
    qmoPredWrapper    * sLobFilter;
    qmoPredWrapper    * sSubqueryFilter;

    IDU_FIT_POINT_FATAL( "qmoPred::makePartKeyRangePredicate::__FT__" );

    sPartKeyRange   = NULL;
    sRemain         = NULL;
    sLobFilter      = NULL;
    sSubqueryFilter = NULL;
    sTempWrapper    = NULL;

    IDE_TEST( extractPartKeyRangePredicate( aStatement,
                                            aPredicate,
                                            aPartKeyColumns,
                                            aPartitionMethod,
                                            &sPartKeyRange,
                                            & sRemain,
                                            & sLobFilter,
                                            & sSubqueryFilter,
                                            & sWrapperPool )
              != IDE_SUCCESS );

    IDE_TEST( fixPredToRangeAndFilter( aStatement,
                                       aQuerySet,
                                       & sPartKeyRange,
                                       & sTempWrapper,
                                       & sTempWrapper,
                                       & sTempWrapper,
                                       & sTempWrapper,
                                       & sWrapperPool )
              != IDE_SUCCESS );

    if ( sPartKeyRange == NULL )
    {
        *aPartKeyRange = NULL;
    }
    else
    {
        *aPartKeyRange = sPartKeyRange->pred;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::makePartFilterPredicate( qcStatement        * aStatement,
                                  qmsQuerySet        * aQuerySet,
                                  qmoPredicate       * aPredicate,
                                  qcmColumn          * aPartKeyColumns,
                                  qcmPartitionMethod   aPartitionMethod,
                                  qmoPredicate      ** aPartFilter,
                                  qmoPredicate      ** aRemain,
                                  qmoPredicate      ** aSubqueryFilter )
{
    qmoPredWrapperPool  sWrapperPool;
    qmoPredWrapper    * sPartFilter;
    qmoPredWrapper    * sRemain;
    qmoPredWrapper    * sLobFilter;
    qmoPredWrapper    * sSubqueryFilter;
    qmoPredWrapper    * sTempWrapper;

    IDU_FIT_POINT_FATAL( "qmoPred::makePartFilterPredicate::__FT__" );

    sPartFilter     = NULL;
    sRemain         = NULL;
    sLobFilter      = NULL;
    sSubqueryFilter = NULL;
    sTempWrapper    = NULL;

    IDE_TEST( extractPartKeyRangePredicate( aStatement,
                                            aPredicate,
                                            aPartKeyColumns,
                                            aPartitionMethod,
                                            & sPartFilter,
                                            & sRemain,
                                            & sLobFilter,
                                            & sSubqueryFilter,
                                            & sWrapperPool )
              != IDE_SUCCESS );

    if ( sPartFilter == NULL )
    {
        *aPartFilter = NULL;
    }
    else
    {
        *aPartFilter = sPartFilter->pred;
    }

    // partFilter�� predicate������踦 �̹� �����Ͽ����Ƿ�,
    // remain, subqueryFilter�� ������� �����ϸ� �ȴ�.
    IDE_TEST( fixPredToRangeAndFilter( aStatement,
                                       aQuerySet,
                                       & sPartFilter,
                                       & sTempWrapper,
                                       & sRemain,
                                       & sLobFilter,
                                       & sSubqueryFilter,
                                       & sWrapperPool )
              != IDE_SUCCESS );

    if ( sRemain == NULL )
    {
        *aRemain = NULL;
    }
    else
    {
        *aRemain = sRemain->pred;
    }

    if ( sSubqueryFilter == NULL )
    {
        *aSubqueryFilter = NULL;
    }
    else
    {
        *aSubqueryFilter = sSubqueryFilter->pred;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::extractPartKeyRangePredicate( qcStatement         * aStatement,
                                       qmoPredicate        * aPredicate,
                                       qcmColumn           * aPartKeyColumns,
                                       qcmPartitionMethod    aPartitionMethod,
                                       qmoPredWrapper     ** aPartKeyRange,
                                       qmoPredWrapper     ** aRemain,
                                       qmoPredWrapper     ** aLobFilter,
                                       qmoPredWrapper     ** aSubqueryFilter,
                                       qmoPredWrapperPool  * aWrapperPool )
{
    qmoPredWrapper * sSource;

    IDU_FIT_POINT_FATAL( "qmoPred::extractPartKeyRangePredicate::__FT__" );

    *aPartKeyRange = NULL;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aPartKeyColumns != NULL );

    //--------------------------------------
    // ����� �ʱ�ȭ
    //--------------------------------------
    IDE_TEST( qmoPredWrapperI::initializeWrapperPool( QC_QMP_MEM( aStatement ),
                                                      aWrapperPool )
              != IDE_SUCCESS );

    *aPartKeyRange = NULL;

    IDE_TEST( qmoPredWrapperI::createWrapperList( aPredicate,
                                                  aWrapperPool,
                                                  & sSource )
              != IDE_SUCCESS );

    IDE_TEST( extractPartKeyRange4LIST( aStatement,
                                        aPartKeyColumns,
                                        & sSource,
                                        aPartKeyRange,
                                        aWrapperPool )
              != IDE_SUCCESS );

    IDE_TEST( extractPartKeyRange4Column( aPartKeyColumns,
                                          aPartitionMethod,
                                          & sSource,
                                          aPartKeyRange )
              != IDE_SUCCESS );

    IDE_TEST( separateFilters( QC_SHARED_TMPLATE( aStatement ),
                               sSource,
                               aRemain,
                               aLobFilter,
                               aSubqueryFilter,
                               aWrapperPool )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::extractPartKeyRange4LIST( qcStatement        * aStatement,
                                   qcmColumn          * aPartKeyColumns,
                                   qmoPredWrapper    ** aSource,
                                   qmoPredWrapper    ** aPartKeyRange,
                                   qmoPredWrapperPool * aWrapperPool )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *               LIST�� ���� partition keyRange�� �����Ѵ�.
 *
 * Implementation :
 *
 *   �� �Լ��� ����Ʈ �÷�����Ʈ�� ���ڷ� �޾Ƽ�,
 *   �ϳ��� ����Ʈ�� ���� partition keyRange��
 *   �Ǵ��ؼ� �ش��ϴ� ���� �����Ѵ�.
 *
 *      ����Ʈ�� ��Ƽ�� �����/���͸� ���ɼ� �˻�.
 *      (1) ����Ʈ�÷��� ��Ƽ�� Ű �÷��� ���������� ���ԵǸ�,
 *          partition keyRange�� �з�
 *      (2) �׷��� �ʴٸ� ������ filter��� ������.
 ***********************************************************************/

    qmoPredWrapper * sWrapperIter;
    qmoPredicate   * sPredIter;
    qmoPredType      sPredType;

    IDU_FIT_POINT_FATAL( "qmoPred::extractPartKeyRange4LIST::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPartKeyColumns != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aPartKeyRange != NULL );

    //--------------------------------------
    // ����Ʈ �÷�����Ʈ�� �޷��ִ� ������ �÷��� ����
    // partition keyRange/filter�� ���� �Ǵ��� ����.
    //--------------------------------------

    for ( sWrapperIter  = *aSource;
          sWrapperIter != NULL;
          sWrapperIter  = sWrapperIter->next )
    {
        if ( sWrapperIter->pred->id == QMO_COLUMNID_LIST )
        {
            IDE_TEST( qmoPredWrapperI::extractWrapper( sWrapperIter,
                                                       aSource )
                      != IDE_SUCCESS );

            break;
        }
        else
        {
            // Nothing to do...
        }
    }

    //--------------------------------------
    // partition keyrange ����
    //--------------------------------------

    if ( sWrapperIter != NULL )
    {
        for ( sPredIter  = sWrapperIter->pred;
              sPredIter != NULL;
              sPredIter  = sPredIter->more )
        {
            IDE_TEST( checkUsablePartitionKey4List( aStatement,
                                                    aPartKeyColumns,
                                                    sPredIter,
                                                    & sPredType )
                      != IDE_SUCCESS );

            if ( sPredType == QMO_KEYRANGE )
            {
                IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                    aPartKeyRange,
                                                    aWrapperPool )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        } // for
    }
    else // list predicate�� ���� ��� �ƹ��� �۾��� ���� �ʴ´�.
    {
        // Nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::extractPartKeyRange4Column( qcmColumn        * aPartKeyColumns,
                                     qcmPartitionMethod aPartitionMethod,
                                     qmoPredWrapper  ** aSource,
                                     qmoPredWrapper  ** aPartKeyRange )
{
/***********************************************************************
 *
 * Description : PROJ-1502 PARTITIONED DISK TABLE
 *               one column�� ���� partition keyRange�� �����Ѵ�.
 *
 *
 * Implementation :
 *
 *   partition key �÷�������� �Ʒ��� �۾� ����
 *
 *   1. partition key �÷��� ���� �÷��� �÷�����Ʈ�� ã�´�.
 *      (1) ���� partition keyRange�� �з��� predicate�� ���� ���,
 *          keyRange�� ã�� predicate ����
 *      (2) �̹� partition keyRange�� �з��� predicate�� �����ϴ� ���,
 *             partition keyRange �� �����ϰ�, ���� partition key �÷����� ����.
 *   2. ã�� �÷���
 *      (1) ���� partition key �÷� ��� ������ �÷��̸� ( equal(=), in )
 *      (2) ���� partition key �÷� ��� �Ұ����� �÷��̸�, (equal���� predicate����)
 *
 *
 ***********************************************************************/

    UInt             sPartKeyColumnID;
    qmoPredWrapper * sWrapperIter;
    qcmColumn      * sKeyColumn;
    idBool           sIsExtractable;

    IDU_FIT_POINT_FATAL( "qmoPred::extractPartKeyRange4Column::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPartKeyColumns != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aPartKeyRange != NULL );

    //--------------------------------------
    // Partition KeyRange ����
    //--------------------------------------

    // hash partition method�� ��� ��� partition key column�� �����ؾ� �Ѵ�.
    // ���� ��� equality �����̾�� �Ѵ�.
    if ( ( aPartitionMethod == QCM_PARTITION_METHOD_HASH ) ||
         ( aPartitionMethod == QCM_PARTITION_METHOD_RANGE_USING_HASH ) )
    {
        for ( sKeyColumn  = aPartKeyColumns;
              sKeyColumn != NULL;
              sKeyColumn  = sKeyColumn->next )
        {
            // partition key �÷��� columnID�� ���Ѵ�.
            sPartKeyColumnID = sKeyColumn->basicInfo->column.id;

            // partition key �÷��� ���� columnID�� ã�´�.
            for ( sWrapperIter  = *aSource;
                  sWrapperIter != NULL;
                  sWrapperIter  = sWrapperIter->next )
            {
                if ( sPartKeyColumnID == sWrapperIter->pred->id )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            if ( sWrapperIter == NULL )
            {
                break;
            }
            else
            {
                if ( ( sWrapperIter->pred->flag &
                       QMO_PRED_NEXT_KEY_USABLE_MASK )
                     == QMO_PRED_NEXT_KEY_USABLE )
                {
                    // �� �÷��� equal(=)�� in predicate������ �����Ǿ� �ִ�.
                    // ���� partition key �÷����� ����.
                    // Nothing To Do
                }
                else
                {
                    // �� �÷��� equal(=)/in �̿��� predicate�� �����ϰ� �����Ƿ�,
                    // ���� partition key �÷��� ����� �� ����.

                    // ���� partition key���� �۾��� �������� �ʴ´�.
                    break;

                }
            }
        }

        // ��� �����Ǵ� partition key �÷��� ���;� �Ѵ�.
        if ( sKeyColumn == NULL )
        {
            sIsExtractable = ID_TRUE;
        }
        else
        {
            sIsExtractable = ID_FALSE;
        }
    }
    else
    {
        sIsExtractable = ID_TRUE;
    }


    //--------------------------------------
    // partition key �÷�������� partition key �÷��� ������ �÷��� ã�Ƽ�
    // partition keyRange���� ���θ� �����Ѵ�.
    // 1. partition key �÷��� ���������� ��밡���Ͽ��� �Ѵ�.
    //    ��: partition key on T1(i1, i2, i3)
    //      (1) i1=1 and i2=1 and i3=1 ==> i1,i2,i3 ��� ��� ����
    //      (2) i1=1 and i3=1          ==> i1�� ��� ����
    // 2. subquery�� ����.
    //--------------------------------------

    if ( sIsExtractable == ID_TRUE )
    {
        for ( sKeyColumn  = aPartKeyColumns;
              sKeyColumn != NULL;
              sKeyColumn  = sKeyColumn->next )
        {
            // partition key �÷��� columnID�� ���Ѵ�.
            sPartKeyColumnID = sKeyColumn->basicInfo->column.id;


            // partition key �÷��� ���� columnID�� ã�´�.
            for ( sWrapperIter  = *aSource;
                  sWrapperIter != NULL;
                  sWrapperIter  = sWrapperIter->next )
            {
                if ( sPartKeyColumnID == sWrapperIter->pred->id )
                {
                    /* BUG-42172  If _PROWID pseudo column is compared with a column having
                     * PRIMARY KEY constraint, server stops abnormally.
                     */
                    if ( ( sWrapperIter->pred->node->lflag &
                           QTC_NODE_COLUMN_RID_MASK )
                         != QTC_NODE_COLUMN_RID_EXIST )
                    {
                        break;
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                else
                {
                    // Nothing To Do
                }
            }

            if ( sWrapperIter == NULL )
            {
                // ���� partition key �÷��� ������ �÷��� predicate�� �������� �ʴ� ���
                break;
            }
            else
            {
                // ���� partition key �÷��� ������ �÷��� predicate�� �����ϴ� ���

                IDE_TEST( qmoPredWrapperI::moveWrapper( sWrapperIter,
                                                        aSource,
                                                        aPartKeyRange )
                          != IDE_SUCCESS );

                if ( ( sWrapperIter->pred->flag &
                       QMO_PRED_NEXT_KEY_USABLE_MASK )
                     == QMO_PRED_NEXT_KEY_USABLE )
                {
                    // �� �÷��� equal(=)�� in predicate������ �����Ǿ� �ִ�.
                    // ���� partition key �÷����� ����.
                    // Nothing To Do
                }
                else
                {
                    // �� �÷��� equal(=)/in �̿��� predicate�� �����ϰ� �����Ƿ�,
                    // ���� partition key �÷��� ����� �� ����.

                    // ���� partition key���� �۾��� �������� �ʴ´�.
                    break;

                }
            } // partition key �÷��� ���� �÷��� ���� predicate�� ���� ���,
        } //  for()��
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
qmoPred::checkUsablePartitionKey4List( qcStatement   * aStatement,
                                       qcmColumn     * aPartKeyColumns,
                                       qmoPredicate  * aPredicate,
                                       qmoPredType   * aPredType )
{
/***********************************************************************
 *
 * Description : LIST �÷�����Ʈ�� partition key ��뿩�� �˻�
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    UInt           sCount;
    UInt           sListCount;
    UInt           sPartKeyColumnID;
    UInt           sColumnID;
    idBool         sIsKeyRange;
    idBool         sIsExist;
    idBool         sIsNotExistIndexCol = ID_FALSE;
    qtcNode      * sCompareNode;
    qtcNode      * sColumnLIST;
    qtcNode      * sColumnNode;
    qcmColumn    * sColumn;

    IDU_FIT_POINT_FATAL( "qmoPred::checkUsablePartitionKey4List::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPartKeyColumns != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aPredType != NULL );

    //---------------------------------------
    // LIST�� �ε��� ��� ���ɼ� �˻�
    // �ε��� ��밡�� �÷��� LIST�÷��� ��� ���ԵǱ⸸ �ϸ� �ȴ�.
    // 1. ����Ʈ�÷��� �ε��� �÷��� ���������� ��� ���ԵǸ�,
    //     keyRange�� �з�
    // 2. ������������ ������, ����Ʈ �÷��� ��� �ε��� �÷���
    //    ���ԵǸ�, keyFilter�� �з�
    // 3. 1��2�� ���Ե��� ������, filter�� �з�
    //---------------------------------------

    if ( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);
    }
    else
    {
        sCompareNode = aPredicate->node;
    }

    if( sCompareNode->indexArgument == 0 )
    {
        sColumnLIST = (qtcNode *)(sCompareNode->node.arguments);
    }
    else
    {
        sColumnLIST = (qtcNode *)(sCompareNode->node.arguments->next);
    }

    // LIST column�� ���� ȹ��
    sListCount = sColumnLIST->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // LIST�� �ε��� ��� ���ɼ� �˻�.
    sCount      = 0;
    sIsKeyRange = ID_TRUE;
    for ( sColumn  = aPartKeyColumns;
          sColumn != NULL;
          sColumn  = sColumn->next )
    {
        sPartKeyColumnID = sColumn->basicInfo->column.id;

        for ( sColumnNode = (qtcNode *)(sColumnLIST->node.arguments);
              sColumnNode != NULL;
              sColumnNode  = (qtcNode *)(sColumnNode->node.next) )
        {
            sColumnID = QC_SHARED_TMPLATE(aStatement)->
                tmplate.rows[sColumnNode->node.table].
                columns[sColumnNode->node.column].column.id;

            if ( sPartKeyColumnID == sColumnID )
            {
                sCount++;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sColumnNode != NULL )
        {
            // Nothing To Do
            if ( sIsNotExistIndexCol == ID_TRUE )
            {
                sIsKeyRange = ID_FALSE;
                sIsNotExistIndexCol = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            if ( sIsNotExistIndexCol == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                sIsNotExistIndexCol = ID_TRUE;
            }
        }
    }

    if ( sCount == sListCount )
    {
        // LIST�� ��� column�� index ���� ���Ե�
        sIsExist = ID_TRUE;
    }
    else
    {
        // LIST�� column ��  index�� ���Ե��� �ʴ� ���� ������.
        // filter ó�� ����
        sIsExist = ID_FALSE;
    }

    if ( ( sIsExist == ID_TRUE ) &&
         ( sIsKeyRange == ID_TRUE ) )
    {
        *aPredType = QMO_KEYRANGE;
    }
    else
    {
        *aPredType = QMO_FILTER;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoPred::makeRangeAndFilterPredicate( qcStatement   * aStatement,
                                      qmsQuerySet   * aQuerySet,
                                      idBool          aIsMemory,
                                      qmoPredicate  * aPredicate,
                                      qcmIndex      * aIndex,
                                      qmoPredicate ** aKeyRange,
                                      qmoPredicate ** aKeyFilter,
                                      qmoPredicate ** aFilter,
                                      qmoPredicate ** aLobFilter,
                                      qmoPredicate ** aSubqueryFilter )
{
    qmoPredWrapperPool  sWrapperPool;
    qmoPredWrapper    * sKeyRange;
    qmoPredWrapper    * sKeyFilter;
    qmoPredWrapper    * sFilter;
    qmoPredWrapper    * sLobFilter;
    qmoPredWrapper    * sSubqueryFilter;

    IDU_FIT_POINT_FATAL( "qmoPred::makeRangeAndFilterPredicate::__FT__" );

    IDE_TEST( extractRangeAndFilter( aStatement,
                                     QC_SHARED_TMPLATE( aStatement ),
                                     aIsMemory,
                                     ID_FALSE,
                                     aIndex,
                                     aPredicate,
                                     & sKeyRange,
                                     & sKeyFilter,
                                     & sFilter,
                                     & sLobFilter,
                                     & sSubqueryFilter,
                                     & sWrapperPool )
                 != IDE_SUCCESS );

    IDE_TEST( fixPredToRangeAndFilter( aStatement,
                                       aQuerySet,
                                       & sKeyRange,
                                       & sKeyFilter,
                                       & sFilter,
                                       & sLobFilter,
                                       & sSubqueryFilter,
                                       & sWrapperPool )
              != IDE_SUCCESS );

    // keyRange
    if ( sKeyRange == NULL )
    {
        *aKeyRange = NULL;
    }
    else
    {
        *aKeyRange = sKeyRange->pred;
    }

    // keyFilter
    if ( sKeyFilter == NULL )
    {
        *aKeyFilter = NULL;
    }
    else
    {
        *aKeyFilter = sKeyFilter->pred;
    }

    // filter
    if ( sFilter == NULL )
    {
        *aFilter = NULL;
    }
    else
    {
        *aFilter = sFilter->pred;
    }

    // lobFilter
    if ( sLobFilter == NULL )
    {
        *aLobFilter = NULL;
    }
    else
    {
        *aLobFilter = sLobFilter->pred;
    }

    // subqueryFilter
    if ( sSubqueryFilter == NULL )
    {
        *aSubqueryFilter = NULL;
    }
    else
    {
        *aSubqueryFilter = sSubqueryFilter->pred;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::extractRangeAndFilter( qcStatement        * aStatement,
                                qcTemplate         * aTemplate,
                                idBool               aIsMemory,
                                idBool               aInExecutionTime,
                                qcmIndex           * aIndex,
                                qmoPredicate       * aPredicate,
                                qmoPredWrapper    ** aKeyRange,
                                qmoPredWrapper    ** aKeyFilter,
                                qmoPredWrapper    ** aFilter,
                                qmoPredWrapper    ** aLobFilter,
                                qmoPredWrapper    ** aSubqueryFilter,
                                qmoPredWrapperPool * aWrapperPool )
{
/***********************************************************************
 *
 * Description : keyRange, keyFilter, filter, subqueryFilter�� �����Ѵ�.
 *
 *   ���Ǵ� �ε��� ������ ����,
 *   1. disk table �̸�,
 *      keyRange, keyFilter, filter, subqueryFilter�� �и�
 *   2. memory table �̸�,
 *      keyRange, filter, subqueryFilter�� �и��Ѵ�.
 *      (��, keyFilter�� �и��س��� �ʴ´�.)
 *
 * Implementation :
 *
 *   1. KeyRange ����
 *      1) LIST �÷��� ���� ó��
 *          LIST �÷�����Ʈ�� ����� predicate�鿡 ���ؼ�,
 *          keyRange, keyFilter, filter, subqueryFilter�� �и��Ѵ�.
 *      2) one column�� ���� ó��.
 *         (1) index nested loop join predicate�� �����ϴ� ���,
 *             join index ����ȭ�� ���Ͽ� join predicate�� �켱 ����.
 *             ���� BUG-7098
 *             ( ����, LIST���� ����� keyRange�� �ִٸ�,
 *               .IN subquery�� ���� subqueryFilter��
 *               .IN subquery�� �ƴ� ���� keyFilter�� �з��ǵ��� �Ѵ�. )
 *         (2) index nested loop join predicate�� �������� �ʴ� ���,
 *           A. LIST �÷��� ���� keyRange�� �����ϸ�,
 *              one column�� ���� keyRange�� �������� �ʰ�
 *           B. LIST �÷��� ���� keyRange�� �������� ������,
 *              one column�� ���� keyRange ����
 *   2. keyFilter ����
 *      (1) ����� keyRange�� �����ϸ�,
 *      (2) disk table�� ���ؼ� keyFilter ����
 *   3. filter, subquery filter  ó��
 *
 ***********************************************************************/

    qmoPredWrapper * sSource;

    IDU_FIT_POINT_FATAL( "qmoPred::extractRangeAndFilter::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aKeyRange != NULL );
    IDE_DASSERT( aKeyFilter != NULL );
    IDE_DASSERT( aFilter != NULL );
    IDE_DASSERT( aLobFilter != NULL );
    IDE_DASSERT( aSubqueryFilter != NULL );

    //--------------------------------------
    // ����� �ʱ�ȭ
    //--------------------------------------

    if ( aInExecutionTime == ID_TRUE )
    {
        IDE_TEST( qmoPredWrapperI::initializeWrapperPool( QC_QMX_MEM( aStatement ),
                                                          aWrapperPool )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qmoPredWrapperI::initializeWrapperPool( QC_QMP_MEM( aStatement ),
                                                          aWrapperPool )
                  != IDE_SUCCESS );
    }

    *aKeyRange       = NULL;
    *aKeyFilter      = NULL;
    *aFilter         = NULL;
    *aLobFilter      = NULL;
    *aSubqueryFilter = NULL;

    IDE_TEST( qmoPredWrapperI::createWrapperList( aPredicate,
                                                  aWrapperPool,
                                                  & sSource )
              != IDE_SUCCESS );

    //--------------------------------------
    // keyRange, keyFilter, filter, subuqeyrFilter ����
    //--------------------------------------

    if ( aIndex == NULL )
    {
        // filter, subqueryFilter�� �и��ؼ� �ѱ��.
        IDE_TEST( separateFilters( aTemplate,
                                   sSource,
                                   aFilter,
                                   aLobFilter,
                                   aSubqueryFilter,
                                   aWrapperPool )
                  != IDE_SUCCESS );
    }
    else
    {
        //--------------------------------------------
        // LIST �÷��� ���� ó��
        // : ���� predicate ���Ḯ��Ʈ���� LIST �÷��� �и��ؼ�,
        //   LIST �÷��� ���� keyRange/keyFilter/filter/subqueryFilter�� �и�
        //--------------------------------------------

        IDE_TEST( extractRange4LIST( aTemplate,
                                     aIndex,
                                     & sSource,
                                     aKeyRange,
                                     aKeyFilter,
                                     aFilter,
                                     aSubqueryFilter,
                                     aWrapperPool )
                  != IDE_SUCCESS );

        //--------------------------------------
        // keyRange ����
        // 1. index nested loop join predicate�� �ִ� ���,
        //    index nested loop join predicate�� keyRange�� ���õǵ��� �Ѵ�.
        // 2. index nested loop join predicate�� ���� ���,
        //    (1) LIST���� ���õ� keyRange�� �ִ� ���,
        //        one column�� ���� keyRange ���� skip
        //    (2) LIST���� ���õ� keyRange�� ���� ���,
        //        one column�� ���� keyRange ����
        //--------------------------------------

        IDE_TEST( extractKeyRange( aIndex,
                                   & sSource,
                                   aKeyRange,
                                   aKeyFilter,
                                   aSubqueryFilter )
                  != IDE_SUCCESS );

        //--------------------------------------
        // keyFilter ����
        // : LIST �÷��� �����ϴ� ���, �̹� LIST �÷�ó���� keyFilter �з���.
        //   ����, one column�� ���� keyFilter�� �����ϸ� �ȴ�.
        //
        // 1. keyRange ��������
        //    (1) keyRange�� ���� : keyFilter ����
        //    (2) keyRange�� �������� ������, keyFilter�� �뵵�� ���ǹ��ϹǷ�,
        //        keyFilter�� �������� �ʴ´�.
        // 2. table�� ����
        //    (1) disk table   : keyFilter ����(disk I/O�� ���̱� ����)
        //    (2) memory table : keyFilter �������� ����.(�ε�������� �ش�ȭ)
        //--------------------------------------

        IDE_TEST( extractKeyFilter( aIsMemory,
                                    aIndex,
                                    & sSource,
                                    aKeyRange,
                                    aKeyFilter,
                                    aFilter )
                  != IDE_SUCCESS );

        //--------------------------------------
        // Filter ����
        //--------------------------------------

        // keyRange, keyFilter ������,
        // �����ִ� predicate��� ��� filter�� �з��Ѵ�.
        // ���� �����ִ� predicate�� ������ �� �����̴�.
        // (1) one column�� indexable predicate
        // (2) non-indexable predicate

        IDE_TEST( separateFilters( aTemplate,
                                   sSource,
                                   aFilter,
                                   aLobFilter,
                                   aSubqueryFilter,
                                   aWrapperPool )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::getColumnID( qcStatement   * aStatement,
                      qtcNode       * aNode,
                      idBool          aIsIndexable,
                      UInt          * aColumnID )
{
/***********************************************************************
 *
 * Description : columnID ����
 *
 *     selection graph�� ��� predicate�� ���� columnID�� �����Ѵ�.
 *
 *     �� �Լ��� ȣ���ϴ� ����
 *     (1) qmoPred::classifyTablePredicate()
 *     (2) qmoPred::makeJoinPushDownPredicate()
 *     (3) qmoPred::addNonJoinablePredicate() �̸�,
 *     columnID�� �����ϴ� �Լ��� �������� ����ϱ� ���ؼ�,
 *     predicate�� indexable���θ� ���ڷ� �޴´�.
 *
 * Implementation :
 *
 *     1. indexable predicate �� ���,
 *        (1) one column : �ش� columnID����
 *        (2) LIST       : QMO_COLUMNID_LIST�� ����
 *
 *        OR ��� ������ �������� �񱳿����ڰ� �ִ���,
 *        ��� ���� �÷����� �����Ǿ� �ִ�.
 *
 *     2. non-indexable predicate �� ���,
 *        QMO_COLUMNID_NON_INDEXABLE�� ����
 *     (���ڷ� ���� �� �ִ� ����� ���´� ������ ����.)
 *
 *
 *     (1)  OR         (2)  OR                        (3) �񱳿�����
 *          |               |                                 |
 *       �񱳿�����     �񱳿�����->...->�񱳿�����
 *          |               |                |
 *
 ***********************************************************************/

    qtcNode * sNode = aNode;
    qtcNode * sCurNode;

    IDU_FIT_POINT_FATAL( "qmoPred::getColumnID::__FT__" );

    // �ʱⰪ ����
    *aColumnID = QMO_COLUMNID_NOT_FOUND;

    if ( aIsIndexable == ID_TRUE )
    {
        //--------------------------------------
        // indexable predicate�� ����
        // one column�� LIST�� columnID ����
        //--------------------------------------

        //--------------------------------------
        // �񱳿����� ��带 ã�´�.
        // ���ڷ� �Ѿ�� �ֻ��� ��尡 ���������� ���,
        // �񱳿����� ��带 ã�� ����, ��������(OR) skip
        //--------------------------------------
        if ( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
             == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sNode = (qtcNode *)(sNode->node.arguments);
        }

        // BUG-39036 select one or all value optimization
        // keyRange or ����� �� �����϶� keyRange ������ �����ϴ°��� �����ϰ� �Ѵ�.
        // ������� ������ keyRange�� �� �� �����Ƿ� ��带 ��ȸ�ؾ� �Ѵ�.
        while ( sNode != NULL )
        {
            if ( qtc::dependencyEqual( & sNode->depInfo,
                                       & qtc::zeroDependencies ) == ID_FALSE )
            {
                //--------------------------------------
                // indexArgument ������ columnID�� �����Ѵ�.
                //--------------------------------------

                // indexArgument ������ column�� ã�´�.
                if ( sNode->indexArgument == 0 )
                {
                    sCurNode = (qtcNode *)( sNode->node.arguments );
                }
                else // (sNode->indexArgument == 1)
                {
                    sCurNode = (qtcNode *)( sNode->node.arguments->next );
                }

                // ã�� column������ columnID ����
                if ( ( sCurNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_LIST )
                {
                    // LIST�� columnID ����
                    *aColumnID = QMO_COLUMNID_LIST;
                }
                else if ( QTC_IS_RID_COLUMN( sCurNode ) == ID_TRUE )
                {
                    /* BUG-41599 */
                    *aColumnID = QMO_COLUMNID_NON_INDEXABLE;
                }
                else
                {
                    // one column�� columnID ����
                    *aColumnID =
                        QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sCurNode->node.table].
                        columns[sCurNode->node.column].column.id;
                }

                break;
            }
            else
            {
                sNode = (qtcNode *)(sNode->node.next);
            }
        }
    }
    else
    {
        // non-indexable predicate�� ���� columnID ����.
        *aColumnID = QMO_COLUMNID_NON_INDEXABLE;
    }

    // ���� �������� columnID�� �ݵ�� ã�ƾ� �Ѵ�.
    IDE_DASSERT( *aColumnID != QMO_COLUMNID_NOT_FOUND );

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::checkUsableIndex4List( qcTemplate    * aTemplate,
                                qcmIndex      * aIndex,
                                qmoPredicate  * aPredicate,
                                qmoPredType   * aPredType )
{
/***********************************************************************
 *
 * Description : LIST �÷�����Ʈ�� �ε��� ��뿩�� �˻�
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    UInt           sCount;
    UInt           sKeyColCount;
    UInt           sListCount;
    UInt           sIdxColumnID;
    UInt           sColumnID;
    idBool         sIsKeyRange;
    idBool         sIsExist;
    idBool         sIsNotExistIndexCol = ID_FALSE;
    qtcNode      * sCompareNode;
    qtcNode      * sColumnLIST;
    qtcNode      * sColumnNode;

    IDU_FIT_POINT_FATAL( "qmoPred::checkUsableIndex4List::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aPredType != NULL );

    //---------------------------------------
    // LIST�� �ε��� ��� ���ɼ� �˻�
    // �ε��� ��밡�� �÷��� LIST�÷��� ��� ���ԵǱ⸸ �ϸ� �ȴ�.
    // 1. ����Ʈ�÷��� �ε��� �÷��� ���������� ��� ���ԵǸ�,
    //     keyRange�� �з�
    // 2. ������������ ������, ����Ʈ �÷��� ��� �ε��� �÷���
    //    ���ԵǸ�, keyFilter�� �з�
    // 3. 1��2�� ���Ե��� ������, filter�� �з�
    //---------------------------------------

    if ( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);
    }
    else
    {
        sCompareNode = aPredicate->node;
    }

    if ( sCompareNode->indexArgument == 0 )
    {
        sColumnLIST = (qtcNode *)(sCompareNode->node.arguments);
    }
    else
    {
        sColumnLIST = (qtcNode *)(sCompareNode->node.arguments->next);
    }

    // LIST column�� ���� ȹ��
    sListCount = sColumnLIST->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK;

    // LIST�� �ε��� ��� ���ɼ� �˻�.
    sCount      = 0;
    sIsKeyRange = ID_TRUE;
    for ( sKeyColCount = 0;
          sKeyColCount < aIndex->keyColCount;
          sKeyColCount++ )
    {
        sIdxColumnID = aIndex->keyColumns[sKeyColCount].column.id;

        for ( sColumnNode = (qtcNode *)(sColumnLIST->node.arguments);
              sColumnNode != NULL;
              sColumnNode  = (qtcNode *)(sColumnNode->node.next) )
        {
            sColumnID = aTemplate->
                tmplate.rows[sColumnNode->node.table].
                columns[sColumnNode->node.column].column.id;

            if ( sIdxColumnID == sColumnID )
            {
                sCount++;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sColumnNode != NULL )
        {
            // Nothing To Do
            if ( sIsNotExistIndexCol == ID_TRUE )
            {
                sIsKeyRange = ID_FALSE;
                sIsNotExistIndexCol = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            if ( sIsNotExistIndexCol == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                sIsNotExistIndexCol = ID_TRUE;
            }
        }
    }

    if ( sCount == sListCount )
    {
        // LIST�� ��� column�� index ���� ���Ե�
        sIsExist = ID_TRUE;
    }
    else
    {
        // LIST�� column ��  index�� ���Ե��� �ʴ� ���� ������.
        // filter ó�� ����
        sIsExist = ID_FALSE;
    }

    if ( sIsExist == ID_TRUE )
    {
        if ( sIsKeyRange == ID_TRUE )
        {
            *aPredType = QMO_KEYRANGE;
        }
        else
        {
            *aPredType = QMO_KEYFILTER;
        }
    }
    else
    {
        *aPredType = QMO_FILTER;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::findChildGraph( qtcNode   * aCompareNode,
                         qcDepInfo * aFromDependencies,
                         qmgGraph  * aGraph1,
                         qmgGraph  * aGraph2,
                         qmgGraph ** aLeftColumnGraph,
                         qmgGraph ** aRightColumnGraph )
{
/***********************************************************************
 *
 * Description : �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qcDepInfo sTempDependencies1;
    qcDepInfo sTempDependencies2;

    IDU_FIT_POINT_FATAL( "qmoPred::findChildGraph::__FT__" );

    //--------------------------------------
    // �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
    //--------------------------------------


    IDE_DASSERT( aCompareNode->node.arguments       != NULL );
    IDE_DASSERT( aCompareNode->node.arguments->next != NULL );

    //-------------------------------------------------------------------
    // aCompareNode�� dependencies�� outer column�� ����
    // dependencies�� ������ �� �˻��Ͽ��� �Ѵ�.
    //-------------------------------------------------------------------

    qtc::dependencyAnd( & ((qtcNode *)(aCompareNode->node.arguments))->depInfo,
                        aFromDependencies,
                        & sTempDependencies1 );

    qtc::dependencyAnd( & ((qtcNode *)(aCompareNode->node.arguments->next))->depInfo,
                        aFromDependencies,
                        & sTempDependencies2 );


    //-------------------------------------------------------------------
    // aCompareNode->node.arguments�� dependencies��
    // aGraph1�� dependencies�� ���Եǰų�
    // aGraph2�� dependencies�� ���ԵǾ�� �Ѵ�.
    // ã�� ���� ���� ����� �������� �˸��� ���� ASSERT�� ����Ѵ�.
    //-------------------------------------------------------------------

    if ( ( qtc::dependencyContains( & aGraph1->depInfo,
                                    & sTempDependencies1 )
           == ID_TRUE ) &&
         ( qtc::dependencyContains( & aGraph2->depInfo,
                                    & sTempDependencies2 )
           == ID_TRUE ) )
    {
        *aLeftColumnGraph = aGraph1;
        *aRightColumnGraph = aGraph2;
    }
    else if ( ( qtc::dependencyContains( & aGraph2->depInfo,
                                         & sTempDependencies1 )
                == ID_TRUE ) &&
              ( qtc::dependencyContains( & aGraph1->depInfo,
                                         & sTempDependencies2 )
                == ID_TRUE ) )
    {
        // BUG-24981 joinable predicate�� �񱳿����� �� ���� ��忡 �ش��ϴ�
        // graph(join�����׷���)�� ã�� �������� ���� ����������

        *aLeftColumnGraph = aGraph2;
        *aRightColumnGraph = aGraph1;
    }
    else // �Ѵ� �ƴ� ��쿡�� �� �Լ��� �Ҹ��� �ȵȴ�.
    {
        IDE_RAISE( ERR_INVALID_GRAPH );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPred::findChildGraph",
                                  "Invalid graph" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::classifyTablePredicate( qcStatement      * aStatement,
                                 qmoPredicate     * aPredicate,
                                 qcDepInfo        * aTableDependencies,
                                 qcDepInfo        * aOuterDependencies,
                                 qmoStatistics    * aStatiscalData )
{
/***********************************************************************
 *
 * Description : Base Table�� Predicate�� ���� �з�
 *
 *    predicate ���ġ �������� �� predicate�� ����
 *    indexable predicate�� �Ǵ��ϰ�, columnID�� selectivity�� �����Ѵ�.
 *    (���: selection graph�� myPredicate)
 *
 * Implementation :
 *     1. indexable predicate������ �Ǵ�.
 *     2. predicate�� fixed/variable ���� ����.
 *        [variable predicate �Ǵܱ���]
 *        . join predicate
 *        . host ���� ����
 *        . subquery ����
 *        . prior column�� ���Ե� predicate( ��: prior i1=i1 )
 *     3. columnID ����
 *        (1) indexable predicate �̸�,
 *            . one column : smiColumn.id
 *            . LIST       : QMO_COLUMNID_LIST
 *        (2) non-indexable predicate �̸�,
 *            columnID = QMO_COLUMNID_NON_INDEXABLE
 *     4. predicate�� ���� selectivity ����.
 *
 ***********************************************************************/

    idBool sIsIndexable;

    IDU_FIT_POINT_FATAL( "qmoPred::classifyTablePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // predicate�� ���� indexable �Ǵ�
    //--------------------------------------

    IDE_TEST( isIndexable( aStatement,
                           aPredicate,
                           aTableDependencies,
                           aOuterDependencies,
                           & sIsIndexable )
              != IDE_SUCCESS );

    //--------------------------------------
    // columnID ����
    //--------------------------------------

    IDE_TEST( getColumnID( aStatement,
                           aPredicate->node,
                           sIsIndexable,
                           & aPredicate->id )
              != IDE_SUCCESS );

    //--------------------------------------
    // predicate�� ���� selectivity ����.
    //--------------------------------------


    if ( aStatiscalData != NULL )
    {
        // fix BUG-12515
        // VIEW ������ push selection�ǰ� �� ��,
        // where���� �����ִ� predicate�� ���ؼ��� selectivity�� ������ ����.
        // predicate->mySelectivity = 1�� �ʱ�ȭ �Ǿ� ����.

        if ( ( aPredicate->flag & QMO_PRED_PUSH_REMAIN_MASK )
             == QMO_PRED_PUSH_REMAIN_FALSE )
        {
            // PROJ-2242
            IDE_TEST( qmoSelectivity::setMySelectivity( aStatement,
                                                        aStatiscalData,
                                                        aTableDependencies,
                                                        aPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // Nothing to do.
        // ��������� �����Ƿ� selectivity�� �������� �ʴ´�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::classifyPartTablePredicate( qcStatement        * aStatement,
                                     qmoPredicate       * aPredicate,
                                     qcmPartitionMethod   aPartitionMethod,
                                     qcDepInfo          * aTableDependencies,
                                     qcDepInfo          * aOuterDependencies )
{
/***********************************************************************
 *
 * Description : Base Table�� Predicate�� ���� �з�
 *
 *    predicate ���ġ �������� �� predicate�� ����
 *    indexable predicate�� �Ǵ��ϰ�, columnID�� selectivity�� �����Ѵ�.
 *    (���: selection graph�� myPredicate)
 *
 * Implementation :
 *     1. indexable predicate������ �Ǵ�.\
 *     2. predicate�� fixed/variable ���� ����.
 *        [variable predicate �Ǵܱ���]
 *        . join predicate
 *        . host ���� ����
 *        . subquery ����
 *        . prior column�� ���Ե� predicate( ��: prior i1=i1 )
 *     3. columnID ����
 *        (1) indexable predicate �̸�,
 *            . one column : smiColumn.id
 *            . LIST       : QMO_COLUMNID_LIST
 *        (2) non-indexable predicate �̸�,
 *            columnID = QMO_COLUMNID_NON_INDEXABLE
 *     4. predicate�� ���� selectivity ����.
 *
 ***********************************************************************/

    idBool sIsPartitionPrunable;

    IDU_FIT_POINT_FATAL( "qmoPred::classifyPartTablePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // predicate�� ���� indexable �Ǵ�
    //--------------------------------------

    IDE_TEST( isPartitionPrunable( aStatement,
                                   aPredicate,
                                   aPartitionMethod,
                                   aTableDependencies,
                                   aOuterDependencies,
                                   & sIsPartitionPrunable )
              != IDE_SUCCESS );

    //--------------------------------------
    // columnID ����
    //--------------------------------------

    IDE_TEST( getColumnID( aStatement,
                           aPredicate->node,
                           sIsPartitionPrunable,
                           & aPredicate->id )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::isPartitionPrunable( qcStatement        * aStatement,
                              qmoPredicate       * aPredicate,
                              qcmPartitionMethod   aPartitionMethod,
                              qcDepInfo          * aTableDependencies,
                              qcDepInfo          * aOuterDependencies,
                              idBool             * aIsPartitionPrunable )
{
/***********************************************************************
 *
 * Description : predicate�� indexable ���� �Ǵ�
 *
 *     <indexable predicate�� ����>
 *
 *     ����1. indexable operator �̾�� �Ѵ�.
 *            system level operator �Ӹ� �ƴ϶�,
 *            user level operator(quantify�񱳿�����)�� ���Եȴ�.
 *
 *     ����2. column�� �־�� �Ѵ�.
 *            ��) i1=1(O), i1=i2(O), i1=i1+1(O), i1+1=1(O), 1=1(X),
 *                (i1,i2,i3)=(i1,i2,i3)(O), (i1,i2,i3)=(1,1,1)(O),
 *                (i1,i2,1 )=( 1, 1, 1)(X), (1,subquery)=(1,1)(X)
 *
 *     ����3. column�� ������ ����� �Ѵ�.
 *
 *            ��) i1=1(O), i1=i2(O), i1=i1+1(O), i1+1=1(X)
 *
 *     ����4. column�� ���ʿ��� �����ؾ� �Ѵ�.
 *            ��) i1=1(O), i1=i1+1(X), i1=i2(X)
 *
 *     ����5. column�� conversion�� �߻����� �ʾƾ� �Ѵ�.
 *            ��) i1(integer)=smallint'1'(O), i1(integer)=3.5(X)
 *            sameGroupCompare�� ���� �� ������ ��ȭ��.
 *
 *     ����6. value�� ���� ����üũ
 *            6-1. subquery�� ������ �ȵȴ�.
 *                 ��) a1 = (select i1 from t2 where i1=1)(X)
 *                     a1 = (select sum(i1) from t2)(X)
 *                     a1 = (select i1 from t2 where i1=al)(X)
 *                     a1 = (select sum(i1) from t2 where i1=a1)(X)
 *            6-2. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
 *                 ��) i1 like 'a%'(O) , i1 like '\_a%' escape'\'(O)
 *                     i1 like '%a%'(X), i1 like '_bc'(X)
 *            6-3. host ������ ���� ���� ������ �ȵȴ�.
 *                 ��) i1=?(X), i1=?+1(X)
 *
 *     ����7. OR��� ������ ������ �÷��� �ִ� ����̾�� �Ѵ�.
 *            ��, subquery �� ���� ���� ���ܵȴ�.
 *            ��) i1=1 OR i1=2(O), (i1,i2)=(1,1) OR (i1,i2)=(2,2)(O),
 *                i1=1 OR i2=2(X),
 *                i1 in (subquery) OR i1 in (subquery)(X),
 *                i1=1 OR i1 in (subquery) (X)
 *                (i1=1 and i2=1) or i1=( subquery ) (X)
 *
 *
 *     < ���� ������������� partition prunable predicate �� �Ǵܹ��� >
 *
 *     (1) parsing & validation ���������� �Ǵܹ���
 *         ����1, ����2, ����3, �� ������ ������ �����ϸ�
 *         mtcNode.lflag�� MTC_NODE_INDEX_USABLE�� ������.
 *
 *     (2) graph �������������� �Ǵܹ���
 *         ����4, ����5, ����6, ����7
 *
 *     (3) plan tree �������������� �Ǵܹ���
 *         partition filter�� ������.
 *
 *     (4) execution ���������� �Ǵܹ���
 *         binding�� ���� ��, �Ǵ� doItFirst���� partition filter��
 *         ���� �����Ѵ�.
 *
 * Implementation : �� �Լ��� graph �������������� �Ǵܹ����� �˻���.
 *
 *     1. ����1,2,3�� �Ǵ�
 *        mtcNode.lflag�� MTC_NODE_INDEX_USABLE ������ �˻�
 *
 *     2. ����4�� �Ǵ�
 *        (1) operand�� dependency�� �ߺ����� �ʴ����� �˻��Ѵ�.
 *            (�� �˻�� �÷��� ���ʿ��� �����Ѵٴ� ���� ������)
 *
 *            dependency �ߺ� �Ǵܹ����,
 *            ( ( �񱳿����������� �ΰ� ����� dependencies�� AND����)
 *              & FROM�� �ش� table�� dependency ) != 0
 *
 *        (2) (1)�� �����Ǹ�,
 *            �÷��� ��� �ش� table�� �÷����� �����Ǿ������� �˻�.
 *
 *     3. ����5�� �Ǵ�
 *        (1) column
 *            value�ʿ� host������ ���µ���,
 *            column�� conversion�� �߻��ߴ����� �˻�.
 *
 *        (2) LIST
 *            value�� LIST ���� ������ ��� �����ؼ�, host������ �ƴϸ鼭,
 *            value�� leftConversion�� �����ϴ��� �˻�.
 *
 *     4. ����6�� �Ǵ�
 *        (1) 6-1���� : QTC_NODE_SUBQUERY_EXIST
 *        (2) 6-2���� : ?
 *        (3) 6-3���� :
 *            value node�� tuple�� MTC_TUPLE_TYPE_CONSTANT������ �˻�.
 *
 *     5. ����7�� �Ǵ�
 *        (1) OR��� ������ 1���� �񱳿����ڰ� ������ ���,
 *            subquery�� ���� �ȵ�.
 *            partition prunable predicate���� �Ǵ�
 *        (2) OR��� ������ 2���̻��� �񱳿����ڰ� ������ ���,
 *            . subquery�� �������� �ʾƾ� �Ѵ�.
 *            . ���� �񱳿����ڵ��� ��� partition prunable�̾�� �Ѵ�.
 *            . ���� ������ columID�� ��� ���ƾ� �Ѵ�.
 *              (��, �÷��� LIST�� ���� non���� ó��)
 *
 ***********************************************************************/
    UInt     sColumnID;
    UInt     sFirstColumnID;
    idBool   sIsFirstNode             = ID_TRUE;
    idBool   sIsPartitionPrunablePred = ID_FALSE;
    qtcNode *sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isPartitionPrunable::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aIsPartitionPrunable != NULL );

    sNode = aPredicate->node;

    if ( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
         == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        //--------------------------------------
        // CNF �� ����,
        // ���ڷ� �Ѿ�� predicate�� �ֻ��� ���� OR����̸�,
        // OR ��� ������ �������� �񱳿����ڰ� �� �� �ִ�.
        // OR ��� ������ �񱳿����ڰ� �ϳ��϶��� �������϶���
        // ���ǰ˻簡 Ʋ���Ƿ�, �̸� �����Ͽ� ó���Ѵ�.
        //--------------------------------------

        // sNode�� �񱳿����� ���
        sNode = (qtcNode *)(sNode->node.arguments);

        if ( aPredicate->node->node.arguments->next == NULL )
        {
            // 1. OR ��� ������ �񱳿����ڰ� �ϳ��� ���� ���,
            // subquery�� �����ϸ� �ȵȴ�.

            if ( ( aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                sIsPartitionPrunablePred = ID_FALSE;
            }
            else
            {

                IDE_TEST( isPartitionPrunableOnePred( aStatement,
                                                      sNode,
                                                      aPartitionMethod,
                                                      aTableDependencies,
                                                      aOuterDependencies,
                                                      & sIsPartitionPrunablePred )
                          != IDE_SUCCESS );
            }

        }
        else
        {
            // 2. OR ��� ������ �񱳿����ڰ� ������ ���� ���,
            //   (1) subquery�� �������� �ʾƾ� �Ѵ�.
            //   (2) �񱳿����ڰ� ��� indexable predicate�̾�� �Ѵ�.
            //   (3) �񱳿����� ����� columnID�� ��� �����ؾ� �Ѵ�.
            //       (��, column�� LIST�� ���� �����Ѵ�.)

            // subquery�� �������� �ʾƾ� �Ѵ�.
            if ( ( aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
                 == QTC_NODE_SUBQUERY_EXIST )
            {
                sIsPartitionPrunablePred = ID_FALSE;
            }
            else
            {
                while ( sNode != NULL )
                {
                    // partition prunable predicate���� �˻�
                    IDE_TEST( isPartitionPrunableOnePred( aStatement,
                                                          sNode,
                                                          aPartitionMethod,
                                                          aTableDependencies,
                                                          aOuterDependencies,
                                                          & sIsPartitionPrunablePred )
                              != IDE_SUCCESS );

                    if ( sIsPartitionPrunablePred == ID_TRUE )
                    {
                        // columnID�� ��´�.
                        IDE_TEST( getColumnID( aStatement,
                                               sNode,
                                               ID_TRUE,
                                               & sColumnID )
                                  != IDE_SUCCESS );

                        if ( sIsFirstNode == ID_TRUE )
                        {
                            // OR ��� ������ ù��° �񱳿����� ó����,
                            // ������ columnID �񱳸� ����,
                            // sFirstColumnID�� columnID�� ����.
                            sFirstColumnID = sColumnID;
                            sIsFirstNode   = ID_FALSE;
                        }
                        else
                        {
                            // Nothing To Do
                        }

                        // column�� LIST�� �ƴ� one column���� �����Ǿ� �ְ�,
                        // ù��° �񱳿������� columnID�� �������� �˻�.
                        if ( ( sColumnID != QMO_COLUMNID_LIST )
                             && ( sColumnID == sFirstColumnID ) )
                        {
                            // Nothing To Do
                        }
                        else
                        {
                            sIsPartitionPrunablePred = ID_FALSE;
                        }
                    }
                    else
                    {
                        sIsPartitionPrunablePred = ID_FALSE;
                    }

                    if ( sIsPartitionPrunablePred == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        break;
                    }

                    sNode = (qtcNode *)(sNode->node.next);
                }

            }

        }
    }
    else
    {
        IDE_TEST( isPartitionPrunableOnePred( aStatement,
                                              sNode,
                                              aPartitionMethod,
                                              aTableDependencies,
                                              aOuterDependencies,
                                              & sIsPartitionPrunablePred )
                  != IDE_SUCCESS );
    }

    if ( sIsPartitionPrunablePred == ID_TRUE )
    {
        *aIsPartitionPrunable = ID_TRUE;
    }
    else
    {
        *aIsPartitionPrunable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::isPartitionPrunableOnePred( qcStatement        * aStatement,
                                     qtcNode            * aNode,
                                     qcmPartitionMethod   aPartitionMethod,
                                     qcDepInfo          * aTableDependencies,
                                     qcDepInfo          * aOuterDependencies,
                                     idBool             * aIsPartitionPrunable )
{
/***********************************************************************
 *
 * Description : �񱳿����� ������ partition prunable ���θ� �Ǵ��Ѵ�.
 *
 * Implementation : qmoPred::isPartitionPrunable()�� �ּ� ����.
 *
 ************************************************************************/

    idBool    sIsTemp = ID_TRUE;
    idBool    sIsPartitionPrunablePred = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isPartitionPrunableOnePred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsPartitionPrunable != NULL );

    //--------------------------------------
    // partition prunable �Ǵ�
    //--------------------------------------

    sCompareNode = aNode;

    while ( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        if ( ( sCompareNode->node.lflag & MTC_NODE_INDEX_MASK )
             == MTC_NODE_INDEX_USABLE )
        {
            if ( ( sCompareNode->node.module == &mtfEqual )    ||
                 ( sCompareNode->node.module == &mtfEqualAll ) ||
                 ( sCompareNode->node.module == &mtfEqualAny ) ||
                 ( sCompareNode->node.module == &mtfIsNull ) )
            {
                // �񱳿����ڰ� equal, IN, isnull �� ���
                // hash, range, list partition method��� ����.
                // Nothing To Do.

            }
            else
            {
                if ( ( aPartitionMethod == QCM_PARTITION_METHOD_HASH ) ||
                     ( aPartitionMethod == QCM_PARTITION_METHOD_LIST ) ||
                     ( aPartitionMethod == QCM_PARTITION_METHOD_RANGE_USING_HASH ) )
                {
                    // list, hash�� equality����� isnull���� ����� ��� �Ұ���.
                    sIsPartitionPrunablePred = ID_FALSE;
                }
                else
                {
                    if ( ( sCompareNode->node.module == &mtfNotEqual )    ||
                         ( sCompareNode->node.module == &mtfNotEqualAny ) ||
                         ( sCompareNode->node.module == &mtfNotEqualAll ) )
                    {
                        // �񱳿����ڰ� equal, IN �� �ƴ� ���
                        // �÷����� LIST�̸�, non-indexable�� �з�
                        if ( ( sCompareNode->node.arguments->lflag &
                               MTC_NODE_INDEX_MASK ) == MTC_NODE_INDEX_USABLE )
                        {
                            sNode = (qtcNode *)(sCompareNode->node.arguments);
                        }
                        else
                        {
                            sNode = (qtcNode *)(sCompareNode->node.arguments->next);
                        }

                        if ( sNode->node.module == &mtfList )
                        {
                            sIsPartitionPrunablePred = ID_FALSE;
                        }
                        else
                        {
                            // Nothing To Do
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }

        }
        else
        {
            sIsPartitionPrunablePred = ID_FALSE;
        }

        if ( sIsPartitionPrunablePred == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        //--------------------------------------
        // ���� 4�� �Ǵ�
        //   : column�� ���ʿ��� �����ؾ� �Ѵ�.
        //   (1) �� operand�� dependency�� �ߺ����� �ʴ����� �˻��Ѵ�.
        //   (2) column�� ��� �ش� table�� column���� �����Ǿ������� �˻�.
        //--------------------------------------
        IDE_TEST( isExistColumnOneSide( aStatement,
                                        sCompareNode,
                                        aTableDependencies,
                                        & sIsPartitionPrunablePred )
                  != IDE_SUCCESS );

        if ( sIsPartitionPrunablePred == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }


        if ( ( sCompareNode->node.module == &mtfIsNull ) ||
             ( sCompareNode->node.module == &mtfIsNotNull ) )
        {
            // IS NULL, IS NOT NULL �� ���,
            // ��: i1 is null, i1 is not null
            // �� ����, value node�� ������ �ʱ� ������,
            // (1) column�� conversion�� �߻����� �ʰ�,
            // (2) value�� ���� ����üũ�� ���� �ʾƵ� �ȴ�.

            // fix BUG-15773
            // R-tree �ε����� ������ ���ڵ忡 ���ؼ�
            // is null, is not null �����ڴ� full scan�ؾ� �Ѵ�.
            if ( QC_SHARED_TMPLATE(aStatement)->tmplate.
                 rows[sCompareNode->node.arguments->table].
                 columns[sCompareNode->node.arguments->column].type.dataTypeId
                 ==  MTD_GEOMETRY_ID )
            {
                sIsPartitionPrunablePred = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            //--------------------------------------
            // ���� 5�� �Ǵ�
            //   : column�� value�� ���ϰ迭�� ���ϴ� data type������ �˻�
            //--------------------------------------

            IDE_TEST( checkSameGroupType( aStatement,
                                          sCompareNode,
                                          & sIsPartitionPrunablePred )
                      != IDE_SUCCESS );

            if ( sIsPartitionPrunablePred == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }

            //--------------------------------------
            // ���� 6�� �Ǵ�
            //   : value�� ���� ����üũ
            //   (1) host������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
            //   (2) subquery�� ���� ���� �ȵ�
            //   (3) deterministic function�� ���� ��쵵 �� ��( BUG-39823 ).
            //   (4) LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
            //--------------------------------------

            IDE_TEST( isPartitionPrunableValue( aStatement,
                                                sCompareNode,
                                                aOuterDependencies,
                                                & sIsPartitionPrunablePred )
                      != IDE_SUCCESS );

            if ( sIsPartitionPrunablePred == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }
        }


    }

    if ( sIsPartitionPrunablePred == ID_TRUE )
    {
        *aIsPartitionPrunable = ID_TRUE;
    }
    else
    {
        *aIsPartitionPrunable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::isPartitionPrunableValue( qcStatement * aStatement,
                                   qtcNode     * aNode,
                                   qcDepInfo   * aOuterDependencies,
                                   idBool      * aIsPartitionPrunable )
{
/***********************************************************************
 *
 * Description : indexable Predicate �Ǵܽ�,
 *               value�� ���� ���� �˻�( ���� 6�� �˻�).
 *
 * Implementation :
 *
 *     1. host������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
 *     2. subquery�� ���� ��� �ȵ�.
 *     3. deterministion function�� ��쵵 �� ��( BUG-39823 ).
 *     4. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

    idBool     sIsTemp = ID_TRUE;
    idBool     sIsPartitionPrunableValue = ID_TRUE;
    qtcNode  * sCompareNode;
    qtcNode  * sValueNode;
    qtcNode  * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isPartitionPrunableValue::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsPartitionPrunable != NULL );

    //--------------------------------------
    // ����6�� �Ǵ� : value�� ���� üũ
    //--------------------------------------

    sCompareNode = aNode;

    if ( sCompareNode->indexArgument == 0 )
    {
        sValueNode = (qtcNode *)(sCompareNode->node.arguments->next);
    }
    else
    {
        sValueNode = (qtcNode *)(sCompareNode->node.arguments);
    }

    // PROJ-1492
    // ȣ��Ʈ ������ Ÿ���� �����Ǵ��� �� ���� data binding������
    // �� �� ����.
    if ( MTC_NODE_IS_DEFINED_VALUE( & sCompareNode->node ) == ID_FALSE )
    {
        // 1. host ������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
        // Nothing To Do
    }
    else
    {
        while ( sIsTemp == ID_TRUE )
        {
            sIsTemp = ID_FALSE;

            for ( sNode  = sValueNode;
                  sNode != NULL;
                  sNode  = (qtcNode *)(sNode->node.next) )
            {
                // 2. subquery�� ���� ��� partition prunable���� �ʴ�.
                if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_SUBQUERY )
                {
                    sIsPartitionPrunableValue = ID_FALSE;
                    break;
                }
                else
                {
                    // subquery node�� �ƴ� ���
                    // Nothing To Do
                }

                /* BUG-39823
                   3. deterministic function node�� ���� ���,
                   partition prunable ���� �ʴ�. */
                if ( ( ( sNode->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                       == QTC_NODE_PROC_FUNCTION_TRUE ) &&
                     ( ( sNode->lflag & QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK )
                       == QTC_NODE_PROC_FUNC_DETERMINISTIC_TRUE ) )
                {
                    sIsPartitionPrunableValue = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            } // end of for()

            // 4. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
            if ( sCompareNode->node.module == &mtfLike )
            {
                // �Ϲݹ��ڷ� �����ϴ� ��������� �˻��Ѵ�.
                if ( (QC_SHARED_TMPLATE( aStatement )->
                      tmplate.rows[sValueNode->node.table].lflag
                      & MTC_TUPLE_TYPE_MASK )
                     == MTC_TUPLE_TYPE_CONSTANT)
                {
                    IDE_TEST( isIndexableLIKE( aStatement,
                                               sCompareNode,
                                               & sIsPartitionPrunableValue )
                              != IDE_SUCCESS );

                    if ( sIsPartitionPrunableValue == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    // BUG-25594
                    // dynamic constant expression�̸� indexable�� �Ǵ��Ѵ�.
                    if ( qtc::isConstNode4LikePattern( aStatement,
                                                       sValueNode,
                                                       aOuterDependencies )
                         == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsPartitionPrunableValue = ID_FALSE;                    
                        break;
                    }
                }
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    if ( sIsPartitionPrunableValue == ID_TRUE )
    {
        *aIsPartitionPrunable = ID_TRUE;
    }
    else
    {
        *aIsPartitionPrunable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::isIndexableUnitPred( qcStatement * aStatement,
                              qtcNode     * aNode,
                              qcDepInfo   * aTableDependencies,
                              qcDepInfo   * aOuterDependencies,
                              idBool      * aIsIndexable )
{
/***********************************************************************
 *
 * Description : �񱳿����� ������ indexable ���θ� �Ǵ��Ѵ�.
 *
 * Implementation : qmoPred::isIndexable()�� �ּ� ����.
 *
 ************************************************************************/

    idBool    sIsTemp = ID_TRUE;
    idBool    sIsIndexableUnitPred = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isIndexableUnitPred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsIndexable != NULL );

    //--------------------------------------
    // indexable �Ǵ�
    //--------------------------------------

    sCompareNode = aNode;

    while ( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        //--------------------------------------
        // ���� 1, 2, 3�� �Ǵ�
        //  (1) ���� 1 : indexable operator �̾�� �Ѵ�.
        //  (2) ���� 2 : column�� �־�� �Ѵ�.
        //  (3) ���� 3 : column�� ������ ����� �Ѵ�.
        //--------------------------------------

        if ( ( sCompareNode->node.lflag & MTC_NODE_INDEX_MASK )
             == MTC_NODE_INDEX_USABLE )
        {
            // LIST �÷��� ����,
            // equal(=)�� IN �����ڸ� indexable operator�� �Ǵ��Ѵ�.
            // �񱳿����ڰ� equal�� IN�� �ƴ϶��,
            // �񱳿����� ���� �÷����� LIST ���� �˻�.
            // LIST �÷��� �� �� �ִ� �񱳿����ڴ� ������ ����.
            // [ =, !=, =ANY(IN), !=ANY, =ALL, !=ALL(NOT IN) ]
            if ( ( sCompareNode->node.module == &mtfEqual )    ||
                 ( sCompareNode->node.module == &mtfEqualAll ) ||
                 ( sCompareNode->node.module == &mtfEqualAny ) )
            {
                // �񱳿����ڰ� equal, IN �� ���
                // Nothing To Do
            }
            else if ( ( sCompareNode->node.module == &mtfNotEqual )    ||
                      ( sCompareNode->node.module == &mtfNotEqualAny ) ||
                      ( sCompareNode->node.module == &mtfNotEqualAll ) )
            {
                // �񱳿����ڰ� equal, IN �� �ƴ� ���
                // �÷����� LIST�̸�, non-indexable�� �з�

                if ( ( sCompareNode->node.arguments->lflag &
                       MTC_NODE_INDEX_MASK )
                     == MTC_NODE_INDEX_USABLE )
                {
                    sNode = (qtcNode *)(sCompareNode->node.arguments);
                }
                else
                {
                    sNode = (qtcNode *)(sCompareNode->node.arguments->next);
                }

                if ( sNode->node.module == &mtfList )
                {
                    sIsIndexableUnitPred = ID_FALSE;
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                /* Nothing to Do */
                // BETWEEN , NOT BETWEEN , IS NULL, IS NOT NULL
                // NVL_EQUAL, NOT NVL_EQUL �� �����ڰ� �ü� �ִ�.
            }
        }
        else
        {
            sIsIndexableUnitPred = ID_FALSE;
        }

        if ( sIsIndexableUnitPred == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        //--------------------------------------
        // ���� 4�� �Ǵ�
        //   : column�� ���ʿ��� �����ؾ� �Ѵ�.
        //   (1) �� operand�� dependency�� �ߺ����� �ʴ����� �˻��Ѵ�.
        //   (2) column�� ��� �ش� table�� column���� �����Ǿ������� �˻�.
        //--------------------------------------

        IDE_TEST( isExistColumnOneSide( aStatement,
                                        sCompareNode,
                                        aTableDependencies,
                                        & sIsIndexableUnitPred )
                  != IDE_SUCCESS );

        if( sIsIndexableUnitPred == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        if( ( sCompareNode->node.module == &mtfIsNull ) ||
            ( sCompareNode->node.module == &mtfIsNotNull ) )
        {
            // IS NULL, IS NOT NULL �� ���,
            // ��: i1 is null, i1 is not null
            // �� ����, value node�� ������ �ʱ� ������,
            // (1) column�� conversion�� �߻����� �ʰ�,
            // (2) value�� ���� ����üũ�� ���� �ʾƵ� �ȴ�.

            // fix BUG-15773
            // R-tree �ε����� ������ ���ڵ忡 ���ؼ�
            // is null, is not null �����ڴ� full scan�ؾ� �Ѵ�.
            if ( QC_SHARED_TMPLATE( aStatement )->tmplate.
                 rows[sCompareNode->node.arguments->table].
                 columns[sCompareNode->node.arguments->column].type.dataTypeId
                 == MTD_GEOMETRY_ID )
            {
                sIsIndexableUnitPred = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
             if ( sCompareNode->node.module == &mtfInlist )
            {
                /* BUG-32622 inlist operator
                   INLIST �� ���
                   ��: inlist(i1, 'aa,bb,cc')
                   �� ���� value node�� column�� �ٸ� type������
                   column�� index�� �ִ� ��� ������ index�� �¿�⶧����
                   �׻� indexable�ϴٰ� �Ǵ��Ѵ�.
                   (index�� ���ϴ� ���� ������ ��ȯ�Ѵ�.)
                */
                sNode = (qtcNode *)(sCompareNode->node.arguments);
                sNode->lflag &= ~QTC_NODE_CHECK_SAMEGROUP_MASK;
                sNode->lflag |= QTC_NODE_CHECK_SAMEGROUP_TRUE;
            }
            else
            {
                //--------------------------------------
                // ���� 5�� �Ǵ�
                //   : column�� value�� ���ϰ迭�� ���ϴ� data type������ �˻�
                //--------------------------------------

                IDE_TEST( checkSameGroupType( aStatement,
                                              sCompareNode,
                                              & sIsIndexableUnitPred )
                          != IDE_SUCCESS );

                if ( sIsIndexableUnitPred == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    break;
                }

                //--------------------------------------
                // ���� 6�� �Ǵ�
                //   : value�� ���� ����üũ
                //   (1) host������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
                //   (2) subquery�� ���� ���, subquery type�� A, N���̾�� �Ѵ�.
                //   (3) LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
                //--------------------------------------

                IDE_TEST( isIndexableValue( aStatement,
                                            sCompareNode,
                                            aOuterDependencies,
                                            & sIsIndexableUnitPred )
                          != IDE_SUCCESS );

                if ( sIsIndexableUnitPred == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    break;
                }
            }
        }
    }

    *aIsIndexable = sIsIndexableUnitPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isExistColumnOneSide( qcStatement * aStatement,
                               qtcNode     * aNode,
                               qcDepInfo   * aTableDependencies,
                               idBool      * aIsIndexable )
{
/***********************************************************************
 *
 * Description : indexable predicate �Ǵܽ�,
 *               �÷��� ���ʿ��� �����ϴ����� �Ǵ�(����4�� �Ǵ�).
 *
 * Implementation :
 *
 *    1. �� operand�� dependency�� �ߺ����� �ʾƾ� �Ѵ�.
 *        dependencies �ߺ� =
 *               ( ( �񱳿����������� �ΰ� ����� dependencies�� AND���� )
 *                 & FROM�� �ش� table�� dependency ) != 0 )
 *
 *    2. column�� ��� �ش� table�� column���� �����Ǿ�� �Ѵ�.
 *       column�� ���ʿ��� �����Ѵ� �ϴ���,
 *       LIST ������ outer column, ���, �񱳿����ڰ� ���� ������ �� �ְ�,
 *       one column�� ��쵵 �÷��� �ƴ� �񱳿������� �� �ִ�.
 *       (1) LIST : outer column�� �������� �ʾƾ� �Ѵ�.
 *                  ��� column�̾�� �Ѵ�.
 *       (2) one column : ��� column�̾�� �Ѵ�.
 *
 ***********************************************************************/

    qcDepInfo sAndDependencies;
    qcDepInfo sResultDependencies;
    UInt      sIndexArgument;
    idBool    sIsTemp = ID_TRUE;
    idBool    sIsExistColumnOneSide = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sColumnNode;
    qtcNode * sCurNode = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::isExistColumnOneSide::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsIndexable != NULL );

    //--------------------------------------
    // ����4�� �Ǵ� : �÷��� ���ʿ��� �����ؾ� �Ѵ�.
    //--------------------------------------

    while ( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;
        sCompareNode = aNode;

        //----------------------------
        // 1. �� operand�� dependency�� �ߺ����� �ʾƾ� �Ѵ�.
        //----------------------------

        // 1) IS NULL, IS NOT NULL �� ���
        //    �񱳿����� ������ �ϳ��� ��常 �����Ѵ�.
        //    ����, dependency �ߺ� �˻�� ���� �ʾƵ� ��.
        // 2) BETWEEN, NOT BETWEEN �� ���
        //    �񱳿����� ��� ������ ������ ��尡 �����Ѵ�.
        //     [�񱳿����ڳ��]   i1 between 1 and 2
        //             |
        //            \ /
        //        [�÷����] -> [value���] -> [value���]
        //      ����, �߰��� ������ �ϳ��� value ��忡 ����
        //      dependency �ߺ��˻絵 �����ؾ� �Ѵ�.
        //
        //    NVL_EQUAL, NOT NVL_EQUAL
        //    �� ������ ��� ������ ������ ��尡 �����Ѵ�.
        //    [�÷����] -> [value���] -> [value���]
        //     ������ ù��° value���� ���� ����.
        //
        // 3) ������ �񱳿����ڵ��� �񱳿����� ���� �ΰ� ��忡 ���� �˻�.
        //

        for ( sCurNode  = (qtcNode *)(sCompareNode->node.arguments->next);
              sCurNode != NULL;
              sCurNode  = (qtcNode *)(sCurNode->node.next) )
        {
            // (1). �񱳿����� ���� �ΰ� ����� dependencies�� AND ����
            qtc::dependencyAnd(
                & ((qtcNode*)(sCompareNode->node.arguments))->depInfo,
                & sCurNode->depInfo,
                & sAndDependencies );

            // (2). (1)�� ����� FROM�� �ش� table�� dependencies�� AND ����
            qtc::dependencyAnd( & sAndDependencies,
                                aTableDependencies,
                                & sResultDependencies );

            // (3). (2)�� ����� 0�� �ƴϸ�, dependency �ߺ�����
            // �񱳿����� ���� �ΰ� ��� ��� ���� ���̺��� �÷��� �����Ѵ�.
            if ( qtc::dependencyEqual( & sResultDependencies,
                                       & qtc::zeroDependencies ) == ID_TRUE )
            {
                // �÷��� ���ʿ��� �����ϴ� ���

                // between�� not between �񱳿����ڴ�
                // ������ �ϳ��� value node�� ���� dependency �ߺ��˻����.
                if ( sCurNode->node.next != NULL )
                {
                    // To Fix PR-8728
                    // ������ ����� ����� �˻��Ͽ��� ��.
                    if ( ( sCompareNode->node.module == &mtfBetween )    ||
                         ( sCompareNode->node.module == &mtfNotBetween ) ||
                         ( sCompareNode->node.module == &mtfNvlEqual )   ||
                         ( sCompareNode->node.module == &mtfNvlNotEqual ) )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
            else
            {
                // NVL_EQUAL (expr1, expr2, expr3)
                // expr2�� ������ ����.
                if ( sCurNode->node.next != NULL )
                {
                    if ( ( sCompareNode->node.module == &mtfNvlEqual ) ||
                         ( sCompareNode->node.module == &mtfNvlNotEqual ) )
                    {
                        /* Nothing to do */
                    }
                    else
                    {
                        sIsExistColumnOneSide = ID_FALSE;
                        break;
                    }
                }
                else
                {
                    sIsExistColumnOneSide = ID_FALSE;
                    break;
                }
            }
        }

        if ( sIsExistColumnOneSide == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        //----------------------------
        // 2. column�� ���� �߰� ��������,
        //    �ش� table�� �÷����� �����ϴ����� �˻�.
        //----------------------------

        // (1). columnNode�� ���Ѵ�.
        //      columnNode�� �ش� table�� dependencies�� ������ ���
        //      ( LIST�� ���, outer Column���簡 ���⼭ �˻��.)

        // ���� �ִ� ��å�� -------------------------------------------------------
        // indexable operator ��,
        // =, !=, <, >, <=, >= ��
        // �÷����� value����� ��ġ�� �ٲ� �� �ִ�.
        // ����, �� ��츸 �񱳿����� ���� �� ��带 ��� �˻��ؼ�
        // �÷� ��带 ���Ѵ�.
        //  ��) i1 = 1 , 1 = i1 �� ���� ��� ����.
        // �� ���� �񱳿����ڴ� �÷���尡 �񱳿������� argument����
        // �� �� �����Ƿ�, �񱳿������� argument�� �˻�.
        //  ��) i1 between 1 and 2, i1 in ( 1, 2 )
        // AST�� �߰� �Ǹ鼭 ------------------------------------------------------
        // PR-15291 "Geometry ������ �� �Ķ������ ������ ���� ���� Ʋ�� �����ڰ� �ֽ��ϴ�"
        // Geometry ��ü�� �÷����� value����� ��ġ�� �ٲ� �� �־�� �ϹǷ�
        // MTC_NODE_INDEX_ARGUMENT_BOTH �÷��׸� �̿��� �ε��� ������ �����Ѵ�.
        if ( qtc::dependencyEqual(
                 & ((qtcNode *)(sCompareNode->node.arguments))->depInfo,
                 aTableDependencies ) == ID_TRUE )
        {
            sIndexArgument = 0;
            sColumnNode = (qtcNode *)(sCompareNode->node.arguments);
        }
        else
        {
            // To Fix PR-15291
            if ( ( sCompareNode->node.module->lflag &
                   MTC_NODE_INDEX_ARGUMENT_MASK )
                 == MTC_NODE_INDEX_ARGUMENT_BOTH )
            {
                if ( qtc::dependencyEqual(
                         & ((qtcNode *)(sCompareNode->node.arguments->next))->depInfo,
                         aTableDependencies ) == ID_TRUE )
                {
                    sIndexArgument = 1;
                    sColumnNode =
                        (qtcNode*)(sCompareNode->node.arguments->next);
                }
                else
                {
                    sIsExistColumnOneSide = ID_FALSE;
                }
            }
            else
            {
                sIsExistColumnOneSide = ID_FALSE;
            }
        }

        if ( sIsExistColumnOneSide == ID_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            break;
        }

        // (2) columnNode�� �ش� table�� dependencies�� �����ϴ� �ϴ���,
        //     .LIST       : ������ ����� �񱳿����ڰ� ���� �� �ְ�,
        //     .one column : ������ �񱳿����ڰ� ���� �� �����Ƿ�,
        //     ��� �÷����� �����Ǿ������� �˻�.
        if ( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
             == MTC_NODE_OPERATOR_LIST )
        {
            // LIST�� ���

            sCurNode = (qtcNode *)(sColumnNode->node.arguments);

            while ( sCurNode != NULL )
            {
                if ( QTC_IS_COLUMN( aStatement, sCurNode ) == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsExistColumnOneSide = ID_FALSE;
                    break;
                }
                sCurNode = (qtcNode *)(sCurNode->node.next);
            }
        }
        else
        {
            // one column�� ���

            if ( QTC_IS_COLUMN( aStatement, sColumnNode ) == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                sIsExistColumnOneSide = ID_FALSE;
            }
        }
    } // end of while()

    //--------------------------------------
    // indexArgument�� ����.
    //--------------------------------------

    if ( sIsExistColumnOneSide == ID_TRUE )
    {
        sCompareNode->indexArgument = sIndexArgument;
        *aIsIndexable = ID_TRUE;
    }
    else
    {
        *aIsIndexable = ID_FALSE;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::checkSameGroupType( qcStatement * aStatement,
                             qtcNode     * aNode,
                             idBool      * aIsIndexable )
{
/***********************************************************************
 *
 * Description : indexable predicate �Ǵܽ�,
 *               column�� conversion�� �߻��ߴ����� �˻�(���� 5�� �˻�).
 *
 * Implementation :
 *
 *    column�� value�� ������ �迭�� ���ϴ� ������Ÿ�������� �Ǵ��Ѵ�.
 *
 * ��: select * from t1
 *   where (i1, i2) = ( (select i1 from t2), (select i2 from t2) );
 *
 *   [ = ]
 *     |
 *   [LIST]--------[LIST]  <------------------- sCurValue
 *     |             |
 *   [I1]--[I2]    [SUBQUERY,INDIRECT]--[SUBQUERY,INDIRECT] <--sValue
 *     |             |                       |
 *    sCurColumn   [t2.i1]              [t2.i2]
 *
 * ��: select * from t1
 *     where (i1, i2) = ((select max(i1) from t2), (select max(i2) from t2) );
 *
 *   [ = ]
 *     |
 *   [LIST]--------[LIST]  <------------------- sCurValue
 *     |             |
 *   [I1]--[I2]    [SUBQUERY,INDIRECT]--[SUBQUERY,INDIRECT] <--sValue
 *     |             |                       |
 *    sCurColumn   [max(i1)]              [max(i2)]
 *                   |                       |
 *                  [t2.i1]               [t2.i2]
 *
 * ��: where ( i1, i2 ) in ( (select i1, i2 from t1 where i1=1),
 *                           (select i1, i2 from t2 where i1=2) );
 *
 *   [ IN ]
 *     |
 *   [LIST]--------[LIST]  |------------------- sCurValue
 *     |             |     |
 *   [I1]--[I2]    [SUBQUERY]-------------[SUBQUERY]
 *     |             |                       |
 *    sCurColumn   [t1.i1]      [t2.i1]    [t2.i1]   [t2.i2] <--sValue
 *
 * ��: where ( i1, i2 ) in ( (select min(i1), min(i2) from t1),
 *                           (select max(i1), max(i2) from t2) );
 *
 *   [ IN ]
 *     |
 *   [LIST]--------[LIST]  |------------------- sCurValue
 *     |             |     |
 *   [I1]--[I2]    [SUBQUERY]-------------[SUBQUERY]
 *     |             |                       |
 *     |           [min(i1)]--[min(i2)]   [min(i1)]--[min(i2)] <---sValue
 *    sCurColumn     |             |         |          |
 *                 [t1.i1]      [t2.i1]    [t2.i1]   [t2.i2]
 *
 * ��:  where ( i1, i2 ) in ( select (select min(i1) from t1),
 *                                   (select min(i1) from t2)
 *                            from t2 );
 *   [ IN ]
 *     |
 *   [LIST]----------[LIST] <------------------- sCurValue
 *     |               |
 *   [I1]--[I2]      [INDIRECT]---[INDIRECT] <--- sValue
 *     |               |             |
 *     |             [min(i1)]    [min(i1)]
 *    sCurColumn       |             |
 *                   [t1.i1]      [t2.i1]
 *
 * ��: where ( i1, i2 ) in ( select (select (select min(i1) from t1) from t1),
 *                                  (select (select min(i1) from t2) from t2)
 *                           from t2 );
 *
 *   [ IN ]
 *     |
 *   [LIST]----------[LIST] <------------------- sCurValue
 *     |               |
 *   [I1]--[I2]      [INDIRECT]---[INDIRECT] <--- sValue
 *     |               |             |
 *     |             [INDIRECT]   [INDIRECT]
 *     |               |             |
 *     |             [min(i1)]    [min(i1)]
 *    sCurColumn       |             |
 *                   [t1.i1]      [t2.i1]
 *
 ***********************************************************************/

    idBool    sIsNotExist = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sColumnNode;
    qtcNode * sValueNode;
    qtcNode * sCurColumn;
    qtcNode * sCurValue;
    qtcNode * sValue;
    qtcNode * sValue2;
    qtcNode * sColumn;

    IDU_FIT_POINT_FATAL( "qmoPred::checkSameGroupType::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsIndexable != NULL );

    //--------------------------------------
    // ����5�� �Ǵ� : column�� conversion�� �߻����� �ʾƾ� �Ѵ�.
    // column�� value�� ���ϰ迭�� ���ϴ� data type������ �˻�
    //--------------------------------------

    sCompareNode = aNode;

    if ( sCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode *)(sCompareNode->node.arguments);
        sValueNode  = (qtcNode *)(sCompareNode->node.arguments->next);
    }
    else
    {
        sColumnNode = (qtcNode *)(sCompareNode->node.arguments->next);
        sValueNode  = (qtcNode *)(sCompareNode->node.arguments);
    }

    // PROJ-1364
    // column node�� conversion�� �����ϴ���
    // value�� ���ϰ迭�� data type�̸�, indexable predicate����
    // �з��ؼ�, index�� Ż �� �ֵ��� �Ѵ�.

    if ( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_LIST )
    {
        // column node�� LIST�� ���,
        // (i1,i2,i3) = ( 1,2,3 )
        // (i1,i2,i3) in ( (1,1,1), (2,2,2) )
        // (i1,i2,i3) in ( (select i1,i2,i3 ...), (select i1,i2,i3 ...) )
        // (i1,i2) in ( select (select min(i1) from ... ),
        //                     (select min(i2) from ... )
        //              from ... )

        sCurColumn = (qtcNode *)(sColumnNode->node.arguments);

        if ( ( ( sValueNode->node.arguments->lflag &
                 MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_LIST )
             ||
             ( ( ( sValueNode->node.arguments->lflag &
                   MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_SUBQUERY )
               &&
               ( ( sValueNode->node.arguments->lflag &
                   MTC_NODE_INDIRECT_MASK )
                 == MTC_NODE_INDIRECT_FALSE ) ) )
        {
            sCurValue = (qtcNode*)(sValueNode->node.arguments);
        }
        else
        {
            // (i1,i2,i3)=(1,2,3)
            sCurValue = sValueNode;
        }
    }
    else
    {
        // column node�� column�� ���
        // i1 = 1
        // i1 in ( 1, 2, 3 )
        // i1 in ( select i1 from ... )

        sCurColumn = sColumnNode;

        if ( ( ( sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST )
             ||
             ( ( sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_SUBQUERY ) )
        {
            sCurValue = (qtcNode*)(sValueNode->node.arguments);
        }
        else
        {
            sCurValue = sValueNode;
        }
    }

    while ( ( sIsNotExist == ID_TRUE ) &&
            ( sCurValue != NULL ) && ( sCurValue != sColumnNode ) )
    {
        // (1=i1)�� ���, value->next�� ����������
        // columnNode�� valueNode�� ���� ������ �˻�.

        // ( i1, i2 ) in ( ( 1,1 ), ( 2, 2 ) )
        // ( i1, i2 ) in ( select i1, i2 from ... )
        if ( ( ( sCurValue->node.lflag & MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST )
             ||
             ( ( sCurValue->node.lflag & MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_SUBQUERY ) )
        {
            sValue  = (qtcNode*)(sCurValue->node.arguments);
        }
        else
        {
            sValue = sCurValue;
        }

        sColumn = sCurColumn;

        while ( ( sColumn != NULL ) && ( sColumn != sValueNode ) )
        {
            // (i1=1)�� ���, column->next�� ����������
            // columnNode�� valueNode�� ���� ������ �˻�.

            // QTC_NODE_CHECK_SAMEGROUP_MASK�� �����ϴ� ������,
            // qmoKeyRange::isIndexable() �Լ�������
            //  (1) host ������ binding �� ��,
            //  (2) sort temp table�� ���� keyRange ������,
            // ���ϰ迭�� index ��밡�������� �Ǵ��ϰ� �Ǹ�,
            // �̶�, prepare �ܰ迡�� �̹� �Ǵܵ� predicate�� ����,
            // �ߺ� �˻����� �ʱ� ����

            // fix BUG-12058 BUG-12061
            sColumn->lflag &= ~QTC_NODE_CHECK_SAMEGROUP_MASK;
            sColumn->lflag |= QTC_NODE_CHECK_SAMEGROUP_TRUE;

            // fix BUG-32079
            // sValue may be one column subquery node.
            // ex) 
            // select * from dual where ( dummy, dummy ) in ( ( 'X', ( select 'X' from dual ) ) ); 
            // select * from dual where ( dummy, dummy ) in ( ( ( select 'X' from dual ), ( select 'X' from dual ) ) ); 
            if ( isOneColumnSubqueryNode( sValue ) == ID_TRUE )
            {
                sValue2  = (qtcNode*)(sValue->node.arguments);
            }
            else
            {
                sValue2  = sValue;
            }

            if ( isSameGroupType( QC_SHARED_TMPLATE(aStatement),
                                  sColumn,
                                  sValue2 ) == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                sIsNotExist = ID_FALSE;
                break;
            }

            IDE_FT_ASSERT( sValue != NULL );

            sColumn = (qtcNode*)(sColumn->node.next);
            sValue  = (qtcNode*)(sValue->node.next);
        }

        sCurValue  = (qtcNode*)(sCurValue->node.next);
    }

    if ( sIsNotExist == ID_TRUE )
    {
        *aIsIndexable = ID_TRUE;
    }
    else
    {
        *aIsIndexable = ID_FALSE;
    }

    return IDE_SUCCESS;
}

idBool
qmoPred::isSameGroupType( qcTemplate  * aTemplate,
                          qtcNode     * aColumnNode,
                          qtcNode     * aValueNode )
{
/***********************************************************************
 *
 * Description : �÷��� conversion�� �����ϴ� ���,
 *               value node�� ���ϰ迭�� ������Ÿ�������� �Ǵ�.
 *
 * Implementation :
 *
 *    ���� : PROJ-1364
 *
 *    data type�� �з�
 *
 *    -------------------------------------------------------------
 *    �������迭 | CHAR, VARCHAR, NCHAR, NVARCHAR,
 *               | BIT, VARBIT, ECHAR, EVARCHAR
 *    -------------------------------------------------------------
 *    �������迭 | Native | ������ | BIGINT, INTEGER, SMALLINT
 *               |        |----------------------------------------
 *               |        | �Ǽ��� | DOUBLE, REAL
 *               --------------------------------------------------
 *               | Non-   | �����Ҽ����� | NUMERIC, DECIMAL,
 *               | Native |              | NUMBER(p), NUMBER(p,s)
 *               |        |----------------------------------------
 *               |        | �����Ҽ����� | FLOAT, NUMBER
 *    -------------------------------------------------------------
 *    To fix BUG-15768
 *    ��Ÿ �迭  | NIBBLE, BYTE �� ��ǥŸ���� ����.
 *               | ���� ���ϰ迭 �񱳸� �� �� ����.
 *    -------------------------------------------------------------
 *
 ***********************************************************************/

    UInt         sColumnType = 0;
    UInt         sValueType  = 0;
    idBool       sIsSameGroupType = ID_TRUE;
    qtcNode    * sColumnConversionNode;
    qtcNode    * sCheckValue;
    mtcColumn  * sColumnColumn;
    mtcColumn  * sValueColumn;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate   != NULL );
    IDE_DASSERT( aColumnNode != NULL );

    //--------------------------------------
    // column�� value�� ���� �迭�� data type������ �Ǵ�
    //--------------------------------------

    sColumnConversionNode = aColumnNode;

    // fix BUG-12061
    // ��:  where ( i1, i2 ) = ( (select max(i1) ..),
    //                           (select max(i2) ..) )
    //  aValueNode�� indirect node�� ��찡 �����Ƿ�,
    //  �̶�, ���� �񱳴�� ��带 ã�´�.
    //  [ = ]
    //    |
    //  [LIST]-----------[LIST]    |----- aValueNode-----|
    //    |                 |      |                     |
    //   [I1]-[I2]       [SUBQUERY,INDIRECT]---[SUBQUERY-INDIRECT]
    //                      |                      |
    //                   [MAX(I1)]                [MAX(I2)]
    //                      |                      |
    //                     [I1]                   [I2]
    //
    // fix BUG-16047
    // conversion node�� �޷��ִ� ���,
    // ���� �񱳴�� ���� conversion node�̴�.
    if( aValueNode == NULL )
    {
        sCheckValue = NULL;
    }
    else
    {
        sCheckValue = (qtcNode*)
            mtf::convertedNode( (mtcNode*) aValueNode,
                                & aTemplate->tmplate );
    }

    if( aColumnNode->node.module == &qtc::passModule )
    {
        // To Fix PR-8700
        // t1.i1 + 1 > t2.i1 + 1 �� ���� ������ ó���� ��,
        // ������ �ݺ� ������ �����ϱ� ���� Pass Node�� ����Ͽ�
        // �����ϰ� �Ǹ�, Pass Node�� �ݵ�� Indirection�ȴ�.
        //
        //      [>]
        //       |
        //       V
        //      [+] ---> [Pass]
        //                 |
        //                 V
        //                [+]
        //
        // ����, Key Range �����ÿ� Pass Node�� ���
        // Conversion�� �߻����� �ʴ��� Conversion�� �߻��� ������
        // �Ǵܵȴ�.  �̷��� ��� Pass Node�� Conversion �� ����
        // ���� ���� Node�̸�, Conversion�� �߻����� ���� ������
        // �Ǵ��Ͽ��� �Ѵ�.

        // Nothing To Do
    }
    else
    {
        if( aColumnNode->node.conversion == NULL )
        {
            if( aValueNode != NULL )
            {
                // �� : varchar_col in ( intger'1', bigint'1' )

                // BUG-21936
                // aValueNode�� leftCoversion�� ����Ǿ�� �Ѵ�.
                if( aValueNode->node.leftConversion != NULL )
                {
                    sColumnConversionNode =
                        (qtcNode*)(aValueNode->node.leftConversion);
                }
                else
                {
                    // Nothing To Do
                }
            }
            else
            {
                // �� : i1 is null
                // Nothing To Do
            }
        }
        else
        {
            sColumnConversionNode = (qtcNode*)
                mtf::convertedNode( (mtcNode*)aColumnNode,
                                    & aTemplate->tmplate );
        }
    }

    if( aColumnNode == sColumnConversionNode )
    {
        // Nothing To Do
    }
    else
    {
        IDE_FT_ASSERT( sCheckValue != NULL );

        sColumnColumn = QTC_TMPL_COLUMN( aTemplate, aColumnNode );
        sValueColumn  = QTC_TMPL_COLUMN( aTemplate, sCheckValue );
        
        sColumnType = ( sColumnColumn->module->flag & MTD_GROUP_MASK );
        sValueType  = ( sValueColumn->module->flag & MTD_GROUP_MASK );

        if( sColumnType == sValueType )
        {
            if( sColumnType == MTD_GROUP_TEXT )
            {
                // PROJ-2002 Column Security
                // echar, evarchar�� text group�̳� ������ ���� ���
                // group compare�� �Ұ����ϴ�.
                //
                // -------------+-------------------------------------------
                //              |                column
                //              +----------+----------+----------+----------
                //              | char     | varchar  | echar    | evarchar
                // ---+---------+----------+----------+----------+----------
                //    | char    | char     | varchar  | echar    | evarchar 
                //    |         |          |          | (�Ұ���) | (�Ұ���)
                //  v +---------+----------+----------+----------+----------
                //  a | varchar | varchar  | varchar  | varchar  | evarchar 
                //  l |         |          |          | (�Ұ���) | (�Ұ���)
                //  u +---------+----------+----------+----------+----------
                //  e | echar   | echar    | varchar  | echar    | evarchar
                //    |         | (�Ұ���) | (�Ұ���) |          | (�Ұ���)
                //    +---------+----------+----------+----------+----------
                //    | evarchar| evarchar | evarchar | evarchar | evarchar 
                //    |         | (�Ұ���) | (�Ұ���) | (�Ұ���) |
                // ---+---------+----------+----------+----------+----------
                
                if ( ( sColumnColumn->module->id == MTD_ECHAR_ID ) ||
                     ( sColumnColumn->module->id == MTD_EVARCHAR_ID ) ||
                     ( sValueColumn->module->id == MTD_ECHAR_ID ) ||
                     ( sValueColumn->module->id == MTD_EVARCHAR_ID ) )
                {
                    // column�̳� value�� ��ȣ Ÿ���� �ִٸ�
                    // ���� �迭 �׷� �񱳰� �Ұ���.
                    sIsSameGroupType = ID_FALSE;
                }
                else
                {
                    // Nothing to do.
                }
                
                // BUG-26283
                // nchar, nvarchar�� text group�̳� ������ ���� ���
                // group compare�� �Ұ����ϴ�.
                //
                // -------------+-------------------------------------------
                //              |                column
                //              +----------+----------+----------+----------
                //              | char     | varchar  | nchar    | nvarchar
                // ---+---------+----------+----------+----------+----------
                //    | char    | char     | varchar  | nchar    | nvarchar 
                //    |         |          |          |          |
                //  v +---------+----------+----------+----------+----------
                //  a | varchar | varchar  | varchar  | nvarchar | nvarchar 
                //  l |         |          |          |          | 
                //  u +---------+----------+----------+----------+----------
                //  e | nchar   | nchar    | nvarchar | nchar    | nvarchar
                //    |         | (�Ұ���) | (�Ұ���) |          |
                //    +---------+----------+----------+----------+----------
                //    | nvarchar| nvarchar | nvarchar | nvarchar | nvarchar 
                //    |         | (�Ұ���) | (�Ұ���) |          |
                // ---+---------+----------+----------+----------+----------
                
                if ( ( ( sColumnColumn->module->id == MTD_CHAR_ID ) ||
                       ( sColumnColumn->module->id == MTD_VARCHAR_ID ) )
                     &&
                     ( ( sValueColumn->module->id == MTD_NCHAR_ID ) ||
                       ( sValueColumn->module->id == MTD_NVARCHAR_ID ) ) )
                {
                    // column�� char/varchar�̰� value�� nchar/nvarchar�̸�
                    // ���� �迭 �׷� �񱳰� �Ұ���.
                    sIsSameGroupType = ID_FALSE;
                }
                else
                {
                    // Nothing to do.
                }    
            }
            else
            {
                // Nothing To Do
            }
            
            if( sColumnType == MTD_GROUP_MISC )
            {
                // To fix BUG-15768
                // ��Ÿ �׷��� ��ǥŸ���� �������� ����.
                // ����, ���� �迭 �׷� �񱳰� �Ұ���.
                sIsSameGroupType = ID_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            sIsSameGroupType = ID_FALSE;
        }
    }

    return sIsSameGroupType;
}


IDE_RC
qmoPred::isIndexableValue( qcStatement * aStatement,
                           qtcNode     * aNode,
                           qcDepInfo   * aOuterDependencies,
                           idBool      * aIsIndexable )
{
/***********************************************************************
 *
 * Description : indexable Predicate �Ǵܽ�,
 *               value�� ���� ���� �˻�( ���� 6�� �˻�).
 *
 * Implementation :
 *
 *     1. host������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
 *     2. subquery�� ���� ���, subquery type�� A, N���̾�� �Ѵ�.
 *        subquery����ȭ �� ��������
 *        subquery keyRange or IN���� subquery keyRange ���� �˻�.
 *     3. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
 *
 ***********************************************************************/

    idBool     sIsTemp = ID_TRUE;
    idBool     sIsIndexableValue = ID_TRUE;
    qtcNode  * sCompareNode;
    qtcNode  * sValueNode;
    qtcNode  * sNode;
    qmgPROJ  * sPROJGraph;

    IDU_FIT_POINT_FATAL( "qmoPred::isIndexableValue::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIsIndexable != NULL );

    //--------------------------------------
    // ����6�� �Ǵ� : value�� ���� üũ
    //--------------------------------------

    sCompareNode = aNode;

    if ( sCompareNode->indexArgument == 0 )
    {
        sValueNode = (qtcNode *)(sCompareNode->node.arguments->next);
    }
    else
    {
        sValueNode = (qtcNode *)(sCompareNode->node.arguments);
    }

    // PROJ-1492
    // ȣ��Ʈ ������ Ÿ���� �����Ǵ��� �� ���� data binding������
    // �� �� ����.
    if ( MTC_NODE_IS_DEFINED_VALUE( & sCompareNode->node ) == ID_FALSE )
    {
        // 1. host ������ �����ϸ�, indexable�� �Ǵ��Ѵ�.
        // Nothing To Do
    }
    else
    {
        while ( sIsTemp == ID_TRUE )
        {
            sIsTemp = ID_FALSE;

            // 2. subquery�� ���� ���, subquery type�� A, N���̾�� �Ѵ�.
            //    subquery����ȭ �� ��������
            //    subquery keyRange or IN���� subquery keyRange ���� �˻�.
            //    ��, between, not between�� ����
            //    store and search ����ȭ �������� �˻��Ѵ�.

            for ( sNode  = sValueNode;
                  sNode != NULL;
                  sNode  = (qtcNode *)(sNode->node.next) )
            {
                if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_SUBQUERY )
                {
                    sPROJGraph = (qmgPROJ *)(sNode->subquery->myPlan->graph);

                    if ( ( sCompareNode->node.module == &mtfBetween ) ||
                         ( sCompareNode->node.module == &mtfNotBetween ) )
                    {
                        // between, not between�� ���

                        if ( ( sPROJGraph->subqueryTipFlag
                               & QMG_PROJ_SUBQUERY_TIP_MASK )
                             == QMG_PROJ_SUBQUERY_TIP_STORENSEARCH )
                        {
                            // Nothing To Do
                        }
                        else
                        {
                            sIsIndexableValue = ID_FALSE;
                            break;
                        }
                    }
                    else
                    {
                        // between, not between�� �ƴ� ���
                        // value node�� �ϳ��̹Ƿ�,
                        // �ϳ��� value node �˻� ��, for���� ���� ������.

                        if ( ( ( sPROJGraph->subqueryTipFlag
                                 & QMG_PROJ_SUBQUERY_TIP_MASK )
                               == QMG_PROJ_SUBQUERY_TIP_KEYRANGE )
                             || ( ( sPROJGraph->subqueryTipFlag
                                    & QMG_PROJ_SUBQUERY_TIP_MASK )
                                  == QMG_PROJ_SUBQUERY_TIP_IN_KEYRANGE ) )
                        {
                            // Nothing To Do
                        }
                        else
                        {
                            sIsIndexableValue = ID_FALSE;
                        }

                        break;
                    }
                }
                else
                {
                    // subquery node�� �ƴ� ���
                    // Nothing To Do
                }
            } // end of for()

            if ( sIsIndexableValue == ID_TRUE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }

            // 3. LIKE�� ���� ���Ϲ��ڴ� �Ϲݹ��ڷ� �����Ͽ��� �Ѵ�.
            if ( sCompareNode->node.module == &mtfLike )
            {
                // �Ϲݹ��ڷ� �����ϴ� ��������� �˻��Ѵ�.
                if ( ( QC_SHARED_TMPLATE( aStatement )->
                       tmplate.rows[sValueNode->node.table].lflag
                       & MTC_TUPLE_TYPE_MASK)
                     == MTC_TUPLE_TYPE_CONSTANT)
                {
                    IDE_TEST( isIndexableLIKE( aStatement,
                                               sCompareNode,
                                               & sIsIndexableValue )
                              != IDE_SUCCESS );

                    if ( sIsIndexableValue == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    // BUG-25594
                    // dynamic constant expression�̸� indexable�� �Ǵ��Ѵ�.
                    if ( qtc::isConstNode4LikePattern( aStatement,
                                                       sValueNode,
                                                       aOuterDependencies )
                         == ID_TRUE )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsIndexableValue = ID_FALSE;
                        break;
                    }
                }
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    if ( sIsIndexableValue == ID_TRUE )
    {
        *aIsIndexable = ID_TRUE;
    }
    else
    {
        *aIsIndexable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isIndexableLIKE( qcStatement  * aStatement,
                          qtcNode      * aLikeNode,
                          idBool       * aIsIndexable )
{
/***********************************************************************
 *
 * Description : LIKE �������� index ��� ���� ���� �Ǵ�
 *
 * Implementation :
 *
 *     LIKE�� ���Ϲ��ڰ� �Ϲݹ��ڷ� �����ϴ����� �˻��Ѵ�.
 *     ��) i1 like 'acd%' (O), i1 like '%de' (X), i1 like ? (O),
 *         i1 like '%%de' escape '%' (O), i1 like 'bc'||'d%' (O)
 *
 *     1. (1) value�� ù��° ���ڰ� %, _ �� �ƴϸ�, indexable�� �Ǵ�.
 *        (2) value�� ù��° ���ڰ� %, _ �̸�,
 *            escape ���ڿ� ��ġ�ϴ����� �˻��ؼ�,
 *            ��ġ�ϸ�, indexable�� �Ǵ�.
 *
 ***********************************************************************/

    const mtlModule * sLanguage;

    qcTemplate      * sTemplate;
    idBool            sIsIndexableLike = ID_TRUE;
    mtdEcharType    * sPatternEcharValue;
    mtdCharType     * sPatternValue;
    mtdCharType     * sEscapeValue;
    mtcColumn       * sColumn;
    void            * sValue;
    qtcNode         * sValueNode;
    qtcNode         * sEscapeNode;
    idBool            sIsEqual1;
    idBool            sIsEqual2;
    idBool            sIsEqual3;
    UChar             sSize;

    mtcColumn       * sEncColumn;
    UShort            sPatternLength;
    UChar           * sPattern;

    IDU_FIT_POINT_FATAL( "qmoPred::isIndexableLIKE::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aLikeNode != NULL );
    IDE_DASSERT( aIsIndexable != NULL );

    //--------------------------------------
    // LIKE �����ڿ� ���� indexable �Ǵ�
    //--------------------------------------

    sTemplate   = QC_SHARED_TMPLATE(aStatement);
    sValueNode  = (qtcNode *)(aLikeNode->node.arguments->next);
    sEscapeNode = (qtcNode *)(sValueNode->node.next);

    // value node���� ���Ϲ��ڸ� ���´�.
    IDE_TEST( qtc::calculate( sValueNode, sTemplate )
              != IDE_SUCCESS );

    sColumn = sTemplate->tmplate.stack->column;
    sValue  = sTemplate->tmplate.stack->value;

    // To Fix PR-12999
    // Char�� Null �� ��� sPatternEcharValue->value[0] ���� �ƹ� ���� �������� ����
    // ����, UMR�� �����ϱ� ���ؼ��� Null ���θ� ���� �Ǵ��ؾ� ��.
    if ( sColumn->module->isNull( sColumn, sValue )
         == ID_TRUE )
    {
        // Null �� ���
        sIsIndexableLike = ID_FALSE;
    }
    else
    {
        sLanguage = sColumn->language;

        if ( (sColumn->module->id == MTD_ECHAR_ID) ||
             (sColumn->module->id == MTD_EVARCHAR_ID) )
        {
            // PROJ-2002 Column Security
            // pattern�� echar, evarchar�� ���

            sPatternEcharValue = (mtdEcharType*)sValue;

            //--------------------------------------------------
            // format string�� plain text�� ��´�.
            //--------------------------------------------------

            sEncColumn = QTC_STMT_COLUMN( aStatement, sValueNode );

            // ����� policy�� ���� �� ����.
            IDE_FT_ASSERT( sEncColumn->mColumnAttr.mEncAttr.mPolicy[0] == '\0' );

            sPattern       = sPatternEcharValue->mValue;
            sPatternLength = sPatternEcharValue->mCipherLength;
        }
        else
        {
            // pattern�� char, varchar, nchar, nvarchar�� ���

            sPatternValue = (mtdCharType*)sValue;

            sPattern       = sPatternValue->value;
            sPatternLength = sPatternValue->length;
        }

        sSize = mtl::getOneCharSize( sPattern,
                                     sPattern + sPatternLength,
                                     sLanguage );

        // '%' �������� �˻�
        sIsEqual1 = mtc::compareOneChar( sPattern,
                                         sSize,
                                         sLanguage->specialCharSet[MTL_PC_IDX],
                                         sLanguage->specialCharSize );

        // '_' �������� �˻�
        sIsEqual2 = mtc::compareOneChar( sPattern,
                                         sSize,
                                         sLanguage->specialCharSet[MTL_UB_IDX],
                                         sLanguage->specialCharSize );

        if ( ( sIsEqual1 != ID_TRUE ) && ( sIsEqual2 != ID_TRUE ) )
        {
            // ���Ϲ����� ù��° ���ڰ� '%', '_'�� �������� �ʴ´�.
            // Nothing To Do
        }
        else
        {
            // ���Ϲ����� ù��° ���ڰ� '%', '_'�� �����Ѵٸ�,
            // escape ���ڿ� ���������� �˻��Ѵ�.
            if ( sEscapeNode != NULL )
            {
                IDE_TEST( qtc::calculate( sEscapeNode,
                                          sTemplate )
                          != IDE_SUCCESS );

                // escape ���ڸ� ���´�.
                sEscapeValue = (mtdCharType*)sTemplate->tmplate.stack->value;

                sIsEqual3 = mtc::compareOneChar( sPattern,
                                                 sSize,
                                                 sEscapeValue->value,
                                                 sEscapeValue->length );

                if ( sIsEqual3 == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsIndexableLike = ID_FALSE;
                }
            }
            else  // escape ���ڰ� ���� ���
            {
                sIsIndexableLike = ID_FALSE;
            }
        }
    }

    if ( sIsIndexableLike == ID_TRUE )
    {
        *aIsIndexable = ID_TRUE;
    }
    else
    {
        *aIsIndexable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::processIndexableInSubQ( qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description : ���ġ�� �Ϸ�� �� indexable column�� ����
 *               IN(subquery)�� ���� ó��
 *
 * Implementation :
 *
 *     IN(subquery)�� �ٸ� predicate�� �Բ� keyRange�� ������ �� ����,
 *     �ܵ����θ� keyRange�� �����ؾ��ϹǷ� predicate �з��� ������ ����,
 *     indexable �÷��� ���� IN(subquery)�� �����ϴ� ���,
 *     ������ ���� ó���Ѵ�.
 *
 *     selectivity�� ���� predicate�� ã�´�.
 *     A. ã�� predicate�� IN(subquery)�̸�,
 *        . �ش� �÷�����Ʈ�� ã�� IN(subquery)�� �����,
 *        . ������ predicate���� non-indexable�� ����Ʈ�� �޾��ش�.
 *     B. ã�� predicate�� IN(subquery)�� �ƴϸ�,
 *        . IN(subquery)���� ��� ã�Ƽ� non-indexable�� ����Ʈ�� �޾��ش�.
 *
 *     ����, indexable column LIST���� ������ ���� �� ������ ��찡 �����.
 *             (1) IN(subquery) �ϳ��� ����
 *             (2) IN(subquery)�� ���� predicate��θ� ����
 *
 *     Example )
 *
 *
 *             [i1 = 1] ----> [i2 IN] ---> [N/A]
 *                |              |
 *             [i1 IN ]       [I2 = 1]
 *
 *                            ||
 *                            \/
 *
 *             [i1 = 1] ----> [i2 IN] ---> [N/A]
 *                                           |
 *                                         [i1 IN]
 *                                           |
 *                                         [i2 = 1]
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoPredicate * sNonIndexablePred;
    qmoPredicate * sLastNonIndexable;
    qmoPredicate * sCurPredicate;
    qmoPredicate * sNextPredicate;
    qmoPredicate * sMorePredicate;
    qmoPredicate * sMoreMorePred;
    qmoPredicate * sSelPredicate;   // selectivity�� ���� predicate
    qmoPredicate * sTempPredicate = NULL;
    qmoPredicate * sLastTempPred  = NULL;
    qmoPredicate * sTemp = NULL;
    qmoPredicate * sLastTemp;

    IDU_FIT_POINT_FATAL( "qmoPred::processIndexableInSubQ::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //------------------------------------------
    // non-indexable �÷�����Ʈ�� ã�´�.
    //------------------------------------------

    sPredicate = *aPredicate;

    for ( sCurPredicate = sPredicate;
          ( sCurPredicate != NULL ) &&
              ( sCurPredicate->id != QMO_COLUMNID_NON_INDEXABLE );
          sCurPredicate = sCurPredicate->next );

    if ( sCurPredicate == NULL )
    {
        // ���ڷ� �Ѿ�� predicate ����Ʈ����
        // non-indexable predicate�� �������� �ʴ� ���

        sNonIndexablePred = NULL;
    }
    else
    {
        // ���ڷ� �Ѿ�� predicate ����Ʈ����
        // non-indexable predicate�� �����ϴ� ���
        sNonIndexablePred = sCurPredicate;

        for ( sLastNonIndexable        = sNonIndexablePred;
              sLastNonIndexable->more != NULL;
              sLastNonIndexable        = sLastNonIndexable->more ) ;
    }

    //------------------------------------------
    // �� �÷����� IN(subquery)�� ���� ó��
    //------------------------------------------

    for ( sCurPredicate  = sPredicate;
          sCurPredicate != NULL;
          sCurPredicate  = sNextPredicate )
    {
        //------------------------------------------
        // IN(subquery)�� �����ϴ��� Ȯ��.
        //------------------------------------------

        sTemp = NULL;

        sNextPredicate = sCurPredicate->next;

        if ( sCurPredicate->id == QMO_COLUMNID_NON_INDEXABLE )
        {
            sTemp = sCurPredicate;
        }
        else
        {
            // �÷�����Ʈ�� IN(subquery)�� ������ predicate�� �ִ��� �˻�
            for ( sMorePredicate  = sCurPredicate;
                  sMorePredicate != NULL;
                  sMorePredicate  = sMorePredicate->more )
            {
                if ( ( sMorePredicate->flag & QMO_PRED_INSUBQUERY_MASK )
                     == QMO_PRED_INSUBQUERY_EXIST )
                {
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            if ( sMorePredicate == NULL )
            {
                // �÷�����Ʈ�� IN(subquery)�� ������ predicate�� ���� ���
                sTemp = sCurPredicate;
            }
            else
            {
                // �÷�����Ʈ�� IN(subquery)�� ������ predicate�� �ִ� ���

                //------------------------------------------
                // selectivity�� ���� ���� predicate�� ã�´�.
                //------------------------------------------

                sMorePredicate = sCurPredicate;
                sSelPredicate  = sMorePredicate;
                sMorePredicate = sMorePredicate->more;

                while ( sMorePredicate != NULL )
                {
                    if ( sSelPredicate->mySelectivity >
                         sMorePredicate->mySelectivity )
                    {
                        sSelPredicate = sMorePredicate;
                    }
                    else
                    {
                        // Nothing To Do
                    }

                    sMorePredicate = sMorePredicate->more;
                }

                if ( ( sSelPredicate->flag & QMO_PRED_INSUBQUERY_MASK )
                     == QMO_PRED_INSUBQUERY_EXIST )
                {
                    //------------------------------------------
                    // ã�� predicate�� IN(subquery)�̸�,
                    // . �ش� �÷�����Ʈ��  IN(subquery)�� �����,
                    // . ������ predicate���� non-indexable�� ����Ʈ��
                    //   �޾��ش�.
                    //------------------------------------------

                    for ( sMorePredicate  = sCurPredicate;
                          sMorePredicate != NULL;
                          sMorePredicate  = sMoreMorePred )
                    {
                        sMoreMorePred = sMorePredicate->more;

                        if ( sSelPredicate == sMorePredicate )
                        {
                            sTemp = sMorePredicate;
                        }
                        else
                        {
                            // non-indexable �� ����
                            // columnID ����
                            sMorePredicate->id = QMO_COLUMNID_NON_INDEXABLE;

                            if ( ( sMorePredicate->node->lflag &
                                  QTC_NODE_SUBQUERY_MASK )
                                  == QTC_NODE_SUBQUERY_EXIST )
                            {
                                // subqueryKeyRange ����ȭ ���� �����Ѵ�.
                                IDE_TEST( removeIndexableSubQTip( sMorePredicate->node ) != IDE_SUCCESS );
                            }
                            else
                            {
                                // Nothing To Do
                            }

                            if ( sNonIndexablePred == NULL )
                            {
                                sNonIndexablePred = sMorePredicate;
                                sLastNonIndexable = sNonIndexablePred;

                                // ���ڷ� �Ѿ�� predicate ����Ʈ����
                                // non-indexable predicate�� �������� �ʴ� ���
                                // predicate list�� ����Ǿ�� �Ѵ�.
                                if ( sTempPredicate == NULL )
                                {
                                    sTempPredicate = sNonIndexablePred;
                                    sLastTempPred = sTempPredicate;
                                }
                                else
                                {
                                    sLastTempPred->next = sNonIndexablePred;
                                    sLastTempPred = sLastTempPred->next;
                                }
                            }
                            else
                            {
                                sLastNonIndexable->more = sMorePredicate;
                                sLastNonIndexable = sLastNonIndexable->more;
                            }
                        }

                        sMorePredicate->more = NULL;
                    }
                } // selectivity�� ���� predicate�� IN(subquery)�� ����� ó��
                else
                {
                    //------------------------------------------
                    // ã�� predicate�� IN(subquery)�� �ƴϸ�,
                    // . IN(subquery)���� ��� ã�Ƽ�
                    //   non-indexable�� ����Ʈ�� �޾��ش�.
                    //------------------------------------------
                    for ( sMorePredicate  = sCurPredicate;
                          sMorePredicate != NULL;
                          sMorePredicate  = sMoreMorePred )
                    {
                        sMoreMorePred = sMorePredicate->more;

                        if ( ( sMorePredicate->flag
                               & QMO_PRED_INSUBQUERY_MASK )
                             != QMO_PRED_INSUBQUERY_EXIST )
                        {
                            if ( sTemp == NULL )
                            {
                                sTemp = sMorePredicate;
                                sLastTemp = sTemp;
                            }
                            else
                            {
                                sLastTemp->more = sMorePredicate;
                                sLastTemp = sLastTemp->more;
                            }
                        }
                        else
                        {
                            // non-indexable �� ����
                            // IN(subquery)�� filter�� ���ǹǷ�,
                            // IN(subquery) ����ȭ �� flag�� �����Ѵ�.
                            IDE_TEST( removeIndexableSubQTip( sMorePredicate->node ) != IDE_SUCCESS );
                            // columnID ����
                            sMorePredicate->id = QMO_COLUMNID_NON_INDEXABLE;

                            if ( sNonIndexablePred == NULL )
                            {
                                sNonIndexablePred = sMorePredicate;
                                sLastNonIndexable = sNonIndexablePred;

                                // ���ڷ� �Ѿ�� predicate ����Ʈ����
                                // non-indexable predicate�� �������� �ʴ� ���
                                // predicate list�� ����Ǿ�� �Ѵ�.
                                if ( sTempPredicate == NULL )
                                {
                                    sTempPredicate = sNonIndexablePred;
                                    sLastTempPred = sTempPredicate;
                                }
                                else
                                {
                                    sLastTempPred->next = sNonIndexablePred;
                                    sLastTempPred = sLastTempPred->next;
                                }
                            }
                            else
                            {
                                sLastNonIndexable->more = sMorePredicate;
                                sLastNonIndexable = sLastNonIndexable->more;
                            }
                        }

                        sMorePredicate->more = NULL;
                    }
                }
            }
        }

        sCurPredicate->next = NULL;

        if ( sTempPredicate == NULL )
        {
            sTempPredicate = sTemp;
            sLastTempPred  = sTempPredicate;
        }
        else
        {
            sLastTempPred->next = sTemp;
            sLastTempPred       = sLastTempPred->next;
        }
    }

    *aPredicate = sTempPredicate;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isJoinablePredicate( qmoPredicate * aPredicate,
                              qcDepInfo    * aFromDependencies,
                              qcDepInfo    * aLeftChildDependencies,
                              qcDepInfo    * aRightChildDependencies,
                              idBool       * aIsOnlyIndexNestedLoop   )
{
/***********************************************************************
 *
 * Description : joinable predicate�� �Ǵ�
 *
 * [joinable predicate]: predicate�� ���� ��尡
 *                       JOIN graph�� child graph�� �ϳ��� �����Ǿ�� ��.
 *     ��) t1.i1 = t2.i1 (O),  t1.i1+1 = t2.i1+1 (O),
 *         t1.i1=t2.i1 OR t1.i1=t2.i2 (O) : only index nested loop join
 *         t1.i1=1 OR t2.i1=3 (X) : join predicate������, non-joinable.
 *
 * Implementation :
 *
 *    joinable predicate�� �Ǵ��ؼ�, qmoPredicate.flag�� �� ���� ����.
 *
 *    OR ���� ��尡 2���̻��� ����, ��� joinable predicate�̾�� �Ѵ�.
 *     1. joinable predicate���� ���ɼ� �ִ� �񱳿����� �Ǵ�.
 *        [ =, >, >=, <, <= ]
 *     2. 1�� ������ �����Ǹ�,
 *        �񱳿����� ���� �ΰ� ����� dependencies ������
 *        �ϳ��� ��尡 left child�� �÷��̶��,
 *        ������ �ϳ��� ���� right child�� �÷������� �˻��Ѵ�.
 *
 * [ aIsOnlyIndexNestedLoop �� t1.i1=t2.i1 OR t1.i1=t2.i2�� ���� ��쿡
 *   true�� �����ȴ�. ]
 *
 ***********************************************************************/

    idBool    sIsJoinable = ID_TRUE;
    idBool    sIsOnlyIndexNestedLoopPred = ID_FALSE;
    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isJoinablePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aLeftChildDependencies != NULL );
    IDE_DASSERT( aRightChildDependencies != NULL );
    IDE_DASSERT( aIsOnlyIndexNestedLoop != NULL );

    //--------------------------------------
    // joinable predicate�� �Ǵ�
    //--------------------------------------

    sNode = aPredicate->node;

    if ( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_OR )
    {
        //-------------------------------------
        // CNF �� ����,
        // ���ڷ� �Ѿ�� predicate�� �ֻ��� ���� OR����̸�,
        // OR ��� ������ �������� join predicate�� �� �� �ִ�.
        // OR ��� ������ �������� join predicate�� ���� ���,
        // ��� predicate�� joinable�̾�� �ϸ�,
        // index nested loop join method�� ����� �� �ִ�.
        //--------------------------------------

        // sNode�� �񱳿����� ���
        sNode = (qtcNode *)(sNode->node.arguments);

        if ( aPredicate->node->node.arguments->next == NULL )
        {
            // 1. OR ��� ������ �񱳿����ڰ� �ϳ��� ���� ���
            //    joinable predicate������ �Ǵ��ϸ� �ȴ�.
            IDE_TEST( isJoinableOnePred( sNode,
                                         aFromDependencies,
                                         aLeftChildDependencies,
                                         aRightChildDependencies,
                                         & sIsJoinable )
                      != IDE_SUCCESS );
        }
        else
        {
            // 2. OR ��� ������ �񱳿����ڰ� ������ ���� ���
            //    ���� ��尡 ��� joinable predicate�̾�� �Ѵ�.
            //    �� ����, index nested loop join method�� ����� �� �ִ�.

            sIsOnlyIndexNestedLoopPred = ID_TRUE;

            while ( sNode != NULL )
            {
                IDE_TEST( isJoinableOnePred( sNode,
                                             aFromDependencies,
                                             aLeftChildDependencies,
                                             aRightChildDependencies,
                                             & sIsJoinable )
                          != IDE_SUCCESS );

                if ( sIsJoinable == ID_TRUE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsOnlyIndexNestedLoopPred = ID_FALSE;
                    break;
                }

                sNode = (qtcNode *)(sNode->node.next);
            }
        }
    }
    else
    {
        //-------------------------------------
        // DNF�� ����,
        // ���ڷ� �Ѿ�� predicate�� �ֻ��� ���� �񱳿����� ����̴�.
        // ���� predicate�� ������谡 �������� ���� �����̹Ƿ�,
        // ���ڷ� �Ѿ�� �񱳿����� ��� �ϳ��� ���ؼ��� ó���ؾ� �Ѵ�.
        //--------------------------------------

        IDE_TEST( isJoinableOnePred( sNode,
                                     aFromDependencies,
                                     aLeftChildDependencies,
                                     aRightChildDependencies,
                                     & sIsJoinable )
                  != IDE_SUCCESS );
    }

    //-------------------------------------
    // qmoPredicate.flag�� joinable predicate ������ �����Ѵ�.
    //-------------------------------------

    if ( sIsJoinable == ID_TRUE )
    {
        aPredicate->flag &= ~QMO_PRED_JOINABLE_PRED_MASK;
        aPredicate->flag |= QMO_PRED_JOINABLE_PRED_TRUE;
    }
    else
    {
        aPredicate->flag &= ~QMO_PRED_JOINABLE_PRED_MASK;
        aPredicate->flag |= QMO_PRED_JOINABLE_PRED_FALSE;
    }

    *aIsOnlyIndexNestedLoop = sIsOnlyIndexNestedLoopPred;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isJoinableOnePred( qtcNode     * aNode,
                            qcDepInfo   * aFromDependencies,
                            qcDepInfo   * aLeftChildDependencies,
                            qcDepInfo   * aRightChildDependencies,
                            idBool      * aIsJoinable )
{
/***********************************************************************
 *
 * Description : �ϳ��� �񱳿����ڿ� ���� joinable predicate �Ǵ�
 *
 *     qmoPred::isJoinablePredicate() �Լ� �ּ� ����.
 *
 * Implementation :
 *
 *  �񱳿������� �� ������尡
 *  left/right child graph�� �ϳ��� �����Ǵ��� �˻�.
 *
 *  1. �񱳿����� �� ������忡 ���Ͽ�, �ش� table�� dependencies�� ���Ѵ�.
 *     nodeDependencies = ����� dependencies & FROM�� dependencies
 *  2. ��尡 left/right graph�� ���ϴ� �÷����� �˻�
 *     orDependencies = nodeDependencies | left/right graph�� dependencies
 *  3. orDependencies�� left/right graph�� dependencies�� �����ϸ�,
 *     �ش� ���� left/right graph�� ���ϴ� �÷��̶�� �Ǵ��Ѵ�.
 *
 ***********************************************************************/

    qcDepInfo sFirstNodeDependencies;
    qcDepInfo sSecondNodeDependencies;
    qcDepInfo sLeftOrDependencies;
    qcDepInfo sRightOrDependencies;
    qcDepInfo sOrDependencies;
    idBool    sIsTemp = ID_TRUE;
    idBool    sIsJoinablePred = ID_TRUE;
    qtcNode * sCompareNode;
    qtcNode * sFirstNode;
    qtcNode * sSecondNode;

    IDU_FIT_POINT_FATAL( "qmoPred::isJoinableOnePred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aNode != NULL );
    IDE_DASSERT( aIsJoinable != NULL );

    //--------------------------------------
    // joinable �Ǵ�
    //--------------------------------------

    sCompareNode = aNode;

    while ( sIsTemp == ID_TRUE )
    {
        sIsTemp = ID_FALSE;

        // fix BUG-12282
        //--------------------------------------
        // subquery�� �������� �ʾƾ� ��.
        //--------------------------------------
        if ( ( sCompareNode->lflag & QTC_NODE_SUBQUERY_MASK )
             == QTC_NODE_SUBQUERY_EXIST )
        {
            sIsJoinablePred  = ID_FALSE;
            break;
        }
        else
        {
            // Nothing To Do
        }

        //--------------------------------------
        // joinable predicate���� ���ɼ� �ִ� �񱳿����� �Ǵ�.
        // [ =, >, >=, <, <= ]
        //--------------------------------------
        // To Fix PR-15434
        if ( ( sCompareNode->node.module->lflag & MTC_NODE_INDEX_JOINABLE_MASK )
             == MTC_NODE_INDEX_JOINABLE_TRUE )
        {
            // Nothing To Do
        }
        else
        {
            sIsJoinablePred = ID_FALSE;
            break;
        }

        //--------------------------------------
        // �񱳿����� ���� �ΰ� ����� dependency ������
        // �ΰ��� ���� ��尡 join graph�� left/right child��
        // �ϳ��� �����Ǵ��� �˻��Ѵ�.
        //--------------------------------------

        sFirstNode = (qtcNode *)(sCompareNode->node.arguments);
        sSecondNode = (qtcNode *)(sCompareNode->node.arguments->next);

        //--------------------------------------
        // �� ������ �� ���� ������ �ش� table�� dependencies�� ���Ѵ�.
        // (node�� dependencies) & FROM�� dependencies
        //--------------------------------------

        qtc::dependencyAnd( & sFirstNode->depInfo,
                            aFromDependencies,
                            & sFirstNodeDependencies );

        qtc::dependencyAnd( & sSecondNode->depInfo,
                            aFromDependencies,
                            & sSecondNodeDependencies );

        //--------------------------------------
        // �񱳿����� ����� argument�� ���ؼ�,
        // left child, right child�� dependencies�� oring
        //--------------------------------------

        IDE_TEST( qtc::dependencyOr( & sFirstNodeDependencies,
                                     aLeftChildDependencies,
                                     & sLeftOrDependencies )
                  != IDE_SUCCESS );


        IDE_TEST( qtc::dependencyOr( & sFirstNodeDependencies,
                                     aRightChildDependencies,
                                     & sRightOrDependencies )
                  != IDE_SUCCESS );

        if ( qtc::dependencyEqual( & sLeftOrDependencies,
                                   aLeftChildDependencies ) == ID_TRUE )
        {
            //--------------------------------------
            // �񱳿������� argument ��尡 left child graph�� ���ϴ� �÷�
            //--------------------------------------

            IDE_TEST( qtc::dependencyOr( & sSecondNodeDependencies,
                                         aRightChildDependencies,
                                         & sOrDependencies )
                      != IDE_SUCCESS );


            if ( qtc::dependencyEqual( & sOrDependencies,
                                       aRightChildDependencies ) == ID_TRUE )
            {
                //--------------------------------------
                // �񱳿������� argument->next ��尡
                // right child graph�� ���ϴ� �÷�
                //--------------------------------------

                //sCompareNode->node.arguments�� left child graph�� ���Եǰ�,
                //sCompareNode->node.arguemtns->next�� right child graph��
                // �����ϹǷ�, joinable predicate���� �Ǵ�

                // Nothing To Do
            }
            else
            {
                // non-joinable predicate
                sIsJoinablePred = ID_FALSE;
                break;
            }
        }
        else if ( qtc::dependencyEqual( & sRightOrDependencies,
                                        aRightChildDependencies ) == ID_TRUE )
        {
            //--------------------------------------
            // �񱳿������� argument ��尡 right child graph�� ���ϴ� �÷�
            //--------------------------------------

            IDE_TEST( qtc::dependencyOr( & sSecondNodeDependencies,
                                         aLeftChildDependencies,
                                         & sOrDependencies )
                      != IDE_SUCCESS );

            if ( qtc::dependencyEqual( & sOrDependencies,
                                       aLeftChildDependencies ) == ID_TRUE )
            {
                //--------------------------------------
                // �񱳿������� argument->next ��尡
                // left child graph�� ���ϴ� �÷�
                //--------------------------------------

                //sCompareNode->node.arguments�� right child graph�� ���Եǰ�,
                //sCompareNode->node.arguments->next�� left child graph��
                //���ԵǹǷ�, joinable predicate���� �Ǵ�
                // Nothing To Do
            }
            else
            {
                // non-joinable predicate
                sIsJoinablePred = ID_FALSE;
                break;
            }
        }
        else
        {
            // non-joinable predicate
            sIsJoinablePred = ID_FALSE;
            break;
        }
    }

    if ( sIsJoinablePred == ID_TRUE )
    {
        *aIsJoinable = ID_TRUE;
    }
    else
    {
        *aIsJoinable = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isIndexableJoinPredicate( qcStatement  * aStatement,
                                   qmoPredicate * aPredicate,
                                   qmgGraph     * aLeftChildGraph,
                                   qmgGraph     * aRightChildGraph )
{
/***********************************************************************
 *
 * Description : indexable join predicate�� �Ǵ�
 *
 *     < indexable join predicate�� ���� >
 *     indexable join predicate�� one table predicate�� ���� �����ϸ�,
 *     �������� ������ ����.
 *
 *     ����0.
 *            0-1. right child graph�� qmgSelection graph�̰�,
 *            0-2. joinable predicate�̾�� �ϰ�,
 *
 *     ����1. joinable operator�� [=, >, >=, <, <=]�� �������̰�,
 *            [ joinable predicate�˻翡�� �̹� �ɷ���. ]
 *
 *     ����6. subquery�� �������� �ʴ´�.
 *
 * Implementation :
 *
 *     one table predicate�� ���̳��� ���ǵ��� �̸� �ɷ�����,
 *     one table predicate�� ������ isIndexable()�Լ��� ����Ѵ�.
 *
 *     ���ڷ� �Ѿ�� predicate�� joinable operator�� joinable predicate�̹Ƿ�,
 *     subquery���翩�ο� right child graph�� qmgSelection ������ Ȯ���Ѵ�.
 *
 *     1. subquery�� �����ϴ��� �˻�.
 *
 *     2. left->right���� �˻�.
 *        right child graph�� qmgSelection graph�̰�,
 *        indexable predicate�̸�,
 *
 *     3. right->left���� �˻�.
 *        left child graph�� qmgSelection graph�̰�,
 *        indexable predicate�̸�,
 *
 *     indexable predicate���� �Ǵ��ϰ�, index nested loop join��
 *     ������ ���డ���� join������ qmoPredicate.flag�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool     sIsIndexableJoinPred = ID_TRUE;
    qmgGraph * sRightGraph;

    IDU_FIT_POINT_FATAL( "qmoPred::isIndexableJoinPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // indexable join predicate�� �Ǵ�
    //--------------------------------------

    aPredicate->flag &= ~QMO_PRED_INDEX_JOINABLE_MASK;
    aPredicate->flag &= ~QMO_PRED_INDEX_DIRECTION_MASK;

    //--------------------------------------
    // subquery�� �������� �ʾƾ� �Ѵ�.
    // joinable predicate�˻��, �̹� �ɷ���.
    //--------------------------------------

    //--------------------------------------
    // join ������ left child -> right child�� �����,
    // index joinable predicate���� ��밡������ �˻�.
    //--------------------------------------

    sRightGraph = aRightChildGraph;

    // PROJ-1502 PARTITIONED DISK TABLE
    // right child graph�� selection �Ǵ� partition graph���� �˻�.
    if ( ( sRightGraph->type == QMG_SELECTION ) ||
         ( sRightGraph->type == QMG_PARTITION ) )
    {
        IDE_TEST( isIndexable( aStatement,
                               aPredicate,
                               & sRightGraph->depInfo,
                               & qtc::zeroDependencies,
                               & sIsIndexableJoinPred )
                  != IDE_SUCCESS );

        // qmoPredicate.flag��
        // left->right���� ���� ���� ������ �����Ѵ�.
        if ( sIsIndexableJoinPred == ID_TRUE )
        {
            aPredicate->flag |= QMO_PRED_INDEX_JOINABLE_TRUE;
            aPredicate->flag |= QMO_PRED_INDEX_LEFT_RIGHT;
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

    //--------------------------------------
    // join ������ right child -> left child�� �����,
    // index joinable predicate���� ��밡������ �˻�.
    //--------------------------------------

    sRightGraph = aLeftChildGraph;

    // PROJ-1502 PARTITIONED DISK TABLE
    // left child graph�� selection graph�Ǵ� partition graph���� �˻�.
    if ( ( sRightGraph->type == QMG_SELECTION ) ||
         ( sRightGraph->type == QMG_PARTITION ) )
    {
        IDE_TEST( isIndexable( aStatement,
                               aPredicate,
                               & sRightGraph->depInfo,
                               & qtc::zeroDependencies,
                               & sIsIndexableJoinPred )
                  != IDE_SUCCESS );

        // qmoPredicate.flag��
        // right->left���� ���� ���� ������ �����Ѵ�.
        if ( sIsIndexableJoinPred == ID_TRUE )
        {
            aPredicate->flag |= QMO_PRED_INDEX_JOINABLE_TRUE;
            aPredicate->flag |= QMO_PRED_INDEX_RIGHT_LEFT;
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

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isSortJoinablePredicate( qmoPredicate * aPredicate,
                                  qtcNode      * aNode,
                                  qcDepInfo    * aFromDependencies,
                                  qmgGraph     * aLeftChildGraph,
                                  qmgGraph     * aRightChildGraph )
{
/***********************************************************************
 *
 * Description : sort joinable predicate�� �Ǵ� ( key size limit �˻� )
 *
 * Implementation :
 *
 *     disk sort temp table�� ����ؾ� �ϴ� ���,
 *     index ���� ������ �÷������� ���� �˻縦 �����ϰ�,
 *     index ���� ���� �÷��̸�, sort joinable predicate���� �Ǵ��Ѵ�.
 *
 *     1. index ���� ���� �÷��Ǵ�
 *     2. sort column�� index ���� ���� �÷��̸�,
 *        qmoPredicate.flag�� sort join method ��밡������ ����.
 *
 *     TODO :
 *     host ������ �����ϴ� ���
 *     key size limit �˻縦 execution time�� �Ǵ��ؾ� �Ѵ�.
 *
 ***********************************************************************/

    idBool      sIsSortJoinablePred = ID_TRUE;
    qtcNode   * sCompareNode;
    qmgGraph  * sLeftSortColumnGraph  = NULL;
    qmgGraph  * sRightSortColumnGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::isSortJoinablePredicate::__FT__" );

    //--------------------------------------
    // sort joinable �Ǵ�
    //--------------------------------------

    // sCompareNode�� �񱳿����� ���
    sCompareNode = aNode;

    //--------------------------------------
    // BUG-15849
    // equal�� ����������, sort join�� ����� �� ���� ��찡 �ִ�.
    // ��) stfContains, stfCrosses, stfEquals, stfIntersects,
    //     stfOverlaps, stfTouches, stfWithin
    //--------------------------------------
    if ( ( sCompareNode->node.module == &mtfEqual )
         || ( sCompareNode->node.module == &mtfGreaterThan )
         || ( sCompareNode->node.module == &mtfGreaterEqual )
         || ( sCompareNode->node.module == &mtfLessThan )
         || ( sCompareNode->node.module == &mtfLessEqual ) )
    {
        // �񱳿����ڰ� equal���� ���,

        if ( ( ( sCompareNode->node.arguments->lflag &
                 MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST ) ||
             ( ( sCompareNode->node.arguments->next->lflag &
                 MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST ) )
        {
            // BUGBUG: List type�� join key�� ��� ó�� �ʿ�
            sIsSortJoinablePred = ID_FALSE;
        }
        else
        {
            // �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
            IDE_TEST( findChildGraph( sCompareNode,
                                      aFromDependencies,
                                      aLeftChildGraph,
                                      aRightChildGraph,
                                      &sLeftSortColumnGraph,
                                      & sRightSortColumnGraph )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // �񱳿����ڰ� equal���� �ƴ� ���,

        sIsSortJoinablePred = ID_FALSE;
    }

    if ( sIsSortJoinablePred == ID_TRUE )
    {
        aPredicate->flag &= ~QMO_PRED_SORT_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_SORT_JOINABLE_TRUE;
    }
    else
    {
        aPredicate->flag &= ~QMO_PRED_SORT_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_SORT_JOINABLE_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isHashJoinablePredicate( qmoPredicate * aPredicate,
                                  qtcNode      * aNode,
                                  qcDepInfo    * aFromDependencies,
                                  qmgGraph     * aLeftChildGraph,
                                  qmgGraph     * aRightChildGraph )
{
/***********************************************************************
 *
 * Description : Hash joinable predicate�� �Ǵ� ( key size limit �˻� )
 *
 * Implementation :
 *
 *     disk hash temp table�� ����ؾ� �ϴ� ���,
 *     index ���� ������ �÷������� ���� �˻縦 �����ϰ�,
 *     index ���� ���� �÷��̸�, hash joinable predicate���� �Ǵ��Ѵ�.
 *
 *     1. hash column�� index ���� ���� �÷� �Ǵ�
 *     2. hash column�� index ���� ���� �÷��̸�,
 *        qmoPredicate.flag�� hash join method ��밡������ ����.
 *
 *     TODO :
 *     (1) host ������ �����ϴ� ���
 *         key size limit �˻縦 execution time�� �Ǵ��ؾ� �Ѵ�.
 *     (2) hash key column�� �������� ���
 *
 ***********************************************************************/

    idBool      sIsHashJoinablePred = ID_TRUE;
    qtcNode   * sCompareNode;
    qmgGraph  * sLeftHashColumnGraph  = NULL;
    qmgGraph  * sRightHashColumnGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::isHashJoinablePredicate::__FT__" );

    //--------------------------------------
    // Hash joinable �Ǵ�
    //--------------------------------------

    // TODO :
    // �� ���������� hash column�� �ϳ��� ��쿡 ���ؼ���
    // index �������� ���θ� �˻��ϴµ�,
    // hash column�� ������ ���� ���,
    // index ������ �� �� ���� ��찡 ���� �� �ִ�.

    // sCompareNode�� �񱳿����� ���
    sCompareNode = aNode;

    if ( sCompareNode->node.module == &mtfEqual )
    {
        // �񱳿����ڰ� equal�� ���,

        if ( ( ( sCompareNode->node.arguments->lflag &
                 MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST ) ||
             ( ( sCompareNode->node.arguments->next->lflag &
                 MTC_NODE_OPERATOR_MASK )
               == MTC_NODE_OPERATOR_LIST ) )
        {
            // BUGBUG: List type�� join key�� ��� ó�� �ʿ�
            sIsHashJoinablePred = ID_FALSE;
        }
        else
        {
            // �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
            IDE_TEST( findChildGraph( sCompareNode,
                                      aFromDependencies,
                                      aLeftChildGraph,
                                      aRightChildGraph,
                                      & sLeftHashColumnGraph,
                                      & sRightHashColumnGraph )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // �񱳿����ڰ� equal�� �ƴ� ���,

        sIsHashJoinablePred = ID_FALSE;
    }

    if ( sIsHashJoinablePred == ID_TRUE )
    {
        aPredicate->flag &= ~QMO_PRED_HASH_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_HASH_JOINABLE_TRUE;
    }
    else
    {
        aPredicate->flag &= ~QMO_PRED_HASH_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_HASH_JOINABLE_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::isMergeJoinablePredicate( qmoPredicate * aPredicate,
                                   qtcNode      * aNode,
                                   qcDepInfo    * aFromDependencies,
                                   qmgGraph     * aLeftChildGraph,
                                   qmgGraph     * aRightChildGraph )
{
/***********************************************************************
 *
 * Description : merge joinable predicate�� �Ǵ� (���⼺ �˻�)
 *
 * Implementation :
 *
 *      [ �ε��� ���� ���� ���� �˻� ]
 *      sort joinable �Ǵܽÿ� �����ϴ�.
 *      ����, �̹� sort joinable �Ǵܽ� �ε��� ���� ���� ���ΰ�
 *      �ǴܵǾ����Ƿ�, QMO_PRED_SORT_JOINABLE_TRUE�̸�,
 *      merge join method ��� ���ɰ� ������⿡ ���� ������ �����Ѵ�.
 *
 *      merge join�� [=, >, >=, <, <=]�񱳿����ڿ� ���� ���డ���ϸ�,
 *      �� �����ڿ� ���� ���⼺ �˻縦 �����Ѵ�.
 *
 *      1) Semi/anti join�� ���
 *        �����ڿ� ���� ���� left->right�� ���
 *      2) Inner join�� ���
 *        1. equal(=) �񱳿�����
 *           left->right, right->left ��� ���డ��
 *
 *        2. [ >, >=, <, <= ] �񱳿�����
 *           (1) [ >, >= ] : left->right�� ���డ��
 *           (2) [ <, <= ] : right->left�� ���డ��
 *
 ***********************************************************************/

    qtcNode   * sCompareNode;
    qmgGraph  * sLeftMergeColumnGraph  = NULL;
    qmgGraph  * sRightMergeColumnGraph = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::isMergeJoinablePredicate::__FT__" );

    //--------------------------------------
    // merge joinable �Ǵ�
    //--------------------------------------

    //--------------------------------------
    // merge join�� �ε��� �������� ���δ� sort joinable �Ǵܽÿ� �����ϴ�.
    // ����, QMO_PRED_SORT_JOINABLE_TRUE �̸�,
    // merge join ������ �ε��� ���� ������ ����̹Ƿ�,
    // merge join method ��밡�ɿ��ο� ���� ���� ������ �Ǵ��Ѵ�.
    //--------------------------------------

    if ( ( aPredicate->flag & QMO_PRED_SORT_JOINABLE_MASK )
         == QMO_PRED_SORT_JOINABLE_TRUE )
    {
        //-------------------------------------
        // �ε��� ���������� ����, merge join�� ���� ������ ����.
        //-------------------------------------

        aPredicate->flag &= ~QMO_PRED_MERGE_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_MERGE_JOINABLE_TRUE;

        sCompareNode = aNode;

        if ( sCompareNode->node.module == &mtfEqual )
        {
            // equal �񱳿����ڴ� left->right, right->left ��� ���డ���ϴ�.
            aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
            aPredicate->flag |= QMO_PRED_MERGE_LEFT_RIGHT;
            aPredicate->flag |= QMO_PRED_MERGE_RIGHT_LEFT;
        }
        else
        {
            // [ >, >=, <, <= ] �񱳿�����
            // . [ >, >= ] : left->right�� ���డ��
            // . [ <, <= ] : right->left�� ���డ��

            // �񱳿����� �� ���� ��忡 �ش��ϴ� graph�� ã�´�.
            IDE_TEST( findChildGraph( sCompareNode,
                                      aFromDependencies,
                                      aLeftChildGraph,
                                      aRightChildGraph,
                                      & sLeftMergeColumnGraph,
                                      & sRightMergeColumnGraph )
                      != IDE_SUCCESS );

            if ( ( sCompareNode->node.module == &mtfGreaterThan )
                 || ( sCompareNode->node.module == &mtfGreaterEqual ) )
            {
                // >, >=

                if ( sLeftMergeColumnGraph == aLeftChildGraph )
                {
                    // ��:    [join]     ,         [>]
                    //          |                   |
                    //      [T1]   [T2]     [t1.i1]   [t2.i1]
                    // merge join ��������� left->right
                    aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
                    aPredicate->flag |= QMO_PRED_MERGE_LEFT_RIGHT;
                }
                else
                {
                    // ��:    [join]     ,         [>]
                    //          |                   |
                    //      [T1]   [T2]     [t2.i1]   [t1.i1]
                    // merge join ��������� right->left
                    aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
                    aPredicate->flag |= QMO_PRED_MERGE_RIGHT_LEFT;
                }
            }
            else
            {
                // <, <=

                if ( sLeftMergeColumnGraph == aLeftChildGraph )
                {
                    // ��:    [join]     ,         [<]
                    //          |                   |
                    //      [T1]   [T2]     [t1.i1]   [t2.i1]
                    // merge join ��������� right->left
                    aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
                    aPredicate->flag |= QMO_PRED_MERGE_RIGHT_LEFT;
                }
                else
                {
                    // ��:    [join]     ,         [<]
                    //          |                   |
                    //      [T1]   [T2]     [t2.i1]   [t1.i1]
                    // merge join ��������� left->right
                    aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
                    aPredicate->flag |= QMO_PRED_MERGE_LEFT_RIGHT;
                }
            }
        } // �񱳿����ڰ� [ >, >=, <, <= ]�� ���
    } // QMO_PRED_SORT_JOINABLE_TRUE�� ���(��, �ε��� ���������� ���)
    else
    {
        //-------------------------------------
        // QMO_PRED_SORT_JOINABLE_FALSE�� ���(��, �ε��� ���� �Ұ����� ���)
        //-------------------------------------

        aPredicate->flag &= ~QMO_PRED_MERGE_JOINABLE_MASK;
        aPredicate->flag |= QMO_PRED_MERGE_JOINABLE_FALSE;

        aPredicate->flag &= ~QMO_PRED_MERGE_DIRECTION_MASK;
        aPredicate->flag |= QMO_PRED_MERGE_NON_DIRECTION;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::extractKeyRange( qcmIndex        * aIndex,
                          qmoPredWrapper ** aSource,
                          qmoPredWrapper ** aKeyRange,
                          qmoPredWrapper ** aKeyFilter,
                          qmoPredWrapper ** aSubqueryFilter )
{
/***********************************************************************
 *
 * Description : keyRange ����
 *
 *     index nested loop join predicate�� ������ ���,
 *     index nested loop join predicate�� keyRange�� ���õǵ��� �Ѵ�.
 *
 * Implementation :
 *
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPred::extractKeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aKeyRange != NULL );
    IDE_DASSERT( aKeyFilter != NULL );
    IDE_DASSERT( aSubqueryFilter != NULL );

    //--------------------------------------
    // keyRange ����
    //--------------------------------------

    if ( *aSource != NULL )
    {
        if ( ( (*aSource)->pred->flag & QMO_PRED_INDEX_NESTED_JOINABLE_MASK )
             == QMO_PRED_INDEX_NESTED_JOINABLE_TRUE )
        {
            //--------------------------------------
            // index nested loop join predicate�� �����ϴ� ���,
            // LIST ó���� keyRange�� �з��� predicate�� �ִٸ�,
            // (1) IN subquery�� ���, subquery filter�� ����
            // (2) IN subquery�� �ƴ� ���, keyFilter�� �����ϰ�
            // index nested loop join predicate�� keyRange��
            // ���õǵ��� �Ѵ�.
            //--------------------------------------

            if ( *aKeyRange == NULL )
            {
                // Nothing To Do
            }
            else
            {
                if ( ( (*aKeyRange)->pred->flag & QMO_PRED_INSUBQUERY_MASK )
                     == QMO_PRED_INSUBQUERY_EXIST )
                {
                    IDE_TEST( qmoPredWrapperI::moveAll( aKeyRange,
                                                        aSubqueryFilter )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( qmoPredWrapperI::moveAll( aKeyRange,
                                                        aKeyFilter )
                              != IDE_SUCCESS );
                }
            }

            IDE_TEST( extractKeyRange4Column( aIndex,
                                              aSource,
                                              aKeyRange )
                      != IDE_SUCCESS );
        }
        else
        {
            // index nested loop join predicate�� �������� �ʴ� ���,

            //--------------------------------------
            // LIST ó����, keyRange�� �з��� predicate��
            // (1) �����ϸ�, one column�� ���� keyRange�� �������� �ʰ�,
            // (2) �������� ������, one column�� ���� keyRange�� �����Ѵ�.
            //--------------------------------------

            if ( *aKeyRange == NULL )
            {
                //--------------------------------------
                // one column�� ���� keyRange ����
                //--------------------------------------
                IDE_TEST( extractKeyRange4Column( aIndex,
                                                  aSource,
                                                  aKeyRange )
                          != IDE_SUCCESS );
            }
            else
            {
                // LIST �÷��� ���� keyRange�� �ִ� ���,
                // one column�� ���� keyRange ������ skip

                // Nothing To Do
            }
        }
    }
    else
    {
        // source�� �����Ƿ� �Ұ� ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::extractKeyRange4Column( qcmIndex        * aIndex,
                                 qmoPredWrapper ** aSource,
                                 qmoPredWrapper ** aKeyRange )
{
/***********************************************************************
 *
 * Description : one column�� ���� keyRange�� �����Ѵ�.
 *
 *  [join index ����ȭ�� ���Ͽ� join predicate�� �켱 ����.(BUG-7098)]
 *   join index ����ȭ�� ���� index nested joinable predicate�� ������,
 *   �� join predicate�� one table predicate�� �켱�Ͽ� keyRange�� �����ؾ�
 *   �Ѵ�. ���� predicate ��ġ�� index joinable predicate�� �ٸ� predicate����
 *   �տ� ��ġ�� �ְ�, �ε����� joinable predicate�� column�� ����������
 *   ����� �� �ִ� ���̹Ƿ�, �ڿ������� joinable predicate�� �켱������
 *   ó���� �� �����Ƿ�, �̿� ���� ������ �˻縦 ���� �ʾƵ� �ȴ�.
 *
 *  [���� �÷��� �÷�����Ʈ�� ������ ���� �����Ǿ� �ִ�.]
 *   . IN(subquery) �ϳ��� ����
 *   . IN(subquery)�� �������� �ʴ� predicate���� ����
 *  ������, IN(subquery)�� ���� predicate�ϳ��� ���ؼ��� keyRange�� ������ ��
 *  �����Ƿ�, keyRange ������ �����ϰ� �ϱ� ����, predicate���ġ�� ������,
 *  selectivity�� ���� predicate�� �������� �̿� ���� ���� �÷��� ����Ʈ��
 *  �����Ѵ�. (���� qmoPred::processIndexableInSubQ())
 *
 * Implementation :
 *
 *   �ε��� �÷�������� �Ʒ��� �۾� ����
 *
 *   1. �ε��� �÷��� ���� �÷��� �÷�����Ʈ�� ã�´�.
 *      (1) ���� keyRange�� �з��� predicate�� ���� ���,
 *          keyRange�� ã�� predicate ����
 *          A. IN subquery �̸�, ���� �ε��� �÷����� �������� �ʴ´�.
 *          B. IN subquery�� �ƴϸ�, ���� �ε��� �÷����� ����.
 *      (2) �̹� keyRange�� �з��� predicate�� �����ϴ� ���,
 *          A. IN subquery �̸�,
 *             keyRange �� �������� �ʰ�,
 *             ���� �ε��� �÷����ε� �������� �ʴ´�.
 *          B. IN subquery�� �ƴϸ�,
 *             keyRange �� �����ϰ�, ���� �ε��� �÷����� ����.
 *   2. ã�� �÷���
 *      (1) ���� �ε��� �÷� ��� ������ �÷��̸� ( equal(=), in )
 *      (2) ���� �ε��� �÷� ��� �Ұ����� �÷��̸�, (equal���� predicate����)
 *
 *
 ***********************************************************************/

    UInt             sIdxColumnID;
    UInt             sKeyColCount;
    qmoPredWrapper * sWrapperIter;
    idBool           sIsOrConstPred = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoPred::extractKeyRange4Column::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aKeyRange != NULL );

    //--------------------------------------
    // KeyRange ����
    //--------------------------------------

    //--------------------------------------
    // �ε��� �÷�������� �ε��� �÷��� ������ �÷��� ã�Ƽ�
    // keyRange���� ���θ� �����Ѵ�.
    // 1. �ε��� �÷��� ���������� ��밡���Ͽ��� �Ѵ�.
    //    ��: index on T1(i1, i2, i3)
    //      (1) i1=1 and i2=1 and i3=1 ==> i1,i2,i3 ��� ��� ����
    //      (2) i1=1 and i3=1          ==> i1�� ��� ����
    // 2. IN subquery keyRange�� �ܵ����� �����ؾ� �Ѵ�.
    //    ��: index on T1(i1, i2, i3, i4, i5)
    //      (1) i1 in ( subquery ) and i2=1  ==> i1�� ��� ����
    //      (2) i1=1 and i2=1 and i3 in (subquery) and i4=1 and i5=1
    //          ==> i1, i2�� ��� ����
    //--------------------------------------

    for ( sKeyColCount = 0;
          sKeyColCount < aIndex->keyColCount;
          sKeyColCount++ )
    {
        // �ε��� �÷��� columnID�� ���Ѵ�.
        sIdxColumnID = aIndex->keyColumns[sKeyColCount].column.id;

        //--------------------------------------------
        // join index ����ȭ�� ���� index nested joinable predicate�� ������,
        // �� join predicate�� one table predicate�� �켱�Ͽ�
        // keyRange�� �����ؾ� �Ѵ�.
        // ���� predicate ��ġ�� index joinable predicate��
        // �ٸ� predicate���� �տ� ��ġ�� �ְ�,
        // �ε����� joinable predicate�� column�� ���������� ����� �� �ִ�
        // ���̹Ƿ�, �ڿ������� joinable predicate�� �켱������ ó���� ��
        // �����Ƿ�, �̿� ���� ������ �˻縦 ���� �ʾƵ� �ȴ�.
        //--------------------------------------------

        // �ε��� �÷��� ���� columnID�� ã�´�.
        for ( sWrapperIter  = *aSource;
              sWrapperIter != NULL;
              sWrapperIter  = sWrapperIter->next )
        {
            if ( sIdxColumnID == sWrapperIter->pred->id )
            {
                /* BUG-42172  If _PROWID pseudo column is compared with a column having
                 * PRIMARY KEY constraint, server stops abnormally.
                 */
                if ( ( sWrapperIter->pred->node->lflag &
                       QTC_NODE_COLUMN_RID_MASK )
                     != QTC_NODE_COLUMN_RID_EXIST )
                {
                    break;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sWrapperIter == NULL )
        {
            // ���� �ε��� �÷��� ������ �÷��� predicate�� �������� �ʴ� ���
            break;
        }
        else
        {
            // ���� �ε��� �÷��� ������ �÷��� predicate�� �����ϴ� ���

            // ���� predicate�� IN subquery keyRange�̸�,
            if ( ( sWrapperIter->pred->flag & QMO_PRED_INSUBQUERY_MASK )
                 == QMO_PRED_INSUBQUERY_EXIST )
            {
                // ù��° range�� �з��� �Ÿ� range�� �߰��ϰ�
                // �׷��� ������ range�� �߰����� �ʴ´�.
                if ( ( *aKeyRange == NULL ) &&
                     ( aIndex->indexHandle != NULL ) )
                {
                    IDE_TEST( qmoPredWrapperI::moveWrapper( sWrapperIter,
                                                            aSource,
                                                            aKeyRange )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing To Do
                }

                // in subquery�� ��� ���̻� �������� �ʴ´�.
                break;
            }
            else
            {
                /* BUG-47836 */
                if ( sIsOrConstPred == ID_FALSE )
                {
                    /* BUG-47509 */
                    if ( ( sWrapperIter->pred->flag & QMO_PRED_OR_VALUE_INDEX_MASK )
                         == QMO_PRED_OR_VALUE_INDEX_FALSE )
                    {
                        IDE_TEST( qmoPredWrapperI::moveWrapper( sWrapperIter,
                                                                aSource,
                                                                aKeyRange )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        if ( ( *aKeyRange == NULL ) &&
                             ( aIndex->indexHandle != NULL ) )
                        {
                            IDE_TEST( qmoPredWrapperI::moveWrapper( sWrapperIter,
                                                                    aSource,
                                                                    aKeyRange )
                                      != IDE_SUCCESS );
                            sIsOrConstPred = ID_TRUE;
                        }
                    }
                }
            }

            if ( ( sWrapperIter->pred->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                 == QMO_PRED_NEXT_KEY_USABLE )
            {
                // �� �÷��� equal(=)�� in predicate������ �����Ǿ� �ִ�.
                // ���� �ε��� �÷����� ����.
                // Nothing To Do
            }
            else
            {
                // �� �÷��� equal(=)/in �̿��� predicate�� �����ϰ� �����Ƿ�,
                // ���� �ε��� �÷��� ����� �� ����.

                // ���� �ε������� �۾��� �������� �ʴ´�.
                break;

            }
        } // �ε��� �÷��� ���� �÷��� ���� predicate�� ���� ���,
    } // index column for()��

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::extractKeyFilter( idBool            aIsMemory,
                           qcmIndex        * aIndex,
                           qmoPredWrapper ** aSource,
                           qmoPredWrapper ** aKeyRange,
                           qmoPredWrapper ** aKeyFilter,
                           qmoPredWrapper ** aFilter )
{
/***********************************************************************
 *
 * Description :  keyFilter�� ����
 *
 *
 * Implementation :
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPred::extractKeyFilter::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aKeyRange != NULL );
    IDE_DASSERT( aKeyFilter != NULL );
    IDE_DASSERT( aFilter != NULL );

    //--------------------------------------
    // KeyFilter ����
    //--------------------------------------

    if ( ( aIsMemory == ID_FALSE ) &&
         ( *aKeyRange != NULL ) )
    {
        if ( ( (*aKeyRange)->pred->flag & QMO_PRED_INSUBQUERY_MASK )
             == QMO_PRED_INSUBQUERY_ABSENT )
        {
            //----------------------------------
            // disk table�̰�,
            // keyRange�� �����ϰ�,
            // keyRange�� IN subquery keyRange�� �ƴ� ���,
            // keyFilter ����
            //----------------------------------

            // To fix BUG-27401
            // list���� �̹� keyfilter�� ����Ǿ� �ִ� ���
            // keyfilter�� �������� �ʴ´�.
            if ( *aKeyFilter == NULL )
            {
                IDE_TEST( extractKeyFilter4Column( aIndex,
                                                   aSource,
                                                   aKeyFilter )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing to do.
            }
        }
        else
        {
            // �� �̻� ó���� predicate�� ���� ���
            // Nothing To Do
        }
    }
    else
    {
        //----------------------------------
        // keyRange�� �������� �ʰų�,
        // memory table�� ���, keyFilter�� �������� �ʴ´�.
        // LIST ���� ������ keyFilter�� �ִ� ���,
        // filter�� �����Ѵ�.
        //----------------------------------

        IDE_TEST( qmoPredWrapperI::moveAll( aKeyFilter, aFilter )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::extractKeyFilter4Column( qcmIndex        * aIndex,
                                  qmoPredWrapper ** aSource,
                                  qmoPredWrapper ** aKeyFilter )
{
/***********************************************************************
 *
 * Description : one column�� ���� keyFilter�� �����Ѵ�.
 *
 *  memory table�� ���ؼ��� keyFilter�� �������� �ʴ´�.
 *  keyFilter�� �ε����� �������� ������ ������� �ε����� ���ԵǱ⸸
 *  �ϸ�ȴ�.
 *
 * Implementation :
 *
 *   disk table�� ���,
 *   1. �ε��� �÷��� ������ �÷��� �÷�����Ʈ�� ã�´�.
 *   2. ã�� �÷�����Ʈ��
 *      (1) IN(subquery)�� �����ϸ�,
 *          �ƹ� ó���� ���� �ʰ�, ���� �ε��� �÷� ����.
 *      (2) IN(subquery)�� �������� ������,
 *          keyFilter �� �����ϰ�, ���� �ε��� �÷� ����.
 *
 ***********************************************************************/

    UInt             sIdxColumnID;
    UInt             sKeyColCount;
    qmoPredWrapper * sWrapperIter;

    IDU_FIT_POINT_FATAL( "qmoPred::extractKeyFilter4Column::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aKeyFilter != NULL );

    //--------------------------------------
    // KeyFilter ����
    //--------------------------------------

    for ( sKeyColCount = 0;
          sKeyColCount < aIndex->keyColCount && *aSource != NULL;
          sKeyColCount++ )
    {
        // �ε��� �÷��� columnID�� ���Ѵ�.
        sIdxColumnID = aIndex->keyColumns[sKeyColCount].column.id;

        // �ε��� �÷��� ���� columnID�� ã�´�.
        for ( sWrapperIter  = *aSource;
              sWrapperIter != NULL;
              sWrapperIter  = sWrapperIter->next )
        {
            if ( sIdxColumnID == sWrapperIter->pred->id )
            {
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sWrapperIter == NULL )
        {
            // �ε��� �÷��� ������ �÷��� predicate�� �������� �ʴ� ���,
            // ���� �ε��� �÷����� ����
            // keyFilter�� �������� �ε��� ��뿡 �������
            // �ε��� �÷��� ���ϱ⸸ �ϸ� �ȴ�.

            // Nothing To Do
        }
        else
        {
            // �ε��� �÷��� ������ �÷��� predicate�� �����ϴ� ���,

            if ( ( sWrapperIter->pred->flag & QMO_PRED_INSUBQUERY_MASK )
                 == QMO_PRED_INSUBQUERY_EXIST )
            {
                // �ε��� �÷��� ���� columnID�� ���� �÷��� predicate��
                // IN subquery�� ���,
                // IN subquery�� keyFilter�� ������� ���ϹǷ�, skip

                // Nothing To Do
            }
            else
            {
                // �ε��� �÷��� ���� columnID�� ���� �÷��� predicate��
                // IN subquery�� �ƴ� ���,
                // ��, keyFilter�� ����� �� �ִ� predicate
                /* BUG-47986 */
                if ( ( sWrapperIter->pred->flag & QMO_PRED_JOIN_OR_VALUE_INDEX_MASK )
                     == QMO_PRED_JOIN_OR_VALUE_INDEX_FALSE )
                {
                    // keyFilter�� ����
                    IDE_TEST( qmoPredWrapperI::moveWrapper( sWrapperIter,
                                                            aSource,
                                                            aKeyFilter )
                              != IDE_SUCCESS );
                }
            }

            // fix BUG-10091
            // = �̿��� �񱳿����ڰ� �����ϸ�
            // keyFilter ������ �����.
            if ( ( sWrapperIter->pred->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                 == QMO_PRED_NEXT_KEY_USABLE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::extractRange4LIST( qcTemplate         * aTemplate,
                            qcmIndex           * aIndex,
                            qmoPredWrapper    ** aSource,
                            qmoPredWrapper    ** aKeyRange,
                            qmoPredWrapper    ** aKeyFilter,
                            qmoPredWrapper    ** aFilter,
                            qmoPredWrapper    ** aSubqueryFilter,
                            qmoPredWrapperPool * aWrapperPool )
{
/***********************************************************************
 *
 * Description : LIST�� ����
 *               keyRange, keyFilter, filter, subqueryFilter�� �����Ѵ�.
 *
 * Implementation :
 *
 *   �� �Լ��� ����Ʈ �÷�����Ʈ�� ���ڷ� �޾Ƽ�,
 *   �ϳ��� ����Ʈ�� ���� keyRange/keyFilter/filter/subqueryFilter��
 *   �Ǵ��ؼ� �ش��ϴ� ���� �����Ѵ�.
 *
 *      ����Ʈ�� �ε��� ��밡�ɼ� �˻�.
 *      (1) ����Ʈ�÷��� �ε��� �÷��� ���������� ���ԵǸ�, keyRange�� �з�
 *      (2) ������������ ������, ����Ʈ �÷��� ��� �ε��� �÷��� ���ԵǸ�,
 *          keyFilter�� �з�
 *      (3) (1)�� (2)�� �ƴϸ�, filter�� �з�
 *
 ***********************************************************************/

    qmoPredWrapper * sWrapperIter;
    qmoPredicate   * sPredIter;
    qmoPredType      sPredType;

    IDU_FIT_POINT_FATAL( "qmoPred::extractRange4LIST::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aIndex != NULL );
    IDE_DASSERT( aSource != NULL );
    IDE_DASSERT( aKeyRange != NULL );
    IDE_DASSERT( aKeyFilter != NULL );
    IDE_DASSERT( aFilter != NULL );
    IDE_DASSERT( aSubqueryFilter != NULL );

    //--------------------------------------
    // ����Ʈ �÷�����Ʈ�� �޷��ִ� ������ �÷��� ����
    // keyRange/keyFilter/filter/IN(subquery)�� ���� �Ǵ��� ����.
    //--------------------------------------

    for ( sWrapperIter  = *aSource;
          sWrapperIter != NULL;
          sWrapperIter  = sWrapperIter->next )
    {
        if ( sWrapperIter->pred->id == QMO_COLUMNID_LIST )
        {
            IDE_TEST( qmoPredWrapperI::extractWrapper( sWrapperIter,
                                                       aSource )
                      != IDE_SUCCESS );

            break;
        }
        else
        {
            // Nothing to do...
        }
    }

    //--------------------------------------
    // KeyRange, KeyFilter, filter, subqueryFilter ����
    //--------------------------------------

    if ( sWrapperIter != NULL )
    {
        for ( sPredIter  = sWrapperIter->pred;
              sPredIter != NULL;
              sPredIter  = sPredIter->more )
        {
            IDE_TEST( checkUsableIndex4List( aTemplate,
                                             aIndex,
                                             sPredIter,
                                             & sPredType )
                      != IDE_SUCCESS );

            if ( sPredType == QMO_KEYRANGE )
            {
                IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                    aKeyRange,
                                                    aWrapperPool )
                          != IDE_SUCCESS );
            }
            else
            {
                // keyFilter�� filter�� �з��� ���,
                // �� ���, IN subquery�� subqueryFilter�� �����ؾ� �Ѵ�.

                if ( ( sPredIter->flag & QMO_PRED_INSUBQUERY_MASK )
                     == QMO_PRED_INSUBQUERY_EXIST )
                {
                    // IN subquery�̸�, subqueryFilter�� ����
                    IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                        aSubqueryFilter,
                                                        aWrapperPool )
                              != IDE_SUCCESS );
                }
                else
                {
                    if ( sPredType == QMO_KEYFILTER )
                    {
                        // keyFilter�� �з��� ����, keyFilter�� ����
                        IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                            aKeyFilter,
                                                            aWrapperPool )
                                  != IDE_SUCCESS );
                    }
                    else
                    {
                        // BUG-11328 fix
                        // ���� �ִ� predicate�� ����
                        // filter ���� subquery filter����
                        // �Ǵ��ؾ� �Ѵ�.
                        // �������� �� ������ filter�θ� ó���ߴ�.
                        if ( ( sPredIter->node->lflag & QTC_NODE_SUBQUERY_MASK )
                             == QTC_NODE_SUBQUERY_EXIST )
                        {
                            IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                                aSubqueryFilter,
                                                                aWrapperPool )
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                                aFilter,
                                                                aWrapperPool )
                                      != IDE_SUCCESS );
                        }
                    }
                }
            }
        } // for
    }
    else // list predicate�� ���� ��� �ƹ��� �۾��� ���� �ʴ´�.
    {
        // Nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::process4Range( qcStatement        * aStatement,
                        qmsQuerySet        * aQuerySet,
                        qmoPredicate       * aRange,
                        qmoPredWrapper    ** aFilter,
                        qmoPredWrapperPool * aWrapperPool )
{
/***********************************************************************
 *
 * Description : keyRange�� keyFilter�� �з��� predicate�� ���� ó��
 *
 *       (1) quantify �񱳿����ڿ� ���� ��� ��ȯ ����
 *       (2) LIKE �񱳿����ڿ� ���� filter ����
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;
    qmoPredicate  * sMorePredicate;
    qmoPredicate  * sLikeFilter = NULL;
    qmsProcessPhase sOrgPhase;

    IDU_FIT_POINT_FATAL( "qmoPred::process4Range::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aRange != NULL );
    IDE_DASSERT( aFilter != NULL );

    //------------------------------------------
    // Range�� ���� ó��
    //------------------------------------------

    sOrgPhase = aQuerySet->processPhase;
    aQuerySet->processPhase = QMS_OPTIMIZE_NODE_TRANS;

    for ( sPredicate  = aRange;
          sPredicate != NULL;
          sPredicate  = sPredicate->next )
    {
        if ( sPredicate->id == QMO_COLUMNID_LIST )
        {
            for ( sMorePredicate  = sPredicate;
                  sMorePredicate != NULL;
                  sMorePredicate  = sMorePredicate->more )
            {
                //------------------------------------------
                // quantify �񱳿����ڿ� ���� ��� ��ȯ
                //------------------------------------------

                IDE_TEST( nodeTransform( aStatement,
                                         sMorePredicate->node,
                                         & sMorePredicate->node,
                                         aQuerySet )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            if ( ( sPredicate->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                 == QMO_PRED_NEXT_KEY_USABLE )
            {
                //------------------------------------------
                // one column predicate�̸鼭, �����ε��� ��밡���� ���
                // ��庯ȯ�� �����ϸ� ��.
                //------------------------------------------
                for ( sMorePredicate  = sPredicate;
                      sMorePredicate != NULL;
                      sMorePredicate  = sMorePredicate->more )
                {
                    //------------------------------------------
                    // quantify �񱳿����ڿ� ���� ��� ��ȯ
                    //------------------------------------------
                    IDE_TEST( nodeTransform( aStatement,
                                             sMorePredicate->node,
                                             & sMorePredicate->node,
                                             aQuerySet )
                              != IDE_SUCCESS );

                }
            }
            else
            {
                //------------------------------------------
                // one column predicate�̸鼭,
                // �����ε��� ��밡������ ���� ���
                // (��, equal/in �̿��� �񱳿����ڰ� �ִ� ���)
                //------------------------------------------
                for ( sMorePredicate  = sPredicate;
                      sMorePredicate != NULL;
                      sMorePredicate  = sMorePredicate->more )
                {
                    //------------------------------------------
                    // quantify �񱳿����ڿ� ���� ��� ��ȯ
                    //------------------------------------------

                    IDE_TEST( nodeTransform( aStatement,
                                             sMorePredicate->node,
                                             & sMorePredicate->node,
                                             aQuerySet )
                              != IDE_SUCCESS );

                    //------------------------------------------
                    // To Fix PR-9679
                    // LIKE ��� ���� Filter�� �ݵ�� �ʿ���
                    // �񱳿����ڿ� ���� filter ����
                    //------------------------------------------

                    IDE_TEST( makeFilterNeedPred( aStatement,
                                                  sMorePredicate,
                                                  & sLikeFilter )
                              != IDE_SUCCESS );

                    if ( sLikeFilter == NULL )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        IDE_TEST( qmoPredWrapperI::addPred( sLikeFilter,
                                                            aFilter,
                                                            aWrapperPool )
                                  != IDE_SUCCESS );
                    }
                }
            }
        }
    }

    aQuerySet->processPhase = sOrgPhase;

    //------------------------------------------
    // variable / fixed / IN subquery KeyRange ���� ����
    //------------------------------------------

    IDE_TEST( setRangeInfo( aQuerySet, aRange )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::separateFilters( qcTemplate         * aTemplate,
                          qmoPredWrapper     * aSource,
                          qmoPredWrapper    ** aFilter,
                          qmoPredWrapper    ** aLobFilter,
                          qmoPredWrapper    ** aSubqueryFilter,
                          qmoPredWrapperPool * aWrapperPool )
{
/***********************************************************************
 *
 * Description : qmoPredicate ����Ʈ���� filter, subqueryFilter�� �и��Ѵ�.
 *
 *   extractRangeAndFilter()�Լ�����
 *   1. �ε��� ������ ���� ���,
 *      ���ڷ� �Ѿ�� predicate�� filter�� subqueryFilter�� �и��ϱ� ���ؼ�
 *   2. �ε��� ������ ���� ���,
 *      keyRange, keyFilter�� ��� �����ϰ� ���� predicate����
 *      filter�� subqueryFilter�� �и��ϱ� ���ؼ�
 *   �� �Լ��� ȣ���ϰ� �ȴ�.
 *
 * Implementation :
 *
 *    1. indexable predicate�� ����
 *       IN(subquery), subquery KeyRange ����ȭ �� ����
 *    2. subqueryFilter, filter �и�
 *
 ***********************************************************************/

    qmoPredWrapper * sWrapperIter;
    qmoPredicate   * sPredIter;

    IDU_FIT_POINT_FATAL( "qmoPred::separateFilters::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aFilter != NULL );
    IDE_DASSERT( aLobFilter != NULL );
    IDE_DASSERT( aSubqueryFilter != NULL );

    //------------------------------------------
    // ���ڷ� �Ѿ�� predicate list�� ���� �����ؼ�,
    // filter�� subqueryFilter�� �и��Ѵ�.
    //------------------------------------------

    for( sWrapperIter = aSource;
         sWrapperIter != NULL;
         sWrapperIter = sWrapperIter->next )
    {
        for( sPredIter = sWrapperIter->pred;
             sPredIter != NULL;
             sPredIter = sPredIter->more )
        {
            if( ( sPredIter->node->lflag & QTC_NODE_SUBQUERY_MASK )
                == QTC_NODE_SUBQUERY_EXIST )
            {
                //----------------------------
                // subquery�� �����ϴ� ���
                //----------------------------
                IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                    aSubqueryFilter,
                                                    aWrapperPool )
                          != IDE_SUCCESS );
            }
            else
            {
                //-------------------------------
                // subquery�� �������� �ʴ� ���
                //-------------------------------

                if( (QTC_NODE_LOB_COLUMN_EXIST ==
                     (sPredIter->node->lflag & QTC_NODE_LOB_COLUMN_MASK)) &&
                    (aTemplate->stmt->myPlan->parseTree->stmtKind ==
                     QCI_STMT_SELECT_FOR_UPDATE) )
                {
                    /* BUG-25916
                     * clob�� select fot update �ϴ� ���� Assert �߻� */
                    /* lob �÷��� ��� ����ó���ÿ� ����������(getPage)�� �ʿ��ϴ�.
                     * �׷��� select for update�ÿ��� record lock�� �ɱ�����
                     * �̹� �������� X lock�� ���� �����̱� ������,
                     * lob �÷��� ����ó���� �Ҷ� S lock�� ȹ���Ϸ��ٰ�
                     * ASSERT�� �״� ������ �߻��Ѵ�.
                     * �׷��� �� ���(select for update && lob �÷��� �ִ� ���)
                     * lobFilter�� �з��ϰ� SCAN���� FILT ��带 �߰��Ͽ� ó���Ѵ�. */
                    IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                        aLobFilter,
                                                        aWrapperPool )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( qmoPredWrapperI::addPred( sPredIter,
                                                        aFilter,
                                                        aWrapperPool )
                              != IDE_SUCCESS );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::setRangeInfo( qmsQuerySet  * aQuerySet,
                       qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description : keyRange/keyFilter�� ����� predicate�� ���Ḯ��Ʈ ��
 *               �����ֻ�ܿ� fixed/variable/InSubqueryKeyRange�� ����
 *               ������ �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoPredicate * sMorePredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::setRangeInfo::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //------------------------------------------
    //  ���ڷ� �Ѿ�� predicate�� ���Ḯ��Ʈ �� �����ֻ�ܿ�
    //  fixed/variable/IN Subquery keyRange ���� ����
    //------------------------------------------

    for( sPredicate = aPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        for( sMorePredicate = sPredicate;
             sMorePredicate != NULL;
             sMorePredicate = sMorePredicate->more )
        {
            if( ( QMO_PRED_IS_VARIABLE( sMorePredicate ) == ID_TRUE ) ||
                ( ( sMorePredicate->flag & QMO_PRED_VALUE_MASK )
                  == QMO_PRED_VARIABLE ) )
            {
                aPredicate->flag &= ~QMO_PRED_VALUE_MASK;
                aPredicate->flag |= QMO_PRED_VARIABLE;

                if( ( sMorePredicate->flag & QMO_PRED_INSUBQUERY_MASK )
                    == QMO_PRED_INSUBQUERY_EXIST )
                {
                    aPredicate->flag &= ~QMO_PRED_INSUBQ_KEYRANGE_MASK;
                    aPredicate->flag |= QMO_PRED_INSUBQ_KEYRANGE_TRUE;
                }
                else
                {
                    // Nothing To Do
                }

                break;
            }
            else
            {
                // Nothing To Do
            }
        }
    }

    //-------------------------------------
    // fixed �� ���, LEVEL column�� ���Ե� ���,
    // hierarchy query �̸�, variable keyRange��,
    // hierarchy query�� �ƴϸ�, fixed keyRange�� �з��Ǿ�� �Ѵ�.
    //--------------------------------------

    if( aQuerySet->SFWGH->hierarchy != NULL )
    {
        // hierarchy�� �����ϴ� ���

        if( ( aPredicate->flag & QMO_PRED_VALUE_MASK ) == QMO_PRED_FIXED )
        {
            for( sPredicate = aPredicate;
                 sPredicate != NULL;
                 sPredicate = sPredicate->next )
            {
                for( sMorePredicate = sPredicate;
                     sMorePredicate != NULL;
                     sMorePredicate = sMorePredicate->more )
                {
                    if( ( sMorePredicate->node->lflag
                          & QTC_NODE_LEVEL_MASK ) == QTC_NODE_LEVEL_EXIST )
                    {
                        aPredicate->flag &= ~QMO_PRED_VALUE_MASK;
                        aPredicate->flag |= QMO_PRED_VARIABLE;

                        break;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }
            }
        }
        else
        {
            // Nothing To Do
        }
    }
    else
    {
        // hierarchy�� �������� �ʴ� ���,
        // Nothing To Do
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::makeFilterNeedPred( qcStatement   * aStatement,
                             qmoPredicate  * aPredicate,
                             qmoPredicate ** aLikeFilterPred )
{
/***********************************************************************
 *
 * Description :  Like�� ���� Range�� �����ϴ���
 *                Filter�� �ʿ��� ��� �̿� ���� Filter�� �����Ѵ�.
 *
 *     LIKE �񱳿����ڴ� keyRange�� ����Ǿ����ϴ���
 *     'ab%de', 'a_ce' �� ���� ���Ϲ��ڸ� �˻��ϱ� ���� filter�� �ʿ��ϴ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;
    qtcNode       * sLikeNode;
    qtcNode       * sLikeFilterNode = NULL;
    qtcNode       * sLastNode = NULL;
    qtcNode       * sCompareNode;
    qtcNode       * sORNode[2];
    qcNamePosition  sNullPosition;
    idBool          sNeedFilter = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoPred::makeFilterNeedPred::__FT__" );

    //------------------------------------------
    // LIKE�� ���� filter ����
    //------------------------------------------

    if( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        // CNF �� ���

        // OR ��� ������ �񱳿����� ��� �� LIKE�񱳿����ڿ� ����
        // filter ������ �ʿ����� �˻�
        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);

        while( sCompareNode != NULL )
        {
            if( ( sCompareNode->node.lflag & MTC_NODE_FILTER_MASK )
                == MTC_NODE_FILTER_NEED )
            {
                // To Fix BUG-12306
                IDE_TEST( mtf::checkNeedFilter( & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                                & sCompareNode->node,
                                                & sNeedFilter )
                          != IDE_SUCCESS );

                if ( sNeedFilter == ID_TRUE )
                {
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

            sCompareNode = (qtcNode *)(sCompareNode->node.next);
        }

        if ( sNeedFilter == ID_TRUE )
        {
            // BUG-15482, BUG-24843
            // OR ��� ������ �񱳿����� ��� �� LIKE�񱳿����ڿ�
            // �ϳ��� filter�� �ʿ��ϸ� LIKE�� ������ ��� �����ڿ�
            // ���� filter�� �����ؾ� �Ѵ�.
            sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);

            while( sCompareNode != NULL )
            {
                // �񱳿����� ���� ���� ��� ����
                IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM( aStatement ),
                                                 sCompareNode,
                                                 & sLikeNode,
                                                 ID_FALSE,
                                                 ID_FALSE,
                                                 ID_FALSE,
                                                 ID_FALSE )
                          != IDE_SUCCESS );

                if( sLastNode == NULL )
                {
                    sLikeFilterNode = sLikeNode;
                    sLastNode = sLikeFilterNode;
                }
                else
                {
                    sLastNode->node.next = (mtcNode *)&(sLikeNode->node);
                    sLastNode = (qtcNode *)(sLastNode->node.next);
                }

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }

            // BUG-15482
            // ���ο� OR ��带 �ϳ� �����Ѵ�.
            SET_EMPTY_POSITION( sNullPosition );

            IDE_TEST( qtc::makeNode( aStatement,
                                     sORNode,
                                     & sNullPosition,
                                     (const UChar*)"OR",
                                     2 )
                      != IDE_SUCCESS );

            // OR ��� ������ sLikeFilterNode�� �����Ѵ�.
            sORNode[0]->node.arguments = (mtcNode *)sLikeFilterNode;

            IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                        sORNode[0] )
                      != IDE_SUCCESS );

            sLikeFilterNode = sORNode[0];
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // DNF �� ���

        if ( ( aPredicate->node->node.lflag & MTC_NODE_FILTER_MASK )
            == MTC_NODE_FILTER_NEED )
        {
            // To Fix BUG-12306
            IDE_TEST( mtf::checkNeedFilter( & QC_SHARED_TMPLATE( aStatement )->tmplate,
                                            & aPredicate->node->node,
                                            & sNeedFilter )
                      != IDE_SUCCESS );
            if( sNeedFilter == ID_TRUE )
            {
                IDE_TEST( qtc::cloneQTCNodeTree( QC_QMP_MEM(aStatement),
                                                 aPredicate->node,
                                                 & sLikeFilterNode,
                                                 ID_FALSE,
                                                 ID_FALSE,
                                                 ID_FALSE,
                                                 ID_FALSE )
                          != IDE_SUCCESS );
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            // nothing to do
        }
    }

    if( sLikeFilterNode != NULL )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredicate),
                                                 (void **)&sPredicate )
                  != IDE_SUCCESS );

        idlOS::memcpy( sPredicate, aPredicate, ID_SIZEOF(qmoPredicate) );

        sPredicate->node = sLikeFilterNode;
        sPredicate->flag = QMO_PRED_CLEAR;
        sPredicate->id   = QMO_COLUMNID_NON_INDEXABLE;
        sPredicate->more = NULL;
        sPredicate->next = NULL;

        *aLikeFilterPred = sPredicate;
    }
    else
    {
        *aLikeFilterPred = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoPred::nodeTransform( qcStatement  * aStatement,
                        qtcNode      * aSourceNode,
                        qtcNode     ** aTransformNode,
                        qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description : keyRange ������ ���� ��� ��ȯ
 *
 *     LIST �� quantify �񱳿����ڿ� ���ؼ�, keyRange�� �����ϱ� ����
 *     system level operator�� ��庯ȯ��Ų��.
 *
 *     OR ��� ������ one column�� ���, �������� �񱳿����ڰ� �� �� �ְ�,
 *     OR ��� ������ LIST �� �ϳ��� �� �� �ִ�.
 *
 * Implementation :
 *
 *       1. �񱳿����� ��尡 quantify�� ��� ��� ��ȯ
 *           ��: i1 in ( 1, 2 ) or i1 in ( 3, 4)
 *               i1 in ( 1, 2 ) or i1 < 5
 *           ��: i1 in ( 1, 2 )
 *       2. �񱳿����� ��尡 quantify�� �ƴ� ���, LIST�� ��� ��ȯ
 *
 ***********************************************************************/

    idBool    sIsNodeTransformNeed;
    qtcNode * sTransformOneCond;
    qtcNode * sTransformNode = NULL;
    qtcNode * sLastNode      = NULL;
    qtcNode * sCompareNode;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransform::__FT__" );

    if( ( aSourceNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        // CNF�� ����, �ֻ��� ��尡 OR ���
        sCompareNode = (qtcNode *)(aSourceNode->node.arguments);

        //------------------------------------------
        // �ϳ��� �񱳿����� ������ ��� ��ȯ
        // 1. �񱳿����� ��尡 quantify�� ��� ��� ��ȯ
        //    ��: i1 in ( 1, 2 ) or i1 in ( 3, 4)
        //        i1 in ( 1, 2 ) or i1 < 5
        //    ��: i1 in ( 1, 2 )
        // 2. �񱳿����� ��尡 quantify�� �ƴ� ���, LIST�� ��� ��ȯ
        //------------------------------------------

        while( sCompareNode != NULL )
        {
            sIsNodeTransformNeed = ID_TRUE;

            if( ( sCompareNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK )
                == MTC_NODE_GROUP_COMPARISON_TRUE )
            {
                // quantify �񱳿����ڷγ�庯ȯ �ʿ�
                // Nothing To Do
            }
            else
            {
                // quantify �񱳿����ڰ� �ƴ� ���, LIST�� ��� ��ȯ
                if( sCompareNode->indexArgument == 0 )
                {
                    if( ( sCompareNode->node.arguments->lflag &
                          MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_LIST )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsNodeTransformNeed = ID_FALSE;
                    }
                }
                else
                {
                    if( ( sCompareNode->node.arguments->next->lflag &
                          MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_LIST )
                    {
                        // Nothing To Do
                    }
                    else
                    {
                        sIsNodeTransformNeed = ID_FALSE;
                    }
                }
            }

            // ��� ��ȯ ����.
            if( sIsNodeTransformNeed == ID_TRUE )
            {
                IDE_TEST( nodeTransform4OneCond( aStatement,
                                                 sCompareNode,
                                                 & sTransformOneCond,
                                                 aQuerySet )
                          != IDE_SUCCESS );
            }
            else
            {
                sTransformOneCond = sCompareNode;
            }

            // ó���� �񱳿����� ���Ḯ��Ʈ �����
            if( sTransformNode == NULL )
            {
                sTransformNode = sTransformOneCond;
                sLastNode = sTransformNode;
            }
            else
            {
                sLastNode->node.next = (mtcNode *)(sTransformOneCond);
                sLastNode = (qtcNode *)(sLastNode->node.next);
            }

            sCompareNode = (qtcNode *)(sCompareNode->node.next);
        }

        //--------------------------------------
        // ��� ��ȯȯ ������ ���� OR��� ������ �����Ѵ�.
        // ��: i1 in (1,2) OR i1 = 7
        //     �Ʒ��� ���� ��� ��ȯ�Ǵµ�, ����, qtcNode�� �����Ҷ�
        //     �ֻ��� ����� next�� �ִ� ��츦 ����ؾ� �ϹǷ�,
        //     ���⼭, ���� OR ��� ������ ��� ��ȯ�� ��带 ������ �����Ѵ�.
        //
        //                                      OR
        //                                      |
        //      OR ------------ [=]             OR ------------ [=]
        //      |                |              |                |
        //     [=]    - [=]     [i1]-[7]  ==>  [=]    - [=]     [i1]-[7]
        //      |        |                      |        |
        //     [i1]-[1] [i1]-[2]               [i1]-[1] [i1]-[2]
        //--------------------------------------
        aSourceNode->node.arguments = (mtcNode *)&(sTransformNode->node);
        sTransformNode = aSourceNode;
    } // CNF �� ����� ó��
    else
    {
        // DNF�� ����, �ֻ��� ��尡 �񱳿�����

        sIsNodeTransformNeed = ID_TRUE;

        if( ( aSourceNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK )
            == MTC_NODE_GROUP_COMPARISON_TRUE )
        {
            // quantify �񱳿����ڷγ�庯ȯ �ʿ�
            // Nothing To Do
        }
        else
        {
            // quantify �񱳿����ڰ� �ƴ� ���, LIST�� ��� ��ȯ
            if( aSourceNode->indexArgument == 0 )
            {
                if( ( aSourceNode->node.arguments->lflag &
                      MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_LIST )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsNodeTransformNeed = ID_FALSE;
                }
            }
            else
            {
                if( ( aSourceNode->node.arguments->next->lflag &
                      MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_LIST )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsNodeTransformNeed = ID_FALSE;
                }
            }
        }

        // ��� ��ȯ ����.
        if( sIsNodeTransformNeed == ID_TRUE )
        {
            IDE_TEST( nodeTransform4OneCond( aStatement,
                                             aSourceNode,
                                             & sTransformNode,
                                             aQuerySet )
                      != IDE_SUCCESS );
        }
        else
        {
            sTransformNode = aSourceNode;
        }
    } // DNF �� ����� ó��

    *aTransformNode = sTransformNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * qmoPredTrans ���� ����
 *
 *    : nodeTransform() �Լ��� �����ϴ� ��������
 *      ���� ���� �Լ����� ȣ��Ǵµ�
 *      ��� ������ �籸���ϴ� ������ �������� ������ �Ǿ� �ִ�.
 *      nodeTransform() �� ���õ� �Լ����� ȣ��Ǵ� �������̽���
 *      �ܼ�ȭ�ϰ�, �Լ� ������ ���� ���� ������ �� �ְ� �ϱ� ����
 *      �����Լ����� �����丵�Ѵ�.
 *
 *      �����丵�� ������, nodeTransform ���� �Լ��� ������
 *      parameter�� ���⵵�� ���̱� ����
 *      qmoPredTrans��� ����ü�� �����ϰ�
 *      �� ����ü�� operation�� �� �ִ� �Լ����� ��� ����
 *      qmoPredTransI��� class�� �����Ѵ�.
 *
 *      qtcNode�� ������ Ž���ϴ� ������ ���
 *      qmoPredTransI�� �Լ����� ó���ϰ�
 *      nodeTransform ���� �Լ����� ��� ������ ���õ� �������� �����Ѵ�.
 *
 *      by kumdory, 2005-04-25
 *
 **********************************************************************/

void
qmoPredTransI::initPredTrans( qmoPredTrans  * aPredTrans,
                              qcStatement   * aStatement,
                              qtcNode       * aCompareNode,
                              qmsQuerySet   * aQuerySet )
{
/***********************************************************************
 *
 * Description :
 *      aPredTrans�� �� �ɹ��� �ʱ�ȭ�Ѵ�.
 *
 * Implementation :
 *      aStatement�� �����ϰ�,
 *      Logical node�� condition node�� ����� ����
 *      compareNode�� ���۷��̼��� �м��Ѵ�.
 *
 ***********************************************************************/

    IDE_DASSERT( aPredTrans != NULL );

    aPredTrans->statement = aStatement;
    setLogicalNCondition( aPredTrans, aCompareNode );

    // fix BUG-18242
    aPredTrans->myQuerySet = aQuerySet;
}

IDE_RC
qmoPredTransI::makeConditionNode( qmoPredTrans * aPredTrans,
                                  qtcNode      * aArgumentNode,
                                  qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      "=", "<=" ���� condition node �� �����Ѵ�.
 *
 * Implementation :
 *      qmoPredTransI�� private �Լ��� makePredNode�� ����Ѵ�.
 *      ������� condition node�� indexArgument �� 0���� �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makeConditionNode::__FT__" );

    IDE_TEST( makePredNode( aPredTrans->statement,
                            aArgumentNode,
                            aPredTrans->condition,
                            aResultNode )
              != IDE_SUCCESS );

    // ������ condition node�� ���� estimation���ش�.
    IDE_TEST( estimateConditionNode( aPredTrans,
                                     *aResultNode )
              != IDE_SUCCESS );

    // �� ������ ��忡 ���� indexArgument�� 0���� �����Ѵ�.
    (*aResultNode)->indexArgument = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::makeLogicalNode( qmoPredTrans * aPredTrans,
                                qtcNode      * aArgumentNode,
                                qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      "AND", "OR" ���� logical operator node �� �����Ѵ�.
 *
 * Implementation :
 *      qmoPredTransI�� private �Լ��� makePredNode�� ����Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makeLogicalNode::__FT__" );

    IDE_TEST( makePredNode( aPredTrans->statement,
                            aArgumentNode,
                            aPredTrans->logical,
                            aResultNode )
              != IDE_SUCCESS );

    // ������ logical node�� ���� estimation���ش�.
    IDE_TEST( estimateLogicalNode( aPredTrans,
                                   * aResultNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::makeAndNode( qmoPredTrans * aPredTrans,
                            qtcNode      * aArgumentNode,
                            qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      "AND" node �� �����Ѵ�.
 *
 * Implementation :
 *      qmoPredTransI�� private �Լ��� makePredNode�� ����Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makeAndNode::__FT__" );

    IDE_TEST( makePredNode( aPredTrans->statement,
                            aArgumentNode,
                            (SChar*)"AND",
                            aResultNode )
              != IDE_SUCCESS );

    // ������ logical node�� ���� estimation���ش�.
    IDE_TEST( estimateLogicalNode( aPredTrans,
                                   *aResultNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::makePredNode( qcStatement  * aStatement,
                             qtcNode      * aArgumentNode,
                             SChar        * aOperator,
                             qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      ���� predicate node�� �����Ѵ�.
 *      priavate �Լ��̸�, makeLogicalNode, makeConditionNode, makeAndNode��
 *      ���� ȣ��ȴ�.
 *
 * Implementation :
 *      aArgumentNode�� �ݵ�� next�� �޷� �־�� �Ѵ�.
 *
 ***********************************************************************/

    qcNamePosition sNullPosition;
    qtcNode      * sResultNode[2];

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makePredNode::__FT__" );

    IDE_DASSERT( aArgumentNode != NULL );

    SET_EMPTY_POSITION( sNullPosition );

    IDE_TEST( qtc::makeNode( aStatement,
                             sResultNode,
                             & sNullPosition,
                             (const UChar*)aOperator,
                             idlOS::strlen( aOperator ) )
              != IDE_SUCCESS );

    sResultNode[0]->node.arguments = (mtcNode*)&(aArgumentNode->node);

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                sResultNode[0] )
              != IDE_SUCCESS );

    *aResultNode = sResultNode[0];

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::copyNode( qmoPredTrans * aPredTrans,
                         qtcNode      * aNode,
                         qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      qtcNode�� ���ο� �޸� ������ �����Ѵ�.
 *
 * Implementation :
 *      ��� �� �Լ��� qtc.cpp�� �־�� �Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sNode;

    IDU_FIT_POINT_FATAL( "qmoPredTransI::copyNode::__FT__" );

    // ���ο� ��带 ����� ���� �޸𸮸� �Ҵ�޴´�.
    IDE_TEST( QC_QMP_MEM( aPredTrans->statement )->alloc( ID_SIZEOF( qtcNode ),
                                                          (void **)& sNode )
              != IDE_SUCCESS  );

    // ���ڷ� ���� column node�� value node�� ����.
    idlOS::memcpy( sNode, aNode, ID_SIZEOF(qtcNode) );

    sNode->node.next = NULL;

    *aResultNode = sNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::makeSubQWrapperNode( qmoPredTrans * aPredTrans,
                                    qtcNode      * aValueNode,
                                    qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      subqueryWrapper ��带 �����Ѵ�.
 *
 * Implementation :
 *      qtc�� �̹� �ִ� �Լ�����
 *      qmoPred�� transformNode �迭 �Լ�����
 *      ���� ��带 ����� ������ �Լ� �������̽��� �����ϱ� ����
 *      �ѹ� wrapping �Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makeSubQWrapperNode::__FT__" );

    IDE_TEST( qtc::makeSubqueryWrapper( aPredTrans->statement,
                                        aValueNode,
                                        aResultNode )
                 != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::makeIndirectNode( qmoPredTrans * aPredTrans,
                                 qtcNode      * aValueNode,
                                 qtcNode     ** aResultNode )
{
/***********************************************************************
 *
 * Description :
 *      ���ο� �޸� ������ indirect node�� �����Ѵ�.
 *
 * Implementation :
 *      qtc�� �ִ� �Լ��� ���������, alloc�� ���⼭ ���ش�.
 *      makeSubQWrapperNode�� ���������� ������ �������̽���
 *      �����ϱ� ���� �ѹ� wrapping �Ѵ�.
 *
 ***********************************************************************/

    qtcNode * sIndNode;

    IDU_FIT_POINT_FATAL( "qmoPredTransI::makeIndirectNode::__FT__" );

    IDE_TEST( QC_QMP_MEM( aPredTrans->statement )->alloc( ID_SIZEOF( qtcNode ),
                                                          (void **)& sIndNode )
              != IDE_SUCCESS );

    IDE_TEST( qtc::makeIndirect( aPredTrans->statement,
                                 sIndNode,
                                 aValueNode )
              != IDE_SUCCESS );

    // PROJ-2415 Grouping Sets
    // clear subquery depInfo
    qtc::dependencyClear( & sIndNode->depInfo );

    *aResultNode = sIndNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::estimateConditionNode( qmoPredTrans * aPredTrans,
                                      qtcNode      * aConditionNode )
{
/***********************************************************************
 *
 * Description :
 *      qmoPred::nodeTransform4OneNode�� �ִ� estimate �κ���
 *      ���� �� �Լ��̴�.
 *
 * Implementation :
 *
 *   [ column node�� conversion ���� ���� ] :
 *    quantify �񱳿����ڴ� column�� ���� conversion ������
 *    value node�� leftConversion�� ������ �ִ�.
 *    ����, value ��忡 leftConversion node�� ������,
 *    �� leftConversion�� columnNode�� conversion���� �����ϰ�,
 *    valueNode�� leftConversion�� �������� ���´�.
 *
 *   // fix BUG-10574
 *   // host������ �����ϴ� predicate�� ���� keyrange ���� ��庯ȯ��,
 *   // ��� ��ȯ�� ����ǰ� �� ��,
 *   // host������ �������� �ʴ� predicate�� ����, estimate �� �����Ѵ�.
 *   //   ��) i1 in ( 1, 1.0, ? )
 *   //       ��庯ȯ ����
 *   //       ==> (i1=1) or (i1=1.0) or (i1=?)
 *   //           ------------------
 *   //           estimate ����
 ***********************************************************************/

    qtcNode        * sArgColumnNode;
    qtcNode        * sArgValueNode;
    qtcNode        * sValueNode;

    IDU_FIT_POINT_FATAL( "qmoPredTransI::estimateConditionNode::__FT__" );

    sArgColumnNode = qtcNodeI::argumentNode( aConditionNode );
    sArgValueNode = qtcNodeI::nextNode( sArgColumnNode );

    // fix BUG-12061 BUG-12058
    // �� : where ( i1, i2 ) = ( (select max(i1) ..),
    //                           (select max(i2) ..) ) �� ��� ��ȯ ��,
    //      where ( i1, i2 ) in ( (select i1, i2 from ...),
    //                            (select i1, i2 from ...) ) �� ��� ��ȯ ��,
    // ���...
    // �� ���ǹ��� ���, ��� ��ȯ��,
    // subqueryWrapper ���� indirect node���� ����Ǹ�,
    // value node�� left conversion�� ������ ��� ���� ����
    // ������ �����ϴ� value node�� ã�ƾ� ��.
    // subquery�� ������ ��庯ȯ �Լ��� �ּ� ����

    for( sValueNode = sArgValueNode;
         ( (sValueNode->node.lflag & MTC_NODE_INDIRECT_MASK )
           == MTC_NODE_INDIRECT_TRUE ) ||
         ( (sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
           == MTC_NODE_OPERATOR_SUBQUERY ) ;
         sValueNode = (qtcNode*)(sValueNode->node.arguments) ) ;

    if( sValueNode->node.leftConversion == NULL )
    {
        // Nothing To Do
    }
    else
    {
        sArgColumnNode->node.conversion = sValueNode->node.leftConversion;
        sValueNode->node.leftConversion = NULL;
    }

    IDE_TEST( qtc::estimateNodeWithoutArgument( aPredTrans->statement,
                                                aConditionNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredTransI::estimateLogicalNode( qmoPredTrans * aPredTrans,
                                    qtcNode      * aLogicalNode )
{

    IDU_FIT_POINT_FATAL( "qmoPredTransI::estimateLogicalNode::__FT__" );

    IDE_TEST( qtc::estimateNodeWithoutArgument( aPredTrans->statement,
                                                aLogicalNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void
qmoPredTransI::setLogicalNCondition( qmoPredTrans * aPredTrans,
                                     qtcNode      * aCompareNode )
{
/***********************************************************************
 *
 * Description : keyRange ������ ���� ��� ��ȯ�� ��� ��ȯ�� �񱳿����ڿ�
 *               �����Ǵ� �񱳿����ڿ� �������ڸ� ã�´�.
 *
 * Implementation :
 *
 *     1. =ANY        : I1 IN(1,2)    -> (I1=1)  OR  (I1=2)
 *     2. !=ANY       : I1 !=ANY(1,2) -> (I1!=1) OR  (I1!=2)
 *     3. >ANY, >=ANY : I1 >ANY(1,2)  -> (I1>1)  OR  (I1>2)
 *     4. <ANY, <=ANY : I1 <ANY(1,2)  -> (I1<1)  OR  (I1<2)
 *     5. =ALL        : I1 =ALL(1,2)  -> (I1=1)  AND (I1=2)
 *     6. !=ALL       : I1 !=ALL(1,2) -> (I1!=1) AND (I1!=2)
 *     7. >ALL, >=ALL : I1 >ALL(1,2)  -> (I1>1)  AND (I1>2)
 *     8. <ALL, <=ALL : I1 <ALL(1,2)  -> (I1<1)  AND (I1<2)
 *
 ***********************************************************************/

    //------------------------------------------
    // �����Ǵ� �񱳿����ڸ� ã�´�.
    //------------------------------------------

    if( ( aCompareNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK )
        != MTC_NODE_GROUP_COMPARISON_TRUE )
    {
        // LIST���� ��� ��ȯ �� =, !=�� �����Ǵ� �񱳿�����
        // ��: (i1,i2,i3)=(1,2,3), (i1,i2,i3)!=(1,2,3)
        switch ( aCompareNode->node.module->lflag &
                 ( MTC_NODE_OPERATOR_MASK ) )
        {
            case ( MTC_NODE_OPERATOR_EQUAL ) :
                // (=)
                idlOS::strcpy( aPredTrans->condition, "=" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_NOT_EQUAL ) :
                // (!=)
                idlOS::strcpy( aPredTrans->condition, "<>" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            default :
                aPredTrans->condition[0] = '\0';
                aPredTrans->logical[0]   = '\0';
                break;
        }

        // BUG-15816
        aPredTrans->isGroupComparison = ID_FALSE;
    }
    else
    {
        // one column�� LIST�� quantify �񱳿����ڿ� �����Ǵ� �񱳿�����
        switch ( aCompareNode->node.module->lflag &
                 ( MTC_NODE_OPERATOR_MASK | MTC_NODE_GROUP_MASK ) )
        {
            case ( MTC_NODE_OPERATOR_EQUAL | MTC_NODE_GROUP_ANY ) :
                // (=ANY)
                idlOS::strcpy( aPredTrans->condition, "=" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_NOT_EQUAL | MTC_NODE_GROUP_ANY ) :
                // (!=ANY)
                idlOS::strcpy( aPredTrans->condition, "<>" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_GREATER | MTC_NODE_GROUP_ANY ):
                // (>ANY)
                idlOS::strcpy( aPredTrans->condition, ">" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_GREATER_EQUAL | MTC_NODE_GROUP_ANY ) :
                // (>=ANY)
                idlOS::strcpy( aPredTrans->condition, ">=" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_LESS | MTC_NODE_GROUP_ANY ) :
                // (<ANY)
                idlOS::strcpy( aPredTrans->condition, "<" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_LESS_EQUAL | MTC_NODE_GROUP_ANY ) :
                // (<=ANY)
                idlOS::strcpy( aPredTrans->condition, "<=" );
                idlOS::strcpy( aPredTrans->logical, "OR" );
                break;
            case ( MTC_NODE_OPERATOR_EQUAL | MTC_NODE_GROUP_ALL ) :
                // (=ALL)
                idlOS::strcpy( aPredTrans->condition, "=" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_NOT_EQUAL | MTC_NODE_GROUP_ALL ) :
                // (!=ALL)
                idlOS::strcpy( aPredTrans->condition, "<>" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_GREATER | MTC_NODE_GROUP_ALL ):
                // (>ALL)
                idlOS::strcpy( aPredTrans->condition, ">" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_GREATER_EQUAL | MTC_NODE_GROUP_ALL ) :
                // (>=ALL)
                idlOS::strcpy( aPredTrans->condition, ">=" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_LESS | MTC_NODE_GROUP_ALL ) :
                // (<ALL)
                idlOS::strcpy( aPredTrans->condition, "<" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            case ( MTC_NODE_OPERATOR_LESS_EQUAL | MTC_NODE_GROUP_ALL ) :
                // (<=ALL)
                idlOS::strcpy( aPredTrans->condition, "<=" );
                idlOS::strcpy( aPredTrans->logical, "AND" );
                break;
            default :
                aPredTrans->condition[0] = '\0';
                aPredTrans->logical[0]   = '\0';
                break;
        }

        // BUG-15816
        aPredTrans->isGroupComparison = ID_TRUE;
    }
}

IDE_RC
qmoPred::nodeTransform4OneCond( qcStatement  * aStatement,
                                qtcNode      * aCompareNode,
                                qtcNode     ** aTransformNode,
                                qmsQuerySet  * aQuerySet )
{
/***********************************************************************
 *
 * Description : OR ��� ���� �ϳ��� �񱳿����� ��� ������ ��� ��ȯ
 *
 *     LIST �� quantify �񱳿����ڿ� ���ؼ�, keyRange�� �����ϱ� ����
 *     system level operator�� ��庯ȯ��Ų��.
 *
 * Implementation :
 *
 *
 *
 ***********************************************************************/

    qtcNode      * sTransformNode = NULL;
    qmoPredTrans   sPredTrans;
    qtcNode      * sColumnNode;
    qtcNode      * sValueNode;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransform4OneCond::__FT__" );

    // ��� ������ �ʿ��� �ڷᱸ���� �ʱ�ȭ�Ѵ�.
    qmoPredTransI::initPredTrans( &sPredTrans,
                                  aStatement,
                                  aCompareNode,
                                  aQuerySet );

    // indexArgument ������ columnNode�� valueNode�� ã�´�.
    if( aCompareNode->indexArgument == 0 )
    {
        sColumnNode = qtcNodeI::argumentNode( aCompareNode );
        sValueNode  = qtcNodeI::nextNode( sColumnNode );
    }
    else
    {
        sValueNode = qtcNodeI::argumentNode( aCompareNode );
        sColumnNode  = qtcNodeI::nextNode( sValueNode );
    }

    // value node�� list����̰ų� subquery node�̴�.
    // �ܵ� value ����� ���� ����.
    // ��, where i1 in (1) �̶� �ϴ��� 1�� list�� �����ȴ�.
    if( qtcNodeI::isListNode( sValueNode ) == ID_TRUE )
    {
        IDE_TEST( nodeTransform4List( & sPredTrans,
                                      sColumnNode,
                                      sValueNode,
                                      & sTransformNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // value node�� subquery ����̴�.
        IDE_TEST( nodeTransform4SubQ( & sPredTrans,
                                      aCompareNode,
                                      sColumnNode,
                                      sValueNode,
                                      & sTransformNode )
                  != IDE_SUCCESS );
    }

    //------------------------------------------
    // ���ο� �������ڸ� �����ϰ�, ������ ��ȯ�� ��带 �����Ѵ�.
    //------------------------------------------

    IDE_TEST( qmoPredTransI::makeLogicalNode( & sPredTrans,
                                              sTransformNode,
                                              aTransformNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransform4List( qmoPredTrans * aPredTrans,
                             qtcNode      * aColumnNode,
                             qtcNode      * aValueNode,
                             qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 * Description : �ϳ��� ����Ʈ�� ���� ��� ��ȯ
 *
 * Implementation :
 *
 *    aValueNode�� list node�̴�.
 *    �� list node�� argument�� next�� ��ȸ�ϸ鼭 ������ ���� ��� ��ȯ�� �Ѵ�.
 *
 *    aValueNode = ( a, b )�� ����,
 *      1) a, b�� ���� list�� ���
 *         - subqeury�� ���: multi column subquery�� �θ���.
 *           (��) (i1, i2) in ( (select a1, a2 from ...), (select a1, a2 from ...) )
 *           (��) i1 in ( (select a1, a2 from ...), (select a1, a2 from ...) )
 *         - �Ϲ� list�� ���
 *           (��) (i1, i2) in ( (1, 2), (3, 4) )
 *      2) a, b�� ���� one value�� ���
 *         - subquery�� ���: one column subquery�� �θ���.
 *           (��) (i1, i2) in ( (select a1 from ...), (select a2 from ...) )
 *           (��) i1 in ( (select a1 from ...), (select a2 from ...) )
 *         - �Ϲ� value�� ���
 *           (��) (i1, i2) in ( 1, 2 )
 *           (��) i1 in ( 1, 2 )
 *
 ************************************************************************/

    qtcNode            * sArgValueNode;
    qtcNode            * sTransformedNode;
    qtcNode            * sLastNode          = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransform4List::__FT__" );

    *aTransformNode = NULL;

    // list�� ù��° argument node ���� ��� ��ȯ�� �����Ѵ�.
    sArgValueNode  = qtcNodeI::argumentNode( aValueNode );

    // column list, list of list value
    // column list, list of subquery value ó��
    if( qtcNodeI::isListNode( sArgValueNode ) == ID_TRUE ||
        isMultiColumnSubqueryNode( sArgValueNode ) == ID_TRUE )
    {
        while( sArgValueNode != NULL )
        {
            if( qtcNodeI::isListNode( sArgValueNode ) == ID_TRUE )
            {
                IDE_TEST( nodeTransform4List( aPredTrans,
                                              aColumnNode,
                                              sArgValueNode,
                                              & sTransformedNode )
                          != IDE_SUCCESS );
            }
            else if ( isMultiColumnSubqueryNode( sArgValueNode ) == ID_TRUE )
            {
                IDE_TEST( nodeTransform4OneRowSubQ( aPredTrans,
                                                    aColumnNode,
                                                    sArgValueNode,
                                                    & sTransformedNode )
                          != IDE_SUCCESS );
            }
            else
            {
                // sArgValueNode�� list�̰ų� subquery ������
                // �׻� �� �� ���� �ϳ����� �Ѵ�. ��, value node�̸� �ȵȴ�.
                // (i1, i2) in ( (1,2), (1,(2,3)) ) �̷��� �ȵȴٴ� �ǹ���.
                // one column subquery�� �� �� ����.

                IDE_FT_ASSERT( 0 );
            }

            IDE_DASSERT( sTransformedNode != NULL );

            if( *aTransformNode == NULL )
            {
                *aTransformNode = sTransformedNode;
                sLastNode = sTransformedNode;
            }
            else
            {
                qtcNodeI::linkNode( sLastNode, sTransformedNode );
                sLastNode = sTransformedNode;
            }

            sArgValueNode  = qtcNodeI::nextNode( sArgValueNode );
        }
    }
    // column list, list value
    // one column, list value ó��
    else
    {
        if( qtcNodeI::isListNode( aColumnNode ) == ID_TRUE )
        {
            // (i1, i2) in (1,2) ���� ���
            IDE_TEST( qmoPred::nodeTransformListColumnListValue( aPredTrans,
                                                                 aColumnNode,
                                                                 sArgValueNode,
                                                                 aTransformNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // i1 in (1) ���� ���
            IDE_TEST( qmoPred::nodeTransformOneColumnListValue( aPredTrans,
                                                                aColumnNode,
                                                                sArgValueNode,
                                                                aTransformNode )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransformListColumnListValue( qmoPredTrans * aPredTrans,
                                           qtcNode      * aColumnNode,
                                           qtcNode      * aValueNode,
                                           qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 *   Description :
 *         nodeTransform4List���� value list�� ó���ϱ� ���� �Լ�
 *         (i1, i2) in (1,2) ���� ��츦 ó���Ѵ�.
 *
 *   Implementation :
 *         column�� ������ value�� ������ ���ٰ� �����ϰ�,
 *         ���� ���� ��쿡�� validation�ÿ� ������ ����.
 *
 ***********************************************************************/

    qtcNode * sTransformedNode = NULL;
    qtcNode * sLastNode        = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransformListColumnListValue::__FT__" );

    *aTransformNode = NULL;
    aColumnNode = qtcNodeI::argumentNode( aColumnNode );

    while( aValueNode != NULL && aColumnNode != NULL )
    {
        IDE_TEST( nodeTransformOneNode( aPredTrans,
                                        aColumnNode,
                                        aValueNode,
                                        & sTransformedNode )
                  != IDE_SUCCESS );

        IDE_DASSERT( sTransformedNode != NULL );

        if( *aTransformNode == NULL )
        {
            *aTransformNode = sTransformedNode;
            sLastNode = sTransformedNode;
        }
        else
        {
            qtcNodeI::linkNode( sLastNode, sTransformedNode );
            sLastNode = sTransformedNode;
        }

        aValueNode  = qtcNodeI::nextNode( aValueNode );
        aColumnNode = qtcNodeI::nextNode( aColumnNode );
    }

    // AND �������ڸ� �����ϰ�,
    // AND ������ ��ȯ�� ��带 �����Ѵ�.
    IDE_TEST( qmoPredTransI::makeAndNode( aPredTrans,
                                          * aTransformNode,
                                          aTransformNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransformOneColumnListValue( qmoPredTrans * aPredTrans,
                                          qtcNode      * aColumnNode,
                                          qtcNode      * aValueNode,
                                          qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 *   Description :
 *         nodeTransform4List���� value list�� ó���ϱ� ���� �Լ�
 *         i1 in (1) ���� ��츦 ó���Ѵ�.
 *
 *   Implementation :
 *         column�� �ϳ��̴�.
 *
 ***********************************************************************/

    qtcNode * sTransformedNode = NULL;
    qtcNode * sLastNode        = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransformOneColumnListValue::__FT__" );

    *aTransformNode = NULL;

    while( aValueNode != NULL )
    {
        IDE_TEST( nodeTransformOneNode( aPredTrans,
                                        aColumnNode,
                                        aValueNode,
                                        & sTransformedNode )
                  != IDE_SUCCESS );

        IDE_DASSERT( sTransformedNode != NULL );

        if( *aTransformNode == NULL )
        {
            *aTransformNode = sTransformedNode;
            sLastNode = sTransformedNode;
        }
        else
        {
            qtcNodeI::linkNode( sLastNode, sTransformedNode );
            sLastNode = sTransformedNode;
        }

        aValueNode  = qtcNodeI::nextNode( aValueNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransformOneNode( qmoPredTrans * aPredTrans,
                               qtcNode      * aColumnNode,
                               qtcNode      * aValueNode,
                               qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 * Description : �ϳ��� column���� value��忡 ���� ��� ��ȯ ����
 *
 *     ���ڷ� ���� column node�� value node�� �����ϰ�,
 *     ��庯ȯ�� �˸��� �񱳿�����(system level operator) ��带 ����
 *     ������ ��带 �����Ͽ� ���ο� predicate�� �����.
 *
 *     [ column node�� conversion ���� ���� ]
 *     quantify �񱳿����ڴ� column�� ���� conversion ������
 *     value node�� leftConversion�� ������ �ִ�.
 *     ��: i1(integer) in ( 1, 2, 3.5, ? )
 *         ���� ������ value���� 3.5�� ?�� ���,
 *         leftConversion�� column conversion�� ������ ������ �ִ�.
 *     ��� ��ȯ�� ����Ǹ� system level operator�� ����ϱ� ������
 *     column node�� conversion ������ ������ �־�� �Ѵ�.
 *     ����� column node���� conversion ������ ���� ������,
 *     value node�� leftConversion ������ ����
 *     column node�� ���� conversion ������ ������ �Ѵ�.
 *
 * Implementation :
 *
 *     1. ���ڷ� ���� column node�� value node�� ����.
 *     2. value node�� leftConversion ������ ����,
 *        column node�� conversion ���� ����.
 *     3. ��庯ȯ�� �˸´� �񱳿����� ��带 �����ϰ�,
 *        1�� ��带 �������� ����
 *        (1) �񱳿����� ��� estimateNode
 *        (2) �񱳿����� ��� indexArgument ����.
 *
 ***********************************************************************/

    qtcNode        * sConditionNode;
    qtcNode        * sColumnNode;
    qtcNode        * sValueNode;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransformOneNode::__FT__" );

    // multi column subquery�� ���ؼ��� �� �Լ��� ���ͼ��� �ȵȴ�.
    // �� ���� nodeTransform4OneRowSubQ() �Լ��� Ÿ�� �Ѵ�.
    if( isMultiColumnSubqueryNode( aValueNode ) == ID_TRUE )
    {
        IDE_DASSERT( 0 );
    }

    //-----------------------------------
    // column, value node ����
    //-----------------------------------
    IDE_TEST( qmoPredTransI::copyNode( aPredTrans,
                                       aColumnNode,
                                       & sColumnNode )
              != IDE_SUCCESS );

    // fix BUG-32079
    // one column subquery node has MTC_NODE_INDIRECT_TRUE flag and may have indirect conversion node at arguments.
    // ex)
    // var a integer;
    // create table a ( dummy char(1) primary key );
    // prepare select 'a' from a where dummy in ( :a, (select dummy from a) );
    IDE_TEST( qmoPredTransI::copyNode( aPredTrans,
                                       aValueNode,
                                       & sValueNode )
              != IDE_SUCCESS );

    qtcNodeI::linkNode( sColumnNode, sValueNode );

    // quantify �񱳿����ڿ� �����Ǵ� ���ο� �񱳿����ڸ� �����.
    IDE_TEST( qmoPredTransI::makeConditionNode( aPredTrans,
                                                sColumnNode,
                                                & sConditionNode )
              != IDE_SUCCESS );

    *aTransformNode = sConditionNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransform4OneRowSubQ( qmoPredTrans * aPredTrans,
                                   qtcNode      * aColumnNode,
                                   qtcNode      * aValueNode,
                                   qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 * Description : value node�� one row SUBQUERY �� ����� ��� ��ȯ
 *
 * Implementation :
 *
 *     ��) (i1,i2) in ( ( select a1, a2 from ... ), ( select a1, a2 from ... ) )
 *
 *    [IN]
 *      |
 *    [LIST]--------------->[LIST]
 *      |                     |
 *    [i1]-->[i2]           [SUBQ-a]-------->[SUBQ-b]
 *                            |                 |
 *                          [i1]->[i2]        [i1]->[i2]
 *
 *   [OR]
 *    |
 *   [AND]----------------------------->[AND]
 *    |                                   |
 *   [=]------------>[=]                 [=]-------------->[=]
 *    |               |                   |                 |
 *   [i1]->[ind]     [i2]->[ind]         [i1]->[ind]       [i2]->[ind]
 *           |               |                  |                  |
 *         [SUBQ-a]        [a.a2]             [SUBQ-b]           [b.a2]
 *
 ***********************************************************************/

    qtcNode * sFirstNode;
    qtcNode * sLastNode;
    qtcNode * sConditionNode;
    qtcNode * sIndNode;
    qtcNode * sCopiedColumnNode;
    qtcNode * sIteratorColumnNode;
    qtcNode * sIteratorValueNode;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransform4OneRowSubQ::__FT__" );

    *aTransformNode = NULL;
    sFirstNode = NULL;
    sIteratorColumnNode = qtcNodeI::argumentNode( aColumnNode );
    sIteratorValueNode  = aValueNode;

    while( sIteratorColumnNode != NULL )
    {
        IDE_TEST( qmoPredTransI::copyNode( aPredTrans,
                                           sIteratorColumnNode,
                                           & sCopiedColumnNode )
                  != IDE_SUCCESS );

        IDE_TEST( qmoPredTransI::makeIndirectNode( aPredTrans,
                                                   sIteratorValueNode,
                                                   & sIndNode )
                  != IDE_SUCCESS );

        qtcNodeI::linkNode( sCopiedColumnNode, sIndNode );

        IDE_TEST( qmoPredTransI::makeConditionNode( aPredTrans,
                                                    sCopiedColumnNode,
                                                    & sConditionNode )
                  != IDE_SUCCESS );

        if( sFirstNode == NULL )
        {

            // fix BUG-13939
            // keyRange ������, in subquery or subquery keyRange�� ���,
            // subquery�� ������ �� �ֵ��� �ϱ� ����
            // ��庯ȯ��, subquery node�� ����� �񱳿����ڳ�忡
            // �� ������ �����Ѵ�.
            sConditionNode->lflag &= ~QTC_NODE_SUBQUERY_RANGE_MASK;
            sConditionNode->lflag |= QTC_NODE_SUBQUERY_RANGE_TRUE;

            sFirstNode = sConditionNode;
            sLastNode = sConditionNode;

            // sIteratorValueNode�� ó���� subquery�� ����Ű�� ��忴��.
            // �ι�° �÷����ʹ� subquery�� �ι�° �÷� ��带 �����Ѿ� �Ѵ�.
            // �� �׸� ����.
            sIteratorValueNode = qtcNodeI::argumentNode( sIteratorValueNode );
        }
        else
        {
            qtcNodeI::linkNode( sLastNode, sConditionNode );
            sLastNode = sConditionNode;
        }

        sIteratorColumnNode = qtcNodeI::nextNode( sIteratorColumnNode );
        sIteratorValueNode  = qtcNodeI::nextNode( sIteratorValueNode );
    }

    IDE_TEST( qmoPredTransI::makeAndNode( aPredTrans,
                                          sFirstNode,
                                          aTransformNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::nodeTransform4SubQ( qmoPredTrans * aPredTrans,
                             qtcNode      * aCompareNode,
                             qtcNode      * aColumnNode,
                             qtcNode      * aValueNode,
                             qtcNode     ** aTransformNode )
{
/***********************************************************************
 *
 * Description : value node�� SUBQUERY �� ����� ��� ��ȯ
 *
 * Implementation :
 *
 *     ��) i1 in ( select a1 from ... )
 *
 *        [IN]                        [=]
 *         |                           |
 *        [i1]-->[subquery]   ==>     [i1]-->[Wrapper]
 *                   |                           |
 *                  [a1]                    [subquery]
 *                                               |
 *                                             [a1]
 *
 *     ��) (i1,i2) in ( select a1, a2 from ... )
 *
 *     [IN]                          [=] -------------------> [=]
 *      |                             |                        |
 *    [LIST]--->[subquery]           [i1] ----> [Wrapper]     [i2]--> [ind]
 *      |           |          ==>                 |                    |
 *    [i1]-->[i2] [a1]-->[a2]                  [Subquery]               |
 *                                                 |                    |
 *                                                [a1] -------------> [a2]
 *
 ***********************************************************************/

    idBool           sIsListColumn;
    qtcNode        * sSubqueryWrapperNode = NULL;
    qtcNode        * sConditionNode;
    qtcNode        * sLastNode;
    qtcNode        * sNewColumnNode;
    qtcNode        * sIndNode;

    IDU_FIT_POINT_FATAL( "qmoPred::nodeTransform4SubQ::__FT__" );

    // To Fix BUG-13308
    if( qtcNodeI::isListNode( aColumnNode ) == ID_TRUE )
    {
        sIsListColumn = ID_TRUE;
    }
    else
    {
        sIsListColumn = ID_FALSE;
    }

    if ( ( sIsListColumn == ID_TRUE ) &&
         ( ( aCompareNode->node.lflag & MTC_NODE_GROUP_COMPARISON_MASK )
           == MTC_NODE_GROUP_COMPARISON_FALSE ) )
    {
        IDE_TEST( qmoPred::nodeTransform4OneRowSubQ( aPredTrans,
                                                     aColumnNode,
                                                     aValueNode,
                                                     aTransformNode )
                  != IDE_SUCCESS );
    }
    else
    {
        if( sIsListColumn == ID_TRUE )
        {
            aColumnNode = qtcNodeI::argumentNode( aColumnNode );
        }
        else
        {
            // nothing to do
        }

        //------------------------------------------
        // column node ����
        //------------------------------------------
        IDE_TEST( qmoPredTransI::copyNode( aPredTrans,
                                           aColumnNode,
                                           & sNewColumnNode )
                  != IDE_SUCCESS );

        //------------------------------------------
        // value node�� ���� subqueryWrapperNode ����
        //------------------------------------------
        IDE_TEST( qmoPredTransI::makeSubQWrapperNode( aPredTrans,
                                                      aValueNode,
                                                      & sSubqueryWrapperNode )
                  != IDE_SUCCESS );

        qtcNodeI::linkNode( sNewColumnNode, sSubqueryWrapperNode );

        // ��庯ȯ�� �񱳿����ڿ� �����Ǵ� ���ο� �񱳿����ڸ� �����.
        IDE_TEST( qmoPredTransI::makeConditionNode( aPredTrans,
                                                    sNewColumnNode,
                                                    & sConditionNode )
                  != IDE_SUCCESS );

        // fix BUG-13939
        // keyRange ������, in subquery or subquery keyRange�� ���,
        // subquery�� ������ �� �ֵ��� �ϱ� ����
        // ��庯ȯ��, subquery node�� ����� �񱳿����ڳ�忡
        // �� ������ �����Ѵ�.
        sConditionNode->lflag &= ~QTC_NODE_SUBQUERY_RANGE_MASK;
        sConditionNode->lflag |= QTC_NODE_SUBQUERY_RANGE_TRUE;

        *aTransformNode = sConditionNode;
        sLastNode = sConditionNode;

        if( sIsListColumn == ID_TRUE )
        {
            // ���ڷ� �Ѿ�� �÷���尡 LIST�� ���
            // ������ �÷��� ���� ��庯ȯ ����

            aColumnNode = qtcNodeI::nextNode( aColumnNode );
            aValueNode  = qtcNodeI::nextNode(
                qtcNodeI::argumentNode( aValueNode ) );

            while( aColumnNode != NULL )
            {
                //------------------------------------------
                // column node�� �����ϰ�,
                // subquery part���� InDirect ������ argument�� �����Ѵ�.
                //------------------------------------------

                // column node ó��
                IDE_TEST( qmoPredTransI::copyNode( aPredTrans,
                                                   aColumnNode,
                                                   & sNewColumnNode )
                          != IDE_SUCCESS );

                IDE_TEST( qmoPredTransI::makeIndirectNode( aPredTrans,
                                                           aValueNode,
                                                           & sIndNode )
                          != IDE_SUCCESS );

                qtcNodeI::linkNode( sNewColumnNode, sIndNode );

                // ��庯ȯ�� �񱳿����ڿ� �����Ǵ� ���ο� �񱳿����ڸ� �����.
                IDE_TEST( qmoPredTransI::makeConditionNode( aPredTrans,
                                                            sNewColumnNode,
                                                            & sConditionNode )
                          != IDE_SUCCESS );

                qtcNodeI::linkNode( sLastNode, sConditionNode );
                sLastNode = sConditionNode;

                aColumnNode = qtcNodeI::nextNode( aColumnNode );
                aValueNode  = qtcNodeI::nextNode( aValueNode );
            }

            IDE_TEST( qmoPredTransI::makeAndNode( aPredTrans,
                                                  *aTransformNode,
                                                  aTransformNode )
                      != IDE_SUCCESS );
        }
        else
        {
            // ���ڷ� �Ѿ�� �÷���尡 LIST�� �ƴ� column�� ���
            // Nothing To Do
        }
    }

    // fix BUG-13969
    // subquery�� ���� ��� ��ȯ��,
    // subquery node�� ���� dependency�� �������� �ʴ´�.
    // column node�� dependency ������ �������� �ø���.
    //
    // ��: SELECT COUNT(*) FROM T1
    //      WHERE (I1,I2) IN ( SELECT I1, I2 FROM T2 );
    //           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //  �� IN predicate�� dependency�� T1�̴�.
    //  ��庯ȯ��, �� dependency������ (T1,T2)�� �ǹǷ�,
    //  �� dependency������ ���� dependency������ (T1)���� �����Ѵ�.
    //  �÷������ dependency�� �񱳿������� dependency�� �����Ѵ�.
    //  ��庯ȯ��, column ���� �񱳿����ڳ���� argument�� ����ȴ�.

    // BUG-36575
    // subquery�� �������� ���� �ܺ������÷��� in-subquery key range��
    // �����Ǵ��� �ܺ������÷��� ���� �ٲ�� ��� rebuild�� �� �ֵ���
    // column�� dependency�� �����Ѵ�.
    //
    // ��: select
    //        (select count(*) from t1 where i1 in (select i1 from t2 where i1=t3.i1))
    //                                       ^^^
    //     from t3;
    //
    // �� i1�� dependency�� (t1,t3)�̴�.
    
    if( sIsListColumn == ID_TRUE )
    {
        sConditionNode = (qtcNode*)(((*aTransformNode)->node).arguments);
    }
    else
    {
        sConditionNode = NULL;
    }

    if ( sSubqueryWrapperNode != NULL )
    {
        for( sConditionNode = sConditionNode;
             sConditionNode != NULL;
             sConditionNode = (qtcNode*)(sConditionNode->node.next) )
        {
            qtc::dependencyOr( 
                &(((qtcNode*)(sConditionNode->node.arguments))->depInfo),
                &(sSubqueryWrapperNode->depInfo),
                &(sConditionNode->depInfo) );
        }

        // subquery�� outer dependency�� or�Ѵ�.
        qtc::dependencyOr( 
            &((qtcNode*)(((*aTransformNode)->node).arguments))->depInfo,
            &(sSubqueryWrapperNode->depInfo),
            &((*aTransformNode)->depInfo) );
    }
    else
    {
        for( sConditionNode = sConditionNode;
             sConditionNode != NULL;
             sConditionNode = (qtcNode*)(sConditionNode->node.next) )
        {
            qtc::dependencySetWithDep(
                &(sConditionNode->depInfo),
                &(((qtcNode*)(sConditionNode->node.arguments))->depInfo) );
        }

        qtc::dependencySetWithDep(
            &((*aTransformNode)->depInfo),
            &((qtcNode*)(((*aTransformNode)->node).arguments))->depInfo );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::setIndexArgument( qtcNode       * aNode,
                           qcDepInfo     * aDependencies )
{
/***********************************************************************
 *
 * Description : indexArgument ����
 *
 *     indexable predicate�� ����, indexArgument�� �����Ѵ�.
 *
 *     indexArgument�� ������ ���� ��쿡 �����ϰ� �ȴ�.
 *     (1) indexable predicate �Ǵܽ�.
 *     (2) Index Nested Loop Join predicate�� selection graph�� ������.
 *         �� ����, indexable join predicate�Ǵܽ� indexArgument��
 *         �����Ǳ�� ������, �̶��� ������ ��Ȯ�� ������ �ƴϱ⶧����,
 *         join method�� Ȯ���ǰ�, selection graph�� join predicate��
 *         ������, ��Ȯ�� ������ �ٽ� �����ϰ� �ȴ�.
 *         ��, Anti Outer Nested Loop Join��, AOJN ��忡 join predicate��
 *         �����Ҷ��� indexArgument�� ����Ǿ�� �Ѵ�.
 *
 *     �� �Լ��� ȣ���ϴ� ����
 *     (1) qmoPred::isIndexable()
 *     (2) qmoPred::makeJoinPushDownPredicate()
 *     (3) qmoPred::makeNonJoinPushDownPredicate() �̸�,
 *     indexArgument �����ϴ� �Լ��� �������� ����ϱ� ���ؼ�,
 *     qtcNode�� ���ڷ� �޴´�.
 *
 * Implementation :
 *
 *     dependencies ������ �񱳿����� ��忡 indexArgument������ ����.
 *
 *     (���ڷ� ���� �� �ִ� ����� ���´� ������ ����.)
 *
 *     (1)  OR         (2)  OR                        (3) �񱳿�����
 *          |               |                                 |
 *       �񱳿�����     �񱳿�����->...->�񱳿�����
 *          |               |                |
 *
 ***********************************************************************/

    qtcNode * sNode = aNode;
    qtcNode * sCurNode;

    IDU_FIT_POINT_FATAL( "qmoPred::setIndexArgument::__FT__" );

    //------------------------------------------
    // �񱳿����� ��带 ã�´�.
    // �񱳿����� ��带 ã������, ��������(OR)��� skip
    //------------------------------------------
    if( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
           == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sNode = (qtcNode *)(sNode->node.arguments);

        //------------------------------------------
        // �ֻ��� ��尡 OR ����� ���,
        // OR ��� ������ ���� �÷��� �񱳿����ڰ� ������ �� �� �ִ�.
        //------------------------------------------
        for( sCurNode = sNode ;
             sCurNode != NULL;
             sCurNode = (qtcNode *)(sCurNode->node.next) )
        {
            //--------------------------------------
            // indexArgument ����
            //--------------------------------------

            // dependencies������ �÷��� ã��, indexArgument������ �����Ѵ�.
            if( qtc::dependencyEqual(
                    & ((qtcNode *)(sCurNode->node.arguments))->depInfo,
                    aDependencies ) == ID_TRUE )
            {
                sCurNode->indexArgument = 0;
            }
            else
            {
                sCurNode->indexArgument = 1;
            }
        }
    }
    else
    {
        //--------------------------------------
        // indexArgument ����
        //--------------------------------------

        // dependencies������ �÷��� ã��, indexArgument������ �����Ѵ�.
        if( qtc::dependencyEqual(
                & ((qtcNode *)(sNode->node.arguments))->depInfo,
                aDependencies ) == ID_TRUE )
        {
            sNode->indexArgument = 0;
        }
        else
        {
            sNode->indexArgument = 1;
        }
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::linkPred4ColumnID( qmoPredicate * aDestPred,
                            qmoPredicate * aSourcePred )
{
/***********************************************************************
 *
 * Description :  �÷����� ������踦 �����.
 *
 *     qmoPred::addNonJoinablePredicate()�� qmoPred::relocatePredicate()
 *     ���� �� �Լ��� ȣ���Ѵ�.
 *
 * Implementation :
 *
 *     1. dest predicate�� source predicate�� ���� �÷��� �ִ� ���,
 *        ���� �÷��� ������ qmoPredicate->more�� source predicate ����
 *
 *     2. dest predicate�� source predicate�� ���� �÷��� ���� ���,
 *        dest predicate �� ������ qmoPredicate->next�� source predicate����
 *
 ***********************************************************************/

    idBool         sIsSameColumnID = ID_FALSE;
    qmoPredicate * sPredicate;
    qmoPredicate * sMorePredicate;

    IDU_FIT_POINT_FATAL( "qmoPred::linkPred4ColumnID::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aDestPred != NULL );
    IDE_DASSERT( aSourcePred != NULL );

    for( sPredicate = aDestPred;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        if( sPredicate->id == aSourcePred->id )
        {
            sIsSameColumnID = ID_TRUE;
            break;
        }
        else
        {
            // Nothing To Do
        }
    }

    /* BUG-48526 */
    if ( ( ( aSourcePred->flag & QMO_PRED_OR_VALUE_INDEX_MASK )
           == QMO_PRED_OR_VALUE_INDEX_TRUE ) &&
         ( sIsSameColumnID == ID_TRUE ) )
    {
        aSourcePred->id = QMO_COLUMNID_NON_INDEXABLE;
        sIsSameColumnID = ID_FALSE;
    }

    if( sIsSameColumnID == ID_TRUE )
    {
        // ���� �÷��� �����ϴ� ���

        for( sMorePredicate = sPredicate;
             sMorePredicate->more != NULL;
             sMorePredicate = sMorePredicate->more ) ;

        sMorePredicate->more = aSourcePred;
        // ����� predicate�� next ������踦 ���´�.
        sMorePredicate->more->next = NULL;
    }
    else // ( sIsSameColumnID == ID_FALSE )
    {
        // ���� �÷��� �������� �ʴ� ���

        for( sPredicate = aDestPred;
             sPredicate->next != NULL;
             sPredicate = sPredicate->next ) ;

        sPredicate->next = aSourcePred;
        // ����� predicate�� next ������踦 ���´�.
        sPredicate->next->next = NULL;
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::removeIndexableSubQTip( qtcNode      * aNode  )
{
/***********************************************************************
 *
 * Description : indexable subqueryTip ����
 *
 *     IN(subquery) �Ǵ� subquery keyRange ����ȭ �� ���� predicate��
 *     keyRange�� ������� ���ϰ�, filter�� ������ ���,
 *     �����Ǿ� �ִ� �� ������ �����ϰ�,
 *     store and search ����ȭ ���� �����ϵ��� �����Ѵ�.
 *     subquery ����ȭ��, �̷� ��츦 �����  store and search ���� ����
 *     ������ �̸� ������ �������Ƿ�, flag�� �ٲٸ� �ȴ�.
 *
 * Implementation :
 *
 *     1. indexArgument ������ subquery node�� ã�´�.
 *     2. �ش� subquery ����ȭ �� ������ �����ϰ�,
 *        store and search ����ȭ �� ���� ����.
 *
 ***********************************************************************/

    qtcNode  * sCompareNode;
    qtcNode  * sNode;
    qmgPROJ  * sPROJGraph;

    IDU_FIT_POINT_FATAL( "qmoPred::removeIndexableSubQTip::__FT__" );

    if( ( aNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode = (qtcNode *)(aNode->node.arguments);
    }
    else
    {
        sCompareNode = aNode;
    }

    //------------------------------------------
    // subquery node�� ã�´�.
    //------------------------------------------

    // ��) i1 = (select i1 from t2 where i1 = 1 );
    //     i1 = (select i1 from ...) or i1 = (select i1 fro ... )

    while( sCompareNode != NULL )
    {
        // fix BUG-11485
        // non-indexable, indexable predicate��
        // ��� �� �ڵ�� ���� �� �����Ƿ�
        // indexArgument�� value node�� ã�� ���,
        // (1) indexArgument�� �������� �ʾ� subquery node�� �� ã�� �� �ְ�,
        // (2) indexArgument�� �ʱ�ȭ ���� ���� ��쵵 ������ �־�,
        // �񱳿����� ���� ������ ���Ḯ��Ʈ�� ���󰡸鼭,
        // subquery tip ������ �����Ѵ�.
        // in subquery/subquery keyRange�� ����� predicate��
        // i1 OP subquery �� ���� �����̹Ƿ�
        // �̿� ���� ó���ϴ°��� ����.
        sNode = (qtcNode*)(sCompareNode->node.arguments);

        while( sNode != NULL )
        {
            //------------------------------------------
            // subqueryTipFlag��
            // in subquery keyRange/subuqery keyRange�� ���� flag�� �����Ȱ��,
            // indexable Subquery Tip�� �����ϰ�,
            // store and search ����ȭ ���� �����ϵ��� flag�� �缳���Ѵ�.
            // ��, [IN(=ANY), NOT IN(!=ALL), =ALL, !=ANY
            // �������� �������]����
            //     ���� store and search ����ȭ�� ������ �� ���� ����
            //     (qmoSubquery::storeAndSearch() �Լ� �ּ� ����)
            //     subquery ����ȭ ���� �������� �ʴ°ɷ� �����Ѵ�.
            //-----------------------------------------

            if( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_SUBQUERY )
            {
                sPROJGraph = (qmgPROJ *)(sNode->subquery->myPlan->graph);

                if( ( ( sPROJGraph->subqueryTipFlag &
                        QMG_PROJ_SUBQUERY_TIP_MASK )
                      == QMG_PROJ_SUBQUERY_TIP_KEYRANGE )
                    ||( ( sPROJGraph->subqueryTipFlag &
                          QMG_PROJ_SUBQUERY_TIP_MASK )
                        == QMG_PROJ_SUBQUERY_TIP_IN_KEYRANGE ) )
                {
                    // fix PR-8936
                    // store and search�� ������ �� ���� ���,
                    if( ( sPROJGraph->subqueryTipFlag &
                          QMG_PROJ_SUBQUERY_STORENSEARCH_MASK )
                        == QMG_PROJ_SUBQUERY_STORENSEARCH_NONE )
                    {
                        // store and search ����ȭ ���� ������ �� ���� ���
                        // [ IN(=ANY), NOT IN(!=ALL), =ALL, !=ANY
                        //   �������� ������� ]
                        // qmoSubquery::storeAndSearch() �Լ� �ּ� ����

                        sPROJGraph->subqueryTipFlag &=
                            ~QMG_PROJ_SUBQUERY_TIP_MASK;
                        sPROJGraph->subqueryTipFlag |=
                            QMG_PROJ_SUBQUERY_TIP_NONE;
                    }
                    else
                    {
                        sPROJGraph->subqueryTipFlag &=
                            ~QMG_PROJ_SUBQUERY_TIP_MASK;
                        sPROJGraph->subqueryTipFlag
                            |= QMG_PROJ_SUBQUERY_TIP_STORENSEARCH;
                    }
                }
                else
                {
                    // Nothing To Do
                }
            }

            sNode = (qtcNode *)(sNode->node.next);
        }

        sCompareNode = (qtcNode *)(sCompareNode->node.next);
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoPred::copyPredicate4Partition( qcStatement   * aStatement,
                                  qmoPredicate  * aSource,
                                  qmoPredicate ** aResult,
                                  UShort          aSourceTable,
                                  UShort          aDestTable,
                                  idBool          aCopyVariablePred )
{
    qmoPredicate *sNextIter;
    qmoPredicate *sMoreIter;
    qmoPredicate *sNextHead;
    qmoPredicate *sMoreHead;
    qmoPredicate *sNextLast;
    qmoPredicate *sMoreLast;
    qmoPredicate *sNewPred;

    IDU_FIT_POINT_FATAL( "qmoPred::copyPredicate4Partition::__FT__" );

    if( aSource != NULL )
    {
        sNextHead = NULL;
        sNextLast = NULL;
        for( sNextIter = aSource;
             sNextIter != NULL;
             sNextIter = sNextIter->next )
        {
            sMoreHead = NULL;
            sMoreLast = NULL;
            for( sMoreIter = sNextIter;
                 sMoreIter != NULL;
                 sMoreIter = sMoreIter->more )
            {
                if( ( sMoreIter->node->lflag & QTC_NODE_SUBQUERY_MASK )
                    == QTC_NODE_SUBQUERY_EXIST )
                {
                    // Nothing to do.
                }
                else
                {
                    // variable predicate copy�� FALSE�̰�,
                    // predicate�� variable�̸� copy���� ����
                    if( ( aCopyVariablePred == ID_FALSE ) &&
                        QMO_PRED_IS_VARIABLE( sMoreIter ) == ID_TRUE )
                    {
                        // Nothing to do.
                    }
                    else
                    {
                        IDE_TEST( copyOnePredicate4Partition( aStatement,
                                                              sMoreIter,
                                                              &sNewPred,
                                                              aSourceTable,
                                                              aDestTable )
                                  != IDE_SUCCESS );

                        linkToMore( &sMoreHead, &sMoreLast, sNewPred );
                    }
                }
            }

            if( sMoreHead != NULL )
            {
                linkToNext( &sNextHead, &sNextLast, sMoreHead );
            }
            else
            {
                // Nothing to do.
            }
        }
        *aResult = sNextHead;
    }
    else
    {
        *aResult = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::copyOnePredicate4Partition( qcStatement   * aStatement,
                                     qmoPredicate  * aSource,
                                     qmoPredicate ** aResult,
                                     UShort          aSourceTable,
                                     UShort          aDestTable )
{
    IDU_FIT_POINT_FATAL( "qmoPred::copyOnePredicate4Partition::__FT__" );

    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoPredicate ),
                                             (void**)aResult )
              != IDE_SUCCESS );

    idlOS::memcpy( *aResult, aSource, ID_SIZEOF( qmoPredicate ) );

    IDE_TEST( qtc::cloneQTCNodeTree4Partition( QC_QMP_MEM( aStatement ),
                                               aSource->node,
                                               &(*aResult)->node,
                                               aSourceTable,
                                               aDestTable,
                                               ID_FALSE )
              != IDE_SUCCESS );

    // conversion node�� ���� ����� �ش�.
    // BUG-27291
    // re-estimate�� �����ϵ��� aStatement�� ���ڿ� �߰��Ѵ�.
    IDE_TEST( qtc::estimate( (*aResult)->node,
                             QC_SHARED_TMPLATE(aStatement),
                             aStatement,
                             NULL,
                             NULL,
                             NULL )
              != IDE_SUCCESS );

    (*aResult)->next = NULL;
    (*aResult)->more = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::deepCopyPredicate( iduVarMemList * aMemory,
                            qmoPredicate  * aSource,
                            qmoPredicate ** aResult )
{
    qmoPredicate *sNextIter;
    qmoPredicate *sMoreIter;
    qmoPredicate *sNextHead;
    qmoPredicate *sMoreHead;
    qmoPredicate *sNextLast;
    qmoPredicate *sMoreLast;
    qmoPredicate *sNewPred;

    IDU_FIT_POINT_FATAL( "qmoPred::deepCopyPredicate::__FT__" );

    if( aSource != NULL )
    {
        sNextHead = NULL;
        sNextLast = NULL;
        for( sNextIter = aSource;
             sNextIter != NULL;
             sNextIter = sNextIter->next )
        {
            sMoreHead = NULL;
            sMoreLast = NULL;
            for( sMoreIter = sNextIter;
                 sMoreIter != NULL;
                 sMoreIter = sMoreIter->more )
            {
                IDE_TEST( copyOnePredicate( aMemory, sMoreIter, &sNewPred )
                          != IDE_SUCCESS );

                linkToMore( &sMoreHead, &sMoreLast, sNewPred );
            }

            linkToNext( &sNextHead, &sNextLast, sMoreHead );
        }
        *aResult = sNextHead;
    }
    else
    {
        *aResult = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void
qmoPred::linkToMore( qmoPredicate ** aHead,
                     qmoPredicate ** aLast,
                     qmoPredicate  * aNew )
{
    if( *aHead == NULL )
    {
        *aHead = aNew;
    }
    else
    {
        IDE_DASSERT( *aLast != NULL );
        (*aLast)->more = aNew;
    }
    *aLast = aNew;
}

void
qmoPred::linkToNext( qmoPredicate ** aHead,
                     qmoPredicate ** aLast,
                     qmoPredicate  * aNew )
{
    if( *aHead == NULL )
    {
        *aHead = aNew;
    }
    else
    {
        IDE_DASSERT( *aLast != NULL );
        (*aLast)->next = aNew;
    }
    *aLast = aNew;
}

IDE_RC
qmoPred::copyOnePredicate( iduVarMemList * aMemory,
                           qmoPredicate  * aSource,
                           qmoPredicate ** aResult )
{
    IDU_FIT_POINT_FATAL( "qmoPred::copyOnePredicate::__FT__" );

    IDE_TEST( aMemory->alloc( ID_SIZEOF( qmoPredicate ),
                              (void**)aResult )
              != IDE_SUCCESS );

    idlOS::memcpy( *aResult, aSource, ID_SIZEOF( qmoPredicate ) );

    IDE_TEST( qtc::cloneQTCNodeTree( aMemory,
                                     aSource->node,
                                     &(*aResult)->node,
                                     ID_FALSE,
                                     ID_FALSE,
                                     ID_FALSE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    (*aResult)->next = NULL;
    (*aResult)->more = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// fix BUG-19211
// constant predicate���� �����Ѵ�.
IDE_RC
qmoPred::copyOneConstPredicate( iduVarMemList * aMemory,
                                qmoPredicate  * aSource,
                                qmoPredicate ** aResult )
{
    IDU_FIT_POINT_FATAL( "qmoPred::copyOneConstPredicate::__FT__" );

    IDE_TEST( aMemory->alloc( ID_SIZEOF( qmoPredicate ),
                              (void**)aResult )
              != IDE_SUCCESS );

    idlOS::memcpy( *aResult, aSource, ID_SIZEOF( qmoPredicate ) );

    IDE_TEST( qtc::cloneQTCNodeTree( aMemory,
                                     aSource->node,
                                     &(*aResult)->node,
                                     ID_FALSE,
                                     ID_FALSE,
                                     ID_TRUE,
                                     ID_FALSE )
              != IDE_SUCCESS );

    (*aResult)->next = NULL;
    (*aResult)->more = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::createPredicate( iduVarMemList * aMemory,
                          qtcNode       * aNode,
                          qmoPredicate ** aNewPredicate )
{
    IDU_FIT_POINT_FATAL( "qmoPred::createPredicate::__FT__" );

    IDE_TEST( aMemory->alloc( ID_SIZEOF( qmoPredicate ),
                              (void**)aNewPredicate )
              != IDE_SUCCESS );

    (*aNewPredicate)->idx = 0;
    (*aNewPredicate)->flag = QMO_PRED_CLEAR;
    (*aNewPredicate)->id = 0;
    (*aNewPredicate)->node = aNode;
    (*aNewPredicate)->mySelectivity    = 1;
    (*aNewPredicate)->totalSelectivity = 1;
    (*aNewPredicate)->mySelectivityOffset = QMO_SELECTIVITY_OFFSET_NOT_USED;
    (*aNewPredicate)->totalSelectivityOffset = QMO_SELECTIVITY_OFFSET_NOT_USED;
    (*aNewPredicate)->next = NULL;
    (*aNewPredicate)->more = NULL;

    // transitive predicate�� ��� �����Ѵ�.
    if ( (aNode->lflag & QTC_NODE_TRANS_PRED_MASK)
         == QTC_NODE_TRANS_PRED_EXIST )
    {
        (*aNewPredicate)->flag &= ~QMO_PRED_TRANS_PRED_MASK;
        (*aNewPredicate)->flag |= QMO_PRED_TRANS_PRED_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool
qmoPred::checkPredicateForHostOpt( qmoPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description : Host ������ ������ ���� ����ȭ�� ������ ������
 *               �Ǵ��Ѵ�.
 *
 *            ** (����) **
 *               execution�ÿ� ������ ���� predicate->flag�� ������ �����Ѵ�.
 *
 *               - ���� predicate�� ���� execution Ÿ�ӿ� selectivity��
 *                 �ٽ� ���ؾ��ϴ����� ����
 *                 : QMO_PRED_HOST_OPTIMIZE_TRUE
 *
 *               - more list�� ù��° predicate�� ����
 *                 total selectivity�� �ٽ� ���ؾ��ϴ����� ����
 *                 : QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE
 *
 *   ������ ������ �����ؾ� �����Ѵ�.
 *    - ȣ��Ʈ ������ �����ؾ� �Ѵ�.
 *    - ȣ��Ʈ ������ ������ predicate�� ����...
 *      - indexable�̾�� �Ѵ�.
 *      - list �̸� �ȵȴ�.
 *      - <, >, <=, >=, between, not between �������̾�� �Ѵ�.
 *    - predicate�� �߿� in-subquery�� ������ ������ �ȵȴ�.
 *
 ***********************************************************************/

    idBool         sResult = ID_FALSE;
    qmoPredicate * sNextIter;
    qmoPredicate * sMoreIter;

    for( sNextIter = aPredicate;
         sNextIter != NULL;
         sNextIter = sNextIter->next )
    {
        if( ( sNextIter->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
            == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE )
        {
            sResult = ID_TRUE;
            break;
        }
        else
        {
            // Nothing to do...
        }
    }

    if( sResult == ID_TRUE )
    {
        for( sNextIter = aPredicate;
             sNextIter != NULL;
             sNextIter = sNextIter->next )
        {
            for( sMoreIter = sNextIter;
                 sMoreIter != NULL;
                 sMoreIter = sMoreIter->more )
            {
                if( ( sMoreIter->flag & QMO_PRED_INSUBQUERY_MASK )
                    == QMO_PRED_INSUBQUERY_EXIST )
                {
                    sResult = ID_FALSE;
                    break;
                }
                else
                {
                    // Nothing to do...
                }
            }
        }
    }
    else
    {
        // Nothing to do...
    }

    return sResult;
}

IDE_RC
qmoPredWrapperI::createWrapperList( qmoPredicate       * aPredicate,
                                    qmoPredWrapperPool * aWrapperPool,
                                    qmoPredWrapper    ** aResult )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::createWrapperList::__FT__" );

    *aResult = NULL;

    for( ;
         aPredicate != NULL;
         aPredicate = aPredicate->next )
    {
        IDE_TEST( addPred( aPredicate, aResult, aWrapperPool )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredWrapperI::extractWrapper( qmoPredWrapper  * aTarget,
                                 qmoPredWrapper ** aFrom )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::extractWrapper::__FT__" );

    IDE_DASSERT( aTarget != NULL );
    IDE_DASSERT( *aFrom != NULL );

    // aTarget�� aFrom�� ó���� ��
    // aFrom�� ��ġ�� �ٲ�� �Ѵ�.
    if( *aFrom == aTarget )
    {
        *aFrom = aTarget->next;
    }
    else
    {
        // Nothing to do...
    }

    // aTaget�� prev, next�� ���� �����ϰ�
    // aTarget�� ����.
    if( aTarget->prev != NULL )
    {
        aTarget->prev->next = aTarget->next;
    }
    else
    {
        // Nothing to do...
    }

    if( aTarget->next != NULL )
    {
        aTarget->next->prev = aTarget->prev;
    }
    else
    {
        // Nothing to do...
    }

    aTarget->prev = NULL;
    aTarget->next = NULL;

    return IDE_SUCCESS;
}

IDE_RC
qmoPredWrapperI::addTo( qmoPredWrapper  * aTarget,
                        qmoPredWrapper ** aTo )
{
    qmoPredWrapper * sWrapperIter;

    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::addTo::__FT__" );

    IDE_DASSERT( aTarget != NULL );

    if( *aTo == NULL )
    {
        *aTo = aTarget;
    }
    else
    {
        for( sWrapperIter = *aTo;
             sWrapperIter->next != NULL;
             sWrapperIter = sWrapperIter->next ) ;

        sWrapperIter->next = aTarget;
        aTarget->prev = sWrapperIter;
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoPredWrapperI::moveWrapper( qmoPredWrapper  * aTarget,
                              qmoPredWrapper ** aFrom,
                              qmoPredWrapper ** aTo )
{
    IDE_DASSERT( aTarget != NULL );

    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::moveWrapper::__FT__" );

    IDE_TEST( extractWrapper( aTarget, aFrom ) != IDE_SUCCESS );

    IDE_TEST( addTo( aTarget, aTo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredWrapperI::moveAll( qmoPredWrapper ** aFrom,
                          qmoPredWrapper ** aTo )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::moveAll::__FT__" );

    if( *aFrom != NULL )
    {
        IDE_TEST( addTo( *aFrom, aTo ) != IDE_SUCCESS );
        *aFrom = NULL;
    }
    else
    {
        // Nothing to do...
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredWrapperI::addPred( qmoPredicate       * aPredicate,
                          qmoPredWrapper    ** aWrapperList,
                          qmoPredWrapperPool * aWrapperPool )
{
    qmoPredWrapper * sNewWrapper;

    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::addPred::__FT__" );

    IDE_DASSERT( aWrapperList != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aWrapperPool != NULL );

    IDE_TEST( newWrapper( aWrapperPool, &sNewWrapper ) != IDE_SUCCESS );

    sNewWrapper->pred = aPredicate;
    sNewWrapper->prev = NULL;
    sNewWrapper->next = NULL;

    IDE_TEST( addTo( sNewWrapper, aWrapperList ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredWrapperI::newWrapper( qmoPredWrapperPool * aWrapperPool,
                             qmoPredWrapper    ** aNewWrapper )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::newWrapper::__FT__" );

    IDE_FT_ASSERT( aWrapperPool != NULL );

    if( aWrapperPool->used >= QMO_DEFAULT_WRAPPER_POOL_SIZE )
    {
        if( aWrapperPool->prepareMemory != NULL )
        {
            IDE_TEST( aWrapperPool->prepareMemory->alloc( ID_SIZEOF( qmoPredWrapper )
                                                          * QMO_DEFAULT_WRAPPER_POOL_SIZE,
                                                          (void**)&aWrapperPool->current )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_FT_ASSERT( aWrapperPool->executeMemory != NULL );

            IDE_TEST( aWrapperPool->executeMemory->alloc( ID_SIZEOF( qmoPredWrapper )
                                                          * QMO_DEFAULT_WRAPPER_POOL_SIZE,
                                                          (void**)&aWrapperPool->current )
                      != IDE_SUCCESS );
        }

        aWrapperPool->used = 0;
    }
    else
    {
        // nothing to do...
    }

    *aNewWrapper = (aWrapperPool->current)++;
    aWrapperPool->used++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPredWrapperI::initializeWrapperPool( iduVarMemList      * aMemory,
                                        qmoPredWrapperPool * aWrapperPool )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::initializeWrapperPool::__FT__" );

    aWrapperPool->prepareMemory = aMemory;
    aWrapperPool->executeMemory = NULL;
    aWrapperPool->current = &aWrapperPool->base[0];
    aWrapperPool->used = 0;

    return IDE_SUCCESS;
}

IDE_RC
qmoPredWrapperI::initializeWrapperPool( iduMemory          * aMemory,
                                        qmoPredWrapperPool * aWrapperPool )
{
    IDU_FIT_POINT_FATAL( "qmoPredWrapperI::initializeWrapperPool::__FT__" );

    aWrapperPool->prepareMemory = NULL;
    aWrapperPool->executeMemory = aMemory;
    aWrapperPool->current = &aWrapperPool->base[0];
    aWrapperPool->used = 0;

    return IDE_SUCCESS;
}

IDE_RC
qmoPred::separateRownumPred( qcStatement   * aStatement,
                             qmsQuerySet   * aQuerySet,
                             qmoPredicate  * aPredicate,
                             qmoPredicate ** aStopkeyPred,
                             qmoPredicate ** aFilterPred,
                             SLong         * aStopRecordCount )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1405 ROWNUM
 *     stopkey predicate�� filter predicate�� �и��Ѵ�.
 *
 *     1. ���ڷ� �Ѿ�� predicate list���� stopkey predicate��
 *        ������ ����.
 *
 *        aPredicate [p1]-[p2]-[p3]-[p4]-[p5]
 *                      ______________|
 *                      |
 *                      |
 *             stopkey predicate
 *
 *     2. �и���ġ�� ���
 *       (1) stopkey Predicate    (2) filter predicate
 *           [p4]                     [p1]-[p2]-[p3]-[p5]
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sPredicate;
    qmoPredicate * sPrevPredicate;
    qmoPredicate * sStopkeyPredicate;
    qmoPredicate * sFilterPredicate;
    idBool         sIsStopkey;
    SLong          sStopRecordCount;

    IDU_FIT_POINT_FATAL( "qmoPred::separateRownumPred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aStopkeyPred != NULL );
    IDE_DASSERT( aFilterPred  != NULL );

    //--------------------------------------
    // �ʱ�ȭ �۾�
    //--------------------------------------

    sFilterPredicate = aPredicate;
    sStopkeyPredicate = NULL;
    sPrevPredicate = NULL;
    sStopRecordCount = 0;

    //--------------------------------------
    // stopkey predicate �и�
    //--------------------------------------

    for ( sPredicate = aPredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        IDE_TEST( isStopkeyPredicate( aStatement,
                                      aQuerySet,
                                      sPredicate->node,
                                      & sIsStopkey,
                                      & sStopRecordCount )
                  != IDE_SUCCESS );

        if ( sIsStopkey == ID_TRUE )
        {
            if ( sPrevPredicate == NULL )
            {
                sFilterPredicate = sPredicate->next;
                sStopkeyPredicate = sPredicate;
                sStopkeyPredicate->next = NULL;
            }
            else
            {
                sPrevPredicate->next = sPredicate->next;
                sStopkeyPredicate = sPredicate;
                sStopkeyPredicate->next = NULL;
            }
            break;
        }
        else
        {
            // Nothing to do.
        }

        sPrevPredicate = sPredicate;
    }

    *aStopkeyPred = sStopkeyPredicate;
    *aFilterPred = sFilterPredicate;
    *aStopRecordCount = sStopRecordCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoPred::isStopkeyPredicate( qcStatement  * aStatement,
                             qmsQuerySet  * aQuerySet,
                             qtcNode      * aNode,
                             idBool       * aIsStopkey,
                             SLong        * aStopRecordCount )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1405 ROWNUM
 *     stopkey predicate���� �˻��Ѵ�.
 *     predicate�� CNF�� ó���ɶ��� stopkey predicate�� �����ȴ�.
 *     DNF�� NNF�� ó���Ǵ� ���� ��� filter predicate���� �����ȴ�.
 *
 *     stopkey�� �� �� �ִ� ������ ������ ����.
 *     1. ROWNUM <= ���/ȣ��Ʈ ����
 *     2. ROWNUM < ���/ȣ��Ʈ ����
 *     3. ROWNUM = 1
 *     4. ���/ȣ��Ʈ ���� >= ROWNUM
 *     5. ���/ȣ��Ʈ ���� > ROWNUM
 *     6. 1 = ROWNUM
 *
 *     ��)
 *           OR
 *           |
 *           <
 *           |
 *        ROWNUM - 3
 *
 *     stopkey�� ��� aStopRecordCount�� ������ ���� ������.
 *     a. ȣ��Ʈ ���� : -1
 *     b. �ǹ� ���� ��� : 0
 *     c. �ǹ� �ִ� ��� : 1�̻�
 *
 * Implementation :
 *     1. �ֻ��� ���� OR ��忩�� �Ѵ�.
 *     2. �񱳿������� next�� NULL�̾�� �Ѵ�.
 *     3. �񱳿������� ���ڵ��� �� ������ �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

    qtcNode         * sOrNode;
    qtcNode         * sCompareNode;
    qtcNode         * sRownumNode;
    qtcNode         * sValueNode;
    idBool            sIsStopkey;
    SLong             sConstValue;
    SLong             sStopRecordCount;
    SLong             sMinusOne;

    IDU_FIT_POINT_FATAL( "qmoPred::isStopkeyPredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aQuerySet  != NULL );
    IDE_DASSERT( aNode      != NULL );

    //--------------------------------------
    // �ʱ�ȭ �۾�
    //--------------------------------------

    sOrNode = aNode;
    sIsStopkey = ID_FALSE;
    sStopRecordCount = 0;
    sMinusOne = 0;

    //--------------------------------------
    // stopkey predicate �˻�
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
                    // ROWNUM < ���/ȣ��Ʈ ����

                    sMinusOne = 1;
                    /* fall through */

                case MTC_NODE_OPERATOR_LESS_EQUAL:
                    // ROWNUM <= ���/ȣ��Ʈ ����

                    sRownumNode = (qtcNode*) sCompareNode->node.arguments;
                    sValueNode = (qtcNode*) sRownumNode->node.next;

                    if ( isROWNUMColumn( aQuerySet,
                                         sRownumNode ) == ID_TRUE )
                    {
                        if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                                                sValueNode ) == ID_TRUE )
                        {
                            sIsStopkey = ID_TRUE;

                            if ( qtc::getConstPrimitiveNumberValue( QC_SHARED_TMPLATE(aStatement),
                                                                    sValueNode,
                                                                    & sConstValue )
                                 == ID_TRUE )
                            {
                                sStopRecordCount = sConstValue - sMinusOne;

                                if ( sStopRecordCount < 1 )
                                {
                                    // �ǹ̾��� ����� 0�� ����.
                                    sStopRecordCount = 0;
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                            }
                            else
                            {
                                sStopRecordCount = -1;
                            }
                        }
                        else
                        {
                            if ( ( QTC_IS_DYNAMIC_CONSTANT( sValueNode ) == ID_TRUE ) &&
                                 ( qtc::dependencyEqual( & sValueNode->depInfo,
                                                         & qtc::zeroDependencies )
                                   == ID_TRUE ) )
                            {
                                sIsStopkey = ID_TRUE;
                                sStopRecordCount = -1;
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
                    break;

                case MTC_NODE_OPERATOR_GREATER:
                    // ���/ȣ��Ʈ ���� > ROWNUM

                    sMinusOne = 1;
                    /* fall through */

                case MTC_NODE_OPERATOR_GREATER_EQUAL:
                    // ���/ȣ��Ʈ ���� >= ROWNUM

                    sValueNode = (qtcNode*) sCompareNode->node.arguments;
                    sRownumNode = (qtcNode*) sValueNode->node.next;

                    if ( isROWNUMColumn( aQuerySet,
                                         sRownumNode ) == ID_TRUE )
                    {
                        if ( qtc::isConstValue( QC_SHARED_TMPLATE(aStatement),
                                                sValueNode ) == ID_TRUE )
                        {
                            sIsStopkey = ID_TRUE;

                            if ( qtc::getConstPrimitiveNumberValue( QC_SHARED_TMPLATE(aStatement),
                                                                    sValueNode,
                                                                    & sConstValue )
                                 == ID_TRUE )
                            {
                                sStopRecordCount = sConstValue - sMinusOne;

                                if ( sStopRecordCount < 1 )
                                {
                                    // �ǹ̾��� ����� 0�� ����.
                                    sStopRecordCount = 0;
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                            }
                            else
                            {
                                sStopRecordCount = -1;
                            }
                        }
                        else
                        {
                            if ( ( QTC_IS_DYNAMIC_CONSTANT( sValueNode ) == ID_TRUE ) &&
                                 ( qtc::dependencyEqual( & sValueNode->depInfo,
                                                         & qtc::zeroDependencies )
                                   == ID_TRUE ) )
                            {
                                sIsStopkey = ID_TRUE;
                                sStopRecordCount = -1;
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
                    break;

                case MTC_NODE_OPERATOR_EQUAL:

                    // ROWNUM = 1
                    sRownumNode = (qtcNode*) sCompareNode->node.arguments;
                    sValueNode = (qtcNode*) sRownumNode->node.next;

                    if ( isROWNUMColumn( aQuerySet,
                                         sRownumNode ) == ID_TRUE )
                    {
                        if ( qtc::getConstPrimitiveNumberValue( QC_SHARED_TMPLATE(aStatement),
                                                                sValueNode,
                                                                & sConstValue )
                             == ID_TRUE )
                        {
                            if ( sConstValue == (SLong) 1 )
                            {
                                sIsStopkey = ID_TRUE;
                                sStopRecordCount = 1;
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
                        // 1 = ROWNUM
                        sValueNode = (qtcNode*) sCompareNode->node.arguments;
                        sRownumNode = (qtcNode*) sValueNode->node.next;

                        if ( isROWNUMColumn( aQuerySet,
                                             sRownumNode ) == ID_TRUE )
                        {
                            if ( qtc::getConstPrimitiveNumberValue( QC_SHARED_TMPLATE(aStatement),
                                                                    sValueNode,
                                                                    & sConstValue )
                                 == ID_TRUE )
                            {
                                if ( sConstValue == (SLong) 1 )
                                {
                                    sIsStopkey = ID_TRUE;
                                    sStopRecordCount = 1;
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
                            // Nothing to do.
                        }
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

    *aIsStopkey = sIsStopkey;
    *aStopRecordCount = sStopRecordCount;

    return IDE_SUCCESS;
}

IDE_RC
qmoPred::removeTransitivePredicate( qmoPredicate ** aPredicate,
                                    idBool          aOnlyJoinPred )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation
 *     predicate list���� ������ transitive predicate�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate  * sPredicate;
    qmoPredicate  * sMorePredicate;
    qmoPredicate  * sFirstPredicate = NULL;
    qmoPredicate  * sPrevPredicate = NULL;
    qmoPredicate  * sFirstMorePredicate = NULL;
    qmoPredicate  * sPrevMorePredicate = NULL;

    IDU_FIT_POINT_FATAL( "qmoPred::removeTransitivePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // transitive predicate ����
    //--------------------------------------

    for ( sPredicate = *aPredicate;
          sPredicate != NULL;
          sPredicate = sPredicate->next )
    {
        sFirstMorePredicate = NULL;
        sPrevMorePredicate = NULL;
        
        for ( sMorePredicate = sPredicate;
              sMorePredicate != NULL;
              sMorePredicate = sMorePredicate->more )
        {
            if ( ( (sMorePredicate->flag & QMO_PRED_TRANS_PRED_MASK)
                   == QMO_PRED_TRANS_PRED_TRUE ) &&
                 ( 
                     ( (sMorePredicate->flag & QMO_PRED_JOIN_PRED_MASK)
                       == QMO_PRED_JOIN_PRED_TRUE )
                     ||
                     ( aOnlyJoinPred == ID_FALSE ) )
                 )
            {
                // Nothing to do.

                // transitive join predicate�� �����Ѵ�.
            }
            else
            {
                if ( sPrevMorePredicate == NULL )
                {
                    sFirstMorePredicate = sMorePredicate;
                    sPrevMorePredicate = sFirstMorePredicate;
                }
                else
                {
                    sPrevMorePredicate->more = sMorePredicate;
                    sPrevMorePredicate = sPrevMorePredicate->more;
                }   
            }
        }

        // predicate more list������ ������ ���´�.
        if ( sPrevMorePredicate != NULL )
        {
            sPrevMorePredicate->more = NULL;
        }
        else
        {
            // Nothing to do.
        }

        // predicate list�� �����Ѵ�.
        if ( sFirstMorePredicate != NULL )
        {
            if ( sPrevPredicate == NULL )
            {
                sFirstPredicate = sFirstMorePredicate;
                sPrevPredicate = sFirstPredicate;
            }
            else
            {
                sPrevPredicate->next = sFirstMorePredicate;
                sPrevPredicate = sPrevPredicate->next;
            }
        }
        else
        {
            // Nothing to do.
        }
    }

    // predicate list������ ������ ���´�.
    if ( sPrevPredicate != NULL )
    {
        sPrevPredicate->next = NULL;
    }
    else
    {
        // Nothing to do.
    }

    *aPredicate = sFirstPredicate;

    return IDE_SUCCESS;
}

IDE_RC
qmoPred::removeEquivalentTransitivePredicate( qcStatement   * aStatement,
                                              qmoPredicate ** aPredicate )
{
/***********************************************************************
 *
 * Description :
 *     PROJ-1404 Transitive Predicate Generation
 *     ������ transitive predicate�� �������� �ߺ��� predicate��
 *     �ִ� ��� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoPredicate * sOutPredicate = NULL;
    qmoPredicate * sPrevPredicate = NULL;
    qmoPredicate * sPredicate;
    idBool         sIsExist;

    IDU_FIT_POINT_FATAL( "qmoPred::removeEquivalentTransitivePredicate::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // �ʱ�ȭ �۾�
    //--------------------------------------

    sPredicate = *aPredicate;

    //--------------------------------------
    // �ߺ��� transitive predicate ����
    //--------------------------------------

    while ( sPredicate != NULL )
    {
        // ���� relocate ���̴�.
        IDE_DASSERT( sPredicate->more == NULL );

        sIsExist = ID_FALSE;

        if ( (sPredicate->flag & QMO_PRED_TRANS_PRED_MASK)
             == QMO_PRED_TRANS_PRED_TRUE )
        {
            IDE_TEST( qmoTransMgr::isExistEquivalentPredicate( aStatement,
                                                               sPredicate,
                                                               sOutPredicate,
                                                               & sIsExist )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
        }

        if ( sIsExist != ID_TRUE )
        {
            if ( sPrevPredicate == NULL )
            {
                sPrevPredicate = sPredicate;
                sOutPredicate = sPredicate;
            }
            else
            {
                sPrevPredicate->next = sPredicate;
                sPrevPredicate = sPredicate;
            }
        }
        else
        {
            // Nothing to do.
        }

        sPredicate = sPredicate->next;
    }

    if ( sPrevPredicate != NULL )
    {
        sPrevPredicate->next = NULL;
    }
    else
    {
        // Nothing to do.
    }

    *aPredicate = sOutPredicate;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoPred::addNNFFilter4linkedFilter( qcStatement   * aStatement,
                                           qtcNode       * aNNFFilter,
                                           qtcNode      ** aNode      )
{
/***********************************************************************
 *
 * Description :
 *     BUG-35155 Partial CNF
 *     linkFilterPredicate ���� ������� qtcNode �׷쿡 NNF ���͸� �߰��Ѵ�.
 *
 * Implementation :
 *     �ֻ����� AND ��尡 �����Ƿ� arguments �� next ���� NNF ���͸� �����Ѵ�.
 *
 ***********************************************************************/

    qtcNode  * sNode;

    IDU_FIT_POINT_FATAL( "qmoPred::addNNFFilter4linkedFilter::__FT__" );

    // Attach nnfFilter to filter
    sNode = (qtcNode *)((*aNode)->node.arguments);
    while (sNode->node.next != NULL)
    {
        sNode = (qtcNode *)(sNode->node.next);
    }
    sNode->node.next = (mtcNode *)aNNFFilter;

    IDE_TEST( qtc::estimateNodeWithoutArgument( aStatement,
                                                *aNode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool qmoPred::hasOnlyColumnCompInPredList( qmoPredInfo * aJoinPredList )
{
/****************************************************************************
 * 
 *  Description : BUG-39403 Inverse Index Join Method Predicate ����
 *  
 *  �Էµ� Join Predicate List����, 
 *  ��� Join Predicate�� ������ �����ϴ��� �˻��Ѵ�.
 *
 *   - (Assertion) Join Predicate�� INDEX_JOINABLE �̾�� �Ѵ�.
 *   - (Assertion) Join Predicate�� '=' ���� �Ѵ�.
 *   - Join Predicate�� Operand ��� Column���� �����Ѵ�.
 *  
 *  > ��� �ϳ��� Column�� �ƴ� Expression�� ������ ���� ��, FALSE
 *  > ��� Predicate�� Column �鸸 ���ϴ� ���, TRUE
 *
 ***************************************************************************/

    qmoPredInfo  * sJoinPredMoreInfo = NULL;
    qmoPredInfo  * sJoinPredInfo     = NULL;
    qtcNode      * sNode             = NULL;
    idBool         sHasOnlyColumn    = ID_TRUE;

    for ( sJoinPredMoreInfo = aJoinPredList;
          sJoinPredMoreInfo != NULL;
          sJoinPredMoreInfo = sJoinPredMoreInfo->next )
    {
        for ( sJoinPredInfo = sJoinPredMoreInfo;
              sJoinPredInfo != NULL;
              sJoinPredInfo = sJoinPredInfo->more )
        {
            // ������ Join Predicate�� INDEX_JOINABLE �̾�� �Ѵ�.
            IDE_DASSERT( ( sJoinPredInfo->predicate->flag & QMO_PRED_INDEX_JOINABLE_MASK )
                         == QMO_PRED_INDEX_JOINABLE_TRUE );

            // Predicate�� ù Node�� ������ �´�.
            sNode = sJoinPredInfo->predicate->node;

            // CNF Form�̹Ƿ�, �� Predicate�� OR Node�̰ų�
            // Predicate ��ü�� �Ѿ�´�.
            if ( ( sNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                 == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                // sNode�� OR Node�� ��� (AND�� �� �� ����)
                IDE_DASSERT( ( sNode->node.lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_OR )

                // OR Node�� ���� Node�� Ž���Ѵ�.
                sNode = (qtcNode *)(sNode->node.arguments);

                // OR Node�� ���� Node�� ��� Ž���� ����
                while( sNode != NULL )
                {
                    sHasOnlyColumn = hasOnlyColumnCompInPredNode( sNode );

                    if ( sHasOnlyColumn == ID_FALSE )
                    {
                        break;
                    }
                    else
                    {
                        sNode = (qtcNode *)sNode->node.next;
                    }
                }
            }
            else
            {
                // sNode�� OR Node�� �ƴ� ���
                // �ٷ� �����Ѵ�.
                sHasOnlyColumn = hasOnlyColumnCompInPredNode( sNode );
            }

            // Expression�� ���� Join Predicat�� ã�Ҵٸ�
            // �ٸ� Join Predicate Ž���� �ߴ��Ѵ�.
            if ( sHasOnlyColumn == ID_FALSE )
            {
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        // Expression�� ���� Join Predicat�� ã�Ҵٸ�
        // �ٸ� Join Predicate Ž���� �ߴ��Ѵ�.
        if ( sHasOnlyColumn == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    return sHasOnlyColumn;
}

idBool qmoPred::hasOnlyColumnCompInPredNode( qtcNode * aNode )
{
/****************************************************************************
 * 
 *  Description : BUG-39403 Inverse Index Join Method Predicate ����
 *  
 *  �Էµ� Join Predicate Node����, ������ �����ϴ��� �˻��Ѵ�.
 *
 *   - (Assertion) Join Predicate�� '=' ���� �Ѵ�.
 *   - Join Predicate Node�� Operand ��� Column���� �����Ѵ�.
 *  
 *  > ��� �� ���̶� Column�� �ƴ� Expression�� ������ ���� ��, FALSE
 *  > �ش� Predicate Node�� Column �鸸 ���ϴ� ���, TRUE
 *
 ***************************************************************************/

    qtcNode      * sNodeArg       = NULL;
    idBool         sHasOnlyColumn = ID_TRUE;

    IDE_DASSERT( aNode->node.module == &mtfEqual );

    // left
    sNodeArg = (qtcNode *)aNode->node.arguments;
    if ( sNodeArg->node.module != &qtc::columnModule )
    {
        sHasOnlyColumn = ID_FALSE;
    }
    else
    {
        // right
        sNodeArg = (qtcNode *)aNode->node.arguments->next;
        if ( sNodeArg->node.module != &qtc::columnModule )
        {
            sHasOnlyColumn = ID_FALSE;
        }
        else
        {
            sHasOnlyColumn = ID_TRUE;
        }
    }

    return sHasOnlyColumn;
}

/* TASK-7219 */
IDE_RC qmoPred::isValidPushDownPredicate( qcStatement  * aStatement,
                                          qmgGraph     * aGraph,
                                          qmoPredicate * aPredicate,
                                          idBool       * aIsValid )
{
/****************************************************************************************
 *
 * Description : qmgSelection::doViewPushSelection �� �����Ͽ� Predicate �� Push Down
 *               ������ Valid�� ������� �˻��Ѵ�.
 *
 * Implementation : 1. Predicate�� Subquery�� �ƴϿ��� �Ѵ�.
 *                  2. PUSH_PRED ��Ʈ�� ���� ������ Join Predicate�� ���
 *                  3. Predicate�� Indexable�ؾ� �ϸ�
 *                  4. Predicate�� List�� �ƴϿ��� �Ѵ�.
 *                  5. One Table Predicate�� ���, �ܺ����� �÷��� ����� �Ѵ�.
 *                  6. Valid ���θ� ��ȯ�Ѵ�.
 *
 ****************************************************************************************/

    UInt   sColumnID    = ID_UINT_MAX;
    idBool sIsIndexable = ID_FALSE;
    idBool sIsValid     = ID_FALSE;

    IDE_TEST_RAISE( aStatement == NULL, ERR_NULL_STATEMENT );
    IDE_TEST_RAISE( aGraph == NULL, ERR_NULL_GRAPH );
    IDE_TEST_RAISE( aPredicate == NULL, ERR_NULL_PREDICATE );

    /* 1. Predicate�� Subquery�� �ƴϿ��� �Ѵ�. */
    if ( ( aPredicate->node->lflag & QTC_NODE_SUBQUERY_MASK )
         == QTC_NODE_SUBQUERY_EXIST )
    {
        IDE_RAISE( NORMAL_EXIT );
    }
    else
    {
        /* Nothing to do */
    }

    /* 2. PUSH_PRED ��Ʈ�� ���� ������ Join Predicate�� ��� */
    if ( ( aPredicate->flag & QMO_PRED_PUSH_PRED_HINT_MASK )
         == QMO_PRED_PUSH_PRED_HINT_TRUE )
    {
        IDE_TEST( isIndexable( aStatement,
                               aPredicate,
                               &( aGraph->myFrom->depInfo ),
                               &( qtc::zeroDependencies ),
                               &( sIsIndexable ) )
                  != IDE_SUCCESS );

        /* 3. Predicate�� Indexable�ؾ� �ϸ� */
        if ( sIsIndexable != ID_TRUE )
        {
            IDE_RAISE( NORMAL_EXIT );
        }
        else
        {
            IDE_TEST( getColumnID( aStatement,
                                   aPredicate->node,
                                   ID_TRUE,
                                   &( sColumnID ) )
                      != IDE_SUCCESS );

            /* 4. Predicate�� List�� �ƴϿ��� �Ѵ�. */
            if ( sColumnID == QMO_COLUMNID_LIST )
            {
                IDE_RAISE( NORMAL_EXIT );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* 5. One Table Predicate�� ��� */
        if ( qtc::getPosNextBitSet( &( aPredicate->node->depInfo ),
                                    qtc::getPosFirstBitSet(
                                        &( aPredicate->node->depInfo ) ) )
             != QTC_DEPENDENCIES_END )
        {
            /* �ܺ� ������ ������ ���, property�� ���� �����Ѵ�. */
            IDE_TEST_CONT( ( SDU_SHARD_TRANSFORM_MODE & SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_MASK  )
                           == SDU_SHARD_TRANSFORM_PUSH_OUT_REF_PRED_DISABLE,
                           NORMAL_EXIT );

            IDE_TEST( qmoPred::getColumnID( aStatement,
                                            aPredicate->node,
                                            ID_TRUE,
                                            & sColumnID )
                      != IDE_SUCCESS );

            /* Predicate�� List�� �ƴϿ��� �Ѵ�. */
            if ( sColumnID != QMO_COLUMNID_LIST )
            {
                setOutRefColumnForQtcNode( aPredicate->node,
                                           &aPredicate->node->depInfo );
            }
            else
            {
                IDE_RAISE( NORMAL_EXIT );
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    sIsValid = ID_TRUE;

    IDE_EXCEPTION_CONT( NORMAL_EXIT );

    /* 6. Valid ���θ� ��ȯ�Ѵ�. */
    if ( aIsValid != NULL )
    {
        *aIsValid = sIsValid;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NULL_STATEMENT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPred::isValidPushDownPredicate",
                                  "statement is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_GRAPH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPred::isValidPushDownPredicate",
                                  "graph is null" ) );
    }
    IDE_EXCEPTION( ERR_NULL_PREDICATE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoPred::isValidPushDownPredicate",
                                  "predicate is null" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* TASK-7219 Non-shard DML */
void qmoPred::setOutRefColumnForQtcNode( qtcNode   * aNode,
                                         qcDepInfo * aPredicateDepInfo )
{
    if ( aNode != NULL )
    {
        if ( aNode->node.module == &qtc::columnModule )
        {
            if ( qtc::getPosNextBitSet( aPredicateDepInfo,
                                        qtc::getPosFirstBitSet( &aNode->depInfo ) ) 
                != QTC_DEPENDENCIES_END )
            {
                aNode->lflag &= ~QTC_NODE_OUT_REF_COLUMN_MASK;
                aNode->lflag |= QTC_NODE_OUT_REF_COLUMN_TRUE;
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

        setOutRefColumnForQtcNode( (qtcNode*)aNode->node.next,
                                   aPredicateDepInfo );

        setOutRefColumnForQtcNode( (qtcNode*)aNode->node.arguments,
                                   aPredicateDepInfo );
    }
    else
    {
        /* Nothing to do. */
    }
}

