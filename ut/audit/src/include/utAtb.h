/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/*******************************************************************************
 * $Id: utAtb.h 82705 2018-04-05 01:31:36Z bethy $
 ******************************************************************************/
#ifndef _UT_ALTIBASE_H_
#define _UT_ALTIBASE_H_ 1

#include <utdb.h>

#define  SERVER_NOT_CONNECTED 0
#define  SERVER_NORMAL        1

#define TRN_AUTOCOMMIT    1
#define TRN_DEFAULT      -1
#define TRM_NOAUTOCOMMIT  0

#define COMMIT_ON_SUCCESS  0
#define ATB_ERROR  -999

#define DATE_FMT "YYYY-MM-DD HH24:MI:SS.FF"
#define TIME_OUT ((UInt)0)

class utAtbConnection;
class utAtbQuery;
class utAtbRow;
class utAtbField;
class utAtbLob; // BUG-45909

IDL_EXTERN_C_BEGIN
    SInt sqlTypeToAltibase(SInt type);
IDL_EXTERN_C_END

class utAtbConnection : public Connection
{
public:
    utAtbConnection(dbDriver *aDbd);
   ~utAtbConnection(void);

    IDE_RC  connect( const SChar * ,...);
    IDE_RC  connect();

    IDE_RC disconnect(void);

    IDE_RC autocommit(bool = true); /* on by default in init() */

    IDE_RC commit  (void);
    IDE_RC rollback(void);

    Query *query(void);

    bool isConnected( );
    metaColumns * getMetaColumns(SChar*,SChar*);


    SChar   *  error(void * = NULL);
    SInt    getErrNo(void);
    dba_t   getDbType(void);
    SQLHDBC   getDbchp() { return dbchp; };

    IDE_RC     initialize(SChar* = NULL,UInt = 0);
    IDE_RC     finalize  ();

    SInt  checkState(  SInt, SQLHSTMT = NULL );
    IDE_RC AppendConnStrAttr( SChar *aConnStr, UInt aConnStrSize, SChar *aAttrKey, SChar *aAttrVal );

protected: friend class utAtbQuery;
    friend class utAtbRow  ;
    friend class utAtbField;

    SChar              mDSN[512];    // server Driver Connection DSN

    SQLHENV           envhp;         /* Environment handle     */
    SQLHDBC           dbchp;         /* DataBase connection    */

    SQLHSTMT     error_stmt;

    SInt          serverStatus;
};


class utAtbQuery : Query
{
public:
    utAtbQuery( utAtbConnection *);
    IDE_RC close   (void);
    IDE_RC clear   (void);
    IDE_RC reset   (void);
    IDE_RC prepare (void);

    Row   *fetch   (dba_t, bool);

    Row   *fetch   (dba_t);
    UInt   rows    (void);

    IDE_RC execute (bool=true         );
    IDE_RC execute (const SChar*, ... );    // direct execution
    IDE_RC lobAtToAt (Query *, Query *, SChar *, SChar *);

    IDE_RC bind(const UInt   , void *, UInt, SInt  // SQL Type aka SQL_VARCHAR etc.
                , bool isNull // for Indicator
                ,SQLSMALLINT  // SQL_PARAM_INPUT/OUTUT etc.
                ,SQLSMALLINT  // C Type parametr
                );

    /* BUG-32569 The string with null character should be processed in Audit */
    IDE_RC bind(const UInt   , void *, UInt, SInt  // SQL Type aka SQL_VARCHAR etc.
                , bool isNull // for Indicator
                ,SQLSMALLINT  // SQL_PARAM_INPUT/OUTUT etc.
                ,SQLSMALLINT  // C Type parametr
                ,UInt);

    IDE_RC bind(const UInt i, void *buf, UInt bufLen, SInt type, bool isNull);

    /* BUG-32569 The string with null character should be processed in Audit */
    IDE_RC bind(const UInt i, void *buf, UInt bufLen, SInt type, bool isNull, UInt aValueLength);

    inline
    IDE_RC bind(const SChar *, void *, UInt, SInt, bool) { return IDE_FAILURE; }

    IDE_RC initialize(UInt = COMMIT_ON_SUCCESS);
    IDE_RC finalize  ();

    SChar * error(void) { return _conn->error(_stmt); }

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    IDE_RC  utaCloseCur(void);

