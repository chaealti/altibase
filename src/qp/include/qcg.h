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
 * $Id: qcg.h 16929 2006-07-04 09:19:34Z shsuh $
 **********************************************************************/

#ifndef _O_QCG_H_
#define _O_QCG_H_ 1

#include <qci.h>
#include <qmxSessionCache.h>
#include <sdi.h>

#define QCG_DEFAULT_BIND_MEM_SIZE          (1024)

// ���������� qcStatement�� �Ҵ�޾� ����ϴ� ��쿡,
// qcStatement �ʱ�ȭ�� ���� ����ϴ� ����.
#define QCG_INTERNAL_OPTIMIZER_MODE        ( 0 ) // Cost
#define QCG_INTERNAL_SELECT_HEADER_DISPLAY ( 0 ) // No Display
#define QCG_INTERNAL_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE ( 0 )  // optimizer�� �Ǵ�

#define QCG_GET_SESSION_LANGUAGE( _QcStmt_ )    \
    qcg::getSessionLanguage( _QcStmt_ )

#define QCG_GET_SESSION_TABLESPACE_ID( _QcStmt_ )       \
    qcg::getSessionTableSpaceID( _QcStmt_ )

#define QCG_GET_SESSION_TEMPSPACE_ID( _QcStmt_ )        \
    qcg::getSessionTempSpaceID( _QcStmt_ )

#define QCG_GET_SESSION_USER_ID( _QcStmt_ )     \
    qcg::getSessionUserID( _QcStmt_ )

#define QCG_SET_SESSION_USER_ID( _QcStmt_, _userId_ )   \
    qcg::setSessionUserID( _QcStmt_, _userId_ )

#define QCG_GET_SESSION_USER_IS_SYSDBA( _QcStmt_)       \
    qcg::getSessionIsSysdbaUser( _QcStmt_ )

#define QCG_GET_SESSION_OPTIMIZER_MODE( _QcStmt_ )      \
    qcg::getSessionOptimizerMode( _QcStmt_ )

#define QCG_GET_SESSION_SELECT_HEADER_DISPLAY( _QcStmt_ )       \
    qcg::getSessionSelectHeaderDisplay( _QcStmt_ )

#define QCG_GET_SESSION_STACK_SIZE( _QcStmt_ )  \
    qcg::getSessionStackSize( _QcStmt_ )

#define QCG_GET_SESSION_NORMALFORM_MAXIMUM( _QcStmt_ )  \
    qcg::getSessionNormalFormMaximum( _QcStmt_ )

#define QCG_GET_SESSION_DATE_FORMAT( _QcStmt_ ) \
    qcg::getSessionDateFormat( _QcStmt_ )

/* PROJ-2209 DBTIMEZONE */
#define QCG_GET_SESSION_TIMEZONE_STRING( _QcStmt_ ) \
    qcg::getSessionTimezoneString( _QcStmt_ )

#define QCG_GET_SESSION_TIMEZONE_SECOND( _QcStmt_ ) \
    qcg::getSessionTimezoneSecond( _QcStmt_ )

#define QCG_GET_SESSION_ST_OBJECT_SIZE( _QcStmt_ ) \
    qcg::getSessionSTObjBufSize( _QcStmt_ )

// BUG-20767
#define QCG_GET_SESSION_USER_NAME( _QcStmt_ )     \
    qcg::getSessionUserName( _QcStmt_ )

#define QCG_GET_SESSION_USER_PASSWORD( _QcStmt_ ) \
    qcg::getSessionUserPassword( _QcStmt_ )

// BUG-19041
#define QCG_GET_SESSION_ID( _QcStmt_ )             \
    qcg::getSessionID( _QcStmt_ )

// PROJ-2002 Column Security
#define QCG_GET_SESSION_LOGIN_IP( _QcStmt_ ) \
    qcg::getSessionLoginIP( _QcStmt_ )

// PROJ-2660 hybrid sharding
#define QCG_GET_SESSION_SHARD_PIN( _QcStmt_ ) \
    qcg::getSessionShardPIN( _QcStmt_ )

#define QCG_GET_SESSION_SHARD_META_NUMBER( _QcStmt_ ) \
    qcg::getSessionShardMetaNumber( _QcStmt_ )

#define QCG_GET_LAST_SESSION_SHARD_META_NUMBER( _QcStmt_ ) \
    qcg::getLastSessionShardMetaNumber( _QcStmt_ )

// PROJ-2701 Sharding online data rebuild
#define QCG_GET_SESSION_IS_SHARD_USER_SESSION( _QcStmt_ ) \
    qcg::getSessionIsShardUserSession( _QcStmt_ )

/* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
#define QCG_GET_CALL_BY_SHARD_ANALYZE_PROTOCOL( _QcStmt_ ) \
    qcg::getCallByShardAnalyzeProtocol( _QcStmt_ )

#define QCG_GET_SESSION_SHARD_NODE_NAME( _QcStmt_ ) \
    qcg::getSessionShardNodeName( _QcStmt_ )

#define QCG_GET_SESSION_SHARD_SESSION_TYPE( _QcStmt_ ) \
    qcg::getSessionShardSessionType( _QcStmt_ )

/* BUG-45899 */
#define QCG_GET_SESSION_TRCLOG_DETAIL_SHARD( _QcStmt_ ) \
    qcg::getSessionTrclogDetailShard( _QcStmt_ )

#define QCG_GET_SESSION_EXPLAIN_PLAN( _QcStmt_ ) \
    qcg::getSessionExplainPlan( _QcStmt_ )

#define QCG_GET_SESSION_GTX_LEVEL( _QcStmt_ ) \
    qcg::getSessionGTXLevel( _QcStmt_ )

#define QCG_GET_SESSION_IS_GTX( _QcStmt_ ) \
    qcg::isGTxSession( _QcStmt_ )

#define QCG_GET_SESSION_IS_GCTX( _QcStmt_ ) \
    qcg::isGCTxSession( _QcStmt_ )

/* PROJ-2677 DDL synchronization */
#define QCG_GET_SESSION_REPLICATION_DDL_SYNC( _QcStmt_ ) \
    qcg::getReplicationDDLSync( _QcStmt_ )

#define QCG_GET_SESSION_IS_NEED_DDL_INFO( _QcStmt_ ) \
    qcg::getIsNeedDDLInfo( _QcStmt_ )

#define QCG_GET_SESSION_TRANSACTIONAL_DDL( _QcStmt_ ) \
    qcg::getTransactionalDDL( _QcStmt_ )

