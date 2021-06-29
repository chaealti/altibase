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

#ifndef _O_ULN_OBJECT_H_
#define _O_ULN_OBJECT_H_ 1

#include <ulnPrivate.h>
#include <uluLock.h>

/*
 * uln���� ���Ǵ� ��ü���� Ÿ�Ե�.
 */
typedef enum
{
    ULN_OBJ_TYPE_MIN  = SQL_HANDLE_ENV - 1,

    /*
     * SQL_HANDLE_xxx�� ���߱� ���ؼ� 1���� ������.
     */

    ULN_OBJ_TYPE_ENV  = SQL_HANDLE_ENV,
    ULN_OBJ_TYPE_DBC  = SQL_HANDLE_DBC,
    ULN_OBJ_TYPE_STMT = SQL_HANDLE_STMT,
    ULN_OBJ_TYPE_DESC = SQL_HANDLE_DESC,

    ULN_OBJ_TYPE_SQL,

    ULN_OBJ_TYPE_MAX

} ulnObjType;

typedef enum
{
    ULN_DESC_TYPE_NODESC,
    ULN_DESC_TYPE_APD,
    ULN_DESC_TYPE_ARD,
    ULN_DESC_TYPE_IRD,
    ULN_DESC_TYPE_IPD,
    ULN_DESC_TYPE_MAX
} ulnDescType;

#define ULN_OBJ_GET_TYPE(x)         ( (ulnObjType)(((ulnObject *)(x))->mType) )
#define ULN_OBJ_SET_TYPE(x, a)      ( ((ulnObject *)(x))->mType = (a) )

#define ULN_OBJ_GET_DESC_TYPE(x)    ( (ulnDescType)(((ulnObject *)(x))->mDescType) )
#define ULN_OBJ_SET_DESC_TYPE(x, a) ( (((ulnObject *)(x))->mDescType) = (a))

#define ULN_OBJ_GET_STATE(x)        ( (acp_uint16_t)(((ulnObject *)(x))->mState) )
#define ULN_OBJ_SET_STATE(x, a)     ( ((ulnObject *)(x))->mState = (a) )


#define ULN_OBJ_MALLOC(aObj, aPP, aSize)                                    \
    ((ulnObject*)aObj)->mMemory->mOp->mMalloc(((ulnObject*)aObj)->mMemory,  \
                                              ((void**) aPP),               \
                                              aSize)

/*
 * BUGBUG: lock�� ���õ� ���Ϸ� �̵��Ǿ�� �Ѵ� {{{
 */
typedef enum
{
    ULN_MUTEX,  /* Synchronize by PDL mutex     */
    ULN_CONDV   /* Synchronize by Cond Variable */
} ulnLockType;

/*
 * Header portion of following Objects :
 *      ulnEnv
 *      ulnDbc
 *      ulnDbd
 *      ulnStmt
 *      ulnDesc
 */
struct ulnObject
{
    acp_list_node_t      mList;

    /* BUG-38755 Improve to check invalid handle in the CLI */
    volatile ulnObjType  mType;        /* Object Type */
    ulnDescType          mDescType;    /* Descriptor Type */

    acp_sint16_t         mState;

    /*
     * Statement ulnDescStmt �� S11, S12 ������Ʈ����
     * NS ��� �������̰� �ִµ� �̿� �ش��ϴ� state
     * ������ ������ �־�� �� �ʿ䰡 ������ ���� �ʵ���.
     * �� �ʵ尡 0 �� �ƴϸ� �� STMT �� ���� �Ҹ�����
     * �Լ��� ����ǰ� �ִ� ���̶�� ���� ��Ÿ��.
     */
    ulnFunctionId    mExecFuncID;

    uluChunkPool    *mPool;
    uluMemory       *mMemory;

    acp_thr_mutex_t *mLock;              /* Lock Data Pointer */

    ulnDiagHeader    mDiagHeader;        /* Header of Diagnostic Messages */

    /*
     * �Ʒ��� ����� SQLError() �Լ��� SQLGetDiagRec() �Լ����� ���ζ����� �����Ѵ�.
     * uloSqlWrapper.cpp �� SQLError() �Լ��� ���� ���ʸ� �� �� �ִ�.
     */
    acp_sint32_t     mSqlErrorRecordNumber;
};

#define ULN_OBJECT_LOCK(aObject, aFuncID)   \
    acpThrMutexLock((aObject)->mLock)

#define ULN_OBJECT_UNLOCK(aObject, aFuncID)     \
    do                                          \
    {                                           \
        acpThrMutexUnlock((aObject)->mLock);    \
        ACP_UNUSED(aFuncID);                    \
    } while (0)

/*
 * Function Declarations
 */
ACI_RC ulnObjectInitialize(ulnObject    *aObject,
                           ulnObjType    aType,
                           ulnDescType   aSubType,
                           acp_sint16_t  aState,
                           uluChunkPool *aPool,
                           uluMemory    *aMemory);

#endif /* _ULN_OBJECT_H_ */

