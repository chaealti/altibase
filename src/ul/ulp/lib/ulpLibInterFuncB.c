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

#include <ulpLibMultiErrorMgr.h>
#include <ulpLibInterFuncB.h>

/* XA���� �ڵ��� �ʱ�ȭ �ƴ����� �Ǵ��ϱ����� �ʿ���. */
extern acp_bool_t gUlpLibDoInitProc;

/********************************************************************************
 *  Description :
 *     select�� ���� ������ ��ȸ�� �����͸� ������ host variable�� bind�Ѵ�.
 *     dynamic,  static ��Ŀ� ���� �б�ȴ�.
 *
 *  Parameters : ���������� ���� ���ڸ� ������, �Լ����� ��� ���ڰ� �߰�/���ŵ� �� �ִ�.
 *     ulpLibConnNode *aConnNode   : env�ڵ�, dbc�ڵ� �׸��� ���� ó���� ���� ����.
 *     ulpLibStmtNode *aStmtNode   : statement ���� ������ �����Ѵ�.
 *     SQLHSTMT    *aHstmt         : CLIȣ���� ���� statement �ڵ�.
 *     acp_bool_t   aIsReBindCheck : rebind ���� ����.
 *     ulpSqlca    *aSqlca         : ���� ó���� ���� ����.
 *     ulpSqlcode  *aSqlcode       : ���� ó���� ���� ����.
 *     ulpSqlstate *aSqlstate      : ���� ó���� ���� ����.
 * ******************************************************************************/
ACI_RC ulpBindCol(ulpLibConnNode *aConnNode,    
                  ulpLibStmtNode *aStmtNode,  
                  SQLHSTMT    *aHstmt,        
                  ulpSqlstmt  *aSqlstmt,      
                  acp_bool_t   aIsReBindCheck,
                  ulpSqlca    *aSqlca,        
                  ulpSqlcode  *aSqlcode,      
                  ulpSqlstate *aSqlstate )    