#define QCG_GET_SESSION_GLOBAL_DDL( _QcStmt_ ) \
    qcg::getGlobalDDL( _QcStmt_ )

// PROJ-1579 NCHAR
#define QCG_GET_SESSION_NCHAR_LITERAL_REPLACE( _QcStmt_ ) \
    qcg::getSessionNcharLiteralReplace( _QcStmt_ )

//BUG-21122
#define QCG_GET_SESSION_AUTO_REMOTE_EXEC( _QcStmt_ ) \
    qcg::getSessionAutoRemoteExec( _QcStmt_ )

#define QCG_SESSION_PRINT_TO_CLIENT( _QcStmt_, _aMessage_, _aMessageLen_ )      \
    ( qci::mSessionCallback.mPrintToClient( _QcStmt_->session->mMmSession,      \
                                            _aMessage_,                         \
                                            _aMessageLen_ ) )

#define QCG_SESSION_SAVEPOINT( _QsxEnvInfo_, _savePoint_, _inStoredProc_ )      \
    ( qci::mSessionCallback.mSavepoint( _QsxEnvInfo_->mSession->mMmSession,     \
                                        _savePoint_,                            \
                                        _inStoredProc_ ) )

#define QCG_SESSION_COMMIT( _QsxEnvInfo_, _inStoredProc_ )                      \
    ( qci::mSessionCallback.mCommit( _QsxEnvInfo_->mSession->mMmSession,        \
                                     _inStoredProc_ ) )

#define QCG_SESSION_ROLLBACK( _QsxEnvInfo_, _savePoint_, _inStoredProc_ )       \
    ( qci::mSessionCallback.mRollback( _QsxEnvInfo_->mSession->mMmSession,      \
                                       _savePoint_,                             \
                                       _inStoredProc_ ) )

// BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
#define QCG_GET_SESSION_OPTIMIZER_DEFAULT_TEMP_TBS_TYPE( _QcStmt_ )      \
    qcg::getSessionOptimizerDefaultTempTbsType( _QcStmt_ )

/* BUG-34830 */
#define QCG_GET_SESSION_TRCLOG_DETAIL_PREDICATE( _QcStmt_ )              \
    qcg::getSessionTrclogDetailPredicate( _QcStmt_ )

#define QCG_GET_SESSION_OPTIMIZER_DISK_INDEX_COST_ADJ( _QcStmt_ )        \
    qcg::getOptimizerDiskIndexCostAdj( _QcStmt_ )

// BUG-43736
#define QCG_GET_SESSION_OPTIMIZER_MEMORY_INDEX_COST_ADJ( _QcStmt_ )        \
    qcg::getOptimizerMemoryIndexCostAdj( _QcStmt_ )

/* PROJ-2208 Multi Currency */
#define QCG_GET_SESSION_CURRENCY( _QcStmt_ ) \
        ( qci::mSessionCallback.mGetNlsCurrency( (_QcStmt_)->session->mMmSession ))
#define QCG_GET_SESSION_ISO_CURRENCY( _QcStmt_) \
        ( qci::mSessionCallback.mGetNlsISOCurrency( (_QcStmt_)->session->mMmSession ))
#define QCG_GET_SESSION_NUMERIC_CHAR( _QcStmt_ ) \
        ( qci::mSessionCallback.mGetNlsNumChar( (_QcStmt_)->session->mMmSession ))

/* PROJ-2047 Strengthening LOB - LOBCACHE */
#define QCG_GET_LOB_CACHE_THRESHOLD( _QcStmt_ ) \
    qcg::getLobCacheThreshold( _QcStmt_ )

/* PROJ-1832 New database link. */
#define QCG_GET_DATABASE_LINK_SESSION( _QcStmt_ )       \
    qcg::getDatabaseLinkSession( _QcStmt_ )

/* PROJ-1090 Function-based Index */
#define QCG_GET_SESSION_QUERY_REWRITE_ENABLE( _QcStmt_ ) \
    qcg::getSessionQueryRewriteEnable( _QcStmt_ )

/* BUG-38430 */
#define QCG_GET_LAST_PROCESS_ROW( _QcStmt_ ) \
    qcg::getSessionLastProcessRow( _QcStmt_ )

/* PROJ-1812 ROLE */
#define QCG_GET_SESSION_USER_ROLE_LIST( _QcStmt_ )     \
    qcg::getSessionRoleList( _QcStmt_ )

/* PROJ-2441 flashback */
#define QCG_GET_SESSION_RECYCLEBIN_ENABLE( _QcStmt_ ) \
    qcg::getSessionRecyclebinEnable( _QcStmt_ )

#define QCG_GET_SESSION_PRINT_OUT_ENABLE( _QcStmt_ ) \
    qcg::getSessionPrintOutEnable( _QcStmt_ )

/* BUG-42853 LOCK TABLE�� UNTIL NEXT DDL ��� �߰� */
#define QCG_GET_SESSION_LOCK_TABLE_UNTIL_NEXT_DDL( _QcStmt_ )                                   \
    ( ( (_QcStmt_)->session->mMmSession != NULL ) ?                                             \
        qci::mSessionCallback.mGetLockTableUntilNextDDL( (_QcStmt_)->session->mMmSession ) :    \
        ID_FALSE )

#define QCG_SET_SESSION_LOCK_TABLE_UNTIL_NEXT_DDL( _QcStmt_, _aBoolValue_ )                     \
    {                                                                                           \
        if ( (_QcStmt_)->session->mMmSession != NULL )                                          \
        {                                                                                       \
            qci::mSessionCallback.mSetLockTableUntilNextDDL( (_QcStmt_)->session->mMmSession,   \
                                                             _aBoolValue_ );                    \
        }                                                                                       \
    }

#define QCG_GET_SESSION_TABLE_ID_OF_LOCK_TABLE_UNTIL_NEXT_DDL( _QcStmt_ )                               \
    ( ( (_QcStmt_)->session->mMmSession != NULL ) ?                                                     \
        qci::mSessionCallback.mGetTableIDOfLockTableUntilNextDDL( (_QcStmt_)->session->mMmSession ) :   \
        0 )

#define QCG_SET_SESSION_TABLE_ID_OF_LOCK_TABLE_UNTIL_NEXT_DDL( _QcStmt_, _aUIntValue_ )                 \
    {                                                                                                   \
        if ( (_QcStmt_)->session->mMmSession != NULL )                                                  \
        {                                                                                               \
            qci::mSessionCallback.mSetTableIDOfLockTableUntilNextDDL( (_QcStmt_)->session->mMmSession,  \
                                                                      _aUIntValue_ );                   \
        }                                                                                               \
    }

