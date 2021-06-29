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
#include <ulpLibError.h>

void ulpSetErrorInfo4PCOMP( ulpSqlca          *aSqlca,
                            ulpSqlcode        *aSqlcode,
                            ulpSqlstate       *aSqlstate,
                            const acp_char_t  *aErrMsg,
                            acp_sint32_t       aErrCode,
                            const acp_char_t  *aErrState )
{
/***********************************************************************
 *
 * Description :
 *    ���� SQL�� ���� ��������� �����ϴ� sqlca, sqlcode, sqlstate��
 *    ��������� ���ڷ� �Ѱ��� ������ ���� ���ִ� �Լ�. precompiler ���� ���� ���� ���.
 *
 * Implementation :
 *
 ***********************************************************************/
    if ( aErrMsg != NULL )
    {
        aSqlca->sqlerrm.sqlerrml =
                (short)acpCStrLen(aErrMsg, ACP_SINT16_MAX);

        acpCStrCpy( aSqlca->sqlerrm.sqlerrmc,
                    MAX_ERRMSG_LEN,
                    aErrMsg,
                    acpCStrLen(aErrMsg, ACP_SINT32_MAX));
    }
    else
    {
        aSqlca->sqlerrm.sqlerrmc[0] = '\0';
        aSqlca->sqlerrm.sqlerrml    = 0;
    }

    if ( aErrCode != ERRCODE_NULL )
    {
        switch ( aErrCode )
        {
        /* SQL_ERROR, SQL_SUCCESS, SQL_INVALID_HANDLE ... �� ���� *
         * SQLCODE, sqlca->sqlcode�� ��õ� ������ ��������.         */
            case SQL_SUCCESS:
                aSqlca->sqlcode = SQL_SUCCESS;
                *(aSqlcode) = SQL_SUCCESS;
                break;
            case SQL_SUCCESS_WITH_INFO:
                aSqlca->sqlcode = SQL_SUCCESS_WITH_INFO;
                *(aSqlcode) = SQL_SUCCESS_WITH_INFO;
                break;
            case SQL_NO_DATA:
                aSqlca->sqlcode = SQL_NO_DATA;
                *(aSqlcode) = SQL_NO_DATA;
                break;
            case SQL_ERROR:
                aSqlca->sqlcode = SQL_ERROR;
                *(aSqlcode) = SQL_ERROR;
                break;
            case SQL_INVALID_HANDLE:
                aSqlca->sqlcode = SQL_INVALID_HANDLE;
                *(aSqlcode) = SQL_INVALID_HANDLE;
                break;
            default:
                aSqlca->sqlcode = SQL_ERROR;
                *(aSqlcode) = (acp_sint32_t)( -1 * (aErrCode) );
                break;
        }
    }

    if ( aErrState != NULL )
    {
        acpCStrCpy( *aSqlstate,
                     MAX_ERRSTATE_LEN,
                     aErrState,
                     acpCStrLen(aErrState, ACP_SINT16_MAX) );
    }
    else
    {
        (*aSqlstate)[0] = '\0';
    }
    /* fetch �ų� select�ϰ�� sqlerrd[2]���� fetch�� row���� ����ž��Ѵ�.*/
    aSqlca->sqlerrd[2] = 0;
    aSqlca->sqlerrd[3] = 0;
}


