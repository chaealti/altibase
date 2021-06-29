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
#include <iduHeapSort.h>


/*----------------------------------------------------------------------
  task-2457 heap sort�� �����մϴ�.

  Description  : �׻� ����ð� O(NlnN)�� ������ heap sort�Դϴ�. �⺻������ c����
  �����ϴ� qsort�� ���� �������̽��� �����ϴ�.
  �ӵ��� qsort�� ���ؼ� 2������ �����ϴ�. heap sort�� ������
  �޸� ������ �䱸���� �ʴ´ٴ� ���� qsort���� �����Դϴ�.
                 
  aArray       -   [IN/OUT]  ������ ��Ű�� ���ϴ� �迭,
  �� �Լ� ������, �� �迭�� ���ĵǾ� �ִ�.
  aArrayNum    -   [IN]      �迭�� ������ �� ����
  aDataSize    -   [IN]      �迭�� �� ������ ũ��(byte����)
  aCompar      -   [IN]      ������ �����ϱ� ���ؼ� ���� �� �� �� �־�� �Ѵ�.
  
  �� ���Ҹ� �޾Ƽ� �񱳸� �����ϴ� �Լ�.
  �������� = �տ����Ұ� Ŭ��� return 1,���� ��� return 0,
  ���� ���Ұ� �� Ŭ��� return -1
  �������� = �տ����Ұ� Ŭ��� return -1, ���� ��� return 0,
  ���� ���Ұ� �� Ŭ��� return 1
  ----------------------------------------------------------------------*/
void iduHeapSort::sort( void  *aArray,
                        UInt aArrayNum,
                        UInt aDataSize,
                        SInt (*aCompar)(const void *, const void *))
{
    SChar *sFirstNode, *sLastNode;
    
    UInt sNodeIdx;

    IDE_ASSERT( aDataSize > 0);
    IDE_ASSERT( aArrayNum > 0);

    if (aArrayNum != 1)
    {
        /* �� �Լ� ������ item�� 1���� �����Ѵٰ� �����ϹǷ�, �����͸� ��ĭ
         * �ڷ� ������. ��, c������ aArray[0]�� ù��°�� ����Ű���� �Ʒ�
         * ������ �����ϸ�, aArray[1]�� ù��°�� ����Ű�� �ȴ�.
         * 1���� �����ϴ� ������ �ڽ� ��带 ���Ҷ�, 0���� �����ϸ� �ڽĳ�尡 0��
         * ������ �����̴�.*/
        aArray = (void*)((SChar*)aArray - aDataSize);

        /*  �ڽ��� ������ �ּ� ��� ���� ��Ʈ���� �ö󰡸鼭 �� ��忡 ����
         *  MAX_HEAPIFY�� �����Ѵ�.
         *  
         *  ������ ���� �ϴ� ���� =>
         *  1. MAX_HEAPIFY�� �����ϱ� ���� ���ǿ� left child ���� Ʈ����
         *     right child ����Ʈ���� ��Ʈ������ �Ѵٴ� ������ �ִ�.
         *  2. MAX_HEAPIFY�� �����ϰ� ���� ��Ʈ�� ������ �����Ѵ�.
         *  3. � sNodeIdx�� ���� MAX_HEAPIFY�� �����Ҷ�,
         *     �� left child�� right child�� �̹� MAX_HEAPIFY��
         *     �����ϰ� �� ���̹Ƿ� ��Ʈ�� �� ���̹Ƿ� MAX_HEAPIFY��
         *     ������ �� �ִ�.
         *  4. sNodeIdx�� 1�϶��� ���� ����������, �̰��� ��ü Ʈ���� ��Ʈ�̴�.
         *     �׷��Ƿ� �̰��� ������, ��ü Ʈ���� ��Ʈ���� ���� ������ �� �ִ�.
         */
        for (sNodeIdx = aArrayNum / 2; sNodeIdx != 0 ; --sNodeIdx)
        {
            IDU_HEAPSORT_MAX_HEAPIFY(sNodeIdx, aArray, aArrayNum, aDataSize, aCompar);
        }
    

        /*
         * ��Ʈ���� ���� ������ �����ϴ� �κ�, aArray[1], �� Ʈ���� ��Ʈ��
         * ���� ���� Ʈ��(�迭,aArray)������ ���� ū ���� ������ �ִٴ� ����
         * �̿��Ͽ� ������ �Ѵ�.
         */
        while (aArrayNum > 1)
        {
            /*Ʈ���� ��Ʈ, ���� Ʈ�������� ���� ū ���� ����*/
            sFirstNode = IDU_HEAPSORT_GET_NTH_DATA(1, aArray, aDataSize);
        
            /*Ʈ���� ������ ���*/
            sLastNode  = IDU_HEAPSORT_GET_NTH_DATA(aArrayNum,aArray, aDataSize);

            /*���� Ʈ������ ���� ū ���� �迭�� ���� ���������� ����.*/
            IDU_HEAPSORT_SWAP(sFirstNode, sLastNode, aDataSize);

            /*�迭�� ���������� �� ���� ���̻� Ʈ������ ���Ұ� �ƴϴ�.*/
            --aArrayNum;
        
            /*���� Ʈ���� ��Ʈ, �� aArray[1]���� ���̻� ���� ū ���� �ƴϴ�.  ������
             *�̰��� left child�� rigth child�� ��� ��Ʈ���̹Ƿ� ��Ʈ�� ���ؼ���
             *MAX_HEAPIFY�� �����ϸ�, ��ü Ʈ���� ��Ʈ���� �ȴ�.*/
            IDU_HEAPSORT_MAX_HEAPIFY(1, aArray, aArrayNum, aDataSize, aCompar);
        
            /*�̷��� �ൿ�� Ʈ���� ũ�Ⱑ 1�� �ɶ����� �����Ѵ�. �׷��� �ᱹ ����
             * ���� ������ ū ������ ���ĵ� ���� ���� �� �ִ�.*/
        }
    }    
}


