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
#include <ulnConfigFile.h>

#include <sqlcli.h>

#ifndef SQL_API
#define SQL_API
#endif

/*
 * ======================
 * LOB �� ���õ� �Լ���.
 * ======================
 */

SQLRETURN SQL_API SQLBindFileToCol(SQLHSTMT     aHandle,
                                   SQLSMALLINT  aColumnNumber,
                                   SQLCHAR     *aFileNameArray,
                                   SQLLEN      *aFileNameLengthArray,
                                   SQLUINTEGER *aFileOptionsArray,
                                   SQLLEN       aMaxFileNameLength,
                                   SQLLEN      *aLenOrIndPtr)
{
    return ulnBindFileToCol((ulnStmt *)aHandle,
                            (acp_sint16_t  )aColumnNumber,
                            (acp_char_t **)aFileNameArray,
                            (ulvSLen *)aFileNameLengthArray,
                            (acp_uint32_t *  )aFileOptionsArray,
                            (ulvSLen  )aMaxFileNameLength,
                            (ulvSLen *)aLenOrIndPtr);
}

SQLRETURN SQL_API SQLBindFileToParam(SQLHSTMT     aHandle,
                                     SQLSMALLINT  aParameterNumber,
                                     SQLSMALLINT  aSqlType,
                                     SQLCHAR     *aFileNameArray,
                                     SQLLEN      *aFileNameLengthArray,
                                     SQLUINTEGER *aFileOptionsArray,
                                     SQLLEN       aMaxFileNameLength,
                                     SQLLEN      *aLenOrIndPtr)
{
    return ulnBindFileToParam((ulnStmt *)aHandle,
                              (acp_uint16_t  )aParameterNumber,
                              (acp_sint16_t  )aSqlType,
                              (acp_char_t **)aFileNameArray,
                              (ulvSLen *)aFileNameLengthArray,
                              (acp_uint32_t *  )aFileOptionsArray,
                              (ulvSLen  )aMaxFileNameLength,
                              (ulvSLen *)aLenOrIndPtr);
}

SQLRETURN SQL_API SQLGetLobLength(SQLHSTMT     aHandle,
                                  SQLUBIGINT   aLocator,
                                  SQLSMALLINT  aLocatorCType,
                                  SQLUINTEGER *aLobLengthPtr)
{
    acp_uint16_t sIsNull;
    // acp_uint64_t sLobLocator;

    // sLobLocator = ulnTypeConvertUBIGINTtoacp_uint64_t(aLocator);

    return ulnGetLobLength(SQL_HANDLE_STMT,
                           (ulnObject *)aHandle,
                           (acp_uint64_t   )aLocator,
                           (acp_sint16_t  )aLocatorCType,
                           (acp_uint32_t   *)aLobLengthPtr,
                           &sIsNull);
}

SQLRETURN SQL_API SQLGetLobLength2(SQLHSTMT      aHandle,
                                   SQLUBIGINT    aLocator,
                                   SQLSMALLINT   aLocatorCType,
                                   SQLUINTEGER  *aLobLengthPtr,
                                   SQLUSMALLINT *aIsNull)
{
    return ulnGetLobLength(SQL_HANDLE_STMT,
                           (ulnObject    *)aHandle,
                           (acp_uint64_t  )aLocator,
                           (acp_sint16_t  )aLocatorCType,
                           (acp_uint32_t *)aLobLengthPtr,
                           (acp_uint16_t *)aIsNull);
}

SQLRETURN SQL_API SQLPutLob(SQLHSTMT    aHandle,
                            SQLSMALLINT aLocatorCType,
                            SQLUBIGINT  aLocator,
                            SQLUINTEGER aStartOffset,
                            SQLUINTEGER aSizeToBeUpdated,
                            SQLSMALLINT aSourceCType,
                            SQLPOINTER  aDataToPut,
                            SQLUINTEGER aSizeDataToPut)
{
    // acp_uint64_t sLobLocator;

    // sLobLocator = ulnTypeConvertUBIGINTtoacp_uint64_t(aLocator);

    return ulnPutLob(SQL_HANDLE_STMT,
                     (ulnObject *)aHandle,
                     (acp_sint16_t)aLocatorCType,
                     (acp_uint64_t )aLocator,
                     (acp_uint32_t  )aStartOffset,
                     (acp_uint32_t  )aSizeToBeUpdated,
                     (acp_sint16_t)aSourceCType,
                     (void *)aDataToPut,
                     (acp_uint32_t  )aSizeDataToPut);
}

SQLRETURN SQL_API SQLGetLob(SQLHSTMT     aHandle,
                            SQLSMALLINT  aLocatorCType,
                            SQLUBIGINT   aLocator,
                            SQLUINTEGER  aStartOffset,
                            SQLUINTEGER  aSizeToGet,
                            SQLSMALLINT  aTargetCType,
                            SQLPOINTER   aBufferToStoreData,
                            SQLUINTEGER  aSizeBuffer,
                            SQLUINTEGER *aSizeReadPtr)
{
    // acp_uint64_t sLobLocator;

    // sLobLocator = ulnTypeConvertUBIGINTtoacp_uint64_t(aLocator);

    return ulnGetLob(SQL_HANDLE_STMT,
                     (ulnObject *)aHandle,
                     (acp_sint16_t)aLocatorCType,
                     (acp_uint64_t )aLocator,
                     (acp_uint32_t  )aStartOffset,
                     (acp_uint32_t  )aSizeToGet,
                     (acp_sint16_t)aTargetCType,
                     (void *)aBufferToStoreData,
                     (acp_uint32_t  )aSizeBuffer,
                     (acp_uint32_t *)aSizeReadPtr);
}

