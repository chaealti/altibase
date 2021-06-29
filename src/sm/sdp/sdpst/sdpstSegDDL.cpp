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
 * $Id: sdpstSegDDL.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment�� Create/Drop/Alter/Reset ������
 * STATIC �������̽����� �����Ѵ�.
 *
 ***********************************************************************/

# include <smr.h>
# include <sdr.h>
# include <sdbBufferMgr.h>
# include <sdpReq.h>

# include <sdpstDef.h>
# include <sdpstSegDDL.h>

# include <sdpstSH.h>
# include <sdpstLfBMP.h>
# include <sdpstItBMP.h>
# include <sdpstRtBMP.h>
# include <sdpstExtDir.h>
# include <sdpstCache.h>
# include <sdpstAllocExtInfo.h>
# include <sdptbExtent.h>
# include <sdpstDPath.h>
# include <sdpstStackMgr.h>

/***********************************************************************
 * Description : [ INTERFACE ] Segment �Ҵ� �� Segment Meta Header �ʱ�ȭ
 *
 * aStatistics - [IN]  �������
 * aMtx        - [IN]  Mini Transaction�� Pointer
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegType    - [IN]  Segment�� Type
 * aSegHandle  - [IN]  Segment�� Handle
 ***********************************************************************/
IDE_RC sdpstSegDDL::createSegment( idvSQL                * aStatistics,
                                   sdrMtx                * aMtx,
                                   scSpaceID               aSpaceID,
                                   sdpSegType              /* aSegType */,
                                   sdpSegHandle          * aSegHandle )
{
    IDE_ASSERT( aMtx != NULL );
    IDE_ASSERT( aSegHandle != NULL );

    IDE_TEST( allocateSegment( aStatistics,
                               aMtx,
                               aSpaceID,
                               aSegHandle ) != IDE_SUCCESS );

    IDE_ASSERT( aSegHandle->mSegPID != SD_NULL_PID );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] Segment ����
 *
 * Segment���� Extent�� �����ϴ� ������ ù���� ExtDir����������
 * ������ ExtDir���������� ������ ���� SegHdr�� ExtDir������
 * �����Ͽ� ��� Extent�� �����Ѵ�.
 *
 * aStatistics - [IN]  �������
 * aMtx        - [IN]  Mini Transaction�� Pointer
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegPID     - [IN]  Segment�� Hdr PID
 ***********************************************************************/
IDE_RC sdpstSegDDL::dropSegment( idvSQL           * aStatistics,
                                 sdrMtx           * aMtx,
                                 scSpaceID          aSpaceID,
                                 scPageID           aSegPID )
{
    UChar           * sPagePtr;
    sdpstSegHdr     * sSegHdr;

    IDE_ASSERT( aMtx != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    IDE_TEST( freeAllExts( aStatistics,
                           aMtx,
                           aSpaceID,
                           sSegHdr ) != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [ INTERFACE ] Segment ����
 *
 * aStatistics - [IN]  �������
 * aMtx        - [IN]  Mini Transaction�� Pointer
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegHandle  - [IN]  Segment�� Handle
 * aSegType    - [IN]  Segment�� Type
 ***********************************************************************/
IDE_RC sdpstSegDDL::resetSegment( idvSQL           * aStatistics,
                                  sdrMtx           * aMtx,
                                  scSpaceID          aSpaceID,
                                  sdpSegHandle     * aSegHandle,
                                  sdpSegType         /*aSegType*/ )
{
    sdrMtxStartInfo      sStartInfo;
    sdrMtx               sMtx;
    sdpstSegHdr        * sSegHdr;
    scPageID             sSegPID = SD_NULL_PID;
    sdpstExtDesc         sExtDesc;
    sdpstExtDesc       * sExtDescPtr;
    UInt                 sState = 0 ;
    idBool               sIsFixedPage = ID_FALSE;
    sdpstBfrAllocExtInfo sBfrAllocExtInfo;
    sdpstAftAllocExtInfo sAftAllocExtInfo;
    UChar              * sPagePtr;
    sdpSegType           sSegType;
    sdRID                sAllocExtRID;
    sdpstSegCache      * sSegCache;

    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aMtx != NULL );

    sSegCache = (sdpstSegCache*)(aSegHandle->mCache);

    /*
     * A. ù��° Extent ������ ��´�.
     */
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegHandle->mSegPID,
                                          &sPagePtr ) != IDE_SUCCESS );
    sIsFixedPage = ID_TRUE;

    sSegHdr  = sdpstSH::getHdrPtr( sPagePtr );
    sSegType = sSegHdr->mSegType;

    IDE_TEST( freeExtsExceptFst( aStatistics,
                                 aMtx,
                                 aSpaceID,
                                 sSegHdr ) != IDE_SUCCESS );

    /* �Լ� ���������� ������ Mtx�� ������ �۾��� �����Ѵ�.
     * ���ڷ� �Ѿ�� aMtx�� Reset Segment ��������
     * ù��° Extent �ʱ�ȭ�ϴ� �۾��� ����Ѵ�. */

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    // ù��° Extent�� ����� ��� �����Ǿ����Ƿ� ù��° Extent��
    // �ٽ� �����Ѵ�.
    sdpstExtDir::initExtDesc( &sExtDesc );
    sdpstAllocExtInfo::initialize( &sBfrAllocExtInfo );
    sdpstAllocExtInfo::initialize( &sAftAllocExtInfo );

    /* ù��° Extent Slot�� ��ȯ�Ѵ�. */
    sExtDescPtr = sdpstExtDir::getFstExtDesc( sdpstSH::getExtDirHdr(sPagePtr) );

    sExtDesc.mExtFstPID = sExtDescPtr->mExtFstPID;
    sExtDesc.mLength    = sExtDescPtr->mLength;

    sIsFixedPage = ID_FALSE;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr ) != IDE_SUCCESS );

    sdpSegment::initCommonCache( &sSegCache->mCommon,
                                 sSegType,
                                 sExtDesc.mLength,
                                 sSegCache->mCommon.mTableOID,
                                 SM_NULL_INDEX_ID);

    /* Segment Header�� ������ ��� ���� Treelist ������
     * ���� ������ ������. */
    IDE_TEST( sdpstAllocExtInfo::getAllocExtInfo(
                                        aStatistics,
                                        aSpaceID,
                                        sSegPID,
                                        aSegHandle->mSegStoAttr.mMaxExtCnt,
                                        &sExtDesc,
                                        &sBfrAllocExtInfo,
                                        &sAftAllocExtInfo ) != IDE_SUCCESS );

    /* createMetaPage() ���� Segment Header�� �����ϰ� �Ѵ�.
     * mSegHdrCnt�� 0 �Ǵ� 1 ���� ����, �̹� SegHdr�� �����ϴ� ��� �׻� 0
     * �̾�� �Ѵ�. */
    sAftAllocExtInfo.mSegHdrCnt = 1;

    // ���ο� Extent�� ���� Bitmap Page���� �����Ѵ�.
    IDE_TEST( createMetaPages( aStatistics,
                               &sStartInfo,
                               aSpaceID,
                               sSegCache,
                               &sExtDesc,
                               &sBfrAllocExtInfo,
                               &sAftAllocExtInfo,
                               &sSegPID ) != IDE_SUCCESS );

    IDE_TEST( createDataPages( aStatistics,
                               &sStartInfo,
                               aSpaceID,
                               aSegHandle,
                               &sExtDesc,
                               &sBfrAllocExtInfo,
                               &sAftAllocExtInfo ) != IDE_SUCCESS );

    IDE_ASSERT( aSegHandle->mSegPID == sSegPID );

    sdpstCache::initItHint( aStatistics,
                            sSegCache,
                            aSegHandle->mSegPID,
                            sAftAllocExtInfo.mFstPID[SDPST_ITBMP] );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 1;

    /* Extent ������ Segment Header �������� ExtDir ��������
     * ����ϰ�, Segment Header �������� �����Ѵ�.
     * ���� Lst BMP �������� ���� ������ bmp ���������� ������ ����Ѵ�. */
    IDE_TEST( linkNewExtToLstBMPs( aStatistics,
                                   &sMtx,
                                   aSpaceID,
                                   aSegHandle,
                                   &sExtDesc,
                                   &sBfrAllocExtInfo,
                                   &sAftAllocExtInfo ) != IDE_SUCCESS );

    /* Extent ������ Segment Header �������� ExtDir ��������
     * ����ϰ�, Segment Header �������� �����Ѵ�. */
    IDE_TEST( linkNewExtToExtDir( aStatistics,
                                  &sMtx,
                                  aSpaceID,
                                  sSegPID,
                                  &sExtDesc,
                                  &sBfrAllocExtInfo,
                                  &sAftAllocExtInfo,
                                  &sAllocExtRID ) != IDE_SUCCESS );

    /* Extent �Ҵ� ������ �Ϸ��۾����� Segment Header��
     * BMP �������� �����Ѵ�. */
    IDE_TEST( linkNewExtToSegHdr( aStatistics,
                                  &sMtx,
                                  aSpaceID,
                                  sSegPID,
                                  &sExtDesc,
                                  &sBfrAllocExtInfo,
                                  &sAftAllocExtInfo,
                                  sAllocExtRID,
                                  ID_TRUE,
                                  sSegCache ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sIsFixedPage == ID_TRUE )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sPagePtr ) == IDE_SUCCESS );
    }
    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� �Ҵ��Ѵ�.
 *               segment ������ �ʱ� extent ������ �Ҵ��Ѵ�. ����, �����ϴٰ�
 *               �����ϴٸ� exception ó���Ѵ�.
 *
 *               [ ���� �˰��� ]
 *               alloc ext from space : alloc ext from space �������� ó���Ѵ�.
 *                                      �ݵ�� Undo �ǵ��� �Ѵ�.
 *               alloc ext to segment : alloc ext to seg �α��Ͽ� Undo �ǵ��� 
 *                                      �ϱ⳪, op null �α��Ͽ� Undo ����
 *                                      �ʵ��� �Ѵ�.
 *               create segment(DDL)  : Table/Index/Lob Segment
 *                                      OP_CREATE_XX_SEGMENT Ÿ���� NTA�� ����
 *                                      UNDO ������ �����Ѵ�.
 *
 * aStatistics - [IN]  �������
 * aMtx        - [IN]  Mini Transaction�� Pointer
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegHandle  - [IN]  Segment�� Handle
 * aSegType    - [IN]  Segment�� Type
 ***********************************************************************/