{
    SQLDA *sBinda = NULL;
    /* BUG-41010 dynamic binding */
    if ( IS_DYNAMIC_VARIABLE(aSqlstmt) )
    {
        sBinda = (SQLDA*) aSqlstmt->hostvalue[0].mHostVar;

        ACI_TEST( ulpDynamicBindCol( aConnNode,
                                     aStmtNode,
                                     aHstmt,
                                     aSqlstmt,
                                     sBinda,
                                     aSqlca,
                                     aSqlcode,
                                     aSqlstate )
                  == ACI_FAILURE );

        ACI_TEST( ulpSetStmtAttrDynamicRowCore( aConnNode,
                                                aStmtNode,
                                                aHstmt,
                                                aSqlca,
                                                aSqlcode,
                                                aSqlstate )
                  == ACI_FAILURE );
    }
    else
    {
        if ( aIsReBindCheck == ACP_TRUE )
        {
            if ( ulpCheckNeedReBindColCore( aStmtNode, aSqlstmt ) == ACP_TRUE )
            {
                ACI_TEST( ulpBindHostVarCore( aConnNode,
                                              aStmtNode,
                                              aHstmt,
                                              aSqlstmt,
                                              aSqlca,
                                              aSqlcode,
                                              aSqlstate )
                          == ACI_FAILURE );

                if ( ulpCheckNeedReSetStmtAttrCore( aStmtNode, aSqlstmt ) == ACP_TRUE )
                {
                    ACI_TEST( ulpSetStmtAttrRowCore( aConnNode,
                                                     aStmtNode,
                                                     aHstmt,
                                                     aSqlstmt,
                                                     aSqlca,
                                                     aSqlcode,
                                                     aSqlstate )
                              == ACI_FAILURE );
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
        }
        else
        {
            ACI_TEST( ulpBindHostVarCore( aConnNode,
                                          aStmtNode,
                                          aHstmt,
                                          aSqlstmt,
                                          aSqlca,
                                          aSqlcode,
                                          aSqlstate )
                      == ACI_FAILURE );

            ACI_TEST( ulpSetStmtAttrRowCore( aConnNode,
                                             aStmtNode,
                                             aHstmt,
                                             aSqlstmt,
                                             aSqlca,
                                             aSqlcode,
                                             aSqlstate )
                      == ACI_FAILURE );
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpOpen ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    ����� Ŀ�� open ó�� �����.
 *
 *    ó������> EXEC SQL OPEN <cursor_name> [ USING <in_host_var_list> ];
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    ulpLibStmtNode *sStmtNode;

    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    sStmtNode = ulpLibStLookupCur( &( sConnNode->mCursorHashT )
                                   , aSqlstmt->curname );

    ACI_TEST_RAISE( sStmtNode == NULL, ERR_NO_CURSOR );

    ulpSetColRowSizeCore( aSqlstmt );

    if( sStmtNode->mCurState >= C_DECLARE )
    {
        switch ( sStmtNode->mStmtState )
        {
            case S_PREPARE:
            case S_CLOSE:
                /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
                if ( sStmtNode->mNeedReprepare == ACP_TRUE ) /* BUG-47239  mFailoveredJustnow ���� */
                {
                    // do prepare
                    ACI_TEST_RAISE( ulpPrepareCore  ( sConnNode,
                                                      sStmtNode,
                                                      sStmtNode-> mQueryStr,
                                                      aSqlstmt -> sqlcaerr,
                                                      aSqlstmt -> sqlcodeerr,
                                                      aSqlstmt -> sqlstateerr )
                                    == ACI_FAILURE, ERR_PREPARE_CORE );

                    sStmtNode->mNeedReprepare = ACP_FALSE;
                }

                /*host ������ ������ binding & setstmt���ش�.*/
                if( aSqlstmt->numofhostvar > 0 )
                {
                    /* BUG-41010 dynamic binding */
                    ACI_TEST( ulpBindParam ( sConnNode,
                                             sStmtNode,
                                             &(sStmtNode->mHstmt),
                                             aSqlstmt,
                                             ACP_FALSE,
                                             aSqlstmt->sqlcaerr,
                                             aSqlstmt->sqlcodeerr,
                                             aSqlstmt->sqlstateerr ) == ACI_FAILURE );
                }
                break;
            case S_BINDING:
            case S_SETSTMTATTR:
            case S_EXECUTE:
                /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
                if ( sStmtNode->mNeedReprepare == ACP_TRUE ) /* BUG-47239  mFailoveredJustnow ���� */
                {
                    // do prepare
                    ACI_TEST_RAISE( ulpPrepareCore  ( sConnNode,
                                                      sStmtNode,
                                                      sStmtNode-> mQueryStr,
                                                      aSqlstmt -> sqlcaerr,
                                                      aSqlstmt -> sqlcodeerr,
                                                      aSqlstmt -> sqlstateerr )
                                    == ACI_FAILURE, ERR_PREPARE_CORE );

                    sStmtNode->mNeedReprepare = ACP_FALSE;
                }
                else
                {
                    /* BUG-43716 ���Ǹ� ���� close ���� �ٽ� open�ϴ°� ��� */
                    (void) ulpCloseStmtCore(sConnNode,
                                            sStmtNode,
                                            &(sStmtNode->mHstmt),
                                            aSqlstmt->sqlcaerr,
                                            aSqlstmt->sqlcodeerr,
                                            aSqlstmt->sqlstateerr);
                }

                /* �ʿ��ϴٸ� binding & setstmt �� �ٽ����ش�.*/
                if( aSqlstmt->numofhostvar > 0 )
                {
                    /* BUG-41010 dynamic binding */
                    ACI_TEST( ulpBindParam ( sConnNode,
                                             sStmtNode,
                                             &(sStmtNode->mHstmt),
                                             aSqlstmt,
                                             ACP_TRUE,
                                             aSqlstmt->sqlcaerr,
                                             aSqlstmt->sqlcodeerr,
                                             aSqlstmt->sqlstateerr ) == ACI_FAILURE );
                }
                break;
            default: /*S_INITIAL*/
                /* PREPARE�� ���� �ؾ��Ѵ�.*/
                ACI_RAISE( ERR_STMT_NEED_PREPARE_4EXEC );
                break;
        }
    }
    else
    {
        ACI_RAISE( ERR_CUR_NEED_DECL_4OPEN );
    }

    ACI_TEST( ulpExecuteCore( sConnNode,
                              sStmtNode,
                              aSqlstmt,
                              &(sStmtNode->mHstmt) ) == ACI_FAILURE );

    sStmtNode -> mStmtState = S_EXECUTE;
    sStmtNode -> mCurState  = C_OPEN;

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_NO_CURSOR );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Not_Exist_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_NO_CURSOR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_STMT_NEED_PREPARE_4EXEC );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Stmt_Need_Prepare_4Execute_Error,
                         sStmtNode->mStmtName );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CUR_NEED_DECL_4OPEN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Need_Declare_4Open_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_PREPARE_CORE );
    {
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpFetch ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    Open�� Ŀ���� ���� fetch ó�� �����.
 *
 *    ó������> FETCH [ FIRST| PRIOR|NEXT|LAST|CURRENT | RELATIVE fetch_offset
 *            | ABSOLUTE fetch_offset ] <cursor_name> INTO <host_variable_list>;
 *
 * Implementation :
 *
 ***********************************************************************/
    acp_sint32_t    i;
    ulpLibConnNode *sConnNode;
    ulpLibStmtNode *sStmtNode;
    SQLRETURN       sSqlRes = SQL_ERROR;
    SQLUINTEGER     sNumFetched;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    sStmtNode = ulpLibStLookupCur( &( sConnNode->mCursorHashT )
                                   , aSqlstmt->curname );

    ACI_TEST_RAISE( sStmtNode == NULL, ERR_NO_CURSOR );

    ulpSetColRowSizeCore( aSqlstmt );

    /* FOR�� ó��*/
    ACI_TEST( ulpAdjustArraySize(aSqlstmt) == ACI_FAILURE );

    if( (sStmtNode->mCurState == C_OPEN) || (sStmtNode->mCurState == C_FETCH) )
    {
        switch ( sStmtNode->mStmtState )
        {
            case S_EXECUTE:
            case S_SETSTMTATTR:
            case S_BINDING:
                /*host ������ ������ binding & setstmt���ش�.*/
                if( aSqlstmt->numofhostvar > 0 )
                {
                    /* BUG-41010 dynamic binding */
                    ACI_TEST( ulpBindCol(sConnNode,    
                                         sStmtNode,  
                                         &(sStmtNode->mHstmt),        
                                         aSqlstmt,      
                                         ACP_TRUE,
                                         aSqlstmt->sqlcaerr,        
                                         aSqlstmt->sqlcodeerr,      
                                         aSqlstmt->sqlstateerr )
                              == ACI_FAILURE );
                }
                break;
            default: 
                /* S_INITIAL, S_PREPARE, S_INITIAL*/
                /* EXECUTE�� ���� �ؾ��Ѵ�.*/
                ACI_RAISE( ERR_STMT_NEED_EXECUTE_4FETCH );
                break;
        }
    }
    else
    {
        ACI_RAISE( ERR_CUR_NEED_OPEN_4FETCH );
    }

    if ( sStmtNode -> mCurState  != C_FETCH )
    {
        sSqlRes = SQLSetStmtAttr( sStmtNode->mHstmt,
                                  SQL_ATTR_ROWS_FETCHED_PTR,
                                  (SQLPOINTER) &(sStmtNode->mRowsFetched),
                                  0 );
        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_SETSTMT );
    }

    /* BUG-45448 FETCH������ FOR������ ����ǵ��� SQL_ATTR_ROW_ARRAY_SIZE �Ź� �����Ѵ� */
    if(aSqlstmt->arrsize > 0)
    {
        sSqlRes = SQLSetStmtAttr( sStmtNode->mHstmt,
                                  SQL_ATTR_ROW_ARRAY_SIZE,
                                  (SQLPOINTER)(acp_slong_t)aSqlstmt->arrsize,
                                  0 );
    }

    switch( aSqlstmt -> scrollcur )
    {
        case F_NONE:
            sSqlRes = SQLFetch( sStmtNode->mHstmt );
            break;
        case F_FIRST:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_FIRST, 0 );
            break;
        case F_PRIOR:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_PRIOR, 0 );
            break;
        case F_NEXT:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_NEXT, 0 );
            break;
        case F_LAST:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_LAST, 0 );
            break;
        case F_CURRENT:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_RELATIVE, 0 );
            break;
        case F_RELATIVE:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_RELATIVE
                                      , aSqlstmt -> sqlinfo );
            break;
        case F_ABSOLUTE:
            sSqlRes = SQLFetchScroll( sStmtNode->mHstmt, SQL_FETCH_ABSOLUTE
                                      , aSqlstmt -> sqlinfo );
            break;
        default:
            ACE_ASSERT(0);
            break;
    }

    /* varchar �� ȣ��Ʈ ������ ����ϸ� ����ڰ� indicator�� ��� ���������,*/
    /* fetch ������ indicator��� ���� varchar.len �� ������ �����.*/
    for( i = 0 ; aSqlstmt->numofhostvar > i ; i++ )
    {
        if( ((aSqlstmt->hostvalue[i].mType == H_VARCHAR) ||
            (aSqlstmt->hostvalue[i].mType  == H_NVARCHAR)) &&
            (aSqlstmt->hostvalue[i].mVcInd != NULL) )
        {
            *(aSqlstmt->hostvalue[i].mVcInd) = *(aSqlstmt->hostvalue[i].mHostInd);
        }
    }

    ulpSetErrorInfo4CLI( sConnNode,
                         sStmtNode->mHstmt,
                         sSqlRes,
                         aSqlstmt->sqlcaerr,
                         aSqlstmt->sqlcodeerr,
                         aSqlstmt->sqlstateerr,
                         ERR_TYPE1 );

    /*---------------------------------------------------------------*/
    /* if "22002" error(null fetched without indicator) occured and unsafe_null is true,*/
    /*  we allow null fetch without indicator and return SQL_SUCCESS*/
    /*---------------------------------------------------------------*/
    if( (acpCStrCmp( *(aSqlstmt->sqlstateerr), SQLSTATE_UNSAFE_NULL,
                     MAX_ERRSTATE_LEN-1 ) == 0) &&
        (ulpGetStmtOption( aSqlstmt,
                           ULP_OPT_UNSAFE_NULL ) == ACP_TRUE) )
    {
        sSqlRes = SQL_SUCCESS;
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               SQLMSG_SUCCESS,
                               sSqlRes,
                               SQLSTATE_SUCCESS );
    }

    sNumFetched = sStmtNode->mRowsFetched;
    aSqlstmt->sqlcaerr->sqlerrd[2] = sNumFetched;

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FETCH );

    /* Ŀ�� ���¸� FETCH�� �ٲ�;*/
    sStmtNode -> mCurState  = C_FETCH;

    return ACI_SUCCESS;

    /* fetch�� row���� sqlca->sqlerrd[2] �� �����ؾ��ϴµ� ��� ó���ؾ��ϳ�? */

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_NO_CURSOR );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Not_Exist_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_NO_CURSOR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_STMT_NEED_EXECUTE_4FETCH );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Stmt_Need_Execute_4Fetch_Error,
                         sStmtNode->mStmtName );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CUR_NEED_OPEN_4FETCH );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Need_Open_4Fetch_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_SETSTMT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             sStmtNode->mHstmt,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION ( ERR_CLI_FETCH );
    {

    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpClose ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    open�� Ŀ���� ���� closeó�� �����. statement�� drop���� �ʰ� close�Ѵ�. ��Ȱ�� ����.
 *
 *    ó������> EXEC SQL CLOSE <cursor_name>;
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    ulpLibStmtNode *sStmtNode;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    sStmtNode = ulpLibStLookupCur( &( sConnNode->mCursorHashT )
                                   , aSqlstmt->curname );

    ACI_TEST_RAISE( sStmtNode == NULL, ERR_NO_CURSOR );

    ACI_TEST( ulpCloseStmtCore(    sConnNode,
                                   sStmtNode,
                                   &(sStmtNode-> mHstmt),
                                   aSqlstmt -> sqlcaerr,
                                   aSqlstmt -> sqlcodeerr,
                                   aSqlstmt -> sqlstateerr )
              == ACI_FAILURE );

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_NO_CURSOR );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Not_Exist_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_NO_CURSOR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpCloseRelease ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    open�� Ŀ���� ���� releaseó�� �����. statement�� drop�ϸ�, �ش� cursor hash node��
 *    �����Ѵ�.
 *
 *    ó������> EXEC SQL CLOSE RELEASE <cursor_name>;
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    ulpLibStmtNode *sStmtNode;
    SQLRETURN       sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    sStmtNode = ulpLibStLookupCur( &( sConnNode->mCursorHashT )
                                   , aSqlstmt->curname );

    ACI_TEST_RAISE( sStmtNode == NULL, ERR_NO_CURSOR );

    if( sStmtNode->mStmtName[0] == '\0' )
    {
        sSqlRes = SQLFreeStmt( sStmtNode->mHstmt, SQL_DROP );
        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_FREE_STMT );
    }
    else
    {
        /* stmt �̸��� ��� �Ǿ� ������ ���߿� ���� �� �ֱ⶧���� close�� �Ѵ�.*/
        ACI_TEST( ulpCloseStmtCore( sConnNode,
                                    sStmtNode,
                                    &(sStmtNode-> mHstmt),
                                    aSqlstmt -> sqlcaerr,
                                    aSqlstmt -> sqlcodeerr,
                                    aSqlstmt -> sqlstateerr )
                  == ACI_FAILURE );
    }

    /* cursor hash table ���� �ش� cursor node�� �����Ѵ�. ( link�� ���� or node��ü�� ���� )*/
    ulpLibStDeleteCur( &(sConnNode->mCursorHashT), aSqlstmt->curname );

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_NO_CURSOR );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Cursor_Not_Exist_Error,
                         aSqlstmt->curname );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_NO_CURSOR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_FREE_STMT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             sStmtNode->mHstmt,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );

    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC ulpDeclareStmt ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    ���ο� statement�� ����ó�� �����. stmt hash table�� �� node �߰���.
 *    statement�� �̹� �����ϸ� �ƹ��� ó���� ���� �ʴ´�.
 *
 *    ó������> EXEC SQL DECLARE <statement_name> STATEMENT;
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    ulpLibStmtNode *sStmtNode;
    SQLRETURN       sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    /* �̹� �ش� stmtname���� stmt node�� �����ϴ��� ã��*/
    sStmtNode = ulpLibStLookupStmt( &( sConnNode->mStmtHashT ),
                                    aSqlstmt->stmtname );
    /* �����ϸ� do nothing*/
    if( sStmtNode != NULL )
    {
        /* do nothing */
    }
    else
    {
        /* �� stmt ��� �Ҵ�*/
        sStmtNode = ulpLibStNewNode(aSqlstmt, aSqlstmt->stmtname );
        ACI_TEST( sStmtNode == NULL);

        /* AllocStmt ����.*/
        sSqlRes = SQLAllocStmt( sConnNode -> mHdbc, &( sStmtNode -> mHstmt ) );

        ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                        , ERR_CLI_ALLOC_STMT);

        /* stmt node�� hash table�� �߰�.*/
        ACI_TEST_RAISE ( ulpLibStAddStmt( &(sConnNode->mStmtHashT),
                                          sStmtNode ) == NULL,
                         ERR_STMT_ADDED_JUST_BEFORE );
    }

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_ALLOC_STMT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION ( ERR_STMT_ADDED_JUST_BEFORE );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr, ulpERR_ABORT_Conflict_Two_Emsqls_Error );

        SQLFreeStmt( sStmtNode -> mHstmt, SQL_DROP );
        acpMemFree( sStmtNode );

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpAutoCommit ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *
 *    ó������> EXEC SQL AUTOCOMMIT { ON | OFF };
 *
 * Implementation :
 *
 ***********************************************************************/

    ulpLibConnNode *sConnNode;
    SQLRETURN       sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    if ( aSqlstmt->sqlinfo == 0 ) /* AUTOCOMMIT OFF*/
    {
        sSqlRes = SQLSetConnectAttr ( sConnNode->mHdbc, SQL_ATTR_AUTOCOMMIT,
                                      (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0 );
    }
    else                          /* AUTOCOMMIT ON*/
    {
        sSqlRes = SQLSetConnectAttr ( sConnNode->mHdbc, SQL_ATTR_AUTOCOMMIT,
                                      (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0 );
    }

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_SETCONNATTR_4AUTOCOMMIT );

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_SETCONNATTR_4AUTOCOMMIT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             sSqlRes,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpCommit ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *
 *    ó������> EXEC SQL <COMMIT|ROLLBACK> [WORK] [RELEASE];
 *
 * Implementation :
 *
 ***********************************************************************/

    ulpLibConnNode *sConnNode;
    SQLRETURN    sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    if ( aSqlstmt -> sqlinfo < 2 )
    {
        sSqlRes = SQLEndTran ( SQL_HANDLE_DBC, sConnNode->mHdbc, SQL_COMMIT );
        if ( aSqlstmt -> sqlinfo == 1 )
        {
            ulpDisconnect( aConnName, aSqlstmt, NULL );
        }
    }
    else
    {
        sSqlRes = SQLEndTran ( SQL_HANDLE_DBC, sConnNode->mHdbc, SQL_ROLLBACK );
        if ( aSqlstmt -> sqlinfo == 3 )
        {
            ulpDisconnect( aConnName, aSqlstmt, NULL );
        }
    }

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_ENDTRANS_4COMMIT );

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_ENDTRANS_4COMMIT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             sSqlRes,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulpBatch ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    ODBC CLI�Լ� SQLSetConnectAttr ���� SQL_ATTR_BATCH �� �������� �ʰ� ������.
 *    ȣ���� ���� ������ �߻���Ű�� �ʰ� ��� �޼����� �����ش�.
 *
 *    ó������> EXEC SQL BATCH { ON | OFF };
 *
 * Implementation :
 *
 ***********************************************************************/
    ACP_UNUSED(aConnName);
    ACP_UNUSED(reserved);
    /*��� �޼����� sqlca�� �����Ѵ�.*/
    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_BATCH_NOT_SUPPORTED,
                           SQL_SUCCESS_WITH_INFO,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

}

