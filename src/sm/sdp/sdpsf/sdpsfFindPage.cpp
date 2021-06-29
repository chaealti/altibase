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
 * $Id:$
 ***********************************************************************/

#include <smxTrans.h>

#include <sdr.h>
#include <sdp.h>
#include <sdpPhyPage.h>
#include <sdpSglRIDList.h>

#include <sdpsfDef.h>
#include <sdpsfSH.h>
#include <sdpReq.h>
#include <sdpsfAllocPage.h>
#include <sdpsfFindPage.h>

/***********************************************************************
 * Description : aRecordSize�� ���ϵ� Free Page�� ã�´�.
 *
 * 1. SegHdr�� SLatch�� ��� Free PID List�� ã�´�.
 * 2. ���� Free PID List�� ���ٸ� ���ο� �������� �Ҵ��Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHandle   - [IN] Segment Handle
 * aTableInfo   - [IN] Insert, update, delete�� Transaction�� �߰��Ǵ�
 *                     TableInfo�����μ� InsertPID Hint�� �����ϱ� ����
 *                     ���
 * aPageType    - [IN] Page Type
 * aRecordSize  - [IN] Record Size
 * aNeedKeySlot - [IN] ������ free���� ���� KeySlot�� �����ؼ� ����ؾ�
 *                     �Ǹ� ID_TRUE, else ID_FALSE
 *
 * aPagePtr     - [OUT] Free������ ���� Page�� ���� Pointer�� �����ȴ�.
 *                      return �� �ش� �������� XLatch�� �ɷ��ִ�.
 *
 ***********************************************************************/
IDE_RC sdpsfFindPage::findFreePage( idvSQL           *aStatistics,
                                    sdrMtx           *aMtx,
                                    scSpaceID         aSpaceID,
                                    sdpSegHandle     *aSegHandle,
                                    void             *aTableInfo,
                                    sdpPageType       aPageType,
                                    UInt              aRecordSize,
                                    idBool            aNeedKeySlot,
                                    UChar           **aPagePtr,
                                    UChar            *aCTSlotIdx )
{
    UChar         * sPagePtr;
    sdpsfSegHdr   * sSegHdr;
    scPageID        sSegPID;
    scPageID        sHintDataPID;
    SInt            sState = 0;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aSegHandle   != NULL );
    IDE_ASSERT( aPageType    < SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aRecordSize  < SD_PAGE_SIZE );
    IDE_ASSERT( aRecordSize  > 0 );
    IDE_ASSERT( aPagePtr     != NULL );

    sPagePtr  = NULL;
    sSegPID   = aSegHandle->mSegPID;

    IDE_TEST( checkHintPID4Insert( aStatistics,
                                   aMtx,
                                   aSpaceID,
                                   aTableInfo,
                                   aRecordSize,
                                   aNeedKeySlot,
                                   &sPagePtr,
                                   aCTSlotIdx )
              != IDE_SUCCESS );

    if( sPagePtr == NULL )
    {
        /* SegHdr�� XLatch�� �Ǵ�. */
        IDE_TEST( sdpsfSH::fixAndGetSegHdr4Update( aStatistics,
                                                   aSpaceID,
                                                   sSegPID,
                                                   &sSegHdr )
                  != IDE_SUCCESS );
        sState = 1;

        /* Free PID List���� Free Page�� ã�´�. */
        IDE_TEST( sdpsfFreePIDList::walkAndUnlink( aStatistics,
                                                   aMtx,
                                                   aSpaceID,
                                                   sSegHdr,
                                                   aRecordSize,
                                                   aNeedKeySlot,
                                                   &sPagePtr,
                                                   aCTSlotIdx )
                  != IDE_SUCCESS );

        if( sPagePtr == NULL )
        {
            /* Free PID List�� ���ٸ� Pvt, UFmt, Ext List���� ��������
             * �Ҵ�޴´�. */
            IDE_TEST( sdpsfAllocPage::allocNewPage( aStatistics,
                                                    aMtx,
                                                    aSpaceID,
                                                    aSegHandle,
                                                    sSegHdr,
                                                    aPageType,
                                                    &sPagePtr )
                      != IDE_SUCCESS );

            IDE_ASSERT( sPagePtr != NULL );
        }

        sState = 0;
        IDE_TEST( sdpsfSH::releaseSegHdr( aStatistics,
                                          sSegHdr )
                  != IDE_SUCCESS );
    }

    sHintDataPID = sdpPhyPage::getPageID( sPagePtr );

    smLayerCallback::setHintDataPIDofTableInfo( aTableInfo,
                                                sHintDataPID );

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdpsfSH::releaseSegHdr( aStatistics,
                                            sSegHdr )
                    == IDE_SUCCESS );
    }

    smLayerCallback::setHintDataPIDofTableInfo( aTableInfo,
                                                SD_NULL_PID );

    *aPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aTableInfo���� ����Ű�� Hint Page�� �� ������ �ִ��� Check
 *               �Ѵ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aTableInfo   - [IN] TableInfo
 * aRecordSize  - [IN] Record Size
 * aNeedKeySlot - [IN] Insert�� KeySlot�� �ʿ��Ѱ�?
 *
 * aPagePtr     - [OUT] Hint PID�� ����Ű�� �������� Insert�� �����ϸ� Hint
 *                      PID�� ����Ű�� Page Pointer�� ���ϵǰ�, �ƴϸ� Null
 *                      �� Return
 ***********************************************************************/
