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

#include <uln.h>
#include <ulnPrivate.h>

#include <ulsdFailover.h>
#include <ulsdCommunication.h>
#include <ulsdnExecute.h>
#include <ulsdnLob.h>
#include <ulsdLob.h>

/*
 * PROJ-2739 Client-side Sharding LOB
 */

SQLRETURN ulsdGetLobLength( ulnStmt      *aStmt,
                            acp_uint64_t  aLocator,
                            acp_sint16_t  aLocatorType,
                            acp_uint32_t *aLengthPtr )
{
    SQLRETURN      sRet = SQL_ERROR;
    ulnFnContext   sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_GETLOBLENGTH, aStmt, SQL_HANDLE_STMT);

    ACI_TEST( ulsdEnter( &sFnContext ) != ACI_SUCCESS );

    sRet = ulsdModuleGetLobLength( &sFnContext,
                                   aStmt,
                                   aLocator,
                                   aLocatorType,
                                   aLengthPtr );

    return sRet;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC( &sFnContext );
}

SQLRETURN ulsdGetLob( ulnStmt      *aStmt,
                      acp_sint16_t  aLocatorCType,
                      acp_uint64_t  aSrcLocator,
                      acp_uint32_t  aFromPosition,
                      acp_uint32_t  aForLength,
                      acp_sint16_t  aTargetCType,
                      void         *aBuffer,
                      acp_uint32_t  aBufferSize,
                      acp_uint32_t *aLengthWritten )
{
    SQLRETURN      sRet = SQL_ERROR;
    ulnFnContext   sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_GETLOB, aStmt, SQL_HANDLE_STMT);

    ACI_TEST( ulsdEnter( &sFnContext ) != ACI_SUCCESS );

    sRet = ulsdModuleGetLob( &sFnContext,
                             aStmt,
                             aLocatorCType,
                             aSrcLocator,
                             aFromPosition,
                             aForLength,
                             aTargetCType,
                             aBuffer,
                             aBufferSize,
                             aLengthWritten );

    return sRet;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC( &sFnContext );
}

SQLRETURN ulsdPutLob( ulnStmt      *aStmt,
                      acp_sint16_t  aLocatorCType,
                      acp_uint64_t  aLocator,
                      acp_uint32_t  aFromPosition,
                      acp_uint32_t  aForLength,
                      acp_sint16_t  aSourceCType,
                      void         *aBuffer,
                      acp_uint32_t  aBufferSize )
{
    SQLRETURN      sRet = SQL_ERROR;
    ulnFnContext   sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_PUTLOB, aStmt, SQL_HANDLE_STMT);

    ACI_TEST( ulsdEnter( &sFnContext ) != ACI_SUCCESS );

    sRet = ulsdModulePutLob( &sFnContext,
                             aStmt,
                             aLocatorCType,
                             aLocator,
                             aFromPosition,
                             aForLength,
                             aSourceCType,
                             aBuffer,
                             aBufferSize );

    return sRet;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC( &sFnContext );
}

SQLRETURN ulsdFreeLob( ulnStmt      *aStmt,
                       acp_uint64_t  aLocator )
{
    SQLRETURN      sRet = SQL_ERROR;
    ulnFnContext   sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_FREELOB, aStmt, SQL_HANDLE_STMT);

    ACI_TEST( ulsdEnter( &sFnContext ) != ACI_SUCCESS );

    sRet = ulsdModuleFreeLob( &sFnContext,
                              aStmt,
                              aLocator );

    return sRet;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC( &sFnContext );
}

SQLRETURN ulsdTrimLob( ulnStmt       *aStmt,
                       acp_sint16_t   aLocatorCType,
                       acp_uint64_t   aLocator,
                       acp_uint32_t   aStartOffset )
{
    SQLRETURN      sRet = SQL_ERROR;
    ulnFnContext   sFnContext;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext, ULN_FID_TRIMLOB, aStmt, SQL_HANDLE_STMT);

    ACI_TEST( ulsdEnter( &sFnContext ) != ACI_SUCCESS );

    sRet = ulsdModuleTrimLob( &sFnContext,
                              aStmt,
                              aLocatorCType,
                              aLocator,
                              aStartOffset );

    return sRet;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC( &sFnContext );
}

SQLRETURN ulsdGetLobLengthNodes( ulnFnContext * aFnContext,
                                 ulnStmt      * aStmt,
                                 acp_uint64_t   aLocator,
                                 acp_sint16_t   aLocatorType,
                                 acp_uint32_t * aLengthPtr )
{
    SQLRETURN          sNodeResult   = SQL_ERROR;
    ulsdDbc           *sShard;
    ulnStmt           *sNodeStmt     = NULL;
    ulsdLobLocator    *sShardLocator = NULL;
    acp_uint16_t       sNodeDbcIndex;
    acp_uint16_t       sIsNull;
             
    ACI_TEST_RAISE( aLengthPtr == NULL, LABEL_INVALID_NULL );

    ulsdGetShardFromDbc(aStmt->mParentDbc, &sShard);

    sShardLocator = (ulsdLobLocator *)aLocator;

    ACI_TEST_RAISE( sShardLocator == NULL, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mArraySize != 1, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mLobLocator == 0, LABEL_INVALID_LOCATOR );

    sNodeDbcIndex = sShardLocator->mNodeDbcIndex;
    sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

    SHARD_LOG("(GetLobLength) NodeId=%d, Server=%s:%d, StmtID=%d\n",
              sShard->mNodeInfo[sNodeDbcIndex]->mNodeId,
              sShard->mNodeInfo[sNodeDbcIndex]->mServerIP,
              sShard->mNodeInfo[sNodeDbcIndex]->mPortNo,
              sNodeStmt->mStatementID);

    sNodeResult = ulnGetLobLength( SQL_HANDLE_STMT,
                                   (ulnObject *)sNodeStmt,
                                   sShardLocator->mLobLocator,
                                   aLocatorType,
                                   aLengthPtr,
                                   &sIsNull );

    ACI_TEST_RAISE( sNodeResult != SQL_SUCCESS, LABEL_NODE_GETLOBLENGTH_FAIL );

    return sNodeResult;

    ACI_EXCEPTION( LABEL_INVALID_NULL )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER );
    }
    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
    }
    ACI_EXCEPTION( LABEL_NODE_GETLOBLENGTH_FAIL )
    {
        ulsdNativeErrorToUlnError(aFnContext,
                                  SQL_HANDLE_STMT,
                                  (ulnObject *)sNodeStmt,
                                  sShard->mNodeInfo[sNodeDbcIndex],
                                  (acp_char_t *)"GetLobLength");
    }
    ACI_EXCEPTION_END;

    return sNodeResult;
}