IDE_RC sdpstSegDDL::allocateSegment( idvSQL       * aStatistics,
                                     sdrMtx       * aMtx,
                                     scSpaceID      aSpaceID,
                                     sdpSegHandle * aSegHandle )
{
    sdrMtx               sMtx;
    UInt                 sLoop;
    sdpstExtDesc         sExtDesc;
    scPageID             sSegPID;
    UInt                 sState = 0;
    sdrMtxStartInfo      sStartInfo;
    sdpstBfrAllocExtInfo sBfrAllocExtInfo;
    sdpstAftAllocExtInfo sAftAllocExtInfo;
    smiSegStorageAttr  * sSegStoAttr;
    sdpstSegCache      * sSegCache;
    sdRID                sAllocExtRID;

    IDE_ASSERT( aMtx != NULL );
    IDE_ASSERT( aSegHandle != NULL );

    sSegPID    = SD_NULL_PID;

    sSegCache   = (sdpstSegCache*)(aSegHandle->mCache);
    sSegStoAttr = sdpSegDescMgr::getSegStoAttr( aSegHandle );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    IDE_ASSERT( sSegStoAttr->mInitExtCnt > 0 );

    for ( sLoop = 0; sLoop < sSegStoAttr->mInitExtCnt; sLoop++ )
    {
        sdpstExtDir::initExtDesc( &sExtDesc );
        sdpstAllocExtInfo::initialize( &sBfrAllocExtInfo );
        sdpstAllocExtInfo::initialize( &sAftAllocExtInfo );

        /* A. TBS�κ��� Extent�� �Ҵ��Ѵ�.
         * �� Extent �Ҵ��� local group ������ op NTA ó���� �Ѵ�.
         * Op NTA - alloc extents from TBS */
        IDE_TEST( sdptbExtent::allocExts( aStatistics,
                                          &sStartInfo,
                                          aSpaceID,
                                          1,       /* Extent ���� */
                                          (sdpExtDesc*)&sExtDesc )
                  != IDE_SUCCESS );


        /* Segment Header�� ������ ��� ���� Treelist ������
         * ���� ������ ������. */
        IDE_TEST( sdpstAllocExtInfo::getAllocExtInfo( aStatistics,
                                                      aSpaceID,
                                                      sSegPID,
                                                      sSegStoAttr->mMaxExtCnt,
                                                      &sExtDesc,
                                                      &sBfrAllocExtInfo,
                                                      &sAftAllocExtInfo )
                != IDE_SUCCESS );

        /* ���ο� Extent�� ���� Bitmap Page���� �����Ѵ�. */
        IDE_TEST( createMetaPages( aStatistics,
                                   &sStartInfo,
                                   aSpaceID,
                                   sSegCache,
                                   &sExtDesc,
                                   &sBfrAllocExtInfo,
                                   &sAftAllocExtInfo,
                                   &sSegPID ) != IDE_SUCCESS );

        IDE_TEST( createDataPages( aStatistics,
                                   &sStartInfo,
                                   aSpaceID,
                                   aSegHandle,
                                   &sExtDesc,
                                   &sBfrAllocExtInfo,
                                   &sAftAllocExtInfo ) != IDE_SUCCESS );



        /* mSegHdrCnt�� 0 �Ǵ� 1 ���� ����, �̹� SegHdr�� �����ϴ� ��� �׻� 0
         * �̾�� �Ѵ�. */
        if ( sAftAllocExtInfo.mSegHdrCnt == 1 )
        {
            aSegHandle->mSegPID = sSegPID;

            sdpstCache::initItHint( aStatistics,
                                    sSegCache,
                                    sSegPID,
                                    sAftAllocExtInfo.mFstPID[SDPST_ITBMP] );
        }

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       &sStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
        sState = 1;

        /* Extent ������ Segment Header �������� ExtDir ��������
         * ����ϰ�, Segment Header �������� �����Ѵ�.
         * ���� Lst BMP �������� ���� ������ bmp ���������� ������ ����Ѵ�. */
        IDE_TEST( linkNewExtToLstBMPs( aStatistics,
                                       &sMtx,
                                       aSpaceID,
                                       aSegHandle,
                                       &sExtDesc,
                                       &sBfrAllocExtInfo,
                                       &sAftAllocExtInfo ) != IDE_SUCCESS );

        /* Extent ������ Segment Header �������� ExtDir ��������
         * ����ϰ�, Segment Header �������� �����Ѵ�. */
        IDE_TEST( linkNewExtToExtDir( aStatistics,
                                      &sMtx,
                                      aSpaceID,
                                      sSegPID,
                                      &sExtDesc,
                                      &sBfrAllocExtInfo,
                                      &sAftAllocExtInfo,
                                      &sAllocExtRID ) != IDE_SUCCESS );

        /* Extent �Ҵ� ������ �Ϸ��۾����� Segment Header��
         * BMP �������� �����Ѵ�. */
        IDE_TEST( linkNewExtToSegHdr( aStatistics,
                                      &sMtx,
                                      aSpaceID,
                                      sSegPID,
                                      &sExtDesc,
                                      &sBfrAllocExtInfo,
                                      &sAftAllocExtInfo,
                                      sAllocExtRID,
                                      ID_TRUE,
                                      sSegCache ) != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    }

    sSegCache->mCommon.mPageCntInExt = sExtDesc.mLength;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    aSegHandle->mSegPID = SD_NULL_PID;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : ���ο� Extent�� ���� BMP ����
 *
 * aStatistics   - [IN] �������
 * aStartInfo    - [IN] Mtx�� StartInfo
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegCache     - [IN] Segment�� Cache
 * aExtDesc      - [IN] Extent Desc Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 * aSegPID       - [OUT] Segment ��� �������� PID
 ***********************************************************************/
IDE_RC sdpstSegDDL::createMetaPages( idvSQL               * aStatistics,
                                     sdrMtxStartInfo      * aStartInfo,
                                     scSpaceID              aSpaceID,
                                     sdpstSegCache        * aSegCache,
                                     sdpstExtDesc         * aExtDesc,
                                     sdpstBfrAllocExtInfo * aBfrInfo,
                                     sdpstAftAllocExtInfo * aAftInfo,
                                     scPageID             * aSegPID )
{
    scPageID    sSegPID;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aSpaceID   != SD_NULL_PID );
    IDE_ASSERT( aExtDesc   != NULL );
    IDE_ASSERT( aAftInfo   != NULL );

    if ( aAftInfo->mPageCnt[SDPST_LFBMP] > 0 )
    {
        IDE_TEST( sdpstLfBMP::createAndInitPages( aStatistics,
                                                  aStartInfo,
                                                  aSpaceID,
                                                  aExtDesc,
                                                  aBfrInfo,
                                                  aAftInfo ) != IDE_SUCCESS );
    }

    if ( aAftInfo->mPageCnt[SDPST_ITBMP] > 0 )
    {
        IDE_TEST( sdpstBMP::createAndInitPages( aStatistics,
                                                aStartInfo,
                                                aSpaceID,
                                                aExtDesc,
                                                SDPST_ITBMP,
                                                aBfrInfo,
                                                aAftInfo ) != IDE_SUCCESS );
    }

    if ( aAftInfo->mPageCnt[SDPST_RTBMP] > 0 )
    {
        IDE_TEST( sdpstBMP::createAndInitPages( aStatistics,
                                                aStartInfo,
                                                aSpaceID,
                                                aExtDesc,
                                                SDPST_RTBMP,
                                                aBfrInfo,
                                                aAftInfo ) != IDE_SUCCESS );
    }

    /* mSegHdrCnt�� 0 �Ǵ� 1 ���� ����, �̹� SegHdr�� �����ϴ� ��� �׻� 0
     * �̾�� �Ѵ�. */
    if ( aAftInfo->mSegHdrCnt == 1 )
    {
        IDE_TEST( sdpstSH::createAndInitPage( aStatistics,
                                              aStartInfo,
                                              aSpaceID,
                                              aExtDesc,
                                              aBfrInfo,
                                              aAftInfo,
                                              aSegCache,
                                              &sSegPID ) != IDE_SUCCESS );

        if ( aSegPID != NULL )
        {
            *aSegPID = sSegPID;
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}
 
/***********************************************************************
 * Description : ���ο� Extent�� ���� BMP ����
 *
 * aStatistics   - [IN] �������
 * aStartInfo    - [IN] Mtx�� StartInfo
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegCache     - [IN] Segment�� Cache
 * aExtDesc      - [IN] Extent Desc Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 ***********************************************************************/
IDE_RC sdpstSegDDL::createDataPages( idvSQL               * aStatistics,
                                     sdrMtxStartInfo      * aStartInfo,
                                     scSpaceID              aSpaceID,
                                     sdpSegHandle         * aSegHandle,
                                     sdpstExtDesc         * aExtDesc,
                                     sdpstBfrAllocExtInfo * aBfrInfo,
                                     sdpstAftAllocExtInfo * aAftInfo )
{
    scPageID              sFstDataPID;
    scPageID              sLstDataPID;
    scPageID              sCurPID;
    scPageID              sLfBMP;
    scPageID              sPrvLfBMP;
    SShort                sPBSNo;
    sdpstPBS              sCreatePBS;
    sdrMtx                sMtx;
    UInt                  sState = 0 ;
    UChar               * sPagePtr;
    SShort                sLfBMPIdx;
    UInt                  sMetaPageCnt;
    ULong                 sDataSeqNo;

    sCreatePBS = (SDPST_BITSET_PAGETP_DATA | SDPST_BITSET_PAGEFN_FMT);

    sMetaPageCnt = aAftInfo->mPageCnt[SDPST_EXTDIR] +
                   aAftInfo->mPageCnt[SDPST_LFBMP] +
                   aAftInfo->mPageCnt[SDPST_ITBMP] +
                   aAftInfo->mPageCnt[SDPST_RTBMP] +
                   aAftInfo->mSegHdrCnt;

    /* fst, lst PID ��� */
    sFstDataPID = aExtDesc->mExtFstPID + sMetaPageCnt;
    sLstDataPID = aExtDesc->mExtFstPID + aExtDesc->mLength - 1;

    /* ù��° ������ �������� ���� SeqNo */
    sDataSeqNo = aBfrInfo->mNxtSeqNo + sMetaPageCnt;

    if ( aAftInfo->mPageCnt[SDPST_LFBMP] == 0 )
    {
        /* ���� ������ ������ LfBMP�� �Ҵ��� Extent��
         * ��� �� �� �ִ� ��� */
        sLfBMP = aBfrInfo->mLstPID[SDPST_LFBMP];
        sPBSNo = aBfrInfo->mLstPBSNo + sFstDataPID - aExtDesc->mExtFstPID + 1;

        for ( sCurPID = sFstDataPID; sCurPID <= sLstDataPID; sCurPID++ )
        {
            sState = 0;
            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           &sMtx,
                                           aStartInfo,
                                           ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                           SM_DLOG_ATTR_DEFAULT )
                      != IDE_SUCCESS );
            sState = 1;

            IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                                  &sMtx,
                                                  aSpaceID,
                                                  aSegHandle,
                                                  sCurPID,
                                                  sDataSeqNo,
                                                  SDP_PAGE_FORMAT,
                                                  sLfBMP,
                                                  sPBSNo,
                                                  sCreatePBS,
                                                  &sPagePtr ) != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            sPBSNo++;
            sDataSeqNo++;
        }
    }
    else
    {
        /* �� �Ҵ��� Extent�� ���� ������ LfBMP�� ���� ���. */
        sPBSNo = sFstDataPID - aExtDesc->mExtFstPID;
        sPrvLfBMP = aExtDesc->mExtFstPID + aAftInfo->mPageCnt[SDPST_EXTDIR];

        for ( sCurPID = sFstDataPID; sCurPID <= sLstDataPID; sCurPID++ )
        {
            /* ���° LfBMP�� ��ġ�ϴ��� ����Ѵ�. */
            sLfBMPIdx = (sCurPID - aExtDesc->mExtFstPID) /
                        aAftInfo->mPageRange;
            sLfBMP    = aExtDesc->mExtFstPID +
                        aAftInfo->mPageCnt[SDPST_EXTDIR] +
                        sLfBMPIdx;

            if ( sPrvLfBMP != sLfBMP )
            {
                /* BUG-42820 leaf ���� page range �������� �̹� ���Ǿ� �ִ�.
                 * leaf node������ �����ϰ� sPBSNo�� �ٽ� ����Ѵ�. */
                sPBSNo = ( sCurPID - aExtDesc->mExtFstPID ) % aAftInfo->mPageRange;
            }

            sState = 0;
            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           &sMtx,
                                           aStartInfo,
                                           ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                           SM_DLOG_ATTR_DEFAULT )
                      != IDE_SUCCESS );
            sState = 1;

            IDE_TEST( sdpstAllocPage::createPage( aStatistics,
                                                  &sMtx,
                                                  aSpaceID,
                                                  aSegHandle,
                                                  sCurPID,
                                                  sDataSeqNo,
                                                  SDP_PAGE_FORMAT,
                                                  sLfBMP,
                                                  sPBSNo,
                                                  sCreatePBS,
                                                  &sPagePtr ) != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            sPBSNo++;
            sPrvLfBMP = sLfBMP;
            sDataSeqNo++;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� Lst Bitmap �������� ���� ������ BMP ����������
 *               ���� ��� ���� bmp �������� ����ϴ� ������ DDL ���н�
 *               Ʈ����� Undo�� �ʿ��ϴ�.
 *
 * aStatistics   - [IN] �������
 * aMtx          - [IN] Mtx�� Pointer
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegHandle    - [IN] Segment�� Handle
 * aExtDesc      - [IN] Extent Desc Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 ***********************************************************************/
IDE_RC sdpstSegDDL::linkNewExtToLstBMPs( idvSQL                * aStatistics,
                                         sdrMtx                * aMtx,
                                         scSpaceID               aSpaceID,
                                         sdpSegHandle          * aSegHandle,
                                         sdpstExtDesc          * aExtDesc,
                                         sdpstBfrAllocExtInfo  * aBfrInfo,
                                         sdpstAftAllocExtInfo  * aAftInfo )
{
    UChar             * sPagePtr;
    sdpstLfBMPHdr     * sLfBMPHdr;
    sdpstBMPHdr       * sBMPHdr;
    scPageID            sFstPID;
    scPageID            sLstPID;
    UShort              sFullLfBMPCnt;
    UShort              sFullItBMPCnt;
    idBool              sNeedToChangeMFNL;
    sdpstMFNL           sNewMFNL;
    sdpstStack          sRevStack;

    IDE_ASSERT( aSegHandle    != NULL );
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aExtDesc      != NULL );
    IDE_ASSERT( aBfrInfo != NULL );

    /* lf-bmp �������� �������� �ʰ�, ���� lf-bmp�� �߰��ϴ� ��� */
    if ( aAftInfo->mPageCnt[SDPST_LFBMP] == 0 )
    {
        /* lf-bmp �������� �������� ���������� last lf-bmp��
         * Range slot�� �ִ´�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              aBfrInfo->mLstPID[SDPST_LFBMP],
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPagePtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );

        sLfBMPHdr = sdpstLfBMP::getHdrPtr( sPagePtr );

        if ( aAftInfo->mPageCnt[SDPST_EXTDIR] > 0 )
        {
            /* ���Ŀ� ExtDir �������� �����ɰ��̰�, ���⼭ �̸� ExtDir
             * �� ���� ó���� �����Ѵ�.
             * Lf-BMP �������� ��ǥ MFNL�� ����� �� ����. �ֳ��ϸ�,
             * Lf-BMP �����Ҵ����� �ʾҰ�, ���� Lf-BMP�� �߰��߱� ������
             * ���� Lf-BMP �������� MFNL�� ��� UNF�����̱� �����̴�. */
            IDE_ASSERT( aAftInfo->mLfBMP4ExtDir ==
                        aBfrInfo->mLstPID[SDPST_LFBMP] );
        }

        /* lf-bmp �������� Extent�� Range Slot�� ����Ѵ�. */
        IDE_TEST( sdpstLfBMP::logAndAddPageRangeSlot(
                      aMtx,
                      sLfBMPHdr,
                      aExtDesc->mExtFstPID,
                      aExtDesc->mLength,
                      aAftInfo->mPageCnt[SDPST_EXTDIR],
                      aBfrInfo->mLstPID[SDPST_EXTDIR],
                      aBfrInfo->mSlotNoInExtDir,
                      &sNeedToChangeMFNL,
                      &sNewMFNL ) != IDE_SUCCESS );

        /* ExtDesc�� ���� Lf bmp�� Range Slot�� �߰��Ǵ� ����̹Ƿ�,
         * ExtDesc�� �����ϴ� lf-bmp�� last lf-bmp�̰�, ù��° data
         * page�� ExtDir �������� ����ؼ� �����ؾ��Ѵ�. */

        /* Extent�� �Ҵ��ϸ�, �켱������ Page���� ��ġ�ϰ� �ǰ�, ���Ŀ�
         * Data ������ �뵵�� �Ҵ�ǰ� �ȴ�. �׷��Ƿ�, ù��° lf-bmp��
         * �������� PID�� ExtDesc�� ����صθ� Sequential Scan�ÿ� ����
         * lf-bmp�� �����ϰ��� �Ҷ�, �ٷ� ���� �������� �����غ��� �ȴ�. */
        aExtDesc->mExtMgmtLfBMP  = aBfrInfo->mLstPID[SDPST_LFBMP];
        aExtDesc->mExtFstDataPID = aExtDesc->mExtFstPID +
                                   aAftInfo->mPageCnt[SDPST_EXTDIR];

        // ������ lf-bmp �������� parent it-bmp�� last it-bmp�̴�.
        IDE_ASSERT( aBfrInfo->mLstPID[SDPST_ITBMP] ==
                    sLfBMPHdr->mBMPHdr.mParentInfo.mParentPID);

        if ( sNeedToChangeMFNL == ID_TRUE )
        {
            /* ItBMP �������� lf-slot MFNL�� ����õ��Ͽ� ��������
             * �����Ѵ�. �ֳ��ϸ�, leaf bmp�� MFNL�� ����Ǿ��� �����̴�. */
            sdpstStackMgr::initialize( &sRevStack );
            IDE_TEST( sdpstAllocPage::tryToChangeMFNLAndItHint(
                          aStatistics,
                          aMtx,
                          aSpaceID,
                          aSegHandle,
                          SDPST_CHANGEMFNL_ITBMP_PHASE,
                          aBfrInfo->mLstPID[SDPST_LFBMP],  // child pid
                          SDPST_INVALID_PBSNO,             // PBSNo for lf
                          (void*)&sNewMFNL,
                          aBfrInfo->mLstPID[SDPST_ITBMP],
                          sLfBMPHdr->mBMPHdr.mParentInfo.mIdxInParent,
                          &sRevStack ) != IDE_SUCCESS );
        }
    }
    else
    {
        /* ���� it-bmp �������� ���� ������ lf-bmp ���������� ����Ѵ�.
         * ���� it-bmp�� ����ϹǷ� undo�� ���� ����� �ؾ��Ѵ�. */
        if ( (aAftInfo->mPageCnt[SDPST_LFBMP] > 0) &&
             (aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] > 0) )
        {
            // ���� it-bmp �������� ��ϵ� ù��° lf-bmp �������� PID
            sFstPID = aExtDesc->mExtFstPID + aAftInfo->mPageCnt[SDPST_EXTDIR];

            // ���� it-bmp �������� ��ϵ� ������ lf-bmp �������� PID
            sLstPID = sFstPID +
                (aAftInfo->mPageCnt[SDPST_LFBMP] >=
                    aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] ?
                 aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] :
                 aAftInfo->mPageCnt[SDPST_LFBMP]) - 1;

            /* LfBMP�� MFNL�� ó������ FULL�ΰ�찡 �ִ�.
             * ( Lfbmp �������� ��� page�� �ִ� ��� ) */
            sFullLfBMPCnt = (aAftInfo->mFullBMPCnt[SDPST_LFBMP] <=
                                aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] ?
                             aAftInfo->mFullBMPCnt[SDPST_LFBMP] :
                             aBfrInfo->mFreeSlotCnt[SDPST_ITBMP]);

            IDE_TEST( sdbBufferMgr::getPageByPID(
                                            aStatistics,
                                            aSpaceID,
                                            aBfrInfo->mLstPID[SDPST_ITBMP],
                                            SDB_X_LATCH,
                                            SDB_WAIT_NORMAL,
                                            SDB_SINGLE_PAGE_READ,
                                            aMtx,
                                            &sPagePtr,
                                            NULL, /* aTrySuccess */
                                            NULL  /* aIsCorruptPage */ )
                      != IDE_SUCCESS );

            sBMPHdr = sdpstBMP::getHdrPtr( sPagePtr );

            /* ���� ������ ItBMP �������� ���ο� slot�� ����Ѵ�. */
            IDE_TEST( sdpstBMP::logAndAddSlots( aMtx,
                                                sBMPHdr,
                                                sFstPID,
                                                sLstPID,
                                                sFullLfBMPCnt,
                                                &sNeedToChangeMFNL,
                                                &sNewMFNL ) != IDE_SUCCESS );

            if ( sNeedToChangeMFNL == ID_TRUE )
            {
                /* RtBMP�������� it-slot MFNL�� ����õ��Ͽ� ����
                 * ���� �����Ѵ�. */
                sdpstStackMgr::initialize( &sRevStack );

                IDE_TEST( sdpstAllocPage::tryToChangeMFNLAndItHint(
                              aStatistics,
                              aMtx,
                              aSpaceID,
                              aSegHandle,
                              SDPST_CHANGEMFNL_RTBMP_PHASE,
                              aBfrInfo->mLstPID[SDPST_ITBMP], // child pid
                              SDPST_INVALID_PBSNO,            // unuse
                              (void*)&sNewMFNL,
                              sBMPHdr->mParentInfo.mParentPID,
                              sBMPHdr->mParentInfo.mIdxInParent,
                              &sRevStack ) != IDE_SUCCESS );
            }
        }

        /* ���� rt-bmp �������� ���� ������ it-bmp ���������� ����Ѵ�.
         * ���� rt-bmp�� ����ϹǷ� undo�� ���� ����� �ؾ��Ѵ�. */
        if ( (aAftInfo->mPageCnt[SDPST_ITBMP] > 0) &&
             (aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] > 0) )
        {
            // ���� rt-bmp �������� ��ϵ� ù��° it-bmp �������� PID
            sFstPID = aExtDesc->mExtFstPID + aAftInfo->mPageCnt[SDPST_EXTDIR] +
                      aAftInfo->mPageCnt[SDPST_LFBMP];

            // ���� rt-bmp �������� ��ϵ� ������ it-bmp �������� PID
            sLstPID = sFstPID +
                (aAftInfo->mPageCnt[SDPST_ITBMP] >=
                    aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] ?
                 aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] :
                 aAftInfo->mPageCnt[SDPST_ITBMP]) - 1;

            sFullItBMPCnt =
                (aAftInfo->mFullBMPCnt[SDPST_ITBMP] <=
                    aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] ?
                 aAftInfo->mFullBMPCnt[SDPST_ITBMP] :
                 aBfrInfo->mFreeSlotCnt[SDPST_RTBMP]);

            IDE_TEST( sdbBufferMgr::getPageByPID(
                                            aStatistics,
                                            aSpaceID,
                                            aBfrInfo->mLstPID[SDPST_RTBMP],
                                            SDB_X_LATCH,
                                            SDB_WAIT_NORMAL,
                                            SDB_SINGLE_PAGE_READ,
                                            aMtx,
                                            &sPagePtr,
                                            NULL, /* aTrySuccess */
                                            NULL  /* aIsCorruptPage */ )
                      != IDE_SUCCESS );

            sBMPHdr = sdpstBMP::getHdrPtr( sPagePtr );

            /* RtBMP �������� slot�� ����Ѵ�.
             * freeslots�� free slotno ���� �������ش�. */
            IDE_TEST( sdpstBMP::logAndAddSlots( aMtx,
                                                sBMPHdr,
                                                sFstPID,
                                                sLstPID,
                                                sFullItBMPCnt,
                                                &sNeedToChangeMFNL,
                                                &sNewMFNL ) != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���ο� �ϳ��� Extent�� Segment�� �Ҵ��ϴ� ������ �Ϸ�
 *
 * Segment Header �������� last bmps �� total ������ ����
 * Tot Extent ���� �׸���, extent slot�� ����Ѵ�.
 *
 * aStatistics   - [IN] �������
 * aMtx          - [IN] Mtx�� Pointer
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegPID       - [IN] Segment Header�� PID
 * aExtDesc      - [IN] Extent Slot Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 * aAllocExtRID  - [IN] �Ҵ޵� Extent�� RID
 ***********************************************************************/
IDE_RC sdpstSegDDL::linkNewExtToExtDir( idvSQL                * aStatistics,
                                        sdrMtx                * aMtx,
                                        scSpaceID               aSpaceID,
                                        scPageID                aSegPID,
                                        sdpstExtDesc          * aExtDesc,
                                        sdpstBfrAllocExtInfo  * aBfrInfo,
                                        sdpstAftAllocExtInfo  * aAftInfo,
                                        sdRID                 * aAllocExtRID )
{

    UChar              * sSegHdrPtr;
    UChar              * sLstExtDirPtr;
    scPageID             sLstExtDirPID;
    UShort               sFreeExtSlotCnt;
    sdpstSegHdr        * sSegHdr;
    sdpstExtDirHdr     * sExtDirHdr;

    IDE_ASSERT( aExtDesc != NULL );
    IDE_ASSERT( aAftInfo != NULL );
    IDE_ASSERT( aMtx     != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sSegHdrPtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr       = sdpstSH::getHdrPtr( sSegHdrPtr );
    sLstExtDirPID = sdpstSH::getLstExtDir( sSegHdr );

    if ( sLstExtDirPID == aSegPID )
    {
        sExtDirHdr      = sdpstExtDir::getHdrPtr( sSegHdrPtr );
        sFreeExtSlotCnt = sdpstExtDir::getFreeSlotCnt( sExtDirHdr );
        sLstExtDirPtr   = NULL;
    }
    else
    {
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sLstExtDirPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sLstExtDirPtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );

        sExtDirHdr      = sdpstExtDir::getHdrPtr( sLstExtDirPtr );
        sFreeExtSlotCnt = sdpstExtDir::getFreeSlotCnt( sExtDirHdr );
    }

    if ( sFreeExtSlotCnt == 0 )
    {
        IDE_ASSERT( aAftInfo->mPageCnt[SDPST_EXTDIR] > 0 );

        /* ����, extent�� page�� �����Ǿ� ���� ���� �����ϱ�
         * ù��° data �������� ExtDir �������� �����ϰ�,
         * ù���� data �������� �����Ѵ�. */
        IDE_TEST( sdpstExtDir::createAndInitPage(
                                        aStatistics,
                                        aMtx,
                                        aSpaceID,
                                        aExtDesc,
                                        aBfrInfo,
                                        aAftInfo,
                                        &sLstExtDirPtr ) != IDE_SUCCESS );

        /* Segment Header�� ������ ExtDir �������� �����Ѵ�.
         * sExtDirHdr ������ ������ ���̶� �ݵ�� sExtDirHdr Page�� 
         * X-Latch �����־�� �Ѵ�. */
        IDE_TEST( sdpstSH::addNewExtDirPage( aMtx,
                                             sSegHdrPtr,
                                             sLstExtDirPtr ) != IDE_SUCCESS );

        sExtDirHdr = sdpstExtDir::getHdrPtr( sLstExtDirPtr );
    }

    /* ������ ExtDir�� ����Ѵ�. */
    IDE_TEST( sdpstExtDir::logAndAddExtDesc( aMtx,
                                             sExtDirHdr,
                                             aExtDesc,
                                             aAllocExtRID )
              != IDE_SUCCESS );


    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ BMP���� Segment Header�� �����Ѵ�.
 *
 *
 * aStatistics   - [IN] �������
 * aMtx          - [IN] Mtx�� Pointer
 * aSpaceID      - [IN] Tablespace�� ID
 * aSegPID       - [IN] Segment Header�� PID
 * aExtDesc      - [IN] ExtDesc Pointer
 * aBfrInfo      - [IN] Extent ���� �Ҵ� ����
 * aAftInfo      - [IN] Extent ���� �Ҵ� ����
 * aAllocExtRID  - [IN] �Ҵ��� Extent�� RID
 * aNeedToUpdateHWM - [IN] HWM ���� ����
 * aSegCache     - [IN] HWM �����ϱ� ���� ���
 ***********************************************************************/
IDE_RC sdpstSegDDL::linkNewExtToSegHdr( idvSQL               * aStatistics,
                                        sdrMtx               * aMtx,
                                        scSpaceID              aSpaceID,
                                        scPageID               aSegPID,
                                        sdpstExtDesc         * aExtDesc,
                                        sdpstBfrAllocExtInfo * aBfrInfo,
                                        sdpstAftAllocExtInfo * aAftInfo,
                                        sdRID                  aAllocExtRID,
                                        idBool                 aNeedToUpdateHWM,
                                        sdpstSegCache        * aSegCache)
{
    UChar       * sPagePtr;
    sdpstSegHdr * sSegHdr;
    scPageID      sBfrLstRtBMP;
    sdpstStack    sHWMStack;
    SShort        sSlotNoInRtBMP;
    SShort        sSlotNoInItBMP;
    SShort        sPBSNo;
    ULong         sMetaPageCnt;


    IDE_ASSERT( aSegPID  != SD_NULL_PID );
    IDE_ASSERT( aAftInfo != NULL );

    sMetaPageCnt = aAftInfo->mPageCnt[SDPST_EXTDIR] +
                   aAftInfo->mPageCnt[SDPST_LFBMP] +
                   aAftInfo->mPageCnt[SDPST_ITBMP] +
                   aAftInfo->mPageCnt[SDPST_RTBMP] +
                   aAftInfo->mSegHdrCnt;

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
              != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    /*
     * BUG-22474     [valgrind]sdbMPRMgr::getMPRCnt�� UMR�ֽ��ϴ�.
     */
    IDE_TEST( sdpstSH::setLstExtRID( aMtx,
                                     sSegHdr,
                                     aAllocExtRID ) != IDE_SUCCESS );


    IDE_TEST( sdpstSH::logAndLinkBMPs( aMtx,
                                       sSegHdr,
                                       aAftInfo->mTotPageCnt -
                                           aBfrInfo->mTotPageCnt,
                                       sMetaPageCnt,
                                       aAftInfo->mPageCnt[SDPST_RTBMP],
                                       aAftInfo->mLstPID[SDPST_RTBMP],
                                       aAftInfo->mLstPID[SDPST_ITBMP],
                                       aAftInfo->mLstPID[SDPST_LFBMP],
                                       &sBfrLstRtBMP )
              != IDE_SUCCESS );

    /* SegCache�� format page count ���� */
    aSegCache->mFmtPageCnt = aAftInfo->mTotPageCnt;

    /* RtBMP �� �����Ǿ��ٸ� ������ RtBMP�� NxtRtBMP�� �������ش�. */
    if ( aAftInfo->mPageCnt[SDPST_RTBMP] > 0 )
    {
        IDE_TEST( sdpstRtBMP::setNxtRtBMP( aStatistics,
                                           aMtx,
                                           aSpaceID,
                                           sBfrLstRtBMP,
                                           aAftInfo->mFstPID[SDPST_RTBMP] )
                != IDE_SUCCESS );
    }

    /* HWM�� �����ϱ� ���� SlotNo�� ����Ѵ�. */
    if ( aNeedToUpdateHWM == ID_TRUE )
    {
        if ( aAftInfo->mPageCnt[SDPST_RTBMP] > 0 )
        {
            sSlotNoInRtBMP = aAftInfo->mPageCnt[SDPST_ITBMP] -
                             aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] - 1;
        }
        else
        {
            sSlotNoInRtBMP = ( ( aBfrInfo->mMaxSlotCnt[SDPST_RTBMP] -
                                 aBfrInfo->mFreeSlotCnt[SDPST_RTBMP] ) +
                               aAftInfo->mPageCnt[SDPST_ITBMP] ) - 1;
        }

        if ( aAftInfo->mPageCnt[SDPST_ITBMP] > 0 )
        {
            sSlotNoInItBMP = aAftInfo->mPageCnt[SDPST_LFBMP] - 
                             aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] - 1;
        }
        else
        {
            sSlotNoInItBMP = ( ( aBfrInfo->mMaxSlotCnt[SDPST_ITBMP] -
                                 aBfrInfo->mFreeSlotCnt[SDPST_ITBMP] ) +
                               aAftInfo->mPageCnt[SDPST_LFBMP] ) - 1;
        }

        if ( aAftInfo->mPageCnt[SDPST_LFBMP] > 0 )
        {
            sPBSNo = (aExtDesc->mLength - 1) % aAftInfo->mPageRange;
        }
        else
        {
            sPBSNo = aBfrInfo->mLstPBSNo + aExtDesc->mLength;
        }

        sdpstStackMgr::initialize( &sHWMStack );
        sdpstStackMgr::push( &sHWMStack, SD_NULL_PID, sSegHdr->mTotRtBMPCnt );
        sdpstStackMgr::push( &sHWMStack, sSegHdr->mLstRtBMP, sSlotNoInRtBMP );
        sdpstStackMgr::push( &sHWMStack, sSegHdr->mLstItBMP, sSlotNoInItBMP );
        sdpstStackMgr::push( &sHWMStack, sSegHdr->mLstLfBMP, sPBSNo );

        IDE_TEST( sdpstSH::logAndUpdateWM( aMtx,
                                           &sSegHdr->mHWM,
                                           aExtDesc->mExtFstPID +
                                           aExtDesc->mLength - 1,
                                           aAftInfo->mLstPID[SDPST_EXTDIR],
                                           aAftInfo->mSlotNoInExtDir,
                                           &sHWMStack ) != IDE_SUCCESS );
        sdpstCache::lockHWM4Write( aStatistics, aSegCache );
        aSegCache->mHWM = sSegHdr->mHWM;
        sdpstCache::unlockHWM( aSegCache );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : [INTERFACE] Segment�� 1���̻��� Extent�� �Ҵ��Ѵ�
 *
 * aStatistics  - [IN] �������
 * aStartInfo   - [IN] Mtx�� StartInfo
 * aSpaceID     - [IN] Tablespace�� ID
 * aSegHandle   - [IN] Segment�� Handle
 * aExtCnt      - [IN] �Ҵ��� Extent ����
 ***********************************************************************/
IDE_RC sdpstSegDDL::allocateExtents( idvSQL           * aStatistics,
                                     sdrMtxStartInfo  * aStartInfo,
                                     scSpaceID          aSpaceID,
                                     sdpSegHandle     * aSegHandle,
                                     UInt               aExtCnt )
{
    sdRID   sAllocFstExtRID;

    IDE_ASSERT( aStartInfo  != NULL );
    IDE_ASSERT( aExtCnt > 0 );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aSegHandle->mSegPID != SD_NULL_PID );

    return allocNewExtsAndPage( aStatistics,
                                aStartInfo,
                                aSpaceID,
                                aSegHandle,
                                aExtCnt,
                                ID_TRUE, /* sNeedToFormatPage */
                                &sAllocFstExtRID,
                                NULL,    /* aMtx */
                                NULL     /* aFstDataPage */ );
}



/***********************************************************************
 * Description : Segment�� 1���̻��� Extent�� �Ҵ��Ѵ�.
 *               # DML/DDL
 *               Segment�� ����� �Ҵ�� Extent���� NTA logging �Ͽ�
 *               Undo���� �ʵ��� ó���Ѵ�. ������ �ϴ��ϰ� �Ǹ�,
 *               �ٸ� DML Ʈ������� Ȯ��� ������ ����� �� �ֱ� ������
 *               Undo �Ǿ�� �ȵȴ�.
 *
 * aStatistics        - [IN]  �������
 * aStartInfo         - [IN]  Mtx ��������
 * aSpaceID           - [IN]  Segment�� ���̺����̽�
 * aSegHandle         - [IN]  Segment Handle
 * aExtCnt            - [IN]  �Ҵ��� Extent ����
 * aNeedToFormatPage  - [IN]  Extent �Ҵ� �� HWM ���ſ��� �÷���
 * aAllocFstExtRID    - [OUT] �Ҵ���� ù��° Extent RID
 *                         �߿�: �� output ������ DPath������ ���ȴ�.
 *                              ���� �ٸ� �뵵�� ���� Ʈ����ǿ��� ���ÿ�
 *                              ����ϸ�, ���ü� ������ �ִ�.
 *                              ���� �����ؼ� ����ؾ� �Ѵ�.
 * aMtx4Logging       - [OUT]
 * aFstDataPagePtr    - [OUT]
 ***********************************************************************/
IDE_RC sdpstSegDDL::allocNewExtsAndPage( idvSQL           * aStatistics,
                                         sdrMtxStartInfo  * aStartInfo,
                                         scSpaceID          aSpaceID,
                                         sdpSegHandle     * aSegHandle,
                                         UInt               aExtCnt,
                                         idBool             aNeedToFormatPage,
                                         sdRID            * aAllocFstExtRID,
                                         sdrMtx           * aMtx,
                                         UChar           ** aFstDataPage )
{
    sdrMtx                 sMtx;
    UInt                   sLoop;
    UInt                   sState = 0;
    sdpstExtDesc           sExtDesc;
    smLSN                  sOpNTA;
    sdpstBfrAllocExtInfo   sBfrAllocExtInfo;
    sdpstAftAllocExtInfo   sAftAllocExtInfo;
    scPageID               sSegPID;
    idBool                 sIsExtendExt;
    idBool                 sNeedToUpdateHWM = ID_FALSE;
    idBool                 sNeedToExtendExt;
    sdpstSegCache        * sSegCache;
    sdRID                  sAllocExtRID;
    ULong                  sData[ SM_DISK_NTALOG_DATA_COUNT ];

    IDE_DASSERT( aExtCnt              > 0 );
    IDE_DASSERT( aStartInfo          != NULL );
    IDE_DASSERT( aSegHandle          != NULL );
    IDE_DASSERT( aSegHandle->mSegPID != SD_NULL_PID );
    IDE_DASSERT( aAllocFstExtRID     != NULL );

    sSegPID      = aSegHandle->mSegPID;
    sSegCache    = (sdpstSegCache*)(aSegHandle->mCache);
    sIsExtendExt = ID_FALSE;

    if ( aFstDataPage != NULL )
    {
        *aFstDataPage = NULL;
    }

    // �ٸ� Ʈ����ǰ��� ���ü� ��� ���� ȹ��
    // ���ÿ� Ȯ���û�� ���ؼ� �ߺ� Ȯ������ �ʰ�, �ڴʰ� ���� Ʈ�������
    // �ռ� Ȯ��� Extent�� ����� �� �ֵ��� �Ѵ�.
    IDE_ASSERT( sdpstCache::prepareExtendExtOrWait( aStatistics,
                                                    sSegCache,
                                                    &sIsExtendExt )
                == IDE_SUCCESS );

    IDE_TEST_CONT( sIsExtendExt == ID_FALSE, already_extended );
    sState = 1;

    if ( aNeedToFormatPage == ID_TRUE )
    {
        IDE_TEST( tryToFormatPageAfterHWM( aStatistics,
                                           aStartInfo,
                                           aSpaceID,
                                           aSegHandle,
                                           aAllocFstExtRID,
                                           aMtx,
                                           aFstDataPage,
                                           &sNeedToExtendExt ) != IDE_SUCCESS );

        IDE_TEST_CONT( sNeedToExtendExt == ID_FALSE, skip_extend );
    }

    // Extent�鿡 ���� bmp ���������� �����Ѵ�.
    for( sLoop = 0; sLoop < aExtCnt; sLoop++ )
    {
        sdpstExtDir::initExtDesc( &sExtDesc );
        sdpstAllocExtInfo::initialize( &sBfrAllocExtInfo );
        sdpstAllocExtInfo::initialize( &sAftAllocExtInfo );

        if ( aStartInfo->mTrans != NULL )
        {
            sOpNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );
        }
        else
        {
             /* Temporary Table �����ÿ��� Ʈ������� NULL��
              * ������ �� �ִ�. */
        }

        // A. TBS�κ��� Extent�� �Ҵ��Ѵ�.
        // �� Extent �Ҵ��� local group ������ op NTA ó���� �Ѵ�.
        // Op NTA - alloc extents from TBS
        IDE_TEST( sdptbExtent::allocExts( aStatistics,
                                          aStartInfo,
                                          aSpaceID,
                                          1,
                                          (sdpExtDesc*)&sExtDesc )
                != IDE_SUCCESS );

        /* mSegHdrCnt�� 0 �Ǵ� 1 ���� ����, �̹� SegHdr�� �����ϴ� ��� �׻� 0
         * �̾�� �Ѵ�. */
        IDE_ASSERT( sAftAllocExtInfo.mSegHdrCnt == 0 );


        /* Segment Header�κ��� BMPs ������ �ǵ��Ѵ�. */
        IDE_TEST( sdpstAllocExtInfo::getAllocExtInfo(
                                        aStatistics,
                                        aSpaceID,
                                        sSegPID,
                                        aSegHandle->mSegStoAttr.mMaxExtCnt,
                                        &sExtDesc,
                                        &sBfrAllocExtInfo,
                                        &sAftAllocExtInfo ) != IDE_SUCCESS );

        /* ���ο� Extent�� ���� Bitmap Page���� �����Ѵ�.
         * aSegType�� SDP_SEG_TYPE_NONE���� �Ѱ��ִµ�, allocNewExts ������
         * Segment Header�� �������� �ʱ� ������ aSegType �� �ǹ̾���. */
        IDE_ASSERT( sAftAllocExtInfo.mSegHdrCnt == 0 );
        IDE_TEST( createMetaPages( aStatistics,
                                   aStartInfo,
                                   aSpaceID,
                                   sSegCache,
                                   &sExtDesc,
                                   &sBfrAllocExtInfo,
                                   &sAftAllocExtInfo,
                                   NULL ) != IDE_SUCCESS );

        /* BUG-29299 TC/Server/sm4/Bugs/BUG-28723/recovery.sql
         *           ����� ���� insert�� �����Ͱ� ������
         * 
         * DPath ����� aNeedToFormatPage �÷����� TRUE�� �Ѿ���µ�,
         * �� ��� Data Page�� ���ؼ� format�� �������� �ʴ´�. */
        if ( aNeedToFormatPage == ID_TRUE )
        {
            IDE_TEST( createDataPages( aStatistics,
                                       aStartInfo,
                                       aSpaceID,
                                       aSegHandle,
                                       &sExtDesc,
                                       &sBfrAllocExtInfo,
                                       &sAftAllocExtInfo ) != IDE_SUCCESS );
            sNeedToUpdateHWM = ID_TRUE;
        }

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
        sState = 2;

        /* Extent ������ Segment Header �������� ExtDir ��������
         * ����ϰ�, Segment Header �������� �����Ѵ�.
         * ���� Lst BMP �������� ���� ������ bmp ���������� ������ ����Ѵ�. */
        IDE_TEST( linkNewExtToLstBMPs( aStatistics,
                                       &sMtx,
                                       aSpaceID,
                                       aSegHandle,
                                       &sExtDesc,
                                       &sBfrAllocExtInfo,
                                       &sAftAllocExtInfo ) != IDE_SUCCESS );

        /* Extent ������ Segment Header �������� ExtDir ��������
         * ����ϰ�, Segment Header �������� �����Ѵ�. */
        IDE_TEST( linkNewExtToExtDir( aStatistics,
                                      &sMtx,
                                      aSpaceID,
                                      sSegPID,
                                      &sExtDesc,
                                      &sBfrAllocExtInfo,
                                      &sAftAllocExtInfo,
                                      &sAllocExtRID ) != IDE_SUCCESS );

        /* Extent �Ҵ� ������ �Ϸ��۾����� Segment Header��
         * BMP �������� �����Ѵ�. */
        IDE_TEST( linkNewExtToSegHdr( aStatistics,
                                      &sMtx,
                                      aSpaceID,
                                      sSegPID,
                                      &sExtDesc,
                                      &sBfrAllocExtInfo,
                                      &sAftAllocExtInfo,
                                      sAllocExtRID,
                                      sNeedToUpdateHWM,
                                      sSegCache ) != IDE_SUCCESS );

        // OnDemand �ÿ��� Undo ���� ���ϵ��� ��� NTA ó���Ѵ�.
        sData[0] = (ULong)sSegPID;
        sdrMiniTrans::setNTA( &sMtx,
                              aSpaceID,
                              SDR_OP_NULL,
                              &sOpNTA,
                              &sData[0],
                              1 /* Data Cnt */ );

        if ( sLoop == 0 )
        {
            *aAllocFstExtRID = sAllocExtRID;

            /*
             * [BUG-29137] [SM] [PROJ-2037] sdpstFindPage::tryToAllocExtsAndPage����
             *             ���ÿ� ������ �Ҵ��� �߻��Ҽ� �ֽ��ϴ�.
             */
            if( aFstDataPage != NULL )
            {
                IDE_ASSERT( aMtx != NULL );
                IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                                      aSpaceID,
                                                      sExtDesc.mExtFstDataPID,
                                                      SDB_X_LATCH,
                                                      SDB_WAIT_NORMAL,
                                                      SDB_SINGLE_PAGE_READ,
                                                      aMtx,
                                                      aFstDataPage,
                                                      NULL, /* aTrySuccess */
                                                      NULL  /* aIsCorruptPage */ )
                          != IDE_SUCCESS );
            }
        }


        sState = 1;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    }

    IDE_EXCEPTION_CONT( skip_extend );


    // Ȯ��Ϸ��Ŀ� ExtendExt Mutex�� �����ϸ鼭 ����ϴ� Ʈ������� �����.
    sState = 0;
    IDE_ASSERT( sdpstCache::completeExtendExtAndWakeUp(
                                            aStatistics,
                                            sSegCache ) == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( already_extended );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* Segment Cache�� ExtendExt Mutex�� ȹ���� ���¿��� Ʈ�������
     * Ʈ������� Extent�� 2���̻��� Ȯ���Ҷ�, 1���� �Ҵ��Ͽ�
     * Segment ������ ������ �ٸ� Ʈ������� �� Extent�� ���Եȴ�.
     * �׸��� ����� ���Ȱ�, �� ���Ŀ� Ȯ�忬���� Undo�� �ȴٸ�,
     * �ٸ� Ʈ������� ����� �������� ��ȯ�Ǿ� ������.
     *
     * ���� ���� ������ �����ϱ� ���ؼ��� �̹� Ȯ���߰��� Segment
     * �� ����� Extent�� Ʈ������� �� ���ķ� �����ϴ��� Undo����
     * �ʴ´�. */
    switch ( sState )
    {
        case 2:
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdpstCache::completeExtendExtAndWakeUp(
                            aStatistics,
                            sSegCache ) == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC sdpstSegDDL::tryToFormatPageAfterHWM(
                                    idvSQL           * aStatistics,
                                    sdrMtxStartInfo  * aStartInfo,
                                    scSpaceID          aSpaceID,
                                    sdpSegHandle     * aSegHandle,
                                    sdRID            * aFstExtRID,
                                    sdrMtx           * aMtx,
                                    UChar           ** aFstDataPage,
                                    idBool           * aNeedToExtendExt )
{
    UChar           * sPagePtr;
    sdpstSegHdr     * sSegHdr;
    sdpstSegCache   * sSegCache;
    sdRID             sHWMExtRID;
    sdRID             sFmtExtRID;
    sdpstStack        sHWMStack;
    sdrMtx            sMtx;
    scPageID          sFstFmtPID;
    scPageID          sLstFmtPID;
    scPageID          sLstFmtExtDirPID;
    SShort            sLstFmtSlotNoInExtDir;
    UInt              sState = 0;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aFstExtRID != NULL );
    IDE_ASSERT( aNeedToExtendExt != NULL );

    *aNeedToExtendExt = ID_FALSE;

    sSegCache = (sdpstSegCache*)aSegHandle->mCache;

    IDE_ASSERT( sSegCache != NULL );

    /* HWM�� �����ϴ� Extent�� RID�� ���Ѵ�. */
    IDE_TEST( sdpstExtDir::makeExtRID( aStatistics,
                                       aSpaceID,
                                       sSegCache->mHWM.mExtDirPID,
                                       sSegCache->mHWM.mSlotNoInExtDir,
                                       &sHWMExtRID ) != IDE_SUCCESS );

    /* HWM�� �����ϴ� Extent�� ���� Extent�� �����´�. */
    IDE_TEST( sdpstExtDir::getNxtExtRID( aStatistics,
                                         aSpaceID,
                                         aSegHandle->mSegPID,
                                         sHWMExtRID,
                                         &sFmtExtRID ) != IDE_SUCCESS );

    /* ���� Extent�� ������ Ȯ���ؾ� �Ѵ�. */
    if ( sFmtExtRID == SD_NULL_RID )
    {
        *aNeedToExtendExt = ID_TRUE;
        IDE_CONT( skip_format_page );
    }

    *aFstExtRID = sFmtExtRID;

    /* ���� Extent���� �ش� Extent�� �����ϴ� LfBMP�� ���� ��� Extent��
     * format �Ѵ�. */
    IDE_TEST( sdpstExtDir::formatPageUntilNxtLfBMP( aStatistics, 
                                                    aStartInfo,
                                                    aSpaceID,
                                                    aSegHandle,
                                                    sFmtExtRID,
                                                    &sFstFmtPID,
                                                    &sLstFmtPID,
                                                    &sLstFmtExtDirPID,
                                                    &sLstFmtSlotNoInExtDir )
              != IDE_SUCCESS );

    IDE_ASSERT( sFstFmtPID != SD_NULL_PID );
    IDE_ASSERT( sLstFmtPID != SD_NULL_PID );
    IDE_ASSERT( sLstFmtExtDirPID      != SD_NULL_PID );
    IDE_ASSERT( sLstFmtSlotNoInExtDir != SDPST_INVALID_SLOTNO );

    /* ù��° �������� X-Latch ��Ƽ� �ٷ� �Ҵ����ش�. */
    if ( aFstDataPage != NULL )
    {
        IDE_ASSERT( aMtx != NULL );

        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sFstFmtPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              aFstDataPage,
                                              NULL, /* aTruSuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );
    }

    /* ������ �������� HWM ������ ���� ����Ѵ�. */
    sdpstStackMgr::initialize( &sHWMStack );
    IDE_TEST( sdpstAllocPage::makeOrderedStackFromDataPage(
                                                aStatistics,
                                                aSpaceID,
                                                aSegHandle->mSegPID,
                                                sLstFmtPID,
                                                &sHWMStack ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
    sState = 2;

    /* HWM�� �����Ѵ�. */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aSegHandle->mSegPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          NULL, /* aTrySuccess */
                                          NULL  /* aIsCorruptPage */ )
             != IDE_SUCCESS );

    sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

    IDE_TEST( sdpstSH::logAndUpdateWM( &sMtx,
                                       &sSegHdr->mHWM,
                                       sLstFmtPID,
                                       sLstFmtExtDirPID,
                                       sLstFmtSlotNoInExtDir,
                                       &sHWMStack ) != IDE_SUCCESS );

    sdpstCache::lockHWM4Write( aStatistics, sSegCache );
    sSegCache->mHWM = sSegHdr->mHWM;
    sdpstCache::unlockHWM( sSegCache );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_format_page );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                    == IDE_SUCCESS );
    }

    if ( sState == 2 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;


}

