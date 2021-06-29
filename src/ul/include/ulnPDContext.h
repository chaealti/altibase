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

#ifndef _O_ULN_PUTDATA_CONTEXT_H_
#define _O_ULN_PUTDATA_CONTEXT_H_ 1

#define ULN_PD_BUFFER_SIZE   (32 * 1024)

typedef enum
{
    ULN_PD_BUFFER_TYPE_UNDETERMINED,
    ULN_PD_BUFFER_TYPE_USER,
    ULN_PD_BUFFER_TYPE_ALLOC,
    ULN_PD_BUFFER_TYPE_MAX
} ulnPDBufferType;

typedef enum
{
    ULN_PD_ST_CREATED,              // 0. ������ ����
    ULN_PD_ST_NEED_DATA,            // 1. PutData() �� �� ����
    ULN_PD_ST_INITIAL,              // 2. �ʱ�ȭ�� ����
    ULN_PD_ST_ACCUMULATING_DATA     // 3. �ѹ� �̻� PutData() �� �̷����.
} ulnPDState;

typedef struct ulnPDOpSet ulnPDOpSet;

typedef struct ulnPDContext
{
    /*
     * SQLPutData() �� ���� �ӽ� ������
     */
    acp_list_node_t  mList;

    ulnPDOpSet      *mOp;

    ulnPDBufferType mBufferType;

    acp_uint8_t    *mBuffer;     /* PutData ����.
                                    variable type �� SQLPutData() �� piece by piece ��
                                    ������ �� �� ���ȴ�. */

    acp_uint32_t    mBufferSize;
    acp_uint32_t    mDataLength;
    acp_sint32_t    mIndicator;  /* SQL_NULL_DATA �� ���ŵǾ� ���� �� ����. �� �ܿ��� 0 */

    ulnPDState       mState;

} ulnPDContext;

/*
 * ========================
 * Operation Set ����
 * ========================
 */

typedef ACI_RC ulnPDPrepareBuffer(ulnPDContext *aPDContext, void *aArgument);
typedef void   ulnPDFinalize(ulnPDContext *aPDContext);
typedef ACI_RC ulnPDAccumulate(ulnPDContext *aPDContext, acp_uint8_t *aBuffer, acp_uint32_t aLength);

struct ulnPDOpSet
{
    ulnPDPrepareBuffer  *mPrepare;
    ulnPDFinalize       *mFinalize;

    ulnPDAccumulate     *mAccumulate;

};

/*
 * =========================
 * ��Ÿ �Լ���
 * =========================
 */

void       ulnPDContextSetState(ulnPDContext *aPDContext, ulnPDState aState);
ulnPDState ulnPDContextGetState(ulnPDContext *aPDContext);

void ulnPDContextCreate(ulnPDContext *aPDContext);
void ulnPDContextInitialize(ulnPDContext *aPDContext, ulnPDBufferType aBufferType);

acp_uint32_t ulnPDContextGetDataLength(ulnPDContext *aPDContext);
acp_uint32_t ulnPDContextGetBufferSize(ulnPDContext *aPDContext);

acp_uint8_t *ulnPDContextGetBuffer(ulnPDContext *aPDContext);
ACI_RC       ulnPDContextAllocBuffer(ulnPDContext *aPDContext);
void         ulnPDContextFreeBuffer(ulnPDContext *aPDContext);

#endif /* _O_ULN_PUTDATA_CONTEXT_H_ */