    /* BUG-45909 Improve LOB Processing */
    IDE_RC      putLob(UShort, Field *);
    IDE_RC      close4DML();
    void        printError();
    SQLUBIGINT  getBindLobLoc(UShort aPos);

protected:  friend class utAtbConnection;
    friend class utAtbRow       ;
    friend class utAtbField     ;
    friend class utAtbLob       ;

    SQLLEN     mLobInd;
    
    //** from utAtbcleConnect **//
    utAtbConnection *   _conn;

    SQLHSTMT            _stmt;
    
    SQLSMALLINT      _rescols;
    struct binds_t
    {
        SQLSMALLINT      pType;    // The TypeOf parametr could be IN OUT & IN/OUT
        SQLSMALLINT    sqlType;    // SQL data type

        SQLSMALLINT      scale;    // Scale of the parametrs
        SQLINTEGER  columnSize;    // Precizion of the parametrs

        SQLPOINTER       value;    // Pointer to buffer
        SQLLEN     valueLength;    // Parametr Length for the pointer


        /* BUG-45909 Improve LOB Processing */
        SQLUSMALLINT      mPos;
        SQLUBIGINT     mLobLoc;

        binds_t       * next;
    };
    binds_t    * _binds;

};

class  utAtbRow : Row
{
public:
    utAtbRow( utAtbQuery *, SInt &); // import from Query

    IDE_RC initialize();
    IDE_RC reset();

    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    IDE_RC setStmtAttr4Array(void);

protected:  friend class utAtbQuery;
    friend class utAtbField;
    utAtbQuery   *mQuery;
    SQLHSTMT      &_stmt;

};

class utAtbField : Field
{
public:

/// IDE_RC  getSChar(SChar *,UInt );
/// SInt    compare(Field * );
    IDE_RC  bindColumn(SInt,void* = NULL);

    inline bool isNull() { return ( *mValueInd == SQL_NULL_DATA); }
    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    inline SQLLEN *getValueInd() { return (mValueInd); }
    // file mode�� ��쿡�� file�� ���� �ʵ带 �о���� ������ indicator����
    // SQL_NULL_DATA�� set�Ҽ� �ִ� �Լ��� �ʿ�.
    inline void setIsNull(bool aIsNull) { *mValueInd = (aIsNull)?SQL_NULL_DATA:0; }

    /* BUG-45909 Improve LOB Processing */
    inline utAtbLob * getLob() { return mLob; }
    IDE_RC initLob();
    IDE_RC finiLob();
    bool   compareLob(Field *);

protected:  friend class utAtbRow;
// BUG-17604
#if defined(_MSC_VER) && defined(COMPILE_64BIT)
    SQLLEN      displaySize;
#else
    SQLINTEGER  displaySize;
#endif

    SQLULEN     precision;
    SQLSMALLINT scale;
    /* TASK-4212: audit���� ��뷮 ó���� ���� */
    // array fetch �� ���� indicator �迭.
    SQLLEN     *mValueInd;

    IDE_RC initialize( UShort aNo ,utAtbRow * aRow);
    IDE_RC finalize  (void);

    utAtbRow        *  mRow;
    utAtbLob        *  mLob; // BUG-45909

    inline SInt mapType (SInt aSqlType) { return sqlTypeToAltibase(aSqlType); }
};

/* BUG-45909 Improve LOB Processing */
class utAtbLob : Object
{
    SQLHDBC      mDbc;
    SQLHSTMT     mStmt;
    SQLSMALLINT  mSourceCType;
    SInt         mLocatorCType;
    SQLUINTEGER  mOffset;
    SQLUINTEGER  mTotalLen;
    SQLUINTEGER  mCurrLen;
    SQLUBIGINT   mLobLoc;
    SQLPOINTER   mBuf[BUF_LEN];

    utAtbQuery  *mQuery; 

public:
    IDE_RC initialize(utAtbQuery *aQuery,
                      SInt        aSqlType,
                      SQLUBIGINT  aLobLoc);
    IDE_RC finalize  (void);

    inline SQLUINTEGER  getLobLength() { return mTotalLen; }
    inline SQLUINTEGER  getCurrLen()   { return mCurrLen; }
    inline SQLPOINTER   getValue()     { return mBuf; }

    IDE_RC next(SQLUINTEGER *aLen);
    bool   equals(utAtbLob *aLob);

};

#endif
