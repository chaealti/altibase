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

#include <dktRemoteStmt.h>
#include <dkErrorCode.h>
#include <dkdMisc.h>

/************************************************************************
 * Description : �� remote statement ��ü�� type �� ���� �ʱ�ȭ���ش�.
 *
 *  aGTxId      - [IN] �� remote statement �� ���� global transaction �� 
 *                     id
 *  aRTxId      - [IN] �� remote statement �� ���� remote transaction ��
 *                     id
 *  aId         - [IN] Remote statement �� id
 *  aStmtType   - [IN] Remote statement �� type ���� ������ �ϳ��� ���� 
 *                     ���´�.
 *  
 *            ++ Remote Statement Types:
 *
 *              0. DKT_STMT_TYPE_REMOTE_TABLE
 *              1. DKT_STMT_TYPE_REMOTE_EXECUTE_IMMEDIATE
 *              2. DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT
 *              3. DKT_STMT_TYPE_REMOTE_EXECUTE_NON_QUERY_STATEMENT
 *              4. DKT_STMT_TYPE_REMOTE_TABLE_STORE
 *
 *  aStmtStr    - [IN] ���ݼ������� ����� ����
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::initialize( UInt    aGlobalTxId,
                                   UInt     aLocalTxId,
                                   UInt     aRTxId,
                                   SLong    aId,
                                   UInt     aStmtType,
                                   SChar   *aStmtStr )
{
    UInt    sStmtLen = 0;

    mId             = aId;
    mStmtType       = aStmtType;
    mResult         = DKP_RC_FAILED;
    mDataMgr        = NULL;
    mRemoteTableMgr = NULL;
    mBuffSize       = 0;
    mBatchStatementMgr = NULL;
    mGlobalTxId     = aGlobalTxId;
    mLocalTxId      = aLocalTxId;
    mRTxId          = aRTxId;
    mStmtStr        = NULL;

    dkoLinkObjMgr::initCopiedLinkObject( &mExecutedLinkObj );

    switch ( aStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
            IDE_TEST( createDataManager() != IDE_SUCCESS );
            break;

        case DKT_STMT_TYPE_REMOTE_TABLE:
#ifdef ALTIBASE_PRODUCT_XDB
            IDE_TEST( createDataManager() != IDE_SUCCESS );
#else
            IDE_TEST( createRemoteTableManager() != IDE_SUCCESS );
#endif
            break;

        default:
            /* nothing to do */
            break;
    }

    idlOS::memset( &mErrorInfo, 0, ID_SIZEOF( dktErrorInfo ) );
    
    sStmtLen = idlOS::strlen( aStmtStr );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       sStmtLen + 1,
                                       (void **)&mStmtStr,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_STMT_STR );

    idlOS::strcpy( mStmtStr, aStmtStr );

    IDE_TEST( dkpBatchStatementMgrCreate( &mBatchStatementMgr ) != IDE_SUCCESS );
    mBatchExecutionResult = NULL;
    mBatchExecutionResultSize = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_STMT_STR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( mBatchStatementMgr != NULL )
    {
        dkpBatchStatementMgrDestroy( mBatchStatementMgr );
        mBatchStatementMgr = NULL;
    }
    else
    {
        /* nothing to do */
    }
    
    if ( mRemoteTableMgr != NULL )
    {    
        destroyRemoteTableManager();
    }
    else
    {
        if ( mDataMgr != NULL )
        {
            destroyDataManager();
        }
        else
        {
            /* there is no data manager created */
        }
    }

    if ( mStmtStr != NULL )
    {
        (void)iduMemMgr::free( mStmtStr );
        mStmtStr = NULL;
    }
    else
    {
        /* do nothing */
    }
    
    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �� remote statement ��ü�� ���� �Ҵ�� �ڿ��� �������ְ� 
 *               ������� �ʱ�ȭ���ش�. ���������� clean() �� ȣ���Ѵ�.
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktRemoteStmt::finalize()
{
    clean();
}

/************************************************************************
 * Description : ���� �Ҵ�� �ڿ��� �������ְ� ������� �ʱ�ȭ���ش�.
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktRemoteStmt::clean() 
{
    mId        = DK_INVALID_STMT_ID;
    mStmtType  = DK_INVALID_STMT_TYPE;
    mResult    = DKP_RC_FAILED;

    dkoLinkObjMgr::initCopiedLinkObject( &mExecutedLinkObj );

    if ( mRemoteTableMgr != NULL )
    {
        destroyRemoteTableManager();
    }
    else
    {
        if ( mDataMgr != NULL )
        {
            destroyDataManager();
        }
        else
        {
            /* do nothing */
        }
    }

    if ( mStmtStr != NULL )
    {
        (void)iduMemMgr::free( mStmtStr );
        mStmtStr = NULL;
    }
    else
    {
        /* do nothing */
    }

    mErrorInfo.mErrorCode = 0;

    (void)idlOS::memset( mErrorInfo.mSQLState, 
                         0, 
                         ID_SIZEOF( mErrorInfo.mSQLState ) );

    if ( mErrorInfo.mErrorDesc != NULL )
    {
        (void)iduMemMgr::free( mErrorInfo.mErrorDesc );
        mErrorInfo.mErrorDesc = NULL;
    }
    else
    {
        /* do nothing */
    }

    if ( mBatchExecutionResult != NULL )
    {
        (void)iduMemMgr::free( mBatchExecutionResult );
        mBatchExecutionResult = NULL;
        mBatchExecutionResultSize = 0;
    }
    else
    {
        /* nothing to do */
    }
     
    if ( mBatchStatementMgr != NULL )
    {
        dkpBatchStatementMgrDestroy( mBatchStatementMgr );
        mBatchStatementMgr = NULL;
    }
    else
    {
        /* nothing to do */
    }
}

