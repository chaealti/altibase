/**
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _O_SDI_ZOOKEEPER_H_
#define _O_SDI_ZOOKEEPER_H_ 1

#include <aclZookeeper.h>
#include <idTypes.h>
#include <idp.h>
#include <idl.h>
#include <idu.h>
#include <sdi.h>
#include <sdm.h>

/* Zookeeper C Client�� �������� �����ϹǷ� ������ ������ ���������� sdiZookeeper.cpp�� ������ ������ *
 * ������ �� OS������ ���¸� �ִ� ����ִ� ����(sdiZookeeperstub.cpp)�� ��� compile�Ѵ�.             *
 * �׷��Ƿ� sdiZookeeper ������ �Լ�/������ �߰�/����/������ ��쿡�� sdiZookeeper.cpp�� �ƴ϶�       *
 * sdiZookeeperstub.cpp�� ���������� �������־���� ������ �� OS������ ���������� ������ �� �� �ִ�.  */

#define SDI_ZKC_PATH_LENGTH     (128)
#define SDI_ZKC_BUFFER_SIZE     (128)
#define SDI_ZKC_SHARD_ID_SIZE   (10)
#define SDI_ZKC_ZKS_MIN_CNT     (3)
#define SDI_ZKC_ZKS_MAX_CNT     (7)
#define SDI_ZKC_ALLOC_WAIT_TIME (1000*1000*60*30)

/* meta path */
#define SDI_ZKC_PATH_ALTIBASE_SHARD     "/altibase_shard"
#define SDI_ZKC_PATH_META_LOCK          "/altibase_shard/cluster_meta/zookeeper_meta_lock"
#define SDI_ZKC_PATH_SHARD_META_LOCK    "/altibase_shard/cluster_meta/shard_meta_lock"
#define SDI_ZKC_PATH_NODE_META          "/altibase_shard/node_meta"
#define SDI_ZKC_PATH_CONNECTION_INFO    "/altibase_shard/connection_info"
#define SDI_ZKC_PATH_DISPLAY_LOCK       "/locked"

/* cluster meta path */
#define SDI_ZKC_PATH_CLUSTER_META       "/altibase_shard/cluster_meta"
#define SDI_ZKC_PATH_FAILOVER_HISTORY   "/altibase_shard/cluster_meta/failover_history"
#define SDI_ZKC_PATH_SMN                "/altibase_shard/cluster_meta/SMN"
#define SDI_ZKC_PATH_FAULT_DETECT_TIME  "/altibase_shard/cluster_meta/fault_detection_time"

/* validation path */
#define SDI_ZKC_PATH_VALIDATION         "/altibase_shard/cluster_meta/validation"
#define SDI_ZKC_PATH_DB_NAME            "/altibase_shard/cluster_meta/validation/sharded_database_name"
#define SDI_ZKC_PATH_CHAR_SET           "/altibase_shard/cluster_meta/validation/character_set"
#define SDI_ZKC_PATH_NAT_CHAR_SET       "/altibase_shard/cluster_meta/validation/national_character_set"
#define SDI_ZKC_PATH_K_SAFETY           "/altibase_shard/cluster_meta/validation/k-safety"
#define SDI_ZKC_PATH_REP_MODE           "/altibase_shard/cluster_meta/validation/replication_mode"
#define SDI_ZKC_PATH_BINARY_VERSION     "/altibase_shard/cluster_meta/validation/binary_version"
#define SDI_ZKC_PATH_SHARD_VERSION      "/altibase_shard/cluster_meta/validation/shard_version"
#define SDI_ZKC_PATH_PARALLEL_CNT       "/altibase_shard/cluster_meta/validation/parallel_count"
#define SDI_ZKC_PATH_TRANS_TBL_SIZE     "/altibase_shard/cluster_meta/validation/trans_TBL_size"

/* node meta sub path */
#define SDI_ZKC_PATH_NODE_ID            "/shard_node_id"
#define SDI_ZKC_PATH_NODE_IP            "/node_ip:port"
#define SDI_ZKC_PATH_NODE_INTERNAL_IP   "/internal_node_ip:port"
#define SDI_ZKC_PATH_NODE_REPL_HOST_IP  "/internal_replication_host_ip:port"
#define SDI_ZKC_PATH_NODE_CONN_TYPE     "/conn_type"
#define SDI_ZKC_PATH_NODE_STATE         "/state"
#define SDI_ZKC_PATH_NODE_FAILOVER_TO   "/failoverTo"

