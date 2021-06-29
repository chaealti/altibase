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
 * $Id: isqlFloat.cpp 80544 2017-07-19 08:04:46Z daramix $
 **********************************************************************/

#include <utISPApi.h>

extern mtdModule mtcdFloat;
extern mtdModule mtcdVarchar;

IDE_RC isqlFloat::CheckFormat( UChar       * aFmt,
                               UInt          aFmtLen,
                               UChar       * aToken )
{
/***********************************************************************
 *
 * Description : SET NUMFORMAT fmt ����� fmt ����
 *               mtcfTo_char.c�� mtfToCharInterface_checkFormat ȣ��
 *
 * Implementation :
 *
 * @param[in]  aFmt : �Էµ� ����
 * @param[in]  aFmtLen : aFmt�� ���ڿ� ����
 * @param[out] aToken : �ٸ� �Լ����� ���� number format token
 *
***********************************************************************/
    IDE_TEST( mtfToCharInterface_checkFormat( aFmt,
                                              aFmtLen,
                                              aToken )
              != ACI_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC isqlFloat::ToChar( SChar       * aDst,
                          SChar       * aSrc,
                          SInt          aSrcLen,
                          SChar       * aFmt,
                          SInt          aFmtLen,
                          UChar       * aToken,
                          mtlCurrency * aCurrency )
{
/***********************************************************************
 *
 * Description : number �������� ǥ�� ���� ��ȯ
 *               mtcfTo_char.c�� mtfToCharInterface_mtfTo_char ȣ��
 *
 * Implementation :
 *
 * @param[out] aDst : ��ȯ�� ���ڿ�
 * @param[in]  aSrc : ���ڿ��� ǥ���� number ������
 * @param[in]  aSrcLen : aSrc�� ���ڿ� ����
 * @param[in]  aFmt : �Էµ� ����
 * @param[in]  aFmtLen : aFmt�� ���ڿ� ����
 * @param[in]  aToken : number format token
 * @param[in]  aCurrency : �ش� ������ NLS_ISO_CURRENCY, NLS_CURRENCY,
 *                         NLS_NUMERIC_CHARACTERS
 *
***********************************************************************/
    SInt             sColSize = 0;
    mtcColumn        sSrcColumn;
    mtcColumn        sDstColumn;
    mtcColumn        sFmtColumn;
    mtdNumericType * sSrcValue;
    mtdCharType    * sDstValue;
    mtdCharType    * sFmtValue;
    SChar            sSrcBuf[ID_SIZEOF(UInt) + FLOAT_SIZE]; // mtcCharType->len + isql�� FLOAT ������
    SChar            sDstBuf[ID_SIZEOF(UInt) + MTC_TO_CHAR_MAX_PRECISION];
    SChar            sFmtBuf[ID_SIZEOF(UInt) + WORD_LEN];
    mtcStack         sStack[3];

    aDst[0] = '\0';
    idlOS::memset( sSrcBuf, 0x00, ID_SIZEOF(sSrcBuf) );
    idlOS::memset( sDstBuf, 0x00, ID_SIZEOF(sDstBuf) );
    idlOS::memset( sFmtBuf, 0x00, ID_SIZEOF(sFmtBuf) );

    sSrcValue = (mtdNumericType*)sSrcBuf;
    sDstValue = (mtdCharType*)sDstBuf;
    sFmtValue = (mtdCharType*)sFmtBuf;

    IDE_TEST( mtcMakeNumeric( sSrcValue,
                              MTD_FLOAT_MANTISSA_MAXIMUM,
                              (const UChar*)aSrc,
                              aSrcLen )
              != ACI_SUCCESS );

    IDE_TEST( mtcInitializeColumn(
                &sSrcColumn,
                &mtcdFloat,
                0,
                0,
                0 )
            != ACI_SUCCESS );

    /* BUGBUG mtcInitializeColumn���� precision ������
     * �������� �����Ƿ� ������ ����.
     * mtcInitializeColumn�� ������ ���� ������ �Ǵ� �Ұ� */
    IDE_TEST( mtcInitializeColumn(
                &sDstColumn,
                &mtcdVarchar,
                1,
                MTC_TO_CHAR_MAX_PRECISION, /* mtfTo_char.cpp:2224 ���� */
                0 )
              != ACI_SUCCESS );
    IDE_TEST( mtcInitializeColumn(
                &sFmtColumn,
                &mtcdVarchar,
                1,
                aFmtLen,
                0 )
              != ACI_SUCCESS );

    sDstColumn.precision = FLOAT_SIZE;
    sFmtColumn.precision = aFmtLen;

    idlOS::memcpy( sFmtValue->value, aFmt, aFmtLen );
    sFmtValue->length = aFmtLen;

    sStack[0].column = &sDstColumn;
    sStack[0].value = sDstValue;
    sStack[1].column = &sSrcColumn;
    sStack[1].value = sSrcValue;
    sStack[2].column = &sFmtColumn;
    sStack[2].value = sFmtValue;

    IDE_TEST( mtfToCharInterface_mtfTo_char(sStack,
                                            aToken,
                                            aCurrency)
              != ACI_SUCCESS );

    idlOS::memcpy(aDst, sDstValue->value, sDstValue->length);
    aDst[sDstValue->length] = '\0';

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* ��ȯ ����(mtERR_ABORT_INVALID_LENGTH)�� �߻��ϸ� #���� ä���� */
    sColSize = GetColSize( aCurrency, aFmt, aToken ) - 1;
    idlOS::memset( aDst, '#', sColSize );
    aDst[sColSize] = '\0';

    return IDE_FAILURE;
}

SInt isqlFloat::GetColSize( mtlCurrency  *aCurrency,
                            SChar        *aFmt,
                            UChar        *aToken )
{
    SInt sColSize = 0;

    if ( idlOS::strncasecmp( aFmt, "RN", 2 ) == 0 )
    {
        // RN �� ��� 15�� ���� (mtfTo_char.cpp:2229 ����)
        sColSize = 15 + 1;
    }
    else if ( idlOS::strncasecmp( aFmt, "XXXX", 4 ) == 0 )
    {
        // XXXX �� ��� signed integer������ ����(mtfTo_char.cpp:2230 ����)
        sColSize = 8 + 1;
    }
    else
    {
        if ( aToken[MTD_NUMBER_FORMAT_C] > 0 )
        {
            sColSize = idlOS::strlen(aFmt) + MTL_TERRITORY_ISO_LEN;
        }
        else if ( aToken[MTD_NUMBER_FORMAT_L] > 0 )
        {
            sColSize = idlOS::strlen(aFmt) + aCurrency->len;
        }
        else
        {
            sColSize = idlOS::strlen(aFmt) + 2;
        }
    }
    return sColSize;
}
