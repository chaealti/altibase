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
 
/***********************************************************************
 * $Id: iloSQLApi.h 90308 2021-03-24 08:32:25Z donlet $
 **********************************************************************/

#ifndef _O_ILO_SQLAPI_H_
#define _O_ILO_SQLAPI_H_ 1

#include <ute.h>
#include <iloApi.h>

#define ILO_DEFAULT_QUERY_LENGTH 4096

class iloColumns
{
public:
    iloColumns();

    ~iloColumns();

    SInt GetSize()                     { return m_nCol; }

    void SetColumnCount(SInt nColCount) { m_nCol = nColCount; }

    SInt SetSize(SInt nColCount);
    SInt Resize(SInt nColCount);
    void AllFree();

    SChar *GetName(SInt nCol)
    { return (nCol >= m_nCol) ? NULL : m_Name[nCol]; }

    /* BUG-17563 : iloader ���� ū����ǥ �̿��� Naming Rule ���� ����  */
    SChar *GetTransName(SInt nCol, SChar *aName, UInt aLen);

    SInt SetName(SInt nCol, SChar *szName);

    SQLSMALLINT GetType(SInt nCol)
    { return (nCol >= m_nCol) ? (SQLSMALLINT)0 : m_Type[nCol]; }

    SInt SetType(SInt nCol, SQLSMALLINT nType);

    UInt GetPrecision(SInt nCol)
    { return (nCol >= m_nCol) ? 0 : m_Precision[nCol]; }

    SInt SetPrecision(SInt nCol, UInt nPrecision);

    SShort GetScale(SInt nCol)
    { return (nCol >= m_nCol) ? 0 : m_Scale[nCol]; }

    SInt SetScale(SInt nCol, SShort nScale);

    IDE_RC MAllocValue(SInt aColIdx, UInt aSize);
    

public:
    SChar                (*m_Name)[256];
    UInt                 *m_Precision;
    SShort               *m_Scale;
    /* * ���� �� ���� ������ iLoader�� out ��� �� ��� * */
    /* SQLBindCol()�� ���� �÷� ���� ����� ����.
     * ���� �������̳� �����δ� �� ������ ũ�Ⱑ �ٸ� 1���� �迭�̴�. */
    void                **m_Value;
    /* m_Value�� �� ����(void *)�� ����Ű�� ������ ũ�� */
    UInt                 *mBufLen;
    /* m_Value�� �� ����(void *)�� ����Ű�� ���ۿ� ����� ���� ���� */
    SQLLEN               **m_Len;
    SInt                 *m_DisplayPos; /* �μ��� ��ġ, �μⰡ �Ϸ�Ǹ� -1 */
    
    //PROJ-1714 
    //Array Fetch
    SQLUINTEGER           m_RowsFetched;        //Fetch�� Row Count
    SQLUSMALLINT         *m_RowStatusArray;     //Fetch�� Row�� Status
    SInt                  m_ArrayCount;
    /* BUG-30467 */
    SChar                 (*m_ArrayPartName)[MAX_OBJNAME_LEN];
    SInt                  m_PartitionCount;

private:
    SInt                  m_nCol;
    SQLSMALLINT          *m_Type;
};

class iloSQLApi
{
public:
    /* Formout �ÿ� ����, Data-Download�ÿ��� Column ������ �����ϴµ� ����� */
    iloColumns m_Column;        

private:
    SQLHENV      m_IEnv;
    SQLHDBC      m_ICon;
    SQLHSTMT     m_IStmt;

    /* mErrorMgr �迭�� ���� �� */
    SInt         mNErrorMgr;
    /* Caution:
     * Right 3 nibbles of mErrorMgr[].mErrorCode must not be read,
     * because they can have dummy values. */
    uteErrorMgr *mErrorMgr;
    /* Fetch() �Լ��� SQL_FALSE ���� ��,
     * SQL_NO_DATA�� ���� ������ �߻��� ��츦 �����ϱ� ���� ����.
     * ���� ������ �߻��� ��� �� ������ ID_TRUE�� �����ȴ�. */
    iloBool       mIsFetchError;
    SChar       *m_SQLStatement;
    SInt         mSqlLength;
    SInt         mSqlBufferLength;

    SQLHSTMT      m_IStmtRetry;
    iloBool       m_UseStmtRetry;
    UInt          m_ParamSetSize;
    SQLUSMALLINT *m_ParamStatusPtr;

    iloBool       mShardEnable;  /* BUG-47891 */

public:
    iloSQLApi();
   
    SInt InitOption( ALTIBASE_ILOADER_HANDLE aHandle );
    //PROJ-1714
    //BUG-22429 CopyConsture�� �Լ��� ó��..
    iloSQLApi& CopyConstructure(const iloSQLApi& o)
    {
        mNErrorMgr = o.mNErrorMgr;
        mErrorMgr = o.mErrorMgr;
        mIsFetchError = o.mIsFetchError;

        return *this;
    }
    
    //PROJ-1760
    // INSERT SQL Statement�� �����Ѵ�.

    void CopySQLStatement(const iloSQLApi& o)
    {
        mSqlBufferLength = o.mSqlBufferLength;  
        mSqlLength = o.mSqlLength;
        
        /* PROJ-1714
         * m_SQLStatement�� �ʱ�ȭ �Ҷ��� ILO_DEFAULT_QUERY_LENGTH�� ������.. AppendQueryString()����
         * Size�� Ȯ���ϰ� �ȴ�. ���� �Ʒ��� ���� ó���Ѵ�.
         */
        if(m_SQLStatement != NULL)
        {
            idlOS::free(m_SQLStatement);
        }
        m_SQLStatement = (SChar*) idlOS::malloc(mSqlBufferLength);
        strncpy(m_SQLStatement,o.m_SQLStatement, mSqlLength); 
        m_SQLStatement[mSqlLength] = '\0';
    }

