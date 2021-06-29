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

#include <idu.h>

#include <dki.h>
#include <dktGlobalTxMgr.h>
#include <dktGlobalCoordinator.h>
#include <dksSessionMgr.h>
#include <smiDef.h>
#include <smiMisc.h>
#include <dktNotifier.h>
#include <dkuProperty.h>
#include <dkm.h>

/************************************************************************
 * Description : Global coordinator �� �ʱ�ȭ�Ѵ�.
 *
 *  aSession    - [IN] �� global coordinator �� ���� linker data session
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::initialize( dksDataSession * aSession )
{
	idBool sIsMutexInit = ID_FALSE;

    IDE_ASSERT( aSession != NULL );

    /* BUG-44672
     * Performance View ��ȸ�Ҷ� RemoteTransactioin �� �߰� ������ PV �� ��ȸ�ϸ� ���ü� ������ �����.
     * �̸� �����ϱ� ���� Lock ���� �Ϲ� DK ���� findRemoteTransaction �� ���� �Լ���
     * ���� DK ���ǿ����� �����Ƿ� ���ü��� ������ �����Ƿ�
     * RemoteTransaction Add �� Remove �� �����ϰ�� Lock �� ���� �ʴ´�. */
    IDE_TEST_RAISE( mDktRTxMutex.initialize( (SChar *)"DKT_REMOTE_TRANSACTION_MUTEX",
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT );
	sIsMutexInit = ID_TRUE;

    /* Initialize members */
    mSessionId          = dksSessionMgr::getDataSessionId( aSession );
    mLocalTxId          = DK_INIT_LTX_ID;
    mGlobalTxId         = DK_INIT_GTX_ID;
    mGTxStatus          = DKT_GTX_STATUS_NON;
    mAtomicTxLevel      = dksSessionMgr::getDataSessionAtomicTxLevel( aSession );
    mRTxCnt             = 0;
    mCurRemoteStmtId    = 0;
    mLinkerType         = DKT_LINKER_TYPE_NONE;
    mDtxInfo            = NULL;
    mFlag               = 0;
    mShardClientInfo    = NULL;
    mIsGTx              = dkiIsGTx( mAtomicTxLevel );
    mIsGCTx             = dkiIsGCTx( mAtomicTxLevel );

    dktXid::initXID( &mGlobalXID );

    IDE_TEST_RAISE( mCoordinatorDtxInfoMutex.initialize( (SChar *)"DKT_COORDINATOR_MUTEX",
                                                         IDU_MUTEX_KIND_POSIX,
                                                         IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS, ERR_MUTEX_INIT );
    /* Remote transaction �� ������ ���� list �ʱ�ȭ */
    IDU_LIST_INIT( &mRTxList );

    /* Savepoint �� ������ ���� list �ʱ�ȭ */
    IDU_LIST_INIT( &mSavepointList );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MUTEX_INIT );
    {
        IDE_SET( ideSetErrorCode( dkERR_FATAL_ThrMutexInit ) );
    }
    IDE_EXCEPTION_END;

	if ( sIsMutexInit == ID_TRUE )
	{
		sIsMutexInit = ID_FALSE;
	    (void)mDktRTxMutex.destroy();
	}
	else
	{
		/* nothing to do */
	}

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Global coordinator �� ���� �ִ� �ڿ��� �����Ѵ�.
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktGlobalCoordinator::finalize()
{
    iduListNode     *sIterator  = NULL;
    iduListNode     *sNext      = NULL;
    dktRemoteTx     *sRemoteTx  = NULL;
    dktSavepoint    *sSavepoint = NULL;

    if ( IDU_LIST_IS_EMPTY( &mRTxList ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;
            destroyRemoteTx( sRemoteTx );
        }
    }
    else
    {
        /* there is no remote transaction */
    }

    if ( IDU_LIST_IS_EMPTY( &mSavepointList ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( &mSavepointList, sIterator, sNext )
        {
            sSavepoint = (dktSavepoint *)sIterator->mObj;
            destroySavepoint( sSavepoint );
        }
    }
    else
    {
        /* there is no savepoint */
    }

    /* PROJ-2569 notifier���� �̰��� dktGlobalTxMgr�� globalCoordinator�� finalize ȣ�� ���� �Ѵ�.
     * dtxInfo �޸� ������ commit/rollback�� ���� �� ���� ���θ� ���� �װ����� �Ѵ�.
     * mDtxInfo         = NULL; */

    mGlobalTxId      = DK_INIT_GTX_ID;
    mLocalTxId       = DK_INIT_LTX_ID;
    mGTxStatus       = DKT_GTX_STATUS_NON;
    mRTxCnt          = 0;
    mCurRemoteStmtId = DK_INVALID_STMT_ID;
    mLinkerType      = DKT_LINKER_TYPE_NONE;
    mFlag            = 0;
    mShardClientInfo = NULL;
    mIsGTx           = ID_FALSE;
    mIsGCTx          = ID_FALSE;

    (void) mCoordinatorDtxInfoMutex.destroy();

    (void)mDktRTxMutex.destroy();
}

/************************************************************************
 * Description : Remote transaction �� �����Ͽ� list �� �߰��Ѵ�.
 *
 *  aSession    - [IN] Linker data session 
 *  aLinkObjId  - [IN] Database link �� id
 *  aRemoteTx   - [OUT] ������ remote transaction
 *  
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::createRemoteTx( idvSQL          *aStatistics,
                                              dksDataSession  *aSession,
                                              dkoLink         *aLinkObj,
                                              dktRemoteTx    **aRemoteTx )
{
    UInt             sRemoteTxId;
    dktRemoteTx     *sRemoteTx   = NULL;
    idBool           sIsAlloced  = ID_FALSE;
    idBool           sIsInited   = ID_FALSE;
    idBool           sIsAdded    = ID_FALSE;

    /* �̹� shard�� ����ϰ� �ִ� ��� ���� */
    IDE_TEST_RAISE( mLinkerType == DKT_LINKER_TYPE_SHARD,
                    ERR_SHARD_TX_ALREADY_EXIST );

    IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::createRemoteTx::malloc::RemoteTx",
                          ERR_MEMORY_ALLOC_REMOTE_TX );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       ID_SIZEOF( dktRemoteTx ),
                                       (void **)&sRemoteTx,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_REMOTE_TX );
    sIsAlloced = ID_TRUE;

    /* Generate remote transaction id */
    sRemoteTxId = generateRemoteTxId( aLinkObj->mId );

    IDE_TEST( sRemoteTx->initialize( aSession->mId,
                                     aLinkObj->mTargetName,
                                     DKT_LINKER_TYPE_DBLINK,
                                     NULL,
                                     mGlobalTxId,
                                     mLocalTxId,
                                     sRemoteTxId )
              != IDE_SUCCESS );
    sIsInited = ID_TRUE;

    IDU_LIST_INIT_OBJ( &(sRemoteTx->mNode), sRemoteTx );

    IDE_ASSERT( mDktRTxMutex.lock( aStatistics ) == IDE_SUCCESS );

    IDU_LIST_ADD_LAST( &mRTxList, &(sRemoteTx->mNode) );
    mRTxCnt++;
    sIsAdded = ID_TRUE;

    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    if ( isGTx() == ID_TRUE )
    {
        generateXID( mGlobalTxId, sRemoteTxId, &(sRemoteTx->mXID) );
        IDE_TEST( mDtxInfo->addDtxBranchTx( &(sRemoteTx->mXID), aLinkObj->mTargetName )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( mLinkerType == DKT_LINKER_TYPE_NONE )
    {
        mLinkerType = DKT_LINKER_TYPE_DBLINK;
    }
    else
    {
        /* Nothing to do */
    }

    *aRemoteTx = sRemoteTx;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_SHARD_TX_ALREADY_EXIST )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKO_DBLINK_IS_BEING_USED ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_REMOTE_TX );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAdded == ID_TRUE )
    {
        destroyRemoteTx( sRemoteTx );
    }
    else
    {
        if ( sIsInited == ID_TRUE )
        {
            sRemoteTx->finalize();
        }
        else
        {
            /* do nothing */
        }

        if ( sIsAlloced == ID_TRUE )
        {
            (void)iduMemMgr::free( sRemoteTx );
            sRemoteTx = NULL;
        }
        else
        {
            /* do nothing */
        }
    }

    IDE_POP();

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::createRemoteTxForShard( idvSQL          *aStatistics,
                                                      dksDataSession  *aSession,
                                                      sdiConnectInfo  *aDataNode,
                                                      dktRemoteTx    **aRemoteTx )
{
    UInt             sRemoteTxId;
    dktRemoteTx     *sRemoteTx = NULL;
    idBool           sIsAlloced = ID_FALSE;
    idBool           sIsInited = ID_FALSE;
    idBool           sIsAdded = ID_FALSE;

    /* �̹� dblink�� ����ϰ� �ִ� ��� ���� */
    IDE_TEST_RAISE( mLinkerType == DKT_LINKER_TYPE_DBLINK,
                    ERR_DBLINK_TX_ALREADY_EXIST );

    IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::createRemoteTx::malloc::RemoteTx",
                          ERR_MEMORY_ALLOC_REMOTE_TX );
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       ID_SIZEOF( dktRemoteTx ),
                                       (void **)&sRemoteTx,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_REMOTE_TX );
    sIsAlloced = ID_TRUE;

    /* Generate remote transaction id */
    sRemoteTxId = generateRemoteTxId( aDataNode->mNodeId );

    IDE_TEST( sRemoteTx->initialize( aSession->mId,
                                     aDataNode->mNodeName,
                                     DKT_LINKER_TYPE_SHARD,
                                     aDataNode,
                                     mGlobalTxId,
                                     mLocalTxId,
                                     sRemoteTxId ) 
              != IDE_SUCCESS );
    sIsInited = ID_TRUE;

    IDU_LIST_INIT_OBJ( &(sRemoteTx->mNode), sRemoteTx );

    IDE_ASSERT( mDktRTxMutex.lock( aStatistics ) == IDE_SUCCESS );

    IDU_LIST_ADD_LAST( &mRTxList, &(sRemoteTx->mNode) );
    mRTxCnt++;
    sIsAdded = ID_TRUE;

    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    if ( isGTx() == ID_TRUE )
    {
        generateXID( mGlobalTxId, sRemoteTxId, &(sRemoteTx->mXID) );
        IDE_TEST( mDtxInfo->addDtxBranchTx( &(sRemoteTx->mXID),
                                            aDataNode->mCoordinatorType,
                                            aDataNode->mNodeName,
                                            aDataNode->mUserName,
                                            aDataNode->mUserPassword,
                                            aDataNode->mServerIP,
                                            aDataNode->mPortNo,
                                            aDataNode->mConnectType )
                  != IDE_SUCCESS );

        /* shard data�� XID�� �����Ѵ�. */
        dktXid::copyXID( &(aDataNode->mXID), &(sRemoteTx->mXID) );
    }
    else
    {
        /* Nothing to do */
    }

    if ( mLinkerType == DKT_LINKER_TYPE_NONE )
    {
        mLinkerType = DKT_LINKER_TYPE_SHARD;
    }
    else
    {
        /* Nothing to do */
    }

    *aRemoteTx = sRemoteTx;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_DBLINK_TX_ALREADY_EXIST )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKO_DBLINK_IS_BEING_USED ) );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_REMOTE_TX );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAdded == ID_TRUE )
    {
        destroyRemoteTx( sRemoteTx );
    }
    else
    {
        if ( sIsInited == ID_TRUE )
        {
            sRemoteTx->finalize();
        }
        else
        {
            /* do nothing */
        }

        if ( sIsAlloced == ID_TRUE )
        {
            (void)iduMemMgr::free( sRemoteTx );
            sRemoteTx = NULL;
        }
        else
        {
            /* do nothing */
        }
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote transaction �� remote transaction list �κ��� 
 *               �����ϰ� ���� �ִ� ��� �ڿ��� �ݳ��Ѵ�. 
 *
 *  aRemoteTx   - [IN] ������ remote transaction �� ����Ű�� ������ 
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktGlobalCoordinator::destroyRemoteTx( dktRemoteTx  *aRemoteTx )
{    
    IDE_ASSERT( mDktRTxMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    IDU_LIST_REMOVE( &(aRemoteTx->mNode) );

    /* BUG-37487 : void */
    aRemoteTx->finalize();

    (void)iduMemMgr::free( aRemoteTx );
        
    if ( mRTxCnt > 0 )
    {
        mRTxCnt--;
    }
    else
    {
        mLinkerType = DKT_LINKER_TYPE_NONE;
    }

    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    aRemoteTx = NULL;
}

void dktGlobalCoordinator::destroyAllRemoteTx()
{        
    dktRemoteTx     *sRemoteTx = NULL;
    iduListNode     *sIterator = NULL;
    iduListNode     *sNext     = NULL;

    IDE_ASSERT( mDktRTxMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    if ( IDU_LIST_IS_EMPTY( &mRTxList ) != ID_TRUE )
    {
        IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;

            if ( sRemoteTx != NULL )
            {
                IDU_LIST_REMOVE( &(sRemoteTx->mNode) );
                mRTxCnt--;

                sRemoteTx->finalize();

                (void)iduMemMgr::free( sRemoteTx );

                sRemoteTx = NULL;
            }
            else
            {
                /* nothing to do */
            }
        }
    }
    else
    {
        /* nothing to do */
    }

    IDE_DASSERT( mRTxCnt == 0 );

    mLinkerType = DKT_LINKER_TYPE_NONE;

    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );
}


/************************************************************************
 * Description : Remote transaction �� id �� �����Ѵ�.
 *
 *  aLinkObjId  - [IN] Database link �� id
 *
 ************************************************************************/
UInt    dktGlobalCoordinator::generateRemoteTxId( UInt aLinkObjId )
{
    UInt    sRemoteTxId = 0;
    UInt    sRTxCnt     = mRTxCnt;

    sRemoteTxId = ( ( sRTxCnt + 1 ) << ( 8 * ID_SIZEOF( UShort ) ) ) + aLinkObjId;

    return sRemoteTxId;
}

/************************************************************************
 * Description : Id �� �Է¹޾� remote transaction �� ã�´�. 
 *
 *  aId         - [IN] Remote transaction id
 *  aRemoteTx   - [OUT] ã�� remote transaction
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::findRemoteTx( UInt            aId, 
                                            dktRemoteTx   **aRemoteTx )
{
    idBool           sIsExist  = ID_FALSE;
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRemoteTx->getRemoteTransactionId() == aId )
        {
            sIsExist = ID_TRUE;
            break;
        }
        else
        {
            /* no more remote transaction */
        }
    }

    IDE_TEST_RAISE( sIsExist != ID_TRUE, ERR_INVALID_REMOTE_TX );

    *aRemoteTx = sRemoteTx;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_TX );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_TX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote target name �� �Է¹޾� �ش��ϴ� remote transaction 
 *               �� ã�´�.
 *
 *  aTargetName - [IN] Remote target server name
 *  aRemoteTx   - [OUT] ã�� remote transaction
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::findRemoteTxWithTarget( SChar         *aTargetName,
                                                      dktRemoteTx  **aRemoteTx )
{
    idBool           sIsExist  = ID_FALSE;
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    IDE_TEST_RAISE( ( isGTx() == ID_TRUE ) &&
                    ( getGTxStatus() >= DKT_GTX_STATUS_PREPARE_REQUEST ),
                    ERR_NOT_EXECUTE_DML );

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        if ( ( sRemoteTx->getLinkerType() == DKT_LINKER_TYPE_DBLINK ) &&
             ( idlOS::strncmp( sRemoteTx->getTargetName(),
                               aTargetName,
                               DK_NAME_LEN + 1 ) == 0 ) )
        {
            sIsExist = ID_TRUE;
            break;
        }
        else
        {
            /* no more remote transaction */
        }
    }

    if ( sIsExist == ID_TRUE )
    {
        *aRemoteTx = sRemoteTx;
    }
    else
    {
        *aRemoteTx = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_NOT_EXECUTE_DML );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKM_NOT_EXECUTE_DML ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::findRemoteTxWithShardNode( UInt          aNodeId,
                                                         dktRemoteTx **aRemoteTx )
{
    idBool           sIsExist  = ID_FALSE;
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    if ( mLinkerType == DKT_LINKER_TYPE_SHARD )
    {
        IDU_LIST_ITERATE( &mRTxList, sIterator )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;

            if ( sRemoteTx->getLinkerType() == DKT_LINKER_TYPE_SHARD )
            {
                if ( sRemoteTx->getDataNode()->mNodeId == aNodeId )
                {
                    sIsExist = ID_TRUE;
                    break;
                }
                else
                {
                    /* no more remote transaction */
                }
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

    if ( sIsExist == ID_TRUE )
    {
        *aRemoteTx = sRemoteTx;
    }
    else
    {
        *aRemoteTx = NULL;
    }

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : �Է¹��� savepoint �� savepoint list �κ��� 
 *               �����ϰ� �ڿ��� �ݳ��Ѵ�. 
 *
 *  aSavepoint   - [IN] ������ savepoint �� ����Ű�� ������ 
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktGlobalCoordinator::destroySavepoint( dktSavepoint  *aSavepoint )
{
    IDE_ASSERT( aSavepoint != NULL );

    IDU_LIST_REMOVE( &(aSavepoint->mNode) );
    (void)iduMemMgr::free( aSavepoint );
}

/************************************************************************
 * Description : �۷ι� Ʈ����� commit �� ���� prepare phase �� �����Ѵ�.
 *               Remote statement execution level ������ prepare �ܰ谡 
 *               �������� �����Ƿ� �� �Լ��� ����� ���� ����. 
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executePrepare()
{
    switch ( mAtomicTxLevel )
    {
        case DKT_ADLP_REMOTE_STMT_EXECUTION:
            break;
        case DKT_ADLP_SIMPLE_TRANSACTION_COMMIT:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeSimpleTransactionCommitPrepare() != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeSimpleTransactionCommitPrepareForShard() != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        case DKT_ADLP_TWO_PHASE_COMMIT:
            /* fall through */
        case DKT_ADLP_GCTX:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeTwoPhaseCommitPrepare() != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeTwoPhaseCommitPrepareForShard() != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        default:
            IDE_RAISE( ERR_ATOMIC_TX_LEVEL );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ATOMIC_TX_LEVEL );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_INVALID_TX_LEVEL ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� simple transaction commit level ������ 
 *               �۷ι� Ʈ����� commit �� ���� prepare phase �� �����Ѵ�.
 *               ���������δ� ��� remote node session ���� ��Ʈ��ũ ����
 *               �� ����ִ��� Ȯ���Ͽ� ��� ������ ����ִ� ��츸 
 *               prepared �� ���¸� �����ϰ� SUCCESS �� return �Ѵ�. 
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitPrepare()
{
    UInt                 i;
    UInt                 sResultCode    = DKP_RC_SUCCESS;
    UInt                 sRemoteNodeCnt = 0;
    ULong                sTimeoutSec; 
    dksSession          *sDksSession    = NULL;
    dksRemoteNodeInfo   *sRemoteInfo    = NULL;
    SInt                 sReceiveTimeout = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    /* Get session */
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    mGTxStatus = DKT_GTX_STATUS_PREPARE_REQUEST;

    /* Check remote sessions */
    IDE_TEST( dkpProtocolMgr::sendCheckRemoteSession( sDksSession, 
                                                      mSessionId,
                                                      0 /* ALL */ )
              != IDE_SUCCESS );

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_WAIT );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_WAIT;

    IDE_TEST( dkpProtocolMgr::recvCheckRemoteSessionResult( sDksSession,
                                                            mSessionId,
                                                            &sResultCode,
                                                            &sRemoteNodeCnt,
                                                            &sRemoteInfo,
                                                            sTimeoutSec )
              != IDE_SUCCESS );                             

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    if ( sRemoteNodeCnt != 0 )
    {
        for ( i = 0; i < sRemoteNodeCnt; ++i )
        {
            /* check connection */
            IDE_TEST( sRemoteInfo[i].mStatus 
                      != DKS_REMOTE_NODE_SESSION_STATUS_CONNECTED );
        }
    } 
    else
    {
        /* error, invalid situation */
    }

    /* BUG-37665 */
    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARED );
    mGTxStatus = DKT_GTX_STATUS_PREPARED;

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

    /* BUG-37665 */
    IDE_PUSH();
    
    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� two phase commit level ������ �۷ι� Ʈ�����
 *               commit �� ���� prepare phase �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitPrepare()
{
    dksSession         * sDksSession = NULL;
    UInt                 sResultCode = 0;
    SInt                 sReceiveTimeout = 0;
    UInt                 sCountRDOnlyXID = 0;
    UInt                 sCountRDOnlyXIDTemp = 0;
    ID_XID             * sRDOnlyXIDs = NULL;
    UInt                 sCountFailXID = 0;
    ID_XID             * sFailXIDs = NULL;
    SInt               * sFailErrCodes = NULL;
    UChar                sXidString[DKT_2PC_XID_STRING_LEN];

    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession )
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    IDE_TEST( writeXaPrepareReqLog() != IDE_SUCCESS );

    mGTxStatus = DKT_GTX_STATUS_PREPARE_REQUEST;

    /* FIT POINT: PROJ-2569 Before send prepare and not receive ack */
    IDU_FIT_POINT( "dktGlobalCoordinator::executeTwoPhaseCommitPrepare::dkpProtocolMgr::sendXAPrepare::BEFORE_SEND_PREPARE" );

    /* Request Prepare protocol for 2PC */
    IDE_TEST( dkpProtocolMgr::sendXAPrepare( sDksSession, mSessionId, mDtxInfo )
              != IDE_SUCCESS );

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_WAIT );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_WAIT;

    /* FIT POINT: PROJ-2569 After send prepare and not receive ack */
    IDU_FIT_POINT( "dktGlobalCoordinator::executeTwoPhaseCommitPrepare::dkpProtocolMgr::sendXAPrepare::AFTER_SEND_PREPARE" );
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );
    
    IDE_TEST( dkpProtocolMgr::recvXAPrepareResult( sDksSession,
                                                   mSessionId,
                                                   &sResultCode,
                                                   (ULong)sReceiveTimeout,
                                                   &sCountRDOnlyXID,
                                                   &sRDOnlyXIDs,
                                                   &sCountFailXID,
                                                   &sFailXIDs,
                                                   &sFailErrCodes )
              != IDE_SUCCESS );
              
    /* FIT POINT: PROJ-2569 After receive prepare ack from remote */          
    IDU_FIT_POINT( "dktGlobalCoordinator::executeTwoPhaseCommitPrepare::dkpProtocolMgr::recvXAPrepareResult::AFTER_RECV_PREPARE_ACK");
    
    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARED );
    mGTxStatus = DKT_GTX_STATUS_PREPARED;

    if ( sCountRDOnlyXID > 0 )
    {
        sCountRDOnlyXIDTemp = sCountRDOnlyXID;

        while ( sCountRDOnlyXIDTemp > 0 )
        {
            IDE_TEST( mDtxInfo->removeDtxBranchTx( &sRDOnlyXIDs[sCountRDOnlyXIDTemp-1] ) != IDE_SUCCESS );
            sCountRDOnlyXIDTemp--;
        }
    }
    else
    {
        /* Nothing to do */
    }

    dkpProtocolMgr::freeXARecvResult( sRDOnlyXIDs,
                                      sFailXIDs,
                                      NULL,
                                      sFailErrCodes );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        if ( sCountFailXID > 0 )
        {
            (void)idaXaConvertXIDToString(NULL,
                                          &(sFailXIDs[0]),
                                          sXidString,
                                          DKT_2PC_XID_STRING_LEN);

            ideLog::log( DK_TRC_LOG_FORCE, DK_TRC_T_COORDINATOR_XA_RESULT_CODE, "Prepare",
                         mDtxInfo->mResult, mDtxInfo->mGlobalTxId,
                         sXidString, sFailErrCodes[0], sCountFailXID );
        }
        else
        {
            /* Nothing to do */
        }

        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }
    IDE_EXCEPTION_END;

    dkpProtocolMgr::freeXARecvResult( sRDOnlyXIDs,
                                      sFailXIDs,
                                      NULL,
                                      sFailErrCodes );

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� ���� �� �۷ι� Ʈ����ǿ� ���ϴ� ��� remote 
 *               transaction ���� ��� prepare �� ��ٸ��� ���������� 
 *               �˻��Ѵ�. 
 *
 ************************************************************************/