IDE_RC sdpsfFindPage::checkHintPID4Insert( idvSQL           *aStatistics,
                                           sdrMtx           *aMtx,
                                           scSpaceID         aSpaceID,
                                           void             *aTableInfo,
                                           UInt              aRecordSize,
                                           idBool            aNeedKeySlot,
                                           UChar           **aPagePtr,
                                           UChar            *aCTSlotIdx )
{
    scPageID         sHintDataPID;
    UChar           *sPagePtr      = NULL;;
    SInt             sState        = 0;
    idBool           sCanAllocSlot = ID_TRUE;
    idBool           sRemFrmList   = ID_FALSE;
    sdrMtxStartInfo  sStartInfo;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aRecordSize  < SD_PAGE_SIZE );
    IDE_ASSERT( aRecordSize  > 0 );
    IDE_ASSERT( aPagePtr     != NULL );

    smLayerCallback::getHintDataPIDofTableInfo( aTableInfo,
                                                &sHintDataPID );

    if( sHintDataPID != SD_NULL_PID )
    {
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sHintDataPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              NULL, /* aMtx */
                                              &sPagePtr,
                                              NULL /*aTrySuccess*/,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );
        sState = 1;

        sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

        IDE_TEST( checkSizeAndAllocCTS( aStatistics,
                                        &sStartInfo,
                                        sPagePtr,
                                        aRecordSize,
                                        aNeedKeySlot,
                                        &sCanAllocSlot,
                                        &sRemFrmList,
                                        aCTSlotIdx )
                  != IDE_SUCCESS );

        if( sCanAllocSlot == ID_TRUE )
        {
            IDE_TEST( sdrMiniTrans::pushPage( aMtx,
                                              sPagePtr,
                                              SDB_X_LATCH )
                      != IDE_SUCCESS );
        }
        else
        {
            sState = 0;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                                 sPagePtr )
                      != IDE_SUCCESS );

            sPagePtr = NULL;
        }
    }

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                               sPagePtr )
                    == IDE_SUCCESS );
    }

    *aPagePtr = NULL;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : �������� ��������� Ȯ���ϰ� Row�� ������ �� �ִ��� �˻�
 *
 * aPagePtr      - [IN] Page Pointer
 * aRowSize      - [IN] ������ Row ũ��
 * aCanAllocSlot - [OUT] ������� �Ҵ� ����
 * aCanAllocSlot - [OUT] List�κ��� ���ſ���
 *
 ***********************************************************************/
