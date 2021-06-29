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
 * $Id: sdcTXSegFreeList.cpp 89495 2020-12-14 05:19:22Z emlee $
 **********************************************************************/

# include <idl.h>
# include <ideErrorMgr.h>
# include <ideMsgLog.h>
# include <iduMutex.h>

# include <sdcTXSegMgr.h>
# include <sdcTXSegFreeList.h>

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� FreeList �ʱ�ȭ
 *
 * FreeList�� ���ü���� ���� Mutex�� �ʱ�ȭ�ϰ�,
 * Ʈ����� ���׸�Ʈ ��Ʈ������ ��� FreeList�� �����Ѵ�.
 *
 * �Ѱ��� ����� ���� Prepare Ʈ������� ����ؾ��Ѵ�.
 *
 * ���������������� UndoAll ������ Prepare Ʈ����ǵ��� ����ϴ�
 * Ʈ����� ���׸�Ʈ ��Ʈ���� Online �����̱� ������ �̸� ������
 * ���������� ������ FreeList�� �����ؾ��Ѵ�.
 *
 * aArrEntry        - [IN] Ʈ����� ���׸�Ʈ ���̺� Pointer
 * aEntryIdx        - [IN] FreeList ����
 * aFstEntry        - [IN] FreeList�� ������ ù��° Entry ����
 * aLstEntry        - [IN] FreeList�� ������ ù��° Entry ����
 *
 ***********************************************************************/
