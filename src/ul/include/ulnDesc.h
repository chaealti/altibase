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

#ifndef _O_ULN_DESC_H_
#define _O_ULN_DESC_H_ 1

#include <ulnConfig.h>
#include <ulnPDContext.h>

typedef enum ulnDescAllocType
{
    ULN_DESC_ALLOCTYPE_IMPLICIT = SQL_DESC_ALLOC_AUTO,
    ULN_DESC_ALLOCTYPE_EXPLICIT = SQL_DESC_ALLOC_USER
} ulnDescAllocType;

/*
 * ulnStmtAssociatedWithDesc
 *
 * SQLFreeHandle() �Լ��� ��ũ���͸� ������ ��
 * �� ��ũ���͸� APD �� ARD �� ����ϴ� statement �� APD Ȥ�� ARD ��
 * ������ ��� �Ѵ�.
 * �׷���, ��ũ���ʹ� �������� statement �� APD, ARD �� ���� �� �ִ�.
 */
struct ulnStmtAssociatedWithDesc
{
    acp_list_node_t  mList;
    ulnStmt         *mStmt;
};

/*
 * ulnDescHeader
 *  - ��ũ���� ��ü�� ���� ������ ������.
 *  - ODBC ���忡�� ��ũ���� ������ ǥ���ȴ�.
 */
typedef struct ulnDescHeader
{
    /*
     * BUGBUG:
     * �̷��� �и��� ���� �� ���� ���µ�, �򰥸��⸸ �ϰ�.
     * �׳� ulnDesc ������ ���������?
     */
    ulnDescAllocType  mAllocType;             /* SQL_DESC_ALLOC_TYPE.
                                                 Explicit ���� Implicit ������ ����.
                                                 ULN_DESC_ALLOCTYPE_IMPLICIT(SQL_DESC_ALLOC_AUTO)
                                                 ULN_DESC_ALLOCTYPE_IMPLICIT(SQL_DESC_ALLOC_USER) */

    ulvULen          *mRowsProcessedPtr;      /* SQL_DESC_ROWS_PROCESSED_PTR */
    acp_uint16_t     *mArrayStatusPtr;        /* SQL_DESC_ARRAY_STATUS_PTR */
    // fix BUG-20745 BIND_OFFSET_PTR ����
    ulvULen          *mBindOffsetPtr;         /* SQL_DESC_BIND_OFFSET_PTR */

    acp_uint32_t      mArraySize;             /* SQL_DESC_ARRAY_SIZE.
                                                 Array Binding �� Array �� ���� */

    acp_uint32_t      mBindType;              /* SQL_DESC_BIND_TYPE
                                                 SQL_BIND_BY_COLUMN Ȥ�� row ����. */

    /*
     * ����ڰ� ���ε��� �÷� Ȥ�� �Ķ������ �ε��� ������ �����ϱ� ���� �����.
     *
     * �Ʒ��� ����鿡 ���� �����ϴ� ���� �߻��ϴ� �Լ��� :
     *      0. ulnDescInitialize()
     *      1. ulnDescAddDescRec()         : mHighestIndex, mLowestIndex
     *      2. ulnDescRemoveDescRec()      : mHighestIndex, mLowestIndex
     */
    acp_uint16_t     mHighestBoundIndex;     /* SQL_DESC_COUNT */
} ulnDescHeader;

struct ulnDesc
{
    ulnObject      mObj;
    ulnObject     *mParentObject;

    ulnDescHeader  mHeader;

    acp_uint32_t   mInitialSPIndex;        /* ���ʿ� Desc �� �����Ǿ��� �� ���� SP �� �ε���.
                                              ��� ���ε� ������ ���ٷ��� �� ��������
                                              freetoSP �ϸ� �ȴ�. */

    acp_list_t     mAssociatedStmtList;    /* ulnStmtAssociatedWithDesc ���� ����Ʈ�̴�. */

    acp_uint32_t   mDescRecArraySize;
    ulnDescRec   **mDescRecArray;
    acp_list_t     mDescRecList;           /* ulnDescRec ���� ����Ʈ */
    acp_uint16_t   mDescRecCount;          /* mDescRecList �� �ִ� ulnDescRec �� ���� */

