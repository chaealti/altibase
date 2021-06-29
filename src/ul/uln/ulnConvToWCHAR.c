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
#include <ulnConvNumeric.h>
#include <ulnConvBit.h>
#include <ulnConvChar.h>
#include <ulnConvWChar.h>

#include <ulnConvToWCHAR.h>

static ACI_RC ulnConvWCharNullTerminate(ulnFnContext *aFnContext,
                                        ulnAppBuffer *aAppBuffer,
                                        acp_sint32_t  aSourceSize,
                                        acp_uint16_t  aColumnNumber,
                                        acp_uint16_t  aRowNumber)
{
    /*
     * Note : SQL_C_WCHAR �� ��ȯ�� null terminate ���� �ִ� �Լ�.
     *        ����, �ҽ� ����� SQL_NULL_DATA �̸� �׳� ������ �����ϰ�, is truncated ��
     *        ID_FALSE �� �����Ѵ�.
     */

    acp_sint32_t sLengthNeeded;
    acp_uint32_t sOffset;

    /*
     * SourceSize �� ������ �����Ͱ� �ִ� ������ ũ�� �ǰڴ�.
     *
     * Note : aSourceSize �� 0 �� ��쵵 �ִ�.
     *        LOB �� NULL �� ��쿡�� cmtNull �� ���°��� �ƴϰ�, ������ 0 �� lob �� ���µ�,
     *        �� ��찡 �ǰڴ�.
     *
     *        �Դٰ�, aSourceSize �� �״���, üũ�� ���ص� �ǰڴ�.
     *        ����, ������ �͸� üũ����.
     */

    // BUG-27515: SQL_NO_TOTAL ó��
    if (aSourceSize == SQL_NO_TOTAL)
    {
        sLengthNeeded = SQL_NO_TOTAL;
    }
    else
    {
        /*
         * Note : SQL_NULL_DATA �� ������ NULL conversion ���� ���� �̰����� �ȿ´�.
         */
        ACI_TEST_RAISE(aSourceSize < 0, LABEL_INVALID_DATA_SIZE);

        sLengthNeeded = aSourceSize + ACI_SIZEOF(ulWChar);
    }

    /*
     * GetData() �� �� �� ���� ����� 0 �� �ֱ⵵ �Ѵ�. -_-;
     */
    if (aAppBuffer->mBufferSize > ULN_vULEN(0))
    {
        ACI_TEST( aAppBuffer->mBuffer == NULL );    //BUG-28561 [CodeSonar] Null Pointer Dereference

        // BUG-27515: SQL_NO_TOTAL ó��
        if ((sLengthNeeded == SQL_NO_TOTAL) || ((acp_uint32_t)sLengthNeeded > aAppBuffer->mBufferSize))
        {
            /* 
             * PROJ-2047 Strengthening LOB - Partial Converting
             *
             * WCHAR���� Null Char�� ó���� ���� ���� ����� ¦������ �Ѵ�.
             * ���� ����� Ȧ���� ��쿡�� BUG-28110 ��å�� ���� ���� ������ ����Ʈ�� �����Ѵ�.
             * rx6600 ��񿡼� BUS Error�� �߻��� ����.
             */
            if (aAppBuffer->mBufferSize % 2 == 1)
            {
                sOffset = aAppBuffer->mBufferSize - ACI_SIZEOF(ulWChar) - 1;
            }
            else
            {
                sOffset = aAppBuffer->mBufferSize - ACI_SIZEOF(ulWChar);
            }

            *(ulWChar*)(aAppBuffer->mBuffer + sOffset) = '\0';

            ulnErrorExtended(aFnContext,
                             aRowNumber,
                             aColumnNumber,
                             ulERR_IGNORE_RIGHT_TRUNCATED);

            aAppBuffer->mColumnStatus = ULN_ROW_SUCCESS_WITH_INFO;
        }
        else
        {
            /* 
             * PROJ-2047 Strengthening LOB - Partial Converting
             *
             * WCHAR���� Null Char�� ó���� ���� ���� ����� ¦������ �Ѵ�.
             * ���� ����� Ȧ���� ��쿡�� BUG-28110 ��å�� ���� ���� ������ ����Ʈ�� �����Ѵ�.
             * rx6600 ��񿡼� BUS Error�� �߻��� ����.
             */
            if (sLengthNeeded % 2 == 1)
            {
                sOffset = sLengthNeeded - ACI_SIZEOF(ulWChar) - 1;
            }
            else
            {
                sOffset = sLengthNeeded - ACI_SIZEOF(ulWChar);
            }

            *(ulWChar*)(aAppBuffer->mBuffer + sOffset) = '\0';
        }
    }
    else
    {
        /*
         * sLengthNeeded �� 0 �� �� ����. �� �̰���
         * sLengthNeeded > 0 and aAppBuffer->mBufferSize == 0
         * �� ����̴�.
         * ������ �� ���� 01004 �߻�
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumnNumber,
                         ulERR_IGNORE_RIGHT_TRUNCATED);

        aAppBuffer->mColumnStatus = ULN_ROW_SUCCESS_WITH_INFO;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_DATA_SIZE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumnNumber,
                         ulERR_FATAL_MEMORY_MANAGEMENT_ERROR,
                         "Data size incorrect. Possible memory corruption");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

static ulWChar ulnConvNibbleToWCharTable[16] =
{
    0x0030 /*'0'*/,
    0x0031 /*'1'*/,
    0x0032 /*'2'*/,
    0x0033 /*'3'*/,
    0x0034 /*'4'*/,
    0x0035 /*'5'*/,
    0x0036 /*'6'*/,
    0x0037 /*'7'*/,
    0x0038 /*'8'*/,
    0x0039 /*'9'*/,
    0x0041 /*'A'*/,
    0x0042 /*'B'*/,
    0x0043 /*'C'*/,
    0x0044 /*'D'*/,
    0x0045 /*'E'*/,
    0x0046 /*'F'*/
};

ACI_RC ulncCHAR_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    ulnDbc* sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    aLength->mNeeded = aColumn->mDataLength - aColumn->mGDPosition;

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            (acp_char_t *)aColumn->mBuffer,
                            aColumn->mDataLength,
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);
    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncVARCHAR_WCHAR(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    return ulncCHAR_WCHAR(aFnContext,
                          aAppBuffer,
                          aColumn,
                          aLength,
                          aRowNumber);
}

