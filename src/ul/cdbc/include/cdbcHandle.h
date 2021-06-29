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

#ifndef CDBC_HANDLE_H
#define CDBC_HANDLE_H

ACP_EXTERN_C_BEGIN


typedef struct cdbcABHandle         cdbcABHandle;
typedef struct cdbcABStmt           cdbcABStmt;
typedef struct cdbcABRowList        cdbcABRowList;
typedef struct cdbcABRes            cdbcABRes;
typedef struct cdbcABConn           cdbcABConn;

typedef SQLUBIGINT ALTIBASE_LOBLOCATOR;

typedef acp_sint32_t ALTIBASE_STATE;

#define ALTIBASE_STATE_INIT             0x00 /* init() ~ close() */
#define ALTIBASE_STATE_CONNECTED        0x40 /* connect() ~ close() */
#define ALTIBASE_STATE_PREPARED         0x01 /* stmt_prepare() ~ stmt_close() */
#define ALTIBASE_STATE_EXECUTED         0x02 /* stmt_execute() ~ stmt_close(), stmt_free_result(), stmt_prepare() */
#define ALTIBASE_STATE_FETCHED          0x04 /* stmt_fetch() ~ stmt_close(), stmt_free_result() */
#define ALTIBASE_STATE_RES_RETURNED     0x10 /* use_result() ~ free_result() */
#define ALTIBASE_STATE_RES_STORED       0x20 /* store_result() ~ free_result() */

#define STATE_SET(v,f)                  ( (v) = ((v) | (f)) )
#define STATE_UNSET(v,f)                ( (v) = ((v) & ~(f)) )
#define STATE_IS_SET(v,f)               ( ((v) & (f)) == (f) )

#define STATE_SET_CONNECTED(s)          STATE_SET(s, ALTIBASE_STATE_CONNECTED)
#define STATE_SET_PREPARED(s)           STATE_SET(s, ALTIBASE_STATE_PREPARED)
#define STATE_SET_EXECUTED(s)           STATE_SET(s, ALTIBASE_STATE_EXECUTED)
#define STATE_SET_FETCHED(s)            STATE_SET(s, ALTIBASE_STATE_FETCHED)
#define STATE_SET_RESRETURNED(s)        STATE_SET(s, ALTIBASE_STATE_RES_RETURNED)
#define STATE_SET_STORED(s)             STATE_SET(s, ALTIBASE_STATE_RES_STORED)
#define STATE_UNSET_CONNECTED(s)        STATE_UNSET(s, ALTIBASE_STATE_CONNECTED)
#define STATE_UNSET_PREPARED(s)         STATE_UNSET(s, ALTIBASE_STATE_PREPARED)
#define STATE_UNSET_EXECUTED(s)         STATE_UNSET(s, ALTIBASE_STATE_EXECUTED)
#define STATE_UNSET_FETCHED(s)          STATE_UNSET(s, ALTIBASE_STATE_FETCHED)
#define STATE_UNSET_RESRETURNED(s)      STATE_UNSET(s, ALTIBASE_STATE_RES_RETURNED)
#define STATE_UNSET_STORED(s)           STATE_UNSET(s, ALTIBASE_STATE_RES_STORED)
#define STATE_IS_CONNECTED(s)           STATE_IS_SET(s, ALTIBASE_STATE_CONNECTED)
#define STATE_IS_PREPARED(s)            STATE_IS_SET(s, ALTIBASE_STATE_PREPARED)
#define STATE_IS_EXECUTED(s)            STATE_IS_SET(s, ALTIBASE_STATE_EXECUTED)
#define STATE_IS_FETCHED(s)             STATE_IS_SET(s, ALTIBASE_STATE_FETCHED)
#define STATE_IS_RESRETURNED(s)         STATE_IS_SET(s, ALTIBASE_STATE_RES_RETURNED)
#define STATE_IS_STORED(s)              STATE_IS_SET(s, ALTIBASE_STATE_RES_STORED)

