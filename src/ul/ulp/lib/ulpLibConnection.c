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

#include <ulnInit.h>
#include <ulpLibConnection.h>
#include <ulpLibInterFuncB.h>


ulpLibConnMgr gUlpConnMgr;

/* extern for ulpLibInterface.cpp */
/* connection hash table�� �ʱ�ȭ �Ǿ����������� flag*/
extern acp_bool_t        gUlpLibDoInitProc;
/* �ʱ�ȭ �ڵ� �ѹ�ȣ���� �����ϱ� ���� latch*/
extern acp_thr_rwlock_t  gUlpLibLatch4Init;

/* initilizer */
ACI_RC ulpLibConInitialize( void )
{
    acp_sint32_t    sI;
    ulpLibConnNode *sDefaultConn;
    ulpErrorMgr     sErrorMgr;

    /* alloc default connection node */
    ACI_TEST ( ( sDefaultConn = ulpLibConNewNode(NULL, ACP_TRUE) ) == NULL );

    /* init connection hash table latch */
    ACI_TEST_RAISE( acpThrRwlockCreate( &(gUlpConnMgr.mConnHashTab.mLatch),
                                        ACP_THR_RWLOCK_DEFAULT )
                    != ACP_RC_SUCCESS, ERR_LATCH_INIT );

    /* init connection latch */
    ACI_TEST_RAISE( acpThrRwlockCreate( &(gUlpConnMgr.mConnLatch),
                                        ACP_THR_RWLOCK_DEFAULT )
                    != ACP_RC_SUCCESS, ERR_LATCH_INIT );

    for( sI=0 ; sI < MAX_NUM_CONN_HASH_TAB_ROW ; sI++ )
    {
        gUlpConnMgr.mConnHashTab.mTable[sI] = NULL;
    }

    gUlpConnMgr.mConnHashTab.mTable[0] = sDefaultConn;
    gUlpConnMgr.mConnHashTab.mSize     = MAX_NUM_CONN_HASH_TAB_ROW;
    gUlpConnMgr.mConnHashTab.mNumNode  = 0;
    gUlpConnMgr.mConnHashTab.mHash     = ulpHashFunc;
    gUlpConnMgr.mConnHashTab.mCmp      = acpCStrCmp;

    /* initialize xa function*/
    ulxSetCallbackSesConn( ulpAfterXAOpen );
    ulxSetCallbackSesDisConn ( ulpAfterXAClose );

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_LATCH_INIT);
    {
        acpMemFree(sDefaultConn);

        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Init_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


/* finalizer */
void ulpLibConFinalize( void )
{
    ulpErrorMgr sErrorMgr;

    /* delete all connection nodes*/
    (void) ulpLibConDelAllConn();

    ACI_TEST_RAISE( acpThrRwlockDestroy( & gUlpConnMgr.mConnHashTab.mLatch )
                    != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );

    ACI_TEST_RAISE( acpThrRwlockDestroy( & gUlpConnMgr.mConnLatch )
                    != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );

    ACI_TEST_RAISE( acpThrRwlockDestroy( & gUlpLibLatch4Init )
                    != ACP_RC_SUCCESS, ERR_LATCH_DESTROY )

    /* Fix BUG-27644 Apre �� ulpConnMgr::ulpInitialzie, finalize�� �߸���. */
    ACE_ASSERT(ulnFinalize() == ACI_SUCCESS);

    return;

    ACI_EXCEPTION (ERR_LATCH_DESTROY);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Destroy_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);

        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    return;
}


