/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduOIDMemory.cpp 67683 2014-11-24 10:39:13Z djin $
 *
 *  BUG-22877 ���ü� ������ alloc�� free ������ �ִ� mutex�� �ϳ��� �ٿ���.
 *  ������ �� mutex�� �����ϱ� ���� allock list�� ù page�� free����
 *  �ʰ� ���� �ξ��ٰ�, free�� �� �ְ� �Ǿ��� �� ��Ƽ� shrink�Ͽ�����,
 *  ���� ù page�� ������ free�Ͽ� �������� page�� �����Ƿ�
 *  �� page���� free�ȴ�. shrink()�� �����ϰ� memFree()����
 *  ���� page�� free�ϵ��� �����Ͽ���.
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <ideLog.h>
#include <iduOIDMemory.h>

/******************************************************************************
 * Description :
 ******************************************************************************/
iduOIDMemory::iduOIDMemory()
{
}

/******************************************************************************
 * Description :
 ******************************************************************************/
iduOIDMemory::~iduOIDMemory()
{
}

/******************************************************************************
 * Description : �ɹ� ���� �ʱ�ȭ
 * aIndex     - [IN] Memory Client Index
 * aElemSize  - [IN] element�� �ϳ��� ũ��
 * aElemCount - [IN] �� page�� element�� ��
 ******************************************************************************/