ACI_RC ulpFree ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    �����ͺ��̽� �������� ���� �� ���� SQL�� ���� �� �Ҵ�޾Ҵ� �ڿ��� ��� �����Ѵ�.
 *
 *    ó������> EXEC SQL FREE;
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    SQLRETURN       sSqlRes;
    acp_bool_t      sIsDefaultConn;
    ACP_UNUSED(reserved);

    sIsDefaultConn = ACP_FALSE;

    if ( aConnName != NULL )
    {
        /* aConnName���� Connection node(ConnNode)�� ã�´�;*/
        sConnNode = ulpLibConLookupConn( aConnName );

        ACI_TEST_RAISE( sConnNode == NULL, ERR_NO_CONN );
    }
    else
    {
        sConnNode = ulpLibConGetDefaultConn();
        /* BUG-26359 valgrind bug */
        ACI_TEST_RAISE( sConnNode->mHenv == SQL_NULL_HENV, ERR_NO_CONN );
        sIsDefaultConn = ACP_TRUE;
    }

    sSqlRes = SQLFreeConnect( sConnNode->mHdbc );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FREE_CONN );

    sConnNode->mHdbc = SQL_NULL_HDBC;

    sSqlRes = SQLFreeEnv( sConnNode->mHenv );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FREE_ENV );

    sConnNode->mHenv = SQL_NULL_HENV;

    if( sIsDefaultConn == ACP_TRUE )
    {
        (void) ulpLibConInitDefaultConn();
    }
    else
    {
        ulpLibConDelConn( aConnName );
    }

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_FREE_CONN );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );

        if( sIsDefaultConn != ACP_TRUE )
        {
            ulpLibConDelConn( aConnName );
        }
    }
    ACI_EXCEPTION ( ERR_CLI_FREE_ENV );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );

        if( sIsDefaultConn != ACP_TRUE )
        {
            ulpLibConDelConn( aConnName );
        }
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC ulpAlterSession ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    DATA FORMAT ���� session property ������.
 *
 *    ó������> EXEC SQL ALTER SESSION SET DATE_FORMAT = '...';
 *            EXEC SQL ALTER SESSION SET DEFAULT_DATE_FORMAT = '...';
 *
 * Implementation :
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    SQLPOINTER      sValue;
    SQLINTEGER      sLen;
    SQLRETURN       sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    sValue = (SQLPOINTER)( aSqlstmt->extrastr + 1 );
    sLen   = acpCStrLen( aSqlstmt->extrastr + 1, ACP_SINT32_MAX ) - 1;

    ACI_TEST_RAISE( (sSqlRes = SQLSetConnectAttr( sConnNode->mHdbc,
                                                  ALTIBASE_DATE_FORMAT,
                                                  sValue,
                                                  sLen ))
                    != SQL_SUCCESS, ERR_CLI_SETCONN );

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                        
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION( ERR_CLI_SETCONN );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             sSqlRes,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}



