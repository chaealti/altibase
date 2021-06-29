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

#include <idtCPUSet.h>
#include <mmm.h>
#include <mmErrorCode.h>
#include <mmcSession.h>
#include <mmcTask.h>
#include <mmtServiceThread.h>
#include <mmtSessionManager.h>
#include <mmtThreadManager.h>
#include <mmuProperty.h>
#include <mmm.h>

#if defined(WRS_VXWORKS) 
# define MMT_SERVICE_THREAD_MIN_POLL_TIMEOUT 100000
#else
# define MMT_SERVICE_THREAD_MIN_POLL_TIMEOUT 50
#endif
/* PROJ-2108 Dedicated thread mode which uses less CPU
 * This timeout(765432) is a magic number to notify
 * dispatcher that the server is in dedicated thread mode
 */
# define MMT_SERVICE_THREAD_DEDICATED_POLL_TIMEOUT 765432

mmtServiceThread::mmtServiceThread() : idtBaseThread()
{
}

//fix BUG-19464
IDE_RC mmtServiceThread::initialize(mmcServiceThreadType aServiceThreadType,
                                    mmtServiceThreadStartFunc aServiceThreadStartFunc)
{
    this->setIsServiceThread( ID_TRUE );
    mInfo.mServiceThreadType = aServiceThreadType;
    mInfo.mServiceThreadID   = 0;
    mInfo.mIpcID             = 0;
    mInfo.mState             = MMT_SERVICE_THREAD_STATE_NONE;
    mInfo.mStartTime         = 0;
    mInfo.mSessionID         = 0;
    mInfo.mStmtID            = 0;
    mInfo.mTaskCount         = 0;
    mInfo.mReadyTaskCount    = 0;
    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       Load balance history���� �� �ʵ��߰�.
    */

    mInfo.mInTaskCntFromIdle    = 0;
    mInfo.mOutTaskCntInIdle     = 0;
    mInfo.mInTaskCntFromBusy    = 0;
    mInfo.mOutTaskCntInBusy     = 0;
    mInfo.mBusyExperienceCnt    = 0;
    mInfo.mLifeSpan = mmuProperty::getServiceThrInitialLifeSpan();
    mInfo.mCurAddedTasks        = 0;
    IDV_TIME_INIT(&mInfo.mExecuteBegin);
    IDV_TIME_INIT(&mInfo.mExecuteEnd);

    /* BUG-38384 A task in a service thread can be a orphan */
    mInfo.mRemoveAllTasks = ID_FALSE;

    mStatement   = NULL;

    mLoopCounter = 1;
    mLoopCheck   = 0;

    mRun         = ID_FALSE;
    mErrorFlag   = ID_FALSE;

    IDU_LIST_INIT_OBJ(&mThreadListNode, this);
    IDU_LIST_INIT_OBJ(&mCheckThreadListNode, this);

    IDU_LIST_INIT(&mNewTaskList);
    IDU_LIST_INIT(&mWaitTaskList);
    IDU_LIST_INIT(&mReadyTaskList);
    //fix BUG-19464
    mServiceThreadStartFunc = aServiceThreadStartFunc;

    IDE_TEST(mMutex.initialize((SChar *)"MMT_SERVICE_THREAD_MUTEX",
                               IDU_MUTEX_KIND_POSIX,
                               IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);

    IDE_TEST(mNewTaskListMutex.initialize((SChar *)"MMT_SERVICE_THREAD_TASK_LIST_MUTEX",
                                          IDU_MUTEX_KIND_POSIX,
                                          IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);

    /* PROJ-2108 Dedicated thread mode which uses less CPU */
    /* BUG-37038 DEDICATED TYPE�� ��쿡�� initialize �Ѵ�. */
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_DEDICATED)
    {
        IDE_TEST(mMutexForServiceThreadSignal.initialize(
                                            (SChar *)"MMT_SERVICE_THREAD_MUTEX_FOR_SIGNAL",
                                            IDU_MUTEX_KIND_POSIX,
                                            IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);
        IDE_TEST(mServiceThreadCV.initialize() != IDE_SUCCESS);
    }
    else
    {
        /* Nothing */
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::finalize()
{
    IDE_TEST(mMutex.destroy() != IDE_SUCCESS);
    IDE_TEST(mNewTaskListMutex.destroy() != IDE_SUCCESS);

    /* PROJ-2108 Dedicated thread mode which uses less CPU */
    /* BUG-37038 DEDICATED TYPE�� ��쿡�� finalize �Ѵ�. */
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_DEDICATED)
    {
        IDE_TEST(mMutexForServiceThreadSignal.destroy() != IDE_SUCCESS);
        IDE_TEST(mServiceThreadCV.destroy() != IDE_SUCCESS);
    }
    else
    {
        /* Nothing */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::initializeThread()
{
    mDispatcher = NULL;

    switch (mInfo.mServiceThreadType)
    {
        case MMC_SERVICE_THREAD_TYPE_SOCKET:
            // BUG-24318 IPC �ϰ�� ���� ������ �߸� ���ɴϴ�.
            mInfo.mRunMode           = MMT_SERVICE_THREAD_RUN_SHARED;
            mMultiPlexingFunc = &mmtServiceThread::multiplexingAsShared;

            /* PROJ-2681 */
            if ((mmuProperty::getIBEnable() == ID_TRUE) && (mmuProperty::getIBListenerDisable() != ID_TRUE))
            {
                IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                            CMI_DISPATCHER_IMPL_IB,
                                            mmuProperty::getMaxClient()) != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                            CMI_DISPATCHER_IMPL_SOCK,
                                            mmuProperty::getMaxClient()) != IDE_SUCCESS);
            }
            break;

        /* PROJ-2108 Dedicated thread mode which uses less CPU */
        case MMC_SERVICE_THREAD_TYPE_DEDICATED:
            mInfo.mRunMode           = MMT_SERVICE_THREAD_RUN_DEDICATED;
            mMultiPlexingFunc = &mmtServiceThread::dedicatedThreadMode;

            /* PROJ-2681 */
            if ((mmuProperty::getIBEnable() == ID_TRUE) && (mmuProperty::getIBListenerDisable() != ID_TRUE))
            {
                IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                            CMI_DISPATCHER_IMPL_IB,
                                            mmuProperty::getMaxClient()) != IDE_SUCCESS);
            }
            else
            {
                IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                            CMI_DISPATCHER_IMPL_SOCK,
                                            mmuProperty::getMaxClient()) != IDE_SUCCESS);
            }
            break;

        case MMC_SERVICE_THREAD_TYPE_IPC:
            // BUG-24318 IPC �ϰ�� ���� ������ �߸� ���ɴϴ�.
            mInfo.mRunMode           = MMT_SERVICE_THREAD_RUN_DEDICATED;
            mMultiPlexingFunc = &mmtServiceThread::multiplexingAsDedicated;

            IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                        CMI_DISPATCHER_IMPL_IPC,
                                        1) != IDE_SUCCESS);
            break;

        case MMC_SERVICE_THREAD_TYPE_IPCDA:
            // BUG-24318 IPC �ϰ�� ���� ������ �߸� ���ɴϴ�.
            mInfo.mRunMode           = MMT_SERVICE_THREAD_RUN_DEDICATED;
            mMultiPlexingFunc = &mmtServiceThread::multiplexingAsDedicated;

            IDE_TEST(cmiAllocDispatcher(&mDispatcher,
                                        CMI_DISPATCHER_IMPL_IPCDA,
                                        1) != IDE_SUCCESS);
            break;

        //fix PROJ-1749
        case MMC_SERVICE_THREAD_TYPE_DA:
            break;

        default:
            IDE_ASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if (mDispatcher != NULL)
    {
        /* BUG-45240 Non-reachable */
        cmiFreeDispatcher(mDispatcher);
        mDispatcher = NULL;
    }

    return IDE_FAILURE;
}

void   mmtServiceThread::finalizeThread()
{
    //fix PROJ-1749
    if (mInfo.mServiceThreadType != MMC_SERVICE_THREAD_TYPE_DA)
    {
        IDE_DASSERT(cmiFreeDispatcher(mDispatcher) == IDE_SUCCESS);
    }
}

