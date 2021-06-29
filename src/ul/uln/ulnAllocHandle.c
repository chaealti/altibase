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
#include <ulnAllocHandle.h>

/*
 * State transition functions
 *
 * AllocHandle �� ��� ulnFnContext �� *mArgs �� SShort aHandleType �� �Ѿ�´�.
 */

/*
 * ULN_SFID_90
 *      env
 *          --[4]
 *      dbc
 *          E2[5]
 *          (HY010)[6]
 *      stmt and desc
 *          (IH)
 *  where
 *      [4] Calling SQLAllocHandle with OutputHandlePtr pointing to a valid handle overwrites
 *          that handle. This might be an application programming error.
 *      [5] The SQL_ATTR_ODBC_VERSION environment attribute had been set on the environment.
 *      [6] The SQL_ATTR_ODBC_VERSION environment attribute had NOT been set on the environment.
 */
ACI_RC ulnSFID_90(ulnFnContext *aContext)
{
    acp_sint16_t sHandleType;

    sHandleType = *(acp_sint16_t *)(aContext->mArgs);

    switch(sHandleType)
    {
        case SQL_HANDLE_ENV:
            break;

        case SQL_HANDLE_DBC:
            if(ulnEnvGetOdbcVersion(aContext->mHandle.mEnv) == 0)
            {
                /*
                 * [6] ODBC version's NOT been set
                 */
                ACI_RAISE(LABEL_FUNC_SEQUENCE_ERR);
            }
            else
            {
                if(aContext->mWhere == ULN_STATE_EXIT_POINT)
                {
                    ULN_OBJ_SET_STATE(aContext->mHandle.mEnv, ULN_S_E2);
                }
            }
            break;

        case SQL_HANDLE_STMT:
        case SQL_HANDLE_DESC:
            ACI_RAISE(LABEL_INVALID_HANDLE);
            break;

        default:
            ACI_RAISE(LABEL_INVALID_HANDLE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_FUNC_SEQUENCE_ERR)
    {
        ulnError(aContext, ulERR_ABORT_FUNCTION_SEQUENCE_ERR);
    }

    ACI_EXCEPTION(LABEL_INVALID_HANDLE)
    {
        ulnError(aContext, ulERR_ABORT_INVALID_HANDLE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ULN_SFID_60
 * SQLAllocHandle, DBC, C2, C3
 */
ACI_RC ulnSFID_60(ulnFnContext *aContext)
{
    acp_sint16_t sHandleType;

    sHandleType = *(acp_sint16_t *)(aContext->mArgs);

    switch(sHandleType)
    {
        case SQL_HANDLE_ENV:
        case SQL_HANDLE_DBC:
            break;

        case SQL_HANDLE_STMT:
        case SQL_HANDLE_DESC:
            ACI_RAISE(LABEL_NO_CONNECTION);
            break;

        default:
            ACI_RAISE(LABEL_INVALID_HANDLE_TYPE);
            break;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NO_CONNECTION)
    {
        /*
         * 08003
         */
        ulnError(aContext, ulERR_ABORT_NO_CONNECTION, "");
    }

    ACI_EXCEPTION(LABEL_INVALID_HANDLE_TYPE)
    {
        /*
         * HY092
         */
        ulnError(aContext, ulERR_ABORT_INVALID_ATTR_OPTION, sHandleType);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ULN_SFID_61
 */
ACI_RC ulnSFID_61(ulnFnContext *aContext)
{
    acp_sint16_t sHandleType;

    if(aContext->mWhere == ULN_STATE_EXIT_POINT)
    {
        sHandleType = *(acp_sint16_t *)(aContext->mArgs);

        switch(sHandleType)
        {
            case SQL_HANDLE_ENV:
            case SQL_HANDLE_DBC:
            case SQL_HANDLE_DESC:
                break;

            case SQL_HANDLE_STMT:
                ULN_OBJ_SET_STATE(aContext->mHandle.mDbc, ULN_S_C5);
                break;

            default:
                ACI_RAISE(LABEL_INVALID_HANDLE_TYPE);
                break;
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_HANDLE_TYPE)
    {
        /*
         * HY092
         */
        ulnError(aContext, ulERR_ABORT_INVALID_ATTR_OPTION, sHandleType);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC ulnAllocHandleCheckArgs(ulnFnContext *aContext,
                                      ulnObjType    aObjType,
                                      ulnObject    *aInputHandle,
                                      ulnObject   **aOutputHandlePtr)
{
    ACI_TEST_RAISE( (aInputHandle == NULL && aObjType != ULN_OBJ_TYPE_ENV), LABEL_INVALID_HANDLE );

    ACI_TEST_RAISE( aOutputHandlePtr == NULL, LABEL_INVALID_USE_OF_NULL );

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_HANDLE)
    {
        /*
         * Invalid Handle
         */
        ulnError(aContext, ulERR_ABORT_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_INVALID_USE_OF_NULL)
    {
        /*
         * HY009 : Invalid Use of Null Pointer
         */
        ulnError(aContext, ulERR_ABORT_INVALID_USE_OF_NULL_POINTER);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * ulnAllocEnv.
 *
 * ENV ��ü�� �ϳ� �Ҵ��ؼ� �� �����͸� �����Ѵ�.
 *
 * @param[in] aContext
 *  ulnAllocHandle() �� Context �� ����Ű�� ������.
 *  �� Context �� ���� �ڵ��� ����Ű�� �����Ͱ� ��� �ִ�.
 *  �� �Լ��� ���, �����ڵ��� �����Ƿ� aContext->mHandle.mObject �� NULL �� ��� �ִ�.
 * @param[out] aOutputHandlePtr
 *  �Լ� ���� ����� �Ҵ�� ENV ��ü�� �����Ͱ� ����� ������ ������.
 */
static SQLRETURN ulnAllocHandleEnv(ulnEnv **aOutputHandlePtr)
{
    ulnEnv *sEnv;
    /*
     * ENV �� ������ ����� ���̹Ƿ� Context �� �ʿ� ����.
     */

    /*
     * ENV�� �ν��Ͻ� ����
     */
    ACI_TEST(ulnEnvCreate(&sEnv) != ACI_SUCCESS);

    /*
     * BUGBUG: �ν��Ͻ��� �����ϰ� ���� lock �ؾ� ���� ������?
     */

    /*
     * ulnEnv �� ������ �κ��� �ʱ�ȭ (����Ʈ ������)
     */
    ulnEnvInitialize(sEnv);

    /* initialize the CM module */
    //!BUGBUG!//  IDE_ASSERT(cmiInitialize() == ACI_SUCCESS);

    *aOutputHandlePtr = sEnv;

    return SQL_SUCCESS;

    ACI_EXCEPTION_END;

    return SQL_ERROR;
}

/**
 * ulnAllocDbc.
 *
 * DBC ��ü�� �ϳ� �Ҵ��ؼ� �� �����͸� �����Ѵ�.
 *
 * @param[in] aContext
 *  ulnAllocHandle() �� Context �� ����Ű�� ������.
 *  �� Context �� ���� �ڵ��� ����Ű�� �����Ͱ� ��� �ִ�.
 * @param[out] aOutputHandlePtr
 *  �Ҵ�� DBC ��ü�� �����Ͱ� ����� ������ ������
 *
 * MSDN ODBC 3.0 :
 * Application ��, ������ ���� ������ ���� �ڵ���
 * InputHandle �� �Ŵ޷� �ִ� Diagnostic ����ü�κ���
 * ���� �� �ִ�.
 * --> ��, �����ڵ��� DiagHeader �� �߻��� ������ �־� �־�� �Ѵ�.
 */
static SQLRETURN ulnAllocHandleDbc(ulnEnv *aEnv, ulnDbc **aOutputHandlePtr)
{
    ulnDbc       *sDbc;
    ulnFnContext  sFnContext;

    acp_bool_t    sNeedExit   = ACP_FALSE;
    acp_sint16_t  sHandleType = SQL_HANDLE_DBC;

    ULN_INIT_FUNCTION_CONTEXT(sFnContext,
                              ULN_FID_ALLOCHANDLE,
                              aEnv,
                              ULN_OBJ_TYPE_ENV);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, (void *)(&sHandleType)) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;

    /*
     * Check if arguments are valid.
     */
    ACI_TEST(ulnAllocHandleCheckArgs(&sFnContext,
                                     ULN_OBJ_TYPE_DBC,
                                     (ulnObject *)aEnv,
                                     (ulnObject **)aOutputHandlePtr) != ACI_SUCCESS);

    /*
     * DBC �ν��Ͻ� ����
     */
    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    ACI_TEST_RAISE(ulnDbcCreate(&sDbc) != ACI_SUCCESS, MEMORY_ERROR);

    /*
     * DBC �ν��Ͻ��� �ʱ�ȭ
     */
    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    ACI_TEST_RAISE(ulnDbcInitialize(&sFnContext, sDbc) != ACI_SUCCESS, INIT_ERROR);

    /*
     * DBC �� ENV �� �Ŵޱ�
     */
    ulnEnvAddDbc(sFnContext.mHandle.mEnv, sDbc);

    /*
     * Inheration Of ODBC Version, but could be overvrite
     * by Connection String !!!
     */
    sDbc->mOdbcVersion = (acp_uint32_t)sFnContext.mHandle.mEnv->mOdbcVersion;

    /* 
     * BUG-35332 The socket files can be moved
     *
     * ȯ�溯�� or ������Ƽ ���Ͽ��� ���� ������ �ʱ�ȭ�Ѵ�.
     */
    ulnStrCpyToCStr( sDbc->mUnixdomainFilepath,
                     IPC_FILE_PATH_LEN,
                     &aEnv->mProperties.mUnixdomainFilepath );

    ulnStrCpyToCStr( sDbc->mIpcFilepath,
                     IPC_FILE_PATH_LEN,
                     &aEnv->mProperties.mIpcFilepath );
    
    ulnStrCpyToCStr( sDbc->mIPCDAFilepath,
                     IPC_FILE_PATH_LEN,
                     &aEnv->mProperties.mIPCDAFilepath );
    sDbc->mIPCDAMicroSleepTime = aEnv->mProperties.mIpcDaClientSleepTime;
    sDbc->mIPCDAULExpireCount  = aEnv->mProperties.mIpcDaClientExpireCount;

    *aOutputHandlePtr = sDbc;

    /*
     * Exit
     */
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    ACI_EXCEPTION(MEMORY_ERROR);
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "ulnAllocHandleDbc");
    }
    ACI_EXCEPTION(INIT_ERROR);
    {
        ulnDbcDestroy(sDbc);
    }
    ACI_EXCEPTION_END;

    if(sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

/**
 * ulnAllocStmt.
 *
 * STMT ��ü�� �ϳ� �Ҵ��ؼ� �� �����͸� �Ѱ��ش�.
 *
 * @param[in] aContext
 *  ulnAllocHandle() �� Context �� ����Ű�� ������.
 *  �� Context �� ���� �ڵ��� ����Ű�� �����Ͱ� ��� �ִ�.
 * @param[out] aOutputHandlePtr
 *  �Ҵ�� STMT ��ü�� �����Ͱ� ����� ������ ������
 *
 * �Լ� ����� �߻��� ������ ���� �ڵ��� DBC ������ Diagnostic
 * ����ü���ٰ� �Ŵ޾� �־�� �Ѵ�.
 */
static SQLRETURN ulnAllocHandleStmt(ulnDbc *aDbc, ulnStmt **aOutputHandlePtr)
{
    ulnDbc       *sDbc;
    ulnStmt      *sStmt;
    ulnFnContext  sFnContext;

    acp_bool_t    sNeedExit   = ACP_FALSE;
    acp_sint16_t  sHandleType = SQL_HANDLE_STMT;

    /* PROJ-1573 XA */
    sDbc = ulnDbcSwitch(aDbc);

    ULN_INIT_FUNCTION_CONTEXT(sFnContext,
                              ULN_FID_ALLOCHANDLE,
                              sDbc,
                              ULN_OBJ_TYPE_DBC);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, (void *)(&sHandleType)) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;
    /*
     * Check if arguments are valid.
     */
    ACI_TEST(ulnAllocHandleCheckArgs(&sFnContext,
                                     ULN_OBJ_TYPE_STMT,
                                     (ulnObject *)sDbc,
                                     (ulnObject **)aOutputHandlePtr) != ACI_SUCCESS);

    /* PROJ-2177 Stmt ���� ���� */
    ACI_TEST_RAISE(ulnDbcGetStmtCount(sDbc) >= ULN_DBC_MAX_STMT, TOO_MANY_STMT_ERROR);

    /*
     * ulnStmt �ν��Ͻ� ����
     */
    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    ACI_TEST_RAISE(ulnStmtCreate(sDbc, &sStmt) != ACI_SUCCESS, MEMORY_ERROR);

    /*
     * ulnStmt�ʱ�ȭ
     */
    ulnStmtInitialize(sStmt);

    /*
     * DBC �� �Ŵޱ�
     */
    ulnDbcAddStmt(sDbc, sStmt);

    /* PROJ-2616 IPCDA Cache Buffer �����ϱ� */
    ulnCacheCreateIPCDA(&sFnContext, sDbc);

    /* �ʿ��� DBC �� �Ӽ����� ��ӹޱ� */
    ulnStmtSetAttrPrefetchRows(sStmt, ulnDbcGetAttrFetchAheadRows(sStmt->mParentDbc));
    ulnStmtSetAttrDeferredPrepare(sStmt, ulnDbcGetAttrDeferredPrepare(sStmt->mParentDbc));

    *aOutputHandlePtr = sStmt;

    /* Exit */
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    /* PROJ-2177 Stmt ���� ���� */
    ACI_EXCEPTION(TOO_MANY_STMT_ERROR);
    {
        ulnError(&sFnContext, ulERR_ABORT_TOO_MANY_STATEMENT);
    }
    // BUG-25315 [CodeSonar] �ʱ�ȭ���� �ʴ� ���� ���
    ACI_EXCEPTION(MEMORY_ERROR);
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "AllocHandleStmt");
    }
    ACI_EXCEPTION_END;

    if(sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }
    
    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

/**
 * ulnAllocHandleDesc.
 *
 * @param[in] aContext
 *  ulnAllocHandle() �� Context �� ����Ű�� ������.
 *  �� Context �� ���� �ڵ��� ����Ű�� �����Ͱ� ��� �ִ�.
 * @param[out] aOutputHandlePtr
 *  �Ҵ�� DESC ��ü�� �����Ͱ� ����� ������ ������
 *
 * ��ũ���͸� �Ҵ��Ѵ�.
 * �� ���� ����ڰ� SQLAllocHandle()�Լ��� �̿��ؼ� Explicit�ϰ� �Ҵ��ϴ� ����̴�.
 * ����ڴ� APD�� ARD�� ������ �� �ִ�.
 */
static SQLRETURN ulnAllocHandleDesc(ulnDbc *aDbc, ulnDesc **aOutputHandlePtr)
{
    ulnDbc       *sDbc;
    ulnDesc      *sDesc;
    ulnFnContext  sFnContext;

    acp_bool_t    sNeedExit   = ACP_FALSE;
    acp_sint16_t  sHandleType = SQL_HANDLE_DESC;

    /* PROJ-1573 XA */

    sDbc = ulnDbcSwitch(aDbc);

    ULN_INIT_FUNCTION_CONTEXT(sFnContext,
                              ULN_FID_ALLOCHANDLE,
                              sDbc,
                              ULN_OBJ_TYPE_DBC);

    /*
     * Enter
     */
    ACI_TEST(ulnEnter(&sFnContext, (void *)(&sHandleType)) != ACI_SUCCESS);
    sNeedExit = ACP_TRUE;

    /*
     * ulnDesc ����, �ʱ�ȭ
     */
    ACI_TEST_RAISE(ulnDescCreate(sFnContext.mHandle.mObj,
                                 &sDesc,
                                 ULN_DESC_TYPE_ARD, /* BUGBUG: �ϴ� ARD �� Ÿ���� ����  */
                                 ULN_DESC_ALLOCTYPE_EXPLICIT) != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM);

    ACI_TEST_RAISE(ulnDescInitialize(sDesc, sFnContext.mHandle.mObj) != ACI_SUCCESS,
                   LABEL_NOT_ENOUGH_MEM_ROLLBACK);
    ulnDescInitializeUserPart(sDesc);

    /*
     * DBC �� �Ŵޱ�
     */
    ulnDbcAddDesc(sFnContext.mHandle.mDbc, sDesc);

    /*
     * Note: DESC �� ���� ���̴� �ʿ� ����. �Ҵ� �����ϸ� ������ 1
     */

    *aOutputHandlePtr = sDesc;

    /*
     * Exit
     */
    ACI_TEST(ulnExit(&sFnContext) != ACI_SUCCESS);

    return ULN_FNCONTEXT_GET_RC(&sFnContext);

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM_ROLLBACK)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "AllocHandleDesc rollback");

        if(ulnDescDestroy(sDesc) != ACI_SUCCESS)
        {
            ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "AllocHandleDesc rollback");
        }
    }

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        ulnError(&sFnContext, ulERR_FATAL_MEMORY_ALLOC_ERROR, "AllocHandleDesc");
    }

    ACI_EXCEPTION_END;

    if(sNeedExit == ACP_TRUE)
    {
        ulnExit(&sFnContext);
    }

    return ULN_FNCONTEXT_GET_RC(&sFnContext);
}

/**
 * ulnAllocHandle.
 *
 * SQLAllocHandle �� ���� ���εǴ� �Լ��̴�.
 * ODBC 3.0 ������ ���忡�� ���Ǵ� SQLAllocEnv, SQLAllocDbc, SQLAllocStmt����
 * �Լ����� ���� ulnAllocEnv�� ���� �Լ��� ���� �θ��� ���� ulnAllocHandle �Լ���
 * ȣ���ϴ� ������� ������ �̷������ �ϰڴ�.
 * ��, �� �Լ��� ��� SQLAllocXXXX �Լ��� uln ������ ��Ʈ�� ����Ʈ�� �ϰڴ�.
 */
SQLRETURN ulnAllocHandle(acp_sint16_t   aHandleType,
                         void          *aInputHandle,
                         void         **aOutputHandlePtr)
{
    SQLRETURN sReturnCode;
    acp_char_t  sTypeStr[80];

	switch(aHandleType)
	{
        case SQL_HANDLE_ENV:
            sReturnCode = ulnAllocHandleEnv((ulnEnv**)aOutputHandlePtr);
            acpSnprintf(sTypeStr , ACI_SIZEOF(sTypeStr), "%s", "env");
            break;

        case SQL_HANDLE_DBC:
            sReturnCode = ulnAllocHandleDbc((ulnEnv *)aInputHandle, (ulnDbc**)aOutputHandlePtr);
            acpSnprintf(sTypeStr , ACI_SIZEOF(sTypeStr), "%s", "dbc");
            break;

        case SQL_HANDLE_STMT:
            sReturnCode = ulnAllocHandleStmt((ulnDbc *)aInputHandle, (ulnStmt **)aOutputHandlePtr);
            acpSnprintf(sTypeStr , ACI_SIZEOF(sTypeStr), "%s", "stmt");
            break;

        case SQL_HANDLE_DESC:
            sReturnCode = ulnAllocHandleDesc((ulnDbc *)aInputHandle, (ulnDesc **)aOutputHandlePtr);
            acpSnprintf(sTypeStr , ACI_SIZEOF(sTypeStr), "%s", "desc");
            break;

        default:
            /*
             * HY092 : Invalid Attribute/Option Identifier
             *
             * BUGBUG:
             * ���⼭�� context �� �����Ƿ� ��ü�� ��ȿ�� ��쿡 ��� lock �� �ϰ�,
             * �׷� �� ���� diagnostic record �� �ž�޵��� ����.
             */
            // ACI_TEST_RAISE(ulnProcessErrorSituation(&sFnContext, 1, ULN_EI_HY092)
                           // != ACI_SUCCESS,
                           // ULN_LABEL_NEED_EXIT);
            sReturnCode = SQL_ERROR;
            acpSnprintf(sTypeStr , ACI_SIZEOF(sTypeStr), "%s", "invalid");
            break;
	}

    /*
     * MSDN ODBC 3.0 SQLAllocHandle() Returns ���� :
     * �Լ��� SQL_ERROR �� �����ϸ� OutputHandlePtr �� NULL �� �����Ѵ�.
     */
    if(sReturnCode == SQL_ERROR)
    {
        *aOutputHandlePtr = NULL;
    }

    ULN_TRACE_LOG(NULL, ULN_TRACELOG_MID, NULL, 0,
            "%-18s| %-4s [in: %p out: %p] %"ACI_INT32_FMT,
            "ulnAllocHandle", sTypeStr,
            aInputHandle, *aOutputHandlePtr, sReturnCode);

    return sReturnCode;
}

/**
 * SQL_API_SQLALLOCHANDLESTD (73) - ISO-92 std.
 * AllocateHandle Function, SQL_OV_ODBC3 - default
 */
SQLRETURN ulnAllocHandleStd(acp_sint16_t   aHandleType,
                            void          *aInputHandle,
                            void         **aOutputHandlePtr)
{
    SQLRETURN sRetCode = ulnAllocHandle(aHandleType,
                                        aInputHandle,
                                        aOutputHandlePtr);

    if( SQL_SUCCEEDED(sRetCode) && aHandleType == SQL_HANDLE_ENV)
    {
        (void)ulnEnvSetOdbcVersion( *((ulnEnv**)aOutputHandlePtr), SQL_OV_ODBC3);
    }
    return sRetCode;
}