/************************************************************************
 * Description : Remote statement �� �����ϱ� ���� protocol operation 
 *               �� �����Ѵ�. 
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *  aLinkObj    - [IN] �� ������ ����ϴ� database link ��ü�� ����
 *  aRemoteSessionId - [IN] database link �� ���� �����Ϸ��� remote 
 *                          session �� id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::prepareProtocol( dksSession   * aSession, 
                                        UInt           aSessionId,
                                        idBool         aIsTxBegin,
                                        ID_XID       * aXID,
                                        dkoLink      * aLinkObj,
                                        UShort       * aRemoteSessionId )
{
    UInt            sResultCode = DKP_RC_SUCCESS;
    UInt            sTimeoutSec;
    UShort          sRemoteSessionId = DK_INIT_REMOTE_SESSION_ID;
    SInt            sReceiveTimeout = 0;
    SInt            sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dkpProtocolMgr::sendPrepareRemoteStmt( aSession,
                                                     aSessionId,
                                                     aIsTxBegin,
                                                     aXID,
                                                     aLinkObj,
                                                     mId,
                                                     mStmtStr )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvPrepareRemoteStmtResult( aSession,
                                                           aSessionId,
                                                           mId,
                                                           &sResultCode,
                                                           &sRemoteSessionId,
                                                           sTimeoutSec )
              != IDE_SUCCESS );

    /* Analyze result code */
    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    *aRemoteSessionId = sRemoteSessionId;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC  dktRemoteStmt::prepareBatchProtocol( dksSession    *aSession, 
                                             UInt           aSessionId,
                                             idBool         aIsTxBegin,
                                             ID_XID        *aXID,
                                             dkoLink       *aLinkObj,
                                             UShort        *aRemoteSessionId )
{
    UInt            sResultCode = DKP_RC_SUCCESS;
    UInt            sTimeoutSec = 0;
    UShort          sRemoteSessionId = DK_INVALID_REMOTE_SESSION_ID;
    SInt            sReceiveTimeout = 0;
    SInt            sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dkpProtocolMgr::sendPrepareRemoteStmtBatch( aSession,
                                                          aSessionId,
                                                          aIsTxBegin,
                                                          aXID,
                                                          aLinkObj,
                                                          mId,
                                                          mStmtStr )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvPrepareRemoteStmtBatchResult( aSession,
                                                                aSessionId,
                                                                mId,
                                                                &sResultCode,
                                                                &sRemoteSessionId,
                                                                sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    *aRemoteSessionId = sRemoteSessionId;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */ 
IDE_RC  dktRemoteStmt::prepare( dksSession    *aSession, 
                                UInt           aSessionId,
                                idBool         aIsTxBegin,
                                ID_XID        *aXID,
                                dkoLink       *aLinkObj,
                                UShort        *aRemoteSessionId )
{
    switch ( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT:
            IDE_TEST( prepareProtocol( aSession,
                                       aSessionId,
                                       aIsTxBegin,
                                       aXID,
                                       aLinkObj,
                                       aRemoteSessionId )
                      != IDE_SUCCESS );
            break;
            
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH:
            IDE_TEST( prepareBatchProtocol( aSession,
                                            aSessionId,
                                            aIsTxBegin,
                                            aXID,
                                            aLinkObj,
                                            aRemoteSessionId )
                      != IDE_SUCCESS );            
            break;

        case DKT_STMT_TYPE_REMOTE_EXECUTE_IMMEDIATE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_NON_QUERY_STATEMENT:
        default:
            IDE_RAISE( ERR_INVALID_REMOTE_STMT_TYPE );
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Result set �� meta �� ������ ���� protocol operation 
 *               �� �����Ѵ�. 
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::requestResultSetMetaProtocol( dksSession *aSession, 
                                                     UInt        aSessionId )
{
    UInt            sColCnt     = 0;
    UInt            sResultCode = DKP_RC_SUCCESS;
    ULong           sTimeoutSec;
    dkpColumn      *sColMetaArr = NULL;
    SInt            sReceiveTimeout = 0;
    SInt            sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;
                 
    IDE_TEST( dkpProtocolMgr::sendRequestResultSetMeta( aSession,
                                                        aSessionId,
                                                        mId )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestResultSetMetaResult( aSession, 
                                                              aSessionId, 
                                                              mId, 
                                                              &sResultCode, 
                                                              &sColCnt,
                                                              &sColMetaArr,
                                                              sTimeoutSec )
              != IDE_SUCCESS );
     
    /* Analyze result code */
    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    /* Validate result column meta using DK converter */
    IDE_TEST( createTypeConverter( sColMetaArr, sColCnt ) != IDE_SUCCESS );

    if ( sColMetaArr != NULL )
    {
        (void)iduMemMgr::free( sColMetaArr );
        sColMetaArr = NULL;
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    /* >> BUG-37663 */
    IDE_PUSH();

    if ( sColMetaArr != NULL )
    {
        (void)iduMemMgr::free( sColMetaArr );
        sColMetaArr = NULL;
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();
    /* << BUG-37663 */

    return IDE_FAILURE;

}

/************************************************************************
 * Description : requestResultSetMetaProtocol ���� ���ݼ����κ��� ������
 *               result set meta �� �̿��� type converter �� �����Ѵ�.
 *
 *  aColMetaArr - [IN] ���ݼ����κ��� ������ result set meta arrary
 *  aColCount   - [IN] result set �� �÷����� 
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::createTypeConverter( dkpColumn *aColMetaArr,
                                            UInt       aColCount )
{
    switch( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
#ifdef ALTIBASE_PRODUCT_XDB
            IDE_TEST( mDataMgr->createTypeConverter( aColMetaArr, aColCount )
                      != IDE_SUCCESS );
#else
            IDE_TEST( mRemoteTableMgr->createTypeConverter( aColMetaArr, aColCount )
                      != IDE_SUCCESS );
#endif
            break;

        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
            IDE_TEST( mDataMgr->createTypeConverter( aColMetaArr, aColCount ) 
                      != IDE_SUCCESS );
            break;

        default:
            IDE_RAISE( ERR_INVALID_REMOTE_STMT_TYPE );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : type converter �� ��ȯ�Ѵ�.
 *
 ************************************************************************/
IDE_RC dktRemoteStmt::getTypeConverter( dkdTypeConverter ** aTypeConverter )
{
    dkdTypeConverter    *sTypeConverter = NULL;

    switch( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
#ifdef ALTIBASE_PRODUCT_XDB
            sTypeConverter = mDataMgr->getTypeConverter();
#else
            sTypeConverter = mRemoteTableMgr->getTypeConverter();
#endif
            break;

        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:

            sTypeConverter = mDataMgr->getTypeConverter();
            break;

        default:
            break;
    }

    IDE_TEST_RAISE( sTypeConverter == NULL, ERR_GET_TYPE_CONVERTER_NULL );
    *aTypeConverter = sTypeConverter;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_GET_TYPE_CONVERTER_NULL )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_INTERNAL_ERROR, 
                                  "[getTypeConverter] sTypeConverter is NULL" ) ); 
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : type converter �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::destroyTypeConverter()
{
    switch( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
#ifdef ALTIBASE_PRODUCT_XDB
            IDE_TEST( mDataMgr->destroyTypeConverter() != IDE_SUCCESS );
#else
            IDE_TEST( mRemoteTableMgr->destroyTypeConverter() != IDE_SUCCESS );
#endif
            break;

        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:

            IDE_TEST( mDataMgr->destroyTypeConverter() != IDE_SUCCESS );
            break;

        default:
            IDE_RAISE( ERR_INVALID_REMOTE_STMT_TYPE );
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote statement �� �����ϱ� ���� protocol operation 
 *               �� �����Ѵ�. 
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *  aLinkObj    - [IN] �� ������ ����ϴ� database link ��ü�� ����
 *  aResult     - [IN] Protocol operation ������ ���
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::executeProtocol( dksSession    *aSession,
                                        UInt           aSessionId,
                                        idBool         aIsTxBegin,
                                        ID_XID        *aXID,
                                        dkoLink       *aLinkObj,
                                        SInt          *aResult )
{
    SInt            sResult;
    UInt            sStmtType;
    UInt            sSendStmtType;
    UInt            sResultCode = DKP_RC_SUCCESS;
    ULong           sTimeoutSec;
    UShort          sRemoteSessionId;
    SInt            sReceiveTimeout = 0;
    SInt            sQueryTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerQueryTimeoutFromConf( &sQueryTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sQueryTimeout;
                 
    if ( mStmtType == DKT_STMT_TYPE_REMOTE_TABLE_STORE )
    {
        sSendStmtType = DKT_STMT_TYPE_REMOTE_TABLE;
    }
    else
    {
        sSendStmtType = mStmtType;
    }

    IDE_TEST( dkpProtocolMgr::sendExecuteRemoteStmt( aSession,
                                                     aSessionId,
                                                     aIsTxBegin,
                                                     aXID,
                                                     aLinkObj,
                                                     mId,
                                                     sSendStmtType,
                                                     mStmtStr )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvExecuteRemoteStmtResult( aSession,
                                                           aSessionId,
                                                           mId,
                                                           &sResultCode,
                                                           &sRemoteSessionId,
                                                           &sStmtType,
                                                           &sResult,
                                                           sTimeoutSec )
              != IDE_SUCCESS );

    if ( ( mStmtType != DKT_STMT_TYPE_REMOTE_TABLE ) &&
         ( mStmtType != DKT_STMT_TYPE_REMOTE_TABLE_STORE ) )
    {
        mStmtType = sStmtType;
    }
    else
    {
        /* DKT_STMT_TYPE_REMOTE_TABLE / DKT_STMT_TYPE_REMOTE_TABLE_STORE */
    }

    mResult = sResultCode;

    switch ( sResultCode )
    {
        case DKP_RC_SUCCESS:
            break;
            
        case DKP_RC_FAILED_STMT_TYPE_ERROR:
            /* Anyway, write trace log */
            IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_STMT_TYPE ) );            
            break;     
           
        default:
            IDE_RAISE( ERR_RECEIVE_RESULT );
            break;
    }

    if ( mStmtType == DKT_STMT_TYPE_REMOTE_TABLE )
    {
        if ( aLinkObj != &mExecutedLinkObj )
        {
            dkoLinkObjMgr::copyLinkObject( aLinkObj, &mExecutedLinkObj );
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        /* do nothing */
    }

    *aResult = sResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 *
 */
IDE_RC dktRemoteStmt::addBatch( void )
{
    IDE_TEST( dkpBatchStatementMgrAddBatchStatement( mBatchStatementMgr, mId ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dktRemoteStmt::prepareAddBatch( dksSession *aSession,
                                       UInt aSessionId,
                                       ULong aTimeoutSec )
{
    UInt sResultCode = DKP_RC_SUCCESS;

    IDE_TEST( dkpProtocolMgr::sendPrepareAddBatch( aSession,
                                                   aSessionId,
                                                   mId,
                                                   mBatchStatementMgr )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvPrepareAddBatch( aSession,
                                                   aSessionId,
                                                   mId,
                                                   &sResultCode,
                                                   aTimeoutSec )
              != IDE_SUCCESS );

    mResult = sResultCode;

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dktRemoteStmt::addBatches( dksSession *aSession,
                                  UInt aSessionId,
                                  ULong aTimeoutSec )
{
    UInt sResultCode = DKP_RC_SUCCESS;
    UInt sProcessedBatchCount = 0;

    IDE_TEST( dkpProtocolMgr::sendAddBatches( aSession,
                                              aSessionId,
                                              mBatchStatementMgr )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvAddBatchResult( aSession,
                                                  aSessionId,
                                                  mId,
                                                  &sResultCode,
                                                  &sProcessedBatchCount,
                                                  aTimeoutSec )
              != IDE_SUCCESS );

    mResult = sResultCode;

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    if ( mBatchExecutionResult != NULL )
    {
        (void)iduMemMgr::free( mBatchExecutionResult );
        mBatchExecutionResult = NULL;
        mBatchExecutionResultSize = 0;
    }
    else
    {
        /* nothing to do */
    }
    
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       sProcessedBatchCount * ID_SIZEOF( SInt ),
                                       (void **)&mBatchExecutionResult,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC );
    mBatchExecutionResultSize = sProcessedBatchCount;    
    

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    if ( mBatchExecutionResult != NULL )
    {
        (void)iduMemMgr::free( mBatchExecutionResult );
        mBatchExecutionResult = NULL;
    }
    else
    {
        /* do nothing */
    }
    
    return IDE_FAILURE;    
}

/*
 *
 */
IDE_RC dktRemoteStmt::executeBatch( dksSession *aSession,
                                    UInt aSessionId,
                                    ULong aTimeoutSec )
{
    UInt sResultCode = DKP_RC_SUCCESS;
    
    IDE_TEST( dkpProtocolMgr::sendExecuteBatch( aSession,
                                                aSessionId,
                                                mId )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvExecuteBatch( aSession,
                                                aSessionId,
                                                mId,
                                                mBatchExecutionResult,
                                                &sResultCode,
                                                aTimeoutSec )
              != IDE_SUCCESS );

    /* TODO: Is need to check batch count ? */

    mResult = sResultCode;

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}

/*
 *
 */
IDE_RC dktRemoteStmt::executeBatchProtocol( dksSession    *aSession, 
                                            UInt           aSessionId, 
                                            SInt          *aResult )
{
    ULong           sTimeoutSec = 0;
    SInt            sReceiveTimeout = 0;
    SInt            sQueryTimeout = 0;
    
    IDE_DASSERT( mStmtType == DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH );
        
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerQueryTimeoutFromConf( &sQueryTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sQueryTimeout;

    if ( dkpBatchStatementMgrCountBatchStatement( mBatchStatementMgr ) > 0 )
    {
        IDE_TEST( prepareAddBatch( aSession, aSessionId, sTimeoutSec )
                  != IDE_SUCCESS );

        IDE_TEST( addBatches( aSession, aSessionId, sTimeoutSec )
                  != IDE_SUCCESS );

        IDE_TEST( dkpBatchStatementMgrClear( mBatchStatementMgr ) != IDE_SUCCESS );
        
        IDE_TEST( executeBatch( aSession, aSessionId, sTimeoutSec )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */        
        mResult = DKP_RC_SUCCESS;
    }
    
    *aResult = mResult;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote statement �� abort �ϱ�����  protocol operation 
 *               �� �����Ѵ�.
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::abortProtocol( dksSession    *aSession, 
                                      UInt           aSessionId )
{
    UInt            sResultCode = DKP_RC_SUCCESS;
    ULong           sTimeoutSec;
    SInt            sReceiveTimeout = 0;
    SInt            sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;
                  
    IDE_TEST( dkpProtocolMgr::sendAbortRemoteStmt( aSession, 
                                                   aSessionId, 
                                                   mId )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvAbortRemoteStmtResult( aSession, 
                                                         aSessionId, 
                                                         mId, 
                                                         &sResultCode, 
                                                         sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote statement �� binding �� �����ϱ� ���� bind 
 *               protocol operation �� �����Ѵ�. 
 *
 *  aSession     - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId   - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *  aBindVarIdx  - [IN] Bind �� ������ ��ġ�� ���� �ο��Ǵ� index
 *  aBindVarType - [IN] Bind �� ������ Ÿ��
 *  aBindVarLen  - [IN] Bind �� ������ ���� 
 *  aBindVar     - [IN] Bind �� ������
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::bindProtocol( dksSession    *aSession, 
                                     UInt           aSessionId, 
                                     UInt           aBindVarIdx, 
                                     UInt           aBindVarType, 
                                     UInt           aBindVarLen, 
                                     SChar         *aBindVar )
{
    UInt    sResultCode = DKP_RC_SUCCESS;
    UInt    sTimeoutSec;
    SInt    sReceiveTimeout = 0;
    SInt    sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dkpProtocolMgr::sendBindRemoteVariable( aSession, 
                                                      aSessionId,
                                                      mId,
                                                      aBindVarIdx,
                                                      aBindVarType,
                                                      aBindVarLen,
                                                      aBindVar )
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvBindRemoteVariableResult( aSession,
                                                            aSessionId,
                                                            mId,
                                                            &sResultCode,
                                                            sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * TODO: Remove wrapping function.
 */
IDE_RC dktRemoteStmt::bindBatch( UInt           /* aSessionId */,
                                 UInt           aBindVarIdx, 
                                 UInt           aBindVarType, 
                                 UInt           aBindVarLen, 
                                 SChar         *aBindVar )
{
    IDE_TEST( dkpBatchStatementMgrAddBindVariable( mBatchStatementMgr,
                                                   aBindVarIdx,
                                                   aBindVarType,
                                                   aBindVarLen,
                                                   aBindVar )
              != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC  dktRemoteStmt::bind( dksSession    *aSession, 
                             UInt           aSessionId, 
                             UInt           aBindVarIdx, 
                             UInt           aBindVarType, 
                             UInt           aBindVarLen, 
                             SChar         *aBindVar )
{
    switch ( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_NON_QUERY_STATEMENT:
            IDE_TEST( bindProtocol( aSession,
                                    aSessionId,
                                    aBindVarIdx, 
                                    aBindVarType, 
                                    aBindVarLen, 
                                    aBindVar )
                      != IDE_SUCCESS );
            break;
            
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH:
            IDE_TEST( bindBatch( aSessionId,
                                 aBindVarIdx, 
                                 aBindVarType, 
                                 aBindVarLen, 
                                 aBindVar )
                      != IDE_SUCCESS );            
            break;

        case DKT_STMT_TYPE_REMOTE_EXECUTE_IMMEDIATE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        default:
            IDE_RAISE( ERR_INVALID_REMOTE_STMT_TYPE );
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote query �� �������� fetch �ؿ´�.
 *
 *  aSession     - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                      �ִ� dksSession ����ü
 *  aSessionId   - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *  aQcStatement - [IN] insertRow �Լ��� �Ѱ��� ����
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::fetchProtocol( dksSession    *aSession, 
                                      UInt           aSessionId,
                                      void          *aQcStatement,
                                      idBool         aFetchAllRecord )
{
    idBool      sIsAllocated  = ID_FALSE;
    idBool      sIsEndOfFetch = ID_FALSE;
    UInt        i;
    UInt        sRowSize         = 0;
    UInt        sFetchRowSize    = 0;
    UInt        sFetchRowBufSize = 0;
    UInt        sResultCode      = DKP_RC_SUCCESS;
    UInt        sFetchRowCnt     = 0;
    UInt        sFetchedRowCnt   = 0;
    UInt        sRecvRowLen      = 0;
    UInt        sRecvRowCnt      = 0;
    UInt        sRecvRowBufferSize = 0;    
    SChar      *sFetchRowBuffer  = NULL;
    SChar      *sRecvRowBuffer   = NULL;
    SChar      *sRow             = NULL;
    ULong       sTimeoutSec;

    dkdTypeConverter *sConverter = NULL;
    mtcColumn        *sColMeta   = NULL;

    SInt    sReceiveTimeout = 0;
    SInt    sQueryTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerQueryTimeoutFromConf( &sQueryTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + ( 2 * sQueryTimeout );

    sConverter = mDataMgr->getTypeConverter();
    IDE_TEST_RAISE( sConverter == NULL, ERR_GET_TYPE_CONVETER_NULL );

    IDE_TEST( dkdTypeConverterGetConvertedMeta( sConverter, &sColMeta )
              != IDE_SUCCESS );

    /* calculate fetch row count */
    IDE_TEST( dkdTypeConverterGetRecordLength( sConverter, &sFetchRowSize )
              != IDE_SUCCESS );

    if ( sFetchRowSize != 0 )
    {
        /* 1. pre-setting */
        if ( sFetchRowSize > DKP_ADLP_PACKET_DATA_MAX_LEN )
        {
            sFetchRowBufSize = sFetchRowSize;
        }
        else
        {
            sFetchRowBufSize = DK_MAX_PACKET_LEN;
        }

        IDE_TEST( dkdMisc::getFetchRowCnt( sFetchRowSize,
                                           mBuffSize,
                                           &sFetchRowCnt,
                                           mStmtType )
                  != IDE_SUCCESS );

        /* 2. allocate temporary buffer */
        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                           sFetchRowBufSize,
                                           (void **)&sFetchRowBuffer,
                                           IDU_MEM_IMMEDIATE )
                      != IDE_SUCCESS, ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );

        sIsAllocated   = ID_TRUE;
        sRecvRowBuffer = sFetchRowBuffer;
        sRecvRowBufferSize = sFetchRowBufSize;

        /* 3. fetch all records from remote server */
        while ( sIsEndOfFetch != ID_TRUE )
        {
            sFetchedRowCnt = 0;

            IDE_TEST( dkpProtocolMgr::sendFetchResultSet( aSession,
                                                          aSessionId,
                                                          mId,
                                                          sFetchRowCnt,
                                                          mBuffSize ) 
                      != IDE_SUCCESS );

            while ( ( sFetchRowCnt > sFetchedRowCnt ) && 
                    ( sIsEndOfFetch != ID_TRUE ) )
            {
                /* BUG-37850 */
                if ( sResultCode == DKP_RC_SUCCESS_FRAGMENTED )
                {
                    sRecvRowCnt = DKP_FRAGMENTED_ROW;
                }
                else
                {
                    /* BUG-36837 */
                    sRecvRowCnt = 0;
                }

                IDE_TEST( dkpProtocolMgr::recvFetchResultRow( aSession,
                                                              aSessionId,
                                                              mId,
                                                              sRecvRowBuffer,
                                                              sRecvRowBufferSize,
                                                              &sRecvRowLen,
                                                              &sRecvRowCnt,
                                                              &sResultCode,
                                                              sTimeoutSec )
                          != IDE_SUCCESS );

                /* Analyze result code */
                switch ( sResultCode )
                {
                    case DKP_RC_SUCCESS:

                        sFetchedRowCnt += sRecvRowCnt;
                        sRecvRowBuffer = sFetchRowBuffer;
                        sRecvRowBufferSize = sFetchRowBufSize;

                        for ( i = 0; i < sRecvRowCnt; i++ )
                        {
                            /* get the size of a row */
                            idlOS::memcpy( &sRowSize, sRecvRowBuffer, ID_SIZEOF( sRowSize ) );

                            /* get the offset to read a row */
                            sRow = sRecvRowBuffer += ID_SIZEOF( sRowSize );
                            sRecvRowBufferSize -= ID_SIZEOF( sRowSize );
                            
                            /* insert row */
                            IDE_TEST( mDataMgr->insertRow( sRow, aQcStatement ) != IDE_SUCCESS );

                            /* move the offset to the starting point of the next row */
                            sRecvRowBuffer += sRowSize;
                            sRecvRowBufferSize -= sRowSize;

                            /* BUG-36837 */
                            sRowSize = 0;
                        }

                        /* clean temporary buffer */
                        idlOS::memset( sFetchRowBuffer, 0, sFetchRowBufSize );

                        sRecvRowBuffer = sFetchRowBuffer;
                        sRecvRowBufferSize = sFetchRowBufSize;

                        break;
                        
                    case DKP_RC_SUCCESS_FRAGMENTED:

                        sRecvRowBuffer += ID_SIZEOF( sRecvRowLen );
                        sRecvRowBufferSize -= ID_SIZEOF( sRecvRowLen );
                        sRecvRowBuffer += sRecvRowLen;
                        sRecvRowBufferSize -= sRecvRowLen;
                        break;

                    case DKP_RC_END_OF_FETCH:

                        sIsEndOfFetch = ID_TRUE;
                        mDataMgr->setEndOfFetch( sIsEndOfFetch );
                        break;

                    default:

                        IDE_RAISE( ERR_RECEIVE_RESULT );
                        break;
                }
            }
            if ( aFetchAllRecord == ID_FALSE )
            {
                break;
            }
            else
            {
                /* nothing to do */
            }
        }

        /* 4. free fetch row buffer */
        sIsAllocated = ID_FALSE;
        IDE_TEST( iduMemMgr::free( sFetchRowBuffer ) != IDE_SUCCESS );
    }
    else
    {
        /* row size is 0, success */
    }

    if ( aFetchAllRecord == ID_TRUE )
    {
        IDE_TEST( mDataMgr->restart() != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( mDataMgr->restartOnce() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION( ERR_MEMORY_ALLOC_FETCH_ROW_BUFFER );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION( ERR_GET_TYPE_CONVETER_NULL )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DK_INTERNAL_ERROR, 
                                  "[dktRemoteStmt::fetchProtocol] sConverter is NULL" ) ); 
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAllocated == ID_TRUE )
    {
        (void)iduMemMgr::free( sFetchRowBuffer );
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote query �� �������� �Ϻ� fetch �ؿ´�.
 *  
 *      @ Related Keyword : REMOTE_TABLE
 *
 *  aSession     - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                      �ִ� dksSession ����ü
 *  aSessionId   - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::executeRemoteFetch( dksSession    *aSession, 
                                           UInt           aSessionId )
{
    UInt                i;
    UInt                sRowSize            = 0;
    UInt                sResultCode         = 0;
    UInt                sFetchRowSize       = 0;
    UInt                sFetchRowCnt        = 0;
    UInt                sFetchedRowCnt      = 0;
    UInt                sFetchRowBufSize    = 0;
    UInt                sRecvRowLen         = 0;
    UInt                sRecvRowCnt         = 0;
    UInt                sRecvRowBufferSize  = 0;
    SChar              *sRecvRowBuffer      = NULL;
    SChar              *sFetchRowBuffer     = NULL;
    SChar              *sRow                = NULL;
    dkdRemoteTableMgr  *sRemoteTableMgr     = NULL;
    ULong               sTimeoutSec;
    SInt                sReceiveTimeout     = 0;
    SInt                sQueryTimeout       = 0;
    
    IDE_TEST_RAISE( mStmtType != DKT_STMT_TYPE_REMOTE_TABLE,
                    ERR_INVALID_STMT_TYPE );
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerQueryTimeoutFromConf( &sQueryTimeout )
              != IDE_SUCCESS );

    sTimeoutSec = sReceiveTimeout + ( 2 * sQueryTimeout );

    IDE_ASSERT( ( sRemoteTableMgr = getRemoteTableMgr() ) != NULL );

    sFetchRowSize = sRemoteTableMgr->getFetchRowSize();

    if ( ( sFetchRowSize > 0 ) && 
         ( sRemoteTableMgr->getEndOfFetch() != ID_TRUE ) )
    {
        sFetchRowBufSize = sRemoteTableMgr->getConvertedRowBufSize();
        sFetchRowBuffer  = sRemoteTableMgr->getConvertedRowBuffer();
        sFetchRowCnt     = sRemoteTableMgr->getFetchRowCnt();
    
        IDE_TEST( dkpProtocolMgr::sendFetchResultSet( aSession, 
                                                      aSessionId, 
                                                      mId, 
                                                      sFetchRowCnt,
                                                      mBuffSize ) 
                  != IDE_SUCCESS );

        sRecvRowBuffer = sFetchRowBuffer;
        sRecvRowBufferSize = sFetchRowBufSize;
        sFetchedRowCnt = 0;

        while ( ( sFetchRowCnt > sFetchedRowCnt ) && 
                ( sRemoteTableMgr->getEndOfFetch() != ID_TRUE ) )
        {   
            /* BUG-37850 */
            if ( sResultCode == DKP_RC_SUCCESS_FRAGMENTED )
            {
                sRecvRowCnt = DKP_FRAGMENTED_ROW;
            }
            else
            {
                sRecvRowCnt = 0;
            }
            
            IDE_TEST( dkpProtocolMgr::recvFetchResultRow( aSession, 
                                                          aSessionId, 
                                                          mId, 
                                                          sRecvRowBuffer, 
                                                          sRecvRowBufferSize,
                                                          &sRecvRowLen, 
                                                          &sRecvRowCnt, 
                                                          &sResultCode, 
                                                          sTimeoutSec ) 
                      != IDE_SUCCESS );

            switch ( sResultCode )
            {
                case DKP_RC_SUCCESS:

                    sFetchedRowCnt += sRecvRowCnt;
                    sRecvRowBuffer = sFetchRowBuffer;
                    sRecvRowBufferSize = sFetchRowBufSize;
                    for ( i = 0; i < sRecvRowCnt; i++ )
                    {
                        idlOS::memcpy( &sRowSize, sRecvRowBuffer, ID_SIZEOF( sRowSize ) );

                        sRecvRowBuffer += ID_SIZEOF( sRowSize );
                        sRecvRowBufferSize -= ID_SIZEOF( sRowSize );
                        
                        sRow = sRecvRowBuffer;

                        IDE_TEST( mRemoteTableMgr->insertRow( sRow ) != IDE_SUCCESS );

                        sRecvRowBuffer += sRowSize;
                        sRecvRowBufferSize -= sRowSize;
                        sRowSize = 0;
                    }

                    idlOS::memset( sFetchRowBuffer, 0, sFetchRowBufSize );

                    sRecvRowBuffer = sFetchRowBuffer;
                    sRecvRowBufferSize = sFetchRowBufSize;
                    break;

                case DKP_RC_SUCCESS_FRAGMENTED:

                    sRecvRowBuffer += ID_SIZEOF( sRecvRowLen );
                    sRecvRowBufferSize -= ID_SIZEOF( sRecvRowLen );
                    sRecvRowBuffer += sRecvRowLen;
                    sRecvRowBufferSize -= sRecvRowLen;
                    break;

                case DKP_RC_END_OF_FETCH:

                    sRemoteTableMgr->setEndOfFetch( ID_TRUE );
                    break;

                default:

                    IDE_RAISE( ERR_RECEIVE_RESULT );
                    break;
            }
        }

        sRemoteTableMgr->moveFirst();
    }
    else
    {
        sRemoteTableMgr->setEndOfFetch( ID_TRUE );
        sRemoteTableMgr->setCurRowNull();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote statement �� free ���ֱ� ���� ���������� protocol
 *               operation �� ȣ���Ͽ� AltiLinker ���μ����� �ڿ��� 
 *               �����ϵ��� �� ��, dkt ���� �Ҵ���� �ڿ��鵵 �������ش�.
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::freeProtocol( dksSession    *aSession, 
                                     UInt           aSessionId )
{
    UInt        sResultCode = DKP_RC_SUCCESS;
    UInt        sTimeoutSec;
    SInt        sReceiveTimeout = 0;
    SInt        sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dkpProtocolMgr::sendRequestFreeRemoteStmt( aSession, 
                                                         aSessionId, 
                                                         mId ) 
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestFreeRemoteStmtResult( aSession, 
                                                               aSessionId, 
                                                               mId, 
                                                               &sResultCode, 
                                                               sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */ 
IDE_RC  dktRemoteStmt::freeBatchProtocol( dksSession    *aSession, 
                                          UInt           aSessionId )
{
    UInt        sResultCode = DKP_RC_SUCCESS;
    UInt        sTimeoutSec = 0;
    SInt        sReceiveTimeout = 0;
    SInt        sRemoteNodeRecvTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );

    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerRemoteNodeReceiveTimeoutFromConf( &sRemoteNodeRecvTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout + sRemoteNodeRecvTimeout;

    IDE_TEST( dkpProtocolMgr::sendRequestFreeRemoteStmtBatch( aSession, 
                                                              aSessionId, 
                                                              mId ) 
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestFreeRemoteStmtBatchResult( aSession, 
                                                                    aSessionId, 
                                                                    mId, 
                                                                    &sResultCode, 
                                                                    sTimeoutSec )
              != IDE_SUCCESS );

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */ 
IDE_RC  dktRemoteStmt::free( dksSession    *aSession, 
                             UInt           aSessionId )
{
    switch ( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_IMMEDIATE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_NON_QUERY_STATEMENT:
            IDE_TEST( freeProtocol( aSession,
                                    aSessionId )
                      != IDE_SUCCESS );
            break;
            
        case DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH:
            IDE_TEST( freeBatchProtocol( aSession,
                                         aSessionId )
                      != IDE_SUCCESS );            
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dktRemoteStmt::getResultCountBatch( SInt *aResultCount )
{
    IDE_TEST_RAISE( mStmtType != DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH,
                    INVALID_STMT_TYPE );
    
    *aResultCount = mBatchExecutionResultSize;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_STMT_TYPE )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 *
 */
IDE_RC dktRemoteStmt::getResultBatch( SInt           aIndex,
                                      SInt          *aResult )
{
    IDE_TEST_RAISE( mStmtType != DKT_STMT_TYPE_REMOTE_EXECUTE_STATEMENT_BATCH,
                    INVALID_STMT_TYPE );

    IDE_TEST_RAISE( aIndex < 0, INVALID_INDEX_NUMBER );
    IDE_TEST_RAISE( aIndex > mBatchExecutionResultSize, INVALID_INDEX_NUMBER );    
    
    *aResult = mBatchExecutionResult[aIndex - 1];
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( INVALID_STMT_TYPE )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION( INVALID_INDEX_NUMBER )
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_COLUMN_INDEX ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/************************************************************************
 * Description : �� remote statement �� query ������ ���� ��ü�� ��� 
 *               ���ݼ����κ��� ���۹޴� data �� ���� �� �����ϱ� ���� 
 *               �ʿ��� data manager �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::createDataManager()
{
    idBool   sIsAllocated = ID_FALSE;

    destroyDataManager();
    
    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       ID_SIZEOF( dkdDataMgr ),
                                       (void **)&mDataMgr,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_DATA_MGR );

    sIsAllocated = ID_TRUE;

    IDE_TEST( mDataMgr->initialize( &mBuffSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_DATA_MGR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAllocated == ID_TRUE )
    {
        (void)iduMemMgr::free( mDataMgr );
        mDataMgr = NULL;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �� remote statement �� query ������ ���� ��ü�� ��� 
 *               ������ �Ҵ�Ǿ� ����� ���� data manager �� ������
 *               �ְ� �����Ѵ�.
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dktRemoteStmt::destroyDataManager()
{
    if ( mDataMgr != NULL )
    {
        mDataMgr->finalize();

        (void)iduMemMgr::free( mDataMgr );
        mDataMgr = NULL;
    }
    else
    {
        /* already destroyed */
    }
}

/************************************************************************
 * Description : ���ݼ����κ��� ���۹޴� ���ڵ带 �����ϱ� ���� 
 *               �ʿ��� remote table manager �� �����Ѵ�.
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::createRemoteTableManager()
{
    if ( mRemoteTableMgr != NULL )
    {
        (void)iduMemMgr::free( mRemoteTableMgr );
        mRemoteTableMgr = NULL;
    }
    else
    {
        /* no remote table manager */
    }

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK, 
                                       ID_SIZEOF( dkdRemoteTableMgr ), 
                                       (void **)&mRemoteTableMgr, 
                                       IDU_MEM_IMMEDIATE ) 
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_REMOTE_TABLE_MGR ); 

    IDE_TEST( mRemoteTableMgr->initialize( &mBuffSize ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_REMOTE_TABLE_MGR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( mRemoteTableMgr != NULL )
    {
        (void)iduMemMgr::free( mRemoteTableMgr );
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Remote statement type �� ���� remote data manager �� 
 *               Ȱ��ȭ ��Ų��. 
 *
 *  aQcStatement - [IN] Remote statement type �� REMOTE_TABLE_STORE �Ǵ� 
 *                      REMOTE_EXECUTE_QUERY_STATEMENT �� ��� 
 *                      data manager �� �Ѱ��� ����
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::activateRemoteDataManager( void  *aQcStatement )
{
    switch ( mStmtType )
    {
        case DKT_STMT_TYPE_REMOTE_TABLE:
#ifdef ALTIBASE_PRODUCT_XDB
            IDE_DASSERT( mDataMgr != NULL );
            IDE_TEST( mDataMgr->activate( aQcStatement ) != IDE_SUCCESS );
#else
            IDE_DASSERT( mRemoteTableMgr != NULL );
            IDE_TEST( mRemoteTableMgr->activate() != IDE_SUCCESS );
#endif
            break;

        case DKT_STMT_TYPE_REMOTE_TABLE_STORE:
        case DKT_STMT_TYPE_REMOTE_EXECUTE_QUERY_STATEMENT:
            IDE_DASSERT( mDataMgr != NULL );
            IDE_TEST( mDataMgr->activate( aQcStatement ) != IDE_SUCCESS );
            break;

        default:
            IDE_RAISE( ERR_INVALID_REMOTE_STMT_TYPE );
            break;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_INVALID_REMOTE_STMT_TYPE );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_DKT_INVALID_REMOTE_STMT_TYPE ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : �� remote statement �� query ������ ���� ��ü�� ��� 
 *               ������ �Ҵ�Ǿ� ����� ���� remote table manager �� 
 *               �������ְ� �����Ѵ�.
 *
 ************************************************************************/
void dktRemoteStmt::destroyRemoteTableManager()
{
    if ( mRemoteTableMgr != NULL )
    {
        mRemoteTableMgr->finalize();

        (void)iduMemMgr::free( mRemoteTableMgr );
        mRemoteTableMgr = NULL;
    }
    else
    {
        /* already destroyed */
    }
}

/************************************************************************
 * Description : �� remote statement �������� error �� ���, ��������
 *               �� ���� AltiLinker ���μ����κ��� ���ݼ����� error ��
 *               ���� �޾ƿ� �� �ִµ� �̷��� �޾ƿ� error ������ remote
 *               statement ��ü�� �����ϱ� ���� ����Ǵ� �Լ�
 *
 *  aSession    - [IN] AltiLinker process ���� ������ ���� ������ ���� 
 *                     �ִ� dksSession ����ü
 *  aSessionId  - [IN] �� protocol operation �� �����Ϸ��� ������ id
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::getErrorInfoFromProtocol( dksSession  *aSession,
                                                 UInt         aSessionId )
{
    UInt        sResultCode = DKP_RC_SUCCESS;
    ULong       sTimeoutSec;
    SInt        sReceiveTimeout = 0;
    
    IDE_TEST( dkaLinkerProcessMgr::getAltiLinkerReceiveTimeoutFromConf( &sReceiveTimeout )
              != IDE_SUCCESS );
    
    sTimeoutSec = sReceiveTimeout;

    IDE_TEST( dkpProtocolMgr::sendRequestRemoteErrorInfo( aSession,
                                                          aSessionId, 
                                                          mId ) 
              != IDE_SUCCESS );

    IDE_TEST( dkpProtocolMgr::recvRequestRemoteErrorInfoResult( aSession, 
                                                                aSessionId, 
                                                                &sResultCode, 
                                                                mId, 
                                                                &mErrorInfo, 
                                                                sTimeoutSec ) 
              != IDE_SUCCESS ); 

    IDE_TEST_RAISE( sResultCode != DKP_RC_SUCCESS, ERR_RECEIVE_RESULT );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_RECEIVE_RESULT );
    {
        dkpProtocolMgr::setResultErrorCode( sResultCode, 
                                            aSession,
                                            aSessionId,
                                            mId,
                                            &mErrorInfo );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Performance view �� ���� �� remote statement �� ������ 
 *               info ����ü�� ����ִ� �Լ�
 *
 *  aInfo       - [IN] Remote statement ������ ����� ����ü
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::getRemoteStmtInfo( dktRemoteStmtInfo *aInfo )
{
    aInfo->mGlobalTxId = mGlobalTxId;
    aInfo->mLocalTxId  = mLocalTxId;
    aInfo->mRTxId  = mRTxId;
    aInfo->mStmtId = mId;
    idlOS::strcpy( aInfo->mStmtStr, mStmtStr );

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : �Է¹��� ���ڿ��� �� remote statement ��ü�� copy �Ѵ�.
 *               ���������� �޸𸮸� �Ҵ��ϹǷ� �ٸ� ������ �޸� ������ 
 *               ���־�� �Ѵ�. �� remote statement ��ü�� ������ �� ����
 *               ���ش�.
 *
 *  aStmtStr    - [IN] �Է¹��� remote statement ����
 *
 ************************************************************************/
IDE_RC  dktRemoteStmt::setStmtStr( SChar   *aStmtStr )
{
    idBool      sIsAllocated = ID_FALSE;
    UInt        sStmtLen     = 0;

    sStmtLen = idlOS::strlen( aStmtStr );

    IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_DK,
                                       sStmtLen + 1,
                                       (void **)&mStmtStr,
                                       IDU_MEM_IMMEDIATE )
                    != IDE_SUCCESS, ERR_MEMORY_ALLOC_REMOTE_STMT_STR );

    sIsAllocated = ID_TRUE;

    idlOS::strcpy( aStmtStr, mStmtStr );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_MEMORY_ALLOC_REMOTE_STMT_STR );
    {
        IDE_SET( ideSetErrorCode( dkERR_ABORT_MEMORY_ALLOCATION ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsAllocated == ID_TRUE )
    {
        (void)iduMemMgr::free( mStmtStr );
    }
    else
    {
        /* do nothing */
    }

    IDE_POP();

    return IDE_FAILURE;
}