/* PROJ-2108 Dedicated thread mode which uses less CPU */
void mmtServiceThread::run()
{
    mmcServiceThreadType sServiceThreadType;
    UInt                 sIpcServiceThreadID;
    iduMutex*            sIpcServiceThreadMutex;
    iduCond*             sIpcServiceThreadCV;

    /*PROJ-2616*/
    UInt                 sIPCDAServiceThreadID    = 0;
    iduMutex*            sIPCDAServiceThreadMutex = NULL;
    iduCond*             sIPCDAServiceThreadCV    = NULL;

    UInt                 sState=0;        

    IDE_RC               sRC;
    /* PROJ-2108 Dedicated thread mode which uses less CPU */
    /* BUG-35356 Memory violation may occur when calculating CPU busy count */
    UInt                 sSelectedCoreIdx = 0;
    UInt                 sSelectedCoreNumber = 0;
    UInt                 sIsBound = 0;

    IDE_ASSERT(mmtThreadManager::setupDefaultThreadSignal() == IDE_SUCCESS);

    /* PROJ-2617 */
    IDE_ASSERT(ideEnableFaultMgr(ID_TRUE) == IDE_SUCCESS);

    mInfo.mThreadID  = getTid();
    mInfo.mStartTime = mmtSessionManager::getBaseTime();
    mRun             = ID_TRUE;

    mPollTimeout.set(0, mmuProperty::getMultiplexingPollTimeout());

    lockForThread();
    sState=1;	

    setState(MMT_SERVICE_THREAD_STATE_POLL);

    //fix BUG-19464
    mServiceThreadStartFunc(this,
                            mInfo.mServiceThreadType,
                            &mInfo.mServiceThreadID);

    /*
     * BUG-37038
     *
     * DEDICATED TYPE�� ��쿡�� Lock�� ȹ���Ѵ�.
     * �׷��� ������ finalize()���� mutex destroy�� ����(EBUSY)�Ѵ�.
     * IPC Service Thread������ DEDICATED TYPE������ ����ϴ� Mutex��
     * Unlock() �ϴ� �κ��� ���� �����̴�.
     */ 
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_DEDICATED)
    {
        IDE_ASSERT(mMutexForServiceThreadSignal.lock(NULL) == IDE_SUCCESS);
        mmtThreadManager::signalToThreadManager();
    }
    else
    {
        /* do nothing */
    }
    sServiceThreadType = mInfo.mServiceThreadType;

    /* PROJ-2108 Dedicated thread mode which uses less CPU
     * Bind thread to a specific CPU 
     */
    if ( mmuProperty::getIsCPUAffinity() == 1 )
    {
        sSelectedCoreIdx = mmtThreadManager::getCoreIdxToAssign();
        sSelectedCoreNumber = mmtThreadManager::getCoreNumberFromIdx( sSelectedCoreIdx );
        /* BUG-35356 Memory violation may occur when calculating CPU busy count */
        if ( this->setAffinity( sSelectedCoreNumber ) != IDE_SUCCESS )
        {
            mmtThreadManager::decreaseCoreBusyCount( sSelectedCoreIdx );
        }
        else
        {
            sIsBound = 1;
        }
    }
    else
    {
        /* do nothing */
    }

    switch (sServiceThreadType)
    {
        case MMC_SERVICE_THREAD_TYPE_SOCKET:
            while (mRun == ID_TRUE)
            {
                //PROJ-1677 �Ʒ��� ���� run-mode�� ��ȯ�ɼ� �ֱ� ������
                //shared thread run mode < ---> dedicated thread run mode 
                //�Ʒ��� ����  function pointer call���� ��ġ �Ѵ�.
                (this->*mMultiPlexingFunc)();
            }//while
            break;
            /* PROJ-2108 Dedicated thread mode which uses less CPU */
        case MMC_SERVICE_THREAD_TYPE_DEDICATED:
            sRC = mServiceThreadCV.wait(&mMutexForServiceThreadSignal);
            if (sRC != IDE_SUCCESS) 
            {
                IDE_ASSERT(mMutexForServiceThreadSignal.unlock() == IDE_SUCCESS);
                IDE_RAISE(CondWaitError);
            }
            IDE_ASSERT(mMutexForServiceThreadSignal.unlock() == IDE_SUCCESS);
            while (mRun == ID_TRUE)
            {
                (this->*mMultiPlexingFunc)();                
                if( getAssignedTasks() == 0)
                {

                    IDE_ASSERT(mMutexForServiceThreadSignal.lock(NULL) == IDE_SUCCESS);

                    sRC = mServiceThreadCV.wait(&mMutexForServiceThreadSignal);
                    if(sRC != IDE_SUCCESS) 
                    {
                        IDE_ASSERT(mMutexForServiceThreadSignal.unlock() == IDE_SUCCESS);
                        IDE_RAISE(CondWaitError);
                    }
                    IDE_ASSERT(mMutexForServiceThreadSignal.unlock() == IDE_SUCCESS);
                }
            }//while
            break;

        case MMC_SERVICE_THREAD_TYPE_IPC:
            sIpcServiceThreadID    = getIpcID();
            sIpcServiceThreadMutex = mmtThreadManager::getIpcServiceThreadMutex(sIpcServiceThreadID);
            sIpcServiceThreadCV    = mmtThreadManager::getIpcServiceThreadCV(sIpcServiceThreadID);

            IDE_ASSERT(sIpcServiceThreadMutex->lock(NULL /* idvSQL* */) == IDE_SUCCESS);

            while (mRun == ID_TRUE)
            {
                IDE_CLEAR();

                mLoopCounter++;

                if (mRun == ID_FALSE)
                {
                    break;
                }

                sRC = sIpcServiceThreadCV->wait(sIpcServiceThreadMutex);
                if(sRC != IDE_SUCCESS) 
                {
                    IDE_ASSERT(sIpcServiceThreadMutex->unlock() == IDE_SUCCESS);
                    IDE_RAISE(CondWaitError);
                }
                
                if (mRun == ID_FALSE)
                {
                    break;
                }

                getNewTask();

                if (getReadyTaskCount() > 0)
                {
                    do
                    {
                        mLoopCounter++;

                        executeTask();

                        IDV_TIME_GET(&mInfo.mExecuteEnd);

                        setState(MMT_SERVICE_THREAD_STATE_POLL);

                    } while (getReadyTaskCount() > 0);
                }
            }

            IDE_ASSERT(sIpcServiceThreadMutex->unlock() == IDE_SUCCESS);
            break;
            /*PROJ-2616*/
        case MMC_SERVICE_THREAD_TYPE_IPCDA:
            sIPCDAServiceThreadID    = getIpcID();
            sIPCDAServiceThreadMutex = mmtThreadManager::getIPCDAServiceThreadMutex(sIPCDAServiceThreadID);
            sIPCDAServiceThreadCV    = mmtThreadManager::getIPCDAServiceThreadCV(sIPCDAServiceThreadID);

            IDE_ASSERT(sIPCDAServiceThreadMutex->lock(NULL /* idvSQL* */) == IDE_SUCCESS);

            while (mRun == ID_TRUE)
            {
                IDE_CLEAR();

                mLoopCounter++;

                if (mRun == ID_FALSE)
                {
                    break;
                }

                sRC = sIPCDAServiceThreadCV->wait(sIPCDAServiceThreadMutex);
                if(sRC != IDE_SUCCESS)
                {
                    IDE_ASSERT(sIPCDAServiceThreadMutex->unlock() == IDE_SUCCESS);
                    IDE_RAISE(CondWaitError);
                }

                if (mRun == ID_FALSE)
                {
                    break;
                }

                if (getReadyTaskCount() > 0)
                {
                    do
                    {
                        mLoopCounter++;

                        /* BUG-46163 */
                        executeTask();

                        IDV_TIME_GET(&mInfo.mExecuteEnd);

                        setState(MMT_SERVICE_THREAD_STATE_POLL);

                    } while (getReadyTaskCount() > 0);
                }
            }

            IDE_ASSERT(sIPCDAServiceThreadMutex->unlock() == IDE_SUCCESS);
            break;

        default:
            IDE_ASSERT(0);
    }

    /* PROJ-2108 Dedicated thread mode which uses less CPU
     * Unbind thread from a specific CPU 
     */
    if ( sIsBound == 1 )
    {
        mmtThreadManager::decreaseCoreBusyCount( sSelectedCoreIdx );
    }
    else
    {
        /* do nothing */
    }

    mmtThreadManager::serviceThreadStopped(this);

    sState=0;	
    unlockForThread();

    return;

    IDE_EXCEPTION(CondWaitError);
    {
        /*
         * BUG-35298    [mm] hang occurs because ipc count is incorrect.
         */
    	mmtThreadManager::serviceThreadStopped(this);
        if( sState == 1 )
        {
    	    unlockForThread();
        }
        IDE_SET(ideSetErrorCode(mmERR_FATAL_THREAD_CONDITION_WAIT));
    }
    IDE_EXCEPTION_END;
}

void mmtServiceThread::stop()
{
    mRun = ID_FALSE;
}

void mmtServiceThread::addTask(mmcTask *aTask)
{
    cmiProtocolContext *sCtx = aTask->getProtocolContext();
    if (cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA)
    {
        /* PROJ-2616 */
        IDU_LIST_ADD_FIRST(&mReadyTaskList, aTask->getThreadListNode());
        addReadyTaskCount(1);
        aTask->setTaskState(MMC_TASK_STATE_READY);
    }
    else
    {
        lockForNewTaskList();
        /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
           assign�� task ���� ����.
           �ֳ��ϸ� �ش� service thread�� OS���� scheduling�Ǿ�
           getNewTask function�� ����Ǿ� ,
           task ������ �����Ǳ� ����...
        */

        mInfo.mCurAddedTasks++;
        mInfo.mLifeSpan += 1;
        IDU_LIST_ADD_LAST(&mNewTaskList, aTask->getThreadListNode());

        unlockForNewTaskList();
    }
}

void mmtServiceThread::addTasks(iduList *aTaskList,UInt aTaskCnt)
{
    lockForNewTaskList();
    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       assign�� task ���� ����.
       �ֳ��ϸ� �ش� service thread�� OS���� scheduling�Ǿ�
       getNewTask function�� ����Ǿ� ,
       task ������ �����Ǳ� ����...
    */

    mInfo.mCurAddedTasks += aTaskCnt;
    mInfo.mLifeSpan += aTaskCnt;
    IDU_LIST_JOIN_LIST(&mNewTaskList, aTaskList);

    unlockForNewTaskList();
}

