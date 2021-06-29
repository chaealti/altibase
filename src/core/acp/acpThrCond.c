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
 
/*******************************************************************************
 * $Id: acpThrCond.c 11316 2010-06-22 23:56:42Z djin $
 ******************************************************************************/

#include <acpAtomic.h>
#include <acpSpinWait.h>
#include <acpThrCond.h>


#if defined(ALTI_CFG_OS_WINDOWS) && !defined(ACP_CFG_DOXYGEN)

/*
 * Windows Vista ���� ���������� condition variable API�� �������� �ʱ� ������
 * Win32 Event ��ü�� atomic operation�� ���� �����Ѵ�.
 *
 * Win32 condition variable �������� ������ �������� �߻��� �� �����Ƿ�
 * �����ؾ� �Ѵ�.
 *
 * - lost wakeup
 *   cond_wait�Լ��� mutex unlock�� event wait���� �ٸ� �����尡 PulseEvent()
 *   �Լ��� ���� signal�� �����ϸ� cond_wait�Լ��� ����� �ʴ´�.
 *   mutex�� event�� atomic�ϰ� unlock-wait-lock���� �ʱ� ������ PulseEvent()
 *   �Լ��� ���� �ʴ� ���� ����.
 *
 * - unfairness
 *   cond_broadcast�Լ��� cond_wait�ϰ� �ִ� ��� �����带 �����ش�. ������,
 *   Win32 Event���� ��� �����带 �ѹ��� ������� auto-reset event�� �����
 *   �� ���� manual-reset event�� ����ؾ� �Ѵ�. �� ��, ��� cond_wait �����尡
 *   ����� ���� ����Ǿ�� �Ѵ�.
 *
 * - incorrectness
 *   PulseEvent()�Լ��� ������� �ʴ� �� Win32 Event�� reliable�ϴ�.
 *   ��, event wait���� set event�� �߻��ϴ��� event wait�� event�� ���´�.
 *   �ٸ� �����尡 cond_signal�̳� cond_broadcastȣ�� �Ŀ� cond_wait�� �����ϴ�
 *   �����尡 wakeup�Ǹ� �ȵȴ�.
 *
 * - busy waiting
 *   unfairness��(��) incorrectness�� �����ϱ� ���� busy wait�� �ؾ��ϴ� ��찡
 *   ���� �� �ִ�. �ּ����� busy wait�� �ϵ��� �����ؾ� �Ѵ�.
 */

static acp_rc_t acpThrCondTimedWaitInternal(acp_thr_cond_t  *aCond,
                                            acp_thr_mutex_t *aMutex,
                                            DWORD            aMsec)
{
    DWORD        sRet;
    acp_sint32_t sWaiterRemain;
    acp_sint32_t sIsBroadcast;
    acp_rc_t     sRC = ACP_RC_SUCCESS;

    /*
     * [1] WaiterCount Increase
     */
    (void)acpAtomicInc32(&aCond->mWaiterCount);

    /*
     * [2] Mutex Unlock
     */
    (void)acpThrMutexUnlock(aMutex);

    /*
     * [3] Signal Wait
     *
     * cond_signal/cond_broadcast�� ȣ��� ������ �־��� timeout��ŭ ���
     */
    sRet = WaitForSingleObject(aCond->mSemaphore, aMsec);

    /*
     * [4] WaiterCount Decrease
     */
    sWaiterRemain = acpAtomicDec32(&aCond->mWaiterCount);

    /*
     * [5] Event To Signaling Thread
     *
     * cond_signal/cond_broadcast�����忡 wakeup�� �Ϸ�Ǿ����� event�� �˷���
     *
     * cond_broadcast�� ��� cond_wait�ϴ� ��� �����尡 ������� event�� ����
     */
    sIsBroadcast = acpAtomicGet32(&aCond->mIsBroadcast);
    switch(sRet)
    {
        case WAIT_OBJECT_0:
            if ((sWaiterRemain == 0) ||
                (sIsBroadcast == 0))
            {
                ACP_TEST_RAISE(SetEvent(aCond->mWaitDone) == 0, ERROR_SETEVENT);
            }
            else
            {
                /* do nothing */
            }
            break;
        case WAIT_TIMEOUT:
            if ((sWaiterRemain == 0) &&
                (sIsBroadcast == 1))
            {
                /*
                 * This condition could be reached only in case when
                 * semaphore released but we already stoped waiting it.
                 * So we should take semaphore and report to broadcast
                 * thread about it to avoid deadlock.
                 */
                sRet = WaitForSingleObject(aCond->mSemaphore, INFINITE);
                /* Set event without check of sRC to avoid deadlock */
                ACP_TEST_RAISE(SetEvent(aCond->mWaitDone) == 0, ERROR_SETEVENT);
                if (sRet != WAIT_OBJECT_0)
                {
                    /*
                     * If no semaphore set at this moment
                     * than somthing strange happens.
                     */
                    sRC = ACP_RC_EINVAL;
                }
                else
                {
                    /* do nothing */
                }
            }
            else
            {
                sRC = ACP_RC_ETIMEDOUT;
            }
            break;
        case WAIT_FAILED:
            {
                sRC = ACP_RC_GET_OS_ERROR();
            }
            break;
        default:
            {
                sRC = ACP_RC_EINVAL;
            }
            break;
    }

    /*
     * [6] Mutex Lock & Return
     */
    (void)acpThrMutexLock(aMutex);

    ACP_EXCEPTION(ERROR_SETEVENT)
    {
        sRC = ACP_RC_GET_OS_ERROR();
    }

    ACP_EXCEPTION_END;

    return sRC;
}