SQLRETURN ulsdGetLobNodes( ulnFnContext * aFnContext,
                           ulnStmt      * aStmt,
                           acp_sint16_t   aLocatorCType,
                           acp_uint64_t   aSrcLocator,
                           acp_uint32_t   aFromPosition,
                           acp_uint32_t   aForLength,
                           acp_sint16_t   aTargetCType,
                           void         * aBuffer,
                           acp_uint32_t   aBufferSize,
                           acp_uint32_t * aLengthWritten )
{
    SQLRETURN          sNodeResult   = SQL_ERROR;
    ulsdDbc           *sShard;
    ulnStmt           *sNodeStmt     = NULL;
    ulsdLobLocator    *sShardLocator = NULL;
    acp_uint16_t       sNodeDbcIndex;
             
    ulsdGetShardFromDbc(aStmt->mParentDbc, &sShard);

    sShardLocator = (ulsdLobLocator *)aSrcLocator;

    ACI_TEST_RAISE( sShardLocator == NULL, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mArraySize != 1, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mLobLocator == 0, LABEL_INVALID_LOCATOR );

    sNodeDbcIndex = sShardLocator->mNodeDbcIndex;
    sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

    SHARD_LOG("(GetLob) NodeId=%d, Server=%s:%d, StmtID=%d\n",
              sShard->mNodeInfo[sNodeDbcIndex]->mNodeId,
              sShard->mNodeInfo[sNodeDbcIndex]->mServerIP,
              sShard->mNodeInfo[sNodeDbcIndex]->mPortNo,
              sNodeStmt->mStatementID);

    sNodeResult = ulnGetLob( SQL_HANDLE_STMT,
                             (ulnObject *)sNodeStmt,
                             aLocatorCType,
                             sShardLocator->mLobLocator,
                             aFromPosition,
                             aForLength,
                             aTargetCType,
                             aBuffer,
                             aBufferSize,
                             aLengthWritten );

    ACI_TEST_RAISE( sNodeResult != SQL_SUCCESS, LABEL_NODE_GETLOB_FAIL );

    return sNodeResult;

    ACI_EXCEPTION( LABEL_NODE_GETLOB_FAIL )
    {
        ulsdNativeErrorToUlnError(aFnContext,
                                  SQL_HANDLE_STMT,
                                  (ulnObject *)sNodeStmt,
                                  sShard->mNodeInfo[sNodeDbcIndex],
                                  (acp_char_t *)"GetLob");
    }
    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
    }
    ACI_EXCEPTION_END;

    return sNodeResult;
}

