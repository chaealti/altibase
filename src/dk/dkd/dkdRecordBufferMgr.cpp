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
 * $id$
 **********************************************************************/

#include <dkdRecordBufferMgr.h>


/***********************************************************************
 * Description: Record buffer manager �� �ʱ�ȭ�Ѵ�.
 *              �� �������� DK data buffer �κ��� record buffer �Ҵ��� 
 *              �������� üũ�غ��� �Ҵ� ������ ���� record buffer 
 *              manager �� �����ϰ�, �׷��� �ʴٸ� disk temp table 
 *              manager �� �����Ѵ�.
 *
 *  BUG-37215: ������ record buffer manager ��ü�� �����Ǵ� �������� 
 *             TLSF memory allocator �� �����ϵ��� �Ǿ� �־����� ȿ������ 
 *             �޸� �Ҵ� �� ������ ���� global allocator �� data buffer 
 *             manager ( dkdDataBufferMgr )�� �ΰ�, �Է����ڷ� �Ѱܹ޾�
 *             ����ϵ��� ����ʿ� ���� TLSF memory allocator �� ������ 
 *             ���õ� �κе��� �����ǰ� �Է����ڰ� �ϳ� �߰���.
 *
 *  aBufBlockCnt    - [IN] �� record buffer manager �� �Ҵ���� buffer
 *                         �� �����ϴ� buffer block �� ����
 *  aAllocator      - [IN] record buffer �� �Ҵ���� �� ����ϱ� ���� 
 *                         TLSF memory allocator �� ����Ű�� ������
 *
 **********************************************************************/
IDE_RC  dkdRecordBufferMgr::initialize( UInt             aBufBlockCnt, 
                                        iduMemAllocator *aAllocator )
{
    IDE_ASSERT( aBufBlockCnt > 0 );

    mIsEndOfFetch = ID_FALSE;
    mRecordCnt    = 0;
    mBufSize      = aBufBlockCnt * (ULong)dkdDataBufferMgr::getBufferBlockSize();
    mBufBlockCnt  = aBufBlockCnt;
    mCurRecord    = NULL;
    mAllocator    = aAllocator;

    IDU_LIST_INIT( &mRecordList );

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : Record buffer manager �� �����Ѵ�.
 *
 *  BUG-37487 : return ���� IDE_RC --> void �� ����.
 *
 ************************************************************************/
void dkdRecordBufferMgr::finalize()
{
    iduListNode     *sCurRecordNode;
    iduListNode     *sNextRecordNode;

    IDU_LIST_ITERATE_SAFE( &mRecordList, sCurRecordNode, sNextRecordNode )
    {
        /* BUG-37487 : void */
        (void)iduMemMgr::free( sCurRecordNode->mObj, mAllocator ); 

        if ( --mRecordCnt < 0 )
        {
            mRecordCnt = 0;
        }
        else
        {
            /* record count became zero */
        }
    }

    IDE_ASSERT( mRecordCnt == 0 );

    /* BUG-37215 */
    mAllocator = NULL;
}

/************************************************************************
 * Description : Record buffer �κ��� record �ϳ��� fetch �ؿ´�. 
 *
 *  aRow        - [OUT] fetch �ؿ� row �� ����Ű�� ������
 *
 ************************************************************************/
IDE_RC  dkdRecordBufferMgr::fetchRow( void  **aRow )
{
    /* fetch row */
    if ( mCurRecord != NULL )
    {
        if ( mCurRecord->mData != NULL )
        {
            *aRow = mCurRecord->mData;
            IDE_TEST( moveNext() != IDE_SUCCESS );
        }
        else
        {
            /* error!! */
        }
    }
    else
    {
        /* no more rows exists, success */
        *aRow = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Record buffer �� record �ϳ��� insert �Ѵ�. 
 *
 *  aRecord     - [IN] insert �� record
 *
 ************************************************************************/
IDE_RC  dkdRecordBufferMgr::insertRow( dkdRecord  *aRecord )
{
    IDU_LIST_INIT_OBJ( &(aRecord->mNode), aRecord );
    IDU_LIST_ADD_LAST( &mRecordList, &(aRecord->mNode) );

    mRecordCnt++;

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : Record buffer �� iteration �� restart ��Ų��.
 *
 ************************************************************************/
IDE_RC  dkdRecordBufferMgr::restart()
{
    IDE_TEST( moveFirst() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/************************************************************************
 * Description : Record buffer �� cursor �� record buffer �� ���� record
 *               �� ����Ű���� �Ѵ�.
 *
 ************************************************************************/
IDE_RC  dkdRecordBufferMgr::moveNext()
{
    dkdRecord       *sRecord = NULL;
    iduListNode     *sNextNode;

    sNextNode = IDU_LIST_GET_NEXT( &mCurRecord->mNode );
    sRecord   = (dkdRecord *)(sNextNode->mObj);

    mCurRecord = sRecord;

    return IDE_SUCCESS;
}

/************************************************************************
 * Description : Record buffer �� cursor �� record buffer �� ó�� record
 *               �� ����Ű���� �Ѵ�.
 *
 ************************************************************************/
IDE_RC  dkdRecordBufferMgr::moveFirst()
{
    dkdRecord       *sRecord = NULL;
    iduListNode     *sFirstNode;

    sFirstNode = IDU_LIST_GET_FIRST( &mRecordList );
    sRecord    = (dkdRecord *)(sFirstNode->mObj);

    mCurRecord = sRecord;

    return IDE_SUCCESS;
}

IDE_RC  fetchRowFromRecordBuffer( void *aMgrHandle, void **aRow )
{
    IDE_TEST( ((dkdRecordBufferMgr *)aMgrHandle)->fetchRow( aRow )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


IDE_RC  insertRowIntoRecordBuffer( void *aMgrHandle, dkdRecord *aRecord )
{
    IDE_TEST( ((dkdRecordBufferMgr *)aMgrHandle)->insertRow( aRecord ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC  restartRecordBuffer( void  *aMgrHandle )
{
    IDE_TEST( ((dkdRecordBufferMgr *)aMgrHandle)->restart() 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