ACI_RC ulpSetErrorInfo4CLI( ulpLibConnNode    *aConnNode,
                            SQLHSTMT           aHstmt,
                            const SQLRETURN    aSqlRes,
                            ulpSqlca          *aSqlca,
                            ulpSqlcode        *aSqlcode,
                            ulpSqlstate       *aSqlstate,
                            ulpLibErrType      aCLIType )
{
/***********************************************************************
 *
 * Description :
 *     ���� SQL�� ���� ��������� ODBC CLI �Լ��� �̿��� ���� ���ִ� �Լ�.
 *    CLI���� �߻��� ���� ���� ���.
 *     ulpSetErrorInfo4CLI �Լ� ������ CLIȣ�� �� ������ �߻��ϸ� SQL_FAILURE ����,
 *    �׿��� ��� SQL_SUCCESS ����.
 *
 * Implementation :
 *
 ***********************************************************************/

    acp_uint64_t sRowCnt;
    SQLRETURN    sRc;
    SQLHENV     *sHenv;
    SQLHDBC     *sHdbc;
    /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
    SQLSMALLINT sRecordNo;
    /* sqlcli������������ �ӽ÷� �����صα����� ������ */
    ulpSqlcode   sSqlcode;
    ulpSqlstate  sSqlstate;
    acp_char_t   sErrMsg[MAX_ERRMSG_LEN];
    acp_sint16_t sErrMsgLen;

    acp_bool_t        sExistFailOverSuccess;
    ulpMultiErrorMgr *sMultiErrMgr; /* TASK-7218 */

    sHenv = &(aConnNode->mHenv);
    sHdbc = &(aConnNode->mHdbc);
    sMultiErrMgr = &(aConnNode->mMultiErrorMgr);

    ulpLibInitMultiErrorMgr(sMultiErrMgr);
    ulpLibSetMultiErrorMgr(NULL);

    switch ( aSqlca->sqlcode = aSqlRes )
    {
        case SQL_SUCCESS :
            *aSqlcode = aSqlRes;
            acpCStrCpy( *aSqlstate,
                         MAX_ERRSTATE_LEN,
                         (acp_char_t *)SQLSTATE_SUCCESS,
                         MAX_ERRSTATE_LEN-1);

            acpCStrCpy( aSqlca->sqlerrm.sqlerrmc,
                        MAX_ERRMSG_LEN,
                        (acp_char_t *)SQLMSG_SUCCESS,
                        acpCStrLen((acp_char_t *)SQLMSG_SUCCESS, ACP_SINT16_MAX));

            aSqlca->sqlerrm.sqlerrml = (short)acpCStrLen(aSqlca->sqlerrm.sqlerrmc,
                                                         ACP_SINT16_MAX);
            sRowCnt = 0;
            switch ( aCLIType )
            {
                case ERR_TYPE1_DML :
                    sRc = SQLGetStmtAttr( aHstmt,
                                          ALTIBASE_STMT_ATTR_TOTAL_AFFECTED_ROWCOUNT,
                                          &sRowCnt, 0, NULL);
                    ACI_TEST_RAISE ( sRc != SQL_SUCCESS, ERR_GETERRINFO );
                    aSqlca->sqlerrd[2] = (acp_sint32_t) sRowCnt;
                    break;
                default :
                    aSqlca->sqlerrd[2] = (acp_sint32_t) sRowCnt;
            }
            /*aSqlca->sqlerrd[3] = 0;*/
            break;
        case SQL_NO_DATA :
            switch ( aCLIType )
            {
                case ERR_TYPE1:
                case ERR_TYPE1_DML :
                    *(aSqlcode) = SQLCODE_SQL_NO_DATA;

                    acpCStrCpy( *aSqlstate,
                                 MAX_ERRSTATE_LEN,
                                 (acp_char_t *)SQLSTATE_SQL_NO_DATA,
                                 MAX_ERRSTATE_LEN-1);

                    acpCStrCpy( aSqlca->sqlerrm.sqlerrmc,
                                MAX_ERRMSG_LEN,
                                (acp_char_t *)SQLMSG_SQL_NO_DATA,
                                acpCStrLen((acp_char_t *)SQLMSG_SQL_NO_DATA,
                                           ACP_SINT16_MAX)
                              );

                    aSqlca->sqlerrm.sqlerrml = (short)acpCStrLen(aSqlca->sqlerrm.sqlerrmc,
                                                                 ACP_SINT16_MAX);

                    sRowCnt = 0;
                    if ( aCLIType == ERR_TYPE1_DML )
                    {
                        sRc = SQLGetStmtAttr( aHstmt,
                                              ALTIBASE_STMT_ATTR_TOTAL_AFFECTED_ROWCOUNT,
                                              &sRowCnt, 0, NULL);
                        ACI_TEST_RAISE ( sRc != SQL_SUCCESS, ERR_GETERRINFO );
                    }
                    aSqlca->sqlerrd[2] = (acp_sint32_t) sRowCnt;
                    /*aSqlca->sqlerrd[3] = 0;*/
                    break;
                default :
                    goto GOTO_DEFAULT;
            }
            break;
        case SQL_INVALID_HANDLE :
            ACI_RAISE ( ERR_INVALID_HANDLE );
            break;
        case SQL_SUCCESS_WITH_INFO :
            switch ( aCLIType )
            {
                case ERR_TYPE1 :
                case ERR_TYPE1_DML :
                case ERR_TYPE2 :
                    /* SQLError �Լ��� ���� SQLSTATE, SQLCODE, ���� �޽��� ����. */
                    sRc = SQLError(*sHenv,
                                   *sHdbc,
                                   aHstmt,
                                   (SQLCHAR *)aSqlstate,
                                   (SQLINTEGER *)aSqlcode,
                                   (SQLCHAR *)aSqlca->sqlerrm.sqlerrmc,
                                   MAX_ERRMSG_LEN,
                                   (SQLSMALLINT *)&(aSqlca->sqlerrm.sqlerrml));

                    if ( sRc == SQL_SUCCESS || sRc == SQL_SUCCESS_WITH_INFO)
                    {
                        *(aSqlcode) = SQL_SUCCESS_WITH_INFO;
                        sRowCnt = 0;
                        if ( aCLIType == ERR_TYPE1_DML )
                        {
                            sRc = SQLGetStmtAttr( aHstmt,
                                                  ALTIBASE_STMT_ATTR_TOTAL_AFFECTED_ROWCOUNT,
                                                  &sRowCnt, 0, NULL);
                            ACI_TEST_RAISE ( sRc != SQL_SUCCESS, ERR_GETERRINFO );
                        }
                        aSqlca->sqlerrd[2] = (acp_sint32_t) sRowCnt;
                        /*aSqlca->sqlerrd[3] = 0;*/
                    }
                    else
                    {
                        ACI_TEST_RAISE ( sRc == SQL_INVALID_HANDLE, ERR_INVALID_HANDLE );
                    }
                    break;
                default :
                    goto GOTO_DEFAULT;
            }
            break;

        default :
/* goto */
GOTO_DEFAULT:
        sRecordNo = 1;
        /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
        /* SQLGetDiagRec �Լ��� ���� SQLSTATE, SQLCODE, ���� �޽��� ����. */
        /* failover������ �Ǵٸ� SQLCLI�Լ��� ȣ��Ǿ� �ٸ������� �������� ���� ���� �� �ֱ⶧����
           SQLGetDiagRec �Լ��� ���� ��Ȳ�� �������� Ȯ����. */
        if( aHstmt == SQL_NULL_HSTMT )
        {
            sRc = SQLGetDiagRec(SQL_HANDLE_DBC,
                               *sHdbc,
                                sRecordNo,
                                (SQLCHAR *)aSqlstate,
                                (SQLINTEGER *)aSqlcode,
                                (SQLCHAR *)aSqlca->sqlerrm.sqlerrmc,
                                MAX_ERRMSG_LEN,
                                (SQLSMALLINT *)&(aSqlca->sqlerrm.sqlerrml));
        }
        else
        {
            /* TASK-7218 Handling Multiple Errors */
            ACI_TEST( ulpLibMultiErrorMgrSetNumber( aHstmt, sMultiErrMgr )
                      != ACI_SUCCESS );

            /* TASK-7218 Handling Multiple Errors 
               FAILOVER_SUCCESS�� ���ϵǴ��� Multi-Error�� ��� �����ϱ� ����
               break ��ſ� flag�� �ξ� ��� �ݺ��ϵ��� �� */
            /* ���� or SQL_NO_DATA�� ���ϵɶ����� �ݺ��ؼ� ȣ���ϸ�, FAILOVER_SUCCESS�� ���ϵǸ�
               ���� ���� ������ reprepare�� �ϱ����� ó���� �ϸ�, �׿��� ��쿡�� ���� �ֱ��� error������
               ������. */
            /* �ݺ��ؼ� SQL_ERROR�� ���ϵ����� �ʴ´ٰ� ������.
               �����̶� ����ó���� �Ǹ� �������� SQL_NO_DATA�� ���ϵ�. */
            sExistFailOverSuccess = ACP_FALSE;
            while ((sRc = SQLGetDiagRec(SQL_HANDLE_STMT,
                                        aHstmt,
                                        sRecordNo,
                                        (SQLCHAR *)sSqlstate,
                                        (SQLINTEGER *)&sSqlcode,
                                        (SQLCHAR *)sErrMsg,
                                        MAX_ERRMSG_LEN,
                                        (SQLSMALLINT *)&sErrMsgLen) != SQL_NO_DATA))
            {
                if(sRc == SQL_INVALID_HANDLE )
                {
                    break;
                }

                if ( sSqlcode == ALTIBASE_FAILOVER_SUCCESS &&
                     sExistFailOverSuccess == ACP_FALSE )
                {
                    *aSqlcode = sSqlcode;
                    acpSnprintf(*aSqlstate, MAX_SQLSTATE_LEN, "%s", sSqlstate);
                    acpSnprintf(aSqlca->sqlerrm.sqlerrmc, MAX_ERRMSG_LEN, "%s", sErrMsg);
                    aSqlca->sqlerrm.sqlerrml = sErrMsgLen;
                    sExistFailOverSuccess = ACP_TRUE;
                }
                else
                {
                    /* ���� �ֱ��� error������ �����صд�. */
                    if(sRecordNo==1)
                    {
                        *aSqlcode = sSqlcode;
                        acpSnprintf(*aSqlstate, MAX_SQLSTATE_LEN, "%s", sSqlstate);
                        acpSnprintf(aSqlca->sqlerrm.sqlerrmc, MAX_ERRMSG_LEN, "%s", sErrMsg);
                        aSqlca->sqlerrm.sqlerrml = sErrMsgLen;
                    }
                }

                /* TASK-7218 Handling Multiple Errors */
                ulpLibMultiErrorMgrSetDiagRec( aHstmt,
                                         sMultiErrMgr,
                                         sRecordNo,
                                         sSqlcode,
                                         sSqlstate,
                                         sErrMsg );

                sRecordNo++;
            }

            /* TASK-7218 Handling Multiple Errors */
            ulpLibSetMultiErrorMgr(sMultiErrMgr);
        }

        ACI_TEST_RAISE ( sRc == SQL_INVALID_HANDLE, ERR_INVALID_HANDLE );

        switch(*(aSqlcode))
        {
            case SQL_SUCCESS:
                *(aSqlcode) = SQL_ERROR;
                break;
            case ALTIBASE_FAILOVER_SUCCESS:
                /* BUG-47239 mFailoveredJustnow�� �����ϰ� FAILOVER_SUCCESS�� 
                 * �����Ǵ´�� ulpSetFailoverFlag�� ���ؼ� mNeedReprepare�� �ٷ� �������ش�. */
                (void) ulpSetFailoverFlag( aConnNode );
                *(aSqlcode) *= -1;
                break;
            default:
                *(aSqlcode) *= -1;
                break;
        }

        sRowCnt = 0;
        if ( aCLIType == ERR_TYPE1_DML )
        {
            sRc = SQLGetStmtAttr( aHstmt,
                                  ALTIBASE_STMT_ATTR_TOTAL_AFFECTED_ROWCOUNT,
                                  &sRowCnt, 0, NULL);
            ACI_TEST_RAISE ( sRc != SQL_SUCCESS, ERR_GETERRINFO );
        }
        aSqlca->sqlerrd[2] = (acp_sint32_t) sRowCnt;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION (ERR_GETERRINFO);
    {
        aSqlca->sqlcode = SQL_ERROR;
        aSqlca->sqlerrm.sqlerrml = (short)acpCStrLen(aSqlca->sqlerrm.sqlerrmc,
                                                     ACP_SINT16_MAX);
        aSqlca->sqlerrd[2] = 0;
       /* aSqlca->sqlerrd[3] = 0;*/
    }
    ACI_EXCEPTION (ERR_INVALID_HANDLE);
    {
        ulpErrorMgr sErrorMgr;
        ulpSetErrorCode( &sErrorMgr,
                         ulpERR_ABORT_Conn_Not_Exist_Error,
                         aConnNode->mConnName );

        aSqlca->sqlcode = SQL_INVALID_HANDLE;
        *(aSqlcode) = SQL_INVALID_HANDLE;

        acpCStrCpy( *aSqlstate,
                    MAX_ERRSTATE_LEN,
                    ulpGetErrorSTATE( &sErrorMgr ),
                    MAX_ERRSTATE_LEN-1 );

        acpCStrCpy( aSqlca->sqlerrm.sqlerrmc,
                    MAX_ERRMSG_LEN,
                    ulpGetErrorMSG(&sErrorMgr),
                    MAX_ERRMSG_LEN-1 );

        aSqlca->sqlerrm.sqlerrml = (short)acpCStrLen( aSqlca->sqlerrm.sqlerrmc,
                                                      ACP_SINT16_MAX );

        /*idlOS::strcpy(ses_sqlerrmsg, aSqlstmt->mSqlca->sqlerrm.sqlerrmc);*/
        aSqlca->sqlerrd[2] = 0;
        /*aSqlca->sqlerrd[3] = 0;*/
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC ulpGetConnErrorMsg( ulpLibConnNode *aConnNode, acp_char_t *aMsg )
{
/***********************************************************************
 *
 * Description :
 *    Connection �� ������ �߻��� ���, �� �Լ��� �̿��� ���� �޼����� ���´�.
 * 
 * Implementation :
 *
 ***********************************************************************/
    acp_char_t  sMsg[MAX_ERRMSG_LEN];
    SQLCHAR     sState[MAX_ERRSTATE_LEN];
    SQLINTEGER  sCode;
    SQLRETURN   sRes;
    SQLSMALLINT sMsglen;

    sRes = SQLError( aConnNode->mHenv,
                     aConnNode->mHdbc,
                     aConnNode->mHstmt,
                     sState,
                     &sCode,
                     (SQLCHAR *) sMsg,
                     MAX_ERRMSG_LEN,
                     &sMsglen );

    ACI_TEST( (sRes==SQL_ERROR) || (sRes==SQL_INVALID_HANDLE) );

    acpCStrCat( aMsg,
                MAX_ERRMSG_LEN-1,
                sMsg,
                acpCStrLen(sMsg, MAX_ERRMSG_LEN) );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

