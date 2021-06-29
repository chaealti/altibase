/***********************************************************************
 * Copyright 1999-2000, ALTIBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduLatchTypeNative.c 66837 2014-09-24 02:05:41Z returns $
 **********************************************************************/

#include <iduLatchObj.h>
#include <iduBridge.h>

/* =============================================================================
 *
 *  Primitive Mutex Implementations
 *
 *  => ��� �÷����� �Ʒ��� 3���� Define�� 3���� �Լ��� �����ϸ�,
 *     �ڵ����� Latch�� ������ �����ǵ��� �Ǿ� �ִ�.
 *  Wrappers Implementations
 *
 *  - Used Symbol
 *    1. iduNativeMutexObj : Primitive Mutex Values
 *    2. IDU_LATCH_IS_UNLOCKED(a) : whether the mutex is unlocked.
 *    3. IDU_HOLD_POST_OPERATION(a) : sync op after hold mutex.
 *    4. initNativeMutex();     init value
 *    5. tryHoldPricMutex()   try Hold Native Mutex
 *    6. releaseNativeMutex();  Release Native Mutex
 *
 * =========================================================================== */

#include "iduNativeMutex.ic"

/* ------------------------------------------------
 *  Common Operation 
 *  holdMutex()
*   sleepForNativeMutex()
 
 * ----------------------------------------------*/

#include "iduNativeMutex-COMMON.ic"

/* =============================================================================
 *
 *  Wrappers Implementations
 *
 * =========================================================================== */

static UInt gLatchSpinCount = 1;

static void initializeStaticNativeLatch(UInt aSpinCount)
{
    gLatchSpinCount = (aSpinCount == 0 ? 1 : aSpinCount);
}

static void destroyStaticNativeLatch()
{}

static void initializeNativeLatch(iduLatchObj *aLatch, SChar *aName)
{
    iduNativeMutexObj *aObj = (iduNativeMutexObj *)IDU_LATCH_CORE(aLatch);

    /* To remove compilation warining, aName is not used currently */
    (void)(aName);

    initNativeMutex(aObj);
}

/*
 *  destroy();
 *  Latch ��ü�� �Ҹ��Ų��..
 */
static void destroyNativeLatch(iduLatchObj *aLatch)
{
    /* To Remove Compile Error */
    (void)(aLatch);
}

/*
 *  sleepForValueChange();
 *  Latch�� Ǯ���� ���� ���
 *  ��� �ð��� 200 usec���� ���� *2 �� �����Ͽ�,
 *  �ִ� 99999 usec���� �����Ѵ�.
 */

static void sleepForLatchValueChange(iduLatchObj *aObj,
                                     UInt         aFlag /* 0 : read 1 : write */
                                     )
{
    UInt   i;
    UInt   sStart = iduLatchMinSleep();
    UInt   sMax   = iduLatchMaxSleep();
    UInt   sType  = iduMutexSleepType();
    /* busy wait */
    while(1)
    {
        for (i = 0; i < gLatchSpinCount; i++)
        {
            if (aFlag == 0)
            {
                /*  read wait.. */
                if (aObj->mMode >= 0)
                {
                    return;
                }
            }
            else
            {
                /*  write wait */
                if (aObj->mMode == 0)
                {
                    return;
                }
            }
        }

        if(sType == IDU_MUTEX_SLEEP)
        {
            idlOS_sleep(0, sStart);
            sStart = ( (sStart * 2) > sMax) ? sMax : sStart * 2;
        }
        else
        {
            idlOS_thryield();
        }
    }
}


/*
 *  lockReadInternal();
 *  read latch�� �����ϴ� ���� �Լ��̸�, �ܺο��� ���� �Ұ����ϴ�.
 *  �Էµ� �μ��� ���� ���Ѵ�� Ȥ�� �������˻縦 �����ϸ�,
 *  ������ ��� ����/������ �÷��׸� �����Ѵ�.
 */

static IDE_RC lockReadInternalNativeLatch (iduLatchObj *aLatch,
                                           idBool       aWaitForever,
                                           void*       aStatSQL,
                                           void*       aWeArgs,
                                           idBool      *aSuccess)
{
    iduNativeMutexObj *aObj = (iduNativeMutexObj *)IDU_LATCH_CORE(aLatch);

    UInt sOnce = 0;

    holdNativeMutex(aObj, gLatchSpinCount);

    if( (aLatch->mMode >= 0) || (aLatch->mWriteThreadID != idlOS_getThreadID()))
    {
        aLatch->mGetReadCount++;
    }
    else
    {
        /*
         * BUG-30204 ���� ���� �ڽ��� �̹� X Latch�� ��� �ִ� ���
         *           ����ִ� X Latch�� Count�� �߰��Ѵ�.
         * X Latch�� ������ ó��
         */
        aLatch->mMode--;
        aLatch->mGetWriteCount++;
        *aSuccess = ID_TRUE;
        goto my_break;
    }

    while (aLatch->mMode < 0) /* now write latch held */
    {
        if (sOnce++ == 0)
        {
            aLatch->mReadMisses++;
        }

        if (aWaitForever == ID_TRUE)
        {
            aLatch->mSleeps++;
            releaseNativeMutex(aObj);
            if ( sOnce++ == 1 )
            {
                /* SpinLock�� Wait Event Time�� �������� �ʴ´�. */
                idv_BEGIN_WAIT_EVENT( aStatSQL, aWeArgs );
            }
            sleepForLatchValueChange(aLatch, 0);
            holdNativeMutex(aObj, gLatchSpinCount);
            continue;
        }
        else
        {
            *aSuccess = ID_FALSE;
            goto my_break;
        }
    }

    if ( sOnce > 1 )
    {
        /* Wait Event Time �����Ϸ��Ѵ�. */
        idv_END_WAIT_EVENT( aStatSQL, aWeArgs );
    }
    
    *aSuccess = ID_TRUE;

    aLatch->mMode++;

    IDE_DCASSERT(aLatch->mMode > 0);

  my_break:;
    releaseNativeMutex(aObj);

    return IDE_SUCCESS;
}

