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
#include <ulnCharSet.h>
#include <ulnConv.h>    // for ulnConvGetEndianFunc

static ulnCharSetValidFunc * const ulnCharSetValidMap[ULN_CHARACTERSET_VALIDATION_MAX] =
{
    ulnCharSetValidOff,
    ulnCharSetValidOn,
};

ACI_RC ulnCharSetValidOff(const mtlModule *aSrcCharSet,
                          acp_uint8_t     *aSourceIndex,
                          acp_uint8_t     *aSourceFence)
{
    ACP_UNUSED(aSrcCharSet);
    ACP_UNUSED(aSourceIndex);
    ACP_UNUSED(aSourceFence);

    return ACI_SUCCESS;
}


ACI_RC ulnCharSetValidOn(const mtlModule *aSrcCharSet,
                         acp_uint8_t     *aSourceIndex,
                         acp_uint8_t     *aSourceFence)
{
    while(aSourceIndex < aSourceFence)
    {
        ACI_TEST_RAISE( aSrcCharSet->nextCharPtr(&aSourceIndex, aSourceFence)
                        != NC_VALID,
                        ERR_INVALID_CHARACTER );
    }

    ACI_TEST(aSourceIndex > aSourceFence);

    return ACI_SUCCESS;

    // error�� �� �Լ��� ȣ���ϴ� ������ ������.
    ACI_EXCEPTION(ERR_INVALID_CHARACTER);

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnCharSetConvertNLiteral(ulnCharSet      *aCharSet,
                                 ulnFnContext    *aFnContext,
                                 const mtlModule *aSrcCharSet,
                                 const mtlModule *aDestCharSet,
                                 void            *aSrc,
                                 acp_sint32_t     aSrcLen)
{
    acp_uint8_t  *sTempIndex;
    acp_uint8_t  *sSourceIndex;
    acp_uint8_t  *sSourceFence;
    acp_uint8_t  *sResultValue;
    acp_uint8_t  *sResultFence;
    acp_sint32_t  sSrcRemain  = 0;
    acp_sint32_t  sDestRemain = 0;
    acp_sint32_t  sTempRemain = 0;

    aciConvCharSetList sSrcChatSet;
    aciConvCharSetList sDestChatSet;

    acp_bool_t    sIsSame_n;
    acp_bool_t    sIsSame_N;
    acp_bool_t    sIsSame_Quote;

    acp_bool_t    sNCharFlag = ACP_FALSE;
    acp_bool_t    sQuoteFlag = ACP_FALSE;
    acp_uint8_t   sSrcCharSize;

    aCharSet->mSrc     = (acp_uint8_t*)aSrc;
    aCharSet->mSrcLen  = aSrcLen;
    aCharSet->mDestLen = aSrcLen;

    if ((aDestCharSet != NULL) &&
        (aSrcCharSet  != NULL))
    {
        sSourceIndex = (acp_uint8_t*)aSrc;
        sSrcRemain   = aSrcLen;
        sSourceFence = sSourceIndex + sSrcRemain;

        if (aSrcCharSet != aDestCharSet)
        {
            if (aCharSet->mDest != NULL)
            {
                acpMemFree(aCharSet->mDest);
                aCharSet->mDest = NULL;
            }

            sDestRemain = aCharSet->mSrcLen * 2;
            // BUG-24878 iloader ���� NULL ����Ÿ ó���� ����.
            // sDestRemain �� 0�϶� malloc �� �ϸ� �ȵ˴ϴ�.
            if(sDestRemain > 0)
            {
                /* BUG-30336 */
                ACI_TEST_RAISE(acpMemCalloc((void**)&aCharSet->mDest,
                                            2,
                                            aCharSet->mSrcLen) != ACP_RC_SUCCESS,
                               LABEL_NOT_ENOUGH_MEM);
            }
            else
            {
                aCharSet->mDest = NULL;
            }

            sResultValue = aCharSet->mDest;
            sResultFence = aCharSet->mDest + sDestRemain;

            sSrcChatSet  = mtlGetIdnCharSet(aSrcCharSet);
            sDestChatSet = mtlGetIdnCharSet(aDestCharSet);

            while(sSourceIndex < sSourceFence)
            {
                ACI_TEST_RAISE(sResultValue >= sResultFence,
                               LABEL_INVALID_DATA_LENGTH);

                sSrcCharSize =  mtlGetOneCharSize( sSourceIndex,
                                                   sSourceFence,
                                                   aSrcCharSet );

                // Nliteral Prefix (N'..') �������������� �˻�
                if (sNCharFlag == ACP_FALSE)
                {
                    sIsSame_N = mtcCompareOneChar( sSourceIndex,
                                                   sSrcCharSize,
                                                   (acp_uint8_t*)"N",
                                                   1 );

                    sIsSame_n = mtcCompareOneChar( sSourceIndex,
                                                   sSrcCharSize,
                                                   (acp_uint8_t*)"n",
                                                   1 );

                    if (sIsSame_N == ACP_TRUE || sIsSame_n == ACP_TRUE)
                    {
                        sNCharFlag = ACP_TRUE;
                    }
                }
                // N prefix (N'..')�� �̹� ã�� ���¿��� quote(') ���� �˻�
                else
                {
                    // N-literal�� open quote ���ڸ� ���� ��ã�� ���
                    // open quote �������� �˻�(N'...')
                    if (sQuoteFlag == ACP_FALSE)
                    {
                        sIsSame_Quote = mtcCompareOneChar( sSourceIndex,
                                                           sSrcCharSize,
                                                           (acp_uint8_t*)"\'",
                                                           1 );

                        // N-literal�� open quote�� ã�� ��� (N')
                        if (sIsSame_Quote == ACP_TRUE)
                        {
                            sTempRemain = sDestRemain;

                            ACI_TEST_RAISE(aciConvConvertCharSet(sSrcChatSet,
                                                                 sDestChatSet,
                                                                 sSourceIndex,
                                                                 sSrcRemain,
                                                                 sResultValue,
                                                                 &sDestRemain,
                                                                 -1 /* mNlsNcharConvExcp */ )
                                           != ACI_SUCCESS, LABEL_INVALID_DATA_LENGTH);

                            // aciConvConvertCharSet ���� �������� ���Ͼ�� �ֱ� ������ sDestRemain �� ���� �����δ�.
                            sResultValue += (sTempRemain - sDestRemain);

                            sTempIndex = sSourceIndex;

                            (void)aSrcCharSet->nextCharPtr(&sSourceIndex, sSourceFence);

                            sSrcRemain -= (sSourceIndex - sTempIndex);
                            sQuoteFlag = ACP_TRUE;
                        }
                        // N ���� �ڿ� quote ���ڰ� ���� ������ N ���ڴ�
                        // N-literal prefix�� �ƴϹǷ� ����
                        else
                        {
                            sNCharFlag = ACP_FALSE;
                        }
                    }

                    // N-literal�� close quote �������� �˻�(N'...')
                    // ���� if�� sQuoteFlag���� ���� if-else ���迡 �����Ƿ�
                    // else�� ��� ������, N'' �� ��� open quote �ڿ�
                    // close quote�� �������� ���Ƿ� �̸� ó���ϱ� ����
                    // ������ if �� �˻��Ѵ�
                    if (sQuoteFlag == ACP_TRUE)
                    {
                        sSrcCharSize =  mtlGetOneCharSize( sSourceIndex,
                                                           sSourceFence,
                                                           aSrcCharSet );

                        sIsSame_Quote = mtcCompareOneChar( sSourceIndex,
                                                           sSrcCharSize,
                                                           (acp_uint8_t*)"\'",
                                                           1 );

                        if (sIsSame_Quote == ACP_TRUE)
                        {
                            sQuoteFlag = ACP_FALSE;
                            sNCharFlag = ACP_FALSE;
                        }
                    }
                }

                sTempIndex = sSourceIndex;

                // �Ϲ� ���ڿ��� ��� DB charset���� ��ȯ
                if (sQuoteFlag != ACP_TRUE)
                {
                    sTempRemain = sDestRemain;

                    ACI_TEST_RAISE(aciConvConvertCharSet(sSrcChatSet,
                                                         sDestChatSet,
                                                         sSourceIndex,
                                                         sSrcRemain,
                                                         sResultValue,
                                                         &sDestRemain,
                                                         -1 /* mNlsNcharConvExcp */ )
                                   != ACI_SUCCESS, LABEL_INVALID_DATA_LENGTH);

                    // aciConvConvertCharSet ���� �������� ���Ͼ�� �ֱ� ������ sDestRemain �� ���� �����δ�.
                    sResultValue += (sTempRemain - sDestRemain);

                    (void)aSrcCharSet->nextCharPtr(&sSourceIndex, sSourceFence);

                    sSrcRemain -= (sSourceIndex - sTempIndex);
                }
                // N-literal ���ڿ��� ��� (N'..') client charset �״�� ����
                else
                {
                    (void)aSrcCharSet->nextCharPtr(&sSourceIndex, sSourceFence);

                    acpMemCpy(sResultValue, sTempIndex, sSourceIndex - sTempIndex);
                    sDestRemain -= sSourceIndex - sTempIndex;
                    sSrcRemain -= sSourceIndex - sTempIndex;
                    sResultValue += sSourceIndex - sTempIndex;
                }
            }

            aCharSet->mDestLen = sResultValue - aCharSet->mDest;
        }
        else
        {
            // TASK-3420 ���� ó�� ��å ����
            // ������ ĳ���ͼ��ϰ�� �˻����� �ʴ´�.
            // ȯ�溯�� ALTIBASE_NLS_CHARACTERSET_VALIDATION ������� �ʴ´�.
        }
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                     ulERR_FATAL_MEMORY_ALLOC_ERROR,
                     "ulnCharSet::convertNLiteral");
        }
    }
    ACI_EXCEPTION(LABEL_INVALID_DATA_LENGTH)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                     ulERR_ABORT_VALIDATE_INVALID_LENGTH);
        }
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

