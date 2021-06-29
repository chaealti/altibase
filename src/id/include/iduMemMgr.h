/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemMgr.h 85026 2019-03-15 02:16:46Z jayce.park $
 **********************************************************************/

#ifndef _O_IDU_MEM_MGR_H_
#define _O_IDU_MEM_MGR_H_ 1

/**
 * @file
 * @ingroup idu
*/

#include <idl.h>
#include <ide.h>
#include <iduMutex.h>
#include <iduCond.h>
#include <idCore.h>
#include <iduMemDefs.h>

/// �޸� �Ҵ� ��忡 ���� �ɼ� �߰�
/// - IDU_MEM_FORCE : �Ҵ翡 �����ϸ� ������ ����Ͽ�, �ᱹ �Ҵ翡 ����
/// - IDU_MEM_IMMEDIATE : ����/���и� ��� ��ȯ�Ѵ�.
/// �� ���� ���� �־��� �ð���ŭ ����Ͽ� ��õ��� ��, �� �����
/// ��ȯ�ϵ��� �Ѵ�. �ð� ������ u-sec�̸�, ����/���� ��ȯ
#define IDU_MEM_FORCE     (-1)
#define IDU_MEM_IMMEDIATE (0)

/// �޸� �Ҵ� �õ��� �����Ͽ��� ���, ��õ��Ҷ������� ���ð�
/// �ð� ������ u-sec�̸�, IDU_MEM_IMMEDIATE�� ��� �ش���� ����
#define IDU_MEM_DEFAULT_RETRY_TIME        (50*1000) // 50 ms

/** �߸��� �޸� �ε��� */
#define IDU_INVALID_CLIENT_INDEX ID_UINT_MAX

// allocator index for special cases
#define IDU_ALLOC_INDEX_PRIVATE 0xdeaddead
#define IDU_ALLOC_INDEX_LIBC 0xbeefbeef    

typedef enum
{
    IDU_MEMMGR_SINGLE,
    IDU_MEMMGR_CLIENT = IDU_MEMMGR_SINGLE,
    IDU_MEMMGR_LIBC,
    IDU_MEMMGR_TLSF,
    IDU_MEMMGR_INNOCENSE, /*BUG-46568*/
    IDU_MEMMGR_MAX
} iduMemMgrType;

/*
 * Project 2408
 * initializeStatic and destroyStatic for each memmgr type
 */
typedef IDE_RC (*iduMemInitializeStaticFunc)(void);
typedef IDE_RC (*iduMemDestroyStaticFunc)(void);
/*
 * malloc(index, size, pointer)
 * malign(index, size, align, pointer)
 * calloc(index, size, count, pointer)
 * realloc(index, newsize, pointer)
 * free(pointer)
 * shrink(void) - shrinks the memory area of current process
 * buildft(header, dumpobj, table memory)
 *  - export information of all memory blocks
 *  - not usable in single or libc
 */
typedef IDE_RC (*iduMemMallocFunc)(iduMemoryClientIndex, ULong, void **);
typedef IDE_RC (*iduMemMAlignFunc)(iduMemoryClientIndex, ULong, ULong, void **);
typedef IDE_RC (*iduMemCallocFunc)(iduMemoryClientIndex, vSLong, ULong, void **);
typedef IDE_RC (*iduMemReallocFunc)(iduMemoryClientIndex, ULong, void **);
typedef IDE_RC (*iduMemFreeFunc)(void *);
typedef IDE_RC (*iduMemFree4MAlignFunc)(void *, iduMemoryClientIndex aIndex, ULong aSize);

typedef IDE_RC (*iduMemShrinkFunc)(void);

