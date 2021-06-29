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

#ifndef _O_ULN_DIAGNOSTIC_H_
#define _O_ULN_DIAGNOSTIC_H_ 1

#include <uln.h>
#include <ulnPrivate.h>

#include <ulnError.h>

/*
 * Diagnostic Header and Record
 *  ��� ENV, DBC, STMT, DESC �� ���� �ֱٿ� ������ ODBC �Լ���
 *  ���� �޼����� �����ϱ� ���ؼ� Diagnostic Header �� Record �� ������.
 *  ������ �߻����� �ʴ��� Diagnostic Header �� �⺻���� ������ �ִ´�.
 *  �ϳ��� Record �� �ϳ��� ���� �Ǵ� ������ ������ ������.
 *  Header �� Record ���� ����Ʈ�� ������.
 *
 * �������� : M$ ODBC Spec 3.0
 *      SQLGetDiagField()
 *      SQLGetDiagRec()
 *      ODBC->ODBC Programmer'sReference -> Developing Applications and Drivers
 *      -> Diagnostics
 */

/*
 * Structure Definitions
 */

struct ulnDiagRec
{
    acp_list_node_t  mList;

    ulnDiagHeader   *mHeader;

    acp_bool_t       mIsStatic;          /* BUGBUG : Ÿ���� ������. */

    acp_char_t       mSQLSTATE[6];       /* SQL_DIAG_SQLSTATE */
    acp_uint32_t     mNativeErrorCode;   /* SQL_DIAG_NATIVE. Native Error Code */
    acp_char_t      *mMessageText;       /* SQL_DIAG_MESSAGE_TEXT */

    acp_char_t      *mServerName;        /* SQL_DIAG_SERVER_NAME */
    acp_char_t      *mConnectionName;    /* SQL_DIAG_CONNECTION_NAME */

    acp_char_t      *mClassOrigin;       /* SQL_DIAG_CLASS_ORIGIN.
                                            SQLSTATE ���� Class �κ��� �����ϴ� ������ ����Ű��
                                            ��. X/Open CLI ���� �����ϴ� Class �� ���ϴ�
                                            SQLSTATE �� "ISO 9075" �� Class Origin String ��
                                            ������. ODBC ���� �����ϴ� Class �� ���ϴ� �༮����
                                            "ODBC 3.0" �� ���� ������. */

    acp_char_t      *mSubClassOrigin;    /* SQL_DIAG_SUBCLASS_ORIGIN.
                                            Identifies the defining portion of the subclass
                                            portion of the SQLSTATE code.
                                            The ODBC-specific SQLSTATES for which "ODBC 3.0" is
                                            returned include the following :
                                            01S00, 01S01, 01S02, .... IM012.
                                            �ڼ��� ������ SQLGetDiagField() �Լ� API ���۷���
                                            ����. */

    acp_sint32_t     mColumnNumber;      /* SQL_DIAG_COLUMN_NUMBER */
    acp_sint32_t     mRowNumber;         /* SQL_DIAG_ROW_NUMBER */

    acp_uint32_t     mNodeId;            /* for Shard */
};

#define ULN_DIAG_CONTINGENCY_BUFFER_SIZE 512

#ifdef COMPILE_SHARDCLI
#define ULSD_MAX_MULTI_ERROR_MSG_LEN     (ACI_MAX_ERROR_MSG_LEN+256)
#endif

struct ulnDiagHeader
{
    uluChunkPool *mPool;                /* DiagRec ���� ���� �޸� Ǯ */

    uluMemory    *mMemory;              /* DiagRec ���� ���� �޸� */
    ulnObject    *mParentObject;

    acp_list_t    mDiagRecList;         /* Diagnostic Records list */

    acp_sint32_t  mNumber;              /* SQL_DIAG_NUMBER. status record�� ����.
                                           ��, Diagnostic Record ���� ���� */

    acp_uint32_t  mAllocedDiagRecCount; /* DiagRec�� �Ҵ�� �� �����ϰ�, ������ �� �����Ѵ�. */

    acp_sint32_t  mCursorRowCount;      /* SQL_DIAG_CURSOR_ROW_COUNT */

    acp_sint64_t  mRowCount;            /* SQL_DIAG_ROW_COUNT. affected row�� ���� */
    SQLRETURN     mReturnCode;          /* SQL_DIAG_RETURNCODE */

    ulnDiagRec    mStaticDiagRec;       /* �޸� ������Ȳ�� ���� ���� diag rec */

    acp_char_t    mStaticMessage[ULN_DIAG_CONTINGENCY_BUFFER_SIZE];
                                        /* �޸� ������Ȳ�� ���� message ���� */

#ifdef COMPILE_SHARDCLI
    /* TASK-7218 Multi-Error Handling 2nd */
    acp_bool_t    mIsAllTheSame;
    acp_sint32_t  mMultiErrorMessageLen;
    acp_char_t    mMultiErrorMessage[ULSD_MAX_MULTI_ERROR_MSG_LEN];
#endif
};

#define ULN_DIAG_HDR_DESTROY_CHUNKPOOL    ACP_TRUE
#define ULN_DIAG_HDR_NOTOUCH_CHUNKPOOL    ACP_FALSE

typedef enum ulnDiagIdentifierClass
{
    ULN_DIAG_IDENTIFIER_CLASS_UNKNOWN,
    ULN_DIAG_IDENTIFIER_CLASS_HEADER,
    ULN_DIAG_IDENTIFIER_CLASS_RECORD
} ulnDiagIdentifierClass;

/*
 * Function Declarations
 */

