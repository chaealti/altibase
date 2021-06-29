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
 
#ifndef _O_SVR_DEF_H_
#define _O_SVR_DEF_H_ 1

#include <smDef.h>

#define SVR_LSN_BEFORE_FIRST      (NULL)

typedef struct svrLog             svrLog;
typedef struct svrLogEnv          svrLogEnv;
typedef struct svrLogRec          svrLogRec;
typedef svrLogRec*                svrLSN;
typedef struct svrLogPage         svrLogPage;

typedef IDE_RC (*svrUndoFunc)(svrLogEnv *aEnv,
                              svrLog    *aLog,
                              svrLSN     aSubLSN);


/* svrLogRec�� svrLog�� ������ */
/* svrLogRec�� svrLogMgr ���ο��� ����ϴ� �ڷᱸ���μ�,
   head�� body�� �����ȴ�.
   svrLog�� user log�� �ֻ��� Ÿ�����μ�
   mType�� ������ �ִ�. svrLogMgr�� readLog, writeLog��
   ��� Ÿ������ ���ȴ�. �׸��� svrLog�� svrLogRec�� body�� �ش��Ѵ�. */
typedef struct svrLog
{
    svrUndoFunc     mUndo;
} svrLog;

typedef struct svrLogEnv
{
    svrLogPage     *mHeadPage;
    svrLogPage     *mCurrentPage;

    /* mPageOffset�� �α� ���ڵ尡 ��ϵ� ��ġ�� ����Ų��.
       mAlignForce�� ID_TRUE�̸� mPageOffset��
       �׻� align�� �����̾�� �Ѵ�. */
    UInt            mPageOffset;
    svrLogRec      *mLastLogRec;

    /* �������� ����ߴ� sub log record */
    svrLogRec      *mLastSubLogRec;
    idBool          mAlignForce;
    UInt            mAllocPageCount;

    /* �� Env�� ����� transaction�� ����� ó�� LSN */
    svrLSN          mFirstLSN;
} svrLogEnv;

#endif