/* state */
#define SDI_ZKC_STATE_RUN               "RUN"
#define SDI_ZKC_STATE_ADD               "ADD"
#define SDI_ZKC_STATE_FAILBACK          "FAILBACK"
#define SDI_ZKC_STATE_FAILOVER          "FAILOVER"
#define SDI_ZKC_STATE_SHUTDOWN          "SHUTDOWN"
#define SDI_ZKC_STATE_JOIN              "JOIN"
#define SDI_ZKC_STATE_DROP              "DROP"
#define SDI_ZKC_STATE_KILL              "KILL"


typedef enum
{
    ZK_NONE = 0 ,
    ZK_ADD,
    ZK_RUN,
    ZK_FAILBACK,
    ZK_FAILOVER,
    ZK_FAILOVER_FOR_WATCHER,
    ZK_SHUTDOWN,
    ZK_JOIN,
    ZK_DROP,
    ZK_KILL,
    ZK_ERROR
} ZKState;

typedef enum
{
    ZK_JOB_NONE = 0 ,
    ZK_JOB_ADD,
    ZK_JOB_FAILBACK,
    ZK_JOB_FAILOVER,
    ZK_JOB_FAILOVER_FOR_WATCHER,
    ZK_JOB_SHUTDOWN,
    ZK_JOB_JOIN,
    ZK_JOB_DROP,
    ZK_JOB_RESHARD,
    ZK_JOB_ERROR
} ZKJOB;

typedef enum
{
    ZK_PENDING_JOB_NONE = 0,
    ZK_PENDING_JOB_AFTER_ROLLBACK,
    ZK_PENDING_JOB_AFTER_COMMIT,
    ZK_PENDING_JOB_AFTER_END_ALL
} ZKPendingJobType;

typedef struct sdiZKPendingJob
{
    iduListNode      mListNode;
    SChar            mNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    idBool           mIsRollbackJob;
    idBool           mIsCommitJob;
    qciStmtType      mStmtType;
    SChar            mSQL[1];
} sdiZKPendingJob;

typedef enum
{
    ZK_REVERT_JOB_NONE = 0,
    ZK_REVERT_JOB_REPL_DROP,
    ZK_REVERT_JOB_REPL_ITEM_DROP,
    ZK_REVERT_JOB_TABLE_ALTER
} ZKRevertJobType;

typedef struct sdiZKRevertJob
{
    iduListNode      mListNode;
    SChar            mNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
    UInt             mRevertJobType;
    SChar            mSQL[1];
} sdiZKRevertJob;

typedef enum
{
    SDI_ZKS_META_LOCK = 0,
    SDI_ZKS_SHARD_LOCK
} ZookeeperLockType;

typedef enum
{
    SDI_ZKS_LIST_NODENAME = 0,
    SDI_ZKS_LIST_NODEINFO,
    SDI_ZKS_LIST_PRINT
} sdiZKListType;

typedef struct sdiZookeeperInfo4PV
{
    SChar   mPath[SDI_ZKC_PATH_LENGTH];
    SChar   mData[SDI_ZKC_BUFFER_SIZE];
} sdiZookeeperInfo4PV;

/* ������ : zookeeper ���� �Լ��� �ش� ��尡 shard cluster�� ���� �� ���¿��� ����ϴ� ���� ������ �����εǾ� �ִ�. *
 *          1. sdiZookeeper Ŭ������ �Լ��� ���� ����Ϸ��� �� ��� sdiZookeeper::initialize �Լ���                  *
 *             sdiZookeeper::connect�Լ��� ���� ����Ǿ�� �Ѵ�.(validateDB �Լ��� ����)                             *
 *          2. sdiZookeeper Ŭ������ �Լ��� zookeeper�� �����Ϳ� �����Ϸ��� �� ���                                  *
 *             cluster meta�� �����Ǿ� �־�� �Ѵ�.(�ܼ� path üũ���� ����)                                         */

