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
 * $Id: sdpstFreePage.cpp 27229 2008-07-23 17:37:19Z newdaily $
 *
 * �� ������ Treelist Managed Segment���� ������� ���� ���� ���� STATIC
 * �������̽��� �����Ѵ�.
 *
 ***********************************************************************/

# include <sdpstDef.h>

# include <sdpstRtBMP.h>
# include <sdpstLfBMP.h>
# include <sdpstCache.h>
# include <sdpstAllocPage.h>
# include <sdpstFreePage.h>
# include <sdpstSH.h>

# include <sdpstStackMgr.h>


/***********************************************************************
 * Description : [ INTERFACE ] Segment�� �������� �����Ѵ�
 ***********************************************************************/
IDE_RC sdpstFreePage::freePage( idvSQL       * aStatistics,
                                sdrMtx       * aMtx,
                                scSpaceID      aSpaceID,
                                sdpSegHandle * aSegHandle,
                                UChar        * aPagePtr )
{
    UChar            * sPagePtr;
    sdpstSegHdr      * sSegHdr;
    idBool             sIsTurnON;
    sdpstStack         sItHintStack;
    sdpstStack         sRevStack;
    sdpstSegCache    * sSegCache;
    scPageID           sPageID;
    sdpPageType        sPageType;
    sdpParentInfo      sParentInfo;
    sdpstPBS           sPBS;
    ULong              sSeqNo;

    IDE_ASSERT( aSegHandle != NULL );
    IDE_ASSERT( aPagePtr   != NULL );
    IDE_ASSERT( aMtx       != NULL );

    sSegCache   = (sdpstSegCache*)aSegHandle->mCache;

    sPageID     = sdpPhyPage::getPageID( aPagePtr );
    sSeqNo      = sdpPhyPage::getSeqNo( sdpPhyPage::getHdr(aPagePtr) );
    sPageType   = sdpPhyPage::getPageType( sdpPhyPage::getHdr(aPagePtr) );
    sParentInfo = sdpPhyPage::getParentInfo( aPagePtr );
    sPBS        = (sdpstPBS)
                  ( SDPST_BITSET_PAGETP_DATA | SDPST_BITSET_PAGEFN_FMT );

    /*
     * Page�� Free �ϰ� �Ǹ� ������ �� format �������� �ȴ�.
     * ���� ���⼭ �ʱ�ȭ�� ���ش�.
     */
    IDE_TEST( sdpstAllocPage::formatPageHdr( aMtx,
                                             aSegHandle,
                                             sdpPhyPage::getHdr( aPagePtr ),
                                             sPageID,
                                             sSeqNo,
                                             sPageType,
                                             sParentInfo.mParentPID,
                                             sParentInfo.mIdxInParent,
                                             sPBS ) != IDE_SUCCESS );

    /* reverse stack�� it->bottom������ stack�� �����Ѵ�. */
    sdpstStackMgr::initialize( &sRevStack );
    IDE_TEST( sdpstAllocPage::tryToChangeMFNLAndItHint(
                                 aStatistics,
                                 aMtx,
                                 aSpaceID,
                                 aSegHandle,
                                 SDPST_CHANGEMFNL_LFBMP_PHASE,
                                 sParentInfo.mParentPID,
                                 sParentInfo.mIdxInParent,
                                 (void*)&sPBS,
                                 SD_NULL_PID,
                                 SDPST_INVALID_SLOTNO,
                                 &sRevStack ) != IDE_SUCCESS );

    // Format Index Page Count�� ������Ų��.
    if ( sSegCache->mSegType == SDP_SEG_TYPE_INDEX )
    {
        // prepareNewPages ������ ���ؼ� index segment �� ���ؼ��� Ư����
        // format ������ ������ �����Ѵ�.
        IDE_ASSERT( sdpstLfBMP::isEqFN( sPBS, 
                                        SDPST_BITSET_PAGEFN_FMT )
                    == ID_TRUE );

        // internal slot�� MFNL�� ����Ǿ Internal Hint�� �����ؾ� �ϰų�,
        // Index Segment�� ��쿡�� Segment Header�� fix�Ѵ�.
        IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                              aSpaceID,
                                              aSegHandle->mSegPID,            
                                              SDB_X_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              aMtx,
                                              &sPagePtr,
                                              NULL, /* aTrySuccess */
                                              NULL  /* aIsCorruptPage */ )
                  != IDE_SUCCESS );

        sSegHdr = sdpstSH::getHdrPtr( sPagePtr );

        IDE_TEST( sdpstSH::setFreeIndexPageCnt( aMtx,
                                                sSegHdr,
                                                sSegHdr->mFreeIndexPageCnt + 1 )
                  != IDE_SUCCESS );
    }

    if ( (sdpstCache::needToUpdateItHint(sSegCache, SDPST_SEARCH_NEWPAGE) 
          == ID_FALSE) &&
         (sdpstStackMgr::getDepth( &sRevStack ) 
          == SDPST_RTBMP) )
    {
        /* ���� ���� �����ϴ� ������ free page�� �߻��ϸ�, 
         * slot hint�� page hint��
         * ��� ������ ��ġ�� �����̴� */
        sItHintStack = sdpstCache::getMinimumItHint( aStatistics, sSegCache );

        IDE_TEST( checkAndUpdateHintItBMP( aStatistics,
                                           aSpaceID,
                                           aSegHandle->mSegPID,
                                           &sRevStack,
                                           &sItHintStack,
                                           &sIsTurnON ) != IDE_SUCCESS );

        if ( sIsTurnON == ID_TRUE )
        {
            sdpstCache::setUpdateHint4Page( sSegCache, ID_TRUE );
            sdpstCache::setUpdateHint4Slot( sSegCache, ID_TRUE );
        }
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ش� �������� Free ������ ���θ� ��ȯ�Ѵ�.
 * 
 * BUG-32942 When executing rebuild Index stat, abnormally shutdown
 ***********************************************************************/
idBool sdpstFreePage::isFreePage( UChar * aPagePtr )
{
    idBool      sRet = ID_FALSE;
    sdpstPBS    sPBS;

    sPBS = (sdpstPBS)sdpPhyPage::getState((sdpPhyPageHdr*)aPagePtr);

    if ( sPBS == ((sdpstPBS)(SDPST_BITSET_PAGETP_DATA | SDPST_BITSET_PAGEFN_FMT)) )
    {
        sRet = ID_TRUE;
    }

    return sRet;
}

/***********************************************************************
 * Description : �� ������ Ž���� ���� Hint�� �缳���� �ʿ������� �Ǵ��ϰ�
 *               Update Hint Flag�� �缳���Ѵ�.
 ***********************************************************************/
IDE_RC sdpstFreePage::checkAndUpdateHintItBMP( idvSQL         * aStatistics,
                                               scSpaceID        aSpaceID,
                                               scPageID         aSegPID,
                                               sdpstStack     * aRevStack,
                                               sdpstStack     * aItHintStack,
                                               idBool         * aIsTurnOn )
{
    idBool             sIsTurnOn;
    sdpstPosItem       sRtPos1;
    sdpstPosItem       sRtPos2;
    SShort             sDist;
    sdpstStack         sStack;

    IDE_ASSERT( aSegPID != SD_NULL_PID );
    IDE_ASSERT( aRevStack != NULL );
    IDE_ASSERT( aItHintStack != NULL );
    IDE_ASSERT( aIsTurnOn    != NULL );

    sIsTurnOn = ID_FALSE;

    /* root bmp���� MFNL�� ����� ��쿡�� Stack�� �����Ͽ� Segment Cache��
     * it hint stack�� ���Ͽ� Update Hint Flag�� on ��Ų��.
     * stack�� Ư���ϰ� it->root �������� push�� �Ǿ� �־� reverse stack
     * �̹Ƿ� ������ ���� �Ųٷ� üũ�Ѵ�. */
    if ( sdpstStackMgr::getDepth( aRevStack ) == SDPST_RTBMP )
    {
        /* ���� root bmp�� �ƴѰ��� ������ stack�� ���İ��踦 
         * ��Ȯ�� �Ǵ��غ���
         * ���� It Hint�� �� ũ�ٸ�, It Hint�� ���ܾ��ϰ�, 
         * �׷��� �ʴٸ�, �׳� �д�. */
        sRtPos1 = sdpstStackMgr::getSeekPos( aRevStack, SDPST_RTBMP);
        sRtPos2 = sdpstStackMgr::getSeekPos( aItHintStack, SDPST_RTBMP);

        sDist = sdpstStackMgr::getDist( &sRtPos1, &sRtPos2 );

        if ( sDist == SDPST_FAR_AWAY_OFF )
        {
            sdpstStackMgr::initialize( &sStack );
            // ��Ȯ�� ���İ��踦 �Ǵ��� �� ���⶧���� RevStack�� �����ϰ�,
            // ���Ѵ�.
            IDE_TEST( makeOrderedStackFromRevStack( aStatistics,
                                          aSpaceID,
                                          aSegPID,
                                          aRevStack,
                                          &sStack ) != IDE_SUCCESS );

            if ( sdpstStackMgr::compareStackPos( aItHintStack, &sStack ) > 0 )
            {
                sIsTurnOn = ID_TRUE;
            }
            else
            {
                sIsTurnOn = ID_FALSE;
            }
        }
        else
        {
            // ������ root���� ���İ��踦 �ľ��Ѵ�.
            if ( sDist < 0 )
            {
                sIsTurnOn = ID_TRUE;
            }
            else
            {
                sIsTurnOn = ID_FALSE;
            }
        }
    }
    else
    {
        // root���� mfnl�� ���ŵ��� �ʾ����Ƿ�, rescan ���ο� ����� ����.
    }

    *aIsTurnOn = sIsTurnOn;

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �� stack���� ������ �´� stack�� ������.
 ***********************************************************************/
IDE_RC sdpstFreePage::makeOrderedStackFromRevStack( idvSQL      * aStatistics,
                                          scSpaceID     aSpaceID,
                                          scPageID      aSegPID,
                                          sdpstStack  * aRevStack,
                                          sdpstStack  * aOrderedStack )
{
    idBool           sDummy;
    UInt             sLoop;
    sdpstPosItem     sCurrPos;
    scPageID         sCurrRtBMP;
    UChar          * sPagePtr;
    SShort           sRtBMPIdx;
    sdpstBMPHdr    * sRtBMPHdr;
    sdpstStack       sRevStack;
    UInt             sState = 0;

    IDE_DASSERT( aSegPID != SD_NULL_PID );
    IDE_DASSERT( aRevStack != NULL );
    IDE_DASSERT( aOrderedStack != NULL );

    /* reverse stack���� (it,lfslotno)->(rt,itslotno) �� �� �ִ�. */
    if ( sdpstStackMgr::getDepth( aRevStack ) != SDPST_RTBMP )
    {
        sdpstStackMgr::dump( aRevStack );
        IDE_ASSERT( 0 );
    }

    sRevStack = *aRevStack;
    sCurrPos = sdpstStackMgr::getCurrPos( &sRevStack );

    sRtBMPIdx  = SDPST_INVALID_SLOTNO;
    sCurrRtBMP = aSegPID;
    IDE_ASSERT( sCurrRtBMP != SD_NULL_PID );

    while( sCurrRtBMP != SD_NULL_PID )
    {
        sRtBMPIdx++;
        if ( sCurrPos.mNodePID == sCurrRtBMP )
        {
            break; // found it
        }

        sState = 0;
        IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                              aSpaceID,
                                              sCurrRtBMP,
                                              &sPagePtr,
                                              &sDummy ) != IDE_SUCCESS );
        sState = 1;

        sRtBMPHdr  = sdpstSH::getRtBMPHdr(sPagePtr);
        sCurrRtBMP = sdpstRtBMP::getNxtRtBMP(sRtBMPHdr);

        sState = 0;
        IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                           sPagePtr ) != IDE_SUCCESS );
    }

    sdpstStackMgr::push( aOrderedStack,
                         SD_NULL_PID, // root bmp ���� ����� PID
                         sRtBMPIdx );

    for( sLoop = 0; sLoop < (UInt)SDPST_ITBMP; sLoop++ )
    {
        sCurrPos = sdpstStackMgr::pop( &sRevStack );
        sdpstStackMgr::push( aOrderedStack, &sCurrPos );
    }

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    if ( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sPagePtr ) == IDE_SUCCESS );

    }
    return IDE_FAILURE;
}

