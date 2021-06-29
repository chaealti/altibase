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

#include <idtContainer.h>
#include <mtc.h>
#include <sdiZookeeper.h>
#include <sdiFOThread.h>
#include <smi.h>
#include <qdsd.h>

idBool          sdiZookeeper::mIsConnect = ID_FALSE;
idBool          sdiZookeeper::mIsGetShardLock = ID_FALSE;
idBool          sdiZookeeper::mIsGetZookeeperLock = ID_FALSE;
aclZK_Handler * sdiZookeeper::mZKHandler = NULL ;
SChar           sdiZookeeper::mMyNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
iduMutex        sdiZookeeper::mConnectMutex;
SChar           sdiZookeeper::mTargetNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
SChar           sdiZookeeper::mFailoverNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
ZKJOB           sdiZookeeper::mRunningJobType = ZK_JOB_NONE;
qciUserInfo     sdiZookeeper::mUserInfo;
UInt            sdiZookeeper::mTickTime = 0;
UInt            sdiZookeeper::mSyncLimit = 0;
UInt            sdiZookeeper::mServerCnt = 0;
iduList         sdiZookeeper::mJobAfterTransEnd;
iduList         sdiZookeeper::mRevertJobList;

SChar           sdiZookeeper::mFirstStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
SChar           sdiZookeeper::mSecondStopNodeName[SDI_NODE_NAME_MAX_SIZE + 1];
SChar           sdiZookeeper::mFirstStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];
SChar           sdiZookeeper::mSecondStopReplName[SDI_REPLICATION_NAME_MAX_SIZE + 1];

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::logAndSetErrMsg              *
 * ------------------------------------------------------------------*
 * ZKC �Լ��� ���������� �� ����� TRC �α׿� �����.
 *
 * aErrCode - [IN] ���� �ڵ�
 *********************************************************************/
void sdiZookeeper::logAndSetErrMsg( ZKCResult aErrCode, SChar * aInfo )
{
    SChar sEmptyStr[] = "";
    SChar * sInfo = sEmptyStr;
    if ( aInfo != NULL )
    {
        sInfo = aInfo;
    }
    switch( aErrCode )
    {
        case ZKC_CONNECTION_FAIL:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_CONNECTION_FAIL );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Failed to connect ZKS." );
            break;
        case ZKC_CONNECTION_MISSING:
            /* ZKS�� ������ ���� ���� */
            /* �����ڵ带 �����ϰ� trc�α׸� ���� �� assert�� ȣ���Ѵ�. */
            mIsConnect = ID_FALSE;
            ideSetErrorCode( sdERR_FATAL_SDI_ZKC_CONNECTION_MISSING );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Connection with ZKS has been lost." );
            IDE_CALLBACK_FATAL( "Connection with ZKS has been lost." );
            break;
        case ZKC_NODE_EXISTS:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_NODE_EXISTS );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Node already exist." );
            break;
        case ZKC_NO_NODE:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_NO_NODE );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Node does not exist." );
            break;
        case ZKC_WAITING_TIMEOUT:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_WAITING_TIMEOUT );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Waiting timeout." );
            break;
        case ZKC_DB_VALIDATE_FAIL:  
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_DB_VALIDATE_FAIL );
            break;
        case ZKC_VALIDATE_FAIL:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_VALIDATE_FAIL, sInfo );
            break;
        case ZKC_STATE_VALIDATE_FAIL:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_STATE_VALIDATE_FAIL );
            break;
        case ZKC_META_CORRUPTED:
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Metadata is Corrupted." );
            IDE_CALLBACK_FATAL( "Zookeeper metadata is corrupted." );
            break;
        case ZKC_LOCK_ERROR:
            ideSetErrorCode( sdERR_FATAL_SDI_ZKC_LOCK_ERROR );
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] Lock is Currupted." );
            break;
        case ZKC_ETC_ERROR:
        default:
            ideSetErrorCode( sdERR_ABORT_SDI_ZKC_ETC_ERROR , aErrCode);
            ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] ETC Error %"ID_INT32_FMT, aErrCode );
            break;
    } 
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::initializeStatic             *
 * ------------------------------------------------------------------*
 * sdiZookeeper class staticaly initialize
 *
 *********************************************************************/
