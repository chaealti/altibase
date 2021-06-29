/***********************************************************************
 * Copyright 1999-2000, RTBase Corporation or its subsidiaries.
 * All rights reserved.
 **********************************************************************/

/***********************************************************************
 * $Id: iduOIDMemory.h 31545 2009-03-09 05:24:18Z a464 $
 **********************************************************************/

#ifndef _O_IDU_OID_MEMORY_H_
#define _O_IDU_OID_MEMORY_H_ 1

#include <idl.h>
#include <idu.h>
#include <iduMutex.h>

#define IDU_OID_MEMORY_AUTOFREE_LIMIT 5  // free page Count in free page list

struct iduOIDMemAllocPage
{
    vULong               freeIncCount;    // free call count
    vULong               allocIncCount;   // alloc call count
    iduOIDMemAllocPage * prev;            // previous page
    iduOIDMemAllocPage * next;            // next page

};

// BUG-22877
// alloc list�� ���� ���� list�̰� free list�� ���� ���� list �̹Ƿ�
// ȥ���� ���ϱ� ����, free list�� single link list page header��
// ������ �����ؼ� ĳ�����Ͽ� ����Ѵ�.
struct iduOIDMemFreePage
{
    iduOIDMemFreePage * next;      // next page
};


struct iduOIDMemItem
{
    iduOIDMemAllocPage * myPage;  // item�� �Ҽӵ� page
};

/*----------------------------------------------------------------
  Name : iduOIDMemory Class
  Arguments :
  Description :
      OID�� �޸� �������� iduMemMgr�� ������ �غ��ϱ� ���Ͽ� ����.
      - iduMemMgr�� ���� ����
          : free�� latch�� ȹ������ ����
      - iduMemMgr�� ���� �غ�
          : kernel���� �޸� �ݳ��� �����
      - allocation page list�� free page list�� �����ϸ�,
        �޸� �Ҵ��� allocation page list�� top page�κ���
        ���������� �Ҵ��ϸ�(iduMemMgr�� �ٸ�),
        �޸� ������ �ش� Page�� ��� �� ��� free page list��
        ����ϰų� kernel�� �ݳ��Ͽ� �޸� ������ �� �� �ֵ��� �Ѵ�.
        iduMemMgr���� free slot list�� top list�� alloc���� �ʴ� �����
        allocation page list�� top�� free page list�� tail�� �����Ͽ�
        memfree()�ÿ� latch�� ������� �ʵ��� �Ѵ�.
  ----------------------------------------------------------------*/

class iduOIDMemory
{
public:
    iduOIDMemory();
    ~iduOIDMemory();

    IDE_RC     initialize(iduMemoryClientIndex aIndex,
                          vULong               aElemSize,
                          vULong               aElemCount );
    IDE_RC     destroy();

    IDE_RC     alloc(void ** aMem );
    IDE_RC     memfree( void * aMem );

    void       dumpState( SChar * aBuffer, UInt aBufferSize );
    void       status();
    ULong      getMemorySize();

    IDE_RC     lockMtx() { return mMutex.lock(NULL /* idvSQL* */); }
    IDE_RC     unlockMtx() { return mMutex.unlock(); }

private:
    IDE_RC     grow();         // add to allocation page list

private:
    iduMemoryClientIndex  mIndex;

    iduMutex   mMutex;

    vULong     mElemSize;      // size of element
    vULong     mItemSize;      // size of item (= element + OIDMemItem)
    vULong     mItemCnt;       // item count in a page
    vULong     mPageSize;      // page size

    iduOIDMemAllocPage mAllocPageHeader;   // alloc page list
    ULong              mPageCntInAllocLst; // alloc page ��

    iduOIDMemFreePage  mFreePageHeader;    // free page list
    ULong              mPageCntInFreeLst;  // free page ��
};

#endif /* _O_IDU_OID_MEMORY_H_ */






