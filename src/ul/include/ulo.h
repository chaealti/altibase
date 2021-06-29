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

#ifndef _O_ULO_H_
#define _O_ULO_H_ 1

#include <sqlcli.h>
#include <uln.h>

ACP_EXTERN_C_BEGIN

#define SQL_ATTR_EXPLAIN_PLAN           3001

/*
 * --------------------------------------------
 * SQL_ATTR_INPUT_NTS
 *      ���� �� �ִ� �� : SQL_TRUE | SQL_FALSE
 * --------------------------------------------
 * SES �� �ɼ� �� -n �ɼ��� �ִ�.
 *
 * SQLBindParameter() �� ������ ������ indicator �����͸� NULL �� �ָ�,
 * SQLBindParameter() �� ���޵� ���۰� SQL_C_BINARY �̵�, SQL_C_CHAR �̵簣��
 * Null terminated ��� �����Ѵ�.
 *
 * �׷���, SES ���� �׷��� �ؼ��� �ȵ� ������ �߰ߵȴ�.
 * �׸� ���ؼ� -n �ɼ��� �����ϴµ�,
 * �� �ɼ��� �� ���, indicator �� NULL �̴��� ������ ������
 * Null terminated ��� �����ؼ��� �ȵȴ�.
 * ������ ������� ��Ȯ�� ���� ���� ������ ��� �ִٰ� �����ϰ� �����ؾ� �Ѵ�.
 *
 * fix for BUG-13704
 *
 * �� STMT �Ӽ��� ������ ���� ���� ������ :
 *      SQL_TRUE  : default ��. indicator �� NULL �̸� ������ ������ null terminated ��� ����.
 *      SQL_FALSE : indocator �� NULL �̴��� ������ ������ null terminated �� �ƴ϶�� ����.
 */
#define SQL_ATTR_INPUT_NTS              20011

/* PROJ-1719 */
typedef void (*ulxCallbackForSesConn)(acp_sint32_t aRmid,
                                      SQLHENV      aEnv,
                                      SQLHDBC      aDbc);
void ulxSetCallbackSesConn( ulxCallbackForSesConn );

//BUG-26374 XA_CLOSE�� Client �ڿ� ������ ���� �ʽ��ϴ�. 
typedef void (*ulxCallbackForSesDisConn)();
void ulxSetCallbackSesDisConn( ulxCallbackForSesDisConn );

ACP_EXTERN_C_END

#endif /* _O_ULO_H_ */
