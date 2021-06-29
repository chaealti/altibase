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
 * $Id: $
 *
 * Description : Selectivity Manager
 *
 *        - Unit predicate �� ���� selectivity ���
 *        - qmoPredicate �� ���� selectivity ���
 *        - qmoPredicate list �� ���� ���� selectivity ���
 *        - qmoPredicate wrapper list �� ���� ���� selectivity ���
 *        - �� graph �� ���� selectivity ���
 *        - �� graph �� ���� output record count ���
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <qmgDef.h>
#include <qmgSelection.h>
#include <qmgHierarchy.h>
#include <qmgCounting.h>
#include <qmgProjection.h>
#include <qmgGrouping.h>
#include <qmgJoin.h>
#include <qmgLeftOuter.h>
#include <qmgFullOuter.h>
#include <qmo.h>
#include <qmoPredicate.h>
#include <qmoDependency.h>
#include <qmoSelectivity.h>

extern mtfModule mtfEqual;
extern mtfModule mtfNotEqual;
extern mtfModule mtfIsNull;
extern mtfModule mtfIsNotNull;
extern mtfModule mtfGreaterThan;
extern mtfModule mtfGreaterEqual;
extern mtfModule mtfLessThan;
extern mtfModule mtfLessEqual;
extern mtfModule mtfBetween;
extern mtfModule mtfNotBetween;
extern mtfModule mtfLike;
extern mtfModule mtfNotLike;
extern mtfModule mtfEqualAny;
extern mtfModule mtfNotEqualAll;
extern mtfModule mtfDecrypt;
extern mtfModule mtfLnnvl;
extern mtdModule mtdDouble;
extern mtfModule stfEquals;
extern mtfModule stfNotEquals;