/* BUG-41511 supporting to similar DBMS_APPLICATION_INFO */
#define QCG_SET_SESSION_CLIENT_APP_INFO( _QcStmt_, _aInfo_, _aInfoLen_ )      \
( qci::mSessionCallback.mSetClientAppInfo( (_QcStmt_)->session->mMmSession,   \
                                           _aInfo_,                           \
                                           _aInfoLen_ ) )

#define QCG_SET_SESSION_MODULE_INFO( _QcStmt_, _aInfo_, _aInfoLen_ )       \
( qci::mSessionCallback.mSetModuleInfo( (_QcStmt_)->session->mMmSession,   \
                                        _aInfo_,                           \
                                        _aInfoLen_ ) )

#define QCG_SET_SESSION_ACTION_INFO( _QcStmt_, _aInfo_, _aInfoLen_ )       \
( qci::mSessionCallback.mSetActionInfo( (_QcStmt_)->session->mMmSession,   \
                                        _aInfo_,                           \
                                        _aInfoLen_ ) )


// BUG-41398 use old sort
#define QCG_GET_SESSION_USE_OLD_SORT( _QcStmt_ ) \
    qcg::getSessionUseOldSort( _QcStmt_ )

/* BUG-41561 */
#define QCG_GET_SESSION_LOGIN_USER_ID( _QcStmt_ ) \
    qcg::getSessionLoginUserID( _QcStmt_ )

// BUG-41944
#define QCG_GET_SESSION_ARITHMETIC_OP_MODE( _QcStmt_ ) \
    qcg::getArithmeticOpMode( _QcStmt_ )

/* PROJ-2462 Result Cache */
#define QCG_GET_SESSION_RESULT_CACHE_ENABLE( _QcStmt_ ) \
    qcg::getSessionResultCacheEnable( _QcStmt_ )
/* PROJ-2462 Result Cache */
#define QCG_GET_SESSION_TOP_RESULT_CACHE_MODE( _QcStmt_ ) \
    qcg::getSessionTopResultCacheMode( _QcStmt_ )
/* PROJ-2492 Dynamic sample selection */
#define QCG_GET_SESSION_OPTIMIZER_AUTO_STATS( _QcStmt_ ) \
    qcg::getSessionOptimizerAutoStats( _QcStmt_ )

#define QCG_GET_SESSION_IS_AUTOCOMMIT( _QcStmt_ ) \
    qcg::getSessionIsAutoCommit( _QcStmt_ )

#define QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( _QcStmt_ ) \
    qcg::getSessionIsInternalLocalOperation( _QcStmt_ )

#define QCG_GET_SESSION_SHARD_INTERNAL_LOCAL_OPERATION( _QcStmt_ ) \
    qcg::getSessionInternalLocalOperation( _QcStmt_ )

/* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
#define QCG_GET_SESSION_OPTIMIZER_TRANSITIVITY_OLD_RULE( _QcStmt_ ) \
    qcg::getSessionOptimizerTransitivityOldRule( _QcStmt_ )

/* BUG-42639 Monitoring query */
#define QCG_GET_SESSION_OPTIMIZER_PERFORMANCE_VIEW( _QcStmt_ ) \
    qcg::getSessionOptimizerPerformanceView( _QcStmt_ )

/* PROJ-2632 */
#define QCG_GET_SERIAL_EXECUTE_MODE( _QcStmt_ ) \
    qcg::getSerialExecuteMode( _QcStmt_ )

#define QCG_GET_SESSION_TRCLOG_DETAIL_INFORMATION( _QcStmt_ ) \
    qcg::getSessionTrclogDetailInformation( _QcStmt_ )

/* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
#define QCG_GET_REDUCE_PART_PREPARE_MEMORY( _QcStmt_ ) \
    qcg::getReducePartPrepareMemory( _QcStmt_ )

// PROJ-2727 
#define QCG_GET_SESSION_PROPERTY_ATTRIBUTE( _QcStmt_ ) \
    qcg::getSessionPropertyAttribute( _QcStmt_ )

#define QCG_SET_SESSION_PROPERTY_ATTRIBUTE( _QcStmt_, _aValue_ )  \
    {                                                                     \
        if ( (_QcStmt_)->session->mMmSession != NULL )                    \
        {                                                                 \
            qci::mSessionCallback.mSetPropertyAttribute( (_QcStmt_)->session->mMmSession, \
                                                          _aValue_ );     \
        }                                                                 \
    }

#define QCG_SET_SESSION_SHARD_IN_PSM_ENABLE( _QcStmt_, _aValue_ ) \
        qcg::setSessionShardInPSMEnable( _QcStmt_, _aValue_ )     \

#define QCG_GET_SESSION_SHARD_IN_PSM_ENABLE( _QcStmt_ ) \
    qcg::getSessionShardInPSMEnable( _QcStmt_ )

/* TASK-7219 Non-shard DML */
#define QCG_INCREASE_SESSION_STMT_EXEC_SEQ_FOR_SHARD_TX( _QcStmt_, _aIsIncreased_ )    \
    qcg::increaseStmtExecSeqForShardTx( _QcStmt_, _aIsIncreased_ )

#define QCG_DECREASE_SESSION_STMT_EXEC_SEQ_FOR_SHARD_TX( _QcStmt_ ) \
    qcg::decreaseStmtExecSeqForShardTx( _QcStmt_ )

#define QCG_GET_SESSION_STMT_EXEC_SEQ_FOR_SHARD_TX( _QcStmt_ ) \
    qcg::getStmtExecSeqForShardTx( _QcStmt_ )

#define QCG_GET_SHARD_PARTIAL_EXEC_TYPE( _QcStmt_ ) \
    qcg::getShardPartialExecType( _QcStmt_ )

// BUG-47861 INVOKE_USER_ID, INVOKE_USER_NAME function
#define QCG_GET_SESSION_INVOKE_USER_NAME( _QcStmt_ )     \
    qcg::getSessionInvokeUserName( _QcStmt_ )

#define QCG_SET_SESSION_INVOKE_USER_NAME( _QcStmt_, _userId_ )   \
    qcg::setSessionInvokeUserName( _QcStmt_, _userId_ )

/* BUG-48132 */
#define QCG_GET_PLAN_HASH_OR_SORT_METHOD( _QcStmt_ ) \
    qcg::getPlanHashOrSortMethod( _QcStmt_ )

