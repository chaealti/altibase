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
 * $Id: smmShmKeyMgr.cpp 18299 2006-09-26 07:51:56Z xcom73 $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smmShmKeyList.h>
#include <smmShmKeyMgr.h>


smmShmKeyList smmShmKeyMgr::mFreeKeyList; // ��Ȱ�� �����޸� Key List
iduMutex      smmShmKeyMgr::mMutex; 
key_t         smmShmKeyMgr::mSeekKey;

// ��밡���� �����޸� Key�� ���� ���� �� 
#define SMM_MIN_SHM_KEY_CANDIDATE (1024)

smmShmKeyMgr::smmShmKeyMgr()
{
}
smmShmKeyMgr::~smmShmKeyMgr()
{
}



/*
   ���� ����� �����޸� Key �ĺ��� ã�� �����Ѵ�.

   �ĺ��� ���� :
     �ش� Key�� �̿��Ͽ� �� �����޸� ������ ������ �� ���� ���� �ְ�
     �̹� �ش� Key�� �����޸� ������ �����Ǿ� ���� ���� �ֱ� ����.

   aShmKeyCandidate [OUT] 0 : �����޸� Key �ĺ��� ����
                          Otherwise : ����� �� �ִ� �����޸� Key �ĺ�

   PROJ-1548 Memory Tablespace
   - mSeekKey�� ���� ���ü� ��� �ϴ� ����
     - mSeekKey�� ���� Tx�� ���ÿ� ������ �� �ִ�.
     - mSeekKey�� ���� Tablespace���� ���ÿ� ������ �� �ִ�.

  - �����ϸ� ������ �ٸ� Tablespace���� ���Ǿ��� Key��
     ��Ȱ���Ͽ� ����Ѵ�.
*/
IDE_RC smmShmKeyMgr::getShmKeyCandidate(key_t * aShmKeyCandidate) 
{
    UInt sState = 0;
    
    IDE_DASSERT( aShmKeyCandidate != NULL )
    
    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;
    
    
    // ��Ȱ�� Key List�� ��Ȱ�� Key�� �ִٸ� �̸� ���� ����Ѵ�.
    if ( mFreeKeyList.isEmpty() == ID_FALSE )
    {
        IDE_TEST( mFreeKeyList.removeKey( aShmKeyCandidate ) != IDE_SUCCESS );
    }
    else // ��Ȱ�� Key List �� Key�� ����.
    {
        // ����� �� �ִ� Key�ĺ��� �ִ°�?
        if ( mSeekKey > SMM_MIN_SHM_KEY_CANDIDATE ) 
        {
            *aShmKeyCandidate = --mSeekKey;
        }
        else // ����� �� �ִ� Key�ĺ��� ����.
        {
            IDE_RAISE(error_no_more_key);
        }
    }

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION(error_no_more_key);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_No_More_Shm_Key));
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1 :
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();
    
    return IDE_FAILURE;
}

/*
 * ���� ������� Key�� �˷��ش�.
 *
 * �� �Լ��� attach�ÿ� ȣ��Ǹ�,
 * �̹� ���� Ű���� ū ���� �����޸� �ĺ�Ű�� ������� �ʵ��� �Ѵ�.
 *
 * aUsedKey [IN] ���� ������� Key
 */ 
IDE_RC smmShmKeyMgr::notifyUsedKey(key_t aUsedKey)
{
    UInt sState = 0;
    
    IDE_DASSERT( aUsedKey != 0 );
    
    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;
    
    // BUGBUG-1548 FreeKey List�� Incremental�ϰ� ���� ���־�� �Ѵ�.
    if ( mSeekKey >= aUsedKey ) 
    {
        // �̹� ���� Key���� �ϳ� ���� Key���� ����Ѵ�.
        mSeekKey = aUsedKey -1;
    }

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1 :
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();
    
    return IDE_FAILURE;
}


/*
 * ���̻� ������ �ʴ� Key�� �˷��ش�.
 *
 * �� �Լ��� Tablespace drop/offline�� �����޸� ������ �����Ҷ�
 * ȣ��Ǹ�, ���̻� ������ �ʴ� �����޸� Key�� �ٸ�
 * Tablespace���� ��Ȱ��� �� �ֵ��� ���´�.
 *
 * aUnusedKey [IN] ���̻� ������ �ʴ� Key
 */ 
IDE_RC smmShmKeyMgr::notifyUnusedKey(key_t aUnusedKey)
{
    UInt sState = 0;
    
    IDE_DASSERT( aUnusedKey != 0 );
    
    IDE_TEST( lock() != IDE_SUCCESS );
    sState = 1;
    
    // ��Ȱ�� Key List�� �߰��Ѵ�.
    IDE_TEST( mFreeKeyList.addKey( aUnusedKey ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( unlock() != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1 :
            IDE_ASSERT( unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();
    
    return IDE_FAILURE;
}

/*
 * shmShmKeyMgr�� static �ʱ�ȭ ���� 
 */
IDE_RC smmShmKeyMgr::initializeStatic()
{
    // ��Ȱ�� Key������ �ʱ�ȭ
    IDE_TEST( mFreeKeyList.initialize() != IDE_SUCCESS );
    
    // mSeekKey���� Read/Write�� �� ��� Mutex
    IDE_TEST( mMutex.initialize((SChar*)"SHARED_MEMORY_KEY_MUTEX",
                                IDU_MUTEX_KIND_NATIVE,
                                IDV_WAIT_INDEX_NULL)
              != IDE_SUCCESS );
    
    mSeekKey = (key_t) smuProperty::getShmDBKey();
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/*
 * shmShmKeyMgr�� static �ı� ���� 
 */
IDE_RC smmShmKeyMgr::destroyStatic()
{
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    IDE_TEST( mFreeKeyList.destroy() != IDE_SUCCESS );
    
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;

}