idBool  dktGlobalCoordinator::isAllRemoteTxPrepareReady()
{
    idBool           sIsAllReady = ID_TRUE;
    iduListNode     *sIterator     = NULL;
    dktRemoteTx     *sRemoteTx     = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRemoteTx->getStatus() != DKT_RTX_STATUS_PREPARE_READY )
        {
            sIsAllReady = ID_FALSE;
            break;
        }
        else
        {
            /* check next */
        }
    }

    return sIsAllReady;
}

/************************************************************************
 * Description : �۷ι� Ʈ����� commit �� ���� ADLP ���������� 
 *               commit phase �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeCommit()
{
    IDE_TEST_RAISE( mGTxStatus != DKT_GTX_STATUS_PREPARED,
                    ERR_GLOBAL_TX_NOT_PREPARED );

    switch ( mAtomicTxLevel )
    {
        case DKT_ADLP_REMOTE_STMT_EXECUTION:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeRemoteStatementExecutionCommit() != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeSimpleTransactionCommitCommitForShard() != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        case DKT_ADLP_SIMPLE_TRANSACTION_COMMIT:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeSimpleTransactionCommitCommit() != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeSimpleTransactionCommitCommitForShard() != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        case DKT_ADLP_TWO_PHASE_COMMIT:
            /* fall through */
        case DKT_ADLP_GCTX:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeTwoPhaseCommitCommit() != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeTwoPhaseCommitCommitForShard() != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        default:
            IDE_RAISE( ERR_INVALID_TX_LEVEL );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GLOBAL_TX_NOT_PREPARED );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_GLOBAL_TX_NOT_PREPARED ) );
    }
    IDE_EXCEPTION( ERR_INVALID_TX_LEVEL );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_INVALID_TX_LEVEL ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Ʈ������� atomic transaction level �� ADLP ���� �����ϴ�
 *               remote statement execution level �� ������ ��� Ʈ����� 
 *               commit �� �����Ѵ�. 
 *               Remote statement execution level �� DB-Link �� ����� 
 *               remote server �� �ڵ����� auto-commit mode �� ON ���� 
 *               �����ϹǷ� ��ǻ� ������ ������ ��� remote transaction
 *               �� ��ǻ� remote server �� commit �Ǿ� �ִٰ� �� �� �ִ�. 
 *               �׷��� remote server �� auto-commit mode �� �������� �ʴ�
 *               ��� ����ڰ� ��������� commit �� ������ �� �־�� �ϸ� 
 *               �׷��� ���� ��쿡�� ����ڴ� commit �� ������ �� �ִ�. 
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeRemoteStatementExecutionCommit()
{
    UInt                 sResultCode = DKP_RC_SUCCESS;
    ULong                sTimeoutSec; 
    dksSession          *sDksSession = NULL;
    SInt                 sReceiveTimeout = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    mGTxStatus = DKT_GTX_STATUS_COMMIT_REQUEST;
    IDE_TEST_RAISE( dkpProtocolMgr::sendCommit( sDksSession, mSessionId ) 
                    != IDE_SUCCESS, ERR_COMMIT_PROTOCOL_OP );

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMIT_WAIT );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_WAIT;
    IDE_TEST_RAISE( dkpProtocolMgr::recvCommitResult( sDksSession,
                                                      mSessionId,
                                                      &sResultCode,
                                                      sTimeoutSec )
                    != IDE_SUCCESS, ERR_COMMIT_PROTOCOL_OP );             

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMITTED );
    mGTxStatus = DKT_GTX_STATUS_COMMITTED;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COMMIT_PROTOCOL_OP );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_ADLP_PROTOCOL ) );
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
 * Description : ADLP �� simple transaction commit level ������ 
 *               �۷ι� Ʈ����� commit �� ���� commit phase �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitCommit()
{
    UInt                 sResultCode = DKP_RC_SUCCESS;
    ULong                sTimeoutSec; 
    dksSession          *sDksSession = NULL;
    SInt                 sReceiveTimeout = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    mGTxStatus = DKT_GTX_STATUS_COMMIT_REQUEST;
    IDE_TEST_RAISE( dkpProtocolMgr::sendCommit( sDksSession, mSessionId ) 
                    != IDE_SUCCESS, ERR_COMMIT_PROTOCOL_OP );

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMIT_WAIT );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_WAIT;
    IDE_TEST_RAISE( dkpProtocolMgr::recvCommitResult( sDksSession,
                                                      mSessionId,
                                                      &sResultCode,
                                                      sTimeoutSec )
                    != IDE_SUCCESS, ERR_COMMIT_PROTOCOL_OP );             

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMITTED );
    mGTxStatus = DKT_GTX_STATUS_COMMITTED;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_COMMIT_PROTOCOL_OP );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_ADLP_PROTOCOL ) );
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
 * Description : ADLP �� two phase commit level ������ �۷ι� Ʈ�����
 *               commit �� ���� commit phase �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitCommit()
{
    dksSession   * sSession = NULL;
    UInt           sResultCode = 0;
    SInt           sReceiveTimeout = 0;
    UInt           sCountFailXID = 0;
    ID_XID       * sFailXIDs = NULL;
    SInt         * sFailErrCodes = NULL;
    UInt           sCountHeuristicXID = 0;
    ID_XID       * sHeuristicXIDs     = NULL;
    UChar          sXidString[DKT_2PC_XID_STRING_LEN];
    ideLogEntry    sLog( IDE_DK_0 );

    mDtxInfo->mResult = SMI_DTX_COMMIT;
    
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sSession ) 
              != IDE_SUCCESS );

    IDE_TEST_RAISE( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
                    != IDE_SUCCESS, ERR_GET_CONF );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_REQUEST;

    /* FIT POINT: PROJ-2569 Before send commit request */ 
    IDU_FIT_POINT_RAISE("dktGlobalCoordinator::executeTwoPhaseCommitCommit::dkpProtocolMgr::sendXACommit::BEFORE_SEND_COMMIT", ERR_XA_COMMIT);
    
    IDE_TEST_RAISE( dkpProtocolMgr::sendXACommit( sSession,
                                                  mSessionId,
                                                  mDtxInfo )
                    != IDE_SUCCESS, ERR_XA_COMMIT );
    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMIT_WAIT );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_WAIT;
    
    /* FIT POINT: PROJ-2569 After send commit request */ 
    IDU_FIT_POINT_RAISE("dktGlobalCoordinator::executeTwoPhaseCommitCommit::dkpProtocolMgr::sendXACommit::AFTER_SEND_COMMIT", ERR_XA_COMMIT);
    
    IDE_TEST_RAISE( dkpProtocolMgr::recvXACommitResult( sSession,
                                                        mSessionId,
                                                        &sResultCode,
                                                        sReceiveTimeout,
                                                        &sCountFailXID,
                                                        &sFailXIDs,
                                                        &sFailErrCodes,
                                                        &sCountHeuristicXID,
                                                        &sHeuristicXIDs )
                    != IDE_SUCCESS, ERR_XA_COMMIT );

    /* FIT POINT: PROJ-2569 After receive commit result */ 
    IDU_FIT_POINT_RAISE("dktGlobalCoordinator::executeTwoPhaseCommitCommit::dkpProtocolMgr::recvXACommitResult::AFTER_RECV_COMMIT_ACK", ERR_XA_COMMIT);
    
    if ( sResultCode == DKP_RC_SUCCESS )
    {
        if ( sCountHeuristicXID > 0 )
        {
            (void)sLog.appendFormat( "Heuristic Completed. Global Tx ID : %"ID_XINT32_FMT,
                                     mDtxInfo->mGlobalTxId );
            (void)sLog.appendFormat( ", Count : %"ID_UINT32_FMT"\n", sCountHeuristicXID );
            while ( sCountHeuristicXID > 0 )
            {
                (void)idaXaConvertXIDToString(NULL,
                                              &(sHeuristicXIDs[sCountHeuristicXID - 1]),
                                              sXidString,
                                              DKT_2PC_XID_STRING_LEN);

                (void)sLog.appendFormat( "XID : %s\n", sXidString );
                sCountHeuristicXID--;
            }
            sLog.write();
        }
        else
        {
            /* Nothing to do */
        }
        removeDtxInfo();

        IDE_TEST( writeXaEndLog() != IDE_SUCCESS );
    }
    else
    {
        if ( ( sFailErrCodes != NULL ) && ( sFailXIDs != NULL ) )
        {
            (void)idaXaConvertXIDToString(NULL,
                                          &(sFailXIDs[0]),
                                          sXidString,
                                          DKT_2PC_XID_STRING_LEN);

            ideLog::log( DK_TRC_LOG_FORCE, DK_TRC_T_COORDINATOR_XA_RESULT_CODE, "Commit",
                         mDtxInfo->mResult, mDtxInfo->mGlobalTxId,
                         sXidString, sFailErrCodes[0], sCountFailXID );
        }
        else
        {
            ideLog::log( DK_TRC_LOG_FORCE, " global transaction: %"ID_UINT32_FMT
                        " was failed with result code %"ID_UINT32_FMT,
                        mDtxInfo->mGlobalTxId, mDtxInfo->mResult );
        }
        
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMITTED );
    mGTxStatus = DKT_GTX_STATUS_COMMITTED;
    dkpProtocolMgr::freeXARecvResult( NULL,
                                      sFailXIDs,
                                      sHeuristicXIDs,
                                      sFailErrCodes );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GET_CONF );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_OPEN_DBLINK_CONF_FAILED ) );
    }
    IDE_EXCEPTION( ERR_XA_COMMIT );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_XA_APPLY_FAIL,
                                  mDtxInfo->mResult, mDtxInfo->mGlobalTxId ) );
    }
    IDE_EXCEPTION_END;

    dkpProtocolMgr::freeXARecvResult( NULL,
                                      sFailXIDs,
                                      sHeuristicXIDs,
                                      sFailErrCodes );

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �۷ι� Ʈ����� rollback �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeRollback( SChar  *aSavepointName )
{
    switch ( mAtomicTxLevel )
    {
        case DKT_ADLP_REMOTE_STMT_EXECUTION:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeRemoteStatementExecutionRollback( aSavepointName ) 
                              != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeSimpleTransactionCommitRollbackForShard( aSavepointName ) 
                              != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        case DKT_ADLP_SIMPLE_TRANSACTION_COMMIT:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeSimpleTransactionCommitRollback( aSavepointName ) 
                              != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeSimpleTransactionCommitRollbackForShard( aSavepointName ) 
                              != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        case DKT_ADLP_TWO_PHASE_COMMIT:
            /* fall through */
        case DKT_ADLP_GCTX:
        {
            switch ( mLinkerType )
            {
                case DKT_LINKER_TYPE_DBLINK:
                    IDE_TEST( executeTwoPhaseCommitRollback()
                              != IDE_SUCCESS );
                    break;

                case DKT_LINKER_TYPE_SHARD:
                    IDE_TEST( executeTwoPhaseCommitRollbackForShard( aSavepointName )
                              != IDE_SUCCESS );
                    break;

                default:
                    break;
            }
            break;
        }

        default:
            IDE_RAISE( ERR_ATOMIC_TX_LEVEL );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ATOMIC_TX_LEVEL );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_INVALID_TX_LEVEL ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �۷ι� Ʈ����� rollback FORCE DATABASE_LINK �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeRollbackForce()
{
    switch ( mLinkerType )
    {
        case DKT_LINKER_TYPE_DBLINK:
            IDE_TEST( executeRollbackForceForDBLink() != IDE_SUCCESS );
            break;

        case DKT_LINKER_TYPE_SHARD:
            IDE_TEST( executeSimpleTransactionCommitRollbackForceForShard()
                      != IDE_SUCCESS );
            break;

        default:
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeRollbackForceForDBLink()
{
    UInt                 sResultCode      = DKP_RC_SUCCESS;
    UShort              *sRemoteNodeIdArr = NULL;
    ULong                sTimeoutSec; 
    dksSession          *sDksSession      = NULL;
    SInt                 sReceiveTimeout  = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    /* Get session */
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }    

    /* request rollback */
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_REQUEST;
    IDE_TEST_RAISE( dkpProtocolMgr::sendRollback( sDksSession, 
                                                  mSessionId,
                                                  NULL, /* no savepoint */
                                                  0,    /* not used */
                                                  sRemoteNodeIdArr ) 
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    IDU_FIT_POINT_RAISE("dktGlobalCoordinator::executeRollbackForce::dkpProtocolMgr::recvRollbackResult::ERR_RESULT",
                        ERR_ROLLBACK_PROTOCOL_OP );
    IDE_TEST_RAISE( dkpProtocolMgr::recvRollbackResult( sDksSession,
                                                        mSessionId,
                                                        &sResultCode,
                                                        sTimeoutSec )
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ROLLBACK_PROTOCOL_OP );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_ADLP_PROTOCOL ) );
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

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_READY;
        
    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� remote statement execution level ������ 
 *               �۷ι� Ʈ����� rollback �� �����Ѵ�. 
 *   ____________________________________________________________________
 *  |                                                                    | 
 *  |  ������ �� �Լ��� �׻� �۷ι� Ʈ����� �ѹ�ó���� �ϸ� �ȴ�.       |
 *  |  savepoint �� �Ѿ�͵� �����ϴ�.                                   |
 *  |  �׷��� ���� �����ǵ��ʹ� �޸� REMOTE_STATEMENT_EXECUTION level    |
 *  |  ������ ���ݼ����� autocommit off �� �� �� �ֵ��� �ϴ� �䱸������  |
 *  |  �ݿ��Ǹ鼭 executeSimpleTransactionCommitRollback �� �����ϰ�     |
 *  |  ó���ǵ��� �ۼ��Ǿ���.                                            | 
 *  |  ����, executeSimpleTransactionCommitRollback �� ���������      |
 *  |  ����� �� �Լ����� �ݿ��Ǿ�� �Ѵ�.                               |
 *  |____________________________________________________________________|
 *
 *  aSavepointName  - [IN] Rollback to savepoint �� ������ savepoint name
 *                         �� ���� NULL �̸� ��ü Ʈ������� rollback.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeRemoteStatementExecutionRollback( SChar *aSavepointName )
{
    UInt                 i;
    UInt                 sResultCode      = DKP_RC_SUCCESS;
    UInt                 sRollbackNodeCnt = 0;
    UInt                 sRemoteNodeCnt   = 0;
    UShort               sRemoteNodeId;
    UShort              *sRemoteNodeIdArr = NULL;
    ULong                sTimeoutSec;
    dksSession          *sDksSession      = NULL;
    dksRemoteNodeInfo   *sRemoteInfo      = NULL;
    SInt                 sReceiveTimeout  = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    /* Get session */
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }    
    
    /* Check remote sessions */
    IDE_TEST( dkpProtocolMgr::sendCheckRemoteSession( sDksSession, 
                                                      mSessionId,
                                                      0 /* ALL */ )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvCheckRemoteSessionResult( sDksSession,
                                                            mSessionId,
                                                            &sResultCode,
                                                            &sRemoteNodeCnt,
                                                            &sRemoteInfo,
                                                            sTimeoutSec )
              != IDE_SUCCESS );                             

    IDE_TEST( sResultCode != DKP_RC_SUCCESS );

    for ( i = 0; i < sRemoteNodeCnt; ++i )
    {
        /* check connection */
        IDE_TEST( sRemoteInfo[i].mStatus 
                  != DKS_REMOTE_NODE_SESSION_STATUS_CONNECTED );
    }

    /* BUG-37665 */
    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    /* Check savepoint */
    if ( aSavepointName != NULL )
    {
        /* >> BUG-37512 */
        if ( findSavepoint( aSavepointName ) == NULL )
        {
            /* global transaction �� ���� ������ ���� savepoint.
               set rollback all */
            sRollbackNodeCnt = 0;
        }
        else
        {
            /* set rollback to savepoint */
            IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::executeRemoteStatementExecutionRollback::calloc::RemoteNodeIdArr", 
                                  ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );
            IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_DK,
                                               mRTxCnt,
                                               ID_SIZEOF( sRemoteNodeId ),
                                               (void **)&sRemoteNodeIdArr,
                                               IDU_MEM_IMMEDIATE )
                            != IDE_SUCCESS, ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );

            sRollbackNodeCnt = getRemoteNodeIdArrWithSavepoint( aSavepointName,
                                                                sRemoteNodeIdArr );
        }
        /* << BUG-37512 */
    }
    else
    {
        /* set rollback all */
        sRollbackNodeCnt = 0;
    }

    /* request rollback */
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_REQUEST;    
    IDE_TEST_RAISE( dkpProtocolMgr::sendRollback( sDksSession, 
                                                  mSessionId,
                                                  aSavepointName,
                                                  sRollbackNodeCnt,
                                                  sRemoteNodeIdArr ) 
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    if ( sRemoteNodeIdArr != NULL )
    {
        (void)iduMemMgr::free( sRemoteNodeIdArr );
        sRemoteNodeIdArr = NULL;
    }
    else
    {
        /* do nothing */
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    IDE_TEST_RAISE( dkpProtocolMgr::recvRollbackResult( sDksSession,
                                                        mSessionId,
                                                        &sResultCode,
                                                        sTimeoutSec )
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    if ( aSavepointName != NULL )
    {
        /* BUG-37487 : void */
        destroyRemoteTransactionWithoutSavepoint( aSavepointName );

        /* >> BUG-37512 */
        if ( sRollbackNodeCnt == 0 )
        {
            removeAllSavepoint();
        }
        else
        {
            removeAllNextSavepoint( aSavepointName );
        }
        /* << BUG-37512 */
    }
    else
    {
        destroyAllRemoteTx();
        removeAllSavepoint();
    }

    if ( mRTxCnt == 0 )
    {
    	setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
        mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;
    }
    else
    {
    	setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
        mGTxStatus = DKT_GTX_STATUS_PREPARE_READY; 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_ROLLBACK_PROTOCOL_OP );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_ADLP_PROTOCOL ) );
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

    /* BUG-37665 */
    IDE_PUSH();

    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    /* >> BUG-37512 */
    if ( sRemoteNodeIdArr != NULL )
    {
        (void)iduMemMgr::free( sRemoteNodeIdArr );
        sRemoteNodeIdArr = NULL;
    }
    else
    {
        /* do nothing */
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_READY;
    /* << BUG-37512 */

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� simple transaction commit level ������ 
 *               �۷ι� Ʈ����� rollback �� �����Ѵ�.
 *   ___________________________________________________________________
 *  |                                                                   | 
 *  |  �� �Լ��� ��������� executeRemoteStatementExecutionRollback     |
 *  |  �Լ����� �ݿ��Ǿ�� �Ѵ�.                                        |
 *  |___________________________________________________________________|
 *
 *  aSavepointName  - [IN] Rollback to savepoint �� ������ savepoint name
 *                         �� ���� NULL �̸� ��ü Ʈ������� rollback.
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitRollback( SChar *aSavepointName )
{
    UInt                 i;
    UInt                 sResultCode      = DKP_RC_SUCCESS;
    UInt                 sRollbackNodeCnt = 0;
    UInt                 sRemoteNodeCnt   = 0;
    UShort               sRemoteNodeId;
    UShort              *sRemoteNodeIdArr = NULL;
    ULong                sTimeoutSec; 
    dksSession          *sDksSession      = NULL;
    dksRemoteNodeInfo   *sRemoteInfo      = NULL;
    SInt                 sReceiveTimeout  = 0;
    SInt                 sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    /* Get session */
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) 
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sDksSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    /* Check remote sessions */
    IDE_TEST( dkpProtocolMgr::sendCheckRemoteSession( sDksSession, 
                                                      mSessionId,
                                                      0 /* ALL */ )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvCheckRemoteSessionResult( sDksSession,
                                                            mSessionId,
                                                            &sResultCode,
                                                            &sRemoteNodeCnt,
                                                            &sRemoteInfo,
                                                            sTimeoutSec )
              != IDE_SUCCESS );                             

    IDE_TEST( sResultCode != DKP_RC_SUCCESS );

    for ( i = 0; i < sRemoteNodeCnt; ++i )
    {
        /* check connection */
        IDE_TEST_RAISE( sRemoteInfo[i].mStatus 
                        != DKS_REMOTE_NODE_SESSION_STATUS_CONNECTED,
                        ERR_ROLLBACK_FAILED );
    }

    /* BUG-37665 */
    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    /* Check savepoint */
    if ( aSavepointName != NULL )
    {
        /* >> BUG-37512 */
        if ( findSavepoint( aSavepointName ) == NULL )
        {
            /* global transaction �� ���� ������ ���� savepoint.
               set rollback all */
            sRollbackNodeCnt = 0;
        }
        else
        {
            /* set rollback to savepoint */
            IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::executeSimpleTransactionCommitRollback::calloc::RemoteNodeIdArr", 
                                  ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );
            IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_DK,
                                               mRTxCnt,
                                               ID_SIZEOF( sRemoteNodeId ),
                                               (void **)&sRemoteNodeIdArr,
                                               IDU_MEM_IMMEDIATE )
                            != IDE_SUCCESS, ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );
            
            sRollbackNodeCnt = getRemoteNodeIdArrWithSavepoint( aSavepointName,
                                                                sRemoteNodeIdArr );
        }
        /* << BUG-37512 */
    }
    else
    {
        /* set rollback all */
        sRollbackNodeCnt = 0;
    }

    /* request rollback */
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_REQUEST;
    IDE_TEST_RAISE( dkpProtocolMgr::sendRollback( sDksSession, 
                                                  mSessionId,
                                                  aSavepointName,
                                                  sRollbackNodeCnt,
                                                  sRemoteNodeIdArr ) 
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    if ( sRemoteNodeIdArr != NULL )
    {
        (void)iduMemMgr::free( sRemoteNodeIdArr );
        sRemoteNodeIdArr = NULL;
    }
    else
    {
        /* do nothing */
    }

    /* for performance view during delayed responsbility */
    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::executeSimpleTransactionCommitRollback::recvRollbackResult", 
                         ERR_ROLLBACK_PROTOCOL_OP );
    IDE_TEST_RAISE( dkpProtocolMgr::recvRollbackResult( sDksSession,
                                                        mSessionId,
                                                        &sResultCode,
                                                        sTimeoutSec )
                    != IDE_SUCCESS, ERR_ROLLBACK_PROTOCOL_OP );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    if ( aSavepointName != NULL )
    {
        /* BUG-37487 : void */
        destroyRemoteTransactionWithoutSavepoint( aSavepointName );

        /* >> BUG-37512 */
        if ( sRollbackNodeCnt == 0 )
        {
            removeAllSavepoint();
        }
        else
        {
            removeAllNextSavepoint( aSavepointName );
        }
        /* << BUG-37512 */
    }
    else
    {
        destroyAllRemoteTx();
        removeAllSavepoint();
    }

    if ( mRTxCnt == 0 )
    {
    	setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
        mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;
    }
    else
    {
    	setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
        mGTxStatus = DKT_GTX_STATUS_PREPARE_READY; 
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEM_ALLOC_REMOTE_NODE_ID_ARR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_ROLLBACK_PROTOCOL_OP );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_ADLP_PROTOCOL ) );
    }
    IDE_EXCEPTION( ERR_ROLLBACK_FAILED );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_ROLLBACK_FAILED ) );
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

    /* BUG-37665 */
    IDE_PUSH();

    if ( sRemoteInfo != NULL )
    {
        (void)iduMemMgr::free( sRemoteInfo );
        sRemoteInfo = NULL;
    }
    else
    {
        /* do nothing */
    }

    /* >> BUG-37512 */
    if ( sRemoteNodeIdArr != NULL )
    {
        (void)iduMemMgr::free( sRemoteNodeIdArr );
        sRemoteNodeIdArr = NULL;
    }
    else
    {
        /* do nothing */
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_READY;
    /* << BUG-37512 */

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ADLP �� two phase commit level ������ �۷ι� Ʈ����� 
 *               rollback �� �����Ѵ�. 
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitRollback()
{
    dksSession    * sSession = NULL;
    UInt            sResultCode = 0;
    SInt            sReceiveTimeout = 0;
    UInt            sCountFailXID = 0;
    ID_XID        * sFailXIDs = NULL;
    SInt          * sFailErrCodes = NULL;
    UInt            sCountHeuristicXID   = 0;
    ID_XID        * sHeuristicXIDs       = NULL;
    UChar           sXidString[DKT_2PC_XID_STRING_LEN];
    ideLogEntry     sLog( IDE_DK_0 );
    idBool          sIsPrepared = ID_FALSE;

    mDtxInfo->mResult = SMI_DTX_ROLLBACK;
    
    IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sSession )
              != IDE_SUCCESS );

    if ( getAllRemoteStmtCount() > 0 )
    {
        IDE_TEST( freeAndDestroyAllRemoteStmt( sSession, mSessionId )
                  != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }
    
    if ( ( mGTxStatus >= DKT_GTX_STATUS_PREPARE_REQUEST ) && 
         ( mGTxStatus <= DKT_GTX_STATUS_PREPARED ) ) 
    {
        sIsPrepared = ID_TRUE;
    }
    else
    {
        /* Nothing to do */
    }
    
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_REQUEST;

    IDE_TEST_RAISE( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
                    != IDE_SUCCESS, ERR_GET_CONF );
        
    IDE_TEST_RAISE( dkpProtocolMgr::sendXARollback( sSession,
                                                    mSessionId,
                                                    mDtxInfo ) 
                    != IDE_SUCCESS, ERR_XA_ROLLBACK );
    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    IDE_TEST_RAISE( dkpProtocolMgr::recvXARollbackResult( sSession,
                                                          mSessionId,
                                                          &sResultCode,
                                                          sReceiveTimeout,
                                                          &sCountFailXID,
                                                          &sFailXIDs,
                                                          &sFailErrCodes,
                                                          &sCountHeuristicXID,
                                                          &sHeuristicXIDs )
                    != IDE_SUCCESS, ERR_XA_ROLLBACK );

    if ( sResultCode == DKP_RC_SUCCESS )
    {
        if ( sCountHeuristicXID > 0 )
        {
            sLog.appendFormat( "Heuristic Completed. Global Tx ID : %"ID_XINT32_FMT,
                               mDtxInfo->mGlobalTxId );
            sLog.appendFormat( ", Count : %"ID_UINT32_FMT"\n", sCountHeuristicXID );
            while ( sCountHeuristicXID > 0 )
            {
                (void)idaXaConvertXIDToString(NULL,
                                              &(sHeuristicXIDs[sCountHeuristicXID - 1]),
                                              sXidString,
                                              DKT_2PC_XID_STRING_LEN);

                (void)sLog.appendFormat( "XID : %s\n", sXidString );
                sCountHeuristicXID--;
            }
            sLog.write();
        }
        else
        {
            /* Nothing to do */
        }

        removeDtxInfo();
    }
    else
    {
        if ( ( sFailErrCodes != NULL ) && ( sFailXIDs != NULL ) )
        {
            (void)idaXaConvertXIDToString(NULL,
                                          &(sFailXIDs[0]),
                                          sXidString,
                                          DKT_2PC_XID_STRING_LEN);

            ideLog::log( DK_TRC_LOG_FORCE, DK_TRC_T_COORDINATOR_XA_RESULT_CODE, "Rollback",
                         mDtxInfo->mResult, mDtxInfo->mGlobalTxId,
                         sXidString, sFailErrCodes[0], sCountFailXID );
        }
        else
        {
            ideLog::log( DK_TRC_LOG_FORCE, " global transaction: %"ID_UINT32_FMT
                        " was failed with result code %"ID_UINT32_FMT,
                        mDtxInfo->mGlobalTxId, mDtxInfo->mResult );
        }
        
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            NULL, 
                                            0, 
                                            0, 
                                            NULL );
    
        if ( sIsPrepared == ID_FALSE )
        {
            removeDtxInfo();
        }
        else
        {
            /* Nothing to do */
        }
    }

    if ( sIsPrepared == ID_TRUE )
    {
        IDE_TEST( writeXaEndLog() != IDE_SUCCESS );
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;        


    destroyAllRemoteTx();

    dkpProtocolMgr::freeXARecvResult( NULL,
                                      sFailXIDs,
                                      sHeuristicXIDs,
                                      sFailErrCodes );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GET_CONF );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_OPEN_DBLINK_CONF_FAILED ) );
    }
    IDE_EXCEPTION( ERR_XA_ROLLBACK );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_XA_APPLY_FAIL,
                                  mDtxInfo->mResult, mDtxInfo->mGlobalTxId ) );
    }
    IDE_EXCEPTION_END;

    dkpProtocolMgr::freeXARecvResult( NULL,
                                      sFailXIDs,
                                      sHeuristicXIDs,
                                      sFailErrCodes );
   
    return IDE_FAILURE;
}