ACI_RC ulpLibConInitConnMgr( void )
{
/***********************************************************************
 *
 * Description :
 *    ó�� ulpDoEmsql�� ȣ��Ǹ� ���Լ��� ȣ��Ǿ� connection hash table��
 *    XA�� �ʱ�ȭ �����ش�.
 * Implementation :
 *
 ***********************************************************************/
    ACI_RC sRes = ACI_FAILURE;

    /* fix BUG-25597 APRE���� AIX�÷��� �νõ� ���������� �ذ��ؾ� �մϴ�.
    ulConnMgr �ʱ�ȭ���� �̹� ������ CLI �� XA Connection����
    Loading�Ѵ�. */
    ULP_SERIAL_BEGIN(sRes = ulpLibConInitialize());
    ULP_SERIAL_EXEC(gUlpLibDoInitProc = ACP_FALSE,1);
    /* ulpConnMgr�� �ʱ�ȭ�� �Ϸ����
    CLI�� XA Connection���� HDBC, HENV�� APRE��
    Connection list�� ����Ѵ� */

    ULP_SERIAL_END(ulnLoadOpenedXaConnections2APRE());
    return sRes;
}


ulpLibConnNode* ulpLibConNewNode( acp_char_t *aConnName , acp_bool_t aValidInit )
{
/***********************************************************************
 *
 * Description :
 *    create a new connection node with given name.
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    sI;
    ulpLibConnNode *sConnNode;
    ulpErrorMgr     sErrorMgr;

    /** create new connection node **/
    /* alloc connection node */
    ACI_TEST_RAISE( acpMemCalloc( (void**)&sConnNode,
                                  1,
                                  ACI_SIZEOF(ulpLibConnNode) )
                    != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

    if( aConnName != NULL )
    {
        ACI_TEST_RAISE( acpCStrLen(aConnName, ACP_SINT32_MAX)
                        > MAX_CONN_NAME_LEN, ERR_CONN_NAME_OVERFLOW );
        acpCStrCpy( sConnNode->mConnName,
                    MAX_CONN_NAME_LEN + 1,
                    aConnName,
                    acpCStrLen(aConnName, ACP_SINT32_MAX));
    }

    /* set hash func. & comparision func.*/
    sConnNode->mStmtHashT.mSize = MAX_NUM_STMT_HASH_TAB_ROW;
    sConnNode->mStmtHashT.mNumNode = 0;
    sConnNode->mStmtHashT.mHash = ulpHashFunc;
    sConnNode->mStmtHashT.mCmp  = acpCStrCmp;

    sConnNode->mCursorHashT.mSize = MAX_NUM_STMT_HASH_TAB_ROW;
    sConnNode->mCursorHashT.mNumNode = 0;
    sConnNode->mCursorHashT.mHash = ulpHashFunc;
    sConnNode->mCursorHashT.mCmp  = acpCStrCmp;

    sConnNode->mUnnamedStmtCacheList.mSize = NUM_UNNAME_STMT_CACHE;
    sConnNode->mUnnamedStmtCacheList.mNumNode = 0;
    sConnNode->mValid = aValidInit;

    /* initialize latchs */
    ACI_TEST_RAISE( 
        acpThrRwlockCreate( &(sConnNode->mUnnamedStmtCacheList.mLatch),
                            ACP_THR_RWLOCK_DEFAULT ) != ACP_RC_SUCCESS,
        ERR_LATCH_INIT);
    ACI_TEST_RAISE(
        acpThrRwlockCreate( &(sConnNode->mLatch4Xa),
                            ACP_THR_RWLOCK_DEFAULT ) != ACP_RC_SUCCESS,
        ERR_LATCH_INIT );

    for( sI=0 ; sI < MAX_NUM_STMT_HASH_TAB_ROW ; sI++ )
    {
        ACI_TEST_RAISE(
            acpThrRwlockCreate( &(sConnNode->mStmtHashT.mTable[sI].mLatch),
                                ACP_THR_RWLOCK_DEFAULT ) != ACP_RC_SUCCESS,
            ERR_LATCH_INIT );
        sConnNode->mStmtHashT.mTable[sI].mList = NULL;
    }

    for( sI=0 ; sI < MAX_NUM_STMT_HASH_TAB_ROW ; sI++ )
    {
        ACI_TEST_RAISE(
            acpThrRwlockCreate( &(sConnNode->mCursorHashT.mTable[sI].mLatch),
                                ACP_THR_RWLOCK_DEFAULT ) != ACP_RC_SUCCESS,
            ERR_LATCH_INIT );
        sConnNode->mCursorHashT.mTable[sI].mList = NULL;
    }

    /* TASK-7218 Handling Multiple Errors */
    ACI_TEST_RAISE( acpMemCalloc( (void**)&(sConnNode->mMultiErrorMgr.mErrorStack),
                                  MULTI_ERROR_INIT_SIZE,
                                  ACI_SIZEOF(ulpMultiErrorStack) )
                    != ACP_RC_SUCCESS, ERR_MEMORY_ALLOC );

    sConnNode->mMultiErrorMgr.mStackSize = MULTI_ERROR_INIT_SIZE;
    sConnNode->mMultiErrorMgr.mErrorNum = 0;

    return sConnNode;

    ACI_EXCEPTION (ERR_MEMORY_ALLOC);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Memory_Alloc_Error );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_INIT);
    {
        acpMemFree(sConnNode);

        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Init_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_CONN_NAME_OVERFLOW);
    {
        acpMemFree(sConnNode);
    }
    ACI_EXCEPTION_END;

    return NULL;
}