// malloc, calloc, free �Լ� �����͸� �����ϴ� �ڷᱸ��
typedef struct iduMemFuncType
{
    iduMemInitializeStaticFunc  mInitializeStaticFunc;
    iduMemDestroyStaticFunc     mDestroyStaticFunc;
    iduMemMallocFunc            mMallocFunc;
    iduMemMAlignFunc            mMAlignFunc;
    iduMemCallocFunc            mCallocFunc;
    iduMemReallocFunc           mReallocFunc;
    iduMemFreeFunc              mFreeFunc;
    iduMemFree4MAlignFunc       mFree4MAlignFunc;
    iduMemShrinkFunc            mShrinkFunc;
} iduMemFuncType;

struct idvSQL;

/*
 * TLSF allocator with mmap
 * on expand, shrink, or alloc/free large block,
 * this class will use mmap system call
 */
class iduMemTlsfMmap : public iduMemTlsf
{
public:
    IDE_RC  initialize(SChar*, idBool, ULong);

    virtual IDE_RC allocChunk(iduMemTlsfChunk**);
    virtual IDE_RC freeChunk(iduMemTlsfChunk*);

    virtual IDE_RC mallocLarge(ULong, void**);
    virtual IDE_RC freeLarge(void*, ULong);

    virtual ~iduMemTlsfMmap() {};
};

/*
 * TLSF local allocator
 * on expand, shrink, or alloc/free large block,
 * this class will get the memory block from global TLSF instance
 */
class iduMemTlsfLocal : public iduMemTlsf
{
public:
    IDE_RC  initialize(SChar*, idBool, ULong);

    virtual IDE_RC malloc(iduMemoryClientIndex, ULong, void**);
    virtual IDE_RC malign(iduMemoryClientIndex, ULong, ULong, void**);

    virtual IDE_RC allocChunk(iduMemTlsfChunk**);
    virtual IDE_RC freeChunk(iduMemTlsfChunk*);

    virtual IDE_RC mallocLarge(ULong, void**) {return IDE_FAILURE;}
    virtual IDE_RC freeLarge(void*, ULong)    {return IDE_FAILURE;}

    virtual ~iduMemTlsfLocal() {};
};

/*
 * TLSF global allocator
 * on expand, shrink, or alloc/free large block,
 * this class will allocate chunk only once.
 */
class iduMemTlsfGlobal : public iduMemTlsf
{
public:
    IDE_RC  initialize(SChar*, ULong);
    virtual IDE_RC destroy(void);

    virtual IDE_RC allocChunk(iduMemTlsfChunk**){return IDE_FAILURE;}
    virtual IDE_RC freeChunk(iduMemTlsfChunk*)  {return IDE_FAILURE;}

    virtual IDE_RC mallocLarge(ULong, void**) {return IDE_FAILURE;}
    virtual IDE_RC freeLarge(void*, ULong)    {return IDE_FAILURE;}

    virtual ~iduMemTlsfGlobal() {};
};

///
/// @class iduMemMgr
/// Memory allocators: malloc, calloc, realloc and free.
/// initializeStatic() should be called first to initialize memory allocators.
///
class iduMemMgr
{
public:
    static iduMemClientInfo mClientInfo[IDU_MEM_UPPERLIMIT];

    static idBool               mAutoShrink;
    static PDL_thread_mutex_t   mAllocListLock;
    static iduMemAllocCore      mAllocList;
    static SInt                 mSpinCount;

    static SInt getSpinCount(void) {return mSpinCount;}

    /// check allocator is set as server type
    /// @return ID_TRUE or ID_FALSE
    static idBool isServerMemType(void)
    {
       idBool sRet;

       if( mMemType !=  IDU_MEMMGR_CLIENT )
       {
            sRet = ID_TRUE;
       }
       else
       {
            sRet = ID_FALSE;
       }
       
       return sRet;
    }

    /* Statistics update function */
    static void server_statupdate(iduMemoryClientIndex  aIndex,
                                  SLong                 aSize,
                                  SLong                 aCount);
    static void single_statupdate(iduMemoryClientIndex  aIndex,
                                  SLong                 aSize,
                                  SLong                 aCount);

