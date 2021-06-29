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
 * $Id: sdpscED.cpp 89495 2020-12-14 05:19:22Z emlee $
 *
 * �� ������ Circular-List Managed Segment�� ExtDir �������� ���� STATIC
 * �������̽��� �����Ѵ�.
 *
 * ������ Segment Ȯ�忡 ���� �����̴�.
 *
 * (1) Extent Dir.Page ���� �Ҵ� ����
 *     Undo Tablespace�� ID 0 File Header���� ���� ������ Free Extent Dir.
 *     ������ List�� �����Ѵ�.���� ������ Extent Dir. ���������� Extent��
 *     max ������ŭ ���� ���ԵǾ� �ִ�.
 *     Segment�� ������ ������ ���� Extent Dir. �������� Next Extent Dir.������
 *     ���̿� ��ġ��Ų��.
 *
 * (2) Extent ���� �Ҵ� ����
 *     Undo Tablespace�� ID 0 File Header�� ���� ������ Extent Dir. ��������
 *     ���ų� �ٸ� Ʈ������� �̹� Latch�� �ɰ� �ִٸ�  ������� �ʰ�, �ٷ� Undo
 *     Tablespace�κ��� Extent�� �Ҵ�޴´�.
 *     �Ҵ���� Extent�� ����� Extent Dir. �������� ���ٸ� �����ϰ� ���� Extent
 *     Dir. �������� Next Extent Dir.������ ���̿� ��ġ��Ų��.
 *
 ***********************************************************************/

# include <sdpDef.h>

# include <smxTrans.h>
# include <smxTransMgr.h>

# include <sdptbDef.h>
# include <sdptbExtent.h>

# include <sdpscSH.h>
# include <sdpscAllocPage.h>
# include <sdpscSegDDL.h>
# include <sdpscCache.h>
# include <sdpscED.h>


/***********************************************************************
 *
 * Description : ExtDir Control Header �ʱ�ȭ
 *
 * aCntlHdr   - [IN] Extent Dir. �������� Control ��� ������
 * aNxtExtDir - [IN] NxtExtDir �������� PID
 * aMapOffset - [IN] Extent Desc. ���� ������
 * aMaxExtCnt - [IN] Extent Dir. �������� ����� �� �ִ�
 *                   �ִ� Extent Desc. ����
 *
 ***********************************************************************/
void  sdpscExtDir::initCntlHdr( sdpscExtDirCntlHdr * aCntlHdr,
                                scPageID             aNxtExtDir,
                                scOffset             aMapOffset,
                                UShort               aMaxExtCnt )
{
    IDE_ASSERT( aCntlHdr   != NULL );
    IDE_ASSERT( aNxtExtDir != SD_NULL_PID );

    aCntlHdr->mExtCnt    = 0;           // ���������� Extent ����
    aCntlHdr->mNxtExtDir = aNxtExtDir;  // ���� ExtDir �������� PID
    aCntlHdr->mMapOffset = aMapOffset;  // Extent Map�� Offset
    aCntlHdr->mMaxExtCnt = aMaxExtCnt;  // ���������� Max Extent ����

    // ������ ����� Ʈ������� CommitSCN�� �����Ͽ� ���뿩�θ�
    // �Ǵ��ϴ� �������� ���
    SM_INIT_SCN( &(aCntlHdr->mLstCommitSCN) );

    // �ڽ��� ����� �������� Ȯ���� �� ���
    SM_INIT_SCN( &(aCntlHdr->mFstDskViewSCN) );

    return;
}

/***********************************************************************
 *
 * Description : Extent Dir. ������ ���� �� �ʱ�ȭ
 *
 * aStatistics        - [IN]  �������
 * aMtx               - [IN]  Mtx ������
 * aSpaceID           - [IN]  ���̺����̽� ID
 * aNewExtDirPID      - [IN]  �����ؾ��� ExtDir PID
 * aNxtExtDirPID      - [IN]  NxtExtDir �������� PID
 * aMaxExtCntInExtDir - [IN]  ExtDir ���� ExtDesc �ִ� ���� ����
 * aPagePtr           - [OUT] ������ Extent Dir. �������� ���� ������
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::createAndInitPage( idvSQL            * aStatistics,
                                       sdrMtx            * aMtx,
                                       scSpaceID           aSpaceID,
                                       scPageID            aNewExtDirPID,
                                       scPageID            aNxtExtDirPID,
                                       UShort              aMaxExtCntInExtDir,
                                       UChar            ** aPagePtr )
{
    UChar            * sPagePtr;
    sdpParentInfo      sParentInfo;

    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aNewExtDirPID  != SD_NULL_PID );

    sParentInfo.mParentPID   = aNewExtDirPID;
    sParentInfo.mIdxInParent = SDPSC_INVALID_IDX;

    IDE_TEST( sdpscAllocPage::createPage(
                                    aStatistics,
                                    aSpaceID,
                                    aNewExtDirPID,
                                    SDP_PAGE_CMS_EXTDIR,
                                    &sParentInfo,
                                    (sdpscPageBitSet)
                                    ( SDPSC_BITSET_PAGETP_META |
                                      SDPSC_BITSET_PAGEFN_FUL ),
                                    aMtx,
                                    aMtx,
                                    &sPagePtr ) != IDE_SUCCESS );

    IDE_TEST( logAndInitCntlHdr( aMtx,
                                 getHdrPtr( sPagePtr ),
                                 aNxtExtDirPID,
                                 aMaxExtCntInExtDir )
              != IDE_SUCCESS );

    *aPagePtr = sPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir Page Control Header �ʱ�ȭ �� write logging
 *
 * aMtx          - [IN] Mtx ������
 * aCntlHdr      - [IN] Extent Dir.�������� Control ��� ������
 * aNxtExtDirPID - [IN] NxtExtDir �������� PID
 * aMaxExtCnt    - [IN] Extent Dir. �������� ����� �� �ִ�
 *                      �ִ� Extent Desc. ����
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::logAndInitCntlHdr( sdrMtx               * aMtx,
                                       sdpscExtDirCntlHdr   * aCntlHdr,
                                       scPageID               aNxtExtDirPID,
                                       UShort                 aMaxExtCnt )
{
    scOffset sMapOffset;

    IDE_ASSERT( aCntlHdr != NULL );
    IDE_ASSERT( aMtx     != NULL );

    // Segment Header�� ExtDir Offset�� ExtDir ������������
    // Map Offset�� �ٸ���.
    sMapOffset =
        sdpPhyPage::getDataStartOffset( ID_SIZEOF( sdpscExtDirCntlHdr ) );

    // page range slot�ʱ�ȭ�� ���ش�.
    initCntlHdr( aCntlHdr, aNxtExtDirPID, sMapOffset, aMaxExtCnt );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aCntlHdr,
                                         NULL,
                                         (ID_SIZEOF( aNxtExtDirPID ) +
                                          ID_SIZEOF( scOffset )      +
                                          ID_SIZEOF( aMaxExtCnt )),
                                         SDR_SDPSC_INIT_EXTDIR )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&aNxtExtDirPID,
                                   ID_SIZEOF( aNxtExtDirPID ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&sMapOffset,
                                   ID_SIZEOF( scOffset ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::write( aMtx,
                                   (UChar*)&aMaxExtCnt,
                                   ID_SIZEOF( aMaxExtCnt ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir�� Extent Desc. ���
 *
 * aMapPtr  - [IN] Extent Dir. �������� Extent Desc. �� ������
 * aLstIdx  - [IN] Extent Desc.�� ��ϵ� ������ �ʿ����� ����
 * aExtDesc - [IN] ����� Extent Desc. ������
 *
 ***********************************************************************/
void sdpscExtDir::addExtDescToMap( sdpscExtDirMap  * aMapPtr,
                                   SShort            aLstIdx,
                                   sdpscExtDesc    * aExtDesc )
{
    IDE_ASSERT( aMapPtr                  != NULL );
    IDE_ASSERT( aExtDesc                 != NULL );
    IDE_ASSERT( aLstIdx                  != SDPSC_INVALID_IDX );
    IDE_ASSERT( aExtDesc->mExtFstPID     != SD_NULL_PID );
    IDE_ASSERT( aExtDesc->mLength        >= SDP_MIN_EXTENT_PAGE_CNT );
    IDE_ASSERT( aExtDesc->mExtFstDataPID != SD_NULL_PID );

    // ���� Emtpy Extent Slot �� ���Ѵ�.
    aMapPtr->mExtDesc[ aLstIdx ] = *aExtDesc;

    return;
}

/***********************************************************************
 *
 * Description : ExtDir�� extslot�� ����Ѵ�.
 *
 * aCntlHdr - [IN] Extent Dir.�������� Control ��� ������
 * aLstIdx  - [IN] Extent Desc.�� ��ϵ� ������ �ʿ����� ����
 * aExtDesc - [IN] ����� Extent Desc. ������
 *
 ***********************************************************************/
void sdpscExtDir::addExtDesc( sdpscExtDirCntlHdr * aCntlHdr,
                              SShort               aLstIdx,
                              sdpscExtDesc       * aExtDesc )
{
    IDE_ASSERT( aCntlHdr != NULL );
    IDE_ASSERT( aExtDesc != NULL );
    IDE_ASSERT( aLstIdx  != SDPSC_INVALID_IDX );

    addExtDescToMap( getMapPtr(aCntlHdr), aLstIdx, aExtDesc );
    aCntlHdr->mExtCnt++;

    return;
}

