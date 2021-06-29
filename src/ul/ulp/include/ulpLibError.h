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

#ifndef _UTP_LIB_ERROR_H_
#define _UTP_LIB_ERROR_H_ 1

#include <acp.h>
#include <aciTypes.h>
#include <aciErrorMgr.h>
#include <sqlcli.h>
#include <ulpLibInterface.h>
#include <ulpLibConnection.h>
#include <ulpLibMacro.h>
#include <ulpLibInterCoreFuncB.h>

/*************************
 * CLI ���� ���� ���� type����.
 * TYPE1         = ���ϰ� { SQL_SUCCESS, SQL_ERROR, SQL_INVALID_HANDLE,
 *                        SQL_SUCCESS_WITH_INFO, SQL_NO_DATA_FOUND }
 * TYPE1_EXECDML = TYPE1�� ���ԵǸ� SQLExecute�� SQLExecDirect�� �̿��� DML query�� ���� �� ���.
 * TYPE2         = ���ϰ� { SQL_SUCCESS, SQL_ERROR, SQL_INVALID_HANDLE, SQL_SUCCESS_WITH_INFO }
 * TYPE3         = ���ϰ� { SQL_SUCCESS, SQL_ERROR, SQL_INVALID_HANDLE }
 *************************/
typedef enum
{
    ERR_TYPE1,
    ERR_TYPE1_DML,
    ERR_TYPE2,
    ERR_TYPE3
} ulpLibErrType;

/* ���� SQL�� ���� ��������� �����ϴ� sqlca, sqlcode, sqlstate��  *
 * ��������� ���ڷ� �Ѱ��� ������ ���� ���ִ� �Լ�.                 *
 * precompiler ���� ���� ���� ���.                             */
void ulpSetErrorInfo4PCOMP( ulpSqlca          *aSqlca,
                            ulpSqlcode        *aSqlcode,
                            ulpSqlstate       *aSqlstate,
                            const acp_char_t  *aErrMsg,
                            acp_sint32_t       aErrCode,
                            const acp_char_t  *aErrState );

/* ���� SQL�� ���� ��������� ODEBC CLI �Լ��� �̿��� ���� ���ִ� �Լ�. *
 * CLI���� �߻��� ���� ���� ���.                                  */
ACI_RC ulpSetErrorInfo4CLI( ulpLibConnNode    *aConnNode,
                            SQLHSTMT           aHstmt,
                            const SQLRETURN    aSqlRes,
                            ulpSqlca          *aSqlca,
                            ulpSqlcode        *aSqlcode,
                            ulpSqlstate       *aSqlstate,
                            ulpLibErrType      aCLIType );

ACI_RC ulpGetConnErrorMsg( ulpLibConnNode *aConnNode,
                           acp_char_t     *aMsg );

#endif