/************************************************************************
 * Description : Savepoint �� �����Ѵ�.
 *              
 *  aSavepointName  - [IN] ������ savepoint name
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::setSavepoint( const SChar   *aSavepointName )
{
    UInt                sSavepointNameLen = 0;
    UInt                sStage            = 0;
    iduListNode        *sIterator         = NULL;
    dktRemoteTx        *sRemoteTx         = NULL;
    dktSavepoint       *sSavepoint        = NULL;

    IDE_TEST( aSavepointName == NULL );

    IDE_TEST( mGTxStatus >= DKT_GTX_STATUS_PREPARED );

    /* >> BUG-37512 */
    if ( findSavepoint( aSavepointName ) != NULL )
    {
        /* remove this savepoint to reset */
        removeSavepoint( aSavepointName );
    }
    else
    {
        /* do nothing */
    }
    /* << BUG-37512 */

    /* Set savepoint to all remote transactions */
    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        IDE_DASSERT( sRemoteTx != NULL );
            
        IDE_TEST( sRemoteTx->setSavepoint( aSavepointName ) 
                      != IDE_SUCCESS );
    }

    sStage = 1;

    /* Add a savepoint to global coordinator */
    IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::setSavepoint::calloc::Savepoint",
                          ERR_MEMORY_ALLOC_SAVEPOINT );
    IDE_TEST_RAISE( iduMemMgr::calloc( IDU_MEM_DK,
                                       1,   /* alloc count */
                                       ID_SIZEOF( dktSavepoint ),
                                       (void **)&sSavepoint,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_SAVEPOINT );

    sStage = 2;

    sSavepointNameLen = idlOS::strlen( aSavepointName );
    idlOS::memcpy( sSavepoint->mName, aSavepointName, sSavepointNameLen );

    IDU_LIST_INIT_OBJ( &(sSavepoint->mNode), sSavepoint );
    IDU_LIST_ADD_LAST( &mSavepointList, &(sSavepoint->mNode) );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_SAVEPOINT );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sStage )
    {
        case 2:
            (void)iduMemMgr::free( sSavepoint );
            sSavepoint = NULL;
            /* keep going */
        case 1:
            IDU_LIST_ITERATE( &mRTxList, sIterator )
            {
                sRemoteTx = (dktRemoteTx *)sIterator->mObj;
                sRemoteTx->removeSavepoint( aSavepointName );
            }

            break;

        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : ������ �̸��� savepoint �� list ���� ã�´�.
 *  
 * Return : ã�� savepoint, list �� ���� ���� NULL
 *
 *  aSavepointName  - [IN] ã�� savepoint name
 *
 ************************************************************************/
dktSavepoint *  dktGlobalCoordinator::findSavepoint( const SChar *aSavepointName )
{
    iduListNode     *sIterator  = NULL;
    dktSavepoint    *sSavepoint = NULL;

    IDU_LIST_ITERATE( &mSavepointList, sIterator )
    {
        sSavepoint = (dktSavepoint *)sIterator->mObj;

        /* >> BUG-37512 */
        if ( sSavepoint != NULL )
        {
            if ( idlOS::strcmp( aSavepointName, sSavepoint->mName ) == 0 )
            {
                break;
            }
            else
            {
                /* iterate next or NULL return */
                sSavepoint = NULL;
            }
        }
        else
        {
            /* no more iteration */
            IDE_DASSERT( sSavepoint != NULL );
        }
        /* << BUG-37512 */
    }

    return sSavepoint;
}

/************************************************************************
 * Description : �Է¹��� savepoint �� savepoint list �κ��� �����Ѵ�.
 *
 *  aSavepointName  - [IN] ������ savepoint name 
 *
 *  BUG-37512 : ���� ����� removeAllNextSavepoint �Լ��� �����ϰ� 
 *              �� �Լ��� �Է¹��� savepoint �� �����ϴ� �Լ��� ����.
 *
 ************************************************************************/
void    dktGlobalCoordinator::removeSavepoint( const SChar  *aSavepointName )
{
    iduListNode     *sIterator  = NULL;
    dktRemoteTx     *sRemoteTx  = NULL;
    dktSavepoint    *sSavepoint = NULL;

    IDE_DASSERT( aSavepointName != NULL );

    sSavepoint = findSavepoint( aSavepointName );

    if ( sSavepoint != NULL )
    {
        IDU_LIST_ITERATE( &mRTxList, sIterator )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;

            IDE_DASSERT( sRemoteTx != NULL );

            if ( sRemoteTx->findSavepoint( aSavepointName ) != NULL ) 
            {
                sRemoteTx->removeSavepoint( aSavepointName );
            }
            else
            {
                /* next remote transaction */
            }
        }

        destroySavepoint( sSavepoint );
    }
    else
    {
        /* success */
    }
}