ACI_RC ulnCreateDiagHeader(ulnObject *aParentObject, uluChunkPool *aSrcPool);
ACI_RC ulnDestroyDiagHeader(ulnDiagHeader *aDiagHeader, acp_bool_t aDestroyChunkPool);

ACI_RC ulnAddDiagRecToObject(ulnObject *aObject, ulnDiagRec *aDiagRec);
ACI_RC ulnClearDiagnosticInfoFromObject(ulnObject *aObject);

ACI_RC         ulnGetDiagRecFromObject(ulnObject *aObject, ulnDiagRec **aDiagRec, acp_sint32_t aRecNumber);
ulnDiagHeader *ulnGetDiagHeaderFromObject(ulnObject *aObject);

ACI_RC ulnDiagGetDiagIdentifierClass(acp_sint16_t aDiagIdentifier, ulnDiagIdentifierClass *aClass);


/*
 * Diagnostic Header
 */

void ulnDiagHeaderAddDiagRec(ulnDiagHeader *aDiagHeader, ulnDiagRec *aDiagRec);
void ulnDiagHeaderRemoveDiagRec(ulnDiagHeader *aDiagHeader, ulnDiagRec *aDiagRec);

ACP_INLINE ACI_RC ulnDiagSetReturnCode(ulnDiagHeader *aDiagHeader, SQLRETURN aReturnCode)
{
    /*
     * BUGBUG : ��ü�� Diagnostic Header �� �ִ� SQL_DIAG_RETURNCODE �ʵ带 �����Ѵ�.
     *          ���ýÿ� �����ڵ��� �켱���� �˻��ؼ�  ������ �����ϴ� �༮�� �켱������
     *          ������ �״�� �־� �Ѵ�.
     */

    aDiagHeader->mReturnCode = aReturnCode;

    return ACI_SUCCESS;
}

ACI_RC ulnDiagGetReturnCode         (ulnDiagHeader *aDiagHeader, SQLRETURN *aReturnCode);
ACI_RC ulnDiagGetCursorRowCount     (ulnDiagHeader *aDiagHeader, acp_sint32_t *aCursorRowcount);
ACI_RC ulnDiagGetDynamicFunctionCode(ulnDiagHeader *aDiagHeader, acp_sint32_t *aFunctionCode);
ACI_RC ulnDiagGetNumber             (ulnDiagHeader *aDiagHeader, acp_sint32_t *aNumberOfStatusRecords);
ACI_RC ulnDiagGetDynamicFunction    (ulnDiagHeader *aDiagHeader, const acp_char_t **aFunctionName);

ACP_INLINE acp_sint64_t ulnDiagGetRowCount(ulnDiagHeader *aDiagHeader)
{
    return aDiagHeader->mRowCount;
}

ACP_INLINE void ulnDiagSetRowCount(ulnDiagHeader *aDiagHeader, acp_sint64_t aRowCount)
{
    aDiagHeader->mRowCount = aRowCount;
}

/*
 * Diagnostic Record
 */

void ulnDiagRecCreate(ulnDiagHeader *aHeader, ulnDiagRec **aRec);

ACI_RC ulnDiagRecDestroy(ulnDiagRec *aRec);

void   ulnDiagRecSetMessageText(ulnDiagRec *aDiagRec, acp_char_t *aMessageText);
ACI_RC ulnDiagRecGetMessageText(ulnDiagRec   *aDiagRec,
                                acp_char_t   *aMessageText,
                                acp_sint16_t  aBufferSize,
                                acp_sint16_t *aActualLength);

void ulnDiagRecSetSqlState(ulnDiagRec *aDiagRec, acp_char_t *aSqlState);
void ulnDiagRecGetSqlState(ulnDiagRec *aDiagRec, acp_char_t **aSqlState);

void         ulnDiagRecSetNativeErrorCode(ulnDiagRec *aDiagRec, acp_uint32_t aNativeErrorCode);
acp_uint32_t ulnDiagRecGetNativeErrorCode(ulnDiagRec *aDiagRec);

ACI_RC ulnDiagGetClassOrigin   (ulnDiagRec *aDiagRec, acp_char_t **aClassOrigin);
ACI_RC ulnDiagGetConnectionName(ulnDiagRec *aDiagRec, acp_char_t **aConnectionName);
ACI_RC ulnDiagGetNative        (ulnDiagRec *aDiagRec, acp_sint32_t   *aNative);

acp_sint32_t ulnDiagRecGetRowNumber(ulnDiagRec *aDiagRec);
void         ulnDiagRecSetRowNumber(ulnDiagRec *aDiagRec, acp_sint32_t aRowNumber);
acp_sint32_t ulnDiagRecGetColumnNumber(ulnDiagRec *aDiagRec);
void         ulnDiagRecSetColumnNumber(ulnDiagRec *aDiagRec, acp_sint32_t aColumnNumber);

ACI_RC ulnDiagGetServerName    (ulnDiagRec *aDiagRec, acp_char_t **aServerName);
ACI_RC ulnDiagGetSubClassOrigin(ulnDiagRec *aDiagRec, acp_char_t **aSubClassOrigin);

/* PROJ-1381 Fetch Across Commit */

void ulnDiagRecMoveAll(ulnObject *aObjectTo, ulnObject *aObjectFrom);
void ulnDiagRecRemoveAll(ulnObject *aObject);

/* BUG-48216 */
void ulnDiagRecSoftMoveAll(ulnObject *aObjectTo, ulnObject *aObjectFrom);

/* TASK-7218 */
acp_uint32_t ulnDiagRecGetNodeId(ulnDiagRec *aDiagRec);
void         ulnDiagRecSetNodeId(ulnDiagRec *aDiagRec, acp_uint32_t aNodeId);

#endif  /* _O_ULN_DIAGNOSTIC_H_ */

