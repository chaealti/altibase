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
 *
 * Description :
 *
 * �� ������ extent directory page�� ���� ���� �����̴�.
 *
 **********************************************************************/

#include <sdr.h>
#include <sdpDef.h>
#include <sdpsfExtDirPage.h>
#include <sdpsfExtent.h>
#include <sdpsfSH.h>
//XXX sdptb�� �ٷ� �����ϸ� �ȵ˴ϴ�.
#include <sdptbExtent.h>
#include <smErrorCode.h>

/***********************************************************************
 * Description: ExtDirPage�� �ʱ�ȭ�Ѵ�.
 *
 * aMtx           - [IN] Min Transaction Pointer
 * aExtDirCntlHdr - [IN] ExtDirCntlHdr
 * aMaxExtDescCnt - [IN] �ִ� Extent ����
 **********************************************************************/
IDE_RC sdpsfExtDirPage::initialize( sdrMtx*              aMtx,
                                    sdpsfExtDirCntlHdr*  aExtDirCntlHdr,
                                    UShort               aMaxExtDescCnt )
{
    scOffset sFstExtDescOffset;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );
    IDE_ASSERT( aMaxExtDescCnt != 0 );

    IDE_TEST( setExtDescCnt( aMtx,
                             aExtDirCntlHdr,
                             0 )
              != IDE_SUCCESS );

    IDE_TEST( setMaxExtDescCnt( aMtx,
                                aExtDirCntlHdr,
                                aMaxExtDescCnt ) != IDE_SUCCESS );

    sFstExtDescOffset = sdpPhyPage::getOffsetFromPagePtr( (UChar*)aExtDirCntlHdr ) +
        ID_SIZEOF( sdpsfExtDirCntlHdr ) ;

    IDE_TEST( setFstExtDescOffset( aMtx,
                                   aExtDirCntlHdr,
                                   sFstExtDescOffset )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description :
 **********************************************************************/
IDE_RC sdpsfExtDirPage::destroy()
{
    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : ExtDirPage�� �����Ѵ�. ������ ExtDirPage�� Pointer��
 *               Header�� �����ش�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aPageID        - [IN] ExtDirPage ID
 *
 * aPagePtr       - [OUT] Create�� ExtDirPage Pointer�� �����ȴ�.
 * aRetExtDirHdr  - [OUT] ExtDirPage�� ExtDirCntlHdr�� �����ȴ�.
 **********************************************************************/
IDE_RC sdpsfExtDirPage::create( idvSQL              * aStatistics,
                                sdrMtx              * aMtx,
                                scSpaceID             aSpaceID,
                                scPageID              aPageID,
                                UChar              ** aPagePtr,
                                sdpsfExtDirCntlHdr ** aRetExtDirHdr )
{
    sdpsfExtDirCntlHdr * sExtDirHdr;
    UChar              * sPagePtr;

    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aSpaceID      != 0 );
    IDE_ASSERT( aPageID       != SD_NULL_PID );
    IDE_ASSERT( aPagePtr      != NULL );
    IDE_ASSERT( aRetExtDirHdr != NULL );

    IDE_TEST( sdpPhyPage::create( aStatistics,
                                  aSpaceID,
                                  aPageID,
                                  NULL,    /* Parent Info */
                                  0,       /* Page State */
                                  SDP_PAGE_FMS_EXTDIR,
                                  SM_NULL_OID, // SegMeta
                                  SM_NULL_INDEX_ID,
                                  aMtx,    /* Create Mtx */
                                  aMtx,    /* Page Init Mtx */
                                  &sPagePtr )
              != IDE_SUCCESS );

    sExtDirHdr = sdpsfExtDirPage::getExtDirCntlHdr( sPagePtr );

    IDE_TEST( initialize( aMtx,
                          sExtDirHdr,
                          getMaxExtDescCntInExtDirPage() )
              != IDE_SUCCESS );

    *aPagePtr      = sPagePtr;
    *aRetExtDirHdr = sExtDirHdr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr      = NULL;
    *aRetExtDirHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : <aSpaceID, aPageID>�� �ش��ϴ� Page�� ���ؼ� XLatch��
 *               ��� �������� �����Ϳ� ExtDirCntlHeader�� �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aPageID        - [IN] ExtDirPage ID
 *
 * aPagePtr       - [OUT] ExtDirPage Pointer�� �����ȴ�.
 * aRetExtDirHdr  - [OUT] ExtDirPage�� ExtDirCntlHdr�� �����ȴ�.
 **********************************************************************/
IDE_RC sdpsfExtDirPage::getPage4Update( idvSQL              * aStatistics,
                                        sdrMtx              * aMtx,
                                        scSpaceID             aSpaceID,
                                        scPageID              aPageID,
                                        UChar              ** aPagePtr,
                                        sdpsfExtDirCntlHdr ** aRetExtDirHdr )
{
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aSpaceID      != 0 );
    IDE_ASSERT( aPageID       != SD_NULL_PID );
    IDE_ASSERT( aPagePtr      != NULL );
    IDE_ASSERT( aRetExtDirHdr != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aPageID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          aPagePtr,
                                          NULL /*aTrySuccess*/,
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );

    *aRetExtDirHdr = sdpsfExtDirPage::getExtDirCntlHdr( *aPagePtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr      = NULL;
    *aRetExtDirHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : <aSpaceID, aPageID>�� �ش��ϴ� Page�� Buffer�� Fix�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] TableSpace ID
 * aPageID        - [IN] ExtDirPage ID
 *
 * aPagePtr       - [OUT] ExtDirPage Pointer�� �����ȴ�.
 * aRetExtDirHdr  - [OUT] ExtDirPage�� ExtDirCntlHdr�� �����ȴ�.
 **********************************************************************/
IDE_RC sdpsfExtDirPage::fixPage( idvSQL              * aStatistics,
                                 scSpaceID             aSpaceID,
                                 scPageID              aPageID,
                                 UChar              ** aPagePtr,
                                 sdpsfExtDirCntlHdr ** aRetExtDirHdr )
{
    IDE_ASSERT( aSpaceID      != 0 );
    IDE_ASSERT( aPageID       != SD_NULL_PID );
    IDE_ASSERT( aPagePtr      != NULL );
    IDE_ASSERT( aRetExtDirHdr != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aPageID,
                                          aPagePtr )
              != IDE_SUCCESS );

    *aRetExtDirHdr = sdpsfExtDirPage::getExtDirCntlHdr( *aPagePtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr      = NULL;
    *aRetExtDirHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� ExtDesc�� �� aExtDirHdr�� ���� �߰��Ѵ�. ���� ����
 *               �� ���ٸ� aNewExtDesc�� NULL�� �����ϰ� �ִٸ� �߰���
 *               ExtDesc�� �����ּҸ� �Ѱ��ش�.
 *
 * aMtx           - [IN] Mini Transaction Pointer
 * aExtDirHdr     - [IN] ExtDirPage Header
 * aFstPID        - [IN] ���ο� Extent�� ù��° ������
 * aFlag          - [IN] ExtDesc�� Flag ����.
 *
 * aNewExtDesc    - [OUT] If ������ ���ٸ� NULL, else ���� Add�� Extent Desc
 *                        �� ���� �ּ�.
 **********************************************************************/
IDE_RC sdpsfExtDirPage::addNewExtDescAtLst( sdrMtx             * aMtx,
                                            sdpsfExtDirCntlHdr * aExtDirHdr,
                                            scPageID             aFstPID,
                                            UInt                 aFlag,
                                            sdpsfExtDesc      ** aNewExtDesc )
{
    UShort        sExtCnt;
    sdpsfExtDesc *sNewExtDesc;

    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aExtDirHdr    != NULL );
    IDE_ASSERT( aFstPID       != SD_NULL_PID );
    IDE_ASSERT( aNewExtDesc   != NULL );

    if( isFull( aExtDirHdr ) == ID_FALSE )
    {
        sExtCnt = aExtDirHdr->mExtDescCnt;

        /* ���ο� Extent�� �߰��Ǿ��⶧���� ExtDesc������ 1����
         * �����ش�. */
        IDE_TEST( setExtDescCnt( aMtx,
                                 aExtDirHdr,
                                 sExtCnt + 1 )
                  != IDE_SUCCESS );

        sNewExtDesc  = getLstExtDesc( aExtDirHdr );

        /* ���ο� Extent�� ���� �������� �����Ѵ�. */
        IDE_TEST( sdpsfExtent::setFirstPID( sNewExtDesc,
                                            aFstPID,
                                            aMtx )
                  != IDE_SUCCESS );

        IDE_TEST( sdpsfExtent::setFlag( sNewExtDesc,
                                        aFlag,
                                        aMtx )
                  != IDE_SUCCESS );

        *aNewExtDesc = sNewExtDesc;
    }
    else
    {
        /* ������ ����. */
        *aNewExtDesc = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aNewExtDesc = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment���� aCurExtRID ������ �����ϴ� Extent RID��
 *               ExtDesc�� ���Ѵ�.
 *
 * aExtDirHdr     - [IN] Extent Directory Page Pointer
 * aCurExtRID     - [IN] ���� Extent RID
 * aNxtExtRID     - [IN] ���� Extent RID
 * aExtDesc       - [OUT] ���� ExtDesc �� ����� ����
 **********************************************************************/
IDE_RC sdpsfExtDirPage::getNxtExt( sdpsfExtDirCntlHdr * aExtDirHdr,
                                   sdRID                aCurExtRID,
                                   sdRID              * aNxtExtRID,
                                   sdpsfExtDesc       * aExtDesc )
{
    sdRID sLstExtRID;

    IDE_ASSERT( aExtDirHdr  != NULL );
    IDE_ASSERT( aCurExtRID  != SD_NULL_RID );
    IDE_ASSERT( aNxtExtRID  != NULL );
    IDE_ASSERT( aExtDesc    != NULL );

    sLstExtRID = getLstExtRID( aExtDirHdr );

    IDE_ASSERT( aCurExtRID <= sLstExtRID );

    if( sLstExtRID == aCurExtRID )
    {
        sdpsfExtent::init( aExtDesc );

        *aNxtExtRID = SD_NULL_RID;
    }
    else
    {
        *aNxtExtRID = aCurExtRID + ID_SIZEOF( sdpsfExtDesc );
        *aExtDesc   = *getExtDesc( aExtDirHdr, *aNxtExtRID );
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : aExtDirPID�� ����Ű�� ���������� OUT������ ������ ����
 *               �´�. aExtDirPID�� ����Ű�� �������� Extent Directory
 *               Page�̾�� �Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] TableSpace ID
 * aExtDirPID     - [IN] Extent Directory PageID
 *
 * aExtCnt        - [OUT] aExtDirPID�� ����Ű�� �������� �����ϴ� Extent
 *                        ����
 * aFstExtRID     - [OUT] ù��° Extent RID
 * aFstExtDescPtr - [OUT] ù���� Extent Desc�� ����� ����
 * aLstExtRID     - [OUT] Extent Directory Page���� ������ Extent RID
 * aNxtExtDirPID  - [OUT] aExtDirPID�� ���� ExtDirPageID
 **********************************************************************/
IDE_RC sdpsfExtDirPage::getPageInfo( idvSQL        * aStatistics,
                                     scSpaceID       aSpaceID,
                                     scPageID        aExtDirPID,
                                     UShort        * aExtCnt,
                                     sdRID         * aFstExtRID,
                                     sdpsfExtDesc  * aFstExtDescPtr,
                                     sdRID         * aLstExtRID,
                                     scPageID      * aNxtExtDirPID )
{
    sdpsfExtDesc       * sFstExtDescPtr;
    UChar              * sExtDirPagePtr;
    sdpsfExtDirCntlHdr * sExtDirCntlHdr;
    sdpPhyPageHdr      * sPhyHdrOfExtPage;
    SInt                 sState = 0;

    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aExtDirPID     != SD_NULL_PID );
    IDE_ASSERT( aExtCnt        != NULL );
    IDE_ASSERT( aFstExtRID     != NULL );
    IDE_ASSERT( aFstExtDescPtr != NULL );
    IDE_ASSERT( aLstExtRID     != NULL );
    IDE_ASSERT( aNxtExtDirPID  != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aExtDirPID,
                                          (UChar**)&sExtDirPagePtr )
              != IDE_SUCCESS );

    IDE_ASSERT( sExtDirPagePtr != NULL );

    sPhyHdrOfExtPage = sdpPhyPage::getHdr( sExtDirPagePtr );

    IDE_ASSERT( sdpPhyPage::getPageType( sPhyHdrOfExtPage )
                == SDP_PAGE_FMS_EXTDIR );

    sExtDirCntlHdr   = getExtDirCntlHdr( sExtDirPagePtr );
    sFstExtDescPtr   = getFstExtDesc( sExtDirCntlHdr );

    if( sExtDirCntlHdr->mExtDescCnt != 0 )
    {
        *aFstExtRID = SD_MAKE_RID( aExtDirPID, getFstExtOffset() );
        *aLstExtRID = SD_MAKE_RID( aExtDirPID, getFstExtOffset() +
                                   ( sExtDirCntlHdr->mExtDescCnt - 1 ) * ID_SIZEOF( sdpsfExtDesc ) );
    }
    else
    {
        *aFstExtRID = SD_NULL_RID;
        *aLstExtRID = SD_NULL_RID;
    }

    *aFstExtDescPtr = *sFstExtDescPtr;
    *aNxtExtDirPID  = sdpPhyPage::getNxtPIDOfDblList( sPhyHdrOfExtPage );
    *aExtCnt        = sExtDirCntlHdr->mExtDescCnt;

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       sExtDirPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sExtDirPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtDirCntlHdr�� ����Ű�� �������� ������ ExtDesc�� ����
 *               �Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aMtx           - [IN] Mini Transaction Pointer
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header
 * aExtDirHdr     - [IN] Extent Direct Control Header
 **********************************************************************/
IDE_RC sdpsfExtDirPage::freeLstExt( idvSQL             * aStatistics,
                                    sdrMtx             * aMtx,
                                    scSpaceID            aSpaceID,
                                    sdpsfSegHdr        * aSegHdr,
                                    sdpsfExtDirCntlHdr * aExtDirCntlHdr )
{
    sdpsfExtDesc     * sLstExtDesc;
    UInt               sFreeExtCnt;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );

    sLstExtDesc = getLstExtDesc( aExtDirCntlHdr );

    IDE_ASSERT( sLstExtDesc->mFstPID != 0 );

    IDE_TEST( sdptbExtent::freeExt( aStatistics,
                                    aMtx,
                                    aSpaceID,
                                    sLstExtDesc->mFstPID,
                                    &sFreeExtCnt )
              != IDE_SUCCESS );

    IDE_TEST( setExtDescCnt( aMtx,
                             aExtDirCntlHdr,
                             aExtDirCntlHdr->mExtDescCnt - 1 )
              != IDE_SUCCESS );

    IDE_TEST( sdpsfSH::setTotExtCnt( aMtx,
                                     aSegHdr,
                                     aSegHdr->mTotExtCnt - 1 )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtDirCntlHdr�� ����Ű�� ���������� ù��° Extent
 *               �����ϰ� ��� Extent�� free�Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aStartInfo     - [IN] Mini Transaction Start Info
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header Pointer
 * aExtDirCntlHdr - [IN] Extent Direct Control Header
 **********************************************************************/
IDE_RC sdpsfExtDirPage::freeAllExtExceptFst( idvSQL             * aStatistics,
                                             sdrMtxStartInfo    * aStartInfo,
                                             scSpaceID            aSpaceID,
                                             sdpsfSegHdr        * aSegHdr,
                                             sdpsfExtDirCntlHdr * aExtDirCntlHdr )
{
    UShort             sExtDescCnt;
    sdrMtx             sFreeMtx;
    SInt               sState = 0;
    UChar            * sExtDirPagePtr;
    UChar            * sSegHdrPagePtr;

    IDE_ASSERT( aStartInfo     != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );

    sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aExtDirCntlHdr );
    sSegHdrPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    /* ù��° ExtDesc�� ExtDirPage�� �ִ� Extent�̱⶧���� ���� ���߿� Free�ؾ� �Ѵ�.
     * �Ͽ� Extent Dir Page�� Extent�� �������� Free�Ѵ�. */
    sExtDescCnt    = aExtDirCntlHdr->mExtDescCnt;

    while( sExtDescCnt > 1 )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       aStartInfo,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        sExtDescCnt--;

        IDE_TEST( freeLstExt( aStatistics,
                              &sFreeMtx,
                              aSpaceID,
                              aSegHdr,
                              aExtDirCntlHdr ) != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sExtDirPagePtr )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sSegHdrPagePtr )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aExtRID�� ����Ű�� Extent������ ��� Extent�� free�Ѵ�.
 *
 * aStatistics    - [IN] ��� ����
 * aStartInfo     - [IN] Mini Transaction Start Info
 * aSpaceID       - [IN] TableSpace ID
 * aSegHdr        - [IN] Segment Header
 * aExtDirHdr     - [IN] Extent Direct Control Header
 * aExtRID        - [IN] Extent RID
 **********************************************************************/
IDE_RC sdpsfExtDirPage::freeAllNxtExt( idvSQL             * aStatistics,
                                       sdrMtxStartInfo    * aStartInfo,
                                       scSpaceID            aSpaceID,
                                       sdpsfSegHdr        * aSegHdr,
                                       sdpsfExtDirCntlHdr * aExtDirCntlHdr,
                                       sdRID                aExtRID )
{
    UShort             sExtDescCnt;
    sdrMtx             sFreeMtx;
    SInt               sState = 0;
    UChar            * sExtDirPagePtr;
    UChar            * sSegHdrPagePtr;
    sdRID              sLstExtRID;

    IDE_ASSERT( aStartInfo     != NULL );
    IDE_ASSERT( aSpaceID       != 0 );
    IDE_ASSERT( aSegHdr        != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );
    IDE_ASSERT( aExtRID        != SD_NULL_RID );

    sExtDirPagePtr = sdpPhyPage::getPageStartPtr( aExtDirCntlHdr );
    sSegHdrPagePtr = sdpPhyPage::getPageStartPtr( aSegHdr );

    /* ù��° ExtDesc�� ExtDirPage�� �ִ� Extent�̱⶧���� ���� ���߿� Free�ؾ� �Ѵ�.
     * �Ͽ� Extent Dir Page�� Extent�� �������� Free�Ѵ�. */
    sExtDescCnt = aExtDirCntlHdr->mExtDescCnt;

    while( sExtDescCnt > 0 )
    {
        sLstExtRID = sdpsfExtDirPage::getLstExtRID( aExtDirCntlHdr );

        if( sLstExtRID == aExtRID )
        {
            break;
        }

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       aStartInfo,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( freeLstExt( aStatistics,
                              &sFreeMtx,
                              aSpaceID,
                              aSegHdr,
                              aExtDirCntlHdr ) != IDE_SUCCESS );

        sExtDescCnt--;

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sExtDirPagePtr )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sFreeMtx,
                                              sSegHdrPagePtr )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Debug�� ���ؼ� Console�� ExtentDesc������ ����Ѵ�.
 *
 * aExtDirCntlHdr- [IN] Extent Direct Control Header
 **********************************************************************/
IDE_RC sdpsfExtDirPage::dump( sdpsfExtDirCntlHdr * aExtDirCntlHdr )
{
    UInt          i;
    sdpsfExtDesc *sExtDesc;
    UInt          sExtCnt;

    IDE_ERROR( aExtDirCntlHdr != NULL );

    sExtCnt = aExtDirCntlHdr->mExtDescCnt;

    for( i = 0; i < sExtCnt; i++ )
    {
        sExtDesc = getNthExtDesc( aExtDirCntlHdr, i );
        ideLog::log( IDE_SERVER_0, 
                     "%uth,"
                     "FstPID: %u,"
                     "Flag:%u",
                     i,
                     sExtDesc->mFstPID,
                     sExtDesc->mFlag );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
