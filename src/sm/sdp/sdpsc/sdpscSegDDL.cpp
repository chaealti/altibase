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
 * $Id: sdpscSegDDL.cpp 85837 2019-07-14 23:44:48Z emlee $
 *
 * �� ������ Circular-List Managed Segment�� Create/Drop/Alter/Reset ������
 * STATIC �������̽����� �����Ѵ�.
 *
 ***********************************************************************/

# include <smErrorCode.h>
# include <sdr.h>
# include <sdpReq.h>

# include <sdpscDef.h>
# include <sdpscSegDDL.h>

# include <sdpscSH.h>
# include <sdpscED.h>
# include <sdpscCache.h>
# include <sdptbExtent.h>

/***********************************************************************
 *
 * Description : [ INTERFACE ] Segment �Ҵ� �� Segment Header �ʱ�ȭ
 *
 * aStatistics - [IN]  �������
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegType    - [IN]  Segment�� Type
 * aMtx        - [IN]  Mini Transaction�� Pointer
 * aSegHandle  - [IN]  Segment�� Handle
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::createSegment( idvSQL                * aStatistics,
                                   sdrMtx                * aMtx,
                                   scSpaceID               aSpaceID,
                                   sdpSegType              aSegType,
                                   sdpSegHandle          * aSegHandle )
{
    IDE_ASSERT( aMtx       != NULL );
    IDE_ASSERT( aSegHandle != NULL );

    IDE_TEST_RAISE( !((aSegType == SDP_SEG_TYPE_UNDO) ||
                      (aSegType == SDP_SEG_TYPE_TSS)),
                    cannot_create_segment );

    IDE_TEST( allocateSegment( aStatistics,
                               aMtx,
                               aSpaceID,
                               aSegHandle,
                               aSegType ) != IDE_SUCCESS );

    IDE_ASSERT( aSegHandle->mSegPID != SD_NULL_PID );

    return IDE_SUCCESS;

    IDE_EXCEPTION( cannot_create_segment );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_CannotCreateSegInUndoTBS) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� �Ҵ��Ѵ�.
 *
 * segment ������ �ʱ� extent ������ �Ҵ��Ѵ�. ����, �����ϴٰ� �����ϴٸ�
 * exception ó���Ѵ�.
 *
 * aStatistics - [IN]  �������
 * aSpaceID    - [IN]  Tablespace�� ID
 * aSegHandle  - [IN]  Segment�� Handle
 * aSegType    - [IN]  Segment�� Type
 * aMtx        - [IN]  Mini Transaction�� Pointer
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::allocateSegment( idvSQL       * aStatistics,
                                     sdrMtx       * aMtx,
                                     scSpaceID      aSpaceID,
                                     sdpSegHandle * aSegHandle,
                                     sdpSegType     aSegType )
{
    sdrMtx               sMtx;
    sdrMtxStartInfo      sStartInfo;
    sdpscExtDirInfo      sCurExtDirInfo;
    sdpscExtDesc         sExtDesc;
    sdRID                sFstExtDescRID = SD_NULL_RID;
    UInt                 sState         = 0;
    UInt                 sTotExtDescCnt = 0;

    IDE_ASSERT( aMtx                != NULL );
    IDE_ASSERT( aSegHandle          != NULL );
    IDE_ASSERT( aSegHandle->mSegPID == SD_NULL_PID );

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    /* Undo Tablespace�� �ý��ۿ� ���ؼ� �ڵ����� �����ǹǷ�
     * Segment Storage Parameter ���� ��� �����Ѵ� */

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    sdpscExtDir::initExtDesc( &sExtDesc );

    IDE_TEST( sdptbExtent::allocExts( aStatistics,
                                      &sStartInfo,
                                      aSpaceID,
                                      1,       // Extent ����
                                      (sdpExtDesc*)&sExtDesc )
              != IDE_SUCCESS );

    IDE_ASSERT( sExtDesc.mExtFstPID != SD_NULL_PID );

    aSegHandle->mSegPID       = sExtDesc.mExtFstPID;
    sCurExtDirInfo.mExtDirPID = sExtDesc.mExtFstPID;
    sCurExtDirInfo.mIsFull    = ID_FALSE;
    sCurExtDirInfo.mTotExtCnt = 0;
    sCurExtDirInfo.mMaxExtCnt = sdpscSegHdr::getMaxExtDescCnt( 
                       ((sdpscSegCache*)aSegHandle->mCache)->mCommon.mSegType );

    IDE_TEST( sdpscSegHdr::createAndInitPage(
                               aStatistics,
                               &sStartInfo,
                               aSpaceID,
                               &sExtDesc,
                               aSegType,
                               sCurExtDirInfo.mMaxExtCnt )
              != IDE_SUCCESS );

    // Extent ������ Segment Header �������� ExtDir ��������
    // ����ϰ�, Segment Header �������� �����Ѵ�.
    IDE_TEST( addAllocExtDesc( aStatistics,
                               &sMtx,
                               aSpaceID,
                               aSegHandle->mSegPID,
                               &sCurExtDirInfo,
                               &sFstExtDescRID,
                               &sExtDesc,
                               &sTotExtDescCnt )
              != IDE_SUCCESS );

    IDE_ASSERT( sFstExtDescRID != SD_NULL_RID );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Segment�� 1���̻��� Extent�� �Ҵ��Ѵ�.
 *
 * Segment�� ����� �Ҵ�� Extent Ȥ�� Extent Dir.�� Undo ���� �ʴ´�.
 *
 * aStatistics     - [IN]  �������
 * aSpaceID        - [IN]  Segment�� ���̺����̽�
 * aCurExtDir      - [IN]  ���� ExtDir�� PID
 * aStartInfo      - [IN]  Mtx ��������
 * aFreeListIdx    - [IN]  �Ҵ���� ���̺����̽��� ExtDir FreeList Ÿ��
 * aAllocExtRID    - [OUT] �Ҵ���� Extent RID
 * aFstPIDOfExt    - [OUT] �Ҵ���� Extent�� ù��° ������ ID
 * aFstDataPIDOfExt- [OUT] �Ҵ���� Extent�� ù��° ����Ÿ ������ ID
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::allocNewExts( idvSQL           * aStatistics,
                                  sdrMtxStartInfo  * aStartInfo,
                                  scSpaceID          aSpaceID,
                                  sdpSegHandle     * aSegHandle,
                                  scPageID           aCurExtDir,
                                  sdpFreeExtDirType  aFreeListIdx,
                                  sdRID            * aAllocExtRID,
                                  scPageID         * aFstPIDOfExt,
                                  scPageID         * aFstDataPIDOfExt )
{
    sdrMtx               sMtx;
    scPageID             sSegPID;
    smLSN                sNTA;
    UInt                 sState          = 0;
    UInt                 sTotExtCntOfSeg = 0;
    sdpscAllocExtDirInfo sAllocExtDirInfo;
    sdpscExtDirInfo      sCurExtDirInfo;
    sdpscExtDesc         sExtDesc;
    sdRID                sExtDescRID;
    idBool               sTryReusable = ID_FALSE;

    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aSegHandle          != NULL );
    IDE_ASSERT( aSegHandle->mSegPID != SD_NULL_PID );
    IDE_ASSERT( aCurExtDir          != SD_NULL_PID );
    IDE_ASSERT( aAllocExtRID        != NULL );
    IDE_ASSERT( aFstPIDOfExt        != NULL );
    IDE_ASSERT( aFstDataPIDOfExt    != NULL );

    /* BUG-46036 codesonar warning ���� */
    sAllocExtDirInfo.mIsAllocNewExtDir = ID_FALSE;

