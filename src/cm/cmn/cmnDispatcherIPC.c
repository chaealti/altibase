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

#if !defined(CM_DISABLE_IPC)

ACI_RC cmnDispatcherWaitLinkIPC(cmnLink      *aLink,
                                cmnDirection  aDirection,
                                acp_time_t    aTimeout)
{
    ACI_RC sRet = ACI_SUCCESS;

    // bug-27250 free Buf list can be crushed when client killed
    if (aDirection == CMN_DIRECTION_WR)
    {
        // receiver�� �۽� ��� ��ȣ�� �ٶ����� ���� ���
        // cmiWriteBlock���� protocol end packet �۽Ž�
        // pending block�� �ִ� ��� �� �ڵ� ����
        if (aTimeout == ACP_TIME_INFINITE)
        {
            // client
            sRet = cmnLinkPeerWaitSendClientIPC(aLink);
        }
        // cmiWriteBlock���� �۽� ��� list�� �Ѿ ��� ����.
        else
        {
            acpSleepMsec(1); // wait 1 msec
        }
    }
    return sRet;
}


#endif
