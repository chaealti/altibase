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

#ifndef _ULP_LIB_STMTCUR_H_
#define _ULP_LIB_STMTCUR_H_ 1

#include <acp.h>
#include <aciTypes.h>
#include <aciErrorMgr.h>
#include <sqlcli.h>
#include <ulpLibHash.h>
#include <ulpLibInterface.h>
#include <ulpLibErrorMgr.h>
#include <ulpLibOption.h>
#include <ulpLibMacro.h>
#include <ulpTypes.h>

/* ulpBindHostVarCore(...) �Լ� ������ ulpBindInfo �ʵ尪���� setting�Ѵ�.             *
 * row/column-wise binding�� ulpSetStmtAttrCore(...) �Լ����� �ش� �ʵ尪���� �����Ѵ�. */
typedef struct ulpStmtBindInfo
{
    acp_uint16_t mNumofInHostvar;          /* �ش� ������ host ���� ������ ���´�. */
    acp_uint16_t mNumofOutHostvar;
    acp_uint16_t mNumofExtraHostvar;
    acp_bool_t   mIsScrollCursor;    /* Scrollable Cusor�� �����ϸ� true���� ���´�.   *
                                      * �̰��� ���� �߰����� statement �Ӽ��� �������ش�. */
    acp_bool_t   mIsArray;           /* column-wise binding�� �ʿ����� ����,         *
                                      * ��� host ������ Ÿ���� array�� ��� ���� �ȴ�. */
    acp_uint32_t mArraySize;         /* array size��.  */
    acp_bool_t   mIsStruct;          /* row-wise binding�� �ʿ����� ����. */
    acp_uint32_t mStructSize;        /* size of struct*/
    acp_bool_t   mIsAtomic;

    acp_bool_t   mCursorScrollable;
    acp_bool_t   mCursorSensitivity;
    acp_bool_t   mCursorWithHold;

} ulpStmtBindInfo;


/* binding�� host variable/indicator pointer�� �����ϴ� �ڷᱸ��. */
typedef struct  ulpHostVarPtr
{
    void*        mHostVar;           /* host���� ��*/
    /* BUG-27833 :
        ������ �ٸ����� �����Ͱ� ������ �ֱ� ������ type���� ���ؾ���. */
    acp_sint16_t mHostVarType;       /* host���� type*/
    void*        mHostInd;           /* host������ indicator ��*/

} ulpHostVarPtr;

/* BUG-45779 APRE���� PSM ���ڷ� Array type�� �����ؾ� �մϴ�. */
#define APRE_PSM_ARRAY_META_VERSION 808377001

/* BUG-45779 server���� ����� ���� meta�����̸�, ��ü size��  8�� ������� �Ѵ�. */
typedef struct psmArrayMetaInfo
{
    acp_uint32_t    mUlpVersion;            /* VERSION                       */
    acp_sint32_t    mUlpSqltype;            /* SQL TYPE                      */
    acp_uint32_t    mUlpElemCount;          /* from client to server         */
    acp_uint32_t    mUlpReturnElemCount;    /* from server to client         */
    acp_uint32_t    mUlpHasNull;            /* out parameter has null        */
    acp_uint8_t     mUlpPadding[20];        /* padding & reserved            */
} psmArrayMetaInfo;

typedef struct ulpPSMArrDataInfo
{
    void         *mData;            /* Meta Data + Host Variable Data */
    ulpHostVar   *mUlpHostVar;      /* Host Variable Point address */
} ulpPSMArrDataInfo;

typedef struct ulpPSMArrInfo
{
    acp_bool_t         mIsPSMArray;             /* PSM�� array type ���ڸ� ������ �ִ���? */
    acp_list_t         mPSMArrDataInfoList;     /* ulpPSMArrDataInfo List */
} ulpPSMArrInfo;

/* statement�� ���� ���� ���� type */
typedef enum ulpStmtState
{
    S_INITIAL = 0,
    S_PREPARE,
    S_BINDING,
    S_SETSTMTATTR,
    S_EXECUTE,
    S_CLOSE
} ulpStmtState;


/* cursor�� ���� ���� ���� type */
typedef enum ulpCurState
{
    C_INITIAL = 0,
    C_DECLARE,
    C_OPEN,
    C_FETCH,
    C_CLOSE
} ulpCurState;


/*
 * statement/cursor hash table���� �����ǰ� �ִ� node.
 */
typedef struct ulpLibStmtNode ulpLibStmtNode;
struct ulpLibStmtNode
{
    SQLHSTMT      mHstmt;             /* statement handle */
    /* statement name ( PREPARE, DECLARE STATEMENT���� ����� �̸�) */
    acp_char_t    mStmtName[MAX_STMT_NAME_LEN];
    acp_char_t    mCursorName[MAX_CUR_NAME_LEN];  /* cursor�� ���� �̸����� ���´�. */
    /* BUG-30789: Memory usage per prepared statement is too BIG. (>300k) */
    acp_char_t   *mQueryStr;                      /* query���� ���� */
    acp_sint32_t  mQueryStrLen;

    ulpStmtState       mStmtState;    /* statement�� ���� ���������� ���´�.*/
    ulpCurState        mCurState;     /* cursor�� ���� ���������� ���´�.   */
    ulpStmtBindInfo    mBindInfo;     /* row/column-wise bind������ ������ ����ȴ�. */