// bug-21918: fnContext arg is NULL
// object type�� ���� Dbc ���ϱ�
// dbc object : self return
// stmt object: parent dbc return
// desc object: parent dbc or parent stmt's parent dbc return
ulnDbc* getDbcFromObj(ulnObject* aObj)
{
    ulnDbc    *sDbc = NULL;
    ulnObject *sParentObj;

    //BUG-28184 [CodeSonar] Null Pointer Dereference
    ACI_TEST(aObj == NULL);

    switch (ULN_OBJ_GET_TYPE(aObj))
    {
        case ULN_OBJ_TYPE_DBC:
            sDbc = (ulnDbc *)aObj;
            break;
        case ULN_OBJ_TYPE_STMT:
            sDbc = (ulnDbc *)(((ulnStmt *)aObj)->mParentDbc);
            break;
        // desc �� parent obj�� stmt�̰ų� dbc�� �� �ִ�
        case ULN_OBJ_TYPE_DESC:
            sParentObj = ((ulnDesc *)aObj)->mParentObject;
            // desc's parent: Dbc
            if (ULN_OBJ_GET_TYPE(sParentObj) == ULN_OBJ_TYPE_DBC)
            {
                sDbc = (ulnDbc *)(sParentObj);
            }
            // desc's parent: stmt
            else
            {
                sDbc = (ulnDbc *)(((ulnStmt *)sParentObj)->mParentDbc);
            }
            break;
        default:
            sDbc = NULL;
            break;
    }
    return sDbc;

    ACI_EXCEPTION_END;

    return sDbc;
}