/************************************************************************
 * Description : ��� savepoint �� list �κ��� �����Ѵ�.
 *
 *  BUG-37512 : �ż�.
 *
 ************************************************************************/
void    dktGlobalCoordinator::removeAllSavepoint()
{
    iduListNode     *sIterator  = NULL;
    iduListNode     *sNextNode  = NULL;
    dktRemoteTx     *sRemoteTx  = NULL;
    dktSavepoint    *sSavepoint = NULL;

    /* 1. Remove all savepoints from remote transactions */
    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        IDE_DASSERT( sRemoteTx != NULL );

        sRemoteTx->deleteAllSavepoint();
    }

    /* 2. Remove all savepoints from global savepoint list */
    IDU_LIST_ITERATE_SAFE( &mSavepointList, sIterator, sNextNode )
    {
        sSavepoint = (dktSavepoint *)sIterator->mObj;

        destroySavepoint( sSavepoint );
    }
}

/************************************************************************
 * Description : �Է¹��� savepoint ���Ŀ� ���� ��� savepoint �� list 
 *               ���� �����Ѵ�.
 *
 *  aSavepointName  - [IN] savepoint name 
 *
 *  BUG-37512 : �ż�.
 *
 ************************************************************************/
void    dktGlobalCoordinator::removeAllNextSavepoint( const SChar  *aSavepointName )
{
    iduList          sRemoveList;
    iduListNode     *sIterator        = NULL;
    iduListNode     *sNextNode        = NULL;
    iduListNode     *sNextSavepointNode = NULL;
    dktRemoteTx     *sRemoteTx        = NULL;
    dktSavepoint    *sSavepoint       = NULL;
    dktSavepoint    *sRemoveSavepoint = NULL;

    IDE_DASSERT( aSavepointName != NULL );

    sSavepoint = findSavepoint( aSavepointName );

    if ( sSavepoint != NULL )
    {
        IDU_LIST_INIT( &sRemoveList );
        sNextSavepointNode = (iduListNode *)(&sSavepoint->mNode)->mNext;
        IDU_LIST_SPLIT_LIST( &mSavepointList, sNextSavepointNode, &sRemoveList );
        IDU_LIST_ITERATE_SAFE( &sRemoveList, sIterator, sNextNode )
        {
            sRemoveSavepoint = (dktSavepoint *)sIterator->mObj;

            IDU_LIST_ITERATE( &mRTxList, sIterator )
            {
                sRemoteTx = (dktRemoteTx *)sIterator->mObj;

                if ( sRemoteTx->findSavepoint( (const SChar *)sRemoveSavepoint->mName ) != NULL ) 
                {
                    sRemoteTx->removeSavepoint( sRemoveSavepoint->mName );
                }
                else
                {
                    /* savepoint not exist */
                }
            }

            destroySavepoint( sRemoveSavepoint );
        }
    }
    else
    {
        /* success */
    }
}

