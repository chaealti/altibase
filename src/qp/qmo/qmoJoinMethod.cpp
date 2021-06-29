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
 * $Id: qmoJoinMethod.cpp 89835 2021-01-22 10:10:02Z andrew.shin $
 *
 * Description :
 *    Join Cost�� ���Ͽ� �پ��� Join Method�� �߿��� ���� cost �� ����
 *    Join Method�� �����Ѵ�.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <ide.h>
#include <idl.h>
#include <qmoJoinMethod.h>
#include <qmoSelectivity.h>
#include <qmgSelection.h>
#include <qmgJoin.h>
#include <qmo.h>
#include <qmoCost.h>
#include <qmgPartition.h>
#include <qcgPlan.h>
#include <qmoCrtPathMgr.h>
#include <qmgShardSelect.h>
#include <qmv.h>

IDE_RC
qmoJoinMethodMgr::init( qcStatement    * aStatement,
                        qmgGraph       * aGraph,
                        SDouble          aFirstRowsFactor,
                        qmoPredicate   * aJoinPredicate,
                        UInt             aJoinMethodType,
                        qmoJoinMethod  * aJoinMethod )
{
/***********************************************************************
 *
 * Description : Join Method�� �ʱ�ȭ
 *
 * Implementation :
 *    (1) qmoJoinMethod �ʱ�ȭ
 *    (2) Join Method Type�� ���� joinMethodCnt, joinMethodCost ����
 *    (3) joinMethodCost �ʱ�ȭ
 *
 ***********************************************************************/

    qmoJoinMethodCost       * sJoinMethodCost;
    qmoJoinLateralDirection   sLateralDirection;
    UInt                      sJoinMethodCnt;
    UInt                      sCount;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::init::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );

    // Join Method �ʱ�ȭ
    aJoinMethod->flag = QMO_JOIN_METHOD_FLAG_CLEAR;
    aJoinMethod->selectedJoinMethod = NULL;
    aJoinMethod->hintJoinMethod = NULL;
    aJoinMethod->hintJoinMethod2 = NULL;
    aJoinMethod->joinMethodCnt = 0;
    aJoinMethod->joinMethodCost = NULL;

    // PROJ-2418
    // Lateral Position ȹ��
    IDE_TEST( getJoinLateralDirection( aGraph, & sLateralDirection )
              != IDE_SUCCESS );

    // Join Method Type Flag ����  �� Join Method Cost�� �ʱ�ȭ
    switch ( aJoinMethodType & QMO_JOIN_METHOD_MASK )
    {
        case QMO_JOIN_METHOD_NL :
            // Join Method Type ����
            aJoinMethod->flag &= ~QMO_JOIN_METHOD_MASK;
            aJoinMethod->flag |= QMO_JOIN_METHOD_NL;

            // Join Method Cost�� �ʱ�ȭ
            IDE_TEST( initJoinMethodCost4NL( aStatement,
                                             aGraph,
                                             aJoinPredicate,
                                             sLateralDirection,
                                             & sJoinMethodCost,
                                             & sJoinMethodCnt )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_HASH :
            aJoinMethod->flag &= ~QMO_JOIN_METHOD_MASK;
            aJoinMethod->flag |= QMO_JOIN_METHOD_HASH;

            // Join Method Cost�� �ʱ�ȭ
            IDE_TEST( initJoinMethodCost4Hash( aStatement,
                                               aGraph,
                                               aJoinPredicate,
                                               sLateralDirection,
                                               & sJoinMethodCost,
                                               & sJoinMethodCnt )
                      != IDE_SUCCESS );
            break;

        case QMO_JOIN_METHOD_SORT :
            aJoinMethod->flag &= ~QMO_JOIN_METHOD_MASK;
            aJoinMethod->flag |= QMO_JOIN_METHOD_SORT;

            // Join Method Cost�� �ʱ�ȭ
            IDE_TEST( initJoinMethodCost4Sort( aStatement,
                                               aGraph,
                                               aJoinPredicate,
                                               sLateralDirection,
                                               & sJoinMethodCost,
                                               & sJoinMethodCnt)
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_MERGE :
            aJoinMethod->flag &= ~QMO_JOIN_METHOD_MASK;
            aJoinMethod->flag |= QMO_JOIN_METHOD_MERGE;

            // Join Method Cost�� �ʱ�ȭ
            IDE_TEST( initJoinMethodCost4Merge( aStatement,
                                                aGraph,
                                                aJoinPredicate,
                                                sLateralDirection,
                                                & sJoinMethodCost,
                                                & sJoinMethodCnt)
                      != IDE_SUCCESS );
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    for( sCount=0; sCount < sJoinMethodCnt; ++sCount)
    {
        sJoinMethodCost[sCount].firstRowsFactor = aFirstRowsFactor;
    }

    aJoinMethod->joinMethodCost  = sJoinMethodCost;
    aJoinMethod->joinMethodCnt   = sJoinMethodCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoJoinMethodMgr::getBestJoinMethodCost( qcStatement   * aStatement,
                                         qmgGraph      * aGraph,
                                         qmoPredicate  * aJoinPredicate,
                                         qmoJoinMethod * aJoinMethod )
{
    /********************************************************************
     *
     * Description : Join Method Cost �߿� ���� cost�� ���� method ����
     *
     * Implementation :
     *    (1) Join Ordered Hint ó��
     *        left->right ���⸸�� feasibility�� �����ϹǷ�
     *        right->left ������ feasibility�� FALSE�� ����
     *    (2) �� Join Method Cost�� cost�� ���Ѵ�.
     *    (3) Join Method ����
     *        - Join Method Hint�� �����ϴ� ��� :
     *          A. Join Method Hint�� �����ϴ� �� �߿��� ���� cost�� ���� �� ����
     *          B. Join Method Hint�� �����ϴ� ���� ���� ���
     *             hint�� �������� �ʴ� �Ͱ� �����ϰ� ó��
     *        - Join Method Hint�� �������� �ʴ� ��� :
     *          ���� cost�� ���� JoinMethod�� selectedJoinMethod�� ����
     *          ��, Index Nested Loop Join�� Anti Outer Join�� ���
     *          table access hint�� �ִ� ��� �׿� ���� feasibility �˻�
     *
     ***********************************************************************/

    qmsHints           * sHints;
    qmsJoinMethodHints * sJoinMethodHints;
    qmoJoinMethodCost  * sJoinMethodCost;
    qmoJoinMethodCost  * sSelected;
    qmoJoinMethodCost  * sSameTableOrder;
    qmoJoinMethodCost  * sDiffTableOrder;
    idBool               sCurrentHint;
    UInt                 sNoUseHintMask = 0;
    UInt                 i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getBestJoinMethodCost::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aJoinMethod != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sHints          = aGraph->myQuerySet->SFWGH->hints;
    sJoinMethodCost = aJoinMethod->joinMethodCost;
    sSelected       = NULL;
    sCurrentHint    = ID_TRUE;
    sSameTableOrder = NULL;
    sDiffTableOrder = NULL;

    //------------------------------------------
    // Join Ordered Hint ó��
    //    direction�� right->left�� access method�� feasibility��
    //    ��� false�� ���� ( left->right�� ordered �����̹Ƿ� )
    //------------------------------------------

    // PROJ-1495
    // ordered hint�� push_pred hint�� ����Ǵ� ���
    // push_pred hint�� ordered hint���� �켱����ǹǷ�
    // ordered hint�� ����� �� ����.
    if( sHints != NULL )
    {
        if( sHints->pushPredHint == NULL )
        {
            if ( sHints->joinOrderType == QMO_JOIN_ORDER_TYPE_ORDERED )
            {
                for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
                {
                    // BUG-38701
                    sJoinMethodCost = & aJoinMethod->joinMethodCost[i];

                    if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
                         == QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT )
                    {
                        sJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                    }
                    else
                    {
                        // Nothing To Do
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
            IDE_TEST( forcePushPredHint( aGraph,
                                         aJoinMethod )
                      != IDE_SUCCESS );
        }
    }
    else
    {
        // Nothing To Do
    }

    //------------------------------------------
    // recursive view�� join�� ���� �������� ����
    //------------------------------------------

    // PROJ-2582 recursive with
    // push pred, ordered hint�� ���õǾ���.
    IDE_TEST( forceJoinOrder4RecursiveView( aGraph,
                                            aJoinMethod )
              != IDE_SUCCESS );

    //------------------------------------------
    // Join Method Cost ���
    //------------------------------------------

    for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
    {
        sJoinMethodCost = &aJoinMethod->joinMethodCost[i];

        if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_FEASIBILITY_MASK )
             == QMO_JOIN_METHOD_FEASIBILITY_TRUE )
        {
            if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK ) ==
                 QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
            {
                IDE_TEST( getJoinCost( aStatement,
                                       sJoinMethodCost,
                                       aJoinPredicate,
                                       aGraph->left,
                                       aGraph->right )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( getJoinCost( aStatement,
                                       sJoinMethodCost,
                                       aJoinPredicate,
                                       aGraph->right,
                                       aGraph->left )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // feasibility �� FALSE�� ���
            // nothing to do
        }
    }

    //------------------------------------------
    // (1) Join Method Hint�� �����ϴ� Join Method�� ã��
    //    Hint�� �����ϴ� Join Method �� ���� cost�� ���� Method ������ ����
    //     - hintJoinMethod : Table Order���� Join Method Hint�� ������
    //                        Method�� ����Ŵ
    //     - hintJoinMethod2 : Join Method�� Join Method Hint�� ������
    //                         Method�� ����Ŵ
    //                        ( hintJoinMethod�� NULL�϶��� ���� )
    // (2) Join Method Hint�� ���� ���
    //------------------------------------------

    if( sHints != NULL )
    {
        // BUG-42413 NO_USE hint ����
        // NO_USE�� ���� ���� �޼ҵ带 ��� ã�Ƴ���.
        for ( sJoinMethodHints = sHints->joinMethod;
              sJoinMethodHints != NULL;
              sJoinMethodHints = sJoinMethodHints->next )
        {
            IDE_TEST( qmo::currentJoinDependencies( aGraph,
                                                    & sJoinMethodHints->depInfo,
                                                    & sCurrentHint )
                      != IDE_SUCCESS );

            if ( (sCurrentHint == ID_TRUE) &&
                 (sJoinMethodHints->isNoUse == ID_TRUE) )
            {
                sNoUseHintMask |= (sJoinMethodHints->flag & QMO_JOIN_METHOD_MASK);
            }
            else
            {
                // nothing to do
            }
        }

        for ( sJoinMethodHints = sHints->joinMethod;
              sJoinMethodHints != NULL;
              sJoinMethodHints = sJoinMethodHints->next )
        {
            IDE_TEST( qmo::currentJoinDependencies( aGraph,
                                                    & sJoinMethodHints->depInfo,
                                                    & sCurrentHint )
                      != IDE_SUCCESS );

            if ( sCurrentHint == ID_TRUE )
            {
                // ���� �����ؾ��� Hint�� ���,
                // Hint�� ������ Join Method�� ã��
                IDE_TEST( setJoinMethodHint( aJoinMethod,
                                             sJoinMethodHints,
                                             aGraph,
                                             sNoUseHintMask,
                                             & sSameTableOrder,
                                             & sDiffTableOrder )
                          != IDE_SUCCESS );

                if ( sSameTableOrder != NULL )
                {
                    if ( aJoinMethod->hintJoinMethod == NULL )
                    {
                        aJoinMethod->hintJoinMethod = sSameTableOrder;
                    }
                    else
                    {
                        /* BUG-40589 floating point calculation */
                        if (QMO_COST_IS_GREATER(aJoinMethod->hintJoinMethod->totalCost,
                                                sSameTableOrder->totalCost)
                            == ID_TRUE)
                        {
                            aJoinMethod->hintJoinMethod = sSameTableOrder;
                        }
                        else
                        {
                            // nothing to do
                        }
                    }
                    aJoinMethod->hintJoinMethod2 = NULL;
                }
                else
                {
                    if ( sDiffTableOrder != NULL )
                    {
                        if ( aJoinMethod->hintJoinMethod == NULL )
                        {
                            if ( aJoinMethod->hintJoinMethod2 == NULL )
                            {
                                aJoinMethod->hintJoinMethod2 = sDiffTableOrder;
                            }
                            else
                            {
                                /* BUG-40589 floating point calculation */
                                if (QMO_COST_IS_GREATER(aJoinMethod->hintJoinMethod2->totalCost,
                                                        sDiffTableOrder->totalCost)
                                    == ID_TRUE)
                                {
                                    aJoinMethod->hintJoinMethod2 = sDiffTableOrder;
                                }
                                else
                                {
                                    // nothing to do
                                }
                            }
                        }
                        else
                        {
                            // nothing to do
                        }
                    }
                }
            }
            else
            {
                // ���� Join�� ���þ��� Hint
                // nothing to do
            }
        }
    } 
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // Join Method Hint�� ���� ���,
    // Join Method Cost�� ���� ���� Join Method ����
    //------------------------------------------

    if ( aJoinMethod->hintJoinMethod == NULL &&
         aJoinMethod->hintJoinMethod2 == NULL )
    {
        for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
        {
            sJoinMethodCost = & aJoinMethod->joinMethodCost[i];

            if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_FEASIBILITY_MASK )
                 == QMO_JOIN_METHOD_FEASIBILITY_TRUE )
            {
                // feasibility�� TRUE �� ���
                if ( sSelected == NULL )
                {
                    sSelected = sJoinMethodCost;
                }
                else
                {
                    /* BUG-40589 floating point calculation */
                    if (QMO_COST_IS_GREATER(sSelected->totalCost,
                                            sJoinMethodCost->totalCost)
                        == ID_TRUE)
                    {
                        sSelected = sJoinMethodCost;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            }
            else
            {
                // nothing to do
            }
        }
    }
    else
    {
        // nothing to do
    }

    aJoinMethod->selectedJoinMethod = sSelected;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::printJoinMethod( qmoJoinMethod * aMethod,
                                   ULong           aDepth,
                                   iduVarString  * aString )
{
/***********************************************************************
 *
 * Description :
 *    Join Method ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    UInt  i;
    UInt  j;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::printJoinMethod::__FT__" );

    //-----------------------------------
    // ���ռ� �˻�
    //-----------------------------------

    IDE_DASSERT( aMethod != NULL );
    IDE_DASSERT( aString != NULL );

    //-----------------------------------
    // Join Method�� ���� ���
    //-----------------------------------

    for ( j = 0; j < aMethod->joinMethodCnt; j++ )
    {
        if ( ( aMethod->joinMethodCost[j].flag &
               QMO_JOIN_METHOD_FEASIBILITY_MASK )
             == QMO_JOIN_METHOD_FEASIBILITY_TRUE )
        {
            // ���� ������ Join Method

            //-----------------------------------
            // Join Method�� ���� ���
            //-----------------------------------

            QMG_PRINT_LINE_FEED( i, aDepth, aString );

            switch ( aMethod->joinMethodCost[j].flag &
                     QMO_JOIN_METHOD_MASK )
            {
                case QMO_JOIN_METHOD_FULL_NL       :
                    iduVarStringAppend( aString,
                                        "FULL_NESTED_LOOP[" );
                    break;
                case QMO_JOIN_METHOD_INDEX         :
                    iduVarStringAppend( aString,
                                        "INDEX_NESTED_LOOP[" );
                    iduVarStringAppendFormat( aString, "%s,",
                                              aMethod->joinMethodCost[j].
                                              rightIdxInfo->index->name );
                    break;
                case QMO_JOIN_METHOD_INVERSE_INDEX :
                    iduVarStringAppend( aString,
                                        "INVERSE_INDEX_NESTED_LOOP[" );
                    iduVarStringAppendFormat( aString, "%s,",
                                              aMethod->joinMethodCost[j].
                                              rightIdxInfo->index->name );
                    break;
                case QMO_JOIN_METHOD_FULL_STORE_NL :
                    iduVarStringAppend( aString,
                                        "FULL_STORE_NESTED_LOOP[" );
                    break;
                case QMO_JOIN_METHOD_ANTI          :
                    iduVarStringAppend( aString,
                                        "ANTI_OUTER_NESTED_LOOP[" );
                    iduVarStringAppendFormat( aString, "%s, %s",
                                              aMethod->joinMethodCost[j]
                                              .leftIdxInfo->index->name,
                                              aMethod->joinMethodCost[j]
                                              .rightIdxInfo->index->name );
                    break;
                case QMO_JOIN_METHOD_ONE_PASS_HASH :
                    iduVarStringAppend( aString,
                                        "ONE_PASS_HASH[" );
                    break;
                case QMO_JOIN_METHOD_TWO_PASS_HASH :
                    iduVarStringAppend( aString,
                                        "MULTI_PASS_HASH[" );
                    break;
                case QMO_JOIN_METHOD_INVERSE_HASH :
                    iduVarStringAppend( aString,
                                        "INVERSE_HASH[" );
                    break;
                case QMO_JOIN_METHOD_ONE_PASS_SORT :
                    iduVarStringAppend( aString,
                                        "ONE_PASS_SORT[" );
                    break;
                case QMO_JOIN_METHOD_TWO_PASS_SORT :
                    iduVarStringAppend( aString,
                                        "MULTI_PASS_SORT[" );
                    break;
                case QMO_JOIN_METHOD_INVERSE_SORT :
                    iduVarStringAppend( aString,
                                        "INVERSE_SORT[" );
                    break;
                case QMO_JOIN_METHOD_MERGE         :
                    iduVarStringAppend( aString,
                                        "MERGE[" );
                    break;
                default                            :
                    IDE_FT_ASSERT( 0 );
                    break;
            }

            //-----------------------------------
            // Join ������ ���� ���
            //-----------------------------------

            if ( ( aMethod->joinMethodCost[j].flag &
                   QMO_JOIN_METHOD_DIRECTION_MASK )
                 == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
            {
                iduVarStringAppend( aString,
                                    "LEFT=>RIGHT]" );
            }
            else
            {
                iduVarStringAppend( aString,
                                    "RIGHT=>LEFT]" );
            }

            //-----------------------------------
            // Join ������ ���
            //-----------------------------------

            // Selectivity
            QMG_PRINT_LINE_FEED( i, aDepth, aString );
            iduVarStringAppendFormat( aString,
                                      "  SELECTIVITY : %"ID_PRINT_G_FMT,
                                      aMethod->joinMethodCost[j].selectivity );

            QMG_PRINT_LINE_FEED( i, aDepth, aString );
            iduVarStringAppendFormat( aString,
                                      "  ACCESS_COST : %"ID_PRINT_G_FMT,
                                      aMethod->joinMethodCost[j].accessCost );

            QMG_PRINT_LINE_FEED( i, aDepth, aString );
            iduVarStringAppendFormat( aString,
                                      "  DISK_COST   : %"ID_PRINT_G_FMT,
                                      aMethod->joinMethodCost[j].diskCost );

            QMG_PRINT_LINE_FEED( i, aDepth, aString );
            iduVarStringAppendFormat( aString,
                                      "  TOTAL_COST  : %"ID_PRINT_G_FMT,
                                      aMethod->joinMethodCost[j].totalCost );

        }
        else
        {
            // ������ �Ұ����� Join Method��
        }
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoJoinMethodMgr::getJoinCost( qcStatement       * aStatement,
                               qmoJoinMethodCost * aMethod,
                               qmoPredicate      * aJoinPredicate,
                               qmgGraph          * aLeft,
                               qmgGraph          * aRight)
{
/***********************************************************************
 *
 * Description : Join Cost ����
 *
 * Implementation :
 *    Join Method Type�� �´� Join Cost ���⸦ ȣ���Ѵ�.
 *
 ***********************************************************************/

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getJoinCost::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aMethod != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // Cost ���
    //------------------------------------------
    switch( aMethod->flag & QMO_JOIN_METHOD_MASK )
    {
        case QMO_JOIN_METHOD_FULL_NL :
            IDE_TEST( getFullNestedLoop( aStatement,
                                         aMethod,
                                         aJoinPredicate,
                                         aLeft,
                                         aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_INDEX :
            IDE_TEST( getIndexNestedLoop( aStatement,
                                          aMethod,
                                          aJoinPredicate,
                                          aLeft,
                                          aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_INVERSE_INDEX :
            IDE_TEST( getIndexNestedLoop( aStatement,
                                          aMethod,
                                          aJoinPredicate,
                                          aLeft,
                                          aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_FULL_STORE_NL :
            IDE_TEST( getFullStoreNestedLoop( aStatement,
                                              aMethod,
                                              aJoinPredicate,
                                              aLeft,
                                              aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_ANTI :
            IDE_TEST( getAntiOuter( aStatement,
                                    aMethod,
                                    aJoinPredicate,
                                    aLeft,
                                    aRight )
                != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_ONE_PASS_HASH :
            IDE_TEST( getOnePassHash( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_TWO_PASS_HASH :
            IDE_TEST( getTwoPassHash( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_INVERSE_HASH :
            IDE_TEST( getInverseHash( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_ONE_PASS_SORT :
            IDE_TEST( getOnePassSort( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_TWO_PASS_SORT :
            IDE_TEST( getTwoPassSort( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_INVERSE_SORT :
            IDE_TEST( getInverseSort( aStatement,
                                      aMethod,
                                      aJoinPredicate,
                                      aLeft,
                                      aRight )
                      != IDE_SUCCESS );
            break;
        case QMO_JOIN_METHOD_MERGE :
            IDE_TEST( getMerge( aStatement,
                                aMethod,
                                aJoinPredicate,
                                aLeft,
                                aRight )
                      != IDE_SUCCESS );
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getFullNestedLoop( qcStatement       * aStatement,
                                     qmoJoinMethodCost * aJoinMethodCost,
                                     qmoPredicate      * aJoinPredicate,
                                     qmgGraph          * aLeft,
                                     qmgGraph          * aRight)
{
    SDouble         sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getFullNestedLoop::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
    aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;

    // left ����� cost ���� �����.
    QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

    sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                          aJoinPredicate,
                                          (aLeft->costInfo.outputRecordCnt *
                                           aRight->costInfo.outputRecordCnt) );

    // BUG-37407 semi, anti join cost
    if( (aJoinMethodCost->flag & QMO_JOIN_SEMI_MASK)
         == QMO_JOIN_SEMI_TRUE )
    {
        aJoinMethodCost->totalCost =
                qmoCost::getFullNestedJoin4SemiCost( aLeft,
                                                     aRight,
                                                     aJoinMethodCost->selectivity,
                                                     sFilterCost,
                                                     &(aJoinMethodCost->accessCost),
                                                     &(aJoinMethodCost->diskCost) );
    }
    else if( (aJoinMethodCost->flag & QMO_JOIN_ANTI_MASK)
              == QMO_JOIN_ANTI_TRUE )
    {
        aJoinMethodCost->totalCost =
                qmoCost::getFullNestedJoin4AntiCost( aLeft,
                                                     aRight,
                                                     aJoinMethodCost->selectivity,
                                                     sFilterCost,
                                                     &(aJoinMethodCost->accessCost),
                                                     &(aJoinMethodCost->diskCost) );
    }
    else
    {
        aJoinMethodCost->totalCost =
                qmoCost::getFullNestedJoinCost( aLeft,
                                                aRight,
                                                sFilterCost,
                                                &(aJoinMethodCost->accessCost),
                                                &(aJoinMethodCost->diskCost) );

        /* BUG-48234 Recursive With Join cost adjust */
        if ( ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
              == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
             ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
              == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) ||
             ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
              == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
             ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
              == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
        {
            aJoinMethodCost->totalCost = aJoinMethodCost->totalCost * 8;
        }
    }

    // left ����� cost ���� �����Ѵ�.
    QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::getFullStoreNestedLoop( qcStatement       * aStatement,
                                          qmoJoinMethodCost * aJoinMethodCost,
                                          qmoPredicate      * aJoinPredicate,
                                          qmgGraph          * aLeft,
                                          qmgGraph          * aRight)
{
    SDouble sFilterCost = 1;
    idBool  sIsDisk;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getFullStoreNestedLoop::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    IDE_TEST( qmg::isDiskTempTable( aRight, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Cost �� ���
    //------------------------------------------
    QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

    sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                          aJoinPredicate,
                                          (aLeft->costInfo.outputRecordCnt *
                                           aRight->costInfo.outputRecordCnt) );

    aJoinMethodCost->totalCost =
            qmoCost::getFullStoreJoinCost( aLeft,
                                           aRight,
                                           sFilterCost,
                                           aStatement->mSysStat,
                                           sIsDisk,
                                           &(aJoinMethodCost->accessCost),
                                           &(aJoinMethodCost->diskCost) );

    /* BUG-48234 Recursive With Join cost adjust */
    if ( ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
          == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
         ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
          == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) ||
         ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
          == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
         ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
          == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
    {
        aJoinMethodCost->totalCost = aJoinMethodCost->totalCost * 4;
    }

    QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getIndexNestedLoop( qcStatement       * aStatement,
                                      qmoJoinMethodCost * aJoinMethodCost,
                                      qmoPredicate      * aJoinPredicate,
                                      qmgGraph          * aLeft,
                                      qmgGraph          * aRight)
{
    UInt            i;
    SDouble         sRightIndexMemCost  = 0;
    SDouble         sRightIndexDiskCost = 0;
    SDouble         sTempCost           = 0;
    SDouble         sTempMemCost        = 0;
    SDouble         sTempDiskCost       = 0;

    // BUG-36450
    SDouble         sMinCost            = QMO_COST_INVALID_COST;
    SDouble         sMinMemCost         = 0;
    SDouble         sMinDiskCost        = 0;
    idBool          sIsDisk;
    qmoPredInfo   * sJoinPredicate      = NULL;
    qmoIdxCardInfo* sIdxInfo            = NULL;

    UInt              sAccessMethodCnt;
    qmoAccessMethod * sAccessMethod;
    // BUG-45169
    SDouble           sMinIndexCost     = QMO_COST_INVALID_COST;
    SDouble           sCurrIndexCost    = QMO_COST_INVALID_COST;
    idBool            sIsChange         = ID_FALSE;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getIndexNestedLoop::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aLeft, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY;
    }

    if( aRight->type == QMG_SELECTION )
    {
        sAccessMethodCnt = ((qmgSELT*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgSELT*) aRight)->accessMethod;
    }
    else if( aRight->type == QMG_PARTITION )
    {
        sAccessMethodCnt = ((qmgPARTITION*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgPARTITION*) aRight)->accessMethod;
    }
    else if ( aRight->type == QMG_SHARD_SELECT )
    {
        // PROJ-2638
        sAccessMethodCnt = ((qmgShardSELT*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgShardSELT*) aRight)->accessMethod;
    }
    else
    {
        sAccessMethodCnt = 0;
        sAccessMethod    = NULL;
    }

    QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

    // BUG-36454 index hint �� ����ϸ� sAccessMethod[0] �� fullscan �ƴϴ�.
    for ( i = 0; i < sAccessMethodCnt; i++ )
    {
        sIsChange       = ID_FALSE;
        sCurrIndexCost  = QMO_COST_INVALID_COST;

        if ( sAccessMethod[i].method != NULL )
        {
            // keyRange �� �����س���.
            IDE_TEST ( setJoinPredInfo4NL( aStatement,
                                           aJoinPredicate,
                                           sAccessMethod[i].method,
                                           aRight,
                                           aJoinMethodCost )
                       != IDE_SUCCESS );

            if ( aJoinMethodCost->joinPredicate != NULL )
            {
                // BUG-42429 index cost �� �ٽ� ����ؾ���
                adjustIndexCost( aStatement,
                                 aRight,
                                 sAccessMethod[i].method,
                                 aJoinMethodCost->joinPredicate,
                                 &sRightIndexMemCost,
                                 &sRightIndexDiskCost );

                // BUG-37407 semi, anti join cost
                if ( (aJoinMethodCost->flag & QMO_JOIN_SEMI_MASK)
                    == QMO_JOIN_SEMI_TRUE )
                {
                    if ( ( aJoinMethodCost->flag & QMO_JOIN_METHOD_MASK )
                           == QMO_JOIN_METHOD_INVERSE_INDEX )
                    {
                        sTempCost =
                        qmoCost::getInverseIndexNestedJoinCost( aLeft,
                                                                aRight->myFrom->tableRef->statInfo,
                                                                sAccessMethod[i].method,
                                                                aStatement->mSysStat,
                                                                sIsDisk,
                                                                0, // filter cost
                                                                &sTempMemCost,
                                                                &sTempDiskCost );

                        // BUG-45169 semi inverse index NL join �� ����, index cost�� ����� �ʿ䰡 �ֽ��ϴ�.
                        sCurrIndexCost = sRightIndexMemCost + sRightIndexDiskCost;
                        IDE_DASSERT(QMO_COST_IS_LESS(sCurrIndexCost, 0.0) == ID_FALSE);

                    }
                    else
                    {
                        sTempCost =
                        qmoCost::getIndexNestedJoin4SemiCost( aLeft,
                                                              aRight->myFrom->tableRef->statInfo,
                                                              sAccessMethod[i].method,
                                                              aStatement->mSysStat,
                                                              0, // filter cost
                                                              &sTempMemCost,
                                                              &sTempDiskCost );
                        // BUG-48327 semi join wrong index
                        sCurrIndexCost = sRightIndexMemCost + sRightIndexDiskCost;
                    }
                }
                else if ( (aJoinMethodCost->flag & QMO_JOIN_ANTI_MASK)
                          == QMO_JOIN_ANTI_TRUE )
                {
                    sTempCost =
                    qmoCost::getIndexNestedJoin4AntiCost( aLeft,
                                                          aRight->myFrom->tableRef->statInfo,
                                                          sAccessMethod[i].method,
                                                          aStatement->mSysStat,
                                                          0, // filter cost
                                                          &sTempMemCost,
                                                          &sTempDiskCost );
                        // BUG-48327 semi join wrong index
                        sCurrIndexCost = sRightIndexMemCost + sRightIndexDiskCost;
                }
                else
                {
                    sTempCost =
                    qmoCost::getIndexNestedJoinCost( aLeft,
                                                     aRight->myFrom->tableRef->statInfo,
                                                     sAccessMethod[i].method,
                                                     aStatement->mSysStat,
                                                     sRightIndexMemCost,
                                                     sRightIndexDiskCost,
                                                     0, // filter cost
                                                     &sTempMemCost,
                                                     &sTempDiskCost );
                }

                // �ּ� Cost �� index �� �����Ѵ�.
                if (((QMO_COST_IS_EQUAL(sMinCost, QMO_COST_INVALID_COST) == ID_TRUE) ||
                     (QMO_COST_IS_LESS(sTempCost, sMinCost) == ID_TRUE)))
                {
                    sIsChange = ID_TRUE;
                }
                else
                {
                    /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����.
                         0 ( 7.1.0 default ) : primary key �켱������ ���� (BUG-44850) +
                             Inverse index NL ������ �� index cost�� ���� �ε��� ���� (BUG-45169)
                         1 ( 6.5.1. default ): ���� �÷� ��İ� ����
                         2 ( trunk defulat ) : primary key �켱������ ���� (BUG-44850) +
                             Inverse index NL ������ �� index cost�� ���� �ε��� ���� (BUG-45169) +
                             index NL ���� �϶� index cost�� ���� �ε��� ���� + ( BUG-48327 )
                             Anti ���� �϶� index cost�� ���� �ε��� ����
                    */
                    if ( (QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY != 1) &&
                         (QMO_COST_IS_EQUAL( sTempCost, sMinCost ) == ID_TRUE) )
                    {
                        if ( QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY == 0 )
                        {
                            // BUG-45169 semi inverse index NL join �� ����, index cost�� ����� �ʿ䰡 �ֽ��ϴ�.
                            if ( ( aJoinMethodCost->flag & QMO_JOIN_METHOD_MASK ) 
                                 == QMO_JOIN_METHOD_INVERSE_INDEX )
                            {
                                if ( QMO_COST_IS_LESS(sCurrIndexCost, sMinIndexCost) == ID_TRUE )
                                {
                                    // sMinIndexCost�� ���� QMO_COST_INVALID_COST �� �� ����.
                                    IDE_DASSERT(QMO_COST_IS_LESS(sMinIndexCost, 0.0) == ID_FALSE);
                                    sIsChange = ID_TRUE;
                                }
                                else
                                {
                                    /* BUG-44850 Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
                                    if ( ( QMO_COST_IS_EQUAL( sCurrIndexCost, sMinIndexCost) == ID_TRUE ) &&
                                         ( (sAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_PRIMARY_MASK) ==
                                           QMO_STAT_CARD_IDX_PRIMARY_TRUE ) )
                                    {
                                        sIsChange = ID_TRUE; 
                                    }
                                    else
                                    {
                                        // Nothing to do.
                                    }
                                }
                            }
                            else
                            {
                                /* BUG-44850 Index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
                                if ( (sAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_PRIMARY_MASK) ==
                                     QMO_STAT_CARD_IDX_PRIMARY_TRUE )
                                {
                                    sIsChange = ID_TRUE; 
                                }
                                else
                                {
                                    // Nothing to do.
                                }
                            }
                        }
                        else if ( QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY == 2 )
                        {
                            if ( QMO_COST_IS_LESS(sCurrIndexCost, sMinIndexCost) == ID_TRUE )
                            {
                                // sMinIndexCost�� ���� QMO_COST_INVALID_COST �� �� ����.
                                IDE_DASSERT(QMO_COST_IS_LESS(sMinIndexCost, 0.0) == ID_FALSE);
                                sIsChange = ID_TRUE;
                            }
                            else
                            {
                                if ( ( QMO_COST_IS_EQUAL( sCurrIndexCost, sMinIndexCost) == ID_TRUE ) &&
                                     ( (sAccessMethod[i].method->flag & QMO_STAT_CARD_IDX_PRIMARY_MASK) ==
                                       QMO_STAT_CARD_IDX_PRIMARY_TRUE ) )
                                {
                                    sIsChange = ID_TRUE;
                                }
                            }
                        }
                    }
                    else
                    {
                        // Nothing to do.
                    }

                    qcgPlan::registerPlanProperty( aStatement,
                                                   PLAN_PROPERTY_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY );
                }

                if ( sIsChange == ID_TRUE )
                {
                    sMinCost        = sTempCost;
                    sMinMemCost     = sTempMemCost;
                    sMinDiskCost    = sTempDiskCost;
                    sJoinPredicate  = aJoinMethodCost->joinPredicate;
                    sIdxInfo        = sAccessMethod[i].method;
                    sMinIndexCost   = sCurrIndexCost;
                }
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

    if (QMO_COST_IS_EQUAL(sMinCost, QMO_COST_INVALID_COST) == ID_FALSE)
    {
        IDE_DASSERT(QMO_COST_IS_LESS(sMinCost, 0.0) == ID_FALSE);

        // BUG-39403
        // INVERSE_INDEX Join Method ������,
        // Predicate List�� ��� Join Predicate��
        // �� ������ �� �� operand�� ��� Column Node���� �Ѵ�.
        if ( ( ( aJoinMethodCost->flag & QMO_JOIN_METHOD_MASK )
               == QMO_JOIN_METHOD_INVERSE_INDEX ) &&
             ( qmoPred::hasOnlyColumnCompInPredList( sJoinPredicate ) == ID_FALSE ) )
        {
            aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // INVERSE_INDEX Join Method�� �ƴϰų�,
            // INVERSE_INDEX �̰� Join Predicate�� ��� Column Node�� ���ϴ� ���
            aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;

            aJoinMethodCost->accessCost   = sMinMemCost;
            aJoinMethodCost->diskCost     = sMinDiskCost;
            aJoinMethodCost->totalCost    = sMinCost;

            /* BUG-48234 Recursive With Join cost adjust */
            if ( ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                  == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
                 ( ( aLeft->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                  == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) ||
                 ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                  == QMV_QUERYSET_RECURSIVE_VIEW_LEFT ) ||
                 ( ( aRight->myQuerySet->lflag & QMV_QUERYSET_RECURSIVE_VIEW_MASK )
                  == QMV_QUERYSET_RECURSIVE_VIEW_RIGHT ) )
            {
                aJoinMethodCost->totalCost = aJoinMethodCost->totalCost * 2;
            }
            IDE_DASSERT( sJoinPredicate != NULL );
            IDE_DASSERT( sIdxInfo != NULL );

            aJoinMethodCost->joinPredicate = sJoinPredicate;
            aJoinMethodCost->rightIdxInfo  = sIdxInfo;
        }
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getAntiOuter( qcStatement       * aStatement,
                                qmoJoinMethodCost * aJoinMethodCost,
                                qmoPredicate      * aJoinPredicate,
                                qmgGraph          * aLeft,
                                qmgGraph          * aRight )
{
    UInt            i;
    SDouble         sTempCost       = 0;
    SDouble         sTempMemCost    = 0;
    SDouble         sTempDiskCost   = 0;

    // BUG-36450
    SDouble         sMinCost        = QMO_COST_INVALID_COST;
    SDouble         sMinMemCost     = 0;
    SDouble         sMinDiskCost    = 0;
    SDouble         sFilterCost     = 1;
    qmoPredInfo   * sJoinPredicate  = NULL;
    qmoIdxCardInfo* sLeftIdxInfo    = NULL;
    qmoIdxCardInfo* sRightIdxInfo   = NULL;

    UInt              sAccessMethodCnt;
    qmoAccessMethod * sAccessMethod;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getAntiOuter::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    if( aRight->type == QMG_SELECTION )
    {
        sAccessMethodCnt = ((qmgSELT*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgSELT*) aRight)->accessMethod;
    }
    else if( aRight->type == QMG_PARTITION )
    {
        sAccessMethodCnt = ((qmgPARTITION*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgPARTITION*) aRight)->accessMethod;
    }
    else if ( aRight->type == QMG_SHARD_SELECT )
    {
        // PROJ-2638
        sAccessMethodCnt = ((qmgShardSELT*) aRight)->accessMethodCnt;
        sAccessMethod    = ((qmgShardSELT*) aRight)->accessMethod;
    }
    else
    {
        sAccessMethodCnt = 0;
        sAccessMethod    = NULL;
    }

    QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

    // BUG-36454 index hint �� ����ϸ� sAccessMethod[0] �� fullscan �ƴϴ�.
    for( i = 0; i < sAccessMethodCnt; i++ )
    {
        if ( sAccessMethod[i].method != NULL )
        {
            IDE_TEST( setJoinPredInfo4AntiOuter( aStatement,
                                                 aJoinPredicate,
                                                 sAccessMethod[i].method,
                                                 aRight,
                                                 aLeft,
                                                 aJoinMethodCost )
                         != IDE_SUCCESS );

            if( aJoinMethodCost->joinPredicate != NULL )
            {
                sTempCost =
                    qmoCost::getAntiOuterJoinCost( aLeft,
                                                   sAccessMethod[i].accessCost,
                                                   sAccessMethod[i].diskCost,
                                                   sFilterCost,
                                                   &sTempMemCost,
                                                   &sTempDiskCost );

                // �ּ� Cost �� index �� �����Ѵ�.
                /* BUG-40589 floating point calculation */
                if ((QMO_COST_IS_EQUAL(sMinCost, QMO_COST_INVALID_COST) == ID_TRUE) ||
                    (QMO_COST_IS_LESS(sTempCost, sMinCost) == ID_TRUE))
                {
                    sMinCost        = sTempCost;
                    sMinMemCost     = sTempMemCost;
                    sMinDiskCost    = sTempDiskCost;
                    sJoinPredicate  = aJoinMethodCost->joinPredicate;
                    sLeftIdxInfo    = aJoinMethodCost->leftIdxInfo;
                    sRightIdxInfo   = sAccessMethod[i].method;
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

    /* BUG-40589 floating point calculation */
    if (QMO_COST_IS_GREATER(sMinCost, 0) == ID_TRUE)
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;

        aJoinMethodCost->accessCost   = sMinMemCost;
        aJoinMethodCost->diskCost     = sMinDiskCost;
        aJoinMethodCost->totalCost    = sMinCost;

        IDE_DASSERT( sJoinPredicate != NULL );
        IDE_DASSERT( sRightIdxInfo  != NULL );
        IDE_DASSERT( sLeftIdxInfo   != NULL );

        aJoinMethodCost->joinPredicate = sJoinPredicate;
        aJoinMethodCost->rightIdxInfo  = sRightIdxInfo;
        aJoinMethodCost->leftIdxInfo   = sLeftIdxInfo;
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getOnePassSort( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aJoinMethodCost,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight)
{
    idBool  sIsDisk;
    idBool  sUseOrder;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getOnePassSort::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Preserved Order�� ��� ���� ���� �Ǵ�
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Sort( aStatement,
                                    aJoinPredicate,
                                    &(aRight->depInfo),
                                    aJoinMethodCost )
              != IDE_SUCCESS );

    if( aJoinMethodCost->joinPredicate != NULL )
    {
        IDE_TEST( canUsePreservedOrder( aStatement,
                                        aJoinMethodCost,
                                        aRight,
                                        QMO_JOIN_CHILD_RIGHT,
                                        & sUseOrder )
                  != IDE_SUCCESS );

        if ( sUseOrder == ID_TRUE )
        {
            // Child Node�� ���� ����
            aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_STORE;
        }
        else
        {
            // Child Node�� ���� ����
            aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_SORTING;
        }

        //------------------------------------------
        // Cost �� ���
        //------------------------------------------

        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost =
                qmoCost::getOnePassSortJoinCost( aJoinMethodCost,
                                                 aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 &(aJoinMethodCost->accessCost),
                                                 &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getTwoPassSort( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aJoinMethodCost,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight)
{
    idBool  sLeftIsDisk;
    idBool  sRightIsDisk;
    idBool  sLeftUseOrder;
    idBool  sRightUseOrder;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getTwoPassSort::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------
    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // Left ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aLeft, & sLeftIsDisk )
              != IDE_SUCCESS );

    if ( sLeftIsDisk == ID_TRUE )
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_DISK;
    }
    else
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY;
    }

    // Right ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sRightIsDisk )
              != IDE_SUCCESS );

    if ( sRightIsDisk == ID_TRUE )
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Preserved Order�� ��� ���� ���� �Ǵ�
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Sort( aStatement,
                                    aJoinPredicate,
                                    &(aRight->depInfo),
                                    aJoinMethodCost )
              != IDE_SUCCESS );

    if( aJoinMethodCost->joinPredicate != NULL )
    {
        // Left�� Order ��� ���� ���� �Ǵ�
        IDE_TEST( canUsePreservedOrder( aStatement,
                                        aJoinMethodCost,
                                        aLeft,
                                        QMO_JOIN_CHILD_LEFT,
                                        & sLeftUseOrder )
                  != IDE_SUCCESS );

        if ( sLeftUseOrder == ID_TRUE )
        {
            // BUG-37861 two pass sort �϶��� ��带 ������ ������ �Ѵ�.
            aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_STORE;
        }
        else
        {
            aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_SORTING;
        }

        // Right�� Order ��� ���� ���� �Ǵ�
        IDE_TEST( canUsePreservedOrder( aStatement,
                                        aJoinMethodCost,
                                        aRight,
                                        QMO_JOIN_CHILD_RIGHT,
                                        & sRightUseOrder )
                  != IDE_SUCCESS );

        if ( sRightUseOrder == ID_TRUE )
        {
            // Child Node�� ���� ����
            aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_STORE;
        }
        else
        {
            // Child Node�� ���� ����
            aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_SORTING;
        }

        //------------------------------------------
        // Cost �� ���
        //------------------------------------------
        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost =
                qmoCost::getTwoPassSortJoinCost( aJoinMethodCost,
                                                 aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 &(aJoinMethodCost->accessCost),
                                                 &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoJoinMethodMgr::getInverseSort( qcStatement       * aStatement,
                                         qmoJoinMethodCost * aMethod,
                                         qmoPredicate      * aJoinPredicate,
                                         qmgGraph          * aLeft,
                                         qmgGraph          * aRight)
{
    idBool  sIsDisk;
    idBool  sUseOrder;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getInverseSort::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aMethod != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aMethod->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aMethod->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aMethod->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aMethod->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Preserved Order�� ��� ���� ���� �Ǵ�
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Sort( aStatement,
                                    aJoinPredicate,
                                    &(aRight->depInfo),
                                    aMethod )
              != IDE_SUCCESS );

    if( aMethod->joinPredicate != NULL )
    {
        IDE_TEST( canUsePreservedOrder( aStatement,
                                        aMethod,
                                        aRight,
                                        QMO_JOIN_CHILD_RIGHT,
                                        & sUseOrder )
                  != IDE_SUCCESS );

        if ( sUseOrder == ID_TRUE )
        {
            // Child Node�� ���� ����
            aMethod->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aMethod->flag |= QMO_JOIN_RIGHT_NODE_STORE;
        }
        else
        {
            // Child Node�� ���� ����
            aMethod->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aMethod->flag |= QMO_JOIN_RIGHT_NODE_SORTING;
        }

        //------------------------------------------
        // Cost �� ���
        //------------------------------------------

        QMO_FIRST_ROWS_SET(aLeft, aMethod);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aMethod->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aMethod->selectivity) );

        aMethod->totalCost =
                qmoCost::getInverseSortJoinCost( aMethod,
                                                 aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 &(aMethod->accessCost),
                                                 &(aMethod->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aMethod);

        aMethod->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aMethod->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
    }
    else
    {
        aMethod->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aMethod->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getOnePassHash( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aJoinMethodCost,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight)
{
    idBool  sIsDisk;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getOnePassHash::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Cost �� ���
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Hash( aStatement,
                                    aJoinPredicate,
                                    aJoinMethodCost )
              != IDE_SUCCESS );

    if( aJoinMethodCost->joinPredicate != NULL )
    {
        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost =
                qmoCost::getOnePassHashJoinCost( aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 sIsDisk,
                                                 &(aJoinMethodCost->accessCost),
                                                 &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
        //------------------------------------------
        // ���� ����� ���
        //------------------------------------------

        aJoinMethodCost->totalCost *= QCU_OPTIMIZER_HASH_JOIN_COST_ADJ;
        aJoinMethodCost->totalCost /= 100.0;

        qcgPlan::registerPlanProperty(aStatement,
                                      PLAN_PROPERTY_OPTIMIZER_HASH_JOIN_COST_ADJ);
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getTwoPassHash( qcStatement       * aStatement,
                                  qmoJoinMethodCost * aJoinMethodCost,
                                  qmoPredicate      * aJoinPredicate,
                                  qmgGraph          * aLeft,
                                  qmgGraph          * aRight)
{
    idBool  sLeftIsDisk;
    idBool  sRightIsDisk;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getTwoPassHash::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // Left ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aLeft, & sLeftIsDisk )
              != IDE_SUCCESS );

    if ( sLeftIsDisk == ID_TRUE )
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_DISK;
    }
    else
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY;
    }

    // Right ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sRightIsDisk )
              != IDE_SUCCESS );

    if ( sRightIsDisk == ID_TRUE )
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Cost �� ���
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Hash( aStatement,
                                    aJoinPredicate,
                                    aJoinMethodCost )
              != IDE_SUCCESS );

    if( aJoinMethodCost->joinPredicate != NULL )
    {
        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost =
                qmoCost::getTwoPassHashJoinCost( aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 sLeftIsDisk,
                                                 sRightIsDisk,
                                                 &(aJoinMethodCost->accessCost),
                                                 &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;

        //------------------------------------------
        // ���� ����� ���
        //------------------------------------------
        aJoinMethodCost->totalCost *= QCU_OPTIMIZER_HASH_JOIN_COST_ADJ;
        aJoinMethodCost->totalCost /= 100.0;

        qcgPlan::registerPlanProperty(aStatement,
                                      PLAN_PROPERTY_OPTIMIZER_HASH_JOIN_COST_ADJ);
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoJoinMethodMgr::getInverseHash( qcStatement       * aStatement,
                                         qmoJoinMethodCost * aJoinMethodCost,
                                         qmoPredicate      * aJoinPredicate,
                                         qmgGraph          * aLeft,
                                         qmgGraph          * aRight)
{
    idBool  sIsDisk;
    SDouble sFilterCost = 1;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getInverseHash::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sIsDisk )
              != IDE_SUCCESS );

    if ( sIsDisk == ID_TRUE )
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Cost �� ���
    //------------------------------------------
    IDE_TEST( setJoinPredInfo4Hash( aStatement,
                                    aJoinPredicate,
                                    aJoinMethodCost )
              != IDE_SUCCESS );

    if ( aJoinMethodCost->joinPredicate != NULL )
    {
        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost =
                qmoCost::getInverseHashJoinCost( aLeft,
                                                 aRight,
                                                 sFilterCost,
                                                 aStatement->mSysStat,
                                                 sIsDisk,
                                                 &(aJoinMethodCost->accessCost),
                                                 &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
        //------------------------------------------
        // ���� ����� ���
        //------------------------------------------

        aJoinMethodCost->totalCost *= QCU_OPTIMIZER_HASH_JOIN_COST_ADJ;
        aJoinMethodCost->totalCost /= 100.0;

        qcgPlan::registerPlanProperty(aStatement,
                                      PLAN_PROPERTY_OPTIMIZER_HASH_JOIN_COST_ADJ);
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoJoinMethodMgr::getMerge( qcStatement       * aStatement,
                            qmoJoinMethodCost * aJoinMethodCost,
                            qmoPredicate      * aJoinPredicate,
                            qmgGraph          * aLeft,
                            qmgGraph          * aRight)
{
    idBool  sLeftIsDisk;
    idBool  sRightIsDisk;
    idBool  sLeftUseOrder;
    idBool  sRightUseOrder;
    SDouble sFilterCost = 1;

    qmoAccessMethod * sLeftAccessMethod;
    qmoAccessMethod * sRightAccessMethod;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getMerge::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL);
    IDE_DASSERT( aJoinMethodCost != NULL );
    IDE_DASSERT( aLeft != NULL );
    IDE_DASSERT( aRight != NULL );

    //------------------------------------------
    // ���� ��ü�� ����
    //------------------------------------------

    // Left ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aLeft, & sLeftIsDisk )
              != IDE_SUCCESS );

    if ( sLeftIsDisk == ID_TRUE )
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_DISK;
    }
    else
    {
        // Left Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_LEFT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_LEFT_STORAGE_MEMORY;
    }

    // Right ���� ��ü �Ǵ�
    IDE_TEST( qmg::isDiskTempTable( aRight, & sRightIsDisk )
              != IDE_SUCCESS );

    if ( sRightIsDisk == ID_TRUE )
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_DISK;
    }
    else
    {
        // Right Child�� ���� ��ü ����
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_RIGHT_STORAGE_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_RIGHT_STORAGE_MEMORY;
    }

    //------------------------------------------
    // Child �� ���� ����
    //------------------------------------------
    // Left Child�� ���� �Ǵ�

    IDE_TEST( setJoinPredInfo4Merge( aStatement,
                                     aJoinPredicate,
                                     aJoinMethodCost )
              != IDE_SUCCESS );

    if( aJoinMethodCost->joinPredicate != NULL )
    {
        aJoinMethodCost->leftIdxInfo  = NULL;
        aJoinMethodCost->rightIdxInfo = NULL;

        // preserved order ��� ���� ���� �Ǵ�
        IDE_TEST( usablePreservedOrder4Merge( aStatement,
                                              aJoinMethodCost->joinPredicate,
                                              aLeft,
                                              & sLeftAccessMethod,
                                              & sLeftUseOrder )
                  != IDE_SUCCESS );

        if ( sLeftUseOrder == ID_TRUE )
        {
            switch ( aLeft->type )
            {
                case QMG_SELECTION :
                case QMG_PARTITION:
                case QMG_SHARD_SELECT:     // PROJ-2638
                    if ( aLeft->myFrom->tableRef->view == NULL )
                    {
                        // �Ϲ� ���̺��� ���
                        // �ε����� �̿��� ���� �� Ŀ�� ������ �����ϴ�.
                        aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_NONE;

                        aJoinMethodCost->leftIdxInfo = sLeftAccessMethod->method;
                    }
                    else
                    {
                        // ���� ���
                        // ������ ����ǳ� �����Ͽ� ó���Ͽ��� ��.
                        aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_STORE;
                    }
                    break;

                case QMG_INNER_JOIN:
                    if ( ( ((qmgJOIN*)aLeft)->selectedJoinMethod->flag
                        & QMO_JOIN_METHOD_MASK ) ==
                        QMO_JOIN_METHOD_MERGE )
                    {
                        // ���� ������ ���Ͽ� ������ ����Ǵ� ����
                        // ������ ������ �ʿ����.
                        aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_NONE;
                    }
                    else
                    {
                        // ������ ����ǳ� �����Ͽ� ó���Ͽ��� ��.
                        aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_STORE;
                    }

                    break;
                default:
                    aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
                    aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_STORE;
                    break;
            }

            aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_TRUE;
        }
        else
        {
            aJoinMethodCost->flag &= ~QMO_JOIN_LEFT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_LEFT_NODE_SORTING;
        }

        // Right Child�� ���� �Ǵ�
        // preserved order ��� ���� ���� �Ǵ�
        IDE_TEST( usablePreservedOrder4Merge( aStatement,
                                              aJoinMethodCost->joinPredicate,
                                              aRight,
                                              & sRightAccessMethod,
                                              & sRightUseOrder )
                  != IDE_SUCCESS );

        if ( sRightUseOrder == ID_TRUE )
        {
            switch ( aRight->type )
            {
                case QMG_SELECTION:
                case QMG_PARTITION:
                case QMG_SHARD_SELECT:     // PROJ-2638
                    if ( aRight->myFrom->tableRef->view == NULL )
                    {
                        // �Ϲ� ���̺��� ���
                        // �ε����� �̿��� ���� �� Ŀ�� ������ �����ϴ�.
                        aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_NONE;

                        aJoinMethodCost->rightIdxInfo = sRightAccessMethod->method;
                    }
                    else
                    {
                        // ���� ���
                        // ������ ����ǳ� �����Ͽ� ó���Ͽ��� ��.
                        aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
                        aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_STORE;
                    }

                    break;
                default:
                    // Left �� �޸� Right�� Merge Join���� ���� ������
                    // ����Ǵ��� Merge Join Algorithm�� Ư����
                    // �����Ͽ� ó���Ͽ��� ��.
                    aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
                    aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_STORE;
                    break;
            }

            aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_TRUE;
        }
        else
        {
            aJoinMethodCost->flag &= ~QMO_JOIN_RIGHT_NODE_MASK;
            aJoinMethodCost->flag |= QMO_JOIN_RIGHT_NODE_SORTING;
        }

        //------------------------------------------
        // Cost �� ���
        //------------------------------------------
        QMO_FIRST_ROWS_SET(aLeft, aJoinMethodCost);

        sFilterCost = qmoCost::getFilterCost( aStatement->mSysStat,
                                              aJoinMethodCost->joinPredicate->predicate,
                                              (aLeft->costInfo.outputRecordCnt *
                                               aRight->costInfo.outputRecordCnt *
                                               aJoinMethodCost->selectivity) );

        aJoinMethodCost->totalCost = qmoCost::getMergeJoinCost(
                                                aJoinMethodCost,
                                                aLeft,
                                                aRight,
                                                sLeftAccessMethod,
                                                sRightAccessMethod,
                                                sFilterCost,
                                                aStatement->mSysStat,
                                                &(aJoinMethodCost->accessCost),
                                                &(aJoinMethodCost->diskCost) );

        QMO_FIRST_ROWS_UNSET(aLeft, aJoinMethodCost);

        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_TRUE;
    }
    else
    {
        aJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
        aJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmoJoinMethodMgr::initJoinMethodCost4NL( qcStatement             * aStatement,
                                                qmgGraph                * aGraph,
                                                qmoPredicate            * aJoinPredicate,
                                                qmoJoinLateralDirection   aLateralDirection,
                                                qmoJoinMethodCost      ** aMethod,
                                                UInt                    * aMethodCnt)
{
/***********************************************************************
 *
 * Description : Nested Loop Join�� Join Method Cost �ʱ� ����
 *
 * Implementation :
 *    (1) Join Method Cost ���� �� �ʱ�ȭ
 *    (2) flag ���� : join type, join direction, left right DISK/MEMORY
 *    (3) �� type�� feasibility �˻�
 *
 ***********************************************************************/

    qmoJoinMethodCost * sJoinMethodCost;
    UInt                sFirstTypeFlag   = 0;
    UInt                i;
    UInt                sMethodCnt       = 0;
    idBool              sIsDirectedJoin  = ID_FALSE;
    idBool              sIsUsable        = ID_FALSE;
    idBool              sIsInverseUsable = ID_FALSE;
    qmoPredicate      * sJoinPredicate   = NULL;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::initJoinMethodCost4NL::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL);
    IDE_DASSERT( aGraph != NULL );

    //--------------------------------------------------------
    //     Join Type  |
    //     ---------------------------------------------------
    //     Inner Join | full nested loop ( left->right )
    //       (6)      | full nested loop ( right->left )
    //                | full store nested loop ( left->right )
    //                | full store nested loop ( right->left )
    //                | index nested loop ( left->right )
    //                | index nested loop ( right->left )
    //     ---------------------------------------------
    //     Semi Join  | full nested loop ( left->right )
    //       (4)      | full store nested loop ( left->right )
    //                | index nested loop ( left->right )
    //                | inverse index nested loop ( right->left )
    //     ---------------------------------------------
    //     Anti Join  | full nested loop ( left->right )
    //     Left Outer | full store nested loop ( left->right )
    //       (3)      | index nested loop ( left->right )
    //     ---------------------------------------------
    //     Full Outer | full store nested loop ( left->right )
    //       (4)      | full store nested loop ( right->left )
    //                | anti outer ( left->right )
    //                | anti outer ( right->left )
    //---------------------------------------------------------

    //------------------------------------------
    // �� Join Type�� �´� Join Method Cost ���� �� flag ���� ����
    //------------------------------------------
    switch ( aGraph->type )
    {
        case QMG_INNER_JOIN :
            sMethodCnt = 6;
            sIsDirectedJoin = ID_FALSE;

            // ù��° Join Method Type
            sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
            sFirstTypeFlag |= QMO_JOIN_METHOD_FULL_NL;

            break;
        case QMG_SEMI_JOIN:
            sMethodCnt = 4;
            sIsDirectedJoin = ID_TRUE;

            sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
            sFirstTypeFlag |= QMO_JOIN_METHOD_FULL_NL;

            break;
        case QMG_ANTI_JOIN:
        case QMG_LEFT_OUTER_JOIN :
            sMethodCnt = 3;
            sIsDirectedJoin = ID_TRUE;

            sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
            sFirstTypeFlag |= QMO_JOIN_METHOD_FULL_NL;

            break;
        case QMG_FULL_OUTER_JOIN :
            sMethodCnt = 4;
            sIsDirectedJoin = ID_FALSE;

            sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
            sFirstTypeFlag |= QMO_JOIN_METHOD_FULL_STORE_NL;
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    // Join Method Cost �迭 ����
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinMethodCost ) *
                                             sMethodCnt,
                                             (void **)&sJoinMethodCost )
              != IDE_SUCCESS );

    // join method type, direction, left�� right child�� inter result type ����
    IDE_TEST( setFlagInfo( sJoinMethodCost,
                           sFirstTypeFlag,
                           sMethodCnt,
                           sIsDirectedJoin )
              != IDE_SUCCESS );

    // PROJ-2385 Inverse Index NL
    if( aGraph->type == QMG_SEMI_JOIN )
    {
        IDE_DASSERT( sMethodCnt == 4 );

        // sJoinMethodCost[0] : Full nested loop join       (LEFTRIGHT)
        // sJoinMethodCost[1] : Index nested loop join      (LEFTRIGHT)
        // sJoinMethodCost[2] : Full store nested loop join (LEFTRIGHT)
        // sJoinMethodCost[3] : Anti-Outer                  (LEFTRIGHT) << ���⸦ �Ʒ��� ���� �����Ѵ�.
        sJoinMethodCost[3].flag &= ~QMO_JOIN_METHOD_MASK;
        sJoinMethodCost[3].flag |= QMO_JOIN_METHOD_INVERSE_INDEX;
        sJoinMethodCost[3].flag &= ~QMO_JOIN_METHOD_DIRECTION_MASK;
        sJoinMethodCost[3].flag |= QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT;
    }
    else
    {
        // Nothing to do.
    }

    // full (store) nested loop join ��� ���� ���� ����
    IDE_TEST( usableJoinMethodFullNL( aGraph,
                                      aJoinPredicate,
                                      & sIsUsable )
              != IDE_SUCCESS );

    // PROJ-2385
    // inverse index nl ��� ���� ���� ����
    //  > Predicate�� Equal Operator�� ������ �ִٸ� HASH_JOINABLE
    // >> Predicate�� ��� HASH_JOINABLE �̸� Inverse Index NL ����
    sIsInverseUsable = ID_TRUE;

    for ( sJoinPredicate = aJoinPredicate;
          sJoinPredicate != NULL;
          sJoinPredicate = sJoinPredicate->next )
    {
        if ( ( sJoinPredicate->flag & QMO_PRED_HASH_JOINABLE_MASK )
                == QMO_PRED_HASH_JOINABLE_FALSE )
        {
            sIsInverseUsable = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    //------------------------------------------
    // �� Join Method Type�� direction�� ���� Join Method Cost ���� ����
    //    - selectivity ����
    //    - feasibility ����
    //      feasibility �ʱⰪ�� TRUE, feasibility�� ������ FALSE�� ������
    //------------------------------------------

    for ( i = 0; i < sMethodCnt; i++ )
    {
        sJoinMethodCost[i].selectivity     = aGraph->costInfo.selectivity;

        switch ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK )
        {
            case QMO_JOIN_METHOD_FULL_NL :
            case QMO_JOIN_METHOD_FULL_STORE_NL :

                //------------------------------------------
                // fix BUG-12580
                // left ������� ���ڵ� ī��Ʈ ������ �߸��� ���
                // right ��尡 join �Ǵ� view�� ���� ���ڵ尡 ���� ��쿡
                // ��û�� �������ϰ� �߻��ϰ� �ȴ�.
                // ����, hash �Ǵ� sort join���� ������ �� �ֵ��� �ϱ� ����
                // joinable predicate�� �ִ� ���,
                // full nested loop, full store nested loop join method��
                // �������� �ʴ´�.
                //------------------------------------------

                /**********************************************************
                 *
                 *  PROJ-2418
                 * 
                 *  Lateral View��, Lateral View�� �����ϴ� Relation��
                 *  Join�� Join Method�� �ݵ�� Full NL�� ���õǾ�� �Ѵ�.
                 *  �װ͵�, Lateral View�� Driven(RIGHT)�� ��ġ�ؾ� �Ѵ�.
                 * 
                 *  - Lateral View�� Driving(LEFT)�� �Ǹ�,
                 *    Driven Table�� Lateral View�� ��� ������ �����ؾ� �ϴµ�..
                 *    Lateral View�� Ž���� ������ �� ���� ����.
                 *
                 *  - Lateral View�� Driven�� ������ MTR Tuple�� ���̸�,
                 *    Driving Table�� Lateral View�� ��� ������ ������ ������
                 *    ���� MTR Tuple�� �װ� �ȴ�. Full NL���� �� ȿ���� ��������.
                 * 
                 *  ����, Full Store NL ���� ���⼭�� �����Ѵ�.
                 *  Lateral View�� Index NL�� ����� �� ���� ����.
                 *  �Ʒ��� Hash-based / Sort-based / Merge ���� ��� �����Ѵ�.
                 *
                **********************************************************/

                switch ( aLateralDirection )
                {
                    case QMO_JOIN_LATERAL_LEFT:
                        // Left����, Right�� �����ϴ� LView�� �����ϴ� ���

                        // FULL_NL�� ������ ��� �ϹǷ� sIsUsable�� TRUE�� �����Ѵ�.
                        sIsUsable = ID_TRUE;

                        // RIGHTLEFT Full NL�� ����� ��� ����
                        // RIGHTLEFT���, Left�� Lateral View�� Right ��ġ�� ����ȴ�.
                        if ( ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK ) 
                             != QMO_JOIN_METHOD_FULL_NL ) ||
                             ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_DIRECTION_MASK ) 
                             != QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT ) )
                        {
                            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                        break;
                    case QMO_JOIN_LATERAL_RIGHT:
                        // Right����, Left�� �����ϴ� LView�� �����ϴ� ���

                        // FULL_NL�� ������ ��� �ϹǷ� sIsUsable�� TRUE�� �����Ѵ�.
                        sIsUsable = ID_TRUE;

                        // LEFTRIGHT Full NL�� ����� ��� ����
                        if ( ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK ) 
                             != QMO_JOIN_METHOD_FULL_NL ) ||
                             ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_DIRECTION_MASK ) 
                             != QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT ) )
                        {
                            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                        else
                        {
                            // Nothing to do.
                        }
                        break;
                    case QMO_JOIN_LATERAL_NONE:
                        break;
                    default:
                        IDE_DASSERT(0);
                        break;
                }

                if( sIsUsable == ID_TRUE )
                {
                    // joinable predicate�� ���� ���
                    // �Ǵ� use_nl hint�� ����� ���
                    // �ܺ� ������ �ʿ�� �ϴ� Lateral View�� �ִ� ���

                    // Nothing To Do
                }
                else
                {
                    if( ( sJoinMethodCost[i].flag &
                          QMO_JOIN_METHOD_DIRECTION_MASK ) ==
                        QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
                    {
                        if ( ( aGraph->right->type == QMG_SELECTION ) ||
                             ( aGraph->right->type == QMG_SHARD_SELECT ) || // PROJ-2638
                             ( aGraph->right->type == QMG_PARTITION ) )
                        {
                            // BUG-43424 �����ʿ� view �� ������ push_pred ��Ʈ�� ���ÿ��� NL ������ ����մϴ�.
                            if( (aGraph->right->myFrom->tableRef->view != NULL) &&
                                (aGraph->myQuerySet->SFWGH->hints->pushPredHint == NULL) &&
                                ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
                                  == QMG_JOIN_ONLY_NL_FALSE ) )
                            {
                                sJoinMethodCost[i].flag &=
                                    ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                                sJoinMethodCost[i].flag |=
                                    QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                            }
                            else
                            {
                                // Nothing To Do
                            }
                        }
                        else
                        {
                            sJoinMethodCost[i].flag &=
                                ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |=
                                QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                    }
                    else
                    {
                        if ( ( aGraph->left->type == QMG_SELECTION ) ||
                             ( aGraph->left->type == QMG_SHARD_SELECT ) || // PROJ-2638
                             ( aGraph->left->type == QMG_PARTITION ) )
                        {
                            // BUG-43424 �����ʿ� view �� ������ push_pred ��Ʈ�� ���ÿ��� NL ������ ����մϴ�.
                            if( (aGraph->left->myFrom->tableRef->view != NULL) &&
                                (aGraph->myQuerySet->SFWGH->hints->pushPredHint == NULL) &&
                                ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
                                  == QMG_JOIN_ONLY_NL_FALSE ) )
                            {
                                sJoinMethodCost[i].flag &=
                                    ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                                sJoinMethodCost[i].flag |=
                                    QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                            }
                            else
                            {
                                // Nothing To Do
                            }
                        }
                        else
                        {
                            sJoinMethodCost[i].flag &=
                                ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |=
                                QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                    }
                }

                break;
            case QMO_JOIN_METHOD_INDEX :

                //------------------------------------------
                // feasibility :
                // (1) predicate�� ����
                // (2) right�� qmgSelection �� ���
                // (3) - Index Nested Loop Join Hint�� �����ϴ� ���
                //       Join Method Hint ���� ��, table access hint ����
                //     - �������� �ʴ� ���
                //       right�� Full Scan Hint�� ������, FALSE
                //------------------------------------------

                // PROJ-2418
                // Child Graph�� �ܺ� �����ϴ� ��Ȳ�̶��
                // Index NL�� ����� �� ����.
                if ( ( aJoinPredicate == NULL ) || 
                     ( aLateralDirection != QMO_JOIN_LATERAL_NONE ) )
                {
                    sJoinMethodCost[i].flag &=
                        ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                    sJoinMethodCost[i].flag |=
                        QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                }
                else
                {
                    // PROJ-1502 PARTITIONED DISK TABLE
                    // feasibility : right�� qmgSelection �Ǵ� qmgPartition �� ��쿡�� ����
                    // To Fix BUG-8804
                    if ( ( sJoinMethodCost[i].flag &
                           QMO_JOIN_METHOD_DIRECTION_MASK ) ==
                         QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
                    {
                        if ( ( aGraph->right->type == QMG_SELECTION ) ||
                             ( aGraph->right->type == QMG_SHARD_SELECT ) || // PROJ-2638
                             ( aGraph->right->type == QMG_PARTITION ) )
                        {
                            // nothing to do
                        }
                        else
                        {
                            sJoinMethodCost[i].flag &=
                                ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |=
                                QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                    }
                    else
                    {
                        if ( ( aGraph->left->type == QMG_SELECTION ) ||
                             ( aGraph->left->type == QMG_SHARD_SELECT ) || // PROJ-2638
                             ( aGraph->left->type == QMG_PARTITION ) )
                        {
                            // nothing to do
                        }
                        else
                        {
                            sJoinMethodCost[i].flag &=
                                ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |=
                                QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                    }
                }

                break;
            case QMO_JOIN_METHOD_INVERSE_INDEX :

                //------------------------------------------
                // PROJ-2385
                // index nested loop method�� �����ϰ� �˻�������
                // right�� �ƴ� left�� selection�̾�� �Ѵ�.
                //------------------------------------------

                if ( ( aJoinPredicate == NULL ) ||
                     ( sIsInverseUsable == ID_FALSE ) )
                {
                    sJoinMethodCost[i].flag &=
                        ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                    sJoinMethodCost[i].flag |=
                        QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                }
                else
                {
                    if ( ( aGraph->left->type == QMG_SELECTION ) ||
                         ( aGraph->left->type == QMG_SHARD_SELECT ) || // PROJ-2638
                         ( aGraph->left->type == QMG_PARTITION ) )
                    {
                        // nothing to do
                    }
                    else
                    {
                        sJoinMethodCost[i].flag &=
                            ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost[i].flag |=
                            QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                    }
                }

                break;
            case QMO_JOIN_METHOD_ANTI :

                //------------------------------------------
                // feasibility :
                //    (1) left, right ��� qmgSelection �� ���
                //    (2) join predicate�� �����ϴ� ���
                //------------------------------------------
                // TODO1502:
                // anti outer join�� ���� ����ؾ� ��.

                // PROJ-2418
                // Child Graph�� �ܺ� �����ϴ� ��Ȳ�̶��
                // Anti Outer�� ����� �� ����.
                if ( aLateralDirection != QMO_JOIN_LATERAL_NONE )
                {
                    sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                    sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                }
                else
                {
                    if ( ( (aGraph->left->type == QMG_SELECTION) ||
                           (aGraph->left->type == QMG_SHARD_SELECT ) || // PROJ-2638
                           (aGraph->left->type == QMG_PARTITION) )
                         &&
                         ( (aGraph->right->type == QMG_SELECTION) ||
                           (aGraph->right->type == QMG_SHARD_SELECT ) || // PROJ-2638
                           (aGraph->right->type == QMG_PARTITION) ) )
                    {
                        if ( aJoinPredicate != NULL )
                        {
                            // Nothing to do.
                        }
                        else
                        {
                            // To Fix BUG-8763
                            // Join Predicate�� ���� ���
                            sJoinMethodCost[i].flag &=
                                ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                            sJoinMethodCost[i].flag |=
                                QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                        }
                    }
                    else
                    {
                        sJoinMethodCost[i].flag &=
                            ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost[i].flag |=
                            QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                    }
                }
                break;
            default :
                IDE_DASSERT( 0 );
                break;
        }
    }

    // BUG-37407 semi, anti join cost
    for ( i = 0; i < sMethodCnt; i++ )
    {
        switch ( aGraph->type )
        {
            case QMG_SEMI_JOIN :
                sJoinMethodCost[i].flag &= ~QMO_JOIN_SEMI_MASK;
                sJoinMethodCost[i].flag |=  QMO_JOIN_SEMI_TRUE;
                break;

            case QMG_ANTI_JOIN :
                sJoinMethodCost[i].flag &= ~QMO_JOIN_ANTI_MASK;
                sJoinMethodCost[i].flag |=  QMO_JOIN_ANTI_TRUE;
                break;

            default :
                // nothing todo
                break;
        }
    }

    // BUG-40022
    if ( (QCU_OPTIMIZER_JOIN_DISABLE & QMO_JOIN_DISABLE_NL ) == QMO_JOIN_DISABLE_NL )
    {
        for ( i = 0; i < sMethodCnt; i++ )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }
    else
    {
        // Nothing To Do
    }

    if ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
         == QMG_JOIN_ONLY_NL_TRUE )
    {
        for ( i = 0; i < sMethodCnt; i++ )
        {
            if ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK )
                 == QMO_JOIN_METHOD_FULL_STORE_NL )
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                /* Nothing to do */
            }
        }
    }
    else
    {
        /* Nothing to do */
    }

    qcgPlan::registerPlanProperty( aStatement,
                                    PLAN_PROPERTY_OPTIMIZER_JOIN_DISABLE );

    // BUG-46932
    if ( ( QCU_OPTIMIZER_INVERSE_JOIN_ENABLE == 0 ) &&
         ( ( aGraph->type == QMG_SEMI_JOIN ) ) ) 
    {
        // recursive with�� ��� forceJoinOrder4RecursiveView���� ������ ������ ����
        if ( (aGraph->myQuerySet->lflag & QMV_QUERYSET_FROM_RECURSIVE_WITH_MASK) 
             == QMV_QUERYSET_FROM_RECURSIVE_WITH_FALSE )
        {
            // 3 : inverse hash
            sJoinMethodCost[3].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[3].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_INVERSE_JOIN_ENABLE );

    /* BUG-47786 Unnesting ��� ���� */
    if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_MASK )
         == QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_TRUE )
    {
        if ( aGraph->type == QMG_SEMI_JOIN )
        {
            // 3 : inverse hash
            sJoinMethodCost[3].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[3].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    *aMethodCnt = sMethodCnt;
    *aMethod = sJoinMethodCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qmoJoinMethodMgr::initJoinMethodCost4Hash( qcStatement             * aStatement,
                                                  qmgGraph                * aGraph,
                                                  qmoPredicate            * aJoinPredicate,
                                                  qmoJoinLateralDirection   aLateralDirection,
                                                  qmoJoinMethodCost      ** aMethod,
                                                  UInt                    * aMethodCnt )
{
/***********************************************************************
 *
 * Description : Hash Join�� join method cost ���� ����
 *
 * Implementation :
 *    (1) Join Method Cost ���� �� �ʱ�ȭ
 *    (2) flag ���� : join type, join direction, left right DISK/MEMORY
 *    (3) �� type�� feasibility �˻�
 *
 ***********************************************************************/

    qmoJoinMethodCost * sJoinMethodCost;
    UInt                sFirstTypeFlag;
    UInt                sMethodCnt      = 0;
    idBool              sIsDirectedJoin = ID_FALSE;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::initJoinMethodCost4NL::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL);
    IDE_DASSERT( aGraph != NULL );

    //--------------------------------------------------------
    //     Join Type  |
    //     ---------------------------------------------------
    //     Inner Join | one pass hash join ( left->right )
    //       (4)      | one pass hash join ( right->left )
    //                | two pass hash join ( left->right )
    //                | two pass hash join ( right->left )
    //     ---------------------------------------------
    //     Semi Join  | one pass hash join ( left->right )
    //     Anti Join  | two pass hash join ( left->right )
    //     Left Outer | inverse  hash join ( right->left )
    //       (3)      |
    //     ---------------------------------------------
    //     Full Outer | one pass hash join ( left->right )
    //       (4)      | one pass hash join ( right->left )
    //                | two pass hash join ( left->right )
    //                | two pass hash join ( right->left )
    //---------------------------------------------------------

    //------------------------------------------
    // �� Join Type�� �´� Join Method Cost ���� �� flag ���� ����
    //------------------------------------------

    sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
    sFirstTypeFlag |= QMO_JOIN_METHOD_ONE_PASS_HASH;

    switch ( aGraph->type )
    {
        case QMG_INNER_JOIN :
        case QMG_FULL_OUTER_JOIN :
            sMethodCnt = 4;
            sIsDirectedJoin = ID_FALSE;
            break;
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
        case QMG_LEFT_OUTER_JOIN :
            sMethodCnt = 3;
            sIsDirectedJoin = ID_TRUE;
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    // Join Method Cost �迭 ����
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinMethodCost ) *
                                             sMethodCnt,
                                             (void **)&sJoinMethodCost )
              != IDE_SUCCESS );

    // join method type, direction, left�� right child�� inter result type ����
    IDE_TEST( setFlagInfo( sJoinMethodCost,
                           sFirstTypeFlag,
                           sMethodCnt,
                           sIsDirectedJoin )
              != IDE_SUCCESS );

    /* PROJ-2339 : Revert the direction of INVERSE HASH */
    if( ( aGraph->type == QMG_SEMI_JOIN ) ||
        ( aGraph->type == QMG_ANTI_JOIN ) ||
        ( aGraph->type == QMG_LEFT_OUTER_JOIN ) )
    {
        IDE_DASSERT( sMethodCnt == 3 );

        // sJoinMethodCost[0] : One-Pass Hash Join (LEFTRIGHT)
        // sJoinMethodCost[1] : Two-Pass Hash Join (LEFTRIGHT)
        // sJoinMethodCost[2] : Inverse Hash Join  (LEFTRIGHT) << ���⸦ �Ʒ��� ���� �����Ѵ�.
        sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_DIRECTION_MASK;
        sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT;
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // �� Join Method Type�� direction�� ���� Join Method Cost ���� ����
    //    - selectivity ����
    //    - feasibility ����
    //------------------------------------------

    for ( i = 0; i < sMethodCnt; i++ )
    {
        sJoinMethodCost[i].selectivity     = aGraph->costInfo.selectivity;

        //------------------------------------------
        // feasibility  : predicate�� ������ ��쿡�� ����
        //------------------------------------------

        if ( aJoinPredicate == NULL )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // PROJ-2418
            // Child Graph�� �ܺ� �����ϴ� ��Ȳ�̶��
            // Hash-based Join Method�� ����� �� ����.
            if ( aLateralDirection != QMO_JOIN_LATERAL_NONE )
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // Nothing to do
            }

            /* BUG-41599 */
            if ((aJoinPredicate->node->lflag & QTC_NODE_COLUMN_RID_MASK)
                == QTC_NODE_COLUMN_RID_EXIST)
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( (aStatement->disableLeftStore == ID_TRUE) &&
             ((sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK) == QMO_JOIN_METHOD_TWO_PASS_HASH) )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // Nothing To Do
        }
    }

    // BUG-40022
    if ( (QCU_OPTIMIZER_JOIN_DISABLE & QMO_JOIN_DISABLE_HASH) == QMO_JOIN_DISABLE_HASH )
    {
        for ( i = 0; i < sMethodCnt; i++ )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }
    else
    {
        // Nothing To Do
    }

    qcgPlan::registerPlanProperty( aStatement,
                                    PLAN_PROPERTY_OPTIMIZER_JOIN_DISABLE );

    // BUG-46932
    if ( ( QCU_OPTIMIZER_INVERSE_JOIN_ENABLE == 0 ) &&
         ( ( aGraph->type == QMG_SEMI_JOIN ) || 
           ( aGraph->type == QMG_ANTI_JOIN ) ||
           ( aGraph->type == QMG_LEFT_OUTER_JOIN ) ) )
    {
        // recursive with�� ��� forceJoinOrder4RecursiveView���� ������ ������ ����
        if ( (aGraph->myQuerySet->lflag & QMV_QUERYSET_FROM_RECURSIVE_WITH_MASK) 
             == QMV_QUERYSET_FROM_RECURSIVE_WITH_FALSE )
        {
            // 2 : inverse hash
            sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_INVERSE_JOIN_ENABLE );

    /* BUG-47786 Unnesting ��� ���� */
    if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_MASK )
         == QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_TRUE )
    {
        if ( ( aGraph->type == QMG_SEMI_JOIN ) ||
             ( aGraph->type == QMG_ANTI_JOIN ) )
        {
            // 2 : inverse hash
            sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    *aMethodCnt = sMethodCnt;
    *aMethod = sJoinMethodCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoJoinMethodMgr::initJoinMethodCost4Sort( qcStatement             * aStatement,
                                                  qmgGraph                * aGraph,
                                                  qmoPredicate            * aJoinPredicate,
                                                  qmoJoinLateralDirection   aLateralDirection,
                                                  qmoJoinMethodCost      ** aMethod,
                                                  UInt                    * aMethodCnt )
{
/***********************************************************************
 *
 * Description : Sort Join�� Join Method Cost �ʱ� ����
 *
 * Implementation :
 *    (1) Join Method Cost ���� �� �ʱ�ȭ
 *    (2) flag ���� : join type, join direction, left right DISK/MEMORY
 *    (3) �� type�� feasibility �˻�
 *
 ***********************************************************************/

    qmoJoinMethodCost * sJoinMethodCost;
    UInt                sFirstTypeFlag;
    UInt                sMethodCnt      = 0;
    idBool              sIsDirectedJoin = ID_FALSE;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::initJoinMethodCost4Sort::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL);
    IDE_DASSERT( aGraph != NULL );

    //--------------------------------------------------------
    //     Join Type  |
    //     ---------------------------------------------------
    //     Inner Join | one pass sort join ( left->right )
    //       (4)      | one pass sort join ( right->left )
    //                | two pass sort join ( left->right )
    //                | two pass sort join ( right->left )
    //     ---------------------------------------------
    //     Semi Join  | one pass sort join ( left->right )
    //     Anti Join  | two pass sort join ( left->right )
    //       (3)      | inverse  sort join ( right->left )
    //                |
    //     ---------------------------------------------
    //     Left Outer | one pass sort join ( left->right )
    //       (2)      | two pass sort join ( left->right )
    //     ---------------------------------------------
    //     Full Outer | one pass sort join ( left->right )
    //       (4)      | one pass sort join ( right->left )
    //                | two pass sort join ( left->right )
    //                | two pass sort join ( right->left )
    //---------------------------------------------------------

    //------------------------------------------
    // �� Join Type�� �´� Join Method Cost ���� �� flag ���� ����
    //    - �����Ǵ� flag ���� : joinMethodType,
    //                           direction,
    //                           left right disk/memory
    //------------------------------------------

    //------------------------------------------
    // joinMethodType, direction ���� ����
    //------------------------------------------

    sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
    sFirstTypeFlag |= QMO_JOIN_METHOD_ONE_PASS_SORT;

    switch ( aGraph->type )
    {
        case QMG_INNER_JOIN :
        case QMG_FULL_OUTER_JOIN :
            sMethodCnt = 4;
            sIsDirectedJoin = ID_FALSE;
            break;
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
            sMethodCnt = 3;
            sIsDirectedJoin = ID_TRUE;
            break;
        case QMG_LEFT_OUTER_JOIN :
            sMethodCnt = 2;
            sIsDirectedJoin = ID_TRUE;
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    // Join Method Cost �迭 ����
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinMethodCost ) *
                                                sMethodCnt,
                                                (void **)&sJoinMethodCost )
                 != IDE_SUCCESS );

    // join method type, direction, left�� right child�� inter result type ����
    IDE_TEST( setFlagInfo( sJoinMethodCost,
                              sFirstTypeFlag,
                              sMethodCnt,
                              sIsDirectedJoin )
                 != IDE_SUCCESS );

    /* PROJ-2385 : Revert the direction of INVERSE SORT */
    if ( ( aGraph->type == QMG_SEMI_JOIN ) ||
         ( aGraph->type == QMG_ANTI_JOIN ) )
    {
        IDE_DASSERT( sMethodCnt == 3 );

        // sJoinMethodCost[0] : One-Pass Sort Join (LEFTRIGHT)
        // sJoinMethodCost[1] : Two-Pass Sort Join (LEFTRIGHT)
        // sJoinMethodCost[2] : Inverse  Sort Join (LEFTRIGHT) << ���⸦ �Ʒ��� ���� �����Ѵ�.
        sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_DIRECTION_MASK;
        sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT;
    }
    else
    {
        // Nothing to do.
    }

    //------------------------------------------
    // �� Join Method Type�� direction�� ���� Join Method Cost ���� ����
    //    - selectivity ����
    //    - feasibility ����
    //------------------------------------------

    for ( i = 0; i < sMethodCnt; i++ )
    {
        sJoinMethodCost[i].selectivity     = aGraph->costInfo.selectivity;

        //------------------------------------------
        // feasibility  : predicate�� ������ ��쿡�� ����
        //------------------------------------------

        if ( aJoinPredicate == NULL )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // PROJ-2418
            // Child Graph�� �ܺ� �����ϴ� ��Ȳ�̶��
            // Sort-based Join Method�� ����� �� ����.
            if ( aLateralDirection != QMO_JOIN_LATERAL_NONE )
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // Nothing To Do
            }

            /* BUG-41599 */
            if ((aJoinPredicate->node->lflag & QTC_NODE_COLUMN_RID_MASK)
                == QTC_NODE_COLUMN_RID_EXIST)
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( (aStatement->disableLeftStore == ID_TRUE) &&
             ((sJoinMethodCost[i].flag & QMO_JOIN_METHOD_MASK) == QMO_JOIN_METHOD_TWO_PASS_SORT) )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // Nothing To Do
        }
    }

    // BUG-40022
    if ( (QCU_OPTIMIZER_JOIN_DISABLE & QMO_JOIN_DISABLE_SORT) == QMO_JOIN_DISABLE_SORT )
    {
        for ( i = 0; i < sMethodCnt; i++ )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }
    else
    {
        // Nothing To Do
    }

    qcgPlan::registerPlanProperty( aStatement,
                                    PLAN_PROPERTY_OPTIMIZER_JOIN_DISABLE );

    // BUG-46932
    if ( ( QCU_OPTIMIZER_INVERSE_JOIN_ENABLE == 0 ) &&
         ( ( aGraph->type == QMG_SEMI_JOIN ) || 
           ( aGraph->type == QMG_ANTI_JOIN ) ) )
    {
        // recursive with�� ��� forceJoinOrder4RecursiveView���� ������ ������ ����
        if ( (aGraph->myQuerySet->lflag & QMV_QUERYSET_FROM_RECURSIVE_WITH_MASK) 
             == QMV_QUERYSET_FROM_RECURSIVE_WITH_FALSE )
        {
            // 2 : inverse sort
            sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    qcgPlan::registerPlanProperty( aStatement,
                                   PLAN_PROPERTY_OPTIMIZER_INVERSE_JOIN_ENABLE );

    /* BUG-47786 Unnesting ��� ���� */
    if ( ( QC_SHARED_TMPLATE(aStatement)->flag & QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_MASK )
         == QC_TMP_UNNEST_INVERSE_JOIN_DISABLE_TRUE )
    {
        if ( ( aGraph->type == QMG_SEMI_JOIN ) ||
             ( aGraph->type == QMG_ANTI_JOIN ) )
        {
            // 2 : inverse sort
            sJoinMethodCost[2].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[2].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
    }

    *aMethodCnt = sMethodCnt;
    *aMethod = sJoinMethodCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoJoinMethodMgr::initJoinMethodCost4Merge(qcStatement             * aStatement,
                                                  qmgGraph                * aGraph,
                                                  qmoPredicate            * aJoinPredicate,
                                                  qmoJoinLateralDirection   aLateralDirection,
                                                  qmoJoinMethodCost      ** aMethod,
                                                  UInt                    * aMethodCnt )
{
/***********************************************************************
 *
 * Description : Merge Join�� Join Method Cost �ʱ� ����
 *
 * Implementation :
 *    (1) joinMethodType, direction ���� ����
 *    (2) feasibility �˻�
 *
 ***********************************************************************/

    UInt                sFirstTypeFlag;
    qmoJoinMethodCost * sJoinMethodCost;
    UInt                sMethodCnt          = 0;
    UInt                i;
    idBool              sIsDirectedJoin     = ID_FALSE;

    qmgGraph          * sRightChildGraph;
    qmgJOIN           * sRightJoinGraph;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::initJoinMethodCost4Merge::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL);
    IDE_DASSERT( aGraph != NULL );

    //--------------------------------------------------------
    //     Join Type  |
    //     ---------------------------------------------------
    //     Merge Join | merge join ( left->right )
    //       (2)      | merge join ( right->left )
    //--------------------------------------------------------

    //------------------------------------------
    // Inner Join �� Merge Join
    //------------------------------------------

    switch ( aGraph->type )
    {
        case QMG_INNER_JOIN :
            sMethodCnt = 2;
            sIsDirectedJoin = ID_FALSE;
            break;
        case QMG_SEMI_JOIN:
        case QMG_ANTI_JOIN:
            sMethodCnt = 1;
            sIsDirectedJoin = ID_TRUE;
            break;
        default :
            IDE_FT_ASSERT( 0 );
            break;
    }

    // To Fix PR-7989
    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmoJoinMethodCost ) *
                                             sMethodCnt,
                                             (void **)&sJoinMethodCost )
              != IDE_SUCCESS );

    // Join Method Type�� Join Direction ����
    sFirstTypeFlag = QMO_JOIN_METHOD_FLAG_CLEAR;
    sFirstTypeFlag |= QMO_JOIN_METHOD_MERGE;

    IDE_TEST( setFlagInfo( sJoinMethodCost,
                           sFirstTypeFlag,
                           sMethodCnt,
                           sIsDirectedJoin ) // is left outer
              != IDE_SUCCESS );

    //------------------------------------------
    // �� Join Method Type�� direction�� ���� Join Method Cost ���� ����
    //    - selectivity ����
    //    - feasibility ����
    //------------------------------------------

    for ( i = 0; i < sMethodCnt; i++ )
    {
        sJoinMethodCost[i].selectivity     = aGraph->costInfo.selectivity;

        // feasibility ����
        if ( aJoinPredicate == NULL )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // PROJ-2418
            if ( aLateralDirection != QMO_JOIN_LATERAL_NONE )
            {
                // Child Graph�� �ܺ� �����ϴ� ��Ȳ�̶��
                // Merge Join Method�� ���õ� �� ����.
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                if ( ( sJoinMethodCost[i].flag & QMO_JOIN_METHOD_DIRECTION_MASK )
                     == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
                {
                    sRightChildGraph = aGraph->right;
                }
                else
                {
                    sRightChildGraph = aGraph->left;
                }

                // To Fix PR-11944
                // Right Child�� Merge Join�� ���
                // Store Cursor�� �ϰ����� ������� �����Ƿ� ����� �� ����.
                if ( sRightChildGraph->type == QMG_INNER_JOIN )
                {
                    sRightJoinGraph = (qmgJOIN *) sRightChildGraph;
                    if ( ( sRightJoinGraph->selectedJoinMethod->flag &
                           QMO_JOIN_METHOD_MASK ) == QMO_JOIN_METHOD_MERGE )
                    {
                        sJoinMethodCost[i].flag &=
                            ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost[i].flag |=
                            QMO_JOIN_METHOD_FEASIBILITY_FALSE;
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

            /* BUG-41599 */
            if ((aJoinPredicate->node->lflag & QTC_NODE_COLUMN_RID_MASK)
                == QTC_NODE_COLUMN_RID_EXIST)
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( aStatement->disableLeftStore == ID_TRUE )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }
        else
        {
            // Nothing To Do
        }
    }

    /* TASK-6744 */
    if ( (QCU_PLAN_RANDOM_SEED != 0) &&
         (aStatement->mRandomPlanInfo != NULL) &&
         (smiGetStartupPhase() == SMI_STARTUP_SERVICE) )
    {
        for ( i = 0; i < sMethodCnt; i++ )
        {
            sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
            sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
        }

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED );
    }
    else
    {
        // BUG-40022
        if ( (QCU_OPTIMIZER_JOIN_DISABLE & QMO_JOIN_DISABLE_MERGE) == QMO_JOIN_DISABLE_MERGE )
        {
            for ( i = 0; i < sMethodCnt; i++ )
            {
                sJoinMethodCost[i].flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost[i].flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
        }
        else
        {
            // Nothing To Do
        }

        qcgPlan::registerPlanProperty( aStatement,
                                       PLAN_PROPERTY_OPTIMIZER_JOIN_DISABLE );
    }

    *aMethodCnt = sMethodCnt;
    *aMethod = sJoinMethodCost;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setFlagInfo( qmoJoinMethodCost * aJoinMethodCost,
                               UInt                aFirstTypeFlag,
                               UInt                aJoinMethodCnt,
                               idBool              aIsDirected )
{
/***********************************************************************
 *
 * Description : Join Method Type �� Direction ������ ����
 *
 * Implementation :
 *    (1) joinMethodType ���� ����
 *    (2) direction ���� ����
 *
 ***********************************************************************/

    UInt                sJoinMethodTypeFlag;
    qmoJoinMethodCost * sCurJoinMethodCost;
    qmoJoinMethodCost * sNextJoinMethodCost;
    UInt                i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setFlagInfo::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethodCost != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinMethodTypeFlag = aFirstTypeFlag;

    for ( i = 0; i < aJoinMethodCnt; i++ )
    {
        sCurJoinMethodCost = & aJoinMethodCost[i];

        // Join Method Cost �ʱ�ȭ
        sCurJoinMethodCost->selectivity = 0;
        sCurJoinMethodCost->joinPredicate = NULL;
        sCurJoinMethodCost->accessCost = 0;
        sCurJoinMethodCost->diskCost = 0;
        sCurJoinMethodCost->totalCost = 0;
        sCurJoinMethodCost->rightIdxInfo = NULL;
        sCurJoinMethodCost->leftIdxInfo = NULL;
        sCurJoinMethodCost->hashTmpTblCntHint = QMS_NOT_DEFINED_TEMP_TABLE_CNT;

        // flag ���� �ʱ�ȭ
        if ( aIsDirected == ID_FALSE )
        {
            //------------------------------------------
            // ���⼺�� �ִ� join�� �ƴ� ���
            // left->right �� right->left ���� ��� ����
            //------------------------------------------

            if ( i % 2 == 0 )
            {
                //------------------------------------------
                // left->right
                //------------------------------------------

                // join method type ����
                sCurJoinMethodCost->flag = QMO_JOIN_METHOD_FLAG_CLEAR;
                sCurJoinMethodCost->flag |= sJoinMethodTypeFlag;

                sNextJoinMethodCost = & aJoinMethodCost[i+1];
                sNextJoinMethodCost->flag = QMO_JOIN_METHOD_FLAG_CLEAR;
                sNextJoinMethodCost->flag |= sJoinMethodTypeFlag;

                // direction ���� ����
                sCurJoinMethodCost->flag &= ~QMO_JOIN_METHOD_DIRECTION_MASK;
                sCurJoinMethodCost->flag |=
                    QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT;
            }
            else
            {
                //------------------------------------------
                // right->left
                //------------------------------------------

                sJoinMethodTypeFlag = sJoinMethodTypeFlag << 1;

                // direction ���� ����
                sCurJoinMethodCost->flag &= ~QMO_JOIN_METHOD_DIRECTION_MASK;
                sCurJoinMethodCost->flag |=
                    QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT;
            }
        }
        else
        {
            //------------------------------------------
            // ���⼺�� �ִ� join�� ���
            // left->right ���⸸�� ����
            //------------------------------------------

            // join method type ����
            sCurJoinMethodCost->flag = QMO_JOIN_METHOD_FLAG_CLEAR;
            sCurJoinMethodCost->flag |= sJoinMethodTypeFlag;

            sJoinMethodTypeFlag = sJoinMethodTypeFlag << 1;
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::usablePreservedOrder4Merge( qcStatement    * aStatement,
                                              qmoPredInfo    * aJoinPred,
                                              qmgGraph       * aGraph,
                                              qmoAccessMethod ** aAccessMethod,
                                              idBool         * aUsable )
{
/***********************************************************************
 *
 * Description :  Preserver Order ��� ���� ����
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool              sUsable;
    qmgPreservedOrder * sPreservedOrder;
    qtcNode           * sCompareNode;
    qtcNode           * sColumnNode;
    qtcNode           * sJoinColumnNode;
    qcmIndex          * sIndex;
    qmoAccessMethod   * sOrgMethod;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::usablePreservedOrder4Merge::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement != NULL );
    IDE_DASSERT( aJoinPred != NULL );
    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aAccessMethod != NULL );
    IDE_DASSERT( aUsable != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sUsable = ID_FALSE;

    //------------------------------------------
    // preserved order ��� ���� ���� �˻�
    //------------------------------------------
    sJoinColumnNode =  qmoPred::getColumnNodeOfJoinPred(
        aStatement,
        aJoinPred->predicate,
        & aGraph->depInfo );

    if ( aGraph->preservedOrder != NULL )
    {
        // Preserved Order�� �ִ� ���

        if( sJoinColumnNode != NULL )
        {
            if( ( sJoinColumnNode->node.table ==
                  aGraph->preservedOrder->table ) &&
                ( sJoinColumnNode->node.column ==
                  aGraph->preservedOrder->column ) )
            {
                // BUG-21195
                // direction�� ASC�� ���� NOT DEFINED�� ��츦 ���� �и��� �ʿ䰡 ����.
                // �������� ���� �и��� �Ǿ� �־���,
                // ASC�� ��쿡 aAccessMethod�� �������� �ʾ� segment fault ������ �߻��߾���.
                if ( ( aGraph->preservedOrder->direction == QMG_DIRECTION_ASC ) ||
                     ( aGraph->preservedOrder->direction == QMG_DIRECTION_NOT_DEFINED ) )
                {
                    IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                               (void**) & sPreservedOrder )
                              != IDE_SUCCESS );

                    sPreservedOrder->table = aGraph->preservedOrder->table;
                    sPreservedOrder->column = aGraph->preservedOrder->column;
                    sPreservedOrder->direction = QMG_DIRECTION_ASC;

                    // To Fix PR-7989
                    sPreservedOrder->next = NULL;

                    IDE_TEST( qmg::checkUsableOrder( aStatement,
                                                     sPreservedOrder,
                                                     aGraph,
                                                     & sOrgMethod,
                                                     aAccessMethod,
                                                     & sUsable )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do
                }
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // Preserved Order�� ���� ���,
        // Preserved Order ��� �������� �˻�

        // To Fix PR-8023
        //------------------------------
        // Column Node�� ����
        //------------------------------

        sCompareNode = aJoinPred->predicate->node;

        if( ( sCompareNode->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            // CNF�� ���
            sCompareNode = (qtcNode *) sCompareNode->node.arguments;
        }
        else
        {
            // DNF�� ���� �� ��������.
            // Nothing To Do
        }

        if ( sCompareNode->indexArgument == 0 )
        {
            sColumnNode = (qtcNode*) sCompareNode->node.arguments;
        }
        else
        {
            sColumnNode = (qtcNode*) sCompareNode->node.arguments->next;
        }

        //------------------------------
        // Want Order�� ����
        //------------------------------

        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF( qmgPreservedOrder ),
                                                 (void**) & sPreservedOrder )
                  != IDE_SUCCESS );

        sPreservedOrder->table = sColumnNode->node.table;
        sPreservedOrder->column = sColumnNode->node.column;
        sPreservedOrder->direction = QMG_DIRECTION_ASC;

        // To Fix PR-7989
        sPreservedOrder->next = NULL;

        IDE_TEST( qmg::checkUsableOrder( aStatement,
                                         sPreservedOrder,
                                         aGraph,
                                         & sOrgMethod,
                                         aAccessMethod,
                                         & sUsable )
                  != IDE_SUCCESS );
    }

    // BUG-38118 merge join ��� ����
    // merge join �� ��� ������ asc ������ ���ĵǾ�� �Ѵ�.
    // ���� index �� QMNC_SCAN_TRAVERSE_FORWARD ������� �о�� �Ѵ�.
    // index �� QMNC_SCAN_TRAVERSE_BACKWARD ������� ������ null ���� ���� ����� Ʋ������.
    if ( sUsable == ID_TRUE )
    {
        //----------------------------------
        // ù��° Column�� ASC �˻�.
        //----------------------------------

        if( *aAccessMethod != NULL )
        {
            sIndex  = (*aAccessMethod)->method->index;

            // index desc ��Ʈ�� �ִ°�� QMNC_SCAN_TRAVERSE_BACKWARD ������� �д´�.
            // QMNC_SCAN_TRAVERSE_BACKWARD ������δ� merge ������ �ؼ��� �ȵȴ�.
            if( ((*aAccessMethod)->method->flag & QMO_STAT_CARD_IDX_HINT_MASK)
                == QMO_STAT_CARD_IDX_INDEX_DESC )
            {
                sUsable = ID_FALSE;
            }
            else
            {
                // nothing to do
            }

            // index �÷��� desc ���϶� index�� �ݴ�� �о �䱸�� asc ������� ������ �ϴ°��̴�.
            // QMNC_SCAN_TRAVERSE_BACKWARD ������δ� merge ������ �ؼ��� �ȵȴ�.
            if ( (sIndex->keyColsFlag[0] & SMI_COLUMN_ORDER_MASK)
                == SMI_COLUMN_ORDER_DESCENDING )
            {
                sUsable = ID_FALSE;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // nothing to do
        }
    }
    else
    {
        // nothing to do
    }

    *aUsable = sUsable;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setJoinMethodHint( qmoJoinMethod      * aJoinMethod,
                                     qmsJoinMethodHints * aJoinMethodHints,
                                     qmgGraph           * aGraph,
                                     UInt                 aNoUseHintMask,
                                     qmoJoinMethodCost ** aSameTableOrder,
                                     qmoJoinMethodCost ** aDiffTableOrder )
{
/***********************************************************************
 *
 * Description : Join Method Hint�� �����ϴ� Join Method Cost�� �߿���
 *               ���� cost�� ���� ���� ã�Ƴ�
 *
 * Implementation :
 *
 ***********************************************************************/

    qmoJoinMethodCost * sJoinMethodCost = NULL;
    qmoJoinMethodCost * sSameTableOrder = NULL;
    qmoJoinMethodCost * sDiffTableOrder = NULL;
    qcDepInfo           sDependencies;
    idBool              sIsSameTableOrder = ID_FALSE;
    UInt i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinMethodHint::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aJoinMethod != NULL );
    IDE_DASSERT( aJoinMethodHints != NULL );
    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // Hint�� ������ Join Method Type�� ������ Join Method Cost�� �߿���
    // Cost�� ���� ���� ���� ����
    //------------------------------------------

    for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
    {
        sJoinMethodCost = &(aJoinMethod->joinMethodCost[i]);

        // BUG-42413 NO_USE_ hint ����
        // NO_USE ��Ʈ skip
        if ( aJoinMethodHints->isNoUse == ID_TRUE )
        {
            // NO_USE�� ���� ���� �޼ҵ� skip
            if ( ( (sJoinMethodCost->flag & QMO_JOIN_METHOD_MASK) &
                   (aNoUseHintMask & QMO_JOIN_METHOD_MASK) ) != 0 )
            {
                continue;
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            // USE ��Ʈ�� �ٸ� ���� �޼ҵ� skip
            if ( ( (sJoinMethodCost->flag & QMO_JOIN_METHOD_MASK) &
                   (aJoinMethodHints->flag & QMO_JOIN_METHOD_MASK) ) == 0 )
            {
                continue;
            }
            else
            {
                // nothing to do
            }
        }

        // feasibility�� ���� ���
        if ( (sJoinMethodCost->flag & QMO_JOIN_METHOD_FEASIBILITY_MASK) ==
              QMO_JOIN_METHOD_FEASIBILITY_FALSE )
        {
            continue;
        }
        else
        {
            // nothing to do
        }

        //------------------------------------------
        // feasility�� TRUE ���,
        // Table Order���� ������ Join Method ���� �˻�
        //------------------------------------------

        // To Fix PR-13145
        // Two Pass Hash Join�� ��� Temp Table�� ���� ��Ʈ��
        // ������Ѿ� �Ѵ�.
        // Temp Table Count�� Two Pass Hash Join������ ���� ����
        // �ٸ� Join Method�� 0���� �����Ƿ�,
        // Two Pass Hash Join�� �ٸ� Join Method�� ������ �ʿ�� ����.
        sJoinMethodCost->hashTmpTblCntHint = aJoinMethodHints->tempTableCnt;

        if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
               == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
        {
            qtc::dependencySetWithDep( & sDependencies,
                                       & aGraph->left->depInfo );
        }
        else
        {
            qtc::dependencySetWithDep( & sDependencies,
                                       & aGraph->right->depInfo );
        }

        /* PROJ-2339 Inverse Hash Join + PROJ-2385
            *
            *  Hint�� ������ ��, Join Method ������ ������ ���� �̷������.
            *  - Hint�� ���ϴ� (Driven, Driving ������ �̾�����) Order���� ��ġ�ϴ� Method
            *  - Hint�� ���ϴ� Method����, Order�� ��ġ���� �ʴ� Method
            *  >>> Order���� ��ġ�ϴ� Method�� �ִٸ�, �� �߿��� Method�� �����Ѵ�.
            *  >>> Order���� ��ġ�ϴ� Method�� ���� ���,
            *      Order�� ��ġ���� ������ ��Ʈ�� ���ϴ� Method�� �����Ѵ�.
            *
            *  �� ����� Inner Join / Outer Join������ ������ ����. ������,
            *  Semi/Anti Join������ Inverse Join Method �߰��� �Ʒ��� ���� ������ �����Ѵ�.
            *
            *  Inverse�� Non-Inverse Join Method ��θ� ����Ѵٰ� ��������.
            *  Hint�� ���ϴ� Order�� ��ġ�ϴ� Method�� Non-Inverse Join �߿��� �ϳ��� ���õȴ�.
            *  Inverse Join Method�� Cost�� Order�� ��ġ���� �ʴ� Method�� �νĵȴ�.
            *  �� ��, Inverse Join Method�� Cost�� �� ������ ���õǴ� ������ �߻��Ѵ�.
            *
            *  ����, Inverse�� Non-Inverse Join Method ��θ� ����ϴ� ��쿡��
            *  (INVERSE_JOIN�̳� NO_INVERSE_JOIN�� ������ ���� ���, �� isUndirected == TRUE)
            *  Inverse Join Method�� 'Order�� ��ġ�ϴ� Method'�� ��ó�� ���Խ�Ų��.
            *
            */
        if ( aJoinMethodHints->isUndirected == ID_TRUE )
        {
            sIsSameTableOrder = ID_TRUE;
        }
        else
        {
            if ( qtc::dependencyContains( & sDependencies,
                                          & aJoinMethodHints->joinTables->table->depInfo )
                 == ID_TRUE )
            {
                sIsSameTableOrder = ID_TRUE;
            }
            else
            {
                sIsSameTableOrder = ID_FALSE;
            }
        }

        if ( sIsSameTableOrder == ID_TRUE )
        {
            //------------------------------------------
            // table order�� ���� ���,
            // �̹� ���õ� Join Method�� ���Ͽ� cost�� ���� �� ����
            //------------------------------------------

            if ( sSameTableOrder == NULL )
            {
                sSameTableOrder = sJoinMethodCost;
            }
            else
            {
                /* BUG-40589 floating point calculation */
                if (QMO_COST_IS_GREATER(sSameTableOrder->totalCost,
                                        sJoinMethodCost->totalCost)
                    == ID_TRUE)
                {
                    sSameTableOrder = sJoinMethodCost;
                }
                else
                {
                    // nothing to do
                }
            }
            sDiffTableOrder = NULL;
        }
        else
        {
            //------------------------------------------
            // table order�� �ٸ� ���
            // ������ ���õ� Join Method�� ���Ͽ� cost�� ���� �� ����
            //------------------------------------------

            if ( sSameTableOrder == NULL )
            {
                // Table Order ���� ������ Join Method�� ��������
                // ���õ��� ���� ���
                // ������ ���õ� Join Method�� cost �� �� ������ ����
                if ( sDiffTableOrder == NULL )
                {
                    sDiffTableOrder = sJoinMethodCost;
                }
                else
                {
                    /* BUG-40589 floating point calculation */
                    if (QMO_COST_IS_GREATER(sDiffTableOrder->totalCost,
                                            sJoinMethodCost->totalCost)
                        == ID_TRUE)
                    {
                        sDiffTableOrder = sJoinMethodCost;
                    }
                    else
                    {
                        // nothing to do
                    }
                }
            }
            else
            {
                // nothing to do
            }
        }
    }

    *aSameTableOrder = sSameTableOrder;
    *aDiffTableOrder = sDiffTableOrder;

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::canUsePreservedOrder( qcStatement       * aStatement,
                                        qmoJoinMethodCost * aJoinMethodCost,
                                        qmgGraph          * aGraph,
                                        qmoJoinChild        aChildPosition,
                                        idBool            * aUsable )
{
/***********************************************************************
 *
 * Description :
 *
 *    Sort Join���� ���Ͽ� Preserved Order�� ��� �������� �˻�
 *
 * Implementation :
 *
 ***********************************************************************/

    idBool sUsable;
    qtcNode* sJoinColumnNode;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::canUsePreservedOrder::__FT__" );

    if ( aGraph->preservedOrder != NULL )
    {
        sJoinColumnNode = qmoPred::getColumnNodeOfJoinPred(
            aStatement,
            aJoinMethodCost->joinPredicate->predicate,
            & aGraph->depInfo );

        if( sJoinColumnNode != NULL )
        {
            if( ( sJoinColumnNode->node.table ==
                  aGraph->preservedOrder->table ) &&
                ( sJoinColumnNode->node.column ==
                  aGraph->preservedOrder->column ) )
            {
                switch ( aChildPosition )
                {
                    case QMO_JOIN_CHILD_LEFT:

                        aJoinMethodCost->flag &=
                            ~QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_MASK;
                        aJoinMethodCost->flag |=
                            QMO_JOIN_METHOD_USE_LEFT_PRES_ORDER_TRUE;
                        break;

                    case QMO_JOIN_CHILD_RIGHT:
                        aJoinMethodCost->flag &=
                            ~QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_MASK;
                        aJoinMethodCost->flag |=
                            QMO_JOIN_METHOD_USE_RIGHT_PRES_ORDER_TRUE;
                        break;

                    default:
                        IDE_FT_ASSERT( 0 );
                        break;
                }
                sUsable = ID_TRUE;
            }
            else
            {
                sUsable = ID_FALSE;
            }
        }
        else
        {
            sUsable = ID_FALSE;
        }
    }
    else
    {
        sUsable = ID_FALSE;
    }

    *aUsable = sUsable;

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::usableJoinMethodFullNL( qmgGraph      * aGraph,
                                          qmoPredicate  * aJoinPredicate,
                                          idBool        * aIsUsable )
{
/***********************************************************************
 *
 * Description : full nested loop, full store nested loop join��
 *               ��뿩�θ� �Ǵ�
 *
 * Implementation :
 *
 *   join �����
 *   left ������� ���ڵ� ī��Ʈ ������ �߸��� ���,
 *   right ��尡 join �Ǵ� view�� ���� ���ڵ尡 ���� ���,
 *   �������ϰ� �߻��ϰ� �Ǿ�
 *   full (store) nested loop join�� feasibility�� üũ�ϰ� �ȴ�.
 *
 *   1. use_nl hint�� ���� ����
 *      hint�� �����ϱ� ����,
 *      full (store) nested loop join�� feasibility�� true�� �����.
 *
 *   2. joinable predicate�� �����ϴ� ���,
 *      ������ join method�� ���õǵ��� �ϱ� ����,
 *      full (store) nested loop join�� feasibility�� false�� �����.
 *     ( right graph�� view �Ǵ� join�� ��� feasibility�� flase��.. )
 *
 ***********************************************************************/

    qmsJoinMethodHints * sJoinMethodHint;
    qmoPredicate       * sJoinPredicate;
    idBool               sIsUsable = ID_FALSE;
    idBool               sIsCurrentHint;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::usableJoinMethodFullNL::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aGraph != NULL );

    //------------------------------------------
    // USE_NL hint �˻�
    //------------------------------------------

    // fix BUG-13232
    // use_nl hint�� �ִ� ��� (��: use_nl(table, view) )
    // full nested loop, full store nested loop join�� �����ؾ� ��.
    for( sJoinMethodHint = aGraph->myQuerySet->SFWGH->hints->joinMethod;
         sJoinMethodHint != NULL;
         sJoinMethodHint = sJoinMethodHint->next )
    {
        // �߸��� hint�� �ƴϰ�
        // USE_NL hint�� ���
        if( ( sJoinMethodHint->depInfo.depCount != 0 ) &&
            ( ( sJoinMethodHint->flag & QMO_JOIN_METHOD_NL )
              != QMO_JOIN_METHOD_NONE ) )
        {
            IDE_TEST( qmo::currentJoinDependencies( aGraph,
                                                    & sJoinMethodHint->depInfo,
                                                    & sIsCurrentHint )
                      != IDE_SUCCESS );

            if( sIsCurrentHint == ID_TRUE )
            {
                sIsUsable = ID_TRUE;
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

    if( sIsUsable == ID_TRUE )
    {
        // Nothing To Do
    }
    else
    {
        // fix BUG-12580
        // joinable predicate�� �����ϴ��� �˻�
        // joinable predicate�� ���, �־��� ���
        // ��밡���� join method�� full_nll, full_store_nl �ۿ� ���� ��츦
        // ����ؼ�, full_nl, full_store_nl �̿ܿ�
        // ��밡���� join method�� �ִ����� �˻�
        for( sJoinPredicate = aJoinPredicate;
             sJoinPredicate != NULL;
             sJoinPredicate = sJoinPredicate->next )
        {
            if( ( ( sJoinPredicate->flag & QMO_PRED_JOINABLE_PRED_MASK )
                  == QMO_PRED_JOINABLE_PRED_TRUE )
                &&
                ( ( ( sJoinPredicate->flag & QMO_PRED_HASH_JOINABLE_MASK )
                    == QMO_PRED_HASH_JOINABLE_TRUE )  ||
                  ( ( sJoinPredicate->flag & QMO_PRED_SORT_JOINABLE_MASK )
                    ==QMO_PRED_SORT_JOINABLE_TRUE )   ||
                  ( ( sJoinPredicate->flag & QMO_PRED_MERGE_JOINABLE_MASK )
                    == QMO_PRED_MERGE_JOINABLE_TRUE ) )
                )
            {
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if( sJoinPredicate == NULL )
        {
            sIsUsable = ID_TRUE;
        }
        else
        {
            // Nothing To Do
        }
    }

    if ( ( aGraph->flag & QMG_JOIN_ONLY_NL_MASK )
         == QMG_JOIN_ONLY_NL_TRUE )
    {
        sIsUsable = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }

    *aIsUsable = sIsUsable;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::forcePushPredHint( qmgGraph      * aGraph,
                                     qmoJoinMethod * aJoinMethod )
{
    qmsHints           * sHints;
    qmsPushPredHints   * sPushPredHint;
    qmoJoinMethodCost  * sJoinMethodCost;
    UInt                 i;
    qmgGraph           * sLeftGraph;
    qmgGraph           * sRightGraph;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::forcePushPredHint::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aJoinMethod != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sHints          = aGraph->myQuerySet->SFWGH->hints;
    sJoinMethodCost = aJoinMethod->joinMethodCost;

    // fix BUG-16677
    for( sPushPredHint = sHints->pushPredHint;
         sPushPredHint != NULL;
         sPushPredHint = sPushPredHint->next )
    {
        if( ( sPushPredHint->table->table->tableRef->flag &
              QMS_TABLE_REF_PUSH_PRED_HINT_MASK )
            == QMS_TABLE_REF_PUSH_PRED_HINT_TRUE )
        {
            // push_pred hint�� ����Ǵ� �� �Ǵ� JOIN�� �����ϴ��� �˻�
            if( qtc::dependencyContains(
                    &(((qmgGraph*)(aGraph))->right->depInfo),
                    &(sPushPredHint->table->table->depInfo) )
                == ID_TRUE )
            {
                break;
            }
            else
            {
                // BUG-26800
                // inner join on���� join���ǿ� push_pred ��Ʈ ����
                // view�� ����� ���������� ������ ���̺���
                // ������ �����ʿ��� ó���ǵ��� ������ �����Ѵ�.

                // push_pred hint �����,
                // (1) from v1, t1 where v1.i1=t1.i1 �� ����
                //     join ordering�� left t1, right v1�� ������ ó���Ǿ�������,
                // (2) from v1 inner join t1 on v1.i1=t1.i1 �� ����
                //     join odering������ ��ġ�� �ʱ⶧����
                //     righ�� v1�� ������ �������ش�.
                if( ( aGraph->type == QMG_INNER_JOIN ) &&
                    ( ((qmgJOIN*)aGraph)->onConditionCNF != NULL ) )
                {
                    // ���ʿ� ���̺� �Ǵ� �䰡 �����ϴ��� �˻�
                    if( (aGraph->left->type == QMG_SELECTION) ||
                        (aGraph->left->type == QMG_PARTITION) )
                    {
                        if( ( aGraph->left->myFrom->tableRef->flag &
                              QMS_TABLE_REF_PUSH_PRED_HINT_MASK )
                            == QMS_TABLE_REF_PUSH_PRED_HINT_TRUE )
                        {
                            // push_pred hint�� ����Ǵ� �䰡 �����ϴ��� �˻�
                            if( qtc::dependencyContains(
                                    &(((qmgGraph*)(aGraph))->left->depInfo),
                                    &(sPushPredHint->table->table->depInfo) )
                                == ID_TRUE )
                            {
                                // push_pred hint�� ����Ǵ� �䰡
                                // �����ʿ� ������ join �׷����� ���� ����
                                sLeftGraph   = aGraph->left;
                                sRightGraph  = aGraph->right;

                                aGraph->left  = sRightGraph;
                                aGraph->right = sLeftGraph;
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
                    else
                    {
                        // Nothint To Do
                    }
                }
                else
                {
                    // JOIN ���ʿ� ���̺� �Ǵ� �䰡 �ƴ� ���
                    // ex)    JOIN
                    //    ------------
                    //    |          |
                    //   JOIN       ???

                    // Nothing To Do
                }
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    // PUSH_PRED(view) hint�� ����Ǿ���,
    // join graph�� view�� �����ϴ� �����,
    // join ��������� left->right�� full nested loop join�� �����ϴ�.
    // joinOrdering �����߿� view�� push�Ǵ� predicate�� ������
    // �׷������� ���߿� ����ǵ��� ������ ������.
    // inner join, left outer join, full outer join��
    // PUSH_PRED(view) hint�� ������� ����.

    // BUG-29572
    // PUSH_PRED HINT�� ����Ǵ� �䰡
    // JOIN�� �����ʿ� �� ��쿡 ���ؼ��� join method�� �����ϵ��� �Ѵ�.
    // ��) ���� ������ join method�� join order
    //        JOIN
    //    ------------
    //    |          |
    //   ???        VIEW  <= PUSH_PRED HINT�� ����Ǵ� ��
    //
    // ��) ���� �Ұ����� join method�� join order
    //        JOIN
    //    ------------
    //    |          |
    //   VIEW       ???
    //    ^
    //    |
    //  PUSH_PRED HINT�� ����Ǵ� ��
    //
    // ��, PUSH_PRED HINT�� ����Ǵ� �並 ���� JOIN��
    //     ���� JOIN�� ���ʿ� �� ���� �ִ�.
    //
    // ��) ���� ������ join method�� join order
    //        JOIN
    //    ------------
    //    |          |
    //   ???        JOIN  <= PUSH_PRED HINT�� ����Ǵ� �䰡 ���Ե� JOIN
    //
    //        JOIN
    //    ------------
    //    |          |
    //   JOIN       ???
    //    ^
    //    |
    //  PUSH_PRED HINT�� ����Ǵ� �䰡 ���Ե� JOIN

    // �Ʒ��� if���� join graph�� �����ʿ� ���� join �����
    // push_pred�� ����� �������� �����ϴ� ����
    //
    // aGraph->right->type == QMG_SELECTION
    //   => TABLE/VIEW�� JOIN �׷����� �����ϱ� ���� ���
    // sPushPredHint == NULL �� ���� push_pred�� ����Ǵ� �䰡 ���� �����
    //   => ���� for������ push_pred�� ����� view�� �ִ� ��� sPushPredHint�� ������
    if( ( (aGraph->right->type == QMG_SELECTION) ||
          (aGraph->right->type == QMG_PARTITION) )
        &&
        ( sPushPredHint != NULL ) )
    {
        for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
        {
            sJoinMethodCost = & aJoinMethod->joinMethodCost[i];

            if( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_NL )
                != QMO_JOIN_METHOD_FULL_NL )
            {
                sJoinMethodCost->flag &=
                    ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                sJoinMethodCost->flag
                    |=  QMO_JOIN_METHOD_FEASIBILITY_FALSE;
            }
            else
            {
                // ������ join order�� right->left�� ��츦 ������
                if( i == 1 )
                {
                    sJoinMethodCost->flag &=
                        ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                    sJoinMethodCost->flag
                        |=  QMO_JOIN_METHOD_FEASIBILITY_FALSE;
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

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::forceJoinOrder4RecursiveView( qmgGraph      * aGraph,
                                                qmoJoinMethod * aJoinMethod )
{
/******************************************************************************
 *
 * Description : PROJ-2582 recursive with
 *          recursive view�� right subquery���� �����Ǵ� bottom recursive
 *          view�� ���� �ѹ��� scan������ ����Ǿ�� �Ѵ�. ���� join��
 *          �����ʿ� ���� ���, ��쿡 ���� restart�Ǿ� ���� �ݺ��� �� �ִ�.
 *
 * Implementation :
 *
 *****************************************************************************/

    qmoJoinMethodCost  * sJoinMethodCost;
    UShort               sRecursiveViewID;
    qcDepInfo            sDepInfo;
    UInt                 i;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::forceJoinOrder4RecursiveView::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aGraph != NULL );
    IDE_DASSERT( aJoinMethod != NULL );

    //------------------------------------------
    // �⺻ �ʱ�ȭ
    //------------------------------------------

    sJoinMethodCost = aJoinMethod->joinMethodCost;
    sRecursiveViewID = aGraph->myQuerySet->SFWGH->recursiveViewID;

    //------------------------------------------
    // recursive view�� join�� ���� �������� �����Ѵ�.
    //------------------------------------------

    if ( sRecursiveViewID != ID_USHORT_MAX )
    {
        qtc::dependencySet( sRecursiveViewID, &sDepInfo );

        if ( qtc::dependencyContains( &(aGraph->depInfo),
                                      &sDepInfo )
             == ID_TRUE )
        {
            if ( qtc::dependencyContains( &(aGraph->right->depInfo),
                                          &sDepInfo )
                 == ID_TRUE )
            {
                // left->right join ����
                for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
                {
                    sJoinMethodCost = & aJoinMethod->joinMethodCost[i];

                    if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
                         == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
                    {
                        sJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
                    }
                    else
                    {
                        // Nothing To Do
                    }
                }
            }
            else
            {
                // right->left join ����
                for ( i = 0; i < aJoinMethod->joinMethodCnt; i++ )
                {
                    sJoinMethodCost = & aJoinMethod->joinMethodCost[i];

                    if ( ( sJoinMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
                         == QMO_JOIN_METHOD_DIRECTION_RIGHTLEFT )
                    {
                        sJoinMethodCost->flag &= ~QMO_JOIN_METHOD_FEASIBILITY_MASK;
                        sJoinMethodCost->flag |= QMO_JOIN_METHOD_FEASIBILITY_FALSE;
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
        // Nothing To Do
    }

    return IDE_SUCCESS;
}

IDE_RC
qmoJoinMethodMgr::setJoinPredInfo4NL( qcStatement           * aStatement,
                                      qmoPredicate          * aJoinPredicate,
                                      qmoIdxCardInfo        * aRightIndexInfo,
                                      qmgGraph              * aRightGraph,
                                      qmoJoinMethodCost     * aMethodCost )
{
/******************************************************************************
 *
 * Description : Index Nested Loop Join �� ���õ� predicate �� �����Ͽ�
 *               qmoPredicate.totalSelectivity �� �����ϰ�
 *               qmoJoinMethodCost.joinPredicate (qmoPredInfo list) �� ����
 *
 * Implementation :
 *
 *****************************************************************************/

    qmoJoinIndexNL    sIndexNLInfo;
    qmoPredInfo     * sJoinPredInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinPredInfo4NL::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement      != NULL );
    IDE_DASSERT( aRightIndexInfo != NULL );
    IDE_DASSERT( aRightGraph     != NULL );
    IDE_DASSERT( aMethodCost     != NULL );

    //------------------------------------------
    // Join ���� qmoPredInfo list ȹ��
    //------------------------------------------

    if ( aJoinPredicate != NULL )
    {
        // Index Nested Loop Join ��� �� �Ѱ��� ���� ����
        sIndexNLInfo.index = aRightIndexInfo->index;
        sIndexNLInfo.predicate = aJoinPredicate;
        sIndexNLInfo.rightChildPred = aRightGraph->myPredicate;
        sIndexNLInfo.rightDepInfo = & aRightGraph->depInfo;
        sIndexNLInfo.rightStatiscalData =
            aRightGraph->myFrom->tableRef->statInfo;

        if( ( aMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
            == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
        {
            sIndexNLInfo.direction = QMO_JOIN_DIRECTION_LEFT_RIGHT;
        }
        else
        {
            sIndexNLInfo.direction = QMO_JOIN_DIRECTION_RIGHT_LEFT;
        }

        // Join ���� qmoPredInfo list ȹ��
        IDE_TEST( getIndexNLPredInfo( aStatement,
                                      & sIndexNLInfo,
                                      & sJoinPredInfo )
                  != IDE_SUCCESS );

        aMethodCost->joinPredicate = sJoinPredInfo;
        aMethodCost->rightIdxInfo = aRightIndexInfo;
    }
    else
    {
        // join predicate �� ���� ���
        aMethodCost->joinPredicate = NULL;
        aMethodCost->rightIdxInfo = aRightIndexInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC
qmoJoinMethodMgr::getIndexNLPredInfo( qcStatement      * aStatement,
                                      qmoJoinIndexNL   * aIndexNLInfo,
                                      qmoPredInfo     ** aJoinPredInfo )
{
/******************************************************************************
 *
 * Description : Index Nested Loop Join ������ qmoPredInfo list �� �����Ѵ�.
 *              [ composite index�� ���, join index Ȱ���� ����ȭ ���� ���� ]
 *
 * Implementation :
 *
 *  join predicate�� one table predicateó�� predicate���ġ�� �� �� �����Ƿ�,
 *  �ε����÷��� ���õ� �÷��� ���Ḯ��Ʈ�� �����Ѵ�.
 *
 *  index �� ������ predicate�� ���������� graph�� �ѱ�µ�, �� ����������
 *  join method�� �����Ǹ�, �ش� join predicate�� �и��س��� ������ �̿�ȴ�.
 *
 *  right child graph�� predicate�� qmoPredicate�� ����Ǿ� �ִ� ���� �̿��Ѵ�.
 *
 ******************************************************************************/

    UInt           sCnt;
    UInt           sIdxColumnID;
    UInt           sJoinDirection   = 0;
    idBool         sIsUsableNextKey = ID_TRUE;
    idBool         sExistKeyRange   = ID_FALSE;
    qmoPredicate * sPredicate;
    qmoPredicate * sTempPredicate;
    qmoPredicate * sTempMorePredicate;
    qmoPredInfo  * sPredInfo;
    qmoPredInfo  * sTempInfo = NULL;
    qmoPredInfo  * sMoreInfo;
    qmoPredInfo  * sJoinPredInfo = NULL;
    qmoPredInfo  * sLastJoinPredInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getIndexNLPredInfo::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aIndexNLInfo->index != NULL );
    IDE_DASSERT( aIndexNLInfo->predicate != NULL );
    IDE_DASSERT( aIndexNLInfo->rightDepInfo != NULL );
    IDE_DASSERT( aIndexNLInfo->rightStatiscalData != NULL );
    IDE_DASSERT( aJoinPredInfo != NULL );

    //--------------------------------------
    // index nested loop join qmoPred info ȹ��
    //--------------------------------------

    if( aIndexNLInfo->index->isOnlineTBS == ID_TRUE )
    {
        // �ε��� ��� ����

        //--------------------------------------
        // �ӽ� ���� ����.
        // 1. ���ڷ� ���� right dependencies ������ columnID�� ����.
        //    ( �ε��� �÷��� ���ϱ� ����)
        // 2. ���� �÷��� ���� ��ǥ selectivity��
        //    �����ε����÷� ��뿩�ο� ���� ������ ���ϱ� ���� �ʿ��� ������
        //    indexArgument ���� ����.
        //    one table predicate�� ������ �Լ� ��� (qmoPred::setTotal)
        //--------------------------------------

        if( aIndexNLInfo->direction == QMO_JOIN_DIRECTION_LEFT_RIGHT )
        {
            sJoinDirection = QMO_PRED_INDEX_LEFT_RIGHT;
        }
        else
        {
            sJoinDirection = QMO_PRED_INDEX_RIGHT_LEFT;
        }

        IDE_TEST( setTempInfo( aStatement,
                               aIndexNLInfo->predicate,
                               aIndexNLInfo->rightDepInfo,
                               sJoinDirection )
                  != IDE_SUCCESS );

        for( sCnt = 0; sCnt < aIndexNLInfo->index->keyColCount; sCnt++ )
        {
            sIdxColumnID = aIndexNLInfo->index->keyColumns[sCnt].column.id;

            // �ε��� �÷��� ���� �÷��� ���� predicate�� ã�´�.
            sTempInfo = NULL;
            sTempPredicate = NULL;

            for( sPredicate = aIndexNLInfo->predicate;
                 sPredicate != NULL;
                 sPredicate = sPredicate->next )
            {
                if( ( ( sPredicate->flag & QMO_PRED_INDEX_JOINABLE_MASK )
                      == QMO_PRED_INDEX_JOINABLE_TRUE )
                    &&
                    ( ( sPredicate->flag & QMO_PRED_INDEX_DIRECTION_MASK
                        & sJoinDirection ) == sJoinDirection ) )
                {
                    if( sIdxColumnID == sPredicate->id )
                    {

                        IDE_TEST( QC_QMP_MEM( aStatement )->alloc( ID_SIZEOF( qmoPredInfo ),
                                                                   (void **)& sPredInfo )
                                  != IDE_SUCCESS );

                        sPredInfo->predicate = sPredicate;
                        sPredInfo->next = NULL;
                        sPredInfo->more = NULL;

                        //---------------------------------------
                        // ������豸��
                        // 1. sTempInfo :
                        //    ���� �÷��� ���� predicate �����������
                        // 2. sTempPredicate :
                        //    one table predicate�� ���� qmoPred::setTotal()
                        //    �� ����ϱ� ���� �ӽ� predicate������豸��
                        //---------------------------------------
                        if( sTempInfo == NULL )
                        {
                            sTempInfo = sPredInfo;
                            sMoreInfo = sTempInfo;

                            sTempPredicate = sPredInfo->predicate;
                            sTempMorePredicate = sTempPredicate;
                        }
                        else
                        {
                            sMoreInfo->more = sPredInfo;
                            sMoreInfo = sMoreInfo->more;

                            sTempMorePredicate->more = sPredInfo->predicate;
                            sTempMorePredicate = sTempMorePredicate->more;
                        }
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
            } // �ϳ��� �ε��� �÷��� ���� �÷��� predicate�� ã�´�.

            // �ε��� �÷��� ���� �÷��� predicate�� �����Ѵ�.

            if( sTempInfo != NULL )
            {
                // �ϳ��� �÷�����Ʈ�� ���� ��ǥ selectivity
                // (qmoPredicate.totalSelectivity) �� ȹ���ϰ�
                // ���� �ε��� ��� ���� ������ ��´�.
                IDE_TEST( qmoSelectivity::setTotalSelectivity( aStatement,
                                                               aIndexNLInfo->rightStatiscalData,
                                                               sTempPredicate )
                          != IDE_SUCCESS );

                qmoPred::setCompositeKeyUsableFlag( sTempPredicate );

                // ���� �÷�����Ʈ�� ������� ����
                if( sJoinPredInfo == NULL )
                {
                    sJoinPredInfo = sTempInfo;
                    sLastJoinPredInfo = sJoinPredInfo;
                }
                else
                {
                    sLastJoinPredInfo->next = sTempInfo;
                    sLastJoinPredInfo = sLastJoinPredInfo->next;
                }

                // ������ �ӽ÷� ������ predicate�� more ������� ���󺹱�
                for( sPredicate = sTempPredicate;
                     sPredicate != NULL;
                     sPredicate = sTempMorePredicate )
                {
                    sTempMorePredicate = sPredicate->more;
                    sPredicate->more = NULL;
                }

                // ���� �ε��� �÷� ��뿩��
                if( ( sPredInfo->predicate->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                      == QMO_PRED_NEXT_KEY_USABLE )
                {
                    // Nothing To Do
                }
                else
                {
                    sIsUsableNextKey = ID_FALSE;
                    break;
                }
            }
            else
            {
                // Nothing To Do
            }
        }

        //-------------------------------
        // join index Ȱ���� ����ȭ ����
        // right graph�� predicate�� �ش� �ε��� �÷��� ����
        // predicate�� �����ϴ��� �˻�.
        //-------------------------------

        if( ( sJoinPredInfo == NULL ) || ( sIsUsableNextKey == ID_FALSE ) )
        {
            // ���ڷ� �Ѿ�� �ε����� ���õ�
            // joinable predicate�� �������� �ʴ� ����̰ų�,
            // joinable predicate�� ���� �ε��� ��밡������ ���� ���.

            // Nothing To Do
        }
        else
        {
            // ù��° �÷��� �����ϴ��� �˻��Ѵ�.
            sIdxColumnID = aIndexNLInfo->index->keyColumns[0].column.id;

            // right �׷����� �����ϴ��� �˻��Ѵ�.
            for( sPredicate = aIndexNLInfo->rightChildPred;
                 sPredicate != NULL;
                 sPredicate = sPredicate->next )
            {
                if( sIdxColumnID == sPredicate->id )
                {
                    sExistKeyRange = ID_TRUE;
                    break;
                }
                else
                {
                    // �ε��� �÷��� ���� �÷��� ���� predicate�� ���� ���
                    // Nothing To Do
                }
            }

            // Join Predicate�� �����ϴ��� �˻��Ѵ�.
            for( sPredInfo = sJoinPredInfo;
                 sPredInfo != NULL;
                 sPredInfo = sPredInfo->next )
            {
                if( sIdxColumnID == sPredInfo->predicate->id )
                {
                    sExistKeyRange = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }

            if( sExistKeyRange == ID_FALSE )
            {
                sJoinPredInfo = NULL;
            }
            else
            {
                // Nothing To Do
            }
        }
    }
    else // ( index->isOnlineTBS == ID_FALSE )
    {
        sJoinPredInfo = NULL;
    }

    *aJoinPredInfo = sJoinPredInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setJoinPredInfo4AntiOuter( qcStatement       * aStatement,
                                             qmoPredicate      * aJoinPredicate,
                                             qmoIdxCardInfo    * aRightIndexInfo,
                                             qmgGraph          * aRightGraph,
                                             qmgGraph          * aLeftGraph,
                                             qmoJoinMethodCost * aMethodCost )
{
/******************************************************************************
 *
 * Description : Full outer join �� ���� join predicate �� �������
 *               qmoPredicate.totalSelectivity �� �����ϰ�
 *               qmoJoinMethodCost.joinPredicate (qmoPredInfo list) �� ����
 *
 * Implementation :
 *
 ******************************************************************************/

    qcDepInfo       * sRightDepInfo;
    qmoJoinAntiOuter  sAntiOuterInfo;
    qmoIdxCardInfo  * sLeftIndexInfo;
    UInt              sLeftIndexCnt;
    qmgSELT         * sLeftMyGraph;
    qmoPredInfo     * sJoinPredInfo;
    qmoIdxCardInfo  * sSelectedLeftIdxInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinPredInfo4AntiOuter::__FT__" );

    //------------------------------------------
    // ���ռ� �˻�
    //------------------------------------------

    IDE_DASSERT( aStatement      != NULL );
    IDE_DASSERT( aRightIndexInfo != NULL );
    IDE_DASSERT( aRightGraph     != NULL );
    IDE_DASSERT( aLeftGraph      != NULL );
    IDE_DASSERT( aMethodCost     != NULL );

    //------------------------------------------
    // qmoPredInfo list ȹ��
    //------------------------------------------

    sJoinPredInfo = NULL;
    sSelectedLeftIdxInfo = NULL;

    if( aJoinPredicate != NULL )
    {
        sRightDepInfo = & aRightGraph->depInfo;
        sLeftIndexInfo = aLeftGraph->myFrom->tableRef->statInfo->idxCardInfo;
        sLeftIndexCnt = aLeftGraph->myFrom->tableRef->statInfo->indexCnt;

        // sJoinPredInfo ȹ�� �Ѱ��� ���� ����
        sAntiOuterInfo.index = aRightIndexInfo->index;
        sAntiOuterInfo.predicate = aJoinPredicate;
        // To Fix BUG-8384
        sAntiOuterInfo.rightDepInfo = sRightDepInfo;
        sAntiOuterInfo.leftIndexCnt = sLeftIndexCnt;
        sAntiOuterInfo.leftIndexInfo = sLeftIndexInfo;

        if( aLeftGraph->type == QMG_SELECTION )
        {
            sLeftMyGraph = (qmgSELT*) aLeftGraph;

            // sJoinPredInfo ȹ��
            IDE_TEST( getAntiOuterPredInfo( aStatement,
                                            sLeftMyGraph->accessMethodCnt,
                                            sLeftMyGraph->accessMethod,
                                            & sAntiOuterInfo,
                                            & sJoinPredInfo,
                                            & sSelectedLeftIdxInfo )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do
        }

        aMethodCost->joinPredicate = sJoinPredInfo;
        aMethodCost->leftIdxInfo = sSelectedLeftIdxInfo;
        aMethodCost->rightIdxInfo = aRightIndexInfo;
    }
    else
    {
        aMethodCost->joinPredicate = NULL;
        aMethodCost->leftIdxInfo = NULL;
        aMethodCost->rightIdxInfo = aRightIndexInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getAntiOuterPredInfo( qcStatement      * aStatement,
                                        UInt               aMethodCount,
                                        qmoAccessMethod  * aAccessMethod,
                                        qmoJoinAntiOuter * aAntiOuterInfo,
                                        qmoPredInfo     ** aJoinPredInfo,
                                        qmoIdxCardInfo  ** aLeftIdxInfo )
{
/******************************************************************************
 *
 * Description : Anti Outer Join ���� qmoPredInfo list ��
 *               cost �� ���� ���� qmoAccessMethod �� index �� ��ȯ�Ѵ�.
 *
 *               ���ڷ� �Ѿ�� qmoAccessMethod ���õ� predicate�� ã�Ƽ�
 *               qmoPredInfo list �� ���Ѵ�.
 *
 *   Anti Outer Join predicate��
 *   (1) join predicate ���� ��� �ε����� ������ �־�� �ϸ�,
 *   (2) index column ���� �ϳ��� predicate�� ���õ� �� �ִ�.
 *
 * Implementation :
 *
 *****************************************************************************/

    UInt              sCnt;
    UInt              sDirection = 0;
    UInt              sIdxColumnID;
    qmoPredicate    * sPredicate = NULL;
    qmoPredicate    * sTempPredicate = NULL;
    qmoPredicate    * sTempMorePredicate = NULL;
    qmoPredicate    * sAntiOuterPred = NULL;
    qmoPredInfo     * sPredInfo = NULL;
    qmoPredInfo     * sJoinPredInfo = NULL;
    qmoPredInfo     * sLastJoinPredInfo = NULL;
    qmoIdxCardInfo  * sSelectedLeftIdxInfo = NULL;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getAntiOuterPredInfo::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aAccessMethod  != NULL );
    IDE_DASSERT( aAntiOuterInfo != NULL );
    IDE_DASSERT( aAntiOuterInfo->index != NULL );
    IDE_DASSERT( aAntiOuterInfo->predicate != NULL );
    IDE_DASSERT( aAntiOuterInfo->rightDepInfo != NULL );
    IDE_DASSERT( aJoinPredInfo  != NULL );
    IDE_DASSERT( aLeftIdxInfo   != NULL );

    //--------------------------------------
    // Anti outer join qmoPredInfo list ȹ��
    //--------------------------------------

    if( aAntiOuterInfo->index->isOnlineTBS == ID_TRUE )
    {
        // �ε��� ��� ����

        //--------------------------------------
        // �ӽ� ���� ����.
        // 1. ���ڷ� ���� right dependencies ������ columnID�� ����.
        //    ( �ε��� �÷��� ���ϱ� ����)
        // 2. left child graph�� column�� ã�� ����.
        //--------------------------------------

        sDirection = QMO_PRED_INDEX_DIRECTION_MASK;

        IDE_TEST( setTempInfo( aStatement,
                               aAntiOuterInfo->predicate,
                               aAntiOuterInfo->rightDepInfo,
                               sDirection )
                  != IDE_SUCCESS );

        //--------------------------------------
        // �ε��� �÷������� �ε��� �÷��� ������ �÷��� predicate�� ã�´�.
        //--------------------------------------

        for( sCnt = 0; sCnt < aAntiOuterInfo->index->keyColCount; sCnt++ )
        {
            sIdxColumnID = aAntiOuterInfo->index->keyColumns[sCnt].column.id;

            // �ε��� �÷��� ���� �÷��� ���� predicate�� ã�´�.
            sTempPredicate = NULL;

            for( sPredicate = aAntiOuterInfo->predicate;
                 sPredicate != NULL;
                 sPredicate = sPredicate->next )
            {
                if( ( ( sPredicate->flag & QMO_PRED_INDEX_JOINABLE_MASK )
                      == QMO_PRED_INDEX_JOINABLE_TRUE )
                    &&
                    ( ( sPredicate->flag & QMO_PRED_INDEX_DIRECTION_MASK )
                      == sDirection ) )
                {
                    if( sIdxColumnID == sPredicate->id )
                    {
                        //---------------------------------------
                        // ������豸��
                        // sTempPredicate :
                        // ���� right �ε��� �÷��� predicate�� ���õ�
                        // left �ε��� �÷��� ã�� ����
                        // �ӽ� predicate�� �������
                        //---------------------------------------
                        if( sTempPredicate == NULL )
                        {
                            sTempPredicate = sPredicate;
                            sTempMorePredicate = sTempPredicate;
                        }
                        else
                        {
                            sTempMorePredicate->more = sPredicate;
                            sTempMorePredicate = sTempMorePredicate->more;
                        }
                    }
                    else
                    {
                        // Nothing To Do
                    }
                } // ���� �ε��� �÷��� ���õ� predicate�� �����Ҷ��� ó��
                else
                {
                    // Nothing To Do
                }
            } // �ϳ��� �ε��� �÷��� ���� �÷��� predicate�� ã�´�.

            if( sTempPredicate == NULL )
            {
                // ���� �ε����� ���õ� predicate�� �������� �ʴ� ���
                break;
            }
            else
            {
                IDE_TEST( getAntiOuterPred( aStatement,
                                            sCnt,          // keyColCount
                                            aMethodCount,  // aAntiOuterInfo->leftIndexCnt,
                                            aAccessMethod, // aAntiOuterInfo->leftIndexInfo,
                                            sTempPredicate,
                                            & sAntiOuterPred,
                                            & sSelectedLeftIdxInfo )
                          != IDE_SUCCESS );
            }

            // �ε��� �÷��� ���� �÷��� predicat�� �����Ѵ�.
            if( sAntiOuterPred != NULL )
            {
                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredInfo),
                                                         (void **)&sPredInfo )
                          != IDE_SUCCESS );

                // qmoPredicate.totalSelectivity ȹ��
                sAntiOuterPred->totalSelectivity = sAntiOuterPred->mySelectivity;
                sPredInfo->predicate = sAntiOuterPred;
                sPredInfo->next = NULL;
                sPredInfo->more = NULL;

                // ���� �÷�����Ʈ�� ������� ����
                if( sJoinPredInfo == NULL )
                {
                    sJoinPredInfo = sPredInfo;
                    sLastJoinPredInfo = sJoinPredInfo;
                }
                else
                {
                    sLastJoinPredInfo->next = sPredInfo;
                    sLastJoinPredInfo = sLastJoinPredInfo->next;
                }

                // ������ �ӽ÷� ������ predicate�� more ������� ���󺹱�
                for( sPredicate = sTempPredicate;
                     sPredicate != NULL;
                     sPredicate = sTempMorePredicate )
                {
                    sTempMorePredicate = sPredicate->more;
                    sPredicate->more = NULL;
                }
            }
            else
            {
                break;
            }

            // ���� �ε��� �÷� ��뿩��
            if( ( sPredInfo->predicate->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                == QMO_PRED_NEXT_KEY_USABLE )
            {
                // Nothing To Do
            }
            else
            {
                break;
            }
        } // end of for()

    } // ( index->isOnlineTBS == ID_FALSE )
    else
    {
        sJoinPredInfo = NULL;
    }

    *aJoinPredInfo = sJoinPredInfo;
    *aLeftIdxInfo = sSelectedLeftIdxInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::getAntiOuterPred( qcStatement      * aStatement,
                                    UInt               aKeyColCnt,
                                    UInt               aMethodCount,
                                    qmoAccessMethod  * aAccessMethod,
                                    qmoPredicate     * aPredicate,
                                    qmoPredicate    ** aAntiOuterPred,
                                    qmoIdxCardInfo  ** aSelectedLeftIdxInfo )
{
/******************************************************************************
 *
 * Description : anti outer join ���� qmoPredInfo list ȹ��� ȣ��ȴ�.
 *               right graph �� ���� �ε��� �÷��� ���� predicate �� ����
 *               left graph (scan) �� qmoAccessMethod array �� ��ȸ�ϸ� cost ��
 *               ���� ���� qmoAccessMethod �� index �� �׿� ���ϴ� predicate ��ȯ
 *
 *     aMethodCount : left graph (scan) �� method count
 *     aKeyColCnt  : ���� right index �÷�
 *
 * Implementation :
 *
 *****************************************************************************/

    idBool             sIsAntiOuterPred;
    UInt               sValueColumnID;
    UInt               sCnt;
    UInt               sIdxColumnID;
    qtcNode          * sCompareNode;
    qtcNode          * sValueNode;
    qmoPredicate     * sPredicate;
    qmoPredicate     * sAntiOuterPred;
    qmoAccessMethod  * sAccessMethod;
    qmoIdxCardInfo   * sMethodIndexInfo;
    qmoIdxCardInfo   * sSelLeftIdxInfo;
    SDouble            sTotalCost;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getAntiOuterPred::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement           != NULL );
    IDE_DASSERT( aAccessMethod        != NULL );
    IDE_DASSERT( aPredicate           != NULL );
    IDE_DASSERT( aAntiOuterPred       != NULL );
    IDE_DASSERT( aSelectedLeftIdxInfo != NULL );

    //--------------------------------------
    // ���� �÷�����Ʈ���� anti outer join predicate ����
    //--------------------------------------

    sAntiOuterPred = NULL;
    sSelLeftIdxInfo = *aSelectedLeftIdxInfo;
    sAccessMethod = aAccessMethod;

    for( sPredicate = aPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->more )
    {
        sIsAntiOuterPred = ID_TRUE;

        if( ( sPredicate->node->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
            == MTC_NODE_LOGICAL_CONDITION_TRUE )
        {
            sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);

            if( sCompareNode->node.next == NULL )
            {
                // Nothing To Do
            }
            else
            {
                // OR ��� ������ �񱳿����ڰ� ������ �ִ� ����
                // anti outer joinable predicate�� �ƴ�.
                sIsAntiOuterPred = ID_FALSE;
            }
        }
        else
        {
            sCompareNode = sPredicate->node;
        }

        if( sIsAntiOuterPred == ID_TRUE )
        {
            if( sCompareNode->indexArgument == 0 )
            {
                sValueNode = (qtcNode *)(sCompareNode->node.arguments->next);
            }
            else
            {
                sValueNode = (qtcNode *)(sCompareNode->node.arguments);
            }

            //-------------------------------------------
            // anti outer join predicate�� ã�´�.
            // ���� right index column�� ���� columnID�� ����
            // predicate�� ���Ḯ��Ʈ�� value node��
            // ���� columnID�� ���� left index�� ã�´�.
            //--------------------------------------------
            sValueColumnID =
                QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sValueNode->node.table].
                columns[sValueNode->node.column].column.id;

            // BUG-36454 index hint �� ����ϸ� sAccessMethod[0] �� fullscan �ƴϴ�.
            for( sCnt = 0; sCnt < aMethodCount; sCnt++ )
            {
                sMethodIndexInfo = sAccessMethod[sCnt].method;

                if( sMethodIndexInfo != NULL )
                {
                    if( ( sMethodIndexInfo->index->isOnlineTBS == ID_TRUE ) &&
                        ( ( sMethodIndexInfo->index->keyColCount - 1 )
                          >= aKeyColCnt ) )
                    {
                        // �ε��� ��� ����
                        sIdxColumnID =
                            sMethodIndexInfo->index->keyColumns[aKeyColCnt].column.id;

                        if( sIdxColumnID == sValueColumnID )
                        {
                            if( sAntiOuterPred == NULL )
                            {
                                sTotalCost = sAccessMethod[sCnt].totalCost;
                                sSelLeftIdxInfo = sMethodIndexInfo;
                                sAntiOuterPred = sPredicate;
                            }
                            else
                            {
                                // cost �� ���� access method �� index info ����
                                /* BUG-40589 floating point calculation */
                                if (QMO_COST_IS_GREATER(sTotalCost,
                                                        sAccessMethod[sCnt].totalCost)
                                    == ID_TRUE)
                                {
                                    sTotalCost = sAccessMethod[sCnt].totalCost;
                                    sSelLeftIdxInfo = sMethodIndexInfo;
                                    sAntiOuterPred = sPredicate;
                                }
                                else
                                {
                                    // Nothing To Do
                                }
                            }
                        }
                        else
                        {
                            // Nothing To Do
                        }
                    }
                    else // ( index->isOnlineTBS == ID_FALSE )
                    {
                        // Nothing To Do
                    }
                }
                else // ( sMethodIndexInfo == NULL )
                {
                    // Nothing To Do
                }
            }
        } // AntiOuterJoin predicate�� ���
        else
        {
            // OR ��� ������ �񱳿����ڰ� �ΰ��̻��� ����,
            // Anti outer joinable predicate�� �ƴ�.

            // Nothing To Do
        }
    }

    *aAntiOuterPred = sAntiOuterPred;
    *aSelectedLeftIdxInfo = sSelLeftIdxInfo;

    return IDE_SUCCESS;
}


IDE_RC
qmoJoinMethodMgr::setJoinPredInfo4Hash( qcStatement       * aStatement,
                                        qmoPredicate      * aJoinPredicate,
                                        qmoJoinMethodCost * aMethodCost )
{
/******************************************************************************
 *
 * Description : Hash Join �� ���õ� predicate �� �����Ͽ�
 *               qmoPredicate.totalSelectivity �� �����ϰ�
 *               qmoJoinMethodCost.joinPredicate (qmoPredInfo list) �� ����
 *
 * Implementation : ���ڷ� �Ѿ�� join predicate list�߿���
 *                  hash joinable predicate �� ��� ã��,
 *                  ã�� predicate�� ���������� �����Ѵ�.
 *
 *****************************************************************************/

    qmoPredicate * sPredicate;
    qmoPredInfo  * sPredInfo;
    qmoPredInfo  * sJoinPredInfo = NULL;
    qmoPredInfo  * sLastJoinPredInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinPredInfo4Hash::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aJoinPredicate != NULL );
    IDE_DASSERT( aMethodCost    != NULL );

    //--------------------------------------
    // Hash join predicate info ȹ��
    //--------------------------------------

    sJoinPredInfo = NULL;
    sPredicate = aJoinPredicate;

    while( sPredicate != NULL )
    {
        if( ( sPredicate->flag & QMO_PRED_HASH_JOINABLE_MASK )
            == QMO_PRED_HASH_JOINABLE_TRUE )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredInfo),
                                                     (void **)&sPredInfo )
                      != IDE_SUCCESS );

            sPredInfo->predicate = sPredicate;
            sPredInfo->next = NULL;
            sPredInfo->more = NULL;

            // qmoPredicate.totalSelectivity ȹ��
            sPredInfo->predicate->totalSelectivity
                    = sPredInfo->predicate->mySelectivity;

            if( sJoinPredInfo == NULL )
            {
                sJoinPredInfo = sPredInfo;
                sLastJoinPredInfo = sJoinPredInfo;
            }
            else
            {
                sLastJoinPredInfo->next = sPredInfo;
                sLastJoinPredInfo = sLastJoinPredInfo->next;
            }
        }
        else
        {
            // Nothing To Do
        }

        sPredicate = sPredicate->next;
    }

    aMethodCost->joinPredicate = sJoinPredInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setJoinPredInfo4Sort( qcStatement       * aStatement,
                                        qmoPredicate      * aJoinPredicate,
                                        qcDepInfo         * aRightDepInfo,
                                        qmoJoinMethodCost * aMethodCost )
{
/******************************************************************************
 *
 * Description : Sort Join �� ���õ� predicate �� �����Ͽ�
 *               qmoPredicate.totalSelectivity �� �����ϰ�
 *               qmoJoinMethodCost.joinPredicate (qmoPredInfo list) �� ����
 *               (sort join�� �� �÷��� ���ؼ��� ������ �� ����)
 *
 * Implementation :
 *
 *****************************************************************************/

    qcDepInfo       sAndDepInfo;
    qtcNode       * sCompareNode;
    qtcNode       * sSortColumnNode;
    qmoPredicate  * sPredicate;
    qmoPredInfo   * sPredInfo;
    qmoPredInfo   * sTempInfo = NULL;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinPredInfo4Sort::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aJoinPredicate != NULL );
    IDE_DASSERT( aRightDepInfo  != NULL );
    IDE_DASSERT( aMethodCost    != NULL );

    //--------------------------------------
    // Sort join qmoPredInfo list ȹ��
    //--------------------------------------

    // ó���� predicate�� ���� �ߺ� ���� ������ ���� flag �ʱ�ȭ
    for( sPredicate = aJoinPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        sPredicate->flag &= ~QMO_PRED_SEL_PROCESS_MASK;
    }

    //--------------------------------------
    // sort joinable predicate���� �÷����� �и���ġ�ϰ�,
    // �÷����� total selectivity�� ���� ��,
    // total selectivity�� ���� ���� �÷� �ϳ��� �����Ѵ�.
    //--------------------------------------
    for( sPredicate = aJoinPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        if( ( ( sPredicate->flag & QMO_PRED_SORT_JOINABLE_MASK )
              == QMO_PRED_SORT_JOINABLE_TRUE )
            &&
            ( ( sPredicate->flag & QMO_PRED_SEL_PROCESS_MASK )
              == QMO_PRED_SEL_PROCESS_FALSE ) )
        {
            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredInfo),
                                                     (void **)&sPredInfo )
                      != IDE_SUCCESS );

            sPredicate->flag &= ~QMO_PRED_SEL_PROCESS_MASK;
            sPredicate->flag |= QMO_PRED_SEL_PROCESS_TRUE;

            sPredInfo->predicate = sPredicate;
            sPredInfo->next = NULL;
            sPredInfo->more = NULL;

            // �� predicate�� ������ �÷��� predicate�� �����ϴ���
            // �˻��ϱ� ����, right dependencies�� �ش��ϴ� ��带 ã�´�.
            if( ( sPredicate->node->node.lflag
                  & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
            }
            else
            {
                sCompareNode = sPredicate->node;
            }

            // sort column�� ã�´�.
            qtc::dependencyAnd(
                aRightDepInfo,
                & ( (qtcNode *)(sCompareNode->node.arguments) )->depInfo,
                & sAndDepInfo );

            if( qtc::dependencyEqual( & sAndDepInfo,
                                      & qtc::zeroDependencies ) != ID_TRUE )
            {
                sSortColumnNode = (qtcNode *)(sCompareNode->node.arguments);
            }
            else
            {
                sSortColumnNode
                    = (qtcNode *)(sCompareNode->node.arguments->next);
            }

            //--------------------------------------------
            // ���� sort column�� ������ �÷��� �����ϴ��� ã��
            // qmoPredicate.totalSelectivity �� ȹ���Ѵ�.
            //--------------------------------------------
            if( sPredicate->next != NULL )
            {
                IDE_TEST( findEqualSortColumn( aStatement,
                                               sPredicate->next,
                                               aRightDepInfo,
                                               sSortColumnNode,
                                               sPredInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                sPredInfo->predicate->totalSelectivity
                    = sPredInfo->predicate->mySelectivity;
            }

            if( sTempInfo == NULL )
            {
                sTempInfo = sPredInfo;
            }
            else
            {
                /* BUG-40589 floating point calculation */
                if (QMO_COST_IS_GREATER(sTempInfo->predicate->totalSelectivity,
                                        sPredInfo->predicate->totalSelectivity)
                    == ID_TRUE)
                {
                    sTempInfo = sPredInfo;
                }
                else
                {
                    // Nothing To Do
                }
            }

        } // sort joinable predicate�� ���� ó��
        else
        {
            // sort joinable predicate�� �ƴϰų�,
            // sort joinable predicate�� ���, �̹� ó���� predicate�� ���

            // Nothing To Do
        }
    }

    aMethodCost->joinPredicate = sTempInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setJoinPredInfo4Merge( qcStatement         * aStatement,
                                         qmoPredicate        * aJoinPredicate,
                                         qmoJoinMethodCost   * aMethodCost )
{
/******************************************************************************
 *
 * Description : Merge Join �� ���õ� predicate ��
 *               selectivity �� ���� ���� qmoPredicate �� �ϳ��� �����Ͽ�
 *               (merge join�� �� �ϳ��� predicate�� ���ð���)
 *               qmoPredicate.totalSelectivity �� �����ϰ�
 *               qmoJoinMethodCost.joinPredicate (qmoPredInfo) �� ����
 *
 * Implementation :
 *
 *****************************************************************************/

    UInt            sDirection = 0;
    SDouble         sSelectivity;
    qmoPredInfo   * sPredInfo;
    qmoPredicate  * sPredicate;
    qmoPredicate  * sMergeJoinPred = NULL;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setJoinPredInfo4Merge::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement     != NULL );
    IDE_DASSERT( aJoinPredicate != NULL );
    IDE_DASSERT( aMethodCost    != NULL );

    //--------------------------------------
    // Merge join qmoPredInfo list ȹ��
    //--------------------------------------

    sSelectivity = 1;
    sPredicate = aJoinPredicate;

    if( ( aMethodCost->flag & QMO_JOIN_METHOD_DIRECTION_MASK )
        == QMO_JOIN_METHOD_DIRECTION_LEFTRIGHT )
    {
        sDirection = QMO_PRED_MERGE_LEFT_RIGHT;
    }
    else
    {
        sDirection = QMO_PRED_MERGE_RIGHT_LEFT;
    }

    // merge joinable predicate�̰�,
    // ���ڷ� �Ѿ�� join ���డ�ɹ����� predicate ��
    // selectivity�� ���� predicate�� ã�´�.
    while( sPredicate != NULL )
    {
        if( ( ( sPredicate->flag & QMO_PRED_MERGE_JOINABLE_MASK )
              == QMO_PRED_MERGE_JOINABLE_TRUE )
            &&
            ( ( sPredicate->flag & QMO_PRED_MERGE_DIRECTION_MASK
                & sDirection ) == sDirection ) )
        {
            // To Fix BUG-10978
            if ( sMergeJoinPred == NULL )
            {
                sMergeJoinPred = sPredicate;
            }
            else
            {
                // Nothing to do.
            }

            /* BUG-40589 floating point calculation */
            if (QMO_COST_IS_GREATER(sSelectivity, sPredicate->mySelectivity)
                == ID_TRUE)
            {
                sSelectivity = sPredicate->mySelectivity;
                sMergeJoinPred = sPredicate;
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
        sPredicate = sPredicate->next;
    }

    // merge join �� ����� qmoPredInfo ���� ����.
    if( sMergeJoinPred == NULL )
    {
        aMethodCost->joinPredicate = NULL;
    }
    else
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredInfo),
                                                 (void **)&sPredInfo )
                  != IDE_SUCCESS );

        sPredInfo->predicate = sMergeJoinPred;
        sPredInfo->next = NULL;
        sPredInfo->more = NULL;

        // qmoPredicate.totalSelectivity ȹ��
        sPredInfo->predicate->totalSelectivity =
            sPredInfo->predicate->mySelectivity;

        aMethodCost->joinPredicate = sPredInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmoJoinMethodMgr::setTempInfo( qcStatement  * aStatement,
                               qmoPredicate * aPredicate,
                               qcDepInfo    * aRightDepInfo,
                               UInt           aDirection )
{
/******************************************************************************
 *
 * Description : index nested loop, anti outer join qmoPredInfo list ȹ���
 *               �ʿ��� �ӽ����� ���� ( columnID, indexArgument )
 *
 * Implementation :
 *
 *****************************************************************************/

    qtcNode      * sCompareNode;
    qtcNode      * sColumnNode;
    qmoPredicate * sPredicate;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::setTempInfo::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aPredicate    != NULL );
    IDE_DASSERT( aRightDepInfo != NULL );

    //--------------------------------------
    // �ӽ����� ����( columnID, indexArguments )
    //--------------------------------------

    //--------------------------------------
    // �ӽ� ���� ����.
    // 1. ���ڷ� ���� right dependencies ������ columnID�� ����.
    //    ( �ε��� �÷��� ���ϱ� ����)
    // 2. ���� �÷��� ���� ��ǥ selectivity��
    //    �����ε����÷� ��뿩�ο� ���� ������ ���ϱ� ���� �ʿ��� ������
    //    indexArgument ���� ����.
    //    (1) index nested loop join
    //        one table predicate�� ������ �Լ� ��� (qmoPred::setTotal)
    //    (2) anti outer join
    //        left child graph�� ã�� ����.
    //--------------------------------------

    for( sPredicate = aPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        if ( ( ( sPredicate->flag & QMO_PRED_INDEX_DIRECTION_MASK & aDirection )
               == aDirection ) &&
             ( ( sPredicate->node->lflag & QTC_NODE_COLUMN_RID_MASK )
               != QTC_NODE_COLUMN_RID_EXIST ) )
        {
            if( ( sPredicate->node->node.lflag
                  & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
            }
            else
            {
                sCompareNode = sPredicate->node;
            }

            if( qtc::dependencyEqual(
                    & ((qtcNode *)(sCompareNode->node.arguments))->depInfo,
                    aRightDepInfo ) == ID_TRUE )
            {
                sCompareNode->indexArgument = 0;
                sColumnNode = (qtcNode *)(sCompareNode->node.arguments);
            }
            else
            {
                sCompareNode->indexArgument = 1;
                sColumnNode = (qtcNode *)(sCompareNode->node.arguments->next);
            }

            sPredicate->id
                = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[sColumnNode->node.table].
                columns[sColumnNode->node.column].column.id;
        }
        else
        {
            sPredicate->id = QMO_COLUMNID_NON_INDEXABLE;
        }
    }

    return IDE_SUCCESS;
}


IDE_RC
qmoJoinMethodMgr::findEqualSortColumn( qcStatement  * aStatement,
                                       qmoPredicate * aPredicate,
                                       qcDepInfo    * aRightDepInfo,
                                       qtcNode      * aSortColumn,
                                       qmoPredInfo  * aPredInfo )
{
/***********************************************************************
 *
 * Description : sort join qmoPredInfo list ȹ���,
 *               ���� sort column�� ������ �÷��� �����ϴ��� �˻�.
 *
 * Implementation :
 *
 *   ���ڷ� �Ѿ�� sort column�� ������ �÷��� ��� ã�Ƽ�,
 *   aPredInfo->more�� ���Ḯ��Ʈ�� �����Ѵ�.
 *   �÷��� total selectivity�� ���ؼ�,
 *   ù��° qmoPredicate->totalSelectivity�� �����Ѵ�.
 *
 ***********************************************************************/

    idBool          sIsEqual;
    qcDepInfo       sAndDependencies;
    SDouble         sSelectivity;
    qtcNode       * sCompareNode;
    qtcNode       * sSortColumnNode;
    qmoPredicate  * sPredicate;
    qmoPredInfo   * sPredInfo;
    qmoPredInfo   * sMorePredInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::findEqualSortColumn::__FT__" );

    //--------------------------------------
    // ���ռ� �˻�
    //--------------------------------------

    IDE_DASSERT( aStatement    != NULL );
    IDE_DASSERT( aPredicate    != NULL );
    IDE_DASSERT( aRightDepInfo != NULL );
    IDE_DASSERT( aSortColumn   != NULL );
    IDE_DASSERT( aPredInfo     != NULL );

    //--------------------------------------
    // ���� sort column�� ������ �÷��� ã�´�.
    //--------------------------------------

    sMorePredInfo = aPredInfo;
    sSelectivity  = sMorePredInfo->predicate->mySelectivity;

    for( sPredicate = aPredicate;
         sPredicate != NULL;
         sPredicate = sPredicate->next )
    {
        // PR-13286
        // ������ Column�̶� �ϴ��� Conversion�� �����Ѵٸ�,
        // �ϳ��� Column���� �����ϴ� Sort Join������ �ϳ��� ������
        // �����Ͽ� ó���� �� ����.
        // Ex) T1.int > T2.double + AND T1.int < T2.int
        if ( aSortColumn->node.conversion != NULL )
        {
            break;
        }
        else
        {
            // Go Go
        }

        if( ( ( sPredicate->flag & QMO_PRED_SORT_JOINABLE_MASK )
              == QMO_PRED_SORT_JOINABLE_TRUE )
            &&
            ( ( sPredicate->flag & QMO_PRED_SEL_PROCESS_MASK )
              == QMO_PRED_SEL_PROCESS_FALSE ) )
        {
            // right dependencies�� �ش��ϴ� ��带 ã�´�.
            if( ( sPredicate->node->node.lflag
                  & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                sCompareNode = (qtcNode *)(sPredicate->node->node.arguments);
            }
            else
            {
                sCompareNode = sPredicate->node;
            }

            // sort column�� ã�´�.
            qtc::dependencyAnd( aRightDepInfo,
                                & ((qtcNode *)(sCompareNode->node.arguments))->\
                                depInfo,
                                & sAndDependencies );

            if( qtc::dependencyEqual( & sAndDependencies,
                                      & qtc::zeroDependencies ) != ID_TRUE )
            {
                sSortColumnNode = (qtcNode *)(sCompareNode->node.arguments);
            }
            else
            {
                sSortColumnNode
                    = (qtcNode *)(sCompareNode->node.arguments->next);
            }

            // PR-13286
            // ������ Column�̶� �ϴ��� Conversion�� �����Ѵٸ�,
            // �ϳ��� Column���� �����ϴ� Sort Join������ �ϳ��� ������
            // �����Ͽ� ó���� �� ����.
            // Ex) T1.int > T2.int + AND T1.int < T2.double
            if ( sSortColumnNode->node.conversion != NULL )
            {
                sIsEqual = ID_FALSE;
            }
            else
            {
                IDE_TEST( qtc::isEquivalentExpression( aStatement,
                                                       aSortColumn,
                                                       sSortColumnNode,
                                                       &sIsEqual )
                          != IDE_SUCCESS );
            }

            if( sIsEqual == ID_TRUE )
            {
                sSelectivity *= sPredicate->mySelectivity;

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qmoPredInfo),
                                                         (void **)&sPredInfo )
                          != IDE_SUCCESS );

                sPredicate->flag &= ~QMO_PRED_SEL_PROCESS_MASK;
                sPredicate->flag |= QMO_PRED_SEL_PROCESS_TRUE;

                sPredInfo->predicate = sPredicate;
                sPredInfo->next = NULL;
                sPredInfo->more = NULL;

                sMorePredInfo->more = sPredInfo;
                sMorePredInfo = sMorePredInfo->more;
            }
            else
            {
                // Nothint To Do
            }
        }
        else
        {
            // Nothing To Do
        }
    }

    aPredInfo->predicate->totalSelectivity = sSelectivity;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qmoJoinMethodMgr::getJoinLateralDirection( qmgGraph                * aJoinGraph,
                                                  qmoJoinLateralDirection * aLateralDirection )
{
/***********************************************************************
 *
 * Description : PROJ-2418 Cross/Outer APPLY & Lateral View
 *
 *   ���� Join Graph�� Child Graph ���� ���� �����ϴ���,
 *   �׷��ٸ� �����ϴ� Graph�� LEFT���� RIGHT������ ��Ÿ����
 *   Lateral Position�� ��ȯ�Ѵ�.
 *
 * Implementation :
 *
 *  - Child Graph�� LateralDepInfo�� �����´�.
 *  - �� ���� depInfo�� �ٸ� ���� lateralDepInfo�� AND �Ѵ�.
 *  - AND�� ����� �����ϸ�, lateralDepInfo�� ���� ���� ������ �ϰ� �ִ� ��.
 *
 ***********************************************************************/

    qcDepInfo   sDepInfo;
    qcDepInfo   sLeftLateralDepInfo;
    qcDepInfo   sRightLateralDepInfo;

    IDU_FIT_POINT_FATAL( "qmoJoinMethodMgr::getJoinLateralDirection::__FT__" );

    // Child Graph�� Lateral DepInfo ȹ��
    IDE_TEST( qmg::getGraphLateralDepInfo( aJoinGraph->left,
                                           & sLeftLateralDepInfo )
              != IDE_SUCCESS );

    IDE_TEST( qmg::getGraphLateralDepInfo( aJoinGraph->right,
                                           & sRightLateralDepInfo )
              != IDE_SUCCESS );


    qtc::dependencyAnd( & sLeftLateralDepInfo,
                        & aJoinGraph->right->depInfo,
                        & sDepInfo );

    if ( qtc::haveDependencies( & sDepInfo ) == ID_TRUE )
    {
        // LEFT
        *aLateralDirection = QMO_JOIN_LATERAL_LEFT;
    }
    else
    {
        qtc::dependencyAnd( & sRightLateralDepInfo,
                            & aJoinGraph->left->depInfo,
                            & sDepInfo );

        if ( qtc::haveDependencies( & sDepInfo ) == ID_TRUE )
        {
            // RIGHT
            *aLateralDirection = QMO_JOIN_LATERAL_RIGHT;
        }
        else
        {
            // �ܺ� ������ �̷������ �ʰ� ����
            *aLateralDirection = QMO_JOIN_LATERAL_NONE;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qmoJoinMethodMgr::adjustIndexCost( qcStatement      * aStatement,
                                        qmgGraph         * aRight,
                                        qmoIdxCardInfo   * aIndexCardInfo,
                                        qmoPredInfo      * aJoinPredInfo,
                                        SDouble          * aMemCost,
                                        SDouble          * aDiskCost )
{
    UInt             sCount;
    UInt             sColCount;
    UInt             sIdxColumnID;
    qmoPredicate   * sPredicate = NULL;
    qmoPredInfo    * sPredInfo;
    mtcColumn      * sColumns;
    qmoColCardInfo * sColCardInfo;
    qmoStatistics  * sStatistics;
    SDouble          sKeyRangeCost        = 0.0;
    SDouble          sRightFilterCost     = 0.0;
    SDouble          sKeyRangeSelectivity = 1.0;
    SDouble          sNDV = 1.0;
    qmoPredWrapper   sKeyRange;
    idBool           sFind;

    sColumns    = QC_SHARED_TMPLATE(aStatement)->tmplate.rows[aRight->myFrom->tableRef->table].columns;
    sStatistics = aRight->myFrom->tableRef->statInfo;

    //--------------------------------------
    // right predicate �� �Ϻκ��� filter �� �ȴ�.
    //--------------------------------------
    sRightFilterCost = qmoCost::getFilterCost(
                            aStatement->mSysStat,
                            aRight->myPredicate,
                            1 );

    //--------------------------------------
    // join predicate �� �Ϻκ��� filter �� �ȴ�.
    //--------------------------------------
    for ( sPredInfo = aJoinPredInfo;
          sPredInfo != NULL;
          sPredInfo = sPredInfo->next )
    {
        sRightFilterCost += qmoCost::getFilterCost(
                            aStatement->mSysStat,
                            sPredInfo->predicate,
                            1 );
    }

    //--------------------------------------
    // join predicate, right predicate �� index �� Ż�� �ִ� ���� ã�´�.
    //--------------------------------------

    for ( sCount = 0; sCount < aIndexCardInfo->index->keyColCount; sCount++ )
    {
        sFind        = ID_FALSE;
        sNDV         = QMO_STAT_INDEX_KEY_NDV;
        sIdxColumnID = aIndexCardInfo->index->keyColumns[sCount].column.id;
        sColCardInfo = sStatistics->colCardInfo;

        // BUG-43161 keyColumn�� NDV�� �����ϴ� ��ġ�� ������
        for ( sColCount    = 0;
              sColCount < sStatistics->columnCnt;
              sColCount++ )
        {
            if ( sIdxColumnID == sColCardInfo[sColCount].column->column.id )
            {
                sNDV = sColCardInfo[sColCount].columnNDV;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        // join predicate �� �켱�Ѵ�.
        for ( sPredInfo = aJoinPredInfo;
              sPredInfo != NULL;
              sPredInfo = sPredInfo->next )
        {
            if( sIdxColumnID == sPredInfo->predicate->id )
            {
                sPredicate  = sPredInfo->predicate;
                sFind       = ID_TRUE;
                break;
            }
            else
            {
                // Nothing To Do
            }
        }

        if ( sFind == ID_FALSE )
        {
            for ( sPredicate = aRight->myPredicate;
                  sPredicate != NULL;
                  sPredicate = sPredicate->next )
            {
                if( sIdxColumnID == sPredicate->id )
                {
                    sFind = ID_TRUE;
                    break;
                }
                else
                {
                    // Nothing To Do
                }
            }
        }
        else
        {
            // Nothing To Do
        }

        if( sFind == ID_TRUE )
        {
            // BUG-42857
            // Index key column �� join predicate �� ���õ� ����
            // �̹� ������ join predicate �� totalSelectivity �� ����ϸ� �ȵ�
            sKeyRangeSelectivity *= 1 / sNDV;

            sKeyRange.pred = sPredicate;
            sKeyRange.prev = NULL;
            sKeyRange.next = NULL;

            sKeyRangeCost       += qmoCost::getKeyRangeCost(
                                        aStatement->mSysStat,
                                        aRight->myFrom->tableRef->statInfo,
                                        aIndexCardInfo,
                                        &sKeyRange,
                                        1.0 );

            sRightFilterCost    -= qmoCost::getFilterCost(
                                        aStatement->mSysStat,
                                        sPredicate,
                                        1 );

            // ���� index �÷��� ����Ҽ� ���� ���
            if( ( sPredicate->flag & QMO_PRED_NEXT_KEY_USABLE_MASK )
                == QMO_PRED_NEXT_KEY_UNUSABLE )
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
            // �´� �÷��� �����Ƿ� �� �˻��� �ʿ䰡 ����.
            break;
        }
    }

    IDE_DASSERT_MSG( sKeyRangeSelectivity >= 0 && sKeyRangeSelectivity <= 1,
                     "Selectivity : %"ID_DOUBLE_G_FMT"\n",
                     sKeyRangeSelectivity );

    //--------------------------------------
    // index_nl �� right �׷����� filter �� index �� output ��ŭ �ݺ� ����ȴ�.
    //--------------------------------------

    sRightFilterCost  = IDL_MAX( sRightFilterCost, 0.0 );
    sRightFilterCost *= (aRight->costInfo.inputRecordCnt * sKeyRangeSelectivity);

    //--------------------------------------
    // sKeyRangeCost ���� Selectivity�� ����ȵǾ� �ִ�.
    //--------------------------------------
    sKeyRangeCost    *= sKeyRangeSelectivity;

    //--------------------------------------
    // index cost ���
    //--------------------------------------

    (void) qmoCost::getIndexScanCost(
                        aStatement,
                        sColumns,
                        sPredicate,
                        aRight->myFrom->tableRef->statInfo,
                        aIndexCardInfo,
                        sKeyRangeSelectivity,
                        1,
                        sKeyRangeCost,
                        0,
                        sRightFilterCost,
                        aMemCost,
                        aDiskCost );

}