class sdiZookeeper
{
    public:
        static IDE_RC initializeStatic();
        static void initialize( SChar       * aNodeName ,
                                qciUserInfo * aUserInfo);
        static IDE_RC finalizeStatic();
        static void finalize();

        /* ���� ���� */
        static IDE_RC connect( idBool aIsTempConnect );
        static void   disconnect();
        static idBool isConnect();

        /* ��Ÿ lock ȹ�� */
        static IDE_RC getZookeeperMetaLock( UInt aSessionID );
        static IDE_RC getShardMetaLock( UInt aTID );
        static IDE_RC getMetaLock( ZookeeperLockType aLockType, 
                                   UInt aID );
        static void   releaseZookeeperMetaLock();
        static void   releaseShardMetaLock();
        static IDE_RC releaseMetaLock( ZookeeperLockType aLockType );

        static void zookeeperLockDump(SChar * aFuncName,
                                      ZookeeperLockType aLockType,
                                      idBool aPrintCallstack);
        /* validation ���� */
        static IDE_RC validateDB( SChar  * aDBName,
                                  SChar  * aCharacterSet,
                                  SChar  * aNationalCharacterSet );
        static IDE_RC validateNode( SChar  * aDBName,
                                    SChar  * aCharacterSet,
                                    SChar  * aNationalCharacterSet,
                                    SChar  * aKSafety,
                                    SChar  * aReplicationMode,
                                    SChar  * aParallelCount,
                                    SChar  * aBinaryVersion,
                                    SChar  * aShardVersion,
                                    SChar  * aTransTBLSize,
                                    SChar  * aNodeName );
        static IDE_RC getKSafetyInfo(  SChar  * aKSafety,
                                       SChar  * aReplicationMode,
                                       SChar  * aParallelCount );
        static IDE_RC validateState( ZKState   aState,
                                     SChar   * aNodeName );
        static IDE_RC checkAllNodeAlive( idBool * aIsValid );

        static IDE_RC checkNodeAlive( SChar * aNodeName,
                                      idBool * aIsValid );

        static IDE_RC checkRecentDeadNode( SChar * aNodeName,
                                           ULong * aBeforeSMN,
                                           ULong * aAfterSMN );

        /* ��Ÿ�� �����ϴ� �� �۾� */
        static IDE_RC initClusterMeta( sdiDatabaseInfo  aShardDBInfo,
                                       sdiLocalMetaInfo aLocalMetaInfo );
        static IDE_RC finalizeClusterMeta();

        static IDE_RC addNode( SChar * aShardNodeID,
                               SChar * aNodeIPnPort,
                               SChar * aInternalNodeIPnPort,
                               SChar * aInternalReplicationHostIPnPort,
                               SChar * aConnType );
        static IDE_RC addNodeAfterCommit( ULong aSMN );
        static IDE_RC dropNode( SChar * aNodeName );
        static IDE_RC dropNodeAfterCommit( ULong   aSMN );
        static IDE_RC shutdown();
        static IDE_RC joinNode();
        static IDE_RC joinNodeAfterCommit();
        static void   reshardingJob();
        static IDE_RC failoverForWatcher( SChar * aNodeName );
        static IDE_RC failoverForQuery( SChar * aNodeName,
                                        SChar * aFailoverNodeName );
        static IDE_RC failoverAfterCommit( ULong aNewSMN, ULong aBeforeSMN );
        static IDE_RC failoverAfterRollback();
        static IDE_RC failback();
        static IDE_RC failbackAfterCommit( ULong aSMN );
        static void callAfterCommitFunc( ULong aAfterSMN, ULong aBeforeSMN );
        static void callAfterRollbackFunc( ULong   aNewSMN = SDI_NULL_SMN, ULong aBeforeSMN = SDI_NULL_SMN );

