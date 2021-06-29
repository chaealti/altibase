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
#include <ulnConv.h>
#include <ulnConvToBINARY.h>

static ACI_RC ulnConvBinCheckTruncation(ulnFnContext *aFnContext,
                                        ulnAppBuffer *aAppBuffer,
                                        acp_sint32_t  aSourceSize,
                                        acp_uint16_t  aColumnNumber,
                                        acp_uint16_t  aRowNumber)
{
    /*
     * Note : SQL_C_BINARY Ÿ�Կ� ���� 01004 ����
     *        ����, �ҽ� ����� SQL_NULL_DATA �̸� �׳� ������ �����ϰ�, is truncated ��
     *        ID_FALSE �� �����Ѵ�.
     */

    ACI_TEST_RAISE(aSourceSize < 0, LABEL_INVALID_DATA_SIZE);

    if ((acp_uint32_t)aSourceSize > aAppBuffer->mBufferSize)
    {
        if (aAppBuffer->mColumnStatus == ULN_ROW_SUCCESS)
        {
            ulnErrorExtended(aFnContext,
                             aRowNumber,
                             aColumnNumber,
                             ulERR_IGNORE_RIGHT_TRUNCATED);
        }

        aAppBuffer->mColumnStatus = ULN_ROW_SUCCESS_WITH_INFO;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    ACI_EXCEPTION(LABEL_INVALID_DATA_SIZE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumnNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "Data size incorrect. Possible memory corruption");
    }

    return ACI_FAILURE;
}

