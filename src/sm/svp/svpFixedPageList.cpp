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
 
#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <svm.h>
#include <svpFixedPageList.h>
#include <svpFreePageList.h>
#include <svpAllocPageList.h>
#include <svpManager.h>
#include <svmExpandChunk.h>
#include <svpReq.h>

/**********************************************************************
 * Tx's PrivatePageList�� FreePage�κ��� Slot�� �Ҵ��� �� ������ �˻��ϰ�
 * �����ϸ� �Ҵ��Ѵ�.
 *
 * aTrans     : �۾��ϴ� Ʈ����� ��ü
 * aTableOID  : �Ҵ��Ϸ��� ���̺� OID
 * aRow       : �Ҵ��ؼ� ��ȯ�Ϸ��� Slot ������
 **********************************************************************/

IDE_RC svpFixedPageList::tryForAllocSlotFromPrivatePageList(
    void             * aTrans,
    scSpaceID          aSpaceID,
    smOID              aTableOID,
    smpPageListEntry * aFixedEntry,
    SChar           ** aRow )
{
    smpPrivatePageListEntry* sPrivatePageList = NULL;
    smpFreePageHeader*       sFreePageHeader;

    IDE_DASSERT(aTrans != NULL);
    IDE_DASSERT(aRow != NULL);

    *aRow = NULL;

    IDE_TEST( smLayerCallback::findVolPrivatePageList( aTrans,
                                                       aTableOID,
                                                       &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList != NULL)
    {
        sFreePageHeader = sPrivatePageList->mFixedFreePageHead;
        if(sFreePageHeader != NULL)
        {
            IDE_DASSERT(sFreePageHeader->mFreeSlotCount > 0);

            removeSlotFromFreeSlotList(sFreePageHeader, aRow);

            if( sFreePageHeader->mFreeSlotCount == (aFixedEntry->mSlotCount-1) )
            {
                IDE_TEST( linkScanList( aSpaceID,
                                        sFreePageHeader->mSelfPageID,
                                        aFixedEntry )
                          != IDE_SUCCESS );
            }

            if(sFreePageHeader->mFreeSlotCount == 0)
            {
                svpFreePageList::removeFixedFreePageFromPrivatePageList(
                    sPrivatePageList,
                    sFreePageHeader);
            }
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * FreePageList�� FreePagePool���� FreeSlot�� �Ҵ��� �� �ִ��� �õ�
 * �Ҵ��� �Ǹ� aRow�� ��ȯ�ϰ� �Ҵ��� FreeSlot�� ���ٸ� aRow�� NULL�� ��ȯ
 *
 * aTrans      : �۾��ϴ� Ʈ����� ��ü
 * aFixedEntry : Slot�� �Ҵ��Ϸ��� PageListEntry
 * aPageListID : Slot�� �Ҵ��Ϸ��� PageListID
 * aRow        : �Ҵ��ؼ� ��ȯ�Ϸ��� Slot
 ***********************************************************************/
IDE_RC svpFixedPageList::tryForAllocSlotFromFreePageList(
    void*             aTrans,
    scSpaceID         aSpaceID,
    smpPageListEntry* aFixedEntry,
    UInt              aPageListID,
    SChar**           aRow )
{
    UInt                  sState = 0;
    UInt                  sSizeClassID;
    idBool                sIsPageAlloced;
    smpFreePageHeader*    sFreePageHeader;
    smpFreePageHeader*    sNextFreePageHeader;
    smpFreePageListEntry* sFreePageList;
    UInt                  sSizeClassCount;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aRow != NULL );

    sFreePageList = &(aFixedEntry->mRuntimeEntry->mFreePageList[aPageListID]);

    IDE_DASSERT( sFreePageList != NULL );

    *aRow = NULL;

    sSizeClassCount = SMP_SIZE_CLASS_COUNT( aFixedEntry->mRuntimeEntry );

    while(1)
    {
        // FreePageList�� SizeClass�� ��ȸ�ϸ鼭 tryAllocSlot�Ѵ�.
        for(sSizeClassID = 0;
            sSizeClassID < sSizeClassCount;
            sSizeClassID++)
        {
            sFreePageHeader = sFreePageList->mHead[sSizeClassID];
            // list�� Head�� �����ö� lock�� ������ ������
            // freeSlot������ PageLock��� ListLock��� ������
            // ���⼭ list Lock���� ��� HeadPage Lock������ ����� �߻�
            // �׷��� ���� Head�� �����ͼ� PageLock��� �ٽ� Ȯ���Ͽ� �ذ�

            while(sFreePageHeader != NULL)
            {
                // �ش� Page�� ���� Slot�� �Ҵ��Ϸ��� �ϰ�
                // �Ҵ��ϰԵǸ� �ش� Page�� �Ӽ��� ����ǹǷ� lock���� ��ȣ
                IDE_TEST(sFreePageHeader->mMutex.lock(NULL) != IDE_SUCCESS);
                sState = 1;

                // lock������� �ش� Page�� ���� �ٸ� Tx�� ���� ����Ǿ����� �˻�
                if(sFreePageHeader->mFreeListID == aPageListID)
                {
                    IDE_ASSERT(sFreePageHeader->mFreeSlotCount > 0);

                    removeSlotFromFreeSlotList(sFreePageHeader, aRow);

                    if( sFreePageHeader->mFreeSlotCount == (aFixedEntry->mSlotCount-1) )
                    {
                        IDE_TEST( linkScanList( aSpaceID,
                                                sFreePageHeader->mSelfPageID,
                                                aFixedEntry )
                                  != IDE_SUCCESS );
                    }

                    // FreeSlot�� �Ҵ��� Page�� SizeClass�� ����Ǿ�����
                    // Ȯ���Ͽ� ����
                    IDE_TEST(svpFreePageList::modifyPageSizeClass(
                                 aTrans,
                                 aFixedEntry,
                                 sFreePageHeader)
                             != IDE_SUCCESS);

                    sState = 0;
                    IDE_TEST(sFreePageHeader->mMutex.unlock()
                             != IDE_SUCCESS);

                    IDE_CONT(normal_case);
                }
                else
                {
                    // �ش� Page�� ����� ���̶�� List���� �ٽ� Head�� �����´�.
                    sNextFreePageHeader = sFreePageList->mHead[sSizeClassID];
                }

                sState = 0;
                IDE_TEST(sFreePageHeader->mMutex.unlock() != IDE_SUCCESS);

                sFreePageHeader = sNextFreePageHeader;
            }
        }

        // FreePageList���� FreeSlot�� ã�� ���ߴٸ�
        // FreePagePool���� Ȯ���Ͽ� �����´�.

        IDE_TEST( svpFreePageList::tryForAllocPagesFromPool( aFixedEntry,
                                                             aPageListID,
                                                             &sIsPageAlloced )
                  != IDE_SUCCESS );

        if(sIsPageAlloced == ID_FALSE)
        {
            // Pool���� �������Դ�.
            IDE_CONT(normal_case);
        }
    }

    IDE_EXCEPTION_CONT( normal_case );

    IDE_ASSERT(sState == 0);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT( sFreePageHeader->mMutex.unlock() == IDE_SUCCESS );
    }

    IDE_POP();

    return IDE_FAILURE;
}

/****************************************************************************
 *
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 *
 * �־��� �������� Scan List�κ��� �����Ѵ�.
 *
 ****************************************************************************/
IDE_RC svpFixedPageList::unlinkScanList( scSpaceID          aSpaceID,
                                         scPageID           aPageID,
                                         smpPageListEntry * aFixedEntry )
{
    svmPCH               * sMyPCH;
    svmPCH               * sNxtPCH;
    svmPCH               * sPrvPCH;
    smpScanPageListEntry * sScanPageList;

    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    IDU_FIT_POINT("svpFixedPageList::unlinkScanList::wait1");

    IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

    IDE_DASSERT( sScanPageList->mHeadPageID != SM_NULL_PID );
    IDE_DASSERT( sScanPageList->mTailPageID != SM_NULL_PID );

    sMyPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                           aPageID ));
    IDE_DASSERT( sMyPCH != NULL );
    IDE_DASSERT( sMyPCH->mNxtScanPID != SM_NULL_PID );
    IDE_DASSERT( sMyPCH->mPrvScanPID != SM_NULL_PID );

    /* BUG-43463 Fullscan ���ü� ����,
     * - svnnMoveNextNonBlock()�� ���� ���ü� ����� ���� atomic ����
     * - ����� modify_seq, nexp_pid, prev_pid�̴�.
     * - modify_seq�� link, unlink������ ����ȴ�.
     * - link, unlink ���� �߿��� modify_seq�� Ȧ���̴�
     * - ù��° page�� lock�� ��� Ȯ���ϹǷ� atomic���� set���� �ʾƵ� �ȴ�.
     * */
    SVM_PCH_SET_MODIFYING( sMyPCH );

    if( sScanPageList->mHeadPageID == aPageID )
    {
        if( sMyPCH->mNxtScanPID != SM_SPECIAL_PID )
        {
            IDE_DASSERT( sScanPageList->mTailPageID != aPageID );
            IDE_DASSERT( sMyPCH->mPrvScanPID == SM_SPECIAL_PID );
            sNxtPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                                    sMyPCH->mNxtScanPID ));
            sNxtPCH->mPrvScanPID       = sMyPCH->mPrvScanPID;
            sScanPageList->mHeadPageID = sMyPCH->mNxtScanPID;
        }
        else
        {
            IDE_DASSERT( sScanPageList->mTailPageID == aPageID );
            sScanPageList->mHeadPageID = SM_NULL_PID;
            sScanPageList->mTailPageID = SM_NULL_PID;
        }
    }
    else
    {
        IDE_DASSERT( sMyPCH->mPrvScanPID != SM_SPECIAL_PID );
        sPrvPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                                sMyPCH->mPrvScanPID ));

        if( sMyPCH->mNxtScanPID != SM_SPECIAL_PID )
        {
            sNxtPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                                    sMyPCH->mNxtScanPID ));

            idCore::acpAtomicSet32( &(sNxtPCH->mPrvScanPID),
                                    sMyPCH->mPrvScanPID );
        }
        else
        {
            IDE_DASSERT( sScanPageList->mTailPageID == aPageID );
            sScanPageList->mTailPageID = sMyPCH->mPrvScanPID;
        }

        idCore::acpAtomicSet32( &(sPrvPCH->mNxtScanPID),
                                sMyPCH->mNxtScanPID );
    }

    idCore::acpAtomicSet32( &(sMyPCH->mNxtScanPID),
                            SM_NULL_PID );

    idCore::acpAtomicSet32( &( sMyPCH->mPrvScanPID),
                            SM_NULL_PID );

    SVM_PCH_SET_MODIFIED( sMyPCH );

    IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 *
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 *
 * �־��� �������� Scan List�κ��� �����Ѵ�.
 *
 ****************************************************************************/