    /* host ���� ������ ����Ű�� ������   */
    ulpHostVarPtr     *mInHostVarPtr;
    ulpHostVarPtr     *mOutHostVarPtr;
    ulpHostVarPtr     *mExtraHostVarPtr; /* INOUT, PSM OUT type�ϰ�� �����. */

    SQLUINTEGER  mRowsFetched;
     /* BUG-31405 : Failover������ Failure of finding statement ���� �߻�. */
    acp_bool_t   mNeedReprepare; /* failover������ reprepare�ǰ� �ϱ����� flag */

    ulpSqlca    *mSqlca;
    ulpSqlcode  *mSqlcode;
    ulpSqlstate *mSqlstate;

    ulpLibStmtNode *mNextStmt;           /* stmt hash table�� ���� list link   */
    ulpLibStmtNode *mNextCur;            /* cursor hash table�� ���� list�� link */

    /* BUG-31467 : APRE should consider the thread safety of statement */
    /* unnamed statement�� ���� thread-safty ��� */
    acp_thr_rwlock_t mLatch;

    /* BUG-45779 */
    ulpPSMArrInfo    mUlpPSMArrInfo;
};


typedef struct  ulpLibStmtBucket
{
    acp_thr_rwlock_t mLatch;
    ulpLibStmtNode  *mList;

} ulpLibStmtBUCKET;


typedef struct  ulpLibStmtHashTab
{
    acp_sint32_t  mSize    ;                /* Max number of elements in table */
    acp_sint32_t  mNumNode ;                /* number of elements currently in table */
    acp_uint32_t  (*mHash) (acp_uint8_t *);       /* hash function */
    acp_sint32_t  (*mCmp ) (
                    const acp_char_t*,
                    const acp_char_t*,
                    acp_size_t );       /* comparison funct */
    ulpLibStmtBUCKET mTable[MAX_NUM_STMT_HASH_TAB_ROW];  /* actual hash table    */

} ulpLibStmtHASHTAB;


typedef struct  ulpLibStmtList
{
    acp_sint32_t        mSize;
    acp_sint32_t        mNumNode;
    acp_thr_rwlock_t    mLatch;
    ulpLibStmtNode     *mList;

} ulpLibStmtLIST;


/*
 * statement/cursor hash table managing functions.
 */

/* ���ο� statement node�� �����. */
ulpLibStmtNode* ulpLibStNewNode(ulpSqlstmt* aSqlstmt, acp_char_t *aStmtName );

/* stmt hash table���� �ش� stmt�̸����� stmt node�� ã�´�. */
ulpLibStmtNode* ulpLibStLookupStmt( ulpLibStmtHASHTAB *aStmtHashT,
                                    acp_char_t *aStmtName );

/* stmt hash table�� �ش� stmt�̸����� stmt node�� �߰��Ѵ�. List�� ���� �ڿ� �Ŵܴ�. */
ulpLibStmtNode* ulpLibStAddStmt( ulpLibStmtHASHTAB *aStmtHashT,
                                 ulpLibStmtNode *aStmtNode );

/* stmt hash table�� ��� statement�� �����Ѵ�.                                   *
 * Prepare�� declare statement�� ���� ������ stmt node���� disconnect�ɶ� �����ȴ�. */
/* BUG-26370 [valgrind error] :  cursor hash table�� �����ؾ��Ѵ�. */
void ulpLibStDelAllStmtCur( ulpLibStmtHASHTAB *aStmtTab,
                            ulpLibStmtHASHTAB *aCurTab );

/* cursor hash table���� �ش� cursor�̸����� cursor node�� ã�´�. */
ulpLibStmtNode* ulpLibStLookupCur( ulpLibStmtHASHTAB *aCurHashT, acp_char_t *aCurName );

/* cursor hash table�� �ش� cursor�̸��� ���� cursor node�� �����Ѵ�.                *
 * Statement �̸��� ������� link�� �����ϸ�, ���� ��쿡�� utpStmtNode ��ü�� �����Ѵ�. */
ACI_RC ulpLibStDeleteCur( ulpLibStmtHASHTAB *aCurHashT, acp_char_t *aCurName );

/* cursor hash table�� bucket list �������� �ش� stmt node�� ���� ��link�� �����Ѵ�. */
ACI_RC ulpLibStAddCurLink( ulpLibStmtHASHTAB *aCurHashT,
                           ulpLibStmtNode *aStmtNode,
                           acp_char_t *aCurName );

/* unnamed stmt list���� �ش� query�� stmt node�� ã�´�. */
ulpLibStmtNode *ulpLibStLookupUnnamedStmt( ulpLibStmtLIST *aStmtList,
                                           acp_char_t *aQuery );

/* unnamed stmt list�� �� stmt node�� �߰��Ѵ�. */
ACI_RC ulpLibStAddUnnamedStmt( ulpLibStmtLIST *aStmtList,
                               ulpLibStmtNode *aStmtNode );

/* unnamed stmt list�� �Ŵ޸� ��� stmt node���� �����Ѵ�. */
void ulpLibStDelAllUnnamedStmt( ulpLibStmtLIST *aStmtList );


#endif