    static inline idBool isPrivateAvailable(void)
    {
        idBool sRet;

        if(isServerMemType() == ID_TRUE)
        {
            if(mMemType == IDU_MEMMGR_TLSF)
            {
                sRet = ID_FALSE;
            }
            else
            {
                sRet = (mUsePrivateAlloc != 0)? ID_TRUE:ID_FALSE;
            }
        }
        else
        {
            sRet = ID_FALSE;
        }

        return sRet;
    }

    static inline idBool isBlockTracable(void)
    {
        idBool sRet;

        if(isServerMemType() == ID_TRUE)
        {
            if(mMemType == IDU_MEMMGR_TLSF)
            {
                sRet = ID_TRUE;
            }
            else
            {
                sRet = isPrivateAvailable();
            }
        }
        else
        {
            sRet = ID_FALSE;
        }

        return sRet;
    }

    static inline idBool isUseResourcePool(void)
    {
        idBool sRet;

        if(isServerMemType() == ID_TRUE)
        {
            if(mUsePrivateAlloc != 0)
            {
                sRet = ID_TRUE;
            }
            else
            {
                sRet = ID_FALSE;
            }
        }
        else
        {
            sRet = ID_FALSE;
        }

        return sRet;

    }

private:
    static iduMemMgrType    mMemType;
    static iduMemFuncType   mMemFunc[IDU_MEMMGR_MAX];

    // Allocation ���н�, ��õ��� ������ �������� ���ð�(u-sec)
    static SLong            mAllocRetryTime;

    // �������� ����� ���� �޸� �Ҵ� ��û�� ������ �ݽ����� ����
    static UInt             mLogLevel;
    static ULong            mLogLowerSize;
    static ULong            mLogUpperSize;

    // BUG-20129
    static ULong            mHeapMemMaxSize;

    static UInt             mNoAllocators;
    static idBool           mIsCPU;

    static UInt             mUsePrivateAlloc;
    static ULong            mPrivatePoolSize;

    static UInt             mAllocatorIndex;

public:
    /// initialize class
    /// @param aType execution of application,
    /// @see iduMemMgrType
    static IDE_RC initializeStatic(iduPeerType aType);

    /// destruct class
    static IDE_RC destroyStatic();

    /// get the amount of allocated memory
    /// @return memory size
    static ULong  getTotalMemory();

    /// ��� �޸𸮸� ������ �Ŀ��� ������ �ȵ� �޸𸮰� �ִ��� Ȯ���Ѵ�.
    /// ���� ������ ���� ���� �޸𸮰� �����Ѵٸ� �޸� ���� �߻��� ���̴�.
    /// ���� �����Ѵٸ� �α׿� � Ŭ���̾�Ʈ �ε����� ��� ũ����
    /// �޸𸮰� �������� �ʾҴ��� �޽����� ����Ѵ�.
    static IDE_RC logMemoryLeak();

    /// �üü�κ��� ���� �޸𸮸� �Ҵ� �� ����������,
    /// time out ����� �ʿ�� �Ҷ� ����Ѵ�.
    /// @param aSize memory size
    /// @param aTimeOut amount of wait time
    static void * mallocRaw(ULong aSize,
                            SLong aTimeOut = IDU_MEM_IMMEDIATE);
    /// �ü���� calloc�� ȣ����
    static void * callocRaw(vSLong aCount,
                            ULong aSize,
                            SLong aTimeOut = IDU_MEM_IMMEDIATE);
    /// �ü���� realloc�� ȣ����
    static void * reallocRaw(void *aMemPtr,
                             ULong aSize,
                             SLong aTimeOut = IDU_MEM_IMMEDIATE);
    /// �ü���� free�� ȣ����
    /// mallocRaw(), callocRaw(), reallocRaw()�� �Ҵ���� �޸𸮴� �ݵ��
    /// freeRaw�� �����ؾ���
    static void   freeRaw(void*  aMemPtr);

    // iduMemMgr�� �޸� �Ҵ� ���� �Լ���

    /// malloc�� ������ ������ �ϸ� ��������� �����Ѵ�.
    /// ������ ���� Ŭ������ �ʱ�ȭ Ÿ�Կ� ���� ��� ���� ���ſ� ���̰� �ִ�.
    /// - IDU_MEMMGR_CLIENT: ��� ���� ������ ���� �ʴ´�.
    /// - IDU_MEMMGR_SERVER: �������� �����尡 ���ÿ� ����� �� �ֵ��� ����ȭ�� ��� ���� ������ �Ѵ�.
    /// @param aIndex �޸𸮸� ��û�ϴ� ����� �ε���
    /// @param aSize �޸� ũ��
    /// @param aMemPtr �Ҵ�� �޸��� �����͸� ������ ������ ������
    /// @param aTimeOut ��� �ð�
    /// @param aAlloc �޸� ������
    /// @see iduMemMgrType
    static IDE_RC malloc(iduMemoryClientIndex   aIndex,
                         ULong                  aSize,
                         void                 **aMemPtr,
                         SLong                  aTimeOut = IDU_MEM_IMMEDIATE);

    /** malloc�� ������ ������ �ϵ�
     * aAlign�� ��ġ�� ���� �޸� �����͸� �����Ѵ�.
     * @param aIndex �޸𸮸� ��û�ϴ� ����� �ε���
     * @param aAlign �޸� ���� ����. 2�� �ŵ�������(=2^n)�̰� sizeof(void*)�� ������� ��.
     * @param aSize �޸� ũ��
     * @param aMemPtr �Ҵ�� �޸��� �����͸� ������ ������ ������
     * @param aTimeOut ��� �ð�
     * @param aAlloc �޸� ������
     * @see iduMemMgrType
     */
    static IDE_RC malign(iduMemoryClientIndex   aIndex,
                         ULong                  aSize,
                         ULong                  aAlign,
                         void                 **aMemPtr,
                         SLong                  aTimeOut = IDU_MEM_IMMEDIATE);

    /// calloc�� ����
    /// @see malloc()
    static IDE_RC calloc(iduMemoryClientIndex   aIndex,
                         vSLong                 aCount,
                         ULong                  aSize,
                         void                 **aMemPtr,
                         SLong                  aTimeOut = IDU_MEM_IMMEDIATE);

    /// realloc�� ����
    /// @see malloc()
    static IDE_RC realloc(iduMemoryClientIndex  aIndex,
                          ULong                 aSize,
                          void                **aMemPtr,
                          SLong                 aTimeOut = IDU_MEM_IMMEDIATE);

    /// free�� ����
    /// malloc(), calloc(), realloc()���� �Ҵ���� �޸𸮴� �ݵ�� free()�� �����Ǿ� �Ѵ�.
    /// @param aMemPtr ������ �޸��� ������
    /// @param aAlloc �޸� ������
    static IDE_RC free(void                 * aMemPtr);

    //BUG-46165
    //this function must be called by the address getting from malign()   not malloc()
    static IDE_RC free4malign(void                  * aMemPtr,
                              iduMemoryClientIndex    aIndex,
                              ULong                   aSize);

    /* Memory management functions with specific allocator */
    static IDE_RC malloc(iduMemoryClientIndex   aIndex,
                         ULong                  aSize,
                         void**                 aMemPtr,
                         SLong                  aTimeOut,
                         iduMemAllocator*       aAlloc);
    static IDE_RC malign(iduMemoryClientIndex   aIndex,
                         ULong                  aSize,
                         ULong                  aAlign,
                         void**                 aMemPtr,
                         SLong                  aTimeOut,
                         iduMemAllocator*       aAlloc);
    static IDE_RC calloc(iduMemoryClientIndex   aIndex,
                         vSLong                 aCount,
                         ULong                  aSize,
                         void**                 aMemPtr,
                         SLong                  aTimeOut,
                         iduMemAllocator*       aAlloc);
    static IDE_RC realloc(iduMemoryClientIndex  aIndex,
                          ULong                 aSize,
                         void**                 aMemPtr,
                         SLong                  aTimeOut,
                         iduMemAllocator*       aAlloc);
    static IDE_RC free(void*                    aMemPtr,
                       iduMemAllocator*         aAlloc);

