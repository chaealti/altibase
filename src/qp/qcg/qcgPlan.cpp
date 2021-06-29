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
 * $Id: qcgPlan.cpp 90785 2021-05-06 07:26:22Z hykim $
 **********************************************************************/

#include <qc.h>
#include <qcg.h>
#include <qcgPlan.h>
#include <qsxProc.h>
#include <qsxRelatedProc.h>
#include <qcmSynonym.h>
#include <qdpPrivilege.h>
#include <qsxPkg.h>
#include <mtuProperty.h>
#include <qdpRole.h>
#include <sdi.h>

IDE_RC qcgPlan::allocAndInitPlanEnv( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     statement�� environment ����� ���� �ڷᱸ���� �����Ѵ�.
 *
 ***********************************************************************/

    // PROJ-2163
    qcPlanBindParam   * sBindParam      = NULL;
    UShort              sBindParamCount = 0;

    IDU_FIT_POINT( "qcgPlan::allocAndInitPlanEnv::cralloc::aStatement",
                    idERR_ABORT_InsufficientMemory );

    IDE_TEST( QC_QMP_MEM(aStatement)->cralloc(
            ID_SIZEOF(qcgPlanEnv),
            (void**) & aStatement->myPlan->planEnv )
        != IDE_SUCCESS);

    // PROJ-2163
    // Plan cache �� ���� ���ε� ������ ���� �޸� �Ҵ�
    sBindParamCount = qcg::getBindCount( aStatement );

    if( sBindParamCount > 0 )
    {
        IDE_TEST( QC_QMP_MEM(aStatement)->cralloc(
                ID_SIZEOF(qcPlanBindParam) * sBindParamCount,
                (void**) & sBindParam )
            != IDE_SUCCESS);
    }
    else
    {
        // Nothing to do.
    }

    aStatement->myPlan->planEnv->planBindInfo.bindParam      = sBindParam;
    aStatement->myPlan->planEnv->planBindInfo.bindParamCount = sBindParamCount;

    initPlanProperty( & aStatement->myPlan->planEnv->planProperty );

    // PROJ-2223 audit
    aStatement->myPlan->planEnv->auditInfo.operation = QCI_AUDIT_OPER_MAX;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcgPlan::initPlanProperty( qcPlanProperty * aProperty )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan property�� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    idlOS::memset(aProperty, 0x00, ID_SIZEOF(qcPlanProperty));
}

