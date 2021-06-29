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
 * $Id$
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <qcg.h>
#include <qcd.h>
#include <qdsd.h>
#include <sdi.h>
#include <sdl.h>
#include <sdm.h>
#include <sdiZookeeper.h>
#include <qcmUser.h>

IDE_RC qdsd::validateShardAdd( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD ADD
 *
 * Implementation :
 *    1. SYS, SYSTEM ����� üũ
 ***********************************************************************/
    idBool              sIsValid = ID_FALSE;
    idBool              sIsConnect = ID_FALSE;
    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;
    SChar               sShardVersion[20] = {0,};
    SChar               sTransTBLSize[10] = {0,};
    sdiDatabaseInfo     sShardDBInfo;
    sdiLocalMetaInfo    sLocalMetaInfo;
    idBool sIsAlreadyExistZKSMeta = ID_FALSE;
    SChar  sPath[SDI_ZKC_PATH_LENGTH] = SDI_ZKC_PATH_ALTIBASE_SHARD;

    qciUserInfo         sUserInfo;
    void *              sMmSession = aStatement->session->mMmSession;
    idBool              sIsKSafetyNULL = ID_FALSE;
    SChar               sKSafetyStr[SDI_KSAFETY_STR_MAX_SIZE + 1] = {0,};
    SChar               sReplicationModeStr[SDI_REPLICATION_MODE_MAX_SIZE + 1] = {0,};
    SChar               sParallelCountStr[SDI_PARALLEL_COUNT_STR_MAX_SIZE + 1] = {0,};

    qci::mSessionCallback.mGetUserInfo( sMmSession, (void *)&sUserInfo );

    //    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    /***********************************************************************
     * validateCommon
     ***********************************************************************/

    IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );

    /* BUG-48770 �ٸ� session�� �ְų� shard cli�� ������ ��쿡�� shard DDL�� ������ �� ����.*/
    if( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        IDE_TEST_RAISE( qci::mSessionCallback.mCheckSessionCount() > 1, ERR_TOO_MANY_SESSION );
    }

    IDE_TEST_RAISE( qci::mSessionCallback.mIsShardClient( aStatement->session->mMmSession )
                    == ID_TRUE,
                    ERR_SHARD_CLI );

    /***********************************************************************
     * validate SHARD ADD
     ***********************************************************************/

    /***********************************************************************
     * Zookeeper validation
     ***********************************************************************/

    /* valiation�� �ʿ��� ������ �����´�. */
    sShardDBInfo = sdi::getShardDBInfo();

    idlOS::snprintf( sShardVersion, 20, "%"ID_INT32_FMT".%"ID_INT32_FMT".%"ID_INT32_FMT,
                    sShardDBInfo.mMajorVersion, sShardDBInfo.mMinorVersion, sShardDBInfo.mPatchVersion );
    idlOS::snprintf( sTransTBLSize, 10, "%"ID_UINT32_FMT, sShardDBInfo.mTransTblSize );

    IDE_TEST( sdi::getLocalMetaInfoAndCheckKSafety( &sLocalMetaInfo, &sIsKSafetyNULL ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', INVALID_LOCAL_NODE_NAME );
    sdiZookeeper::initialize( sLocalMetaInfo.mNodeName,
                              &sUserInfo );

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );

    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS );
    sIsConnect = ID_TRUE;


    /* Ŭ������ ��Ÿ�� �����Ǿ��ִ��� Ȯ���Ѵ�. */
    IDE_TEST( sdiZookeeper::checkPath( sPath,
                                       &sIsAlreadyExistZKSMeta ) != IDE_SUCCESS );

    /* zookeeper server�� Ŭ������ ��Ÿ�� �����Ǿ����� �ʴٸ� �����Ѵ�. */
    if( sIsAlreadyExistZKSMeta == ID_FALSE )
    {
        IDE_TEST_RAISE( sIsKSafetyNULL == ID_TRUE, ERR_INVALID_KSAFETY );
        /* zookeeper server�� Ŭ������ ��Ÿ�� �����Ѵ�. */
        IDE_TEST( sdiZookeeper::initClusterMeta( sShardDBInfo,
                                                 sLocalMetaInfo ) != IDE_SUCCESS );
    }

    /* ���� ��Ÿ�� �����ϹǷ� zookeeper�� ���� ��Ÿ lock�� ��´�. */
    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    sIsAcquireZookeeperMetaLock = ID_TRUE;

    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );
    sIsAcquireShardMetaLock = ID_TRUE;

    /* zookeeper server�� �Ͱ� ��� ��Ÿ(DB�̸�,���̳ʸ� ���� ��)�� ��ġ�ϴ��� üũ�Ѵ�. */

    /* replication ������ ���õ��� �ʾ��� ��쿡�� �ش� ������ ���� üũ�� ���� �ʰ� *
     * zookeeper server�� ���� �޾ƿ� �����Ѵ�. at execute                    */
    if (sIsAlreadyExistZKSMeta == ID_TRUE)
    {
        if( sIsKSafetyNULL == ID_TRUE )
        {
            /* replication ������ �����ϰ� �� */
            IDE_TEST( sdiZookeeper::validateNode( sShardDBInfo.mDBName,
                                                sShardDBInfo.mDBCharSet,
                                                sShardDBInfo.mNationalCharSet,
                                                NULL, /* k-safety */
                                                NULL, /* replication mode */
                                                NULL, /* parallel count */
                                                sShardDBInfo.mDBVersion,
                                                sShardVersion,
                                                sTransTBLSize,
                                                sLocalMetaInfo.mNodeName ) != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST_RAISE( idlOS::uIntToStr( sLocalMetaInfo.mKSafety,
                                              sKSafetyStr,
                                              SDI_KSAFETY_STR_MAX_SIZE + 1) != 0, ERR_INVALID_KSAFETY );
            IDE_TEST_RAISE( idlOS::uIntToStr( sLocalMetaInfo.mReplicationMode,
                                              sReplicationModeStr,
                                              SDI_REPLICATION_MODE_MAX_SIZE +1 ) != 0, ERR_INVALID_KSAFETY );
            IDE_TEST_RAISE( idlOS::uIntToStr( sLocalMetaInfo.mParallelCount,
                                              sParallelCountStr,
                                              SDI_PARALLEL_COUNT_STR_MAX_SIZE +1) != 0, ERR_INVALID_KSAFETY );

            IDE_TEST( sdiZookeeper::validateNode( sShardDBInfo.mDBName,
                                                  sShardDBInfo.mDBCharSet,
                                                  sShardDBInfo.mNationalCharSet,
                                                  sKSafetyStr, /* k-safety */
                                                  sReplicationModeStr, /* replication mode */
                                                  sParallelCountStr, /* parallel count */
                                                  sShardDBInfo.mDBVersion,
                                                  sShardVersion,
                                                  sTransTBLSize,
                                                  sLocalMetaInfo.mNodeName ) != IDE_SUCCESS );

        }
    }
    else
    {
        IDE_TEST_RAISE( sIsKSafetyNULL == ID_TRUE, ERR_INVALID_KSAFETY ); /* already checked, for enhance readablility */
    }

    /* zookeeper server�� �׾��ִ� ��尡 �ִ��� üũ�Ѵ�. */
    IDE_TEST( sdiZookeeper::checkAllNodeAlive( &sIsValid ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sIsValid == ID_FALSE, deadNode_exist );
    
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }

    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    sdiZookeeper::disconnect();
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_MANY_SESSION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_SESSION, "add" ) );
    }
    IDE_EXCEPTION( ERR_SHARD_CLI )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_DDL_TRY_WITH_SHARD_CLI ) );
    }
    IDE_EXCEPTION( ERR_INVALID_KSAFETY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_KSAFETY ) );
    }
    IDE_EXCEPTION( INVALID_LOCAL_NODE_NAME )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_LOCAL_NODE_NAME ) );
    }
    IDE_EXCEPTION( deadNode_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_DEADNODE_EXIST ) );
    }
    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }

    IDE_EXCEPTION_END;

    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }

    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    if( sIsConnect == ID_TRUE )
    {
        sdiZookeeper::disconnect();
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardAdd( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD ADD
 *
 * Implementation :
 *    1. zks meta�� ��� ���� add �� Ÿ ��� ���� get
 *    2. ���� ��Ÿ ���� (from other node)
 *       Internal Replication - add Temporary replication
 *    3. set_node ����
 ***********************************************************************/
    qdShardParseTree  * sParseTree = NULL;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    /***********************************************************************
     * check internal property validate
     ***********************************************************************/
    IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );

    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS );

    /* for smn propagation to all nodes at commit */
    sdi::setShardMetaTouched( aStatement->session );

    IDE_TEST( checkShardPIN( aStatement ) != IDE_SUCCESS );

    sdi::setInternalTableSwap( aStatement->session );

    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );
    
    /***********************************************************************
     * Zookeeper meta 1�� ����
     ***********************************************************************/
    IDE_TEST( executeZookeeperAdd() != IDE_SUCCESS );

    /***********************************************************************
     * SHARD META CHECK RESET
     ***********************************************************************/
    // R2HA SMN�� reset���� �ʴ´�. ���� ��å Ȥ�� �ʿ� �� �߰�
    IDE_TEST( resetShardMeta( aStatement ) != IDE_SUCCESS);

    /***********************************************************************
     * synchronize k-safety info with zookeeper meta
     ***********************************************************************/
    IDE_TEST( synchronizeKSafetyInfo(aStatement) != IDE_SUCCESS );

    /***********************************************************************
     * ���� ��Ÿ ����
     ***********************************************************************/
    
    IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );
    
    IDE_TEST( executeTemporaryReplication( aStatement ) != IDE_SUCCESS );

    /***********************************************************************
     * check SMN
     ***********************************************************************/
    if ( sParseTree->mNodeCount > 1 )
    {
        IDE_TEST( checkZookeeperSMNAndDataSMN() != IDE_SUCCESS );
    }
    
    /***********************************************************************
     * SET_NODE ����
     ***********************************************************************/
    IDE_TEST( executeSetNodes( aStatement ) != IDE_SUCCESS );

    /***********************************************************************
     * FAILOVER ó���� ���� üũ�� ����  REPLICA_SETS_ ��������
     * replication stop
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_STOP )
              != IDE_SUCCESS );

    /***********************************************************************
     * replication drop
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_DROP )
              != IDE_SUCCESS );

    /***********************************************************************
     * backup table no item create replication
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_CREATE )
              != IDE_SUCCESS );
        
    /***********************************************************************
     * backup table add replication
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_ADD )
              != IDE_SUCCESS );

    /***********************************************************************
     * backup table truncate
     ***********************************************************************/
    IDE_TEST( truncateBackupTable( aStatement ) != IDE_SUCCESS );

    /***********************************************************************
     * replication sync start
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_START )
              != IDE_SUCCESS );

    /***********************************************************************
     * replication flush
     ***********************************************************************/
    IDE_TEST( executeReplicationFunction( aStatement,
                                          QDSD_REPL_QUERY_TYPE_FLUSH )
              != IDE_SUCCESS );

    /***********************************************************************
     * TASK-7307 DML Data Consistency in Shard
     *   SYSTEM_.SYS_TABLES_.SHARD_FLAG ������Ʈ
     ***********************************************************************/
    IDE_TEST( executeAlterShardFlag( aStatement ) != IDE_SUCCESS );
    
    /***********************************************************************
     * execute add clone
     ***********************************************************************/
    IDE_TEST( executeShardAddForClone( aStatement ) != IDE_SUCCESS );

    sdi::loadShardPinInfo();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }
    IDE_EXCEPTION_END;
    
    sdi::unsetInternalTableSwap( aStatement->session );

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();
    
    return IDE_FAILURE;
}

IDE_RC qdsd::synchronizeKSafetyInfo( qcStatement * aStatement )
{
    sdiLocalMetaInfo    sLocalMetaInfo;
    idBool              sIsKSafetyNULL = ID_FALSE;
    SChar               sKSafetyStr[SDI_KSAFETY_STR_MAX_SIZE + 1] = {0,};
    SChar               sReplicationModeStr[SDI_REPLICATION_MODE_MAX_SIZE + 1] = {0,};
    SChar               sParallelCountStr[SDI_PARALLEL_COUNT_STR_MAX_SIZE + 1] = {0,};
    UInt                sRowCnt = 0;

    IDE_TEST( sdi::getLocalMetaInfoAndCheckKSafety( &sLocalMetaInfo, &sIsKSafetyNULL ) != IDE_SUCCESS );

    if ( sIsKSafetyNULL == ID_TRUE )
    {
        IDE_TEST( sdiZookeeper::getKSafetyInfo( sKSafetyStr,
                                                sReplicationModeStr,
                                                sParallelCountStr ) != IDE_SUCCESS );

        IDE_TEST( sdi::updateLocalMetaInfoForReplication( aStatement,
                                                          idlOS::strToUInt((UChar*)sKSafetyStr,idlOS::strlen(sKSafetyStr)),
                                                          sReplicationModeStr,
                                                          idlOS::strToUInt((UChar*)sParallelCountStr,idlOS::strlen(sParallelCountStr)),
                                                         &sRowCnt ) 
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sRowCnt != 1, ERR_META_CRASH );
    }
    else
    {
        /* do nothing */
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_META_CRASH ) );
    }
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC qdsd::validateShardJoin( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD JOIN
 *
 * Implementation :
 *    1.
 ***********************************************************************/

    idBool              sIsConnect = ID_FALSE;
    sdiLocalMetaInfo    sLocalMetaInfo;
    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;
    qciUserInfo         sUserInfo;
    void *              sMmSession = aStatement->session->mMmSession;
        
    qci::mSessionCallback.mGetUserInfo( sMmSession, &sUserInfo );

    /***********************************************************************
     * validateCommon
     ***********************************************************************/

    IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );

    /* BUG-48770 �ٸ� session�� �ְų� shard cli�� ������ ��쿡�� shard DDL�� ������ �� ����.*/
    if( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        IDE_TEST_RAISE( qci::mSessionCallback.mCheckSessionCount() > 1, ERR_TOO_MANY_SESSION );
    }

    IDE_TEST_RAISE( qci::mSessionCallback.mIsShardClient( aStatement->session->mMmSession )
                    == ID_TRUE,
                    ERR_SHARD_CLI );

    /***********************************************************************
     * Zookeeper validation
     ***********************************************************************/

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );

    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', no_meta_exist );

    sdiZookeeper::initialize( sLocalMetaInfo.mNodeName,
                              &sUserInfo );
    IDE_TEST( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS );
    sIsConnect = ID_TRUE;

    /* ���� ��Ÿ�� �����ϹǷ� zookeeper�� ���� ��Ÿ lock�� ��´�. */
    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    sIsAcquireZookeeperMetaLock = ID_TRUE;
    
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );
    sIsAcquireShardMetaLock = ID_TRUE;

    /* ���� ����� ���°� JOIN�� ������ �������� üũ�Ѵ�. */
    IDE_TEST( sdiZookeeper::validateState( ZK_JOIN,
                                           sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );
    
    /***********************************************************************
     * check SMN
     ***********************************************************************/
//    IDE_TEST( checkZookeeperSMNAndDataSMN() != IDE_SUCCESS );
    
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }
    
    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }
    
    sdiZookeeper::disconnect();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_MANY_SESSION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_SESSION, "join" ) );
    }
    IDE_EXCEPTION( ERR_SHARD_CLI )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_DDL_TRY_WITH_SHARD_CLI ) );
    }
    IDE_EXCEPTION( no_meta_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }

    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    if( sIsConnect == ID_TRUE )
    {
        sdiZookeeper::disconnect();
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardJoin( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD JOIN
 *
 * Implementation :
 *    1. 
 ***********************************************************************/

    SChar               sGetShardMetaFromNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    ULong               sOldSMN = 0;
    ULong               sZKSMN  = 0;

    sdiReplicaSetInfo   sReplicaSetInfo;
    sdiClientInfo     * sClientInfo  = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    iduList           * sList;
    idBool              sIsAllocList = ID_FALSE;
    
    /***********************************************************************
     * check internal property validate
     ***********************************************************************/
    IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );

    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS );

    /* for smn propagation to all nodes at commit */
    sdi::setShardMetaTouched( aStatement->session );

    IDE_TEST( checkShardPIN( aStatement ) != IDE_SUCCESS );

    sdi::setInternalTableSwap( aStatement->session );

    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );

    IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );
    
    /***********************************************************************
     * Zookeeper meta 1�� ����
     ***********************************************************************/
    IDE_TEST( executeZookeeperJoin() != IDE_SUCCESS );
    
    IDE_TEST( sdiZookeeper::getZookeeperSMN( & sZKSMN ) != IDE_SUCCESS );
    sOldSMN = sdi::getSMNForDataNode();

    /* Node�� Zookeeper�� SMN�� ������ Join, �ٸ��� JOIN_SMN_MODIFIED �̴�. */
    if ( sOldSMN != sZKSMN )
    {
        /* SMN�� �ٸ��� Alive Node�� ���� Shard Meta�� �����´�. */
        IDE_TEST( sdiZookeeper::getNextAliveNode( sdiZookeeper::mMyNodeName,
                                                  sGetShardMetaFromNodeName ) != IDE_SUCCESS );

        IDE_TEST( resetShardMeta( aStatement ) != IDE_SUCCESS);
        IDE_TEST( getShardMetaFromOtherNode( aStatement,
                                             sGetShardMetaFromNodeName ) != IDE_SUCCESS );

        sOldSMN = sdi::getSMNForDataNode();
    }

    /* get Alive Node List + LocalNode �ʿ� */
    IDE_TEST( sdiZookeeper::getAliveNodeNameListIncludeNodeName( &sList,
                                                                 sdiZookeeper::mMyNodeName ) 
              != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    /* NodeList ������� ShardLinker �����ؾ� �Ѵ�. */
    IDE_TEST( sdi::checkShardLinkerWithNodeList ( aStatement,
                                                  sOldSMN,
                                                  NULL,
                                                  sList ) != IDE_SUCCESS );

    sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

    IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );

    /* get ReplicaSet Info */
    IDE_TEST( sdm::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sOldSMN )
              != IDE_SUCCESS );

    /* JOIN�� �׻� SMN Update�� �߻��ؾ� �Ѵ�. */
    idlOS::snprintf( sSqlStr, 
                     QD_MAX_SQL_LENGTH,
                     "EXEC DBMS_SHARD.TOUCH_META(); " );

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_NORMAL,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  NULL,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Touch Meta Complete");

    /* RP Recover */
    IDE_TEST( joinRecoverRP( aStatement,
                             &sReplicaSetInfo )
              != IDE_SUCCESS );

    sIsAllocList = ID_FALSE;
    sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );

    sdi::loadShardPinInfo();

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }
    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    
    (void)executeReplicationForJoin( aStatement, ID_TRUE );

    IDE_POP();

    if( sIsAllocList == ID_TRUE )
    {
        sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    sdi::unsetInternalTableSwap( aStatement->session );

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();

    return IDE_FAILURE;
}


IDE_RC qdsd::validateShardDrop( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD DROP {'node_name'} [ FORCE ] ;
 *
 * Implementation :
 *    1.
 ***********************************************************************/

    qdShardParseTree  * sParseTree = NULL;
    idBool              sIsValid = ID_FALSE;
    SChar               sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar               sNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;
    
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    /***********************************************************************
     * validateCommon
     ***********************************************************************/

    IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );
    
    /***********************************************************************
     * Zookeeper validation
     ***********************************************************************/

    /* zookeeper server�� ������ �Ǿ� �ִ� �������� üũ�Ѵ�. */
    IDE_TEST_RAISE( sdiZookeeper::isConnect() != ID_TRUE, connect_Fail );

    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    sIsAcquireZookeeperMetaLock = ID_TRUE;

    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() )
              != IDE_SUCCESS );
    sIsAcquireShardMetaLock = ID_TRUE;

    // DROP �߿� �̹� �ٸ� ��忡�� ��尡 ���ŵ� ��� SMN���� ��ũ �Ѵ�.
    IDE_TEST( sdi::validateSMNForShardDDL(aStatement )
              != IDE_SUCCESS );
    
    /***********************************************************************
     * check SMN
     ***********************************************************************/
    IDE_TEST( checkZookeeperSMNAndDataSMN() != IDE_SUCCESS );

    /* force�� �ƴ� ��� �׾��ִ� ��尡 �ִ��� üũ�Ѵ�. */
    if( sParseTree->mDDLType != SHARD_DROP_FORCE )
    {
        IDE_TEST( sdiZookeeper::checkAllNodeAlive( &sIsValid ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIsValid == ID_FALSE, deadNode_exist );

        /* DROP Force �� ��� Reference Check�� SKIP �ϵ��� �Ѵ�. ������ ������ �ϴϱ�. */
        /* �⺻������ ���� ��� ��尡 �����ϴ��� Ȯ���ϴ� �۾��� validateState �Լ����� ó���ϳ�
         * ADD/DROP ������ validateState�� �������� �����Ƿ� ���� üũ�ؾ� �Ѵ�. */
        if( QC_IS_NULL_NAME( sParseTree->mNodeName ) == ID_FALSE )
        {
            QC_STR_COPY( sNodeName, sParseTree->mNodeName );

            idlOS::strncpy( sPath,
                            SDI_ZKC_PATH_NODE_META,
                            SDI_ZKC_PATH_LENGTH );
            idlOS::strncat( sPath,
                            "/",
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            idlOS::strncat( sPath,
                            sNodeName,
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );

            IDE_TEST( sdiZookeeper::checkPath( sPath,
                                               &sIsValid ) != IDE_SUCCESS );

            IDE_TEST_RAISE( sIsValid == ID_FALSE, invalid_nodeName );
        }
        else
        {
            // nothing to do
        }
    }
    
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }

    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( connect_Fail )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_CONNECTION_FAIL ) );
    }
    IDE_EXCEPTION( deadNode_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_DEADNODE_EXIST ) );
    }
    IDE_EXCEPTION( invalid_nodeName )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_NODE_NAME, sNodeName ) );
    }
    IDE_EXCEPTION_END;

    // the logic below must be move to commit or rollback.
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }

    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardDrop( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD DROP {'node_name'} [ FORCE ] ;
 *
 * Implementation :
 *    1. 
 ***********************************************************************/

    qdShardParseTree  * sParseTree = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    SChar               sNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    sdiLocalMetaInfo    sLocalMetaInfo;
    idBool              sIsRemoteNode = ID_FALSE;
        
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    if( sParseTree->mDDLType == SHARD_DROP )
    {
        /***********************************************************************
         * check internal property validate
         ***********************************************************************/
        IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

        /* for smn propagation to all nodes at commit */
        sdi::setShardMetaTouched( aStatement->session );
        IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
        IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );

        IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );

        /***********************************************************************
         * check node_name
         ***********************************************************************/
        IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

        if ( QC_IS_NULL_NAME( sParseTree->mNodeName ) != ID_TRUE )
        {
            QC_STR_COPY( sNodeName, sParseTree->mNodeName );

            if ( idlOS::strncmp( sNodeName,
                                 sLocalMetaInfo.mNodeName,
                                 ( SDI_NODE_NAME_MAX_SIZE + 1 ) ) != 0 )
            {
                sIsRemoteNode = ID_TRUE;
            }
        }
        else
        {
            // nothing to do
        }

        if ( sIsRemoteNode == ID_TRUE )
        {
            
            /***********************************************************************
             * SHARD DROP node_name
             ***********************************************************************/
            IDE_RAISE(ERR_NOT_SUPPORTED_SYNTAX);
            sdiZookeeper::releaseShardMetaLock();
            sdiZookeeper::releaseZookeeperMetaLock();

            aStatement->session->mQPSpecific.mFlag &= ~QC_SESSION_HANDOVER_SHARD_DDL_MASK;
            aStatement->session->mQPSpecific.mFlag |= QC_SESSION_HANDOVER_SHARD_DDL_TRUE;

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "EXEC DBMS_SHARD.EXECUTE_IMMEDIATE( 'ALTER DATABASE SHARD DROP', '"QCM_SQL_STRING_SKIP_FMT"' ) ",
                             sNodeName );

            IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                              sSqlStr )
                      != IDE_SUCCESS );
        }
        else
        {
            /***********************************************************************
             * SHARD DROP
             ***********************************************************************/

            /***********************************************************************
             * RESET CLONES_ META
             ***********************************************************************/
            IDE_TEST( executeResetCloneMeta( aStatement ) != IDE_SUCCESS );
    
            /***********************************************************************
             * UNSET_NODE ����
             ***********************************************************************/
            IDE_TEST( executeUnsetNodes( aStatement ) != IDE_SUCCESS );
            
            /***********************************************************************
             * FAILOVER ó���� ���� üũ�� ����  REPLICA_SETS_ ��������
             * replication stop
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_STOP )
                      != IDE_SUCCESS );
            
            /***********************************************************************
             * replication drop
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_DROP )
                      != IDE_SUCCESS );

            /***********************************************************************
             * backup table no item create replication
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_CREATE )
                      != IDE_SUCCESS );

            /***********************************************************************
             * backup table add replication
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_ADD )
                      != IDE_SUCCESS );

            /***********************************************************************
             * replication sync start
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_START )
                      != IDE_SUCCESS );
            
            /***********************************************************************
             * replication flush
             ***********************************************************************/
            IDE_TEST( executeReplicationFunction( aStatement,
                                                  QDSD_REPL_QUERY_TYPE_FLUSH )
                      != IDE_SUCCESS );

            /***********************************************************************
             * TASK-7307 DML Data Consistency in Shard
             *   SYSTEM_.SYS_TABLES_.SHARD_FLAG ������Ʈ
             ***********************************************************************/
            IDE_TEST( executeAlterShardNone( aStatement ) != IDE_SUCCESS );
        }
    }
    else
    {
        /* Drop force�� ����ϸ� ���� �� ���ϰ� �׳� zookeeper�� �����ϵ��� �ص�. */
        IDE_DASSERT( sParseTree->mDDLType == SHARD_DROP_FORCE );
    }

    if (( aStatement->session->mQPSpecific.mFlag & QC_SESSION_HANDOVER_SHARD_DDL_MASK ) == 
        QC_SESSION_HANDOVER_SHARD_DDL_FALSE )
    {
        IDE_DASSERT( sIsRemoteNode != ID_TRUE );
        
        /***********************************************************************
         * Zookeeper meta 1�� ����
         ***********************************************************************/
        IDE_TEST( executeZookeeperDrop( aStatement ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_SUPPORTED_SYNTAX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SYNTAX_ERROR_SHARD_DROP, sNodeName) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


IDE_RC qdsd::validateShardFailover( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD FAILOVER [ NORMAL | IMMEDIATE | EXIT | FORCE ] 'node_name';
 *
 * Implementation :
 *    1.
 ***********************************************************************/

    qdShardParseTree  * sParseTree = NULL;
    SChar               sNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    SChar               sNextNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    SChar               sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    idBool              sIsValid = ID_FALSE;

    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;
    sdiLocalMetaInfo    sLocalMetaInfo;


    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    /***********************************************************************
     * validateCommon
     ***********************************************************************/

    IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );

    /* ���� ��Ÿ�� �����ϹǷ� zookeeper�� ���� ��Ÿ lock�� ��´�. */
    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    sIsAcquireZookeeperMetaLock = ID_TRUE;
    
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );
    sIsAcquireShardMetaLock = ID_TRUE;

    /***********************************************************************
     * validate SHARD FAILOVER
     ***********************************************************************/

    /***********************************************************************
     * Zookeeper validation
     ***********************************************************************/

    /* zookeeper server�� ������ �Ǿ� �ִ� �������� üũ�Ѵ�. */
    IDE_TEST_RAISE( sdiZookeeper::isConnect() != ID_TRUE, connect_Fail );

    if( QC_IS_NULL_NAME( sParseTree->mNodeName ) == ID_FALSE )
    {
        /* �Է¹��� Node ���� ���� �� Node�� ������ Ȯ���Ѵ�.*/
        IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

        QC_STR_COPY( sNodeName, sParseTree->mNodeName );

        if ( idlOS::strncmp( sNodeName,
                             sLocalMetaInfo.mNodeName,
                             ( SDI_NODE_NAME_MAX_SIZE + 1 ) ) == 0 )
        {
            /* �� ��带 ���� ���� FailOver�� ���� �ʵ��� �Ѵ�.
             * Node Stop�� �ؾߵǰ� Commit�� ���־�� �ϴϱ�. */
            IDE_RAISE( same_nodeName );
        }

        /* �ٸ� ����� �̸��� �Է¹��� ��� ��� ��尡 �����ϴ��� Ȯ���Ѵ�. */
        idlOS::strncpy( sPath,
                        SDI_ZKC_PATH_NODE_META,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sPath,
                        "/",
                        ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
        idlOS::strncat( sPath,
                        sNodeName,
                        ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );

        IDE_TEST( sdiZookeeper::checkPath( sPath,
                                           &sIsValid ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sIsValid == ID_FALSE, invalid_nodeName );
    }
    else
    {
        /* Failover Target NodeName�� �׻� ���;� �Ѵ�. */
        IDE_RAISE( invalid_nodeName );
    }

    /* Failover�� ������ Node�� �̸��� ���� �´�. */
    IDE_TEST( sdiZookeeper::getNextAliveNode( sNodeName,
                                              sNextNodeName ) != IDE_SUCCESS );


    /* Failover ������ Node�� ���� Ȯ�� */
    IDE_TEST( sdiZookeeper::validateState( ZK_FAILOVER,
                                           sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );

    IDE_TEST( sdiZookeeper::validateState( ZK_FAILOVER,
                                           sNextNodeName ) != IDE_SUCCESS );

    /***********************************************************************
     * check SMN
     ***********************************************************************/
    IDE_TEST( checkZookeeperSMNAndDataSMN() != IDE_SUCCESS );
    
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }
    
    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( connect_Fail )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_CONNECTION_FAIL ) );
    }
    IDE_EXCEPTION( invalid_nodeName )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_NODE_NAME, sNodeName ) );
    }
    IDE_EXCEPTION( same_nodeName )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_FAILOVER_NODE_NAME, sNodeName ) );
    }

    IDE_EXCEPTION_END;

    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }
    
    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardFailover( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD FAILOVER [ NORMAL | IMMEDIATE | EXIT | FORCE ] 'node_name';
 *
 * Implementation :
 *    1. 
 ***********************************************************************/

    qdShardParseTree  * sParseTree = NULL;
    idBool              sIsTargetAlive = ID_FALSE;

    SChar             * sUserName;
    SChar               sTargetNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    SChar               sFailoverNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar               sFailoverNextNodeName[SDI_NODE_NAME_MAX_SIZE + 1];

    SChar               sFirstStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    SChar               sSecondStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];

    SChar               sFirstStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
    SChar               sSecondStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];

    SChar               sReverseReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
    SChar               sFailoverCreatedReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];

    sdiClientInfo     * sClientInfo  = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    ULong               sOldSMN = 0;

    iduList           * sList;
    idBool              sIsAllocList = ID_FALSE;

    UInt                i;

    sdiReplicaSetInfo   sReplicaSetInfo;

    sdiReplicaSet     * sOldNodeReplicaSet = NULL;

    sdiReplicaSet     * sFirstToOldReplicaSet = NULL;
    sdiReplicaSet     * sSecondToOldReplicaSet = NULL;

    sdiNode             sOldNodeInfo;
    sdiNode             sNewNodeInfo;

    sdiTableInfoList  * sTableInfoList = NULL;

    sdiLocalMetaInfo  * sFailoverNextLocalMetaInfo = NULL;
    sdiLocalMetaInfo  * sNodeMetaInfo = NULL;

    SInt                sDataLength;
    SChar               sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    ZKState             sState = ZK_NONE;

    idBool              sIsUnsetNode = ID_TRUE;

    UInt                sExecCount = 0;
    ULong               sCount = 0;
    idBool              sFetchResult = ID_FALSE;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    sUserName = QCG_GET_SESSION_USER_NAME( aStatement );
    QC_STR_COPY( sTargetNodeName, sParseTree->mNodeName );

    /***********************************************************************
     * check internal property validate
     ***********************************************************************/
    IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

    /* for smn propagation to all nodes at commit */
    sdi::setShardMetaTouched( aStatement->session );

    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );

    /* Meta Lock ȹ�� �� Failover Check */
    /* Meta Lock ȹ�� �ϴ� ���� �ٸ� Node�� Failover�� �����Ͽ����� Ȯ�� �Ѵ�. */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    IDE_TEST( sdiZookeeper::getNodeInfo( sTargetNodeName,
                                         (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                         sBuffer,
                                         &sDataLength ) != IDE_SUCCESS );

    if ( sDataLength > 0 )
    {
        /* �̹� failover�� �Ǿ��ִ� �����̹Ƿ� ���� �۾��� �ʿ䰡 ����. */
        ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Already Failed over To %s\n", sBuffer );

        IDE_CONT( normal_exit );
    }

    /* Failover�� ������ Node�� �̸��� ���� �´�. */
    IDE_TEST( sdiZookeeper::getNextAliveNode( sTargetNodeName,
                                              sFailoverNodeName ) != IDE_SUCCESS );

    /* Failover�� ������ ����� ���¸� Ȯ�� */
    IDE_TEST( sdiZookeeper::validateState( ZK_FAILOVER,
                                           sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );

    IDE_TEST( sdiZookeeper::validateState( ZK_FAILOVER,
                                           sFailoverNodeName ) != IDE_SUCCESS );


    /* Failover ���� */
    /***********************************************************************
     * Zookeeper meta 1�� ����
     ***********************************************************************/
    IDE_TEST( executeZookeeperFailover( sTargetNodeName,
                                        sFailoverNodeName,
                                        &sIsTargetAlive ) != IDE_SUCCESS );

    if ( sIsTargetAlive == ID_TRUE )
    {
        /* ��� ��尡 ��� ������ KILL �ϰ� �ٸ� Node�� Failover�� �����Ѵ�. */
        // Set Zookeeper TargetNode State = KILL
        // Zookeeper TargetNode Connection Info remove
        IDE_TEST( sdiZookeeper::killNode( sTargetNodeName ) != IDE_SUCCESS );
    }

    /* get SMN */
    sOldSMN = sdi::getSMNForDataNode();

    /* get Alive Node List */
    IDE_TEST( sdiZookeeper::getAliveNodeNameList( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    /* AliveNode�鸸 ������� ShardLinker �����ؾ� �Ѵ�. Node Stop �Ŀ� ȣ��Ǿ�� ��. */
    IDE_TEST( sdi::checkShardLinkerWithNodeList ( aStatement,
                                                  sOldSMN,
                                                  NULL,
                                                  sList ) != IDE_SUCCESS );

    sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

    IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );

    /* get ReplicaSet Info */
    IDE_TEST( sdm::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sOldSMN )
              != IDE_SUCCESS );

    /* get OldNode Info */
    IDE_TEST( sdi::getNodeByName( QC_SMI_STMT( aStatement ),
                                  sTargetNodeName,
                                  sOldSMN,
                                  &sOldNodeInfo )
              != IDE_SUCCESS );

    /* get NewNode Info */
    IDE_TEST( sdi::getNodeByName( QC_SMI_STMT( aStatement ),
                                  sFailoverNodeName,
                                  sOldSMN,
                                  &sNewNodeInfo )
              != IDE_SUCCESS );

    /* get All TableInfo */
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sOldSMN )
              != IDE_SUCCESS );

    /* FailoverNode Receiver Failover */
    /*
    for ( i = 0; i < sReplicaSetInfo.mCount; i++ )
    {
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mFirstBackupNodeName,
                             sFailoverNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION %s FAILOVER ", 
                             sReplicaSetInfo.mReplicaSets[i].mFirstReplName );

            IDE_TEST ( executeRemoteSQL( aStatement,
                                         sFailoverNodeName,
                                         SDI_INTERNAL_OP_FAILOVER,
                                         QCI_STMT_MASK_DCL,
                                         sSqlStr,
                                         QD_MAX_SQL_LENGTH,
                                         NULL ) 
                       != IDE_SUCCESS );
        }

        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mSecondBackupNodeName,
                             sFailoverNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION %s FAILOVER ", 
                             sReplicaSetInfo.mReplicaSets[i].mSecondReplName );

            IDE_TEST ( executeRemoteSQL( aStatement,
                                         sFailoverNodeName,
                                         SDI_INTERNAL_OP_FAILOVER,
                                         QCI_STMT_MASK_DCL,
                                         sSqlStr,
                                         QD_MAX_SQL_LENGTH,
                                         NULL ) 
                       != IDE_SUCCESS );
        }
    }
    ideLog::log(IDE_SD_21, "[SHARD_FAILOVER] Replication failover success" );
    */

    /* Find ReplicaSets */
    for ( i = 0 ; i < sReplicaSetInfo.mCount; i++ )
    {
        /* TargetNode�� ���� Failover ������ ReplicaSet�� ã�´�.
         * 2�� �̻� ������ ������ �����Ƿ� Prev�� ���� ã�´�. */
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mPrimaryNodeName,
                             sTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mFirstBackupNodeName,
                                 sFailoverNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                sOldNodeReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
            }
        }
        /* First/Second Backup == TargetNode �ΰ��� 1�� ���̴�. */
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mFirstBackupNodeName,
                             sTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            sFirstToOldReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
        }
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mSecondBackupNodeName,
                             sTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            sSecondToOldReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
        }
    }

    if ( sOldNodeReplicaSet == NULL )
    {
        /* Prev ReplicaSet�� ��ã������ PrevPrev�� ã�´�. 
         * ��ã���� Failover ������ ReplicaSet�� ���� ��. */
        for ( i = 0 ; i < sReplicaSetInfo.mCount; i++ )
        {
            if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mPrimaryNodeName,
                                 sTargetNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mSecondBackupNodeName,
                                     sFailoverNodeName,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
                {
                    sOldNodeReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
                }
            }
        }
    }

    IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Backup ReplicaSet Start");

    /***********************************************************************
     * FAILOVER ���� �� ����
     ***********************************************************************/
    /* ReplicaSet backup */
    // Node ����
    // �� ��� ����
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "exec dbms_shard.backup_ReplicaSets( '"QCM_SQL_STRING_SKIP_FMT"', '"
                                                           QCM_SQL_STRING_SKIP_FMT"', '"
                                                           QCM_SQL_STRING_SKIP_FMT"' )",
                     sUserName,
                     sTargetNodeName,
                     sFailoverNodeName );

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_FAILOVER,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  sTargetNodeName,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Backup ReplicaSet");


    /* Log ������ ���� ������ȭ ���� �� Start */
    /* �� ����ȭ�� FailoverNode to TargetNode�� �̷������ ������
     * Sender�� ���������ϴ�. Receiver�� Failback���� ���� �ؾ���. */
    for ( i = 0; i < sReplicaSetInfo.mCount; i++ )
    {
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mPrimaryNodeName,
                             sTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mFirstBackupNodeName,
                                 sFailoverNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                idlOS::snprintf( sReverseReplName, 
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1,
                                 "R_%s", 
                                 sReplicaSetInfo.mReplicaSets[i].mFirstReplName );
            }
            else if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mSecondBackupNodeName,
                                 sFailoverNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                idlOS::snprintf( sReverseReplName, 
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1,
                                 "R_%s", 
                                 sReplicaSetInfo.mReplicaSets[i].mSecondReplName );
            }
            else
            {
                idlOS::snprintf( sReverseReplName,
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1,
                                 SDM_NA_STR );
            }

            if ( idlOS::strncmp( sReverseReplName,
                                 SDM_NA_STR,
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1 ) != 0 )
            {
                sNodeMetaInfo = sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                           sTargetNodeName );

                IDE_TEST( createReverseReplication( aStatement,
                                                    sFailoverNodeName,
                                                    sReverseReplName,
                                                    sNodeMetaInfo->mNodeName,
                                                    sNodeMetaInfo->mInternalRPHostIP,
                                                    sNodeMetaInfo->mInternalRPPortNo,
                                                    SDI_REPL_SENDER )
                          != IDE_SUCCESS );

                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "EXEC DBMS_SHARD.EXECUTE_IMMEDIATE( 'ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START RETRY', '"QCM_SQL_STRING_SKIP_FMT"' ) ", 
                                 sReverseReplName,
                                 sFailoverNodeName );

                /* DCL �����̶� NewTrans�� ȣ�� */
                IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement, sSqlStr ) != IDE_SUCCESS );

                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "EXEC DBMS_SHARD.EXECUTE_IMMEDIATE( 'ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP', '"QCM_SQL_STRING_SKIP_FMT"' ) ", 
                                 sReverseReplName,
                                 sFailoverNodeName );

                /* DCL �����̶� NewTrans�� ȣ�� */
                IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement, sSqlStr ) != IDE_SUCCESS );

                ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Reverser Replication Created : %s", sReverseReplName );
            }
        }
    }

    IDE_TEST( failoverAddReverseReplicationItems( aStatement,
                                                  sTargetNodeName,
                                                  sFailoverNodeName,
                                                  sTableInfoList,
                                                  &sReplicaSetInfo,
                                                  &sOldNodeInfo,
                                                  sOldSMN ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Shard Meta Change Start");


    /* Shard Meta Update */
    // Table Partition ����
    IDE_TEST( failoverResetShardMeta( aStatement,
                                      sTargetNodeName,
                                      sFailoverNodeName,
                                      sTableInfoList,
                                      &sReplicaSetInfo,
                                      &sOldNodeInfo,
                                      sList,
                                      sOldSMN,
                                      &sIsUnsetNode ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] RangeInfo Update Complete");

    /* FailoverNode�� Next Node �� Failover�� FirstBackupNode�� �����´�. */
    IDE_TEST( sdiZookeeper::getNextNode( sFailoverNodeName, 
                                         sFailoverNextNodeName, 
                                         &sState ) != IDE_SUCCESS );
    /* create RP NewNode to NextNode */
    /* N1 �� N2�� Failover �ߴٸ� N1->N3�� RP�� N2->N3 ���·� ���� �Ͽ��� �Ѵ�.
     * N2 ���忡���� ���� ���ο� ����ȭ�� ����� �� */
    if ( sOldNodeReplicaSet != NULL )
    {
        /* First�� FailoverNode �̰ų� PrevNode�� �� �ۿ� ��� Check ����. */
        if ( idlOS::strncmp( sOldNodeReplicaSet->mSecondBackupNodeName,
                             sFailoverNextNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1) == 0 )
        {
            /* matched */
            idlOS::snprintf( sFailoverCreatedReplName, 
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             "%s", 
                             sOldNodeReplicaSet->mSecondReplName );

            /* FailoverCreatedRP�� Failover �ÿ� �����Ǿ� Failback �ÿ� ���ŵȴ�.
             * Failover ������ �̹� ������ �����Ǿ� ������ ������ �������� ���� ����̴�.
             * Drop �ϰ� Create �ؾ� �Ѵ�. */
            idlOS::snprintf( sSqlStr, 
                             QD_MAX_SQL_LENGTH + 1,
                             "select Count(*) from system_.sys_replications_ where replication_name='%s' ",
                             sFailoverCreatedReplName );

            IDE_TEST( sdi::shardExecDirect( aStatement,
                                            sFailoverNodeName,
                                            (SChar*)sSqlStr,
                                            (UInt) idlOS::strlen(sSqlStr),
                                            SDI_INTERNAL_OP_FAILOVER,
                                            &sExecCount,
                                            &sCount,
                                            NULL,
                                            ID_SIZEOF(sCount),
                                            &sFetchResult )
                      != IDE_SUCCESS );

            if ( sCount != 0 )
            {
                /* RP ���� ������ �����ϸ� �ȵȴ�. Drop �ϰ� ���� �����ؾ� �Ѵ�. */
                IDE_TEST( dropReplicationForInternal( aStatement,
                                                      sFailoverNodeName,
                                                      sFailoverCreatedReplName )
                          != IDE_SUCCESS );
            }
 
            /* ����ȭ ������ Start�� ��� NewTrans�� �̷�� ����. */
            /* Sender�� �����Ѵ�. */
            sFailoverNextLocalMetaInfo = sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                                    sOldNodeReplicaSet->mSecondReplToNodeName );

            IDE_TEST( createReplicationForInternal( aStatement,
                                                    sFailoverNodeName,
                                                    sFailoverCreatedReplName,
                                                    sFailoverNextLocalMetaInfo->mInternalRPHostIP,
                                                    sFailoverNextLocalMetaInfo->mInternalRPPortNo )
                      != IDE_SUCCESS );

            /* Create �� RP�� Rollback�� �����־�� �Ѵ�. */
            idlOS::snprintf(sSqlStr,
                            QD_MAX_SQL_LENGTH,
                            "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                            sFailoverCreatedReplName );

            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               sFailoverNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );

            idlOS::snprintf(sSqlStr,
                            QD_MAX_SQL_LENGTH,
                            "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                            sFailoverCreatedReplName );

            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               sFailoverNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );

            ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] FailoverCreatedReplication Sender Created");


            /* Replication Start */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ", 
                             sOldNodeReplicaSet->mSecondReplName );

            if ( executeRemoteSQL( aStatement,
                                   sFailoverNodeName,
                                   SDI_INTERNAL_OP_FAILOVER,
                                   QCI_STMT_MASK_DCL,
                                   sSqlStr,
                                   QD_MAX_SQL_LENGTH,
                                   NULL ) == IDE_SUCCESS )
            {
                ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Replication Started");

                /* Flush ȣ�� */
                IDE_TEST( flushReplicationForInternal( aStatement,
                                                       sFailoverNodeName,
                                                       sOldNodeReplicaSet->mSecondReplName )
                          != IDE_SUCCESS );
            }
            else
            {
                /* FailoverNextNode�� �׾��ְ� ���� Failover ���� �ʾҴٸ� Start�� �����Ҽ� �ִ�.
                 * Create RP/Add Item�� �����ϰ� Start�� �����Ͽ����Ƿ� 
                 * Retry�� ��õ� �صд�. ���� FailoverNextNode�� Failover ���࿡�� Stop�� ��û�� ���̴�. */
                ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Replication Failed to Start");

                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START RETRY ", 
                                 sOldNodeReplicaSet->mSecondReplName );

                IDE_TEST ( executeRemoteSQL( aStatement,
                                             sFailoverNodeName,
                                             SDI_INTERNAL_OP_FAILOVER,
                                             QCI_STMT_MASK_DCL,
                                             sSqlStr,
                                             QD_MAX_SQL_LENGTH,
                                             NULL ) 
                           != IDE_SUCCESS );

                ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Replication Start Retry");
            }
        }
    }

    /* Replication Add */
    IDE_TEST( failoverAddReplicationItems( aStatement,
                                           sTargetNodeName,
                                           sFailoverNodeName,
                                           sFailoverNextNodeName,
                                           sTableInfoList,
                                           &sReplicaSetInfo,
                                           &sOldNodeInfo,
                                           sOldSMN ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Replication Item Added");

    /* Data Failover �� ����Ǿ��ٸ� �ش� Node�� Unset �Ѵ�.
     * Data�� ������ ���� �� �˼� �־�� �Ѵ�. */
    if ( ( sOldNodeReplicaSet != NULL ) && ( sIsUnsetNode == ID_TRUE ) )
    {
        /* Nodes_ ���� ���� */
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "exec dbms_shard.unset_node( '"QCM_SQL_STRING_SKIP_FMT"' )",
                         sTargetNodeName );

        IDE_TEST( executeSQLNodeList( aStatement,
                                      SDI_INTERNAL_OP_FAILOVER,
                                      QCI_STMT_MASK_SP,
                                      sSqlStr,
                                      sTargetNodeName,
                                      sList ) != IDE_SUCCESS );

        ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Node Meta Update Complete");
    }

    /* ReplicaSet Update & To TargetNode RP Stop */
    /* ReplicaSet Update�� TargetNode->FailoverNode �� �ƴ϶� to TargetNode�� �����ؾ� �ϱ� ������
     * checkFailoverAvailable �δ� �Ǵ��Ҽ� ���� ���ο��� �Ǵ��Ѵ�. */
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "exec dbms_shard.reset_ReplicaSets( '"QCM_SQL_STRING_SKIP_FMT"', '"
                                                          QCM_SQL_STRING_SKIP_FMT"', '"
                                                          QCM_SQL_STRING_SKIP_FMT"' )",
                     sUserName,
                     sTargetNodeName,
                     sFailoverNodeName );

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_FAILOVER,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  sTargetNodeName,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] ReplicaSet Update Complete");

    /* Stop�� Repl�� �����Ѵٸ� After Commit�� Stop�ϵ��� �Ѵ�. */
    if ( sFirstToOldReplicaSet != NULL )
    {
        idlOS::strncpy( sFirstStopNodeName,
                        sFirstToOldReplicaSet->mPrimaryNodeName,
                        SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::strncpy( sFirstStopReplName,
                        sFirstToOldReplicaSet->mFirstReplName,
                        SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    }
    else
    {
        idlOS::strncpy( sFirstStopNodeName,
                        SDM_NA_STR,
                        SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::strncpy( sFirstStopReplName,
                        SDM_NA_STR,
                        SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    }
    if ( sSecondToOldReplicaSet != NULL )
    {
        idlOS::strncpy( sSecondStopNodeName,
                        sSecondToOldReplicaSet->mPrimaryNodeName,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        idlOS::strncpy( sSecondStopReplName,
                        sSecondToOldReplicaSet->mSecondReplName,
                        SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    }
    else
    {
        idlOS::strncpy( sSecondStopNodeName,
                        SDM_NA_STR,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        idlOS::strncpy( sSecondStopReplName,
                        SDM_NA_STR,
                        SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    }

    sdiZookeeper::setReplInfo4FailoverAfterCommit( sFirstStopNodeName,
                                                   sSecondStopNodeName,
                                                   sFirstStopReplName,
                                                   sSecondStopReplName );

    /* Partition Swap */
    // Table Partition ���� Swap
    // �۾��� �ʿ��� Node���� ����
    /* Swap�� ����Ǵ� Node�� Table�� Lock ȹ�� �� Swap */

    /* R2HA Flush XLog Files */
    IDE_TEST( lockTables4Failover( aStatement,
                                   sSqlStr,
                                   QD_MAX_SQL_LENGTH,
                                   sFailoverNodeName,
                                   sTableInfoList,
                                   &sReplicaSetInfo,
                                   &sOldNodeInfo,
                                   sOldSMN ) != IDE_SUCCESS );
    
    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] TargetTable Lock Acquired");

    IDE_TEST( failoverTablePartitionSwap( aStatement,
                                          sFailoverNodeName,
                                          sTableInfoList,
                                          &sReplicaSetInfo,
                                          &sOldNodeInfo,
                                          sOldSMN ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Partition Swap Complete");

    /* BUG-48586 */ 
    IDE_TEST( executeRemoteSQL( aStatement,
                                sFailoverNodeName,
                                SDI_INTERNAL_OP_NORMAL,
                                QCI_STMT_MASK_SP,
                                sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "exec dbms_shard.set_ReplicaSCN()") != IDE_SUCCESS );

    sIsAllocList = ID_FALSE;
    sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );    

    ideLog::log(IDE_SD_21,"[SHARD_FAILOVER] Zookeeper Meta Update Complete");

//    sdi::finalizeSession( aStatement->session );

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );    
    }

    ideLog::log(IDE_SD_5,"[SHARD_FAILOVER_ERROR] Fail");

//    sdi::finalizeSession( aStatement->session );

    return IDE_FAILURE;
}

IDE_RC qdsd::validateShardFailback( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD FAILBACK
 *
 * Implementation :
 *    1.
 ***********************************************************************/

    idBool              sIsConnect  = ID_FALSE;
    SChar               sRecentDeadNode[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    sdiLocalMetaInfo    sLocalMetaInfo;
    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;

    qciUserInfo         sUserInfo;
    void *              sMmSession = aStatement->session->mMmSession;

    SInt                sDataLength;
    SChar               sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    qci::mSessionCallback.mGetUserInfo( sMmSession, &sUserInfo );

    /***********************************************************************
     * validateCommon
     ***********************************************************************/

    IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );

    /* BUG-48770 �ٸ� session�� �ְų� shard cli�� ������ ��쿡�� shard DDL�� ������ �� ����.*/
    if( QCU_DISPLAY_PLAN_FOR_NATC == 0 )
    {
        IDE_TEST_RAISE( qci::mSessionCallback.mCheckSessionCount() > 1, ERR_TOO_MANY_SESSION );
    }

    IDE_TEST_RAISE( qci::mSessionCallback.mIsShardClient( aStatement->session->mMmSession )
                    == ID_TRUE,
                    ERR_SHARD_CLI );

    /***********************************************************************
     * validate SHARD FAILBACK
     ***********************************************************************/

    /***********************************************************************
     * Zookeeper validation
     ***********************************************************************/

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );
    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', no_meta_exist );

    sdiZookeeper::initialize( sLocalMetaInfo.mNodeName,
                              &sUserInfo );
    IDE_TEST_RAISE( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS, connect_Fail );
    sIsConnect = ID_TRUE;

    /* ���� ��Ÿ�� �����ϹǷ� zookeeper�� ���� ��Ÿ lock�� ��´�. */
    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    sIsAcquireZookeeperMetaLock = ID_TRUE; 

    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );
    sIsAcquireShardMetaLock = ID_TRUE;

    /* Check Fault_detect_time */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    IDE_TEST( sdiZookeeper::readInfo( (SChar *)SDI_ZKC_PATH_FAULT_DETECT_TIME,
                                      sBuffer,
                                      &sDataLength ) != IDE_SUCCESS );

    // getProperty SHARD_FAILBACK_GIVEUP_INTERVAL
    

    /* ���� ����� ���°� FAILBACK�� ������ �������� üũ�Ѵ�. */
    IDE_TEST( sdiZookeeper::validateState( ZK_FAILBACK,
                                           sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );

    /* Check Failover To */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    IDE_TEST( sdiZookeeper::getNodeInfo( sdiZookeeper::mMyNodeName,
                                         (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                         sBuffer,
                                         &sDataLength ) != IDE_SUCCESS );

    if ( sDataLength > 0 )
    {
        /* �� ��尡 Failover �Ǿ� ������ */
        /* �� ��尡 ���� �ֱٿ� ���� ������� Ȯ���Ѵ�. */
        IDE_TEST( sdiZookeeper::checkRecentDeadNode( sRecentDeadNode, NULL, NULL ) != IDE_SUCCESS );

        IDE_TEST_RAISE( idlOS::strncmp( sdiZookeeper::mMyNodeName,
                                        sRecentDeadNode,
                                        idlOS::strlen( sRecentDeadNode ) ) != 0, not_my_turn );
    }
    else
    {
        /* �� ��尡 Failover ������� �ʾ����� �׳� Failback ����̴�. */
    }

    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }
    
    if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    sdiZookeeper::disconnect();
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TOO_MANY_SESSION )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_SESSION, "failback" ) );
    }
    IDE_EXCEPTION( ERR_SHARD_CLI )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_DDL_TRY_WITH_SHARD_CLI ) );
    }
    IDE_EXCEPTION( connect_Fail )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_CONNECTION_FAIL ) );
    }
    IDE_EXCEPTION( no_meta_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( not_my_turn )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_NOT_MY_TURN ) ); 
    }
    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }
    IDE_EXCEPTION_END;
 
    if ( sIsAcquireShardMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseShardMetaLock();
        sIsAcquireShardMetaLock = ID_FALSE;
    }
   
    if( sIsAcquireZookeeperMetaLock == ID_TRUE )
    {
        sdiZookeeper::releaseZookeeperMetaLock();
        sIsAcquireZookeeperMetaLock = ID_FALSE;
    }

    if( sIsConnect == ID_TRUE )
    {
        sdiZookeeper::disconnect();
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardFailback( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD FAILBACK
 *
 * Implementation :
 *    1. 
 ***********************************************************************/

    SChar               sRecentDeadNode[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar               sFailbackFromNodeName[SDI_NODE_NAME_MAX_SIZE + 1];

    ULong               sFailbackSMN = 0;
    ULong               sFailoveredSMN = 0;
    ULong               sOldSMN = 0;
    ULong               sZKSMN = 0;

    SInt                sDataLength;
    SChar               sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    idBool              sExistNotFailedoverDeadNode = ID_FALSE;

    sdiFailbackState    sFailbackState = SDI_FAILBACK_NONE;

    /***********************************************************************
     * check internal property validate
     ***********************************************************************/
    IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sdiZookeeper::isConnect() == ID_TRUE, ERR_ALREADY_CONNECT );

    /* zookeeper server�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::connect( ID_FALSE ) != IDE_SUCCESS );
    
    /* Meta Lock ȹ�� */
    sdi::setShardMetaTouched( aStatement->session );

    IDE_TEST( checkShardPIN( aStatement ) != IDE_SUCCESS );

    sdi::setInternalTableSwap( aStatement->session );

    IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
    IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Meta Lock Acquired");

    /* Meta Lock ȹ�� �� Validation */
    /* ���� ����� ���°� FAILBACK�� ������ �������� üũ�Ѵ�. */
    IDE_TEST( sdiZookeeper::validateState( ZK_FAILBACK,
                                           sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );

    /***********************************************************************
     * Zookeeper meta 1�� ����
     ***********************************************************************/
    /* zookeeper ��Ÿ�� �����Ѵ�. */
    IDE_TEST( executeZookeeperFailback() != IDE_SUCCESS );

    /* Check Failover To */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    IDE_TEST( sdiZookeeper::getNodeInfo( sdiZookeeper::mMyNodeName,
                                         (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                         sBuffer,
                                         &sDataLength ) != IDE_SUCCESS );

    if ( sDataLength > 0 )
    {
        /* �� ��尡 Failover �Ǿ� ������ */
        /* �� ��尡 ���� �ֱٿ� ���� ������� Ȯ���Ѵ�. */
        IDE_TEST( sdiZookeeper::checkRecentDeadNode( sRecentDeadNode, &sFailbackSMN, &sFailoveredSMN ) != IDE_SUCCESS );

        IDE_TEST_RAISE( idlOS::strncmp( sdiZookeeper::mMyNodeName,
                                        sRecentDeadNode,
                                        idlOS::strlen( sRecentDeadNode ) ) != 0, not_my_turn );

        /* �� ��尡 ���� �ֱ� FAILOVER ����̸� FAILBACK_FAILEDOVER �̴�. */

        /* FailoverHistory�� �ִ� Node �ܿ� DeadNode�� �����Ѵٸ� Failback �������� �ʾƾ� �Ѵ�. */
        IDE_TEST( sdiZookeeper::checkExistNotFailedoverDeadNode( &sExistNotFailedoverDeadNode ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sExistNotFailedoverDeadNode != ID_FALSE, EXIST_NOT_FAILEDOVER_DEADNODE );

        /* Failover�� �ذ� Node�� ���� Meta�� ������ ���̴�. */
        idlOS::strncpy( sFailbackFromNodeName,
                        sBuffer,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        sFailbackState = SDI_FAILBACK_FAILEDOVER;

        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILBACK_FAILEDOVER Start");
    }
    else
    {
        /* �� ��尡 Failover ������� �ʾ����� Failback ����̴�. */

        /* check Zookeeper SMN */
        /* SMN�� Ȯ���Ͽ� Zookeeper�� �����ϸ� FAILBACK_JOIN �̴�. */
        //IDE_TEST( checkZookeeperSMNAndDataSMN( aStatement ) != IDE_SUCCESS );
        IDE_TEST( sdiZookeeper::getZookeeperSMN( &sZKSMN ) != IDE_SUCCESS );
        sOldSMN = sdi::getSMNForDataNode();

        if ( sOldSMN == sZKSMN )
        {
            /* Failover ���� �ʾҴµ� Zookeeper�� Node�� SMN�� ��ġ�ϸ� FAILBACK_JOIN�̴�.*/
            sFailbackState = SDI_FAILBACK_JOIN;
            ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILBACK_JOIN Start");
        }
        else
        {
            /* Failover ���� �ʾҴµ�, SMN�� ��ġ���� ������ FAILBACK_JOIN_SMN_MODIFIED �̴� */
            sFailbackState = SDI_FAILBACK_JOIN_SMN_MODIFIED;
            ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILBACK_JOIN_SMN_MODIFIED Start");
        }

        /* NextAliveNode�� �̸��� �����´�. */
        /* �ƹ��� Failover ���� �ʾ����� NextAliveNode�� �������� Meta�� �����. */
        IDE_TEST( sdiZookeeper::getNextAliveNode( sdiZookeeper::mMyNodeName,
                                                  sFailbackFromNodeName ) != IDE_SUCCESS );
    }

    /***********************************************************************
     * FAILBACK ���� �� ����
     ***********************************************************************/  

    switch ( sFailbackState )
    {
        case SDI_FAILBACK_JOIN:
        case SDI_FAILBACK_JOIN_SMN_MODIFIED:
            IDE_TEST( failbackJoin( aStatement,
                                    sFailbackFromNodeName,
                                    sFailbackState ) != IDE_SUCCESS );
            break;
        case SDI_FAILBACK_FAILEDOVER:
            IDE_TEST( failbackFailedover( aStatement,
                                          sFailbackFromNodeName,
                                          sFailbackSMN ) != IDE_SUCCESS );
            break;
        case SDI_FAILBACK_NONE:
        default:
            /* �̷��� ���� �ȵ� */
            IDE_DASSERT( 0 );
            break;
    }

    sdi::loadShardPinInfo();

    return IDE_SUCCESS;

    IDE_EXCEPTION( EXIST_NOT_FAILEDOVER_DEADNODE )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_EXIST_NOT_FAILEDOVER_DEADNODE ));
    }
    IDE_EXCEPTION( not_my_turn )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_NOT_MY_TURN ) ); 
    }
    IDE_EXCEPTION( ERR_ALREADY_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_ALREADY_CONNECTED ) );
    }
    IDE_EXCEPTION_END;

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILED FAILBACK");

    sdi::unsetInternalTableSwap( aStatement->session );

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();

    return IDE_FAILURE;
}

IDE_RC qdsd::getProcedureKeyValue( qdReShardAttribute * aReshardAttr,
                                   SChar              * aProcKeyVal )
{
    qcNamePosition           sValPos;
    qmmValueNode           * sValNode;

    SET_EMPTY_POSITION( sValPos );

    idlOS::memset( aProcKeyVal, 0x00, SDI_RANGE_VARCHAR_MAX_SIZE + 1 );
    IDE_DASSERT(aReshardAttr->mKeyValue != NULL);
    IDE_DASSERT(aReshardAttr->mKeyValue->next == NULL);
    sValNode = aReshardAttr->mKeyValue;
    if ( sValNode->value != NULL )
    {
        sValPos = sValNode->value->position;

        IDE_TEST_RAISE(sValPos.size > (SInt)SDI_RANGE_VARCHAR_MAX_SIZE, ERR_SHARD_KEY_MAX_VALUE_TOO_LONG);
        idlOS::memcpy( aProcKeyVal,
                       sValPos.stmtText + sValPos.offset,
                       sValPos.size );
        aProcKeyVal[sValPos.size] = '\0';
    }
    else
    {
        aProcKeyVal[0] = '\0';
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_SHARD_KEY_MAX_VALUE_TOO_LONG)
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_KEY_MAX_VALUE_TOO_LONG ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::validateShardMove( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD MOVE [ {TABLE object_name {PARTITION ( partition_name ) }} |
 *                                {PROCEDURE object_name {KEY ( key_value )} } ]+
 *    TO node_name
 *
 ***********************************************************************/
    idBool               sIsValid = ID_FALSE;
    qdShardParseTree   * sParseTree = NULL;
    qdReShardAttribute * sReShardAttr = NULL;
    SChar               sPath[SDI_ZKC_PATH_LENGTH] = SDI_ZKC_PATH_ALTIBASE_SHARD;
    SChar               sToNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = { 0, };
    SChar               sFromNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = { 0, };
    UInt                sUserID;
    SChar               sUserName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    SChar               sObjectName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    SChar               sPartitionName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    idBool              sIsAcquireZookeeperMetaLock = ID_FALSE;
    idBool              sIsAcquireShardMetaLock = ID_FALSE;
    sdiLocalMetaInfo  * sToNodeLocalInfo = NULL;
    SInt                i = 0;
    idBool              sIsMetaInited = ID_FALSE;
    SChar               sProcKeyValue[SDI_RANGE_VARCHAR_MAX_SIZE + 1];
    SChar             * sKeyValue = NULL;
    idBool              sIsCloneExist = ID_FALSE;
    idBool              sIsNotCloneExist = ID_FALSE;
    iduList           * sNodeInfoList;
    UInt                sNodeCount;
    IDE_CLEAR();
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    /***********************************************************************
     * validateCommon
     ***********************************************************************/
    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {
        IDE_TEST( validateCommon( aStatement ) != IDE_SUCCESS );

        IDE_TEST_RAISE( sdiZookeeper::isConnect() != ID_TRUE, ERR_NOT_CONNECT );

        /* Ŭ������ ��Ÿ�� �����Ǿ��ִ��� Ȯ���Ѵ�. */
        IDE_TEST( sdiZookeeper::checkPath( sPath,
                                           &sIsMetaInited ) != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsMetaInited != ID_TRUE, ERR_META );

        /* property confirmation resharding properties */
        IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS ); 
        sIsAcquireZookeeperMetaLock = ID_TRUE;

        IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() )
                  != IDE_SUCCESS );
        sIsAcquireShardMetaLock = ID_TRUE;

        IDE_TEST( sdiZookeeper::getAllNodeInfoList( &sNodeInfoList,
                                                    &sNodeCount )
                  != IDE_SUCCESS );

        IDE_TEST_RAISE( sNodeInfoList == NULL,
                        ERR_CANNOT_RESHARDING_SHORT_OF_NODE_COUNT );
    }
    
    /***********************************************************************
     * validate SHARD MOVE
     ***********************************************************************/

    switch(sParseTree->mDDLType)
    {
        case SHARD_MOVE_TO:
            IDE_DASSERT( QC_IS_NULL_NAME(sParseTree->mNodeName) != ID_TRUE );
            QC_STR_COPY(sToNodeName, sParseTree->mNodeName);
            IDE_TEST_RAISE( sNodeCount < 2, ERR_CANNOT_RESHARDING_SHORT_OF_NODE_COUNT );
            sToNodeLocalInfo = sdiZookeeper::getNodeInfo(sNodeInfoList, sToNodeName);
            IDE_TEST_RAISE( sToNodeLocalInfo == NULL, ERR_NOT_FOUND_NODE );
            break;
        case SHARD_MOVE_REMOVE:
            IDE_RAISE(ERR_NOT_SUPPORTED_OPTION );
            IDE_DASSERT( QC_IS_NULL_NAME(sParseTree->mNodeName) == ID_TRUE );
            break;
        case SHARD_MOVE_TO_INTERNAL_CLONE_COPY:
        case SHARD_DDL_UNKNOWN:
        case SHARD_ADD:
            if ( sdi::isShardDDLForAddClone( aStatement ) == ID_TRUE )
            {
                // shard add���� mNodeInfoList, mNodeCount�� set �Ǿ� �ִ�.
                IDE_DASSERT( sParseTree->mNodeInfoList != NULL );
                IDE_DASSERT( QC_IS_NULL_NAME(sParseTree->mNodeName) != ID_TRUE );
                
                QC_STR_COPY(sToNodeName, sParseTree->mNodeName);
                IDE_TEST_RAISE( sParseTree->mNodeCount < 2, ERR_CANNOT_RESHARDING_SHORT_OF_NODE_COUNT );
                sToNodeLocalInfo = sdiZookeeper::getNodeInfo(sParseTree->mNodeInfoList, sToNodeName);
                IDE_TEST_RAISE( sToNodeLocalInfo == NULL, ERR_NOT_FOUND_NODE );
            }
            break;
        case SHARD_JOIN:
        case SHARD_DROP:
        case SHARD_DROP_FORCE:
        case SHARD_FAILOVER_NORMAL:
        case SHARD_FAILOVER_IMMEDIATE:
        case SHARD_FAILOVER_EXIT:
        case SHARD_FAILOVER_FORCE:
        case SHARD_FAILBACK:
        default:
            IDE_RAISE( ERR_INVALID_CONDITION );
            break;
    }
    
    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {                    
        /* zookeeper server�� �׾��ִ� ��尡 �ִ��� üũ�Ѵ�. */
        IDE_TEST( sdiZookeeper::checkAllNodeAlive( &sIsValid )
                  != IDE_SUCCESS );
        IDE_TEST_RAISE( sIsValid == ID_FALSE, deadNode_exist );
    }
                
    /* BUG-48290 */
    IDE_TEST( qcg::setSessionIsInternalLocalOperation( aStatement, SDI_INTERNAL_OP_NORMAL) != IDE_SUCCESS);

    for ( sReShardAttr = sParseTree->mReShardAttr, i = 0;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next, i++ )
    {
        if (QC_IS_NULL_NAME(sReShardAttr->mUserName) == ID_TRUE)
        {
            sUserID = QCG_GET_SESSION_USER_ID(aStatement);
            IDE_TEST(qciMisc::getUserName(aStatement, sUserID, sUserName) != IDE_SUCCESS);
            sReShardAttr->mUserName.offset = 0;
            sReShardAttr->mUserName.size = idlOS::strlen(sUserName);
            IDE_TEST( QC_QMP_MEM(aStatement)->cralloc( sReShardAttr->mUserName.size + 1,
                                                     (void **) &(sReShardAttr->mUserName.stmtText))
                      != IDE_SUCCESS );
            idlOS::strncpy(sReShardAttr->mUserName.stmtText, sUserName, sReShardAttr->mUserName.size + 1);
        }
        else
        {
            IDE_TEST(qciMisc::getUserID( aStatement, sReShardAttr->mUserName, &sUserID )
                     != IDE_SUCCESS);
            QC_STR_COPY( sUserName, sReShardAttr->mUserName );
        }

        IDE_TEST_RAISE( QC_IS_NULL_NAME(sReShardAttr->mObjectName) == ID_TRUE, ERR_OBJECT_NAME );
        QC_STR_COPY( sObjectName, sReShardAttr->mObjectName );

        switch (sReShardAttr->mObjectType)
        {
            case 'T':
                if ( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE )
                {
                    QC_STR_COPY( sPartitionName, sReShardAttr->mPartitionName );
                }
                else
                {
                    sPartitionName[0] = '\0';
                }
                IDE_TEST( sdi::getTableInfoAllForDDL( aStatement,
                                                      sUserID,
                                                      (UChar*)sObjectName,
                                                      (SInt)idlOS::strlen(sObjectName),
                                                      ID_FALSE,
                                                      &(sReShardAttr->mTableInfo),
                                                      &(sReShardAttr->mTableSCN),
                                                      &(sReShardAttr->mTableHandle),
                                                      &(sReShardAttr->mShardObjectInfo) )
                          != IDE_SUCCESS);

                IDE_TEST( sdi::validateOneReshardTable( aStatement,
                                                        sUserName,
                                                        sObjectName,
                                                        sPartitionName,
                                                        sReShardAttr->mShardObjectInfo,
                                                        sReShardAttr->mFromNodeName,   /* out */
                                                        sReShardAttr->mDefaultNodeName /* out */
                                                      ) != IDE_SUCCESS );

                IDE_TEST_RAISE( ( sParseTree->mDDLType == SHARD_MOVE_REMOVE ) &&  /* remove syntax */
                                ( sdi::getShardObjectType( &(sReShardAttr->mShardObjectInfo->mTableInfo) )
                                  == SDI_SINGLE_SHARD_KEY_DIST_OBJECT ) &&
                                ( idlOS::strncmp( sReShardAttr->mDefaultNodeName,
                                                  sReShardAttr->mFromNodeName,
                                                  SDI_NODE_NAME_MAX_SIZE ) != 0 ) && /* not default node partition remove */
                                ( SDI_IS_NULL_NAME(sReShardAttr->mDefaultNodeName) != ID_TRUE ), /* exist default node */
                                ERR_EXIST_DEFAULT_NODE );

                if ( sdi::getShardObjectType( &(sReShardAttr->mShardObjectInfo->mTableInfo) ) == SDI_CLONE_DIST_OBJECT )
                {
                    sIsCloneExist = ID_TRUE;
                }
                else /* not clone */
                {
                    sIsNotCloneExist = ID_TRUE;
                }
                break;
            case 'P':
                if( sReShardAttr->mKeyValue != NULL )
                {
                    IDE_TEST( getProcedureKeyValue( sReShardAttr,
                                                    sProcKeyValue ) != IDE_SUCCESS );

                    if(sProcKeyValue[0] == '\0') /* strlen 0, it is DEFAULT */
                    {
                        sKeyValue = NULL;
                    }
                    else
                    {
                        sKeyValue = sProcKeyValue;
                    }
                }
                else
                {
                    /* clone & solo : do nothing */
                    sKeyValue = NULL;
                }
                IDE_TEST( sdi::getProcedureInfo(aStatement,
                                                sUserID,
                                                sReShardAttr->mUserName,
                                                sReShardAttr->mObjectName,
                                                &(sReShardAttr->mShardObjectInfo)) != IDE_SUCCESS );
                IDE_TEST_RAISE(sReShardAttr->mShardObjectInfo == NULL, ERR_NOT_FOUND_OBJ);

                IDE_TEST( sdi::validateOneReshardProc( aStatement,
                                                       sUserName,
                                                       sObjectName,
                                                       sKeyValue,
                                                       sReShardAttr->mShardObjectInfo,
                                                       sReShardAttr->mFromNodeName,   /* out */
                                                       sReShardAttr->mDefaultNodeName /* out */
                                                     ) != IDE_SUCCESS );
                if ( sdi::getShardObjectType( &(sReShardAttr->mShardObjectInfo->mTableInfo) ) == SDI_CLONE_DIST_OBJECT )
                {
                    sIsCloneExist = ID_TRUE;
                }
                else /* not clone */
                {
                    sIsNotCloneExist = ID_TRUE;
                }
                break;
            default:
                IDE_RAISE(ERR_INVALID_OBJ_TYPE);
                break;
        }

        if ( i == 0 ) /* first reshard attribute */
        {
            (void)idlOS::strncpy( sFromNodeName, sReShardAttr->mFromNodeName, SDI_NODE_NAME_MAX_SIZE + 1 );
        }
        else
        {
            /* the fromNodeName of reshard table must be one node */
            IDE_TEST_RAISE( idlOS::strncmp( sFromNodeName, 
                                            sReShardAttr->mFromNodeName, 
                                            SDI_NODE_NAME_MAX_SIZE ) != 0, ERR_FROM_NODE );
        }
    }
    IDE_TEST_RAISE( ( sIsCloneExist == ID_TRUE ) &&
                    ( QCG_GET_SESSION_IS_SHARD_INTERNAL_LOCAL_OPERATION( aStatement ) != ID_TRUE ),
                    ERR_CLONE_RESHARDING_NOT_SUPPORTED);
    /* clone and not clone resharding is not support, user can not execute clone resharding */
    IDE_TEST_RAISE(( sIsCloneExist == ID_TRUE ) && (sIsNotCloneExist == ID_TRUE), ERR_CLONE_RESHARDING_NOT_SUPPORTED);

    if ( sIsCloneExist == ID_TRUE )
    {
        sParseTree->mDDLType = SHARD_MOVE_TO_INTERNAL_CLONE_COPY;
    }
    else
    {
        IDE_DASSERT(sIsNotCloneExist == ID_TRUE);
        /* do nothing: keep orignal ddl type */
    }
    
    /***********************************************************************
     * check SMN
     ***********************************************************************/
    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {                    
        IDE_TEST( checkZookeeperSMNAndDataSMN() != IDE_SUCCESS );

        if ( sIsAcquireShardMetaLock == ID_TRUE )
        {
            sdiZookeeper::releaseShardMetaLock();
            sIsAcquireShardMetaLock = ID_FALSE;
        }
    
        if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
        {
            sdiZookeeper::releaseZookeeperMetaLock();
            sIsAcquireZookeeperMetaLock = ID_FALSE;
        }
    }
                
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_META );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( ERR_FROM_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_SOURCE_NODES_FOR_RESHARDING ) );
    }
    IDE_EXCEPTION( ERR_OBJECT_NAME );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_NOT_EXIST_OBJECT_NAME_FOR_RESHARDING ) );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_NODE_NAME, sToNodeName ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CONDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::validateShardMove", "Invalid condition." ) );
    }
    IDE_EXCEPTION( ERR_NOT_SUPPORTED_OPTION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCP_SYNTAX,": REMOVE option is not supportted." ) );
    }
    IDE_EXCEPTION( ERR_CANNOT_RESHARDING_SHORT_OF_NODE_COUNT );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_FEW_NODES_FOR_RESHARDING ) );
    }
    IDE_EXCEPTION( ERR_EXIST_DEFAULT_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_NOT_SUPPORT_REMOVE_SHARD_TABLE_WITH_DEFAULT_NODE ) );
    }
    IDE_EXCEPTION( deadNode_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_DEADNODE_EXIST ) );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND_OBJ );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_OBJECT_NOT_FOUND,
                                  sUserName,
                                  sObjectName) );
    }
    IDE_EXCEPTION( ERR_NOT_CONNECT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_NOT_JOIN ) );
    }
    IDE_EXCEPTION( ERR_INVALID_OBJ_TYPE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                  "qdsd::validateShardMove invalid object type",
                                  sReShardAttr->mObjectType ) );
    }
    IDE_EXCEPTION( ERR_CLONE_RESHARDING_NOT_SUPPORTED )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_NOT_SUPP_CLONE_RESHRD ) );
    }
    IDE_EXCEPTION_END;

    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {
        // the logic below must be move to rollback.  
        if ( sIsAcquireShardMetaLock == ID_TRUE )
        {
            sdiZookeeper::releaseShardMetaLock();
            sIsAcquireShardMetaLock = ID_FALSE;
        }
        if ( sIsAcquireZookeeperMetaLock == ID_TRUE )
        {
            sdiZookeeper::releaseZookeeperMetaLock();
            sIsAcquireZookeeperMetaLock = ID_FALSE;
        }
    }
    
    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardMove( qcStatement * aStatement )
{
/***********************************************************************
 *
 * Description :
 *    ALTER DATABASE SHARD MOVE [ {TABLE object_name {PARTITION ( partition_name ) }} |
 *                                {PROCEDURE object_name {KEY ( key_value )} } ]+
 *    TO node_name
 *
 * Implementation :
 *    1. parse tree shard object smn validation
 *    2. make name for resharding toNode
 *    3. check node is exist and self resharding
 *    4. get replicaSets fromNode and toNode. and check replication running
 *    5. Determine whether the primary data should be synchronized.
 *    6. Determine whether the first backup data should be synchronized.
 *    7. Determine whether the second backup data should be synchronized.
 *    8. correct truncate flag from k-safey and count of nodes
 *    ex> k-safety : 1
 *    1. alter database shard move table t1 partition p2 to N4
 * <BEFORE>     N1        N2        N3        N4
 * Primary      P1     |--P2--|     P3        P4
 * Backup       P4     |  P1  |     P2        P3
 * <AFTER>      N1     |  N2  |     N3        N4
 * Primary      P1     |      |     P3        P2, P4
 * Backup       P4,P2<-|  P1  |----------------^  P3
 *    2. alter database shard move table t1 partition p2 to N1
 * <BEFORE>     N1        N2        N3        N4
 * Primary      P1     |--P2-----|  P3        P4
 * Backup       P4     |  P1     |  P2        P3
 * <AFTER>      N1     |  N2     |  N3        N4
 * Primary      P1,P2  |         |  P3        P4
 * Backup       P4 ^---|  P1,P2<-|            P3
 ***********************************************************************/
    qdShardParseTree   * sParseTree = NULL;
    qdReShardAttribute * sReShardAttr;
    sdiLocalMetaInfo     sLocalMetaInfo;
    SChar                sFromNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = { 0, };
    SChar                sToNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = { 0, };
    sdiReplicaSetInfo  sFromReplicaSetsInfo;
    sdiReplicaSetInfo  sToReplicaSetsInfo;
    sdiReplicaSet    * sFromReplicaSetPtr = NULL;
    sdiReplicaSet    * sToReplicaSetPtr = NULL;
    sdiReplicaSet      sToReplicaSet;
    ULong              sSMN = SDI_NULL_SMN;
    idBool             sIsPrimaryDataSync = ID_FALSE;
    idBool             sIsFirstBackupDataSync = ID_FALSE;
    idBool             sIsSecondBackupDataSync = ID_FALSE;
    SChar            * sSqlStr = NULL;
    SChar              sPrimaryReplicationName[SDI_REPLICATION_NAME_MAX_SIZE + 1] = { 0, };
    SChar              sFirstBackupReplicationName[SDI_REPLICATION_NAME_MAX_SIZE + 1] = { 0, };
    SChar              sSecondBackupReplicationName[SDI_REPLICATION_NAME_MAX_SIZE + 1] = { 0, };
    idBool             sIsFromNodeTruncate = ID_TRUE;
    idBool             sIsFirstBackupOfFromNodeTruncate = ID_TRUE;
    idBool             sIsSecondBackupOfFromNodeTruncate = ID_TRUE;
    sdiLocalMetaInfo * sToNodeLocalInfo = NULL;
    UInt               sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt               sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);
    sdiGlobalMetaInfo  sMetaNodeInfo = { ID_ULONG(0) };
    
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    /***********************************************************************
     * check internal property validate
     ***********************************************************************/
    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {
        IDE_TEST( validateCommonForExecute( aStatement ) != IDE_SUCCESS );

        /* for smn propagation to all nodes at commit */
        sdi::setShardMetaTouched( aStatement->session );

        IDE_TEST( sdiZookeeper::getZookeeperMetaLock(QCG_GET_SESSION_ID(aStatement)) != IDE_SUCCESS );
        IDE_TEST( sdiZookeeper::getShardMetaLock( QC_SMI_STMT(aStatement)->getTrans()->getTransID() )
                  != IDE_SUCCESS );

        IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );

        sdiZookeeper::reshardingJob();
    }

    /* 1. parse tree shard object smn validation */
    for ( sReShardAttr = sParseTree->mReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_TEST( sdi::validateObjectSMN(aStatement, sReShardAttr->mShardObjectInfo) != IDE_SUCCESS );
    }
    /* mFromNodeName of all reshard attr are identical */
    (void)idlOS::strncpy( sFromNodeName, sParseTree->mReShardAttr->mFromNodeName, SDI_NODE_NAME_MAX_SIZE + 1 );

    /* 2. make resharding tonode name */
    switch(sParseTree->mDDLType)
    {
        case SHARD_MOVE_TO:
        case SHARD_MOVE_TO_INTERNAL_CLONE_COPY:
            IDE_DASSERT( QC_IS_NULL_NAME(sParseTree->mNodeName) != ID_TRUE );
            QC_STR_COPY( sToNodeName, sParseTree->mNodeName );
            break;
        case SHARD_MOVE_REMOVE:
            SDI_SET_NULL_NAME( sToNodeName );
            break;
        case SHARD_DDL_UNKNOWN:
        case SHARD_ADD:
        case SHARD_JOIN:
        case SHARD_DROP:
        case SHARD_DROP_FORCE:
        case SHARD_FAILOVER_NORMAL:
        case SHARD_FAILOVER_IMMEDIATE:
        case SHARD_FAILOVER_EXIT:
        case SHARD_FAILOVER_FORCE:
        case SHARD_FAILBACK:
        default:
            IDE_RAISE( ERR_INVALID_CONDITION );
            break;
    }

    /* 3. check node is exist and self resharding */
    if ( SDI_IS_NULL_NAME(sToNodeName) != ID_TRUE )
    {
        sToNodeLocalInfo = sdiZookeeper::getNodeInfo(sParseTree->mNodeInfoList, sToNodeName);
        IDE_TEST_RAISE( sToNodeLocalInfo == NULL, ERR_NOT_FOUND_NODE);
        IDE_TEST_RAISE( sdi::isSameNode(sToNodeName, sFromNodeName) == ID_TRUE, ERR_SAME_NODE );
    }

    /* 4. get replicaSets fromNode and toNode. and check replication running   */
    if ( sdi::isShardDDLForAddClone( aStatement ) == ID_TRUE )
    {
        IDE_TEST( sdi::getGlobalMetaInfoCore( QC_SMI_STMT( aStatement ),
                                              &sMetaNodeInfo )
                  != IDE_SUCCESS );

        sSMN = sMetaNodeInfo.mShardMetaNumber;
    }
    else
    {    
        sSMN = sdi::getSMNForDataNode();
    }
    
    
    IDE_TEST( sdi::getReplicaSet( QC_SMI_STMT(aStatement)->getTrans(),
                                  sFromNodeName,
                                  ID_FALSE,
                                  sSMN,
                                  &sFromReplicaSetsInfo ) != IDE_SUCCESS );

    IDE_TEST_RAISE( sFromReplicaSetsInfo.mCount != 1, ERR_SHARD_META );
    sFromReplicaSetPtr = &sFromReplicaSetsInfo.mReplicaSets[0];
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    /* Backup Replications running check, 
     * If backup replications of from node and to node does not running normally 
     * then you cannot run resharding.
     */
    IDE_TEST( sdi::checkBackupReplicationRunning( sLocalMetaInfo.mKSafety,
                                                  sFromReplicaSetPtr,
                                                  sParseTree->mNodeCount )
              != IDE_SUCCESS );

    if ( SDI_IS_NULL_NAME(sToNodeName) != ID_TRUE )
    {
        IDE_TEST( sdi::getReplicaSet( QC_SMI_STMT(aStatement)->getTrans(),
                                      sToNodeName,
                                      ID_FALSE,
                                      sSMN,
                                      &sToReplicaSetsInfo ) != IDE_SUCCESS );
        IDE_TEST_RAISE( sToReplicaSetsInfo.mCount != 1, ERR_SHARD_META );
        sToReplicaSetPtr = &sToReplicaSetsInfo.mReplicaSets[0];
        IDE_TEST( sdi::checkBackupReplicationRunning( sLocalMetaInfo.mKSafety,
                                                      sToReplicaSetPtr,
                                                      sParseTree->mNodeCount ) != IDE_SUCCESS );
    }
    else
    {
        SDI_INIT_REPLICA_SET(&sToReplicaSet);
        sToReplicaSetPtr = &sToReplicaSet;
    }

    /* 5. Determine whether the primary data should be synchronized. */
    if ( SDI_IS_NULL_NAME(sToNodeName) == ID_TRUE )
    {
        /* REMOVE */
        SDI_SET_NULL_NAME(sPrimaryReplicationName);
        sIsPrimaryDataSync = ID_FALSE;
    }
    else
    {
        if ( sParseTree->mDDLType == SHARD_MOVE_TO_INTERNAL_CLONE_COPY )
        {
            idlOS::snprintf(sPrimaryReplicationName,
                            SDI_REPLICATION_NAME_MAX_SIZE + 1,
                            "%s#%s#PR",                   /* nodename size = 10 *2 , + # + # + PR => 24 */
                            sFromNodeName,
                            sToNodeName);
            sIsPrimaryDataSync = ID_TRUE;
        }
        else if ( sdi::isSameNode(sToNodeName, sFromReplicaSetPtr->mFirstBackupNodeName) == ID_TRUE )
        {
            /* the resharding target(to) node of primary data is first backup node
             * the primary data is in backup table of to node, therefore, the primary data does not need sync
             */
            idlOS::strncpy(sPrimaryReplicationName, sFromReplicaSetPtr->mFirstReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsPrimaryDataSync = ID_FALSE;
            sIsFirstBackupOfFromNodeTruncate = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToNodeName, sFromReplicaSetPtr->mSecondBackupNodeName) == ID_TRUE )
        {
            /* the resharding target(to) node of primary data is second backup node
             * the primary data is in backup table of to node, therefore,  the primary data does not need sync
             */
            idlOS::strncpy(sPrimaryReplicationName, sFromReplicaSetPtr->mSecondReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsPrimaryDataSync = ID_FALSE;
            sIsSecondBackupOfFromNodeTruncate = ID_FALSE;
        }
        else /* not first or second backup node */
        {
            idlOS::snprintf(sPrimaryReplicationName,
                            SDI_REPLICATION_NAME_MAX_SIZE + 1,
                            "%s#%s#PR",                   /* nodename size = 10 *2 , + # + # + PR => 24 */
                            sFromNodeName,
                            sToNodeName);
            sIsPrimaryDataSync = ID_TRUE;
        }
    }

    /* 6. Determine whether the first backup data should be synchronized. */

    if ( SDI_IS_NULL_NAME(sToReplicaSetPtr->mFirstBackupNodeName) == ID_TRUE )
    {
        SDI_SET_NULL_NAME(sFirstBackupReplicationName);
        sIsFirstBackupDataSync = ID_FALSE;
    }
    else
    {
        if ( sParseTree->mDDLType == SHARD_MOVE_TO_INTERNAL_CLONE_COPY )
        {
            SDI_SET_NULL_NAME(sFirstBackupReplicationName);
            sIsFirstBackupDataSync = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mFirstBackupNodeName, sFromReplicaSetPtr->mFirstBackupNodeName) == ID_TRUE )
        {
            idlOS::strncpy(sFirstBackupReplicationName, sFromReplicaSetPtr->mFirstReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsFirstBackupDataSync = ID_FALSE;
            sIsFirstBackupOfFromNodeTruncate = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mFirstBackupNodeName, sFromReplicaSetPtr->mSecondBackupNodeName) == ID_TRUE )
        {
            idlOS::strncpy(sFirstBackupReplicationName, sFromReplicaSetPtr->mSecondReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsFirstBackupDataSync = ID_FALSE;
            sIsSecondBackupOfFromNodeTruncate = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mFirstBackupNodeName, sFromNodeName) == ID_TRUE )
        {
            SDI_SET_NULL_NAME(sFirstBackupReplicationName);
            sIsFirstBackupDataSync = ID_FALSE;
            sIsFromNodeTruncate = ID_FALSE;
        }
        else
        {
            idlOS::snprintf(sFirstBackupReplicationName,
                            SDI_REPLICATION_NAME_MAX_SIZE + 1,
                            "%s#%s#FB",                   /* nodename size = 10 *2 , + # + # + FB => 24 */
                            sFromNodeName,
                            sToReplicaSetPtr->mFirstBackupNodeName);
            sIsFirstBackupDataSync = ID_TRUE;
        }
    }
    /* 7. Determine whether the second backup data should be synchronized. */
    if ( SDI_IS_NULL_NAME(sToReplicaSetPtr->mSecondBackupNodeName) == ID_TRUE )
    {
        SDI_SET_NULL_NAME(sSecondBackupReplicationName);
        sIsSecondBackupDataSync = ID_FALSE;
    }
    else
    {
        if ( sParseTree->mDDLType == SHARD_MOVE_TO_INTERNAL_CLONE_COPY )
        {
            SDI_SET_NULL_NAME(sSecondBackupReplicationName);
            sIsSecondBackupDataSync = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mSecondBackupNodeName, sFromReplicaSetPtr->mFirstBackupNodeName) == ID_TRUE )
        {
            idlOS::strncpy(sSecondBackupReplicationName, sFromReplicaSetPtr->mFirstReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsSecondBackupDataSync = ID_FALSE;
            sIsFirstBackupOfFromNodeTruncate = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mSecondBackupNodeName, sFromReplicaSetPtr->mSecondBackupNodeName) == ID_TRUE )
        {
            idlOS::strncpy(sSecondBackupReplicationName, sFromReplicaSetPtr->mSecondReplName, SDI_REPLICATION_NAME_MAX_SIZE + 1);
            sIsSecondBackupDataSync = ID_FALSE;
            sIsSecondBackupOfFromNodeTruncate = ID_FALSE;
        }
        else if ( sdi::isSameNode(sToReplicaSetPtr->mSecondBackupNodeName, sFromNodeName) == ID_TRUE )
        {
            SDI_SET_NULL_NAME(sSecondBackupReplicationName);
            sIsSecondBackupDataSync = ID_FALSE;
            sIsFromNodeTruncate = ID_FALSE;
        }
        else
        {
            idlOS::snprintf(sSecondBackupReplicationName,
                            SDI_REPLICATION_NAME_MAX_SIZE + 1,
                            "%s#%s#SB",                   /* nodename size = 10 *2 , + # + # + SB => 24 */
                            sFromNodeName,
                            sToReplicaSetPtr->mSecondBackupNodeName);
            sIsSecondBackupDataSync = ID_TRUE;
        }
    }

    /* 8. correct truncate flag: exception situation by k-safey and count of nodes and clone resharding */
    if ( sParseTree->mDDLType == SHARD_MOVE_TO_INTERNAL_CLONE_COPY )
    {
        sIsFromNodeTruncate = ID_FALSE;
        sIsFirstBackupOfFromNodeTruncate = ID_FALSE;
        sIsSecondBackupOfFromNodeTruncate = ID_FALSE;
    }
    else
    {
        if ( ( sIsFirstBackupOfFromNodeTruncate == ID_TRUE ) &&
             ( SDI_IS_NULL_NAME(sFromReplicaSetPtr->mFirstBackupNodeName) == ID_TRUE ) )
        {
            sIsFirstBackupOfFromNodeTruncate = ID_FALSE;
        }

        if ( ( sIsSecondBackupOfFromNodeTruncate == ID_TRUE ) &&
             ( SDI_IS_NULL_NAME(sFromReplicaSetPtr->mSecondBackupNodeName) == ID_TRUE ) )
        {
            sIsSecondBackupOfFromNodeTruncate = ID_FALSE;
        }
    }

    IDE_DASSERT(SDI_IS_NULL_NAME(sFromNodeName) != ID_TRUE);

    /* start resharding operation */
    IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );
    IDE_TEST_RAISE( aStatement->session->mQPSpecific.mClientInfo == NULL, ERR_NOT_EXIST_NODE );

    /* until here, prepare resharding with new transaction
     * from the logic below to commit transaction is one transaction.
     */
    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    if ( sIsPrimaryDataSync == ID_TRUE )
    {
        // BUG-48616
        if ( sdi::isShardDDLForAddClone( aStatement ) == ID_TRUE )
        {
            IDE_TEST( checkCloneTableDataAndTruncateForShardAdd( aStatement,
                                                                 sParseTree->mReShardAttr )
                      != IDE_SUCCESS);
        }
        else
        {
            IDE_TEST(clearBackupDataWithNewTrans( aStatement->session->mMmSession,
                                                  sToNodeName,
                                                  sParseTree->mReShardAttr )
                     != IDE_SUCCESS);
        }
        
        IDE_TEST( prepareReplicationsForSync( aStatement,
                                              sParseTree->mNodeInfoList,
                                              sFromNodeName,
                                              sToNodeName,
                                              sPrimaryReplicationName,
                                              sParseTree->mReShardAttr )
                  != IDE_SUCCESS );

        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                        sPrimaryReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_ROLLBACK,
                                          QCI_STMT_MASK_MAX );
        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                        sPrimaryReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_ROLLBACK,
                                          QCI_STMT_MASK_MAX );
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sToNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_MAX );
    }

    if ( sIsFirstBackupDataSync == ID_TRUE )
    {
        IDE_TEST(clearBackupDataWithNewTrans( aStatement->session->mMmSession,
                                              sToReplicaSetPtr->mFirstBackupNodeName,
                                              sParseTree->mReShardAttr )
                 != IDE_SUCCESS);
        IDE_TEST( prepareReplicationsForSync( aStatement,
                                              sParseTree->mNodeInfoList,
                                              sFromNodeName,
                                              sToReplicaSetPtr->mFirstBackupNodeName,
                                              sFirstBackupReplicationName,
                                              sParseTree->mReShardAttr )
                  != IDE_SUCCESS );

        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                        sFirstBackupReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_ROLLBACK,
                                          QCI_STMT_MASK_MAX );
        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                        sFirstBackupReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_ROLLBACK,
                                          QCI_STMT_MASK_MAX );
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sToReplicaSetPtr->mFirstBackupNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_MAX );
    }

    if ( sIsSecondBackupDataSync == ID_TRUE )
    {
        IDE_TEST(clearBackupDataWithNewTrans( aStatement->session->mMmSession,
                                              sToReplicaSetPtr->mSecondBackupNodeName,
                                              sParseTree->mReShardAttr )
                 != IDE_SUCCESS);
        IDE_TEST( prepareReplicationsForSync( aStatement,
                                              sParseTree->mNodeInfoList,
                                              sFromNodeName,
                                              sToReplicaSetPtr->mSecondBackupNodeName,
                                              sSecondBackupReplicationName,
                                              sParseTree->mReShardAttr )
                  != IDE_SUCCESS );

        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                        sSecondBackupReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_END_ALL,
                                          QCI_STMT_MASK_MAX );
        idlOS::snprintf(sSqlStr,
                        QD_MAX_SQL_LENGTH,
                        "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                        sSecondBackupReplicationName);
        (void)sdiZookeeper::addPendingJob(sSqlStr,
                                          sFromNodeName,
                                          ZK_PENDING_JOB_AFTER_END_ALL,
                                          QCI_STMT_MASK_MAX );
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sToReplicaSetPtr->mSecondBackupNodeName,
                                           ZK_PENDING_JOB_AFTER_END_ALL,
                                           QCI_STMT_MASK_MAX );
    }

    /* 1. there is serial sync problem , but it goes to serial this time */
    /* 2. there is a conditional sync performance problem, reviewreview
     *    A user may want to synchronize parallel. 
     *    but currently, the conditional sync do not support parallel sync.
     */
    if ( sIsPrimaryDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" sync conditional",
                                                   sPrimaryReplicationName) != IDE_SUCCESS );
    }
    if ( sIsFirstBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" sync conditional",
                                                   sFirstBackupReplicationName) != IDE_SUCCESS );
    }
    if ( sIsSecondBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" sync conditional",
                                                   sSecondBackupReplicationName) != IDE_SUCCESS );
    }

    if ( SDI_IS_NULL_NAME(sPrimaryReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sPrimaryReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    if ( SDI_IS_NULL_NAME(sFirstBackupReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sFirstBackupReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    if ( SDI_IS_NULL_NAME(sSecondBackupReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sSecondBackupReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    /* until here, data sync end. 
     * table locks to execute resharding main swap operation
     */
    IDE_TEST( lockAllRemoteReshardTables( aStatement,
                                          sSqlStr,
                                          QD_MAX_SQL_LENGTH,
                                          sFromNodeName,
                                          sToNodeName,
                                          sParseTree->mReShardAttr ) != IDE_SUCCESS );

    if ( SDI_IS_NULL_NAME(sPrimaryReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sPrimaryReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    if ( SDI_IS_NULL_NAME(sFirstBackupReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sFirstBackupReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    if ( SDI_IS_NULL_NAME(sSecondBackupReplicationName) != ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" flush wait %"ID_UINT32_FMT,
                                                   sSecondBackupReplicationName,
                                                   (sShardDDLLockTryCount * sRemoteLockTimeout)
                                                   ) != IDE_SUCCESS );
    }

    if ( sIsPrimaryDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                                   sPrimaryReplicationName) != IDE_SUCCESS );
    }
    if ( sIsFirstBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                                   sFirstBackupReplicationName) != IDE_SUCCESS );
    }
    if ( sIsSecondBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQLWithNewTransArg( aStatement,
                                                   sFromNodeName,
                                                   SDI_INTERNAL_OP_NORMAL,
                                                   QCI_STMT_MASK_DCL,
                                                   sSqlStr,
                                                   QD_MAX_SQL_LENGTH,
                                                   ID_FALSE,
                                                   "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                                   sSecondBackupReplicationName) != IDE_SUCCESS );
    }
    
    /* temp replication drop: the drop replication operation must be executed by the transaction that was acquired lock. */
    if ( sIsPrimaryDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sFromNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sPrimaryReplicationName) != IDE_SUCCESS );
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sToNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sPrimaryReplicationName) != IDE_SUCCESS );
    }
    if ( sIsFirstBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sFromNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sFirstBackupReplicationName) != IDE_SUCCESS );
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sToReplicaSetPtr->mFirstBackupNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sFirstBackupReplicationName) != IDE_SUCCESS );
    }
    if ( sIsSecondBackupDataSync == ID_TRUE )
    {
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sFromNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sSecondBackupReplicationName) != IDE_SUCCESS );
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sToReplicaSetPtr->mSecondBackupNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DDL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                    sSecondBackupReplicationName) != IDE_SUCCESS );
    }
    
    IDE_TEST( resetShardMetaAndSwapPartition( aStatement,
                                              sSqlStr,
                                              QD_MAX_SQL_LENGTH,
                                              sFromNodeName,
                                              sToNodeName,
                                              sParseTree->mNodeInfoList,
                                              sParseTree->mReShardAttr) != IDE_SUCCESS );
    /* BUG-48586 */
    IDE_TEST( executeRemoteSQL( aStatement,
                                sToNodeName,
                                SDI_INTERNAL_OP_NORMAL,
                                QCI_STMT_MASK_SP,
                                sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "exec dbms_shard.set_ReplicaSCN()") != IDE_SUCCESS );

    /* replicaset move
     * logical move: the replicaset of resharding item move to replicaset of tonode through updating replicaset id: it was updated when reset_shard_partition(resident)_node execute
     * physical move: replication drop and add
     */
    if ( (sLocalMetaInfo.mKSafety > 0) && (sParseTree->mDDLType != SHARD_MOVE_TO_INTERNAL_CLONE_COPY) )
    {
        IDE_TEST( moveReplication( aStatement,
                                   sSqlStr,
                                   QD_MAX_SQL_LENGTH,
                                   sLocalMetaInfo.mKSafety,
                                   sFromReplicaSetPtr,
                                   sToReplicaSetPtr,
                                   sParseTree->mReShardAttr ) != IDE_SUCCESS );
    }

    /* truncate garbage backup table  */
    if ( sIsFromNodeTruncate == ID_TRUE )
    {
        IDE_TEST( truncateAllBackupData( aStatement,
                                         sSqlStr,
                                         QD_MAX_SQL_LENGTH,
                                         sFromNodeName,
                                         sParseTree->mReShardAttr,
                                         ZK_PENDING_JOB_NONE ) != IDE_SUCCESS );
    }

    if ( sIsFirstBackupOfFromNodeTruncate == ID_TRUE )
    {
        IDE_TEST( truncateAllBackupData( aStatement,
                                         sSqlStr,
                                         QD_MAX_SQL_LENGTH,
                                         sFromReplicaSetPtr->mFirstBackupNodeName,
                                         sParseTree->mReShardAttr,
                                         ZK_PENDING_JOB_NONE ) != IDE_SUCCESS );
    }

    if ( sIsSecondBackupOfFromNodeTruncate == ID_TRUE )
    {
        IDE_TEST( truncateAllBackupData( aStatement,
                                         sSqlStr,
                                         QD_MAX_SQL_LENGTH,
                                         sFromReplicaSetPtr->mSecondBackupNodeName,
                                         sParseTree->mReShardAttr,
                                         ZK_PENDING_JOB_NONE ) != IDE_SUCCESS );
    }

    IDE_TEST( truncateAllBackupData( aStatement,
                                     sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     sToNodeName,
                                     sParseTree->mReShardAttr,
                                     ZK_PENDING_JOB_NONE ) != IDE_SUCCESS );

    if ( sdi::isShardDDLForAddClone( aStatement ) != ID_TRUE )
    {                
        sdiZookeeper::removePendingJob();
    }
            
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_NODE );
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION( ERR_SAME_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SAME_NODE_NAME, sToNodeName, sFromNodeName ) );
    }
    IDE_EXCEPTION( ERR_SHARD_META );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_REPLICA_SETS ) );
    }
    IDE_EXCEPTION( ERR_NOT_FOUND_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_NODE_NAME, sToNodeName ) );
    }
    IDE_EXCEPTION( ERR_INVALID_CONDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeShardMove", "Invalid condition." ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    /* clear temporary replication for sync */
    /* job for truncate garbage backup table add to zookeeper pending job*/
    if ( sIsPrimaryDataSync == ID_TRUE )
    {
         (void)truncateAllBackupData( aStatement,
                                      sSqlStr,
                                      QD_MAX_SQL_LENGTH,
                                      sToNodeName,
                                      sParseTree->mReShardAttr,
                                      ZK_PENDING_JOB_AFTER_ROLLBACK );
    }

    if ( sIsFirstBackupDataSync == ID_TRUE )
    {
        (void)truncateAllBackupData( aStatement,
                                     sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     sToReplicaSetPtr->mFirstBackupNodeName,
                                     sParseTree->mReShardAttr,
                                     ZK_PENDING_JOB_AFTER_ROLLBACK );
    }

    if ( sIsSecondBackupDataSync == ID_TRUE )
    {
        (void)truncateAllBackupData( aStatement,
                                     sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     sToReplicaSetPtr->mSecondBackupNodeName,
                                     sParseTree->mReShardAttr,
                                     ZK_PENDING_JOB_AFTER_ROLLBACK );
    }
    IDE_POP();
    return IDE_FAILURE;
}
/**
 * aSmiStmt(IN): executing smistatement of transaction using for shard meta upate
 * aOutNewSMN(OUT): return updated SMN value
 */
IDE_RC qdsd::getIncreasedSMN( qcStatement * aStatement, ULong * aOutNewSMN )
{
    idBool sIsSmiStmtEnd = ID_FALSE;
    smiStatement * sSmiStmt = QC_SMI_STMT(aStatement);
    smiStatement * spRootStmt = sSmiStmt->getTrans()->getStatement();
    UInt sSmiStmtFlag = sSmiStmt->mFlag;

    IDE_TEST( sSmiStmt->end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );
    sIsSmiStmtEnd = ID_TRUE;

    IDE_TEST( sdi::getIncreasedSMNForMetaNode( sSmiStmt->getTrans(), aOutNewSMN ) != IDE_SUCCESS );

    IDE_TEST( sSmiStmt->begin( aStatement->mStatistics,
                               spRootStmt,
                               sSmiStmtFlag )
              != IDE_SUCCESS );
    sIsSmiStmtEnd = ID_FALSE;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    if( sIsSmiStmtEnd == ID_TRUE )
    {
        IDE_ASSERT( sSmiStmt->begin( aStatement->mStatistics,
                                    spRootStmt,
                                    SMI_STATEMENT_NORMAL|SMI_STATEMENT_ALL_CURSOR ) 
                    == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::validateCommon( qcStatement * aStatement )
{
    sdiLocalMetaInfo sLocalMetaInfo;
    UInt             sPropertyValue = 0;
    SInt             sPropertyStrIdx = 0;

    const SChar    * sReplPropertyStr[] = { "REPLICATION_DDL_ENABLE",
                                            "REPLICATION_DDL_ENABLE_LEVEL",
                                            "REPLICATION_SQL_APPLY_ENABLE",
                                            "REPLICATION_ALLOW_DUPLICATE_HOSTS",
                                            NULL };
    IDE_CLEAR();

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    
    // check OS
#ifndef ALTI_CFG_OS_LINUX
    IDE_RAISE( ERR_ONLY_ACCEPT_LINUX );
#endif   
    // check SYS
    IDE_TEST_RAISE( QCG_GET_SESSION_USER_ID(aStatement) != QC_SYS_USER_ID,
                    ERR_NO_ALTER_DATABASE_SHARD );

    // check SHARD_ENABLE = 1
    IDE_TEST_RAISE( SDU_SHARD_ENABLE != 1, ERR_SHARD_ENABLE );

    // check SHARD META
    IDE_TEST_RAISE( sdi::checkMetaCreated() != ID_TRUE, ERR_SYS_SHARD_META );
    
    // check AUTOCOMMIT OFF
    // R2HA ���� default autocommit off�� ���� �Ǹ� ���� �ض�
    IDE_TEST_RAISE( QCG_GET_SESSION_IS_AUTOCOMMIT( aStatement ) == ID_TRUE,
                    ERR_AUTOCOMMIT_MDOE );

    if ( sLocalMetaInfo.mKSafety != 0 )
    {
        for( sPropertyStrIdx = 0; sReplPropertyStr[sPropertyStrIdx] != NULL; sPropertyStrIdx++ )
        {
            /* read 4byte property */
            IDE_TEST(idp::read(sReplPropertyStr[sPropertyStrIdx], &sPropertyValue) != IDE_SUCCESS);
            IDE_TEST_RAISE( sPropertyValue != 1, ERR_REPL_PROPERTY );
        }
    }
    
    // set SHARD_DDL session flag
    aStatement->session->mQPSpecific.mFlag &= ~QC_SESSION_SHARD_DDL_MASK;
    aStatement->session->mQPSpecific.mFlag |= QC_SESSION_SHARD_DDL_TRUE;

    return IDE_SUCCESS;
    
#ifndef ALTI_CFG_OS_LINUX
    IDE_EXCEPTION( ERR_ONLY_ACCEPT_LINUX );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ZKC_NOT_SUPPORT_OS ) );
    }
#endif
    IDE_EXCEPTION( ERR_NO_ALTER_DATABASE_SHARD );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_ALTER_DATABASE_SHARD ) );
    }    
    IDE_EXCEPTION( ERR_AUTOCOMMIT_MDOE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INSUFFICIENT_ATTRIBUTE,
                                "AUTOCOMMIT = FALSE" ) );
    }
    IDE_EXCEPTION( ERR_SHARD_ENABLE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INSUFFICIENT_ATTRIBUTE,
                                "SHARD_ENABLE = 1" ) );
    }
    IDE_EXCEPTION( ERR_SYS_SHARD_META );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( ERR_REPL_PROPERTY );
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_PROPERTY_FOR_SHARDING, sReplPropertyStr[sPropertyStrIdx] ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::validateCommonForExecute( qcStatement * aStatement )
{
    // check �Ǵ� ������Ƽ�� shard ddl���� internal�� ���� �ȴ�.
    // ���� ������ execute ���� ���� ���� �ǹǷ� validate check�� execute���� ��
    
    // check GLOBAL_TRANSACTION_LEVEL = 2
    IDE_TEST_RAISE( QCG_GET_SESSION_GTX_LEVEL( aStatement ) < 2,
                    ERR_GLOBAL_TRANSACTION_LEVEL );

    // check DDL TRANSACTION = 1
    IDE_TEST_RAISE ( QCG_GET_SESSION_TRANSACTIONAL_DDL( aStatement) != ID_TRUE,
                     ERR_DDL_TRANSACTION );

    aStatement->session->mQPSpecific.mFlag &= ~QC_SESSION_SHARD_DDL_MASK;
    aStatement->session->mQPSpecific.mFlag |= QC_SESSION_SHARD_DDL_TRUE;

    /* BUG-48290 */
    IDE_TEST( qcg::setSessionIsInternalLocalOperation( aStatement, SDI_INTERNAL_OP_NORMAL) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GLOBAL_TRANSACTION_LEVEL );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INSUFFICIENT_ATTRIBUTE,
                                "GLOBAL_TRANSACTION_LEVEL = 2" ) );
    }
    IDE_EXCEPTION( ERR_DDL_TRANSACTION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INSUFFICIENT_ATTRIBUTE,
                                 "TRANSACTIONAL_DDL = 1" ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeSetNodes( qcStatement * aStatement )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1]        = {0,};
    sdiClientInfo     * sClientInfo  = NULL;
    sdiConnectInfo    * sConnectInfo = NULL;
    UShort              i            = 0;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    // LOCAL_META_INFO
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
              != IDE_SUCCESS );

    IDE_TEST( executeLocalSQL( aStatement, 
                               QCI_STMT_MASK_SP,
                               sSqlStr,
                               QD_MAX_SQL_LENGTH,
                               "EXEC DBMS_SHARD.SET_NODE( '"QCM_SQL_STRING_SKIP_FMT"', VARCHAR'%s', INTEGER'%"ID_INT32_FMT"',NULL, NULL, NULL, INTEGER'%"ID_INT32_FMT"' ); ",
                               sLocalMetaInfo.mNodeName,
                               sLocalMetaInfo.mHostIP,
                               sLocalMetaInfo.mPortNo,
                               sLocalMetaInfo.mShardNodeId )
              != IDE_SUCCESS );

    /* it don't need remote execute when first add node */
    if ( sParseTree->mNodeCount > 1 )
    {
        IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

        sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

        sConnectInfo = sClientInfo->mConnectInfo;

        for ( i = 0; i < sClientInfo->mCount; i++, sConnectInfo++ )
        {
            if ( idlOS::strMatch( sLocalMetaInfo.mNodeName,
                                  idlOS::strlen( sLocalMetaInfo.mNodeName ),
                                  sConnectInfo->mNodeName,
                                  idlOS::strlen( sConnectInfo->mNodeName )) != 0 )
            {
                IDE_DASSERT ( QC_IS_NULL_NAME( sParseTree->mNodeName ) == ID_TRUE );
                IDE_TEST( executeRemoteSQL( aStatement, 
                                            sConnectInfo->mNodeName,
                                            SDI_INTERNAL_OP_NORMAL,
                                            QCI_STMT_MASK_SP,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                               "EXEC DBMS_SHARD.SET_NODE( '"QCM_SQL_STRING_SKIP_FMT"', VARCHAR'%s', INTEGER'%"ID_INT32_FMT"',NULL, NULL, NULL, INTEGER'%"ID_INT32_FMT"' ); ",
                                            sLocalMetaInfo.mNodeName,
                                            sLocalMetaInfo.mHostIP,
                                            sLocalMetaInfo.mPortNo,
                                            sLocalMetaInfo.mShardNodeId )
                            != IDE_SUCCESS);
            }
            else
            {
                    // nothing to do
            }
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeUnsetNodes( qcStatement * aStatement )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1]        = {0,};
    sdiClientInfo     * sClientInfo  = NULL;
    sdiConnectInfo    * sConnectInfo = NULL;
    UShort              i            = 0;
    
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    // LOCAL_META_INFO
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
              != IDE_SUCCESS );

    IDE_TEST_RAISE ( sParseTree->mNodeCount < 1 , ERR_INVALID_CONDITION );
    IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

    IDE_TEST( executeLocalSQL( aStatement, 
                               QCI_STMT_MASK_SP,
                               sSqlStr,
                               QD_MAX_SQL_LENGTH,
                               "EXEC DBMS_SHARD.UNSET_NODE( '"QCM_SQL_STRING_SKIP_FMT"' ); ",
                               sLocalMetaInfo.mNodeName )
              != IDE_SUCCESS );

    sClientInfo = aStatement->session->mQPSpecific.mClientInfo;
    sConnectInfo = sClientInfo->mConnectInfo;

    for ( i = 0; i < sClientInfo->mCount; i++, sConnectInfo++ )
    {
        if ( idlOS::strMatch( sLocalMetaInfo.mNodeName,
                              idlOS::strlen( sLocalMetaInfo.mNodeName ),
                              sConnectInfo->mNodeName,
                              idlOS::strlen( sConnectInfo->mNodeName )) != 0 )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        sConnectInfo->mNodeName,
                                        SDI_INTERNAL_OP_NORMAL,
                                        QCI_STMT_MASK_SP,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        "EXEC DBMS_SHARD.UNSET_NODE( '"QCM_SQL_STRING_SKIP_FMT"' ); ",
                                        sLocalMetaInfo.mNodeName )
                        != IDE_SUCCESS);
        }
        else
        {
                    // nothing to do
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_CONDITION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeUnSetNode", "Invalid condition." ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qdsd::executeZookeeperAdd()
{
    sdiLocalMetaInfo      sLocalMetaInfo;
    idBool                sIsMetaInited = ID_FALSE;
    SInt                  sDataLength = 0;
    /* zookeeper meta ���Կ� */
    SChar                 sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar                 sShardID[SDI_ZKC_SHARD_ID_SIZE] = {0,};
    SChar                 sPath[SDI_ZKC_PATH_LENGTH] = SDI_ZKC_PATH_ALTIBASE_SHARD;
    SChar                 sNodeIPnPort[SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE] = {0,};
    SChar                 sInternalNodeIPnPort[SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE] = {0,};
    SChar                 sInternalReplicationHostIPnPort[SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE] = {0,};
    SChar                 sConnType[10] = {0,};

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    /* Ŭ������ ��Ÿ�� �����Ǿ��ִ��� Ȯ���Ѵ�. */
    IDE_TEST( sdiZookeeper::checkPath( sPath,
                                       &sIsMetaInited ) != IDE_SUCCESS );
    /* 1. shardID */
    idlOS::snprintf( sShardID, SDI_ZKC_BUFFER_SIZE, "%"ID_INT32_FMT, sLocalMetaInfo.mShardNodeId );

    /* 2. Host IP / Port */
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT32_FMT, sLocalMetaInfo.mPortNo );
    idlOS::strncpy( sNodeIPnPort,
                    sLocalMetaInfo.mHostIP,
                    SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE );
    idlOS::strncat( sNodeIPnPort,
                    ":",
                    ID_SIZEOF( sNodeIPnPort ) - idlOS::strlen( sNodeIPnPort ) - 1 );
    idlOS::strncat( sNodeIPnPort,
                    sBuffer,
                    ID_SIZEOF( sNodeIPnPort ) - idlOS::strlen( sNodeIPnPort ) - 1 );

    /* 3. Internal Host IP / Port */
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT32_FMT, sLocalMetaInfo.mInternalPortNo );
    idlOS::strncpy( sInternalNodeIPnPort,
                    sLocalMetaInfo.mInternalHostIP,
                    SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE );
    idlOS::strncat( sInternalNodeIPnPort,
                    ":",
                    ID_SIZEOF( sInternalNodeIPnPort ) - idlOS::strlen( sInternalNodeIPnPort ) - 1 );
    idlOS::strncat( sInternalNodeIPnPort,
                    sBuffer,
                    ID_SIZEOF( sInternalNodeIPnPort ) - idlOS::strlen( sInternalNodeIPnPort ) - 1 );

    /* 4. Internal Replication Host IP / Port */
    idlOS::memset( sBuffer, 0, SDI_PORT_NUM_BUFFER_SIZE );
    idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT32_FMT, sLocalMetaInfo.mInternalRPPortNo );
    idlOS::strncpy( sInternalReplicationHostIPnPort,
                    sLocalMetaInfo.mInternalRPHostIP,
                    SDI_SERVER_IP_SIZE + SDI_PORT_NUM_BUFFER_SIZE );
    idlOS::strncat( sInternalReplicationHostIPnPort,
                    ":",
                    ID_SIZEOF( sInternalReplicationHostIPnPort ) - idlOS::strlen( sInternalReplicationHostIPnPort ) - 1 );
    idlOS::strncat( sInternalReplicationHostIPnPort,
                    sBuffer,
                    ID_SIZEOF( sInternalReplicationHostIPnPort ) - idlOS::strlen( sInternalReplicationHostIPnPort ) - 1 );

    /* 5. Internal Conn Type */
    idlOS::memset( sConnType, 0, 10 );
    idlOS::snprintf( sConnType, 10, "%"ID_UINT64_FMT, sLocalMetaInfo.mInternalConnType );
    /* zookeeper server�� ��� ��Ÿ ������ �߰��Ѵ�. */
    IDE_TEST( sdiZookeeper::addNode( sShardID,
                                     sNodeIPnPort,
                                     sInternalNodeIPnPort,
                                     sInternalReplicationHostIPnPort,
                                     sConnType ) != IDE_SUCCESS );

    /* ���� ��Ÿ�� �����ϰ� �� ��Ÿ ������ replication ���� ������ ����ִٸ� �����´�. */
    /* BUGBUG : ����� sdiLocalMetaInfo�� replication ���� ������ �����Ƿ� �ϴ� �ּ�ó���� ������ *
     *          ���� replication ������ �߰��Ǹ� �׿� ���� �����ؾ� �Ѵ�.                         */
    /* BUGBUG : ���� replication ������ ����ִ� ���� ��� Ȯ�� �� �������� ���� ������ �ʿ��ϴ�. */
    if( sIsMetaInited == ID_TRUE )
//    if( ( sIsMetaInited == ID_TRUE ) &&
//        ( sLocalMetaInfo.mKSafety == 0 ) &&
//        ( sLocalMetaInfo.mReplicationMode == 0 ) &&
//        ( sLocalMetaInfo.mParallelCount == 0 ) )

    {
        // k-safety ���� �����´�.
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        IDE_TEST( sdiZookeeper::readInfo( (SChar*)SDI_ZKC_PATH_K_SAFETY,
                                          sBuffer,
                                          &sDataLength ) != IDE_SUCCESS );
//        sLocalMetaInfo.mKSafety = idlOS::atoi( sBuffer );

        // replication mode ���� �����´�.
        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        IDE_TEST( sdiZookeeper::readInfo( (SChar*)SDI_ZKC_PATH_REP_MODE,
                                          sBuffer,
                                          &sDataLength ) != IDE_SUCCESS );
//        sLocalMetaInfo.mReplicationMode = idlOS::atoi( sBuffer );

        // parallel count ���� �����´�.
        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        IDE_TEST( sdiZookeeper::readInfo( (SChar*)SDI_ZKC_PATH_PARALLEL_CNT,
                                          sBuffer,
                                          &sDataLength ) != IDE_SUCCESS );
//        sLocalMetaInfo.mParallelCount = idlOS::atoi( sBuffer );

        //������ ������ ��Ÿ�� �߰��ؾ� �Ѵ�.
    }

    /* TASK-7217 Sharded sequence */
    qcm::setShardNodeID(sLocalMetaInfo.mShardNodeId);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();
    
    return IDE_FAILURE;
}

IDE_RC qdsd::executeZookeeperJoin()
{
    sdiLocalMetaInfo      sLocalMetaInfo;

    IDE_TEST( sdiZookeeper::joinNode() != IDE_SUCCESS );

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    /* TASK-7217 Sharded sequence */
    qcm::setShardNodeID(sLocalMetaInfo.mShardNodeId);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();
    
    return IDE_FAILURE;
}

IDE_RC qdsd::executeZookeeperDrop( qcStatement * aQcStmt )
{
    SChar   sNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    idBool  sIsSelfDrop = ID_FALSE;
    qdShardParseTree * sParseTree = (qdShardParseTree *)aQcStmt->myPlan->parseTree;

    if( QC_IS_NULL_NAME( sParseTree->mNodeName ) == ID_TRUE )
    {
        sIsSelfDrop = ID_TRUE;
        /* ������ ��� �̸��� ���ٸ� �� ��带 �����Ѵ�. */
        IDE_TEST( sdiZookeeper::dropNode( sdiZookeeper::mMyNodeName ) != IDE_SUCCESS );
    }
    else
    {
        /* ������ ��� �̸��� �ִٸ� �ش� ��带 �����Ѵ�. */
        QC_STR_COPY( sNodeName, sParseTree->mNodeName );

        if ( idlOS::strMatch(sdiZookeeper::mMyNodeName, idlOS::strlen(sdiZookeeper::mMyNodeName),
                             sNodeName,                 idlOS::strlen(sNodeName)) == 0 )
        {
            sIsSelfDrop = ID_TRUE;
        }

        IDE_TEST( sdiZookeeper::dropNode( sNodeName ) != IDE_SUCCESS );
    }

    /* drop�Ϸ��� �ϴ� ��忡 �� session�� �޶���� ���ϰ� ���ƾ� �Ѵ�. */
    idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)1, 0, NULL );
    sdi::setShardStatus(0); // SHARD_STATUS 1 -> 0

    /* ������ ����Ǿ��ִ� session�� �����Ѵ�. */
    IDE_TEST_RAISE( sdi::closeSessionForShardDrop( aQcStmt ) != IDE_SUCCESS, errSessionClose );

    /* unsetShardNodeID�� commit�� �Ŀ� ����Ǿ�� �ϹǷ� sdiZookeeper::callAfterCommitFunc���� �����Ѵ�. */

    return IDE_SUCCESS;

    IDE_EXCEPTION( errSessionClose )
    {
        sdi::setShardStatus(1); // SHARD_STATUS 0 -> 1 
        idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)0, 0, NULL );
    }
    IDE_EXCEPTION_END;

    sdiZookeeper::releaseZookeeperMetaLock();

    return IDE_FAILURE;
}

/* validate ���� �̹� Z-Meta/S-Meta Lock�� ȹ���Ͽ���. */
IDE_RC qdsd::executeZookeeperFailover( SChar   * aTargetNodeName,
                                       SChar   * aFailoverNodeName,
                                       idBool  * aIsTargetAlive )
{
    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};

    /* failover ������ ����� �� ��尡 ��� �ִٸ� �ش� ��带 ������
     * failover�� �����ؾ� �ϹǷ� �ش� ��尡 ����ִ��� Ȯ���Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aTargetNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );

    /* Target�� ��� �ִٸ� Execute���� Stop ���� �־�� �Ѵ�. */
    IDE_TEST( sdiZookeeper::checkPath( sPath, aIsTargetAlive ) != IDE_SUCCESS );

    /* zookeeper ��Ÿ�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::failoverForQuery( aTargetNodeName,
                                              aFailoverNodeName ) != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeZookeeperFailback()
{
    sdiLocalMetaInfo      sLocalMetaInfo;

    /* zookeeper ��Ÿ�� �����Ѵ�. */
    IDE_TEST( sdiZookeeper::failback() != IDE_SUCCESS );

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    /* TASK-7217 Sharded sequence */
    qcm::setShardNodeID(sLocalMetaInfo.mShardNodeId);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* TASK-7217 Sharded sequence */
    qcm::unsetShardNodeID();

    return IDE_FAILURE;
}

IDE_RC qdsd::getAllNodeInfoList( qcStatement * aStatement )
{
    qdShardParseTree  * sParseTree = NULL;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( sdiZookeeper::getAllNodeInfoList( &sParseTree->mNodeInfoList,
                                                &sParseTree->mNodeCount,
                                                QC_QMX_MEM(aStatement) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qdsd::executeTemporaryReplication( qcStatement * aStatement )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo  * sRemoteLocalMetaInfo = NULL;
    sdiLocalMetaInfo    sCurrLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH];
    iduListNode       * sNode  = NULL;
    iduListNode       * sDummy = NULL;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sCurrLocalMetaInfo ) != IDE_SUCCESS );
    
    if ( sParseTree->mNodeCount > 1 )
    {
        //���� ����� node info
        if ( IDU_LIST_IS_EMPTY( sParseTree->mNodeInfoList ) != ID_TRUE )
        {
            IDU_LIST_ITERATE_SAFE( sParseTree->mNodeInfoList, sNode, sDummy )
            {
                sRemoteLocalMetaInfo = (sdiLocalMetaInfo *)sNode->mObj;
            
                if( idlOS::strncmp( sRemoteLocalMetaInfo->mNodeName,
                                    sCurrLocalMetaInfo.mNodeName,
                                    SDI_NODE_NAME_MAX_SIZE ) != 0 )
                {
                    break;
                }
            }
        }

        IDE_TEST_RAISE( sRemoteLocalMetaInfo == NULL, ERR_NOT_EXIST_LOCAL_META_INFO );
        
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_SHARD.GLOBAL_META_INFO_ " );

        IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                          sSqlStr )
                  != IDE_SUCCESS );

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "ALTER REPLICATION TEMPORARY GET TABLE "
                         "SYS_SHARD.NODES_, SYS_SHARD.OBJECTS_, SYS_SHARD.REPLICA_SETS_, "
                         "SYS_SHARD.SOLOS_, SYS_SHARD.CLONES_, SYS_SHARD.GLOBAL_META_INFO_, "
                         "SYS_SHARD.RANGES_,SYS_SHARD.FAILOVER_HISTORY_ "
                         "FROM '%s',%"ID_INT32_FMT" ",
                         sRemoteLocalMetaInfo->mInternalRPHostIP, // internal_host_ip
                         sRemoteLocalMetaInfo->mInternalRPPortNo ); // internal_port_no

        IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                          sSqlStr )
                  != IDE_SUCCESS );

        // ���� ���� ���� ���� SNM�� ����ȭ
        IDE_TEST( sdi::reloadSMNForDataNode( QC_SMI_STMT( aStatement ) )
                  != IDE_SUCCESS );
        
        // session SMN
        qci::mSessionCallback.mSetShardMetaNumber( aStatement->session->mMmSession,
                                                   sdi::getSMNForDataNode());
        
        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.OBJECTS_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );

        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.REPLICA_SETS_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );

        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.SOLOS_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );

        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.CLONES_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );
        
        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.RANGES_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );

        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "TRUNCATE TABLE SYS_SHARD.FAILOVER_HISTORY_" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DDL );

        // GLOBAL_META_INFO_ �� �ʱ� ������ update.
        idlOS::snprintf( sSqlStr,
                         QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_SHARD.GLOBAL_META_INFO_ SET ID = 0, SHARD_META_NUMBER = 1" );
        
        (void)sdiZookeeper::addPendingJob( sSqlStr,
                                           sCurrLocalMetaInfo.mNodeName,
                                           ZK_PENDING_JOB_AFTER_ROLLBACK,
                                           QCI_STMT_MASK_DML );

        // addpending����  SYS_SHARD.NODES_ �� ���� ���� �ʴ´�. (pending job�� ������� ����)
        // �ش� ��Ÿ ���� �� ���� ��忡 ���� executependingjob�� ���� �� �� ����.
        // cf> �ش� ������ shard add�ÿ� ���� �ȴ�.
    }
    else
    {
        // nothing to do
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_LOCAL_META_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "qdsd::executeTemporaryReplication",
                                  "Meta information of remote node does not exist in cluster meta." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qdsd::executeReplicationFunction( qcStatement       * aStatement,
                                         qdsdReplQueryType   aReplQueryType )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiReplicaSetInfo   sReplicaSetInfo;
    sdiReplicaSetInfo   sOldReplicaSetInfo;
    sdiReplicaSetInfo   sCurrReplicaSetInfo;
    sdiReplicaSetInfo   sPrevReplicaSetInfo;
    sdiReplicaSetInfo   sPrevPrevReplicaSetInfo;
    sdiReplicaSetInfo   sOldCurrReplicaSetInfo;
    sdiReplicaSetInfo   sOldPrevReplicaSetInfo;
    sdiReplicaSetInfo   sOldPrevPrevReplicaSetInfo;
    sdiGlobalMetaInfo   sMetaNodeInfo = { ID_ULONG(0) };
    ULong               sSMN = ID_ULONG(0);

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    SDI_INIT_REPLICA_SET( sReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sOldReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sCurrReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sPrevReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sPrevPrevReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sOldCurrReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sOldPrevReplicaSetInfo.mReplicaSets );
    SDI_INIT_REPLICA_SET( sOldPrevPrevReplicaSetInfo.mReplicaSets );
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );    

    IDE_TEST( sdi::getGlobalMetaInfoCore( QC_SMI_STMT( aStatement ),
                                          &sMetaNodeInfo )
              != IDE_SUCCESS );
    
    if ( sParseTree->mDDLType == SHARD_ADD )
    {
        // current SMN
        sSMN = sMetaNodeInfo.mShardMetaNumber;
    
        IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                          &sReplicaSetInfo,
                                                          sSMN )
                  != IDE_SUCCESS );

        IDE_TEST( findBackupReplicaSetInfo( sLocalMetaInfo.mNodeName,
                                            &sReplicaSetInfo,
                                            &sCurrReplicaSetInfo,
                                            &sPrevReplicaSetInfo,
                                            &sPrevPrevReplicaSetInfo )
                  != IDE_SUCCESS );


        switch( aReplQueryType )
        {
            case QDSD_REPL_QUERY_TYPE_STOP:
            case QDSD_REPL_QUERY_TYPE_DROP:
                
                // DROP OLD ������ �ȴ�.
                // old SMN, REPLICA_SETS_ ���ŵǱ� ���� DATA SMN���� GET
                sSMN = sdi::getSMNForDataNode();

                IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                                  &sOldReplicaSetInfo,
                                                                  sSMN )
                          != IDE_SUCCESS );

                if ( sOldReplicaSetInfo.mCount != 0 )
                {
                    IDE_TEST( findBackupReplicaSetInfo( sPrevReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                                        &sOldReplicaSetInfo,
                                                        &sOldCurrReplicaSetInfo,
                                                        &sOldPrevReplicaSetInfo,
                                                        &sOldPrevPrevReplicaSetInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( runForNodeReplicationQuery( aStatement,
                                                          &sOldReplicaSetInfo,
                                                          &sOldCurrReplicaSetInfo,
                                                          &sOldPrevReplicaSetInfo,
                                                          &sOldPrevPrevReplicaSetInfo,
                                                          aReplQueryType,
                                                          ID_TRUE )
                              != IDE_SUCCESS );
                }
                break;
            case QDSD_REPL_QUERY_TYPE_CREATE:
            case QDSD_REPL_QUERY_TYPE_ADD:
            case QDSD_REPL_QUERY_TYPE_START:
            case QDSD_REPL_QUERY_TYPE_FLUSH:
                if ( sReplicaSetInfo.mCount != 0 )
                {
                    IDE_TEST( runForNodeReplicationQuery( aStatement,
                                                          &sReplicaSetInfo,
                                                          &sCurrReplicaSetInfo,
                                                          &sPrevReplicaSetInfo,
                                                          &sPrevPrevReplicaSetInfo,
                                                          aReplQueryType,
                                                          ID_FALSE )
                              != IDE_SUCCESS );
                }
                break;
            default:
            IDE_DASSERT(0);
            break;
        }
    }
    else
    {
        // DROP OLD ������ �ȴ�.
        // old SMN, REPLICA_SETS_ ���ŵǱ� ���� DATA SMN���� GET
        sSMN = sdi::getSMNForDataNode();
        
        IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                          &sOldReplicaSetInfo,
                                                          sSMN )
                  != IDE_SUCCESS );

        IDE_TEST( findBackupReplicaSetInfo( sLocalMetaInfo.mNodeName,
                                            &sOldReplicaSetInfo,
                                            &sOldCurrReplicaSetInfo,
                                            &sOldPrevReplicaSetInfo,
                                            &sOldPrevPrevReplicaSetInfo )
                  != IDE_SUCCESS );

        switch( aReplQueryType )
        {
            case QDSD_REPL_QUERY_TYPE_STOP:
            case QDSD_REPL_QUERY_TYPE_DROP:
                if ( sOldReplicaSetInfo.mCount != 0 )
                {
                    IDE_TEST( runForNodeReplicationQuery( aStatement,
                                                          &sOldReplicaSetInfo,
                                                          &sOldCurrReplicaSetInfo,
                                                          &sOldPrevReplicaSetInfo,
                                                          &sOldPrevPrevReplicaSetInfo,
                                                          aReplQueryType,
                                                          ID_FALSE )
                              != IDE_SUCCESS );
                }
                break;
            case QDSD_REPL_QUERY_TYPE_CREATE:
            case QDSD_REPL_QUERY_TYPE_ADD:
            case QDSD_REPL_QUERY_TYPE_START:
            case QDSD_REPL_QUERY_TYPE_FLUSH:
                // current SMN
                sSMN = sMetaNodeInfo.mShardMetaNumber;
            
                IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                                  &sReplicaSetInfo,
                                                                  sSMN )
                          != IDE_SUCCESS );

                if ( sReplicaSetInfo.mCount != 0 )
                {
                    IDE_TEST( findBackupReplicaSetInfo( sOldPrevReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                                        &sReplicaSetInfo,
                                                        &sCurrReplicaSetInfo,
                                                        &sPrevReplicaSetInfo,
                                                        &sPrevPrevReplicaSetInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( runForNodeReplicationQuery( aStatement,
                                                          &sReplicaSetInfo,
                                                          &sCurrReplicaSetInfo,
                                                          &sPrevReplicaSetInfo,
                                                          &sPrevPrevReplicaSetInfo,
                                                          aReplQueryType,
                                                          ID_TRUE )
                              != IDE_SUCCESS );
                }
                break;
            default:
            IDE_DASSERT(0);
            break;
        }    
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::runForNodeReplicationQuery( qcStatement       * aStatement,
                                         sdiReplicaSetInfo * aReplicaSetInfo,
                                         sdiReplicaSetInfo * aCurrReplicaSetInfo,
                                         sdiReplicaSetInfo * aPrevReplicaSetInfo,
                                         sdiReplicaSetInfo * aPrevPrevReplicaSetInfo,
                                         qdsdReplQueryType   aReplQueryType,
                                         idBool              aIsOld )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiReplicaSetInfo   sFoundReplicaSetInfo;
    UInt                i = 0;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    // old��� ���� ���� ��尡 add�Ǳ� ���� replicaSetInfo �������� ����
    // KSafety = 1 �̸� �ش���� ����
    // KSafety = 2 �̸� �� ����� second stop/drop, old ����� first/second�� stop/drop
    // �� ����� ���
    // KSafety = 1 �̸� �� ����� first create/add/start
    // KSafety = 2 �̸� �� ���, ���� ����� second create/add/start
    for ( i = 0; i < sLocalMetaInfo.mKSafety; i++ )
    {
        if ( aCurrReplicaSetInfo->mCount != 0 ) // �� ���
        {
            if ( sParseTree->mDDLType == SHARD_ADD )
            {
                // add�Ǵ� ���� SET_SHARD_RANGE�� ���� �ʾұ� ������ add/start replication���� ����
                // R2HA add �Ǵ� ��忡 ���ؼ��� ���� start replication �ϵ��� �ؾ���.
                // no item replcation start�� ���� shardExecDirect()�� ���ؼ� �����ϴ� ���� �������� ����
                // ���� ������ QDSD_REPL_QUERY_TYPE_START ���� ���� �ϸ��
                if ( aReplQueryType != QDSD_REPL_QUERY_TYPE_ADD )
                {
                    IDE_TEST( sdi::findSendReplInfoFromReplicaSet(
                                  aReplicaSetInfo,
                                  aCurrReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                                  &sFoundReplicaSetInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( searchAndRunReplicaSet( aStatement,
                                                      &sFoundReplicaSetInfo,
                                                      i,
                                                      aReplQueryType )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_TEST( sdi::findSendReplInfoFromReplicaSet(
                              aReplicaSetInfo,
                              aCurrReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                              &sFoundReplicaSetInfo )
                          != IDE_SUCCESS );

                IDE_TEST( searchAndRunReplicaSet( aStatement,
                                                  &sFoundReplicaSetInfo,
                                                  i,
                                                  aReplQueryType )
                          != IDE_SUCCESS );
            }
        }
        
        if ( aPrevReplicaSetInfo->mCount != 0 ) // �� ���
        {
            if ( aIsOld == ID_TRUE )
            {
                if ( i == 1 )
                {
                    IDE_TEST( sdi::findSendReplInfoFromReplicaSet(
                                  aReplicaSetInfo,
                                  aPrevReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                                  &sFoundReplicaSetInfo )
                              != IDE_SUCCESS );

                    IDE_TEST( searchAndRunReplicaSet( aStatement,
                                                      &sFoundReplicaSetInfo,
                                                      i,
                                                      aReplQueryType )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                IDE_TEST( sdi::findSendReplInfoFromReplicaSet(
                              aReplicaSetInfo,
                              aPrevReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                              &sFoundReplicaSetInfo )
                          != IDE_SUCCESS );

                IDE_TEST( searchAndRunReplicaSet( aStatement,
                                                  &sFoundReplicaSetInfo,
                                                  i,
                                                  aReplQueryType )
                          != IDE_SUCCESS );
            }            
        }
        
        if ( aPrevPrevReplicaSetInfo->mCount != 0 ) // ���� ���
        {
            if (( i == 1 ) && ( aIsOld != ID_TRUE ))
            {
                IDE_TEST( sdi::findSendReplInfoFromReplicaSet(
                              aReplicaSetInfo,
                              aPrevPrevReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                              &sFoundReplicaSetInfo )
                          != IDE_SUCCESS );

                IDE_TEST( searchAndRunReplicaSet( aStatement,
                                                  &sFoundReplicaSetInfo,
                                                  i,
                                                  aReplQueryType )
                          != IDE_SUCCESS );
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::searchAndRunReplicaSet( qcStatement       * aStatement,
                                     sdiReplicaSetInfo * aReplicaSetInfo,
                                     UInt                aNth,
                                     qdsdReplQueryType   aReplQueryType )
{
    qdShardParseTree * sParseTree = NULL;
    sdiLocalMetaInfo * sLocalMetaInfo = NULL;
    SChar            * sTempNodeName = NULL;
    sdiBackupOrder     sBackupOrder;
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    
    IDE_TEST_RAISE( aReplicaSetInfo->mCount > 1, ERR_REPLICA_SET_COUNT );
    IDE_TEST_RAISE( aNth > 1, ERR_INVALID_KSAFETY );
    
    if ( aNth == 0 ) /* First */
    {
        sBackupOrder = SDI_BACKUP_1ST;
        if ( idlOS::strncmp( SDM_NA_STR,
                             aReplicaSetInfo->mReplicaSets->mFirstBackupNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
        {
            sTempNodeName = aReplicaSetInfo->mReplicaSets->mFirstBackupNodeName;
        }
        else
        {
            sTempNodeName = aReplicaSetInfo->mReplicaSets->mStopFirstBackupNodeName;
        }                                

        /* BackupNodeName�� �����Ǿ� �־�߸� �Ѵ�. */
        if ( idlOS::strncmp( sTempNodeName,
                             SDM_NA_STR,
                             SDI_NODE_NAME_MAX_SIZE + 1) != 0 )
        {
            switch( aReplQueryType )
            {
                case QDSD_REPL_QUERY_TYPE_STOP:
                    IDE_TEST( changeBuckupNodeNameForFailOver( aStatement,
                                                               aReplicaSetInfo,
                                                               aNth )
                              != IDE_SUCCESS );

                    // sender
                    IDE_TEST( stopReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName )
                              != IDE_SUCCESS );                    
                    break;
                case QDSD_REPL_QUERY_TYPE_DROP:
                    // sender
                    IDE_TEST( dropReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName )
                              != IDE_SUCCESS );
                    // receiver
                    IDE_TEST( dropReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplToNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName )
                              != IDE_SUCCESS );                    
                    break;
                case QDSD_REPL_QUERY_TYPE_CREATE:
                    sLocalMetaInfo =
                        sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                   aReplicaSetInfo->mReplicaSets->mFirstReplToNodeName );
                    // sender
                    IDE_TEST( createReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName,
                                  sLocalMetaInfo->mInternalRPHostIP,
                                  sLocalMetaInfo->mInternalRPPortNo )
                              != IDE_SUCCESS );
                    
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP",
                                      aReplicaSetInfo->mReplicaSets->mFirstReplName );
                    
                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DCL );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "DROP REPLICATION "QCM_SQL_STRING_SKIP_FMT"",
                                     aReplicaSetInfo->mReplicaSets->mFirstReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    sLocalMetaInfo =
                        sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                   aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName );
                    // receiver
                    IDE_TEST( createReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplToNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName,
                                  sLocalMetaInfo->mInternalRPHostIP,
                                  sLocalMetaInfo->mInternalRPPortNo )
                              != IDE_SUCCESS );
                    
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "DROP REPLICATION "QCM_SQL_STRING_SKIP_FMT"",
                                     aReplicaSetInfo->mReplicaSets->mFirstReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mFirstReplToNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );
                    break;
                case QDSD_REPL_QUERY_TYPE_ADD:

                    IDE_TEST( addReplicationForInternal( aStatement,
                                                         aReplicaSetInfo,
                                                         sBackupOrder )
                              != IDE_SUCCESS );
                    break;
                case QDSD_REPL_QUERY_TYPE_START:
                    // sender
                    IDE_TEST( startReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName )
                              != IDE_SUCCESS );
                    
                    break;
                case QDSD_REPL_QUERY_TYPE_FLUSH:
                    IDE_TEST( flushReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mPrimaryNodeName,
                                  aReplicaSetInfo->mReplicaSets->mFirstReplName )
                              != IDE_SUCCESS );
                    break;                    
                default:
                    IDE_ASSERT(0);
                    break;
            }
        }
    }
    else if ( aNth == 1 ) /* Second */
    {
        sBackupOrder = SDI_BACKUP_2ND;
        if ( idlOS::strncmp( SDM_NA_STR,
                             aReplicaSetInfo->mReplicaSets->mSecondBackupNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
        {
            sTempNodeName = aReplicaSetInfo->mReplicaSets->mSecondBackupNodeName;
        }
        else
        {
            sTempNodeName = aReplicaSetInfo->mReplicaSets->mStopSecondBackupNodeName;
        }

        /* BackupNodeName�� �����Ǿ� �־�߸� �Ѵ�. */
        if ( idlOS::strncmp( sTempNodeName,
                             SDM_NA_STR,
                             SDI_NODE_NAME_MAX_SIZE + 1) != 0 )
        {
            switch( aReplQueryType )
            {
                case QDSD_REPL_QUERY_TYPE_STOP:
                    IDE_TEST( changeBuckupNodeNameForFailOver( aStatement,
                                                               aReplicaSetInfo,
                                                               aNth )
                              != IDE_SUCCESS );
                      
                    // sender
                    IDE_TEST( stopReplicationForInternal(
                                    aStatement,
                                    aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                    aReplicaSetInfo->mReplicaSets->mSecondReplName )
                                != IDE_SUCCESS );
                      break;
                case QDSD_REPL_QUERY_TYPE_DROP:
                    // sender
                    IDE_TEST( dropReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName )
                              != IDE_SUCCESS );
                    // receiver
                    IDE_TEST( dropReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplToNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName )
                              != IDE_SUCCESS );
                    break;
                case QDSD_REPL_QUERY_TYPE_CREATE:
                    // sender
                    sLocalMetaInfo =
                        sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                   aReplicaSetInfo->mReplicaSets->mSecondReplToNodeName );

                    IDE_TEST( createReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName,
                                  sLocalMetaInfo->mInternalRPHostIP,
                                  sLocalMetaInfo->mInternalRPPortNo )
                              != IDE_SUCCESS );
                    
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP",
                                     aReplicaSetInfo->mReplicaSets->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DCL );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "DROP REPLICATION "QCM_SQL_STRING_SKIP_FMT"",
                                     aReplicaSetInfo->mReplicaSets->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    // receiver
                    sLocalMetaInfo =
                        sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                   aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName );

                    IDE_TEST( createReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplToNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName,
                                  sLocalMetaInfo->mInternalRPHostIP,
                                  sLocalMetaInfo->mInternalRPPortNo )
                              != IDE_SUCCESS );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "DROP REPLICATION "QCM_SQL_STRING_SKIP_FMT"",
                                     aReplicaSetInfo->mReplicaSets->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       aReplicaSetInfo->mReplicaSets->mSecondReplToNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    break;
                case QDSD_REPL_QUERY_TYPE_ADD:
                    IDE_TEST( addReplicationForInternal( aStatement,
                                                         aReplicaSetInfo,
                                                         sBackupOrder )
                              != IDE_SUCCESS );
                    break;
                case QDSD_REPL_QUERY_TYPE_START:
                    // sender
                    IDE_TEST( startReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName )
                              != IDE_SUCCESS );
                    break;
                case QDSD_REPL_QUERY_TYPE_FLUSH:
                    IDE_TEST( flushReplicationForInternal(
                                  aStatement,
                                  aReplicaSetInfo->mReplicaSets->mPrimaryNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSecondReplName )
                              != IDE_SUCCESS );
                    break;
                default:
                    IDE_ASSERT(0);
                    break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_REPLICA_SET_COUNT )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TOO_MANY_REPLICA_SETS ) );
    }
    IDE_EXCEPTION( ERR_INVALID_KSAFETY )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_KSAFETY ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::createReplicationForInternal( qcStatement * aStatement,
                                           SChar       * aNodeName,
                                           SChar       * aReplName,
                                           SChar       * aHostIP,
                                           UInt          aPortNo )
{
    sdiLocalMetaInfo   sLocalMetaInfo;
    SChar              sSqlStr[QD_MAX_SQL_LENGTH + 1];
    
    SChar              sReplicationModeStr[SDI_REPLICATION_MODE_MAX_SIZE + 1];

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    switch ( sLocalMetaInfo.mReplicationMode )
    {
        case SDI_REP_MODE_CONSISTENT_CODE:
            idlOS::snprintf( sReplicationModeStr,
                             ID_SIZEOF(sReplicationModeStr),
                             SDI_REP_MODE_CONSISTENT_STR );
            break;

            /* unset replication mode */
        case SDI_REP_MODE_NETWAIT_CODE:
        case SDI_REP_MODE_NOWAIT_CODE:
            /* R2HA unsupported mode, not yet */
            /* Currently, these mode temporarily run in LAZY mode instead */
            idlOS::snprintf( sReplicationModeStr,
                             ID_SIZEOF(sReplicationModeStr),
                             "LAZY" );
            break;
        case SDI_REP_MODE_NULL_CODE:
            IDE_RAISE( ERR_DOES_NOT_SET_REPLICATION );
        default :
            IDE_DASSERT( 0 );
            break;
    }

    if ( idlOS::strncmp( sLocalMetaInfo.mNodeName, 
                         aNodeName,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "CREATE %s REPLICATION "QCM_SQL_STRING_SKIP_FMT" OPTIONS PARALLEL %"ID_INT32_FMT" WITH '%s',%"ID_INT32_FMT" ",
                         sReplicationModeStr,
                         aReplName,
                         sLocalMetaInfo.mParallelCount,
                         aHostIP,
                         aPortNo );
    }
    else
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "CREATE %s REPLICATION "QCM_SQL_STRING_SKIP_FMT" OPTIONS PARALLEL %"ID_INT32_FMT" WITH ''%s'',%"ID_INT32_FMT" ",
                         sReplicationModeStr,
                         aReplName,
                         sLocalMetaInfo.mParallelCount,
                         aHostIP,
                         aPortNo );
    }
        
    IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            ID_TRUE )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_DOES_NOT_SET_REPLICATION )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_NOT_EXIST_REPLICA_SET ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::createReverseReplication( qcStatement * aStatement,
                                       SChar       * aNodeName,
                                       SChar       * aReplName,
                                       SChar       * aOtherNodeName,
                                       SChar       * aHostIP,
                                       UInt          aPortNo,
                                       sdiReplDirectionalRole aRole )
{
    sdiLocalMetaInfo   sLocalMetaInfo;
    SChar              sSqlStr[QD_MAX_SQL_LENGTH + 1];
    
    SChar              sReplicationModeStr[SDI_REPLICATION_MODE_MAX_SIZE + 1];
    SChar              sPropagationOption[30];

    UInt               sExecCount = 0;
    ULong              sCount = 0;
    idBool             sFetchResult = ID_FALSE;

    /* Check RP Exist */
    /* ReverseRP�� Failover/Failback �ÿ��� �����Ǹ� Failback �Ϸ�� �����ȴ�.
     * ReverseRP������ �����Ǿ� �ִ� ���� ReverseRP�� �����Ѵٸ� ���� �õ����� ������ ��� �̹Ƿ� Drop ���־�� �Ѵ�. */
    idlOS::snprintf( sSqlStr, 
                     QD_MAX_SQL_LENGTH + 1,
                     "select Count(*) from system_.sys_replications_ where replication_name='%s' ",
                     aReplName );

    IDE_TEST( sdi::shardExecDirect( aStatement,
                                    aNodeName,
                                    (SChar*)sSqlStr,
                                    (UInt) idlOS::strlen(sSqlStr),
                                    SDI_INTERNAL_OP_FAILBACK,
                                    &sExecCount,
                                    &sCount,
                                    NULL,
                                    ID_SIZEOF(sCount),
                                    &sFetchResult )
              != IDE_SUCCESS );

    if ( sCount != 0 )
    {
        /* ReverseRP ���� ������ �����ϸ� �ȵȴ�. Drop �ϰ� ���� �����ؾ� �Ѵ�. */
        IDE_TEST( dropReplicationForInternal( aStatement,
                                              aNodeName,
                                              aReplName )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    idlOS::snprintf( sReplicationModeStr,
                     ID_SIZEOF(sReplicationModeStr),
                     "LAZY" );

    switch ( aRole )
    {
        case SDI_REPL_SENDER:
            idlOS::snprintf( sPropagationOption,
                             ID_SIZEOF( sPropagationOption),
                             "FOR PROPAGATION" );
            break;
        case SDI_REPL_RECEIVER:
            idlOS::snprintf( sPropagationOption,
                             ID_SIZEOF( sPropagationOption),
                             "FOR PROPAGABLE LOGGING" );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    if ( idlOS::strncmp( sLocalMetaInfo.mNodeName, 
                         aNodeName,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "CREATE %s REPLICATION "QCM_SQL_STRING_SKIP_FMT" %s OPTIONS PARALLEL %"ID_INT32_FMT" WITH '%s',%"ID_INT32_FMT" ",
                         sReplicationModeStr,
                         aReplName,
                         sPropagationOption,
                         sLocalMetaInfo.mParallelCount,
                         aHostIP,
                         aPortNo );
    }
    else
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "CREATE %s REPLICATION "QCM_SQL_STRING_SKIP_FMT" %s OPTIONS PARALLEL %"ID_INT32_FMT" WITH ''%s'',%"ID_INT32_FMT" ",
                         sReplicationModeStr,
                         aReplName,
                         sPropagationOption,
                         sLocalMetaInfo.mParallelCount,
                         aHostIP,
                         aPortNo );
    }
        
    IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            ID_TRUE )
              != IDE_SUCCESS );

    /* Create �� RP�� Rollback�� �����־�� �Ѵ�. */
    idlOS::snprintf(sSqlStr,
                    QD_MAX_SQL_LENGTH,
                    "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                    aReplName );

    switch ( aRole )
    {
        case SDI_REPL_SENDER:
            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               aNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );
            break;
        case SDI_REPL_RECEIVER:
            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               aOtherNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );
            break;
        default:
            IDE_DASSERT( 0 );
            break;
    }

    idlOS::snprintf(sSqlStr,
                    QD_MAX_SQL_LENGTH,
                    "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                    aReplName );

    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                       aNodeName,
                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                       QCI_STMT_MASK_MAX );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdsd::stopReplicationForInternal( qcStatement * aStatement,
                                         SChar       * aNodeName,
                                         SChar       * aReplName )
{
    SChar sSqlStr[QD_MAX_SQL_LENGTH + 1];

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP ",
                     aReplName );
    
    IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            ID_FALSE )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::dropReplicationForInternal( qcStatement * aStatement,
                                         SChar       * aNodeName,
                                         SChar       * aReplName )
{
    SChar sSqlStr[QD_MAX_SQL_LENGTH + 1];

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DROP REPLICATION "QCM_SQL_STRING_SKIP_FMT" ",
                     aReplName );
    
    IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            ID_TRUE)
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::addReplicationForInternal( qcStatement       * aStatement,
                                        sdiReplicaSetInfo * aReplicaSetInfo,
                                        sdiBackupOrder      aBackupOrder )
{
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfoList  * sTmpTableInfoList = NULL;    
    sdiNode             sNode;
    sdiRangeInfo        sRangeInfo;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiTableInfo      * sTableInfo = NULL;
    UInt                i = 0;
    SChar             * sReplicationName = NULL;
    SChar             * sSenderNodeName = NULL;
    SChar             * sReceiverNodeName = NULL;

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    switch (aBackupOrder)
    {
        case SDI_BACKUP_1ST:
            sReplicationName = aReplicaSetInfo->mReplicaSets->mFirstReplName;
            sSenderNodeName = aReplicaSetInfo->mReplicaSets->mFirstReplFromNodeName;
            sReceiverNodeName =  aReplicaSetInfo->mReplicaSets->mFirstReplToNodeName;
            break;
        case SDI_BACKUP_2ND:
            sReplicationName = aReplicaSetInfo->mReplicaSets->mSecondReplName;
            sSenderNodeName = aReplicaSetInfo->mReplicaSets->mSecondReplFromNodeName;
            sReceiverNodeName =  aReplicaSetInfo->mReplicaSets->mSecondReplToNodeName;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }
    // get node id
    IDE_TEST( sdi::getNodeByName( QC_SMI_STMT( aStatement ),
                                  aReplicaSetInfo->mReplicaSets->mPrimaryNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                  &sNode )
              != IDE_SUCCESS );

    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          aReplicaSetInfo->mReplicaSets->mSMN )
              != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )
    {
        sTableInfo = sTmpTableInfoList->mTableInfo;
        /* Procedure�� RP ADD�� �������� �ʴ´�. */
        if ( sTableInfo->mObjectType == 'T' )
        {
            switch (sdi::getShardObjectType(sTableInfo))
            {
                case SDI_SOLO_DIST_OBJECT:
                    // get range info
                    IDE_TEST( sdi::getRangeInfo ( aStatement,
                                                  QC_SMI_STMT( aStatement ),
                                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                                  sTableInfo,
                                                  &sRangeInfo,
                                                  ID_FALSE )
                              != IDE_SUCCESS );
                    // solo�� �ϳ��� ��带 �������� backup table�� �����ϱ� ������
                    // solo�� �����ϴ� ���������� add replication�� �����Ѵ�.
                    if (( sRangeInfo.mCount != 0 ) &&
                        ( sRangeInfo.mRanges->mNodeId == sNode.mNodeId ))
                    {
                        IDE_TEST( sdi::addReplTable( aStatement,
                                                     sSenderNodeName,
                                                     sReplicationName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     SDM_NA_STR,
                                                     SDI_REPL_SENDER,
                                                     ID_TRUE )
                                  != IDE_SUCCESS );

                        IDE_TEST( sdi::addReplTable( aStatement,
                                                     sReceiverNodeName,
                                                     sReplicationName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     SDM_NA_STR,
                                                     SDI_REPL_RECEIVER,
                                                     ID_TRUE )
                                  != IDE_SUCCESS );
                    }
                    break;
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    /* Default Partition check */
                    if ( sTableInfo->mDefaultNodeId == sNode.mNodeId )
                    {
                        IDE_TEST( sdi::addReplTable( aStatement,
                                                     sSenderNodeName,
                                                     sReplicationName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sTableInfo->mDefaultPartitionName,
                                                     SDI_REPL_SENDER,
                                                     ID_TRUE )
                                  != IDE_SUCCESS );

                        IDE_TEST( sdi::addReplTable( aStatement,
                                                     sReceiverNodeName,
                                                     sReplicationName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sTableInfo->mDefaultPartitionName,
                                                     SDI_REPL_RECEIVER,
                                                     ID_TRUE )
                                  != IDE_SUCCESS );
                    }
                    // get range info
                    IDE_TEST( sdi::getRangeInfo ( aStatement,
                                                  QC_SMI_STMT( aStatement ),
                                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                                  sTableInfo,
                                                  &sRangeInfo,
                                                  ID_FALSE )
                              != IDE_SUCCESS );

                    for ( i = 0; i < sRangeInfo.mCount; i++ )
                    {
                        if (( sRangeInfo.mRanges[i].mNodeId == sNode.mNodeId ) &&
                            ( sRangeInfo.mRanges[i].mReplicaSetId == aReplicaSetInfo->mReplicaSets->mReplicaSetId ))
                        {
                            IDE_TEST( sdi::addReplTable( aStatement,
                                                         sSenderNodeName,
                                                         sReplicationName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sRangeInfo.mRanges[i].mPartitionName,
                                                         SDI_REPL_SENDER,
                                                         ID_TRUE )
                                      != IDE_SUCCESS );

                            IDE_TEST( sdi::addReplTable( aStatement,
                                                         sReceiverNodeName,
                                                         sReplicationName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sRangeInfo.mRanges[i].mPartitionName,
                                                         SDI_REPL_RECEIVER,
                                                         ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                    }
                    break;
                case SDI_CLONE_DIST_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                case SDI_NON_SHARD_OBJECT:
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::startReplicationForInternal( qcStatement * aStatement,
                                          SChar       * aNodeName,
                                          SChar       * aReplName )
{
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    sdiInternalOperation sInternalOP = SDI_INTERNAL_OP_NORMAL;

    if( qci::mSessionCallback.mGetShardInternalLocalOperation( aStatement->session->mMmSession ) 
        == SDI_INTERNAL_OP_FAILBACK )
    {
        sInternalOP = SDI_INTERNAL_OP_FAILBACK;
    }
    
    // LOCAL_META_INFO
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
              != IDE_SUCCESS );

    if ( idlOS::strncmp( sLocalMetaInfo.mNodeName, 
                         aNodeName,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
    {
        IDE_TEST( executeLocalSQL( aStatement, 
                                   QCI_STMT_MASK_DCL,
                                   sSqlStr,
                                   QD_MAX_SQL_LENGTH,
                                   "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" SYNC PARALLEL %"ID_UINT32_FMT,
                                   aReplName,
                                   sLocalMetaInfo.mParallelCount )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

        if ( aStatement->session->mQPSpecific.mClientInfo != NULL )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aNodeName,
                                        sInternalOP,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" SYNC PARALLEL %"ID_UINT32_FMT,
                                        aReplName,
                                        sLocalMetaInfo.mParallelCount )
                      != IDE_SUCCESS);
        }
    }
    
    ideLog::log(IDE_SD_17,"[SHARD INTERNAL SQL]: %s, %s",aNodeName, sSqlStr);
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::truncateBackupTable( qcStatement * aStatement )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiReplicaSetInfo   sReplicaSetInfo;
    sdiReplicaSetInfo   sCurrReplicaSetInfo;
    sdiReplicaSetInfo   sPrevReplicaSetInfo;
    sdiReplicaSetInfo   sPrevPrevReplicaSetInfo;
    sdiGlobalMetaInfo   sMetaNodeInfo = { ID_ULONG(0) };
    ULong               sSMN = ID_ULONG(0);

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );    

    IDE_TEST( sdi::getGlobalMetaInfoCore( QC_SMI_STMT( aStatement ),
                                          &sMetaNodeInfo )
              != IDE_SUCCESS );

    sSMN = sMetaNodeInfo.mShardMetaNumber;
    
    IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sSMN )
              != IDE_SUCCESS );

    IDE_TEST( findBackupReplicaSetInfo( sLocalMetaInfo.mNodeName,
                                        &sReplicaSetInfo,
                                        &sCurrReplicaSetInfo,
                                        &sPrevReplicaSetInfo,
                                        &sPrevPrevReplicaSetInfo )
              != IDE_SUCCESS );
    
    if ( sLocalMetaInfo.mKSafety == 1 )
    {
        // node3 ���� backup table ����
        // ���� ��忡�� ������� first backup partition table�� ����
        if ( sParseTree->mNodeCount > 2 )
        {
            if ( idlOS::strncmp( SDM_NA_STR,
                                 sCurrReplicaSetInfo.mReplicaSets[0].mFirstBackupNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mFirstBackupNodeName,
                              &sPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mStopFirstBackupNodeName,
                              &sPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
        }
    }
    else if ( sLocalMetaInfo.mKSafety == 2 )
    {
        // node4 ���� backup table ����
        // ���� ��忡�� ������� second backup partition table�� ����
        // ���� ��忡�� ���� ����� second backup partition table�� ����    
        if ( sParseTree->mNodeCount > 3 )
        {
            if ( idlOS::strncmp( SDM_NA_STR,
                                 sCurrReplicaSetInfo.mReplicaSets[0].mSecondBackupNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mSecondBackupNodeName,
                              &sPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mStopSecondBackupNodeName,
                              &sPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
            

            if ( idlOS::strncmp( SDM_NA_STR,
                                 sCurrReplicaSetInfo.mReplicaSets[0].mFirstBackupNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mFirstBackupNodeName,
                              &sPrevPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
            else
            {
                IDE_TEST( truncateBackupTableForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mStopFirstBackupNodeName,
                              &sPrevPrevReplicaSetInfo )
                          != IDE_SUCCESS );
            }
        }
    }    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::truncateBackupTableForInternal( qcStatement       * aStatement,
                                             SChar             * aNodeName,
                                             sdiReplicaSetInfo * aReplicaSetInfo )
{
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfoList  * sTmpTableInfoList = NULL;
    sdiTableInfo      * sTableInfo = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    sdiNode             sNode;
    sdiRangeInfo        sRangeInfo;
    UInt                i = 0;
    SChar               sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    // get node id
    IDE_TEST( sdi::getNodeByName( QC_SMI_STMT( aStatement ),
                                  aReplicaSetInfo->mReplicaSets->mPrimaryNodeName,
                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                  &sNode )
              != IDE_SUCCESS );

    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          aReplicaSetInfo->mReplicaSets->mSMN )
              != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )
    {
        sTableInfo = sTmpTableInfoList->mTableInfo;
        /* Procedure�� truncate�� �������� �ʴ´�. */
        if ( sTableInfo->mObjectType == 'T' )
        {
            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTmpTableInfoList->mTableInfo->mObjectName );

            switch (sdi::getShardObjectType(sTableInfo))
            {
                case SDI_SOLO_DIST_OBJECT:
                    // get range info
                    IDE_TEST( sdi::getRangeInfo ( aStatement,
                                                  QC_SMI_STMT( aStatement ),
                                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                                  sTableInfo,
                                                  &sRangeInfo,
                                                  ID_FALSE )
                              != IDE_SUCCESS );
                    IDE_DASSERT( sRangeInfo.mCount != 0 );
                    if (( sRangeInfo.mCount != 0 ) &&
                        ( sRangeInfo.mRanges->mNodeId == sNode.mNodeId ))
                    {
                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                         "TRUNCATE TABLE "QCM_SQL_STRING_SKIP_FMT" ",
                                         sBufferNameStr,
                                         aNodeName );

                        IDE_TEST( sdi::shardExecTempDDLWithNewTrans( aStatement,
                                                                     aNodeName,
                                                                     sSqlStr,
                                                                     idlOS::strlen(sSqlStr) )
                                  != IDE_SUCCESS );
                    }
                    break;
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    /* Default Partition check */
                    if ( sTableInfo->mDefaultNodeId == sNode.mNodeId )
                    {
                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                         "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT" TRUNCATE PARTITION "QCM_SQL_STRING_SKIP_FMT" ",
                                         sBufferNameStr,
                                         sTableInfo->mDefaultPartitionName,
                                         aNodeName );

                        IDE_TEST( sdi::shardExecTempDDLWithNewTrans( aStatement,
                                                                     aNodeName,
                                                                     sSqlStr,
                                                                     idlOS::strlen(sSqlStr) )
                                  != IDE_SUCCESS );
                    }
                    // get range info
                    IDE_TEST( sdi::getRangeInfo ( aStatement,
                                                  QC_SMI_STMT( aStatement ),
                                                  aReplicaSetInfo->mReplicaSets->mSMN,
                                                  sTableInfo,
                                                  &sRangeInfo,
                                                  ID_FALSE )
                              != IDE_SUCCESS );

                    for ( i = 0; i < sRangeInfo.mCount; i++ )
                    {
                        if (( sRangeInfo.mRanges[i].mNodeId == sNode.mNodeId ) &&
                            ( sRangeInfo.mRanges[i].mReplicaSetId == aReplicaSetInfo->mReplicaSets->mReplicaSetId ))
                        {
                            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                             "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT" TRUNCATE PARTITION "QCM_SQL_STRING_SKIP_FMT" ",
                                             sBufferNameStr,
                                             sRangeInfo.mRanges[i].mPartitionName,
                                             aNodeName );

                            IDE_TEST( sdi::shardExecTempDDLWithNewTrans( aStatement,
                                                                         aNodeName,
                                                                         sSqlStr,
                                                                         idlOS::strlen(sSqlStr) )
                                      != IDE_SUCCESS );
                        }
                    }
                    break;
                case SDI_CLONE_DIST_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                case SDI_NON_SHARD_OBJECT:
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::findBackupReplicaSetInfo( SChar             * aNodeName,
                                       sdiReplicaSetInfo * aReplicaSetInfo,
                                       sdiReplicaSetInfo * aCurrReplicaSetInfo,
                                       sdiReplicaSetInfo * aPrevReplicaSetInfo,
                                       sdiReplicaSetInfo * aPrevPrevReplicaSetInfo )
{
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar             * sTempNodeName = NULL;
    UInt                sCnt = 0;
    UInt                i = 0;

    aCurrReplicaSetInfo->mCount     = 0;
    aPrevReplicaSetInfo->mCount     = 0;
    aPrevPrevReplicaSetInfo->mCount = 0;

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    for ( sCnt = 0; sCnt < aReplicaSetInfo->mCount; sCnt++ )
    {
        // curr replicaSetInfo
        if ( aCurrReplicaSetInfo->mCount != 1 )
        {   
            if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[sCnt].mPrimaryNodeName, 
                                 aNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                idlOS::memcpy( &aCurrReplicaSetInfo->mReplicaSets[0],
                               &aReplicaSetInfo->mReplicaSets[sCnt],
                               ID_SIZEOF( sdiReplicaSet ) );
            
                aCurrReplicaSetInfo->mCount = 1;
            }
        }        
        
        for ( i = 0; i < sLocalMetaInfo.mKSafety; i++ )
        {
            // prev replicaSetInfo
            if ( aPrevReplicaSetInfo->mCount != 1 )
            {
                if ( idlOS::strncmp( SDM_NA_STR,
                                     aReplicaSetInfo->mReplicaSets[sCnt].mFirstBackupNodeName,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
                {
                    sTempNodeName = aReplicaSetInfo->mReplicaSets[sCnt].mFirstBackupNodeName;
                }
                else
                {
                    sTempNodeName = aReplicaSetInfo->mReplicaSets[sCnt].mStopFirstBackupNodeName;
                }
                                
                if ( idlOS::strncmp( sTempNodeName, 
                                     aNodeName,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
                {
                    idlOS::memcpy( &aPrevReplicaSetInfo->mReplicaSets[0],
                                   &aReplicaSetInfo->mReplicaSets[sCnt],
                                   ID_SIZEOF( sdiReplicaSet ) );
            
                    aPrevReplicaSetInfo->mCount = 1;
                }
            }

            if ( i == 1 )
            {    
                // prev prev replicaSetInfo
                if ( aPrevPrevReplicaSetInfo->mCount != 1 )
                {
                    if ( idlOS::strncmp( SDM_NA_STR,
                                         aReplicaSetInfo->mReplicaSets[sCnt].mSecondBackupNodeName,
                                         SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
                    {
                        sTempNodeName = aReplicaSetInfo->mReplicaSets[sCnt].mSecondBackupNodeName;
                    }
                    else
                    {
                        sTempNodeName = aReplicaSetInfo->mReplicaSets[sCnt].mStopSecondBackupNodeName;
                    }
                    
                    if ( idlOS::strncmp( sTempNodeName, 
                                         aNodeName,
                                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
                    {
                        idlOS::memcpy( &aPrevPrevReplicaSetInfo->mReplicaSets[0],
                                       &aReplicaSetInfo->mReplicaSets[sCnt],
                                       ID_SIZEOF( sdiReplicaSet ) );
            
                        aPrevPrevReplicaSetInfo->mCount = 1;
                    }
                }
            }
        }       
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * IN: aReShardAttr, aNameBufSize
 * IN/OUT: aPrimaryTableName, aBackupTableName
 */
IDE_RC qdsd::getTableNames( qdReShardAttribute * aReShardAttr, 
                            SChar * aSessionUserName, 
                            UInt    aNameBufSize, 
                            SChar * aPrimaryTableName, 
                            SChar * aBackupTableName )
{
    UInt               sObjectNameBufPosition = 0;
    SChar              sObjectNameBuf[ QC_MAX_OBJECT_FULL_STR_LEN + 1 ] = {0,};
    UInt               sBackupObjectNameBufPosition = 0;
    SChar              sBackupObjectNameBuf[ QC_MAX_OBJECT_FULL_STR_LEN + 1 ] = {0,};

    IDE_TEST_RAISE( aNameBufSize < QC_MAX_OBJECT_FULL_STR_LEN + 1, ERR_BUF_SIZE);

    if ( QC_IS_NULL_NAME(aReShardAttr->mUserName) != ID_TRUE )
    {
        QC_STR_COPY(sObjectNameBuf, aReShardAttr->mUserName); /* "user_name" */
        sObjectNameBufPosition = aReShardAttr->mUserName.size ;
        QC_STR_COPY(sBackupObjectNameBuf, aReShardAttr->mUserName); /* "user_name" */
        sBackupObjectNameBufPosition = aReShardAttr->mUserName.size; 
    }
    else
    {
        idlOS::strncpy( sObjectNameBuf, aSessionUserName, QC_MAX_OBJECT_NAME_LEN ); /* "user_name" */
        sObjectNameBufPosition = idlOS::strlen(aSessionUserName); 
        idlOS::strncpy( sBackupObjectNameBuf, aSessionUserName, QC_MAX_OBJECT_NAME_LEN ); /* "user_name" */
        sBackupObjectNameBufPosition = idlOS::strlen(aSessionUserName);
    }

    sObjectNameBuf[sObjectNameBufPosition] = '\"'; 
    sObjectNameBufPosition++;
    sObjectNameBuf[sObjectNameBufPosition] = '.'; /* "user." */
    sObjectNameBufPosition++;
    sObjectNameBuf[sObjectNameBufPosition] = '\"'; 
    sObjectNameBufPosition++;
    sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '\"';
    sBackupObjectNameBufPosition++;
    sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '.'; /* "user." */
    sBackupObjectNameBufPosition++;
    sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '\"';
    sBackupObjectNameBufPosition++;

    idlOS::strncpy(sBackupObjectNameBuf + sBackupObjectNameBufPosition, 
                SDI_BACKUP_TABLE_PREFIX, 
                ID_SIZEOF(SDI_BACKUP_TABLE_PREFIX)); /* "user_name._BAK_" */
    sBackupObjectNameBufPosition += idlOS::strlen(SDI_BACKUP_TABLE_PREFIX);

    IDE_DASSERT(QC_IS_NULL_NAME(aReShardAttr->mObjectName) != ID_TRUE);

    QC_STR_COPY(sObjectNameBuf + sObjectNameBufPosition, aReShardAttr->mObjectName); /* "user_name.table_name" */
    sObjectNameBufPosition += aReShardAttr->mObjectName.size;

    QC_STR_COPY(sBackupObjectNameBuf + sBackupObjectNameBufPosition, aReShardAttr->mObjectName); /* "user_name._BAk_table_name" */
    sBackupObjectNameBufPosition += aReShardAttr->mObjectName.size;

    if ( QC_IS_NULL_NAME(aReShardAttr->mPartitionName) != ID_TRUE )
    {
        sObjectNameBuf[sObjectNameBufPosition] = '\"'; 
        sObjectNameBufPosition++;

        idlOS::snprintf( sObjectNameBuf + sObjectNameBufPosition, 
                         ID_SIZEOF(sObjectNameBuf) - sObjectNameBufPosition,
                         " partition ",
                         11 ); /* "user_name.table_name partition " */
        sObjectNameBufPosition += 11;

        sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '\"';
        sBackupObjectNameBufPosition++;

        idlOS::snprintf( sBackupObjectNameBuf + sBackupObjectNameBufPosition, 
                         ID_SIZEOF(sBackupObjectNameBuf) - sBackupObjectNameBufPosition, 
                         " partition ",
                         11 ); /* "user_name._BAk_table_name partition " */
        sBackupObjectNameBufPosition += 11;

        sObjectNameBuf[sObjectNameBufPosition] = '\"'; 
        sObjectNameBufPosition++;

        QC_STR_COPY(sObjectNameBuf + sObjectNameBufPosition, aReShardAttr->mPartitionName); /* "user_name.table_name partition partition_name" */
        sObjectNameBufPosition += aReShardAttr->mPartitionName.size;

        sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '\"';
        sBackupObjectNameBufPosition++;
        
        QC_STR_COPY(sBackupObjectNameBuf + sBackupObjectNameBufPosition, aReShardAttr->mPartitionName); /* "user_name.table_name partition partition_name" */
        sBackupObjectNameBufPosition += aReShardAttr->mPartitionName.size;
    }

    sObjectNameBuf[sObjectNameBufPosition] = '\0';
    sBackupObjectNameBuf[sBackupObjectNameBufPosition] = '\0';

    idlOS::strncpy(aPrimaryTableName, sObjectNameBuf, aNameBufSize);
    idlOS::strncpy(aBackupTableName, sBackupObjectNameBuf, aNameBufSize);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_BUF_SIZE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::getTableNames", "The length of table name is too long." ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* 
 * cleanup garbage replication
 */ 
IDE_RC qdsd::cleanupReplWithNewTransCommit( qcStatement * aStatement, 
                                            SChar * aNodeName, 
                                            SChar * aReplName )
{
    const SChar  sReplStopSQLFmt[] = "alter replication "QCM_SQL_STRING_SKIP_FMT" stop";
    const SChar  sReplDropSQLFmt[] = "drop replication "QCM_SQL_STRING_SKIP_FMT"";
    SChar sSqlStr[ SDI_REPLICATION_NAME_MAX_SIZE + 
                   IDL_MAX(ID_SIZEOF(sReplStopSQLFmt), ID_SIZEOF(sReplDropSQLFmt)) ] = {0,};

    //stop
    idlOS::snprintf(sSqlStr, ID_SIZEOF(sSqlStr), sReplStopSQLFmt, aReplName);
    (void)sdi::shardExecTempDDLWithNewTrans(aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            idlOS::strlen(sSqlStr));

    //drop
    idlOS::snprintf(sSqlStr, ID_SIZEOF(sSqlStr), sReplDropSQLFmt, aReplName);
    (void)sdi::shardExecTempDDLWithNewTrans(aStatement,
                                            aNodeName,
                                            sSqlStr,
                                            idlOS::strlen(sSqlStr));
    return IDE_SUCCESS;
}

IDE_RC qdsd::createReplWithNewTransCommit( qcStatement * aStatement, 
                                           SChar       * aNodeName, 
                                           SChar       * aReplName, 
                                           SChar       * aIP, 
                                           UInt          aPort)
{
    const SChar      * sReplCreateSQLFmt;
    SChar            * sSqlStr = NULL;
    UInt               sLen = 0;
    sdiLocalMetaInfo   sLocalMetaInfo;
   
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    // BUG-48616
    if ( sdi::isSameNode( sLocalMetaInfo.mNodeName, aNodeName ) == ID_TRUE )
    {
        sReplCreateSQLFmt = "create lazy replication "QCM_SQL_STRING_SKIP_FMT" with '%s', %"ID_UINT32_FMT;
    }
    else
    {
        sReplCreateSQLFmt = "create lazy replication "QCM_SQL_STRING_SKIP_FMT" with ''%s'', %"ID_UINT32_FMT;
    }
    
    sLen = idlOS::strlen(sReplCreateSQLFmt) + 1 + SDI_REPLICATION_NAME_MAX_SIZE +
        IDL_IP_ADDR_MAX_LEN + IDL_IP_PORT_MAX_LEN;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      sLen,
                                      &sSqlStr )
              != IDE_SUCCESS );

    idlOS::snprintf(sSqlStr,
                    sLen,
                    sReplCreateSQLFmt,
                    aReplName,
                    aIP,
                    aPort);
    
    IDE_TEST( sdi::shardExecTempDDLWithNewTrans( aStatement,
                                                 aNodeName,
                                                 sSqlStr,
                                                 idlOS::strlen(sSqlStr)) != IDE_SUCCESS );
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qdsd::alterReplAddTableWithNewTrans( qcStatement * aStatement, 
                                            SChar * aNodeName, 
                                            SChar * aReplName, 
                                            SChar * aFromTableName, 
                                            SChar * aToTableName)
{
    UInt sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt i = 0;
    const SChar sAlterReplAddTblSQLFmt[] = "alter replication "QCM_SQL_STRING_SKIP_FMT" add table from "QCM_SQL_STRING_SKIP_FMT" to "QCM_SQL_STRING_SKIP_FMT"";
    SChar sSqlStr[ ID_SIZEOF(sAlterReplAddTblSQLFmt) + 
                   SDI_REPLICATION_NAME_MAX_SIZE + 
                   QC_MAX_OBJECT_FULL_STR_LEN + 
                   QC_MAX_OBJECT_FULL_STR_LEN ] = {0,};
    
    idlOS::snprintf(sSqlStr, ID_SIZEOF(sSqlStr), sAlterReplAddTblSQLFmt, aReplName, aFromTableName, aToTableName);
    
    for (i = 0; i < sShardDDLLockTryCount; i++ )
    {
        if ( sdi::shardExecTempDDLWithNewTrans(aStatement,
                                               aNodeName,
                                               sSqlStr,
                                               idlOS::strlen(sSqlStr)) != IDE_SUCCESS )
        {
            /* do nothing : retry */
        }
        else
        {
            break;
        }
    }

    IDE_TEST(i == sShardDDLLockTryCount);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qdsd::createReplicationsWithNewTrans( qcStatement * aStatement,
                                             iduList * aNodeInfoList,
                                             SChar * aFromNodeName,
                                             SChar * aToNodeName,
                                             SChar * aReplicationName )
{
    sdiLocalMetaInfo * sToNodeLocalInfo = NULL;

    sToNodeLocalInfo = sdiZookeeper::getNodeInfo(aNodeInfoList, aToNodeName);
    IDE_TEST_RAISE( sToNodeLocalInfo == NULL, ERR_TO_NODE );

    IDE_TEST( cleanupReplWithNewTransCommit( aStatement,
                                             aFromNodeName,
                                             aReplicationName ) != IDE_SUCCESS);

    IDE_TEST( cleanupReplWithNewTransCommit( aStatement,
                                             aToNodeName,
                                             aReplicationName ) != IDE_SUCCESS);

    IDE_TEST( createReplWithNewTransCommit( aStatement,
                                            aFromNodeName,
                                            aReplicationName,
                                            sToNodeLocalInfo->mInternalRPHostIP,
                                            sToNodeLocalInfo->mInternalRPPortNo) != IDE_SUCCESS );

    // temporary replication for resharding must be start on FromNode
    // to prevent starting on ToNode, set the replication host of ToNode to self host
    IDE_TEST(createReplWithNewTransCommit( aStatement,
                                           aToNodeName,
                                           aReplicationName,
                                           sToNodeLocalInfo->mInternalRPHostIP,
                                           sToNodeLocalInfo->mInternalRPPortNo ) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_TO_NODE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_NODE_NAME, aToNodeName ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC qdsd::prepareReplicationsForSync( qcStatement * aStatement,
                                         iduList * aNodeInfoList,
                                         SChar * aFromNodeName,
                                         SChar * aToNodeName,
                                         SChar * aReplicationName,
                                         qdReShardAttribute * aReShardAttr)
{
    qdReShardAttribute * sReShardAttr = NULL;
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };
    SChar                sObjectNameBuf[ QC_MAX_OBJECT_FULL_STR_LEN + 1 ] = {0,};
    SChar                sBackupObjectNameBuf[ QC_MAX_OBJECT_FULL_STR_LEN + 1 ] = {0,};
    UInt                 sUserID;
    /* the precede operation related to replication must be commit by other transaction */
    IDE_TEST( createReplicationsWithNewTrans( aStatement,
                                              aNodeInfoList,
                                              aFromNodeName,
                                              aToNodeName,
                                              aReplicationName ) != IDE_SUCCESS );

    /* replication add table shard key dist table(range) and solo */
    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        if ( sReShardAttr->mObjectType == 'T' )
        {
            if (QC_IS_NULL_NAME(sReShardAttr->mUserName) == ID_TRUE)
            {
                sUserID = QCG_GET_SESSION_USER_ID(aStatement);
                IDE_TEST(qciMisc::getUserName(aStatement, sUserID, sUserName) != IDE_SUCCESS);
            }
            else
            {
                QC_STR_COPY(sUserName, sReShardAttr->mUserName);
            }

            IDE_TEST( getTableNames( sReShardAttr,
                                     sUserName,
                                     QC_MAX_OBJECT_FULL_STR_LEN + 1,
                                     sObjectNameBuf,
                                     sBackupObjectNameBuf ) != IDE_SUCCESS );

                IDE_TEST( alterReplAddTableWithNewTrans( aStatement,
                                                         aFromNodeName,
                                                         aReplicationName,
                                                         sObjectNameBuf,
                                                         sBackupObjectNameBuf )
                          != IDE_SUCCESS );

                IDE_TEST( alterReplAddTableWithNewTrans( aStatement,
                                                         aToNodeName,
                                                         aReplicationName,
                                                         sBackupObjectNameBuf,
                                                         sObjectNameBuf )
                          != IDE_SUCCESS )

        }
        else
        {
            /* 'P' procedure: do noting */
        }
    }
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
IDE_RC qdsd::executeLocalSQL(qcStatement         * aStatement,
                             qciStmtType           aSQLBufStmtType,
                             SChar               * aSQLBuf,
                             UInt                  aSQLBufSize,
                             const SChar         * aSQLFormat,
                             ... )
{
    SInt    sWrittenSize = 0;
    va_list sAp;
    vSLong              sRowCnt;
    sdiLocalMetaInfo    sLocalMetaInfo;
    smiStatement      * sOldStmt = NULL;
    smiStatement        sSmiStmt;
    UInt                sSmiStmtFlag = SMI_STATEMENT_NORMAL|SMI_STATEMENT_ALL_CURSOR;
    SInt                sState = 0;
    smiStatement* spRootStmt;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', no_meta_exist );
    
    if ( aSQLFormat != NULL )
    {
        va_start(sAp, aSQLFormat);
        
        sWrittenSize = idlOS::vsnprintf( aSQLBuf, 
                                         aSQLBufSize,
                                         aSQLFormat,
                                         sAp );
        va_end(sAp);
        
        IDE_TEST_RAISE(sWrittenSize < 0, BUF_OUTPUT_ERROR);
        IDE_TEST_RAISE(sWrittenSize >= (SInt)aSQLBufSize, ERR_NOT_ENOUGH_BUFSIZE );
    }

    ideLog::log(IDE_SD_17,"[SHARD_META] executeLocalSQL Begin: %s", aSQLBuf);
    qcg::getSmiStmt( aStatement, &sOldStmt );
    spRootStmt = sOldStmt->getTrans()->getStatement();
    IDE_TEST( sOldStmt->end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );

    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 1;

    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                              spRootStmt,
                              sSmiStmtFlag )
              != IDE_SUCCESS );
    sState = 2;

    switch (qciMisc::getStmtType(aSQLBufStmtType))
    {
        case QCI_STMT_MASK_DDL:
            /* R2HA 2 qcstatement -> 1 smstatement problem , implicit savepoint problem */
            IDE_TEST( qciMisc::runRollbackableInternalDDL( aStatement,
                                                           QC_SMI_STMT( aStatement ),
                                                           QCG_GET_SESSION_USER_ID( aStatement ),
                                                           aSQLBuf )
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_SP:
        case QCI_STMT_MASK_DML:
            IDE_TEST( qcg::runSQLforShardMeta( QC_SMI_STMT( aStatement ),
                                               aSQLBuf,
                                               & sRowCnt,
                                               NULL ) //aStatement->session ) 
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_DCL:
            IDE_TEST( qciMisc::runDCLforInternal( aStatement,
                                                  aSQLBuf,
                                                  aStatement->session->mMmSession )
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_DB:
        default:
            IDE_DASSERT(0);
            break;
    }
    sState = 1;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );
    
    IDE_TEST( sOldStmt->begin( aStatement->mStatistics,
                               spRootStmt,
                               sSmiStmtFlag )
                != IDE_SUCCESS );
    sState = 0;
    qcg::setSmiStmt( aStatement, sOldStmt );

    ideLog::log(IDE_SD_17,"[SHARD_META] executeLocalSQL Success: %s", aSQLBuf);

    return IDE_SUCCESS;

    IDE_EXCEPTION( no_meta_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( BUF_OUTPUT_ERROR );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeLocalSQL", "Internal SQL does not exist." ) );
    }
    IDE_EXCEPTION( ERR_NOT_ENOUGH_BUFSIZE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeLocalSQL", "The length of Internal SQL is too long." ) );
    }

    IDE_EXCEPTION_END;
    ideLog::log(IDE_SD_1,"[SHARD_META_ERROR] executeLocalSQL Failure: %s", aSQLBuf);
    IDE_PUSH();
    switch ( sState )
    {
    case 2:
        (void)sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE);
    case 1:
        (void) sOldStmt->begin( aStatement->mStatistics,
                                spRootStmt,
                                sSmiStmtFlag );
        
        qcg::setSmiStmt( aStatement, sOldStmt );
    default:
        break;
    }
    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC qdsd::executeLocalSQLWithSession( qcStatement         * aStatement,
                                         qciStmtType           aSQLBufStmtType,
                                         SChar               * aSQLBuf,
                                         UInt                  aSQLBufSize,
                                         const SChar         * aSQLFormat,
                                         ... )
{
    SInt    sWrittenSize = 0;
    va_list sAp;
    vSLong              sRowCnt;
    sdiLocalMetaInfo    sLocalMetaInfo;
    smiStatement      * sOldStmt = NULL;
    smiStatement        sSmiStmt;
    UInt                sSmiStmtFlag = SMI_STATEMENT_NORMAL|SMI_STATEMENT_ALL_CURSOR;
    SInt                sState = 0;
    smiStatement* spRootStmt;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', no_meta_exist );
    
    if ( aSQLFormat != NULL )
    {
        va_start(sAp, aSQLFormat);
        
        sWrittenSize = idlOS::vsnprintf( aSQLBuf, 
                                         aSQLBufSize,
                                         aSQLFormat,
                                         sAp );
        va_end(sAp);
        
        IDE_TEST_RAISE(sWrittenSize < 0, BUF_OUTPUT_ERROR);
        IDE_TEST_RAISE(sWrittenSize >= (SInt)aSQLBufSize, ERR_NOT_ENOUGH_BUFSIZE );
    }

    ideLog::log(IDE_SD_17,"[SHARD_META] executeLocalSQLWithSession Begin: %s", aSQLBuf);
    qcg::getSmiStmt( aStatement, &sOldStmt );
    spRootStmt = sOldStmt->getTrans()->getStatement();
    IDE_TEST( sOldStmt->end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );

    qcg::setSmiStmt( aStatement, &sSmiStmt );
    sState = 1;

    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                              spRootStmt,
                              sSmiStmtFlag )
              != IDE_SUCCESS );
    sState = 2;

    switch (qciMisc::getStmtType(aSQLBufStmtType))
    {
        case QCI_STMT_MASK_DDL:
            /* R2HA 2 qcstatement -> 1 smstatement problem , implicit savepoint problem */
            IDE_TEST( qciMisc::runRollbackableInternalDDL( aStatement,
                                                           QC_SMI_STMT( aStatement ),
                                                           QCG_GET_SESSION_USER_ID( aStatement ),
                                                           aSQLBuf )
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_SP:
        case QCI_STMT_MASK_DML:
            IDE_TEST( qcg::runSQLforShardMeta( QC_SMI_STMT( aStatement ),
                                               aSQLBuf,
                                               & sRowCnt,
                                               aStatement->session ) 
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_DCL:
            IDE_TEST( qciMisc::runDCLforInternal( aStatement,
                                                  aSQLBuf,
                                                  aStatement->session->mMmSession )
                      != IDE_SUCCESS );
            break;
        case QCI_STMT_MASK_DB:
        default:
            IDE_DASSERT(0);
            break;
    }
    sState = 1;
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );
    
    IDE_TEST( sOldStmt->begin( aStatement->mStatistics,
                               spRootStmt,
                               sSmiStmtFlag )
                != IDE_SUCCESS );
    sState = 0;
    qcg::setSmiStmt( aStatement, sOldStmt );

    ideLog::log(IDE_SD_17,"[SHARD_META] executeLocalSQLWithSession Success: %s", aSQLBuf);

    return IDE_SUCCESS;

    IDE_EXCEPTION( no_meta_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( BUF_OUTPUT_ERROR );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeLocalSQLWithSession", "Internal SQL does not exist." ) );
    }
    IDE_EXCEPTION( ERR_NOT_ENOUGH_BUFSIZE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeLocalSQLWithSession", "The length of Internal SQL is too long." ) );
    }

    IDE_EXCEPTION_END;
    ideLog::log(IDE_SD_1,"[SHARD_META_ERROR] executeLocalSQLWithSession Failure: %s", aSQLBuf);
    IDE_PUSH();
    switch ( sState )
    {
    case 2:
        (void)sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE);
    case 1:
        (void) sOldStmt->begin( aStatement->mStatistics,
                                spRootStmt,
                                sSmiStmtFlag );
        
        qcg::setSmiStmt( aStatement, sOldStmt );
    default:
        break;
    }
    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC qdsd::executeRemoteSQL( qcStatement         * aStatement,
                               SChar               * aNodeName,
                               sdiInternalOperation  aInternalOP,
                               qciStmtType           aSQLBufStmtType,
                               SChar               * aSQLBuf,
                               UInt                  aSQLBufSize,
                               const SChar         * aSQLFormat,
                               ... )
{
    SInt    sWrittenSize = 0;
    va_list sAp;
    UInt    sExecCount;
    vSLong              sRowCnt;
    sdiLocalMetaInfo    sLocalMetaInfo;
    smiStatement      * sOldStmt = NULL;
    smiStatement        sSmiStmt;
    UInt                sSmiStmtFlag = SMI_STATEMENT_NORMAL|SMI_STATEMENT_ALL_CURSOR;
    SInt                sState = 0;
    smiStatement      * spRootStmt = NULL;
    idBool              sIsMyNode = ID_FALSE;

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sLocalMetaInfo.mNodeName[0] == '\0', no_meta_exist );
    IDE_TEST_RAISE( SDI_IS_NULL_NAME(sLocalMetaInfo.mNodeName) == ID_TRUE , no_meta_exist );

    if ( aSQLFormat != NULL )
    {
        va_start(sAp, aSQLFormat);

        sWrittenSize = idlOS::vsnprintf( aSQLBuf, 
                                         aSQLBufSize,
                                         aSQLFormat,
                                         sAp );
        va_end(sAp);

        IDE_TEST_RAISE(sWrittenSize < 0, BUF_OUTPUT_ERROR);
        IDE_TEST_RAISE(sWrittenSize >= (SInt)aSQLBufSize, ERR_NOT_ENOUGH_BUFSIZE );
    }

    sIsMyNode = sdi::isSameNode(sLocalMetaInfo.mNodeName, aNodeName);

    if ( sIsMyNode == ID_TRUE )
    {
        ideLog::log(IDE_SD_17,
                    "[SHARD_META] [Node: %s] executeReomteSQL for local: %s",
                    aNodeName,
                    aSQLBuf);
        /* it's me */
        qcg::getSmiStmt( aStatement, &sOldStmt );
        spRootStmt = sOldStmt->getTrans()->getStatement();
        IDE_TEST( sOldStmt->end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );

        qcg::setSmiStmt( aStatement, &sSmiStmt );
        sState = 1;

        IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                                    spRootStmt,
                                    sSmiStmtFlag )
                    != IDE_SUCCESS );
        sState = 2;

        switch (qciMisc::getStmtType(aSQLBufStmtType))
        {
            case QCI_STMT_MASK_DDL:
                /* R2HA 2 qcstatement -> 1 smstatement problem , implicit savepoint problem */
                IDE_TEST( qciMisc::runRollbackableInternalDDL( aStatement,
                                                               QC_SMI_STMT( aStatement ),
                                                               QCG_GET_SESSION_USER_ID( aStatement ),
                                                               aSQLBuf )
                          != IDE_SUCCESS );
                break;
            case QCI_STMT_MASK_SP:                            
                if (( aStatement->session->mQPSpecific.mFlag & QC_SESSION_SAHRD_ADD_CLONE_MASK ) ==
                    QC_SESSION_SAHRD_ADD_CLONE_TRUE )
                {
                    sState = 1;
                    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

                    IDE_TEST( qcg::runSQLforShardMeta( QC_SMI_STMT( aStatement ),
                                                       aSQLBuf,
                                                       & sRowCnt,
                                                       NULL ) //aStatement->session ) 
                              != IDE_SUCCESS );

                    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                                              spRootStmt,
                                              sSmiStmtFlag )
                              != IDE_SUCCESS );
                    sState = 2;
                }
                else
                {
                    /* Local�̶� InternalOp�� �ʿ��ؼ� ShardExecDirect ��� */
                    sState = 1;
                    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );
                    IDE_TEST( sdi::shardExecDirect( aStatement,
                                                    aNodeName,
                                                    (SChar*)aSQLBuf,
                                                    (UInt) idlOS::strlen( aSQLBuf ),
                                                    aInternalOP,
                                                    &sExecCount,
                                                    NULL,
                                                    NULL,
                                                    0,
                                                    NULL )
                              != IDE_SUCCESS );
                    IDE_TEST_RAISE( sExecCount != 1, ERR_REMOTE_EXECUTION );
                    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                                              spRootStmt,
                                              sSmiStmtFlag )
                              != IDE_SUCCESS );
                    sState = 2;
                }
                break;
            case QCI_STMT_MASK_DCL:
                if (( aStatement->session->mQPSpecific.mFlag & QC_SESSION_SAHRD_ADD_CLONE_MASK ) ==
                    QC_SESSION_SAHRD_ADD_CLONE_TRUE )
                {
                    sState = 1;
                    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

                    IDE_TEST( qciMisc::runDCLforInternal( aStatement,
                                                          aSQLBuf,
                                                          aStatement->session->mMmSession )
                              != IDE_SUCCESS);

                    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                                              spRootStmt,
                                              sSmiStmtFlag )
                              != IDE_SUCCESS );
                    sState = 2;
                }
                else
                {
                    /* Local�̶� InternalOp�� �ʿ��ؼ� ShardExecDirect ��� */
                    sState = 1;
                    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );
                    IDE_TEST( sdi::shardExecDirect( aStatement,
                                                    aNodeName,
                                                    (SChar*)aSQLBuf,
                                                    (UInt) idlOS::strlen( aSQLBuf ),
                                                    aInternalOP,
                                                    &sExecCount,
                                                    NULL,
                                                    NULL,
                                                    0,
                                                    NULL )
                              != IDE_SUCCESS );
                    IDE_TEST_RAISE( sExecCount != 1, ERR_REMOTE_EXECUTION );
                    IDE_TEST( sSmiStmt.begin( aStatement->mStatistics,
                                              spRootStmt,
                                              sSmiStmtFlag )
                              != IDE_SUCCESS );
                    sState = 2;
                }
                break;
            case QCI_STMT_MASK_DML:
                IDE_TEST(qcg::runDMLforDDL( QC_SMI_STMT( aStatement ),
                                            aSQLBuf,
                                            & sRowCnt ) != IDE_SUCCESS);

                break;
            case QCI_STMT_MASK_DB:
            default:
                IDE_DASSERT(0);
                break;
        }
        sState = 1;
        IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS ) != IDE_SUCCESS );

        IDE_TEST( sOldStmt->begin( aStatement->mStatistics,
                                    spRootStmt,
                                    sSmiStmtFlag )
                    != IDE_SUCCESS );
        sState = 0;
        qcg::setSmiStmt( aStatement, sOldStmt );
    }
    else
    {
        ideLog::log(IDE_SD_17,
                    "[SHARD_META] [Node: %s] executeRemoteSQL: %s",
                    aNodeName,
                    aSQLBuf);
        IDE_TEST( sdi::shardExecDirect( aStatement,
                                        aNodeName,
                                        (SChar*)aSQLBuf,
                                        (UInt) idlOS::strlen( aSQLBuf ),
                                        aInternalOP,
                                        &sExecCount,
                                        NULL,
                                        NULL,
                                        0,
                                        NULL )
                    != IDE_SUCCESS );
        IDE_TEST_RAISE( sExecCount != 1 , ERR_REMOTE_EXECUTION );
    }

    ideLog::log(IDE_SD_17,"[SHARD_META] executeRemoteSQL Success: %s", aSQLBuf);

    return IDE_SUCCESS;

    IDE_EXCEPTION( no_meta_exist )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_META_NOT_CREATED ) );
    }
    IDE_EXCEPTION( BUF_OUTPUT_ERROR );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeRemoteSQL", "Internal SQL does not exist." ) );
    }
    IDE_EXCEPTION( ERR_NOT_ENOUGH_BUFSIZE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeRemoteSQL", "The length of Internal SQL is too long." ) );
    }
    IDE_EXCEPTION( ERR_REMOTE_EXECUTION );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_EXECUTE_REMOTE_SQL_FAILED, aSQLBuf ) );
    }
    IDE_EXCEPTION_END;
    ideLog::log(IDE_SD_1,"[SHARD_META_ERROR] executeRemoteSQL Failure: %s", aSQLBuf);
    IDE_PUSH();
    switch ( sState )
    {
        case 2:
            (void)sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE);
        case 1:
            (void) sOldStmt->begin( aStatement->mStatistics,
                                    spRootStmt,
                                    sSmiStmtFlag );
            
            qcg::setSmiStmt( aStatement, sOldStmt );
        default:
            break;
    }
    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC qdsd::executeRemoteSQLWithNewTransArg( qcStatement         * aStatement,
                                              SChar               * aNodeName,
                                              sdiInternalOperation  /*aInternalOP*/,
                                              qciStmtType           /*aSQLBufStmtType*/,
                                              SChar               * aSQLBuf,
                                              UInt                  aSQLBufSize,
                                              idBool                aIsDDLTimeOut,
                                              const SChar         * aSQLFormat,
                                              ... )
{
    SInt    sWrittenSize = 0;
    va_list sAp;

    if ( aSQLFormat != NULL )
    {
        va_start(sAp, aSQLFormat);

        sWrittenSize = idlOS::vsnprintf( aSQLBuf,
                                         aSQLBufSize,
                                         aSQLFormat,
                                         sAp );
        va_end(sAp);

        IDE_TEST_RAISE(sWrittenSize < 0, BUF_OUTPUT_ERROR);
        IDE_TEST_RAISE(sWrittenSize >= (SInt)aSQLBufSize, ERR_NOT_ENOUGH_BUFSIZE );
    }

    IDE_TEST( executeRemoteSQLWithNewTrans(aStatement,
                                           aNodeName,
                                           aSQLBuf,
                                           aIsDDLTimeOut )
              != IDE_SUCCESS );

    ideLog::log(IDE_SD_17,"[SHARD_META] executeRemoteSQLWithNewTransArg Success: %s", aSQLBuf);

    return IDE_SUCCESS;

    IDE_EXCEPTION( BUF_OUTPUT_ERROR );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeRemoteSQLWithNewTransArg", "Internal SQL does not exist." ) );
    }
    IDE_EXCEPTION( ERR_NOT_ENOUGH_BUFSIZE );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QMC_UNEXPECTED_ERROR,"qdsd::executeRemoteSQLWithNewTransArg", "The length of Internal SQL is too long." ) );
    }
    IDE_EXCEPTION_END;

    ideLog::log(IDE_SD_1,"[SHARD_META_ERROR] executeRemoteSQLWithNewTransArg Failure: %s", aSQLBuf);

    return IDE_FAILURE;
}

IDE_RC qdsd::lockAllRemoteReshardTables( qcStatement * aStatement,
                                         SChar * aSQLBuf,
                                         UInt    aSQLBufSize,
                                         SChar * aFromNodeName,
                                         SChar * aToNodeName,
                                         qdReShardAttribute * aReShardAttr )
{
    UInt sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);
    UInt  sLoopCnt = 0;
    idBool sIsCancel = ID_FALSE;
    idBool sIsLockAcquired = ID_FALSE;
    qdReShardAttribute * sReShardAttr = NULL;
    SChar sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar sTableName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar sPartitionName[QC_MAX_OBJECT_NAME_LEN] = {0,};

    /* the reomte cancel is not running. therefore remote lock timeout property sec(shard_ddl_lock_timeout).
     * if a application lock order is A->B->C then executing reshard table should be in the order A, B and C 
     * if A table is locked then the next table lock may will be acquired.
     */
    sLoopCnt = 0;
    while ( (sLoopCnt++ < sShardDDLLockTryCount) && (sIsLockAcquired != ID_TRUE ) )
    {
        sIsCancel = ID_FALSE;
        sIsLockAcquired = ID_FALSE;
        IDE_TEST( executeRemoteSQL( aStatement, 
                                    aFromNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_DCL,
                                    aSQLBuf,
                                    aSQLBufSize,
                                    "SAVEPOINT __RESHARD_SVP0") != IDE_SUCCESS);

        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aToNodeName,
                                        SDI_INTERNAL_OP_NORMAL,
                                        QCI_STMT_MASK_DCL,
                                        aSQLBuf,
                                        aSQLBufSize,
                                        "SAVEPOINT __RESHARD_SVP0") != IDE_SUCCESS);
        }

        for ( sReShardAttr = aReShardAttr;
              sReShardAttr != NULL;
              sReShardAttr = sReShardAttr->next )
        {
            if ( sReShardAttr->mObjectType == 'T' )
            {
                IDE_DASSERT(QC_IS_NULL_NAME(sReShardAttr->mUserName) != ID_TRUE); // the username was set at validation time.
                QC_STR_COPY(sUserName, sReShardAttr->mUserName);
                QC_STR_COPY(sTableName, sReShardAttr->mObjectName);
                if ( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE )
                {
                    QC_STR_COPY(sPartitionName, sReShardAttr->mPartitionName);
                    /* acquire table lock to fromnode */
                    if( executeRemoteSQL( aStatement,
                                          aFromNodeName,
                                          SDI_INTERNAL_OP_NORMAL,
                                          QCI_STMT_MASK_DML,
                                          aSQLBuf,
                                          aSQLBufSize,
                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                          //"LOCK TABLE %s.%s PARTITION (%s) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                          sUserName,
                                          sTableName,
                                          //sPartitionName,
                                          sRemoteLockTimeout
                                        ) != IDE_SUCCESS )
                    {
                        /* cancel retry */
                        sIsCancel = ID_TRUE;
                        break;
                    }
                    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                    {
                        /* acquire table lock to tonode*/
                        if( executeRemoteSQL( aStatement,
                                              aToNodeName,
                                              SDI_INTERNAL_OP_NORMAL,
                                              QCI_STMT_MASK_DCL,
                                              aSQLBuf,
                                              aSQLBufSize,
                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                              //"LOCK TABLE %s.%s PARTITION (%s) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                              sUserName,
                                              sTableName,
                                              //sPartitionName,
                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                        {
                            /* cancel retry */
                            sIsCancel = ID_TRUE;
                            break;
                        }
                    }
                }
                else /* PartitionName IS NULL : solo table */
                {
                    /* acquire table lock to fromnode */
                    if( executeRemoteSQL( aStatement,
                                          aFromNodeName,
                                          SDI_INTERNAL_OP_NORMAL,
                                          QCI_STMT_MASK_DML,
                                          aSQLBuf,
                                          aSQLBufSize,
                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                          sUserName,
                                          sTableName,
                                          sRemoteLockTimeout
                                        ) != IDE_SUCCESS )
                    {
                        /* cancel retry */
                        sIsCancel = ID_TRUE;
                        break;
                    }
                    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                    {
                        /* acquire table lock to tonode*/
                        if( executeRemoteSQL( aStatement,
                                              aToNodeName,
                                              SDI_INTERNAL_OP_NORMAL,
                                              QCI_STMT_MASK_DML,
                                              aSQLBuf,
                                              aSQLBufSize,
                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                              sUserName,
                                              sTableName,
                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                        {
                            /* cancel retry */

                            sIsCancel = ID_TRUE;
                            break;
                        }
                    }
                }
            }
            else
            {
                /* 'P' procedure: R2HA lock procedure %s.%s IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT */
            }
        }
        if ( sIsCancel == ID_TRUE )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aFromNodeName,
                                        SDI_INTERNAL_OP_NORMAL,
                                        QCI_STMT_MASK_DCL,
                                        aSQLBuf,
                                        aSQLBufSize,
                                        "ROLLBACK TO SAVEPOINT __RESHARD_SVP0") != IDE_SUCCESS );
            if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
            {
                IDE_TEST( executeRemoteSQL( aStatement, 
                                            aToNodeName,
                                            SDI_INTERNAL_OP_NORMAL,
                                            QCI_STMT_MASK_DCL,
                                            aSQLBuf,
                                            aSQLBufSize,
                                            "ROLLBACK TO SAVEPOINT __RESHARD_SVP0") != IDE_SUCCESS );
            }
            sIsLockAcquired = ID_FALSE;
        }
        else
        {
            sIsLockAcquired = ID_TRUE;
            break;
        }
    }
    IDE_TEST_RAISE(sIsLockAcquired == ID_FALSE, ERR_LOCK);

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_LOCK );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_LOCK_FAIL ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    (void)executeRemoteSQL( aStatement, 
                            aFromNodeName,
                            SDI_INTERNAL_OP_NORMAL,
                            QCI_STMT_MASK_DCL,
                            aSQLBuf,
                            aSQLBufSize,
                            "ROLLBACK TO SAVEPOINT __RESHARD_SVP0" );
    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
    {
        (void)executeRemoteSQL( aStatement, 
                                aToNodeName,
                                SDI_INTERNAL_OP_NORMAL,
                                QCI_STMT_MASK_DCL,
                                aSQLBuf,
                                aSQLBufSize,
                                "ROLLBACK TO SAVEPOINT __RESHARD_SVP0" );
    }
    IDE_POP();
    return IDE_FAILURE;
}


IDE_RC qdsd::resetShardMetaAndSwapPartition( qcStatement * aStatement,
                                             SChar * aSQLBuf,
                                             UInt    aSQLBufSize,
                                             SChar * aFromNodeName,
                                             SChar * aToNodeName,
                                             iduList * aNodeInfoList,
                                             qdReShardAttribute * aReShardAttr )
{
    qdReShardAttribute * sReShardAttr = NULL;
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sTableName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sPartitionName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    sdiLocalMetaInfo   * sNodeInfo = NULL;
    iduListNode        * sNode = NULL;
    iduListNode        * sDummy = NULL;
    sdiShardObjectType    sShardTableType = SDI_NON_SHARD_OBJECT;
    SChar                sProcKeyValue[SDI_RANGE_VARCHAR_MAX_SIZE + 1];
    UInt                 sIsDefault = 0;
    SChar                sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_DASSERT(QC_IS_NULL_NAME(sReShardAttr->mUserName) != ID_TRUE); // the username was set at validation time.
        IDE_DASSERT(sReShardAttr->mShardObjectInfo != NULL);
        QC_STR_COPY(sUserName, sReShardAttr->mUserName);
        QC_STR_COPY(sTableName, sReShardAttr->mObjectName);

        if ( sReShardAttr->mObjectType == 'T' )
        {
            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableName );
            
            sShardTableType = sdi::getShardObjectType(&sReShardAttr->mShardObjectInfo->mTableInfo );
            IDE_DASSERT( IDU_LIST_IS_EMPTY( aNodeInfoList ) != ID_TRUE );
            switch ( sShardTableType )
            {
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    IDE_DASSERT( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE );
                    QC_STR_COPY(sPartitionName, sReShardAttr->mPartitionName);

                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        /* EXEC dbms_shard.reset_shard_resident_node('SYS','L1','NODE3','NODE4','300'); */
                        // reset_shard_resident_node can not modify default node therefore do not use it.

                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_PARTITION_NODE('SYS','@{aTable}', 'NODE3', 'NODE1', 'P4'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_partition_node('"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"',"
                                                        QCM_SQL_VARCHAR_FMT")",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        aToNodeName,
                                                        sPartitionName ) != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_PARTITION_NODE('SYS','@{aTable}', 'NODE3', '', 'P4'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_partition_node('"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','',"
                                                        QCM_SQL_VARCHAR_FMT")",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        sPartitionName ) != IDE_SUCCESS );
                        }
                    }

                    IDE_TEST( executeRemoteSQL( aStatement,
                                                aFromNodeName,
                                                SDI_INTERNAL_OP_NORMAL,
                                                QCI_STMT_MASK_DDL,
                                                aSQLBuf,
                                                aSQLBufSize,
                                                "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                                sUserName,
                                                sBufferNameStr,
                                                sUserName,
                                                sTableName,
                                                sPartitionName ) != IDE_SUCCESS );
                    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                    {
                        /* ToNode: ALTER TABLE L1_REBUILD REPLACE L1 PARTITION P41 */
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aToNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sUserName,
                                                    sTableName,
                                                    sPartitionName ) != IDE_SUCCESS );
                    }
                    break;

                case SDI_SOLO_DIST_OBJECT:
                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{aTable}', 'NODE3', 'NODE1'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        aToNodeName) != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{aTable}', 'NODE3', ''); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName ) != IDE_SUCCESS );
                        }
                    }

                    IDE_TEST( executeRemoteSQL( aStatement,
                                                aFromNodeName,
                                                SDI_INTERNAL_OP_NORMAL,
                                                QCI_STMT_MASK_DDL,
                                                aSQLBuf,
                                                aSQLBufSize,
                                                "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                sUserName,
                                                sBufferNameStr,
                                                sUserName,
                                                sTableName) != IDE_SUCCESS );
                    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                    {
                        /* ToNode: ALTER TABLE S1_REBUILD REPLACE S1 */
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aToNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sUserName,
                                                    sTableName) != IDE_SUCCESS );
                    }
                    break;

                case SDI_CLONE_DIST_OBJECT:
                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* TASK-7307 DML Data Consistency in Shard
                             *   dbms_shard.set_shard_clone_local �Լ��� 4��° ���� �߰��ؼ� DDL���� ȣ����� ǥ�� */
                            /* EXEC DBMS_SHARD.SET_SHARD_CLONE_LOCAL('SYS','@{aTable}', 'NODE4', 1); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.set_shard_clone_local('"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"', "
                                                        "NULL, "QCM_SQL_UINT32_FMT")", // TASK-7307
                                                        sUserName,
                                                        sTableName,
                                                        aToNodeName,
                                                        SDM_CALLED_BY_SHARD_MOVE ) /* TASK-7307 */
                                      != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{aTable}', 'NODE3', ''); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName ) != IDE_SUCCESS );
                        }
                    }

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                aFromNodeName,
                                                SDI_INTERNAL_OP_NORMAL,
                                                QCI_STMT_MASK_DDL,
                                                aSQLBuf,
                                                aSQLBufSize,
                                                "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" TOUCH",
                                                sUserName, 
                                                sTableName ) != IDE_SUCCESS );
                    if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                    {
                        /* ToNode: ALTER TABLE S1_REBUILD REPLACE S1 */
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aToNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sUserName,
                                                    sTableName) != IDE_SUCCESS );
                    }
                    break;
                case SDI_NON_SHARD_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            /* 'P' procedure */
            sShardTableType = sdi::getShardObjectType(&sReShardAttr->mShardObjectInfo->mTableInfo );
            switch (sShardTableType)
            {
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    IDE_DASSERT( sReShardAttr->mKeyValue != NULL );
                    IDE_TEST( getProcedureKeyValue( sReShardAttr,
                                                    sProcKeyValue ) != IDE_SUCCESS );
                    IDE_DASSERT( IDU_LIST_IS_EMPTY( aNodeInfoList ) != ID_TRUE );
                    if(sProcKeyValue[0] == '\0')
                    {
                        sIsDefault = 1;
                    }
                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{Procedure}', 'NODE3', 'NODE1','300'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node("
                                                        "'"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"',"QCM_SQL_VARCHAR_FMT",'',INTEGER'%"ID_INT32_FMT"')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        aToNodeName,
                                                        sProcKeyValue,
                                                        sIsDefault) != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{Procedure}', 'NODE3', '','300'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','',"QCM_SQL_VARCHAR_FMT" )",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        sProcKeyValue) != IDE_SUCCESS );
                        }
                        /* R2HA: FromNode: ALTER PROCEDURE PROC1 TOUCH
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aFromNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE %s.%s TOUCH",
                                                    sUserName,
                                                    sTableName ) != IDE_SUCCESS );
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        aToNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "ALTER TABLE %s.%s TOUCH",
                                                        sUserName,
                                                        sTableName ) != IDE_SUCCESS );
                        }
                        */
                    }
                    break;
                case SDI_SOLO_DIST_OBJECT:
                    sProcKeyValue[0] = '\0';
                    sIsDefault = 0;
                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{Procedure}', 'NODE3', 'NODE1','300'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node("
                                                        "'"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','','',INTEGER'%"ID_INT32_FMT"')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName,
                                                        aToNodeName,
                                                        sIsDefault) != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{Procedure}', 'NODE3', '','300'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','"QCM_SQL_STRING_SKIP_FMT"','','')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName ) != IDE_SUCCESS );
                        }
                        /* R2HA: FromNode: ALTER PROCEDURE PROC1 TOUCH
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aFromNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE %s.%s TOUCH",
                                                    sUserName,
                                                    sTableName ) != IDE_SUCCESS );
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        aToNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "ALTER TABLE %s.%s TOUCH",
                                                        sUserName,
                                                        sTableName ) != IDE_SUCCESS );
                        }
                        */
                    }
                    break;
                case SDI_CLONE_DIST_OBJECT:
                    sProcKeyValue[0] = '\0';
                    sIsDefault = 0;
                    IDU_LIST_ITERATE_SAFE( aNodeInfoList, sNode, sDummy )
                    {
                        sNodeInfo = (sdiLocalMetaInfo *)sNode->mObj;
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            /* EXEC DBMS_SHARD.SET_SHARD_CLONE_LOCAL('SYS','@{Procedure}', 'NODE3'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.set_shard_clone_local('"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"')",
                                                        sUserName,
                                                        sTableName,
                                                        aToNodeName ) != IDE_SUCCESS );
                        }
                        else
                        {
                            /* EXEC DBMS_SHARD.RESET_SHARD_RESIDENT_NODE('SYS','@{Procedure}', 'NODE3', '','300'); */
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sNodeInfo->mNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_SP,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "EXEC dbms_shard.reset_shard_resident_node('"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','"
                                                        QCM_SQL_STRING_SKIP_FMT"','','')",
                                                        sUserName,
                                                        sTableName,
                                                        aFromNodeName ) != IDE_SUCCESS );
                        }
                        /* R2HA: FromNode: ALTER PROCEDURE PROC1 TOUCH
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aFromNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE %s.%s TOUCH",
                                                    sUserName,
                                                    sTableName ) != IDE_SUCCESS );
                        if ( SDI_IS_NULL_NAME(aToNodeName) != ID_TRUE )
                        {
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        aToNodeName,
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "ALTER TABLE %s.%s TOUCH",
                                                        sUserName,
                                                        sTableName ) != IDE_SUCCESS );
                        }
                        */
                    }
                    break;
                case SDI_NON_SHARD_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::moveReplication( qcStatement * aStatement,
                              SChar       * aSQLBuf,
                              UInt          aSQLBufSize,
                              UInt          aKSafety,
                              sdiReplicaSet  * aFromReplicaSet,
                              sdiReplicaSet  * aToReplicaSet,
                              qdReShardAttribute * aReShardAttr )
{
    qdReShardAttribute * sReShardAttr = NULL;
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sTableName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sPartitionName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    sdiShardObjectType    sShardTableType = SDI_NON_SHARD_OBJECT;
    SChar              * sBeforeFromNodeName[3];
    SChar              * sBeforeToNodeName[3];
    SChar              * sBeforeRepName[3];
    SChar              * sAfterFromNodeName[3];
    SChar              * sAfterToNodeName[3];
    SChar              * sAfterRepName[3];
    UInt                 k = 1;
    qdShardParseTree  * sParseTree = NULL;
    SChar               sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    IDE_TEST_RAISE( aKSafety < 1, ERR_KSAFETY );

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    sShardTableType = sdi::getShardObjectType( &aReShardAttr->mShardObjectInfo->mTableInfo );
    IDE_TEST_CONT(sShardTableType == SDI_CLONE_DIST_OBJECT, NORMAL_EXIT);

    sBeforeFromNodeName[0] = SDM_NA_STR;
    sBeforeFromNodeName[1] = aFromReplicaSet->mFirstReplFromNodeName;
    sBeforeFromNodeName[2] = aFromReplicaSet->mSecondReplFromNodeName;

    sBeforeToNodeName[0] = SDM_NA_STR;
    sBeforeToNodeName[1] = aFromReplicaSet->mFirstReplToNodeName;
    sBeforeToNodeName[2] = aFromReplicaSet->mSecondReplToNodeName;

    sBeforeRepName[0] = SDM_NA_STR;
    sBeforeRepName[1] = aFromReplicaSet->mFirstReplName;
    sBeforeRepName[2] = aFromReplicaSet->mSecondReplName;

    sAfterFromNodeName[0] = SDM_NA_STR;
    sAfterFromNodeName[1] = aToReplicaSet->mFirstReplFromNodeName;
    sAfterFromNodeName[2] = aToReplicaSet->mSecondReplFromNodeName;

    sAfterToNodeName[0] = SDM_NA_STR;
    sAfterToNodeName[1] = aToReplicaSet->mFirstReplToNodeName;
    sAfterToNodeName[2] = aToReplicaSet->mSecondReplToNodeName;

    sAfterRepName[0] = SDM_NA_STR;
    sAfterRepName[1] = aToReplicaSet->mFirstReplName;
    sAfterRepName[2] = aToReplicaSet->mSecondReplName;

    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_DASSERT(QC_IS_NULL_NAME(sReShardAttr->mUserName) != ID_TRUE); // the username was set at validation time.
        IDE_DASSERT(sReShardAttr->mShardObjectInfo != NULL); // the shardobjectinfo was set at validation time.

        if ( sReShardAttr->mObjectType == 'T' )
        {
            QC_STR_COPY(sUserName, sReShardAttr->mUserName);
            QC_STR_COPY(sTableName, sReShardAttr->mObjectName);

            sShardTableType = sdi::getShardObjectType(&sReShardAttr->mShardObjectInfo->mTableInfo );

            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableName );
                        
            switch (sShardTableType)
            {
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    QC_STR_COPY(sPartitionName, sReShardAttr->mPartitionName);
                    IDE_DASSERT( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE );
                    for (k = 1; k <= aKSafety; k++ )
                    {
                        // BUG-28625
                        if (( sParseTree->mNodeCount == 2 ) && ( k == 2 ))
                        {
                            break;
                        }
                                
                        /* This function should only be called when there is something to be moved. */
                        IDE_TEST_RAISE( SDI_IS_NULL_NAME(sBeforeFromNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    sBeforeFromNodeName[k],
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "alter replication "QCM_SQL_STRING_SKIP_FMT" drop table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT" "
                                                    "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT"",
                                                    sBeforeRepName[k],
                                                    sUserName,
                                                    sTableName,
                                                    sPartitionName,
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sPartitionName )
                                    != IDE_SUCCESS );

                        IDE_TEST_RAISE( SDI_IS_NULL_NAME(sBeforeToNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    sBeforeToNodeName[k],
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "alter replication "QCM_SQL_STRING_SKIP_FMT" drop table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT" "
                                                    "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT"",
                                                    sBeforeRepName[k],
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sPartitionName,
                                                    sUserName,
                                                    sTableName,
                                                    sPartitionName )
                                    != IDE_SUCCESS );

                        if ( SDI_IS_NULL_NAME(sAfterFromNodeName[k]) != ID_TRUE ) /* toreplicaset has N/A in REMOVE syntax */
                        {
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sAfterFromNodeName[k],
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "alter replication "QCM_SQL_STRING_SKIP_FMT" add table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT" "
                                                        "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT"",
                                                        sAfterRepName[k],
                                                        sUserName,
                                                        sTableName,
                                                        sPartitionName,
                                                        sUserName,
                                                        sBufferNameStr,
                                                        sPartitionName )
                                        != IDE_SUCCESS );

                            IDE_TEST_RAISE( SDI_IS_NULL_NAME(sAfterToNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sAfterToNodeName[k],
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "alter replication "QCM_SQL_STRING_SKIP_FMT" add table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT" "
                                                        "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" partition "QCM_SQL_STRING_SKIP_FMT"",
                                                        sAfterRepName[k],
                                                        sUserName,
                                                        sBufferNameStr,
                                                        sPartitionName,
                                                        sUserName,
                                                        sTableName,
                                                        sPartitionName )
                                        != IDE_SUCCESS );
                        }
                    }
                    break;
                case SDI_SOLO_DIST_OBJECT:
                    for (k = 1; k <= aKSafety; k++ )
                    {
                        /* This function should only be called when there is something to be moved. */
                        IDE_TEST_RAISE( SDI_IS_NULL_NAME(sBeforeFromNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    sBeforeFromNodeName[k],
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "alter replication "QCM_SQL_STRING_SKIP_FMT" drop table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" "
                                                    "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                    sBeforeRepName[k],
                                                    sUserName,
                                                    sTableName,
                                                    sUserName,
                                                    sBufferNameStr )
                                    != IDE_SUCCESS );

                        IDE_TEST_RAISE( SDI_IS_NULL_NAME(sBeforeToNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    sBeforeToNodeName[k],
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "alter replication "QCM_SQL_STRING_SKIP_FMT" drop table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" "
                                                    "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                    sBeforeRepName[k],
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sUserName,
                                                    sTableName ) //check
                                    != IDE_SUCCESS );

                        if( SDI_IS_NULL_NAME(sAfterFromNodeName[k]) != ID_TRUE )
                        {
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sAfterFromNodeName[k],
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "alter replication "QCM_SQL_STRING_SKIP_FMT" add table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" "
                                                        "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                        sAfterRepName[k],
                                                        sUserName,
                                                        sTableName,
                                                        sUserName,
                                                        sBufferNameStr )
                                        != IDE_SUCCESS );

                            IDE_TEST_RAISE( SDI_IS_NULL_NAME(sAfterToNodeName[k]) == ID_TRUE, ERR_REPLICA_SET );
                            IDE_TEST( executeRemoteSQL( aStatement,
                                                        sAfterToNodeName[k],
                                                        SDI_INTERNAL_OP_NORMAL,
                                                        QCI_STMT_MASK_DDL,
                                                        aSQLBuf,
                                                        aSQLBufSize,
                                                        "alter replication "QCM_SQL_STRING_SKIP_FMT" add table from "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" "
                                                        "to "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                        sAfterRepName[k],
                                                        sUserName,
                                                        sBufferNameStr,
                                                        sUserName,
                                                        sTableName )
                                    != IDE_SUCCESS );
                        }
                    }
                    break;
                case SDI_CLONE_DIST_OBJECT:
                    break;
                case SDI_NON_SHARD_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            /* 'P' procedure: do noting */
        }
    }
    IDE_EXCEPTION_CONT(NORMAL_EXIT);
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_REPLICA_SET );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_REPLICA_SET_INFO ) );
    }
    IDE_EXCEPTION( ERR_KSAFETY );
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_INVALID_KSAFETY ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::truncateAllBackupData( qcStatement * aStatement,
                                    SChar * aSQLBuf,
                                    UInt    aSQLBufSize,
                                    SChar * aNodeName,
                                    qdReShardAttribute * aReShardAttr,
                                    ZKPendingJobType  aPendingJobType )
{
    qdReShardAttribute * sReShardAttr = NULL;
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sTableName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sPartitionName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    sdiShardObjectType   sShardTableType = SDI_NON_SHARD_OBJECT;
    SChar                sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_DASSERT(QC_IS_NULL_NAME(sReShardAttr->mUserName) != ID_TRUE);
        IDE_DASSERT(sReShardAttr->mShardObjectInfo != NULL);
        if(sReShardAttr->mObjectType == 'T')
        {
            QC_STR_COPY(sUserName, sReShardAttr->mUserName);
            QC_STR_COPY(sTableName, sReShardAttr->mObjectName);
            sShardTableType = sdi::getShardObjectType(&sReShardAttr->mShardObjectInfo->mTableInfo );

            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableName );

            switch (sShardTableType)
            {
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    IDE_DASSERT( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE );
                    QC_STR_COPY(sPartitionName, sReShardAttr->mPartitionName);
                    if ( aPendingJobType == ZK_PENDING_JOB_NONE )
                    {
                        /* no pending job, immediately execute to remote */
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" TRUNCATE PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                                    sUserName,
                                                    sBufferNameStr,
                                                    sPartitionName ) != IDE_SUCCESS );
                    }
                    else
                    {
                        idlOS::snprintf(aSQLBuf,
                                        aSQLBufSize,
                                        "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" TRUNCATE PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                        sUserName,
                                        sBufferNameStr,
                                        sPartitionName);
                        
                        sdiZookeeper::addPendingJob(aSQLBuf,
                                                    aNodeName,
                                                    aPendingJobType,
                                                    QCI_STMT_MASK_MAX );
                    }
                    break;
                case SDI_SOLO_DIST_OBJECT:
                case SDI_CLONE_DIST_OBJECT:
                    sShardTableType = sdi::getShardObjectType(&sReShardAttr->mShardObjectInfo->mTableInfo );

                    if ( aPendingJobType == ZK_PENDING_JOB_NONE )
                    {
                        /* no pending job, immediately execute to remote */
                        IDE_TEST( executeRemoteSQL( aStatement,
                                                    aNodeName,
                                                    SDI_INTERNAL_OP_NORMAL,
                                                    QCI_STMT_MASK_DDL,
                                                    aSQLBuf,
                                                    aSQLBufSize,
                                                    "TRUNCATE TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                    sUserName,
                                                    sBufferNameStr ) != IDE_SUCCESS );
                    }
                    else
                    {
                        idlOS::snprintf(aSQLBuf,
                                        aSQLBufSize,
                                        "TRUNCATE TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                        sUserName,
                                        sBufferNameStr);
                        
                        sdiZookeeper::addPendingJob( aSQLBuf,
                                                     aNodeName,
                                                     aPendingJobType,
                                                     QCI_STMT_MASK_MAX );
                    }
                    break;
                case SDI_NON_SHARD_OBJECT:
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }
        else
        {
            /* 'P' procedure: do noting */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::isExistBackupDataForAllReshardAttr( qcStatement * aStatement,
                                                 SChar * aNodeName,
                                                 qdReShardAttribute * aReShardAttr,
                                                 idBool  * aIsExist )
{
    qdReShardAttribute * sReShardAttr = NULL;
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sTableName[QC_MAX_OBJECT_NAME_LEN] = SDI_BACKUP_TABLE_PREFIX;
    SChar                sPartitionName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar              * sPartitionNamePtr = NULL;
    UInt                 sRowCount = 0;
    ULong                sTotalDataCount = 0;

    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_DASSERT(QC_IS_NULL_NAME(sReShardAttr->mUserName) != ID_TRUE);
        IDE_DASSERT(sReShardAttr->mShardObjectInfo != NULL);
        if(sReShardAttr->mObjectType == 'T')
        {
            QC_STR_COPY(sUserName, sReShardAttr->mUserName);
            QC_STR_COPY(sTableName + idlOS::strlen(SDI_BACKUP_TABLE_PREFIX), sReShardAttr->mObjectName);
            if ( QC_IS_NULL_NAME(sReShardAttr->mPartitionName) != ID_TRUE )
            {
                QC_STR_COPY(sPartitionName, sReShardAttr->mPartitionName);
                sPartitionNamePtr = sPartitionName;
            }
            else
            {
                sPartitionNamePtr = NULL;
            }

            IDE_TEST( sdi::getRemoteRowCount( aStatement,
                                              aNodeName,
                                              sUserName,
                                              sTableName,
                                              sPartitionNamePtr,
                                              SDI_INTERNAL_OP_NOT,
                                              &sRowCount ) != IDE_SUCCESS );
            sTotalDataCount += sRowCount;
        }
        else
        {
            /* 'P' procedure: do nothing */
        }
    }

    if ( sTotalDataCount > 0 )
    {
        *aIsExist = ID_TRUE;
    }
    else
    {
        *aIsExist = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::clearBackupDataWithNewTrans( void  * aOriMmSession,
                                          SChar * aNodeName,
                                          qdReShardAttribute * aReShardAttr )
{
    const UInt   sSqlLen = 1024;
    void        *sMmSession = NULL;
    QCD_HSTMT    sHstmt;
    qcStatement *sStatement = NULL;
    SChar        sSqlStr[sSqlLen] = {0,};
    SInt         sStage = 0;
    smiTrans     * sTrans = NULL;
    qciStmtType sStmtType = QCI_STMT_MASK_MAX;
    idBool        sRecordExist;
    smiStatement sSmiStmt;
    UInt sShardDDLLockTryCount = qci::mSessionCallback.mGetShardDDLLockTryCount(aOriMmSession);
    UInt sRemoteLockTimeout = qci::mSessionCallback.mGetShardDDLLockTimeout(aOriMmSession);
    UInt sSelectLockTimeout = (UInt)IDL_MIN((ULong)(sShardDDLLockTryCount * sRemoteLockTimeout), (ULong)ID_UINT_MAX);

    IDE_TEST( qci::mSessionCallback.mAllocInternalSession( &sMmSession, aOriMmSession ) != IDE_SUCCESS );
    IDE_TEST_RAISE( sMmSession == NULL, ERR_INVALID_CONDITION );
    sStage = 1;

    qci::mSessionCallback.mSetShardMetaNumber( sMmSession,
                                               sdi::getSMNForDataNode());

    sTrans     = qci::mSessionCallback.mGetTransWithBegin( sMmSession );
    IDE_ERROR_RAISE( sTrans != NULL, ERR_INVALID_CONDITION );
    IDE_TEST_RAISE( sTrans->isBegin() != ID_TRUE, ERR_INVALID_CONDITION );
    sStage = 2;

    IDE_TEST( qcd::allocStmtNoParent( sMmSession,
                                      ID_TRUE,
                                      & sHstmt )
              != IDE_SUCCESS );
    sStage = 3;

    IDE_TEST( qcd::getQcStmt(sHstmt, &sStatement) != IDE_SUCCESS );

    idlOS::snprintf(sSqlStr, sSqlLen, "ALTER SESSION SET TRANSACTIONAL_DDL = 1");
    IDE_TEST(qcg::runDCLforInternal(sStatement ,
                                    sSqlStr,
                                    sStatement->session->mMmSession ) != IDE_SUCCESS);

    idlOS::snprintf( sSqlStr, sSqlLen,
                     "ALTER SESSION SET QUERY_TIMEOUT = %"ID_INT32_FMT" ", sSelectLockTimeout);

    IDE_TEST( qciMisc::runDCLforInternal( sStatement,
                                          sSqlStr,
                                          sStatement->session->mMmSession ) != IDE_SUCCESS);

    /* the bellow sql do not use. it exists just for plan structure */
    idlOS::snprintf(sSqlStr, sSqlLen, "select 1 from dual");
    IDE_TEST( qcd::prepare( sHstmt,
                            NULL,
                            NULL,
                            & sStmtType,
                            sSqlStr,
                            idlOS::strlen(sSqlStr),
                            ID_TRUE )  // direct-execute mode
              != IDE_SUCCESS );

    IDE_TEST( sSmiStmt.begin( NULL,
                              sTrans->getStatement(),
                              SMI_STATEMENT_NORMAL |
                              SMI_STATEMENT_MEMORY_CURSOR )
              != IDE_SUCCESS );
    sStage = 4;
    qcg::setSmiStmt( sStatement, &sSmiStmt );
    /* execute statement related logic */
    IDE_TEST( sdi::checkShardLinkerWithoutSQL( sStatement,
                                               QCG_GET_SESSION_SHARD_META_NUMBER( sStatement ),
                                               sTrans )
              != IDE_SUCCESS );

    IDE_TEST( qdsd::isExistBackupDataForAllReshardAttr( sStatement,
                                                        aNodeName,
                                                        aReShardAttr,
                                                        &sRecordExist ) != IDE_SUCCESS );

    if ( sRecordExist == ID_TRUE )
    {
        IDE_TEST( qdsd::truncateAllBackupData( sStatement,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            aNodeName,
                                            aReShardAttr,
                                            ZK_PENDING_JOB_NONE) != IDE_SUCCESS );
    }
    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStage = 3;


    sStage = 2;
    IDE_TEST( qcd::freeStmt( sHstmt,
                             ID_TRUE )  // free & drop
              != IDE_SUCCESS );

    sStage = 1;
    IDE_TEST_RAISE ( qci::mSessionCallback.mCommit( sMmSession,
                                                    ID_FALSE /* is stored proc*/ )
                     != IDE_SUCCESS, ERR_COMMIT );

    sStage = 0;
    qci::mSessionCallback.mFreeInternalSession( sMmSession,
                                                ID_TRUE /* aIsSuccess */ );
    return IDE_SUCCESS;
    IDE_EXCEPTION(ERR_INVALID_CONDITION)
    {
        IDE_SET( ideSetErrorCode(qpERR_ABORT_QMC_UNEXPECTED_ERROR,
                                "qdsd::clearBackupDataWithNewTrans",
                                "Invalid condition.") );
    }
    IDE_EXCEPTION( ERR_COMMIT );
    {
        ideLog::log( IDE_SD_1, "[FAILURE] error code 0x%05X %s",
                     E_ERROR_CODE(ideGetErrorCode()),
                     ideGetErrorMsg(ideGetErrorCode()));
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_TRANSACTION_COMMIT_ERROR ) ); 
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch ( sStage )
    {
        case 4:
            IDE_ASSERT(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);
        case 3:
            (void)qcd::freeStmt( sHstmt, ID_TRUE );
                /* fall through */
        case 2:
            (void)qci::mSessionCallback.mRollback( sMmSession, NULL/* savepoint */, ID_FALSE );
                /* fall through */
        case 1:
            (void)qci::mSessionCallback.mFreeInternalSession( sMmSession, ID_FALSE /* aIsSuccess */ );
                /* fall through */
            break;
        default:
                // Nothing to do.
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}


IDE_RC qdsd::executeSQLNodeList( qcStatement          * aStatement,
                                 sdiInternalOperation   aInternalOp,
                                 qciStmtType            aSQLStmtType,
                                 SChar                * aSQLBuf,
                                 SChar                * aExceptNodeName,
                                 iduList              * aList )
{
    iduListNode       * sIterator = NULL;
    sdiClientInfo     * sClientInfo  = NULL;

    IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

    sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

    // ��ü Alive Node ������� ����
    if ( sClientInfo != NULL )
    {
        IDU_LIST_ITERATE( aList, sIterator )
        {
            /* Ư�� Node ���� �ϰ� ���� */
            if ( aExceptNodeName != NULL )
            {
                if ( idlOS::strncmp ( (SChar*)sIterator->mObj, 
                                      aExceptNodeName, 
                                      SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
                {
                    /* Failover ����� TargetNode���� ���� �ʿ䰡 ����.
                     * �̹� Stop ��Ű�� �Ѿ�°Ŷ� Alive �ƴϿ��� ������ Z-Meta S-Meta Lock�� ��� ��ұ� ������
                     * Alive�� ��Ÿ���� ����. -> Ȯ���ʿ�
                     * Stop Ÿ�̹� ������ �ʿ��Ѱ�? */
                    continue;
                }
            }

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        (SChar*) sIterator->mObj,
                                        aInternalOp,
                                        aSQLStmtType,
                                        aSQLBuf,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );
        }
    }
    else
    {
        IDE_RAISE( ERR_NODE_NOT_EXIST );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qdsd::resetShardMeta( qcStatement * aStatement )
{
    
    smiTrans       sTrans;
    smiStatement   sSmiStmt;
    smiStatement * sDummySmiStmt = NULL;
    SChar        * sSqlStr       = NULL;
    vSLong         sRowCnt       = 0;
    UInt           sStage        = 0;
    idBool         sIsSmiStmtEnd = ID_FALSE;
    smiStatement * spRootStmt    = QC_SMI_STMT(aStatement)->getTrans()->getStatement();
    UInt           sSmiStmtFlag  = QC_SMI_STMT(aStatement)->mFlag;

    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                      SChar,
                                      QD_MAX_SQL_LENGTH,
                                      &sSqlStr )
              != IDE_SUCCESS );

    IDE_TEST( sTrans.initialize() != IDE_SUCCESS );
    sStage = 1;

    IDE_TEST( sTrans.begin( &sDummySmiStmt,
                            NULL,
                            ( SMI_ISOLATION_NO_PHANTOM     |
                              SMI_TRANSACTION_NORMAL       |
                              SMI_TRANSACTION_REPL_DEFAULT |
                              SMI_COMMIT_WRITE_NOWAIT ) )
              != IDE_SUCCESS );
    sStage = 2;

    IDE_TEST( sSmiStmt.begin( NULL,
                              sDummySmiStmt,
                              ( SMI_STATEMENT_NORMAL |
                                SMI_STATEMENT_MEMORY_CURSOR ) )
              != IDE_SUCCESS );
    sStage = 3;
    
    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.NODES_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.OBJECTS_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.RANGES_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.CLONES_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.SOLOS_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.REPLICA_SETS_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "DELETE FROM SYS_SHARD.FAILOVER_HISTORY_ " );

    IDE_TEST( qciMisc::runDMLforDDL( &sSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    // reset shard sequence
    IDE_TEST( resetShardSequence( &sSmiStmt,
                                  (SChar*)"NEXT_NODE_ID" )
              != IDE_SUCCESS );

    IDE_TEST( resetShardSequence( &sSmiStmt,
                                  (SChar*)"NEXT_SHARD_ID" )
              != IDE_SUCCESS );
    
    IDE_TEST( sSmiStmt.end( SMI_STATEMENT_RESULT_SUCCESS )
              != IDE_SUCCESS );
    sStage = 2;

    IDE_TEST( sTrans.commit() != IDE_SUCCESS );
    sStage = 1;

    sStage = 0;
    IDE_TEST( sTrans.destroy( NULL ) != IDE_SUCCESS );

    // commit�� ���� �ش� ������ ���� ����
    // statement end, begin ����
    IDE_TEST( QC_SMI_STMT(aStatement)->end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS );
    sIsSmiStmtEnd = ID_TRUE;

    IDE_TEST( QC_SMI_STMT(aStatement)->begin( aStatement->mStatistics,
                                              spRootStmt,
                                              sSmiStmtFlag )
              != IDE_SUCCESS );
    sIsSmiStmtEnd = ID_FALSE;    

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sStage )
    {
        case 3:
            ( void )sSmiStmt.end( SMI_STATEMENT_RESULT_FAILURE );
        case 2:
            ( void )sTrans.rollback();
        case 1:
            ( void )sTrans.destroy( NULL );
        default:
            break;
    }
    
    if( sIsSmiStmtEnd == ID_TRUE )
    {
        IDE_ASSERT( QC_SMI_STMT(aStatement)->begin( aStatement->mStatistics,
                                                    spRootStmt,
                                                    SMI_STATEMENT_NORMAL|SMI_STATEMENT_ALL_CURSOR ) 
                    == IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

IDE_RC qdsd::resetShardSequence( smiStatement * aSmiStmt,
                                 SChar        * aSeqName )
{
    qcmSequenceInfo   sSequenceInfo;
    void            * sSequenceHandle;
    SChar             sSqlStr[QD_MAX_SQL_LENGTH + 1];
    UInt              sUserID;
    vSLong            sRowCnt;
    SChar             sQueueSequenceName[QC_MAX_SEQ_NAME_LEN + 1];
    
    // zookeeper shard meta lock�� ���� �����̹Ƿ� ������ lock�� ���� �ʴ´�.
    idlOS::snprintf( sQueueSequenceName,
                     ID_SIZEOF( sQueueSequenceName ),
                     "%s",
                     aSeqName );

    IDE_TEST_RAISE( qcmUser::getUserID( NULL,
                                        aSmiStmt,
                                        SDM_USER,
                                        idlOS::strlen(SDM_USER),
                                        & sUserID )
                    != IDE_SUCCESS, ERR_NOT_EXIST_USER );

    // Queue Sequence
    IDE_TEST( qcm::getSequenceInfoByName( aSmiStmt,
                                          sUserID,
                                          (UChar*) sQueueSequenceName,
                                          idlOS::strlen( sQueueSequenceName ),
                                          &sSequenceInfo,
                                          &sSequenceHandle )
              != IDE_SUCCESS );

    // sequence reset
    IDE_TEST( smiTable::resetSequence( aSmiStmt,
                                       sSequenceHandle )
              != IDE_SUCCESS );
    
    // tables_ �� sequence udpate
    idlOS::snprintf( sSqlStr,
                     QD_MAX_SQL_LENGTH,
                     "UPDATE SYS_TABLES_ "
                     "SET USER_ID = INTEGER'%"ID_INT32_FMT"', "
                     "TABLE_NAME = '%s', "
                     "TABLE_OID = BIGINT'%"ID_INT64_FMT"', "
                     "COLUMN_COUNT = INTEGER'%"ID_INT32_FMT"', "
                     "PARALLEL_DEGREE = INTEGER'%"ID_INT32_FMT"', "
                     "LAST_DDL_TIME = SYSDATE "
                     "WHERE TABLE_ID = INTEGER'%"ID_INT32_FMT"'",
                     sUserID,
                     aSeqName,
                     (ULong)smiGetTableId( sSequenceHandle ),
                     0,
                     1,
                     sSequenceInfo.sequenceID );

    IDE_TEST( qciMisc::runDMLforDDL( aSmiStmt,
                                     sSqlStr,
                                     &sRowCnt )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sRowCnt == 0, ERR_META_CRASH );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_USER )
    {
        IDE_SET( ideSetErrorCode(sdERR_ABORT_SDM_SHARD_META_NOT_CREATED,
                                 SDM_USER) );
    }
    IDE_EXCEPTION( ERR_META_CRASH )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_META_CRASH ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::lockTables4Failover( qcStatement        * aStatement,
                                  SChar              * aSQLBuf,
                                  UInt                 aSQLBufSize,
                                  SChar              * aFailoverNodeName,
                                  sdiTableInfoList   * aTableInfoList,
                                  sdiReplicaSetInfo  * aReplicaSetInfo,
                                  sdiNode            * aTargetNodeInfo,
                                  ULong                aSMN )
{
    UInt               sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt               sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);
    UInt               sLoopCnt = 0;

    idBool             sIsCancel = ID_FALSE;
    idBool             sIsLockAcquired = ID_FALSE;
    idBool             sIsLocked = ID_FALSE;

    sdiTableInfoList * sTmpTableInfoList = NULL;
    sdiTableInfo     * sTableInfo = NULL;

    sdiRangeInfo       sRangeInfos;

    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;
    
    UInt               j;
    SChar              sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    /* the reomte cancel is not running. therefore remote lock timeout property sec(shard_ddl_lock_timeout).
     * if a application lock order is A->B->C then executing reshard table should be in the order A, B and C 
     * if A table is locked then the next table lock may will be acquired.
     */
    sLoopCnt = 0;
    while ( (sLoopCnt++ < sShardDDLLockTryCount) && (sIsLockAcquired != ID_TRUE ) )
    {
        sIsCancel = ID_FALSE;
        sIsLockAcquired = ID_FALSE;

        IDE_TEST( executeRemoteSQL( aStatement, 
                                    aFailoverNodeName,
                                    SDI_INTERNAL_OP_FAILOVER,
                                    QCI_STMT_MASK_SP,
                                    aSQLBuf,
                                    aSQLBufSize,
                                    "SAVEPOINT __FAILOVER_SVP0") != IDE_SUCCESS);

        for ( sTmpTableInfoList = aTableInfoList;
              sTmpTableInfoList != NULL;
              sTmpTableInfoList = sTmpTableInfoList->mNext )
        {
            sIsLocked = ID_FALSE;
            sTableInfo = sTmpTableInfoList->mTableInfo;

            if ( sTableInfo->mObjectType == 'T' )
            {
                idlOS::snprintf( sBufferNameStr,
                                 QC_MAX_OBJECT_NAME_LEN + 1,
                                 "%s%s",
                                 SDI_BACKUP_TABLE_PREFIX,
                                 sTableInfo->mObjectName );

                /* Default Partition check */
                if ( sTableInfo->mDefaultNodeId == aTargetNodeInfo->mNodeId )
                {
                    if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                      sTableInfo->mDefaultPartitionReplicaSetId,
                                                      aFailoverNodeName ) == ID_TRUE )
                    {
                        /* Swap ����̶�� Lock�� ȹ���ؾ� �Ѵ�. */
                        /* Default Partition Lock ȹ�� */
                        if( executeRemoteSQL( aStatement, 
                                              aFailoverNodeName,
                                              SDI_INTERNAL_OP_FAILOVER,
                                              QCI_STMT_MASK_DML,
                                              aSQLBuf,
                                              aSQLBufSize,
                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,

//                                              "LOCK TABLE %s.%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                              sTableInfo->mUserName,
                                              sTableInfo->mObjectName,
//                                              sTableInfo->mDefaultPartitionName,
                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                        {
                            /* cancel retry */
                            sIsCancel = ID_TRUE;
                            break;
                        }
                        /* _BAK_Table Default Partition Lock ȹ�� */
                        if( executeRemoteSQL( aStatement, 
                                              aFailoverNodeName,
                                              SDI_INTERNAL_OP_FAILOVER,
                                              QCI_STMT_MASK_DML,
                                              aSQLBuf,
                                              aSQLBufSize,
                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
//                                              "LOCK TABLE %s._BAK_%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                              sTableInfo->mUserName,
                                              sBufferNameStr,
//                                              sTableInfo->mDefaultPartitionName,
                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                        {
                            /* cancel retry */
                            sIsCancel = ID_TRUE;
                            break;
                        }

                        /* Table lock ȹ�� ����, ���� Table�� Ȯ���Ѵ�. */
                        continue;
                    }
                    else
                    {
                        /* Swap ����� �ƴϸ� Lock ���� �ʿ䰡 ����. */
                    }
                }

                /* Range Info Check */
                IDE_TEST( sdm::getRangeInfo( aStatement,
                                             QC_SMI_STMT( aStatement ),
                                             aSMN,
                                             sTableInfo,
                                             &sRangeInfos,
                                             ID_FALSE )
                          != IDE_SUCCESS );

                /* check Table Type */
                sShardTableType = sdi::getShardObjectType( sTableInfo );

                for ( j = 0; j < sRangeInfos.mCount; j++ )
                {

                    if ( sRangeInfos.mRanges[j].mNodeId == aTargetNodeInfo->mNodeId )
                    {
                        if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                          sRangeInfos.mRanges[j].mReplicaSetId,
                                                          aFailoverNodeName ) == ID_TRUE )
                        {
                            switch ( sShardTableType )
                            {
                                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                    /* Partition Swap ����̴�. Partition lock ȹ���Ѵ�. */
                                    /* Partition Lock ȹ�� */
                                    if( executeRemoteSQL( aStatement, 
                                                          aFailoverNodeName,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_DML,
                                                          aSQLBuf,
                                                          aSQLBufSize,
                                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
//                                                          "LOCK TABLE %s.%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                          sTableInfo->mUserName,
                                                          sTableInfo->mObjectName,
//                                                          sRangeInfos.mRanges[j].mPartitionName,
                                                          sRemoteLockTimeout ) != IDE_SUCCESS )
                                    {
                                        /* cancel retry */
                                        sIsCancel = ID_TRUE;
                                        break;
                                    }
                                    /* _BAK_ Table Partition Lock ȹ�� */
                                    if( executeRemoteSQL( aStatement, 
                                                          aFailoverNodeName,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_DML,
                                                          aSQLBuf,
                                                          aSQLBufSize,
                                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
//                                                          "LOCK TABLE %s._BAK_%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                          sTableInfo->mUserName,
                                                          sBufferNameStr,
//                                                          sRangeInfos.mRanges[j].mPartitionName,
                                                          sRemoteLockTimeout ) != IDE_SUCCESS )
                                    {
                                        /* cancel retry */
                                        sIsCancel = ID_TRUE;
                                        break;
                                    }
                                    sIsLocked = ID_TRUE;
                                    break;
                                case SDI_SOLO_DIST_OBJECT:
                                    /* Table Swap ����̴�. Table lock ȹ���Ѵ�. */
                                    /* Table Lock ȹ�� */
                                    if( executeRemoteSQL( aStatement, 
                                                          aFailoverNodeName,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_DML,
                                                          aSQLBuf,
                                                          aSQLBufSize,
                                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                          sTableInfo->mUserName,
                                                          sTableInfo->mObjectName,
                                                          sRemoteLockTimeout ) != IDE_SUCCESS )
                                    {
                                        /* cancel retry */
                                        sIsCancel = ID_TRUE;
                                        break;
                                    }
                                    /* _BAK_ Table Lock ȹ�� */
                                    if( executeRemoteSQL( aStatement, 
                                                          aFailoverNodeName,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_DML,
                                                          aSQLBuf,
                                                          aSQLBufSize,
                                                          "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                          sTableInfo->mUserName,
                                                          sBufferNameStr,
                                                          sRemoteLockTimeout ) != IDE_SUCCESS )
                                    {
                                        /* cancel retry */
                                        sIsCancel = ID_TRUE;
                                        break;
                                    }
                                    sIsLocked = ID_TRUE;
                                    break;
                                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                    /* Composite�� HA�� �������� �ʱ� ������ Partition Swap ����.
                                     * Lock ���� �ʿ� ����. */
                                    continue;
                                case SDI_CLONE_DIST_OBJECT:
                                    /* Clone �� �� Node�� Primary�� �����ϱ� ������ Partition Swap �� �ʿ� ����.
                                     * Lock ���� �ʿ� ����. */
                                    continue;
                                case SDI_NON_SHARD_OBJECT:
                                default:
                                    /* Non Shard Table�� ���� �ȵ�. */
                                    IDE_DASSERT( 0 );
                                    break;
                            }

                        }
                        else
                        {
                            /* Swap ����� �ƴϸ� Lock ���� �ʿ䰡 ����. */
                        }
                    }

                    /* Table Lock�� ������� ���� Table Lock�� ȹ���Ϸ� ����.
                     * Range Info Ž�� �߿� Lock ȹ�濡 ���������� Retry �Ұ��̴�. */
                    if ( ( sIsCancel == ID_TRUE ) || ( sIsLocked == ID_TRUE ) )
                    {
                        break;
                    }
                }

                if ( sIsCancel == ID_TRUE )
                {
                    /* RangeInfo Ž���߿� Lock ȹ�濡 �����ߴ�. */
                    break;
                }
            }
            else
            {
                /* R2HA PROCEDURE LOCK NEED */
            }
        }
        if ( sIsCancel == ID_TRUE )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aFailoverNodeName,
                                        SDI_INTERNAL_OP_FAILOVER,
                                        QCI_STMT_MASK_SP,
                                        aSQLBuf,
                                        aSQLBufSize,
                                        "ROLLBACK TO SAVEPOINT __FAILOVER_SVP0") != IDE_SUCCESS );
            sIsLockAcquired = ID_FALSE;
        }
        else
        {
            sIsLockAcquired = ID_TRUE;
            break;
        }
    }

    IDE_TEST_RAISE( sIsLockAcquired != ID_TRUE, ERR_FAILOVERLOCK );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FAILOVERLOCK )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_LOCK_FAIL ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    (void)executeRemoteSQL( aStatement, 
                            aFailoverNodeName,
                            SDI_INTERNAL_OP_FAILOVER,
                            QCI_STMT_MASK_SP,
                            aSQLBuf,
                            aSQLBufSize,
                            "ROLLBACK TO SAVEPOINT __FAILOVER_SVP0" );
    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC qdsd::executeRemoteSQLWithNewTrans( qcStatement * aStatement,
                                           SChar       * aNodeName,
                                           SChar       * aSqlStr,
                                           idBool        aIsDDLTimeOut )
{
    sdiLocalMetaInfo   sLocalMetaInfo;
    SChar              sSqlStr[QD_MAX_SQL_LENGTH + 1];

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    if ( aIsDDLTimeOut == ID_TRUE )
    {
        IDE_TEST( sdi::shardExecTempDDLWithNewTrans( aStatement,
                                                     aNodeName,
                                                     aSqlStr,
                                                     idlOS::strlen(aSqlStr) )
                  != IDE_SUCCESS );
    }
    else
    {
        if ( aNodeName != NULL )
        {
            if ( idlOS::strncmp( sLocalMetaInfo.mNodeName, 
                                 aNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {            
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH, "%s ", aSqlStr );
            }
            else
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "EXEC DBMS_SHARD.EXECUTE_IMMEDIATE( '%s', '"QCM_SQL_STRING_SKIP_FMT"' ) ",
                                 aSqlStr,
                                 aNodeName );
            }
        }
        else
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "EXEC DBMS_SHARD.EXECUTE_IMMEDIATE( '%s', NULL ) ",
                             aSqlStr );
        }

        IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                          sSqlStr )
                  != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC qdsd::changeBuckupNodeNameForFailOver( qcStatement       * aStatement,
                                              sdiReplicaSetInfo * aReplicaSetInfo,
                                              UInt                aNth)
{
    sdiLocalMetaInfo sLocalMetaInfo;
    SChar            sSqlStr[QD_MAX_SQL_LENGTH + 1];

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    
    if ( aNth == 0 ) /* First */
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_SHARD.REPLICA_SETS_  "
                         "   SET FIRST_BACKUP_NODE_NAME = '%s', "
                         "       STOP_FIRST_BACKUP_NODE_NAME = '%s' "                         
                         "   WHERE PRIMARY_NODE_NAME    = '%s' "
                         "         AND SMN              = %"ID_INT32_FMT,
                         SDM_NA_STR,
                         aReplicaSetInfo->mReplicaSets[0].mFirstBackupNodeName,                         
                         aReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                         aReplicaSetInfo->mReplicaSets[0].mSMN );

        IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                                sLocalMetaInfo.mNodeName,
                                                sSqlStr,
                                                ID_FALSE )
                  != IDE_SUCCESS );

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_SHARD.REPLICA_SETS_  "
                         "   SET FIRST_BACKUP_NODE_NAME = ''%s'', "
                         "       STOP_FIRST_BACKUP_NODE_NAME = ''%s'' "                         
                         "   WHERE PRIMARY_NODE_NAME    = ''%s'' "
                         "         AND SMN              = %"ID_INT32_FMT,
                         SDM_NA_STR,
                         aReplicaSetInfo->mReplicaSets[0].mFirstBackupNodeName,                         
                         aReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                         aReplicaSetInfo->mReplicaSets[0].mSMN );

        IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                                NULL,
                                                sSqlStr,
                                                ID_FALSE )
                  != IDE_SUCCESS );
        
    }
    else if ( aNth == 1 ) /* Second */
    {
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_SHARD.REPLICA_SETS_ "
                         "   SET SECOND_BACKUP_NODE_NAME = '%s', "
                         "       STOP_SECOND_BACKUP_NODE_NAME = '%s' "                         
                         "   WHERE PRIMARY_NODE_NAME     = '%s' "
                         "         AND SMN               = %"ID_INT32_FMT,
                         SDM_NA_STR,
                         aReplicaSetInfo->mReplicaSets[0].mSecondBackupNodeName,                         
                         aReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                         aReplicaSetInfo->mReplicaSets[0].mSMN );

        IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                                sLocalMetaInfo.mNodeName,
                                                sSqlStr,
                                                ID_FALSE )
                  != IDE_SUCCESS );
        
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "UPDATE SYS_SHARD.REPLICA_SETS_ "
                         "   SET SECOND_BACKUP_NODE_NAME = ''%s'', "
                         "       STOP_SECOND_BACKUP_NODE_NAME = ''%s'' "
                         "   WHERE PRIMARY_NODE_NAME     =  ''%s'' "
                         "         AND SMN               = %"ID_INT32_FMT,
                         SDM_NA_STR,
                         aReplicaSetInfo->mReplicaSets[0].mSecondBackupNodeName,                         
                         aReplicaSetInfo->mReplicaSets[0].mPrimaryNodeName,
                         aReplicaSetInfo->mReplicaSets[0].mSMN );

        IDE_TEST( executeRemoteSQLWithNewTrans( aStatement,
                                                NULL,
                                                sSqlStr,
                                                ID_FALSE )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qdsd::flushReplicationForInternal( qcStatement * aStatement,
                                          SChar       * aNodeName,
                                          SChar       * aReplName )
{
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    UInt                sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt                sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);

    sdiInternalOperation sInternalOP = SDI_INTERNAL_OP_NORMAL;

    if( qci::mSessionCallback.mGetShardInternalLocalOperation( aStatement->session->mMmSession ) 
        == SDI_INTERNAL_OP_FAILBACK )
    {
        sInternalOP = SDI_INTERNAL_OP_FAILBACK;
    }

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

    if ( aStatement->session->mQPSpecific.mClientInfo != NULL )
    {
        if ( idlOS::strncmp( sLocalMetaInfo.mNodeName, 
                             aNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            IDE_TEST( executeLocalSQL( aStatement, 
                                       QCI_STMT_MASK_DCL,
                                       sSqlStr,
                                       QD_MAX_SQL_LENGTH,
                                       "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                       aReplName,
                                       (sShardDDLLockTryCount * sRemoteLockTimeout) )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aNodeName,
                                        sInternalOP,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                        aReplName,
                                        (sShardDDLLockTryCount * sRemoteLockTimeout) )
                      != IDE_SUCCESS);
        }
            
        ideLog::log(IDE_SD_17,"[SHARD INTERNAL SQL]: %s, %s",aNodeName, sSqlStr);
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeReplicationForJoin( qcStatement * aStatement,
                                        idBool        aIsException )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiReplicaSetInfo   sReplicaSetInfo;
    sdiReplicaSetInfo   sCurrReplicaSetInfo;
    sdiReplicaSetInfo   sPrevReplicaSetInfo;
    sdiReplicaSetInfo   sPrevPrevReplicaSetInfo;
    sdiGlobalMetaInfo   sMetaNodeInfo = { ID_ULONG(0) };
    ULong               sSMN = ID_ULONG(0);
    UInt                i = 0;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    IDE_TEST( sdi::getGlobalMetaInfoCore( QC_SMI_STMT( aStatement ),
                                          &sMetaNodeInfo )
              != IDE_SUCCESS );
    
    sSMN = sMetaNodeInfo.mShardMetaNumber;
    
    IDE_TEST( sdi::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sSMN )
              != IDE_SUCCESS );

    IDE_TEST( findBackupReplicaSetInfo( sLocalMetaInfo.mNodeName,
                                        &sReplicaSetInfo,
                                        &sCurrReplicaSetInfo,
                                        &sPrevReplicaSetInfo,
                                        &sPrevPrevReplicaSetInfo )
              != IDE_SUCCESS );

    for ( i = 0; i < sLocalMetaInfo.mKSafety; i++ )
    {
        if ( i == 0 )
        {
            if ( aIsException == ID_TRUE )
            {
                IDE_TEST( stopReplicationForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                              sCurrReplicaSetInfo.mReplicaSets[0].mFirstReplName )
                          != IDE_SUCCESS );            
            }
            else
            {
                // current node replication start
                IDE_TEST( startReplicationForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                              sCurrReplicaSetInfo.mReplicaSets[0].mFirstReplName )
                          != IDE_SUCCESS );
            
                // current node replication flush
                IDE_TEST( flushReplicationForInternal(
                              aStatement,
                              sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                              sCurrReplicaSetInfo.mReplicaSets[0].mFirstReplName )
                          != IDE_SUCCESS );


                // prev node replication flush
                IDE_TEST( flushReplicationForInternal(
                              aStatement,
                              sPrevReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                              sPrevReplicaSetInfo.mReplicaSets[0].mFirstReplName )
                          != IDE_SUCCESS );
            }
        }
        else
        {
            // ���2�� ��� second backup�� �������� �ʴ´�.
            if ( sParseTree->mNodeCount > 2 )
            {
                if ( aIsException == ID_TRUE )
                {
                    IDE_TEST( stopReplicationForInternal(
                                  aStatement,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mSecondReplName )
                              != IDE_SUCCESS );
                }
                else
                {    
                    // current node replication start
                    IDE_TEST( startReplicationForInternal(
                                  aStatement,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mSecondReplName )
                              != IDE_SUCCESS );
            
                    // current node replication flush
                    IDE_TEST( flushReplicationForInternal(
                                  aStatement,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                  sCurrReplicaSetInfo.mReplicaSets[0].mSecondReplName )
                              != IDE_SUCCESS );

                    // prev prev node replication flush
                    IDE_TEST( flushReplicationForInternal(
                                  aStatement,
                                  sPrevPrevReplicaSetInfo.mReplicaSets[0].mPrimaryNodeName,
                                  sPrevPrevReplicaSetInfo.mReplicaSets[0].mSecondReplName )
                              != IDE_SUCCESS );
                }
            }
        }
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::checkZookeeperSMNAndDataSMN()
{
    SInt  sDataLength = 0;
    SChar sBuffer[SDI_ZKC_BUFFER_SIZE];
    ULong sZookeeperSMN = SDI_NULL_SMN;
    ULong sMyNodeDataSMN = SDI_NULL_SMN;
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    
    IDE_TEST( sdiZookeeper::readInfo( (SChar*)SDI_ZKC_PATH_SMN,
                                      sBuffer,
                                      &sDataLength ) != IDE_SUCCESS );
    sZookeeperSMN = (ULong)idlOS::strtoul( sBuffer, NULL , 10);
    sMyNodeDataSMN = sdi::getSMNForDataNode();
    IDE_TEST_RAISE( sZookeeperSMN != sMyNodeDataSMN,
                    ERR_DIFFERENT_SMN );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_DIFFERENT_SMN )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QDSD_SHARD_NOT_SAME_CLUSTER_META_DATA_SMN, sZookeeperSMN, sMyNodeDataSMN ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failoverTablePartitionSwap( qcStatement        * aStatement,
                                         SChar              * aFailoverNodeName,
                                         sdiTableInfoList   * aTableInfoList,
                                         sdiReplicaSetInfo  * aReplicaSetInfo,
                                         sdiNode            * aTargetNodeInfo,
                                         ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;

    sdiShardObjectType   sShardTableType = SDI_NON_SHARD_OBJECT;
    
    UInt                j;

    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    SChar               sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* Procedure�� Swap�� Partition�� ����. */
        if ( sTableInfo->mObjectType == 'T' )
        {
            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableInfo->mObjectName );

            /* Default Partition check */
            if ( sTableInfo->mDefaultNodeId == aTargetNodeInfo->mNodeId )
            {
                if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                  sTableInfo->mDefaultPartitionReplicaSetId,
                                                  aFailoverNodeName ) == ID_TRUE )
                {
                    /* Partition Swap Send to TargetNode */
                    idlOS::snprintf( sSqlStr, 
                                     QD_MAX_SQL_LENGTH,
                                     "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                     sTableInfo->mUserName,
                                     sBufferNameStr,
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     sTableInfo->mDefaultPartitionName );

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                aFailoverNodeName,
                                                SDI_INTERNAL_OP_FAILOVER,
                                                QCI_STMT_MASK_DDL,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                                != IDE_SUCCESS );
                }
                else
                {
                    /* Failover �Ҽ� ���ٸ� Swap ���� �ʴ´�. */
                }
            }

            /* Range Info Check */
            IDE_TEST( sdm::getRangeInfo( aStatement,
                                         QC_SMI_STMT( aStatement ),
                                         aSMN,
                                         sTableInfo,
                                         &sRangeInfos,
                                         ID_FALSE )
                      != IDE_SUCCESS );

            /* check Table Type */
            sShardTableType = sdi::getShardObjectType( sTableInfo );

            for ( j = 0; j < sRangeInfos.mCount; j++ )
            {

                if ( sRangeInfos.mRanges[j].mNodeId == aTargetNodeInfo->mNodeId )
                {
                    if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                      sRangeInfos.mRanges[j].mReplicaSetId,
                                                      aFailoverNodeName ) == ID_TRUE )
                    {
                        switch ( sShardTableType )
                        {
                            case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                /* Partition Swap */
                                idlOS::snprintf( sSqlStr, 
                                                 QD_MAX_SQL_LENGTH,
                                                 "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                                 sTableInfo->mUserName,
                                                 sBufferNameStr,
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 sRangeInfos.mRanges[j].mPartitionName );
                                break;
                            case SDI_SOLO_DIST_OBJECT:
                                /* Partition Swap */
                                idlOS::snprintf( sSqlStr, 
                                                 QD_MAX_SQL_LENGTH,
                                                 "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                 sTableInfo->mUserName,
                                                 sBufferNameStr,
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName );

                                break;
                            case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                /* Composite�� HA�� �������� �ʱ� ������ Partition Swap �� �ʿ� ����. */
                                continue;
                            case SDI_CLONE_DIST_OBJECT:
                                /* Clone �� �� Node�� Primary�� �����ϱ� ������ Partition Swap �� �ʿ� ����. */
                                continue;
                            case SDI_NON_SHARD_OBJECT:
                            default:
                                /* Non Shard Table�� ���� �ȵ�. */
                                IDE_DASSERT( 0 );
                                break;
                        }
                        IDE_TEST( executeRemoteSQL( aStatement, 
                                                    aFailoverNodeName,
                                                    SDI_INTERNAL_OP_FAILOVER,
                                                    QCI_STMT_MASK_DDL,
                                                    sSqlStr,
                                                    QD_MAX_SQL_LENGTH,
                                                    NULL )
                                    != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Failover �Ҽ� ���ٸ� Swap ���� �ʴ´�. */
                    }
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failoverResetShardMeta( qcStatement        * aStatement,
                                     SChar              * aTargetNodeName,
                                     SChar              * aFailoverNodeName,
                                     sdiTableInfoList   * aTableInfoList,
                                     sdiReplicaSetInfo  * aReplicaSetInfo,
                                     sdiNode            * aTargetNodeInfo,
                                     iduList            * aAliveNodeList,
                                     ULong                aSMN,
                                     idBool             * aIsUnsetNode )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;

    sdiShardObjectType   sShardTableType = SDI_NON_SHARD_OBJECT;
    
    UInt                j;

    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    idBool              sIsUnsetNode = ID_TRUE;

    sdiValue            sValueStr;

    SChar               sObjectValue[SDI_RANGE_VARCHAR_MAX_PRECISION + 1];

    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* Default Partition check */
        if ( sTableInfo->mDefaultNodeId == aTargetNodeInfo->mNodeId )
        {
            if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                              sTableInfo->mDefaultPartitionReplicaSetId,
                                              aFailoverNodeName ) == ID_TRUE )
            {
                /* Send resetShardPartitioinNode to All Alive Node */
                if ( sTableInfo->mObjectType == 'T' ) /* Table */
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "exec dbms_shard.reset_shard_partition_node( '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', "
                                     QCM_SQL_VARCHAR_FMT" )",
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     aTargetNodeName,
                                     aFailoverNodeName,
                                     sTableInfo->mDefaultPartitionName );
                }
                else /* Procedure */
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "exec dbms_shard.reset_shard_resident_node( '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', "
                                     QCM_SQL_VARCHAR_FMT", "
                                     QCM_SQL_VARCHAR_FMT", "
                                     QCM_SQL_UINT32_FMT" )",
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     aTargetNodeName,
                                     aFailoverNodeName,
                                     "",
                                     "",
                                     1 );
                }

                IDE_TEST( executeSQLNodeList( aStatement,
                                              SDI_INTERNAL_OP_FAILOVER,
                                              QCI_STMT_MASK_SP,
                                              sSqlStr,
                                              aTargetNodeName,
                                              aAliveNodeList ) != IDE_SUCCESS );

            }
            else
            {
                /* Failover �Ҽ� ���ٸ� Shard Meta �� �׳� �д�. */
                /* Shard Meta�� �����ִٸ� UnsetNode ���� ����� �ƴϴ�. */
                sIsUnsetNode = ID_FALSE;
            }
        }

        /* Range Info Check */
        IDE_TEST( sdm::getRangeInfo( aStatement,
                                     QC_SMI_STMT( aStatement ),
                                     aSMN,
                                     sTableInfo,
                                     &sRangeInfos,
                                     ID_FALSE )
                  != IDE_SUCCESS );

        /* check Table Type */
        sShardTableType = sdi::getShardObjectType( sTableInfo );

        for ( j = 0; j < sRangeInfos.mCount; j++ )
        {

            if ( sRangeInfos.mRanges[j].mNodeId == aTargetNodeInfo->mNodeId )
            {
                if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                  sRangeInfos.mRanges[j].mReplicaSetId,
                                                  aFailoverNodeName ) == ID_TRUE )
                {
                    switch ( sShardTableType )
                    {
                        case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                        case SDI_SOLO_DIST_OBJECT:
                            /* send resetShardPartitionNode to All Alive Node */
                            if ( sTableInfo->mObjectType == 'T' ) /* Table */
                            {
                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_partition_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 aFailoverNodeName,
                                                 sRangeInfos.mRanges[j].mPartitionName );
                            }
                            else /* Procedure */
                            {
                                if ( sTableInfo->mSplitMethod == SDI_SPLIT_HASH )
                                {
                                    IDE_TEST( sdi::getValueStr( MTD_INTEGER_ID,
                                                                &sRangeInfos.mRanges[j].mValue,
                                                                &sValueStr )
                                              != IDE_SUCCESS );
                                }
                                else
                                {
                                    IDE_TEST( sdi::getValueStr( sTableInfo->mKeyDataType,
                                                                &sRangeInfos.mRanges[j].mValue,
                                                                &sValueStr )
                                              != IDE_SUCCESS );
                                }

                                if ( sValueStr.mCharMax.value[0] == '\'' )
                                {
                                    // INPUT ARG ('''A''') => 'A' => '''A'''
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "''%s''",
                                                     sValueStr.mCharMax.value );
                                }
                                else
                                {
                                    // INPUT ARG ('A') => A => 'A'
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "'%s'",
                                                     sValueStr.mCharMax.value );
                                }

                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_resident_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', "
                                                 "%s, "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 aFailoverNodeName,
                                                 sObjectValue,
                                                 "" );
                            }

                            IDE_TEST( executeSQLNodeList( aStatement,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_SP,
                                                          sSqlStr,
                                                          aTargetNodeName,
                                                          aAliveNodeList ) != IDE_SUCCESS );
                            break;
                        case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                            /* Composite�� HA�� �������� �ʱ� ������ Failover ����� �ƴϴ�. */
                            continue;
                        case SDI_CLONE_DIST_OBJECT:
                            /* Clones_ ���� ���� �� �־�� �Ѵ�. */
                            if ( sTableInfo->mObjectType == 'T' ) /* Table */
                            {
                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_partition_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '', "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 sRangeInfos.mRanges[j].mPartitionName );
                            }
                            else /* Procedure */
                            {
                                IDE_TEST( sdi::getValueStr( sTableInfo->mKeyDataType,
                                                            &sRangeInfos.mRanges[j].mValue,
                                                            &sValueStr )
                                          != IDE_SUCCESS );

                                if ( sValueStr.mCharMax.value[0] == '\'' )
                                {
                                    // INPUT ARG ('''A''') => 'A' => '''A'''
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "''%s''",
                                                     sValueStr.mCharMax.value );
                                }
                                else
                                {
                                    // INPUT ARG ('A') => A => 'A'
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "'%s'",
                                                     sValueStr.mCharMax.value );
                                }

                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_resident_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '', "
                                                 "%s, "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 sObjectValue,
                                                 "" );
                            }

                            IDE_TEST( executeSQLNodeList( aStatement,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_SP,
                                                          sSqlStr,
                                                          aTargetNodeName,
                                                          aAliveNodeList ) != IDE_SUCCESS );
                            continue;
                        case SDI_NON_SHARD_OBJECT:
                        default:
                            /* Non Shard Table�� ���� �ȵ�. */
                            IDE_DASSERT( 0 );
                            break;
                    }
                }
                else
                {
                    /* Failover �Ҽ� ���ٸ� CLONE Table �� Shard Meta ���� �����Ѵ�.
                     * reset_shard_partition_node�� NewNodeName�� null �̸� �����̴�. */
                    switch ( sShardTableType )
                    {
                        case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                        case SDI_SOLO_DIST_OBJECT:
                            /* Shard Meta�� �����ִٸ� UnsetNode ���� ����� �ƴϴ�. */
                            sIsUnsetNode = ID_FALSE;
                            continue;
                        case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                            /* Composite�� HA�� �������� �ʱ� ������ Failover ����� �ƴϴ�. */
                            continue;
                        case SDI_CLONE_DIST_OBJECT:
                            /* Clones_ ���� ���� �� �־�� �Ѵ�. */
                            if ( sTableInfo->mObjectType == 'T' ) /* Table */
                            {
                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_partition_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '', "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 sRangeInfos.mRanges[j].mPartitionName );
                            }
                            else /* Procedure */
                            {
                                IDE_TEST( sdi::getValueStr( sTableInfo->mKeyDataType,
                                                            &sRangeInfos.mRanges[j].mValue,
                                                            &sValueStr )
                                          != IDE_SUCCESS );

                                if ( sValueStr.mCharMax.value[0] == '\'' )
                                {
                                    // INPUT ARG ('''A''') => 'A' => '''A'''
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "''%s''",
                                                     sValueStr.mCharMax.value );
                                }
                                else
                                {
                                    // INPUT ARG ('A') => A => 'A'
                                    idlOS::snprintf( sObjectValue,
                                                     SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                     "'%s'",
                                                     sValueStr.mCharMax.value );
                                }

                                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                 "exec dbms_shard.reset_shard_resident_node('"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '"
                                                 QCM_SQL_STRING_SKIP_FMT"', '', "
                                                 "%s, "
                                                 QCM_SQL_VARCHAR_FMT" )",
                                                 sTableInfo->mUserName,
                                                 sTableInfo->mObjectName,
                                                 aTargetNodeName,
                                                 sObjectValue,
                                                 "" );
                            }

                            IDE_TEST( executeSQLNodeList( aStatement,
                                                          SDI_INTERNAL_OP_FAILOVER,
                                                          QCI_STMT_MASK_SP,
                                                          sSqlStr,
                                                          aTargetNodeName,
                                                          aAliveNodeList ) != IDE_SUCCESS );
                            continue;
                        case SDI_NON_SHARD_OBJECT:
                        default:
                            /* Non Shard Table�� ���� �ȵ�. */
                            IDE_DASSERT( 0 );
                            break;
                    }
                }
            }
        }
    }

    *aIsUnsetNode = sIsUnsetNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failoverAddReverseReplicationItems( qcStatement        * aStatement,
                                                 SChar              * aTargetNodeName,
                                                 SChar              * aFailoverNodeName,
                                                 sdiTableInfoList   * aTableInfoList,
                                                 sdiReplicaSetInfo  * aReplicaSetInfo,
                                                 sdiNode            * aTargetNodeInfo,
                                                 ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;

    sdiReplicaSet     * sPrevNodeReplicaSet = NULL;
    sdiReplicaSet     * sPrevPrevNodeReplicaSet = NULL;

    sdiShardObjectType   sShardTableType = SDI_NON_SHARD_OBJECT;

    UInt                i;
    UInt                j;

    SChar               sPartitionName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };

    UInt                sPrevReplicaSetId = SDI_REPLICASET_NULL_ID;
    UInt                sPrevPrevReplicaSetId = SDI_REPLICASET_NULL_ID;

    SChar               sPrevReverseReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
    SChar               sPrevPrevReverseReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];


    /* Find Old/New Node ReplicaSet */
    for ( i = 0 ; i < aReplicaSetInfo->mCount; i++ )
    {
        if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mPrimaryNodeName,
                             aTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            /* TargetNode�� ���� Failover ������ ReplicaSet�� ã�´�.
             * 2�� �̻� ������ ������ �����Ƿ� Prev�� ���� ã�´�. */
            if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mFirstBackupNodeName,
                                 aFailoverNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                sPrevNodeReplicaSet = &(aReplicaSetInfo->mReplicaSets[i]);
                sPrevReplicaSetId   = sPrevNodeReplicaSet->mReplicaSetId;
                idlOS::snprintf( sPrevReverseReplName, 
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1,
                                 "R_%s", 
                                 sPrevNodeReplicaSet->mFirstReplName );
            }
            else if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mSecondBackupNodeName,
                                      aFailoverNodeName,
                                      SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                sPrevPrevNodeReplicaSet = &(aReplicaSetInfo->mReplicaSets[i]);
                sPrevPrevReplicaSetId   = sPrevPrevNodeReplicaSet->mReplicaSetId;

                idlOS::snprintf( sPrevPrevReverseReplName, 
                                 SDI_REPLICATION_NAME_MAX_SIZE + 1,
                                 "R_%s", 
                                 sPrevPrevNodeReplicaSet->mSecondReplName );
            }
        }
    }

    /* R2HA Clone ó�� �ʿ� */
    /* N1 N2 N3 N4 N5 N6 ���� N1�� N4�� Failover �� �� ... 
     * N1->N4 ������ ����ȭ�� ���� ReverseRP�� �������� �ʴ´�.
     * �̷� Failover �߻��� Failback���� Fullsync �޴��� ���� �ؾ� ��. */
    /* Clone�� ReverseRP������ ó���Ѵ�. */

    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* Procedure�� RP ADD�� �������� �ʴ´�. */
        if ( sTableInfo->mObjectType == 'T' )
        {
            /* Default Partition check */
            if ( sTableInfo->mDefaultNodeId == aTargetNodeInfo->mNodeId )
            {
                if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                  sTableInfo->mDefaultPartitionReplicaSetId,
                                                  aFailoverNodeName ) == ID_TRUE )
                {
                    /* Failover �����ϸ� ReverseRP�� �����Ǿ� �ִ�. */
                    if ( sTableInfo->mDefaultPartitionReplicaSetId == sPrevReplicaSetId )
                    {
                        IDE_TEST( sdm::addReplTable( aStatement,
                                                     aFailoverNodeName,
                                                     sPrevReverseReplName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sTableInfo->mDefaultPartitionName,
                                                     SDM_REPL_CLONE,
                                                     ID_FALSE )
                                  != IDE_SUCCESS );
                    }
                    else if ( sTableInfo->mDefaultPartitionReplicaSetId == sPrevPrevReplicaSetId )
                    {
                        IDE_TEST( sdm::addReplTable( aStatement,
                                                     aFailoverNodeName,
                                                     sPrevPrevReverseReplName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sTableInfo->mDefaultPartitionName,
                                                     SDM_REPL_CLONE,
                                                     ID_FALSE )
                                  != IDE_SUCCESS );
                    }
                }
            }

            /* Range Info Check */
            IDE_TEST( sdm::getRangeInfo( aStatement,
                                         QC_SMI_STMT( aStatement ),
                                         aSMN,
                                         sTableInfo,
                                         &sRangeInfos,
                                         ID_FALSE )
                      != IDE_SUCCESS );

            /* check Table Type */
            sShardTableType = sdi::getShardObjectType( sTableInfo );

            for ( j = 0; j < sRangeInfos.mCount; j++ )
            {
                if ( sRangeInfos.mRanges[j].mNodeId == aTargetNodeInfo->mNodeId )
                {
                    if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                      sRangeInfos.mRanges[j].mReplicaSetId,
                                                      aFailoverNodeName ) == ID_TRUE )
                    {
                        switch ( sShardTableType )
                        {
                            case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                /* Range/Solo�� ���� Failover �����̸� �߰��ϸ� ��. */
                                idlOS::strncpy( sPartitionName,
                                                sRangeInfos.mRanges[j].mPartitionName,
                                                QC_MAX_OBJECT_NAME_LEN + 1 );
                                break;
                            case SDI_SOLO_DIST_OBJECT:
                                /* Range/Solo�� ���� Failover �����̸� �߰��ϸ� ��. */
                                idlOS::strncpy( sPartitionName,
                                                SDM_NA_STR,
                                                QC_MAX_OBJECT_NAME_LEN + 1 );

                                break;
                            case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                /* Composite�� HA�� �������� �ʱ� ������ RP add ���� ����. */
                                continue;
                            case SDI_CLONE_DIST_OBJECT:
                                /* Clone�� ReverseRP������ ó���Ѵ�. */
                                idlOS::strncpy( sPartitionName,
                                                SDM_NA_STR,
                                                QC_MAX_OBJECT_NAME_LEN + 1 );
                                break;
                            case SDI_NON_SHARD_OBJECT:
                            default:
                                /* Non Shard Table�� ���� �ȵ�. */
                                IDE_DASSERT( 0 );
                                break;
                        }

                        if ( sRangeInfos.mRanges[j].mReplicaSetId == sPrevReplicaSetId )
                        {
                            IDE_TEST( sdm::addReplTable( aStatement,
                                                         aFailoverNodeName,
                                                         sPrevReverseReplName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sPartitionName,
                                                         SDM_REPL_CLONE,
                                                         ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                        else if ( sRangeInfos.mRanges[j].mReplicaSetId == sPrevPrevReplicaSetId )
                        {
                            IDE_TEST( sdm::addReplTable( aStatement,
                                                         aFailoverNodeName,
                                                         sPrevPrevReverseReplName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sPartitionName,
                                                         SDM_REPL_CLONE,
                                                         ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                    }
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


IDE_RC qdsd::failoverAddReplicationItems( qcStatement        * aStatement,
                                          SChar              * aTargetNodeName,
                                          SChar              * aFailoverNodeName,
                                          SChar              * aFailoverNextNodeName,
                                          sdiTableInfoList   * aTableInfoList,
                                          sdiReplicaSetInfo  * aReplicaSetInfo,
                                          sdiNode            * aTargetNodeInfo,
                                          ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;

    sdiReplicaSet     * sOldNodeReplicaSet = NULL;

    sdiShardObjectType   sShardTableType = SDI_NON_SHARD_OBJECT;

    UInt                i;
    UInt                j;

    SChar               sReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
    SChar               sPartitionName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };

    idBool              sIsPrevNode = ID_FALSE;

    /* Find Old/New Node ReplicaSet */
    for ( i = 0 ; i < aReplicaSetInfo->mCount; i++ )
    {
        if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mPrimaryNodeName,
                             aTargetNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            /* TargetNode�� ���� Failover ������ ReplicaSet�� ã�´�.
             * 2�� �̻� ������ ������ �����Ƿ� Prev�� ���� ã�´�. */
            if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mFirstBackupNodeName,
                                 aFailoverNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                sOldNodeReplicaSet = &(aReplicaSetInfo->mReplicaSets[i]);
                sIsPrevNode = ID_TRUE;
            }
        }
    }

    if ( sOldNodeReplicaSet == NULL )
    {
        /* Prev ReplicaSet�� ��ã������ PrevPrev�� ã�´�. 
         * ��ã���� Failover ������ ReplicaSet�� ���� ��. */
        for ( i = 0 ; i < aReplicaSetInfo->mCount; i++ )
        {
            if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mPrimaryNodeName,
                                 aTargetNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                if ( idlOS::strncmp( aReplicaSetInfo->mReplicaSets[i].mSecondBackupNodeName,
                                     aFailoverNodeName,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
                {
                    sOldNodeReplicaSet = &(aReplicaSetInfo->mReplicaSets[i]);
                }
            }
        }
    }

    /* R2HA Clone ó�� �ʿ� */
    /* N1 N2 N3 N4 N5 N6 ���� N1�� N4�� Failover �� �� ... N1->N4 ������ ����ȭ�� ����.
     * �̷� Failover �߻��� Failback���� Fullsync �޴��� ���� �ؾ� ��. */
    /* N1 N2 N3 N4 N5 N6 ���� N2�� N4�� Failover �� �� ... N2->N5�� �����ϱ� ����ȭ add�� ��������
     * Clone�� N4->N2 �� �־�� �Ѵ�... */

    /* ���ǿ� �´� ReplicaSet�� ������ Add�� Item�� ����. */
    if ( sOldNodeReplicaSet == NULL )
    {
        IDE_CONT( normal_exit );
    }

    // �۾��� �ʿ��� Node���� ����
    /* N1->N3�� ������ Repl�� N2->N3�� �߰� �� �־�� �Ѵ�. */
    /* Clone Table�� ������ N2->N1�� �߰��� �־�� �Ѵ�. N2->N1�� N1->N2 �� N1�� First�� */
    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* Procedure�� RP ADD�� �������� �ʴ´�. */
        if ( sTableInfo->mObjectType == 'T' )
        {

            /* Default Partition check */
            if ( sTableInfo->mDefaultNodeId == aTargetNodeInfo->mNodeId )
            {
                if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                  sTableInfo->mDefaultPartitionReplicaSetId,
                                                  aFailoverNodeName ) == ID_TRUE )
                {
                    /* FailoverNode�� FailoverNextNode�� ��� Failover ������ ���� RP Add �ؾ� �Ѵ�. */
                    if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                      sTableInfo->mDefaultPartitionReplicaSetId,
                                                      aFailoverNextNodeName ) == ID_TRUE )
                    {
                        /* FailoverNode->FailoverNextNode�� RP Add �Ϸ���
                         * �׻� FirstBackupNode�� FailoverNode ���� ��. */
                        if ( sIsPrevNode == ID_TRUE )
                        {
                            IDE_TEST( sdm::addReplTable( aStatement,
                                                         aFailoverNodeName,
                                                         sOldNodeReplicaSet->mSecondReplName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sTableInfo->mDefaultPartitionName,
                                                         SDM_REPL_SENDER,
                                                         ID_FALSE )
                                      != IDE_SUCCESS );
                        }
                    }
                }
                                                  
            }

            /* Range Info Check */
            IDE_TEST( sdm::getRangeInfo( aStatement,
                                         QC_SMI_STMT( aStatement ),
                                         aSMN,
                                         sTableInfo,
                                         &sRangeInfos,
                                         ID_FALSE )
                      != IDE_SUCCESS );

            /* check Table Type */
            sShardTableType = sdi::getShardObjectType( sTableInfo );

            for ( j = 0; j < sRangeInfos.mCount; j++ )
            {
                if ( sRangeInfos.mRanges[j].mNodeId == aTargetNodeInfo->mNodeId )
                {
                    if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                      sRangeInfos.mRanges[j].mReplicaSetId,
                                                      aFailoverNodeName ) == ID_TRUE )
                    {
                        switch ( sShardTableType )
                        {
                            case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                /* Range/Solo�� ���� Failover �����̸� �߰��ϸ� ��. */
                                idlOS::strncpy( sReplName,
                                                sOldNodeReplicaSet->mSecondReplName,
                                                SDI_REPLICATION_NAME_MAX_SIZE + 1 );

                                idlOS::strncpy( sPartitionName,
                                                sRangeInfos.mRanges[j].mPartitionName,
                                                QC_MAX_OBJECT_NAME_LEN + 1 );
                                break;
                            case SDI_SOLO_DIST_OBJECT:
                                /* Range/Solo�� ���� Failover �����̸� �߰��ϸ� ��. */
                                idlOS::strncpy( sReplName,
                                                sOldNodeReplicaSet->mSecondReplName,
                                                SDI_REPLICATION_NAME_MAX_SIZE + 1 );

                                idlOS::strncpy( sPartitionName,
                                                SDM_NA_STR,
                                                QC_MAX_OBJECT_NAME_LEN + 1 );

                                break;
                            case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                /* Composite�� HA�� �������� �ʱ� ������ RP add ���� ����. */
                                continue;
                            case SDI_CLONE_DIST_OBJECT:
                                /* Clone�� ReverseRP���� �߰��Ѵ�. */
                                continue;
                            case SDI_NON_SHARD_OBJECT:
                            default:
                                /* Non Shard Table�� ���� �ȵ�. */
                                IDE_DASSERT( 0 );
                                break;
                        }

                        /* FailoverNode�� FailoverNextNode�� ��� Failover ������ ���� RP Add �ؾ� �Ѵ�. */
                        /* Clone�� ������ȭ�� FailoverNode�� ����ȭ ���Ḹ ������ �ȴ�. */
                        if ( sdm::checkFailoverAvailable( aReplicaSetInfo,
                                                          sRangeInfos.mRanges[j].mReplicaSetId,
                                                          aFailoverNextNodeName ) == ID_TRUE )
                        {
                            if ( sIsPrevNode == ID_TRUE )
                            {
                                IDE_TEST( sdm::addReplTable( aStatement,
                                                             aFailoverNodeName,
                                                             sReplName,
                                                             sTableInfo->mUserName,
                                                             sTableInfo->mObjectName,
                                                             sPartitionName,
                                                             SDM_REPL_SENDER,
                                                             ID_FALSE )
                                          != IDE_SUCCESS );
                            }
                        }
                    }
                }
            }

        }
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC qdsd::unsetShardTableForDrop( qcStatement       * aStatement )
{
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfoList  * sTmpTableInfoList = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    sdiTableInfo      * sTableInfo = NULL;
    ULong               sSMN = SDI_NULL_SMN;
    const UInt          sSqlStrLen = 4096;
    SChar               sSqlStr[sSqlStrLen] = {0,};
    SChar             * sMyNodeName;

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    sMyNodeName = sLocalMetaInfo.mNodeName;
    sSMN = sdi::getSMNForDataNode();
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sSMN )
              != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )
    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

        //EXEC DBMS_SHARD.UNSET_SHARD_TABLE_LOCAL('SYS','T1_SOLO' );
        IDE_TEST( executeRemoteSQL( aStatement,
                                    sMyNodeName,
                                    SDI_INTERNAL_OP_NORMAL,
                                    QCI_STMT_MASK_SP,
                                    sSqlStr,
                                    sSqlStrLen,
                                    "EXEC DBMS_SHARD.UNSET_SHARD_TABLE_LOCAL( '"
                                    QCM_SQL_STRING_SKIP_FMT"', '"
                                    QCM_SQL_STRING_SKIP_FMT"'); ",
                                    sTableInfo->mUserName,
                                    sTableInfo->mObjectName )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* PROJ-2748 Shard Failback */
/* ������ NodeName���� ���� Shard Meta�� �����´�. */
IDE_RC qdsd::getShardMetaFromOtherNode( qcStatement * aStatement,
                                        SChar       * aNodeName )
{
    qdShardParseTree  * sParseTree = NULL;
    sdiLocalMetaInfo  * sRemoteLocalMetaInfo = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH];
    iduListNode       * sNode  = NULL;
    iduListNode       * sDummy = NULL;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;
    
    if ( sParseTree->mNodeCount > 1 )
    {
        //���� ����� node info
        if ( IDU_LIST_IS_EMPTY( sParseTree->mNodeInfoList ) != ID_TRUE )
        {
            IDU_LIST_ITERATE_SAFE( sParseTree->mNodeInfoList, sNode, sDummy )
            {
                sRemoteLocalMetaInfo = (sdiLocalMetaInfo *)sNode->mObj;
            
                if( idlOS::strncmp( sRemoteLocalMetaInfo->mNodeName,
                                    aNodeName,
                                    SDI_NODE_NAME_MAX_SIZE ) == 0 )
                {
                    break;
                }
            }
        }

        IDE_TEST_RAISE( sRemoteLocalMetaInfo == NULL, ERR_NOT_EXIST_LOCAL_META_INFO );
        
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_SHARD.GLOBAL_META_INFO_ " );

        IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                          sSqlStr )
                  != IDE_SUCCESS );

        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "ALTER REPLICATION TEMPORARY GET TABLE "
                         "SYS_SHARD.NODES_, SYS_SHARD.OBJECTS_, SYS_SHARD.REPLICA_SETS_, "
                         "SYS_SHARD.SOLOS_, SYS_SHARD.CLONES_, SYS_SHARD.GLOBAL_META_INFO_, "
                         "SYS_SHARD.RANGES_,SYS_SHARD.FAILOVER_HISTORY_ "
                         "FROM '%s',%"ID_INT32_FMT" ",
                         sRemoteLocalMetaInfo->mInternalRPHostIP, // internal_host_ip
                         sRemoteLocalMetaInfo->mInternalRPPortNo ); // internal_port_no

        IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                          sSqlStr )
                  != IDE_SUCCESS );

        // ���� ���� ���� ���� SNM�� ����ȭ
        IDE_TEST( sdi::reloadSMNForDataNode( QC_SMI_STMT( aStatement ) )
                  != IDE_SUCCESS );
        
        // session SMN
        qci::mSessionCallback.mSetShardMetaNumber( aStatement->session->mMmSession,
                                                   sdi::getSMNForDataNode());
    }
    else
    {
        /* Failback �̴ϱ� Zookeeper�� NodeInfo�� ���� �ؾ߸� �Ѵ�. 0�� ���� ���� �� ����. */
        IDE_RAISE( ERR_NOT_EXIST_LOCAL_META_INFO );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXIST_LOCAL_META_INFO )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "qdsd::getShardMetaFromOtherNode",
                                  "Meta information of remote node does not exist in cluster meta." ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failbackResetShardMeta( qcStatement        * aStatement,
                                     SChar              * aTargetNodeName,
                                     SChar              * aFailbackFromNodeName,
                                     sdiTableInfoList   * aTableInfoList,
                                     sdiReplicaSetInfo  * aReplicaSetInfo,
                                     sdiReplicaSetInfo  * aFailoverHistoryInfo,
                                     iduList            * aAliveNodeList,
                                     UInt                 aReplicaSetId,
                                     ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;
    sdiReplicaSet     * sFailoverHistory = NULL;

    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;

    UInt                i;
    UInt                j;

    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    sdiValue            sValueStr;

    SChar               sObjectValue[SDI_RANGE_VARCHAR_MAX_PRECISION + 1];

    if ( idlOS::strncmp( aTargetNodeName,
                         aFailbackFromNodeName,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
    {
        IDE_DASSERT( 0 );
    }

    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* check Table Type */
        sShardTableType = sdi::getShardObjectType( sTableInfo );

        if ( sShardTableType == SDI_CLONE_DIST_OBJECT )
        {
            /* Clone�̸� Failover ����ÿ� ������ ����Ƿ� ������ �ٽ� Set ���־�� �Ѵ�. */
            /* �׳� Set�̹Ƿ� Table/Procedure �������. */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "exec dbms_shard.set_shard_clone_local('"
                             QCM_SQL_STRING_SKIP_FMT"', '"
                             QCM_SQL_STRING_SKIP_FMT"', '"
                             QCM_SQL_STRING_SKIP_FMT"', "
                             QCM_SQL_UINT32_FMT", "
                             QCM_SQL_UINT32_FMT") ", // TASK-7307
                             sTableInfo->mUserName,
                             sTableInfo->mObjectName,
                             aTargetNodeName,
                             aReplicaSetId,
                             SDM_CALLED_BY_SHARD_FAILBACK );

            IDE_TEST( executeSQLNodeList( aStatement,
                                          SDI_INTERNAL_OP_FAILBACK,
                                          QCI_STMT_MASK_SP,
                                          sSqlStr,
                                          NULL,
                                          aAliveNodeList ) != IDE_SUCCESS );
        }
        else
        {
            for ( i = 0; i < aFailoverHistoryInfo->mCount; i++ )
            {
                sFailoverHistory = &aFailoverHistoryInfo->mReplicaSets[i];

                /* Default Partition check */
                if ( sTableInfo->mDefaultPartitionReplicaSetId == sFailoverHistory->mReplicaSetId )
                {
                    /* FailoverHistory�� Primary�� ���ϸ� �ȵ�,
                     * ���� ReplicaSet�� Primary�� ���ؾߵ�. */
                    if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                      sTableInfo->mDefaultPartitionReplicaSetId,
                                                      aTargetNodeName ) == ID_TRUE )
                    {
                        /* Send resetShardPartitioinNode to All Alive Node */
                        if ( sTableInfo->mObjectType == 'T' ) /* Table */
                        {
                            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                             "exec dbms_shard.reset_shard_partition_node( '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', "
                                             QCM_SQL_VARCHAR_FMT", "
                                             QCM_SQL_UINT32_FMT") ", // TASK-7307
                                             sTableInfo->mUserName,
                                             sTableInfo->mObjectName,
                                             aFailbackFromNodeName,
                                             aTargetNodeName,
                                             sTableInfo->mDefaultPartitionName,
                                             SDM_CALLED_BY_SHARD_FAILBACK );
                        }
                        else /* Procedure */
                        {
                            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                             "exec dbms_shard.reset_shard_resident_node( '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', '"
                                             QCM_SQL_STRING_SKIP_FMT"', "
                                             QCM_SQL_VARCHAR_FMT", "
                                             QCM_SQL_VARCHAR_FMT", "
                                             QCM_SQL_UINT32_FMT" )",
                                             sTableInfo->mUserName,
                                             sTableInfo->mObjectName,
                                             aFailbackFromNodeName,
                                             aTargetNodeName,
                                             "",
                                             "",
                                             1 );
                        }

                        IDE_TEST( executeSQLNodeList( aStatement,
                                                      SDI_INTERNAL_OP_FAILBACK,
                                                      QCI_STMT_MASK_SP,
                                                      sSqlStr,
                                                      NULL,
                                                      aAliveNodeList ) != IDE_SUCCESS );

                    }
                    else
                    {
                        /* ReplicaSet�� FailbackFromNode�� �ƴϸ� Failover�� DataFailover�� �߻����� �ʾҴ�.
                         * �׳� �θ� �ȴ�. */
                    }
                }

                /* Range Info Check */
                IDE_TEST( sdm::getRangeInfo( aStatement,
                                             QC_SMI_STMT( aStatement ),
                                             aSMN,
                                             sTableInfo,
                                             &sRangeInfos,
                                             ID_FALSE )
                          != IDE_SUCCESS );

                /* check Table Type */
                sShardTableType = sdi::getShardObjectType( sTableInfo );

                for ( j = 0; j < sRangeInfos.mCount; j++ )
                {

                    if ( sRangeInfos.mRanges[j].mReplicaSetId == sFailoverHistory->mReplicaSetId )
                    {
                        if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                          sRangeInfos.mRanges[j].mReplicaSetId,
                                                          aTargetNodeName ) == ID_TRUE )
                        {
                            switch ( sShardTableType )
                            {
                                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                case SDI_SOLO_DIST_OBJECT:
                                    /* send resetShardPartitionNode to All Alive Node */
                                    if ( sTableInfo->mObjectType == 'T' ) /* Table */
                                    {
                                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                         "exec dbms_shard.reset_shard_partition_node('"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', "
                                                         QCM_SQL_VARCHAR_FMT", "
                                                         QCM_SQL_UINT32_FMT") ", // TASK-7307
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         aFailbackFromNodeName,
                                                         aTargetNodeName,
                                                         sRangeInfos.mRanges[j].mPartitionName,
                                                         SDM_CALLED_BY_SHARD_FAILBACK );
                                    }
                                    else /* Procedure */
                                    {
                                        if ( sTableInfo->mSplitMethod == SDI_SPLIT_HASH )
                                        {
                                            IDE_TEST( sdi::getValueStr( MTD_INTEGER_ID,
                                                                        &sRangeInfos.mRanges[j].mValue,
                                                                        &sValueStr )
                                                      != IDE_SUCCESS );
                                        }
                                        else
                                        {
                                            IDE_TEST( sdi::getValueStr( sTableInfo->mKeyDataType,
                                                                        &sRangeInfos.mRanges[j].mValue,
                                                                        &sValueStr )
                                                      != IDE_SUCCESS );
                                        }

                                        if ( sValueStr.mCharMax.value[0] == '\'' )
                                        {
                                            // INPUT ARG ('''A''') => 'A' => '''A'''
                                            idlOS::snprintf( sObjectValue,
                                                             SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                             "''%s''",
                                                             sValueStr.mCharMax.value );
                                        }
                                        else
                                        {
                                            // INPUT ARG ('A') => A => 'A'
                                            idlOS::snprintf( sObjectValue,
                                                             SDI_RANGE_VARCHAR_MAX_PRECISION + 1,
                                                             "'%s'",
                                                             sValueStr.mCharMax.value );
                                        }

                                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                                         "exec dbms_shard.reset_shard_resident_node('"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', '"
                                                         QCM_SQL_STRING_SKIP_FMT"', "
                                                         "%s, "
                                                         QCM_SQL_VARCHAR_FMT" )",
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         aFailbackFromNodeName,
                                                         aTargetNodeName,
                                                         sObjectValue,
                                                         "" );
                                    }

                                    IDE_TEST( executeSQLNodeList( aStatement,
                                                                  SDI_INTERNAL_OP_FAILBACK,
                                                                  QCI_STMT_MASK_SP,
                                                                  sSqlStr,
                                                                  NULL,
                                                                  aAliveNodeList ) != IDE_SUCCESS );
                                    break;
                                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                    /* Composite�� HA�� �������� �ʱ� ������ Failover ����� �ƴϴ�. */
                                    continue;
                                case SDI_CLONE_DIST_OBJECT:
                                    /* Clone�� ���� ó�� �Ѵ�. */
                                    /* ���� ������ �ȵȴ�. */
                                    IDE_DASSERT( 0 );
                                    continue;
                                case SDI_NON_SHARD_OBJECT:
                                default:
                                    /* Non Shard Table�� ���� �ȵ�. */
                                    IDE_DASSERT( 0 );
                                    break;
                            }
                        }
                        else
                        {
                            /* Failback ����� �ƴϸ� �ƹ��͵� ���Ѵ�. */
                        }
                    }
                }
            }

        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::lockTables4Failback( qcStatement        * aStatement,
                                  SChar              * aSQLBuf,
                                  UInt                 aSQLBufSize,
                                  SChar              * aFailbackFromNodeName,
                                  sdiTableInfoList   * aTableInfoList,
                                  sdiReplicaSetInfo  * aReplicaSetInfo,
                                  sdiReplicaSetInfo  * aFailoverHistoryInfo,
                                  ULong                aSMN )
{
    UInt               sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt               sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);
    UInt               sLoopCnt = 0;

    idBool             sIsCancel = ID_FALSE;
    idBool             sIsLockAcquired = ID_FALSE;
    idBool             sIsLocked = ID_FALSE;

    sdiTableInfoList * sTmpTableInfoList = NULL;
    sdiTableInfo     * sTableInfo = NULL;
    sdiReplicaSet    * sFailoverHistory = NULL;

    sdiRangeInfo       sRangeInfos;

    sdiShardObjectType sShardTableType = SDI_NON_SHARD_OBJECT;
   
    UInt               i;
    UInt               j;
    SChar              sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    /* the reomte cancel is not running. therefore remote lock timeout property sec(shard_ddl_lock_timeout).
     * if a application lock order is A->B->C then executing reshard table should be in the order A, B and C 
     * if A table is locked then the next table lock may will be acquired.
     */
    sLoopCnt = 0;
    while ( (sLoopCnt++ < sShardDDLLockTryCount) && (sIsLockAcquired != ID_TRUE ) )
    {
        sIsCancel = ID_FALSE;
        sIsLockAcquired = ID_FALSE;

        IDE_TEST( executeRemoteSQL( aStatement, 
                                    aFailbackFromNodeName,
                                    SDI_INTERNAL_OP_FAILBACK,
                                    QCI_STMT_MASK_SP,
                                    aSQLBuf,
                                    aSQLBufSize,
                                    "SAVEPOINT __FAILBACK_SVP0") != IDE_SUCCESS);

        for ( sTmpTableInfoList = aTableInfoList;
              sTmpTableInfoList != NULL;
              sTmpTableInfoList = sTmpTableInfoList->mNext )
        {
            sIsLocked = ID_FALSE;
            sTableInfo = sTmpTableInfoList->mTableInfo;

            if ( sTableInfo->mObjectType == 'T' )
            {
                idlOS::snprintf( sBufferNameStr,
                                 QC_MAX_OBJECT_NAME_LEN + 1,
                                 "%s%s",
                                 SDI_BACKUP_TABLE_PREFIX,
                                 sTableInfo->mObjectName );

                for ( i = 0; i < aFailoverHistoryInfo->mCount; i++ )
                {
                    sFailoverHistory = &aFailoverHistoryInfo->mReplicaSets[i];

                    /* Default Partition check */
                    if ( sTableInfo->mDefaultPartitionReplicaSetId == sFailoverHistory->mReplicaSetId )
                    {
                        if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                          sTableInfo->mDefaultPartitionReplicaSetId,
                                                          sdiZookeeper::mMyNodeName ) == ID_TRUE )
                        {
                            /* R2HA ���� Remote Lock ����� �ִ�(Service Node) Local Node�� Lock�� �ʿ��Ҽ��� �ִ�. */
                            /* Swap ����̶�� Lock�� ȹ���ؾ� �Ѵ�. */
                            /* Default Partition Lock ȹ�� */
                            if( executeRemoteSQL( aStatement, 
                                                  aFailbackFromNodeName,
                                                  SDI_INTERNAL_OP_FAILBACK,
                                                  QCI_STMT_MASK_DML,
                                                  aSQLBuf,
                                                  aSQLBufSize,
                                                  "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,

    //                                              "LOCK TABLE %s.%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                  sTableInfo->mUserName,
                                                  sTableInfo->mObjectName,
    //                                              sTableInfo->mDefaultPartitionName,
                                                  sRemoteLockTimeout ) != IDE_SUCCESS )
                            {
                                /* cancel retry */
                                sIsCancel = ID_TRUE;
                                break;
                            }
                            /* _BAK_Table Default Partition Lock ȹ�� */
                            if( executeRemoteSQL( aStatement, 
                                                  aFailbackFromNodeName,
                                                  SDI_INTERNAL_OP_FAILBACK,
                                                  QCI_STMT_MASK_DML,
                                                  aSQLBuf,
                                                  aSQLBufSize,
                                                  "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
    //                                              "LOCK TABLE %s._BAK_%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                  sTableInfo->mUserName,
                                                  sBufferNameStr,
    //                                              sTableInfo->mDefaultPartitionName,
                                                  sRemoteLockTimeout ) != IDE_SUCCESS )
                            {
                                /* cancel retry */
                                sIsCancel = ID_TRUE;
                                break;
                            }

                            /* Table lock ȹ�� ����, ���� Table�� Ȯ���Ѵ�. */
                            continue;
                        }
                        else
                        {
                            /* Swap ����� �ƴϸ� Lock ���� �ʿ䰡 ����. */
                        }
                    }

                    /* Range Info Check */
                    IDE_TEST( sdm::getRangeInfo( aStatement,
                                                 QC_SMI_STMT( aStatement ),
                                                 aSMN,
                                                 sTableInfo,
                                                 &sRangeInfos,
                                                 ID_FALSE )
                              != IDE_SUCCESS );

                    /* check Table Type */
                    sShardTableType = sdi::getShardObjectType( sTableInfo );

                    for ( j = 0; j < sRangeInfos.mCount; j++ )
                    {

                        if ( sRangeInfos.mRanges[j].mReplicaSetId == sFailoverHistory->mReplicaSetId )
                        {
                            if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                              sRangeInfos.mRanges[j].mReplicaSetId,
                                                              sdiZookeeper::mMyNodeName ) == ID_TRUE )
                            {
                                switch ( sShardTableType )
                                {
                                    case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                        /* Partition Swap ����̴�. Partition lock ȹ���Ѵ�. */
                                        /* Partition Lock ȹ�� */
                                        if( executeRemoteSQL( aStatement, 
                                                              aFailbackFromNodeName,
                                                              SDI_INTERNAL_OP_FAILBACK,
                                                              QCI_STMT_MASK_DML,
                                                              aSQLBuf,
                                                              aSQLBufSize,
                                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
    //                                                          "LOCK TABLE %s.%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                              sTableInfo->mUserName,
                                                              sTableInfo->mObjectName,
    //                                                          sRangeInfos.mRanges[j].mPartitionName,
                                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                                        {
                                            /* cancel retry */
                                            sIsCancel = ID_TRUE;
                                            break;
                                        }
                                        /* _BAK_ Table Partition Lock ȹ�� */
                                        if( executeRemoteSQL( aStatement, 
                                                              aFailbackFromNodeName,
                                                              SDI_INTERNAL_OP_FAILBACK,
                                                              QCI_STMT_MASK_DML,
                                                              aSQLBuf,
                                                              aSQLBufSize,
                                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
    //                                                          "LOCK TABLE %s._BAK_%s PARTITION ( %s ) IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                              sTableInfo->mUserName,
                                                              sBufferNameStr,
    //                                                          sRangeInfos.mRanges[j].mPartitionName,
                                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                                        {
                                            /* cancel retry */
                                            sIsCancel = ID_TRUE;
                                            break;
                                        }
                                        sIsLocked = ID_TRUE;
                                        break;
                                    case SDI_SOLO_DIST_OBJECT:
                                        /* Table Swap ����̴�. Table lock ȹ���Ѵ�. */
                                        /* Table Lock ȹ�� */
                                        if( executeRemoteSQL( aStatement, 
                                                              aFailbackFromNodeName,
                                                              SDI_INTERNAL_OP_FAILBACK,
                                                              QCI_STMT_MASK_DML,
                                                              aSQLBuf,
                                                              aSQLBufSize,
                                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                              sTableInfo->mUserName,
                                                              sTableInfo->mObjectName,
                                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                                        {
                                            /* cancel retry */
                                            sIsCancel = ID_TRUE;
                                            break;
                                        }
                                        /* _BAK_ Table Lock ȹ�� */
                                        if( executeRemoteSQL( aStatement, 
                                                              aFailbackFromNodeName,
                                                              SDI_INTERNAL_OP_FAILBACK,
                                                              QCI_STMT_MASK_DML,
                                                              aSQLBuf,
                                                              aSQLBufSize,
                                                              "LOCK TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" IN EXCLUSIVE MODE WAIT %"ID_UINT32_FMT,
                                                              sTableInfo->mUserName,
                                                              sBufferNameStr,
                                                              sRemoteLockTimeout ) != IDE_SUCCESS )
                                        {
                                            /* cancel retry */
                                            sIsCancel = ID_TRUE;
                                            break;
                                        }
                                        sIsLocked = ID_TRUE;
                                        break;
                                    case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                        /* Composite�� HA�� �������� �ʱ� ������ Partition Swap ����.
                                         * Lock ���� �ʿ� ����. */
                                        continue;
                                    case SDI_CLONE_DIST_OBJECT:
                                        /* Clone �� �� Node�� Primary�� �����ϱ� ������ Partition Swap �� �ʿ� ����.
                                         * Lock ���� �ʿ� ����. */
                                        /* R2HA Lock ���� �ʿ䰡 �������� �ִ�. Service ������ ����? */
                                        continue;
                                    case SDI_NON_SHARD_OBJECT:
                                    default:
                                        /* Non Shard Table�� ���� �ȵ�. */
                                        IDE_DASSERT( 0 );
                                        break;
                                }

                            }
                            else
                            {
                                /* Swap ����� �ƴϸ� Lock ���� �ʿ䰡 ����. */
                            }
                        }

                        /* Table Lock�� ������� ���� Table Lock�� ȹ���Ϸ� ����.
                         * Range Info Ž�� �߿� Lock ȹ�濡 ���������� Retry �Ұ��̴�. */
                        if ( ( sIsCancel == ID_TRUE ) || ( sIsLocked == ID_TRUE ) )
                        {
                            break;
                        }
                    }

                    if ( sIsCancel == ID_TRUE )
                    {
                        /* RangeInfo Ž���߿� Lock ȹ�濡 �����ߴ�. */
                        break;
                    }
                }
            }
            else
            {
                /* R2HA PROCEDURE LOCK NEED */
            }
        }
        if ( sIsCancel == ID_TRUE )
        {
            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aFailbackFromNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_SP,
                                        aSQLBuf,
                                        aSQLBufSize,
                                        "ROLLBACK TO SAVEPOINT __FAILBACK_SVP0") != IDE_SUCCESS );
            sIsLockAcquired = ID_FALSE;
        }
        else
        {
            sIsLockAcquired = ID_TRUE;
            break;
        }
    }

    IDE_TEST_RAISE( sIsLockAcquired != ID_TRUE, ERR_FAILBACKLOCK );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_FAILBACKLOCK )
    {
        IDE_SET( ideSetErrorCode( qpERR_ABORT_QCM_LOCK_FAIL ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    (void)executeRemoteSQL( aStatement, 
                            aFailbackFromNodeName,
                            SDI_INTERNAL_OP_FAILBACK,
                            QCI_STMT_MASK_SP,
                            aSQLBuf,
                            aSQLBufSize,
                            "ROLLBACK TO SAVEPOINT __FAILBACK_SVP0" );
    IDE_POP();
    return IDE_FAILURE;
}

IDE_RC qdsd::failbackTablePartitionSwap( qcStatement        * aStatement,
                                         SChar              * aFailbackFromNodeName,
                                         sdiTableInfoList   * aTableInfoList,
                                         sdiReplicaSetInfo  * aReplicaSetInfo,
                                         sdiReplicaSetInfo  * aFailoverHistoryInfo,
                                         ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    

    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;
    sdiReplicaSet     * sFailoverHistory = NULL;

    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;
    
    UInt                i;
    UInt                j;

    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    SChar               sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
    
    for ( sTmpTableInfoList = aTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* Procedure�� Swap�� Partition�� ����. */
        if ( sTableInfo->mObjectType == 'T' )
        {
            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableInfo->mObjectName );

            for ( i = 0; i < aFailoverHistoryInfo->mCount; i++ )
            {
                sFailoverHistory = &aFailoverHistoryInfo->mReplicaSets[i];

                /* Default Partition check */
                if ( sTableInfo->mDefaultPartitionReplicaSetId == sFailoverHistory->mReplicaSetId )
                {
                    if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                      sTableInfo->mDefaultPartitionReplicaSetId,
                                                      sdiZookeeper::mMyNodeName ) == ID_TRUE )
                    {
                        /* Partition Swap Send to TargetNode */
                        idlOS::snprintf( sSqlStr, 
                                         QD_MAX_SQL_LENGTH,
                                         "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                         sTableInfo->mUserName,
                                         sBufferNameStr,
                                         sTableInfo->mUserName,
                                         sTableInfo->mObjectName,
                                         sTableInfo->mDefaultPartitionName );

                        IDE_TEST( executeRemoteSQL( aStatement, 
                                                    aFailbackFromNodeName,
                                                    SDI_INTERNAL_OP_FAILBACK,
                                                    QCI_STMT_MASK_DDL,
                                                    sSqlStr,
                                                    QD_MAX_SQL_LENGTH,
                                                    NULL )
                                    != IDE_SUCCESS );
                    }
                    else
                    {
                        /* Failback �Ҽ� ���ٸ� Swap ���� �ʴ´�. */
                    }
                }

                /* Range Info Check */
                IDE_TEST( sdm::getRangeInfo( aStatement,
                                             QC_SMI_STMT( aStatement ),
                                             aSMN,
                                             sTableInfo,
                                             &sRangeInfos,
                                             ID_FALSE )
                          != IDE_SUCCESS );

                /* check Table Type */
                sShardTableType = sdi::getShardObjectType( sTableInfo );

                for ( j = 0; j < sRangeInfos.mCount; j++ )
                {

                    if ( sRangeInfos.mRanges[j].mReplicaSetId == sFailoverHistory->mReplicaSetId )
                    {
                        if ( sdm::checkFailbackAvailable( aReplicaSetInfo,
                                                          sRangeInfos.mRanges[j].mReplicaSetId,
                                                          sdiZookeeper::mMyNodeName ) == ID_TRUE )
                        {
                            switch ( sShardTableType )
                            {
                                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                    /* Partition Swap */
                                    idlOS::snprintf( sSqlStr, 
                                                     QD_MAX_SQL_LENGTH,
                                                     "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" PARTITION "QCM_SQL_STRING_SKIP_FMT"",
                                                     sTableInfo->mUserName,
                                                     sBufferNameStr,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sRangeInfos.mRanges[j].mPartitionName );
                                    break;
                                case SDI_SOLO_DIST_OBJECT:
                                    /* Partition Swap */
                                    idlOS::snprintf( sSqlStr, 
                                                     QD_MAX_SQL_LENGTH,
                                                     "ALTER TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" REPLACE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"",
                                                     sTableInfo->mUserName,
                                                     sBufferNameStr,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName );

                                    break;
                                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                    /* Composite�� HA�� �������� �ʱ� ������ Partition Swap �� �ʿ� ����. */
                                    continue;
                                case SDI_CLONE_DIST_OBJECT:
                                    /* Clone �� �� Node�� Primary�� �����ϱ� ������ Partition Swap �� �ʿ� ����. */
                                    continue;
                                case SDI_NON_SHARD_OBJECT:
                                default:
                                    /* Non Shard Table�� ���� �ȵ�. */
                                    IDE_DASSERT( 0 );
                                    break;
                            }
                            IDE_TEST( executeRemoteSQL( aStatement, 
                                                        aFailbackFromNodeName,
                                                        SDI_INTERNAL_OP_FAILBACK,
                                                        QCI_STMT_MASK_DDL,
                                                        sSqlStr,
                                                        QD_MAX_SQL_LENGTH,
                                                        NULL )
                                        != IDE_SUCCESS );
                        }
                        else
                        {
                            /* Failover �Ҽ� ���ٸ� Swap ���� �ʴ´�. */
                        }
                    }
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failbackOrganizeReverseRP( qcStatement        * aStatement,
                                        SChar              * aFailbackFromNodeName,
                                        sdiTableInfoList   * aTableInfoList,
                                        sdiReplicaSetInfo  * aFailoverHistoryInfo,
                                        UInt                 aMainReplicaSetId,
                                        ULong                aSMN )
{
    sdiTableInfoList  * sTmpTableInfoList = NULL;    
    sdiTableInfo      * sTableInfo  = NULL;
    sdiRangeInfo        sRangeInfos;

    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;

    SChar               sReverseReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
    SChar               sPartitionName[QC_MAX_OBJECT_NAME_LEN + 1] = { 0, };

    sdiReplicaSet     * sReplicaSet = NULL;

    qdShardParseTree  * sParseTree = NULL;

    UInt                i;
    UInt                j;

    sdiLocalMetaInfo  * sNodeMetaInfo = NULL;

    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    ideLog::log( IDE_SD_0, "[SHARD_FAILBACK] Start REVERSE REP" );

    /* Reverse RP Create + Add Item */
    for ( i = 0; i < aFailoverHistoryInfo->mCount; i++ )
    {
        sReplicaSet = &aFailoverHistoryInfo->mReplicaSets[i];

        if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                             aFailbackFromNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sReverseReplName, 
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             "R_%s", 
                             sReplicaSet->mFirstReplName );
        }
        else if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                                  aFailbackFromNodeName,
                                  SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sReverseReplName, 
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             "R_%s", 
                             sReplicaSet->mSecondReplName );
        }
        else
        {
            idlOS::snprintf( sReverseReplName,
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             SDM_NA_STR );
        }

        if ( idlOS::strncmp( sReverseReplName,
                             SDM_NA_STR,
                             SDI_REPLICATION_NAME_MAX_SIZE + 1 ) != 0 )
        {
            sNodeMetaInfo = sdiZookeeper::getNodeInfo( sParseTree->mNodeInfoList,
                                                       aFailbackFromNodeName );

            IDE_TEST( createReverseReplication( aStatement,
                                                sdiZookeeper::mMyNodeName,
                                                sReverseReplName,
                                                sNodeMetaInfo->mNodeName,
                                                sNodeMetaInfo->mInternalRPHostIP,
                                                sNodeMetaInfo->mInternalRPPortNo,
                                                SDI_REPL_RECEIVER )
                      != IDE_SUCCESS );

            ideLog::log(IDE_SD_21,"[SHARD_FAILBACK] Reverser Replication Created : %s", sReverseReplName );

            /* RP Add */
            for ( sTmpTableInfoList = aTableInfoList;
                  sTmpTableInfoList != NULL;
                  sTmpTableInfoList = sTmpTableInfoList->mNext )
            {
                sTableInfo = sTmpTableInfoList->mTableInfo;
                if ( sTableInfo->mObjectType == 'T' )
                {
                    if ( sTableInfo->mDefaultPartitionReplicaSetId == sReplicaSet->mReplicaSetId )
                    {
                        IDE_TEST( sdm::addReplTable( aStatement,
                                                     sdiZookeeper::mMyNodeName,
                                                     sReverseReplName,
                                                     sTableInfo->mUserName,
                                                     sTableInfo->mObjectName,
                                                     sTableInfo->mDefaultPartitionName,
                                                     SDM_REPL_CLONE,
                                                     ID_TRUE )
                                  != IDE_SUCCESS );
                    }
                    /* Range Info Check */
                    IDE_TEST( sdm::getRangeInfo( aStatement,
                                                 QC_SMI_STMT( aStatement ),
                                                 aSMN,
                                                 sTableInfo,
                                                 &sRangeInfos,
                                                 ID_FALSE )
                              != IDE_SUCCESS );

                    sShardTableType = sdi::getShardObjectType( sTableInfo );

                    /* CLONE�� Failover �ÿ� CLONES_���� ���� �Ǿ��� ������ ���� ó���Ѵ�. */
                    if ( sShardTableType == SDI_CLONE_DIST_OBJECT )
                    {
                        /* ���� ����Ǿ� �ִ� ReplicaSet�϶��� Clone ó���Ѵ�. */
                        if ( aMainReplicaSetId == sReplicaSet->mReplicaSetId )
                        {
                            /* CLONE�� PartitionName�� ����. */
                            idlOS::strncpy( sPartitionName,
                                            SDM_NA_STR,
                                            QC_MAX_OBJECT_NAME_LEN + 1 );

                            IDE_TEST( sdm::addReplTable( aStatement,
                                                         sdiZookeeper::mMyNodeName,
                                                         sReverseReplName,
                                                         sTableInfo->mUserName,
                                                         sTableInfo->mObjectName,
                                                         sPartitionName,
                                                         SDM_REPL_CLONE,
                                                         ID_TRUE )
                                      != IDE_SUCCESS );
                        }
                    }
                    else
                    {
                        for ( j = 0; j< sRangeInfos.mCount; j++ )
                        {
                            if ( sRangeInfos.mRanges[j].mReplicaSetId == sReplicaSet->mReplicaSetId )
                            {
                                switch( sShardTableType )
                                {
                                    case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                                        /* Range�� Partition�� �����Ѵ�. */
                                        idlOS::strncpy( sPartitionName,
                                                        sRangeInfos.mRanges[j].mPartitionName,
                                                        QC_MAX_OBJECT_NAME_LEN + 1 );
                                        break;
                                    case SDI_SOLO_DIST_OBJECT:
                                        /* SOLO�� PartitionName�� ����. */
                                        idlOS::strncpy( sPartitionName,
                                                        SDM_NA_STR,
                                                        QC_MAX_OBJECT_NAME_LEN + 1 );

                                        break;
                                    case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                                        /* Composite�� HA�� �������� �ʱ� ������ RP add ���� ����. */
                                        continue;
                                    case SDI_CLONE_DIST_OBJECT:
                                        /* Clone�� ���� ó�� �Ͽ���. */
                                    case SDI_NON_SHARD_OBJECT:
                                    default:
                                        /* Non Shard Table�� ���� �ȵ�. */
                                        IDE_DASSERT( 0 );
                                        break;
                                }
                                IDE_TEST( sdm::addReplTable( aStatement,
                                                             sdiZookeeper::mMyNodeName,
                                                             sReverseReplName,
                                                             sTableInfo->mUserName,
                                                             sTableInfo->mObjectName,
                                                             sPartitionName,
                                                             SDM_REPL_CLONE,
                                                             ID_TRUE )
                                          != IDE_SUCCESS );
                            }
                        }
                    }
                }
            }

            /* Reverse RP Create�� Item Add �� �������� Flush �޾ƾ� �Ѵ�. */
            /* Receiver Create�̱� ������ Sender�� �̹� Send ���ϼ� �ִ�. Flush�� Ȯ�� �Ѵ�.
             * Reverse RP�� Sender ������ Start �ص��� ������
             * Failback���� Start ��û �� KILL �� �� �ٽ� �õ��ϼ� �ִ�. */
            if( flushReplicationForInternal( aStatement,
                                             aFailbackFromNodeName,
                                             sReverseReplName )
                == IDE_SUCCESS )
            {
                ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FLUSH REVERSE REP COMPLETE %s", sReverseReplName);
            }
            else
            {
                /* Flush�� �����Ͽ��ٸ� start �ȵǾ� �ִ°ɷ� ���� Start ��Ų��. */
                IDE_TEST( startReplicationForInternal( aStatement,
                                                       aFailbackFromNodeName,
                                                       sReverseReplName )
                          != IDE_SUCCESS );

                ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FLUSH REVERSE REP, %s \n", sReverseReplName );
                
                IDE_TEST( flushReplicationForInternal( aStatement,
                                                       aFailbackFromNodeName,
                                                       sReverseReplName )
                          != IDE_SUCCESS );

                ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FLUSH REVERSE REP COMPLETE %s", sReverseReplName);
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* After Lock�� Flush �� ������ �Ѵ�. */
IDE_RC qdsd::failbackOrganizeReverseRPAfterLock( qcStatement        * aStatement,
                                                 SChar              * aFailbackFromNodeName,
                                                 sdiReplicaSetInfo  * aFailoverHistoryInfo )
{
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    SChar               sReverseReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];

    sdiReplicaSet     * sReplicaSet = NULL;

    UInt                i;

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Start REVERSE REP After Lock" );

    for ( i = 0; i < aFailoverHistoryInfo->mCount; i++ )
    {
        sReplicaSet = &aFailoverHistoryInfo->mReplicaSets[i];

        if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                             aFailbackFromNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sReverseReplName, 
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             "R_%s", 
                             sReplicaSet->mFirstReplName );
        }
        else if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                                  aFailbackFromNodeName,
                                  SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sReverseReplName, 
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             "R_%s", 
                             sReplicaSet->mSecondReplName );
        }
        else
        {
            idlOS::snprintf( sReverseReplName,
                             SDI_REPLICATION_NAME_MAX_SIZE + 1,
                             SDM_NA_STR );
        }

        if ( idlOS::strncmp( sReverseReplName,
                             SDM_NA_STR,
                             SDI_REPLICATION_NAME_MAX_SIZE + 1 ) != 0 )
        {
            /* Lock ���� �Ŀ� �� ���� �־��� ������ Flush �޾ƿ´�. */
            IDE_TEST( flushReplicationForInternal( aStatement,
                                                   aFailbackFromNodeName,
                                                   sReverseReplName )
                      != IDE_SUCCESS );

            ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] AFTER LOCK FLUSH REVERSE REP COMPLETE %s", sReverseReplName);

            /* Flush �������� Stop �صд�. */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP ", 
                             sReverseReplName );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aFailbackFromNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] STOP REVERSE REP %s", sReverseReplName);

            /* Reverse RP�� Commit �ÿ� Drop�� TxDDL�� ó���Ѵ�. */
            /* Sender/Receiver �Ѵ� ���� �ؾ� �Ѵ�. */
            idlOS::snprintf(sSqlStr,
                            QD_MAX_SQL_LENGTH,
                            "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                            sReverseReplName );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        aFailbackFromNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DDL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            /* Local Receiver�� NewTrans�� ������ �Ѵ�. TxDDL�� �ϸ� Local Start ���� Lock ��� �߻�
             * Local Receiver�� Failback���� �����ѰŶ� ���� �����ص� �ٽ� �����ؼ��ϴϱ� ������. */
            IDE_TEST( dropReplicationForInternal( aStatement,
                                                  sdiZookeeper::mMyNodeName,
                                                  sReverseReplName )
                      != IDE_SUCCESS );

        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/* Before Lock�� FailoverCreateRP�� ������
 * Failback Node�� Sender�� Receiver RP�� Start �Ѵ�. */
IDE_RC qdsd::failbackRecoverRPbeforeLock( qcStatement        * aStatement,
                                          SChar              * aFailbackFromNodeName,
                                          sdiReplicaSetInfo  * aFailoverHistoryInfoWithOtherNode )
{
    UInt             i = 0;
    sdiReplicaSet  * sReplicaSet = NULL;
    SChar            sSqlStr[QD_MAX_SQL_LENGTH + 1];
    UInt              sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt              sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);

    /* Failback Start RP */
    for ( i = 0; i < aFailoverHistoryInfoWithOtherNode->mCount; i++ )
    {
        sReplicaSet = &aFailoverHistoryInfoWithOtherNode->mReplicaSets[i];

        /* SendRP Start */
        if ( idlOS::strncmp( sReplicaSet->mPrimaryNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                                 aFailbackFromNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                /* Failback Node���� Tx�� Failback �Ϸ� ������ ���� �Ŵϱ� lock ������ Start �Ѵ�. */
                /* Failback Node���� Send �ϴ� RP�� Failback �Ϸ���� ���� Send �ϸ� �ȴ�. */
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" RESET ",
                                 sReplicaSet->mFirstReplName );

                IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                                  sSqlStr )
                          != IDE_SUCCESS );

                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                 sReplicaSet->mFirstReplName );

                IDE_TEST( executeLocalSQL( aStatement, 
                                            QCI_STMT_MASK_DCL,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            NULL )
                            != IDE_SUCCESS );

                idlOS::snprintf(sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                sReplicaSet->mFirstReplName );

                (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                   sdiZookeeper::mMyNodeName,
                                                   ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                   QCI_STMT_MASK_MAX );

                ideLog::log( IDE_SD_0,
                             "[SHARD_FAILBACK] Failback Send Start RP : %s", 
                             sReplicaSet->mFirstReplName );

                if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                                     SDM_NA_STR,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
                {
                    /* Failover Create RP �̴�. beforeLock������ Start ���� �ʴ´�. */
                    /* Before Lock���� Flush �ѹ� ���ش�. */
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                     sReplicaSet->mSecondReplName,
                                     (sShardDDLLockTryCount * sRemoteLockTimeout) );

                    if( executeRemoteSQL( aStatement, 
                                          aFailbackFromNodeName,
                                          SDI_INTERNAL_OP_FAILBACK,
                                          QCI_STMT_MASK_DCL,
                                          sSqlStr,
                                          QD_MAX_SQL_LENGTH,
                                          NULL )
                        != IDE_SUCCESS )
                    {
                        /* Failback ���� Stop �� kill �Ǵ� ��Ȳ���� start �ȵǾ� �ִٸ�
                         * Start/Flush�� ������ �ش�. */
                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                         "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                         sReplicaSet->mSecondReplName );

                        IDE_TEST( executeRemoteSQL( aStatement, 
                                                    aFailbackFromNodeName,
                                                    SDI_INTERNAL_OP_FAILBACK,
                                                    QCI_STMT_MASK_DCL,
                                                    sSqlStr,
                                                    QD_MAX_SQL_LENGTH,
                                                    NULL )
                                  != IDE_SUCCESS );

                        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                         "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                         sReplicaSet->mSecondReplName,
                                         (sShardDDLLockTryCount * sRemoteLockTimeout));

                        IDE_TEST( executeRemoteSQL( aStatement, 
                                                    aFailbackFromNodeName,
                                                    SDI_INTERNAL_OP_FAILBACK,
                                                    QCI_STMT_MASK_DCL,
                                                    sSqlStr,
                                                    QD_MAX_SQL_LENGTH,
                                                    NULL )
                                  != IDE_SUCCESS );
                    }

                    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FailoverCreatedRP Flush Before Lock" );
                }
            }
            if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                                 aFailbackFromNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" RESET ",
                                 sReplicaSet->mSecondReplName );

                IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                                  sSqlStr )
                          != IDE_SUCCESS );

                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                 sReplicaSet->mSecondReplName );

                IDE_TEST( executeLocalSQL( aStatement, 
                                            QCI_STMT_MASK_DCL,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            NULL )
                            != IDE_SUCCESS );

                idlOS::snprintf(sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                sReplicaSet->mSecondReplName );

                (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                   sdiZookeeper::mMyNodeName,
                                                   ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                   QCI_STMT_MASK_MAX );

                ideLog::log( IDE_SD_0,
                             "[SHARD_FAILBACK] Failback Send Start RP : %s", 
                             sReplicaSet->mSecondReplName );
            }
        }

        /* Receive RP Start */
        if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            /* Failback_Failedover���� Receive RP�� Stop �Ǿ� �־�� �Ѵ�.
             * Failback ���� Start ��Ų�� Kill ������ Failback ��õ��� Start�Ǿ� ������ �ִ� */
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP ",
                             sReplicaSet->mFirstReplName );

            (void)executeRemoteSQL( aStatement, 
                                    sReplicaSet->mPrimaryNodeName,
                                    SDI_INTERNAL_OP_FAILBACK,
                                    QCI_STMT_MASK_DCL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    NULL );

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                             sReplicaSet->mFirstReplName );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            idlOS::snprintf(sSqlStr,
                            QD_MAX_SQL_LENGTH,
                            "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                            sReplicaSet->mFirstReplName );

            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               sReplicaSet->mPrimaryNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );

            ideLog::log( IDE_SD_0,
                         "[SHARD_FAILBACK] Failback Receive Start RP : %s", 
                         sReplicaSet->mFirstReplName );

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                             sReplicaSet->mFirstReplName,
                             (sShardDDLLockTryCount * sRemoteLockTimeout) );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            ideLog::log( IDE_SD_0,
                         "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                         sReplicaSet->mFirstReplName );


        }
        if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP ",
                             sReplicaSet->mSecondReplName );

            (void)executeRemoteSQL( aStatement, 
                                    sReplicaSet->mPrimaryNodeName,
                                    SDI_INTERNAL_OP_FAILBACK,
                                    QCI_STMT_MASK_DCL,
                                    sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    NULL );

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                             sReplicaSet->mSecondReplName );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            idlOS::snprintf(sSqlStr,
                            QD_MAX_SQL_LENGTH,
                            "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                            sReplicaSet->mSecondReplName );

            (void)sdiZookeeper::addPendingJob( sSqlStr,
                                               sReplicaSet->mPrimaryNodeName,
                                               ZK_PENDING_JOB_AFTER_ROLLBACK,
                                               QCI_STMT_MASK_MAX );

            ideLog::log( IDE_SD_0,
                         "[SHARD_FAILBACK] Failback Receive Start RP : %s", 
                         sReplicaSet->mSecondReplName );

            idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                             "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                             sReplicaSet->mSecondReplName,
                             (sShardDDLLockTryCount * sRemoteLockTimeout) );

            IDE_TEST( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                        != IDE_SUCCESS );

            ideLog::log( IDE_SD_0,
                         "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                         sReplicaSet->mSecondReplName );


        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Lock�� ������ϱ� ���� FailoverCreatedRP�� Stop �ϰ� Failback Node���� Start �ϵ����Ѵ�.
 * Failback Node�� Receiver�� RP�� ���� Start �Ѵ�. */
IDE_RC qdsd::failbackRecoverRPAfterLock( qcStatement        * aStatement,
                                         SChar              * aFailbackFromNodeName,
                                         sdiReplicaSet      * aMainReplicaSet,
                                         sdiReplicaSetInfo  * aFailoverHistoryInfoWithOtherNode )
{
    UInt             i = 0;
    sdiReplicaSet  * sReplicaSet = NULL;
    SChar            sSqlStr[QD_MAX_SQL_LENGTH + 1];
    SChar            sFailoverCreatedReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1] = {0,};
    UInt             sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt             sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);

    /* Failover Created RP Stop */
    /* N1�� Failover���� N1->N3�� N2->N3�� �߰��� RP ���� Stop/Drop �ؾ���. */
    if ( idlOS::strncmp( aMainReplicaSet->mPrimaryNodeName,
                         aFailbackFromNodeName,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
    {
        if ( idlOS::strncmp( aMainReplicaSet->mStopFirstBackupNodeName,
                             aFailbackFromNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( idlOS::strncmp( aMainReplicaSet->mSecondBackupNodeName,
                                 SDM_NA_STR,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
            {
                idlOS::strncpy( sFailoverCreatedReplName,
                                aMainReplicaSet->mSecondReplName,
                                SDI_REPLICATION_NAME_MAX_SIZE + 1 );

                /* Lock ȹ�� �� Flush �Ѵ�. */
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                 sFailoverCreatedReplName,
                                 (sShardDDLLockTryCount * sRemoteLockTimeout) );

                IDE_TEST( executeRemoteSQL( aStatement, 
                                            aFailbackFromNodeName,
                                            SDI_INTERNAL_OP_FAILBACK,
                                            QCI_STMT_MASK_DCL,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            NULL )
                            != IDE_SUCCESS );

                /* FailoverCreatedRP�� lock ȹ�� �� Stop �ؾ� �Ѵ�. */
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP ",
                                 sFailoverCreatedReplName,
                                 QCI_STMT_MASK_MAX );

                IDE_TEST( executeRemoteSQL( aStatement, 
                                            aFailbackFromNodeName,
                                            SDI_INTERNAL_OP_FAILBACK,
                                            QCI_STMT_MASK_DCL,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            NULL )
                            != IDE_SUCCESS );

                /* Rollback�� Local Start�� �Ǿ� ������ �ֱ� ������ local Stop ���� �ϰ�
                 * Remote start retry�� �ɾ�д�. */
                idlOS::snprintf(sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "alter replication "QCM_SQL_STRING_SKIP_FMT" stop ",
                                sFailoverCreatedReplName );

                (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                   sdiZookeeper::mMyNodeName,
                                                   ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                   QCI_STMT_MASK_MAX );

                idlOS::snprintf(sSqlStr,
                                QD_MAX_SQL_LENGTH,
                                "alter replication "QCM_SQL_STRING_SKIP_FMT" start retry",
                                sFailoverCreatedReplName );

                (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                   aFailbackFromNodeName,
                                                   ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                   QCI_STMT_MASK_DDL );

                ideLog::log( IDE_SD_0,
                             "[SHARD_FAILBACK] STOP FailoverCreatedRP : %s", 
                             sFailoverCreatedReplName );

                /* RP Drop�� TxDDL�� ó���Ѵ�. Sender �� ���� ��� �Ѵ�.
                 * AfterRollback�� ��� Start�̴�. */
                idlOS::snprintf( sSqlStr,
                                 QD_MAX_SQL_LENGTH,
                                 "drop replication "QCM_SQL_STRING_SKIP_FMT"",
                                 sFailoverCreatedReplName );

                IDE_TEST( executeRemoteSQL( aStatement, 
                                            aFailbackFromNodeName,
                                            SDI_INTERNAL_OP_FAILBACK,
                                            QCI_STMT_MASK_DDL,
                                            sSqlStr,
                                            QD_MAX_SQL_LENGTH,
                                            NULL )
                            != IDE_SUCCESS );

            }
        }
    }

    /* Failback Start RP */
    for ( i = 0; i < aFailoverHistoryInfoWithOtherNode->mCount; i++ )
    {
        sReplicaSet = &aFailoverHistoryInfoWithOtherNode->mReplicaSets[i];

        /* SendRP Start */
        if ( idlOS::strncmp( sReplicaSet->mPrimaryNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                                 aFailbackFromNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                                     SDM_NA_STR,
                                     SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
                {
                    /* FailoverCreatedRP���� �̹� �� ���������ǵ� �� ������ �ȵȴ�.
                     * Failback Node���� Send �ϴ� ���� ������ �߻��� Log�� ���ؼ��� ������ �ȴ�. */
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" RESET ",
                                     sReplicaSet->mSecondReplName );

                    IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                                      sSqlStr )
                              != IDE_SUCCESS );

                    /* Failover Created RP�� Stop ���Ŀ� Start �Ǿ�� �Ѵ�. */
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                     sReplicaSet->mSecondReplName );

                    IDE_TEST( executeLocalSQL( aStatement, 
                                               QCI_STMT_MASK_DCL,
                                               sSqlStr,
                                               QD_MAX_SQL_LENGTH,
                                               NULL )
                              != IDE_SUCCESS );

                    idlOS::snprintf(sSqlStr,
                                    QD_MAX_SQL_LENGTH,
                                    "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                    sReplicaSet->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sdiZookeeper::mMyNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Send Start RP : %s", 
                                 sReplicaSet->mSecondReplName );
                }
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::failbackJoin( qcStatement        * aStatement,
                           SChar              * aFailbackFromNodeName,
                           sdiFailbackState     aFailbackState )
{
    sdiReplicaSetInfo   sReplicaSetInfo;

    ULong               sOldSMN = 0;

    sdiClientInfo     * sClientInfo  = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    iduList           * sList;
    idBool              sIsAllocList = ID_FALSE;

    switch ( aFailbackState )
    {
        case SDI_FAILBACK_JOIN:
            break;
        case SDI_FAILBACK_JOIN_SMN_MODIFIED:
            /* Shard Meta Recovery From aFailbackFromNodeName
             * SMN Recovery */
            /***********************************************************************
             * ���� ��Ÿ ����
             ***********************************************************************/
            IDE_TEST( resetShardMeta( aStatement ) != IDE_SUCCESS);
            IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );
            IDE_TEST( getShardMetaFromOtherNode( aStatement,
                                                 aFailbackFromNodeName ) != IDE_SUCCESS );

            ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] SMN Update Complete");
            break;
        case SDI_FAILBACK_FAILEDOVER:
        case SDI_FAILBACK_NONE:
        default:
            /* �̷��� ���� �ȵ� */
            IDE_DASSERT( 0 );
            break;
    }

    sOldSMN = sdi::getSMNForDataNode();

    /* get Alive Node List + LocalNode �ʿ� */
    IDE_TEST( sdiZookeeper::getAliveNodeNameListIncludeNodeName( &sList,
                                                                 sdiZookeeper::mMyNodeName ) 
              != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    /* NodeList ������� ShardLinker �����ؾ� �Ѵ�. */
    IDE_TEST( sdi::checkShardLinkerWithNodeList ( aStatement,
                                                  sOldSMN,
                                                  NULL,
                                                  sList ) != IDE_SUCCESS );

    sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

    IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );

    /* get ReplicaSet Info */
    IDE_TEST( sdm::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sOldSMN )
              != IDE_SUCCESS );

    /* Failback�� �׻� SMN Update�� �߻��ؾ� �Ѵ�. */
    idlOS::snprintf( sSqlStr, 
                     QD_MAX_SQL_LENGTH,
                     "EXEC DBMS_SHARD.TOUCH_META(); " );

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_FAILBACK,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  NULL,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Touch Meta Complete");

    /* RP Recover */
    IDE_TEST( joinRecoverRP( aStatement,
                             &sReplicaSetInfo )
              != IDE_SUCCESS );

    sIsAllocList = ID_FALSE;
    sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsAllocList == ID_TRUE )
    {
        sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}


IDE_RC qdsd::failbackFailedover( qcStatement        * aStatement,
                                 SChar              * aFailbackFromNodeName,
                                 ULong                aFailbackSMN )
{

    SChar             * sUserName;
    SChar               sMyNextNodeName[SDI_NODE_NAME_MAX_SIZE + 1];

    sdiReplicaSetInfo   sFailoverHistoryInfo;
    sdiReplicaSetInfo   sFailoverHistoryInfoWithOtherNode;

    sdiReplicaSetInfo   sReplicaSetInfo;

    sdiReplicaSet     * sMainReplicaSet = NULL;
    sdiReplicaSet     * sReplicaSet = NULL;

    ULong               sOldSMN = 0;
    ULong               sNewSMN = 0;

    sdiClientInfo     * sClientInfo  = NULL;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];

    iduList           * sList;
    idBool              sIsAllocList = ID_FALSE;

    sdiTableInfoList  * sTableInfoList = NULL;

    sdiNodeInfo         sNodeInfo;
    sdiNode           * sLocalNodeInfo = NULL;

    UInt                i;
    UInt                sReplicaSetId = SDI_REPLICASET_NULL_ID;

    sdiLocalMetaInfo    sLocalMetaInfo;

    ZKState             sState = ZK_NONE;

    sUserName = QCG_GET_SESSION_USER_NAME( aStatement );

    /* Shard Meta Recovery From aFailbackFromNodeName
     * SMN Recovery */
    /***********************************************************************
     * ���� ��Ÿ ����
     ***********************************************************************/
    IDE_TEST( resetShardMeta( aStatement ) != IDE_SUCCESS);
    IDE_TEST( getAllNodeInfoList( aStatement ) != IDE_SUCCESS );
    IDE_TEST( getShardMetaFromOtherNode( aStatement,
                                         aFailbackFromNodeName ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] SMN Update Complete");


    /* Failback �������� ��� Shard Meta�� ��ȸ�� SMN Update�� �Ϸ�� ���Ŀ� ����Ǿ�� �Ѵ�.
     * Lock�� ������ SMN Update�� �����Ҽ� �ֱ� ���� */

    /* SetNode �����ʿ� ���� Ȯ��. */
    /* Shard Meta NodeInfo Check  */
    sOldSMN = sdi::getSMNForDataNode();

    IDE_TEST( sdi::getExternalNodeInfo( &sNodeInfo,
                                        sOldSMN )
              != IDE_SUCCESS );

    for ( i = 0; i < sNodeInfo.mCount; i++ )
    {
        if ( idlOS::strncmp( sNodeInfo.mNodes[i].mNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            sLocalNodeInfo = &sNodeInfo.mNodes[i];
        }
    }

    IDE_TEST( qci::mSessionCallback.mSetShardInternalLocalOperation( aStatement->session->mMmSession,
                                                                     SDI_INTERNAL_OP_FAILBACK ) 
              != IDE_SUCCESS );

    /* NodeInfo�� �ڽ��� ������ SetNode �ʿ��ϴ�. */
    if ( sLocalNodeInfo == NULL )
    {
        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Need Node Add");

        // LOCAL_META_INFO
        IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
                  != IDE_SUCCESS );

        IDE_TEST( executeLocalSQLWithSession( aStatement, 
                                              QCI_STMT_MASK_SP,
                                              sSqlStr,
                                              QD_MAX_SQL_LENGTH,
                               "EXEC DBMS_SHARD.SET_NODE( '"QCM_SQL_STRING_SKIP_FMT"', VARCHAR'%s', INTEGER'%"ID_INT32_FMT"',NULL, NULL, NULL, INTEGER'%"ID_INT32_FMT"' ); ",
                                              sLocalMetaInfo.mNodeName,
                                              sLocalMetaInfo.mHostIP,
                                              sLocalMetaInfo.mPortNo,
                                              sLocalMetaInfo.mShardNodeId )
                  != IDE_SUCCESS );

        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] LOCAL SET NODE COMPLETE");

        /* get SMN SetNode�� Meta�� ������ �۾� �ؾ��ϹǷ� IncreasedSMN�� �����´�. */
        IDE_TEST( getIncreasedSMN( aStatement, &sNewSMN ) != IDE_SUCCESS );

        /* get Alive Node List + LocalNode �ʿ� */
        IDE_TEST( sdiZookeeper::getAliveNodeNameListIncludeNodeName( &sList,
                                                                     sdiZookeeper::mMyNodeName ) 
                  != IDE_SUCCESS );
        sIsAllocList = ID_TRUE;

        /* NodeList ������� ShardLinker �����ؾ� �Ѵ�. */
        IDE_TEST( sdi::checkShardLinkerWithNodeList ( aStatement,
                                                      sNewSMN,
                                                      ( QC_SMI_STMT( aStatement ) )->getTrans(),
                                                      sList ) != IDE_SUCCESS );

        sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

        IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );

        /* SetNode �Ѱ� ����� ���� �Ͽ��� �Ѵ�. */
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "EXEC DBMS_SHARD.SET_NODE( VARCHAR'"QCM_SQL_STRING_SKIP_FMT"', VARCHAR'%s', INTEGER'%"ID_INT32_FMT"',NULL, NULL, NULL, INTEGER'%"ID_INT32_FMT"' ); ",
                         sLocalMetaInfo.mNodeName,
                         sLocalMetaInfo.mHostIP,
                         sLocalMetaInfo.mPortNo,
                         sLocalMetaInfo.mShardNodeId );

        /* Local Node�� SetNode�� ���� ���������Ƿ� Exception ���� �����Ѵ�. */
        IDE_TEST( executeSQLNodeList( aStatement,
                                      SDI_INTERNAL_OP_FAILBACK,
                                      QCI_STMT_MASK_SP,
                                      sSqlStr,
                                      sdiZookeeper::mMyNodeName,
                                      sList ) != IDE_SUCCESS );

        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] REMOTE SET NODE COMPLETE");

    }
    else
    {
        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Already Node Exist");

        /* get Alive Node List + LocalNode �ʿ� */
        IDE_TEST( sdiZookeeper::getAliveNodeNameListIncludeNodeName( &sList,
                                                                     sdiZookeeper::mMyNodeName ) 
                  != IDE_SUCCESS );
        sIsAllocList = ID_TRUE;

        /* NodeList ������� ShardLinker �����ؾ� �Ѵ�. */
        IDE_TEST( sdi::checkShardLinkerWithNodeList ( aStatement,
                                                      sOldSMN,
                                                      NULL,
                                                      sList ) != IDE_SUCCESS );

        sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

        IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );
    }

    /* Failback�� �׻� SMN Update�� �߻��ؾ� �Ѵ�. */
    idlOS::snprintf( sSqlStr, 
                     QD_MAX_SQL_LENGTH,
                     "EXEC DBMS_SHARD.TOUCH_META(); " );

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_FAILBACK,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  NULL,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILOVER_HISTORY CHECK");

    /* Check Failover_history_ */
    /* Get ReplicaSetInfo From FailoverHistory using sOldSMN  */
    /* get LocalNode's ReplicaSets Using sOldSMN */
    /* ��� ReplicaSet�� �������̿����� Ȯ��. �ִ� 3��( Original 1��+ K-Safety 2��) */
    IDE_TEST( sdm::getFailoverHistoryWithPrimaryNodename( QC_SMI_STMT( aStatement),
                                                          &sFailoverHistoryInfo,
                                                          sdiZookeeper::mMyNodeName,
                                                          aFailbackSMN ) != IDE_SUCCESS );

    /* Failover�� �����߾ SMN Update�� ������� �ʾ����� �ִ�. DataFailover�� ������� �ʾ������� �ִ�. */

    IDE_TEST( sdm::getFailoverHistoryWithSMN( QC_SMI_STMT( aStatement),
                                              &sFailoverHistoryInfoWithOtherNode,
                                              aFailbackSMN ) != IDE_SUCCESS );

    /* get ReplicaSet Info */
    IDE_TEST( sdm::getAllReplicaSetInfoSortedByPName( QC_SMI_STMT( aStatement ),
                                                      &sReplicaSetInfo,
                                                      sOldSMN )
              != IDE_SUCCESS );

    /* get All TableInfo */
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sOldSMN )
              != IDE_SUCCESS );

    /* Find Failback MainReplicaSet ID */
    IDE_TEST( sdiZookeeper::getNextNode( sdiZookeeper::mMyNodeName, 
                                         sMyNextNodeName, 
                                         &sState ) != IDE_SUCCESS );

    /* ReplicaSet �߿� Failback Node�� Main ReplicaSet�� ã�´�.
     * �ٸ� ��ü�� �������� �ʾ� ReplicaSet ���� �������� ���������, 
     * Clone Table�� �����Ǿ� �ֱ� ������ ������ ����Ǿ� �ִ� ReplicaSet�� ã�Ƽ� �Է����־�� �Ѵ�.
     * FailbackNode�� �������� ReplicaSet�� ����Ǿ� ������ �ֱ� ������ �� �� ������ Main�� ã�ƾ��Ѵ�. */
    for ( i = 0; i < sReplicaSetInfo.mCount; i++ )
    {
        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mFirstBackupNodeName,
                             sMyNextNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            sMainReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
            break;
        }

        if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mStopFirstBackupNodeName,
                             sMyNextNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            sMainReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
            break;
        }
    }

    if ( sMainReplicaSet != NULL )
    {
        sReplicaSetId = sMainReplicaSet->mReplicaSetId;
    }
    else
    {
        /* Failback�̴ϱ� First�� ������ Next ã�ƾ� �Ѵ�. ��ã���� Failover/Failback �� ���� ���� ����. */
        /* K-Safety �� 0 �� ����� ���� ����.
         * 0�̶�� DataFailover�� �Ͼ�� �ʱ� ������ ReplicaSet�� ����/UnsetNode ���� ���� ���� �ʾ���.
         * ���� Failback �Ϸ��� node ������ �״�� ���� ����. */
        for( i = 0 ; i< sReplicaSetInfo.mCount; i++ )
        {
            if ( idlOS::strncmp( sReplicaSetInfo.mReplicaSets[i].mPrimaryNodeName,
                                 sdiZookeeper::mMyNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                sMainReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
                sReplicaSetId = sMainReplicaSet->mReplicaSetId;
                break;
            }
        }
    }

    /* Replication Item Recover */
    /* �� ����ȭ ���� */
    /* �� ����ȭ�� DataFailover�� �߻��Ͽ��� ��쿡�� �ξ��� �ִ�. */
    /* Clone FullSync�� ���� ó���ؾ� �Ѵ�. */
    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RECOVER REPLICATION START");

    if ( sFailoverHistoryInfo.mCount != 0  )
    {
        IDE_TEST( failbackOrganizeReverseRP( aStatement,
                                             aFailbackFromNodeName,
                                             sTableInfoList,
                                             &sFailoverHistoryInfo,
                                             sReplicaSetId,
                                             sOldSMN ) != IDE_SUCCESS );

        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RECOVER REPLICATION START");
    }
    else
    {
        /* ReverseRP�� �ξ �ִٸ� Clone Table�� Sync �޾� ���� �������� ������ Clone FullSync�� �ʿ��ϴ�.
         * ReverseRP ���� ReplicaSet�� Failback �ؾ��ϴ� Node�� ReplicaSet������ ���� �ִٸ�
         * Clone Fullsync ����̴�. lock ��� �ѹ��� �ϸ� �ȴ�... �ٵ� �ʹ� ������ */
        for ( i = 0 ; i < sReplicaSetInfo.mCount ; i++ )
        {
            sReplicaSet = &sReplicaSetInfo.mReplicaSets[i];
            if ( idlOS::strncmp( sReplicaSet->mPrimaryNodeName,
                                 sdiZookeeper::mMyNodeName,
                                 SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
            {
                /* CLONE FULLSYNC*/
            }
        }
    }

    /* RP Stop/Start�� Lock ȹ�� �� �ϴϱ� �����ϳ� .. ��¾�� */
    IDE_TEST( failbackRecoverRPbeforeLock( aStatement,
                                           aFailbackFromNodeName,
                                           &sFailoverHistoryInfoWithOtherNode ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RESET SHARD META START");

    /* Shard Meta Update Using Old ReplicaSet's ID, All Table, Procedure */
    IDE_TEST( failbackResetShardMeta( aStatement,
                                      sdiZookeeper::mMyNodeName,
                                      aFailbackFromNodeName,
                                      sTableInfoList,
                                      &sReplicaSetInfo,
                                      &sFailoverHistoryInfo,
                                      sList,
                                      sReplicaSetId,
                                      sOldSMN ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RESET SHARD META COMPLETE");

    /* Data Failback = Swap Data Partition on aFailbackFromNodeName */
    IDE_TEST( lockTables4Failback( aStatement,
                                   sSqlStr,
                                   QD_MAX_SQL_LENGTH,
                                   aFailbackFromNodeName,
                                   sTableInfoList,
                                   &sReplicaSetInfo,
                                   &sFailoverHistoryInfo,
                                   sOldSMN ) != IDE_SUCCESS );
    
    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] TargetTable Lock Acquired");

    IDE_TEST( failbackOrganizeReverseRPAfterLock( aStatement,
                                                  aFailbackFromNodeName,
                                                  &sFailoverHistoryInfo ) 
              != IDE_SUCCESS );

    IDE_TEST( failbackRecoverRPAfterLock( aStatement,
                                          aFailbackFromNodeName,
                                          sMainReplicaSet,
                                          &sFailoverHistoryInfoWithOtherNode ) != IDE_SUCCESS );

    IDE_TEST( failbackTablePartitionSwap( aStatement,
                                          aFailbackFromNodeName,
                                          sTableInfoList,
                                          &sReplicaSetInfo,
                                          &sFailoverHistoryInfo,
                                          sOldSMN ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Partition Swap Complete");

    /* Reset ReplicaSet */
    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RESET REPLICASET START");

    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                     "exec dbms_shard.reset_ReplicaSets( '"QCM_SQL_STRING_SKIP_FMT"', '"
                     QCM_SQL_STRING_SKIP_FMT"','"
                     QCM_SQL_STRING_SKIP_FMT"' )",
                     sUserName,
                     aFailbackFromNodeName,       /* Failback �̴ϱ� OldNode�� FailbackFromNode�̴�.    */
                     sdiZookeeper::mMyNodeName ); /* Failback �̴ϱ� NewNode�� Failback ���� Node �̴�. */

    IDE_TEST( executeSQLNodeList( aStatement,
                                  SDI_INTERNAL_OP_FAILBACK,
                                  QCI_STMT_MASK_SP,
                                  sSqlStr,
                                  NULL,
                                  sList ) != IDE_SUCCESS );

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] RESET REPLICASET COMPLETE");

    /* deleteFailoverHistoryWithSMN */
    if ( sFailoverHistoryInfoWithOtherNode.mCount != 0 )
    { 
        idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                         "DELETE FROM SYS_SHARD.FAILOVER_HISTORY_ WHERE SMN = "QCM_SQL_BIGINT_FMT, aFailbackSMN );

        IDE_TEST( executeSQLNodeList( aStatement,
                                      SDI_INTERNAL_OP_FAILBACK,
                                      QCI_STMT_MASK_SP,
                                      sSqlStr,
                                      NULL,
                                      sList ) != IDE_SUCCESS );
    }

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] REMOVE FAILOVERHISTORY COMPLETE");

    sIsAllocList = ID_FALSE;
    sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsAllocList == ID_TRUE )
    {
        sdiZookeeper::freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}


IDE_RC qdsd::joinRecoverRP( qcStatement        * aStatement,
                            sdiReplicaSetInfo  * aReplicaSetInfo )
{
    UInt             i = 0;
    sdiReplicaSet  * sReplicaSet = NULL;
    SChar            sSqlStr[QD_MAX_SQL_LENGTH + 1];
    idBool           sIsAlive = ID_FALSE;
    UInt             sShardDDLLockTryCount = QCG_GET_SESSION_SHARD_DDL_LOCK_TRY_COUNT(aStatement);
    UInt             sRemoteLockTimeout = QCG_GET_SESSION_SHARD_DDL_LOCK_TIMEOUT(aStatement);

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Failback Recover RP Start");

    /* FailbackNode Send RP Start */
    for ( i = 0; i < aReplicaSetInfo->mCount; i++ )
    {
        sReplicaSet = &aReplicaSetInfo->mReplicaSets[i];

        /* SendRP Start */
        if ( idlOS::strncmp( sReplicaSet->mPrimaryNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            if ( SDI_IS_NULL_NAME( sReplicaSet->mFirstBackupNodeName ) == ID_FALSE )
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                 sReplicaSet->mFirstReplName );

                if ( executeLocalSQL( aStatement, 
                                      QCI_STMT_MASK_DCL,
                                      sSqlStr,
                                      QD_MAX_SQL_LENGTH,
                                      NULL )
                     == IDE_SUCCESS )
                {
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                     sReplicaSet->mFirstReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sdiZookeeper::mMyNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_21,
                                 "[SHARD_FAILBACK] Replication Started : %s", 
                                 sReplicaSet->mFirstReplName );

                    /* Flush ȣ�� */
                    IDE_TEST( flushReplicationForInternal( aStatement,
                                                           sdiZookeeper::mMyNodeName,
                                                           sReplicaSet->mFirstReplName )
                              != IDE_SUCCESS );
                }
                else
                {

                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START RETRY ",
                                     sReplicaSet->mFirstReplName );

                    IDE_TEST( executeLocalSQL( aStatement, 
                                               QCI_STMT_MASK_DCL,
                                               sSqlStr,
                                               QD_MAX_SQL_LENGTH,
                                               NULL )
                                != IDE_SUCCESS );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                     sReplicaSet->mFirstReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sdiZookeeper::mMyNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_21,
                                 "[SHARD_FAILBACK] Replication Start Retry : %s", 
                                 sReplicaSet->mFirstReplName );
                }
            }
            if ( SDI_IS_NULL_NAME( sReplicaSet->mSecondBackupNodeName ) == ID_FALSE )
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                 sReplicaSet->mSecondReplName );

                if ( executeLocalSQL( aStatement, 
                                      QCI_STMT_MASK_DCL,
                                      sSqlStr,
                                      QD_MAX_SQL_LENGTH,
                                      NULL )
                     == IDE_SUCCESS )
                {
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                     sReplicaSet->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sdiZookeeper::mMyNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_21,
                                 "[SHARD_FAILBACK] Replication Started : %s", 
                                 sReplicaSet->mSecondReplName );

                    /* Flush ȣ�� */
                    IDE_TEST( flushReplicationForInternal( aStatement,
                                                           sdiZookeeper::mMyNodeName,
                                                           sReplicaSet->mSecondReplName )
                              != IDE_SUCCESS );
                }
                else
                {

                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START RETRY ",
                                     sReplicaSet->mSecondReplName );

                    IDE_TEST( executeLocalSQL( aStatement, 
                                               QCI_STMT_MASK_DCL,
                                               sSqlStr,
                                               QD_MAX_SQL_LENGTH,
                                               NULL )
                                != IDE_SUCCESS );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" stop",
                                     sReplicaSet->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sdiZookeeper::mMyNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_21,
                                 "[SHARD_FAILBACK] Replication Start Retry : %s", 
                                 sReplicaSet->mSecondReplName );
                }
            }
        }
    }

    /* Receive RP Start */
    for ( i = 0; i < aReplicaSetInfo->mCount; i++ )
    {
        sReplicaSet = &aReplicaSetInfo->mReplicaSets[i];
        sIsAlive    = ID_FALSE;

        /* Receive RP Start */
        if ( idlOS::strncmp( sReplicaSet->mFirstBackupNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            IDE_TEST ( sdiZookeeper::checkNodeAlive( sReplicaSet->mPrimaryNodeName,
                                                     &sIsAlive ) != IDE_SUCCESS );

            if ( sIsAlive == ID_TRUE )
            {
                /* Join ��Ȳ���� Failback Node�� Receive RP�� Sender�� �̹� Start retry �� �ϼ� �ִ�.
                 * Flush �� �����ϸ� Start �Ȱɷ� �Ǵ��Ѵ�. */
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                 sReplicaSet->mFirstReplName,
                                 (sShardDDLLockTryCount * sRemoteLockTimeout) );

                if ( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL ) == IDE_SUCCESS )
                {
                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                                 sReplicaSet->mFirstReplName );
                }
                else
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                     sReplicaSet->mFirstReplName );

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                sReplicaSet->mPrimaryNodeName,
                                                SDI_INTERNAL_OP_FAILBACK,
                                                QCI_STMT_MASK_DCL,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                                != IDE_SUCCESS );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" start retry",
                                     sReplicaSet->mFirstReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sReplicaSet->mPrimaryNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );


                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive Start RP : %s", 
                                 sReplicaSet->mFirstReplName );

                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                     sReplicaSet->mFirstReplName,
                                     (sShardDDLLockTryCount * sRemoteLockTimeout) );

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                sReplicaSet->mPrimaryNodeName,
                                                SDI_INTERNAL_OP_FAILBACK,
                                                QCI_STMT_MASK_DCL,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                                != IDE_SUCCESS );

                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                                 sReplicaSet->mFirstReplName );
                }
            }
        }
        if ( idlOS::strncmp( sReplicaSet->mSecondBackupNodeName,
                             sdiZookeeper::mMyNodeName,
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            IDE_TEST ( sdiZookeeper::checkNodeAlive( sReplicaSet->mPrimaryNodeName,
                                                     &sIsAlive ) != IDE_SUCCESS );

            if ( sIsAlive == ID_TRUE )
            {
                idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                 "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                 sReplicaSet->mSecondReplName,
                                 (sShardDDLLockTryCount * sRemoteLockTimeout) );

                if ( executeRemoteSQL( aStatement, 
                                        sReplicaSet->mPrimaryNodeName,
                                        SDI_INTERNAL_OP_FAILBACK,
                                        QCI_STMT_MASK_DCL,
                                        sSqlStr,
                                        QD_MAX_SQL_LENGTH,
                                        NULL )
                     == IDE_SUCCESS )
                {
                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                                 sReplicaSet->mSecondReplName );
                }
                else
                {

                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" START ",
                                     sReplicaSet->mSecondReplName );

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                sReplicaSet->mPrimaryNodeName,
                                                SDI_INTERNAL_OP_FAILBACK,
                                                QCI_STMT_MASK_DCL,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                                != IDE_SUCCESS );

                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter replication "QCM_SQL_STRING_SKIP_FMT" start retry",
                                     sReplicaSet->mSecondReplName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sReplicaSet->mPrimaryNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );

                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive Start RP : %s", 
                                 sReplicaSet->mSecondReplName );

                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" FLUSH WAIT %"ID_UINT32_FMT,
                                     sReplicaSet->mSecondReplName,
                                     (sShardDDLLockTryCount * sRemoteLockTimeout));

                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                sReplicaSet->mPrimaryNodeName,
                                                SDI_INTERNAL_OP_FAILBACK,
                                                QCI_STMT_MASK_DCL,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                                != IDE_SUCCESS );

                    ideLog::log( IDE_SD_0,
                                 "[SHARD_FAILBACK] Failback Receive FLUSH RP : %s", 
                                 sReplicaSet->mSecondReplName );
                }
            }
        }
    }

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Failback Recover RP Complete");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeAlterShardFlag( qcStatement        * aStatement )
{
/***********************************************************************
 * TASK-7307 DML Data Consistency in Shard
 *   ��� ���� ���̺� ���ؼ� usable, shard_flag �� �ʱ�ȭ ����
 ***********************************************************************/
    sdiTableInfoList  * sTmpTableInfoList = NULL;    
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfo      * sTableInfo  = NULL;
    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    ULong               sSMN = SDI_NULL_SMN;

    // LOCAL_META_INFO
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
              != IDE_SUCCESS );

    sSMN = sdi::getSMNForDataNode();

    /* get All TableInfo */
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sSMN )
              != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* check Table Type */
        sShardTableType = sdi::getShardObjectType( sTableInfo );

        if ( sTableInfo->mObjectType == 'T' ) /* Table */
        {
            switch ( sShardTableType )
            {
                case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
                    IDE_TEST( executeLocalSQL( aStatement, 
                                               QCI_STMT_MASK_DDL,
                                               sSqlStr,
                                               QD_MAX_SQL_LENGTH,
                                               "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard split",
                                               sTableInfo->mUserName,
                                               sTableInfo->mObjectName )
                              != IDE_SUCCESS );
                    break;
                case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                    continue;
                case SDI_SOLO_DIST_OBJECT:
                    IDE_TEST( executeLocalSQL( aStatement, 
                                               QCI_STMT_MASK_DDL,
                                               sSqlStr,
                                               QD_MAX_SQL_LENGTH,
                                               "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard solo",
                                               sTableInfo->mUserName,
                                               sTableInfo->mObjectName )
                              != IDE_SUCCESS );
                    break;
                case SDI_CLONE_DIST_OBJECT:
                    idlOS::snprintf( sSqlStr,
                                     QD_MAX_SQL_LENGTH,
                                     "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard clone",
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName );

                    IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                                      sSqlStr )
                              != IDE_SUCCESS );

                    // ���� �� revert ó��
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard none",
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName );

                    (void)sdiZookeeper::addPendingJob( sSqlStr,
                                                       sLocalMetaInfo.mNodeName,
                                                       ZK_PENDING_JOB_AFTER_ROLLBACK,
                                                       QCI_STMT_MASK_DDL );
                    break;
                case SDI_NON_SHARD_OBJECT:
                default:
                    /* Non Shard Table�� ���� �ȵ�. */
                    IDE_DASSERT( 0 );
                    break;
            }

            /* Default Partition check */
            if ( ( sTableInfo->mDefaultNodeId == sLocalMetaInfo.mShardNodeId ) &&
                 ( sShardTableType == SDI_SINGLE_SHARD_KEY_DIST_OBJECT ) )
            {
                IDE_TEST( executeLocalSQL( aStatement, 
                                           QCI_STMT_MASK_DDL,
                                           sSqlStr,
                                           QD_MAX_SQL_LENGTH,
                                           "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" access partition "QCM_SQL_STRING_SKIP_FMT" usable",
                                           sTableInfo->mUserName,
                                           sTableInfo->mObjectName,
                                           sTableInfo->mDefaultPartitionName )
                      != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeAlterShardNone( qcStatement        * aStatement )
{
/***********************************************************************
 * TASK-7307 DML Data Consistency in Shard
 *   ��� ���� ���̺� ���ؼ� usable, shard_flag �� non-shard �ʱ�ȭ 
 ***********************************************************************/
    sdiTableInfoList  * sTmpTableInfoList = NULL;    
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfo      * sTableInfo  = NULL;
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    ULong               sSMN = SDI_NULL_SMN;

    // LOCAL_META_INFO
    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) 
              != IDE_SUCCESS );

    sSMN = sdi::getSMNForDataNode();

    /* get All TableInfo */
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sSMN )
              != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )

    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* check Table Type */
        if ( sTableInfo->mObjectType == 'T' ) /* Table */
        {
            IDE_TEST( executeLocalSQL( aStatement, 
                                       QCI_STMT_MASK_DDL,
                                       sSqlStr,
                                       QD_MAX_SQL_LENGTH,
                                       "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard none",
                                       sTableInfo->mUserName,
                                       sTableInfo->mObjectName )
              != IDE_SUCCESS );

#if 0
            idlOS::snprintf( sBufferNameStr,
                             QC_MAX_OBJECT_NAME_LEN + 1,
                             "%s%s",
                             SDI_BACKUP_TABLE_PREFIX,
                             sTableInfo->mObjectName );

            if ( ( SDU_SHARD_LOCAL_FORCE != 1 ) &&
                 ( sLocalMetaInfo.mKSafety > 1 ) &&
                 ( sdi::getShardObjectType( sTableInfo ) != SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT ) )
            {
                IDE_TEST( executeLocalSQL( aStatement, 
                                           QCI_STMT_MASK_DDL,
                                           sSqlStr,
                                           QD_MAX_SQL_LENGTH,
                                           "alter table "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" shard none",
                                           sTableInfo->mUserName,
                                           sBufferNameStr )
                          != IDE_SUCCESS );
            }
            else
            {
                /* Nothing to do */
            }
#endif
        }
        else
        {
            /* Nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::executeShardAddForClone( qcStatement * aStatement )
{
    qdShardParseTree   * sParseTree = NULL;
    sdiTableInfoList   * sTableInfoList = NULL;
    sdiTableInfoList   * sTmpTableInfoList = NULL;
    sdiTableInfo       * sTableInfo  = NULL;
    SChar              * sSqlStr = NULL;
    const SChar        * sBackupTableQuery = NULL;
    qdReShardAttribute * sReshardAttrbute = NULL;
    UInt                 sLen = 0;
    UInt                 sErrCode = 0;
    sdiLocalMetaInfo     sLocalMetaInfo;
    sdiGlobalMetaInfo    sMetaNodeInfo = { ID_ULONG(0) };
    SChar                sBufferNameStr[QC_MAX_OBJECT_NAME_LEN + 1];
        
    sParseTree = (qdShardParseTree *)aStatement->myPlan->parseTree;

    IDE_TEST( sdi::getGlobalMetaInfoCore( QC_SMI_STMT( aStatement ),
                                          &sMetaNodeInfo )
              != IDE_SUCCESS );

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );
    
    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sMetaNodeInfo.mShardMetaNumber )
              != IDE_SUCCESS );
    
    IDE_TEST( qci::mSessionCallback.mSetShardInternalLocalOperation(
                  aStatement->session->mMmSession,
                  SDI_INTERNAL_OP_NORMAL ) 
              != IDE_SUCCESS );
    
    sParseTree->mNodeName.stmtText = sLocalMetaInfo.mNodeName;
    sParseTree->mNodeName.offset   = 0;
    sParseTree->mNodeName.size     = idlOS::strlen( sLocalMetaInfo.mNodeName );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )
    {
        sTableInfo = sTmpTableInfoList->mTableInfo;
        
        switch ( sdi::getShardObjectType( sTableInfo ))
        {
            case SDI_NON_SHARD_OBJECT:                   
            case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:                   
            case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
            case SDI_SOLO_DIST_OBJECT:
                // nothing to do
                break;                    
            case SDI_CLONE_DIST_OBJECT:
                IDE_TEST( QC_QMX_MEM(aStatement)->alloc( ID_SIZEOF(qdReShardAttribute),
                                                         (void**) &sReshardAttrbute )
                          != IDE_SUCCESS );

                QD_SET_INIT_RESHARD_ATTR( sReshardAttrbute );
        
                sReshardAttrbute->mObjectType = sTableInfo->mObjectType;

                sReshardAttrbute->mUserName.stmtText = sTableInfo->mUserName;
                sReshardAttrbute->mUserName.offset   = 0;
                sReshardAttrbute->mUserName.size     = idlOS::strlen( sTableInfo->mUserName );

                sReshardAttrbute->mObjectName.stmtText = sTableInfo->mObjectName;
                sReshardAttrbute->mObjectName.offset   = 0;
                sReshardAttrbute->mObjectName.size     = idlOS::strlen( sTableInfo->mObjectName );

                if ( sTableInfo->mObjectType == 'T' )
                {
                    sBackupTableQuery  = "CREATE TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" FROM TABLE SCHEMA "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" USING PREFIX "QCM_SQL_STRING_SKIP_FMT"";
                        
                    sLen = ( ID_SIZEOF(sTableInfo->mUserName) * 2 ) +
                        ( ID_SIZEOF(sTableInfo->mObjectName) * 2 ) +
                        ( idlOS::strlen(SDI_BACKUP_TABLE_PREFIX) * 2 ) + 1;

                    IDE_TEST( STRUCT_ALLOC_WITH_SIZE( aStatement->qmxMem,
                                                      SChar,
                                                      sLen,
                                                      &sSqlStr )
                              != IDE_SUCCESS );

                    idlOS::snprintf( sBufferNameStr,
                                     QC_MAX_OBJECT_NAME_LEN + 1,
                                     "%s%s",
                                     SDI_BACKUP_TABLE_PREFIX,
                                     sTableInfo->mObjectName );
                    
                    // bak table create
                    idlOS::snprintf( sSqlStr,
                                     sLen,
                                     "DROP TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT" CASCADE",
                                     sTableInfo->mUserName,
                                     sBufferNameStr );
                    
                    if ( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement, sSqlStr ))
                    {
                        sErrCode = ideGetErrorCode();

                        if ( sErrCode == qpERR_ABORT_QCV_NOT_EXISTS_TABLE )
                        {
                            IDE_CLEAR();
                        }
                        else
                        {
                            IDE_RAISE( ERR_ALREDAY_EXIST_ERROR_MSG );
                        }
                    }

                    // bak table create
                    idlOS::snprintf( sSqlStr,
                                     sLen,
                                     sBackupTableQuery,
                                     sTableInfo->mUserName,
                                     sBufferNameStr,
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     SDI_BACKUP_TABLE_PREFIX );

                    IDE_TEST( sdi::shardExecTempDMLOrDCLWithNewTrans( aStatement,
                                                                      sSqlStr )
                              != IDE_SUCCESS );
                }
                else
                {
                    // nothing to do
                }

                sReshardAttrbute->next = NULL;            

                if( sParseTree->mReShardAttr == NULL )
                {
                    sParseTree->mReShardAttr = sReshardAttrbute;
                }
                else
                {
                    sReshardAttrbute->next = sParseTree->mReShardAttr;
                    sParseTree->mReShardAttr = sReshardAttrbute;
                }
                break;                                        
            default:
                IDE_DASSERT(0);
                break;
        }
    }
        
    if ( sParseTree->mReShardAttr != NULL )
    {
        aStatement->session->mQPSpecific.mFlag &= ~QC_SESSION_SAHRD_ADD_CLONE_MASK;
        aStatement->session->mQPSpecific.mFlag |= QC_SESSION_SAHRD_ADD_CLONE_TRUE;

        // shard move�� ����Ϸ��� SHARD_STATUS 1 �̿��� �Ѵ�.
        sdi::setShardStatus(1); // 0->1

        IDE_TEST( validateShardMove( aStatement ) != IDE_SUCCESS );
        IDE_TEST( executeShardMove( aStatement ) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALREDAY_EXIST_ERROR_MSG )
    {
        // nothing to do
    }
    IDE_EXCEPTION_END;

    sdi::setShardStatus(0);
    
    return IDE_FAILURE;
}

IDE_RC qdsd::checkCloneTableDataAndTruncateForShardAdd( qcStatement        * aStatement,
                                                        qdReShardAttribute * aReShardAttr )
{
    SChar                sUserName[QC_MAX_OBJECT_NAME_LEN] = {0,};
    SChar                sTableName[QC_MAX_OBJECT_NAME_LEN] = SDI_BACKUP_TABLE_PREFIX;
    idBool               sRecordExist = ID_FALSE;
    mtdBigintType        sResult;
    ULong                sTotalDataCount = 0;
    qdReShardAttribute * sReShardAttr = NULL;     
    const SChar          sRowCntSQLFmt[] = "SELECT COUNT(*) FROM "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"";
    const SChar          sTruncateSQLFmt[] = "TRUNCATE TABLE "QCM_SQL_STRING_SKIP_FMT"."QCM_SQL_STRING_SKIP_FMT"";
    SChar sSqlStr[ ( QC_MAX_OBJECT_NAME_LEN * 2 ) + 
                   IDL_MAX(ID_SIZEOF(sRowCntSQLFmt), ID_SIZEOF(sTruncateSQLFmt)) ] = {0,};

    for ( sReShardAttr = aReShardAttr;
          sReShardAttr != NULL;
          sReShardAttr = sReShardAttr->next )
    {
        IDE_DASSERT( QC_IS_NULL_NAME( sReShardAttr->mUserName ) != ID_TRUE );
        IDE_DASSERT( sReShardAttr->mShardObjectInfo != NULL );
        
        if ( sReShardAttr->mObjectType == 'T' )
        {
            QC_STR_COPY( sUserName, sReShardAttr->mUserName );
            QC_STR_COPY( sTableName, sReShardAttr->mObjectName );
            
            idlOS::snprintf( sSqlStr,
                             ID_SIZEOF(sSqlStr),
                             sRowCntSQLFmt,
                             sUserName,
                             sTableName );

            IDE_TEST( qciMisc::runSelectOneRowforDDL( QC_SMI_STMT( aStatement ),
                                                      sSqlStr,
                                                      &sResult,
                                                      &sRecordExist,
                                                      ID_FALSE )
                      != IDE_SUCCESS );
    
            IDE_TEST_RAISE( sRecordExist == ID_FALSE,
                            ERR_NO_DATA_FOUND );

            sTotalDataCount = (UInt)sResult;

            if ( sTotalDataCount > 0 )
            {
                idlOS::snprintf( sSqlStr,
                                 ID_SIZEOF(sSqlStr),
                                 sTruncateSQLFmt,
                                 sUserName,
                                 sTableName );
                
                IDE_TEST( qciMisc::runDDLforInternalWithMmSession(
                              aStatement->mStatistics,
                              aStatement->session->mMmSession,
                              QC_SMI_STMT( aStatement ),
                              QC_EMPTY_USER_ID,
                              QCI_SESSION_INTERNAL_DDL_TRUE,
                              sSqlStr )
                          != IDE_SUCCESS );
            }
            else
            {
                // nothing to do
            }
        }
        else
        {
            /* 'P' procedure: do nothing */
        }
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NO_DATA_FOUND )
    {
        IDE_SET(ideSetErrorCode( qpERR_ABORT_QSX_NO_DATA_FOUND ));
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

IDE_RC qdsd::executeResetCloneMeta( qcStatement * aStatement )
{
    sdiTableInfoList  * sTableInfoList = NULL;
    sdiTableInfoList  * sTmpTableInfoList = NULL;
    sdiTableInfo      * sTableInfo  = NULL;
    sdiClientInfo     * sClientInfo  = NULL;
    sdiConnectInfo    * sConnectInfo = NULL;
    UInt                i = 0;
    ULong               sSMN = SDI_NULL_SMN;
    sdiShardObjectType  sShardTableType = SDI_NON_SHARD_OBJECT;
    sdiLocalMetaInfo    sLocalMetaInfo;
    SChar               sSqlStr[QD_MAX_SQL_LENGTH + 1];
    
    sSMN = sdi::getSMNForDataNode();

    IDE_TEST( sdi::getTableInfoAllObject( aStatement,
                                          &sTableInfoList,
                                          sSMN )
              != IDE_SUCCESS );

    IDE_TEST( sdi::getLocalMetaInfo( &sLocalMetaInfo ) != IDE_SUCCESS );

    IDE_TEST( sdi::checkShardLinker( aStatement ) != IDE_SUCCESS );

    for ( sTmpTableInfoList = sTableInfoList;
          sTmpTableInfoList != NULL;
          sTmpTableInfoList = sTmpTableInfoList->mNext )
    {
        sTableInfo = sTmpTableInfoList->mTableInfo;

        /* check Table Type */
        sShardTableType = sdi::getShardObjectType( sTableInfo );

        switch ( sShardTableType )
        {
            case SDI_SINGLE_SHARD_KEY_DIST_OBJECT:
            case SDI_SOLO_DIST_OBJECT:
            case SDI_COMPOSITE_SHARD_KEY_DIST_OBJECT:
                break;
            case SDI_CLONE_DIST_OBJECT:
                /* Clones_ ���� ���� �� �־�� �Ѵ�. */
                if ( sTableInfo->mObjectType == 'T' ) /* Table */
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "exec dbms_shard.reset_shard_partition_node('"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', "
                                     "NULL ) ",
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     sLocalMetaInfo.mNodeName );
                }
                else /* Procedure */
                {
                    idlOS::snprintf( sSqlStr, QD_MAX_SQL_LENGTH,
                                     "exec dbms_shard.reset_shard_resident_node('"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', '"
                                     QCM_SQL_STRING_SKIP_FMT"', "
                                     "NULL ) ",                // clone not exist value
                                     sTableInfo->mUserName,
                                     sTableInfo->mObjectName,
                                     sLocalMetaInfo.mNodeName );
                }

                sClientInfo = aStatement->session->mQPSpecific.mClientInfo;

                IDE_TEST_RAISE( sClientInfo == NULL, ERR_NODE_NOT_EXIST );

                sConnectInfo = sClientInfo->mConnectInfo;

                for ( i = 0; i < sClientInfo->mCount; i++, sConnectInfo++ )
                {
                    IDE_TEST( executeRemoteSQL( aStatement, 
                                                sConnectInfo->mNodeName,
                                                SDI_INTERNAL_OP_NORMAL,
                                                QCI_STMT_MASK_SP,
                                                sSqlStr,
                                                QD_MAX_SQL_LENGTH,
                                                NULL )
                              != IDE_SUCCESS );
                }
                break;
            case SDI_NON_SHARD_OBJECT:
            default:
                /* Non Shard Table�� ���� �ȵ�. */
                IDE_DASSERT( 0 );
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDM_SHARD_NODE_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC qdsd::checkShardPIN( qcStatement * aStatement )
{
    sdiShardPin sShardPin = qci::mSessionCallback.mGetShardPIN( aStatement->session->mMmSession );

    /* shard PIN�� üũ�Ѵ�. */
    if( sShardPin == SDI_SHARD_PIN_INVALID )
    {
        /* SMN�� reload�ϰ� shard PIN�� �����Ѵ�.  */
        IDE_TEST( qci::mSessionCallback.mReloadShardMetaNumber(
                aStatement->session->mMmSession,
                ID_TRUE )
            != IDE_SUCCESS );

        /* shard PIN�� �����Ѵ�. */
        qci::mSessionCallback.mSetNewShardPIN( aStatement->session->mMmSession );
    }
    else
    {
        /* �̹� shard PIN�� ���õǾ� �ִٸ� ���� �۾��� �ʿ䰡 ����. */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