/* BUG-48161 */
#define QCG_GET_BUCKET_COUNT_MAX( _QcStmt_ ) \
    qcg::getBucketCountMax( _QcStmt_ )


#define QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT( _QcStmt_ )     \
    qci::mSessionCallback.mGetShardDDLLockTimeout((_QcStmt_)->session->mMmSession)

#define QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT( _QcStmt_ )     \
    qci::mSessionCallback.mGetShardDDLLockTryCount((_QcStmt_)->session->mMmSession)

#define QCG_GET_SESSION_QUERY_TIMEOUT( _QcStmt_ )     \
    qci::mSessionCallback.mGetQueryTimeout((_QcStmt_)->session->mMmSession)

/* BUG-48348 */
#define QCG_GET_SESSION_ELIMINATE_COMMON_SUBEXPRESSION( _QcStmt_ ) \
    qcg::getEliminateCommonSubexpression( _QcStmt_ )

// BUG-48384
#define QCG_GET_SESSION_SHARD_CLIENT_TOUCH_NODE_COUNT( _QcStmt_ )    \
    qci::mSessionCallback.mGetClientTouchNodeCount((_QcStmt_)->session->mMmSession)

/* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
#define QCG_MEMPOOL_ELEMENT_CNT 16

/* TASK-7307 DML Data Consistency in Shard */
#define QCG_CHECK_SHARD_DML_CONSISTENCY( _QcStmt_ )                       \
    (                                                                     \
        ( ( SDU_SHARD_ENABLE == 1 ) &&                                    \
          ( QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( _QcStmt_ ) \
            != ID_TRUE )                                                  \
        )                                                                 \
        ? ID_TRUE : ID_FALSE                                              \
    )

struct  qciStatement;


typedef IDE_RC (*qcgDatabaseCallback)( idvSQL * aStatistics, void * aArg );

typedef IDE_RC (*qcgQueueCallback)( void * aArg);
//PROJ-1677 DEQUEUE
typedef IDE_RC (*qcgDeQueueCallback)( void * aArg,idBool aBookDeq);

/*
 * PROJ-1071 Parallel Query
 * parallel worker thread �ִ� ���� ����
 */
typedef struct qcgPrlThrUseCnt
{
    iduMutex mMutex;
    UInt     mCnt;
} qcgPrlThrUseCnt;

class qcg
{
private:

    // qp���ο��� ó���Ǵ� ���ǹ��� ���,
    // mm�� session ���� ���� ó���Ǿ�� ��.
    // ����, callback �Լ��� ����� �� �����Ƿ�,
    // ���������� session�� userID�� �����ϰ� ��� ���ؼ�.
    static UInt    mInternalUserID;
    static SChar * mInternalUserName;

    // BUG-17224
    // startup service�ܰ� �� meta cache�� �ʱ�ȭ�Ǿ����� ��Ÿ��.
    static idBool     mInitializedMetaCaches;
    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* To reduce the number of system calls and eliminate the contention
       on iduMemMgr::mClientMutex[IDU_MEM_QCI] in the moment of calling iduMemMgr::malloc/free,
       we better use iduMemPool than call malloc/free directly. */
    static iduMemPool mIduVarMemListPool;
    static iduMemPool mIduMemoryPool;
    static iduMemPool mSessionPool;
    static iduMemPool mStatementPool;

    /* PROJ-1071 Parallel Query */
    static qcgPrlThrUseCnt mPrlThrUseCnt;

    /* PROJ-2451 Concurrent Execute Package */
    static qcgPrlThrUseCnt mConcThrUseCnt;

public:
    //-----------------------------------------------------------------
    // allocStatement() :
    //     qcStatement�� ���� ���� �ʱ�ȭ �� Memory �Ҵ�
    // freeStatement() :
    //     qcStatement�� ������ ����
    // clearStatement() : : reset for a statement - for Direct Execution
    //     qcStatement�� RESET : Parsing, Validation������ ����
    //        - SQLExecDirect() �Ϸ� ��
    //        - SQLFreeStmt(SQL_DROP) :
    // closeStatement() :
    //     qcStatement�� Execution ���� ����
    //     (Parsing, Validation, Optimization�� ������ ��ȿ��)
    //        - SQLFreeStmt(SQL_CLOSE)
    // stepAfterPVO() :
    //     PVO(Parsing->Validation->Optimization)���Ŀ� �ݵ�� ȣ��
    //     ���� ������ ���� Template ������ �ʱ�ȭ��.
    //
    // stepAfterPXX() : Prepare Execution���� SQLExecute()�Ŀ� �ݵ�� ȣ��
    //     ���� ������ ���� �����ߴ� �ڿ��� �����ϰ�, Template������ �ʱ�ȭ��.
    //-----------------------------------------------------------------

    // qcStatement�� ���� ���� ���� �� Memory Manager�� ����
    static IDE_RC allocStatement( qcStatement * aStatement,
                                  qcSession   * aSession,
                                  qcStmtInfo  * aStmtInfo,
                                  idvSQL      * aStatistics );

    // qcTemplate�� ���� ���� ���� �ʱ�ȭ �� Memory �Ҵ�
    static IDE_RC allocSharedTemplate( qcStatement * aStatement,
                                       SInt          aStackSize );

    static IDE_RC allocPrivateTemplate( qcStatement * aStatement,
                                        iduMemory   * aMemory );

    static IDE_RC allocPrivateTemplate( qcStatement   * aStatement,
                                        iduVarMemList * aMemory );

    // qcStatement�� ����
    static IDE_RC freeStatement( qcStatement * aStatement );

    // qcStatement�� Parsing, Validation ������ ����
    static IDE_RC clearStatement( qcStatement * aStatement,
                                  idBool        aRebuild );

    // qcStatement�� Execution ������ ����
    static IDE_RC closeStatement( qcStatement * aStatement );

    // Parsing ������ �۾� ó��
    static IDE_RC fixAfterParsing( qcStatement * aStatement );

    // Validation ������ �۾� ó��
    static IDE_RC fixAfterValidation( qcStatement * aStatement );

    static IDE_RC fixAfterValidationAB( qcStatement * aQcStmt );

    // Optimization ������ �۾� ó��
    static IDE_RC fixAfterOptimization( qcStatement * aStatement );

    // Parsing, Validation, Optimization������ ó��
    // ���� ������ ���� �غ� �۾� ����
    static IDE_RC stepAfterPVO( qcStatement * aStatement );

