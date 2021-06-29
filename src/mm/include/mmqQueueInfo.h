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

#ifndef _O_MMQ_QUEUE_INFO_H_
#define _O_MMQ_QUEUE_INFO_H_

#include <idl.h>
#include <ide.h>
#include <idu.h>
#include <smiDef.h>
#include <mmqManager.h>


class mmcTask;

class mmqQueueInfo
{
private:
    UInt       mTableID;

    //PROJ-1677 DEQ
    /*
                                    (sm-commit& mm-commit )
                                     |
                                     V                         
      dequeue statement begin  ----------------- > execute .... 
                                          ^
                                          |
                                        get queue stamp (session�� queue stamp�� ����)
                                        
      dequeue statement begin�� execute �ٷ� ������ queue item�� commit�Ǿ��ٸ�,
      dequeue execute�ÿ�  �ش� queue item�� MVCC������ ���� ��� ��� ���·� ����.
      �׸��� session�� queue timestamp�� queue timestamp�� ���Ƽ�  ���� enqueue
      event�� �߻��Ҷ� ����  queue�� ����Ÿ�� �������� �ұ��ϰ� dequeue�� �Ҽ� ���� .
      �̹����� �ذ� �ϱ� ���Ͽ�  commitSCN�� �ξ���.
                                          
     */
    smSCN      mCommitSCN;
    

    
    idBool     mQueueDropFlag;

    iduList    mSessionList;

    iduMutex   mMutex;

    iduCond    mDequeueCond;

public:
    IDE_RC initialize(UInt aTableID);
    IDE_RC destroy();

    void   lock();
    void   unlock();
    //fix BUG-19321
    void   addSession(mmcSession *aSession);
    void   removeSession(mmcSession *aSession);
    //fix BUG-19320
    void   wakeup4DeqRollback();
    void   wakeup(smSCN* aCommitSCN);    
    inline idBool isQueueReady4Session(smSCN* aSessionDEQViewSCN);

public:
    UInt   getTableID();

    idBool isSessionListEmpty();

    void   setQueueDrop();

public:
    IDE_RC timedwaitDequeue(ULong aWaitSec,idBool* aTimeOut);
    IDE_RC broadcastEnqueue();

public:
    static UInt hashFunc(void *aKey);
    static SInt compFunc(void *aLhs, void *aRhs);
};


inline UInt mmqQueueInfo::getTableID()
{
    return mTableID;
}

inline idBool mmqQueueInfo::isSessionListEmpty()
{
    return IDU_LIST_IS_EMPTY(&mSessionList);
}

inline void mmqQueueInfo::setQueueDrop()
{
    mQueueDropFlag = ID_TRUE;
}

//PROJ-1677 DEQ call�� queue info�� lock�� ���� ���¿���
//��  �Լ��� ȣ���Ѵ�.
inline idBool mmqQueueInfo::isQueueReady4Session(smSCN* aSessionDEQViewSCN)
{
    idBool sRetVal;
    if( SM_SCN_IS_GT(&mCommitSCN,aSessionDEQViewSCN))
    {
        sRetVal = ID_TRUE;
    }
    else
    {
        //fix BUG-19321
        if(mQueueDropFlag == ID_TRUE)
        {
            sRetVal = ID_TRUE;
        }
        else
        {    
            sRetVal = ID_FALSE;
        }
    }//else   
    return sRetVal;
}



#endif
