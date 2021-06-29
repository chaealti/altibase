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

#ifndef _O_ULN_LOB_CACHE_H_
#define _O_ULN_LOB_CACHE_H_ 1

#include <ulnPrivate.h>

typedef enum ulnLobCacheErr {
    LOB_CACHE_NO_ERR        = 0,
    LOB_CACHE_INVALID_RANGE = 1
} ulnLobCacheErr;

/**
 *  ulnLobCacheCreate
 *
 *  @aLobCache          : ulnLobCache�� ��ü
 *
 *  = LOB�� ĳ���� �� �ֵ��� ������ �����Ѵ�.
 *  = �����ϸ� ACI_SUCCESS, �����ϸ� ACI_FAILURE
 */
ACI_RC ulnLobCacheCreate(ulnLobCache **aLobCache);

/**
 *  ulnLobCacheReInitialize
 *
 *  = ������ �Ҵ�� �޸𸮸� �����Ѵ�.
 */
ACI_RC ulnLobCacheReInitialize(ulnLobCache *aLobCache);

/**
 *  ulnLobCacheDestroy
 *
 *  = ulnLobCache ��ü�� �Ҹ��Ѵ�.
 */
void   ulnLobCacheDestroy(ulnLobCache **aLobCache);

/**
 *  ulnLobCacheGetErr
 *
 *  ���ο��� �߻��� ������ �����´�.
 */
ulnLobCacheErr ulnLobCacheGetErr(ulnLobCache  *aLobCache);

/**
 *  ulnLobCacheAdd
 *
 *  @aValue     : Caching�� LOB Data
 *  @aLength    : LOB Data ����
 *
 *  aValue�� NULL�� ��쿡�� LOB Data���̸� �����Ѵ�.
 *  �� �Ӱ�ġ���� LOB Data�� ũ���� LOB Data ���̴� ������
 *  SQLGetLobLength()���� Ȱ���Ѵ�.
 */
ACI_RC ulnLobCacheAdd(ulnLobCache  *aLobCache,
                      acp_uint64_t  aLocatorID,
                      acp_uint8_t  *aValue,
                      acp_uint32_t  aLength,
                      acp_bool_t    aIsNull);

/**
 *  ulnLobCacheRemove
 *
 *  aLocatorID�� �ش��ϴ� �����͸� �����Ѵ�.
 */
ACI_RC ulnLobCacheRemove(ulnLobCache  *aLobCache,
                         acp_uint64_t  aLocatorID);

/**
 *  ulnLobCacheGetLob
 *
 *  @aFromPos   : LOB�� ���� Offset
 *  @aForLength : LOB�� ����
 *  @aContext   : LOB Interface�� �����Ͱ� ����Ǿ� �ִ�.
 *
 *  Cache�Ǿ� �ִ� LOB �����͸� LOB Buffer(User Buffer)�� ��ȯ�Ѵ�.
 */
ACI_RC ulnLobCacheGetLob(ulnLobCache  *aLobCache,
                         acp_uint64_t  aLocatorID,
                         acp_uint32_t  aFromPos,
                         acp_uint32_t  aForLength,
                         ulnFnContext *aContext);

/**
 *  ulnLobCacheGetLobLength
 *
 *  aLocatorID�� �ش��ϴ� LOB Data�� ���������� aLength�� ��ȯ�Ѵ�.
 */
ACI_RC ulnLobCacheGetLobLength(ulnLobCache  *aLobCache,
                               acp_uint64_t  aLocatorID,
                               acp_uint32_t *aLength,
                               acp_bool_t   *aIsNull);

#endif /* _O_ULN_LOB_CACHE_H_ */
