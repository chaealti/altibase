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
 * $Id: iduReusedMemoryHandle.cpp 15368 2006-03-23 01:14:43Z leekmo $
 **********************************************************************/

#include <idl.h>
#include <iduMemMgr.h>
#include <iduReusedMemoryHandle.h>


// ��ü ������,�ı��� => �ƹ��ϵ� �������� �ʴ´�.
iduReusedMemoryHandle::iduReusedMemoryHandle()
{
}
iduReusedMemoryHandle::~iduReusedMemoryHandle()
{
}


/*
  Resizable Memory Handle�� �ʱ�ȭ �Ѵ�.

  [IN] aMemoryClient - iduMemMgr�� �ѱ� �޸� �Ҵ� Client
*/
IDE_RC iduReusedMemoryHandle::initialize( iduMemoryClientIndex   aMemoryClient )
{
    /* ��ü�� virtual function table�ʱ�ȭ ���ؼ� new�� ȣ�� */
    new (this) iduReusedMemoryHandle();
   
#ifdef __CSURF__
    IDE_ASSERT( this != NULL );
#endif   
    
    /* ũ�� 0, �޸� NULL�� �ʱ�ȭ */
    mSize   = 0;
    mMemory = NULL;
    mMemoryClient = aMemoryClient;
    
    assertConsistency();
    
    return IDE_SUCCESS;
}
    
/*
  Resizable Memory Handle�� �ı� �Ѵ�.
  
  [IN] aHandle - �ı��� Memory Handle
*/
IDE_RC iduReusedMemoryHandle::destroy()
{
    assertConsistency();

    if ( mSize > 0 )
    {
        IDE_TEST( iduMemMgr::free( mMemory ) != IDE_SUCCESS );
        mMemory = NULL;
        mSize = 0;
    }
    
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


    
/*
  Resizable Memory Handle�� aSize�̻��� �޸𸮸� ��밡���ϵ��� �غ��Ѵ�.
  �̹� aSize�̻� �޸𸮰� �Ҵ�Ǿ� �ִٸ� �ƹ��ϵ� ���� �ʴ´�.

  [����] �޸𸮴���ȭ ������ �����ϱ� ���� 2�� N���� ũ��θ� �Ҵ��Ѵ�.

  [IN] aSize   - �غ��� �޸� ������ ũ��
  [OUT] aPreparedMemory - �غ�� �޸��� �ּ�
*/

IDE_RC iduReusedMemoryHandle::prepareMemory( UInt aSize,
                                             void ** aPreparedMemory )
{
    IDE_DASSERT( aSize > 0 );
    IDE_DASSERT( aPreparedMemory != NULL);
    
    
    UInt aNewSize ;
    
    assertConsistency();

    if ( mSize < aSize )
    {
        aNewSize = alignUpPowerOfTwo( aSize );

        if ( mSize == 0 )
        {
            IDE_TEST( iduMemMgr::malloc( mMemoryClient,
                                         aNewSize,
                                         & mMemory )
                      != IDE_SUCCESS );
        }
        else
        {
            IDE_TEST( iduMemMgr::realloc( mMemoryClient,
                                          aNewSize,
                                          & mMemory )
                      != IDE_SUCCESS );
        }
        
        mSize = aNewSize;
        
        assertConsistency();
    }

    *aPreparedMemory = mMemory;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    �� Memory Handle�� ���� �Ҵ���� �޸��� �ѷ��� ����
 */
ULong iduReusedMemoryHandle::getSize( void )
{
    assertConsistency();
    
    return mSize;
}


/*
  Resizable Memory Handle�� ���ռ� ����(ASSERT)
*/
void iduReusedMemoryHandle::assertConsistency()
{
    if ( mSize == 0 )
    {
        IDE_ASSERT( mMemory == NULL );
    }
    else
    {
        IDE_ASSERT( mMemory != NULL );
    }
    
    
    if ( mMemory == NULL )
    {
        IDE_ASSERT( mSize == 0 );
    }
    else
    {
        IDE_ASSERT( mSize > 0 );
    }
}

/*
  aSize�� 2�� N���� ũ��� Align up�Ѵ�.
  
  ex> 1000 => 1024
      2017 => 2048

  [IN] aSize - Align Up�� ũ��
*/
UInt iduReusedMemoryHandle::alignUpPowerOfTwo( UInt aSize )
{
    IDE_ASSERT ( aSize > 0 );

    --aSize;
    
    aSize |= aSize >> 1;
    aSize |= aSize >> 2;
    aSize |= aSize >> 4;
    aSize |= aSize >> 8;
    aSize |= aSize >> 16;
    
    return ++aSize ;
}

/*
  Resizable Memory Handle �� ���� �̻��� Size�� Memory�� �Ҵ� �Ǿ� �������
  ���ο� Size�� �������� ������ ���ش�.

  [����] �޸𸮴���ȭ ������ �����ϱ� ���� 2�� N���� ũ��θ� �Ҵ��Ѵ�.

  [IN] aSize   - �غ��� �޸� ������ ũ�� ( 0���� Ŀ�߸� �Ѵ� )
*/

IDE_RC iduReusedMemoryHandle::tuneSize( UInt aSize )
{
    IDE_ASSERT( aSize > 0 );
    
    UInt sNewSize ;
    
    assertConsistency();

    /* aSize�� 2�� N���� �ƴ� ���� �Ѿ� �ð�� aSize�� mSize�� ���� ����ϸ�
     * mSize = 1024, aSize = 1000 ���� ��� ���� Size�� ���Ҵ� �Ϸ� �Ҽ� �����Ƿ�
     * alignUp�� ���� ������ �ش�.
     * ���������� aSize�� 0�� ��� ASSERT �̹Ƿ� aSize�� �׻� 0���� Ŀ�� �Ѵ�. */
    sNewSize = alignUpPowerOfTwo( aSize );

    if ( mSize > sNewSize )
    {
        /* sNewSize�� �׻� 0���� Ŀ�ߵǹǷ� mSize�� 0 �� NULL�� ���� ����.  */
        IDE_TEST( iduMemMgr::free( mMemory ) != IDE_SUCCESS );
        mMemory = NULL;
        mSize = 0;

        IDE_TEST( iduMemMgr::malloc( mMemoryClient,
                                     sNewSize,
                                     &mMemory )
                  != IDE_SUCCESS );
        
        mSize = sNewSize;
        
        assertConsistency();
    }
   
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

