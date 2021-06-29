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
#include <smuProperty.h>
#include <svmManager.h>

/**********************************************************************
 * AllocPageList�� �ʱ�ȭ�Ѵ�.
 *
 * aAllocPageList : �ʱ�ȭ�Ϸ��� AllocPageList
 **********************************************************************/

void svpAllocPageList::initializePageListEntry(
    smpAllocPageListEntry* aAllocPageList )
{
    UInt sPageListID;

    IDE_DASSERT(aAllocPageList != NULL);

    for(sPageListID = 0; sPageListID < SMP_PAGE_LIST_COUNT; sPageListID++)
    {
        // AllocPageList �ʱ�ȭ
        aAllocPageList[sPageListID].mPageCount  = 0;
        aAllocPageList[sPageListID].mHeadPageID = SM_NULL_PID;
        aAllocPageList[sPageListID].mTailPageID = SM_NULL_PID;
        aAllocPageList[sPageListID].mMutex      = NULL;
    }

    return;
}

/* Runtime Item�� NULL�� �����Ѵ�.
   DISCARD/OFFLINE Tablespace�� ���� Table�鿡 ���� ����ȴ�.
 */
IDE_RC svpAllocPageList::setRuntimeNull(
    UInt                   aAllocPageListCount,
    smpAllocPageListEntry* aAllocPageListArray )
{
    UInt  sPageListID;

    IDE_DASSERT( aAllocPageListArray != NULL );

    for( sPageListID = 0;
         sPageListID < aAllocPageListCount;
         sPageListID++ )
    {

        aAllocPageListArray[sPageListID].mMutex = NULL;
    }

    return IDE_SUCCESS;
}

/**********************************************************************
 * AllocPageListEntry�� Runtime ����(Mutex) �ʱ�ȭ
 *
 * aAllocPageList : �����Ϸ��� AllocPageList
 * aPageListID    : AllocPageList�� ID
 * aTableOID      : AllocPageList�� ���̺� OID
 * aPageType      : List�� �����ϴ� Page ����
 **********************************************************************/
