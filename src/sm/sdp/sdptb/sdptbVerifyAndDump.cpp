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
 * $Id: sdptbVerifyAndDump.cpp 27228 2008-07-23 17:36:52Z newdaily $
 *
 * Bitmap based TBS�� verify�ϰ� dump�ϱ� ����  ��ƾ���̴�.
 **********************************************************************/
#include <sdp.h>
#include <sdpPhyPage.h>
#include <smErrorCode.h>

#include <smErrorCode.h>

#include <sdptb.h>
#include <sdptbDef.h>
#include <sdpsfVerifyAndDump.h>
#include <sctTableSpaceMgr.h>

/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::dump( scSpaceID   aSpaceID,
                                 UInt        aDumpFlag ) //����� ������ ����
{
    UChar               * sPagePtr;
    sdrMtx                sMtx;
    idBool                sDummy;
    UInt                  sState=0;
    sdptbGGHdr          * sGGHdr;
    UInt                  sGGID;
    sddTableSpaceNode   * sSpaceNode=NULL;
    sddDataFileNode     * sFileNode;
    UInt                  sGGPID;
    SChar               * sTempBuf;

    aDumpFlag =aDumpFlag;
    
    IDE_TEST( sdrMiniTrans::begin( NULL, /* idvSQL* */
                                   &sMtx,
                                   &sDummy,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState=1;

    //XXX ���� ������ �о �ؾ���.
    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode)

              != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode != NULL );

    IDE_ASSERT( sSpaceNode->mHeader.mID == aSpaceID );

    /* BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
     * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
     *
     * Dump�� ������� ������ ���۸� Ȯ���մϴ�. Stack�� ������ ���,
     * �� �Լ��� ���� ������ ����� �� �����Ƿ� Heap�� �Ҵ��� �õ��� 
     * ��, �����ϸ� ���, �������� ������ �׳� return�մϴ�. */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_ID, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState =2;

    for ( sGGID=0; sGGID < sSpaceNode->mNewFileID ; sGGID++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[ sGGID ] ;

        if( sFileNode == NULL)
        {
            continue;
        }
    
        sGGPID = SDPTB_GLOBAL_GROUP_HEADER_PID( sGGID ); 

        IDE_TEST( sdbBufferMgr::getPageByPID( NULL, /* idvSQL* */
                                              aSpaceID,
                                              sGGPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              (void**)&sMtx,
                                              &sPagePtr,
                                              &sDummy,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

        if( dumpGGHdr( sPagePtr,
                       sTempBuf,
                       IDE_DUMP_DEST_LIMIT )
            == IDE_SUCCESS )
        {
            idlOS::printf( "%s\n", sTempBuf );
        }

        printAllocLGs( &sMtx,
                       aSpaceID,
                       sGGHdr );

        printDeallocLGs( &sMtx,
                         aSpaceID,
                         sGGHdr );

    }

    sState=1;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    sState=0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
    case 2:
        IDE_ASSERT( iduMemMgr::free( sTempBuf ) == IDE_SUCCESS );
    case 1:
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    default:
        break;
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 ***********************************************************************/
void sdptbVerifyAndDump::printStructureSizes(void)
{

    idlOS::printf( "----------- Header Size Begin ----------\n" );

    idlOS::printf( "ID_SIZEOF(sdptbSpaceCache) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdptbSpaceCache) );

    idlOS::printf( "ID_SIZEOF(sdptbGGHdr) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdptbGGHdr) );

    idlOS::printf( "ID_SIZEOF(sdptbLGFreeInfo) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdptbLGFreeInfo) );

    idlOS::printf( "ID_SIZEOF(sdptbLGHdr) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdptbLGHdr) );

    idlOS::printf( "ID_SIZEOF(sdpPhyPageHdr) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdpPhyPageHdr) );

    idlOS::printf( "ID_SIZEOF(sdpPageFooter) \t: %"ID_UINT32_FMT"\n",
                   (UInt)ID_SIZEOF(sdpPageFooter) );


    idlOS::printf( "----------- Header Size End ----------\n" );

}

/***********************************************************************
 * Description:
 * �� �Լ��� LG(Local Group)�� Bitmap�� String���·� Dump�ϴ� �Լ��Դϴ�.
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. 
 *
 ***********************************************************************/
void sdptbVerifyAndDump::printBitmapOfLG( sdptbLGHdr * aLGHdr ,
                                          SChar      * aOutBuf ,
                                          UInt         aOutSize )
{
    UInt    sNBytes = aLGHdr->mValidBits / SDPTB_BITS_PER_BYTE ; 
    UInt    sRest = aLGHdr->mValidBits % SDPTB_BITS_PER_BYTE; 
    UInt    i;
    UInt    sTemp;
    UChar   sVal;
    UChar * sBitmap = (UChar *)aLGHdr->mBitmap;
    
    while( sNBytes-- )
    {
        i=SDPTB_BITS_PER_BYTE;
        sTemp = 0x1;

        sVal = *sBitmap++;
        while(i--)
        {
            if( (sVal & sTemp) == sTemp )
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "1" );
            }
            else
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "0" );
            }

            sTemp <<= 1;
        }

    }

    sVal = *sBitmap;
    sTemp = 0x01;
    
    while( sRest-- )
    {
            if( (sVal & sTemp) == sTemp )
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "1" );
            }
            else
            {
                idlVA::appendFormat( aOutBuf,
                                     aOutSize,
                                     "0" );
            }

            sTemp <<= 1;
    }
}

/***********************************************************************
 * Description:
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. 
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::dumpLGHdr( UChar *aPage ,
                                      SChar *aOutBuf ,
                                      UInt   aOutSize )
{
    sdptbLGHdr          * sLGHdr ;
    UInt                  sCurrentOutStrSize;
    UChar               * sPage;

    IDE_ERROR( aPage   != NULL );
    IDE_ERROR( aOutBuf != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPage = sdpPhyPage::getPageStartPtr( aPage );

    IDE_DASSERT( sPage == sPage );

    sLGHdr = sdptbGroup::getLGHdr( (UChar*)sPage );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- Local Group Header Begin ----------\n" );

    /*
     * alloc LG ���� dealloc LG������ GG�� ���� �˼��ִ�.
     * ���⼭�� �׳� �տ��ִ°��� �ڿ��ִ°����� �����ֵ��� �Ѵ�?
     *              �ϴ��� �ּ�ó��..XXX
     */
    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "Local Group ID : %"ID_UINT32_FMT"\n"
                         "Start PID      : %"ID_UINT32_FMT"\n"
                         "Extent ID Hint : %"ID_UINT32_FMT"\n"
                         "Valid bits PID : %"ID_UINT32_FMT"\n"
                         "Free Extents   : %"ID_UINT32_FMT"\n"
                         "Bitmap         :\n",
                         sLGHdr->mLGID,
                         sLGHdr->mStartPID,
                         sLGHdr->mHint,
                         sLGHdr->mValidBits,
                         sLGHdr->mFree );

    sCurrentOutStrSize = idlOS::strlen( aOutBuf );

    printBitmapOfLG( sLGHdr, 
                     aOutBuf + sCurrentOutStrSize,
                     aOutSize - sCurrentOutStrSize );

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "\n----------- Local Group Header End ----------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *
 * BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
 * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
 * Local Array��� OutBuf�� �޾� �����ϵ��� �����մϴ�. 
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::dumpGGHdr( UChar *aPage ,
                                      SChar *aOutBuf ,
                                      UInt   aOutSize )
{
    sdptbGGHdr          * sGGHdr ;
    SInt                  i;
    UChar               * sPage;

    IDE_ERROR( aPage   != NULL );
    IDE_ERROR( aOutBuf != NULL );
    IDE_ERROR( aOutSize > 0 );

    sPage = sdpPhyPage::getPageStartPtr(aPage);

    IDE_DASSERT( sPage == sPage );

    sGGHdr = sdptbGroup::getGGHdr( (UChar*)sPage );

    idlOS::snprintf( aOutBuf,
                     aOutSize,
                     "----------- File Super Block Begin ----------\n" );

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "File Super Block ID         : %"ID_UINT32_FMT"\n" 
                         "Pages per extent            : %"ID_UINT32_FMT"\n"
                         "HWM page ID                 : %"ID_UINT32_FMT"\n"
                         "LG groups                   : %"ID_UINT32_FMT"\n"
                         "Pages                       : %"ID_UINT32_FMT"\n"
                         "LG type                     : %s\n"
                         "Free extents of alloc LGs   : %u\n"
                         "Bits of alloc LGs           : %x %x\n"
                         "Free extents of dealloc LGs : %u\n"
                         "Bits of dealloc LGs         : %x %x\n", 
                         sGGHdr->mGGID,
                         sGGHdr->mPagesPerExt,
                         sGGHdr->mHWM,
                         sGGHdr->mLGCnt,
                         sGGHdr->mTotalPages,
                         sGGHdr->mAllocLGIdx ? "Second LG" : "First LG",
                         sGGHdr->mLGFreeness[sdptbGroup::getAllocLGIdx(sGGHdr)].mFreeExts,
                         sGGHdr->mLGFreeness[sdptbGroup::getAllocLGIdx(sGGHdr)].mBits[0],
                         sGGHdr->mLGFreeness[sdptbGroup::getAllocLGIdx(sGGHdr)].mBits[1],
                         sGGHdr->mLGFreeness[sdptbGroup::getDeallocLGIdx(sGGHdr)].mFreeExts,
                         sGGHdr->mLGFreeness[sdptbGroup::getDeallocLGIdx(sGGHdr)].mBits[0],
                         sGGHdr->mLGFreeness[sdptbGroup::getDeallocLGIdx(sGGHdr)].mBits[1] );

    for( i = 0; i < SDP_MAX_TSSEG_PID_CNT; i++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "RIDs For TSS\t\t\t: %"ID_UINT32_FMT" \n", 
                             sGGHdr->mArrTSSegPID[i] );
    }

    for( i = 0; i < SDP_MAX_UDSEG_PID_CNT; i++ )
    {
        idlVA::appendFormat( aOutBuf,
                             aOutSize,
                             "RIDs For UDS\t\t\t: %"ID_UINT32_FMT" \n", 
                             sGGHdr->mArrUDSegPID[i] );
    }

    idlVA::appendFormat( aOutBuf,
                         aOutSize,
                         "----------- File Super Block End ------------\n" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::printLGsCore( sdrMtx     *aMtx,
                                         scSpaceID   aSpaceID,
                                         sdptbGGHdr *aGGHdr,
                                         UInt        aType )
{
    UChar     * sPagePtr;
    idBool      sDummy;
    UInt        sLGCnt;
    UInt        sLGID  = 0;
    UInt        sLGHdrPID;
    UInt        sWhich = 0;
    UInt        sState = 0;
    SChar     * sTempBuf;

    IDE_ERROR( aMtx   != NULL );
    IDE_ERROR( aGGHdr != NULL );

    /* BUG-28379 [SD] sdnbBTree::dumpNodeHdr( UChar *aPage ) ������
     * local Array�� ptr�� ��ȯ�ϰ� �ֽ��ϴ�. 
     *
     * Dump�� ������� ������ ���۸� Ȯ���մϴ�. Stack�� ������ ���,
     * �� �Լ��� ���� ������ ����� �� �����Ƿ� Heap�� �Ҵ��� �õ��� 
     * ��, �����ϸ� ���, �������� ������ �׳� return�մϴ�. */
    IDE_TEST( iduMemMgr::calloc( IDU_MEM_ID, 1,
                                 ID_SIZEOF( SChar ) * IDE_DUMP_DEST_LIMIT,
                                 (void**)&sTempBuf )
              != IDE_SUCCESS );
    sState = 1;

    sLGCnt = aGGHdr->mLGCnt;

    if( aType == SDPTB_ALLOC_LG ) 
    {
        sWhich =  sdptbGroup::getAllocLGIdx(aGGHdr);
    } 
    else if( aType == SDPTB_DEALLOC_LG ) 
    {
        sWhich =  sdptbGroup::getDeallocLGIdx(aGGHdr);
    } 

    while( sLGCnt-- )
    {
        sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aGGHdr->mGGID, 
                                                sLGID,
                                                sWhich,
                                                aGGHdr->mPagesPerExt );

        IDE_TEST( sdbBufferMgr::getPageByPID( NULL, /* idvSQL* */
                                              aSpaceID,
                                              sLGHdrPID,
                                              SDB_S_LATCH,
                                              SDB_WAIT_NORMAL,
                                              SDB_SINGLE_PAGE_READ,
                                              (void**)aMtx,
                                              &sPagePtr,
                                              &sDummy,
                                              NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        if( dumpLGHdr( sPagePtr,
                       sTempBuf,
                       IDE_DUMP_DEST_LIMIT )
            == IDE_SUCCESS )
        {
            idlOS::printf( "%s\n", sTempBuf );
        }

        sLGID++;
    }

    sState=1;
    IDE_TEST( iduMemMgr::free( sTempBuf ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch(sState)
    {
    case 1:
        IDE_ASSERT( iduMemMgr::free( sTempBuf ) == IDE_SUCCESS );
    default:
        break;
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 ***********************************************************************/
void sdptbVerifyAndDump::printAllocLGs( sdrMtx      *   aMtx,
                                        scSpaceID       aSpaceID,
                                        sdptbGGHdr  *   aGGHdr )
{
    printLGsCore( aMtx,
                  aSpaceID,
                  aGGHdr,
                  SDPTB_ALLOC_LG );
}

/***********************************************************************
 * Description:
 ***********************************************************************/
void sdptbVerifyAndDump::printDeallocLGs( sdrMtx      *  aMtx,
                                          scSpaceID      aSpaceID,
                                          sdptbGGHdr  *  aGGHdr )
{
    printLGsCore( aMtx,
                  aSpaceID,
                  aGGHdr,
                  SDPTB_DEALLOC_LG );
}



/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::verify( idvSQL*   aStatistics,
                                   scSpaceID aSpaceID,
                                   UInt      /* aFlag */ )
{
    sddDataFileNode *   sFileNode;
    UInt                i;
    sddTableSpaceNode * sSpaceNode=NULL;


    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode)

              != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode != NULL );

    IDE_ASSERT( sSpaceNode->mHeader.mID == aSpaceID );

    for (i=0; i < sSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = sSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL)
        {
            continue;
        }
    
        verifyGG( aStatistics,
                  aSpaceID,
                  sFileNode->mID );

    }

    ideLog::log( SM_TRC_LOG_LEVEL_DPAGE,
                 SM_TRC_DPAGE_VERIFY_TBS_SUCCESS,
                 aSpaceID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( SM_TRC_LOG_LEVEL_DPAGE,
                 SM_TRC_DPAGE_VERIFY_TBS_FAILURE,
                 aSpaceID );

    return IDE_FAILURE;
}
/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::verifyGG( idvSQL*   aStatistics,
                                     scSpaceID aSpaceID,
                                     UInt      aGGID )
{
    UInt                 sPageType;
    sdpPhyPageHdr    *   sPhyPageHdr;
    sdptbGGHdr       *   sGGHdr;
    UChar            *   sPagePtr;
    idBool               sDummy;
    UInt                 sLGID;
    idBool               sSuccess;
    UInt                 sFree=0;
    sdptbSpaceCache  *   sCache;
    UInt                 sLGCnt;
    scPageID             sGGPID;
    UInt                 sSumOfLGFree;

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ASSERT( sCache != NULL );

    sGGPID = SDPTB_GLOBAL_GROUP_HEADER_PID( aGGID ); 

    IDE_TEST( sdbBufferMgr::getPage( aStatistics, /* idvSQL* */
                                     aSpaceID,
                                     sGGPID,
                                     SDB_S_LATCH,
                                     SDB_WAIT_NORMAL,
                                     SDB_SINGLE_PAGE_READ,
                                     &sPagePtr,
                                     &sDummy )
              != IDE_SUCCESS );

    sPhyPageHdr = sdpPhyPage::getHdr(sPagePtr);

    IDE_TEST( sdpPhyPage::verify( sPhyPageHdr,
                                  aSpaceID,
                                  sGGPID )
              != IDE_SUCCESS );


    sPageType = sdpPhyPage::getPageType(sPhyPageHdr);

    if( sPageType != SDP_PAGE_FEBT_GGHDR)
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }

    sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

    /*
     * verify GG
     */

    if( sGGHdr->mGGID != aGGID )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }
    if( sGGHdr->mPagesPerExt < SDP_MIN_EXTENT_PAGE_CNT ) 
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }

    if( sGGHdr->mHWM > SD_CREATE_PID( aGGID, sGGHdr->mTotalPages) )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }

    if( sGGHdr->mLGCnt != 
        sdptbGroup::getNGroups( sGGHdr->mTotalPages, sCache, &sDummy ) )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }

    if( sGGHdr->mAllocLGIdx  >= SDPTB_ALLOC_LG_IDX_CNT )
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

        IDE_RAISE(err_verify_failed);
    }

    sSumOfLGFree=0;
    sLGCnt = sGGHdr->mLGCnt;
    sLGID = 0;

    /*
     * verify alloc LGs
     */
    while( sLGCnt-- )
    {

        IDE_TEST( verifyLG( aStatistics,
                            aSpaceID,
                            sGGHdr,
                            sLGID,
                            SDPTB_ALLOC_LG,
                            &sFree,
                            &sSuccess )
                  != IDE_SUCCESS);


        if( sSuccess == ID_FALSE )
        {
            smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DPAGE_TABLESPACE_ERROR,
                                    sPageType);

            IDE_RAISE(err_verify_failed);

        }

        sSumOfLGFree += sFree;

        sLGID++;

    }

    if( sGGHdr->mLGFreeness[ sdptbGroup::getAllocLGIdx(sGGHdr)].mFreeExts 
            != sSumOfLGFree)
    {
            smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DPAGE_TABLESPACE_ERROR,
                                    sPageType);

            IDE_RAISE(err_verify_failed);
    }

    /*
     * verify dealloc LGs
     */

    sLGCnt = sGGHdr->mLGCnt;
    sLGID = 0;
    sSumOfLGFree=0;

    while( sLGCnt-- )
    {

        verifyLG( aStatistics,
                  aSpaceID,
                  sGGHdr,
                  sLGID,
                  SDPTB_DEALLOC_LG,
                  &sFree,
                  &sSuccess );

        if( sSuccess == ID_FALSE )
        {
            smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DPAGE_TABLESPACE_ERROR,
                                    sPageType);

            IDE_RAISE(err_verify_failed);

        }

        sSumOfLGFree += sFree;
        sLGID++;
    }

    if( sGGHdr->mLGFreeness[sdptbGroup::getDeallocLGIdx(sGGHdr)].mFreeExts 
                    != sSumOfLGFree)
    {
            smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DPAGE_TABLESPACE_ERROR,
                                    sPageType);

            IDE_RAISE(err_verify_failed);
    }

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sPagePtr )
                 != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_verify_failed)
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_InvalidDB));
    }
    IDE_EXCEPTION_END;

    ideLog::log( SM_TRC_LOG_LEVEL_DPAGE,
                 SM_TRC_DPAGE_VERIFY_TBS_FAILURE,
                 aSpaceID );

    return IDE_FAILURE;
}
/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbVerifyAndDump::verifyLG( idvSQL     * aStatistics,
                                     scSpaceID    aSpaceID,
                                     sdptbGGHdr * aGGHdr,
                                     UInt         aLGID,
                                     sdptbLGType  aType,
                                     UInt       * aFreeCnt,
                                     idBool     * aSuccess )
{
    scPageID          sLGHdrPID;
    UInt              sWhich = 0;
    UChar           * sPagePtr;
    UInt              sFreeCnt;
    sdpPhyPageHdr   * sPhyPageHdr;
    idBool            sDummy;
    UInt              sPageType;
    sdptbLGHdr      * sLGHdr;

    *aSuccess = ID_TRUE;

    if( aType == SDPTB_ALLOC_LG ) 
    {
        sWhich =  sdptbGroup::getAllocLGIdx(aGGHdr);
    } 
    else if( aType == SDPTB_DEALLOC_LG ) 
    {
        sWhich =  sdptbGroup::getDeallocLGIdx(aGGHdr);
    } 

    sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aGGHdr->mGGID, 
                                            aLGID,
                                            sWhich,
                                            aGGHdr->mPagesPerExt );

    IDE_TEST( sdbBufferMgr::getPage( aStatistics, /* idvSQL* */
                                     aSpaceID,
                                     sLGHdrPID,
                                     SDB_S_LATCH,
                                     SDB_WAIT_NORMAL,
                                     SDB_SINGLE_PAGE_READ,
                                     &sPagePtr,
                                     &sDummy )
              != IDE_SUCCESS );

    sPhyPageHdr = sdpPhyPage::getHdr(sPagePtr);

    IDE_TEST( sdpPhyPage::verify( sPhyPageHdr,
                                  aSpaceID,
                                  sLGHdrPID )
              != IDE_SUCCESS );


    sPageType = sdpPhyPage::getPageType(sPhyPageHdr);

    if( sPageType != SDP_PAGE_FEBT_LGHDR)
    {
        smuUtility::mWriteError(SM_TRC_LOG_LEVEL_FATAL,
                                SM_TRC_DPAGE_TABLESPACE_ERROR,
                                sPageType);

    }

    sLGHdr = sdptbGroup::getLGHdr(sPagePtr);

    if( sLGHdr->mLGID != aLGID )
    {
        IDE_CONT( error );
    }

    if( SD_MAKE_FPID( sLGHdr->mStartPID ) != 
            SDPTB_EXTENT_START_FPID_FROM_LGID( aLGID, aGGHdr->mPagesPerExt ))
    {
        IDE_CONT( error );
    }

    if( sLGHdr->mHint > sdptbGroup::nBitsPerLG() )
    {
        IDE_CONT( error );
    }

    if( sLGHdr->mValidBits > sdptbGroup::nBitsPerLG() )
    {
        IDE_CONT( error );
    }

    sdptbBit::sumOfZeroBit( sLGHdr->mBitmap, sLGHdr->mValidBits,  &sFreeCnt ); 

    if( sFreeCnt != sLGHdr->mFree )
    {
        IDE_CONT( error );
    }

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sPagePtr )
                 != IDE_SUCCESS );


    *aFreeCnt = sLGHdr->mFree;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_CONT( error );

    *aSuccess = ID_FALSE;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;


    return IDE_FAILURE;

}
