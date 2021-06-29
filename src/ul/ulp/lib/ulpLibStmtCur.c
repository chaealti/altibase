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

#include <ulpLibStmtCur.h>
#include <ulpLibError.h>
#include <ulpLibInterCoreFuncB.h>

ulpLibStmtNode* ulpLibStNewNode(ulpSqlstmt* aSqlstmt, acp_char_t *aStmtName )
{
/***********************************************************************
 *
 * Description :
 *    ���ο� statement node�� �����.
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibStmtNode *sStmtNode;
    ulpErrorMgr     sErrorMgr;

    /** create new statement node **/
    /* alloc statement node */
    ACI_TEST_RAISE( 
        acpMemCalloc( (void**)&sStmtNode,
                      1,
                      ACI_SIZEOF(ulpLibStmtNode) ) != ACP_RC_SUCCESS,
        ERR_MEMORY_ALLOC );

    if( aStmtName != NULL )
    {
        ACI_TEST_RAISE( acpCStrLen(aStmtName, ACP_SINT32_MAX)
                        >= MAX_STMT_NAME_LEN, ERR_STMT_NAME_OVERFLOW );

        acpCStrCpy( sStmtNode->mStmtName,
                    MAX_ERRMSG_LEN,
                    aStmtName,
                    acpCStrLen(aStmtName, ACP_SINT32_MAX));
    }

    /* BUG-31467 : APRE should consider the thread safety of statement */
    /* initialize latchs */
    ACI_TEST_RAISE( 
        acpThrRwlockCreate( & (sStmtNode->mLatch),
                            ACP_THR_RWLOCK_DEFAULT ) != ACP_RC_SUCCESS,
        ERR_LATCH_INIT );

    sStmtNode->mStmtState = S_INITIAL;
    sStmtNode->mCurState  = C_INITIAL;

    /* BUG-45779 */
    sStmtNode->mUlpPSMArrInfo.mIsPSMArray = ACP_FALSE;
    acpListInit( &(sStmtNode->mUlpPSMArrInfo.mPSMArrDataInfoList) );

    return sStmtNode;

    ACI_EXCEPTION (ERR_MEMORY_ALLOC);
    {
        /* bug-36991: assertion codes in ulpLibStNewNode is not good */
        ulpSetErrorCode( &sErrorMgr,
                ulpERR_ABORT_Memory_Alloc_Failed, "ulpLibStNewNode");

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                aSqlstmt->sqlcodeerr,
                aSqlstmt->sqlstateerr,
                ulpGetErrorMSG(&sErrorMgr),
                ulpGetErrorCODE(&sErrorMgr),
                ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION (ERR_STMT_NAME_OVERFLOW);
    {
        acpMemFree(sStmtNode);

        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_COMP_Exceed_Max_Stmtname_Error,
                         aSqlstmt->stmtname );
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION (ERR_LATCH_INIT);
    {
        acpMemFree(sStmtNode);

        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Latch_Init_Error,
                         __LINE__,
                         __FILE__ );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION_END;

    return NULL;
}

ulpLibStmtNode* ulpLibStLookupStmt( ulpLibStmtHASHTAB *aStmtHashT, acp_char_t *aStmtName )
{
/***********************************************************************
 *
 * Description :
 *    stmt hash table���� �ش� stmt�̸����� stmt node�� ã�´�.
 *
 *    ã�� ���ϸ� NULL ����.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    sIndex;
    ulpLibStmtNode *sStmtNode;
    acp_bool_t      sIsLatched;

    sIsLatched = ACP_FALSE;

    sIndex = (*aStmtHashT->mHash)( (acp_uint8_t *)aStmtName ) % ( aStmtHashT->mSize );

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get read lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockLockRead( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_READ );
        sIsLatched = ACP_TRUE;
    }

    /* hash table is empty */
    ACI_TEST( aStmtHashT->mNumNode == 0 );

    sStmtNode = aStmtHashT->mTable[ sIndex ].mList;

    while ( ( sStmtNode != NULL ) &&
            ( *aStmtHashT->mCmp )( aStmtName, sStmtNode->mStmtName,
                                   MAX_STMT_NAME_LEN-1 )
          )
    {
        sStmtNode = sStmtNode->mNextStmt;
    }

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
        sIsLatched = ACP_FALSE;
    }

    return sStmtNode;

    ACI_EXCEPTION (ERR_LATCH_READ);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Read_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return NULL;
}

