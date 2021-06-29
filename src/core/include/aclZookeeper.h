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

#ifndef _O_ACL_ZKC_H_
#define _O_ACL_ZKC_H_ 1

#include <acpTypes.h>

#if defined(ALTI_CFG_OS_LINUX)
#include <zookeeper.h>
#include <zookeeper.jute.h>
#include <zk_adaptor.h>
#endif

/* Zookeeper C Client�� �������� �����ϹǷ� ������ ������ ���������� aclZookeeper.c�� ������ ������ *
 * ������ �� OS������ ���¸� �ִ� ����ִ� ����(aclZookeeperstub.cpp)�� ��� compile�Ѵ�.           */

ACP_EXTERN_C_BEGIN

#define ZKC_CONNECT_RETRY_MAX_CNT (10)

#if defined(ALTI_CFG_OS_LINUX)
typedef zhandle_t   aclZK_Handler;
#else
/* �ش� �κ��� LINUX�� ��񿡼� compile�� �����ϱ� ���� �߰��� �κ��ϻ� ���� ������� ���谡 ����. */
typedef acp_sint32_t aclZK_Handler;
typedef acp_sint32_t watcher_fn;
typedef acp_sint32_t String_vector;
typedef acp_sint32_t zoo_op_t;
typedef acp_sint32_t zoo_op_result_t;
#endif

typedef enum
{
    ZKC_SUCCESS = 0,
    ZKC_CONNECTION_INFO_MISSING,
    ZKC_CONNECTION_FAIL,
    ZKC_CONNECTION_MISSING,
    ZKC_ZOOCFG_NO_EXIST,
    ZKC_NODE_EXISTS,
    ZKC_NO_NODE,
    ZKC_WAITING_TIMEOUT,
    ZKC_DB_VALIDATE_FAIL,
    ZKC_VALIDATE_FAIL,
    ZKC_STATE_VALIDATE_FAIL,
    ZKC_LOCK_ERROR,
    ZKC_DATA_TOO_LARGE,
    ZKC_META_CORRUPTED,
    ZKC_ETC_ERROR
} ZKCResult;

/* ZKC �Լ��� ���ϰ��� �����ڵ�� �ٲ��ִ� �Լ� */
ZKCResult aclZKC_result( acp_sint32_t );

/* ����� ZKS�� ������ ZKS�� �����ϴ� �Լ� */
ZKCResult aclZKC_connect( aclZK_Handler **, acp_char_t *, acp_uint32_t );

/* ZKS���� ������ ���� �Լ� */
void aclZKC_disconnect( aclZK_Handler * );

/* ZKS�� �����͸� �����ϴ� �Լ� */
ZKCResult aclZKC_setInfo( aclZK_Handler *, acp_char_t *, acp_char_t *, acp_sint32_t );

/* ZKS�� ���丮(���)�� �߰��ϴ� �Լ� */
ZKCResult aclZKC_insertNode( aclZK_Handler *, acp_char_t *, acp_char_t *, acp_sint32_t, acp_bool_t );

/* ZKS�� �����͸� �������� �Լ� */
ZKCResult aclZKC_getInfo( aclZK_Handler *, acp_char_t *, acp_char_t *, acp_sint32_t *);

/* ZKS�� Ư�� ����� �ڽ��� �������� �Լ� */
ZKCResult aclZKC_getChildren( aclZK_Handler *, acp_char_t *, String_vector * );

/* ZKS�� ���丮�� �����ϴ� �Լ� */
ZKCResult aclZKC_deleteNode( aclZK_Handler *, acp_char_t * );

/* ZKS�� �����͸� �����ϴ� �Լ� */
ZKCResult aclZKC_deleteInfo( aclZK_Handler *, acp_char_t * );

/* ZKS�� Ư�� ����� ���縦 Ȯ���ϴ� �Լ� */
ZKCResult aclZKC_existNode( aclZK_Handler *, acp_char_t * );

/* multi op�� �����ϴ� �Լ� */
ZKCResult aclZKC_multiOp( aclZK_Handler *, acp_sint32_t, zoo_op_t *, zoo_op_result_t * );

/* Ư�� ����� �����Ϳ� watch�� �Ŵ� �Լ� */
ZKCResult aclZKC_Watch_Data( aclZK_Handler *, watcher_fn, acp_char_t * );

/* Ư�� ����� �߰�/������ watch�� �Ŵ� �Լ� */
ZKCResult aclZKC_Watch_Node( aclZK_Handler *, watcher_fn, acp_char_t * );

ACP_EXTERN_C_END

#endif