void mmtServiceThread::removeAllTasks(iduList *aTaskList, mmtServiceThreadLock aLock)
{
    idBool       sIsLocked = ID_TRUE;
    mmcTask     *sTask;

    /* BUG-45274 */
    UInt              sPreRemoveTaskCount = 0;

    sPreRemoveTaskCount = getAssignedTasks();

    if (aLock == MMT_SERVICE_THREAD_LOCK)
    {
        sIsLocked = trylockForThread();
    }

    if (sIsLocked == ID_TRUE)
    {
        /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
           task ���� 1 �����̸� �Ʒ��� ��ƾ�� Ÿ�� ����
           CPU clock �����̴�.
        */

        /*
         * BUG-38384 A task in a service thread can be a orphan
         *
         * mInfo.mRemoveAllTasks�� �ξ� ��� Task�� �����ؾ� ��츦 �Ǵ��Ѵ�.
         * ���� DEDICATED THREAD ��Ŀ��� ��� Task�� �����ϴ� �ڵ尡 ����Ǹ�
         * �̻��� ������ �߻��ϴµ� ��Ȯ�ϰ� ������ �ľ������� ���ߴ�.
         * ���� �ǽ��� ���� ��Ȳ������ �Ʒ� �ڵ尡 �ݵ�� ����ǵ��� �����ߴ�.
         */
        if( getAssignedTasks() > 1 || getRemoveAllTasks() == ID_TRUE )
        {
            /*fix BUG-29717 When a task is terminated or migrated ,
              a busy degree of service thread which the task was assigned
              to should be decreased.*/
            for (;  (IDU_LIST_IS_EMPTY(&mReadyTaskList) != ID_TRUE); )       
            {
                sTask = (mmcTask *)mReadyTaskList.mNext->mObj;
                removeTask(aTaskList, sTask, ID_TRUE);
            }

            lockForNewTaskList();
            for (;  (IDU_LIST_IS_EMPTY(&mNewTaskList) != ID_TRUE); )    
            {
                sTask = (mmcTask *)mNewTaskList.mNext->mObj;
                removeTask(aTaskList, sTask, ID_FALSE);
            }
            increaseOutTaskCount(MMT_SERVICE_THREAD_RUN_IN_BUSY,(getTaskCount() -1));

            unlockForNewTaskList();
            for (;  (IDU_LIST_IS_EMPTY(&mWaitTaskList) != ID_TRUE); )
            {
                sTask = (mmcTask *)mWaitTaskList.mNext->mObj;
                removeTask(aTaskList, sTask, ID_TRUE);
            }
            mInfo.mCurAddedTasks = 0;
        
            mInfo.mTaskCount = 1;
        
            mInfo.mReadyTaskCount = 0;
        }
        else
        {
            //nothing to do.
        }
        if (aLock == MMT_SERVICE_THREAD_LOCK)
        {
            unlockForThread();
        }
    }
    else
    {
        /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
           new tasks���̶�  migration�Ϸô� ����� �ؾ� �Ѵ�.
        */
        if(trylockForNewTaskList() == ID_TRUE)
        {
            for (;  (IDU_LIST_IS_EMPTY(&mNewTaskList) != ID_TRUE); )
            {
                sTask = (mmcTask *)mNewTaskList.mNext->mObj;
                increaseOutTaskCount(MMT_SERVICE_THREAD_RUN_IN_BUSY,1);
                removeTask(aTaskList, sTask, ID_FALSE);
                decreaseCurAddedTaskCnt();
            }
            unlockForNewTaskList();
        }
        else
        {
            //nothing to do
        }
        
    }//else
    
    /* BUG-45274 reference http://nok.altibase.com/pages/viewpage.action?pageId=40570104  */
    if( getAssignedTasks() < sPreRemoveTaskCount)
    {
        ideLog::logLine(IDE_LB_2, "decrease Task count(%"ID_UINT32_FMT" --> %"ID_UINT32_FMT") in ServiceThread(%"ID_UINT32_FMT")",
                        sPreRemoveTaskCount,
                        getAssignedTasks(),
                        getServiceThreadID());
    }
}

/* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
   load balance �������� idle max thread���� idle min thread����
   task �й�� �Ҹ��� �Լ��̴�.
 */

idBool mmtServiceThread::need2Move(UInt aTaskCount2Move,UInt aMinTaskCount )
{

    UInt  sMoveRate;
    
    if(aTaskCount2Move == 0)
    {
        return ID_FALSE;
    }
    else
    {
        if(aMinTaskCount == 0)
        {
            /* BUG-45274 reference http://nok.altibase.com/pages/viewpage.action?pageId=40570104  */
            ideLog::logLine(IDE_LB_2, "L7 Min Task Count is zero.");

            return ID_TRUE;
            
        }
        else
        {
            sMoveRate = (( aTaskCount2Move * 100) / aMinTaskCount);
            /* task �����ϴ� thread�� task ������ �� MIN_MIGRATION_TASK_RATE
               �� �Ѵ� ��쿡�� task�� �����Ѵ�.*/
            if(sMoveRate >  mmuProperty::getMinMigrationTaskRate())
            {
                /* BUG-45274 reference http://nok.altibase.com/pages/viewpage.action?pageId=40570104  */
                ideLog::logLine(IDE_LB_2, "L7 Task Move Rate(%"ID_UINT32_FMT") > MIN_MIGRATION_TASK_RATE(%"ID_UINT32_FMT")",
                                sMoveRate, mmuProperty::getMinMigrationTaskRate());
                return ID_TRUE;
            }
            else
            {
                return ID_FALSE;
            }
        }//else
    }//else
}



idBool mmtServiceThread::needToRemove()
{
    idBool               sRetVal;
    
    UInt    sTaskCount = getAssignedTasks();
    
    if( sTaskCount >= 2)
    {
        sRetVal = ID_TRUE;
    }
    else
    {
        //  <= 1
        sRetVal = ID_FALSE;
    }//else
    return sRetVal;
}


/* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
   load balance �������� idle max thread���� idle min thread����
   task �й��ϴ� �Լ��̴�.
 */
void mmtServiceThread::removeFewTasks(iduList *aTaskList, UInt aMinTaskCount,UInt *aRemovedTaskCount)
{
    mmcTask *sTask;
    UInt sMaxTaskCount = getAssignedTasks();
    UInt sTaskCount;
    

    *aRemovedTaskCount = 0;
    if(sMaxTaskCount <= aMinTaskCount) /* BUG-45549 */
    {
        sTaskCount = 0;
    }
    else
    {
         sTaskCount = (sMaxTaskCount  - aMinTaskCount - 1) / 2;
    }
    // task �й踦 �� �ʿ䰡 �ִ����� �Ǵ�.
    if (need2Move(sTaskCount,aMinTaskCount ) == ID_TRUE )
    {
        lockForNewTaskList();

        for (; (sTaskCount > 0) && (IDU_LIST_IS_EMPTY(&mNewTaskList) != ID_TRUE); sTaskCount--)
        {
            sTask = (mmcTask *)mNewTaskList.mNext->mObj;
            increaseOutTaskCount(MMT_SERVICE_THREAD_RUN_IN_IDLE,1);
            *aRemovedTaskCount +=1;
            decreaseCurAddedTaskCnt();
            removeTask(aTaskList, sTask, ID_FALSE);
        }

        unlockForNewTaskList();

        if (sTaskCount > 0)
        {
            if (trylockForThread() == ID_TRUE)
            {
                for (; (sTaskCount > 0) && (IDU_LIST_IS_EMPTY(&mWaitTaskList) != ID_TRUE); sTaskCount--)
                {
                    sTask = (mmcTask *)mWaitTaskList.mNext->mObj;
                    *aRemovedTaskCount +=1;
                    removeTask(aTaskList, sTask, ID_TRUE);
                    increaseOutTaskCount(MMT_SERVICE_THREAD_RUN_IN_IDLE,1);
                    addTaskCount(-1);

                }
                
                for (; (sTaskCount > 0) && (IDU_LIST_IS_EMPTY(&mReadyTaskList) != ID_TRUE); sTaskCount--)

                {
                    sTask = (mmcTask *)mReadyTaskList.mNext->mObj;
                    removeTask(aTaskList, sTask, ID_TRUE);
                    *aRemovedTaskCount +=1;
                    addTaskCount(-1);
                    addReadyTaskCount(-1);
                    increaseOutTaskCount(MMT_SERVICE_THREAD_RUN_IN_IDLE,1);
                }

                unlockForThread();

            }

        }//if
    }//if

}

void mmtServiceThread::removeTask(iduList *aTaskList, mmcTask *aTask, idBool aRemoveFlag)
{
    IDU_LIST_REMOVE(aTask->getThreadListNode());

    IDU_LIST_ADD_LAST(aTaskList, aTask->getThreadListNode());
    /*fix BUG-29717 When a task is terminated or migrated ,
      a busy degree of service thread which the task was assigned
      to should be decreased.*/
    decreaseBusyExperienceCnt4Task(aTask);

    if (aRemoveFlag == ID_TRUE)
    {
        IDE_ASSERT(cmiRemoveLinkFromDispatcher(mDispatcher,
                                               aTask->getLink()) == IDE_SUCCESS);

    }
}

void mmtServiceThread::getNewTask()
{
    mmcTask     *sTask;
    iduListNode *sIterator;
    iduListNode *sNodeNext;


    lockForNewTaskList();

    IDU_LIST_ITERATE_SAFE(&mNewTaskList, sIterator, sNodeNext)
    {
        sTask = (mmcTask *)sIterator->mObj;

        IDU_LIST_REMOVE(sTask->getThreadListNode());

        switch (mInfo.mServiceThreadType)
        {
            case MMC_SERVICE_THREAD_TYPE_SOCKET:
            /* PROJ-2108 Dedicated thread mode which uses less CPU */
            case MMC_SERVICE_THREAD_TYPE_DEDICATED:
                switch (sTask->getTaskState())
                {
                    case MMC_TASK_STATE_WAITING:
                        IDE_ASSERT(cmiAddLinkToDispatcher(mDispatcher, sTask->getLink()) == IDE_SUCCESS);

                        IDU_LIST_ADD_LAST(&mWaitTaskList, sTask->getThreadListNode());

                        break;

                    case MMC_TASK_STATE_READY:
                        IDE_ASSERT(cmiAddLinkToDispatcher(mDispatcher, sTask->getLink()) == IDE_SUCCESS);

                        IDU_LIST_ADD_FIRST(&mReadyTaskList, sTask->getThreadListNode());

                        addReadyTaskCount(1);
                        break;

                    default:
                        IDE_CALLBACK_FATAL("invalid task state in mmtServiceThread::getNewTask()");
                        break;
                }
                break;
            case MMC_SERVICE_THREAD_TYPE_IPC:
                    IDU_LIST_ADD_FIRST(&mReadyTaskList, sTask->getThreadListNode());
                    addReadyTaskCount(1);
                    sTask->setTaskState(MMC_TASK_STATE_READY);
                    break;
            /*PROJ-2616*/
            case MMC_SERVICE_THREAD_TYPE_IPCDA:
                    IDU_LIST_ADD_FIRST(&mReadyTaskList, sTask->getThreadListNode());
                    addReadyTaskCount(1);
                    sTask->setTaskState(MMC_TASK_STATE_READY);
                    break;
            //fix PROJ-1749
            case MMC_SERVICE_THREAD_TYPE_DA:
                    IDU_LIST_ADD_FIRST(&mReadyTaskList, sTask->getThreadListNode());
                    addReadyTaskCount(1);
                    sTask->setTaskState(MMC_TASK_STATE_READY);
                    break;
            default:
                IDE_ASSERT(0);
        }
        /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
           �ش� service thread�� task�� counting�Ǿ��⶧����
           add�� task������ 1���� ��Ų��.
        */
        //fix BUG-29526 service thread�� getNewTask Function����
        //decreaseCurAddedTaskCnt�� ȣ�� ��ġ�� �߸��Ǿ����ϴ�.
        decreaseCurAddedTaskCnt();
        addTaskCount(1);
    }

    
    unlockForNewTaskList();
}

