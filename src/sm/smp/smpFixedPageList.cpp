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
 * $Id: smpFixedPageList.cpp 88917 2020-10-15 04:54:02Z et16 $
 **********************************************************************/

#include <idl.h>
#include <ideErrorMgr.h>
#include <idErrorCode.h>
#include <smErrorCode.h>
#include <smm.h>
#include <smr.h>
#include <smpDef.h>
#include <smpFixedPageList.h>
#include <smpFreePageList.h>
#include <smpAllocPageList.h>
#include <smmExpandChunk.h>
#include <smpReq.h>

/**********************************************************************
 * Tx's PrivatePageList�� FreePage�κ��� Slot�� �Ҵ��� �� ������ �˻��ϰ�
 * �����ϸ� �Ҵ��Ѵ�.
 *
 * aTrans     : �۾��ϴ� Ʈ����� ��ü
 * aTableOID  : �Ҵ��Ϸ��� ���̺� OID
 * aRow       : �Ҵ��ؼ� ��ȯ�Ϸ��� Slot ������
 **********************************************************************/

IDE_RC smpFixedPageList::tryForAllocSlotFromPrivatePageList(
    void              * aTrans,
    scSpaceID           aSpaceID,
    smOID               aTableOID,
    smpPageListEntry  * aFixedEntry,
    SChar            ** aRow )
{
    smpPrivatePageListEntry* sPrivatePageList = NULL;
    smpFreePageHeader*       sFreePageHeader;

    IDE_DASSERT(aTrans != NULL);
    IDE_DASSERT(aRow != NULL);

    *aRow = NULL;

    IDE_TEST( smLayerCallback::findPrivatePageList( aTrans,
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
                smpFreePageList::removeFixedFreePageFromPrivatePageList(
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
IDE_RC smpFixedPageList::tryForAllocSlotFromFreePageList(
    void*             aTrans,
    scSpaceID         aSpaceID,
    smpPageListEntry* aFixedEntry,
    UInt              aPageListID,
    SChar**           aRow )
{
    UInt                  sState = 0;
    UInt                  sSizeClassID;
    idBool                sIsPageAlloced;
    smpFreePageHeader*    sFreePageHeader = NULL;
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
                IDE_TEST(sFreePageHeader->mMutex.lock( NULL )
                         != IDE_SUCCESS);
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
                    IDE_TEST(smpFreePageList::modifyPageSizeClass(
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

        IDE_TEST( smpFreePageList::tryForAllocPagesFromPool( aFixedEntry,
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
IDE_RC smpFixedPageList::unlinkScanList( scSpaceID          aSpaceID,
                                         scPageID           aPageID,
                                         smpPageListEntry * aFixedEntry )
{
    smmPCH               * sMyPCH;
    smmPCH               * sNxtPCH;
    smmPCH               * sPrvPCH;
    smpScanPageListEntry * sScanPageList;

    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    IDU_FIT_POINT("smpFixedPageList::unlinkScanList::wait1");

    IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

    IDE_DASSERT( sScanPageList->mHeadPageID != SM_NULL_PID );
    IDE_DASSERT( sScanPageList->mTailPageID != SM_NULL_PID );

    sMyPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                           aPageID ));
    IDE_DASSERT( sMyPCH != NULL );
    IDE_DASSERT( sMyPCH->mNxtScanPID != SM_NULL_PID );
    IDE_DASSERT( sMyPCH->mPrvScanPID != SM_NULL_PID );

    /* BUG-43463 Fullscan ���ü� ����,
     * - smnnSeq::moveNextNonBlock()�� ���� ���ü� ����� ���� atomic ����
     * - ����� modify_seq, nexp_pid, prev_pid�̴�.
     * - modify_seq�� link, unlink������ ����ȴ�.
     * - link, unlink ���� �߿��� modify_seq�� Ȧ���̴�
     * - ù��° page�� lock�� ��� Ȯ���ϹǷ� atomic���� set���� �ʾƵ� �ȴ�.
     * */
    SMM_PCH_SET_MODIFYING( sMyPCH );

    if( sScanPageList->mHeadPageID == aPageID )
    {
        if( sMyPCH->mNxtScanPID != SM_SPECIAL_PID )
        {
            IDE_DASSERT( sScanPageList->mTailPageID != aPageID );
            IDE_DASSERT( sMyPCH->mPrvScanPID == SM_SPECIAL_PID );
            sNxtPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
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
        sPrvPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                                sMyPCH->mPrvScanPID ));

        if( sMyPCH->mNxtScanPID != SM_SPECIAL_PID )
        {
            sNxtPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
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

    idCore::acpAtomicSet32( &(sMyPCH->mPrvScanPID),
                            SM_NULL_PID );

    SMM_PCH_SET_MODIFIED( sMyPCH );

    IDE_TEST( sScanPageList->mMutex->unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************************
 *
 * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
 *
 * �־��� �������� Scan List�� �߰��Ѵ�.
 *
 ****************************************************************************/
IDE_RC smpFixedPageList::linkScanList( scSpaceID          aSpaceID,
                                       scPageID           aPageID,
                                       smpPageListEntry * aFixedEntry )
{
    smmPCH               * sMyPCH;
    smmPCH               * sTailPCH;
    smpScanPageListEntry * sScanPageList;
#ifdef DEBUG
    smpFreePageHeader    * sFreePageHeader;
#endif
    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    IDE_TEST( sScanPageList->mMutex->lock( NULL ) != IDE_SUCCESS );

#ifdef DEBUG
    sFreePageHeader = smpFreePageList::getFreePageHeader( aSpaceID,
                                                          aPageID );

    IDE_DASSERT( (sFreePageHeader == NULL) ||
                 (sFreePageHeader->mFreeSlotCount != aFixedEntry->mSlotCount) );
#endif
    sMyPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                           aPageID ));
    IDE_DASSERT( sMyPCH != NULL );
    IDE_DASSERT( sMyPCH->mNxtScanPID == SM_NULL_PID );
    IDE_DASSERT( sMyPCH->mPrvScanPID == SM_NULL_PID );

    /* BUG-43463 Fullscan ���ü� ����,
     * - smnnSeq::moveNextNonBlock()�� ���� ���ü� ����� ���� atomic ����
     * - ����� modify_seq, nexp_pid, prev_pid�̴�.
     * - modify_seq�� link, unlink������ ����ȴ�.
     * - link, unlink ���� �߿��� modify_seq�� Ȧ���̴�
     * - ù��° page�� lock�� ��� Ȯ���ϹǷ� atomic���� set���� �ʾƵ� �ȴ�.
     * */
    SMM_PCH_SET_MODIFYING( sMyPCH );
    /*
     * Scan List�� ��� �ִٸ�
     */
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
        sTailPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                                 sScanPageList->mTailPageID ));
        idCore::acpAtomicSet32( &(sTailPCH->mNxtScanPID),
                                aPageID );

        idCore::acpAtomicSet32( &(sMyPCH->mNxtScanPID),
                                SM_SPECIAL_PID );

        idCore::acpAtomicSet32( &(sMyPCH->mPrvScanPID),
                                sScanPageList->mTailPageID );

        sScanPageList->mTailPageID = aPageID;
    }

    SMM_PCH_SET_MODIFIED( sMyPCH );

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
IDE_RC smpFixedPageList::setRuntimeNull( smpPageListEntry* aFixedEntry )
{
    IDE_DASSERT( aFixedEntry != NULL );

    // RuntimeEntry �ʱ�ȭ
    IDE_TEST(smpFreePageList::setRuntimeNull( aFixedEntry )
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
IDE_RC smpFixedPageList::initEntryAtRuntime(
    smOID                  aTableOID,
    smpPageListEntry*      aFixedEntry,
    smpAllocPageListEntry* aAllocPageList )
{
    SChar                  sBuffer[128];
    smpScanPageListEntry * sScanPageList;

    IDE_DASSERT( aFixedEntry != NULL );
    IDE_DASSERT( aAllocPageList != NULL );

    IDE_ASSERT( aTableOID == aFixedEntry->mTableOID);

    // RuntimeEntry �ʱ�ȭ
    IDE_TEST(smpFreePageList::initEntryAtRuntime( aFixedEntry )
             != IDE_SUCCESS);

    aFixedEntry->mRuntimeEntry->mAllocPageList = aAllocPageList;
    sScanPageList = &(aFixedEntry->mRuntimeEntry->mScanPageList);

    sScanPageList->mHeadPageID = SM_NULL_PID;
    sScanPageList->mTailPageID = SM_NULL_PID;

    idlOS::snprintf( sBuffer, 128,
                     "SCAN_PAGE_LIST_MUTEX_%"
                     ID_XINT64_FMT"",
                     (ULong)aTableOID );

    /* smpFixedPageList_initEntryAtRuntime_malloc_Mutex.tc */
    IDU_FIT_POINT("smpFixedPageList::initEntryAtRuntime::malloc::Mutex");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SMP,
                                 sizeof(iduMutex),
                                 (void **)&(sScanPageList->mMutex),
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );

    IDE_TEST( sScanPageList->mMutex->initialize( sBuffer,
                                                 IDU_MUTEX_KIND_NATIVE,
                                                 IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * memory table��  fixed page list entry�� �����ϴ� runtime ���� ����
 *
 * aFixedEntry : �����Ϸ��� PageListEntry
 ***********************************************************************/
IDE_RC smpFixedPageList::finEntryAtRuntime( smpPageListEntry* aFixedEntry )
{
    UInt                   sPageListID;
    smpScanPageListEntry * sScanPageList;

    IDE_DASSERT( aFixedEntry != NULL );

    if ( aFixedEntry->mRuntimeEntry != NULL )
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
            smpAllocPageList::finEntryAtRuntime(
                &(aFixedEntry->mRuntimeEntry->mAllocPageList[sPageListID]) );
        }

        // RuntimeEntry ����
        IDE_TEST(smpFreePageList::finEntryAtRuntime(aFixedEntry)
                 != IDE_SUCCESS);

        // smpFreePageList::finEntryAtRuntime���� RuntimeEntry�� NULL�� ����
        IDE_ASSERT( aFixedEntry->mRuntimeEntry == NULL );
    }
    else
    {
        // Do Nothing
        // OFFLINE/DISCARD�� Table�� ��� mRuntimeEntry�� NULL�� ���� �ִ�.
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
IDE_RC smpFixedPageList::freePageListToDB( void*             aTrans,
                                           scSpaceID         aSpaceID,
                                           smOID             aTableOID,
                                           smpPageListEntry* aFixedEntry )
{
    UInt sPageListID;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );
    IDE_DASSERT( aTableOID == aFixedEntry->mTableOID );

    // [0] FreePageList ����

    smpFreePageList::initializeFreePageListAndPool(aFixedEntry);

    /* ----------------------------
     * [1] fixed page list��
     *     smmManager�� ��ȯ�Ѵ�.
     * ---------------------------*/

    for(sPageListID = 0;
        sPageListID < SMP_PAGE_LIST_COUNT;
        sPageListID++)
    {
        // for AllocPageList
        IDE_TEST( smpAllocPageList::freePageListToDB(
                      aTrans,
                      aSpaceID,
                      &(aFixedEntry->mRuntimeEntry->mAllocPageList[sPageListID]),
                      aTableOID )
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
void smpFixedPageList::initializePage( vULong       aSlotSize,
                                       vULong       aSlotCount,
                                       UInt         aPageListID,
                                       smOID        aTableOID,
                                       smpPersPage* aPage )
{
    UInt sSlotCounter;
    UShort sCurOffset;
    smpFreeSlotHeader* sCurFreeSlotHeader = NULL;
    smpFreeSlotHeader* sNextFreeSlotHeader;

    IDE_DASSERT( aSlotSize > 0 );
    IDE_DASSERT( aSlotCount > 0 );
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

    // fix BUG-26934 : [codeSonar] Null Pointer Dereference
    IDE_ASSERT( sCurFreeSlotHeader != NULL );

    sCurFreeSlotHeader->mNextFreeSlot = NULL;

    IDL_MEM_BARRIER;


    return;
}
/***********************************************************************
 * DB���� Page���� �Ҵ�޴´�.
 *
 * fixed slot�� ���� persistent page�� system���κ��� �Ҵ�޴´�.
 *
 * aTrans      : �۾��ϴ� Ʈ����� ��ü
 * aFixedEntry : �Ҵ���� PageListEntry
 ***********************************************************************/
IDE_RC smpFixedPageList::allocPersPages( void*             aTrans,
                                         scSpaceID         aSpaceID,
                                         smpPageListEntry* aFixedEntry )
{
    UInt                     sState = 0;
    UInt                     sPageListID;
    smLSN                    sNTA;
    smOID                    sTableOID;
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
    smmPCH                 * sPCH;
    UInt                     sAllocPageCnt = 0;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    sTableOID      = aFixedEntry->mTableOID;

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

    IDE_TEST( smLayerCallback::findPrivatePageList( aTrans,
                                                    aFixedEntry->mTableOID,
                                                    &sPrivatePageList )
              != IDE_SUCCESS );

    if(sPrivatePageList == NULL)
    {
        // ������ PrivatePageList�� �����ٸ� ���� �����Ѵ�.
        IDE_TEST( smLayerCallback::createPrivatePageList(
                      aTrans,
                      aFixedEntry->mTableOID,
                      &sPrivatePageList )
                  != IDE_SUCCESS );
    }

    // Begin NTA
    sNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );
    sState = 1;

    // DB���� �޾ƿ���
    IDE_TEST( smmManager::allocatePersPageList( aTrans,
                                                aSpaceID,
                                                SMP_ALLOCPAGECOUNT_FROMDB,
                                                (void **)&sAllocPageHead,
                                                (void **)&sAllocPageTail,
                                                &sAllocPageCnt )
              != IDE_SUCCESS);

    IDE_DASSERT( smpAllocPageList::isValidPageList(
                     aSpaceID,
                     sAllocPageHead->mHeader.mSelfPageID,
                     sAllocPageTail->mHeader.mSelfPageID,
                     sAllocPageCnt )
                 == ID_TRUE );

    // �Ҵ���� HeadPage�� PrivatePageList�� ����Ѵ�.
    if( sPrivatePageList->mFixedFreePageTail == NULL )
    {
        IDE_DASSERT( sPrivatePageList->mFixedFreePageHead == NULL );

        sPrivatePageList->mFixedFreePageHead =
            smpFreePageList::getFreePageHeader(
                aSpaceID,
                sAllocPageHead->mHeader.mSelfPageID);
    }
    else
    {
        IDE_DASSERT( sPrivatePageList->mFixedFreePageHead != NULL );

        sPrivatePageList->mFixedFreePageTail->mFreeNext =
            smpFreePageList::getFreePageHeader(
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
        sState = 3;

        IDE_TEST( smrUpdate::initFixedPage(NULL, /* idvSQL* */
                                           aTrans,
                                           aSpaceID,
                                           sPagePtr->mHeader.mSelfPageID,
                                           sPageListID,
                                           aFixedEntry->mSlotSize,
                                           aFixedEntry->mSlotCount,
                                           aFixedEntry->mTableOID)
                  != IDE_SUCCESS);


        // PersPageHeader �ʱ�ȭ�ϰ� (FreeSlot���� �����Ѵ�.)
        initializePage( aFixedEntry->mSlotSize,
                        aFixedEntry->mSlotCount,
                        sPageListID,
                        aFixedEntry->mTableOID,
                        sPagePtr );

        sPCH = (smmPCH*)(smmManager::getPCH( aSpaceID,
                                             sPagePtr->mHeader.mSelfPageID ));
        sPCH->mNxtScanPID       = SM_NULL_PID;
        sPCH->mPrvScanPID       = SM_NULL_PID;
        sPCH->mModifySeqForScan = 0;

        sState = 2;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                                sPagePtr->mHeader.mSelfPageID)
                  != IDE_SUCCESS );

        // FreePageHeader �ʱ�ȭ�ϰ�
        smpFreePageList::initializeFreePageHeader(
            smpFreePageList::getFreePageHeader(aSpaceID, sPagePtr) );

        // FreeSlotList�� Page�� ����Ѵ�.
        smpFreePageList::initializeFreeSlotListAtPage( aSpaceID,
                                                       aFixedEntry,
                                                       sPagePtr );

        sNextPageID = sPagePtr->mHeader.mNextPageID;

        // FreePageHeader�� PrivatePageList ��ũ�� �����Ѵ�.
        smpFreePageList::addFreePageToPrivatePageList( aSpaceID,
                                                       sPagePtr->mHeader.mSelfPageID,
                                                       sPrevPageID,
                                                       sNextPageID );

        sPrevPageID = sPagePtr->mHeader.mSelfPageID;
    }

    IDE_DASSERT( sPagePtr == sAllocPageTail );

    // TailPage�� PrivatePageList�� ����Ѵ�.
    sPrivatePageList->mFixedFreePageTail =
        smpFreePageList::getFreePageHeader(aSpaceID,
                                           sAllocPageTail->mHeader.mSelfPageID);

    // ��ü�� AllocPageList ���
    IDE_TEST(sAllocPageList->mMutex->lock(NULL /* idvSQL* */) != IDE_SUCCESS);
    sState = 2;

    IDE_TEST( smpAllocPageList::addPageList( aTrans,
                aSpaceID,
                sAllocPageList,
                sTableOID,
                sAllocPageHead,
                sAllocPageTail,
                sAllocPageCnt )
            != IDE_SUCCESS);

    // End NTA[-1-]
    IDE_TEST(smrLogMgr::writeNTALogRec( NULL, /* idvSQL* */
                                        aTrans,
                                        &sNTA,
                                        SMR_OP_NULL )
             != IDE_SUCCESS);

    sState = 0;
    IDE_TEST(sAllocPageList->mMutex->unlock() != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 3:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aSpaceID,
                                                      sPagePtr->mHeader.mSelfPageID)
                        == IDE_SUCCESS );

        case 2:
            // DB���� TAB���� Page���� �����Դµ� ��ó TAB�� ���� ���ߴٸ� �ѹ�
            IDE_ASSERT( smrRecoveryMgr::undoTrans( NULL, /* idvSQL* */
                                                   aTrans,
                                                   &sNTA )
                        == IDE_SUCCESS );
            IDE_ASSERT( sAllocPageList->mMutex->unlock() == IDE_SUCCESS );
            break;

        case 1:
            // DB���� TAB���� Page���� �����Դµ� ��ó TAB�� ���� ���ߴٸ� �ѹ�
            IDE_ASSERT( smrRecoveryMgr::undoTrans( NULL, /* idvSQL* */
                                                   aTrans,
                                                   &sNTA)
                        == IDE_SUCCESS );
            break;

        default:
            break;
    }

    IDE_POP();


    return IDE_FAILURE;
}

/***********************************************************************
 * temporary table header�� ���� fixed slot �Ҵ�
 *
 * temporary table header�� �����Ұ��, slot �Ҵ翡 ���� �α��� ���� �ʵ���
 * ó���ϰ�, ��, system���κ��� persistent page�� �Ҵ��ϴ� ���꿡 ���ؼ���
 * �α��� �ϵ��� �Ѵ�.
 *
 * aTableOID   : �Ҵ��Ϸ��� ���̺��� OID
 * aFixedEntry : �Ҵ��Ϸ��� PageListEntry
 * aRow        : �Ҵ��ؼ� ��ȯ�Ϸ��� Row ������
 * aInfinite   : SCN Infinite ��
 ***********************************************************************/
IDE_RC smpFixedPageList::allocSlotForTempTableHdr( scSpaceID         aSpaceID,
                                                   smOID             aTableOID,
                                                   smpPageListEntry* aFixedEntry,
                                                   SChar**           aRow,
                                                   smSCN             aInfinite )
{
    SInt                sState = 0;
    UInt                sPageListID;
    smSCN               sInfiniteSCN;
    smOID               sRecOID;
    scPageID            sPageID;
    smpFreeSlotHeader * sCurFreeSlotHeader;
    smpSlotHeader     * sCurSlotHeader;
    void*               sDummyTx;

    IDE_DASSERT(aFixedEntry != NULL);

    sPageID  = SM_NULL_PID;

    IDE_ASSERT(aRow != NULL);
    IDE_ASSERT(aTableOID == aFixedEntry->mTableOID);

    // BUG-8083
    // temp table ���� statement�� untouchable �Ӽ��̹Ƿ�
    // �α��� ���� ���Ѵ�. �׷��Ƿ� ���ο� tx�� �Ҵ��Ͽ�
    // PageListID�� �����ϰ�,
    // system���κ��� page�� �Ҵ������ �α��� ó���ϵ��� �Ѵ�.
    IDE_TEST( smLayerCallback::allocTx( &sDummyTx ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smLayerCallback::beginTx( sDummyTx,
                                        SMI_TRANSACTION_REPL_DEFAULT, // Replicate
                                        NULL )     // SessionFlagPtr
              != IDE_SUCCESS );
    sState = 2;

    smLayerCallback::allocRSGroupID( sDummyTx, &sPageListID );

    while(1)
    {
        // 1) Tx's PrivatePageList���� ã��
        IDE_TEST( tryForAllocSlotFromPrivatePageList( sDummyTx,
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
        IDE_TEST( tryForAllocSlotFromFreePageList( sDummyTx,
                                                   aSpaceID,
                                                   aFixedEntry,
                                                   sPageListID,
                                                   aRow )
                  != IDE_SUCCESS );

        if( *aRow != NULL)
        {
            break;
        }

        // 3) system���κ��� page�� �Ҵ�޴´�.
        IDE_TEST( allocPersPages( sDummyTx,
                                  aSpaceID,
                                  aFixedEntry )
                  != IDE_SUCCESS );
    }

    IDU_FIT_POINT( "1.BUG-48210@smpFixedPageList::allocSlotForTempTableHdr" );
    IDE_TEST( smLayerCallback::commitTx( sDummyTx ) != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( smLayerCallback::freeTx( sDummyTx ) != IDE_SUCCESS );
    sState = 0;

    IDE_ASSERT( *aRow != NULL );

    /* ------------------------------------------------
     * �Ҵ�� slot header�� �ʱ�ȭ�Ѵ�. temporary table
     * header�� �����ϱ� ���� slot �Ҵ�ÿ���
     * �α��� ���� �ʵ��� ó���Ѵ�. initFixedRow��
     * NTA�� ���Ͽ� �α����� ����
     * ----------------------------------------------*/
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

    sState = 3;

    // Init header of fixed row
    sInfiniteSCN = aInfinite;

    SM_SET_SCN_DELETE_BIT( &sInfiniteSCN );
    SM_SET_SCN( &(sCurFreeSlotHeader->mCreateSCN) , &sInfiniteSCN );

    SM_SET_SCN_FREE_ROW( &(sCurFreeSlotHeader->mLimitSCN) );

    SMP_SLOT_INIT_POSITION( sCurFreeSlotHeader );
    SMP_SLOT_SET_USED( sCurFreeSlotHeader );

    sState = 0;
    IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID)
             != IDE_SUCCESS);

    /* BUG-47368: ���̺� ����̱� ������ MAX ROW üũ���� �ʰ�
     *            mutex ����, atomic �������� ����
     */
    (void)idCore::acpAtomicInc64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch (sState)
    {
        case 3:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID)
                        == IDE_SUCCESS );
            break;

        case 2: /* BUG-48210 commit ���п� ���� ���� ó�� ���� */
            IDE_ASSERT( smLayerCallback::abortTx( sDummyTx )
                        == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( smLayerCallback::freeTx( sDummyTx )
                        == IDE_SUCCESS );
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
 * aOPType         : �Ҵ��Ϸ��� �۾� ����
 ***********************************************************************/
IDE_RC smpFixedPageList::allocSlot( void*             aTrans,
                                    scSpaceID         aSpaceID,
                                    void*             aTableInfoPtr,
                                    smOID             aTableOID,
                                    smpPageListEntry* aFixedEntry,
                                    SChar**           aRow,
                                    smSCN             aInfinite,
                                    ULong             aMaxRow,
                                    SInt              aOptFlag,
                                    smrOPType         aOPType)
{
    SInt                sState = 0;
    UInt                sPageListID;
    smLSN               sNTA;
    smOID               sRecOID;
    ULong               sRecordCnt;
    scPageID            sPageID;
    smpFreeSlotHeader * sCurFreeSlotHeader;
    smpSlotHeader     * sCurSlotHeader;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    IDE_ASSERT(( aOPType == SMR_OP_SMC_FIXED_SLOT_ALLOC ) ||
               ( aOPType == SMR_OP_SMC_TABLEHEADER_ALLOC ));

    IDE_ASSERT( aRow != NULL );
    IDE_ASSERT( aTableOID == aFixedEntry->mTableOID );

    sPageID     = SM_NULL_PID;
    smLayerCallback::allocRSGroupID( aTrans, &sPageListID );

    /* BUG-19573 Table�� Max Row�� Disable�Ǿ������� �� ���� insert�ÿ�
     *           Check���� �ʾƾ� �Ѵ�. */
    if(( aTableInfoPtr != NULL ) && ( aMaxRow != ID_ULONG_MAX ))
    {
        // BUG-47368: allocSlot, mutex ����, atomic �������� ����
        sRecordCnt = idCore::acpAtomicGet64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt) );
        sRecordCnt += smLayerCallback::getRecCntOfTableInfo( aTableInfoPtr ); 

        IDE_TEST_RAISE((aMaxRow <= sRecordCnt) &&
                       ((aOptFlag & SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT)
                        == SMP_ALLOC_FIXEDSLOT_ADD_INSERTCNT),
                       err_exceed_maxrow);
    }

    /* need to alloc page from smmManager */
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

        // Begin NTA[-2-]
        sNTA = smLayerCallback::getLstUndoNxtLSN( aTrans );

        sCurFreeSlotHeader = (smpFreeSlotHeader*)(*aRow);
        sPageID = SMP_SLOT_GET_PID((SChar *)sCurFreeSlotHeader);
        sRecOID = SM_MAKE_OID( sPageID,
                               SMP_SLOT_GET_OFFSET( sCurFreeSlotHeader ) );

        sState = 1;
        if( SMP_SLOT_IS_USED(sCurFreeSlotHeader) )
        {
            sCurSlotHeader = (smpSlotHeader*)sCurFreeSlotHeader;

            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL1);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL2);
            ideLog::log(SM_TRC_LOG_LEVEL_MPAGE, SM_TRC_MPAGE_ALLOC_SLOT_FATAL3, (ULong)sPageID, (ULong)sRecOID);

            dumpSlotHeader( sCurSlotHeader );

            IDE_ASSERT(0);
        }

        // End NTA[-2-]
        IDE_TEST(smrLogMgr::writeNTALogRec( NULL, /* idvSQL* */
                                            aTrans,
                                            &sNTA,
                                            aSpaceID,
                                            aOPType,
                                            sRecOID,
                                            aTableOID )
                 != IDE_SUCCESS);


        setAllocatedSlot( aInfinite, *aRow );

        /* BUG-43291: mVarOID�� �׻� �ʱ�ȭ �Ǿ�� �Ѵ�. */
        sCurSlotHeader = (smpSlotHeader*)*aRow;
        sCurSlotHeader->mVarOID = SM_NULL_OID;

        sState = 0;
        IDE_TEST(smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID)
                 != IDE_SUCCESS);

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

        /* smpFixedPageList::setFreeSlot���� SCN�� Delete Bit��
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

    IDE_PUSH();

    switch(sState)
    {
        case 1:
            IDE_ASSERT(smmDirtyPageMgr::insDirtyPage(aSpaceID, sPageID)
                       == IDE_SUCCESS);
            break;

        default:
            break;
    }

    IDE_POP();


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
IDE_RC smpFixedPageList::freeSlot( void*             aTrans,
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

IDE_RC smpFixedPageList::initForScan( scSpaceID         aSpaceID,
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
        IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID, 
                                                   sPageID,
                                                   (void**)aPage )
                       == IDE_SUCCESS,
                       "TableOID : %"ID_UINT64_FMT"\n"
                       "SpaceID  : %"ID_UINT32_FMT"\n"
                       "PageID   : %"ID_UINT32_FMT"\n",
                       aFixedEntry->mTableOID,
                       aSpaceID,
                       sPageID );
        *aRowPtr = aRow + aFixedEntry->mSlotSize;
    }
    else
    {
        sPageID = smpManager::getFirstAllocPageID(aFixedEntry);

        if(sPageID != SM_NULL_PID)
        {
            IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID, 
                                                       sPageID,
                                                       (void**)aPage )
                           == IDE_SUCCESS,
                           "TableOID : %"ID_UINT64_FMT"\n"
                           "SpaceID  : %"ID_UINT32_FMT"\n"
                           "PageID   : %"ID_UINT32_FMT"\n",
                           aFixedEntry->mTableOID,
                           aSpaceID,
                           sPageID );
            *aRowPtr = (SChar *)((*aPage)->mBody);
        }
        else
        {
            /* Allcate�� �������� �������� �ʴ´�.*/
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PageList���� ��� Row�� ��ȸ�ϸ鼭 ��ȿ�� Row�� ã���ش�.
 *
 * aSpaceID      [IN]  Tablespace ID
 * aFixedEntry   [IN]  ��ȸ�Ϸ��� PageListEntry
 * aCurRow       [IN]  ã������Ϸ��� Row
 * aNxtRow       [OUT] ���� ��ȿ�� Row�� ã�Ƽ� ��ȯ
 * aNxtPID       [OUT] ���� Page�� ��ȯ��.
 **********************************************************************/
IDE_RC smpFixedPageList::nextOIDallForRefineDB( scSpaceID           aSpaceID,
                                                smpPageListEntry  * aFixedEntry,
                                                SChar             * aCurRow,
                                                SChar            ** aNxtRow,
                                                scPageID          * aNxtPID )
{
    scPageID            sNxtPID;
    smpPersPage       * sPage;
    smpFreePageHeader * sFreePageHeader;
    SChar             * sNxtPtr;
    SChar             * sFence;

    IDE_ERROR( aFixedEntry != NULL );
    IDE_ERROR( aNxtRow != NULL );

    IDE_TEST( initForScan( aSpaceID, aFixedEntry, aCurRow, &sPage, &sNxtPtr )
              != IDE_SUCCESS );

    *aNxtRow = NULL;

    while(sPage != NULL)
    {
        // �ش� Page�� ���� ������ ���
        sFence          = (SChar *)((smpPersPage *)sPage + 1);
        sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID,sPage);

        for( ;
             sNxtPtr + aFixedEntry->mSlotSize <= sFence;
             sNxtPtr += aFixedEntry->mSlotSize )
        {
            // In case of free slot
            if( SMP_SLOT_IS_NOT_USED((smpSlotHeader*)sNxtPtr) )
            {
                // refineDB�� FreeSlot�� FreeSlotList�� ����Ѵ�.
                addFreeSlotToFreeSlotList(sFreePageHeader,
                                          sNxtPtr);

                continue;
            }

            *aNxtRow = sNxtPtr;
            *aNxtPID = sPage->mHeader.mSelfPageID;

            IDE_CONT(normal_case);
        } /* for */

        // refineDB�� �ϳ��� Page Scan�� ��ġ�� FreePageList�� ����Ѵ�.
        if( sFreePageHeader->mFreeSlotCount > 0 )
        {
            // FreeSlot�� �־�� FreePage�̰� FreePageList�� ��ϵȴ�.
            IDE_TEST( smpFreePageList::addPageToFreePageListAtInit(
                          aFixedEntry,
                          smpFreePageList::getFreePageHeader(aSpaceID, sPage))
                      != IDE_SUCCESS );
        }

        sNxtPID = smpManager::getNextAllocPageID( aSpaceID,
                                                  aFixedEntry,
                                                  sPage->mHeader.mSelfPageID );

        if(sNxtPID == SM_NULL_PID)
        {
            // NextPage�� NULL�̸� ���̴�.
            IDE_CONT(normal_case);
        }

        IDE_ERROR_MSG( smmManager::getPersPagePtr( aSpaceID, 
                                                   sNxtPID,
                                                   (void**)&sPage )
                       == IDE_SUCCESS,
                       "TableOID : %"ID_UINT64_FMT"\n"
                       "SpaceID  : %"ID_UINT32_FMT"\n"
                       "PageID   : %"ID_UINT32_FMT"\n",
                       aFixedEntry->mTableOID,
                       aSpaceID,
                       sNxtPID  );

        sNxtPtr  = (SChar *)sPage->mBody;
    }

    IDE_EXCEPTION_CONT( normal_case );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**********************************************************************
 * FreePageHeader���� FreeSlot����
 *
 * aFreePageHeader : �����Ϸ��� FreePageHeader
 * aRow            : ������ FreeSlot�� Row������ ��ȯ
 **********************************************************************/

void smpFixedPageList::removeSlotFromFreeSlotList(
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

IDE_RC smpFixedPageList::setFreeSlot( void         * aTrans,
                                      scSpaceID      aSpaceID,
                                      scPageID       aPageID,
                                      SChar        * aRow,
                                      smpTableType   aTableType )
{
    smpSlotHeader       sAfterSlotHeader;
    SInt                sState = 0;
    smpSlotHeader     * sCurSlotHeader;
    smpSlotHeader     * sTmpSlotHeader;
    smOID               sRecOID;

    IDE_DASSERT( ((aTrans != NULL) && (aTableType == SMP_TABLE_NORMAL)) ||
                 ((aTrans == NULL) && (aTableType == SMP_TABLE_TEMP)) );

    IDE_ERROR( aPageID != SM_NULL_PID );
    IDE_ERROR( aRow != NULL );

    ACP_UNUSED( aTrans  );

    sCurSlotHeader = (smpSlotHeader*)aRow;

    if( SMP_SLOT_IS_USED(sCurSlotHeader) )
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
        sAfterSlotHeader.mVarOID   = SM_NULL_OID;

        if(aTableType == SMP_TABLE_NORMAL)
        {

            IDE_ERROR( smmManager::getOIDPtr( aSpaceID, 
                                               sRecOID,
                                               (void**)&sTmpSlotHeader )
                       == IDE_SUCCESS );

            IDE_ERROR( SMP_SLOT_GET_OFFSET( sTmpSlotHeader )
                       == SMP_SLOT_GET_OFFSET( sCurSlotHeader ) );

            IDE_ERROR( SMP_SLOT_GET_OFFSET( sTmpSlotHeader )
                       == SM_MAKE_OFFSET(sRecOID));
        }

        // BUG-14373 ager�� seq-iterator���� ���ü� ����
        IDE_TEST(smmManager::holdPageXLatch(aSpaceID, aPageID) != IDE_SUCCESS);
        sState = 2;

        *sCurSlotHeader = sAfterSlotHeader;

        sState = 1;
        IDE_TEST(smmManager::releasePageLatch(aSpaceID, aPageID) != IDE_SUCCESS);

        sState = 0;
        IDE_TEST( smmDirtyPageMgr::insDirtyPage( aSpaceID, aPageID)
                  != IDE_SUCCESS );

    }

    IDE_ERROR( SM_SCN_IS_DELETED( sCurSlotHeader->mCreateSCN ) &&
               SM_SCN_IS_FREE_ROW( sCurSlotHeader->mLimitSCN ) &&
               SMP_SLOT_HAS_NULL_NEXT_OID( sCurSlotHeader ) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch(sState)
    {
        case 2:
            IDE_ASSERT( smmManager::releasePageLatch(aSpaceID,aPageID)
                        == IDE_SUCCESS );

        case 1:
            IDE_ASSERT( smmDirtyPageMgr::insDirtyPage(aSpaceID,aPageID)
                        == IDE_SUCCESS );

        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/**********************************************************************
 * BUG-25611
 * Undo�� ����� PrivatePageList���� ���� ����� RefineDB�� ����
 * ScanList�� �����ؾ� �Ѵ�.
 * ���� ���� restart�� Undo�Ҷ����� �Ҹ�����Ѵ�.
 *
 * aSpaceID        : ��� ���̺��� ���̺� �����̽� ID
 * aFixedEntry      : ��� ���̺��� FixedEntry
 * aPageListEntry   : ��ĵ����Ʈ�� ������ FreePage���� Head
 **********************************************************************/
IDE_RC smpFixedPageList::resetScanList( scSpaceID          aSpaceID,
                                        smpPageListEntry  *aPageListEntry)
{
    scPageID sPageID;

    IDE_DASSERT( aPageListEntry != NULL );

    sPageID = smpManager::getFirstAllocPageID(aPageListEntry);

    while(sPageID != SM_NULL_PID)
    {
        IDE_TEST( unlinkScanList( aSpaceID,
                                  sPageID,
                                  aPageListEntry )
                  != IDE_SUCCESS );

        sPageID = smpManager::getNextAllocPageID( aSpaceID,
                                                  aPageListEntry,
                                                  sPageID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

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
IDE_RC smpFixedPageList::addFreeSlotPending( void*             aTrans,
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
    sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID, sPageID);

    IDE_TEST(sFreePageHeader->mMutex.lock( NULL /* idvSQL* */) != IDE_SUCCESS);
    sState = 1;

    // PrivatePageList������ FreeSlot���� �ʴ´�.
    IDE_ASSERT(sFreePageHeader->mFreeListID != SMP_PRIVATE_PAGELISTID);

    // FreeSlot�� FreeSlotList�� �߰�
    addFreeSlotToFreeSlotList(sFreePageHeader, aRow);

    // FreeSlot�� �߰��� ���� SizeClass�� ����Ǿ����� Ȯ���Ͽ� �����Ѵ�.
    IDE_TEST(smpFreePageList::modifyPageSizeClass( aTrans,
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
void smpFixedPageList::addFreeSlotToFreeSlotList(
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
ULong smpFixedPageList::getRecordCount( smpPageListEntry* aFixedEntry )
{
    ULong sRecordCount;

    /* PROJ-2334 PMT */
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
 * PageList�� ��ȿ�� ���ڵ� ������ ����.
 *
 * aFixedEntry  : �˻��ϰ��� �ϴ� PageListEntry
 * aRecordCount : ��ȯ�ϴ� ���ڵ� ����
 **********************************************************************/
IDE_RC smpFixedPageList::setRecordCount( smpPageListEntry* aFixedEntry,
                                         ULong             aRecordCount )
{
    // BUG-47368: setRecordCount, mutex ����, atomic �������� ����
    (void)idCore::acpAtomicSet64( &(aFixedEntry->mRuntimeEntry->mInsRecCnt), aRecordCount);

    return IDE_SUCCESS;
}

/**********************************************************************
 * PageList�� FreeSlotList,FreePageList,FreePagePool�� �籸���Ѵ�.
 *
 * FreeSlot/FreePage ���� ������ Disk�� ����Ǵ� Durable�� ������ �ƴϱ⶧����
 * ������ restart�Ǹ� �籸�����־�� �Ѵ�.
 *
 * aTrans      : �۾��� �����ϴ� Ʈ����� ��ü
 * aFixedEntry : �����Ϸ��� PageListEntry
 **********************************************************************/

IDE_RC smpFixedPageList::refinePageList( void*             aTrans,
                                         scSpaceID         aSpaceID,
                                         UInt              aTableType,
                                         smpPageListEntry* aFixedEntry )
{
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    // Slot�� Refine�ϰ� �� Page���� FreeSlotList�� �����ϰ�
    // �� �������� FreePage�̸� �켱 FreePageList[0]�� ����Ѵ�.
    IDE_TEST( buildFreeSlotList( aTrans,
                                 aSpaceID,
                                 aTableType,
                                 aFixedEntry )
              != IDE_SUCCESS );

    // FreePageList[0]���� N���� FreePageList�� FreePage���� �����ְ�
    smpFreePageList::distributePagesFromFreePageList0ToTheOthers(aFixedEntry);

    // EmptyPage(������������ʴ� FreePage)�� �ʿ��̻��̸�
    // FreePagePool�� �ݳ��ϰ� FreePagePool���� �ʿ��̻��̸�
    // DB�� �ݳ��Ѵ�.
    IDE_TEST(smpFreePageList::distributePagesFromFreePageList0ToFreePagePool(
                 aTrans,
                 aSpaceID,
                 aFixedEntry )
             != IDE_SUCCESS);


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * FreeSlotList ����
 *
 * aTrans      : �۾��� �����ϴ� Ʈ����� ��ü
 * aTableType  : Table Type
 * aFixedEntry : �����Ϸ��� PageListEntry
 * aPtrList    : Index Rebuild�� ���� ��ȿ�� ���ڵ���� ����Ʈ�� ����� ��ȯ
 **********************************************************************/
IDE_RC smpFixedPageList::buildFreeSlotList( void*             aTrans,
                                            scSpaceID         aSpaceID,
                                            UInt              aTableType,
                                            smpPageListEntry* aFixedEntry )
{
    SChar      * sCurPtr;
    SChar      * sNxtPtr;
    smiColumn ** sArrLobColumn;
    UInt         sLobColumnCnt;
    UInt         sState = 0;
    void       * sTable;
    scPageID     sCurPageID = SM_NULL_PID;
    scPageID     sPrvPageID = SM_NULL_PID;
    idBool       sRefined;

    IDE_TEST( smLayerCallback::getTableHeaderFromOID( 
                    aFixedEntry->mTableOID,
                    (void**)&sTable )
              != IDE_SUCCESS );

    IDE_TEST( smLayerCallback::makeLobColumnList(
                  sTable,
                  &sArrLobColumn,
                  &sLobColumnCnt )
              != IDE_SUCCESS );
    sState = 1;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aFixedEntry != NULL );

    sCurPtr = NULL;

    while(1)
    {
        // FreeSlot�� �����ϰ�
        IDE_TEST( nextOIDallForRefineDB( aSpaceID,
                                         aFixedEntry,
                                         sCurPtr,
                                         &sNxtPtr,
                                         &sCurPageID )
                  != IDE_SUCCESS );

        if(sNxtPtr == NULL)
        {
            break;
        }

        // sNxtPtr�� refine�Ѵ�.
        IDE_TEST( refineSlot( aTrans,
                              aSpaceID,
                              aTableType,
                              sArrLobColumn,
                              sLobColumnCnt,
                              aFixedEntry,
                              sNxtPtr,
                              &sRefined )
                  != IDE_SUCCESS );

        /*
         * BUG-25179 [SMM] Full Scan�� ���� �������� Scan List�� �ʿ��մϴ�.
         * refine���� ���� ���ڵ�(��ȿ�� ���ڵ�)��� Scan List�� �߰��Ѵ�.
         */
        if( (sRefined == ID_FALSE) && (sCurPageID != sPrvPageID) )
        {
            IDE_TEST( smpFixedPageList::linkScanList( aSpaceID,
                                                      sCurPageID,
                                                      aFixedEntry )
                      != IDE_SUCCESS );
            sPrvPageID = sCurPageID;
        }

        sCurPtr = sNxtPtr;
    }

    sState = 0;
    IDE_TEST( smLayerCallback::destLobColumnList( sArrLobColumn )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( smLayerCallback::destLobColumnList( sArrLobColumn )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/**********************************************************************
 * Slot�� �������� ���� FreeSlot���� Ȯ��
 *
 * aTrans            : �۾��� �����ϴ� Ʈ����� ��ü
 * aTableType        : Table Type
 * aArrLobColumn     : Lob Column List
 * aLobColumnCnt     : Lob Column Count
 * aFixedEntry       : Ȯ���Ϸ��� Slot�� �Ҽ� PageListEntry
 * aCurRow           : Ȯ���Ϸ��� Slot
 **********************************************************************/
IDE_RC smpFixedPageList::refineSlot( void*             aTrans,
                                     scSpaceID         aSpaceID,
                                     UInt              aTableType,
                                     smiColumn**       aArrLobColumn,
                                     UInt              aLobColumnCnt,
                                     smpPageListEntry* aFixedEntry,
                                     SChar*            aCurRow,
                                     idBool          * aRefined )
{
    smpSlotHeader     * sCurRowHeader;
    smSCN               sSCN;
    smOID               sFixOid;
    scPageID            sPageID;
    idBool              sIsNeedFreeSlot;
    smpFreePageHeader * sFreePageHeader;
    smcTableHeader    * sTable;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aCurRow != NULL );

    sPageID = SMP_SLOT_GET_PID(aCurRow);
    sCurRowHeader = (smpSlotHeader*)aCurRow;

    sSCN = sCurRowHeader->mCreateSCN;

    sFreePageHeader = smpFreePageList::getFreePageHeader(aSpaceID, sPageID);
    *aRefined = ID_FALSE;

    IDE_ASSERT( smLayerCallback::getTableHeaderFromOID( 
                    aFixedEntry->mTableOID,
                    (void**)&sTable )
                == IDE_SUCCESS );

    /* NULL Row�� ��� */
    IDE_TEST_CONT( SM_SCN_IS_NULL_ROW(sSCN) ,
                    cont_refine_slot_end );

    /* BUG-32544 [sm-mem-collection] The MMDB alter table operation can
     * generate invalid null row. */
    sFixOid = SM_MAKE_OID(sPageID, SMP_SLOT_GET_OFFSET( sCurRowHeader ) );

    if( sTable->mNullOID == sFixOid )
    {
        ideLog::log( IDE_SERVER_0, 
                     "InvalidNullRow : %u, %u - %llu",
                     aSpaceID,
                     sPageID,
                     aFixedEntry->mTableOID );
        /* ���� SM_SCN_IS_NULL_ROW�� �ɷ������ �ϴµ�
         * �ɷ����� �ʾҴٴ� ���� ������ �ִٴ� �� */
        IDE_DASSERT( 0 );

        /* ������ ���⼭ ������ ���������� ��Ű��,
         * ���� ���� ��ü�� �����ϴ� Release��忡����
         * ������ ũ��. ���� FreeSlot������ ��ϸ� ��
         * �Ƽ� ���� ������ �����Ѵ�. */
        IDE_CONT( cont_refine_slot_end );
    }

    /* TASK-4690
     * update -> recordLockValidate �� checkpoint �߻��Ͽ� lock �ɸ� ���·�
     * �̹����� ������ �� �ִ�. (�α� ����) */
    if( SMP_SLOT_IS_LOCK_TRUE( sCurRowHeader ) )
    {
        SMP_SLOT_SET_UNLOCK( sCurRowHeader );
    }

    /* Slot�� Free�� �ʿ䰡 �ִ��� Ȯ���Ͽ� Free�Ѵ�. */
    IDE_TEST( isNeedFreeSlot( sCurRowHeader,
                              aSpaceID,
                              aFixedEntry,
                              &sIsNeedFreeSlot ) != IDE_SUCCESS );

    if( sIsNeedFreeSlot == ID_TRUE )
    {
        IDE_TEST( setFreeSlot( aTrans,
                               aSpaceID,
                               sPageID,
                               aCurRow,
                               SMP_TABLE_NORMAL )
                  != IDE_SUCCESS );

        addFreeSlotToFreeSlotList( sFreePageHeader,
                                   aCurRow );
        *aRefined = ID_TRUE;

        IDE_CONT( cont_refine_slot_end );
    }

    if(SM_SCN_IS_NOT_INFINITE( sSCN ) &&
       SM_SCN_IS_GT( &sSCN, &(smmDatabase::getDicMemBase()->mSystemSCN) ))
    {
        ideLog::log(SM_TRC_LOG_LEVEL_MPAGE,
                    SM_TRC_MPAGE_INVALID_SCN,
                    sFixOid,
                    SM_SCN_TO_LONG( sSCN ),
                    SM_SCN_TO_LONG( *(smmDatabase::getSystemSCN()) ) );

        /* BUG-32144 [sm-mem-recovery] If the record scn is larger than system
         * scn, Emergency-startup fails.
         * ��� �����ÿ��� SCN Check�� �ϸ� �ȵ�. �� Emergency Property�� 0��
         * ���� ������ �����Ŵ*/
        IDE_TEST_RAISE( smuProperty::getEmergencyStartupPolicy() == SMR_RECOVERY_NORMAL,
                        err_invalide_scn );
    }

    // LPCH�� rebuild�Ѵ�.
    if(aTableType != SMI_TABLE_META)
    {

        IDE_TEST( smLayerCallback::rebuildLPCH(
                      aFixedEntry->mTableOID,
                      aArrLobColumn,
                      aLobColumnCnt,
                      aCurRow )
                  != IDE_SUCCESS );
    }

    /*
     * [BUG-26415] XA Ʈ������� Partial Rollback(Unique Volation)�� Prepare
     *             Ʈ������� �����ϴ� ��� ���� �籸���� �����մϴ�.
     * : Prepare Transaction�� ���� Ŀ���� �ȵ� �����̱� ������ RuntimeEntry
     *   �� InsRecCnt�� �÷����� �ȵȴ�.
     */
    if( SM_SCN_IS_INFINITE( sSCN ) )
    {
       if ( ( smLayerCallback::incRecCnt4InDoubtTrans( SMP_GET_TID( sSCN ),
                                                       aFixedEntry->mTableOID ) )
              != IDE_SUCCESS )
        {
            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)aCurRow,
                            aFixedEntry->mSlotSize,
                            "Currupt CreateSCN\n"
                            "[ FixedSlot ]\n"
                            "TableOID :%"ID_UINT64_FMT"\n"
                            "SpaceID  :%"ID_UINT32_FMT"\n"
                            "PageID   :%"ID_UINT32_FMT"\n"
                            "Offset   :%"ID_UINT32_FMT"\n",
                            aFixedEntry->mTableOID,
                            aSpaceID,
                            sPageID,
                            (UInt)SMP_SLOT_GET_OFFSET( sCurRowHeader ) ); 

            /* BUG-38515 Prepare Tx�� �ƴ� ���¿��� SCN�� ���� ��쿡��
             * __SM_SKIP_CHECKSCN_IN_STARTUP ������Ƽ�� ���� �ִٸ�
             * ������ ������ �ʰ� ���� ������ ����ϰ� �״�� �����Ѵ�.
             * BUG-41600 SkipCheckSCNInStartup ������Ƽ�� ���� 2�� ��츦 �߰��Ѵ�.�̿� ����
             * �Լ��� ���ϰ� �����Ѵ�. */
            IDE_ASSERT( smuProperty::isSkipCheckSCNInStartup() != 0 );
        }
        else
        {
            /* do nothing... */
        }
    }
    else
    {
        (aFixedEntry->mRuntimeEntry->mInsRecCnt)++;
    }

    IDE_EXCEPTION_CONT( cont_refine_slot_end );


    return IDE_SUCCESS;

    IDE_EXCEPTION(err_invalide_scn);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_INVALID_ROW_SCN));
    }
    IDE_EXCEPTION_END;

    ideLog::logMem( IDE_DUMP_0,
                    (UChar*)aCurRow,
                    aFixedEntry->mSlotSize,
                    "[ FixedSlot ]\n"
                    "TableOID :%llu\n"
                    "SpaceID  :%u\n"
                    "PageID   :%u\n"
                    "Offset   :%u\n",
                    aFixedEntry->mTableOID,
                    aSpaceID,
                    sPageID,
                    (UInt)SMP_SLOT_GET_OFFSET( sCurRowHeader ) );

    return IDE_FAILURE;
}

/**********************************************************************
 * ���� : Slot�� Free �� �ʿ䰡 �ִ��� Ȯ���Ѵ�.
 *        BUG-31521 Prepare Transaction�� ���� ���, �����ǰ� Aging����
 *        ���� Memory Row�� ���Ͽ� refine���� ���� �� �� �ֽ��ϴ�.
 *
 * Slot�� Free �Ǵ� ���� ���� 3���� �� �ϳ��̴�.
 *
 * 1. Update Rollback�� ��� New Version�� Free�ȴ�.
 * 2. Update Commit�� ��� Old Version�� Free�ȴ�.
 * 3. Delete Row ������ ���� �� ��� �ش� Slot�� Free�ȴ�.
 *
 * ������ ��� ���� ��߿� Aging������, ���� Aging���� ���� ���
 * Restart�� Refine���� �߿� �ش� Slot�� Free�ȴ�.
 *
 * Delete Row, Update���� ������ ���� �Ǿ��ٰ� �ص�,
 * XA Prepare Transaction�� ������ ��� �������� �ʴ´�.
 * �� ����
 *  1. Update - Slot Header�� Next�� ����Ű�� New Version�� SCN��
 *              ���Ѵ��� ���� ������.
 *  2. Delete - Slot Header�� Next�� ���Ѵ��� ���� ������.
 *
 * ----------------------------------------------------------------
 * recovery ���� prepared transaction�� ���� ���ٵǰ� ���� ����
 * ��� row���� SCN�� ���Ѵ밡 �ƴϴ�.
 * �׷��� prepared transaction�� �����ϴ� ���
 * �� Ʈ������� access�� ���ڵ忡 ���Ͽ� record lock�� ȹ���ϹǷ�
 * row�� ���´� ���Ѵ�.
 * ---------------------------------------------------------------
 *
 * aCurRowHeader   - [IN] Free Slot���θ� �Ǵ��� Slot�� Header
 * SpaceID         - [IN] �ش� Slot�� ���Ե� TableSpace�� ID
 * aFixedEntry     - [IN] �ش� Slot�� ���Ե� PageListEntry
 * aIsNeedFreeSlot - [OUT] Free�� �ʿ��� Slot������ ���θ� ��ȯ
 *
 **********************************************************************/
IDE_RC smpFixedPageList::isNeedFreeSlot( smpSlotHeader    * aCurRowHeader,
                                         scSpaceID          aSpaceID,
                                         smpPageListEntry * aFixedEntry,
                                         idBool           * aIsNeedFreeSlot )
{
    smSCN           sSCN;
    smTID           sDummyTID;
    smSCN           sNextSCN;
    smTID           sNextTID;
    ULong           sNextOID;
    idBool          sIsNeedFreeSlot = ID_FALSE;
    smpSlotHeader * sSlotHeaderPtr  = NULL;

    sNextOID = SMP_SLOT_GET_NEXT_OID( aCurRowHeader );

    SMX_GET_SCN_AND_TID( aCurRowHeader->mCreateSCN, sSCN,     sDummyTID );
    SMX_GET_SCN_AND_TID( aCurRowHeader->mLimitSCN,  sNextSCN, sNextTID );

    if( SMP_SLOT_IS_SKIP_REFINE( aCurRowHeader ) )
    {
        /* Refine�� Free�Ǹ� �ȵǴ� �������� Slot�� ��� �� Flag�� ��ϵǾ� �ִ�.
         * Refine�� ������ ���Ǵ� Flag�� Ȯ���� �ٷ� �����Ѵ�.*/
        SMP_SLOT_UNSET_SKIP_REFINE( aCurRowHeader );

        /* ���ܴ� ����δ� XA Prepare Transaction�� Update�� Slot��
         * Old Version�� ��� �ϳ����̴�. */
        /* �� ��� Header Next�� New Version�� ����Ű�� OID�̸�
         * ���� Commit���� �ʾ����Ƿ� New Version�� SCN�� ���Ѵ��̴�.*/
        IDE_ERROR( SM_IS_OID( sNextOID ) );
        /* BUG-42724 limitSCN�� free�� ��� �߰�( XA + partial rollback�� �߻��� ���) */
        IDE_ERROR( SM_SCN_IS_INFINITE( sNextSCN ) || SM_SCN_IS_FREE_ROW( sNextSCN) );
        IDE_ERROR( SM_SCN_IS_NOT_LOCK_ROW( sNextSCN ) );
        
        IDE_CONT( cont_is_need_free_slot_end );
    }

    // SCN�� Delete bit�� ��ϵ� ���� Rollback�� ����̴�.
    // ���̻� New Version�� �ʿ� �����Ƿ�, �ٷ� �����Ѵ�.
    if( SM_SCN_IS_DELETED( sSCN ) )
    {
        sIsNeedFreeSlot = ID_TRUE;
        IDE_CONT( cont_is_need_free_slot_end );
    }

    /* BUG-41600 : refine �ܰ迡�� invalid�� slot�� �����ϴ� �޸� ���̺��� �����Ѵ�. */
    if( smuProperty::isSkipCheckSCNInStartup() == 2 )
    {
        if( SM_SCN_IS_INFINITE( sSCN ) )
        {
            if( SM_SCN_IS_FREE_ROW( sNextSCN ) )
            {
                /* Prepare Tx�� �ƴ� ���¿��� SCN�� ���� ��� */
                if ( smLayerCallback::getPreparedTransCnt() == 0 )
                {
                    ideLog::logMem( IDE_DUMP_0,
                                    (UChar*)aCurRowHeader,
                                    aFixedEntry->mSlotSize,
                                    "Currupt CreateSCN\n"
                                    "[ FixedSlot ]\n"
                                    "TableOID :%"ID_UINT64_FMT"\n"
                                    "SpaceID  :%"ID_UINT32_FMT"\n"
                                    "PageID   :%"ID_UINT32_FMT"\n"
                                    "Offset   :%"ID_UINT32_FMT"\n",
                                    aFixedEntry->mTableOID,
                                    aSpaceID,
                                    (UInt)SMP_SLOT_GET_PID( aCurRowHeader ),
                                    (UInt)SMP_SLOT_GET_OFFSET( aCurRowHeader ) );

                    sIsNeedFreeSlot = ID_TRUE;

                    IDE_CONT( cont_is_need_free_slot_end );
                }
            }
        }
    }

    /* Header Next�� Ȯ���غ���.
     * Header Next���� ���� �� �� �ϳ��� �� �� �ִ�.
     *
     * 0. 0 - Free Slot����� �ƴѰ��
     * 1. OID - Update���� Old Version�� ���
     *          Header Next���� New Verstion�� ����Ű�� OID�� �´�.
     * 2. CommitSCN + Delete bit - Slot�� Delete�� ���
     * 3. Infinite SCN + Delete bit - XA Prepare Trans�� ���� Delete �� ��� */

    /* 0. Free Slot����� �ƴ� ��� */
    IDE_TEST_CONT( SM_SCN_IS_FREE_ROW( sNextSCN ),
                    cont_is_need_free_slot_end );

    if( SM_SCN_IS_INFINITE( sNextSCN ) )
    {
        if( SM_SCN_IS_DELETED( sNextSCN ) )
        {
            /* DELETE BIT�� �����Ǿ� �ְ�, Next SCN�� ���Ѵ��� ����
             * IN-DOUBT Ʈ������� DELETE�� ������ ���ڵ��̴�.
             * BUG-31521 Next�� ���Ѵ��� SCN�� ��쿡�� ȣ�� �Ͽ��� �մϴ�. */

            /* BUG-42724 : sTID -> sNextTID�� ����. next version�� ���� TID�� ��뿩�� �Ѵ�. */
            if ( ( smLayerCallback::decRecCnt4InDoubtTrans( sNextTID, aFixedEntry->mTableOID ) )
                != IDE_SUCCESS )
            {    
                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)aCurRowHeader,
                                aFixedEntry->mSlotSize,
                                "Currupt LimitSCN"
                                "[ FixedSlot ]\n"
                                "TableOID :%"ID_UINT64_FMT"\n"
                                "SpaceID  :%"ID_UINT32_FMT"\n"
                                "PageID   :%"ID_UINT32_FMT"\n"
                                "Offset   :%"ID_UINT32_FMT"\n",
                                aFixedEntry->mTableOID,
                                aSpaceID,
                                (UInt)SMP_SLOT_GET_PID( aCurRowHeader ),
                                (UInt)SMP_SLOT_GET_OFFSET( aCurRowHeader ) ); 

                /* BUG-38515 Prepare Tx�� �ƴ� ���¿��� SCN�� ���� ��쿡��
                 * __SM_SKIP_CHECKSCN_IN_STARTUP ������Ƽ�� ���� �ִٸ�
                 * ������ ������ �ʰ� ���� ������ ����ϰ� �״�� �����Ѵ�.
                 * BUG-41600 SkipCheckSCNInStartup ������Ƽ�� ���� 2�� ��츦 �߰��Ѵ�.�̿� ����
                 * �Լ��� ���ϰ��� �����Ѵ�. */
                IDE_ASSERT( smuProperty::isSkipCheckSCNInStartup() != 0 ); 
            }    
            else 
            {    
                /* do nothing ... */
            } 
        }

        /* BUG-39233
         * next�� �����Ǿ� �ְ�, next�� createSCN �� infinite�� �ƴϸ�,
         * ���� ������ �����Ǿ�� �Ѵ�. */
        if( sNextOID != SMI_NULL_OID )
        {
            IDE_ASSERT( smmManager::getOIDPtr( aSpaceID,
                                               sNextOID,
                                               (void**)&sSlotHeaderPtr )
                        == IDE_SUCCESS );

            if( SM_SCN_IS_NOT_INFINITE( sSlotHeaderPtr->mCreateSCN ) )
            {
                // trc
                ideLog::logMem( IDE_DUMP_0,
                                (UChar*)aCurRowHeader,
                                aFixedEntry->mSlotSize,
                                "Invalid Currupt LimitSCN\n"
                                "[ FixedSlot ]\n"
                                "TableOID :%"ID_UINT64_FMT"\n"
                                "SpaceID  :%"ID_UINT32_FMT"\n"
                                "PageID   :%"ID_UINT32_FMT"\n"
                                "Offset   :%"ID_UINT32_FMT"\n",
                                aFixedEntry->mTableOID,
                                aSpaceID,
                                (UInt)SMP_SLOT_GET_PID( aCurRowHeader ),
                                (UInt)SMP_SLOT_GET_OFFSET( aCurRowHeader ) ); 

                /* debug ��忡���� assert ��Ű��,
                 * release ��忡���� freeslot �Ѵ�. */
                IDE_DASSERT( 0 );

                sIsNeedFreeSlot = ID_TRUE;
            }
            else
            {
                /* do nothing */
            }
        }
        else
        {
            /* do nothing */
        }
    }
    else
    {
        sIsNeedFreeSlot = ID_TRUE;
    }

    IDE_EXCEPTION_CONT( cont_is_need_free_slot_end );

    *aIsNeedFreeSlot = sIsNeedFreeSlot;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * PageListEntry�� �ʱ�ȭ�Ѵ�.
 *
 * aFixedEntry : �ʱ�ȭ�Ϸ��� PageListEntry
 * aTableOID   : PageListEntry�� ���̺� OID
 * aSlotSize   : PageListEntry�� SlotSize
 **********************************************************************/
void smpFixedPageList::initializePageListEntry( smpPageListEntry* aFixedEntry,
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
void smpFixedPageList::setAllocatedSlot( smSCN  aInfinite,
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
smpFixedPageList::isValidFreeSlotList(smpFreePageHeader* aFreePageHeader )
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
 *  BUG-31206 improve usability of DUMPCI and DUMPDDF
 *            Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 *  Slot Header�� �����Ѵ�
 *
 *  aSlotHeader : dump�� slot ���
 *  aOutBuf     : ��� ����
 *  aOutSize    : ��� ������ ũ��
 **********************************************************************/
IDE_RC smpFixedPageList::dumpSlotHeaderByBuffer(
                                        smpSlotHeader  * aSlotHeader,
                                        idBool           aDisplayTable,
                                        SChar          * aOutBuf,
                                        UInt             aOutSize )
{
    smSCN    sCreateSCN;
    smSCN    sLimitSCN;
    smSCN    sRowSCN;
    smTID    sRowTID;
    smSCN    sNxtSCN;
    smTID    sNxtTID;
    scPageID sPID;

    IDE_ERROR( aSlotHeader != NULL );
    IDE_ERROR( aOutBuf     != NULL );
    IDE_ERROR( aOutSize     > 0 );

    sPID = SMP_SLOT_GET_PID( aSlotHeader );
    SM_GET_SCN( &sCreateSCN, &(aSlotHeader->mCreateSCN) );
    SM_GET_SCN( &sLimitSCN,  &(aSlotHeader->mLimitSCN) );

    SMX_GET_SCN_AND_TID( sCreateSCN, sRowSCN, sRowTID );
    SMX_GET_SCN_AND_TID( sLimitSCN,  sNxtSCN, sNxtTID );

    if( aDisplayTable == ID_FALSE )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "Slot PID      : %"ID_UINT32_FMT"\n"
                             "Slot CreateSCN : 0x%"ID_XINT64_FMT"\n"
                             "Slot CreateTID :   %"ID_UINT32_FMT"\n"
                             "Slot LimitSCN  : 0x%"ID_XINT64_FMT"\n"
                             "Slot LimitTID  :   %"ID_UINT32_FMT"\n"
                             "Slot NextOID   : 0x%"ID_XINT64_FMT"\n"
                             "Slot Offset    : %"ID_UINT64_FMT"\n"
                             "Slot Flag      : %"ID_XINT64_FMT"\n",
                             sPID,
                             SM_SCN_TO_LONG( sRowSCN ),
                             sRowTID,
                             SM_SCN_TO_LONG( sNxtSCN ),
                             sNxtTID, 
                             SMP_SLOT_GET_NEXT_OID( aSlotHeader ),
                             (ULong)SMP_SLOT_GET_OFFSET( aSlotHeader ),
                             (ULong)SMP_SLOT_GET_FLAGS( aSlotHeader ) );
    }
    else
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "%7"ID_UINT32_FMT
                             "     0x%-14"ID_XINT64_FMT
                             "%-14"ID_UINT32_FMT
                             "0x%-11"ID_XINT64_FMT
                             "  %-15"ID_UINT32_FMT
                             "0x%-7"ID_XINT64_FMT
                             "%-12"ID_UINT64_FMT
                             "%-4"ID_XINT64_FMT,
                             sPID,
                             SM_SCN_TO_LONG( sRowSCN ),
                             sRowTID,
                             SM_SCN_TO_LONG( sNxtSCN ),
                             sNxtTID ,
                             SMP_SLOT_GET_NEXT_OID( aSlotHeader ),
                             (ULong)SMP_SLOT_GET_OFFSET( aSlotHeader ),
                             (ULong)SMP_SLOT_GET_FLAGS( aSlotHeader ) );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/**********************************************************************
 *  BUG-31206 improve usability of DUMPCI and DUMPDDF
 *            Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 *  dumpSlotHeaderByBuffer �Լ��� �̿��� TRC boot log�� ������ ����Ѵ�.
 *
 *  aSlotHeader : dump�� slot ���
 **********************************************************************/
IDE_RC smpFixedPageList::dumpSlotHeader( smpSlotHeader     * aSlotHeader )
{
    SChar          * sTempBuffer;

    IDE_ERROR( aSlotHeader != NULL );

    if( iduMemMgr::calloc( IDU_MEM_SM_SMC,
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        dumpSlotHeaderByBuffer( aSlotHeader,
                                ID_FALSE, // display table
                                sTempBuffer,
                                IDE_DUMP_DEST_LIMIT );

        ideLog::log( IDE_ERR_0,
                     "Slot Offset %"ID_xPOINTER_FMT"\n"
                     "%s\n",
                     aSlotHeader,
                     sTempBuffer );

        (void) iduMemMgr::free( sTempBuffer );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * BUG-31206 improve usability of DUMPCI and DUMPDDF
 *           Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 * Description: <aSpaceID, aPageID>�� �ִ� Record Header����
 *              altibase_boot.log�� ��´�.
 *
 * aPagePtr    - [IN]  Dump�� page�� �ּ�
 * aSlotSize   - [IN]  Slit�� ũ��
 * aOutBuf     : [OUT] ��� ����
 * aOutSize    : [OUT] ��� ������ ũ��
 **********************************************************************/
IDE_RC smpFixedPageList::dumpFixedPageByBuffer( UChar            * aPagePtr,
                                                UInt               aSlotSize,
                                                SChar            * aOutBuf,
                                                UInt               aOutSize )
{
    UInt                sSlotCnt;
    UInt                sSlotSize;
    smpSlotHeader     * sCurSlotHeader;
    smpSlotHeader     * sNxtSlotHeader;
    UInt                sCurOffset;
    smpPersPageHeader * sHeader;
    UInt                i;

    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aOutBuf  != NULL );
    IDE_ERROR( aOutSize  > 0 );

    sSlotSize      = aSlotSize;

    sCurOffset     = (UShort)SMP_PERS_PAGE_BODY_OFFSET;
    sHeader        = (smpPersPageHeader*)aPagePtr;
    sNxtSlotHeader = (smpSlotHeader*)(aPagePtr + sCurOffset);

    ideLog::ideMemToHexStr( aPagePtr,
                            SM_PAGE_SIZE,
                            IDE_DUMP_FORMAT_NORMAL,
                            aOutBuf,
                            aOutSize );

    /* Flags[ Skip Refine : 4 | Drop : 2 | Used : 1 ] */
    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "\n= FixedPage =\n"
                         "SelfPID      : %"ID_UINT32_FMT"\n"
                         "PrevPID      : %"ID_UINT32_FMT"\n"
                         "NextPID      : %"ID_UINT32_FMT"\n"
                         "Type         : %"ID_UINT32_FMT"\n"
                         "TableOID     : %"ID_vULONG_FMT"\n"
                         "AllocListID  : %"ID_UINT32_FMT"\n\n"
                         "PID         CreateSCN             CreateTID         "
                         "LimitSCN              LimitTID          NextOID     "
                         "        Offset             Flags[SR|DR|US]\n",
                         sHeader->mSelfPageID,
                         sHeader->mPrevPageID,
                         sHeader->mNextPageID,
                         sHeader->mType,
                         sHeader->mTableOID,
                         sHeader->mAllocListID );

    /* ��ȿ�� Slot ũ�Ⱑ ���� �Ǿ����� */
    if( ( sSlotSize > 0 ) &&
        ( sSlotSize < SMP_PERS_PAGE_BODY_SIZE ) )
    {
        sSlotCnt       = SMP_PERS_PAGE_BODY_SIZE / aSlotSize;

        for( i = 0; i < sSlotCnt; i++)
        {
            sCurSlotHeader = sNxtSlotHeader;

            dumpSlotHeaderByBuffer( sCurSlotHeader,
                                    ID_TRUE, // display table
                                    aOutBuf,
                                    aOutSize );

            sNxtSlotHeader =
                (smpSlotHeader*)((SChar*)sCurSlotHeader + sSlotSize);
            sCurOffset += sSlotSize;

            idlVA::appendFormat( aOutBuf,
                                 aOutSize,
                                 "\n" );
        }
    }
    else
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "\nInvalidSlotSize : %"ID_UINT32_FMT
                             "( %"ID_UINT32_FMT" ~  %"ID_UINT32_FMT" )\n",
                             aSlotSize,
                             0,
                             SMP_PERS_PAGE_BODY_SIZE
                           );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/**********************************************************************
 * BUG-31206 improve usability of DUMPCI and DUMPDDF
 *           Slot Header�� altibase_sm.log�� �����Ѵ�
 *
 * dumpFixedPageByByffer �Լ��� �̿��� TRC boot log�� ������ ����Ѵ�.
 *
 * aSpaceID    - [IN] SpaceID
 * aPageID     - [IN] Page ID
 * aSlotSize   - [IN] Row Slot Size
 **********************************************************************/

IDE_RC smpFixedPageList::dumpFixedPage( scSpaceID         aSpaceID,
                                        scPageID          aPageID,
                                        UInt              aSlotSize )
{
    SChar     * sTempBuffer;
    UChar     * sPagePtr;

    IDE_ASSERT( smmManager::getPersPagePtr( aSpaceID,
                                            aPageID,
                                            (void**)&sPagePtr )
                == IDE_SUCCESS );

    if( iduMemMgr::calloc( IDU_MEM_SM_SMC,
                           1,
                           ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                           (void**)&sTempBuffer )
        == IDE_SUCCESS )
    {
        sTempBuffer[0] = '\0';
        dumpFixedPageByBuffer( sPagePtr,
                               aSlotSize,
                               sTempBuffer,
                               IDE_DUMP_DEST_LIMIT );

        ideLog::log( IDE_SERVER_0,
                     "SpaceID      : %u\n"
                     "PageID       : %u\n"
                     "PageOffset   : %"ID_xPOINTER_FMT"\n"
                     "%s\n",
                     aSpaceID,
                     aPageID,
                     sPagePtr,
                     sTempBuffer );

        (void) iduMemMgr::free( sTempBuffer );
    }

    return IDE_SUCCESS;
}

