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
 * $Id: qdc.cpp 90785 2021-05-06 07:26:22Z hykim $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <smi.h>
#include <qcuProperty.h>
#include <qcmDatabase.h>
#include <qcmTableSpace.h>
#include <qdc.h>
#include <qcg.h>
#include <qcgPlan.h>
#include <qcm.h>
#include <mtdTypes.h>
#include <mtl.h>
#include <mtuProperty.h>
#include <mtlTerritory.h>
#include <qdpPrivilege.h>
#include <qcmUser.h>
#include <qcuSqlSourceInfo.h>
#include <qsv.h>
#include <qcs.h>
#include <qdcAudit.h>
#include <qci.h>

#include <smiMisc.h>

#include <iduMemPoolMgr.h>
#include <qdpRole.h>
#include <sdi.h>
#include <sdiZookeeper.h>

//------------------------------------------------------------------------
// PROJ-2242 OPTIMIZER_FEATURE_ENABLE :
//     Plan �� ������ ��ġ�� property �� �ϰ� �����ϴµ� ���Ǹ�
//     �Ʒ��� ���� optimizer property ��������� �߻��� ���
//     - Plan �� ������ ��ġ�� ���ο� property �߰�
//     - Plan �� ������ ��ġ�� ���� property �� default value ����
//     ���� ������ �����Ѵ�.
//
//     1. QDC_OPTIMIZER_FEATURE_CNT ����
//     2. �Ʒ� �׸��� ������ QCU_OPTIMIZER_FEATURE_VERSION_MAX ��
//        ����� property �� ���� ���� �ݿ��� �ʱ� version ���� ����
//      - QDC_OPTIMIZER_FEATURE_VERSION_NONE �� �����ϴ� enum
//      - gFeatureProperty (property history)
//      - qdc::getFeatureVersionNo
//     3. 2���� ������ �κ��� �߰��Ϸ��� property �� �ݿ�
//        �߰��� QCU_OPTIMIZER_FEATURE_VERSION_MAX �� �̿�
//     4. Manual �� OPTIMIZER_FEATURE_ENABLE ������ �߰���û
//
//  comment) system property �� �������� �����ϴ� ������ ���� �����
//           value �� �ϳ��� ���� property �������� �ܼ��ϰ� ������.
//------------------------------------------------------------------------

#define QDC_OPTIMIZER_FEATURE_CNT   (34)

static qdcFeatureProperty gFeatureProperty[QDC_OPTIMIZER_FEATURE_CNT] =
{
    /* tag �� ���� �� �߰��Ǵ� ����
     * QDC_OPTIMIZER_FEATURE_VERSION_MAX �� �̿��Ѵ�.
     * Example)
     *   { (SChar *)"__OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION",
     *     (SChar *)"0",
     *     (SChar *)"1",
     *     QDC_OPTIMIZER_FEATURE_VERSION_MAX,  <--
     *     PLAN_PROPERTY_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION }
     */
    { (SChar *)"OPTIMIZER_ANSI_JOIN_ORDERING",                  // 1
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_0_6,
      PLAN_PROPERTY_OPTIMIZER_ANSI_JOIN_ORDERING },
    { (SChar *)"OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD",            // 2
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_0_7,
      PLAN_PROPERTY_OPTIMIZER_SUBQUERY_OPTIMIZE_METHOD },
    { (SChar *)"__OPTIMIZER_DNF_DISABLE",                       // 3
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_4_3,
      PLAN_PROPERTY_OPTIMIZER_DNF_DISABLE },
    { (SChar *)"QUERY_REWRITE_ENABLE",                          // 4
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_QUERY_REWRITE_ENABLE },
    { (SChar *)"OPTIMIZER_PARTIAL_NORMALIZE",                   // 5
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_PARTIAL_NORMALIZE },
    { (SChar *)"OPTIMIZER_UNNEST_SUBQUERY",                     // 6
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_UNNEST_SUBQUERY },
    { (SChar *)"OPTIMIZER_UNNEST_COMPLEX_SUBQUERY",             // 7
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPLEX_SUBQUERY },
    { (SChar *)"OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY",         // 8
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY },
    { (SChar *)"__OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION",    // 9
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_ELIMINATE_COMMON_SUBEXPRESSION },
    { (SChar *)"__OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION",       // 10
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_CONSTANT_FILTER_SUBSUMPTION },
    { (SChar *)"__OPTIMIZER_FIXED_GROUP_MEMORY_TEMP",           // 11
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_FIXED_GROUP_MEMORY_TEMP },
    { (SChar *)"__OPTIMIZER_OUTERJOIN_ELIMINATION",             // 12
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_OUTERJOIN_ELIMINATION },
    { (SChar *)"__OPTIMIZER_ANSI_INNER_JOIN_CONVERT",           // 13
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_ANSI_INNER_JOIN_CONVERT },
    { (SChar *)"OPTIMIZER_COUNT_COLUMN_TO_COUNT_ASTAR",         // 14
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_COUNT_COLUMN_TO_COUNT_ASTAR },
    { (SChar *)"OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY",         // 15
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_UNNEST_AGGREGATION_SUBQUERY },
    { (SChar *)"__OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE",       // 16
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_ORDER_BY_ELIMINATION_ENABLE },
    { (SChar *)"__OPTIMIZER_DISTINCT_ELIMINATION_ENABLE",       // 17
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0,
      PLAN_PROPERTY_OPTIMIZER_DISTINCT_ELIMINATION_ENABLE },
    { (SChar *)"__OPTIMIZER_VIEW_TARGET_ENABLE",                // 18
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_VIEW_TARGET_ENABLE },
    { (SChar *)"RESULT_CACHE_ENABLE",                           // 19
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_RESULT_CACHE_ENABLE },
    { (SChar *)"TOP_RESULT_CACHE_MODE",                         // 20
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_TOP_RESULT_CACHE_MODE },
    { (SChar *)"__OPTIMIZER_LIST_TRANSFORMATION",               // 21
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_LIST_TRANSFORMATION },
    { (SChar *)"OPTIMIZER_AUTO_STATS",                          // 22
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_AUTO_STATS },
    { (SChar *)"OPTIMIZER_PERFORMANCE_VIEW",                    // 23
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_PERFORMANCE_VIEW },
    { (SChar *)"__OPTIMIZER_INNER_JOIN_PUSH_DOWN",              // 24
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_INNER_JOIN_PUSH_DOWN },
    { (SChar *)"__OPTIMIZER_ORDER_PUSH_DOWN",                   // 25
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_ORDER_PUSH_DOWN },
    { (SChar *)"__OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE",    // 26
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_TARGET_SUBQUERY_UNNEST_DISABLE },
    { (SChar *)"__OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE",   // 27
      (SChar *)"0",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_TARGET_SUBQUERY_REMOVAL_DISABLE },
    { (SChar *)"HOST_OPTIMIZE_ENABLE",                          // 28
      (SChar *)"1",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_HOST_OPTIMIZE_ENABLE },
    { (SChar *)"__OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE",  // 29 
      (SChar *)"1",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_LIKE_INDEX_SCAN_WITH_OUTER_COLUMN_DISABLE },
    { (SChar *)"__OPTIMIZER_HIERARCHY_TRANSFORMATION",  // 30
      (SChar *)"1",
      (SChar *)"0",
      QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0,
      PLAN_PROPERTY_OPTIMIZER_HIERARCHY_TRANSFORMATION },
    { (SChar *)"__OPTIMIZER_UNNEST_COMPATIBILITY",  // 31
      (SChar *)"3",
      (SChar *)"2",
      QDC_OPTIMIZER_FEATURE_VERSION_MAX,
      PLAN_PROPERTY_OPTIMIZER_UNNEST_COMPATIBILITY },
    { (SChar *)"__OPTIMIZER_INVERSE_JOIN_ENABLE",               // 32
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1,
      PLAN_PROPERTY_OPTIMIZER_INVERSE_JOIN_ENABLE },
    { (SChar *)"__OPTIMIZER_INDEX_COST_MODE",               // 33
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_MAX,
      PLAN_PROPERTY_OPTIMIZER_INDEX_COST_MODE},
    { (SChar *)"__LEFT_OUTER_SKIP_RIGHT_ENABLE",               // 34
      (SChar *)"0",
      (SChar *)"1",
      QDC_OPTIMIZER_FEATURE_VERSION_MAX,
      PLAN_PROPERTY_LEFT_OUTER_SKIP_RIGHT_ENABLE }
};


/***********************************************************************
 * VALIDATE
 **********************************************************************/
// ALTER SYSTEM
IDE_RC qdc::validate(qcStatement * /* aStatement */ )
{
#define IDE_FN "qdc::validate"

    // nothing to do for BUG-6950

    return IDE_SUCCESS;

#undef IDE_FN
}

