/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduMemList.cpp 84564 2018-12-10 05:40:03Z kclee $
 **********************************************************************/

#include <idl.h>
#include <idu.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <ideLog.h>
#include <iduRunTimeInfo.h>
#include <iduMemList.h>
#include <iduMemPoolMgr.h>

#ifndef X86_64_DARWIN
#include <malloc.h>
#endif

extern const vULong gFence =
#ifdef COMPILE_64BIT
ID_ULONG(0xDEADBEEFDEADBEEF);
#else
0xDEADBEEF;
#endif


#undef IDU_ALIGN
#define IDU_ALIGN(x)          idlOS::align(x,mP->mAlignByte)

//get chunk address in the type TIGHT.
#define IDU_GET_CHUNK_FOR_TIGHT(addr) (iduMemChunk *)( (ULong)addr - mP->mChunkSize )

void iduCheckMemConsistency(iduMemList * aMemList)
{
    return;

    vULong sCnt = 0;
    iduMemSlot *sSlot;

    //check free chunks
    iduMemChunk *sCur = aMemList->mFreeChunk.mNext;
    while(sCur != NULL)
    {
        sSlot = (iduMemSlot *)sCur->mTop;
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
        /*
         *���� �˰��򿡼� slot�� �Ѱ��� chunk�� partial chunk list
         *�� �����Ҽ��� ����. free�� full chunk list���� �����ؾ���.
         */
        IDE_ASSERT( sCur->mMaxSlotCnt > 1 );

        //sCur->mMaxSlotCnt > 1
        IDE_ASSERT( (0  < sCur->mFreeSlotCnt) &&  
                    (sCur->mFreeSlotCnt < sCur->mMaxSlotCnt) );

        sCur = sCur->mNext;
        sCnt++;
    }

    IDE_ASSERT(sCnt == aMemList->mPartialChunkCnt);
}


#ifndef MEMORY_ASSERT
#define iduCheckMemConsistency( xx )
#endif

iduMemList::iduMemList()
{
}

iduMemList::~iduMemList(void)
{
}

/*---------------------------------------------------------
  iduMemList

   ___________ mFullChunk
  |iduMemList |    _____      _____
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
  |iduMemChunk |Slot|Slot|... | Slot|
  |____________|____|____|____|_____|

  iduMemChunk        : chunk ���(���� chunk�� ���� ������ ��� �ִ�.)
  
  
  
  Slot:
   ___________________________________
  |momory element |iduMemChunk pointer|
  |_______________|___________________|

  memory element      : ����ڰ� ���� �޸𸮸� ����ϴ� ����
             �� ������ ����ڿ��� �Ҵ�Ǿ� ���� ��������, ����
             (free)slot������ ����Ű�� ������ ��Ȱ�� �Ѵ�.
  iduMemChunk pointer : slot�� ���� chunk�� ���(chunk�� iduMemChunk����)��
                        ������ �ϰ� �ִ�.
  -----------------------------------------------------------*/
/*-----------------------------------------------------------
 * task-2440 iduMemPool���� �޸𸮸� ���ö� align�� �޸� �ּҸ� ���
 *          �� �� �ְ� ����.
 *          
 * chunk������ align�� �Ͼ�� ���� �����ϰڴ�.
 * ___________________________________________
  |iduMemChunk |   |  Slot|  Slot|... |   Slot|
  |____________|___|______|______|____|_______|
  ^            ^   ^      ^
  1            2   3      4
  
  �ϴ� chunk�� �Ҵ��ϸ� 1���� ������ �ּҰ����� �����ȴ�.  �� �ּҰ���
  ����ڰ� align�Ǳ⸦ ���ϴ� �ּҰ� �ƴϱ� ������ �׳� ���д�.
  2���ּ� = 1�� �ּ� + SIZEOF(iduMemChunk)
  2�� �ּҴ� ���� ���� ��������, 2���ּҸ� align up��Ű�� 3��ó�� �� ������
  ����Ű�� �ȴ�. �׷��� 3���� align�� �Ǿ��ְ� �ȴ�.
  ������ ����ڿ��� �Ҵ� �Ǵ� ������ 3�� ���� �����̴�.
  4���� align�Ǿ� �ֱ� ���ؼ��� slotSize�� align �Ǿ� �־�� �Ѵ�.
  �׷��� ������ SlotSize�� ���� size���� align up�� ���� ����Ѵ�.
  �׷��� ��� slot�� �ּҴ� align�Ǿ������� �� ���ִ�.

  ���� 2���� 3������ ������ �ִ� ũ��� alignByte�̴�.
  �׷��� ������
  chunkSize = SIZEOF(iduMemChunk)+ alignByte +
              ('alignByte�� align up�� SIZEOF(slot)' * 'slot�� ����')
  �̴�.
 * ---------------------------------------------------------*/

