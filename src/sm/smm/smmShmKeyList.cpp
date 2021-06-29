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
 * $Id: smmShmKeyList.cpp 18299 2006-09-26 07:51:56Z xcom73 $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smuList.h>
#include <smmShmKeyList.h>

smmShmKeyList::smmShmKeyList()
{
}

smmShmKeyList::~smmShmKeyList()
{
}



/*
 * �����޸� Key List�� �����Ѵ�.
 */
IDE_RC smmShmKeyList::initialize()
{
    IDE_TEST( mListNodePool.initialize( IDU_MEM_SM_INDEX,
                                       (SChar*)"FREE_SHARED_KEY_NODE_POOL",
                                       ID_SCALABILITY_SYS,
                                       ID_SIZEOF(smuList), /* Element Size */
                                       100, /* Element Count */
                                       IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                                       ID_TRUE,								/* UseMutex */
                                       IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,		/* AlignByte */
                                       ID_FALSE,							/* ForcePooling */
                                       ID_TRUE,								/* GarbageCollection */
                                       ID_TRUE,                             /* HWCacheLine */
                                       IDU_MEMPOOL_TYPE_LEGACY              /* mempool type */) 
              != IDE_SUCCESS);			

    SMU_LIST_INIT_BASE( & mKeyList );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * �����޸� Key List�� �ı��Ѵ�
 */
IDE_RC smmShmKeyList::destroy()
{

    // �����޸� Key�� ��� ����
    IDE_TEST( removeAll() != IDE_SUCCESS );

    IDE_TEST( mListNodePool.destroy() != IDE_SUCCESS);

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/*
    �����޸� Key�� �����޸� Key List�� �߰��Ѵ�

    aKey [IN] - �����޸� Key List�� �߰��� Key
*/
IDE_RC smmShmKeyList::addKey(key_t aKey)
{
    UInt  sState = 0;

    smuList * sListNode ;

    IDE_DASSERT( aKey != 0 );

    /* smmShmKeyList_addKey_alloc_ListNode.tc */
    IDU_FIT_POINT("smmShmKeyList::addKey::alloc::ListNode");
    IDE_TEST( mListNodePool.alloc( (void**) & sListNode ) != IDE_SUCCESS );
    SMU_LIST_INIT_NODE( sListNode );
    sState = 1;


    IDE_ASSERT( ID_SIZEOF( sListNode->mData ) >= ID_SIZEOF( aKey ) );
    sListNode->mData = (void*) (vULong) aKey ;

    SMU_LIST_ADD_LAST( & mKeyList, sListNode );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1 :
            IDE_ASSERT( mListNodePool.memfree( (void**) & sListNode )
                        == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}


/*
    �����޸� Key List���� �����޸� Key�� ������.

    aKey [OUT] - �����޸� Key List���� ���� �����޸� Key

    �������� : �����޸� Key List�� �����޸� Key�� �־�� �Ѵ�.
*/
IDE_RC smmShmKeyList::removeKey(key_t * aKey)
{
    smuList * sListNode ;

    IDE_DASSERT( aKey != NULL );
    IDE_DASSERT( isEmpty() == ID_FALSE );

    sListNode = SMU_LIST_GET_FIRST( & mKeyList );
    SMU_LIST_DELETE( sListNode );

    IDE_ASSERT( ID_SIZEOF( sListNode->mData ) >= ID_SIZEOF( aKey ) );
    *aKey = (key_t) (vULong) sListNode->mData ;

    IDE_TEST( mListNodePool.memfree( sListNode ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
/*
    �����޸� Key List���� ��� Key�� �����Ѵ�
 */
IDE_RC smmShmKeyList::removeAll()
{
    key_t sKey;

    while( isEmpty() == ID_FALSE )
    {
        IDE_TEST( removeKey( & sKey ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}


/*
 * �����޸� Key List�� ����ִ��� üũ�Ѵ�.
 * return [OUT] ID_TRUE - �����޸� Key�� ����
 *              ID_FALSE - �����޸� Key�� �ִ�.
 */
idBool smmShmKeyList::isEmpty()
{
    return ( SMU_LIST_GET_FIRST(&mKeyList) == &mKeyList ) ? ID_TRUE : ID_FALSE;
}
