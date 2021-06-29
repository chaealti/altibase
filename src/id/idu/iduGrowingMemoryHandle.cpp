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
 * $Id: iduGrowingMemoryHandle.cpp 15368 2006-03-23 01:14:43Z leekmo $
 **********************************************************************/

#include <idl.h>
#include <iduMemMgr.h>
#include <iduGrowingMemoryHandle.h>


// ��ü ������,�ı��� => �ƹ��ϵ� �������� �ʴ´�.
iduGrowingMemoryHandle::iduGrowingMemoryHandle()
{
}
iduGrowingMemoryHandle::~iduGrowingMemoryHandle()
{
}

/*
  Growing Memory Handle�� �ʱ�ȭ �Ѵ�.

  [IN] aMemoryClient - iduMemMgr�� �ѱ� �޸� �Ҵ� Client
  [IN] aChunkSize - �޸� �Ҵ��� ������ Chunk�� ũ��
*/
IDE_RC iduGrowingMemoryHandle::initialize(
           iduMemoryClientIndex   aMemoryClient,
           ULong                  aChunkSize )
{
    /* ��ü�� virtual function table�ʱ�ȭ ���ؼ� new�� ȣ�� */
    new (this) iduGrowingMemoryHandle();
    
#ifdef __CSURF__
    IDE_ASSERT( this != NULL );
#endif

    /* �޸� �Ҵ�� �ʱ�ȭ */
    mAllocator.init( aMemoryClient, aChunkSize);
    
    // prepareMemory�� ȣ���Ͽ� �Ҵ�޾ư� �޸� ũ���� ����
    mTotalPreparedSize = 0;
        
    return IDE_SUCCESS;
}
    
/*
  Growing Memory Handle�� �ı� �Ѵ�.
  
  [IN] aHandle - �ı��� Memory Handle
*/
IDE_RC iduGrowingMemoryHandle::destroy()
{
    /* �޸� �Ҵ�Ⱑ �Ҵ��� ��� Memory Chunk�� �Ҵ����� */
    mAllocator.destroy();
   
    return IDE_SUCCESS;

}

/*
  Growing Memory Handle�� �Ҵ��� �޸𸮸� ���� ����
  
  [IN] aHandle - �ı��� Memory Handle
*/
IDE_RC iduGrowingMemoryHandle::clear()
{
    /* �޸� �Ҵ�Ⱑ �Ҵ��� ��� Memory Chunk�� �Ҵ����� */
    mAllocator.clear();

    // prepareMemory�� ȣ���Ͽ� �Ҵ�޾ư� �޸� ũ���� ����
    mTotalPreparedSize = 0;
    
    return IDE_SUCCESS;

}

    
/*
  Growing Memory Handle�� aSize�̻��� �޸𸮸� ��밡���ϵ��� �غ��Ѵ�.
  
  �׻� ���ο� �޸𸮸� �Ҵ��κ��� �Ҵ��Ͽ� �����Ѵ�.

  [IN] aSize   - �غ��� �޸� ������ ũ��
  [OUT] aPreparedMemory - �غ�� �޸��� �ּ�
*/
IDE_RC iduGrowingMemoryHandle::prepareMemory( UInt    aSize,
                                             void ** aPreparedMemory)
{
    IDE_DASSERT( aSize > 0 );
    IDE_DASSERT( aPreparedMemory != NULL );

    IDE_TEST( mAllocator.alloc( aSize, aPreparedMemory)
              != IDE_SUCCESS );

    mTotalPreparedSize += aSize;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    �� Memory Handle�� ���� OS�κ��� �Ҵ���� �޸��� �ѷ��� ����
 */
ULong iduGrowingMemoryHandle::getSize()
{
    return mTotalPreparedSize;
}




