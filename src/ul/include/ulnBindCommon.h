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

#ifndef _O_ULN_BIND_COMMON_H_
#define _O_ULN_BIND_COMMON_H_ 1

#include <ulnDesc.h>
#include <ulnDescRec.h>

typedef struct ulnAppBuffer
{
    ulnCTypeID      mCTYPE;
    acp_uint8_t    *mBuffer;
    ulvULen         mBufferSize;
    acp_uint32_t   *mFileOptionPtr;
    acp_uint32_t    mColumnStatus;
} ulnAppBuffer;

struct ulnIndLenPtrPair
{
    ulvSLen *mIndicatorPtr;
    ulvSLen *mLengthPtr;
};

/*
 * ����ڰ� ���ε带 ���� �ϰų� BINDINFO GET RES �� �޾Ƽ� IRD �� �����ؾ� �� �ʿ䰡 ���� ���
 * ���ο�(Ȥ�� ������) Descriptor Record �� �������ִ� �Լ�
 */
ACI_RC ulnBindArrangeNewDescRec(ulnDesc       *aDesc,
                                acp_uint16_t   aIndex,
                                ulnDescRec   **aOutputDescRec);

/*
 * ����ڰ� ���ε��� ��巹���� ����ϴ� �Լ�
 */
ACP_INLINE ACI_RC ulnBindGetBindOffsetAndElementSize(ulnDescRec   *aAppDescRec,
                                                        ulvULen      *aBindOffset,
                                                        acp_uint32_t *aElementSize)
{
    // fix BUG-20745 BIND_OFFSET_PTR ����
    if( aAppDescRec->mParentDesc->mHeader.mBindOffsetPtr != NULL )
    {
        *aBindOffset = *(aAppDescRec->mParentDesc->mHeader.mBindOffsetPtr);
    }
    else
    {
        *aBindOffset = 0;
    }

    /*
     * Note : �� ��Ģ�� desc �� APD ��, ARD �� ������� ����ȴ�.
     *        ��, SQL_ATTR_PARAM_BIND_TYPE �̰�, SQL_ATTR_ROW_BIND_TYPE �̰� ������ٴ� ���̴�.
     */
    if (aAppDescRec->mParentDesc->mHeader.mBindType != SQL_PARAM_BIND_BY_COLUMN)
    {
        /*
         * row wise binding �� ������ ����ü�� ũ�Ⱑ element size.
         */
        *aElementSize = aAppDescRec->mParentDesc->mHeader.mBindType;
    }

    return ACI_SUCCESS;
}

/*
 * ����� ������ �ּҸ� ����ϴ� �Լ�.
 *
 *      ULN_CALC_USER_BUF_ADDR()
 *      ulnBindCalcUserOctetLengthAddr()
 *      ulnBindCalcUserFileOptionAddr()
 *      ulnBindCalcUserIndLenPair()
 *
 * �Լ��鿡�� �������� ���̴� �Լ��̴�.
 *
 * BUGBUG : Execute �ÿ��� ���� ����ڰ� indicator ptr �� strlen ptr �� �и����� ���
 *          ������ ��� �˻��ؼ� SQL_NTS, SQL_NULL_DATA, length �� �Ǵ��ؼ�
 *          ����ؾ� �Ѵ�.
 *
 *          �׷��� ������ ������ ���� �ʾҴ�.
 *
 *          ���߿� ulnBindCalcUserOctetLengthAddr() �� ȣ���ϴ� �κе���
 *          ��� ã�Ƽ� ���� �־�� �Ѵ�.
 *          ���� Fetch() �ÿ� �и��� indicator, strlen ���۸� ����ؼ� ���̸� �����ϴ� 
 *          ���� ������ �Ǿ� �ִ�.
 */
#define ULN_CALC_USER_BUF_ADDR(aBoundAddr, aRowNumber, aBindOffset, aElementSize) \
    ((void *)((aBoundAddr) + (aBindOffset) + ((aRowNumber) * (aElementSize))))