IDE_RC sdpsfFindPage::checkSizeAndAllocCTS(
                              idvSQL          * aStatistics,
                              sdrMtxStartInfo * aStartInfo,
                              UChar           * aPagePtr,
                              UInt              aRowSize,
                              idBool            aNeedKeySlot,
                              idBool          * aCanAllocSlot,
                              idBool          * aRemFrmList,
                              UChar           * aCTSlotIdx )
{
    UChar            sCTSlotIdx;
    idBool           sCanAllocSlot;
    idBool           sRemFrmList;
    sdpPhyPageHdr  * sPageHdr;
    idBool           sAfterSelfAging;
    sdpSelfAgingFlag sCheckFlag = SDP_SA_FLAG_NOTHING_TO_AGING;

    IDE_ASSERT( aStartInfo    != NULL );
    IDE_ASSERT( aPagePtr      != NULL );
    IDE_ASSERT( aRowSize      > 0 );
    IDE_ASSERT( aCanAllocSlot != NULL );
    IDE_ASSERT( aRemFrmList   != NULL );
    IDE_ASSERT( aCTSlotIdx    != NULL );

    sCanAllocSlot    = ID_FALSE;
    sRemFrmList      = ID_TRUE;
    sCTSlotIdx       = SDP_CTS_IDX_NULL;
    sPageHdr         = (sdpPhyPageHdr*)aPagePtr;
    sAfterSelfAging  = ID_FALSE;

    IDE_TEST_CONT( sdpPhyPage::getState( sPageHdr )
                                != SDPSF_PAGE_USED_INSERTABLE,
                    remove_page_from_list );

    while ( 1 )
    {
        IDE_TEST( smLayerCallback::allocCTSAndSetDirty( aStatistics,
                                                        NULL,        /* aFixMtx */
                                                        aStartInfo,  /* for Logging */
                                                        sPageHdr,    /* in-out param */
                                                        &sCTSlotIdx )
                  != IDE_SUCCESS );

        sCanAllocSlot = sdpPhyPage::canAllocSlot( sPageHdr,
                                                  aRowSize,
                                                  aNeedKeySlot,
                                                  SDP_1BYTE_ALIGN_SIZE );

        if ( ( smLayerCallback::getCountOfCTS( sPageHdr ) > 0 ) &&
             ( sCTSlotIdx == SDP_CTS_IDX_NULL ) )
        {
            sCanAllocSlot = ID_FALSE;
        }

        if ( sCanAllocSlot == ID_TRUE )
        {
            sRemFrmList = ID_FALSE;
            break;
        }
        else
        {
            sCTSlotIdx = SDP_CTS_IDX_NULL;
        }

        if ( sAfterSelfAging == ID_TRUE )
        {
            if ( sCheckFlag != SDP_SA_FLAG_NOTHING_TO_AGING )
            {
                sRemFrmList = ID_FALSE;
            }
            break;
        }
        else
        {
            /* nothing to do */
        }

        IDE_TEST( smLayerCallback::checkAndRunSelfAging( aStatistics,
                                                         aStartInfo,
                                                         sPageHdr,
                                                         &sCheckFlag )
                  != IDE_SUCCESS );

        sAfterSelfAging = ID_TRUE;
    }

    IDE_EXCEPTION_CONT( remove_page_from_list );

    *aCanAllocSlot = sCanAllocSlot;
    *aRemFrmList   = sRemFrmList;
    *aCTSlotIdx    = sCTSlotIdx;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Page�� ��뷮�� ���� �������� ���¸� UPDATE_ONLY��
 *               INSERTABLE�� �����Ѵ�.
 *
 * aStatistics   - [IN] ��� ����
 * aMtx          - [IN] Mini Transaction Pointer
 * aSpaceID      - [IN] TableSpace ID
 * aSegHandle    - [IN] Segment Handle
 * aDataPagePtr  - [IN] insert, update, delete�� �߻��� Data Page
 *
 ***********************************************************************/
IDE_RC sdpsfFindPage::updatePageState( idvSQL           * aStatistics,
                                       sdrMtx           * aMtx,
                                       scSpaceID          aSpaceID,
                                       sdpSegHandle     * aSegHandle,
                                       UChar            * aDataPagePtr )
{
    sdpsfPageState    sPageState;
    SInt              sState   = 0;
    sdpsfSegHdr      *sSegHdr  = NULL;
    sdpPhyPageHdr    *sPageHdr = sdpPhyPage::getHdr( aDataPagePtr );

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aSegHandle   != NULL );
    IDE_ASSERT( aDataPagePtr != NULL );

    if( sdpPhyPage::getPageType( sPageHdr ) == SDP_PAGE_DATA )
    {
        sPageState = (sdpsfPageState)sdpPhyPage::getState( sPageHdr  );

        /* PCTFREE�� PCTUSED�� �����ؾ� �Ѵ�. */
        switch( sPageState )
        {
            case SDPSF_PAGE_USED_INSERTABLE:
                /* Insertable�̴ٰ� insertRow�� updateRow�� ���ؼ�
                 * �������� ���°� Update Only�� �ٲ�� �ֱ⶧����
                 * Check�Ѵ�. */
                if( isPageUpdateOnly( sPageHdr,
                                      aSegHandle->mSegAttr.mPctFree )
                    == ID_TRUE )
                {
                    /* Page���¸� Update Only�� �ٲ۴�. */
                    IDE_TEST( sdpPhyPage::setState(
                                    sPageHdr,
                                    (UShort)SDPSF_PAGE_USED_UPDATE_ONLY,
                                    aMtx ) != IDE_SUCCESS );
                }
                break;

            case SDPSF_PAGE_USED_UPDATE_ONLY:
                /* Update Only���ٰ� FreeRow�� ���ؼ� ���°� ����� �� �ֱ� ������ �̰���
                 * Check�Ѵ�. */
                if( isPageInsertable( sPageHdr,
                                      aSegHandle->mSegAttr.mPctUsed )
                    == ID_TRUE )
                {
                    IDE_TEST( sdpPhyPage::setState(
                                    sPageHdr,
                                    (UShort)SDPSF_PAGE_USED_INSERTABLE,
                                    aMtx ) != IDE_SUCCESS );

                    if( sPageHdr->mLinkState == SDP_PAGE_LIST_UNLINK )
                    {
                        /* ���� Free PID List�� ����� �Ǿ� ���� �ʴٸ� Add��Ų��. */
                        IDE_TEST( sdpsfAllocPage::addPageToFreeList( aStatistics,
                                                                     aMtx,
                                                                     aSpaceID,
                                                                     aSegHandle->mSegPID,
                                                                     aDataPagePtr )
                                  != IDE_SUCCESS );
                    }
                }
                break;

                /* �̻��·� ���ü� ����. */
            case SDPSF_PAGE_FREE:
            default:
                IDE_ASSERT(0);
                break;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdpsfSH::releaseSegHdr( aStatistics,
                                            sSegHdr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}