ACP_EXPORT acp_rc_t acpThrCondCreate(acp_thr_cond_t *aCond)
{
    acp_rc_t sRC;

    aCond->mIsBroadcast = 0;
    aCond->mWaiterCount = 0;

    /*
     * cond_signal/cond_broadcast��
     * cond_wait�����带 ����� ���� ����ϴ� semaphore
     */
    aCond->mSemaphore = CreateSemaphore(NULL, 0, ACP_SINT32_MAX, NULL);

    if (aCond->mSemaphore == NULL)
    {
        return ACP_RC_GET_OS_ERROR();
    }
    else
    {
        /* do nothing */
    }

    /*
     * cond_signal/cond_broadcast��
     * cond_wait������ ����Ⱑ �Ϸ�� ������ ��ٸ��� ���� event
     */
    aCond->mWaitDone = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (aCond->mWaitDone == NULL)
    {
        sRC = ACP_RC_GET_OS_ERROR();

        (void)CloseHandle(aCond->mSemaphore);

        return sRC;
    }
    else
    {
        /* do nothing */
    }

    return ACP_RC_SUCCESS;
}

ACP_EXPORT acp_rc_t acpThrCondDestroy(acp_thr_cond_t *aCond)
{
    (void)CloseHandle(aCond->mWaitDone);
    (void)CloseHandle(aCond->mSemaphore);

    return ACP_RC_SUCCESS;
}

ACP_EXPORT acp_rc_t acpThrCondWait(acp_thr_cond_t  *aCond,
                                   acp_thr_mutex_t *aMutex)
{
    if (aCond == NULL || aMutex == NULL)
    {
        return ACP_RC_EFAULT;
    }
    else
    {
        /* do nothing */
    }

    return acpThrCondTimedWaitInternal(aCond, aMutex, INFINITE);
}

ACP_EXPORT acp_rc_t acpThrCondTimedWait(acp_thr_cond_t  *aCond,
                                        acp_thr_mutex_t *aMutex,
                                        acp_time_t       aTimeout,
                                        acp_time_type_t  aTimeoutType)
{
    DWORD sMsec;

    if (aCond == NULL || aMutex == NULL)
    {
        return ACP_RC_EFAULT;
    }
    else
    {
        /* do nothing */
    }

    /*
     * microsecond������ timeout�� millisecond������ ��ȯ
     */
    if (aTimeout == ACP_TIME_INFINITE)
    {
        sMsec = INFINITE;
    }
    else
    {
        /*
         * absolute timeout�� relative timeout���� ��ȯ
         */
        if (aTimeoutType == ACP_TIME_ABS)
        {
            aTimeout -= acpTimeNow();

            if (aTimeout < 0)
            {
                aTimeout = 0;
            }
            else
            {
                /* do nothing */
            }
        }
        else
        {
            /* do nothing */
        }

        sMsec = (DWORD)((aTimeout + 999) / 1000);
    }

    return acpThrCondTimedWaitInternal(aCond, aMutex, sMsec);
}

ACP_EXPORT acp_rc_t acpThrCondSignal(acp_thr_cond_t *aCond)
{
    BOOL sRet;

    if (aCond == NULL)
    {
        return ACP_RC_EFAULT;
    }
    else
    {
        /* do nothing */
    }

    /*
     * cond_wait�ϴ� �����尡 ������ �ƹ��͵� ���� ����
     */
    if (acpAtomicGet32(&aCond->mWaiterCount) > 0)
    {
        /*
         * cond_wait�ϴ� ������ �� �ϳ��� ����
         */
        sRet = ReleaseSemaphore(aCond->mSemaphore, 1, 0);

        if (sRet == 0)
        {
            return ACP_RC_GET_OS_ERROR();
        }
        else
        {
            /* do nothing */
        }

        /*
         * cond_wait�����尡 ��� ������ ���
         */
        (void)WaitForSingleObject(aCond->mWaitDone, INFINITE);
    }
    else
    {
        /* do nothing */
    }

    return ACP_RC_SUCCESS;
}

ACP_EXPORT acp_rc_t acpThrCondBroadcast(acp_thr_cond_t *aCond)
{
    acp_sint32_t sWaiterCount;
    BOOL         sRet;

    if (aCond == NULL)
    {
        return ACP_RC_EFAULT;
    }
    else
    {
        /* do nothing */
    }

    /*
     * cond_wait�ϴ� �����尡 ������ �ƹ��͵� ���� ����
     */
    sWaiterCount = acpAtomicGet32(&aCond->mWaiterCount);

    if (sWaiterCount > 0)
    {
        /*
         * ��� cond_wait�ϴ� �����尡 ��� �� WaitDone event�� �߻���Ű����
         * IsBroadcast flag�� 1�� ����
         */
        (void)acpAtomicSet32(&aCond->mIsBroadcast, 1);

        /*
         * ���� cond_wait�ϰ� �ִ� �����带 ��� ����
         */
        sRet = ReleaseSemaphore(aCond->mSemaphore, sWaiterCount, 0);

        if (sRet == 0)
        {
            return ACP_RC_GET_OS_ERROR();
        }
        else
        {
            /* do nothing */
        }

        /*
         * ��� cond_wait�����尡 ��� ������ ���
         */
        (void)WaitForSingleObject(aCond->mWaitDone, INFINITE);

        /*
         * IsBroadcast flag�� 0���� ����
         */
        (void)acpAtomicSet32(&aCond->mIsBroadcast, 0);
    }
    else
    {
        /* do nothing */
    }

    return ACP_RC_SUCCESS;
}

#endif