void ulpLibConInitDefaultConn( void )
{
/***********************************************************************
 *
 * Description :
 *    initialize the default connection node
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sDefaultConn = ulpLibConGetDefaultConn();

    if ( sDefaultConn != NULL )
    {
        ulpLibStDelAllStmtCur( &(sDefaultConn->mStmtHashT),
                               &(sDefaultConn->mCursorHashT) );
        ulpLibStDelAllUnnamedStmt( &(sDefaultConn->mUnnamedStmtCacheList) );

        /* initialize default connection node */
        sDefaultConn->mHenv     = SQL_NULL_HENV;
        sDefaultConn->mHdbc     = SQL_NULL_HDBC;
        sDefaultConn->mHstmt    = SQL_NULL_HSTMT;
        sDefaultConn->mConnName[0] = '\0';
        sDefaultConn->mUser     = NULL;
        sDefaultConn->mPasswd   = NULL;
        sDefaultConn->mConnOpt1 = NULL;
        sDefaultConn->mConnOpt2 = NULL;
        sDefaultConn->mIsXa     = ACP_FALSE;

        /* TASK-7218 Handling Multiple Errors */
        ulpLibInitMultiErrorMgr( &(sDefaultConn->mMultiErrorMgr) );
    }
    else
    {
        /* do nothing */
    }
}

/* default connection ��ü�� ���´� */
ulpLibConnNode *ulpLibConGetDefaultConn()
{
    return gUlpConnMgr.mConnHashTab.mTable[0];
}


ulpLibConnNode *ulpLibConLookupConn( acp_char_t* aConName )
{
/***********************************************************************
 *
 * Description :
 *    connection �̸��� ������ �ش� connection��ü�� ã�´�
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    acp_bool_t      sIsLatched;
    ulpErrorMgr     sErrorMgr;

    sIsLatched = ACP_FALSE;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get read lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockRead( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_READ );
        sIsLatched = ACP_TRUE;
    }

    /* hash table is empty */
    ACI_TEST_RAISE( gUlpConnMgr.mConnHashTab.mNumNode == 0 , ERR_EMPTY_HASH );

    sConnNode =
      gUlpConnMgr.mConnHashTab.mTable[ ( (*gUlpConnMgr.mConnHashTab.mHash)( (acp_uint8_t *)aConName )
                                       % ( gUlpConnMgr.mConnHashTab.mSize-1 ) ) + 1 ];
    while ( ( sConnNode != NULL ) &&
            ( *gUlpConnMgr.mConnHashTab.mCmp )( aConName, sConnNode->mConnName,
                                                MAX_CONN_NAME_LEN )
          )
    {
        sConnNode = sConnNode->mNext;
    }

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
        sIsLatched = ACP_FALSE;
    }

    ACI_TEST( sConnNode == NULL );

    return sConnNode;

    ACI_EXCEPTION (ERR_LATCH_READ);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Read_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION ( ERR_EMPTY_HASH )
    {
        /*DO NOTHING*/
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return NULL;
}