void qcgPlan::startIndirectRefFlag( qcStatement * aStatement,
                                    idBool      * aIndirectRef )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����ϴ� ��ü�� ���Ͽ� ���� ������ ���۵Ǿ�����
 *     ����Ѵ�. �̷��� ������ plan �����ÿ��� ���Ǵ� �ӽ�������
 *     �ٸ� ������ó�� template�� ����Ѵ�.
 *
 *     environment�� ���� �� ������ ������ �� �ִ�.
 *     1. plan ������ �����Ǵ� session & system property
 *     2. plan ������ �����Ǵ� �����ͺ��̽� ��ü
 *     3. plan ������ �����Ǵ� ��ü�� ���� �ʿ��� ����
 *
 *     ���߿��� 1,2���� ���������� �����Ǵ��� environment�ν�
 *     ��ϵǾ�� �ϳ�, 3���� ���������� �����Ǵ� ��ü�� ���Ͽ�
 *     ����ؼ��� �ȵǱ� ������ �̸� �����ؾ� �Ѵ�.
 *
 ***********************************************************************/
    
    *aIndirectRef = ID_FALSE;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) )
    {
        if ( QC_SHARED_TMPLATE(aStatement)->indirectRef == ID_FALSE )
        {
            QC_SHARED_TMPLATE(aStatement)->indirectRef = ID_TRUE;
            *aIndirectRef = ID_TRUE;
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

void qcgPlan::endIndirectRefFlag( qcStatement * aStatement,
                                  idBool      * aIndirectRef )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����ϴ� ��ü�� ���Ͽ� ���� ������ ��������
 *     ����Ѵ�.
 *
 ***********************************************************************/

    if ( *aIndirectRef == ID_TRUE )
    {
        QC_SHARED_TMPLATE(aStatement)->indirectRef = ID_FALSE;
        *aIndirectRef = ID_FALSE;
    }
    else
    {
        // Nothing to do.
    }
}

void qcgPlan::registerPlanProperty( qcStatement        * aStatement,
                                    qcPlanPropertyKind   aPropertyID )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� session & system property�� ����Ѵ�.
 *
 ***********************************************************************/

    qcPlanProperty  * sProperty;
    SChar           * sFormat;
    UInt              sFormatLen;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.

        sProperty = & aStatement->myPlan->planEnv->planProperty;

        switch( aPropertyID )
        {
            case PLAN_PROPERTY_USER_ID:
                if ( QC_SHARED_TMPLATE(aStatement)->indirectRef == ID_FALSE )
                {
                    QCG_REGISTER_PLAN_PROPERTY( userIDRef,
                                                userID,
                                                QCG_GET_SESSION_USER_ID(aStatement) );
                }
                else
                {
                    // Nothing to do.
                }
                break;

            case PLAN_PROPERTY_OPTIMIZER_MODE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerModeRef,
                                            optimizerMode,
                                            QCG_GET_SESSION_OPTIMIZER_MODE(aStatement) );

                break;

            case PLAN_PROPERTY_HOST_OPTIMIZE_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( hostOptimizeEnableRef,
                                            hostOptimizeEnable,
                                            QCU_HOST_OPTIMIZE_ENABLE );

                break;

            case PLAN_PROPERTY_SELECT_HEADER_DISPLAY:

                QCG_REGISTER_PLAN_PROPERTY( selectHeaderDisplayRef,
                                            selectHeaderDisplay,
                                            QCG_GET_SESSION_SELECT_HEADER_DISPLAY(aStatement) );

                break;

            case PLAN_PROPERTY_STACK_SIZE:

                QCG_REGISTER_PLAN_PROPERTY( stackSizeRef,
                                            stackSize,
                                            QCG_GET_SESSION_STACK_SIZE(aStatement) );

                break;

            case PLAN_PROPERTY_NORMAL_FORM_MAXIMUM:

                QCG_REGISTER_PLAN_PROPERTY( normalFormMaximumRef,
                                            normalFormMaximum,
                                            QCG_GET_SESSION_NORMALFORM_MAXIMUM(aStatement) );

                break;

            case PLAN_PROPERTY_DEFAULT_DATE_FORMAT:
                sFormat = QCG_GET_SESSION_DATE_FORMAT(aStatement);
                sFormatLen = idlOS::strlen( sFormat );

                IDE_DASSERT( sFormatLen < IDP_MAX_VALUE_LEN );

                if ( sProperty->defaultDateFormatRef == ID_FALSE )
                {
                    sProperty->defaultDateFormatRef = ID_TRUE;

                    idlOS::strncpy( sProperty->defaultDateFormat,
                                    sFormat,
                                    sFormatLen );
                    sProperty->defaultDateFormat[sFormatLen] = '\0';
                }
                else
                {
                    IDE_DASSERT( idlOS::strMatch( sProperty->defaultDateFormat,
                                                  idlOS::strlen( sProperty->defaultDateFormat ),
                                                  sFormat,
                                                  sFormatLen ) == 0 );
                }
                break;

            case PLAN_PROPERTY_ST_OBJECT_SIZE:

                QCG_REGISTER_PLAN_PROPERTY( STObjBufSizeRef,
                                            STObjBufSize,
                                            QCG_GET_SESSION_ST_OBJECT_SIZE(aStatement) );

                break;

            case PLAN_PROPERTY_OPTIMIZER_SIMPLE_VIEW_MERGE_DISABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerSimpleViewMergingDisableRef,
                                            optimizerSimpleViewMergingDisable,
                                            QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE );

                break;

            // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
            case PLAN_PROPERTY_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerDefaultTempTbsTypeRef,
                                            optimizerDefaultTempTbsType,
                                            QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE(aStatement) );

                break;

            case PLAN_PROPERTY_FAKE_TPCH_SCALE_FACTOR:

                QCG_REGISTER_PLAN_PROPERTY( fakeTpchScaleFactorRef,
                                            fakeTpchScaleFactor,
                                            QCU_FAKE_TPCH_SCALE_FACTOR );

                break;

            case PLAN_PROPERTY_FAKE_BUFFER_SIZE:

                QCG_REGISTER_PLAN_PROPERTY( fakeBufferSizeRef,
                                            fakeBufferSize,
                                            QCU_FAKE_BUFFER_SIZE );

                break;

            case PLAN_PROPERTY_OPTIMIZER_DISK_INDEX_COST_ADJ:

                QCG_REGISTER_PLAN_PROPERTY( optimizerDiskIndexCostAdjRef,
                                            optimizerDiskIndexCostAdj,
                                            QCG_GET_SESSION_OPTIMIZER_DISK_INDEX_COST_ADJ(aStatement) );

                break;

            // BUG-43736
            case PLAN_PROPERTY_OPTIMIZER_MEMORY_INDEX_COST_ADJ:

                QCG_REGISTER_PLAN_PROPERTY( optimizerMemoryIndexCostAdjRef,
                                            optimizerMemoryIndexCostAdj,
                                            QCG_GET_SESSION_OPTIMIZER_MEMORY_INDEX_COST_ADJ(aStatement) );

                break;

            case PLAN_PROPERTY_OPTIMIZER_HASH_JOIN_COST_ADJ:

                QCG_REGISTER_PLAN_PROPERTY( optimizerHashJoinCostAdjRef,
                                            optimizerHashJoinCostAdj,
                                            QCU_OPTIMIZER_HASH_JOIN_COST_ADJ );

                break;

            case PLAN_PROPERTY_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD:

                QCG_REGISTER_PLAN_PROPERTY( optimizerSubqueryOptimizeMethodRef,
                                            optimizerSubqueryOptimizeMethod,
                                            QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD );

                break;

            // BUG-34295
            case PLAN_PROPERTY_OPTIMIZER_ANSI_JOIN_ORDERING:

                QCG_REGISTER_PLAN_PROPERTY( optimizerAnsiJoinOrderingRef,
                                            optimizerAnsiJoinOrdering,
                                            QCU_OPTIMIZER_ANSI_JOIN_ORDERING );

                break;
            // BUG-38402
            case PLAN_PROPERTY_OPTIMIZER_ANSI_INNER_JOIN_CONVERT:

                QCG_REGISTER_PLAN_PROPERTY( optimizerAnsiInnerJoinConvertRef,
                                            optimizerAnsiInnerJoinConvert,
                                            QCU_OPTIMIZER_ANSI_INNER_JOIN_CONVERT );

                break;
            case PLAN_PROPERTY_NLS_CURRENCY_FORMAT:
                /* PROJ-2208 Multi Currency */
                if ( sProperty->nlsCurrencyRef == ID_FALSE )
                {
                    sProperty->nlsCurrencyRef = ID_TRUE;
                    sFormat = QCG_GET_SESSION_ISO_CURRENCY( aStatement );
                    sProperty->nlsISOCurrency[0] = *sFormat;
                    sProperty->nlsISOCurrency[1] = *( sFormat + 1 );
                    sProperty->nlsISOCurrency[2] = *( sFormat + 2 );
                    sProperty->nlsISOCurrency[3] = '\0';

                    sFormat = QCG_GET_SESSION_CURRENCY( aStatement );
                    sFormatLen = idlOS::strlen( sFormat );

                    idlOS::strncpy( sProperty->nlsCurrency,
                                    sFormat,
                                    sFormatLen );
                    sProperty->nlsCurrency[sFormatLen] = '\0';

                    sFormat = QCG_GET_SESSION_NUMERIC_CHAR( aStatement );
                    sProperty->nlsNumericChar[0] = *sFormat;
                    sProperty->nlsNumericChar[1] = *( sFormat + 1 );
                    sProperty->nlsNumericChar[2] = '\0';
                }
                else
                {
                    sFormat = QCG_GET_SESSION_ISO_CURRENCY( aStatement );
                    sFormatLen = idlOS::strlen( sFormat );
                    IDE_DASSERT( idlOS::strMatch( sProperty->nlsISOCurrency,
                                                  idlOS::strlen( sProperty->nlsISOCurrency ),
                                                  sFormat,
                                                  sFormatLen ) == 0 );
                    sFormat = QCG_GET_SESSION_CURRENCY( aStatement );
                    sFormatLen = idlOS::strlen( sFormat );
                    IDE_DASSERT( idlOS::strMatch( sProperty->nlsCurrency,
                                                  idlOS::strlen( sProperty->nlsCurrency ),
                                                  sFormat,
                                                  sFormatLen ) == 0 );
                    sFormat = QCG_GET_SESSION_NUMERIC_CHAR( aStatement );
                    sFormatLen = idlOS::strlen( sFormat );
                    IDE_DASSERT( idlOS::strMatch( sProperty->nlsNumericChar,
                                                  idlOS::strlen( sProperty->nlsNumericChar ),
                                                  sFormat,
                                                  sFormatLen ) == 0 );
                }
                break;
            // BUG-34350
            case PLAN_PROPERTY_OPTIMIZER_REFINE_PREPARE_MEMORY:

                QCG_REGISTER_PLAN_PROPERTY( optimizerRefinePrepareMemoryRef,
                                            optimizerRefinePrepareMemory,
                                            QCU_OPTIMIZER_REFINE_PREPARE_MEMORY );

                break;
            // BUG-35155 Partial CNF
            case PLAN_PROPERTY_OPTIMIZER_PARTIAL_NORMALIZE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerPartialNormalizeRef,
                                            optimizerPartialNormalize,
                                            QCU_OPTIMIZER_PARTIAL_NORMALIZE );

                break;

            /* PROJ-1090 Function-based Index */
            case PLAN_PROPERTY_QUERY_REWRITE_ENABLE :

                QCG_REGISTER_PLAN_PROPERTY( queryRewriteEnableRef,
                                            queryRewriteEnable,
                                            QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) );

                break;
            case PLAN_PROPERTY_OPTIMIZER_UNNEST_SUBQUERY:

                QCG_REGISTER_PLAN_PROPERTY( optimizerUnnestSubqueryRef,
                                            optimizerUnnestSubquery,
                                            QCU_OPTIMIZER_UNNEST_SUBQUERY );

                break;
            case PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY:

                QCG_REGISTER_PLAN_PROPERTY( optimizerUnnestComplexSubqueryRef,
                                            optimizerUnnestComplexSubquery,
                                            QCU_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY );

                break;
            case PLAN_PROPERTY_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY:

                QCG_REGISTER_PLAN_PROPERTY( optimizerUnnestAggregationSubqueryRef,
                                            optimizerUnnestAggregationSubquery,
                                            QCU_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY );

                break;
            /* PROJ-2242 Eliminate common subexpression */
            case PLAN_PROPERTY_OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION:

                QCG_REGISTER_PLAN_PROPERTY( optimizerEliminateCommonSubexpressionRef,
                                            optimizerEliminateCommonSubexpression,
                                            QCG_GET_SESSION_ELIMINATE_COMMON_SUBEXPRESSION( aStatement ) );

                break;

            /* PROJ-2242 Constant filter subsumption */
            case PLAN_PROPERTY_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION:

                QCG_REGISTER_PLAN_PROPERTY( optimizerConstantFilterSubsumptionRef,
                                            optimizerConstantFilterSubsumption,
                                            QCU_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION );

                break;
            /* PROJ-2242 Feature enable */
            case PLAN_PROPERTY_OPTIMIZER_FEATURE_ENABLE:

                sFormat = QCU_OPTIMIZER_FEATURE_ENABLE;
                sFormatLen = idlOS::strlen( sFormat );

                if ( sProperty->optimizerFeatureEnableRef == ID_FALSE )
                {
                    sProperty->optimizerFeatureEnableRef = ID_TRUE;

                    idlOS::strncpy( sProperty->optimizerFeatureEnable,
                                    sFormat,
                                    sFormatLen );
                    sProperty->optimizerFeatureEnable[sFormatLen] = '\0';
                }
                else
                {
                    IDE_DASSERT( idlOS::strMatch( sProperty->optimizerFeatureEnable,
                                                  idlOS::strlen( sProperty->optimizerFeatureEnable ),
                                                  sFormat,
                                                  sFormatLen ) == 0 );
                }
                break;
            case PLAN_PROPERTY_SYS_CONNECT_BY_PATH_PRECISION:

                QCG_REGISTER_PLAN_PROPERTY( sysConnectByPathPrecisionRef,
                                            sysConnectByPathPrecision,
                                            QCU_SYS_CONNECT_BY_PATH_PRECISION );

                break;
            case PLAN_PROPERTY_GROUP_CONCAT_PRECISION:

                QCG_REGISTER_PLAN_PROPERTY( groupConcatPrecisionRef,
                                            groupConcatPrecision,
                                            MTU_GROUP_CONCAT_PRECISION );

                break;
            case PLAN_PROPERTY_OPTIMIZER_USE_TEMP_TYPE:

                QCG_REGISTER_PLAN_PROPERTY( reduceTempMemoryEnableRef,
                                            reduceTempMemoryEnable,
                                            QCU_REDUCE_TEMP_MEMORY_ENABLE );

                break;
            /* PROJ-2361 */
            case PLAN_PROPERTY_AVERAGE_TRANSFORM_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( avgTransformEnableRef,
                                            avgTransformEnable,
                                            QCU_AVERAGE_TRANSFORM_ENABLE );

                break;
            // BUG-38132
            case PLAN_PROPERTY_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP:

                QCG_REGISTER_PLAN_PROPERTY( fixedGroupMemoryTempRef,
                                            fixedGroupMemoryTemp,
                                            QCU_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP );

                break;
            // BUG-38339 Outer Join Elimination
            case PLAN_PROPERTY_OPTIMIZER_OUTERJOIN_ELIMINATION:

                QCG_REGISTER_PLAN_PROPERTY( outerJoinEliminationRef,
                                            outerJoinElimination,
                                            QCU_OPTIMIZER_OUTERJOIN_ELIMINATION );

                break;

            // BUG-38434
            case PLAN_PROPERTY_OPTIMIZER_DNF_DISABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerDnfDisableRef,
                                            optimizerDnfDisable,
                                            QCU_OPTIMIZER_DNF_DISABLE);

                break;

            // BUG-38416 count(column) to count(*)
            case PLAN_PROPERTY_OPTIMIZER_COUNT_COLUMN_TO_COUNT_ASTAR:

                QCG_REGISTER_PLAN_PROPERTY( countColumnToCountAStarRef,
                                            countColumnToCountAStar,
                                            MTU_COUNT_COLUMN_TO_COUNT_ASTAR );

                break;

            // BUG-40022
            case PLAN_PROPERTY_OPTIMIZER_JOIN_DISABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerJoinDisableRef,
                                            optimizerJoinDisable,
                                            QCU_OPTIMIZER_JOIN_DISABLE);

                break;

            // PROJ-2469 Optimize View Materialization
            case PLAN_PROPERTY_OPTIMIZER_VIEW_TARGET_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerViewTargetEnableRef,
                                            optimizerViewTargetEnable,
                                            QCU_OPTIMIZER_VIEW_TARGET_ENABLE );

                break;

            // BUG-41183 ORDER BY Elimination
            case PLAN_PROPERTY_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerOrderByEliminationEnableRef,
                                            optimizerOrderByEliminationEnable,
                                            QCU_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE );

                break;
            // BUG-41249 DISTINCT Elimination
            case PLAN_PROPERTY_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( optimizerDistinctEliminationEnableRef,
                                            optimizerDistinctEliminationEnable,
                                            QCU_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE );

                break;
            // BUG-41944
            case PLAN_PROPERTY_ARITHMETIC_OP_MODE:

                QCG_REGISTER_PLAN_PROPERTY( arithmeticOpModeRef,
                                            arithmeticOpMode,
                                            QCG_GET_SESSION_ARITHMETIC_OP_MODE(aStatement) );

                break;
            /* PROJ-2462 Result Cache */
            case PLAN_PROPERTY_RESULT_CACHE_ENABLE:

                QCG_REGISTER_PLAN_PROPERTY( resultCacheEnableRef,
                                            resultCacheEnable,
                                            QCG_GET_SESSION_RESULT_CACHE_ENABLE(aStatement) );
                break;
            /* PROJ-2462 Result Cache */
            case PLAN_PROPERTY_TOP_RESULT_CACHE_MODE:

                QCG_REGISTER_PLAN_PROPERTY( topResultCacheModeRef,
                                            topResultCacheMode,
                                            QCG_GET_SESSION_TOP_RESULT_CACHE_MODE(aStatement) );
                break;
            /* PROJ-2616 */
            case PLAN_PROPERTY_EXECUTOR_FAST_SIMPLE_QUERY:

                QCG_REGISTER_PLAN_PROPERTY( executorFastSimpleQueryRef,
                                            executorFastSimpleQuery,
                                            QCU_EXECUTOR_FAST_SIMPLE_QUERY );
                break;
            // BUG-36438 LIST transformation
            case PLAN_PROPERTY_OPTIMIZER_LIST_TRANSFORMATION:

                QCG_REGISTER_PLAN_PROPERTY( optimizerListTransformationRef,
                                            optimizerListTransformation,
                                            QCU_OPTIMIZER_LIST_TRANSFORMATION );
                break;
            /* PROJ-2492 Dynamic sample selection */
            case PLAN_PROPERTY_OPTIMIZER_AUTO_STATS:
                QCG_REGISTER_PLAN_PROPERTY( mOptimizerAutoStatsRef,
                                            mOptimizerAutoStats,
                                            QCG_GET_SESSION_OPTIMIZER_AUTO_STATS(aStatement) );
                break;
            case PLAN_PROPERTY_OPTIMIZER_TRANSITIVITY_OLD_RULE:
                QCG_REGISTER_PLAN_PROPERTY( optimizerTransitivityOldRuleRef,
                                            optimizerTransitivityOldRule,
                                            QCG_GET_SESSION_OPTIMIZER_TRANSITIVITY_OLD_RULE(aStatement) );
                break;
            /* BUG-42639 Monitoring query */
            case PLAN_PROPERTY_OPTIMIZER_PERFORMANCE_VIEW:
                QCG_REGISTER_PLAN_PROPERTY( optimizerPerformanceViewRef,
                                            optimizerPerformanceView,
                                            QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) );
                break;
            // BUG-43039 inner join push down
            case PLAN_PROPERTY_OPTIMIZER_INNER_JOIN_PUSH_DOWN:

                QCG_REGISTER_PLAN_PROPERTY( optimizerInnerJoinPushDownRef,
                                            optimizerInnerJoinPushDown,
                                            QCU_OPTIMIZER_INNER_JOIN_PUSH_DOWN );
                break;
            // BUG-43068 Indexable order by ����
            case PLAN_PROPERTY_OPTIMIZER_ORDER_PUSH_DOWN:

                QCG_REGISTER_PLAN_PROPERTY( optimizerOrderPushDownRef,
                                            optimizerOrderPushDown,
                                            QCU_OPTIMIZER_ORDER_PUSH_DOWN );
                break;
            // BUG-43059 Target subquery unnest/removal disable
            case PLAN_PROPERTY_OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE:
                QCG_REGISTER_PLAN_PROPERTY( optimizerTargetSubqueryUnnestDisableRef,
                                            optimizerTargetSubqueryUnnestDisable,
                                            QCU_OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE );
                break;
            case PLAN_PROPERTY_OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE:
                QCG_REGISTER_PLAN_PROPERTY( optimizerTargetSubqueryRemovalDisableRef,
                                            optimizerTargetSubqueryRemovalDisable,
                                            QCU_OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE );
                break;
            case PLAN_PROPERTY_OPTIMIZER_DELAYED_EXECUTION:
                QCG_REGISTER_PLAN_PROPERTY( optimizerDelayedExecutionRef,
                                            optimizerDelayedExecution,
                                            QCU_OPTIMIZER_DELAYED_EXECUTION );
                break;
            /* BUG-43495 */
            case PLAN_PROPERTY_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE : 
                QCG_REGISTER_PLAN_PROPERTY( optimizerLikeIndexScanWithOuterColumnDisableRef,
                                            optimizerLikeIndexScanWithOuterColumnDisable,
                                            QCU_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE );
            break;
            /* TASK-6744 */
            case PLAN_PROPERTY_OPTIMIZER_PLAN_RANDOM_SEED : 
                QCG_REGISTER_PLAN_PROPERTY( optimizerPlanRandomSeedRef,
                                            optimizerPlanRandomSeed,
                                            QCU_PLAN_RANDOM_SEED );
                break;
            /* PROJ-2641 Hierarchy Query Index */
            case PLAN_PROPERTY_OPTIMIZER_HIERARCHY_TRANSFORMATION:
                QCG_REGISTER_PLAN_PROPERTY( mOptimizerHierarchyTransformationRef,
                                            mOptimizerHierarchyTransformation,
                                            QCU_OPTIMIZER_HIERARCHY_TRANSFORMATION );
                break;
            // BUG-44795
            case PLAN_PROPERTY_OPTIMIZER_DBMS_STAT_POLICY:
                QCG_REGISTER_PLAN_PROPERTY( optimizerDBMSStatPolicyRef,
                                            optimizerDBMSStatPolicy,
                                            QCU_OPTIMIZER_DBMS_STAT_POLICY );
                break;
            /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
            case PLAN_PROPERTY_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY:
                QCG_REGISTER_PLAN_PROPERTY( optimizerIndexNLJoinAccessMethodPolicyRef,
                                            optimizerIndexNLJoinAccessMethodPolicy,
                                            QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY );
                break;
            case PLAN_PROPERTY_OPTIMIZER_SEMI_JOIN_REMOVE:
                QCG_REGISTER_PLAN_PROPERTY( optimizerSemiJoinRemoveRef,
                                            optimizerSemiJoinRemove,
                                            QCU_OPTIMIZER_SEMI_JOIN_REMOVE );
                break;
            /* PROJ-2701 Sharding online data rebuild */
            case PLAN_PROPERTY_SHARD_META_NUMBER_FOR_DATA:
                QCG_REGISTER_PLAN_PROPERTY( mSMNForDataNodeRef,
                                            mSMNForDataNode,
                                            sdi::getSMNForDataNode() );
                break;
            /* PROJ-2701 Sharding online data rebuild */
            case PLAN_PROPERTY_SHARD_META_NUMBER_FOR_SESSION:
                QCG_REGISTER_PLAN_PROPERTY( mSMNForSessionRef,
                                            mSMNForSession,
                                            QCG_GET_SESSION_SHARD_META_NUMBER(aStatement) );
                break;
            /* PROJ-2701 Sharding online data rebuild */
            case PLAN_PROPERTY_SHARD_IS_USER_SESSION:
                QCG_REGISTER_PLAN_PROPERTY( mIsShardUserSessionRef,
                                            mIsShardUserSession,
                                            QCG_GET_SESSION_IS_SHARD_USER_SESSION(aStatement) );
                break;
            /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
            case PLAN_PROPERTY_CALL_BY_SHARD_ANALYZE_PROTOCOL:
                QCG_REGISTER_PLAN_PROPERTY( mCallByShardAnalyzeProtocolRef,
                                            mCallByShardAnalyzeProtocol,
                                            QCG_GET_CALL_BY_SHARD_ANALYZE_PROTOCOL(aStatement) );
                break;
            /* TASK-7219 Non-shard DML */
            case PLAN_PROPERTY_SHARD_PARTIAL_EXEC_TYPE:
                QCG_REGISTER_PLAN_PROPERTY( mShardPartialExecTypeRef,
                                            mShardPartialExecType,
                                            QCG_GET_SHARD_PARTIAL_EXEC_TYPE(aStatement) );
                break;
            /* PROJ-2687 */
            case PLAN_PROPERTY_SHARD_AGGREGATION_TRANSFORM_ENABLE:
                QCG_REGISTER_PLAN_PROPERTY( mShardAggregationTransformEnableRef,
                                            mShardAggregationTransformEnable,
                                            SDU_SHARD_AGGREGATION_TRANSFORM_ENABLE );
                break;
            /* TASK-7219 */
            case PLAN_PROPERTY_SHARD_TRANSFORM_MODE:
                QCG_REGISTER_PLAN_PROPERTY( mShardTransformModeRef,
                                            mShardTransformMode,
                                            SDU_SHARD_TRANSFORM_MODE );
                break;
            // key preserved property
            case PLAN_PROPERTY_KEY_PRESERVED_TABLE:
                QCG_REGISTER_PLAN_PROPERTY( mKeyPreservedTableRef,
                                            mKeyPreservedTable,
                                            QCU_KEY_PRESERVED_TABLE );

                break;
            case PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPATIBILITY:
                QCG_REGISTER_PLAN_PROPERTY( mUnnestCompatibilityRef,
                                            mUnnestCompatibility,
                                            QCU_OPTIMIZER_UNNEST_COMPATIBILITY );
                break;

            /* PROJ-2632 */
            case PLAN_PROPERTY_SERIAL_EXECUTE_MODE:
                QCG_REGISTER_PLAN_PROPERTY( mSerialExecuteModeRef,
                                            mSerialExecuteMode,
                                            QCG_GET_SERIAL_EXECUTE_MODE( aStatement ) );
                break;
            /* BUG-46932 */
            case PLAN_PROPERTY_OPTIMIZER_INVERSE_JOIN_ENABLE:
                QCG_REGISTER_PLAN_PROPERTY( mInverseJoinEnableRef,
                                            mInverseJoinEnable,
                                            QCU_OPTIMIZER_INVERSE_JOIN_ENABLE );
                break;
            /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
            case PLAN_PROPERTY_REDUCE_PART_PREPARE_MEMORY:
                QCG_REGISTER_PLAN_PROPERTY( mReducePartPrepareMemoryRef,
                                            mReducePartPrepareMemory,
                                            QCG_GET_REDUCE_PART_PREPARE_MEMORY( aStatement ) );
                break;
            case PLAN_PROPERTY_SHARD_IN_PSM_ENABLE:
                QCG_REGISTER_PLAN_PROPERTY( mShardInPSMEnableRef,
                                            mShardInPSMEnable,
                                            QCG_GET_SESSION_SHARD_IN_PSM_ENABLE(aStatement) );
                break;
            /* BUG-47986 */
            case PLAN_PROPERTY_OPTIMIZER_OR_VALUE_INDEX:
                QCG_REGISTER_PLAN_PROPERTY( mOrValueIndexRef,
                                            mOrValueIndex,
                                            QCU_OPTIMIZER_OR_VALUE_INDEX );
                break;
            /* BUG-48132 */
            case PLAN_PROPERTY_OPTIMIZER_PLAN_HASH_OR_SORT_METHOD:
                QCG_REGISTER_PLAN_PROPERTY( mPlanHashOrSortMethodRef,
                                            mPlanHashOrSortMethod,
                                            QCG_GET_PLAN_HASH_OR_SORT_METHOD( aStatement ) );
                break;
            /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
            case PLAN_PROPERTY_OPTIMIZER_INDEX_NL_JOIN_PENALTY:
                QCG_REGISTER_PLAN_PROPERTY( mIndexNlJoinPenaltyRef,
                                            mIndexNlJoinPenalty,
                                            QCU_OPTIMIZER_INDEX_NL_JOIN_PENALTY );
                break;
                /* BUG-48120 */
            case PLAN_PROPERTY_OPTIMIZER_INDEX_COST_MODE :
                QCG_REGISTER_PLAN_PROPERTY( mIndexCostModeRef,
                                            mIndexCostMode,
                                            QCU_OPTIMIZER_INDEX_COST_MODE );
                break;
            /* BUG-48161 */
            case PLAN_PROPERTY_OPTIMIZER_BUCKET_COUNT_MAX:
                QCG_REGISTER_PLAN_PROPERTY( mBucketCountMaxRef,
                                            mBucketCountMax,
                                            QCG_GET_BUCKET_COUNT_MAX( aStatement ) );
                break;
            case PLAN_PROPERTY_SHARD_STATUS:
                QCG_REGISTER_PLAN_PROPERTY( mShardStatusRef,
                                            mShardStatus,
                                            sdi::getShardStatus() );
                break;
            /* TASK-7307 */
            case PLAN_PROPERTY_SHARD_INTERNAL_LOCAL_OPERATION:
                QCG_REGISTER_PLAN_PROPERTY( mShardInternalLocalOperationRef,
                                            mShardInternalLocalOperation,
                                            QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( aStatement ) );
                break;
            /* PROJ-2750 */
            case PLAN_PROPERTY_LEFT_OUTER_SKIP_RIGHT_ENABLE:
                QCG_REGISTER_PLAN_PROPERTY( mLeftOuterSkipRightEnableRef,
                                            mLeftOuterSkipRightEnable,
                                            QCU_LEFT_OUTER_SKIP_RIGHT_ENABLE );
                break;
            default:
                IDE_DASSERT( 0 );
                break;
        }
    }
    else
    {
        // Nothing to do.
    }
}

