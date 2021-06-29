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
 

#ifndef _O_IDU_HEAP_SORT_H_
#define _O_IDU_HEAP_SORT_H_ 1

/*------------------------------------------------------------------------------
 * TASK-2457 heap sort�� �����մϴ�.
 * 
 * heap sort�� �ڼ��� ������ INTRODUCTION TO ALGORITHMS 2���� 6���� ���� �ٶ���.
 *
 *
 * �Է¹��� �迭�� ��Ʈ���� ���� ��
 * ---------------------------------
 * 1.������ �Ǿ����� ���� �迭�� �Է����� �޴´�.
 * 
 * 2.�迭�� ����2�� Ʈ���� ����. (���峷�� ���� ����, ���� ���� ���� ���ʺ��� ���ִ�.)
 * 
 * 3.�迭�� ���� ó�� ���Ҹ� ��ü ��Ʈ���� root�� �Ѵ�.
 * 
 * 4.left child�� '���� ������ �ε���(�迭���� ����)'*2 �� ���Ѵ�.
 * 
 * 5.right child�� '���� ������ �ε���(�迭���� ����)'*2 +1�� ���Ѵ�.
 * 
 * 6.��,root�� �ε��� 1�϶�(������ 0������ �������� 1�� �ٲ۴�) root�� left child��
 *    2��° �ε����� ���� ����, rigth child�� 3��° �ε����� ���� ���Ұ� �ǰ�,
 *    �� ���� ���� ���� Ʈ���� root�� �Ǿ� �ڽ��� left child�� right child��
 *    ���ϰ� �ȴ�.
 *
 *
 * ���� Ư��
 * ----------------------------------
 * 1. ���� Ư�� = �ڽ� ���� ����� �θ� ��庸�� ū ���� ���� �� ����.
 * 
 * 2. ���� Ư���� �����Ѵٸ� �迭�� 1��° ���Ҵ� ��� ���� �θ��̱� ������ ����
 *      ū ���� ������ �ȴ�.
 *
 *
 * 
 * ���� Ư���� ���� ��Ű�� ��
 * ----------------------------------
 * 1. MAX_HEAPIFY(i)
 * 
 *   ����: i��带 root���ϴ� ��Ʈ������ ���� left child(i)�� right child(i)���
 *        ���� Ư���� �����Ѵ�. ��, left child(i)�� ��Ʈ�� �ϴ� ��Ʈ����
 *        right child(i)�� ��Ʈ�� �ϴ� ��Ʈ�� ��� ���� Ư���� �����Ѵ�.
 *        
 *     a. i��尡 left child(i)���� ũ��, right child(i)���� ũ�ٸ� ���� Ư����
 *        �����ϹǷ� �� �Լ��� �ƹ��� �ϵ� ���� �ʴ´�.
 *        
 *     b. i��尡 child���� �۴ٸ�, left�� right �� �� ū child node�� SWAP�Ѵ�.
 *        ��, ��ġ�� �����Ѵ�. ���� ��� left�� right���� Ŀ�� left�� i(root)��
 *        ������ �ߴ�. �׷���, ����� i�� �ڽĵ� ���� ũ�Ƿ� ���� Ư���� �����Ѵ�.
 *        ������ ����� left�� �ڽ��� ���� Ʈ���� ���� ���� Ư���� �������� �ʴ´�.
 *        �׷��� ������ left�� ���ؼ� MAX_HEAPIFY�� ��������� �����Ѵ�.
 *        �� ������ leaf�� ������ ������ �����Ѵ�.
 *        (���������δ� ����������� �����δ� �׷��� �ʴ�. �ֳ��ϸ�, MAX_HEAPIFY��
 *        �ſ� ���� ȣ��Ǵ� �ٽɺκ����� ��������� ���� ��ũ�η� ���� �Ͽ���.
 *        ��ũ���Լ��δ� recusive�� ������ �� ���� ������ ������ ������ �Ͽ���.)
 *          
 *     c. ������ ������Ű�� ���� leaf���� ���� �� ��忡 ���� MAX_HEAPIFY�� �����Ѵ�.
 *        ������, leaf node�� ��쿡�� �ڽ��� ���� ���� ������ ������ �����Ѵ�.
 *        �׷��Ƿ� leaf ��忡 ���ؼ��� MAX_HEAPIFY�� �������� �ʴ´�.
 *
 *        ��, �ڽ��� �����ϴ� ��常 MAX_HEAPIFY�� �����Ѵ�.
 *        �ڽ��� �����ϴ� ���� ������ ��� = ������ ����� �θ��� = array_size / 2
 *
 *        ��, for( i = array_size/2; i != 0; i-- )
 *               MAX_HEAPIFY(i);
 *
 *     d. ��Ʈ ��忡 ���� MAX_HEAPIFY�� �����ϰ� ���� ��ü array�� ���� Ư����
 *        �����ϰ� �ȴ�.
 *
 *
 *     
 * �����ϴ� ��
 * ---------------------------------
 * 1. array[1]���� �׻� ���� ū ���� ����ִ�. �̰��� array�� ���� ��������
 *    ��ȯ(SWAP)�Ѵ�.�׷���, array[1]�� ���� ���� ����, array�� ��������
 *    ���� ū���� ����. �׷���, array�� ũ�⸦ ���δ�. �� ���� ū����
 *    ���� heap���� ���ܵȴ�.
 *    
 * 2. ���� ��Ʈ�� ���ؼ��� MAX_HEAPIFY�� �����Ѵ�. �ֳ��ϸ�, ���� MAX_HEAPIFY��
 *    ������ ���� ������ ������Ű�� ������, ���� ��Ʈ�� ���ؼ��� �����ϸ�
 *    ��ü array�� ���� Ư���� �����Ѵ�.
 *    
 * 3. �̰��� ��� �����Ѵ�. ��,
 *    for( i = array_length; i > 1 ; i--)
 *        SWAP( array[1], array[array_length]);
 *        array_length -= 1;
 *        MAX_HEAPIFY(1);
 *        
 * 4. ������ �������, ���� ���� �� ���� ū ������ ���ĵǾ��ִ�.
 *
 *------------------------------------------------------------------------------*/