ACI_RC ulpFreeLob ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    �ش� LOB locator�� ���õ� �ڿ����� ��� �������ش�.
 *
 *    ó������> EXEC SQL FREE LOB <hostvalue_name_list>;
 *
 * Implementation :
 *    SQLAllocStmt(...) -> SQLFreeLob(...)
 *
 ***********************************************************************/
    ulpLibConnNode *sConnNode;
    SQLHSTMT        sHstmt;
    SQLRETURN       sSqlRes;
    acp_uint32_t            sI;
    acp_uint32_t            sJ;
    acp_uint32_t            sArrSize;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    /* 1. alloc statement.*/
    sSqlRes = SQLAllocStmt( sConnNode -> mHdbc, &( sHstmt ) );

    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_ALLOC_STMT);

    /* 2. host value ������ ���� SQLFreeLob ȣ��.*/
    for (sI = 0; sI < aSqlstmt -> numofhostvar ; sI++)
    {
        if ( aSqlstmt -> hostvalue[ sI ].isarr == 0 )
        {
            sArrSize = 1;
        }
        else
        {
            /* FOR �� ���*/
            if ( (aSqlstmt -> iters > 0) &&
                 (aSqlstmt -> iters < aSqlstmt -> hostvalue[ sI ].arrsize) )
            {
                sArrSize = aSqlstmt -> iters;
            }
            else
            {
                sArrSize = aSqlstmt -> hostvalue[ sI ].arrsize;
            }
        }

        for (sJ = 0; sJ < sArrSize; sJ++)
        {
            sSqlRes = SQLFreeLob( sHstmt,
                                  ((SQLUBIGINT *)aSqlstmt -> hostvalue[ sI ].mHostVar)[sJ] );

            ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                            , ERR_CLI_FREE_LOB );
        }
    }

    /* 3. statement free*/
    sSqlRes = SQLFreeStmt( sHstmt, SQL_DROP );
    ACI_TEST_RAISE( (sSqlRes == SQL_ERROR) || (sSqlRes == SQL_INVALID_HANDLE)
                    , ERR_CLI_FREE_STMT );


    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                         

        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION ( ERR_CLI_ALLOC_STMT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION( ERR_CLI_FREE_LOB );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             sHstmt,
                             sSqlRes,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE3 );
    }
    ACI_EXCEPTION ( ERR_CLI_FREE_STMT );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             sHstmt,
                             SQL_ERROR,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;

}