ACI_RC ulnCharSetConvert(ulnCharSet      *aCharSet,
                         ulnFnContext    *aFnContext,
                         void            *aObj,
                         const mtlModule *aSrcCharSet,
                         const mtlModule *aDestCharSet,
                         void            *aSrc,
                         acp_sint32_t    aSrcLen,
                         acp_sint32_t    aOption)
{
    ulnDbc       *sDbc = NULL;
    acp_uint8_t  *sTempIndex;
    acp_uint8_t  *sSourceIndex;
    acp_uint8_t  *sSourceFence;
    acp_uint8_t  *sResultValue;
    acp_uint8_t  *sResultFence;
    acp_sint32_t  sSrcRemain  = 0;
    acp_sint32_t  sDestRemain = 0;
    acp_sint32_t  sTempRemain = 0;

    aciConvCharSetList sSrcCharSet;
    aciConvCharSetList sDestCharSet;

    ulnObject* sObj = (ulnObject *)aObj;

// =================================================================
// bug-21918(fnContext null) ���� ��������
// 1��° ���ڿ� 2��° ���ڴ� ���߿� �ϳ��� ������ �ȴ�(�ϳ��� NULL)
// 1��° ���ڸ�(fnContext) ������ ���ο��� DBC ���� ���Ѵ�
// 1��° ���ڰ�(fnContext) NULL �� ���(SQLCWrapper���� ���� ȣ���� ���)
// 2��° ����(object)�� �޾Ƽ� object Ÿ�Կ� ����(dbc, stmt, desc)
// dbc�� ���� ���Ѵ�
// ����� FnContext�� error msg�� ����ϱ� ���� ����ϴ� ���̴�
// dbc�� ���ϴ� ������ mNlsCharactersetValidation�� ����ϱ� ���ؼ���
// =================================================================
    if (aFnContext == NULL)
    {
        sDbc = getDbcFromObj(sObj);
    }
    else
    {
        ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    }
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    aCharSet->mSrc     = (acp_uint8_t*)aSrc;
    aCharSet->mSrcLen  = aSrcLen;
    aCharSet->mDestLen = aSrcLen;

    if (aCharSet->mDest != NULL)
    {
        acpMemFree(aCharSet->mDest);
        aCharSet->mDest = NULL;
    }

    // BUG-24831 �����ڵ� ����̹����� mtl::defaultModule() �� ȣ���ϸ� �ȵ˴ϴ�.
    // ���ڷ� NULL �� �Ѿ�ð�� Ŭ���̾�Ʈ ĳ���� ������ �����Ѵ�.
    if(aSrcCharSet == NULL)
    {
        aSrcCharSet = sDbc->mClientCharsetLangModule;
    }
    if(aDestCharSet == NULL)
    {
        aDestCharSet = sDbc->mClientCharsetLangModule;
    }

    if ((aDestCharSet != NULL) &&
        (aSrcCharSet  != NULL))
    {
        sSourceIndex = (acp_uint8_t*)aSrc;
        /*
         * BUG-29148 [CodeSonar]Null Pointer Dereference
         */
        ACI_TEST(sSourceIndex == NULL);
        sSrcRemain   = aSrcLen;
        sSourceFence = sSourceIndex + sSrcRemain;

        // bug-23311: utf-16 little-endian error in ODBC unicode driver
        // utf16 little-endian�� ��� big-endian���� ��ȯ
        // ex) windows ODBC unicode driver(SQL..Set..W()�迭 �Լ�)�� ���� ���ڿ���
#ifndef ENDIAN_IS_BIG_ENDIAN
        if ((aSrcCharSet->id == MTL_UTF16_ID) &&
            ((aOption & CONV_DATA_IN) == CONV_DATA_IN) &&
            (aSrc != NULL) && (aSrcLen != 0))
        {
            ACI_TEST_RAISE(ulnCharSetConvWcharEndian(aCharSet,
                                                     (acp_uint8_t*)aSrc,
                                                     aSrcLen)
                           != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);
            sSourceIndex = aCharSet->mWcharEndianBuf;
            sSourceFence = sSourceIndex + sSrcRemain;
        }
#endif

        if (aSrcCharSet != aDestCharSet)
        {
            // bug-34592: converting a shift-jis to utf8 needs 3 bytes
            // ex) 0xb1 (shift-jis) => 0x ef bd b1 (utf8)
            sDestRemain = aCharSet->mSrcLen * (aDestCharSet->maxPrecision(1));

            // BUG-24878 iloader ���� NULL ����Ÿ ó���� ����.
            // sDestRemain �� 0�϶� malloc �� �ϸ� �ȵ˴ϴ�.
            if(sDestRemain > 0)
            {
                /* BUG-30336 */
                ACI_TEST_RAISE(acpMemCalloc((void**)&aCharSet->mDest,
                                            aDestCharSet->maxPrecision(1),
                                            aCharSet->mSrcLen) != ACP_RC_SUCCESS,
                               LABEL_NOT_ENOUGH_MEM);
            }
            else
            {
                aCharSet->mDest = NULL;
            }

            sResultValue = aCharSet->mDest;
            sResultFence = aCharSet->mDest + sDestRemain;

            sSrcCharSet  = mtlGetIdnCharSet(aSrcCharSet);
            sDestCharSet = mtlGetIdnCharSet(aDestCharSet);

            while(sSourceIndex < sSourceFence)
            {
                ACI_TEST_RAISE(sResultValue >= sResultFence,
                               LABEL_INVALID_DATA_LENGTH);

                sTempRemain = sDestRemain;

                ACI_TEST_RAISE(aciConvConvertCharSet(sSrcCharSet,
                                                     sDestCharSet,
                                                     sSourceIndex,
                                                     sSrcRemain,
                                                     sResultValue,
                                                     &sDestRemain,
                                                     -1 /* mNlsNcharConvExcp */ )
                               != ACI_SUCCESS, LABEL_INVALID_DATA_LENGTH);
                // aciConvConvertCharSet ���� �������� ���Ͼ�� �ֱ� ������ sDestRemain �� ���� �����δ�.
                sResultValue += (sTempRemain - sDestRemain);

                sTempIndex = sSourceIndex;
                // TASK-3420 ���� ó�� ��å ����
                // �ٸ� ĳ���� ���� ��� ������ �߻����� �ʴ´�.
                (void)aSrcCharSet->nextCharPtr(&sSourceIndex, sSourceFence);
                sSrcRemain -= (sSourceIndex - sTempIndex);
            }

            aCharSet->mDestLen = sResultValue - aCharSet->mDest;
        }
        else
        {
            // TASK-3420 ���� ó�� ��å ����
            // ������ ĳ���ͼ��ϰ�� �˻����� �ʴ´�.
            // ȯ�溯�� ALTIBASE_NLS_CHARACTERSET_VALIDATION ������� �ʴ´�.
        }

        // bug-23311: utf-16 little-endian error in ODBC unicode driver
        // �����κ��� ��ȯ�� utf16 big-endian�� ��� little-endian���� ��ȯ
        // ex) windows ODBC unicode driver(SQL..Get..W()�迭 �Լ�)�� ��ȯ�ϴ� ���ڿ���
#ifndef ENDIAN_IS_BIG_ENDIAN
        if ((aDestCharSet->id == MTL_UTF16_ID) &&
            ((aOption & CONV_DATA_OUT) == CONV_DATA_OUT))
        {
            // utf16 to utf16�� ��� src endian ���� ����
            if (aSrcCharSet == aDestCharSet)
            {
                ACI_TEST( aSrc == NULL );     //BUG-28561 [CodeSonar] Null Pointer Dereference
                ulnConvEndian_ADJUST((acp_uint8_t*)aSrc, aSrcLen);
            }
            // ������ malloc�� �ϹǷ� �׳� �����⸸ �ϸ� ��
            else
            {
                ulnConvEndian_ADJUST(aCharSet->mDest, aCharSet->mDestLen);
            }
        }
#endif

    }

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                    ulERR_FATAL_MEMORY_ALLOC_ERROR,
                    "ulnCharSet::ulConvert");
        }
    }
    ACI_EXCEPTION(LABEL_INVALID_DATA_LENGTH)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                     ulERR_ABORT_VALIDATE_INVALID_LENGTH);
        }
    }

    ACI_EXCEPTION_END;

    return ACI_FAILURE;
}