IDE_RC sdiZookeeper::initializeStatic()
{
    mIsConnect = ID_FALSE;
    mIsGetShardLock = ID_FALSE;
    mIsGetZookeeperLock = ID_FALSE;

    idlOS::memset( &mUserInfo, 0, ID_SIZEOF( qciUserInfo ) );
    
    IDE_TEST( mConnectMutex.initialize( "ZOOKEEPER_CLIENT_CONNECTION_MUTEX",
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );
    mMyNodeName[0] = '\0';

    idlOS::strncpy( mFailoverNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFirstStopNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFirstStopReplName,
                    SDM_NA_STR,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopReplName,
                    SDM_NA_STR,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );

    IDU_LIST_INIT(&mJobAfterTransEnd);

    IDU_LIST_INIT(&mRevertJobList);

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::initialize                   *
 * ------------------------------------------------------------------*
 * sdiZookeeper nodename �ʱ�ȭ �Ѵ�.
 *
 * aNodeName - [IN] �ش� ��忡�� ����� ����� �̸�
 *********************************************************************/
void sdiZookeeper::initialize( SChar       * aNodeName,
                               qciUserInfo * aUserInfo )
{
    qciUserInfo * sUserInfo = &mUserInfo;
    idlOS::strncpy( mMyNodeName,
                    aNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    QCI_COPY_USER_INFO( sUserInfo, aUserInfo );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::finalizeStatic               *
 * ------------------------------------------------------------------*
 * sdiZookeeper class staticaly finalize
 *********************************************************************/
IDE_RC sdiZookeeper::finalizeStatic()
{
    IDE_TEST( mConnectMutex.destroy() != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::finalize                     *
 * ------------------------------------------------------------------*
 * sdiZookeeper Ŭ���� nodename ���� �Ѵ�.
 *********************************************************************/
void sdiZookeeper::finalize()
{
    idlOS::memset( mMyNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );

    return;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::connect                      *
 * ------------------------------------------------------------------*
 * zookeeper server�� �����Ѵ�.
 * zoo.cfg�� ������ ���� ������ ������ �� �ش� ������ ���� ZKS�� �����Ѵ�.
 * 
 * aIsTempConnect - [IN]  �ӽ� �������� ����.
 *                        �ش� ���ڴ� create database �������� ȣ��� ��쿡��
 *                        TRUE�� ���õǰ� ������ ��쿡�� FALSE�� ���õȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::connect( idBool aIsTempConnect )
{
    ZKCResult sResult;
    idBool    sIsLock = ID_FALSE;

    /* zoo.cfg open */
    SChar   * sAltiHomePath;
    SChar     sFilePath[256] = {0,};
    FILE    * sFP;

    /* file read */
    SInt      sLength;
    SChar     sLineBuffer[IDP_MAX_PROP_LINE_SIZE];
    idBool    sIsFileOpen = ID_FALSE;

    /* zookeeper info */
    UInt      sServerCnt = 0;
    SChar     sServerInfo[256] = {0,};
    UInt      sServerInfoLength = 0;
    SChar   * sStringPtr;
    SChar     sPortNum[6] = {0,};
    UInt      sTickTime = 0;
    UInt      sSyncLimit = 0;

    if( aIsTempConnect == ID_FALSE )
    {
        IDE_ASSERT( mConnectMutex.lock( NULL ) == IDE_SUCCESS );
        sIsLock = ID_TRUE;
    }
    else
    {
        /* �ӽ� ������ ��� ���� ������ �Ұ����ϹǷ� mutex�� ���� �ʿ䰡 ����. */
    }

    if( mIsConnect == ID_TRUE )
    {
        /* �̹� ����Ǿ� �ִٸ� �翬�� �� �ʿ䰡 ����. */
        IDE_CONT( normal_exit );
    }

    /* 1. zoo.cfg�� ������ ���� ������ �����´�. */
    sAltiHomePath = idlOS::getenv( IDP_HOME_ENV );

    idlOS::strncpy( sFilePath,
                    sAltiHomePath,
                    ID_SIZEOF( sFilePath ) );
    idlOS::strncat( sFilePath,
                    IDL_FILE_SEPARATORS,
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    idlOS::strncat( sFilePath,
                    "ZookeeperServer",
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    idlOS::strncat( sFilePath,
                    IDL_FILE_SEPARATORS,
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    idlOS::strncat( sFilePath, 
                    "conf",
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    idlOS::strncat( sFilePath,
                    IDL_FILE_SEPARATORS,
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    idlOS::strncat( sFilePath, 
                    "zoo.cfg",
                    ID_SIZEOF( sFilePath ) - idlOS::strlen( sFilePath ) - 1 );
    sFP = idlOS::fopen( sFilePath, "r" );

    if( sFP == NULL )
    {
        /* ���� open�� �����ߴٸ� create database �������� ȣ��Ȱ� �ƴ϶�� ���� ��Ȳ�̴�. */
        IDE_TEST_RAISE( aIsTempConnect == ID_FALSE , err_no_file );

        /* create database �������� ȣ��� ��쿡�� ��� �Ѵ�. */
        IDE_CONT( normal_exit );
    }

    sIsFileOpen = ID_TRUE;

    while( idlOS::idlOS_feof( sFP ) == 0 )
    {
        idlOS::memset( sLineBuffer, 0, IDP_MAX_PROP_LINE_SIZE );

        if( idlOS::fgets( sLineBuffer, IDP_MAX_PROP_LINE_SIZE, sFP ) == NULL )
        {
            /* �����Ͱ� ���� */
            break;        
        }

        /* white space ���� */
        idp::eraseWhiteSpace( sLineBuffer );
        sLength = idlOS::strlen( sLineBuffer );

        /* �� ���̳� �ּ��� ���� */
        if( ( sLength > 0 ) && ( sLineBuffer[0] != '#' ) )
        {
            /* tickTime ������ �����´�. */
            if( idlOS::strncmp( sLineBuffer, "tickTime", 8 ) == 0 )
            {
                sStringPtr = idlOS::strtok( sLineBuffer, "=" );
                sStringPtr = idlOS::strtok( NULL, "\0" );

                /* ����� �Ǽ��� ���ڰ� �ƴ� ���ڰ� ������� ��� atoi �Լ��� ����ϸ� 0���� �����Ѵ�. */
                sTickTime = idlOS::atoi( sStringPtr );
            }

            /* synclimit ������ �����´�. */
            else if( idlOS::strncmp( sLineBuffer, "syncLimit", 9 ) == 0 )
            {
                sStringPtr = idlOS::strtok( sLineBuffer, "=" );
                sStringPtr = idlOS::strtok( NULL, "\0" );

                /* ����� �Ǽ���  ���ڰ� �ƴ� ���ڰ� ������� ��� atoi �Լ��� ����ϸ� 0���� �����Ѵ�. */
                sSyncLimit = idlOS::atoi( sStringPtr );
            }

            /* clientPort ������ �����´�. */
            else if( idlOS::strncmp( sLineBuffer, "clientPort", 10 ) == 0 )
            {
                sStringPtr = idlOS::strtok( sLineBuffer, "=" );
                sStringPtr = idlOS::strtok( NULL, "\0" );

                /* clientPort�� 5�ڸ�+/0 ���� �ִ� 6�ڸ��̸� �� �̻��� ��� ������ ��Ȳ�̴�. */
                IDE_TEST_RAISE( idlOS::strlen( sStringPtr ) > 6, err_no_serverInfo );

                idlOS::strncpy( sPortNum,
                                sStringPtr,
                                ID_SIZEOF( sPortNum ) );
            }

            /* server ������ �����´�. */
            /* ���� : ���� ������ �����ϴ� server string�� port ������ �Բ� ���Ƿ�
               zoo.cfg�� client port ������ server �������� ���� ���;� �Ѵ�. */
            else if( idlOS::strncmp( sLineBuffer, "server.", 7 ) == 0 )
            {
                sStringPtr = idlOS::strtok( sLineBuffer, "=" );
                sStringPtr = idlOS::strtok( NULL, ":" );

                if( sServerCnt == 0 )
                {
                    sServerInfoLength = idlOS::strlen( sStringPtr ) + 1 + idlOS::strlen( sPortNum );

                    idlOS::strncat( sServerInfo,
                                    sStringPtr,
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                    idlOS::strncat( sServerInfo, 
                                    ":",
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                    idlOS::strncat( sServerInfo,
                                    sPortNum,
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                }
                else
                {

                    sServerInfoLength += idlOS::strlen( sStringPtr ) + 2 + idlOS::strlen( sPortNum );

                    /* ����ũ��(256)���� ��� �ȵȴ�. */
                    IDE_TEST_RAISE( sServerInfoLength >= 256, err_too_much_serverInfo );

                    idlOS::strncat( sServerInfo,
                                    ",",
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                    idlOS::strncat( sServerInfo,
                                    sStringPtr,
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                    idlOS::strncat( sServerInfo,
                                    ":",
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                    idlOS::strncat( sServerInfo,
                                    sPortNum,
                                    ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                }

                sServerCnt++;
            }
        }
    }

    sIsFileOpen = ID_FALSE;
    (void)idlOS::fclose( sFP );

    if( SDU_SHARD_ZOOKEEPER_TEST == 1 )
    {
            if( sServerCnt == 0 )
            {
                sServerInfoLength = idlOS::strlen( "localhost" ) + 1 + idlOS::strlen( sPortNum );
                /* ����ũ��(256)���� ��� �ȵȴ�. */
                IDE_TEST_RAISE( sServerInfoLength >= 256, err_too_much_serverInfo );

                idlOS::strncat( sServerInfo,
                                "localhost" ,
                                ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                idlOS::strncat( sServerInfo, 
                                ":",
                                ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                idlOS::strncat( sServerInfo,
                                sPortNum,
                                ID_SIZEOF( sServerInfo ) - idlOS::strlen( sServerInfo ) - 1 );
                sServerCnt++;
            }
    }
    else
    {
        /* ������ �ּ� 3�� �̻� �־�� �Ѵ�. */
        IDE_TEST_RAISE( sServerCnt < SDI_ZKC_ZKS_MIN_CNT, err_no_serverInfo );
     }
    /* ������ ���ڴ� �ִ� 7�������� ���ȴ�. */
    IDE_TEST_RAISE( sServerCnt > SDI_ZKC_ZKS_MAX_CNT, err_too_much_serverInfo );

    /* ticktime�� synclimit�� �Ѵ� �����ؾ� �Ѵ�. */
    IDE_TEST_RAISE( sTickTime * sSyncLimit == 0, err_no_serverInfo );

    /* 2. ������ ���� ������ ZKS�� �����Ѵ�. */
    sResult = aclZKC_connect( &mZKHandler,
                              sServerInfo,
                              sTickTime * sSyncLimit );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_connectionFail );

    mIsConnect = ID_TRUE;

    mTickTime  = sTickTime;
    mSyncLimit = sSyncLimit;
    mServerCnt = sServerCnt;

    IDE_EXCEPTION_CONT( normal_exit );

    if( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        IDE_ASSERT( mConnectMutex.unlock() == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_no_file )
    {
        /* zoo.cfg�� ���� */
        ideSetErrorCode( sdERR_ABORT_SDI_ZKC_ZOOCFG_NO_EXIST );
    }
    IDE_EXCEPTION( err_no_serverInfo )
    {
        /* zoo.cfg�� ������ server�� ������ ���� */
        ideSetErrorCode( sdERR_ABORT_SDI_ZKC_CONNECTION_INFO_MISSING );
    }
    IDE_EXCEPTION( err_too_much_serverInfo )
    {
        /* ���� ������ �ʹ� ���� */
       ideSetErrorCode( sdERR_ABORT_SDI_ZKC_TOO_MUCH_SERVER_INFO ); 
    }
    IDE_EXCEPTION( err_connectionFail )
    {
        /* ������ �õ������� ���� */
        if( aIsTempConnect == ID_FALSE )
        {
            logAndSetErrMsg( sResult );
        }
        else
        {
            /* create databse ���� ���� ZKS�� ������ �ȵ� ��쿡�� trc log�� ������ �ʴ´�. */
        }
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    mIsConnect = ID_FALSE;

    if( sIsFileOpen == ID_TRUE )
    {
        (void)idlOS::fclose( sFP );
    }

    if( sIsLock == ID_TRUE )
    {
        IDE_ASSERT( mConnectMutex.unlock() == IDE_SUCCESS );
    }
    IDE_POP();
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::disconnect                   *
 * ------------------------------------------------------------------*
 * ZKS���� ������ �����Ѵ�.
 * ���� ����� connection_info�� ���ŵǸ鼭 �ٸ� ��忡�� �˶��� ����.
 *********************************************************************/
void sdiZookeeper::disconnect()
{
    if ( mIsConnect == ID_TRUE )
    {
        aclZKC_disconnect( mZKHandler );
    }

    mIsConnect = ID_FALSE;

    mTickTime  = 0;
    mSyncLimit = 0;
    mServerCnt = 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getZookeeperMetaLock         *
 * ------------------------------------------------------------------*
 * ZKS�� Ŭ������ ��Ÿ�� �����ϱ� ���� ��Ÿ�� ���� ������ ���� ���� ������ Lock�� ��´�.
 * �ش��Լ��� ZKS�� Ŭ������ ��Ÿ�� �����Ҷ� ȣ��ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::getZookeeperMetaLock(UInt aSessionID)
{

    IDE_TEST_RAISE( isConnect() != ID_TRUE, NOT_CONNECTED );

    IDE_TEST( getMetaLock( SDI_ZKS_META_LOCK, aSessionID ) != IDE_SUCCESS );

    idCore::acpAtomicSet32( &mIsGetZookeeperLock,
                            ID_TRUE );

    return IDE_SUCCESS;

    IDE_EXCEPTION( NOT_CONNECTED )
    {
        ideSetErrorCode( sdERR_ABORT_SDI_SHARD_NOT_JOIN );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getShardMetaLock             *
 * ------------------------------------------------------------------*
 * ��� ���ο��� ��Ÿ�� �����Ҷ� Ÿ ��尡 ���ÿ� �����ϴ� ���� ���� ���� ����ϴ� lock
 * �ش� �Լ��� �������̽� �Լ��� ZKS ��� �������� ȣ��ȴ�.
 *
 * aTID     - [IN] �ش� lock�� ��� transaction�� ID
 *********************************************************************/
IDE_RC sdiZookeeper::getShardMetaLock( UInt aTID )
{
    idBool sIsAlreadyConnected = ID_FALSE;

    /* R2HA to be remove */
    if ( SDU_SHARD_ZOOKEEPER_TEST == 1 )
    {
        if ( sdiZookeeper::isConnect() != ID_TRUE )
        {
            IDE_TEST( sdiZookeeper::connect(ID_FALSE) != IDE_SUCCESS );
        }
        else
        {
            sIsAlreadyConnected = ID_TRUE;
        }
        IDE_TEST_RAISE( mMyNodeName[0] == '\0', ERR_NOT_EXIST_MYNODENAME );
    
        IDE_TEST( getMetaLock( SDI_ZKS_SHARD_LOCK, aTID ) != IDE_SUCCESS );

         idCore::acpAtomicSet32( & mIsGetShardLock,
                            ID_TRUE );
    }
    else
    {
        sIsAlreadyConnected = ID_TRUE;
    }


    return IDE_SUCCESS;
    IDE_EXCEPTION( ERR_NOT_EXIST_MYNODENAME )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdiZookeeper::getShardMetaLock",
                                  "NOT EXIST MY NODE NAME " ) );
    }
    IDE_EXCEPTION_END;
    if ( sIsAlreadyConnected != ID_TRUE )
    {
        sdiZookeeper::disconnect();
    }
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getMetaLock                  *
 * ------------------------------------------------------------------*
 * �ش� �Լ��� ���� ����� ������ ����.
 * 1. zookeeper_meta_lock�� �ڽ� ��尡 �ִ��� Ȯ���Ѵ�.
 * 2. ���� ��� �����ð� ��� �� 1������ ������Ѵ�.
 * 3. ���� ��� �ڽ� ��带 �ӽ� ���� �����Ѵ�.
 * 4. �ڽ� ��� ������ �������� ��� lock�� ������ ���� ������ �Ǵ��ϰ� �Լ��� �����Ѵ�.
 * 5. �ڽ� ��� ������ �������� ��� �ٸ� client�� ���� lock�� ���� ������ �Ǵ��ϰ�
 *    �����ð� ��� �� 1������ ������Ѵ�.
 *
 * aLockType    - [IN]  ���� lock�� ����(Zookeeper meta lock / Shard meta lock �� �ϳ�)
 * aID         - [IN]  meta lock�� ����� ��� ������ ID 
 *********************************************************************/
IDE_RC sdiZookeeper::getMetaLock( ZookeeperLockType aLockType,
                                  UInt              aID )
{
    ZKCResult       sResult;

    String_vector   sChildren;
    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sLockPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sBuffer2[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar         * sStrCheck = NULL;
    SInt            sDataLength = SDI_ZKC_BUFFER_SIZE;
    UInt            sWaitingTime = 0;
    idBool          sIsGetLock = ID_FALSE;

    IDE_DASSERT( ( aLockType == SDI_ZKS_META_LOCK ) || ( aLockType == SDI_ZKS_SHARD_LOCK ) );

    if( aLockType == SDI_ZKS_META_LOCK )
    {
        idlOS::strncpy( sLockPath, 
                        SDI_ZKC_PATH_META_LOCK, 
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sNodePath,
                        SDI_ZKC_PATH_META_LOCK, 
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sNodePath,
                        SDI_ZKC_PATH_DISPLAY_LOCK,
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        /* sLockPath�� '/altibase_shard/cluster_meta/zookeeper_meta_lock' �̰�
         * sNodePath�� '/altibase_shard/cluster_meta/zookeeper_meta_lock/locked' �̴�. */
        /* ID of zookeeper meta lock may be session id */
    }
    else
    {
        /* ID of shard meta lock is transaction id */
        IDE_DASSERT( aID != SM_NULL_TID );

        idlOS::strncpy( sLockPath,
                        SDI_ZKC_PATH_SHARD_META_LOCK,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sNodePath,
                        SDI_ZKC_PATH_SHARD_META_LOCK, 
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sNodePath,
                        SDI_ZKC_PATH_DISPLAY_LOCK,
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        /* sLockPath�� '/altibase_shard/cluster_meta/shard_meta_lock' �̰�
         * sNodePath�� '/altibase_shard/cluster_meta/shard_meta_lock/locked' �̴�. */
    }

    idlOS::snprintf( sBuffer2, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT32_FMT, aID );

    while( sIsGetLock == ID_FALSE )
    {
        do{
            /* 1. meta lock�� �ڽ� ��尡 �ִ��� Ȯ���Ѵ�. */
            sResult = aclZKC_getChildren( mZKHandler,
                                          sLockPath,
                                          &sChildren );

            IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

            /* 2. �ڽ� ��尡 ���� ��� �����ð� ��� �� 1�� ���� ������Ѵ�. */
            if( sChildren.count > 0 )
            {
                idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
                sDataLength = SDI_ZKC_BUFFER_SIZE;
                sResult = aclZKC_getInfo( mZKHandler,
                                            sNodePath,
                                            sBuffer,
                                            &sDataLength );

                /* �ڽ��� üũ�Ϸ��� �д� ���� lock�� Ǯ�� ��� */
                if( sResult == ZKC_NO_NODE )
                {
                    continue;
                }
                else
                {
                    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
                }

                /* ������ data�� [����̸�]/[ID]�� �����̹Ƿ� �и��Ͽ� üũ�Ѵ�. */
                sStrCheck = idlOS::strtok( sBuffer, "/" );

                /* ��� �̸��� ������ üũ */
                if( idlOS::strncmp( sStrCheck,
                                    mMyNodeName, 
                                    SDI_NODE_NAME_MAX_SIZE ) == 0 )
                {
                    sStrCheck = idlOS::strtok( NULL, "\0" );

                    /* ID�� �������� üũ */
                    if( idlOS::strncmp( sStrCheck,
                                        sBuffer2,
                                        idlOS::strlen( sBuffer2 ) ) == 0 )
                    {
                        /* �Ѵ� �����ϴٸ� �̹� lock�� ���������� ����Ѵ�. */
                        IDE_CONT( normal_exit );
                    }
                } 
                idlOS::sleep( 1 );
                sWaitingTime++;

                IDE_TEST_RAISE( sWaitingTime > ZOOKEEPER_LOCK_WAIT_TIMEOUT, err_waitingTime );
            }

        }while( sChildren.count > 0 );

        zookeeperLockDump((SChar*)"get meta lock begin", aLockType, ID_FALSE);
        /* 3. �ڽ� ��尡 ���� ��� �ڽ� ��带 �ӽ� ���� �����Ѵ�. */
        /* lock�� ��� [����̸�]/[ID]�� ���·� �����Ѵ�. */
        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        idlOS::strncpy( sBuffer,
                        mMyNodeName,
                        SDI_ZKC_BUFFER_SIZE );
        idlOS::strncat( sBuffer,
                        "/",
                        ID_SIZEOF( sBuffer ) - idlOS::strlen( sBuffer ) - 1 );
        idlOS::strncat( sBuffer,
                        sBuffer2,
                        ID_SIZEOF( sBuffer ) - idlOS::strlen( sBuffer ) - 1 );


        sResult = aclZKC_insertNode( mZKHandler,
                                     sNodePath,
                                     sBuffer,
                                     idlOS::strlen( sBuffer ),
                                     ACP_TRUE );
        if( sResult == ZKC_SUCCESS )
        {
            /* 4.1 ���� �ߴٸ� ���� �������� �ٽ� üũ�Ѵ�. *
                  ( ������ ���ؼ���� ������ �Ǵ� �κ� )    */

            /* �ڽ� ���� 1������ üũ */
            sResult = aclZKC_getChildren( mZKHandler,
                                          sLockPath,
                                          &sChildren );

            IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

            IDE_TEST_RAISE( sChildren.count != 1, err_lockCurrupted );

            /* �ڽ��� �����Ѱ� ������ �ٽ� üũ */
            sDataLength = SDI_ZKC_BUFFER_SIZE;
            sResult = aclZKC_getInfo( mZKHandler,
                                      sNodePath,
                                      sBuffer2,
                                      &sDataLength );

            IDE_TEST_RAISE( idlOS::strncmp( sBuffer2, sBuffer, sDataLength ) != 0, err_lockCurrupted )

            sIsGetLock = ID_TRUE;
            break;
        }
        else
        {
            /* 4.2 ������ ��� �ٸ� ��尡 ���� �ڽ� ��带 ������ lock�� ���� ���̹Ƿ�
             *    �����ð� ��� �� 1������ ������Ѵ�.*/
            idlOS::sleep( 1 );
            sWaitingTime++;

            IDE_TEST_RAISE( sWaitingTime > ZOOKEEPER_LOCK_WAIT_TIMEOUT, err_waitingTime );
        }
    }
    zookeeperLockDump((SChar*)"get meta lock end success", aLockType, ID_FALSE);
    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( err_waitingTime )
    {
        logAndSetErrMsg( ZKC_WAITING_TIMEOUT );        
    }
    IDE_EXCEPTION( err_lockCurrupted )
    {
        logAndSetErrMsg( ZKC_LOCK_ERROR );
    }
    IDE_EXCEPTION_END;

    zookeeperLockDump((SChar*)"get meta lock end failure", aLockType, ID_FALSE);
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::releaseZookeeperMetaLock     *
 * ------------------------------------------------------------------*
 * getZookeeperMetaLock() �Լ����� ���� lock�� �����Ѵ�.
 *********************************************************************/
void sdiZookeeper::releaseZookeeperMetaLock()
{
    idBool sIsGetZookeeperLock = ID_FALSE;

    sIsGetZookeeperLock = (idBool)idCore::acpAtomicGet32( & mIsGetZookeeperLock );
    if( ( sIsGetZookeeperLock == ID_TRUE ) && ( mIsConnect == ID_TRUE ) )
    {
        idCore::acpAtomicSet( &mIsGetZookeeperLock,
                              ID_FALSE );

        (void)releaseMetaLock( SDI_ZKS_META_LOCK );
    }

    /* ���������� commit�� �۾��� �Ϸ��� �� �� �ƴ϶� ����ó���� ����� ��쿡�� *
     * RunningJob�� �ʱ�ȭ���� ���� �۾��� �浹���� �ʵ��� �Ѵ�.                 */
    mRunningJobType = ZK_JOB_NONE;
    idlOS::memset( mTargetNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::releaseShardMetaLock         *
 * ------------------------------------------------------------------*
 * getShardMetaLock() �Լ����� ���� lock�� �����Ѵ�.
 *********************************************************************/
void sdiZookeeper::releaseShardMetaLock()
{
    idBool sIsGetShardLock = ID_TRUE;
    /* R2HA to be remove */
    if ( SDU_SHARD_ZOOKEEPER_TEST == 1 )
    {
        sIsGetShardLock = (idBool)idCore::acpAtomicGet32( &mIsGetShardLock );

        if ( SDU_SHARD_LOCAL_FORCE != 1)
        {
            if( ( sIsGetShardLock == ID_TRUE ) && ( mIsConnect == ID_TRUE ) )
            {
                idCore::acpAtomicSet32( &mIsGetShardLock,
                                        ID_FALSE );

                (void)releaseMetaLock( SDI_ZKS_SHARD_LOCK );
            }
        }
    }
    
    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::releaseMetaLock              *
 * ------------------------------------------------------------------*
 * getMetaLock �Լ����� ���� lock�� �����Ѵ�.
 * �ش� �Լ��� ���� �ӽ� ��尡 ���ŵǳ�
 * watch ����� �ƴϹǷ� �˶��� ���� �ʴ´�.
 *********************************************************************/
IDE_RC sdiZookeeper::releaseMetaLock( ZookeeperLockType aLockType )
{
    ZKCResult   sResult;
    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt        sDataLength = SDI_ZKC_BUFFER_SIZE;
    SChar     * sOwnerCheck = NULL;

    zookeeperLockDump((SChar*)"release lock begin", aLockType, ID_FALSE);

    IDE_DASSERT( ( aLockType == SDI_ZKS_META_LOCK ) || ( aLockType == SDI_ZKS_SHARD_LOCK ) );

    if( aLockType == SDI_ZKS_META_LOCK )
    {
        idlOS::strncpy( sPath, 
                        SDI_ZKC_PATH_META_LOCK,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sPath, 
                        SDI_ZKC_PATH_DISPLAY_LOCK,
                        ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
        /* sLockPath�� '/altibase_shard/cluster_meta/zookeeper_meta_lock' �̰�
         * sNodePath�� '/altibase_shard/cluster_meta/zookeeper_meta_lock/locked' �̴�. */
    }
    else
    {
        idlOS::strncpy( sPath,
                        SDI_ZKC_PATH_SHARD_META_LOCK,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sPath,
                        SDI_ZKC_PATH_DISPLAY_LOCK,
                        ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
        /* 'sLockPath�� /altibase_shard/cluster_meta/shard_meta_lock' �̰�
         * 'sNodePath�� /altibase_shard/cluster_meta/shard_meta_lock/locked' �̴�. */
    }
    /* ������ lock�� �ڽ��� ���� ���� �´��� Ȯ���Ѵ�. */
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    sOwnerCheck = idlOS::strtok( sBuffer, "/" );
    IDE_TEST_RAISE( sOwnerCheck == NULL , err_NotMyLock );
    IDE_TEST_RAISE( idlOS::strncmp( sOwnerCheck, mMyNodeName, sDataLength ) != 0, err_NotMyLock );

    /* �ӽ� ��带 �����Ͽ� lock�� �����Ѵ�. */
    sResult = aclZKC_deleteNode( mZKHandler, sPath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    zookeeperLockDump((SChar*)"release lock end success", aLockType, ID_FALSE);
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( err_NotMyLock )
    {
        logAndSetErrMsg( ZKC_LOCK_ERROR );
    }
    IDE_EXCEPTION_END;
    zookeeperLockDump((SChar*)"release lock end failure", aLockType, ID_FALSE);
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::validateDB                   *
 * ------------------------------------------------------------------*
 * ���� �����Ϸ����ϴ� DB�� char set�� ZKS�� ����� �Ͱ� �������� Ȯ���Ѵ�.
 * �ϳ��� ��ġ���� ���� ��� aIsValid�� ID_FALSE�� �����Ѵ�.
 * �ش� �Լ��� DB �����ÿ��� ȣ��ǹǷ� ZKS�� ������� ���� ��Ȳ������ ȣ��� �� �ִ�.
 *
 * aDBName               - [IN]  ���� ������ DB�� �̸�
 * aCharacterSet         - [IN]  ���� ������ DB�� character set
 * aNationalCharacterSet - [IN] ���� ������ DB�� national character set
 *********************************************************************/
IDE_RC sdiZookeeper::validateDB( SChar  * aDBName,
                                 SChar  * aCharacterSet,
                                 SChar  * aNationalCharacterSet )
{
    ZKCResult sResult;

    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt      sDataLength = SDI_ZKC_BUFFER_SIZE;
    idBool    sIsConnect = ID_FALSE;

    /* �ش��Լ��� create Database ���� ȣ��� �� �����Ƿ� ZKS�� ������� ���� ���¿����� ȣ��� �� �ִ�. *
     * ����  ZKS�� ����Ǿ� ���� �ʴٸ� �ӽ÷� ��������ش�.                                             */
    if( mIsConnect == ID_FALSE )
    {
        if( connect( ID_TRUE ) != IDE_SUCCESS )
        {
            /* ������ ������ ��� ������ ���� �����Ƿ� valid ó���Ѵ�. */
            IDE_CONT( normal_exit );
        }
        else
        {
            sIsConnect = ID_TRUE;
        }
    }

    /* 1. DB�̸��� ���Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_DB_NAME,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/sharded_database_name'�� ���� ������. */

    IDE_TEST_CONT( mZKHandler == NULL, normal_exit );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    /* ����� NO_NODE��� Ŭ������ ��Ÿ�� ���� ���� ��Ȳ�̹Ƿ� ���� �ʿ䰡 ����. */
    IDE_TEST_CONT( sResult == ZKC_NO_NODE, normal_exit );

    /* �� ��쿡�� �߰����� �����޽����� ����� ����ó���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���� ���Ѵ�. */
    IDE_TEST_RAISE( idlOS::strncmp( sBuffer,
                                    aDBName,
                                    idlOS::strlen( aDBName ) ) != 0,
                    validate_Fail );

    /* 2. character set�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath, 
                    SDI_ZKC_PATH_CHAR_SET, 
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/character_set'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���� ���Ѵ�. */
    IDE_TEST_RAISE( idlOS::strncmp( sBuffer,
                                    aCharacterSet,
                                    idlOS::strlen( aCharacterSet ) ) != 0, 
                    validate_Fail );

    /* 3. national character set�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NAT_CHAR_SET,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/national_character_set'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���� ���Ѵ�. */
    IDE_TEST_RAISE( idlOS::strncmp( sBuffer,
                                    aNationalCharacterSet,
                                    idlOS::strlen( aNationalCharacterSet ) ) != 0,
                    validate_Fail );

    /* �ӽÿ����� ��� ������ �����Ѵ�. */
    if( ( mIsConnect == ID_FALSE ) || ( sIsConnect == ID_TRUE ) )
    {
        disconnect();
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( validate_Fail )
    {
        logAndSetErrMsg( ZKC_DB_VALIDATE_FAIL );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    /* �ӽÿ����� ��� ������ �����Ѵ�. */
    if( mIsConnect == ID_FALSE )
    {
        disconnect();
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::validateNode                 *
 * ------------------------------------------------------------------*
 * Ŭ�����Ϳ� �߰��Ǵ� ����� ��Ÿ������ ZKS�� ����� �Ͱ� ��ġ�ϴ��� Ȯ���Ѵ�.
 * �ϳ��� ��ġ���� ���� ��� ���� ó���� ƨ�� ���´�.
 * �߰���, ���� ������ ���� ������ �̸��� ��尡 �̹� �ִ����� üũ�Ѵ�.
 * �ش��Լ��� ZKS�� ����� ���Ŀ� ����� �����ϴ�.
 *
 * aDBName                   - [IN]  DB�� �̸�
 * aCharacterSet             - [IN]  DB�� character set
 * aNationalCharacterSet     - [IN]  DB�� national character set
 * aKSafety                  - [IN]  k-safety ��
 * aReplicationMode          - [IN]  replication mode(10,11,12 �� �ϳ�)
 * aParallelCount            - [IN]  ����ȭ ������ ó���ϴ� applier�� ��
 * aBinaryVersion            - [IN]  binary version(sm version)
 * aShardVersion             - [IN]  shard version
 * aTransTBLSize             - [IN]  Ʈ����� ���̺��� ũ��
 * aNodeName                 - [IN]  ���� ������ ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::validateNode( SChar  * aDBName,
                                   SChar  * aCharacterSet,
                                   SChar  * aNationalCharacterSet,
                                   SChar  * aKSafety,
                                   SChar  * aReplicationMode,
                                   SChar  * aParallelCount,
                                   SChar  * aBinaryVersion,
                                   SChar  * aShardVersion,
                                   SChar  * aTransTBLSize,
                                   SChar  * aNodeName )
{
    ZKCResult sResult;

    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt      sDataLength;
    const UInt sErrMsgBufferLen = 1024;
    SChar      sErrMsgBuffer[sErrMsgBufferLen] = {0,};

    /* 1. DB�̸��� ���Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_DB_NAME,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/sharded_database_name'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    if( sResult != ZKC_SUCCESS )
    {
        if( sResult == ZKC_NO_NODE )
        {
            /* �� ��쿡�� Ŭ������ ��Ÿ�� ���� ���� ��Ȳ�̹Ƿ� ���� �ʿ䰡 ����. */
            IDE_CONT( normal_exit );
        }
        else
        {
            IDE_RAISE( err_ZKC );
        }
    }

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aDBName, idlOS::strlen( aDBName ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local sharded database name[%s] is different with cluster's[%s]",
                        aDBName,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 2. character set�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CHAR_SET,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/character_set'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aCharacterSet, idlOS::strlen( aCharacterSet ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local database character set[%s] is different with cluster's[%s]",
                        aCharacterSet,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 3. national character set�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NAT_CHAR_SET,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/national_character_set'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aNationalCharacterSet, idlOS::strlen( aNationalCharacterSet ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local database national character set[%s] is different with cluster's[%s]",
                        aNationalCharacterSet,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 4. K-safety�� ���Ѵ�. */
    if( aKSafety != NULL )
    {
        idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sPath,
                        SDI_ZKC_PATH_K_SAFETY,
                        SDI_ZKC_PATH_LENGTH );
        /* sPath�� '/altibase_shard/cluster_meta/validation/k-safety'�� ���� ������. */

        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* ���������� �����Դٸ� ���Ѵ�. */
        if( idlOS::strncmp( sBuffer, aKSafety, idlOS::strlen( aKSafety ) ) != 0 )
        {
            idlOS::snprintf(sErrMsgBuffer,
                            sErrMsgBufferLen,
                            "local database k-safety[%s] is different with cluster's[%s]",
                            aKSafety,
                            sBuffer);
            IDE_RAISE( validate_fail );
        }
    }
    else
    {
        /* replication ����( k-safety, replication mode, parallel count )�� ����ִ� ���    *
         * ���������� validation�� ����ó�� �� �� ���� �޾� �� ����� ��Ÿ�� �����ؾ� �Ѵ�.  */
    }

    /* 5. replication_mode�� ���Ѵ�. */
    if( aReplicationMode != NULL )
    {
        idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sPath,
                        SDI_ZKC_PATH_REP_MODE,
                        SDI_ZKC_PATH_LENGTH );
        /* sPath�� '/altibase_shard/cluster_meta/validation/replication_mode'�� ���� ������. */

        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* ���������� �����Դٸ� ���Ѵ�. */
        if( idlOS::strncmp( sBuffer, aReplicationMode, idlOS::strlen( aReplicationMode ) ) != 0 )
        {
            idlOS::snprintf(sErrMsgBuffer,
                            sErrMsgBufferLen,
                            "local database replication mode[%s] is different with cluster's[%s]",
                            aReplicationMode,
                            sBuffer);
            IDE_RAISE( validate_fail );
        }
    }
    else
    {
        /* replication ����( k-safety, replication mode, parallel count )�� ����ִ� ���    *
         * ���������� validation�� ����ó�� �� �� ���� �޾� �� ����� ��Ÿ�� �����ؾ� �Ѵ�.  */
    }

    /* 6. parallel count�� ���Ѵ�. */
    if( aParallelCount != NULL )
    {
        idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sPath,
                        SDI_ZKC_PATH_PARALLEL_CNT,
                        SDI_ZKC_PATH_LENGTH );
        /* sPath�� '/altibase_shard/cluster_meta/validation/parallel_count'�� ���� ������. */

        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler, 
                                  sPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* ���������� �����Դٸ� ���Ѵ�. */
        if( idlOS::strncmp( sBuffer, aParallelCount, idlOS::strlen( aParallelCount ) ) != 0 )
        {
            idlOS::snprintf(sErrMsgBuffer,
                            sErrMsgBufferLen,
                            "local database replication parallel count[%s] is different with cluster's[%s]",
                            aParallelCount,
                            sBuffer);
            IDE_RAISE( validate_fail );
        }
    }
    else
    {
        /* replication ����( k-safety, replication mode, parallel count )�� ����ִ� ���    *
         * ���������� validation�� ����ó�� �� �� ���� �޾� �� ����� ��Ÿ�� �����ؾ� �Ѵ�.  */
    }

    /* 7. binary version�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath, 
                    SDI_ZKC_PATH_BINARY_VERSION,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/binary_version'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aBinaryVersion, idlOS::strlen( aBinaryVersion ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local database binary version[%s] is different with cluster's[%s]",
                        aBinaryVersion,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 8. shard version�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_SHARD_VERSION,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/shard_version'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aShardVersion, idlOS::strlen( aShardVersion ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local database shard version[%s] is different with cluster's[%s]",
                        aShardVersion,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 9. trans TBL size�� ���Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_TRANS_TBL_SIZE,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/validation/trans_TBL_size'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* ���������� �����Դٸ� ���Ѵ�. */
    if( idlOS::strncmp( sBuffer, aTransTBLSize, idlOS::strlen( aTransTBLSize ) ) != 0 )
    {
        idlOS::snprintf(sErrMsgBuffer,
                        sErrMsgBufferLen,
                        "local database transaction table size[%s] is different with cluster's[%s]",
                        aTransTBLSize,
                        sBuffer);
        IDE_RAISE( validate_fail );
    }

    /* 10. ���� ������ ���� ������ �̸��� ��尡 �ִ��� üũ�Ѵ�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aNodeName]/state'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );

    if( sResult == ZKC_SUCCESS )
    {
        if( ( stateStringConvert( sBuffer ) ) == ZK_ADD )
        {
            /* �̹� ������ ��尡 �����ϰ� �ش� ����� ���°� ADD��� ���� ADD ���� ������ ���̹Ƿ� * 
             * ���� �������� ���� ������ ����� �����Ѵ�. �ϴ� ����� ���ó�� �Ѵ�.                */
        }
        else
        {
            /* �̹� ������ �̸��� ��尡 �����ϰ� ���� ���� �� */
            IDE_RAISE( err_alreadyExist );
        }
    }
    else
    {
        IDE_TEST_RAISE( sResult != ZKC_NO_NODE, err_ZKC );
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( validate_fail )
    {
        logAndSetErrMsg( ZKC_VALIDATE_FAIL, sErrMsgBuffer );
    }
    IDE_EXCEPTION( err_alreadyExist )
    {
        logAndSetErrMsg( ZKC_NODE_EXISTS );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdiZookeeper::getKSafetyInfo( SChar  * aKSafety,
                                     SChar  * aReplicationMode,
                                     SChar  * aParallelCount )
{
    ZKCResult sResult;
    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt      sDataLength;

    IDE_DASSERT( aKSafety != NULL );
    /* sPath�� '/altibase_shard/cluster_meta/validation/k-safety'�� ���� ������. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_K_SAFETY,
                    SDI_ZKC_PATH_LENGTH );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                                sPath,
                                sBuffer,
                                &sDataLength );
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    idlOS::strncpy( aKSafety, sBuffer, sDataLength );

    IDE_DASSERT( aReplicationMode != NULL );
    /* sPath�� '/altibase_shard/cluster_meta/validation/replication_mode'�� ���� ������. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_REP_MODE,
                    SDI_ZKC_PATH_LENGTH );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                                sPath,
                                sBuffer,
                                &sDataLength );
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    idlOS::strncpy( aReplicationMode, sBuffer, sDataLength );

    IDE_DASSERT( aParallelCount != NULL );
    /* sPath�� '/altibase_shard/cluster_meta/validation/parallel_count'�� ���� ������. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_PARALLEL_CNT,
                    SDI_ZKC_PATH_LENGTH );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    idlOS::strncpy( aParallelCount, sBuffer, sDataLength );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::validateState                *
 * ------------------------------------------------------------------*
 * Ư�� ��忡 ���� ���� ������ �����Ϸ��� �� ��
 * �ش� ����� ���� ���¿� ���Ͽ� ������ ���º������� üũ�Ѵ�.
 * �ش� �Լ��� ZKS�� ����� ���Ŀ� ����� �����ϴ�.
 * �ش� �Լ��� �ڽ� ���� �ٸ� ��忡 ���ؼ��� ������ �� �ִ�.
 *
 * aState    - [IN]  �����Ϸ��� ����
 * aNodeName - [IN]  ���¸� �����Ϸ��� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::validateState( ZKState   aState, 
                                    SChar   * aNodeName )
{
    ZKCResult sResult;

    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt      sDataLength;

    ZKState   sOldState;
    idBool    sIsAlive;

    if( ( aState == ZK_NONE ) || ( aState == ZK_ADD ) )
    {
        /* �����Ϸ��� ���°� ADD�� DROP�� ��쿡�� üũ�� �ʿ䰡 ����.  */
        IDE_CONT( normal_exit );
    }

    /* 1. ��� ����� state�� ���´�. */
    idlOS::strncpy( sPath, 
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aNodeName]'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );
                              
    if( sResult != ZKC_SUCCESS )
    {
        /* ��� ��尡 �������� ���� ��� */
        if( sResult == ZKC_NO_NODE )
        {
            IDE_CONT( normal_exit );
        }
        else
        {
            IDE_RAISE( err_ZKC );
        }
    }

    sOldState = stateStringConvert( sBuffer );

    /* 2. ��� ����� connection_info�� ���´�. */
    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );
    /* sPath�� '/altibase_shard/connection_info/[aNodeName]'�� ���� ������. */

    switch( sResult )
    {
        case ZKC_SUCCESS:
            sIsAlive = ID_TRUE;
            break;
        case ZKC_NO_NODE:
            sIsAlive = ID_FALSE;
            break;
        default:
            IDE_RAISE( err_ZKC );
            break;
    }
    
    /* 3. ��� ����� state�� connection_info�� ������ �������� ������������ üũ�Ѵ�. */
    if( sOldState == ZK_ADD ) 
    {
        /* ���� ���°� ADD�� ��� DROP�ܿ��� RUN������ ���游 �����ϴ�. */
        IDE_TEST_RAISE( aState != ZK_RUN, validate_Fail );
    }
    else if( ( sOldState == ZK_RUN ) && ( sIsAlive == ID_TRUE ) )
    {
        /* ���� ���°� RUN(alive)�� ��� FAILOVER, SHUTDOWN�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_FAILOVER ) && 
                        ( aState != ZK_SHUTDOWN ) &&
                        ( aState != ZK_KILL ) , validate_Fail );
    }
    else if( ( sOldState == ZK_RUN ) && ( sIsAlive == ID_FALSE ) )
    {
        /* ���� ���°� RUN(die)�� ��쿡�� RUN/FAILBACK�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_RUN ) && ( aState != ZK_FAILBACK ), validate_Fail );
    }
    else if( sOldState == ZK_FAILBACK )
    {
        /* ���� ���°� FAILBACK�� ��쿡�� RUN/FAILOVER/FAILBACK�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_RUN ) && ( aState != ZK_FAILOVER ) && ( aState != ZK_FAILBACK ), validate_Fail );
    }
    else if( sOldState == ZK_FAILOVER )
    {
        /* ���� ���°� FAILOVER�� ��쿡�� RUN/FAILBACK�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_RUN ) && ( aState != ZK_FAILBACK ), validate_Fail );
    }
    else if( sOldState == ZK_SHUTDOWN )
    {
        /* ���� ���°� SHUTDOWN�� ��쿡�� JOIN �� failover�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_JOIN ) && ( aState != ZK_FAILOVER ), validate_Fail );
    }
    else if( sOldState == ZK_JOIN )
    {
        /* ���� ���°� JOIN�� ��쿡�� RUN�� �����ϴ�. */
        IDE_TEST_RAISE( aState != ZK_RUN, validate_Fail );
    }
    else if( sOldState == ZK_KILL )
    {
        /* ���� ���°� KILL�� ��쿡�� RUN/FAILBACK�� �����ϴ�. */
        IDE_TEST_RAISE( ( aState != ZK_RUN ) && ( aState != ZK_FAILBACK ), validate_Fail );
    }
    else
    {
        /* ����ó�� �� assert */
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( validate_Fail )
    {
        logAndSetErrMsg( ZKC_STATE_VALIDATE_FAIL );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::initClusterMeta              *
 * ------------------------------------------------------------------*
 * Ŭ������ ��Ÿ�� ��������� Ŭ������ ��Ÿ�� �����Ѵ�.
 * �ش� �Լ��� �������� ����ϴ� /altibase_shard/cluster_meta ������ ������ �����Ѵ�.
 * �ش� �Լ��� multi op�� �̿��� cluster meta�� �����Ѵ�.
 *
 * aShardDBInfo     - [IN]  ������ Ŭ������ ��Ÿ�� ����
 * aLocalMetaInfo   - [IN]  ������ Ŭ������ ��Ÿ�� ���� �� replication ���� ����
 *********************************************************************/
IDE_RC sdiZookeeper::initClusterMeta( sdiDatabaseInfo   aShardDBInfo,
                                      sdiLocalMetaInfo aLocalMetaInfo)
/* [BUGBUG] aLocalMetaInfo���� replication ������ ������ ����ϳ� PROJ-2726 ���� ��ÿ���                 *
   aLocalMetaInfo�� ���� replication ������ ���� ������ compiler waring�� ���� ���� ����� ���� ó���Ѵ�. */
{
    ZKCResult   sResult;

    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt        sDataLength;
    SChar       sShardVersion[10] = {0,};
    SChar       sTransTBLSize[10] = {0,};
    SChar               sKSafetyStr[SDI_KSAFETY_STR_MAX_SIZE + 1] = {0,};
    SChar               sReplicationModeStr[SDI_REPLICATION_MODE_MAX_SIZE + 1] = {0,};
    SChar               sParallelCountStr[SDI_PARALLEL_COUNT_STR_MAX_SIZE + 1] = {0,};

    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[19];
    zoo_op_result_t sOpResult[19];

    /* cluster meta�� �����ϱ� �� �ٸ� client�� ���� ���� �Ǿ��� �� ������ �ѹ� �� Ȯ���Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_META_LOCK,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/zookeeper_meta_lock'�� ���� ������. '*/

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler, 
                              sPath,
                              sBuffer,
                              &sDataLength );

    if( sResult == ZKC_SUCCESS )
    {
        /* �� ���� �ٸ� client�� ���� cluster meta�� ������ �����̹Ƿ� ���� �� �ʿ䰡 ����. */
        IDE_CONT( normal_exit );
    }
    
    /* cluster meta�� �ѹ��� �����ϱ� ���� multi op�� ó���� operation�� �����Ѵ�. */
    zoo_create_op_init( &sOp[0],                      /* operation */
                        SDI_ZKC_PATH_ALTIBASE_SHARD,  /* path */
                        NULL,                         /* value */
                        -1,                           /* value length */
                        &ZOO_OPEN_ACL_UNSAFE,         /* ACL */
                        ZOO_PERSISTENT,               /* flag */
                        NULL,                         /* path_buffer */
                        0 );                          /* path_buffer_len */
    /* sOp[0]�� '/altibase_shard'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[1],
                        SDI_ZKC_PATH_CLUSTER_META,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[1]�� '/altibase_shard/cluster_meta'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[2],
                        SDI_ZKC_PATH_META_LOCK,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[2]�� '/altibase_shard/cluster_meta/zookeeper_meta_lock'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[3],
                        SDI_ZKC_PATH_VALIDATION,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[3]�� '/altibase_shard/cluster_meta/validation'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[4],
                        SDI_ZKC_PATH_DB_NAME,
                        aShardDBInfo.mDBName,   /* DB ������ �Է��� DB�� �̸� */
                        idlOS::strlen( aShardDBInfo.mDBName ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[4]�� '/altibase_shard/cluster_meta/validation/sharded_database_name'�� path�� �����Ѵ�. */

    IDE_TEST_RAISE( idlOS::uIntToStr( aLocalMetaInfo.mKSafety,
                                      sKSafetyStr,
                                      SDI_KSAFETY_STR_MAX_SIZE + 1 ) != 0, ERR_INVALID_KSAFETY );
    zoo_create_op_init( &sOp[5],
                        SDI_ZKC_PATH_K_SAFETY,
                        sKSafetyStr,   /* ���纻�� �� */
                        idlOS::strlen( sKSafetyStr ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[5]�� '/altibase_shard/cluster_meta/validation/k-safety'�� path�� �����Ѵ�. */

    IDE_TEST_RAISE( idlOS::uIntToStr( aLocalMetaInfo.mReplicationMode,
                                      sReplicationModeStr,
                                      SDI_REPLICATION_MODE_MAX_SIZE + 1) != 0, ERR_INVALID_KSAFETY );
    zoo_create_op_init( &sOp[6],
                        SDI_ZKC_PATH_REP_MODE,
                        sReplicationModeStr,/* replication mode */
                        idlOS::strlen( sReplicationModeStr ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[6]�� '/altibase_shard/cluster_meta/validation/replication_mode'�� path�� �����Ѵ�. */

    IDE_TEST_RAISE( idlOS::uIntToStr( aLocalMetaInfo.mParallelCount,
                                      sParallelCountStr,
                                      SDI_PARALLEL_COUNT_STR_MAX_SIZE + 1) != 0, ERR_INVALID_KSAFETY );
    zoo_create_op_init( &sOp[7],
                        SDI_ZKC_PATH_PARALLEL_CNT,
                        sParallelCountStr,   /* parallel count */
                        idlOS::strlen( sParallelCountStr ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[7]�� '/altibase_shard/cluster_meta/validation/parallel_count'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[8],
                        SDI_ZKC_PATH_CHAR_SET,
                        aShardDBInfo.mDBCharSet,   /* character set */
                        idlOS::strlen( aShardDBInfo.mDBCharSet ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[8]�� '/altibase_shard/cluster_meta/validation/character_set'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[9],
                        SDI_ZKC_PATH_NAT_CHAR_SET,
                        aShardDBInfo.mNationalCharSet,   /* national character set */
                        idlOS::strlen( aShardDBInfo.mNationalCharSet ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[9]�� '/altibase_shard/cluster_meta/validation/national_character_set'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[10],
                        SDI_ZKC_PATH_BINARY_VERSION,
                        aShardDBInfo.mDBVersion,   /* binary version */
                        idlOS::strlen( aShardDBInfo.mDBVersion ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[10]�� '/altibase_shard/cluster_meta/validation/binary_version'�� path�� �����Ѵ�. */

    idlOS::snprintf( sShardVersion, 10, "%"ID_INT32_FMT".%"ID_INT32_FMT".%"ID_INT32_FMT,
                    aShardDBInfo.mMajorVersion, aShardDBInfo.mMinorVersion, aShardDBInfo.mPatchVersion );
    zoo_create_op_init( &sOp[11],
                        SDI_ZKC_PATH_SHARD_VERSION,
                        sShardVersion,   /* shard version */
                        idlOS::strlen( sShardVersion ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[11]�� '/altibase_shard/cluster_meta/validation/shard_version'�� path�� �����Ѵ�. */

    idlOS::snprintf( sTransTBLSize, 10, "%"ID_UINT32_FMT, aShardDBInfo.mTransTblSize );
    zoo_create_op_init( &sOp[12],
                        SDI_ZKC_PATH_TRANS_TBL_SIZE,
                        sTransTBLSize,  /* trans TBL size */
                        idlOS::strlen( sTransTBLSize ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[12]�� '/altibase_shard/cluster_meta/validation/trans_TBL_size'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[13],
                        SDI_ZKC_PATH_FAILOVER_HISTORY,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[13]�� '/altibase_shard/cluster_meta/failover_history'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[14],
                        SDI_ZKC_PATH_SMN,
                        NULL,   /* Ŭ������ ��Ÿ �����ÿ��� SMN�� ����ִ�. */
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[14]�� '/altibase_shard/cluster_meta/validation/SMN'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[15],
                        SDI_ZKC_PATH_FAULT_DETECT_TIME,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[15]�� '/altibase_shard/cluster_meta/validation/fault_detection_time'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[16],
                        SDI_ZKC_PATH_SHARD_META_LOCK,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[16]�� '/altibase_shard/cluster_meta/validation/shard_meta_lock'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[17],
                        SDI_ZKC_PATH_CONNECTION_INFO,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[17]�� '/altibase_shard/cluster_meta/connection_info'�� path�� �����Ѵ�. */

    zoo_create_op_init( &sOp[18],
                        SDI_ZKC_PATH_NODE_META,
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );
    /* sOp[18]�� '/altibase_shard/node_meta'�� path�� �����Ѵ�. */

    /* ������ operation���� multi op�� �ѹ��� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 19, sOp, sOpResult );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i <= 18; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_KSAFETY )
    {
        ideSetErrorCode( sdERR_ABORT_SDI_INVALID_KSAFETY );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdiZookeeper::finalizeClusterMeta()
{
    IDE_TEST( removeRecursivePathAndInfo((SChar*)"/altibase_shard") != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::addNode                      *
 * ------------------------------------------------------------------*
 * ��尡 �߰��ɶ� ��� ��Ÿ�� �ش� ��忡 ���� ������ �߰��Ѵ�.
 * �ش� �Լ��� /altibase_shard/node_meta/����̸� ������ ������ �����Ѵ�.
 * ���� Ŭ������ ��Ÿ�� ������� ��쿡�� ����ó���Ѵ�.
 * �ش� �Լ��� �����ϱ� �� validateNode �Լ��� ���� ������ add�� �������� üũ�ؾ� �Ѵ�.
 * �ش� �Լ������� SMN�� state�� �������� �����Ƿ� �� ����
 * ���� updateSMN�� updateState�� ȣ���Ͽ� ó���ؾ� �Ѵ�.
 *
 * aShardNodeID         - [IN] �߰��� ����� �ĺ��� ID
 * aNodeIPnPort         - [IN] �߰��� ����� �ܺ� IP �� Port
 * aInternalNodeIPnPort - [IN] �߰��� ����� ���� IP �� Port
 * aInternalR...IPnPort - [IN] �߰��� ����� replication ���� IP �� Port
 * aConnType            - [IN] �߰��� ����� internal ���� ���
 *********************************************************************/
IDE_RC sdiZookeeper::addNode( SChar * aShardNodeID,
                              SChar * aNodeIPnPort,
                              SChar * aInternalNodeIPnPort,
                              SChar * aInternalReplicationHostIPnPort,
                              SChar * aConnType )
{
    ZKCResult       sResult;
    
    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sPath[8][SDI_ZKC_PATH_LENGTH] = {{0,},};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt            sDataLength;
    ZKState         sState;

    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[8];
    zoo_op_result_t sOpResult[8];

    idlOS::strncpy( sNodePath,
                    SDI_ZKC_PATH_NODE_META, 
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    mMyNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    /* sNodePath�� '/altibase_shard/node_meta/[mMyNodeName]'�� ���� ������. */

    /* 1. �߰��� ���� ������ �̸��� ��尡 �ִ��� Ȯ���Ѵ�. */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sNodePath,
                              sBuffer,
                              &sDataLength );

    /* �߰��� ���� ������ �̸��� ��尡 ���� ��� */
    if( sResult == ZKC_SUCCESS )
    {
        /* �ش� ����� ���¸� üũ�Ѵ�. */
        idlOS::strncpy( sPath[0],
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sPath[0],
                        SDI_ZKC_PATH_NODE_STATE,
                        ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
        /* sPath[0]�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */       
 
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sPath[0],
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sState = stateStringConvert( sBuffer );

        if( sState == ZK_ADD )
        {
            /* �ش� ��尡 ADD ���� ���������� ���ŵ��� ���� �����̹Ƿ� �����ϰ� �����Ѵ�. */
            IDE_TEST( dropNode( mMyNodeName ) != IDE_SUCCESS );
        }
        else
        {
            /* �������� ���� �̸��� ��ħ */
            IDE_RAISE( err_alreadyExist );
        }
    }
    else
    {
        IDE_TEST_RAISE( sResult != ZKC_NO_NODE, err_ZKC );
    }

    /* 3. ���� �߰��� ����� ��� ��Ÿ�� multi op�� �����Ѵ�. */
    idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[0],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[0]�� '/altibase_shard/node_meta/[mMyNodeName]'�� ���� ������. */

    zoo_create_op_init( &sOp[0],                 /* operation */
                        sPath[0],                /* path */
                        NULL,                    /* value */
                        -1,                      /* value length */
                        &ZOO_OPEN_ACL_UNSAFE,    /* ACL */
                        ZOO_PERSISTENT,          /* flag */
                        NULL,                    /* path_buffer */
                        0 );                     /* path_buffer_len */

    idlOS::strncpy( sPath[1],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[1],
                    SDI_ZKC_PATH_NODE_ID,
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    /* sPath[1]�� '/altibase_shard/node_meta/[mMyNodeName]/shard_node_id'�� ���� ������. */

    zoo_create_op_init( &sOp[1],
                        sPath[1],
                        aShardNodeID,
                        idlOS::strlen( aShardNodeID ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[2],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[2],
                    SDI_ZKC_PATH_NODE_IP,
                    ID_SIZEOF( sPath[2] ) - idlOS::strlen( sPath[2] ) - 1 );

    /* sPath[2]�� '/altibase_shard/node_meta/[mMyNodeName]/node_ip:port'�� ���� ������. */
    zoo_create_op_init( &sOp[2],
                        sPath[2],
                        aNodeIPnPort,
                        idlOS::strlen( aNodeIPnPort ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[3],
                    sNodePath, 
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[3],
                    SDI_ZKC_PATH_NODE_INTERNAL_IP,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    /* sPath[3]�� '/altibase_shard/node_meta/[mMyNodeName]/internal_node_ip:port'�� ���� ������. */

    zoo_create_op_init( &sOp[3],
                        sPath[3],
                        aInternalNodeIPnPort,
                        idlOS::strlen( aInternalNodeIPnPort ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[4],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[4],
                    SDI_ZKC_PATH_NODE_REPL_HOST_IP,
                    ID_SIZEOF( sPath[4] ) - idlOS::strlen( sPath[4] ) - 1 );
    /* sPath[4]�� '/altibase_shard/node_meta/[mMyNodeName]/internal_replication_host_ip:port'�� ���� ������. */

    zoo_create_op_init( &sOp[4],
                        sPath[4],
                        aInternalReplicationHostIPnPort,
                        idlOS::strlen( aInternalReplicationHostIPnPort ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[5], 
                    sNodePath, 
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[5],
                    SDI_ZKC_PATH_NODE_CONN_TYPE,
                    ID_SIZEOF( sPath[5] ) - idlOS::strlen( sPath[5] ) - 1 );
    /* sPath[5]�� '/altibase_shard/node_meta/[mMyNodeName]/conn_type'�� ���� ������. */

    zoo_create_op_init( &sOp[5],
                        sPath[5],
                        aConnType,
                        idlOS::strlen( aConnType ),
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[6],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[6],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[6] ) - idlOS::strlen( sPath[6] ) - 1 );
    /* sPath[6]�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    zoo_create_op_init( &sOp[6],
                        sPath[6],
                        SDI_ZKC_STATE_ADD,
                        3,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    idlOS::strncpy( sPath[7],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[7],
                    SDI_ZKC_PATH_NODE_FAILOVER_TO,
                    ID_SIZEOF( sPath[7] ) - idlOS::strlen( sPath[7] ) - 1 );
    /* sPath[7]�� '/altibase_shard/node_meta/[mMyNodeName]/failoverTo'�� ���� ������. */

    zoo_create_op_init( &sOp[7],
                        sPath[7],
                        NULL,
                        -1,
                        &ZOO_OPEN_ACL_UNSAFE,
                        ZOO_PERSISTENT,
                        NULL,
                        0 );

    /* 4. multi op�� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 8, sOp, sOpResult );

    /* multi op�� ������ �� operation�� ���� ���� ���θ� Ȯ���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i <= 7; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    /* 5. connection_info�� �߰��Ѵ�. */
    IDE_TEST( addConnectionInfo() != IDE_SUCCESS );

    /* 6. watch�� �Ǵ�. */
    IDE_TEST( settingAddNodeWatch() != IDE_SUCCESS );
    IDE_TEST( settingAllDeleteNodeWatch() != IDE_SUCCESS );

    /* mm ��⿡�� commit�� addNodeAfterCommit�� ȣ���� �� �ֵ��� ���� ���� ���� �۾��� add���� ǥ���Ѵ�. */
    mRunningJobType = ZK_JOB_ADD;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_alreadyExist )
    {
        logAndSetErrMsg( ZKC_NODE_EXISTS );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::addNodeAfterCommit           *
 * ------------------------------------------------------------------*
 * ADD �۾��� �Ϸ�Ǿ� commit �� �� ȣ��ȴ�.
 * �� ����� ���¸� RUN���� �����ϰ� Ŭ�������� SMN ���� ������ �� lock�� �����Ѵ�.
 * state�� SMN�� multi op�� �ѹ��� �����Ѵ�.
 *
 * aSMN    - [IN] ������ Ŭ������ ��Ÿ�� SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::addNodeAfterCommit( ULong aSMN )
{
    ZKCResult   sResult;
    SChar       sPath[2][SDI_ZKC_PATH_LENGTH] = {{0,},};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[2];
    zoo_op_result_t sOpResult[2];

    IDE_DASSERT( mRunningJobType == ZK_JOB_ADD );
    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );

    /* state�� �����ϱ� ���� path�� �����Ѵ�. */
    idlOS::strncpy( sPath[0],
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0],
                    "/",
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    idlOS::strncat( sPath[0],
                    mMyNodeName,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    idlOS::strncat( sPath[0],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    /* SMN�� �����ϱ� ���� path�� ��(aSMN)�� �����Ѵ�. */
    idlOS::strncpy( sPath[1],
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[1]�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. */
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aSMN );

    /* multi op�� �����Ѵ�. */

    /* state */
    zoo_set_op_init( &sOp[0],
                     sPath[0],
                     SDI_ZKC_STATE_RUN,
                     idlOS::strlen( SDI_ZKC_STATE_RUN ),
                     -1,
                     NULL );

    /* SMN */
    zoo_set_op_init( &sOp[1],
                     sPath[1],
                     sBuffer,
                     idlOS::strlen( sBuffer ),
                     -1,
                     NULL );

    /* multi op�� ������ �ѹ��� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 2, sOp, sOpResult );

    /* ���� ���� ���θ� Ȯ���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i <= 1; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::dropNode                     *
 * ------------------------------------------------------------------*
 * Ŭ������ ��Ÿ���� Ư�� ����� ������ �����Ѵ�.
 * �ش� �Լ��� �ڽ� �� �ƴ϶� Ÿ ��忡 ���ؼ��� ������ �����ϴ�.
 * �ش� �Լ������� ��� ����� state�� DROP���� �ٲٴ� �۾��� �����Ѵ�.
 * ���� ���� �۾��� commit�� ȣ��Ǵ� dropNodeAfterCommit���� ����ȴ�.
 *
 * aNodeName - [IN] Ŭ������ ��Ÿ���� ������ ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::dropNode( SChar * aNodeName )
{
    ZKCResult       sResult;

    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1);
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aNodeName]/state'�� ���� ������. */

    /* �ش� ����� ���¸� DROP���� �����Ѵ�. */
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_DROP, 
                    SDI_ZKC_BUFFER_SIZE );

    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* mm ��⿡�� commit�� dropNodeAfterCommit�� ȣ���� �� �ֵ��� ���� ���� ���� �۾��� drop���� ǥ���Ѵ�. */
    mRunningJobType = ZK_JOB_DROP;
    idlOS::strncpy( mTargetNodeName, 
                    aNodeName, 
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::dropNodeAfterCommit          *
 * ------------------------------------------------------------------*
 * DROP �۾��� �Ϸ�Ǿ� commit �� �� ȣ��ȴ�.
 * �ش� ����� ��� ��Ÿ�� ������ �� failover history / Ŭ�������� SMN ���� �����Ѵ�.
 * ���� DROP �۾��� ����� �ڽ��̶�� lock�� Ǯ�� zookeeper���� ������ �����Ѵ�. 
 *
 * aSMN      - [IN]  ������ Ŭ������ ��Ÿ�� SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::dropNodeAfterCommit( ULong     aSMN )
{
    ZKCResult       sResult;
    SChar           sPath[2][SDI_ZKC_PATH_LENGTH] = {{0,},};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar         * sStringPtr = NULL;
    SChar           sOldFailoverHistory[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sNewFailoverHistory[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sTargetNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SInt            sDataLength;
    idBool          sIsDead = ID_FALSE;
    idBool          sIsAlloc = ID_FALSE;
    iduList       * sNodeInfoList;
    UInt            sNodeCnt ;
    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[2];
    zoo_op_result_t sOpResult[2];

    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );
    IDE_DASSERT( mRunningJobType == ZK_JOB_DROP );
    IDE_DASSERT( idlOS::strlen( mTargetNodeName ) > 0 );

    idlOS::strncpy( sTargetNodeName,
                    mTargetNodeName,
                    idlOS::strlen( mTargetNodeName ) );

    /* ���� ��� ��尡 ��� �ִ� �������� üũ�Ѵ�. */
    idlOS::strncpy( sPath[0],
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0],
                    "/",
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    idlOS::strncat( sPath[0],
                    mTargetNodeName,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/connection_info/[mTargetNodeName]'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath[0],
                              sBuffer,
                              &sDataLength );

    /* ���� ��� ��尡 �׾��ִ� ���¶�� failover history���� �ش� ����� ������ �����ؾ� �� �� �ִ�. */
    if( sResult == ZKC_NO_NODE )
    {
        sIsDead = ID_TRUE;

        idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sPath[0],
                        SDI_ZKC_PATH_FAILOVER_HISTORY,
                        SDI_ZKC_PATH_LENGTH );
        /* sPath[0]�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. '*/

        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sPath[0],
                                  sOldFailoverHistory,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        if( sDataLength > 0 )
        {
            sStringPtr = idlOS::strtok( sOldFailoverHistory, "/" );

            /* failover history�� [����̸�]:[SMN]/[����̸�]:[SMN]/...�� �������� �Ǿ� �����Ƿ�  *
             * �պκи� ���Ѵٸ� ��� �̸��� ���������� ���� �� �ִ�.                         */
            if( isNodeMatch( sStringPtr, mTargetNodeName ) == ID_TRUE )
            {
                /* ��� ����� failover history ������ �о��ٸ� �ش� ������ �����Ѵ�. */
            }
            else
            {
                /* ��� ��尡 �ƴ� �ٸ� ����� failover history ������ �о��ٸ� *
                 * �� ������ �״�� new Failover histroy�� �����Ѵ�.             */
                idlOS::strncpy( sNewFailoverHistory,
                                sStringPtr,
                                SDI_ZKC_BUFFER_SIZE );
            }

            while( sStringPtr != NULL )
            {
                sStringPtr = idlOS::strtok( NULL, "/" );

                if( sStringPtr != NULL )
                {
                    if( isNodeMatch( sStringPtr, mTargetNodeName ) == ID_TRUE )
                    {
                        /* ��� ����� failover history ������ �о��ٸ� �ش� ������ �����Ѵ�. */
                    }
                    else
                    {
                        /* ��� ��尡 �ƴ� �ٸ� ����� failover history ������ �о��ٸ� *
                         * �� ������ �״�� new Failover histroy�� �����Ѵ�.             */

                        if( idlOS::strlen( sNewFailoverHistory ) > 1 )
                        {
                            idlOS::strncat( sNewFailoverHistory,
                                            "/",
                                            ID_SIZEOF( sNewFailoverHistory ) - idlOS::strlen( sNewFailoverHistory ) - 1 );
                        }

                        idlOS::strncat( sNewFailoverHistory,
                                        sStringPtr,
                                        ID_SIZEOF( sNewFailoverHistory ) - idlOS::strlen( sNewFailoverHistory ) - 1 );
                    }
                }
                else
                {
                    /* failover history�� ������ �� �� */
                }
            }
        }
        else
        {
            /* failover history�� ����ִٸ� �ش� ���� ������ �ƴ϶� SHUTDOWN�� �����̴�. */
            sIsDead = ID_FALSE;
        }
    }
    else
    {
        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    }

    /* ��Ÿ���� ��� ������ �����Ѵ�. */
    IDE_TEST( removeNode( mTargetNodeName ) != IDE_SUCCESS );

    /* SMN�� �����ϱ� ���� path�� ��(aSMN)�� �����Ѵ�. */
    idlOS::strncpy( sPath[0],
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[0]�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. */
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aSMN );

    idlOS::strncpy( sPath[1],
                    SDI_ZKC_PATH_FAILOVER_HISTORY,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[1]�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. */

    /* multi op�� �����Ѵ�. */

    /* SMN */
    zoo_set_op_init( &sOp[0],
                     sPath[0],
                     sBuffer,
                     idlOS::strlen( sBuffer ),
                     -1,
                     NULL );

    /* failover history */

    if( sNewFailoverHistory != NULL )
    {
        zoo_set_op_init( &sOp[1],
                         sPath[1],
                         sNewFailoverHistory,
                         idlOS::strlen( sNewFailoverHistory ),
                         -1,
                         NULL );
    }
    else
    {
        zoo_set_op_init( &sOp[1],
                         sPath[1],
                         NULL,
                         -1,
                         -1,
                         NULL );       
    }

    /* multi op�� ������ �ѹ��� �����Ѵ�. */
    if( sIsDead == ID_FALSE )
    {
        /* ��� ��尡 ����ִٸ� state�� SMN�� �����Ѵ�. */
        sResult = aclZKC_multiOp( mZKHandler, 1, sOp, sOpResult );

        /* ���� ���� ���θ� Ȯ���Ѵ�. */
        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
        IDE_TEST_RAISE( sOpResult[0].err != ZOK, err_ZKC );
    }
    else
    {
        /* ��� ��尡 �׾��ִٸ� failover history�� �����ؾ� �Ѵ�. */
        sResult = aclZKC_multiOp( mZKHandler, 2, sOp, sOpResult );

        /* ���� ���� ���θ� Ȯ���Ѵ�. */
        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        for( i = 0; i <= 1; i++ )
        {
            IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
        }
    }

    IDE_TEST( getAllNodeInfoList( &sNodeInfoList,
                                  &sNodeCnt )
              != IDE_SUCCESS );
    sIsAlloc = ID_TRUE;

    if (sNodeCnt == 0)
    {
        IDE_TEST(finalizeClusterMeta() != IDE_SUCCESS);
    }
    else
    {
        releaseShardMetaLock();
        releaseZookeeperMetaLock();
    }
    /* ���ŵ� ��尡 �ڽ��̶�� ������ ���´�. */
    /* metalock�� �����ϸ鼭 mTargetNodeName�� �ʱ�ȭ�ϹǷ� �̸� �����ص� sTargetNodeName�� ���ؾ� �Ѵ�. */
    if( isNodeMatch( sTargetNodeName, mMyNodeName ) == ID_TRUE )
    {
        sdiZookeeper::disconnect();
    }

    freeList( sNodeInfoList, SDI_ZKS_LIST_NODEINFO );

    /* shard meta�� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( sdi::resetShardMetaWithDummyStmt() != IDE_SUCCESS );
    /* ���� local node ������ �����ؾ� �Ѵ�.(node id ������Ʈ�� ó���� ����)*/

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    if (sIsAlloc == ID_TRUE)
    {
        freeList( sNodeInfoList, SDI_ZKS_LIST_NODEINFO );
    }
    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::shutdown                     *
 * ------------------------------------------------------------------*
 * Ŭ������ ��Ÿ�� �ش� ��尡 SHUTDOWN �������� ǥ���Ѵ�.
 *********************************************************************/
IDE_RC sdiZookeeper::shutdown()
{
    ZKCResult   sResult;

    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    /* ����Ǿ� ���� �ʴٸ� zookeeper ��Ÿ�� ������ �ʿ䰡 ����. */
    if( isConnect() != ID_TRUE )
    {
        IDE_CONT( normal_exit );
    }
    /* 1. zookeeper_meta_lock�� lock�� ��´� */
    IDE_TEST( getZookeeperMetaLock(ID_NULL_SESSION_ID) != IDE_SUCCESS );

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/", 
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    mMyNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    /* 2. shutdown�� ������ �������� Ȯ���Ѵ�. */
    IDE_TEST( validateState( ZK_SHUTDOWN, 
                             mMyNodeName ) != IDE_SUCCESS );

    /* 3. state�� shutdown���� �����Ѵ�. */
    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_SHUTDOWN,
                    SDI_ZKC_BUFFER_SIZE );
    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* 4. zookeeper_meta_lock�� lock�� �����Ѵ�. */
    releaseZookeeperMetaLock();

    /* 5. ZKS�� ������ ���´�. */
    disconnect();

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    releaseZookeeperMetaLock();

    return IDE_FAILURE; 
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::joinNode                     *
 * ------------------------------------------------------------------*
 * Ŭ������ ��Ÿ�� �ش� ��尡 JOIN �������� ǥ���� �� watch�� �Ǵ�.
 *********************************************************************/
IDE_RC sdiZookeeper::joinNode()
{
    ZKCResult   sResult;

    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

     /* 1. join�� ������ �������� Ȯ���Ѵ�. */
    IDE_TEST( validateState( ZK_JOIN, 
                             mMyNodeName ) != IDE_SUCCESS );

    /* 3. ��Ÿ ������ �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    mMyNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sNodePath�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_JOIN,
                    SDI_ZKC_BUFFER_SIZE );

    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );
    
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* 4. ���� ������ �����Ѵ�. */
    IDE_TEST( addConnectionInfo() != IDE_SUCCESS );
   
    /* 5. watch�� �Ǵ�. */
    IDE_TEST( settingAddNodeWatch() != IDE_SUCCESS );
    IDE_TEST( settingAllDeleteNodeWatch() != IDE_SUCCESS );

    /* mm ��⿡�� commit�� joinNodeAfterCommit�� ȣ���� �� �ֵ��� ���� ���� ���� �۾��� join���� ǥ���Ѵ�. */
    mRunningJobType = ZK_JOB_JOIN;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::joinNodeAfterCommit          *
 * ------------------------------------------------------------------*
 * JOIN �۾��� �Ϸ�Ǿ� commit �� �� ȣ��ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::joinNodeAfterCommit()
{
    SChar sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );
    IDE_DASSERT( mRunningJobType == ZK_JOB_JOIN );

    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_RUN,
                    SDI_ZKC_BUFFER_SIZE );

    IDE_TEST( updateState( sBuffer,
                           mMyNodeName ) != IDE_SUCCESS );

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::reshardingJob                *
 * ------------------------------------------------------------------*
 * �� ��忡�� ���� �۾��� resharding �������� ǥ��
 *********************************************************************/
void sdiZookeeper::reshardingJob()
{
    mRunningJobType = ZK_JOB_RESHARD;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failoverForWatcher           *
 * ------------------------------------------------------------------*
 * FAILOVER�� �����ϰ� �׿� ���� Ŭ������ ��Ÿ�� �����Ѵ�.
 * �ش� �Լ��� watcher�� ���� �ڵ������� ȣ��ȴ�.
 * failoverForQuery �Լ��ʹ� �޸� validate �۾��� �߰��� ����� �Ѵ�.
 * ���� : ��� ����� �� ��嵵 failover�� �ʿ��ϴٸ� �߰��� failover�� �����Ѵ�.
 *
 * aNodeName - [IN] FAILOVER�� ������ ���� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::failoverForWatcher( SChar * aNodeName )
{
    ZKCResult       sResult;
    idBool          sIsAlive  = ID_FALSE;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sPrevNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar           sCheckNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SInt            sDataLength;
    SInt            sDeadNodeCnt = 0;
    ZKState         sState;
    SInt            i = 0;
    ULong           sWaitTime;

    iduList       * sDeadNodeList = NULL;
    iduListNode   * sDeadNode     = NULL;
    SChar         * sDeadNodeName = NULL;
    SChar           sNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};

    iduListNode   * sIterator = NULL;

    idBool          sIsAlloc = ID_FALSE;
    SInt            sAllocState = 0;
    idBool          sIsLocked = ID_FALSE;

    IDE_DASSERT( idlOS::strlen( aNodeName ) > 0 );

    IDE_TEST( getZookeeperMetaLock( ID_UINT_MAX ) != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    /* 1. failover ������� validate�� �����Ѵ�. */
    /* �� ����� state�� üũ�� failover�� ������ �������� Ȯ���Ѵ�. */
    IDE_TEST( validateState( ZK_FAILOVER,
                             mMyNodeName ) != IDE_SUCCESS );

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aNodeName]/state'�� ���� ������. */

    /* watcher�� ���� ȣ��� ��� DROP�̳� SHUTDOWN���� ���� ���� ���� ���Ŷ��� ȣ��ǹǷ� *
     * DROP�̳� SHUTDOWN���� ���� ���� ���� �������� Ȯ���ؾ� �Ѵ�.                        */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );

    if( sResult == ZKC_NO_NODE )
    {
        /* �ش� ��尡 DROP�Ǿ� ���ŵǾ��ų� ADD ���� ���� �� ���ܰ� �߻��� ��Ȳ���� *
         * �ٸ� ��尡 ���� �ش� ��带 failover�� ������ ��� ��Ÿ�� ���ŵ� ���·�  *
         * �� ��쿡�� failover�� ������ �ʿ䰡 ����. */
        IDE_CONT( normal_exit );
    }
    else
    {
        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    }

    sState = stateStringConvert( sBuffer );

    if ( ( sState == ZK_SHUTDOWN ) || ( sState == ZK_KILL ) )
    {
        /* ZK_SHUTDOWN : �ش� ��尡 SHUTDOWN�Ǿ� ������ ���� ���̹Ƿ� failover�� ������ �ʿ䰡 ����. */
        /* ZK_KILL     : Failover ������ ���� ���� KILL �̹Ƿ� FailoverForWatcher ����� �ƴϴ�. */
        IDE_CONT( normal_exit );
    }
    if ( sState == ZK_ADD )
    {
        /* ���� ��尡 ADD �۾� �� �׾��� ��� zookeeper ��Ÿ�� ������ �����ϱ⸸ �ϸ� �ȴ�. */
        IDE_TEST( removeNode( aNodeName ) != IDE_SUCCESS );

        IDE_CONT( normal_exit );
    }
    else
    {
        IDE_ERROR( sState != ZK_ERROR );
    }

    /* �ش� ��尡 �̹� failover �Ǿ����� Ȯ���Ѵ�. */
    sDataLength = SDI_ZKC_BUFFER_SIZE;
    IDE_TEST( getNodeInfo( aNodeName,
                           (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                           sBuffer,
                           &sDataLength ) != IDE_SUCCESS );

    if( sDataLength > 0 )
    {
        /* �̹� failover�� �Ǿ��ִ� �����̹Ƿ� ���� �۾��� �ʿ䰡 ����. */
        IDE_CONT( normal_exit );
    }

    /* Lock Ǯ�� ��ٸ���. */
    sIsLocked = ID_FALSE;
    releaseZookeeperMetaLock();

    /* Delete �� Node�� Re-Connect�� �����Ͽ��� �� �ִ�. ����Ѵ�. */
    sWaitTime = mTickTime * mSyncLimit * mServerCnt; // *2

    /* sWaitTime�� millisecond �̴�. */
    idlOS::sleep((UInt) (sWaitTime / 1000) );

    /* �ٽ� lock ��� Ȯ���Ѵ�. */
    IDE_TEST( getZookeeperMetaLock( ID_UINT_MAX ) != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    /* ��� ���ܼ� �� ���� ������ �ִ�. NodeAliveCheck�� �����Ѵ�. */
    IDE_TEST( checkNodeAlive( aNodeName,
                              &sIsAlive ) != IDE_SUCCESS );

    IDE_TEST_CONT( sIsAlive == ID_TRUE, normal_exit );

//R2HA Merge�� ���� �ϴ� Failover�� ���´�.
//    sIsLocked = ID_FALSE;
//    releaseZookeeperMetaLock();

//    return IDE_SUCCESS;

    /* failover �ؾ� �ϴ� ��Ȳ�̴�. Failover Target List�� �����Ѵ�. */

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF( iduList ),
                                 (void**)&(sDeadNodeList),
                                 SDI_ZKC_ALLOC_WAIT_TIME ) != IDE_SUCCESS );
    IDU_LIST_INIT( sDeadNodeList );

    sIsAlloc = ID_TRUE;

    /* 3. ���� ��� �ܿ��� failover�� �����ؾ� �ϴ� ��尡 �ִ��� üũ�Ѵ�. */

    /* ���� ��尡 failover�� �����ϴ� ���̾��ų� failover ��û�� �޾�����  *
     * failover�� �������� ���ϰ� ���� ��츦 ����Ͽ� ���� ��� �ܿ���     *
     * failover�� �����ؾ� �ϴ� ��尡 �ִ��� üũ�ؾ� �Ѵ�.                */
    idlOS::strncpy( sCheckNodeName,
                    mMyNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );
    // sPrevNodeName �ʱ�ȭ �ʿ�?

    /* ���� : sCheckNodeName�� �̹� üũ�� ���� ��带 �ǹ��Ѵ�.                                  *
     *        �� loop���� ���������� üũ�ϴ� ����� sChecNodeName�� �� ����� sPrevNodeName�̴�. */
    while( isNodeMatch( mMyNodeName, sPrevNodeName ) != ID_TRUE )  
    {
        /* ��� ����� �� ��尡 ����ִ��� Ȯ���Ѵ�. */
        IDE_TEST( getPrevNode( sCheckNodeName, sPrevNodeName, &sState ) != IDE_SUCCESS );

        IDE_TEST( checkNodeAlive( sPrevNodeName,
                                  &sIsAlive ) != IDE_SUCCESS );

        if( sIsAlive == ID_TRUE )
        {
            /* ��������� ���� Ȯ��. */
            idlOS::strncpy( sCheckNodeName,
                            sPrevNodeName,
                            SDI_NODE_NAME_MAX_SIZE + 1 );

            continue;
        }
        else
        {
            /* �׾����� ��� �ش� ����� ���¿� failoverTo�� üũ�Ѵ�. */

            if( sState == ZK_SHUTDOWN )
            {
                /* �׾����� ��쿡�� ����� ���°� SHUTDOWN �� ��쿡�� failover ����� �ƴϹǷ� *
                 * �ش� ���� �����ϰ� �� ��忡 ���� �ٽ� �����Ѵ�.                            */
                idlOS::memset( sCheckNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
                idlOS::strncpy( sCheckNodeName,
                                sPrevNodeName,
                                SDI_NODE_NAME_MAX_SIZE + 1 );
            }
            else if( sState == ZK_ADD )
            {
                /* �� ��尡 ADD �۾� �� �׾��� ��� zookeeper ��Ÿ�� ������ �����ϱ⸸ �ϸ� �ȴ�. */
                IDE_TEST( removeNode( sPrevNodeName ) != IDE_SUCCESS );

                /* �� ��尡 ���ŵǾ����Ƿ� sCheckNodeName�� �� ��尡 ����Ǿ��� ���̴�. */
            }
            else
            {
                /* failoverTo�� üũ�Ͽ� failover�� �̹� ����Ǿ����� Ȯ���Ѵ�. */
                sDataLength = SDI_ZKC_BUFFER_SIZE;
                IDE_TEST( getNodeInfo( sPrevNodeName,
                                       (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                       sBuffer,
                                       &sDataLength ) != IDE_SUCCESS );

                if( sDataLength > 0 )
                {
                    /* failoverTo�� ������� ���� ��� failover�� �̹� ����� ���̹Ƿ�
                     * �ش� ���� �����ϰ� �� ��忡 ���� �ٽ� �����Ѵ�. */
                    idlOS::memset( sCheckNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
                    idlOS::strncpy( sCheckNodeName,
                                    sPrevNodeName,
                                    SDI_NODE_NAME_MAX_SIZE + 1 );
                }
                else
                {
                    /* ������� �Դٸ� ���� ���� failover�� ���� ����̴�. */
                    sDeadNodeCnt++;

                    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                                 ID_SIZEOF( iduListNode ),
                                                 (void**)&(sDeadNode),
                                                 SDI_ZKC_ALLOC_WAIT_TIME ) != IDE_SUCCESS );
                    sAllocState = 1;

                    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                                 SDI_NODE_NAME_MAX_SIZE + 1,
                                                 (void**)&(sDeadNodeName),
                                                 SDI_ZKC_ALLOC_WAIT_TIME ) != IDE_SUCCESS );
                    sAllocState = 2;

                    idlOS::strncpy( sDeadNodeName,
                                    sPrevNodeName,
                                    SDI_NODE_NAME_MAX_SIZE + 1 );

                    IDU_LIST_INIT_OBJ( sDeadNode, (void*)sDeadNodeName );

                    /* List �� Last�� �����Ѵ�. ������ �����ϱ� ����. */
                    IDU_LIST_ADD_LAST( sDeadNodeList, sDeadNode );

                    sAllocState = 0;

                    /* ���� �� ��忡 ���ؼ��� �ٽ� �����Ѵ�. */
                    idlOS::memset( sCheckNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
                    idlOS::strncpy( sCheckNodeName,
                                    sPrevNodeName,
                                    SDI_NODE_NAME_MAX_SIZE + 1 );
                }
            }
        }
    }

    /* Failover List �ۼ��� ������. 
     * Failover �������� Meta Lock �⵵�� Ǯ���ش�.
     * ���� ����Ǵ� Failover�� �ִٸ� ���� ���ο��� Lock ȹ�� �� Ȯ���Ѵ�. */
    sIsLocked = ID_FALSE;
    releaseZookeeperMetaLock();

    /* 4. failover �۾��� �����Ѵ�. */
    // for check
    i = 0;
    IDU_LIST_ITERATE( sDeadNodeList, sIterator )
    {
        idlOS::memset( sNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::strncpy( sNodeName,
                        (SChar *) sIterator->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        mRunningJobType = ZK_JOB_FAILOVER_FOR_WATCHER;
        idlOS::strncpy( mTargetNodeName,
                        sNodeName,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* failover�� �����Ѵ�. */
        /* Zookeeper Thread�� idtBaseThread�� ������ ���� �ʾƼ� �������� ������ �߻��Ѵ�.
         * ���� Thread�� �����ؼ� �����ϵ��� �Ѵ�. */
        IDE_TEST( sdiFOThread::failoverExec( mMyNodeName,
                                             mTargetNodeName,
                                             1 ) != IDE_SUCCESS );

        i++;
    }

    /* for check */
    IDE_DASSERT( i == sDeadNodeCnt );

    if ( sIsAlloc == ID_TRUE )
    {
        sIsAlloc = ID_FALSE;
        freeList( sDeadNodeList, SDI_ZKS_LIST_NODENAME );
    }

    IDE_EXCEPTION_CONT( normal_exit );

    if ( sIsLocked == ID_TRUE )
    {
        releaseZookeeperMetaLock();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    /* list�� ���� �߰����� ���� �޸� �Ҵ��� �����Ѵ�. */
    switch( sAllocState )
    {
        case 2:
            (void)iduMemMgr::free( sDeadNodeName );
        case 1:
            (void)iduMemMgr::free( sDeadNode );
        case 0:
            break;
        default:
            break;
    }

    if ( sIsAlloc == ID_TRUE )
    {
        freeList( sDeadNodeList, SDI_ZKS_LIST_NODENAME );
    }

    if ( sIsLocked == ID_TRUE ) 
    {
        releaseZookeeperMetaLock();
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failoverForQuery             *
 * ------------------------------------------------------------------*
 * FAILOVER�� �����ϰ� �׿� ���� Ŭ������ ��Ÿ�� �����Ѵ�.
 * �ش� �Լ��� ALTER DATABASE SHARD FAILOVER ������ ���� ȣ��ȴ�.
 * failoverForWatcher �Լ��ʹ� �޸� qdsd::validateShardFailover �Լ�����
 * validation�� ���������Ƿ� ���� ���� �ʿ䰡 ����.
 * �ش� �Լ������� �� ����� state�� FAILOVER�� �ٲٴ� �۾��� �����Ѵ�.
 * ���� failover �۾��� commit�� ȣ��Ǵ� failoverAfterCommit���� ����ȴ�.
 * ���� : failoverForWatcher�ʹ� �޸� ��� ��� �ϳ��� failover�� ����ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::failoverForQuery( SChar * aNodeName,
                                       SChar * aFailoverNodeName )
{
    ZKCResult   sResult;
    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    /* �� state�� FAILOVER�� �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    mMyNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_FAILOVER,
                    SDI_ZKC_BUFFER_SIZE );

    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* mm ��⿡�� commit�� failoverAfterCommit�� ȣ���� �� �ֵ��� ���� ���� ���� �۾��� failover���� ǥ���Ѵ�. */
    mRunningJobType = ZK_JOB_FAILOVER;
    idlOS::strncpy( mTargetNodeName, 
                    aNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFailoverNodeName, 
                    aFailoverNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failoverAfterCommit          *
 * ------------------------------------------------------------------*
 * FAILOVER ������ �Ϸ�Ǿ� commit �� �� ȣ��ȴ�.
 * watcher�� ���� failover �۾��� ����� ��쿡�� ȣ����� �ʴ´�.a
 * (������ �ʿ��� ���) fault detection time�� ���� �� ��
 * failoverTo / failover history / Ŭ�������� SMN�� multi op�� ���ÿ� �����Ѵ�.
 *
 * aSMN      - [IN] ������ Ŭ������ ��Ÿ�� SMN ��
 * aSMN      - [IN] �����ϱ� �� Ŭ������ ��Ÿ�� SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::failoverAfterCommit( ULong   aNewSMN, ULong aBeforeSMN )
{
    ZKCResult   sResult;

    SChar       sPath[5][SDI_ZKC_PATH_LENGTH] = {{0,},};
    SChar       sFailoverHistory[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt        sDataLength;
    SLong       sDeadtime;

    SChar       sSqlStr[128];

    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[5];
    zoo_op_result_t sOpResult[5];

    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );
    IDE_DASSERT( ( mRunningJobType == ZK_JOB_FAILOVER ) || ( mRunningJobType == ZK_JOB_FAILOVER_FOR_WATCHER ) );
    IDE_DASSERT( idlOS::strlen( mTargetNodeName ) > 0 );

    /* 1. fault detection time�� Ȯ���ϰ� ����ִٸ� �߰��Ѵ�. */
    idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[0],
                    SDI_ZKC_PATH_FAULT_DETECT_TIME,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[0]�� '/altibase_shard/cluster_meta/fault_detection_time'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath[0],
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    if( sDataLength > 0 )
    {
        /* �̹� �ٸ� ����� ���� ����ִٸ� ������ �ʿ䰡 ����. */
    }
    else
    {
        /* fault dectection time�� ������� ��� ���� �ð��� �����Ѵ�. */
        sDeadtime = mtc::getSystemTimezoneSecond();

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_INT32_FMT, sDeadtime );

        sResult = aclZKC_setInfo( mZKHandler,
                                  sPath[0],
                                  sBuffer,
                                  idlOS::strlen( sBuffer ) );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    }

    /* '���� ����̸�:���� SMN' ������ ���� ��忡 ���� failover_history�� �����. */
    idlOS::strncpy( sFailoverHistory,
                    mTargetNodeName,
                    SDI_ZKC_BUFFER_SIZE );
    idlOS::strncat( sFailoverHistory,
                    ":",
                    ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aBeforeSMN );

    idlOS::strncat( sFailoverHistory,
                    sBuffer,
                    ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aNewSMN );

    idlOS::strncat( sFailoverHistory,
                    ":",
                    ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );
    idlOS::strncat( sFailoverHistory,
                    sBuffer,
                    ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );

    /* ������ failover_history�� ������ ��ģ��. */
    idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[0],
                    SDI_ZKC_PATH_FAILOVER_HISTORY,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[0]�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath[0],
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    IDE_TEST_RAISE( sDataLength + idlOS::strlen( sFailoverHistory ) >= SDI_ZKC_BUFFER_SIZE, too_long_info );

    /* failover history�� �ٸ� ����� ������ �̹� �ִٸ� /�� ������ ��� �Ѵ�. */
    if( sDataLength > 0 )
    {
        idlOS::strncat( sFailoverHistory,
                        "/",
                        ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );
        idlOS::strncat( sFailoverHistory,
                        sBuffer,
                        ID_SIZEOF( sFailoverHistory ) - idlOS::strlen( sFailoverHistory ) - 1 );
    }

    /* ���� ����� failoverTo�� �߰��Ѵ�. */
    idlOS::memset( sPath[1], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[1],
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[1],
                    "/",
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    idlOS::strncat( sPath[1],
                    mTargetNodeName,
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    idlOS::strncat( sPath[1],
                    SDI_ZKC_PATH_NODE_FAILOVER_TO,
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    /* sPath�� '/altibase_shard/cluster_meta/[mTargetNodeName]/failoverTo'�� ���� ������. */

    /* SMN�� �����ϱ� ���� path�� ��(aNewSMN)�� �����Ѵ�. */
    idlOS::strncpy( sPath[2],
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[2]�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. */
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aNewSMN );

    /* state�� �����ϱ� ���� path�� �����Ѵ�. */
    idlOS::strncpy( sPath[3],
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[3],
                    "/",
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    idlOS::strncat( sPath[3],
                    mMyNodeName,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    idlOS::strncat( sPath[3],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    /* sPath[3]�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    /* TargetNode�� state�� �����ϱ� ���� path�� �����Ѵ�. */
    idlOS::strncpy( sPath[4],
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[4],
                    "/",
                    ID_SIZEOF( sPath[4] ) - idlOS::strlen( sPath[4] ) - 1 );
    idlOS::strncat( sPath[4],
                    mTargetNodeName,
                    ID_SIZEOF( sPath[4] ) - idlOS::strlen( sPath[4] ) - 1 );
    idlOS::strncat( sPath[4],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[4] ) - idlOS::strlen( sPath[4] ) - 1 );
    /* sPath[4]�� '/altibase_shard/node_meta/[mTargetNodeName]/state'�� ���� ������. */


    /* multi op�� �����Ѵ�. */
    /* failover history */
    zoo_set_op_init( &sOp[0],
                     sPath[0],
                     sFailoverHistory,
                     idlOS::strlen( sFailoverHistory ),
                     -1,
                     NULL );

    /* failoverTo */
    zoo_set_op_init( &sOp[1],
                     sPath[1],
                     mFailoverNodeName, /* Failover �ذ� Node�� �̸��� ��� */
                     idlOS::strlen( mFailoverNodeName ),
                     -1,
                     NULL );

    /* SMN */
    zoo_set_op_init( &sOp[2],
                     sPath[2],
                     sBuffer,
                     idlOS::strlen( sBuffer ),
                     -1,
                     NULL );

    /* state */
    zoo_set_op_init( &sOp[3],
                     sPath[3],
                     SDI_ZKC_STATE_RUN,
                     idlOS::strlen( SDI_ZKC_STATE_RUN ),
                     -1,
                     NULL );

    /* Target Node state */
    zoo_set_op_init( &sOp[4],
                     sPath[4],
                     SDI_ZKC_STATE_KILL,
                     idlOS::strlen( SDI_ZKC_STATE_KILL ),
                     -1,
                     NULL );

                    
    /* multi op�� ������ �ѹ��� �����Ѵ�. */
    /* state ����� lock �������� ��� �����ؾ� �Ѵ�. */    
    sResult = aclZKC_multiOp( mZKHandler, 5, sOp, sOpResult );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i < 5; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    /* RP Stop�� �ʿ��ϴٸ� Stop �Ѵ�. */
    if ( idlOS::strncmp( mFirstStopNodeName,
                         SDM_NA_STR,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
    {
        idlOS::snprintf( sSqlStr, 128,
                         "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP", 
                         mFirstStopReplName );

        IDE_TEST( sdi::shardExecTempSQLWithoutSession( sSqlStr,
                                                       mFirstStopNodeName,
                                                       0,
                                                       QCI_STMT_MASK_MAX )
                  != IDE_SUCCESS );
    }

    if ( idlOS::strncmp( mSecondStopNodeName,
                         SDM_NA_STR,
                         SDI_NODE_NAME_MAX_SIZE + 1 ) != 0 )
    {
        idlOS::snprintf( sSqlStr, 128,
                         "ALTER REPLICATION "QCM_SQL_STRING_SKIP_FMT" STOP", 
                         mSecondStopReplName );

        IDE_TEST( sdi::shardExecTempSQLWithoutSession( sSqlStr,
                                                       mSecondStopNodeName,
                                                       0,
                                                       QCI_STMT_MASK_MAX )
                  != IDE_SUCCESS );
    }

    /* Stop �������� �ʱ�ȭ. */
    idlOS::strncpy( mFirstStopNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFirstStopReplName,
                    SDM_NA_STR,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopReplName,
                    SDM_NA_STR,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFailoverNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_info )
    {
        /* failover history�� 128�ڸ� �Ѱ��� ��� */
        IDE_SET( ideSetErrorCode( sdERR_ABORT_ZKC_FAILOVER_HISTORY_TOO_LONG ) );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failoverAfterRollback          *
 * ------------------------------------------------------------------*
 * FAILOVER ������ Rollback �� �� ȣ��ȴ�.
 * FailoverNode�� State �� mFailoverNodeName�� �ʱ�ȭ �ؾ��Ѵ�.
 * mRunningJobType/mTargetNodeName �� releaseZookeeperMetaLock���� ó���Ѵ�.
 * Create�� RP�� ������ DROP �ؾ��Ѵ�.
 *
 *********************************************************************/
IDE_RC sdiZookeeper::failoverAfterRollback()
{
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );
    IDE_DASSERT( ( mRunningJobType == ZK_JOB_FAILOVER ) || ( mRunningJobType == ZK_JOB_FAILOVER_FOR_WATCHER ) );
    IDE_DASSERT( idlOS::strlen( mTargetNodeName ) > 0 );

    idlOS::strncpy( mFailoverNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    IDE_TEST( validateState( ZK_RUN,
                             mMyNodeName ) != IDE_SUCCESS );

    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_RUN,
                    SDI_ZKC_BUFFER_SIZE );

    IDE_TEST( updateState( sBuffer,
                           mMyNodeName ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    idlOS::strncpy( mFailoverNodeName,
                    SDM_NA_STR,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failback                     *
 * ------------------------------------------------------------------*
 * FAILBACK�� �����ϰ� �׿� ���� Ŭ������ ��Ÿ�� �����Ѵ�.
 * �ش� �Լ��� ALTER DATABASE SHARD FAILBACK ������ ���� ȣ��ȴ�.
 * �ش� �Լ������� �� ����� state�� FAILBACK���� �ٲٴ� �۾��� �����Ѵ�.
 * ������ ��Ÿ�� �����Ű�� �۾��� commit�� ȣ��Ǵ� failbackAfterCommit���� ����ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::failback()
{
    ZKCResult   sResult;
    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    /* �� state�� FAILBACK���� �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    mMyNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[mMyNodeName]/state'�� ���� ������. */

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    idlOS::strncpy( sBuffer,
                    SDI_ZKC_STATE_FAILBACK,
                    SDI_ZKC_BUFFER_SIZE );

    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* mm ��⿡�� commit�� failbackAfterCommit�� ȣ���� �� �ֵ��� ���� ���� ���� �۾��� failback���� ǥ���Ѵ�. */
    mRunningJobType = ZK_JOB_FAILBACK;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::failbackAfterCommit          *
 * ------------------------------------------------------------------*
 * FAILBACK �۾��� �Ϸ�Ǿ� commit �� �� ȣ��ȴ�.
 * failover history / fault detection time / Ŭ�������� SMN�� �����Ѵ�.
 *
 * aSMN    - [IN] ������ Ŭ������ ��Ÿ�� SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::failbackAfterCommit( ULong aSMN )
{
    ZKCResult       sResult;

    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sPath[4][SDI_ZKC_PATH_LENGTH] = {{0,}};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sFailoverHistory[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar         * sCheckBuffer;
    SChar         * sOtherFailoverHistory;
    SInt            sDataLength;

    /* multi op */
    SInt            i = 0;
    zoo_op_t        sOp[4];
    zoo_op_result_t sOpResult[4];

    IDE_DASSERT( mIsGetZookeeperLock == ID_TRUE );
    IDE_DASSERT( mRunningJobType == ZK_JOB_FAILBACK );

    idlOS::strncpy( sNodePath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    mMyNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    /* sNodePath�� '/altibase_shard/node_meta/[mMyNodeName]'�� ���� ������. */

    /* 2. failover history���� �ڽſ� ���� ������ �����Ѵ�. */
    idlOS::strncpy( sPath[1], 
                    SDI_ZKC_PATH_FAILOVER_HISTORY,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[1]�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath[1],
                              sFailoverHistory,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* �� node�� Failover �Ǿ� �ִ��� Ȯ���Ѵ�. */
    idlOS::strncpy( sPath[0],
                    sNodePath, 
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0], 
                    SDI_ZKC_PATH_NODE_FAILOVER_TO,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/node_meta/[mMyNodeName]/failoverTo'�� ���� ������. */
    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath[0],
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* �� Node�� Failover �Ǿ� ������ Failover History�� �����ؾ� �Ѵ�. */
    if ( sDataLength > 0 )
    {
        /* failover history�� ����̸�1:SMN/����̸�2:SMN/...�� ���·� �̷���� �����Ƿ� 
         * ù ':'������ ��ġ�� �˸� ���� �ֱٿ� ���� ����� �̸��� �� �� �ִ�. */   
        sCheckBuffer = idlOS::strtok( sFailoverHistory, ":" );

        /* �ڽ��� ���� �ֱٿ� ���� ��尡 �ƴ϶�� failback�� ������ �� ����. */
        IDE_TEST( isNodeMatch( mMyNodeName, sCheckBuffer ) != ID_TRUE );

        /* failback �۾��� ���� �� failover history�� �����ϱ� ���� �޾ƿ� ���� �ڸ���. */
        sOtherFailoverHistory = idlOS::strtok( NULL, "/" );
        sOtherFailoverHistory = idlOS::strtok( NULL, "\0" );
    }
    else
    {
        /* �� Node�� Failover �Ǿ� ���� ������ FailoverHistory�� �������� �ʾƵ� �ȴ�. */
        sOtherFailoverHistory = sFailoverHistory;
    }

    /* 3. ��Ÿ ������ ������ multi op�� �����Ѵ�. */
    idlOS::strncpy( sPath[0],
                    sNodePath, 
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0], 
                    SDI_ZKC_PATH_NODE_FAILOVER_TO,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/node_meta/[mMyNodeName]/failoverTo'�� ���� ������. */

    zoo_set_op_init( &sOp[0],
                     sPath[0],
                     NULL,
                     -1,
                     -1,
                     NULL );

    idlOS::strncpy( sPath[1], 
                    SDI_ZKC_PATH_FAILOVER_HISTORY, 
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[1]�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. */

    /* �� ��带 ������ �ٸ� failover ������ �ִٸ� �ش� ������ �����Ѵ�. */
    if( sOtherFailoverHistory != NULL )
    {
        zoo_set_op_init( &sOp[1],
                         sPath[1],
                         sOtherFailoverHistory,
                         idlOS::strlen( sOtherFailoverHistory ),
                         -1,
                         NULL );
    }
    else
    {
        /* �� ��尡 �׾��ִ� ������ ����� �� ���� �����Ѵ�. */
        zoo_set_op_init( &sOp[1],
                         sPath[1],
                         NULL,
                         -1,
                         -1,
                         NULL );       
    }

    idlOS::strncpy( sPath[2],
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath[2]�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. */

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aSMN );

    zoo_set_op_init( &sOp[2],
                     sPath[2],
                     sBuffer,
                     idlOS::strlen( sBuffer ),
                     -1,
                     NULL );

    idlOS::strncpy( sPath[3],
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[3],
                    "/",
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    idlOS::strncat( sPath[3],
                    mMyNodeName,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    idlOS::strncat( sPath[3],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    /* sPath[3]�� '/altibase_shard/node_meta/[myNodeName]/state'�� ���� ������. */

    zoo_set_op_init( &sOp[3],
                     sPath[3],
                     SDI_ZKC_STATE_RUN,
                     idlOS::strlen( SDI_ZKC_STATE_RUN ),
                     -1,
                     NULL );   

    /* 4. multi op�� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 4, sOp, sOpResult );

    /* multi op�� ������ �� operation�� ���� ���� ���θ� Ȯ���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i <= 3; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    /* 5. �ڽ��� �����ϰ� �׾��ִ� ����� fault detection time�� �����ؾ� �Ѵ�. */
    if( sOtherFailoverHistory == NULL )
    {
        idlOS::memset( sPath[0],
                       0,
                       SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sPath[0], 
                        SDI_ZKC_PATH_FAULT_DETECT_TIME,
                        SDI_ZKC_PATH_LENGTH );
        sResult = aclZKC_deleteInfo( mZKHandler,
                                     sPath[0] );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    } 

    /* 6. ���� ������ �����Ѵ�. */
    IDE_TEST( addConnectionInfo() != IDE_SUCCESS );

    /* 7. watch�� �Ǵ�. */
    IDE_TEST( settingAddNodeWatch() != IDE_SUCCESS );
    IDE_TEST( settingAllDeleteNodeWatch() != IDE_SUCCESS );

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] FAILBACK After Commit Complete");

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] Failed FAILBACK After Commit");

    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::updateSMN                    *
 * ------------------------------------------------------------------*
 * Ŭ������ ��Ÿ�� ����� SMN ���� �����Ѵ�.
 * �ش� �Լ��� ��Ÿ�� �����ϴ� �۾��� ���� �� ȣ��Ǿ�� �Ѵ�.
 *
 * aSMN    [IN] - ������ SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::updateSMN( ULong aSMN )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    /* 1. path�� �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );

    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%"ID_UINT64_FMT, aSMN );
    /* sPath�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. */

    /* 3. zookeeper server�� SMN�� �����Ѵ�. */
    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS ,err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::updateSMN                    *
 * ------------------------------------------------------------------*
 * preparing for update SMN
 * aSMN    [IN] - ������ SMN ��
 *********************************************************************/
IDE_RC sdiZookeeper::prepareForUpdateSMN( ULong aSMN )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    if ( mIsConnect == ID_FALSE )
    {
        sResult = ZKC_CONNECTION_FAIL;
        IDE_RAISE(err_ZKC);
    }
    IDE_TEST_RAISE(mMyNodeName[0] == '\0', ERR_NOT_EXIST_MYNODENAME);
    
    /* 1. path�� �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_SMN,
                    SDI_ZKC_PATH_LENGTH );
    
    (void)idlOS::snprintf( sBuffer, SDI_ZKC_BUFFER_SIZE, "%s:%"ID_UINT64_FMT, mMyNodeName, aSMN );
    /* sPath�� '/altibase_shard/cluster_meta/SMN'�� ���� ������. NODENAME:SMNValue*/
    
    /* 3. zookeeper server�� SMN�� temporary �����Ѵ�. */
    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              idlOS::strlen( sBuffer ) );
    
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS ,err_ZKC );
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( ERR_NOT_EXIST_MYNODENAME )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdiZookeeper::updateSMN",
                                  "NOT EXIST MY NODE NAME " ) );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::updateState                  *
 * ------------------------------------------------------------------*
 * zookeeper server �� ����� �� ����� state�� �����Ѵ�.
 * �ش� �Լ������� state�� validate�� �������� �����Ƿ� validate�� �ʿ��ϴٸ�
 * �������� ���� validateState �Լ��� ȣ���ؼ� Ȯ���ؾ� �Ѵ�.
 *
 * aState    [IN] - ������ state
 *********************************************************************/
IDE_RC sdiZookeeper::updateState( SChar     * aState,
                                  SChar     * aTargetNodeName )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};

    /* 1. path�� �����Ѵ�. */
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aTargetNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aTargetNodeName]/state'�� ���� ������. */

    /* 3. zookeeper server�� ����� �� ����� state�� �����Ѵ�. */
    sResult = aclZKC_setInfo( mZKHandler,
                              sPath,
                              aState,
                              idlOS::strlen( aState ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getAllNodeNameList           *
 * ------------------------------------------------------------------*
 * Ŭ�������� ��� ����� �̸� ����Ʈ�� �̸������� �����Ͽ� �����´�.
 * �׾��ְų� SHUTDOWN�� ��嵵 Ŭ�����Ϳ� �߰��Ǿ��ٸ� ����� �ȴ�.
 *
 * aList    [OUT] - ���ĵ� ����� ����Ʈ
 *********************************************************************/
IDE_RC sdiZookeeper::getAllNodeNameList( iduList ** aList )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector   sChildren;
    SInt            i = 0;
    idBool          sIsAlloced = ID_FALSE;
    SInt            sState = 0;

    iduList       * sList     = NULL;
    iduListNode   * sNode     = NULL;
    SChar         * sNodeName = NULL;

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/node_meta'�� ���� ������. */

    /* 1. ��� ��Ÿ�� �� children�� �����´�. */
    sResult = aclZKC_getChildren( mZKHandler,
                                  sPath,
                                  &sChildren );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    IDE_TEST_RAISE( sChildren.count <= 0, err_no_node );

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF( iduList ),
                                 (void**)&( sList ),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    sIsAlloced = ID_TRUE;

    /* 2. ��带 �Ҵ��ϰ� ZKS���� ������ �����͸� �ִ´�. */
    for( i = 0; sChildren.count > i; i++ )
    {
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(iduListNode),
                                     (void**)&(sNode),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     SDI_NODE_NAME_MAX_SIZE + 1,
                                     (void**)&(sNodeName),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 2;

        idlOS::strncpy( sNodeName,
                        sChildren.data[i], 
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeName );

        IDU_LIST_ADD_FIRST( sList, sNode );

        sState = 0;
    }

    /* 3. ��� �̸� ������ �����Ѵ�. */

    /* ��尡 �ּ� 2�� �̻��̾���� ������ �ǹ̰� �ִ�. */
    if( sChildren.count > 1)
    {
        sortNode( sList );
    }

    *aList = sList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( err_no_node )
    {
        IDE_SET(ideSetErrorCode(sdERR_ABORT_SDI_ZKC_NO_NODE));
    }
    IDE_EXCEPTION_END;

    /* list�� ���� �߰����� ���� �޸� �Ҵ��� �����Ѵ�. */   
    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( sNodeName );
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
            break;
        default:
            break;
    }
 
    if( sIsAlloced == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getAllNodeInfoList           *
 * ------------------------------------------------------------------*
 * Ŭ�������� ��� ����� ����Ʈ�� ������ �̸������� �����ؼ� �����´�.
 * ���� �ش� ��尡 ù ���� �ٸ� ��尡 ���� ��쿡�� aList�� NULL�� �����Ѵ�.
 * getAllNodeNameList �Լ��ʹ� �޸� ����� �̸� �� �ƴ϶� �ٸ� ������ �����´�.
 * list�� ���� ��ü������ �޸𸮸� �Ҵ��� ����Ѵ�.
 *
 * aList    [OUT] - ���ĵ� ����� ����Ʈ
 * aNodeCnt [OUT] - ���� �����ϴ� ����� ����
 *********************************************************************/
IDE_RC sdiZookeeper::getAllNodeInfoList( iduList ** aList,
                                         UInt     * aNodeCnt )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sSubPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    String_vector   sChildren;
    SInt            i = 0;
    SInt            sDataLength = 0;
    SChar         * sStringPtr = NULL;
    idBool          sIsAlloced = ID_FALSE;
    SInt            sState = 0;

    iduList             * sList     = NULL;
    iduListNode         * sNode     = NULL;
    sdiLocalMetaInfo    * sNodeInfo = NULL;

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/node_meta'�� ���� ������. */

    /* 1. ��� ��Ÿ�� �� children�� �����´�. */
    sResult = aclZKC_getChildren( mZKHandler,
                                  sPath,
                                  &sChildren );

    if( sResult == ZKC_NO_NODE )
    {
        /* Ŭ������ ��Ÿ�� �������� �ʴ� ��� ����� ��尡 �ϳ��� ����. */
        *aList = NULL;
        *aNodeCnt = 0;
        IDE_CONT( normal_exit );
    }

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    if( sChildren.count < 1 )
    {
        /* ���� ����� ��尡 �ϳ��� ���� ��� */
        *aList = NULL;
        *aNodeCnt = 0;
        IDE_CONT( normal_exit );
    }

    IDE_DASSERT( sChildren.count > 0 );

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF(iduList),
                                 (void**)&(sList),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    sIsAlloced = ID_TRUE;

    /* 2. �� ��忡�Լ� �ڽ��� ������ ������ ������. */
    for( i = 0; sChildren.count > i; i++ )
    {
        /* 2.1 ��� ����Ʈ�� ����� ������ �Ҵ� �޴´�. */
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(iduListNode),
                                     (void**)&(sNode),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(sdiLocalMetaInfo),
                                     (void**)&(sNodeInfo),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 2;

        /* 2.2  ����ü������ �Ҵ� ���� ������ �ʱ�ȭ */
        sNodeInfo->mShardNodeId = 0;
        idlOS::memset( sNodeInfo->mShardedDBName, 0, SDI_SHARDED_DB_NAME_MAX_SIZE + 1 );
        idlOS::memset( sNodeInfo->mNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::memset( sNodeInfo->mHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mPortNo = 0;
        idlOS::memset( sNodeInfo->mInternalHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mInternalPortNo = 0;
        idlOS::memset( sNodeInfo->mInternalRPHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mInternalRPPortNo = 0;
        sNodeInfo->mInternalConnType = 0;

        /* 2.3 ��� �̸� ���� */
        idlOS::strncpy( sNodeInfo->mNodeName,
                        sChildren.data[i],
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* sNodePath ���� */
        idlOS::memset( sNodePath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sNodePath,
                        SDI_ZKC_PATH_NODE_META,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sNodePath,
                        "/",
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        idlOS::strncat( sNodePath,
                        sChildren.data[i],
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        /* sNodePath�� '/altibase_shard/node_meta/[sChildren.data[i]]'�� ���� ������. */

        /* 2.4 Shard Node ID ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_ID,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/shard_node_id'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sNodeInfo->mShardNodeId = idlOS::atoi( sBuffer );

        /* mShardedDBName�� �������� �ʴ´�. */

        /* 2.5 Node IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/node_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE + 1 );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mPortNo = idlOS::atoi( sStringPtr );

        /* 2.6 Internal Node IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_INTERNAL_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/internal_node_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mInternalHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mInternalPortNo = idlOS::atoi( sStringPtr );

        /* 2.7 Replication ���� IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_REPL_HOST_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/internal_replication_host_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mInternalRPHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mInternalRPPortNo = idlOS::atoi( sStringPtr );

        /* 2.8 ���� ��� ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_CONN_TYPE,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/conn_type'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sNodeInfo->mInternalConnType = idlOS::atoi( sBuffer );

        /* 2.9 ��� ����Ʈ�� �����͸� �����Ѵ�. */
        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeInfo );

        IDU_LIST_ADD_FIRST( sList, sNode );

        sState = 0;
    }

    /* 3. ��� �̸� ������ �����Ѵ�. */
    /* ��尡 �ּ� 2�� �̻��̾���� ������ �ǹ̰� �ִ�. */
    if( sChildren.count > 1 )
    {
        sortNode( sList );
    }

    *aList = sList;
    *aNodeCnt = sChildren.count;

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    /* list�� ���� �߰����� ���� �޸� �Ҵ��� �����Ѵ�. */
    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( sNodeInfo );
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
            break;
        default:
            break;
    }

    if( sIsAlloced == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODEINFO );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getAllNodeInfoList           *
 * ------------------------------------------------------------------*
 * Ŭ�������� ��� ����� ����Ʈ�� ������ �̸������� �����ؼ� �����´�.
 * ���� �ش� ��尡 ù ���� �ٸ� ��尡 ���� ��쿡�� aList�� NULL�� �����Ѵ�.
 * getAllNodeNameList �Լ��ʹ� �޸� ����� �̸� �� �ƴ϶� �ٸ� ������ �����´�.
 *
 * aList    [OUT] - ���ĵ� ����� ����Ʈ
 * aNodeCnt [OUT] - ���� �����ϴ� ����� ����
 * aMem     [IN]  - ����Ʈ�� �Ҵ���� �޸�
 *********************************************************************/
IDE_RC sdiZookeeper::getAllNodeInfoList( iduList  ** aList,
                                         UInt      * aNodeCnt,
                                         iduMemory * aMem )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sSubPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    String_vector   sChildren;
    SInt            i = 0;
    SInt            sDataLength = 0;
    SChar         * sStringPtr = NULL;

    iduList             * sList     = NULL;
    iduListNode         * sNode     = NULL;
    sdiLocalMetaInfo    * sNodeInfo = NULL;

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/node_meta'�� ���� ������. */

    /* 1. ��� ��Ÿ�� �� children�� �����´�. */
    sResult = aclZKC_getChildren( mZKHandler,
                                  sPath,
                                  &sChildren );

    if( sResult == ZKC_NO_NODE )
    {
        /* Ŭ������ ��Ÿ�� �������� �ʴ� ��� ����� ��尡 �ϳ��� ����. */
        *aList = NULL;
        *aNodeCnt = 0;
        IDE_CONT( normal_exit );
    }

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    if( sChildren.count < 1 )
    {
        /* ���� ����� ��尡 �ϳ��� ���� ��� */
        *aList = NULL;
        *aNodeCnt = 0;
        IDE_CONT( normal_exit );
    }

    IDE_DASSERT( sChildren.count > 0 );

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( aMem->alloc( ID_SIZEOF(iduList),
                          (void**)&(sList) )
              != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    /* 2. �� ��忡�Լ� �ڽ��� ������ ������ ������. */
    for( i = 0; sChildren.count > i; i++ )
    {
        /* 2.1 ��� ����Ʈ�� ����� ������ �Ҵ� �޴´�. */
        IDE_TEST( aMem->alloc( ID_SIZEOF(iduListNode),
                               (void**)&(sNode) )
                  != IDE_SUCCESS );

        IDE_TEST( aMem->alloc( ID_SIZEOF(sdiLocalMetaInfo),
                               (void**)&(sNodeInfo) )
                  != IDE_SUCCESS );

        /* 2.2  ����ü������ �Ҵ� ���� ������ �ʱ�ȭ */
        sNodeInfo->mShardNodeId = 0;
        idlOS::memset( sNodeInfo->mShardedDBName, 0, SDI_SHARDED_DB_NAME_MAX_SIZE + 1 );
        idlOS::memset( sNodeInfo->mNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::memset( sNodeInfo->mHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mPortNo = 0;
        idlOS::memset( sNodeInfo->mInternalHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mInternalPortNo = 0;
        idlOS::memset( sNodeInfo->mInternalRPHostIP, 0, SDI_SERVER_IP_SIZE + 1 );
        sNodeInfo->mInternalRPPortNo = 0;
        sNodeInfo->mInternalConnType = 0;

        /* 2.3 ��� �̸� ���� */
        idlOS::strncpy( sNodeInfo->mNodeName,
                        sChildren.data[i],
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* sNodePath ���� */
        idlOS::memset( sNodePath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sNodePath,
                        SDI_ZKC_PATH_NODE_META,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sNodePath,
                        "/",
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        idlOS::strncat( sNodePath,
                        sChildren.data[i],
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        /* sNodePath�� '/altibase_shard/node_meta/[sChildren.data[i]]'�� ���� ������. */

        /* 2.4 Shard Node ID ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_ID,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/shard_node_id'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sNodeInfo->mShardNodeId = idlOS::atoi( sBuffer );

        /* mShardedDBName�� �������� �ʴ´�. */

        /* 2.5 Node IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/node_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE + 1 );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mPortNo = idlOS::atoi( sStringPtr );

        /* 2.6 Internal Node IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_INTERNAL_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/internal_node_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mInternalHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mInternalPortNo = idlOS::atoi( sStringPtr );

        /* 2.7 Replication ���� IP �� Port ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_REPL_HOST_IP,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/internal_replication_host_ip:port'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        /* IP�� port�� �и��Ѵ�. */
        sStringPtr = idlOS::strtok( sBuffer, ":" );
        idlOS::strncpy( sNodeInfo->mInternalRPHostIP,
                        sStringPtr,
                        SDI_SERVER_IP_SIZE );

        sStringPtr = idlOS::strtok( NULL, "\0" );
        sNodeInfo->mInternalRPPortNo = idlOS::atoi( sStringPtr );

        /* 2.8 ���� ��� ȹ�� �� ���� */
        idlOS::memset( sSubPath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sSubPath,
                        sNodePath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sSubPath,
                        SDI_ZKC_PATH_NODE_CONN_TYPE,
                        ID_SIZEOF( sSubPath ) - idlOS::strlen( sSubPath ) - 1 );
        /* sSubPath�� '/altibase_shard/node_meta/[sChildren.data[i]]/conn_type'�� ���� ������. */

        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        sDataLength = SDI_ZKC_BUFFER_SIZE;
        sResult = aclZKC_getInfo( mZKHandler,
                                  sSubPath,
                                  sBuffer,
                                  &sDataLength );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sNodeInfo->mInternalConnType = idlOS::atoi( sBuffer );

        /* 2.9 ��� ����Ʈ�� �����͸� �����Ѵ�. */
        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeInfo );

        IDU_LIST_ADD_FIRST( sList, sNode );
    }

    /* 3. ��� �̸� ������ �����Ѵ�. */
    /* ��尡 �ּ� 2�� �̻��̾���� ������ �ǹ̰� �ִ�. */
    if( sChildren.count > 1 )
    {
        sortNode( sList );
    }

    *aList = sList;
    *aNodeCnt = sChildren.count;

    IDE_EXCEPTION_CONT( normal_exit );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getAliveNodeNameList         *
 * ------------------------------------------------------------------*
 * Ŭ�������� ��� �������� ����� �̸� ����Ʈ�� �̸������� �����Ͽ� �����´�.
 * �׾��ְų� SHUTDOWN�� ���� Ŭ�����Ϳ� �߰��Ǿ ����� ���� �ʴ´�.
 *
 * aList    [OUT] - ���ĵ� ����� ����Ʈ
 *********************************************************************/
IDE_RC sdiZookeeper::getAliveNodeNameList( iduList ** aList )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector   sChildren;
    SInt            i = 0;
    idBool          sIsAlloced = ID_FALSE;
    SInt            sState = 0;

    iduList       * sList     = NULL;
    iduListNode   * sNode     = NULL;
    SChar         * sNodeName = NULL;

    IDE_TEST_RAISE( sdiZookeeper::isConnect() != ID_TRUE, ERR_NOT_CONNECT );

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO, 
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/connection_info'�� ���� ������. */

    /* 1. connection_info���� ����ִ� ��常 �����Ƿ� ���⼭ children�� �����´�. */
    sResult = aclZKC_getChildren( mZKHandler,
                                  sPath,
                                  &sChildren );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    IDE_TEST_RAISE( sChildren.count <= 0, ERR_NODE_NOT_EXIST );

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF(iduList),
                                 (void**)&(sList),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    sIsAlloced = ID_TRUE;

    /* 2. ��带 �Ҵ��ϰ� ZKS���� ������ �����͸� �ִ´�. */
    for( i = 0; sChildren.count > i; i++ )
    {
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(iduListNode),
                                     (void**)&(sNode),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     SDI_NODE_NAME_MAX_SIZE + 1,
                                     (void**)&(sNodeName),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 2;

        idlOS::strncpy( sNodeName, 
                        sChildren.data[i], 
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* list�� �����Ѵ�. */
        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeName );

        IDU_LIST_ADD_FIRST( sList, sNode );

        sState = 0;
    }

    /* 3. ��� �̸� ������ �����Ѵ�. */

    /* ��尡 �ּ� 2�� �̻��̾���� ������ �ǹ̰� �ִ�. */
    if( sChildren.count > 1)
    {
        sortNode( sList );
    }

    *aList = sList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( ERR_NODE_NOT_EXIST )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdiZookeeper::getAliveNodeNameList",
                                  "znode not exist" ) );
    }
    IDE_EXCEPTION( ERR_NOT_CONNECT )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdiZookeeper::getAliveNodeNameList",
                                  "The shard node has not yet been joined to the sharding cluster." ) );
    }
    IDE_EXCEPTION_END;

    /* list�� ���� �߰����� ���� �޸� �Ҵ��� �����Ѵ�. */
    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( sNodeName );
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
            break;
        default:
            break;
    }

    if( sIsAlloced == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getAliveNodeNameListIncludeNodeName *
 * ------------------------------------------------------------------*
 * Ŭ�������� ��� Alive ���� ����� �̸� ����Ʈ�� Ư�� Node�� ���Խ��Ѽ� �̸������� �����Ͽ� �����´�.
 * �׾��ְų� SHUTDOWN�� ���� Ŭ�����Ϳ� �߰��Ǿ ����� ���� �ʴ´�.
 *
 * aList    [OUT] - ���ĵ� ����� ����Ʈ
 *********************************************************************/
IDE_RC sdiZookeeper::getAliveNodeNameListIncludeNodeName( iduList ** aList,
                                                          SChar    * aNodeName )
{
    ZKCResult       sResult;
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector   sChildren;
    SInt            i = 0;
    idBool          sIsAlloced = ID_FALSE;
    SInt            sState = 0;
    idBool          sTargetNodeAlived = ID_FALSE;

    iduList       * sList     = NULL;
    iduListNode   * sNode     = NULL;
    SChar         * sNodeName = NULL;

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO, 
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/connection_info'�� ���� ������. */

    /* 1. connection_info���� ����ִ� ��常 �����Ƿ� ���⼭ children�� �����´�. */
    sResult = aclZKC_getChildren( mZKHandler,
                                  sPath,
                                  &sChildren );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    IDE_DASSERT( sChildren.count > 0 );

    /* list�� �ʱ�ȭ �Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF(iduList),
                                 (void**)&(sList),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    sIsAlloced = ID_TRUE;

    /* 2. ��带 �Ҵ��ϰ� ZKS���� ������ �����͸� �ִ´�. */
    for( i = 0; sChildren.count > i; i++ )
    {
        if ( idlOS::strncmp( aNodeName,
                             sChildren.data[i],
                             SDI_NODE_NAME_MAX_SIZE + 1 ) == 0 )
        {
            /* Alive List�� ������ NodeName�� ������ Ȯ���� �д�. */
            sTargetNodeAlived = ID_TRUE;
        }

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(iduListNode),
                                     (void**)&(sNode),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     SDI_NODE_NAME_MAX_SIZE + 1,
                                     (void**)&(sNodeName),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 2;

        idlOS::strncpy( sNodeName, 
                        sChildren.data[i], 
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* list�� �����Ѵ�. */
        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeName );

        IDU_LIST_ADD_FIRST( sList, sNode );

        sState = 0;
    }

    if ( sTargetNodeAlived == ID_FALSE )
    {
        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     ID_SIZEOF(iduListNode),
                                     (void**)&(sNode),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                     SDI_NODE_NAME_MAX_SIZE + 1,
                                     (void**)&(sNodeName),
                                     IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
        sState = 2;

        idlOS::strncpy( sNodeName, 
                        aNodeName, 
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* list�� �����Ѵ�. */
        IDU_LIST_INIT_OBJ( sNode, (void*)sNodeName );

        IDU_LIST_ADD_FIRST( sList, sNode );

        sState = 0;
    }

    /* 3. ��� �̸� ������ �����Ѵ�. */

    /* ��尡 �ּ� 2�� �̻��̾���� ������ �ǹ̰� �ִ�. */
    if( sChildren.count > 1)
    {
        sortNode( sList );
    }

    *aList = sList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    /* list�� ���� �߰����� ���� �޸� �Ҵ��� �����Ѵ�. */
    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( sNodeName );
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
            break;
        default:
            break;
    }

    if( sIsAlloced == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getNextNode                  *
 * ------------------------------------------------------------------*
 * Ư�� ����� ���� ���(�̸��� ���� ����)�� �̸��� �ش� ����� ���¸� �����´�.
 * �Է¹��� ��尡 �̸��� ������ ������ ����� ���
 * �̸��� ������ ù ��尡 ��ȯ ����� �ȴ�.
 * �׾��ְų� SHUTDOWN�� ��嵵 Ž�� ����� �ȴ�.
 *
 * aTargetName [IN]  - ���� ��带 ã�� ��� ����� �̸�
 * aReturnName [OUT] - ��� ����� ���� ����� �̸�
 * aNodeState  [OUT] - ��� ����� ���� ����� ����
 *********************************************************************/
IDE_RC sdiZookeeper::getNextNode( SChar   * aTargetName, 
                                  SChar   * aReturnName, 
                                  ZKState * aNodeState )
{
    ZKCResult         sResult;
    iduList         * sList = NULL;
    iduListNode     * sIterator = NULL;
    SChar             sBuffer[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar             sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    idBool            sIsAllocList = ID_FALSE;
    idBool            sIsFound = ID_FALSE;
    SInt              sDataLength;

    IDE_TEST( getAllNodeNameList ( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    IDU_LIST_ITERATE( sList, sIterator )
    {
        idlOS::memset( sBuffer, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::strncpy( sBuffer,
                        (SChar*)sIterator->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* ����Ʈ���� ��ǥ ��带 ã�� ��� �ش� ����� ���� ����� state�� �����´�. */
        if( isNodeMatch( sBuffer, aTargetName ) == ID_TRUE )
        {
            sIsFound = ID_TRUE;

            idlOS::memset( sBuffer, 0, SDI_NODE_NAME_MAX_SIZE + 1 );

            if( sIterator->mNext != sList )
            {
                /* list�� ������ ��尡 �ƴ϶�� ���� ��尡 Ž�� ����̴�. */
                idlOS::strncpy( sBuffer, 
                                (SChar*)sIterator->mNext->mObj, 
                                SDI_NODE_NAME_MAX_SIZE + 1 );
            }
            else
            {
                /* list�� ������ ����� ��� ù ��尡 Ž�� ����̴�. */
                idlOS::strncpy( sBuffer, 
                                (SChar*)sList->mNext->mObj, 
                                SDI_NODE_NAME_MAX_SIZE + 1 ); 
            }

            idlOS::strncpy( aReturnName,
                            sBuffer,
                            SDI_NODE_NAME_MAX_SIZE + 1 );
            idlOS::strncpy( sPath,
                            SDI_ZKC_PATH_NODE_META, 
                            SDI_ZKC_PATH_LENGTH );
            idlOS::strncat( sPath,
                            "/",
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            idlOS::strncat( sPath,
                            sBuffer,
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            idlOS::strncat( sPath,
                            SDI_ZKC_PATH_NODE_STATE,
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            /* sPath�� '/altibase_shard/node_meta/[sBuffer]/state'�� ���� ������. */

            sDataLength = SDI_NODE_NAME_MAX_SIZE + 1;
            sResult = aclZKC_getInfo( mZKHandler,
                                      sPath,
                                      sBuffer,
                                      &sDataLength );    

            IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

            *aNodeState = stateStringConvert( sBuffer );

            IDE_TEST( *aNodeState == ZK_ERROR );

            break;
        }
    }

    sIsAllocList = ID_FALSE;
    freeList( sList, SDI_ZKS_LIST_NODENAME );

    IDE_TEST_RAISE( sIsFound == ID_FALSE, err_Meta );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( err_Meta )
    {
        logAndSetErrMsg( ZKC_META_CORRUPTED );
    }
    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getPrevNode                  *
 * ------------------------------------------------------------------*
 * Ư�� ����� ���� ���(�̸��� ���� ����)�� �̸��� �ش� ����� ���¸� �����´�.
 * �Է¹��� ��尡 �̸��� ������ ù ����� ���
 * �̸��� ������ ������ ��尡 ��ȯ ����� �ȴ�.
 * �׾��ְų� SHUTDOWN�� ��嵵 Ž�� ����� �ȴ�.
 *
 * aTargetName [IN]  - ���� ��带 ã�� ��� ����� �̸�
 * aReturnName [OUT] - ��� ����� ���� ����� �̸�
 * aNodeState  [OUT] - ��� ����� ���� ����� ����
 *********************************************************************/
IDE_RC sdiZookeeper::getPrevNode( SChar   * aTargetName, 
                                  SChar   * aReturnName, 
                                  ZKState * aNodeState )
{
    ZKCResult         sResult;
    iduList         * sList = NULL;
    iduListNode     * sIterator = NULL;
    SChar             sBuffer[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar             sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    idBool            sIsAllocList = ID_FALSE;
    idBool            sIsFound = ID_FALSE;
    SInt              sDataLength;

    IDE_TEST( getAllNodeNameList ( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    IDU_LIST_ITERATE( sList, sIterator )
    {
        idlOS::memset( sBuffer, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
        idlOS::strncpy( sBuffer,
                        (SChar*)sIterator->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        if( isNodeMatch( sBuffer, aTargetName ) == ID_TRUE )
        {
            sIsFound = ID_TRUE;

            idlOS::memset( sBuffer, 0, SDI_NODE_NAME_MAX_SIZE + 1 );

            if( sIterator->mPrev != sList )
            {
                /* list�� ù ��尡 �ƴ϶�� �ٷ� ���� ��尡 Ž�� ����̴�. */
                idlOS::strncpy( sBuffer,
                                (SChar*)sIterator->mPrev->mObj,
                                SDI_NODE_NAME_MAX_SIZE + 1 );
            }
            else
            {
                /* list�� ù ����� list�� ������ ��尡 Ž�� ����̴�. */
                idlOS::strncpy( sBuffer,
                                (SChar*)sList->mPrev->mObj,
                                SDI_NODE_NAME_MAX_SIZE + 1 );
            }

            idlOS::strncpy( aReturnName,
                            sBuffer, 
                            SDI_NODE_NAME_MAX_SIZE + 1 );
            idlOS::strncpy( sPath,
                            SDI_ZKC_PATH_NODE_META,
                            SDI_ZKC_PATH_LENGTH );
            idlOS::strncat( sPath,
                            "/",
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            idlOS::strncat( sPath,
                            sBuffer,
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            idlOS::strncat( sPath,
                            SDI_ZKC_PATH_NODE_STATE,
                            ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
            /* sPath�� '/altibase_shard/node_meta/[sBuffer]/state'�� ���� ������. */

            sDataLength = SDI_NODE_NAME_MAX_SIZE + 1;
            sResult = aclZKC_getInfo( mZKHandler,
                                      sPath,
                                      sBuffer,
                                      &sDataLength );    

            IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

            *aNodeState = stateStringConvert( sBuffer );

            IDE_TEST( *aNodeState == ZK_ERROR );

            break;
        }
    }

    sIsAllocList = ID_FALSE;
    freeList( sList, SDI_ZKS_LIST_NODENAME );

    IDE_TEST_RAISE( sIsFound == ID_FALSE, err_Meta );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION( err_Meta )
    {
        logAndSetErrMsg( ZKC_META_CORRUPTED );
    }
    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getNextAliveNode             *
 * ------------------------------------------------------------------*
 * Ư�� ����� ���� ���(�̸��� ���� ����)�� �̸��� �ش� ����� ���¸� �����´�.
 * �Է¹��� ��尡 �̸��� ������ ������ ����� ���
 * �̸��� ������ ù ��尡 ��ȯ ����� �ȴ�.
 * �׾��ְų� SHUTDOWN�� ���� Ž�� ����� �ƴϴ�.
 *
 * aTargetName [IN]  - ���� ��带 ã�� ��� ����� �̸�
 * aReturnName [OUT] - ��� ����� ���� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::getNextAliveNode( SChar   * aTargetNodeName,
                                       SChar   * aReturnName )
{
    iduList         * sList = NULL;
    iduListNode     * sIterator = NULL;
    iduListNode     * sNode;
    idBool            sIsAllocList = ID_FALSE;
    idBool            sIsFound = ID_FALSE;

    IDE_TEST( getAliveNodeNameList ( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    IDU_LIST_ITERATE( sList, sIterator )
    {
        if( idlOS::strncmp( (SChar*)sIterator->mObj, aTargetNodeName, SDI_NODE_NAME_MAX_SIZE ) > 0 )
        {
            /* Ž�� ��󺸴� ū ��带 ó������ ã�� ��� �ش� ��尡 ��ȯ ��� ����. */
            sNode = sIterator;
            sIsFound = ID_TRUE;

            idlOS::strncpy( aReturnName,
                            (SChar*)sNode->mObj,
                            SDI_NODE_NAME_MAX_SIZE + 1 );
            break;
        }
    }

    /* ���� ��尡 �̸������� ���� ū ����� ��� */
    if( sIsFound == ID_FALSE )
    {
        /* �� ��� ����Ʈ�� ���� �� ���(���� ���� ���)�� ��ȯ ��� ��尡 �ȴ�. */
        sNode = IDU_LIST_GET_FIRST(sList);

        idlOS::strncpy( aReturnName,
                        (SChar*)sNode->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );
    }

    sIsAllocList = ID_FALSE;
    freeList( sList, SDI_ZKS_LIST_NODENAME );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getPrevAliveNode             *
 * ------------------------------------------------------------------*
 * Ư�� ����� ���� ���(�̸��� ���� ����)�� �̸��� �ش� ����� ���¸� �����´�.
 * �Է¹��� ��尡 �̸��� ������ ù ����� ���
 * �̸��� ������ ������ ��尡 ��ȯ ����� �ȴ�.
 * �׾��ְų� SHUTDOWN�� ���� Ž�� ����� �ƴϴ�.
 *
 * aTargetName [IN]  - ���� ��带 ã�� ��� ����� �̸�
 * aReturnName [OUT] - ��� ����� ���� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::getPrevAliveNode( SChar   * aTargetNodeName,
                                       SChar   * aReturnName )
{
    iduList         * sList = NULL;
    iduListNode     * sIterator = NULL;
    iduListNode     * sNode;
    idBool            sIsAllocList = ID_FALSE;
    idBool            sIsFound = ID_FALSE;

    IDE_TEST( getAliveNodeNameList ( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    IDU_LIST_ITERATE( sList, sIterator )
    {
        if( sIterator->mPrev != sList )
        {
            if( idlOS::strncmp( (SChar*)sIterator->mObj, aTargetNodeName, SDI_NODE_NAME_MAX_SIZE ) > 0 )
            {
                /* Ž�� ��󺸴� ū ��带 ó������ ã�� ��� �ش� ����� ���� ��尡 ��ȯ ��� ����. */
                sNode = sIterator->mPrev;
                sIsFound = ID_TRUE;

                idlOS::strncpy( aReturnName,
                                (SChar*)sNode->mObj,
                                SDI_NODE_NAME_MAX_SIZE + 1 );
                break;
            }
        }
        else
        {
            /* ù ����� ��� header�� ���Ϸ��� ���ٵ� header�� ���� �����Ƿ� ���ؼ��� �ȵȴ�. */
        }
    }

    /* ���� ��尡 �̸������� ���� ū ����� ��� */
    if( sIsFound == ID_FALSE )
    {
        /* �� ��� ����Ʈ�� ������ ��尡 ��ȯ ��� ��尡 �ȴ�. */
        sNode = IDU_LIST_GET_LAST( sList );

        idlOS::strncpy( aReturnName,
                        (SChar*)sNode->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );
    }

    sIsAllocList = ID_FALSE;
    freeList( sList, SDI_ZKS_LIST_NODENAME );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::settingAddNodeWatch          *
 * ------------------------------------------------------------------*
 * ZKS�� ��������(/altibase_shard/connection_info)�� CHILD_CHANGED watch�� �Ǵ�.
 * �� ��尡 ����Ǹ� ���������� �����͸� ������Ų��.
 * ���濡 ���� �˶��� ���� �� ������ �� ��尡 �߰��� ���� �� �� �ִ�.
 *********************************************************************/
IDE_RC sdiZookeeper::settingAddNodeWatch()
{

    ZKCResult sResult;
    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/connection_info'�� ���� ������. */

    sResult = aclZKC_Watch_Data( mZKHandler,
                                 watch_addNewNode,
                                 sPath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::settingAllDeleteNodeWatch    *
 * ------------------------------------------------------------------*
 * ZKS�� ��������(/altibase_shard/connection_info)�� ��� �ڽĵ鿡�� watch�� �Ǵ�.
 * ���������� ������ ���� ��� �˶��� �߻��� watch_deleteNode�Լ��� ����ȴ�.
 * �ش� �Լ��� ���������� �� ��尡 �߰��Ǿ����� ȣ��ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::settingAllDeleteNodeWatch()
{
    ZKCResult       sResult;
    String_vector   sChildren;
    SChar           sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar           sConnInfoPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SInt            i = 0;

    idlOS::strncpy( sConnInfoPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    /* sConnInfoPath�� '/altibase_shard/connection_info'�� ���� ������. */

    sResult = aclZKC_getChildren( mZKHandler,
                                  sConnInfoPath,
                                  &sChildren );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; sChildren.count > i; i++ )
    {
        idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );
        idlOS::strncpy( sBuffer,
                        sChildren.data[i],
                        SDI_ZKC_BUFFER_SIZE );

        /* �ڱ� �ڽ��� delete�� Watch �Ѵ�. Failover ������ ���� ���ŵɼ� �ֱ� ���� */
        idlOS::memset( sNodePath, 0, SDI_ZKC_PATH_LENGTH );
        idlOS::strncpy( sNodePath,
                        sConnInfoPath,
                        SDI_ZKC_PATH_LENGTH );
        idlOS::strncat( sNodePath, 
                        "/",
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        idlOS::strncat( sNodePath,
                        sBuffer,
                        ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
        /* sNodePath�� '/altibase_shard/connection_info/[sBuffer]'�� ���� ������. */

        sResult = aclZKC_Watch_Node( mZKHandler,
                                     watch_deleteNode,
                                     sNodePath );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::settingDeleteNodeWatch       *
 * ------------------------------------------------------------------*
 * ZKS�� ��������(/altibase_shard/connection_info)�� Ư�� �ڽĿ��� watch�� �Ǵ�.
 * ���������� ������ ���� ��� �˶��� �߻��� watch_deleteNode�Լ��� ����ȴ�.
 * �ش� �Լ��� Ư�� ��� ���� �˶��� �� �� �ٽ� �˶��� �ɱ� ���� ȣ��ȴ�.
 *********************************************************************/
IDE_RC sdiZookeeper::settingDeleteNodeWatch( SChar * aPath )
{
    ZKCResult sResult;

    sResult = aclZKC_Watch_Node( mZKHandler,
                                 watch_deleteNode,
                                 aPath );

    IDE_TEST_RAISE( ( sResult != ZKC_SUCCESS ) && ( sResult != ZKC_NO_NODE ), err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::watch_addNewNode             *
 * ------------------------------------------------------------------*
 * ���������� �����Ϳ� ������ ���� ��� �˶��� �߻��� ȣ��ȴ�.
 * Ȥ�� �˶��� ���� ���� �� �˶��� ���� ȣ��ȴ�.
 * �� ��� �� ��尡 �߰��� ���̹Ƿ� connection_info�� �� �ڽĳ�忡 ����
 * watch�� �ٽ� �ɼ� �ֵ��� watch_deleteNode �Լ��� ȣ���Ѵ�.
 *********************************************************************/
void sdiZookeeper::watch_addNewNode( aclZK_Handler *,
                                     SInt           aType,
                                     SInt           ,
                                     const SChar   *,
                                     void          * )
{
    SInt                sDataLength = SDI_ZKC_BUFFER_SIZE;
    SChar               sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};

    initTempContainer( (SChar *)"[ZOOKEEPER] watch_addNewNode", IDE_SD_20 );

    if( aType == ZOO_SESSION_EVENT )
    {
        /* ZKS�� ������ ���� ���� */
        if ( isConnect() == ID_TRUE )
        {
            disconnect();
        }

        if ( connect( ID_FALSE ) == IDE_SUCCESS )
        {
            /* ConnectionInfo�� Add�ϱ� ���� MetaLock�� ȹ���Ѵ�. */
            if( getZookeeperMetaLock( ID_UINT_MAX ) == IDE_SUCCESS )
            {

                /* �̹� failover �Ǿ����� Ȯ�� �ؾ� �Ѵ�. */
                (void) getNodeInfo( mMyNodeName,
                                    (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                    sBuffer,
                                    &sDataLength );

                if ( sDataLength > 0 )
                {
                    /* �̹� failover�� �Ǿ��ִ� �����̹Ƿ� ���� �۾��� �ʿ䰡 ����. */
                    releaseZookeeperMetaLock();
                    disconnect();
                }
                else
                {
                    if ( addConnectionInfo() == IDE_SUCCESS )
                    {
                        /* R2HA Watch ������ �������� ��� �׳� �����ϸ� �ȵȴ�.
                         * ���� Ȯ���� �ʿ��� */
                        /* add watch�� �ɾ�� �Ѵ�. */
                        (void)settingAddNodeWatch();

                        /* delete watch�� �ٽ� �ɾ�� �Ѵ�. */
                        (void)settingAllDeleteNodeWatch();

                        releaseZookeeperMetaLock();

                    }
                    else
                    {
                        /* ������ ���� */
                        releaseZookeeperMetaLock();
                        disconnect();
                    }
                }
            }
            else
            {
                /* ������ ���� */
                disconnect();
            }
        }
        else
        {
            /* ������ ���� */
        }

        if ( isConnect() != ID_TRUE )
        {
            /* re-connect �� �����Ͽ���. */
            /* �����ڵ带 �����ϰ� trc�α׸� ���� �� assert�� ȣ���Ѵ�. */
            logAndSetErrMsg( ZKC_CONNECTION_MISSING );
            IDE_CALLBACK_FATAL( "Connection with ZKS has been lost." );
        }

    }
    else if( aType == ZOO_DELETED_EVENT )
    {
        /* ������ �Ǿ� ������ connection_info ��ü�� ������ ������ ���� ���� */
    }
    else
    {
        /* watch �˶��� ���� ���� �ٽ� watch�� �ɾ�� �Ѵ�. */
        (void)settingAddNodeWatch();

        /* �� ��尡 �߰��Ǿ����Ƿ� CHILD�� ���� watch�� �ٽ� �ɾ�� �Ѵ�. */
        (void)settingAllDeleteNodeWatch();
    }

    destroyTempContainer();

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::watch_deleteNode             *
 * ------------------------------------------------------------------*
 * ���������� ������ ���� ��� �˶��� �߻��� ȣ��ȴ�.
 * ���� �˶��� �м��� �ڽ��� failover�� �����ؾ� �ϴ� ��� failover�� ȣ���Ѵ�.
 *********************************************************************/
void sdiZookeeper::watch_deleteNode( aclZK_Handler *,
                                     SInt            aType,
                                     SInt           ,
                                     const SChar   * aPath,
                                     void          * )
{
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SChar       sNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     * sStrPtr;
    ZKState     sNodeState = ZK_NONE;

    initTempContainer( (SChar *)"[ZOOKEEPER] watch_deleteNode", IDE_SD_20 );

    if( aType != ZOO_SESSION_EVENT )
    {
        /* watch �˶��� ���� ���� �ٽ� watch�� �ɾ�� �Ѵ�. */
        idlOS::strncpy( sPath,
                        aPath,
                        SDI_ZKC_PATH_LENGTH );
        (void)settingDeleteNodeWatch( sPath );

        if( ( aType == ZOO_DELETED_EVENT ) )
        {
            /* aPath�� '/altibase_shard/connection_info/[deadNodeName]'�� �����̹Ƿ� * 
             * [deadNodeName] �κи� �и��ϸ� �ٷ� ���� ����� �̸��� �� �� �ִ�. */
            sStrPtr = (SChar*)aPath + idlOS::strlen( SDI_ZKC_PATH_CONNECTION_INFO ) + 1;
            idlOS::strncpy( sNodeName,
                            sStrPtr,
                            SDI_NODE_NAME_MAX_SIZE + 1 );

            /* delete�� ���� �˶��� ��� ���� ó���ؾ� �ϴ��� üũ */
            if ( getNextAliveNode( sNodeName, sBuffer ) == IDE_SUCCESS )
            {
                if( isNodeMatch( sBuffer, mMyNodeName ) == ID_TRUE )
                {
                    (void)getNodeState( sNodeName, &sNodeState );

                    // check State ZK_KILL �̸� Failover4Watcher����� �ƴ�
                    if ( sNodeState != ZK_KILL )
                    {
                        /* �� ���� ��尡 �׾��� ��� ���� failover �ؾ� �Ѵ�. */
                        /* FailoverForWatcher�� �����ִ��� Ȯ��. */
                        if ( SDU_DISABLE_FAILOVER_FOR_WATCHER == 0 )
                        {
                            (void)failoverForWatcher( sNodeName );
                        }
                    }
                }
                else if ( isNodeMatch( sNodeName, mMyNodeName ) == ID_TRUE )
                {
                    (void)getNodeState( sNodeName, &sNodeState );

                    // check State ZK_KILL �̸� KILL
                    if ( sNodeState == ZK_KILL )
                    {
                        ideLog::log( IDE_SD_20, "This Node is Failed Over" );
                        IDE_CALLBACK_FATAL("This node is Failed Over" );
                    }
                }
            }
            else
            {
                /* in case of all node delete, the alarm was ignored */
            }
        }
        else
        {
            /* add�� ���� �˶��� ��� ���� */
        }
    }
    else
    {
        /* ZKS�� ������ ���� ���� */
        /* ������ ����� add/delete watch ��� �����Ƿ� add ���� ó���Ѵ�. */
        /* �����ڵ带 �����ϰ� trc�α׸� ���� �� assert�� ȣ���Ѵ�. */
//        logAndSetErrMsg( ZKC_CONNECTION_MISSING );
//        IDE_CALLBACK_FATAL("Connection with ZKS has been lost." );
    }

    destroyTempContainer();

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getNodeInfo                  *
 * ------------------------------------------------------------------*
 * Ư�� ����� ��Ÿ ������ ��ȯ�Ѵ�.
 * aDataLen�� �ΰ��� �뵵�� ���ȴ�.
 *
 * aTargetName - [IN]  ��Ÿ ������ ��� ���� ����� �̸�
 * aInfoType   - [IN]  ��� ���� ��Ÿ ����
 * aValue      - [OUT] ��Ÿ ������ ��ȯ�� ����
 * aDataLen    - [IN]  ��Ÿ ������ ��ȯ�� ������ ����
 *             - [OUT] ��ȯ�� ��Ÿ ������ ����
 *********************************************************************/
IDE_RC sdiZookeeper::getNodeInfo( SChar * aTargetName,
                                  SChar * aInfoType,
                                  SChar * aValue,
                                  SInt  * aDataLen )
{
    ZKCResult sResult;
    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aTargetName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aInfoType,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aTargetName][aInfoType]'�� ���� ������. */

    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              aValue,
                              aDataLen );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getNodeState                 *
 * ------------------------------------------------------------------*
 * Ư�� ����� ����(state)�� ��ȯ�Ѵ�.
 *
 * aTargetName - [IN]  ���� ������ ��� ���� ����� �̸�
 * aState      - [OUT] ��û�� ����� ����
 *********************************************************************/
IDE_RC sdiZookeeper::getNodeState( SChar   * aTargetName,
                                   ZKState * aState )
{
    ZKCResult sResult;
    SChar     sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar     sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt      sDataSize = SDI_ZKC_BUFFER_SIZE;
    ZKState   sState;

    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aTargetName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* sPath�� '/altibase_shard/node_meta/[aTargetName]/state'�� ���� ������. */

    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataSize );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    sState = sdiZookeeper::stateStringConvert( sBuffer );

    IDE_TEST_RAISE( sState == ZK_ERROR, err_ZKC );

    *aState = sState;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::insertInfo                   *
 * ------------------------------------------------------------------*
 * Ư�� path�� ���ϴ� ���� �����Ѵ�.
 * �ش� �Լ��� metaLock�� ���� �ʰ� �����ϹǷ� �ټ��� ��尡 
 * ���ÿ� ������ ������ �ִ� �κ��� ������ ��쿡�� ���� metaLock�� ��ƾ� �Ѵ�.
 *
 * aPath       - [IN] �����͸� ������ path(full path)
 * aData       - [IN] ������ ������
 * aDataLength - [IN] ������ �������� ����
 *********************************************************************/
IDE_RC sdiZookeeper::insertInfo( SChar * aPath,
                                 SChar * aData )
{
    ZKCResult sResult;

    sResult = aclZKC_setInfo( mZKHandler,
                              aPath,
                              aData,
                              idlOS::strlen( aData ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::readInfo                     *
 * ------------------------------------------------------------------*
 * Ư�� path�� ���� �д´�.
 *
 * aPath    - [IN]  �����͸� ���� path(full path)
 * aData    - [OUT] ���� �����͸� ��ȯ�� ����
 * aDataLen - [IN]  ���� �����͸� ��ȯ�� ������ ũ��
 *          - [OUT] ���� �������� ����
 *********************************************************************/
IDE_RC sdiZookeeper::readInfo( SChar * aPath,
                               SChar * aData,
                               SInt  * aDataLen )
{
    ZKCResult sResult;

    sResult = aclZKC_getInfo( mZKHandler,
                              aPath,
                              aData,
                              aDataLen );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );


    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::removeInfo                   *
 * ------------------------------------------------------------------*
 * Ư�� path�� ���� �����.(path�� �������� �ʴ´�.)
 * �ش� �Լ��� metaLock�� ���� �ʰ� �����ϹǷ� �ټ��� ��尡 
 * ���ÿ� ������ ������ �ִ� �κ��� ������ ��쿡�� ���� metaLock�� ��ƾ� �Ѵ�.
 *
 * aPath       - [IN] �����͸� ������ path(full path)
 *********************************************************************/
IDE_RC sdiZookeeper::removeInfo( SChar * aPath )
{
    ZKCResult sResult;

    sResult = aclZKC_deleteInfo( mZKHandler,
                                 aPath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::createPath                   *
 * ------------------------------------------------------------------*
 * Ư�� path�� �����ϰ� �����͸� �ִ´�.
 * �ش� �Լ��� metaLock�� ���� �ʰ� �����ϹǷ� �ټ��� ��尡 
 * ���ÿ� ������ ������ �ִ� �κ��� ������ ��쿡�� ���� metaLock�� ��ƾ� �Ѵ�.
 *
 * aPath       - [IN] ���� ������ path(full path)
 * aData       - [IN] ������ ������ ������
 *********************************************************************/
IDE_RC sdiZookeeper::createPath( SChar * aPath,
                                 SChar * aData )
{
    ZKCResult sResult;

    sResult = aclZKC_insertNode( mZKHandler,
                                 aPath,
                                 aData,
                                 idlOS::strlen( aData ),
                                 ACP_FALSE );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::removePath                   *
 * ------------------------------------------------------------------*
 * Ư�� path�� �� �����͸� �����Ѵ�.
 * �ش� �Լ��� metaLock�� ���� �ʰ� �����ϹǷ� �ټ��� ��尡 
 * ���ÿ� ������ ������ �ִ� �κ��� ������ ��쿡�� ���� metaLock�� ��ƾ� �Ѵ�.
 *
 * aPath       - [IN] ������ path(full path)
 *********************************************************************/
IDE_RC sdiZookeeper::removePath( SChar * aPath )
{
    ZKCResult sResult;

    sResult = aclZKC_deleteNode( mZKHandler,
                                 aPath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdiZookeeper::removeRecursivePathAndInfo( SChar * aPath )
{
    ZKCResult sResult;
    String_vector sChildren;
    SInt i = 0;
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE+1] = {0,};
    SInt        sReturn = 0;
    
    sResult = aclZKC_getChildren( mZKHandler,
                                  aPath,
                                  &sChildren );
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for ( i = 0; i < sChildren.count ; i++ )
    {
        sReturn = idlOS::snprintf(sBuffer,SDI_ZKC_BUFFER_SIZE+1,"%s/%s",aPath, sChildren.data[i]);
        IDE_TEST_RAISE(sReturn >= SDI_ZKC_BUFFER_SIZE+1, err_buffer_size);
        removeRecursivePathAndInfo( sBuffer);
    }

    sResult = aclZKC_deleteNode( mZKHandler,
                                 aPath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;
    IDE_EXCEPTION( err_buffer_size )
    {
        IDE_SET( ideSetErrorCode( sdERR_ABORT_SDC_UNEXPECTED_ERROR,
                                  "sdiZookeeper::removeRecursivePathAndInfo",
                                  "internal module error, buffer size too small" ) );
    }
    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::checkPath                    *
 * ------------------------------------------------------------------*
 * Ư�� path�� �����ϴ��� Ȯ���Ѵ�.
 *
 * aPath       - [IN]  ���翩�θ� Ȯ���� path(full path)
 * aIsExist    - [OUT] �ش� path�� ���翩��
 *********************************************************************/
IDE_RC sdiZookeeper::checkPath( SChar  * aPath,
                                idBool * aIsExist )
{
    ZKCResult sResult;

    sResult = aclZKC_existNode( mZKHandler,
                                aPath );

    switch( sResult )
    {
        case ZKC_SUCCESS:
            *aIsExist = ID_TRUE;
            break;
        case ZKC_NO_NODE:
            *aIsExist = ID_FALSE;
            break;
        default:
            /* �� �ܿ� �ٸ� ������ ���ܰ� �߻��Ѵ�. */
            IDE_RAISE( err_ZKC );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::addConnectionInfo            *
 * ------------------------------------------------------------------*
 * �� ����� ���� ������ �߰��Ѵ�.
 * /altibase_shard/connection_info�� �� ����� �̸����� �� �ӽ� ��带 ������ ��
 * connection_info�� ���� ������� �ٸ� ���鿡�� �� ��尡 �߰� �Ǿ����� �˸���.
 * �˶��� ���� ������ �� �ӽ� ��忡 ���� watch�� �߰��Ѵ�.
 *********************************************************************/
IDE_RC sdiZookeeper::addConnectionInfo()
{
    ZKCResult   sResult;
    SChar       sConnPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};

    idlOS::strncpy( sConnPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    /* sNodePath�� '/altibase_shard/connection_info'�� ���� ������. */

    idlOS::strncpy( sNodePath,
                    sConnPath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    mMyNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    /* sNodePath�� '/altibase_shard/connection_info/[mMyNodeName]'�� ���� ������. */

    /* connection_info�� �� ����� �̸����� �� �ӽ� ��带 �����Ѵ�.  */
    sResult = aclZKC_insertNode( mZKHandler,
                                 sNodePath,
                                 NULL,
                                 -1,
                                 ACP_TRUE );
 
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    /* connection_info�� ���� ������� �� ��尡 �߰� �� ���� �˸���.*/
    sResult = aclZKC_setInfo( mZKHandler,
                              sConnPath,
                              mMyNodeName,
                              idlOS::strlen( mMyNodeName ) );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::addConnectionInfo            *
 * ------------------------------------------------------------------*
 * ����Ǿ� �ִ� ��������� �����Ѵ�.
 * /altibase_shard/connection_info/[TargetNodeName]���� �����Ǿ� �ִ�
 * ���� �����Ͽ� �ٸ� ���鿡�� ��� ������ ����Ǿ����� �˸���.
 * �˶��� ���� ������ �ٽ� watch�� �߰��Ѵ�.
 *********************************************************************/
IDE_RC sdiZookeeper::removeConnectionInfo( SChar * aTargetNodeName )
{
    ZKCResult   sResult;
    SChar       sConnPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};

    idlOS::strncpy( sConnPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    /* sNodePath�� '/altibase_shard/connection_info'�� ���� ������. */

    idlOS::strncpy( sNodePath,
                    sConnPath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    aTargetNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    /* sNodePath�� '/altibase_shard/connection_info/[aTargetNodeName]'�� ���� ������. */

    /* connection_info�� �����Ѵ�. */
    sResult = aclZKC_deleteNode( mZKHandler, sNodePath );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::checkAllNodeAlive            *
 * ------------------------------------------------------------------*
 * zookeeper�� ����� ��� ��尡 ����ִ��� üũ�Ѵ�.
 * ��� ��尡 ������� ��� true�� �����ϰ� �ϳ��� �׾����� ��� false�� �����Ѵ�.
 * �ش� �۾��� �б� �۾��̶� lock�� ���� �ʿ�� ������
 * ��� ���� Ȯ�� �� ��尡 �߰�/���ŵ� ��츦 ����� lock�� ��� �����Ѵ�.
 * �׾��ִ� ��尡 �ϳ��� ���� ��� add/drop(non-force) �۾���
 * ���ƾ� �ϹǷ� �۾� ������ �ش� �Լ��� ȣ���� üũ�ؾ� �Ѵ�.
 *
 * aIsAlive     - [OUT] ��� ��尡 ����ִ��� ����
 *********************************************************************/
IDE_RC sdiZookeeper::checkAllNodeAlive( idBool   * aIsAlive )
{
    ZKCResult       sResult;
    SChar           sConnPath[] = SDI_ZKC_PATH_CONNECTION_INFO;
    SChar           sNodePath[] = SDI_ZKC_PATH_NODE_META;
    SChar           sCheckPath[] = SDI_ZKC_PATH_META_LOCK;
    SInt            sAllNodeCnt = 0;
    SInt            sAliveNodeCnt = 0;
    String_vector   sChildren;
    idBool          sIsInited = ID_FALSE;

    UInt            sRemoveCnt = 0;
    ZKState         sNodeState = ZK_NONE;

    iduList       * sList = NULL;
    iduListNode   * sIterator = NULL;
    SChar           sNodeName[SDI_NODE_NAME_MAX_SIZE + 1] = {0,};
    idBool          sIsAllocList = ID_FALSE;

    /* R2HA to be remove */
    if ( SDU_SHARD_ZOOKEEPER_TEST == 1 )
    {
        /* Ŭ������ ��Ÿ�� �������� �ʴ´ٸ� lock�� ������ �����Ƿ� �ѹ� üũ�Ѵ�. */
        IDE_TEST( checkPath( sCheckPath, &sIsInited ) != IDE_SUCCESS );

        if( sIsInited == ID_FALSE )
        {
            *aIsAlive = ID_TRUE;
            IDE_CONT( normal_exit );
        }

        /* 1. ����� ����ִ� ����� ���� ��´�. */
        sResult = aclZKC_getChildren( mZKHandler,
                                      sConnPath,
                                      &sChildren );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sAliveNodeCnt = sChildren.count;

        /* 2. ����� ��� ����� ���� ��´�. */
        sResult = aclZKC_getChildren( mZKHandler,
                                      sNodePath,
                                      &sChildren );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        sAllNodeCnt = sChildren.count;

        /* ���� ���Ѵ�. */
        if( sAllNodeCnt == sAliveNodeCnt )
        {
            *aIsAlive = ID_TRUE;
        }
        else
        {
            /* Add �߿� �����ؼ� ���� �ִ� ���� �����Ѵ�. */
            // getAllNodeInfo
            IDE_TEST( getAllNodeNameList( &sList ) != IDE_SUCCESS );
            sIsAllocList = ID_TRUE;

            IDU_LIST_ITERATE( sList, sIterator )
            {
                idlOS::memset( sNodeName, 0, SDI_NODE_NAME_MAX_SIZE + 1 );
                idlOS::strncpy( sNodeName,
                                (SChar*)sIterator->mObj,
                                SDI_NODE_NAME_MAX_SIZE + 1 );
                // check NodeState
                IDE_TEST( getNodeState( sNodeName, &sNodeState ) != IDE_SUCCESS );
                if( sNodeState == ZK_ADD )
                {
                    IDE_TEST( removeNode( sNodeName ) != IDE_SUCCESS );
                    sRemoveCnt++;
                }
            }

            if ( sAllNodeCnt == ( sAliveNodeCnt + sRemoveCnt ) )
            {
                *aIsAlive = ID_TRUE;
            }
            else
            {
                *aIsAlive = ID_FALSE;
            }
        }

        if ( sIsAllocList == ID_TRUE )
        {
            sIsAllocList = ID_FALSE;
            freeList( sList, SDI_ZKS_LIST_NODENAME );
        }

        IDE_EXCEPTION_CONT( normal_exit );
    }
    else
    {
        *aIsAlive = ID_TRUE;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::checkRecentDeadNode          *
 * ------------------------------------------------------------------*
 * � ��尡 ���� �ֱٿ� �׾������� ��ȯ�Ѵ�.
 * ���� �ֱٿ� ���� ��常 failback�� �����ϹǷ� failback�� ȣ��ȴ�.
 * ���� ������ ������ failover history�̴�.
 *
 * aNodeName - [OUT] ���� �ֱٿ� ���� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::checkRecentDeadNode( SChar * aNodeName,
                                          ULong * aFailbackSMN,
                                          ULong * aFailoveredSMN )
{
    ZKCResult   sResult;
    SChar       sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar       sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt        sDataLength;
    SChar     * sCheckBuffer = NULL;
    ULong       sFailbackSMN = 0;
    ULong       sFailoveredSMN = 0;

    idlOS::strncpy( sPath, 
                    SDI_ZKC_PATH_FAILOVER_HISTORY,
                    SDI_ZKC_PATH_LENGTH );
    /* sPath�� '/altibase_shard/cluster_meta/failover_history'�� ���� ������. */

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    sResult = aclZKC_getInfo( mZKHandler,
                              sPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    if( sDataLength > 0 )
    {
        /* failover history�� ����̸�1:SMN/����̸�2:SMN/...�� ���·� �̷���� �����Ƿ� 
         * ù ':'������ ��ġ�� �˸� ���� �ֱٿ� ���� ����� �̸��� �� �� �ִ�. */
        /* NodeName */
        sCheckBuffer = idlOS::strtok( sBuffer, ":" );

        idlOS::strncpy( aNodeName,
                        sCheckBuffer,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        /* SMN */
        sCheckBuffer = idlOS::strtok( NULL, ":" );

        sFailbackSMN = idlOS::strToULong( (UChar*)sCheckBuffer, idlOS::strlen( sCheckBuffer ) );

        sCheckBuffer = idlOS::strtok( NULL, "/" );

        sFailoveredSMN = idlOS::strToULong( (UChar*)sCheckBuffer, idlOS::strlen( sCheckBuffer ) );

        ideLog::log(IDE_SD_0,"[SHARD_FAILBACK] failback SMN : %u, failoverd SMN : %u", sFailbackSMN, sFailoveredSMN);

        if ( aFailbackSMN != NULL )
        {
            *aFailbackSMN = sFailbackSMN;
        }
        if ( aFailoveredSMN != NULL )
        {
            *aFailoveredSMN = sFailoveredSMN;
        }

    }
    else
    {
        /* ���� ��尡 ���� ��� */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::removeNode                   *
 * ------------------------------------------------------------------*
 * Ư�� ����� ������ zookeeper ��Ÿ���� �����Ѵ�.
 * �ش� �Լ����� validation�� �������� �����Ƿ� �ش� �Լ��� ȣ���ϱ� �� �����ؾ� �Ѵ�.
 * �ش� �Լ��� lock�� ���� �����Ƿ� ���ü� ������ �����ȴٸ� lock�� ��� �����ؾ� �Ѵ�.
 * ���� �۾��� multi operation���� �̷������ ������ ������ ������ ����.
 * �ش� �Լ��� DROP �����̳� FAILOVER �������� ADD���� ������ ������ �����Ҷ� ���ȴ�.
 *
 * aNodeName - [OUT] ������ ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::removeNode( SChar * aNodeName )
{
    ZKCResult       sResult;

    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sPath[7][SDI_ZKC_PATH_LENGTH] = {{0,},};

    SInt            i = 0;
    zoo_op_t        sOp[8];
    zoo_op_result_t sOpResult[8];

    idlOS::strncpy( sNodePath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    aNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1);
    /* sNodePath�� '/altibase_shard/node_meta/[aNodeName]'�� ���� ������. */

    /* ��Ÿ ������ ������ multi op�� �����Ѵ�. */
    idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[0],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0],
                    SDI_ZKC_PATH_NODE_ID,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/node_meta/[aNodeName]/shard_node_id'�� ���� ������. */

    zoo_delete_op_init( &sOp[0],
                        sPath[0],
                        -1 );

    idlOS::strncpy( sPath[1],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[1],
                    SDI_ZKC_PATH_NODE_IP,
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    /* sPath[1]�� '/altibase_shard/node_meta/[aNodeName]/node_ip:port'�� ���� ������. */

    zoo_delete_op_init( &sOp[1],
                        sPath[1],
                        -1 );

    idlOS::strncpy( sPath[2],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[2],
                    SDI_ZKC_PATH_NODE_INTERNAL_IP,
                    ID_SIZEOF( sPath[2] ) - idlOS::strlen( sPath[2] ) - 1 );
    /* sPath[2]�� '/altibase_shard/node_meta/[aNodeName]/internal_node_ip:port'�� ���� ������. */

    zoo_delete_op_init( &sOp[2],
                        sPath[2],
                        -1 );

    idlOS::strncpy( sPath[3],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[3],
                    SDI_ZKC_PATH_NODE_REPL_HOST_IP,
                    ID_SIZEOF( sPath[3] ) - idlOS::strlen( sPath[3] ) - 1 );
    /* sPath[3]�� '/altibase_shard/node_meta/[aNodeName]/internal_replication_host_ip:port'�� ���� ������. */

    zoo_delete_op_init( &sOp[3],
                        sPath[3],
                        -1 );

    idlOS::strncpy( sPath[4],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[4],
                    SDI_ZKC_PATH_NODE_CONN_TYPE,
                    ID_SIZEOF( sPath[4] ) - idlOS::strlen( sPath[4] ) - 1 );
    /* sPath[4]�� '/altibase_shard/node_meta/[aNodeName]/conn_type'�� ���� ������. */

    zoo_delete_op_init( &sOp[4],
                        sPath[4],
                        -1 );

    idlOS::strncpy( sPath[5],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[5],
                    SDI_ZKC_PATH_NODE_FAILOVER_TO,
                    ID_SIZEOF( sPath[5] ) - idlOS::strlen( sPath[5] ) - 1 );
    /* sPath[5]�� '/altibase_shard/node_meta/[aNodeName]/failoverTo'�� ���� ������. */

    zoo_delete_op_init( &sOp[5],
                        sPath[5],
                        -1 );

    idlOS::strncpy( sPath[6],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[6],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[6] ) - idlOS::strlen( sPath[6] ) - 1 );
    /* sPath[6]�� '/altibase_shard/node_meta/[aNodeName]/state'�� ���� ������. */

    zoo_delete_op_init( &sOp[6],
                        sPath[6],
                        -1 );

    /* ��� ���� path�� ���ŵ� �Ŀ� ��� path�� ���ŵǾ�� �Ѵ�. */
    zoo_delete_op_init( &sOp[7],
                        sNodePath,
                        -1 );

    /* ������ multi op�� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 8, sOp, sOpResult );

    /* multi op�� ������ �� operation�� ���� ���� ���θ� Ȯ���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i <= 7; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::killNode                     *
 * ------------------------------------------------------------------*
 * Ư�� ����� State�� KILL�� �����ϰ� Connection_info�� �����Ѵ�.
 * �ش� �Լ����� validation�� �������� �����Ƿ� �ش� �Լ��� ȣ���ϱ� �� �����ؾ� �Ѵ�.
 * �ش� �Լ��� lock�� ���� �����Ƿ� ���ü� ������ �����ȴٸ� lock�� ��� �����ؾ� �Ѵ�.
 * ���� �۾��� multi operation���� �̷������ ����� ������ ������ ����.
 * �ش� �Լ��� Failover ����� ��� Node�� KILL�ϱ� ���� ȣ��ȴ�.
 *
 * aNodeName - [OUT] KILL�� ����� �̸�
 *********************************************************************/
IDE_RC sdiZookeeper::killNode( SChar * aNodeName )
{
    ZKCResult       sResult;

    SChar           sNodePath[SDI_ZKC_PATH_LENGTH] = {0,};
    SChar           sPath[2][SDI_ZKC_PATH_LENGTH] = {{0,},};

    SInt            i = 0;
    zoo_op_t        sOp[2];
    zoo_op_result_t sOpResult[2];

    IDE_TEST( validateState( ZK_KILL,
                             aNodeName ) != IDE_SUCCESS );

    idlOS::strncpy( sNodePath,
                    SDI_ZKC_PATH_NODE_META,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sNodePath,
                    "/",
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1 );
    idlOS::strncat( sNodePath,
                    aNodeName,
                    ID_SIZEOF( sNodePath ) - idlOS::strlen( sNodePath ) - 1);
    /* sNodePath�� '/altibase_shard/node_meta/[aNodeName]'�� ���� ������. */

    /* ��Ÿ ������ ������ multi op�� �����Ѵ�. */
    idlOS::memset( sPath[0], 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath[0],
                    sNodePath,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[0],
                    SDI_ZKC_PATH_NODE_STATE,
                    ID_SIZEOF( sPath[0] ) - idlOS::strlen( sPath[0] ) - 1 );
    /* sPath[0]�� '/altibase_shard/node_meta/[aNodeName]/state'�� ���� ������. */

    zoo_set_op_init( &sOp[0],
                     sPath[0],
                     SDI_ZKC_STATE_KILL,
                     idlOS::strlen( SDI_ZKC_STATE_KILL ),
                     -1,
                     NULL );   

    idlOS::strncpy( sPath[1],
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath[1],
                    "/",
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    idlOS::strncat( sPath[1],
                    aNodeName,
                    ID_SIZEOF( sPath[1] ) - idlOS::strlen( sPath[1] ) - 1 );
    /* sPath[1]�� '/altibase_shard/connection_info/[aNodeName]'�� ���� ������. */

    zoo_delete_op_init( &sOp[1],
                        sPath[1],
                        -1 );

    /* ������ multi op�� �����Ѵ�. */
    sResult = aclZKC_multiOp( mZKHandler, 2, sOp, sOpResult );

    /* multi op�� ������ �� operation�� ���� ���� ���θ� Ȯ���Ѵ�. */
    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i < 2; i++ )
    {
        IDE_TEST_RAISE( sOpResult[i].err != ZOK, err_ZKC );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

sdiLocalMetaInfo * sdiZookeeper::getNodeInfo(iduList * aNodeList, SChar * aNodeName)
{
    sdiLocalMetaInfo * sLocalMetaInfo = NULL;
    iduListNode * sNode = NULL;
    iduListNode * sDummy = NULL;

    if ( IDU_LIST_IS_EMPTY( aNodeList ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( aNodeList, sNode, sDummy )
        {
            sLocalMetaInfo = (sdiLocalMetaInfo *)sNode->mObj;
            if( idlOS::strncmp(sLocalMetaInfo->mNodeName, aNodeName, SDI_NODE_NAME_MAX_SIZE) != 0 )
            {
                sLocalMetaInfo = NULL;
            }
            else
            {
                /* return current sLocalMetaInfo */
                break;
            }
        }
    }
    else
    {
        sLocalMetaInfo = NULL;
    }

    return sLocalMetaInfo;
}

IDE_RC sdiZookeeper::checkNodeAlive( SChar   * aNodeName,
                                     idBool  * aIsAlive )
{
    SChar           sPath[SDI_ZKC_PATH_LENGTH] = {0,};
    idBool          sIsAlive  = ID_FALSE;

    idlOS::memset( sPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::strncpy( sPath,
                    SDI_ZKC_PATH_CONNECTION_INFO,
                    SDI_ZKC_PATH_LENGTH );
    idlOS::strncat( sPath,
                    "/",
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    idlOS::strncat( sPath,
                    aNodeName,
                    ID_SIZEOF( sPath ) - idlOS::strlen( sPath ) - 1 );
    /* path�� '/altibase_shard/connection_info/[aNodeName]'�� ���� ������. */

    IDE_TEST( checkPath( sPath,
                         &sIsAlive ) != IDE_SUCCESS );

    * aIsAlive = sIsAlive;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

void sdiZookeeper::setReplInfo4FailoverAfterCommit( SChar * aFirstNodeName,
                                                    SChar * aSecondNodeName,
                                                    SChar * aFirstReplName,
                                                    SChar * aSecondReplName )
{
    idlOS::strncpy( mFirstStopNodeName,
                    aFirstNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopNodeName,
                    aSecondNodeName,
                    SDI_NODE_NAME_MAX_SIZE + 1 );

    idlOS::strncpy( mFirstStopReplName,
                    aFirstReplName,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );
    idlOS::strncpy( mSecondStopReplName,
                    aSecondReplName,
                    SDI_REPLICATION_NAME_MAX_SIZE + 1 );
}

IDE_RC sdiZookeeper::addPendingJob( SChar            * aSQL,
                                    SChar            * aNodeName,
                                    ZKPendingJobType   aPendingType,
                                    qciStmtType        aSQLStmtType )
{
    sdiZKPendingJob * sJob = NULL;
    UInt sJobSQLLen = 0;
    UInt sJobSize = 0;

    sJobSQLLen = idlOS::strlen(aSQL);
    sJobSize   = idlOS::align8((UInt)(offsetof(sdiZKPendingJob, mSQL) + sJobSQLLen + 1));

    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_SDI,
                                     1,
                                     sJobSize,
                                     (void **)&sJob,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );
    sJob->mIsCommitJob = ID_FALSE;
    sJob->mIsRollbackJob = ID_FALSE;
    sJob->mStmtType = QCI_STMT_MASK_MAX;
        
    /* node name copy */
    idlOS::strncpy(sJob->mNodeName, (const SChar*)aNodeName, SDI_NODE_NAME_MAX_SIZE + 1);

    /* job sql copy */
    idlOS::strncpy(sJob->mSQL, (const SChar*)aSQL, sJobSQLLen + 1);

    switch ( aPendingType )
    {
        case ZK_PENDING_JOB_AFTER_COMMIT:
            sJob->mIsCommitJob = ID_TRUE;
            sJob->mStmtType = aSQLStmtType;
            break;
        case ZK_PENDING_JOB_AFTER_ROLLBACK:
            sJob->mIsRollbackJob = ID_TRUE;
            sJob->mStmtType = aSQLStmtType;
            break;
        case ZK_PENDING_JOB_AFTER_END_ALL:
            sJob->mIsRollbackJob = ID_TRUE;
            sJob->mIsCommitJob = ID_TRUE;
            sJob->mStmtType = aSQLStmtType;
            break;
        case ZK_PENDING_JOB_NONE:
            sJob->mIsRollbackJob = ID_FALSE;
            sJob->mIsCommitJob = ID_FALSE;
            sJob->mStmtType = QCI_STMT_MASK_MAX;
            break;
        default:
            IDE_DASSERT(0);
            break;
    }
    
    /* add to list */
    IDU_LIST_INIT_OBJ( &(sJob->mListNode), sJob );

    IDU_LIST_ADD_LAST( &mJobAfterTransEnd, &(sJob->mListNode) );

    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool sdiZookeeper::isExistPendingJob()
{
    if ( IDU_LIST_IS_EMPTY( &mJobAfterTransEnd ) != ID_TRUE )
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

void sdiZookeeper::executePendingJob( ZKPendingJobType aPendingType )
{
    UInt             sPendingDDLLockTimeout = 3;
    iduListNode     *sIterator = NULL;
    iduListNode     *sNextNode = NULL;
    sdiZKPendingJob * sJob = NULL;
    idBool          sIsExecute = ID_FALSE;

    if ( IDU_LIST_IS_EMPTY( &mJobAfterTransEnd ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( &mJobAfterTransEnd, sIterator, sNextNode )
        {
            sJob = (sdiZKPendingJob *)sIterator->mObj;
            IDU_LIST_REMOVE( &(sJob->mListNode) );
            sIsExecute = ID_FALSE;
            switch (aPendingType)
            {
                case ZK_PENDING_JOB_AFTER_COMMIT:
                    if ( sJob->mIsCommitJob == ID_TRUE )
                    {
                        ideLog::logLine(IDE_SD_17,"ZKC commit pending execute: %s", sJob->mSQL);
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_PENDING_JOB_AFTER_ROLLBACK:
                    if ( sJob->mIsRollbackJob == ID_TRUE )
                    {
                        ideLog::logLine(IDE_SD_17,"ZKC rollback pending execute: %s", sJob->mSQL);
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_PENDING_JOB_AFTER_END_ALL:
                    if ( ( sJob->mIsRollbackJob == ID_TRUE ) || ( sJob->mIsCommitJob == ID_TRUE ) )
                    {
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_PENDING_JOB_NONE:
                default:
                    /* only remove */
                    break;
            }
            if ( sIsExecute == ID_TRUE )
            {
                if( sdi::shardExecTempSQLWithoutSession(sJob->mSQL, //idlOS::strlen(sJob->mSQL),
                                                        sJob->mNodeName,
                                                        sPendingDDLLockTimeout,
                                                        sJob->mStmtType ) != IDE_SUCCESS )
                {
                    IDE_ERRLOG(IDE_SD_4);
                    /* ideLog::logErrorMgrStackInternalForDebug(IDE_SD_0); */
                    ideLog::logLine( IDE_SD_4,
                                     "ZKC Pending Job failed: [NODE:%s], [JOB: %s]",
                                     sJob->mNodeName, sJob->mSQL);
                }
                else
                {
                    ideLog::logLine( IDE_SD_17,
                                 "ZKC Pending Job success: [NODE:%s], [JOB: %s]",
                                 sJob->mNodeName, sJob->mSQL);
                }
            }
            (void)iduMemMgr::free(sJob);
        }
    }
    IDE_DASSERT( IDU_LIST_IS_EMPTY( &mJobAfterTransEnd ) == ID_TRUE );

    return;
}

void sdiZookeeper::removePendingJob()
{
    sdiZookeeper::executePendingJob( ZK_PENDING_JOB_NONE );
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::callAfterCommitFunc          *
 * ------------------------------------------------------------------*
 * ���� �� �۾��� ���� �ش��ϴ� AfterCommit �Լ��� ȣ���Ѵ�.
 * �ش� �Լ��� mm ��⿡�� commit�Ҷ� ȣ��Ǹ� 1�� �������� ���� ���� �۾��� �����Ѵ�.
 *
 * aNewSMN         - [IN] �۾� ������ zookeeper ��Ÿ�� ������ SMN
 * aBeforeSMN      - [IN] �۾� ������ zookeeper ��Ÿ�� ����Ǿ� �ִ� SMN
 *********************************************************************/
void sdiZookeeper::callAfterCommitFunc( ULong   aNewSMN, ULong aBeforeSMN )
{
    ULong             sNewSMN = SDI_NULL_SMN;
    smiTrans          sTrans;
    smiStatement    * sDummySmiStmt = NULL;
    UInt              sTxState = 0;

    /* ���� ���°� �ƴ϶�� ��ó�� �� �۾��� ����. */
    if( isConnect() == ID_FALSE )
    {
        removePendingJob();
        IDE_CONT( normal_exit );
    }
    else
    {
        executePendingJob( ZK_PENDING_JOB_AFTER_COMMIT );
    }

    if( aNewSMN == SDI_NULL_SMN )
    {
        /* ���� �������� shard ddl �������� �Ϲ� tx���� shard tx�� ��ȯ�� ��� */
        IDE_TEST( sTrans.initialize() != IDE_SUCCESS );
        sTxState = 1;

        IDE_TEST( sTrans.begin( &sDummySmiStmt,
                                NULL,
                                ( SMI_ISOLATION_NO_PHANTOM     |
                                  SMI_TRANSACTION_NORMAL       |
                                  SMI_TRANSACTION_REPL_DEFAULT |
                                  SMI_COMMIT_WRITE_NOWAIT ) )
                  != IDE_SUCCESS );
        sTxState = 2;

        IDE_TEST( sdi::getIncreasedSMNForMetaNode( &sTrans, &sNewSMN ) != IDE_SUCCESS );

        IDE_TEST( sTrans.commit() != IDE_SUCCESS );
        sTxState = 1;

        sTxState = 0;
        IDE_TEST( sTrans.destroy( NULL ) != IDE_SUCCESS );

        IDE_DASSERT( sNewSMN > sdi::getSMNForDataNode() )
        sdi::setSMNForDataNode(sNewSMN );
        ideLog::log( IDE_SD_17, "[CHANGE_SHARD_META : RELOAD SMN(%"ID_UINT64_FMT")]", sNewSMN );

    }
    else
    {
        sNewSMN = aNewSMN;
    }

    switch( mRunningJobType )
    {
        case ZK_JOB_ADD:
            IDE_DASSERT( sNewSMN != SDI_NULL_SMN );
            IDE_TEST( addNodeAfterCommit( sNewSMN ) != IDE_SUCCESS );
            sdi::setShardStatus(1); // SHARD_STATUS  0 -> 1
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)0, 0, NULL ); //SHARD_ADMIN_MODE 1 -> 0
            break;
        case ZK_JOB_DROP:
            IDE_DASSERT( sNewSMN != SDI_NULL_SMN );
            IDE_TEST( dropNodeAfterCommit( sNewSMN ) != IDE_SUCCESS );
            /* shard status�� SHARD_ADMIN_MODE�� ���� �Լ�(qdsd::executeZookeeperDrop)����
             * �̹� �ٲ��־����Ƿ� ���⼭�� �� �ʿ䰡 ����. */
            qcm::unsetShardNodeID();
            break;
        case ZK_JOB_JOIN:
            IDE_TEST( joinNodeAfterCommit() != IDE_SUCCESS );
            sdi::setShardStatus(1); // SHARD_STATUS  0 -> 1
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)0, 0, NULL ); //SHARD_ADMIN_MODE 1 -> 0
            break;
        case ZK_JOB_FAILOVER:
        case ZK_JOB_FAILOVER_FOR_WATCHER:
            IDE_DASSERT( sNewSMN != SDI_NULL_SMN );
            IDE_DASSERT( aBeforeSMN != SDI_NULL_SMN );
            IDE_TEST( failoverAfterCommit( sNewSMN, aBeforeSMN ) != IDE_SUCCESS );
            break;
        case ZK_JOB_FAILBACK:
            IDE_DASSERT( sNewSMN != SDI_NULL_SMN );
            IDE_TEST( failbackAfterCommit( sNewSMN ) != IDE_SUCCESS );
            sdi::setShardStatus(1); // SHARD_STATUS  0 -> 1
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)0, 0, NULL ); //SHARD_ADMIN_MODE 1 -> 0
            break;
        case ZK_JOB_SHUTDOWN:
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        case ZK_JOB_RESHARD:
            sdiZookeeper::updateSMN( sNewSMN );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        case ZK_JOB_NONE:
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        default:
            IDE_CONT( normal_exit );
        case ZK_JOB_ERROR:
            IDE_DASSERT( 0 );
            break;
    }

    IDE_EXCEPTION_CONT( normal_exit );

    ideLog::log( IDE_SD_20, "[ZOOKEEPER] after commit for smn: SMN(%"ID_UINT64_FMT")", sNewSMN );

    return;

    IDE_EXCEPTION_END;

    switch( sTxState )
    {
        case 2:
            ( void )sTrans.rollback();
        case 1:
            ( void )sTrans.destroy( NULL );
        case 0:
        default:
            break;
    }

    IDE_ERRLOG(IDE_SD_0);
    releaseShardMetaLock();
    releaseZookeeperMetaLock();

    ideLog::log( IDE_SD_4, "[ZOOKEEPER_ERROR] after commit for smn: SMN(%"ID_UINT64_FMT")", aNewSMN );

    return;
}

void sdiZookeeper::callAfterRollbackFunc( ULong   aNewSMN, ULong aBeforeSMN )
{
    IDE_DASSERT(aNewSMN == SDI_NULL_SMN);
    PDL_UNUSED_ARG(aNewSMN);
    PDL_UNUSED_ARG(aBeforeSMN);
    sdiZookeeper::executePendingJob( ZK_PENDING_JOB_AFTER_ROLLBACK );

    switch( mRunningJobType )
    {
        case ZK_JOB_ADD:
            sdi::setShardStatus(0);
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)1, 0, NULL );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            sdiZookeeper::disconnect();
            qcm::unsetShardNodeID();
            break;
        case ZK_JOB_DROP:
            sdi::setShardStatus(1);
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)0, 0, NULL );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        case ZK_JOB_JOIN:
            sdi::setShardStatus(0);
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)1, 0, NULL );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            sdiZookeeper::disconnect();
            qcm::unsetShardNodeID();
            break;
        case ZK_JOB_FAILOVER:
        case ZK_JOB_FAILOVER_FOR_WATCHER:
            sdiZookeeper::failoverAfterRollback();
            sdiZookeeper::setReplInfo4FailoverAfterCommit( SDM_NA_STR, SDM_NA_STR, SDM_NA_STR, SDM_NA_STR );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        case ZK_JOB_FAILBACK:
            sdi::setShardStatus(0);
            (void)idp::update( NULL, "SHARD_ADMIN_MODE", (ULong)1, 0, NULL );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            sdiZookeeper::disconnect();
            qcm::unsetShardNodeID();
            break;
        case ZK_JOB_SHUTDOWN:
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        case ZK_JOB_RESHARD:
            sdiZookeeper::releaseShardMetaLock();
            sdiZookeeper::releaseZookeeperMetaLock();
            break;
        case ZK_JOB_NONE:
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
        default:
        case ZK_JOB_ERROR:
            IDE_DASSERT( 0 );
            releaseShardMetaLock();
            releaseZookeeperMetaLock();
            break;
    }

    return;
}

IDE_RC sdiZookeeper::addRevertJob( SChar           * aSQL,
                                   SChar           * aNodeName,
                                   ZKRevertJobType   aRevertType )
{
    sdiZKRevertJob * sJob = NULL;
    UInt             sJobSQLLen = 0;
    UInt             sJobSize = 0;

    sJobSQLLen = idlOS::strlen(aSQL);
    sJobSize   = idlOS::align8((UInt)(offsetof(sdiZKPendingJob, mSQL) + sJobSQLLen + 1));
    
    IDE_TEST_RAISE(iduMemMgr::calloc(IDU_MEM_SDI,
                                     1,
                                     sJobSize,
                                     (void **)&sJob,
                                     IDU_MEM_IMMEDIATE) != IDE_SUCCESS, InsufficientMemory );
    
    /* node name copy */
    idlOS::strncpy(sJob->mNodeName, (const SChar*)aNodeName, SDI_NODE_NAME_MAX_SIZE + 1);

    /* job sql copy */
    idlOS::strncpy(sJob->mSQL, (const SChar*)aSQL, sJobSQLLen + 1);

    sJob->mRevertJobType = aRevertType;
    
    /* add to list */
    IDU_LIST_INIT_OBJ( &(sJob->mListNode), sJob );

    IDU_LIST_ADD_LAST( &mRevertJobList, &(sJob->mListNode) );

    return IDE_SUCCESS;

    IDE_EXCEPTION(InsufficientMemory);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

idBool sdiZookeeper::isExistRevertJob()
{
    if ( IDU_LIST_IS_EMPTY( &mRevertJobList ) != ID_TRUE )
    {
        return ID_TRUE;
    }
    return ID_FALSE;
}

void sdiZookeeper::executeRevertJob( ZKRevertJobType aRevertType )
{
    UInt              sPendingDDLLockTimeout = 3;
    iduListNode     * sIterator = NULL;
    iduListNode     * sNextNode = NULL;
    sdiZKRevertJob  * sJob = NULL;
    idBool            sIsExecute = ID_FALSE;

    if ( IDU_LIST_IS_EMPTY( &mRevertJobList ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( &mRevertJobList, sIterator, sNextNode )
        {
            sJob = (sdiZKRevertJob *)sIterator->mObj;
            sIsExecute = ID_FALSE;
            
            switch ( aRevertType )
            {
                case ZK_REVERT_JOB_REPL_DROP:
                    if ( sJob->mRevertJobType == ZK_REVERT_JOB_REPL_DROP )
                    {
                        ideLog::logLine(IDE_SD_17,"ZKC revert execute: %s", sJob->mSQL);
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_REVERT_JOB_REPL_ITEM_DROP:
                    if ( sJob->mRevertJobType == ZK_REVERT_JOB_REPL_ITEM_DROP )
                    {
                        ideLog::logLine(IDE_SD_17,"ZKC revert execute: %s", sJob->mSQL);
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_REVERT_JOB_TABLE_ALTER:
                    /* TASK-7307 DML Data Consistency in Shard */
                    if ( sJob->mRevertJobType == ZK_REVERT_JOB_TABLE_ALTER )
                    {
                        ideLog::logLine(IDE_SD_17,"ZKC revert execute: %s", sJob->mSQL);
                        sIsExecute = ID_TRUE;
                    }
                    break;
                case ZK_REVERT_JOB_NONE:
                    IDU_LIST_REMOVE( &(sJob->mListNode) );
                    (void)iduMemMgr::free(sJob);
                    break;
                default:
                    /* only remove */
                    break;
            }
            
            if ( sIsExecute == ID_TRUE )
            {
                IDU_LIST_REMOVE( &(sJob->mListNode) );

                if ( sdi::shardExecTempSQLWithoutSession( sJob->mSQL,
                                                          sJob->mNodeName,
                                                          sPendingDDLLockTimeout,
                                                          QCI_STMT_MASK_MAX )
                     != IDE_SUCCESS )
                {
                    IDE_ERRLOG(IDE_SD_4);
                    /* ideLog::logErrorMgrStackInternalForDebug(IDE_SD_0); */
                    ideLog::logLine( IDE_SD_4,
                                     "ZKC Revert Job failed: [NODE:%s], [JOB: %s]",
                                     sJob->mNodeName, sJob->mSQL);
                }
                else
                {
                    ideLog::logLine( IDE_SD_17,
                                 "ZKC Revert Job success: [NODE:%s], [JOB: %s]",
                                 sJob->mNodeName, sJob->mSQL);
                }
                
                (void)iduMemMgr::free(sJob);
            }
        }
    }
    
    return;
}

void sdiZookeeper::removeRevertJob()
{
    sdiZookeeper::executeRevertJob( ZK_REVERT_JOB_NONE );
}

IDE_RC sdiZookeeper::checkExistNotFailedoverDeadNode( idBool * aExistNotFailedoverDeadNode )
{
    idBool            sExist = ID_FALSE;
    idBool            sIsAlive = ID_FALSE;
    iduList         * sList = NULL;
    iduListNode     * sIterator = NULL;
    SChar             sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    idBool            sIsAllocList = ID_FALSE;
    SInt              sDataLength;
    SChar             sNodeName[SDI_NODE_NAME_MAX_SIZE + 1 ] = {0,};

    IDE_TEST( getAllNodeNameList( &sList ) != IDE_SUCCESS );
    sIsAllocList = ID_TRUE;

    IDU_LIST_ITERATE( sList, sIterator )
    {
        idlOS::strncpy( sNodeName,
                        (SChar *) sIterator->mObj,
                        SDI_NODE_NAME_MAX_SIZE + 1 );

        IDE_TEST( checkNodeAlive( sNodeName,
                                  &sIsAlive ) != IDE_SUCCESS );

        if ( sIsAlive == ID_FALSE )
        {
            sDataLength = SDI_ZKC_BUFFER_SIZE;
            IDE_TEST( getNodeInfo( sNodeName,
                                   (SChar*)SDI_ZKC_PATH_NODE_FAILOVER_TO,
                                   sBuffer,
                                   &sDataLength ) != IDE_SUCCESS );

            if ( sDataLength <= 0 )
            {
                sExist = ID_TRUE;
            }
        }
    }

    *aExistNotFailedoverDeadNode =  sExist;

    if ( sIsAllocList == ID_TRUE )
    {
        sIsAllocList = ID_FALSE;
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsAllocList == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_NODENAME );
    }

    return IDE_FAILURE;
}

IDE_RC sdiZookeeper::getZookeeperSMN( ULong * aSMN )
{

    SInt  sDataLength = 0;
    SChar sBuffer[SDI_ZKC_BUFFER_SIZE];

    idlOS::memset( sBuffer, 0, SDI_ZKC_BUFFER_SIZE );

    sDataLength = SDI_ZKC_BUFFER_SIZE;
    
    IDE_TEST( sdiZookeeper::readInfo( (SChar*)SDI_ZKC_PATH_SMN,
                                      sBuffer,
                                      &sDataLength ) != IDE_SUCCESS );

    *aSMN = idlOS::strToULong ( (UChar*)sBuffer, idlOS::strlen( sBuffer) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::getZookeeperInfo4PV          *
 * ------------------------------------------------------------------*
 * X$ZOOKEEPER_DATA_INFO�� ����ϱ� ���� zookeeper server���� �����͸� �����´�.
 * ����� �����ʹ� list�� ���¸� �����Ƿ� ����� ���� �Ŀ� freeList �Լ��� ȣ���ؾ� �Ѵ�.
 *
 * aList - [OUT] zookeeper���� ������ �����Ͱ� ����� list�� header
 *********************************************************************/
IDE_RC sdiZookeeper::getZookeeperInfo4PV( iduList ** aList )
{
    ZKCResult           sResult;

    /* list */
    idBool              sIsAlloced = ID_FALSE;
    iduList           * sList = NULL;

    /* for */
    SInt                i = 0;
    SInt                j = 0;
    SInt                k = 0;

    /* subpath */
    SChar               sPathLevel1[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector       sChildrenLevel1;
    SChar               sPathLevel2[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector       sChildrenLevel2;
    SChar               sPathLevel3[SDI_ZKC_PATH_LENGTH] = {0,};
    String_vector       sChildrenLevel3;
    SChar               sPathLevel4[SDI_ZKC_PATH_LENGTH] = {0,};

    /* list �ʱ�ȭ */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                  ID_SIZEOF( iduList ),
                                  (void**)&( sList ),
                                  IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    IDU_LIST_INIT( sList );

    sIsAlloced = ID_TRUE;

    /* zookeeper�κ��� �����͸� �޾ƿ´�. */
    idlOS::snprintf( sPathLevel1, 
                     SDI_ZKC_PATH_LENGTH, 
                     "%s", 
                     SDI_ZKC_PATH_ALTIBASE_SHARD );

    IDE_TEST( insertList4PV( sList, sPathLevel1 ) != IDE_SUCCESS );

    sResult = aclZKC_getChildren( mZKHandler,
                                  sPathLevel1,
                                  &sChildrenLevel1 );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    for( i = 0; i < sChildrenLevel1.count; i++ )
    {
        idlOS::memset( sPathLevel2, 0, SDI_ZKC_BUFFER_SIZE );

        idlOS::snprintf( sPathLevel2, 
                         SDI_ZKC_PATH_LENGTH, 
                         "%s/%s", 
                         sPathLevel1, sChildrenLevel1.data[i] );

        IDE_TEST( insertList4PV( sList, sPathLevel2 ) != IDE_SUCCESS );

        sResult = aclZKC_getChildren( mZKHandler,
                                      sPathLevel2,
                                      &sChildrenLevel2 );

        IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

        for( j = 0; j < sChildrenLevel2.count; j++ )
        {
            idlOS::memset( sPathLevel3, 0, SDI_ZKC_BUFFER_SIZE );

            idlOS::snprintf( sPathLevel3, 
                             SDI_ZKC_PATH_LENGTH, 
                             "%s/%s", 
                             sPathLevel2, sChildrenLevel2.data[j] );

            IDE_TEST( insertList4PV( sList, sPathLevel3 ) != IDE_SUCCESS );

            sResult = aclZKC_getChildren( mZKHandler,
                                          sPathLevel3,
                                          &sChildrenLevel3 );

            IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

            for( k = 0; k < sChildrenLevel3.count; k++ )
            {
                idlOS::memset( sPathLevel4, 0, SDI_ZKC_BUFFER_SIZE );

                idlOS::snprintf( sPathLevel4, 
                                 SDI_ZKC_PATH_LENGTH, 
                                 "%s/%s", 
                                 sPathLevel3, sChildrenLevel3.data[k] );

                IDE_TEST( insertList4PV( sList, sPathLevel4 ) != IDE_SUCCESS );

                /* ���� ���������δ� zookeeper data level�� 4�ܰ谡 �Ѱ��̹Ƿ� ���⼭ �����.      *
                 * ���� zookeeper data ������ ����Ǿ� level�� ����� ��� �� ���ϸ� Ž���ؾ� �Ѵ�.*/
            }
        }
    }

    *aList = sList;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    if( sIsAlloced == ID_TRUE )
    {
        freeList( sList, SDI_ZKS_LIST_PRINT );
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : sdiZookeeper::insertList4PV                *
 * ------------------------------------------------------------------*
 * �־��� zookeeper path���� ���� ������ list�� �����Ѵ�.
 * ���� �����Ͱ� ���ٸ� NULL�� ��� �����Ѵ�.
 *
 * aList          - [IN] zookeeper���� ������ �����Ͱ� ����� list�� header
 * aZookeeperPath - [IN] �����͸� ���� zookeeper path 
 *********************************************************************/
IDE_RC sdiZookeeper::insertList4PV( iduList * aList,
                                    SChar   * aZookeeperPath )
{
    ZKCResult               sResult;
    SChar                   sBuffer[SDI_ZKC_BUFFER_SIZE] = {0,};
    SInt                    sDataLength = SDI_ZKC_BUFFER_SIZE;
    iduListNode *           sNode = NULL;
    sdiZookeeperInfo4PV *   sInfo4PV = NULL;
    SInt                    sState = 0;

    IDE_DASSERT( aList != NULL );

    sResult = aclZKC_getInfo( mZKHandler,
                              aZookeeperPath,
                              sBuffer,
                              &sDataLength );

    IDE_TEST_RAISE( sResult != ZKC_SUCCESS, err_ZKC );

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF(iduListNode),
                                 (void**)&(sNode),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SDI,
                                 ID_SIZEOF(sdiZookeeperInfo4PV),
                                 (void**)&(sInfo4PV),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    sState = 2;

    idlOS::memset( sInfo4PV->mPath, 0, SDI_ZKC_PATH_LENGTH );
    idlOS::memset( sInfo4PV->mData, 0, SDI_ZKC_BUFFER_SIZE );

    idlOS::strncpy( sInfo4PV->mPath,
                    aZookeeperPath,
                    SDI_ZKC_PATH_LENGTH );

    if( sDataLength > 0 )
    {
        idlOS::strncpy( sInfo4PV->mData,
                        sBuffer,
                        SDI_ZKC_BUFFER_SIZE );
    }
    else
    {
        idlOS::strncpy( sInfo4PV->mData,
                        "NULL",
                        4 );
    }  

    IDU_LIST_INIT_OBJ( sNode, (void*)sInfo4PV );

    IDU_LIST_ADD_LAST( aList, sNode );

    sState = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_ZKC )
    {
        logAndSetErrMsg( sResult );
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 2:
            (void)iduMemMgr::free( sInfo4PV );
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}
