/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/***********************************************************************
 * $Id$
 **********************************************************************/

#include <utISPApi.h>
#include <isqlTypes.h>

extern iSQLProgOption       gProgOption;

isqlBit::isqlBit()
{
    mCType = SQL_C_BINARY;
}

IDE_RC isqlBit::initBuffer()
{
    SInt sSize = 0;

    if ( mPrecision == 0 )
    {
        sSize = (SInt)ID_SIZEOF(UInt);
    }
    else
    {
        sSize = (SInt)(ID_SIZEOF(UInt) + (mPrecision - 1) / 8 + 1);
    }

    mValueSize = (sSize > mPrecision + 1)? sSize : mPrecision + 1;

    mValue = (SChar *)idlOS::malloc(mValueSize);
    IDE_TEST(mValue == NULL);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void isqlBit::initDisplaySize()
{
    mDisplaySize = mPrecision;
}

void isqlBit::Reformat()
{
    SChar  sDst[64000 + 1];
    SQLLEN sDstLen;

    if ( mInd != SQL_NULL_DATA )
    {
        ToChar((UChar *)mValue, sDst, &sDstLen);
        idlOS::strncpy(mValue, sDst, mPrecision);
        mValue[sDstLen] = '\0';
    }
    else
    {
        mValue[0] = '\0';
    }
    mCurr = mValue;
    mCurrLen = idlOS::strlen(mCurr);
}

SInt isqlBit::AppendToBuffer( SChar *aBuf, SInt *aBufLen )
{
    isqlType::AppendToBuffer(aBuf, aBufLen);

    aBuf[(*aBufLen)++] = ' ';
    aBuf[*aBufLen] = '\0';

    return 0;
}

SInt isqlBit::AppendAllToBuffer( SChar *aBuf )
{
    SInt sLen;

    sLen = isqlType::AppendAllToBuffer(aBuf);
    aBuf[sLen++] = ' ';
    aBuf[sLen] = '\0';

    return sLen;
}

/**
 * ConvBitToChar.
 *
 * SELECT ������ ����� ����� raw BIT �����͸�
 * ���� '0' �Ǵ� '1'�� ���ڿ� ���·� ���������Ѵ�.
 * SQL_NULL_DATA�� ��� �� �Լ��� ȣ���ؼ��� �ȵȴ�.
 *
 * @param[in] aRaw
 *  SELECT ������ ����� ����� raw BIT ������.
 * @param[out] aCVal
 *  ���� '0' �Ǵ� '1'�� ���ڿ� ���·� �������õ� BIT ������.
 * @param[out] aCValLen
 *  aCVal�� ����.
 */
void isqlBit::ToChar( UChar  *aRaw,
                      SChar  *aCVal,
                      SQLLEN *aCValLen )
{
    UChar *sBit;
    UInt   sI;
    UInt   sLen;

    /* Raw BIT �����ʹ� ���� 4����Ʈ�� UInt Ÿ���� ���̰� ��ġ�ϰ�,
     * �� �ڿ� ���� BIT �����Ͱ� ���̳ʸ� ���·� ����ȴ�. */
    idlOS::memcpy(&sLen, aRaw, ID_SIZEOF(UInt));
    sBit = aRaw + ID_SIZEOF(UInt);

    for ( sI = 0 ; sI < sLen ; sI++ )
    {
        if ( sBit[sI / 8] & (1 << (7 - sI % 8)) )
        {
            aCVal[sI] = '1';
        }
        else
        {
            aCVal[sI] = '0';
        }
    }
    aCVal[sI] = '\0';

    *aCValLen = (SQLLEN)sLen;
}

