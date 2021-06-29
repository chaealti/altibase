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

#include <cmAllClient.h>


static acl_mem_pool_t *gCmtVariablePiecePoolClient = NULL;


ACI_RC cmtVariableInitializeStatic()
{
    /*
     * Pool �޸� �Ҵ�
     */
    ACI_TEST(acpMemAlloc((void **)&gCmtVariablePiecePoolClient, ACI_SIZEOF(acl_mem_pool_t))
             != ACP_RC_SUCCESS);

    /*
     * Pool �ʱ�ȭ
     */
    ACI_TEST(aclMemPoolCreate(gCmtVariablePiecePoolClient,
                              ACI_SIZEOF(cmtVariablePiece),
                              8, /* ElementCount   */
                              -1 /* ParallelFactor */) != ACP_RC_SUCCESS);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

ACI_RC cmtVariableFinalizeStatic()
{
    /*
     * Pool ����
     */
    aclMemPoolDestroy(gCmtVariablePiecePoolClient);

    /*
     * Pool �޸� ����
     */
    acpMemFree(gCmtVariablePiecePoolClient);
    gCmtVariablePiecePoolClient = NULL;

    return ACI_SUCCESS;
}


ACI_RC cmtVariableInitialize(cmtVariable *aVariable)
{
    /*
     * ��� �ʱ�ȭ
     */
    aVariable->mTotalSize   = 0;
    aVariable->mCurrentSize = 0;
    aVariable->mPieceCount  = 0;

    acpListInit(&aVariable->mPieceList);

    return ACI_SUCCESS;
}

ACI_RC cmtVariableFinalize(cmtVariable *aVariable)
{
    cmtVariablePiece *sPiece;
    acp_list_node_t  *sIterator;
    acp_list_node_t  *sNodeNext;

    /*
     * Piece List�� Piece���� Free
     */
    ACP_LIST_ITERATE_SAFE(&aVariable->mPieceList, sIterator, sNodeNext)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        /*
         * �⺻ Piece�� Free �� �� ����
         */
        if (sPiece != &aVariable->mPiece)
        {
            aclMemPoolFree(gCmtVariablePiecePoolClient, sPiece);
        }
    }

    /*
     * �ʱ�ȭ
     */
    ACI_TEST(cmtVariableInitialize(aVariable) != ACI_SUCCESS);

    return ACI_SUCCESS;
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}

acp_uint32_t cmtVariableGetSize(cmtVariable *aVariable)
{
    return aVariable->mTotalSize;
}

ACI_RC cmtVariableGetData(cmtVariable *aVariable, acp_uint8_t *aBuffer, acp_uint32_t aBufferSize)
{
    acp_list_node_t  *sIterator;
    cmtVariablePiece *sPiece;

    /*
     * Variable Data�� ��� ä�����ִ��� �˻�
     */
    ACI_TEST_RAISE(aVariable->mTotalSize != aVariable->mCurrentSize, IncompleteVariable);

    /*
     * ������ ���� ���
     */
    aBufferSize = ACP_MIN(aBufferSize, aVariable->mTotalSize);

    /*
     * Piece List�� �湮�Ͽ� Data ����
     */
    ACP_LIST_ITERATE(&aVariable->mPieceList, sIterator)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        if (sPiece->mOffset < aBufferSize)
        {
            acpMemCpy(aBuffer + sPiece->mOffset,
                      sPiece->mData,
                      ACP_MIN(sPiece->mSize, aBufferSize - sPiece->mOffset));
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(IncompleteVariable);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INCOMPLETE_VARIABLE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmtVariableGetDataWithCallback(cmtVariable                *aVariable,
                                        cmtVariableGetDataCallback  aCallback,
                                        void                       *aContext)
{
    acp_list_node_t  *sIterator;
    cmtVariablePiece *sPiece;

    /*
     * Variable Data�� ��� ä�����ִ��� �˻�
     */
    ACI_TEST_RAISE(aVariable->mTotalSize != aVariable->mCurrentSize, IncompleteVariable);

    /*
     * Piece List�� �湮�ϸ� GetDataCallback ȣ��
     */
    ACP_LIST_ITERATE(&aVariable->mPieceList, sIterator)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        ACI_TEST(aCallback(aVariable,
                           sPiece->mOffset,
                           sPiece->mSize,
                           sPiece->mData,
                           aContext) != ACI_SUCCESS);
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(IncompleteVariable);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_INCOMPLETE_VARIABLE));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmtVariableSetData(cmtVariable *aVariable, acp_uint8_t *aBuffer, acp_uint32_t aBufferSize)
{
    /*
     * �� Variable���� �˻�
     */
    ACI_TEST_RAISE(aVariable->mPieceCount > 0, VariableNotEmpty);

    /*
     * ��� ����
     */
    aVariable->mTotalSize     = aBufferSize;
    aVariable->mCurrentSize   = aBufferSize;
    aVariable->mPieceCount    = 1;

    /*
     * �⺻ Piece ����
     */
    aVariable->mPiece.mOffset = 0;
    aVariable->mPiece.mSize   = aBufferSize;
    aVariable->mPiece.mData   = aBuffer;

    acpListInitObj(&aVariable->mPiece.mPieceListNode, &aVariable->mPiece);

    /*
     * Piece �߰�
     */
    acpListAppendNode(&aVariable->mPieceList, &aVariable->mPiece.mPieceListNode);

    return ACI_SUCCESS;

    ACI_EXCEPTION(VariableNotEmpty);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_NOT_EMPTY));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}


