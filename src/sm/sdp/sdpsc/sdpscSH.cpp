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
 *
 * $Id: sdpscSH.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment�� Segment Header ����
 * STATIC �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <smErrorCode.h>
# include <sdr.h>

# include <sdpReq.h>
# include <sdpPhyPage.h>

# include <sdpscDef.h>
# include <sdpscSH.h>
# include <sdpscCache.h>
# include <sdpscAllocPage.h>
# include <sdpscSegDDL.h>

/***********************************************************************
 *
 * Description : Segment Header�� �ʱ�ȭ �Ѵ�.
 *
 * aSegHdr            - [IN] ���׸�Ʈ ��Ÿ ��� ������
 * aSegPID            - [IN] ���׸�Ʈ ��� �������� PID
 * aSegType           - [IN] ���׸�Ʈ Ÿ��
 * aPageCntInExt      - [IN] �ϳ��� Extent Desc.�� ������ ����
 * aMaxExtCntInExtDir - [IN] ���׸�Ʈ ��� �������� Extent Map�� ����� �ִ�
 *                           Extent Desc. ����
 *
 ***********************************************************************/
void sdpscSegHdr::initSegHdr( sdpscSegMetaHdr   * aSegHdr,
                              scPageID            aSegHdrPID,
                              sdpSegType          aSegType,
                              UInt                aPageCntInExt,
                              UShort              aMaxExtCntInExtDir )
{
    scOffset   sMapOffset;

    IDE_ASSERT( aSegHdr != NULL );

    /* Segment Control Header �ʱ�ȭ */
    aSegHdr->mSegCntlHdr.mSegType     = aSegType;
    aSegHdr->mSegCntlHdr.mSegState    = SDP_SEG_USE;

    /* Extent Control Header �ʱ�ȭ */
    aSegHdr->mExtCntlHdr.mTotExtCnt         = 0;
    aSegHdr->mExtCntlHdr.mLstExtDir         = aSegHdrPID;
    aSegHdr->mExtCntlHdr.mTotExtDirCnt      = 0;
    aSegHdr->mExtCntlHdr.mPageCntInExt      = aPageCntInExt;

    sdpPhyPage::initLogicalHdr( sdpPhyPage::getHdr((UChar*)aSegHdr),
                                ID_SIZEOF(sdpscSegMetaHdr) );

    sMapOffset = sdpPhyPage::getDataStartOffset(
                                ID_SIZEOF(sdpscSegMetaHdr) ); // Logical Header

    /* ExtDir Control Header �ʱ�ȭ */
    sdpscExtDir::initCntlHdr( &aSegHdr->mExtDirCntlHdr,
                              aSegHdrPID,  /* aNxtExtDir */
                              sMapOffset,
                              aMaxExtCntInExtDir );

    return;
}