IDE_RC svpFixedPageList::linkScanList( scSpaceID          aSpaceID,
                                       scPageID           aPageID,
                                       smpPageListEntry * aFixedEntry )
{
    svmPCH               * sMyPCH;
    svmPCH               * sTailPCH;
    smpScanPageListEntry * sScanPageList;
#ifdef DEBUG
    smpFreePageHeader    * sFreePageHeader;
#endif
    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

#ifdef DEBUG
    sFreePageHeader = svpFreePageList::getFreePageHeader( aSpaceID,
                                                          aPageID );

#endif
    IDE_DASSERT( (sFreePageHeader == NULL) ||
                 (sFreePageHeader->mFreeSlotCount != aFixedEntry->mSlotCount) );

    sMyPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                           aPageID ));
    IDE_DASSERT( sMyPCH != NULL );
    IDE_DASSERT( sMyPCH->mNxtScanPID == SM_NULL_PID );
    IDE_DASSERT( sMyPCH->mPrvScanPID == SM_NULL_PID );

    /* BUG-43463 Fullscan ���ü� ����,
     * - svnnMoveNextNonBlock()�� ���� ���ü� ����� ���� atomic ����
     * - ����� modify_seq, nexp_pid, prev_pid�̴�.
     * - modify_seq�� link, unlink������ ����ȴ�.
     * - link, unlink ���� �߿��� modify_seq�� Ȧ���̴�
     * - ù��° page�� lock�� ��� Ȯ���ϹǷ� atomic���� set���� �ʾƵ� �ȴ�.
     * */
    SVM_PCH_SET_MODIFYING( sMyPCH );

    if( sScanPageList->mTailPageID == SM_NULL_PID )
    {
        IDE_DASSERT( sScanPageList->mHeadPageID == SM_NULL_PID );
        sScanPageList->mHeadPageID = aPageID;
        sScanPageList->mTailPageID = aPageID;

        sMyPCH->mNxtScanPID = SM_SPECIAL_PID;
        sMyPCH->mPrvScanPID = SM_SPECIAL_PID;
    }
    else
    {
        sTailPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                                 sScanPageList->mTailPageID ));

        idCore::acpAtomicSet32( &(sTailPCH->mNxtScanPID),
                                aPageID );

        idCore::acpAtomicSet32( &(sMyPCH->mNxtScanPID),
                                SM_SPECIAL_PID );

        idCore::acpAtomicSet32( &(sMyPCH->mPrvScanPID),
                                sScanPageList->mTailPageID );

        sScanPageList->mTailPageID = aPageID;
    }

    SVM_PCH_SET_MODIFIED( sMyPCH );

    IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Runtime Item�� NULL�� �����Ѵ�.
 * DISCARD/OFFLINE Tablespace�� ���� Table�鿡 ���� ����ȴ�.
 *
 * aFixedEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 ***********************************************************************/