IDE_RC svpAllocPageList::initEntryAtRuntime(
    smpAllocPageListEntry* aAllocPageList,
    smOID                  aTableOID,
    smpPageType            aPageType )
{
    UInt                    i           = 0;
    UInt                    sState      = 0;
    UInt                    sAllocCount = 0;
    UInt                    sPageListID;
    SChar                   sBuffer[128];
    smpAllocPageListEntry * sAllocPageList;

    IDE_DASSERT( aAllocPageList != NULL );

    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        /* BUG-32434 [sm-mem-resource] If the admin uses a volatile table
         * with the parallel free page list, the server hangs while server start
         * up.
         * Parallel FreePageList�� ����� ���� �ֱ� ������, �ʱ�ȭ�� �׻�
         * PageListID�� Array ������ ����ؾ� �Ѵ�. */
        sAllocPageList = & aAllocPageList[ sPageListID ];
        if(aPageType == SMP_PAGETYPE_FIX)
        {
            idlOS::snprintf( sBuffer, 128,
                             "FIXED_ALLOC_PAGE_LIST_MUTEX_%"
                             ID_XINT64_FMT"_%"ID_UINT32_FMT"",
                             (ULong)aTableOID, sPageListID );
        }
        else
        {
            idlOS::snprintf(sBuffer, 128,
                            "VAR_ALLOC_PAGE_LIST_MUTEX_%"
                            ID_XINT64_FMT"_%"ID_UINT32_FMT"",
                            (ULong)aTableOID, sPageListID);
        }

        /* svpAllocPageList_initEntryAtRuntime_malloc_Mutex.tc */
        IDU_FIT_POINT("svpAllocPageList::initEntryAtRuntime::malloc::Mutex");
        IDE_TEST(iduMemMgr::malloc(
                     IDU_MEM_SM_SVP,
                     sizeof(iduMutex),
                     (void **)&(sAllocPageList->mMutex),
                     IDU_MEM_FORCE )
                 != IDE_SUCCESS);
        sAllocCount = sPageListID;
        sState      = 1;

        IDE_TEST(sAllocPageList->mMutex->initialize(
                     sBuffer,
                     IDU_MUTEX_KIND_NATIVE,
                     IDV_WAIT_INDEX_NULL )
                 != IDE_SUCCESS);

        /* volatile table�� �ʱ�ȭ�Ǿ�� �ϱ� ������
           sAllocPageList�� head, tail�� ��� null pid�̾�� �Ѵ�. */
        sAllocPageList->mHeadPageID = SM_NULL_PID;
        sAllocPageList->mTailPageID = SM_NULL_PID;

        /* mPageCount�� 0���� �ʱ�ȭ�ؾ� �Ѵ�. */
        sAllocPageList->mPageCount = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            for( i = 0 ; i <= sAllocCount ; i++ )
            {
                sAllocPageList = & aAllocPageList[ i ];
                IDE_ASSERT( iduMemMgr::free( sAllocPageList->mMutex )
                            == IDE_SUCCESS );
                sAllocPageList->mMutex = NULL;
            }
        default:
            break;
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * AllocPageList�� Runtime ���� ����
 *
 * aAllocPageList : �����Ϸ��� AllocPageList
 **********************************************************************/

IDE_RC svpAllocPageList::finEntryAtRuntime(
    smpAllocPageListEntry* aAllocPageList )
{
    IDE_DASSERT( aAllocPageList != NULL );
    IDE_DASSERT( aAllocPageList->mMutex != NULL );

    IDE_TEST(aAllocPageList->mMutex->destroy() != IDE_SUCCESS);

    IDE_TEST(iduMemMgr::free(aAllocPageList->mMutex) != IDE_SUCCESS);

    aAllocPageList->mMutex = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * aAllocPageHead~aAllocPageTail�� AllocPageList�� �߰�
 *
 * aTrans         : �۾��Ϸ��� Ʈ����� ��ü
 * aAllocPageList : �߰��Ϸ��� AllocPageList
 * aAllocPageHead : �߰��Ϸ��� Page�� Head
 * aAllocPageTail : �߰��Ϸ��� Page�� Tail
 **********************************************************************/

IDE_RC svpAllocPageList::addPageList( scSpaceID               aSpaceID,
                                      smpAllocPageListEntry * aAllocPageList,
                                      smpPersPage           * aAllocPageHead,
                                      smpPersPage           * aAllocPageTail,
                                      UInt                    aAllocPageCnt )
{
    smpPersPage* sTailPagePtr = NULL;

    IDE_DASSERT( aAllocPageList != NULL );
    IDE_DASSERT( aAllocPageHead != NULL );
    IDE_DASSERT( aAllocPageTail != NULL );

    IDE_DASSERT( isValidPageList( aSpaceID,
                                  aAllocPageList->mHeadPageID,
                                  aAllocPageList->mTailPageID,
                                  aAllocPageList->mPageCount )
                 == ID_TRUE );

    IDE_DASSERT( isValidPageList( aSpaceID,
                                  aAllocPageHead->mHeader.mSelfPageID,
                                  aAllocPageTail->mHeader.mSelfPageID,
                                  aAllocPageCnt )
                 == ID_TRUE );

    if(aAllocPageList->mHeadPageID == SM_NULL_PID)
    {
        // Head�� ������� �״�� aAllocPageHead~aAllocPageTail����
        // AllocPageList�� Head~Tail�� ��ġ�Ѵ�.

        IDE_DASSERT( aAllocPageList->mTailPageID == SM_NULL_PID );

        aAllocPageList->mHeadPageID = aAllocPageHead->mHeader.mSelfPageID;
        aAllocPageList->mTailPageID = aAllocPageTail->mHeader.mSelfPageID;
    }
    else
    {
        // AllocPageList�� Tail�ڿ� sAllocPageHead~sAllocPageTail�� ���δ�.
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                                aAllocPageList->mTailPageID,
                                                (void**)&sTailPagePtr )
                    == IDE_SUCCESS );

        // Tail�ڿ� sAllocPageHead�� ���̰�

        sTailPagePtr->mHeader.mNextPageID = aAllocPageHead->mHeader.mSelfPageID;

        // sAllocPageHead �տ� Tail�� ���δ�.

        aAllocPageHead->mHeader.mPrevPageID = aAllocPageList->mTailPageID;

        aAllocPageList->mTailPageID = aAllocPageTail->mHeader.mSelfPageID;
    }

    //  PageListEntry ���� ����
    aAllocPageList->mPageCount += aAllocPageCnt;

    IDE_DASSERT( isValidPageList( aSpaceID,
                                  aAllocPageList->mHeadPageID,
                                  aAllocPageList->mTailPageID,
                                  aAllocPageList->mPageCount )
                 == ID_TRUE );

    return IDE_SUCCESS;
}