IDE_RC sdcTXSegFreeList::initialize( sdcTXSegEntry   * aArrEntry,
                                     UInt              aEntryIdx,
                                     SInt              aFstEntry,
                                     SInt              aLstEntry )
{
    SInt              i;
    SChar             sBuffer[128];

    IDE_DASSERT( aFstEntry <= aLstEntry );

    idlOS::memset( sBuffer, 0, 128 );

    idlOS::snprintf( sBuffer, 128, "TXSEG_FREELIST_MUTEX_%"ID_UINT32_FMT,
                     aEntryIdx );

    IDE_TEST( mMutex.initialize( 
                     sBuffer,
                     IDU_MUTEX_KIND_NATIVE,
                     IDV_WAIT_INDEX_LATCH_FREE_DRDB_TXSEG_FREELIST )
              != IDE_SUCCESS );

    mEntryCnt     = aLstEntry - aFstEntry + 1;
    mFreeEntryCnt = mEntryCnt;

    SMU_LIST_INIT_BASE( &mBase );

    for(i = aFstEntry; i <= aLstEntry; i++)
    {
        initEntry4Runtime( &aArrEntry[i], this );

        SMU_LIST_ADD_LAST( &mBase, &aArrEntry[i].mListNode );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� FreeList ����
 *
 ***********************************************************************/
IDE_RC sdcTXSegFreeList::destroy()
{
    IDE_ASSERT( SMU_LIST_IS_EMPTY( &mBase ) );
    return mMutex.destroy();
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �Ҵ�
 *
 * Ʈ����� ���׸�Ʈ ��Ʈ���� �Ҵ��Ѵ�.
 *
 * aEntry           - [OUT] Ʈ����� ���׸�Ʈ Entry Reference Pointer
 *
 ***********************************************************************/
void sdcTXSegFreeList::allocEntry( sdcTXSegEntry ** aEntry )
{
    smuList *sNode;

    IDE_ASSERT( aEntry != NULL );

    *aEntry = NULL;

    IDE_TEST_CONT( mFreeEntryCnt == 0, CONT_NO_FREE_ENTRY );

    IDE_ASSERT( lock() == IDE_SUCCESS );

    sNode = SMU_LIST_GET_FIRST( &mBase );

    if( sNode != &mBase )
    {
        IDE_ASSERT( mFreeEntryCnt != 0 );

        SMU_LIST_DELETE( sNode );
        mFreeEntryCnt--;
        SMU_LIST_INIT_NODE( sNode );

        *aEntry = (sdcTXSegEntry*)sNode->mData;
        IDE_ASSERT( (*aEntry)->mStatus == SDC_TXSEG_OFFLINE );

        (*aEntry)->mStatus = SDC_TXSEG_ONLINE;
    }

    IDE_ASSERT( unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( CONT_NO_FREE_ENTRY );

    return;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �Ҵ�
 *
 * BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
 * transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
 * 
 * aEntryID         - [IN]  �Ҵ���� Ʈ����� ���׸�Ʈ Entry ID
 * aEntry           - [OUT] Ʈ����� ���׸�Ʈ Entry Reference Pointer
 *
 ***********************************************************************/
void sdcTXSegFreeList::allocEntryByEntryID( UInt             aEntryID,
                                            sdcTXSegEntry ** aEntry )
{
    smuList       * sNode;
    sdcTXSegEntry * sEntry;

    IDE_ASSERT( aEntry != NULL );

    *aEntry = NULL;

    IDE_TEST_CONT( mFreeEntryCnt == 0, CONT_NO_FREE_ENTRY );

    IDE_ASSERT( lock() == IDE_SUCCESS );

    sNode = SMU_LIST_GET_FIRST( &mBase );

    while( 1 )
    {
        if( SMU_LIST_IS_EMPTY( &mBase ) || (sNode == &mBase) )
        {
            // aEntryID�� freeList�� empty�̰ų�
            // list ��ȸ�� ������ ��� ����
            break;
        }
            
        sEntry = (sdcTXSegEntry*)sNode->mData;

        // Ư�� entry ID�� segment entry�� ã�´�.
        if( sEntry->mEntryIdx == aEntryID )
        {
            // �Ҵ��� segment entry�� ������
            SMU_LIST_DELETE( sNode );
            mFreeEntryCnt--;
            SMU_LIST_INIT_NODE( sNode );

            *aEntry = (sdcTXSegEntry*)sNode->mData;
            IDE_ASSERT( (*aEntry)->mStatus == SDC_TXSEG_OFFLINE );

            (*aEntry)->mStatus = SDC_TXSEG_ONLINE;
            break;
        }
        else
        {
            // �Ҵ��� segment entry�� ã�� ������ list ��ȸ
            sNode = SMU_LIST_GET_NEXT( sNode );
        }
    }

    IDE_ASSERT( unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( CONT_NO_FREE_ENTRY );

    return;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 * Ʈ����� ���׸�Ʈ ��Ʈ���� �����Ѵ�.
 *
 * aEntry        - [IN] Ʈ����� ���׸�Ʈ Entry ������
 *
 ***********************************************************************/
void sdcTXSegFreeList::freeEntry( sdcTXSegEntry * aEntry,
                                  idBool          aMoveToFirst )
{
    IDE_ASSERT( aEntry->mStatus == SDC_TXSEG_ONLINE );

    IDE_ASSERT( lock() == IDE_SUCCESS );

    aEntry->mStatus = SDC_TXSEG_OFFLINE;

    initEntry4Runtime( aEntry, this );

    if( aMoveToFirst == ID_TRUE )
    {
        /*
         * �Ϲ����� ����� freeEntry�� Buffer Hit�� ����Ͽ� ���� Ȯ���� ���δ�.
         */
        SMU_LIST_ADD_FIRST( &mBase, &aEntry->mListNode );
    }
    else
    {
        /*
         * STEAL�� ���� freeEntry�� �ڷ� �̵��Ͽ� ���� Ȯ���� ���δ�
         */
        SMU_LIST_ADD_LAST( &mBase, &aEntry->mListNode );
    }
    
    mFreeEntryCnt++;

    IDE_ASSERT( unlock() == IDE_SUCCESS );

    return;
}

/***********************************************************************
 *
 * Description : ������ Ʈ����� ���׸�Ʈ�� ���������̸鼭
 *               Expired�Ǿ����� Ȯ���ϰ� ����Ʈ���� ����
 *
 * aEntry           - [IN] Ʈ����� ���׸�Ʈ Entry ������
 * aSegType         - [IN] Segment Type
 * aOldestTransBSCN - [IN] ���� ������ Ʈ������� Stmt�� SCN
 * aTrySuccess      - [OUT] �Ҵ翩��
 *
 ***********************************************************************/
void sdcTXSegFreeList::tryAllocExpiredEntryByIdx(
                               UInt             aEntryIdx,
                               sdpSegType       aSegType,
                               smSCN          * aOldestTransBSCN,
                               sdcTXSegEntry ** aEntry )
{
    sdcTXSegFreeList      * sFreeList;
    sdcTXSegEntry         * sEntry;
    UInt                    sState = 0;

    IDE_ASSERT( aOldestTransBSCN != NULL );
    IDE_ASSERT( aEntry           != NULL );

    *aEntry   = NULL;
    sEntry    = sdcTXSegMgr::getEntryByIdx( aEntryIdx );
    sFreeList = sEntry->mFreeList;

    IDE_TEST_CONT( sFreeList->mFreeEntryCnt == 0, CONT_NO_ENTRY );
    IDE_TEST_CONT( sEntry->mStatus == SDC_TXSEG_ONLINE, CONT_NO_ENTRY );

    IDE_ASSERT( sFreeList->lock() == IDE_SUCCESS );
    sState = 1;

    IDE_TEST_CONT( sEntry->mStatus == SDC_TXSEG_ONLINE, CONT_NO_ENTRY );

    IDE_TEST_CONT( isEntryExpired( sEntry, 
                                   aSegType, 
                                   aOldestTransBSCN ) == ID_FALSE,
                    CONT_NO_ENTRY );

    IDE_ASSERT( sFreeList->mFreeEntryCnt != 0 );

    SMU_LIST_DELETE( &sEntry->mListNode );
    sFreeList->mFreeEntryCnt--;
    SMU_LIST_INIT_NODE( &sEntry->mListNode );

    sEntry->mStatus = SDC_TXSEG_ONLINE;

    sState = 0;
    IDE_ASSERT( sFreeList->unlock() == IDE_SUCCESS );

    *aEntry = sEntry;

    IDE_EXCEPTION_CONT( CONT_NO_ENTRY );

    if ( sState != 0 )
    {
        IDE_ASSERT( sFreeList->unlock() == IDE_SUCCESS );
    }

    return;
}


/***********************************************************************
 *
 * Description : ������ Ʈ����� ���׸�Ʈ�� ���������̸� ����Ʈ���� ����
 *
 * aEntryIdx     - [IN]
 * aEntry        - [OUT] Ʈ����� ���׸�Ʈ Entry ������
 *
 ***********************************************************************/
void sdcTXSegFreeList::tryAllocEntryByIdx( UInt               aEntryIdx,
                                           sdcTXSegEntry   ** aEntry )
{
    smuList               * sNode;
    sdcTXSegFreeList      * sFreeList;
    sdcTXSegEntry         * sEntry;
    UInt                    sState = 0;

    IDE_ASSERT( aEntry != NULL );

    *aEntry   = NULL;
    sEntry    = sdcTXSegMgr::getEntryByIdx( aEntryIdx );
    sFreeList = sEntry->mFreeList;

    IDE_TEST_CONT( sFreeList->mFreeEntryCnt == 0, CONT_NO_ENTRY );
    IDE_TEST_CONT( sEntry->mStatus == SDC_TXSEG_ONLINE, CONT_NO_ENTRY );

    IDE_ASSERT( sFreeList->lock() == IDE_SUCCESS );
    sState = 1;

    IDE_TEST_CONT( sEntry->mStatus == SDC_TXSEG_ONLINE, CONT_NO_ENTRY );
    IDE_ASSERT( sFreeList->mFreeEntryCnt != 0 );

    sNode = &sEntry->mListNode;
    SMU_LIST_DELETE( sNode );
    sFreeList->mFreeEntryCnt--;
    SMU_LIST_INIT_NODE( sNode );

    sEntry->mStatus = SDC_TXSEG_ONLINE;

    sState = 0;
    IDE_ASSERT( sFreeList->unlock() == IDE_SUCCESS );

    *aEntry = sEntry;

    IDE_EXCEPTION_CONT( CONT_NO_ENTRY );

    if ( sState != 0 )
    {
        IDE_ASSERT( sFreeList->unlock() == IDE_SUCCESS );
    }

    return;
}
