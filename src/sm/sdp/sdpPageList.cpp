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
 * $Id: sdpPageList.cpp 86435 2019-12-13 05:59:48Z jiwon.kim $
 *
 * Description :
 *
 * # sdp layer�� ��� ����
 *
 * sdc�� page�� ����Ǵ� ���ڵ忡 ���õ� ����� �����Ѵ�.
 * sdc�� logical header, keymap, slot���� �۾��� ���Ͽ�
 * sdp�� ȣ���ϰ� sdp�� �̷��� ����� sdc�� �����Ѵ�.
 *
 *
 *      sdc - record internal - �÷�, ���ڵ� ���, var col hdr����
 *            page�� ���ڵ� insert, delete, update
 *
 * -------------------------------------
 *
 *
 *      sdp - physical page ����
 *            logical page ����
 *            keymap ����
 *            slot ���� ( alloc, free )
 *
 *
 * sdp layer�� page internal�� ������ ���� index, temp table�� �����ϱ� ����
 * sdpPage ���� table�� �����ϱ� ���� sdpPageList ���, sdpDataPage ����
 * �����Ѵ�. sdpPageList, sdpDataPage, index, temp table������
 * sdpPage�� ����� ȣ���Ѵ�.
 *
 *       sdpDataPage - data page internal �۾��� ���� ��� ����
 *
 *       sdpPageList - table�� �����ϱ� ���� ��� ����
 *                     sdpPageListEntry Ŭ���� ����
 *
 * -------------------------------------------------
 *
 *       sdpPage  - index, temp table�� ������ internal�۾��� ����
 *                  ��� ����
 *                  page�� physical header����
 *                  page�� free size����
 *                  keymap�� ����
 *                  page�� free offset����
 *
 * �� �ܿ��� sdp layer���� Segment, TableSpace, Extent�� ������
 * page, extent�� �����Ѵ�.
 *
 * # data page-list entry ����
 *
 * �ϳ��� ���̺� ���� ������ �Ҵ�,
 * ������ ������ slot�� ������ ���� ����� �����Ѵ�.
 *
 * - parameter type�� ��Ģ
 *   logical header�� �ʿ��� ���� sdpDataPageHdr * type�� �޴´�.
 *   insertable page list entry�� �ʿ��� ���� sdpTableMetaPageHdr * type�� �޴´�.
 *
 **********************************************************************/
#include <smErrorCode.h>
#include <smiDef.h>
#include <smu.h>
#include <sdd.h>
#include <sdr.h>
#include <smr.h>
#include <sct.h>

#include <sdpDef.h>
#include <sdpTableSpace.h>
#include <sdpPageList.h>
#include <sdpReq.h>

#include <sdpModule.h>
#include <sdpDPathInfoMgr.h>
#include <sdpSegDescMgr.h>
#include <sdpPhyPage.h>

/***********************************************************************
 * Description : page list entry �ʱ�ȭ
 ***********************************************************************/
void sdpPageList::initializePageListEntry( sdpPageListEntry*  aPageEntry,
                                           vULong             aSlotSize,
                                           smiSegAttr         aSegmentAttr,
                                           smiSegStorageAttr  aSegmentStoAttr )

{
    // fixed page list
    aPageEntry->mSlotSize = aSlotSize;

    aPageEntry->mSegDesc.mSegHandle.mSpaceID    = SC_NULL_SPACEID;
    aPageEntry->mSegDesc.mSegHandle.mSegPID     = SD_NULL_PID;
    aPageEntry->mSegDesc.mSegHandle.mSegAttr    = aSegmentAttr;
    aPageEntry->mSegDesc.mSegHandle.mSegStoAttr = aSegmentStoAttr;

    aPageEntry->mRecCnt           = 0;

    return;
}

