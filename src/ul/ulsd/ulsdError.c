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
 

/***********************************************************************
 * $Id$
 **********************************************************************/

#include <uln.h>
#include <ulnPrivate.h>

#include <ulsd.h>
#include <sdErrorCodeClient.h>

/* TASK-7218 Multi-Error Handling 2nd */
static void ulsdMultiErrorCreate( ulnDiagHeader  *aHeader,
                                  ulnDiagRec    **aDiagRec )
{
    ulnDiagRec *sDiagRec = NULL;

    ulnDiagRecCreate( aHeader, &sDiagRec );

    sDiagRec->mMessageText = aHeader->mMultiErrorMessage;

    /* NodeId ���� ACP_UINT32_MAX�� �����Ͽ� Multi�������� ǥ���Ѵ� */
    ulnDiagRecSetNodeId( sDiagRec, ACP_UINT32_MAX );

    *aDiagRec = sDiagRec;
}

/*
 * TASK-7218 Multi-Error Handling 2nd
 *   1. Multi������ DiagRec ����
 *     Multi������ �ʿ��ϸ�(���� ������ 2 �̻��̸�),
 *     aObject�� DiagRecList�� �� �� DiagRec�� Multi�������� üũ�Ѵ�.
 *     Multi������ �ƴϸ�, DiagRec�� ������ �� List �� �տ� �����ִ´�.
 *
 *   2. Multi�����ڵ� �� ����
 *     ��� ���� �ڵ尡 �����ϸ�, �� �ڵ�� �����Ѵ�.
 *     �׷��� ������, sdERR_ABORT_SHARD_MULTIPLE_ERRORS�� �����Ѵ�.
 */
static void ulsdMultiErrorSetDiagRec( ulnObject    * aObject )
{
    ulnDiagRec   *sDiagRec4MultiError = NULL;
    ulnDiagRec   *sDiagRec            = NULL;
    acp_char_t   *sSQLSTATE;
    acp_uint32_t  sNativeErrorCode;

    ACI_TEST_CONT( aObject->mDiagHeader.mNumber <= 1,
                   NO_NEED_MULTIERROR );

    ulnGetDiagRecFromObject( aObject,
                             &sDiagRec,
                             1 );

    // 1.
    if ( ulsdIsMultipleError(sDiagRec) == ACP_FALSE )
    {
        ulsdMultiErrorCreate( &(aObject->mDiagHeader), &sDiagRec4MultiError );

        /* ulnDiagHeaderAddDiagRec���� list�� �� �տ� �����ֱ� ���� �ӽ� ���� */
        ulnDiagRecSetNativeErrorCode(
                sDiagRec4MultiError,
                sdERR_ABORT_SHARD_MULTIPLE_ERRORS );

        ulnDiagHeaderAddDiagRec( sDiagRec->mHeader, sDiagRec4MultiError );
    }
    else
    {
        /* Nothing to do */
        sDiagRec4MultiError = sDiagRec;
    }

    // 2.
    if ( aObject->mDiagHeader.mIsAllTheSame == ACP_TRUE )
    {
        sNativeErrorCode = sDiagRec->mNativeErrorCode;
        sSQLSTATE = sDiagRec->mSQLSTATE;
    }
    else
    {
        sNativeErrorCode = ulsdMultiErrorGetErrorCode();
        sSQLSTATE = ulsdMultiErrorGetSQLSTATE(); // instead of ulnErrorMgrGetSQLSTATE_Server
    }

    ulnDiagRecSetNativeErrorCode( sDiagRec4MultiError, sNativeErrorCode );
    ulnDiagRecSetSqlState( sDiagRec4MultiError, sSQLSTATE );

    ulnDiagRecSetRowNumber( sDiagRec4MultiError, SQL_NO_ROW_NUMBER );
    ulnDiagRecSetColumnNumber( sDiagRec4MultiError, SQL_NO_COLUMN_NUMBER );

    ACI_EXCEPTION_CONT( NO_NEED_MULTIERROR );

    return;
}

/*
 * TASK-7218 Multi-Error Handling 2nd
 *   1. mIsAllTheSame ����
 *   2. Multi-Error�� ���� �޽��� ����
 */