IDE_RC qcgPlan::registerPlanTable( qcStatement  * aStatement,
                                   void         * aTableHandle,
                                   smSCN          aTableSCN,
                                   UInt           aTableOwnerID, /* BUG-45893 */
                                   SChar        * aTableName )   /* BUG-45893 */
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� table, view, queue,
 *     dblink ��ü�� ������ ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanObject   * sObject;
    qcgEnvTableList * sTable;
    idBool            sExist = ID_FALSE;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�. �������� ���� �����ϴ� ��ü�鵵
        // ��� ����Ѵ�. view ��ü�� ��� view�� invalid�� �����Ҷ�
        // view�� ���� �����ϴ� view�� ���ؼ��� invalid�� �ٲٱ� ������
        // view�� ���� �����ϴ� view�� ���ؼ��� view�� handle�� SCN�����δ�
        // �̸� �Ǵ��� �� ���⶧���̴�.
        sObject = & aStatement->myPlan->planEnv->planObject;

        for ( sTable = sObject->tableList;
              sTable != NULL;
              sTable = sTable->next )
        {
            if ( sTable->tableHandle == aTableHandle )
            {
                /* BUG-44926 DASSERT */
                if ( SM_SCN_IS_EQ(&(sTable->tableSCN), &aTableSCN) )
                {
                    /* Nothing to do */
                }
                else
                {
                    sTable->tableSCN = aTableSCN;
                }
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sExist == ID_FALSE )
        {
            IDU_FIT_POINT( "qcgPlan::registerPlanTable::alloc::sTable",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvTableList),
                                                     (void**)&sTable )
                      != IDE_SUCCESS);

            sTable->tableHandle = aTableHandle;
            sTable->tableSCN    = aTableSCN;
            sTable->next        = sObject->tableList;

            /* BUG-45893 */
            if ( aTableOwnerID == QC_SYSTEM_USER_ID )
            {
                if ( ( idlOS::strlen( aTableName ) >= 4 ) &&
                     ( idlOS::strMatch( aTableName,
                                        4,
                                        (SChar *)"DBA_",
                                        4 ) == 0 ) )
                {
                    sTable->mIsDBAUser = ID_TRUE;
                }
                else
                {
                    sTable->mIsDBAUser = ID_FALSE;
                }
            }
            else
            {
                sTable->mIsDBAUser = ID_FALSE;
            }

            // �����Ѵ�.
            sObject->tableList = sTable;
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
        
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::registerPlanSequence( qcStatement * aStatement,
                                      void        * aSequenceHandle )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� sequence ��ü�� ������
 *     ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanObject      * sObject;
    qcgEnvSequenceList * sSequence;
    idBool               sExist = ID_FALSE;
    smSCN                sSCN;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.
        sObject = & aStatement->myPlan->planEnv->planObject;

        for ( sSequence = sObject->sequenceList;
              sSequence != NULL;
              sSequence = sSequence->next )
        {
            if ( sSequence->sequenceHandle == aSequenceHandle )
            {
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sExist == ID_FALSE )
        {
            IDU_FIT_POINT( "qcgPlan::registerPlanSequence::alloc::sSequence",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvSequenceList),
                                                     (void**)&sSequence )
                      != IDE_SUCCESS);

            /* Slot ��Ȱ���� �����ϱ� ���� Plan ��ϴ����
             * Slot SCN ���� ����� �ξ�� �Ѵ�. */
            sSequence->sequenceHandle = aSequenceHandle;
            sSCN                      = smiGetRowSCN( aSequenceHandle );
            sSequence->sequenceSCN    = sSCN;
            sSequence->next           = sObject->sequenceList;

            // �����Ѵ�.
            sObject->sequenceList = sSequence;
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
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::registerPlanProc( qcStatement     * aStatement,
                                  qsxProcPlanList * aProcPlanList )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� PSM ��ü�� ������
 *     ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanObject   * sObject;
    qcgEnvProcList  * sProc;
    qsxProcPlanList * sProcPlanList;
    idBool            sExist = ID_FALSE;
    void            * sProcHandle;
    smSCN             sSCN;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.

        sObject = & aStatement->myPlan->planEnv->planObject;

        for ( sProcPlanList = aProcPlanList;
              sProcPlanList != NULL;
              sProcPlanList = sProcPlanList->next )
        {
            sExist = ID_FALSE;

            // PROJ-1073 Package
            if( (sProcPlanList->objectType == QS_PROC) ||
                (sProcPlanList->objectType == QS_FUNC) ||
                (sProcPlanList->objectType == QS_TYPESET) )
            {
                for ( sProc = sObject->procList;
                      sProc != NULL;
                      sProc = sProc->next )
                {
                    if ( sProc->procID == sProcPlanList->objectID )
                    {
                        IDE_DASSERT( sProc->modifyCount ==
                                     sProcPlanList->modifyCount );

                        sExist = ID_TRUE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sExist == ID_FALSE )
                {
                    IDU_FIT_POINT( "qcgPlan::registerPlanProc::alloc::sProc",
                                    idERR_ABORT_InsufficientMemory );

                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvProcList),
                                                             (void**)&sProc )
                              != IDE_SUCCESS);

                    sProc->procID = sProcPlanList->objectID;
                    sProc->planTree = (qsProcParseTree *)(sProcPlanList->planTree);
                    sProc->modifyCount = sProcPlanList->modifyCount;
                    sProc->next = sObject->procList;

                    /* Slot ��Ȱ���� �����ϱ� ���� Plan ��ϴ����
                     * Slot SCN ���� ����� �ξ�� �Ѵ�. */
                    sProcHandle       = (void*)smiGetTable( sProc->procID );
                    sSCN              = smiGetRowSCN( sProcHandle );
                    sProc->procHandle = sProcHandle;
                    sProc->procSCN    = sSCN;

                    // �����Ѵ�.
                    sObject->procList = sProc;
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::registerPlanSynonym( qcStatement           * aStatement,
                                     struct qcmSynonymInfo * aSynonymInfo,
                                     qcNamePosition          aUserName,
                                     qcNamePosition          aObjectName,
                                     void                  * aTableHandle,
                                     void                  * aSequenceHandle )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� synonym ��ü�� ������
 *     ����ϸ�, synonym ��ü�� �� ���� ��ü�� table ��ü�� sequence
 *     ��ü�϶� ���� ������ ����Ѵ�.
 *     synonym ��ü�� ����� synonym�� ����Ű�� ���� ��ü�� �Բ�
 *     ����Ͽ� ������ ���������� �Ǵ��� �� �ֵ��� �Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanObject     * sObject;
    qcgEnvSynonymList * sSynonym;
    idBool              sExist = ID_FALSE;

    IDE_DASSERT( QC_IS_NULL_NAME( aObjectName ) == ID_FALSE );
    
    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.

        if ( aSynonymInfo->isSynonymName == ID_TRUE )
        {
            sObject = & aStatement->myPlan->planEnv->planObject;

            for ( sSynonym = sObject->synonymList;
                  sSynonym != NULL;
                  sSynonym = sSynonym->next )
            {
                if ( QC_IS_NULL_NAME( aUserName ) == ID_TRUE )
                {
                    if ( (sSynonym->userName[0] == '\0') &&
                         (idlOS::strMatch( sSynonym->objectName,
                                           idlOS::strlen( sSynonym->objectName ),
                                           aObjectName.stmtText + aObjectName.offset,
                                           aObjectName.size ) == 0) &&
                         (sSynonym->isPublicSynonym == aSynonymInfo->isPublicSynonym) )
                    {
                        IDE_DASSERT( sSynonym->tableHandle == aTableHandle );
                        IDE_DASSERT( sSynonym->sequenceHandle == aSequenceHandle );
                        
                        sExist = ID_TRUE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
                else
                {
                    if ( (idlOS::strMatch( sSynonym->userName,
                                           idlOS::strlen( sSynonym->userName ),
                                           aUserName.stmtText + aUserName.offset,
                                           aUserName.size ) == 0) &&
                         (idlOS::strMatch( sSynonym->objectName,
                                           idlOS::strlen( sSynonym->objectName ),
                                           aObjectName.stmtText + aObjectName.offset,
                                           aObjectName.size ) == 0) &&
                         (sSynonym->isPublicSynonym == aSynonymInfo->isPublicSynonym) )
                    {
                        IDE_DASSERT( sSynonym->tableHandle == aTableHandle );
                        IDE_DASSERT( sSynonym->sequenceHandle == aSequenceHandle );
                        
                        sExist = ID_TRUE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
            
            if ( sExist == ID_FALSE )
            {
                IDU_FIT_POINT( "qcgPlan::registerPlanSynonym::alloc::sSynonym", 
                                idERR_ABORT_InsufficientMemory );

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvSynonymList),
                                                         (void**)&sSynonym )
                          != IDE_SUCCESS);

                if ( QC_IS_NULL_NAME( aUserName ) == ID_TRUE )
                {
                    sSynonym->userName[0] = '\0';
                }
                else
                {
                    idlOS::strncpy( sSynonym->userName,
                                    aUserName.stmtText + aUserName.offset,
                                    aUserName.size );
                    sSynonym->userName[aUserName.size] = '\0';
                }
                
                idlOS::strncpy( sSynonym->objectName,
                                aObjectName.stmtText + aObjectName.offset,
                                aObjectName.size );
                sSynonym->objectName[aObjectName.size] = '\0';

                sSynonym->isPublicSynonym = aSynonymInfo->isPublicSynonym;
                
                sSynonym->tableHandle    = aTableHandle;
                sSynonym->sequenceHandle = aSequenceHandle;
                sSynonym->objectHandle   = NULL;
                sSynonym->objectID       = QS_EMPTY_OID;

                if ( aTableHandle != NULL )
                {
                    sSynonym->objectSCN = smiGetRowSCN( aTableHandle );
                }
                else
                {
                    sSynonym->objectSCN = smiGetRowSCN( aSequenceHandle );
                }
                
                sSynonym->next = sObject->synonymList;
                
                // �����Ѵ�.
                sObject->synonymList = sSynonym;
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
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::registerPlanProcSynonym( qcStatement              * aStatement,
                                         struct qsSynonymList     * aObjectSynonymList )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� synonym ��ü�� ������
 *     ����ϸ�, synonym ��ü�� �� ���� ��ü�� PSM ��ü�϶� ����
 *     ������ ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanObject     * sObject;
    qcgEnvSynonymList * sSynonym;
    qsSynonymList     * sObjectSynonym;
    void              * sObjectHandle;
    smSCN               sSCN;


    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.

        sObject = & aStatement->myPlan->planEnv->planObject;

        for ( sObjectSynonym = aObjectSynonymList;
              sObjectSynonym != NULL;
              sObjectSynonym = sObjectSynonym->next )
        {
            // aObjectSynonymList�� �̹� �ߺ��� ���ŵǾ���.
            IDU_FIT_POINT( "qcgPlan::registerPlanProcSynonym::alloc::sSynonym",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvSynonymList),
                                                     (void**)&sSynonym )
                      != IDE_SUCCESS);

            idlOS::strncpy( sSynonym->userName,
                            sObjectSynonym->userName,
                            QC_MAX_OBJECT_NAME_LEN + 1 );
            sSynonym->userName[QC_MAX_OBJECT_NAME_LEN] = '\0';

            idlOS::strncpy( sSynonym->objectName,
                            sObjectSynonym->objectName,
                            QC_MAX_OBJECT_NAME_LEN + 1 );
            sSynonym->objectName[QC_MAX_OBJECT_NAME_LEN] = '\0';
            
            sSynonym->isPublicSynonym = sObjectSynonym->isPublicSynonym;
                
            sSynonym->tableHandle = NULL;
            sSynonym->sequenceHandle = NULL;
            sSynonym->objectID = (smOID) sObjectSynonym->objectID;

            /* Slot ��Ȱ���� �����ϱ� ���� Plan ��ϴ����
             * Slot SCN ���� ����� �ξ�� �Ѵ�. */
            sObjectHandle          = (void*)smiGetTable( sSynonym->objectID );
            sSCN                   = smiGetRowSCN( sObjectHandle );
            sSynonym->objectHandle = sObjectHandle;
            sSynonym->objectSCN    = sSCN;
            
            sSynonym->next = sObject->synonymList;
                
            // �����Ѵ�.
            sObject->synonymList = sSynonym;
        }
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qcgPlan::registerPlanPrivTable( qcStatement         * aStatement,
                                       UInt                  aPrivilegeID,
                                       struct qcmTableInfo * aTableInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� table, view, queue, dblink ��ü�� ����
 *     �ʿ��� ������ ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanPrivilege    * sPrivilege;
    qcgEnvTablePrivList * sTablePriv;
    idBool                sExist = ID_FALSE;
    UInt                  i;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (QC_SHARED_TMPLATE(aStatement)->indirectRef == ID_FALSE) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�. ���� �������� ���� �����Ǵ�
        // ��ü�� ���� ������ ������� �ʴ´�.

        sPrivilege = & aStatement->myPlan->planEnv->planPrivilege;

        for ( sTablePriv = sPrivilege->tableList;
              sTablePriv != NULL;
              sTablePriv = sTablePriv->next )
        {
            if ( (sTablePriv->tableHandle == aTableInfo->tableHandle) &&
                 (sTablePriv->privilegeID == aPrivilegeID) )
            {
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sExist == ID_FALSE )
        {
            IDU_FIT_POINT( "qcgPlan::registerPlanPrivTable::alloc::sTablePriv",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvTablePrivList),
                                                     (void**)&sTablePriv )
                      != IDE_SUCCESS);

            sTablePriv->tableHandle  = aTableInfo->tableHandle;
            sTablePriv->tableOwnerID = aTableInfo->tableOwnerID;
            idlOS::strncpy( sTablePriv->tableName,
                            aTableInfo->name,
                            QC_MAX_OBJECT_NAME_LEN + 1 );
            sTablePriv->tableName[QC_MAX_OBJECT_NAME_LEN] = '\0';
            sTablePriv->privilegeCount = aTableInfo->privilegeCount;

            // soft prepare�� ���� �˻�� cached meta�� �������� �ʰ�
            // ������ �˻��ϱ� ���� tableInfo�� ���Ѹ��� �����ؼ� ����Ѵ�.
            // �̰��� validatePlan���� table ��ü�� ���� ��ȿ���� �˻������Ƿ�
            // tableInfo�� ���纻�� ����ϴ��� ���� �˻簡 �����ϴ�.
            // ���� validatePlan�Ŀ� table ��ü�� ����Ǿ� ���� ������
            // ��ȿ���� ���� ���� ������ prepare �Ŀ� table ��ü��
            // ����� ���� �������� �����ϹǷ� ������ ����.
            if ( sTablePriv->privilegeCount > 0 )
            {
                IDU_FIT_POINT( "qcgPlan::registerPlanPrivTable::alloc::privilegeInfo",
                                idERR_ABORT_InsufficientMemory );

                IDE_TEST( QC_QMP_MEM(aStatement)->alloc(
                        ID_SIZEOF(qcmPrivilege) * sTablePriv->privilegeCount,
                        (void**)&sTablePriv->privilegeInfo )
                    != IDE_SUCCESS);

                for ( i = 0; i < sTablePriv->privilegeCount; i++ )
                {
                    sTablePriv->privilegeInfo[i].userID =
                        aTableInfo->privilegeInfo[i].userID;
                    sTablePriv->privilegeInfo[i].privilegeBitMap =
                        aTableInfo->privilegeInfo[i].privilegeBitMap;
                }
            }
            else
            {
                sTablePriv->privilegeInfo = NULL;
            }

            sTablePriv->privilegeID = aPrivilegeID;

            sTablePriv->next = sPrivilege->tableList;
            
            // �����Ѵ�.
            sPrivilege->tableList = sTablePriv;
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::registerPlanPrivSequence( qcStatement            * aStatement,
                                          struct qcmSequenceInfo * aSequenceInfo )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� sequence ��ü�� ���� �ʿ��� ������ ����Ѵ�.
 *
 ***********************************************************************/
    
    qcgPlanPrivilege       * sPrivilege;
    qcgEnvSequencePrivList * sSequencePriv;
    idBool                   sExist = ID_FALSE;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (QC_SHARED_TMPLATE(aStatement)->indirectRef == ID_FALSE) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�. ���� �������� ���� �����Ǵ�
        // ��ü�� ���� ������ ������� �ʴ´�.

        sPrivilege = & aStatement->myPlan->planEnv->planPrivilege;

        for ( sSequencePriv = sPrivilege->sequenceList;
              sSequencePriv != NULL;
              sSequencePriv = sSequencePriv->next )
        {
            if ( sSequencePriv->sequenceID == aSequenceInfo->sequenceID )
            {
                IDE_DASSERT( sSequencePriv->sequenceOwnerID ==
                             aSequenceInfo->sequenceOwnerID );
                
                sExist = ID_TRUE;
                break;
            }
            else
            {
                // Nothing to do.
            }
        }

        if ( sExist == ID_FALSE )
        {
            IDU_FIT_POINT( "qcgPlan::registerPlanPrivSequence::alloc::sSequencePriv",
                            idERR_ABORT_InsufficientMemory );

            IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvSequencePrivList),
                                                     (void**)&sSequencePriv )
                      != IDE_SUCCESS);

            sSequencePriv->sequenceOwnerID = aSequenceInfo->sequenceOwnerID;
            sSequencePriv->sequenceID      = aSequenceInfo->sequenceID;
            idlOS::strncpy( sSequencePriv->name,
                            aSequenceInfo->name,
                            QC_MAX_OBJECT_NAME_LEN + 1 );
            sSequencePriv->name[QC_MAX_OBJECT_NAME_LEN] = '\0';
            sSequencePriv->next = sPrivilege->sequenceList;
            
            // �����Ѵ�.
            sPrivilege->sequenceList = sSequencePriv;
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

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1073 Package
IDE_RC qcgPlan::registerPlanPkg( qcStatement     * aStatement,
                                 qsxProcPlanList * aPlanList )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan ������ �����Ǵ� �����ͺ��̽� ��ü�� PACKAGE ��ü�� ������
 *     ����Ѵ�.
 *
 ***********************************************************************/

    qcgPlanObject   * sObject;
    qcgEnvPkgList   * sPkg;
    qsxProcPlanList * sPlanList;
    qsxPkgInfo      * sPkgInfo;
    qsxPkgInfo      * sPkgBodyInfo;                      // BUG-37250
    idBool            sExist = ID_FALSE;
    void            * sPkgHandle;
    smSCN             sSCN;

    if ( (aStatement->sharedFlag == ID_FALSE) &&
         (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
         (aStatement->myPlan->planEnv != NULL) )
    {
        // hard prepare������ ����Ѵ�.

        sObject = & aStatement->myPlan->planEnv->planObject;

        for ( sPlanList = aPlanList;
              sPlanList != NULL;
              sPlanList = sPlanList->next )
        {
            sExist = ID_FALSE;

            // Package�� subprogram�� pkgPist�� �߰�
            if( sPlanList->objectType == QS_PKG )
            {
                for ( sPkg = sObject->pkgList;
                      sPkg != NULL;
                      sPkg = sPkg->next )
                {
                    if ( sPkg->pkgID == sPlanList->objectID )
                    {
                        IDE_DASSERT( sPkg->modifyCount ==
                                     sPlanList->modifyCount );

                        sExist = ID_TRUE;
                        break;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }

                if ( sExist == ID_FALSE )
                {
                    IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvPkgList),
                                                             (void**)&sPkg )
                              != IDE_SUCCESS);

                    IDE_TEST( qsxPkg::getPkgInfo( sPlanList->objectID,
                                                  &sPkgInfo)
                              != IDE_SUCCESS );

                    sPkg->pkgID       = sPlanList->objectID;
                    sPkg->planTree    = sPkgInfo->planTree;          // BUG-37250
                    sPkg->modifyCount = sPlanList->modifyCount;
                    sPkg->next        = sObject->pkgList;

                    /* Slot ��Ȱ���� �����ϱ� ���� Plan ��ϴ����
                     * Slot SCN ���� ����� �ξ�� �Ѵ�. */
                    sPkgHandle      = (void*)smiGetTable( sPkg->pkgID );
                    sSCN            = smiGetRowSCN( sPkgHandle );
                    sPkg->pkgHandle = sPkgHandle;
                    sPkg->pkgSCN    = sSCN;

                    // �����Ѵ�.
                    sObject->pkgList = sPkg;

                    /* BUG-37250
                       package body�� qcgEnvPkgList�� �߰��Ͽ� ���� �� ���� ���� �˻�. */
                    if ( sPlanList->pkgBodyOID != QS_EMPTY_OID )
                    {
                        IDE_DASSERT( sPkgInfo->objType == QS_PKG );

                        IDE_TEST( QC_QMP_MEM(aStatement)->alloc( ID_SIZEOF(qcgEnvPkgList),
                                                                 (void**)&sPkg )
                                  != IDE_SUCCESS);

                        IDE_TEST( qsxPkg::getPkgInfo( sPlanList->pkgBodyOID,
                                                      &sPkgBodyInfo )
                                  != IDE_SUCCESS );

                        sPkg->pkgID       = sPlanList->pkgBodyOID;
                        sPkg->planTree    = sPkgBodyInfo->planTree;
                        sPkg->modifyCount = sPlanList->modifyCount;
                        sPkg->next        = sObject->pkgList;

                        /* Slot ��Ȱ���� �����ϱ� ���� Plan ��ϴ����
                         * Slot SCN ���� ����� �ξ�� �Ѵ�. */
                        sPkgHandle      = (void*)smiGetTable( sPkg->pkgID );
                        sSCN            = smiGetRowSCN( sPkgHandle );
                        sPkg->pkgHandle = sPkgHandle;
                        sPkg->pkgSCN    = sSCN;

                        // �����Ѵ�.
                        sObject->pkgList = sPkg;
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
    }
    else
    {
        // Nothing to do.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::isMatchedPlanProperty( qcStatement    * aStatement,
                                       qcPlanProperty * aPlanProperty,
                                       idBool         * aIsMatched )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan cache�� cache�� environment(plan property)�� soft prepare����
 *     qcStatement�� ���� environment(plan property)�� ���Ͽ�
 *     �ùٸ� plan���� �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcPlanProperty * sProperty;
    SChar          * sDateFormat;
    UInt             sDateFormatLen;
    SChar          * sFormat;
    UInt             sFormatLen;
    UInt             sMatchedCount  = 0;
    idBool           sIsMatched     = ID_TRUE;

    sProperty = aStatement->propertyEnv;

    ////////////////////////////////////////////////////////////////////
    // QCG_MATCHED_PLAN_PROPERTY ��ũ�η� üũ�Ҽ� �ִ� ���
    ////////////////////////////////////////////////////////////////////

    QCG_MATCHED_PLAN_PROPERTY( userIDRef,
                               userID,
                               QCG_GET_SESSION_USER_ID(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( optimizerModeRef,
                               optimizerMode,
                               QCG_GET_SESSION_OPTIMIZER_MODE(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( hostOptimizeEnableRef,
                               hostOptimizeEnable,
                               QCU_HOST_OPTIMIZE_ENABLE );

    QCG_MATCHED_PLAN_PROPERTY( selectHeaderDisplayRef,
                               selectHeaderDisplay,
                               QCG_GET_SESSION_SELECT_HEADER_DISPLAY(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( stackSizeRef,
                               stackSize,
                               QCG_GET_SESSION_STACK_SIZE(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( normalFormMaximumRef,
                               normalFormMaximum,
                               QCG_GET_SESSION_NORMALFORM_MAXIMUM(aStatement) );

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    QCG_MATCHED_PLAN_PROPERTY( optimizerDefaultTempTbsTypeRef,
                               optimizerDefaultTempTbsType,
                               QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( STObjBufSizeRef,
                               STObjBufSize,
                               QCG_GET_SESSION_ST_OBJECT_SIZE(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( optimizerSimpleViewMergingDisableRef,
                               optimizerSimpleViewMergingDisable,
                               QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE );

    QCG_MATCHED_PLAN_PROPERTY( fakeTpchScaleFactorRef,
                               fakeTpchScaleFactor,
                               QCU_FAKE_TPCH_SCALE_FACTOR );

    QCG_MATCHED_PLAN_PROPERTY( fakeBufferSizeRef,
                               fakeBufferSize,
                               QCU_FAKE_BUFFER_SIZE );

    QCG_MATCHED_PLAN_PROPERTY( optimizerDiskIndexCostAdjRef,
                               optimizerDiskIndexCostAdj,
                               QCG_GET_SESSION_OPTIMIZER_DISK_INDEX_COST_ADJ(aStatement) );

    // BUG-43736
    QCG_MATCHED_PLAN_PROPERTY( optimizerMemoryIndexCostAdjRef,
                               optimizerMemoryIndexCostAdj,
                               QCG_GET_SESSION_OPTIMIZER_MEMORY_INDEX_COST_ADJ(aStatement) );

    QCG_MATCHED_PLAN_PROPERTY( optimizerHashJoinCostAdjRef,
                               optimizerHashJoinCostAdj,
                               QCU_OPTIMIZER_HASH_JOIN_COST_ADJ );

    QCG_MATCHED_PLAN_PROPERTY( optimizerSubqueryOptimizeMethodRef,
                               optimizerSubqueryOptimizeMethod,
                               QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD );

    QCG_MATCHED_PLAN_PROPERTY( optimizerAnsiJoinOrderingRef,
                               optimizerAnsiJoinOrdering,
                               QCU_OPTIMIZER_ANSI_JOIN_ORDERING );
    // BUG-38402
    QCG_MATCHED_PLAN_PROPERTY( optimizerAnsiInnerJoinConvertRef,
                               optimizerAnsiInnerJoinConvert,
                               QCU_OPTIMIZER_ANSI_INNER_JOIN_CONVERT );

    // BUG-34350
    QCG_MATCHED_PLAN_PROPERTY( optimizerRefinePrepareMemoryRef,
                               optimizerRefinePrepareMemory,
                               QCU_OPTIMIZER_REFINE_PREPARE_MEMORY );

    // BUG-35155 Partial CNF
    QCG_MATCHED_PLAN_PROPERTY( optimizerPartialNormalizeRef,
                               optimizerPartialNormalize,
                               QCU_OPTIMIZER_PARTIAL_NORMALIZE );

    /* PROJ-1090 Function-based Index */
    QCG_MATCHED_PLAN_PROPERTY( queryRewriteEnableRef,
                               queryRewriteEnable,
                               QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) );

    // PROJ-1718
    QCG_MATCHED_PLAN_PROPERTY( optimizerUnnestSubqueryRef,
                               optimizerUnnestSubquery,
                               QCU_OPTIMIZER_UNNEST_SUBQUERY );

    QCG_MATCHED_PLAN_PROPERTY( optimizerUnnestComplexSubqueryRef,
                               optimizerUnnestComplexSubquery,
                               QCU_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY );

    QCG_MATCHED_PLAN_PROPERTY( optimizerUnnestAggregationSubqueryRef,
                               optimizerUnnestAggregationSubquery,
                               QCU_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY );

    /* PROJ-2242 Eliminate common subexpression */
    QCG_MATCHED_PLAN_PROPERTY( optimizerEliminateCommonSubexpressionRef,
                               optimizerEliminateCommonSubexpression,
                               QCG_GET_SESSION_ELIMINATE_COMMON_SUBEXPRESSION( aStatement ) );

    /* PROJ-2242 Constant filter subsumption */
    QCG_MATCHED_PLAN_PROPERTY( optimizerConstantFilterSubsumptionRef,
                               optimizerConstantFilterSubsumption,
                               QCU_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION );

    // BUG-37247
    QCG_MATCHED_PLAN_PROPERTY( sysConnectByPathPrecisionRef,
                               sysConnectByPathPrecision,
                               QCU_SYS_CONNECT_BY_PATH_PRECISION );

    // BUG-37247
    QCG_MATCHED_PLAN_PROPERTY( groupConcatPrecisionRef,
                               groupConcatPrecision,
                               MTU_GROUP_CONCAT_PRECISION );

    /* PROJ-2362  */
    QCG_MATCHED_PLAN_PROPERTY( reduceTempMemoryEnableRef,
                               reduceTempMemoryEnable,
                               QCU_REDUCE_TEMP_MEMORY_ENABLE );

    /* PROJ-2361 */
    QCG_MATCHED_PLAN_PROPERTY( avgTransformEnableRef,
                               avgTransformEnable,
                               QCU_AVERAGE_TRANSFORM_ENABLE );

    // BUG-38132
    QCG_MATCHED_PLAN_PROPERTY( fixedGroupMemoryTempRef,
                               fixedGroupMemoryTemp,
                               QCU_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP );

    // BUG-38339 Outer Join Elimination
    QCG_MATCHED_PLAN_PROPERTY( outerJoinEliminationRef,
                               outerJoinElimination,
                               QCU_OPTIMIZER_OUTERJOIN_ELIMINATION );

    // BUG-38434
    QCG_MATCHED_PLAN_PROPERTY( optimizerDnfDisableRef,
                               optimizerDnfDisable,
                               QCU_OPTIMIZER_DNF_DISABLE );

    // BUG-38416 count(column) to count(*)
    QCG_MATCHED_PLAN_PROPERTY( countColumnToCountAStarRef,
                               countColumnToCountAStar,
                               MTU_COUNT_COLUMN_TO_COUNT_ASTAR );

    // BUG-40022
    QCG_MATCHED_PLAN_PROPERTY( optimizerJoinDisableRef,
                               optimizerJoinDisable,
                               QCU_OPTIMIZER_JOIN_DISABLE );

    // PROJ-2469 Optimize View Materialization
    QCG_MATCHED_PLAN_PROPERTY( optimizerViewTargetEnableRef,
                               optimizerViewTargetEnable,
                               QCU_OPTIMIZER_VIEW_TARGET_ENABLE );    

    // BUG-41183 ORDER BY Elimination
    QCG_MATCHED_PLAN_PROPERTY( optimizerOrderByEliminationEnableRef,
                               optimizerOrderByEliminationEnable,
                               QCU_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE );

    // BUG-41249 DISTINCT Elimination
    QCG_MATCHED_PLAN_PROPERTY( optimizerDistinctEliminationEnableRef,
                               optimizerDistinctEliminationEnable,
                               QCU_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE );

    // BUG-41499
    QCG_MATCHED_PLAN_PROPERTY( arithmeticOpModeRef,
                               arithmeticOpMode,
                               QCG_GET_SESSION_ARITHMETIC_OP_MODE(aStatement) );

    /* PROJ-2462 Result Cache */
    QCG_MATCHED_PLAN_PROPERTY( resultCacheEnableRef,
                               resultCacheEnable,
                               QCG_GET_SESSION_RESULT_CACHE_ENABLE(aStatement) );
    /* PROJ-2462 Result Cache */
    QCG_MATCHED_PLAN_PROPERTY( topResultCacheModeRef,
                               topResultCacheMode,
                               QCG_GET_SESSION_TOP_RESULT_CACHE_MODE(aStatement) );
    /* PROJ-2616 */
    QCG_MATCHED_PLAN_PROPERTY( executorFastSimpleQueryRef,
                               executorFastSimpleQuery,
                               QCU_EXECUTOR_FAST_SIMPLE_QUERY );
    /* PROJ-2492 Dynamic sample selection */
    QCG_MATCHED_PLAN_PROPERTY( mOptimizerAutoStatsRef,
                               mOptimizerAutoStats,
                               QCG_GET_SESSION_OPTIMIZER_AUTO_STATS(aStatement) );

    // BUG-36438 LIST transformation
    QCG_MATCHED_PLAN_PROPERTY( optimizerListTransformationRef,
                               optimizerListTransformation,
                               QCU_OPTIMIZER_LIST_TRANSFORMATION );

    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    QCG_MATCHED_PLAN_PROPERTY( optimizerTransitivityOldRuleRef,
                               optimizerTransitivityOldRule,
                               QCG_GET_SESSION_OPTIMIZER_TRANSITIVITY_OLD_RULE(aStatement) );

    /* BUG-42639 Monitoring query */
    QCG_MATCHED_PLAN_PROPERTY( optimizerPerformanceViewRef,
                               optimizerPerformanceView,
                               QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) );

    // BUG-43039 inner join push down
    QCG_MATCHED_PLAN_PROPERTY( optimizerInnerJoinPushDownRef,
                               optimizerInnerJoinPushDown,
                               QCU_OPTIMIZER_INNER_JOIN_PUSH_DOWN );

    // BUG-43068 Indexable order by ����
    QCG_MATCHED_PLAN_PROPERTY( optimizerOrderPushDownRef,
                               optimizerOrderPushDown,
                               QCU_OPTIMIZER_ORDER_PUSH_DOWN );

    // BUG-43059 Target subquery unnest/removal disable
    QCG_MATCHED_PLAN_PROPERTY( optimizerTargetSubqueryUnnestDisableRef,
                               optimizerTargetSubqueryUnnestDisable,
                               QCU_OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE );

    QCG_MATCHED_PLAN_PROPERTY( optimizerTargetSubqueryRemovalDisableRef,
                               optimizerTargetSubqueryRemovalDisable,
                               QCU_OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE );

    // BUG-43493
    QCG_MATCHED_PLAN_PROPERTY( optimizerDelayedExecutionRef,
                               optimizerDelayedExecution,
                               QCU_OPTIMIZER_DELAYED_EXECUTION );

    /* BUG-43495 */
    QCG_MATCHED_PLAN_PROPERTY( optimizerLikeIndexScanWithOuterColumnDisableRef,
                               optimizerLikeIndexScanWithOuterColumnDisable,
                               QCU_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE );

    /* TASK-6744 */
    QCG_MATCHED_PLAN_PROPERTY( optimizerPlanRandomSeedRef,
                               optimizerPlanRandomSeed,
                               QCU_PLAN_RANDOM_SEED );

    /* PROJ-2641 Hierarchy Query Index */
    QCG_MATCHED_PLAN_PROPERTY( mOptimizerHierarchyTransformationRef,
                               mOptimizerHierarchyTransformation,
                               QCU_OPTIMIZER_HIERARCHY_TRANSFORMATION );

    // BUG-44795
    QCG_MATCHED_PLAN_PROPERTY( optimizerDBMSStatPolicyRef,
                               optimizerDBMSStatPolicy,
                               QCU_OPTIMIZER_DBMS_STAT_POLICY );

    /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
    QCG_MATCHED_PLAN_PROPERTY( optimizerIndexNLJoinAccessMethodPolicyRef,
                               optimizerIndexNLJoinAccessMethodPolicy,
                               QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY );

    QCG_MATCHED_PLAN_PROPERTY( optimizerSemiJoinRemoveRef,
                               optimizerSemiJoinRemove,
                               QCU_OPTIMIZER_SEMI_JOIN_REMOVE );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_MATCHED_PLAN_PROPERTY( mSMNForDataNodeRef,
                               mSMNForDataNode,
                               sdi::getSMNForDataNode() );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_MATCHED_PLAN_PROPERTY( mSMNForSessionRef,
                               mSMNForSession,
                               QCG_GET_SESSION_SHARD_META_NUMBER(aStatement) );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_MATCHED_PLAN_PROPERTY( mIsShardUserSessionRef,
                               mIsShardUserSession,
                               QCG_GET_SESSION_IS_SHARD_USER_SESSION(aStatement) );

    /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
    QCG_MATCHED_PLAN_PROPERTY( mCallByShardAnalyzeProtocolRef,
                               mCallByShardAnalyzeProtocol,
                               QCG_GET_CALL_BY_SHARD_ANALYZE_PROTOCOL(aStatement) );

    /* TASK-7219 Non-shard DML */
    QCG_MATCHED_PLAN_PROPERTY( mShardPartialExecTypeRef,
                               mShardPartialExecType,
                               QCG_GET_SHARD_PARTIAL_EXEC_TYPE(aStatement) );

    /* PROJ-2687 */
    QCG_MATCHED_PLAN_PROPERTY( mShardAggregationTransformEnableRef,
                               mShardAggregationTransformEnable,
                               SDU_SHARD_AGGREGATION_TRANSFORM_ENABLE );

    /* TASK-7219 */
    QCG_MATCHED_PLAN_PROPERTY( mShardTransformModeRef,
                               mShardTransformMode,
                               SDU_SHARD_TRANSFORM_MODE );

    // key preserved property
    QCG_MATCHED_PLAN_PROPERTY( mKeyPreservedTableRef,
                               mKeyPreservedTable,
                               QCU_KEY_PRESERVED_TABLE );

    QCG_MATCHED_PLAN_PROPERTY( mUnnestCompatibilityRef,
                               mUnnestCompatibility,
                               QCU_OPTIMIZER_UNNEST_COMPATIBILITY );

    /* PROJ-2632 */
    QCG_MATCHED_PLAN_PROPERTY( mSerialExecuteModeRef,
                               mSerialExecuteMode,
                               QCG_GET_SERIAL_EXECUTE_MODE( aStatement ) );

    /* BUG-46932 */ 
    QCG_MATCHED_PLAN_PROPERTY( mInverseJoinEnableRef,
                               mInverseJoinEnable,
                               QCU_OPTIMIZER_INVERSE_JOIN_ENABLE );

    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    QCG_MATCHED_PLAN_PROPERTY( mReducePartPrepareMemoryRef,
                               mReducePartPrepareMemory,
                               QCG_GET_REDUCE_PART_PREPARE_MEMORY( aStatement ) );

    QCG_MATCHED_PLAN_PROPERTY( mShardInPSMEnableRef,
                               mShardInPSMEnable,
                               QCG_GET_SESSION_SHARD_IN_PSM_ENABLE(aStatement) );

    /* BUG-47986 */
    QCG_MATCHED_PLAN_PROPERTY( mOrValueIndexRef,
                               mOrValueIndex,
                               QCU_OPTIMIZER_OR_VALUE_INDEX );

    /* BUG-48132 */
    QCG_MATCHED_PLAN_PROPERTY( mPlanHashOrSortMethodRef,
                               mPlanHashOrSortMethod,
                               QCG_GET_PLAN_HASH_OR_SORT_METHOD( aStatement ) );

    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    QCG_MATCHED_PLAN_PROPERTY( mIndexNlJoinPenaltyRef,
                               mIndexNlJoinPenalty,
                               QCU_OPTIMIZER_INDEX_NL_JOIN_PENALTY );

    /* BUG-48120 */
    QCG_MATCHED_PLAN_PROPERTY( mIndexCostModeRef,
                               mIndexCostMode,
                               QCU_OPTIMIZER_INDEX_COST_MODE );

    /* BUG-4861 */
    QCG_MATCHED_PLAN_PROPERTY( mBucketCountMaxRef,
                               mBucketCountMax,
                               QCG_GET_BUCKET_COUNT_MAX( aStatement ) );

    QCG_MATCHED_PLAN_PROPERTY( mShardStatusRef,
                               mShardStatus,
                               sdi::getShardStatus() );
    
    /* TASK-7307 */
    QCG_MATCHED_PLAN_PROPERTY( mShardInternalLocalOperationRef,
                               mShardInternalLocalOperation,
                               QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( aStatement ) );

    /* PROJ-2750 */
    QCG_MATCHED_PLAN_PROPERTY( mLeftOuterSkipRightEnableRef,
                               mLeftOuterSkipRightEnable,
                               QCU_LEFT_OUTER_SKIP_RIGHT_ENABLE );

    ////////////////////////////////////////////////////////////////////
    // QCG_MATCHED_PLAN_PROPERTY ��ũ�η� üũ�Ҽ� ���� ���
    ////////////////////////////////////////////////////////////////////


    ++sMatchedCount;
    if ( aPlanProperty->defaultDateFormatRef == ID_TRUE )
    {
        if ( sProperty->defaultDateFormatRef == ID_FALSE )
        {
            sProperty->defaultDateFormatRef = ID_TRUE;

            sDateFormat = QCG_GET_SESSION_DATE_FORMAT(aStatement);
            sDateFormatLen = idlOS::strlen( sDateFormat );

            IDE_DASSERT( sDateFormatLen < IDP_MAX_VALUE_LEN );

            idlOS::strncpy( sProperty->defaultDateFormat,
                            sDateFormat,
                            sDateFormatLen );
            sProperty->defaultDateFormat[sDateFormatLen] = '\0';
        }
        else
        {
            // Nothing to do.
        }

        if ( idlOS::strMatch( aPlanProperty->defaultDateFormat,
                              idlOS::strlen(aPlanProperty->defaultDateFormat),
                              sProperty->defaultDateFormat,
                              idlOS::strlen(sProperty->defaultDateFormat) ) != 0 )
        {
            sIsMatched = ID_FALSE;

            IDE_CONT( NOT_MATCHED );
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

    /* PROJ-2208 Multi Currency */
    ++sMatchedCount;
    if ( aPlanProperty->nlsCurrencyRef == ID_TRUE )
    {
        if ( sProperty->nlsCurrencyRef == ID_FALSE )
        {
            sProperty->nlsCurrencyRef = ID_TRUE;

            sFormat = QCG_GET_SESSION_CURRENCY( aStatement );
            sFormatLen = idlOS::strlen( sFormat );

            idlOS::strncpy( sProperty->nlsCurrency,
                            sFormat,
                            sFormatLen );
            sProperty->nlsCurrency[sFormatLen] = '\0';

            sFormat = QCG_GET_SESSION_ISO_CURRENCY( aStatement );
            sProperty->nlsISOCurrency[0] = *sFormat;
            sProperty->nlsISOCurrency[1] = *( sFormat + 1 );
            sProperty->nlsISOCurrency[2] = *( sFormat + 2 );
            sProperty->nlsISOCurrency[3] = '\0';


            sFormat = QCG_GET_SESSION_NUMERIC_CHAR( aStatement );
            sProperty->nlsNumericChar[0] = *sFormat;
            sProperty->nlsNumericChar[1] = *( sFormat + 1 );
            sProperty->nlsNumericChar[2] = '\0';
        }
        else
        {
            /* Nothing to do */
        }

        if ( idlOS::strMatch( aPlanProperty->nlsCurrency,
                              idlOS::strlen( aPlanProperty->nlsCurrency ),
                              sProperty->nlsCurrency,
                              idlOS::strlen( sProperty->nlsCurrency )) != 0 )
        {
            sIsMatched = ID_FALSE;
            IDE_CONT( NOT_MATCHED );
        }
        else
        {
            /* Nothing to do */
        }

        if (( aPlanProperty->nlsISOCurrency[0] != sProperty->nlsISOCurrency[0] )
               ||
            ( aPlanProperty->nlsISOCurrency[1] != sProperty->nlsISOCurrency[1] )
               ||
            ( aPlanProperty->nlsISOCurrency[2] != sProperty->nlsISOCurrency[2] ))
        {
            sIsMatched = ID_FALSE;
            IDE_CONT( NOT_MATCHED );
        }
        else
        {
            /* Nothing to do */
        }

        if (( aPlanProperty->nlsNumericChar[0] != sProperty->nlsNumericChar[0] )
                ||
            ( aPlanProperty->nlsNumericChar[1] != sProperty->nlsNumericChar[1] ))
        {
            sIsMatched = ID_FALSE;
            IDE_CONT( NOT_MATCHED );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2242 Feature enable */
    ++sMatchedCount;
    if ( aPlanProperty->optimizerFeatureEnableRef == ID_TRUE )
    {
        if ( sProperty->optimizerFeatureEnableRef == ID_FALSE )
        {
            sProperty->optimizerFeatureEnableRef = ID_TRUE;

            sFormat = QCU_OPTIMIZER_FEATURE_ENABLE;
            sFormatLen = idlOS::strlen( sFormat );

            idlOS::strncpy( sProperty->optimizerFeatureEnable,
                            sFormat,
                            sFormatLen );
            sProperty->optimizerFeatureEnable[sFormatLen] = '\0';
        }
        else
        {
            /* Nothing to do */
        }

        if ( idlOS::strMatch( aPlanProperty->optimizerFeatureEnable,
                              idlOS::strlen( aPlanProperty->optimizerFeatureEnable ),
                              sProperty->optimizerFeatureEnable,
                              idlOS::strlen( sProperty->optimizerFeatureEnable ) )
             != 0 )
        {
            sIsMatched = ID_FALSE;
            IDE_CONT( NOT_MATCHED );
        }
        else
        {
            /* Nothing to do */
        }
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-38148 ������Ƽ �߰��۾��� �Ǽ��� �ϸ� DASSERT �� �ɸ����� �Ѵ�.
    IDE_DASSERT( sMatchedCount == PLAN_PROPERTY_MAX);

    // BUG-45385 ���� ���ǿ��� plan cache ����� ������Ų ���
    if ( ( aStatement->session->mQPSpecific.mFlag & QC_SESSION_PLAN_CACHE_MASK ) ==
         QC_SESSION_PLAN_CACHE_DISABLE )
    {
        sIsMatched = ID_FALSE;
        IDE_CONT( NOT_MATCHED );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_EXCEPTION_CONT( NOT_MATCHED );

    *aIsMatched = sIsMatched;

    return IDE_SUCCESS;
}

IDE_RC qcgPlan::rebuildPlanProperty( qcStatement    * aStatement,
                                     qcPlanProperty * aPlanProperty )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     plan cache�� cache�� environment(plan property)�� �����Ѵ�.
 *
 *     rebuild�� plan�� ������Ǵµ� �̶� ���� plan���� �����ϴ�
 *     session & system property�� ��� ����Ǵ� ���� �ƴ϶�,
 *     rebuild ���������� ���ο� property�� ����Ǳ� ������
 *     plan cache�� cache�� environment(plan property)�� ���ο�
 *     property�� �����ؾ� �Ѵ�.
 *
 ***********************************************************************/

    SChar  * sFormat        = NULL;
    UInt     sFormatLen     = 0;
    UInt     sRebuildCount  = 0;

    ////////////////////////////////////////////////////////////////////
    // QCG_REBUILD_PLAN_PROPERTY ��ũ�η� üũ�Ҽ� �ִ� ���
    ////////////////////////////////////////////////////////////////////

    QCG_REBUILD_PLAN_PROPERTY( userIDRef,
                               userID,
                               QCG_GET_SESSION_USER_ID(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( optimizerModeRef,
                               optimizerMode,
                               QCG_GET_SESSION_OPTIMIZER_MODE(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( hostOptimizeEnableRef,
                               hostOptimizeEnable,
                               QCU_HOST_OPTIMIZE_ENABLE );

    QCG_REBUILD_PLAN_PROPERTY( selectHeaderDisplayRef,
                               selectHeaderDisplay,
                               QCG_GET_SESSION_SELECT_HEADER_DISPLAY(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( stackSizeRef,
                               stackSize,
                               QCG_GET_SESSION_STACK_SIZE(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( normalFormMaximumRef,
                               normalFormMaximum,
                               QCG_GET_SESSION_NORMALFORM_MAXIMUM(aStatement) );

    // To Fix BUG-27619
    QCG_REBUILD_PLAN_PROPERTY( STObjBufSizeRef,
                               STObjBufSize,
                               QCG_GET_SESSION_ST_OBJECT_SIZE(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( optimizerSimpleViewMergingDisableRef,
                               optimizerSimpleViewMergingDisable,
                               QCU_OPTIMIZER_SIMPLE_VIEW_MERGING_DISABLE );

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    QCG_REBUILD_PLAN_PROPERTY( optimizerDefaultTempTbsTypeRef,
                               optimizerDefaultTempTbsType,
                               QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( fakeTpchScaleFactorRef,
                               fakeTpchScaleFactor,
                               QCU_FAKE_TPCH_SCALE_FACTOR );

    QCG_REBUILD_PLAN_PROPERTY( fakeBufferSizeRef,
                               fakeBufferSize,
                               QCU_FAKE_BUFFER_SIZE );

    QCG_REBUILD_PLAN_PROPERTY( optimizerDiskIndexCostAdjRef,
                               optimizerDiskIndexCostAdj,
                               QCG_GET_SESSION_OPTIMIZER_DISK_INDEX_COST_ADJ(aStatement) );

    // BUG-43736
    QCG_REBUILD_PLAN_PROPERTY( optimizerMemoryIndexCostAdjRef,
                               optimizerMemoryIndexCostAdj,
                               QCG_GET_SESSION_OPTIMIZER_MEMORY_INDEX_COST_ADJ(aStatement) );

    QCG_REBUILD_PLAN_PROPERTY( optimizerHashJoinCostAdjRef,
                               optimizerHashJoinCostAdj,
                               QCU_OPTIMIZER_HASH_JOIN_COST_ADJ );

    QCG_REBUILD_PLAN_PROPERTY( optimizerSubqueryOptimizeMethodRef,
                               optimizerSubqueryOptimizeMethod,
                               QCU_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD );

    // BUG-34295
    QCG_REBUILD_PLAN_PROPERTY( optimizerAnsiJoinOrderingRef,
                               optimizerAnsiJoinOrdering,
                               QCU_OPTIMIZER_ANSI_JOIN_ORDERING );
    // BUG-38402
    QCG_REBUILD_PLAN_PROPERTY( optimizerAnsiInnerJoinConvertRef,
                               optimizerAnsiInnerJoinConvert,
                               QCU_OPTIMIZER_ANSI_INNER_JOIN_CONVERT );

    // BUG-34350
    QCG_REBUILD_PLAN_PROPERTY( optimizerRefinePrepareMemoryRef,
                               optimizerRefinePrepareMemory,
                               QCU_OPTIMIZER_REFINE_PREPARE_MEMORY );

    // BUG-35155 Partial CNF
    QCG_REBUILD_PLAN_PROPERTY( optimizerPartialNormalizeRef,
                               optimizerPartialNormalize,
                               QCU_OPTIMIZER_PARTIAL_NORMALIZE );

    /* PROJ-1090 Function-based Index */
    QCG_REBUILD_PLAN_PROPERTY( queryRewriteEnableRef,
                               queryRewriteEnable,
                               QCG_GET_SESSION_QUERY_REWRITE_ENABLE(aStatement) );

    // PROJ-1718
    QCG_REBUILD_PLAN_PROPERTY( optimizerUnnestSubqueryRef,
                               optimizerUnnestSubquery,
                               QCU_OPTIMIZER_UNNEST_SUBQUERY );

    QCG_REBUILD_PLAN_PROPERTY( optimizerUnnestComplexSubqueryRef,
                               optimizerUnnestComplexSubquery,
                               QCU_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY );

    QCG_REBUILD_PLAN_PROPERTY( optimizerUnnestAggregationSubqueryRef,
                               optimizerUnnestAggregationSubquery,
                               QCU_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY );

    /* PROJ-2242 Eliminate common subexpression */
    QCG_REBUILD_PLAN_PROPERTY( optimizerEliminateCommonSubexpressionRef,
                               optimizerEliminateCommonSubexpression,
                               QCG_GET_SESSION_ELIMINATE_COMMON_SUBEXPRESSION( aStatement ) );

    /* PROJ-2242 Constant filter subsumption */
    QCG_REBUILD_PLAN_PROPERTY( optimizerConstantFilterSubsumptionRef,
                               optimizerConstantFilterSubsumption,
                               QCU_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION );

    // BUG-37247
    QCG_REBUILD_PLAN_PROPERTY( sysConnectByPathPrecisionRef,
                               sysConnectByPathPrecision,
                               QCU_SYS_CONNECT_BY_PATH_PRECISION );

    // BUG-37247
    QCG_REBUILD_PLAN_PROPERTY( groupConcatPrecisionRef,
                               groupConcatPrecision,
                               MTU_GROUP_CONCAT_PRECISION );

    // PROJ-2362
    QCG_REBUILD_PLAN_PROPERTY( reduceTempMemoryEnableRef,
                               reduceTempMemoryEnable,
                               QCU_REDUCE_TEMP_MEMORY_ENABLE );

    /* PROJ-2361 */
    QCG_REBUILD_PLAN_PROPERTY( avgTransformEnableRef,
                               avgTransformEnable,
                               QCU_AVERAGE_TRANSFORM_ENABLE );

    // BUG-38132
    QCG_REBUILD_PLAN_PROPERTY( fixedGroupMemoryTempRef,
                               fixedGroupMemoryTemp,
                               QCU_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP );

    // BUG-38339 Outer Join Elimination
    QCG_REBUILD_PLAN_PROPERTY( outerJoinEliminationRef,
                               outerJoinElimination,
                               QCU_OPTIMIZER_OUTERJOIN_ELIMINATION );

    // BUG-38434
    QCG_REBUILD_PLAN_PROPERTY( optimizerDnfDisableRef,
                               optimizerDnfDisable,
                               QCU_OPTIMIZER_DNF_DISABLE );

    // BUG-38416 count(column) to count(*)
    QCG_REBUILD_PLAN_PROPERTY( countColumnToCountAStarRef,
                               countColumnToCountAStar,
                               MTU_COUNT_COLUMN_TO_COUNT_ASTAR );

    // BUG-40022
    QCG_REBUILD_PLAN_PROPERTY( optimizerJoinDisableRef,
                               optimizerJoinDisable,
                               QCU_OPTIMIZER_JOIN_DISABLE );

    // PROJ-2469 Optimize View Materialization
    QCG_REBUILD_PLAN_PROPERTY( optimizerViewTargetEnableRef,
                               optimizerViewTargetEnable,
                               QCU_OPTIMIZER_VIEW_TARGET_ENABLE );    

    // BUG-41183 ORDER BY Elimination
    QCG_REBUILD_PLAN_PROPERTY( optimizerOrderByEliminationEnableRef,
                               optimizerOrderByEliminationEnable,
                               QCU_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE );

    // BUG-41249 DISTINCT Elimination
    QCG_REBUILD_PLAN_PROPERTY( optimizerDistinctEliminationEnableRef,
                               optimizerDistinctEliminationEnable,
                               QCU_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE );

    // BUG-41499
    QCG_REBUILD_PLAN_PROPERTY( arithmeticOpModeRef,
                               arithmeticOpMode,
                               QCG_GET_SESSION_ARITHMETIC_OP_MODE(aStatement) );

    /* PROJ-2462 Result Cache */
    QCG_REBUILD_PLAN_PROPERTY( resultCacheEnableRef,
                               resultCacheEnable,
                               QCG_GET_SESSION_RESULT_CACHE_ENABLE(aStatement) );
    /* PROJ-2462 Result Cache */
    QCG_REBUILD_PLAN_PROPERTY( topResultCacheModeRef,
                               topResultCacheMode,
                               QCG_GET_SESSION_TOP_RESULT_CACHE_MODE(aStatement) );
    /* PROJ-2616 */
    QCG_REBUILD_PLAN_PROPERTY( executorFastSimpleQueryRef,
                               executorFastSimpleQuery,
                               QCU_EXECUTOR_FAST_SIMPLE_QUERY );
    /* PROJ-2492 Dynamic sample selection */
    QCG_REBUILD_PLAN_PROPERTY( mOptimizerAutoStatsRef,
                               mOptimizerAutoStats,
                               QCG_GET_SESSION_OPTIMIZER_AUTO_STATS(aStatement) );

    // BUG-36438 LIST transformation
    QCG_REBUILD_PLAN_PROPERTY( optimizerListTransformationRef,
                               optimizerListTransformation,
                               QCU_OPTIMIZER_LIST_TRANSFORMATION );

    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    QCG_REBUILD_PLAN_PROPERTY( optimizerTransitivityOldRuleRef,
                               optimizerTransitivityOldRule,
                               QCG_GET_SESSION_OPTIMIZER_TRANSITIVITY_OLD_RULE(aStatement) );

    /* BUG-42639 Monitoring query */
    QCG_REBUILD_PLAN_PROPERTY( optimizerPerformanceViewRef,
                               optimizerPerformanceView,
                               QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW(aStatement) );

    // BUG-43039 inner join push down
    QCG_REBUILD_PLAN_PROPERTY( optimizerInnerJoinPushDownRef,
                               optimizerInnerJoinPushDown,
                               QCU_OPTIMIZER_INNER_JOIN_PUSH_DOWN );

    // BUG-43068 Indexable order by ����
    QCG_REBUILD_PLAN_PROPERTY( optimizerOrderPushDownRef,
                               optimizerOrderPushDown,
                               QCU_OPTIMIZER_ORDER_PUSH_DOWN );

    // BUG-43059 Target subquery unnest/removal disable
    QCG_REBUILD_PLAN_PROPERTY( optimizerTargetSubqueryUnnestDisableRef,
                               optimizerTargetSubqueryUnnestDisable,
                               QCU_OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE );

    QCG_REBUILD_PLAN_PROPERTY( optimizerTargetSubqueryRemovalDisableRef,
                               optimizerTargetSubqueryRemovalDisable,
                               QCU_OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE );

    // BUG-43493
    QCG_REBUILD_PLAN_PROPERTY( optimizerDelayedExecutionRef,
                               optimizerDelayedExecution,
                               QCU_OPTIMIZER_DELAYED_EXECUTION );
    
    /* BUG-43495 */
    QCG_REBUILD_PLAN_PROPERTY( optimizerLikeIndexScanWithOuterColumnDisableRef,
                               optimizerLikeIndexScanWithOuterColumnDisable,
                               QCU_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE );
    /* TASK-6744 */
    QCG_REBUILD_PLAN_PROPERTY( optimizerPlanRandomSeedRef,
                               optimizerPlanRandomSeed,
                               QCU_PLAN_RANDOM_SEED );

    /* PROJ-2641 Hierarchy Query Index */
    QCG_REBUILD_PLAN_PROPERTY( mOptimizerHierarchyTransformationRef,
                               mOptimizerHierarchyTransformation,
                               QCU_OPTIMIZER_HIERARCHY_TRANSFORMATION );

    // BUG-44795
    QCG_REBUILD_PLAN_PROPERTY( optimizerDBMSStatPolicyRef,
                               optimizerDBMSStatPolicy,
                               QCU_OPTIMIZER_DBMS_STAT_POLICY );

    /* BUG-44850 Index NL , Inverse index NL ���� ����ȭ ����� ����� �����ϸ� primary key�� �켱������ ����. */
    QCG_REBUILD_PLAN_PROPERTY( optimizerIndexNLJoinAccessMethodPolicyRef,
                               optimizerIndexNLJoinAccessMethodPolicy,
                               QCU_OPTIMIZER_INDEX_NL_JOIN_ACCESS_METHOD_POLICY );

    QCG_REBUILD_PLAN_PROPERTY( optimizerSemiJoinRemoveRef,
                               optimizerSemiJoinRemove,
                               QCU_OPTIMIZER_SEMI_JOIN_REMOVE );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_REBUILD_PLAN_PROPERTY( mSMNForDataNodeRef,
                               mSMNForDataNode,
                               sdi::getSMNForDataNode() );

    /* TASK-7219 Non-shard DML */
    QCG_REBUILD_PLAN_PROPERTY( mShardPartialExecTypeRef,
                               mShardPartialExecType,
                               QCG_GET_SHARD_PARTIAL_EXEC_TYPE(aStatement) );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_REBUILD_PLAN_PROPERTY( mSMNForSessionRef,
                               mSMNForSession,
                               QCG_GET_SESSION_SHARD_META_NUMBER(aStatement) );

    /* PROJ-2701 Sharding online data rebuild */
    QCG_REBUILD_PLAN_PROPERTY( mIsShardUserSessionRef,
                               mIsShardUserSession,
                               QCG_GET_SESSION_IS_SHARD_USER_SESSION(aStatement) );

    /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
    QCG_REBUILD_PLAN_PROPERTY( mCallByShardAnalyzeProtocolRef,
                               mCallByShardAnalyzeProtocol,
                               QCG_GET_CALL_BY_SHARD_ANALYZE_PROTOCOL(aStatement) );

    /* PROJ-2687 */
    QCG_REBUILD_PLAN_PROPERTY( mShardAggregationTransformEnableRef,
                               mShardAggregationTransformEnable,
                               SDU_SHARD_AGGREGATION_TRANSFORM_ENABLE );

    /* TASK-7219 */
    QCG_REBUILD_PLAN_PROPERTY( mShardTransformModeRef,
                               mShardTransformMode,
                               SDU_SHARD_TRANSFORM_MODE );
    
    // key preserved property
    QCG_REBUILD_PLAN_PROPERTY( mKeyPreservedTableRef,
                               mKeyPreservedTable,
                               QCU_KEY_PRESERVED_TABLE );

    QCG_REBUILD_PLAN_PROPERTY( mUnnestCompatibilityRef,
                               mUnnestCompatibility,
                               QCU_OPTIMIZER_UNNEST_COMPATIBILITY );

    /* PROJ-2632 */
    QCG_REBUILD_PLAN_PROPERTY( mSerialExecuteModeRef,
                               mSerialExecuteMode,
                               QCG_GET_SERIAL_EXECUTE_MODE( aStatement ) );

    /* BUG-46932 */ 
    QCG_REBUILD_PLAN_PROPERTY( mInverseJoinEnableRef,
                               mInverseJoinEnable,
                               QCU_OPTIMIZER_INVERSE_JOIN_ENABLE );

    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    QCG_REBUILD_PLAN_PROPERTY( mReducePartPrepareMemoryRef,
                               mReducePartPrepareMemory,
                               QCG_GET_REDUCE_PART_PREPARE_MEMORY( aStatement ) );

    QCG_REBUILD_PLAN_PROPERTY( mShardInPSMEnableRef,
                               mShardInPSMEnable,
                               QCG_GET_SESSION_SHARD_IN_PSM_ENABLE(aStatement) );

    /* BUG-47986 */
    QCG_REBUILD_PLAN_PROPERTY( mOrValueIndexRef,
                               mOrValueIndex,
                               QCU_OPTIMIZER_OR_VALUE_INDEX );

    /* BUG-48132 */
    QCG_REBUILD_PLAN_PROPERTY( mPlanHashOrSortMethodRef,
                               mPlanHashOrSortMethod,
                               QCG_GET_PLAN_HASH_OR_SORT_METHOD( aStatement ) );

    /* BUG-48135 NL Join Penalty ���� �����Ҽ� �ִ� property �߰� */
    QCG_REBUILD_PLAN_PROPERTY( mIndexNlJoinPenaltyRef,
                               mIndexNlJoinPenalty,
                               QCU_OPTIMIZER_INDEX_NL_JOIN_PENALTY );

    /* BUG- 48120 */
    QCG_REBUILD_PLAN_PROPERTY( mIndexCostModeRef,
                               mIndexCostMode,
                               QCU_OPTIMIZER_INDEX_COST_MODE );

    /* BUG-48161 */
    QCG_REBUILD_PLAN_PROPERTY( mBucketCountMaxRef,
                               mBucketCountMax,
                               QCG_GET_BUCKET_COUNT_MAX( aStatement ) );

    QCG_REBUILD_PLAN_PROPERTY( mShardStatusRef,
                               mShardStatus,
                               sdi::getShardStatus() );    
    
    /* TASK-7307 */
    QCG_REBUILD_PLAN_PROPERTY( mShardInternalLocalOperationRef,
                               mShardInternalLocalOperation,
                               QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( aStatement ) );    

    /* PROJ-2750 */
    QCG_REBUILD_PLAN_PROPERTY( mLeftOuterSkipRightEnableRef,
                               mLeftOuterSkipRightEnable,
                               QCU_LEFT_OUTER_SKIP_RIGHT_ENABLE );

    ////////////////////////////////////////////////////////////////////
    // QCG_REBUILD_PLAN_PROPERTY ��ũ�η� üũ�Ҽ� ���� ���
    ////////////////////////////////////////////////////////////////////

    ++sRebuildCount;
    if ( aPlanProperty->defaultDateFormatRef == ID_TRUE )
    {
        sFormat = QCG_GET_SESSION_DATE_FORMAT(aStatement);
        sFormatLen = idlOS::strlen( sFormat );

        idlOS::strncpy( aPlanProperty->defaultDateFormat,
                        sFormat,
                        sFormatLen );
        aPlanProperty->defaultDateFormat[sFormatLen] = '\0';
    }
    else
    {
        // Nothing to do.
    }

    /* PROJ-2208 Multi Currency */
    ++sRebuildCount;
    if ( aPlanProperty->nlsCurrencyRef == ID_TRUE )
    {
        sFormat    = QCG_GET_SESSION_ISO_CURRENCY( aStatement );
        sFormatLen = idlOS::strlen( sFormat );

        idlOS::strncpy( aPlanProperty->nlsISOCurrency,
                        sFormat,
                        sFormatLen );
        aPlanProperty->nlsISOCurrency[sFormatLen] = '\0';

        sFormat    = QCG_GET_SESSION_NUMERIC_CHAR( aStatement );
        sFormatLen = idlOS::strlen( sFormat );

        idlOS::strncpy( aPlanProperty->nlsNumericChar,
                        sFormat,
                        sFormatLen );
        aPlanProperty->nlsNumericChar[sFormatLen] = '\0';

        sFormat    = QCG_GET_SESSION_CURRENCY( aStatement );
        sFormatLen = idlOS::strlen( sFormat );

        idlOS::strncpy( aPlanProperty->nlsCurrency,
                        sFormat,
                        sFormatLen );
        aPlanProperty->nlsCurrency[sFormatLen] = '\0';
    }
    else
    {
        /* Nothing to do */
    }

    /* PROJ-2242 Feature enable */
    ++sRebuildCount;
    if ( aPlanProperty->optimizerFeatureEnableRef == ID_TRUE )
    {
        sFormat    = QCU_OPTIMIZER_FEATURE_ENABLE;
        sFormatLen = idlOS::strlen( sFormat );

        idlOS::strncpy( aPlanProperty->optimizerFeatureEnable,
                        sFormat,
                        sFormatLen );
        aPlanProperty->optimizerFeatureEnable[sFormatLen] = '\0';
    }
    else
    {
        /* Nothing to do */
    }

    // BUG-38148 ������Ƽ �߰��۾��� �Ǽ��� �ϸ� DASSERT �� �ɸ����� �Ѵ�.
    IDE_DASSERT( sRebuildCount == PLAN_PROPERTY_MAX );

    return IDE_SUCCESS;
}

IDE_RC qcgPlan::validatePlanTable( qcgEnvTableList * aTableList,
                                   UInt              aUserID, /* BUG-45893 */
                                   idBool          * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� table ��ü�� ���Ͽ� ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvTableList * sTable;
    idBool            sIsValid = ID_TRUE;

    for ( sTable = aTableList;
          sTable != NULL;
          sTable = sTable->next )
    {
        /* BUG-45893 */
        if ( sTable->mIsDBAUser == ID_TRUE )
        {
            if ( ( aUserID == QC_SYSTEM_USER_ID ) ||
                 ( aUserID == QC_SYS_USER_ID ) )
            {
                sIsValid = ID_TRUE;
            }
            else
            {
                sIsValid = ID_FALSE;
                break;
            }
        }
        else
        {
            /* Nothing to do */
        }

        // ������ ������ �����Ѵ�.
        if ( smiValidateObjects( sTable->tableHandle,
                                 sTable->tableSCN )
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }

    *aIsValid = sIsValid;

    return IDE_SUCCESS;
}

IDE_RC qcgPlan::validatePlanSequence( qcgEnvSequenceList * aSequenceList,
                                      idBool             * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� sequence ��ü�� ���Ͽ� ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvSequenceList * sSequence;
    idBool               sIsValid = ID_TRUE;

    for ( sSequence = aSequenceList;
          sSequence != NULL;
          sSequence = sSequence->next )
    {
        // ������ �����Ѵ�.
        if ( smiValidateObjects( sSequence->sequenceHandle,
                                 sSequence->sequenceSCN )
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    *aIsValid = sIsValid;

    return IDE_SUCCESS;
}

IDE_RC qcgPlan::validatePlanProc( qcgEnvProcList * aProcList,
                                  idBool         * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� PSM ��ü�� ���Ͽ� ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvProcList * sProc;
    qsxProcInfo    * sProcInfo;
    idBool           sIsValid = ID_TRUE;
    UInt             sLatchCount = 0;
    UInt             i;

    for ( sProc = aProcList;
          sProc != NULL;
          sProc = sProc->next )
    {
        /* PROJ-2268 Reuse Catalog Table Slot
         * procID�� Latch�� ��� ���� Slot�� ����Ǿ����� Ȯ���Ͽ��� �Ѵ�.
         * ����Ǿ��ٸ� Latch ��ü�� �̹� �ʱ�ȭ �Ǿ� ������ �ֱ� ���� */
        if ( smiValidatePlanHandle( sProc->procHandle, 
                                    sProc->procSCN ) 
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            /* nothing to do ... */
        }

        // latch proc
        if ( qsxProc::latchS( sProc->procID )
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }
        
        sLatchCount++;
    }

    if ( sIsValid == ID_TRUE )
    {
        for ( sProc = aProcList;
              sProc != NULL;
              sProc = sProc->next )
        {
            // check proc
            IDE_TEST( qsxProc::getProcInfo( sProc->procID,
                                            & sProcInfo )
                      != IDE_SUCCESS );
            
            if ( sProcInfo == NULL )
            {
                sIsValid = ID_FALSE;
                break;
            }
            else
            {
                if ( sProcInfo->isValid != ID_TRUE )
                {
                    sIsValid = ID_FALSE;
                    break;
                }
                else
                {
                    if ( sProc->modifyCount != sProcInfo->modifyCount )
                    {
                        sIsValid = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // valid proc
                        // Nothing to do.
                    }
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( sIsValid == ID_FALSE )
    {
        // unlatch proc
        for ( i = 0, sProc = aProcList;
              i < sLatchCount;
              i ++, sProc = sProc->next )
        {
            (void) qsxProc::unlatch( sProc->procID );
        }
    }
    else
    {
        // Nothing to do.
    }

    *aIsValid = sIsValid;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::validatePlanSynonym( qcStatement       * aStatement,
                                     qcgEnvSynonymList * aSynonymList,
                                     idBool            * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� synonym ��ü�� ���Ͽ� ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvSynonymList * sSynonym;
    idBool              sIsValid = ID_TRUE;

    for ( sSynonym = aSynonymList;
          sSynonym != NULL;
          sSynonym = sSynonym->next )
    {
        if ( sSynonym->tableHandle != NULL )
        {
            IDE_TEST( validatePlanSynonymTable( aStatement,
                                                sSynonym,
                                                & sIsValid )
                      != IDE_SUCCESS );
        }
        else
        {
            if ( sSynonym->sequenceHandle != NULL )
            {
                IDE_TEST( validatePlanSynonymSequence( aStatement,
                                                       sSynonym,
                                                       & sIsValid )
                          != IDE_SUCCESS );
            }
            else
            {
                if ( sSynonym->objectID != QS_EMPTY_OID )
                {
                    IDE_TEST( validatePlanSynonymProc( aStatement,
                                                       sSynonym,
                                                       & sIsValid )
                              != IDE_SUCCESS );
                }
                else
                {
                    // Nothing to do.
                }
            }
        }

        // BUG-24206
        // �ϳ��� invalid�ϴٸ� invalid�� �����Ѵ�.
        if ( sIsValid == ID_FALSE )
        {
            break;
        }
        else
        {
            // Nothing to do.
        }
    }
    
    *aIsValid = sIsValid;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::validatePlanSynonymTable( qcStatement       * aStatement,
                                          qcgEnvSynonymList * aSynonym,
                                          idBool            * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� synonym ��ü�� ������ü�� table ��ü�� ���
 *     ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcNamePosition   sUserName;
    qcNamePosition   sObjectName;
    qcmTableInfo   * sTableInfo;
    UInt             sUserID;
    void           * sTableHandle;
    smSCN            sSCN;
    idBool           sExist;
    qcmSynonymInfo   sSynonymInfo;
    idBool           sIsValid = ID_TRUE;

    if ( aSynonym->userName[0] != '\0' )
    {
        sUserName.stmtText = aSynonym->userName;
        sUserName.offset = 0;
        sUserName.size = idlOS::strlen(aSynonym->userName);
    }
    else
    {
        SET_EMPTY_POSITION( sUserName );
    }

    if ( aSynonym->objectName[0] != '\0' )
    {
        sObjectName.stmtText = aSynonym->objectName;
        sObjectName.offset = 0;
        sObjectName.size = idlOS::strlen(aSynonym->objectName);
    }
    else
    {
        sIsValid = ID_FALSE;
        IDE_CONT(NORMAL_EXIT);
    }

    if ( QCU_SQL_PLAN_CACHE_VALID_MODE == 1 )
    {
        /* BUG-48594 */
        IDE_TEST( qcmSynonym::resolveTableViewQueue4PlanCache( aStatement,
                                                               sUserName,
                                                               sObjectName,
                                                               &sExist,
                                                               &sSynonymInfo,
                                                               &sTableHandle )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( qcmSynonym::resolveTableViewQueue(
                      aStatement,
                      sUserName,
                      sObjectName,
                      &sTableInfo,
                      &sUserID,
                      &sSCN,
                      &sExist,
                      &sSynonymInfo,
                      &sTableHandle )
                  != IDE_SUCCESS);
    }

    if ( sExist == ID_FALSE )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        if ( sSynonymInfo.isSynonymName == ID_FALSE )
        {
            sIsValid = ID_FALSE;
        }
        else
        {
            if ( sSynonymInfo.isPublicSynonym != aSynonym->isPublicSynonym )
            {
                sIsValid = ID_FALSE;
            }
            else
            {
                if ( sTableHandle != aSynonym->tableHandle )
                {
                    sIsValid = ID_FALSE;
                }
                else
                {
                    /* PROJ-2268 Reuse Catalog Table Slot */
                    if ( smiValidatePlanHandle( sTableHandle,
                                                aSynonym->objectSCN )
                         != IDE_SUCCESS )
                    {
                        sIsValid = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
        }
    }

    IDE_EXCEPTION_CONT(NORMAL_EXIT);
    
    *aIsValid = sIsValid;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::validatePlanSynonymSequence( qcStatement       * aStatement,
                                             qcgEnvSynonymList * aSynonym,
                                             idBool            * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� synonym ��ü�� ������ü�� sequence ��ü�� ���
 *     ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    qcNamePosition   sUserName;
    qcNamePosition   sObjectName;
    qcmSequenceInfo  sSequenceInfo;
    UInt             sUserID;
    void           * sSequenceHandle;
    idBool           sExist;
    qcmSynonymInfo   sSynonymInfo;
    idBool           sIsValid = ID_TRUE;

    if ( aSynonym->userName[0] != '\0' )
    {
        sUserName.stmtText = aSynonym->userName;
        sUserName.offset = 0;
        sUserName.size = idlOS::strlen(aSynonym->userName);
    }
    else
    {
        SET_EMPTY_POSITION( sUserName );
    }

    if ( aSynonym->objectName[0] != '\0' )
    {
        sObjectName.stmtText = aSynonym->objectName;
        sObjectName.offset = 0;
        sObjectName.size = idlOS::strlen(aSynonym->objectName);
    }
    else
    {
        IDE_RAISE( ERR_EMPTY_OBJECT_NAME );
    }

    if ( QCU_SQL_PLAN_CACHE_VALID_MODE == 1 )
    {
        /* BUG-48594 */
        IDE_TEST( qcmSynonym::resolveSequence4PlanCache( aStatement,
                                                         sUserName,
                                                         sObjectName,
                                                         &sSequenceInfo,
                                                         &sExist,
                                                         &sSynonymInfo,
                                                         &sSequenceHandle )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qcmSynonym::resolveSequence(
                      aStatement,
                      sUserName,
                      sObjectName,
                      &sSequenceInfo,
                      &sUserID,
                      &sExist,
                      &sSynonymInfo,
                      &sSequenceHandle )
                  != IDE_SUCCESS);
    }

    if ( sExist == ID_FALSE )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        if ( sSynonymInfo.isSynonymName == ID_FALSE )
        {
            sIsValid = ID_FALSE;
        }
        else
        {
            if ( sSynonymInfo.isPublicSynonym != aSynonym->isPublicSynonym )
            {
                sIsValid = ID_FALSE;
            }
            else
            {
                if ( sSequenceHandle != aSynonym->sequenceHandle )
                {
                    sIsValid = ID_FALSE;
                }
                else
                {
                    /* PROJ-2268 Reuse Catalog Table Slot */
                    if ( smiValidatePlanHandle( sSequenceHandle,
                                                aSynonym->objectSCN )
                         != IDE_SUCCESS )
                    {
                        sIsValid = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
        }
    }

    *aIsValid = sIsValid;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_EMPTY_OBJECT_NAME )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "IDE_RC qcgPlan::validatePlanSynonymSequence",
                                  "Object name is empty." ) );
    }
    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::validatePlanSynonymProc( qcStatement       * aStatement,
                                         qcgEnvSynonymList * aSynonym,
                                         idBool            * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� synonym ��ü�� ������ü�� PSM ��ü�� ���
 *     ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcNamePosition   sUserName;
    qcNamePosition   sObjectName;
    qsOID            sProcID;
    UInt             sUserID;
    idBool           sExist;
    qcmSynonymInfo   sSynonymInfo;
    idBool           sIsValid = ID_TRUE;

    if ( aSynonym->userName[0] != '\0' )
    {
        sUserName.stmtText = aSynonym->userName;
        sUserName.offset = 0;
        sUserName.size = idlOS::strlen(aSynonym->userName);
    }
    else
    {
        SET_EMPTY_POSITION( sUserName );
    }

    if ( aSynonym->objectName[0] != '\0' )
    {
        sObjectName.stmtText = aSynonym->objectName;
        sObjectName.offset = 0;
        sObjectName.size = idlOS::strlen(aSynonym->objectName);
    }
    else
    {
        SET_EMPTY_POSITION( sObjectName );
    }

    if ( QCU_SQL_PLAN_CACHE_VALID_MODE == 1 )
    {
        /* BUG-48594 */
        IDE_TEST( qcmSynonym::resolvePSM4PlanCache( aStatement,
                                                    sUserName,
                                                    sObjectName,
                                                    &sProcID,
                                                    &sExist,
                                                    &sSynonymInfo )
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_TEST( qcmSynonym::resolvePSM(
                      aStatement,
                      sUserName,
                      sObjectName,
                      &sProcID,
                      &sUserID,
                      &sExist,
                      &sSynonymInfo )
                  != IDE_SUCCESS);
    }

    if ( sExist == ID_FALSE )
    {
        sIsValid = ID_FALSE;
    }
    else
    {
        if ( sSynonymInfo.isSynonymName == ID_FALSE )
        {
            sIsValid = ID_FALSE;
        }
        else
        {
            if ( sSynonymInfo.isPublicSynonym != aSynonym->isPublicSynonym )
            {
                sIsValid = ID_FALSE;
            }
            else
            {
                if ( sProcID != aSynonym->objectID )
                {
                    sIsValid = ID_FALSE;
                }
                else
                {
                    /* PROJ-2268 Reuse Catalog Table Slot */
                    if ( smiValidatePlanHandle( aSynonym->objectHandle,
                                                aSynonym->objectSCN )
                         != IDE_SUCCESS )
                    {
                        sIsValid = ID_FALSE;
                    }
                    else
                    {
                        // Nothing to do.
                    }
                }
            }
        }
    }

    *aIsValid = sIsValid;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

// PROJ-1073 Package
IDE_RC qcgPlan::validatePlanPkg( qcgEnvPkgList  * aPkgList,
                                 idBool         * aIsValid )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� PACKAGE ��ü�� ���Ͽ� ������ ������ �˻��Ѵ�.
 *
 ***********************************************************************/

    qcgEnvPkgList * sPkg;
    qsxPkgInfo    * sPkgInfo;
    idBool          sIsValid = ID_TRUE;
    UInt            sLatchCount = 0;
    UInt            i;

    for ( sPkg = aPkgList;
          sPkg != NULL;
          sPkg = sPkg->next )
    {
        /* PROJ-2268 Reuse Catalog Table Slot
         * procID�� Latch�� ��� ���� Slot�� ����Ǿ����� Ȯ���Ͽ��� �Ѵ�.
         * ����Ǿ��ٸ� Latch ��ü�� �̹� �ʱ�ȭ �Ǿ� ������ �ֱ� ���� */
        if ( smiValidatePlanHandle( sPkg->pkgHandle, 
                                    sPkg->pkgSCN ) 
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            /* nothing to do ... */
        }

        // latch pkg
        if ( qsxPkg::latchS( sPkg->pkgID )
             != IDE_SUCCESS )
        {
            sIsValid = ID_FALSE;
            break;
        }
        else
        {
            // Nothing to do.
        }

        sLatchCount++;
    }

    if ( sIsValid == ID_TRUE )
    {
        for ( sPkg = aPkgList;
              sPkg != NULL;
              sPkg = sPkg->next )
        {
            // check pkg
            IDE_TEST( qsxPkg::getPkgInfo( sPkg->pkgID,
                                          & sPkgInfo )
                      != IDE_SUCCESS );

            if ( sPkgInfo == NULL )
            {
                sIsValid = ID_FALSE;
                break;
            }
            else
            {
                if ( sPkgInfo->isValid != ID_TRUE )
                {
                    sIsValid = ID_FALSE;
                    break;
                }
                else
                {
                    if ( sPkg->modifyCount != sPkgInfo->modifyCount )
                    {
                        sIsValid = ID_FALSE;
                        break;
                    }
                    else
                    {
                        // valid pkg
                        // Nothing to do.
                    }
                }
            }
        }
    }
    else
    {
        // Nothing to do.
    }

    if ( sIsValid == ID_FALSE )
    {
        // unlatch pkg
        for ( i = 0, sPkg = aPkgList;
              i < sLatchCount;
              i ++, sPkg = sPkg->next )
        {
            (void) qsxPkg::unlatch( sPkg->pkgID );
        }
    }
    else
    {
        // Nothing to do.
    }

    *aIsValid = sIsValid;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aIsValid = ID_FALSE;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::checkPlanPrivTable(
    qcStatement                        * aStatement,
    qcgEnvTablePrivList                * aTableList,
    qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
    void                               * aGetSmiStmt4PrepareContext )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� table ��ü�� ���� ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvTablePrivList * sTable;

    for ( sTable = aTableList;
          sTable != NULL;
          sTable = sTable->next )
    {
        switch ( sTable->privilegeID )
        {
            case QCM_PRIV_ID_OBJECT_SELECT_NO:
                IDE_TEST( qdpRole::checkDMLSelectTablePriv(
                              aStatement,
                              sTable->tableOwnerID,
                              sTable->privilegeCount,
                              sTable->privilegeInfo,
                              ID_TRUE,
                              aGetSmiStmt4PrepareCallback,
                              aGetSmiStmt4PrepareContext )
                          != IDE_SUCCESS );
                break;
                
            case QCM_PRIV_ID_OBJECT_INSERT_NO:
                IDE_TEST( qdpRole::checkDMLInsertTablePriv(
                              aStatement,
                              sTable->tableHandle,
                              sTable->tableOwnerID,
                              sTable->privilegeCount,
                              sTable->privilegeInfo,
                              ID_TRUE,
                              aGetSmiStmt4PrepareCallback,
                              aGetSmiStmt4PrepareContext )
                          != IDE_SUCCESS );
                break;
                
            case QCM_PRIV_ID_OBJECT_UPDATE_NO:
                IDE_TEST( qdpRole::checkDMLUpdateTablePriv(
                              aStatement,
                              sTable->tableHandle,
                              sTable->tableOwnerID,
                              sTable->privilegeCount,
                              sTable->privilegeInfo,
                              ID_TRUE,
                              aGetSmiStmt4PrepareCallback,
                              aGetSmiStmt4PrepareContext )
                          != IDE_SUCCESS );
                break;
                
            case QCM_PRIV_ID_OBJECT_DELETE_NO:
                IDE_TEST( qdpRole::checkDMLDeleteTablePriv(
                              aStatement,
                              sTable->tableHandle,
                              sTable->tableOwnerID,
                              sTable->privilegeCount,
                              sTable->privilegeInfo,
                              ID_TRUE,
                              aGetSmiStmt4PrepareCallback,
                              aGetSmiStmt4PrepareContext )
                          != IDE_SUCCESS );
                break;
                
            default:
                IDE_DASSERT( 0 );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::checkPlanPrivSequence(
    qcStatement                        * aStatement,
    qcgEnvSequencePrivList             * aSequenceList,
    qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
    void                               * aGetSmiStmt4PrepareContext )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� sequence ��ü�� ���� ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvSequencePrivList * sSequence;

    for ( sSequence = aSequenceList;
          sSequence != NULL;
          sSequence = sSequence->next )
    {
        IDE_TEST( qdpRole::checkDMLSelectSequencePriv(
                      aStatement,
                      sSequence->sequenceOwnerID,
                      sSequence->sequenceID,
                      ID_TRUE,
                      aGetSmiStmt4PrepareCallback,
                      aGetSmiStmt4PrepareContext )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::checkPlanPrivProc(
    qcStatement                        * aStatement,
    qcgEnvProcList                     * aProcList,
    qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
    void                               * aGetSmiStmt4PrepareContext )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     environment�� ��ϵ� PSM ��ü�� ���� ������ �˻��Ѵ�.
 *
 ***********************************************************************/
    
    qcgEnvProcList * sProc;
    qsxProcInfo    * sProcInfo;

    for ( sProc = aProcList;
          sProc != NULL;
          sProc = sProc->next )
    {
        // proc�� �̹� latch�� ȹ��Ǿ� �ִ�.
        IDE_TEST( qsxProc::getProcInfo( sProc->procID,
                                        &sProcInfo )
                  != IDE_SUCCESS);

        IDE_TEST( qdpRole::checkDMLExecutePSMPriv(
                      aStatement,
                      sProcInfo->planTree->userID,
                      sProcInfo->privilegeCount,
                      sProcInfo->granteeID,
                      ID_TRUE,
                      aGetSmiStmt4PrepareCallback,
                      aGetSmiStmt4PrepareContext )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::checkPlanPrivPkg(
    qcStatement                        * aStatement,
    qcgEnvPkgList                      * aPkgList,
    qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
    void                               * aGetSmiStmt4PrepareContext )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *               BUG-37251   
 *
 * Implementation :
 *      environment�� ��ϵ� Package ��ü�� ���� ������ �˻��Ѵ�.
 *
 ***********************************************************************/

    qcgEnvPkgList * sPkg;
    qsxPkgInfo    * sPkgInfo;

    for ( sPkg = aPkgList;
          sPkg != NULL;
          sPkg = sPkg->next )
    {
        IDE_TEST( qsxPkg::getPkgInfo( sPkg->pkgID,
                                      &sPkgInfo )
                  != IDE_SUCCESS);

        if( sPkgInfo->objType == QS_PKG )
        {
            IDE_TEST( qdpRole::checkDMLExecutePSMPriv(
                          aStatement,
                          sPkgInfo->planTree->userID,
                          sPkgInfo->privilegeCount,
                          sPkgInfo->granteeID,
                          ID_TRUE,
                          aGetSmiStmt4PrepareCallback,
                          aGetSmiStmt4PrepareContext )
                      != IDE_SUCCESS );
        }
        else
        {
            // Nothing to do.
            // privilege check�� spec���� �̹� �ߴ�.
            // ���� spec���� privilege chekc�� ���� ������ 
            // package�� ������� �ʴ´�.
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::setSmiStmtCallback(
    qcStatement                        * aStatement,
    qciGetSmiStatement4PrepareCallback   aGetSmiStmt4PrepareCallback,
    void                               * aGetSmiStmt4PrepareContext )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation : soft prepare ���� ���� tip
 *
 *     environment�� ��ϵ� ��ü�� ���� ������ �˻��Ҷ� �ʿ���
 *     smiStatement�� begin�Ѵ�. ���� ȣ��ÿ��� begin �ϹǷ�
 *     ������ ȣ���ϴ��� �������.
 *
 *     table, sequence, PSM ��ü���� �����ϴ� plan�� ��� system
 *     privilege�� ������� �ʴ��� Ư���� meta table�� �˻��� �ʿ䰡
 *     �����Ƿ� meta table�� �˻��� ���� smiStatement�� begin�Ͽ�
 *     soft prepare�� �˻� ������ ���δ�.
 *
 ***********************************************************************/
    
    smiStatement  * sSmiStmt;

    // ���� ȣ��ÿ��� soft prepare�� smiStmt�� �����ϸ�
    // ���Ŀ��� �׳� �����´�.
    IDE_TEST( aGetSmiStmt4PrepareCallback( aGetSmiStmt4PrepareContext,
                                           & sSmiStmt )
              != IDE_SUCCESS );

    qcg::setSmiStmt( aStatement, sSmiStmt );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcgPlan::initPrepTemplate4Reuse( qcTemplate * aTemplate )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation :
 *     prepared private template�� �����ϱ� ���� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

    aTemplate->tmplate.stackBuffer = NULL;
    aTemplate->tmplate.stack       = NULL;
    aTemplate->tmplate.stackCount  = 0;
    aTemplate->tmplate.stackRemain = 0;

    aTemplate->tmplate.dateFormat     = NULL;

    /* PROJ-2208 Multi Currency */
    aTemplate->tmplate.nlsCurrency    = NULL;

    aTemplate->tmplate.data     = NULL;
    aTemplate->tmplate.execInfo = NULL;

    aTemplate->planFlag     = NULL;
    aTemplate->cursorMgr    = NULL;
    aTemplate->tempTableMgr = NULL;

    aTemplate->stmt = NULL;

    aTemplate->numRows = 0;
    aTemplate->stmtType = QCI_STMT_MASK_MAX;
}

IDE_RC qcgPlan::allocUnusedTupleRow( qcSharedPlan * aSharedPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation : shared plan ũ�� ���̱� tip
 *
 *     ���� check-in�� �����ϴ� ��� ���� template�� �ٷ� ����ϱ�
 *     ���� ������ tuple�� row�� ������Ѵ�.
 *
 ***********************************************************************/

    mtcTemplate  * sTemplate;
    ULong          sFlag;
    UShort         i;

    sTemplate = & aSharedPlan->sTmplate->tmplate;

    for ( i = 0; i < sTemplate->rowCountBeforeBinding; i++ )
    {
        sFlag = sTemplate->rows[i].lflag;

        if( ( sFlag & MTC_TUPLE_TYPE_MASK ) != MTC_TUPLE_TYPE_VARIABLE )
        {
            if ( ( (sFlag & MTC_TUPLE_ROW_SET_MASK)      == MTC_TUPLE_ROW_SET_FALSE ) &&
                 ( (sFlag & MTC_TUPLE_ROW_ALLOCATE_MASK) == MTC_TUPLE_ROW_ALLOCATE_TRUE ) &&
                 ( (sFlag & MTC_TUPLE_ROW_COPY_MASK)     == MTC_TUPLE_ROW_COPY_FALSE ) )
            {
                // ����� �̷� tuple�� intermediate/table tuple�̴�.
                if ( ( sTemplate->rows[i].rowMaximum > 0 ) &&
                     ( sTemplate->rows[i].row == NULL ) )
                {
                    // BUG-15548
                    IDU_FIT_POINT( "qcgPlan::allocUnusedTupleRow::cralloc::sTemplate",
                                    idERR_ABORT_InsufficientMemory );

                    IDE_TEST( aSharedPlan->qmpMem->cralloc(
                            ID_SIZEOF(SChar) * sTemplate->rows[i].rowMaximum,
                            (void**) &(sTemplate->rows[i].row) )
                        != IDE_SUCCESS);
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
            // PROJ-2163 
            // variable tuple �� ����� ����� �ƴ�
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qcgPlan::freeUnusedTupleRow( qcSharedPlan * aSharedPlan )
{
/***********************************************************************
 *
 * Description : PROJ-1436 SQL Plan Cache
 *
 * Implementation : shared plan ũ�� ���̱� tip
 *
 *     ���� check-in�� �����Ѵٸ� intermediate/table tuple��
 *     row�� ���� alloc�� �ϱ� ������ free�� �����ϴ�.
 *     �׷��� ���� check-in�� �����ϴ� ��� ���� template�� �ٷ�
 *     ����ϱ� ������ �̶� ������ tuple�� row�� ������ؾ� �Ѵ�.
 *
 ***********************************************************************/

    mtcTemplate * sTemplate;
    UInt          sFlag;
    UShort        i;

    sTemplate = & aSharedPlan->sTmplate->tmplate;

    for( i = 0; i < sTemplate->rowCountBeforeBinding; i++ )
    {
        sFlag = sTemplate->rows[i].lflag;

        if( ( sFlag & MTC_TUPLE_TYPE_MASK ) != MTC_TUPLE_TYPE_VARIABLE )
        {
            if( ( ( sFlag & MTC_TUPLE_ROW_SET_MASK )      == MTC_TUPLE_ROW_SET_FALSE ) &&
                ( ( sFlag & MTC_TUPLE_ROW_ALLOCATE_MASK ) == MTC_TUPLE_ROW_ALLOCATE_TRUE ) &&
                ( ( sFlag & MTC_TUPLE_ROW_COPY_MASK )     == MTC_TUPLE_ROW_COPY_FALSE ) )
            {
                // ����� �̷� tuple�� intermediate/table tuple�̴�.

                if( ( sTemplate->rows[i].rowMaximum > 0 ) &&
                    ( sTemplate->rows[i].row != NULL ) )
                {
                    IDE_TEST( aSharedPlan->qmpMem->free( (void*)sTemplate->rows[i].row )
                              != IDE_SUCCESS );

                    sTemplate->rows[i].row = NULL;
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
            // PROJ-2163
            // variable tuple �� ���� ����� �ƴ�
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void qcgPlan::registerPlanBindInfo( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2163 Plan cache �� �Է��� ���ε� ���� ���
 *
 * Implementation :
 *     Plan cache �� key ������ ���� environment �� ���ε� ������
 *     myPlan �� plan envirionment �� ����Ѵ�.
 *     �� ���� plan ��� �� qciSQLPlanCacheContext �� ��Ƽ�
 *     MM �� �ѱ��.
 *
 ***********************************************************************/

    qcPlanBindParam   * sPlanBindParam;
    qciBindParamInfo  * sBindInfo;
    UInt                i = 0;

    if( (aStatement->sharedFlag         == ID_FALSE) &&
        (QC_PRIVATE_TMPLATE(aStatement) == NULL) &&
        (aStatement->myPlan->planEnv    != NULL) )
    {
        sPlanBindParam = aStatement->myPlan->planEnv->planBindInfo.bindParam;
        sBindInfo      = aStatement->pBindParam;

        // ���ε� ���� ����
        for( i = 0; i < aStatement->pBindParamCount; i++ )
        {
            sPlanBindParam[i].id        = sBindInfo[i].param.id;
            sPlanBindParam[i].type      = sBindInfo[i].param.type;
            sPlanBindParam[i].precision = sBindInfo[i].param.precision;
            sPlanBindParam[i].scale     = sBindInfo[i].param.scale;
        }
    }
    else
    {
        // Non cacheable statement
        // Nothing to do.
    }
}

IDE_RC qcgPlan::isMatchedPlanBindInfo( qcStatement    * aStatement,
                                       qcPlanBindInfo * aBindInfo,
                                       idBool         * aIsMatched )
{
/***********************************************************************
 *
 * Description : PROJ-2163 Plan cache �� ���ε� ���� �߰�
 *
 * Implementation :
 *     plan cache�� cache�� environment �� ���ε� ������
 *     soft prepare���� qcStatement�� ���� ���ε� ������ ���Ͽ�
 *     �ùٸ� plan���� �˻��Ѵ�.
 *     ��, pBindParam �� NULL �̸� ȣ��Ʈ ������ �˻����� �ʴ´�.
 *
 ***********************************************************************/

    qcPlanBindParam   * sPlanBindParam;
    qciBindParamInfo  * sBindInfo;
    idBool              sIsMatched = ID_TRUE;
    UShort              i;

    sPlanBindParam = aBindInfo->bindParam;
    sBindInfo      = aStatement->pBindParam;

    if( sBindInfo == NULL )
    {
        // pBindParam �� NULL �̸� ����ڰ� ȣ��Ʈ ������ ���ε� �ϱ� ���̴�.
        // �� ��� ���ε� ������ ������ �ʰ� ������ �÷��̶�� �Ǵ��Ѵ�.
        sIsMatched = ID_TRUE;
    }
    else
    {
        if( ( aBindInfo->bindParamCount != 0 ) && 
            ( aBindInfo->bindParam == NULL ) )
        {
            // child PCO is already finalized or garbage collected.
            sIsMatched = ID_FALSE;
        }
        else
        {
            // ���ε� ������ ��ġ�ϴ��� �˻�
            for( i = 0; i < aBindInfo->bindParamCount; i++ )
            {
                // memcmp Ȥ�� bitmap ������ ���Ѵ�.
                if( ( sPlanBindParam[i].id        != sBindInfo[i].param.id        ) ||
                    ( sPlanBindParam[i].type      != sBindInfo[i].param.type      ) ||
                    ( sPlanBindParam[i].precision != sBindInfo[i].param.precision ) ||
                    ( sPlanBindParam[i].scale     != sBindInfo[i].param.scale     ) )
                {
                    sIsMatched = ID_FALSE;
                    break;
                }
            }
        }
    }

    // �˻� ����� ��ȯ
    *aIsMatched = sIsMatched;

    return IDE_SUCCESS;
}