retry:
    
    sSegPID     = aSegHandle->mSegPID;
    sExtDescRID = SD_NULL_RID;
    sdpscExtDir::initExtDesc( &sExtDesc );

    sNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sdpscExtDir::getCurExtDirInfo( aStatistics,
                                             aSpaceID,
                                             aSegHandle,
                                             aCurExtDir,
                                             &sCurExtDirInfo )
              != IDE_SUCCESS );

    if ( sCurExtDirInfo.mIsFull == ID_TRUE )
    {
        // ExtDir �������� �����ؾ��Ѵٴ� ���� ���� LstExtDir ��������
        // ��� ExtDesc�� ä�����ٴ� ���̴�. �׷��Ƿ� ���� ���ο�
        // Extent Dir. �������� �Ҵ�Ǿ������Ѵٴ� �ǹ̷� SD_NULL_PID��
        // �����صд�.
        IDE_TEST( sdpscExtDir::tryAllocExtDir( aStatistics,
                                               aStartInfo,
                                               aSpaceID,
                                               (sdpscSegCache*)aSegHandle->mCache,
                                               sSegPID,
                                               aFreeListIdx,
                                               sCurExtDirInfo.mNxtExtDirPID,
                                               &sAllocExtDirInfo )
                  != IDE_SUCCESS );

        if ( sAllocExtDirInfo.mNewExtDirPID != SD_NULL_PID )
        {
            IDE_ASSERT( sAllocExtDirInfo.mFstExtDescRID != SD_NULL_RID );

            if ( (sAllocExtDirInfo.mIsAllocNewExtDir == ID_TRUE) ||
                 (sAllocExtDirInfo.mShrinkExtDirPID  != SD_NULL_PID) )
            {
                IDE_TEST( addOrShrinkAllocExtDir( aStatistics,
                                                  &sMtx,
                                                  aSpaceID,
                                                  sSegPID,
                                                  aFreeListIdx,
                                                  &sCurExtDirInfo,
                                                  &sAllocExtDirInfo ) 
                          != IDE_SUCCESS );
            }
            else
            {
                // Shrink ���� ����Ǵ� ��� Ȥ�� prepareNewPage4Append�� ���ؼ�
                // NxtExtDir�� ������ ���� ���׸�Ʈ����� �ƹ��� �۾��� ���� �ʴ´�.
            }
        }

        sExtDescRID     = sAllocExtDirInfo.mFstExtDescRID;
        sExtDesc        = sAllocExtDirInfo.mFstExtDesc;
        sTotExtCntOfSeg = sAllocExtDirInfo.mTotExtCntOfSeg;
    }

    if ( sExtDescRID == SD_NULL_RID )
    {
        // ���� �����̳� ExtDir�� �Ҵ���� ���Ͽ��ٸ�,
        // ���������� TBS�κ��� Extent�� �Ҵ��Ѵ�.
        if ( sdptbExtent::allocExts( aStatistics,
                                     aStartInfo,
                                     aSpaceID,
                                     1, /* aExtCnt */
                                     (sdpExtDesc*)&sExtDesc )
            != IDE_SUCCESS )
        {
            IDE_TEST( ideGetErrorCode() != smERR_ABORT_NOT_ENOUGH_SPACE );
            IDE_TEST( sTryReusable == ID_TRUE );
            IDE_CLEAR();

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

            /*
             * BUG-27288 [5.3.3] Undo Full �� �����ð��� ������ �ؼҵ��� ����.
             * : TBS �Ҵ��� ������ ���� ExtDir�� ���������� FULL�� �����
             *   �ٽ� �ѹ� ExtDir ���뿩�θ� Ȯ���Ѵ�.
             */
            IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                           &sMtx,
                                           aStartInfo,
                                           ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                           SM_DLOG_ATTR_DEFAULT )
                      != IDE_SUCCESS );
            sState = 1;
            
            IDE_TEST( sdpscExtDir::makeExtDirFull( aStatistics,
                                                   &sMtx,
                                                   aSpaceID,
                                                   aCurExtDir )
                      != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            
            sTotExtCntOfSeg = 0;
            sTryReusable = ID_TRUE;
            
            goto retry;
        }
        else
        {
            /* nothing  to do */
        }

        /* Extent ������ Segment Header �������� ���� ExtDir ��������
         * ����ϰų�, �ʿ��ϴٸ� ExtDir�� �����Ͽ� ����ϰ�
         * Segment Header �������� �����Ѵ�. */
        IDE_TEST( addAllocExtDesc( aStatistics,
                                   &sMtx,
                                   aSpaceID,
                                   sSegPID,
                                   &sCurExtDirInfo,
                                   &sExtDescRID,
                                   &sExtDesc,
                                   &sTotExtCntOfSeg )
                  != IDE_SUCCESS );
    }

    IDE_ASSERT( (sExtDesc.mExtFstPID != SD_NULL_PID) &&
                (sExtDesc.mLength > 0) );

    IDE_ASSERT( sExtDesc.mExtFstDataPID != SD_MAKE_PID( sExtDescRID ) );

    sdrMiniTrans::setNullNTA( &sMtx, aSpaceID, &sNTA );

    if ( sTotExtCntOfSeg > 0 )
    {
        sdpscCache::setSegSizeByBytes(
            aSegHandle,
            ((sdpscSegCache*)aSegHandle->mCache)->mCommon.mPageCntInExt *
            SD_PAGE_SIZE * sTotExtCntOfSeg );
    }
    else
    {
        // Shrink ���� ����Ǵ� ��� Ȥ�� prepareNewPage4Append�� ���ؼ�
        // NxtExtDir�� ������ ���� ���׸�Ʈ����� �ƹ��� �۾��� ���� �ʴ´�.
        IDE_ASSERT( (sAllocExtDirInfo.mIsAllocNewExtDir == ID_FALSE) ||
                    (sAllocExtDirInfo.mNewExtDirPID     == sCurExtDirInfo.mNxtExtDirPID) );
    }

    *aFstPIDOfExt     = sExtDesc.mExtFstPID;
    *aFstDataPIDOfExt = sExtDesc.mExtFstDataPID;

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    *aAllocExtRID     = sExtDescRID;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    *aAllocExtRID     = SD_NULL_RID;
    *aFstPIDOfExt     = SD_NULL_PID;
    *aFstDataPIDOfExt = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���ο� �ϳ��� Extent�� Segment�� �Ҵ��ϴ� ������ �Ϸ�
 *
 * Segment�� Ȯ��� Extent Desc.�� ���ؼ� �ʿ��ϴٸ� Extent Dir. ��������
 * �����Ͽ� Extent Desc�� ����ϰų� ���� Extent Dir. �������� ����Ѵ�.
 *
 * aStatistics     - [IN]  �������
 * aMtx            - [IN]  Mtx�� Pointer
 * aSpaceID        - [IN]  Tablespace�� ID
 * aSegPID         - [IN]  ���׸�Ʈ ��� PID
 * aCurExtDirInfo  - [IN]  ���� ExtDir ����
 * aAllocExtRID    - [OUT] �Ҵ���� ExtDesc �� RID
 * aExtDesc        - [OUT] ExtDesc ����
 * sTotExtDescCnt  - [OUT] Ȯ�����Ŀ� �� ExtDesc ����
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::addAllocExtDesc( idvSQL             * aStatistics,
                                     sdrMtx             * aMtx,
                                     scSpaceID            aSpaceID,
                                     scPageID             aSegPID,
                                     sdpscExtDirInfo    * aCurExtDirInfo,
                                     sdRID              * aAllocExtRID,
                                     sdpscExtDesc       * aExtDesc,
                                     UInt               * aTotExtDescCnt )
{
    sdpscSegMetaHdr    * sSegHdr;
    sdpscExtDirCntlHdr * sCurExtDirCntlHdr;
    sdpscExtDirCntlHdr * sNxtExtDirCntlHdr;
    UChar              * sNxtExtDirPagePtr;
    sdpscExtCntlHdr    * sExtCntlHdr;
    UInt                 sTotExtDescCnt;
    scPageID             sCurExtDirPID;
    sdRID                sAllocExtRID;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aSegPID        != SD_NULL_PID );
    IDE_ASSERT( aCurExtDirInfo != NULL );
    IDE_ASSERT( aAllocExtRID   != NULL );
    IDE_ASSERT( aExtDesc       != NULL );
    IDE_ASSERT( aTotExtDescCnt != NULL );

    IDE_TEST( sdpscSegHdr::fixAndGetHdr4Write( aStatistics,
                                               aMtx,
                                               aSpaceID,
                                               aSegPID,
                                               &sSegHdr ) 
              != IDE_SUCCESS );

    sExtCntlHdr = sdpscSegHdr::getExtCntlHdr( sSegHdr );

    if ( aCurExtDirInfo->mExtDirPID == aSegPID )
    {
        sCurExtDirCntlHdr = sdpscSegHdr::getExtDirCntlHdr( sSegHdr );
    }
    else
    {
        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write(
                                         aStatistics,
                                         aMtx,
                                         aSpaceID,
                                         aCurExtDirInfo->mExtDirPID,
                                         &sCurExtDirCntlHdr )
                  != IDE_SUCCESS );

    }

    IDE_ASSERT( aCurExtDirInfo->mTotExtCnt == sCurExtDirCntlHdr->mExtCnt );

    // ����, extent�� page�� �����Ǿ� ���� ���� �����ϱ�
    // ù��° data �������� ExtDir �������� �����ϰ�,
    // ù���� data �������� �����Ѵ�.
    if ( aCurExtDirInfo->mIsFull == ID_TRUE )
    {
        IDE_ASSERT( sdpscExtDir::getFreeDescCnt( sCurExtDirCntlHdr ) == 0 );

        /* makeExtDirFull() �� ȣ���ϰ� �Ǹ�, �ش� ExtDir�� mMaxExtCnt��
         * mExtCnt�� �����Ͽ� ������ Full ���·� �����.
         * �̷��� mMaxExtCnt�� ����� ExtDir�� CurExtDirInfo���� mMaxExtCnt��
         * �������� ����� Max���� �����ǰ�, ����� Max���� ����ؼ� ����
         * ExtDir�� �ݿ��ǰ� �ȴ�. ���� Max���� ������Ƽ���� ����������
         * �����Ѵ�. */
        IDE_TEST( sdpscExtDir::createAndInitPage(
                               aStatistics,
                               aMtx,
                               aSpaceID,
                               aExtDesc->mExtFstPID,
                               aCurExtDirInfo->mNxtExtDirPID,
                               sdpscSegHdr::getMaxExtDescCnt( sSegHdr->mSegCntlHdr.mSegType ),
                               &sNxtExtDirPagePtr ) 
                  != IDE_SUCCESS );

        sNxtExtDirCntlHdr = sdpscExtDir::getHdrPtr( sNxtExtDirPagePtr );

        IDE_TEST( sdpscSegHdr::addNewExtDir( aMtx,
                                             sSegHdr,
                                             sCurExtDirCntlHdr,
                                             sNxtExtDirCntlHdr )
                  != IDE_SUCCESS );

        sCurExtDirCntlHdr = sNxtExtDirCntlHdr;
        sCurExtDirPID     = aExtDesc->mExtFstPID;
    }
    else
    {
        sCurExtDirPID = aCurExtDirInfo->mExtDirPID;
    }

    /* To Fix BUG-23271 [SD] UDS/TSS Ȯ��� ù��° Extent Dir.��
     * ���Ե� Extent�� First Data PageID�� �߸�������
     *
     * mIsNeedExtDir�� True�� ���� (Extent Dir.�������� ���̻�
     * Extent Desc.�� ����� �� ���� Overflow��Ȳ) Extent Desc.��
     * FstPID�� SegPID�� ���� Extent�� FstPID�� FstDataPID�� �ٸ���.
     */
    if ( (aCurExtDirInfo->mIsFull == ID_TRUE) ||
         (aExtDesc->mExtFstPID    == aSegPID) )
    {
        aExtDesc->mExtFstDataPID = aExtDesc->mExtFstPID + 1;
    }
    else
    {
        aExtDesc->mExtFstDataPID = aExtDesc->mExtFstPID;
    }

    sAllocExtRID =
        SD_MAKE_RID( sCurExtDirPID,
                     sdpscExtDir::calcDescIdx2Offset(
                                      sCurExtDirCntlHdr,
                                      sCurExtDirCntlHdr->mExtCnt ) );

    IDE_ASSERT( sAllocExtRID != SD_NULL_RID );

    IDE_TEST( sdpscExtDir::logAndAddExtDesc( aMtx,
                                             sCurExtDirCntlHdr,
                                             aExtDesc )
              != IDE_SUCCESS );

    sTotExtDescCnt = sExtCntlHdr->mTotExtCnt + 1;
    IDE_TEST( sdpscSegHdr::setTotExtCnt( aMtx,
                                         sExtCntlHdr,
                                         sTotExtDescCnt )
              != IDE_SUCCESS );

    *aAllocExtRID   = sAllocExtRID;
    *aTotExtDescCnt = sTotExtDescCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aAllocExtRID   = SD_NULL_RID;
    *aTotExtDescCnt = 0;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���ο� �ϳ��� ExtDir�� Segment�� �����ϴ� ����
 *
 * Extent Dir. ������ ������ Extent�� �Ҵ��ؿԴٸ�, ���׸�Ʈ�� ���� Extent Dir.
 * ������ �տ��� �߰��Ѵ�.
 * ����, Nxt Extent Dir.�� Shrink �� �����ϴٰ� �ǴܵǾ��ٸ�,
 * Undo TBS�� ExtDir. FreeList�� �߰��Ѵ�.
 *
 * aStatistics      - [IN] �������
 * aMtx             - [IN] Mtx�� Pointer
 * aSpaceID         - [IN] Tablespace�� ID
 * aSegPID          - [IN] ���׸�Ʈ ��� PID
 * aFreeListIdx     - [IN] �Ҵ���� ���̺����̽��� ExtDir FreeList Ÿ��
 * aCurExtDirInfo   - [IN] ���� ExtDir ����
 * aAllocExtDirInfo - [IN] ���ο� ExtDir �Ҵ� ����
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::addOrShrinkAllocExtDir(
                            idvSQL                * aStatistics,
                            sdrMtx                * aMtx,
                            scSpaceID               aSpaceID,
                            scPageID                aSegPID,
                            sdpFreeExtDirType       aFreeListIdx,
                            sdpscExtDirInfo       * aCurExtDirInfo,
                            sdpscAllocExtDirInfo  * aAllocExtDirInfo )
{
    sdpscSegMetaHdr    * sSegHdr;
    sdpscExtDirCntlHdr * sCurExtDirCntlHdr = NULL;
    sdpscExtDirCntlHdr * sNewExtDirCntlHdr = NULL;
    sdpscExtCntlHdr    * sExtCntlHdr       = NULL;
    UInt                 sTotExtCntOfSeg   = 0;

    IDE_ASSERT( aMtx             != NULL );
    IDE_ASSERT( aCurExtDirInfo   != NULL );
    IDE_ASSERT( aAllocExtDirInfo != NULL );

    IDE_ASSERT( aAllocExtDirInfo->mNewExtDirPID != SD_NULL_PID );

    IDE_TEST( sdpscSegHdr::fixAndGetHdr4Write( aStatistics,
                                               aMtx,
                                               aSpaceID,
                                               aSegPID,
                                               &sSegHdr ) 
              != IDE_SUCCESS );

    sExtCntlHdr     = sdpscSegHdr::getExtCntlHdr( sSegHdr );
    sTotExtCntOfSeg = sExtCntlHdr->mTotExtCnt;
    IDE_ASSERT( sTotExtCntOfSeg > 0 );

    if ( aAllocExtDirInfo->mIsAllocNewExtDir == ID_TRUE )
    {
        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aCurExtDirInfo->mExtDirPID,
                                  &sCurExtDirCntlHdr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aAllocExtDirInfo->mNewExtDirPID,
                                  &sNewExtDirCntlHdr )
                  != IDE_SUCCESS );

        // �������� �ʴ� ��쿡�� Shrink ���� �ʴ´�.
        IDE_ASSERT( aAllocExtDirInfo->mShrinkExtDirPID == SD_NULL_PID );

        /* Extent Dir. ���� �Ҵ��� �߻��Ҷ����� �ٸ� ���׸�Ʈ���� ����ϴ�
         * Extent Dir. �������� �Ҵ��ؿ��� ���̴�. �׷��Ƿ� ���� UndoTBS��
         * �籸���� ������ ���� ���� ��쿡�� ���� �����ÿ� MaxExtInExtDir
         * ������Ƽ�� ���� ������ Extent Dir.�� ������ �� �ִ�.
         * �Ϲ������δ� �籸���� UndoTBS�� �����ϱ� ������ �ٸ��� �ʴ�. */
        IDE_TEST( sdpscSegHdr::addNewExtDir( aMtx,
                                             sSegHdr,
                                             sCurExtDirCntlHdr,
                                             sNewExtDirCntlHdr ) 
                  != IDE_SUCCESS );

        /* BUG-29709 undo segment�� total extent count�� �߸� �����ǰ� �ֽ��ϴ�.
         *
         * Segment�� �߰��� ExtDir�� ���(sNewExtDirCntlHdr)���� Extent ������
         * �����´�. */
        sTotExtCntOfSeg += sNewExtDirCntlHdr->mExtCnt;
    }
    else
    {
        // Nxt ExtDir�� �����ϴ� ��쿡�� ExtDir List�� ������
        // �̹� �Ǿ� �ִ�. �� ��쿡�� ShrinkExtDir�� ������ָ� �ȴ�.
    }

    if ( aAllocExtDirInfo->mShrinkExtDirPID != SD_NULL_PID )
    {
        IDE_ASSERT( sCurExtDirCntlHdr == NULL );

        // Shrink ������ �����ϴٴ� ���� Nxt Nxt��
        // �����ϱ� ������, ���ο� Extent Ȥ�� Ext Dir. �Ҵ��� ����.
        IDE_ASSERT( aAllocExtDirInfo->mIsAllocNewExtDir == ID_FALSE );
        IDE_ASSERT( aAllocExtDirInfo->mShrinkExtDirPID  != aSegPID );

        IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aCurExtDirInfo->mExtDirPID,
                                  &sCurExtDirCntlHdr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpscSegHdr::removeExtDir(
                                  aMtx,
                                  sSegHdr,
                                  sCurExtDirCntlHdr,
                                  aAllocExtDirInfo->mShrinkExtDirPID,
                                  aAllocExtDirInfo->mNewExtDirPID )
                  != IDE_SUCCESS );

        IDE_TEST( sdptbExtent::freeExtDir(
                                  aStatistics,
                                  aMtx,
                                  aSpaceID,
                                  aFreeListIdx,
                                  aAllocExtDirInfo->mShrinkExtDirPID )
                  != IDE_SUCCESS );

        /* BUG-29709 undo segment�� total extent count�� �߸� �����ǰ� �ֽ��ϴ�.
         *
         * Segment���� ������ Extent Dir�� ���Ե� Extent ������ ���־�� �Ѵ�.
         *
         * mExtCntInShrinkExtDir�� sdpscExtDir::tryAllocExtDir()����
         * Next ExtDir�� Next Next ExtDir �� ���� �����Ҷ�,
         * Next ExtDir�� shrink�ϰ�, Next Next ExtDir�� �����ϴµ�,
         * shrink�� Next ExtDir PID�� AllocExtDirInfo�� �����Ҷ� �Բ�
         * �����Ѵ�. */
        IDE_ASSERT( sTotExtCntOfSeg > aAllocExtDirInfo->mExtCntInShrinkExtDir );
        sTotExtCntOfSeg -= aAllocExtDirInfo->mExtCntInShrinkExtDir;
    }
    else
    {
        // ���� �߰��� Shrink �� ���� �����ϴ� ��쿡��
        // ExtDesc ������ ������ ����.
    }

    if ( sExtCntlHdr->mTotExtCnt != sTotExtCntOfSeg )
    {
        IDE_TEST( sdpscSegHdr::setTotExtCnt( aMtx, 
                                             sExtCntlHdr, 
                                             sTotExtCntOfSeg )
                  != IDE_SUCCESS );
    }

    aAllocExtDirInfo->mTotExtCntOfSeg = sTotExtCntOfSeg;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    aAllocExtDirInfo->mTotExtCntOfSeg = 0;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : [INTERFACE] From ���׸�Ʈ���� To ���׸�Ʈ��
 *               Extent Dir�� �ű��
 *
 * aStatistics  - [IN] �������
 * aStartInfo   - [IN] Mtx ��������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aFrSegHandle - [IN] From ���׸�Ʈ�� �ڵ�
 * aFrSegPID    - [IN] From ���׸�Ʈ�� PID
 * aFrCurExtDir - [IN] From ���׸�Ʈ�� ���� ExtDir �������� PID
 * aToSegHandle - [IN] To ���׸�Ʈ�� �ڵ�
 * aToSegPID    - [IN] To ���׸�Ʈ�� PID
 * aToCurExtDir - [IN] To ���׸�Ʈ�� ���� ExtDir �������� PID
 * aTrySuccess  - [OUT] Steal������ �������� ��ȯ
 *
 ***********************************************************************/