/***********************************************************************
 *
 * Descripton : Segment Header�κ��� ������ ExtDir�� ���
 *
 * aMtx         - [IN] Mtx ������
 * aCntlHdr     - [IN] Extent Dir.�������� Control ��� ������
 * aExtDesc     - [IN] ����� Extent Desc. ������
 * aAllocExtRID - [IN] ����� Extent Desc.�� RID
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::logAndAddExtDesc( sdrMtx             * aMtx,
                                      sdpscExtDirCntlHdr * aCntlHdr,
                                      sdpscExtDesc       * aExtDesc )
{
    IDE_ASSERT( aCntlHdr                 != NULL );
    IDE_ASSERT( aMtx                     != NULL );
    IDE_ASSERT( aExtDesc                 != NULL );
    IDE_ASSERT( aExtDesc->mLength        > 0 );
    IDE_ASSERT( aExtDesc->mExtFstPID     != SD_NULL_PID );
    IDE_ASSERT( aExtDesc->mExtFstDataPID != SD_NULL_PID );

    addExtDesc( aCntlHdr, aCntlHdr->mExtCnt, aExtDesc );

    IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                         (UChar*)aCntlHdr,
                                         aExtDesc,
                                         ID_SIZEOF( sdpscExtDesc ),
                                         SDR_SDPSC_ADD_EXTDESC_TO_EXTDIR )
              != IDE_SUCCESS );

    IDE_ASSERT( aCntlHdr->mExtCnt > 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir Control Header�� �����ִ� �������� release�Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aCntlHdr       - [IN] ExtDir Cntl Header
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::releaseCntlHdr( idvSQL              * aStatistics,
                                    sdpscExtDirCntlHdr  * aCntlHdr )
{
    UChar *sPagePtr = sdpPhyPage::getPageStartPtr( (UChar*)aCntlHdr );

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : [ INTERFACE ] Squential Scan�� Extent �� ù���� Data
 *               �������� ��ȯ�Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceID    - [IN] ���̺����̽� ID
 * aExtRID     - [IN] ������ ���� ��� Extent Desc.�� RID
 * aExtInfo    - [OUT] Extent Desc.�� ����
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::getExtInfo( idvSQL       *aStatistics,
                                scSpaceID     aSpaceID,
                                sdRID         aExtRID,
                                sdpExtInfo   *aExtInfo )
{
    UInt             sState = 0;
    sdpscExtDesc   * sExtDesc;

    IDE_ASSERT( aExtInfo    != NULL );

    IDE_TEST( sdbBufferMgr::fixPageByRID( aStatistics, /* idvSQL */
                                          aSpaceID,
                                          aExtRID,
                                          (UChar**)&sExtDesc  )
              != IDE_SUCCESS );
    sState = 1;

    aExtInfo->mFstPID     = sExtDesc->mExtFstPID;
    aExtInfo->mFstDataPID = sExtDesc->mExtFstDataPID;
    aExtInfo->mLstPID     = sExtDesc->mExtFstPID + sExtDesc->mLength - 1;

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       (UChar*)sExtDesc )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    if (sState != 0)
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, (UChar*)sExtDesc )
                    == IDE_SUCCESS );
    }
    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���� Extent Desc.�� RID ��ȯ
 *
 * aStatistics - [IN]  �������
 * aSpaceID    - [IN]  ���̺����̽� ID
 * aSegHdrPID  - [IN]  ���׸�Ʈ ��� �������� PID
 * aCurrExtRID - [IN]  ���� Extent Desc.�� RID
 * aNxtExtRID  - [OUT] ���� Extent Desc.�� RID
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::getNxtExtRID( idvSQL       *aStatistics,
                                  scSpaceID     aSpaceID,
                                  scPageID    /*aSegHdrPID*/,
                                  sdRID         aCurrExtRID,
                                  sdRID        *aNxtExtRID )
{
    UInt                  sState = 0;
    scPageID              sNxtExtDir;
    sdpscExtDirCntlHdr  * sCntlHdr;
    SShort                sExtDescIdx;

    IDE_ASSERT( aNxtExtRID != NULL );

    IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                     NULL,  /* aMtx */
                                     aSpaceID,
                                     SD_MAKE_PID( aCurrExtRID ),
                                     &sCntlHdr  )
              != IDE_SUCCESS );
    sState = 1;

    sExtDescIdx = calcOffset2DescIdx( sCntlHdr,
                                      SD_MAKE_OFFSET( aCurrExtRID ) );

    IDE_ASSERT( sCntlHdr->mExtCnt > sExtDescIdx );

    if ( sCntlHdr->mExtCnt > ( sExtDescIdx + 1 ) )
    {
        // ExtDir ���������� ������ Extent Slot�� �ƴѰ��
        *aNxtExtRID = SD_MAKE_RID( SD_MAKE_PID( aCurrExtRID ),
                                   calcDescIdx2Offset( sCntlHdr, (sExtDescIdx+1) ) );
    }
    else
    {
        // ExtDir ���������� ������ Extent Slot�� ��� NULL_RID ��ȯ
        if ( sCntlHdr->mNxtExtDir == SD_NULL_PID )
        {
            *aNxtExtRID = SD_NULL_RID;
        }
        else
        {
            // Next Extent Dir. �������� �����ϴ� ���
            sNxtExtDir = sCntlHdr->mNxtExtDir;

            sState = 0;
            IDE_TEST( releaseCntlHdr( aStatistics, sCntlHdr) != IDE_SUCCESS );

            IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                             NULL, /* aMtx */
                                             aSpaceID,
                                             sNxtExtDir,
                                             &sCntlHdr  ) != IDE_SUCCESS );
            sState = 1;

            *aNxtExtRID = SD_MAKE_RID( sNxtExtDir,
                                       calcDescIdx2Offset( sCntlHdr, 0 ));
        }
    }

    sState = 0;
    IDE_TEST( releaseCntlHdr( aStatistics, sCntlHdr) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( releaseCntlHdr( aStatistics, sCntlHdr ) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : ���� Extent�κ��� ���ο� �������� �Ҵ��Ѵ�.
 *
 * aPrvAllocExtRID�� ����Ű�� Extent�� aPrvAllocPageID����
 * Page�� �����ϴ� �ϴ��� üũ�ؼ� ������ ���ο� ����
 * Extent�� �̵��ϰ� ���� Extent�� ������ TBS�� ���� ���ο�
 * Extent�� �Ҵ�޴´�. ���� Extent���� Free Page�� ã�Ƽ�
 * Page�� �Ҵ�� ExtRID�� PageID�� �Ѱ��ش�.
 *
 * aStatistics          - [IN] ��� ����
 * aMtx                 - [IN] Mtx ������
 * aSpaceID             - [IN] ���̺����̽� ID
 * aSegHandle           - [IN] ���׸�Ʈ �ڵ� ������
 * aPrvAllocExtRID      - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aFstPIDOfPrvAllocExt - [IN] ���� Extent�� ù��° ������ ID
 * aPrvAllocPageID      - [IN] ������ �Ҵ���� PageID
 * aAllocExtRID         - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aFstPIDOfAllocExt    - [OUT] �Ҵ����� Extent Desc.�� ù��° PageID
 * aAllocPID            - [OUT] ���Ӱ� �Ҵ���� PageID
 * aParentInfo          - [OUT] �Ҵ�� Page's Parent Info
 *
 **********************************************************************/
IDE_RC sdpscExtDir::allocNewPageInExt(
                                idvSQL         * aStatistics,
                                sdrMtx         * aMtx,
                                scSpaceID        aSpaceID,
                                sdpSegHandle   * aSegHandle,
                                sdRID            aPrvAllocExtRID,
                                scPageID         aFstPIDOfPrvAllocExt,
                                scPageID         aPrvAllocPageID,
                                sdRID          * aAllocExtRID,
                                scPageID       * aFstPIDOfAllocExt,
                                scPageID       * aAllocPID,
                                sdpParentInfo  * aParentInfo )
{
    sdRID                 sAllocExtRID;
    sdrMtxStartInfo       sStartInfo;
    sdpFreeExtDirType     sFreeListIdx;
    sdRID                 sPrvAllocExtRID;
    scPageID              sPrvAllocPageID;
    scPageID              sFstPIDOfPrvAllocExt;
    idBool                sIsNeedNewExt;
    scPageID              sFstPIDOfExt;
    scPageID              sFstDataPIDOfExt;

    IDE_ASSERT( aSegHandle        != NULL );
    IDE_ASSERT( aMtx              != NULL );
    IDE_ASSERT( aAllocExtRID      != NULL );
    IDE_ASSERT( aFstPIDOfAllocExt != NULL );
    IDE_ASSERT( aAllocPID         != NULL );

    sAllocExtRID  = SD_NULL_RID;
    sIsNeedNewExt = ID_TRUE;

    if ( sdpscCache::getSegType( aSegHandle ) == SDP_SEG_TYPE_TSS )
    {
        sFreeListIdx = SDP_TSS_FREE_EXTDIR_LIST;
    }
    else
    {
        sFreeListIdx = SDP_UDS_FREE_EXTDIR_LIST;
    }

    sdrMiniTrans::makeStartInfo( aMtx, &sStartInfo );

    sPrvAllocExtRID      = aPrvAllocExtRID;
    sFstPIDOfPrvAllocExt = aFstPIDOfPrvAllocExt;

    IDE_ASSERT( aPrvAllocExtRID != SD_NULL_RID );

    if ( aPrvAllocPageID == SD_NULL_PID )
    {
        IDE_ASSERT( sFstPIDOfPrvAllocExt == aSegHandle->mSegPID );

        sPrvAllocPageID = aSegHandle->mSegPID;
    }
    else
    {
        sPrvAllocPageID = aPrvAllocPageID;
    }

    if ( isFreePIDInExt( ((sdpscSegCache*)aSegHandle->mCache)->mCommon.mPageCntInExt,
                        sFstPIDOfPrvAllocExt,
                        sPrvAllocPageID ) == ID_TRUE )
    {
        sIsNeedNewExt = ID_FALSE;
    }
    else
    {
        /* nothing to do */
    }

    if ( sIsNeedNewExt == ID_TRUE )
    {
        /* aPrvAllocExtRID ���� Extent�� �����ϴ��� Check�Ѵ�. */
        IDE_TEST( getNxtExt4Alloc( aStatistics,
                                   aSpaceID,
                                   aPrvAllocExtRID,
                                   &sAllocExtRID,
                                   &sFstPIDOfExt,
                                   &sFstDataPIDOfExt ) 
                  != IDE_SUCCESS );

        if ( sFstDataPIDOfExt != SD_NULL_PID )
        {
            IDE_ASSERT( sFstDataPIDOfExt != SD_MAKE_PID( sAllocExtRID ) );
        }
        else
        {
            IDE_ASSERT( sAllocExtRID == SD_NULL_RID );
        }

        if ( sAllocExtRID == SD_NULL_RID )
        {
            /* ���ο� Extent�� TBS�κ��� �Ҵ�޴´�. */
            IDE_TEST( sdpscSegDDL::allocNewExts( aStatistics,
                                                 &sStartInfo,
                                                 aSpaceID,
                                                 aSegHandle,
                                                 SD_MAKE_PID( sPrvAllocExtRID ),
                                                 sFreeListIdx,
                                                 &sAllocExtRID,
                                                 &sFstPIDOfExt,
                                                 &sFstDataPIDOfExt ) 
                      != IDE_SUCCESS );

            IDE_ASSERT( sFstDataPIDOfExt != SD_MAKE_PID( sAllocExtRID ) );
        }
        else
        {
                /* nothing to do */
        }

        IDE_ASSERT( sFstPIDOfExt != SD_NULL_PID );

        *aAllocPID         = sFstDataPIDOfExt;
        *aFstPIDOfAllocExt = sFstPIDOfExt;
        *aAllocExtRID      = sAllocExtRID;
    }
    else
    {
        /* �� Extent���� �������� ���ӵǾ� �����Ƿ� ���ο�
         * PageID�� ���� Alloc�ߴ� �������� 1���� ���� �ȴ�. */
        *aAllocPID         = sPrvAllocPageID + 1;
        *aFstPIDOfAllocExt = aFstPIDOfPrvAllocExt;
        *aAllocExtRID      = aPrvAllocExtRID;

        IDE_ASSERT( *aAllocPID != SD_MAKE_PID( aPrvAllocExtRID ) );
    }

    aParentInfo->mParentPID   = *aAllocExtRID;
    aParentInfo->mIdxInParent = SDPSC_INVALID_IDX;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment�� ExtDir �������� PID�� �����Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aToLstExtDir - [IN] Next Extent Dir. ������ ID�� ������ ������ Extent Dir.
 *                     �������� PageID
 * aPageID      - [IN] ������ ID
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::setNxtExtDir( idvSQL          * aStatistics,
                                  sdrMtx          * aMtx,
                                  scSpaceID         aSpaceID,
                                  scPageID          aToLstExtDir,
                                  scPageID          aPageID )
{
    sdpscExtDirCntlHdr  * sCntlHdr;

    IDE_ASSERT( aToLstExtDir != SD_NULL_PID );
    IDE_ASSERT( aPageID      != SD_NULL_PID );
    IDE_ASSERT( aMtx         != NULL );

    IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                      aMtx,
                                      aSpaceID,
                                      aToLstExtDir,
                                      &sCntlHdr ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(sCntlHdr->mNxtExtDir),
                                         &aPageID,
                                         ID_SIZEOF( scPageID ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Segment�� ExtDir�� ���������� FULL ���¸� �����.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aExtDir      - [IN] FULL ���¸� ���� Extent Dir. Page ID
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::makeExtDirFull( idvSQL     * aStatistics,
                                    sdrMtx     * aMtx,
                                    scSpaceID    aSpaceID,
                                    scPageID     aExtDir )
{
    sdpscExtDirCntlHdr  * sCntlHdr;

    IDE_ASSERT( aExtDir != SD_NULL_PID );
    IDE_ASSERT( aMtx    != NULL );

    IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                      aMtx,
                                      aSpaceID,
                                      aExtDir,
                                      &sCntlHdr ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(sCntlHdr->mMaxExtCnt),
                                         &sCntlHdr->mExtCnt,
                                         ID_SIZEOF( sCntlHdr->mExtCnt ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ���Ÿ������� ExtDir �������� Control Header�� fix�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aExtDirPID   - [IN] X-Latch�� ȹ���� Extent Dir. �������� PID
 * aCntlHdr     - [OUT] Extnet Dir.�������� Control ��� ������
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::fixAndGetCntlHdr4Write( idvSQL              * aStatistics,
                                            sdrMtx              * aMtx,
                                            scSpaceID             aSpaceID,
                                            scPageID              aExtDirPID,
                                            sdpscExtDirCntlHdr ** aCntlHdr )
{
    idBool               sDummy;
    UChar              * sPagePtr;
    sdpPageType          sPageType;
    sdpscSegMetaHdr    * sSegHdr;

    IDE_ASSERT( aExtDirPID != SD_NULL_PID );
    IDE_ASSERT( aCntlHdr   != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aExtDirPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL )
              != IDE_SUCCESS );

    sPageType = sdpPhyPage::getPageType( (sdpPhyPageHdr*)sPagePtr );

    if ( sPageType == SDP_PAGE_CMS_SEGHDR )
    {
        sSegHdr   = sdpscSegHdr::getHdrPtr( sPagePtr );
        *aCntlHdr = sdpscSegHdr::getExtDirCntlHdr( sSegHdr );
    }
    else
    {
        IDE_ASSERT( sPageType == SDP_PAGE_CMS_EXTDIR );
        *aCntlHdr = sdpscExtDir::getHdrPtr( sPagePtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Read �������� ExtDir �������� Control Header�� fix�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aMtx         - [IN] Mtx ������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aExtDirPID   - [IN] S-Latch�� ȹ���� Extent Dir. �������� PID
 * aCntlHdr     - [OUT] Extnet Dir.�������� Control ��� ������
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::fixAndGetCntlHdr4Read( idvSQL              * aStatistics,
                                           sdrMtx              * aMtx,
                                           scSpaceID             aSpaceID,
                                           scPageID              aExtDirPID,
                                           sdpscExtDirCntlHdr ** aCntlHdr )
{
    idBool            sDummy;
    UChar           * sPagePtr;
    sdpPageType       sPageType;
    sdpscSegMetaHdr * sSegHdr;

    IDE_ASSERT( aExtDirPID != SD_NULL_PID );
    IDE_ASSERT( aCntlHdr   != NULL );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aExtDirPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL )
              != IDE_SUCCESS );

    sPageType = (sdpPageType)sdpPhyPage::getPhyPageType( sPagePtr );

    if ( sPageType == SDP_PAGE_CMS_SEGHDR )
    {
        sSegHdr   = sdpscSegHdr::getHdrPtr( sPagePtr );
        *aCntlHdr = sdpscSegHdr::getExtDirCntlHdr( sSegHdr );
    }
    else
    {
        IDE_ASSERT( sPageType == SDP_PAGE_CMS_EXTDIR );

        *aCntlHdr = sdpscExtDir::getHdrPtr( sPagePtr );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : [ INTERFACE ] Extent���� ���� Data �������� ��ȯ�Ѵ�.
 *
 * aStatistics  - [IN] �������
 * aSpaceID     - [IN] ���̺����̽� ID
 * aSegInfo     - [IN] �ش� ���׸�Ʈ ���� ������
 * aSegCacheInfo- [IN] �ش� ���׸�Ʈ ĳ�� ���� ������
 * aExtRID      - [IN/OUT] ���� Extent Desc.�� RID
 * aExtInfo     - [IN/OUT] ���� Extent Desc.�� ����
 * aPageID      - [OUT] ���� �Ҵ�� �������� IDx
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::getNxtAllocPage( idvSQL             * aStatistics,
                                     scSpaceID            aSpaceID,
                                     sdpSegInfo         * aSegInfo,
                                     sdpSegCacheInfo    * /*aSegCacheInfo*/,
                                     sdRID              * aExtRID,
                                     sdpExtInfo         * aExtInfo,
                                     scPageID           * aPageID )
{
    sdRID               sNxtExtRID;

    IDE_ASSERT( aExtInfo    != NULL );

    /*
     * BUG-24566 UDS/TSS ���׸�Ʈ�� GetNxtAllocPage����
     * Undo/TSS ���������� ��ȯ�ϵ��� �����ؾ���.
     *
     * aPageID ��ȯ�� Data ���������� ��ȯ�Ѵ�. �ֳ��ϸ�,
     * ���� ��⿡�� ���׸�Ʈ�� ��Ÿ�������� ������ �� ������, �Ϲ�������
     * �� �ʿ䵵 ����. �ε����ϰ� ��Ÿ�� ���� Ư�������� ������� ���̶��,
     * ������ �������̽��� �����Ѵ�.
     */

    if ( *aPageID == SD_NULL_PID )
    {
        *aPageID = aExtInfo->mFstDataPID;
    }
    else
    {
        while ( 1 )
        {
            // ���� Extent ������ �Ѿ�� �������̸�,
            // ���� extent ������ ���� �Ѵ�.
            if( *aPageID == aExtInfo->mLstPID )
            {
                IDE_TEST( getNxtExtRID( aStatistics,
                                        aSpaceID,
                                        aSegInfo->mSegHdrPID,
                                        *aExtRID,
                                        &sNxtExtRID )
                          != IDE_SUCCESS );

                if( sNxtExtRID == SD_NULL_RID )
                {
                    // ������ Extent Descriptor�� ��쿡��
                    // ù��° Extent Descriptor�� ��ȯ�Ѵ�.
                    sNxtExtRID = aSegInfo->mFstExtRID;
                }

                IDE_TEST( getExtInfo( aStatistics,
                                      aSpaceID,
                                      sNxtExtRID,
                                      aExtInfo )
                          != IDE_SUCCESS );

                *aExtRID = sNxtExtRID;
                *aPageID = aExtInfo->mFstDataPID;
                break;
            }
            else
            {
                *aPageID += 1;
                break;
            }
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir ������ �Ҵ�õ�
 *
 * ù��°, ���׸�Ʈ �������� ���� ExtDir�� �Ҵ� �Ҽ� �ִ��� Ȯ���ϰ� �Ҵ��Ѵ�.
 * �ι�°, ���̺����̽��κ��� Free�� ExtDir �Ҵ��Ѵ�.
 *
 * aStatistics      - [IN]  �������
 * aStartInfo       - [IN]  Mtx ���� ����
 * aSpaceID         - [IN]  ���̺����̽� ID
 * aSegCache        - [IN]  ���׸�Ʈ Cache ������
 * aSegPID          - [IN]  ���׸�Ʈ ��� ������ ID
 * aFreeListIdx     - [IN]  Free Extent Dir. List Ÿ�Թ�ȣ
 * aNxtExtDirPID    - [IN]  ���� ExtDir �������� PID
 * aAllocExtDirInfo - [OUT] �Ҵ� ������ ExtDir �������� �Ҵ翡 ���� ����
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::tryAllocExtDir( idvSQL               * aStatistics,
                                    sdrMtxStartInfo      * aStartInfo,
                                    scSpaceID              aSpaceID,
                                    sdpscSegCache        * aSegCache,
                                    scPageID               aSegPID,
                                    sdpFreeExtDirType      aFreeListIdx,
                                    scPageID               aNxtExtDirPID,
                                    sdpscAllocExtDirInfo * aAllocExtDirInfo )
{
    smSCN            sMyFstDskViewSCN;
    smSCN            sSysMinDskViewSCN;
    SInt             sLoop;
    scPageID         sExtDirPID;
    scPageID         sNxtExtDirPID;
    sdpscExtDesc     sFstExtDesc;
    sdRID            sFstExtDescRID;
    sdpscExtDirState sExtDirState[2];
    UInt             sExtCntInExtDir[2];

    IDE_ASSERT( aSegCache        != NULL );
    IDE_ASSERT( aAllocExtDirInfo != NULL );
    IDE_ASSERT( aNxtExtDirPID    != SD_NULL_PID );

    sMyFstDskViewSCN = smxTrans::getFstDskViewSCN( aStartInfo->mTrans );
    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

    initAllocExtDirInfo( aAllocExtDirInfo );

    /* A. Next ExtDir �������� ������ �� �ִ��� Ȯ���Ѵ�.
     *    ���� NextNext ExtDir�������� ���밡������ Ȯ���Ͽ�
     *    Shrink �� �� �ִٸ� Shrink �Ѵ�. */
    for ( sLoop = 0, sExtDirPID = aNxtExtDirPID; sLoop < 2; sLoop++ )
    {
        IDE_TEST( checkExtDirState4Reuse( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          sExtDirPID,
                                          &sMyFstDskViewSCN,
                                          &sSysMinDskViewSCN,
                                          &sExtDirState[sLoop],
                                          &sFstExtDescRID,
                                          &sFstExtDesc,
                                          &sNxtExtDirPID,
                                          &sExtCntInExtDir[sLoop] )
                  != IDE_SUCCESS );

        if ( sExtDirState[sLoop] != SDPSC_EXTDIR_EXPIRED )
        {
            break;
        }

        IDE_ASSERT( sFstExtDescRID != SD_NULL_RID );
        IDE_ASSERT( sNxtExtDirPID  != SD_NULL_PID );

        if ( sLoop > 0 )
        {
            // Nxt ExtDir�� Shrink �����ϰ�, NxtNxt ExtDir �������� �����Ѵ�.
            aAllocExtDirInfo->mShrinkExtDirPID      =
                                           aAllocExtDirInfo->mNewExtDirPID;
            aAllocExtDirInfo->mExtCntInShrinkExtDir = 
                                           sExtCntInExtDir[0];
        }

        /* ���� NxtNxt ExtDir �������� ���� �Ұ����ϴٸ�
         * Nxt ExtDir �������� �����Ѵ�. */
        aAllocExtDirInfo->mFstExtDescRID     = sFstExtDescRID;
        aAllocExtDirInfo->mFstExtDesc        = sFstExtDesc;
        aAllocExtDirInfo->mNewExtDirPID      = sExtDirPID;
        aAllocExtDirInfo->mNxtPIDOfNewExtDir = sNxtExtDirPID;

        if ( (aAllocExtDirInfo->mNewExtDirPID   == aSegPID)   ||
             (isExeedShrinkThreshold(aSegCache) == ID_FALSE) )
        {
            // Next ExtDir �������� ������ �����ѵ� SegHdr �������̰ų�
            // SHRINK_THREADHOLD�� �������� ���ϸ� Shrink�� �Ұ����ϴ�.
            break;
        }

        sExtDirPID = sNxtExtDirPID; // Go Go Next!!
    }

    if ( sExtDirState[0] != SDPSC_EXTDIR_EXPIRED )
    {
        IDE_ASSERT( aAllocExtDirInfo->mShrinkExtDirPID   == SD_NULL_PID );
        IDE_ASSERT( aAllocExtDirInfo->mNewExtDirPID      == SD_NULL_PID );
        IDE_ASSERT( aAllocExtDirInfo->mNxtPIDOfNewExtDir == SD_NULL_PID );

        if ( sExtDirState[0] == SDPSC_EXTDIR_UNEXPIRED )
        {
            /* B. ������ �� ���ٸ� ���̺� �����̽��κ��� ExtDir�� �Ҵ��Ҽ� �ִ���
             * Ȯ���Ѵ�. �Ҵ�� ExtDir.���� Extent�� ���� ���ִ�.
             * ������, ������Ƽ�� ������ �����Ͽ�, ���׸�Ʈ ���������� MaxExtInDir
             * �����ʹ� �ٸ��� �ִ�. ( �籸���� Undo TBS�� Reset�� �ȵ� ��� ) */
            IDE_TEST( sdptbExtent::tryAllocExtDir(
                                      aStatistics,
                                      aStartInfo,
                                      aSpaceID,
                                      aFreeListIdx,
                                      &aAllocExtDirInfo->mNewExtDirPID )
                      != IDE_SUCCESS );

            if ( aAllocExtDirInfo->mNewExtDirPID != SD_NULL_PID )
            {
                aAllocExtDirInfo->mIsAllocNewExtDir = ID_TRUE;
            }
            else
            {
                /* nothing to do */
            }
        }
        else
        {
            IDE_ASSERT( sExtDirState[0] == SDPSC_EXTDIR_PREPARED );

            /* C. prepareNewPage4Append�� ���ؼ� �̹� ���׸�Ʈ�� �Ҵ�� ����
             *    ��밡���ϴ�. (Next ExtDir�� SegHdr�� �ƴ�.)
             *
             *  BUG-25352 Index SMO �������� prepareNewPage4Append�� ����
             *            UDS�� ExtDirList �� NxtExtDir�� ExtDesc�� Full��
             *            �ƴҼ� ����.
             *
             * prepareNewPage4Append�� �̸� Ȯ���� ExtDir�� ��쿡�� Segment�� �̹�
             * �߰��Ǿ� �ֱ�������(����Ȯ�����Ѵ�), Segment�� ��Ÿ�� �Ҵ������� ��������
             * �ʴ´�. �ش� ExtDir�� ù��° ExtDesc ������ �Ѱ��ָ� �ȴ�.
             */
            aAllocExtDirInfo->mNewExtDirPID     = aNxtExtDirPID;
            aAllocExtDirInfo->mIsAllocNewExtDir = ID_FALSE;
        }

        if ( aAllocExtDirInfo->mNewExtDirPID != SD_NULL_PID )
        {
            IDE_TEST( getExtDescInfo( aStatistics,
                                      aSpaceID,
                                      aAllocExtDirInfo->mNewExtDirPID,
                                      0,  /* aIdx */
                                      &sFstExtDescRID,
                                      &sFstExtDesc ) != IDE_SUCCESS );

            aAllocExtDirInfo->mFstExtDescRID = sFstExtDescRID;
            aAllocExtDirInfo->mFstExtDesc    = sFstExtDesc;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir�� ������ �� �ִ��� Ȯ��
 *
 * ExtDir�� LatestCSCN�� FstDskViewSCN�� ���Ͽ� ���������� �����ϸ�
 * ����(OverWrite) �����ϴ�.
 *
 * (1) Segment�� Ȯ���Ϸ��� Ʈ������� MyFstDskViewSCN�� Extent Cntl Header��
 *     FstDskViewSCN�� �ϴ� �޶�� �Ѵ�.
 *     �ֳ��ϸ�, �ڽ��� �Ҵ��� Extent Dir. �������� �� �ֱ� �����̴�.
 *
 *     aMyFstDskViewSCN != ExtDir.FstDskViewSCN
 *
 * (2) Segment�� Ȯ���Ϸ��� Ʈ������� �˰� �ִ� aSysMinDskViewSCN�� Extent Dir.
 *     Page�� ���������� ����� Ʈ������� Commit �������� Extent Cntl
 *     Header�� �����ߴ� CommitSCN���� Ŀ�� �Ѵ�.
 *
 *     aSysMinDskViewSCN  > ExtDir.LatestCSCN
 *
 * aStatistics     - [IN]  �������
 * aSpaceID        - [IN]  ���̺����̽� ID
 * aCurExtDir      - [IN]  ���� Extent Dir. ������ ID (steal ���)
 * aFstDskViewSCN  - [IN]  Ʈ����� ù��° Dsk Stmt Begin SCN
 * aMinDskViewSCN  - [IN]  Ʈ����� ���۽� �ý��ۿ��� ���� ������ SSCN
 * aExtDirState    - [OUT] ���밡�ɿ���
 * aAllocExtRID    - [OUT] �Ҵ��� Extent Desc. RID
 * aFstExtDesc     - [OUT] �Ҵ��� ù��° Extent Desc. ������
 * aNxtExtDir      - [OUT] ���� Extent Dir. ������ ID
 * aExtCntInExtDir - [OUT] aCurExtDir�� ���Ե� Extent ����.
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::checkExtDirState4Reuse(
                                 idvSQL              * aStatistics,
                                 scSpaceID             aSpaceID,
                                 scPageID              aSegPID,
                                 scPageID              aCurExtDir,
                                 smSCN               * aMyFstDskViewSCN,
                                 smSCN               * aSysMinDskViewSCN,
                                 sdpscExtDirState    * aExtDirState,
                                 sdRID               * aAllocExtRID,
                                 sdpscExtDesc        * aFstExtDesc,
                                 scPageID            * aNxtExtDir,
                                 UInt                * aExtCntInExtDir )
{
    sdpscExtDirCntlHdr   * sCntlHdr       = NULL;
    sdpscExtDesc         * sFstExtDescPtr = NULL;
    UInt                   sState         = 0;

    IDE_ASSERT( aSegPID           != SD_NULL_PID );
    IDE_ASSERT( aCurExtDir        != SD_NULL_PID );
    IDE_ASSERT( aMyFstDskViewSCN  != NULL );
    IDE_ASSERT( aSysMinDskViewSCN != NULL );
    IDE_ASSERT( aExtDirState      != NULL );
    IDE_ASSERT( aNxtExtDir        != NULL );

    *aExtDirState    = SDPSC_EXTDIR_UNEXPIRED;
    *aNxtExtDir      = SD_NULL_PID;

    if ( aFstExtDesc != NULL )
    {
        *aAllocExtRID = SD_NULL_RID;
        initExtDesc( aFstExtDesc );
    }

    if ( aExtCntInExtDir != NULL )
    {
        *aExtCntInExtDir = 0;
    }

    IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                     NULL, /* aMtx */
                                     aSpaceID,
                                     aCurExtDir,
                                     &sCntlHdr ) != IDE_SUCCESS );
    sState = 1;

    IDE_ASSERT( sCntlHdr->mExtCnt > 0 );

    /* BUG-25352 Index SMO�������� prepareNewPage4Append�� ���� UDS�� ExtDirList
     *           �� NxtExtDir�� Extent�� Full�� �ƴ� ��찡 ����. */
    if ( sCntlHdr->mExtCnt != sCntlHdr->mMaxExtCnt )
    {
        /* BUG-34050 when there are XA prepared transactions
         *           and server restart, not reusable undo page is reused.
         * XA prepared transaction�� �����ϴ� ���
         * undo�� �������� �ʱ� ������ restart������ ���Ǵ� extent ������
         * �״�� �����ϰ� �ǰ�, ���� ������� undo �������� flush�Ǹ�
         * mFstDskViewSCN, mLstCommitSCN�� InitSCN�� �ƴ� �� �ִ�.
         * �̷� ���� prepareNewPage4Append �������� �̸� �Ҵ�� extent���
         * ���� ������� prepared ���·� �����Ͽ� ������� �ʵ��� �Ѵ�. */
        *aExtDirState = SDPSC_EXTDIR_PREPARED;
        IDE_CONT( cont_cannot_reusable );
    }
    else
    {
        /* ExtDir�� MaxExtCnt�� ������Ƽ�� ���� 1���� �����Ǳ⵵ �ϱ⶧����
         * ExtCnt == MaxExtCnt ��찡 ������ ���� ������ �����ϸ�
         * prepareNewPage4Append�� ���� Ȯ���� �������� �Ǵ��Ѵ�. */
        if ( (sCntlHdr->mExtCnt == 1) &&
             (SM_SCN_IS_INIT(sCntlHdr->mFstDskViewSCN)) &&
             (SM_SCN_IS_INIT(sCntlHdr->mLstCommitSCN)))
        {
            *aExtDirState = SDPSC_EXTDIR_PREPARED;
            IDE_CONT( cont_cannot_reusable );
        }
    }

    IDE_TEST_CONT( 
            SM_SCN_IS_EQ( &sCntlHdr->mFstDskViewSCN, aMyFstDskViewSCN ),
            cont_cannot_reusable );

    IDE_TEST_CONT( 
            SM_SCN_IS_GE( &sCntlHdr->mLstCommitSCN, aSysMinDskViewSCN ),
            cont_cannot_reusable );

    *aExtDirState    = SDPSC_EXTDIR_EXPIRED;
    *aNxtExtDir      = sCntlHdr->mNxtExtDir;

    if ( aFstExtDesc != NULL )
    {
        sFstExtDescPtr = getExtDescByIdx( sCntlHdr, 0 );
        idlOS::memcpy( aFstExtDesc,
                       sFstExtDescPtr,
                       ID_SIZEOF(sdpscExtDesc) );
        *aAllocExtRID = sdpPhyPage::getRIDFromPtr( sFstExtDescPtr );
    }

    if ( aExtCntInExtDir != NULL )
    {
        *aExtCntInExtDir = sCntlHdr->mExtCnt;
    }

    IDE_EXCEPTION_CONT( cont_cannot_reusable );

    sState = 0;
    IDE_TEST( releaseCntlHdr( aStatistics, sCntlHdr ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( releaseCntlHdr( aStatistics, sCntlHdr )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir�� ������ ����� Ʈ������� CommitSCN�� ������.
 *
 * CMS���� ExtDir�� CommitSCN�� ǥ���Ͽ� ���� Ʈ������� ExtDir�� �����Ϸ���
 * �Ҷ� �Ǵܱ������� ����Ѵ�.
 *
 * aCntlHdr   - [IN] Extent Dir. �������� Control ��� ������
 * aCommitSCN - [IN] Extent Dir. �������� ������ Ʈ������� CommitSCN
 *
 ***********************************************************************/
void sdpscExtDir::setLatestCSCN( sdpscExtDirCntlHdr * aCntlHdr,
                                 smSCN              * aCommitSCN )
{
     SM_SET_SCN( &(aCntlHdr->mLstCommitSCN),
                 aCommitSCN );
}

/***********************************************************************
 *
 * Description : ExtDir�� ����� Ʈ������� FstDskViewSCN�� ������.
 *
 * �ڽ��� ����ϰ� �ϴ� ExtDir�� ǥ���Ͽ� ��������� �ʵ��� �ϱ� �����̴�.
 *
 * aCntlHdr         - [IN] Extent Dir. �������� Control ��� ������
 * aMyFstDskViewSCN - [IN] Extent Dir. �������� ������ Ʈ������� FstDskViewSCN
 *
 ***********************************************************************/
void sdpscExtDir::setFstDskViewSCN( sdpscExtDirCntlHdr * aCntlHdr,
                                    smSCN              * aMyFstDskViewSCN )
{
    SM_SET_SCN( &(aCntlHdr->mFstDskViewSCN), aMyFstDskViewSCN );
}

/***********************************************************************
 *
 * Description : Ʈ����� Commit/Abort�� ����� ExtDir �������� SCN�� ����
 *
 * �ڽ��� ����ϰ� �ϴ� ExtDir�� ǥ���Ͽ� ��������� �ʵ��� �ϱ� �����̴�.
 * FstExtRID�� �����ϴ� Extent Dir.���� LstExtRID�� �����ϴ� Extent Dir.
 * ���� CSCN �Ǵ� ASCN �� �����Ѵ�.
 * No-Logging ���� �����ϰ� �������� dirty��Ų��.
 *
 * aStatistics     - [IN] �������
 * aSpaceID        - [IN] ���̺����̽� ID
 * aSegPID         - [IN] ���׸�Ʈ ������ ID
 * aFstExtRID      - [IN] Ʈ������� ����� ����� ù��° Extent Desc. RID
 * aLstExtRID      - [IN] Ʈ������� ����� ����� ������ Extent Desc. RID
 * aCSCNorASCN     - [IN] Extent Dir. �������� ������ Ʈ������� CommitSCN
 *                        �Ǵ� AbortSCN(GSCN)
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::markSCN4ReCycle( idvSQL          * aStatistics,
                                     scSpaceID         aSpaceID,
                                     scPageID          aSegPID,
                                     sdpSegHandle    * aSegHandle,
                                     sdRID             aFstExtRID,
                                     sdRID             aLstExtRID,
                                     smSCN           * aCSCNorASCN )
{
    scPageID              sCurExtDir;
    scPageID              sLstExtDir;
    scPageID              sNxtExtDir;
    UInt                  sState   = 0;
    sdpscExtDirCntlHdr  * sCntlHdr = NULL;

    IDE_ASSERT( aSegPID    != SD_NULL_PID );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aFstExtRID != SD_NULL_RID );
    IDE_ASSERT( aLstExtRID != SD_NULL_RID );
    IDE_ASSERT( aCSCNorASCN != NULL );
    IDE_ASSERT( SM_SCN_IS_NOT_INIT( *aCSCNorASCN ) );

    sCurExtDir = SD_MAKE_PID( aFstExtRID );
    sLstExtDir = SD_MAKE_PID( aLstExtRID );

    while ( 1 )
    {
        IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                          NULL,  /* aMtx */
                                          aSpaceID,
                                          sCurExtDir,
                                          &sCntlHdr )
                  != IDE_SUCCESS );
        sState = 1;

        setLatestCSCN( sCntlHdr, aCSCNorASCN );

        sNxtExtDir = sCntlHdr->mNxtExtDir;

        sdbBufferMgr::setDirtyPageToBCB(
                                aStatistics,
                                sdpPhyPage::getPageStartPtr( sCntlHdr ) );

        sState = 0;
        IDE_TEST( releaseCntlHdr( aStatistics, sCntlHdr )
                  != IDE_SUCCESS );

        if ( sCurExtDir == sLstExtDir )
        {
            break;
        }

        sCurExtDir = sNxtExtDir;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( releaseCntlHdr( aStatistics, sCntlHdr )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * BUG-31055 Can not reuse undo pages immediately after it is used to 
 *           aborted transaction.
 *
 * Description : ED���� Tablespace�� ��ȯ��.
 *
 * Abort�� Transaction�� ����� ED���� ��� ��Ȱ���ϱ� ����,  TableSpace����
 * ��ȯ�Ѵ�.
 *
 * aStatistics     - [IN] �������
 * aSpaceID        - [IN] ���̺����̽� ID
 * aSegPID         - [IN] ���׸�Ʈ ������ ID
 * aSegHandle      - [IN] ���׸�Ʈ �ڵ�
 * aStartInfo      - [IN] Mtx ���� ����
 * aFreeListIdx    - [IN] FreeList ��ȣ (Tran or Undo )
 * aFstExtRID      - [IN] ��ȯ�� ù��° Extent Desc. RID
 * aLstExtRID      - [IN] ��ȯ�� ������ Extent Desc. RID
 *
 * Issue :
 * Segment�� ExtentDirectoryList�� Singly-linked list�� �����ȴ�.
 *
 * [First] -> [ A ] -> [ B ] -> [ C ] -> [ Last ] ->...
 * 
 * 1) First�� Last�� shrink�ϸ� �ȵȴ�.
 *    First�� ���, �� Transaction�� UndoRecord�� ����ֱ� �����̴�.
 *    Last�� ���, ���� Ŀ���� ����Ű�� �ֱ� ������ �� �ٽ� ���� ���̱�
 *    �����̴�.
 *
 * 2) �� �������� shrink�Ϸ���, PrevPID, CurrPID, NextPID�� �ʿ��ϴ�.
 *    PrevPID�� ED�� NextPID�� ����Ű���� �����Ѵ�.
 *    CurrPID�� Tablespace�� ��ȯ�ȴ�.
 *    ���� CurrPID�� SegHdr�� LastED�� ���, SegHdr�� mLastED�� �����Ѵ�.
 *
 * 3) Shrink Threshold�� �����ϰ� ��� shrink�Ѵ�.
 *    �⺻������ Seg ũ�Ⱑ Shrink Threshold ������ ���, shrink ���� �ʴ´�.
 *    ������ Abort�� Transaction�� ED�� ������ �ʿ���� UndoRecord�� ����
 *    �ֱ� ������, ������ ��ȯ���� ��Ȱ��� �� �ֵ��� �Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::shrinkExts( idvSQL            * aStatistics,
                                scSpaceID           aSpaceID,
                                scPageID            aSegPID,
                                sdpSegHandle      * aSegHandle,
                                sdrMtxStartInfo   * aStartInfo,
                                sdpFreeExtDirType   aFreeListIdx,
                                sdRID               aFstExtRID,
                                sdRID               aLstExtRID )
{
    scPageID              sCurExtDir;
    scPageID              sLstExtDir;
    scPageID              sPrvExtDir = SC_NULL_PID;
    scPageID              sNxtExtDir;
    UInt                  sState     = 0;
    sdpscExtDirCntlHdr  * sCntlHdr   = NULL;
    sdrMtx                sMtx;
    UInt                  sTotExtCnt = 0;
    UInt                  sCheckCnt  = 0;

    IDE_ASSERT( aSegPID    != SD_NULL_PID );
    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aFstExtRID != SD_NULL_RID );
    IDE_ASSERT( aLstExtRID != SD_NULL_RID );

    /* 1. Initialize */
    sPrvExtDir = SD_MAKE_PID( aFstExtRID );
    sLstExtDir = SD_MAKE_PID( aLstExtRID );

    /* First�� Last�� ������ ���, Skip */
    IDE_TEST_CONT( SD_MAKE_PID( aFstExtRID ) == SD_MAKE_PID( aLstExtRID ),
                    SKIP );

    /* 2. Set CurExtDir
     * Cur = Prv->Next; */
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_FALSE,/*Undoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                     &sMtx,
                                     aSpaceID,
                                     sPrvExtDir,
                                     &sCntlHdr )
              != IDE_SUCCESS );
    sCurExtDir = sCntlHdr->mNxtExtDir;

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    /* First �ٷ� ������ Last�� ��쵵 Skip.
     * First�� Last ������ ExtDir���� shrink�ؾ� �Ѵ�. */
    IDE_TEST_CONT( sCurExtDir == sLstExtDir, SKIP );

    /* 3. Loop */
    while ( 1 )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_FALSE,/*Undoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                          &sMtx,
                                          aSpaceID,
                                          sCurExtDir,
                                          &sCntlHdr )
                  != IDE_SUCCESS );

        /* 4. NXT = CUR->NXT */
        sNxtExtDir = sCntlHdr->mNxtExtDir;

        /* ���� ��Ȳ�� ���� �� ����. */
        IDE_TEST_RAISE( sCurExtDir == sPrvExtDir, ERR_ASSERT );
        IDE_TEST_RAISE( sCurExtDir == sNxtExtDir, ERR_ASSERT );
        IDE_TEST_RAISE( sPrvExtDir == sNxtExtDir, ERR_ASSERT );
        IDE_TEST_RAISE( sCurExtDir == sLstExtDir, ERR_ASSERT );

        /* 5. if CUR != SEGHDR
         * Segment Header�� shrink�ϸ� �ȵȴ�. */
        if ( sCurExtDir != aSegPID )
        {
            /* 6. PRV -> NXT = NXT */
            IDE_TEST( sdpscSegHdr::removeExtDir( aStatistics,
                                                 &sMtx,
                                                 aSpaceID,
                                                 aSegPID,
                                                 sPrvExtDir,
                                                 sCurExtDir,
                                                 sNxtExtDir,
                                                 sCntlHdr->mExtCnt,
                                                 &sTotExtCnt )
                      != IDE_SUCCESS );

            /* �ϳ��� Extent�� ���� ��Ȳ�� �� �� �����ϴ�. */
            /* Rollback�������� ȣ��Ǵ� �Լ��Դϴ�.
             * �����ϸ� ����ó���� �� �� ���� ������ ����մϴ�. */
            IDE_TEST_RAISE( sTotExtCnt == 0, ERR_ASSERT );

            IDE_TEST( sdptbExtent::freeExtDir( aStatistics,
                                               &sMtx,
                                               aSpaceID,
                                               aFreeListIdx,
                                               sCurExtDir )
                      != IDE_SUCCESS );
        }
        else
        {
            /* 7. PRV = CUR; */
            /* SegHdr�� Skip�߱� ������, Prv�� �Űܾ� �Ѵ�.
            *
             * ��)
             * 0 -> 1 -> 2 -> 3 -> 0   (0�� ExtDir)
             * 3 = Prv, 0 = Cur, 1 = Nxt�� ��Ȳ
             *
             * �� ��Ȳ���� Cur�� 0�� Seghdr�̱� ������ Shrink�ϸ� �ȵȴ�.
             * ���� Skip�� 1�� Shrink�ؾ� �ϴ� ��Ȳ���� ������ �Ѵ�.
             * �׷��Ƿ�,
             * 0 = Prv, 1 = Cur, 2 = Nxt�� ��Ȳ���� ������ �Ѵ�. */
            sPrvExtDir = sCurExtDir;
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

        /* 8. if( NXT == LST) Break */
        /* ���� ED�� �������̸� �����Ѵ�. */
        if ( sNxtExtDir == sLstExtDir )
        {
            break;
        }
        else
        {
            /* nothing to do */
        }

        /* 9. CUR = NXT */
        sCurExtDir = sNxtExtDir;

        sCheckCnt++;
        IDE_TEST_RAISE( sCheckCnt >= ( ID_UINT_MAX / 8 ), ERR_ASSERT );
        /* UndoED�� �ּ� ũ��� Page 8�� �̴�.
         * (SYS_UNDO_TBS_EXTENT_SIZE�� �������� �ּҰ��� 65536�̴�)
         * ���� �� �̻� ������ ���� �ִٴ� ���� ���ѷ����̴�.
         * �� ��� UndoTBS�� �������� ���̱� ������ �����Ų��. */
        /* �� ��� MiniTransaction�� ��û�� Log�� ������ ���̱�
           ������, �̸� ���� ���� �����ϴ�. */
    }

    /* BUG-31171 set incorrect undo segment size when shrinking 
     * undo extent for aborted transaction. */
    /* Segment Cache�� ����Ǵ� Segment Size�� ���� isExeedShrinkThreshold
     * �� ���� Shrink ���θ� �Ǵ��Ҷ� �̿�ȴ�. */
    if ( sTotExtCnt > 0 )
    {
        sdpscCache::setSegSizeByBytes(
            aSegHandle,
            ((sdpscSegCache*)aSegHandle->mCache)->mCommon.mPageCntInExt 
            * SD_PAGE_SIZE * sTotExtCnt );
    }
    else
    {
        /* sTotExtCnt�� 0���� �̾߱�� shrink�� ���� �ʾұ⿡ ��������
         * �ʾҴٴ� ���̴�. ���� �ƹ��͵� �� ���� ����. */
    }

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ASSERT );
    {
        ideLog::log( IDE_SERVER_0,
                     "aSpaceID          =%u\n"
                     "aSegPID           =%u\n"
                     "sPrvExtDir        =%u\n"
                     "sCurExtDir        =%u\n"
                     "sNxtExtDir        =%u\n"
                     "sCntlHdr->mExtCnt =%u\n"
                     "aFstExtRID        =%llu\n"
                     "aLstExtRID        =%llu\n"
                     "sTotExtCnt        =%u\n"
                     "sCheckCnt         =%u\n",
                     aSpaceID,
                     aSegPID,
                     sPrvExtDir,
                     sCurExtDir,
                     sNxtExtDir,
                     sCntlHdr->mExtCnt,
                     aFstExtRID,
                     aLstExtRID,
                     sTotExtCnt,
                     sCheckCnt );

        IDE_ASSERT( 0 );
    }

    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch ( sState )
    {
        case 1:
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
            break;
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ExtDir�� split �ؼ� ���� ExtDir�� shrink ��Ŵ
 *                      
 * aStatistics     - [IN] �������
 * aSpaceID        - [IN] ���̺����̽� ID
 * aToSegPID       - [IN] ���׸�Ʈ ������ ID
 * aToExtDirPID    - [IN] split ��� ExtDirPID
 ***********************************************************************/
IDE_RC sdpscExtDir::shrinkExtDir( idvSQL                * aStatistics,
                                  sdrMtx                * aMtx,
                                  scSpaceID               aSpaceID,
                                  sdpSegHandle          * aToSegHandle,
                                  sdpscAllocExtDirInfo  * aAllocExtDirInfo )
{
    sdpSegType           sSegType;
    UChar              * sExtDirPagePtr;
    scPageID             sNewExtDirPID;
    sdpscExtDirCntlHdr * sNewExtDirCntlHdr;
    sdpscExtDirCntlHdr * sAllocExtDirCntlHdr;
    sdRID                sExtDescRID;
    sdRID                sNxtExtRID;
    sdpscExtDesc         sExtDesc;
    sdpFreeExtDirType    sFreeListIdx;
    SInt                 i;
    SInt                 sLoop;
    UShort               sMaxExtDescCnt;
    UInt                 sExtCnt;
    SInt                 sDebugLoopCnt;
    UInt                 sDebugTotalExtDescCnt;
    smLSN                sNTA;
    void               * sTrans;

    /* rp������ aStatistics�� null�� �Ѿ�ü� �ִ�. */
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( aToSegHandle != NULL );
    IDE_ERROR( aAllocExtDirInfo != NULL );
    
    sSegType = ((sdpscSegCache*)aToSegHandle->mCache)->mCommon.mSegType; 
    sTrans   = sdrMiniTrans::getTrans( aMtx );
    sNTA     = smLayerCallback::getLstUndoNxtLSN( sTrans );

    IDE_ERROR( (sSegType == SDP_SEG_TYPE_TSS) ||
               (sSegType == SDP_SEG_TYPE_UNDO) );

    if ( sSegType == SDP_SEG_TYPE_TSS )
    {
        sFreeListIdx = SDP_TSS_FREE_EXTDIR_LIST;
    }
    else
    {
        sFreeListIdx = SDP_UDS_FREE_EXTDIR_LIST;
    }

    sMaxExtDescCnt = sdpscSegHdr::getMaxExtDescCnt( sSegType );

    /* TargetExtDir�� To Segment �� MaxExtCnt �� �ٸ��ٸ�
       ���� �ٸ� Ÿ���� Segment���� steal�� �� ��찡 �ȴ�.
       split�ؼ� To.mMaxExtCnt���� Ext�� freelist�� �Ŵܴ�.  */
    if ( aAllocExtDirInfo->mExtCntInShrinkExtDir > sMaxExtDescCnt )
    {
        /* NewExtDir �� ExtDirCntlHdr �� ��� ���� ���� */
        IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                          aMtx,  /* aMtx */
                                          aSpaceID,
                                          aAllocExtDirInfo->mNewExtDirPID,
                                          &sAllocExtDirCntlHdr )
                   != IDE_SUCCESS );

        sLoop = ( aAllocExtDirInfo->mExtCntInShrinkExtDir + (sMaxExtDescCnt-1) ) / 
                sMaxExtDescCnt;

        /* loop �� �ѹ��̸� �������� ���ü� ������.  */
        IDE_DASSERT( sLoop > 1 ); 

        sDebugTotalExtDescCnt = 0;

        for ( i= (sLoop-1) ; i >= 0 ; i-- )
        { 
            sExtCnt = sMaxExtDescCnt;

            IDE_TEST( sdpscExtDir::getExtDescInfo( aStatistics,
                                                   aSpaceID,
                                                   aAllocExtDirInfo->mNewExtDirPID,
                                                   (UShort)(i * sMaxExtDescCnt), /*aIdx*/
                                                   &sExtDescRID,
                                                   &sExtDesc ) 
                      != IDE_SUCCESS );

            IDE_ERROR( sExtDesc.mExtFstPID != SD_NULL_RID );
            IDE_ERROR( sExtDesc.mLength > 0 );

            /* Ext�� ExtDir�� ������ */
            if ( aAllocExtDirInfo->mNewExtDirPID != sExtDesc.mExtFstPID )
            {
                /* ������ Extent ù page�� ExtDir ���� */
                sNewExtDirPID = sExtDesc.mExtFstPID;
                /* ù��° Ext�� FstDataPID �� �ι�° ������ ���� */
                sExtDesc.mExtFstDataPID = sExtDesc.mExtFstPID + 1;

                IDE_TEST( sdpscExtDir::createAndInitPage(
                                    aStatistics,
                                    aMtx,
                                    aSpaceID,
                                    sNewExtDirPID,                       /* aNewExtDirPID */
                                    aAllocExtDirInfo->mNxtPIDOfNewExtDir,/* aNxtExtDirPID */
                                    sMaxExtDescCnt, 
                                    &sExtDirPagePtr )
                           != IDE_SUCCESS );

                sdrMiniTrans::setNTA( aMtx,
                                      aSpaceID,
                                      SDR_OP_NULL,
                                      &sNTA,
                                      NULL,
                                      0 /* Data Count */);

                IDE_TEST( sdpscExtDir::fixAndGetCntlHdr4Write(
                                                         aStatistics,
                                                         aMtx,
                                                         aSpaceID,
                                                         sNewExtDirPID,
                                                         &sNewExtDirCntlHdr )
                          != IDE_SUCCESS );

                IDE_TEST( sdpscExtDir::logAndAddExtDesc( aMtx,
                                                         sNewExtDirCntlHdr,
                                                         &sExtDesc )
                          != IDE_SUCCESS );
                sDebugTotalExtDescCnt++;

                sDebugLoopCnt = 0;
                while ( --sExtCnt > 0 )
                {
                    getNxtExt( sAllocExtDirCntlHdr, sExtDescRID, &sNxtExtRID, &sExtDesc ); 

                    if ( sNxtExtRID == SD_NULL_PID )
                    {
                        break;
                    }

                    IDE_ERROR( sExtDesc.mExtFstPID != SD_NULL_RID );
                    IDE_ERROR( sExtDesc.mLength > 0 );

                    sExtDescRID = sNxtExtRID;

                    IDE_TEST( sdpscExtDir::logAndAddExtDesc( aMtx,
                                                             sNewExtDirCntlHdr,
                                                             &sExtDesc )
                              != IDE_SUCCESS );
                    sDebugTotalExtDescCnt++;
                    /* xxSEG_EXTDESC_COUNT_PER_EXTDIR�� �ִ밪�� 128
                        ���̻� loop �� ���� �ִٴ� ���� ���� ��Ȳ�� */
                    sDebugLoopCnt++;
                    IDE_ERROR( sDebugLoopCnt < 128 )
                }

                IDE_TEST( sdptbExtent::freeExtDir( aStatistics,
                                                   aMtx,
                                                   aSpaceID,
                                                   sFreeListIdx,
                                                   sNewExtDirPID )
                          != IDE_SUCCESS );
            }
            else
            {
                /* ExtDir�� �����Ƿ� CntlHdr ���� ������ */
                sAllocExtDirCntlHdr->mExtCnt = sMaxExtDescCnt;          
                sAllocExtDirCntlHdr->mMaxExtCnt = sMaxExtDescCnt;              

                IDE_TEST( sdrMiniTrans::writeNBytes( 
                                         aMtx,
                                         (UChar*)&(sAllocExtDirCntlHdr->mMaxExtCnt),
                                         &sAllocExtDirCntlHdr->mExtCnt,
                                         ID_SIZEOF( sAllocExtDirCntlHdr->mExtCnt ) )
                          != IDE_SUCCESS );

                sDebugTotalExtDescCnt += sAllocExtDirCntlHdr->mExtCnt ;
            }
        }

        IDE_ERROR( sDebugTotalExtDescCnt == aAllocExtDirInfo->mExtCntInShrinkExtDir );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ����� ������� �Ҵ�� ����� ExtDir ��������
 *               Ʈ����� ù��° DskViewSCN ����
 *
 * �ڽ��� ����ϰ� �ϴ� ExtDir�� ǥ���Ͽ� ��������� �ʵ��� �ϱ� �����̴�.
 * No-Logging ���� �����ϰ� �������� dirty��Ų��.
 *
 * aStatistics       - [IN] �������
 * aSpaceID          - [IN] ���̺����̽� ID
 * aExtRID           - [IN] Ʈ������� ����� Extent Desc. RID
 * aMyFstDskViewSCN  - [IN] Extent Dir. �������� ������ FstDskViewSCN
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::setSCNAtAlloc( idvSQL        * aStatistics,
                                   scSpaceID       aSpaceID,
                                   sdRID           aExtRID,
                                   smSCN         * aMyFstDskViewSCN )
{
    sdpscExtDirCntlHdr  * sCntlHdr;
    smSCN                 sInfiniteSCN;

    IDE_ASSERT( aExtRID          != SD_NULL_RID );
    IDE_ASSERT( aMyFstDskViewSCN != NULL );

    IDE_TEST( fixAndGetCntlHdr4Write( aStatistics,
                                      NULL, /* aMtx */
                                      aSpaceID,
                                      SD_MAKE_PID( aExtRID ),
                                      &sCntlHdr )
              != IDE_SUCCESS );

    setFstDskViewSCN( sCntlHdr, aMyFstDskViewSCN );
    /* BUG-30567 Users need the function that check the amount of 
     * usable undo tablespace. 
     * UndoRecord��Ͻ�, LatestCommitSCN�� ���Ѵ�� �ھ��ݴϴ�.
     * ���� �������� Transaction�� UndoPage�鵵 ������� ���Ѵٰ�
     * �ν��ϱ� �����Դϴ�.
     * ���� Commit/Rollback�� ��� CommitSCN���� �ٽ� �����鼭
     * �� ���� �����˴ϴ�.*/
    SM_SET_SCN_INFINITE( &sInfiniteSCN );
    setLatestCSCN( sCntlHdr, &sInfiniteSCN );

    sdbBufferMgr::setDirtyPageToBCB(
                        aStatistics,
                        sdpPhyPage::getPageStartPtr((UChar*)sCntlHdr) );

    IDE_TEST( releaseCntlHdr( aStatistics, sCntlHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : aCurExtRID�� ����Ű�� Extent�� aPrvAllocPageID����
 *               Page�� �����ϴ� �ϴ��� üũ�ؼ� ������ ���ο� ����
 *               Extent�� �̵��ϰ� ���� Extent���� Free Page�� ã�Ƽ�
 *               Page�� �Ҵ�� ExtRID�� PageID�� �Ѱ��ش�.
 *
 *               ���� Extent�� ������ aNxtExtRID�� aFstDataPIDOfNxtExt
 *               �� SD_NULL_RID, SD_NULL_PID�� �ѱ��.
 *
 * Caution:
 *
 *  1. �� �Լ��� ȣ��ɶ� SegHdr�� �ִ� �������� XLatch��
 *     �ɷ� �־�� �Ѵ�.
 *
 * aStatistics         - [IN] ��� ����
 * aSpaceID            - [IN] TableSpace ID
 * aSegHdr             - [IN] Segment Header
 * aCurExtRID          - [IN] ���� Extent RID
 *
 * aNxtExtRID          - [OUT] ���� Extent RID
 * aFstPIDOfExt        - [OUT] ���� Extent�� ù��° PageID
 * aFstDataPIDOfNxtExt - [OUT] ���� Extent �� ù��° Data Page ID,
 *                             Extent�� ù��° �������� Extent Dir
 *                             Page�� ���Ǳ⵵ �Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::getNxtExt4Alloc( idvSQL       * aStatistics,
                                     scSpaceID      aSpaceID,
                                     sdRID          aCurExtRID,
                                     sdRID        * aNxtExtRID,
                                     scPageID     * aFstPIDOfExt,
                                     scPageID     * aFstDataPIDOfNxtExt )
{
    sdpscExtDesc          sExtDesc;
    scPageID              sExtDirPID;
    sdRID                 sNxtExtRID = SD_NULL_RID;
    sdpscExtDirCntlHdr  * sExtDirCntlHdr;
    SInt                  sState = 0;

    IDE_ASSERT( aSpaceID            != 0 );
    IDE_ASSERT( aCurExtRID          != SD_NULL_RID );
    IDE_ASSERT( aNxtExtRID          != NULL );
    IDE_ASSERT( aFstPIDOfExt        != NULL );
    IDE_ASSERT( aFstDataPIDOfNxtExt != NULL );

    sExtDirPID = SD_MAKE_PID( aCurExtRID );

    IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                     NULL,
                                     aSpaceID,
                                     sExtDirPID,
                                     &sExtDirCntlHdr )
              != IDE_SUCCESS );
    sState = 1;

    /* aCurExtRID ���� Extent�� ���� ExtDirPage���� �����ϴ��� �˻� */
    getNxtExt( sExtDirCntlHdr, aCurExtRID, &sNxtExtRID, &sExtDesc );

    sState = 0;
    IDE_TEST( releaseCntlHdr( aStatistics, sExtDirCntlHdr )
              != IDE_SUCCESS );

    if ( sNxtExtRID != SD_NULL_RID )
    {
        *aNxtExtRID          = sNxtExtRID;
        *aFstPIDOfExt        = sExtDesc.mExtFstPID;
        *aFstDataPIDOfNxtExt = sExtDesc.mExtFstDataPID;
    }
    else
    {
        *aNxtExtRID          = SD_NULL_RID;
        *aFstDataPIDOfNxtExt = SD_NULL_PID;
        *aFstPIDOfExt        = SD_NULL_PID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if ( sState != 0 )
    {
        IDE_ASSERT( releaseCntlHdr( aStatistics, sExtDirCntlHdr )
                    == IDE_SUCCESS );
    }
    IDE_POP();

    *aNxtExtRID          = SD_NULL_RID;
    *aFstPIDOfExt        = SD_NULL_PID;
    *aFstDataPIDOfNxtExt = SD_NULL_PID;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : aExtDirPID�� ����Ű�� ���������� OUT������ ������ ����
 *               �´�. aExtDirPID�� ����Ű�� �������� Extent Directory
 *               Page�̾�� �Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceID    - [IN] TableSpace ID
 * aExtDirPID  - [IN] Extent Directory PageID
 * aIdx        - [IN] ���Ϸ��� ExtDescInfo�� �ε��� 0:ù��° ExtDesc
 * aExtRID     - [OUT] aIdx��° Extent RID
 * aExtDescPtr - [OUT] aIdx��° Extent Desc�� ����� ����
 *
 **********************************************************************/
IDE_RC sdpscExtDir::getExtDescInfo( idvSQL        * aStatistics,
                                    scSpaceID       aSpaceID,
                                    scPageID        aExtDirPID,
                                    UShort          aIdx,
                                    sdRID         * aExtRID,
                                    sdpscExtDesc  * aExtDescPtr )
{
    SInt                 sState = 0;
    sdpscExtDesc       * sExtDescPtr;
    sdpscExtDirCntlHdr * sExtDirCntlHdr = NULL;

    IDE_ASSERT( aSpaceID    != 0 );
    IDE_ASSERT( aExtDirPID  != SD_NULL_PID );
    IDE_ASSERT( aExtRID     != NULL );
    IDE_ASSERT( aExtDescPtr != NULL );

    initExtDesc( aExtDescPtr );

    IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                     NULL,  /* aMtx */
                                     aSpaceID,
                                     aExtDirPID,
                                     &sExtDirCntlHdr )
              != IDE_SUCCESS );
    sState = 1;

    if ( sExtDirCntlHdr->mExtCnt != 0 )
    {
        *aExtRID = SD_MAKE_RID( aExtDirPID,
                                calcDescIdx2Offset( sExtDirCntlHdr, aIdx ) );

        sExtDescPtr  = getExtDescByIdx( sExtDirCntlHdr, aIdx );
        *aExtDescPtr = *sExtDescPtr;
    }
    else
    {
        *aExtRID = SD_NULL_RID;
    }

    sState = 0;
    IDE_TEST( releaseCntlHdr( aStatistics, sExtDirCntlHdr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        IDE_ASSERT( releaseCntlHdr( aStatistics, sExtDirCntlHdr )
                    == IDE_SUCCESS );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Next ExtDir �������� Expire�� �Ǿ����� �˻��� �� ��ȯ
 *
 * ���׸�Ʈ �������� ���� ExtDir�� Expired�Ǿ����� Ȯ���Ѵ�.
 *
 * aStatistics      - [IN]  �������
 * aStartInfo       - [IN]  Mtx ���� ����
 * aSpaceID         - [IN]  ���̺����̽� ID
 * aSegHdrPID       - [IN]  ���׸�Ʈ ��� ������ ID
 * aNxtExtDirPID    - [IN]  Steal �������� �˻��� ExtDir PID
 * aAllocExtDirInfo - [OUT] Steal ������ ExtDir �������� �Ҵ翡 ���� ����
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::checkNxtExtDir4Steal( idvSQL               * aStatistics,
                                          sdrMtxStartInfo      * aStartInfo,
                                          scSpaceID              aSpaceID,
                                          scPageID               aSegHdrPID,
                                          scPageID               aNxtExtDirPID,
                                          sdpscAllocExtDirInfo * aAllocExtDirInfo )
{
    scPageID         sNewNxtExtDirPID;
    scPageID         sNewCurExtDirPID;
    smSCN            sMyFstDskViewSCN;
    smSCN            sSysMinDskViewSCN;
    sdpscExtDirState sExtDirState;
    UInt             sExtCntInExtDir;


    IDE_ASSERT( aStartInfo       != NULL );
    IDE_ASSERT( aSegHdrPID       != SD_NULL_PID );
    IDE_ASSERT( aNxtExtDirPID    != SD_NULL_PID );
    IDE_ASSERT( aAllocExtDirInfo != NULL );

    initAllocExtDirInfo( aAllocExtDirInfo );

    sNewCurExtDirPID = SD_NULL_PID;
    sNewNxtExtDirPID = SD_NULL_PID;
    sExtCntInExtDir  = 0;

    sMyFstDskViewSCN = smxTrans::getFstDskViewSCN( aStartInfo->mTrans );
    SMX_GET_MIN_DISK_VIEW( &sSysMinDskViewSCN );

    /* Next ExtDir�� SegHdr�� ��� ��ƿ�Ҽ� ���� */
    IDE_TEST_CONT( aSegHdrPID == aNxtExtDirPID, CONT_CANT_STEAL ); 

    IDE_TEST( checkExtDirState4Reuse( aStatistics,
                                      aSpaceID,
                                      aSegHdrPID,
                                      aNxtExtDirPID,
                                      &sMyFstDskViewSCN,
                                      &sSysMinDskViewSCN,
                                      &sExtDirState,
                                      NULL, /* aAllocExtRID */
                                      NULL, /* aExtDesc */
                                      &sNewNxtExtDirPID,
                                      &sExtCntInExtDir ) 
              != IDE_SUCCESS );

    if ( sExtDirState == SDPSC_EXTDIR_EXPIRED )
    {
        sNewCurExtDirPID = aNxtExtDirPID;
    }
    else
    {
        // Next ExtDir�� ������ �� ���ų� prepareNewPage4Append�� Ȯ����
        // ExtDir�� Steal�� �� ����.
    }

    IDE_EXCEPTION_CONT( CONT_CANT_STEAL ); 

    aAllocExtDirInfo->mNewExtDirPID         = sNewCurExtDirPID;
    aAllocExtDirInfo->mNxtPIDOfNewExtDir    = sNewNxtExtDirPID;
    aAllocExtDirInfo->mExtCntInShrinkExtDir = sExtCntInExtDir;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : ExtDir Control Header�� Next ExtDir �������� PID ����
 *
 * aMtx           - [IN] Mtx ������
 * aExtDirCntlHdr - [IN] ExtDir Control Header ������
 * aNxtExtDirPID  - [IN] Next ExtDir �������� PID
 *                       SD_NULL_PID�� ���� ������ ExtDir �������� ����̴�.
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::setNxtExtDir( sdrMtx             * aMtx,
                                  sdpscExtDirCntlHdr * aExtDirCntlHdr,
                                  scPageID             aNxtExtDirPID )
{
    IDE_ASSERT( aMtx           != NULL );
    IDE_ASSERT( aExtDirCntlHdr != NULL );

    return sdrMiniTrans::writeNBytes( aMtx,
                                      (UChar*)&aExtDirCntlHdr->mNxtExtDir,
                                      &aNxtExtDirPID,
                                      ID_SIZEOF( aNxtExtDirPID ) );
}


/***********************************************************************
 *
 * Description : ���� ExtDir �������� �پ��� ������ �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aStartInfo     - [IN] Mtx ���� ����
 * aSpaceID       - [IN] ���̺����̽� ID
 * aSegHandle     - [IN] ���׸�Ʈ �ڵ�
 * aCurExtDir     - [IN] �ڷḦ ������ Extent Dir. �������� PID
 * aCurExtDirInfo - [OUT] ���� Extent Dir. �������� ����
 *
 ***********************************************************************/
IDE_RC sdpscExtDir::getCurExtDirInfo( idvSQL           * aStatistics,
                                      scSpaceID          aSpaceID,
                                      sdpSegHandle     * aSegHandle,
                                      scPageID           aCurExtDir,
                                      sdpscExtDirInfo  * aCurExtDirInfo )
{
    scPageID             sSegPID;
    sdpscSegMetaHdr    * sSegHdr;
    sdpscExtDirCntlHdr * sExtDirCntlHdr;
    UShort               sExtCntInExtDir;
    UInt                 sFreeDescCntOfExtDir;
    UInt                 sState = 0;

    IDE_ASSERT( aSegHandle     != NULL );
    IDE_ASSERT( aCurExtDir     != SD_NULL_PID );
    IDE_ASSERT( aCurExtDirInfo != NULL );

    sSegPID = aSegHandle->mSegPID;
    initExtDirInfo( aCurExtDirInfo );

    aCurExtDirInfo->mExtDirPID = aCurExtDir;

    if ( aCurExtDir != sSegPID )
    {
        IDE_TEST( fixAndGetCntlHdr4Read( aStatistics,
                                         NULL,     /* aMtx */
                                         aSpaceID,
                                         aCurExtDir,
                                         &sExtDirCntlHdr )
                  != IDE_SUCCESS );
        sState = 1;
    }
    else
    {
        // Segment Header�� �����ϴ� ���
        IDE_TEST( sdpscSegHdr::fixAndGetHdr4Read( 
                                           aStatistics,
                                           NULL, /* aMtx */
                                           aSpaceID,
                                           sSegPID,
                                           &sSegHdr )
                  != IDE_SUCCESS );
        sState = 2;

        IDE_ASSERT( aCurExtDirInfo->mExtDirPID == sSegPID );

        sExtDirCntlHdr = sdpscSegHdr::getExtDirCntlHdr( sSegHdr );
    }

    aCurExtDirInfo->mNxtExtDirPID = sExtDirCntlHdr->mNxtExtDir;
    aCurExtDirInfo->mMaxExtCnt    = sExtDirCntlHdr->mMaxExtCnt;

    // ���� ExtDir �������� freeSlot ������ ���Ѵ�.
    sFreeDescCntOfExtDir = getFreeDescCnt( sExtDirCntlHdr );
    sExtCntInExtDir      = sExtDirCntlHdr->mExtCnt;

    if ( sState == 1 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sExtDirCntlHdr )
                  != IDE_SUCCESS );
    }

    if ( sState == 2 )
    {
        sState = 0;
        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             (UChar*)sSegHdr )
                != IDE_SUCCESS );
    }

    aCurExtDirInfo->mTotExtCnt = sExtCntInExtDir;

    if ( sFreeDescCntOfExtDir == 0 )
    {
        // ExtDir Extent Map�� �����ؼ� ����ؾ��ϴ� ���
        aCurExtDirInfo->mIsFull    = ID_TRUE;
    }
    else
    {
        // ExtDir�� ���� ExtDir �������� ����� �� �ִ� ���
        aCurExtDirInfo->mIsFull    = ID_FALSE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch ( sState )
    {
        case 2 :
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   (UChar*)sSegHdr )
                        == IDE_SUCCESS );
            break;
        case 1:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   (UChar*)sExtDirCntlHdr )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}
