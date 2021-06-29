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
 * $Id: smuSynchroList.cpp 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#include <idl.h>
#include <smuProperty.h>
#include <smuSynchroList.h>

/* ��ü �ʱ�ȭ
   
   [IN] aListName - List�� �̸�
   [IN] aMemoryIndex - List Node�� �Ҵ��� Memory Pool�� ����� ����
 */
IDE_RC smuSynchroList::initialize( SChar * aListName,
                                   iduMemoryClientIndex aMemoryIndex)
{
    SChar sName[128];

    idlOS::snprintf(sName, 128, "%s_MUTEX",
                    aListName );
    
    IDE_TEST( mListMutex.initialize( sName,
                                     IDU_MUTEX_KIND_NATIVE,
                                     IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
        

    idlOS::snprintf(sName, 128, "%s_NODE_POOL",
                    aListName );
    
    IDE_TEST( mListNodePool.initialize(
                  aMemoryIndex,
                  sName,
                  ID_SCALABILITY_SYS, 
                  (vULong)ID_SIZEOF(smuList),  // elem_size
                  128,                         // elem_count
                  IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                  ID_TRUE,							/* UseMutex */
                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                  ID_FALSE,							/* ForcePooling */
                  ID_TRUE,							/* GarbageCollection */
                  ID_TRUE,                          /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
             != IDE_SUCCESS );			
        
    SMU_LIST_INIT_BASE( & mList );

    mElemCount = 0;
        
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* ��ü �ı� */
IDE_RC smuSynchroList::destroy()
{
    // List�� List Node�� �����־�� �ȵȴ�.
    IDE_DASSERT( SMU_LIST_IS_EMPTY( &mList ) );
    IDE_DASSERT( mElemCount == 0 );
    
    SMU_LIST_INIT_BASE( & mList );

    IDE_TEST( mListNodePool.destroy() != IDE_SUCCESS );
    
    IDE_TEST( mListMutex.destroy() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/* Linked List�� Head�κ��� Data�� ���� 

   [IN] aData - Linked List�� �Ŵ޾� ���� �������� ������
                ���� List�� ����ִٸ� NULL�� �����Ѵ�.
 */
IDE_RC smuSynchroList::removeFromHead( void ** aData )
{
    IDE_DASSERT( aData != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode = NULL;

    IDE_TEST( mListMutex.lock( NULL ) != IDE_SUCCESS );
    sStage = 1;

    if ( ! SMU_LIST_IS_EMPTY( & mList ) )
    {
        sListNode = SMU_LIST_GET_FIRST( & mList );

        SMU_LIST_DELETE( sListNode );

        mElemCount --;
    }
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );

    if ( sListNode == NULL )
    {
        // List�� ����ִ� ���
        *aData = NULL;
    }
    else
    {
        *aData = sListNode->mData ;

        // List�� Add�Ҷ����� 
        // �������� �����ͷ� NULL�� ������� �ʾ����Ƿ�,
        // Data�� �����Ͱ� NULL�� �� ����.
        IDE_ASSERT( *aData != NULL );
        
        IDE_TEST( mListNodePool.memfree(sListNode) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( mListMutex.unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/* Linked List�� Tail�κ��� Data�� ���� 

   [IN] aData - Linked List�� �Ŵ޾� ���� �������� ������
                ���� List�� ����ִٸ� NULL�� �����Ѵ�.
 */
IDE_RC smuSynchroList::removeFromTail( void ** aData )
{
    IDE_DASSERT( aData != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode = NULL;

    IDE_TEST( mListMutex.lock(NULL) != IDE_SUCCESS );
    sStage = 1;

    if ( ! SMU_LIST_IS_EMPTY( & mList ) )
    {
        sListNode = SMU_LIST_GET_LAST( & mList );

        SMU_LIST_DELETE( sListNode );

        mElemCount --;
    }
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );

    if ( sListNode == NULL )
    {
        // List�� ����ִ� ���
        *aData = NULL;
    }
    else
    {
        *aData = sListNode->mData ;

        // List�� Add�Ҷ����� 
        // �������� �����ͷ� NULL�� ������� �ʾ����Ƿ�,
        // Data�� �����Ͱ� NULL�� �� ����.
        IDE_ASSERT( *aData != NULL );
        
        IDE_TEST( mListNodePool.memfree(sListNode) != IDE_SUCCESS );
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( mListMutex.unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/* Linked List�� Tail�� ����, ����Ʈ���� ���������� ���� )

   [IN] aData - Linked List�� �Ŵ޾� ���� �������� ������
                ���� List�� ����ִٸ� NULL�� �����Ѵ�.
 */
IDE_RC smuSynchroList::peekTail( void ** aData )
{
    IDE_DASSERT( aData != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode = NULL;

    IDE_TEST( mListMutex.lock(NULL) != IDE_SUCCESS );
    sStage = 1;

    if ( ! SMU_LIST_IS_EMPTY( & mList ) )
    {
        sListNode = SMU_LIST_GET_LAST( & mList );

    }
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );

    if ( sListNode == NULL )
    {
        // List�� ����ִ� ���
        *aData = NULL;
    }
    else
    {
        *aData = sListNode->mData ;
    }
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( mListMutex.unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/* Linked List�� Head�� Data�� Add 

   [IN] aData - Linked List�� �Ŵ޾� ���� �������� ������
 */
IDE_RC smuSynchroList::addToHead( void * aData )
{
    IDE_DASSERT( aData != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode ;

    // Pool���� List Node�Ҵ�
    // List Pool�� ������ ���ü� ��� �ϱ� ������
    // mListMutex�� ���� ���� ä�� ���డ��.
    IDE_TEST( mListNodePool.alloc( (void**) & sListNode ) != IDE_SUCCESS );

    SMU_LIST_INIT_NODE( sListNode );
    sListNode->mData = aData;
    
    IDE_TEST( mListMutex.lock( NULL ) != IDE_SUCCESS );
    sStage = 1;
    
    SMU_LIST_ADD_FIRST( &mList, sListNode );

    mElemCount ++;
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( mListMutex.unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/* Linked List�� Tail�� Data�� Add 

   [IN] aData - Linked List�� �Ŵ޾� ���� �������� ������
 */
IDE_RC smuSynchroList::addToTail( void * aData )
{
    IDE_DASSERT( aData != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode ;

    // Pool���� List Node�Ҵ�
    // List Pool�� ������ ���ü� ��� �ϱ� ������
    // mListMutex�� ���� ���� ä�� ���డ��.
    IDE_TEST( mListNodePool.alloc( (void**) & sListNode ) != IDE_SUCCESS );

    SMU_LIST_INIT_NODE( sListNode );
    sListNode->mData = aData;
    
    IDE_TEST( mListMutex.lock(NULL) != IDE_SUCCESS );
    sStage = 1;
    
    SMU_LIST_ADD_LAST( &mList, sListNode );

    mElemCount ++;
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( mListMutex.unlock() == IDE_SUCCESS );
                break;
            default:
                break;
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/* Linked List���� Element������ �����Ѵ�

   [OUT] aElemCount - ����Ʈ ���� Element����
 */
IDE_RC smuSynchroList::getElementCount( UInt * aElemCount )
{
    IDE_DASSERT( aElemCount != NULL );


    *aElemCount = mElemCount;

    return IDE_SUCCESS;
}