    // ���� ������ private �������� ���
    static IDE_RC setPrivateArea( qcStatement * aStatement );

    //-------------------------------------------------
    // Query Processor���� �ʿ��� Session ������ ������� �Լ�.
    // MT �� SM�� ������ �����ϱ� ���Ͽ� ������.
    // Statement �Ҵ� ���� �ݵ�� �����Ͽ��� ��.
    //-------------------------------------------------

    // smiStatement�� ���� (for SM)
    inline static void setSmiStmt( qcStatement * aStatement,
                                   smiStatement * aSmiStmt )
    {
        QC_SMI_STMT( aStatement ) = aSmiStmt;
    }

    // smiStatement�� ȹ�� (for SM)
    inline static void getSmiStmt( qcStatement * aStatement,
                                   smiStatement ** aSmiStmt )
    {
        *aSmiStmt = QC_SMI_STMT( aStatement );
    }

    // smiTrans�� ���� (for SM)
    static void   setSmiTrans( qcStatement * aStatement,
                               smiTrans    * aSmiTrans );

    // smiTrans�� ȹ�� (for SM)
    static void   getSmiTrans( qcStatement * aStatement,
                               smiTrans   ** aSmiTrans );

    // SQL Statement�� ���� (for QP)
    static void   getStmtText( qcStatement  * aStatement,
                               SChar       ** aText,
                               SInt         * aTextLen );

    // SQL Statement�� ���� (for QP)
    // PROJ-2163
    static IDE_RC   setStmtText( qcStatement * aStatement,
                                 SChar       * aText,
                                 SInt          aTextLen );

    static const mtlModule* getSessionLanguage( qcStatement * aStatement );
    static scSpaceID  getSessionTableSpaceID( qcStatement * aStatement );
    static scSpaceID  getSessionTempSpaceID( qcStatement * aStatement );
    static void setSessionUserID( qcStatement * aStatement,
                                  UInt          aUserID );
    static UInt getSessionUserID( qcStatement * aStatement );
    static idBool getSessionIsSysdbaUser( qcStatement * aSatement );
    static UInt getSessionOptimizerMode( qcStatement * aStatement );
    static UInt getSessionSelectHeaderDisplay( qcStatement * aStatement );
    static UInt getSessionStackSize( qcStatement * aStatement );
    static UInt getSessionNormalFormMaximum( qcStatement * aStatement );
    static SChar* getSessionDateFormat( qcStatement * aStatement );
    /* PROJ-2209 DBTIMEZONE */
    static SChar* getSessionTimezoneString( qcStatement * aStatement );
    static SLong  getSessionTimezoneSecond( qcStatement * aStatement );

    static UInt getSessionSTObjBufSize( qcStatement * aStatement );
    static SChar* getSessionUserName( qcStatement * aStatement );
    static SChar* getSessionUserPassword( qcStatement * aStatement );
    static UInt getSessionID( qcStatement * aStatement );
    static SChar* getSessionLoginIP( qcStatement * aStatemet );
    static ULong getSessionShardPIN( qcStatement  * aStatement );
    static ULong getSessionShardMetaNumber( qcStatement * aStatement );
    static ULong getLastSessionShardMetaNumber( qcStatement * aStatement );
    static idBool getSessionIsShardUserSession( qcStatement * aStatement );
    /* TASK-7219 Analyzer/Transformer/Executor ���ɰ��� */
    static idBool getCallByShardAnalyzeProtocol( qcStatement * aStatement );

    static SChar* getSessionShardNodeName( qcStatement * aStatement );
    static UInt getSessionShardSessionType( qcStatement * aStatement );

    /* BUG-45899 */
    static UInt getSessionTrclogDetailShard( qcStatement * aStatement );

    static UChar  getSessionExplainPlan( qcStatement * aStatement );
    static UInt   getSessionGTXLevel( qcStatement * aStatement );
    static idBool isGTxSession( qcStatement * aStatement );
    static idBool isGCTxSession( qcStatement * aStatement );

    /* PROJ-2677 DDL synchronization */
    static UInt   getReplicationDDLSync( qcStatement * aStatement);

    static idBool getIsNeedDDLInfo( qcStatement * aStatement);
    static idBool getTransactionalDDL( qcStatement * aStatement);
    static idBool getGlobalDDL( qcStatement * aStatement);

    static idBool getIsRollbackableInternalDDL( qcStatement * aStatment );

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    static UInt getSessionOptimizerDefaultTempTbsType( qcStatement * aStatement );

    // PROJ-1579 NCHAR
    static UInt getSessionNcharLiteralReplace( qcStatement * aSatement );

    //BUG-21122
    static UInt getSessionAutoRemoteExec(qcStatement * aStatement);

    static SInt getOptimizerDiskIndexCostAdj(qcStatement * aStatement);

    // BUG-43736
    static SInt getOptimizerMemoryIndexCostAdj(qcStatement * aStatement);

    // BUG-34380
    static UInt getSessionTrclogDetailPredicate(qcStatement * aStatement);

    /* PROJ-2047 Strengthening LOB - LOBCACHE */
    static UInt getLobCacheThreshold(qcStatement * aStatement);

    /* PROJ-1090 Function-based Index */
    static UInt getSessionQueryRewriteEnable(qcStatement * aStatement);

    // BUG-38430
    static ULong getSessionLastProcessRow( qcStatement * aStatement );

    /* PROJ-1812 ROLE */
    static UInt* getSessionRoleList( qcStatement * aStatement );
    
    /* PROJ-2441 flashback */
    static UInt   getSessionRecyclebinEnable( qcStatement * aStatement );

    static UInt   getSessionPrintOutEnable( qcStatement * aStatement );
    
    // BUG-41398 use old sort
    static UInt   getSessionUseOldSort( qcStatement * aStatement );

    /* BUG-41561 */
    static UInt getSessionLoginUserID( qcStatement * aStatement );

    static void setSessionInvokeUserName( qcStatement * aStatement,
                                          SChar       * aInvokeUserName );
    static SChar* getSessionInvokeUserName( qcStatement * aStatement );

    // BUG-41944
    static mtcArithmeticOpMode getArithmeticOpMode( qcStatement * aStatement );
    
    /* PROJ-1832 New database link */
    static void * getDatabaseLinkSession( qcStatement * aStatement );

