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

#include <acp.h>
#include <acl.h>
#include <ace.h>
#include <aciErrorMgr.h>
#include <uluArray.h>

typedef struct uluArrayBlock
{
    acp_list_node_t mList;
    acp_uint32_t    mFirstIndex;
    acp_uint32_t    mElementCount;
    acp_uint8_t     mData[1];
} uluArrayBlock;

static void uluArrayInitBlockData(uluArray *aArray, uluArrayBlock *aBlock)
{
    acpMemSet(aBlock->mData, 0, (aBlock->mElementCount - 1) * aArray->mElementSize);
}

static ACI_RC uluArrayAllocBlock(uluArray *aArray, uluArrayBlock **aBlock)
{
    acp_uint32_t sBlockSize;
    acp_uint32_t sDataSize;

    sDataSize  = aArray->mNumberOfElementsPerBlock * aArray->mElementSize;
    sBlockSize = (ACI_SIZEOF(uluArrayBlock) - 1) + sDataSize;

    ACI_TEST(aArray->mMemory->mOp->mMalloc(aArray->mMemory, (void **)aBlock, sBlockSize)
             != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ACI_RC uluArrayCreateNewBlock(uluArray       *aArray,
                                     uluArrayBlock **aBlockOut,
                                     acp_uint32_t    aInvokingIndex)
{
    uluArrayBlock *sBlock;

    *aBlockOut = sBlock = NULL;

    ACI_TEST(uluArrayAllocBlock(aArray, &sBlock) != ACI_SUCCESS);

    acpListInit(&sBlock->mList);

    sBlock->mFirstIndex    = (aInvokingIndex / aArray->mNumberOfElementsPerBlock) *
                                                        aArray->mNumberOfElementsPerBlock;

    sBlock->mElementCount  = aArray->mNumberOfElementsPerBlock;

    uluArrayInitBlockData(aArray, sBlock);

    acpListPrependNode(&(aArray->mBlockList), (acp_list_node_t *)sBlock);

    aArray->mBlockCount++;

    *aBlockOut = sBlock;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * --------------------------------------------
 * ã���� �ϴ� index �� element �� /��/��/ ���
 * --------------------------------------------
 * aAllocNewBlock ��
 *
 *      ULU_ARRAY_IGNORE �̸�,
 *          ACI_SUCCESS �� *aElementOut = NULL �� ����.
 *
 *      ULU_ARRAY_NEW �̸�,
 *          �ش� index �� element �� �����ϴ� block �� �ϳ� �����Ϸ��� �õ��ؼ�
 *          ������ �����ϸ� ACI_FAILURE �� *aElementOut = NULL �� ����
 *          ������ �����ϸ� ACI_SUCCESS �� *aElementOut =
 *                                          ���ϴ� index �� element �� ����Ű�� �����͸� ����
 */
ACI_RC uluArrayGetElement(uluArray          *aArray,
                          acp_uint32_t       aIndex,
                          void             **aElementOut,
                          uluArrayAllocNew   aAllocNewBlock)
{
    void            *sRetElement;
    acp_list_node_t *sIterator;
    uluArrayBlock   *sBlock;

    *aElementOut = sRetElement = NULL;

    ACP_LIST_ITERATE(&(aArray->mBlockList), sIterator)
    {
        sBlock = (uluArrayBlock *)sIterator;

        if(sBlock->mFirstIndex <= aIndex &&
                                  aIndex <= sBlock->mFirstIndex + sBlock->mElementCount - 1)
        {
            /*
             * aIndex �� �ش� ����� array �� ������ ����.
             */
            sRetElement = sBlock->mData +
                          ((aIndex - sBlock->mFirstIndex) * aArray->mElementSize);
            break;
        }
    }

    if(aAllocNewBlock == ULU_ARRAY_NEW && sRetElement == NULL)
    {
        ACI_TEST(uluArrayCreateNewBlock(aArray, &sBlock, aIndex) != ACI_SUCCESS);

        sRetElement = sBlock->mData +
                      ((aIndex - sBlock->mFirstIndex) * aArray->mElementSize);
    }

    *aElementOut = sRetElement;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC uluArrayReserve(uluArray *aArray, acp_uint32_t aCount)
{
    void         *sDummyElement;
    acp_uint32_t  sCurrentIndex = 0;

    while(sCurrentIndex < aCount)
    {
        ACI_TEST(uluArrayGetElement(aArray, sCurrentIndex, &sDummyElement, ULU_ARRAY_NEW)
                 != ACI_SUCCESS);
        sCurrentIndex += aArray->mNumberOfElementsPerBlock;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC uluArrayCreate(uluMemory        *aMemory,
                      uluArray        **aArray,
                      acp_uint32_t      aElementSize,
                      acp_uint32_t      aElementsPerBlock,
                      uluArrayInitFunc *aInitFunc)
{
    uluArray *sArray;

    *aArray = sArray = NULL;

    ACI_TEST(aMemory->mOp->mMalloc(aMemory,
                                   (void **)&sArray,
                                   ACI_SIZEOF(uluArray)) != ACI_SUCCESS);

    sArray->mMemory                   = aMemory;
    sArray->mElementSize              = aElementSize;
    sArray->mNumberOfElementsPerBlock = aElementsPerBlock;
    sArray->mInitializeElements       = aInitFunc;

    sArray->mDummyAlign               = 38317; /* ^-^ */
    sArray->mBlockCount               = 0;

    acpListInit(&sArray->mBlockList);

    *aArray = sArray;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * Array �� ��� element ���� ��� 0 ���� ��������.
 */
void uluArrayInitializeContent(uluArray *aArray)
{
    acp_list_node_t   *sIterator;

    ACP_LIST_ITERATE(&(aArray->mBlockList), sIterator)
    {
        uluArrayInitBlockData(aArray, (uluArrayBlock *)sIterator);
    }
}

/*
 * Array �� �ִ� ����� ������ ��� ����Ʈ�� �ʱ�ȭ�����ش�.
 * ��, ������ ������ ���·� �ǵ�����. �ٸ� ���ڵ��� �״�� �����Ѵ�.
 */
void uluArrayInitializeToInitial(uluArray *aArray)
{
    aArray->mBlockCount = 0;
    acpListInit(&aArray->mBlockList);
}