#define GET_LEFT_CHILD_IDX(i)           ((i)<<1)
#define GET_PARENT_IDX(i)               ((i)>>1)

/*-----------------------------------------------------------------------------
  IDU_HEAPSORT_GET_NTH_DATA
 
 aArray���� aNodeIdx��° data�� �����Ѵ�. ���� ������ �迭�� �ٸ��� 1���� �����Ѵ�.
 idx            - [IN]      ���ϰ��� �ϴ� ������ index
 aArray         - [IN]      �迭
 aDataSize      - [IN]      ������ ũ��
 -----------------------------------------------------------------------------*/
#define IDU_HEAPSORT_GET_NTH_DATA(idx,aArray,aDataSize) \
    ((SChar*)aArray + (aDataSize * (idx)))


//��ũ�� swap�̴�. �� ��ũ�� ����Ŀ� �� �Լ��� ���ڷ�
//�Ѱ��� a�� b�� ���� ���ϹǷ� ������ ó���ؾ� �Ѵ�.
#define	IDU_HEAPSORT_SWAP(a, b,aDataSize) { \
    UInt  sCnt;                             \
    SChar sTmp;                             \
    sCnt = aDataSize;                       \
    do {                                    \
	sTmp = *a;                          \
	*a++ = *b;                          \
	*b++ = sTmp;                        \
    } while (--sCnt);                       \
}