/************************************************************************
 * Description : �� �۷ι� Ʈ����ǿ� ���� ��� remote transaction ����
 *               �����Ͽ� ������ savepoint �� ������ remote transaction
 *               �� ���� �ִ� remote node session id �� �Է¹��� �迭�� 
 *               ä���ش�.
 *
 * Return      : the number of remote transactions with input savepoint
 *
 *  aSavepointName      - [IN] �˻��� savepoint name
 *  aRemoteNodeIdArr    - [OUT] Remote node session id's array 
 *
 ************************************************************************/
UInt    dktGlobalCoordinator::getRemoteNodeIdArrWithSavepoint( 
    const SChar     *aSavepointName, 
    UShort          *aRemoteNodeIdArr )
{
    UInt            sRemoteTxCnt = 0;
    iduListNode    *sIterator    = NULL;
    dktRemoteTx    *sRTx         = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRTx->findSavepoint( aSavepointName ) != NULL )
        {
            aRemoteNodeIdArr[sRemoteTxCnt] = sRTx->getRemoteNodeSessionId();
            sRemoteTxCnt++;
        }
        else
        {
            /* iterate next */
        }
    }

    return sRemoteTxCnt;
}

/************************************************************************
 * Description : List ���� �Է¹��� savepoint �� ���� remote transaction 
 *               ���� ã�� �����Ѵ�.
 *              
 *  aSavepointName  - [IN] Savepoint name 
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktGlobalCoordinator::destroyRemoteTransactionWithoutSavepoint( const SChar *aSavepointName )
{
    iduListNode    *sIterator = NULL;
    iduListNode    *sNext     = NULL;
    dktRemoteTx    *sRemoteTx = NULL;

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRemoteTx->findSavepoint( aSavepointName ) == NULL )
        {
            destroyRemoteTx( sRemoteTx );
        }
        else
        {
            /* next remote transaction */
        }
    }
}

/************************************************************************
 * Description : Id �� ���� �ش��ϴ� remote statement �� ã�� ��ȯ�Ѵ�.
 *              
 *  aRemoteStmtId   - [IN] ã�� remote statement �� id
 *  aRemoteStmt     - [OUT] ��ȯ�� remote statement ��ü�� ����Ű�� ������
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::findRemoteStmt( SLong            aRemoteStmtId,
                                              dktRemoteStmt  **aRemoteStmt )
{
    iduListNode    *sIterator    = NULL;
    dktRemoteTx    *sRTx         = NULL;
    dktRemoteStmt  *sRemoteStmt  = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRTx = (dktRemoteTx *)sIterator->mObj;
        sRemoteStmt = sRTx->findRemoteStmt( aRemoteStmtId );

        if ( sRemoteStmt != NULL )
        {
            break;
        }
        else
        {
            /* iterate next */
        }
    }

    *aRemoteStmt = sRemoteStmt;

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : �� global coordinator ���� �������� ��� remote 
 *               statement ���� ������ ���Ѵ�.
 *              
 * Return      : remote statement ����
 *
 ************************************************************************/
UInt    dktGlobalCoordinator::getAllRemoteStmtCount()
{
    UInt            sRemoteStmtCnt = 0;
    iduListNode    *sIterator      = NULL;
    dktRemoteTx    *sRTx           = NULL;

    IDE_ASSERT( mDktRTxMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRTx != NULL )
        {   
            sRemoteStmtCnt += sRTx->getRemoteStmtCount();
        }
        else
        {
            /* nothing to do */
        }
    }
    
    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    return sRemoteStmtCnt;
}