ACI_RC ulncCHAR_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncVARCHAR_BINARY(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncVARBIT_BINARY(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    return ulncBIT_BINARY(aFnContext,
                          aAppBuffer,
                          aColumn,
                          aLength,
                          aRowNumber);
}

ACI_RC ulncBIT_BINARY(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    acp_uint32_t sPosition = 0;

    if (aAppBuffer->mBuffer == NULL || aAppBuffer->mBufferSize <= 0)
    {
        aLength->mWritten = 0;
    }
    else
    {
        if( aColumn->mGDPosition == 0 )
        {
            if( aAppBuffer->mBufferSize < ACI_SIZEOF(acp_uint32_t) )
            {
                aLength->mWritten = 0;
            }
            else
            {
                /* BUG-40865 */
                acpMemCpy( aAppBuffer->mBuffer, &aColumn->mPrecision, ACI_SIZEOF(acp_uint32_t) );
                aLength->mWritten += ACI_SIZEOF(acp_uint32_t);
                aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer + ACI_SIZEOF(acp_uint32_t),
                                                aAppBuffer->mBufferSize - ACI_SIZEOF(acp_uint32_t),
                                                aColumn->mBuffer + sPosition,
                                                aColumn->mDataLength - sPosition);

            }
        }
        else
        {
            sPosition = aColumn->mGDPosition - ACI_SIZEOF(acp_uint32_t);
            aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                            aAppBuffer->mBufferSize,
                                            aColumn->mBuffer + sPosition,
                                            aColumn->mDataLength - sPosition);
        }
    }

    aLength->mNeeded  = aColumn->mDataLength + ACI_SIZEOF(acp_uint32_t);

    aColumn->mGDPosition += aLength->mWritten;

    ACI_TEST(ulnConvBinCheckTruncation(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncSMALLINT_BINARY(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncINTEGER_BINARY(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncBIGINT_BINARY(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncREAL_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncDOUBLE_BINARY(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncBINARY_BINARY(ulnFnContext  * aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                        aAppBuffer->mBufferSize,
                                        aColumn->mBuffer + aColumn->mGDPosition,
                                        aColumn->mDataLength - aColumn->mGDPosition);
    }

    /* bug-31613: SQLGetData with SQL_C_BINARY returns a wrong len value
     * for remained data.
     *  mNeeded has to be adjusted as written size (GDPosition).
     * cf) bufLen: 3,  dataLen: 10  -> retLen: 10, 7, 4, 1 */
    aLength->mNeeded  = aColumn->mDataLength - aColumn->mGDPosition;

    aColumn->mGDPosition += aLength->mWritten;

    ACI_TEST(ulnConvBinCheckTruncation(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncNIBBLE_BINARY(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    acp_uint32_t sPosition = 0;
    /* BUG-40865 */
    acp_uint8_t  sAppValue = 0;

    if (aAppBuffer->mBuffer == NULL || aAppBuffer->mBufferSize <= 0)
    {
        aLength->mWritten = 0;
    }
    else
    {
        if( aColumn->mGDPosition == 0 )
        {
            /* BUG-40865 */
            sAppValue = (acp_uint8_t) aColumn->mPrecision;
            acpMemCpy( aAppBuffer->mBuffer, &sAppValue, ACI_SIZEOF(acp_uint8_t) );
            aLength->mWritten += ACI_SIZEOF(acp_uint8_t);
            aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer + ACI_SIZEOF(acp_uint8_t),
                                            aAppBuffer->mBufferSize - ACI_SIZEOF(acp_uint8_t),
                                            aColumn->mBuffer + sPosition,
                                            aColumn->mDataLength - sPosition);
        }
        else
        {
            sPosition = aColumn->mGDPosition - ACI_SIZEOF(acp_uint8_t);
            aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                            aAppBuffer->mBufferSize,
                                            aColumn->mBuffer + sPosition,
                                            aColumn->mDataLength - sPosition);
        }
    }

    aLength->mNeeded  = aColumn->mDataLength + ACI_SIZEOF(acp_uint8_t);

    aColumn->mGDPosition += aLength->mWritten;

    ACI_TEST(ulnConvBinCheckTruncation(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBYTE_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncDATE_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncINTERVAL_BINARY(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncNUMERIC_BINARY(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    SQL_NUMERIC_STRUCT sNumericStruct;
    /* BUG-47384 ulncNUMERIC_NUMERIC�� Ȱ���ϱ� ���ؼ� ���̷� �����Ͽ���. */
    ulnAppBuffer  sTmpAppBuffer;
    ulnLengthPair sTmpLength;

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        /* BUG-47384 
         * SQL_C_BINARY && SQL_NUMERIC ���ε��� ��쿡 
         * cmtNumeric -> SQL_NUMERIC_STRUCT�� ��ȯ�Ͽ� ����� Buffer�� �����Ѵ�.
         * �̶� ulnColumn.mGDPosition�� Ȱ���Ͽ� position �̵��� �����Ѵ�. 
         * �Ź� ��ȯ�� �����ϱ� ������ SQLGetData�� �����͸� �߶� ������ ��� 
         * ��ȯ�� SQLGetData ȣ�� ���� ��ŭ ����� �� ������ ��� �󵵰� ���������� ���̹Ƿ� �ϴ��� �����Ѵ�. (BUGBUG ���� �������� ����) */
        sTmpAppBuffer.mBuffer = &sNumericStruct;
        sTmpAppBuffer.mBufferSize = sizeof(SQL_NUMERIC_STRUCT);
        sTmpAppBuffer.mCTYPE = aAppBuffer->mCTYPE;

        ACI_TEST( ulncNUMERIC_NUMERIC(aFnContext,
                                      &sTmpAppBuffer,
                                      aColumn,
                                      &sTmpLength,
                                      aRowNumber) != ACI_SUCCESS );

        aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                        aAppBuffer->mBufferSize,
                                        sTmpAppBuffer.mBuffer + aColumn->mGDPosition,
                                        sTmpAppBuffer.mBufferSize - aColumn->mGDPosition);
    }

    /* bug-31613: SQLGetData with SQL_C_BINARY returns a wrong len value
     * for remained data.
     *  mNeeded has to be adjusted as written size (GDPosition).
     * cf) bufLen: 3,  dataLen: 10  -> retLen: 10, 7, 4, 1 */
    aLength->mNeeded  = sTmpAppBuffer.mBufferSize - aColumn->mGDPosition;
    aColumn->mGDPosition += aLength->mWritten;

    ACI_TEST(ulnConvBinCheckTruncation(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBLOB_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    ulnPtContext *sPtContext = (ulnPtContext *)aFnContext->mArgs;

    ulnLob       *sLob;
    ulnLobBuffer  sLobBuffer;
    acp_uint32_t  sSizeToRequest;

    sLob = (ulnLob *)aColumn->mBuffer;

    /*
     * ulnLobBuffer �غ�
     */

    ACI_TEST_RAISE(ulnLobBufferInitialize(&sLobBuffer,
                                          NULL,
                                          sLob->mType,
                                          ULN_CTYPE_BINARY,
                                          aAppBuffer->mBuffer,
                                          aAppBuffer->mBufferSize) != ACI_SUCCESS,
                   LABEL_MEM_MAN_ERR);

    ACI_TEST(sLobBuffer.mOp->mPrepare(aFnContext, &sLobBuffer) != ACI_SUCCESS);

    /*
     * open LOB �� get data
     */

    ACI_TEST(sLob->mOp->mOpen(aFnContext, sPtContext, sLob) != ACI_SUCCESS);

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        /*
         * ulnLobGetData() �Լ��� ȣ���.
         *
         * ���� ������ ���� ������ �߸� �ٵ�, �� �ʿ��� ��ŭ�� ������ ��û�ϵ��� �Ѵ�.
         */

        sSizeToRequest = ACP_MIN(aAppBuffer->mBufferSize, sLob->mSize - aColumn->mGDPosition);

        ACI_TEST(sLob->mOp->mGetData(aFnContext,
                                     sPtContext,
                                     sLob,
                                     &sLobBuffer,
                                     aColumn->mGDPosition,
                                     sSizeToRequest) != ACI_SUCCESS);

        /*
         * �о�ͼ� ������ LOB �������� ����� ����ڿ��� ��ȯ
         *
         * GetData �� ���� GDPosition ����
         */

        aLength->mWritten     = sLob->mSizeRetrieved;
        aLength->mNeeded      = sLob->mSize - aColumn->mGDPosition;
        aColumn->mGDPosition += aLength->mWritten;
    }

    /*
     * ulnLobBuffer ����
     */

    ACI_TEST(sLobBuffer.mOp->mFinalize(aFnContext, &sLobBuffer) != ACI_SUCCESS);

    /*
     * close LOB :
     *      1. scrollable Ŀ���� ���� Ŀ���� ���� ��.
     *         ulnCursorClose() �Լ�����
     *      2. forward only �� ������ ĳ�� �̽��� �߻����� ��.
     *         ulnFetchFromCache() �Լ����� 
     *
     *      ulnCacheCloseLobInCurrentContents() �� ȣ���ؼ� �����Ŵ.
     */

    ACI_TEST(ulnConvBinCheckTruncation(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_MEM_MAN_ERR)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "ulncBLOB_BINARY");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/*
 * PROJ-2047 Strengthening LOB - Partial Converting 
 */
ACI_RC ulncCLOB_BINARY(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    /*
     * ��ȯ���� �׳� �����ϸ� �ǹǷ�
     */
    return ulncBLOB_BINARY(aFnContext,
                           aAppBuffer,
                           aColumn,
                           aLength,
                           aRowNumber);
}

ACI_RC ulncNCHAR_BINARY(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}

ACI_RC ulncNVARCHAR_BINARY(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    return ulncBINARY_BINARY(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}
