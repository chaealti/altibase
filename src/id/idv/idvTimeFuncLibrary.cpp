/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: idvTimeFuncLibrary.cpp 35244 2009-09-02 23:52:47Z orc $
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <idv.h>
#include <idp.h>
#include <idu.h>
#include <idv.h>

/* ------------------------------------------------
 * Library
 * ----------------------------------------------*/

static void gLibraryInit(idvTime *aValue)
{
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS) || defined(OS_LINUX_KERNEL) || defined(WRS_VXWORKS)
    aValue->iTime.mSpec.tv_sec = 0;
    aValue->iTime.mSpec.tv_nsec = 0;
#elif defined(IBM_AIX)
    aValue->iTime.mSpec.tb_high = 0;
    aValue->iTime.mSpec.tb_low  = 0;
// bug-23877 v$statement total_time is 0 when short SQL on hp-ux
// hp-ux������ time interval ���ϴ� ��� ���� (thread -> library)
#elif defined(HP_HPUX) || defined(IA64_HP_HPUX)
    aValue->iTime.mClock = 0;
#else

#endif
}

static idBool gLibraryIsInit(idvTime *aValue)
{
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS) || defined(OS_LINUX_KERNEL) || defined(WRS_VXWORKS)
    return (aValue->iTime.mSpec.tv_sec == 0 && aValue->iTime.mSpec.tv_nsec == 0) ?
        ID_TRUE : ID_FALSE;
#elif defined(IBM_AIX)
    return (aValue->iTime.mSpec.tb_high == 0 && aValue->iTime.mSpec.tb_low == 0) ?
        ID_TRUE : ID_FALSE;
#elif defined(HP_HPUX) || defined(IA64_HP_HPUX)
    return (aValue->iTime.mClock == 0) ? ID_TRUE : ID_FALSE;
#else
    return ID_FALSE;
#endif
}

static void gLibraryGet(idvTime *aValue)
{
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS) || defined(WRS_VXWORKS)
    clock_gettime(CLOCK_REALTIME, &(aValue->iTime.mSpec));
#elif defined(OS_LINUX_KERNEL)
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &(aValue->iTime.mSpec));
#elif defined(IBM_AIX)
    read_real_time(&(aValue->iTime.mSpec), TIMEBASE_SZ);
    time_base_to_time(&((aValue)->iTime.mSpec), TIMEBASE_SZ);
// =============================================================
// bug-23877 v$statement total_time is 0 when short SQL on hp-ux
// hp-ux������ time interval ���ϴ� ��� ���� (thread -> library)
// gethrtime()   : 130 cycles (return nano secs)
// hrtime_t now = gethrtime(); (hrtime_t: int64_t => ULong)
// ref) hp ����(HowToTellTheTime.doc)�� ���� ������ ���� ������ �ִ�.
// gethrtime() is an excellent interface to use to determine an ultra-high
// resolution time. It is accurate across CPUs, and is very cheap
// It is ideal in particular for timing events.
// cf) hg_gethrtime(): 61 cycles (HP-UX 11.31 only)
#elif defined(HP_HPUX) || defined(IA64_HP_HPUX)
    aValue->iTime.mClock = (PDL_OS::gethrtime()) / ((ULong)1000); // micro sec
#else

#endif
}

// ( �� -> micro �� ) ��ȯ ���̺�
ULong gIdvSec2MicroTable[] = {
          0,
    1000000,
    2000000,
    3000000,
    4000000,
    5000000,
    6000000,
    7000000,
    8000000,
    9000000,
};

#define IDV_SEC2MICRO_TABLE_SIZE (10)

/*
 * second => micro second���� ��ȯ
 *
 * aSec - ��ȯ�� ��
 */
static inline SLong sec2micro ( SLong aSec )
{
    if ( ( aSec >= 0 ) &&
         ( aSec < IDV_SEC2MICRO_TABLE_SIZE ) )
    {
        return gIdvSec2MicroTable[ aSec ];
    }
    else
    {
        return aSec * 1000000;
    }
}

/*
 * nano second => micro second���� ��ȯ
 *
 * aSec - ��ȯ�� nano ��
 */
static inline SLong nano2micro( SLong aNanoSec )
{
    return aNanoSec / 1000;
}