/**********************************************************************
 * AllocPageList���� �� DB�� �ݳ�
 *
 * FreePage�� �����ϱ� ���ؼ��� �ٸ� Tx�� �����ؼ��� �ȵȴ�.
 * refineDB���� ���� �ϳ��� Tx�� �����ϴ�.
 *
 * aTrans         : �۾��Ϸ��� Ʈ����� ��ü
 * aAllocPageList : �����Ϸ��� AllocPageList
 * aDropFlag      : DROP TABLE -> ID_TRUE
 *                  ALTER TABLE -> ID_FALSE
 **********************************************************************/

IDE_RC svpAllocPageList::freePageListToDB(
    void*                  aTrans,
    scSpaceID              aSpaceID,
    smpAllocPageListEntry* aAllocPageList)
{
    smpPersPage*           sHead;
    smpPersPage*           sTail;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aAllocPageList != NULL );

    IDE_DASSERT( isValidPageList( aSpaceID,
                                  aAllocPageList->mHeadPageID,
                                  aAllocPageList->mTailPageID,
                                  aAllocPageList->mPageCount )
                 == ID_TRUE );

    if( aAllocPageList->mHeadPageID != SM_NULL_PID )
    {
        // Head�� NULL�̶�� AllocPageList�� �����.
        IDE_DASSERT( aAllocPageList->mTailPageID != SM_NULL_PID );

        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                aAllocPageList->mHeadPageID,
                                                (void**)&sHead )
                    == IDE_SUCCESS );
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                aAllocPageList->mTailPageID,
                                                (void**)&sTail )
                    == IDE_SUCCESS );

        aAllocPageList->mPageCount  = 0;
        aAllocPageList->mHeadPageID = SM_NULL_PID;
        aAllocPageList->mTailPageID = SM_NULL_PID;

        IDE_TEST( svmManager::freePersPageList( aTrans,
                                                aSpaceID,
                                                sHead,
                                                sTail )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * AllocPageList���� aPageID �����Ͽ� DB�� �ݳ�
 *
 * aTrans         : �۾��Ϸ��� Ʈ����� ��ü
 * aAllocPageList : �����Ϸ��� AllocPageList
 * aPageID        : �����Ϸ��� Page ID
 **********************************************************************/

IDE_RC svpAllocPageList::removePage( scSpaceID               aSpaceID,
                                     smpAllocPageListEntry * aAllocPageList,
                                     scPageID                aPageID )
{
    idBool                 sLocked = ID_FALSE;
    scPageID               sPrevPageID;
    scPageID               sNextPageID;
    scPageID               sListHeadPID;
    scPageID               sListTailPID;
    smpPersPageHeader*     sPrevPageHeader;
    smpPersPageHeader*     sNextPageHeader;
    smpPersPageHeader*     sCurPageHeader;

    IDE_DASSERT( aAllocPageList != NULL );

    IDE_TEST( aAllocPageList->mMutex->lock( NULL ) != IDE_SUCCESS );
    sLocked = ID_TRUE;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                            aPageID,
                                            (void**)&sCurPageHeader )
                == IDE_SUCCESS );
    sPrevPageID    = sCurPageHeader->mPrevPageID;
    sNextPageID    = sCurPageHeader->mNextPageID;
    sListHeadPID   = aAllocPageList->mHeadPageID;
    sListTailPID   = aAllocPageList->mTailPageID;

    if(sPrevPageID == SM_NULL_PID)
    {
        // PrevPage�� NULL�̶�� CurPage�� PageList�� Head�̴�.
        IDE_DASSERT( aPageID == sListHeadPID );

        sListHeadPID = sNextPageID;
    }
    else
    {
        // ���� ��ũ�� �����ش�.
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sPrevPageID,
                                                (void**)&sPrevPageHeader )
                    == IDE_SUCCESS );
  
        sPrevPageHeader->mNextPageID = sNextPageID;
    }

    if(sNextPageID == SM_NULL_PID)
    {
        // NextPage�� NULL�̶�� CurPage�� PageList�� Tail�̴�.
        IDE_DASSERT( aPageID == sListTailPID );

        sListTailPID = sPrevPageID;
    }
    else
    {
        // ���� ��ũ�� �����ش�.
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sNextPageID,
                                                (void**)&sNextPageHeader )
                    == IDE_SUCCESS );

        sNextPageHeader->mPrevPageID = sPrevPageID;

    }

    aAllocPageList->mHeadPageID = sListHeadPID;
    aAllocPageList->mTailPageID = sListTailPID;
    aAllocPageList->mPageCount--;

    sLocked = ID_FALSE;
    IDE_TEST( aAllocPageList->mMutex->unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sLocked == ID_TRUE)
    {
        IDE_ASSERT( aAllocPageList->mMutex->unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * aHeadPage~aTailPage������ PageList�� ������ �ùٸ��� �˻��Ѵ�.
 *
 * aHeadPage  : �˻��Ϸ��� List�� Head
 * aTailPage  : �˻��Ϸ��� List�� Tail
 * aPageCount : �˻��Ϸ��� List�� Page�� ����
 **********************************************************************/

idBool svpAllocPageList::isValidPageList( scSpaceID aSpaceID,
                                          scPageID  aHeadPage,
                                          scPageID  aTailPage,
                                          vULong    aPageCount )
{
    idBool     sIsValid;
    vULong     sPageCount = 0;
    scPageID   sCurPage = SM_NULL_PID;
    scPageID   sNxtPage;
    SChar    * sPagePtr;

    sIsValid = ID_FALSE;

    sNxtPage = aHeadPage;

    while(sNxtPage != SM_NULL_PID)
    {
        sCurPage = sNxtPage;

        sPageCount++;

        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sCurPage,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );
        sNxtPage = SMP_GET_NEXT_PERS_PAGE_ID( sPagePtr );
    }

    if(sCurPage == aTailPage)
    {
        if(sPageCount == aPageCount)
        {
            sIsValid = ID_TRUE;
        }
    }

    return sIsValid;
}

/**********************************************************************
 * aAllocPageList���� ��� Row�� ��ȸ�ϸ鼭 ��ȿ�� Row�� ã���ش�.
 *
 * aAllocPageList : ��ȸ�Ϸ��� PageListEntry
 * aCurRow        : ã������Ϸ��� Row
 * aNxtRow        : ���� ��ȿ�� Row�� ã�Ƽ� ��ȯ
 **********************************************************************/

IDE_RC svpAllocPageList::nextOIDall( scSpaceID              aSpaceID,
                                     smpAllocPageListEntry* aAllocPageList,
                                     SChar*                 aCurRow,
                                     vULong                 aSlotSize,
                                     SChar**                aNxtRow )
{
    scPageID            sNxtPID;
    smpPersPage       * sPage;
    SChar             * sNxtPtr;
    SChar             * sFence;
#if defined(DEBUG_SVP_NEXTOIDALL_PAGE_CHECK)
    idBool              sIsFreePage ;
    smpPersPageHeader * sPageHdr;
#endif // DEBUG_SVP_NEXTOIDALL_PAGE_CHECK

    IDE_DASSERT( aAllocPageList != NULL );
    IDE_DASSERT( aNxtRow != NULL );

    initForScan( aSpaceID,
                 aAllocPageList,
                 aCurRow,
                 aSlotSize,
                 &sPage,
                 &sNxtPtr );

    *aNxtRow = NULL;


    while(sPage != NULL)
    {

#if defined(DEBUG_SVP_NEXTOIDALL_PAGE_CHECK)
        // nextOIDall ��ĵ ����� �Ǵ� ��������
        // ���̺� �Ҵ�� Page�̹Ƿ� Free Page�� �� ����.
        sPageHdr = (smpPersPageHeader *)sPage;
        IDE_TEST( svmExpandChunk::isFreePageID( sPageHdr->mSelfPageID,
                                                & sIsFreePage )
                  != IDE_SUCCESS );
        IDE_DASSERT( sIsFreePage == ID_FALSE );
#endif // DEBUG_SVP_NEXTOIDALL_PAGE_CHECK

        // �ش� Page�� ���� ������ ���
        sFence = (SChar *)((smpPersPage *)sPage + 1);

        for( ;
             sNxtPtr+aSlotSize <= sFence;
             sNxtPtr += aSlotSize )
        {
            // In case of free slot
            if( SMP_SLOT_IS_NOT_USED( (smpSlotHeader *)sNxtPtr ) )
            {
                continue;
            }

            *aNxtRow = sNxtPtr;

            IDE_CONT(normal_case);
        } /* for */

        sNxtPID = getNextAllocPageID( aSpaceID,
                                      aAllocPageList,
                                      sPage->mHeader.mSelfPageID );

        if(sNxtPID == SM_NULL_PID)
        {
            // NextPage�� NULL�̸� ���̴�.
            IDE_CONT(normal_case);
        }

        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sNxtPID,
                                                (void**)&sPage )
                    == IDE_SUCCESS );
        sNxtPtr  = (SChar *)sPage->mBody;
    }

    IDE_EXCEPTION_CONT( normal_case );

    return IDE_SUCCESS;