ulpLibStmtNode* ulpLibStAddStmt( ulpLibStmtHASHTAB *aStmtHashT, ulpLibStmtNode *aStmtNode )
{
/***********************************************************************
 *
 * Description :
 *    stmt hash table�� �ش� stmt�̸����� stmt node�� �߰��Ѵ�.
 *    List�� ���� �ڿ� �Ŵܴ�.
 *
 * Implementation :
 *    1. stmt �̸����� hash table���� stmt node�� ã�´�.
 *    2. ã�����ϸ� �� stmt node�� ����� hash table�� �߰��Ѵ�.
 *
 ***********************************************************************/
    acp_sint32_t    sIndex;
    ulpLibStmtNode *sStmtNode;
    ulpLibStmtNode *sStmtNodeP;
    acp_char_t     *sStmtName;
    acp_bool_t      sIsLatched;

    sStmtName  = aStmtNode->mStmtName;
    sIsLatched = ACP_FALSE;
    sStmtNodeP = NULL;

    /** find statement **/
    sIndex = (*aStmtHashT->mHash)( (acp_uint8_t *)sStmtName ) % ( aStmtHashT->mSize ) ;
    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockWrite( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_WRITE );

        sIsLatched = ACP_TRUE;
    }

    sStmtNode = aStmtHashT->mTable[ sIndex ].mList;
    while ( ( sStmtNode != NULL ) &&
            ( *aStmtHashT->mCmp )( sStmtName, sStmtNode->mStmtName,
                                   MAX_STMT_NAME_LEN-1 )
          )
    {
        sStmtNodeP = sStmtNode;
        sStmtNode  = sStmtNode->mNextStmt;
    }
    /* already exist -> return NULL*/
    ACI_TEST( sStmtNode != NULL );

    if ( sStmtNodeP == NULL )
    {
        aStmtHashT->mTable[ sIndex ].mList = aStmtNode;
    }
    else
    {
        sStmtNodeP->mNextStmt = aStmtNode;
    }

    (aStmtHashT->mNumNode)++;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockUnlock ( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    return aStmtNode;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockUnlock ( &(aStmtHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }
    return NULL;
}


void ulpLibStDelAllStmtCur( ulpLibStmtHASHTAB *aStmtTab,
                            ulpLibStmtHASHTAB *aCurTab )
{
/***********************************************************************
 *
 * Description :
 *    statement/cursor hash table���� ��� stmt node���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibStmtNode *sStmtNode;
    ulpLibStmtNode *sStmtNodeN;
    ulpErrorMgr     sErrorMgr;
    acp_sint32_t    i;

    /* BUG-26370 [valgrind error] :  cursor hash table�� �����ؾ��Ѵ�. */
    if( aCurTab->mNumNode == 0 )
    {
        /* Do nothing */
    }
    else
    {
        for ( i = 0 ; i < aCurTab->mSize ; i++ )
        {
            sStmtNode = aCurTab->mTable[ i ].mList;
            while ( sStmtNode != NULL )
            {
                if ( sStmtNode->mStmtName[0] == '\0' )
                {
                    /* BUG-45779 */
                    (void)ulpPSMArrayMetaFree(&sStmtNode->mUlpPSMArrInfo);

                    /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
                    acpMemFree( sStmtNode->mQueryStr );
                    acpMemFree( sStmtNode->mInHostVarPtr );
                    acpMemFree( sStmtNode->mOutHostVarPtr );
                    acpMemFree( sStmtNode->mExtraHostVarPtr );
                    /* BUG-31467 : APRE should consider the thread safety of statement */
                    ACI_TEST_RAISE ( acpThrRwlockDestroy( &(sStmtNode->mLatch) )
                                     != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
                    sStmtNodeN = sStmtNode -> mNextCur;
                    acpMemFree( sStmtNode );
                    sStmtNode = sStmtNodeN;
                }
                else
                {
                    sStmtNode = sStmtNode -> mNextCur;
                }
            }
            aCurTab->mTable[ i ].mList = NULL;
        }
        aCurTab->mNumNode = 0;
    }

    if( aStmtTab->mNumNode == 0 )
    {
        /* Do nothing */
    }
    else
    {
        for ( i = 0 ; i < aStmtTab->mSize ; i++ )
        {
            sStmtNode = aStmtTab->mTable[ i ].mList;
            while ( sStmtNode != NULL )
            {
                /* BUG-45779 */
                (void)ulpPSMArrayMetaFree(&sStmtNode->mUlpPSMArrInfo);

                /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
                acpMemFree( sStmtNode->mQueryStr );
                acpMemFree( sStmtNode->mInHostVarPtr );
                acpMemFree( sStmtNode->mOutHostVarPtr );
                acpMemFree( sStmtNode->mExtraHostVarPtr );
                /* BUG-31467 : APRE should consider the thread safety of statement */
                ACI_TEST_RAISE ( acpThrRwlockDestroy( &(sStmtNode->mLatch) )
                                 != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
                sStmtNodeN = sStmtNode -> mNextStmt;
                acpMemFree( sStmtNode );
                sStmtNode = sStmtNodeN;
            }
            aStmtTab->mTable[ i ].mList = NULL;
        }
        aStmtTab->mNumNode = 0;
    }

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


ulpLibStmtNode* ulpLibStLookupCur( ulpLibStmtHASHTAB *aCurHashT, acp_char_t *aCurName )
{
/***********************************************************************
 *
 * Description :
 *    cursor hash table���� �ش� cursor�̸����� cursor node�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    sIndex;
    ulpLibStmtNode *sStmtNode;
    acp_bool_t      sIsLatched;

    sIsLatched = ACP_FALSE;

    sIndex = (*aCurHashT->mHash)( (acp_uint8_t *)aCurName ) % ( aCurHashT->mSize );

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get read lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockRead( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_READ );

        sIsLatched = ACP_TRUE;
    }

    /* hash table is empty */
    ACI_TEST( aCurHashT->mNumNode == 0 );

    sStmtNode = aCurHashT->mTable[ sIndex ].mList;
    while ( ( sStmtNode != NULL ) &&
            ( *aCurHashT->mCmp )( aCurName, sStmtNode->mCursorName,
                                  MAX_CUR_NAME_LEN-1 )
          )
    {
        sStmtNode = sStmtNode->mNextCur;
    }

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    ACI_TEST( sStmtNode == NULL );

    return sStmtNode;

    ACI_EXCEPTION (ERR_LATCH_READ);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Read_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return NULL;
}


ACI_RC ulpLibStAddCurLink( ulpLibStmtHASHTAB *aCurHashT,
                           ulpLibStmtNode    *aStmtNode,
                           acp_char_t        *aCurName )
{
/***********************************************************************
 *
 * Description :
 *    cursor hash table�� bucket list �������� �ش� stmt node�� ���� ��link�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t         sIndex;
    ulpLibStmtNode      *sStmtNode;
    ulpLibStmtNode      *sStmtNodeP;
    acp_bool_t           sIsLatched;

    sIsLatched = ACP_FALSE;
    sStmtNodeP = NULL;

    /* get hash key */
    sIndex = (*aCurHashT->mHash)( (acp_uint8_t *)aCurName ) % ( aCurHashT->mSize ) ;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE (
            acpThrRwlockLockWrite( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_WRITE );

        sIsLatched = ACP_TRUE;
    }

    sStmtNode = aCurHashT->mTable[ sIndex ].mList;
    /* bucket list�� �������� ã�´�. */
    while ( sStmtNode != NULL )
    {
        sStmtNodeP = sStmtNode;
        sStmtNode  = sStmtNode->mNextCur;
    }

    if( sStmtNodeP == NULL )
    {
        aCurHashT->mTable[ sIndex ].mList = aStmtNode;
    }
    else
    {
        sStmtNodeP->mNextCur = aStmtNode;
    }

    /* cusor �̸� ���� */
    acpSnprintf(aStmtNode->mCursorName, MAX_CUR_NAME_LEN, aCurName);

    (aCurHashT->mNumNode)++;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE (
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return ACI_FAILURE;
}


ACI_RC ulpLibStDeleteCur( ulpLibStmtHASHTAB *aCurHashT, acp_char_t *aCurName )
{
/***********************************************************************
 *
 * Description :
 *    cursor hash table�� �ش� cursor�̸��� ���� cursor node�� �����Ѵ�.
 *    statement �̸��� ������� link�� �����ϸ�, ���� ��쿡�� stmt node��ü�� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    sIndex;
    ulpLibStmtNode *sStmtNode;
    ulpLibStmtNode *sStmtNodeP;
    acp_bool_t      sIsLatched;

    sIsLatched = ACP_FALSE;
    sStmtNodeP = NULL;

    /** 1. find cursor **/
    sIndex = (*aCurHashT->mHash)( (acp_uint8_t *)aCurName ) % ( aCurHashT->mSize ) ;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockLockWrite( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_WRITE );

        sIsLatched = ACP_TRUE;
    }

    sStmtNode = aCurHashT->mTable[ sIndex ].mList;
    while ( ( sStmtNode != NULL ) &&
            ( *aCurHashT->mCmp )( aCurName, sStmtNode->mCursorName,
                                  MAX_CUR_NAME_LEN-1 )
          )
    {
        sStmtNodeP = sStmtNode;
        sStmtNode  = sStmtNode->mNextCur;
    }
    /* can't find -> return ACI_FAILURE */
    ACI_TEST( sStmtNode == NULL );

    /** 2. delete or unlink cursor **/
    if( sStmtNode->mStmtName[0] == '\0' )
    {   /* delete */
        if ( sStmtNodeP == NULL )
        {
            aCurHashT->mTable[ sIndex ].mList = sStmtNode->mNextCur;
        }
        else
        {
            sStmtNodeP->mNextCur = sStmtNode->mNextCur;
        }

        /* BUG-45779 */
        (void)ulpPSMArrayMetaFree(&sStmtNode->mUlpPSMArrInfo);

        /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
        acpMemFree( sStmtNode->mQueryStr );
        acpMemFree( sStmtNode->mInHostVarPtr );
        acpMemFree( sStmtNode->mOutHostVarPtr );
        acpMemFree( sStmtNode->mExtraHostVarPtr );
        acpMemFree( sStmtNode );

    }
    else
    {   /* unlink */
        if ( sStmtNodeP == NULL )
        {
            aCurHashT->mTable[ sIndex ].mList = sStmtNode->mNextCur;
        }
        else
        {
            sStmtNodeP->mNextCur = sStmtNode->mNextCur;
        }
        sStmtNode ->mCursorName[0] = '\0';
        sStmtNode ->mNextCur = NULL;
    }

    (aCurHashT->mNumNode)--;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( 
            acpThrRwlockUnlock ( &(aCurHashT->mTable[sIndex].mLatch) )
            != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return ACI_FAILURE;
}


ulpLibStmtNode *ulpLibStLookupUnnamedStmt( ulpLibStmtLIST *aStmtList, acp_char_t *aQuery )
{
/***********************************************************************
 *
 * Description :
 *    unnamed stmt list���� �ش� query�� stmt node�� ã�´�.
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibStmtNode *sStmtNode;
    acp_bool_t      sIsLatched;

    sIsLatched = ACP_FALSE;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get read lock */
        ACI_TEST_RAISE ( acpThrRwlockLockRead( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_READ );

        sIsLatched = ACP_TRUE;
    }

    /* unnamed list is empty */
    ACI_TEST( aStmtList->mNumNode == 0 );

    sStmtNode = aStmtList->mList;
    while ( sStmtNode != NULL )
    {
        /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
        if( sStmtNode->mQueryStr != NULL )
        {
            if ( acpCStrCmp( aQuery, sStmtNode->mQueryStr,
                             sStmtNode->mQueryStrLen ) == 0 )
            {
                break;
            }
        }
        sStmtNode = sStmtNode->mNextStmt;
    }

    ACI_TEST( sStmtNode == NULL );

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE ( acpThrRwlockUnlock ( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    return sStmtNode;

    ACI_EXCEPTION (ERR_LATCH_READ);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Read_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release read lock */
        ACI_TEST_RAISE ( acpThrRwlockUnlock ( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return NULL;
}


ACI_RC ulpLibStAddUnnamedStmt( ulpLibStmtLIST *aStmtList, ulpLibStmtNode *aStmtNode )
{
/***********************************************************************
 *
 * Description :
 *    unnamed stmt list�� �� stmt node�� �߰��Ѵ�.
 *    list ���� �տ� �߰�.
 * Implementation :
 *
 ***********************************************************************/
    ulpLibStmtNode *sStmtNode;
    ulpLibStmtNode *sStmtNodeP;
    acp_bool_t      sIsLatched;

    sStmtNodeP = NULL;
    sIsLatched = ACP_FALSE;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        /* get write lock */
        ACI_TEST_RAISE ( acpThrRwlockLockWrite( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_WRITE );

        sIsLatched = ACP_TRUE;
    }

    if ( aStmtList->mNumNode < aStmtList->mSize )
    {   /* ���� list�� ������ list�� �������ִ� �ִ������ ������, add front.*/
        if( aStmtList->mList == NULL )
        {
            aStmtList->mList = aStmtNode;
        }
        else
        {
            aStmtNode->mNextStmt = aStmtList->mList;
            aStmtList->mList = aStmtNode;
        }
        (aStmtList->mNumNode)++;
    }
    else
    {   /* ���� list�� ������ list�� �������ִ� �ִ������ ũ��,*/
        /* ���� �� ��带 �����Ѵ��� add front.*/
        sStmtNode = aStmtList->mList;
        while( sStmtNode != NULL )
        {
            if( sStmtNode->mNextStmt == NULL )
            {
                break;
            }
            sStmtNodeP= sStmtNode;
            sStmtNode = sStmtNode->mNextStmt;
        }

        if ( sStmtNode != NULL ) /* NULL�� ������ ������... */
        {
            /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
            acpMemFree( sStmtNode->mQueryStr );
            acpMemFree( sStmtNode->mInHostVarPtr );
            acpMemFree( sStmtNode->mOutHostVarPtr );
            acpMemFree( sStmtNode->mExtraHostVarPtr );
            acpMemFree( sStmtNode );
        }

        if ( sStmtNodeP != NULL )
        {
            sStmtNodeP->mNextStmt = NULL;
        }
        else /* ��������� ������... */
        {
            aStmtList->mList = NULL;
        }

        aStmtNode->mNextStmt = aStmtList->mList;
        aStmtList->mList = aStmtNode;
    }

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( acpThrRwlockUnlock ( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );

        sIsLatched = ACP_FALSE;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_LATCH_WRITE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Write_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION (ERR_LATCH_RELEASE);
    {
        ulpErrorMgr mErrorMgr;
        ulpSetErrorCode( &mErrorMgr,
                         ulpERR_ABORT_Latch_Release_Error,
                         __LINE__,
                         __FILE__ );
        ulpPrintfErrorCode( ACP_STD_ERR,
                            &mErrorMgr);
        ACE_ASSERT(0);
    }
    ACI_EXCEPTION_END;

    if( sIsLatched == ACP_TRUE )
    {
        /* release write lock */
        ACI_TEST_RAISE ( acpThrRwlockUnlock ( &(aStmtList->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    return ACI_FAILURE;
}


void ulpLibStDelAllUnnamedStmt( ulpLibStmtLIST *aStmtList )
{
/***********************************************************************
 *
 * Description :
 *    unnamed stmt list�� �Ŵ޸� ��� stmt node���� �����Ѵ�.
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibStmtNode *sStmtNode;
    ulpLibStmtNode *sStmtNodeN;
    ulpErrorMgr     sErrorMgr;

    sStmtNode = aStmtList->mList;
    while( sStmtNode != NULL )
    {
        sStmtNodeN = sStmtNode->mNextStmt;

        /* BUG-45779 */
        (void)ulpPSMArrayMetaFree(&sStmtNode->mUlpPSMArrInfo);

        /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
        acpMemFree( sStmtNode->mQueryStr );
        acpMemFree( sStmtNode->mInHostVarPtr );
        acpMemFree( sStmtNode->mOutHostVarPtr );
        acpMemFree( sStmtNode->mExtraHostVarPtr );
        /* BUG-31467 : APRE should consider the thread safety of statement */
        ACI_TEST_RAISE ( acpThrRwlockDestroy( &(sStmtNode->mLatch) )
                         != ACP_RC_SUCCESS, ERR_LATCH_DESTROY );
        acpMemFree( sStmtNode );
        sStmtNode = sStmtNodeN;
    }
    aStmtList->mList = NULL;
    aStmtList->mNumNode = 0;

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