static void ulsdMultiErrorAppendMessage( ulnObject    * aObject,
                                         acp_uint32_t   aNativeErrorCode,
                                         acp_char_t   * aErrorMessage )
{
    ulnDiagRec      *sDiagRec;
    acp_sint32_t     sBufferSize;
    acp_size_t       sWrittenLen = 0;

    // 1.
    if ( aObject->mDiagHeader.mNumber == 1 ) // first input
    {
        aObject->mDiagHeader.mMultiErrorMessageLen = 0;
        aObject->mDiagHeader.mIsAllTheSame = ACP_TRUE;
    }
    else
    {
        /* ������ ���� �ڵ尡 ��� ������ ��쿡�� �˻��ϸ� �ȴ�.
         * �� �� ���� �ڵ带 �����ͼ� ���Ѵ�. */
        if ( aObject->mDiagHeader.mIsAllTheSame == ACP_TRUE )
        {
            ulnGetDiagRecFromObject( aObject,
                                     &sDiagRec,
                                     1 );

            if ( aNativeErrorCode != ulnDiagRecGetNativeErrorCode(sDiagRec) )
            {
                aObject->mDiagHeader.mIsAllTheSame = ACP_FALSE;
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
    }

    // 2.
    sBufferSize = ULSD_MAX_MULTI_ERROR_MSG_LEN -
                      aObject->mDiagHeader.mMultiErrorMessageLen;
    if ( sBufferSize > 1 )
    {
        acpSnprintfSize(aObject->mDiagHeader.mMultiErrorMessage +
                            aObject->mDiagHeader.mMultiErrorMessageLen,
                        sBufferSize,
                        &sWrittenLen,
                        (aObject->mDiagHeader.mNumber == 1)? "%s" : "\n%s",
                        aErrorMessage);
        aObject->mDiagHeader.mMultiErrorMessageLen += sWrittenLen;
    }
    else
    {
        /* Nothing to do */
    }
}

void ulsdMoveNodeDiagRec( ulnFnContext * aFnContext,
                          ulnObject    * aObjectTo,
                          ulnObject    * aObjectFrom,
                          acp_uint32_t   aNodeId,
                          acp_char_t   * aNodeString,
                          acp_char_t   * aOperation )
{
    ulnDiagRec      *sDiagRecFrom;
    ulnDiagRec      *sDiagRecTo;
    acp_char_t       sErrorMessage[ULSD_MAX_ERROR_MESSAGE_LEN];

    while ( ulnGetDiagRecFromObject( aObjectFrom,
                                     &sDiagRecFrom,
                                     1 )
            == ACI_SUCCESS )
    {
        ulnDiagRecCreate( &(aObjectTo->mDiagHeader), &sDiagRecTo );

        ulsdMakeErrorMessage( sErrorMessage,
                              ULSD_MAX_ERROR_MESSAGE_LEN,
                              sDiagRecFrom->mMessageText,
                              aNodeString,
                              aOperation );

        SHARD_LOG( "Error:(%s), Message:%s\n", aOperation, sErrorMessage );

        ulnDiagRecSetMessageText( sDiagRecTo, sErrorMessage );
        ulnDiagRecSetSqlState( sDiagRecTo, sDiagRecFrom->mSQLSTATE );
        ulnDiagRecSetNativeErrorCode( sDiagRecTo, sDiagRecFrom->mNativeErrorCode );
        ulnDiagRecSetNodeId( sDiagRecTo, aNodeId ); // TASK-7218
        ulnDiagRecSetRowNumber( sDiagRecTo, sDiagRecFrom->mRowNumber );
        ulnDiagRecSetColumnNumber( sDiagRecTo, sDiagRecFrom->mColumnNumber );

        ulnDiagHeaderAddDiagRec( sDiagRecTo->mHeader, sDiagRecTo );
        ULN_FNCONTEXT_SET_RC( aFnContext,
                              ulnErrorDecideSqlReturnCode( sDiagRecTo->mSQLSTATE ) );

        /* TASK-7218 */
        ulsdMultiErrorAppendMessage( aObjectTo,
                                     sDiagRecFrom->mNativeErrorCode,
                                     sErrorMessage );

        ulnDiagHeaderRemoveDiagRec( sDiagRecFrom->mHeader, sDiagRecFrom );
        (void)ulnDiagRecDestroy( sDiagRecFrom );
    }

    /* TASK-7218 */
    ulsdMultiErrorSetDiagRec( aObjectTo );

    (void)ulnClearDiagnosticInfoFromObject( aObjectFrom );
}

void ulsdNativeErrorToUlnError( ulnFnContext       * aFnContext,
                                acp_sint16_t         aHandleType,
                                ulnObject          * aErrorRaiseObject,
                                ulsdNodeInfo       * aNodeInfo,
                                acp_char_t         * aOperation)
{
    ulnFnContext           sFnContext;
    ulnObject             *sObject          = NULL;
    ulnDbc                *sDbc             = NULL;
    ulnFailoverServerInfo *sServerInfo      = NULL;
    const acp_char_t      *sNodeName        = NULL;
    const acp_char_t      *sServerIP        = NULL;
    acp_uint16_t           sServerPort      = 0;
    acp_char_t             sNodeString[ULSD_MAX_ERROR_MESSAGE_LEN];

    sObject = aErrorRaiseObject;

    ACI_TEST_RAISE( sObject == NULL, InvalidHandle );

    ULN_INIT_FUNCTION_CONTEXT( sFnContext, ULN_FID_NONE, sObject, aHandleType );

    ULN_FNCONTEXT_GET_DBC( &sFnContext, sDbc );

    ACI_TEST_RAISE( sDbc == NULL, InvalidHandle );
    ACI_TEST_RAISE( sDbc->mShardDbcCxt.mParentDbc == NULL, InvalidHandle );

    if ( aNodeInfo != NULL )
    {
        sNodeName = aNodeInfo->mNodeName;
    }
    else
    {
        sNodeName = "NODE_UNKNOWN";
    }

    sServerInfo = ulnDbcGetCurrentServer( sDbc );
    if ( sServerInfo != NULL )
    {
        sServerIP   = sServerInfo->mHost;
        sServerPort = sServerInfo->mPort;
    }
    else
    {
        /* sServerInfo is null -> there is only active server */

        if ( aNodeInfo != NULL )
        {
            sServerIP   = aNodeInfo->mServerIP;
            sServerPort = aNodeInfo->mPortNo;
        }
        else
        {
            sServerIP   = "UNKNOWN";
            sServerPort = 0;
        }
    }

    acpSnprintf( sNodeString,
                 ACI_SIZEOF(sNodeString),
                 "%s,%s:%"ACI_INT32_FMT,
                 sNodeName,
                 sServerIP,
                 sServerPort );

    ulsdMoveNodeDiagRec( aFnContext,
                         aFnContext->mHandle.mObj,
                         sObject,
                         aNodeInfo->mNodeId,
                         sNodeString,
                         aOperation );

    return;

    ACI_EXCEPTION( InvalidHandle );
    ACI_EXCEPTION_END;

    return;
}

void ulsdMakeErrorMessage( acp_char_t         * aOutputErrorMessage,
                           acp_uint16_t         aOutputErrorMessageLength,
                           acp_char_t         * aOriginalErrorMessage,
                           acp_char_t         * aNodeString,
                           acp_char_t         * aOperation )
{
    *aOutputErrorMessage = '\0';

    acpSnprintf( aOutputErrorMessage,
                 aOutputErrorMessageLength,
                 (acp_char_t *)"The %s of client-side sharding failed.: \"%s\" [%s]",
                 aOperation,
                 aOriginalErrorMessage,
                 aNodeString );
}

acp_bool_t ulsdNodeFailConnectionLost( acp_sint16_t   aHandleType,
                                       ulnObject    * aObject )
{
    acp_sint16_t        sRecNumber;
    acp_char_t          sSqlstate[SQL_SQLSTATE_SIZE + 1];
    acp_sint32_t        sNativeError;
    acp_char_t          sMessage[ULSD_MAX_ERROR_MESSAGE_LEN];
    acp_sint16_t        sMessageLength;

    acp_bool_t          sIsConnectionLost = ACP_FALSE;

    sRecNumber = 1;

    while ( ulnGetDiagRec(aHandleType,
                          aObject,
                          sRecNumber,
                          sSqlstate,
                          &sNativeError,
                          sMessage,
                          ACI_SIZEOF(sMessage),
                          &sMessageLength,
                          ACP_FALSE) != SQL_NO_DATA )
    {
        if ( ( sNativeError == ALTIBASE_FAILOVER_SUCCESS ) ||
             ( sNativeError == ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE ) )
        {
            sIsConnectionLost = ACP_TRUE;
            break;
        }
        else
        {
            /* Do Nothing */
        }

        sRecNumber++;
    }

    return sIsConnectionLost;
}

acp_bool_t ulsdNodeInvalidTouch( acp_sint16_t   aHandleType,
                                 ulnObject    * aObject )
{
    acp_sint16_t        sRecNumber;
    acp_char_t          sSqlstate[6];
    acp_sint32_t        sNativeError;
    acp_char_t          sMessage[ULSD_MAX_ERROR_MESSAGE_LEN];
    acp_sint16_t        sMessageLength;

    acp_bool_t          sShardNodeInvalidTouchError = ACP_FALSE;

    sRecNumber = 1;

    while ( ulnGetDiagRec(aHandleType,
                          aObject,
                          sRecNumber,
                          sSqlstate,
                          &sNativeError,
                          sMessage,
                          sizeof(sMessage),
                          &sMessageLength,
                          ACP_FALSE) != SQL_NO_DATA )
    {
        if ( sNativeError == ALTIBASE_SHARD_NODE_INVALID_TOUCH )
        {
            sShardNodeInvalidTouchError = ACP_TRUE;
            break;
        }
        else
        {
            /* Do Nothing */
        }

        sRecNumber++;
    }

    return sShardNodeInvalidTouchError;
}

static acp_bool_t ulsdIsFailoverErrorCode( acp_uint32_t aNativeErrorCode )
{
    acp_bool_t sIsFailoverErrorCode = ACP_FALSE;

    switch ( aNativeErrorCode )
    {
        case ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_LIBRARY_LINK_FAILURE_ERROR ) :
        case ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_LIBRARY_FAILOVER_SUCCESS )   :
        case ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE ) :
            sIsFailoverErrorCode = ACP_TRUE;
            break;

        default:
            /* Do nothing */
            break;
    }

    return sIsFailoverErrorCode;
}