    /* Project 2408 */
    static iduMemAlloc* getGlobalAlloc(void) {return mTLSFGlobal;}
    static IDE_RC getSmallAlloc(iduMemSmall**);
    static IDE_RC destroySmallAlloc(iduMemSmall*);

    static IDE_RC getTlsfAlloc(iduMemTlsf**);
    static IDE_RC dumpAllMemory(SChar*, SInt);


    ///
    static IDE_RC createAllocator(iduMemAllocator *,
                                  idCoreAclMemAllocType = ACL_MEM_ALLOC_TLSF,
                                  void* = NULL); // common for all kinds of allocator

    static IDE_RC freeAllocator(iduMemAllocator *aAlloc,
                                idBool aCheckEmpty = ID_TRUE);

    static IDE_RC shrinkAllocator(iduMemAllocator *aAlloc);
    static IDE_RC shrinkAllAllocators(void); // fix BUG-37960

    // BUG-39198
    static UInt   getAllocatorType( void )
    {
        return (UInt)mMemType;
    }

private:
    /*
     * IDU_MEMMGR_SINGLE = IDU_MEMMGR_CLIENT
     * initializeStatic ������ single-threaded server ��Ȳ,
     * Ȥ�� Ŭ���̾�Ʈ���� ���Ǵ� �Լ���
     * mutex lock�� ���� �ʰ� ��⺰ ��� ������ ������� �ʴ´�.
     */
    static IDE_RC single_initializeStatic(void);
    static IDE_RC single_destroyStatic(void);
    static IDE_RC single_malloc(iduMemoryClientIndex   aIndex,
                                ULong                  aSize,
                                void                 **aMemPtr);
    static IDE_RC single_malign(iduMemoryClientIndex   aIndex,
                                ULong                  aSize,
                                ULong                  aAlign,
                                void                 **aMemPtr);
    static IDE_RC single_calloc(iduMemoryClientIndex   aIndex,
                                vSLong                 aCount,
                                ULong                  aSize,
                                void                 **aMemPtr);
    static IDE_RC single_realloc(iduMemoryClientIndex  aIndex,
                                 ULong                 aSize,
                                 void                **aMemPtr);
    static IDE_RC single_free(void                   *aMemPtr);
    static IDE_RC single_free4malign(void                   *aMemPtr,
                                     iduMemoryClientIndex    aIndex,
                                     ULong                   aSize);
    static IDE_RC single_shrink(void);


private:
    /*
     * IDU_MEMMGR_LIBC
     * multi-threaded server���� ���Ǵ� �Լ���
     * malloc�� ���� ȣ���ϰ� ��⺰ ��� ������ ����Ѵ�.
     * �޸� �� �������̺��� ��ȸ�� �� ����.
     */
    static IDE_RC libc_initializeStatic(void);
    static IDE_RC libc_destroyStatic(void);
    static IDE_RC libc_malloc(iduMemoryClientIndex   aIndex,
                              ULong                  aSize,
                              void                 **aMemPtr);
    static IDE_RC libc_malign(iduMemoryClientIndex   aIndex,
                              ULong                  aSize,
                              ULong                  aAlign,
                              void                 **aMemPtr);
    static IDE_RC libc_calloc(iduMemoryClientIndex   aIndex,
                              vSLong                 aCount,
                              ULong                  aSize,
                              void                 **aMemPtr);
    static IDE_RC libc_realloc(iduMemoryClientIndex  aIndex,
                               ULong                 aSize,
                               void                **aMemPtr);
    static IDE_RC libc_free(void                   *aMemPtr);
    static IDE_RC libc_free4malign(void                   *aMemPtr,
                                   iduMemoryClientIndex    aIndex,
                                   ULong                   aSize);