/***********************************************************************
 *
 * Description : [ INTERFACE ] segment ���¸� ��ȯ�Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceID    - [IN] ���̺����̽� ID
 * aSegPID     - [IN] ���׸�Ʈ ��� �������� PID
 * aSegState   - [OUT] ���׸�Ʈ ���°�
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::getSegState( idvSQL        *aStatistics,
                                 scSpaceID      aSpaceID,
                                 scPageID       aSegPID,
                                 sdpSegState   *aSegState )
{
    idBool        sDummy;
    UChar       * sPagePtr;
    sdpSegState   sSegState;
    UInt          sState = 0;

    IDE_ASSERT( aSegPID != SD_NULL_PID );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          &sPagePtr,
                                          &sDummy ) != IDE_SUCCESS );
    sState = 1;

    sSegState = getSegCntlHdr( getHdrPtr(sPagePtr) )->mSegState;

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    *aSegState = sSegState;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment Header ������ ���� �� �ʱ�ȭ
 *
 * ù��° Extent�� bmp �������� ��� ������ �� Segment Header
 * �������� �����Ѵ�.
 *
 * aStatistics        - [IN] �������
 * aStartInfo         - [IN] Mtx ���� ����
 * aSpaceID           - [IN] ���̺����̽� ID
 * aNewSegPID         - [IN] ������ ���׸�Ʈ ��� PID
 * aSegType           - [IN] ���׸�Ʈ Ÿ��
 * aMaxExtCntInExtDir - [IN] ���׸�Ʈ ��� �������� Extent Map��
 *                           ����� �ִ� Extent Desc. ����
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::createAndInitPage( idvSQL            * aStatistics,
                                       sdrMtxStartInfo   * aStartInfo,
                                       scSpaceID           aSpaceID,
                                       sdpscExtDesc      * aFstExtDesc,
                                       sdpSegType          aSegType,
                                       UShort              aMaxExtCntInExtDir )
{
    sdrMtx             sMtx;
    UInt               sState = 0;
    UChar            * sPagePtr;
    scPageID           sSegHdrPID;
    sdpParentInfo      sParentInfo;

    IDE_ASSERT( aStartInfo  != NULL );
    IDE_ASSERT( aFstExtDesc != NULL );

    sSegHdrPID = aFstExtDesc->mExtFstPID;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    sParentInfo.mParentPID   = sSegHdrPID;
    sParentInfo.mIdxInParent = SDPSC_INVALID_IDX;

    /*
     * ù��° Extent���� Segment Header�� �Ҵ��Ѵ�.
     * ������ Segment Header �������� PID�� ��ȯ�Ѵ�.
     * logical header�� �Ʒ����� ������ �ʱ�ȭ�Ѵ�.
     */
    IDE_TEST( sdpscAllocPage::createPage( aStatistics,
                                          aSpaceID,
                                          sSegHdrPID,
                                          SDP_PAGE_CMS_SEGHDR,
                                          &sParentInfo,
                                          (sdpscPageBitSet)
                                          ( SDPSC_BITSET_PAGETP_META |
                                            SDPSC_BITSET_PAGEFN_FUL ),
                                          &sMtx,
                                          &sMtx,
                                          &sPagePtr ) != IDE_SUCCESS );

    // Segment Header ������ �ʱ�ȭ
    initSegHdr( getHdrPtr(sPagePtr),
                sSegHdrPID,
                aSegType,
                aFstExtDesc->mLength,
                aMaxExtCntInExtDir );

    // INIT_SEGMENT_META_HEADER �α�
    IDE_TEST( sdrMiniTrans::writeLogRec( &sMtx,
                                         (UChar*)getHdrPtr(sPagePtr),
                                         NULL,
                                         ID_SIZEOF( aSegType ) +
                                         ID_SIZEOF( sSegHdrPID ) +
                                         ID_SIZEOF( UInt ) +
                                         ID_SIZEOF( UShort ),
                                         SDR_SDPSC_INIT_SEGHDR )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   (UChar*)&aSegType,
                                   ID_SIZEOF( aSegType ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   (UChar*)&sSegHdrPID,
                                   ID_SIZEOF( sSegHdrPID ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   (UChar*)&aFstExtDesc->mLength,
                                   ID_SIZEOF( UInt ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( &sMtx,
                                   (UChar*)&aMaxExtCntInExtDir,
                                   ID_SIZEOF( UShort ) ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment Header �������� ���ο� ExtDir�� �߰�
 *               Segment Header�� TotExtDescCnt�� ������Ų��.
 *
 * aStatistics    - [IN] �������
 * aMtx           - [IN] Mtx ������
 * aSpaceID       - [IN] ���̺����̽� ID
 * aSegHdrPID     - [IN] ���׸�Ʈ ��� ������ ID
 * aCurExtDirPID  - [IN] ���� Extent Dir. ������ Control ��� ������
 * aNewExtDirPtr  - [IN] �߰��� Extent Dir. ������ ���� ������
 * aTotExtDescCnt - [OUT] �߰��� ExtDir�� ExtDesc ����
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::addNewExtDir( idvSQL             * aStatistics,
                                  sdrMtx             * aMtx,
                                  scSpaceID            aSpaceID,
                                  scPageID             aSegHdrPID,
                                  scPageID             aCurExtDirPID,
                                  scPageID             aNewExtDirPID,
                                  UInt               * aTotExtCntOfToSeg )
{
    sdpscSegMetaHdr    * sSegHdr;
    sdpscExtCntlHdr    * sExtCntlHdr;
    sdpscExtDirCntlHdr * sCurExtDirCntlHdr;
    sdpscExtDirCntlHdr * sNewExtDirCntlHdr;
    UInt                 sTotExtCntOfSeg;

    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aSegHdrPID        != SD_NULL_PID );
    IDE_ASSERT( aCurExtDirPID     != SD_NULL_PID );
    IDE_ASSERT( aNewExtDirPID     != SD_NULL_PID );
    IDE_ASSERT( aTotExtCntOfToSeg != NULL );

    IDE_TEST( fixAndGetHdr4Write( aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aSegHdrPID,
                                  &sSegHdr ) != IDE_SUCCESS );

    if ( aCurExtDirPID != aSegHdrPID )
    {
        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write( aStatistics,
                                                       aMtx,
                                                       aSpaceID,
                                                       aCurExtDirPID,
                                                       &sCurExtDirCntlHdr )
                  != IDE_SUCCESS );
    }
    else
    {
        sCurExtDirCntlHdr = getExtDirCntlHdr( sSegHdr );
    }

    IDE_ASSERT( sCurExtDirCntlHdr->mMaxExtCnt == sCurExtDirCntlHdr->mExtCnt );

    IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write( aStatistics,
                                                   aMtx,
                                                   aSpaceID,
                                                   aNewExtDirPID,
                                                   &sNewExtDirCntlHdr )
              != IDE_SUCCESS );
    /* full ������ Ext�� steal ����� �� */
    IDE_ASSERT( sNewExtDirCntlHdr->mMaxExtCnt == sNewExtDirCntlHdr->mExtCnt );
    /* CurExtDir�� NewExtDir ���� */
    IDE_TEST( sdpscSegHdr::addNewExtDir( aMtx,
                                         sSegHdr,
                                         sCurExtDirCntlHdr,
                                         sNewExtDirCntlHdr )
              != IDE_SUCCESS );

    sExtCntlHdr = getExtCntlHdr( sSegHdr );

    sTotExtCntOfSeg = sExtCntlHdr->mTotExtCnt + sNewExtDirCntlHdr->mExtCnt;

    IDE_TEST( setTotExtCnt( aMtx, sExtCntlHdr, sTotExtCntOfSeg )
              != IDE_SUCCESS );

    *aTotExtCntOfToSeg = sExtCntlHdr->mTotExtCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aTotExtCntOfToSeg = 0;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Segment Header �������� ���ο� ExtDir�� �߰�
 *               Segment Header�� TotExtDescCnt�� �������� �ʰ�,
 *               ȣ���ϴ� �ʿ��� �۾��Ѵ�.
 *
 * aStatistics       - [IN] �������
 * aMtx              - [IN] Mtx ������
 * aSpaceID          - [IN] ���̺����̽� ID
 * aSegHdr           - [IN] ���׸�Ʈ ��� ������
 * aCurExtDirCntlHdr - [IN] ���� Extent Dir. ������ Control ��� ������
 * aNewExtDirPtr     - [IN] �߰��� Extent Dir. ������ ���� ������
 *
 ***********************************************************************/
IDE_RC  sdpscSegHdr::addNewExtDir( sdrMtx             * aMtx,
                                   sdpscSegMetaHdr    * aSegHdr,
                                   sdpscExtDirCntlHdr * aCurExtDirCntlHdr,
                                   sdpscExtDirCntlHdr * aNewExtDirCntlHdr )
{
    sdpscExtCntlHdr    * sExtCntlHdr;
    scPageID             sNewExtDirPID;

    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aSegHdr           != NULL );
    IDE_ASSERT( aCurExtDirCntlHdr != NULL );
    IDE_ASSERT( aNewExtDirCntlHdr != NULL );

    sNewExtDirPID = sdpPhyPage::getPageID(
        sdpPhyPage::getPageStartPtr((UChar*)aNewExtDirCntlHdr) );

    /*
     * Segment Header�� �� Extent Dir. ������ ������ ������Ŵ
     */
    sExtCntlHdr = getExtCntlHdr( aSegHdr );

    // ���� ExtDir �������� ���׸�Ʈ�� �������̶�� ������ ExtDir�� �����Ѵ�.
    if ( sdpPhyPage::getPageID(
             sdpPhyPage::getPageStartPtr((UChar*)aCurExtDirCntlHdr) )
         == sExtCntlHdr->mLstExtDir )
    {
        IDE_ASSERT( aCurExtDirCntlHdr->mNxtExtDir != SD_NULL_PID );

        IDE_TEST( setLstExtDir( aMtx,
                                sExtCntlHdr,
                                sNewExtDirPID ) != IDE_SUCCESS );
    }

    IDE_TEST( setTotExtDirCnt( aMtx,
                               sExtCntlHdr,
                               (sExtCntlHdr->mTotExtDirCnt + 1) )
              != IDE_SUCCESS );

    /*
     * New Ext Dir�� Next Extent Dir. ������
     *
     * Cur�� Last Extent Dir.������ ���ٸ� Nxt�� SD_NULL_PID�̾��� ���̰�,
     * �׷��� �ʴٸ�, Nxt Extent Dir ������ ID�� �����ɰ��̴�
     */
    IDE_TEST( sdpscExtDir::setNxtExtDir( aMtx,
                                         aNewExtDirCntlHdr,
                                         aCurExtDirCntlHdr->mNxtExtDir )
              != IDE_SUCCESS );

    /*
     * ���� ExtDir�� Next ExtDir�� New ExtDir PID�� ������.
     */
    IDE_TEST( sdpscExtDir::setNxtExtDir( aMtx,
                                         aCurExtDirCntlHdr,
                                         sNewExtDirPID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment Header �������κ��� ExtDir ����
 *               Segment Header�� TotExtDescCnt�� �����Ѵ�.
 *
 * aStatistics           - [IN] �������
 * aMtx                  - [IN] Mtx ������
 * aSpaceID              - [IN] ���̺����̽� ID
 * aSegHdrPID            - [IN] ���׸�Ʈ ��� ������
 * aPrvPIDOfExtDir       - [IN] ���� Extent Dir. ������ Control ��� ������
 * aRemExtDirPID         - [IN] ������ Extent Dir. ������ ���� ������
 * aNxtPIDOfNewExtDir    - [IN] ������ Extent Dir. �������� NxtPID
 * aTotExtCntOfRemExtDir - [IN] ������ Extent Dir.�� ExtDesc ���� 
 * aTotExtCntOfFrSeg     - [OUT] ������ ���� Segment�� �� ExtDesc ����
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::removeExtDir( idvSQL             * aStatistics,
                                  sdrMtx             * aMtx,
                                  scSpaceID            aSpaceID,
                                  scPageID             aSegHdrPID,
                                  scPageID             aPrvPIDOfExtDir,
                                  scPageID             aRemExtDirPID,
                                  scPageID             aNxtPIDOfNewExtDir,
                                  UShort               aTotExtCntOfRemExtDir,
                                  UInt               * aTotExtCntOfFrSeg )
{
    sdpscSegMetaHdr    * sSegHdr;
    sdpscExtCntlHdr    * sExtCntlHdr;
    sdpscExtDirCntlHdr * sPrvExtDirCntlHdr;
    UInt                 sTotExtCntOfSeg = 0;

    IDE_ASSERT( aSegHdrPID           != SD_NULL_PID );
    IDE_ASSERT( aPrvPIDOfExtDir      != SD_NULL_PID );
    IDE_ASSERT( aRemExtDirPID        != SD_NULL_PID );
    IDE_ASSERT( aNxtPIDOfNewExtDir   != SD_NULL_PID );
    IDE_ASSERT( aTotExtCntOfRemExtDir > 0 );
    IDE_ASSERT( aTotExtCntOfFrSeg    != NULL );

    /* BUG-31055 Can not reuse undo pages immediately after it is used to 
     *           aborted transaction.
     * Segment Header�� �����ϸ� �ȵȴ� */
    if( aSegHdrPID == aRemExtDirPID )
    {
        ideLog::log( IDE_SERVER_0, 
                     "aSpaceID              = %u\n"
                     "aSegHdrPID            = %u\n"
                     "aPrvPIDOfExtDir       = %u\n"
                     "aRemExtDirPID         = %u\n"
                     "aNxtPIDOfNewExtDir    = %u\n"
                     "aTotExtCntOfRemExtDir = %u\n",
                     aSpaceID,
                     aSegHdrPID,
                     aPrvPIDOfExtDir,
                     aRemExtDirPID,
                     aNxtPIDOfNewExtDir,
                     aTotExtCntOfRemExtDir );

        IDE_ASSERT( 0 );
    }

    IDE_TEST( fixAndGetHdr4Write( aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aSegHdrPID,
                                  &sSegHdr ) != IDE_SUCCESS );

    if ( aPrvPIDOfExtDir != aSegHdrPID )
    {
        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write( aStatistics,
                                                       aMtx,
                                                       aSpaceID,
                                                       aPrvPIDOfExtDir,
                                                       &sPrvExtDirCntlHdr )
                  != IDE_SUCCESS );
    }
    else
    {
        sPrvExtDirCntlHdr = getExtDirCntlHdr( sSegHdr );
    }

    IDE_TEST( removeExtDir( aMtx,
                            sSegHdr,
                            sPrvExtDirCntlHdr,
                            aRemExtDirPID,
                            aNxtPIDOfNewExtDir ) != IDE_SUCCESS );

    sExtCntlHdr = getExtCntlHdr( sSegHdr );

    IDE_ASSERT( sExtCntlHdr->mTotExtCnt > aTotExtCntOfRemExtDir );

    sTotExtCntOfSeg = sExtCntlHdr->mTotExtCnt - aTotExtCntOfRemExtDir;

    IDE_TEST( setTotExtCnt( aMtx, sExtCntlHdr, sTotExtCntOfSeg )
              != IDE_SUCCESS );

    *aTotExtCntOfFrSeg = sExtCntlHdr->mTotExtCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Segment Header �������κ��� ExtDir ����
 *               Segment Header�� TotExtDescCnt�� �������� �ʰ�,
 *               ȣ���ϴ� �ʿ��� �۾��Ѵ�.
 *
 * �������, ������ ���� ���
 *
 * ExtDir0   ExtDir1    ExtDir2    ExtDir3
 *
 * [ 67 ] -> [ 131 ] -> [ 163 ] -> [ 195 ] -> NULL
 *  Cur        shk       NewCur
 *
 * ���� ExtDir1 [ 131 ] �� Shrink ����̶�� [67]�� [163]�� �����ؾ��Ѵ�.
 *
 * ExtDir0   ExtDir1    ExtDir2
 *
 * [ 67 ] -> [ 163 ] -> [ 195 ] -> NULL
 *  Cur        shk       NewCur
 *
 * �������, ������ ���� ���
 *
 * ExtDir0   ExtDir1
 *
 * [ 67 ] -> [ 131 ] -> NULL
 *  Cur        Shk
 *
 * ���� ExtDir1 [ 131 ] �� Shrink ����̶�� [67]�� NULL �� �����ؾ��Ѵ�.
 *
 * ExtDir0   ExtDir1    ExtDir2
 *
 * [ 67 ] -> NULL
 *  NewCur
 *
 * aMtx              - [IN] Mtx ������
 * aSegHdrPtr        - [IN] ���׸�Ʈ ��� ������
 * aPrvExtDirCntlHdr - [IN] ���� Extent Dir. Control ��� ������
 * aRemExtDirPID     - [IN] ������ Extent Dir. ������ ID
 * aNewNxtExtDir     - [IN] ���� Extent Dir. ������ ID
 *
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::removeExtDir( sdrMtx             * aMtx,
                                  sdpscSegMetaHdr    * aSegHdr,
                                  sdpscExtDirCntlHdr * aPrvExtDirCntlHdr,
                                  scPageID             aRemExtDirPID,
                                  scPageID             aNewNxtExtDir )
{
    scPageID            sPrvExtDirPID;
    sdpscExtCntlHdr   * sExtCntlHdr;
    UInt                sTotExtDirCnt;

    IDE_ASSERT( aSegHdr           != NULL );
    IDE_ASSERT( aPrvExtDirCntlHdr != NULL );
    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aRemExtDirPID     != SD_NULL_PID );
    IDE_ASSERT( aNewNxtExtDir     != SD_NULL_PID );

    sExtCntlHdr   = getExtCntlHdr( aSegHdr );

    IDE_ERROR_RAISE( sExtCntlHdr->mTotExtDirCnt > 0,
                     ERR_ZERO_TOTEXTDIRCNT );

    sTotExtDirCnt = sExtCntlHdr->mTotExtDirCnt - 1;
    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sExtCntlHdr->mTotExtDirCnt,
                                         &sTotExtDirCnt,
                                         ID_SIZEOF( sTotExtDirCnt ) )
              != IDE_SUCCESS );

    if ( aRemExtDirPID == sExtCntlHdr->mLstExtDir )
    {
        // ������ ExtDir �������� ���׸�Ʈ�� LstExtDir�̶�� ������ ExtDir�� ���� �����Ѵ�.
        sPrvExtDirPID = sdpPhyPage::getPageID(
            sdpPhyPage::getPageStartPtr(aPrvExtDirCntlHdr) );

        IDE_TEST( sdrMiniTrans::writeNBytes(
                                aMtx,
                                (UChar*)&sExtCntlHdr->mLstExtDir,
                                &sPrvExtDirPID,
                                ID_SIZEOF( scPageID ) )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(aPrvExtDirCntlHdr->mNxtExtDir),
                  &aNewNxtExtDir,
                  ID_SIZEOF( scPageID ) ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_ZERO_TOTEXTDIRCNT);
    {
        ideLog::log(
            IDE_ERR_0,
            "PrvExtDirCntlHdr : mExtCnt        : %u\n"
            "                   mNxtExtDir     : %u\n"
            "                   mMaxExtCnt     : %u\n"
            "                   mMapOffset     : %u\n"
            "                   mLstCommitSCN  : %llu\n"
            "                   mFstDskViewSCN : %llu\n"
            "RemExtDirPID     : %u\n"
            "NewNxtExtDir     : %u\n"
            "ExtCntlHdr       : mTotExtCnt     : %u\n"
            "                   mLstExtDir     : %u\n"
            "                   mTotExtDirCnt  : %u\n"
            "                   mPageCntInExt  : %u\n",
             aPrvExtDirCntlHdr->mExtCnt,
             aPrvExtDirCntlHdr->mNxtExtDir,
             aPrvExtDirCntlHdr->mMaxExtCnt,
             aPrvExtDirCntlHdr->mMapOffset,
#ifdef COMPILE_64BIT
            aPrvExtDirCntlHdr->mLstCommitSCN,
#else
            ( ((ULong)aPrvExtDirCntlHdr->mLstCommitSCN.mHigh) << 32 ) |
                (ULong)aPrvExtDirCntlHdr->mLstCommitSCN.mLow,
#endif
#ifdef COMPILE_64BIT
            aPrvExtDirCntlHdr->mFstDskViewSCN,
#else
            ( ((ULong)aPrvExtDirCntlHdr->mFstDskViewSCN.mHigh) << 32 ) |
                (ULong)aPrvExtDirCntlHdr->mFstDskViewSCN.mLow,
#endif
            aRemExtDirPID,
            aNewNxtExtDir,
            sExtCntlHdr->mTotExtCnt,
            sExtCntlHdr->mLstExtDir,
            sExtCntlHdr->mTotExtDirCnt,
            sExtCntlHdr->mPageCntInExt );
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Sequential Scan�� ���� Segment�� ������ ��ȯ�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aSegPID      - [IN] ���׸�Ʈ ��� �������� PID
 * aSegInfo     - [OUT] ������ ���׸�Ʈ ���� �ڷᱸ��
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::getSegInfo( idvSQL        * aStatistics,
                                scSpaceID       aSpaceID,
                                scPageID        aSegPID,
                                void          * /*aTableHeader*/,
                                sdpSegInfo    * aSegInfo )
{
    sdpscSegMetaHdr  * sSegHdr;
    SInt               sState = 0;

    IDE_ASSERT( aSegInfo != NULL );

    idlOS::memset( aSegInfo, 0x00, ID_SIZEOF(sdpSegInfo) );

    IDE_TEST( fixAndGetHdr4Read( aStatistics,
                                 NULL,
                                 aSpaceID,
                                 aSegPID,
                                 &sSegHdr )
              != IDE_SUCCESS );
    sState = 1;

    aSegInfo->mSegHdrPID    = aSegPID;
    aSegInfo->mType         = (sdpSegType)(sSegHdr->mSegCntlHdr.mSegType);
    aSegInfo->mState        = (sdpSegState)(sSegHdr->mSegCntlHdr.mSegState);
    aSegInfo->mPageCntInExt = sSegHdr->mExtCntlHdr.mPageCntInExt;
    aSegInfo->mExtCnt       = sSegHdr->mExtCntlHdr.mTotExtCnt;
    aSegInfo->mExtDirCnt    = sSegHdr->mExtCntlHdr.mTotExtDirCnt;

    IDE_ASSERT( aSegInfo->mExtCnt > 0 );

    aSegInfo->mFstExtRID    = SD_MAKE_RID(
        aSegPID,
        sdpscExtDir::calcDescIdx2Offset(&(sSegHdr->mExtDirCntlHdr), 0) );

    aSegInfo->mFstPIDOfLstAllocExt = aSegPID;

    sState = 0;
    IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, (UChar*)sSegHdr )
                == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, (UChar*)sSegHdr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : FmtPageCnt�� ����ش�.
 *
 * aStatistics  - [IN] ��� ����
 * aSpaceID     - [IN] TableSpace ID
 * aSegHandle   - [IN] Segment Handle
 *
 * aSegCache    - [OUT] SegInfo�� �����ȴ�.
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::getFmtPageCnt( idvSQL        *aStatistics,
                                   scSpaceID      aSpaceID,
                                   sdpSegHandle  *aSegHandle,
                                   ULong         *aFmtPageCnt )
{
    sdpSegInfo  sSegInfo;

    IDE_ASSERT( aSegHandle  != NULL );
    IDE_ASSERT( aFmtPageCnt != NULL );

    IDE_TEST( getSegInfo( aStatistics,
                          aSpaceID,
                          aSegHandle->mSegPID,
                          NULL, /* aTableHeader */
                          &sSegInfo ) != IDE_SUCCESS );

    *aFmtPageCnt = sSegInfo.mFmtPageCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���Ÿ������� SegHdr �������� Control Header�� fix�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aSegHdrPID   - [IN] X-Latch�� ȹ���� SegHdr �������� PID
 * aSegHdr      - [OUT] SegHdr.�������� ��� ������
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::fixAndGetHdr4Write(
                      idvSQL           * aStatistics,
                      sdrMtx           * aMtx,
                      scSpaceID          aSpaceID,
                      scPageID           aSegHdrPID,
                      sdpscSegMetaHdr ** aSegHdr )
{
    idBool   sDummy;
    UChar  * sPagePtr;

    IDE_ASSERT( aSegHdrPID != SD_NULL_PID );
    IDE_ASSERT( aSegHdr    != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegHdrPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL )
              != IDE_SUCCESS );

    IDE_ASSERT( sdpPhyPage::getPhyPageType( sPagePtr ) ==
                SDP_PAGE_CMS_SEGHDR );

    *aSegHdr = getHdrPtr( sPagePtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aSegHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���Ÿ������� SegHdr �������� Control Header�� fix�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aSegHdrPID   - [IN] S-Latch�� ȹ���� SegHdr �������� PID
 * aSegHdr      - [OUT] SegHdr.�������� ��� ������
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::fixAndGetHdr4Read(
                      idvSQL             * aStatistics,
                      sdrMtx             * aMtx,
                      scSpaceID            aSpaceID,
                      scPageID             aSegHdrPID,
                      sdpscSegMetaHdr   ** aSegHdr )
{
    idBool               sDummy;
    UChar              * sPagePtr;

    IDE_ASSERT( aSegHdrPID != SD_NULL_PID );
    IDE_ASSERT( aSegHdr    != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegHdrPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL )
              != IDE_SUCCESS );

    IDE_ASSERT( sdpPhyPage::getPhyPageType( sPagePtr ) ==
                SDP_PAGE_CMS_SEGHDR );

    *aSegHdr = getHdrPtr( sPagePtr );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aSegHdr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���׸�Ʈ Extent Control Header�� �� ExtDir ���� ����
 *
 * aMtx           - [IN] Mtx ������
 * aExtCntlHdr    - [IN] ���׸�Ʈ Extent Control Header ������
 * aTotExtDirCnt - [IN] ���׸�Ʈ �� ExtDir ������ ����
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::setTotExtDirCnt( sdrMtx          * aMtx,
                                     sdpscExtCntlHdr * aExtCntlHdr,
                                     UInt              aTotExtDirCnt )
{
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aExtCntlHdr   != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&aExtCntlHdr->mTotExtDirCnt,
                                      &aTotExtDirCnt,
                                      ID_SIZEOF( aTotExtDirCnt ) );
}


/***********************************************************************
 *
 * Description : ���׸�Ʈ Extent Control Header�� LstExtDirPageID ����
 *
 * aMtx          - [IN] Mtx ������
 * aExtCntlHdr   - [IN] ���׸�Ʈ Extent Control Header ������
 * aLstExtDirPID - [IN] ������ ExtDirPage�� ������ ID
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::setLstExtDir( sdrMtx             * aMtx,
                                  sdpscExtCntlHdr    * aExtCntlHdr,
                                  scPageID             aLstExtDirPID )
{
    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aExtCntlHdr       != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&aExtCntlHdr->mLstExtDir,
                                      &aLstExtDirPID,
                                      ID_SIZEOF( aLstExtDirPID ) );
}


/***********************************************************************
 *
 * Description : ���׸�Ʈ Extent Control Header�� �� ExtDesc ���� ����
 *
 * aMtx           - [IN] Mtx ������
 * aExtCntlHdr    - [IN] ���׸�Ʈ Extent Control Header ������
 * aTotExtDescCnt - [IN] ���׸�Ʈ �� ExtDesc ����
 *
 ***********************************************************************/
IDE_RC sdpscSegHdr::setTotExtCnt( sdrMtx          * aMtx,
                                  sdpscExtCntlHdr * aExtCntlHdr,
                                  UInt              aTotExtDescCnt )
{
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aExtCntlHdr   != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&aExtCntlHdr->mTotExtCnt,
                                      &aTotExtDescCnt,
                                      ID_SIZEOF( aTotExtDescCnt ) );
}

