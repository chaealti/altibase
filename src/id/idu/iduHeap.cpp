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
 
#include <idl.h>
#include <ide.h>
#include <iduHeap.h>
#include <iduHeapSort.h>

/*-------------------------------------------------------------------------
 * TASK-2457 heap sort�� �����մϴ�.
 * 
 * �Ʒ��� ����� ��ũ�� �Լ��� iduHeapSort.h�� ���ǵ� ��ũ�� �Լ��� �״�� ���
 * �ϰ� �ִ�. �Լ��� ���� �ڼ��� ����� Heap�� ���� �ڼ��� ������
 * iduHeapSort.h�� ����Ǿ� �ִ�. ���⼭�� ������ ������ �Ѵ�.
 * ----------------------------------------------------------------------*/
//heap array������ n��° ���Ҹ� �����Ѵ�.  ���Ҵ� 1���� �����Ѵ�.
#define IDU_HEAP_GET_NTH_DATA(idx) \
    IDU_HEAPSORT_GET_NTH_DATA( idx, mArray, mDataSize)

//�� ���Ҹ� ���� ��ȯ�Ѵ�.
#define IDU_HEAP_SWAP(a,b)      IDU_HEAPSORT_SWAP(a,b,mDataSize)

//'aSubRoot��  �� child'�� ��Ʈ�� �ϴ� �� ����Ʈ���� ��� heapƮ���� Ư����
//�����Ҷ�,�� ��ũ�� �����ϸ� aSubRoot�� ��Ʈ�� �ϴ� Ʈ���� heapƮ���� Ư����
//�����Ѵ�.
#define IDU_HEAP_MAX_HEAPIFY(aSubRoot) \
    IDU_HEAPSORT_MAX_HEAPIFY(aSubRoot, mArray, mDataCnt, mDataSize, mCompar)



/*------------------------------------------------------------------------------
  iduHeap�� �ʱ�ȭ �Ѵ�.

  aIndex        - [IN]  Memory Mgr Index
  aDataMaxCnt   - [IN]  iduHeap�� ���� �� �ִ� Data�� �ִ� ����
  aDataSize     - [IN]  Data�� ũ��, byte����
  aCompar       - [IN]  Data���� ���ϱ� ���� ����ϴ� Function, 2���� data��
                        ���ڷ� �޾� ���� ���Ѵ�. �� �Լ��� �����ϰ� ����ϸ�
                        iduHeap�� �ִ�켱����ť �Ǵ� �ּҿ켱����ť�� ����� �� �ִ�.
                        
    ���� ū����   root =  aCompar�Լ������� ù��° ���ڰ� �� Ŭ�� 1�� ����,
                       ������ 0�� ����, �ι�° ���ڰ� �� Ŭ�� -1�� �����ϸ� �ȴ�.
    ���� �������� root  = aCompar�Լ������� ù��° ���ڰ� �� ������ 1�� ����,
                       ������ 0�� ����, �ι�° ���ڰ� �� ������ -1�� �����ϸ� �ȴ�.
  -----------------------------------------------------------------------------*/
