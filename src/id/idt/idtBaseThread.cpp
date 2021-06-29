/*****************************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 ****************************************************************************/

/*****************************************************************************
 * $Id: idtBaseThread.cpp 82088 2018-01-18 09:21:15Z yoonhee.kim $
 ****************************************************************************/

/*****************************************************************************
 *   NAME
 *     idtBaseThread.cpp - ������ �⺻ Ŭ���� 
 *
 *   DESCRIPTION
 *      iSpeener���� ���� ��� ��������� Base Thread Class�� ����
 *
 *   PUBLIC FUNCTION(S)
 *
 *   PRIVATE FUNCTION(S)
 *
 *   NOTES
 *
 *   MODIFIED   (MM/DD/YY)
 ****************************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idtBaseThread.h>
#include <idtContainer.h>
#include <idErrorCode.h>
#include <iduProperty.h>
#include <iduFitManager.h>

/*----------------------- idtBaseThread ������--------------------------------
     NAME
	     idtBaseThread ������

     DESCRIPTION

     ARGUMENTS

     RETURNS
     	����
----------------------------------------------------------------------------*/

idtBaseThread::idtBaseThread(SInt aFlag)
{
#define IDE_FN "idtBaseThread::idtBaseThread(SInt Flags, SInt Priority, void *Stack, UInt StackSize)"
    mContainer = NULL;
    mIsServiceThread = ID_FALSE;

    switch(aFlag)
    {
    case IDT_JOINABLE:
        mIsJoin = ID_TRUE;
        break;
    case IDT_DETACHED:
        mIsJoin = ID_FALSE;
        break;
    default:
        IDE_ASSERT(0);
    }
#undef IDE_FN
}

PDL_thread_t idtBaseThread::getTid()
{
    return (mContainer==NULL)? (PDL_thread_t)-1:(mContainer->getTid());
}

PDL_hthread_t idtBaseThread::getHandle()
{
    return (mContainer==NULL)? (PDL_hthread_t)-1:(mContainer->getHandle());
}

idBool idtBaseThread::isStarted()
{
    return (mContainer==NULL)? ID_FALSE:(mContainer->isStarted());
}

IDE_RC idtBaseThread::join()
{
    idtContainer* sContainer = mContainer;

    IDE_TEST_RAISE(sContainer == NULL, ENOTSTARTED);
    IDE_TEST_RAISE(mIsJoin !=  ID_TRUE, EUNBOUNDTHREAD);
    IDE_TEST_RAISE(sContainer->join() != IDE_SUCCESS, ESYSTEMERROR);
    mContainer = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION(ENOTSTARTED)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_THREAD_NOTSTARTED));
    }
    IDE_EXCEPTION(EUNBOUNDTHREAD)
    {
        IDE_SET(ideSetErrorCode(idERR_IGNORE_THREAD_UNBOUND));
    }
    IDE_EXCEPTION(ESYSTEMERROR)
    {
        IDE_SET(ideSetErrorCode(idERR_FATAL_THREAD_JOINERROR));
    }

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
/*--------------------------- start  ---------------------------------------
     NAME
	     start()

     DESCRIPTION
     	�ش� Ŭ������ ������� ���۽�Ų��. (run()�� ȣ���)
        
     ARGUMENTS
     	����
        
     RETURNS
     	idlOS::thr_create() �Լ��� ���ϰ�
----------------------------------------------------------------------------*/
IDE_RC idtBaseThread::start()
{
    idtContainer* sContainer;

#define IDE_FN "SInt idtBaseThread::start()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

#if defined(WRS_VXWORKS) 
    static UInt gTid = 0; 

    if( taskLock() == OK ) 
    { 
        idlOS::snprintf( handle_.name, 
                         sizeof(handle_.name), 
                         "aSrv$%"ID_INT32_FMT"", 
                         gTid++ ); 
        tid_ = handle_.name; 
        (void)taskUnlock(); 
    } 
#endif 

    IDE_TEST_RAISE( idtContainer::pop( &sContainer ) != IDE_SUCCESS, create_error );

    /* TC/FIT/Limit/BUG-16241/BUG-16241.sql */
    IDU_FIT_POINT_RAISE( "idtBaseThread::start::Thread::sContainer", create_error );
    mAffinity.copyFrom(idtCPUSet::mProcessPset);

    IDE_TEST_RAISE(sContainer->start( (void*)this ) != IDE_SUCCESS, create_error);

    return IDE_SUCCESS;

    IDE_EXCEPTION(create_error);
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_THR_CREATE_FAILED));
    }
    IDE_EXCEPTION_END;
   
    return IDE_FAILURE;
    

#undef IDE_FN
}