ACP_INLINE acp_uint32_t *ulnBindCalcUserFileOptionAddr(ulnDescRec *aAppDescRec, acp_uint32_t aRowNumber)
{
    ulvULen      sBindOffset;
    acp_uint32_t sElementSize;

    /*
     * aRowNumber �� 0 ���̽� �ε���
     */

    if (aAppDescRec->mFileOptionsPtr == NULL)
    {
        return NULL;
    }

    /*
     * �⺻ column wise binding �� ������ ����ڰ� ���ε��� ������ Ÿ�Կ� ���� ũ��.
     * �������̸� ���ε����� ������ ���������̴�. ����ڰ� �غ��� �ξ��ٰ� �뺸�� ������ ũ��.
     */
    sElementSize = ACI_SIZEOF(acp_uint32_t);
    sBindOffset  = 0;

    /*
     * Note : sBindOffset �� sElementSize �� �ٲ� �ʿ䰡 ���� ��,
     *        ��, row wise binding �ÿ��� �ٲ۴�.
     */
    ulnBindGetBindOffsetAndElementSize(aAppDescRec, &sBindOffset, &sElementSize);

    return (acp_uint32_t *)ULN_CALC_USER_BUF_ADDR((acp_uint8_t *)(aAppDescRec->mFileOptionsPtr),
                                                  aRowNumber,
                                                  sBindOffset,
                                                  sElementSize);
}

ACP_INLINE void *ulnBindCalcUserDataAddr(ulnDescRec *aAppDescRec, acp_uint32_t aRowNumber)
{
    ulvULen      sBindOffset;
    acp_uint32_t sElementSize;

    /*
     * aRowNumber �� 0 ���̽� �ε���
     */

    if (aAppDescRec->mDataPtr == NULL)
    {
        return NULL;
    }

    /*
     * �⺻ column wise binding �� ������ ����ڰ� ���ε��� ������ Ÿ�Կ� ���� ũ��.
     * �������̸� ���ε����� ������ ���������̴�. ����ڰ� �غ��� �ξ��ٰ� �뺸�� ������ ũ��.
     *
     * Note : 64bit ���� octet length �� ���ٰ� �ص� ���� �Ⱑ �����ϱ� -_-;; �׳� acp_uint32_t �� ĳ����
     *        �ؼ� �ᵵ �Ǹ��� ������.
     */
    sElementSize = (acp_uint32_t)(aAppDescRec->mMeta.mOctetLength);
    sBindOffset  = 0;

    /*
     * Note : sBindOffset �� sElementSize �� �ٲ� �ʿ䰡 ���� ��,
     *        ��, row wise binding �ÿ��� �ٲ۴�.
     */
    ulnBindGetBindOffsetAndElementSize(aAppDescRec, &sBindOffset, &sElementSize);

    return (void *)ULN_CALC_USER_BUF_ADDR((acp_uint8_t *)(aAppDescRec->mDataPtr),
                                          aRowNumber,
                                          sBindOffset,
                                          sElementSize);
}

ACP_INLINE ulvSLen *ulnBindCalcUserOctetLengthAddr(ulnDescRec *aAppDescRec, acp_uint32_t aRowNumber)
{
    ulvULen       sBindOffset;
    acp_uint32_t  sElementSize;
    ulvSLen      *sOctetLengthPtr;

    /* BUG-36518 */
    if (aAppDescRec == NULL)
    {
        return NULL;
    }

    sOctetLengthPtr = aAppDescRec->mOctetLengthPtr;

    if (sOctetLengthPtr == NULL)
    {
        return NULL;
    }

    /*
     * �⺻ column wise binding �� ���� len or ind �� ������
     */
    sElementSize = ACI_SIZEOF(ulvSLen);
    sBindOffset  = 0;

    /*
     * Note : sBindOffset �� sElementSize �� �ٲ� �ʿ䰡 ���� ��,
     *        ��, row wise binding �ÿ��� �ٲ۴�.
     */
    ulnBindGetBindOffsetAndElementSize(aAppDescRec, &sBindOffset, &sElementSize);

    return (ulvSLen *)ULN_CALC_USER_BUF_ADDR((acp_uint8_t *)sOctetLengthPtr,
                                             aRowNumber,
                                             sBindOffset,
                                             sElementSize);
}

/*
 * ���ǻ� aLengthRetrievedFromServer �� 0 �� ���͵� NULL_DATA �� �������.
 */