// using callback
SQLRETURN ulsdPutLobNodes( ulnFnContext *aFnContext,
                           ulnStmt      *aStmt,
                           acp_sint16_t  aLocatorCType,
                           acp_uint64_t  aLocator,
                           acp_uint32_t  aFromPosition,
                           acp_uint32_t  aForLength,
                           acp_sint16_t  aSourceCType,
                           void         *aBuffer,
                           acp_uint32_t  aBufferSize )
{
    SQLRETURN          sNodeResult    = SQL_ERROR;
    SQLRETURN          sSuccessResult = SQL_SUCCESS;
    SQLRETURN          sErrorResult   = SQL_ERROR;
    acp_bool_t         sSuccess       = ACP_TRUE;
    ulsdFuncCallback  *sCallback      = NULL;
    ulsdDbc           *sShard         = NULL;
    ulnStmt           *sNodeStmt      = NULL;
    ulsdLobLocator    *sShardLocator  = NULL;
    acp_uint16_t       sNodeDbcIndex;
    acp_uint16_t       i;
             
    ulsdGetShardFromDbc(aStmt->mParentDbc, &sShard);

    sShardLocator = (ulsdLobLocator *)aLocator;

    ACI_TEST_RAISE( sShardLocator == NULL, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mArraySize == 0, LABEL_INVALID_LOCATOR );

    for ( i = 0; i < sShardLocator->mArraySize; i++ )
    {
        if ( sShardLocator[i].mLobLocator == 0 )
        {
            continue;
        }

        sNodeDbcIndex = sShardLocator[i].mNodeDbcIndex;
        sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

        sErrorResult = ulsdPutLobAddCallback( sNodeDbcIndex,
                                              sNodeStmt,
                                              aLocatorCType,
                                              sShardLocator[i].mLobLocator,
                                              aFromPosition,
                                              aForLength,
                                              aSourceCType,
                                              aBuffer,
                                              aBufferSize, 
                                              &sCallback );
        ACI_TEST( sErrorResult != SQL_SUCCESS );
    }

    /* node execute ���ļ��� */
    ulsdDoCallback( sCallback );

    for ( i = 0; i < sShardLocator->mArraySize; i++ )
    {
        if ( sShardLocator[i].mLobLocator == 0 )
        {
            continue;
        }

        sNodeDbcIndex = sShardLocator[i].mNodeDbcIndex;
        sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

        sNodeResult = ulsdGetResultCallback( sNodeDbcIndex, sCallback, (acp_uint8_t)0 );

        SHARD_LOG("(PutLob) NodeId=%d, Server=%s:%d, StmtID=%d\n",
                  sShard->mNodeInfo[sNodeDbcIndex]->mNodeId,
                  sShard->mNodeInfo[sNodeDbcIndex]->mServerIP,
                  sShard->mNodeInfo[sNodeDbcIndex]->mPortNo,
                  sNodeStmt->mStatementID);

        if ( sNodeResult == SQL_SUCCESS )
        {
            /* Do Nothing */
        }
        else if ( sNodeResult == SQL_SUCCESS_WITH_INFO )
        {
            /* info�� ���� */
            ulsdNativeErrorToUlnError(aFnContext,
                                      SQL_HANDLE_STMT,
                                      (ulnObject *)sNodeStmt,
                                      sShard->mNodeInfo[sNodeDbcIndex],
                                      (acp_char_t *)"PutLob");

            sSuccessResult = sNodeResult;
        }
        else
        {
            /* BUGBUG �������� ������ ��忡 ���ؼ� retry�� �����Ѱ�? */
            if ( ulsdNodeFailConnectionLost( SQL_HANDLE_STMT,
                                             (ulnObject *)sNodeStmt ) == ACP_TRUE )
            {
                if ( aStmt->mParentDbc->mAttrAutoCommit == SQL_AUTOCOMMIT_OFF )
                {
                    ulsdClearOnTransactionNode(aStmt->mParentDbc);
                }
                else
                {
                    /* Do Nothing */
                }
            }

            ulsdNativeErrorToUlnError(aFnContext,
                                      SQL_HANDLE_STMT,
                                      (ulnObject *)sNodeStmt,
                                      sShard->mNodeInfo[sNodeDbcIndex],
                                      (acp_char_t *)"PutLob");

            sErrorResult = sNodeResult;
            sSuccess = ACP_FALSE;
        }
    }

    ACI_TEST_RAISE( sSuccess == ACP_FALSE, ERR_EXECUTE_FAIL );

    ulsdRemoveCallback( sCallback );

    return sSuccessResult;

    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
    }
    ACI_EXCEPTION( ERR_EXECUTE_FAIL )
    {
        ulsdProcessExecuteError( aFnContext, aStmt, sCallback );
    }
    ACI_EXCEPTION_END;

    ulsdRemoveCallback( sCallback );

    return sErrorResult;
}

SQLRETURN ulsdFreeLobNodes( ulnFnContext * aFnContext,
                            ulnStmt      * aStmt,
                            acp_uint64_t   aLocator )
{
    SQLRETURN          sNodeResult    = SQL_ERROR;
    SQLRETURN          sSuccessResult = SQL_SUCCESS;
    SQLRETURN          sErrorResult   = SQL_ERROR;
    acp_bool_t         sSuccess       = ACP_TRUE;
    ulsdDbc           *sShard;
    ulnStmt           *sNodeStmt      = NULL;
    ulsdLobLocator    *sShardLocator  = NULL;
    acp_uint16_t       sNodeDbcIndex;
    acp_uint16_t       i;
             
    ulsdGetShardFromDbc(aStmt->mParentDbc, &sShard);

    sShardLocator = (ulsdLobLocator *)aLocator;

    /* �̹� free �� ���̾ success ��ȯ */
    ACI_TEST_CONT( sShardLocator == NULL, NORMAL_EXIT );
    ACI_TEST_CONT( sShardLocator->mArraySize == 0, NORMAL_EXIT );

    for ( i = 0; i < sShardLocator->mArraySize; i++ )
    {
        if ( sShardLocator[i].mLobLocator == 0 )
        {
            continue;
        }

        sNodeDbcIndex = sShardLocator[i].mNodeDbcIndex;
        sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

        SHARD_LOG("(FreeLob) NodeId=%d, Server=%s:%d, StmtID=%d\n",
                  sShard->mNodeInfo[sNodeDbcIndex]->mNodeId,
                  sShard->mNodeInfo[sNodeDbcIndex]->mServerIP,
                  sShard->mNodeInfo[sNodeDbcIndex]->mPortNo,
                  sNodeStmt->mStatementID);

        sNodeResult = ulnFreeLob( SQL_HANDLE_STMT,
                                  (ulnObject *)sNodeStmt,
                                  sShardLocator[i].mLobLocator);
        if ( sNodeResult == SQL_SUCCESS )
        {
            /* Nothing to do */
        }
        else
        {
            ulsdNativeErrorToUlnError(aFnContext,
                                      SQL_HANDLE_STMT,
                                      (ulnObject *)sNodeStmt,
                                      sShard->mNodeInfo[sNodeDbcIndex],
                                      (acp_char_t *)"FreeLob");

            sErrorResult = sNodeResult;
            sSuccess = ACP_FALSE;
        }

        sShardLocator[i].mLobLocator = 0;
    }

    sShardLocator->mArraySize = 0;

    ulsdLobLocatorDestroy( aStmt->mParentDbc,
                           sShardLocator,
                           ACP_TRUE /* NeedLock */);

    ACI_TEST( sSuccess == ACP_FALSE );

    ACI_EXCEPTION_CONT(NORMAL_EXIT);

    return sSuccessResult;

    ACI_EXCEPTION_END;

    return sErrorResult;
}