SQLRETURN SQL_API SQLFreeLob(SQLHSTMT aHandle, SQLUBIGINT aLocator)
{
    // acp_uint64_t sLobLocator;

    // sLobLocator = ulnTypeConvertUBIGINTtoacp_uint64_t(aLocator);

    return ulnFreeLob(SQL_HANDLE_STMT,
                      (ulnObject *)aHandle,
                      aLocator);
}

/* 
 * PROJ-2047 Strengthening LOB - Added Interfaces
 *
 * ������ Offset���� LOB �����͸� �����Ѵ�.
 */
SQLRETURN SQL_API SQLTrimLob(SQLHSTMT     aHandle,
                             SQLSMALLINT  aLocatorCType,
                             SQLUBIGINT   aLocator,
                             SQLUINTEGER  aStartOffset)
{
    return ulnTrimLob(SQL_HANDLE_STMT,
                      (ulnObject *)aHandle,
                      (acp_sint16_t)aLocatorCType,
                      (acp_uint64_t)aLocator,
                      (acp_uint32_t)aStartOffset);
}


/*
 * ===============================
 * ��ǥ��������, �ʿ��� �Լ���
 * ===============================
 */

SQLRETURN SQL_API SQLGetPlan(SQLHSTMT aStmt, SQLCHAR **aPlan)
{
    return ulnGetPlan((ulnStmt *)aStmt, (acp_char_t **)aPlan);
}

//PROJ-1645 UL-FailOver.
void  SQL_API   SQLDumpDataSources()
{
    ulnConfigFileDump();
}

/* BUG-28793 */
void  SQL_API   SQLDumpDataSourceFromName(SQLCHAR *aDataSourceName)
{
    ulnConfigFileDumpFromName((acp_char_t*)aDataSourceName);
}


/* 
 * PROJ-1721 Name-based Binding
 *
 * �Լ� �̸��� Beta�� ���� �� ���� CLI���� �Ʒ� �Լ��� �������� �߰���
 * ���ɼ��� �����߱� �����̸� SQLBindParameterByName�� ������ �д�.
 * ���� ADO.net ����̹������� ȣ��ȴ�.
 *
 * @ParameterName : NULL�� �ƴ� ��� '\0' ���ڷ� �ݵ�� ������ �Ѵ�.
 */
#if (ODBCVER >= 0x0300)
SQLRETURN SQL_API SQLBindParameterByNameBeta(SQLHSTMT     StatementHandle,
                                             SQLCHAR     *ParameterName,
                                             SQLUSMALLINT ParameterNumber,
                                             SQLSMALLINT  InputOutputType,
                                             SQLSMALLINT  ValueType,
                                             SQLSMALLINT  ParameterType,
                                             SQLULEN      ColumnSize,
                                             SQLSMALLINT  DecimalDigits,
                                             SQLPOINTER   ParameterValuePtr,
                                             SQLLEN       BufferLength,
                                             SQLLEN      *StrLen_or_IndPtr)
{
    ULN_TRACE(SQLBindParameterByNameBeta);

    return ulnBindParameter((ulnStmt *)StatementHandle,
                            (acp_uint16_t  )ParameterNumber,
                            (acp_char_t   *)ParameterName,
                            (acp_sint16_t  )InputOutputType,
                            (acp_sint16_t  )ValueType,
                            (acp_sint16_t  )ParameterType,
                            (ulvULen  )ColumnSize,
                            (acp_sint16_t  )DecimalDigits,
                            (void    *)ParameterValuePtr,
                            (ulvSLen  )BufferLength,
                            (ulvSLen *)StrLen_or_IndPtr);
}
#endif

/* 
 * PROJ-1721 Name-based Binding
 *
 * CLI���� Name-based Binding�� �����ϸ� �� �Լ��� ���ʿ��ϴ�.
 * ���� ADO.net ����̹������� ȣ��Ǹ� ADO.net ����̹� �ܿ���
 * ȣ��� ��� Statement�� ���ʿ��ϰ� �м����� �ʱ� ���� �߰��Ѵ�.
 *
 * @AnalyzeText : NULL�� �ƴ� ��� '\0' ���ڷ� �ݵ�� ������ �Ѵ�.
 *                NULL�̸� StatementText�� �м����� �ʴ´�.
 */
SQLRETURN SQL_API SQLPrepareByName(SQLHSTMT   StatementHandle,
                                   SQLCHAR   *StatementText,
                                   SQLINTEGER TextLength,
                                   SQLCHAR   *AnalyzeText)
{
    ULN_TRACE(SQLPrepareByName);
    return ulnPrepare((ulnStmt *)StatementHandle,
                      (acp_char_t *)StatementText,
                      (acp_sint32_t)TextLength,
                      (acp_char_t *)AnalyzeText);
}

