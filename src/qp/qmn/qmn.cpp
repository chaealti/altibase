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
 * $Id: qmn.cpp 88800 2020-10-06 10:29:32Z hykim $
 *
 * Description :
 *     ���� Plan Node�� ���������� ����ϴ� ����� ������.
 *
 * ��� ���� :
 *
 * ��� :
 *
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <qtc.h>
#include <qcg.h>
#include <qmn.h>
#include <qmoUtil.h>

IDE_RC
qmn::printSubqueryPlan( qcTemplate   * aTemplate,
                        qtcNode      * aSubQuery,
                        ULong          aDepth,
                        iduVarString * aString,
                        qmnDisplay     aMode )
{
/***********************************************************************
 *
 * Description :
 *    �ش� Expression�� Predicate�� Subquery�� ���� ���,
 *    Subquery�� ���� Plan Tree ������ ����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    qtcNode * sNode;
    qmnPlan * sPlan;
    UInt      sArguCount;
    UInt      sArguNo;

    if ( ( aSubQuery->node.lflag & MTC_NODE_OPERATOR_MASK )
         == MTC_NODE_OPERATOR_SUBQUERY )
    {
        printSpaceDepth(aString, aDepth + 1);

        // BUG-47591
        if ( ( aMode == QMN_DISPLAY_ALL ) &&
             ( aTemplate->forceSubqueryCacheDisable == QTC_CACHE_FORCE_SUBQUERY_CACHE_NONE ) &&
             ( QCG_GET_SESSION_TRCLOG_DETAIL_INFORMATION( aTemplate->stmt ) == 1 ) &&
             ( aTemplate->cacheObjects != NULL ) &&
             ( aSubQuery->node.info != ID_UINT_MAX ) )
        {
            if ( ( aTemplate->cacheObjects[aSubQuery->node.info].mHitCnt  != 0 ) &&
                 ( aTemplate->cacheObjects[aSubQuery->node.info].mMissCnt != 0 ) )
            {
                iduVarStringAppendFormat( aString,
                                          "::SUB-QUERY BEGIN ( HIT_COUNT: %"ID_INT32_FMT", MISS_COUNT: %"ID_INT32_FMT" )\n",
                                          aTemplate->cacheObjects[aSubQuery->node.info].mHitCnt,
                                          aTemplate->cacheObjects[aSubQuery->node.info].mMissCnt );
            }
            else
            {
                // multiple row subquery
                iduVarStringAppend( aString, "::SUB-QUERY BEGIN\n" );
            }
        }
        else
        {
            iduVarStringAppend( aString, "::SUB-QUERY BEGIN\n" );
        }

        sPlan = aSubQuery->subquery->myPlan->plan;
        IDE_TEST( qmnPROJ::printPlan( aTemplate,
                                      sPlan,
                                      aDepth + 1,
                                      aString,
                                      aMode ) != IDE_SUCCESS );

        printSpaceDepth(aString, aDepth + 1);
        iduVarStringAppend( aString, "::SUB-QUERY END\n" );
    }
    else
    {
        sArguCount = ( aSubQuery->node.lflag & MTC_NODE_ARGUMENT_COUNT_MASK );

        if ( sArguCount > 0 ) // This node has arguments.
        {
            if ( (aSubQuery->node.lflag & MTC_NODE_LOGICAL_CONDITION_MASK )
                == MTC_NODE_LOGICAL_CONDITION_TRUE )
            {
                for (sNode = (qtcNode *)aSubQuery->node.arguments;
                     sNode != NULL;
                     sNode = (qtcNode *)sNode->node.next)
                {
                    IDE_TEST( printSubqueryPlan( aTemplate,
                                                 sNode,
                                                 aDepth,
                                                 aString,
                                                 aMode ) != IDE_SUCCESS );
                }
            }
            else
            {
                for (sArguNo = 0,
                         sNode = (qtcNode *)aSubQuery->node.arguments;
                     sArguNo < sArguCount && sNode != NULL;
                     sArguNo++,
                         sNode = (qtcNode *)sNode->node.next)
                {
                    IDE_TEST( printSubqueryPlan( aTemplate,
                                                 sNode,
                                                 aDepth,
                                                 aString,
                                                 aMode ) != IDE_SUCCESS );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmn::printResult( qcTemplate        * aTemplate,
                  ULong               aDepth,
                  iduVarString      * aString,
                  const qmcAttrDesc * aResultDesc )
{
    UInt i, j;
    const qmcAttrDesc * sItrAttr;

    for( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString, " " );
    }
    iduVarStringAppend( aString, "[ RESULT ]\n" );

    for( sItrAttr = aResultDesc;
         sItrAttr != NULL;
         sItrAttr = sItrAttr->next )
    {
        for( j = 0; j < aDepth; j++ )
        {
            iduVarStringAppend( aString, " " );
        }

        if( ( sItrAttr->flag & QMC_ATTR_KEY_MASK ) == QMC_ATTR_KEY_TRUE )
        {
            iduVarStringAppend( aString, "#" );
        }
        else
        {
            // Nothing to do.
        }

        // PROJ-2469 Optimize View Materialization
        // ������ �ʴ� Result�� ���ؼ� Plan�� ǥ���Ѵ�.
        if ( ( sItrAttr->flag & QMC_ATTR_USELESS_RESULT_MASK ) == QMC_ATTR_USELESS_RESULT_TRUE )
        {
            iduVarStringAppend( aString, "~" );
        }
        else
        {
            // Nothing to do.
        }
        
        IDE_TEST( qmoUtil::printPredInPlan( aTemplate,
                                            aString,
                                            0,
                                            (qtcNode *)sItrAttr->expr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmn::makeKeyRangeAndFilter( qcTemplate         * aTemplate,
                            qmnCursorPredicate * aPredicate )
{
/***********************************************************************
 *
 * Description :
 *    Cursor�� ���� ���� Key Range, Key Filter, Filter�� �����Ѵ�.
 *
 * Implementation :
 *    - Key Range ����
 *    - Key Filter ����
 *    - Filter ����
 *        - Variable Key Range, Variable Key Filter�� Filter��
 *          ���еǾ��� ��, �̸� �����Ͽ� �ϳ��� Filter�� �����Ѵ�.
 *    - IN SUBQUERY�� Key Range�� ���� ��� ������ ���� ������
 *      �����Ͽ� ó���Ѵ�.
 *        - Key Range ������ �����ϴ� ���� �� �̻� Key Range�� ���ٴ�
 *          �ǹ��̹Ƿ� �� �̻� record�� fetch�ؼ��� �ȵȴ�.
 *
 ***********************************************************************/

#define IDE_FN "qmn::makeKeyRangeAndFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    qtcNode     * sKeyRangeFilter;  // Variable Key Range�κ��� ������ Filter
    qtcNode     * sKeyFilterFilter; // Variable Key Filter�κ��� ������ Filter

    //----------------------------
    // ���ռ� �˻�
    //----------------------------

    IDE_DASSERT( aTemplate != NULL );
    IDE_DASSERT( aPredicate != NULL );

    IDE_DASSERT( aPredicate->filterCallBack != NULL );
    IDE_DASSERT( aPredicate->callBackDataAnd != NULL );
    IDE_DASSERT( aPredicate->callBackData != NULL );

    IDE_DASSERT( aPredicate->fixKeyRangeOrg == NULL ||
                 aPredicate->varKeyRangeOrg == NULL );
    IDE_DASSERT( aPredicate->fixKeyFilterOrg == NULL ||
                 aPredicate->varKeyRangeOrg == NULL );

    sKeyRangeFilter = NULL;
    sKeyFilterFilter = NULL;

    //---------------------------------------------------------------
    // Key Range�� ����
    //---------------------------------------------------------------

    IDE_TEST( makeKeyRange( aTemplate,
                            aPredicate,
                            & sKeyRangeFilter ) != IDE_SUCCESS );

    //---------------------------------------------------------------
    // Key Filter�� ����
    //---------------------------------------------------------------

    IDE_TEST( makeKeyFilter( aTemplate,
                             aPredicate,
                             & sKeyFilterFilter ) != IDE_SUCCESS );

    //---------------------------------------------------------------
    // Filter�� ����
    //    A4������ ������ ���� �� ������ Filter�� ������ �� �ִ�.
    //       - Variable Key Range => Filter
    //       - Variable Key Filter => Filter
    //       - Normal Filter => Filter
    //    ������ ���� Filter�� Cursor�� ���� ó������ �ʴ´�.
    //       - Constant Filter : ��ó����
    //       - Subquery Filter : ��ó����
    //---------------------------------------------------------------

    // �پ��� Filter�� Storage Manager���� ó���� �� �ֵ���
    // CallBack�� �����Ѵ�.
    IDE_TEST( makeFilter( aTemplate,
                          aPredicate,
                          sKeyRangeFilter,
                          sKeyFilterFilter,
                          aPredicate->filter ) != IDE_SUCCESS );

    //---------------------------------------------------------------
    //  Key Range �� Key Filter�� ����
    //      - Key Range�� ������ ��� Partial Key�� ����
    //      - Key Range�� ���� ��� default key range ���
    //      - Key Filter�� ���� ��� default key filter ���
    //---------------------------------------------------------------

    // Key Range ������ ������ ���, Default Key Range�� �����Ѵ�.
    if ( aPredicate->keyRange == NULL )
    {
        aPredicate->keyRange = smiGetDefaultKeyRange();
    }
    else
    {
        // Nothing To Do
    }

    // Key Filter ������ ������ ���, Default Key Filter�� �����Ѵ�.
    if ( aPredicate->keyFilter == NULL )
    {
        // Key Filter�� Key Range�� ������ ���¸� ���´�.
        aPredicate->keyFilter = smiGetDefaultKeyRange();
    }
    else
    {
        // Nothing To Do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmn::makeKeyRange( qcTemplate         * aTemplate,
                   qmnCursorPredicate * aPredicate,
                   qtcNode           ** aFilter )
{
/***********************************************************************
 *
 * Description :
 *     Key Range�� �����Ѵ�.
 *
 * Implementation :
 *     Key Range�� ����
 *       - Fixed Key Range�� ���, Key Range ����
 *       - Variable Key Range�� ���, Key Range ����
 *       - ���н�, Filter�� ó����.
 *       - Not Null Key Range�� �ʿ��� ���, Key Range ����
 *
 ***********************************************************************/
#define IDE_FN "qmn::makeKeyRange"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt              sCompareType;
    qtcNode         * sAndNode;
    mtdBooleanType    sValue;

    if ( aPredicate->fixKeyRangeOrg != NULL )
    {
        //------------------------------------
        // Fixed Key Range�� �����ϴ� ���
        //------------------------------------

        IDE_DASSERT( aPredicate->index != NULL );
        IDE_DASSERT( aPredicate->fixKeyRangeArea != NULL );

        // PROJ-1872
        if ( ( aTemplate->tmplate.rows[aPredicate->tupleRowID].lflag
               & MTC_TUPLE_STORAGE_MASK )
             == MTC_TUPLE_STORAGE_DISK )
        {
            // Disk Table�� Key Range��
            // Stored Ÿ���� Į�� value�� Mt Ÿ���� value����
            // compare 
            sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
        }
        else
        {
            /* BUG-43006 FixedTable Indexing Filter
             * FixedTable�� Index���� indexHandle�� ���� �� �ִ�
             */
            if ( aPredicate->index->indexHandle != NULL )
            {
                /*
                 * PROJ-2433
                 * Direct Key Index�� ���� key compare �Լ� type ����
                 */
                if ( ( smiTable::getIndexInfo( aPredicate->index->indexHandle ) &
                     SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
                {
                    sCompareType = MTD_COMPARE_INDEX_KEY_MTDVAL;
                }
                else
                {
                    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
                }
            }
            else
            {
                sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
            }
        }

        IDE_TEST(
            qmoKeyRange::makeKeyRange( aTemplate,
                                       aPredicate->fixKeyRangeOrg,
                                       aPredicate->index->keyColCount,
                                       aPredicate->index->keyColumns,
                                       aPredicate->index->keyColsFlag,
                                       sCompareType,
                                       aPredicate->fixKeyRangeArea,
                                       aPredicate->fixKeyRangeSize,
                                       &(aPredicate->fixKeyRange),
                                       aFilter )
            != IDE_SUCCESS );

        // host ������ �����Ƿ� fixed key�� ���� �����ϴ� ���� ����.
        IDE_DASSERT( *aFilter == NULL );
        
        aPredicate->keyRange = aPredicate->fixKeyRange;
    }
    else
    {
        if ( aPredicate->varKeyRangeOrg != NULL )
        {
            //------------------------------------
            // Variable Key Range�� �����ϴ� ���
            //------------------------------------

            IDE_DASSERT( aPredicate->index != NULL );
            IDE_DASSERT( aPredicate->varKeyRangeArea != NULL );

            // PROJ-1872
            if ( ( aTemplate->tmplate.rows[aPredicate->tupleRowID].lflag
                   & MTC_TUPLE_STORAGE_MASK )
                 == MTC_TUPLE_STORAGE_DISK )
            {
                // Disk Table�� Key Range��
                // Stored Ÿ���� Į�� value�� Mt Ÿ���� value����
                // compare 
                sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
            }
            else
            {
                /* BUG-43006 FixedTable Indexing Filter
                 * FixedTable�� Index���� indexHandle�� ���� �� �ִ�
                 */
                if ( aPredicate->index->indexHandle != NULL )
                {
                    /*
                     * PROJ-2433
                     * Direct Key Index�� ���� key compare �Լ� type ����
                     */
                    if ( ( smiTable::getIndexInfo( aPredicate->index->indexHandle ) &
                         SMI_INDEX_DIRECTKEY_MASK ) == SMI_INDEX_DIRECTKEY_TRUE )
                    {
                        sCompareType = MTD_COMPARE_INDEX_KEY_MTDVAL;
                    }
                    else
                    {
                        sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
                    }
                }
                else
                {
                    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
                }
            }

            // BUG-39036 select * from t1 where :emp is null or i1 = :emp;
            // ������� �����ϴ� keyRange �� ��������� ������ ���ؼ� �����Ѵ�.
            // ������� true �̸� keyRange �� ����
            for ( sAndNode = (qtcNode*)(aPredicate->varKeyRangeOrg->node.arguments);
                  sAndNode != NULL;
                  sAndNode = (qtcNode *)(sAndNode->node.next) )
            {
                if( qtc::haveDependencies( &sAndNode->depInfo ) == ID_FALSE )
                {
                    IDE_TEST( qtc::calculate( (qtcNode*)(sAndNode->node.arguments),
                                              aTemplate )
                              != IDE_SUCCESS );

                    sValue  = *(mtdBooleanType*)(aTemplate->tmplate.stack->value);

                    if ( sValue == MTD_BOOLEAN_TRUE )
                    {
                        aPredicate->varKeyRange           = NULL;
                        aPredicate->varKeyRangeOrg        = NULL;
                        aPredicate->varKeyRange4FilterOrg = NULL;
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

            if ( aPredicate->varKeyRangeOrg != NULL )
            {
                IDE_TEST(
                    qmoKeyRange::makeKeyRange( aTemplate,
                                            aPredicate->varKeyRangeOrg,
                                            aPredicate->index->keyColCount,
                                            aPredicate->index->keyColumns,
                                            aPredicate->index->keyColsFlag,
                                            sCompareType,
                                            aPredicate->varKeyRangeArea,
                                            aPredicate->varKeyRangeSize,
                                            &(aPredicate->varKeyRange),
                                            aFilter )
                    != IDE_SUCCESS );

                if ( *aFilter != NULL )
                {
                    aPredicate->keyRange = NULL;

                    // Key Range ������ ������ ��� �̸�
                    // ������ �� Fix Key Range�� ���
                    *aFilter = aPredicate->varKeyRange4FilterOrg;
                }
                else
                {
                    // Key Range ������ ������ ���

                    aPredicate->keyRange = aPredicate->varKeyRange;
                }
            }
            else
            {
                aPredicate->keyRange = NULL;
            }
        }
        else
        {
            //----------------------------------
            // NOT NULL RANGE ����
            //----------------------------------    

            // Not Null Ranage ����
            // (1) indexable min-max�ε�, keyrange�� ���� ���
            // (2) Merge Join ������ SCAN �� ��� ( To Fix BUG-8747 )
            if ( (aPredicate->notNullKeyRange != NULL) &&
                 (aPredicate->index != NULL) )
            {    
                IDE_TEST( qmoKeyRange::makeNotNullRange( aPredicate,
                                                         aPredicate->index->keyColumns,
                                                         aPredicate->notNullKeyRange )
                          != IDE_SUCCESS );
                
                aPredicate->keyRange = aPredicate->notNullKeyRange;
            }
            else
            {
                //------------------------------------
                // Key Range�� ���� ���
                //------------------------------------
                
                aPredicate->keyRange = NULL;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmn::makeKeyFilter( qcTemplate         * aTemplate,
                    qmnCursorPredicate * aPredicate,
                    qtcNode           ** aFilter )
{
/***********************************************************************
 *
 * Description :
 *     Key Filter�� �����Ѵ�.
 *
 * Implementation :
 *     Key Filter�� ����
 *       - Key Range�� �����ϴ� ��쿡�� ��ȿ�ϴ�.
 *       - Fixed Key Filter�� ���, Key Filter ����
 *       - Variable Key Filter�� ���, Key Filter ����
 *
 ***********************************************************************/
#define IDE_FN "qmn::makeKeyFilter"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    UInt              sCompareType;
    qtcNode         * sAndNode;
    mtdBooleanType    sValue;

    if ( aPredicate->keyRange == NULL )
    {
        //------------------------------
        // Key Range�� ���� ���
        //------------------------------

        // ���ռ� �˻�
        // key range�� ���ų� �����ϴ� �����,
        // Fixed Key Filter�� ������ �� ����.
        IDE_DASSERT( aPredicate->fixKeyFilterOrg == NULL );
        
        aPredicate->keyFilter = NULL;        
        *aFilter = aPredicate->varKeyFilter4FilterOrg;
    }
    else
    {
        //------------------------------
        // Key Range �� �����ϴ� ���
        //------------------------------

        if ( aPredicate->fixKeyFilterOrg != NULL )
        {
            //------------------------------
            // Fixed Key Filter�� �����ϴ� ���
            //------------------------------

            // ���ռ� �˻�
            // Fixed Key Filter�� Variable Key Range�� ������ �� ����.
            IDE_DASSERT( aPredicate->varKeyRangeOrg == NULL );
            
            IDE_DASSERT( aPredicate->index != NULL );
            IDE_DASSERT( aPredicate->fixKeyFilterArea != NULL );

            // PROJ-1872
            if ( ( aTemplate->tmplate.rows[aPredicate->tupleRowID].lflag
                   & MTC_TUPLE_STORAGE_MASK )
                 == MTC_TUPLE_STORAGE_DISK )
            {
                // Disk Table�� Key Range��
                // Stored Ÿ���� Į�� value�� Mt Ÿ���� value����
                // compare 
                sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
            }
            else
            {
                sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
            }

            IDE_TEST(
                qmoKeyRange::makeKeyFilter(
                    aTemplate,
                    aPredicate->fixKeyFilterOrg,
                    aPredicate->index->keyColCount,
                    aPredicate->index->keyColumns,
                    aPredicate->index->keyColsFlag,
                    sCompareType,
                    aPredicate->fixKeyFilterArea,
                    aPredicate->fixKeyFilterSize,
                    &(aPredicate->fixKeyFilter),
                    aFilter )
                != IDE_SUCCESS );

            // host ������ �����Ƿ� fixed key�� ���� �����ϴ� ���� ����.
            IDE_DASSERT( *aFilter == NULL );
            
            aPredicate->keyFilter = aPredicate->fixKeyFilter;
        }
        else
        {
            if ( aPredicate->varKeyFilterOrg != NULL )
            {
                //------------------------------
                // Variable Key Filter�� �����ϴ� ���
                //------------------------------

                IDE_DASSERT( aPredicate->index != NULL );
                IDE_DASSERT( aPredicate->varKeyFilterArea != NULL );

                // PROJ-1872
                if ( ( aTemplate->tmplate.rows[aPredicate->tupleRowID].lflag
                       & MTC_TUPLE_STORAGE_MASK )
                     == MTC_TUPLE_STORAGE_DISK )
                {
                    // Disk Table�� Key Range��
                    // Stored Ÿ���� Į�� value�� Mt Ÿ���� value����
                    // compare 
                    sCompareType = MTD_COMPARE_STOREDVAL_MTDVAL;
                }
                else
                {
                    sCompareType = MTD_COMPARE_MTDVAL_MTDVAL;
                }

                // BUG-48076 disk table�� with ���� ��� �� or ���� ���յ� constant predicate�� ������ ��� ��� ���� 
                // ������� �����ϴ� keyFilter �� ��������� ������ ���ؼ� �����Ѵ�.
                // ������� true �̸� keyFilter �� ����
                for ( sAndNode = (qtcNode*)(aPredicate->varKeyFilterOrg->node.arguments);
                      sAndNode != NULL;
                      sAndNode = (qtcNode *)(sAndNode->node.next) )
                {
                    if( qtc::haveDependencies( &sAndNode->depInfo ) == ID_FALSE )
                    {
                        IDE_TEST( qtc::calculate( (qtcNode*)(sAndNode->node.arguments),
                                                  aTemplate )
                                  != IDE_SUCCESS );

                        sValue  = *(mtdBooleanType*)(aTemplate->tmplate.stack->value);

                        if ( sValue == MTD_BOOLEAN_TRUE )
                        {
                            aPredicate->varKeyFilter           = NULL;
                            aPredicate->varKeyFilterOrg        = NULL;
                            aPredicate->varKeyFilter4FilterOrg = NULL;
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

                if ( aPredicate->varKeyFilterOrg != NULL )
                {
                    IDE_TEST(
                        qmoKeyRange::makeKeyFilter(
                            aTemplate,
                            aPredicate->varKeyFilterOrg,
                            aPredicate->index->keyColCount,
                            aPredicate->index->keyColumns,
                            aPredicate->index->keyColsFlag,
                            sCompareType,
                            aPredicate->varKeyFilterArea,
                            aPredicate->varKeyFilterSize,
                            &(aPredicate->varKeyFilter),
                            aFilter )
                        != IDE_SUCCESS );

                    if ( *aFilter != NULL )
                    {
                        // Key Filter ���� ����
                        aPredicate->keyFilter = NULL;

                        // Key Filter ������ ������ ��� �̸�
                        // ������ �� Fix Key Filter�� ���
                        *aFilter = aPredicate->varKeyFilter4FilterOrg;
                    }
                    else
                    {
                        // Key Filter ���� ����
                        aPredicate->keyFilter = aPredicate->varKeyFilter;
                    }
                }
                else
                {
                    aPredicate->keyFilter = NULL;
                }
            }
            else
            {
                //------------------------------
                // Key Filter�� ���� ���
                //------------------------------

                aPredicate->keyFilter = NULL;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qmn::makeFilter( qcTemplate         * aTemplate,
                 qmnCursorPredicate * aPredicate,
                 qtcNode            * aFirstFilter,
                 qtcNode            * aSecondFilter,
                 qtcNode            * aThirdFilter )
{
/***********************************************************************
 *
 * Description :
 *    ���� Filter�� �����Ͽ� Storage Manager���� ����� �� �ֵ���
 *    CallBack �� ������ �ش�.
 * Implementation :
 *    �Էµ� Filter�� NULL�� �ƴ� ���, CallBackData�� �����ϰ�
 *    ���������� CallBackAnd�� �����Ѵ�.
 *
 ***********************************************************************/

    UInt   sFilterCnt;
    idBool sIsOnlyThirdFilter = ID_TRUE; /* PROJ-2632 */

    if ( (aFirstFilter == NULL) &&
         (aSecondFilter == NULL) &&
         (aThirdFilter == NULL) )
    {
        //---------------------------------
        // Filter�� ���� ���
        //---------------------------------
        // Default Filter�� ����
        idlOS::memcpy( aPredicate->filterCallBack,
                       smiGetDefaultFilter(),
                       ID_SIZEOF(smiCallBack) );
    }
    else
    {
        //---------------------------------
        // Filter�� �����ϴ� ���
        //     - �����ϴ� �� Filter���� CallBack Data�� �����ϰ�,
        //     - ������ CallBack Data�� ����
        //     - ���������� CallBack�� �����Ѵ�.
        //---------------------------------

        // Filter�� �����ϴ� ��� CallBack Data�� �����Ѵ�.
        sFilterCnt = 0;
        if ( aFirstFilter != NULL )
        {
            qtc::setSmiCallBack( & aPredicate->callBackData[sFilterCnt],
                                 aFirstFilter,
                                 aTemplate,
                                 aPredicate->tupleRowID );
            sFilterCnt++;

            sIsOnlyThirdFilter = ID_FALSE; /* PROJ-2632 */
        }
        else
        {
            // Nothing to do.
        }

        if ( aSecondFilter != NULL )
        {
            qtc::setSmiCallBack( & aPredicate->callBackData[sFilterCnt],
                                 aSecondFilter,
                                 aTemplate,
                                 aPredicate->tupleRowID );
            sFilterCnt++;

            sIsOnlyThirdFilter = ID_FALSE; /* PROJ-2632 */
        }
        else
        {
            // Nothing to do.
        }

        if ( aThirdFilter != NULL )
        {
            qtc::setSmiCallBack( & aPredicate->callBackData[sFilterCnt],
                                 aThirdFilter,
                                 aTemplate,
                                 aPredicate->tupleRowID );
            sFilterCnt++;
        }
        else
        {
            sIsOnlyThirdFilter = ID_FALSE; /* PROJ-2632 */
        }

        // CallBack Data���� �����Ѵ�.
        switch ( sFilterCnt )
        {
            case 1:
                {
                    // BUG-12514 fix
                    // Filter�� �ϳ��� ���� callBackDataAnd�� ������ �ʰ�
                    // �ٷ� callBackData�� �Ѱ��ش�.
                    break;
                }
            case 2:
                {
                    // 2���� CallBack Data�� ����
                    qtc::setSmiCallBackAnd( aPredicate->callBackDataAnd,
                                            & aPredicate->callBackData[0],
                                            & aPredicate->callBackData[1],
                                            NULL );
                    break;
                }
            case 3:
                {
                    // 3���� CallBack Data�� ����
                    qtc::setSmiCallBackAnd( aPredicate->callBackDataAnd,
                                            & aPredicate->callBackData[0],
                                            & aPredicate->callBackData[1],
                                            & aPredicate->callBackData[2] );
                    break;
                }
            default :
                {
                    IDE_DASSERT(0);
                    break;
                }
        } // end of switch

        // BUG-12514 fix
        // Filter�� �ϳ��� ���� callBackDataAnd�� ������ �ʰ�
        // �ٷ� callBackData�� �Ѱ��ش�.
        if( sFilterCnt > 1 )
        {
            aPredicate->filterCallBack->callback = qtc::smiCallBackAnd;
            aPredicate->filterCallBack->data = aPredicate->callBackDataAnd;
        }
        else
        {
            aPredicate->filterCallBack->callback = qtc::smiCallBack;
            aPredicate->filterCallBack->data = aPredicate->callBackData;
        }

        /* PROJ-2632 */
        if ( sIsOnlyThirdFilter == ID_TRUE )
        {
            if ( mtx::generateSerialExecute( &( aTemplate->tmplate ),
                                             aPredicate->mSerialFilterInfo,
                                             aPredicate->mSerialExecuteData,
                                             aPredicate->tupleRowID )
                 == IDE_SUCCESS )
            {
                aPredicate->filterCallBack->callback = mtx::doSerialFilterExecute;
                aPredicate->filterCallBack->data     = aPredicate->mSerialExecuteData;
            }
            else
            {
                aPredicate->mSerialExecuteData = NULL;
            }
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;
}

IDE_RC
qmn::getReferencedTableInfo( qmnPlan     * aPlan,
                             const void ** aTableHandle,
                             smSCN       * aTableSCN,
                             idBool      * aIsFixedTable )
{
    qmncSCAN * sScan;
    qmncCUNT * sCunt;

    if( aPlan->type == QMN_SCAN )
    {
        sScan = ( qmncSCAN * )aPlan;

        *aTableHandle = sScan->table;
        *aTableSCN    = sScan->tableSCN;

        if ( ( sScan->flag & QMNC_SCAN_TABLE_FV_MASK )
             == QMNC_SCAN_TABLE_FV_TRUE )
        {
            *aIsFixedTable = ID_TRUE;
        }
        else
        {
            *aIsFixedTable = ID_FALSE;
        }
    }
    else if( aPlan->type == QMN_CUNT )
    {
        sCunt = ( qmncCUNT * )aPlan;
        *aTableHandle = sCunt->tableRef->tableInfo->tableHandle;
        *aTableSCN    = sCunt->tableSCN;

        if ( ( sCunt->flag & QMNC_CUNT_TABLE_FV_MASK )
             == QMNC_CUNT_TABLE_FV_TRUE )
        {
            *aIsFixedTable = ID_TRUE;
        }
        else
        {
            *aIsFixedTable = ID_FALSE;
        }
    }
    else
    {
        IDE_RAISE( ERR_INVALID_PLAN_TYPE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_PLAN_TYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qmn::getReferencedTableInfo",
                                  "Invalid plan type" ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmn::notifyOfSelectedMethodSet( qcTemplate * aTemplate, qmnPlan * aPlan )
{
    if( aPlan->type == QMN_SCAN )
    {
        IDE_TEST( qmnSCAN::notifyOfSelectedMethodSet( aTemplate,
                                                      (qmncSCAN*)aPlan )
                  != IDE_SUCCESS );
    }
    else if( aPlan->type == QMN_CUNT )
    {
        IDE_TEST( qmnCUNT::notifyOfSelectedMethodSet( aTemplate,
                                                      (qmncCUNT*)aPlan )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( 0 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC
qmn::makeNullRow( mtcTuple   * aTuple,
                  void       * aNullRow )
{
/***********************************************************************
 *
 * Description : PROJ-1705 
 *
 *    ��ũ���̺��� ���, null row�� sm���� �������� �ʰ�,
 *    qp���� �ʿ��� ������ null row�� ����/�����صΰ� �̸� ���
 * 
 * Implementation :
 *
 *
 ***********************************************************************/

    UShort            sColumnCnt;
    mtcColumn       * sColumn;
    UChar           * sColumnPtr;
    smcTableHeader  * sTableHeader;

    //----------------------------------------------
    // PROJ_1705_PEH_TODO
    // sm���κ��� �ʿ��� �÷��� ����Ÿ�� qp�޸𸮿������� �����ϰ� �Ǵµ�,
    // �� qp �޸𸮿����� �ʿ��� �÷��� ���� ������ �Ҵ�ް� �ڵ������
    // �� �κе� �ڵ庯��Ǿ�� ��.
    //----------------------------------------------    

    for( sColumnCnt = 0; sColumnCnt < aTuple->columnCount; sColumnCnt++ )
    {
        sColumn = &(aTuple->columns[sColumnCnt]);

        // PROJ-2429 Dictionary based data compress for on-disk DB
        if ( (sColumn->column.flag & SMI_COLUMN_COMPRESSION_MASK) 
             == SMI_COLUMN_COMPRESSION_TRUE )
        {
            sColumnPtr = (UChar*)aNullRow + sColumn->column.offset;

            sTableHeader = 
                (smcTableHeader*)SMI_MISC_TABLE_HEADER(smiGetTable( sColumn->column.mDictionaryTableOID ));

            IDE_DASSERT( sTableHeader->mNullOID != SM_NULL_OID );

            idlOS::memcpy( sColumnPtr, &sTableHeader->mNullOID, ID_SIZEOF(smOID));
        }
        else
        {
            sColumnPtr = (UChar *)mtc::value( sColumn,
                                              aNullRow,
                                              MTD_OFFSET_USE ); 

            sColumn->module->null( sColumn, 
                                   sColumnPtr ); 
        }
    }

    return IDE_SUCCESS;

}

void qmn::printMTRinfo( iduVarString * aString,
                        ULong          aDepth,
                        qmcMtrNode   * aMtrNode,
                        const SChar  * aMtrName,
                        UShort         aSelfID,
                        UShort         aRefID1,
                        UShort         aRefID2 )
{
/***********************************************************************
 *
 * Description : PROJ-2242
 *    mtr node info �� ����Ѵ�.
 *
 ***********************************************************************/

    ULong i;
    UInt  j;

    // BUG-37245
    // tuple id type �� UShort �� �����ϵ� default ��°��� -1 �� �����Ѵ�.
    SInt  sNonMtrId = QMN_PLAN_PRINT_NON_MTR_ID;

    qmcMtrNode  * sMtrNode;

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString, " " );
    }

    iduVarStringAppendFormat( aString,
                              "[ %s NODE INFO, "
                              "SELF: %"ID_INT32_FMT", "
                              "REF1: %"ID_INT32_FMT", "
                              "REF2: %"ID_INT32_FMT" ]\n",
                              aMtrName,
                              ( aSelfID == ID_USHORT_MAX )? sNonMtrId: (SInt)aSelfID,
                              ( aRefID1 == ID_USHORT_MAX )? sNonMtrId: (SInt)aRefID1,
                              ( aRefID2 == ID_USHORT_MAX )? sNonMtrId: (SInt)aRefID2 );

    for( sMtrNode = aMtrNode, j = 0;
         sMtrNode != NULL;
         sMtrNode = sMtrNode->next, j++ )
    {
        for ( i = 0; i < aDepth; i++ )
        {
            iduVarStringAppend( aString, " " );
        }

        switch( sMtrNode->flag & QMC_MTR_TYPE_MASK )
        {
            case QMC_MTR_TYPE_DISK_TABLE:
                iduVarStringAppendFormat(
                    aString,
                    "sMtrNode[%"ID_INT32_FMT"] : src[%"ID_INT32_FMT", ROWID],"
                    "dst[%"ID_INT32_FMT", %"ID_INT32_FMT"]\n",
                    j, 
                    ( sMtrNode->srcNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.table,
                    ( sMtrNode->dstNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.table,
                    ( sMtrNode->dstNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.column
                    );
                break;

            case QMC_MTR_TYPE_MEMORY_TABLE:
                iduVarStringAppendFormat(
                    aString,
                    "sMtrNode[%"ID_INT32_FMT"] : src[%"ID_INT32_FMT", ROWPTR],"
                    "dst[%"ID_INT32_FMT", %"ID_INT32_FMT"]\n",
                    j, 
                    ( sMtrNode->srcNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.table,
                    ( sMtrNode->dstNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.table,
                    ( sMtrNode->dstNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.column
                    );
                break;

            case QMC_MTR_TYPE_MEMORY_KEY_COLUMN:
            case QMC_MTR_TYPE_MEMORY_PARTITION_KEY_COLUMN:
                iduVarStringAppendFormat(
                    aString,
                    "sMtrNode[%"ID_INT32_FMT"] : src[%"ID_INT32_FMT", *%"ID_INT32_FMT"],"
                    "dst[%"ID_INT32_FMT", %"ID_INT32_FMT"]\n",
                    j, 
                    ( sMtrNode->srcNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.table,
                    ( sMtrNode->srcNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.column,
                    ( sMtrNode->dstNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.table,
                    ( sMtrNode->dstNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.column
                    );
                break;

            default:
                iduVarStringAppendFormat(
                    aString,
                    "sMtrNode[%"ID_INT32_FMT"] : src[%"ID_INT32_FMT", %"ID_INT32_FMT"],"
                    "dst[%"ID_INT32_FMT", %"ID_INT32_FMT"]\n",
                    j, 
                    ( sMtrNode->srcNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.table,
                    ( sMtrNode->srcNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->srcNode->node.column,
                    ( sMtrNode->dstNode->node.table  == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.table,
                    ( sMtrNode->dstNode->node.column == ID_USHORT_MAX )? sNonMtrId: (SInt)sMtrNode->dstNode->node.column
                    );
                break;
        }
    }
}

void qmn::printJoinMethod( iduVarString * aString,
                           UInt           aQmnFlag )
{
/***********************************************************************
 *
 * Description : PROJ-2242
 *   JoinMethod�� ����Ѵ�.
 *
 ***********************************************************************/

    switch( (aQmnFlag & QMN_PLAN_JOIN_METHOD_TYPE_MASK) )
    {
        case QMN_PLAN_JOIN_METHOD_FULL_NL :
            iduVarStringAppend( aString,
                                "METHOD: NL" );
            break;
        case QMN_PLAN_JOIN_METHOD_INDEX :
        case QMN_PLAN_JOIN_METHOD_INVERSE_INDEX :
            iduVarStringAppend( aString,
                                "METHOD: INDEX_NL" );
            break;
        case QMN_PLAN_JOIN_METHOD_FULL_STORE_NL :
            iduVarStringAppend( aString,
                                "METHOD: STORE_NL" );
            break;
        case QMN_PLAN_JOIN_METHOD_ANTI :
            iduVarStringAppend( aString,
                                "METHOD: ANTI" );
            break;
        case QMN_PLAN_JOIN_METHOD_ONE_PASS_HASH :
        case QMN_PLAN_JOIN_METHOD_TWO_PASS_HASH :
        case QMN_PLAN_JOIN_METHOD_INVERSE_HASH  :
            iduVarStringAppend( aString,
                                "METHOD: HASH" );
            break;
        case QMN_PLAN_JOIN_METHOD_ONE_PASS_SORT :
        case QMN_PLAN_JOIN_METHOD_TWO_PASS_SORT :
        case QMN_PLAN_JOIN_METHOD_INVERSE_SORT  :
            iduVarStringAppend( aString,
                                "METHOD: SORT" );
            break;
        case QMN_PLAN_JOIN_METHOD_MERGE :
            iduVarStringAppend( aString,
                                "METHOD: MERGE" );
            break;
        default :
            IDE_DASSERT(0);
            break;
    }
}

void qmn::printCost( iduVarString * aString,
                     SDouble        aCost )
{
/***********************************************************************
 *
 * Description : PROJ-2242
 *   cost info�� ����Ѵ�.
 *
 ***********************************************************************/

    if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        iduVarStringAppendFormat( aString,
                                  ", COST: %.2f )\n",
                                  aCost );
    }
    else
    {
        iduVarStringAppend( aString,
                            ", COST: BLOCKED )\n" );
    }
}

void qmn::printSpaceDepth(iduVarString* aString, ULong aDepth)
{
    ULong i;

    i = 0;
    while (i + 8 <= aDepth)
    {
        iduVarStringAppendLength(aString, "        ", 8);
        i += 8;
    }
    while (i + 4 <= aDepth)
    {
        iduVarStringAppendLength(aString, "    ", 4);
        i += 4;
    }
    while (i < aDepth)
    {
        iduVarStringAppendLength(aString, " ", 1);
        i++;
    }
}

void qmn::printResultCacheRef( iduVarString    * aString,
                               ULong             aDepth,
                               qcComponentInfo * aComInfo )
{
    ULong   sDepth = 0;
    UInt    i      = 0;

    if ( aComInfo->count > 0 )
    {
        for ( sDepth = 0; sDepth < aDepth; sDepth++ )
        {
            iduVarStringAppend( aString, " " );
        }

        iduVarStringAppendFormat( aString,
                                  "[ RESULT CACHE REF: " );
        for ( i = 0; i < aComInfo->count - 1; i++ )
        {
            iduVarStringAppendFormat( aString,
                                      "%"ID_INT32_FMT",", aComInfo->components[i] );
        }

        iduVarStringAppendFormat( aString,
                                  "%"ID_INT32_FMT" ]\n", aComInfo->components[i] );
    }
    else
    {
        /* Nothing to do */
    }
}

void qmn::printResultCacheInfo( iduVarString    * aString,
                                ULong             aDepth,
                                qmnDisplay        aMode,
                                idBool            aIsInit,
                                qmndResultCache * aResultData )
{
    ULong         i = 0;
    const SChar * sString[3] = {"INIT", "HIT", "MISS"};
    SChar       * sStatus    = NULL;

    if ( aIsInit == ID_TRUE )
    {
        IDE_TEST_CONT( aResultData->memory == NULL, normal_exit );
    }
    else
    {
        /* Nothing to do */
    }

    for ( i = 0; i < aDepth; i++ )
    {
        iduVarStringAppend( aString, " " );
    }

    if ( aMode == QMN_DISPLAY_ALL )
    {
        if ( aIsInit == ID_TRUE )
        {
            sStatus = (SChar *)sString[aResultData->status];
            if ( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
            {
                iduVarStringAppendFormat( aString,
                                          "[ RESULT CACHE HIT: %"ID_INT32_FMT", "
                                          "MISS: %"ID_INT32_FMT", "
                                          "SIZE: %"ID_INT64_FMT", "
                                          "STATUS: %s ]\n",
                                          aResultData->hitCount,
                                          aResultData->missCount,
                                          aResultData->memory->getSize(),
                                          sStatus );
            }
            else
            {
                iduVarStringAppendFormat( aString,
                                          "[ RESULT CACHE HIT: %"ID_INT32_FMT", "
                                          "MISS: %"ID_INT32_FMT", "
                                          "SIZE: BLOCKED, "
                                          "STATUS: %s ]\n",
                                          aResultData->hitCount,
                                          aResultData->missCount,
                                          sStatus );
            }
        }
        else
        {
            iduVarStringAppendFormat( aString,
                                      "[ RESULT CACHE HIT: 0, "
                                      "MISS: 0, "
                                      "SIZE: 0, "
                                      "STATUS: INIT ]\n" );
        }
    }
    else
    {
        iduVarStringAppendFormat( aString,
                                  "[ RESULT CACHE HIT: ??, "
                                  "MISS: ??, "
                                  "SIZE: ??, "
                                  "STATUS: ?? ]\n" );
    }

    IDE_EXCEPTION_CONT( normal_exit );
}

void qmn::setDisplayInfo( qmsTableRef      * aTableRef,
                          qmsNamePosition  * aTableOwnerName,
                          qmsNamePosition  * aTableName,
                          qmsNamePosition  * aAliasName )
{
    qcNamePosition     sTableOwnerNamePos;
    qcNamePosition     sTableNamePos;
    qcNamePosition     sAliasNamePos;

    sTableOwnerNamePos = aTableRef->userName;
    sTableNamePos      = aTableRef->tableName;
    sAliasNamePos      = aTableRef->aliasName;

    //table owner name ����
    if ( ( aTableRef->tableInfo->tableType == QCM_FIXED_TABLE ) ||
         ( aTableRef->tableInfo->tableType == QCM_DUMP_TABLE ) ||
         ( aTableRef->tableInfo->tableType == QCM_PERFORMANCE_VIEW ) )
    {
        // performance view�� tableInfo�� ����.
        aTableOwnerName->name = NULL;
        aTableOwnerName->size = QC_POS_EMPTY_SIZE;
    }
    else
    {
        aTableOwnerName->name = sTableOwnerNamePos.stmtText + sTableOwnerNamePos.offset;
        aTableOwnerName->size = sTableOwnerNamePos.size;
    }

    if ( ( sTableNamePos.stmtText != NULL ) && ( sTableNamePos.size > 0 ) )
    {
        //table name ����
        aTableName->name = sTableNamePos.stmtText + sTableNamePos.offset;
        aTableName->size = sTableNamePos.size;
    }
    else
    {
        aTableName->name = NULL;
        aTableName->size = QC_POS_EMPTY_SIZE;
    }


    //table name �� alias name�� ���� ���
    if ( sTableNamePos.offset == sAliasNamePos.offset )
    {
        aAliasName->name = NULL;
        aAliasName->size = QC_POS_EMPTY_SIZE;
    }
    else
    {
        aAliasName->name = sAliasNamePos.stmtText + sAliasNamePos.offset;
        aAliasName->size = sAliasNamePos.size;
    }
}