ACI_RC ulncNUMERIC_WCHAR(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    ulncNumeric sDecimal;
    acp_uint8_t sNumericBuffer[ULNC_DECIMAL_ALLOCSIZE];

    ulncNumericInitialize(&sDecimal, 10, ULNC_ENDIAN_BIG, sNumericBuffer, ULNC_DECIMAL_ALLOCSIZE);
    ulncCmtNumericToDecimal((cmtNumeric *)aColumn->mBuffer, &sDecimal);

    ulncDecimalPrintW(&sDecimal,
                     (ulWChar *)aAppBuffer->mBuffer,
                     aAppBuffer->mBufferSize / 2,
                     aColumn->mGDPosition,
                     aLength);

    if (aLength->mWritten > 0)
    {
		aColumn->mGDPosition += (aLength->mWritten / ACI_SIZEOF(ulWChar));
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncVARBIT_WCHAR(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    acp_uint32_t i;
    acp_uint32_t j;
    acp_uint32_t sStartingBit     = 0;
    acp_uint32_t sWritingPosition = 0;
    acp_uint32_t sBitLength;
    acp_uint32_t sFence;
    acp_uint8_t  sBitValue = 0;

    // BUG-23630 VARCHAR�� WCHAR�� �������� �ȵ˴ϴ�.
    // ������ �Լ��� ������� �ʱ� ������ �˾Ƽ� ������� ����ؾ� �Ѵ�.
#ifndef ENDIAN_IS_BIG_ENDIAN
    ulWChar sOneValue   = 0x0031; /*'1'*/
    ulWChar sZeroValue  = 0x0030; /*'0'*/
#else
    ulWChar sOneValue   = 0x3100; /*'1'*/
    ulWChar sZeroValue  = 0x3000; /*'0'*/
#endif

    /*
     * VARBIT �� ��� GDPosition �� ������ ��Ʈ������ �Ѵ�.
     * ��, 0x34 0xAB �� VARBIT �����Ϳ��� GDPosition �� 11 �̶��,
     *
     *      11�� �ε����� ��Ʈ -------+
     *                                |
     *                                V
     *      0 0 1 1   0 1 0 0   1 0 1 0   1 0 1 1
     *
     * ���� �׸��� ���� �κк��� �б⸦ �����ؾ� �Ѵ�.
     *
     * �̴� 2��° ����Ʈ�� 3�� ��Ʈ�̸�, sStartingBit �� ���� 3 ���� ���õȴ�.
     */

    sBitLength    = aColumn->mDataLength - aColumn->mGDPosition / 8;

    // BUG-23630 VARCHAR�� WCHAR�� �������� �ȵ˴ϴ�.
    // Precision *2 ��ŭ�� ����� ���ۿ� ��� �Ѵ�.
    sFence  = (aColumn->mPrecision * ACI_SIZEOF(ulWChar));

    // mNeeded �� ����ڰ� ��� ����Ÿ�� ��⿡ ����� ������ ũ�⸦ ���Ѵ�.
    aLength->mNeeded = (aColumn->mPrecision - aColumn->mGDPosition) * ACI_SIZEOF(ulWChar);

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        sStartingBit = aColumn->mGDPosition % 8;

        for (i = 0; i < sBitLength; i++)
        {
            sBitValue = *(aColumn->mBuffer + (aColumn->mGDPosition / 8) + i);

            for (j = sStartingBit; j < 8; j++)
            {
                // BUG-23630 VARCHAR�� WCHAR�� �������� �ȵ˴ϴ�.
                // 2byte�� ���Ƿ� ���� ����Ʈ�� üũ�ؾ� �Ѵ�.
                if ((sWritingPosition + 1) < sFence)
                {
                    if ((sWritingPosition + 1) < aAppBuffer->mBufferSize)
                    {
                        if (sBitValue & 0x01 << (7 - j))
                        {
                            *(ulWChar*)(aAppBuffer->mBuffer + sWritingPosition) = sOneValue;
                        }
                        else
                        {
                            *(ulWChar*)(aAppBuffer->mBuffer + sWritingPosition) = sZeroValue;
                        }
                        sWritingPosition = sWritingPosition + ACI_SIZEOF(ulWChar);
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }

            sStartingBit = 0;
        }

        aLength->mWritten = sWritingPosition;
    }

    if (sWritingPosition > 0)
    {
        aColumn->mGDPosition += (sWritingPosition / ACI_SIZEOF(ulWChar)) - 1;
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBIT_WCHAR(ulnFnContext  *aFnContext,
                     ulnAppBuffer  *aAppBuffer,
                     ulnColumn     *aColumn,
                     ulnLengthPair *aLength,
                     acp_uint16_t   aRowNumber)
{
    acp_uint8_t sBitValue = 0;

    sBitValue = ulnConvBitToUChar(aColumn->mBuffer);

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        if (aAppBuffer->mBufferSize > 0)
        {
            *(ulWChar*)(aAppBuffer->mBuffer) = sBitValue + 0x0030 /*'0'*/;
            aLength->mWritten = ACI_SIZEOF(ulWChar);
        }
        else
        {
            aLength->mWritten = 0;
        }
    }

    aLength->mNeeded = ACI_SIZEOF(ulWChar);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncSMALLINT_WCHAR(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    ulnDbc     *sDbc;
    acp_char_t  sTempBuffer[ULN_TRACE_LOG_MAX_DATA_LEN];
    acp_char_t *sValue;
    acp_size_t  sLen;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    sValue = ulnItoA( *(acp_sint16_t *)aColumn->mBuffer, sTempBuffer );
    sLen   = acpCStrLen( sValue, ACI_SIZEOF(sTempBuffer));

    acpMemMove( sTempBuffer, sValue, sLen+1);

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            sTempBuffer,
                            acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncINTEGER_WCHAR(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    ulnDbc     *sDbc;
    acp_char_t  sTempBuffer[ULN_TRACE_LOG_MAX_DATA_LEN];
    acp_char_t *sValue;
    acp_size_t  sLen;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    sValue = ulnItoA( *(acp_sint32_t *)aColumn->mBuffer, sTempBuffer );
    sLen   = acpCStrLen( sValue, ACI_SIZEOF(sTempBuffer));

    acpMemMove( sTempBuffer, sValue, sLen+1);

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            sTempBuffer,
                            acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBIGINT_WCHAR(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    ulnDbc     *sDbc;
    acp_char_t  sTempBuffer[ULN_TRACE_LOG_MAX_DATA_LEN];
    acp_char_t *sValue;
    acp_size_t  sLen;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    sValue = ulnItoA( *(acp_sint64_t *)aColumn->mBuffer, sTempBuffer );
    sLen   = acpCStrLen( sValue, ACI_SIZEOF(sTempBuffer));

    acpMemMove( sTempBuffer, sValue, sLen+1);

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            sTempBuffer,
                            acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncREAL_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    ulnDbc     *sDbc;
    acp_char_t  sTempBuffer[50];

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    acpSnprintf(sTempBuffer,
                ACI_SIZEOF(sTempBuffer),
                "%"ACI_FLOAT_G_FMT,
                *(acp_float_t *)aColumn->mBuffer);

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            sTempBuffer,
                            acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncDOUBLE_WCHAR(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    ulnDbc     *sDbc;
    acp_char_t  sTempBuffer[50];

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    ACI_TEST( sDbc == NULL );     //BUG-28561 [CodeSonar] Null Pointer Dereference

    acpSnprintf(sTempBuffer,
                ACI_SIZEOF(sTempBuffer),
                "%"ACI_DOUBLE_G_FMT,
                *(acp_double_t *)aColumn->mBuffer);

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            sTempBuffer,
                            acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBINARY_WCHAR(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    ACP_UNUSED(aAppBuffer);
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aLength);
    ACP_UNUSED(aRowNumber);

    ulnError(aFnContext,
             ulERR_ABORT_INVALID_APP_BUFFER_TYPE,
             SQL_C_WCHAR);

    return ACI_FAILURE;
}

ACI_RC ulncNIBBLE_WCHAR(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    acp_uint32_t i;
    acp_uint8_t  sNibbleData;
    acp_uint8_t  sNibbleLength;
    acp_uint32_t sPosition = 0;
    acp_uint32_t sStartingPosition;

    /*
     * NIBBLE �� ��� GDPosition �� ������ �Ϻ������ �Ѵ�.
     * ��, 0x34 0xAB 0x1D 0xF �� NIBBLE �����Ϳ��� GDPosition �� 5 ���,
     *
     *      5�� �ε����� �Ϻ� --+
     *                          |
     *                          V
     *              3 4  A B  1 D  F
     *
     * ���� �׸��� ���� �κк��� �б⸦ �����ؾ� �Ѵ�.
     *
     * �̴� 3��° ����Ʈ�� ���� �Ϻ��̸�, sStartingBit �� ���� 3 ���� ���õȴ�.
     */

    sNibbleLength     = aColumn->mPrecision;
    aLength->mNeeded  = (sNibbleLength - aColumn->mGDPosition) * ACI_SIZEOF(ulWChar);

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        /*
         * �������� ���� ������ �Ϻ��� �Ϻ��ε���
         */

        sStartingPosition = aColumn->mGDPosition;

        /*
         * ��ȯ ����
         */

        for (i = sStartingPosition; i < sNibbleLength; i++)
        {
            if((i & 0x01) == 0x01)
            {
                sNibbleData = aColumn->mBuffer[i / 2] & 0x0f;
            }
            else
            {
                sNibbleData = aColumn->mBuffer[i / 2] >> 4;
            }

            if (sPosition < aAppBuffer->mBufferSize - 1)
            {
                *(ulWChar*)(aAppBuffer->mBuffer + sPosition) = ulnConvNibbleToWCharTable[sNibbleData];
                sPosition = sPosition + ACI_SIZEOF(ulWChar);
            }
            else
            {
                break;
            }
        }

        aLength->mWritten = sPosition;
    }

    if (aLength->mWritten > 0)
    {
        aColumn->mGDPosition += (aLength->mWritten / ACI_SIZEOF(ulWChar));
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncBYTE_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    acp_uint32_t sNumberOfBytesWritten = 0;

    aLength->mNeeded  = (aColumn->mDataLength - aColumn->mGDPosition) * ACI_SIZEOF(ulWChar) * 2;

    if (aAppBuffer->mBuffer == NULL)
    {
        sNumberOfBytesWritten = 0;
    }
    else
    {
        sNumberOfBytesWritten = ulnConvDumpAsWChar(aAppBuffer->mBuffer,
                                                   aAppBuffer->mBufferSize,
                                                   aColumn->mBuffer + aColumn->mGDPosition,
                                                   aColumn->mDataLength - aColumn->mGDPosition);

        /*
         * Null termination.
         *
         * ulnConvDumpAsChar() �Լ��� �����ϰ� null �͹̳������� �� ���� �����Ѵ�.
         */

        *(ulWChar*)(aAppBuffer->mBuffer + sNumberOfBytesWritten) = 0;
    }

    aLength->mWritten = sNumberOfBytesWritten;    // null termination ���� ����.

    if (sNumberOfBytesWritten > 0)
    {
        aColumn->mGDPosition += ((sNumberOfBytesWritten / 2) / ACI_SIZEOF(ulWChar));
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncDATE_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    acp_char_t   *sDateFormat;
    mtdDateType  *sMtdDate;

    acp_char_t    sDateString[128];
    acp_uint32_t  sDateStringLength = 0;
    ulnDbc       *sDbc;
    ulnCharSet    sCharSet;
    acp_sint32_t  sState = 0;

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * Date �� ��Ʈ������ ��ȯ
     */

    sDateFormat = aFnContext->mHandle.mStmt->mParentDbc->mDateFormat;

    sMtdDate = (mtdDateType *)aColumn->mBuffer;

    ACI_TEST_RAISE( mtdDateInterfaceToChar(sMtdDate,
                                           (acp_uint8_t *)sDateString,
                                           &sDateStringLength,
                                           ACI_SIZEOF(sDateString),
                                           (acp_uint8_t *)sDateFormat,
                                           acpCStrLen(sDateFormat, ACP_SINT32_MAX))
                    != ACI_SUCCESS, INVALID_DATETIME_FORMAT_ERROR );

    ACI_TEST(ulnCharSetConvert(&sCharSet,
                               aFnContext,
                               NULL,
                               sDbc->mClientCharsetLangModule,    //BUG-22684
                               (const mtlModule*)sDbc->mWcharCharsetLangModule,
                               (void*)sDateString,
                               sDateStringLength,
                               CONV_DATA_OUT)
             != ACI_SUCCESS);

    aLength->mNeeded  = ulnCharSetGetConvertedTextLen(&sCharSet) -
                       (aColumn->mGDPosition * ACI_SIZEOF(ulWChar));

    /*
     * ����� ���ۿ� ����
     */

    aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                    aAppBuffer->mBufferSize,
                                    ulnCharSetGetConvertedText(&sCharSet) + (aColumn->mGDPosition * ACI_SIZEOF(ulWChar)),
                                    ulnCharSetGetConvertedTextLen(&sCharSet) - (aColumn->mGDPosition * ACI_SIZEOF(ulWChar)));

    if (aLength->mWritten > 0)
    {
        aColumn->mGDPosition += (aLength->mWritten / ACI_SIZEOF(ulWChar)) - 1;
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    sState = 0;
    ulnCharSetFinalize(&sCharSet);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION( INVALID_DATETIME_FORMAT_ERROR )
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_Invalid_Datatime_Format_Error);
    }

    ACI_EXCEPTION_END;

    if (sState == 1)
    {
        ulnCharSetFinalize(&sCharSet);
    }

    return ACI_FAILURE;
}

ACI_RC ulncINTERVAL_WCHAR(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    /*
     * BUGBUG : ODBC �� interval literal �� ���� ������ �������� �ұ��ϰ�
     *          �߻��� ������ diff �� ���� Double �� �� �ش�.
     *
     *          �������� ǥ�ش�� �ϵ��� ����.
     */
    cmtInterval  *sCmInterval;
    acp_double_t  sDoubleValue;
    acp_char_t    sTempBuffer[50];
    ulnDbc       *sDbc;
    ulnCharSet    sCharSet;
    acp_sint32_t  sState = 0;

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    sCmInterval = (cmtInterval *)aColumn->mBuffer;

    sDoubleValue = ((acp_double_t)sCmInterval->mSecond / (3600 * 24)) +
                   (((acp_double_t)sCmInterval->mMicroSecond * 0.000001) / (3600 * 24));

    acpSnprintf(sTempBuffer, ACI_SIZEOF(sTempBuffer), "%"ACI_DOUBLE_G_FMT, sDoubleValue);

    ACI_TEST(ulnCharSetConvert(&sCharSet,
                               aFnContext,
                               NULL,
                               sDbc->mClientCharsetLangModule,    //BUG-22684
                               (const mtlModule*)sDbc->mWcharCharsetLangModule,
                               (void*)sTempBuffer,
                               acpCStrLen(sTempBuffer, ACI_SIZEOF(sTempBuffer)),
                               CONV_DATA_OUT)
             != ACI_SUCCESS);

    aLength->mWritten = ulnConvCopy(aAppBuffer->mBuffer,
                                    aAppBuffer->mBufferSize,
                                    ulnCharSetGetConvertedText(&sCharSet) + (aColumn->mGDPosition * ACI_SIZEOF(ulWChar)),
                                    ulnCharSetGetConvertedTextLen(&sCharSet) - (aColumn->mGDPosition * ACI_SIZEOF(ulWChar)));


    aLength->mNeeded  = ulnCharSetGetConvertedTextLen(&sCharSet) -
                       (aColumn->mGDPosition * ACI_SIZEOF(ulWChar));

    if (aLength->mWritten > 0)
    {
        aColumn->mGDPosition += (aLength->mWritten / ACI_SIZEOF(ulWChar)) - 1;
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    sState = 0;
    ulnCharSetFinalize(&sCharSet);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    if (sState == 1)
    {
        ulnCharSetFinalize(&sCharSet);
    }

    return ACI_FAILURE;
}

ACI_RC ulncBLOB_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    ulnPtContext *sPtContext  = (ulnPtContext *)aFnContext->mArgs;

    ulnLob       *sLob        = (ulnLob *)aColumn->mBuffer;
    ulnLobBuffer  sLobBuffer;
    acp_uint32_t  sSizeToRequest;

    /*
     * ulnLobBuffer �غ�
     */

    ACI_TEST_RAISE(ulnLobBufferInitialize(&sLobBuffer,
                                          NULL,
                                          sLob->mType,
                                          ULN_CTYPE_WCHAR,
                                          aAppBuffer->mBuffer,
                                          aAppBuffer->mBufferSize) != ACI_SUCCESS,
                   LABEL_MEM_MAN_ERR);

    ACI_TEST(sLobBuffer.mOp->mPrepare(aFnContext, &sLobBuffer) != ACI_SUCCESS);

    /*
     * open LOB �� get data
     */

    ACI_TEST(sLob->mOp->mOpen(aFnContext, sPtContext, sLob) != ACI_SUCCESS);

    /*
     * ulnLobGetData() �Լ��� ȣ���.
     *      �̿Ͱ��� blob --> char �� ��쿡�� ulnLobBufferDataInCHAR() �Լ���.
     *
     * CHAR �� ��ȯ�� �� ���̱� ������ �ι��� ���۰� �ʿ��ϴ�.
     * ������ ����ڿ��� �Ѱ����� ���� �����͸� �� ���� �ʿ� ����,
     * �Ѱ��� �� �ִ� ��ŭ�� ������ ��û����.
     *
     * Note : BINARY --> CHAR �� ��� �Ʒ��� ��Ģ�� ����ȴ� :
     *
     *          1. ����� ���۰� ����� Ŭ ��� (8 bytes)
     *              binary data    : 0x1B 0x2D 0x3F
     *              converted char : 1B2D3F\0
     *
     *          2. ����� ���۰� ���� ��� (6 bytes)
     *              binary data    : 0x1B 0x2D 0x3F
     *              converted char : 1B2D\0   : ������ ������ ���� : 5 bytes
     *                                          ����� ������ ������ ����Ʈ : �ƹ��͵� �� ��.
     *
     *          3. ����� ���۰� ���� ��� (5 bytes)
     *              binary data    : 0x1B 0x2D 0x3F
     *              converted char : 1B2D\0   : ������ ������ ���� : 5 bytes
     *
     *        ��ó�� ����� ���۰� �۾Ƽ� null terminate �� �ؾ� �ϴ� ��쿡,
     *        �� ������ 16���� �ѹ���Ʈ�� �߸��� ���� �Ϻ� ������ �� ��쿡��
     *        �ι���Ʈ�� �� �߶������ �� �͹̳���Ʈ ���Ѿ� �Ѵ�.
     */

    if (aAppBuffer->mBuffer == NULL)
    {
        aLength->mWritten = 0;
    }
    else
    {
        sSizeToRequest = (aAppBuffer->mBufferSize == 0) ? 0 : ((aAppBuffer->mBufferSize / ACI_SIZEOF(ulWChar)) - 1) / 2;

        sSizeToRequest = ACP_MIN(sSizeToRequest, sLob->mSize - aColumn->mGDPosition);

        ACI_TEST(sLob->mOp->mGetData(aFnContext,
                                     sPtContext,
                                     sLob,
                                     &sLobBuffer,
                                     aColumn->mGDPosition,
                                     sSizeToRequest) != ACI_SUCCESS);

        /*
         * null terminating
         *
         * ulnConvDumpAsChar �� �����ϴ� ���� ������ ¦���̸�,
         * �׻� null termination �� ���� ������ ���� �� ���̸� �����Ѵ�.
         *
         * ��,
         *
         *      *(destination buffer + sLengthConverted) = 0;
         *
         * �� ���� �� �͹̳��̼��� �����ϰ� �� �� �ִ�.
         *
         * �׸���, sLob �� mSizeRetrieved �� ������ ������ �������� ���ŵ� ��������
         * ����Ʈ���� �����̸�, ulnConvDumpAsChar() �� ������ ���� 2 �� ���� ���̴�.
         */

        *(ulWChar*)(sLobBuffer.mObject.mMemory.mBuffer + (sLob->mSizeRetrieved * ACI_SIZEOF(ulWChar) * 2)) = 0;

        /*
         * Note : Null Termination ���̸� �����ְų� �ϸ� �ȵȴ�.
         *        �װ��� �Ʒ��� null terminate �Լ����� �ϰ������� ó���Ѵ�.
         */
        aLength->mWritten = sLob->mSizeRetrieved * ACI_SIZEOF(ulWChar) * 2;
    }

    aLength->mNeeded  = (sLob->mSize * 2 - aColumn->mGDPosition * 2) * ACI_SIZEOF(ulWChar);

    if (sLob->mSizeRetrieved > 0)
    {
        aColumn->mGDPosition += sLob->mSizeRetrieved;
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

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
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
                         "ulncBLOB_CHAR");
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

/* 
 * PROJ-2047 Strengthening LOB - Partial Converting
 */
ACI_RC ulncCLOB_WCHAR(ulnFnContext  *aFnContext,
                      ulnAppBuffer  *aAppBuffer,
                      ulnColumn     *aColumn,
                      ulnLengthPair *aLength,
                      acp_uint16_t   aRowNumber)
{
    ulnPtContext *sPtContext  = (ulnPtContext *)aFnContext->mArgs;
    ulnDbc       *sDbc;
    ulnLob       *sLob        = (ulnLob *)aColumn->mBuffer;
    ulnLobBuffer  sLobBuffer;
    acp_uint32_t  sSizeToRequest = 0;

    ulnCharSet   *sCharSet = NULL;
    acp_uint32_t  sHeadSize = 0;
    acp_uint32_t  sRemainAppBufferSize = 0;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    if (aAppBuffer->mBuffer == NULL ||
        aAppBuffer->mBufferSize <= ACI_SIZEOF(ulWChar))
    {
        sRemainAppBufferSize = 0;
    }
    else
    {
        sRemainAppBufferSize = aAppBuffer->mBufferSize - ACI_SIZEOF(ulWChar);

        if (sRemainAppBufferSize % ACI_SIZEOF(ulWChar) == 1)
        {
            sRemainAppBufferSize -= 1;
        }
        else
        {
            /* Nothing */
        }
    }

    if (aColumn->mGDPosition == 0)
    {
        aColumn->mRemainTextLen = 0;
        ACI_RAISE(REQUEST_TO_SERVER);
    }
    else
    {
        /* Nothing */
    }

    /* ������ ���� ���ߴ� �����͸� ������ ���� */
    if (aColumn->mRemainTextLen > 0) 
    {
        sHeadSize = ACP_MIN(sRemainAppBufferSize, aColumn->mRemainTextLen);
        acpMemCpy(aAppBuffer->mBuffer, aColumn->mRemainText, sHeadSize);

        aColumn->mRemainTextLen -= sHeadSize;
        sRemainAppBufferSize    -= sHeadSize;
 
        /*
         * ����� ���ۿ� ������ ���� �����͸� ���� ������ �����ϰų�
         * ����� ���۰� �� á�ٸ� ������ �����͸� ��û�� �ʿ䰡 ����. 
         */
        if (aColumn->mRemainTextLen > 0)
        {
            /* aAppBuffer->mBuffer�� ����� ������ �������� */
            acpMemCpy(aColumn->mRemainText,
                      aColumn->mRemainText + sHeadSize,
                      aColumn->mRemainTextLen);
        }
        else
        {
            /* Nothing */
        }
    }
    else
    {
        /* Nothing */
    }

    ACI_EXCEPTION_CONT(REQUEST_TO_SERVER);

    // BUG-27515: ����� ���۸� ��� ä�� �� ���� ������ ����� ���´�.
    sSizeToRequest = sRemainAppBufferSize *
                     sDbc->mCharsetLangModule->maxPrecision(1);

    sSizeToRequest = ACP_MIN(sSizeToRequest, sLob->mSize - aColumn->mGDPosition);

    /*
     * ulnLobBuffer �غ�
     */
    ACI_TEST(ulnLobBufferInitialize(&sLobBuffer,
                                    NULL,
                                    sLob->mType,
                                    ULN_CTYPE_WCHAR,
                                    aAppBuffer->mBuffer + sHeadSize,
                                    sRemainAppBufferSize) != ACI_SUCCESS);

    ACI_TEST(sLobBuffer.mOp->mPrepare(aFnContext, &sLobBuffer) != ACI_SUCCESS);

    /*
     * open LOB �� get data
     */
    ACI_TEST(sLob->mOp->mOpen(aFnContext, sPtContext, sLob) != ACI_SUCCESS);

    ACI_TEST(sLob->mOp->mGetData(aFnContext,
                                 sPtContext,
                                 sLob,
                                 &sLobBuffer,
                                 aColumn->mGDPosition,
                                 sSizeToRequest) != ACI_SUCCESS);

    sCharSet = &sLobBuffer.mCharSet;

    /*
     * sCharSet->mRemainTextLen, aColumn->mRemainTextLen ���� �ϳ��� 0�̴�.
     */
    aLength->mNeeded      = sCharSet->mDestLen + sHeadSize +
                            sCharSet->mRemainTextLen + aColumn->mRemainTextLen;
    aLength->mWritten     = sCharSet->mCopiedDesLen + sHeadSize;
    aColumn->mGDPosition += sCharSet->mConvedSrcLen;

    if (sCharSet->mRemainTextLen > 0)
    {
        aColumn->mRemainTextLen = sCharSet->mRemainTextLen;
        acpMemCpy(aColumn->mRemainText,
                  sCharSet->mRemainText,
                  sCharSet->mRemainTextLen);
    }
    else
    {
        /* Nothing */
    }

    /*
     * ulnLobBuffer ����
     */
    ACI_TEST(sLobBuffer.mOp->mFinalize(aFnContext, &sLobBuffer) != ACI_SUCCESS);

    // BUG-27515: ���� ���̸� ��Ȯ�ϰ� ����� �� ������ SQL_NO_TOTAL ��ȯ
    // ����: ODBC.NET������ ó���� SQL_NO_TOTAL�� ������ ������ ����Ÿ��
    // �������� ��ȿ�� ���̸� ���� �� �ִٰ� �����. �׷��Ƿ� ����Ÿ��
    // �� �޾ƿ°� �ƴϸ� �ùٸ� ���̸� ����� �� �ִ��� SQL_NO_TOTAL�� ����Ѵ�.
    if ((aColumn->mGDPosition < sLob->mSize) || (aColumn->mRemainTextLen > 0))
    {
        aLength->mNeeded = SQL_NO_TOTAL;
    }

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncNCHAR_WCHAR(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    ulnDbc* sDbc;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    aLength->mNeeded = aColumn->mDataLength - aColumn->mGDPosition;

    ACI_TEST(ulnConvCopyStr(aFnContext,
                            sDbc->mNcharCharsetLangModule,
                            sDbc->mWcharCharsetLangModule,
                            aAppBuffer,
                            aColumn,
                            (acp_char_t *)aColumn->mBuffer,
                            aColumn->mDataLength,
                            aLength) != ACI_SUCCESS);

    ACI_TEST(ulnConvWCharNullTerminate(aFnContext,
                                       aAppBuffer,
                                       aLength->mNeeded,
                                       aColumn->mColumnNumber,
                                       aRowNumber) != ACI_SUCCESS);
    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncNVARCHAR_WCHAR(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    return ulncNCHAR_WCHAR(aFnContext,
                           aAppBuffer,
                           aColumn,
                           aLength,
                           aRowNumber);
}
