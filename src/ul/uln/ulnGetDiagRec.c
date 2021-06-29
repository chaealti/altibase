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
#include <ulnDiagnostic.h>

/*
 * BUGBUG:
 * SQLCHAR * �� C �� unsigned char * �� ���εȴ�.
 * MSDN ODBC Appendix D:Data Types -> C Data Types �� ���� ǥ�� ���´�.
 */
SQLRETURN ulnGetDiagRec(acp_sint16_t   aHandleType,
                        ulnObject     *aObject,
                        acp_sint16_t   aRecNumber,
                        acp_char_t    *aSqlState,
                        acp_sint32_t  *aNativeErrorPtr,
                        acp_char_t    *aMessageText,
                        acp_sint16_t   aBufferLength,
                        acp_sint16_t  *aTextLengthPtr,
                        acp_bool_t     aRemoveAfterGet)
{
    ULN_FLAG(sNeedUnlock);

    acp_char_t   *sSqlState;
    ulnObject    *sObject = NULL;

    acp_sint32_t  sNativeErrorCode;
    acp_sint16_t  sActualLength;
    SQLRETURN     sSqlReturnCode = SQL_SUCCESS;

    ulnDiagRec   *sDiagRec;

    ACI_TEST_RAISE(aObject == NULL, InvalidHandle);

    sObject = aObject;

    /*
     * �ڵ� Ÿ�԰� ��ü�� ���� Ÿ���� ��ġ�ϴ��� üũ
     */
    switch (aHandleType)
    {
        case SQL_HANDLE_STMT:
            ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sObject) != ULN_OBJ_TYPE_STMT, InvalidHandle);
            break;

        case SQL_HANDLE_DBC:
            ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sObject) != ULN_OBJ_TYPE_DBC, InvalidHandle);
            sObject = (ulnObject*)ulnDbcSwitch((ulnDbc *) sObject);
            break;

        case SQL_HANDLE_ENV:
            ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sObject) != ULN_OBJ_TYPE_ENV, InvalidHandle);
            break;

        case SQL_HANDLE_DESC:
            ACI_TEST_RAISE(ULN_OBJ_GET_TYPE(sObject) != ULN_OBJ_TYPE_DESC, InvalidHandle);
            ACI_TEST_RAISE(ULN_OBJ_GET_DESC_TYPE(sObject) != ULN_DESC_TYPE_ARD &&
                           ULN_OBJ_GET_DESC_TYPE(sObject) != ULN_DESC_TYPE_APD &&
                           ULN_OBJ_GET_DESC_TYPE(sObject) != ULN_DESC_TYPE_IRD &&
                           ULN_OBJ_GET_DESC_TYPE(sObject) != ULN_DESC_TYPE_IPD,
                           InvalidHandle);
            break;

        default:
            ACI_RAISE(InvalidHandle);
            break;
    }

    /*
     * ======================================
     * Function BEGIN
     * ======================================
     */

    ACI_TEST_RAISE(aRecNumber <= 0, Error);
    ACI_TEST_RAISE(aBufferLength < 0, Error);

    /*
     * Note : �������� ���̺��� �ʿ� ����. �ڵ� �Ҵ� �ȵ������� ������ Invalid Handle
     */

    /*
     * Diagnostic Record �� ����Ű�� �����͸� ��´�.
     */
    ACI_TEST_RAISE(ulnGetDiagRecFromObject(sObject, &sDiagRec, aRecNumber) != ACI_SUCCESS,
                   NoData);

    /*
     * SQLSTATE String �� ����Ű�� �����͸� ���
     * Note : ���� ����� �޸𸮿� ���縦 ���� ���� ������ GetDiagField ������ ���� ����
     */
    ulnDiagRecGetSqlState(sDiagRec, &sSqlState);

    /*
     * NativeErrorCode
     */
    sNativeErrorCode = ulnDiagRecGetNativeErrorCode(sDiagRec);

    /*
     * Message Text
     */
    // BUG-22887 Windows ODBC ����
    // �Լ��ȿ��� �˾Ƽ� ���ְ� �ִ�.
    ACI_TEST(ulnDiagRecGetMessageText(sDiagRec,
                                      aMessageText,
                                      aBufferLength,
                                      &sActualLength) != ACI_SUCCESS);
    /*
     * Truncation �� SQL_SUCCESS_WITH_INFO ����
     */
    if ( sActualLength >= aBufferLength )
    {
        sSqlReturnCode = SQL_SUCCESS_WITH_INFO;
    }

    // BUG-23965 aTextLengthPtr �� NULL �� ���ö� �߸� ó����
    /*
     * TextLength
     */
    if(aTextLengthPtr != NULL)
    {
        *aTextLengthPtr = sActualLength;
    }

    /*
     * SQLSTATE
     */
    if(aSqlState != NULL)
    {
        acpSnprintf(aSqlState, 6, "%s", sSqlState);
    }

    /*
     * NativeErrorCode
     */
    if(aNativeErrorPtr != NULL)
    {
        *aNativeErrorPtr = sNativeErrorCode;
    }

    /*
     * SQLError() �� ���ؼ� ȣ�� �Ǿ��� ���,
     * ����ڿ��� �˸��� ���� ������ �� �ش� ���ڵ带 �����ؾ� �Ѵ�.
     */
    if(aRemoveAfterGet == ACP_TRUE)
    {
        ulnDiagHeaderRemoveDiagRec(sDiagRec->mHeader, sDiagRec);
    }

    /*
     * ======================================
     * Function END
     * ======================================
     */

    return sSqlReturnCode;

    ACI_EXCEPTION(InvalidHandle)
    {
        sSqlReturnCode = SQL_INVALID_HANDLE;
    }

    ACI_EXCEPTION(Error)
    {
        sSqlReturnCode = SQL_ERROR;
    }

    ACI_EXCEPTION(NoData)
    {
        sSqlReturnCode = SQL_NO_DATA;
    }

    ACI_EXCEPTION_END;
    {
        if (aSqlState != NULL)
        {
            *aSqlState = 0;
        }

        if (aNativeErrorPtr != NULL)
        {
            *aNativeErrorPtr = 0;
        }

        if ((aMessageText != NULL) && (aBufferLength > 0))
        {
            *aMessageText = 0;
        }

        if (aTextLengthPtr != NULL)
        {
            *aTextLengthPtr = 0;
        }
    }

    ULN_IS_FLAG_UP(sNeedUnlock)
    {
        ULN_OBJECT_UNLOCK(sObject, ULN_FID_GETDIAGREC);
    }

    return sSqlReturnCode;
}

