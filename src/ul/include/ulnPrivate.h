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

#ifndef _O_ULN_PRIVATE_H_
#define _O_ULN_PRIVATE_H_ 1

/*
 * �� ������ �ܺη� Export ���� �ʰ�
 * uln ���ο����� ���Ǵ� Ÿ�԰� ����� ���� ���Ǹ� �Ѵ�.
 *
 * uln ������ �ҽ��ڵ���� ��� �� ������ include�� �־�� �Ѵ�.
 *
 * �ܼ��� ���� �� �������� ���� �����̴�.
 */


#include <acp.h>
#include <acl.h>

#include <sqlcli.h>
#include <sql.h>
#include <sqlext.h>

#include <cmAllClient.h>

#include <uluMemory.h>
#include <uluArray.h>

#include <uln.h>
#include <ulo.h>
#include <ulnInit.h>

/* PROJ-1573 XA */
#include <ulxXaProtocol.h>

typedef struct ulnCache         ulnCache;
typedef struct ulnLob           ulnLob;
typedef struct ulnMeta          ulnMeta;

typedef struct ulnDiagHeader    ulnDiagHeader;
typedef struct ulnDiagRec       ulnDiagRec;

typedef struct ulnCursor        ulnCursor;

typedef struct ulnFnContext     ulnFnContext;
typedef struct ulnPtContext     ulnPtContext;
typedef struct ulnStmtAssociatedWithDesc  ulnStmtAssociatedWithDesc;

typedef struct ulnKeyset        ulnKeyset;

/* PROJ-1721 Name-based Binding */
typedef struct ulnAnalyzeStmt   ulnAnalyzeStmt;

/* PROJ-2047 Strengthening LOB - LOBCACHE */
typedef struct ulnLobCache      ulnLobCache;

/* BUG-35332 The socket files can be moved */
typedef struct ulnProperties    ulnProperties;

/* PROJ-2625 Semi-async Prefetch, Prefetch Auto-tuning */
typedef struct ulnSemiAsyncPrefetch ulnSemiAsyncPrefetch;

/* PROJ-2658 altibase sharding */
typedef struct ulsdModule   ulsdModule;

#include <ulnDebug.h>

/*
 * Note: �Ʒ� �� file�� ������ �ٲ�� ������ �ȵȴ�.
 */

#include <ulnDiagnostic.h>
#include <ulnStateMachine.h>

#include <ulnObject.h>
#include <ulnString.h>
#include <ulnParse.h>

#include <ulnTypes.h>
#include <ulnData.h>
#include <ulnMeta.h>

/* BUG-35332 The socket files can be moved */
#include <ulnProperties.h>

#include <ulnEnv.h>
#include <ulnStmtType.h>
#include <ulnStmt.h>
#include <ulnDescRec.h>
#include <ulnDesc.h>

#include <ulnCommunication.h>
#include <ulnConnAttribute.h>
#include <ulnDbc.h>
#include <ulnCache.h>
#include <ulnCursor.h>
#include <ulnFunctionContext.h>

#include <ulnError.h>

#include <ulnBindCommon.h>
#include <ulnBindInfo.h>
#include <ulnBindData.h>
#include <ulnLob.h>

/* PROJ-2047 Strengthening LOB - LOBCACHE */
#include <ulnLobCache.h>

/*
 * �Լ���
 */
#include <ulnAllocHandle.h>
#include <ulnFreeHandle.h>
#include <ulnFreeStmt.h>

#include <ulnBindCol.h>
#include <ulnBindParameter.h>

#include <ulnPrepare.h>

#include <ulnDescribeParam.h>
#include <ulnExecute.h>
#include <ulnGetStmtAttr.h>
#include <ulnSetStmtAttr.h>
#include <ulnEndTran.h>
#include <ulnSetConnectAttr.h>
#include <ulnGetConnectAttr.h>
#include <ulnServerMessage.h>

#include <ulnConnect.h>
#include <ulnDriverConnect.h>
#include <ulnDisconnect.h>
#include <ulnCloseCursor.h>
#include <ulnTables.h>
#include <ulnFetch.h>
#include <ulnExtendedFetch.h>
#include <ulnGetPlan.h>
#include <ulnGetData.h>
#include <ulnDbc.h>

/* PROJ-2177 User Interface - Cancel */
#include <ulnCancel.h>

/* PROJ-1789 Updatable Scrollable Cursor */
#include <ulnKeyset.h>

/* PROJ-1721 Name-based Binding */
#include <ulnAnalyzeStmt.h>
#include <ulnTraceLog.h> /* bug-35142 cli trace log */

/* PROJ-2638 shard native linker */
#ifdef COMPILE_SHARDCLI
#include <ulsdMain.h>
#include <ulsdModule.h>
#include <ulsdStmtInline.h>
#include <ulsdVersionClient.h>
/* PROJ-2739 Client-side Sharding LOB */
#include <ulsdLob.h>
#endif /* COMPILE_SHARDCLI */
#include <ulsdnBindData.h>
#include <ulnShard.h>

/* BUG-46785 Shard statement partial rollback */
#include <ulnSavepoint.h>
#include <ulsdnStmt.h>

/*
 * SQLBindFileToParam() �� SQLBindFileToCol() �Լ����� ���̴� SQL_C_ Ÿ��
 * BUGBUG : 15 �� ���� ������ ������ üũ�� �ʿ䰡 �ִ�.
 */
#define SQL_C_FILE 15

/*
 * BUGBUG : �ʿ���� �� ����. ȥ��������Ƿ� ���߿� ������ �� ���� ������.
 * Specified this Attributes
 * such as Autocommit etc are not
 * set one of SQL_TRUE or SQL_FALSE
 */
#define SQL_UNDEF          (0xFF)

#define ULN_FLAG(x)         acp_bool_t x = ACP_FALSE
#define ULN_FLAG_UP(x)      (x) = ACP_TRUE
#define ULN_FLAG_DOWN(x)    (x) = ACP_FALSE
#define ULN_IS_FLAG_UP(x)   if((x) == ACP_TRUE)

/*
 * ulnDesc �� �����Ǵ� DescRecArray �� ���� ������
 */
#define ULN_DESC_REC_ARRAY_UNIT_SIZE       1024

#endif /* _O_ULN_PRIVATE_H_ */