ACI_RC ulpFailOver ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    fail over callback ������.
 *
 *    ó������> EXEC SQL REGISTER [AT <conn_name>] FAIL_OVER_CALLBACK <:host_variable>;
 *            EXEC SQL UNREGISTER [AT <conn_name>] FAIL_OVER_CALLBACK ;
 *
 * Implementation :
 *       SQLSetConnectAttr(...ALTIBASE_FAILOVER_CALLBACK...)
 *
 ***********************************************************************/

    ulpLibConnNode *sConnNode;
    SQLRETURN       sSqlRes;
    ACP_UNUSED(reserved);

    ULPGETCONNECTION(aConnName,sConnNode);

    if ( aSqlstmt->sqlinfo != 0 )
    {
        ACI_TEST_RAISE( (sSqlRes = SQLSetConnectAttr(sConnNode->mHdbc,
                                        ALTIBASE_FAILOVER_CALLBACK,
                                        (SQLPOINTER)aSqlstmt->hostvalue[0].mHostVar,
                                        0))
                        != SQL_SUCCESS, ERR_CLI_SETCONN );
    }
    else
    {
        ACI_TEST_RAISE( (sSqlRes = SQLSetConnectAttr(sConnNode->mHdbc,
                                        ALTIBASE_FAILOVER_CALLBACK,
                                        (SQLPOINTER)NULL,
                                        0))
                        != SQL_SUCCESS, ERR_CLI_SETCONN );
    }

    ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                           aSqlstmt->sqlcodeerr,
                           aSqlstmt->sqlstateerr,
                           SQLMSG_SUCCESS,
                           SQL_SUCCESS,
                           SQLSTATE_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION ( ERR_NO_CONN );
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         ULPCHECKCONNECTIONNAME(aConnName) ); 
                        
        ulpSetErrorInfo4PCOMP( aSqlstmt->sqlcaerr,
                               aSqlstmt->sqlcodeerr,
                               aSqlstmt->sqlstateerr,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_INVALID_HANDLE,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION( ERR_CLI_SETCONN );
    {
        ulpSetErrorInfo4CLI( sConnNode,
                             SQL_NULL_HSTMT,
                             sSqlRes,
                             aSqlstmt->sqlcaerr,
                             aSqlstmt->sqlcodeerr,
                             aSqlstmt->sqlstateerr,
                             ERR_TYPE2 );
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

void ulpAfterXAOpen ( acp_sint32_t aRmid,
                      SQLHENV      aEnv,
                      SQLHDBC      aDbc )
{
/***********************************************************************
 *
 * Description :
 *      XA ���� ó���Լ�.
 *
 * Implementation :
 *
 ***********************************************************************/

    ulpLibConnNode *sNode;
    ulpSqlca       *sSqlca;
    ulpSqlcode     *sSqlcode;
    ulpSqlstate    *sSqlstate;
    SQLRETURN       sSqlres;
    acp_char_t      sDBName[MAX_CONN_NAME_LEN + 1];

    SQLINTEGER      sInd;
    acp_bool_t      sAllocStmt;
    acp_bool_t      sIsLatched;

    /*fix BUG-25597 APRE���� AIX�÷��� �νõ� ���������� �ذ��ؾ� �մϴ�.
     APRE�� ulpConnMgr�ʱ������� CLI�� XaOpen�Լ��� �� �Լ��� �Ҹ����
     �����Ѵ�.
     ���߿�  APRE�� ulpConnMgr �ʱ�ȭ��  CLI�� ����  �� Open�� Xa Connection��
     �ε��ϵ��� �Ѵ�.
   */
    /* gUlpLibDoInitProc�� true��� ���� XA ���� �ڵ��� ������ ���� �����̴�.*/
    ACI_TEST(gUlpLibDoInitProc == ACP_TRUE);

    sSqlca    = ulpGetSqlca();
    sSqlcode  = ulpGetSqlcode();
    sSqlstate = ulpGetSqlstate();

    sSqlres = SQLGetConnectAttr( aDbc,
                                 ALTIBASE_XA_NAME,
                                 sDBName,
                                 MAX_CONN_NAME_LEN + 1,
                                 &sInd);

    ACI_TEST_RAISE ( (sSqlres == SQL_ERROR) || (sSqlres == SQL_INVALID_HANDLE),
                     ERR_CLI_GETCONNECT_ATTR );

    sAllocStmt = sIsLatched = ACP_FALSE;

    if ( sDBName[0] == '\0' )
    {
        /* set default connection*/
        sNode = ulpLibConGetDefaultConn();
        sAllocStmt = ACP_TRUE;
    }
    else
    {
        /* set connection with name*/
        sNode = ulpLibConLookupConn( sDBName );
        if (sNode == NULL)
        {
            sNode = ulpLibConNewNode( sDBName, ACP_TRUE );

            ACI_TEST_RAISE( sNode == NULL, ERR_CONN_NAME_OVERFLOW );

            ACI_TEST_RAISE( ulpLibConAddConn( sNode ) == NULL,
                            ERR_ALREADY_CONNECTED );

            sAllocStmt = ACP_TRUE;
        }
    }

    sNode->mHenv     = aEnv;
    sNode->mHdbc     = aDbc;
    sNode->mUser     = NULL;
    sNode->mPasswd   = NULL;
    sNode->mConnOpt1 = NULL;
    sNode->mConnOpt2 = NULL;
    sNode->mIsXa     = ACP_TRUE;
    sNode->mXaRMID   = aRmid;

    if( gUlpLibOption.mIsMultiThread == ACP_TRUE )
    {
        ACI_TEST_RAISE ( acpThrRwlockLockWrite( &(sNode->mLatch4Xa) )
                         != ACP_RC_SUCCESS, ERR_LATCH_WRITE );

        sIsLatched = ACP_TRUE;
    }

    ulpSetDateFmtCore( sNode );

    if( sIsLatched == ACP_TRUE )
    {
        ACI_TEST_RAISE ( acpThrRwlockUnlock ( &(sNode->mLatch4Xa) )
                         != ACP_RC_SUCCESS, ERR_LATCH_RELEASE );
    }

    if ( sAllocStmt == ACP_TRUE )
    {
        sSqlres = SQLAllocStmt( sNode->mHdbc,
                                &(sNode->mHstmt) );

        ulpSetErrorInfo4CLI( sNode,
                             sNode->mHstmt,
                             sSqlres,
                             sSqlca,
                             sSqlcode,
                             sSqlstate,
                             ERR_TYPE2 );

        if (sSqlres == SQL_ERROR)
        {
            sNode->mHenv  = SQL_NULL_HENV;
            sNode->mHdbc  = SQL_NULL_HDBC;
            sNode->mHstmt = SQL_NULL_HSTMT;
        }

    }

    ACI_EXCEPTION(ERR_CLI_GETCONNECT_ATTR);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_XA_GetConnectAttr_Error );
        ulpSetErrorInfo4PCOMP( sSqlca,
                               sSqlcode,
                               sSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQL_ERROR,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION(ERR_CONN_NAME_OVERFLOW);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_COMP_Exceed_Max_Connname_Error,
                         sDBName );
        ulpSetErrorInfo4PCOMP( sSqlca,
                               sSqlcode,
                               sSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_CONNAME_OVERFLOW,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION(ERR_ALREADY_CONNECTED);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Aleady_Exist_Error,
                         sDBName );

        ulpSetErrorInfo4PCOMP( sSqlca,
                               sSqlcode,
                               sSqlstate,
                               ulpGetErrorMSG(&sErrorMgr),
                               SQLCODE_ALREADY_CONNECTED,
                               ulpGetErrorSTATE(&sErrorMgr) );
    }
    ACI_EXCEPTION(ERR_LATCH_WRITE);
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
    ACI_EXCEPTION(ERR_LATCH_RELEASE);
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
}