IDE_RC sdpscSegDDL::tryStealExts( idvSQL          * aStatistics,
                                  sdrMtxStartInfo * aStartInfo,
                                  scSpaceID         aSpaceID,
                                  sdpSegHandle    * aFrSegHandle,
                                  scPageID          aFrSegPID,
                                  scPageID          aFrCurExtDir,
                                  sdpSegHandle    * aToSegHandle,
                                  scPageID          aToSegPID,
                                  scPageID          aToCurExtDir,
                                  idBool          * aTrySuccess )
{
    sdrMtx                 sMtx;
    sdpscAllocExtDirInfo   sAllocExtDirInfo;
    sdpscExtDirInfo        sCurExtDirInfo;
    sdpscExtDirInfo        sNxtExtDirInfo;
    sdpscExtDirInfo        sOldExtDirInfo;
    UInt                   sTotExtCntOfFrSeg;
    UInt                   sTotExtCntOfToSeg;
    UInt                   sState = 0;

    IDE_ASSERT( aStartInfo   != NULL );
    IDE_ASSERT( aFrSegHandle != NULL );
    IDE_ASSERT( aFrSegPID    != SD_NULL_PID );
    IDE_ASSERT( aToSegHandle != NULL );
    IDE_ASSERT( aToSegPID    != SD_NULL_PID );
    IDE_ASSERT( aTrySuccess  != NULL );

    *aTrySuccess = ID_FALSE;
    /*
     * (1) From Segment�� Current Extent Dir��
     *     Next Extent Dir�� ���� �������� üũ�غ���.
     */
    IDE_TEST( sdpscExtDir::getCurExtDirInfo( aStatistics,
                                             aSpaceID,
                                             aFrSegHandle,
                                             aFrCurExtDir,
                                             &sCurExtDirInfo )
              != IDE_SUCCESS );

    /* BUG-30897 When the CurAllocExt locates in the Segment Header Page, 
     *           An UndoSegemnt can not steal an Extent from other Segment.
     * Next Extend Directory�� SegHdr�� ���, ����� SegmentHeader�� ��
     * ExtentDirectory�� ��Ȱ�� �� �� ���� ������ ���� Extent�� �����Ѵ�. */
    if( aFrSegPID == sCurExtDirInfo.mNxtExtDirPID )
    {
        /* �ڱ� ȥ�ڹۿ� �����ʾ����� �� �ʿ� ����.*/
        IDE_TEST_CONT( sCurExtDirInfo.mExtDirPID == 
                             sCurExtDirInfo.mNxtExtDirPID,
                        CONT_CANT_STEAL );

        idlOS::memcpy( (void*)&sOldExtDirInfo, 
                       (void*)&sCurExtDirInfo, 
                       ID_SIZEOF( sdpscExtDirInfo ) );

        /* ���� ExtentDirectory�� ��´�. */
        aFrCurExtDir = sCurExtDirInfo.mNxtExtDirPID;

        IDE_TEST( sdpscExtDir::getCurExtDirInfo( aStatistics,
                                                 aSpaceID,
                                                 aFrSegHandle,
                                                 aFrCurExtDir,
                                                 &sCurExtDirInfo )

                  != IDE_SUCCESS );       


        /* NextOfNext�� Old(������ ED)�� ���, �ѹ��� ���Ҵٴ� ������
         * ������� ED��. ����, �� �ʿ䰡 ����. */
        IDE_TEST_CONT( sCurExtDirInfo.mNxtExtDirPID == sOldExtDirInfo.mExtDirPID,
                       CONT_CANT_STEAL );
    }

    /* NxtExt �� steal �������� Ȯ���Ѵ� */
    IDE_TEST( sdpscExtDir::checkNxtExtDir4Steal( aStatistics,
                                                 aStartInfo,
                                                 aSpaceID,
                                                 aFrSegPID,
                                                 sCurExtDirInfo.mNxtExtDirPID,
                                                 &sAllocExtDirInfo )
                  != IDE_SUCCESS );

    IDE_TEST_CONT( sAllocExtDirInfo.mNewExtDirPID == SD_NULL_PID,
                    CONT_CANT_STEAL );

    /*
     * BUG-29709 undo segment�� total extent count�� �߸� �����ǰ� �ֽ��ϴ�.
     *
     * (2) From Segment���� steal ������(NxtExt)ExtDirInfo�� �����´�.
     */
    IDE_TEST( sdpscExtDir::getCurExtDirInfo( aStatistics,
                                             aSpaceID,
                                             aFrSegHandle,
                                             sAllocExtDirInfo.mNewExtDirPID,
                                             &sNxtExtDirInfo )
              != IDE_SUCCESS );

    /*
     * (3) From Segment�κ��� ã�� Extent Dir �������� �����,
     *     To Segment�� Current Extent Dir�� Next�� �߰��Ѵ�.
     *     ������ �����ص� �׸��̴�.
     */
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_ASSERT( sAllocExtDirInfo.mNewExtDirPID != aFrSegPID );
    
    IDE_ERROR( sNxtExtDirInfo.mTotExtCnt == (SShort)sAllocExtDirInfo.mExtCntInShrinkExtDir );
    IDE_ERROR( sNxtExtDirInfo.mExtDirPID == sAllocExtDirInfo.mNewExtDirPID );
    IDE_ERROR( sNxtExtDirInfo.mNxtExtDirPID == sAllocExtDirInfo.mNxtPIDOfNewExtDir );

    /* From Segment���� NxtExt�� (steal ������) ����  */
    IDE_TEST( sdpscSegHdr::removeExtDir( 
                         aStatistics,
                         &sMtx,
                         aSpaceID,
                         aFrSegPID,                 
                         sCurExtDirInfo.mExtDirPID,           /*aPrvPIDOfExtDir*/
                         sAllocExtDirInfo.mNewExtDirPID,      /*aRemExtDirPID */ 
                         sAllocExtDirInfo.mNxtPIDOfNewExtDir, /*aNxtPIDOfNewExtDir */
                         sAllocExtDirInfo.mExtCntInShrinkExtDir,
                         &sTotExtCntOfFrSeg )
              != IDE_SUCCESS );

    /* steal�� ExtCnt���� To Segment���� �����ϴ� MaxExtCnt�� ���ٸ�
       ������ �κ��� free */
    IDE_TEST( sdpscExtDir::shrinkExtDir( aStatistics,
                                         &sMtx,
                                         aSpaceID,
                                         aToSegHandle,
                                         &sAllocExtDirInfo )
              != IDE_SUCCESS ); 

    /* To Segment�� NxtExt�� (steal ������) add */
    IDE_TEST( sdpscSegHdr::addNewExtDir( 
                             aStatistics,
                             &sMtx,
                             aSpaceID,
                             aToSegPID,
                             aToCurExtDir,                    /* aCurExtDirPID */
                             sAllocExtDirInfo.mNewExtDirPID,  /* aNewExtDirPID*/
                             &sTotExtCntOfToSeg )             /* aTotExtCntOfToSeg */
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    sdpscCache::setSegSizeByBytes(
                aFrSegHandle,
                ((sdpscSegCache*)aFrSegHandle->mCache)->mCommon.mPageCntInExt *
                SD_PAGE_SIZE * sTotExtCntOfFrSeg );

    sdpscCache::setSegSizeByBytes(
                aToSegHandle,
                ((sdpscSegCache*)aToSegHandle->mCache)->mCommon.mPageCntInExt *
                SD_PAGE_SIZE * sTotExtCntOfToSeg );

    *aTrySuccess = ID_TRUE;

    IDE_EXCEPTION_CONT( CONT_CANT_STEAL );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}