IDE_RC iduOIDMemory::initialize(iduMemoryClientIndex aIndex,
                                vULong               aElemSize,
                                vULong               aElemCount )
{
    mIndex = aIndex;

    // 1. Initialize Item Size
    mElemSize = idlOS::align( aElemSize, sizeof(SDouble) );
    mItemSize = idlOS::align( mElemSize + sizeof(iduOIDMemItem), sizeof(SDouble) );

    // 2. Initialize Item Count and Page Size
    mItemCnt  = aElemCount;
    mPageSize = idlOS::align( sizeof(iduOIDMemAllocPage) + (mItemSize * mItemCnt),
                              sizeof(SDouble) );

    // 3. Initialize Other Members
    mAllocPageHeader.prev = &mAllocPageHeader;
    mAllocPageHeader.next = &mAllocPageHeader;
    mAllocPageHeader.freeIncCount  = 0;
    mAllocPageHeader.allocIncCount = 0;
    mPageCntInAllocLst    = 0;

    mFreePageHeader.next = NULL;
    mPageCntInFreeLst    = 0;

    // 4. Initialize Mutex
    IDE_TEST_RAISE( mMutex.initialize((SChar *)"OID_MEMORY_MUTEX",
                                           IDU_MUTEX_KIND_NATIVE,
                                           IDV_WAIT_INDEX_NULL )
                    != IDE_SUCCESS,error_mutex_init );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_mutex_init);
    {
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : ��� ���� ���� �� �޸� ����
 ******************************************************************************/
IDE_RC iduOIDMemory::destroy()
{
    iduOIDMemAllocPage * sNxtAllocPage;
    iduOIDMemAllocPage * sCurAllocPage;
    iduOIDMemFreePage  * sNxtFreePage;
    iduOIDMemFreePage  * sCurFreePage;

    // alloc page list �޸� ����
    sCurAllocPage = mAllocPageHeader.prev;

    while( sCurAllocPage != &mAllocPageHeader )
    {
        sNxtAllocPage = sCurAllocPage->next;
        IDE_TEST(iduMemMgr::free(sCurAllocPage)
                 != IDE_SUCCESS);
        sCurAllocPage = sNxtAllocPage;
    }

    mAllocPageHeader.next = &mAllocPageHeader;
    mAllocPageHeader.prev = &mAllocPageHeader;

    // free page list �޸� ����
    sCurFreePage = mFreePageHeader.next;

    while( sCurFreePage != NULL )
    {
        sNxtFreePage = sCurFreePage->next;

        IDE_TEST(iduMemMgr::free(sCurFreePage)
                 != IDE_SUCCESS);

        sCurFreePage = sNxtFreePage;
    }

    mFreePageHeader.next = NULL;

    IDE_TEST_RAISE( mMutex.destroy() != IDE_SUCCESS,
                    error_mutex_destroy );

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_mutex_destroy);
    {
        IDE_SET_AND_DIE(ideSetErrorCode(idERR_FATAL_ThrMutexDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/******************************************************************************
 * Description : aMem�� �޸� �Ҵ�
 * aMem  - [OUT] �Ҵ���� �޸��ּҸ� ��ȯ
 ******************************************************************************/
IDE_RC iduOIDMemory::alloc(void ** aMem)
{
    vULong sHeaderSize;
    UInt   sState = 0;

    sHeaderSize = idlOS::align( sizeof(iduOIDMemAllocPage), sizeof(SDouble) );
    *aMem = NULL;

    // 1. Lock ȹ��
    IDE_ASSERT( lockMtx() == IDE_SUCCESS );
    sState = 1;

    while( *aMem == NULL )
    {
        // 2. alloc page list�� free item�� �ִ��� Ȯ��
        if( (mAllocPageHeader.next != &mAllocPageHeader) &&
            (mAllocPageHeader.next->allocIncCount < mItemCnt) )
        {
            *aMem = (iduOIDMemItem *)
                    ((SChar *)mAllocPageHeader.next + sHeaderSize +
                     (mAllocPageHeader.next->allocIncCount * mItemSize));
            mAllocPageHeader.next->allocIncCount++;
        }
        else
        {
            // 2. alloc page list�� page�� �ϳ��� ���ų�
            //    page�� �ִ���� free item�� ���� ���,
            //    kernel or free page list�� ���� �޸𸮸� �Ҵ�޴´�.
            IDE_TEST( grow() != IDE_SUCCESS );
        }

    } //while

    // 3. Lock ����
    sState = 0;
    IDE_ASSERT( unlockMtx() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState != 0)
    {
        IDE_ASSERT( unlockMtx() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : aMem�� �޸� �ݳ�
 * aMem  - [IN] �ݳ��� element �޸��ּ�
 ******************************************************************************/
IDE_RC iduOIDMemory::memfree(void * aMem)
{
    iduOIDMemAllocPage * sFreePage;
    iduOIDMemFreePage  * sFreePage2OS = NULL;
    iduOIDMemItem      * sMemItem;

    IDE_ASSERT( lockMtx() == IDE_SUCCESS );

    sMemItem  = (iduOIDMemItem *)((SChar *)aMem + mElemSize);
    sFreePage = sMemItem->myPage;

    // 1. �ش� page�� freeIncCount ����.
    sFreePage->freeIncCount++;

    // 2 �ش� page�� freeIncCount�� mItemCnt ��
    if( sFreePage->freeIncCount == mItemCnt )
    {
        // remove from alloc page list
        sFreePage->next->prev = sFreePage->prev;
        sFreePage->prev->next = sFreePage->next;

        // count ����
        mPageCntInAllocLst--;

        // free page count��
        // IDU_OID_MEMORY_AUTOFREE_LIMIT �̻��̸� kernel�� �ݳ�.

        if( mPageCntInFreeLst > IDU_OID_MEMORY_AUTOFREE_LIMIT )
        {
            // lock��� free�ϴ� ���� ���ϱ� ���� ������ ������
            // �ξ��ٰ� ���Ŀ� lock�� Ǯ�� free �Ѵ�.
            sFreePage2OS = (iduOIDMemFreePage*)sFreePage;
        }
        else
        {
            //  free page list�� ����
            ((iduOIDMemFreePage*)sFreePage)->next
                                 = mFreePageHeader.next;
            mFreePageHeader.next = (iduOIDMemFreePage*)sFreePage;

            // count ����
            mPageCntInFreeLst++;
        }
    }
    else
    {
        // �ش� page�� ���� free ���� ���� Item���� ���� �ִ�.
        IDE_ASSERT( sFreePage->freeIncCount < mItemCnt );
    }

    IDE_ASSERT( unlockMtx() == IDE_SUCCESS );

    if( sFreePage2OS != NULL )
    {
        IDE_TEST( iduMemMgr::free( sFreePage2OS )
                  != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    IDE_ASSERT( unlockMtx() == IDE_SUCCESS );

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : mFreePageHeader or kernel�κ��� �޸� �Ҵ�.
 ******************************************************************************/
void iduOIDMemory::dumpState(SChar * aBuffer, UInt aBufferSize)
{
    idlOS::snprintf( aBuffer, aBufferSize, "MemMgr Used Memory:%"ID_UINT64_FMT"\n",
                     ((mPageCntInAllocLst + mPageCntInFreeLst ) * mPageSize) );
}

/******************************************************************************
 * Description :
 ******************************************************************************/
void iduOIDMemory::status()
{
    SChar sBuffer[IDE_BUFFER_SIZE];

    IDE_CALLBACK_SEND_SYM_NOLOG( "     Mutex Internal State\n" );
    mMutex.status();

    idlOS::snprintf( sBuffer, ID_SIZEOF(sBuffer),
                     " Memory Usage: %"ID_UINT32_FMT" KB\n",
                     (UInt)((mPageCntInAllocLst + mPageCntInFreeLst ) * mPageSize)/1024 );
    IDE_CALLBACK_SEND_SYM_NOLOG( sBuffer );
}


/******************************************************************************
 * Description :
 ******************************************************************************/
ULong iduOIDMemory::getMemorySize()
{
    ULong sSize;

    sSize = ( mPageCntInAllocLst + mPageCntInFreeLst ) * mPageSize;

    return sSize;
}

/******************************************************************************
 * Description : mFreePageHeader or kernel�κ��� �޸� �Ҵ�.
 ******************************************************************************/
IDE_RC iduOIDMemory::grow()
{
    vULong i;
    iduOIDMemAllocPage * sMemPage;
    iduOIDMemItem      * sMemItem;
    vULong               sPageHeaderSize;

    sPageHeaderSize = idlOS::align( sizeof(iduOIDMemAllocPage), sizeof(SDouble) );

    // 1. free page list�� �������� �����ϴ��� �˻�.
    if( mFreePageHeader.next != NULL )
    {
        // 2. alloc page list�� �Ҵ��� page �ּ����� & free page list���� page ����
        sMemPage = (iduOIDMemAllocPage*)mFreePageHeader.next;
        mFreePageHeader.next = mFreePageHeader.next->next;

        // count ����
        IDE_ASSERT( mPageCntInFreeLst > 0 );
        mPageCntInFreeLst--;
    }
    else
    {
        // 2. free page list�� �������� �ϳ��� ������ ���, Ŀ�ηκ��� �Ҵ�.
        IDE_TEST(iduMemMgr::malloc(mIndex,
                                   mPageSize,
                                   (void**)&sMemPage,
                                   IDU_MEM_FORCE)
                 != IDE_SUCCESS);

        // �� element���� iduMemItem �ʱ�ȭ
        for( i = 0; i < mItemCnt; i++ )
        {
            sMemItem = (iduOIDMemItem *)((UChar *)sMemPage + sPageHeaderSize
                       + (i * mItemSize) + mElemSize);
            sMemItem->myPage = sMemPage;
        }
    }

    // 3. alloc page list�� ����
    sMemPage->freeIncCount  = 0;
    sMemPage->allocIncCount = 0;
    sMemPage->next = mAllocPageHeader.next;
    sMemPage->prev = mAllocPageHeader.next->prev;

    mAllocPageHeader.next->prev = sMemPage;
    mAllocPageHeader.next = sMemPage;

    // count ����
    mPageCntInAllocLst++;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