IDE_RC svpFixedPageList::setRuntimeNull( smpPageListEntry* aFixedEntry )
{
    IDE_DASSERT( aFixedEntry != NULL );

    // RuntimeEntry �ʱ�ȭ
    IDE_TEST(svpFreePageList::setRuntimeNull( aFixedEntry )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * memory table��  fixed page list entry�� �����ϴ� runtime ���� �ʱ�ȭ
 *
 * aTableOID   : PageListEntry�� ���ϴ� ���̺� OID
 * aFixedEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 ***********************************************************************/
IDE_RC svpFixedPageList::initEntryAtRuntime(
    smOID                  aTableOID,
    smpPageListEntry*      aFixedEntry,
    smpAllocPageListEntry* aAllocPageList )
{
    SChar                   sBuffer[128];
    smpScanPageListEntry  * sScanPageList;
    UInt                    sState  = 0;

    IDE_DASSERT( aFixedEntry != NULL );
    IDE_ASSERT( aTableOID == aFixedEntry->mTableOID);
    IDE_DASSERT( aAllocPageList != NULL );

    // RuntimeEntry �ʱ�ȭ
    IDE_TEST(svpFreePageList::initEntryAtRuntime( aFixedEntry )
             != IDE_SUCCESS);

    aFixedEntry->mRuntimeEntry->mAllocPageList = aAllocPageList;
    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    sScanPageList->mHeadPageID = SM_NULL_PID;
    sScanPageList->mTailPageID = SM_NULL_PID;

    idlOS::snprintf( sBuffer, 128,
                     "SCAN_PAGE_LIST_MUTEX_%"
                     ID_XINT64_FMT"",
                     (ULong)aTableOID );

    /* svpFixedPageList_initEntryAtRuntime_malloc_Mutex.tc */
    IDU_FIT_POINT("svpFixedPageList::initEntryAtRuntime::malloc::Mutex");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SVP,
                                 sizeof(iduMutex),
                                 (void **)&(sScanPageList->mMutex),
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sScanPageList->mMutex->initialize( sBuffer,
                                                 IDU_MUTEX_KIND_NATIVE,
                                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sScanPageList->mMutex ) == IDE_SUCCESS );
            sScanPageList->mMutex = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * memory table��  fixed page list entry�� �����ϴ� runtime ���� ����
 *
 * aFixedEntry : �����Ϸ��� PageListEntry
 ***********************************************************************/
IDE_RC svpFixedPageList::finEntryAtRuntime( smpPageListEntry* aFixedEntry )
{
    UInt                   sPageListID;
    smpScanPageListEntry * sScanPageList;

    IDE_DASSERT( aFixedEntry != NULL );

    if (aFixedEntry->mRuntimeEntry != NULL)
    {
        sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

        IDE_TEST(sScanPageList->mMutex->destroy() != IDE_SUCCESS);
        IDE_TEST(iduMemMgr::free(sScanPageList->mMutex) != IDE_SUCCESS);

        sScanPageList->mMutex = NULL;

        for(sPageListID = 0;
            sPageListID < SMP_PAGE_LIST_COUNT;
            sPageListID++)
        {
            // AllocPageList�� Mutex ����
            svpAllocPageList::finEntryAtRuntime(
                &(aFixedEntry->mRuntimeEntry->mAllocPageList[sPageListID]) );
        }

        // RuntimeEntry ����
        IDE_TEST(svpFreePageList::finEntryAtRuntime(aFixedEntry)
                 != IDE_SUCCESS);

        // svpFreePageList::finEntryAtRuntime���� RuntimeEntry�� NULL�� ����
        IDE_ASSERT( aFixedEntry->mRuntimeEntry == NULL );
    }
    else
    {
        /* Memory Table�� ��쿣 aFixedEntry->mRuntimeEntry�� NULL�� ���
           (OFFLINE/DISCARD)�� ������ Volatile Table�� ���ؼ���
           aFixedEntry->mRuntimeEntry�� �̹� NULL�� ���� ����.
           ���� aFixedEntry->mRuntimeEntry�� NULL�̶��
           ��򰡿� ���װ� �ִٴ� �ǹ��̴�. */

        IDE_DASSERT(0);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * PageListEntry�� ������ �����ϰ� DB�� �ݳ��Ѵ�.
 *
 * aTrans      : �۾��� �����ϴ� Ʈ����� ��ü
 * aTableOID   : ������ ���̺� OID
 * aFixedEntry : ������ PageListEntry
 * aDropFlag   : �����ϱ� ���� OP FLAG
 ***********************************************************************/
IDE_RC svpFixedPageList::freePageListToDB( void*             aTrans,
                                           scSpaceID         aSpaceID,
                                           smOID             aTableOID,
                                           smpPageListEntry* aFixedEntry )
{
    UInt sPageListID;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );
    IDE_ASSERT( aTableOID == aFixedEntry->mTableOID );

    // [0] FreePageList ����

    svpFreePageList::initializeFreePageListAndPool(aFixedEntry);

    /* ----------------------------
     * [1] fixed page list��
     *     svmManager�� ��ȯ�Ѵ�.
     * ---------------------------*/

    for(sPageListID = 0;
        sPageListID < SMP_PAGE_LIST_COUNT;
        sPageListID++)
    {
        // for AllocPageList
        IDE_TEST( svpAllocPageList::freePageListToDB(
                      aTrans,
                      aSpaceID,
                      &(aFixedEntry->mRuntimeEntry->mAllocPageList[sPageListID]) )
                  != IDE_SUCCESS );
    }

    //BUG-25505
    //page list entry�� allocPageList���� ��� DB�� ��ȯ������, ScanList���� �ʱ�ȭ �Ǿ�� �Ѵ�.
    aFixedEntry->mRuntimeEntry->mScanPageList.mHeadPageID = SM_NULL_PID;
    aFixedEntry->mRuntimeEntry->mScanPageList.mTailPageID = SM_NULL_PID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Page�� �ʱ�ȭ �Ѵ�.
 * Page���� ��� Slot�鵵 �ʱ�ȭ�ϸ� Next ��ũ�� �����Ѵ�.
 *
 * aSlotSize   : Page�� ���� Slot�� ũ��
 * aSlotCount  : Page���� ��� Slot ����
 * aPageListID : Page�� ���� PageListID
 * aPage       : �ʱ�ȭ�� Page
 ***********************************************************************/
void svpFixedPageList::initializePage( vULong       aSlotSize,
                                       vULong       aSlotCount,
                                       UInt         aPageListID,
                                       smOID        aTableOID,
                                       smpPersPage* aPage)
{
    UInt               sSlotCounter;
    UShort             sCurOffset;
    smpFreeSlotHeader* sCurFreeSlotHeader = NULL;
    smpFreeSlotHeader* sNextFreeSlotHeader;

    // BUG-26937 CodeSonar::NULL Pointer Dereference (4)
    IDE_DASSERT( aSlotSize > 0 );
    IDE_ASSERT( aSlotCount > 0 );
    IDE_DASSERT( aPageListID < SMP_PAGE_LIST_COUNT );
    IDE_DASSERT( aPage != NULL );

    aPage->mHeader.mType        = SMP_PAGETYPE_FIX;
    aPage->mHeader.mAllocListID = aPageListID;
    aPage->mHeader.mTableOID    = aTableOID;


    sCurOffset = (UShort)SMP_PERS_PAGE_BODY_OFFSET;
    // BUG-32091 MemPage Body�� �׻� 8Byte align �� ���¿��� �Ѵ�.
    IDE_DASSERT( idlOS::align8( sCurOffset ) == sCurOffset );
    sNextFreeSlotHeader = (smpFreeSlotHeader*)((SChar*)aPage + sCurOffset);

    for(sSlotCounter = 0; sSlotCounter < aSlotCount; sSlotCounter++)
    {
        sCurFreeSlotHeader = sNextFreeSlotHeader;

        SM_SET_SCN_FREE_ROW( &(sCurFreeSlotHeader->mCreateSCN) );
        SM_SET_SCN_FREE_ROW( &(sCurFreeSlotHeader->mLimitSCN) );
        SMP_SLOT_SET_OFFSET( sCurFreeSlotHeader, sCurOffset );

        sNextFreeSlotHeader = (smpFreeSlotHeader*)((SChar*)sCurFreeSlotHeader + aSlotSize);
        sCurFreeSlotHeader->mNextFreeSlot = sNextFreeSlotHeader;

        sCurOffset += aSlotSize;
    }

    sCurFreeSlotHeader->mNextFreeSlot = NULL;

    IDL_MEM_BARRIER;

    return;
}

/**********************************************************************
 *  Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 *  aSlotHeader : dump�� slot ���
 **********************************************************************/
IDE_RC svpFixedPageList::dumpSlotHeader( smpSlotHeader     * aSlotHeader )
{
    smSCN sCreateSCN;
    smSCN sLimitSCN;
    smSCN sRowSCN;
    smTID sRowTID;
    smSCN sNxtSCN;
    smTID sNxtTID;

    IDE_ERROR( aSlotHeader != NULL );

    SM_SET_SCN( &sCreateSCN, &(aSlotHeader->mCreateSCN) );
    SM_SET_SCN( &sLimitSCN,  &(aSlotHeader->mLimitSCN) );

    SMX_GET_SCN_AND_TID( sCreateSCN, sRowSCN, sRowTID );
    SMX_GET_SCN_AND_TID( sLimitSCN,  sNxtSCN, sNxtTID );

    ideLog::log( SM_TRC_LOG_LEVEL_MPAGE,
                 "Slot Ptr       : 0x%X\n"
                 "Slot CreateSCN : 0x%llX\n"
                 "Slot CreateTID :   %llu\n"
                 "Slot LimitSCN  : 0x%llX\n"
                 "Slot LimitTID  :   %llu\n"
                 "Slot NextOID   : 0x%llX\n"
                 "Slot Offset    : %llu\n"
                 "Slot Flag      : %llX\n",
                 aSlotHeader,
                 SM_SCN_TO_LONG( sRowSCN ),
                 sRowTID,
                 SM_SCN_TO_LONG( sNxtSCN ),
                 sNxtTID,
                 SMP_SLOT_GET_NEXT_OID( aSlotHeader ),
                 (ULong)SMP_SLOT_GET_OFFSET( aSlotHeader ),
                 (UInt)SMP_SLOT_GET_FLAGS( aSlotHeader ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * DB���� Page���� �Ҵ�޴´�.
 *
 * fixed slot�� ���� persistent page�� system���κ��� �Ҵ�޴´�.
 *
 * aTrans      : �۾��ϴ� Ʈ����� ��ü
 * aFixedEntry : �Ҵ���� PageListEntry
 ***********************************************************************/
IDE_RC svpFixedPageList::allocPersPages( void*             aTrans,
                                         scSpaceID         aSpaceID,
                                         smpPageListEntry* aFixedEntry )
{
    UInt                     sState = 0;
    UInt                     sPageListID;
    smpPersPage*             sPagePtr = NULL;
    smpPersPage*             sAllocPageHead;
    smpPersPage*             sAllocPageTail;
    scPageID                 sPrevPageID = SM_NULL_PID;
    scPageID                 sNextPageID = SM_NULL_PID;
#ifdef DEBUG
    smpFreePagePoolEntry*    sFreePagePool;
    smpFreePageListEntry*    sFreePageList;
#endif
    smpAllocPageListEntry*   sAllocPageList;
    smpPrivatePageListEntry* sPrivatePageList = NULL;
    svmPCH                 * sPCH;
    UInt                     sAllocPageCnt = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    sAllocPageList = &(aFixedEntry->mRuntimeEntry->mAllocPageList[sPageListID]);
#ifdef DEBUG
    sFreePagePool  = &(aFixedEntry->mRuntimeEntry->mFreePagePool);
    sFreePageList  = &(aFixedEntry->mRuntimeEntry->mFreePageList[sPageListID]);
#endif
    IDE_DASSERT( sAllocPageList != NULL );
    IDE_DASSERT( sFreePagePool != NULL );
    IDE_DASSERT( sFreePageList != NULL );

    // DB���� Page���� �Ҵ������ FreePageList��
    // Tx's Private Page List�� ���� ����� ��
    // Ʈ������� ����� �� �ش� ���̺��� PageListEntry�� ����ϰ� �ȴ�.

    IDE_TEST( smLayerCallback::findVolPrivatePageList( aTrans,
                                                       aFixedEntry->mTableOID,
                                                       &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList == NULL)
    {
        // ������ PrivatePageList�� �����ٸ� ���� �����Ѵ�.
        IDE_TEST( smLayerCallback::createVolPrivatePageList( aTrans,
                                                             aFixedEntry->mTableOID,
                                                             &sPrivatePageList )
                  != IDE_SUCCESS );
    }

    // DB���� �޾ƿ���
    IDE_TEST( svmManager::allocatePersPageList( aTrans,
                                                aSpaceID,
                                                SMP_ALLOCPAGECOUNT_FROMDB,
                                                (void **)&sAllocPageHead,
                                                (void **)&sAllocPageTail,
                                                &sAllocPageCnt )
              != IDE_SUCCESS);

    IDE_DASSERT( svpAllocPageList::isValidPageList(
                     aSpaceID,
                     sAllocPageHead->mHeader.mSelfPageID,
                     sAllocPageTail->mHeader.mSelfPageID,
                     sAllocPageCnt )
                 == ID_TRUE );
    sState = 1;

    // �Ҵ���� HeadPage�� PrivatePageList�� ����Ѵ�.
    if( sPrivatePageList->mFixedFreePageTail == NULL )
    {
        IDE_DASSERT( sPrivatePageList->mFixedFreePageHead == NULL );

        sPrivatePageList->mFixedFreePageHead =
            svpFreePageList::getFreePageHeader(
                aSpaceID,
                sAllocPageHead->mHeader.mSelfPageID);
    }
    else
    {
        IDE_DASSERT( sPrivatePageList->mFixedFreePageHead != NULL );

        sPrivatePageList->mFixedFreePageTail->mFreeNext =
            svpFreePageList::getFreePageHeader(
                aSpaceID,
                sAllocPageHead->mHeader.mSelfPageID);

        sPrevPageID = sPrivatePageList->mFixedFreePageTail->mSelfPageID;
    }

    // �Ҵ���� ���������� �ʱ�ȭ �Ѵ�.
    sNextPageID = sAllocPageHead->mHeader.mSelfPageID;

    while(sNextPageID != SM_NULL_PID)
    {
        IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID, 
                                                sNextPageID,
                                                (void**)&sPagePtr )
                    == IDE_SUCCESS );

        // PersPageHeader �ʱ�ȭ�ϰ� (FreeSlot���� �����Ѵ�.)
        initializePage( aFixedEntry->mSlotSize,
                        aFixedEntry->mSlotCount,
                        sPageListID,
                        aFixedEntry->mTableOID,
                        sPagePtr );

        sPCH = (svmPCH*)(smmManager::getPCH( aSpaceID,
                                             sPagePtr->mHeader.mSelfPageID ));
        sPCH->mNxtScanPID       = SM_NULL_PID;
        sPCH->mPrvScanPID       = SM_NULL_PID;
        sPCH->mModifySeqForScan = 0;

        // FreePageHeader �ʱ�ȭ�ϰ�
        svpFreePageList::initializeFreePageHeader(
            svpFreePageList::getFreePageHeader(aSpaceID, sPagePtr) );

        // FreeSlotList�� Page�� ����Ѵ�.
        svpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
                                                       aFixedEntry,
                                                       sPagePtr );

        sNextPageID = sPagePtr->mHeader.mNextPageID;

        // FreePageHeader�� PrivatePageList ��ũ�� �����Ѵ�.
        svpFreePageList::addFreePageToPrivatePageList( aSpaceID,
                                                       sPagePtr->mHeader.mSelfPageID,
                                                       sPrevPageID,
                                                       sNextPageID );

        sPrevPageID = sPagePtr->mHeader.mSelfPageID;
    }

    IDE_DASSERT( sPagePtr == sAllocPageTail );

    // TailPage�� PrivatePageList�� ����Ѵ�.
    sPrivatePageList->mFixedFreePageTail =
        svpFreePageList::getFreePageHeader(aSpaceID,
                                           sAllocPageTail->mHeader.mSelfPageID);

    // ��ü�� AllocPageList ���
    IDE_TEST(sAllocPageList->mMutex->lock(NULL) != IDE_SUCCESS);

    IDE_TEST( svpAllocPageList::addPageList( aSpaceID,
                                             sAllocPageList,
                                             sAllocPageHead,
                                             sAllocPageTail,
                                             sAllocPageCnt )
              != IDE_SUCCESS );

    IDE_TEST(sAllocPageList->mMutex->unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 1:
            /* rollback�� �Ͼ�� �ȵȴ�. ������ �����ؾ� �Ѵ�. */
            /* BUGBUG assert ���� �ٸ� ó�� ��� ����غ� �� */
            IDE_ASSERT(0);
            break;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * fixed slot�� �Ҵ��Ѵ�.
 *
 * aTrans          : �۾��Ϸ��� Ʈ����� ��ü
 * aTableOID       : �Ҵ��Ϸ��� ���̺��� OID
 * aFixedEntry     : �Ҵ��Ϸ��� PageListEntry
 * aRow            : �Ҵ��ؼ� ��ȯ�Ϸ��� Row ������
 * aInfinite       : SCN Infinite
 * aMaxRow         : �ִ� Row ����
 * aOptFlag        :
 *           1. SMP_ALLOC_FIXEDSLOT_NONE
 *              ��۾��� �������� �ʴ´�.
 *           2. SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT
 *              Allocate�� ��û�ϴ� Table �� Record Count�� ������Ų��.
 *           3. SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER
 *              �Ҵ���� Slot�� Header�� Update�ϰ� Logging�϶�.
 *
 ***********************************************************************/
IDE_RC svpFixedPageList::allocSlot( void*             aTrans,
                                    scSpaceID         aSpaceID,
                                    void*             aTableInfoPtr,
                                    smOID             aTableOID,
                                    smpPageListEntry* aFixedEntry,
                                    SChar**           aRow,
                                    smSCN             aInfinite,
                                    ULong             aMaxRow,
                                    SInt              aOptFlag)
{
    UInt                sPageListID;
    smOID               sRecOID;
    ULong               sRecordCnt;
    scPageID            sPageID;
    smpFreeSlotHeader * sCurFreeSlotHeader;
    smpSlotHeader     * sCurSlotHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    IDE_ASSERT( aRow != NULL );
    IDE_ASSERT( aTableOID == aFixedEntry->mTableOID );

    sPageID     = SM_NULL_PID;
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    /* BUG-19573 Table�� Max Row�� Disable�Ǿ������� �� ���� insert�ÿ�
     *           Check���� �ʾƾ� �Ѵ�. */
    if( (aTableInfoPtr != NULL) && (aMaxRow != ID_ULONG_MAX) )
    {
        // BUG-47368: allocSlot, mutex ����, atomic �������� ����
        sRecordCnt = idCore::acpAtomicGet64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt) );
        sRecordCnt += smLayerCallback::getRecCntOfTableInfo( aTableInfoPtr );   

        IDE_TEST_RAISE((aMaxRow <= sRecordCnt) &&
                       ((aOptFlag & SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT)
                        == SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT),
                       err_exceed_maxrow);
    }

    /* need to alloc page from svmManager */
    while(1)
    {
        // 1) Tx's PrivatePageList���� ã��
        IDE_TEST( tryForAllocSlotFromPrivatePageList( aTrans,
                                                      aSpaceID,
                                                      aTableOID,
                                                      aFixedEntry,
                                                      aRow )
                  != IDE_SUCCESS );

        if(*aRow != NULL)
        {
            break;
        }

        // 2) FreePageList���� ã��
        IDE_TEST( tryForAllocSlotFromFreePageList( aTrans,
                                                   aSpaceID,
                                                   aFixedEntry,
                                                   sPageListID,
                                                   aRow )
                  != IDE_SUCCESS );

        if(*aRow != NULL)
        {
            break;
        }

        // 3) system���κ��� page�� �Ҵ�޴´�.
        IDE_TEST( allocPersPages( aTrans,
                                  aSpaceID,
                                  aFixedEntry )
                  != IDE_SUCCESS );

    }

    if( (aOptFlag & SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER)
        == SMP_ALLOC_FIXEDSLOT_SET_SLOTHEADER)
    {
        IDE_ASSERT( *aRow != NULL );

        sCurFreeSlotHeader = (smpFreeSlotHeader*)(*aRow);
        sPageID = SMP_SLOT_GET_PID((SChar *)sCurFreeSlotHeader);
        sRecOID = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( sCurFreeSlotHeader ) );

        if( SMP_SLOT_IS_USED( sCurFreeSlotHeader ) )
        {
            sCurSlotHeader = (smpSlotHeader*)sCurFreeSlotHeader;

            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL1);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL2);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL3, (ULong)sPageID, (ULong)sRecOID);

            dumpSlotHeader( sCurSlotHeader );

            IDE_ASSERT(0);
        }

        setAllocatedSlot( aInfinite, *aRow );
    }
    else
    {
        /* BUG-14513:
           DML(insert, update, delete)�� Alloc Slot�Ҷ� ���⼭ header�� update����
           �ʰ� insert, update, delete log�� ����Ŀ� header update�� �����Ѵ�.
           �׷��� ���⼭ slot header�� tid�� setting�ϴ� ������ Update, Delete��
           �ٸ� Transaction�� �̹� update�� ������ transaction�� ��ٸ��� next version
           �� tid�� ��ٸ� transaction�� �����ϱ� ������ tid�� setting�ؾ��Ѵ�.
           �׸��� tid�� ���� logging�� ���� �ʴ� ������ free slot�� tid�� ����� �Ǵ���
           ������ �ȵǱ� �����̴�.

           BUG-14953 :
           1. SCN�� infinite���� �����Ͽ� �ٸ� tx�� ��ٸ����� �Ѵ�.
           2. alloc�� �ϰ� DML �α׸� ���� ���� ���� ��쿡 restart��
              refine�� �ǰ� �ϱ� ���� SCN Delete bit�� �����Ѵ�.
              DML�α�ÿ� �ٽ� delete bit�� clear�ȴ�.

        */
        sCurSlotHeader = (smpSlotHeader*)*aRow;

        SM_SET_SCN( &(sCurSlotHeader->mCreateSCN), &aInfinite );

        /* svpFixedPageList::setFreeSlot���� SCN�� Delete Bit��
           Setting�Ǿ� �ִ��� Check��.*/
        SM_SET_SCN_DELETE_BIT( &(sCurSlotHeader->mCreateSCN) );
        sCurSlotHeader->mVarOID = SM_NULL_OID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_exceed_maxrow);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_ExceedMaxRows));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * slot�� free �Ѵ�.
 *
 * BUG-14093 Ager Tx�� freeSlot�� ���� commit���� ���� ��Ȳ����
 *           �ٸ� Tx�� �Ҵ�޾� ��������� ���� ����� �����߻�
 *           ���� Ager Tx�� Commit���Ŀ� FreeSlot�� FreeSlotList�� �Ŵܴ�.
 *
 * aTrans      : �۾��� �����ϴ� Ʈ����� ��ü
 * aFixedEntry : aRow�� ���� PageListEntry
 * aRow        : free�Ϸ��� slot
 * aTableType  : Temp Table�� slot������ ���� ����
 ***********************************************************************/
