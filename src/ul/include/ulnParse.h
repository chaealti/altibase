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

#ifndef _ULN_PARSE_H_
#define _ULN_PARSE_H_ 1

ACP_EXTERN_C_BEGIN

ACP_INLINE acp_sint32_t ulnParseIndexOf( const acp_char_t *aSrcStr,
                                         acp_sint32_t      aSrcStrSize,
                                         acp_sint32_t      aStartPos,
                                         acp_char_t        aFindChar )
{
    acp_sint32_t  i;  

    for (i = aStartPos; i < aSrcStrSize; i++)
    {   
        if (aSrcStr[i] == aFindChar)
        {
            break;
        }
    }   

    return i;
}

ACP_INLINE acp_sint32_t ulnParseIndexOfNonWhitespace( const acp_char_t *aSrcStr,
                                                      acp_sint32_t      aSrcStrSize,
                                                      acp_sint32_t      aStartPos )
{
    acp_sint32_t  i;  

    for (i = aStartPos; i < aSrcStrSize; i++)
    {   
        if (acpCharIsSpace(aSrcStr[i]) == ACP_FALSE)
        {
            break;
        }
    }   

    return i;
}

ACP_INLINE acp_sint32_t ulnParseIndexOfIdEnd( const acp_char_t *aSrcStr,
                                              acp_sint32_t      aSrcStrSize,
                                              acp_sint32_t      aStartPos )
{
    acp_sint32_t i;

    if (aSrcStr[aStartPos] == '_' || acpCharIsAlpha(aSrcStr[aStartPos]))
    {   
        for (i = aStartPos + 1; i < aSrcStrSize; i++)
        {
            if (aSrcStr[i] != '_' && acpCharIsAlnum(aSrcStr[i]) == ACP_FALSE)
            {
                break;
            }
        }
    }   
    else
    {   
        i = aStartPos;
    }   

    return i;
}

typedef enum ulnConnStrParseCallbackEvent
{
    ULN_CONNSTR_PARSE_EVENT_GO,
    ULN_CONNSTR_PARSE_EVENT_IGNORE,
    ULN_CONNSTR_PARSE_EVENT_ERROR
} ulnConnStrParseCallbackEvent;

#define ULN_CONNSTR_PARSE_RC_SUCCESS 0x01
#define ULN_CONNSTR_PARSE_RC_STOP    0x02
#define ULN_CONNSTR_PARSE_RC_ERROR   0x04

/**
 * ulnConnStrParse()�� ���� �ݹ�
 *
 * @param[in]  aFnContext  function context
 * @param[in]  aKey        Ű
 * @param[in]  aKeyLen     Ű ����
 * @param[in]  aVal        ��
 * @param[in]  aValLen     �� ����
 * @param[in]  aFilter     ����
 *
 * @return ������ ���� ������ ������ �÷���
 *         - ULN_CONNSTR_PARSE_SUCCESS : ó���� �� ����
 *         - ULN_CONNSTR_PARSE_STOP    : �� �Ľ��� �ʿ� ����
 *         - ULN_CONNSTR_PARSE_ERROR   : ������ �߻��߰�, aFnContext�� ���� ������ ������
 */
typedef acp_sint32_t (*ulnConnStrParseCallback)( void                         *aContext,
                                                 ulnConnStrParseCallbackEvent  aEvent,
                                                 acp_sint32_t                  aPos,
                                                 const acp_char_t             *aKey,
                                                 acp_sint32_t                  aKeyLen,
                                                 const acp_char_t             *aVal,
                                                 acp_sint32_t                  aValLen,
                                                 void                         *aFilter );

acp_sint32_t ulnConnStrParse( void                    *aContext,
                              const acp_char_t        *aConnStr,
                              acp_sint16_t             aConnStrLen,
                              ulnConnStrParseCallback  aCallback,
                              void                    *aFilter );

/* PROJ-1538 IPv6 */
ACP_INLINE acp_bool_t ulnParseIsBracketedAddress( acp_char_t   *aAddress,
                                                  acp_sint32_t  aAddressLen )
{
    if ( aAddress != NULL &&
         aAddressLen > 2 &&
         aAddress[0] == '[' &&
         aAddress[aAddressLen - 1] == ']')
    {
        return ACP_TRUE;
    }   
    else
    {   
        return ACP_FALSE;
    }
}

typedef enum ulnConnStrAttrType
{
    ULN_CONNSTR_ATTR_TYPE_INVALID = -1,
    ULN_CONNSTR_ATTR_TYPE_NONE,
    ULN_CONNSTR_ATTR_TYPE_NORMAL,
    ULN_CONNSTR_ATTR_TYPE_ENCLOSED,
    ULN_CONNSTR_ATTR_TYPE_NEED_TO_ENCLOSE
} ulnConnStrAttrType;

ulnConnStrAttrType ulnConnStrGetAttrType( const acp_char_t *aAttrVal );

acp_rc_t ulnConnStrAppendStrAttr( acp_char_t       *aConnStr,
                                  acp_size_t        aConnStrSize,
                                  const acp_char_t *aAttrKey,
                                  const acp_char_t *aAttrVal );

ACP_EXTERN_C_END

#endif /* _ULN_PARSE_H_ */