/***********************************************************************
 * EXECUTE
 **********************************************************************/
// ALTER SYSTEM
IDE_RC qdc::execute(qcStatement * /* aStatement */ )
{
#define IDE_FN "qdc::execute"

    return IDE_SUCCESS;

#undef IDE_FN
}

// ALTER SYSTEM CHECKPOINT
IDE_RC qdc::checkpoint( qcStatement * aStatement )
{
#define IDE_FN "qdc::checkpoint"

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    // execution
    IDE_TEST( smiCheckPoint(aStatement->mStatistics,
                            ID_FALSE) /* Turn Off�� Flusher����
                                       *  ������ �ʴ´�
                                       */
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/* 
 * PROJ-2065 �Ѱ��Ȳ�׽�Ʈ garbage collection
 *
 * ALTER SYSTEM shrink_mempool
 */
IDE_RC qdc::shrinkMemPool( qcStatement * aStatement )
{
#define IDE_FN "qdc::shrinkMemPool"

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    // execution
    IDE_TEST( iduMemPoolMgr::shrinkMemPools() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}




//BUG-45182
//ALTER SYSTEM dump_callstack;
IDE_RC qdc::dumpAllCallstacks( qcStatement * aStatement )
{
#define IDE_FN "qdc::dumpAllCallstacks"

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    // execution
    IDE_TEST( iduStack::dumpAllCallstack() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// ALTER SYSTEM REORGANIZE
IDE_RC qdc::reorganize(qcStatement * aStatement)
{
#define IDE_FN "qdc::reorganize"

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// ALTER SYSTEM VERIFY
IDE_RC qdc::verify(qcStatement * aStatement)
{
#define IDE_FN "qdc::verify"

    UInt sFlag = 0;

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    sFlag = (SMI_VERIFY_TBS);

    IDE_TEST( smiVerifySM( aStatement->mStatistics,
                           sFlag ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// ALTER SYSTEM SET name = value;
IDE_RC
qdc::setSystemProperty(qcStatement * aStatement, idpArgument *aArg)
{
#define IDE_FN "qdc::setSystemProperty"

    qdSystemSetParseTree   * sParseTree;
    SChar                    sName[IDP_MAX_VALUE_LEN + 1];
    SChar                    sValue[IDP_MAX_VALUE_LEN + 1];
    SChar                    sMsg[(IDP_MAX_VALUE_LEN + 1) * 2 + 24];
    SInt                     sTemp = -1;
    idBool                   sIsFeatureEnable = ID_FALSE;
    UInt                     sPropertyAttribute = 0;
    UInt                     sOldValue ;
    UInt                     sNewValue ;
    
    sParseTree = (qdSystemSetParseTree *)(aStatement->myPlan->parseTree);

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Property Name �� Value String ����
    //-----------------------------------------------------

    IDE_TEST_RAISE( sParseTree->name.size > IDP_MAX_VALUE_LEN,
                    ERR_TOO_LONG_PROPERTY );
    IDE_TEST_RAISE( sParseTree->value.size > IDP_MAX_VALUE_LEN,
                    ERR_TOO_LONG_PROPERTY );
    
    idlOS::memcpy( sName,
                   sParseTree->name.stmtText + sParseTree->name.offset,
                   sParseTree->name.size );
    sName[sParseTree->name.size] = '\0';

    idlOS::memcpy( sValue,
                   sParseTree->value.stmtText + sParseTree->value.offset,
                   sParseTree->value.size );
    sValue[sParseTree->value.size] = '\0';

    if ( idlOS::strMatch( sName,
                          idlOS::strlen( sName ),
                          "NLS_CURRENCY",
                          12 ) == 0 )
    {
        if ( sParseTree->value.size > MTL_TERRITORY_CURRENCY_LEN )
        {
            sValue[MTL_TERRITORY_CURRENCY_LEN] = '\0';
        }
        else
        {
            /* Nothing to do */
        }
    }
    else if ( idlOS::strMatch( sName,
                               idlOS::strlen( sName ),
                               "NLS_NUMERIC_CHARACTERS",
                               22 ) == 0 )
    {
        if ( sParseTree->value.size > MTL_TERRITORY_NUMERIC_CHAR_LEN )
        {
            sValue[MTL_TERRITORY_NUMERIC_CHAR_LEN] = '\0';
        }
        else
        {
            /* Nothing to do */
        }
    }
    else if ( idlOS::strMatch( sName,
                               idlOS::strlen( sName ),
                               "OPTIMIZER_FEATURE_ENABLE",
                               24 ) == 0 )
    {
        /* PROJ-2242
         * OPTIMIZER_FEATURE_ENABLE �����
         * gFeatureProperty �� ���ǵ� property ���� �ϰ� ����
         */
        
        IDE_TEST( changeFeatureProperty( aStatement,
                                         sValue,
                                         (void *)aArg )
                  != IDE_SUCCESS );

        sIsFeatureEnable = ID_TRUE;
    }
    else if ( idlOS::strMatch( sName,
                               idlOS::strlen( sName ),
                               "AUTO_COMMIT",
                               11 ) == 0 )
    {
        /* BUG-48247 
         * SHARD ȯ�濡�� auto commit�� ������ �ʿ䰡 �ֽ��ϴ�.  
         * SDU_SHARD_ALLOW_AUTO_COMMIT�� 0���� �����Ǿ� �ִ� ��� AUTO_COMMIT=1�� ���� �Ұ� 
         */
        if ( (SDU_SHARD_ENABLE == 1) && (SDU_SHARD_ALLOW_AUTO_COMMIT == 0) )
        {
            IDE_TEST( idp::read( sName, &sOldValue ) != IDE_SUCCESS );

            sNewValue = idlOS::strToUInt( (UChar *)sValue, idlOS::strlen( sValue ) );

            /* AUTO_COMMIT�� 1�� �����Ǿ� �ִ� ��� ERROR �߻� */ 
            IDE_TEST_RAISE( sOldValue == 1 , ERR_INVALID_CONDITION ); 

            /* AUTO_COMMIT�� 1�� �����Ϸ��� ��� ERROR �߻� */ 
            IDE_TEST_RAISE( sNewValue == 1 , ERR_CANNOT_SET_AUTOCOMMIT ); 

        } 
    }
    else if ( idlOS::strMatch( sName,
                               idlOS::strlen( sName ),
                               "GLOBAL_TRANSACTION_LEVEL",
                               24 ) == 0 )
    {
        /* BUG-48352 */
        if ( SDU_SHARD_ENABLE == 0 )
        {
            sNewValue = idlOS::strToUInt( (UChar *)sValue, idlOS::strlen( sValue ) );
            
            IDE_TEST_RAISE( sNewValue == 3, ERR_CANNOT_SET_GLOBALTRANSACTIONLEVEL );
        }
    }
    else
    {
        /* Nothing to do */
    }

    //-----------------------------------------------------
    // System Property ����
    //-----------------------------------------------------

    // BUG-19498 value range check
    IDE_TEST( idp::validate( sName, sValue, ID_TRUE )
              != IDE_SUCCESS );    
    
    // PROJ-2727
    IDE_TEST( idp::getPropertyAttribute( sName, &sPropertyAttribute )
              != IDE_SUCCESS );

    QCG_SET_SESSION_PROPERTY_ATTRIBUTE( aStatement, sPropertyAttribute );

    IDE_TEST( idp::update( aStatement->mStatistics, sName, sValue, 0, (void *)aArg )
              != IDE_SUCCESS );

    //-----------------------------------------------------
    // PROJ-2242
    // OPTIMIZER_FEATURE_ENABLE �� ��� environment ���
    //-----------------------------------------------------

    if( sIsFeatureEnable == ID_TRUE )
    {
        qcgPlan::registerPlanProperty(
                aStatement,
                PLAN_PROPERTY_OPTIMIZER_FEATURE_ENABLE );
    }
    else
    {
        /* Nothing to do */
    }
    
    //-----------------------------------------------------
    // ����� System Propery ������ Boot Log�� ���
    //-----------------------------------------------------

    idlOS::snprintf( sMsg, ID_SIZEOF(sMsg),
                     "[SET-PROP] %s=[%s]\n",
                     sName,
                     sValue );

    ideLog::log(IDE_QP_0, "%s", sMsg);

    /* PROJ-2208
     * NLS_TERRITORY�� ������ NLS_ISO_CURRENCY, NLS_CURRENCY,
     * NLS_NUMERIC_CHARACTERS�� ���� ����ȴ�.
     */
    if ( idlOS::strMatch( sName,
                          idlOS::strlen( sName ),
                          "NLS_TERRITORY",
                          13 ) == 0 )
    {
        IDE_TEST( mtlTerritory::searchNlsTerritory( sValue, &sTemp )
                  != IDE_SUCCESS );
        IDE_TEST( mtlTerritory::setNlsISOCurrencyName( sTemp, sValue )
                  != IDE_SUCCESS );
        IDE_TEST( idp::update( aStatement->mStatistics, "NLS_ISO_CURRENCY", sValue, 0, (void *)aArg )
                  != IDE_SUCCESS );

        idlOS::snprintf( sMsg, ID_SIZEOF(sMsg),
                         "[SET-PROP] %s=[%s]\n",
                         "NLS_ISO_CURRENCY",
                         sValue );
        ideLog::log(IDE_QP_0, "%s", sMsg);

        IDE_TEST( mtlTerritory::setNlsCurrency( sTemp, sValue ) != IDE_SUCCESS );
        IDE_TEST( idp::update( aStatement->mStatistics, "NLS_CURRENCY", sValue, 0, (void *)aArg )
                               != IDE_SUCCESS );

        idlOS::snprintf( sMsg, ID_SIZEOF(sMsg),
                         "[SET-PROP] %s=[%s]\n",
                         "NLS_CURRENCY",
                         sValue );
        ideLog::log(IDE_QP_0, "%s", sMsg);

        IDE_TEST( mtlTerritory::setNlsNumericChar( sTemp, sValue ) != IDE_SUCCESS );
        IDE_TEST( idp::update( aStatement->mStatistics, "NLS_NUMERIC_CHARACTERS", sValue, 0, (void *)aArg )
                                != IDE_SUCCESS );

        idlOS::snprintf( sMsg, ID_SIZEOF(sMsg),
                         "[SET-PROP] %s=[%s]\n",
                         "NLS_NUMERIC_CHARACTERS",
                         sValue );
        ideLog::log(IDE_QP_0, "%s", sMsg);
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_TOO_LONG_PROPERTY);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_TOO_LONG_PROPERTY) );
    }
    IDE_EXCEPTION(ERR_INVALID_CONDITION);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR, 
                                 "qdc::setSystemProperty", 
                                 "Invalid condition.") );
    }
    IDE_EXCEPTION(ERR_CANNOT_SET_AUTOCOMMIT);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_CANNOT_CHANGE_AUTOCOMMIT_IN_SHARD_ENV) );
    }
    IDE_EXCEPTION( ERR_CANNOT_SET_GLOBALTRANSACTIONLEVEL )
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_GCTX_NOT_ALLOW) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}


