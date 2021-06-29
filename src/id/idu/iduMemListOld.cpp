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
 
/***********************************************************************
 * $Id: iduMemListOld.cpp 40979 2010-08-10 04:02:04Z orc $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <ideLog.h>
#include <iduRunTimeInfo.h>
#include <iduMemListOld.h>
#include <iduMemPoolMgr.h>

extern const vULong gFence;



void iduCheckMemConsistency(iduMemListOld * aMemList)
{
    vULong sCnt = 0;
    iduMemSlotQP *sSlot;

    //check free chunks
    iduMemChunkQP *sCur = aMemList->mFreeChunk.mNext;
    while(sCur != NULL)
    {
        sSlot = (iduMemSlotQP *)sCur->mTop;
        while(sSlot != NULL)
        {
            sSlot = sSlot->mNext;
        }

        IDE_ASSERT( sCur->mMaxSlotCnt  == sCur->mFreeSlotCnt );

        sCur = sCur->mNext;
        sCnt++;
        IDE_ASSERT(sCnt <= aMemList->mFreeChunkCnt);
    }

    IDE_ASSERT(sCnt == aMemList->mFreeChunkCnt);

    //check full chunks
    sCnt = 0;
    sCur = aMemList->mFullChunk.mNext;
    while(sCur != NULL)
    {
        IDE_ASSERT( sCur->mFreeSlotCnt == 0 );
        sCur = sCur->mNext;
        sCnt++;
    }

    IDE_ASSERT(sCnt == aMemList->mFullChunkCnt);

    //check partial chunks
    sCnt = 0;
    sCur = aMemList->mPartialChunk.mNext;
    while(sCur != NULL)
    {
        IDE_ASSERT( sCur->mMaxSlotCnt > 1 );

        //sCur->mMaxSlotCnt > 1
        IDE_ASSERT( (0  < sCur->mFreeSlotCnt) &&  
                    (sCur->mFreeSlotCnt < sCur->mMaxSlotCnt) );

        sCur = sCur->mNext;
        sCnt++;
    }

    IDE_ASSERT(sCnt == aMemList->mPartialChunkCnt);
}


//#ifndef MEMORY_ASSERT
#define iduCheckMemConsistency( xx )
//#endif

iduMemListOld::iduMemListOld()
{
}

iduMemListOld::~iduMemListOld(void)
{
}

/*---------------------------------------------------------
  iduMemListOld

   ___________ mFullChunk
  |iduMemListOld |    _____      _____
  |___________|-> |chunk| -> |chunk| ->NULL
        |  NULL<- |_____| <- |_____|
        |
        |
        |     mPartialChunk
        |          _____      _____
        |`------->|chunk| -> |chunk| ->NULL
        |   NULL<- |_____| <- |_____|
        |
        | 
        |     mFreeChunk
        |          _____      _____
         `------->|chunk| -> |chunk| ->NULL
           NULL<- |_____| <- |_____|


  mFreeChunk    : ��� slot�� free�� chunk
  mPartialChunk : �Ϻ��� slot�� ������� chunk
  mFullChunk    : ��� slot�� ������� chunk

  *ó�� chunk�� �Ҵ������ mFreeChunk�� �Ŵ޸��Եǰ�
   slot�Ҵ��� �̷����� mPartialChunk�� ���ļ� mFullchunk�� �̵���.
  *mFullChunk�� �ִ� chunk�� slot�ݳ��� mPartialchunk�� ���ļ� mFreeChunk�� �̵�.
  *mFreeChunk�� �ִ� chunk���� �޸𸮰� ������ �Ѱ��Ȳ������ OS�� �ݳ��Ǿ����� ����.
  
  chunk:
   _________________________________
  |iduMemChunkQP |Slot|Slot|... | Slot|
  |____________|____|____|____|_____|

  iduMemChunkQP        : chunk ���(���� chunk�� ���� ������ ��� �ִ�.)
  
  
  
  Slot:
   ___________________________________
  |momory element |iduMemChunkQP pointer|
  |_______________|___________________|

  memory element      : ����ڰ� ���� �޸𸮸� ����ϴ� ����
             �� ������ ����ڿ��� �Ҵ�Ǿ� ���� ��������, ����
             (free)slot������ ����Ű�� ������ ��Ȱ�� �Ѵ�.
  iduMemChunkQP pointer : slot�� ���� chunk�� ���(chunk�� iduMemChunkQP����)��
                        ������ �ϰ� �ִ�.
  -----------------------------------------------------------*/
