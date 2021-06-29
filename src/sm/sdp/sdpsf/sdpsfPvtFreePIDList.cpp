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
 **********************************************************************/

# include <smErrorCode.h>
# include <sdr.h>
# include <sdpReq.h>

# include <sdpsfDef.h>
# include <sdpsfSH.h>
# include <sdpsfVerifyAndDump.h>
# include <sdpSglPIDList.h>
# include <sdpTableSpace.h>
# include <sdpsfPvtFreePIDList.h>

IDE_RC sdpsfPvtFreePIDList::initialize()
{
    return IDE_SUCCESS;
}

IDE_RC sdpsfPvtFreePIDList::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : aPagePtr�� Private Page List�� Head�� �߰��Ѵ�.
 *
 * Caution:
 *  1. aSegHdr�� XLatch�� �ɷ� �־�� �Ѵ�.
 *  2. aPagePtr�� XLatch�� �ɷ� �־�� �Ѵ�.
 *
 * aSegHdr    - [IN] Segment Hdr
 * aPagePtr   - [IN] Page Ptr
 * aMtx       - [IN] Mini Transaction Pointer
 *
 ***********************************************************************/
IDE_RC sdpsfPvtFreePIDList::add2Head( sdrMtx             * aMtx,
                                      sdpsfSegHdr        * aSegHdr,
                                      UChar              * aPagePtr )
{
    sdpSglPIDListBase *sPvtFreePIDList;
    sdpPhyPageHdr     *sPageHdr;

    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSegHdr      != NULL );
    IDE_ASSERT( aPagePtr     != NULL );

    sPvtFreePIDList = sdpsfSH::getPvtFreePIDList( aSegHdr );

    sPageHdr = sdpPhyPage::getHdr( aPagePtr );

    IDE_TEST( sdpPhyPage::setLinkState( sPageHdr,
                                        SDP_PAGE_LIST_LINK,
                                        aMtx )
              != IDE_SUCCESS );

    IDE_TEST( sdpSglPIDList::addNode2Head( sPvtFreePIDList,
                                           sdpPhyPage::getSglPIDListNode( sPageHdr ),
                                           aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Private Free Page List�� ù��° �������� �����Ѵ�.
 *
 * Caution:
 *  1. aSegHdr�� XLatch�� �ɷ� �־�� �Ѵ�.
 *  2. aPagePtr�� XLatch�� �ɷ� �־�� �Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aRmvMtx        - [IN] Head�������� PageList���� ������ Mini Transaction Pointer
 * aMtx           - [IN] Head�������� ���ۿ� Fix�ϰ� Latch�� ȹ���ϴ�
 *                       Mini Transaction Pointer
 * aSpaceID       - [IN] SpaceID
 * aSegHdr        - [IN] Segment Hdr
 * aPageType      - [IN] Page Type
 *
 * aAllocPID      - [OUT] Head Page ID 
 * aPagePtr       - [OUT] Head Page Pointer�μ� ���ϵɶ� XLatch�� �ɷ��ִ�
 *
 ***********************************************************************/
IDE_RC sdpsfPvtFreePIDList::removeAtHead( idvSQL             * aStatistics,
                                          sdrMtx             * aRmvMtx,
                                          sdrMtx             * aMtx,
                                          scSpaceID            aSpaceID,
                                          sdpsfSegHdr        * aSegHdr,
                                          sdpPageType          aPageType,
                                          scPageID           * aAllocPID,
                                          UChar             ** aPagePtr )
{
    scPageID           sRmvPageID;
    sdpSglPIDListNode *sRmvPageNode;
    sdpSglPIDListBase *sPvtFreePIDList;
    UChar             *sSegPagePtr;
    sdpPhyPageHdr     *sPageHdr;

    IDE_ASSERT( aRmvMtx      != NULL );
    IDE_ASSERT( aMtx         != NULL );
    IDE_ASSERT( aSpaceID     != 0 );
    IDE_ASSERT( aSegHdr      != NULL );
    IDE_ASSERT( aPageType    <  SDP_PAGE_TYPE_MAX );
    IDE_ASSERT( aAllocPID    != NULL );
    IDE_ASSERT( aPagePtr     != NULL );

    *aPagePtr  = NULL;
    *aAllocPID = SD_NULL_PID;

    sPvtFreePIDList = sdpsfSH::getPvtFreePIDList( aSegHdr );

    if( sdpSglPIDList::getNodeCnt( sPvtFreePIDList )  != 0 )
    {
        sRmvPageID = getFstPageID( aSegHdr );

        IDE_ASSERT( sRmvPageID != SD_NULL_PID );

        /* ����Ʈ���� ���ſ����� sRmvMtx�� ������ ���ŵ� ������
         * �� ���� create������ ���� �Լ����� �Ѿ�� aMtx�� �ϰ�
         * �Ͽ� ���� �Լ����� �ش� �������� ���ؼ� ������ get��������
         * �߻����� �ʵ��� �Ѵ�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sRmvPageID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              aPagePtr,
                                              NULL /*aTrySuccess*/,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSglPIDList::removeNodeAtHead( sPvtFreePIDList,
                                                   *aPagePtr,
                                                   aRmvMtx,
                                                   &sRmvPageID,
                                                   &sRmvPageNode )
                  != IDE_SUCCESS );

        sPageHdr = (sdpPhyPageHdr*)*aPagePtr;

        /* sdpPhyPage::initialize���� Page List Node�� �ʱ�ȭ �ϱ⶧����
         * ���� PIDList���� �����Ŀ� Physical Page�� �ʱ�ȭ �ؾ��Ѵ�. */
        IDE_TEST( sdpPhyPage::logAndInit( sPageHdr,
                                          sRmvPageID,
                                          NULL, /* Parent Info */
                                          SDPSF_PAGE_USED_INSERTABLE,
                                          aPageType,
                                          SM_NULL_OID,
                                          SM_NULL_INDEX_ID,
                                          aRmvMtx )
                  != IDE_SUCCESS );

        IDE_TEST( sdpPhyPage::setLinkState( (sdpPhyPageHdr*)*aPagePtr,
                                            SDP_PAGE_LIST_UNLINK,
                                            aRmvMtx )
                  != IDE_SUCCESS );

        sSegPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

        /* Segment Header Page�� Dirty */
        IDE_TEST( sdrMiniTrans::setDirtyPage( aRmvMtx,
                                              sSegPagePtr )
                  != IDE_SUCCESS );

        /* ����Ʈ���� ���ŵ� Page�� Dirty�� ��� */
        IDE_TEST( sdrMiniTrans::setDirtyPage( aRmvMtx,
                                              *aPagePtr )
                  != IDE_SUCCESS );

        *aAllocPID = sRmvPageID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr  = NULL;
    *aAllocPID = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Private Free Page List�� ù��° �������� return�Ѵ�.
 *
 * aSegHdr - [IN] Segment Header
 ***********************************************************************/
scPageID sdpsfPvtFreePIDList::getFstPageID( sdpsfSegHdr  * aSegHdr )
{
    sdpSglPIDListBase *sPvtFreePIDList;

    IDE_ASSERT( aSegHdr != NULL );

    sPvtFreePIDList = sdpsfSH::getPvtFreePIDList( aSegHdr );

    return sdpSglPIDList::getHeadOfList( sPvtFreePIDList );
}