//  PROJ-1677 DEQUEUE
//  shared-mode���� ��Ǵ� service thread��
//  polling timeout�� �ǹ̰� �־
//  mPollTimeout  pointer�� ���ڷ� �Ѿ�´� .
//  �׷���  dedicated mode���� ��Ǵ�  service thread�� polling�� ���� �ʰ�
// ���Ѵ�⸦ �ϱ� ���Ͽ� NULL�� ���ڷ� �Ѿ�´�.
void mmtServiceThread::findReadyTask(PDL_Time_Value* aPollTimeout)
{
    iduList      sReadyList;
    iduListNode *sIterator;
    cmiLink     *sLink;
    mmcTask     *sTask;

    /*
     * Dispatcher�� �̿��Ͽ� Waiting Task -> Ready Task �˻�
     */


    IDE_ASSERT(cmiSelectDispatcher(mDispatcher,
                                   &sReadyList,
                                   NULL,
                                   aPollTimeout) == IDE_SUCCESS);



    IDU_LIST_ITERATE(&sReadyList, sIterator)
    {
        sLink = (cmiLink *)sIterator->mObj;
        sTask = (mmcTask *)cmiGetLinkUserPtr(sLink);
        /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
           task schedule���õ� ���� setting�Ѵ�.
        */
        transitTask_READY(sTask);
    }

}

//PROJ-1677 DEQ
// lockForThread��  wait,ready task list �� ���ü���� ���Ͽ�
// ���ȴ�.
void mmtServiceThread::executeTask()
{
    mmcTask      *sTask;
    mmcTaskState  sTaskState;
    IDE_RC        sExecuteResult = IDE_SUCCESS;
    mmqQueueInfo *sQueueInfo;
    //fix BUG-19647
    idBool        sQueueReady;

    IDE_CLEAR();

    sTask      = (mmcTask *)mReadyTaskList.mNext->mObj;
    /* bug-36875: null-checking for task object is necessary */
    IDE_TEST_RAISE(sTask == NULL, TaskNull);
    sTaskState = sTask->getTaskState();

    setTask(sTask);

    switch (mInfo.mServiceThreadType)
    {
        case MMC_SERVICE_THREAD_TYPE_SOCKET:
        /* PROJ-2108 Dedicated thread mode which uses less CPU */
        case MMC_SERVICE_THREAD_TYPE_DEDICATED:

            /* bug-35395: task schedule time �� sysstat�� �߰�
               ��Ŷ���� �������ĺ��� ��Ŷ�ؼ� �������� �ð�(usec)�� ��� */
            if (iduProperty::getTimedStatistics() == IDV_TIMED_STATISTICS_ON)
            {
                addTaskScheduleTime(sTask->getSession());
            }

            IDU_LIST_REMOVE(sTask->getThreadListNode());
            addReadyTaskCount(-1);
            unlockForThread();
            sTask->setThread(this);
QUEUE_READY:
            switch (sTaskState)
            {
                case MMC_TASK_STATE_READY:
                    sExecuteResult = executeTask_READY(sTask, &sTaskState);
                    break;

                case MMC_TASK_STATE_QUEUEREADY:
                    sExecuteResult = executeTask_QUEUEREADY(sTask, &sTaskState);
                    break;

                default:
                    IDE_CALLBACK_FATAL("invalid task state in mmtServiceThread::executeTask()");
                    break;
            }
            sTask->setThread(NULL);
            setSessionID(0);
            setStatement(NULL);
            IDE_TEST_RAISE(sExecuteResult != IDE_SUCCESS,ExecuteFail);

            switch (sTaskState)
            {
                case MMC_TASK_STATE_WAITING:
                    //PROJ-1677 DEQ
                    lockForThread();
                    transitTask_WAITING(sTask);
                    break;
                case MMC_TASK_STATE_QUEUEWAIT:
                    //PROJ-1677 DEQUEUE
                    if ( isRunModeShared() == ID_TRUE )
                    {
                        //service thread run as shared mode.
                        // need to transform  shared-run mode into dedicated-run mode.
                        // MVCC�� ���Ͽ� Queue�� ����Ÿ �������� �ұ��ϰ� ,
                        //���д� ��찡 ������ �̸� �����ϱ� ����  üũ�Ѵ�.
                        //fix BUG-19647
                        sQueueInfo = sTask->getSession()->getQueueInfo();
                        IDE_DASSERT(sQueueInfo != NULL);
                        sQueueInfo->lock();
                        sQueueReady = sTask->getSession()->isQueueReady();
                        sQueueInfo->unlock();
                        if(sQueueReady == ID_FALSE)
                        {    
                            transformSharedIntoDedicated();
                        }
                        else
                        {
                            sTaskState = MMC_TASK_STATE_QUEUEREADY;
                            sTask->setTaskState(MMC_TASK_STATE_EXECUTING);
                            IDE_RAISE(QUEUE_READY);
                        }
                    }
                    else
                    {
                        //service thread run as dedicated mode,nothing to do.
                    }
                    //fix BUG-30952 A session and statement of a transformed dedicated thread should be showed
                    //through V$SERVICE_THRED, V$SESSION.
                    sTask->setThread(this);
                    setSessionID(sTask->getSession()->getSessionID());
                    setStatement(sTask->getSession()->getExecutingStatement());
                    IDE_TEST_RAISE(waitForEnqueueEvent(sTask) != IDE_SUCCESS,ExecuteFail);
                    //enqueue event�� �ƴ϶� shutdown event�� ������ �ִ�.
                    if(mRun == ID_TRUE)
                    {
                        sTaskState = sTask->getTaskState();
                        sTask->setTaskState(MMC_TASK_STATE_EXECUTING);
                        IDE_RAISE(QUEUE_READY);
                    }
                    else
                    {
                        // thread ����� unlockForThread�� ȣ���ϱ⶧���� ���⼭ lock�� ��´�.
                        lockForThread();
                    }
                    break;

                default:
                    IDE_CALLBACK_FATAL("invalid task state after mmtServiceThread::executeTask()");
                    break;
            }//switch
            break;
        case MMC_SERVICE_THREAD_TYPE_IPC:
        case MMC_SERVICE_THREAD_TYPE_IPCDA: /* BUG-46163 */
        case MMC_SERVICE_THREAD_TYPE_DA: //fix PROJ-1749
            unlockForThread();
            sTask->setThread(this);

            switch (sTaskState)
            {
                case MMC_TASK_STATE_READY:
                    sExecuteResult = executeTask_READY(sTask, &sTaskState);
                    break;
                case MMC_TASK_STATE_QUEUEREADY:
                    sExecuteResult = executeTask_QUEUEREADY(sTask, &sTaskState);
                    break;
                default:
                    IDE_CALLBACK_FATAL("invalid task state in mmtServiceThread::executeTask()");
                    break;
            }
            IDE_TEST_RAISE(sExecuteResult != IDE_SUCCESS, ExecuteFail);
            switch (sTaskState)
            {
                case MMC_TASK_STATE_WAITING:
                    sTask->setTaskState(MMC_TASK_STATE_READY);
                    break;

                case MMC_TASK_STATE_QUEUEWAIT:
                    IDE_TEST_RAISE(waitForEnqueueEvent(sTask) != IDE_SUCCESS,
                                   ExecuteFail);
                    break;

                default:
                    IDE_CALLBACK_FATAL("invalid task state after mmtServiceThread::executeTask()");
                    break;
            }
            //PROJ-1677 DEQUEUE
            lockForThread();
            break;
        default:
            IDE_ASSERT(0);
    }

    return;

    IDE_EXCEPTION(ExecuteFail);
    {

        lockForThread();
        //fix PROJ-1749
        if( (MMC_SERVICE_THREAD_TYPE_IPC == mInfo.mServiceThreadType) ||
            (MMC_SERVICE_THREAD_TYPE_DA == mInfo.mServiceThreadType) ||
            (MMC_SERVICE_THREAD_TYPE_IPCDA == mInfo.mServiceThreadType) )
        {
            IDU_LIST_REMOVE(sTask->getThreadListNode());
            addReadyTaskCount(-1);
        }
        else
        {
            //fix BUG-30952 A session and statement of a transformed dedicated thread should be showed
            //through V$SERVICE_THRED, V$SESSION.
            sTask->setThread(NULL);
            setSessionID(0);
            setStatement(NULL);
        }
        terminateTask(sTask);

    }
    /* bug-36875: null-checking for task object is necessary.
       client �߸����� ���������� async�� ������� ���
       task�� null�� �� �ִ�. ������� */
    IDE_EXCEPTION(TaskNull);
    {
        ideLog::log(IDE_SERVER_0,
                "executeTask: NULL task ignored. mReadyTaskCount: %u",
                mInfo.mReadyTaskCount);
        mInfo.mReadyTaskCount    = 0;
    }
    IDE_EXCEPTION_END;
}

/*
 * transitTask_XXXX �Լ����� ��� lockForThread�� ȹ��� ���¿����� ȣ��Ǿ�� �Ѵ�.
 */

void mmtServiceThread::transitTask_READY(mmcTask *aTask)
{
    mmcSession  *sSession;
    idvSQL      *sStatSQL;

    /* bug-35395: task schedule time �� sysstat�� �߰�
       ��Ŷ���� �������ĺ��� ��Ŷ�ؼ� �������� �ð�(usec)�� ��� */
    sSession = aTask->getSession();
    if (sSession != NULL)
    {
        sStatSQL = sSession->getStatSQL();
        IDV_SQL_OPTIME_BEGIN(sStatSQL, IDV_OPTM_INDEX_TASK_SCHEDULE);
    }

    IDU_LIST_REMOVE(aTask->getThreadListNode());
    IDU_LIST_ADD_LAST(&mReadyTaskList, aTask->getThreadListNode());

    addReadyTaskCount(1);

    aTask->setTaskState(MMC_TASK_STATE_READY);
}