/*
 *  lockWriteInternal();
 *  write latch�� �����ϴ� ���� �Լ��̸�, �ܺο��� ���� �Ұ����ϴ�.
 *  �Էµ� �μ��� ���� ���Ѵ�� Ȥ�� �������˻縦 �����ϸ�,
 *  ������ ��� ����/������ �÷��׸� �����Ѵ�.
 */
static IDE_RC lockWriteInternalNativeLatch (iduLatchObj *aLatch,
                                           idBool       aWaitForever,
                                           void        *aStatSQL,
                                           void        *aWeArgs,
                                           idBool      *aSuccess)
{
    UInt sOnce = 0;
    ULong sThreadID;
    iduNativeMutexObj *aObj = (iduNativeMutexObj *)IDU_LATCH_CORE(aLatch);

    sThreadID = idlOS_getThreadID();

    /* ---------- */

    holdNativeMutex(aObj, gLatchSpinCount);

    aLatch->mGetWriteCount++;

    /* somebody held it except me for xlock! */
    while ( (aLatch->mMode > 0) ||   /* latched on shared mode  or  */
            (aLatch->mMode < 0 &&
             (aLatch->mWriteThreadID != sThreadID)
             )
            )
    {
        if (sOnce++ == 0)
        {
            aLatch->mWriteMisses++;
        }

        if (aWaitForever == ID_TRUE)
        {
            aLatch->mSleeps++;
            releaseNativeMutex(aObj);
            if ( sOnce++ == 1 )
            {
                /* SpinLock�� Wait Event Time�� �������� �ʴ´�. */
                idv_BEGIN_WAIT_EVENT( aStatSQL, aWeArgs );
            }
            sleepForLatchValueChange(aLatch, 1);
            holdNativeMutex(aObj, gLatchSpinCount);
        }
        else
        {
            *aSuccess = ID_FALSE;
            goto my_break;
        }
    }

    if ( sOnce > 1 )
    {
        /* Wait Event Time �����Ϸ��Ѵ�. */
        idv_END_WAIT_EVENT( aStatSQL, aWeArgs );
    }
    *aSuccess = ID_TRUE;

    aLatch->mMode--;

    aLatch->mWriteThreadID = sThreadID;

  my_break:;

    releaseNativeMutex(aObj);

    return IDE_SUCCESS;
}

/*
 *  unlock();
 *
 *  ���� latch�� �����Ѵ�.
 *
 *  ��Ȯ�� ������ �����ϱ� ���ؼ��� lock��  �����ߴ� ������ ���̵�
 *  ��� �����ϰ�, Unlock�ÿ� ������ Lock�� �����ߴ� ����������
 *  �˻縦 �ؾ� �ϳ�, ����� �ʹ� ���� ��� ������,
 *  Write Latch�� ��쿡�� �ڽ��� ���� Latch�� ���� UnLock���� �˻��Ͽ���.
 *
 */

static IDE_RC unlockNativeLatch (iduLatchObj *aLatch)
{
    iduNativeMutexObj *aObj = (iduNativeMutexObj *)IDU_LATCH_CORE(aLatch);
    ULong sThreadID;

    sThreadID = idlOS_getThreadID();

    holdNativeMutex(aObj, gLatchSpinCount);

    IDE_DCASSERT(aLatch->mMode != 0);

    if (aLatch->mMode > 0) /* for read */
    {
        aLatch->mMode--; /*   decrease read latch count */
    }
    else /*   for write */
    {
        IDE_DCASSERT(aLatch->mMode < 0);

        IDE_DASSERT(aLatch->mWriteThreadID == sThreadID);

        aLatch->mMode++; /*   Decrease write latch count */
        if (aLatch->mMode == 0)
        {
            aLatch->mWriteThreadID = 0;
        }
    }

    releaseNativeMutex(aObj);

    return IDE_SUCCESS;
}


iduLatchFunc gNativeLatchFunc =
{
    initializeStaticNativeLatch,
    destroyStaticNativeLatch,
    initializeNativeLatch,
    destroyNativeLatch,
    lockReadInternalNativeLatch,
    lockWriteInternalNativeLatch,
    unlockNativeLatch
};