    acp_list_t     mPDContextList;         /* Ȱ��ȭ�� PDContext ���� ����Ʈ */

    /* BUG-44858 DescRec�� ��Ȱ������ */
    acp_list_t     mFreeDescRecList;       /* Free�� ulnDescRec ���� ����Ʈ */

    /*
     * PROJ-1697: SQLSetDescField or SQLSetDescRec���� DescRec�� ������ ���,
     * �̸� stmt�� �ݿ��ϱ� ����. ���� SQLCopyDesc�� �����ȴٸ� mStmt��
     * ���ŵǾ�� �Ѵ�.
     */
    void           *mStmt;
};

/*
 * Function Declarations
 */

ACI_RC ulnDescCreate(ulnObject       *aParentObject,
                     ulnDesc        **aOutputDesc,
                     ulnDescType      aDescType,
                     ulnDescAllocType aAllocType);

ACI_RC ulnDescDestroy(ulnDesc *aDesc);

ACI_RC ulnDescInitialize(ulnDesc *aDesc, ulnObject *aParentObject);
ACI_RC ulnDescInitializeUserPart(ulnDesc *aDesc);

ACI_RC ulnDescRollBackToInitial(ulnDesc *aDesc);

ACI_RC ulnDescAddDescRec(ulnDesc *aDesc, ulnDescRec *aDescRec);
ACI_RC ulnDescRemoveDescRec(ulnDesc    *aDesc,
                            ulnDescRec *aDescRec,
                            acp_bool_t  aPrependToFreeList);

ACI_RC ulnDescAddStmtToAssociatedStmtList(ulnStmt *aStmt, ulnDesc *aDesc);
ACI_RC ulnDescRemoveStmtFromAssociatedStmtList(ulnStmt *aStmt, ulnDesc *aDesc);

ACI_RC ulnDescSaveAssociatedStmtList(ulnDesc      *aDesc,
                                     uluMemory    *aMemory,
                                     acp_list_t   *aTempStmtList,
                                     acp_uint32_t *aTempSP);

ACI_RC ulnDescRecoverAssociatedStmtList(ulnDesc      *aDesc,
                                        uluMemory    *aMemory,
                                        acp_list_t   *aTempStmtList,
                                        acp_uint32_t  aTempSP);

ACI_RC ulnDescCheckConsistency(ulnDesc *aDesc);

ACP_INLINE ulnDescAllocType ulnDescGetAllocType(ulnDesc *aDesc)
{
    return aDesc->mHeader.mAllocType;
}

ACP_INLINE acp_uint32_t ulnDescGetBindType(ulnDesc *aDesc)
{
    return aDesc->mHeader.mBindType;
}

ACP_INLINE void ulnDescSetBindType(ulnDesc *aDesc, acp_uint32_t aType)
{
    aDesc->mHeader.mBindType = aType;
}

// fix BUG-20745 BIND_OFFSET_PTR ����
ACP_INLINE void ulnDescSetBindOffsetPtr(ulnDesc *aDesc, ulvULen *aOffsetPtr)
{
    aDesc->mHeader.mBindOffsetPtr = aOffsetPtr;
}

ACP_INLINE ulvULen *ulnDescGetBindOffsetPtr(ulnDesc *aDesc)
{
    return aDesc->mHeader.mBindOffsetPtr;
}

ACP_INLINE ulvULen ulnDescGetBindOffsetValue(ulnDesc *aDesc)
{
    ulvULen *sOffsetPtr = NULL;

    /*
     * ���� ����ڰ� Bind Offset �� �����Ͽ��ٸ� �� ���� ���´�.
     */
    sOffsetPtr = aDesc->mHeader.mBindOffsetPtr;

    if (sOffsetPtr != NULL)
    {
        return *sOffsetPtr;
    }
    else
    {
        return 0;
    }
}

void          ulnDescInitStatusArrayValues(ulnDesc      *aDesc,
                                           acp_uint32_t  aStartIndex,
                                           acp_uint32_t  aArraySize,
                                           acp_uint16_t  aValue);