IDE_RC
qdc::checkExecDDLdisableProperty( void )
{
#define IDE_FN ""
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

    IDE_TEST_RAISE ( QCU_EXEC_DDL_DISABLE != 0, ERR_EXEC_DDL);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EXEC_DDL);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QCC_CANNOT_EXEC_DDL) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdc::validateCreateDatabase(qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *      CREATE DATABASE ... �� validation ����
 *
 * Implementation :
 *      1. ���� �˻�
 *      2. �����ͺ��̽� �̸� ����
 *
 ***********************************************************************/

#define IDE_FN "qdc::validateCreateDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::validateCreateDatabase"));

    // check grant
    /* BUGBUG... open meta ���̱� ������ check �Ұ���
       IDE_TEST( qdpPrivilege::checkDDLCreateDatabasePriv(
       aStatement,
       QC_GET_SESSION_USER_ID(aStatement))
       != IDE_SUCCESS );
     */

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdc::validateAlterDatabase(qcStatement * aStatement)
{// BUGBUG... DATABASE_DDL �� ��� validate �Լ� ȣ����� ����
/***********************************************************************
 *
 * Description :
 *      ALTER DATABASE ... �� validation ����
 *
 * Implementation :
 *      1. ���� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qdc::validateAlterDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::validateAlterDatabase"));

    // check grant
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );
    /* BUGBUG... open meta ���̱� ������ check �Ұ���
    IDE_TEST( qdpPrivilege::checkDDLAlterDatabasePriv(
                aStatement,
                QC_GET_SESSION_USER_ID(aStatement))
              != IDE_SUCCESS );
    */

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

/* BUG-39257 The statement 'ALTER DATABASE' has many parsetrees */
IDE_RC
qdc::validateAlterDatabaseOpt2(qcStatement * aStatement)
{// BUGBUG... DATABASE_DDL �� ��� validate �Լ� ȣ����� ����
/***********************************************************************
 *
 * Description :
 *      ALTER DATABASE ... �� validation ����
 *
 * Implementation :
 *      1. ���� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qdc::validateAlterDatabaseOpt2"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::validateAlterDatabaseOpt2"));

    qdDatabaseParseTree * sParseTree;

    sParseTree = (qdDatabaseParseTree *)(aStatement->myPlan->parseTree);

    switch ( sParseTree->flag )
    {
        case QCI_SESSION_CLOSE:
            /* BUG-38990 To close a session is permitted by the sys user */
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYS_USER_ID,
                            ERR_NO_GRANT_ALTER_DATABASE );
            break;

        default:
            // check grant
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                            ERR_NO_GRANT_ALTER_DATABASE );
            /* BUGBUG... open meta ���̱� ������ check �Ұ���
            IDE_TEST( qdpPrivilege::checkDDLAlterDatabasePriv(
                        aStatement,
                        QC_GET_SESSION_USER_ID(aStatement))
                    != IDE_SUCCESS );
            */
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdc::validateDropDatabase(qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *      DROP DATABASE ... �� validation ����
 *
 * Implementation :
 *      1. ���� �˻�
 *
 ***********************************************************************/

#define IDE_FN "qdc::validateDropDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::validateDropDatabase"));

    // check grant
    /* BUGBUG... open meta ���̱� ������ check �Ұ���
       IDE_TEST( qdpPrivilege::checkDDLDropDatabasePriv(
       aStatement,
       QC_GET_SESSION_USER_ID(aStatement))
       != IDE_SUCCESS );
    */

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdc::executeCreateDatabase(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      CREATE DATABASE ... �� execution ����
 *
 *      1. �����ͺ��̽� ĳ���� �� üũ
 *      2. ���ų� ĳ���� �� üũ
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::executeCreateDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::executeCreateDatabase"));

    qdDatabaseParseTree * sParseTree;
    qciArgCreateDB        sArgCreateDB;
    const UInt            sUnit = 1024;
    SChar                 sDBCharSet[MTL_NAME_LEN_MAX+1];
    SChar                 sNationalCharSet[MTL_NAME_LEN_MAX+1];
    const mtlModule     * sDBCharSetModule;
    const mtlModule     * sNCharSetModule;
    qcuSqlSourceInfo      sqlInfo;

    sParseTree = (qdDatabaseParseTree *) aStatement->myPlan->parseTree;

    // To fix BUG-24023
    // create db������ ��츦 ����Ͽ� ���
    sDBCharSetModule = mtl::mDBCharSet;
    sNCharSetModule = mtl::mNationalCharSet;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    if( SM_PAGE_SIZE >= sUnit )
    {
        // ������ �����ͺ��̽� Page��
        sArgCreateDB.mUserCreatePageCount = sParseTree->intValue1 / (SM_PAGE_SIZE / sUnit);
    }
    else
    {
        // ������ �����ͺ��̽� Page��
        sArgCreateDB.mUserCreatePageCount = sParseTree->intValue1 * (sUnit / SM_PAGE_SIZE);
    }

    if ( sParseTree->dbCharSet.size > MTL_NAME_LEN_MAX )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->dbCharSet );
        IDE_RAISE( ERR_MAX_NAME_LEN_OVERFLOW );
    }
    else
    {
        // Nothing to do.
    }
                    
    // PROJ-1579 NCHAR
    // DB CHARACTER SET CHECK
    // �����ϴ� ĳ���� �� �� UTF16�� �����ϰ� ��� ����� �� �ִ�.
    idlOS::memcpy(sDBCharSet,
                  aStatement->myPlan->stmtText +
                  sParseTree->dbCharSet.offset,
                  sParseTree->dbCharSet.size );

    sDBCharSet[sParseTree->dbCharSet.size] = '\0';
    
    IDE_TEST( mtl::moduleByName( (const mtlModule **) & mtl::mDBCharSet,
                                 sDBCharSet,
                                 sParseTree->dbCharSet.size )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( mtl::mDBCharSet->id == MTL_UTF16_ID,
                    ERR_INVALID_DB_CHAR_SET );
    
    if ( sParseTree->nationalCharSet.size > MTL_NAME_LEN_MAX )
    {
        sqlInfo.setSourceInfo( aStatement, & sParseTree->nationalCharSet );
        IDE_RAISE( ERR_MAX_NAME_LEN_OVERFLOW );
    }
    else
    {
        // Nothing to do.
    }
                    
    // PROJ-1579 NCHAR
    // NATIONAL CHARACTER SET CHECK
    // UTF8, UTF16�� ����� �� �ִ�.

    idlOS::memcpy(sNationalCharSet,
                  aStatement->myPlan->stmtText +
                  sParseTree->nationalCharSet.offset,
                  sParseTree->nationalCharSet.size );
    
    sNationalCharSet[sParseTree->nationalCharSet.size] = '\0';
   
    IDE_TEST( mtl::moduleByName( (const mtlModule **) & mtl::mNationalCharSet,
                                 sNationalCharSet,
                                 sParseTree->nationalCharSet.size )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( (mtl::mNationalCharSet->id != MTL_UTF8_ID) && 
                    (mtl::mNationalCharSet->id != MTL_UTF16_ID),
                    ERR_INVALID_NATIONAL_CHAR_SET );

    sArgCreateDB.mDBName = sParseTree->dbName.stmtText + sParseTree->dbName.offset;
    sArgCreateDB.mDBNameLen = sParseTree->dbName.size;
    /* PROJ-2160 CM Type remove */
    sArgCreateDB.mOwnerDN = NULL;
    sArgCreateDB.mOwnerDNLen = 0 ;

    // Archive Log
    sArgCreateDB.mArchiveLog = sParseTree->archiveLog;

    // DB Charset
    sArgCreateDB.mDBCharSet = (SChar*)(mtl::mDBCharSet->names->string);

    // National Charset
    sArgCreateDB.mNationalCharSet = (SChar*)(mtl::mNationalCharSet->names->string);

    /* shard database check */
    IDE_TEST( sdi::validateCreateDB(sArgCreateDB.mDBName,
                                    sArgCreateDB.mDBCharSet,
                                    sArgCreateDB.mNationalCharSet) != IDE_SUCCESS );

    IDE_TEST( qcg::mCreateCallback( aStatement->mStatistics, (void *)&sArgCreateDB )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION(ERR_INVALID_DB_CHAR_SET);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INVALID_DB_CHAR_SET));
    }

    IDE_EXCEPTION(ERR_INVALID_NATIONAL_CHAR_SET);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INVALID_NATIONAL_CHAR_SET));
    }
    IDE_EXCEPTION( ERR_MAX_NAME_LEN_OVERFLOW );
    {
        (void)sqlInfo.init(aStatement->qmeMem);
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QCP_MAX_NAME_LENGTH_OVERFLOW,
                                sqlInfo.getErrMessage()));
        (void)sqlInfo.fini();
    }
    IDE_EXCEPTION_END;

    // To fix BUG-24023
    // ������ ��� ����.
    mtl::mDBCharSet = sDBCharSetModule;
    mtl::mNationalCharSet = sNCharSetModule;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdc::executeUpgradeMeta( qcStatement * /* aStatement */ )
{
/***********************************************************************
 *
 * Description :
 *      ALTER DATABASE ... UPGRADE META �� execution ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::executeAlterDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::executeAlterDatabase"));

    // TODO ...

    return IDE_SUCCESS;

#undef IDE_FN
}

IDE_RC
qdc::executeAlterDatabase( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *      UPGRADE META �� ������ ALTER DATABASE ... �� execution ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::executeAlterDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::executeAlterDatabase"));

    qdDatabaseParseTree   * sParseTree;
    qciArgCloseSession      sArgCloseSession;
    qciArgStartup           sArgStartup;
    qciArgShutdown          sArgShutdown;
    qciArgCommitDTX         sArgCommitDTX;
    qciArgRollbackDTX       sArgRollbackDTX;

    sParseTree = (qdDatabaseParseTree *)(aStatement->myPlan->parseTree);

    switch ( sParseTree->flag )
    {
        case QCI_STARTUP_PROCESS:
        case QCI_STARTUP_CONTROL:
        case QCI_STARTUP_META:
        case QCI_STARTUP_SERVICE:
        case QCI_META_DOWNGRADE:
            // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
            // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                            ERR_NO_GRANT_ALTER_DATABASE );

            sArgStartup.mQcStatement = (qciStatement*)aStatement;
            sArgStartup.mPhase       = (qciStartupPhase)sParseTree->flag;
            sArgStartup.mStartupFlag = sParseTree->optionflag;
            IDE_TEST( qcg::mStartupCallback( aStatement->mStatistics, (void*)&sArgStartup )
                      != IDE_SUCCESS );
            break;

        case QCI_SESSION_CLOSE:
            // bug-19279 remote sysdba enable + sys can kill session
            // ������: sysdba�� �ƴϸ� alter database ���� ���� �Ұ�
            // ������: sysdba�� �ƴϴ���, sys �����̰�
            //         alter database name session close id �����̸� ���
            // ��, sysdba�� �ƴϴ��� sys �������� session kill�� �Ҽ� �ִ�.
            // ex) ���� ������ ���� ��Ȳ���� ������ �ȴ�
            // a�� �������� sysdba�� ������ ����� ���ϰ� �ִ�.
            // b�� ���ϰ� sysdba�� ������ �ʿ䰡 �ִµ� sysdba�� ���� 1 ���Ǹ�
            // �����ϹǷ� ������ ���ϰ� �ȴ�.
            // �̶�, b�� sys�� ������ a�� sysdba ������ ������ kill �Ѵ�.
            // ���� sysdba ������ �����Ƿ�, b�� �ٽ� sysdba�� ���Ӱ����ϴ�
            // target session�� sysdba session�� ��� logging

            // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
            // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYS_USER_ID,
                            ERR_NO_GRANT_ALTER_DATABASE );

            sArgCloseSession.mMmSession = aStatement->session->mMmSession;
            sArgCloseSession.mDBName = sParseTree->dbName.stmtText + sParseTree->dbName.offset;
            sArgCloseSession.mDBNameLen = sParseTree->dbName.size;

            switch (sParseTree->closeMethod)
            {
                case QDP_SESSION_CLOSE_ALL:
                    sArgCloseSession.mCloseAll = ID_TRUE;
                    sArgCloseSession.mSessionID = 0;
                    sArgCloseSession.mUserName = NULL;
                    break;
                case QDP_SESSION_CLOSE_USER:
                    sArgCloseSession.mCloseAll = ID_FALSE;
                    sArgCloseSession.mSessionID = 0;
                    sArgCloseSession.mUserName = sParseTree->userName.stmtText + sParseTree->userName.offset;
                    sArgCloseSession.mUserNameLen = sParseTree->userName.size;
                    break;
                case QDP_SESSION_CLOSE_ID:
                    sArgCloseSession.mCloseAll = ID_FALSE;
                    sArgCloseSession.mSessionID = sParseTree->intValue1;
                    sArgCloseSession.mUserName = NULL;
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }

            IDE_TEST( qcg::mCloseSessionCallback( aStatement->mStatistics,
                                                  (void*)&sArgCloseSession )
                      != IDE_SUCCESS );
            break;

        case QCI_DTX_COMMIT:
            // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
            // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                            ERR_NO_GRANT_ALTER_DATABASE );

            sArgCommitDTX.mSmTID = sParseTree->intValue1;
            IDE_TEST( qcg::mCommitDTXCallback( aStatement->mStatistics, (void*)&sArgCommitDTX )
                      != IDE_SUCCESS );
            break;

        case QCI_DTX_ROLLBACK:
            // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
            // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                            ERR_NO_GRANT_ALTER_DATABASE );

            sArgRollbackDTX.mSmTID = sParseTree->intValue1;
            IDE_TEST( qcg::mRollbackDTXCallback( aStatement->mStatistics, (void*)&sArgRollbackDTX )
                      != IDE_SUCCESS );
            break;

        case QCI_SHUTDOWN_NORMAL:
        case QCI_SHUTDOWN_IMMEDIATE:
        case QCI_SHUTDOWN_EXIT:
            // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
            // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
            IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                            ERR_NO_GRANT_ALTER_DATABASE );

            sArgShutdown.mQcStatement  = (qciStatement*)aStatement;
            sArgShutdown.mPhase        = (qciStartupPhase)sParseTree->flag;
            sArgShutdown.mShutdownFlag = sParseTree->optionflag;
            (void)sdiZookeeper::shutdown();
            IDE_TEST( qcg::mShutdownCallback( aStatement->mStatistics, (void *)&sArgShutdown )
                      != IDE_SUCCESS );
            break;

        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC
qdc::executeDropDatabase(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      DROP DATABASE ... �� execution ����
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::executeDropDatabase"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY("qdc::executeDropDatabase"));

    qdDatabaseParseTree *sParseTree;
    qciArgDropDB  sArgDropDB;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdDatabaseParseTree *) aStatement->myPlan->parseTree;

    sArgDropDB.mDBName = sParseTree->dbName.stmtText + sParseTree->dbName.offset;
    sArgDropDB.mDBNameLen  = sParseTree->dbName.size;

    /* shard database check */
    IDE_TEST( sdi::validateDropDB(sArgDropDB.mDBName) != IDE_SUCCESS );

    IDE_TEST( qcg::mDropCallback( aStatement->mStatistics, (void *)&sArgDropDB )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::checkPrivileges(qcStatement * aStatement)
{
/***********************************************************************
 *
 * Description :
 *      check privilege ALTER SYSTEM
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::checkPrivileges"

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    //PROJ-1677 DEQ
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // check grant
    IDE_TEST( qdpRole::checkDDLAlterSystemPriv( aStatement )
              != IDE_SUCCESS );
    
    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::archivelog( qcStatement * aStatement )
{

#define IDE_FN "qdc::archivelog"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qdSystemParseTree* sParseTree;
    idBool             sStart;

    sParseTree = (qdSystemParseTree *)(aStatement->myPlan->parseTree);
    sStart = sParseTree->startArchivelog;

    if ( sStart == ID_TRUE )
    {
        IDE_TEST( smiBackup::startArchThread() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smiBackup::stopArchThread() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::alterArchiveMode( qcStatement* aStatement )
{

#define IDE_FN "qdc::alterArchiveMode"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qdDatabaseParseTree* sParseTree;
    smiArchiveMode       sArchiveMode;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdDatabaseParseTree*)(aStatement->myPlan->parseTree);

    sArchiveMode = ( sParseTree->archiveLog == ID_TRUE ) ?
        SMI_LOG_ARCHIVE : SMI_LOG_NOARCHIVE;

    IDE_TEST( smiBackup::alterArchiveMode( sArchiveMode )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::executeFullBackup( qcStatement* aStatement )
{
    qdBackupParseTree*       sParseTree;
    qdFullBackupSpec*        sBackupSpec;

    smiTrans                 sSmiTrans;
    smiStatement*            sDummySmiStmt = NULL;
    smiStatement             sSmiStmt;
    smiStatement*            sSmiStmtOrg = NULL;
    SInt                     sState = 0;
    UInt                     sSmiStmtFlag = 0;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    // transaction initialize
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;
    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    sParseTree = (qdBackupParseTree*)(aStatement->myPlan->parseTree);
    sBackupSpec = &sParseTree->fullBackupSpec;

    switch( sBackupSpec->granularity )
    {
        case QD_BACKUP_LOGANCHOR:
            {
                IDE_TEST(
                    smiBackup::backupLogAnchor(
                        aStatement->mStatistics,
                        sBackupSpec->path )
                    != IDE_SUCCESS );
                break;
            }

        case QD_BACKUP_TABLESPACE:
            {
                smiTableSpaceAttr sTbsAttr;
                IDE_TEST( qcmTablespace::getTBSAttrByName(
                              aStatement,
                              sBackupSpec->srcName,
                              idlOS::strlen(sBackupSpec->srcName),
                              &sTbsAttr ) != IDE_SUCCESS );

                IDE_TEST(
                    smiBackup::backupTableSpace(
                        aStatement->mStatistics,
                        (QC_SMI_STMT( aStatement ))->getTrans(),
                        sTbsAttr.mID,
                        sBackupSpec->path )
                    != IDE_SUCCESS );
                break;
            }
        case QD_BACKUP_DATABASE:
            {
                IDE_TEST(
                    smiBackup::backupDatabase(
                        aStatement->mStatistics,
                        (QC_SMI_STMT( aStatement ))->getTrans(),
                        sBackupSpec->path )
                    != IDE_SUCCESS );
                break;
            }
        default:
            IDE_DASSERT(0);
            break;
    }

    //restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            qcg::setSmiStmt(aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeMediaRecovery( qcStatement* aStatement )
{

#define IDE_FN "qdc::executeMediaRecovery"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    qdMediaRecoveryParseTree* sParseTree;
    qdMediaRecoveryType       sRecoveryType;
    mtdDateType               sDate;
    struct tm                 sTM;
    UInt                      sUntilTime = UINT_MAX;
    smiRecoverType            sRecvType = SMI_RECOVER_COMPLETE;
    static const SChar      * sFormat = "YYYY-MM-DD:HH:MI:SS";
    SChar                   * sFromTag = NULL;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdMediaRecoveryParseTree*)(aStatement->myPlan->parseTree);
    sRecoveryType = sParseTree->recoveryType;

    switch( sRecoveryType )
    {
        case QD_MEDIA_RECOVERY_CREATE_DATAFILE:
            {
                // EMPTY ��ũ ����Ÿ������ �����Ѵ�.
                IDE_TEST(
                    smiMediaRecovery::createDatafile( sParseTree->oldName,
                                                      sParseTree->newName )
                    != IDE_SUCCESS );
                break;
            }
        case QD_MEDIA_RECOVERY_CREATE_CHECKPOINT_IMAGE:
            {
                // EMPTY �޸� ����Ÿ������ �����Ѵ�.
                IDE_TEST(
                    smiMediaRecovery::createChkptImage( sParseTree->oldName )
                    != IDE_SUCCESS );
                break;
            }
        case QD_MEDIA_RECOVERY_RENAME_FILE:
            {
                IDE_TEST(
                    smiMediaRecovery::renameDataFile( sParseTree->oldName,
                                                      sParseTree->newName )
                    != IDE_SUCCESS );
                break;
            }
        case QD_MEDIA_RECOVERY_DATABASE:
            {
                switch( sParseTree->recoverSpec->method )
                {
                    case QD_RECOVER_UNSPECIFIED:
                        {
                            sRecvType = SMI_RECOVER_COMPLETE;
                        }
                        break;
                    case QD_RECOVER_FROM_TAG:
                        {
                            sRecvType = SMI_RECOVER_UNTILTAG;
                            sFromTag = sParseTree->recoverSpec->fromTagSpec->tagName;
                        }
                        break;
                    case QD_RECOVER_UNTIL:
                        {
                            if( sParseTree->recoverSpec->untilSpec->cancel == ID_TRUE )
                            {
                                sRecvType = SMI_RECOVER_UNTILCANCEL;
                            }
                            else
                            {
                                sRecvType = SMI_RECOVER_UNTILTIME;
                                IDE_TEST(
                                    mtdDateInterface::toDate( &sDate,
                                                              (UChar*)sParseTree->recoverSpec->untilSpec->timeString,
                                                              idlOS::strlen(sParseTree->recoverSpec->untilSpec->timeString),
                                                              (UChar*)sFormat,
                                                              idlOS::strlen(sFormat) )
                                    != IDE_SUCCESS );

                                idlOS::memset( &sTM, 0x00, ID_SIZEOF(tm) );
                                sTM.tm_year = mtdDateInterface::year(&sDate) - 1900;
                                sTM.tm_mon  = mtdDateInterface::month(&sDate) - 1;
                                sTM.tm_mday = mtdDateInterface::day(&sDate);
                                sTM.tm_hour = mtdDateInterface::hour(&sDate);
                                sTM.tm_min  = mtdDateInterface::minute(&sDate);
                                sTM.tm_sec  = mtdDateInterface::second(&sDate);
                                sUntilTime = (UInt)idlOS::mktime( &sTM );
                            }
                        }
                        break;
                    default:
                        IDE_DASSERT(0);
                }

                IDE_TEST( smiMediaRecovery::recoverDB(aStatement->mStatistics, 
                                                      sRecvType, 
                                                      sUntilTime,
                                                      sFromTag)
                          != IDE_SUCCESS );
                break;
            }
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_ALTER_DATABASE);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

// PROJ-2133
// PROJ-2488 Incremental Backup in XDB
IDE_RC qdc::executeIncrementalBackup( qcStatement* aStatement )
{
    smiTrans                  sSmiTrans;
    smiStatement            * sDummySmiStmt = NULL;
    smiStatement              sSmiStmt;
    smiStatement            * sSmiStmtOrg = NULL;
    SInt                      sState = 0;
    UInt                      sSmiStmtFlag = 0;
    qdBackupParseTree       * sParseTree;
    qdIncrementalBackupSpec * sBackupSpec;
    qdTablespaceList        * sTbsList;
    smiBackupLevel            sBackupLevel;
    smiTableSpaceAttr         sTbsAttr;
    UShort                    sBackupType;
    SChar                     sName[SMI_MAX_TABLESPACE_NAME_LEN + 1];
    SChar                   * sTagName = NULL;
    idBool                    sCheckTagName;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    // transaction initialize
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;
    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    sParseTree = (qdBackupParseTree*)(aStatement->myPlan->parseTree);
    sBackupSpec = &sParseTree->incrementalBackupSpec;

    if( sBackupSpec->level == 0 )
    {
        sBackupLevel = SMI_BACKUP_LEVEL_0;
        sBackupType = SMI_BACKUP_TYPE_FULL;
    }
    else
    {
        sBackupLevel = SMI_BACKUP_LEVEL_1;
        if( sBackupSpec->cumulative == ID_FALSE )
        {
            sBackupType = SMI_BACKUP_TYPE_DIFFERENTIAL;
        }
        else
        {
            sBackupType = SMI_BACKUP_TYPE_CUMULATIVE;
        }
    }

    if( sBackupSpec->targetSpec->withTagSpec != NULL )
    {
        sTagName = sBackupSpec->targetSpec->withTagSpec->tagName;
    }

    switch( sBackupSpec->targetSpec->granularity )
    {
        case QD_BACKUP_DATABASE:
        {
            IDE_TEST(
                smiBackup::incrementalBackupDatabase(
                    aStatement->mStatistics,
                    (QC_SMI_STMT( aStatement ))->getTrans(),
                    sBackupLevel,
                    sBackupType,
                    NULL,
                    sTagName )
                != IDE_SUCCESS );
            break;
        }
        case QD_BACKUP_TABLESPACE:
        {
            sTbsList = sBackupSpec->targetSpec->tablespaces;
            while( sTbsList != NULL )
            {
                idlOS::memcpy( sName,
                               sTbsList->namePosition.stmtText + sTbsList->namePosition.offset,
                               sTbsList->namePosition.size );
                sName[sTbsList->namePosition.size] = 0;

                IDE_TEST( qcmTablespace::getTBSAttrByName(
                              aStatement,
                              sName,
                              idlOS::strlen(sName),
                              &sTbsAttr ) != IDE_SUCCESS );
                sTbsList->id = sTbsAttr.mID;

                sTbsList = sTbsList->next;
            }

            sTbsList = sBackupSpec->targetSpec->tablespaces;
            sCheckTagName = ID_TRUE;
            while( sTbsList != NULL )
            {
                IDE_TEST(
                    smiBackup::incrementalBackupTableSpace(
                        aStatement->mStatistics,
                        (QC_SMI_STMT( aStatement ))->getTrans(),
                        sTbsList->id,
                        sBackupLevel,
                        sBackupType,
                        NULL,
                        sTagName,
                        sCheckTagName )
                    != IDE_SUCCESS );

                sTbsList = sTbsList->next;

                sCheckTagName = ID_FALSE;
            }
            break;
        }
        default:
            IDE_DASSERT(0);
            break;
    }

    //restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeRestore( qcStatement* aStatement )
{
    qdRestoreParseTree * sParseTree;
    qdRecoverSpec      * sRestoreSpec;
    mtdDateType          sDate;
    struct tm            sTM;
    UInt                 sUntilTime = UINT_MAX;
    static const SChar * sFormat    = "YYYY-MM-DD:HH:MI:SS";
    SChar              * sFromTag   = NULL;
    SChar                sName[SMI_MAX_TABLESPACE_NAME_LEN + 1];
    smiRestoreType       sRecvType = SMI_RESTORE_COMPLETE;
    qdTablespaceList   * sTbsList;
    smiTableSpaceAttr    sTbsAttr;

    // BUG-26088 mmcStatement::beginDB() ���� valgrind ������ �߻��մϴ�.
    // mm ���� ���Ѱ˻縦 �ϸ� �ȵȴ�. QP ���� ���� ���ֵ��� �Ѵ�.
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdRestoreParseTree*)(aStatement->myPlan->parseTree);

    switch( sParseTree->targetSpec->granularity )
    {
        case QD_BACKUP_DATABASE:
        {
            sRestoreSpec = sParseTree->targetSpec->databaseRestoreSpec;
            switch( sRestoreSpec->method )
            {
                case QD_RECOVER_UNSPECIFIED:
                    {
                        sRecvType = SMI_RESTORE_COMPLETE;
                    }
                    break;
                case QD_RECOVER_FROM_TAG:
                    {
                        sRecvType = SMI_RESTORE_UNTILTAG;
                        sFromTag = sRestoreSpec->fromTagSpec->tagName;
                    }
                    break;
                case QD_RECOVER_UNTIL:
                    {
                        sRecvType = SMI_RESTORE_UNTILTIME;
                        IDE_TEST(
                            mtdDateInterface::toDate( &sDate,
                                                      (UChar*)sRestoreSpec->untilSpec->timeString,
                                                      idlOS::strlen(sRestoreSpec->untilSpec->timeString),
                                                      (UChar*)sFormat,
                                                      idlOS::strlen(sFormat) )
                            != IDE_SUCCESS );

                        idlOS::memset( &sTM, 0x00, ID_SIZEOF(tm) );
                        sTM.tm_year = mtdDateInterface::year( &sDate ) - 1900;
                        sTM.tm_mon  = mtdDateInterface::month( &sDate ) - 1;
                        sTM.tm_mday = mtdDateInterface::day( &sDate );
                        sTM.tm_hour = mtdDateInterface::hour( &sDate );
                        sTM.tm_min  = mtdDateInterface::minute( &sDate );
                        sTM.tm_sec  = mtdDateInterface::second( &sDate );
                        sUntilTime = (UInt)idlOS::mktime( &sTM );
                    }
                    break;
                default:
                    IDE_DASSERT(0);
            }

            IDE_TEST( smiMediaRecovery::restoreDB( sRecvType, 
                                                   sUntilTime,
                                                   sFromTag )
                      != IDE_SUCCESS );
            break;
        }
        case QD_BACKUP_TABLESPACE:
        {
            sTbsList = sParseTree->targetSpec->tablespaces;
            while( sTbsList != NULL )
            {
                idlOS::memcpy( sName,
                               sTbsList->namePosition.stmtText + sTbsList->namePosition.offset,
                               sTbsList->namePosition.size );
                sName[sTbsList->namePosition.size] = 0;

                IDE_TEST( qcmTablespace::getTBSAttrByName(
                              aStatement,
                              sName,
                              idlOS::strlen(sName),
                              &sTbsAttr ) != IDE_SUCCESS );
                sTbsList->id = sTbsAttr.mID;

                sTbsList = sTbsList->next;
            }

            sTbsList = sParseTree->targetSpec->tablespaces;
            while( sTbsList != NULL )
            {
                IDE_TEST(
                    smiMediaRecovery::restoreTBS( sTbsList->id )
                    != IDE_SUCCESS );

                sTbsList = sTbsList->next;
            }

            break;
        }
        default:
            IDE_DASSERT(0);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdc::executeChangeTracking( qcStatement* aStatement )
{
    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement    * sSmiStmtOrg   = NULL;
    smiStatement      sSmiStmt;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;

    qdChangeTrackingParseTree* sParseTree;

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdChangeTrackingParseTree*)(aStatement->myPlan->parseTree);

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt, 
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );
    
    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    if( sParseTree->enable == ID_TRUE )
    {
        IDE_TEST( smiBackup::enableChangeTracking() 
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( smiBackup::disableChangeTracking() 
                  != IDE_SUCCESS );
    }

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    sState = 2;
    // statement close
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    sState = 1;
    // transaction commit
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    sState = 0;
    // transaction destroy
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeRemoveBackupInfoFile( qcStatement * aStatement )
{
    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement    * sSmiStmtOrg   = NULL;
    smiStatement      sSmiStmt;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    IDE_TEST( smiBackup::removeBackupInfoFile() 
              != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    sState = 2;
    // statement close
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    sState = 1;
    // transaction commit
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    sState = 0;
    // transaction destroy
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeRemoveObsoleteBackupFile( qcStatement * aStatement )
{
    smiTrans                        sSmiTrans;
    smiStatement                  * sDummySmiStmt = NULL;
    smiStatement                  * sSmiStmtOrg   = NULL;
    smiStatement                    sSmiStmt;
    SInt                            sState        = 0;
    UInt                            sSmiStmtFlag  = 0;

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    IDE_TEST( smiBackup::removeObsoleteBackupFile()
              != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    sState = 2;
    // statement close
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    sState = 1;
    // transaction commit
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    sState = 0;
    // transaction destroy
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeChangeBackupDirectory( qcStatement* aStatement )
{
    smiTrans                        sSmiTrans;
    smiStatement                  * sDummySmiStmt = NULL;
    smiStatement                  * sSmiStmtOrg   = NULL;
    smiStatement                    sSmiStmt;
    SInt                            sState        = 0;
    UInt                            sSmiStmtFlag  = 0;

    qdChangeMoveBackupParseTree   * sParseTree;

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdChangeMoveBackupParseTree*)(aStatement->myPlan->parseTree);

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt, 
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    IDE_TEST( smiBackup::changeIncrementalBackupDir( sParseTree->path ) 
              != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    sState = 2;
    // statement close
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    sState = 1;
    // transaction commit
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    sState = 0;
    // transaction destroy
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::executeMoveBackupFile( qcStatement* aStatement )
{
    smiTrans                        sSmiTrans;
    smiStatement                  * sDummySmiStmt = NULL;
    smiStatement                  * sSmiStmtOrg   = NULL;
    smiStatement                    sSmiStmt;
    SInt                            sState        = 0;
    UInt                            sSmiStmtFlag  = 0;

    qdChangeMoveBackupParseTree * sParseTree;

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    sParseTree = (qdChangeMoveBackupParseTree*)(aStatement->myPlan->parseTree);

    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt, 
                               aStatement->mStatistics )
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_UNTOUCHABLE;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    IDE_TEST( smiBackup::moveBackupFile( sParseTree->path, 
                                         sParseTree->withContents ) 
              != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    sState = 2;
    // statement close
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    sState = 1;
    // transaction commit
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    sState = 0;
    // transaction destroy
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC qdc::switchLogFile( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : To fix BUG-11952
 *      ALTER SYSTEM SWITCH LOGFILE�� execution
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::switchLogFile"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));
    // check privileges
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );

    // TODO: sm interface�� ����Ͽ� �����ؾ� ��
    IDE_TEST( smiBackup::switchLogFile()
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::flushBufferPool( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : BUG-15010
 *      ALTER SYSTEM FLUSH BUFFER_POOL�� execution
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::flushBufferPool"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));
    // check privileges
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );

    IDE_TEST( smiFlushBufferPool( aStatement->mStatistics )
              != IDE_SUCCESS);

    IDE_TEST( smiFlushSBuffer( aStatement->mStatistics )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::flusherOnOff( qcStatement *aStatement,
                          UInt         aFlusherID,
                          idBool       aStart )
{
/***********************************************************************
 *
 * Description : PROJ-1568 BUFFER MANAGER RENEWAL
 *               flusher�� on, off�ϴ� DCL ����
 *
 * Implementation :
 *
 ***********************************************************************/
#define IDE_FN "qdc::flusherOnOff"

    // check privileges
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );

    IDE_TEST( smiFlusherOnOff( aStatement->mStatistics,
                               aFlusherID,
                               aStart )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA);
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::compactPlanCache( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1436
 *    ALTER SYSTEM COMPACT SQL_PLAN_CACHE
 *
 * Implementation :
 *    reference count�� 0�� plan cache�� ��� �����Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdc::compactPlanCache"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    void  * sMmStatement;

    sMmStatement = aStatement->stmtInfo->mMmStatement;
    
    // check privileges
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );

    // compact plan cache
    IDE_TEST( qci::mSessionCallback.mCompactPlanCache( sMmStatement )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::resetPlanCache( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-1436
 *    ALTER SYSTEM RESET SQL_PLAN_CACHE
 *
 * Implementation :
 *    plan cache�� �ʱ�ȭ�Ѵ�.
 *
 ***********************************************************************/

#define IDE_FN "qdc::resetPlanCache"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    void  * sMmStatement;

    sMmStatement = aStatement->stmtInfo->mMmStatement;
    
    // check privileges
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_SYSDBA );

    // compact plan cache
    IDE_TEST( qci::mSessionCallback.mResetPlanCache( sMmStatement )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_NO_GRANT_SYSDBA)
    {
        IDE_SET(ideSetErrorCode(qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                QCM_PRIV_NAME_SYSTEM_SYSDBA_STR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::rebuildMinViewSCN( qcStatement *aStatement )
{
/***********************************************************************
 *
 * Description : �ý����� Active Ʈ����ǵ� �߿� ���� ���� ViewSCN��
 *               ���ϰ� ������.
 *
 * BUG-23637 �ּ� ��ũ ViewSCN�� Ʈ����Ƿ������� Statement ������ ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    return smiRebuildMinViewSCN( aStatement->mStatistics );
}

IDE_RC qdc::startSecurity( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2002
 *    ALTER SYSTEM SECURITY START
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::startSecurity"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;

    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // start security
    IDE_TEST( qcs::startSecurity( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::stopSecurity( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2002
 *    ALTER SYSTEM SECURITY STOP
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::stopSecurity"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop security
    IDE_TEST( qcs::stopSecurity( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::startAudit( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2223
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::startAudit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;

    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // start audit
    IDE_TEST( qdcAudit::start( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::stopAudit( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2223
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::stopAudit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop audit
    IDE_TEST( qdcAudit::stop( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::reloadAudit( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2223
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::reloadAudit"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop audit
    IDE_TEST( qdcAudit::reload( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::auditOption( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2223
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::auditOption"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop audit
    IDE_TEST( qdcAudit::auditOption( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::noAuditOption( qcStatement *  aStatement )
{
/***********************************************************************
 *
 * Description : PROJ-2223
 *
 * Implementation :
 *
 ***********************************************************************/

#define IDE_FN "qdc::noAuditOption"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop audit
    IDE_TEST( qdcAudit::noAuditOption( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

/* BUG-39074 */
IDE_RC qdc::delAuditOption( qcStatement *  aStatement )
{
#define IDE_FN "qdc::delAuditOption"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(IDE_FN));

    smiTrans          sSmiTrans;
    smiStatement    * sDummySmiStmt = NULL;
    smiStatement      sSmiStmt;
    smiStatement    * sSmiStmtOrg   = NULL;
    SInt              sState        = 0;
    UInt              sSmiStmtFlag  = 0;
 
    // check server status
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SECURITY_NOT_IN_SERVICE_PHASE );

    // check privileges
    IDE_TEST( checkPrivileges( aStatement ) != IDE_SUCCESS );

    //-----------------------------------------------------
    // Security Module ����
    //-----------------------------------------------------
    
    // transaction initialze
    IDE_TEST( sSmiTrans.initialize() != IDE_SUCCESS );
    sState = 1;

    // transaction begin
    IDE_TEST( sSmiTrans.begin( &sDummySmiStmt,
                               aStatement->mStatistics)
              != IDE_SUCCESS );
    sState = 2;

    sSmiStmtFlag &= ~SMI_STATEMENT_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_NORMAL;

    sSmiStmtFlag &= ~SMI_STATEMENT_CURSOR_MASK;
    sSmiStmtFlag |= SMI_STATEMENT_MEMORY_CURSOR;

    // statement begin
    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics, sDummySmiStmt, sSmiStmtFlag )
              != IDE_SUCCESS );

    // swap
    qcg::getSmiStmt( aStatement, &sSmiStmtOrg );
    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 3;

    // stop audit
    IDE_TEST( qdcAudit::delAuditOption( aStatement ) != IDE_SUCCESS );

    // restore
    qcg::setSmiStmt( aStatement, sSmiStmtOrg );

    // statement close
    sState = 2;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )!= IDE_SUCCESS );

    // transaction commit
    sState = 1;
    IDE_TEST( sSmiTrans.commit() != IDE_SUCCESS );

    // transaction destroy
    sState = 0;
    IDE_TEST( sSmiTrans.destroy( aStatement->mStatistics ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SECURITY_NOT_IN_SERVICE_PHASE);
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QDC_SECURITY_NOT_IN_SERVICE_PHASE) );
    }
    IDE_EXCEPTION_END;

    switch(sState)
    {
        case 3:
            qcg::setSmiStmt( aStatement, sSmiStmtOrg );

            sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            sSmiTrans.rollback();
        case 1:
            sSmiTrans.destroy( aStatement->mStatistics );
        case 0:
        default:
            break;
    }
    
    return IDE_FAILURE;

#undef IDE_FN
}

IDE_RC qdc::changeFeatureProperty( qcStatement * aStatement,
                                   SChar       * aNewValue,
                                   void        * aArg )
{
/******************************************************************************
 *
 * Description : PROJ-2242 (OPTIMIZER_FEATURE_ENABLE)
 *               Plan �� ������ ��ġ�� property �� �ϰ� ����
 *
 *               BUG-43532
 *               OPTIMIZER_FEATURE_ENABLE�� �����ϸ� ������ PROPERTY��
 *               �ϰ� �����մϴ�.
 *
 * Implementation :
 *
 *    gFeatureProperty �� �����ϴ� property �ϰ� ����
 *     - New Version ���ϴ� new value�� ����
 *     - New Version �ʰ��� old value�� ����
 *
 * comment) system property �� �������� �����ϴ� ������ ���� �����
 *          value �� �ϳ��� ���� property �������� �ܼ��ϰ� ������.
 *
 *****************************************************************************/

    qdcFeatureVersion sNewVersion;
    UInt sLength;
    SInt i;
    
    sLength = idlOS::strlen( aNewValue );

    IDE_TEST_RAISE( (sLength <= 0) ||
                    (sLength > QCU_OPTIMIZER_FEATURE_VERSION_LEN),
                    err_invalid_feature_enable_value );

    IDE_TEST( getFeatureVersionNo( aNewValue, &sNewVersion )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sNewVersion == QDC_OPTIMIZER_FEATURE_VERSION_NONE,
                    err_invalid_feature_enable_value ); 

    // gFeatureProperty �� �����ϴ� property �ϰ� ����
    for ( i = 0; i < QDC_OPTIMIZER_FEATURE_CNT; i++ )
    {
        // 1. sNewVersion ���ϴ� new vlaue�� ����.
        if( sNewVersion >= gFeatureProperty[i].mVersion )
        {
            IDE_TEST( idp::validate(
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mNewValue,
                        ID_TRUE ) // isSystem
                      != IDE_SUCCESS );

            IDE_TEST( idp::update(
                        aStatement->mStatistics,
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mNewValue,
                        0,
                        aArg )
                      != IDE_SUCCESS );

            qcgPlan::registerPlanProperty(
                         aStatement,
                         gFeatureProperty[i].mPlanName );
        }
        // 2. sNewVersion �ʰ��� old value�� ����.
        else
        {
            IDE_TEST( idp::validate(
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mOldValue,
                        ID_TRUE )  // isSystem
                      != IDE_SUCCESS );

            IDE_TEST( idp::update(
                        aStatement->mStatistics,
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mOldValue,
                        0,
                        aArg )
                      != IDE_SUCCESS );

            qcgPlan::registerPlanProperty(
                         aStatement,
                         gFeatureProperty[i].mPlanName );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_feature_enable_value )
    {
        IDE_SET( ideSetErrorCode( 
                    qpERR_ABORT_QCU_INVALID_FEATURE_ENABLE_VALUE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdc::getFeatureVersionNo( SChar             * aVersionStr,
                                 qdcFeatureVersion * aVersionNo )
{
    if( idlOS::strMatch( (SChar *)"6.1.1.0.6",
                         idlOS::strlen( (SChar *)"6.1.1.0.6" ),
                         aVersionStr,
                         idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_0_6;
    }
    else if( idlOS::strMatch( (SChar *)"6.1.1.0.7",
                              idlOS::strlen( (SChar *)"6.1.1.0.7" ),
                              aVersionStr,
                              idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_0_7;
    }
    else if( idlOS::strMatch( (SChar *)"6.1.1.4.3",
                              idlOS::strlen( (SChar *)"6.1.1.4.3" ),
                              aVersionStr,
                              idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_6_1_1_4_3;
    }
    else if( idlOS::strMatch( (SChar *)"6.3.1.0.1",
                              idlOS::strlen( (SChar *)"6.3.1.0.1" ),
                              aVersionStr,
                              idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_6_3_1_0_1;
    }
    else if( idlOS::strMatch( (SChar *)"6.5.1.0.0",
                              idlOS::strlen( (SChar *)"6.5.1.0.0" ),
                              aVersionStr,
                              idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_6_5_1_0_0;
    }
    else if( idlOS::strMatch( (SChar *)"7.1.0.0.0",
                              idlOS::strlen( (SChar *)"7.1.0.0.0" ),
                              aVersionStr,
                              idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_7_1_0_0_0;
    }
    else if( idlOS::strMatch(
                (SChar *)IDU_ALTIBASE_VERSION_STRING,
                idlOS::strlen( (SChar *)IDU_ALTIBASE_VERSION_STRING ),
                aVersionStr,
                idlOS::strlen( aVersionStr ) ) == 0 )
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_MAX;
    }
    else
    {
        *aVersionNo = QDC_OPTIMIZER_FEATURE_VERSION_NONE;
    }

    return IDE_SUCCESS;
}

IDE_RC qdc::changeFeatureProperty4Startup( SChar * aNewValue )
{
/******************************************************************************
 *
 * Description : PROJ-2242 (OPTIMIZER_FEATURE_ENABLE)
 *               Plan �� ������ ��ġ�� property �� �ϰ� ����
 *
 *               BUG-43532
 *               OPTIMIZER_FEATURE_ENABLE�� �����ϸ� ������ PROPERTY��
 *               �ϰ� �����մϴ�.
 *
 *               BUG-43533
 *               Server startup�ÿ��� OPTIMIZER_FEATURE_ENABLE�� �����ص�
 *               property, env�� ������ property�� �������� �ʽ��ϴ�.
 *               �̸� ���� idp::update ��� idp::update4Startup�� ȣ���մϴ�.
 * Implementation :
 *
 *    gFeatureProperty �� �����ϴ� property �ϰ� ����
 *     - New Version ���ϴ� new value�� ����
 *     - New Version �ʰ��� old value�� ����
 *
 * comment) system property �� �������� �����ϴ� ������ ���� �����
 *          value �� �ϳ��� ���� property �������� �ܼ��ϰ� ������.
 *
 *****************************************************************************/

    qdcFeatureVersion sNewVersion;
    UInt sLength;
    SInt i;

    sLength = idlOS::strlen( aNewValue );

    IDE_TEST_RAISE( (sLength <= 0) ||
                    (sLength > QCU_OPTIMIZER_FEATURE_VERSION_LEN),
                    err_invalid_feature_enable_value );

    IDE_TEST( getFeatureVersionNo( aNewValue, &sNewVersion )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sNewVersion == QDC_OPTIMIZER_FEATURE_VERSION_NONE,
                    err_invalid_feature_enable_value ); 

    // gFeatureProperty �� �����ϴ� property �ϰ� ����
    for ( i = 0; i < QDC_OPTIMIZER_FEATURE_CNT; i++ )
    {
        // 1. sNewVersion ���ϴ� new vlaue�� ����.
        if( sNewVersion >= gFeatureProperty[i].mVersion )
        {
            IDE_TEST( idp::validate(
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mNewValue,
                        ID_TRUE )  // isSystem
                      != IDE_SUCCESS );

            IDE_TEST( idp::update4Startup(
                        NULL,
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mNewValue,
                        0,
                        NULL )
                      != IDE_SUCCESS );
        }
        // 2. sNewVersion �ʰ��� old value�� ����.
        else
        {
            IDE_TEST( idp::validate(
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mOldValue,
                        ID_TRUE )  // isSystem
                      != IDE_SUCCESS );

            IDE_TEST( idp::update4Startup(
                        NULL,
                        gFeatureProperty[i].mName,
                        gFeatureProperty[i].mOldValue,
                        0,
                        NULL )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_invalid_feature_enable_value )
    {
        IDE_SET( ideSetErrorCode( 
                    qpERR_ABORT_QCU_INVALID_FEATURE_ENABLE_VALUE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdc::reloadAccessList( qcStatement * aStatement )
{
/******************************************************************************
 *
 * Description : PROJ-2624
 *               [��ɼ�] MM - ������ access_list ������� ����
 *
 * Implementation :
 *    ���� ����� access list�� �����
 *    ACCESS_LIST_FILE���� ���ο� access list�� �о�´�.
 *
 *****************************************************************************/

    /* check privileges */
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA( aStatement ) != ID_TRUE,
                    ERR_NO_GRANT_RELOAD_ACCESS_LIST );

    /* RELOAD */
    IDE_TEST( qci::mSessionCallback.mReloadAccessList() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_GRANT_RELOAD_ACCESS_LIST );
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                 QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* PROJ-2626 Snapshot Export */
IDE_RC qdc::validateAlterDatabaseSnapshot( qcStatement * aStatement )
{
    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SNAPSHOT_NOT_IN_SERVICE_PHASE );

    IDE_TEST_RAISE( QCG_GET_SESSION_USER_IS_SYSDBA(aStatement) != ID_TRUE,
                    ERR_NO_GRANT_ALTER_DATABASE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SNAPSHOT_NOT_IN_SERVICE_PHASE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDC_SNAPSHOT_NOT_IN_SERVICE_PHASE ) );
    }
    IDE_EXCEPTION( ERR_NO_GRANT_ALTER_DATABASE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDP_INSUFFICIENT_PRIVILEGES,
                                  QCM_PRIV_NAME_SYSTEM_ALTER_DATABASE_STR ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2626 Snapshot Export */
IDE_RC qdc::executeAlterDatabaseSnapshot( qcStatement * aStatement )
{
    qdDatabaseParseTree * sParseTree = NULL;

    sParseTree = (qdDatabaseParseTree *)(aStatement->myPlan->parseTree);

    IDE_TEST_RAISE( qci::getStartupPhase() != QCI_STARTUP_SERVICE,
                    ERR_SNAPSHOT_NOT_IN_SERVICE_PHASE );

    if ( sParseTree->optionflag == QD_ALTER_DATABASE_SNAPSHOT_BEGIN )
    {
        IDE_TEST( qci::mSnapshotCallback.mSnapshotBeginEnd( ID_TRUE )
                  != IDE_SUCCESS );
    }
    else if ( sParseTree->optionflag == QD_ALTER_DATABASE_SNAPSHOT_END )
    {
        IDE_TEST( qci::mSnapshotCallback.mSnapshotBeginEnd( ID_FALSE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SNAPSHOT_NOT_IN_SERVICE_PHASE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDC_SNAPSHOT_NOT_IN_SERVICE_PHASE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