/*-----------------------------------------------------------
 * task-2440 iduMemPool���� �޸𸮸� ���ö� align�� �޸� �ּҸ� ���
 *          �� �� �ְ� ����.
 *          
 * chunk������ align�� �Ͼ�� ���� �����ϰڴ�.
 * ___________________________________________
  |iduMemChunkQP |   |  Slot|  Slot|... |   Slot|
  |____________|___|______|______|____|_______|
  ^            ^   ^      ^
  1            2   3      4
  
  �ϴ� chunk�� �Ҵ��ϸ� 1���� ������ �ּҰ����� �����ȴ�.  �� �ּҰ���
  ����ڰ� align�Ǳ⸦ ���ϴ� �ּҰ� �ƴϱ� ������ �׳� ���д�.
  2���ּ� = 1�� �ּ� + SIZEOF(iduMemChunkQP)
  2�� �ּҴ� ���� ���� ��������, 2���ּҸ� align up��Ű�� 3��ó�� �� ������
  ����Ű�� �ȴ�. �׷��� 3���� align�� �Ǿ��ְ� �ȴ�.
  ������ ����ڿ��� �Ҵ� �Ǵ� ������ 3�� ���� �����̴�.
  4���� align�Ǿ� �ֱ� ���ؼ��� slotSize�� align �Ǿ� �־�� �Ѵ�.
  �׷��� ������ SlotSize�� ���� size���� align up�� ���� ����Ѵ�.
  �׷��� ��� slot�� �ּҴ� align�Ǿ������� �� ���ִ�.

  ���� 2���� 3������ ������ �ִ� ũ��� alignByte�̴�.
  �׷��� ������
  chunkSize = SIZEOF(iduMemChunkQP)+ alignByte +
              ('alignByte�� align up�� SIZEOF(slot)' * 'slot�� ����')
  �̴�.
 * ---------------------------------------------------------*/
/*-----------------------------------------------------------
 * Description: iduMemListOld�� �ʱ�ȭ �Ѵ�.
 *
 * aIndex            - [IN] �� �Լ��� ȣ���� ���
 *                        (��� ȣ���ϴ��� ������ �����ϱ� ���� �ʿ�)
 * aSeqNumber        - [IN] iduMemListOld �ĺ� ��ȣ
 * aName             - [IN] iduMemListOld�� ȣ���� iduMemPool�� �̸�.
 * aElemSize         - [IN] �ʿ�� �ϴ� �޸� ũ��( �Ҵ���� �޸��� ����ũ��)
 * aElemCnt        - [IN] �� chunk���� element�� ����, ��, chunk����
 *                        slot�� ����
 * aAutofreeChunkLimit-[IN] mFreeChunk�� ������ �� ���ں��� ū ��쿡
 *                          mFreeChunk�߿��� ���� �ٸ� ������ ������ �ʴ�
 *                          chunk�� �޸𸮿��� �����Ѵ�.
 * aUseMutex         - [IN] mutex��� ����                          
 * aAlignByte        - [IN] aline�Ǿ� �ִ� �޸𸮸� �Ҵ�ް� ������, ���ϴ� align
 *                          ���� �־ �Լ��� ȣ���Ѵ�. 
 * ---------------------------------------------------------*/