void ulpAfterXAClose ( void )
{
    /* default connection node �ʱ�ȭ*/
    (void) ulpLibConInitDefaultConn();
}

/* TASK-7218 Handling Multiple Errors */
ACI_RC ulpGetStmtDiag ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    diagnostics information ������.
 *
 *    ó������ : 
 *    >>---EXEC SQL---GET-.---------.-DIAGNOSTICS-->
 *                        +-CURRENT-+
 *
 *            +-----------,--------------+
 *            V                          |
 *    >-----.-.- :hvar1 = --.-ROW_COUNT--.->
 *                          |            |
 *                          +-NUMBER-----+
 *
 * Implementation :
 *       ulpSetErrorInfo4CLI()���� ���ص� MultiError ������ ��ȯ�Ѵ�.
 *       �׷��� ulpSetErrorInfo4CLI()���� ���� ������ ������ �� statement �ڵ���
 *       NULL�̾��ٸ�, �� dbc ���� ������ �߻��� ����� MultiError�� ���õ���
 *       �ʴ´�.
 *       ������ sqlca, SQLCODE, SQLSTATE�� ���õǹǷ� �� ������ �̿��ؼ�
 *       ��ȯ�Ѵ�.
 *       ...
 *       dbc���� ������ �߻��ߴٸ� ConnNode�� �ٷ� ������ �� �ֱ� ������
 *       MultiError�� �����ϴ� ���� ���ǹ��ϹǷ� �������� �ʵ��� ������.
 *
 ***********************************************************************/
    acp_sint16_t      i;
    ulpMultiErrorMgr *sMultiErrMgr;
    ulpHostVar       *sHostValue = NULL;

    ACP_UNUSED(aConnName);
    ACP_UNUSED(reserved);

    sHostValue = aSqlstmt->hostvalue;

    ACI_TEST( sqlca.sqlcode == SQL_SUCCESS );

    sMultiErrMgr = ulpLibGetMultiErrorMgr();

    /* multi-error�� ���õǾ� ���� ������ sqlca, SQLCODE, SQLSTATE ���� �̿� */
    for (i = 0; i < aSqlstmt->numofhostvar; i++)
    {
        switch ( sHostValue[i].mDiagType )
        {
        case H_STMT_DIAG_NUMBER:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = ulpLibMultiErrorMgrGetNumber( sMultiErrMgr );
            break;
        case H_STMT_DIAG_ROW_COUNT:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = sqlca.sqlerrd[2];
            break;
        }
    }
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    for (i = 0; i < aSqlstmt->numofhostvar; i++)
    {
        *(SQLINTEGER *)(sHostValue[i].mHostVar) = 0;
    }

    return ACI_SUCCESS;
}