#if defined(DEBUG_SVP_NEXTOIDALL_PAGE_CHECK)

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

#endif // DEBUG_SVP_NEXTOIDALL_PAGE_CHECK
}

/***********************************************************************
 * nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
 *
 * aAllocPageList : ��ȸ�Ϸ��� PageListEntry
 * aRow           : ���� Row
 * aPage          : aRow�� ���� Page�� ã�Ƽ� ��ȯ
 * aRowPtr        : aRow ���� Row ������
 ***********************************************************************/

inline void svpAllocPageList::initForScan(
    scSpaceID              aSpaceID,
    smpAllocPageListEntry* aAllocPageList,
    SChar*                 aRow,
    vULong                 aSlotSize,
    smpPersPage**          aPage,
    SChar**                aRowPtr )
{
    scPageID sPageID;

    IDE_DASSERT( aAllocPageList != NULL );
    IDE_DASSERT( aPage != NULL );
    IDE_DASSERT( aRowPtr != NULL );

    *aPage   = NULL;
    *aRowPtr = NULL;

    if(aRow != NULL)
    {
        sPageID = SMP_SLOT_GET_PID(aRow);
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sPageID,
                                                (void**)aPage )
                    == IDE_SUCCESS );
        *aRowPtr = aRow + aSlotSize;
    }
    else
    {
        sPageID = getFirstAllocPageID(aAllocPageList);

        if(sPageID != SM_NULL_PID)
        {
            IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                    sPageID,
                                                    (void**)aPage )
                        == IDE_SUCCESS );
            *aRowPtr = (SChar *)((*aPage)->mBody);
        }
        else
        {
            /* Allcate�� �������� �������� �ʴ´�.*/
        }
    }
}