IDE_RC iduMemListOld::initialize(iduMemoryClientIndex aIndex,
                              UInt   aSeqNumber,
                              SChar* aName,
                              vULong aElemSize,
                              vULong aElemCnt,
                              vULong aAutofreeChunkLimit,
                              idBool aUseMutex,
                              UInt   aAlignByte)
{
    SChar sBuffer[IDU_MUTEX_NAME_LEN*2];//128 bytes

    IDE_ASSERT( aName != NULL );
    //IDE_ASSERT( aElemCnt > 1 ); // then... why do you make THIS mem pool??

    mIndex     = aIndex;
    mElemCnt   = aElemCnt;
    mUseMutex  = aUseMutex;

    IDE_ASSERT(aElemSize <= ID_UINT_MAX);

    /*BUG-19455 mElemSize�� align �Ǿ�� �� */
#if !defined(COMPILE_64BIT)
    mElemSize   = idlOS::align((UInt)aElemSize, sizeof(SDouble));
#else /* !defined(COMPILE_64BIT) */
    mElemSize   = idlOS::align((UInt)aElemSize);
#endif /* !defined(COMPILE_64BIT) */    
    
#ifdef MEMORY_ASSERT
    mSlotSize = (vULong)idlOS::align( (UInt)( mElemSize +
                                              ID_SIZEOF(gFence)       +
                                              ID_SIZEOF(iduMemSlotQP *) +
                                              ID_SIZEOF(gFence)), aAlignByte );
#else
    mSlotSize   = idlOS::align(mElemSize + ID_SIZEOF(iduMemSlotQP *), aAlignByte);
#endif
    
    mChunkSize            = ID_SIZEOF(iduMemChunkQP) + (mSlotSize * mElemCnt) + aAlignByte;
    mAlignByte            = aAlignByte;
    
    mFreeChunk.mParent         = this;
    mFreeChunk.mNext           = NULL;
    mFreeChunk.mPrev           = NULL;
    mFreeChunk.mTop            = NULL;
    mFreeChunk.mMaxSlotCnt     = 0;
    mFreeChunk.mFreeSlotCnt    = 0;
                               
    mPartialChunk.mParent      = this;
    mPartialChunk.mNext        = NULL;
    mPartialChunk.mPrev        = NULL;
    mPartialChunk.mTop         = NULL;
    mPartialChunk.mMaxSlotCnt  = 0;
    mPartialChunk.mFreeSlotCnt = 0;

    mFullChunk.mParent         = this;
    mFullChunk.mNext           = NULL;
    mFullChunk.mPrev           = NULL;
    mFullChunk.mTop            = NULL;
    mFullChunk.mMaxSlotCnt     = 0;
    mFullChunk.mFreeSlotCnt    = 0;

    mFreeChunkCnt              = 0;
    mPartialChunkCnt           = 0;
    mFullChunkCnt              = 0;

    mAutoFreeChunkLimit    = aAutofreeChunkLimit;

    IDE_TEST(grow() != IDE_SUCCESS);

    if (mUseMutex == ID_TRUE)
    {
        if (aName == NULL)
        {
            aName = (SChar *)"noname";
        }

        idlOS::memset(sBuffer, 0, ID_SIZEOF(sBuffer));
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer), 
                         "%s"IDU_MEM_POOL_MUTEX_POSTFIX"%"ID_UINT32_FMT, aName, aSeqNumber);

        IDE_TEST(mMutex.initialize(sBuffer,
                                   IDU_MUTEX_KIND_NATIVE,
                                   IDV_WAIT_INDEX_NULL) != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/* ---------------------------------------------------------------------------
 * BUG-19253
 * iduMemListOld�� mFreeChunk�� mFullChunk�� �ΰ��� chunk list�� �����Ѵ�.
 * iduMemPool�� ����ϴ� ��⿡�� iduMemPool�� destroy�ϱ� ����
 * ��� iduMemPool�� slot�� memfree���� ���� �������� destroy�� ����ȴ�.
 * ����, �ϳ��� slot�̶� free���� ���� ä destroy�� ��û�Ǹ� debug mode������
 * DASSERT()�� �װ�, release mode������ mFullChunk�� ����� chunk��ŭ��
 * memory leak�� �߻��ϰ� �ȴ�.
 * debug mode������ memory leak�� �����ϱ� ���� DASSERT���� �����ϰ�,
 * release mode������ mFullChunk�� ����� chunk�鵵 free ��Ű���� �Ѵ�.
 *
 * index build�� ���� ����� iduMemPool�� �Ϸ� �� �Ѳ����� free ��Ű�� ���
 * mFullChunk�� �߰������� free ��Ű���� �Ѵ�. �Ѳ����� free ��Ű�� ����
 * aBCheck�� ID_FALSE�� �����Ǿ�� �Ѵ�.
 * ---------------------------------------------------------------------------*/
IDE_RC iduMemListOld::destroy(idBool aBCheck)
{
    vULong i;
    iduMemChunkQP *sCur = NULL;

    for (i = 0; i < mFreeChunkCnt; i++)
    {
        sCur = mFreeChunk.mNext;
        IDE_ASSERT(sCur != NULL);

        if(aBCheck == ID_TRUE)
        {
            if (sCur->mFreeSlotCnt != sCur->mMaxSlotCnt )
            {
                ideLog::log(IDE_SERVER_0,ID_TRC_MEMLIST_MISMATCH_FREE_SLOT_COUNT,
                            (UInt)sCur->mFreeSlotCnt,
                            (UInt)sCur->mMaxSlotCnt);

                IDE_DASSERT(0);
            }
        }

        unlink(sCur);

        IDE_TEST(iduMemMgr::free(sCur) != IDE_SUCCESS);
    }

    if(aBCheck == ID_TRUE)
    {
        if( mFullChunkCnt != 0)
        {
            ideLog::log(IDE_SERVER_0,ID_TRC_MEMLIST_WRONG_FULL_CHUNK_COUNT,
                        (UInt)mFullChunkCnt);

            IDE_DASSERT(0);
        }
    }

    for (i = 0; i < mPartialChunkCnt; i++)
    {
        sCur = mPartialChunk.mNext;
        IDE_ASSERT(sCur != NULL);

        if(aBCheck == ID_TRUE)
        {
            if ( (sCur->mFreeSlotCnt == 0) || 
                 (sCur->mFreeSlotCnt == sCur->mMaxSlotCnt) )
            {
                ideLog::log(IDE_SERVER_0, ID_TRC_MEMLIST_WRONG_FREE_SLOT_COUNT,
                            (UInt)sCur->mFreeSlotCnt,
                            (UInt)sCur->mMaxSlotCnt);
                IDE_DASSERT(0);
            }
        }

        unlink(sCur);
        IDE_TEST(iduMemMgr::free(sCur) != IDE_SUCCESS);
    }

    // BUG-19253
    // mFullChunk list�� ����� chunk�� ������ free��Ų��.
    for (i = 0; i < mFullChunkCnt; i++)
    {
        sCur = mFullChunk.mNext;
        IDE_ASSERT(sCur != NULL);

        unlink(sCur);
        IDE_TEST(iduMemMgr::free(sCur) != IDE_SUCCESS);
    }

    if (mUseMutex == ID_TRUE)
    {
        IDE_TEST(mMutex.destroy() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

inline void iduMemListOld::unlink(iduMemChunkQP *aChunk)
{
    iduMemChunkQP *sBefore = aChunk->mPrev;
    iduMemChunkQP *sAfter  = aChunk->mNext;

    sBefore->mNext = sAfter;
    if (sAfter != NULL)
    {
        sAfter->mPrev = sBefore;
    }
}

inline void iduMemListOld::link(iduMemChunkQP *aBefore, iduMemChunkQP *aChunk)
{
    iduMemChunkQP *sAfter   = aBefore->mNext;

    aBefore->mNext        = aChunk;
    aChunk->mPrev         = aBefore;
    aChunk->mNext         = sAfter;

    if (sAfter != NULL)
    {
        sAfter->mPrev = aChunk;
    }
}

/*
 *iduMemChunkQP�� �ϳ� �Ҵ��Ͽ�, �̰��� iduMemListOld->mFreeChunk�� �����Ѵ�.
 */
IDE_RC iduMemListOld::grow(void)
{

    vULong       i;
    iduMemChunkQP *sChunk;
    iduMemSlotQP  *sSlot;
    iduMemSlotQP  *sFirstSlot;

    IDE_TEST(iduMemMgr::malloc(mIndex,
                               mChunkSize,
                               (void**)&sChunk,
                               IDU_MEM_FORCE)
             != IDE_SUCCESS);

    sChunk->mParent    = this;
    sChunk->mMaxSlotCnt  = mElemCnt;
    sChunk->mFreeSlotCnt = mElemCnt;
    sChunk->mTop       = NULL;

    /*chunk���� �� slot�� mNext�� �����Ѵ�. ���� �������� �ִ� slot�� sChunk->mTop�� ����ȴ�.
     * mNext�� ����ڿ��� �Ҵ�ɶ��� ���� ����ϴ� �������� ���δ�.*/
    sFirstSlot = (iduMemSlotQP*)((UChar*)sChunk + ID_SIZEOF( iduMemChunkQP));
    sFirstSlot = (iduMemSlotQP*)idlOS::align(sFirstSlot, mAlignByte);
    for (i = 0; i < mElemCnt; i++)
    {
        sSlot        = (iduMemSlotQP *)((UChar *)sFirstSlot + (i * mSlotSize));
        sSlot->mNext = sChunk->mTop;
        sChunk->mTop = sSlot;
#ifdef MEMORY_ASSERT
        *((vULong *)((UChar *)sSlot + mElemSize)) = gFence;
        *((iduMemChunkQP **)((UChar *)sSlot +
                           mElemSize +
                           ID_SIZEOF(gFence))) = sChunk;
        *((vULong *)((UChar *)sSlot +
                     mElemSize +
                     ID_SIZEOF(gFence)  +
                     ID_SIZEOF(iduMemChunkQP *))) = gFence;
#else
        *((iduMemChunkQP **)((UChar *)sSlot + mElemSize)) = sChunk;
#endif
    }

    iduCheckMemConsistency(this);

    /* ------------------------------------------------
     *  mFreeChunk�� ����
     * ----------------------------------------------*/
    link(&mFreeChunk, sChunk);
    mFreeChunkCnt++;

    iduCheckMemConsistency(this);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

/* ------------------------------------------------
 *  Allocation
 * ----------------------------------------------*/
IDE_RC iduMemListOld::alloc(void **aMem)
{

    iduMemChunkQP *sMyChunk=NULL;
    idBool       sIsFreeChunk=ID_TRUE; //��� chunk���� �Ҵ��ϱ�� �����ߴ°�?
                                       // 1: free  chunk   0: partial chunk


#if defined(ALTIBASE_MEMORY_CHECK)
    void  *  sStartPtr;
    UChar ** sTmpPtr;
    
    /*valgrind�� �׽�Ʈ�� ��, ������ ��Ÿ���� ���� �����ϱ� ���ؼ�
     * valgrind���ÿ��� memory�� �Ҵ��ؼ� �����    */
    /* BUG-20590 : align�� ����ؾ� �� - free�� ����� */

//    -------------------------------------------------------
//    |     | Start Pointer | aligned returned memory       |
//    -------------------------------------------------------
//   / \           |
//    |            |
//    -------------+
    
    if( mAlignByte > ID_SIZEOF(void*) )
    {
        *aMem = (void *)idlOS::malloc((mElemSize * 2) + ID_SIZEOF(void*));
        sStartPtr = *aMem;

        // �޸� ���� ��ġ�� ������ �ּ����� ���� ����
        *aMem = (UChar**)*aMem + 1;
        *aMem = (void *)idlOS::align(*aMem, mAlignByte);
        sTmpPtr = (UChar**)*aMem - 1;
        *sTmpPtr = (UChar*)sStartPtr;
    }
    else
    {
        *aMem = (void *)idlOS::malloc(mElemSize + ID_SIZEOF(void*));
        IDE_ASSERT(*aMem != NULL);
        *((void**)*aMem) = *aMem;
        *aMem = (UChar*)((UChar**)*aMem + 1);
    }
    
    IDE_ASSERT(*aMem != NULL);

#else /* normal case : not memory check */

    if (mUseMutex == ID_TRUE)
    {
        IDE_ASSERT(mMutex.lock( NULL ) == IDE_SUCCESS);
    }
    else
    {
        /* No synchronisation */
    }

    iduCheckMemConsistency(this);

    //victim chunk�� ����.
    if (mPartialChunk.mNext != NULL) 
    {
        sMyChunk = mPartialChunk.mNext;
        sIsFreeChunk=ID_FALSE;
    }
    else
    {
        if (mFreeChunk.mNext == NULL) 
        {
            IDE_TEST(grow() != IDE_SUCCESS);
        }
        sMyChunk = mFreeChunk.mNext;
        sIsFreeChunk=ID_TRUE;

        IDE_ASSERT( sMyChunk != NULL );
    }

    iduCheckMemConsistency(this);

    IDE_ASSERT( sMyChunk->mTop != NULL );

    *aMem           = sMyChunk->mTop;

    sMyChunk->mTop = ((iduMemSlotQP *)(*aMem))->mNext;

    /* 
     * Partial list ->Full list, Free list ->Partial list:
                                chunk��  ������ 1���� �ʰ��Ҷ� (�Ϲ����ΰ��)
     * Free list -> Full list  :chunk��  ������ 1�� �ִ� ���.
     */
    if ((--sMyChunk->mFreeSlotCnt) == 0)
    {
        unlink(sMyChunk);
        link(&mFullChunk, sMyChunk);
        if( sIsFreeChunk == ID_TRUE ) //Free list -> Full list
        {
            IDE_ASSERT( mFreeChunkCnt > 0 );
            mFreeChunkCnt--;
        }
        else  //Partial list -> Full list
        {
            IDE_ASSERT( mPartialChunkCnt > 0 );
            mPartialChunkCnt--;
        }
        mFullChunkCnt++;
    }
    else 
    {
        // Free list ->Partial list
        if ( (sMyChunk->mMaxSlotCnt - sMyChunk->mFreeSlotCnt) == 1)
        {
            unlink(sMyChunk);
            link(&mPartialChunk, sMyChunk);
            IDE_ASSERT( mFreeChunkCnt > 0 );
            mFreeChunkCnt--;
            mPartialChunkCnt++;
        }
    }

    iduCheckMemConsistency(this);

    if (mUseMutex == ID_TRUE)
    {
        IDE_ASSERT( mMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* No synchronisation */
    }

#endif

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

#if !defined(ALTIBASE_MEMORY_CHECK)
    if (mUseMutex == ID_TRUE)
    {
        IDE_ASSERT( mMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* No synchronisation */
    }
#endif

    return IDE_FAILURE;
}

/* ------------------------------------------------
 *  Free for one
 * ----------------------------------------------*/

IDE_RC iduMemListOld::memfree(void *aFreeElem)
{
    iduMemSlotQP*  sFreeElem;
    iduMemChunkQP *sCurChunk;

    IDE_ASSERT(aFreeElem != NULL);

#if defined(ALTIBASE_MEMORY_CHECK)
    /*valgrind�� �׽�Ʈ�� ��, ������ ��Ÿ���� ���� �����ϱ� ���ؼ�
     * valgrind���ÿ��� memory�� �Ҵ��ؼ� �����*/
    /* BUG-20590 : align�� ���� �Ҵ����ִ� �ּ� �ٷ� �տ� ����
       �Ҵ���� �޸��� ���� �ּҰ� �������� */
    void * sStartPtr = (void*)*((UChar**)aFreeElem - 1);
    idlOS::free(sStartPtr);

#else /* normal case : not memory check */

    if (mUseMutex == ID_TRUE)
    {
        IDE_ASSERT(mMutex.lock( NULL ) == IDE_SUCCESS);
    }
    else
    {
        /* No synchronisation */
    }

#  ifdef MEMORY_ASSERT

    IDE_ASSERT(*((vULong *)((UChar *)aFreeElem + mElemSize)) == gFence);
    IDE_ASSERT(*((vULong *)((UChar *)aFreeElem +
                        mElemSize +
                        ID_SIZEOF(gFence)  +
                        ID_SIZEOF(iduMemChunkQP *))) == gFence);
    sCurChunk         = *((iduMemChunkQP **)((UChar *)aFreeElem + mElemSize +
                                           ID_SIZEOF(gFence)));
#  else
    sCurChunk         = *((iduMemChunkQP **)((UChar *)aFreeElem + mElemSize));
#  endif

    IDE_ASSERT((iduMemListOld*)this == sCurChunk->mParent);

    sFreeElem        = (iduMemSlotQP*)aFreeElem;
    sFreeElem->mNext = sCurChunk->mTop;
    sCurChunk->mTop  = sFreeElem;

    sCurChunk->mFreeSlotCnt++;
    IDE_ASSERT(sCurChunk->mFreeSlotCnt <= sCurChunk->mMaxSlotCnt);

    //slot cnt�� �ٲ������ ���� ����Ʈ�̵��� �Ͼ�� �ʾ����Ƿ� check�ϸ� �ȵ�!
    //iduCheckMemConsistency(this);

    /*
     * Partial List -> Free List, Full list -> Partial list :�Ϲ����� ���.
     * Full list -> Free list : chunk�� slot�� �Ѱ��϶�
     */
    if (sCurChunk->mFreeSlotCnt == sCurChunk->mMaxSlotCnt )
    {
        unlink(sCurChunk);

        if( sCurChunk->mMaxSlotCnt  == 1 ) //Full list -> Free list
        {
            IDE_ASSERT( mFullChunkCnt > 0 );
            mFullChunkCnt--;
        }
        else   //Partial List -> Free List
        {
            mPartialChunkCnt--;
        }

        //Limit�� �Ѿ�� ��� autofree
        if (mFreeChunkCnt <= mAutoFreeChunkLimit)
        {
            link(&mFreeChunk, sCurChunk);
            mFreeChunkCnt++;
        }
        else
        {
            IDE_TEST(iduMemMgr::free(sCurChunk) != IDE_SUCCESS);
        }
    }
    else
    {
        // Full list -> Partial list
        if (sCurChunk->mFreeSlotCnt == 1)
        {
            unlink(sCurChunk);
            link(&mPartialChunk, sCurChunk);

            IDE_ASSERT( mFullChunkCnt > 0 );
            mFullChunkCnt--;
            mPartialChunkCnt++;
        }
    }
#endif

    iduCheckMemConsistency(this);

#if !defined(ALTIBASE_MEMORY_CHECK)
    if ( mUseMutex == ID_TRUE )
    {
        IDE_ASSERT( mMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* No synchronisation */
    }
#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

#if !defined(ALTIBASE_MEMORY_CHECK)
    if ( mUseMutex == ID_TRUE )
    {
        IDE_ASSERT( mMutex.unlock() == IDE_SUCCESS );
    }
    else
    {
        /* No synchronisation */
    }
#endif

    return IDE_FAILURE;

}

IDE_RC iduMemListOld::cralloc(void **aMem)
{
    IDE_TEST(alloc(aMem) != IDE_SUCCESS);
    idlOS::memset(*aMem, 0, mElemSize);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iduMemListOld::shrink( UInt *aSize)
{
    iduMemChunkQP *sCur = NULL;
    iduMemChunkQP *sNxt = NULL;
    UInt         sFreeSizeDone = mFreeChunkCnt*mChunkSize;

    IDE_ASSERT( aSize != NULL );

    *aSize = 0;

    sCur = mFreeChunk.mNext;
    while( sCur != NULL )
    {
        sNxt = sCur->mNext;
        unlink(sCur);
        mFreeChunkCnt--;

        IDE_TEST(iduMemMgr::free(sCur) != IDE_SUCCESS);
        sCur = sNxt;
    }

    IDE_ASSERT( mFreeChunkCnt == 0 ); //�翬.

    *aSize = sFreeSizeDone;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


#ifdef NOTDEF // for testing
SInt tmp()
{
    iduMemListOld *p;
    SChar *k;

    p->alloc((void **)&k);
    p->memfree(k);

    return 0;
}
#endif

UInt iduMemListOld::getUsedMemory()
{
    UInt sSize;

    sSize = mElemSize * mFreeChunkCnt * mElemCnt;
    sSize += (mElemSize * mFullChunkCnt * mElemCnt);

    return sSize;
}

void iduMemListOld::status()
{
    SChar sBuffer[IDE_BUFFER_SIZE];

    IDE_CALLBACK_SEND_SYM_NOLOG("    Mutex Internal State\n");
    mMutex.status();

    idlOS::snprintf(sBuffer, ID_SIZEOF(sBuffer),
                    "    Memory Usage: %"ID_UINT32_FMT" KB\n", (UInt)(getUsedMemory() / 1024));
    IDE_CALLBACK_SEND_SYM_NOLOG(sBuffer);
}

/*
 * X$MEMPOOL�� ���ؼ� �ʿ��� ������ ä��.
 */
void iduMemListOld::fillMemPoolInfo( struct iduMemPoolInfo * aInfo )
{
    IDE_ASSERT( aInfo != NULL );

    aInfo->mFreeCnt    = mFreeChunkCnt;
    aInfo->mFullCnt    = mFullChunkCnt;
    aInfo->mPartialCnt = mPartialChunkCnt;
    aInfo->mChunkLimit = mAutoFreeChunkLimit;
    aInfo->mChunkSize  = mChunkSize;
    aInfo->mElemCnt    = mElemCnt;
    aInfo->mElemSize   = mElemSize;
}