ACP_INLINE ACI_RC ulnBindSetUserIndLenValue(ulnIndLenPtrPair *aIndLenPair, ulvSLen aLengthRetrievedFromServer)
{
    if (aLengthRetrievedFromServer == ULN_vLEN(0) ||
        aLengthRetrievedFromServer == SQL_NULL_DATA)
    {
        /*
         * NULL DATA �� ���ŵǾ��� ��
         */
        if (aIndLenPair->mLengthPtr != NULL)
        {
            if (aIndLenPair->mLengthPtr != aIndLenPair->mIndicatorPtr)
            {
                *aIndLenPair->mLengthPtr = ULN_vLEN(0);
            }
        }

        ACI_TEST_RAISE(aIndLenPair->mIndicatorPtr == NULL, LABEL_ERR_22002);

        *aIndLenPair->mIndicatorPtr = SQL_NULL_DATA;
    }
    else
    {
        if (aIndLenPair->mLengthPtr != NULL)
        {
            *aIndLenPair->mLengthPtr = aLengthRetrievedFromServer;
        }

        if (aIndLenPair->mIndicatorPtr != NULL)
        {
            if (aIndLenPair->mLengthPtr != aIndLenPair->mIndicatorPtr)
            {
                *aIndLenPair->mIndicatorPtr = ULN_vLEN(0);
            }
        }
    }

    return ACI_SUCCESS;

    /*
     * 22002 : Indicator Variable Required but not supplied and
     *         the data retrieved was NULL.
     *
     *         �� �Լ��� ȣ���� ������ ������ �˾Ƽ� �߻����� �ָ� �ȴ�.
     *
     * Note : Fetch �ÿ��� �߻���Ű��, outbinding parameter �� ��쿡�� ������
     *        �����ϰ�, ���� �߻���Ű�� ������ ����!
     */
    ACI_EXCEPTION(LABEL_ERR_22002);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ulvSLen ulnBindGetUserIndLenValue(ulnIndLenPtrPair *aIndLenPair);

ACP_INLINE void ulnBindCalcUserIndLenPair(ulnDescRec       *aAppDescRec,
                                             acp_uint32_t      aRowNumber,
                                             ulnIndLenPtrPair *aUserIndLenPair)
{
    ulvSLen      *sOctetLengthPtr = aAppDescRec->mOctetLengthPtr;
    ulvSLen      *sIndicatorPtr   = aAppDescRec->mIndicatorPtr;
    ulvULen       sBindOffset     = 0;
    acp_uint32_t  sElementSize    = 0;

    if (sOctetLengthPtr != NULL || sIndicatorPtr != NULL)
    {
        /*
         * �⺻ column wise binding �� ���� len or ind �� ������
         */
        sElementSize = ACI_SIZEOF(ulvSLen);
        sBindOffset  = 0;

        ulnBindGetBindOffsetAndElementSize(aAppDescRec, &sBindOffset, &sElementSize);
    }

    if (sOctetLengthPtr == NULL)
    {
        aUserIndLenPair->mLengthPtr = NULL;
    }
    else
    {
        aUserIndLenPair->mLengthPtr = (ulvSLen *)ULN_CALC_USER_BUF_ADDR((acp_uint8_t *)sOctetLengthPtr,
                                                                        aRowNumber,
                                                                        sBindOffset,
                                                                        sElementSize);
    }

    if (sIndicatorPtr == NULL)
    {
        aUserIndLenPair->mIndicatorPtr = NULL;
    }
    else
    {
        aUserIndLenPair->mIndicatorPtr = (ulvSLen *)ULN_CALC_USER_BUF_ADDR((acp_uint8_t *)sIndicatorPtr,
                                                                           aRowNumber,
                                                                           sBindOffset,
                                                                           sElementSize);
    }
}

/*
 * ����ڰ� BindParameter �ÿ� ������ BufferSize, StrLenOrIndPtr �� �ִ°��� ������
 * ����ڰ� ���� �����ϰ��� �ϴ� �������� ����� ���Ѵ�.
 */
ACP_INLINE ACI_RC ulnBindCalcUserDataLen(ulnCTypeID    aCType,
                                            ulvSLen       aUserBufferSize,
                                            acp_uint8_t  *aUserDataPtr,
                                            ulvSLen       aUserStrLenOrInd,
                                            acp_sint32_t *aUserDataLength,
                                            acp_bool_t   *aIsNullData)
{
    ulvSLen       sUserDataLength = 0;
    acp_bool_t    sIsNullData     = ACP_FALSE;
    acp_uint16_t *sWCharData      = NULL;
    ulvSLen       sWCharMaxLen    = 0;

    /*
     * Note : �� �Լ��� ȣ���� �� aUserDataPtr �� ����� �������� Ÿ���� CHAR or BINARY
     *        �� ���� Ȯ���ؾ� �Ѵ�.
     */
    if (aUserDataPtr == NULL)
    {
        /*
         * SQLBindParameter() ���� ParamValuePtr �� NULL �̸�, NULL DATA �̴�.
         */
        aUserStrLenOrInd = SQL_NULL_DATA;
    }

    switch(aUserStrLenOrInd)
    {
        case SQL_NTS:
            if (aCType == ULN_CTYPE_CHAR)
            {
                if (aUserBufferSize > ULN_vLEN(0))
                {
                    sUserDataLength = acpCStrLen((acp_char_t *)aUserDataPtr, aUserBufferSize);
                }
                else
                {
                    sUserDataLength = acpCStrLen((acp_char_t *)aUserDataPtr, ACP_SINT32_MAX);
                }
            }
            // BUG-25937 SQL_WCHAR�� CLOB�� ������ ����� �ȵ��ϴ�.
            // WCHAR ����Ÿ�� ���̸� ��Ȯ�� ���Ѵ�.
            // ���̸� �߸����ϸ� ������ �߸��� ������ ����.
            else if(aCType == ULN_CTYPE_WCHAR)
            {

                /* bug-36698: wrong length when SQL_C_WCHAR and 0 buflen.
                   wchar Ÿ�Ե� ���� char Ÿ��ó��, buflen�� 0�ΰ�쿡
                   ���ؼ� ó���� �ֵ��� �Ѵ�.
                   cf) Power Builder ���� parameter�� wchar Ÿ������ ���� */
                if (aUserBufferSize > ULN_vLEN(0))
                {
                    sWCharMaxLen = aUserBufferSize;
                }
                else
                {
                    sWCharMaxLen = (ulvSLen)ACP_SINT32_MAX;
                }

                for(sWCharData = (acp_uint16_t*)aUserDataPtr, sUserDataLength = 0;
                    (sUserDataLength < sWCharMaxLen) && (*sWCharData != 0);
                    sUserDataLength = sUserDataLength +2)
                {
                    ++sWCharData;
                }
            }
            else
            {
                /*
                 * aCType �� ULN_CTYPE_BINARY, ULN_CTYPE_BYTE, ULN_CTYPE_BIT, ULN_CTYPE_NIBBLE
                 */
                sUserDataLength = aUserBufferSize;
            }
            break;

        case SQL_NULL_DATA:
            sUserDataLength = ULN_vLEN(0);
            sIsNullData     = ACP_TRUE;
            break;

        case SQL_DEFAULT_PARAM:
        case SQL_DATA_AT_EXEC:
            /*
             * BUGBUG : ���� ���� ����
             */
            ACI_TEST(1);
            break;

        default:
            if (aUserStrLenOrInd <= SQL_LEN_DATA_AT_EXEC(0))
            {
                /*
                 * SQL_LEN_DATA_AT_EXEC() ��ũ�ο� ���� ��
                 */
                ACI_TEST(1);
            }
            else
            {
                if (aUserBufferSize > ULN_vLEN(0))
                {
                    sUserDataLength = ACP_MIN(aUserBufferSize, aUserStrLenOrInd);
                }
                else
                {
                    sUserDataLength = aUserStrLenOrInd;
                }
            }
            break;
    }

    *aUserDataLength = (acp_sint32_t)sUserDataLength;

    if (aIsNullData != NULL)
    {
        *aIsNullData = sIsNullData;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

acp_bool_t ulnBindIsFixedLengthColumn(ulnDescRec *aAppDescRec, ulnDescRec *aImpDescRec);

acp_uint16_t ulnBindCalcNumberOfParamsToSend(ulnStmt *aStmt);

ACI_RC ulnBindAdjustStmtInfo(ulnStmt *aStmt);

#endif /* _O_ULN_BIND_COMMON_H_ */