/***********************************************************************
 * Description : Segment�� ù���� Extent�� ������ ��� Extent�� TBS��
 *               ��ȯ�Ѵ�.
 *
 * Caution     : �� �Լ��� Return�ɶ� TBS Header�� X-Latch�� �ɷ��ִ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHdr      - [IN] Segment Header
 ***********************************************************************/
IDE_RC sdpstSegDDL::freeExtsExceptFst( idvSQL       * aStatistics,
                                       sdrMtx       * aMtx,
                                       scSpaceID      aSpaceID,
                                       sdpstSegHdr  * aSegHdr )
{
    sdrMtx               sFreeMtx;
    ULong                sExtDirCount;
    sdrMtxStartInfo      sStartInfo;
    UChar              * sPagePtr;
    sdpstExtDirHdr     * sExtDirHdr;
    scPageID             sCurExtDirPID;
    scPageID             sPrvExtDirPID;
    sdpPhyPageHdr      * sPhyHdrOfExtDirPage;
    SInt                 sState = 0;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );

    sStartInfo.mTrans   = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

    /* Segment Header�� ExtDirPage�� �ִ� Extent�� ù��°�� ������ ��� Extent
     * �� Free�Ѵ�. ù��°�� Segment Header�� ������ Extent�̹Ƿ� ���� ��������
     * Free�ϵ��� �Ѵ�. */
    sExtDirCount = sdpstSH::getTotExtDirCnt( aSegHdr );
    sCurExtDirPID = sdpDblPIDList::getListTailNode( &aSegHdr->mExtDirBase );

    while( ( sExtDirCount > 1 ) || ( aSegHdr->mSegHdrPID != sCurExtDirPID ) )
    {
        sExtDirCount--;

        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sFreeMtx,
                                       &sStartInfo,
                                       ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT ) != IDE_SUCCESS );
        sState = 1;

        /* Extent Dir Page�� First Extent�� ������ ��� Extent��
         * Free�Ѵ�. */
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              sCurExtDirPID,
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              &sFreeMtx,
                                              &sPagePtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );

        sExtDirHdr          = sdpstExtDir::getHdrPtr( sPagePtr );

        sPhyHdrOfExtDirPage = sdpPhyPage::getHdr( (UChar*)sExtDirHdr );
        sPrvExtDirPID       = sdpPhyPage::getPrvPIDOfDblList( sPhyHdrOfExtDirPage );

        IDE_TEST( sdpstExtDir::freeAllExtExceptFst( aStatistics,
                                                    &sStartInfo,
                                                    aSpaceID,
                                                    aSegHdr,
                                                    sExtDirHdr )
                  != IDE_SUCCESS );

        /* Extent Dir Page�� ������ ���� Fst Extent�� Free�ϰ� Fst Extent
         * �� ���� �ִ� Extent Dir Page�� ����Ʈ���� �����Ѵ�. �� �ο�����
         * �ϳ��� Mini Transaction���� ����� �Ѵ�. �ֳĸ� Extent�� Free��
         * ExtDirPage�� free�Ǳ� ������ �� �������� ExtDirPage List����
         * ���ŵǾ�� �Ѵ�. */
        IDE_TEST( sdpstExtDir::freeLstExt( aStatistics,
                                           &sFreeMtx,
                                           aSpaceID,
                                           aSegHdr,
                                           sExtDirHdr ) != IDE_SUCCESS );

        IDE_TEST( sdpDblPIDList::removeNode( aStatistics,
                                             &aSegHdr->mExtDirBase,
                                             sdpPhyPage::getDblPIDListNode(
                                                        sPhyHdrOfExtDirPage ),
                                             &sFreeMtx ) != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sFreeMtx ) != IDE_SUCCESS );

        sCurExtDirPID = sPrvExtDirPID;
    }

    /* SegHdr�� ExtDir�� ���´�. */
    IDE_ASSERT( sCurExtDirPID == aSegHdr->mSegHdrPID );

    IDE_TEST( sdpstExtDir::freeAllExtExceptFst( aStatistics,
                                                &sStartInfo,
                                                aSpaceID,
                                                aSegHdr,
                                                &aSegHdr->mExtDirHdr )
              != IDE_SUCCESS );


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sFreeMtx ) == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� ��� Extent�� Free��Ų��.
 *
 * Caution:
 *  1. �� �Լ��� Return�ɶ� TBS Header�� XLatch�� �ɷ��ִ�.
 *
 * aStatistics  - [IN] ��� ����
 * aMtx         - [IN] Mini Transaction Pointer
 * aSpaceID     - [IN] TableSpace ID
 * aSegHdr      - [IN] Segment Hdr
 ***********************************************************************/