/*
 * �� �ð� ���� �ð����̸� Micro Second������ �����Ѵ�.
 *
 * aBefore : ���Ϸ��� �ð� �� ���� ���� ������ �ð�
 * aAfter  : ���Ϸ��� �ð� �� ū���� ������ �ð�
 */
static ULong gLibraryDiff( idvTime *aBefore, idvTime *aAfter )
{
    if ( IDV_TIME_AVAILABLE() )
    {
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS) || defined(OS_LINUX_KERNEL) || defined(WRS_VXWORKS) || defined(VC_WINCE) || defined(SYMBIAN)

        if ((aAfter)->iTime.mSpec.tv_sec < (aBefore)->iTime.mSpec.tv_sec)
        {
            return 0;
        }

        return  (ULong)(  sec2micro( (SLong)  ( (aAfter)->iTime.mSpec.tv_sec -
                        (aBefore)->iTime.mSpec.tv_sec ) )
                + nano2micro( (SLong) ( (aAfter)->iTime.mSpec.tv_nsec -
                        (aBefore)->iTime.mSpec.tv_nsec ) )
                );
#elif defined(IBM_AIX)

        /* BUG-24325: gLibraryDiff�Լ����� IDE_DASSRET�� �ɷ��� �׽��ϴ�.
         *
         * Before�� After���� ũ�� 0�� �����ϵ��� ��.
         * */
        if( (aAfter)->iTime.mSpec.tb_high < (aBefore)->iTime.mSpec.tb_high )
        {
            return 0;
        }

        return  (ULong)(  sec2micro( (SLong)  ( (aAfter)->iTime.mSpec.tb_high -
                        (aBefore)->iTime.mSpec.tb_high ) )
                //fix BUG-18636
                + nano2micro( SLong ((aAfter)->iTime.mSpec.tb_low) -
                    SLong ((aBefore)->iTime.mSpec.tb_low) )
                );
#elif defined(HP_HPUX) || defined(IA64_HP_HPUX)

        /* BUG-24325: gLibraryDiff�Լ����� IDE_DASSRET�� �ɷ��� �׽��ϴ�.
         *
         * Before�� After���� ũ�� 0�� �����ϵ��� ��.
         * */
        if( aAfter->iTime.mClock < aBefore->iTime.mClock )
        {
            return 0;
        }

        return (aAfter->iTime.mClock - aBefore->iTime.mClock);
#else
        // �ٸ� �÷����� ��� �� �Լ��� ���ͼ��� �ȵȴ�.
        IDE_ASSERT(0);
#endif
    }

    return 0;
}

/*
 * (second, nansecond)�� �̷���� �ϳ��� idvTime�� micro second�� ��ȯ�Ѵ�.
 *
 * aValue - ��ȯ�ϰ��� �ϴ� idvTime
 */

static ULong gLibraryMicro(idvTime *aValue)
{
    if ( IDV_TIME_AVAILABLE() )
    {
#if defined(SPARC_SOLARIS) || defined(X86_SOLARIS) || defined(OS_LINUX_KERNEL) || defined(WRS_VXWORKS) || defined(VC_WINCE) || defined(SYMBIAN)
    return (ULong)( sec2micro( (SLong) (aValue)->iTime.mSpec.tv_sec ) +
                    //fix BUG-18636
                    (SLong)((aValue)->iTime.mSpec.tv_nsec / 1000));
#elif defined(IBM_AIX)
    return (ULong)( sec2micro( (SLong) (aValue)->iTime.mSpec.tb_high ) +
                    //fix BUG-18636
                    (SLong)((aValue)->iTime.mSpec.tb_low / 1000));
#elif defined(HP_HPUX) || defined(IA64_HP_HPUX)
    return aValue->iTime.mClock;
#else
        // �ٸ� �÷����� ��� �� �Լ��� ���ͼ��� �ȵȴ�.
        IDE_ASSERT(0)
#endif
    }

    return 0;
}

static idBool gLibraryIsOlder(idvTime *aBefore, idvTime *aAfter)
{
    return (gLibraryMicro(aBefore) > gLibraryMicro(aAfter)) ? ID_TRUE : ID_FALSE;
}


idvTimeFunc gLibraryFunctions =
{
    gLibraryInit,
    gLibraryIsInit,
    gLibraryGet,
    gLibraryDiff,
    gLibraryMicro,
    gLibraryIsOlder
};