/*
  Page List Entry�� Runtime Item�� NULL�� �����Ѵ�.

  OFFLINE/DISCARD�� Tablespace���� Table�� ���� ����ȴ�.
*/
IDE_RC sdpPageList::setRuntimeNull( sdpPageListEntry* aPageEntry )
{
    sdpSegMgmtOp   * sSegMgmtOp;
    IDE_DASSERT( aPageEntry != NULL );

    aPageEntry->mMutex = NULL;
    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp(aPageEntry);
    IDE_ERROR( sSegMgmtOp != NULL );

    // Segment �������� Cache�� �����Ѵ�.
    IDE_TEST ( sSegMgmtOp->mDestroy( &(aPageEntry->mSegDesc.mSegHandle) )
               != IDE_SUCCESS );

    aPageEntry->mSegDesc.mSegHandle.mCache = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page list entry�� runtime ���� �ʱ�ȭ
 ***********************************************************************/
IDE_RC sdpPageList::initEntryAtRuntime( scSpaceID         aSpaceID,
                                        smOID             aTableOID,
                                        UInt              aIndexID,
                                        sdpPageListEntry* aPageEntry,
                                        idBool            aDoInitMutex /* ID_TRUE */ )
{
    UInt        sState  = 0;

    IDE_DASSERT( aPageEntry != NULL );

    if (aDoInitMutex == ID_TRUE)
    {
        SChar sBuffer[128];
        idlOS::memset(sBuffer, 0, 128);

        idlOS::snprintf(sBuffer,
                        128,
                        "DISK_PAGE_LIST_MUTEX_%"ID_XINT64_FMT,
                        (ULong)aTableOID);

        /* sdpPageList_initEntryAtRuntime_malloc_Mutex.tc */
        IDU_FIT_POINT("sdpPageList::initEntryAtRuntime::malloc::Mutex");
        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDP,
                                   ID_SIZEOF(iduMutex),
                                   (void **)&(aPageEntry->mMutex),
                                   IDU_MEM_FORCE)
                 != IDE_SUCCESS);
        sState  = 1;

        IDE_TEST(aPageEntry->mMutex->initialize(
                     sBuffer,
                     IDU_MUTEX_KIND_NATIVE,
                     IDV_WAIT_INDEX_LATCH_FREE_DRDB_PAGE_LIST_ENTRY )
                 != IDE_SUCCESS);
    }
    else
    {
        aPageEntry->mMutex = NULL;
    }

    // Segment Cache �ʱ�ȭ
    IDE_TEST( sdpSegDescMgr::initSegDesc(
                  &aPageEntry->mSegDesc,
                  aSpaceID,
                  sdpSegDescMgr::getSegPID( &aPageEntry->mSegDesc ),
                  SDP_SEG_TYPE_TABLE,
                  aTableOID,
                  aIndexID) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free( aPageEntry->mMutex ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : page list entry�� runtime ���� ����
 ***********************************************************************/
IDE_RC sdpPageList::finEntryAtRuntime( sdpPageListEntry* aPageEntry )
{
    IDE_DASSERT( aPageEntry != NULL );

    if ( aPageEntry->mMutex != NULL )
    {
        /* page list entry�� mutex ���� */
        IDE_TEST(aPageEntry->mMutex->destroy() != IDE_SUCCESS);

        IDE_TEST(iduMemMgr::free(aPageEntry->mMutex) != IDE_SUCCESS);
        aPageEntry->mMutex = NULL;
    }
    else
    {
        // Do Nothing
        // OFFLINE/DISCARD Tablespace�� ���� Table�� ���
        // mMutex�� NULL�� ���� �ִ�.
    }

    if ( aPageEntry->mSegDesc.mSegHandle.mCache != NULL )
    {
        /* BUG-29941 - SDP ��⿡ �޸� ������ �����մϴ�.
         * ���⼭ initEntryAtRuntime���� �Ҵ��� �޸𸮸� �����Ѵ�. */
        IDE_TEST( sdpSegDescMgr::destSegDesc( &aPageEntry->mSegDesc )
                  != IDE_SUCCESS );
    }
    else
    {
        // Do Nothing
        // OFFLINE/DISCARD Tablespace�� ���� Table�� ���
        // Segment Cache�� NULL�� ���� �ִ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * PROJ-1566
 *
 * Description : record�� append�� free page�� ã��
 *
 * Implementation :
 *    (1) Free Page �Ҵ����
 *    (2) ���� Page�� ���� ���� ����
 *    sdpPageList::findSlot4AppendRec()���� ȣ��
 *
 *    - In
 *      aStatistics     : statistics
 *      aTrans          : transaction
 *      aSpaceID        : tablespace ID
 *      aPageEntry      : page list entry
 *      aDPathSegInfo   : Direct-Path Insert Information
 *      aMtx            : mini transaction
 *      aPrevPhyPageHdr : ���� physical page header
 *      aPrevPageID     : ���� page ID
 *      aStartExtRID    : free page�� �����ϴ� ù��° extent RID
 *      aIsLogging      : Logging ����
 *    - Out
 *      aFindPagePtr     : ã�� free page pointer
 **********************************************************************/
IDE_RC sdpPageList::allocPage4AppendRec( idvSQL           * aStatistics,
                                         scSpaceID          aSpaceID,
                                         sdpPageListEntry * aPageEntry,
                                         sdpDPathSegInfo  * aDPathSegInfo,
                                         sdpPageType        aPageType,
                                         sdrMtx           * aMtx,
                                         idBool             aIsLogging,
                                         UChar           ** aAllocPagePtr )
{
    sdRID              sPrvAllocExtRID;
    sdRID              sFstPIDOfPrvAllocExt;
    scPageID           sPrvAllocPID;

    scPageID           sFstPIDOfAllocExt;
    sdRID              sAllocExtRID;
    scPageID           sAllocPID;
    UChar              sIsConsistent;
    sdpSegHandle     * sSegHandle;
    UChar            * sAllocPagePtr;
    sdpSegMgmtOp     * sSegMgmtOp;

    sPrvAllocPID         = aDPathSegInfo->mLstAllocPID;
    sPrvAllocExtRID      = aDPathSegInfo->mLstAllocExtRID;
    sFstPIDOfPrvAllocExt = aDPathSegInfo->mFstPIDOfLstAllocExt;
    sSegHandle           = &aDPathSegInfo->mSegDesc->mSegHandle;
    sSegMgmtOp           = sdpSegDescMgr::getSegMgmtOp( aPageEntry );
    IDE_ERROR( sSegMgmtOp != NULL ); 

    IDE_TEST( sSegMgmtOp->mAllocNewPage4Append( aStatistics,
                                                aMtx,
                                                aSpaceID,
                                                sSegHandle,
                                                sPrvAllocExtRID,
                                                sFstPIDOfPrvAllocExt,
                                                sPrvAllocPID,
                                                aIsLogging,
                                                aPageType,
                                                &sAllocExtRID,
                                                &sFstPIDOfAllocExt,
                                                &sAllocPID,
                                                &sAllocPagePtr )
              != IDE_SUCCESS );

    IDE_ASSERT( sAllocPagePtr != NULL );

    aDPathSegInfo->mTotalPageCount++;

    // PROJ-1665
    if ( aIsLogging == ID_FALSE )
    {
        // no logging mode�� ���,
        // page���°� inconsistent �ϴٴ� ���� ������ �α�
        // ��, page ���°� inconsistent �ϴٰ� ���������� ����
        //---------------------------------------------------------
        // < �̿� ���� ó���ϴ� ���� >
        // Media Recovery �ÿ� Direct-Path INSERT�� nologging����
        // ���� �Ϸ�� Page�� ���Ͽ� DML ������ ���� �ش� Page��
        // �ƹ� ������ ���� ���� �ʱ� ������ ������ �� �� �ִ�.
        // ���� �̷� ������ �����ϱ� ���Ͽ�
        // media recovery�ÿ� �Ʒ��� ���� log�� ����
        // �ش� Page ���¸� in-consistent�� �����Ѵ�.
        sIsConsistent = SDP_PAGE_INCONSISTENT;

        IDE_TEST( smrLogMgr::writePageConsistentLogRec( aStatistics,
                                                        aSpaceID,
                                                        sAllocPID,
                                                        sIsConsistent )
                  != IDE_SUCCESS );
    }

    /* DPath�� �Ҵ��� ������ �������� �� �������� ���� ExtRID��
     * ������ �ִ´�. �ֳ��ϸ� Commit�� Target ���̺��� ExtList��
     * ����� HWM�� ������ ���ؼ� �̴�. */

    aDPathSegInfo->mLstAllocPID         = sAllocPID;
    aDPathSegInfo->mLstAllocExtRID      = sAllocExtRID;
    aDPathSegInfo->mFstPIDOfLstAllocExt = sFstPIDOfAllocExt;

    *aAllocPagePtr = sAllocPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aAllocPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-1566
 *
 * Description : append�� ���� slot�� ã��
 * Implementation :
 *    (1) Max Row�� ���� �ʴ��� �˻�
 *    (2) ���ο� Free page�� �ʿ��� �� �˻��� ��,
 *        ���ο� Free Page�� �ʿ��ϸ� ���� �Ҵ� ����
 *    (3) ���� ������ Page �Ǵ� ���ο� Free page�� Append�� slot ��ġ
 *        ã�� ��������
 *
 *    - In
 *      aStatistics    : statistics
 *      aTrans         : transaction
 *      aSpaceID       : tablespace ID
 *      aPageEntry     : page list entry
 *      aTableInfo     : table info
 *      aRecSize       : record size
 *      aMaxRow        : table�� ������ �ִ� max row ����
 *      aIsLoggingMode : Direct-Path INSERT�� Logging Mode��
 *      aStartInfo     : mini transaction start info
 *    - Out
 *      aPageHdr       : append �� record�� ���� page header
 *      aAllocSlotRID  : append �� record�� ����� slot�� RID
 **********************************************************************/
IDE_RC sdpPageList::findSlot4AppendRec( idvSQL           * aStatistics,
                                        scSpaceID          aSpaceID,
                                        sdpPageListEntry * aPageEntry,
                                        sdpDPathSegInfo  * aDPathSegInfo,
                                        UInt               aRecSize,
                                        idBool             aIsLogging,
                                        sdrMtx           * aMtx,
                                        UChar           ** aPageHdr,
                                        UChar            * aNewCTSlotIdx )
{
    UChar            * sLstAllocPagePtr;
    UChar            * sFreePagePtr;
    idBool             sIsNeedNewPage;
    scPageID           sFreePID;
    UChar              sNewCTSlotIdx;

    IDE_DASSERT( aPageEntry != NULL );
    IDE_DASSERT( aMtx       != NULL );

    sFreePagePtr   = NULL;
    sIsNeedNewPage = ID_TRUE;

    /* Append�̱⶧���� ������ ���������� ������ �ִ��� �˻翩
     * ���ο� Free Page �Ҵ� ���θ� �����Ѵ�. */
    sLstAllocPagePtr    = aDPathSegInfo->mLstAllocPagePtr;

    if( sLstAllocPagePtr != NULL )
    {
        if( canInsert2Page4Append( aPageEntry, sLstAllocPagePtr, aRecSize )
            == ID_TRUE )
        {
            sIsNeedNewPage = ID_FALSE;
            sFreePagePtr   = sLstAllocPagePtr;
        }
    }

    if( sIsNeedNewPage == ID_TRUE )
    {
        IDE_TEST( allocPage4AppendRec( aStatistics,
                                       aSpaceID,
                                       aPageEntry,
                                       aDPathSegInfo,
                                       SDP_PAGE_DATA,
                                       aMtx,
                                       aIsLogging,
                                       &sFreePagePtr )
                  != IDE_SUCCESS );

        sFreePID = sdpPhyPage::getPageIDFromPtr( sFreePagePtr );

        if( sLstAllocPagePtr != NULL )
        {
            /* update only�� ���� Page�� DPath Buffer�� flush list��
             * ������ */
            IDE_TEST( sdbDPathBufferMgr::setDirtyPage( (void*)sLstAllocPagePtr )
                      != IDE_SUCCESS );
        }

        aDPathSegInfo->mLstAllocPagePtr = sFreePagePtr;

        if( aDPathSegInfo->mFstAllocPID == SD_NULL_PID )
        {
            aDPathSegInfo->mFstAllocPID = sFreePID;
        }

        IDE_ASSERT( sFreePagePtr != NULL );

    }

    // Direct-Path Insert�� ������ 0��° CTS�� �Ҵ��Ѵ�.
    IDE_ASSERT( smLayerCallback::allocCTS( aStatistics,
                                           aMtx,   /* aFixMtx */
                                           aMtx,   /* aLogMtx */
                                           SDB_SINGLE_PAGE_READ,
                                           (sdpPhyPageHdr*)sFreePagePtr,
                                           &sNewCTSlotIdx )
                == IDE_SUCCESS );

    *aPageHdr      = sFreePagePtr;
    *aNewCTSlotIdx = sNewCTSlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Insert ������ �������� Ž���Ѵ�.
 ***********************************************************************/
IDE_RC sdpPageList::findPage( idvSQL            *aStatistics,
                              scSpaceID          aSpaceID,
                              sdpPageListEntry  *aPageEntry,
                              void              *aTableInfoPtr,
                              UInt               aRecSize,
                              ULong              aMaxRow,
                              idBool             aAllocNewPage,
                              idBool             aNeedSlotEntry,
                              sdrMtx            *aMtx,
                              UChar            **aPagePtr,
                              UChar             *aNewCTSlotIdx )
{
    UChar            sNewCTSlotIdx;
    sdpPhyPageHdr  * sPagePtr;
    sdpSegHandle   * sSegHandle;
    sdpSegMgmtOp   * sSegMgmtOp;

    IDE_DASSERT( aRecSize      != 0 );
    IDE_DASSERT( aMaxRow       != 0 );
    IDE_DASSERT( aPageEntry    != NULL );
    IDE_DASSERT( aPagePtr      != NULL );
    IDE_DASSERT( aNewCTSlotIdx != NULL );

    sPagePtr      = NULL;
    sNewCTSlotIdx = SDP_CTS_IDX_NULL;
    sSegHandle    = &(aPageEntry->mSegDesc.mSegHandle);

    if (aTableInfoPtr != NULL)
    {
        // max row�� ���� �ʴ��� �˻�
        IDE_TEST( validateMaxRow( aPageEntry,
                                  aTableInfoPtr,
                                  aMaxRow )
                  != IDE_SUCCESS );
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( aPageEntry );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    if( aAllocNewPage == ID_TRUE )
    {
        IDE_TEST( sSegMgmtOp->mAllocNewPage(
                              aStatistics,
                              aMtx,
                              aSpaceID,
                              sSegHandle,
                              SDP_PAGE_DATA,
                              (UChar**)&sPagePtr )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( sSegMgmtOp->mFindInsertablePage(
                              aStatistics,
                              aMtx,
                              aSpaceID,
                              sSegHandle,
                              aTableInfoPtr,
                              SDP_PAGE_DATA,
                              aRecSize,
                              aNeedSlotEntry,
                              (UChar**)&sPagePtr,
                              &sNewCTSlotIdx )
                  != IDE_SUCCESS );
    }

    if ( sNewCTSlotIdx == SDP_CTS_IDX_NULL )
    {
        IDE_ASSERT( smLayerCallback::allocCTS( aStatistics,
                                               aMtx,  /* aFixMtx */
                                               aMtx,  /* aLogMtx */
                                               SDB_SINGLE_PAGE_READ,
                                               sPagePtr,
                                               &sNewCTSlotIdx )
                     == IDE_SUCCESS );
    }
    else
    {
        /* Insert �ÿ��� ������������ �ִ��� CTS�� �Ҵ� ���� ���� ����. */
    }

    IDE_ASSERT( sPagePtr != NULL );

    *aPagePtr   = (UChar*)sPagePtr;
    *aNewCTSlotIdx = sNewCTSlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ������ insert�� ���� slot �Ҵ�
 * findSlot4InsRec�� ���� �����Ͽ� slot�� Ȯ���ϰ� �� �� ȣ��ȴ�.
 **********************************************************************/
IDE_RC sdpPageList::allocSlot( UChar*       aPagePtr,
                               UShort       aSlotSize,
                               UChar**      aSlotPtr,
                               sdSID*       aAllocSlotSID )
{
    sdpPhyPageHdr      *sPhyPageHdr;
    scSlotNum           sAllocSlotNum;
    scOffset            sAllocSlotOffset;

    IDE_DASSERT( aPagePtr != NULL );
    IDE_DASSERT( aSlotPtr != NULL );

    sPhyPageHdr = sdpPhyPage::getHdr(aPagePtr);

    IDE_TEST( sdpPhyPage::allocSlot4SID( sPhyPageHdr,
                                         aSlotSize,
                                         aSlotPtr,
                                         &sAllocSlotNum,
                                         &sAllocSlotOffset )
              != IDE_SUCCESS );

    IDE_ERROR( aSlotPtr != NULL );

    *aAllocSlotSID =
        SD_MAKE_SID( sdpPhyPage::getPageID((UChar*)sPhyPageHdr),
                     sAllocSlotNum );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


// �����Ҵ����� �������� ���뵵 �� ���� Segment �ڷᱸ���� �����Ѵ�.
IDE_RC sdpPageList::updatePageState( idvSQL            * aStatistics,
                                     scSpaceID           aSpaceID,
                                     sdpPageListEntry  * aEntry,
                                     UChar             * aPagePtr,
                                     sdrMtx            * aMtx )
{
    IDE_ASSERT( aEntry != NULL );
    IDE_ASSERT( aPagePtr != NULL );
    IDE_ASSERT( aMtx != NULL );

    IDE_TEST( sdpSegDescMgr::getSegMgmtOp( aEntry )->mUpdatePageState(
                  aStatistics,
                  aMtx,
                  aSpaceID,
                  sdpSegDescMgr::getSegHandle( aEntry ),
                  aPagePtr ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : slot�� free �Ѵ�.
 **********************************************************************/
IDE_RC sdpPageList::freeSlot( idvSQL            *aStatistics,
                              scSpaceID          aTableSpaceID,
                              sdpPageListEntry  *aPageEntry,
                              UChar             *aSlotPtr,
                              scGRID             aSlotGRID,
                              sdrMtx            *aMtx )
{
    UChar               * sRowSlot     = aSlotPtr;
    sdpPhyPageHdr       * sPhyPageHdr;
    UChar               * sSlotDirPtr;
    UShort                sRowSlotSize = 0;

    IDE_DASSERT( aPageEntry     != NULL );
    IDE_DASSERT( aSlotPtr       != NULL );
    IDE_DASSERT( aMtx           != NULL );
    IDE_DASSERT( SC_GRID_IS_NOT_NULL(aSlotGRID) );

    sPhyPageHdr  = sdpPhyPage::getHdr((UChar*)sRowSlot);
    sRowSlotSize = smLayerCallback::getRowPieceSize( sRowSlot );

    IDE_TEST( sdrMiniTrans::writeLogRec(aMtx,
                                        aSlotGRID,
                                        (void*)&sRowSlotSize,
                                        ID_SIZEOF(sRowSlotSize),
                                        SDR_SDP_FREE_SLOT_FOR_SID )
              != IDE_SUCCESS );

    IDE_TEST( sdpPhyPage::freeSlot4SID( sPhyPageHdr,
                                        sRowSlot,
                                        SC_MAKE_SLOTNUM( aSlotGRID ),
                                        sRowSlotSize )
              != IDE_SUCCESS );

    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr((UChar*)sPhyPageHdr);

    if( sdpSlotDirectory::getCount(sSlotDirPtr) == 0 )
    {
        /* ������ ���������� UnFormat Page List�� �־��ش�. */
        IDE_TEST( sdpSegDescMgr::getSegMgmtOp( aPageEntry )->mFreePage(
                                              aStatistics,
                                              aMtx,
                                              aTableSpaceID,
                                              &(aPageEntry->mSegDesc.mSegHandle),
                                              (UChar*)sPhyPageHdr )
                  != IDE_SUCCESS );
    }
    else
    {
        if( sdpPhyPage::getPageType(sPhyPageHdr)
            == SDP_PAGE_DATA )
        {
            IDE_TEST( updatePageState( aStatistics,
                                       aTableSpaceID,
                                       aPageEntry,
                                       (UChar*)sPhyPageHdr,
                                       aMtx )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �α뿡 �ʿ��� page list entry ���� ��ȯ(request api)
 **********************************************************************/
void sdpPageList::getPageListEntryInfo( void*       aPageEntry,
                                        scPageID*   aSegPID )
{
    sdpPageListEntry* sPageEntry;

    IDE_ASSERT( aPageEntry != NULL );
    IDE_ASSERT( aSegPID    != NULL );

    sPageEntry = (sdpPageListEntry*)aPageEntry;

    *aSegPID = getTableSegDescPID( sPageEntry );
}

/***********************************************************************
 *
 * Description :
 *  insert rowpiece ����ÿ�, allocNewPage�� ���� �Ǵ� findPage�� ����
 *  ���θ� �˻��ϴ� �Լ��̴�.
 *
 *  empty page�� aDataSize ũ���� row�� �ִ� ��쿡,
 *  PCTFREE ������ �����ϴ��� üũ�Ѵ�.
 *
 *  1. �������� freespace�� ������ PCTFREE���� ������
 *     ������ allocNewPage�� �ؾ��Ѵ�.
 *     �ֳ��ϸ� ������������ row�� �ְԵǸ�
 *     �� �������� 100% PCTFREE ������ �������� ���� ���̱� �����̴�.
 *     (empty page�� row�� �־ PCTFREE ������ �������� �ʾҴ�.)
 *     ������ ����� row�� update�� ����� ������ ���ܵξ�� �Ѵ�.
 *
 *  2. �������� freespace�� ������ PCTFREE���� ũ��
 *     findPage�� �õ��غ���.
 *
 *  aEntry    - [IN] page list entry
 *  aDataSize - [IN] insert�Ϸ��� data�� ũ��
 *
 **********************************************************************/
idBool sdpPageList::needAllocNewPage( sdpPageListEntry *aEntry,
                                      UInt              aDataSize )
{
    UInt   sRemainSize;
    UInt   sPctFree;
    UInt   sPctFreeSize;

    IDE_DASSERT( aEntry !=NULL );
    IDE_DASSERT( aDataSize >0 );

    sPctFree =
        sdpSegDescMgr::getSegAttr( &(aEntry->mSegDesc) )->mPctFree;

    sPctFreeSize = ( SD_PAGE_SIZE * sPctFree )  / 100 ;

    sRemainSize = SD_PAGE_SIZE
        - sdpPhyPage::getPhyHdrSize()
        - ID_SIZEOF(sdpPageFooter)
        - aDataSize
        - ID_SIZEOF(sdpSlotEntry);

    if( sRemainSize < sPctFreeSize )
    {
        /* �������� freespace�� ������ PCTFREE���� ������
         * ������ allocNewPage�� �ؾ��Ѵ�.
         * �ֳ��ϸ� ������������ row�� �ְԵǸ�
         * �� �������� 100% PCTFREE ������ �������� ���� ���̱� �����̴�.
         * (empty page�� row�� �־ PCTFREE ������ �������� �ʾҴ�.)
         * ������ ����� row�� update�� ����� ������ ���ܵξ�� �Ѵ�. */
        return ID_TRUE;
    }
    else
    {
        /* �������� freespace�� ������ PCTFREE���� ũ��
         * findPage�� �õ��غ���. */
        return ID_FALSE;
    }
}

/***********************************************************************
 *
 * Description :
 *  �� �������� �����͸� insert�� �� �ִ��� üũ�Ѵ�.
 *  1. canAllocSlot �������� üũ
 *  2. �� �������� �����͸� �־��� ���,
 *     freespace�� ������ PCTFREE ���� �۾�������
 *     �ʴ��� üũ
 *
 *  aEntry    - [IN] page list entry
 *  aPagePtr  - [IN] page ptr
 *  aDataSize - [IN] insert�Ϸ��� data�� ũ��
 *
 **********************************************************************/
idBool sdpPageList::canInsert2Page4Append( sdpPageListEntry *aEntry,
                                           UChar            *aPagePtr,
                                           UInt              aDataSize )
{
    UInt            sRemainSize;
    UInt            sPctFree;
    UInt            sPctFreeSize;
    idBool          sCanAllocSlot;
    sdpPhyPageHdr * sPageHdr;

    IDE_DASSERT(aEntry!=NULL);
    IDE_DASSERT(aDataSize>0);

    sPageHdr = sdpPhyPage::getHdr( aPagePtr );

    sCanAllocSlot =  sdpPhyPage::canAllocSlot( sPageHdr,
                                               aDataSize,
                                               ID_TRUE, /* Use Key Slot */
                                               SDP_8BYTE_ALIGN_SIZE );

    if( sCanAllocSlot == ID_TRUE )
    {
        sPctFree =
            sdpSegDescMgr::getSegAttr( &(aEntry->mSegDesc) )->mPctFree;

        sPctFreeSize = ( SD_PAGE_SIZE * sPctFree )  / 100 ;

        sRemainSize = sdpPhyPage::getTotalFreeSize( sPageHdr )
                      - ID_SIZEOF(sdpSlotEntry)
                      - aDataSize;

        if( sRemainSize < sPctFreeSize )
        {
            /* �� �������� ���ο� row�� �ְԵǸ�,
             * �������� freespace�� ������
             * PCTFREE ���� �۾����� �ȴ�.
             * �׷��� �� �������� insert�� �� ����. */
            sCanAllocSlot = ID_FALSE;
        }
    }

    return sCanAllocSlot;
}

/***********************************************************************
 * Description : record count ��ȯ
 * record count�� insert�� record - delete�� record�� ����
 ***********************************************************************/
#ifndef COMPILE_64BIT
IDE_RC sdpPageList::getRecordCount( idvSQL*           aStatistics,
                                    sdpPageListEntry* aPageEntry,
                                    ULong*            aRecordCount,
                                    idBool            aLockMutex /* ID_TRUE */)
#else
IDE_RC sdpPageList::getRecordCount( idvSQL*           /* aStatistics */,
                                    sdpPageListEntry* aPageEntry,
                                    ULong*            aRecordCount,
                                    idBool            /* aLockMutex */)
#endif
{
    IDE_DASSERT( aPageEntry != NULL );
    IDE_DASSERT( aRecordCount != NULL );

    /*
     * TASK-4690
     * 64��Ʈ������ 64��Ʈ ������ atomic�ϰ� read/write�� �� �ִ�.
     * ��, �Ʒ� mutex�� ��� Ǫ�� ���� ���ʿ��� lock�̴�.
     * ���� 64��Ʈ�� ��쿣 lock�� ���� �ʵ��� �Ѵ�.
     */
#ifndef COMPILE_64BIT
    /* mutex lock ȹ�� */
    if (aLockMutex == ID_TRUE)
    {
        IDE_TEST( lock( aStatistics, aPageEntry ) != IDE_SUCCESS );
    }
#endif

    /* record count ��� */
    // BUG-47368: atomic �������� ���� 
    *aRecordCount = idCore::acpAtomicGet64( &(aPageEntry->mRecCnt) );

#ifndef COMPILE_64BIT
    /* mutex lock release  */
    if (aLockMutex == ID_TRUE)
    {
        IDE_TEST( unlock( aPageEntry ) != IDE_SUCCESS );
    }
#endif

    return IDE_SUCCESS;

#ifndef COMPILE_64BIT
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#endif
}

/***********************************************************************
 * Description : page list entry�� item size�� align��Ų slot size ��ȯ
 **********************************************************************/
UInt sdpPageList::getSlotSize( sdpPageListEntry* aPageEntry )
{

    IDE_DASSERT( aPageEntry != NULL );

    return aPageEntry->mSlotSize;

}

/***********************************************************************
 * Description : page list entry�� seg desc rID ��ȯ
 **********************************************************************/
scPageID sdpPageList::getTableSegDescPID( sdpPageListEntry*   aPageEntry )
{
    IDE_DASSERT( aPageEntry != NULL );

    return aPageEntry->mSegDesc.mSegHandle.mSegPID;
}

/***********************************************************************
 * Description : page list entry�� insert count�� get
 * �� �Լ��� ȣ��Ǳ����� �ݵ�� entry�� mutex�� ȹ��� ���¿����Ѵ�.
 * return aPageEntry->insert count
 **********************************************************************/
ULong sdpPageList::getRecCnt( sdpPageListEntry*  aPageEntry )
{
    return aPageEntry->mRecCnt;
}

/***********************************************************************
 PROJ-1566
 Description : write pointer�� page�� table page���� �˻�

***********************************************************************/
idBool sdpPageList::isTablePageType ( UChar * aWritePtr )
{
    sdpPageType sPageType;

    sPageType = sdpPhyPage::getPageType( sdpPhyPage::getHdr(aWritePtr) );

    if ( sPageType == SDP_PAGE_DATA )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/***********************************************************************

  Description : MAX ROW�� �Ѵ��� �˻�

***********************************************************************/
IDE_RC sdpPageList::validateMaxRow( sdpPageListEntry * aPageEntry,
                                    void             * aTableInfoPtr,
                                    ULong              aMaxRow )
{
    ULong sRecordCnt;

    /* BUG-19573 Table�� Max Row�� Disable�Ǿ������� �� ���� insert�ÿ�
     *           Check���� �ʾƾ� �Ѵ�. */
    if( aMaxRow != ID_ULONG_MAX )
    {
        // BUG-47368: mutex ����, atomic �������� ���� 
        sRecordCnt = idCore::acpAtomicGet64( &(aPageEntry->mRecCnt) );
        sRecordCnt += smLayerCallback::getRecCntOfTableInfo( aTableInfoPtr );

        IDE_TEST_RAISE(aMaxRow <= sRecordCnt, err_exceed_maxrow);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_exceed_maxrow);
    {
        IDE_SET(ideSetErrorCode (smERR_ABORT_ExceedMaxRows));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdpPageList::getAllocPageCnt( idvSQL*         aStatistics,
                                     scSpaceID       aSpaceID,
                                     sdpSegmentDesc *aSegDesc,
                                     ULong          *aFmtPageCnt )
{
    IDE_TEST( aSegDesc->mSegMgmtOp->mGetFmtPageCnt( aStatistics,
                                                    aSpaceID,
                                                    &aSegDesc->mSegHandle,
                                                    aFmtPageCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdpPageList::addPage4TempSeg( idvSQL           *aStatistics,
                                     sdpSegHandle     *aSegHandle,
                                     UChar            *aPrvPagePtr,
                                     scPageID          aPrvPID,
                                     UChar            *aPagePtr,
                                     scPageID          aPID,
                                     sdrMtx           *aMtx )
{
    sdpPhyPageHdr       *sPageHdr;
    sdpDblPIDListNode   *sPageNode;
    sdpPhyPageHdr       *sPrvPageHdr;
    sdpDblPIDListNode   *sPrvPageNode;
    sdpPhyPageHdr       *sNxtPageHdr;
    sdpDblPIDListNode   *sNxtPageNode;
    sdpSegCCache        *sCommonCache;
    UChar               *sNxtPagePtr;
    UChar               *sPrvPagePtr;
    idBool               sTrySuccess;
    scSpaceID            sSpaceID;

    sCommonCache = (sdpSegCCache*) ( aSegHandle->mCache );
    sPageHdr     = sdpPhyPage::getHdr( aPagePtr );
    sPageNode    = sdpPhyPage::getDblPIDListNode( sPageHdr );
    sSpaceID     = sdpPhyPage::getSpaceID( aPagePtr );

    if( aPrvPID == SD_NULL_PID )
    {
        if( sCommonCache->mTmpSegTail != SD_NULL_PID )
        {
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  sSpaceID,
                                                  sCommonCache->mTmpSegTail,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  aMtx,
                                                  &sPrvPagePtr,
                                                  &sTrySuccess,
                                                  NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

            sPrvPageHdr  = sdpPhyPage::getHdr( sPrvPagePtr );
            sPrvPageNode = sdpPhyPage::getDblPIDListNode( sPrvPageHdr );

            IDE_ASSERT( sPrvPageNode->mNext == SD_NULL_PID );

            sPrvPageNode->mNext = aPID;
        }
        else
        {
            sCommonCache->mTmpSegHead = aPID;
        }

        sPageNode->mPrev = sCommonCache->mTmpSegTail;
        sPageNode->mNext = SD_NULL_PID;

        sCommonCache->mTmpSegTail = aPID;
    }
    else
    {
        sPrvPageHdr  = sdpPhyPage::getHdr( aPrvPagePtr );
        sPrvPageNode = sdpPhyPage::getDblPIDListNode( sPrvPageHdr );

        sPageNode->mNext = sPrvPageNode->mNext;
        sPageNode->mPrev = aPrvPID;

        sPrvPageNode->mNext = aPID;

        if( sCommonCache->mTmpSegTail == aPrvPID )
        {
            sCommonCache->mTmpSegTail = aPID;
        }
        else
        {
            IDE_ASSERT( sPageNode->mNext != SD_NULL_PID );
            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                  sSpaceID,
                                                  sPageNode->mNext,
                                                  SDB_X_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  aMtx,
                                                  &sNxtPagePtr,
                                                  &sTrySuccess,
                                                  NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

            sNxtPageHdr  = sdpPhyPage::getHdr( sNxtPagePtr );
            sNxtPageNode = sdpPhyPage::getDblPIDListNode( sNxtPageHdr );

            sNxtPageNode->mPrev = aPID;
        }
    }

    IDE_ASSERT( sCommonCache->mTmpSegHead != SD_NULL_PID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :  Data Page ������ �ʱ� CTL Size�� ����Ͽ� ��ȯ
 *
 * DataPage�� InitTrans �Ӽ����� ����Ͽ� Max RowPiece��
 * ũ�⸦ ������ �� �ִ�.
 **********************************************************************/
UShort sdpPageList::getSizeOfInitTrans( sdpPageListEntry * aPageEntry )
{
    UShort sInitTrans =
        sdpSegDescMgr::getSegAttr(
            &aPageEntry->mSegDesc )->mInitTrans;

    return ( (sInitTrans * ID_SIZEOF(sdpCTS)) +
             idlOS::align8(ID_SIZEOF(sdpCTL)));
}