/**********************************************************************
 * aAllocPageList���� aPageID ���� PageID
 *
 * ����ȭ�Ǿ��ִ� AllocPageList���� aPageID�� Tail�̶��
 * aPageID�� ���� Page�� ���� ����Ʈ�� Head�� �ȴ�.
 *
 * aAllocPageList : Ž���Ϸ��� PageListEntry
 * aPageID        : Ž���Ϸ��� PageID
 **********************************************************************/

scPageID svpAllocPageList::getNextAllocPageID(
    scSpaceID              aSpaceID,
    smpAllocPageListEntry* aAllocPageList,
    scPageID               aPageID )
{
    scPageID            sNextPageID;
    smpPersPageHeader * sCurPageHeader;

    IDE_DASSERT( aAllocPageList != NULL );

    if(aPageID == SM_NULL_PID)
    {
        sNextPageID = getFirstAllocPageID(aAllocPageList);
    }
    else
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                aPageID,
                                                (void**)&sCurPageHeader )
                    == IDE_SUCCESS );
        sNextPageID = svpAllocPageList::getNextAllocPageID( aAllocPageList,
                                                            sCurPageHeader );
    }

    return sNextPageID;
}


/**********************************************************************
 * aAllocPageList���� aPageID ���� PageID
 *
 * ����ȭ�Ǿ��ִ� AllocPageList���� aPageID�� Tail�̶��
 * aPageID�� ���� Page�� ���� ����Ʈ�� Head�� �ȴ�.
 *
 * aAllocPageList : Ž���Ϸ��� PageListEntry
 * aPagePtr       : Ž���Ϸ��� Page�� ���� ������
 **********************************************************************/