SQLRETURN ulsdTrimLobNodes( ulnFnContext * aFnContext,
                            ulnStmt      * aStmt,
                            acp_sint16_t   aLocatorCType,
                            acp_uint64_t   aLocator,
                            acp_uint32_t   aStartOffset )
{
    SQLRETURN          sNodeResult    = SQL_ERROR;
    SQLRETURN          sSuccessResult = SQL_SUCCESS;
    SQLRETURN          sErrorResult   = SQL_ERROR;
    acp_bool_t         sSuccess       = ACP_TRUE;
    ulsdDbc           *sShard;
    ulnStmt           *sNodeStmt      = NULL;
    ulsdLobLocator    *sShardLocator  = NULL;
    acp_uint16_t       sNodeDbcIndex;
    //acp_uint16_t       i;
             
    ulsdGetShardFromDbc(aStmt->mParentDbc, &sShard);

    sShardLocator = (ulsdLobLocator *)aLocator;

    ACI_TEST_RAISE( sShardLocator == NULL, LABEL_INVALID_LOCATOR );
    /* select-fetch�� locator�� ���ؼ��� trim�� �����ϴ� */
    ACI_TEST_RAISE( sShardLocator->mArraySize != 1, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sShardLocator->mLobLocator == 0, LABEL_INVALID_LOCATOR );

    sNodeDbcIndex = sShardLocator->mNodeDbcIndex;
    sNodeStmt = aStmt->mShardStmtCxt.mShardNodeStmt[sNodeDbcIndex];

    SHARD_LOG("(TrimLob) NodeId=%d, Server=%s:%d, StmtID=%d\n",
              sShard->mNodeInfo[sNodeDbcIndex]->mNodeId,
              sShard->mNodeInfo[sNodeDbcIndex]->mServerIP,
              sShard->mNodeInfo[sNodeDbcIndex]->mPortNo,
              sNodeStmt->mStatementID);

    sNodeResult = ulnTrimLob( SQL_HANDLE_STMT,
                              (ulnObject *)sNodeStmt,
                              aLocatorCType,
                              sShardLocator->mLobLocator,
                              aStartOffset );

    if ( sNodeResult == SQL_SUCCESS )
    {
        /* Nothing to do */
    }
    else
    {
        ulsdNativeErrorToUlnError(aFnContext,
                                  SQL_HANDLE_STMT,
                                  (ulnObject *)sNodeStmt,
                                  sShard->mNodeInfo[sNodeDbcIndex],
                                  (acp_char_t *)"TrimLob");

        sErrorResult = sNodeResult;
        sSuccess = ACP_FALSE;
    }

    ACI_TEST( sSuccess == ACP_FALSE );

    return sSuccessResult;

    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
    }
    ACI_EXCEPTION_END;

    return sErrorResult;
}

/***********************************************************************
 * Description : Lob Locator List�� �����Ѵ�.
 *
 *    aMetaDbc       - [IN] Meta Dbc handle
 **********************************************************************/
ACI_RC ulsdLobLocatorFreeAll( ulnDbc * aMetaDbc )
{
    ulsdLobLocator  *sLocator;
    acp_list_node_t *sIterator1;
    acp_list_node_t *sIterator2;

    acpThrMutexLock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);
    ACP_LIST_ITERATE_SAFE(&(aMetaDbc->mShardDbcCxt.mLobLocatorList), sIterator1, sIterator2)
    {
        sLocator = (ulsdLobLocator *)sIterator1;

        (void) ulsdLobLocatorDestroy(aMetaDbc, sLocator, ACP_FALSE);
    }
    acpThrMutexUnlock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);

    return ACI_SUCCESS;
}

/***********************************************************************
 * Description : Shard Lob Locator�� �����Ѵ�.
 *
 *    aMetaDbc       - [IN] Meta Dbc handle
 *    aLobLocator    - [IN] ������ Lob Locator
 **********************************************************************/
ACI_RC ulsdLobLocatorDestroy( ulnDbc         * aMetaDbc,
                              ulsdLobLocator * aLobLocator,
                              acp_bool_t       aNeedLock )
{ 
    ACI_TEST(acpListIsEmpty(&aMetaDbc->mShardDbcCxt.mLobLocatorList));
         
    /* list ���� ���� */
    if ( aNeedLock == ACP_TRUE )
    {
        acpThrMutexLock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);
    }

    acpListDeleteNode(&(aLobLocator->mList));

    if ( aNeedLock == ACP_TRUE )
    {
        acpThrMutexUnlock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);
    }

    /* �޸� ���� */
    aLobLocator->mArraySize  = 0;
    aLobLocator->mLobLocator = 0;
    acpMemFree(aLobLocator);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : shard�� locator�� �����Ѵ�.
 *    1. shard locator �迭�� aCount ũ�⸸ŭ �����Ѵ�.
 *    2. ����Ʈ�� �ʱ�ȭ�Ѵ�.
 *    3. aMetaDbc->mShardDbcCxt.mLobLocatorList�� �Ŵܴ�.
 *    4. 1�� ��ȯ�Ѵ�.
 *
 *    aMetaDbc       - [IN] Meta Dbc handle
 *    aCount         - [IN] ������ �迭 ����� ����
 *    aOutputLocator - [OUT] ������ locator �ּ� ��ȯ
 **********************************************************************/
