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

#include <cmAllClient.h>


void *cmnLinkPeerGetUserPtr(cmnLinkPeer *aLink)
{
    return aLink->mUserPtr;
}

void cmnLinkPeerSetUserPtr(cmnLinkPeer *aLink, void *aUserPtr)
{
    aLink->mUserPtr = aUserPtr;
}

/* BUG-44271 */
/**
 * ip �Ǵ� hostname�� ���� �ּ� ������ ��´�.
 * ���⼭ ���� ������ �ݵ�� acpInetFreeAddrInfo(acp_inet_addr_info_t*)�� �����ؾ� �Ѵ�.
 *
 * @param[out] aAddr     �ּ� ������ ���� ������ ����
 * @param[out] aIsIPAddr hostname�� �ƴ� ip���� ����
 * @param[in]  aServer   ip �Ǵ� hostname ���ڿ�
 * @param[in]  aPort     port number
 *
 * @return �����ϸ� ACI_SUCCESS, �ƴϸ� ACI_FAILURE
 */
ACI_RC cmnGetAddrInfo(acp_inet_addr_info_t **aAddr,
                      acp_bool_t            *aIsIPAddr,
                      const acp_char_t      *aServer,
                      acp_sint32_t           aPort)
{
    acp_inet_addr_info_t  *sAddrInfo    = NULL;
    acp_uint8_t            sTempAddr[ACI_SIZEOF(acp_sock_in6_addr_t)];
    acp_bool_t             sIsIPAddr    = ACP_FALSE;
    acp_sint32_t           sAddrFlag    = 0;
    acp_sint32_t           sAddrFamily  = 0;
    acp_sint32_t           sFoundIdx    = 0;
    acp_char_t             sPortStr[ACP_INET_IP_PORT_MAX_LEN];
    acp_rc_t               sRC          = ACP_RC_SUCCESS;
    acp_char_t            *sErrStr      = NULL;
    acp_char_t             sErrMsg[256] = {'\0', };

    /* sun 2.8 not defined */
#if defined(AI_NUMERICSERV)
    sAddrFlag = ACP_INET_AI_NUMERICSERV; /* port_no */
#endif

    /* check to see if ipv4 address. ex) ddd.ddd.ddd.ddd */
    if (acpInetStrToAddr(ACP_AF_INET,
                         (acp_char_t *)aServer,
                         sTempAddr) == ACP_RC_SUCCESS)
    {
        sIsIPAddr   = ACP_TRUE;
        sAddrFamily = ACP_AF_INET;
        sAddrFlag  |= ACP_INET_AI_NUMERICHOST;
    }
    /* check to see if ipv6 address. ex) h:h:h:h:h:h:h:h */
    /* bug-30835: support link-local address with zone index.
     *  before: acpInetStrToAddr(inet6)
     *  after : acpCStrFindChar(':')
     *  acpInetStrToAddr does not support zone index form. */
    else if (acpCStrFindChar((acp_char_t *)aServer,
                             (acp_char_t)':',
                             &sFoundIdx,
                             0,
                             0) == ACP_RC_SUCCESS)
    {
        sIsIPAddr   = ACP_TRUE;
        sAddrFamily = ACP_AF_INET6;
        sAddrFlag  |= ACP_INET_AI_NUMERICHOST;
    }
    /* hostname format. ex) db.server.com */
    else
    {
        sIsIPAddr   = ACP_FALSE;
        sAddrFamily = ACP_AF_UNSPEC;
    }

    acpSnprintf(sPortStr, ACI_SIZEOF(sPortStr), "%"ACI_UINT32_FMT"", aPort);
    sRC = acpInetGetAddrInfo(&sAddrInfo, (acp_char_t *)aServer, sPortStr,
                             ACP_SOCK_STREAM, sAddrFlag, sAddrFamily);
    ACI_TEST_RAISE(ACP_RC_NOT_SUCCESS(sRC) || (sAddrInfo == NULL),
                   GetAddrInfoError);

    *aAddr = sAddrInfo;
    if (aIsIPAddr != NULL)
    {
        *aIsIPAddr = sIsIPAddr;
    }

    return ACI_SUCCESS;

    ACI_EXCEPTION(GetAddrInfoError);
    {
        (void) acpInetGetStrError(sRC, &sErrStr);
        if (sErrStr == NULL)
        {
            acpSnprintf(sErrMsg, ACI_SIZEOF(sErrMsg), "%"ACI_INT32_FMT, sRC);
        }
        else
        {
            acpSnprintf(sErrMsg, ACI_SIZEOF(sErrMsg), "%s", sErrStr);
        }
        ACI_SET(aciSetErrorCode(cmERR_ABORT_GETADDRINFO_ERROR, sErrMsg));
    }
    ACI_EXCEPTION_END;

    if (sAddrInfo != NULL)
    {
        acpInetFreeAddrInfo(sAddrInfo);
    }
    return ACI_FAILURE;
}