    virtual ~iloSQLApi();

    void SetSQLStatement(SChar *szStr);

    SQLHSTMT getStmt();

    void SetErrorMgr(SInt aNErrorMgr, uteErrorMgr *aErrorMgr)
    { mNErrorMgr = aNErrorMgr; mErrorMgr = aErrorMgr; }

    IDE_RC Open(SChar *aHost, SChar *aDB, SChar *aUser, SChar *aPasswd,
                SChar *aNLS, SInt aPortNo, SInt aConnType, 
                iloBool aPreferIPv6, /* BUG-29915 */
                SChar  *aSslCa, /* BUG-41406 SSL */
                SChar  *aSslCapath,
                SChar  *aSslCert,
                SChar  *aSslKey,
                SChar  *aSslVerify,
                SChar  *aSslCipher);

    // BUG-27284: user�� �����ϴ��� Ȯ��
    SInt CheckUserExist(SChar *aUserName);

    SInt CheckIsQueue( ALTIBASE_ILOADER_HANDLE  aHandle,
                       SChar                   *aTableName, 
                       SChar                   *aUserName,
                       iloBool                  *aIsQueue);

    SInt Columns( ALTIBASE_ILOADER_HANDLE  aHandle, 
                  SChar                   *aTableName,
                  SChar                   *aTableOwner);

    /* BUG-30467 */
    SInt PartitionInfo( ALTIBASE_ILOADER_HANDLE aHandle,
                        SChar  *aTableName,
                        SChar  *aTableOwner);

    SInt ExecuteDirect();

    SInt Execute();

    SInt Prepare();

    SInt FreeStmtParams();

    SInt SelectExecute(iloTableInfo  *aTableInfo);
    
    SInt DescribeColumns(iloColumns *aColumns);      //PROJ-1714 
    
    SInt BindColumns( ALTIBASE_ILOADER_HANDLE aHandle, iloColumns *aColumns);         //PROJ-1714

    // BUG-24358 iloader geometry support - add tableinfo
    SInt BindColumns( ALTIBASE_ILOADER_HANDLE  aHandle,
                      iloColumns              *aColumns,
                      iloTableInfo            *aTableInfo);

    SInt Fetch(iloColumns *aColumns);   

    IDE_RC AutoCommit(iloBool aIsCommitOn);

    IDE_RC EndTran(iloBool aIsCommit);

    SInt setQueryTimeOut( SInt aTime );

    IDE_RC alterReplication( SInt aBool );

    SInt StmtClose();

    IDE_RC StmtInit();

    void Close();

    void clearQueryString();

    void replaceTerminalChar(SChar aChar);
    
    SQLRETURN appendQueryString(SChar *aString);

    SChar *getSqlStatement();
    
    /* Fetch() �Լ��� SQL_FALSE ���� �� ȣ�� �����ϸ�,
     * ���� ���� �߻� ���θ� �����Ѵ�. */
    iloBool IsFetchError() { return mIsFetchError; }

    IDE_RC SetColwiseParamBind( ALTIBASE_ILOADER_HANDLE  aHandle,
                                UInt                     aParamSetSize,
                                SQLUSMALLINT            *aParamStatusPtr);

    IDE_RC SetColwiseParamBind_StmtRetry(void);

    IDE_RC BindParameter(UShort aParamNo, SQLSMALLINT aInOutType,
                         SQLSMALLINT aValType, SQLSMALLINT aParamType,
                         UInt aColSize, SShort aDecimalDigits,
                         void *aParamValPtr, SInt aBufLen,
                         SQLLEN *aStrLen_or_IndPtr);

    IDE_RC GetLOBLength(SQLUBIGINT aLOBLoc, SQLSMALLINT aLOBLocCType,
                        UInt *aLOBLen);

    IDE_RC GetLOB(SQLSMALLINT aLocatorCType, SQLUBIGINT aSourceLocator,
                  UInt aFromPosition, UInt aForLength,
                  SQLSMALLINT aTargetCType, void *aValue, UInt aValueMax,
                  UInt *aValueLength);

    IDE_RC PutLOB(SQLSMALLINT aLocatorCType, SQLUBIGINT aTargetLocator,
                  UInt aFromPosition, UInt aForLength,
                  SQLSMALLINT aSourceCType, void *aValue, UInt aValueLength);

    IDE_RC FreeLOB(SQLUBIGINT aLocator);

    IDE_RC StrToUpper(SChar *aStr);

    IDE_RC NumParams(SQLSMALLINT  *aParamCntPtr);
    IDE_RC SetPrefetchRows( SInt aPrefetchRows ); /* BUG-43277 */
    IDE_RC SetAsyncPrefetch( asyncPrefetchType aType ); /* BUG-44187 */
    IDE_RC SetGlobalTransactionLevel( SInt aTxLevel ); /* TASK-7307 */
    IDE_RC GetShardEnable();  /* BUG-47891 */

private:
    IDE_RC SetErrorMsgAfterAllocEnv();

    IDE_RC SetErrorMsgWithHandle(SQLSMALLINT aHandleType, SQLHANDLE aHandle);

    /* for ALTIBASE_DATE_FORMAT */
    IDE_RC SetAltiDateFmt();
};

#endif /* _O_ILO_SQLAPI_H_ */
