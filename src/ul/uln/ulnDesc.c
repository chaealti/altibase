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
#include <ulnPrivate.h>
#include <ulnDesc.h>

ACI_RC ulnDescCreate(ulnObject         *aParentObject,
                     ulnDesc          **aOutputDesc,
                     ulnDescType        aDescType,
                     ulnDescAllocType   aAllocType)
{
    ULN_FLAG(sNeedDestroyMemory);
    ULN_FLAG(sNeedDestroyDiagHeader);

    ulnDesc      *sDesc;
    uluMemory    *sMemory;
    uluChunkPool *sPool;

    acp_sint16_t  sInitialState;

    ACE_ASSERT(aDescType > ULN_DESC_TYPE_NODESC && aDescType < ULN_DESC_TYPE_MAX);

    sPool = aParentObject->mPool;

    /*
     * �޸� �ν��Ͻ� ����. ûũ Ǯ�� ���� ������Ʈ�� ���� ���.
     */

    ACI_TEST(uluMemoryCreate(sPool, &sMemory) != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyMemory);

    /*
     * uluDesc �ν��Ͻ� ����.
     */

    ACI_TEST(sMemory->mOp->mMalloc(sMemory,
                                   (void **)&sDesc,
                                   ACI_SIZEOF(ulnDesc)) != ACI_SUCCESS);

    /*
     * ODBC 3.0 ���忡 ������,
     * Explicit Descriptor�� ����ڰ� SQLAllocHandle()�� �̿��ؼ� �Ҵ��ϴ�
     * ��ũ�����̴�.
     * Explicit ��ũ���ʹ� �ݵ�� DBC�� �Ҵ�Ǿ�� �Ѵ�.
     */

    if (aAllocType == ULN_DESC_ALLOCTYPE_EXPLICIT)
    {
        /*
         * BUGBUG : parent object �� dbc �� �ƴϸ� invalid handle �� ���� ��� �Ѵ�.
         */
        sInitialState = ULN_S_D1e;
    }
    else
    {
        sInitialState = ULN_S_D1i;
    }

    ulnObjectInitialize(&sDesc->mObj,
                        ULN_OBJ_TYPE_DESC,
                        aDescType,
                        sInitialState,
                        aParentObject->mPool,
                        sMemory);

    /*
     * ���� ������ Lock �� �����Ѵ�. ���� ������ Dbc �� �� ����, Stmt �� �� ���� �ִ�.
     */

    sDesc->mObj.mLock = aParentObject->mLock;

    ACI_TEST(ulnCreateDiagHeader((ulnObject *)sDesc, aParentObject->mDiagHeader.mPool)
             != ACI_SUCCESS);
    ULN_FLAG_UP(sNeedDestroyDiagHeader);

    /*
     * DescRecArray �� �����Ѵ�.
     * �ϴ�, ������ ����ϱⰡ ��������, 100 ���� element �� �⺻ ������ ����.
     */

    sDesc->mDescRecArraySize = 50;
    ACI_TEST(acpMemAlloc((void**)&sDesc->mDescRecArray,
                         ACI_SIZEOF(ulnDescRec *) * sDesc->mDescRecArraySize)
             != ACP_RC_SUCCESS);
    acpMemSet(sDesc->mDescRecArray, 0, sDesc->mDescRecArraySize * ACI_SIZEOF(ulnDescRec *));

    /*
     * SP ������ �ʰ��� ��� ������ ��.
     */

    ACI_TEST(sMemory->mOp->mMarkSP(sMemory) != ACI_SUCCESS);
    sDesc->mInitialSPIndex = sMemory->mOp->mGetCurrentSPIndex(sMemory);

    /*
     * ������� descriptor �� ����
     */

    *aOutputDesc = sDesc;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ULN_IS_FLAG_UP(sNeedDestroyDiagHeader)
    {
        ulnDestroyDiagHeader(&sDesc->mObj.mDiagHeader, ACP_FALSE);
    }

    ULN_IS_FLAG_UP(sNeedDestroyMemory)
    {
        sMemory->mOp->mDestroyMyself(sMemory);
    }

    return ACI_FAILURE;
}