#define IDT_WAIT_LOOP_PER_SECOND     10
// ������� ������ �� ���� ���
IDE_RC idtBaseThread::waitToStart(UInt second)
{

#define IDE_FN "SInt idtBaseThread::waitToStart()"
    IDE_MSGLOG_FUNC(IDE_MSGLOG_BODY(""));

#if defined(ITRON)
    /* empty */
    return IDE_SUCCESS;
#else
    static PDL_Time_Value waitDelay;
    UInt i;
    SInt j;
    
    waitDelay.initialize(0, 1000000 / IDT_WAIT_LOOP_PER_SECOND);
    if (second == 0) second = UINT_MAX;
    
    for (i = 0; i < second; i++)
    {
        for (j = 0; j < IDT_WAIT_LOOP_PER_SECOND; j++)
        {
            if (isStarted() == ID_TRUE)
            {
                return IDE_SUCCESS;
            }
            idlOS::sleep(waitDelay);
        }
    }
    
    IDE_SET(ideSetErrorCode(idERR_FATAL_THR_NOT_STARTED, second));
    
    return IDE_FAILURE;
#endif

#undef IDE_FN
}

/*
 * TASK-6764 CPU Affinity interfaces
 */

/*
 * ���� �����尡 aCPUSet�� ������ CPU���� �۵��ϵ���
 * affinity�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * aCPUSet ������ CPU�� �ϳ��̸� ��� �ü������ �۵��Ѵ�.
 * aCPUSet ������ CPU�� �� �̻��̸� Linux������ �۵��ϸ� �ٸ�
 * �ü�������� TRC ���� �޽����� ����ϰ� IDE_FAILURE�� �����Ѵ�.
*/
IDE_RC idtBaseThread::setAffinity(idtCPUSet& aCPUSet)
{
    IDE_TEST( aCPUSet.bindThread() != IDE_SUCCESS );
    mAffinity.copyFrom(aCPUSet);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ���� �����尡 aCPUNo���� �۵��ϵ��� affinity�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * ��� �ü������ �۵��Ѵ�.
 */
IDE_RC idtBaseThread::setAffinity(const SInt aCPUNo)
{
    idtCPUSet sCPUSet(IDT_EMPTY);
    sCPUSet.addCPU(aCPUNo);
    IDE_TEST( sCPUSet.bindThread() != IDE_SUCCESS );
    mAffinity.copyFrom(sCPUSet);
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ���� �����尡 aNUMANo�� �ش��ϴ� CPU���� �۵��ϵ���
 * affinity�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * Linux������ �۵��ϸ� �ٸ� �ü��������
 * TRC ���� �޽����� ����ϰ� IDE_FAILURE�� �����Ѵ�.
 */
IDE_RC idtBaseThread::setNUMAAffinity(const SInt aNUMANo)
{
    return setAffinity(idtCPUSet::mNUMAPsets[aNUMANo]);
}

/*
 * ���� �������� affinity�� �˾Ƴ� aCPUSet�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * unbind ���¶�� ���� DBMS ������ ��� ������
 * CPU Set�� aCPUSet�� ����ȴ�.
 * ���̼����� CPU ������ ������ ��� ��� ������ CPU�鸸�� ����ȴ�.
 */
IDE_RC idtBaseThread::getAffinity(idtCPUSet& aCPUSet)
{
    aCPUSet.copyFrom(mAffinity);
    return IDE_SUCCESS;
}

/*
 * ���� �������� affinity�� �˾Ƴ� aCPUSet�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * unbind ���¶�� ���� DBMS ������ ��� ������
 * CPU Set�� aCPUSet�� ����ȴ�.
 * ���̼����� CPU ������ ������ ��� ��� ������ CPU�鸸�� ����ȴ�.
 */
IDE_RC idtBaseThread::getAffinity(idtCPUSet* aCPUSet)
{
    aCPUSet->copyFrom(mAffinity);
    return IDE_SUCCESS;
}

/*
 * ���� �������� affinity�� �˾Ƴ� aCPUNo�� �����Ѵ�.
 * idtBaseThread�� ��ӹ��� Ŭ���������� ��� �����ϴ�.
 * ���� �����尡 �ϳ��� CPU���� bind�Ǿ� �ִٸ�
 * �ش� CPU ��ȣ�� aCPUNo�� �����Ѵ�.
 * �� �̻��� CPU�� bind�Ǿ� �ְų� unbind ���¶��
 * -1�� �����ϰ� * trc ���� �޽����� ����� ��
 * IDE_FAILURE�� �����Ѵ�.
 */
IDE_RC idtBaseThread::getAffinity(SInt& aCPUNo)
{
    IDE_TEST(mAffinity.getCPUCount() == 0);
    IDE_TEST(mAffinity.getCPUCount() >  1);

    aCPUNo = mAffinity.findFirstCPU();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC idtBaseThread::getAffinity(SInt* aCPUNo)
{
    IDE_TEST(mAffinity.getCPUCount() == 0);
    IDE_TEST(mAffinity.getCPUCount() >  1);

    *aCPUNo = mAffinity.findFirstCPU();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * ���� �������� CPU Affinity�� �����Ѵ�.
 */
IDE_RC idtBaseThread::unbindAffinity(void)
{
    return idtCPUSet::unbindThread();
}


