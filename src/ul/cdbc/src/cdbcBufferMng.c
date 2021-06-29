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

#include <cdbc.h>



/**
 * ����¿��� �����ϴ� ���۸� �ϳ� �����Ѵ�.
 *
 * @param[in] aBufMng  ���� ���� ����ü
 * @param[in] aBufSize ������ ���� ũ��
 * @param[in] aWhence  ������ ���۸� ���� ��� ��� �������� ��Ÿ���� ��.
 *                     CDBC_BUFFER_HEAD, CDBC_BUFFER_TAIL �� �ϳ�.
 * @return ������ ������ ������. �޸𸮰� �����ϸ� NULL
 */
CDBC_INTERNAL
cdbcBufferItm * altibase_new_buffer (cdbcBufferMng *aBufMng, acp_sint32_t aBufSize, acp_sint32_t aWhence)
{
    #define CDBC_FUNC_NAME "altibase_new_buffer"

    acp_char_t    *sBuffer = NULL;
    cdbcBufferItm *sBufItm;
    acp_sint32_t   sBufSize;
    acp_rc_t       sRC;

    CDBCLOG_IN();

    CDBC_DASSERT(aBufMng != NULL);
    CDBC_DASSERT(aBufSize >= 0); /* ����Ÿ�� ���̰� 0�� ���� ��� */
    CDBC_DASSERT((aWhence == CDBC_BUFFER_HEAD) || (aWhence == CDBC_BUFFER_TAIL));
    CDBCLOG_PRINT_VAL("%d", aBufSize);

    /* cdbcBufferItm.mBuffer���� �ѹ��� �Ҵ� */
    sBufSize = ACI_SIZEOF(cdbcBufferItm);
    CDBC_ADJUST_ALIGN(sBufSize);
    CDBCLOG_CALL("acpMemCalloc");
    sRC = acpMemCalloc((void **)&sBuffer, 1, sBufSize + aBufSize);
    CDBCLOG_BACK_VAL("acpMemCalloc", "%d", sRC);
    CDBC_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC), MAllocError);
    sBufItm = (cdbcBufferItm *) sBuffer;

    if (aBufSize > 0)
    {
        sBufItm->mBuffer = sBuffer + sBufSize;
        sBufItm->mBufferLength = aBufSize;
    }
    CDBCLOG_PRINT_VAL("%p", sBufItm);
    CDBCLOG_PRINT_VAL("%p", sBufItm->mBuffer);
    CDBCLOG_PRINT_VAL("%d", sBufItm->mBufferLength);

    if (aBufMng->mHead == NULL)
    {
        aBufMng->mHead = sBufItm;
        aBufMng->mTail = sBufItm;
    }
    else
    {
        if (aWhence == CDBC_BUFFER_HEAD)
        {
            sBufItm->mNext = aBufMng->mHead;
            aBufMng->mHead = sBufItm;
        }
        else /* if (aPos == CDBC_BUFFER_TAIL) */
        {
            aBufMng->mTail->mNext = sBufItm;
            aBufMng->mTail = sBufItm;
        }
    }

    CDBCLOG_OUT_VAL("%p", sBufItm);

    return sBufItm;

    CDBC_EXCEPTION(MAllocError);
    {
        /* ���� �������̽����� ���� ó�� */
    }
    CDBC_EXCEPTION_END;

    SAFE_FREE(sBuffer);

    CDBCLOG_OUT_VAL("%s", "{null}");

    return NULL;

    #undef CDBC_FUNC_NAME
}

/**
 * ����� �ڵ��� ���� ������ ���۸� ��� �����Ѵ�.
 *
 * @param[in] aBufMng ����
 * @return �ڵ��� ��ȿ�ϸ� ALTIBASE_SUCCESS, �׷��� ������ ALTIBASE_ERROR
 */
CDBC_INTERNAL
void altibase_clean_buffer (cdbcBufferMng *aBufMng)
{
    #define CDBC_FUNC_NAME "altibase_clean_buffer"

    cdbcBufferItm *sCur;
    cdbcBufferItm *sNext;

    CDBCLOG_IN();

    CDBC_DASSERT(aBufMng != NULL);
    CDBCLOG_PRINT_VAL("%p", aBufMng);

    if (aBufMng->mHead != NULL)
    {
        sCur = aBufMng->mHead;
        while (sCur != NULL)
        {
            sNext = sCur->mNext;

            CDBCLOG_PRINT_VAL("%p", sCur);
            acpMemFree(sCur); /* free cdbcBufferItm */

            sCur = sNext;
        }

        aBufMng->mHead = NULL;
        aBufMng->mTail = NULL;
    }

    CDBCLOG_OUT();

    #undef CDBC_FUNC_NAME
}