ACI_RC ulnCharSetConvertUseBuffer(ulnCharSet      *aCharSet,
                                  ulnFnContext    *aFnContext,
                                  void            *aObj,
                                  const mtlModule *aSrcCharSet,
                                  const mtlModule *aDestCharSet,
                                  void            *aSrc,
                                  acp_sint32_t     aSrcLen,
                                  void            *aDest,
                                  acp_sint32_t     aDestLen,
                                  acp_sint32_t     aOption)
{
    ulnDbc       *sDbc = NULL;
    acp_uint8_t  *sSrcPrePtr;
    acp_uint8_t  *sSrcCurPtr;
    acp_uint8_t  *sSrcFence;
    acp_uint8_t  *sResultCurPtr;
    acp_uint8_t  *sResultFence;
    acp_sint32_t  sSrcRemain  = 0;
    acp_sint32_t  sDestRemain = 0;
    acp_sint32_t  sTempRemain = 0;
    acp_uint8_t  *sConvBufPtr;
    acp_uint8_t   sConvBuffer[ULN_MAX_CHARSIZE] = {0, };  // ����� ���۰� �����Ҷ� ��ȯ�ϴµ� ���
    acp_sint32_t  sConvSrcSize = 0;                       // ��ȯ�� ���� ����
    acp_sint32_t  sConvDesSize = 0;                       // ��ȯ�� ����

    aciConvCharSetList sSrcCharSet;
    aciConvCharSetList sDestCharSet;

    ulnObject* sObj = (ulnObject *)aObj;

    // bug-21918: FnContext�� NULL�� ��� error
    // FnContext ���ڷ� NULL�� ������ 2��° ���ڷ� �Ѱܹ���
    // Dbc�� ���� �ʿ� ���� �׳� ����ϵ��� �Ѵ�
    if (aFnContext == NULL)
    {
        sDbc = getDbcFromObj(sObj);
    }
    else
    {
        ULN_FNCONTEXT_GET_DBC(aFnContext, sDbc);
    }
    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_TEST_RAISE(sDbc == NULL, InvalidHandleException);

    /*
     * BUG-28980 [CodeSonar]Null Pointer Dereference]
     */
    ACI_TEST(aSrc == NULL);
    aCharSet->mSrc     = (acp_uint8_t*)aSrc;
    aCharSet->mSrcLen  = aSrcLen;
    aCharSet->mDestLen = aSrcLen;

    if (aCharSet->mDest != NULL)
    {
        acpMemFree(aCharSet->mDest);
        aCharSet->mDest = NULL;
    }

    // BUG-24831 �����ڵ� ����̹����� mtl::defaultModule() �� ȣ���ϸ� �ȵ˴ϴ�.
    // ���ڷ� NULL �� �Ѿ�ð�� Ŭ���̾�Ʈ ĳ���� ������ �����Ѵ�.
    if(aSrcCharSet == NULL)
    {
        aSrcCharSet = sDbc->mClientCharsetLangModule;
    }
    if(aDestCharSet == NULL)
    {
        aDestCharSet = sDbc->mClientCharsetLangModule;
    }

    sDestRemain = aDestLen;

    if ((aDestCharSet != NULL) &&
        (aSrcCharSet  != NULL))
    {
        sSrcCurPtr = (acp_uint8_t*)aSrc;
        sSrcRemain = aSrcLen;
        sSrcFence  = sSrcCurPtr + sSrcRemain;

        // bug-23311: utf-16 little-endian error in ODBC unicode driver
        // utf16 little-endian�� ��� big-endian���� ��ȯ
        // ex) windows ODBC unicode driver(SQL..Set..W()�迭 �Լ�)�� ���� ���ڿ���
#ifndef ENDIAN_IS_BIG_ENDIAN
        if ((aSrcCharSet->id == MTL_UTF16_ID) &&
            ((aOption & CONV_DATA_IN) == CONV_DATA_IN) &&
            (aSrc != NULL) && (aSrcLen != 0))
        {
            ACI_TEST_RAISE(ulnCharSetConvWcharEndian(aCharSet,
                                                     (acp_uint8_t*)aSrc,
                                                     aSrcLen)
                           != ACI_SUCCESS, LABEL_NOT_ENOUGH_MEM);
            sSrcCurPtr   = aCharSet->mWcharEndianBuf;
            sSrcFence = sSrcCurPtr + sSrcRemain;
        }
#endif

        if (aSrcCharSet  != aDestCharSet)
        {
            sResultCurPtr = (acp_uint8_t*)aDest;
            sResultFence  = (acp_uint8_t*)aDest + sDestRemain;

            sSrcCharSet  = mtlGetIdnCharSet(aSrcCharSet);
            sDestCharSet = mtlGetIdnCharSet(aDestCharSet);

            aCharSet->mDestLen      = 0;
            aCharSet->mConvedSrcLen = 0;
            aCharSet->mCopiedDesLen = 0;

            while(sSrcCurPtr < sSrcFence)
            {
                ACI_TEST_RAISE(sResultCurPtr >= sResultFence,
                               LABEL_STRING_RIGHT_TRUNCATED);

                // ����� ���۰� ���� �ȳ������� ���� ���۷� ��ȯ
                sDestRemain = (sResultFence - sResultCurPtr);
                if (sDestRemain < ULN_MAX_CHARSIZE)
                {
                    sConvBufPtr = sConvBuffer;
                }
                else
                {
                    sConvBufPtr = sResultCurPtr;
                }
                sTempRemain = ULN_MAX_CHARSIZE;

                ACI_TEST_RAISE(aciConvConvertCharSet(sSrcCharSet,
                                                     sDestCharSet,
                                                     sSrcCurPtr,
                                                     sSrcRemain,
                                                     sConvBufPtr,
                                                     &sTempRemain,
                                                     -1 /* mNlsNcharConvExcp */ )
                               != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);

                sConvDesSize = ULN_MAX_CHARSIZE - sTempRemain;

                // ��ȯ�� aDest buffef �� endian ����
                // BUG-27515: string right truncated�ǰų�
                // ���ڰ� �߷��� ����� ��쿡�� ����� ��ȯ�ϱ� ���ؼ�
                // charset ��ȯ �� �ٷ� endian�� ��ȯ�Ѵ�.
#ifndef ENDIAN_IS_BIG_ENDIAN
                if ((aDestCharSet->id == MTL_UTF16_ID) &&
                    ((aOption & CONV_DATA_OUT) == CONV_DATA_OUT))
                {
                    ulnConvEndian_ADJUST(sConvBufPtr, sConvDesSize);
                }
#endif

                sSrcPrePtr = sSrcCurPtr;
                // TASK-3420 ���� ó�� ��å ����
                // �ٸ� ĳ���� ���� ��� ������ �߻����� �ʴ´�.
                (void)aSrcCharSet->nextCharPtr(&sSrcCurPtr, sSrcFence);
                sConvSrcSize = sSrcCurPtr - sSrcPrePtr;

                sSrcRemain              -= sConvSrcSize;
                aCharSet->mConvedSrcLen += sConvSrcSize;
                aCharSet->mDestLen      += sConvDesSize;

                // ���۰� ���ڶ�� mRemainText�� �־�д�.
                if ((sResultCurPtr + sConvDesSize) > sResultFence)
                {
                    sTempRemain = sResultFence - sResultCurPtr;
                    acpMemCpy(sResultCurPtr, sConvBuffer, sTempRemain);
                    aCharSet->mCopiedDesLen += sTempRemain;

                    aCharSet->mRemainTextLen = sConvDesSize - sTempRemain;
                    acpMemCpy(aCharSet->mRemainText, sConvBuffer + sTempRemain, aCharSet->mRemainTextLen);

                    ACI_RAISE(LABEL_STRING_RIGHT_TRUNCATED);
                }
                else if (sConvDesSize > 0)
                {
                    // ���� ���۸� �̿��ؼ� ��ȯ�� ���, ����� ���ۿ� ����
                    if (sDestRemain < ULN_MAX_CHARSIZE)
                    {
                        acpMemCpy(sResultCurPtr, sConvBuffer, sConvDesSize);
                    }

                    sResultCurPtr           += sConvDesSize;
                    aCharSet->mCopiedDesLen += sConvDesSize;
                }
                else
                {
                    // do nothing: �������� �Ͼ�� ���� ���
                }
            }
        }
        else
        {
            // TASK-3420 ���� ó�� ��å ����
            // ������ ĳ���ͼ��ϰ�� �˻����� �ʴ´�.
            // ȯ�溯�� ALTIBASE_NLS_CHARACTERSET_VALIDATION ������� �ʴ´�.

            // bug-23311: utf-16 little-endian error in ODBC unicode driver
            // �����κ��� ��ȯ�� utf16 big-endian�� ��� little-endian���� ��ȯ
            // ex) windows ODBC unicode driver(SQL..Get..W()�迭 �Լ�)�� ��ȯ�ϴ� ���ڿ���
#ifndef ENDIAN_IS_BIG_ENDIAN
            ACI_TEST( aCharSet->mSrc == NULL );     //BUG-28561 [CodeSonar] Null Pointer Dereference

            if ((aDestCharSet->id == MTL_UTF16_ID) &&
                ((aOption & CONV_DATA_OUT) == CONV_DATA_OUT))
            {
                // utf16 to utf16�� ��� src endian ���� ����
                ulnConvEndian_ADJUST((acp_uint8_t*)aCharSet->mSrc, aCharSet->mSrcLen);
            }
#endif

            // BUG-27515: ������ �ɸ��ͼ��� ��쿡�� ����� ���ۿ� �����ؾ��Ѵ�.
            sConvDesSize = ACP_MIN(aCharSet->mSrcLen, aDestLen);
            acpMemCpy(aDest, aCharSet->mSrc, sConvDesSize);
            aCharSet->mConvedSrcLen = aCharSet->mCopiedDesLen = sConvDesSize;
        }
    }

    ULN_TRACE_LOG(aFnContext, ULN_TRACELOG_HIGH, aDest, aCharSet->mCopiedDesLen,
            "%-18s| [%5"ACI_INT32_FMT" to %5"ACI_INT32_FMT"]",
            "ulnCharSetConvertU", aSrcCharSet->id, aDestCharSet->id);

    return ACI_SUCCESS;

    /* BUG-46052 codesonar Null Pointer Dereference */
    ACI_EXCEPTION(InvalidHandleException)
    {
        ULN_FNCONTEXT_SET_RC(aFnContext, SQL_INVALID_HANDLE);
    }
    ACI_EXCEPTION(LABEL_STRING_RIGHT_TRUNCATED)
    {
        // ���� ���ڿ� ���� ���
        if ((aOption & CONV_CALC_TOTSIZE) == CONV_CALC_TOTSIZE)
        {
            sConvBufPtr = sConvBuffer;
            while (sSrcCurPtr < sSrcFence)
            {
                sTempRemain = ULN_MAX_CHARSIZE;

                ACI_TEST_RAISE(aciConvConvertCharSet(sSrcCharSet,
                                                     sDestCharSet,
                                                     sSrcCurPtr,
                                                     sSrcRemain,
                                                     sConvBufPtr,
                                                     &sTempRemain,
                                                     -1)
                               != ACI_SUCCESS, LABEL_CONVERT_CHARSET_FAILED);
                sSrcPrePtr = sSrcCurPtr;
                (void)aSrcCharSet->nextCharPtr(&sSrcCurPtr, sSrcFence);

                sSrcRemain -= (sSrcCurPtr - sSrcPrePtr);
                aCharSet->mDestLen += (ULN_MAX_CHARSIZE - sTempRemain);
            }
        }
    }
    ACI_EXCEPTION(LABEL_CONVERT_CHARSET_FAILED)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                     ulERR_ABORT_CONVERT_CHARSET_FAILED,
                     sSrcCharSet,
                     sDestCharSet,
                     sSrcFence - sSrcCurPtr,
                     (sConvBufPtr == sConvBuffer) ? -1 : (sConvBufPtr - sResultCurPtr) );
        }
    }