ACI_RC ulsdLobLocatorCreate( ulnDbc          *aMetaDbc,
                             acp_uint16_t     aCount,
                             ulsdLobLocator **aOutputLocator )
{
    ulsdLobLocator *sLocator = NULL;

    // 1.
    ACI_TEST( acpMemCalloc((void **)&sLocator,
                           ACI_SIZEOF(ulsdLobLocator),
                           aCount)
              != ACI_SUCCESS );

    sLocator->mArraySize = aCount;

    // 2.
    acpListInitObj(&sLocator->mList, sLocator);

    // 3.
    acpThrMutexLock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);
    acpListAppendNode(&(aMetaDbc->mShardDbcCxt.mLobLocatorList), (acp_list_t *)sLocator); 
    acpThrMutexUnlock( & aMetaDbc->mShardDbcCxt.mLock4LocatorList);

    // 4.
    *aOutputLocator = sLocator;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : shard�� locator�� �����Ѵ�(for Fetch).
 *    1. �������� stmt�� Node Dbc Index�� ���Ѵ�.
 *       server-side�� ��� Index�� -1 �� �����Ѵ�.
 *    2. shard locator ����
 *    3. 2�� NodeDbcIndex, CType, Node Locator ����
 *    4. 2 ��ȯ
 *
 *    aMetaDbc       - [IN] Statement handle
 *    aCType         - [IN] SQL_C_BLOB_LOCATOR or SQL_C_CLOB_LOCATOR
 *    aUserBufferPtr - [IN/OUT] ����� ������ �ּ�.
 *                          Node�κ��� ���� Locator �� �Է�.
 *                          ������ shard locator �ּҰ� ��ȯ.
 **********************************************************************/