static acp_bool_t ulsdProcessShardingError( ulnFnContext * aFnContext,
                                            ulnDiagRec   * aDiagRec,
                                            acp_uint32_t   aNativeErrorCode,
                                            acp_uint32_t   aNodeId )
{
    ulnDbc                * sMetaDbc           = NULL;
    ulnDbc                * sNodeDbc           = NULL;
    ulsdDbc               * sShard             = NULL;
    ulsdAlignInfo         * sAlignInfo         = NULL;
    acp_sint32_t            sIdx;

    ULN_FNCONTEXT_GET_DBC( aFnContext, sMetaDbc );

    ACI_TEST_RAISE( sMetaDbc == NULL, InvalidHandleException );

    sShard = sMetaDbc->mShardDbcCxt.mShardDbc;

    if ( aNativeErrorCode == ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_LIBRARY_LINK_FAILURE_ERROR ) )
    {
        for ( sIdx = 0; sIdx < sShard->mNodeCount; ++sIdx )
        {
            if ( sShard->mNodeInfo[sIdx]->mNodeId == aNodeId )
            {
                sNodeDbc = sShard->mNodeInfo[sIdx]->mNodeDbc;
                break;
            }
        }
        ACI_TEST_RAISE( sNodeDbc == NULL, NOT_FOUND_DATA_NODE );
        ACI_TEST_RAISE( ulnFailoverIsOn( sNodeDbc ) == ACP_FALSE, NOT_SUPPORTED_DATA_NODE_ALIGN );

        sAlignInfo = &sNodeDbc->mShardDbcCxt.mAlignInfo;

        ACI_TEST( ulsdReallocAlignInfo( aFnContext,
                                        sAlignInfo,
                                        acpCStrLen( aDiagRec->mMessageText,
                                                    ULSD_ALIGN_INFO_MAX_TEXT_LENGTH ) + 1 )
                  != ACP_RC_SUCCESS );

        acpSnprintf( sAlignInfo->mSQLSTATE, SQL_SQLSTATE_SIZE + 1, aDiagRec->mSQLSTATE );
        acpSnprintf( sAlignInfo->mMessageText,
                     sAlignInfo->mMessageTextAllocLength,
                     "%s",
                     aDiagRec->mMessageText );
        sAlignInfo->mNativeErrorCode = aNativeErrorCode;
        sAlignInfo->mIsNeedAlign     = ACP_TRUE;

        /* rest of data node align is will proceed at ulsdModuleErrorCheckAndAlignDataNode() */
    }
    else if ( aNativeErrorCode == ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_LIBRARY_FAILOVER_SUCCESS ) )
    {
        /* server reconnect to data node by sdl::retryConnect */
        (void)ulnError( aFnContext,
                        ulERR_ABORT_FAILOVER_SUCCESS,
                        aNativeErrorCode,
                        aDiagRec->mSQLSTATE,
                        aDiagRec->mMessageText );
    }
    else if ( aNativeErrorCode == ACI_E_ERROR_CODE( sdERR_ABORT_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE ) )
    {
        (void)ulnError( aFnContext, ulERR_ABORT_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE );
    }
    else
    {
        ACI_RAISE( NOT_SURPPORT );
    }

    ULN_TRACE_LOG( aFnContext, ULN_TRACELOG_LOW, NULL, 0,
                   "%-18s|", "ulsdProcessShardingError" );

    return ACP_TRUE;

    ACI_EXCEPTION( InvalidHandleException )
    {
        ULN_TRACE_LOG( aFnContext, ULN_TRACELOG_LOW, NULL, 0,
                       "%-18s| Meta Dbc is invalid handle.",
                       "ulsdProcessShardingError" );
    }
    ACI_EXCEPTION( NOT_FOUND_DATA_NODE )
    {
        ULN_TRACE_LOG( aFnContext, ULN_TRACELOG_LOW, NULL, 0,
                       "%-18s| Data node ID %"ACI_INT32_FMT" not found.",
                       "ulsdProcessShardingError",
                       aNodeId );
    }
    ACI_EXCEPTION( NOT_SURPPORT );
    {
        /* Internal error.
         * ulnCallbackErrorResult �Լ����� �̹� �߰��� diag record �� �����Ƿ�
         * ���⼭ diag record �� �߰����� �ʾƵ� �����ϴ�.
         */
    }
    ACI_EXCEPTION( NOT_SUPPORTED_DATA_NODE_ALIGN );
    {
        (void)ulnError( aFnContext, ulERR_ABORT_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE );
    }
    ACI_EXCEPTION_END;

    return ACP_FALSE;
}