IDE_RC sdpstSegDDL::freeAllExts( idvSQL         * aStatistics,
                                 sdrMtx         * aMtx,
                                 scSpaceID        aSpaceID,
                                 sdpstSegHdr    * aSegHdr )
{
    sdpDblPIDListBase * sExtDirPIDLst;
    sdrMtxStartInfo     sStartInfo;

    IDE_ASSERT( aMtx     != NULL );
    IDE_ASSERT( aSpaceID != 0 );
    IDE_ASSERT( aSegHdr  != NULL );

    sStartInfo.mTrans = sdrMiniTrans::getTrans( aMtx );

    if( sStartInfo.mTrans != NULL )
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }

    IDE_ASSERT( sdpstSH::getTotExtDirCnt( aSegHdr ) > 0 );
    
    IDE_TEST( freeExtsExceptFst( aStatistics,
                                 aMtx,
                                 aSpaceID,
                                 aSegHdr )
              != IDE_SUCCESS );


    sExtDirPIDLst = &aSegHdr->mExtDirBase;

    IDE_ASSERT( sdpstSH::getTotExtDirCnt( aSegHdr ) == 1 );

    IDE_TEST( sdpDblPIDList::initBaseNode( sExtDirPIDLst, aMtx )
              != IDE_SUCCESS );

    /* SegHdr�� ���Ե� ExtDir �� ���� �ȴ�. */
    IDE_TEST( sdpstExtDir::freeLstExt( aStatistics,
                                       aMtx,
                                       aSpaceID,
                                       aSegHdr,
                                       &aSegHdr->mExtDirHdr )
              != IDE_SUCCESS );


    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