/************************************************************************
 * Description : ���� �������� �۷ι� Ʈ������� ������ ���´�.
 *              
 *  aInfo       - [IN] �۷ι� Ʈ������� ������ ���� �迭������
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::getGlobalTransactionInfo( dktGlobalTxInfo  *aInfo )
{
    aInfo->mGlobalTxId    = mGlobalTxId;
    aInfo->mLocalTxId     = mLocalTxId; 
    aInfo->mStatus        = mGTxStatus; 
    aInfo->mSessionId     = mSessionId; 
    aInfo->mRemoteTxCnt   = mRTxCnt; 
    aInfo->mAtomicTxLevel = (UInt)mAtomicTxLevel; 

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : �� global coordinator ���� �������� ��� remote 
 *               transaction �� ������ ���Ѵ�.
 *              
 *  aInfo           - [OUT] ��ȯ�� remote transaction ���� ����
 *  aRemainedCnt    - [IN/OUT] �� ���� �� remote transaction ������ ����
 *  aInfoCnt        - [OUT] �� �Լ����� ä���� remote transaction info (aInfo)
 *                          �� ����
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::getRemoteTransactionInfo( dktRemoteTxInfo  *aInfo,
                                                        UInt              aRTxCnt,
                                                        UInt             *aInfoCnt )
{
    idBool          sIsLock   = ID_FALSE;
    UInt            sInfoCnt  = *aInfoCnt;
    iduListNode    *sIterator = NULL;
    dktRemoteTx    *sRTx      = NULL;

    IDE_ASSERT( mDktRTxMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    sIsLock = ID_TRUE;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRTx = (dktRemoteTx *)sIterator->mObj;

        IDE_TEST_CONT ( sInfoCnt == aRTxCnt, FOUND_COMPLETE );

		if ( sRTx != NULL )
		{
	        IDE_TEST( sRTx->getRemoteTxInfo( &aInfo[sInfoCnt] ) != IDE_SUCCESS );

    	    sInfoCnt++;
		}
		else
		{
			/* nothing to do */
		}
    }

    IDE_EXCEPTION_CONT( FOUND_COMPLETE );

    *aInfoCnt = sInfoCnt;

    sIsLock = ID_FALSE;
    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �� global coordinator ���� �������� ��� remote 
 *               statement �� ������ ���Ѵ�.
 *              
 *  aInfo           - [OUT] ��ȯ�� remote statement ���� ����
 *  aRemaniedCnt    - [IN/OUT] �����ִ� remote statement ������ ����
 *  aInfoCnt        - [OUT] �� �Լ����� ä���� remote transaction info (aInfo)
 *                          �� ����
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::getRemoteStmtInfo( dktRemoteStmtInfo  *aInfo,
                                                 UInt                aStmtCnt,
                                                 UInt               *aInfoCnt )
{
    idBool          sIsLock        = ID_FALSE;
    UInt            sRemoteStmtCnt = *aInfoCnt;
    iduListNode    *sIterator      = NULL;
    dktRemoteTx    *sRTx           = NULL;

    IDE_ASSERT( mDktRTxMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    sIsLock = ID_TRUE;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRTx = (dktRemoteTx *)sIterator->mObj;

        IDE_TEST_CONT( sRemoteStmtCnt >= aStmtCnt, FOUND_COMPLETE );

        if( sRTx != NULL )
        {
            IDE_TEST( sRTx->getRemoteStmtInfo( aInfo,
                                               aStmtCnt,
                                               &sRemoteStmtCnt ) 
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    IDE_EXCEPTION_CONT( FOUND_COMPLETE );

    sIsLock = ID_FALSE;
    IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );

    *aInfoCnt = sRemoteStmtCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sIsLock == ID_TRUE )
    {
		sIsLock = ID_FALSE;
        IDE_ASSERT( mDktRTxMutex.unlock() == IDE_SUCCESS );
    }
    else
    {   
        /* nothing to do */
    }

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::createDtxInfo( UInt aLocalTxId, UInt aGlobalTxId )
{
    mDtxInfo = NULL;

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       ID_SIZEOF( dktDtxInfo ),
                                       (void **)&mDtxInfo,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_DTX_INFO_HEADER );

    generateXID( mGlobalTxId, 0, &mGlobalXID );

    IDE_TEST( mDtxInfo->initialize( &mGlobalXID, 
                                    aLocalTxId, 
                                    aGlobalTxId,
                                    ID_FALSE )
              != IDE_SUCCESS );

    IDE_TEST( writeXaStartReqLog() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_DTX_INFO_HEADER );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( mDtxInfo != NULL )
    {
        (void)iduMemMgr::free( mDtxInfo );
        mDtxInfo = NULL;
    }
    else
    {
        /* do nothing */
    }
    
    return IDE_FAILURE;
}

void  dktGlobalCoordinator::removeDtxInfo()
{
    IDE_ASSERT( mCoordinatorDtxInfoMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );

    mDtxInfo->removeAllBranchTx();
    mDtxInfo->finalize();
    (void)iduMemMgr::free( mDtxInfo );
    mDtxInfo = NULL;

    IDE_ASSERT( mCoordinatorDtxInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

void  dktGlobalCoordinator::removeDtxInfo( dktDtxBranchTxInfo * aDtxBranchTxInfo )
{
    IDE_ASSERT( mCoordinatorDtxInfoMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    (void)mDtxInfo->removeDtxBranchTx( aDtxBranchTxInfo );
    IDE_ASSERT( mCoordinatorDtxInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

void  dktGlobalCoordinator::removeDtxInfo( ID_XID * aXID )
{
    IDE_ASSERT( mCoordinatorDtxInfoMutex.lock( NULL /*idvSQL* */ ) == IDE_SUCCESS );
    (void)mDtxInfo->removeDtxBranchTx( aXID );
    IDE_ASSERT( mCoordinatorDtxInfoMutex.unlock() == IDE_SUCCESS );

    return;
}

IDE_RC  dktGlobalCoordinator::checkAndCloseRemoteTx( dktRemoteTx * aRemoteTx )
{
    dktDtxBranchTxInfo  * sDtxBranchInfo = NULL;
    dksSession          * sDksSession = NULL;

    if ( aRemoteTx->getLinkerType() == DKT_LINKER_TYPE_DBLINK )
    {
        IDE_TEST( dksSessionMgr::getDataDksSession( mSessionId, &sDksSession ) != IDE_SUCCESS );
        IDE_TEST( closeRemoteNodeSessionByRtx( sDksSession, aRemoteTx ) != IDE_SUCCESS );
    }
    else
    {
        /* Nothing to do */
    }

    if ( mDtxInfo != NULL )
    {
        sDtxBranchInfo = mDtxInfo->getDtxBranchTx( &(aRemoteTx->mXID) );
    }
    else
    {
        /*do nothing*/
    }

    if ( sDtxBranchInfo != NULL )
    {
        /*2pc*/
        removeDtxInfo( sDtxBranchInfo );
    }
    else
    {
        /*do nothing: simple transaction and statement execution level*/
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::closeRemoteNodeSessionByRtx( dksSession   *aSession, dktRemoteTx *aRemoteTx )
{
    UShort                sRemoteNodeSessionId = 0;
    UInt                  sRemainedRemoteNodeCnt = 0;
    UInt                  sTimeoutSec = DKU_DBLINK_ALTILINKER_CONNECT_TIMEOUT;
    UInt                  sResultCode = DKP_RC_SUCCESS;

    sRemoteNodeSessionId = aRemoteTx->getRemoteNodeSessionId();

    IDE_TEST( dkpProtocolMgr::sendRequestCloseRemoteNodeSession( aSession,
                                                                 mSessionId,
                                                                 sRemoteNodeSessionId )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestCloseRemoteNodeSessionResult( aSession,
                                                                       mSessionId,
                                                                       sRemoteNodeSessionId,
                                                                       &sResultCode,
                                                                       &sRemainedRemoteNodeCnt,
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
    IDE_ERRLOG(IDE_DK_0);
    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::findRemoteTxNStmt( SLong            aRemoteStmtId,
                                                 dktRemoteTx   ** aRemoteTx,
                                                 dktRemoteStmt ** aRemoteStmt )
{
    iduListNode    *sIterator    = NULL;
    dktRemoteTx    *sRTx         = NULL;
    dktRemoteStmt  *sRemoteStmt  = NULL;

    *aRemoteStmt = NULL;

    if ( &mRTxList != NULL )
    {
        IDU_LIST_ITERATE( &mRTxList, sIterator )
        {
            sRTx = (dktRemoteTx *)sIterator->mObj;
            sRemoteStmt = sRTx->findRemoteStmt( aRemoteStmtId );

            if ( sRemoteStmt != NULL )
            {
                break;
            }
            else
            {
                /* iterate next */
            }
        }

        *aRemoteTx   = sRTx;
        *aRemoteStmt = sRemoteStmt;
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;
}
/*
  * XID: MACADDR (6byte) + Server Initial time sec from 2017 (4byte) + ProcessID (4byte) + Global Transaction ID (4byte)
 */
void dktGlobalCoordinator::generateXID( UInt     aGlobalTxId, 
                                        UInt     aRemoteTxId, 
                                        ID_XID * aXID )
{
    SChar  * sData = aXID->data;

    IDE_DASSERT( ID_SIZEOF(dktGlobalTxMgr::mMacAddr) +
                 ID_SIZEOF(dktGlobalTxMgr::mInitTime) +
                 ID_SIZEOF(dktGlobalTxMgr::mProcessID) +
                 ID_SIZEOF(aGlobalTxId) == DKT_2PC_MAXGTRIDSIZE );
    IDE_DASSERT( ID_SIZEOF(aRemoteTxId) == DKT_2PC_MAXBQUALSIZE );

    dktXid::initXID( aXID );

    idlOS::memcpy( sData, &dktGlobalTxMgr::mMacAddr, ID_SIZEOF(dktGlobalTxMgr::mMacAddr) ); /*6byte*/
    sData += ID_SIZEOF(dktGlobalTxMgr::mMacAddr);

    idlOS::memcpy( sData, &dktGlobalTxMgr::mInitTime, ID_SIZEOF(dktGlobalTxMgr::mInitTime) ); /*4byte*/
    sData += ID_SIZEOF(dktGlobalTxMgr::mInitTime);

    idlOS::memcpy( sData, &dktGlobalTxMgr::mProcessID, ID_SIZEOF(dktGlobalTxMgr::mProcessID) ); /*4byte*/
    sData += ID_SIZEOF(dktGlobalTxMgr::mProcessID);

    idlOS::memcpy( sData, &aGlobalTxId, ID_SIZEOF(aGlobalTxId) ); /*4byte*/
    sData += ID_SIZEOF(aGlobalTxId);

    idlOS::memcpy( sData, &aRemoteTxId, ID_SIZEOF(aRemoteTxId) ); /*4byte*/

    return;
}

void dktGlobalCoordinator::setAllRemoteTxStatus( dktRTxStatus aRemoteTxStatus )
{
    iduListNode     *sIterator     = NULL;
    dktRemoteTx     *sRemoteTx     = NULL;

    IDE_DASSERT( aRemoteTxStatus >= DKT_RTX_STATUS_PREPARE_READY );

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        sRemoteTx->setStatus( aRemoteTxStatus );
    }

    return;
}

IDE_RC dktGlobalCoordinator::freeAndDestroyAllRemoteStmt( dksSession *aSession, UInt  aSessionId )
{
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        if ( sRemoteTx->getStatus() < DKT_RTX_STATUS_PREPARE_READY )
        {
            IDE_TEST( sRemoteTx->freeAndDestroyAllRemoteStmt( aSession, aSessionId )
                      != IDE_SUCCESS );
            
            sRemoteTx->setStatus( DKT_RTX_STATUS_PREPARE_READY );
        }
        else
        {
            /* check next */
        }
    }
    
    setGlobalTxStatus( DKT_GTX_STATUS_PREPARE_READY );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC dktGlobalCoordinator::writeXaStartReqLog()
{
    smLSN    sDummyLSN;

    /* Write Start Log */
    IDE_TEST( smiWriteXaStartReqLog( &mGlobalXID,
                                     mLocalTxId,
                                     &sDummyLSN )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �۷ι� Ʈ����� commit �� ���� XA log�� ����Ѵ�.
 *
 ************************************************************************/
IDE_RC dktGlobalCoordinator::writeXaPrepareReqLog()
{
    UChar  * sSMBranchTx = NULL;
    UInt     sSMBranchTxSize = 0;
    smLSN    sPrepareLSN;
    idBool   sIsAlloced = ID_FALSE;

#ifdef DEBUG
    SChar    sBuf[2048];
    UInt     sBranchTxCnt = 0;
#endif

    if ( mGTxStatus < DKT_GTX_STATUS_PREPARE_REQUEST )
    {
        sSMBranchTxSize = mDtxInfo->estimateSerializeBranchTx();

        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                           sSMBranchTxSize,
                                           (void **)&sSMBranchTx,
                                           IDU_MEM_IMMEDIATE )
                        != IDE_SUCCESS, ERR_MEMORY_ALLOC );
        sIsAlloced = ID_TRUE;

        IDE_TEST( mDtxInfo->serializeBranchTx( sSMBranchTx, sSMBranchTxSize )
                  != IDE_SUCCESS );

#ifdef DEBUG
        mDtxInfo->dumpBranchTx( sBuf,
                                ID_SIZEOF( sBuf ),
                                &sBranchTxCnt );

        ideLog::log( IDE_SD_19,
                     "\n<SMR_LT_XA_PREPARE_REQ Logging> "
                     "LocalID: %"ID_UINT32_FMT", "
                     "GlobalID: %"ID_UINT32_FMT"\n"
                     "BranchTx Count %"ID_UINT32_FMT"\n"
                     "%s",
                     mLocalTxId,
                     mGlobalTxId,
                     sBranchTxCnt,
                     sBuf );
#endif

        /* Write prepare Log */
        IDE_TEST( smiWriteXaPrepareReqLog( &mGlobalXID,
                                           mLocalTxId,
                                           mGlobalTxId,
                                           sSMBranchTx,
                                           sSMBranchTxSize,
                                           &sPrepareLSN )
                  != IDE_SUCCESS );

        /* Set prepare LSN */
        idlOS::memcpy( &(mDtxInfo->mPrepareLSN), &sPrepareLSN, ID_SIZEOF( smLSN ) );

        (void)iduMemMgr::free( sSMBranchTx );
        sSMBranchTx = NULL;

    }
    /*
     * BUG-46262 
     *  if  mGTxStatus >= DKT_GTX_STATUS_PREPARE_REQUEST,just return success. 
     *  because it's a normal case not an error.
     */
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( sIsAlloced == ID_TRUE )
    {
        (void)iduMemMgr::free( sSMBranchTx );
    }
    else
    {
        /* Nothing to do */
    }

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �۷ι� Ʈ����� commit �� ���� XA log�� ����Ѵ�.
 *
 ************************************************************************/
IDE_RC dktGlobalCoordinator::writeXaEndLog()
{
   /* Write End Log */
   IDE_TEST( smiWriteXaEndLog( mLocalTxId,
                               mGlobalTxId )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC dktGlobalCoordinator::executeSimpleTransactionCommitPrepareForShard()
{
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        IDE_TEST( sdi::checkNode( sRemoteTx->getDataNode() ) != IDE_SUCCESS );
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARED );
    mGTxStatus = DKT_GTX_STATUS_PREPARED;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitPrepareForShard()
{
    iduListNode     * sIterator = NULL;
    iduListNode     * sNext     = NULL;
    dktRemoteTx     * sRemoteTx = NULL;
    sdiConnectInfo  * sNode     = NULL;
    void            * sCallback = NULL;
    idBool            sSuccess = ID_TRUE;
    smSCN             sSCN;
    smSCN             sMaxSCN;
    sdiClientInfo   * sClientInfo = NULL;
    idBool            sGCTxSupport = ID_FALSE;

    sClientInfo = getShardClientInfo();
    sGCTxSupport = ( ( isGCTx() == ID_TRUE ) && ( sClientInfo != NULL ) )
                   ? ID_TRUE
                   : ID_FALSE;

    SM_INIT_SCN( &sSCN );
    SM_INIT_SCN( &sMaxSCN );

    IDE_TEST( writeXaPrepareReqLog() != IDE_SUCCESS );

    mGTxStatus = DKT_GTX_STATUS_PREPARE_REQUEST;

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode = sRemoteTx->getDataNode();
        sNode->mReadOnly = (UChar)0;

        if ( ( sNode->mFlag & SDI_CONNECT_COMMIT_PREPARE_MASK )
             == SDI_CONNECT_COMMIT_PREPARE_FALSE )
        {
            IDE_TEST( sdi::setFailoverSuspend( sNode,
                                               SDI_FAILOVER_SUSPEND_ALLOW_RETRY,
                                               sdERR_ABORT_REMOTE_COMMIT_FAILED )
                      != IDE_SUCCESS );

            IDE_TEST( sdi::addPrepareTranCallback( &sCallback, sNode )
                      != IDE_SUCCESS );
        }
        else
        {
            /* Nothing to do. */
        }
    }

    sdi::doCallback( sCallback );

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_WAIT );
    mGTxStatus = DKT_GTX_STATUS_PREPARE_WAIT;

    /* add shard tx ������ �ݴ�� del shard tx�� �����ؾ��Ѵ�. */
    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode = sRemoteTx->getDataNode();

        if ( sdi::resultCallback( sCallback,
                                  sNode,
                                  (SChar*)"SQLEndTransPrepare",
                                  ID_FALSE )
             == IDE_SUCCESS )
        {
            sNode->mFlag &= ~SDI_CONNECT_COMMIT_PREPARE_MASK;
            sNode->mFlag |= SDI_CONNECT_COMMIT_PREPARE_TRUE;

            if ( sNode->mReadOnly == (UChar)1 )
            {
                removeDtxInfo( &sRemoteTx->mXID );
                destroyRemoteTx( sRemoteTx );

                sNode->mRemoteTx = NULL;
                sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
                sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
                // TASK-7244 PSM Partial rollback
                sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
                sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;
            }
            else
            {
                /* Nothing to do. */
            }
        }
        else
        {
            /* ������ ������ ��� */
            sSuccess = ID_FALSE;
        }

        (void)sdi::setFailoverSuspend( sNode, SDI_FAILOVER_SUSPEND_NONE );

        /* PROJ-2733-DistTxInfo */
        if ( sGCTxSupport == ID_TRUE )
        {
            if ( sdi::getSCN( sNode, &sSCN ) == IDE_SUCCESS )
            {
                #if defined(DEBUG)
                ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitPrepareForShard"
                                        ", %s: getSCN : %"ID_UINT64_FMT,
                             sClientInfo->mGCTxInfo.mSessionTypeString,
                             sNode->mNodeName,
                             sSCN );
                #endif

                SM_SET_MAX_SCN( &sMaxSCN, &sSCN );
            }
            else
            {
                sSuccess = ID_FALSE;

                ideLog::log( IDE_SD_0, "= [%s] executeTwoPhaseCommitPrepareForShard"
                                       ", %s: failure getSCN"
                                       ", %s",
                             sClientInfo->mGCTxInfo.mSessionTypeString,
                             sNode->mNodeName,
                             ideGetErrorMsg( ideGetErrorCode() ) );
            }
        }
    }

    IDE_TEST( sSuccess == ID_FALSE );

    sdi::removeCallback( sCallback );

    setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARED );
    mGTxStatus = DKT_GTX_STATUS_PREPARED;

    /* PROJ-2733-DistTxInfo */
    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_SCN( &sClientInfo->mGCTxInfo.mPrepareSCN, &sMaxSCN );
        SM_SET_SCN( &sClientInfo->mGCTxInfo.mGlobalCommitSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitPrepareForShard"
                                ", PrepareSCN : %"ID_UINT64_FMT,
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mPrepareSCN );
        #endif
    }

    IDU_FIT_POINT("5.TASK-7220@dktGlobalCoordinator::executeTwoPhaseCommitPrepareForShard::end");

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    sdi::removeCallback( sCallback );

    if ( sGCTxSupport == ID_TRUE )
    {
        SM_INIT_SCN( &sClientInfo->mGCTxInfo.mPrepareSCN );
        SM_INIT_SCN( &sClientInfo->mGCTxInfo.mGlobalCommitSCN );
        SM_SET_MAX_SCN( &sClientInfo->mGCTxInfo.mCoordSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitPrepareForShard"
                                ", failure CoordSCN : %"ID_UINT64_FMT
                                ", %s",
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mCoordSCN,
                     ideGetErrorMsg( ideGetErrorCode() ) );
        #endif
    }

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitCommitForShard()
{
    iduListNode     *sIterator = NULL;
    iduListNode     *sNext     = NULL;
    dktRemoteTx     *sRemoteTx = NULL;
    sdiConnectInfo  *sNode     = NULL;
    idBool           sSuccess  = ID_TRUE;

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMIT_WAIT );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_WAIT;

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode     = sRemoteTx->getDataNode();

        if ( sdi::commit( sNode ) != IDE_SUCCESS )
        {
            sSuccess = ID_FALSE;
        }
        else
        {
            /* Nothing to do. */
        }

        destroyRemoteTx( sRemoteTx );

        sNode->mRemoteTx = NULL;
        sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
        sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
        // TASK-7244 PSM Partial rollback
        sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
        sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMITTED );
    mGTxStatus = DKT_GTX_STATUS_COMMITTED;

    IDE_TEST( sSuccess == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitCommitForShard()
{
    iduListNode    * sIterator = NULL;
    iduListNode    * sNext     = NULL;
    dktRemoteTx    * sRemoteTx = NULL;
    sdiConnectInfo * sNode     = NULL;
    void           * sCallback = NULL;
    idBool           sSuccess = ID_TRUE;
    sdiClientInfo  * sClientInfo = NULL;
    smSCN            sSCN;
    smSCN            sMaxSCN;
    idBool           sGCTxSupport = ID_FALSE;

    sClientInfo = getShardClientInfo();
    sGCTxSupport = ( ( isGCTx() == ID_TRUE ) && ( sClientInfo != NULL ) )
                   ? ID_TRUE
                   : ID_FALSE;

    SM_INIT_SCN( &sMaxSCN );

    /* PROJ-2733-DistTxInfo */
    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_SCN( &mDtxInfo->mGlobalCommitSCN, &sClientInfo->mGCTxInfo.mGlobalCommitSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] dktGlobalCoordinator::executeTwoPhaseCommitCommitForShard"
                                ", CommitSCN : %"ID_UINT64_FMT,
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mGlobalCommitSCN );
        #endif
    }

    mDtxInfo->mResult = SMI_DTX_COMMIT;

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMIT_WAIT );
    mGTxStatus = DKT_GTX_STATUS_COMMIT_WAIT;

    IDU_FIT_POINT("5.TASK-7220@dktGlobalCoordinator::executeTwoPhaseCommitCommitForShard");

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode = sRemoteTx->getDataNode();

        IDE_DASSERT( ( sNode->mFlag & SDI_CONNECT_COMMIT_PREPARE_MASK )
                     == SDI_CONNECT_COMMIT_PREPARE_TRUE );

        IDE_TEST( sdi::setFailoverSuspend( sNode,
                                           SDI_FAILOVER_SUSPEND_ALL,
                                           sdERR_ABORT_REMOTE_COMMIT_FAILED )
                  != IDE_SUCCESS );

        /* PROJ-2733-DistTxInfo */
        if ( sGCTxSupport == ID_TRUE )
        {
            IDE_TEST_RAISE( sdi::setSCN( sNode, &sClientInfo->mGCTxInfo.mGlobalCommitSCN ) != IDE_SUCCESS,
                            ERR_SET_SCN);

            #if defined(DEBUG)
            ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitCommitForShard"
                                    ", %s: setSCN, GlobalCommitSCN : %"ID_UINT64_FMT,
                         sClientInfo->mGCTxInfo.mSessionTypeString,
                         sNode->mNodeName,
                         sClientInfo->mGCTxInfo.mGlobalCommitSCN );
            #endif
        }

        IDE_TEST( sdi::addEndPendingTranCallback( &sCallback,
                                                  sNode,
                                                  ID_TRUE )
                  != IDE_SUCCESS );
    }

    sdi::doCallback( sCallback );

    /* add shard tx ������ �ݴ�� del shard tx�� �����ؾ��Ѵ�. */
    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode = sRemoteTx->getDataNode();

        if ( sdi::resultCallback( sCallback,
                                  sNode,
                                  (SChar*)"SQLEndPending",
                                  ID_TRUE )
             == IDE_SUCCESS )
        {
            removeDtxInfo( &sRemoteTx->mXID );
        }
        else
        {
            /* ������ ������ ��� */
            sSuccess = ID_FALSE;
        }

        (void)sdi::setFailoverSuspend( sNode, SDI_FAILOVER_SUSPEND_NONE );

        destroyRemoteTx( sRemoteTx );

        sNode->mRemoteTx = NULL;
        sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
        sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
        // TASK-7244 PSM Partial rollback
        sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
        sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;

        /* PROJ-2733-DistTxInfo */
        if ( sGCTxSupport == ID_TRUE )
        {
            if ( sdi::getSCN( sNode, &sSCN ) == IDE_SUCCESS )
            {
                #if defined(DEBUG)
                ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitCommitForShard"
                                        ", %s: getSCN : %"ID_UINT64_FMT,
                             sClientInfo->mGCTxInfo.mSessionTypeString,
                             sNode->mNodeName,
                             sSCN );
                #endif

                SM_SET_MAX_SCN( &sMaxSCN,  &sSCN);
            }
            else
            {
                sSuccess = ID_FALSE;

                ideLog::log( IDE_SD_0, "= [%s] executeTwoPhaseCommitCommitForShard"
                                       ", %s: failure getSCN"
                                       ", %s",
                             sClientInfo->mGCTxInfo.mSessionTypeString,
                             sNode->mNodeName,
                             ideGetErrorMsg( ideGetErrorCode() ) );
            }
        }
    }

    IDU_FIT_POINT_RAISE( "dktGlobalCoordinator::executeTwoPhaseCommitCommitForShard::BeforeWriteXaEndLog",
                          ERR_END_TRANS );
    IDE_TEST_RAISE( sSuccess == ID_FALSE, ERR_END_TRANS );

    IDE_TEST( writeXaEndLog() != IDE_SUCCESS );

    sdi::removeCallback( sCallback );

    setAllRemoteTxStatus( DKT_RTX_STATUS_COMMITTED );
    mGTxStatus = DKT_GTX_STATUS_COMMITTED;

    /* PROJ-2733-DistTxInfo */
    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_MAX_SCN( &sClientInfo->mGCTxInfo.mCoordSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitCommitForShard"
                                ", CoordSCN : %"ID_UINT64_FMT,
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mCoordSCN );
        #endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_END_TRANS )
    {
        /* already set error code */
    }
    IDE_EXCEPTION( ERR_SET_SCN )
    {
        /* already set error code */
        ideLog::log( IDE_SD_0, "= [%s] executeTwoPhaseCommitCommitForShard"
                                ", %s: failure setSCN, GlobalCommitSCN : %"ID_UINT64_FMT
                                ", %s",
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sNode->mNodeName,
                     sClientInfo->mGCTxInfo.mGlobalCommitSCN,
                     ideGetErrorMsg( ideGetErrorCode() ) );
    }
    IDE_EXCEPTION_END;

    sdi::removeCallback( sCallback );

    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_MAX_SCN( &sClientInfo->mGCTxInfo.mCoordSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitCommitForShard"
                                ", failure CoordSCN : %"ID_UINT64_FMT
                                ", %s",
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mCoordSCN,
                     ideGetErrorMsg( ideGetErrorCode() ) );
        #endif
    }

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitRollbackForShard( SChar *aSavepointName )
{
    iduListNode     *sIterator = NULL;
    iduListNode     *sNext     = NULL;
    dktRemoteTx     *sRemoteTx = NULL;
    sdiConnectInfo  *sNode     = NULL;
    idBool           sSuccess = ID_TRUE;

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    if ( aSavepointName != NULL )
    {
        IDE_TEST(executeSavepointRollbackForShard(aSavepointName) != IDE_SUCCESS);
    }
    else
    {
        IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;
            sNode     = sRemoteTx->getDataNode();

            if ( sNode->mDbc != NULL )
            {
                if ( sdi::rollback( sNode, NULL ) != IDE_SUCCESS )
                {
                    sSuccess = ID_FALSE;
                }
                else
                {
                    /* Nothing to do */
                }
            }
            else
            {
                /* Session is disconnected. Do it as success. */
                IDE_DASSERT( getFlag( DKT_COORD_FLAG_TRANSACTION_BROKEN_MASK )
                             == DKT_COORD_FLAG_TRANSACTION_BROKEN_TRUE );
            }

            destroyRemoteTx( sRemoteTx );

            sNode->mRemoteTx = NULL;
            sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
            sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
            // TASK-7244 PSM Partial rollback
            sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
            sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;
        }
    }

    if ( mRTxCnt == 0 )
    {
        setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
        mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;
    }
    else
    {
        setAllRemoteTxStatus( DKT_RTX_STATUS_PREPARE_READY );
        mGTxStatus = DKT_GTX_STATUS_PREPARE_READY;
    }

    IDE_TEST( sSuccess == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeSavepointRollbackForShard( SChar *aSavepointName )
{
    iduListNode     *sIterator = NULL;
    iduListNode     *sNext     = NULL;
    dktRemoteTx     *sRemoteTx = NULL;
    sdiConnectInfo  *sNode     = NULL;
    idBool           sSuccess = ID_TRUE;

    IDE_DASSERT( aSavepointName != NULL );

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode     = sRemoteTx->getDataNode();

        /* BUG-48489 Remote���� Partial, Total rollback�� �Ǵ��Ѵ�. */
        if ( sdi::rollback( sNode, (const SChar*)aSavepointName ) != IDE_SUCCESS )
        {
            sSuccess = ID_FALSE;
        }
        else
        {
            /* Nothing to do */
        }
    }

    IDE_TEST( sSuccess == ID_FALSE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::executeSimpleTransactionCommitRollbackForceForShard()
{
    iduListNode     *sIterator = NULL;
    iduListNode     *sNext     = NULL;
    dktRemoteTx     *sRemoteTx = NULL;
    sdiConnectInfo  *sNode     = NULL;

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

    IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode     = sRemoteTx->getDataNode();

        if ( sNode->mDbc != NULL )
        {
            (void)sdi::rollback( sNode, NULL );
        }
        else
        {
            /* Session is disconnected. Do it as success. */
            IDE_DASSERT( getFlag( DKT_COORD_FLAG_TRANSACTION_BROKEN_MASK )
                         == DKT_COORD_FLAG_TRANSACTION_BROKEN_TRUE );
        }

        destroyRemoteTx( sRemoteTx );

        sNode->mRemoteTx = NULL;
        sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
        sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
        // TASK-7244 PSM Partial rollback
        sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
        sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;
    }

    setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
    mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;

    return IDE_SUCCESS;

    /* failure �� �����ϸ� �ȵȴ�. */
}

IDE_RC  dktGlobalCoordinator::executeTwoPhaseCommitRollbackForShard(SChar * aSavepointName)
{
    iduListNode    * sIterator = NULL;
    iduListNode    * sNext     = NULL;
    dktRemoteTx    * sRemoteTx = NULL;
    sdiConnectInfo * sNode     = NULL;
    void           * sCallback = NULL;
    idBool           sSuccess = ID_TRUE;
    sdiClientInfo  * sClientInfo = NULL;
    smSCN            sSCN;
    smSCN            sMaxSCN;
    idBool           sGCTxSupport = ID_FALSE;

    idBool           sIsPrepared = ID_FALSE;

    sClientInfo = getShardClientInfo();
    sGCTxSupport = ( ( isGCTx() == ID_TRUE ) && ( sClientInfo != NULL ) )
                   ? ID_TRUE
                   : ID_FALSE;

    SM_INIT_SCN( &sMaxSCN );

    if( ( mGTxStatus >= DKT_GTX_STATUS_PREPARE_REQUEST ) &&
        ( mGTxStatus <= DKT_GTX_STATUS_PREPARED ) )
    {
        sIsPrepared = ID_TRUE;
    }
    else
    {
        sIsPrepared = ID_FALSE;
    }

    if ( aSavepointName != NULL )
    {
        /*prepare�Ǿ����� ����������.*/
        IDE_TEST_RAISE(sIsPrepared == ID_TRUE, ERR_CANT_SVP_ROLLBACK);
        IDE_TEST(executeSavepointRollbackForShard(aSavepointName) != IDE_SUCCESS);
    }
    else
    {
        mDtxInfo->mResult = SMI_DTX_ROLLBACK;
        setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACK_WAIT );
        mGTxStatus = DKT_GTX_STATUS_ROLLBACK_WAIT;

        IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;
            sNode = sRemoteTx->getDataNode();

            if ( sNode->mDbc != NULL )
            {
                IDE_TEST( sdi::setFailoverSuspend( sNode,
                                                   SDI_FAILOVER_SUSPEND_ALL,
                                                   sdERR_ABORT_REMOTE_COMMIT_FAILED )
                          != IDE_SUCCESS );

                if ( ( sNode->mFlag & SDI_CONNECT_COMMIT_PREPARE_MASK )
                     == SDI_CONNECT_COMMIT_PREPARE_FALSE )
                {
                    IDE_TEST( sdi::addEndTranCallback( &sCallback,
                                                       sNode,
                                                       ID_FALSE )
                              != IDE_SUCCESS );
                }
                else
                {
                    IDE_TEST( sdi::addEndPendingTranCallback( &sCallback,
                                                              sNode,
                                                              ID_FALSE )
                              != IDE_SUCCESS );
                }
            }
            else
            {
                /* Session is disconnected. Do it as success. */
                IDE_DASSERT( getFlag( DKT_COORD_FLAG_TRANSACTION_BROKEN_MASK )
                             == DKT_COORD_FLAG_TRANSACTION_BROKEN_TRUE );
            }
        }

        sdi::doCallback( sCallback );

        /* add shard tx ������ �ݴ�� del shard tx�� �����ؾ��Ѵ�. */
        IDU_LIST_ITERATE_SAFE( &mRTxList, sIterator, sNext )
        {
            sRemoteTx = (dktRemoteTx *)sIterator->mObj;
            sNode = sRemoteTx->getDataNode();

            if ( sNode->mDbc != NULL )
            {
                if ( sdi::resultCallback( sCallback,
                                          sNode,
                                          (SChar*)"SQLEndTran",
                                          ID_TRUE )
                     == IDE_SUCCESS )
                {
                    removeDtxInfo( &sRemoteTx->mXID );
                }
                else
                {
                    /* ������ ������ ��� */
                    sSuccess = ID_FALSE;
                }

                (void)sdi::setFailoverSuspend( sNode, SDI_FAILOVER_SUSPEND_NONE );
            }
            else
            {
                /* Session is disconnected. Do it as success. */
                removeDtxInfo( &sRemoteTx->mXID );

                IDE_DASSERT( getFlag( DKT_COORD_FLAG_TRANSACTION_BROKEN_MASK )
                             == DKT_COORD_FLAG_TRANSACTION_BROKEN_TRUE );
            }

            destroyRemoteTx( sRemoteTx );

            sNode->mRemoteTx = NULL;
            sNode->mFlag &= ~SDI_CONNECT_REMOTE_TX_CREATE_MASK;
            sNode->mFlag |= SDI_CONNECT_REMOTE_TX_CREATE_FALSE;
            // TASK-7244 PSM Partial rollback
            sNode->mFlag &= ~SDI_CONNECT_PSM_SVP_SET_MASK;
            sNode->mFlag |= SDI_CONNECT_PSM_SVP_SET_FALSE;

            /* PROJ-2733-DistTxInfo */
            if ( sGCTxSupport == ID_TRUE )
            {
                if ( sdi::getSCN( sNode, &sSCN ) == IDE_SUCCESS )
                {
                    #if defined(DEBUG)
                    ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitRollbackForShard"
                                            ", %s: getSCN : %"ID_UINT64_FMT,
                                 sClientInfo->mGCTxInfo.mSessionTypeString,
                                 sNode->mNodeName,
                                 sSCN );
                    #endif

                    SM_SET_MAX_SCN( &sMaxSCN,  &sSCN);
                }
                else
                {
                    sSuccess = ID_FALSE;

                    ideLog::log( IDE_SD_0, "= [%s] executeTwoPhaseCommitRollbackForShard"
                                           ", %s: failure getSCN"
                                           ", %s",
                                 sClientInfo->mGCTxInfo.mSessionTypeString,
                                 sNode->mNodeName,
                                 ideGetErrorMsg( ideGetErrorCode() ) );
                }
            }
        }

        IDE_TEST_RAISE( sSuccess == ID_FALSE, ERR_END_TRANS );

        sdi::removeCallback( sCallback );

        if( sIsPrepared == ID_TRUE )
        {
            IDE_TEST( writeXaEndLog() != IDE_SUCCESS );
        }

        setAllRemoteTxStatus( DKT_RTX_STATUS_ROLLBACKED );
        mGTxStatus = DKT_GTX_STATUS_ROLLBACKED;
    }

    /* PROJ-2733-DistTxInfo */
    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_SCN( &sClientInfo->mGCTxInfo.mCoordSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitRollbackForShard"
                                ", CoordSCN : %"ID_UINT64_FMT,
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mCoordSCN );
        #endif
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_END_TRANS )
    {
        /* already set error code */
    }
    IDE_EXCEPTION( ERR_CANT_SVP_ROLLBACK )
    {
    	IDE_SET(ideSetErrorCode(dkERR_ABORT_DKM_GTX_ROLLBACK_IMPOSSIBLE));
    }

    IDE_EXCEPTION_END;

    if ( sCallback != NULL )
    {
        sdi::removeCallback( sCallback );
    }

    if ( sGCTxSupport == ID_TRUE )
    {
        SM_SET_MAX_SCN( &sClientInfo->mGCTxInfo.mCoordSCN, &sMaxSCN );

        #if defined(DEBUG)
        ideLog::log( IDE_SD_18, "= [%s] executeTwoPhaseCommitRollbackForShard"
                                ", failure CoordSCN : %"ID_UINT64_FMT
                                ", %s",
                     sClientInfo->mGCTxInfo.mSessionTypeString,
                     sClientInfo->mGCTxInfo.mCoordSCN,
                     ideGetErrorMsg( ideGetErrorCode() ) );
        #endif
    }

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Savepoint�� �����Ѵ�.
 *
 *  aSavepointName  - [IN] Savepoint name
 *
 ************************************************************************/
IDE_RC  dktGlobalCoordinator::executeSavepointForShard( const SChar *aSavepointName )
{
    iduListNode     *sIterator = NULL;
    dktRemoteTx     *sRemoteTx = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;

        IDE_TEST( sdi::savepoint( sRemoteTx->getDataNode(),
                                  aSavepointName )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  dktGlobalCoordinator::closeAllShardTransasction()
{
    iduListNode    * sIterator = NULL;
    dktRemoteTx    * sRemoteTx = NULL;
    sdiConnectInfo * sNode     = NULL;

    IDU_LIST_ITERATE( &mRTxList, sIterator )
    {
        sRemoteTx = (dktRemoteTx *)sIterator->mObj;
        sNode = sRemoteTx->getDataNode();

        sdi::freeConnectImmediately( sNode );
    }

    return IDE_SUCCESS;
}

