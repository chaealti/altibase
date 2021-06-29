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
#include <ulnCursor.h>
#include <ulnKeyset.h>

/*
 * ================================================================
 *
 * Init/Destroy Functions
 *
 * ================================================================
 */

ACI_RC ulnKeysetCreate(ulnKeyset **aKeyset)
{
    acp_uint32_t    sStage;
    ulnKeyset      *sKeyset;

    sStage = 0;

    ACI_TEST( acpMemAlloc( (void**)&sKeyset, ACI_SIZEOF(ulnKeyset) ) != ACP_RC_SUCCESS );
    sStage = 1;

    sKeyset->mKeyCount              = 0;
    sKeyset->mIsFullFilled          = ACP_FALSE;
    sKeyset->mSeqKeyset.mCount      = 0;
    sKeyset->mSeqKeyset.mUsedCount  = 0;
    sKeyset->mSeqKeyset.mBlock      = NULL;

    sKeyset->mParentStmt            = NULL;

    ACI_TEST( acpMemAlloc( (void**)&sKeyset->mChunk,
                           ACI_SIZEOF(acl_mem_area_t) ) != ACP_RC_SUCCESS );

    aclMemAreaCreate(sKeyset->mChunk, 0);

    sStage = 2;

    ACI_TEST( ulnKeysetMarkChunk( sKeyset ) != ACI_SUCCESS );

    /* ulnKeyset�� ���� ��, ����ڰ� ������ locator�� ����ϱ� ���� Hash ���� */
    ACI_TEST( aclHashCreate( &sKeyset->mHashKeyset,
                             ULN_KEYSET_HASH_BUCKET_COUNT,
                             ULN_KEYSET_MAX_KEY_SIZE,
                             aclHashHashInt64,
                             aclHashCompInt64,
                             ACP_FALSE ) != ACP_RC_SUCCESS );

    *aKeyset = sKeyset;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    /* >> BUG-40316 */
    switch( sStage )
    {
        case 2:
            aclMemAreaDestroy( sKeyset->mChunk );
            acpMemFree( sKeyset->mChunk );
        case 1:
            acpMemFree( sKeyset );
        default:
            break;
    }
    /* << BUG-40316 */

    return ACI_FAILURE;
}

void ulnKeysetFreeAllBlocks(ulnKeyset *aKeyset)
{
    /*
     * RowBlock �� ������ free �ϹǷ� ĳ���� row �� ������ �����.
     */
    aKeyset->mKeyCount              = 0;
    aKeyset->mIsFullFilled          = ACP_FALSE;
    aKeyset->mSeqKeyset.mUsedCount  = 0;
}

void ulnKeysetDestroy(ulnKeyset *aKeyset)
{
    /*
     * Keyset Chunk ����
     */
    if( aKeyset->mChunk != NULL )
    {
        aclMemAreaFreeAll(aKeyset->mChunk);
        aclMemAreaDestroy(aKeyset->mChunk);
        acpMemFree( aKeyset->mChunk );
    }

    /*
     * RowBlock ����
     */
    ulnKeysetFreeAllBlocks(aKeyset);

    /*
     * RowBlockArray ����
     */
    if (aKeyset->mSeqKeyset.mBlock != NULL)
    {
        acpMemFree(aKeyset->mSeqKeyset.mBlock);
    }

    /* BUG-32474: HashKeyset ���� */
    aclHashDestroy(&aKeyset->mHashKeyset);

    /*
     * Keyset ����
     */
    acpMemFree(aKeyset);
}