    /* PROJ-2462 Result Cache */
    static UInt getSessionResultCacheEnable(qcStatement * aStatement);
    /* PROJ-2462 Result Cache */
    static UInt getSessionTopResultCacheMode(qcStatement * aStatement);
    /* PROJ-2492 Dynamic sample selection */
    static UInt getSessionOptimizerAutoStats(qcStatement * aStatement);
    /* PROJ-2462 Result Cache */
    static idBool getSessionIsAutoCommit(qcStatement * aStatement);

    static idBool getSessionIsInternalLocalOperation(qcStatement * aStatement);
    static IDE_RC setSessionIsInternalLocalOperation( qcStatement * aStatement, sdiInternalOperation aValue );

    static sdiInternalOperation getSessionInternalLocalOperation(qcStatement * aStatement);

    // BUG-38129
    static void getLastModifiedRowGRID( qcStatement * aStatement,
                                        scGRID      * aRowGRID );

    static void setLastModifiedRowGRID( qcStatement * aStatement,
                                        scGRID        aRowGRID );

    /* BUG-42134 Created transitivity predicate of join predicate must be reinforced. */
    static UInt getSessionOptimizerTransitivityOldRule(qcStatement * aStatement);

    // PROJ-2727
    static UInt getSessionPropertyAttribute( qcStatement * aStatement );
    
    /* BUG-42639 Monitoring query */
    static UInt getSessionOptimizerPerformanceView(qcStatement * aStatement);

    // BUG-17224
    static idBool isInitializedMetaCaches();

    /* PROJ-2632 */
    static UInt getSerialExecuteMode( qcStatement * aStatement );
    static UInt getSessionTrclogDetailInformation( qcStatement * aStatement );
   
    /* BUG-47648  disk partition���� ���Ǵ� prepared memory ��뷮 ���� */
    static UInt getReducePartPrepareMemory( qcStatement * aStatement );

    static idBool getSessionShardInPSMEnable( qcStatement * aStatement );
    static void setSessionShardInPSMEnable( qcStatement * aStatement,
                                            idBool        aVal );

    /* BUG-48132 */
    static UInt getPlanHashOrSortMethod( qcStatement * aStatement );

    /* BUG-48161 */
    static UInt getBucketCountMax( qcStatement * aStatement );

    /* BUG-48348 */
    static UInt getEliminateCommonSubexpression( qcStatement * aStatement );

    /* TASK-7219 Non-shard DML */
    static void increaseStmtExecSeqForShardTx( qcStatement * aStatement,
                                               idBool      * aIsIncreased );
    static void decreaseStmtExecSeqForShardTx( qcStatement * aStatement );
    static UInt getStmtExecSeqForShardTx( qcStatement * aStatement );
    static sdiShardPartialExecType getShardPartialExecType( qcStatement * aStatement );

    //-------------------------------------------------
    // ���� ���� �Ϸ� ���� ó��
    //-------------------------------------------------

    // ��� Cursor�� Close
    // ���� ���� �Ϸ� �� Open�� Cursor�� ��� ����
    static IDE_RC closeAllCursor( qcStatement * aStatement );

    //-------------------------------------------------
    // Host ������ Binding�� ���� ó��
    //-------------------------------------------------

    // Host������ Column ������ ���� (bindParamInfo���� ȣ��)
    static IDE_RC setBindColumn( qcStatement * aStatement,
                                 UShort        aId,
                                 UInt          aType,
                                 UInt          aArguments,
                                 SInt          aPrecision,
                                 SInt          aScale );

    // Host ������ ���� ȹ��
    static UShort   getBindCount( qcStatement * aStatement );

    // Host ������ ���� ����
    // Host ������ Column ���� ���� ���Ŀ� �̿� ���� ���� ����
    // ������ �� �����.
    static IDE_RC setBindData( qcStatement * aStatement,
                               UShort        aId,
                               UInt          aInOutType,
                               UInt          aSize,
                               void        * aData );
    // prj-1697
    static IDE_RC setBindData( qcStatement * aStatement,
                               UShort        aId,
                               UInt          aInOutType,
                               void        * aBindParam,
                               void        * aBindContext,
                               void        * aSetParamDataCallback);

    // PROJ-2163
    static IDE_RC initBindParamData( qcStatement * aStatement );

    static IDE_RC fixOutBindParam( qcStatement * aStatement );

    //-------------------------------------------------
    // Query Processor ���������� ����ϱ� ���� �Լ�
    //-------------------------------------------------

    /* PROJ-2677 DDL syncrhonization & 
     * PROJ-2737 Internal Replication (for internal ddl execution) */
    static IDE_RC  runDDLforInternal( idvSQL       * aStatistics,
                                      smiStatement * aSmiStmt,
                                      UInt           aUserID,
                                      UInt           aSessionFlag,
                                      SChar        * aSqlStr );

    static IDE_RC  runDDLforInternalWithMmSession( idvSQL       * aStatistics,
                                                   void         * aMmSession,
                                                   smiStatement * aSmiStmt,
                                                   UInt           aUserID,
                                                   UInt           aSessionFlag,
                                                   SChar        * aSqlStr );

    static IDE_RC rebuildInternalStatement( qcStatement * aStatement );
    static IDE_RC ddlBeginSavepoint( qcStatement * aStatement );
    static IDE_RC backupNewTableInfo( qcStatement * aStatement );
    static IDE_RC backupOldTableInfo( qcStatement * aStatement );
    static IDE_RC runRollbackableInternalDDL( qcStatement  * aStatement,
                                              smiStatement * aSmiStmt,
                                              UInt           aUserID,
                                              SChar        * aSqlStr );

    static IDE_RC runDCLforInternal( qcStatement  * aStatement,
                                     SChar        * aSqlStr,
                                     void         * aSession );

    // DDL ���� �ÿ� Meta�� ���� �߻��ϴ� DML�� ó��
    static IDE_RC runDMLforDDL(smiStatement * aSmiStmt,
                               SChar        * aSqlStr,
                               vSLong       * aRowCnt);

    static IDE_RC runDMLforInternal( smiStatement  * aSmiStmt,
                                     SChar         * aSqlStr,
                                     vSLong        * aRowCnt );

    /* PROJ-2701 Sharding online data rebuild */
    static IDE_RC runSQLforShardMeta( smiStatement * aSmiStmt,
                                      SChar        * aSqlStr,
                                      vSLong       * aRowCnt,
                                      qcSession    * aSession = NULL );

    /* PROJ-2207 Password policy support */
    static IDE_RC runSelectOneRowforDDL(smiStatement * aSmiStmt,
                                        SChar        * aSqlStr,
                                        void         * aResult,
                                        idBool       * aRecordExist,
                                        idBool         aCalledPWVerifyFunc );

