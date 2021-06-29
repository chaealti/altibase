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

/**
 * �� ������ ������ uln2 �� �ִ� escape �� �ҽ��� �״�� ������ ���̴�.
 * ���Ŀ� �ð��� ����ϴ´�� ȿ�����̰� ����� �����ϵ��� ������ �ʿ䰡 �ִ�.
 * �켱�� ���� �迡 ���ư��⸸ �ϸ� �ȴٴ� ��ǥ�� ������ ���̴�.
 *
 * BUGBUG: ���� �����ؾ� �Ѵ�.
 */

#ifndef _ULN_ESCAPE_H_
#define _ULN_ESCAPE_H_ 1

enum
{
    ULN_ESCAPE_DATE = 0,
    ULN_ESCAPE_TIME,
    ULN_ESCAPE_TIMESTAMP,
    ULN_ESCAPE_PROCEDURE_CALL,
    ULN_ESCAPE_MAX
};

typedef struct ulnEscape
{
    acp_char_t   *mSql;
    acp_sint32_t  mLen;
    acp_char_t   *mPtr;

    acp_char_t   *mUnescapedSql;
    acp_sint32_t  mUnescapedLen;
    acp_char_t   *mUnescapedPtr;
} ulnEscape;

typedef ACI_RC (*ulnUnescape)(ulnEscape  *aSelf,
                              acp_char_t *aEscapePtr,
                              acp_char_t *aEnd,
                              acp_char_t *aPtr);

typedef struct ulnEscapeModule
{
    const acp_char_t *mIdentifier;
    ulnUnescape  mUnescape;
} ulnEscapeModule;


void ulnEscapeInitialize(ulnEscape *aEscape);
void ulnEscapeFinalize  (ulnEscape *aEscape);


ACI_RC ulnEscapeUnescapeByLen(ulnEscape    *aEscape,
                              acp_char_t   *aSql,
                              acp_sint32_t  aLen);
ACI_RC ulnEscapeUnescapeByPtr(ulnEscape  *aEscape,
                              acp_char_t *aEscapePtr,
                              acp_char_t *aEnd);

acp_char_t*  ulnEscapeUnescapedSql(ulnEscape  *aEscape);
acp_sint32_t ulnEscapeUnescapedLen(ulnEscape  *aEscape);

ACI_RC ulnEscapeUnescapeDate(ulnEscape  *aSelf,
                             acp_char_t *aEscapePtr,
                             acp_char_t *aEnd,
                             acp_char_t *aPtr);
ACI_RC ulnEscapeUnescapeTime(ulnEscape  *aSelf,
                             acp_char_t *aEscapePtr,
                             acp_char_t *aEnd,
                             acp_char_t *aPtr);
ACI_RC ulnEscapeUnescapeTimestamp(ulnEscape  *aSelf,
                                  acp_char_t *aEscapePtr,
                                  acp_char_t *aEnd,
                                  acp_char_t *aPtr);
ACI_RC ulnEscapeUnescapeProcedureCall(ulnEscape  *aSelf,
                                      acp_char_t *aEscapePtr,
                                      acp_char_t *aEnd,
                                      acp_char_t *aPtr);

ulnEscapeModule* ulnEscapeFindModule(acp_char_t  *aEscapePtr,
                                     acp_char_t  *aEnd,
                                     acp_char_t **aPtr);

ACI_RC ulnEscapeExtendBuffer(ulnEscape    *aEscape,
                             acp_sint32_t  aSizeDelta);

ACI_RC ulnEscapeWriteString(ulnEscape        *aEscape,
                            const acp_char_t *aStr);
ACI_RC ulnEscapeWriteToPtr(ulnEscape  *aEscape,
                           acp_char_t *aPtr);



#endif /* _ULN_ESCAPE_H_ */