ACI_RC ulnKeysetInitialize(ulnKeyset *aKeyset)
{
    aKeyset->mKeyCount              = 0;
    aKeyset->mIsFullFilled          = ACP_FALSE;
    aKeyset->mSeqKeyset.mUsedCount  = 0;

    ACI_TEST( ulnKeysetBackChunkToMark( aKeyset ) != ACI_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_sint32_t ulnKeysetGetKeyCount(ulnKeyset *aKeyset)
{
    return aKeyset->mKeyCount;
}

/**
 * Keyset�� ��� ä������ ���θ� ��´�.
 *
 * @param[in] aKeyset keyset
 *
 * @return Keyset�� ��� á���� ACP_TRUE, �ƴϸ� ACP_FALSE
 */
acp_bool_t ulnKeysetIsFullFilled(ulnKeyset *aKeyset)
{
    return aKeyset->mIsFullFilled;
}

/**
 * Keyset�� ��� ä������ ���θ� �����Ѵ�.
 *
 * @param[in] aKeyset       keyset
 * @param[in] aIsFullFilled Keyset�� ��� ä������ ����
 */
void ulnKeysetSetFullFilled(ulnKeyset *aKeyset, acp_bool_t aIsFullFilled)
{
    aKeyset->mIsFullFilled = aIsFullFilled;
}

/*
 * ================================================================
 *
 * Chunk Functions
 *
 * ================================================================
 */

/**
 * �� Chunk �޸𸮸� �Ҵ��Ѵ�.
 *
 * @param[in]  aKeyset keyset
 * @param[in]  aSize   alloc size
 * @param[out] aData   buffer pointer
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetAllocChunk( ulnKeyset     *aKeyset,
                            acp_uint32_t   aSize,
                            acp_uint8_t  **aData )
{
    ACI_TEST( aclMemAreaAlloc( aKeyset->mChunk, (void**)aData, aSize ) != ACP_RC_SUCCESS );

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * Chunk ������ ���� �ʱ�ȭ ����
 *
 * @param[in] aKeyset keyset
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetMarkChunk( ulnKeyset  *aKeyset )
{
    aclMemAreaGetSnapshot(aKeyset->mChunk, &aKeyset->mChunkStatus);

    return ACI_SUCCESS;
}

/**
 * Chunk�� �����ϱ����� �����Ѵ�.
 *
 * @param[in] aKeyset keyset
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetBackChunkToMark( ulnKeyset  *aKeyset )
{
    aclMemAreaFreeToSnapshot(aKeyset->mChunk, &aKeyset->mChunkStatus);

    return ACI_SUCCESS;
}

/*
 * ================================================================
 *
 * Keyset Build Functions
 *
 * ================================================================
 */

/**
 * Key�� Keyset�� �ִ��� Ȯ���Ѵ�.
 *
 * @param[in] aKeyset   keyset
 * @param[in] aPosition position
 *
 * @return Key�� ������ ULN_KEYSET_KEY_EXISTS,
 *         Key�� ������ ULN_KEYSET_KEY_NOT_EXISTS,
 *         Position�� ��ȿ���� ������ ULN_KEYSET_INVALID_POSITION
 */
acp_sint32_t ulnKeysetIsKeyExists(ulnKeyset    *aKeyset,
                                  acp_sint64_t  aPosition)
{
    acp_sint32_t sExistsRC;

    if (aPosition <= 0)
    {
        /* Position�� 1���� ���� */
        sExistsRC = ULN_KEYSET_INVALID_POSITION;
    }
    else if (aPosition > ulnKeysetGetKeyCount(aKeyset))
    {
        if (ulnKeysetIsFullFilled(aKeyset) == ACP_TRUE)
        {
            /* ResultSet�� ������ ��� Position */
            sExistsRC = ULN_KEYSET_INVALID_POSITION;
        }
        else
        {
            sExistsRC = ULN_KEYSET_KEY_NOT_EXISTS;
        }
    }
    else /* if ((0 < aPosition) && (aPosition <= aKeyset->mKeyCount)) */
    {
        sExistsRC = ULN_KEYSET_KEY_EXISTS;
    }

    return sExistsRC;
}

/**
 * KSAB�� Ȯ���Ѵ�.
 *
 * @param[in] aKeyset keyset
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetExtendKSAB(ulnKeyset *aKeyset)
{
    acp_uint32_t  sNewBlockSize;
    ulnKSAB      *sKSAB = &aKeyset->mSeqKeyset;

    sNewBlockSize = (sKSAB->mCount * ACI_SIZEOF(ulnKey *)) +
                    (ULN_KEYSET_KSAB_UNIT_COUNT * ACI_SIZEOF(ulnKey *));

    ACI_TEST( acpMemRealloc( (void**)&sKSAB->mBlock, sNewBlockSize ) != ACP_RC_SUCCESS );

    sKSAB->mCount += ULN_KEYSET_KSAB_UNIT_COUNT;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * KSAB�� Ȯ���ؾ� �ϴ��� Ȯ��
 *
 * @param[in] aKeyset keyset
 *
 * @return �ʿ��ϸ� ACP_TRUE, �ƴϸ� ACP_FALSE
 */
#define NEED_EXTEND_KSAB( aKeyset ) (\
    ( (aKeyset)->mSeqKeyset.mUsedCount + 1 > (aKeyset)->mSeqKeyset.mCount ) \
    ? ACP_TRUE : ACP_FALSE \
)

/**
 * �� _PROWID ���� Keyset ���� �߰��Ѵ�.
 *
 * @param[in] aKeyset   keyset
 * @param[in] aKeyValue _PROWID value
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetAddNewKSA(ulnKeyset *aKeyset)
{
    ulnKey *sNewKSA;

    if( NEED_EXTEND_KSAB(aKeyset) )
    {
        ACI_TEST( ulnKeysetExtendKSAB(aKeyset) != ACI_SUCCESS );
    }

    ACI_TEST( ulnKeysetAllocChunk( aKeyset,
                                   ULN_KEYSET_MAX_KEY_IN_KSA
                                   * ACI_SIZEOF(ulnKey),
                                   (acp_uint8_t**)&sNewKSA ) != ACI_SUCCESS );

    aKeyset->mSeqKeyset.mBlock[aKeyset->mSeqKeyset.mUsedCount] = sNewKSA;
    aKeyset->mSeqKeyset.mUsedCount++;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/**
 * �� KSA�� �ʿ����� Ȯ��
 *
 * @param[in] aKeyset keyset
 *
 * @return �ʿ��ϸ� ACP_TRUE, �ƴϸ� ACP_FALSE
 */
#define NEED_MORE_KSA( aKeyset ) (\
    ( (((aKeyset)->mKeyCount) / ULN_KEYSET_MAX_KEY_IN_KSA) >= ((aKeyset)->mSeqKeyset.mUsedCount) ) \
    ? ACP_TRUE : ACP_FALSE \
)