#define CONN_SET_CONNECTED(conn)        STATE_SET_CONNECTED((conn)->mState)
#define CONN_SET_PREPARED(conn)         STATE_SET_PREPARED((conn)->mState)
#define CONN_SET_EXECUTED(conn)         STATE_SET_EXECUTED((conn)->mState)
#define CONN_SET_FETCHED(conn)          STATE_SET_FETCHED((conn)->mState)
#define CONN_SET_RESRETURNED(stmt)      STATE_SET_RESRETURNED((conn)->mState)
#define CONN_UNSET_CONNECTED(conn)      STATE_UNSET_CONNECTED((conn)->mState)
#define CONN_UNSET_PREPARED(conn)       STATE_UNSET_PREPARED((conn)->mState)
#define CONN_UNSET_EXECUTED(conn)       STATE_UNSET_EXECUTED((conn)->mState)
#define CONN_UNSET_FETCHED(conn)        STATE_UNSET_FETCHED((conn)->mState)
#define CONN_UNSET_RESRETURNED(stmt)    STATE_UNSET_RESRETURNED((conn)->mState)
#define CONN_IS_CONNECTED(conn)         STATE_IS_CONNECTED((conn)->mState)
#define CONN_IS_PREPARED(conn)          STATE_IS_PREPARED((conn)->mState)
#define CONN_IS_EXECUTED(conn)          STATE_IS_EXECUTED((conn)->mState)
#define CONN_IS_FETCHED(conn)           STATE_IS_FETCHED((conn)->mState)
#define CONN_IS_RESRETURNED(conn)       STATE_IS_RESRETURNED((conn)->mState)
#define CONN_NOT_CONNECTED(conn)        (! CONN_IS_CONNECTED(conn))
#define CONN_NOT_PREPARED(conn)         (! CONN_IS_PREPARED(conn))
#define CONN_NOT_EXECUTED(conn)         (! CONN_IS_EXECUTED(conn))
#define CONN_NOT_FETCHED(conn)          (! CONN_IS_FETCHED(conn))

#define STMT_SET_PREPARED(stmt)         STATE_SET_PREPARED((stmt)->mState)
#define STMT_SET_EXECUTED(stmt)         STATE_SET_EXECUTED((stmt)->mState)
#define STMT_SET_FETCHED(stmt)          STATE_SET_FETCHED((stmt)->mState)
#define STMT_SET_RESRETURNED(stmt)      STATE_SET_RESRETURNED((stmt)->mState)
#define STMT_UNSET_PREPARED(stmt)       STATE_UNSET_PREPARED((stmt)->mState)
#define STMT_UNSET_EXECUTED(stmt)       STATE_UNSET_EXECUTED((stmt)->mState)
#define STMT_UNSET_FETCHED(stmt)        STATE_UNSET_FETCHED((stmt)->mState)
#define STMT_UNSET_RESRETURNED(stmt)    STATE_UNSET_RESRETURNED((stmt)->mState)
#define STMT_IS_PREPARED(stmt)          STATE_IS_PREPARED((stmt)->mState)
#define STMT_IS_EXECUTED(stmt)          STATE_IS_EXECUTED((stmt)->mState)
#define STMT_IS_FETCHED(stmt)           STATE_IS_FETCHED((stmt)->mState)
#define STMT_IS_RESRETURNED(stmt)       STATE_IS_RESRETURNED((stmt)->mState)
#define STMT_IS_STORED(stmt)            STATE_IS_STORED((stmt)->mState)
#define STMT_NOT_PREPARED(stmt)         (! STMT_IS_PREPARED(stmt))
#define STMT_NOT_EXECUTED(stmt)         (! STMT_IS_EXECUTED(stmt))
#define STMT_NOT_FETCHED(stmt)          (! STMT_IS_FETCHED(stmt))
#define STMT_NOT_STORED(stmt)           (! STMT_IS_STORED(stmt))

#define RES_SET_PREPARED(res)           STATE_SET_PREPARED(*((res)->mState))
#define RES_SET_EXECUTED(res)           STATE_SET_EXECUTED(*((res)->mState))
#define RES_SET_FETCHED(res)            STATE_SET_FETCHED(*((res)->mState))
#define RES_SET_RESRETURNED(res)        STATE_SET_RESRETURNED(*((res)->mState))
#define RES_SET_STORED(res)             STATE_SET_STORED(*((res)->mState))
#define RES_UNSET_PREPARED(res)         STATE_UNSET_PREPARED(*((res)->mState))
#define RES_UNSET_EXECUTED(res)         STATE_UNSET_EXECUTED(*((res)->mState))
#define RES_UNSET_FETCHED(res)          STATE_UNSET_FETCHED(*((res)->mState))
#define RES_UNSET_RESRETURNED(res)      STATE_UNSET_RESRETURNED(*((res)->mState))
#define RES_UNSET_STORED(res)           STATE_UNSET_STORED(*((res)->mState))
#define RES_IS_PREPARED(res)            ( ((res)->mState != NULL) && STATE_IS_PREPARED(*((res)->mState)) )
#define RES_IS_EXECUTED(res)            ( ((res)->mState != NULL) && STATE_IS_EXECUTED(*((res)->mState)) )
#define RES_IS_FETCHED(res)             ( ((res)->mState != NULL) && STATE_IS_FETCHED(*((res)->mState)) )
#define RES_IS_RESRETURNED(res)         ( ((res)->mState != NULL) && STATE_IS_RESRETURNED(*((res)->mState)) )
#define RES_IS_STORED(res)              ( ((res)->mState != NULL) && STATE_IS_STORED(*((res)->mState)) )
#define RES_NOT_PREPARED(res)           (! RES_IS_PREPARED(res))
#define RES_NOT_EXECUTED(res)           (! RES_IS_EXECUTED(res))
#define RES_NOT_FETCHED(res)            (! RES_IS_FETCHED(res))
#define RES_NOT_STORED(res)             (! RES_IS_STORED(res))