        /* ��� ����Ʈ ���� */
        static IDE_RC getAllNodeInfoList( iduList   ** aList,
                                          UInt       * aNodeCnt );
        static IDE_RC getAllNodeInfoList( iduList   ** aList,
                                          UInt       * aNodeCnt,
                                          iduMemory  * aMem );
        static IDE_RC getAllNodeNameList( iduList   ** aList );
        static IDE_RC getAliveNodeNameList( iduList  ** aList );
        static IDE_RC getAliveNodeNameListIncludeNodeName( iduList  ** aList,
                                                           SChar     * aNodeName );
        static IDE_RC getNextNode( SChar   * aTargetName,
                                   SChar   * aReturnName,
                                   ZKState * aNodeState );
        static IDE_RC getPrevNode( SChar   * aTargetName,
                                   SChar   * aReturnName,
                                   ZKState * aNodeState );
        static IDE_RC getNextAliveNode( SChar   * aTargetName,
                                        SChar   * aReturnName );
        static IDE_RC getPrevAliveNode( SChar   * aTargetName,
                                        SChar   * aReturnName );
        static sdiLocalMetaInfo * getNodeInfo( iduList * aNodeList,
                                               SChar   * aNodeName );

        /* watch ���� */
        static IDE_RC settingAddNodeWatch();
        static IDE_RC settingDeleteNodeWatch( SChar * aPath );
        static IDE_RC settingAllDeleteNodeWatch();
        static void watch_addNewNode( aclZK_Handler * aZzh,
                                      SInt            aType,
                                      SInt            aState,
                                      const SChar   * aPath,
                                      void          * aContext );
        static void watch_deleteNode( aclZK_Handler * aZzh,
                                      SInt            aType,
                                      SInt            aState,
                                      const SChar   * aPath,
                                      void          * aContext );

        /* �ܺλ�� �������̽� */
        static IDE_RC getNodeInfo( SChar * aTargetName,
                                   SChar * aInfoType,
                                   SChar * aValue,
                                   SInt  * aDataLen );
        static IDE_RC getNodeState( SChar   * aTargetName,
                                    ZKState * aState );
        static IDE_RC insertInfo( SChar * aPath,
                                  SChar * aData );
        static IDE_RC readInfo( SChar * aPath,
                                SChar * aData,
                                SInt  * aDataLen );
        static IDE_RC removeInfo( SChar * aPath );
        static IDE_RC createPath( SChar * aPath,
                                  SChar * aData );
        static IDE_RC removePath( SChar * aPath );
        static IDE_RC removeRecursivePathAndInfo( SChar * aPath );
        static IDE_RC checkPath( SChar  * aPath,
                                 idBool * aIsExist );

        static IDE_RC getZookeeperSMN( ULong * aSMN );

        /* ���� ��� �Լ� */
        static void sortNode( iduList * aList );
        static void freeList( iduList * aList, sdiZKListType aListType );
        static ZKState stateStringConvert( SChar * aStateString);
        static IDE_RC addConnectionInfo();
        static IDE_RC removeConnectionInfo( SChar * aTargetNodeName );

        static void logAndSetErrMsg( ZKCResult aErrCode, SChar * aInfo = NULL );
        static IDE_RC removeNode( SChar * aRemoveNodeName );
        static IDE_RC killNode( SChar * aNodeName );

        static IDE_RC updateSMN( ULong aSMN );
        static IDE_RC prepareForUpdateSMN( ULong aSMN );
        static IDE_RC updateState( SChar * aState,
                                   SChar * aTargetNodeName );
        static idBool isNodeMatch( SChar * aSrcNodeName, SChar * aDstNodeName );
        static IDE_RC addPendingJob( SChar            * aSQL,
                                     SChar            * aNodeName,
                                     ZKPendingJobType   aPendingType,
                                     qciStmtType        aSQLStmtType );
        static void executePendingJob( ZKPendingJobType aPendingType );
        static idBool isExistPendingJob();
        static void  removePendingJob();
    
        static IDE_RC addRevertJob( SChar * aSQL,
                                    SChar * aNodeName,
                                    ZKRevertJobType aRevertType );
        static void executeRevertJob( ZKRevertJobType aRevertType );
        static idBool isExistRevertJob();
        static void  removeRevertJob();
    
        static void setReplInfo4FailoverAfterCommit( SChar * aFirstNodeName,
                                                     SChar * aSecondNodeName,
                                                     SChar * aFirstReplName,
                                                     SChar * aSecondReplName );

