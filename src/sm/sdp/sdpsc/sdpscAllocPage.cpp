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
 * $Id: sdpscAllocPage.cpp 82075 2018-01-17 06:39:52Z jina.kim $
 *
 * �� ������ Circular-List Managed Segment���� ������� �Ҵ� ���� ���� STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpReq.h>
# include <sdpPhyPage.h>

# include <sdpscSH.h>
# include <sdpscED.h>

# include <sdpscSegDDL.h>
# include <sdpscAllocPage.h>


/***********************************************************************
 * Description : �������� �����ϰ�, �ʿ��� ������ Ÿ�� �ʱ�ȭ �� logical
 *               Header�� �ʱ�ȭ�Ѵ�.
 *
 * aStatistics   - [IN]  �������
 * aSpaceID      - [IN]  ���̺����̽� ID
 * aNewPageID    - [IN]  ������ ������ ID
 * aPageType     - [IN]  ������ �������� Ÿ��
 * aParentInfo   - [IN]  ������ ������ ����� ����� ���� ����� ����
 *                       ������ ������� ����
 * aPageBitSet   - [IN]  ������ ��������� ���°�
 * aMtx4Latch    - [IN]  �������� ���� X-Latch�� �����ؾ��� Mtx ������
 * aMtx4Logging  - [IN]  ������ �����α׸� ����ϱ� ���� Mtx ������
 *                       aMtx4Latch�� ������ �������ϼ��� �ִ�.
 * aNewPagePtr   - [OUT] ������ �� �������� ���� ������
 *
 ***********************************************************************/