/* cdbcABDiagRec��, aci_client_error_mgr_t�� �޸�,
 * mErrorCode�� ������ real error code��.
 * �׷��Ƿ�, mErrorCode ���� ����� �� ACI_E_ERROR_CORE()�� ���� �ʾƾ� �Ѵ�.
 */
typedef aci_client_error_mgr_t cdbcABDiagRec;

/* BUG-30319 */
typedef enum cdbcABHandleType
{
    ALTIBASE_HANDLE_NONE        = 0,
    ALTIBASE_HANDLE_CONN        = 10,
    ALTIBASE_HANDLE_STMT        = 20,
    ALTIBASE_HANDLE_RES         = 30,
    ALTIBASE_HANDLE_RES_META    = 31
} cdbcABHandleType;

struct cdbcABHandle
{
    acp_sint32_t        mStSize;
    cdbcABHandleType    mType;
    cdbcABHandle       *mParentHandle;
};

CDBC_INLINE void altibase_invalidate_handle( cdbcABHandle *aHandle )
{
    aHandle->mStSize       = 0;
    aHandle->mType         = ALTIBASE_HANDLE_NONE;
    aHandle->mParentHandle = NULL;
}

/* ALTIBASE */
struct cdbcABConn
{
    cdbcABHandle                     mBaseHandle;
    cdbcABDiagRec                    mDiagRec;
    SQLHENV                          mHenv;
    SQLHDBC                          mHdbc;
    SQLHSTMT                         mHstmt;
    ALTIBASE_STATE                   mState;

    cdbcABQueryType                  mQueryType;

    acp_char_t                       mNlsLang[ALTIBASE_MAX_NLS_USE_LEN + 1];
    SQLINTEGER                       mNlsLangLen;
    acp_char_t                       mServerVerStr[ALTIBASE_MAX_VERSTR_LEN + 1];
    acp_sint16_t                     mServerVerStrLen;
    acp_sint32_t                     mServerVer;
    acp_char_t                       mProtoVerStr[ALTIBASE_MAX_VERSTR_LEN + 1];
    acp_sint16_t                     mProtoVerStrLen;
    acp_sint32_t                     mProtoVer;
    acp_char_t                       mHostInfo[ALTIBASE_MAX_HOSTINFO_LEN + 1];

    altibase_failover_callback_func  mFOCallbackFunc;
    void                            *mFOAppContext;
    SQLFailOverCallbackContext       mFOCallbackContext;
};

/* ALTIBASE_STMT */
struct cdbcABStmt
{
    cdbcABHandle         mBaseHandle;
    cdbcABDiagRec        mDiagRec;
    SQLHSTMT             mHstmt;
    ALTIBASE_STATE       mState;

    acp_char_t          *mQstr;
    acp_sint32_t         mQstrMaxLen;
    cdbcABQueryType      mQueryType;

    SQLSMALLINT          mParamCount;
    ALTIBASE_BIND       *mBindParam;        /**< ����ڰ� ������ param bind ���� */
    ALTIBASE_BIND       *mBindResult;       /**< ����ڰ� ������ result bind ���� */

    cdbcBufferMng        mBindBuffer;
    ALTIBASE_BIND       *mBakBindParam;
    ALTIBASE_LONG       *mBakBindParamLength;
    ALTIBASE_BOOL       *mBakBindParamIsNull;
    ALTIBASE_LONG       *mRealBindParamLength;

    acp_sint32_t         mArrayBindSize;    /**< ����ڰ� ������ array bind size */
    acp_sint32_t         mArrayFetchSize;   /**< ����ڰ� ������ array fetch size */

    cdbcABRes           *mRes;
};