void ulsdErrorHandleShardingError( ulnFnContext * aFnContext,
                                   acp_uint32_t   aNodeId )
{
    ulnDbc              * sMetaDbc       = NULL;
    ulnDiagRec          * sDiagRec         = NULL;
    acp_uint32_t          sNativeErrorCode = 0;

    ACI_TEST( aFnContext->mHandle.mObj == NULL );

    ULN_FNCONTEXT_GET_DBC( aFnContext, sMetaDbc );

    ACI_TEST( sMetaDbc == NULL );

    ACI_TEST( sMetaDbc->mShardDbcCxt.mParentDbc != NULL ); /* Meta dbc has not mParentDbc */

    /* �������� �߰��� DiagRec�� ������ */
    ACI_TEST( ulnGetDiagRecFromObject( aFnContext->mHandle.mObj,
                                       &sDiagRec,
                                       aFnContext->mHandle.mObj->mDiagHeader.mNumber )
              != ACI_SUCCESS );

    ulnDiagRecSetNodeId(sDiagRec, aNodeId);

    ACI_TEST( ULSD_IS_MULTIPLE_ERROR(aNodeId) == ACP_TRUE );

    sNativeErrorCode = ulnDiagRecGetNativeErrorCode(sDiagRec);

    if ( ulsdIsFailoverErrorCode( sNativeErrorCode ) == ACP_TRUE )
    {
        (void)ulsdProcessShardingError( aFnContext,
                                        sDiagRec,
                                        sNativeErrorCode,
                                        aNodeId );
    }

    return;

    ACI_EXCEPTION_END;

    return;
}

