#include <stdio.h>
#include <stdlib.h>
#include <sqlcli.h>
#include <unistd.h>

#define BUFF_SIZE  (1024)
#define BUFF_SIZE2 (8192)

void test_error_exit(SQLRETURN rc, char * msg)
{
    if( !SQL_SUCCEEDED(rc) )
    {
        printf(msg);
        exit(-1);
    }
}

void print_diagnostic(SQLSMALLINT aHandleType, SQLHANDLE aHandle)
{
    SQLRETURN   rc;
    SQLSMALLINT sRecordNo;
    SQLCHAR     sSQLSTATE[6];
    SQLCHAR     sMessage[2048];
    SQLSMALLINT sMessageLength;
    SQLINTEGER  sNativeError;

    sRecordNo = 1;

    while ((rc = SQLGetDiagRec(aHandleType,
                                aHandle,
                               sRecordNo,
                               sSQLSTATE,
                               &sNativeError,
                               sMessage,
                               sizeof(sMessage),
                               &sMessageLength)) != SQL_NO_DATA)
    {
        printf("Diagnostic Record %d\n", sRecordNo);
        printf("     SQLSTATE     : %s\n", sSQLSTATE);
        printf("     Message text : %s\n", sMessage);
        printf("     Message len  : %d\n", sMessageLength);
        printf("     Native error : 0x%X\n", sNativeError);

        if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO)
        {
            break;
        }

        sRecordNo++;
    }
}

void execute_err(SQLHDBC aCon, SQLHSTMT aStmt, char* q)
{
    printf("Error : %s\n",q);

    if (aStmt == SQL_NULL_HSTMT)
    {
        if (aCon != SQL_NULL_HDBC)
        {
            print_diagnostic(SQL_HANDLE_DBC, aCon);
        }
    }
    else
    {
        print_diagnostic(SQL_HANDLE_STMT, aStmt);
    }
}

/* Fail-Over Success�� �� ��尡 �ִ��� ���θ� �Ǵ��ϴ� �Լ�. */
int isFailOverErrorEvent(SQLSMALLINT aHandleType, SQLHANDLE aHandle)
{
    SQLRETURN   sRc;
    SQLSMALLINT sRecordNo;
    SQLCHAR     sSQLSTATE[6];
    SQLCHAR     sMessage[2048];
    SQLSMALLINT sMessageLength;
    SQLINTEGER  sNativeError;
    int         sRet = 0;

    sRecordNo = 1;

    while ((sRc = SQLGetDiagRec(aHandleType,
                               aHandle,
                               sRecordNo,
                               sSQLSTATE,
                               &sNativeError,
                               sMessage,
                               sizeof(sMessage),
                               &sMessageLength)) != SQL_NO_DATA)
    {
        sRecordNo++;
        if(sNativeError == ALTIBASE_FAILOVER_SUCCESS)
        {
            sRet = 1;
            printf("%s\n",sMessage);
            break;
        }
        else
        {
            printf("%s\n",sMessage);
        }
    }
    return sRet;
}

/* Shard Node Retry �� �������� ���θ� �Ǵ��ϴ� �Լ�. */
int  isShardNodeRetryNotAvailable(SQLHSTMT aStmt)
{
    SQLRETURN   sRc;
    SQLSMALLINT sRecordNo;
    SQLCHAR     sSQLSTATE[6];
    SQLCHAR     sMessage[2048];
    SQLSMALLINT sMessageLength;
    SQLINTEGER  sNativeError;
    int         sRet = 0;

    sRecordNo = 1;

    while ((sRc = SQLGetDiagRec(SQL_HANDLE_STMT,
                               aStmt,
                               sRecordNo,
                               sSQLSTATE,
                               &sNativeError,
                               sMessage,
                               sizeof(sMessage),
                               &sMessageLength)) != SQL_NO_DATA)
    {
        sRecordNo++;
        if(sNativeError == ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE)
        {
            sRet = 1;
            printf("%s\n",sMessage);
            break;
        }
        else
        {
            printf("%s\n",sMessage);
        }
    }
    return sRet;
}