    static IDE_RC libc_shrink(void);
    static void   libc_getHeader(void* aMemPtr,
                                 void** aHeader1,
                                 void** aHeader2);

private:
    /*
     * IDU_MEMMGR_TLSF
     * multi-threaded server���� ���Ǵ� �Լ���
     * TLSF �޸� �����ڸ� ������ ����ϰ� ��⺰ ��� ������ ����Ѵ�.
     * �޸� �� �������̺��� ��ȸ�� �� �ִ�.
     */
    static IDE_RC tlsf_initializeStatic(void);
    static IDE_RC tlsf_destroyStatic(void);
    static IDE_RC tlsf_malloc(iduMemoryClientIndex   aIndex,
                              ULong                  aSize,
                              void                 **aMemPtr);
    static IDE_RC tlsf_malign(iduMemoryClientIndex   aIndex,
                              ULong                  aSize,
                              ULong                  aAlign,
                              void                 **aMemPtr);
    static IDE_RC tlsf_calloc(iduMemoryClientIndex   aIndex,
                              vSLong                 aCount,
                              ULong                  aSize,
                              void                 **aMemPtr);
    static IDE_RC tlsf_realloc(iduMemoryClientIndex  aIndex,
                               ULong                 aSize,
                               void                **aMemPtr);
    static IDE_RC tlsf_free(void *aMemPtr);

    static IDE_RC tlsf_free4malign(void *aMemPtr,
                                   iduMemoryClientIndex aIndex,
                                   ULong                aSize);
    static IDE_RC tlsf_shrink(void);
    static void   tlsf_getHeader(void* aMemPtr,
                                 void** aHeader1,
                                 void** aHeader2,
                                 void** aTailer);

    /* TLSF Allocators */
    static iduMemTlsfGlobal*    mTLSFGlobal;
    static ULong                mTLSFGlobalChunkSize;

    static iduMemTlsf**         mTLSFLocal;
    static ULong                mTLSFLocalChunkSize;
    static SInt                 mTLSFLocalInstances;

private:
    /*
     * IDU_MEMMGR_INNOCENSE
     */
    static IDE_RC innocense_initializeStatic(void);
    static IDE_RC innocense_destroyStatic(void);
    static IDE_RC innocense_malloc(iduMemoryClientIndex   aIndex,
                                   ULong                  aSize,
                                   void                 **aMemPtr);
    static IDE_RC innocense_malign(iduMemoryClientIndex   aIndex,
                                   ULong                  aSize,
                                   ULong                  aAlign,
                                   void                 **aMemPtr);
    static IDE_RC innocense_calloc(iduMemoryClientIndex   aIndex,
                                   vSLong                 aCount,
                                   ULong                  aSize,
                                   void                 **aMemPtr);
    static IDE_RC innocense_realloc(iduMemoryClientIndex  aIndex,
                                    ULong                 aSize,
                                    void                **aMemPtr);
    static IDE_RC innocense_free(void                   *aMemPtr);
    static IDE_RC innocense_free4malign(void                   *aMemPtr,
                                        iduMemoryClientIndex    aIndex,
                                        ULong                   aSize);

    static IDE_RC innocense_shrink(void);
    static void   innocense_getHeader(void* aMemPtr,
                                      void** aHeader1,
                                      void** aHeader2,
                                      void** aTailer);

    /*
     * BUG-32751
     * Distributing memory allocation into several allocators
     * so that the locks can be dispersed
     */
    static idCoreAclMemAlloc *getAllocator(void **aMemPtr,
                                           UInt aType,
                                           iduMemAllocator *aAlloc);

    static void   logAllocRequest(iduMemoryClientIndex aIndex, ULong aSize, SLong aTimeOut);

    static inline IDE_RC callbackLogLevel(idvSQL*, SChar*, void*, void*, void*);
    static inline IDE_RC callbackLogLowerSize(idvSQL*, SChar*, void*, void*, void*);
    static inline IDE_RC callbackLogUpperSize(idvSQL*, SChar*, void*, void*, void*);

    static UInt getAllocIndex(void);
};

#endif  // _O_IDU_MEM_MGR_H_
