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
extern utISPApi           * gISPApi;

isqlNumeric::isqlNumeric()
{
    mCType = SQL_C_CHAR;
}

IDE_RC isqlNumeric::initBuffer()
{
    mValueSize = FLOAT_SIZE + 1;

    mValue = (SChar *) idlOS::malloc(mValueSize);
    IDE_TEST(mValue == NULL);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

void isqlNumeric::initDisplaySize()
{
    SChar *sFmt = NULL;
    UChar *sNumToken = NULL;
    SInt sTmpSize = 0;
    SInt sDisplaySize = 0;

    /* BUG-34447 COLUMN col FORMAT fmt */
    sFmt = gProperty.GetColumnFormat(
            mName,
            FMT_NUM,   /* BUG-43351 */
            gISPApi->GetNlsCurrency(),
            &sTmpSize,
            &sNumToken);
    if ( sFmt != NULL )
    {
        SetColumnFormat( sFmt, sNumToken );
        sDisplaySize = sTmpSize;
    }
    else
    {
        /* BUG-34447 SET NUMFORMAT */
        sFmt = gProperty.GetNumFormat(
                gISPApi->GetNlsCurrency(),
                &sTmpSize,
                &sNumToken); 
        if ( sFmt[0] != '\0' )
        {
            SetColumnFormat( sFmt, sNumToken );
            sDisplaySize = sTmpSize;
        }
        else
        {
            /* BUG-39213 Need to support SET NUMWIDTH in isql */
            sDisplaySize = gProperty.GetNumWidth();
            gISPApi->SetNumWidth(sDisplaySize);
        }
    }
    mDisplaySize = sDisplaySize;
}

void isqlNumeric::Reformat()
{
    SChar sDst[FLOAT_SIZE + 1];

    if ( mInd != SQL_NULL_DATA )
    {
        /* BUG-34447 COL col FOR[MAT] fmt */
        /* BUG-34447 SET NUMF[ORMAT] fmt */
        if ( mFmt != NULL )
        {
            isqlFloat::ToChar( sDst,
                               (SChar *)mValue,
                               idlOS::strlen((const SChar *)mValue),
                               mFmt,
                               idlOS::strlen(mFmt),
                               mFmtToken,
                               gISPApi->GetNlsCurrency() );
            idlOS::strcpy((SChar *)mValue, sDst);
        }
        else
        {
            ReformatNumber((SChar *)mValue,
                           &mInd,
                           gISPApi->GetNumWidth());
        }
    }
    else
    {
        mValue[0] = '\0';
    }
    mCurr = mValue;
    mCurrLen = idlOS::strlen(mCurr);
}

/**
 * ReformatNumber.
 *
 * SELECT ������ ����� �����
 * NUMERIC, DECIMAL, NUMBER, FLOAT�� �÷��� ����
 * ���ڷ� ���� ������� ���� ���������Ѵ�.
 * SQL_NULL_DATA�� ��� �� �Լ��� ȣ���ؼ��� �ȵȴ�.
 *
 * @param[in,out] aCValue
 *  SELECT ������ ����� ����� �÷���.
 * @param[in,out] aLen
 *  aCValue�� ����.
 * @param[in] aWidth
 *  �����.
 */
void isqlNumeric::ReformatNumber( SChar  *aCValue,
                                  SQLLEN *aLen,
                                  SInt    aWidth )
{
    SChar  *sC;
    SInt    sExponent;
    SInt    sI;
    SInt    sNDigit;
    idBool  sCarriage;
    idBool  sNeg = ID_FALSE;

    /* ��ȣ�� �����Ѵ�. */
    if (aCValue[0] == '-')
    {
        sNeg = ID_TRUE;
        aCValue++;
        (*aLen)--;
        aWidth--;
    }

    /* ������ ���Ѵ�. */
    sExponent = GetExponent(aCValue);

    /* aCValue���� �Ҽ��� �� �����θ� �����ϰ�, ������ �ڸ����� �����. */
    sC = idlOS::strchr(aCValue, 'E');
    if (sC != NULL)
    {
        *sC = '\0';
        *aLen = (SQLLEN)(sC - aCValue);
    }
    sC = idlOS::strchr(aCValue, '.');
    if (sC != NULL)
    {
        idlOS::memmove(sC, sC + 1, *aLen - (SQLLEN)(sC - aCValue));
        (*aLen)--;
    }
    for (sC = aCValue; *sC == '0'; sC++) {};
    idlOS::memmove(aCValue, sC, *aLen - (SQLLEN)(sC - aCValue) + 1);
    *aLen -= (SQLLEN)(sC - aCValue);

    /* ���� ���� ����ϱ� ���� �ڸ����� ������ ����Ѵ�. */
    /* ������������ ǥ���ؾ� �ϴ� ��� */
    if (-(aWidth - 2) <= sExponent && sExponent <= aWidth - 2)
    {
        if (0 <= sExponent)
        {
            if (sExponent < aWidth - 2)
            {
                sNDigit = aWidth - 2;
            }
            else
            {
                sNDigit = aWidth - 1;
            }
        }
        else
        {
            sNDigit = aWidth + sExponent - 1;
        }
    }
    /* ���������� ǥ���ؾ� �ϴ� ��� */
    else
    {
        if (-9 <= sExponent && sExponent <= 9)
        {
            sNDigit = aWidth - 5;
        }
        else if (-99 <= sExponent && sExponent <= 99)
        {
            sNDigit = aWidth - 6;
        }
        else /* (-999 <= sExponent && sExponent <= 999) */
        {
            sNDigit = aWidth - 7;
        }
    }

    /* �ڸ����� �����. */
    /* �ݿø��� �ʿ� ���� ��� */
    if (*aLen <= (SQLLEN)sNDigit)
    {
        for (; *aLen < (SQLLEN)sNDigit; (*aLen)++)
        {
            aCValue[*aLen] = '0';
        }
        aCValue[*aLen] = '\0';
    }
    /* �ݿø��� �ʿ��� ��� */
    else
    {
        sCarriage = (aCValue[sNDigit] >= '5')? ID_TRUE: ID_FALSE;
        aCValue[sNDigit] = '\0';

        for (sI = sNDigit - 1; sCarriage && sI >= 0; sI--)
        {
            if (aCValue[sI] < '9')
            {
                aCValue[sI]++;
                sCarriage = ID_FALSE;
            }
            else
            {
                aCValue[sI] = '0';
            }
        }

        if (sCarriage == ID_TRUE)
        {
            idlOS::memmove(aCValue + 1, aCValue, sNDigit + 1);
            aCValue[0] = '1';
            sCarriage = ID_FALSE;
            sNDigit++;
            sExponent++;
        }

        *aLen = (SQLLEN)sNDigit;
    }

    /* ���ڸ� �����. */
    /* ������������ ǥ���ؾ� �ϴ� ��� */
    if (-(aWidth - 2) <= sExponent && sExponent <= aWidth - 2)
    {
        if (0 <= sExponent)
        {
            if (sExponent < aWidth - 3)
            {
                idlOS::memmove(aCValue + sExponent + 2,
                               aCValue + sExponent + 1,
                               aWidth - sExponent - 3);
                aCValue[sExponent + 1] = '.';
                aCValue[aWidth - 1] = '\0';
                *aLen = (SQLLEN)(aWidth - 1);

                /* �Ҽ��� ���� ���ʿ��� 0 ����. */
                for (; aCValue[*aLen - 1] == '0'; (*aLen)--) {};
                if (aCValue[*aLen - 1] == '.')
                {
                    (*aLen)--;
                }
                aCValue[*aLen] = '\0';
            }
            else
            {
                aCValue[sExponent + 1] = '\0';
                *aLen = (SQLLEN)(sExponent + 1);
            }
        }
        else
        {
            idlOS::memmove(aCValue - sExponent + 1, aCValue,
                           aWidth + sExponent - 1);
            aCValue[0] = '0';
            aCValue[1] = '.';
            if (sExponent < -1)
            {
                idlOS::memset(aCValue + 2, '0', -sExponent - 1);
            }
            aCValue[aWidth] = '\0';
            *aLen = (SQLLEN)aWidth;

            /* �Ҽ��� ���� ���ʿ��� 0 ����. */
            for (; aCValue[*aLen - 1] == '0'; (*aLen)--) {};
            aCValue[*aLen] = '\0';
        }
    }
    /* ���������� ǥ���ؾ� �ϴ� ��� */
    else
    {
        if (-9 <= sExponent && sExponent <= 9)
        {
            sNDigit = aWidth - 5;
        }
        else if (-99 <= sExponent && sExponent <= 99)
        {
            sNDigit = aWidth - 6;
        }
        else /* (-999 <= sExponent && sExponent <= 999) */
        {
            sNDigit = aWidth - 7;
        }
        idlOS::memmove(aCValue + 2, aCValue + 1, sNDigit - 1);
        aCValue[1] = '.';
        if (0 <= sExponent)
        {
            idlOS::sprintf(aCValue + sNDigit + 1, "E+%"ID_INT32_FMT,
                           sExponent);
        }
        else
        {
            idlOS::sprintf(aCValue + sNDigit + 1, "E%"ID_INT32_FMT, sExponent);
        }
        aCValue[aWidth - 1] = '\0';
        *aLen = (SQLLEN)(aWidth - 1);
    }

    /* �����ߴ� ��ȣ�� ���̿� ���Խ�Ŵ. */
    if (sNeg == ID_TRUE)
    {
        (*aLen)++;
    }
}

/**
 * GetExponent.
 *
 * ���ڷ� ���� ������ ������ ���� �����Ѵ�.
 *
 * @param[in] aCValue
 *  ������ ���� ����(���ڸ� ���ڿ��� ����� ����).
 */
SInt isqlNumeric::GetExponent( SChar *aCValue )
{
    SChar *sPos;
    SInt   sExponent;

    /* ��ȣ�� �����Ѵ�. */
    if (aCValue[0] == '-')
    {
        aCValue++;
    }

    sPos = idlOS::strchr(aCValue, 'E');
    /* ������ ǥ���� ��� */
    if (sPos != NULL)
    {
        sExponent = (SInt)idlOS::strtol(sPos + 1, NULL, 10);
    }
    /* �������� ǥ���� ��� */
    else
    {
        /* 1..9�� �����ϴ� ������ ��� */
        if (aCValue[0] != '0')
        {
            sPos = idlOS::strchr(aCValue + 1, '.');
            /* xxx.xxx */
            if (sPos != NULL)
            {
                sExponent = (SInt)(sPos - aCValue) - 1;
            }
            /* xxx */
            else
            {
                sExponent = idlOS::strlen(aCValue) - 1;
            }
        }
        /* 0���� �����ϴ� ������ ��� */
        else /* (aCValue[0] == '0') */
        {
            /* 0 */
            if (aCValue[1] == '\0')
            {
                sExponent = 0;
            }
            /* 0.xxx */
            else
            {
                /* �Ҽ��� ���Ͽ��� ���ʷ� 0�� �ƴ� �ڸ����� ã�´�. */
                for (sPos = &aCValue[2]; *sPos; sPos++)
                {
                    if (*sPos != '0')
                    {
                        break;
                    }
                }
                /* �Ҽ��� ���ϰ� ��� 0�� ��� */
                if (*sPos == '\0')
                {
                    sExponent = 0;
                }
                /* �Ҽ��� ���Ͽ��� 0�� �ƴ� �ڸ����� ã�� ��� */
                else
                {
                    sExponent = -((SInt)(sPos - aCValue) - 1);
                }
            }
        }
    }

    return sExponent;
}

SInt isqlNumeric::AppendToBuffer( SChar *aBuf, SInt *aBufLen )
{
    isqlType::AppendToBuffer(aBuf, aBufLen);

    return mCurrLen;
}