/*------------------------------------------------------------------------------
  �迭 aArray�� ����2�� Ʈ���� ����, aNodeIdx�� ��Ʈ�� �ϴ� ���� Ʈ���� ����
  Ư���� �����ϴ� ��Ʈ���� �����Ų��.
  �̶�, aNodeIdx�� ��Ʈ�� �ϴ� ���� Ʈ������ aNodeIdx�� left child��
  ��Ʈ�� �ϴ� ���� Ʈ���� rigth child�� ��Ʈ�� �ϴ� ����Ʈ���� �ݵ��
  ���� Ư���� �����ϴ� ��Ʈ������ �Ѵ�.
  
  aSubRoot  - [IN]      aArray������ �����Ʈ�� �ε�����.
  
  aArray    - [IN/OUT]  ��Ʈ��, �� �Լ��� �����ϱ����� aArray�� ���� Ư����
                        �����Ѵٸ�, �� �Լ��� ������ �Ŀ��� aArray��
                        ���� Ư���� �����Ѵ�.
                           
  aArrayNum - [IN]      ���� ��Ʈ���� ũ��, �̰��� ���� aArray�� ��ü ũ�Ⱑ
                        �ƴϴ�. ���� aArray��Ʈ���� ������ ������ ���Ѵ�.
                        �� ���� maxHeapInsert �Լ��� ���������� ���� 1�� �����Ѵ�.
                           
  aDataSize - [IN]      �迭�� �� ������ ũ��(byte����)
  
  aCompar   - [IN]      ���� ����µ� �־� void������ ĳ�����ؼ� ���ϴ� �Լ��� �ʿ��ϴ�.
                            
  �θ� �ڽĺ��� �� ũ�⸦ ���Ҷ� = �տ����Ұ� Ŭ��� return 1,
  ���� ��� return 0, ���� ���Ұ� �� Ŭ��� return -1
  �θ� �ڽĺ��� �� �۱⸦ ���Ҷ� =  �տ����Ұ� Ŭ��� return -1,
  ���� ��� return 0, ���� ���Ұ� �� Ŭ��� return 1
                                        
  ------------------------------------------------------------------------------*/
#define	IDU_HEAPSORT_MAX_HEAPIFY(aSubRoot, aArray, aArrayNum, aDataSize, aCompar) \
{                                                                               \
    UInt   i,j;                                                                 \
    SChar *sParent, *sChild;                                                    \
    for (i = aSubRoot; (j = GET_LEFT_CHILD_IDX(i)) <= aArrayNum; i = j)         \
    {                                                                           \
	sChild = (SChar *)aArray + j * aDataSize;/*get left child*/             \
                                                                                \
        /*left�� right child�߿��� �� ū child�� sChild������ ����*/                 \
	if ((j < aArrayNum)  /*j(left)�� �̰��� �����ϸ� right���� �����Ѵ�.*/          \
          &&(aCompar(sChild/*left child*/, sChild + aDataSize/*Right Child*/ ))< 0)\
        {                                                                       \
            /*right�� �����ϰ� right�� �� ū��� sChild�� right�� ����*/              \
            sChild += aDataSize;                                                \
            ++j;                                                                \
	}                                                                       \
                                                                                \
	sParent = (SChar *)aArray + i * aDataSize;                              \
                                                                                \
	if (aCompar(sChild, sParent) <= 0)                                      \
        {                                                                       \
            /*�θ� ��尡 �� child���� ũ�ٸ� ���� Ư�� �����ϹǷ� �ƹ��� �ൿ�� ��������*/ \
            break;                                                              \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            /*�׷��� �ʴٸ�, �θ� ���� child�� ū ���� ��ġ ����*/                  \
            IDU_HEAPSORT_SWAP(sParent, sChild, aDataSize);                      \
        }                                                                       \
    }                                                                           \
}


/*------------------------------------------------------------------------
  iduHeapSort class
  -----------------------------------------------------------------------*/
class iduHeapSort{
public:
    static void sort( void  *aArray,
                      UInt aArrayNum,
                      UInt aDataSize,
                      SInt (*aCompar)(const void *, const void *));
};


#endif   // _O_IDU_HEAP_H