void ulsdErrorCheckAndAlignDataNode( ulnFnContext * aFnContext )
{
    ulnDbc                * sMetaDbc           = NULL;
    ulnDbc                * sNodeDbc           = NULL;
    ulsdDbc               * sShard             = NULL;
    ulsdAlignInfo         * sAlignInfo         = NULL;
    acp_sint32_t            sIdx;

    ULN_FNCONTEXT_GET_DBC( aFnContext, sMetaDbc );

    if ( sMetaDbc != NULL )
    {
        sShard = sMetaDbc->mShardDbcCxt.mShardDbc;

        for ( sIdx = 0; sIdx < sShard->mNodeCount; ++sIdx )
        {
            sNodeDbc = sShard->mNodeInfo[sIdx]->mNodeDbc;
            sAlignInfo = &sNodeDbc->mShardDbcCxt.mAlignInfo;

            if ( sAlignInfo->mIsNeedAlign == ACP_FALSE )
            {
                continue;
            }

            ulsdModuleAlignDataNodeConnection( aFnContext, sNodeDbc );
        }
    }
}

/* TASK-7218 Multi-Error Handling 2nd */
ACI_RC ulsdMultiErrorAdd( ulnFnContext *aFnContext,
                          ulnDiagRec   *aDiagRec )
{
    ulnDbc     *sDbc      = NULL;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    ACI_TEST_CONT( sDbc == NULL, SKIP_MULTIERROR );
    ACI_TEST_CONT( ULSD_IS_SHARD_LIB_SESSION(sDbc) == ACP_TRUE,
                   SKIP_MULTIERROR );

    ulsdMultiErrorAppendMessage( aFnContext->mHandle.mObj,
                                 aDiagRec->mNativeErrorCode,
                                 aDiagRec->mMessageText );

    ulsdMultiErrorSetDiagRec( aFnContext->mHandle.mObj );

    ACI_EXCEPTION_CONT( SKIP_MULTIERROR );

    return ACI_SUCCESS;
}
