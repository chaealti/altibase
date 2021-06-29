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

#ifndef _O_ULN_PROPERTIES_H_
#define _O_ULN_PROPERTIES_H_ 1

#include <ulnPrivate.h>

#define IPCDA_DEF_CLIENT_SLEEP_TIME 400
#define IPCDA_DEF_CLIENT_EXP_CNT    0
#define IPCDA_DEF_MESSAGEQ_TIMEOUT  100

struct ulnProperties {
    ACP_STR_DECLARE_DYNAMIC(mHomepath);
    ACP_STR_DECLARE_DYNAMIC(mConfFilepath);
    ACP_STR_DECLARE_DYNAMIC(mUnixdomainFilepath);
    ACP_STR_DECLARE_DYNAMIC(mIpcFilepath);
    ACP_STR_DECLARE_DYNAMIC(mIPCDAFilepath);
    acp_uint32_t  mIpcDaClientSleepTime;
    acp_uint32_t  mIpcDaClientExpireCount;
    acp_uint32_t  mIpcDaClientMessageQTimeout;
};

/**
 *  ulnPropertiesCreate
 *
 *  @aProperties : ulnProperties�� ��ü
 *
 *  ���� ȯ�������� �о� aProperties�� �����Ѵ�.
 *
 *  �켱����
 *  ALTIBASE ȯ�溯�� > altibase.properties > �ڵ忡 ������ ��
 */
void ulnPropertiesCreate(ulnProperties *aProperties);

/**
 *  ulnPropertiesDestroy
 *
 *  @aProperties : ulnProperties�� ��ü
 *
 *  aProperties�� �޸𸮸� ��ȯ�Ѵ�.
 */
void ulnPropertiesDestroy(ulnProperties *aProperties);

/**
 *  ulnPropertiesExpandValues
 *
 *  @aProperties : ulnProperties�� ��ü
 *  @aDest       : Ÿ�� ����
 *  @aDestSize   : Ÿ�� ���� ������
 *  @aSrc        : �ҽ� ����
 *  @aSrcLen     : �ҽ� ���ڿ� ����
 *
 *  ?�� $ALTIBASE_HOME���� Ȯ���Ѵ�.
 */
ACI_RC ulnPropertiesExpandValues( ulnProperties *aProperties,
                                  acp_char_t    *aDest,
                                  acp_sint32_t   aDestSize,
                                  acp_char_t    *aSrc,
                                  acp_sint32_t   aSrcLen );

#endif /* _O_ULN_PROPERTIES_H_ */