ACI_RC cmtVariableSetVarString(cmtVariable *aVariable, aci_var_string_t *aString)
{
    acp_list_node_t        *sIterator;
    aci_var_string_piece_t *sPiece;
    acp_uint32_t            sOffset = 0;

    /*
     * �� Variable���� �˻�
     */
    ACI_TEST_RAISE(aVariable->mPieceCount > 0, VariableNotEmpty);

    ACP_LIST_ITERATE(&aString->mPieceList, sIterator)
    {
        sPiece = (aci_var_string_piece_t *)sIterator->mObj;

        ACI_TEST_RAISE(sPiece->mLength > ACP_UINT16_MAX, PieceTooLarge);

        ACI_TEST(cmtVariableAddPiece(aVariable,
                                     sOffset,
                                     sPiece->mLength,
                                     (acp_uint8_t *)sPiece->mData) != ACI_SUCCESS);

        sOffset += sPiece->mLength;
    }

    ACI_TEST_RAISE(cmtVariableGetSize(aVariable) != aciVarStringGetLength(aString),
                   SizeMismatch);
    return ACI_SUCCESS;

    ACI_EXCEPTION(VariableNotEmpty);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_NOT_EMPTY));
    }
    ACI_EXCEPTION(PieceTooLarge);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_PIECE_TOO_LARGE));
    }
    ACI_EXCEPTION(SizeMismatch);
    {
         ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_SIZE_MISMATCH));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmtVariableAddPiece(cmtVariable *aVariable, acp_uint32_t aOffset, acp_uint32_t aSize, acp_uint8_t *aData)
{
    cmtVariablePiece *sPiece;

    /*
     * Piece Offset �˻�
     */
    ACI_TEST_RAISE(aVariable->mCurrentSize != aOffset, VariablePieceRangeMismatch);

    /*
     * Size ����
     */
    aVariable->mCurrentSize += aSize;
    aVariable->mTotalSize    = aVariable->mCurrentSize;

    /*
     * Piece�� �ϳ��� ���� �����̸� �⺻ Piece �̿�, �׷��� ������ Pool�κ��� �Ҵ�
     */
    if (aVariable->mPieceCount == 0)
    {
        sPiece = &aVariable->mPiece;
    }
    else
    {
        ACI_TEST(aclMemPoolCalloc(gCmtVariablePiecePoolClient, (void **)&sPiece)
                 != ACP_RC_SUCCESS);
    }

    /*
     * Piece ����
     */
    sPiece->mOffset = aOffset;
    sPiece->mSize   = aSize;
    sPiece->mData   = aData;

    acpListInitObj(&sPiece->mPieceListNode, sPiece);

    /*
     * Piece �߰�
     */
    aVariable->mPieceCount++;

    acpListAppendNode(&aVariable->mPieceList, &sPiece->mPieceListNode);

    return ACI_SUCCESS;

    ACI_EXCEPTION(VariablePieceRangeMismatch);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_PIECE_RANGE_MISMATCH));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC cmtVariableCopy(cmtVariable *aVariable, acp_uint8_t *aBuffer, acp_uint32_t aOffset, acp_uint32_t aSize)
{
    acp_list_node_t  *sIterator;
    cmtVariablePiece *sPiece;
    acp_uint32_t      sSizeLeft;
    acp_uint32_t      sSizeCopy;
    acp_uint32_t      sOffsetCopy;

    sSizeLeft = aSize;

    ACI_TEST_RAISE(cmtVariableGetSize(aVariable) < aOffset + aSize, RangeMismatch);

    ACP_LIST_ITERATE(&aVariable->mPieceList, sIterator)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        if (aOffset < (sPiece->mOffset + sPiece->mSize))
        {
            if (aOffset > sPiece->mOffset)
            {
                sOffsetCopy = aOffset - sPiece->mOffset;
            }
            else
            {
                sOffsetCopy = 0;
            }

            sSizeCopy = ACP_MIN(sPiece->mSize - sOffsetCopy, sSizeLeft);

            acpMemCpy(aBuffer, sPiece->mData + sOffsetCopy, sSizeCopy);

            aBuffer   += sSizeCopy;
            sSizeLeft -= sSizeCopy;

            if (sSizeLeft == 0)
            {
                break;
            }
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(RangeMismatch);
    {
        ACI_SET(aciSetErrorCode(cmERR_ABORT_VARIABLE_RANGE_MISMATCH));
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_uint8_t *cmtVariableGetPieceData(cmtVariable *aVariable)
{
    /*
     * Piece�� 1�� ��쿡�� Data �����͸� ����
     */
    if (aVariable->mPieceCount == 1)
    {
        return aVariable->mPiece.mData;
    }

    return NULL;
}
