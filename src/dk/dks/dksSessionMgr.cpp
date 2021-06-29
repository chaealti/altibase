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
 * $id$
 **********************************************************************/

#include <dkDef.h>
#include <dki.h>
#include <dkErrorCode.h>

#include <dksSessionMgr.h>

#include <dkpProtocolMgr.h>
#include <dktGlobalTxMgr.h>
#include <dktGlobalCoordinator.h>

#include <dknLink.h>

dksCtrlSession   dksSessionMgr::mCtrlSession;
dksDataSession   dksSessionMgr::mNotifyDataSession;
UInt             dksSessionMgr::mDataSessionCnt;
iduList          dksSessionMgr::mDataSessionList;
iduMutex         dksSessionMgr::mDksMutex;

/************************************************************************
 * Description : DK session manager �� �ʱ�ȭ�Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::initializeStatic()
{
    mDataSessionCnt = 0;

    /* Linker control session �ʱ�ȭ */
    IDE_TEST( initCtrlSession() != IDE_SUCCESS );
    IDE_TEST( initNotifyDataSession() != IDE_SUCCESS );

    /* Linker data session list �ʱ�ȭ */
    IDU_LIST_INIT( &mDataSessionList );

    /* Linker data session list �� ���� mutex �ʱ�ȭ */
    IDE_TEST_RAISE( mDksMutex.initialize( (SChar *)"DKS_DATA_SESSION_LIST_MUTEX",
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_INIT );
    {
        IDE_SET( ideSetErrorCode( dkERR_FATAL_ThrMutexInit ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Session manager �� linker control session �� �ʱ�ȭ �Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::initCtrlSession()
{
    UInt    sState = 0;

    /* member variables */
    mCtrlSession.mStatus = DKS_CTRL_SESSION_STATUS_NON;
    mCtrlSession.mRefCnt = 0;
    mCtrlSession.mSession.mPortNo = DKS_ALTILINKER_CONNECT_PORT_NO;
    mCtrlSession.mSession.mLink = NULL;

    /* mutex */
    IDE_TEST_RAISE( mCtrlSession.mMutex.initialize( (SChar *)"DKS_SESSION_MANAGER_MUTEX",
                                                     IDU_MUTEX_KIND_POSIX,
                                                     IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT);
    sState = 1;

    IDE_TEST( dknLinkCreate( &(mCtrlSession.mSession.mLink) ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_INIT );
    {
        IDE_SET( ideSetErrorCode( dkERR_FATAL_ThrMutexInit ) );
    }
    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        mCtrlSession.mMutex.destroy();
    }
    else
    {
        /* do nothing */
    }

    return IDE_FAILURE;
}

IDE_RC  dksSessionMgr::initNotifyDataSession()
{
    mNotifyDataSession.mSession.mLinkerDataSessionIsCreated = ID_FALSE;
    mNotifyDataSession.mSession.mIsNeedToDisconnect = ID_FALSE;
    mNotifyDataSession.mSession.mPortNo      = DKS_ALTILINKER_CONNECT_PORT_NO;
    mNotifyDataSession.mId                   = DKP_LINKER_NOTIFY_SESSION_ID;
    mNotifyDataSession.mSession.mLink        = NULL;
    mNotifyDataSession.mSession.mIsXA        = ID_TRUE;

    IDE_TEST( dknLinkCreate( &(mNotifyDataSession.mSession.mLink) ) != IDE_SUCCESS );

    /* initialize member variables */
    mNotifyDataSession.mUserId               = DK_INVALID_USER_ID;
    mNotifyDataSession.mId                   = DKP_LINKER_NOTIFY_SESSION_ID;

    mNotifyDataSession.mLocalTxId            = DK_INIT_LTX_ID;
    mNotifyDataSession.mGlobalTxId           = DK_INIT_GTX_ID;
    mNotifyDataSession.mStatus               = DKS_DATA_SESSION_STATUS_NON;
    mNotifyDataSession.mRefCnt               = 0;
    mNotifyDataSession.mAtomicTxLevel        = DKT_ADLP_TWO_PHASE_COMMIT;
    mNotifyDataSession.mAutoCommitMode       = 0;

    mNotifyDataSession.mStatus               = DKS_DATA_SESSION_STATUS_CREATED;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK session manager �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::finalizeStatic()
{
    if ( dkaLinkerProcessMgr::getLinkerStatus() != DKA_LINKER_STATUS_NON )
    {
        /* >> BUG-37487 : void */
        /* Close all linker data sessions */
        (void)closeAllDataSessions();

#ifdef ALTIBASE_PRODUCT_XDB
        switch ( idwPMMgr::getProcType() )
        {
            case IDU_PROC_TYPE_DAEMON:
                /* Request AltiLinker process shutdown */
                (void)dkpProtocolMgr::doLinkerShutdown( &mCtrlSession.mSession );

                /* Close a linker control session */
                (void)closeCtrlSession();

                break;

            default:
                /* nothing to do */
                break;
        }
#else

        /* Request AltiLinker process shutdown */
        (void)dkpProtocolMgr::doLinkerShutdown( &mCtrlSession.mSession );

        /* Close a linker notify session */
        (void)closeDataSession( &mNotifyDataSession );

        /* Close a linker control session */
        (void)closeCtrlSession();
        /* << BUG-37487 : void */

#endif /* ALTIBASE_PRODUCT_XDB */

    }
    else
    {
        /* success */
    }

    (void)mDksMutex.destroy();

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : Linker data session �� �ϳ� �����ϰ� �ʱ�ȭ�Ͽ� session
 *               manager �� data session list �� �߰����ش�.
 *
 *  aSessionId      - [IN] �����Ϸ��� linker data session ���� �ο��� id
 *  aDataSession    - [OUT] ������ linker data session
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::createDataSession( UInt             aSessionId,
                                          dksDataSession  *aDataSession )
{
    SInt  sPortNo = 0;

    IDE_DASSERT( aDataSession != NULL );

    idlOS::memset( aDataSession, 0, ID_SIZEOF( dksDataSession ) );

    aDataSession->mSession.mLinkerDataSessionIsCreated = ID_FALSE;
    aDataSession->mSession.mIsNeedToDisconnect = ID_FALSE;
    aDataSession->mSession.mLink = NULL;

    if ( dkiIsGTx( DKU_GLOBAL_TRANSACTION_LEVEL ) == ID_TRUE )
    {
        aDataSession->mSession.mIsXA = ID_TRUE;
    }
    else
    {
        aDataSession->mSession.mIsXA = ID_FALSE;
    }

    /* initialize member variables */
    aDataSession->mUserId               = DK_INVALID_USER_ID;
    aDataSession->mId                   = aSessionId;

    aDataSession->mLocalTxId            = DK_INIT_LTX_ID;
    aDataSession->mGlobalTxId           = DK_INIT_GTX_ID;
    aDataSession->mStatus               = DKS_DATA_SESSION_STATUS_NON;
    aDataSession->mRefCnt               = 0;
    aDataSession->mAtomicTxLevel        = (dktAtomicTxLevel)DKU_GLOBAL_TRANSACTION_LEVEL;
    aDataSession->mAutoCommitMode       = DKU_DBLINK_REMOTE_STATEMENT_AUTOCOMMIT;

    if ( ( dkaLinkerProcessMgr::getLinkerStatus() != DKA_LINKER_STATUS_NON ) &&
         ( dkaLinkerProcessMgr::checkAltiLinkerEnabledFromConf() == ID_TRUE ) )
    {
        IDE_TEST( dknLinkCreate( &(aDataSession->mSession.mLink) ) != IDE_SUCCESS );

        IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerPortNoFromConf( &sPortNo )
                  != IDE_SUCCESS );
        aDataSession->mSession.mPortNo = (UShort)sPortNo;
    }
    else
    {
        aDataSession->mSession.mPortNo = 0;
    }

    /* add to list */
    IDU_LIST_INIT_OBJ( &(aDataSession->mNode), aDataSession );

    IDE_TEST( lock() != IDE_SUCCESS );

    IDU_LIST_ADD_LAST( &mDataSessionList, &(aDataSession->mNode) );
    mDataSessionCnt++;

    IDE_TEST( unlock() != IDE_SUCCESS );

    aDataSession->mStatus = DKS_DATA_SESSION_STATUS_CREATED;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* >> BUG-37644 */
    IDE_PUSH();

    if ( aDataSession->mSession.mLink != NULL )
    {
        dknLinkDestroy( aDataSession->mSession.mLink );
    }
    else
    {
        /* nothing to do */
    }

    IDE_POP();
    /* << BUG-37644 */

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �Է¹��� data session �� 1:1 ������ ������ AltiLinker ��
 *               data session �� �ϳ� �����Ѵ�.
 *
 *  aDataSession    - [IN] ������ linker data session
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::createLinkerDataSession( dksDataSession  *aDataSession )
{
    idBool      sIsSentCreateLinkerDataSessionOp = ID_FALSE;
    UInt        sResultCode = DKP_RC_SUCCESS;
    ULong       sTimeoutSec = DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT;

    /* execute protocol operation */
    IDE_TEST( dkpProtocolMgr::sendCreateLinkerDataSession( &aDataSession->mSession,
                                                           aDataSession->mId )
              != IDE_SUCCESS );

    /* BUG-37644 */
    sIsSentCreateLinkerDataSessionOp = ID_TRUE;

    /*
     *  BUG-39226
     *  altilinker ���� CreateLinkerDataSession �� �޾�����
     *  �������̵� ���������̵� ó���� �Ǿ��ٶ�� ���� �Ѵ�.
     *  Result �� �� �޾Ҵٶ�� �ؼ� altilinker ���� DataSession ��
     *  �������� �ʾҴٶ�� ������� ����.
     *  �׸��� ���� Send Socket Buffer ���� �������� altilinker ���� ���� �ȵǾ���
     *  �ϴ��� destroyDataSession OP �� ���� �ǵ� ���ۿ��� ������ ����.
     *  �ٸ� altilinker �� DataSession ���ٴ� ���� �޽����� ���� �߰��� ���̴�.
     */
    aDataSession->mSession.mLinkerDataSessionIsCreated = ID_TRUE;

    IDE_TEST( dkpProtocolMgr::recvCreateLinkerDataSessionResult( &aDataSession->mSession,
                                                                 aDataSession->mId,
                                                                 &sResultCode,
                                                                 sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    if ( ( aDataSession->mId == DKP_LINKER_NOTIFY_SESSION_ID ) ||
         ( dkiIsGTx( aDataSession->mAtomicTxLevel ) == ID_TRUE ) )
    {
        IDE_TEST( dkpProtocolMgr::sendSetAutoCommitMode( &aDataSession->mSession,
                                                         aDataSession->mId,
                                                         0 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( dkpProtocolMgr::sendSetAutoCommitMode( &aDataSession->mSession,
                                                         aDataSession->mId,
                                                         (UChar)aDataSession->mAutoCommitMode )    
                  != IDE_SUCCESS );
    }

    IDE_TEST( dkpProtocolMgr::recvSetAutoCommitModeResult( &aDataSession->mSession,
                                                           aDataSession->mId,
                                                           &sResultCode,
                                                           sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }
    IDE_EXCEPTION_END;

    /* >> BUG-37644 */
    IDE_PUSH();

    if ( sIsSentCreateLinkerDataSessionOp == ID_TRUE )
    {
        (void)closeDataSession( aDataSession );
    }
    else
    {
        /* do nothing */
    }

    (void)disconnect( &aDataSession->mSession, ID_FALSE );

    IDE_POP();
    /* << BUG-37644 */

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Linker data session �� session manager �� data session
 *               list �κ��� �����ϰ� �ش� session �� ���õ� ��� �ڿ���
 *               �ݳ��Ѵ�.
 *
 *  aSession    - [IN] ������ linker data session
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::destroyDataSession( dksDataSession  *aSession )
{
    dktGlobalCoordinator    *sGlobalCrd = NULL;

    IDE_TEST( dktGlobalTxMgr::findGlobalCoordinatorWithSessionId( aSession->mId,
                                                                  &sGlobalCrd )
              != IDE_SUCCESS );

    if ( sGlobalCrd != NULL )
    {
        /* Finalize global transaction */
        dktGlobalTxMgr::destroyGlobalCoordinatorAndUnSetSessionTxId( sGlobalCrd, aSession );
    }
    else
    {
        /* do nothing */
    }

    IDE_TEST( lock() != IDE_SUCCESS );

    IDU_LIST_REMOVE( &(aSession->mNode) );
    mDataSessionCnt--;

    IDE_TEST( unlock() != IDE_SUCCESS );

    if ( aSession->mSession.mLink != NULL )
    {
        dknLinkDestroy( aSession->mSession.mLink );
        aSession->mSession.mLink = NULL;
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �Է¹��� DK linker session �� ���� AltiLinker ���μ�����
 *               connection �� �õ��Ѵ�.
 *
 *  aPort       - [IN] ������ port ��ȣ, AltiLinker process�� local ��
 *                     ��ġ�ϹǷ� port ��ȣ�� �ָ�ȴ�.
 *  aDksSession - [OUT] connection �� ���� �ʱ�ȭ ���� ����ü
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::connect( dksSession   *aDksSession )
{
    idBool             sIsConnected = ID_FALSE;

    IDE_TEST_RAISE( aDksSession == NULL, ERR_SESSION_NOT_EXIST );

    IDE_TEST( checkConnection( aDksSession, &sIsConnected ) != IDE_SUCCESS );

    if ( sIsConnected != ID_TRUE )
    {
        IDE_TEST( dknLinkConnect( aDksSession->mLink,
                                  DKS_ALTILINKER_CONNECT_HOST_NAME,
                                  aDksSession->mPortNo,
                                  DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT )
                  != IDE_SUCCESS );

        aDksSession->mIsNeedToDisconnect = ID_FALSE;
    }
    else
    {
        /* already connected */
        aDksSession->mIsNeedToDisconnect = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_NOT_EXIST );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKS_SESSION_NOT_EXIST ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::connectDataSession( dksDataSession * aDataSession )
{
    SInt  sPortNo = 0;

    IDE_TEST_RAISE( aDataSession == NULL, ERR_DATA_SESSION_NOT_EXIST );

    if ( aDataSession->mSession.mLink == NULL )
    {
        IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerPortNoFromConf( &sPortNo )
                  != IDE_SUCCESS );
        aDataSession->mSession.mPortNo = (UShort)sPortNo;

        IDE_TEST( dknLinkCreate( &(aDataSession->mSession.mLink) )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_DASSERT( aDataSession->mSession.mPortNo != 0 );
    }

    IDE_TEST( connect( &(aDataSession->mSession) )
              != IDE_SUCCESS );

    if ( aDataSession->mSession.mLinkerDataSessionIsCreated == ID_FALSE )
    {
        IDE_TEST( createLinkerDataSession( aDataSession ) != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DATA_SESSION_NOT_EXIST );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKM_DATA_SESSION_NOT_EXIST ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::invalidateLinkerDataSession( dksDataSession * aDataSession )
{
    IDE_TEST_RAISE( aDataSession == NULL, ERR_DATA_SESSION_NOT_EXIST );

    aDataSession->mSession.mLinkerDataSessionIsCreated = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DATA_SESSION_NOT_EXIST );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKM_DATA_SESSION_NOT_EXIST ) );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::connectControlSessionInternal( UShort aPortNo )
{
    mCtrlSession.mSession.mPortNo = aPortNo;

    IDE_TEST( connect( &(mCtrlSession.mSession) ) != IDE_SUCCESS );

    mCtrlSession.mStatus = DKS_CTRL_SESSION_STATUS_CONNECTED;

    IDE_TEST_RAISE( createCtrlSessionProtocol( &(mCtrlSession.mSession ) )
                    != IDE_SUCCESS, ERR_CREATE_CTRL_SESSION_FAILED );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_CREATE_CTRL_SESSION_FAILED );
    {
        IDE_ERRLOG( DK_TRC_LOG_FORCE );
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKM_CREATE_CTRL_SESSION_FAILED ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( mCtrlSession.mStatus == DKS_CTRL_SESSION_STATUS_CONNECTED )
    {
        (void)disconnectControlSession();
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::connectControlSession( UShort aPortNo )
{
#ifdef ALTIBASE_PRODUCT_XDB

    switch ( idwPMMgr::getProcType() )
    {
        case IDU_PROC_TYPE_DAEMON:
            IDE_TEST( connectControlSessionInternal( aPortNo ) != IDE_SUCCESS );
            break;

        default:
            mCtrlSession.mStatus = DKS_CTRL_SESSION_STATUS_CONNECTED;
            break;
    }

#else

    IDE_TEST( connectControlSessionInternal( aPortNo ) != IDE_SUCCESS );

#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK linker session �� �ΰ��ִ� AltiLinker ���μ�������
 *               ������ �����Ѵ�.
 *
 *  aDksSession - [IN] disconnect �� linker session �� cm session ����
 *  aIsReuse    - [IN] cm link �� ������ ��� ID_TRUE, �ƴϸ� ID_FALSE
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::disconnect( dksSession *aDksSession, idBool aIsReuse )
{
    if ( aDksSession->mLink != NULL )
    {
        IDE_TEST( dknLinkDisconnect( aDksSession->mLink, aIsReuse ) != IDE_SUCCESS );
    }
    else
    {
        /* not connected */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::disconnectDataSession( dksDataSession  *aDataSession )
{
    IDE_TEST( disconnect( &(aDataSession->mSession), ID_TRUE) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::checkAndDisconnectDataSession( dksDataSession  *aDataSession )
{
    if ( aDataSession->mSession.mIsNeedToDisconnect == ID_TRUE )
    {
        (void)disconnectDataSession( aDataSession );
    }
    else
    {
        /* connected */
    }

    return IDE_SUCCESS;
}

/*
 *
 */
IDE_RC dksSessionMgr::disconnectControlSessionInternal( void )
{
    mCtrlSession.mStatus = DKS_CTRL_SESSION_STATUS_DISCONNECTED;

    IDE_TEST( disconnect( &(mCtrlSession.mSession), ID_FALSE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::disconnectControlSession( void )
{
#ifdef ALTIBASE_PRODUCT_XDB
    switch ( idwPMMgr::getProcType() )
    {
        case IDU_PROC_TYPE_DAEMON:
            IDE_TEST( disconnectControlSessionInternal() != IDE_SUCCESS );
            break;

        default:
            /* nothing to do */
            break;
    }
#else

    IDE_TEST( disconnectControlSessionInternal() != IDE_SUCCESS );

#endif /* ALTIBASE_PRODUCT_XDB */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK linker session �� ������ ���������� üũ�Ѵ�.
 *
 *  aDksSession     - [IN] üũ�� linker session �� cm ��������
 *  aIsConnected    - [OUT] ������ ����ִ��� ���η� ID_TRUE �� ���,
 *                          ������ ��������� �ǹ��Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::checkConnection( dksSession  *aDksSession,
                                        idBool      *aIsConnected )
{
    IDE_TEST( dknLinkCheck( aDksSession->mLink, aIsConnected ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : AltiLinker process �� �Ͽ��� linker control session ��
 *               �����ϰ� �ش� �������� connection �� �ε��� protocol
 *               operation �� �����Ѵ�. �� operation �� ����������
 *               ����ǰ� ���� Altilinker �� altibase server �� �����
 *               Ȱ��ȭ�Ǿ� ADLP �������ݿ� ���� database link �����
 *               ����������.
 *
 *  aSession    - [IN] AltiLinker ���μ����� ������ �α� ���� cm link ��
 *                     ���� �ִ� dk �� linker control session �� ����
 *                     dksSession
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::createCtrlSessionProtocol( dksSession   *aSession )
{
    UInt     sProtocolVer;
    UInt     sResultCode  = DKP_RC_SUCCESS;
    UInt     sDBCharSet;
    ULong    sTimeoutSec  = DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT;
    SChar   *sProdInfoStr = NULL;

    sProdInfoStr = dkaLinkerProcessMgr::getServerInfoStr();

    (void)dkaLinkerProcessMgr::resetDBCharSet();
    sDBCharSet = dkaLinkerProcessMgr::getDBCharSet();

    IDE_TEST( dkpProtocolMgr::sendCreateLinkerCtrlSession( aSession,
                                                           sProdInfoStr,
                                                           sDBCharSet )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvCreateLinkerCtrlSessionResult( aSession,
                                                                 &sResultCode,
                                                                 &sProtocolVer,
                                                                 sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( dkaLinkerProcessMgr::isPropertiesLoaded() != ID_FALSE )
    {
        (void)dkaLinkerProcessMgr::setIsPropertiesLoaded( ID_FALSE );
        (void)dkaLinkerProcessMgr::initLinkerProcessInfo();
    }
    else
    {
        /* AltiLinker process not exist */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK linker control session �� AltiLinker ���μ�����
 *               �ΰ��ִ� ������ �����ϰ� �ش� ������ �ڿ��� ��ȯ�Ѵ�.
 *
 *  BUG-37312 : AltiLinker shutdown delay
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::closeCtrlSessionInternal()
{
    /* BUG-37600 */
    IDE_TEST( disconnect( &mCtrlSession.mSession, ID_FALSE )
              != IDE_SUCCESS );

    mCtrlSession.mStatus = DKS_CTRL_SESSION_STATUS_DISCONNECTED;

    dknLinkDestroy( mCtrlSession.mSession.mLink );
    mCtrlSession.mSession.mLink = NULL;

    /* Destroy mutex */
    IDE_TEST( mCtrlSession.mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dksSessionMgr::closeCtrlSession( void )
{
#ifdef ALTIBASE_PRODUCT_XDB
    switch ( idwPMMgr::getProcType() )
    {
        case IDU_PROC_TYPE_DAEMON:
            IDE_TEST( closeCtrlSessionInternal() != IDE_SUCCESS );
            break;

        default:
            /* nothing to do */
            break;
    }
#else

    IDE_TEST( closeCtrlSessionInternal() != IDE_SUCCESS );

#endif /* ALTIBASE_PRODUCT_XDB */

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : AltiDK linker data session �� AltiLinker ���μ�����
 *               �ΰ��ִ� ������ �����Ѵ�.
 *
 *  aSession     - [IN] close �Ϸ��� linker data session
 *
 *  1 Linker Data Session �� �����Ѵٸ�,
 *     1) ��Ƽ��Ŀ�� ����
 *     2) ��� Linker Data Session�� close�϶�� ����
 *     3) ��Ƽ��Ŀ���� ���� ����
 *  2. �������� �ʴ´ٸ� do nothing
 *  3. ����
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::closeDataSession( dksDataSession *aSession )
{
    UInt    sResultCode = DKP_RC_SUCCESS;
    ULong   sTimeoutSec = DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT;
    idBool  sConnected = ID_FALSE;

    IDE_ASSERT( aSession != NULL );

    if ( aSession->mSession.mLinkerDataSessionIsCreated == ID_TRUE )
    {
        if ( connect( &aSession->mSession ) == IDE_SUCCESS )
        {
            sConnected = ID_TRUE;

            /* Request AltiLinker process to close a linker data session */
            IDE_TEST( dkpProtocolMgr::sendDestroyLinkerDataSession( &aSession->mSession,
                                                                    aSession->mId )
                      != IDE_SUCCESS );

            IDE_TEST( dkpProtocolMgr::recvDestroyLinkerDataSessionResult( &aSession->mSession,
                                                                          aSession->mId,
                                                                          &sResultCode,
                                                                          sTimeoutSec )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
            
            /* success */
            aSession->mSession.mLinkerDataSessionIsCreated = ID_FALSE;

            sConnected = ID_FALSE;
            IDE_TEST( disconnect( &aSession->mSession, ID_FALSE ) != IDE_SUCCESS );
        }
        else
        {
            sConnected = ID_FALSE;
        }
    }
    else
    {
        /* nothing to do */
    }

    aSession->mStatus = DKS_DATA_SESSION_STATUS_DISCONNECTED;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sConnected == ID_TRUE )
    {
        (void)disconnect( &aSession->mSession, ID_FALSE );
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK �� �����ϴ� ��� linker data session �� ����
 *               AltiLinker ���μ����� �ΰ��ִ� ������ �����ϰ�
 *               �ش� ������ �ڿ��� ��ȯ�Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::closeAllDataSessions()
{
    iduListNode     *sIterator       = NULL;
    iduListNode     *sNextNode       = NULL;
    dksDataSession  *sSession        = NULL;

    if ( getDataSessionCount() != 0 )
    {
        if ( IDU_LIST_IS_EMPTY( &mDataSessionList ) != ID_TRUE )
        {
            IDU_LIST_ITERATE_SAFE( &mDataSessionList, sIterator, sNextNode )
            {
                sSession = (dksDataSession *)sIterator->mObj;

                /* BUG-37487 : void */
                (void)closeDataSession( sSession );
                IDE_TEST( destroyDataSession( sSession ) != IDE_SUCCESS );
            }
        }
        else
        {
            /* there is no data sessions to close */
        }
    }
    else
    {
        /* no data session */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : DK �� �����ϴ� ��� linker data session �� ����
 *               AltiLinker ���μ����� �ΰ��ִ� ������ �����ϰ�
 *               �ش� ������ �ڿ��� ��ȯ�Ѵ�.
 *
 *  aSession     - [IN] close �Ϸ��� linker data session
 *  aDblinkName  - [IN] close �Ϸ��� linker data session �� ����ϰ� �ִ�
 *                      database link �̸�
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::closeRemoteNodeSession( idvSQL           *aStatistics,
                                               dksDataSession   *aSession,
                                               SChar            *aDblinkName )
{
    UShort                sRemoteNodeSessionId = 0;
    UInt                  sRemainedRemoteNodeCnt = 0;
    UInt                  sTimeoutSec = DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT;
    UInt                  sResultCode = DKP_RC_SUCCESS;
    dkoLink               sLinkObj;
    dktRemoteTx          *sRemoteTx   = NULL;
    dktGlobalCoordinator *sGlobalCrd  = NULL;

    if ( aDblinkName != NULL )
    {
        IDE_TEST( dkoLinkObjMgr::findLinkObject( aStatistics, aDblinkName, &sLinkObj )
                  != IDE_SUCCESS );

        IDE_TEST( dktGlobalTxMgr::findGlobalCoordinator( aSession->mGlobalTxId,
                                                         &sGlobalCrd )
                  != IDE_SUCCESS );

        if ( sGlobalCrd != NULL )
        {
            IDE_TEST( sGlobalCrd->findRemoteTxWithTarget( sLinkObj.mTargetName,
                                                          &sRemoteTx )
                      != IDE_SUCCESS );

            IDE_TEST_RAISE( sRemoteTx == NULL, ERR_INVALID_TRANSACTION );

            sRemoteNodeSessionId = sRemoteTx->getRemoteNodeSessionId();
        }
        else
        {
            /* this session has no transaction */
            sRemoteNodeSessionId = 0;
        }
    }
    else
    {
        /* set '0' to close all remote node session */
        sRemoteNodeSessionId = 0;
    }

    IDE_TEST( dkpProtocolMgr::sendRequestCloseRemoteNodeSession( &(aSession->mSession),
                                                                 aSession->mId,
                                                                 sRemoteNodeSessionId )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestCloseRemoteNodeSessionResult( &(aSession->mSession),
                                                                       aSession->mId,
                                                                       sRemoteNodeSessionId,
                                                                       &sResultCode,
                                                                       &sRemainedRemoteNodeCnt,
                                                                       sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    if ( sRemainedRemoteNodeCnt == 0 )
    {
        IDE_TEST( closeDataSession( aSession ) != IDE_SUCCESS );
        IDE_TEST( destroyDataSession( aSession ) != IDE_SUCCESS );
    }
    else
    {
        /* BUG-37665 */
        if ( sGlobalCrd != NULL )
        {
            sGlobalCrd->destroyRemoteTx( sRemoteTx );
        }
        else
        {
            /* do nothing */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_TRANSACTION )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKM_INVALID_TRANSACTION ) );
    }
    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Performance view �� ���� ��� linker session ���� ������
 *               ����.
 *
 *  aInfo       - [OUT] linker session �������� ��� �ִ� ����ü�迭
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::getLinkerSessionInfo( dksLinkerSessionInfo ** aInfo, UInt * aSessionCnt )
{
    UInt   i                 = 0;
    UInt   sLinkerSessionCnt = 0;
    idBool sIsLock           = ID_FALSE;

    iduListNode     	 *sIterator    = NULL;
    dksDataSession  	 *sDataSession = NULL;
    dksLinkerSessionInfo *sInfo		   = NULL;

    IDE_ASSERT( mDksMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    sIsLock = ID_TRUE;

    sLinkerSessionCnt = dksSessionMgr::getLinkerSessionCount();

    IDU_FIT_POINT_RAISE( "dkmGetLinkerSessionInfo::malloc::Info",
                         ERR_MEMORY_ALLOC );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       ID_SIZEOF( dksLinkerSessionInfo ) * sLinkerSessionCnt,
                                       (void **)&sInfo,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC );

    /* Get linker control session info */
    sInfo[0].mId     = DKS_LINKER_CONTROL_SESSION_ID;
    sInfo[0].mStatus = (UInt)mCtrlSession.mStatus;
    sInfo[0].mType   = DKS_LINKER_SESSION_TYPE_CONTROL;

    i = 1;
    IDU_LIST_ITERATE( &mDataSessionList, sIterator )
    {
        sDataSession = (dksDataSession *)sIterator->mObj;

		IDE_TEST_CONT( i == sLinkerSessionCnt, FOUND_COMPLETE );

        if ( sDataSession != NULL )
        {
            sInfo[i].mId     = sDataSession->mId;
            sInfo[i].mStatus = (UInt)sDataSession->mStatus;
            sInfo[i].mType   = DKS_LINKER_SESSION_TYPE_DATA;

            i++;
        }
        else
        {
            /* no more data session */
        }
    }

    IDE_EXCEPTION_CONT( FOUND_COMPLETE );

    *aInfo = sInfo;
    *aSessionCnt = sLinkerSessionCnt;

    sIsLock = ID_FALSE;
    IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sInfo != NULL )
    {
        (void)iduMemMgr::free( sInfo );
        sInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    if ( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }
    
    return IDE_FAILURE;
}

/************************************************************************
 * Description : Performance view �� ���� linker control session ������
 *               ����.
 *
 *  aInfo       - [OUT] linker control session ������ ��� �ִ� ����ü
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::getLinkerCtrlSessionInfo( dksCtrlSessionInfo *aInfo )
{
    IDE_ASSERT( mCtrlSession.mMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    /* Get linker control session info */
    aInfo->mStatus         = mCtrlSession.mStatus;
    aInfo->mReferenceCount = mCtrlSession.mRefCnt;

    IDE_ASSERT( mCtrlSession.mMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : Performance view �� ���� linker data session ������
 *               ����.
 *
 * aInfo       - [OUT] linker data session ���� ������ ��� �ִ� ����ü
 *                     �迭
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::getLinkerDataSessionInfo( dksDataSessionInfo ** aInfo, UInt * aSessionCnt )
{
    UInt   i               = 0;
    UInt   sDataSessionCnt = 0;
    idBool sIsLock         = ID_FALSE;

    iduListNode        * sIterator    = NULL;
    dksDataSession     * sDataSession = NULL;
    dksDataSessionInfo * sInfo        = NULL;

    IDE_ASSERT( mDksMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    sIsLock = ID_TRUE;

    sDataSessionCnt = dksSessionMgr::getDataSessionCount();

    if ( sDataSessionCnt > 0 )
    {
        IDU_FIT_POINT_RAISE( "dkmGetLinkerDataSessionInfo::malloc::Info",
                             ERR_MEMORY_ALLOC );
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                           ID_SIZEOF( dksDataSessionInfo ) * sDataSessionCnt,
                                           (void **)&sInfo,
                                           IDU_MEM_IMMEDIATE )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOC );

        IDU_LIST_ITERATE( &mDataSessionList, sIterator )
        {
            sDataSession = (dksDataSession *)sIterator->mObj;

            IDE_TEST_CONT( i == sDataSessionCnt, FOUND_COMPLETE );

            if ( sDataSession != NULL )
            {
                sInfo[i].mId     = sDataSession->mId;
                sInfo[i].mStatus = sDataSession->mStatus;
                sInfo[i].mLocalTxId  = sDataSession->mLocalTxId;
                sInfo[i].mGlobalTxId = sDataSession->mGlobalTxId;

                i++;
            }
            else
            {
                /* no more data session */
            }
        }
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( FOUND_COMPLETE );

    *aInfo = sInfo;
    *aSessionCnt = sDataSessionCnt;

    sIsLock = ID_FALSE;
    IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sInfo != NULL )
    {
        (void)iduMemMgr::free( sInfo );
        sInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    if ( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ���ǰ����ڰ� �����ϴ� linker data session list ��
 *               lock �� ȹ���Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::lock()
{
    IDE_ASSERT( mDksMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : ���ǰ����ڰ� �����ϴ� linker data session list ��
 *               ���� lock �� release �Ѵ�.
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::unlock()
{
    IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : ���ǰ����ڰ� �����ϴ� linker data session list ��
 *               ���� lock �� release �Ѵ�.
 *
 *  aSessionId  - [IN] linker data session �� id
 *  aSession    - [OUT] cm link �� ����ϱ� ���� linker data session ��
 *                      ���� �ִ� dksSession
 *
 ************************************************************************/
IDE_RC  dksSessionMgr::getDataDksSession( UInt  aSessionId, dksSession **aSession )
{
    dksDataSession * sDataSession = NULL;
    iduListNode    * sIterator    = NULL;
    dksSession     * sSession     = NULL; 

    IDE_ASSERT( mDksMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    IDU_LIST_ITERATE( &mDataSessionList, sIterator )
    {
        sDataSession = (dksDataSession *)sIterator->mObj;

        if ( getDataSessionId( sDataSession ) == aSessionId )
        {
            sSession = &sDataSession->mSession;
            break;
        }
        else
        {
            /* no more data session */
        }
    }

    IDE_ASSERT( mDksMutex.unlock() == IDE_SUCCESS );

    IDE_TEST_RAISE( sSession == NULL, ERR_SESSION_NOT_EXIST );

    *aSession = sSession;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SESSION_NOT_EXIST );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKS_SESSION_NOT_EXIST ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