struct cdbcABRowList
{
    ALTIBASE_ROW     mRow;          /**< �� ���� ����Ÿ */
    ALTIBASE_LONG   *mLengths;      /**< �� �÷� ����Ÿ�� ���� */
    ALTIBASE_LONG    mStatus;       /**< array status */
    cdbcABRowList   *mNext;         /**< ���� ���� ����Ű�� ������ */
};

/* ALTIBASE_RES */
struct cdbcABRes
{
    cdbcABHandle     mBaseHandle;
    cdbcABDiagRec   *mDiagRec;              /**< ���� ������ ���� ������ */
    SQLHSTMT         mHstmt;
    ALTIBASE_STATE  *mState;                /**< ���°� */

    SQLSMALLINT      mFieldCount;           /**< ������� �ʵ� ���� */
    ALTIBASE_FIELD  *mFieldInfos;           /**< ������� �ʵ� ���� */
    acp_bool_t       mFieldInfoEx;

    acp_sint32_t     mArrayBindSize;        /**< array binding ����. */
    acp_uint16_t    *mArrayStatusParam;     /**< array binding ����. */
    SQLLEN           mArrayProcessed;       /**< array binding ����. */
    acp_sint32_t     mArrayFetchSize;       /**< array fetch   ����. */
    acp_uint16_t    *mArrayStatusResult;    /**< array fetch   ����. */
    SQLLEN           mArrayFetched;         /**< array fetch   ����. */

    ALTIBASE_BIND   *mBindResult;           /**< fetch�� �� ���ε� ���� */

    ALTIBASE_LONG    mRowCount;
    cdbcABRowList  **mRowMap;               /**< store result ����, row �ٷ� ����� �� */
    acp_bool_t       mRowMapAllocFailed;    /**< mRowMap ������ �����ߴ��� ����. mRowMap ������ �ѹ��� �õ��ϱ� ���ؼ� ���. */
    cdbcABRowList   *mRowCursor;            /**< store result ���� */
    cdbcABRowList   *mFetchedRowCursor;     /**< store result ����. altibase_stmt_fetch_column()���� fetch�� Ŀ���� �����ϱ� ���ؼ� ��� */
    ALTIBASE_LONG   *mFetchedColOffset;     /**< store result ���� */
    acp_sint32_t     mFetchedColOffsetMaxCount; /**< store result ���� */
    cdbcABRowList   *mRowList;              /**< store result ����, ��ü ����Ÿ */
    ALTIBASE_ROW     mFetchedRow;           /**< use   result ����, fetch�� ����Ÿ */
    ALTIBASE_LONG   *mLengths;              /**< *_fetch_lengths()���� ��ȯ�� ���� */

    cdbcBufferMng    mDatBuffer;
    cdbcBufferMng    mBindBuffer;
};



CDBC_INTERNAL ALTIBASE_RC cdbcCheckHandle (cdbcABHandleType aType, void *aABHandle);

#define HCONN_IS_VALID(h)   (cdbcCheckHandle(ALTIBASE_HANDLE_CONN, (h)) == ALTIBASE_SUCCESS)
#define HCONN_NOT_VALID(h)  (! HCONN_IS_VALID(h))
#define HSTMT_IS_VALID(h)   (cdbcCheckHandle(ALTIBASE_HANDLE_STMT, (h)) == ALTIBASE_SUCCESS)
#define HSTMT_NOT_VALID(h)  (! HSTMT_IS_VALID(h))
#define HRES_IS_VALID(h)    (cdbcCheckHandle(ALTIBASE_HANDLE_RES , (h)) == ALTIBASE_SUCCESS)
#define HRES_NOT_VALID(h)   (! HRES_IS_VALID(h))
#define HROM_IS_VALID(h)    (cdbcCheckHandle(ALTIBASE_HANDLE_RES_META , (h)) == ALTIBASE_SUCCESS)
#define HROM_NOT_VALID(h)   (! HROM_IS_VALID(h))

CDBC_INTERNAL cdbcABRes * altibase_result_init (cdbcABConn *aABConn);
CDBC_INTERNAL cdbcABRes * altibase_stmt_result_init (cdbcABStmt *aABStmt, cdbcABHandleType aType);
CDBC_INTERNAL SQLHSTMT altibase_hstmt_init (cdbcABConn *aABConn);
CDBC_INTERNAL ALTIBASE_RC altibase_ensure_hstmt (cdbcABConn *aABConn);



ACP_EXTERN_C_END

#endif /* CDBC_HANDLE_H */

