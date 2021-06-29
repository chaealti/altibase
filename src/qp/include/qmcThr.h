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
 

/***********************************************************************
 * $Id: qmcThr.h 82075 2018-01-17 06:39:52Z jina.kim $
 **********************************************************************/

#ifndef _O_QMC_THR_H_
#define _O_QMC_THR_H_ 1

#include <ide.h>
#include <idTypes.h>

class qmcThrObj;

typedef IDE_RC (*qmcThrFunc)(qmcThrObj* aArg);

class qmcThrObj : public idtBaseThread
{
public:
    iduListNode     mNode;
    iduMutex        mMutex;       /* mCV �� �Բ� ���� */
    iduCond         mCV;          /* ����ȭ�� ���� cond var */
    UInt            mStatus;      /* thread ���¸� ��Ÿ�� */
    idBool          mStopFlag;    /* thread ���߱� ���� flag */
    void*           mPrivateArg;  /* 1. PRLQ ���� data plan */
                                  /* 2. PROJ-2451 DBMS_CONCURRENT_EXEC
                                   *    qscExecInfo */
    qmcThrFunc      mRun;         /* thread �� ������ �Լ� */
    UInt            mID;          /* thread id */
    UInt            mErrorCode;   /* error �� ������ set */
    SChar*          mErrorMsg;

    qmcThrObj() : idtBaseThread()
    {
        mNode.mObj = this;
    }

    virtual IDE_RC  initializeThread();
    virtual void    run();
    virtual void    finalizeThread();
};

typedef struct qmcThrMgr
{
    UInt        mThrCnt;
    iduList     mUseList;
    iduList     mFreeList;
    iduMemory * mMemory;   /* original stmt->qmxMem */
} qmcThrMgr;

/* qmcThrObj.mStatus */
#define QMC_THR_STATUS_NONE           (0) /* thread start ���� */
#define QMC_THR_STATUS_CREATED        (1) /* thread start ���� wait ���� */
#define QMC_THR_STATUS_WAIT           (2) /* thread start �� ��� */
#define QMC_THR_STATUS_RUN            (3) /* thread �۾� �� */
#define QMC_THR_STATUS_END            (4) /* thread ���� (error �߻� or stopflag) */
#define QMC_THR_STATUS_JOINED         (5) /* thread ����, join �Ϸ� */
#define QMC_THR_STATUS_FAIL           (6) /* thread start ���� */
#define QMC_THR_STATUS_RUN_FOR_JOIN   (7) /* thread join�ϱ� ���� �۾� �� ( BUG-41627 ) */

/* qmcThrObj.mErrorCode */
#define QMC_THR_ERROR_CODE_NONE (0)

IDE_RC qmcThrObjCreate(qmcThrMgr* aMgr, UInt aReserveCnt);
IDE_RC qmcThrObjFinal(qmcThrMgr* aMgr);

IDE_RC qmcThrGet(qmcThrMgr  * aMgr,
                 qmcThrFunc   aFunc,
                 void       * aPrivateArg,
                 qmcThrObj ** aThrObj);
void qmcThrReturn(qmcThrMgr* aMgr, qmcThrObj* aThrObj);

IDE_RC qmcThrWakeup(qmcThrObj* aThrObj, idBool* aIsSuccess);
IDE_RC qmcThrWakeupForJoin(qmcThrObj* aThrObj, idBool* aIsSuccess);   /* BUG-41627 */
IDE_RC qmcThrJoin(qmcThrMgr* aMgr);

#endif /* _O_QMC_THR_H_ */
