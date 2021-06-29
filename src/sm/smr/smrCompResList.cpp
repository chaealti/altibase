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
 * $Id: smrCompResList.cpp 15368 2006-03-23 01:14:43Z kmkim $
 **********************************************************************/

#include <idl.h>
#include <smrCompResList.h>

/*  Ư�� �ð����� ������ ���� ���� ���ҽ���
    Linked List�� Tail�κ��� ����

    [IN] aMinimumResourceCount - �ּ� ���ҽ� ���� 
    [IN] aGarbageCollectionMicro -
            �� Micro �ʵ��� ������ ���� ��� Garbage�� �з�����?
    [OUT] aCompRes - ���� ���ҽ� 
 */
IDE_RC smrCompResList::removeGarbageFromTail(
           UInt          aMinimumResourceCount,
           ULong         aGarbageCollectionMicro,
           smrCompRes ** aCompRes )
{
    IDE_ASSERT( IDV_TIME_AVAILABLE() == ID_TRUE );

    IDE_DASSERT( aCompRes != NULL );
    
    UInt         sStage = 0;
    smuList    * sListNode = NULL;
    smrCompRes * sTailRes;
    ULong        sElapsedUS;
    
    idvTime sCurrTime;

    IDE_TEST( mListMutex.lock(NULL) != IDE_SUCCESS );
    sStage = 1;

    // �ּ� ���� �������� Ŭ ���
    if ( mElemCount > aMinimumResourceCount )
    {

        IDE_ASSERT( ! SMU_LIST_IS_EMPTY( & mList ) );
        
        sListNode = SMU_LIST_GET_LAST( & mList );
        
        sTailRes = (smrCompRes*) sListNode->mData;
        
        // ���� �ð��� ������ ���� �ð��� �ð� ���� ���
        IDV_TIME_GET( & sCurrTime );
        sElapsedUS = IDV_TIME_DIFF_MICRO( & sTailRes->mLastUsedTime,
                                          & sCurrTime );
        
        // Garbage Collection�� �ð��� �� ��� 
        if ( sElapsedUS >= aGarbageCollectionMicro )
        {
            SMU_LIST_DELETE( sListNode );
            
            mElemCount --;
        }
        else
        {
            // Garbage Collection�� �ð��� ���� ���� ���
            sListNode = NULL;
        }
    }
    
    sStage = 0;
    IDE_TEST( mListMutex.unlock() != IDE_SUCCESS );

    if ( sListNode == NULL )
    {
        // List�� ����ִ� ���
        *aCompRes = NULL;
    }
    else
    {
        *aCompRes = (smrCompRes*)sListNode->mData ;

        // List�� Add�Ҷ����� 
        // �������� �����ͷ� NULL�� ������� �ʾ����Ƿ�,
        // Data�� �����Ͱ� NULL�� �� ����.
        IDE_ASSERT( *aCompRes != NULL );
        
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