        static IDE_RC checkExistNotFailedoverDeadNode( idBool * aExistNotFailedoverDeadNode );

        /* performance view ���� */
        static IDE_RC getZookeeperInfo4PV( iduList ** aList );
        static IDE_RC insertList4PV( iduList * aList,
                                     SChar  * aZookeeperPath );

        /* ��� ���� */
        static aclZK_Handler * mZKHandler;
        static idBool          mIsConnect;
        static idBool          mIsGetShardLock;
        static idBool          mIsGetZookeeperLock;
        static SChar           mMyNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
        static iduMutex        mConnectMutex;

        /* ���� �������� �۾� ������ �����ؾ� mm ��⿡�� commit�� �˸��� AfterCommit �Լ��� ȣ�� �� �� �ִ�.*/
        static ZKJOB           mRunningJobType;
        static SChar           mTargetNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
        static SChar           mFailoverNodeName[SDI_NODE_NAME_MAX_SIZE + 1];

        static qciUserInfo     mUserInfo;
        static UInt            mTickTime;
        static UInt            mSyncLimit;
        static UInt            mServerCnt;

        /* Failover After Commit ���� RP Stop�� ���� ������ �ʿ��ϴ�. */
        static SChar           mFirstStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
        static SChar           mSecondStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
        static SChar           mFirstStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
        static SChar           mSecondStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];

        /* Failover After Rollback ���� RP Drop�� ���� ������ �ʿ��ϴ�. */
        static SChar           mDropReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
        static iduList         mJobAfterTransEnd;

        // shard package���� ����ó���� ���� ���
        static iduList         mRevertJobList;
};

inline void sdiZookeeper::sortNode( iduList * aList )
{
    iduListNode *   sIterator  = NULL; 
    iduListNode *   sIterator2 = NULL;
    void *          sSwapBuf   = NULL;

    IDU_LIST_ITERATE( aList, sIterator )
    {
        IDU_LIST_ITERATE( aList, sIterator2 )
        {
            if( sIterator2->mNext != aList )
            {
                /* ���� ���� ���� ���� ��庸�� ũ�ٸ� ��ü�Ѵ�. */
                if( idlOS::strncmp( 
                        ( ( SChar* )sIterator2->mObj ), 
                        ( ( SChar* )sIterator2->mNext->mObj ),
                        SDI_NODE_NAME_MAX_SIZE + 1 )  > 0  )
                {
                    sSwapBuf = sIterator2->mObj;
                    sIterator2->mObj = sIterator2->mNext->mObj;
                    sIterator2->mNext->mObj = sSwapBuf;
                }
            }
            else
            {
                /* ������ ����� ��� ���� ��尡 header�̹Ƿ� ���� �ʿ䰡 ����. */
            }
        }
    }

    return;
}

inline void sdiZookeeper::freeList( iduList * aNodeList, sdiZKListType aListType )
{
    iduListNode         * sNode = NULL;
    sdiLocalMetaInfo    * sNodeInfo = NULL;
    SChar               * sNodeName = NULL;
    sdiZookeeperInfo4PV * sInfo4PV = NULL;

    if( aNodeList != NULL )
    {
        while( IDU_LIST_IS_EMPTY( aNodeList ) == ID_FALSE )
        {
            sNode = IDU_LIST_GET_FIRST( aNodeList );

            IDU_LIST_REMOVE( sNode );

            switch( aListType )
            {
                case SDI_ZKS_LIST_NODENAME:
                    sNodeName = (SChar*)sNode->mObj;
                    (void)iduMemMgr::free( sNodeName );
                    (void)iduMemMgr::free( sNode );
                    break;
                case SDI_ZKS_LIST_NODEINFO:
                    sNodeInfo = (sdiLocalMetaInfo*)sNode->mObj;
                    (void)iduMemMgr::free( sNodeInfo );
                    (void)iduMemMgr::free( sNode );
                    break;
                case SDI_ZKS_LIST_PRINT:
                    sInfo4PV = (sdiZookeeperInfo4PV*)sNode->mObj;
                    (void)iduMemMgr::free( sInfo4PV );
                    (void)iduMemMgr::free( sNode );
                    break;
                default:
                    IDE_DASSERT(0);
                    break;
            }
        }

        (void)iduMemMgr::free( aNodeList );
    }

    return;
}

