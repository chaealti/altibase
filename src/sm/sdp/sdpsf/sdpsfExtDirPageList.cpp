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
#include <iduLatch.h>
#include <sdd.h>
#include <sdb.h>
#include <sdr.h>
#include <smu.h>
#include <smr.h>
#include <sct.h>
#include <sdpDef.h>
#include <sdpSglRIDList.h>
#include <sdpPhyPage.h>
#include <sdpReq.h>
#include <smErrorCode.h>
#include <sdpsfSH.h>
#include <sdptbExtent.h>
#include <sdpsfExtMgr.h>

#include <sdpsfExtDirPageList.h>

/***********************************************************************
 * Description : Segment�� ExtDIRPage PID List�� �ʱ�ȭ �Ѵ�.
 *
 * aMtx    - [IN] Mini Transaction Pointer
 * aSegHdr - [IN] Segment Header
 *
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::initialize( sdrMtx      * aMtx,
                                        sdpsfSegHdr * aSegHdr )
{
    IDE_ASSERT( aMtx    != NULL );
    IDE_ASSERT( aSegHdr != 0 );

    IDE_TEST( sdpDblPIDList::initBaseNode( &aSegHdr->mExtDirPIDList,
                                           aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : ���ο� ExtDirPage�� �Ҵ��Ѵ�.
 *
 *  1. ���ο� ExtDirPage�� ���ؼ� Extent�Ҵ��� �ʿ����� �˻��Ѵ�.
 *     �Ҵ��� �ʿ��ϸ� �Ҵ��Ѵ�.
 *
 *    - ExtDirPage�� �Ҵ�� Extent������ �Ҵ��Ѵ�. ������ ����
 *     ������ ExtDirPage�� ���� Extent�� Free Page�� ������ �����Ѵ�.
 *     ���ٸ� ���ο� Extent�� �Ҵ��ؾ� �Ѵ�.
 *
 *  2. ���ο� ExtDirPage�� PageNo�� ExtDirPID�� �����Ѵ�.
 *    - ���ο� Extent�� �Ҵ�Ǿ��ٸ� �� Extent�� ù��° PID
 *    - ���� Extent�� ������ �־��ٸ� �������� �����  ExtDirPageID + 1
 *
 *  3. ExtDirPID�� �ش��ϴ� �������� Create�ϰ� �ʱ�ȭ �Ѵ�.
 *
 * Caution:
 *  1. aTbsHdr�� �ִ� �������� XLatch�� �ɷ� �־�� �Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header
 * aExtDirPagePtr - [IN] Extent Directory Page Ptr
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::addNewExtDirPage2Tail( idvSQL              * aStatistics,
                                                   sdrMtx              * aMtx,
                                                   scSpaceID             aSpaceID,
                                                   sdpsfSegHdr         * aSegHdr,
                                                   UChar               * aExtDirPagePtr )
{
    sdpsfExtDirCntlHdr  * sExtDirCntlHdr;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirPagePtr != NULL );

    sExtDirCntlHdr = sdpsfExtDirPage::getExtDirCntlHdr( aExtDirPagePtr );

    IDE_TEST( sdpsfExtDirPage::initialize(
                  aMtx,
                  sExtDirCntlHdr,
                  aSegHdr->mMaxExtCntInExtDirPage )
              != IDE_SUCCESS );

    /* �Ҵ�� ExtDirPage�� Ext Dir PID List�� �߰��Ѵ�. */
    IDE_TEST( addPage2Tail( aStatistics,
                            aMtx,
                            aSegHdr,
                            sExtDirCntlHdr ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ӵǾ� �ִ� aAddExtDescCnt��ŭ ���ӵ� Extent���� ExtDir
 *               Page�� �߰��Ѵ�.
 *
 * aStatistics        - [IN] �������
 * aMtx               - [IN] Mini Transaction Pointer.
 * aSpaceID           - [IN] Table Space ID
 * aSegHdr            - [IN] Segment Header
 * aFstExtPID         - [IN] Add�� Extent�鿡 ���� �ִ� ù��° PID
 *
 * aExtDirCntlHdr     - [OUT] Extent DirPage Control Header
 * aNewExtDescRID     - [OUT] ���ο� Extent RID
 * aNewExtDescPtr     - [OUT] ���ο� Extent Desc Pointer
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::addExtDesc( idvSQL              * aStatistics,
                                        sdrMtx              * aMtx,
                                        scSpaceID             aSpaceID,
                                        sdpsfSegHdr         * aSegHdr,
                                        scPageID              aFstExtPID,
                                        sdpsfExtDirCntlHdr ** aExtDirCntlHdr,
                                        sdRID               * aNewExtDescRID,
                                        sdpsfExtDesc       ** aNewExtDescPtr )
{
    sdpsfExtDirCntlHdr *sExtDirHdr;
    scPageID            sExtDirPID;
    UChar              *sExtDirPagePtr;
    sdpsfExtDesc       *sNewExtDescPtr;
    UInt                sFlag;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aFstExtPID     != SD_NULL_PID );
    IDE_ASSERT( aExtDirCntlHdr != NULL );
    IDE_ASSERT( aNewExtDescRID != NULL );
    IDE_ASSERT( aNewExtDescPtr != NULL );

    /* ���� �Ҵ�� ExtDirPage�� ExtDirPageList�� �߰��Ѵ�. */
    if( isNeedNewExtDirPage( aSegHdr ) == ID_TRUE )
    {
        sExtDirPID = aFstExtPID;
        sFlag      = SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_TRUE;

        IDE_TEST( sdpsfExtDirPage::create( aStatistics,
                                           aMtx,
                                           aSpaceID,
                                           sExtDirPID,
                                           &sExtDirPagePtr,
                                           &sExtDirHdr )
                  != IDE_SUCCESS );

        IDE_TEST( addNewExtDirPage2Tail( aStatistics,
                                         aMtx,
                                         aSpaceID,
                                         aSegHdr,
                                         sExtDirPagePtr )
              != IDE_SUCCESS );

        IDE_TEST( sdpsfSH::setFmtPageCnt( aMtx,
                                          aSegHdr,
                                          aSegHdr->mFmtPageCnt + 1 )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( getLstExtDirPage4Update( aStatistics,
                                           aMtx,
                                           aSpaceID,
                                           aSegHdr,
                                           &sExtDirPagePtr,
                                           &sExtDirHdr )
                  != IDE_SUCCESS );

        sFlag = SDP_SF_EXTDESC_FST_IS_EXTDIRPAGE_FALSE;
    }

    /* Fix�� ExtDirPage ���� Add�� �õ��Ѵ�. */
    IDE_TEST( sdpsfExtDirPage::addNewExtDescAtLst(
                  aMtx,
                  sExtDirHdr,
                  aFstExtPID,
                  sFlag,
                  &sNewExtDescPtr )
              != IDE_SUCCESS );

    IDE_ASSERT( sNewExtDescPtr != NULL );

    /* Extent�� ������ ���� */
    IDE_TEST( sdpsfSH::setTotExtCnt( aMtx,
                                     aSegHdr,
                                     aSegHdr->mTotExtCnt + 1 )
              != IDE_SUCCESS );

    *aNewExtDescRID = sdpPhyPage::getRIDFromPtr( sNewExtDescPtr );
    *aNewExtDescPtr = sNewExtDescPtr;
    *aExtDirCntlHdr = sExtDirHdr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aNewExtDescRID = SD_NULL_RID;
    *aNewExtDescPtr = NULL;
    *aExtDirCntlHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtDirCntlHdr�� ExtDirPID List ���� �߰��Ѵ�.
 *
 * Caution:
 *   1. aTbsHdr�� ���� �������� XLatch�� �ɷ� �־�� �Ѵ�.
 *   2. aExtDirCntlHdr�� ���� �������� XLath�� �ɷ� �־� �Ѵ�.
 *
 * aStatistics       - [IN] �������
 * aMtx              - [IN] Mini Transaction Pointer.
 * aSegHdr           - [IN] Segment Hdr
 * aNewExtDirCntlHdr - [IN] XLatch�� �ɸ� ExtDirCntl Header
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::addPage2Tail( idvSQL               * aStatistics,
                                          sdrMtx               * aMtx,
                                          sdpsfSegHdr          * aSegHdr,
                                          sdpsfExtDirCntlHdr   * aNewExtDirCntlHdr )
{
    sdpPhyPageHdr       * sPageHdr;
    UChar               * sPagePtr;
    sdpDblPIDListBase   * sExtDirPIDList;

    IDE_ASSERT( aMtx               != NULL );
    IDE_ASSERT( aSegHdr            != NULL );
    IDE_ASSERT( aNewExtDirCntlHdr  != NULL );

    sExtDirPIDList  = &aSegHdr->mExtDirPIDList;
    sPagePtr        = sdpPhyPage::getPageStartPtr( aNewExtDirCntlHdr );
    sPageHdr        = sdpPhyPage::getHdr( sPagePtr );

    IDE_TEST( sdpDblPIDList::insertTailNode( aStatistics,
                                             sExtDirPIDList,
                                             sdpPhyPage::getDblPIDListNode( sPageHdr ),
                                             aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDirPageList���� aExtDirCntlHdr�� ����Ű�� ��������
 *               �����Ѵ�.
 *
 * aStatistics       - [IN] �������
 * aMtx              - [IN] Mini Transaction Pointer.
 * aSegHdr           - [IN] Segment Hdr
 * aExtDirCntlHdr    - [IN] ���ŵ� �������� ExtDir Control Header
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::unlinkPage( idvSQL               * aStatistics,
                                        sdrMtx               * aMtx,
                                        sdpsfSegHdr          * aSegHdr,
                                        sdpsfExtDirCntlHdr   * aExtDirCntlHdr )
{
    sdpPhyPageHdr * sExtDirPhyPageHdr;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );

    sExtDirPhyPageHdr = sdpPhyPage::getHdr( (UChar*)aExtDirCntlHdr );

    IDE_TEST( sdpDblPIDList::removeNode( aStatistics,
                                         &aSegHdr->mExtDirPIDList,
                                         sdpPhyPage::getDblPIDListNode( sExtDirPhyPageHdr ),
                                         aMtx )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aSegHdr�� ������ ExtDirPage�� Buffer�� Fix�ϰ� XLatch��
 *               ��Ƽ� �Ѱ��ش�. aSegHdr�� ExtDirCntlHdr�� �־ ����
 *               �� aSegHdr���� �����Ѵٸ� SegHdr �������� ExtPageCntlHdr
 *               �� �Ѱ��ش�. aSegHdr�� �̹� XLatch�� �����ִ� �����̱�
 *               ������ SegDirty�� ���ָ� �ȴ�.
 *
 * aStatistics       - [IN] �������
 * aMtx              - [IN] Mini Transaction Pointer.
 * aSpaceID          - [IN] Space ID
 * aSegHdr           - [IN] Segment Header
 * aExtDirPagePtr    - [IN] Extent DirPage Ptr
 * aExtDirCntlHdr    - [IN] aExtDirPagePtr�� Extent Dir Control Header
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::getLstExtDirPage4Update( idvSQL               * aStatistics,
                                                     sdrMtx               * aMtx,
                                                     scSpaceID              aSpaceID,
                                                     sdpsfSegHdr          * aSegHdr,
                                                     UChar               ** aExtDirPagePtr,
                                                     sdpsfExtDirCntlHdr  ** aExtDirCntlHdr )
{
    sdpsfExtDirCntlHdr  * sExtDirHdr;
    UChar               * sExtDirPagePtr;
    scPageID              sLstExtDirPID;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSpaceID       != 0 )
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirPagePtr != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );

    sLstExtDirPID = sdpDblPIDList::getListTailNode( &aSegHdr->mExtDirPIDList );

    if( sLstExtDirPID != aSegHdr->mSegHdrPID )
    {
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sLstExtDirPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sExtDirPagePtr,
                                              NULL /*aTrySuccess*/,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        IDE_ASSERT( sExtDirPagePtr != NULL );

        sExtDirHdr = sdpsfExtDirPage::getExtDirCntlHdr( sExtDirPagePtr );
    }
    else
    {
        sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );
        sExtDirHdr     = &aSegHdr->mExtDirCntlHdr;

        IDE_TEST( sdrMiniTrans::setDirtyPage( aMtx,
                                              sExtDirPagePtr )
                  != IDE_SUCCESS );
    }

    *aExtDirCntlHdr = sExtDirHdr;
    *aExtDirPagePtr = sExtDirPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aExtDirCntlHdr = NULL;
    *aExtDirPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aSegHdr�� ExtDirPageList�� ���� ������ Dump�Ѵ�.
 *
 * aSegHdr - [IN] Segment Header
 ***********************************************************************/
IDE_RC sdpsfExtDirPageList::dump( sdpsfSegHdr * aSegHdr )
{
    sdpDblPIDListBase * sExtDirPIDList;

    IDE_ASSERT( aSegHdr != NULL );

    sExtDirPIDList  = &aSegHdr->mExtDirPIDList;;

    ideLog::log( IDE_SERVER_0, 
                 " Add Page: %u,"
                 " %u,"
                 " %u",
                 sdpDblPIDList::getNodeCnt( sExtDirPIDList ),
                 sExtDirPIDList->mBase.mPrev,
                 sExtDirPIDList->mBase.mNext );

    return IDE_SUCCESS;
}
