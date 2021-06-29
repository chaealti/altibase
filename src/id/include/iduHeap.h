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
 

#ifndef _O_IDU_HEAP_H_
#define _O_IDU_HEAP_H_ 1

#include <idl.h>
#include <iduMemMgr.h>
#include <iduMemory.h>
#include <iduHeapSort.h>

/*-----------------------------------------------------------------
 TASK-2457 heap sort�� �����մϴ�.

 heap�� insert�� remove, �׸��� aCompar����.

 heap�� insert�� ���� aDataSize��ŭ memcpy�ؼ� �����Ѵ�.
 �̶�, ������ ���� ������ ���� �ʱ� ������ void*�� ���� �Է� �޴´�.
 
 ��, int���� insert�� ��쿡��
 -----------------------------------
 compar(const void *a, const void *b)
 {
 //���ο��� �迭 ������ ������ �� �� ����. �׷��� a�� (int*)�� ĳ���� �ؼ�
 //������ �ؾ� �Ѵ�.
    return *((int*)a) - *((int*)b); 
 }
 
 heap.initialize( ..., aDataSize = ID_SIZEOF(int), ..); ũ�⸦ ����
 int a = 10;
 heap.insert((void*) &a); �̷������� ������ �����͸� �ǳ� �־�� �Ѵ�.
 heap.remove((void*) &a); a�� ���� �����Ѵ�.


 
 char ���� insert�� ��쿡��
 -----------------------------------
 compar(const void *a, const void *b)
 {
 //���ο��� �迭 ������ ������ �� �� ����. �׷��� a�� (char*)�� ĳ���� �ؼ�
 //������ �ؾ� �Ѵ�.
    return *((char*)a) - *((char*)b); 
 }

 heap.initialize( ..., aDataSize = ID_SIZEOF(char), ..); ũ�⸦ ����
 char a = 10;
 heap.insert((void*) &a); �̷������� ������ �����͸� �ǳ� �־�� �Ѵ�.
 heap.remove((void*) &a); a�� ���� �����Ѵ�.


 
 ����ü�� insert�� ��쿡��
 -----------------------------------
 compar(const void *a, const void *b)
 {
 //���ο��� �迭 ������ ������ �� �� ����. �׷��� a�� (����ü*)�� ĳ���� �ؼ�
 //������ �ؾ� �Ѵ�.
    return ((����ü*)a)->num - ((����ü*)b)->num; 
 }
 
 heap.initialize( ..., aDataSize = ID_SIZEOF(����ü), ..); ũ�⸦ ����
 ����ü a.num = 10;
 heap.insert((void*) &a); �̷������� ������ �����͸� �ǳ� �־�� �Ѵ�.
 heap.remove((void*) &a); a�� ���� �����Ѵ�.


 

 ���� �����͸� insert�ϰ��� �� ��쿡��
 -----------------------------------
 compar(const void *a, const void *b)
 {
 //���ο��� �迭 ������ ������ �� �� ����. �׷��� a�� (����ü**)�� ĳ���� �ؼ�
 //������ �ؾ� �Ѵ�.
 //���⼭ ������ ó�� (����ü*)�� ĳ������ �ϸ� �ȵȴ�. �ֳ��ϸ� ���ο���
 //�迭������ ������ (����ü*)�̱� ������, �̰��� �ٽ� reference�ؾ� ���� ����ü
 //�� ������ �ȴ�. �׷��� ������ (����ü**)���� ĳ�����Ѵ�.
    return (*(����ü**)a)->num - (*(����ü**)b)->num; 
 }
 
  heap.initialize( ..., aDataSize = ID_SIZEOF(����ü *), ..);  ũ�⸦ ����
 ����ü a = malloc(ID_SIZEOF(����ü));
 a->num = 10;
 heap.insert((void*) &a); �̷������� ������ �����͸� �ǳ� �־�� �Ѵ�.
 heap.remove((void*) &a); a�� ���� �����Ѵ�.
 free(a); a���� malloc�Ҷ��� �� �ּҰ��� ����ֱ� ������ a�� �ٷ� free�� �� �ִ�.

 -----------------------------------------------------------------*/
class iduHeap{
public:
    iduHeap(){};
    ~iduHeap(){};
    
    IDE_RC  initialize ( iduMemoryClientIndex aIndex,
                         UInt                 aDataMaxCnt,
                         UInt                 aDataSize,
                         SInt (*aCompar)(const void *, const void *));
    
    IDE_RC  initialize ( iduMemory          * aMemory,
                         UInt                 aDataMaxCnt,
                         UInt                 aDataSize,
                         SInt (*aCompar)(const void *, const void *));
    
    IDE_RC  destroy();

    //���� ���ο� ���Ҹ� �ִ´�.
    void    insert (void *aData, idBool *aOverflow);
    
    //������ ���� ū ���Ҹ� �����Ѵ�.
    void    remove (void *aData, idBool *aUnderflow);

    //���� ���� ���Ҹ� ��� �����Ѵ�.
    void    empty(){ mDataCnt = 0;};

    UInt    getDataMaxCnt() { return mDataMaxCnt;   };
    UInt    getDataCnt()    { return mDataCnt;      };

private:
    void    maxHeapInsert( void  *aKey);
    

    iduMemoryClientIndex    mIndex;
    void                   *mArray;
    UInt                    mDataMaxCnt;
    UInt                    mDataCnt;
    UInt                    mDataSize;
    
    //����ڰ� ������ �� �Լ�, ���� ����� mCompar������ ���ȴ�.
    SInt                  (*mComparUser)  (const void *, const void *);
    
    //���� �����ο��� ����ϴ� ����� ����ڰ� ���ϴ� �Ͱ� �ݴ��̱�
    //������ �̰��� ���߾� �ֱ� ���ؼ� ����Ѵ�.
    SInt                   mCompar(const void *a,const void *b){ return (-1)*mComparUser(a,b);};
};

#endif   // _O_IDU_HEAP_H_

