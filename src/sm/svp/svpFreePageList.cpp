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
 
#include <svmDef.h>
#include <svpReq.h>
#include <svpAllocPageList.h>
#include <svpFreePageList.h>
#include <smuProperty.h>
#include <svmManager.h>

/**********************************************************************
 * Runtime Item�� NULL�� �����Ѵ�.
 * DISCARD/OFFLINE Tablespace�� ���� Table�鿡 ���� ����ȴ�.
 *
 * aPageListEntry : �����Ϸ��� PageListEntry
 **********************************************************************/
IDE_RC svpFreePageList::setRuntimeNull( smpPageListEntry* aPageListEntry )
{
    aPageListEntry->mRuntimeEntry = NULL;

    return IDE_SUCCESS;
}

/**********************************************************************
 * PageListEntry���� FreePage�� ���õ� RuntimeEntry�� ���� �ʱ�ȭ ��
 * FreePageList�� Mutex �ʱ�ȭ
 *
 * aPageListEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

IDE_RC svpFreePageList::initEntryAtRuntime( smpPageListEntry* aPageListEntry )
{
    smOID                   sTableOID;
    UInt                    sPageListID;
    UInt                    sState  = 0;
    SChar                   sBuffer[128];
    smpFreePagePoolEntry  * sFreePagePool;
    smpFreePageListEntry  * sFreePageList;

    IDE_DASSERT( aPageListEntry != NULL );

    sTableOID = aPageListEntry->mTableOID;

    /* svpFreePageList_initEntryAtRuntime_malloc_Mutex.tc */
    IDU_FIT_POINT("svpFreePageList::initEntryAtRuntime::malloc::Mutex");
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_SM_SVP,
                                ID_SIZEOF(smpRuntimeEntry),
                                (void **)&(aPageListEntry->mRuntimeEntry),
                                IDU_MEM_FORCE )
             != IDE_SUCCESS);
    sState = 1;

    /*
     * BUG-25327 : [MDB] Free Page Size Class ������ Propertyȭ �ؾ� �մϴ�.
     */
    aPageListEntry->mRuntimeEntry->mSizeClassCount = smuProperty::getMemSizeClassCount();

    // Record Count ���� �ʱ�ȭ
    aPageListEntry->mRuntimeEntry->mInsRecCnt = 0;
    aPageListEntry->mRuntimeEntry->mDelRecCnt = 0;

    idlOS::snprintf( sBuffer, 128,
                     "TABLE_RECORD_COUNT_MUTEX_%"ID_XINT64_FMT"",
                     (ULong)sTableOID );

    IDE_TEST(aPageListEntry->mRuntimeEntry->mMutex.initialize(
                 sBuffer,
                 IDU_MUTEX_KIND_NATIVE,
                 IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    // FreePageList&Pool �ʱ�ȭ
    initializeFreePageListAndPool( aPageListEntry );

    // FreePagePool�� Mutex �ʱ�ȭ
    sFreePagePool = &(aPageListEntry->mRuntimeEntry->mFreePagePool);

    idlOS::snprintf( sBuffer, 128,
                     "FREE_PAGE_POOL_MUTEX_%"ID_XINT64_FMT"",
                     (ULong)sTableOID );

    IDE_TEST(sFreePagePool->mMutex.initialize( sBuffer,
                                               IDU_MUTEX_KIND_NATIVE,
                                               IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    // FreePageList�� Mutex �ʱ�ȭ
    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        sFreePageList =
            &(aPageListEntry->mRuntimeEntry->mFreePageList[sPageListID]);

        idlOS::snprintf( sBuffer, 128,
                         "FREE_PAGE_LIST_MUTEX_%"ID_XINT64_FMT"_%"ID_UINT32_FMT"",
                         (ULong)sTableOID, sPageListID );

        IDE_TEST(sFreePageList->mMutex.initialize( sBuffer,
                                                   IDU_MUTEX_KIND_NATIVE,
                                                   IDV_WAIT_INDEX_NULL )
                 != IDE_SUCCESS);

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( aPageListEntry->mRuntimeEntry )
                        == IDE_SUCCESS );
            aPageListEntry->mRuntimeEntry = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * PageListEntry���� FreePage�� ���õ� RuntimeEntry�� ���� ���� ��
 * FreePageList�� Mutex ����
 *
 * aPageListEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

IDE_RC svpFreePageList::finEntryAtRuntime( smpPageListEntry* aPageListEntry )
{
    UInt sPageListID;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aPageListEntry->mRuntimeEntry != NULL );

    // for FreePagePool
    IDE_TEST( aPageListEntry->mRuntimeEntry->mFreePagePool.mMutex.destroy()
              != IDE_SUCCESS );

    // fix BUG-13209
    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {

        // for FreePageList
        IDE_TEST(
            aPageListEntry->mRuntimeEntry->mFreePageList[sPageListID].mMutex.destroy()
            != IDE_SUCCESS);
    }

    // RuntimeEntry ����
    // To Fix BUG-14185
    IDE_TEST(aPageListEntry->mRuntimeEntry->mMutex.destroy() != IDE_SUCCESS );

    IDE_TEST(iduMemMgr::free(aPageListEntry->mRuntimeEntry) != IDE_SUCCESS);

    aPageListEntry->mRuntimeEntry = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PageListEntry ������ ���� FreePage�� ���õ� RuntimeEntry ���� ����
 *
 * aPageListEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

void svpFreePageList::initializeFreePageListAndPool(
    smpPageListEntry* aPageListEntry )
{
    UInt                  sPageListID;
    UInt                  sSizeClassID;
    smpFreePagePoolEntry* sFreePagePool;
    smpFreePageListEntry* sFreePageList;
    UInt                  sSizeClassCount;

    IDE_DASSERT( aPageListEntry != NULL );

    sSizeClassCount = SMP_SIZE_CLASS_COUNT( aPageListEntry->mRuntimeEntry );

    // for FreePagePool
    sFreePagePool = &(aPageListEntry->mRuntimeEntry->mFreePagePool);

    sFreePagePool->mPageCount = 0;
    sFreePagePool->mHead      = NULL;
    sFreePagePool->mTail      = NULL;

    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        // for FreePageList
        sFreePageList =
            &(aPageListEntry->mRuntimeEntry->mFreePageList[sPageListID]);

        for( sSizeClassID = 0;
             sSizeClassID < sSizeClassCount;
             sSizeClassID++ )
        {
            sFreePageList->mPageCount[sSizeClassID] = 0;
            sFreePageList->mHead[sSizeClassID]      = NULL;
            sFreePageList->mTail[sSizeClassID]      = NULL;
        }
    }

    return;
}

/**********************************************************************
 * FreeSlotList�� �ʱ�ȭ�Ѵ�.
 * ( Page���� ��� FreeSlot�� ����Ǿ� ������,
 *   FreeSlotList�� FreePageHeader�� ���̴� �۾��� �Ѵ�. )
 *
 * aPageListEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 * aPagePtr       : �ʱ�ȭ�Ϸ��� Page
 **********************************************************************/

void svpFreePageList::initializeFreeSlotListAtPage(
    scSpaceID         aSpaceID,
    smpPageListEntry* aPageListEntry,
    smpPersPage*      aPagePtr )
{
    UShort             sPageBodyOffset;
    smpFreePageHeader* sFreePageHeader;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aPagePtr != NULL );

    // Page���� ù Slot��ġ�� ���� Offset�� ���Ѵ�.
    sPageBodyOffset = (UShort)SMP_PERS_PAGE_BODY_OFFSET;

    if(aPagePtr->mHeader.mType == SMP_PAGETYPE_VAR)
    {
        // VarPage�� ��� �߰����� VarPageHeader��ŭ Offset�� ���Ѵ�.
        sPageBodyOffset += ID_SIZEOF(smpVarPageHeader);
    }

    sFreePageHeader = getFreePageHeader(aSpaceID, aPagePtr);

    // ù Slot�� Head�� ����Ѵ�.

    sFreePageHeader->mFreeSlotHead  = (SChar*)aPagePtr + sPageBodyOffset;
    sFreePageHeader->mFreeSlotTail  = (SChar*)aPagePtr + sPageBodyOffset
        + aPageListEntry->mSlotSize * (aPageListEntry->mSlotCount - 1);
    sFreePageHeader->mFreeSlotCount = aPageListEntry->mSlotCount;

    return;
}

/**********************************************************************
 * FreePagePool�κ��� FreePageList�� FREE_THRESHOLD��ŭ��
 * FreePages�� �Ҵ��� �� ������ �˻��ϰ� �����ϸ� �Ҵ��Ѵ�.
 *
 * aPageListEntry : �۾��� PageListEntry
 * aPageListID    : �Ҵ���� FreePageList�� ListID
 * aRc            : �Ҵ�޾Ҵ��� ����� ��ȯ�Ѵ�.
 **********************************************************************/

IDE_RC svpFreePageList::tryForAllocPagesFromPool(
    smpPageListEntry* aPageListEntry,
    UInt              aPageListID,
    idBool*           aIsPageAlloced )
{
    UInt                  sState = 0;
    smpFreePagePoolEntry* sFreePagePool;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aIsPageAlloced != NULL );

    sFreePagePool = &(aPageListEntry->mRuntimeEntry->mFreePagePool);

    IDE_DASSERT( sFreePagePool != NULL );

    *aIsPageAlloced = ID_FALSE;

    if(sFreePagePool->mPageCount >= SMP_MOVEPAGECOUNT_POOL2LIST)
    {
        // Pool�� ������ ���� ������ ��ŭ �ִ��� ����,
        // Pool�� lock�� ��� �۾��Ѵ�. List�� lock�� ���� �Լ����� ��´�.
        IDE_TEST(sFreePagePool->mMutex.lock(NULL) != IDE_SUCCESS);
        sState = 1;

        // lock�� ������� �ٸ� Tx�� ���� ����Ǿ����� �ٽ� Ȯ��
        if(sFreePagePool->mPageCount >= SMP_MOVEPAGECOUNT_POOL2LIST)
        {
            IDE_TEST(getPagesFromFreePagePool( aPageListEntry,
                                               aPageListID )
                     != IDE_SUCCESS);

            *aIsPageAlloced = ID_TRUE;
        }

        sState = 0;
        IDE_TEST(sFreePagePool->mMutex.unlock() != IDE_SUCCESS);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT( sFreePagePool->mMutex.unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePagePool�κ��� FreePageList�� FREE_THRESHOLD��ŭ�� FreePages �Ҵ�
 *
 * Pool�� ���� Lock�� ��� ���;� �Ѵ�.
 *
 * aPageListEntry : �۾��� PageListEntry
 * aPageListID    : �Ҵ���� FreePageList ID
 **********************************************************************/

IDE_RC svpFreePageList::getPagesFromFreePagePool(
    smpPageListEntry* aPageListEntry,
    UInt              aPageListID )
{
    UInt                  sState = 0;
    UInt                  sPageCounter;
    UInt                  sAssignCount;
    smpFreePageHeader*    sCurFreePageHeader;
    smpFreePageHeader*    sTailFreePageHeader;
    smpFreePagePoolEntry* sFreePagePool;
    smpFreePageListEntry* sFreePageList;
    UInt                  sEmptySizeClassID;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );

    sEmptySizeClassID = SMP_EMPTYPAGE_CLASSID( aPageListEntry->mRuntimeEntry );
    sAssignCount  = SMP_MOVEPAGECOUNT_POOL2LIST;

    sFreePagePool = &(aPageListEntry->mRuntimeEntry->mFreePagePool);
    sFreePageList = &(aPageListEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePagePool != NULL );
    IDE_DASSERT( sFreePageList != NULL );

    IDE_TEST(sFreePageList->mMutex.lock(NULL) != IDE_SUCCESS);
    sState = 1;

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount )
                 == ID_TRUE );

    IDE_DASSERT( isValidFreePageList(
                     sFreePageList->mHead[sEmptySizeClassID],
                     sFreePageList->mTail[sEmptySizeClassID],
                     sFreePageList->mPageCount[sEmptySizeClassID])
                 == ID_TRUE );

    // Before :
    // sFreePagePool : []-[]-[]-[]
    // After :
    // sFreePageList : []-[]
    // sFreePagePool :       []-[]

     // FreePagePool�� Head�� FreePageList�� �ű��
    sCurFreePageHeader  = sFreePagePool->mHead;
    sTailFreePageHeader = sFreePageList->mTail[sEmptySizeClassID];

    if(sTailFreePageHeader == NULL)
    {
        // List�� Tail�� NULL�̶�� List�� ����ִ�.
        IDE_DASSERT( sFreePageList->mHead[sEmptySizeClassID] == NULL );

        sFreePageList->mHead[sEmptySizeClassID] = sCurFreePageHeader;
    }
    else
    {
        IDE_DASSERT( sFreePageList->mHead[sEmptySizeClassID] != NULL );

        sTailFreePageHeader->mFreeNext     = sCurFreePageHeader;
        sCurFreePageHeader->mFreePrev      = sTailFreePageHeader;
    }

    // AssignCount��ŭ Skip�ϰ�
    for( sPageCounter = 0;
         sPageCounter < sAssignCount;
         sPageCounter++ )
    {
        IDE_DASSERT( sCurFreePageHeader != NULL );

        sCurFreePageHeader->mFreeListID  = aPageListID;
        sCurFreePageHeader->mSizeClassID = sEmptySizeClassID;

        sCurFreePageHeader = sCurFreePageHeader->mFreeNext;
    }

    if(sCurFreePageHeader == NULL)
    {
        // ������ FreePage ������ NULL�̶�� �������� Tail�̴�.
        sTailFreePageHeader = sFreePagePool->mTail;

        sFreePagePool->mTail = NULL;
    }
    else
    {
        sTailFreePageHeader = sCurFreePageHeader->mFreePrev;

        // ���� ����Ʈ�� �����ش�.
        sTailFreePageHeader->mFreeNext = NULL;
        sCurFreePageHeader->mFreePrev  = NULL;
    }

    // AssignCount��° Page�� FreePageList�� Tail�� ���
    sFreePageList->mTail[sEmptySizeClassID] = sTailFreePageHeader;
    sFreePagePool->mHead                    = sCurFreePageHeader;

    sFreePageList->mPageCount[sEmptySizeClassID] += sAssignCount;
    sFreePagePool->mPageCount                    -= sAssignCount;

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount )
                 == ID_TRUE );

    IDE_DASSERT( isValidFreePageList(
                     sFreePageList->mHead[sEmptySizeClassID],
                     sFreePageList->mTail[sEmptySizeClassID],
                     sFreePageList->mPageCount[sEmptySizeClassID])
                 == ID_TRUE );

    sState = 0;
    IDE_TEST(sFreePageList->mMutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageList->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePagePool�� FreePage �߰�
 *
 * aPageListEntry  : �����Ϸ��� PageListEntry
 * aFreePageHeader : �߰��Ϸ��� FreePageHeader
 **********************************************************************/

IDE_RC svpFreePageList::addPageToFreePagePool(
                                    smpPageListEntry*  aPageListEntry,
                                    smpFreePageHeader* aFreePageHeader )
{
    UInt                  sState = 0;
    smpFreePagePoolEntry* sFreePagePool;
    smpFreePageHeader*    sTailFreePageHeader;

    IDE_DASSERT(aPageListEntry != NULL);
    IDE_DASSERT(aFreePageHeader != NULL);

    sFreePagePool = &(aPageListEntry->mRuntimeEntry->mFreePagePool);

    IDE_DASSERT(sFreePagePool != NULL);

    // Pool�� ������ Page�� EmptyPage���� �Ѵ�.
    IDE_DASSERT(aFreePageHeader->mFreeSlotCount == aPageListEntry->mSlotCount);

    // FreePageHeader ���� �ʱ�ȭ
    aFreePageHeader->mFreeNext    = (smpFreePageHeader *)SM_NULL_PID;
    aFreePageHeader->mFreeListID  = SMP_POOL_PAGELISTID;
    aFreePageHeader->mSizeClassID = SMP_EMPTYPAGE_CLASSID( aPageListEntry->mRuntimeEntry );

    IDE_TEST(sFreePagePool->mMutex.lock(NULL) != IDE_SUCCESS);
    sState = 1;

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount )
                 == ID_TRUE );

    sTailFreePageHeader = sFreePagePool->mTail;

    // FreePagePool�� Tail�� ���δ�.
    if(sTailFreePageHeader == NULL)
    {
        IDE_DASSERT(sFreePagePool->mHead == NULL);

        sFreePagePool->mHead = aFreePageHeader;
    }
    else
    {
        IDE_DASSERT(sFreePagePool->mPageCount > 0);

        sTailFreePageHeader->mFreeNext = aFreePageHeader;
    }

    aFreePageHeader->mFreePrev = sTailFreePageHeader;
    sFreePagePool->mTail       = aFreePageHeader;
    sFreePagePool->mPageCount++;

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount )
                 == ID_TRUE );

    sState = 0;
    IDE_TEST(sFreePagePool->mMutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePagePool->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePage���� PageListEntry���� ����
 *
 * FreePage�� �����ϱ� ���ؼ��� �ٸ� Tx�� �����ؼ��� �ȵȴ�.
 * refineDB���� ���� �ϳ��� Tx�� �����ؾ� �ȴ�.
 *
 * aTrans         : �۾��� �����ϴ� Ʈ����� ��ü
 * aPageListEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

IDE_RC svpFreePageList::freePagesFromFreePagePoolToDB(
                                            void              * aTrans,
                                            scSpaceID           aSpaceID,
                                            smpPageListEntry  * aPageListEntry,
                                            UInt                aPages )
{
    UInt                    sState = 0;
    UInt                    sPageListID;
    UInt                    sPageCounter;
    UInt                    sRemovePageCount;
    scPageID                sCurPageID = SM_NULL_PID;
    scPageID                sRemovePrevID;
    scPageID                sFreeNextID;
    smpPersPage           * sRemoveHead;
    smpPersPage           * sRemoveTail = NULL;
    smpPersPageHeader     * sCurPageHeader;
    smpFreePageHeader     * sCurFreePageHeader;
    smpFreePageHeader     * sHeadFreePageHeader; // To fix BUG-28189
    smpFreePageHeader     * sTailFreePageHeader; // To fix BUG-28189
    smpFreePagePoolEntry  * sFreePagePool;
    smpAllocPageListEntry * sAllocPageList;
    UInt                    sLimitMinFreePageCount; // To fix BUG-28189
    idBool                  sIsLocked = ID_FALSE;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aPageListEntry != NULL );

    sFreePagePool    = &(aPageListEntry->mRuntimeEntry->mFreePagePool);
    sLimitMinFreePageCount = SMP_FREEPAGELIST_MINPAGECOUNT;

    IDE_DASSERT( sFreePagePool != NULL );

    // To fix BUG-28189
    // Ager �� �������� ���ϵ��� lock �� ��´�.
    IDE_TEST( sFreePagePool->mMutex.lock(NULL /* idvSQL* */) != IDE_SUCCESS );
    sIsLocked = ID_TRUE;

    // To fix BUG-28189
    // �ּ����� page count �� ���ų� ������ �� �̻� compact �� �� �ʿ䰡 ����.
    if( sFreePagePool->mPageCount <= sLimitMinFreePageCount )
    {
        sIsLocked = ID_FALSE;
        IDE_TEST( sFreePagePool->mMutex.unlock() != IDE_SUCCESS );
        IDE_CONT( no_more_need_compact );
    }

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount)
                 == ID_TRUE );

    // FreePagePool���� FREE_THRESHOLD��ŭ�� ����� �����Ѵ�.
    sRemovePageCount = sFreePagePool->mPageCount - sLimitMinFreePageCount;

    /* BUG-43464 compaction ��ɿ��� ������ ���� �����Ҽ� �ֵ��� �մϴ�.
     * #compact pages �� �ԷµǾ��� freePage ������ ���� ����
     * �Էµ� �� ��ŭ�� compaction ���� */
    if ( (sRemovePageCount > aPages) && (aPages > 0) )
    {
        sRemovePageCount = aPages;
    }
    else
    {
        /* nothing to do */
    }

    ideLog::log( IDE_SM_0,
                 "[COMPACT TABLE] TABLEOID(%"ID_UINT64_FMT") "
                 "FreePages %"ID_UINT32_FMT" / %"ID_UINT32_FMT"\n",
                 aPageListEntry->mTableOID, 
                 sRemovePageCount, 
                 sFreePagePool->mPageCount );

    // Before :
    // sFreePagePool : []-[]-[]-[]
    // After :
    // sFreePagePool :       []-[]

    sCurFreePageHeader = sFreePagePool->mHead;

    // BUG-27574 klockwork SM
    if ( sCurFreePageHeader == NULL )
    {
        sIsLocked = ID_FALSE;
        IDE_TEST( sFreePagePool->mMutex.unlock() != IDE_SUCCESS );
        IDE_CONT( skip_free_page );
    }
    else
    {
        /* nothing to do */
    }

    // To fix BUG-28189
    // �����ϰ��� �ϴ� FreePagePool �� ��ġ�� ����صд�.
    sHeadFreePageHeader = sFreePagePool->mHead;
    sTailFreePageHeader = sFreePagePool->mHead;

    // FreePagePool�� Head���� sRemovePageCount��ŭ
    // sRemoveHead~sRemoveTail�� ����� ����
    IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID,
                                               sCurFreePageHeader->mSelfPageID,
                                               (void**)&sRemoveHead )
                   == IDE_SUCCESS,
                   "TableOID : %"ID_UINT64_FMT"\n"
                   "SpaceID  : %"ID_UINT32_FMT"\n"
                   "PageID   : %"ID_UINT32_FMT"\n",
                   aPageListEntry->mTableOID,
                   aSpaceID,
                   sCurFreePageHeader->mSelfPageID );

    sRemovePrevID = SM_NULL_PID;

    // To fix BUG-28189
    // sTailFreePageHeader �� ��ġ�� �� ������ �ű��.
    // �����ϰ��� �ϴ� count ���� 1 �۰� �Űܾ� list �� ������ �ű� �� �ִ�.
    // �׷��� ������ �����ϰ��� �ϴ� list �� ������ ����� �ȴ�.
    for( sPageCounter = 0;
         sPageCounter < (sRemovePageCount - 1);
         sPageCounter++ )
    {
        sTailFreePageHeader = sTailFreePageHeader->mFreeNext;
    }

    // To fix BUG-28189
    // ������ sRemoveTail�� ���� Page�� FreePagePool�� Head�� �Ǿ�� �Ѵ�.
    sFreePagePool->mHead            = sTailFreePageHeader->mFreeNext;
    sFreePagePool->mHead->mFreePrev = NULL;
    sFreePagePool->mPageCount      -= sRemovePageCount;
    sTailFreePageHeader->mFreeNext  = NULL;

    /* �κ����� ������ ��ȯ�� �ֱ� ������ ���� �������� ���� limit ����
       ũ�ų�(�־��� #page ��ŭ free), ���⸸(max�� free) �ϸ� �ȴ� */
    IDE_ERROR_MSG( sFreePagePool->mPageCount >= sLimitMinFreePageCount,
                   "FreePageCount  : %"ID_UINT32_FMT"\n"
                   "LimitPageCount : %"ID_UINT32_FMT"\n",
                   sFreePagePool->mPageCount,
                   sLimitMinFreePageCount );

    IDE_DASSERT( isValidFreePageList( sHeadFreePageHeader,
                                      sTailFreePageHeader,
                                      sRemovePageCount )
                 == ID_TRUE );

    IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                      sFreePagePool->mTail,
                                      sFreePagePool->mPageCount )
                 == ID_TRUE );

    sIsLocked = ID_FALSE;
    IDE_TEST( sFreePagePool->mMutex.unlock() != IDE_SUCCESS );

    // To fix BUG-28189
    // FreePageList ���� �����ϰ��� �ϴ� List �� ��������� sState �� 1 �̴�.
    sState = 1;

    // To fix BUG-28189
    // compact table �� Ager ���̿� ���ü� ��� ���� �ʽ��ϴ�.
    for( sPageCounter = 0;
         sPageCounter < sRemovePageCount;
         sPageCounter++ )
    {
        IDE_ERROR( sCurFreePageHeader != NULL );

        sCurPageID = sCurFreePageHeader->mSelfPageID;

        // FreePage�� �ݳ��ϱ� ���� AllocPageList������ �ش� Page�� �����ؾ� �Ѵ�.

        IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID,
                                                   sCurPageID,
                                                   (void**)&sCurPageHeader )
                       == IDE_SUCCESS,
                       "TableOID : %"ID_UINT64_FMT"\n"
                       "SpaceID  : %"ID_UINT32_FMT"\n"
                       "PageID   : %"ID_UINT32_FMT"\n",
                       aPageListEntry->mTableOID,
                       aSpaceID,
                       sCurPageID );

        sPageListID    = sCurPageHeader->mAllocListID;
        sAllocPageList = &(aPageListEntry->mRuntimeEntry->mAllocPageList[sPageListID]);

        IDE_ERROR_MSG( sCurFreePageHeader->mFreeSlotCount
                       == aPageListEntry->mSlotCount,
                       "FreePage.FreeSlotCount : %"ID_UINT32_FMT"\n"
                       "PageList.SlotCount     : %"ID_UINT32_FMT"\n",
                       sCurFreePageHeader->mFreeSlotCount,
                       aPageListEntry->mSlotCount );

        // DB�� �ݳ��� PageList�� ��ũ�� �����ϱ� ���� Next�� ���Ѵ�.
        if(sPageCounter == sRemovePageCount - 1)
        {
            // sRemoveTail�� Next�� NULL
            sFreeNextID = SM_NULL_PID;
        }
        else
        {
            IDE_DASSERT( sCurFreePageHeader->mFreeNext != NULL );

            sFreeNextID = sCurFreePageHeader->mFreeNext->mSelfPageID;
        }

        IDE_TEST( svpAllocPageList::removePage( aSpaceID,
                                                sAllocPageList,
                                                sCurPageID )
                  != IDE_SUCCESS );

        sCurPageHeader->mPrevPageID = sRemovePrevID;
        // ���� RemovePage�� RemovePrevID�� CurPageID�̴�.
        sRemovePrevID  = sCurPageID;

        // �ݳ��� FreePage�� sRemoveHead ~ sRemoveTail �� ����Ʈ�� ����� �ݳ��Ѵ�.

        sRemoveTail = (smpPersPage*)sCurPageHeader;
        sCurPageHeader->mNextPageID = sFreeNextID;

        sCurFreePageHeader = sCurFreePageHeader->mFreeNext;

        // To fix BUG-28189
        // compact table �� Ager ���̿� ���ü� ��� ���� �ʽ��ϴ�.
    }

    IDE_DASSERT( svpAllocPageList::isValidPageList( aSpaceID,
                                                    sRemoveHead->mHeader.mSelfPageID,
                                                    sRemoveTail->mHeader.mSelfPageID,
                                                    sRemovePageCount )
                 == ID_TRUE );

    // sRemoveHead~sRemoveTail�� DB�� �ݳ�
    IDE_TEST(svmManager::freePersPageList( aTrans,
                                           aSpaceID,
                                           sRemoveHead,
                                           sRemoveTail ) != IDE_SUCCESS);

    IDE_EXCEPTION_CONT( skip_free_page );

    IDE_EXCEPTION_CONT( no_more_need_compact );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if ( sIsLocked == ID_TRUE )
    {
        sIsLocked = ID_FALSE;
        IDE_ASSERT( sFreePagePool->mMutex.unlock() == IDE_SUCCESS );
    }
    else 
    {
        /* ������ sState �� 1�̷��� lock �� Ǯ�� �־�� �Ѵ�. */
        IDE_DASSERT( sState == 1 ); 
    }

    if( sState == 1 )
    {
        // To fix BUG-28189
        // freePage �� �����ϴ� ���� ���ܰ� �߻��ϸ� FreePagePool �� �ٽ� ����������� �Ѵ�.
        IDE_ASSERT( sFreePagePool->mMutex.lock(NULL /* idvSQL* */) == IDE_SUCCESS );

        sFreePagePool->mHead->mFreePrev = sTailFreePageHeader;
        sTailFreePageHeader->mFreeNext  = sFreePagePool->mHead;
        sFreePagePool->mHead            = sHeadFreePageHeader;
        sFreePagePool->mPageCount      += sRemovePageCount;
            
        IDE_DASSERT( isValidFreePageList( sFreePagePool->mHead,
                                          sFreePagePool->mTail,
                                          sFreePagePool->mPageCount )
                     == ID_TRUE );

        IDE_ASSERT( sFreePagePool->mMutex.unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageHeader ���� Ȥ�� DB���� ������ �Ҵ�� �ʱ�ȭ
 *
 * aFreePageHeader : �ʱ�ȭ�Ϸ��� FreePageHeader
 **********************************************************************/

void svpFreePageList::initializeFreePageHeader(
                                    smpFreePageHeader* aFreePageHeader )
{
    IDE_DASSERT( aFreePageHeader != NULL );

    aFreePageHeader->mFreePrev      = NULL;
    aFreePageHeader->mFreeNext      = NULL;
    aFreePageHeader->mFreeSlotHead  = NULL;
    aFreePageHeader->mFreeSlotTail  = NULL;
    aFreePageHeader->mFreeSlotCount = 0;
    aFreePageHeader->mSizeClassID   = SMP_SIZECLASSID_NULL;
    aFreePageHeader->mFreeListID    = SMP_PAGELISTID_NULL;

    return;
}

/**********************************************************************
 * PageListEntry�� ��� FreePageHeader �ʱ�ȭ
 *
 * aPageListEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 **********************************************************************/

void svpFreePageList::initAllFreePageHeader( scSpaceID         aSpaceID,
                                             smpPageListEntry* aPageListEntry )
{
    scPageID sPageID;

    IDE_DASSERT( aPageListEntry != NULL );

    sPageID = svpManager::getFirstAllocPageID(aPageListEntry);

    while(sPageID != SM_NULL_PID)
    {
        initializeFreePageHeader( getFreePageHeader(aSpaceID, sPageID) );

        sPageID = svpManager::getNextAllocPageID( aSpaceID,
                                                  aPageListEntry,
                                                  sPageID );
    }

    return;
}

/**********************************************************************
 * FreePageHeader �ʱ�ȭ
 *
 * aPageID : �ʱ�ȭ�Ϸ��� Page�� ID
 **********************************************************************/

IDE_RC svpFreePageList::initializeFreePageHeaderAtPCH( scSpaceID aSpaceID,
                                                       scPageID  aPageID )
{
    SChar              sBuffer[128];
    svmPCH*            sPCH;
    smpFreePageHeader* sFreePageHeader;

    // FreePageHeader �ʱ�ȭ

    // BUGBUG : aPageID ���� �˻� �ʿ�!!
    sPCH = smmManager::getPCH(aSpaceID, aPageID);

    IDE_DASSERT( sPCH != NULL );

    /* svpFreePageList_initializeFreePageHeaderAtPCH_malloc_FreePageHeader.tc */
    IDU_FIT_POINT("svpFreePageList::initializeFreePageHeaderAtPCH::malloc::FreePageHeader");
    IDE_TEST(iduMemMgr::malloc( IDU_MEM_SM_SVP,
                                ID_SIZEOF(smpFreePageHeader),
                                (void**)&(sPCH->mFreePageHeader),
                                IDU_MEM_FORCE )
             != IDE_SUCCESS);

    sFreePageHeader = (smpFreePageHeader*)(sPCH->mFreePageHeader);

    sFreePageHeader->mSelfPageID    = aPageID;

    initializeFreePageHeader( sFreePageHeader );

    idlOS::snprintf(sBuffer, 128, "FREE_PAGE_MUTEX_%"ID_XINT64_FMT"", aPageID);

    IDE_TEST(sFreePageHeader->mMutex.initialize( sBuffer,
                                                 IDU_MUTEX_KIND_NATIVE,
                                                 IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageHeader ����
 *
 * aPageID : �����Ϸ��� Page�� ID
 **********************************************************************/

IDE_RC svpFreePageList::destroyFreePageHeaderAtPCH( scSpaceID aSpaceID,
                                                    scPageID  aPageID )
{
    svmPCH*            sPCH;
    smpFreePageHeader* sFreePageHeader;

    sFreePageHeader = getFreePageHeader(aSpaceID, aPageID);

    IDE_DASSERT( sFreePageHeader != NULL );

    IDE_TEST(sFreePageHeader->mMutex.destroy() != IDE_SUCCESS);
    IDE_TEST(iduMemMgr::free(sFreePageHeader) != IDE_SUCCESS);

    sPCH = smmManager::getPCH(aSpaceID, aPageID);

    IDE_DASSERT( sPCH != NULL );

    sPCH->mFreePageHeader = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePage�� ���� SizeClass ����
 *
 * SizeClass�� �����ϱ� ���� �ش� Page�� lock�� ��� ���;��ϸ�,
 * List/Pool�� �ű�� �۾��� �Ҷ� lock�� ��� �۾��Ѵ�.
 *
 * aTrans          : �۾��ϴ� Ʈ����� ��ü
 * aPageListEntry  : FreePage�� �Ҽ� PageListEntry
 * aFreePageHeader : SizeClass �����Ϸ��� FreePageHeader
 **********************************************************************/
IDE_RC svpFreePageList::modifyPageSizeClass( void*              aTrans,
                                             smpPageListEntry*  aPageListEntry,
                                             smpFreePageHeader* aFreePageHeader )
{
#ifdef DEBUG
    UInt                  sOldSizeClassID;
#endif
    UInt                  sNewSizeClassID;
    UInt                  sOldPageListID;
    UInt                  sNewPageListID;
    smpFreePageListEntry* sFreePageList;
    UInt                  sSizeClassCount;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aFreePageHeader != NULL );

    IDE_ASSERT( aFreePageHeader->mFreeListID != SMP_POOL_PAGELISTID );

#ifdef DEBUG
    sOldSizeClassID = aFreePageHeader->mSizeClassID;
#endif
    sOldPageListID  = aFreePageHeader->mFreeListID;
    sSizeClassCount = SMP_SIZE_CLASS_COUNT( aPageListEntry->mRuntimeEntry );

    // ����� SizeClassID ���� ���Ѵ�.
    sNewSizeClassID = getSizeClass( aPageListEntry->mRuntimeEntry,
                                    aPageListEntry->mSlotCount,
                                    aFreePageHeader->mFreeSlotCount );

    IDE_ASSERT( sNewSizeClassID < sSizeClassCount );

    if( sOldPageListID == SMP_PAGELISTID_NULL )
    {
        IDE_DASSERT(sOldSizeClassID == SMP_SIZECLASSID_NULL);

        // Slot��ü�� ������̴� Page�� FreePageList�� �����ٸ�
        // ���� Ʈ������� RSGroupID���� FreePageList�� ����Ѵ�.

        smLayerCallback::allocRSGroupID( aTrans, &sNewPageListID );
    }
    else
    {
        sNewPageListID = sOldPageListID;
    }

    if( (sNewSizeClassID != aFreePageHeader->mSizeClassID) ||
        (aFreePageHeader->mFreeSlotCount == 0) ||
        (aFreePageHeader->mFreeSlotCount == aPageListEntry->mSlotCount) )
    {
        sFreePageList =
            &(aPageListEntry->mRuntimeEntry->mFreePageList[sNewPageListID]);

        // ���� Free Page�� ������ ����Ʈ�� �־��°�?
        if(sOldPageListID != SMP_PAGELISTID_NULL)
        {
            IDE_DASSERT(sOldSizeClassID != SMP_SIZECLASSID_NULL);

            // ���� SizeClass���� Page �и�
            IDE_TEST( removePageFromFreePageList(aPageListEntry,
                                                 sOldPageListID,
                                                 aFreePageHeader->mSizeClassID,
                                                 aFreePageHeader)
                      != IDE_SUCCESS );
        }

        // FreeSlot�� �ϳ��� �ִ°�?
        // FreeSlot�� ���ٸ� FreePage�� �ƴϹǷ� FreePageList�� ������� �ʴ´�.
        if(aFreePageHeader->mFreeSlotCount != 0)
        {
            if((aFreePageHeader->mFreeSlotCount == aPageListEntry->mSlotCount) &&
               (sFreePageList->mPageCount[sNewSizeClassID] >= SMP_FREEPAGELIST_MINPAGECOUNT))
            {
                // Page�� Slot ��ü�� FreeSlot�̸�
                // aFreePageHeader�� ���� EmptyPage�̴�.

                // FreePageList�� PageCount�� FREE_THRESHOLD���� ���ٸ�
                // FreePagePool�� ����Ѵ�.
                IDE_TEST( addPageToFreePagePool( aPageListEntry,
                                                 aFreePageHeader )
                          != IDE_SUCCESS );
            }
            else
            {
                // EmptyPage�� �ƴϰų� FreePageList�� ���� ������ �ִ�.

                // BUGBUG: addPageToFreePageListHead�� ���� Emergency��
                //         FreeSlot�� ����Ǿ� INSERT ������ SELECT ������
                //         ���� �ʴ� ���� �߻����� �켱 ������ Tail�� ���
                //         ���� Head�� �̵��ϴ� ���� ���ɿ� ������,
                //         Ȥ�� Emergency�� FreeSlot��� DB���� Emergency��
                //         FreePage�� ���� ������� ������ �ʿ�����.

                IDE_TEST( addPageToFreePageListTail( aPageListEntry,
                                                     sNewPageListID,
                                                     sNewSizeClassID,
                                                     aFreePageHeader )
                          != IDE_SUCCESS );

                /*
                if(sOldPageListID == SMP_PAGELISTID_NULL ||
                   sOldSizeClassID < sNewSizeClassID)
                {
                    // SizeClassID�� ���������� �̵��ߴٸ�
                    // ���� SizeClass�� FreeSlotCount�� �ּҰ��̹Ƿ�
                    // FreeSlot�� ���� FreePage�� List�� �ڿ� ���̰�,
                    IDE_TEST( addPageToFreePageListTail( aPageListEntry,
                                                         sNewPageListID,
                                                         sNewSizeClassID,
                                                         aFreePageHeader )
                              != IDE_SUCCESS );
                }
                else
                {
                    // SizeClassID�� ���������� �̵��ߴٸ�
                    // ���� SizeClass�� FreeSlotCount�� �ִ밪�̹Ƿ�
                    // FreeSlot�� ���� FreePage�� List�� �տ� ���̴°�
                    // allocSlot�� �տ������� �ϱ⶧���� ȿ�����̴�.
                    IDE_TEST( addPageToFreePageListTail( aPageListEntry,
                                                         sNewPageListID,
                                                         sNewSizeClassID,
                                                         aFreePageHeader )
                              != IDE_SUCCESS );
                }
                */
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageList���� FreePage ����
 *
 * aPageListEntry  : FreePage�� �Ҽ� PageListEntry
 * aPageListID     : FreePage�� �Ҽ� FreePageList ID
 * aSizeClassID    : FreePage�� �Ҽ� SizeClass ID
 * aFreePageHeader : �����Ϸ��� FreePageHeader
 **********************************************************************/

IDE_RC svpFreePageList::removePageFromFreePageList(
    smpPageListEntry*  aPageListEntry,
    UInt               aPageListID,
    UInt               aSizeClassID,
    smpFreePageHeader* aFreePageHeader )
{
    UInt                  sState = 0;
    smpFreePageListEntry* sFreePageList;
    smpFreePageHeader*    sPrevFreePageHeader;
    smpFreePageHeader*    sNextFreePageHeader;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aFreePageHeader != NULL );

    sFreePageList
        = &(aPageListEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePageList != NULL );

    IDE_TEST( sFreePageList->mMutex.lock(NULL) != IDE_SUCCESS );
    sState = 1;

    IDE_DASSERT( aSizeClassID == aFreePageHeader->mSizeClassID );
    IDE_DASSERT( aPageListID  == aFreePageHeader->mFreeListID  );

    sPrevFreePageHeader = aFreePageHeader->mFreePrev;
    sNextFreePageHeader = aFreePageHeader->mFreeNext;

    if(sPrevFreePageHeader == NULL)
    {
        // FreePrev�� ���ٸ� HeadPage�̴�.
        IDE_DASSERT( sFreePageList->mHead[aSizeClassID] == aFreePageHeader );

        sFreePageList->mHead[aSizeClassID] = sNextFreePageHeader;
    }
    else
    {
        IDE_DASSERT( sFreePageList->mHead[aSizeClassID] != aFreePageHeader );

        // ���� ��ũ�� ���´�.
        sPrevFreePageHeader->mFreeNext = sNextFreePageHeader;
    }

    if(sNextFreePageHeader == NULL)
    {
        // FreeNext�� ���ٸ� TailPage�̴�.
        IDE_DASSERT( sFreePageList->mTail[aSizeClassID] == aFreePageHeader );

        sFreePageList->mTail[aSizeClassID] = sPrevFreePageHeader;
    }
    else
    {
        IDE_DASSERT( sFreePageList->mTail[aSizeClassID] != aFreePageHeader );

        // ���� ��ũ�� ���´�.
        sNextFreePageHeader->mFreePrev = sPrevFreePageHeader;
    }

    aFreePageHeader->mFreePrev    = NULL;
    aFreePageHeader->mFreeNext    = NULL;
    aFreePageHeader->mFreeListID  = SMP_PAGELISTID_NULL;
    aFreePageHeader->mSizeClassID = SMP_SIZECLASSID_NULL;

    sFreePageList->mPageCount[aSizeClassID]--;

    IDE_DASSERT( isValidFreePageList( sFreePageList->mHead[aSizeClassID],
                                      sFreePageList->mTail[aSizeClassID],
                                      sFreePageList->mPageCount[aSizeClassID] )
                 == ID_TRUE );

    sState = 0;
    IDE_TEST( sFreePageList->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageList->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageList�� Tail�� FreePage ���
 *
 * aPageListEntry  : FreePage�� �Ҽ� PageListEntry
 * aPageListID     : FreePage�� �Ҽ� FreePageList ID
 * aSizeClassID    : FreePage�� �Ҽ� SizeClass ID
 * aFreePageHeader : ����Ϸ��� FreePageHeader
 **********************************************************************/

IDE_RC svpFreePageList::addPageToFreePageListTail(
    smpPageListEntry*  aPageListEntry,
    UInt               aPageListID,
    UInt               aSizeClassID,
    smpFreePageHeader* aFreePageHeader )
{
    UInt                  sState = 0;
    smpFreePageListEntry* sFreePageList;
    smpFreePageHeader*    sTailPageHeader;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aFreePageHeader != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aSizeClassID < SMP_SIZE_CLASS_COUNT(aPageListEntry->mRuntimeEntry) );

    sFreePageList =
        &(aPageListEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePageList != NULL );

    // aFreePageHeader�� ��ȿ���� �˻�
    IDE_DASSERT( aFreePageHeader->mFreeSlotCount > 0 );
    IDE_DASSERT( aFreePageHeader->mFreeListID == SMP_PAGELISTID_NULL );
    IDE_DASSERT( aFreePageHeader->mSizeClassID == SMP_SIZECLASSID_NULL );

    IDE_TEST( sFreePageList->mMutex.lock(NULL) != IDE_SUCCESS );
    sState = 1;

    sTailPageHeader = sFreePageList->mTail[aSizeClassID];

    if(sTailPageHeader == NULL)
    {
        IDE_DASSERT( sFreePageList->mHead[aSizeClassID] == NULL );

        // Head�� ����ִٸ� FreePage�� Head/Tail�� �� ä���� �Ѵ�.
        sFreePageList->mHead[aSizeClassID] = aFreePageHeader;
    }
    else /* sTailPageHeader != NULL */
    {
        IDE_DASSERT( sFreePageList->mHead[aSizeClassID] != NULL );

        sTailPageHeader->mFreeNext = aFreePageHeader;
    }

    sFreePageList->mTail[aSizeClassID] = aFreePageHeader;

    aFreePageHeader->mFreePrev    = sTailPageHeader;
    aFreePageHeader->mFreeNext    = (smpFreePageHeader *)SM_NULL_PID;
    aFreePageHeader->mSizeClassID = aSizeClassID;
    aFreePageHeader->mFreeListID  = aPageListID;

    sFreePageList->mPageCount[aSizeClassID]++;

    IDE_DASSERT( isValidFreePageList( sFreePageList->mHead[aSizeClassID],
                                      sFreePageList->mTail[aSizeClassID],
                                      sFreePageList->mPageCount[aSizeClassID] )
                 == ID_TRUE );

    sState = 0;
    IDE_TEST( sFreePageList->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageList->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageList�� Head�� FreePage ���
 *
 * aPageListEntry  : FreePage�� �Ҽ� PageListEntry
 * aPageListID     : FreePage�� �Ҽ� FreePageList ID
 * aSizeClassID    : FreePage�� �Ҽ� SizeClass ID
 * aFreePageHeader : ����Ϸ��� FreePage
 **********************************************************************/

IDE_RC svpFreePageList::addPageToFreePageListHead(
    smpPageListEntry*  aPageListEntry,
    UInt               aPageListID,
    UInt               aSizeClassID,
    smpFreePageHeader* aFreePageHeader )
{
    UInt                  sState = 0;
    smpFreePageListEntry* sFreePageList;
    smpFreePageHeader*    sHeadPageHeader;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aFreePageHeader != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aSizeClassID < SMP_SIZE_CLASS_COUNT(aPageListEntry->mRuntimeEntry) );

    sFreePageList =
        &(aPageListEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePageList != NULL );
    IDE_DASSERT( aFreePageHeader->mFreeSlotCount > 0 );
    IDE_DASSERT( aFreePageHeader->mFreeListID == SMP_PAGELISTID_NULL );
    IDE_DASSERT( aFreePageHeader->mSizeClassID == SMP_SIZECLASSID_NULL );

    IDE_TEST( sFreePageList->mMutex.lock(NULL) != IDE_SUCCESS );
    sState = 1;

    sHeadPageHeader = sFreePageList->mHead[aSizeClassID];

    if(sHeadPageHeader == NULL)
    {
        IDE_DASSERT( sFreePageList->mTail[aSizeClassID] == NULL );

        // Head�� ����ִٸ� FreePage�� Head/Tail�� �� ä���� �Ѵ�.
        sFreePageList->mTail[aSizeClassID] = aFreePageHeader;
    }
    else /* sHeadPageHeader != NULL */
    {
        IDE_DASSERT( sFreePageList->mTail[aSizeClassID] != NULL );

        sHeadPageHeader->mFreePrev = aFreePageHeader;
    }

    sFreePageList->mHead[aSizeClassID] = aFreePageHeader;

    aFreePageHeader->mFreePrev    = (smpFreePageHeader *)SM_NULL_PID;
    aFreePageHeader->mFreeNext    = sHeadPageHeader;
    aFreePageHeader->mSizeClassID = aSizeClassID;
    aFreePageHeader->mFreeListID  = aPageListID;

    sFreePageList->mPageCount[aSizeClassID]++;

    IDE_DASSERT( isValidFreePageList( sFreePageList->mHead[aSizeClassID],
                                      sFreePageList->mTail[aSizeClassID],
                                      sFreePageList->mPageCount[aSizeClassID] )
                 == ID_TRUE );

    sState = 0;
    IDE_TEST( sFreePageList->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageList->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * refineDB �� �� Page�� Scan�ϸ鼭 FreePage�� ��� 0��° FreePageList��
 * ����� ��, buildFreePageList�� 0��° FreePageList���� �� FreePageList��
 * FreePage���� ������ �ش�.
 *
 * aPageListEntry : �˻��Ϸ��� PageListEntry
 * aRecordCount   : Record ����
 **********************************************************************/

IDE_RC svpFreePageList::addPageToFreePageListAtInit(
    smpPageListEntry*  aPageListEntry,
    smpFreePageHeader* aFreePageHeader )
{
    UInt sSizeClassID;

    IDE_DASSERT( aPageListEntry != NULL );
    IDE_DASSERT( aFreePageHeader != NULL );
    IDE_DASSERT( aFreePageHeader->mFreeSlotCount > 0 );
    IDE_ASSERT( aFreePageHeader->mFreeListID == SMP_PAGELISTID_NULL );

    sSizeClassID = getSizeClass(aPageListEntry->mRuntimeEntry,
                                aPageListEntry->mSlotCount,
                                aFreePageHeader->mFreeSlotCount);

    IDE_TEST( addPageToFreePageListTail( aPageListEntry,
                                         0,
                                         sSizeClassID,
                                         aFreePageHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PrivatePageList�� FreePage���� aPageListEntry�� �߰��Ѵ�.
 *
 * aTrans           : �۾��ϴ� Ʈ����� ��ü
 * aPageListEntry   : �߰��� �� ���̺��� PageListEntry
 * aHeadFreePage    : �߰��Ϸ��� FreePage���� Head
 **********************************************************************/

IDE_RC svpFreePageList::addFreePagesToTable( void*              aTrans,
                                             smpPageListEntry*  aPageListEntry,
                                             smpFreePageHeader* aFreePageHead )
{
    smpFreePageHeader* sFreePageHeader = NULL;
    smpFreePageHeader* sNxtFreePageHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aPageListEntry != NULL );

    sNxtFreePageHeader = aFreePageHead;

    while(sNxtFreePageHeader != NULL)
    {
        sFreePageHeader    = sNxtFreePageHeader;
        sNxtFreePageHeader = sFreePageHeader->mFreeNext;

        sFreePageHeader->mFreeListID  = SMP_PAGELISTID_NULL;
        sFreePageHeader->mSizeClassID = SMP_SIZECLASSID_NULL;

        IDE_TEST( modifyPageSizeClass( aTrans,
                                       aPageListEntry,
                                       sFreePageHeader )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PrivatePageList�� FreePage ����
 *
 * aCurPID  : FreeNext�� ������ PageID
 * aPrevPID : FreePrev�� PageID
 * aNextPID : FreeNext�� PageID
 **********************************************************************/

void svpFreePageList::addFreePageToPrivatePageList( scSpaceID aSpaceID,
                                                    scPageID  aCurPID,
                                                    scPageID  aPrevPID,
                                                    scPageID  aNextPID )
{
    smpFreePageHeader* sCurFreePageHeader;
    smpFreePageHeader* sPrevFreePageHeader;
    smpFreePageHeader* sNextFreePageHeader;

    IDE_DASSERT(aCurPID != SM_NULL_PID);

    sCurFreePageHeader = getFreePageHeader(aSpaceID, aCurPID);

    if(aPrevPID == SM_NULL_PID)
    {
        sPrevFreePageHeader = NULL;
    }
    else
    {
        sPrevFreePageHeader = getFreePageHeader(aSpaceID, aPrevPID);
    }

    if(aNextPID == SM_NULL_PID)
    {
        sNextFreePageHeader = NULL;
    }
    else
    {
        sNextFreePageHeader = getFreePageHeader(aSpaceID, aNextPID);
    }

    // FreePageHeader ���� �ʱ�ȭ
    sCurFreePageHeader->mFreeListID  = SMP_PRIVATE_PAGELISTID;
    sCurFreePageHeader->mSizeClassID = SMP_PRIVATE_SIZECLASSID;
    sCurFreePageHeader->mFreePrev    = sPrevFreePageHeader;
    sCurFreePageHeader->mFreeNext    = sNextFreePageHeader;

    return;
}

/**********************************************************************
 * PrivatePageList���� FixedFreePage ����
 *
 * aPrivatePageList : ���ŵ� PrivatePageList
 * aFreePageHeader  : ������ FreePageHeader
 **********************************************************************/

void svpFreePageList::removeFixedFreePageFromPrivatePageList(
    smpPrivatePageListEntry* aPrivatePageList,
    smpFreePageHeader*       aFreePageHeader )
{
    IDE_DASSERT(aPrivatePageList != NULL);
    IDE_DASSERT(aFreePageHeader != NULL);

    if(aFreePageHeader->mFreePrev == NULL)
    {
        // Prev�� ���ٸ� Head�̴�.
        IDE_DASSERT( aPrivatePageList->mFixedFreePageHead == aFreePageHeader );

        aPrivatePageList->mFixedFreePageHead = aFreePageHeader->mFreeNext;
    }
    else
    {
        IDE_DASSERT( aPrivatePageList->mFixedFreePageHead != NULL );

        aFreePageHeader->mFreePrev->mFreeNext = aFreePageHeader->mFreeNext;
    }

    if(aFreePageHeader->mFreeNext == NULL)
    {
        // Next�� ���ٸ� Tail�̴�.
        IDE_DASSERT( aPrivatePageList->mFixedFreePageTail == aFreePageHeader );

        aPrivatePageList->mFixedFreePageTail = aFreePageHeader->mFreePrev;
    }
    else
    {
        IDE_DASSERT( aPrivatePageList->mFixedFreePageTail != NULL );

        aFreePageHeader->mFreeNext->mFreePrev = aFreePageHeader->mFreePrev;
    }

    initializeFreePageHeader(aFreePageHeader);

    return;
}

#ifdef DEBUG
/**********************************************************************
 * aHeadFreePage~aTailFreePage������ FreePageList�� ������ �ùٸ��� �˻��Ѵ�.
 *
 * aHeadFreePage : �˻��Ϸ��� List�� Head
 * aTailFreePage : �˻��Ϸ��� List�� Tail
 * aPageCount    : �˻��Ϸ��� List�� Page�� ����
 **********************************************************************/
idBool svpFreePageList::isValidFreePageList( smpFreePageHeader* aHeadFreePage,
                                             smpFreePageHeader* aTailFreePage,
                                             vULong             aPageCount )
{
    idBool             sIsValid;
    vULong             sPageCount = 0;
    smpFreePageHeader* sCurFreePage = NULL;
    smpFreePageHeader* sNxtFreePage;

    if ( iduProperty::getEnableRecTest() == 1 )
    {
        sIsValid = ID_FALSE;

        sNxtFreePage = aHeadFreePage;

        while(sNxtFreePage != NULL)
        {
            sCurFreePage = sNxtFreePage;

            sPageCount++;

            sNxtFreePage = sCurFreePage->mFreeNext;
        }

        if(sCurFreePage == aTailFreePage)
        {
            if(sPageCount == aPageCount)
            {
                sIsValid = ID_TRUE;
            }
        }
    }
    else
    {
       sIsValid = ID_TRUE;
    }

     return sIsValid;
}
#endif
/**********************************************************************
 * PCH������ �ִ� aPageID�� FreePageHeader�� �����Ѵ�.
 *
 * aPageID : FreePageHeader�� �ʿ��� PageID
 **********************************************************************/

smpFreePageHeader* svpFreePageList::getFreePageHeader( scSpaceID aSpaceID,
                                                       scPageID aPageID )
{
    svmPCH* sPCH;

    // BUGBUG : aPageID�� ���� DASSERT �ʿ�
    
    sPCH = (svmPCH*)(smmManager::getPCH(aSpaceID,
                                        aPageID));

    IDE_DASSERT( sPCH != NULL );

    return (smpFreePageHeader*)(sPCH->mFreePageHeader);
}
