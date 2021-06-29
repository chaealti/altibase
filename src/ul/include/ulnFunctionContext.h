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

#ifndef _O_ULN_FUNCTION_CONTEXT_H_
#define _O_ULN_FUNCTION_CONTEXT_H_ 1

#define ULN_ARRAY_EXEC_RES_SUCCESS      0x00000001
#define ULN_ARRAY_EXEC_RES_ERROR        0x00000002

#define ULN_ARRAY_EXEC_RES_SUCCESS_WITH_INFO \
            (ULN_ARRAY_EXEC_RES_ERROR | ULN_ARRAY_EXEC_RES_SUCCESS)

struct ulnFnContext
{
    union
    {
        ulnObject      *mObj;
        ulnEnv         *mEnv;
        ulnDbc         *mDbc;
        ulnStmt        *mStmt;
        ulnDesc        *mDesc;
    } mHandle;

    ulnObjType          mObjType;

    ulnFunctionId       mFuncID;

    ulnStateCheckPoint  mWhere;
    ulnStateFunc       *mStateFunc;

    /*
     * �Ʒ��� mErrIndex �� state machine ������ �ʿ��ϸ� �ٸ� �������� 
     * ����� �ʿ����� �ʴ�.
     * �Ű澲�� �ʵ��� �Ѵ�.
     */
    acp_uint32_t        mUlErrorCode;

    /*
     * Array Execution �� SQL_SUCCESS_WITH_INFO �� ������ �ֱ� ���ؼ� �ʿ��ϴ�.
     * EXEC RES �� ������ SUCCESS �÷��׸� on,
     * ERROR RES �� ������ ERROR �÷��׸� on.
     * SQLRETURN �� ������ ������
     *      ERROR �÷��׸� on �̸� SQL_ERROR,
     *      SUCCESS �÷��׸� on �̸� SQL_SUCCESS
     *      �� �� on �̸� SQL_SUCCESS_WITH_INFO
     */
    acp_uint32_t        mArrayExecutionResult;

    SQLRETURN           mSqlReturn;

    void               *mArgs;
    /* PROJ-1573 XA */
    acp_sint32_t        mXaResult;
    acp_sint32_t        mXaXidPos;
    void               *mXaRecoverXid;

    /* PROJ-2177 User Interface - Cancel */
    acp_bool_t          mNeedUnlock;

    /* BUG-46092 */
    acp_bool_t          mIsFailoverSuccess;

    ulnShardCoordFixCtrlContext * mShardCoordFixCtrlCtx;
};

#define ULN_FNCONTEXT_SET_RC(aFnContext, aRC)                                                 \
    do {                                                                                      \
        if((aFnContext)->mHandle.mObj != NULL)                                                \
        {                                                                                     \
            ulnDiagSetReturnCode(&((aFnContext)->mHandle.mObj->mDiagHeader), (aRC));          \
            /* PROJ-2177: NEED DATA�� �߻��� �Լ� ��� */                                     \
            if (((aRC) == SQL_NEED_DATA)                                                      \
             && ((aFnContext)->mFuncID != ULN_FID_PARAMDATA))                                 \
            {                                                                                 \
                ulnStmtSetNeedDataFuncID((aFnContext)->mHandle.mStmt, (aFnContext)->mFuncID); \
            }                                                                                 \
        }                                                                                     \
        (aFnContext)->mSqlReturn = (aRC);                                                     \
    } while(0)

#define ULN_FNCONTEXT_GET_RC(aFnContext)        ((aFnContext)->mSqlReturn)

#define ULN_INIT_FUNCTION_CONTEXT(aContext, aFuncId, aObj, aObjType) \
    do {                                                             \
        aContext.mFuncID      = aFuncId;                             \
        aContext.mHandle.mObj = (ulnObject *)aObj;                   \
        aContext.mObjType     = aObjType;                            \
        aContext.mArgs        = NULL;                                \
        aContext.mWhere       = ULN_STATE_NOWHERE;                   \
        aContext.mSqlReturn   = SQL_SUCCESS;                         \
        aContext.mUlErrorCode = ulERR_IGNORE_NO_ERROR;               \
        aContext.mArrayExecutionResult = 0;                          \
        ULN_FNCONTEXT_SET_RC(&aContext, SQL_SUCCESS);                \
        /* PROJ-1573 XA */                                           \
        aContext.mXaResult    = 0;                                   \
        aContext.mXaXidPos    = 0;                                   \
        aContext.mXaRecoverXid = NULL;                               \
        /* PROJ-2177: User Interface - Cancel */                     \
        aContext.mNeedUnlock  = ACP_FALSE;                           \
        aContext.mIsFailoverSuccess = ACP_FALSE;                     \
        aContext.mShardCoordFixCtrlCtx = NULL;                       \
    } while(0)

#define ULN_FNCONTEXT_GET_DBC(aFnContext, aDbc)                                \
    do {                                                                       \
        ulnObject *sParentObject = NULL;                                       \
        switch((aFnContext)->mObjType)                                         \
        {                                                                      \
            case ULN_OBJ_TYPE_DBC:                                             \
                (aDbc) = (aFnContext)->mHandle.mDbc;                           \
                break;                                                         \
            case ULN_OBJ_TYPE_STMT:                                            \
                (aDbc) = (aFnContext)->mHandle.mStmt->mParentDbc;              \
                break;                                                         \
            case ULN_OBJ_TYPE_DESC:  /* BUG-46113 */                           \
                sParentObject = (aFnContext)->mHandle.mDesc->mParentObject;    \
                if (ULN_OBJ_GET_TYPE(sParentObject) == ULN_OBJ_TYPE_DBC)       \
                {                                                              \
                    (aDbc) = (ulnDbc *)sParentObject;                          \
                }                                                              \
                else if (ULN_OBJ_GET_TYPE(sParentObject) == ULN_OBJ_TYPE_STMT) \
                {                                                              \
                    (aDbc) = ((ulnStmt *)sParentObject)->mParentDbc;           \
                }                                                              \
                else                                                           \
                {                                                              \
                    (aDbc) = NULL;  /* non-reachable */                        \
                }                                                              \
                break;                                                         \
            default:                                                           \
                (aDbc) = NULL;                                                 \
                break;                                                         \
        }                                                                      \
    } while(0)

ACP_INLINE ulnDbc* ulnFnContextGetDbc(ulnFnContext *aFnContext)
{
    ulnObject *sParentObject = NULL;

    switch (aFnContext->mObjType)
    {
        case ULN_OBJ_TYPE_DBC:
            return aFnContext->mHandle.mDbc;
        case ULN_OBJ_TYPE_STMT:
            return aFnContext->mHandle.mStmt->mParentDbc;
        case ULN_OBJ_TYPE_DESC:  /* BUG-46113 */
            sParentObject = (aFnContext)->mHandle.mDesc->mParentObject;
            if (ULN_OBJ_GET_TYPE(sParentObject) == ULN_OBJ_TYPE_DBC)
            {
                return (ulnDbc *)sParentObject;
            }
            else if (ULN_OBJ_GET_TYPE(sParentObject) == ULN_OBJ_TYPE_STMT)
            {
                return ((ulnStmt *)sParentObject)->mParentDbc;
            }
            else
            {
                return NULL;  /* non-reachable */
            }
        default:
            return NULL;
    }
}

#endif /* _O_ULN_FUNCTION_CONTEXT_H_ */
