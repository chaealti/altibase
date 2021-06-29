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

#ifndef _O_ULN_CONV_DECIMAL_H_
#define _O_ULN_CONV_DECIMAL_H_ 1

#include <cmtNumericClient.h>

/*
 * ������ ���޸��� �ִ� ������ 38�ڸ����� �����ϱ� ������,
 * ��, precision�� max �� 38 �̹Ƿ� 38�� ����.
 */
#define ULNC_NUMERIC_ALLOCSIZE 38
#define ULNC_DECIMAL_ALLOCSIZE 256
#define ULN_CM_NUMERIC_BASE    256
#define ULN_NUMERIC_BUFFER_SIZE 20  // MMC_CONV_NUMERIC_BUFFER_SIZE

typedef struct ulncNumeric ulncNumeric;

typedef enum
{
    ULNC_ENDIAN_BIG,
    ULNC_ENDIAN_LITTLE
} ulncEndian;

struct ulncNumeric
{
    acp_uint32_t  mAllocSize;
    acp_sint32_t  mBase;
    acp_uint32_t  mSize;

    acp_sint32_t  mPrecision;
    acp_sint32_t  mScale;

    ulncEndian   mEndian;

    ACI_RC       (*add)     (ulncNumeric *aNumeric, acp_uint32_t aValue);
    ACI_RC       (*multiply)(ulncNumeric *aNumeric, acp_uint32_t aValue);
    ACI_RC       (*devide)  (ulncNumeric *aNumeric, acp_uint32_t aValue);

    acp_uint32_t  mSign;
    acp_uint8_t  *mBuffer;
};

void ulncNumericInitialize(ulncNumeric  *aNumeric,
                           acp_sint32_t  aBase,
                           ulncEndian    aEndian,
                           acp_uint8_t  *aBuffer,
                           acp_uint32_t  aBufferSize);

void ulncNumericInitFromData(ulncNumeric *aNumeric,
                             acp_sint32_t aBase,
                             ulncEndian   aEndian,
                             acp_uint8_t *aBuffer,
                             acp_uint32_t aBufferSize,
                             acp_uint32_t aMantissaLen);

void ulncDecimalPrint(ulncNumeric   *aDecimal,
                      acp_char_t    *aBuffer,
                      acp_uint32_t   aBufferSize,
                      acp_uint32_t   aStartingPositionInSource,
                      ulnLengthPair *aLength);

void ulncDecimalPrintW(ulncNumeric   *aDecimal,
                       ulWChar       *aBuffer,
                       acp_uint32_t   aBufferSize,
                       acp_uint32_t   aStartingPositionInSource,
                       ulnLengthPair *aLength);

acp_float_t  ulncCmtNumericToFloat(cmtNumeric *aNumeric);
acp_double_t ulncCmtNumericToDouble(cmtNumeric *aNumeric);
ACI_RC       ulncCmtNumericToDecimal(cmtNumeric *aCmNumeric, ulncNumeric *aDecimal);

acp_uint64_t ulncDecimalToULong(ulncNumeric *aDecimal, acp_bool_t *aIsOverFlow);
acp_uint64_t ulncDecimalToSLong(ulncNumeric *aDecimal, acp_bool_t *aIsOverFlow);

void   ulncSLongToSQLNUMERIC(acp_sint64_t aLongValue, SQL_NUMERIC_STRUCT *aNumeric);

ACI_RC ulncNumericToNumeric(ulncNumeric *aDst, ulncNumeric *aSrc);

/* PROJ-2160 CM Ÿ������
   MT Ÿ���� numeric ����Ÿ�� CM Ÿ���� numeric ����Ÿ�� �����Ѵ�.
   CM Ÿ������ �����ϴ� ������ 2������ �ִ�.
   1. C Ÿ���� numeric �� CM Ÿ���� numeric �� ������
   2. MT Ÿ���� numeric�� �����Ҽ� �ִ� �Լ��� UL�� ����  */
ACI_RC ulncMtNumericToCmNumeric(cmtNumeric *aCmNumeric, mtdNumericType *aData);

typedef enum
{
    ULNC_SUCCESS,
    ULNC_INVALID_LITERAL,
    ULNC_VALUE_OVERFLOW,
    ULNC_SCALE_OVERFLOW
} ulncConvResult;

ulncConvResult ulncCharToNumeric(ulncNumeric       *aNumeric,
                                 acp_uint32_t       aMaximumMantissa,
                                 const acp_uint8_t *aString,
                                 acp_uint32_t       aLength);

ACI_RC ulncShiftLeft(ulncNumeric *aNumeric);

void debug_dump_numeric(ulncNumeric *aNumeric);

// ulncConvResult ulncDecimalToSQLNUMERIC(ulncDecimal *aDecimal, SQL_NUMERIC_STRUCT *aNumeric);

#endif /* _O_ULN_CONV_DECIMAL_H_ */
