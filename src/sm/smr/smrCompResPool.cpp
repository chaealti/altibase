/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: smrCompResPool.cpp 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#include <idl.h>
#include <smrCompResPool.h>

/* ��ü �ʱ�ȭ
   [IN] aPoolName - ���ҽ� Ǯ�� �̸� 
   [IN] aInitialResourceCount - �ʱ� ���ҽ� ����
   [IN] aMinimumResourceCount - �ּ� ���ҽ� ����
   [IN] aGarbageCollectionSecond - ���ҽ��� �� �� �̻� ������ ���� ��� Garbage Collection����?
 */
IDE_RC smrCompResPool::initialize( SChar * aPoolName,
                                   UInt    aInitialResourceCount,    
                                   UInt    aMinimumResourceCount,
                                   UInt    aGarbageCollectionSecond )
{
    IDE_TEST( mCompResList.initialize( aPoolName,
                                        IDU_MEM_SM_SMR )
              != IDE_SUCCESS );

    IDE_TEST( mCompResMemPool.initialize(
                                  IDU_MEM_SM_SMR,
                                  aPoolName,
                                  ID_SCALABILITY_SYS, 
                                  (vULong)ID_SIZEOF(smrCompRes),    /* elem_size */
                                  aInitialResourceCount,            /* elem_count */
                                  IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                  ID_TRUE,							/* UseMutex   */
                                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                                  ID_FALSE,							/* ForcePooling */
                                  ID_TRUE,							/* GarbageCollection */
                                  ID_TRUE,                          /* HWCacheLine */
                                  IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
              != IDE_SUCCESS);			

    mMinimumResourceCount = aMinimumResourceCount;
    
    // �ð� ������ Micro������ �̷�����Ƿ�,
    // Garbage Collection�� ���ؽð��� Micro�ʷ� �̸� ��ȯ�صд�.
    mGarbageCollectionMicro = aGarbageCollectionSecond * 1000000;

    // Overflow�߻� ���� �˻� 
    IDE_ASSERT( mGarbageCollectionMicro > aGarbageCollectionSecond );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* ��ü �ı� */
IDE_RC smrCompResPool::destroy()
{
    IDE_TEST( destroyAllCompRes() != IDE_SUCCESS );
    
    IDE_TEST( mCompResMemPool.destroy() != IDE_SUCCESS );

    IDE_TEST( mCompResList.destroy() != IDE_SUCCESS );
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* �α� ������ ���� Resource �� ��´�

   
   [IN] aCompRes - �Ҵ��� �α� ���� Resource
 */
IDE_RC smrCompResPool::allocCompRes( smrCompRes ** aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );
    
    smrCompRes * sCompRes = NULL;
    

    // 1. ��Ȱ�� List���� ã�´�
    //      CPU Cache Hit�� ���̱� ����,
    //      List�� ������ġ(Head)���� Remove, Add�Ѵ�.
    IDE_TEST( mCompResList.removeFromHead( (void**) & sCompRes )
              != IDE_SUCCESS );
    
    
    // 2. ��Ȱ�� List�� ���� ��� ���� �Ҵ�
    if ( sCompRes == NULL )
    {
        IDE_TEST( createCompRes( & sCompRes ) != IDE_SUCCESS );
    }

    // 3. ���� �ð� ���
    if ( IDV_TIME_AVAILABLE() == ID_TRUE )
    {
        IDV_TIME_GET( & sCompRes->mLastUsedTime );
    }
    else
    {
        /* Timer�� �ʱ�ȭ �Ǳ� ���� alloc �޴� ��� */
        sCompRes->mLastUsedTime.iTime.mClock = 0;
    }
    
    *aCompRes = sCompRes ;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/* �α� ������ ���� Resource �� �ݳ�
   
   [IN] aCompRes - �ݳ��� �α� ���� Resource
*/
IDE_RC smrCompResPool::freeCompRes( smrCompRes * aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );

    // CPU Cache Hit�� ���̱� ����,
    // List�� ������ġ(Head)���� Remove, Add�Ѵ�.
    IDE_TEST( mCompResList.addToHead( aCompRes ) != IDE_SUCCESS );

    // ���� ���� ���� ���� ���ҽ��� ���ð��� ����ð��� ���̰�
    // Ư�� �ð��� ����� Pool���� ����.
    //
    // Free�� ������ �ϳ����� ������ �ݷ�Ʈ �Ѵ�.
    // �ý��ۿ� ���ڱ� ���ϸ� ���� �ʰ�
    // ������ �ݷ�Ʈ �ϴ� ���ϸ� �л��Ű�� ����

    IDE_TEST( garbageCollectOldestRes() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ���ҽ� Ǯ �������� ���� ������ ���ҽ� �ϳ��� ����
    Garbage Collection�� �ǽ��Ѵ�.
    
    - ���� ���� ���� ���� ���ҽ� (List�� Tail�� �Ŵ޸� Element)��
      Ư�� �ð����� ������ ���� ��� Garbage Collection�ǽ�
 */
IDE_RC smrCompResPool::garbageCollectOldestRes()
{
    smrCompRes * sGarbageRes;
    
    IDE_TEST( mCompResList.removeGarbageFromTail( mMinimumResourceCount,
                                                  mGarbageCollectionMicro,
                                                  & sGarbageRes )
              != IDE_SUCCESS );
    if ( sGarbageRes != NULL )
    {
        // Garbage collection ����!
        IDE_TEST( destroyCompRes( sGarbageRes )
                  != IDE_SUCCESS );
    }
    else
    {
        // Garbage Collection�� ���ҽ��� ����.
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

    
        

/* �α� ������ ���� Resource �� �����Ѵ�

   [IN] aCompRes - �α� ���� Resource
 */
IDE_RC smrCompResPool::createCompRes( smrCompRes ** aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );

    smrCompRes * sCompRes;

    // ���ҽ� ����ü �Ҵ�
    /* smrCompResPool_createCompRes_alloc_CompRes.tc */
    IDU_FIT_POINT("smrCompResPool::createCompRes::alloc::CompRes");
    IDE_TEST( mCompResMemPool.alloc( (void**) & sCompRes ) != IDE_SUCCESS );

    // �α� �������� ���� �ڵ��� �ʱ�ȭ
    IDE_TEST( sCompRes->mDecompBufferHandle.initialize( IDU_MEM_SM_SMR )
              != IDE_SUCCESS );
    
    // �α� ������� �ڵ��� �ʱ�ȭ
    IDE_TEST( sCompRes->mCompBufferHandle.initialize( IDU_MEM_SM_SMR )
              != IDE_SUCCESS );
    
    *aCompRes = sCompRes ;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* �α� ������ ���� Resource �� �ı��Ѵ�

   [IN] aCompRes - �α� ���� Resource
 */
IDE_RC smrCompResPool::destroyCompRes( smrCompRes * aCompRes )
{
    IDE_DASSERT( aCompRes != NULL );

    // �α� �������� �ڵ��� �ı�
    IDE_TEST( aCompRes->mDecompBufferHandle.destroy() != IDE_SUCCESS );
    
    // �α� ������� �ڵ��� �ı�
    IDE_TEST( aCompRes->mCompBufferHandle.destroy() != IDE_SUCCESS );

    IDE_TEST( mCompResMemPool.memfree( aCompRes ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* �� Pool�� ������ ��� ���� Resource �� �ı��Ѵ�

   [IN] aCompRes - �α� ���� Resource

 */
IDE_RC smrCompResPool::destroyAllCompRes()
{

    smrCompRes * sCompRes;

    
    for(;;)
    {
        IDE_TEST( mCompResList.removeFromHead( (void**) & sCompRes )
                  != IDE_SUCCESS );

        if ( sCompRes == NULL ) // �� �̻� List Node�� ���� ��� 
        {
            break;
        }

        IDE_TEST( destroyCompRes( sCompRes ) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* �α� ���� ���ҽ��� ���� ������ ũ�⸦ �����Ѵ�.

   [IN] aCompRes - �α� ���� Resource
   [IN] aSize    - ������ ũ��

 */
IDE_RC smrCompResPool::tuneCompRes( smrCompRes * aCompRes,
                                    UInt         aSize )
{
    IDE_DASSERT( aCompRes != NULL );

    IDE_TEST( aCompRes->mDecompBufferHandle.tuneSize( aSize ) != IDE_SUCCESS );
    
    IDE_TEST( aCompRes->mCompBufferHandle.tuneSize( aSize ) != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