IDE_RC
qmoSelectivity::setMySelectivity( qcStatement   * aStatement,
                                  qmoStatistics * aStatInfo,
                                  qcDepInfo     * aDepInfo,
                                  qmoPredicate  * aPredicate )
{
/******************************************************************************
 *
 * Description : JOIN graph ������ qmoPredicate.mySelectivity ���
 *
 * Implementation : ���� predicate �� ���� unit selectivity �� ȹ���Ͽ�
 *                  qmoPredicate.mySelectivity �� ����
 *
 *     1. qmoPredicate �� ���� ���� unit predicate �� �����Ǿ��� ���
 *        ex) i1=1 or i1<1, t1.i1>=3 or t1.i2<=5
 *        S = 1-PRODUCT(1-USn)  (OR �� Ȯ������)
 *     2. qmoPredicate �� �� ���� unit predicate �� �����Ǿ��� ���
 *        ex) i1=1, t1.i1>=t1.i2
 *        S = US(unit selectivity)
 *
 *****************************************************************************/

    SDouble    sSelectivity;
    SDouble    sUnitSelectivity;
    qtcNode  * sCompareNode;
    idBool     sIsDNF;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setMySelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aStatInfo  != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sIsDNF = ID_FALSE;

    if( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
        == MTC_NODE_LOGICAL_CONDITION_TRUE )
    {
        sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);
    }
    else
    {
        sCompareNode = aPredicate->node;
        sIsDNF = ID_TRUE;
    }

    if( sCompareNode->node.next != NULL && sIsDNF == ID_FALSE )
    {
        // 1. OR ������ �񱳿����ڰ� ������ �ִ� ���,
        //    OR �������ڿ� ���� selectivity ���.
        //    1 - (1-a)(1-b).....
        // ex) i1=1 or i1<1, t1.i1>=3 or t1.i2<=5

        sSelectivity = 1;

        while( sCompareNode != NULL )
        {
            IDE_TEST( getUnitSelectivity( QC_SHARED_TMPLATE(aStatement),
                                          aStatInfo,
                                          aDepInfo,
                                          aPredicate,
                                          sCompareNode,
                                          & sUnitSelectivity,
                                          ID_FALSE )
                      != IDE_SUCCESS );

            sSelectivity *= ( 1 - sUnitSelectivity );
            sCompareNode = (qtcNode *)(sCompareNode->node.next);
        }

        sSelectivity = ( 1 - sSelectivity );
    }
    else
    {
        // 2. OR ��� ������ �񱳿����ڰ� �ϳ��� ����
        //    ex) i1=1, t1.i1>=t1.i2

        IDE_TEST( getUnitSelectivity( QC_SHARED_TMPLATE(aStatement),
                                      aStatInfo,
                                      aDepInfo,
                                      aPredicate,
                                      sCompareNode,
                                      & sSelectivity,
                                      ID_FALSE )
                  != IDE_SUCCESS );
    }

    aPredicate->mySelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setTotalSelectivity( qcStatement   * aStatement,
                                     qmoStatistics * aStatInfo,
                                     qmoPredicate  * aPredicate )
{
/******************************************************************************
 *
 * Description : qmoPredicate list �� ���� ���ġ�� �Ϸ�� ��
 *               qmoPredicate.id ��(���� �÷�)�� more list �� ����
 *               qmoPredicate.totalSelectivity(���� TS) ����
 *
 *   PROJ-1446 : more list �� ������� �� qmoPredicate �� �ϳ���
 *               QMO_PRED_HOST_OPTIMIZE_TRUE (ȣ��Ʈ ������ ����) �̸�
 *               more list �� ù��° predicate �� QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE
 *               ���� (List�� non indexable�� ��� ȣ��Ʈ ����ȭ ���ΰ� ����)
 *
 * Implementation :
 *
 *     1. LIST �Ǵ� non-indexable : S = PRODUCT(MS)
 *     2. Indexable
 *        => more list �� ù��° predicate �� QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE ����
 *     2.1. 2���� qmoPredicate : S = integrate selectivity
 *     2.2. Etc : S = PRODUCT(MS)
 *
 *****************************************************************************/

    SDouble        sTotalSelectivity;
    qmoPredicate * sPredicate;
    qmoPredicate * sMorePredicate;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setTotalSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aStatInfo  != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sTotalSelectivity = 1;
    sPredicate = aPredicate;

    sPredicate->flag &= ~QMO_PRED_HEAD_HOST_OPTIMIZE_MASK;
    sPredicate->flag |= QMO_PRED_HEAD_HOST_OPTIMIZE_FALSE;

    //--------------------------------------
    // �÷��� ��ǥ selectivity ���
    //--------------------------------------

    if( sPredicate->id == QMO_COLUMNID_NON_INDEXABLE ||
        sPredicate->id == QMO_COLUMNID_LIST )
    {
        sMorePredicate = sPredicate;
        while( sMorePredicate != NULL )
        {
            sTotalSelectivity *= sMorePredicate->mySelectivity;
            sMorePredicate = sMorePredicate->more;
        }
    }
    else
    {
        //--------------------------------------
        // PROJ-1446 : Host variable �� ������ ���� ����ȭ
        // more list �� ������� �� qmoPredicate �� �ϳ���
        // QMO_PRED_HOST_OPTIMIZE_TRUE (ȣ��Ʈ ������ ����) �̸�
        // more list �� ù��° predicate �� QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE ����
        // (List�� non indexable�� ��� ȣ��Ʈ ����ȭ ���ΰ� ����)
        //--------------------------------------

        sMorePredicate = sPredicate;
        while( sMorePredicate != NULL )
        {
            if( ( sMorePredicate->flag & QMO_PRED_HOST_OPTIMIZE_MASK )
                == QMO_PRED_HOST_OPTIMIZE_TRUE )
            {
                sPredicate->flag &= ~QMO_PRED_HEAD_HOST_OPTIMIZE_MASK;
                sPredicate->flag |= QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE;
            }
            else
            {
                // Nothing to do.
            }
            sMorePredicate = sMorePredicate->more;
        }

        if( sPredicate->more != NULL &&
            sPredicate->more->more == NULL )
        {
            IDE_TEST( getIntegrateMySelectivity( QC_SHARED_TMPLATE(aStatement),
                                                 aStatInfo,
                                                 sPredicate,
                                                 & sTotalSelectivity,
                                                 ID_FALSE )
                      != IDE_SUCCESS );
        }
        else
        {
            sMorePredicate = sPredicate;
            while( sMorePredicate != NULL )
            {
                sTotalSelectivity *= sMorePredicate->mySelectivity;
                sMorePredicate = sMorePredicate->more;
            }
        }
    }

    // ù��° qmoPredicate�� total selectivity ����
    aPredicate->totalSelectivity = sTotalSelectivity;

    IDE_DASSERT_MSG( sTotalSelectivity >= 0 && sTotalSelectivity <= 1,
                     "TotalSelectivity : %"ID_DOUBLE_G_FMT"\n",
                     sTotalSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::recalculateSelectivity( qcTemplate    * aTemplate,
                                        qmoStatistics * aStatInfo,
                                        qcDepInfo     * aDepInfo,
                                        qmoPredicate  * aPredicate )
{
/******************************************************************************
 *
 * Description : Execution �ܰ�(host ������ ���� ���ε��� ��)����
 *               qmo::optimizeForHost ȣ��� scan method �籸���� ����
 *               qmoPredicate ������ selectivity �� �����ϰ� Offset ��
 *               totalSelectivity �� �缳���Ѵ�.
 *
 * Implementation :
 *
 *            1. ȣ��Ʈ ������ �����ϴ� qmoPredicate more list
 *               - setMySelectivityOffset ���
 *               - setTotalSelectivityOffset ���
 *            2. ȣ��Ʈ ������ �������� �ʴ� qmoPredicate
 *               - �ٽ� ���� �ʿ� ����
 *
 *****************************************************************************/
 
    qmoPredicate     * sNextPredicate;
    qmoPredicate     * sMorePredicate;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::recalculateSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate  != NULL );
    IDE_DASSERT( aStatInfo  != NULL );
    IDE_DASSERT( aDepInfo   != NULL );
    IDE_DASSERT( aPredicate != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sNextPredicate = aPredicate;

    while( sNextPredicate != NULL )
    {
        if( ( sNextPredicate->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
            == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE )
        {
            for( sMorePredicate = sNextPredicate;
                 sMorePredicate != NULL;
                 sMorePredicate = sMorePredicate->more )
            {
                IDE_TEST( setMySelectivityOffset( aTemplate,
                                                  aStatInfo,
                                                  aDepInfo,
                                                  sMorePredicate )
                          != IDE_SUCCESS );
            }

            IDE_TEST( setTotalSelectivityOffset( aTemplate,
                                                 aStatInfo,
                                                 sNextPredicate )
                      != IDE_SUCCESS );
        }
        else
        {
            // more list ��ü�� ȣ��Ʈ ������ ������ ���� �����Ƿ�
            // total selectivity�� �ٽ� ���� �ʿ䰡 ����.
            // Nothing to do.
        }

        sNextPredicate = sNextPredicate->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getSelectivity4KeyRange( qcTemplate      * aTemplate,
                                         qmoStatistics   * aStatInfo,
                                         qmoIdxCardInfo  * aIdxCardInfo,
                                         qmoPredWrapper  * aKeyRange,
                                         SDouble         * aSelectivity,
                                         idBool            aInExecutionTime )
{
    UInt              sPredCnt;
    qmoPredWrapper  * sPredWrapper;
    qmoPredicate    * sPredicate;
    qtcNode         * sCompareNode;
    SDouble           sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getSelectivity4KeyRange::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aIdxCardInfo != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    for( sPredCnt = 0, sPredWrapper = aKeyRange;
         sPredWrapper != NULL;
         sPredCnt++, sPredWrapper = sPredWrapper->next )
    {
        sPredicate = sPredWrapper->pred;

        if( ( sPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
        }
        else
        {
            sCompareNode = sPredicate->node;
        }

        if( sCompareNode->node.next != NULL )
        {
            // or ������ ������ �Ѿ� �ִ� ���
            break;
        }

        if( sCompareNode->node.module != &mtfEqual )
        {
            // mtfEqual �̿ܿ� �ٸ� ������ ���� ���
            break;
        }
    }

    if( aStatInfo->isValidStat == ID_TRUE &&
        aIdxCardInfo->index->keyColCount == sPredCnt )
    {
        // BUG-48120
        if ( (QCU_OPTIMIZER_INDEX_COST_MODE & QMG_OPTI_INDEX_COST_MODE_1_MASK) == 1 )
        {
            aIdxCardInfo->flag &= ~QMO_STAT_CARD_ALL_EQUAL_IDX_MASK;
            aIdxCardInfo->flag |= QMO_STAT_CARD_ALL_EQUAL_IDX_TRUE;

            // scan method �񱳸� ���� cost������ pred selectivity�� �Ѵ�.
            IDE_TEST( getSelectivity4PredWrapper( aTemplate,
                                                  aKeyRange,
                                                  &sSelectivity,
                                                  aInExecutionTime )
                      != IDE_SUCCESS );
        }
        else
        {
            // BUG-48210 ������ �����ϰ�
            sSelectivity = 1 / aIdxCardInfo->KeyNDV;
        }
    }
    else
    {
        IDE_TEST( getSelectivity4PredWrapper( aTemplate,
                                              aKeyRange,
                                              & sSelectivity,
                                              aInExecutionTime )
                  != IDE_SUCCESS );
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::getSelectivity4PredWrapper( qcTemplate      * aTemplate,
                                            qmoPredWrapper  * aPredWrapper,
                                            SDouble         * aSelectivity,
                                            idBool            aInExecutionTime )
{
/******************************************************************************
 *
 * Description : qmoPredWrapper list �� ���� ���� selecltivity ��ȯ
 *               (qmgSelection �� access method �� ���� selectivity ���� ȣ��)
 *
 * Implementation : S = PRODUCT( Predicate selectivity for wrapper list )
 *
 *           1. qmoPredWrapper list �� NULL �̸�
 *              Predicate selectivity = 1
 *
 *           2. qmoPredWrapper list �� NULL �� �ƴϸ�
 *           2.1. qmoPredicate �� LIST �����̸�
 *                Predicate selectivity = MS(mySelectivity)
 *           2.2  qmoPredicate �� LIST ���°� �ƴϸ�
 *                Predicate selectivity = TS(totalSelectivity)
 *
 *****************************************************************************/

    qmoPredWrapper  * sPredWrapper;
    SDouble           sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getSelectivity4PredWrapper::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sSelectivity = 1;
    sPredWrapper = aPredWrapper;

    while( sPredWrapper != NULL )
    {
        if( sPredWrapper->pred->id == QMO_COLUMNID_LIST )
        {
            sSelectivity *= getMySelectivity( aTemplate,
                                              sPredWrapper->pred,
                                              aInExecutionTime );
        }
        else
        {
            sSelectivity *= getTotalSelectivity( aTemplate,
                                                 sPredWrapper->pred,
                                                 aInExecutionTime );
        }

        sPredWrapper = sPredWrapper->next;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::getTotalSelectivity4PredList( qcStatement  * aStatement,
                                              qmoPredicate * aPredList,
                                              SDouble      * aSelectivity,
                                              idBool         aInExecutionTime )
{
/******************************************************************************
 *
 * Description : qmgSelection, qmgHierarchy graph �� qmoPredicate list �� ����
 *               ���� selecltivity ��ȯ
 *
 * Implementation : S = PRODUCT( totalSelectivity for qmoPredicate list )
 *
 *       cf) aInExecutionTime (qmo::optimizeForHost ����)
 *           (aStatement->myPlan->scanDecisionFactors == NULL)
 *        1. qmgSelection
 *         - Prepare time : ID_FALSE
 *         - Execution time : ID_FALSE
 *        2. qmgHierarchy : �׻� ID_FALSE
 *
 *****************************************************************************/

    qcTemplate    * sTemplate;
    qmoPredicate  * sPredicate;
    SDouble         sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getTotalSelectivity4PredList::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    sSelectivity = 1;
    sPredicate = aPredList;

    if( aInExecutionTime == ID_TRUE )
    {
        sTemplate = QC_PRIVATE_TMPLATE( aStatement );
    }
    else
    {
        sTemplate = QC_SHARED_TMPLATE( aStatement );
    }

    while( sPredicate != NULL )
    {
        sSelectivity *= getTotalSelectivity( sTemplate,
                                             sPredicate,
                                             aInExecutionTime );

        sPredicate = sPredicate->next;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setMySelectivity4Join( qcStatement  * aStatement,
                                       qmgGraph     * aGraph,
                                       qmoPredicate * aJoinPredList,
                                       idBool         aIsSetNext )
{
/******************************************************************************
 *
 * Description : Join predicate list �� ���� qmoPredicate.mySelectivity ���
 *
 * Implementation : join selectivity ��� ��
 *                  ���� �ε��� ��밡�� ���� ����(QMO_PRED_NEXT_KEY_USABLE_MASK)
 *
 *     1. aIsSetNext �� TRUE �� ��� : qmoPredicate list ��ü�� ���� ����
 *     => join predicate ���� �̹� �з�
 *
 *     1.1. qmoPredicate �� ���� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2
 *          S = 1-PRODUCT(1-US)n    (OR Ȯ������)
 *
 *     1.2. qmoPredicate �� �� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1
 *          S = US (unit selectivity)
 *
 *     2. aIsSetNext �� FALSE �� ��� : qmoPredicate �ϳ��� ���� ����
 *     => Join order ������ graph::optimize �������̶� predicate �з��� �ҿ���,
 *        �� �� child graph �� �� qmoPredicate �� dependency �� ���Ͽ�
 *        join ���� predicate�� ���� �� �Լ��� ȣ���Ѵ�.
 *        �� ���� �ϳ��� qmoPredicat �� ���� mySelectiity �� ���Ѵ�.
 *
 *        < mySelectivity ��� ���� >
 *     1. baseGraph �� ��� optimize �������� ���ʷ� ���
 *     2. qmoCnfMgr::joinOrdering �� ���� ������ qmgJOIN.graph �� ���
 *      - �Ϻ� qmoPredicate.mySelectivity �� ���������� ���ǰ�
 *        (ordered hint����� join group�� predicate������ ���� �ʱ� ����)
 *      - qmgJoin::optimize �� ���� ���� predicate �� Ȯ��������
 *        ���ġ �� �� ����� ���ȴ�.
 *
 *****************************************************************************/

    qmoPredicate   * sPredicate;
    qtcNode        * sCompareNode;
    SDouble          sSelectivity;
    SDouble          sUnitSelectivity;
    idBool           sIsDNF;
    idBool           sIsUsableNextKey;
    idBool           sIsEqual;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setMySelectivity4Join::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement != NULL );

    //--------------------------------------
    // Selectivity ���
    // �����ε��� ��밡�ɿ��� ����
    //--------------------------------------

    sIsUsableNextKey = ID_TRUE;
    sPredicate = aJoinPredList;

    while( sPredicate != NULL )
    {
        sSelectivity = 1;
        sUnitSelectivity = 1;
        sIsDNF = ID_FALSE;

        if( ( sPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
        }
        else
        {
            sCompareNode = sPredicate->node;
            sIsDNF = ID_TRUE;
        }

        if( sCompareNode->node.next != NULL && sIsDNF == ID_FALSE )
        {
            // 1.1. OR ������ �񱳿����ڰ� ������ �ִ� ���,
            //      OR �������ڿ� ���� selectivity ���.
            //      1 - (1-a)(1-b).....
            // ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2

            while( sCompareNode != NULL )
            {
                IDE_TEST( getUnitSelectivity4Join( aStatement,
                                                   aGraph,
                                                   sCompareNode,
                                                   & sIsEqual,
                                                   & sUnitSelectivity )
                          != IDE_SUCCESS );

                sSelectivity *= ( 1 - sUnitSelectivity );

                if( sIsUsableNextKey == ID_TRUE )
                {
                    sIsUsableNextKey = sIsEqual;
                }
                else
                {
                    // Nothing To Do
                }

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }

            sSelectivity = ( 1 - sSelectivity );
        }
        else
        {
            // 1.2. OR ��� ������ �񱳿����ڰ� �ϳ��� ����
            // ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1

            IDE_TEST( getUnitSelectivity4Join( aStatement,
                                               aGraph,
                                               sCompareNode,
                                               & sIsUsableNextKey,
                                               & sSelectivity )
                      != IDE_SUCCESS );
        }

        sPredicate->mySelectivity = sSelectivity;

        IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                         "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                         sSelectivity );

        if( sIsUsableNextKey == ID_TRUE )
        {
            sPredicate->flag &= ~QMO_PRED_NEXT_KEY_USABLE_MASK;
            sPredicate->flag |= QMO_PRED_NEXT_KEY_USABLE;
        }
        else
        {
            sPredicate->flag &= ~QMO_PRED_NEXT_KEY_USABLE_MASK;
            sPredicate->flag |= QMO_PRED_NEXT_KEY_UNUSABLE;
        }

        if ( aIsSetNext == ID_FALSE )
        {
            // To Fix BUG-10542
            // 2. Predicate list �� �ƴ� �ϳ��� qmoPredicate �� ���ؼ��� ����
            break;
        }
        else
        {
            sPredicate = sPredicate->next;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setMySelectivity4OneTable( qcStatement  * aStatement,
                                           qmgGraph     * aLeftGraph,
                                           qmgGraph     * aRightGraph,
                                           qmoPredicate * aOneTablePred )
{
/******************************************************************************
 *
 * Description : Outer join �� onConditionCNF->oneTablePredicate �� ����
 *               qmoPredicate.mySelectivity ���
 *
 * Implementation :
 *
 *          Child graph �� depInfo �� ����Ͽ�
 *       1. ���� ��� ���� join graph �� ��� setMySelectivity4Join ����
 *       2. �� �� setMySelectivity ����
 *
 *          < mySelectivity ��� ���� >
 *       1. baseGraph �� ��� optimize �������� ���ʷ� ���
 *       2. qmoCnfMgr::joinOrdering �� ���� ������ qmgJOIN.graph �� ���
 *        - �Ϻ� qmoPredicate.mySelectivity �� ���������� ���ǰ�
 *          (ordered hint����� join group�� predicate������ ���� �ʱ� ����)
 *        - qmgJoin::optimize �� ���� ���� predicate �� Ȯ��������
 *          ���ġ �� �� ����� ���ȴ�.
 *
 *****************************************************************************/

    qmoPredicate  * sOneTablePred;
    qcDepInfo       sDependencies;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setMySelectivity4OneTable::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement  != NULL );
    IDE_DASSERT( aLeftGraph  != NULL );
    IDE_DASSERT( aRightGraph != NULL );

    //--------------------------------------
    // mySelectivity ���
    //--------------------------------------

    sOneTablePred = aOneTablePred;

    while( sOneTablePred != NULL )
    {
        sOneTablePred->id = QMO_COLUMNID_NON_INDEXABLE;

        // To Fix BUG-8742
        qtc::dependencyAnd( & sOneTablePred->node->depInfo,
                            & aLeftGraph->depInfo,
                            & sDependencies );

        //------------------------------------------
        // To Fix BUG-10542
        // outer join �迭 ������ outer join�� ���,
        // outer join�� ���� graph�� one table predicate�� selectivity��
        // join predicate �̱� ������ join selectivity ���ϴ� �Լ��� ����ؼ�
        // selectivity�� ���ؾ� ��
        //------------------------------------------

        if ( qtc::dependencyEqual( & sDependencies,
                                   & qtc::zeroDependencies ) == ID_TRUE )
        {
            if ( aRightGraph->myFrom->joinType == QMS_NO_JOIN )
            {
                IDE_TEST( setMySelectivity( aStatement,
                                            aRightGraph->myFrom->tableRef->statInfo,
                                            & aRightGraph->depInfo,
                                            sOneTablePred )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( setMySelectivity4Join( aStatement,
                                                 aRightGraph,
                                                 sOneTablePred,
                                                 ID_FALSE )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            if ( aLeftGraph->myFrom->joinType == QMS_NO_JOIN )
            {
                IDE_TEST( setMySelectivity( aStatement,
                                            aLeftGraph->myFrom->tableRef->statInfo,
                                            & aLeftGraph->depInfo,
                                            sOneTablePred )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( setMySelectivity4Join( aStatement,
                                                 aLeftGraph,
                                                 sOneTablePred,
                                                 ID_FALSE )
                          != IDE_SUCCESS );
            }
        }

        sOneTablePred = sOneTablePred->next;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setJoinSelectivity( qcStatement  * aStatement,
                                    qmgGraph     * aGraph,
                                    qmoPredicate * aJoinPred,
                                    SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgJoin, qmgSemiJoin, qmgAntiJoin �� ���� selectivity ���
 *               (WHERE clause, ON clause ��� ����� ����)
 *
 * Implementation :
 *
 *       1. Join graph ���� mySelectivity ���
 *       2. graph->myPredicate list �� ���� ���� selectivity ȹ��
 *            S = PRODUCT( mySelectivity for graph->myPredicate)
 *
 *      cf) mySelectivity ��� ����
 *       1. baseGraph �� ��� optimize �������� ���ʷ� ���
 *       2. qmoCnfMgr::joinOrdering �� ���� ������ qmgJOIN.graph �� ���
 *        - �Ϻ� qmoPredicate.mySelectivity �� ���������� ���ǰ�
 *          (ordered hint����� join group�� predicate������ ���� �ʱ� ����)
 *        - qmgJoin::optimize �� ���� ���� predicate �� Ȯ��������
 *          ���ġ �� �� ����� ���ȴ�.
 *
 *****************************************************************************/

    SDouble sSelectivity = 1;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setJoinSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aGraph       != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // 1. mySelectivity ���
    //--------------------------------------

    IDE_TEST( setMySelectivity4Join( aStatement,
                                     aGraph,
                                     aJoinPred,
                                     ID_TRUE )
              != IDE_SUCCESS );

    //--------------------------------------
    // 2. ���� selectivity ���
    //--------------------------------------

    switch( aGraph->type )
    {
        case QMG_INNER_JOIN:
            IDE_TEST( getMySelectivity4PredList( aJoinPred,
                                                 & sSelectivity )
                      != IDE_SUCCESS );
            break;

        case QMG_SEMI_JOIN:
            IDE_TEST( getSemiJoinSelectivity( aStatement,
                                              aGraph,
                                              & aGraph->left->depInfo,
                                              aJoinPred,
                                              ID_TRUE,   // Left semi
                                              ID_TRUE,   // List
                                              & sSelectivity )
                      != IDE_SUCCESS );
            break;

        case QMG_ANTI_JOIN:
            IDE_TEST( getAntiJoinSelectivity( aStatement,
                                              aGraph,
                                              & aGraph->left->depInfo,
                                              aJoinPred,
                                              ID_TRUE,   // Left anti
                                              ID_TRUE,   // List
                                              & sSelectivity )
                      != IDE_SUCCESS );
            break;

        default:
            IDE_DASSERT(0);
            break;
    }

    *aSelectivity = sSelectivity;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setOuterJoinSelectivity( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qmoPredicate * aWherePred,
                                         qmoPredicate * aOnJoinPred,
                                         qmoPredicate * aOneTablePred,
                                         SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgLOJN, qmgFOJN �� ���� selectivity ���
 *
 * Implementation :
 *
 *     1. Predicate list �� ���� mySelectivity ����
 *     2. Selectivity ��� : S = PRODUCT(MS for on clause)
 *
 *****************************************************************************/

    SDouble sOnJoinSelectivity;
    SDouble sOneTableSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setOuterJoinSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aGraph        != NULL );
    IDE_DASSERT( aGraph->left  != NULL );
    IDE_DASSERT( aGraph->right != NULL );
    IDE_DASSERT( aSelectivity  != NULL );

    //--------------------------------------
    // mySelectivity ����
    //--------------------------------------

    // WHERE ���� join predicate �� ���� mySelectivity
    IDE_TEST( setMySelectivity4Join( aStatement,
                                     aGraph,
                                     aWherePred,
                                     ID_TRUE )
              != IDE_SUCCESS );

    // ON ���� join predicate �� ���� mySelectivity
    IDE_TEST( setMySelectivity4Join( aStatement,
                                     aGraph,
                                     aOnJoinPred,
                                     ID_TRUE )
              != IDE_SUCCESS );

    // ON ���� one table predicate �� ���� mySelectivity
    IDE_TEST( setMySelectivity4OneTable( aStatement,
                                         aGraph->left,
                                         aGraph->right,
                                         aOneTablePred )
              != IDE_SUCCESS );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // ON ���� ���� join selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOnJoinPred,
                                         & sOnJoinSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� one table predicate selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOneTablePred,
                                         & sOneTableSelectivity )
              != IDE_SUCCESS );

    *aSelectivity = sOnJoinSelectivity * sOneTableSelectivity;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::setLeftOuterOutputCnt( qcStatement  * aStatement,
                                       qmgGraph     * aGraph,
                                       qcDepInfo    * aLeftDepInfo,
                                       qmoPredicate * aWherePred,
                                       qmoPredicate * aOnJoinPred,
                                       qmoPredicate * aOneTablePred,
                                       SDouble        aLeftOutputCnt,
                                       SDouble        aRightOutputCnt,
                                       SDouble      * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgLeftOuter �� ���� output record count ���
 *
 * Implementation : ������ ������ ����.
 *
 *    Output record count = Selectivity(where ��) * Input record count
 *                        = Selectivity(where ��)
 *                          * ( LeftAntiCnt(left anti join for on clause)
 *                            + OnJoinCnt(join for on clause) )
 *
 *  - LeftAntiCnt(anti join)
 *              = T(L) * Selectivity(left anti join for on clause)
 *              = T(L) * PRODUCT(left anti join selectivity for on clause)
 *  - OnJoinCnt(inner join)
 *              = T(L) * T(R) * Selectivity(join selectivity for on clause)
 *              = T(L) * T(R) * PRODUCT(MS for on clause)
 *
 *    cf) Left anti join selectivity = 1 - Right NDV / MAX(Left NDV, Right NDV)
 *        Join selectivity = 1 / MAX(Left NDV, Right NDV)
 *
 *****************************************************************************/

    SDouble   sWhereSelectivity;
    SDouble   sLeftAntiSelectivity;
    SDouble   sOnJoinSelectivity;
    SDouble   sOneTableSelectivity;
    SDouble   sLeftAntiCnt;
    SDouble   sOnJoinCnt;
    SDouble   sOutputRecordCnt;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setLeftOuterOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aLeftDepInfo     != NULL );
    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aLeftOutputCnt  > 0 );
    IDE_DASSERT( aRightOutputCnt > 0 );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // WHERE ���� ���� join selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aWherePred,
                                         & sWhereSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� join selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOnJoinPred,
                                         & sOnJoinSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� one table predicate selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOneTablePred,
                                         & sOneTableSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� left anti selectivity ȹ��
    IDE_TEST( getAntiJoinSelectivity( aStatement,
                                      aGraph,
                                      aLeftDepInfo,
                                      aOnJoinPred,
                                      ID_TRUE,   // Left anti
                                      ID_TRUE,   // List
                                      & sLeftAntiSelectivity )
              != IDE_SUCCESS );

    //--------------------------------------
    // Output record count ���
    //--------------------------------------

    sLeftAntiCnt = aLeftOutputCnt * sLeftAntiSelectivity;
    sOnJoinCnt = aLeftOutputCnt * aRightOutputCnt *
        sOnJoinSelectivity * sOneTableSelectivity;

    sOutputRecordCnt = sWhereSelectivity * ( sLeftAntiCnt + sOnJoinCnt );

    // Output count ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1: sOutputRecordCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::setFullOuterOutputCnt( qcStatement  * aStatement,
                                       qmgGraph     * aGraph,
                                       qcDepInfo    * aLeftDepInfo,
                                       qmoPredicate * aWherePred,
                                       qmoPredicate * aOnJoinPred,
                                       qmoPredicate * aOneTablePred,
                                       SDouble        aLeftOutputCnt,
                                       SDouble        aRightOutputCnt,
                                       SDouble      * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgFullOuter �� ���� output record count ���
 *
 * Implementation : ������ ������ ����.
 *
 *    Output record count = Selectivity(where ��) * Input record count
 *                        = Selectivity(where ��)
 *                          * ( LeftAntiCnt(left anti join for on clause)
 *                            + RightAntiCnt(right anti join for on clause)
 *                            + OnJoinCnt(join for on clause) )
 *
 *  - LeftAntiCnt(anti join)
 *              = T(L) * Selectivity(left anti join for on clause)
 *              = T(L) * PRODUCT( left anti join selectivity for on clause )
 *  - RightAntiCnt(anti join)
 *              = T(R) * Selectivity(right anti join for on clause)
 *              = T(R) * PRODUCT( right anti join selectivity for on clause )
 *  - OnJoinCnt(inner join)
 *              = T(L) * T(R) * Selectivity( join selectivity for on clause)
 *              = T(L) * T(R) * PRODUCT(MS for on clause )
 *
 *    cf) Left anti join selectivity = 1 - Right NDV / MAX( Left NDV, Right NDV )
 *        Right anti join selectivity = 1 - Left NDV / MAX( Left NDV, Right NDV )
 *        Join selectivity = 1 / MAX( Left NDV, Right NDV )
 *
 *****************************************************************************/

    SDouble   sWhereSelectivity;
    SDouble   sLeftAntiSelectivity;
    SDouble   sRightAntiSelectivity;
    SDouble   sOnJoinSelectivity;
    SDouble   sOneTableSelectivity;
    SDouble   sLeftAntiCnt;
    SDouble   sRightAntiCnt;
    SDouble   sOnJoinCnt;
    SDouble   sOutputRecordCnt;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setFullOuterOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aLeftDepInfo     != NULL );
    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aLeftOutputCnt  > 0 );
    IDE_DASSERT( aRightOutputCnt > 0 );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // WHERE ���� ���� join selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aWherePred,
                                         & sWhereSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� join selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOnJoinPred,
                                         & sOnJoinSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� one table predicate selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( aOneTablePred,
                                         & sOneTableSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� left anti selectivity ȹ��
    IDE_TEST( getAntiJoinSelectivity( aStatement,
                                      aGraph,
                                      aLeftDepInfo,
                                      aOnJoinPred,
                                      ID_TRUE,   // Left anti
                                      ID_TRUE,   // List
                                      & sLeftAntiSelectivity )
              != IDE_SUCCESS );

    // ON ���� ���� right anti selectivity ȹ��
    IDE_TEST( getAntiJoinSelectivity( aStatement,
                                      aGraph,
                                      aLeftDepInfo,
                                      aOnJoinPred,
                                      ID_FALSE,  // Right anti
                                      ID_TRUE,   // List
                                      & sRightAntiSelectivity )
              != IDE_SUCCESS );

    //--------------------------------------
    // Output record count ���
    //--------------------------------------

    sLeftAntiCnt = aLeftOutputCnt * sLeftAntiSelectivity;
    sRightAntiCnt = aRightOutputCnt * sRightAntiSelectivity;
    sOnJoinCnt = aLeftOutputCnt * aRightOutputCnt *
        sOnJoinSelectivity * sOneTableSelectivity;

    sOutputRecordCnt =
        ( sLeftAntiCnt + sRightAntiCnt + sOnJoinCnt ) * sWhereSelectivity;

    // Output count ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1: sOutputRecordCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setJoinOrderFactor( qcStatement  * aStatement,
                                    qmgGraph     * aJoinGraph,
                                    qmoPredicate * aJoinPred,
                                    SDouble      * aJoinOrderFactor,
                                    SDouble      * aJoinSize )
{
/******************************************************************************
 *
 * Description : Join ordering �� ���� JoinSize �� JoinOrderFactor ���
 *
 * Implementation :
 *
 *    (1) getSelectivity4JoinOrder �� ���� selectivity ȹ��
 *    (2) JoinSize
 *        - SCAN or PARTITION : [left output] * [right output] * Selectivity
 *        - Etc (join) : [left input] * [right input] * Selectivity
 *    (3) JoinOrderFactor :
 *        - SCAN or PARTITION : JoinSize / ( [left input] + [right input] )
 *        - Etc (join) : JoinSize / ( [left output] + [right output] )
 *
 *    cf) Outer join �� ��� ON ���� join predicate �� ���
 *        => ON ���� one table predicate �� ����
 *
 *****************************************************************************/

    qmgGraph * sLeft;
    qmgGraph * sRight;
    SDouble    sInputRecordCnt;
    SDouble    sLeftOutput;
    SDouble    sRightOutput;
    SDouble    sSelectivity;
    SDouble    sJoinSize;
    SDouble    sJoinOrderFactor;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setJoinOrderFactor::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aJoinGraph       != NULL );
    IDE_DASSERT( aJoinOrderFactor != NULL );
    IDE_DASSERT( aJoinSize        != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sLeft  = aJoinGraph->left;
    sRight = aJoinGraph->right;
    sSelectivity = 1.0;

    //------------------------------------------
    // Join Predicate�� selectivity ���
    //------------------------------------------

    // To Fix PR-8266
    if ( aJoinPred != NULL )
    {
        IDE_TEST( getSelectivity4JoinOrder( aStatement,
                                            aJoinGraph,
                                            aJoinPred,
                                            & sSelectivity )
                  != IDE_SUCCESS );
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // JoinSize ���
    //------------------------------------------
    switch ( sLeft->type )
    {
        case QMG_SELECTION :
        case QMG_PARTITION :
        case QMG_SHARD_SELECT :    // PROJ-2638
            sLeftOutput     = sLeft->costInfo.inputRecordCnt;
            sInputRecordCnt = sLeft->costInfo.outputRecordCnt;
            break;

        case QMG_INNER_JOIN :
        case QMG_SEMI_JOIN :
        case QMG_ANTI_JOIN :
            sLeftOutput     = ((qmgJOIN*)sLeft)->joinSize;
            sInputRecordCnt = sLeftOutput;
            break;

        case QMG_LEFT_OUTER_JOIN :
            sLeftOutput     = ((qmgLOJN*)sLeft)->joinSize;
            sInputRecordCnt = sLeftOutput;
            break;

        case QMG_FULL_OUTER_JOIN :
            sLeftOutput     = ((qmgFOJN*)sLeft)->joinSize;
            sInputRecordCnt = sLeftOutput;
            break;

        default :
            IDE_DASSERT(0);
            IDE_TEST( IDE_FAILURE );
    }

    switch ( sRight->type )
    {
        case QMG_SELECTION :
        case QMG_PARTITION :
        case QMG_SHARD_SELECT :    // PROJ-2638
            sRightOutput     = sRight->costInfo.inputRecordCnt;
            sInputRecordCnt *= sRight->costInfo.outputRecordCnt;
            break;

        case QMG_INNER_JOIN :
        case QMG_SEMI_JOIN :
        case QMG_ANTI_JOIN :
            sRightOutput     = ((qmgJOIN*)sRight)->joinSize;
            sInputRecordCnt *= sRightOutput;
            break;

        case QMG_LEFT_OUTER_JOIN :
            sRightOutput     = ((qmgLOJN*)sRight)->joinSize;
            sInputRecordCnt *= sRightOutput;
            break;

        case QMG_FULL_OUTER_JOIN :
            sRightOutput     = ((qmgFOJN*)sRight)->joinSize;
            sInputRecordCnt *= sRightOutput;
            break;

        default :
            IDE_DASSERT(0);
            IDE_TEST( IDE_FAILURE );
    }

    sJoinSize = sInputRecordCnt * sSelectivity;

    switch( aJoinGraph->type )
    {
        case QMG_SEMI_JOIN:
            // |R SJ S| = MIN(|R|, |R JOIN S|)
            if( sJoinSize > sLeft->costInfo.outputRecordCnt )
            {
                sJoinSize = sLeft->costInfo.outputRecordCnt;
            }
            else
            {
                // Nothing to do.
            }
            break;
        case QMG_ANTI_JOIN:
            // |R AJ S| = |R| x 0.1,              if SF(R SJ S) = 1 (i.e. |R SJ S| = |R|)
            //            |R| x (1 - SF(R SJ S)), otherwise
            if( sJoinSize > sLeft->costInfo.outputRecordCnt )
            {
                sJoinSize = sLeft->costInfo.outputRecordCnt * 0.1;
            }
            else
            {
                sJoinSize = sLeft->costInfo.outputRecordCnt - sJoinSize;
            }
            break;
        default:
            // Inner join ��
            break;
    }


    // To Fix PR-8005
    // 0�� �Ǵ� ���� �����Ͽ��� ��
    sJoinSize = ( sJoinSize < 1 ) ? 1 : sJoinSize;

    //------------------------------------------
    // JoinOrderFactor ���
    //------------------------------------------
    sJoinOrderFactor = sJoinSize / ( sLeftOutput + sRightOutput );

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    *aJoinOrderFactor = sJoinOrderFactor;
    *aJoinSize        = sJoinSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setHierarchySelectivity( qcStatement  * aStatement,
                                         qmoPredicate * aWherePredList,
                                         qmoCNF       * aConnectByCNF,
                                         qmoCNF       * aStartWithCNF,
                                         idBool         aInExecutionTime,
                                         SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgHierarchy �� ���� selectivity ���
 *
 * Implementation :
 *
 *     1. �� �������� ���� selectivity ȹ��
 *     1.1. whereSelectivity = PRODUCT(selectivity for where clause)
 *          (where predicate �� qmgSelection ���� push ���� ����)
 *     1.2. connectBySelectivity = PRODUCT(selectivity for connect by clause)
 *     1.3. startWithSelectivity = PRODUCT(selectivity for start with clause)
 *
 *     2. qmgHierarchy selectivity ȹ��
 *        S = whereSelectivity * connectBySelectivity * startWithSelectivity
 *
 *****************************************************************************/

    SDouble    sWhereSelectivity;
    SDouble    sConnectBySelectivity;
    SDouble    sStartWithSelectivity;
    SDouble    sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setHierarchySelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aConnectByCNF != NULL );
    IDE_DASSERT( aSelectivity  != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    IDE_TEST( getTotalSelectivity4PredList( aStatement,
                                            aWherePredList,
                                            & sWhereSelectivity,
                                            aInExecutionTime )
              != IDE_SUCCESS );

    IDE_TEST( getTotalSelectivity4PredList( aStatement,
                                            aConnectByCNF->oneTablePredicate,
                                            & sConnectBySelectivity,
                                            aInExecutionTime )
              != IDE_SUCCESS );

    if( aStartWithCNF != NULL )
    {
        IDE_TEST( getTotalSelectivity4PredList( aStatement,
                                                aStartWithCNF->oneTablePredicate,
                                                & sStartWithSelectivity,
                                                aInExecutionTime )
                  != IDE_SUCCESS );
    }
    else
    {
        sStartWithSelectivity = 1;
    }

    sSelectivity = sWhereSelectivity *
                   sConnectBySelectivity *
                   sStartWithSelectivity;

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::setCountingSelectivity( qmoPredicate * aStopkeyPred,
                                        SLong          aStopRecordCnt,
                                        SDouble        aInputRecordCnt,
                                        SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgCounting �� ���� selectivity ���
 *
 * Implementation :
 *
 *     1. stopkeyPredicate ����
 *      - OR �ֻ�����带 ���� unit predicate ���� ����(CNF)
 *      - rownum = 1
 *      - rownum lessthan(<, <=, >, >=) ���,ȣ��Ʈ����
 *     1.1. stopkeyPredicate �� ȣ��Ʈ ������ ���� (mtfEqual �Ұ�)
 *          S = Default selectivity for opearator
 *     1.2. stopkeyPredicate �� ȣ��Ʈ ���� ����
 *          S = MIN( stopRecordCount, InputRecordCount ) / InputRecordCount
 *     2. stopkeyPredicate �� NULL : S = 1
 *
 *****************************************************************************/

    qmoPredicate  * sStopkeyPred;
    qtcNode       * sCompareNode;
    SDouble         sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setCountingSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aInputRecordCnt > 0  );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sStopkeyPred = aStopkeyPred;

    if ( sStopkeyPred != NULL )
    {

        sCompareNode = (qtcNode *)sStopkeyPred->node->node.arguments;

        // -2 ������ �� ����.
        IDE_DASSERT( aStopRecordCnt >= -1 );
        IDE_DASSERT( ( sStopkeyPred->node->node.lflag & MTC_NODE_OPERATOR_MASK )
                     == MTC_NODE_OPERATOR_OR )
        IDE_DASSERT( sCompareNode->node.module == &mtfEqual ||
                     sCompareNode->node.module == &mtfLessThan ||
                     sCompareNode->node.module == &mtfLessEqual ||
                     sCompareNode->node.module == &mtfGreaterThan ||
                     sCompareNode->node.module == &mtfGreaterEqual );

        if( aStopRecordCnt == -1 )
        {
            // host ���� ����
            // ex) rownum = :a �� stopkeyPredicate ���� �з����� ����
            IDE_DASSERT( sCompareNode->node.module != &mtfEqual );

            sSelectivity = sCompareNode->node.module->selectivity;
        }
        else
        {
            sSelectivity = IDL_MIN( aInputRecordCnt, (SDouble)aStopRecordCnt )
                           / aInputRecordCnt;
        }
    }
    else
    {
        sSelectivity = 1;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setCountingOutputCnt( qmoPredicate * aStopkeyPred,
                                      SLong          aStopRecordCnt,
                                      SDouble        aInputRecordCnt,
                                      SDouble      * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgCounting �� ���� outputRecordCnt ���
 *
 * Implementation :
 *
 *     1. stopkeyPredicate ����
 *      - OR �ֻ�����带 ���� unit predicate ���� ����(CNF)
 *      - rownum = 1
 *      - rownum lessthan(<, <=, >, >=) ���,ȣ��Ʈ����
 *     1.1. stopkeyPredicate �� ȣ��Ʈ ������ ���� (mtfEqual �Ұ�)
 *          outputRecordCnt = InputRecordCount
 *     1.2. stopkeyPredicate �� ȣ��Ʈ ���� ����
 *          outputRecordCnt = MIN( stopRecordCount, InputRecordCount )
 *     2. stopkeyPredicate �� NULL
 *        outputRecordCnt = InputRecordCount
 *     3. outputRecordCnt ����
 *
 *****************************************************************************/

    SDouble sOutputRecordCnt;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setCountingOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aInputRecordCnt  > 0 );

    //--------------------------------------
    // Output count ���
    //--------------------------------------

    if( aStopkeyPred != NULL )
    {
        // -2 ������ �� ����.
        IDE_DASSERT( aStopRecordCnt >= -1 );

        if( aStopRecordCnt == -1 )
        {
            // host ���� ����
            sOutputRecordCnt = aInputRecordCnt;
        }
        else
        {
            // host ���� ����
            sOutputRecordCnt = IDL_MIN( aInputRecordCnt, (SDouble)aStopRecordCnt );
        }
    }
    else
    {
        sOutputRecordCnt = aInputRecordCnt;
    }

    // outputRecordCnt �ּҰ� ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setProjectionSelectivity( qmsLimit * aLimit,
                                          SDouble    aInputRecordCnt,
                                          SDouble  * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgProjection �� ���� selectivity ���
 *
 * Implementation :
 *
 *       1. LIMIT �� ����
 *       1.1. start, count ��� fixed value
 *          - start > inputRecordCnt : S = 0
 *          - start == inputRecordCnt : S = 1 / inputRecordCnt
 *          - inputRecordCnt - start + 1 >= count : S = count / inputRecordCnt
 *          - Etc : S = (inputRecordCnt - start + 1) / inputRecordCnt
 *i                   = 1 - ( (start-1) / inputRecordCnt )
 *       1.2. start, count �ϳ��� variable value : mtfLessThan.selectivity
 *
 *       2. LIMIT �� ���� : S = 1
 *
 *****************************************************************************/

    SDouble sLimitStart;
    SDouble sLimitCount;
    SDouble sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setProjectionSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aInputRecordCnt > 0  );

    //--------------------------------------
    // Limit �� �ݿ��� selectivity ���
    //--------------------------------------

    if( aLimit != NULL )
    {
        // 1. LIMIT �� ����

        if ( ( qmsLimitI::hasHostBind( qmsLimitI::getStart( aLimit ) )
               == ID_FALSE ) &&
             ( qmsLimitI::hasHostBind( qmsLimitI::getCount( aLimit ) )
               == ID_FALSE ) )
        {
            // 1.1. start, count ��� fixed value
            // BUGBUG : ULong->SDouble
            sLimitStart = ID_ULTODB(qmsLimitI::getStartConstant(aLimit));
            sLimitCount = ID_ULTODB(qmsLimitI::getCountConstant(aLimit));

            if (sLimitStart > aInputRecordCnt)
            {
                sSelectivity = 0;
            }
            else if (sLimitStart == aInputRecordCnt)
            {
                sSelectivity = 1 / aInputRecordCnt;
            }
            else
            {
                if( aInputRecordCnt - sLimitStart + 1 >= sLimitCount )
                {
                    sSelectivity = sLimitCount / aInputRecordCnt;
                }
                else
                {
                    sSelectivity = 1 - ( (sLimitStart - 1) / aInputRecordCnt );
                }
            }
        }
        else
        {
            // 1.2. start, count �ϳ��� variable value
            sSelectivity = mtfLessThan.selectivity;
        }
    }
    else
    {
        // 2. LIMIT �� ����
        sSelectivity = 1;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setGroupingSelectivity( qmgGraph     * aGraph,
                                        qmoPredicate * aHavingPred,
                                        SDouble        aInputRecordCnt,
                                        SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgGrouping �� ���� selectivity ���
 *
 * Implementation :
 *
 *       1. QMG_GROP_TYPE_NESTED : S = 1/ inputRecordCnt
 *       2. Etc
 *       2.1. QMG_GROP_OPT_TIP_COUNT_STAR or
 *            QMG_GROP_OPT_TIP_INDEXABLE_MINMAX :
 *            S = 1/ inputRecordCnt
 *       2.2. QMG_GROP_OPT_TIP_NONE (default) or
 *            QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY or
 *            QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG
 *          - HAVING �� ���� : S = PRODUCT(DS for HAVING clause)
 *            having Predicate�� selectivity ��� �Լ� �������� �ʾ���
 *            ���� compare node�� default selectivity�� ������
 *          - �� �� : S = 1
 *       2.3. QMS_GROUPBY_ROLLUP :
 *            S = QMO_SELECTIVITY_ROLLUP_FACTOR (0.75)
 *       2.4. QMS_GROUPBY_CUBE or
 *            QMS_GROUPBY_GROUPING_SETS : S = 1
 *
 *****************************************************************************/

    qmoPredicate  * sHavingPred;
    SDouble         sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setGroupingSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aGraph       != NULL );
    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aInputRecordCnt > 0  );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sHavingPred = aHavingPred;

    // BUG-38132
    // HAVING ���� ��� grouping �� ��ģ�� ó���ϱ� ������
    // selectivity �� ���߾�� �Ѵ�.
    if ( sHavingPred != NULL )
    {
        sSelectivity = QMO_SELECTIVITY_UNKNOWN;
    }
    else
    {
        sSelectivity = QMO_SELECTIVITY_NOT_EXIST;
    }

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    if( ( aGraph->flag & QMG_GROP_TYPE_MASK ) == QMG_GROP_TYPE_NESTED )
    {
        sSelectivity = 1 / aInputRecordCnt;
    }
    else
    {
        switch( aGraph->flag & QMG_GROP_OPT_TIP_MASK )
        {
            case QMG_GROP_OPT_TIP_COUNT_STAR:
            case QMG_GROP_OPT_TIP_INDEXABLE_MINMAX:
                sSelectivity = 1 / aInputRecordCnt;
                break;

            case QMG_GROP_OPT_TIP_NONE:
            case QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY:
            case QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG:
                while( sHavingPred != NULL )
                {
                    // having �� selectivity ���
                    // qmgGrouping::init ���� having ���� CNF �� normalize

                    sSelectivity
                        *= sHavingPred->node->node.arguments->module->selectivity;
                    sHavingPred = sHavingPred->next;
                }
                break;

            // PROJ-1353
            case QMG_GROP_OPT_TIP_ROLLUP:
                sSelectivity = QMO_SELECTIVITY_ROLLUP_FACTOR;
                break;

            case QMG_GROP_OPT_TIP_CUBE:
            case QMG_GROP_OPT_TIP_GROUPING_SETS:
                break;

            default:
                IDE_DASSERT(0);
                break;
        }
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setGroupingOutputCnt( qcStatement      * aStatement,
                                      qmgGraph         * aGraph,
                                      qmsConcatElement * aGroupBy,
                                      SDouble            aInputRecordCnt,
                                      SDouble            aSelectivity,
                                      SDouble          * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgGrouping �� ���� outputRecordCnt ���
 *
 * Implementation : outputRecordCnt ȹ��
 *
 *       1. groupBy ���� NULL �ƴϰ�
 *       1.1. QMG_GROP_TYPE_NESTED
 *            outputRecordCnt = inputRecordCnt * selectivity
 *       1.2  QMG_GROP_OPT_TIP_COUNT_STAR or
 *            QMG_GROP_OPT_TIP_INDEXABLE_MINMAX
 *            outputRecordCnt = inputRecordCnt * selectivity
 *       1.3. QMG_GROP_OPT_TIP_NONE (default) or
 *            QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY or
 *            QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG
 *          - inputRecordCnt ����
 *            group by �÷��� ��� one column list �̰�
 *            group by �� �����ϴ� ��� table �� ���� ������� ���� :
 *            inputRecordCnt = MIN( inputRecordCnt, PRODUCT(columnNDV) )
 *          - outputRecordCnt = inputRecordCnt * selectivity
 *       1.4. QMS_GROUPBY_ROLLUP
 *            outputRecordCnt = inputRecordCnt *
 *                              QMO_SELECTIVITY_ROLLUP_FACTOR (0.75)
 *       1.5. QMS_GROUPBY_CUBE or
 *            QMS_GROUPBY_GROUPING_SETS
 *            outputRecordCnt = inputRecordCnt
 *       2. groupBy ���� NULL : outputRecordCnt = 1
 *       3. outputRecordCnt �ּҰ� ����
 *
 *****************************************************************************/

    qtcNode          * sNode;
    qmsConcatElement * sGroupBy;
    qmoStatistics    * sStatInfo;
    qmoColCardInfo   * sColCardInfo;
    SDouble            sColumnNDV;
    SDouble            sInputRecordCnt;
    SDouble            sOutputRecordCnt;
    idBool             sAllColumn;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setGroupingOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aGraph           != NULL );
    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aInputRecordCnt > 0 );

    //--------------------------------------
    // outputRecordCnt ���
    //--------------------------------------

    sColumnNDV = 1;
    sOutputRecordCnt = 0;
    sAllColumn = ID_TRUE;

    if( aGroupBy != NULL )
    {
        if( ( aGraph->flag & QMG_GROP_TYPE_MASK ) == QMG_GROP_TYPE_NESTED )
        {
            sOutputRecordCnt = aInputRecordCnt * aSelectivity;
        }
        else
        {
            switch( aGraph->flag & QMG_GROP_OPT_TIP_MASK )
            {
                case QMG_GROP_OPT_TIP_COUNT_STAR:
                case QMG_GROP_OPT_TIP_INDEXABLE_MINMAX:
                    sOutputRecordCnt = aInputRecordCnt * aSelectivity;
                    break;

                case QMG_GROP_OPT_TIP_NONE:
                case QMG_GROP_OPT_TIP_INDEXABLE_GROUPBY:
                case QMG_GROP_OPT_TIP_INDEXABLE_DISTINCTAGG:

                    // BUG-38444 grouping �׷����� output record count�� �߸� ����
                    // qmsConcatElement ����ü�� next �� ���󰡾���
                    for ( sGroupBy = aGroupBy;
                          sGroupBy != NULL;
                          sGroupBy = sGroupBy->next )
                    {
                        sNode = sGroupBy->arithmeticOrList;

                        if ( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
                        {
                            sStatInfo = QC_SHARED_TMPLATE(aStatement)->
                                        tableMap[sNode->node.table].
                                        from->tableRef->statInfo;

                            if( sStatInfo->isValidStat == ID_TRUE )
                            {
                                // group by �÷��� ��� one column list
                                sColCardInfo = sStatInfo->colCardInfo;
                                sColumnNDV  *= sColCardInfo[sNode->node.column].columnNDV;
                            }
                            else
                            {
                                // group by �÷��� ��� one column list
                                sColCardInfo = sStatInfo->colCardInfo;
                                sColumnNDV   = sColCardInfo[sNode->node.column].columnNDV;
                            }
                        }
                        else
                        {
                            sAllColumn = ID_FALSE;
                            break;
                        }
                    }

                    // inputRecordCnt ����
                    if ( sAllColumn == ID_TRUE )
                    {
                        sInputRecordCnt = IDL_MIN( aInputRecordCnt, sColumnNDV );
                    }
                    else
                    {
                        sInputRecordCnt = aInputRecordCnt;
                    }

                    sOutputRecordCnt = sInputRecordCnt * aSelectivity;
                    break;

                // PROJ-1353
                case QMG_GROP_OPT_TIP_ROLLUP:
                    sOutputRecordCnt
                        = aInputRecordCnt * QMO_SELECTIVITY_ROLLUP_FACTOR;
                    break;

                case QMG_GROP_OPT_TIP_CUBE:
                case QMG_GROP_OPT_TIP_GROUPING_SETS:
                    sOutputRecordCnt = aInputRecordCnt;
                    break;

                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }
    else
    {
        sOutputRecordCnt = 1;
    }

    // outputRecordCnt �ּҰ� ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setDistinctionOutputCnt( qcStatement * aStatement,
                                         qmsTarget   * aTarget,
                                         SDouble       aInputRecordCnt,
                                         SDouble     * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgDistinction �� ���� outputRecordCnt ���
 *
 * Implementation :
 *
 *      1. outputRecordCnt ȹ��
 *      1.1. target �� �����ϴ� ��� table �� ���� ������� �����̰�
 *           target �÷��� ��� one column list �̰�
 *           prowid pseudo column �� �ƴϸ� :
 *           outputRecordCnt = MIN( inputRecordCnt, PRODUCT(columnNDV) )
 *      1.2. �� �� : outputRecordCnt = inputRecordCnt
 *      2. outputRecordCnt �ּҰ� ����
 *
 *****************************************************************************/

    qmsTarget      * sTarget;
    qtcNode        * sNode;
    qmoStatistics  * sStatInfo;
    qmoColCardInfo * sColCardInfo;
    SDouble          sColumnNDV;
    SDouble          sOutputRecordCnt;
    idBool           sAllColumn;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setDistinctionOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement       != NULL );
    IDE_DASSERT( aTarget          != NULL );
    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aInputRecordCnt > 0 );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sColumnNDV = 1;
    sAllColumn = ID_TRUE;
    sTarget = aTarget;

    //--------------------------------------
    // outputRecordCnt ���
    //--------------------------------------

    // 1. outputRecordCnt ȹ��
    while( sTarget != NULL )
    {
        sNode = sTarget->targetColumn;

        // BUG-20272
        if( sNode->node.module == &mtfDecrypt )
        {
            sNode = (qtcNode*) sNode->node.arguments;
        }
        else
        {
            // Nothing to do.
        }

        // prowid pseudo column �� �� �� ����
        IDE_DASSERT( sNode->node.column != MTC_RID_COLUMN_ID );

        if( QTC_IS_COLUMN( aStatement, sNode ) == ID_TRUE )
        {
            // target �÷��� ��� one column list
            sStatInfo = QC_SHARED_TMPLATE(aStatement)->
                        tableMap[sNode->node.table].
                        from->tableRef->statInfo;

            if( sStatInfo->isValidStat == ID_TRUE )
            {
                sColCardInfo = sStatInfo->colCardInfo;
                sColumnNDV *= sColCardInfo[sNode->node.column].columnNDV;
            }
            else
            {
                sAllColumn = ID_FALSE;
                break;
            }
        }
        else
        {
            sAllColumn = ID_FALSE;
            break;
        }

        sTarget = sTarget->next;
    }

    if ( sAllColumn == ID_TRUE )
    {
        sOutputRecordCnt = IDL_MIN( aInputRecordCnt, sColumnNDV );
    }
    else
    {
        sOutputRecordCnt = aInputRecordCnt;
    }

    // 2. outputRecordCnt �ּҰ� ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setSetOutputCnt( qmsSetOpType   aSetOpType,
                                 SDouble        aLeftOutputRecordCnt,
                                 SDouble        aRightOutputRecordCnt,
                                 SDouble      * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : qmgSet �� ���� outputRecordCnt ���
 *
 * Implementation :
 *
 *     1. outputRecordCnt ȹ��
 *     1.1. QMS_UNION_ALL
 *          outputRecordCnt = left outputRecordCnt + right outputRecordCnt
 *     1.2. QMS_UNION
 *          outputRecordCnt = (left outputRecordCnt + right outputRecordCnt) / 2
 *     1.3. Etc ( QMS_MINUS, QMS_INTERSECT )
 *          outputRecordCnt = left outputRecordCnt / 2
 *
 *     2. outputRecordCnt �ּҰ� ����
 *
 *    cf) 1.2, 1.3 �� ���� outputRecordCnt ���� idea
 *        target �� �����ϴ� ��� table �� ���� ������� �����̰�
 *        target �÷��� ��� one column list �̸�
 *        (SET �� ��� prowid pseudo column �� target �� �� �� ����) :
 *        outputRecordCnt = MIN( outputRecordCnt, PRODUCT(columnNDV) )
 *     => validation �������� tuple �� �Ҵ�޾� target �� ���� ����
 *        tablemap[table].from �� NULL �� �Ǿ�
 *        one column list �� statInfo ȹ�� �Ұ�
 *
 *****************************************************************************/

    SDouble sOutputRecordCnt = 0;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setSetOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aLeftOutputRecordCnt  > 0 );
    IDE_DASSERT( aRightOutputRecordCnt > 0 );

    //--------------------------------------
    // outputRecordCnt ���
    //--------------------------------------

    // 1. outputRecordCnt ȹ��
    switch( aSetOpType )
    {
        case QMS_UNION_ALL:
            sOutputRecordCnt = aLeftOutputRecordCnt + aRightOutputRecordCnt;
            break;
        case QMS_UNION:
            sOutputRecordCnt
                = ( aLeftOutputRecordCnt + aRightOutputRecordCnt ) / 2;
            break;
        case QMS_MINUS:
        case QMS_INTERSECT:
            sOutputRecordCnt = aLeftOutputRecordCnt / 2;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }

    // 2. outputRecordCnt �ּҰ� ����
    *aOutputRecordCnt = ( sOutputRecordCnt < 1 ) ? 1 : sOutputRecordCnt;

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::setMySelectivityOffset( qcTemplate    * aTemplate,
                                        qmoStatistics * aStatInfo,
                                        qcDepInfo     * aDepInfo,
                                        qmoPredicate  * aPredicate )
{
/******************************************************************************
 *
 * Description : Execution �ܰ�(host ������ ���� ���ε��� ��)����
 *               qmo::optimizeForHost ȣ��� scan method �籸���� ����
 *               template->data + qmoPredicate.mySelectivityOffset ��
 *               mySelectivity �� �缳���Ѵ�.
 *
 * Implementation :
 *
 *****************************************************************************/

    SDouble   * sSelectivity;
    SDouble     sUnitSelectivity;
    qtcNode   * sCompareNode;
    idBool      sIsDNF;
    UChar     * sData;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setMySelectivityOffset::__FT__" );

    IDE_DASSERT( aTemplate   != NULL );
    IDE_DASSERT( aStatInfo   != NULL );
    IDE_DASSERT( aDepInfo    != NULL );
    IDE_DASSERT( aPredicate  != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // �� �Լ��� host ������ �����ϴ� more list�� predicate �鿡 ���ؼ���
    // ȣ��ȴ�. ���� indexable column�̾�� �Ѵ�.

    if( ( aPredicate->flag & QMO_PRED_HOST_OPTIMIZE_MASK )
        == QMO_PRED_HOST_OPTIMIZE_TRUE )
    {
        IDE_DASSERT( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE );

        sIsDNF = ID_FALSE;
        sData = aTemplate->tmplate.data;
        sSelectivity = (SDouble*)( sData + aPredicate->mySelectivityOffset );

        if( ( aPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            // CNF�� ���
            sCompareNode = (qtcNode *)(aPredicate->node->node.arguments);
        }
        else
        {
            // DNF�� ���
            sCompareNode = aPredicate->node;
            sIsDNF = ID_TRUE;
        }

        if( sCompareNode->node.next != NULL && sIsDNF == ID_FALSE )
        {
            // CNF�̸鼭 OR ������ �񱳿����ڰ� ������ �ִ� ���,
            // OR �������ڿ� ���� selectivity ���.
            // 1 - (1-a)(1-b).....

            // DNF �� node->next�� ������ �� ����.

            *sSelectivity = 1;

            while( sCompareNode != NULL )
            {
                IDE_TEST( getUnitSelectivity( aTemplate,
                                              aStatInfo,
                                              aDepInfo,
                                              aPredicate,
                                              sCompareNode,
                                              & sUnitSelectivity,
                                              ID_TRUE )
                          != IDE_SUCCESS );

                *sSelectivity *= ( 1 - sUnitSelectivity );

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }

            *sSelectivity = ( 1 - *sSelectivity );
        }
        else
        {
            // DNF �Ǵ� CNF �� �� �����ڰ� �ϳ��� �ִ� ���
            IDE_TEST( getUnitSelectivity( aTemplate,
                                          aStatInfo,
                                          aDepInfo,
                                          aPredicate,
                                          sCompareNode,
                                          sSelectivity,
                                          ID_TRUE )
                      != IDE_SUCCESS );
        }

    }
    else
    {
        // Host ������ ������ ���� �����Ƿ� prepare �������� ����
        // predicate->mySelectivity�� �״�� ����Ѵ�.
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


SDouble
qmoSelectivity::getMySelectivity( qcTemplate   * aTemplate,
                                  qmoPredicate * aPredicate,
                                  idBool         aInExecutionTime )
{
/******************************************************************************
 *
 * Description : PROJ-1446 Host variable�� ������ ���� ����ȭ
 *               aInExecutionTime �� ���ڷ� �޴� �Լ��鿡��
 *               qmoPredicate �� ���� mySelectivity �� ���� �� �̿��Ѵ�.
 *
 * Implementation :
 *
 *     1. Execution �ܰ�(host ������ ���� ���ε��� ��)����
 *        qmo::optimizeForHost �� ���� ȣ���̰� QMO_PRED_HOST_OPTIMIZE_TRUE �̸�
 *        template->data + qmoPredicate.mySelectivityOffset �� ������ ���� ��ȯ
 *     2. �� ���� ���
 *        qmoPredicate.mySelectivity �� ��ȯ
 *
 *****************************************************************************/

    UChar   *sData;
    SDouble  sSelectivity;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate  != NULL );
    IDE_DASSERT( aPredicate != NULL );

    if( ( aInExecutionTime == ID_TRUE ) &&
        ( aPredicate->flag & QMO_PRED_HOST_OPTIMIZE_MASK )
        == QMO_PRED_HOST_OPTIMIZE_TRUE )
    {
        sData = aTemplate->tmplate.data;
        sSelectivity = *(SDouble*)( sData + aPredicate->mySelectivityOffset );
    }
    else
    {
        sSelectivity = aPredicate->mySelectivity;
    }

    return sSelectivity;
}


IDE_RC
qmoSelectivity::setTotalSelectivityOffset( qcTemplate    * aTemplate,
                                           qmoStatistics * aStatInfo,
                                           qmoPredicate  * aPredicate )
{
/******************************************************************************
 *
 * Description : Execution �ܰ�(host ������ ���� ���ε��� ��)����
 *               qmo::optimizeForHost ȣ��� scan method �籸���� ����
 *               template->data + qmoPredicate.totalSelectivityOffset ��
 *               totalSelectivity �� �缳���Ѵ�.
 *
 * Implementation :
 *
 *     1. ���� selectivity �� ���� �� �ִ� ���� : getIntegrateMySelectivity
 *        => setTotalSelectivity ���� ��� QMO_PRED_HEAD_HOST_OPT_TOTAL_TRUE ����
 *     2. ���� selectivity �� ���� �� ���� ���� : PRODUCT( getMySelectivity )
 *
 *****************************************************************************/

    SDouble      * sTotalSelectivity;
    SDouble        sTempSelectivity;
    qmoPredicate * sMorePredicate;
    UChar        * sData;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setTotalSelectivityOffset::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate  != NULL );
    IDE_DASSERT( aStatInfo  != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( ( aPredicate->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
                 == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE );

    //--------------------------------------
    // ���� selectivity ���
    //--------------------------------------

    sData = aTemplate->tmplate.data;
    sTotalSelectivity = (SDouble*)( sData + aPredicate->totalSelectivityOffset );

    if( ( aPredicate->flag & QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK )
        == QMO_PRED_HEAD_HOST_OPT_TOTAL_TRUE )
    {
        // ���� selectivity �� ���� �� �ִ� ����
        IDE_TEST( getIntegrateMySelectivity( aTemplate,
                                             aStatInfo,
                                             aPredicate,
                                             sTotalSelectivity,
                                             ID_TRUE )
                  != IDE_SUCCESS );
    }
    else
    {
        // ���� selectivity �� ���� �� ���� ����
        sTempSelectivity = 1;
        sMorePredicate = aPredicate;

        while( sMorePredicate != NULL )
        {
            sTempSelectivity *= getMySelectivity( aTemplate,
                                                  sMorePredicate,
                                                  ID_TRUE );
            sMorePredicate = sMorePredicate->more;
        }

        *sTotalSelectivity = sTempSelectivity;
    }

    IDE_DASSERT_MSG( *sTotalSelectivity >= 0 && *sTotalSelectivity <= 1,
                     "TotalSelectivity : %"ID_DOUBLE_G_FMT"\n",
                     *sTotalSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


SDouble
qmoSelectivity::getTotalSelectivity( qcTemplate   * aTemplate,
                                     qmoPredicate * aPredicate,
                                     idBool         aInExecutionTime )
{
/******************************************************************************
 *
 * Description : PROJ-1446 Host variable�� ������ ���� ����ȭ
 *               aInExecutionTime �� ���ڷ� �޴� �Լ��鿡��
 *               qmoPredicate �� ���� totalSelectivity �� ���� �� �̿��Ѵ�.
 *
 * Implementation :
 *
 *     1. Execution �ܰ�(host ������ ���� ���ε��� ��)����
 *        qmo::optimizeForHost �� ���� ȣ���̰� QMO_PRED_HOST_OPTIMIZE_TRUE �̸�
 *        template->data + qmoPredicate.totalSelectivityOffset �� ������ ���� ��ȯ
 *     2. �� ���� ���
 *        qmoPredicate.totalSelectivity �� ��ȯ
 *
 *****************************************************************************/

    UChar   *sData;
    SDouble  sSelectivity;

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate  != NULL );
    IDE_DASSERT( aPredicate != NULL );

    if( ( aInExecutionTime == ID_TRUE ) &&
        ( aPredicate->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
        == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE )
    {
        sData = aTemplate->tmplate.data;
        sSelectivity = *(SDouble*)( sData + aPredicate->totalSelectivityOffset );
    }
    else
    {
        sSelectivity = aPredicate->totalSelectivity;
    }

    return sSelectivity;
}


IDE_RC
qmoSelectivity::getIntegrateMySelectivity( qcTemplate    * aTemplate,
                                           qmoStatistics * aStatInfo,
                                           qmoPredicate  * aPredicate,
                                           SDouble       * aSelectivity,
                                           idBool          aInExecutionTime )
{
/******************************************************************************
 *
 * Description : More list �� 2���� indexable qmoPredicate �� ����
 *               Integrate ������ ��� MIN-MAX selectivity �����Ͽ�
 *               total selectivity �� ����Ѵ�.
 *
 * Implementation :
 *
 *     1. Integrate ��� ����
 *        => PROJ-2242 �� �����Ͽ� filter subsumption �� ����Ǿ��ٴ� ����
 *     1.1. �� qmoPredicate �� arguments(unit predicate) ������ 1��
 *     1.2. �� qmoPredicate �� operator �� (>, >=, <, <=) �� �ش�
 *     1.3. �� qmoPredicate �� operator �� ���� �ٸ� ���⼺
 *     1.4. Column (QMO_STAT_MINMAX_COLUMN_SET_FALSE)
 *          ex) FROM ( SELECT ( SELECT T2.I1 FROM T2 LIMIT 1 )A FROM T1 )V1
 *              WHERE V1.A > 1;
 *     1.5. Execution time
 *     1.6. column ���� MTD_SELECTIVITY_ENABLE
 *     1.7. value ���� fixed variable ����
 *       => 1.1~7 ���� ������ QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK ����
 *     1.8. column ���� MTD_GROUP_MISC �� �ƴ� ��
 *          ex) MTD_GROUP_NUMBER, MTD_GROUP_TEXT, MTD_GROUP_DATE, MTD_GROUP_INTERVAL
 *     1.9. column ���� value ���� ���� group
 *     1.10. ������� ����
 *
 *     2. Integrate selectivity ���
 *     2.1. Integrate ��� : S = MIN-MAX selectivity
 *          => ���� ������ ���� ����
 *     2.2 Etc : S = PRODUCT( MS )
 *
 *     cf) MIN-MAX selectivity : �ΰ��� predicate���� max/min value node�� ã�´�.
 *       (1) >, >= �̸�,
 *         - indexArgument = 0 : min value ( ��: i1>5 )
 *         - indexArgument = 1 : max value ( ��: 5>i1 )
 *       (2) <, <= �̸�,
 *         - indexArgument = 0 : max value ( ��: i1<1 )
 *         - indexArgument = 1 : min value ( ��: 1<i1 )
 *       (3) (1),(2)�� ������,
 *         - min/max value node�� ��� �����ϸ�, ���� selectivity �� ����
 *
 *****************************************************************************/

    qmoColCardInfo  * sColCardInfo;
    qmoPredicate    * sPredicate;
    qmoPredicate    * sMorePredicate;
    qtcNode         * sCompareNode;
    qtcNode         * sColumnNode   = NULL;
    qtcNode         * sMinValueNode = NULL;
    qtcNode         * sMaxValueNode = NULL;
    mtcColumn       * sColumnColumn;
    mtcColumn       * sMinValueColumn;
    mtcColumn       * sMaxValueColumn;
    void            * sColumnMaxValue;
    void            * sColumnMinValue;
    void            * sMinValue;
    void            * sMaxValue;
    UInt              sColumnType = 0;
    UInt              sMaxValueType = 0;
    UInt              sMinValueType = 0;
    mtdDoubleType     sDoubleColumnMaxValue;
    mtdDoubleType     sDoubleColumnMinValue;
    mtdDoubleType     sDoubleMaxValue;
    mtdDoubleType     sDoubleMinValue;

    SDouble           sColumnNDV;
    SDouble           sSelectivity;
    // sBoundForm : min/max bound ����
    // 0:   open,   open ( x <  i1 <  y )
    // 1:   open, closed ( x <  i1 <= y )
    //    closed,   open ( x <= i1 <  y )
    // 2: closed, closed ( x <= i1 <= y )
    SDouble           sBoundForm = 0;
    idBool            sIsIntegrate;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getIntegrateMySelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aPredicate->more       != NULL &&
                 aPredicate->more->more == NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sPredicate = aPredicate;
    sColCardInfo = aStatInfo->colCardInfo;
    sSelectivity = 1;
    sIsIntegrate = ID_TRUE;

    //--------------------------------------
    // 1. Integrate ��� ����
    //--------------------------------------

    sMorePredicate = sPredicate;

    while( sMorePredicate != NULL )
    {
        // BUG-43482
        if ( ( aInExecutionTime == ID_TRUE ) &&
             ( ( sMorePredicate->flag & QMO_PRED_HOST_OPTIMIZE_MASK ) != QMO_PRED_HOST_OPTIMIZE_TRUE ) )
        {
            sIsIntegrate = ID_FALSE;
            sMinValueNode = NULL;
            sMaxValueNode = NULL;
            break;
        }
        else
        {
            // Nothing To Do
        }

        // 1.1. �� qmoPredicate �� arguments(unit predicate) ������ 1��
        // => compare node ����

        if( ( sMorePredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sMorePredicate->node->node.arguments);

            if( sCompareNode->node.next != NULL )
            {
                // OR ��� ������ �񱳿����ڰ� �ΰ��̻� �����ϴ� ���,
                // ���� selectivity�� ���� �� ����.
                // ��: i1>1 or i1<2
                sIsIntegrate = ID_FALSE;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }
        else
        {
            // DNF
            sCompareNode = sMorePredicate->node;
        }
        
        // 1.2. �� qmoPredicate �� operator �� (>, >=, <, <=) �� �ش�
        // => column node, min/max value node ȹ��

        switch( sCompareNode->node.module->lflag & MTC_NODE_OPERATOR_MASK )
        {
            case( MTC_NODE_OPERATOR_GREATER_EQUAL ) :
                // >=
                sBoundForm++;        // ���� ���� ���
                /* fall through */
            case( MTC_NODE_OPERATOR_GREATER ) :
                // >
                if( sCompareNode->indexArgument == 0 )
                {
                    sColumnNode = (qtcNode *)(sCompareNode->node.arguments);
                    sMinValueNode =
                        (qtcNode *)(sCompareNode->node.arguments->next);
                }
                else
                {
                    sColumnNode = (qtcNode *)(sCompareNode->node.arguments->next);
                    sMaxValueNode = (qtcNode *)(sCompareNode->node.arguments);
                }
                break;
            case( MTC_NODE_OPERATOR_LESS_EQUAL ) :
                // <=
                sBoundForm++;        // ���� ���� ���
                /* fall through */
            case( MTC_NODE_OPERATOR_LESS ) :
                // <
                if( sCompareNode->indexArgument == 0 )
                {
                    sColumnNode = (qtcNode *)(sCompareNode->node.arguments);
                    sMaxValueNode
                        = (qtcNode *)(sCompareNode->node.arguments->next);
                }
                else
                {
                    sColumnNode = (qtcNode *)(sCompareNode->node.arguments->next);
                    sMinValueNode = (qtcNode *)(sCompareNode->node.arguments);
                }
                break;
            default :
                // (>, >=, <, <=) �� ���� operator
                sIsIntegrate = ID_FALSE;
                break;
        }

        sMorePredicate = sMorePredicate->more;
    }

    // 1.3. �� qmoPredicate �� operator �� ���� �ٸ� ���⼺

    sIsIntegrate = ( sMinValueNode != NULL && sMaxValueNode != NULL ) ?
                   ID_TRUE: ID_FALSE;

    // 1.4. Column (QMO_STAT_MINMAX_COLUMN_SET_FALSE)
    //  ex) FROM ( SELECT ( SELECT T2.I1 FROM T2 LIMIT 1 )A FROM T1 )V1
    //      WHERE V1.A > 1 AND V1.A < 3;

    if( sIsIntegrate == ID_TRUE )
    {
        // BUG-16265
        // view�� target�� subquery�� ��� ���� column������ ������
        // subquery node�� module�� mtdNull module �� �޸���
        // MTD_SELECTIVITY_DISABLE �̹Ƿ� ��������� �������� �ʴ´�.
        // �̷� ��� colCardInfo�� flag�� ��������� �ִ��� �Ǵ��ϰ�,
        // ������ default�� ���ȴ�.

        if( ( sColCardInfo[sColumnNode->node.column].flag &
              QMO_STAT_MINMAX_COLUMN_SET_MASK )
            == QMO_STAT_MINMAX_COLUMN_SET_FALSE )
        {
            sIsIntegrate = ID_FALSE;
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

    if( sIsIntegrate == ID_TRUE )
    {
        //--------------------------------------
        // 1.5. Execution time
        // 1.6. column ���� MTD_SELECTIVITY_ENABLE
        // 1.7. value ���� fixed variable ����
        //   => 1.1~7 ���� ������ QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK ����
        // 1.8. column ���� MTD_GROUP_MISC �� �ƴ� ��
        // 1.9. column ���� value ���� ���� group
        // 1.10. ������� ����
        //--------------------------------------

        IDE_DASSERT( sColumnNode != NULL );

        sColumnColumn = &( aTemplate->tmplate.
                           rows[sColumnNode->node.table].
                           columns[sColumnNode->node.column] );

        //--------------------------------------------
        // ���� ������ ����,
        // ���� selectivity�� ����� �� ���� ����̹Ƿ�,
        // �ΰ� predicate�� selectivity�� ���Ѵ�.
        // (1) predicate�� variable�� �з��Ǵ� ���
        // (2) �ش� �÷��� min/max value�� ���� ���
        // (3) ��¥, ������ �̿��� dataType�� ���
        // BUG-38758
        // (4) deterministic function�� fixed predicate������ ����� �� ����.
        //--------------------------------------------
        // fix BUG-13708
        // �����÷�����Ʈ�� ����� �ΰ��� predicate�� ���� ���
        // ȣ��Ʈ ������ �����ϴ��� �˻�

        if( ( ( aInExecutionTime == ID_FALSE ) &&
              ( QMO_PRED_IS_VARIABLE( aPredicate ) == ID_TRUE ||
                QMO_PRED_IS_VARIABLE( aPredicate->more ) == ID_TRUE ) )
            ||
            ( ( sColumnColumn->module->flag & MTD_SELECTIVITY_MASK )
              == MTD_SELECTIVITY_DISABLE )
            ||
            ( ( ( aPredicate->node->lflag & QTC_NODE_PROC_FUNCTION_MASK )
                == QTC_NODE_PROC_FUNCTION_TRUE ) &&
              ( ( aPredicate->node->lflag & QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK )
                == QTC_NODE_PROC_FUNC_DETERMINISTIC_TRUE ) )
          )

        {
            sIsIntegrate = ID_FALSE;

            // PROJ-1446 Host variable�� ������ ���� ����ȭ
            // ȣ��Ʈ ������ �����ϰ� ���� selectivity�� ���ؾ� �ϴ� ���
            // aPredicate�� flag�� ������ ǥ���صд�.
            // �� ������ execution time�� total selectivity�� �ٽ� ���� ��
            // ���� selectivity�� ���� �� �ִ����� ���θ�
            // ���� �˾ƿ��� ���ؼ��̴�.

            if( ( aPredicate->flag & QMO_PRED_HEAD_HOST_OPTIMIZE_MASK )
                == QMO_PRED_HEAD_HOST_OPTIMIZE_TRUE )
            {
                aPredicate->flag &= ~QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK;
                aPredicate->flag |= QMO_PRED_HEAD_HOST_OPT_TOTAL_TRUE;
            }
            else
            {
                aPredicate->flag &= ~QMO_PRED_HEAD_HOST_OPT_TOTAL_MASK;
                aPredicate->flag |= QMO_PRED_HEAD_HOST_OPT_TOTAL_FALSE;
            }
        }
        else
        {
            sColumnType = ( sColumnColumn->module->flag & MTD_GROUP_MASK );

            // minValue, maxValue ȹ��

            IDE_TEST( qtc::calculate( sMinValueNode, aTemplate )
                      != IDE_SUCCESS );
            sMinValue = aTemplate->tmplate.stack->value;
            sMinValueColumn = aTemplate->tmplate.stack->column;
            sMinValueType = ( sMinValueColumn->module->flag & MTD_GROUP_MASK );

            IDE_TEST( qtc::calculate( sMaxValueNode, aTemplate )
                      != IDE_SUCCESS );
            sMaxValue = aTemplate->tmplate.stack->value;
            sMaxValueColumn = aTemplate->tmplate.stack->column;
            sMaxValueType = ( sMaxValueColumn->module->flag & MTD_GROUP_MASK );

            if( ( sColumnType != MTD_GROUP_MISC ) &&
                ( sColumnType == sMaxValueType ) &&
                ( sColumnType == sMinValueType ) )
            {
                sIsIntegrate = ( aStatInfo->isValidStat == ID_TRUE ) ?
                               ID_TRUE: ID_FALSE;
            }
            else
            {
                sIsIntegrate = ID_FALSE;
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------
    // 2. Integrate selectivity ���
    //--------------------------------------

    if( sIsIntegrate == ID_TRUE )
    {
        IDE_DASSERT( aStatInfo->isValidStat == ID_TRUE );

        sColumnNDV = sColCardInfo[sColumnNode->node.column].columnNDV;
        sColumnMaxValue =
            (void *)(sColCardInfo[sColumnNode->node.column].maxValue);
        sColumnMinValue =
            (void *)(sColCardInfo[sColumnNode->node.column].minValue);

        //--------------------------------------
        // ���� �÷��� ���� ���� selectivity�� ����Ѵ�.
        // (1) �÷��� date type�� ���,
        //     : �ش� �÷��� mtdModule ���
        // (2) �÷��� �������迭��  ���,
        //     : value�� ���� ��� �ٸ� data type�� �� �����Ƿ�,
        //       double type���� ��ȯ�ϰ�,
        //       �� ��ȯ�� ������ mtdDouble.module�� ����ؼ� selectivity�� ����.
        //--------------------------------------

        if ( sColumnType == MTD_GROUP_NUMBER )
        {
            // PROJ-1364
            // ������ �迭�� ���
            // ���ϰ迭�� �ε��� ������� ����
            // �� �񱳴���� data type�� Ʋ�� �� �����Ƿ�
            // double�� ��ȯ�ؼ�, selectivity�� ���Ѵ�.
            // ��) smallint_col > 3 and smallint_col < numeric'9'
            // integrateSelectivity()

            IDE_TEST ( getConvertToDoubleValue( sColumnColumn,
                                                sMaxValueColumn,
                                                sMinValueColumn,
                                                sColumnMaxValue,
                                                sColumnMinValue,
                                                sMaxValue,
                                                sMinValue,
                                                &sDoubleColumnMaxValue,
                                                &sDoubleColumnMinValue,
                                                &sDoubleMaxValue,
                                                &sDoubleMinValue )
                       != IDE_SUCCESS );

            sSelectivity = mtdDouble.selectivity( (void *)&sDoubleColumnMaxValue,
                                                  (void *)&sDoubleColumnMinValue,
                                                  (void *)&sDoubleMaxValue,
                                                  (void *)&sDoubleMinValue,
                                                  sBoundForm / sColumnNDV,
                                                  aStatInfo->totalRecordCnt );
        }
        else
        {
            // PROJ-1484
            // �������� �ƴ� ��� (���ڿ� ���õ� ���� �߰�)
            // ex) DATE, CHAR(n), VARCHAR(n)

            sSelectivity = sColumnColumn->module->selectivity(
                sColumnMaxValue,
                sColumnMinValue,
                sMaxValue,
                sMinValue,
                sBoundForm / sColumnNDV,
                aStatInfo->totalRecordCnt );
        }

    }
    else
    {
        sMorePredicate = sPredicate;
        while( sMorePredicate != NULL )
        {
            sSelectivity *= getMySelectivity( aTemplate,
                                              sMorePredicate,
                                              aInExecutionTime );

            sMorePredicate = sMorePredicate->more;
        }
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "sSelectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getUnitSelectivity( qcTemplate    * aTemplate,
                                    qmoStatistics * aStatInfo,
                                    qcDepInfo     * aDepInfo,
                                    qmoPredicate  * aPredicate,
                                    qtcNode       * aCompareNode,
                                    SDouble       * aSelectivity,
                                    idBool          aInExecutionTime )
{
/******************************************************************************
 *
 * Description : JOIN �� ������ unit predicate �� unit selectivity ��ȯ
 *
 * Implementation :
 *
 *     1. =, != (<>) : getEqualSelectivity ����
 *     2. IS NULL, IS NOT NULL : getIsNullSelectivity ����
 *     3. >, >=, <, <=, BETWEEN, NOT BETWEEN :
 *        getGreaterLessBeetweenSelectivity ����
 *     4. LIKE, NOT LIKE : getLikeSelectivity ����
 *     5. IN, NOT IN : getInSelectivity ����
 *     6. EQUALS : getEqualsSelectivity ����
 *     7. LNNVL : getLnnvlSelectivity ����
 *     8. Etc : default selectivity
 *
 *****************************************************************************/

    SDouble     sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getUnitSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aDepInfo     != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    // BUG-41748
    // BUG-42283 host variable predicate �� ���� selectivity �� default selectivity �� ����Ѵ�.
    // (ex) WHERE i1 = :a AND i2 = :a OR :a is null
    //                                   ^^^^^^^^^^
    if ( ( ( aCompareNode->lflag & QTC_NODE_COLUMN_RID_MASK ) == QTC_NODE_COLUMN_RID_EXIST ) ||
         ( qtc::haveDependencies( &aCompareNode->depInfo ) == ID_FALSE ) )
    {
        sSelectivity = aCompareNode->node.module->selectivity;
    }
    else
    {
        //--------------------------------------
        // Unit selectivity ���
        //--------------------------------------

        if( ( aCompareNode->node.module == &mtfEqual ) ||
            ( aCompareNode->node.module == &mtfNotEqual ) )
        {
            //---------------------------------------
            // =, !=(<>)
            //---------------------------------------
            IDE_TEST( getEqualSelectivity( aTemplate,
                                           aStatInfo,
                                           aDepInfo,
                                           aPredicate,
                                           aCompareNode,
                                           & sSelectivity )
                      != IDE_SUCCESS );
        }
        else if( ( aCompareNode->node.module == &mtfIsNull ) ||
                 ( aCompareNode->node.module == &mtfIsNotNull ) )
        {
            //---------------------------------------
            // IS NULL, IS NOT NULL
            //---------------------------------------
            IDE_TEST( getIsNullSelectivity( aTemplate,
                                            aStatInfo,
                                            aPredicate,
                                            aCompareNode,
                                            & sSelectivity )
                      != IDE_SUCCESS );
        }
        else if( ( aCompareNode->node.module == &mtfGreaterThan ) ||
                 ( aCompareNode->node.module == &mtfGreaterEqual ) ||
                 ( aCompareNode->node.module == &mtfLessThan ) ||
                 ( aCompareNode->node.module == &mtfLessEqual ) ||
                 ( aCompareNode->node.module == &mtfBetween ) ||
                 ( aCompareNode->node.module == &mtfNotBetween ) )
        {
            //---------------------------------------
            // >, <, >=, <=, BETWEEN, NOT BETWEEN
            //---------------------------------------
            IDE_TEST( getGreaterLessBeetweenSelectivity( aTemplate,
                                                         aStatInfo,
                                                         aPredicate,
                                                         aCompareNode,
                                                         & sSelectivity,
                                                         aInExecutionTime )
                      != IDE_SUCCESS );
        }
        else if( ( aCompareNode->node.module == &mtfLike ) ||
                 ( aCompareNode->node.module == &mtfNotLike ) )
        {
            //---------------------------------------
            // LIKE, NOT LIKE
            //---------------------------------------
            IDE_TEST( getLikeSelectivity( aTemplate,
                                          aStatInfo,
                                          aPredicate,
                                          aCompareNode,
                                          & sSelectivity,
                                          aInExecutionTime )
                      != IDE_SUCCESS );

        }
        else if( ( aCompareNode->node.module == &mtfEqualAny ) ||
                 ( aCompareNode->node.module == &mtfNotEqualAll ) )
        {
            //---------------------------------------
            // IN(=ANY), NOT IN(!=ALL)
            //---------------------------------------
            IDE_TEST( getInSelectivity( aTemplate,
                                        aStatInfo,
                                        aDepInfo,
                                        aPredicate,
                                        aCompareNode,
                                        & sSelectivity )
                      != IDE_SUCCESS );
        }
        else if( ( aCompareNode->node.module == &stfEquals ) ||
                 ( aCompareNode->node.module == &stfNotEquals ) )
        {
            //---------------------------------------
            // EQUALS (Spatial relational operator)
            //---------------------------------------
            IDE_TEST( getEqualsSelectivity( aStatInfo,
                                            aPredicate,
                                            aCompareNode,
                                            & sSelectivity )
                      != IDE_SUCCESS );
        }
        else if( aCompareNode->node.module == &mtfLnnvl )
        {
            //---------------------------------------
            // LNNVL (logical not null value operator)
            //---------------------------------------
            IDE_TEST( getLnnvlSelectivity( aTemplate,
                                           aStatInfo,
                                           aDepInfo,
                                           aPredicate,
                                           aCompareNode,
                                           & sSelectivity,
                                           aInExecutionTime )
                      != IDE_SUCCESS );
        }
        else
        {
            //---------------------------------------
            // {>,>=,<,<=}ANY, {>,>=,<,<=}ALL �� ���
            // qtc::hostConstantWrapperModule �� ��� ( BUG-39036 )
            // default selectivity �� ���
            //---------------------------------------
            sSelectivity = aCompareNode->node.module->selectivity;
        }
    }

    *aSelectivity = sSelectivity;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getEqualSelectivity( qcTemplate    * aTemplate,
                                     qmoStatistics * aStatInfo,
                                     qcDepInfo     * aDepInfo,
                                     qmoPredicate  * aPredicate,
                                     qtcNode       * aCompareNode,
                                     SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : =, !=(<>) �� ���� unit selectivity ��ȯ (one table)
 *
 * Implementation :
 *
 *   1. Indexable predicate
 *   => !=, one side column LIST �� non-indexable ������
 *      selectivity ȹ���� �����ϹǷ� indexable �� selectivity�� ���
 *      ex) (i1, i2) != (1,1)
 *   1.1. ������� ����
 *      - column LIST : S = PRODUCT(1/NDVn)
 *        ex) (t1.i1,t1.i2)=(select t2.i1, t2.i2 from t2)
 *            (i1,i2)=(1,2), (i1,i2)=((1,2)), (i1,i2)=(1,:a), (i1,i2)=(:a,:a)
 *      - one column : S = 1/NDV
 *        ex) t1.i1=(select t2.i1 from t2), i1=1, i1=(1), i1=:a
 *   1.2. ������� �̼���
 *      - column LIST : S = PRODUCT(DSn)
 *      - one column : S = DS
 *
 *   2. Non-indexable predicate
 *   2.1. column LIST : S = PRODUCT(DSn)
 *        ex) (i1,1)=(1,1), (i1,i2)=(1,'1'), (i1,1)=(1,:a), (i1,1)=(1,i2)
 *            (t1.i1,1)=(select i1, i2 from t2)
 *   2.2. Etc (���� ��� LIST ���°� �ƴ�)
 *     => ������� ���� (1/NDV), ������� �̼��� (DS)
 *      - OneColumn and OneColumn : S = MIN( 1/leftNDV, 1/rightNDV )
 *        ex) i1=i2, i1=i1
 *      - OneColumn and Etc : S = MIN( 1/columnNDV , DS )
 *        ex) i1=i2+1, i1=i2+:a, i1=(J/JA type SUBQ)
 *      - Etc and Etc : S = DS
 *        ex) i1+1=i2+1, i1+i2=1
 *
 *   3. !=(<>) ����
 *
 *****************************************************************************/

    qmoColCardInfo * sColCardInfo;
    qtcNode        * sColumnNode;
    qtcNode        * sColumn;
    qtcNode        * sLeftNode;
    qtcNode        * sRightNode;
    SDouble          sDefaultSelectivity;
    SDouble          sLeftSelectivity;
    SDouble          sRightSelectivity;
    SDouble          sSelectivity;
    idBool           sIsIndexable;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getEqualSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aDepInfo     != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfEqual ||
                 aCompareNode->node.module == &mtfNotEqual );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sSelectivity = 1;
    sDefaultSelectivity = mtfEqual.selectivity;
    sColCardInfo = aStatInfo->colCardInfo;

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // column ��� ȹ��
    if( aCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
    }
    else
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments->next);
    }

    // BUG-7814 : (i1, i2) != (1,2)
    // - != operator �� ����� column LIST ���´� non-indexable �� �з�
    // - PROJ-2242 : default selectivity ȸ�Ǹ� ���� indexable ó�� ó��

    if( ( aCompareNode->node.module == &mtfNotEqual ) &&
        ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_LIST )
    {
        IDE_TEST( qmoPred::isExistColumnOneSide( aTemplate->stmt,
                                                 aCompareNode,
                                                 aDepInfo,
                                                 & sIsIndexable )
                  != IDE_SUCCESS );
    }
    else
    {
        sIsIndexable = ( aPredicate->id == QMO_COLUMNID_NON_INDEXABLE ) ?
            ID_FALSE: ID_TRUE;
    }

    if( sIsIndexable == ID_TRUE )
    {
        // 1. Indexable predicate

        if( aStatInfo->isValidStat == ID_TRUE )
        {
            // 1.1. ������� ����

            if( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_LIST )
            {
                // - column LIST
                sColumn = (qtcNode *)(sColumnNode->node.arguments);
                while( sColumn != NULL )
                {
                    sSelectivity *=
                        1 / sColCardInfo[sColumn->node.column].columnNDV;
                    sColumn = (qtcNode *)(sColumn->node.next);
                }
            }
            else if ( QTC_TEMPLATE_IS_COLUMN( aTemplate, sColumnNode ) == ID_TRUE )
            {
                // - one column
                sSelectivity =
                    1 / sColCardInfo[sColumnNode->node.column].columnNDV;
            }
            else
            {
                // BUG-40074 ������� indexable �ϼ� �ִ�.
                // - const value
                sSelectivity = sDefaultSelectivity;
            }
        }
        else
        {
            // 1.2. ������� �̼���

            if( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                == MTC_NODE_OPERATOR_LIST )
            {
                // - column LIST
                sColumn = (qtcNode *)(sColumnNode->node.arguments);

                while( sColumn != NULL )
                {
                    sSelectivity *= sDefaultSelectivity;
                    sColumn = (qtcNode *)(sColumn->node.next);
                }
            }
            else
            {
                // - one column
                sSelectivity = sDefaultSelectivity;
            }
        }
    }
    else
    {
        // 2. Non-indexable predicate

        if( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_LIST )
        {
            // 2.1. Column LIST ����

            sColumn = (qtcNode *)(sColumnNode->node.arguments);

            while( sColumn != NULL )
            {
                sSelectivity *= sDefaultSelectivity;
                sColumn = (qtcNode *)(sColumn->node.next);
            }
        }
        else
        {
            // 2.2. ���� ��� LIST ���°� �ƴ�

            // - OneColumn and OneColumn : S = MIN( 1/leftNDV, 1/rightNDV )
            // - OneColumn and Etc : S = MIN( 1/columnNDV , DS )
            // - Etc and Etc : S = DS

            sLeftNode  = (qtcNode *)(aCompareNode->node.arguments);
            sRightNode = (qtcNode *)(aCompareNode->node.arguments->next);


            // PR-20753 :
            // one column �̰� outer column�� �ƴѰ�쿡
            // �� operand�� ���� columnNDV �� ���Ѵ�.

            if( ( QTC_TEMPLATE_IS_COLUMN( aTemplate, sLeftNode ) == ID_TRUE ) &&
                ( qtc::dependencyContains( aDepInfo,
                                           &sLeftNode->depInfo) == ID_TRUE ) &&
                ( aStatInfo->isValidStat == ID_TRUE ) )
            {
                sLeftSelectivity =
                    1 / sColCardInfo[sLeftNode->node.column].columnNDV;
            }
            else
            {
                sLeftSelectivity = sDefaultSelectivity;
            }

            if( ( QTC_TEMPLATE_IS_COLUMN( aTemplate, sRightNode ) == ID_TRUE ) &&
                ( qtc::dependencyContains( aDepInfo,
                                           &sRightNode->depInfo) == ID_TRUE ) &&
                ( aStatInfo->isValidStat == ID_TRUE ) )
            {
                sRightSelectivity =
                    1 / sColCardInfo[sRightNode->node.column].columnNDV;
            }
            else
            {
                sRightSelectivity = sDefaultSelectivity;
            }

            sSelectivity = IDL_MIN( sLeftSelectivity, sRightSelectivity );
        }
    }

    /* BUG-47702 <> �����ڰ� ���� ��� Selectivity�� 1�� INDEX�� ���� �߸���  index ���� ����
     * NOT EQUAL �� Selectivity �� 1 �ΰ�� �� ��� ���� ���ΰ�� selectivity�� 1�� �ش�.
     */
    if ( aCompareNode->node.module == &mtfNotEqual )
    {
        if ( sSelectivity < 1 )
        {
            sSelectivity = 1 - sSelectivity;
        }
        else
        {
            sSelectivity = 1;
        }
    }
    else
    {
        /* Nothing to do */
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getIsNullSelectivity( qcTemplate    * aTemplate,
                                      qmoStatistics * aStatInfo,
                                      qmoPredicate  * aPredicate,
                                      qtcNode       * aCompareNode,
                                      SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : IS NULL, IS NOT NULL �� ���� unit selectivity ��ȯ
 *
 * Implementation : ��������� ���� ��� operator �� DS(default selectivity),
 *                  ��������� ���� ��� �Ʒ� ���� ������.
 *
 *     1. ������� �̼��� �Ǵ� Non-indexable predicate : S = DS
 *        ex) i1+1 IS NULL, i1+i2 IS NULL
 *     2. ������� �����̰� Indexable predicate
 *        ex) i1 IS NULL
 *     2.1. NULL value count �� 0 �� �ƴϸ� : S = NULL valueCnt / totalRecordCnt
 *     2.2. NULL value count �� 0 �̸�
 *        - NOT NULL constraint �̸� : S = 0
 *        - NOT NULL constraint �ƴϸ� : S = 1 / columnNDV
 *
 *****************************************************************************/

    qtcNode     * sColumnNode;
    mtcColumn   * sColumnColumn;
    SDouble       sTotalRecordCnt;
    SDouble       sNullValueCnt;
    SDouble       sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getIsNullSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfIsNull ||
                 aCompareNode->node.module == &mtfIsNotNull );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    if( ( aStatInfo->isValidStat == ID_FALSE ) ||
        ( aPredicate->id == QMO_COLUMNID_NON_INDEXABLE ) )
    {
        // 1. ������� �̼��� �Ǵ� Non-indexable predicate
        sSelectivity = aCompareNode->node.module->selectivity;
    }
    else
    {
        // 2. ������� �����̰� Indexable predicate
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
        sNullValueCnt =
            aStatInfo->colCardInfo[sColumnNode->node.column].nullValueCount;
        sTotalRecordCnt = ( aStatInfo->totalRecordCnt < sNullValueCnt ) ?
                            sNullValueCnt : aStatInfo->totalRecordCnt;

        sColumnColumn = &( aTemplate->tmplate.
                           rows[sColumnNode->node.table].
                           columns[sColumnNode->node.column] );

        sSelectivity = ( sNullValueCnt > 0 ) ?
                       ( sNullValueCnt / sTotalRecordCnt ): // NULL value ����
                       ( ( sColumnColumn->flag & MTC_COLUMN_NOTNULL_MASK )
                         == MTC_COLUMN_NOTNULL_TRUE ) ?     // NULL value ������
                       0:                       // NOT NULL constraints �� ���
                       ( 1 / sTotalRecordCnt ); // NOT NULL constraints �ƴ� ���

        // IS NOT NULL selectivity
        sSelectivity = ( aCompareNode->node.module == &mtfIsNotNull ) ?
                       ( 1 - sSelectivity ):
                       sSelectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}

IDE_RC
qmoSelectivity::getGreaterLessBeetweenSelectivity(
        qcTemplate    * aTemplate,
        qmoStatistics * aStatInfo,
        qmoPredicate  * aPredicate,
        qtcNode       * aCompareNode,
        SDouble       * aSelectivity,
        idBool          aInExecutionTime )
{
/******************************************************************************
 *
 * Description : >, >=, <, <=, BETWEEN, NOT BETWEEN �� ���� unit selectivity ���
 *               QMO_PRED_HOST_OPTIMIZE_TRUE ����
 *               - Indexable predicate �̰� (one column)
 *               - Variable value (prepare time) �̰�
 *               - QMO_PRED_IS_DYNAMIC_OPTIMIZABLE �̰�
 *               - MTD_SELECTIVITY_ENABLE column
 *
 *           cf) Quantifier operator ���� operand �� LIST ���°� �� �� ����.
 *
 * Implementation : �Ʒ� 1~2 �� ���� default selectivity ����
 *                  �Ʒ� 3 �� ���� Min-Max selectivity ����
 *
 *   1. Indexable predicate �̰� (one column)
 *      MTD_SELECTIVITY_ENABLE column �̰�
 *      Fixed value �Ǵ� variable value(execution time) �� �������
 *   1.1. Value (�ܺ� �÷� ����)
 *        ex) from t1 where exists (select i1 from t2 where i1 <= t1.i1)
 *                                                              |_ �� �κ�
 *   1.2. Column (QMO_STAT_MINMAX_COLUMN_SET_FALSE)
 *        ex) FROM ( SELECT ( SELECT T2.I1 FROM T2 LIMIT 1 )A FROM T1 )V1
 *            WHERE V1.A > 1;
 *   1.3. Column (MTD_GROUP_MISC)
 *        ex) columnType �� TEXT, NUMBER, DATE, INTERVAL group �� ������ ����
 *   1.4. Column and Value (columnType != valueType)
 *   1.5. ������� �̼���
 *
 *   2. Non-indexable predicate �Ǵ�
 *      MTD_SELECTIVITY_DISABLE column �Ǵ�
 *      Variable value (prepare time) 
 *      ex) 1 BETWEEN i1 AND 2, i1+i2 >= 1
 *          i1 BETWEEN 1 AND 2, i1 >= 1 (i1 �� ����Ұ� Ÿ��)
 *          i1 BETWEEN :a AND 2, i1 >= :a (prepare time)
 *
 *   3. Etc (�� ���� �̿�)
 *      ex) i1>1, i1<1, i1>=1, i1<=1, i1 BETWEEN 1 AND 2
 *
 *****************************************************************************/

    qmoColCardInfo * sColCardInfo;
    qtcNode        * sColumnNode;
    qtcNode        * sValueNode;
    qtcNode        * sValue;
    mtcColumn      * sColumnColumn;
    UInt             sColumnType;
    UInt             sValueType;
    SDouble          sSelectivity;
    idBool           sIsDefaultSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getGreaterLessBeetweenSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfGreaterThan ||
                 aCompareNode->node.module == &mtfGreaterEqual ||
                 aCompareNode->node.module == &mtfLessThan ||
                 aCompareNode->node.module == &mtfLessEqual ||
                 aCompareNode->node.module == &mtfBetween ||
                 aCompareNode->node.module == &mtfNotBetween );
    IDE_DASSERT( aSelectivity != NULL );

    //---------------------------------------
    // �⺻ �ʱ�ȭ
    //---------------------------------------

    sIsDefaultSelectivity = ID_FALSE;
    sColCardInfo = aStatInfo->colCardInfo;

    // column node�� value node ȹ��
    if( aCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments->next);
    }
    else
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments->next);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments);
    }

    sColumnColumn = &( aTemplate->tmplate.
                       rows[sColumnNode->node.table].
                       columns[sColumnNode->node.column] );

    sColumnType = ( sColumnColumn->module->flag & MTD_GROUP_MASK );

    //--------------------------------------
    // QMO_PRED_HOST_OPTIMIZE_TRUE ����
    //--------------------------------------

    // BUG-43065 �ܺ� ���� �÷��� �������� host ���� ����ȭ�� �ϸ� �ȵ�
    if( ( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE ) &&
        ( aInExecutionTime == ID_FALSE ) &&
        ( QMO_PRED_IS_VARIABLE( aPredicate ) == ID_TRUE ) &&
        ( QMO_PRED_IS_DYNAMIC_OPTIMIZABLE( aPredicate ) == ID_TRUE ) &&
        ( ( sColumnColumn->module->flag & MTD_SELECTIVITY_MASK )
          != MTD_SELECTIVITY_DISABLE ) &&
        ( qtc::haveDependencies( &sValueNode->depInfo ) == ID_FALSE ) )
    {
        // Indexable predicate �̰� (one column)
        // Variable value (prepare time) �̰�
        // QMO_PRED_IS_DYNAMIC_OPTIMIZABLE �̰�
        // MTD_SELECTIVITY_ENABLE column

        // qmoPredicate->flag�� host optimization flag�� �����Ѵ�.
        aPredicate->flag &= ~QMO_PRED_HOST_OPTIMIZE_MASK;
        aPredicate->flag |= QMO_PRED_HOST_OPTIMIZE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------
    // Default selectivity ���� ��� ����
    //--------------------------------------

    if( ( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE ) &&
        ( ( aInExecutionTime == ID_TRUE ) ||
          ( QMO_PRED_IS_VARIABLE( aPredicate ) == ID_FALSE ) ) &&
        ( ( sColumnColumn->module->flag & MTD_SELECTIVITY_MASK )
          != MTD_SELECTIVITY_DISABLE ) &&
        ( qtc::haveDependencies( &sValueNode->depInfo ) == ID_FALSE ) )
    {
        // 1. Indexable predicate �̰� (one column)
        //    MTD_SELECTIVITY_ENABLE column �̰�
        //    Fixed value �Ǵ� variable value(execution time)

        // BUG-32415 : BETWEEN/NOT BETWEEN�� ���� ���ڰ� 2���� ��� ���

        sValue = sValueNode;
        while( sValue != NULL )
        {
            // BUG-30401 : Selectivity �� ���� �� �ִ� �������� Ȯ���Ѵ�.
            // Dependency�� zero��� bind ���� �Ǵ� ����̹Ƿ�
            // selectivity�� ���� �� �ִ�.

            if( qtc::dependencyEqual( &sValue->depInfo,
                                      &qtc::zeroDependencies ) == ID_TRUE )
            {
                // sValueNode �� Bind ������ ����� ���
                // Nothing to do.
            }
            else
            {
                // 1.1. Value (�ܺ� �÷� ����)
                // ex) from t1 where exists 
                //     (select i1 from t2 where i1 between t1.i1 and 1)
                //                                        ^^^^^
                // ex) from t1 
                //     where exists (select i1 from t2 where i1 >= t1.i1)
                //                                                 ^^^^^
                // ex) hierarchical query �� WHERE ���� ���ΰ�� (view ����)
                //     from t1 where i1>1 connect by i1=i2
                //                   ^^

                sIsDefaultSelectivity = ID_TRUE;
                break;
            }
            sValue = (qtcNode*)(sValue->node.next);
        }

        if( sIsDefaultSelectivity == ID_FALSE )
        {
            // BUG-16265
            // view�� target�� subquery�� ��� ���� column������ ������
            // subquery node�� module�� mtdNull module �� �޸���
            // MTD_SELECTIVITY_DISABLE �̹Ƿ� ��������� �������� �ʴ´�.
            // �̷� ��� colCardInfo�� flag�� ��������� �ִ��� �Ǵ��ϰ�,
            // ������ default�� ���ȴ�.

            if( ( sColCardInfo[sColumnNode->node.column].flag &
                  QMO_STAT_MINMAX_COLUMN_SET_MASK )
                == QMO_STAT_MINMAX_COLUMN_SET_FALSE )
            {
                // 1.2. Column (QMO_STAT_MINMAX_COLUMN_SET_FALSE)
                // ex) FROM ( SELECT ( SELECT T2.I1 FROM T2 LIMIT 1 )A FROM T1 )V1
                //     WHERE V1.A > 1;

                sIsDefaultSelectivity = ID_TRUE;
            }
            else
            {
                // PROJ-2242 : default selectivity ������ ȸ���ϱ� ����
                //             getMinMaxSelectivity ���� �ű�

                // BUG-22064
                // non-indexable�ϰ� bind�ϴ� ��� default selectivity�� ���

                if( sColumnType == MTD_GROUP_MISC )
                {
                    // 1.3. Column (MTD_GROUP_MISC)

                    sIsDefaultSelectivity = ID_TRUE;
                }
                else
                {
                    // 1.4. Column and Value (columnType != valueType)

                    // BUG-32415 : BETWEEN/NOT BETWEEN�� ���� ���ڰ� 2���� ��� ���
                    sValue = sValueNode;
                    while( sValue != NULL )
                    {
                        // BUG-38758
                        // deterministic function�� fixed predicate������
                        // optimize�ܰ迡�� ����� �� ����.
                        if ( ( (sValue->lflag & QTC_NODE_PROC_FUNCTION_MASK)
                               == QTC_NODE_PROC_FUNCTION_TRUE ) &&
                             ( (sValue->lflag & QTC_NODE_PROC_FUNC_DETERMINISTIC_MASK)
                               == QTC_NODE_PROC_FUNC_DETERMINISTIC_TRUE ) )
                        {
                            sIsDefaultSelectivity = ID_TRUE;
                            break;
                        }
                        else
                        {
                            IDE_TEST( qtc::calculate( sValue, aTemplate )
                                      != IDE_SUCCESS );
                        }

                        sValueType =
                            ( aTemplate->tmplate.stack->column->module->flag &
                              MTD_GROUP_MASK );

                        if( sColumnType != sValueType )
                        {
                            sIsDefaultSelectivity = ID_TRUE;
                            break;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                        sValue = (qtcNode *)(sValue->node.next);
                    }
                }
            }
        }
        else
        {
            // Nothing to do.
        }
    }
    else
    {
        // 2. Non-indexable predicate �Ǵ�
        //    MTD_SELECTIVITY_DISABLE column �Ǵ�
        //    Variable value
        //    - (1) predicate�� variable�� �з��Ǵ� ���,
        //    - (2) �ش� �÷��� min/max value�� ���� ���
        // => selectivity�� ����� �� ���� ���� default selectivity

        sIsDefaultSelectivity = ID_TRUE;
    }

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    if( ( aStatInfo->isValidStat == ID_TRUE ) &&
        ( sIsDefaultSelectivity == ID_FALSE ) )
    {
        // Selectivity�� ���� �� �ִ� ����,
        // MIN/MAX�� �̿��� selectivity�� ���Ѵ�.

        IDE_TEST( getMinMaxSelectivity( aTemplate,
                                        aStatInfo,
                                        aCompareNode,
                                        sColumnNode,
                                        sValueNode,
                                        & sSelectivity )
                  != IDE_SUCCESS );

    }
    else
    {
        sSelectivity = aCompareNode->node.module->selectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::getMinMaxSelectivity( qcTemplate    * aTemplate,
                                      qmoStatistics * aStatInfo,
                                      qtcNode       * aCompareNode,
                                      qtcNode       * aColumnNode,
                                      qtcNode       * aValueNode,
                                      SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : MIN, MAX �� �̿��� selectivity ��ȯ
 *               MTD_SELECTIVITY_ENABLE �̰� MTD_GROUP_MISC �� �ƴ�
 *               �Ʒ��� ���� Ÿ�Կ� ���� �� �Լ��� ����ȴ�.
 *             - MTD_GROUP_TEXT : mtdVarchar, mtdChar, mtdNVarchar, mtdNChar,
 *             - MTD_GROUP_DATE : mtdDate
 *             - MTD_GROUP_NUMBER : mtdInteger, mtdDouble, mtdBigint,
 *                                  mtdReal, mtdSmallint, mtdFloat
 *
 *     �ε�ȣ �������� selectivity ���
 *
 *     1. i1 > N
 *        . selectivity = ( MAX - N ) / ( MAX - MIN )
 *
 *     2. i1 < N
 *        . selectivity = ( N - MIN ) / ( MAX - MIN )
 *
 *     3. BETWEEN
 *        between�� ���� max/min value�� ã��, value�� ���ռ�����
 *        �˻��ϱ� ���ؼ���, �� ������ Ÿ�Ժ��� ����Ǿ�� �ϹǷ�
 *        ������ ���⵵�� �������� ������, min/max value�� ������ ����
 *        �����Ѵ�. [value�� ���� ���ռ� �˻�� mt���� selectivity
 *        ��� ���߿� detect�ǰ�, �� ���, default selectivity�� ��ȯ�Ѵ�.]
 *
 *        i1 between A(min value) and B(max value)
 *        . selectivity = ( max value - min value ) / ( MAX - MIN )
 *
 * Implementation :
 *
 *     1. column�� const value��带 ã�´�.
 *     2. column�� mtdModule->selectivity() �Լ��� �̿��Ѵ�.
 *        mt������ ��¥���� �������� ���ؼ� �� dataType�� �´�
 *        selectivity ��� �Լ�[mtdModule->selectivity]�� �����Ѵ�.
 *
 *****************************************************************************/

    qmoColCardInfo  * sColCardInfo;
    UInt              sColumnType = 0;
    mtcColumn       * sColumnColumn;
    mtcColumn       * sMinValueColumn;
    mtcColumn       * sMaxValueColumn;
    void            * sColumnMaxValue;
    void            * sColumnMinValue;
    void            * sMaxValue;
    void            * sMinValue;
    mtdDoubleType     sDoubleColumnMaxValue;
    mtdDoubleType     sDoubleColumnMinValue;
    mtdDoubleType     sDoubleMaxValue;
    mtdDoubleType     sDoubleMinValue;

    SDouble           sColumnNDV;
    SDouble           sSelectivity;
    SDouble           sBoundForm;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getMinMaxSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aColumnNode  != NULL );
    IDE_DASSERT( aValueNode   != NULL );
    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aStatInfo->isValidStat == ID_TRUE );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sColCardInfo    = aStatInfo->colCardInfo;
    sColumnNDV      = sColCardInfo[aColumnNode->node.column].columnNDV;
    sColumnMaxValue =
        (void *)(sColCardInfo[aColumnNode->node.column].maxValue);
    sColumnMinValue =
        (void *)(sColCardInfo[aColumnNode->node.column].minValue);

    sColumnColumn = &( aTemplate->tmplate.
                       rows[aColumnNode->node.table].
                       columns[aColumnNode->node.column] );
    sColumnType = ( sColumnColumn->module->flag & MTD_GROUP_MASK );

    switch( aCompareNode->node.module->lflag & MTC_NODE_OPERATOR_MASK )
    {
        case( MTC_NODE_OPERATOR_GREATER ) :
        case( MTC_NODE_OPERATOR_GREATER_EQUAL ) :
            // >, >=
            if( aCompareNode->indexArgument == 0 )
            {
                // ��: i1>1, i1>=1
                sMaxValue = sColumnMaxValue;
                sMaxValueColumn = sColumnColumn;

                IDE_TEST( qtc::calculate( aValueNode, aTemplate )
                          != IDE_SUCCESS );
                sMinValue = aTemplate->tmplate.stack->value;
                sMinValueColumn = aTemplate->tmplate.stack->column;
            }
            else
            {
                // ��: 1>i1, 1>=i1
                IDE_TEST( qtc::calculate( aValueNode, aTemplate )
                          != IDE_SUCCESS );
                sMaxValue = aTemplate->tmplate.stack->value;
                sMaxValueColumn = aTemplate->tmplate.stack->column;

                sMinValue = sColumnMinValue;
                sMinValueColumn = sColumnColumn;
            }

            break;
        case( MTC_NODE_OPERATOR_LESS ) :
        case( MTC_NODE_OPERATOR_LESS_EQUAL ) :
            // <, <=
            if( aCompareNode->indexArgument == 0 )
            {
                // ��: i1<1, i1<=1
                IDE_TEST( qtc::calculate( aValueNode, aTemplate )
                          != IDE_SUCCESS );
                sMaxValue = aTemplate->tmplate.stack->value;
                sMaxValueColumn = aTemplate->tmplate.stack->column;

                sMinValue = sColumnMinValue;
                sMinValueColumn = sColumnColumn;
            }
            else
            {
                // ��: 1<i1, 1<=i1
                sMaxValue = sColumnMaxValue;
                sMaxValueColumn = sColumnColumn;

                IDE_TEST( qtc::calculate( aValueNode, aTemplate )
                          != IDE_SUCCESS );
                sMinValue = aTemplate->tmplate.stack->value;
                sMinValueColumn = aTemplate->tmplate.stack->column;
            }
            break;
        case( MTC_NODE_OPERATOR_RANGED ) :
        case( MTC_NODE_OPERATOR_NOT_RANGED ) :
            // BETWEEN, NOT BETWEEN
            // ��: between A(min value) AND B(max value)
            IDE_TEST( qtc::calculate( aValueNode,
                                      aTemplate ) != IDE_SUCCESS );
            sMinValue = aTemplate->tmplate.stack->value;
            sMinValueColumn = aTemplate->tmplate.stack->column;

            IDE_TEST( qtc::calculate( (qtcNode *)(aValueNode->node.next),
                                      aTemplate )
                         != IDE_SUCCESS );
            sMaxValue = aTemplate->tmplate.stack->value;
            sMaxValueColumn = aTemplate->tmplate.stack->column;

            break;
        default :
            IDE_RAISE( ERR_INVALID_NODE_OPERATOR );
    }

    // �������� ȹ��
    sBoundForm = ( aCompareNode->node.module == &mtfLessEqual ||
                   aCompareNode->node.module == &mtfGreaterEqual ) ?
                 ( 1 / sColumnNDV ):
                 ( aCompareNode->node.module == &mtfBetween ||
                   aCompareNode->node.module == &mtfNotBetween ) ?
                 ( 2 / sColumnNDV ): 0;

    //--------------------------------------
    // MIN/MAX�� �̿��� selectivity ���
    //--------------------------------------

    // BUG-17791
    if( sColumnType == MTD_GROUP_NUMBER )
    {
        // PROJ-1364
        // ������ �迭�� ���
        // ���ϰ迭�� �ε��� ������� ����
        // �� �񱳴���� data type�� Ʋ�� �� �����Ƿ�
        // double�� ��ȯ�ؼ�, selectivity�� ���Ѵ�.
        // ��) smallint_col > 3.5

        IDE_TEST ( getConvertToDoubleValue( sColumnColumn,
                                            sMaxValueColumn,
                                            sMinValueColumn,
                                            sColumnMaxValue,
                                            sColumnMinValue,
                                            sMaxValue,
                                            sMinValue,
                                            & sDoubleColumnMaxValue,
                                            & sDoubleColumnMinValue,
                                            & sDoubleMaxValue,
                                            & sDoubleMinValue )
                   != IDE_SUCCESS );

        sSelectivity = mtdDouble.selectivity( (void *)&sDoubleColumnMaxValue,
                                              (void *)&sDoubleColumnMinValue,
                                              (void *)&sDoubleMaxValue,
                                              (void *)&sDoubleMinValue,
                                              sBoundForm,
                                              aStatInfo->totalRecordCnt );
    }
    else
    {
        // PROJ-1484
        // �������� �ƴ� ��� (���ڿ� ���õ� ���� �߰�)
        // ex) DATE, CHAR(n), VARCHAR(n)

        sSelectivity = sColumnColumn->module->selectivity( sColumnMaxValue,
                                                           sColumnMinValue,
                                                           sMaxValue,
                                                           sMinValue,
                                                           sBoundForm,
                                                           aStatInfo->totalRecordCnt );
    }


    // NOT BETWEEN selectivity
    sSelectivity = ( aCompareNode->node.module == &mtfNotBetween ) ?
                   ( 1 - sSelectivity ): sSelectivity;

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_NODE_OPERATOR );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmoSelectivity::getMinMaxSelectivity",
                                  "Invalid node operator" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getConvertToDoubleValue( mtcColumn     * aColumnColumn,
                                         mtcColumn     * aMaxValueColumn,
                                         mtcColumn     * aMinValueColumn,
                                         void          * aColumnMaxValue,
                                         void          * aColumnMinValue,
                                         void          * aMaxValue,
                                         void          * aMinValue,
                                         mtdDoubleType * aDoubleColumnMaxValue,
                                         mtdDoubleType * aDoubleColumnMinValue,
                                         mtdDoubleType * aDoubleMaxValue,
                                         mtdDoubleType * aDoubleMinValue )
{
/***********************************************************************
 *
 * Description : NUMBER group �� double �� ��ȯ�� ��ȯ
 *
 * Implementation : double type������ conversion ����
 *
 ***********************************************************************/

    mtdValueInfo  sColumnMaxInfo;
    mtdValueInfo  sColumnMinInfo;
    mtdValueInfo  sValueMaxInfo;
    mtdValueInfo  sValueMinInfo;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getConvertToDoubleValue::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aColumnColumn         != NULL );
    IDE_DASSERT( aMaxValueColumn       != NULL );
    IDE_DASSERT( aMinValueColumn       != NULL );
    IDE_DASSERT( aColumnMaxValue       != NULL );
    IDE_DASSERT( aColumnMinValue       != NULL );
    IDE_DASSERT( aMaxValue             != NULL );
    IDE_DASSERT( aMinValue             != NULL );
    IDE_DASSERT( aDoubleColumnMaxValue != NULL );
    IDE_DASSERT( aDoubleColumnMinValue != NULL );
    IDE_DASSERT( aDoubleMaxValue       != NULL );
    IDE_DASSERT( aDoubleMinValue       != NULL );

    //--------------------------------------
    // Double �� ��ȯ
    //--------------------------------------

    // PROJ-1364
    // ������ �迭�� ���
    // ���ϰ迭�� �ε��� ������� ����
    // �� �񱳴���� data type�� Ʋ�� �� �����Ƿ�
    // double�� ��ȯ�ؼ�, selectivity�� ���Ѵ�.
    // ��) smallint_col < numeric'9'
    // ��) smallint_col > 3 and smallint_col < numeric'9'

    sColumnMaxInfo.column = aColumnColumn;
    sColumnMaxInfo.value  = aColumnMaxValue;
    sColumnMaxInfo.flag   = MTD_OFFSET_USELESS;

    mtd::convertToDoubleType4MtdValue( &sColumnMaxInfo,
                                       aDoubleColumnMaxValue );

    sColumnMinInfo.column = aColumnColumn;
    sColumnMinInfo.value  = aColumnMinValue;
    sColumnMinInfo.flag   = MTD_OFFSET_USELESS;

    mtd::convertToDoubleType4MtdValue( &sColumnMinInfo,
                                       aDoubleColumnMinValue );

    sValueMaxInfo.column = aMaxValueColumn;
    sValueMaxInfo.value  = aMaxValue;
    sValueMaxInfo.flag   = MTD_OFFSET_USELESS;

    mtd::convertToDoubleType4MtdValue( &sValueMaxInfo,
                                       aDoubleMaxValue );

    sValueMinInfo.column = aMinValueColumn;
    sValueMinInfo.value  = aMinValue;
    sValueMinInfo.flag   = MTD_OFFSET_USELESS;

    mtd::convertToDoubleType4MtdValue( &sValueMinInfo,
                                       aDoubleMinValue );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::getLikeSelectivity( qcTemplate    * aTemplate,
                                    qmoStatistics * aStatInfo,
                                    qmoPredicate  * aPredicate,
                                    qtcNode       * aCompareNode,
                                    SDouble       * aSelectivity,
                                    idBool          aInExecutionTime )
{
/******************************************************************************
 *
 * Description : LIKE �� ���� unit selectivity ����ϰ�
 *               QMO_PRED_HOST_OPTIMIZE_TRUE ����
 *               - QMO_PRED_IS_DYNAMIC_OPTIMIZABLE �̰�
 *               - Indexable predicate �̰� (one column)
 *               - Variable value (prepare time) �̰�
 *               - MTD_SELECTIVITY_ENABLE column
 *
 *   cf) NOT LIKE �� MTC_NODE_INDEX_UNUSABLE �� �׻� QMO_COLUMNID_NON_INDEXABLE
 *
 * Implementation :
 *
 *   1. ������� �����̰�
 *      Indexable predicate (one column and value) �̰�
 *      Fixed value �Ǵ� variable value (execution time) �̰�
 *      MTD_SELECTIVITY_ENABLE column
 *   1.1. Exact match : S = 1 / columnNDV
 *        ex) i1 LIKE 'ab'
 *   1.2. Prefix or Pattern match) : S = DS
 *        ex) Prefix match : i1 LIKE 'ab%', i1 LIKE 'ab%d', i1 LIKE 'ab%d%'
 *            Pattern match : i1 LIKE '%ab' (���� non-indexable �� ó��)
 *
 *   2. ������� �̼��� �Ǵ�
 *      Non-indexable predicate �Ǵ�
 *      Variable value (prepare time) �Ǵ�
 *      MTD_SELECTIVITY_DISABLE column : S = DS
 *      ex) t1.i1 LIKE t2.i2, i1 LIKE (select 'a' from t2),
 *          i1 LIKE '1%' (i1 �� ����Ұ� Ÿ��),
 *          i1 LIKE :a (prepare time)
 *
 *****************************************************************************/

    qmoColCardInfo * sColCardInfo;
    mtcColumn      * sColumnColumn;
    qtcNode        * sColumnNode;
    qtcNode        * sValueNode;
    SDouble          sColumnNDV;
    SDouble          sDefaultSelectivity;
    SDouble          sSelectivity;

    mtdCharType      sEscapeEmpty = { 0, { 0 } };
    mtdCharType    * sPattern;
    mtdCharType    * sEscape;
    UShort           sKeyLength;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getLikeSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfLike ||
                 aCompareNode->node.module == &mtfNotLike );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sColCardInfo = aStatInfo->colCardInfo;
    sDefaultSelectivity = aCompareNode->node.module->selectivity;

    // column node�� value node ȹ��
    if( aCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments->next);
    }
    else
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments->next);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments);
    }

    sColumnColumn = &( aTemplate->tmplate.
                       rows[sColumnNode->node.table].
                       columns[sColumnNode->node.column] );

    //--------------------------------------
    // QMO_PRED_HOST_OPTIMIZE_TRUE ����
    //--------------------------------------

    // BUG-43065 �ܺ� ���� �÷��� �������� host ���� ����ȭ�� �ϸ� �ȵ�
    if( ( QMO_PRED_IS_DYNAMIC_OPTIMIZABLE( aPredicate ) == ID_TRUE ) &&
        ( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE ) &&
        ( aInExecutionTime == ID_FALSE ) &&
        ( QMO_PRED_IS_VARIABLE( aPredicate ) == ID_TRUE ) &&
        ( ( sColumnColumn->module->flag & MTD_SELECTIVITY_MASK )
          != MTD_SELECTIVITY_DISABLE ) &&
        ( qtc::haveDependencies( &sValueNode->depInfo ) == ID_FALSE ) )
    {
        // qmoPredicate->flag�� host optimization flag�� �����Ѵ�.
        aPredicate->flag &= ~QMO_PRED_HOST_OPTIMIZE_MASK;
        aPredicate->flag |= QMO_PRED_HOST_OPTIMIZE_TRUE;
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    if( ( aStatInfo->isValidStat == ID_TRUE ) &&
        ( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE ) &&
        ( ( aInExecutionTime == ID_TRUE ) ||
          ( QMO_PRED_IS_VARIABLE( aPredicate ) == ID_FALSE ) ) &&
        ( ( sColumnColumn->module->flag & MTD_SELECTIVITY_MASK )
          != MTD_SELECTIVITY_DISABLE ) &&
        ( qtc::haveDependencies( &sValueNode->depInfo ) == ID_FALSE ) )
    {
        // NOT LIKE �� MTC_NODE_INDEX_UNUSABLE �� QMO_COLUMNID_NON_INDEXABLE
        IDE_DASSERT( aCompareNode->node.module == &mtfLike );

        // 1. ������� �����̰�
        //    Indexable predicate �̰�
        //    Fixed value �Ǵ� Variable value (execution time) �̰�
        //    MTD_SELECTIVITY_ENABLE column

        // LIKE predicate �� ���� ���ڿ� ȹ��
        IDE_TEST( qtc::calculate( sValueNode, aTemplate ) != IDE_SUCCESS );

        sPattern = (mtdCharType*)aTemplate->tmplate.stack->value;

        if( sValueNode->node.next != NULL )
        {
            IDE_TEST( qtc::calculate( (qtcNode *)(sValueNode->node.next),
                                      aTemplate )
                      != IDE_SUCCESS );
            sEscape = (mtdCharType*)aTemplate->tmplate.stack->value;
        }
        else
        {
            // escape ���ڰ� �������� �ʴ� ���
            sEscape = &sEscapeEmpty;
        }

        // Prefix key length ȹ��
        IDE_TEST( getLikeKeyLength( (const mtdCharType*) sPattern,
                                    (const mtdCharType*) sEscape,
                                    & sKeyLength )
                  != IDE_SUCCESS );

        // Prefix key length �� �̿��Ͽ� match type �Ǵ�
        if( sKeyLength != 0 && sKeyLength == sPattern->length )
        {
            // 1.1. Exact match
            sColumnNDV = sColCardInfo[sColumnNode->node.column].columnNDV;
            sSelectivity = 1 / sColumnNDV;
        }
        else
        {
            // 1.2. Pattern match(sKeyLength == 0) : ��ǻ� non-indexable
            //      Prefix match(sKeyLength != sPattern->length)

            sSelectivity = sDefaultSelectivity;
        }
    }
    else
    {
        // 2. ������� �̼��� �Ǵ�
        //    Non-indexable predicate �Ǵ�
        //    Variable value (prepare time) �Ǵ�
        //    MTD_SELECTIVITY_DISABLE column
        //    (���� ���� ���� ���õ��� ������ �� ���� data type)

        sSelectivity = sDefaultSelectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getLikeKeyLength( const mtdCharType   * aSource,
                                  const mtdCharType   * aEscape,
                                  UShort              * aKeyLength )
{
/******************************************************************************
 * Name: getLikeKeyLength() -- LIKE KEY�� prefix key�� ���̸� ����
 *
 * Arguments:
 * aSource: LIKE���� ��Ī ���ڿ�
 * aEscape: escape ����
 * aKeyLen: prefix key�� ���� (output)
 *
 * Description:
 * i1 LIKE 'K%' �Ǵ� i1 LIKE 'K_' ����
 * K�� ���̸� ��� ���� �Լ�
 *
 * 1) exact   match:  aKeyLen == aSource->length
 * 2) prefix  match:  aKeyLen <  aSource->length &&
 * aKeyLen != 0
 * 3) pattern match:  aKeyLen == 0
 *
 * �� �Լ��� ���õ��� �����ϱ� ���� LIKE���� ��Ī ������ �ľ��Ѵ�.
 * ���� ��Ƽ ����Ʈ ���ڼ�(��:�ѱ�)�� ����Ͽ� Ű�� ���̸� �ľ����� �ʴ´�.
 *
 *****************************************************************************/

    UChar       sEscape;     // escape ����
    idBool      sNullEscape;
    UShort      sIdx;        // loop counter

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getLikeKeyLength::__FT__" );

    IDE_DASSERT( aKeyLength != NULL );
    IDE_DASSERT( aSource    != NULL );
    IDE_DASSERT( aEscape    != NULL );

    // escape ���� ����
    if( aEscape->length < 1 )
    {
        // escape ���ڰ� �����Ǿ� ���� ���� ���
        sNullEscape = ID_TRUE;
    }
    else if( aEscape->length == 1 )
    {
        // escape ���� �Ҵ�
        sEscape = *(aEscape->value);
        sNullEscape = ID_FALSE;
    }
    else
    {
        // escape ������ ���̰� 1�� ������ �ȵȴ�.
        IDE_RAISE( ERR_INVALID_ESCAPE );
    }

    sIdx = 0;
    *aKeyLength = 0;
    while( sIdx < aSource->length )
    {
        if( (sNullEscape == ID_FALSE) && (aSource->value[sIdx] == sEscape) )
        {
            // To Fix PR-13004
            // ABR ������ ���Ͽ� ������Ų �� �˻��Ͽ��� ��
            sIdx++;

            // escape ������ ���,
            // escape ���� ���ڰ� '%','_' �������� �˻�
            IDE_TEST_RAISE( sIdx >= aSource->length, ERR_INVALID_LITERAL );

            // To Fix BUG-12578
            IDE_TEST_RAISE( aSource->value[sIdx] != (UShort)'%' &&
                            aSource->value[sIdx] != (UShort)'_' &&
                            aSource->value[sIdx] != sEscape,
                            ERR_INVALID_LITERAL );
        }
        else if( aSource->value[sIdx] == (UShort)'%' ||
                 aSource->value[sIdx] == (UShort)'_' )
        {
            // Ư�������� ���
            break;
        }
        else
        {
            // �Ϲ� ������ ���
        }
        sIdx++;
    }

    // get key length
    *aKeyLength = sIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_ESCAPE )
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_ESCAPE ));
    }
    IDE_EXCEPTION( ERR_INVALID_LITERAL )
    {
        IDE_SET(ideSetErrorCode( mtERR_ABORT_INVALID_LITERAL_AFTER_ESCAPE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getInSelectivity( qcTemplate    * aTemplate,
                                  qmoStatistics * aStatInfo,
                                  qcDepInfo     * aDepInfo,
                                  qmoPredicate  * aPredicate,
                                  qtcNode       * aCompareNode,
                                  SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : IN(=ANY), NOT IN(!=ALL) �� ���� unit selectivity �� ����ϰ�
 *               Indexable IN operator �� ����
 *               QMO_PRED_INSUBQUERY_MASK (subquery keyRange ����) ����
 *
 * Implementation : Selectivity ����
 *
 *     1. ������� �����̰� Indexable predicate �̰� LIST, Subquery value ����
 *        ex) i1 IN (1,2), (i1,i2) IN ((1,1)), (i1,i2) IN ((1,1),(2,2))
 *            i1 IN (select i1 from t2), (i1,i2) IN (select i1,i2 from t2)
 *     => NOT IN, one side column LIST �� non-indexable ������
 *        selectivity ȹ���� �����ϹǷ� indexable selectivity ���� ����
 *        ex) (i1, i2) NOT IN ((1,1))
 *      - ColumnLIST �� ���� AND Ȯ�� ���� (n : column number)
 *        S(AND) = 1 / columnNDV = 1 / PRODUCT( columnNDVn )
 *      - ValueLIST �� ���� OR Ȯ�� ���� (m : value number)
 *        S(OR) = 1 - PRODUCTm( 1 - S(AND) )
 *              = 1 - PRODUCTm( 1 - ( 1/columnNDVn ) )
 *
 *     2. ������� �̼��� �Ǵ� Non-indexable predicate
 *        S = PRODUCT( DSn )
 *        ex) i1 IN (i2,2), i1+1 IN (1,2), (i1,1) IN ((1,1), (2,1))
 *     => DS �� ����ϸ� value list �� ���ڼ��� �����Ҽ��� 1 �� ����
 *     => �Ʒ��� ���� ������ PRODUCT( DSn ) �� ä��
 *        Selectivity[i1 IN (1,2)] >
 *        Selectivity[(i1 IN (1,2)) AND (i2 IN (1,2))] >
 *        Selectivity[(i1, i2) IN ((1,1), (2,2))]
 * 
 *     3. NOT IN ����
 *
 *****************************************************************************/

    qmoColCardInfo * sColCardInfo;
    qtcNode        * sColumnNode;
    qtcNode        * sValueNode;
    qtcNode        * sColumn;
    qtcNode        * sValue;
    SDouble          sColumnNDV;
    SDouble          sDefaultSelectivity;
    SDouble          sSelectivity;
    idBool           sIsIndexable;
    ULong            sValueCount;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getInSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aDepInfo     != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfEqualAny ||
                 aCompareNode->node.module == &mtfNotEqualAll );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // �⺻ �ʱ�ȭ
    //--------------------------------------

    sColumnNDV = 1;
    sSelectivity = 1;
    sDefaultSelectivity = mtfEqualAny.selectivity;

    // column node�� value node ȹ��
    if( aCompareNode->indexArgument == 0 )
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments->next);
    }
    else
    {
        sColumnNode = (qtcNode *)(aCompareNode->node.arguments->next);
        sValueNode  = (qtcNode *)(aCompareNode->node.arguments);
    }

    //--------------------------------------
    // IN subquery keyRange ���� ����.
    //--------------------------------------
    // indexable predicate�� ���,
    // OR ��� ������ �񱳿����ڰ� ������ �ִ� ���,
    // subquery�� �������� �ʴ´�.

    if( ( aPredicate->id != QMO_COLUMNID_NON_INDEXABLE ) &&
        ( aCompareNode->node.module == &mtfEqualAny ) )
    {
        if( ( sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_SUBQUERY )
        {
            aPredicate->flag &= ~QMO_PRED_INSUBQUERY_MASK;
            aPredicate->flag |= QMO_PRED_INSUBQUERY_EXIST;
        }
        else
        {
            aPredicate->flag &= ~QMO_PRED_INSUBQUERY_MASK;
            aPredicate->flag |= QMO_PRED_INSUBQUERY_ABSENT;
        }
    }
    else
    {
        // Nothing to do.
    }

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // BUG-7814 : (i1, i2) NOT IN ((1,2))
    // - NOT IN operator �� ����� column LIST ���´� non-indexable �� �з�
    // - PROJ-2242 : default selectivity ȸ�Ǹ� ���� indexable ó�� ó��

    if( ( aCompareNode->node.module == &mtfNotEqualAll ) &&
        ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
        == MTC_NODE_OPERATOR_LIST )
    {
        IDE_TEST( qmoPred::isExistColumnOneSide( aTemplate->stmt,
                                                 aCompareNode,
                                                 aDepInfo,
                                                 & sIsIndexable )
                  != IDE_SUCCESS );
    }
    else
    {
        sIsIndexable = ( aPredicate->id == QMO_COLUMNID_NON_INDEXABLE ) ?
                       ID_FALSE: ID_TRUE;
    }

    if ( ( aStatInfo->isValidStat == ID_TRUE ) &&
         ( sIsIndexable == ID_TRUE ) )
    {
        if ( ( sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_LIST )
        {
            // 1. ������� ���� �̰�
            //    Indexable predicate (NOT IN one side column LIST ����)
            //    LIST value ����
            // ex) (i1,i2) IN ((1,1),(2,2)) -> (i1=1 AND i2=1) OR (i1=2 AND i2=2)
            // => Column LIST �� ���� AND Ȯ����, Value LIST �� ���� OR Ȯ����
 
            // columnNDV ȹ�� (AND Ȯ����)
            sColCardInfo = aStatInfo->colCardInfo;
 
            if ( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_LIST )
            {
                // (i1,i2) IN ((1,1),(2,2))
                sColumn = (qtcNode *)(sColumnNode->node.arguments);
 
                while ( sColumn != NULL )
                {
                    sColumnNDV *= sColCardInfo[sColumn->node.column].columnNDV;
                    sColumn = (qtcNode *)(sColumn->node.next);
                }
            }
            else
            {
                // i1 IN (1,2)
                sColumnNDV = sColCardInfo[sColumnNode->node.column].columnNDV;
            }
 
            // selectivity ȹ�� (OR Ȯ����)
            sValue = (qtcNode *)(sValueNode->node.arguments);
 
            while ( sValue != NULL )
            {
                sSelectivity *= ( 1 - ( 1 / sColumnNDV ) );
                sValue = (qtcNode *)(sValue->node.next);
            }
 
            sSelectivity = ( 1 - sSelectivity );
        }
        else if ( ( sValueNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_SUBQUERY )
        {
            // columnNDV ȹ�� (AND Ȯ����)
            sColCardInfo = aStatInfo->colCardInfo;
 
            if ( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
                 == MTC_NODE_OPERATOR_LIST )
            {
                // (i1,i2) IN (subquery)
                sColumn = (qtcNode *)(sColumnNode->node.arguments);
 
                while ( sColumn != NULL )
                {
                    sColumnNDV *= sColCardInfo[sColumn->node.column].columnNDV;
                    sColumn = (qtcNode *)(sColumn->node.next);
                }
            }
            else
            {
                // i1 IN (subquery)
                sColumnNDV = sColCardInfo[sColumnNode->node.column].columnNDV;
            }

            // selectivity ȹ�� (OR Ȯ����)
            sValueCount = DOUBLE_TO_UINT64( sValueNode->subquery->myPlan->graph->costInfo.outputRecordCnt );

            while ( sValueCount != 0 )
            {
                sSelectivity *= ( 1 - ( 1 / sColumnNDV ) );
                sValueCount--;
            }
 
            sSelectivity = ( 1 - sSelectivity );
        }
        else
        {
            IDE_DASSERT( 0 );
        } 
    }
    else
    {
        // 2. ������� �̼��� �Ǵ�
        //    Non-indexable predicate (NOT IN one side column LIST ����)
        // => Column LIST �� ���� AND Ȯ����

        if ( ( sColumnNode->node.lflag & MTC_NODE_OPERATOR_MASK )
            == MTC_NODE_OPERATOR_LIST )
        {
            sColumn = (qtcNode *)(sColumnNode->node.arguments);

            while ( sColumn != NULL )
            {
                sSelectivity *= sDefaultSelectivity;
                sColumn = (qtcNode *)(sColumn->node.next);
            }
        }
        else
        {
            sSelectivity = sDefaultSelectivity;
        }
    }

    // NOT IN (!=ALL) selectivity
    /* BUG-47702 <> �����ڰ� ���� ��� Selectivity�� 1�� INDEX�� ���� �߸���  index ���� ����
     * NOT EQUAL �� Selectivity �� 1 �ΰ�� �� ��� ���� ���ΰ�� selectivity�� 1�� �ش�.
     */
    if ( aCompareNode->node.module == &mtfNotEqualAll )
    {
        if ( sSelectivity < 1 )
        {
            sSelectivity = 1 - sSelectivity;
        }
        else
        {
            sSelectivity = 1;
        }
    }
    else
    {
        /* Nothing to do */
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::getEqualsSelectivity( qmoStatistics * aStatInfo,
                                      qmoPredicate  * aPredicate,
                                      qtcNode       * aCompareNode,
                                      SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : EQUALS, NOT EQUALS �� ���� unit selectivity ��ȯ
 *           cf) NOT EQUALS �� MTC_NODE_INDEX_UNUSABLE ��
 *               �׻� QMO_COLUMNID_NON_INDEXABLE
 *
 * Implementation :
 *
 *      1. ������� �̼��� �Ǵ� Non-indexable predicate : S = DS
 *         ex) where EQUALS(TB1.F2, TB1.F2)  --> (TB1.F2 : GEOMETRY type)
 *      2. ������� �����̰� Indexable predicate : S = 1 / columnNDV
 *         ex) where EQUALS(TB1.F2, GEOMETRY'POINT(3 3))
 *
 *****************************************************************************/

    qtcNode          * sColumnNode;
    qmoColCardInfo   * sColCardInfo;
    SDouble            sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getEqualsSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aPredicate != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aCompareNode->node.module == &stfEquals ||
                 aCompareNode->node.module == &stfNotEquals );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    if( ( aStatInfo->isValidStat == ID_FALSE ) ||
        ( aPredicate->id == QMO_COLUMNID_NON_INDEXABLE ) )
    {
        // 1. ������� �̼��� �Ǵ� Non-indexable predicate
        sSelectivity = aCompareNode->node.module->selectivity;
    }
    else
    {
        // 2. ������� �����̰� Indexable predicate
        IDE_DASSERT( aCompareNode->node.module == &stfEquals );

        // column node�� value node ȹ��
        if( aCompareNode->indexArgument == 0 )
        {
            sColumnNode = (qtcNode *)(aCompareNode->node.arguments);
        }
        else
        {
            sColumnNode = (qtcNode *)(aCompareNode->node.arguments->next);
        }

        sColCardInfo = aStatInfo->colCardInfo;
        sSelectivity = 1 / sColCardInfo[sColumnNode->node.column].columnNDV;

        // NOTEQUALS selectivity
        sSelectivity = ( aCompareNode->node.module == &stfNotEquals ) ?
                       1 - sSelectivity: sSelectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}

IDE_RC
qmoSelectivity::getLnnvlSelectivity( qcTemplate    * aTemplate,
                                     qmoStatistics * aStatInfo,
                                     qcDepInfo     * aDepInfo,
                                     qmoPredicate  * aPredicate,
                                     qtcNode       * aCompareNode,
                                     SDouble       * aSelectivity,
                                     idBool          aInExecutionTime )
{
/******************************************************************************
 *
 * Description : LNNVL �� unit selectivity ��ȯ (one table)
 *
 * Implementation :
 *
 *       LNNVL operator �� ���� ���¿� ���� ������ ���� �����Ѵ�.
 *
 *       1. OR, AND (not normal form) : S = DS
 *       2. ��ø�� LNNVL : S = 1 - S(LNNVL)
 *       3. Compare predicate (only one) : S = 1 - S(one predicate)
 *
 *****************************************************************************/

    qtcNode   * sNode;
    SDouble     sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getLnnvlSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aTemplate    != NULL );
    IDE_DASSERT( aStatInfo    != NULL );
    IDE_DASSERT( aDepInfo     != NULL );
    IDE_DASSERT( aPredicate   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSelectivity != NULL );
    IDE_DASSERT( aCompareNode->node.module == &mtfLnnvl );
    IDE_DASSERT( aCompareNode->node.arguments != NULL );

    sNode = (qtcNode *)aCompareNode->node.arguments;

    IDE_DASSERT( ( sNode->node.lflag & MTC_NODE_COMPARISON_MASK )
                 == MTC_NODE_COMPARISON_TRUE );

    if( sNode->node.module == &mtfLnnvl )
    {
        // ��ø�� LNNVL
        IDE_TEST( getLnnvlSelectivity( aTemplate,
                                       aStatInfo,
                                       aDepInfo,
                                       aPredicate,
                                       sNode,
                                       & sSelectivity,
                                       aInExecutionTime )
                  != IDE_SUCCESS );

        sSelectivity = 1 - sSelectivity;
    }
    else
    {
        // Compare predicate
        IDE_DASSERT( ( sNode->node.lflag & MTC_NODE_COMPARISON_MASK )
                     == MTC_NODE_COMPARISON_TRUE );

        IDE_TEST( getUnitSelectivity( aTemplate,
                                      aStatInfo,
                                      aDepInfo,
                                      aPredicate,
                                      sNode,
                                      & sSelectivity,
                                      aInExecutionTime )
                  != IDE_SUCCESS );

        sSelectivity = 1 - sSelectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

SDouble qmoSelectivity::getEnhanceSelectivity4Join( qcStatement  * aStatement,
                                                    qmgGraph     * aGraph,
                                                    qmoStatistics* aStatInfo,
                                                    qtcNode      * aNode )
{
/******************************************************************************
 *
 * Description : BUG-37918 join selectivity ����
 *
 * Implementation : �ܼ��ϰ� �׷����� output ���� NDV �� ū ��쿡��
 *                  output �� ��� ����ϵ��� �մϴ�.
 *
 *
 *****************************************************************************/

    SDouble          sSelectivity;
    qmgGraph       * sGraph;
    qcDepInfo        sNodeDepInfo;

    IDE_DASSERT( aStatInfo->isValidStat == ID_TRUE );
    IDE_DASSERT( QTC_IS_COLUMN( aStatement, aNode ) == ID_TRUE );

    if( aGraph != NULL )
    {
        qtc::dependencySet( aNode->node.table,
                            &sNodeDepInfo );

        if( qtc::dependencyContains( &aGraph->left->depInfo,
                                     &sNodeDepInfo ) == ID_TRUE )
        {
            sGraph = aGraph->left;
        }
        else if( qtc::dependencyContains( &aGraph->right->depInfo,
                                          &sNodeDepInfo ) == ID_TRUE )
        {
            sGraph = aGraph->right;
        }
        else
        {
            // �ܺ� ���� �÷��� ���
            sGraph = NULL;
        }

        if( sGraph != NULL )
        {
            sSelectivity = IDL_MIN( aStatInfo->colCardInfo[aNode->node.column].columnNDV,
                                    sGraph->costInfo.outputRecordCnt );

            sSelectivity = 1 / sSelectivity;
        }
        else
        {
            sSelectivity = 1.0;
        }
    }
    else
    {
        sSelectivity = 1 / aStatInfo->colCardInfo[aNode->node.column].columnNDV;
    }

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return sSelectivity;
}

IDE_RC
qmoSelectivity::getUnitSelectivity4Join( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qtcNode      * aCompareNode,
                                         idBool       * aIsEqual,
                                         SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmgJoin ���� qmoPredicate �� unit predicate �� ����
 *               unit selectivity �� ����ϰ�
 *               (=) operator �� ���� �����ε��� ��밡�ɿ��� ��ȯ
 *
 * Implementation :
 *
 *        1. =, != operator
 *        1.1. One column & one column : S = 1 / max( NDV(L), NDV(R) )
 *             ex) t1.i1=t2.i1
 *           - �� �� �÷� ������� ������ : S = min( 1/NDV(L), 1/NDV(R) )
 *           - �� �� �÷� ������� ������ : S = min( 1/NDV, DS )
 *           - �� �� �÷� ������� ������ : S = DS
 *        1.2. One column & Etc
 *             ex) t1.i1=t2.i1+1, 1=t1.i1 or t2.i1=1 ���� �� unit predicate
 *           - �÷� ������� ������ : S = min( 1/NDV, DS )
 *           - �÷� ������� ������ : S = DS
 *             ex) t1.i1=t2.i1
 *        1.3. Etc & Etc : S = DS
 *             ex) t1.i1+t2.i1=1
 *        cf) != operator : S = 1 - S(=)
 *
 *        2. Etc. : S = DS
 *           ex) t1.i1 > t2.i1
 *
 *****************************************************************************/

    qmoStatistics  * sLeftStatInfo;
    qmoStatistics  * sRightStatInfo;
    qtcNode        * sLeftNode;
    qtcNode        * sRightNode;
    SDouble          sLeftSelectivity;
    SDouble          sRightSelectivity;
    SDouble          sSelectivity;
    idBool           sIsEqual = ID_TRUE;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getUnitSelectivity4Join::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aIsEqual     != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    // BUG-41876
    if ( ( aCompareNode->lflag & QTC_NODE_COLUMN_RID_MASK ) == QTC_NODE_COLUMN_RID_EXIST )
    {
        sSelectivity = aCompareNode->node.module->selectivity;
    }
    else
    {
        if( ( aCompareNode->node.module == &mtfEqual ) ||
            ( aCompareNode->node.module == &mtfNotEqual ) )
        {
            // 1. =, <> operator �� ���� ������ ���� ���

            sSelectivity = mtfEqual.selectivity;

            sLeftNode = (qtcNode *)(aCompareNode->node.arguments);
            sRightNode = (qtcNode *)(aCompareNode->node.arguments->next);

            if( QTC_IS_COLUMN( aStatement, sLeftNode ) == ID_TRUE )
            {
                sLeftStatInfo =
                    QC_SHARED_TMPLATE(aStatement)->tableMap[sLeftNode->node.table].
                    from->tableRef->statInfo;

                if( sLeftStatInfo->isValidStat == ID_TRUE )
                {
                    sLeftSelectivity = getEnhanceSelectivity4Join( aStatement,
                                                                   aGraph,
                                                                   sLeftStatInfo,
                                                                   sLeftNode );
                }
                else
                {
                    sLeftSelectivity = sSelectivity;
                }
            }
            else
            {
                sLeftSelectivity = sSelectivity;
            }

            if( QTC_IS_COLUMN( aStatement, sRightNode ) == ID_TRUE )
            {
                sRightStatInfo =
                    QC_SHARED_TMPLATE(aStatement)->tableMap[sRightNode->node.table].
                    from->tableRef->statInfo;

                if( sRightStatInfo->isValidStat == ID_TRUE )
                {
                    sRightSelectivity = getEnhanceSelectivity4Join( aStatement,
                                                                    aGraph,
                                                                    sRightStatInfo,
                                                                    sRightNode );
                }
                else
                {
                    sRightSelectivity = sSelectivity;
                }
            }
            else
            {
                sRightSelectivity = sSelectivity;
            }

            sSelectivity = IDL_MIN( sLeftSelectivity, sRightSelectivity );

            // NOT EQUAL selectivity
            sSelectivity = ( aCompareNode->node.module == &mtfNotEqual ) ?
                           1 - sSelectivity: sSelectivity;
        }
        else
        {
            // 2. Etc operator

            sSelectivity = aCompareNode->node.module->selectivity;
            sIsEqual = ID_FALSE;
        }
    }

    *aSelectivity = sSelectivity;
    *aIsEqual = sIsEqual;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::getMySelectivity4PredList( qmoPredicate  * aPredList,
                                           SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : Join ���� graph �� qmoPredicate list �� ����
 *               ���յ� mySelecltivity �� ��ȯ�Ѵ�.
 *
 * Implementation : S = PRODUCT( mySelectivity for qmoPredicate list )
 *
 *****************************************************************************/

    qmoPredicate  * sPredicate;
    SDouble         sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getMySelectivity4PredList::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aSelectivity != NULL );
    
    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sSelectivity = 1;
    sPredicate = aPredList;

    while( sPredicate != NULL )
    {
        // BUG-37778 disk hash temp table size estimate
        // tpc-H Q20 ���� �ʹ� ���� join selectivity �� ����
        sSelectivity = IDL_MIN( sSelectivity, sPredicate->mySelectivity);
        // sSelectivity *= sPredicate->mySelectivity;

        sPredicate = sPredicate->next;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::getSemiJoinSelectivity( qcStatement   * aStatement,
                                        qmgGraph      * aGraph,
                                        qcDepInfo     * aLeftDepInfo,
                                        qmoPredicate  * aPredList,
                                        idBool          aIsLeftSemi,
                                        idBool          aIsSetNext,
                                        SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmoPredicate list �Ǵ� qmoPredicate �� ����
 *               semi join selectivity �� ��ȯ
 *
 * Implementation : 
 *
 *     1. aIsSetNext �� TRUE �� ��� qmoPredicate list ��ü�� ���� ����
 *        Selectivity = PRODUCT( MS semi join for qmoPredicate )
 *
 *     1.1. qmoPredicate �� ���� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2
 *          MS(mySelectivity) = 1-PRODUCT(1-US)n    (OR Ȯ������)
 *
 *     1.2. qmoPredicate �� �� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1
 *          MS(mySelectivity) = US (unit semi join selectivity)
 *
 *     2. aIsSetNext �� FALSE �� ��� qmoPredicate �ϳ��� ���� ����
 *        Selectivity = semi join MySelectivity for qmoPredicate
 *                    = US (unit semi join selectivity)
 *
 *****************************************************************************/

    qmoPredicate   * sPredicate;
    qtcNode        * sCompareNode;
    SDouble          sSelectivity;
    SDouble          sMySelectivity;
    SDouble          sUnitSelectivity;
    idBool           sIsDNF;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getSemiJoinSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aLeftDepInfo != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sSelectivity = 1;
    sPredicate   = aPredList;
    
    while( sPredicate != NULL )
    {
        sIsDNF = ID_FALSE;

        if( ( sPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
        }
        else
        {
            sCompareNode = sPredicate->node;
            sIsDNF = ID_TRUE;
        }

        //--------------------------------------
        // Semi join my selectivity ȹ��
        //--------------------------------------
        if( sCompareNode->node.next != NULL && sIsDNF == ID_FALSE )
        {
            // 1.1. OR ������ �񱳿����ڰ� ������ �ִ� ���,
            //      OR �������ڿ� ���� selectivity ���.
            //      1 - (1-a)(1-b).....
            // ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2

            sMySelectivity = 1;
            while( sCompareNode != NULL )
            {
                IDE_TEST( getUnitSelectivity4Semi( aStatement,
                                                   aGraph,
                                                   aLeftDepInfo,
                                                   sCompareNode,
                                                   aIsLeftSemi,
                                                   & sUnitSelectivity )
                          != IDE_SUCCESS );

                sMySelectivity *= ( 1 - sUnitSelectivity );

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }

            sMySelectivity = ( 1 - sMySelectivity );
        }
        else
        {
            // 1.2. OR ��� ������ �񱳿����ڰ� �ϳ��� ����
            // ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1

            IDE_TEST( getUnitSelectivity4Semi( aStatement,
                                               aGraph,
                                               aLeftDepInfo,
                                               sCompareNode,
                                               aIsLeftSemi,
                                               & sMySelectivity )
                      != IDE_SUCCESS );
        }

        sSelectivity *= sMySelectivity;

        IDE_DASSERT_MSG( sMySelectivity >= 0 && sMySelectivity <= 1,
                         "mySelectivity : %"ID_DOUBLE_G_FMT"\n",
                         sMySelectivity );

        if (aIsSetNext == ID_FALSE)
        {
            // 2. Predicate list �� �ƴ� �ϳ��� qmoPredicate �� ���ؼ��� ����
            break;
        }
        else
        {
            sPredicate = sPredicate->next;
        }
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getAntiJoinSelectivity( qcStatement   * aStatement,
                                        qmgGraph      * aGraph,
                                        qcDepInfo     * aLeftDepInfo,
                                        qmoPredicate  * aPredList,
                                        idBool          aIsLeftAnti,
                                        idBool          aIsSetNext,
                                        SDouble       * aSelectivity )
{
/******************************************************************************
 *
 * Description : qmoPredicate list �Ǵ� qmoPredicate �� ����
 *               anti join selectivity �� ��ȯ
 *
 * Implementation : 
 *
 *     1. aIsSetNext �� TRUE �� ��� qmoPredicate list ��ü�� ���� ����
 *        Selectivity = PRODUCT( MS anti join for qmoPredicate )
 *
 *     1.1. qmoPredicate �� ���� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2
 *          MS(mySelectivity) = 1-PRODUCT(1-US)n    (OR Ȯ������)
 *
 *     1.2. qmoPredicate �� �� ���� unit predicate �� �����Ǿ��� ���
 *          ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1
 *          MS(mySelectivity) = US (unit anti join selectivity)
 *
 *     2. aIsSetNext �� FALSE �� ��� qmoPredicate �ϳ��� ���� ����
 *        Selectivity = anti join MySelectivity for qmoPredicate
 *                    = US (unit anti join selectivity)
 *
 *****************************************************************************/

    qmoPredicate   * sPredicate;
    qtcNode        * sCompareNode;
    SDouble          sSelectivity;
    SDouble          sMySelectivity;
    SDouble          sUnitSelectivity;
    idBool           sIsDNF;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getAntiJoinSelectivity::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aLeftDepInfo != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    //--------------------------------------
    // Selectivity ���
    //--------------------------------------

    sSelectivity = 1;
    sPredicate   = aPredList;
    
    while( sPredicate != NULL )
    {
        sIsDNF = ID_FALSE;

        if( ( sPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
        }
        else
        {
            sCompareNode = sPredicate->node;
            sIsDNF = ID_TRUE;
        }

        //--------------------------------------
        // Anti join my selectivity ȹ��
        //--------------------------------------
        if( sCompareNode->node.next != NULL && sIsDNF == ID_FALSE )
        {
            // 1.1. OR ������ �񱳿����ڰ� ������ �ִ� ���,
            //      OR �������ڿ� ���� selectivity ���.
            //      1 - (1-a)(1-b).....
            // ex) t1.i1=t2.i1 or t1.i2=t2.i2, t1.i1>t2.i1 or t1.i2<t2.i2

            sMySelectivity = 1;
            while( sCompareNode != NULL )
            {
                IDE_TEST( getUnitSelectivity4Anti( aStatement,
                                                   aGraph,
                                                   aLeftDepInfo,
                                                   sCompareNode,
                                                   aIsLeftAnti,
                                                   & sUnitSelectivity )
                          != IDE_SUCCESS );

                sMySelectivity *= ( 1 - sUnitSelectivity );

                sCompareNode = (qtcNode *)(sCompareNode->node.next);
            }

            sMySelectivity = ( 1 - sMySelectivity );
        }
        else
        {
            // 1.2. OR ��� ������ �񱳿����ڰ� �ϳ��� ����
            // ex) t1.i1=t2.i1, t1.i1>t2.i1, t1.i2<t2.i2+1

            IDE_TEST( getUnitSelectivity4Anti( aStatement,
                                               aGraph,
                                               aLeftDepInfo,
                                               sCompareNode,
                                               aIsLeftAnti,
                                               & sMySelectivity )
                      != IDE_SUCCESS );
        }

        sSelectivity *= sMySelectivity;

        IDE_DASSERT_MSG( sMySelectivity >= 0 && sMySelectivity <= 1,
                         "mySelectivity : %"ID_DOUBLE_G_FMT"\n",
                         sMySelectivity );

        if (aIsSetNext == ID_FALSE)
        {
            // 2. Predicate list �� �ƴ� �ϳ��� qmoPredicate �� ���ؼ��� ����
            break;
        }
        else
        {
            sPredicate = sPredicate->next;
        }
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getUnitSelectivity4Anti( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qcDepInfo    * aLeftDepInfo,
                                         qtcNode      * aCompareNode,
                                         idBool         aIsLeftAnti,
                                         SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : Join ���� qmoPredicate �� ���� unit predicate �� ����
 *               unit anti join selectivity ��ȯ
 *
 * Implementation : ������ ������ ����.
 *
 *        1. One column = One column ����
 *           ex) t1.i1=t2.i1
 *        1.1. �� �� �÷� ������� ��� ������ : S = 1 - Semi join selectivity
 *        1.2. �� �� �÷� ������� �ϳ��� ������ : S = Defalut selectivity
 *
 *        2. Etc : S = Defalut selectivity
 *           ex) t1.i1 > t2.i1, t1.i1=t2.i1+1,
 *               1=t1.i1 or t2.i1=1 ���� �� unit predicate
 *
 *****************************************************************************/

    qtcNode        * sLeftNode;
    qtcNode        * sRightNode;
    qmoStatistics  * sLeftStatInfo;
    qmoStatistics  * sRightStatInfo;
    SDouble          sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getUnitSelectivity4Anti::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aLeftDepInfo != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    // BUG-41876
    IDE_DASSERT( ( aCompareNode->lflag & QTC_NODE_COLUMN_RID_MASK ) == QTC_NODE_COLUMN_RID_ABSENT );

    //--------------------------------------
    // Unit selectivity ���
    //--------------------------------------

    sLeftNode = (qtcNode *)(aCompareNode->node.arguments);
    sRightNode = (qtcNode *)(aCompareNode->node.arguments->next);

    if( ( aCompareNode->node.module == &mtfEqual ) &&
        ( QTC_IS_COLUMN( aStatement, sLeftNode ) == ID_TRUE ) &&
        ( QTC_IS_COLUMN( aStatement, sRightNode ) == ID_TRUE ) )
    {
        // 1. One column = One column ���� : t1.i1=t2.i1

        sLeftStatInfo =
            QC_SHARED_TMPLATE(aStatement)->tableMap[sLeftNode->node.table].
            from->tableRef->statInfo;

        sRightStatInfo =
            QC_SHARED_TMPLATE(aStatement)->tableMap[sRightNode->node.table].
            from->tableRef->statInfo;

        if( ( sLeftStatInfo->isValidStat == ID_TRUE ) &&
            ( sRightStatInfo->isValidStat == ID_TRUE ) )
        {
            // 1.1. �� �� �÷� ������� ��� ������
            IDE_TEST( getUnitSelectivity4Semi( aStatement,
                                               aGraph,
                                               aLeftDepInfo,
                                               aCompareNode,
                                               aIsLeftAnti,
                                               & sSelectivity )
                      != IDE_SUCCESS );

            sSelectivity = 1 - sSelectivity;
        }
        else
        {
            // 1.2. �� �� �÷� ������� �ϳ��� ������
            sSelectivity = aCompareNode->node.module->selectivity;
        }
    }
    else
    {
        // 2. Etc : t1.i1 > t2.i1, t1.i1=t2.i1+1,
        //          1=t1.i1 or t2.i1=1 ���� �� unit predicate
        sSelectivity = aCompareNode->node.module->selectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::getUnitSelectivity4Semi( qcStatement  * aStatement,
                                         qmgGraph     * aGraph,
                                         qcDepInfo    * aLeftDepInfo,
                                         qtcNode      * aCompareNode,
                                         idBool         aIsLeftSemi,
                                         SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : Join ���� qmoPredicate �� ���� unit predicate �� ����
 *               unit semi join selectivity ��ȯ
 *
 * Implementation :
 *
 *   1. One column = One column ����
 *      ex) t1.i1=t2.i1
 *   1.1. �� �� �÷� ������� ��� ������
 *      - Left semi join selectivity = Right NDV / MAX( Left NDV, Right NDV )
 *      - Right semi join selectivity = Left NDV / MAX( Left NDV, Right NDV )
 *   1.2. �� �� �÷� ������� �ϳ��� ������
 *        Semi join selectivity = Defalut selectivity
 *
 *   2. Etc : Semi join selectivity = Default selectivity
 *      ex) t1.i1 > t2.i1, t1.i1=t2.i1+1,
 *          1=t1.i1 or t2.i1=1 ���� �� unit predicate
 *
 *****************************************************************************/

    qtcNode        * sLeftNode;
    qtcNode        * sRightNode;
    qmoStatistics  * sLeftStatInfo;
    qmoStatistics  * sRightStatInfo;
    SDouble          sLeftSelectivity;
    SDouble          sRightSelectivity;
    SDouble          sMinSelectivity;
    SDouble          sSelectivity;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getUnitSelectivity4Semi::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement   != NULL );
    IDE_DASSERT( aLeftDepInfo != NULL );
    IDE_DASSERT( aCompareNode != NULL );
    IDE_DASSERT( aSelectivity != NULL );

    // BUG-41876
    IDE_DASSERT( ( aCompareNode->lflag & QTC_NODE_COLUMN_RID_MASK ) == QTC_NODE_COLUMN_RID_ABSENT );

    //--------------------------------------
    // Unit selectivity ���
    //--------------------------------------

    sLeftNode = (qtcNode *)(aCompareNode->node.arguments);
    sRightNode = (qtcNode *)(aCompareNode->node.arguments->next);

    if( ( aCompareNode->node.module == &mtfEqual ) &&
        ( QTC_IS_COLUMN( aStatement, sLeftNode ) == ID_TRUE ) &&
        ( QTC_IS_COLUMN( aStatement, sRightNode ) == ID_TRUE ) )
    {
        // 1. One column = One column ���� : t1.i1=t2.i1

        if ( qtc::dependencyContains( aLeftDepInfo,
                                      & sLeftNode->depInfo ) == ID_TRUE )
        {
            // Nothing to do.
        }
        else
        {
            sLeftNode = (qtcNode *)(aCompareNode->node.arguments->next);
            sRightNode = (qtcNode *)(aCompareNode->node.arguments);
        }

        // ������� ȹ��
        sLeftStatInfo =
            QC_SHARED_TMPLATE(aStatement)->tableMap[sLeftNode->node.table].
            from->tableRef->statInfo;

        sRightStatInfo =
            QC_SHARED_TMPLATE(aStatement)->tableMap[sRightNode->node.table].
            from->tableRef->statInfo;

        if( ( sLeftStatInfo->isValidStat == ID_TRUE ) &&
            ( sRightStatInfo->isValidStat == ID_TRUE ) )
        {
            // 1.1. �� �� �÷� ������� ��� ������

            sLeftSelectivity = getEnhanceSelectivity4Join(
                                        aStatement,
                                        aGraph,
                                        sLeftStatInfo,
                                        sLeftNode );

            sRightSelectivity = getEnhanceSelectivity4Join(
                                        aStatement,
                                        aGraph,
                                        sRightStatInfo,
                                        sRightNode );

            sMinSelectivity = IDL_MIN( sLeftSelectivity, sRightSelectivity );

            if( aIsLeftSemi == ID_TRUE )
            {
                // Left semi join selectivity
                sSelectivity = sMinSelectivity / sRightSelectivity;
            }
            else
            {
                // Right semi join selectivity
                sSelectivity = sMinSelectivity / sLeftSelectivity;
            }
        }
        else
        {
            // 1.2. �� �� �÷� ������� �ϳ��� ������
            sSelectivity = aCompareNode->node.module->selectivity;
        }
    }
    else
    {
        // 2. Etc : t1.i1 > t2.i1, t1.i1=t2.i1+1,
        //          1=t1.i1 or t2.i1=1 ���� �� unit predicate
        sSelectivity = aCompareNode->node.module->selectivity;
    }

    *aSelectivity = sSelectivity;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;
}


IDE_RC
qmoSelectivity::getSelectivity4JoinOrder( qcStatement  * aStatement,
                                          qmgGraph     * aJoinGraph,
                                          qmoPredicate * aJoinPred,
                                          SDouble      * aSelectivity )
{
/******************************************************************************
 *
 * Description : Join ordering �� ���Ǵ� joinOrderFactor �� ���ϱ� ����
 *               join selectivity ��ȯ
 *
 * Implementation :
 *
 *       - Join ordering �� ����� ����ȭ �����̹Ƿ� predicate �̱��� ����
 *       - Join ordering �� ����� ��� qmgJoin �̾�� �ϳ�
 *         joinOrderFactor �� ���ϴ� �������� �ڽ� ��尡 outer join �� ���
 *         �ڽ� ����� joinOrderFactor �� �̿��ϹǷ�
 *         PROJ-2242 �ݿ��� �����ڵ带 �����Ѵ�.
 *         => join ordering ������ ����ϴ� ���� ���� �� �մϴ�.
 *
 *       1. Join selectivity ȹ��
 *          S = PRODUCT( qmoPredicate.mySelectivity for current graph depinfo )
 *       2. Join selectivity ����
 *          child graph �� SELECTION, PARTITION �� ��쿡 ����
 *
 *****************************************************************************/

    qmoPredicate   * sJoinPred;
    qmoPredicate   * sOneTablePred;
    qmoStatistics  * sStatInfo;
    qmoIdxCardInfo * sIdxCardInfo;
    UInt             sIdxCnt;
    idBool           sIsRevise;
    idBool           sIsCurrent;
    SDouble          sJoinSelectivity;
    SDouble          sReviseSelectivity;
    SDouble          sOneTableSelectivity;
    UInt             i;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getSelectivity4JoinOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aJoinGraph != NULL );
    IDE_DASSERT( aJoinPred  != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinPred  = aJoinPred;
    sOneTablePred = NULL;
    sIsCurrent = ID_FALSE;
    sJoinSelectivity     = 1;
    sReviseSelectivity   = 1;
    sOneTableSelectivity = 1;

    //------------------------------------------
    // Selectivity ���
    // - qmgJOIN.graph �� ����ȭ �����̹Ƿ� predicate �̱��� ����
    //------------------------------------------

    while( sJoinPred != NULL )
    {
        // To Fix PR-8005
        // Join Predicate�� Dependencies�� �Ѱܾ� ��.
        IDE_TEST( qmo::currentJoinDependencies4JoinOrder( aJoinGraph,
                                                          & sJoinPred->node->depInfo,
                                                          & sIsCurrent )
                  != IDE_SUCCESS );

        if ( sIsCurrent == ID_TRUE )
        {
            // ���� Join Graph�� join predicate�� ���
            sJoinSelectivity *= sJoinPred->mySelectivity;
        }
        else
        {
            // Nothing to do.
        }

        sJoinPred = sJoinPred->next;
    }

    //------------------------------------------
    // Selectivity ����
    //------------------------------------------
    // PROJ-1502 PARTITIONED DISK TABLE
    // partitioned table�� ���� selectivity�� ����.

    if ( ( aJoinGraph->left->type == QMG_SELECTION ) ||
         ( aJoinGraph->left->type == QMG_PARTITION ) )
    {
        sStatInfo = aJoinGraph->left->myFrom->tableRef->statInfo;
        sIdxCnt = sStatInfo->indexCnt;

        for ( i = 0; i < sIdxCnt; i++ )
        {
            sIdxCardInfo = & sStatInfo->idxCardInfo[i];

            IDE_TEST( getReviseSelectivity4JoinOrder( aStatement,
                                                      aJoinGraph,
                                                      & aJoinGraph->left->depInfo,
                                                      sIdxCardInfo,
                                                      aJoinPred,
                                                      & sReviseSelectivity,
                                                      & sIsRevise )
                      != IDE_SUCCESS );

            if ( sIsRevise  == ID_TRUE )
            {
                if ( sReviseSelectivity > sJoinSelectivity )
                {
                    sJoinSelectivity = sReviseSelectivity;
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
    }
    else
    {
        // Nothing to do.
    }

    if ( ( aJoinGraph->right->type == QMG_SELECTION ) ||
         ( aJoinGraph->right->type == QMG_PARTITION ) )
    {
        sStatInfo = aJoinGraph->right->myFrom->tableRef->statInfo;
        sIdxCnt = sStatInfo->indexCnt;

        for ( i = 0; i < sIdxCnt; i++ )
        {
            sIdxCardInfo = & sStatInfo->idxCardInfo[i];

            IDE_TEST( getReviseSelectivity4JoinOrder( aStatement,
                                                      aJoinGraph,
                                                      & aJoinGraph->right->depInfo,
                                                      sIdxCardInfo,
                                                      aJoinPred,
                                                      & sReviseSelectivity,
                                                      & sIsRevise )
                      != IDE_SUCCESS );

            if ( sIsRevise  == ID_TRUE )
            {
                if ( sReviseSelectivity > sJoinSelectivity )
                {
                    sJoinSelectivity = sReviseSelectivity;
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
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // Left Outer Join�� Full Outer Join�� ���,
    // on Condition CNF�� one table predicate�� ���ؾ� ��
    //------------------------------------------

    switch( aJoinGraph->type )
    {
        case QMG_INNER_JOIN :
        case QMG_SEMI_JOIN :
        case QMG_ANTI_JOIN :
            sOneTablePred = NULL;
            break;
        case QMG_LEFT_OUTER_JOIN :
            sOneTablePred = ((qmgLOJN*)aJoinGraph)->onConditionCNF->oneTablePredicate;
            break;
        case QMG_FULL_OUTER_JOIN :
            sOneTablePred = ((qmgFOJN*)aJoinGraph)->onConditionCNF->oneTablePredicate;
            break;
        default :
            IDE_DASSERT( 0 );
            break;
    }

    // ON ���� ���� one table predicate selectivity ȹ��
    IDE_TEST( getMySelectivity4PredList( sOneTablePred,
                                         & sOneTableSelectivity )
              != IDE_SUCCESS );

    *aSelectivity = sJoinSelectivity * sOneTableSelectivity;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoSelectivity::getReviseSelectivity4JoinOrder( 
                                        qcStatement     * aStatement,
                                        qmgGraph        * aJoinGraph,
                                        qcDepInfo       * aChildDepInfo,
                                        qmoIdxCardInfo  * aIdxCardInfo,
                                        qmoPredicate    * aPredicate,
                                        SDouble         * aSelectivity,
                                        idBool          * aIsRevise )
{
/***********************************************************************
 *
 * Description : Join ordering �� ���Ǵ� joinOrderFactor �� ���ϱ� ����
 *               join selectivity �� ������ ��ȯ
 *
 * Implementation :
 *    (1) Selectivity ���� ���� �˻�
 *        A. Composite Index�� ���
 *        B. Index�� �� Į���� Predicate�� ��� �����ϰ� �� predicate�� ��ȣ��
 *           ���
 *    (2) Selectivity ����
 *        A. �� Join Predicate�� ���Ͽ� ������ �����Ѵ�.
 *           - ������ �ʿ��� Į���� ��� : nothing to do
 *           - ������ �ʿ���� Į���� ���
 *             sSelectivity = sSelectivity * ���� Predicate�� selectivity
 *        C. ������ �ʿ��� Į���� �ϳ��� ������ ���
 *           sSelectivity = sSelectivity * (1/composite index�� cardinality)
 *
 ***********************************************************************/

    mtcColumn      * sKeyCols;
    UInt             sKeyColCnt;
    qmoPredicate   * sCurPred;
    UInt             sIdxKeyColID;
    UInt             sColumnID;
    idBool           sIsEqualPredicate;
    idBool           sExistCommonID;
    idBool           sIsRevise;
    SDouble          sSelectivity;
    idBool           sIsCurrent;
    UInt             i;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::getReviseSelectivity4JoinOrder::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aJoinGraph    != NULL );
    IDE_DASSERT( aChildDepInfo != NULL ); // ���� ��� qmgSelection or qmgPartition
    IDE_DASSERT( aIdxCardInfo  != NULL );
    IDE_DASSERT( aPredicate    != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sKeyCols           = aIdxCardInfo->index->keyColumns;
    sKeyColCnt         = aIdxCardInfo->index->keyColCount;
    sIsRevise          = ID_TRUE;
    sSelectivity       = 1;
    sIsCurrent         = ID_FALSE;

    //------------------------------------------
    // Join Predicate �ʱ�ȭ
    //------------------------------------------

    for ( sCurPred = aPredicate; sCurPred != NULL; sCurPred = sCurPred->next )
    {
        sCurPred->flag &= ~QMO_PRED_USABLE_COMP_IDX_MASK;
    }


    //------------------------------------------
    // Selectivity ���� ���� �˻�
    //------------------------------------------

    if ( sKeyColCnt >= 2 )
    {
        //------------------------------------------
        // Composite Index �� ���
        //------------------------------------------

        for ( i = 0; i < sKeyColCnt; i++ )
        {
            //------------------------------------------
            // Index�� �� Į���� Predicate�� �����ϰ�,
            // �� Predicate�� ��ȣ���� �˻�
            //------------------------------------------

            sExistCommonID = ID_FALSE;

            sIdxKeyColID = sKeyCols[i].column.id;

            for ( sCurPred  = aPredicate;
                  sCurPred != NULL;
                  sCurPred  = sCurPred->next )
            {
                IDE_TEST( qmo::currentJoinDependencies4JoinOrder( aJoinGraph,
                                                                  & sCurPred->node->depInfo,
                                                                  & sIsCurrent )
                          != IDE_SUCCESS );

                if ( sIsCurrent == ID_FALSE )
                {
                    // ���� predicate�� �ش� Join�� join predicate�� �ƴϸ�
                    // selectivity ��꿡 ���Խ�Ű�� ����
                    // ����, nothing to do
                }
                else
                {
                    // ���� qmgJoin�� joinPredicate�� columnID
                    IDE_TEST( qmoPred::setColumnIDToJoinPred( aStatement,
                                                              sCurPred,
                                                              aChildDepInfo )
                              != IDE_SUCCESS );

                    sColumnID = sCurPred->id;

                    if ( sIdxKeyColID == sColumnID )
                    {
                        // �� Į�� ID�� ����
                        IDE_TEST( isEqualPredicate( sCurPred,
                                                    & sIsEqualPredicate )
                                  != IDE_SUCCESS );

                        if ( sIsEqualPredicate == ID_TRUE )
                        {
                            // Predicate�� ��ȣ
                            sExistCommonID = ID_TRUE;
                            sCurPred->flag &= ~QMO_PRED_USABLE_COMP_IDX_MASK;
                            sCurPred->flag |= QMO_PRED_USABLE_COMP_IDX_TRUE;
                        }
                        else
                        {
                            // Predicate�� ��ȣ�� �ƴ� : nothing to do
                        }
                    }
                    else
                    {
                        // �� Į�� ID�� Ʋ�� : nothing to do
                    }
                }
            }
            if ( sExistCommonID == ID_TRUE )
            {
                // �ε����� ���� Į���� ������ column�� join predicate��
                // �����ϸ�, ���� Į������ ����
            }
            else
            {
                // Predicate �� �߿��� �ε����� ���� column�� ������ Į����
                // ������ ��ȣ Predicate�� �������� ������ composite index��
                // ��� column�� ���Ͽ� Predicate�� �ִ� ����� ������
                // �����ϹǷ� Selectivity ������ �� ����
                sIsRevise = ID_FALSE;
                break;
            }
        }
    }
    else
    {
        //------------------------------------------
        // Composite Index �� �ƴ� ��� : selectivity ���� �ʿ����
        //------------------------------------------
    }


    //------------------------------------------
    // ���� ���ο� ���� Selectivity ���
    //------------------------------------------

    if ( sIsRevise == ID_TRUE )
    {
        // Selectivity ���� �ʿ���
        for ( sCurPred  = aPredicate;
              sCurPred != NULL;
              sCurPred  = sCurPred->next )
        {
            IDE_TEST( qmo::currentJoinDependencies4JoinOrder( aJoinGraph,
                                                              & sCurPred->node->depInfo,
                                                              & sIsCurrent )
                      != IDE_SUCCESS );

            if ( sIsCurrent == ID_TRUE )
            {
                if ( ( sCurPred->flag & QMO_PRED_USABLE_COMP_IDX_MASK )
                     == QMO_PRED_USABLE_COMP_IDX_TRUE )
                {
                    // skip
                }
                else
                {
                    sSelectivity *= sCurPred->mySelectivity;
                }
            }
            else
            {
                // ���� predicate�� �ش� Join�� join predicate�� �ƴϸ�
                // selectivity ��꿡 ���Խ�Ű�� ����
                // ����, nothing to do
            }
        }
        sSelectivity *= ( 1 / aIdxCardInfo->KeyNDV);
    }
    else
    {
        // Selectivity ���� �ʿ� ���� : nothing to do
    }

    *aSelectivity = sSelectivity;
    *aIsRevise = sIsRevise;

    IDE_DASSERT_MSG( sSelectivity >= 0 && sSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sSelectivity );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoSelectivity::isEqualPredicate( qmoPredicate * aPredicate,
                                  idBool       * aIsEqual )
{
/***********************************************************************
 *
 * Description : = Predicate ���� �˻�
 *
 * Implementation :
 *    (1) OR Predicate �˻�
 *    (2) OR Predicate�� �ƴ� ���, = Predicate �˻�
 *
 *     ex )
 *         < OR Predicate >
 *
 *           (OR)
 *             |
 *            (=)----------(=)
 *             |            |
 *         (T1.I1)-(1)    (T2.I1)-(2)
 *
 *         < Equal Predicate >
 *
 *           (OR)              (OR)
 *             |                 |
 *            (=)               (=)
 *             |                 |
 *          (T1.I1)-(1)      (T1.I1)-(T2.I1)
 *
 *
 ***********************************************************************/

    mtcNode * sNode;
    idBool    sIsOrPredicate;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::isEqualPredicate::__FT__" );

    sIsOrPredicate = ID_FALSE;
    sNode = &aPredicate->node->node;

    // Or Predicate���� �˻�
    if ( ( sNode->lflag & MTC_NODE_OPERATOR_MASK ) == MTC_NODE_OPERATOR_OR )
    {
        if ( sNode->arguments->next == NULL )
        {
            sIsOrPredicate = ID_FALSE;
            sNode = sNode->arguments;
        }
        else
        {
            sIsOrPredicate = ID_TRUE;
        }
    }
    else
    {
        sIsOrPredicate = ID_FALSE;
    }

    if ( sIsOrPredicate == ID_FALSE )
    {
        if ( sNode->module == & mtfEqual )
        {
            *aIsEqual = ID_TRUE;
        }
        else
        {
            *aIsEqual = ID_FALSE;
        }
    }
    else
    {
        *aIsEqual = ID_FALSE;
    }

    return IDE_SUCCESS;
}

IDE_RC qmoSelectivity::setSetRecursiveOutputCnt( SDouble   aLeftOutputRecordCnt,
                                                 SDouble   aRightOutputRecordCnt,
                                                 SDouble * aOutputRecordCnt )
{
/******************************************************************************
 *
 * Description : PROJ-2582 recursvie with
 *      qmgSetRecursive �� ���� outputRecordCnt ���
 *
 * Implementation :
 *
 *     1. outputRecordCnt ȹ��
 *         left outputRecordCnt�� ���ڵ� ���� ��ŭ ���δ�.
 *         right outputRecordCnt�� left�� ���ڵ� ���� ��ŭ recursive �ϰ� ���� �ȴ�.
 *
 *         recursive selectivity = T(R) / T(L) = S
 *
 *         output record count = T(L) + T(R)     + S * T(R)   + S^2 * T(R) + ...
 *                             = T(L) + S * T(L) + S^2 * T(L) + S^3 * T(L) + ...
 *                             = T(L) * T(L) / ( T(L) - T(R) )
 *
 *     2. outputRecordCnt �ּҰ� ����
 *
 *****************************************************************************/

    SDouble sOutputRecordCnt = 0;
    SDouble sLeftOutputRecordCnt = 0;
    SDouble sRightOutputRecordCnt = 0;

    IDU_FIT_POINT_FATAL( "qmoSelectivity::setSetRecursiveOutputCnt::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aOutputRecordCnt != NULL );
    IDE_DASSERT( aLeftOutputRecordCnt  > 0 );
    IDE_DASSERT( aRightOutputRecordCnt > 0 );

    //--------------------------------------
    // LeftOutputRecordCnt, RightOutputRecordCnt ����
    //--------------------------------------

    sLeftOutputRecordCnt  = aLeftOutputRecordCnt;
    sRightOutputRecordCnt = aRightOutputRecordCnt;

    // R�� �� ū ���� ���ѹݺ��ϴ� ���� ����
    // R�� �� ���� ��쿡 ���ؼ��� �����ϵ��� �Ѵ�.
    // R�� �� �۵��� �����Ͽ� �����Ѵ�.
    if ( sLeftOutputRecordCnt <= sRightOutputRecordCnt )
    {
        sRightOutputRecordCnt = sLeftOutputRecordCnt * 0.9;
    }
    else
    {
        // Nothing to do.
    }
    
    //--------------------------------------
    // outputRecordCnt ���
    //--------------------------------------
    
    // 1. outputRecordCnt ȹ��
    sOutputRecordCnt = sLeftOutputRecordCnt * sLeftOutputRecordCnt /
        ( sLeftOutputRecordCnt - sRightOutputRecordCnt );

    // 2. outputRecordCnt �ּҰ� ����
    if ( sOutputRecordCnt < 1 )
    {
        *aOutputRecordCnt = 1;
    }
    else
    {
        *aOutputRecordCnt = sOutputRecordCnt;
    }

    return IDE_SUCCESS;
}