ACI_RC ulsdLobAddFetchLocator( ulnStmt      * aStmt,
                               acp_uint64_t * aUserBufferPtr )
{
    ulnDbc          *sMetaDbc      = NULL;
    ulsdLobLocator  *sShardLocator = NULL;
    ulsdStmtContext *sShardStmtCxt = NULL;
    acp_sint16_t     sNodeDbcIndex = -1;

    if ( ULSD_IS_SHARD_LIB_SESSION(aStmt->mParentDbc) == ACP_TRUE )
    {
        sShardStmtCxt = & aStmt->mShardStmtCxt.mParentStmt->mShardStmtCxt;
        sNodeDbcIndex = sShardStmtCxt->mNodeDbcIndexArr[sShardStmtCxt->mNodeDbcIndexCur];
        sMetaDbc = aStmt->mParentDbc->mShardDbcCxt.mParentDbc;
    }
    else // for COORD(Server-side) stmt
    {
        sNodeDbcIndex = -1;
        sMetaDbc = aStmt->mParentDbc;
    }

    ACI_TEST( ulsdLobLocatorCreate( sMetaDbc,
                                    1, // Array Size
                                    &sShardLocator )
              != ACI_SUCCESS );

    sShardLocator->mLobLocator = *aUserBufferPtr;
    sShardLocator->mNodeDbcIndex = sNodeDbcIndex;

    /* ����� ���ۿ� shard locator �ּ� �����ϱ� */
    *aUserBufferPtr = (acp_uint64_t)sShardLocator;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : shard�� locator�� �����Ѵ�(for OutBind).
 *    ** ���� ���� ����Ǵ� ��츦 ����Ͽ��� �Ѵ�**
 *    1. �������� stmt�� Node Dbc Index�� ���Ѵ�.
 *       server-side�� ��� Index�� -1 �� �����Ѵ�.
 *    2. MetaStmt�� DescRecIpd�� ���Ѵ�.
 *       ����Ǵ� ��� ��忡�� shard locator �迭�� �����ϱ� ����
 *       DescRecIpd->mShardLocator ������ ����Ѵ�.
 *
 *    aStmt           - [IN] Statement handle
 *    aParamNumber    - [IN] Parameter number
 *    aParamInOutType - [IN] INPUT/OUTPUT
 *    aInBindLocator  - [IN] ����ڰ� �Է��� Locator. INPUT�� ���� ��ȿ.
 *    aUserBufferPtr  - [IN/OUT] ����� ������ �ּ�.
 *                          Node�κ��� ���� Locator �� �Է�.
 *                          ������ shard locator �ּҰ� ��ȯ.
 **********************************************************************/
ACI_RC ulsdLobAddOutBindLocator( ulnStmt           * aStmt,
                                 acp_uint16_t        aParamNumber,
                                 ulnParamInOutType   aParamInOutType,
                                 ulsdLobLocator    * aInBindLocator,
                                 acp_uint64_t      * aUserBufferPtr )
{
    ulnDbc          *sMetaDbc          = NULL;
    ulnStmt         *sMetaStmt         = NULL;
    ulsdLobLocator  *sShardLocator     = NULL;
    ulsdStmtContext *sMetaStmtShardCxt = NULL;
    ulnDescRec      *sMetaStmtDescRecIpd;
    acp_uint16_t     sNodeDbcIndexCount;
    acp_sint32_t     sNodeDbcIndexCur;
    acp_sint16_t     sNodeDbcIndex;

    if ( ULSD_IS_SHARD_LIB_SESSION(aStmt->mParentDbc) == ACP_TRUE )
    {
        sMetaDbc  = aStmt->mParentDbc->mShardDbcCxt.mParentDbc;
        sMetaStmt = aStmt->mShardStmtCxt.mParentStmt;

        sMetaStmtShardCxt   = &(sMetaStmt->mShardStmtCxt);
        sMetaStmtDescRecIpd = ulnStmtGetIpdRec(sMetaStmt, aParamNumber),

        sNodeDbcIndexCount = sMetaStmtShardCxt->mNodeDbcIndexCount;
        sNodeDbcIndexCur   = aStmt->mShardStmtCxt.mMyNodeDbcIndexCur;
        sNodeDbcIndex      = sMetaStmtShardCxt->mNodeDbcIndexArr[sNodeDbcIndexCur];
    }
    else // for COORD(Server-side) stmt
    {
        sMetaDbc  = aStmt->mParentDbc;
        sMetaStmt = aStmt;

        sMetaStmtDescRecIpd = ulnStmtGetIpdRec(sMetaStmt, aParamNumber),

        sNodeDbcIndexCount = 1;
        sNodeDbcIndexCur   = 0;
        sNodeDbcIndex      = -1; 
    }

    /* 1. ��� �� ó�� �����̶��  ���� ��� ������ ������
     *      ũ��� ulsdLobLocator �迭�� �����ϰ�,
     *      �� �ּҰ��� parent stmt�� DescRecIpd �� ������ �д�.
     * 2. else, parent stmt�� DescRecIpd ���� 1���� ������ �ξ���
     *      shard Locator �� �����´�. */

    sShardLocator = (ulsdLobLocator *)
            ulnDescRecGetShardLobLocator( sMetaStmtDescRecIpd );

    if ( sShardLocator == NULL )
    {
        ACI_TEST( ulsdLobLocatorCreate( sMetaDbc,
                                        sNodeDbcIndexCount,
                                        &sShardLocator )
                  != ACI_SUCCESS );

        ulnDescRecSetShardLobLocator( sMetaStmtDescRecIpd,
                                     (void *)sShardLocator );
    }
    else
    {        
        /* Nothing to do */
    }

    sShardLocator[sNodeDbcIndexCur].mLobLocator = *aUserBufferPtr;
    sShardLocator[sNodeDbcIndexCur].mNodeDbcIndex = sNodeDbcIndex;

    /* INPUT�� OUTPUT���� �ٲ�� ����� �����,
     *   flag�� �����ϰ� ���� ���κ����� out locator�� 
     *   InBindLocator->mDestLobLocators�� �����ش�.
     * �׷��� ������, ����� ���ۿ� �����ش� */
    if ( aParamInOutType == ULN_PARAM_INOUT_TYPE_INPUT )
    {
        sMetaStmt->mShardStmtCxt.mHasLocatorParamToCopy = ACP_TRUE;

        aInBindLocator->mDestLobLocators = sShardLocator;

        /* ����ڰ� InBind �ߴ� ���� �ٽ� �����ϱ� */
        *aUserBufferPtr = (acp_uint64_t)aInBindLocator;
    }
    else
    {
        /* ����� ���ۿ� shard locator �ּ� �����ϱ� */
        *aUserBufferPtr = (acp_uint64_t)sShardLocator;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/***********************************************************************
 * Description : Src Locator�κ��� �о Dest Locator�� ����
 *    1. Src Locator���� ���̿� NULL ���θ� �����´�.
 *    2. NULL�� �ƴϸ�, �� ��忡 ���� �Ʒ� ����
 *       2-1. Prepare4Write
 *       2-2. ���̰� 0���� ���, 32KB�� Get -> Write
 *       2-3. ���̰� 0�̸�, Write size 0
 *       2-4. FinishWrite
 *    3. NULL�̸� �ƹ��͵� ���� �ʴ´�.
 *    
 *
 *    aFnContext     - [IN] Function Context
 *    aShard         - [IN] shard Dbc
 *    aStmt          - [IN] �������� statement handle
 **********************************************************************/
static SQLRETURN ulsdLobCopyNodes( ulnFnContext     * aFnContext,
                                   ulsdDbc          * aShard,
                                   ulnStmt          * aStmt,
                                   ulsdLobLocator   * aSrcLocator,
                                   acp_sint16_t       aLocatorCType )
{
    SQLRETURN          sRet;
    ulnDbc            *sSrcDbc;
    ulnStmt           *sDestStmt            = NULL;
    acp_uint32_t       sSrcLobLen;
    acp_uint16_t       sIsNullLob;
    acp_uint32_t       sReadBytes           = 0;
    acp_uint32_t       sReadLength          = 0;
    acp_uint32_t       sOffset              = 0;
    acp_uint32_t       sRemainingSize       = 0;
    acp_char_t        *sLobBuffer           = NULL;
    acp_uint16_t       i;
    ulsdLobLocator   * sDestLocators        = NULL;

    acp_sint16_t       sHandleType;
    ulnObject        * sErrorRaiseObject    = NULL;
    ulsdNodeInfo     * sNodeInfo            = NULL;

    sDestLocators = aSrcLocator->mDestLobLocators;

    // INPUT -> INPUT
    ACI_TEST_CONT( sDestLocators == NULL, NORMAL_EXIT );

    ACI_TEST_RAISE( aSrcLocator == NULL, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( aSrcLocator->mLobLocator == 0, LABEL_INVALID_LOCATOR );

    if (aSrcLocator->mNodeDbcIndex >= 0 ) // from NODE
    {
        sSrcDbc = aStmt->mParentDbc->mShardDbcCxt.mShardDbc->
                      mNodeInfo[aSrcLocator->mNodeDbcIndex]->mNodeDbc;
    }
    else // from COORD(Server-side)
    {
        sSrcDbc = aStmt->mParentDbc;
    }

    /* 1. Src Locator���� ���̿� NULL ���θ� �����´�. */
    sRet = ulnGetLobLength ( SQL_HANDLE_DBC,
                             (ulnObject *)sSrcDbc,
                             aSrcLocator->mLobLocator, 
                             aLocatorCType,
                             &sSrcLobLen,
                             &sIsNullLob );

    ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_SRC_ERROR );

    /* 2. NULL�� �ƴϸ�, �� ��忡 ���� �Ʒ� ���� */
    if ( sIsNullLob != ACP_TRUE )
    {
        /*    2-1. Prepare4Write */
        for ( i = 0; i < sDestLocators->mArraySize; i++ )
        {
            if ( sDestLocators[i].mLobLocator != 0 )
            {
                sDestStmt = ulsdLobGetExecuteStmt(aStmt, & sDestLocators[i]);

                sRet = ulsdnLobPrepare4Write( SQL_HANDLE_STMT,
                                              (ulnObject *)sDestStmt,
                                              aLocatorCType,
                                              sDestLocators[i].mLobLocator,
                                              0,            // offset
                                              sSrcLobLen ); // size

                ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_DEST_ERROR );
            }
            else
            {
                /* Nothing to do */
            }
        }

        /*    2-2. ���̰� 0���� ���, 32KB�� Get -> Write */
        if ( sSrcLobLen > 0 )
        {     
            ACI_TEST( acpMemAlloc((void **)&sLobBuffer,
                                   ULN_LOB_PIECE_SIZE)
                      != ACI_SUCCESS );

            while (sOffset < sSrcLobLen)
            {
                sRemainingSize = sSrcLobLen - sOffset;
                sReadBytes = ACP_MIN(sRemainingSize, ULN_LOB_PIECE_SIZE);

                sRet = ulnGetLob( SQL_HANDLE_DBC,
                                  (ulnObject *)sSrcDbc,
                                  aLocatorCType,
                                  aSrcLocator->mLobLocator,
                                  sOffset,
                                  sReadBytes,
                                  SQL_C_BINARY,
                                  sLobBuffer,
                                  ULN_LOB_PIECE_SIZE,
                                  &sReadLength );

                ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_SRC_ERROR );
 
                for ( i = 0; i < sDestLocators->mArraySize; i++ )
                {
                    if ( sDestLocators[i].mLobLocator != 0 )
                    {
                        sDestStmt = ulsdLobGetExecuteStmt(aStmt, & sDestLocators[i]);

                        sRet = ulsdnLobWrite ( SQL_HANDLE_STMT,
                                               (ulnObject *)sDestStmt,
                                               aLocatorCType,
                                               sDestLocators[i].mLobLocator,
                                               SQL_C_BINARY,
                                               sLobBuffer,
                                               sReadLength );

                        ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_DEST_ERROR );
                    }
                    else
                    {
                        /* Nothing to do */
                    }
                }
                sOffset += sReadLength;
            }

        }
        /*    2-3. ���̰� 0�̸�, Write size 0 */
        else
        {
            for ( i = 0; i < sDestLocators->mArraySize; i++ )
            {
                if ( sDestLocators[i].mLobLocator != 0 )
                {
                    sDestStmt = ulsdLobGetExecuteStmt(aStmt, & sDestLocators[i]);

                    sRet = ulsdnLobWrite ( SQL_HANDLE_STMT,
                                           (ulnObject *)sDestStmt,
                                           aLocatorCType,
                                           sDestLocators[i].mLobLocator,
                                           SQL_C_BINARY,
                                           sLobBuffer,
                                           0 ); // size 0

                    ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_DEST_ERROR );
                }
                else
                {
                    /* Nothing to do */
                }
            }
        }

        /*    2-4. FinishWrite */
        for ( i = 0; i < sDestLocators->mArraySize; i++ )
        {
            if ( sDestLocators[i].mLobLocator != 0 )
            {
                sDestStmt = ulsdLobGetExecuteStmt(aStmt, & sDestLocators[i]);

                sRet = ulsdnLobFinishWrite( SQL_HANDLE_STMT,
                                            (ulnObject *)sDestStmt,
                                            aLocatorCType,
                                            sDestLocators[i].mLobLocator );

                ACI_TEST_RAISE( sRet != SQL_SUCCESS, LABEL_DEST_ERROR );
            }
            else
            {
                /* Nothing to do */
            }
        }

    }
    /* 3. NULL�̸� �ƹ��͵� ���� �ʴ´�. */
    else
    {
        /* Nothing to do */
    }

    (void) ulsdFreeLob( aStmt,
                        (acp_uint64_t)sDestLocators );

    if ( sLobBuffer != NULL )
    {
        acpMemFree(sLobBuffer);
    }

    ACI_EXCEPTION_CONT(NORMAL_EXIT);

    return SQL_SUCCESS;

    ACI_EXCEPTION(LABEL_SRC_ERROR)
    {
        sHandleType = SQL_HANDLE_DBC;
        sErrorRaiseObject = (ulnObject *)sSrcDbc;
        if ( ULSD_IS_SHARD_LIB_SESSION(sSrcDbc) == ACP_TRUE )
        {
            sNodeInfo = sSrcDbc->mShardDbcCxt.mNodeInfo;
        }
        else
        {
            sNodeInfo = NULL;
        }
    }
    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
        sNodeInfo = NULL;
    }
    ACI_EXCEPTION(LABEL_DEST_ERROR)
    {
        if ( ULSD_IS_SHARD_LIB_SESSION(sDestStmt->mParentDbc) == ACP_TRUE )
        {
            /* BUGBUG �������� ������ ��忡 ���ؼ� retry�� �����Ѱ�? */
            if ( ulsdNodeFailConnectionLost( SQL_HANDLE_STMT,
                                             (ulnObject *)sDestStmt ) == ACP_TRUE )
            {
                if ( aStmt->mParentDbc->mAttrAutoCommit == SQL_AUTOCOMMIT_OFF )
                {
                    ulsdClearOnTransactionNode(aStmt->mParentDbc);
                }
                else
                {
                    /* Do Nothing */
                }
            }

            sNodeInfo = aShard->mNodeInfo[sDestLocators[i].mNodeDbcIndex];
        }
        else
        {
            sNodeInfo = NULL;
        }

        sHandleType = SQL_HANDLE_STMT;
        sErrorRaiseObject = (ulnObject *)sDestStmt;
    }
    ACI_EXCEPTION_END;

    if ( sNodeInfo != NULL )
    {
        ulsdNativeErrorToUlnError( aFnContext,
                                   sHandleType,
                                   sErrorRaiseObject,
                                   sNodeInfo,
                                   (acp_char_t *)"ulsdLobCopy" );
    }

    (void) ulsdFreeLob( aStmt,
                        (acp_uint64_t)sDestLocators );

    if ( sLobBuffer != NULL )
    {
        acpMemFree(sLobBuffer);
    }

    return ULN_FNCONTEXT_GET_RC(aFnContext);
}