#ifndef ENDIAN_IS_BIG_ENDIAN
    ACI_EXCEPTION(LABEL_NOT_ENOUGH_MEM)
    {
        if (aFnContext != NULL)
        {
            ulnError(aFnContext,
                    ulERR_FATAL_MEMORY_ALLOC_ERROR,
                    "ulnCharSet::ulConvert");
        }
    }
#endif

    ACI_EXCEPTION_END;

    if ((aSrcCharSet != NULL) && (aDestCharSet != NULL))
    {
        ULN_TRACE_LOG(aFnContext, ULN_TRACELOG_LOW, aSrc, aSrcLen,
                "%-18s| [%5"ACI_INT32_FMT" to %5"ACI_INT32_FMT"] fail",
                "ulnCharSetConvertU", aSrcCharSet->id, aDestCharSet->id);
    }

    return ACI_FAILURE;
}

//==============================================================
// bug-23311: utf-16 little-endian error in ODBC unicode driver
// utf-16 little-endian �� big-endian���� ��ȯ��Ŵ (������ �������� ���)
// malloc�� �� mSrc�� ����Ű���� ��.
// ulnCharSet�� ���� ���������� ���ǹǷ� ��κ��� �Ҹ��� ȣ��� free��
// parameter in�� ������ �Ǵ� ��� ȣ��ø��� �ٷιٷ� fee�� �ٽ� malloc ��
// direction�� CONV_DATA_IN�� ��츸 ȣ���
//==============================================================
ACI_RC ulnCharSetConvWcharEndian(ulnCharSet   *aCharSet,
                                 acp_uint8_t  *aSrcPtr,
                                 acp_sint32_t  aSrcLen)
{
    // ������ �Ҵ��ߴ� �޸� ����
    // nchar parameter�� ������ binding�ϴ� ��� ���� memory�� �������� ���̴�
    if (aCharSet->mWcharEndianBuf != NULL)
    {
        acpMemFree(aCharSet->mWcharEndianBuf);
        aCharSet->mWcharEndianBuf = NULL;
    }

    ACI_TEST_RAISE(acpMemAlloc((void**)&aCharSet->mWcharEndianBuf, aSrcLen) != ACP_RC_SUCCESS,
                   LABEL_CONV_WCHAR_ENDIAN_ERR);

    acpMemCpy(aCharSet->mWcharEndianBuf, aSrcPtr, aSrcLen);

    // switch endian
    ulnConvEndian_ADJUST(aCharSet->mWcharEndianBuf, aSrcLen);

    // mSrc�� endian�� �ٲ� ������ ����Ű���� �Ѵ�.
    // ���߿� getConvertedText()�� ȣ���ϸ� ���� mWcharEndianBuf ��ġ�� ��ȯ�� ����
    aCharSet->mSrc = aCharSet->mWcharEndianBuf;

    return ACI_SUCCESS;

    ACI_EXCEPTION(LABEL_CONV_WCHAR_ENDIAN_ERR)
    {
        // nothing to do;
    }
    ACI_EXCEPTION_END;
    return ACI_FAILURE;
}