/****************************************************** 
 BUG-46165 

 S:Slot  
 H:chunk Header    
 A:chunk header Address:8byte , the rest: waste 

 When slot size is 8k ,alignment size is 8k,and count is 8,

 in IDU_MEMPOOL_TYPE_TIGHT, 

  8k 8k 8k 8k 8k 8k 8k 8k
 +--+--+--+--+--+--+--+--+-+
 |S |S |S |S |S |S |S |S |H|
 +--+--+--+--+--+--+--+--+-+

 in IDU_MEMPOOL_TYPE_LEGACY,

    8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k 8k     
 +-+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 |H|S |A |S |A |S |A |S |A |S |A |S |A |S |A |S |A |
 +-+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+

******************************************************/
/*-----------------------------------------------------------
 * Description: iduMemList�� �ʱ�ȭ �Ѵ�.
 *
 * aSeqNumber        - [IN] iduMemList �ĺ� ��ȣ
 * aParent           - [IN] �ڽ��� ���� iduMemPool
 * ---------------------------------------------------------*/
/*-----------------------------------------------------------
 * task-2440 iduMemPool���� �޸𸮸� ���ö� align�� �޸� �ּҸ� ���
 *          �� �� �ְ� ����.
 *          
 * chunkSize = SIZEOF(iduMemChunk)+ alignByte +
 *             ('alignByte�� align up�� SIZEOF(slot)' * 'slot�� ����')
 */