/**
 * �� _PROWID ���� Keyset ���� �߰��Ѵ�.
 *
 * @param[in] aKeyset   keyset
 * @param[in] aKeyValue _PROWID value
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC ulnKeysetAddNewKey(ulnKeyset *aKeyset, acp_uint8_t *aKeyValue)
{
    ulnKey *sCurKSA;
    ulnKey *sCurKey;
    acp_sint64_t sNewKeySeq = aKeyset->mKeyCount + 1;
    acp_rc_t sRC;

    // for SeqKeyset

    if( NEED_MORE_KSA(aKeyset) )
    {
        ACI_TEST( ulnKeysetAddNewKSA(aKeyset) != ACI_SUCCESS );
    }

    sCurKSA = aKeyset->mSeqKeyset.mBlock[aKeyset->mKeyCount / ULN_KEYSET_MAX_KEY_IN_KSA];
    sCurKey = &sCurKSA[aKeyset->mKeyCount % ULN_KEYSET_MAX_KEY_IN_KSA];

    acpMemCpy(sCurKey->mKeyValue, aKeyValue, ULN_KEYSET_MAX_KEY_SIZE);
    sCurKey->mSeq = sNewKeySeq;

    // for HashKeyset

    sRC = aclHashAdd(&aKeyset->mHashKeyset, sCurKey->mKeyValue, &(sCurKey->mSeq));
    ACI_TEST(ACP_RC_NOT_SUCCESS(sRC));

    aKeyset->mKeyCount = sNewKeySeq;

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * ================================================================
 *
 * Keyset Get Functions
 *
 * ================================================================
 */

/**
 * ������ ������ _PROWID ���� ��´�.
 *
 * @param[in] aKeyset keyset
 * @param[in] aSeq    seq number. 1���� ����.
 *
 * @return ������ ������ _PROWID ��. �ش簪�� ������ NULL
 */
acp_uint8_t * ulnKeysetGetKeyValue(ulnKeyset *aKeyset, acp_sint64_t aSeq)
{
    ulnKey *sKSA;
    ulnKey *sKey;

    ACI_TEST((aSeq <= 0) || (aSeq > aKeyset->mKeyCount));
    aSeq = aSeq - 1;

    sKSA = aKeyset->mSeqKeyset.mBlock[aSeq / ULN_KEYSET_MAX_KEY_IN_KSA];
    sKey = &sKSA[aSeq % ULN_KEYSET_MAX_KEY_IN_KSA];

    return sKey->mKeyValue;

    ACI_EXCEPTION_END;

    return NULL;
}

/**
 * _PROWID�� Seq�� ��´�.
 *
 * @param[in] aKeyset   keyset
 * @param[in] aKeyValue _PROWID value
 *
 * @return ����(1���� ����). ������ 0.
 */
acp_sint64_t ulnKeysetGetSeq(ulnKeyset *aKeyset, acp_uint8_t *aKeyValue)
{
    ulnKey *sKey;

    // ��� �ڵ�
    ACE_ASSERT(aKeyset != NULL);
    ACE_ASSERT(aKeyValue != NULL);

    ACI_TEST( aclHashFind( &aKeyset->mHashKeyset,
                           aKeyValue,
                           (void **)&sKey) != ACP_RC_SUCCESS );
    ACE_ASSERT(sKey != NULL);

    return sKey->mSeq;

    ACI_EXCEPTION_END;

    return 0;
}
