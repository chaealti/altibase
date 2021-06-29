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
 * $Id: iduGrowingMemoryHandle.h 15368 2006-03-23 01:14:43Z leekmo $
 **********************************************************************/

#ifndef _O_IDU_GROWING_MEMORY_HANDLE_H_
#define _O_IDU_GROWING_MEMORY_HANDLE_H_ 1

#include <idl.h>
#include <iduMemory.h>
#include <iduMemoryHandle.h>

/*
   Growing Memory Handle : 

   ���� ������ �޸𸮸� ������ ���� �Ұ����� ���
   ����ϴ� Memory Handle
   
   �뵵 :
      <prepare�Լ��� ���� ���޹��� �޸𸮸� ��Ȱ���ؼ��� �ȵǴ� ���>
      
      1. SM���� Redo�� �������� ���۷� ���.

         Disk Log�� Redo�ÿ� I/O�� �ּ�ȭ �ϱ� ���ؼ� ������ ����
         ����� ����Ѵ�.
         
         �ϳ��� Disk Log�� ������ ��� Buffer�� �ݿ�����
         �ʰ�, Page ID�� ������� ������ Hash Table�� �ݿ��� �α���
         ������ �Ŵ޾Ƶξ��ٰ� �̷��� �α��� ������ ������� ���̸�
         Page���� Buffer�� �α׸� Redo�Ѵ�.

         �׷���, �̶� Page���� �޾Ƶ� �αװ� ����� ���� �ƴ϶�,
         �α׷��ڵ� �ּҸ� ���� �������ϵ��� ����Ǿ� �ִ�.
         (���������� �α��� ���, �������� ���۸� ���� ����Ų��.)

         ���� redo�� �α��� ���� ������ ���� �Ҵ�� �����������۰�
         ����ȴٸ�, Disk Log�� Page���� �޾Ƶ� �α��� �����Ͱ�
         ������ �����͸� ����Ű�� �Ǿ� �̷��� ����� ������ ���� ����.

         �̷��� ���, Growing Memory Handle�� �̿��Ͽ� ���������� �����
         �α��� �޸��ּҸ� ��Ȱ������ �ʰ�, �׻� ���� �Ҵ��ϵ��� �Ѵ�.
         
   ���� :
         iduMemory�� �̿��Ͽ� Chunk������ �޸𸮸� �Ҵ��ϵ��� �Ѵ�.
         
*/
class iduGrowingMemoryHandle : public iduMemoryHandle
{
public :

    // ��ü ������,�ı��� => �ƹ��ϵ� �������� �ʴ´�.
    iduGrowingMemoryHandle();
    virtual ~iduGrowingMemoryHandle();
    
    
    /*
       Growing Memory Handle�� �ʱ�ȭ �Ѵ�.
     */
    IDE_RC initialize( iduMemoryClientIndex   aMemoryClient,
                       ULong                  aChunkSize );
    
    /*
       Growing Memory Handle�� �ı� �Ѵ�.
     */
    IDE_RC destroy();
    
    /*
       Growing Memory Handle�� aSize�̻��� �޸𸮸� �Ҵ��Ѵ�.

       �׻� ���ο� ������ �޸𸮸� �Ҵ��Ѵ�.
     */
    virtual IDE_RC prepareMemory( UInt    aSize,
                                  void ** aPreparedMemory);

    // �� Memory Handle�� ���� �Ҵ���� �޸��� �ѷ��� ����
    virtual ULong getSize( void );

    // Growing Memory Handle�� �Ҵ��� �޸𸮸� ���� ����
    IDE_RC clear();
    
private:
    /* Chunk������ OS�� �޸𸮸� �Ҵ��Ͽ� ���� ũ��� �ɰ���
       �޸𸮸� �Ҵ��� �� �޸� ������ */
    iduMemory  mAllocator;

    // prepareMemory�� ȣ���Ͽ� �Ҵ�޾ư� �޸� ũ���� ����
    ULong mTotalPreparedSize;
    
};

    


#endif /*  _O_IDU_GROWING_MEMORY_HANDLE_H_ */