IDE_RC iduMemList::initialize( UInt         aSeqNumber,
                               iduMemPool * aParent)
{

    SChar sBuffer[IDU_MUTEX_NAME_LEN*2];//128 bytes

    mP = aParent;

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

    IDE_TEST(grow() != IDE_SUCCESS);

    if (mP->mFlag & IDU_MEMPOOL_USE_MUTEX)
    {

        idlOS::memset(sBuffer, 0, ID_SIZEOF(sBuffer));
        idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                         "%s"IDU_MEM_POOL_MUTEX_POSTFIX"%"ID_UINT32_FMT, mP->mName, aSeqNumber);


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
 * iduMemList�� mFreeChunk�� mFullChunk�� �ΰ��� chunk list�� �����Ѵ�.
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
IDE_RC iduMemList::destroy(idBool aBCheck)
{
    vULong i;
    iduMemChunk *sCur = NULL;

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

        if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
        {
            sCur  = IDU_GET_CHUNK_FOR_TIGHT(sCur);
            IDE_TEST( iduMemMgr::free4malign(sCur, 
                                             mP->mIndex, 
                                             mP->mChunkSize + ID_SIZEOF(iduMemChunk))
                                     != IDE_SUCCESS);
        }
        else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
        {
            IDE_TEST( iduMemMgr::free(sCur) != IDE_SUCCESS);
        }
    }

/* BUG-45749 */
#ifdef DEBUG
    if(aBCheck == ID_TRUE)
    {
        if( mFullChunkCnt != 0 )
        {
            ideLog::log(IDE_SERVER_0,ID_TRC_MEMLIST_WRONG_FULL_CHUNK_COUNT,
                        (UInt)mFullChunkCnt);
        }
        else
        {
            /* do nothing */
        }

        if( mPartialChunkCnt != 0 )
        {
            ideLog::log(IDE_SERVER_0,ID_TRC_MEMLIST_WRONG_PARTIAL_CHUNK_COUNT,
                        (UInt)mPartialChunkCnt);
        }
        else
        {
            /* do nothing */
        }
    }
#endif

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

        if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
        {
            sCur  = IDU_GET_CHUNK_FOR_TIGHT(sCur);
            IDE_TEST( iduMemMgr::free4malign(sCur, 
                                             mP->mIndex, 
                                             mP->mChunkSize + ID_SIZEOF(iduMemChunk) )
                                     != IDE_SUCCESS);
        }
        else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
        {
            IDE_TEST( iduMemMgr::free(sCur) != IDE_SUCCESS);
        }
    }

    // BUG-19253
    // mFullChunk list�� ����� chunk�� ������ free��Ų��.
    for (i = 0; i < mFullChunkCnt; i++)
    {
        sCur = mFullChunk.mNext;
        IDE_ASSERT(sCur != NULL);

        unlink(sCur);

        if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
        {
            sCur  = IDU_GET_CHUNK_FOR_TIGHT(sCur);
            IDE_TEST( iduMemMgr::free4malign(sCur, 
                                             mP->mIndex, 
                                             mP->mChunkSize + ID_SIZEOF(iduMemChunk))
                                     != IDE_SUCCESS);
        }
        else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
        {
            IDE_TEST( iduMemMgr::free(sCur) != IDE_SUCCESS);
        }
    }

    if (mP->mFlag & IDU_MEMPOOL_USE_MUTEX)
    {
        IDE_TEST(mMutex.destroy() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

inline void iduMemList::unlink(iduMemChunk *aChunk)
{
    iduMemChunk *sBefore = aChunk->mPrev;
    iduMemChunk *sAfter  = aChunk->mNext;

    sBefore->mNext = sAfter;
    if (sAfter != NULL)
    {
        sAfter->mPrev = sBefore;
    }
}

inline void iduMemList::link(iduMemChunk *aBefore, iduMemChunk *aChunk)
{
    iduMemChunk *sAfter   = aBefore->mNext;

    aBefore->mNext        = aChunk;
    aChunk->mPrev         = aBefore;
    aChunk->mNext         = sAfter;

    if (sAfter != NULL)
    {
        sAfter->mPrev = aChunk;
    }
}

IDE_RC iduMemList::grow(void)
{

    vULong       i;
    iduMemChunk *sChunk=NULL;
    iduMemSlot  *sSlot;
    iduMemSlot  *sFirstSlot;



#ifdef MEMORY_ASSERT
    IDE_TEST(iduMemMgr::malloc(mP->mIndex,
                               mP->mChunkSize,
                               (void**)&sChunk,
                               IDU_MEM_FORCE)
             != IDE_SUCCESS);

#else
    if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
    {
        IDE_TEST(iduMemMgr::malign(mP->mIndex,
                                   mP->mChunkSize + ID_SIZEOF(iduMemChunk),
                                   mP->mChunkSize,
                                   (void**)&sChunk,
                                   IDU_MEM_FORCE)
                 != IDE_SUCCESS);

        IDE_DASSERT( ((ULong)sChunk & (ULong)(mP->mChunkSize - 1) ) == 0 );

        sChunk         = (iduMemChunk *)( (ULong)sChunk + mP->mChunkSize );
    }
    else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
    {
        IDE_TEST(iduMemMgr::malloc(mP->mIndex,
                                   mP->mChunkSize,
                                   (void**)&sChunk,
                                   IDU_MEM_FORCE)
                 != IDE_SUCCESS);
    }
#endif


    IDE_ASSERT( sChunk != NULL ); 


    *(iduMemList**)&sChunk->mParent         = this;
    sChunk->mMaxSlotCnt  = mP->mElemCnt;
    sChunk->mFreeSlotCnt = mP->mElemCnt;
    sChunk->mTop       = NULL;

    /*chunk���� �� slot�� mNext�� �����Ѵ�. ���� �������� �ִ� slot�� sChunk->mTop�� ����ȴ�.
     * mNext�� ����ڿ��� �Ҵ�ɶ��� ���� ����ϴ� �������� ���δ�.*/
    if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
    {
        sFirstSlot = (iduMemSlot*)( (ULong)sChunk - mP->mChunkSize );
    }
    else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
    {
        sFirstSlot = (iduMemSlot*)( (UChar*)sChunk + CHUNK_HDR_SIZE );

    }

    sFirstSlot = (iduMemSlot*) IDU_ALIGN(sFirstSlot);
    for (i = 0; i < mP->mElemCnt; i++)
    {
        sSlot        = (iduMemSlot *)((UChar *)sFirstSlot + (i * mP->mSlotSize));
        sSlot->mNext = sChunk->mTop;
        sChunk->mTop = sSlot;
#ifdef MEMORY_ASSERT
        *((vULong *)((UChar *)sSlot + mP->mElemSize)) = gFence;
        *((iduMemChunk **)((UChar *)sSlot +
                           mP->mElemSize +
                           ID_SIZEOF(gFence))) = sChunk;
        *((vULong *)((UChar *)sSlot +
                     mP->mElemSize +
                     ID_SIZEOF(gFence)  +
                     ID_SIZEOF(iduMemChunk *))) = gFence;
#else
        if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
        {
                *((iduMemChunk **)((UChar *)sSlot + mP->mElemSize)) = sChunk;
        }
        else if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
        {
            /*we don't need to do anything here */
        }
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

IDE_RC iduMemList::alloc(void **aMem)
{

    iduMemChunk *sMyChunk=NULL;



#if defined(ALTIBASE_MEMORY_CHECK)
    IDE_TEST( iduMemMgr::malign(mP->mIndex,mP->mElemSize,mP->mAlignByte, aMem)
              != IDE_SUCCESS);

    IDE_ASSERT(*aMem != NULL);
#else /* normal case : not memory check */
    
    iduCheckMemConsistency(this);

    //victim chunk�� ����.
    if (mPartialChunk.mNext != NULL) 
    {
        sMyChunk = mPartialChunk.mNext;
    }
    else
    {
        if (mFreeChunk.mNext == NULL) 
        {
            IDE_TEST(grow() != IDE_SUCCESS);
        }
        sMyChunk = mFreeChunk.mNext;

        IDE_ASSERT( sMyChunk != NULL );
    }

    iduCheckMemConsistency(this);

    IDE_ASSERT( sMyChunk->mTop != NULL );

    *aMem           = sMyChunk->mTop;

    sMyChunk->mTop = ((iduMemSlot *)(*aMem))->mNext;

    if ((--sMyChunk->mFreeSlotCnt) == 0) // * Partial list ->Full list
    {
        unlink(sMyChunk);
        link(&mFullChunk, sMyChunk);
        
        IDE_ASSERT( mPartialChunkCnt > 0 );
        mPartialChunkCnt--;
        mFullChunkCnt++;
    }
    else if ( (sMyChunk->mMaxSlotCnt - sMyChunk->mFreeSlotCnt) == 1)//Free list ->Partial list
    {
            unlink(sMyChunk);
            link(&mPartialChunk, sMyChunk);

            IDE_ASSERT( mFreeChunkCnt > 0 );
            mFreeChunkCnt--;
            mPartialChunkCnt++;
    }

    iduCheckMemConsistency(this);

#endif

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC iduMemList::memfree(void *aFreeElem)
{
    iduMemSlot*  sFreeElem;
    iduMemChunk *sCur;

    IDE_ASSERT(aFreeElem != NULL);

#if defined(ALTIBASE_MEMORY_CHECK)
    IDE_TEST( iduMemMgr::free4malign( aFreeElem, mP->mIndex,mP->mElemSize ) 
              != IDE_SUCCESS);
#else /* normal case : not memory check */


#  ifdef MEMORY_ASSERT

    IDE_ASSERT(*((vULong *)((UChar *)aFreeElem + mP->mElemSize)) == gFence);
    IDE_ASSERT(*((vULong *)((UChar *)aFreeElem +
                        mP->mElemSize +
                        ID_SIZEOF(gFence)  +
                        ID_SIZEOF(iduMemChunk *))) == gFence);
    sCur         = *((iduMemChunk **)((UChar *)aFreeElem + mP->mElemSize +
                                           ID_SIZEOF(gFence)));
#  else
    if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
    {
        sCur   = (iduMemChunk *)( ( (ULong)aFreeElem & (ULong)~(mP->mChunkSize - 1 )) 
                                    + mP->mChunkSize ); 
    }
    else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
    {
        sCur   = *((iduMemChunk **)((UChar *)aFreeElem + mP->mElemSize));
    }
#endif

    sFreeElem        = (iduMemSlot*)aFreeElem;
    sFreeElem->mNext = sCur->mTop;
    sCur->mTop  = sFreeElem;

    sCur->mFreeSlotCnt++;
    IDE_ASSERT(sCur->mFreeSlotCnt <= sCur->mMaxSlotCnt);


    if (sCur->mFreeSlotCnt == sCur->mMaxSlotCnt )  //Partial List -> Free List
    {
        unlink(sCur);
        
        mPartialChunkCnt--;

        //Limit�� �Ѿ�� ��� autofree
        if (mFreeChunkCnt <= mP->mAutoFreeChunkLimit)
        {
            link(&mFreeChunk, sCur);
            mFreeChunkCnt++;
        }
        else
        {
            if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
            {
                sCur  = IDU_GET_CHUNK_FOR_TIGHT(sCur);
                IDE_TEST( iduMemMgr::free4malign(sCur, 
                                                 mP->mIndex, 
                                                 mP->mChunkSize + ID_SIZEOF(iduMemChunk))
                                         != IDE_SUCCESS);
            }
            else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
            {
                IDE_TEST( iduMemMgr::free(sCur) != IDE_SUCCESS);
            }
        }
    }
    else // Full list -> Partial list
    {
        if (sCur->mFreeSlotCnt == 1)
        {
            unlink(sCur);
            link(&mPartialChunk, sCur);

            IDE_ASSERT( mFullChunkCnt > 0 );
            mFullChunkCnt--;
            mPartialChunkCnt++;
        }
    }

    iduCheckMemConsistency(this);

#endif

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

IDE_RC iduMemList::cralloc(void **aMem)
{
    IDE_TEST(alloc(aMem) != IDE_SUCCESS);
    idlOS::memset(*aMem, 0, mP->mElemSize);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


IDE_RC iduMemList::shrink( UInt *aSize)
{
    iduMemChunk *sCur = NULL;
    iduMemChunk *sNxt = NULL;
    UInt         sFreeSizeDone = mFreeChunkCnt*mP->mChunkSize;

    IDE_ASSERT( aSize != NULL );

    *aSize = 0;

    sCur = mFreeChunk.mNext;
    while( sCur != NULL )
    {
        sNxt = sCur->mNext;
        unlink(sCur);
        mFreeChunkCnt--;
        if( mP->mType == IDU_MEMPOOL_TYPE_TIGHT )
        {
            sCur  = IDU_GET_CHUNK_FOR_TIGHT(sCur);
            IDE_TEST( iduMemMgr::free4malign(sCur, 
                                             mP->mIndex, 
                                             mP->mChunkSize + ID_SIZEOF(iduMemChunk))
                                     != IDE_SUCCESS);
        }
        else if( mP->mType == IDU_MEMPOOL_TYPE_LEGACY )
        {
            IDE_TEST( iduMemMgr::free(sCur) != IDE_SUCCESS);
        }

        sCur = sNxt;
    }

    IDE_ASSERT( mFreeChunkCnt == 0 ); //�翬.

    *aSize = sFreeSizeDone;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


UInt iduMemList::getUsedMemory()
{
    UInt sSize;

    sSize = mP->mElemSize * mFreeChunkCnt * mP->mElemCnt;
    sSize += (mP->mElemSize * mFullChunkCnt * mP->mElemCnt);

    return sSize;
}

void iduMemList::status()
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
void iduMemList::fillMemPoolInfo( struct iduMemPoolInfo * aInfo )
{
    IDE_ASSERT( aInfo != NULL );

    aInfo->mFreeCnt    = mFreeChunkCnt;
    aInfo->mFullCnt    = mFullChunkCnt;
    aInfo->mPartialCnt = mPartialChunkCnt;
    aInfo->mChunkLimit = mP->mAutoFreeChunkLimit;
    aInfo->mChunkSize  = mP->mChunkSize;
    aInfo->mElemCnt    = mP->mElemCnt;
    aInfo->mElemSize   = mP->mElemSize;
}

