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

#include <cmAll.h>


static iduMemPool *gCmtVariablePiecePool = NULL;


IDE_RC cmtVariableInitializeStatic()
{
    /*
     * Pool �޸� �Ҵ�
     */
    IDU_FIT_POINT("cmtVariable::cmtVariableInitializeStatic::malloc::CmtVariablePiecePool");
    IDE_TEST(iduMemMgr::malloc(IDU_MEM_CMT,
                               ID_SIZEOF(iduMemPool),
                               (void **)&gCmtVariablePiecePool,
                               IDU_MEM_IMMEDIATE) != IDE_SUCCESS);

    /*
     * Pool �ʱ�ȭ
     */
    gCmtVariablePiecePool = new (gCmtVariablePiecePool) iduMemPool();

    IDE_TEST(gCmtVariablePiecePool == NULL);

    IDE_TEST(gCmtVariablePiecePool->initialize(IDU_MEM_CMT,
                                               (SChar *)"CMT_VARIABLE_PIECE_POOL",
                                               ID_SCALABILITY_SYS,
                                               ID_SIZEOF(cmtVariablePiece),
                                               8,
                                               IDU_AUTOFREE_CHUNK_LIMIT,		/* ChunkLimit */
                                               ID_TRUE,							/* UseMutex */
                                               IDU_MEM_POOL_DEFAULT_ALIGN_SIZE, /* AlignByte */
                                               ID_FALSE,						/* ForcePooling */
                                               ID_TRUE,							/* GarbageCollection */
                                               ID_TRUE,                         /* HWCacheLine */
                                               IDU_MEMPOOL_TYPE_LEGACY          /* mempool type*/) 
              != IDE_SUCCESS);			

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC cmtVariableFinalizeStatic()
{
    /*
     * Pool ����
     */
    IDE_TEST(gCmtVariablePiecePool->destroy() != IDE_SUCCESS);

    /*
     * Pool �޸� ����
     */
    IDE_TEST(iduMemMgr::free(gCmtVariablePiecePool) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC cmtVariableInitialize(cmtVariable *aVariable)
{
    /*
     * ��� �ʱ�ȭ
     */
    aVariable->mTotalSize   = 0;
    aVariable->mCurrentSize = 0;
    aVariable->mPieceCount  = 0;

    IDU_LIST_INIT(&aVariable->mPieceList);

    return IDE_SUCCESS;
}

IDE_RC cmtVariableFinalize(cmtVariable *aVariable)
{
    cmtVariablePiece *sPiece;
    iduListNode      *sIterator;
    iduListNode      *sNodeNext;

    /*
     * Piece List�� Piece���� Free
     */
    IDU_LIST_ITERATE_SAFE(&aVariable->mPieceList, sIterator, sNodeNext)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        /*
         * �⺻ Piece�� Free �� �� ����
         */
        if (sPiece != &aVariable->mPiece)
        {
            IDE_TEST(gCmtVariablePiecePool->memfree(sPiece) != IDE_SUCCESS);
        }
    }

    /*
     * �ʱ�ȭ
     */
    IDE_TEST(cmtVariableInitialize(aVariable) != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

UInt cmtVariableGetSize(cmtVariable *aVariable)
{
    return aVariable->mTotalSize;
}

IDE_RC cmtVariableGetData(cmtVariable *aVariable, UChar *aBuffer, UInt aBufferSize)
{
    iduListNode      *sIterator;
    cmtVariablePiece *sPiece;

    /*
     * Variable Data�� ��� ä�����ִ��� �˻�
     */
    IDE_TEST_RAISE(aVariable->mTotalSize != aVariable->mCurrentSize, IncompleteVariable);

    /*
     * ������ ���� ���
     */
    aBufferSize = IDL_MIN(aBufferSize, aVariable->mTotalSize);

    /*
     * Piece List�� �湮�Ͽ� Data ����
     */
    IDU_LIST_ITERATE(&aVariable->mPieceList, sIterator)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        if (sPiece->mOffset < aBufferSize)
        {
            idlOS::memcpy(aBuffer + sPiece->mOffset,
                          sPiece->mData,
                          IDL_MIN(sPiece->mSize, aBufferSize - sPiece->mOffset));
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(IncompleteVariable);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INCOMPLETE_VARIABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmtVariableGetDataWithCallback(cmtVariable                *aVariable,
                                      cmtVariableGetDataCallback  aCallback,
                                      void                       *aContext)
{
    iduListNode      *sIterator;
    cmtVariablePiece *sPiece;

    /*
     * Variable Data�� ��� ä�����ִ��� �˻�
     */
    IDE_TEST_RAISE(aVariable->mTotalSize != aVariable->mCurrentSize, IncompleteVariable);

    /*
     * Piece List�� �湮�ϸ� GetDataCallback ȣ��
     */
    IDU_LIST_ITERATE(&aVariable->mPieceList, sIterator)
    {
        sPiece = (cmtVariablePiece *)sIterator->mObj;

        IDE_TEST(aCallback(aVariable,
                           sPiece->mOffset,
                           sPiece->mSize,
                           sPiece->mData,
                           aContext) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(IncompleteVariable);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_INCOMPLETE_VARIABLE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmtVariableSetData(cmtVariable *aVariable, UChar *aBuffer, UInt aBufferSize)
{
    /*
     * �� Variable���� �˻�
     */
    IDE_TEST_RAISE(aVariable->mPieceCount > 0, VariableNotEmpty);

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

    IDU_LIST_INIT_OBJ(&aVariable->mPiece.mPieceListNode, &aVariable->mPiece);

    /*
     * Piece �߰�
     */
    IDU_LIST_ADD_LAST(&aVariable->mPieceList, &aVariable->mPiece.mPieceListNode);

    return IDE_SUCCESS;

    IDE_EXCEPTION(VariableNotEmpty);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_NOT_EMPTY));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmtVariableSetVarString(cmtVariable *aVariable, iduVarString *aString)
{
    iduListNode       *sIterator;
    iduVarStringPiece *sPiece;
    UInt               sOffset = 0;

    /*
     * �� Variable���� �˻�
     */
    IDE_TEST_RAISE(aVariable->mPieceCount > 0, VariableNotEmpty);

    IDU_LIST_ITERATE(&aString->mPieceList, sIterator)
    {
        sPiece = (iduVarStringPiece *)sIterator->mObj;

        IDE_TEST_RAISE(sPiece->mLength > ID_USHORT_MAX, PieceTooLarge);

        IDE_TEST(cmtVariableAddPiece(aVariable,
                                     sOffset,
                                     sPiece->mLength,
                                     (UChar *)sPiece->mData) != IDE_SUCCESS);

        sOffset += sPiece->mLength;
    }

    IDE_TEST_RAISE(cmtVariableGetSize(aVariable) != iduVarStringGetLength(aString),
                   SizeMismatch);

    return IDE_SUCCESS;

    IDE_EXCEPTION(VariableNotEmpty);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_NOT_EMPTY));
    }
    IDE_EXCEPTION(PieceTooLarge);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_PIECE_TOO_LARGE));
    }
    IDE_EXCEPTION(SizeMismatch);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_SIZE_MISMATCH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmtVariableAddPiece(cmtVariable *aVariable, UInt aOffset, UInt aSize, UChar *aData)
{
    cmtVariablePiece *sPiece;

    /*
     * Piece Offset �˻�
     */
    IDE_TEST_RAISE(aVariable->mCurrentSize != aOffset, VariablePieceRangeMismatch);

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
        IDU_FIT_POINT("cmtVariable::cmtVariableAddPiece::alloc::piece");
        IDE_TEST(gCmtVariablePiecePool->alloc((void **)&sPiece) != IDE_SUCCESS);
    }

    /*
     * Piece ����
     */
    sPiece->mOffset = aOffset;
    sPiece->mSize   = aSize;
    sPiece->mData   = aData;

    IDU_LIST_INIT_OBJ(&sPiece->mPieceListNode, sPiece);

    /*
     * Piece �߰�
     */
    aVariable->mPieceCount++;

    IDU_LIST_ADD_LAST(&aVariable->mPieceList, &sPiece->mPieceListNode);

    return IDE_SUCCESS;

    IDE_EXCEPTION(VariablePieceRangeMismatch);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_PIECE_RANGE_MISMATCH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC cmtVariableCopy(cmtVariable *aVariable, UChar *aBuffer, UInt aOffset, UInt aSize)
{
    iduListNode      *sIterator;
    cmtVariablePiece *sPiece;
    UInt              sSizeLeft;
    UInt              sSizeCopy;
    UInt              sOffsetCopy;

    sSizeLeft = aSize;

    IDE_TEST_RAISE(cmtVariableGetSize(aVariable) < aOffset + aSize, RangeMismatch);

    IDU_LIST_ITERATE(&aVariable->mPieceList, sIterator)
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

            sSizeCopy = IDL_MIN(sPiece->mSize - sOffsetCopy, sSizeLeft);

            idlOS::memcpy(aBuffer, sPiece->mData + sOffsetCopy, sSizeCopy);

            aBuffer   += sSizeCopy;
            sSizeLeft -= sSizeCopy;

            if (sSizeLeft == 0)
            {
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(RangeMismatch);
    {
        IDE_SET(ideSetErrorCode(cmERR_ABORT_VARIABLE_RANGE_MISMATCH));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

UChar *cmtVariableGetPieceData(cmtVariable *aVariable)
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
