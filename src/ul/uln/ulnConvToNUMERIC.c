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
#include <ulnConvBit.h>
#include <ulnConvNumeric.h>
#include <ulnConvToNUMERIC.h>

void debug_dump_numeric(ulncNumeric *aNumeric)
{
    acp_uint32_t i;

    acpPrintf("mBase      = %d\n", aNumeric->mBase);
    acpPrintf("mSize      = %d\n", aNumeric->mSize);
    acpPrintf("mPrecision = %d\n", aNumeric->mPrecision);
    acpPrintf("mScale     = %d\n", aNumeric->mScale);
    acpPrintf("mSign      = %d\n", aNumeric->mSign);
    acpPrintf("mBuffer    = ");

    for(i = 0; i < aNumeric->mAllocSize; i++)
    {
        acpPrintf("%02X ", aNumeric->mBuffer[i]);
    }

    acpPrintf("\n");
}

ACI_RC ulncCHAR_NUMERIC(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    ulncNumeric         sNumeric;
    SQL_NUMERIC_STRUCT *sSqlNumeric;

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;

    ulncNumericInitialize(&sNumeric,
                          256,
                          ULNC_ENDIAN_LITTLE,
                          sSqlNumeric->val,
                          SQL_MAX_NUMERIC_LEN);

    switch(ulncCharToNumeric(&sNumeric,
                             ULNC_NUMERIC_ALLOCSIZE,
                             (const acp_uint8_t *)aColumn->mBuffer,
                             aColumn->mDataLength))
    {
        case ULNC_INVALID_LITERAL:
            ACI_RAISE(LABEL_INVALID_LITERAL);
            break;

        case ULNC_VALUE_OVERFLOW:
            ACI_RAISE(LABEL_OUT_OF_RANGE);
            break;

        case ULNC_SCALE_OVERFLOW:
            ACI_RAISE(LABEL_SCALE_OUT_OF_RANGE);
            break;

        case ULNC_SUCCESS:
            break;
    }

    ACI_TEST_RAISE(sNumeric.mSize > SQL_MAX_NUMERIC_LEN, LABEL_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale < ACP_SINT8_MIN, LABEL_SCALE_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale > ACP_SINT8_MAX, LABEL_SCALE_OUT_OF_RANGE);

    sSqlNumeric->precision = sNumeric.mPrecision;
    sSqlNumeric->scale     = sNumeric.mScale;
    sSqlNumeric->sign      = sNumeric.mSign;

    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LITERAL)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_INVALID_NUMERIC_LITERAL);
    }

    ACI_EXCEPTION(LABEL_OUT_OF_RANGE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_VALUE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION(LABEL_SCALE_OUT_OF_RANGE)
    {
        /*
         * 22003
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_SCALE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncVARCHAR_NUMERIC(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    return ulncCHAR_NUMERIC(aFnContext,
                            aAppBuffer,
                            aColumn,
                            aLength,
                            aRowNumber);
}

ACI_RC ulncVARBIT_NUMERIC(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    return ulncBIT_NUMERIC(aFnContext,
                           aAppBuffer,
                           aColumn,
                           aLength,
                           aRowNumber);
}

ACI_RC ulncBIT_NUMERIC(ulnFnContext  *aFnContext,
                       ulnAppBuffer  *aAppBuffer,
                       ulnColumn     *aColumn,
                       ulnLengthPair *aLength,
                       acp_uint16_t   aRowNumber)
{
    acp_uint8_t         sBitValue;
    SQL_NUMERIC_STRUCT *sSqlNumeric;

    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aRowNumber);

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sBitValue   = ulnConvBitToUChar(aColumn->mBuffer);

    sSqlNumeric->precision = 1;
    sSqlNumeric->scale     = 0;
    sSqlNumeric->sign      = 1;
    acpMemSet(sSqlNumeric->val, 0, SQL_MAX_NUMERIC_LEN);
    sSqlNumeric->val[0]    = sBitValue;

    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;
}

ACI_RC ulncSMALLINT_NUMERIC(ulnFnContext  *aFnContext,
                            ulnAppBuffer  *aAppBuffer,
                            ulnColumn     *aColumn,
                            ulnLengthPair *aLength,
                            acp_uint16_t   aRowNumber)
{
    acp_sint16_t        sShortValue;
    SQL_NUMERIC_STRUCT *sSqlNumeric;

    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aRowNumber);

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sShortValue = *(acp_sint16_t *)aColumn->mBuffer;

    ulncSLongToSQLNUMERIC(sShortValue, sSqlNumeric);

    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;
}

ACI_RC ulncINTEGER_NUMERIC(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    acp_sint32_t        sIntValue;
    SQL_NUMERIC_STRUCT *sSqlNumeric;

    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aRowNumber);

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sIntValue   = *(acp_sint32_t *)aColumn->mBuffer;

    ulncSLongToSQLNUMERIC(sIntValue, sSqlNumeric);

    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;
}

ACI_RC ulncBIGINT_NUMERIC(ulnFnContext  *aFnContext,
                          ulnAppBuffer  *aAppBuffer,
                          ulnColumn     *aColumn,
                          ulnLengthPair *aLength,
                          acp_uint16_t   aRowNumber)
{
    acp_sint64_t        sBigIntValue;
    SQL_NUMERIC_STRUCT *sSqlNumeric;

    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aRowNumber);

    sSqlNumeric  = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sBigIntValue = *(acp_sint64_t *)aColumn->mBuffer;

    ulncSLongToSQLNUMERIC(sBigIntValue, sSqlNumeric);

    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;
}

ACI_RC ulncREAL_NUMERIC(ulnFnContext  *aFnContext,
                        ulnAppBuffer  *aAppBuffer,
                        ulnColumn     *aColumn,
                        ulnLengthPair *aLength,
                        acp_uint16_t   aRowNumber)
{
    acp_float_t         sRealValue;
    ulncNumeric         sNumeric;
    SQL_NUMERIC_STRUCT *sSqlNumeric;


    /*
     * single precision floating point number �� 23(24) ��Ʈ�� significand �� ���´�.
     * �ִ� ���밪�� 16777216 �ν�, 8�ڸ��̴�. ���⿡ �Ҽ��� ���� 7�ڸ�, �Ҽ���, ��ȣ, 
     * null term ���� ���ϸ� 8 + 7 + 1 + 1 + 1 = 18 �ڸ��� �ʿ��ϴ�.
     * �׳� �˳��� 20����Ʈ�� �Ҵ�����.
     */
    acp_char_t          sTmpBuffer[20];

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sRealValue  = *(acp_float_t *)aColumn->mBuffer;

    ulncNumericInitialize(&sNumeric,
                          256,
                          ULNC_ENDIAN_LITTLE,
                          sSqlNumeric->val,
                          SQL_MAX_NUMERIC_LEN);

    acpSnprintf(sTmpBuffer, 20, "%"ACI_FLOAT_G_FMT, sRealValue);

    switch(ulncCharToNumeric(&sNumeric,
                             ULNC_NUMERIC_ALLOCSIZE,
                             (const acp_uint8_t *)sTmpBuffer,
                             acpCStrLen(sTmpBuffer, 20)))
    {
        case ULNC_INVALID_LITERAL:
            ACI_RAISE(LABEL_INVALID_LITERAL);
            break;

        case ULNC_VALUE_OVERFLOW:
            ACI_RAISE(LABEL_OUT_OF_RANGE);
            break;

        case ULNC_SCALE_OVERFLOW:
            ACI_RAISE(LABEL_SCALE_OUT_OF_RANGE);
            break;

        case ULNC_SUCCESS:
            break;
    }

    ACI_TEST_RAISE(sNumeric.mSize > SQL_MAX_NUMERIC_LEN, LABEL_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale < ACP_SINT8_MIN, LABEL_SCALE_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale > ACP_SINT8_MAX, LABEL_SCALE_OUT_OF_RANGE);

    sSqlNumeric->precision = sNumeric.mPrecision;
    sSqlNumeric->scale     = sNumeric.mScale;
    sSqlNumeric->sign      = sNumeric.mSign;

    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LITERAL)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_INVALID_NUMERIC_LITERAL);
    }

    ACI_EXCEPTION(LABEL_OUT_OF_RANGE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_VALUE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION(LABEL_SCALE_OUT_OF_RANGE)
    {
        /*
         * 22003
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_SCALE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncDOUBLE_NUMERIC(ulnFnContext  *aFnContext,
                            ulnAppBuffer  *aAppBuffer,
                            ulnColumn     *aColumn,
                            ulnLengthPair *aLength,
                            acp_uint16_t   aRowNumber)
{
    acp_double_t        sDoubleValue;
    ulncNumeric         sNumeric;
    SQL_NUMERIC_STRUCT *sSqlNumeric;


    /*
     * double precision floating point number �� 52(53) ��Ʈ�� significand �� ���´�.
     * �ִ� ���밪�� 9007199254740992 �ν�, 16�ڸ��̴�. ���⿡ �Ҽ��� ���� 15�ڸ�, �Ҽ���, ��ȣ, 
     * null term ���� ���ϸ� 16 + 15 + 1 + 1 + 1 = 34 �ڸ��� �ʿ��ϴ�.
     * �׳� �˳��� 40����Ʈ�� �Ҵ�����.
     */
    acp_char_t           sTmpBuffer[40];

    sSqlNumeric  = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;
    sDoubleValue = *(acp_double_t *)aColumn->mBuffer;

    ulncNumericInitialize(&sNumeric,
                          256,
                          ULNC_ENDIAN_LITTLE,
                          sSqlNumeric->val,
                          SQL_MAX_NUMERIC_LEN);

    acpSnprintf(sTmpBuffer, 40, "%"ACI_DOUBLE_G_FMT, sDoubleValue);

    switch(ulncCharToNumeric(&sNumeric,
                             ULNC_NUMERIC_ALLOCSIZE,
                             (const acp_uint8_t *)sTmpBuffer,
                             acpCStrLen(sTmpBuffer, 40)))
    {
        case ULNC_INVALID_LITERAL:
            ACI_RAISE(LABEL_INVALID_LITERAL);
            break;

        case ULNC_VALUE_OVERFLOW:
            ACI_RAISE(LABEL_OUT_OF_RANGE);
            break;

        case ULNC_SCALE_OVERFLOW:
            ACI_RAISE(LABEL_SCALE_OUT_OF_RANGE);
            break;

        case ULNC_SUCCESS:
            break;
    }

    ACI_TEST_RAISE(sNumeric.mSize > SQL_MAX_NUMERIC_LEN, LABEL_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale < ACP_SINT8_MIN, LABEL_SCALE_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale > ACP_SINT8_MAX, LABEL_SCALE_OUT_OF_RANGE);

    sSqlNumeric->precision = sNumeric.mPrecision;
    sSqlNumeric->scale     = sNumeric.mScale;
    sSqlNumeric->sign      = sNumeric.mSign;

    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_INVALID_LITERAL)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_INVALID_NUMERIC_LITERAL);
    }

    ACI_EXCEPTION(LABEL_OUT_OF_RANGE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_VALUE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION(LABEL_SCALE_OUT_OF_RANGE)
    {
        /*
         * 22003
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_SCALE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncINTERVAL_NUMERIC(ulnFnContext  *aFnContext,
                            ulnAppBuffer  *aAppBuffer,
                            ulnColumn     *aColumn,
                            ulnLengthPair *aLength,
                            acp_uint16_t   aRowNumber)
{
    ACP_UNUSED(aFnContext);
    ACP_UNUSED(aAppBuffer);
    ACP_UNUSED(aColumn);
    ACP_UNUSED(aLength);
    ACP_UNUSED(aRowNumber);

    return ACI_SUCCESS;
}

ACI_RC ulncNUMERIC_NUMERIC(ulnFnContext  *aFnContext,
                           ulnAppBuffer  *aAppBuffer,
                           ulnColumn     *aColumn,
                           ulnLengthPair *aLength,
                           acp_uint16_t   aRowNumber)
{
    cmtNumeric         *sCmNumeric;
    SQL_NUMERIC_STRUCT *sSqlNumeric;
    ulnStmt            *sStmt = aFnContext->mHandle.mStmt;
    ulnDesc            *sDescARD;
    ulnDescRec         *sDescRec;
    acp_sint16_t        sUserPrecision;
    acp_sint16_t        sValuePrecision;
    acp_sint16_t        sUserScale;
    acp_sint16_t        sScaleDiff;
    ulncNumeric         sTmpNum;
    acp_uint8_t         sTmpNumBuf[SQL_MAX_NUMERIC_LEN];
    acp_uint32_t        sTmpNumMantissaLen;
    acp_sint32_t        i;

    sCmNumeric  = (cmtNumeric *)aColumn->mBuffer;
    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;

    /*
     * BUGBUG : cmtNumeric �� scale �� acp_sint16_t �ε�, SQL_NUMERIC_STRUCT �� scale �� acp_char_t �̴�.
     *          ������ �����÷ο찡 ���� ��� �ϳ�?
     *          fractional truncation �� �����ְ�, ������ �����ؾ� ����������,
     *          �ϴ�, �׳� ������ ���� �𸣴�ü ���� -_-;
     *
     *          �׷���, � ��쿡 ������ �������� Ŀ����?
     *
     *          e-90 �� numeric scale overflow ���� �Ѱ�����.
     *          e-90 OK
     *          e-91 Overflow
     *
     *          INSERT INTO T1 VALUES('-1.2345678901234567890123456789012345678e-90',
     *                                '-1.2345678901234567890123456789012345678e+125');
     */

    ACI_TEST_RAISE(sCmNumeric->mScale < ACP_SINT8_MIN, LABEL_SCALE_OUT_OF_RANGE);
    ACI_TEST_RAISE(sCmNumeric->mScale > ACP_SINT8_MAX, LABEL_SCALE_OUT_OF_RANGE);

    sSqlNumeric->sign      = sCmNumeric->mSign;
    sSqlNumeric->precision = sCmNumeric->mPrecision;
    sSqlNumeric->scale     = sCmNumeric->mScale;
    acpMemCpy(sSqlNumeric->val, sCmNumeric->mData, SQL_MAX_NUMERIC_LEN);

    /* BUG-47384 User Type�� SQL_C_BINARY�� ��� user precision, scale�� �����Ѵ�. */
    ACI_TEST_RAISE( aAppBuffer->mCTYPE == ULN_CTYPE_BINARY, SKIP_PRECSCALE_CONV );
    /* BUG-37101 */
    sDescARD = ulnStmtGetArd(sStmt);
    ACI_TEST_RAISE(sDescARD == NULL, SKIP_PRECSCALE_CONV); /* BUG-37256 */
    sDescRec = ulnDescGetDescRec(sDescARD, aColumn->mColumnNumber);
    ACI_TEST_RAISE(sDescRec == NULL, SKIP_PRECSCALE_CONV); /* BUG-45453 */
    /* user precision, scale�� �°� ��ȯ */
    {
        sUserPrecision = ulnMetaGetPrecision(&sDescRec->mMeta);
        sUserScale = ulnMetaGetScale(&sDescRec->mMeta);

        if ((sUserScale != ULN_NUMERIC_UNDEF_SCALE)
         && (sUserScale != sSqlNumeric->scale))
        {
            sScaleDiff = sUserScale - sSqlNumeric->scale;
            sTmpNumMantissaLen = SQL_MAX_NUMERIC_LEN;
            for (i = SQL_MAX_NUMERIC_LEN - 1; i >= 0; i--)
            {
                if (sSqlNumeric->val[i] != 0)
                {
                    break;
                }
                sTmpNumMantissaLen--;
            }
            ulncNumericInitFromData(&sTmpNum,
                                    256,
                                    ULNC_ENDIAN_LITTLE,
                                    sSqlNumeric->val,
                                    SQL_MAX_NUMERIC_LEN,
                                    sTmpNumMantissaLen);

            /* user-scale�� value�� ©���� ���� �������� �Ѵ�. */
            if (sScaleDiff > 0)
            {
                for (; sScaleDiff != 0; sScaleDiff--)
                {
                    ACI_TEST_RAISE(sTmpNum.multiply(&sTmpNum, 10) != ACI_SUCCESS,
                                   LABEL_SCALE_OUT_OF_RANGE);
                }
            }
            else
            {
                for (; sScaleDiff != 0; sScaleDiff++)
                {
                    ACI_TEST_RAISE(sTmpNum.devide(&sTmpNum, 10) != ACI_SUCCESS,
                                   LABEL_SCALE_OUT_OF_RANGE);
                }
            }

            sSqlNumeric->scale = sUserScale;
        }

        if ((sUserPrecision != ULN_NUMERIC_UNDEF_PRECISION)
         || (sUserScale != ULN_NUMERIC_UNDEF_SCALE))
        {
            acpMemCpy(sTmpNumBuf, sSqlNumeric->val, SQL_MAX_NUMERIC_LEN);
            sTmpNumMantissaLen = SQL_MAX_NUMERIC_LEN;
            for (i = SQL_MAX_NUMERIC_LEN - 1; i >= 0; i--)
            {
                if (sTmpNumBuf[i] != 0)
                {
                    break;
                }
                sTmpNumMantissaLen--;
            }
            ulncNumericInitFromData(&sTmpNum,
                                    256,
                                    ULNC_ENDIAN_LITTLE,
                                    sTmpNumBuf,
                                    SQL_MAX_NUMERIC_LEN,
                                    sTmpNumMantissaLen);
            for (sValuePrecision = 0; sTmpNum.mSize > 0; sValuePrecision++)
            {
                (void) sTmpNum.devide(&sTmpNum, 10);
            }

            if (sUserPrecision == ULN_NUMERIC_UNDEF_PRECISION)
            {
                sUserPrecision = ULN_NUMERIC_MAX_PRECISION;
            }
            /* user-precision�� value�� �� ���� �� ���� �������� �Ѵ�. */
            ACI_TEST_RAISE(sValuePrecision > sUserPrecision,
                           LABEL_PRECISION_OUT_OF_RANGE);

            sSqlNumeric->precision = sUserPrecision;
        }
    }
    ACI_EXCEPTION_CONT(SKIP_PRECSCALE_CONV);

    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_PRECISION_OUT_OF_RANGE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_PRECISION_OUT_OF_RANGE);
    }
    ACI_EXCEPTION(LABEL_SCALE_OUT_OF_RANGE)
    {
        /*
         * 22003
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_SCALE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulncNCHAR_NUMERIC(ulnFnContext  *aFnContext,
                         ulnAppBuffer  *aAppBuffer,
                         ulnColumn     *aColumn,
                         ulnLengthPair *aLength,
                         acp_uint16_t   aRowNumber)
{
    ulncNumeric         sNumeric;
    SQL_NUMERIC_STRUCT *sSqlNumeric;
    ulnDbc             *sDbc;
    ulnCharSet          sCharSet;
    acp_sint32_t        sState = 0;

    ulnCharSetInitialize(&sCharSet);
    sState = 1;

    ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    ACI_TEST(ulnCharSetConvert(&sCharSet,
                               aFnContext,
                               NULL,
                               (const mtlModule*)sDbc->mNcharCharsetLangModule,
                               sDbc->mClientCharsetLangModule,    //BUG-22684
                               (void*)aColumn->mBuffer,
                               aColumn->mDataLength,
                               CONV_DATA_OUT)
             != ACI_SUCCESS);

    sSqlNumeric = (SQL_NUMERIC_STRUCT *)aAppBuffer->mBuffer;

    ulncNumericInitialize(&sNumeric,
                          256,
                          ULNC_ENDIAN_LITTLE,
                          sSqlNumeric->val,
                          SQL_MAX_NUMERIC_LEN);

    switch(ulncCharToNumeric(&sNumeric,
                             ULNC_NUMERIC_ALLOCSIZE,
                             (const acp_uint8_t *)ulnCharSetGetConvertedText(&sCharSet),
                             ulnCharSetGetConvertedTextLen(&sCharSet)))
    {
        case ULNC_INVALID_LITERAL:
            ACI_RAISE(LABEL_INVALID_LITERAL);
            break;

        case ULNC_VALUE_OVERFLOW:
            ACI_RAISE(LABEL_OUT_OF_RANGE);
            break;

        case ULNC_SCALE_OVERFLOW:
            ACI_RAISE(LABEL_SCALE_OUT_OF_RANGE);
            break;

        case ULNC_SUCCESS:
            break;
    }

    ACI_TEST_RAISE(sNumeric.mSize > SQL_MAX_NUMERIC_LEN, LABEL_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale < ACP_SINT8_MIN, LABEL_SCALE_OUT_OF_RANGE);
    ACI_TEST_RAISE(sNumeric.mScale > ACP_SINT8_MAX, LABEL_SCALE_OUT_OF_RANGE);

    sSqlNumeric->precision = sNumeric.mPrecision;
    sSqlNumeric->scale     = sNumeric.mScale;
    sSqlNumeric->sign      = sNumeric.mSign;

    aLength->mWritten = ACI_SIZEOF(SQL_NUMERIC_STRUCT);
    aLength->mNeeded  = ACI_SIZEOF(SQL_NUMERIC_STRUCT);

    sState = 0;
    ulnCharSetFinalize(&sCharSet);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_INVALID_LITERAL)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_INVALID_NUMERIC_LITERAL);
    }

    ACI_EXCEPTION(LABEL_OUT_OF_RANGE)
    {
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_VALUE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION(LABEL_SCALE_OUT_OF_RANGE)
    {
        /*
         * 22003
         */
        ulnErrorExtended(aFnContext,
                         aRowNumber,
                         aColumn->mColumnNumber,
                         ulERR_ABORT_NUMERIC_SCALE_OUT_OF_RANGE);
    }

    ACI_EXCEPTION_END;

    if (sState == 1)
    {
        ulnCharSetFinalize(&sCharSet);
    }

    return ACI_FAILURE;
}

ACI_RC ulncNVARCHAR_NUMERIC(ulnFnContext  *aFnContext,
                            ulnAppBuffer  *aAppBuffer,
                            ulnColumn     *aColumn,
                            ulnLengthPair *aLength,
                            acp_uint16_t   aRowNumber)
{
    return ulncNCHAR_NUMERIC(aFnContext,
                             aAppBuffer,
                             aColumn,
                             aLength,
                             aRowNumber);
}