    /* PROJ-2441 flashback */
    static IDE_RC runSelectOneRowMultiColumnforDDL( smiStatement * aSmiStmt,
                                                    SChar        * aSqlStr,
                                                    idBool       * aRecordExist,
                                                    UInt           aColumnCount,
                                                    ... );

    // PROJ-1436
    // shared template�� ����� template ������ ����
    static IDE_RC cloneTemplate( iduVarMemList * aMemory,
                                 qcTemplate    * aSource,
                                 qcTemplate    * aDestination );

    // Stored Procedure ���� �� ����� Template ������ �����ϰ� �ʱ�ȭ ��
    static IDE_RC cloneAndInitTemplate( iduMemory   * aMemory,
                                        qcTemplate  * aSource,
                                        qcTemplate  * aDestination );

    // cloneTemlpate() , cloneAndInitTemplate���� date pseudo column ������ ������
    static IDE_RC cloneTemplateDatePseudoColumn( iduMemory   * aMemory,
                                                 qcTemplate  * aSource,
                                                 qcTemplate  * aDestination );

    /* fix BUG-29965 SQL Plan Cache���� plan execution template ������
     * Dynamic SQL ȯ�濡���� ������ �ʿ��ϴ�.
     * prepared�� execution template ���� ��ƾ�� �ߺ��Ǿ�,
     * ������ ���� �����Լ��� �߰��Ͽ����ϴ�.
     */
    static void freePrepTemplate( qcStatement * aStatement,
                                  idBool        aRebuild );

    //-------------------------------------------------
    // MM, UT���� Ư�� ó�� ������ ���� �Լ�
    //-------------------------------------------------

    // Stack Size�� ������ (for MM)
    static IDE_RC refineStackSize( qcStatement * aStatement, SInt aStackSize );

    // MEMORY TABLE or DISK TABLE ���� ����
    // validation ���� ���Ŀ� ȣ���ؾ� ��
    static IDE_RC getSmiStatementCursorFlag( qcTemplate * aTemplate,
                                             UInt       * aFlag );

    // PROJ-1358
    // System�� Query Stack Size�� ȹ��
    static UInt   getQueryStackSize();

    // BUG-26017 [PSM] server restart�� ����Ǵ� psm load��������
    // ����������Ƽ�� �������� ���ϴ� ��� ����.
    static UInt   getOptimizerMode();

    static UInt   getAutoRemoteExec();

    // BUG-23780 TEMP_TBS_MEMORY ��Ʈ ���뿩�θ� property�� ����
    static UInt   getOptimizerDefaultTempTbsType();

    // set callback functions
    static qcgDatabaseCallback mStartupCallback;
    static qcgDatabaseCallback mShutdownCallback;
    static qcgDatabaseCallback mCreateCallback;
    static qcgDatabaseCallback mDropCallback;
    static qcgDatabaseCallback mCloseSessionCallback;
    static qcgDatabaseCallback mCommitDTXCallback;
    static qcgDatabaseCallback mRollbackDTXCallback;

    static void setDatabaseCallback(
        qcgDatabaseCallback aCreatedbFuncPtr,
        qcgDatabaseCallback aDropdbFuncPtr,
        qcgDatabaseCallback aCloseSession,
        qcgDatabaseCallback aCommitDTX,
        qcgDatabaseCallback aRollbackDTX,
        qcgDatabaseCallback aStartupFuncPtr,
        qcgDatabaseCallback aShutdownFuncPtr);

    static qcgQueueCallback  mCreateQueueFuncPtr;
    static qcgQueueCallback  mDropQueueFuncPtr;
    static qcgQueueCallback  mEnqueueQueueFuncPtr;
    //PROJ-1677 DEQUE
    static qcgDeQueueCallback mDequeueQueueFuncPtr;
    static qcgQueueCallback   mSetQueueStampFuncPtr;

    static void setQueueCallback(
        qcgQueueCallback aCreateQueueFuncPtr,
        qcgQueueCallback aDropQueueFuncPtr,
        qcgQueueCallback aEnqueueQueueFuncPtr,
        //PROJ-1677 DEQUE
        qcgDeQueueCallback aDequeueQueueFuncPtr,
        qcgQueueCallback   aSetQueueStampFuncPtr);

    static IDE_RC buildPerformanceView( idvSQL * aStatistics );

    // �ش� ������ FT �� PV���� ����ϴ°����� �˻�.
    static idBool isFTnPV( qcStatement * aStatement );
    static IDE_RC detectFTnPV( qcStatement * aStatement );

    // PROJ-1371 PSM File Handling
    static IDE_RC initSessionObjInfo( qcSessionObjInfo ** aSessionObj );
    static IDE_RC finalizeSessionObjInfo( qcSessionObjInfo ** aSessionObj );

    // PROJ-1904 Extend UDT
    static IDE_RC resetSessionObjInfo( qcSessionObjInfo ** aSessionObj );

    static ULong  getOptimizeMode(qcStatement * aStatement );
    static ULong  getTotalCost(qcStatement * aStatement );

    static void lock  ( qcStatement * aStatement );
    static void unlock( qcStatement * aStatement );

    static void setPlanTreeState(qcStatement * aStatement,
                                 idBool        aPlanTreeFlag);

    static idBool getPlanTreeState(qcStatement * aStatement);

    static IDE_RC startupPreProcess( idvSQL *aStatistics );
    static IDE_RC startupProcess();
    static IDE_RC startupControl();
    static IDE_RC startupMeta();
    static IDE_RC startupService( idvSQL * aStatistics );
    static IDE_RC startupShutdown( idvSQL * aStatistics );
    static IDE_RC startupDowngrade( idvSQL * aStatistics ); /* PROJ-2689 */

    // plan tree text ���� ȹ��.
    static IDE_RC printPlan( qcStatement  * aStatement,
                             qcTemplate   * aTemplate,
                             ULong          aDepth,
                             iduVarString * aString,
                             qmnDisplay     aMode );

    // BUG-25109
    // simple select query���� base table name�� ����
    static void setBaseTableInfo( qcStatement * aStatement );
    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    static IDE_RC allocIduVarMemList(void         **aVarMemList);
    static IDE_RC freeIduVarMemList(iduVarMemList *aVarMemList);

    /* PROJ-2462 Result Cache */
    static IDE_RC allocIduMemory( void ** aMemory );
    static void freeIduMemory( iduMemory * aMemory );