ulpLibConnNode *ulpLibConAddConn( ulpLibConnNode *aConnNode )
{
/***********************************************************************
 *
 * Description :
 *    �ش� connection ��ü�� �߰��Ѵ�.
 * Implementation :
 *
 ***********************************************************************/
    acp_char_t       *sConnName;
    ulpLibConnNode   *sConnNode;
    acp_sint32_t      sI;
    acp_bool_t        sIsLatched;
    ulpErrorMgr       sErrorMgr;

    sConnName  = aConnNode->mConnName;
    sIsLatched = ACP_FALSE;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockWrite( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_WRITE );
        sIsLatched = ACP_TRUE;
    }

    /** find connection **/
    sI = ( (*gUlpConnMgr.mConnHashTab.mHash)( (acp_uint8_t *)sConnName ) %
           ( gUlpConnMgr.mConnHashTab.mSize-1 ) ) + 1;
    sConnNode = gUlpConnMgr.mConnHashTab.mTable[ sI ];

    while ( ( sConnNode != NULL ) &&
            ( *gUlpConnMgr.mConnHashTab.mCmp )( sConnName, sConnNode->mConnName,
                                                MAX_CONN_NAME_LEN )
          )
    {
        sConnNode = sConnNode->mNext;
    }
    /* already exist */
    ACI_TEST( sConnNode != NULL );

    /* link */
    /* list ���� �տ��Ŵ�.*/
    aConnNode->mNext = gUlpConnMgr.mConnHashTab.mTable[ sI ];
    gUlpConnMgr.mConnHashTab.mTable[ sI ] = aConnNode;

    gUlpConnMgr.mConnHashTab.mNumNode++;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
        sIsLatched = ACP_FALSE;
    }

    return aConnNode;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACE_ASSERT ( acpThrRwlockUnlock( &(gUlpConnMgr.mConnHashTab.mLatch) )
                     == ACP_RC_SUCCESS );
    }

    return NULL;
}


ACI_RC ulpLibConDelConn( acp_char_t* aConName )
{
/***********************************************************************
 *
 * Description :
 *    �־��� �̸��� connection�� �����Ѵ�
 * Implementation :
 *
 ***********************************************************************/
    acp_uint32_t    sIndex;
    ulpLibConnNode *sConnNode;
    ulpLibConnNode *sConnNodeP;
    acp_bool_t      sIsLatched;
    ulpErrorMgr     sErrorMgr;

    sIsLatched = ACP_FALSE;
    sConnNodeP = NULL;

    /** find Connection node **/

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockWrite( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_WRITE );
        sIsLatched = ACP_TRUE;
    }

    ACI_TEST( gUlpConnMgr.mConnHashTab.mNumNode == 0 );

    sIndex = ( (*gUlpConnMgr.mConnHashTab.mHash)( (acp_uint8_t*) aConName )
               % ( gUlpConnMgr.mConnHashTab.mSize-1 ) ) + 1;
    sConnNode = gUlpConnMgr.mConnHashTab.mTable[ sIndex ];

    while ( ( sConnNode != NULL ) &&
            ( *gUlpConnMgr.mConnHashTab.mCmp )( aConName, sConnNode->mConnName,
                                                MAX_CONN_NAME_LEN )
          )
    {
        sConnNodeP = sConnNode;
        sConnNode  = sConnNode->mNext;
    }
    ACI_TEST( sConnNode == NULL );

    if ( sConnNodeP != NULL )
    {
        sConnNodeP->mNext = sConnNode->mNext;
    }
    /* BUG-28791 : multi-thread �󿡼� connection�� ������������ �״� ����. */
    else
    {   /* sConnNodeP�� null�ΰ��� list�� ���� ó���� ����̴�.*/
        gUlpConnMgr.mConnHashTab.mTable[ sIndex ] = sConnNode->mNext;
    }

    gUlpConnMgr.mConnHashTab.mNumNode--;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock( &(gUlpConnMgr.mConnHashTab.mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
        sIsLatched = ACP_FALSE;
    }

    /* �޸� ����.*/
    ulpLibConFreeConnNode( sConnNode );

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACE_ASSERT ( acpThrRwlockUnlock( &(gUlpConnMgr.mConnHashTab.mLatch) )
                     == ACP_RC_SUCCESS );
    }

    return ACI_FAILURE;
}