ACP_INLINE void ulnDescSetArrayStatusPtr(ulnDesc *aDesc, acp_uint16_t *aStatusPtr)
{
    aDesc->mHeader.mArrayStatusPtr = aStatusPtr;
}

ACP_INLINE acp_uint16_t *ulnDescGetArrayStatusPtr(ulnDesc *aDesc)
{
    return aDesc->mHeader.mArrayStatusPtr;
}

ACP_INLINE ulvULen *ulnDescGetRowsProcessedPtr(ulnDesc *aDesc)
{
    return aDesc->mHeader.mRowsProcessedPtr;
}

ACP_INLINE void ulnDescSetRowsProcessedPtr(ulnDesc *aDesc, ulvULen *aProcessedPtr)
{
    aDesc->mHeader.mRowsProcessedPtr = aProcessedPtr;
}

ACP_INLINE void ulnDescSetRowsProcessedPtrValue(ulnDesc *aDesc, ulvULen aValue)
{
    if (aDesc->mHeader.mRowsProcessedPtr != NULL)
    {
        // CASE-21536
        // ǥ���� SQLULEN�� ������ ���� ����ϴ� ���α׷��� SQLUINTEGER�� ����ϰ� ������,
        // �ش� ���α׷��� �ٸ� DB�� ������ �� �Ǵ� ��Ȳ�̾, �ᱹ �츮�� �ӽ÷� �ڵ� ������ �� ������.
        // �ڵ�� ǥ�ؿ� �µ��� �����ǰ� ������ �� ���� ���ο� ��Ű���� ��û�ϸ�
        // �Ʒ� �ڵ� �ּ��� ������ ���� ������ ������ �ؾ� ��.
        // *((acp_uint32_t*)(aDesc->mHeader.mRowsProcessedPtr)) = (acp_uint32_t)aValue;
        *(aDesc->mHeader.mRowsProcessedPtr) = aValue;
    }
}

ACP_INLINE void ulnDescSetArraySize(ulnDesc *aDesc, acp_uint32_t aArraySize)
{
    aDesc->mHeader.mArraySize = aArraySize;
}

ACP_INLINE acp_uint32_t ulnDescGetArraySize(ulnDesc *aDesc)
{
    return aDesc->mHeader.mArraySize;
}

/*
 * Descriptor �� ���ε�Ǿ� �ִ� DescRec ���� �ε����� ���õ� �Լ���
 */
acp_uint16_t ulnDescFindHighestBoundIndex(ulnDesc *aDesc, acp_uint16_t aCurrentHighestBoundIndex);

ACP_INLINE void ulnDescSetHighestBoundIndex(ulnDesc *aDesc, acp_uint16_t aIndex)
{
    aDesc->mHeader.mHighestBoundIndex = aIndex;
}

ACP_INLINE acp_uint16_t ulnDescGetHighestBoundIndex(ulnDesc *aDesc)
{
    return aDesc->mHeader.mHighestBoundIndex;
}

ACP_INLINE ulnDescRec *ulnDescGetDescRec(ulnDesc *aDesc, acp_uint16_t aIndex)
{
    if (aIndex > aDesc->mHeader.mHighestBoundIndex )
    {
        return NULL;
    }
    else
    {
        return aDesc->mDescRecArray[aIndex];
    }
}

/*
 * DescRecArray ���� �Լ���
 */
ulnDescRec **ulnDescGetDescRecArrayEntry(ulnDesc *aDesc, acp_uint16_t aIndex);

/*
 * =======================
 * PDContext
 * =======================
 */

void ulnDescAddPDContext(ulnDesc *aDesc, ulnPDContext *aPDContext);
void ulnDescRemovePDContext(ulnDesc *aDesc, ulnPDContext *aPDContext);
void ulnDescRemoveAllPDContext(ulnDesc *aDesc);

/*
 * StatusArray ���� �Լ���
 */
#if 0
ACI_RC ulnDescSetStatusArrayElementValue(ulnDessc, *aDesc, acp_uint32_t aRowNumber, acp_uint16_t aValue);
ACI_RC ulnDescArrangeStatusArray(ulnDesc *aImpDesc, acp_uint32_t aArrayCount, acp_uint16_t aInitValue);
#endif

#endif /* _O_ULN_DESC_H_ */