    // PROJ-1073 Package
    static IDE_RC allocSessionPkgInfo( qcSessionPkgInfo ** aSessionPkg );
    static IDE_RC allocTemplate4Pkg( qcSessionPkgInfo * aSessionPkg,
                                     iduMemory * aMemory );

    /* PROJ-1438 Job Scheduler */
    static IDE_RC runProcforJob( UInt     aJobThreadIndex,
                                 void   * aMmSession,
                                 SInt     aJob,
                                 SChar  * aSqlStr,
                                 UInt     aSqlStrLen,
                                 UInt   * aErrorCode );

    static IDE_RC runTempSQL( void   * aMmSession,
                              SChar  * aSqlStr,
                              UInt     aSqlStrLen,
                              UInt   * aErrorCode );
    /*
     * --------------------------------------------------------------
     * PROJ-1071 Parallel Query
     * --------------------------------------------------------------
     */
    static IDE_RC allocAndCloneTemplate4Parallel( iduMemory  * aMemory,
                                                  iduMemory  * aCacheMemory, // PROJ-2452
                                                  qcTemplate * aSource,
                                                  qcTemplate * aDestination );

    static IDE_RC cloneTemplate4Parallel( iduMemory  * aMemory,
                                          qcTemplate * aSource,
                                          qcTemplate * aDestination );

    static IDE_RC reservePrlThr( UInt aRequire, UInt* aReserveCnt );
    static IDE_RC releasePrlThr( UInt aCount );
    static IDE_RC finishAndReleaseThread( qcStatement* aStatement );
    static IDE_RC joinThread( qcStatement* aStatement );

    /* BUG-38294 */
    static IDE_RC allocPRLQExecutionMemory( qcStatement  * aStatement,
                                            iduMemory   ** aMemory,         // QmxMem,
                                            iduMemory   ** aCacheMemory );  // QxcMem PROJ-2452

    static void freePRLQExecutionMemory(qcStatement * aStatement);

    // PROJ-2527 WITHIN GROUP AGGR
    static IDE_RC addPRLQChildTemplate( qcStatement * aStatement,
                                        qcTemplate  * aTemplate );

    static void finiPRLQChildTemplate( qcStatement * aStatement );
    
    /* PROJ-2451 Concurrent Execute Package */
    static IDE_RC reserveConcThr( UInt aRequire, UInt* aReserveCnt );
    static IDE_RC releaseConcThr( UInt aCount );

    // BUG-41248 DBMS_SQL package
    static IDE_RC openCursor( qcStatement * aStatement,
                              UInt        * aCursorId );

    static IDE_RC isOpen( qcStatement * aStatement,
                          UInt          aCursorId,
                          idBool      * aIsOpen );

    static IDE_RC parse( qcStatement * aStatement,
                         UInt          aCursorId,
                         SChar       * aSql );

    static IDE_RC bindVariable( qcStatement * aStatement,
                                UInt          aCursorId,
                                mtdCharType * aName,
                                mtcColumn   * aColumn,
                                void        * aValue );

    static IDE_RC executeCursor( qcStatement * aStatement,
                                 UInt          aCursorId,
                                 vSLong      * aRowCount );

    static IDE_RC fetchRows( qcStatement * aStatement,
                             UInt          aCursorId,
                             UInt        * aRowCount );

    static IDE_RC columnValue( qcStatement * aStatement,
                               UInt          aCursorId,
                               UInt          aPosition,
                               mtcColumn   * aColumn,
                               void        * aValue,
                               UInt        * aColumnError,
                               UInt        * aActualLength );

    static IDE_RC closeCursor( qcStatement * aStatement,
                               UInt          aCursorId );

    // BUG-44856
    static IDE_RC lastErrorPosition( qcStatement * aStatement,
                                     SInt        * aErrorPosition );

    /* BUG-44563
       trigger ���� �� server stop�ϸ� ������ �����ϴ� ��찡 �߻��մϴ�. */
    static IDE_RC freeSession( qcStatement * aStatement );

    static void prsCopyStrDupAppo( SChar * aDest,
                                   SChar * aSrc,
                                   UInt    aLength );
    
private:

    //-------------------------------------------------
    // qcStatement ���� �ʱ�ȭ �Լ�
    //-------------------------------------------------

    // qcTemplate�� �޸� �Ҵ� �� �ʱ�ȭ
    static IDE_RC allocAndInitTemplateMember( qcTemplate * aTemplate,
                                              iduMemory  * aMemory );

    // AfterPVO
    // qcTemplate ������ �ʱ�ȭ
    static IDE_RC initTemplateMember( qcTemplate * aTemplate,
                                      iduMemory  * aMemory);

    // at CLEAR, CLOSE, AfterPXX
    // qcStatement ������ �ʱ�ȭ
    static void resetTemplateMember( qcStatement * aStatement );

    // at CLEAR, CLOSE, AfterPXX
    // Tuple Set���� ������ �ʱ�ȭ
    static void resetTupleSet( qcTemplate * aTemplate );

    // PROJ-1358
    // ���� �ڵ� Ȯ��� ���� �ִ� Internal Tuple ������ �Ҵ�޴´�.
    static IDE_RC allocInternalTuple( qcTemplate * aTemplate,
                                      iduMemory  * aMemory,
                                      UShort       aTupleCnt );

    // PROJ-1436
    static IDE_RC allocInternalTuple( qcTemplate    * aTemplate,
                                      iduVarMemList * aMemory,
                                      UShort          aTupleCnt );

    // PROJ-2163
    static IDE_RC calculateVariableTupleSize( qcStatement * aStatement,
                                              UInt        * aSize,
                                              UInt        * aOffset,
                                              UInt        * aTupleSize );

    /*
     * PROJ-1071 Parallel Query
     */
    static IDE_RC initPrlThrUseCnt();
    static IDE_RC finiPrlThrUseCnt();

    static IDE_RC allocAndCloneTemplateMember( iduMemory * aMemory,
                                               qcTemplate* aSource,
                                               qcTemplate* aDestination );

    static void cloneTemplateMember( qcTemplate * aSource,
                                     qcTemplate * aDestination );

    // PROJ-2415 Grouping Set Clause 
    static IDE_RC allocStmtListMgr( qcStatement * aStatement );
    static void clearStmtListMgr( qcStmtListMgr * aStmtListMgr );    

    /* PROJ-2451 Concurrent Execute Package */
    static IDE_RC initConcThrUseCnt();
    static IDE_RC finiConcThrUseCnt();
};



#endif /* _O_QCG_H_ */