void ulpLibConFreeConnNode( ulpLibConnNode *aConnNode )
{
/***********************************************************************
 *
 * Description :
 *    �־��� connection node�� �ڷᱸ������ ��� �����Ѵ�.
 *    stmt. hash node ���� -> latch ���� -> connection node ����
 * Implementation :
 *
 ***********************************************************************/

    acp_sint32_t sI;
    ulpErrorMgr  sErrorMgr;

    /** delete connection node **/

    /* delete hash nodes */

    /* delete stmt. and cursor nodes*/
    ulpLibStDelAllStmtCur( &(aConnNode->mStmtHashT),
                           &(aConnNode->mCursorHashT));
    /* delete unnamed stmt. nodes.*/
    ulpLibStDelAllUnnamedStmt( &(aConnNode->mUnnamedStmtCacheList) );

    /* destroy latchs */

    /* unnamed stmt. latch*/
    ACI_TEST_RAISE (
        acpThrRwlockDestroy( &(aConnNode->mUnnamedStmtCacheList.mLatch) )
        != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
    /* xa latch*/
    ACI_TEST_RAISE (
        acpThrRwlockDestroy( &(aConnNode->mLatch4Xa) ) != ACP_RC_SUCCESS,
        ERR_LATCH_DESTROY );
    /* stmt hash table latch*/
    for( sI=0 ; sI < MAX_NUM_STMT_HASH_TAB_ROW ; sI++ )
    {
        ACI_TEST_RAISE(
            acpThrRwlockDestroy( &(aConnNode->mStmtHashT.mTable[sI].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
    }
    /* cursor hash table latch*/
    for( sI=0 ; sI < MAX_NUM_STMT_HASH_TAB_ROW ; sI++ )
    {
        ACI_TEST_RAISE(
            acpThrRwlockDestroy( &(aConnNode->mCursorHashT.mTable[sI].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
    }

    /* TASK-7218 Handling Multiple Errors
     * thread local ���� gMultiErrorMgr�� �����Ϸ��� ConnNode�� mMultiErrorMgr��� gMultiErrorMgr �� NULL�� �ٲ���� �� */
    if ( &(aConnNode->mMultiErrorMgr) == ulpLibGetMultiErrorMgr() )
    {
        ulpLibSetMultiErrorMgr(NULL);
    }
    acpMemFree(aConnNode->mMultiErrorMgr.mErrorStack);

    /* FREE CONNECTION NODE */
    acpMemFree(aConnNode);

    return;

    ACI_EXCEPTION (ERR_LATCH_DESTROY);
    {
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Destroy_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &sErrorMgr);


        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    return;
}


void ulpLibConDelAllConn( void )
{
/***********************************************************************
 *
 * Description :
 *    hash table�� ��� ConnNode���� �����Ѵ�.
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    sI;
    ulpLibConnNode *sConnNode;
    ulpLibConnNode *sConnNodeN;

    for ( sI = 0; sI < MAX_NUM_CONN_HASH_TAB_ROW; sI++ )
    {
        sConnNode   = gUlpConnMgr.mConnHashTab.mTable[sI];
        sConnNodeN  = NULL;

        while (sConnNode != NULL)
        {
            if (sConnNode->mNext != NULL)
            {
                sConnNodeN = sConnNode->mNext;
                /* ConnNode �ڷᱸ�� ����.*/
                ulpLibConFreeConnNode( sConnNode );
                sConnNode = sConnNodeN;
            }
            else
            {
                /* ConnNode �ڷᱸ�� ����.*/
                ulpLibConFreeConnNode( sConnNode );
                sConnNode = NULL;
            }
        }
    }

    gUlpConnMgr.mConnHashTab.mNumNode = 0;
}
