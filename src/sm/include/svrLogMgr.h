/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef _O_SVR_LOGMGR_H_
#define _O_SVR_LOGMGR_H_ 1

#include <svrDef.h>

class svrLogMgr
{
  public:
    /* svrLogMgr�� ����ϱ� ���� static ������� �ʱ�ȭ�Ѵ�. */
    static IDE_RC initializeStatic();

    /* svrLogMgr�� ����ߴ� �ڿ��� �����Ѵ�. */
    static IDE_RC destroyStatic();

    /* writeLog�� �ϱ� ���� �ʿ��� �ڷᱸ���� �ʱ�ȭ�Ѵ�. */
    static IDE_RC initEnv(svrLogEnv *aEnv, idBool aAlignForce);

    /* writeLog�� �ϸ鼭 ������ �α� ���� �޸𸮸� ��� �����Ѵ�. */
    static IDE_RC destroyEnv(svrLogEnv *aEnv);

    /* svrLogEnv���� �α׸� ����ϱ� ���� �Ҵ��� �޸��� ���� */
    static UInt getAllocMemSize(svrLogEnv *aEnv);

    /* �α� ���ۿ� �α׸� ����Ѵ�. */
    static IDE_RC writeLog(svrLogEnv *aEnv, svrLog *aLogData, UInt aLogSize);

    /* �α� ���ۿ� sub �α׸� ����Ѵ�. */
    static IDE_RC writeSubLog(svrLogEnv *aEnv, svrLog *aLogData, UInt aLogSize);

    /* ����ڰ� �Ҵ��� �޸� ������ �α� ������ �о�  �����Ѵ�. */
    static IDE_RC readLogCopy(svrLogEnv *aEnv,
                              svrLSN     aLSNToRead,
                              svrLog    *aBufToLoadAt,
                              svrLSN    *aUndoNextLSN,
                              svrLSN    *aNextSubLSN);

    /* �α� ������ ��ϵ� �޸� �ּҸ� ��ȯ�Ѵ�. */
    static IDE_RC readLog(svrLogEnv *aEnv,
                          svrLSN     aLSNToRead,
                          svrLog   **aLogData,
                          svrLSN    *aUndoNextLSN,
                          svrLSN    *aNextSubLSN);

    /* �־��� LSN ������ ��� �α׵��� ����� �α� ������ �޸𸮸� �����Ѵ�.*/
    static IDE_RC removeLogHereafter(svrLogEnv *aEnv, svrLSN aThisLSN);

    /* ����ڰ� ����� ������ �α� ���ڵ��� LSN�� ���Ѵ�. */
    static svrLSN getLastLSN(svrLogEnv *aEnv);

    /* ���� log env�� ���� ������ ���� �ѹ��̶� �ִ��� ���´�. */
    static idBool isOnceUpdated(svrLogEnv *aEnv);
};

#endif /* _O_SVR_LOGMGR_H_ */