ACI_RC ulnDescDestroy(ulnDesc *aDesc)
{
    ACE_ASSERT(aDesc != NULL);
    ACE_ASSERT(ULN_OBJ_GET_TYPE(aDesc) == ULN_OBJ_TYPE_DESC);

    acpMemFree(aDesc->mDescRecArray);
    aDesc->mDescRecArray     = NULL;
    aDesc->mDescRecArraySize = 0;

    /*
     * PutData ���̾��� DescRec �� ������ ã�ư��� mTempBuffer �� ������ �ش�.
     */
    ulnDescRemoveAllPDContext(aDesc);

    /*
     * DESC �� ���� DiagHeader �� ���� �޸� ��ü���� �ı��Ѵ�.
     */
    ACI_TEST(ulnDestroyDiagHeader(&(aDesc->mObj.mDiagHeader), ULN_DIAG_HDR_NOTOUCH_CHUNKPOOL)
             != ACI_SUCCESS);

    /*
     * DESC �� ���ֱ� ������ �Ǽ��� ���� ������ �����ϱ� ���ؼ� ulnObject �� ǥ�ø� �� �д�.
     * BUG-15894 �� ���� ����� ���� ���α׷��� ���� ���׸� �����ϱ� ���ؼ��̴�.
     */
    aDesc->mObj.mType = ULN_OBJ_TYPE_MAX;

    /*
     * DESC �� ������ ulumemory �� �ı��Ѵ�.
     * DESC �� ������ �ִ� mAssociatedStmtList �� ����� ���� �ڵ����� �ı���.
     */
    aDesc->mObj.mMemory->mOp->mDestroyMyself(aDesc->mObj.mMemory);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnInitializeDesc
 *
 * ulnDesc ��ü�� ���� �ʵ���� �ʱ�ȭ�Ѵ�.
 * ����ڰ� �����ϴ� �κ���
 *      SQL_DESC_BIND_TYPE          : mBindType
 *      SQL_DESC_ARRAY_SIZE         : mArraySize
 *      SQL_DESC_BIND_OFFSET_PTR    : mBindOffsetPtr
 *      SQL_DESC_ROWS_PROCESSED_PTR : mRowsProcessedPtr
 *      SQL_DESC_ARAY_STATUS_PTR    : mArrayStatusPtr
 * �� �ʱ�ȭ���� �ʴ´�.
 */
ACI_RC ulnDescInitialize(ulnDesc *aDesc, ulnObject *aParentObject)
{
    /*
     * ���� �ʵ���� �ʱ�ȭ
     */

    acpListInit(&aDesc->mAssociatedStmtList);
    acpListInit(&aDesc->mDescRecList);
    acpListInit(&aDesc->mFreeDescRecList); /* BUG-44858 */

    aDesc->mParentObject = aParentObject;
    aDesc->mDescRecCount = 0;

    // fix BUG-24380
    // Desc�� �θ� Stmt�� Stmt�� �����͸� ����
    if (aParentObject->mType == ULN_OBJ_TYPE_STMT)
    {
        aDesc->mStmt = aParentObject;
    }
    else
    {
        aDesc->mStmt = NULL;
    }

    /*
     * mHeader�� �ʱ�ȭ
     */
    if (aDesc->mObj.mState == ULN_S_D1i)
    {
        aDesc->mHeader.mAllocType = ULN_DESC_ALLOCTYPE_IMPLICIT;
    }
    else
    {
        if (aDesc->mObj.mState == ULN_S_D1e)
        {
            aDesc->mHeader.mAllocType = ULN_DESC_ALLOCTYPE_EXPLICIT;
        }
        else
        {
            ACE_ASSERT(0);
        }
    }

    acpListInit(&aDesc->mPDContextList);

    aDesc->mHeader.mHighestBoundIndex = 0;                   /* SQL_DESC_COUNT */

    /*
     * DescRecArray �� �ʱ�ȭ
     *
     * Note : �� �Լ��� unbind �ÿ��� ȣ��Ǵµ�, unbind �� �ϸ�, Desc �� uluMemory �� �ʱ�
     *        ���·� rolback ���ѹ�����.
     *        �̶�, DescRecArray �� �ִ� �迭�鵵 ��� �����ǰ� DescRecArray �� �ʱ� ���·�
     *        ���ư� ��� �Ѵ�.
     */
    // memset �� �ؾ� �ϳ�?
    // �ƴϸ�, ulnDescGetDescRec() ���� highest bound index �� �ʰ��ϴ� index �� �� ���
    // NULL �� �����ֵ��� �ߴµ�, �װ����� ����Ѱ�?
    // uluArrayInitializeToInitial(aDesc->mDescRecArray);
    acpMemSet(aDesc->mDescRecArray, 0, aDesc->mDescRecArraySize * ACI_SIZEOF(ulnDescRec *));

    return ACI_SUCCESS;
}

ACI_RC ulnDescInitializeUserPart(ulnDesc *aDesc)
{
    /*
     * ulnDesc ���� ����ڰ� �����ϴ� �κ���
     *      SQL_DESC_BIND_TYPE          : mBindType
     *      SQL_DESC_ARRAY_SIZE         : mArraySize
     *      SQL_DESC_BIND_OFFSET_PTR    : mBindOffsetPtr
     *      SQL_DESC_ROWS_PROCESSED_PTR : mRowsProcessedPtr
     *      SQL_DESC_ARAY_STATUS_PTR    : mArrayStatusPtr
     * �� �ʱ�ȭ�Ѵ�.
     *
     * �Ʒ��� �Լ��鿡�� ulnDescInitialize() �� ȣ���ؾ� �ϱ� ������
     * ��ó�� ���� �� �ξ�� �� �ʿ䰡 �ִ� :
     *      Prepare
     *      Unbind
     *      ResetParams
     */

    aDesc->mHeader.mBindType          = SQL_BIND_BY_COLUMN;
    aDesc->mHeader.mArraySize         = 1;
    aDesc->mHeader.mRowsProcessedPtr  = NULL;
    aDesc->mHeader.mBindOffsetPtr     = NULL;
    aDesc->mHeader.mArrayStatusPtr    = NULL;

    return ACI_SUCCESS;
}

/*
 * ulnDescRollBackToInitial
 *
 * �Լ��� �ϴ� �� :
 *  - ulnDesc �� ó�� ������ ���·� �ǵ�����.
 *  - ���ε带 �Ѵٰų� �ؼ� �Ҵ�� ��� �޸𸮸� �����Ѵ�.
 *    ����, ulnDesc �� ���� �޸𸮸� ���ܵΰ� �����Ѵ�.
 */
ACI_RC ulnDescRollBackToInitial(ulnDesc *aDesc)
{
    acp_uint32_t sInitialSPIndex;

    sInitialSPIndex = aDesc->mInitialSPIndex;

    ACI_TEST(aDesc->mObj.mMemory->mOp->mFreeToSP(aDesc->mObj.mMemory, sInitialSPIndex)
             != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ================================================
 * PutDataContext ����Ʈ�� �����ϴ� �Լ���
 * ================================================
 */

void ulnDescAddPDContext(ulnDesc *aDesc, ulnPDContext *aPDContext)
{
    acpListInitObj(&aPDContext->mList, aPDContext);

    acpListAppendNode(&aDesc->mPDContextList, &aPDContext->mList);
}

void ulnDescRemovePDContext(ulnDesc *aDesc, ulnPDContext *aPDContext)
{
    ACP_UNUSED(aDesc);

    acpListDeleteNode(&aPDContext->mList);
}

void ulnDescRemoveAllPDContext(ulnDesc *aDesc)
{
    acp_list_node_t *sIterator;
    acp_list_node_t *sIteratorNext;

    ACP_LIST_ITERATE_SAFE(&aDesc->mPDContextList, sIterator, sIteratorNext)
    {
        /*
         * Ȥ�ö� PutData() �ϴٰ� ������ ���� ��� �ش� PDContext �� mBuffer �� free
         * ���� ���� ä�� ���� �մ�.
         * �̰͵��� ��Ƽ� ó���� �ش�.
         */
        ((ulnPDContext *)sIterator)->mOp->mFinalize((ulnPDContext *)sIterator);
        ulnDescRemovePDContext(aDesc, (ulnPDContext *)sIterator);
    }
}

/*
 * descriptor record array list ���� �Լ���
 *
 * �������Ƽ��� ����ġ Ʈ�������, 2, 3 �� �ε����� �̿��ؼ� �ϰ� ������,
 * �׷��� �ϸ�, ���� �ʿ��� �Ϳ� ���ؼ� ���� ������. �Ϲ������� 100 �� ������
 * �÷� Ȥ�� �Ķ���͸� ���ε��Ѵٰ� �����ϸ�, ���� desc rec array header list �� �ϳ��ۿ�
 * �������� �ʴ´�.
 * ������� ����� ���� ���� ���ε� ������ 800���� �ణ ����ġ�� ������ �˰� �ִ�.
 * 800 ����, ��� 7���̴�. �־��� ��� DescRec �ϳ��� ã�� ���ؼ� LIST ITERATION �� 6 �� �Ѵ�.
 * ���� 1K �������� �ϳ��� ������� 124 ���� �迭 ���Ұ� �Ҵ�ȴ�. (64��Ʈ �÷��� ����)
 */

/*
 * Invoking Index �� �Ʒ��� �Լ��� ȣ���� ������ ��ũ���� ���ڵ��� �ε����̴�.
 *
 * ���� ��� count 10 �� DescRecArray �� �Ʒ��� �׸��� ���� �޷� �ִٰ� ���� :
 *
 *      LIST  0 10 * * * * * * * * * * ; start index 0, cnt 10
 *      LIST 20 10 * * * * * * * * * * ; start index 20, cnt 10
 *
 * �� �� ����ڰ� ParamNumber 17 ���� ���ε��ϸ�, �Լ�ȣ���� ���� ���ٰ�
 * ulnDescAddDescRec() �Լ��� �ͼ� �� �Լ��� aInvokingIndex �� 17 �� ������ ȣ��ǰ� �ȴ�.
 *
 * �׷��� �� �Լ��� DescRecArray �� �ϳ� ����µ�, start index 10, cnt 10 �� �༮��
 * ���� ����Ʈ�� �߰��Ѵ�.
 * �׷��� ����Ʈ�� ���� �׸��� ���� �ȴ� :
 *
 *      LIST  0 10 * * * * * * * * * * ; start index 0, cnt 10
 *      LIST 20 10 * * * * * * * * * * ; start index 20, cnt 10
 *      LIST 10 10 * * * * * * * * * * ; start index 10, cnt 10
 */

/*
 * Note : ulnDesc �� ulnDescRec ���� ���踦 �����ϴ� �׸�.
 *
 * +--------------+             +-DescRec-+       +-DescRec-+
 * | mDescRecList |-------------|  mList  |-------|  mList  |---
 * +--------------+             |         |       |         |
 *                              |         |       |         |
 * +-------------------+     +->|         |   +-->|         |
 * | mDescRecArrayList |     |  +---------+   |   +---------+
 * +---|---------------+    /   _____________/
 *     |                   /   /
 * +-DescRecArrayHeader---|-+-|-+---+---+----------------------------------+
 * | mList | ....       | * | * |   |   | . . .                            |
 * +---|--------------------+---+---+---+----------------------------------+
 *     |
 * +---|--------------------+---+---+---+----------------------------------+
 * | mList | ....       |   |   |   | A | . . .                            |
 * +---|--------------------+---+---+---+----------------------------------+
 *     |
 * +---|--------------------+---+---+---+----------------------------------+
 * | mList | ....       |   |   |   |   | . . .                            |
 * +--------------------|---+---+---+---+----------------------------------|
 *                      |                                                  |
 *                      |<------------ mDescRecArrayUnitCount ------------>|
 *
 * Note : ���� �׸�����, uluArrayGetElement() �Լ��� ȣ���ϸ�, �̸��׸�, �� �׸���
 *        A �� ����Ű�� �����͸� �����Ѵ�.
 *        ��, (ulnDescRec *) Ÿ���� ����Ű�� �������� (ulnDescRec **) �� �����Ѵ�.
 */

ACI_RC ulnDescAddDescRec(ulnDesc *aDesc, ulnDescRec *aDescRec)
{
    acp_uint32_t sSizeDiffrence;
    acp_uint32_t sSizeToExtend;

    /*
     * DescRecArray �� �߰��Ѵ�.
     * �ش� �ε����� �̸� �� �ִ� ���� �ֵ� ���簣�� ������ ����������.
     */

    if (aDescRec->mIndex >= aDesc->mDescRecArraySize)
    {
        sSizeDiffrence = aDescRec->mIndex - aDesc->mDescRecArraySize;
        sSizeToExtend  = (sSizeDiffrence / 50 + 1) * 50;

        ACI_TEST(acpMemRealloc((void**)&aDesc->mDescRecArray,
                                ACI_SIZEOF(ulnDescRec *) *
                                (aDesc->mDescRecArraySize + sSizeToExtend))
                 != ACP_RC_SUCCESS);

        acpMemSet(aDesc->mDescRecArray + aDesc->mDescRecArraySize,
                      0,
                      sSizeToExtend * ACI_SIZEOF(ulnDescRec *));

        aDesc->mDescRecArraySize += sSizeToExtend;
    }

    aDesc->mDescRecArray[aDescRec->mIndex] = aDescRec;

    /*
     * DescRecList �� �߰��Ѵ�.
     *
     * Note : �ֱٿ� �߰��� DescRec �ϼ��� �տ� �����Ѵ�
     */
    acpListPrependNode(&(aDesc->mDescRecList), (acp_list_t *)aDescRec);

    /*
     * ī���� ����
     */
    aDesc->mDescRecCount++;

    /*
     * Note : ulnDesc::mHeader::mCount ��
     *        ODBC ���� �����ϴ� �ǹ̿� ������ �Ȱ��� �ǹ̷� �����ϵ��� �Ѵ�.
     *        ODBC ���� �����ϴ� �ǹ̴� ��������� �����϶�.
     *
     *        HighestBoundIndex �� �̸����� ����.
     */

    if (ulnDescGetHighestBoundIndex(aDesc) < aDescRec->mIndex)
    {
        ulnDescSetHighestBoundIndex(aDesc, aDescRec->mIndex);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDescRemoveDescRec(ulnDesc    *aDesc,
                            ulnDescRec *aDescRec,
                            acp_bool_t  aPrependToFreeList)
{
    acp_uint16_t sHighestIndex;

    /*
     * BUGBUG : ���⼭ unbound �� DescRec �� �׳� ����� ���� �ȴ�.
     *          ��Ȱ���� ����� ����� ����.
     *
     *          ����ε� �ÿ��� ��Ȱ���� ������, ����ڰ� ���������� �� �����ؼ�
     *          "UnBind" �� �ϰ� �Ǹ� ����� �뷫 �����ϴ�.
     *
     *          FreeDescRecList �� �ϳ� �����?
     *          �ϴ� ���߿� ��������.
     */

    ACI_TEST(aDesc->mDescRecCount == 0);

    /*
     * BUGBUG : �Ʒ��� ��� �޸� �Ŵ��� ������ �ƴϰ�, ����ڰ� ����ε� �ϸ鼭 �Ǽ���
     *          ���ε������� ���� �ε����� �� ���� ���ɼ��� �ִ�.
     */

    ACI_TEST(aDescRec->mIndex >= aDesc->mDescRecArraySize);

    /*
     * DescRecArray �� �ش� ��Ʈ���� �� ����ֱ�
     */

    aDesc->mDescRecArray[aDescRec->mIndex] = NULL;

    /*
     * ����Ʈ���� ����
     */

    acpListDeleteNode((acp_list_t *)aDescRec);

    /*
     * Desc �� �޷� �ִ� DescRec �� ������ ��Ÿ���� ������ mDescRecCount ���ҽ�Ű��
     */

    aDesc->mDescRecCount--;


    /*
     * Note : ODBC ������ �÷��� unbind �� �Ŀ�, �� descriptor �� SQL_DESC_COUNT ��
     *        �� stmt ���� unbind �� �÷��� ������ ������ �÷��� �� ���� ū column number ��
     *        ������ desc record �� column number �� �Ǿ�� �Ѵٰ� �����ϰ� �ִ�.
     */

    sHighestIndex = ulnDescGetHighestBoundIndex(aDesc);

    if (sHighestIndex == aDescRec->mIndex)
    {
        sHighestIndex = ulnDescFindHighestBoundIndex(aDesc, sHighestIndex);
        ulnDescSetHighestBoundIndex(aDesc, sHighestIndex);
    }

    /* BUG-44858 �޸� ��Ȱ���� ���� FreeList�� �־�д�. */
    if (aPrependToFreeList == ACP_TRUE)
    {
        acpListPrependNode(&aDesc->mFreeDescRecList, (acp_list_node_t *)aDescRec);
    }
    else
    {
        /* A obsolete convention */
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnDescAddStmtToAssociatedStmtList
 *
 * STMT �� ����Ű�� �����͸� DESC �� mAssociatedStmtList �� �߰��Ѵ�.
 * SQLSetStmtAttr() �Լ� �����߿� ȣ��� ���� �ִ�.
 */
ACI_RC ulnDescAddStmtToAssociatedStmtList(ulnStmt *aStmt, ulnDesc *aDesc)
{
    ulnStmtAssociatedWithDesc *sItem;

    ACE_ASSERT(aStmt != NULL);
    ACE_ASSERT(aDesc != NULL);

    /*
     * ����Ʈ Item �� ���� �޸� Ȯ��
     */
    ACI_TEST(aDesc->mObj.mMemory->mOp->mMalloc(aDesc->mObj.mMemory,
                                               (void **)&sItem,
                                               ACI_SIZEOF(ulnStmtAssociatedWithDesc))
             != ACI_SUCCESS);

    sItem->mStmt = aStmt;

    acpListAppendNode(&aDesc->mAssociatedStmtList, &sItem->mList);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ulnDescRemoveStmtFromAssociatedStmtList
 *
 * STMT �� DESC �� mAssociatedStmtList ���� �����Ѵ�.
 * �ϳ��� �����ϴ� ���� �ƴ϶� STMT �� ������ �����ϸ� �� �����ϴ°� ������ �� �����Ѵ�.
 *
 * @note
 *  ȣ�� ����Ʈ
 *      - ulnDestroyStmt() �Լ� ���� �߿� ȣ��ȴ�.
 *      - DESC �� STMT �� Explicit Ard/Apd �� ������ �� �չ���
 *        ������ Explicit Ard/Apd �� �����Ҷ� ȣ��ȴ�.
 */
ACI_RC ulnDescRemoveStmtFromAssociatedStmtList(ulnStmt *aStmt, ulnDesc *aDesc)
{
    /*
     * BUGBUG : �߰��ؼ� ����� SUCCESS, �߰� ���ϸ� FAILURE �� �����ϰ� �ϴ°��� ���?
     */

    acp_list_node_t *sIterator;
    acp_list_node_t *sIteratorNext;

    ACE_ASSERT(aStmt != NULL);
    ACE_ASSERT(aDesc != NULL);

    ACP_LIST_ITERATE_SAFE(&(aDesc->mAssociatedStmtList), sIterator, sIteratorNext)
    {
        if (((ulnStmtAssociatedWithDesc *)sIterator)->mStmt == aStmt)
        {
            /*
             * BUGBUG : ���⼭ �Ҵ�� �޸𸮸� ������ ��� �ϳ� ����� �����ϴ�.
             *          �״��� �������� �ʴ� ����, DESC �� �����ϸ鼭 DESC �� 
             *          uluMemory�� �ı��ϸ� �ڵ����� �� �ı��ǹǷ� ���� ���ص� �ȴ�.
             */
            acpListDeleteNode(sIterator);

            /*
             * ������ ���ε� �Ǿ�� ������ ��������
             * ---> break ���ϰ� ��� ���ƶ�.
             */
        }
    }

    return ACI_SUCCESS;
}

/*
 * AssociatedStmtList �� ����ϰ� �����ϴ� �Լ���.
 *
 * �Ʒ��� �� �Լ��� ulnFreeStmtResetParams �� ulnFreeStmtUnbind ���� ȣ��ȴ�.
 * ������ �װ����� ã�� �� �ִ�.
 *
 * ���ڷ� �޴� aMemory �� �Ϲ������� ���� ��ü�� �޸𸮸� ����ϸ� �ǰڴ�.
 *
 * Note : �Ʒ��� �� �Լ�
 *        ulnDescSaveAssociatedStmtList() ��
 *        ulnDescRecoverAssociatedStmtList() �� �ݵ��
 *        ¦�� �̷� ������ �Ѵ�.
 *
 *        ����, �Լ��� ���ڷ� ���Ǵ� ��� ���ڴ� ������ �༮��� ����ϵ��� �����ؾ� �Ѵ�!!!
 */
ACI_RC ulnDescSaveAssociatedStmtList(ulnDesc      *aDesc,
                                     uluMemory    *aMemory,
                                     acp_list_t   *aTempStmtList,
                                     acp_uint32_t *aTempSP)
{
    acp_list_node_t           *sIterator;
    ulnStmtAssociatedWithDesc *sAssociatedStmt;

    acpListInit(aTempStmtList);

    *aTempSP = aMemory->mOp->mGetCurrentSPIndex(aMemory);
    ACI_TEST(aMemory->mOp->mMarkSP(aMemory) != ACI_SUCCESS);

    ACP_LIST_ITERATE(&aDesc->mAssociatedStmtList, sIterator)
    {
        ACI_TEST(aMemory->mOp->mMalloc(aMemory,
                                       (void **)(&sAssociatedStmt),
                                       ACI_SIZEOF(ulnStmtAssociatedWithDesc)) != ACI_SUCCESS);

        sAssociatedStmt->mStmt = ((ulnStmtAssociatedWithDesc *)sIterator)->mStmt;

        acpListAppendNode(aTempStmtList, (acp_list_t *)sAssociatedStmt);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnDescRecoverAssociatedStmtList(ulnDesc      *aDesc,
                                        uluMemory    *aMemory,
                                        acp_list_t   *aTempStmtList,
                                        acp_uint32_t  aTempSP)
{
    acp_list_node_t *sIterator;

    ACP_LIST_ITERATE(aTempStmtList, sIterator)
    {
        ACI_TEST(ulnDescAddStmtToAssociatedStmtList(
                ((ulnStmtAssociatedWithDesc *)sIterator)->mStmt,
                aDesc) != ACI_SUCCESS);
    }

    /*
     * �ռ� ����� temp sp �� �����Ѵ�.
     */
    ACI_TEST(aMemory->mOp->mFreeToSP(aMemory, aTempSP) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ODBC 3.0 �� SQLSetDescRec() �Լ� ���۷��� �Ŵ����� �� �޺κп��� �����ϰ� �ִ�
 * Consistency chech �� ������ �Լ��̴�.
 *
 * - Consistency check �� IRD �� ���ؼ��� ����� �� ����.
 * - IPD �� ���ؼ� ����� ���, IPD �� field �� �ƴϴ��� �� üũ�� �����ϱ� ���ؼ�
 *   Descriptor �� �ʵ���� ������ ������ ���� �ִ�.
 */
ACI_RC ulnDescCheckConsistency(ulnDesc *aDesc)
{
    ACI_TEST(ULN_OBJ_GET_TYPE(aDesc) != ULN_OBJ_TYPE_DESC);
    ACI_TEST(ULN_OBJ_GET_DESC_TYPE(aDesc) == ULN_DESC_TYPE_IRD);

    /*
     * BUGBUG : �Լ� ������ �ؾ� �Ѵ�.
     */

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static void ulnDescInitStatusArrayValuesCore(ulnDesc      *aDesc,
                                             acp_uint32_t  aStartIndex,
                                             acp_uint32_t  aArraySize,
                                             acp_uint16_t  aValue,
                                             acp_uint16_t *aStatusArrayPtr)
{
    acp_uint32_t sRowNumber;

    /*
     * Note : APD �� SQL_DESC_ARRAY_STATUS_PTR �� driver �� ��� �ൿ���� app �� �����ϴ�
     *        ���̴�. ��, driver �� ���忡���� input �̴�.
     */
    ACE_ASSERT(ULN_OBJ_GET_DESC_TYPE(aDesc) == ULN_DESC_TYPE_IRD ||
               ULN_OBJ_GET_DESC_TYPE(aDesc) == ULN_DESC_TYPE_IPD);

    ACE_ASSERT(aArraySize > 0);

    for (sRowNumber = aStartIndex; sRowNumber < aArraySize; sRowNumber++)
    {
        *(aStatusArrayPtr + sRowNumber) = aValue;
    }
}

/*
 * aArraySize �� PARAMSET_SIZE Ȥ�� ROWSET_SIZE �̴�.
 * 0 �� �� ����, 1 �̻��̾�߸� �Ѵ�.
 */
void ulnDescInitStatusArrayValues(ulnDesc      *aDesc,
                                  acp_uint32_t  aStartIndex,
                                  acp_uint32_t  aArraySize,
                                  acp_uint16_t  aValue)
{
    acp_uint16_t *sStatusArrayPtr;

    sStatusArrayPtr = ulnDescGetArrayStatusPtr(aDesc);

    if (sStatusArrayPtr != NULL)
    {
        ulnDescInitStatusArrayValuesCore(aDesc, aStartIndex, aArraySize, aValue, sStatusArrayPtr);
    }
}

/*
 * Descriptor �� ���ε�Ǿ� �ִ� DescRec ���� �ε����� ���õ� �Լ���
 *
 *      1. �ִ� ��ȿ�ε���
 *      2. �ִ� �ε���
 *      3. �ּ� �ε���
 */

static acp_uint16_t ulnDescFindEstIndex(ulnDesc *aDesc, acp_uint16_t aIndexToStartFrom, acp_sint16_t aIncrement)
{
    acp_uint16_t  i;
    ulnDescRec   *sDescRec = NULL;

    for (i = aIndexToStartFrom; i > 0; i += aIncrement)
    {
        sDescRec= ulnDescGetDescRec(aDesc, i);

        if (sDescRec!= NULL)
        {
            break;
        }
    }

    return i;
}

acp_uint16_t ulnDescFindHighestBoundIndex(ulnDesc *aDesc, acp_uint16_t aCurrentHighestBoundIndex)
{
    return ulnDescFindEstIndex(aDesc, aCurrentHighestBoundIndex, -1);
}

