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

#ifndef _ULP_LIB_CONNECTION_H_
#define _ULP_LIB_CONNECTION_H_ 1

#include <acp.h>
#include <aciTypes.h>
#include <aciErrorMgr.h>
#include <sqlcli.h>
#include <ulpLibHash.h>
#include <ulpLibStmtCur.h>
#include <ulpLibOption.h>
#include <ulpLibErrorMgr.h>
#include <ulpLibMacro.h>
#include <ulpLibMultiErrorMgr.h>

/*
 * The node managed by connection hash table.
 */
typedef struct ulpLibConnNode ulpLibConnNode;
struct ulpLibConnNode
{
    SQLHENV        mHenv;
    SQLHDBC        mHdbc;
    SQLHSTMT       mHstmt;         /* single thread�� ��� �����Ǵ� statement handle */
    acp_char_t     mConnName[MAX_CONN_NAME_LEN + 1];  /* connection �̸� (AT��) */
    acp_char_t    *mUser;          /* user id */
    acp_char_t    *mPasswd;        /* password */
    acp_char_t    *mConnOpt1;
    acp_char_t    *mConnOpt2;      /* ù��° ConnOpt1 ���� ���� ������ ��� �̸� �̿��Ͽ� �õ� */

    /* statement hash table */
    ulpLibStmtHASHTAB mStmtHashT;

    /* cursor hash table */
    ulpLibStmtHASHTAB mCursorHashT;

    /* unnamed statement cache list */
    ulpLibStmtLIST mUnnamedStmtCacheList;

    /* TASK-7218 Handling Multiple Errors
     * �� ������ ConnNode ���� �δ� ����:
     *  ���� ������ �������̹Ƿ� ���� �޸� �Ҵ��� �Ұ����ѵ�,
     *  sqlcaó�� thread local ������ ����� ��� ������ ����� ����. 
     *  ����, ConnNode ���� �ξ ConnNode�� ������ �� �� ������
     *  �Ҵ�� �޸𸮵� �����ǵ��� ��.
     */
    ulpMultiErrorMgr mMultiErrorMgr;

    acp_bool_t       mIsXa;       /* Xa���� ���� ���� */
    acp_sint32_t     mXaRMID;
    acp_thr_rwlock_t mLatch4Xa;

    ulpLibConnNode *mNext;       /* bucket list link */

    /* ��밡���� �����ΰ��� ��Ÿ����.ulpConnect()���� ȣ��ÿ��� false�� �ʱ�ȭ�Ǹ�
       �������� true�� �ʱ�ȭ �ȴ�.*/
    acp_bool_t      mValid;
};


/*
 * The connection hash table
 */
typedef struct  ulpLibConnHashTab
{
    acp_sint32_t     mSize    ;        /* Max number of buckets in the table */
    acp_sint32_t     mNumNode ;        /* number of nodes currently in the table */
    acp_thr_rwlock_t mLatch;           /* table latch */
    acp_uint32_t     (*mHash) (acp_uint8_t *);       /* hash function */

    /* comparison funct, cmp(name,bucket_p); */
    acp_sint32_t     (*mCmp ) (
                       const acp_char_t*,
                       const acp_char_t*,
                       acp_size_t );

    /* actual hash table    */
    ulpLibConnNode *mTable[MAX_NUM_CONN_HASH_TAB_ROW];

} ulpLibConnHASHTAB;


/*
 * The connection hash table manager.
 */
typedef struct ulpLibConnMgr
{
    /* the connection hash table */
    ulpLibConnHASHTAB mConnHashTab;

    /* for connection concurrency */
    acp_thr_rwlock_t mConnLatch;

} ulpLibConnMgr;

extern ulpLibConnMgr gUlpConnMgr;


/*
 * Functions for managing connections.
 */

ACI_RC          ulpLibConInitialize( void );

void            ulpLibConFinalize( void );

/* 
 * create new connection node with aConnName,aValidInit.
 *     aConnName : connection name
 *     aValidInit: initial boolean value for mValid fileld 
 */
ulpLibConnNode *ulpLibConNewNode( acp_char_t *aConnName, acp_bool_t  aValidInit );

/* init default connection node */
void            ulpLibConInitDefaultConn( void );

/* default connection ��ü�� ���´� */
ulpLibConnNode *ulpLibConGetDefaultConn();

/* connection �̸��� ������ �ش� connection��ü�� ã�´� */
ulpLibConnNode *ulpLibConLookupConn(acp_char_t* aConName);

/* ���ο� connection�� ���� hash table�� �߰��Ѵ�. */
ulpLibConnNode *ulpLibConAddConn( ulpLibConnNode *aConnNode );

/* �ش� �̸��� connection�� �����Ѵ� */
ACI_RC          ulpLibConDelConn(acp_char_t* aConName);

/* hash table�� ��� ConnNode���� �����Ѵ� */
void            ulpLibConDelAllConn( void );

/* ConnNode �ڷᱸ�� ������. */
void            ulpLibConFreeConnNode( ulpLibConnNode *aConnNode );

/* BUG-28209 : AIX ���� c compiler�� �������ϸ� ������ ȣ��ȵ�. */
ACI_RC          ulpLibConInitConnMgr( void );


#endif
