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
 

#include <idu.h>
#include <iduPriorityQueue.h>

/*------------------------------------------------------------------------------
  iduPriorityQueue�� �ʱ�ȭ �Ѵ�.

  aIndex        - [IN]  Memory Mgr Index
  aDataMaxCnt   - [IN]  iduHeap�� ���� �� �ִ� Data�� �ִ� ����
  aDataSize     - [IN]  Data�� ũ��, byte����
  aCompar       - [IN]  Data���� ���ϱ� ���� ����ϴ� Function, 2���� data�� ���ڷ� �޾�
                        ���� ���Ѵ�. �� �Լ��� �����ϰ� ����ϸ� iduHeap��
                        �ִ�켱���� ť �Ǵ� �ּҿ켱���� ť�� ����� �� �ִ�.
         �ִ� �켱����ť = ū���� ���� �����Ѵ�. �̰��� ���ؼ�,
                        aCompar�Լ������� ù��° ���ڰ� �� Ŭ�� 1�� ����, ������ 0�� ����,
                        �ι�° ���ڰ� �� Ŭ�� -1�� �����ϸ� �ȴ�.
         �ּ� �켱����ť = �������� ���� �����Ѵ�. �̰��� ���ؼ�,
                        aCompar�Լ������� ù��° ���ڰ� �� ������ 1�� ����, ������ 0�� ����,
                        �ι�° ���ڰ� �� ������ -1�� �����ϸ� �ȴ�.
  -----------------------------------------------------------------------------*/
IDE_RC iduPriorityQueue::initialize( iduMemoryClientIndex aIndex,
                                     UInt                 aDataMaxCnt,
                                     UInt                 aDataSize,
                                     SInt (*aCompar)(const void *, const void *))
{
    return mHeap.initialize(aIndex, aDataMaxCnt, aDataSize, aCompar);
}

IDE_RC iduPriorityQueue::initialize( iduMemory          * aMemory,
                                     UInt                 aDataMaxCnt,
                                     UInt                 aDataSize,
                                     SInt (*aCompar)(const void *, const void *))
{
    return mHeap.initialize(aMemory, aDataMaxCnt, aDataSize, aCompar);
}