inline ZKState sdiZookeeper::stateStringConvert( SChar * aStateString )
{
    ZKState sState = ZK_ERROR;

    IDE_ASSERT( aStateString != NULL );

    if( idlOS::strncmp( aStateString, "NONE", 4 ) == 0 )
    {
        sState = ZK_NONE;
    }

    if( idlOS::strncmp( aStateString, "ADD", 3 ) == 0 )
    {
        sState = ZK_ADD;
    }

    if( idlOS::strncmp( aStateString, "RUN", 3 ) == 0 )
    {
        sState = ZK_RUN;
    }

    if( idlOS::strncmp( aStateString, "FAILBACK", 8 ) == 0 )
    {
        sState = ZK_FAILBACK;
    }

    if( idlOS::strncmp( aStateString, "FAILOVER", 8 ) == 0 )
    {
        sState = ZK_FAILOVER;
    }

    if( idlOS::strncmp( aStateString, "SHUTDOWN", 8 ) == 0 )
    {
        sState = ZK_SHUTDOWN;
    }

    if( idlOS::strncmp( aStateString, "JOIN", 4 ) == 0 )
    {
        sState = ZK_JOIN;
    }

    if( idlOS::strncmp( aStateString, "KILL", 4 ) == 0 )
    {
        sState = ZK_KILL;
    }

    return sState;
}

inline idBool sdiZookeeper::isConnect()
{
    return mIsConnect;
}

/*********************************************************************
* FUNCTION DESCRIPTION : sdiZookeeper::isNodeMatch                  *
 * ------------------------------------------------------------------*
 * �� ����� �̸��� �������� üũ�Ѵ�.
 * ���� �ϳ��� ����̸��̶� NULL�� ���´ٸ� �������� ���� ������ ����Ѵ�. 
 *
 * aSrcNodeName - [IN] �̸��� ���� ù��° ����� �̸�
 * aDstNodeName - [IN] �̸��� ���� �ι�° ����� �̸�
 *********************************************************************/
inline idBool sdiZookeeper::isNodeMatch( SChar * aSrcNodeName,
                                         SChar * aDstNodeName )
{
    idBool  sResult = ID_FALSE;
    UInt    sSrcNodeNameSize;
    UInt    sDstNodeNameSize;

    if( ( aSrcNodeName == NULL ) || ( aDstNodeName == NULL ) )
    {
        sResult = ID_FALSE;
    }
    else
    {
        sSrcNodeNameSize = idlOS::strlen( aSrcNodeName );
        sDstNodeNameSize = idlOS::strlen( aDstNodeName );

        if( sSrcNodeNameSize != sDstNodeNameSize )
        {
            sResult = ID_FALSE;
        }
        else
        {
            if( idlOS::strncmp( aSrcNodeName, aDstNodeName, sSrcNodeNameSize ) == 0 )
            {
                sResult = ID_TRUE;
            }
            else
            {
                sResult = ID_FALSE;
            }
        }
    }
    return sResult;
}

inline void sdiZookeeper::zookeeperLockDump(SChar * aMessageLogString,
                                            ZookeeperLockType aLockType,
                                            idBool aPrintCallstack)
{
#if defined (DEBUG)
    if ( aPrintCallstack == ID_TRUE )
    {
        switch ( aLockType )
        {
            case SDI_ZKS_META_LOCK:
                ideLog::logLine(IDE_ERR_0,"zookeeper meta lock: %s ",aMessageLogString, aLockType);
                break;
            case SDI_ZKS_SHARD_LOCK:
                ideLog::logLine(IDE_ERR_0,"shard meta lock: %s ",aMessageLogString, aLockType);
                break;
            default:
                break;
        }
        ideLog::logCallStackInternal();
    }
#else
    PDL_UNUSED_ARG(aMessageLogString);
    PDL_UNUSED_ARG(aLockType);
    PDL_UNUSED_ARG(aPrintCallstack);
#endif
}

#endif // _O_SDI_ZOOKEEPER_H_