void mmtServiceThread::transitTask_WAITING(mmcTask *aTask)
{
    if (cmiProtocolContextHasPendingRequest(aTask->getProtocolContext()) == ID_TRUE)
    {
        IDU_LIST_ADD_LAST(&mReadyTaskList, aTask->getThreadListNode());

        addReadyTaskCount(1);

        aTask->setTaskState(MMC_TASK_STATE_READY);
    }
    else
    {
        IDU_LIST_ADD_LAST(&mWaitTaskList, aTask->getThreadListNode());

        aTask->setTaskState(MMC_TASK_STATE_WAITING);
    }
}

/*
 * executeTask_XXXX �Լ����� lockForThread�� ȹ����� ���� ���¿��� ȣ��ǹǷ�
 * Dispatcher, Task List, Task Count, Task State ��
 * lockForThread�� ��ȣ�Ǿ�� �ϴ� �͵��� ����ϰų� �����ϸ� �ȵȴ�.
 */

IDE_RC mmtServiceThread::executeTask_READY(mmcTask *aTask, mmcTaskState *aNewState)
{
    IDE_RC      sRet;
    mmcSession *sSession;
    cmiProtocolContext* sCtx;

    mErrorFlag = ID_FALSE;


    sSession = aTask->getSession();
    if (sSession != NULL)
    {
        sSession->setIdleStartTime(mmtSessionManager::getBaseTime());
    }


    // fix BUG-29073 task executing ���� �ΰ����ϴ� ������
    //�߸��Ǿ���.
    // BUG-24318 IPC �ϰ�� ���� ������ �߸� ���ɴϴ�.
    if ((mInfo.mServiceThreadType != MMC_SERVICE_THREAD_TYPE_IPC) &&
        (mInfo.mServiceThreadType != MMC_SERVICE_THREAD_TYPE_IPCDA))
    {
        aTask->setTaskState(MMC_TASK_STATE_EXECUTING);
    }
    
    /* proj_2160 cm_type removal */
    sCtx = aTask->getProtocolContext();
    if (cmiGetPacketType(sCtx) != CMP_PACKET_TYPE_A5)
    {
        if ( cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA )
        {
            sRet = cmiRecvIPCDA(sCtx,
                                this,
                                mmuProperty::getIPCDASleepTime());
        }
        else
        {
            sRet = cmiRecv(sCtx, this, NULL, aTask);
        }
    }
    else
    {
        sRet = cmiReadProtocolAndCallback(sCtx, this, NULL, aTask);
    }

    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       task schedule  ������  setting�Ѵ�.
    */

    switch (sRet)
    {
        case IDE_CM_STOP:
            // bug-26975: codesonar: session null ref
            // ������ null�� ���������� ������, �������� �˻��߰�.
            sSession = aTask->getSession();
            IDE_TEST(sSession == NULL);
            IDE_ASSERT(sSession->getQueueInfo() != NULL);

            *aNewState = MMC_TASK_STATE_QUEUEWAIT;
            break;

        case IDE_SUCCESS:
            if ( aTask->isShutdownTask() != ID_TRUE )
            {
                /* proj_2160 cm_type removal */
                if (cmiGetPacketType(sCtx) != CMP_PACKET_TYPE_A5)
                {
                    if ( cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA )
                    {
                        IDE_TEST_RAISE( cmiLinkPeerFinalizeSvrForIPCDA(sCtx) != IDE_SUCCESS, CommunicationFail);
                    }
                    else
                    {
                        IDE_TEST_RAISE(cmiSend(sCtx, ID_TRUE)
                                       != IDE_SUCCESS, FlushFail);
                    }
                }
                else
                {
                    IDE_TEST_RAISE(cmiFlushProtocol(sCtx, ID_TRUE)
                                   != IDE_SUCCESS, FlushFail);
                }
            }

            *aNewState = MMC_TASK_STATE_WAITING;
            break;

        case IDE_FAILURE:
        default:
        {
            /* BUG-46390 cmiLinkPeerFinalizeSvrForIPCDA�� �ݵ�� ȣ��Ǿ�� �� */
            if ( cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA )
            {
                cmiLinkPeerFinalizeSvrForIPCDA(sCtx);
            }
            IDE_RAISE(CommunicationFail);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(FlushFail);
    {
        if (ideGetErrorCode() != cmERR_ABORT_CONNECTION_CLOSED)
        {
            mmtThreadManager::logNetworkError(MM_TRC_FLUSH_PROTOCOL_FAILED);
        }
    }
    IDE_EXCEPTION(CommunicationFail);
    {
        switch (ideGetErrorCode())
        {
            case cmERR_ABORT_CONNECTION_CLOSED:
            case idERR_ABORT_Session_Closed:
                break;

            case idERR_ABORT_Session_Disconnected:
                mmtThreadManager::logDisconnectError(aTask, ideGetErrorCode());
                break;

            default:
                mmtThreadManager::logNetworkError(MM_TRC_PROTOCOL_PROCESSING_FAILED);
                break;
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::executeTask_QUEUEREADY(mmcTask *aTask, mmcTaskState *aNewState)
{
    idBool sSuspended = ID_FALSE;
    cmiProtocolContext* sCtx;
    UChar               sPacketType;

    setSessionID(aTask->getSession()->getSessionID());
    setStatement(aTask->getSession()->getExecutingStatement());
    sCtx = aTask->getProtocolContext();

    sPacketType = cmiGetPacketType(sCtx);
    // proj_2160 cm_type removal: for A7 client
    if (sPacketType != CMP_PACKET_TYPE_A5)
    {
        if (execute(sCtx,
                    aTask->getSession()->getExecutingStatement(),
                    ID_TRUE,
                    &sSuspended,
                    NULL, NULL, NULL) != IDE_SUCCESS)
        {
            IDE_TEST_RAISE(answerErrorResult(sCtx,
                                             CMI_PROTOCOL_OPERATION(DB, Execute),
                                             0) != IDE_SUCCESS,
                           CommunicationFail);
        }
    }
    // for A5 client
    else
    {
        if (executeA5(sCtx,
                    aTask->getSession()->getExecutingStatement(),
                    ID_TRUE,
                    &sSuspended,
                    NULL, NULL) != IDE_SUCCESS)
        {
            IDE_TEST_RAISE(answerErrorResultA5(sCtx,
                                             CMI_PROTOCOL_OPERATION(DB, Execute),
                                             0) != IDE_SUCCESS,
                           CommunicationFail);
        }

    }

    if (sSuspended == ID_TRUE)
    {
        *aNewState = MMC_TASK_STATE_QUEUEWAIT;
    }
    else
    {
        /* BUG-46163 IPCDA�� ��� Pending�� ������ ���� (������ FALSE) */
        if (cmiProtocolContextHasPendingRequest(sCtx) == ID_TRUE)
        {
            IDE_TEST(executeTask_READY(aTask, aNewState) != IDE_SUCCESS);
        }
        else
        {
            // proj_2160 cm_type removal
            if (sPacketType != CMP_PACKET_TYPE_A5)
            {
                if ( cmiGetLinkImpl(sCtx) == CMI_LINK_IMPL_IPCDA )
                {
                    IDE_TEST_RAISE( cmiLinkPeerFinalizeSvrForIPCDA(sCtx) != IDE_SUCCESS, CommunicationFail);
                }
                else
                {
                    IDE_TEST_RAISE(cmiSend(sCtx, ID_TRUE)
                                   != IDE_SUCCESS, FlushFail);
                }
            }
            else
            {
                IDE_TEST_RAISE(cmiFlushProtocol(sCtx, ID_TRUE)
                               != IDE_SUCCESS, FlushFail);
            }

            *aNewState = MMC_TASK_STATE_WAITING;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(FlushFail);
    {
        if (ideGetErrorCode() != cmERR_ABORT_CONNECTION_CLOSED)
        {
            mmtThreadManager::logNetworkError(MM_TRC_FLUSH_PROTOCOL_FAILED);
        }
    }
    IDE_EXCEPTION(CommunicationFail);
    {
        switch (ideGetErrorCode())
        {
            case cmERR_ABORT_CONNECTION_CLOSED:
            case idERR_ABORT_Session_Closed:
                break;

            case idERR_ABORT_Session_Disconnected:
                mmtThreadManager::logDisconnectError(aTask, ideGetErrorCode());
                break;

            default:
                mmtThreadManager::logNetworkError(MM_TRC_PROTOCOL_PROCESSING_FAILED);
                break;
        }
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

//PROJ-1677
void mmtServiceThread::terminateTask(mmcTask *aTask)
{
    IDE_RC sRC;

    /* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
       Load balance�� ���ü��� ���õǾ� �־,
       instruction pipe line�� ���Ͽ� ����Ѵ�.
    */

    // fix BUG-25158
    // Thread Manager�� removeAllTasks ���� ��
    // Service Thread�� Dispatcher���� �� link�� �ٽ� ���� �ʵ��� �ϱ� ����
    // ���� rollback���� task�� NULL�� �����Ѵ�.
    ID_SERIAL_BEGIN(setTask(NULL));

    //PROJ-1677
    ID_SERIAL_EXEC(IDE_ASSERT(cmiRemoveLinkFromDispatcher(mDispatcher,
                                                          aTask->getLink()) == IDE_SUCCESS),1);

    /*fix BUG-29717 When a task is terminated or migrated ,
      a busy degree of service thread which the task was assigned
      to should be decreased.*/
    ID_SERIAL_EXEC(decreaseBusyExperienceCnt4Task(aTask),2);
    // fix BUG-25158
    // rollback�ÿ��� Service Thread�� lock�� Ǯ�� �����Ѵ�.
    // service thread lock �� Ǯ���� execute �� �ݵ�ú����ϱ����Ͽ�...
    ID_SERIAL_EXEC(unlockForThread(),3);
    if (aTask->isShutdownTask() == ID_TRUE)
    {
        // service thread lock �� Ǯ���� execute �� �ݵ�ú����ϱ����Ͽ�...
        ID_SERIAL_EXEC(sRC = mmtSessionManager::freeSession(aTask),4);
        if (sRC != IDE_SUCCESS)
        {
            mmtThreadManager::logMemoryError(MM_TRC_SESSION_FREE_FAILED);
        }
    }
    else
    {
        // service thread lock �� Ǯ���� execute �� �ݵ�ú����ϱ����Ͽ�...
        ID_SERIAL_EXEC(sRC = mmtSessionManager::freeTask(aTask),4);
        if (sRC  != IDE_SUCCESS)
        {
            mmtThreadManager::logMemoryError(MM_TRC_TASK_FREE_FAILED);
        }
    }
    // service thread lock �� Ǯ���� execute �� �ݵ�ú����ϱ����Ͽ�...
    ID_SERIAL_END(lockForThread());

    //PROJ-1677 DEQUEUE
    addTaskCount(-1);

    //service thread type�� IPC�� �ƴ� socket type�̰� ,
    //dedicated mode���� running �Ǿ��ٸ�
    //�ٽ� shread mode  service thread���� transform�ϰų� ,
    //�Ҹ��Ͽ��� �Ѵ�.
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_SOCKET)
    {
        /* PROJ-2108 Dedicated thread mode which uses less CPU */
        if ( ( mmuProperty::getIsDedicatedMode() == 0 ) && ( isRunModeShared() == ID_FALSE ) )
        {
            transformDedicatedIntoShared();
        }//if
    }//if mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_SOCKET
}

IDE_RC mmtServiceThread::getUserInfoFromDB( idvSQL      *aStatistics, 
                                            qciUserInfo *aUserInfo )
{
    smiTrans      sTrans;
    smiStatement  sSmiStmt;
    smiStatement *sDummySmiStmt;
    qciStatement  sQciStmt;

    UInt          sStage = 0;

    IDE_TEST( sTrans.initialize() != IDE_SUCCESS ); /* PROJ-2446 */
    sStage++;

    IDE_TEST( sTrans.begin( &sDummySmiStmt,
                            aStatistics, /* PROJ-2446 */
                            ( SMI_TRANSACTION_UNTOUCHABLE | 
                              SMI_ISOLATION_CONSISTENT    | 
                              SMI_COMMIT_WRITE_NOWAIT ) ) 
              != IDE_SUCCESS );
    sStage++;

    IDE_TEST( sSmiStmt.begin ( sTrans.getStatistics(), /* PROJ-2446 */
                               sDummySmiStmt, 
                               SMI_STATEMENT_UNTOUCHABLE | SMI_STATEMENT_MEMORY_CURSOR) != IDE_SUCCESS );
    sStage++;

    idlOS::memset(&sQciStmt, 0, ID_SIZEOF(sQciStmt));
    IDE_TEST(qci::initializeStatement(&sQciStmt, NULL, NULL, NULL) != IDE_SUCCESS);
    sStage++;

    IDE_TEST(qci::getUserInfo(&sQciStmt, &sSmiStmt, aUserInfo) != IDE_SUCCESS);
    IDE_TEST(qci::finalizeStatement(&sQciStmt) != IDE_SUCCESS);
    sStage--;

    IDE_TEST(sSmiStmt.end(SMI_STATEMENT_RESULT_SUCCESS) != IDE_SUCCESS);
    sStage--;

    IDE_TEST(sTrans.commit() != IDE_SUCCESS);
    sStage--;

    IDE_TEST(sTrans.destroy(NULL) != IDE_SUCCESS);
    sStage--;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    {
        switch (sStage)
        {
            case 4:
                qci::finalizeStatement(&sQciStmt);
            case 3:
                IDE_ASSERT(sSmiStmt.end(SMI_STATEMENT_RESULT_FAILURE) == IDE_SUCCESS);
            case 2:
                IDE_ASSERT(sTrans.commit() == IDE_SUCCESS);
            case 1:
                IDE_ASSERT(sTrans.destroy(NULL) == IDE_SUCCESS);
            default:
                break;
        }
    }

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::getUserInfoFromFile(qciUserInfo *aUserInfo)
{
    PDL_HANDLE  sPasswdFD;
    SChar       sPasswdFile[256];
    SInt        sPasswdLen;

    if( aUserInfo->mUsrDN != NULL )
    {
        SInt    sSvrDNLen = idlOS::strlen(aUserInfo->mSvrDN);
        SInt    sUsrDNLen = idlOS::strlen(aUserInfo->mUsrDN);
        IDE_TEST_RAISE( sSvrDNLen != sUsrDNLen, MMT_ERR_DNMismatched );
        IDE_TEST_RAISE(idlOS::memcmp(aUserInfo->mSvrDN, aUserInfo->mUsrDN, sSvrDNLen) != 0, MMT_ERR_DNMismatched);
        idlOS::snprintf(aUserInfo->loginID,
                        ID_SIZEOF(aUserInfo->loginID),
                        "%s",
                        "SYS");
    }

    IDE_TEST_RAISE(idlOS::strcasecmp(aUserInfo->loginID, IDP_SYSUSER_NAME) != 0, IncorrectUser);

    aUserInfo->loginUserID = QC_SYS_USER_ID;   /* BUG-41561 */
    aUserInfo->userID      = QC_SYS_USER_ID;

    idlOS::snprintf(sPasswdFile,
                    ID_SIZEOF(sPasswdFile),
                    "%s" IDL_FILE_SEPARATORS "%s",
                    idp::getHomeDir(),
                    IDP_SYSPASSWORD_FILE);

    sPasswdFD = idf::open(sPasswdFile, O_RDONLY);

    IDE_TEST_RAISE(sPasswdFD == PDL_INVALID_HANDLE, PasswdFileOpenFail);

    sPasswdLen = idf::read(sPasswdFD,
                              aUserInfo->userPassword,
                              ID_SIZEOF(aUserInfo->userPassword) - 1);

    IDE_TEST_RAISE(sPasswdLen <= 0, PasswdFileReadFail);

    aUserInfo->userPassword[sPasswdLen] = 0;
    idf::close(sPasswdFD);

    return IDE_SUCCESS;

    IDE_EXCEPTION(MMT_ERR_DNMismatched);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_DNMismatched,aUserInfo->mSvrDN,aUserInfo->mUsrDN));
    }
    IDE_EXCEPTION(IncorrectUser);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_NO_USER_ERROR));
    }
    IDE_EXCEPTION(PasswdFileOpenFail);
    {
        IDE_SET(ideSetErrorCode(mmERR_FATAL_PASSWORD_FILE_ERROR));
        IDE_CALLBACK_FATAL("password file open failed");
    }
    IDE_EXCEPTION(PasswdFileReadFail);
    {
        idf::close(sPasswdFD);

        IDE_SET(ideSetErrorCode(mmERR_FATAL_PASSWORD_FILE_ERROR));
        IDE_CALLBACK_FATAL("password file read failed");
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::findSession(mmcTask           *aTask,
                                     mmcSession       **aSession,
                                     mmtServiceThread  *aThread)
{
    IDE_TEST_RAISE(aTask == NULL, NoTask);

    *aSession = aTask->getSession();

    IDE_TEST_RAISE(*aSession == NULL, NoSession);

    aThread->setSessionID((*aSession)->getSessionID());

    return IDE_SUCCESS;

    IDE_EXCEPTION(NoTask);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_SESSION_NOT_SPECIFIED));
    }
    IDE_EXCEPTION(NoSession);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_SESSION_NOT_CONNECTED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::checkSessionState(mmcSession *aSession, mmcSessionState aSessionState)
{
    switch (aSession->getSessionState())
    {
        case MMC_SESSION_STATE_AUTH:
            IDE_TEST_RAISE(aSessionState != MMC_SESSION_STATE_AUTH, InvalidState);
            break;

        case MMC_SESSION_STATE_READY:
            IDE_TEST_RAISE(aSessionState == MMC_SESSION_STATE_SERVICE, InvalidState);
            break;

        case MMC_SESSION_STATE_SERVICE:
            aSession->setIdleStartTime(0);
            break;

        default:
            IDE_RAISE(InvalidState);
            break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_SESSION_STATE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::findStatement(mmcStatement     **aStatement,
                                       mmcSession        *aSession,
                                       mmcStmtID         *aStmtID,
                                       mmtServiceThread  *aThread)
{
    *aStatement = NULL;

    if (aThread->getStatement() != NULL)
    {
        if (*aStmtID == 0)
        {
            *aStatement = aThread->getStatement();
            *aStmtID    = aThread->getStatement()->getStmtID();
        }
        else if (*aStmtID == aThread->getStatement()->getStmtID())
        {
            *aStatement = aThread->getStatement();
        }
        else
        {
            IDE_TEST(mmcStatementManager::findStatement(aStatement,
                                                        aSession,
                                                        *aStmtID) != IDE_SUCCESS);

            aThread->setStatement(*aStatement);

            /* PROJ-2177 User Interface - Cancel */
            IDU_SESSION_CLR_CANCELED(*aSession->getEventFlag());
        }
    }
    else
    {
        IDE_TEST(mmcStatementManager::findStatement(aStatement,
                                                    aSession,
                                                    *aStmtID) != IDE_SUCCESS);

        aThread->setStatement(*aStatement);

        /* PROJ-2177 User Interface - Cancel */
        IDU_SESSION_CLR_CANCELED(*aSession->getEventFlag());
    }

    // BUG-33544
    // �� ���ǿ� ������� current_statement_id�� ���ŵǾ�� ��.
    aSession->getInfo()->mCurrStmtID = *aStmtID;

    /* BUG-38472 Query timeout applies to one statement. */
    if ( (*aStatement)->getTimeoutEventOccured() == ID_TRUE )
    {
        IDU_SESSION_SET_TIMEOUT( *aSession->getEventFlag(), *aStmtID );
    }
    else
    {
        IDU_SESSION_CLR_TIMEOUT( *aSession->getEventFlag() );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC mmtServiceThread::checkStatementState(mmcStatement *aStatement, mmcStmtState aStmtState)
{
    IDE_TEST_RAISE(aStatement->getStmtState() < aStmtState, InvalidState);

    return IDE_SUCCESS;

    IDE_EXCEPTION(InvalidState);
    {
        IDE_SET(ideSetErrorCode(mmERR_ABORT_INVALID_STATEMENT_STATE_ERROR));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

// PROJ-1677 shared mode thread multiplexing function.
void mmtServiceThread::multiplexingAsShared()
{
    ULong sExecDuration;
    // fix BUG-32534 [mm-queue] The polling time of a shared service thread could be 0 in case that the service thread which had been transformed to dedicated service thread is reused
    // and that the ready task count of the service thread eventually becomes 0.
    UInt  sPollTimeOutUsec;
    IDE_CLEAR();


    mLoopCounter++;
    getNewTask();
    if (getReadyTaskCount() > 0)
    {
        setState(MMT_SERVICE_THREAD_STATE_EXECUTE);
        do
        {
            IDV_TIME_GET(&mInfo.mExecuteBegin);
            //execute�Ϸ��Ŀ� LoopCounter�� ������Ų��.
            ID_SERIAL_BEGIN(executeTask());
            ID_SERIAL_END(mLoopCounter++);
            IDV_TIME_GET(&mInfo.mExecuteEnd);
            /* fix BUG-29592 A responsibility for updating a global session time have better to be assigned
               to session manager rather than load balancer
               time resultion�� 1�ʴ����� �Ǿ� execution time�� ������ ���� ���ؾ� ��.
             */
            if(mInfo.mBusyExperienceCnt > 0)
            {   
                sExecDuration = IDV_TIME_DIFF_MICRO(&mInfo.mExecuteBegin,&mInfo.mExecuteEnd);
                if(sExecDuration < mmuProperty::getMultiplexingCheckInterval())
                {
                    /*fix BUG-29717 When a task is terminated or migrated ,
                    a busy degree of service thread which the task was assigned
                    to should be decreased.*/
                    decreaseBusyExperienceCnt(1);
                }
            }
        } while (getReadyTaskCount() > 0);
        mPollTimeout.set(0, MMT_SERVICE_THREAD_MIN_POLL_TIMEOUT);
    }
    else
    {

        // fix BUG-32534 [mm-queue] The polling time of a shared service thread could be 0 in case that the service thread which had been transformed to dedicated service thread is reused
        // and that the ready task count of the service thread eventually becomes 0.
        sPollTimeOutUsec = (UInt)mPollTimeout.usec() * 2;
        if(sPollTimeOutUsec > 0)
        {
            sPollTimeOutUsec = IDL_MIN(sPollTimeOutUsec,mmuProperty::getMultiplexingPollTimeout());
        }
        else
        {
            sPollTimeOutUsec = mmuProperty::getMultiplexingPollTimeout();
        }
        
        mPollTimeout.set(0, sPollTimeOutUsec);
    }

    setState(MMT_SERVICE_THREAD_STATE_POLL);
    findReadyTask(&mPollTimeout);
}

/* PROJ-2108 Dedicated thread mode which uses less CPU */
void mmtServiceThread::dedicatedThreadMode()
{
    IDE_CLEAR();
    getNewTask();

    if (getReadyTaskCount() > 0)
    {
        setState(MMT_SERVICE_THREAD_STATE_EXECUTE);
        do
        {
            IDV_TIME_GET(&mInfo.mExecuteBegin);
            executeTask();
            IDV_TIME_GET(&mInfo.mExecuteEnd);
        }while (getReadyTaskCount() > 0);
    }
    mPollTimeout.set(MMT_SERVICE_THREAD_DEDICATED_POLL_TIMEOUT, 0);
    setState(MMT_SERVICE_THREAD_STATE_POLL);
    findReadyTask(&mPollTimeout);
}
// PROJ-1677 dedicated  mode thread multiplexing function.
void mmtServiceThread::multiplexingAsDedicated()
{
    /* 
     * BUG-30656 When server stop while some service thread runs as dedicated , hang may happen.
     */
    SInt           sWaitSec;
    SInt           sWaitUSec;
    ULong sExecDuration;
    
    IDE_CLEAR();
    if (getReadyTaskCount() > 0)
    {
        setState(MMT_SERVICE_THREAD_STATE_EXECUTE);
        IDV_TIME_GET(&mInfo.mExecuteBegin);
        executeTask();
        IDV_TIME_GET(&mInfo.mExecuteEnd);
        sExecDuration = IDV_TIME_DIFF_MICRO(&mInfo.mExecuteBegin,&mInfo.mExecuteEnd);
        if( sExecDuration <= mmuProperty::getMultiplexingCheckInterval())
        {
            /*fix BUG-29717 When a task is terminated or migrated ,
              a busy degree of service thread which the task was assigned
              to should be decreased.*/
            decreaseBusyExperienceCnt(1);
        }
        else
        {
            increaseBusyExperienceCnt();
        }
        
    }
    // executeTask����  shared���� dedicated���� transformation�Ǵ� ��찡 �ִ�.
    // ����  transformation�Ǵ� ��츦 ����ؾ� �Ѵ�.
    if(mMultiPlexingFunc != &mmtServiceThread::multiplexingAsShared)
    {
        setState(MMT_SERVICE_THREAD_STATE_POLL);
        /* 
         * BUG-30656 When server stop while some service thread runs as dedicated , hang may happen.
         */
        sWaitSec  = mmuProperty::getMaxEnqWaitTime()/1000000;
        sWaitUSec = mmuProperty::getMaxEnqWaitTime() %1000000;
        mPollTimeout.set(sWaitSec, sWaitUSec);
        findReadyTask(&mPollTimeout);
    }//if
}

// PROJ-1677 wait util enqueue event .
IDE_RC  mmtServiceThread::waitForEnqueueEvent(mmcTask* aTask)
{
    mmqQueueInfo *sQueueInfo;
    IDE_RC        sQueueResult;
    idBool        sTimeOut;
    idBool        sQueueReady;
    
    
    sQueueInfo = aTask->getSession()->getQueueInfo();
    IDE_ASSERT(sQueueInfo != NULL);

  RETRY_QWAIT:
    sQueueInfo->lock();
    // condition wait�ϱ����� �̹� enqueue event�� �߻��Ͽ����� �˻��Ѵ�.
    // signal�� �Ҿ������ �ʱ� ���� ....
    if(aTask->getSession()->isQueueReady() == ID_FALSE)
    {
        //fix BUG-24362 dequeue ��� ������ close�ɶ� , session�� queue ��������
        // ���ü��� ������ ����.
        // -- ��¥ enqueue evnet����ϴ� ��츸  task �� ���¸� Queue Wait�� �����Ѵ�.
        aTask->setTaskState(MMC_TASK_STATE_QUEUEWAIT);
        sQueueResult = sQueueInfo->timedwaitDequeue(aTask->getSession()->getQueueWaitTime(),&sTimeOut);
        sQueueReady = aTask->getSession()->isQueueReady();
        //fix BUG-24362 dequeue ��� ������ close�ɶ� , session�� queue ��������
        // ���ü��� ������ ����.
        // -- ��� ���°� Ǯ���� ���� ����.
        aTask->setTaskState(MMC_TASK_STATE_QUEUEREADY);
        sQueueInfo->unlock();
        
        IDE_TEST_RAISE(sQueueResult != IDE_SUCCESS, ExecuteFail);
        // fileter broadcast event.
        if( sQueueReady == ID_FALSE)
        {
            if( IDU_SESSION_CHK_CLOSED(*aTask->getSession()->getEventFlag()))
            {
                // for terminate task.
                IDE_RAISE(ExecuteFail);
            }
            if(sTimeOut == ID_FALSE)
            {
                IDE_RAISE(RETRY_QWAIT);
                
            }//if sTimeOut
        }//if
        else
        {
            //enqueue event happend!!
        }
    }//if aTask 
    else
    {

        aTask->setTaskState(MMC_TASK_STATE_QUEUEREADY);
        sQueueInfo->unlock();
    }
    
    return IDE_SUCCESS;
    
    IDE_EXCEPTION(ExecuteFail);
    {
        //fix BUG-24362 dequeue ��� ������ close�ɶ� , session�� queue ��������
        // ���ü��� ������ ����.
        aTask->getSession()->endQueueWait();
    }
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

//PROJ-1677,need to transform  shared-run mode into dedicated-run mode
//1.shared mode���� dedicate mode���� running�ϱ� ���Ͽ� service list���� ������.
// - ������ �������� ������ �ִ� task list ���� task���� �����Ѵ�.
//2. multiplexing function ��  mtServiceThread::multiplexingAsShared���� �����Ѵ�.
void  mmtServiceThread::transformSharedIntoDedicated()
{
    // service thread list ������ �����  Ȯ���� �Ѵ�.
    if( mmtThreadManager::removeFromServiceThrList(this) == IDE_SUCCESS)
    {
        mMultiPlexingFunc = &mmtServiceThread::multiplexingAsDedicated;
        IDE_DASSERT(IDU_LIST_IS_EMPTY(&mNewTaskList) == ID_TRUE);
        IDE_DASSERT(IDU_LIST_IS_EMPTY(&mWaitTaskList) == ID_TRUE);
        IDE_DASSERT(IDU_LIST_IS_EMPTY(&mReadyTaskList) == ID_TRUE);
        //fix BUG-19323    
        mInfo.mRunMode   = MMT_SERVICE_THREAD_RUN_DEDICATED;
    }
    else
    {
        // task�絵�ϴ� �������� ���ο� ���� �����带 �����ϴٰ� ,
        // �����Ͽ���. ���� shared mode�� ���� �Ѵ�.
    }

}

//PROJ-1677,need to transform dedicated-run mode into  shared-run mode.
//1. multiplexing function  as mtServiceThread::multiplexingAsShared
//2. dedicated service thread�� shared service thread list��
//�ݳ��õ��� �Ѵ�.
// ����  ���� service list�� �ִ� ���� ������ ������
// MULTIPLEXING_MAX_THREAD_COUNT���� ũ�ų� ������
// �ݳ��� �����ʰ� ������ �Ҹ��� �Ѵ�.

void mmtServiceThread::transformDedicatedIntoShared()
{
    mLoopCounter = 1;
    mLoopCheck   = 0;
    mMultiPlexingFunc = &mmtServiceThread::multiplexingAsShared;
    //fix BUG-19323    
    mInfo.mRunMode           = MMT_SERVICE_THREAD_RUN_SHARED;
    //escape dead lock 
    unlockForThread();
    // try to add this service thread in service thread list .
    if(mmtThreadManager::try2AddServiceThread(this) == ID_FALSE)
    {
        // MULTIPLEXING_MAX_THREAD_COUNT���� ���ǿ� �ɷ� �Ҹ��Ͽ��� �Ѵ�.
        mRun  = ID_FALSE;
        /* fix BUG-30322 In case of terminating a transformed dedicated service thread,
           the count of service can be negative . */
        /* dedicated service thread���� shared service thread�� transform�� ����
           �Ͽ��⶧����  multiplexing func, run mode�� �����ؾ� �Ѵ� */
        mMultiPlexingFunc = &mmtServiceThread::multiplexingAsDedicated;
        mInfo.mRunMode    =  MMT_SERVICE_THREAD_RUN_DEDICATED;

    }
    lockForThread();
}

/* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
   a service thread�� busy ���� ���θ� �Ǵ��ϴ� �Լ�
*/

idBool mmtServiceThread::checkBusy()
{
    idBool sBusy; 
    
    if (trylockForThread() == ID_TRUE)
    {
        if (mLoopCheck == mLoopCounter)
        {
            sBusy  = ID_TRUE;
            increaseBusyExperienceCnt();
        }
        else
        {
            mLoopCheck = mLoopCounter;
            sBusy = ID_FALSE;
        }

        unlockForThread();
    }
    else
    {
        if (mLoopCheck == mLoopCounter)
        {
            sBusy  = ID_TRUE;
        }
        else
        {
            sBusy = ID_FALSE;
        }
    }
    return sBusy;
}

/* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
*/

idBool mmtServiceThread::trylockForNewTaskList()
{
    //fix PROJ-1749
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_DA)
    {
        return ID_TRUE;
    }

    idBool sSuccess;

    mNewTaskListMutex.trylock(sSuccess);

    return sSuccess;
}

idBool mmtServiceThread::trylockForThread()
{
    //fix PROJ-1749
    if (mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_DA)
    {
        return ID_TRUE;
    }

    idBool sSuccess;

    mMutex.trylock(sSuccess);

    return sSuccess;
}
void mmtServiceThread::lockForThread()
{
    //fix PROJ-1749
    if (mInfo.mServiceThreadType != MMC_SERVICE_THREAD_TYPE_DA)
    {
        //fix BUG-29514
        IDE_ASSERT(mMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS);
    }
}

void mmtServiceThread::unlockForThread()
{
    //fix PROJ-1749
    if (mInfo.mServiceThreadType != MMC_SERVICE_THREAD_TYPE_DA)
    {
        //fix BUG-29514.
        IDE_ASSERT(mMutex.unlock() == IDE_SUCCESS);
    }
}

/* TASK-4324  Applying lessons learned from CPBS-CAESE to altibase
   load balance�������� task in/out�� ����ϴ� �Լ�.
*/
void mmtServiceThread::increaseInTaskCount(mmtServiceThreadRunStatus  aRunStatus,
                                           UInt                       aCount)
{
    if(aRunStatus == MMT_SERVICE_THREAD_RUN_IN_IDLE)
    {
        mInfo.mInTaskCntFromIdle  +=  aCount;
    }
    else
    {
        mInfo.mInTaskCntFromBusy  += aCount;
    }
}

void mmtServiceThread::increaseOutTaskCount(mmtServiceThreadRunStatus  aRunStatus,
                                            UInt                       aCount)
{
    if(aRunStatus == MMT_SERVICE_THREAD_RUN_IN_IDLE)
    {
        mInfo.mOutTaskCntInIdle  +=  aCount;
    }
    else
    {
        mInfo.mOutTaskCntInBusy  += aCount;
    }
}


void  mmtServiceThread::increaseBusyExperienceCnt()
{
    /*fix BUG-29599 while altibase startup,
      a load balancer should not increase a busy experice count of a service thread.*/
    
    if (mmm::getCurrentPhase() == MMM_STARTUP_SERVICE)
    {
        mInfo.mBusyExperienceCnt += 1;
        /*fix BUG-29717 When a task is terminated or migrated ,
          a busy degree of service thread which the task was assigned
          to should be decreased.*/
        if(mTask != NULL)
        {
            mTask->incBusyExprienceCnt();
        }   
    }
}

void  mmtServiceThread::decreaseBusyExperienceCnt(UInt aDelta)
{
    /*fix BUG-29717 When a task is terminated or migrated ,
      a busy degree of service thread which the task was assigned
      to should be decreased.*/
    if(mTask != NULL)
    {
        mTask->decBusyExprienceCnt(aDelta);
    }
    if(mInfo.mBusyExperienceCnt >= aDelta)
    {    
        mInfo.mBusyExperienceCnt -= aDelta;
    }
    else
    {
        mInfo.mBusyExperienceCnt = 0;
    }

}

void  mmtServiceThread::decreaseBusyExperienceCnt4Task(mmcTask * aTask)
{
    UInt sBusyExprienceCnt;
    
    if(aTask != NULL)
    {
        sBusyExprienceCnt = aTask->getBusyExprienceCnt();
        if(mInfo.mBusyExperienceCnt >= sBusyExprienceCnt)
        {    
            mInfo.mBusyExperienceCnt -= sBusyExprienceCnt;
        }
        else
        {
            mInfo.mBusyExperienceCnt = 0;
        }
        aTask->decBusyExprienceCnt(sBusyExprienceCnt);
    }
}

/* PROJ-2108 Dedicated thread mode which uses less CPU */
IDE_RC mmtServiceThread::signalToServiceThread(mmtServiceThread *aThread)
{
    IDE_ASSERT(aThread->mMutexForServiceThreadSignal.lock(NULL) == IDE_SUCCESS);
    IDE_TEST(aThread->mServiceThreadCV.signal() != IDE_SUCCESS);
    IDE_ASSERT(aThread->mMutexForServiceThreadSignal.unlock() == IDE_SUCCESS);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* bug-35395: task schedule time �� sysstat�� �߰�
   ������ ������ �ð��� ���� ��������� �ݿ�(����, �ִ�).
   sStatSQL : ������ �ð�������(begin, end)�� mOpTime �� ����
   sStatSess: ����ġ, �ִ�ġ�� ������ ���� stat ����
   */
IDE_RC mmtServiceThread::addTaskScheduleTime(mmcSession* aSession)
{
    idvSQL           *sStatSQL;
    idvSession       *sStatSess;
    idvOperTimeIndex  sIndexTaskSched = IDV_OPTM_INDEX_TASK_SCHEDULE;
    ULong             sElaTime;
    ULong            *sSchedMaxTime;

    if (aSession != NULL)
    {
        sStatSQL  = aSession->getStatSQL();
        sStatSess = aSession->getStatistics();
        IDV_SQL_OPTIME_END(sStatSQL, sIndexTaskSched);

        if ( IDV_TIME_AVAILABLE() )
        {
            sElaTime = IDV_TIMEBOX_GET_ELA_TIME(
                    IDV_SQL_OPTIME_DIRECT(sStatSQL, sIndexTaskSched));

            /* ����ð��� ������ �����ð� �̻��� ��츸 ��迡 �ݿ� */
            if (sElaTime >= (ULong) mmuProperty::getMultiplexingCheckInterval())
            {
                /* ������ ������ ����ð��� ������Ų�� */
                IDV_SESS_ADD(sStatSess, IDV_STAT_INDEX_OPTM_TASK_SCHEDULE, sElaTime);

                /* ������ ������ �ִ� ����ð��� �����Ѵ� */
                sSchedMaxTime =
                    &(sStatSess->mStatEvent[IDV_STAT_INDEX_OPTM_TASK_SCHEDULE_MAX].mValue);
                if (sElaTime > *sSchedMaxTime)
                {
                    *sSchedMaxTime = sElaTime;
                }
            }

            /* idvTimeBox.mATD.mAccumTime (�����ð�)��
               OPTIME_END�ÿ� �����ð� ���ǰ�,
               endStmt�ÿ� addOpTimeToSession�� ����
               ��������� �ݿ��ȴ�.
               ������ stmt �����̰�, ������ �����̹Ƿ�
               ���⼭�� ������� �ʰ� ��� GET_ELA_TIME�� ���.
               ����, ��������� �ݿ����� �ʵ��� clear �� �ش� */
            IDV_TIMEBOX_INIT_ACCUM_TIME(
                    IDV_SQL_OPTIME_DIRECT(sStatSQL, sIndexTaskSched ) );
        }

    }

    return IDE_SUCCESS;
}

/* bug-33841: ipc thread's state is wrongly displayed
   IPC ���� �������� ���°��� ��Ȯ�ϰ� �����ϱ� ����
   ��Ŷ ���� �ٷ� �Ŀ� ȣ��Ǵ� callback �Լ��̴�
   cm���� ȣ��Ǵ� mm �Լ��̹Ƿ� �ݹ�ó���Ͽ��� */
IDE_RC mmtServiceThread::setExecuteCallback(void* aThread, void* aTask)
{
    mmtServiceThread* sThread = (mmtServiceThread*) aThread;
    mmcTask*          sTask   = (mmcTask*) aTask;

    /* IPC�� �ش�ȴ�. �� �ڵ��� ���� ��ġ��
       executeTask_READY���� cmiRead...ȣ�� �ٷ� ���̾��� */
    if ((sThread != NULL) && (sTask != NULL))
    {
        /*PROJ-2616*/
        if( (sThread->mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_IPC) ||
            (sThread->mInfo.mServiceThreadType == MMC_SERVICE_THREAD_TYPE_IPCDA))
        {
            sThread->setState(MMT_SERVICE_THREAD_STATE_EXECUTE);
            IDV_TIME_GET(&(sThread->mInfo.mExecuteBegin));
            sTask->setTaskState(MMC_TASK_STATE_EXECUTING);
        }
        else
        {
            /* nothing */
        }
    }
    else
    {
        /* nothing */
    }
    return IDE_SUCCESS;
}

