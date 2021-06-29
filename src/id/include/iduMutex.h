/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMutex.h 86279 2019-10-15 01:10:42Z hykim $
 **********************************************************************/
#ifndef _O_IDU_MUTEX_H_
#define _O_IDU_MUTEX_H_ 1

#include <idl.h>
#include <iduMutexMgr.h>

class iduMutex
{
public:

    /*
     * mutex�� ������ ������ wait event�� ����Ѵ�.
     * wait event�� idv.h�� ���� �����ϰų� �����Ѵ�
     */
    IDE_RC initialize(const SChar *aName,
                      iduMutexKind aKind,
                      idvWaitIndex aWaitEventID );
    
    IDE_RC destroy();
    
    inline IDE_RC trylock(idBool        & bLock );

    /*
     * �ʱ�ȭ�� ��õ� wait event�� wait time�� �����Ϸ���
     * Session ����ڷᱸ���� idvSQL�� ���ڷ� �����ؾ��Ѵ�.
     * TIMED_STATISTICS ������Ƽ�� 1 �̾�� �����ȴ�. 
     */
    inline IDE_RC lock( idvSQL        * aStatSQL );

    inline IDE_RC unlock();

    void  status() {}
    
    inline PDL_thread_mutex_t* getMutexForCond();
    inline iduMutexStat* getMutexStat();
    /* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
    /* Reset statistic info and change name of a Mutex */
    /* ex> mmcMutexPool use these functions for init. a mutex */
    inline void resetStat();
    inline void setName( SChar *aName );
    
    // BUGBUG : BUG-14263
    // for make POD class type, remove copy operator.
    //// Prohibit assign : To Fix PR-4073
    // iduMutex& operator=(const iduMutex& ) { return  *this;};

    static inline IDE_RC unlock( void *aMutex, UInt, void * );
    static inline idBool isMutexSame( void *aLhs, void *aRhs );
    static IDE_RC dump( void * ) { return IDE_SUCCESS; }

private:
    // BUGBUG : BUG-14263
    // for make POD class type, remove copy operator.
    ////Prohibit automatic compile generation of these member function
    //iduMutex(const iduMutex& );
    
public:  /* POD class type should make non-static data members as public */
    iduMutexEntry    *mEntry;
};

iduMutexStat* iduMutex::getMutexStat()
{
#if !defined(SMALL_FOOTPRINT) || defined(WRS_VXWORKS)
    return &(mEntry->mStat);
#else
    return NULL;
#endif
}

PDL_thread_mutex_t* iduMutex::getMutexForCond()
{
    return (PDL_thread_mutex_t *)(mEntry->getMutexForCondWait());
}

inline IDE_RC iduMutex::trylock( idBool  & bLock )
{
    mEntry->trylock( &bLock );

    return IDE_SUCCESS;
}

inline IDE_RC iduMutex::lock( idvSQL * aStatSQL )
{
    mEntry->lock( aStatSQL );

    return IDE_SUCCESS;
}

inline IDE_RC iduMutex::unlock()
{
    mEntry->unlock();

    return IDE_SUCCESS;
}

/* --------------------------------------------------------------------
 * mtx commit �ÿ� latch�� Ǯ���ֱ� ���� �Լ�
 * sdbBufferMgr�� i/f�� ���߱� ���� 2���� ���ڰ� �߰��Ǿ�����
 * ������ �ʴ´�.
 * ----------------------------------------------------------------- */
inline IDE_RC iduMutex::unlock( void *aMutex, UInt, void * )
{
    IDE_DASSERT( aMutex != NULL );

    return ( ((iduMutex *)aMutex)->unlock() );
}

/* --------------------------------------------------------------------
 * 2���� latch�� ���� �� ���Ѵ�.
 * sdrMiniTrans���� ���ÿ� �ִ� Ư�� �������� ã�� �� ���ȴ�.
 * �ܼ��� �����Ͱ� ������ ���Ѵ�. -> ���� �������� ã�� ������.
 * ----------------------------------------------------------------- */
inline idBool iduMutex::isMutexSame( void *aLhs, void *aRhs )
{
    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    return ( aLhs == aRhs ? ID_TRUE : ID_FALSE );
}

/* PROJ-2109 : Remove the bottleneck of alloc/free stmts. */
inline void iduMutex::resetStat()
{
    mEntry->resetStat();
}

inline void iduMutex::setName(SChar *aName)
{
    mEntry->setName( aName );
}

#endif	// _O_MUTEX_H_
