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
 * PROJ-1568 BUFFER MANAGER RENEWAL
 ***********************************************************************/

/***********************************************************************
 * $$Id:$
 **********************************************************************/
#include <smDef.h>
#include <sdbDef.h>
#include <sdbBCB.h>
#include <smErrorCode.h>

ULong sdbBCB::mTouchUSecInterval;

/***********************************************************************
 * Description :
 *  aFrameMemHandle - [IN] mFrame�� ���� MemoryHandle�μ� Free�ÿ� ����Ѵ�. 
 *                         (����:iduMemPool2)
 *  aFrame          - [IN] frame pointer
 *  aBCBID          - [IN] BCB �ĺ���
 ***********************************************************************/
IDE_RC sdbBCB::initialize( void   *aFrameMemHandle,
                           UChar  *aFrame,
                           UInt    aBCBID )
{
    SChar sMutexName[128];

    IDE_ASSERT( aFrame != NULL );

    /*[BUG-22041] mSpaceID�� mPageID�� BCB�� ���°� free�� �̻�
     * �� �ΰ��� �ǹ̰� ���� �ʱ�ȭ�� ���� �ʾ����� umr�� �߻��ϴ� ��찡
     * �־� 0���� �ʱ�ȭ�� �Ѵ�.*/
    mSpaceID        = 0;
    mPageID         = 0;
    mID             = aBCBID;
    mFrame          = aFrame;
    mFrameMemHandle = aFrameMemHandle;

    setBCBPtrOnFrame( (sdbFrameHdr*)aFrame, this );

    idlOS::snprintf( sMutexName, 
                     ID_SIZEOF(sMutexName),
                     "BCB_LATCH_%"ID_UINT32_FMT, 
                     aBCBID );

    IDE_ASSERT( mPageLatch.initialize( sMutexName )
                == IDE_SUCCESS );

    idlOS::snprintf( sMutexName, 128, "BCB_MUTEX_%"ID_UINT32_FMT, aBCBID );


    /*
     * BUG-28331   [SM] AT-P03 Scalaility-*-Disk-* ������ ���� ���� �м� 
     *             (2009�� 11�� 21�� ����)
     */
    IDE_TEST( mMutex.initialize( sMutexName,
                                 IDU_MUTEX_KIND_NATIVE,
                                 IDV_WAIT_INDEX_LATCH_FREE_DRDB_BCB_MUTEX )
              != IDE_SUCCESS );

    idlOS::snprintf( sMutexName, 128, "BCB_READ_IO_MUTEX_%"ID_UINT32_FMT, aBCBID );

    IDE_TEST( mReadIOMutex.initialize(
                  sMutexName,
                  IDU_MUTEX_KIND_NATIVE,
                  IDV_WAIT_INDEX_LATCH_FREE_DRDB_BCB_READ_IO_MUTEX )
              != IDE_SUCCESS );

    /* BUG-24092: [SD] BufferMgr���� BCB�� Latch Stat�� ���Ž� Page Type��
     * Invalid�Ͽ� ������ �׽��ϴ�.
     *
     * mPageType�� ���ϴ� Page�� Buffer�� �������� ���� ��� ReadPage��
     * �Ϸ��� Set�Ѵ�. ������ ReadPage�� ���� BCB�� ��û�ϴ� Ÿ Thread��
     * ����ϴµ� �̶� mPageType�� ���� ��������� �����Ѵ�. ������ mPageType
     * �� ���� ������ ���� �������� ���� �� �ִ�. �� ��� mPageType�� 0����
     * �а��Ѵ�. ������ ��������� ��Ȯ���� ���� �� �ִ�. ������ Member�鵵
     * ������� �������� �ʱ�ȭ �մϴ�. */
    mPageType = SDB_NULL_PAGE_TYPE;

    mState    = SDB_BCB_FREE;
    mSpaceID  = 0;
    mPageID   = 0;

    SM_LSN_INIT( mRecoveryLSN );

    mBCBListType   = SDB_BCB_NO_LIST;
    mBCBListNo     = 0;
    mCPListNo      = 0;
    mTouchCnt      = 0;
    mFixCnt        = 0;
    mReadyToRead   = ID_FALSE;
    mPageReadError = ID_FALSE;
    mWriteCount    = 0;
    mHashBucketNo  = 0;
    /* PROJ-2102 Secondary Buffer */
    mSBCB          = NULL;

    /* BUG-47945 cp list ����� ���� ����
     * setToFree() �Լ����� ���� �� Ȯ���ϵ��� ����Ǿ,
     * ó�� �ʱ�ȭ�� ���� �մϴ�. */
    makeFreeExceptListItem();

    SDB_INIT_BCB_LIST( this );
    SDB_INIT_CP_LIST( this );

    SMU_LIST_INIT_NODE( &mBCBListItem );
    mBCBListItem.mData = this;

    SMU_LIST_INIT_NODE( &mCPListItem );
    mCPListItem.mData  = this;

    SMU_LIST_INIT_NODE( &mHashItem );
    mHashItem.mData    = this;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description :
 *  sdbBCB �Ҹ���.
 ***********************************************************************/
IDE_RC sdbBCB::destroy()
{
    IDE_ASSERT(mPageLatch.destroy() == IDE_SUCCESS);

    IDE_ASSERT(mMutex.destroy() == IDE_SUCCESS);

    IDE_ASSERT(mReadIOMutex.destroy() == IDE_SUCCESS);

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description :
 *  sm �α׿� dump (BUG-47945 cp list ����� ���� ����)
 * 
 *  aBCB    - [IN]  BCB
 ***********************************************************************/
void sdbBCB::dump()
{
    ideLog::log( IDE_ERR_0,
                 SM_TRC_BUFFER_POOL_BCB_INFO,
                 mID,
                 mState,
                 mFrame,
                 mSpaceID,
                 mPageID,
                 mPageType,
                 mRecoveryLSN.mFileNo,
                 mRecoveryLSN.mOffset,
                 mBCBListType,
                 mBCBListNo,
                 mCPListNo,
                 mTouchCnt,
                 mFixCnt,
                 mReadyToRead,
                 mPageReadError,
                 mWriteCount );
}

/***********************************************************************
 * Description :
 *  SDB_BCB_CLEAN ���·� ������ �����Ѵ�.
 ***********************************************************************/
void sdbBCB::clearDirty()
{
    SM_LSN_INIT( mRecoveryLSN );

    mState = SDB_BCB_CLEAN;
}

/***********************************************************************
 * Description :
 *  �� BCB�� mPageID�� mSpaceID�� ������ ���Ѵ�.
 *
 *  aLhs    - [IN]  BCB
 *  aRhs    - [IN]  BCB
 ***********************************************************************/
idBool sdbBCB::isSamePageID( void *aLhs, void *aRhs )
{
    if( (((sdbBCB*)aLhs)->mPageID  == ((sdbBCB*)aRhs)->mPageID ) &&
        (((sdbBCB*)aLhs)->mSpaceID == ((sdbBCB*)aRhs)->mSpaceID ))
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}