/* 
 * ���� failover �߻� �� �л� Ʈ����� rollback�� ���� �Լ� 
 * �л� Ʈ����� rollback �ÿ��� failover�� �߻��� �� �����Ƿ� Rollback�� �� �� �õ��Ѵ�.
 */
int failoverRollback(SQLHANDLE aDbc)
{
    int sReturn = 0;

    if(SQLEndTran(SQL_HANDLE_DBC, aDbc, SQL_ROLLBACK) != SQL_SUCCESS)
    {
        if(isFailOverErrorEvent(SQL_HANDLE_DBC, aDbc) == 1)
        {
            if(SQLEndTran(SQL_HANDLE_DBC, aDbc, SQL_ROLLBACK) != SQL_SUCCESS)
            {
                sReturn = 0; //distributed transaction rollback Fail
            }
            else
            {
                sReturn = 1; //distributed transaction rollback OK
            }
        }
        else
        {
            sReturn = 0; //distributed transaction rollback Fail
        }
    }
    else
    {
        sReturn = 1; //distributed transaction rollback OK
    }
    return sReturn;
}

int main(int argc, char* argv[])
{
    char        sConnStr[BUFF_SIZE] = {0};
    SQLHANDLE   sEnv  = SQL_NULL_HENV;
    SQLHANDLE   sDbc  = SQL_NULL_HDBC;
    SQLHSTMT    sStmt = SQL_NULL_HSTMT;
    SQLRETURN   sRetCode;
    char        sPrepareExecSampleQuery[100] = "SELECT I2 FROM T1 WHERE t1.I1 != ? ORDER BY I2";
    char        sDirectExecSampleQuery[100] = "INSERT INTO T1 VALUES ('F', 100)";

    char        sI1[100] = "TEST DATA TEST DATA ";
    SQLINTEGER  sI2 = 1;

    int         sPort;
    int         sAlternatePort;

    if(argc == 3)
    {
        sPort = atoi(argv[1]);
        sAlternatePort = atoi(argv[2]);
    }
    else
    {
        printf("[Usage] failoversample {Port} {AlternatePort} \n");
        return -1;
    }

    /* AlternateServers�� ������ attribute�̴�. 
     * connection string�� ��� �����ϰ� ���� ���� ���,
     * altibase_cli.ini���� Data Source section�� ����ϰ�,
     * [DataSource�̸�] 
     * AlternateServers=(128.1.3.53:20300,128.1.3.52:20301)
     * DSN=DataSource �̸��� ����ϸ� �ȴ�.
     */
    sprintf(sConnStr,
            "DSN=127.0.0.1;PORT=%d;UID=SYS;PWD=MANAGER;AlternateServers=(127.0.0.1:%d);"
            "ConnectionRetryCount=1;ConnectionRetryDelay=0;"
            "SessionFailOver=on;APP_INFO=failoversample", 
            sPort, sAlternatePort);

    sRetCode = SQLAllocHandle(SQL_HANDLE_ENV, NULL, &sEnv);
    test_error_exit(sRetCode,"SQLAllocHandle(ENV)");

    sRetCode = SQLAllocHandle(SQL_HANDLE_DBC, sEnv,  &sDbc);
    test_error_exit(sRetCode,"SQLAllocHandle(DBC)");

    sRetCode = SQLSetConnectAttr(sDbc, ALTIBASE_CONNECTION_RETRY_DELAY, (SQLPOINTER)1, 0);
    test_error_exit(sRetCode,"SQLSetConnectAttr(ALTIBASE_CONNECTION_RETRY_DELAY)");
    sRetCode = SQLSetConnectAttr(sDbc, ALTIBASE_CONNECTION_RETRY_COUNT, (SQLPOINTER)1, 0);
    test_error_exit(sRetCode,"SQLSetConnectAttr(ALTIBASE_CONNECTION_RETRY_COUNT)");

  retry_connect:
    /* connect to server */
    sRetCode = SQLDriverConnect(sDbc,
                                NULL,
                                (SQLCHAR *)sConnStr,
                                SQL_NTS,
                                NULL, 0, NULL,
                                SQL_DRIVER_NOPROMPT);
    if(sRetCode != SQL_SUCCESS)
    {
        /* CTF�� �Ұ����� ��Ȳ���� ������ �� ������ ������� �� �ִ�. */
        sRetCode = SQLDisconnect(sDbc);
        test_error_exit(sRetCode,"SQLDisconnect()");
        sleep(1);
        goto retry_connect;
    }

    /* autocommit off */
    sRetCode = SQLSetConnectAttr(sDbc, SQL_ATTR_AUTOCOMMIT, (void*)SQL_AUTOCOMMIT_OFF, 0);
    test_error_exit(sRetCode, "SQLSetConnectAttr(SQL_ATTR_AUTOCOMMIT)");

    sRetCode = SQLAllocStmt(sDbc, &sStmt);
    test_error_exit(sRetCode,"SQLAllocStmt()");

    sRetCode = SQLBindCol(sStmt, 1, SQL_C_SLONG, &sI2, 0, NULL);
    test_error_exit(sRetCode,"SQLBindCol()");

    sRetCode = SQLBindParameter(sStmt, 1, SQL_PARAM_INPUT,
                                SQL_C_CHAR, SQL_VARCHAR,
                                20, 0, (void*)sI1,
                                sizeof(sI1),
                                NULL);
    test_error_exit(sRetCode,"SQLBindParameter()");

retryDirect:
    sRetCode = SQLExecDirect(sStmt, (SQLCHAR *)sDirectExecSampleQuery , SQL_NTS);

    if(sRetCode != SQL_SUCCESS)
    {
        if(isFailOverErrorEvent(SQL_HANDLE_STMT, sStmt) == 1)
        {
            if(failoverRollback(sDbc) != 1)
            {
                /* distributed transaction rollback fail, disconnect and reconnect */
                sRetCode = SQLDisconnect(sDbc);
                test_error_exit(sRetCode,"SQLDisconnect()");
                printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds.\n");
                sleep(1);
                goto retry_connect;
            }
           /* Service Time Fail-Over�� �߻��ϸ�
            * ������ ������ Bind�� �ٽ� �������� �ʾƵ� �Ǹ�, 
            * SQLDirectExecute���� SQLPrepare�� �ʿ����� �ʰ�,
            * �ٽ� SQLDirectExecute�� �����ϸ� �ȴ�.
            */
            printf("failover event occur at prepare goto prepare.\n");
            goto retryDirect;
        }
        else if(isShardNodeRetryNotAvailable(sStmt) == 1)
        {
            sRetCode = SQLDisconnect(sDbc);
            test_error_exit(sRetCode,"SQLDisconnect()");
            printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds.\n");
            sleep(1);
            goto retry_connect;
        }
		else
		{
		    /* �Ϲ� ������ ���ؼ� ����� ���� ó�� ������ �����Ѵ�. */
		}
    }

  retry:
    sRetCode = SQLPrepare(sStmt, (SQLCHAR *)sPrepareExecSampleQuery, SQL_NTS);

    if(sRetCode != SQL_SUCCESS)
    {
        if(isFailOverErrorEvent(SQL_HANDLE_STMT, sStmt) == 1)
        {
            if(failoverRollback(sDbc) != 1)
            {
                /* distributed transaction rollback fail, disconnect and reconnect */
                sRetCode = SQLDisconnect(sDbc);
                test_error_exit(sRetCode,"SQLDisconnect()");
                printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds.\n");
                sleep(1);
                goto retry_connect;
            }
            /* Service Time Fail-Over�� �߻��ϸ� SQLPrepare���� �ٽ� �ؾ� �Ѵ�.
            * ������ ������ Bind�� �ٽ� �������� �ʾƵ� �ȴ�.
            */
            printf("failover event occur at prepare goto prepare\n");
            goto retry;
        }
        else if(isShardNodeRetryNotAvailable(sStmt) == 1)
        {
            printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds..\n");
            sRetCode = SQLDisconnect(sDbc);
            test_error_exit(sRetCode,"SQLDisconnect()");
            sleep(1);
            goto retry_connect;
        }

        printf("prepare fail\n");
        execute_err(sDbc, sStmt, sPrepareExecSampleQuery);
        test_error_exit(sRetCode,"SQLPrepare()");
    }
    else
    {
        printf("prepare success\n");
    }

    sRetCode = SQLExecute(sStmt);

    if(sRetCode != SQL_SUCCESS)
    {
        if(isFailOverErrorEvent(SQL_HANDLE_STMT, sStmt) == 1)
        {
            if(failoverRollback(sDbc) != 1)
            {
                /* distributed transaction rollback fail, disconnect and reconnect */
                sRetCode = SQLDisconnect(sDbc);
                test_error_exit(sRetCode,"SQLDisconnect()");
                printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds.\n");
                sleep(1);
                goto retry_connect;
            }
            printf("failover event occur at execute goto prepare\n");
            goto retry;
        }
        else if(isShardNodeRetryNotAvailable(sStmt) == 1)
        {
            printf("ALTIBASE_SHARD_NODE_FAIL_RETRY_CONNECTION_AVAILABLE!! Retry after 1 seconds..\n");
            sRetCode = SQLDisconnect(sDbc);
            test_error_exit(sRetCode,"SQLDisconnect()");
            sleep(1);
            goto retry_connect;
        }
        printf("execute fail\n");
        test_error_exit(sRetCode,"SQLExeccute()");
    }

    while ( (sRetCode = SQLFetch(sStmt)) != SQL_NO_DATA )
    {
        if(sRetCode != SQL_SUCCESS)
        {
            if(isFailOverErrorEvent(SQL_HANDLE_STMT, sStmt) == 1)
            {
                if(failoverRollback(sDbc) != 1)
                {
                    /* distributed transaction rollback fail, disconnect and reconnect */
                    sRetCode = SQLDisconnect(sDbc);
                    test_error_exit(sRetCode,"SQLDisconnect()");
                    printf("ALTIBASE_SHARD_NODE_FAILOVER_IS_NOT_AVAILABLE!! Retry after 1 seconds.\n");
                    sleep(1);
                    goto retry_connect;
                }
                printf("failover event occur at fetch goto prepare\n");
                /* Fetch�߿� FailOver�� �߻��ϸ� SQLCloseCursor�� �ؾ��Ѵ�.*/ 
                SQLCloseCursor(sStmt);
                goto retry;
            }
            else if(isShardNodeRetryNotAvailable(sStmt) == 1)
            {
                printf("ALTIBASE_SHARD_NODE_FAIL_RETRY_CONNECTION_AVAILABLE!! Retry after 1 seconds..\n");
                sRetCode = SQLDisconnect(sDbc);
                test_error_exit(sRetCode,"SQLDisconnect()");
                sleep(1);
                goto retry_connect;
            }

            printf("fetch fail\n");
            execute_err(sDbc, sStmt, sPrepareExecSampleQuery);
            test_error_exit(sRetCode,"SQLFetch()");
        }
        printf("Fetch Value = %d \n", sI2);
        fflush(stdout);
    }
    SQLEndTran(SQL_HANDLE_DBC, sDbc, SQL_COMMIT);
    test_error_exit(sRetCode,"SQLEndTran()");

    sRetCode = SQLFreeStmt(sStmt, SQL_DROP);
    test_error_exit(sRetCode,"SQLFreeStmt()");

    sRetCode = SQLDisconnect(sDbc);
    test_error_exit(sRetCode,"SQLDisconnect()");

    sRetCode = SQLFreeHandle(SQL_HANDLE_DBC, sDbc);
    test_error_exit(sRetCode,"SQLFreeHandle(DBC)");

    sRetCode = SQLFreeHandle(SQL_HANDLE_ENV, sEnv);
    test_error_exit(sRetCode,"SQLFreeHandle(ENV)");

    return 0;
}