IDE_RC sdpscAllocPage::createPage( idvSQL           * aStatistics,
                                   scSpaceID          aSpaceID,
                                   scPageID           aNewPageID,
                                   sdpPageType        aPageType,
                                   sdpParentInfo    * aParentInfo,
                                   sdpscPageBitSet    aPageBitSet,
                                   sdrMtx           * aMtx4Latch,
                                   sdrMtx           * aMtx4Logging,
                                   UChar           ** aNewPagePtr )
{
    UChar         * sNewPagePtr;

    IDE_DASSERT( aNewPageID   != SD_NULL_PID );
    IDE_DASSERT( aParentInfo  != NULL );
    IDE_DASSERT( aMtx4Latch   != NULL );
    IDE_DASSERT( aMtx4Logging != NULL );
    IDE_DASSERT( aNewPagePtr  != NULL );

    IDE_TEST( sdbBufferMgr::createPage( aStatistics,
                                        aSpaceID,
                                        aNewPageID,
                                        aPageType,
                                        aMtx4Latch,
                                        &sNewPagePtr ) != IDE_SUCCESS );

    IDE_TEST( formatPageHdr( (sdpPhyPageHdr*)sNewPagePtr,
                             aNewPageID,
                             aPageType,
                             aParentInfo,
                             aPageBitSet,
                             aMtx4Logging ) != IDE_SUCCESS );

    if ( aMtx4Latch != aMtx4Logging )
    {
        IDE_TEST( sdrMiniTrans::setDirtyPage( aMtx4Logging, sNewPagePtr )
                  != IDE_SUCCESS );
    }

    *aNewPagePtr = sNewPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : phyical hdr�� logical hdr�� page type�� �°� format�Ѵ�.
 *
 * aNewPagePtr   - [IN] �ʱ�ȭ�� �������� ���� ������
 * aNewPageID    - [IN] ������ ������ ID
 * aPageType     - [IN] ������ �������� Ÿ��
 * aParentInfo   - [IN] ������ ������ ����� ����� ���� ����� ����
 *                      ������ ������� ����
 * aPageBitSet   - [IN] ������ ��������� ���°�
 * aMtx          - [IN] ������ �����α׸� ����ϱ� ���� Mtx ������
 *
 ***********************************************************************/
IDE_RC sdpscAllocPage::formatPageHdr( sdpPhyPageHdr    * aNewPagePtr,
                                      scPageID           aNewPageID,
                                      sdpPageType        aPageType,
                                      sdpParentInfo    * aParentInfo,
                                      sdpscPageBitSet    aPageBitSet,
                                      sdrMtx           * aMtx )
{
    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aNewPagePtr != NULL );
    IDE_ASSERT( aNewPageID  != SD_NULL_PID );
    IDE_ASSERT( aParentInfo != NULL );

    IDE_TEST( sdpPhyPage::logAndInit( aNewPagePtr,
                                      aNewPageID,
                                      aParentInfo,
                                      (UShort)aPageBitSet,
                                      aPageType,
                                      SM_NULL_OID,   /* UndoSegment   */
                                      SM_NULL_INDEX_ID,
                                      aMtx ) != IDE_SUCCESS );
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description : [ INTERFACE ] Append ����� Page �Ҵ� ����
 *
 * aStatistics      - [IN] ��� ����
 * aSpaceID         - [IN] TableSpace ID
 * aSegHandle       - [IN] Segment Handle
 * aPrvAllocExtRID  - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aPrvAllocPageID  - [IN] ������ �Ҵ���� PageID
 * aMtx             - [IN] Mini Transaction Pointer
 * aAllocExtRID     - [OUT] ���ο� Page�� �Ҵ�� Extent RID
 * aAllocPID        - [OUT] ���Ӱ� �Ҵ���� PageID
 *
 ***********************************************************************/
IDE_RC sdpscAllocPage::allocNewPage4Append(
                            idvSQL              * aStatistics,
                            sdrMtx              * aMtx,
                            scSpaceID             aSpaceID,
                            sdpSegHandle        * aSegHandle,
                            sdRID                 aPrvAllocExtRID,
                            scPageID              aFstPIDOfPrvAllocExt,
                            scPageID              aPrvAllocPageID,
                            idBool                aIsLogging,
                            sdpPageType           aPageType,
                            sdRID               * aAllocExtRID,
                            scPageID            * aFstPIDOfAllocExt,
                            scPageID            * aAllocPID,
                            UChar              ** aAllocPagePtr )
{
    UChar          * sAllocPagePtr;
    sdpParentInfo    sParentInfo;

    IDE_ASSERT( aSegHandle    != NULL );
    IDE_ASSERT( aMtx          != NULL );
    IDE_ASSERT( aAllocExtRID  != NULL );
    IDE_ASSERT( aAllocPID     != NULL );
    IDE_ASSERT( aAllocPagePtr != NULL );
    IDE_ASSERT( aIsLogging    == ID_TRUE );

    IDE_TEST( sdpscExtDir::allocNewPageInExt( aStatistics,
                                              aMtx,
                                              aSpaceID,
                                              aSegHandle,
                                              aPrvAllocExtRID,
                                              aFstPIDOfPrvAllocExt,
                                              aPrvAllocPageID,
                                              aAllocExtRID,
                                              aFstPIDOfAllocExt,
                                              aAllocPID,
                                              &sParentInfo ) != IDE_SUCCESS );

    IDE_ERROR( *aAllocExtRID      != SD_NULL_RID );
    IDE_ERROR( *aFstPIDOfAllocExt != SD_NULL_PID );
    IDE_ERROR( *aAllocPID         != SD_NULL_PID );

    IDE_TEST( createPage( aStatistics,
                          aSpaceID,
                          *aAllocPID,
                          aPageType,
                          &sParentInfo,
                          (sdpscPageBitSet)
                          (SDPSC_BITSET_PAGETP_DATA|
                           SDPSC_BITSET_PAGEFN_FUL),
                          aMtx,
                          aMtx,
                          &sAllocPagePtr ) != IDE_SUCCESS );

    *aAllocPagePtr = sAllocPagePtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aAllocPagePtr = NULL;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : [ INTERFACE ] Append ����� Page Ȯ��
 *
 * (1) ���� Extent�� ���� �Ҵ���� ������ ���ķ� New Page�� �����Ѵٸ� SUCCESS!!
 * (2) ���� Extent�κ��� ���� �Ҵ���� �������� ������ Page���ٸ� ���� Extent��
 *     �����ϴ��� Ȯ���Ѵ�. �����Ѵٸ� SUCCESS �׷��� ������ (3)
 * (3) ���ο� Extent�� �Ҵ��Ѵ�. �Ҵ������� SUCCESS, �׷��� ������ SpaceNotEnough
 *     ���� �߻��Ѵ�.
 *
 * aStatistics          - [IN] ��� ����
 * aMtx                 - [IN] Mini Transaction Pointer
 * aSpaceID             - [IN] TableSpace ID
 * aSegHandle           - [IN] Segment Handle
 * aPrvAllocExtRID      - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aFstPIDOfPrvAllocExt - [IN] ������ Page�� �Ҵ�޾Ҵ� Extent RID
 * aPrvAllocPageID      - [IN] ������ �Ҵ���� PageID
 * aPageType            - [IN] Ȯ���� ������ Ÿ��
 *
 ***********************************************************************/
IDE_RC sdpscAllocPage::prepareNewPage4Append(
                            idvSQL              * aStatistics,
                            sdrMtx              * aMtx,
                            scSpaceID             aSpaceID,
                            sdpSegHandle        * aSegHandle,
                            sdRID                 aPrvAllocExtRID,
                            scPageID              aFstPIDOfPrvAllocExt,
                            scPageID              aPrvAllocPageID )
{
    sdRID            sAllocExtRID;
    scPageID         sFstPIDOfAllocExt;
    scPageID         sAllocPID;
    sdpParentInfo    sParentInfo;

    IDE_ASSERT( aSegHandle    != NULL );
    IDE_ASSERT( aMtx          != NULL );

    /* ���⼭�� �������� �Ҵ��ؼ� ��ȯ���� �ʰ�,
     * �Ҵ��� �� �ִ� ������ ID�� ��ȯ�һ��̴�. */
    IDE_TEST( sdpscExtDir::allocNewPageInExt( aStatistics,
                                              aMtx,
                                              aSpaceID,
                                              aSegHandle,
                                              aPrvAllocExtRID,
                                              aFstPIDOfPrvAllocExt,
                                              aPrvAllocPageID,
                                              &sAllocExtRID,
                                              &sFstPIDOfAllocExt,
                                              &sAllocPID,
                                              &sParentInfo ) != IDE_SUCCESS );

    IDE_ERROR( sAllocExtRID      != SD_NULL_RID );
    IDE_ERROR( sFstPIDOfAllocExt != SD_NULL_PID );
    IDE_ERROR( sAllocPID         != SD_NULL_PID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