ACI_RC ulpGetConditionDiag ( acp_char_t *aConnName, ulpSqlstmt *aSqlstmt, void *reserved )
{
/***********************************************************************
 *
 * Description :
 *    diagnostics information ������.
 *
 *    ó������:
 *    >>---EXEC SQL---GET-.---------.-DIAGNOSTICS-->            
 *                        +-CURRENT-+
 *
 *                                  +-----------------,------------------+
 *                                  V                                    |  
 *    >----CONDITION--.--:hvar2--.--.-:hvar3 = --.---RETURNED_SQLCODE----.->
 *                    +--integer-+               |                       |
 *                                               +---RETURNED_SQLSTATE---+
 *                                               |                       |
 *                                               +---MESSAGE_TEXT--------+
 *                                               |                       |
 *                                               +---ROW_NUMBER----------+
 *                                               |                       |
 *                                               +---COLUMN_NUMBER-------+
 *
 * Implementation :
 *       ulpSetErrorInfo4CLI()���� SQLGetDiagRec()�� �ݺ��ϸ鼭 �����ص�
 *       MultiError ������ ��ȯ�Ѵ�.
 *       �׷��� ulpSetErrorInfo4CLI()���� ���� ������ ������ �� statement �ڵ���
 *       NULL�̾��ٸ�, �� dbc ���� ������ �߻��� ����� MultiError�� ���õ���
 *       �ʴ´�.
 *       ������ sqlca, SQLCODE, SQLSTATE�� ���õǹǷ� �� ������ �̿��ؼ�
 *       ��ȯ�Ѵ�.
 *       ...
 *       dbc���� ������ �߻��ߴٸ� ConnNode�� �ٷ� ������ �� �ֱ� ������
 *       MultiError�� �����ϴ� ���� ���ǹ��ϹǷ� �������� �ʵ��� ������.
 *
 ***********************************************************************/
    acp_sint16_t        i;
    ulpMultiErrorMgr   *sMultiErrorMgr;
    ulpMultiErrorStack *sError     = NULL;
    ulpHostVar         *sHostValue = NULL;
    acp_sint32_t        sRecNum    = aSqlstmt->iters;

    ACP_UNUSED(aConnName);
    ACP_UNUSED(reserved);

    sHostValue = aSqlstmt->hostvalue;

    ACI_TEST( sqlca.sqlcode == SQL_SUCCESS );

    sMultiErrorMgr = ulpLibGetMultiErrorMgr();

    if ( sMultiErrorMgr == NULL ||
         sMultiErrorMgr->mErrorNum == 0 )
    {
        /* multi-error�� ���õǾ� ���� �ʴٸ� sqlca, SQLCODE, SQLSTATE ���� �̿��� ���̴�.
         * ���� sRecNum�� �ݵ�� 1�̾�� �Ѵ�. */
        ACI_TEST( sRecNum != 1 );
    }
    else
    {
        ACI_TEST( sRecNum < 1 || sRecNum > sMultiErrorMgr->mErrorNum );
        sError = &(ulpLibGetMultiErrorMgr()->mErrorStack[sRecNum-1]);
    }
     
    for (i = 0; i < aSqlstmt->numofhostvar; i++)
    {
        switch ( sHostValue[i].mDiagType )
        {
        case H_COND_DIAG_RETURNED_SQLCODE:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = ulpLibMultiErrorMgrGetSqlcode( sError );
            break;
        case H_COND_DIAG_RETURNED_SQLSTATE:
            ulpLibMultiErrorMgrGetSqlstate( sError,
                                         sHostValue[i].mHostVar,
                                         sHostValue[i].mMaxlen,
                                         sHostValue[i].mHostInd );
            break;
        case H_COND_DIAG_MESSAGE_TEXT:
            ulpLibMultiErrorMgrGetMessageText( sError,
                                            sHostValue[i].mHostVar,
                                            sHostValue[i].mMaxlen,
                                            sHostValue[i].mHostInd );
            break;
        case H_COND_DIAG_ROW_NUMBER:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = ulpLibMultiErrorMgrGetRowNumber( sError );
            break;
        case H_COND_DIAG_COLUMN_NUMBER:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = ulpLibMultiErrorMgrGetColumnNumber( sError );
            break;
        }
    }
    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    for (i = 0; i < aSqlstmt->numofhostvar; i++)
    {
        switch ( sHostValue[i].mDiagType )
        {
        case H_COND_DIAG_RETURNED_SQLCODE:
        case H_COND_DIAG_ROW_NUMBER:
        case H_COND_DIAG_COLUMN_NUMBER:
            *(SQLINTEGER *)(sHostValue[i].mHostVar) = 0;
            break;
        case H_COND_DIAG_RETURNED_SQLSTATE:
        case H_COND_DIAG_MESSAGE_TEXT:
            ((acp_char_t *)(sHostValue[i].mHostVar))[0] = '\0';
            if  (sHostValue[i].mHostInd != NULL )
            {
                *(ulvSLen *)(sHostValue[i].mHostInd) = 0;
            }
            break;
        }
    }
    return ACI_SUCCESS;
}