IDE_RC iduHeap::initialize( iduMemoryClientIndex aIndex,
                            UInt                 aDataMaxCnt,
                            UInt                 aDataSize,
                            SInt (*aCompar)(const void *, const void *))
{
    mIndex      = aIndex;
    mDataMaxCnt = aDataMaxCnt;
    mDataSize   = aDataSize;
    mComparUser = aCompar;
    mDataCnt    = 0;
    mArray      = NULL;


    IDE_TEST(iduMemMgr::malloc( mIndex,
                                mDataSize * mDataMaxCnt,
                                (void**)&mArray)
             != IDE_SUCCESS);
    
    //heap�� 1���� �ε����� �����Ѵ�. �ֳ��ϸ�, root�� 0�̶�� child�� ���Ҽ� ���� �����̴�.
    mArray = (void*)((SChar*)mArray - aDataSize);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iduHeap::initialize( iduMemory          * aMemory,
                            UInt                 aDataMaxCnt,
                            UInt                 aDataSize,
                            SInt (*aCompar)(const void *, const void *))
{
    mIndex      = IDU_MAX_CLIENT_COUNT;
    mDataMaxCnt = aDataMaxCnt;
    mDataSize   = aDataSize;
    mComparUser = aCompar;
    mDataCnt    = 0;
    mArray      = NULL;


    IDE_TEST(aMemory->alloc( mDataSize * mDataMaxCnt,
                             (void**)&mArray)
             != IDE_SUCCESS);
    
    //heap�� 1���� �ε����� �����Ѵ�. �ֳ��ϸ�, root�� 0�̶�� child�� ���Ҽ� ���� �����̴�.
    mArray = (void*)((SChar*)mArray - aDataSize);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

IDE_RC iduHeap::destroy()
{
    if ( mIndex < IDU_MAX_CLIENT_COUNT )
    {
        IDE_TEST(iduMemMgr::free( (void*)((SChar*)mArray + mDataSize) )
                 != IDE_SUCCESS);
    }
    else
    {
        // iduMemory�� free�� �ʿ䰡 ����.
    }
    
    mArray  = NULL;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*------------------------------------------------------------------------------
  iduHeap�� ���� �����Ѵ�.

  aData         - [IN]  ���� ���� �����ؼ� �����Ѵ�.
  aOverflow     - [OUT] ���� overflow�Ǿ��ٸ� ID_TRUE, �׷��� �ʴٸ� ID_FALSE
                        �� �����Ѵ�.
  -----------------------------------------------------------------------------*/
void iduHeap::insert(void *aData, idBool *aOverflow)
{
    if( mDataCnt > mDataMaxCnt)
    {
        *aOverflow = ID_TRUE;
    }
    else
    {
        *aOverflow = ID_FALSE;
        maxHeapInsert(aData);
    }
}


/*------------------------------------------------------------------------------
  iduHeap�� ���� ū ���Ҹ� �����Ѵ�. root�� ���� ū ���� ������ �����Ƿ� root�� �����Ѵ�.

  aData         - [OUT] ���� ū ������ ���� �������Ŀ� �����Ѵ�.
  aUnderflow    - [OUT] ���� ���Ұ� ���̻� ���� ��� ID_TRUE�� �����Ѵ�.
                        �׷��� ���� ��� ID_FALSE�� �����Ѵ�.
  -----------------------------------------------------------------------------*/
void iduHeap::remove (void *aData, idBool *aUnderflow)
{
    if( mDataCnt == 0)
    {
        *aUnderflow = ID_TRUE;
    }
    else
    {
        *aUnderflow = ID_FALSE;

        //ù��° ���Ұ� ���� ū ���� ������ �����Ƿ� �̰��� ���ϰ��� �����Ѵ�.
        idlOS::memcpy( aData,
                       IDU_HEAP_GET_NTH_DATA(1),
                       mDataSize);
    
        //���� �������� �ִ� ���� ���� ������ �����Ѵ�. �̷��� �Ǹ� mArray�� ��Ʈ��
        //���� Ư���� �������� ���ϰ� �ȴ�.
        if( mDataCnt != 1 )
        {
            idlOS::memcpy( IDU_HEAP_GET_NTH_DATA(1),
                           IDU_HEAP_GET_NTH_DATA(mDataCnt),
                           mDataSize);
        }
        
        //��Ʈ�� ������ ������ data�� ��� ���� Ư���� �����ϹǷ�, ��Ʈ�� ���ؼ���
        //maxHeapify�� �������ָ� �ȴ�.
        IDU_HEAP_MAX_HEAPIFY(1);
    
        //mArray���� �ϳ��� ���Ҹ� ���������Ƿ� mDataCnt�� 1 ���δ�.
        mDataCnt--;
    }
}



/*------------------------------------------------------------------------------
  ���� Ư���� �����ϴ� �迭 aArray�� ����, ���ο� ���Ҹ� �߰��Ѵ�. 
  �� �Լ��� ȣ���ϱ� ���� ���Ѿ� �� ������
  1. aArray�� ��Ʈ���� Ư���� �ݵ�� �����ؾ� �Ѵ�.
 
  �� �Լ��� ������,
  1. aArray�� ��Ʈ���� Ư���� �����Ѵ�.
  2. aKey�� ���ο� ���ҷ� aArray���� �� �ִ�.
 
                                   
  aKey         - [IN]      �Է��� ������ ��
  ------------------------------------------------------------------------------*/
void iduHeap::maxHeapInsert(void *aKey)
{
    UInt    i;
    UInt    sParentIdx;
    SChar  *sNode       = NULL;
    SChar  *sParentNode = NULL;

    //mDataCnt�� ����� mDataMaxCnt�� �Ѿ�� �ȵȴ�.
    IDE_ASSERT( mDataCnt < mDataMaxCnt);
    
    mDataCnt++;
    
    idlOS::memcpy( IDU_HEAP_GET_NTH_DATA(mDataCnt),
                   aKey,
                   mDataSize);
    
    for( i = mDataCnt; i > 1; i = sParentIdx)
    {
        sNode       = IDU_HEAP_GET_NTH_DATA(i);
        sParentIdx  = GET_PARENT_IDX(i);
        sParentNode = IDU_HEAP_GET_NTH_DATA(sParentIdx);
        
        if( mCompar( sParentNode, sNode) >= 0)
        {
            break;
        }
        
        IDU_HEAP_SWAP(sParentNode, sNode);
    }
}
    