scPageID svpAllocPageList::getNextAllocPageID(
    smpAllocPageListEntry * aAllocPageList,
    smpPersPageHeader     * aPagePtr )
{
    scPageID           sNextPageID;
    UInt               sPageListID;

    IDE_DASSERT( aAllocPageList != NULL );

    if(aPagePtr == NULL)
    {
        sNextPageID = getFirstAllocPageID(aAllocPageList);
    }
    else
    {
        sNextPageID = aPagePtr->mNextPageID;

        if(sNextPageID == SM_NULL_PID)
        {
            for( sPageListID = aPagePtr->mAllocListID + 1;
                 sPageListID < SMP_PAGE_LIST_COUNT;
                 sPageListID++ )
            {
                sNextPageID =
                    aAllocPageList[sPageListID].mHeadPageID;

                if(sNextPageID != SM_NULL_PID)
                {
                    break;
                }
            }
        }
    }

    return sNextPageID;
}



/**********************************************************************
 * aAllocPageList���� aPageID ���� PageID
 *
 * ����ȭ�Ǿ��ִ� AllocPageList���� aPageID�� Head���
 * aPageID�� ���� Page�� ���� ����Ʈ�� Tail�� �ȴ�.
 *
 * aAllocPageList : Ž���Ϸ��� PageListEntry
 * aPageID        : Ž���Ϸ��� PageID
 **********************************************************************/

scPageID svpAllocPageList::getPrevAllocPageID(
    scSpaceID              aSpaceID,
    smpAllocPageListEntry* aAllocPageList,
    scPageID               aPageID )
{
    smpPersPageHeader* sCurPageHeader;
    scPageID           sPrevPageID;
    UInt               sPageListID;

    IDE_DASSERT( aAllocPageList != NULL );

    if(aPageID == SM_NULL_PID)
    {
        sPrevPageID = getLastAllocPageID(aAllocPageList);
    }
    else
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                aPageID,
                                                (void**)&sCurPageHeader )
                    == IDE_SUCCESS );

        sPrevPageID = sCurPageHeader->mPrevPageID;
        sPageListID = sCurPageHeader->mAllocListID;

        if((sPrevPageID == SM_NULL_PID) && (sPageListID > 0))
        {
            do
            {
                sPageListID--;

                sPrevPageID =
                    aAllocPageList[sPageListID].mTailPageID;

                if(sPrevPageID != SM_NULL_PID)
                {
                    break;
                }
            } while(sPageListID != 0);
        }
    }

    return sPrevPageID;
}

/**********************************************************************
 * aAllocPageList�� ù PageID�� ��ȯ
 *
 * AllocPageList�� ����ȭ�Ǿ� �ֱ� ������
 * 0��° ����Ʈ�� Head�� NULL�̴��� 1��° ����Ʈ�� Head�� NULL�� �ƴ϶��
 * ù PageID�� 1��° ����Ʈ�� Head�� �ȴ�.
 *
 * aAllocPageList : Ž���Ϸ��� PageListEntry
 **********************************************************************/

scPageID svpAllocPageList::getFirstAllocPageID(
    smpAllocPageListEntry* aAllocPageList )
{
    UInt     sPageListID;
    scPageID sHeadPID = SM_NULL_PID;

    IDE_DASSERT( aAllocPageList != NULL );

    for( sPageListID = 0;
         sPageListID < SMP_PAGE_LIST_COUNT;
         sPageListID++ )
    {
        if(aAllocPageList[sPageListID].mHeadPageID != SM_NULL_PID)
        {
            sHeadPID = aAllocPageList[sPageListID].mHeadPageID;

            break;
        }
    }

    return sHeadPID;
}

/**********************************************************************
 * aAllocPageList�� ������ PageID�� ��ȯ
 *
 * ����ȭ�Ǿ� �ִ� AllocPageList���� NULL�� �ƴ� ���� �ڿ� �ִ� Tail��
 * aAllocPageList�� ������ PageID�� �ȴ�.
 *
 * aAllocPageList : Ž���Ϸ��� PageListEntry
 **********************************************************************/

scPageID svpAllocPageList::getLastAllocPageID(
    smpAllocPageListEntry* aAllocPageList )
{
    UInt     sPageListID;
    scPageID sTailPID = SM_NULL_PID;

    IDE_DASSERT( aAllocPageList != NULL );

    sPageListID = SMP_PAGE_LIST_COUNT;

    do
    {
        sPageListID--;

        if(aAllocPageList[sPageListID].mTailPageID != SM_NULL_PID)
        {
            sTailPID = aAllocPageList[sPageListID].mTailPageID;

            break;
        }
    } while(sPageListID != 0);

    return sTailPID;
}