/***********************************************************************
 * Description : InBind Locator�� Node(Src.)�� Dest. Node�� �ٸ� ���,
 *    BindParam list�� IN -> OUT���� �ٲ�� ����� ��� 
 *    Src -> Dest���� Copy �۾��� �����Ѵ�.
 *
 *    aFnContext     - [IN] Function Context
 *    aShard         - [IN] shard Dbc
 *    aStmt          - [IN] �������� statement handle
 **********************************************************************/
SQLRETURN ulsdLobCopy( ulnFnContext     * aFnContext,
                       ulsdDbc          * aShard,
                       ulnStmt          * aStmt )
{
    SQLRETURN     sResult = SQL_SUCCESS;

    acp_list_node_t       * sNode     = NULL;
    ulsdBindParameterInfo * sBind     = NULL;

    ACP_LIST_ITERATE( & aStmt->mShardStmtCxt.mBindParameterList, sNode )
    {
        sBind = (ulsdBindParameterInfo *)sNode->mObj;
                                   
        if ( ulsdParamIsLocInBoundLob( sBind->mValueType,
                                       sBind->mInputOutputType )
             == ACP_TRUE )
        {
            sResult = ulsdLobCopyNodes( aFnContext,
                                        aShard,
                                        aStmt,
                                        (ulsdLobLocator *)(*(acp_uint64_t *)
                                           sBind->mParameterValuePtr),
                                        sBind->mValueType );

            ACI_TEST( sResult != SQL_SUCCESS );
        }
    }

    return sResult;

    ACI_EXCEPTION_END;

    return ULN_FNCONTEXT_GET_RC(aFnContext);
}

