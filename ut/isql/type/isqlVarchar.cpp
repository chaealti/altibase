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

extern iSQLProperty         gProperty;
extern iSQLProgOption       gProgOption;

isqlVarchar::isqlVarchar()
{
    mCType = SQL_C_CHAR;
}

IDE_RC isqlVarchar::initBuffer()
{
    // BUG-24085 �������� �Ͼ�� ����Ÿ�� �þ ��� ������ �߻��մϴ�.
    // ��Ȯ�� ����� �˱Ⱑ ����� ���� NCHAR �� ���� ó���Ѵ�.
    SInt sSize = (SInt)((mPrecision + 1) * 3);

    mValue = (SChar *) idlOS::malloc(sSize);
    IDE_TEST(mValue == NULL);

    mValueSize = sSize;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void isqlVarchar::initDisplaySize()
{
    SInt sTmpSize = 0;
    SInt sDisplaySize = 0;

    /* BUG-43336 Set colsize more than precision */
    sTmpSize = gProperty.GetCharSize(mName); /* COLUMN c1 FORMAT aN */
    if ( sTmpSize > 0 )
    {
        sDisplaySize = sTmpSize;
    }
    else
    {
        sTmpSize = gProperty.GetColSize(); /* SET COLSIZE N */
        if ( sTmpSize > 0 )
        {
            sDisplaySize = sTmpSize;
        }
        else
        {
            if ( mSqlType == SQL_CHAR || mSqlType == SQL_VARCHAR )
            {
                sDisplaySize = mPrecision;
            }
            else
            {
                sDisplaySize = mPrecision * 2;
            }
        }
    }
    if ( sDisplaySize > MAX_COL_SIZE )
    {
        sDisplaySize = MAX_COL_SIZE;
    }
    mDisplaySize = sDisplaySize;
}

SInt isqlVarchar::AppendToBuffer( SChar *aBuf, SInt *aBufLen )
{
    SInt  i;

    /* BUG-43911
     * char, varchar, bit, bytes Ÿ���� �ٸ� Ÿ�԰� �޸� �� ĭ �� ������,
     * ���� ȣȯ���� �����ϱ� ���� �״�� �д�. */
    for ( i = 0 ; i <= mDisplaySize ; i++ )
    {
        if ( mCurrLen > 0 )
        {
            if ( (i == mDisplaySize) && /* last char? */
                 ((aBuf[i-1] & 0x80) == 0) )
            {
                aBuf[i] = ' ';
            }
            /*
             * BUG-47325
             *   Line breaks occur at the wrong position
             *   when newline character included query results.
             */
            else if (*mCurr == '\r' && *(mCurr+1) == '\n')
            {
                mCurrLen = mCurrLen - 2;
                mCurr = mCurr + 2;
                break;
            }
            else if (*mCurr == '\n')
            {
                mCurrLen--;
                mCurr++;
                break;
            }
            else
            {
                aBuf[i] = *mCurr;
                mCurrLen--;
                mCurr++;
            }
        }
        else
        {
            aBuf[i] = ' ';
        }
    }
    aBuf[i] = '\0';
    *aBufLen = i;

    return mCurrLen;
}

SInt isqlVarchar::AppendAllToBuffer( SChar *aBuf )
{
    SInt sLen;

    sLen = isqlType::AppendAllToBuffer(aBuf);
    aBuf[sLen++] = ' ';
    aBuf[sLen] = '\0';

    return sLen;
}