IDE_RC svpFixedPageList::freeSlot( void*             aTrans,
                                   scSpaceID         aSpaceID,
                                   smpPageListEntry* aFixedEntry,
                                   SChar*            aRow,
                                   smpTableType      aTableType,
                                   smSCN             aSCN )
{
    scPageID sPageID;
    smOID    sRowOID;

    IDE_DASSERT( ((aTrans != NULL) && (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aTableType == SMP_TABLE_TEMP)) );

    IDE_DASSERT( aFixedEntry != NULL );
    IDE_DASSERT( aRow != NULL );

    /* ----------------------------
     * BUG-14093
     * freeSlot������ slot�� ���� Free�۾��� �����ϰ�
     * ager Tx�� commit�� ���Ŀ� addFreeSlotPending�� �����Ѵ�.
     * ---------------------------*/

    (aFixedEntry->mRuntimeEntry->mDelRecCnt)++;

    sPageID = SMP_SLOT_GET_PID(aRow);

    IDE_TEST(setFreeSlot( aTrans,
                          aSpaceID,
                          sPageID,
                          aRow,
                          aTableType )
             != IDE_SUCCESS);

    if(aTableType == SMP_TABLE_NORMAL)
    {
        sRowOID = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( (smpSlotHeader*)aRow ) );

        // BUG-14093 freeSlot�ϴ� ager�� commit�ϱ� ������
        //           freeSlotList�� �Ŵ��� �ʰ� ager TX��
        //           commit ���Ŀ� �Ŵ޵��� OIDList�� �߰��Ѵ�.
        IDE_TEST( smLayerCallback::addOID( aTrans,
                                           aFixedEntry->mTableOID,
                                           sRowOID,
                                           aSpaceID,
                                           SM_OID_TYPE_FREE_FIXED_SLOT,
                                           aSCN )
                  != IDE_SUCCESS );
    }
    else
    {
        // TEMP Table�� �ٷ� FreeSlotList�� �߰��Ѵ�.
        IDE_TEST( addFreeSlotPending(aTrans,
                                     aSpaceID,
                                     aFixedEntry,
                                     aRow)
                 != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * nextOIDall�� ���� aRow���� �ش� Page�� ã���ش�.
 *
 * aFixedEntry : ��ȸ�Ϸ��� PageListEntry
 * aRow        : ���� Row
 * aPage       : aRow�� ���� Page�� ã�Ƽ� ��ȯ
 * aRowPtr     : aRow ���� Row ������
 ***********************************************************************/

inline void svpFixedPageList::initForScan( scSpaceID         aSpaceID,
                                           smpPageListEntry* aFixedEntry,
                                           SChar*            aRow,
                                           smpPersPage**     aPage,
                                           SChar**           aRowPtr )
{
    scPageID sPageID;

    IDE_DASSERT( aFixedEntry != NULL );
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
        *aRowPtr = aRow + aFixedEntry->mSlotSize;
    }
    else
    {
        sPageID = svpManager::getFirstAllocPageID(aFixedEntry);

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
 * FreePageHeader���� FreeSlot����
 *
 * aFreePageHeader : �����Ϸ��� FreePageHeader
 * aRow            : ������ FreeSlot�� Row������ ��ȯ
 **********************************************************************/

void svpFixedPageList::removeSlotFromFreeSlotList(
                                        smpFreePageHeader* aFreePageHeader,
                                        SChar**            aRow )
{
    smpFreeSlotHeader* sFreeSlotHeader;

    IDE_DASSERT(aRow != NULL);
    IDE_DASSERT(aFreePageHeader != NULL);
    IDE_DASSERT(aFreePageHeader->mFreeSlotCount > 0);

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    sFreeSlotHeader = (smpFreeSlotHeader*)(aFreePageHeader->mFreeSlotHead);

    *aRow = (SChar*)sFreeSlotHeader;

    aFreePageHeader->mFreeSlotCount--;

    if(sFreeSlotHeader->mNextFreeSlot == NULL)
    {
        // Next�� ���ٸ� ������ FreeSlot�̴�.
        IDE_ASSERT(aFreePageHeader->mFreeSlotCount == 0);

        aFreePageHeader->mFreeSlotTail = NULL;
    }
    else
    {
        // ���� FreeSlot�� Head�� ����Ѵ�.
        IDE_ASSERT(aFreePageHeader->mFreeSlotCount > 0);
    }

        aFreePageHeader->mFreeSlotHead =
                            (SChar*)sFreeSlotHeader->mNextFreeSlot;

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    return ;
}

/**********************************************************************
 * FreeSlot ������ ����Ѵ�.
 *
 * aTrans     : �۾��Ϸ��� Ʈ����� ��ü
 * aPageID    : FreeSlot�߰��Ϸ��� PageID
 * aRow       : FreeSlot�� Row ������
 * aTableType : Temp���̺����� ����
 **********************************************************************/
IDE_RC svpFixedPageList::setFreeSlot( void*          aTrans,
                                      scSpaceID      aSpaceID,
                                      scPageID       aPageID,
                                      SChar*         aRow,
                                      smpTableType   aTableType )
{
    smpSlotHeader       sAfterSlotHeader;
    SInt                sState = 0;
    smpSlotHeader     * sCurSlotHeader;
    smpSlotHeader     * sTmpSlotHeader;
    smOID               sRecOID;

    IDE_DASSERT( ((aTrans != NULL) && (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aTableType == SMP_TABLE_TEMP)) );

    IDE_ASSERT( aPageID != SM_NULL_PID );
    IDE_ASSERT( aRow != NULL );
    
    ACP_UNUSED( aTrans );

    sCurSlotHeader = (smpSlotHeader*)aRow;

    if( SMP_SLOT_IS_USED( sCurSlotHeader ) )
    {
        sState = 1;

        sRecOID = SM_MAKE_OID( aPageID,
                               SMP_SLOT_GET_OFFSET( sCurSlotHeader ) );

        idlOS::memcpy( &sAfterSlotHeader,
                       (SChar*)sCurSlotHeader,
                       ID_SIZEOF(smpSlotHeader) );

        // slot header ����
        SM_SET_SCN_FREE_ROW( &(sAfterSlotHeader.mCreateSCN) );
        SM_SET_SCN_FREE_ROW( &(sAfterSlotHeader.mLimitSCN) );
        SMP_SLOT_INIT_POSITION( &sAfterSlotHeader );

        if(aTableType == SMP_TABLE_NORMAL)
        {
            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID, 
                                               sRecOID,
                                               (void**)&sTmpSlotHeader )
                        == IDE_SUCCESS );

            IDE_ASSERT( SMP_SLOT_GET_OFFSET( sTmpSlotHeader )
                     == SMP_SLOT_GET_OFFSET( sCurSlotHeader ) );

            IDE_ASSERT( SMP_SLOT_GET_OFFSET( sTmpSlotHeader )
                     == SM_MAKE_OFFSET(sRecOID) );
        }

        // BUG-14373 ager�� seq-iterator���� ���ü� ����
        IDE_TEST(svmManager::holdPageXLatch(aSpaceID, aPageID) != IDE_SUCCESS);
        sState = 2;

        *sCurSlotHeader = sAfterSlotHeader;

        sState = 1;
        IDE_TEST(svmManager::releasePageLatch(aSpaceID, aPageID) != IDE_SUCCESS);

        sState = 0;
    }

    // BUG-37593
    IDE_ERROR( SM_SCN_IS_DELETED( sCurSlotHeader->mCreateSCN ) &&
               SM_SCN_IS_FREE_ROW( sCurSlotHeader->mLimitSCN ) &&
               SMP_SLOT_HAS_NULL_NEXT_OID( sCurSlotHeader ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 2:
            IDE_ASSERT( svmManager::releasePageLatch(aSpaceID,aPageID)
                        == IDE_SUCCESS );

        case 1:
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� FreeSlot�� FreeSlotList�� �߰��Ѵ�.
 *
 * BUG-14093 Commit���Ŀ� FreeSlot�� ���� FreeSlotList�� �Ŵܴ�.
 *
 * aTrans      : �۾��ϴ� Ʈ����� ��ü
 * aFixedEntry : FreeSlot�� ���� PageListEntry
 * aRow        : FreeSlot�� Row ������
 **********************************************************************/
IDE_RC svpFixedPageList::addFreeSlotPending( void*             aTrans,
                                             scSpaceID         aSpaceID,
                                             smpPageListEntry* aFixedEntry,
                                             SChar*            aRow )
{
    UInt               sState = 0;
    scPageID           sPageID;
    smpFreePageHeader* sFreePageHeader;

    IDE_DASSERT(aFixedEntry != NULL);
    IDE_DASSERT(aRow != NULL);

    sPageID = SMP_SLOT_GET_PID(aRow);
    sFreePageHeader = svpFreePageList::getFreePageHeader(aSpaceID, sPageID);

    IDE_TEST(sFreePageHeader->mMutex.lock(NULL) != IDE_SUCCESS);
    sState = 1;

    // PrivatePageList������ FreeSlot���� �ʴ´�.
    IDE_ASSERT(sFreePageHeader->mFreeListID != SMP_PRIVATE_PAGELISTID);

    // FreeSlot�� FreeSlotList�� �߰�
    addFreeSlotToFreeSlotList(sFreePageHeader, aRow);

    // FreeSlot�� �߰��� ���� SizeClass�� ����Ǿ����� Ȯ���Ͽ� �����Ѵ�.
    IDE_TEST(svpFreePageList::modifyPageSizeClass( aTrans,
                                                   aFixedEntry,
                                                   sFreePageHeader )
             != IDE_SUCCESS);

    if( sFreePageHeader->mFreeSlotCount == aFixedEntry->mSlotCount )
    {
        IDE_TEST( unlinkScanList( aSpaceID,
                                  sPageID,
                                  aFixedEntry )
                  != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST(sFreePageHeader->mMutex.unlock() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    if(sState == 1)
    {
        IDE_ASSERT(sFreePageHeader->mMutex.unlock() == IDE_SUCCESS);
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * FreePageHeader�� �ִ� FreeSlotList�� FreeSlot�߰�
 *
 * aFreePageHeader : FreeSlot�� ���� Page�� FreePageHeader
 * aRow            : FreeSlot�� Row ������
 **********************************************************************/

void svpFixedPageList::addFreeSlotToFreeSlotList(
                                        smpFreePageHeader* aFreePageHeader,
                                        SChar*             aRow )
{
    smpFreeSlotHeader* sCurFreeSlotHeader;
    smpFreeSlotHeader* sTailFreeSlotHeader;

    IDE_DASSERT( aFreePageHeader != NULL );
    IDE_DASSERT( aRow != NULL );

    sCurFreeSlotHeader = (smpFreeSlotHeader*)aRow;

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    sCurFreeSlotHeader->mNextFreeSlot = NULL;
    sTailFreeSlotHeader = (smpFreeSlotHeader*)aFreePageHeader->mFreeSlotTail;

    /* BUG-32386       [sm_recovery] If the ager remove the MMDB slot and the
     * checkpoint thread flush the page containing the slot at same time, the
     * server can misunderstand that the freed slot is the allocated slot. 
     * DropFlag�� �����ž��µ�, SCN�� �ʱ�ȭ���� ���� ��찡 �߻��� �� ����
     * SCN�� �ʱ�ȭ ����*/
    SM_SET_SCN_FREE_ROW( &sCurFreeSlotHeader->mCreateSCN );
    SM_SET_SCN_FREE_ROW( &sCurFreeSlotHeader->mLimitSCN );
    SMP_SLOT_INIT_POSITION( sCurFreeSlotHeader );

    if(sTailFreeSlotHeader == NULL)
    {
        IDE_DASSERT( aFreePageHeader->mFreeSlotHead == NULL );

        aFreePageHeader->mFreeSlotHead = aRow;
    }
    else
    {
        sTailFreeSlotHeader->mNextFreeSlot = (smpFreeSlotHeader*)aRow;
    }

    aFreePageHeader->mFreeSlotTail = aRow;

    aFreePageHeader->mFreeSlotCount++;

    IDE_DASSERT( isValidFreeSlotList(aFreePageHeader) == ID_TRUE );

    return;
}

/**********************************************************************
 * PageList�� ��ȿ�� ���ڵ� ���� ��ȯ
 *
 * aFixedEntry  : �˻��ϰ��� �ϴ� PageListEntry
 * aRecordCount : ��ȯ�ϴ� ���ڵ� ����
 **********************************************************************/
ULong svpFixedPageList::getRecordCount( smpPageListEntry* aFixedEntry )
{
    ULong sRecordCount;

    if( aFixedEntry->mRuntimeEntry != NULL )
    {
        // BUG-47368: getRecordCount, mutex ����, atomic �������� ����
        sRecordCount = idCore::acpAtomicGet64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt) );
    }
    else
    {
        sRecordCount = 0;
    }

    return sRecordCount;
}

/**********************************************************************
 * PageList�� ��ȿ�� ���ڵ� ���� ����
 *
 * aFixedEntry  : �˻��ϰ��� �ϴ� PageListEntry
 * aRecordCount : ���ڵ� ����
 **********************************************************************/
IDE_RC svpFixedPageList::setRecordCount( smpPageListEntry* aFixedEntry,
                                         ULong             aRecordCount )
{
    // BUG-47368: setRecordCount, mutex ����, atomic �������� ���� 
    (void)idCore::acpAtomicSet64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt), aRecordCount );

    return IDE_SUCCESS;
}

/**********************************************************************
 * PageListEntry�� �ʱ�ȭ�Ѵ�.
 *
 * aFixedEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 * aTableOID   : PageListEntry�� ���̺� OID
 * aSlotSize   : PageListEntry�� SlotSize
 **********************************************************************/
void svpFixedPageList::initializePageListEntry( smpPageListEntry* aFixedEntry,
                                                smOID             aTableOID,
                                                vULong            aSlotSize )
{
    IDE_DASSERT(aFixedEntry != NULL);
    IDE_DASSERT(aTableOID != 0);
    IDE_DASSERT(aSlotSize > 0);

    aFixedEntry->mTableOID     = aTableOID;
    aFixedEntry->mSlotSize     = aSlotSize;
    aFixedEntry->mSlotCount    =
        SMP_PERS_PAGE_BODY_SIZE / aFixedEntry->mSlotSize;
    aFixedEntry->mRuntimeEntry = NULL;

    return;
}

/**********************************************************************
 * aRow�� SlotHeader�� Update�Ͽ� Allocate�� Slot���� �����.
 *
 * aTrans    : Transaction Pointer
 * aInfinite : Slot Header�� setting�� SCN��.
 * aRow      : Record�� Pointer
 **********************************************************************/
void svpFixedPageList::setAllocatedSlot( smSCN  aInfinite,
                                         SChar *aRow )
{
    smpSlotHeader  *sCurSlotHeader;

    // Init header of fixed row
    sCurSlotHeader = (smpSlotHeader*)aRow;

    SM_SET_SCN( &(sCurSlotHeader->mCreateSCN), &aInfinite );

    SM_SET_SCN_FREE_ROW( &(sCurSlotHeader->mLimitSCN) );

    SMP_SLOT_INIT_POSITION( sCurSlotHeader );
    SMP_SLOT_SET_USED( sCurSlotHeader );
}

#ifdef DEBUG
/**********************************************************************
 * Page���� FreeSlotList�� ������ �ùٸ��� �˻��Ѵ�.
 *
 * aFreePageHeader : �˻��Ϸ��� FreeSlotList�� �ִ� Page�� FreePageHeader
 **********************************************************************/
idBool
svpFixedPageList::isValidFreeSlotList(smpFreePageHeader* aFreePageHeader )
{
    idBool             sIsValid;

    if ( iduProperty::getEnableRecTest() == 1 )
    {
        vULong             sPageCount = 0;
        smpFreeSlotHeader* sCurFreeSlotHeader = NULL;
        smpFreeSlotHeader* sNxtFreeSlotHeader;
        smpFreePageHeader* sFreePageHeader = aFreePageHeader;

        IDE_DASSERT( sFreePageHeader != NULL );

        sIsValid = ID_FALSE;

        sNxtFreeSlotHeader = (smpFreeSlotHeader*)sFreePageHeader->mFreeSlotHead;

        while(sNxtFreeSlotHeader != NULL)
        {
            sCurFreeSlotHeader = sNxtFreeSlotHeader;

            sPageCount++;

            sNxtFreeSlotHeader = sCurFreeSlotHeader->mNextFreeSlot;
        }

        if(aFreePageHeader->mFreeSlotCount == sPageCount &&
           aFreePageHeader->mFreeSlotTail  == (SChar*)sCurFreeSlotHeader)
        {
            sIsValid = ID_TRUE;
        }


        if ( sIsValid == ID_FALSE )
        {
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST1,
                        (ULong)aFreePageHeader->mSelfPageID);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST2,
                        aFreePageHeader->mFreeSlotCount);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST3,
                        sPageCount);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST4,
                        aFreePageHeader->mFreeSlotHead);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST5,
                        aFreePageHeader->mFreeSlotTail);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                        SM_TRC_MPAGE_INVALID_FREE_SLOT_LIST6,
                        sCurFreeSlotHeader);

#    if defined(TSM_DEBUG)
            idlOS::printf( "Invalid Free Slot List Detected. Page #%"ID_UINT64_FMT"\n",
                           (ULong) aFreePageHeader->mSelfPageID );
            idlOS::printf( "Free Slot Count on Page ==> %"ID_UINT64_FMT"\n",
                           aFreePageHeader->mFreeSlotCount );
            idlOS::printf( "Free Slot Count on List ==> %"ID_UINT64_FMT"\n",
                           sPageCount );
            idlOS::printf( "Free Slot Head on Page ==> %"ID_xPOINTER_FMT"\n",
                           aFreePageHeader->mFreeSlotHead );
            idlOS::printf( "Free Slot Tail on Page ==> %"ID_xPOINTER_FMT"\n",
                           aFreePageHeader->mFreeSlotTail );
            idlOS::printf( "Free Slot Tail on List ==> %"ID_xPOINTER_FMT"\n",
                           sCurFreeSlotHeader );
            fflush( stdout );
#    endif  /* TSM_DEBUG */
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
 * Description: <aSpaceID, aPageID>�� �ִ� Record Header���� altibase_boot.log
 *              �� ��´�.
 *
 * aSpaceID    - [IN] SpaceID
 * aPageID     - [IN] Page ID
 * aFxiedEntry - [IN] Page List Entry
 **********************************************************************/
IDE_RC svpFixedPageList::dumpFixedPage( scSpaceID         aSpaceID,
                                        scPageID          aPageID,
                                        smpPageListEntry *aFixedEntry )
{
    UInt                sSlotCnt  = aFixedEntry->mSlotCount;
    UInt                sSlotSize = aFixedEntry->mSlotSize;
    UInt                i;
    smpSlotHeader     * sCurSlotHeader;
    smpSlotHeader     * sNxtSlotHeader;
    UInt                sCurOffset;
    SChar             * sPagePtr;
    smpPersPageHeader * sHeader;

    IDE_ERROR( aFixedEntry != NULL );

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            aPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );
    sCurOffset     = (UShort)SMP_PERS_PAGE_BODY_OFFSET;
    sHeader        = (smpPersPageHeader*)sPagePtr;
    sNxtSlotHeader = (smpSlotHeader*)(sPagePtr + sCurOffset);

    ideLog::log(IDE_SERVER_0,
                "Volatile FixedPage\n"
                "SpaceID      : %u\n"
                "PageID       : %u\n"
                "SelfPID      : %u\n"
                "PrevPID      : %u\n"
                "NextPID      : %u\n"
                "Type         : %u\n"
                "TableOID     : %lu\n"
                "AllocListID  : %u\n",
                aSpaceID,
                aPageID,
                sHeader->mSelfPageID,
                sHeader->mPrevPageID,
                sHeader->mNextPageID,
                sHeader->mType,
                sHeader->mTableOID,
                sHeader->mAllocListID );

    for( i = 0; i < sSlotCnt; i++)
    {
        sCurSlotHeader = sNxtSlotHeader;

        dumpSlotHeader( sCurSlotHeader );

        sNxtSlotHeader = (smpSlotHeader*)((SChar*)sCurSlotHeader + sSlotSize);
        sCurOffset += sSlotSize;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