ACI_RC ulsdLobBindInfoBuild4Param(ulnFnContext      *aFnContext,
                                  void              *aUserBufferPtr,
                                  acp_bool_t        *aChangeInOutType)
{
    ulnStmt        *sMetaStmt        = NULL;
    ulnStmt        *sStmt            = aFnContext->mHandle.mStmt;
    ulsdLobLocator *sSrcLocator      = NULL;
    acp_bool_t      sChangeInOutType = ACP_FALSE;
    acp_sint16_t    sDestNodeDbcIndex;

    if ( ULSD_IS_SHARD_LIB_SESSION(sStmt->mParentDbc) == ACP_TRUE )
    {
        sMetaStmt = sStmt->mShardStmtCxt.mParentStmt;
        sDestNodeDbcIndex = sMetaStmt->mShardStmtCxt.mNodeDbcIndexArr[sStmt->mShardStmtCxt.mMyNodeDbcIndexCur];
    }
    else // COORD(Server-side) stmt.
    {
        sMetaStmt = sStmt;
        sDestNodeDbcIndex = -1;
    }

    /*
     * Lob Locator�� InBind�� ���,
     * InBind locator�� ���� execute ��� ��尡 ���� �ٸ���
     * OutBind�� �ٲپ execute �Ͽ� Locator�� �޾ƿ� ��
     * copy �۾�(ulsdCopyLob)�� ������ �����Ѵ�.
     */
    sSrcLocator = (ulsdLobLocator *)(*(acp_uint64_t *)aUserBufferPtr);

    ACI_TEST_RAISE( sSrcLocator == NULL, LABEL_INVALID_LOCATOR );
    ACI_TEST_RAISE( sSrcLocator->mLobLocator == 0, LABEL_INVALID_LOCATOR );

    if ( sSrcLocator->mNodeDbcIndex != sDestNodeDbcIndex )
    {
        sChangeInOutType = ACP_TRUE;
    }
    else
    {
        sChangeInOutType = ACP_FALSE;
    }

    sMetaStmt->mShardStmtCxt.mHasLocatorInBoundParam = ACP_TRUE;

    *aChangeInOutType = sChangeInOutType;

    return ACI_SUCCESS;

    ACI_EXCEPTION( LABEL_INVALID_LOCATOR )
    {
        ulnError( aFnContext, ulERR_ABORT_INVALID_LOCATOR_HANDLE );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

