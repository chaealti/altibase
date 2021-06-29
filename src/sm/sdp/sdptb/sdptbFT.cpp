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
 * $Id: sdptbFT.cpp 27228 2008-07-23 17:36:52Z newdaily $
 *
 * TBS���� Dump Table �Լ����� ���ִ�.
 **********************************************************************/
#include <sdp.h>
#include <sdptb.h>
#include <sctTableSpaceMgr.h>
#include <smErrorCode.h>
#include <sdpst.h>
#include <sdpReq.h>


/******************************************************************************
 * Description : Free Ext List�� �����ִ� D$DISK_TBS_FREEEXTLIST�� Record��
 *               Build�Ѵ�.
 *
 *  aHeader   - [IN]
 *  aDumpObj  - [IN]
 *  aMemory   - [IN]
 ******************************************************************************/
IDE_RC sdptbFT::buildRecord4FreeExtOfTBS(
    void                * aHeader,
    void                * aDumpObj,
    iduFixedTableMemory * aMemory )
{
    sdptbSpaceCache   * sSpaceCache;
    sddTableSpaceNode * sTBSNode;
    UInt                i;
    UInt                sLstGGID;
    UInt                sFileID;
    scSpaceID           sSpaceID;
    UInt                sIdx;

    IDE_TEST_RAISE( aDumpObj == NULL, ERR_EMPTY_OBJECT );

    /* TBS�� �����ϴ��� �˻��ϰ� Dump�߿� Drop���� �ʵ��� Lock�� ��´�. */
    /* BUG-28678  [SM] qmsDumpObjList::mObjInfo�� ������ �޸� �ּҴ� 
     * �ݵ�� ������ �Ҵ��ؼ� �����ؾ��մϴ�. 
     * 
     * aDumpObj�� Pointer�� �����Ͱ� ���� ������ ���� �����;� �մϴ�. */
    sSpaceID = *( (scSpaceID*)aDumpObj );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sSpaceID,
                                                        (void**)&sTBSNode)
              != IDE_SUCCESS );

    sSpaceCache = sddDiskMgr::getSpaceCache( sTBSNode );

    IDE_ASSERT( sTBSNode != NULL );
    IDE_ASSERT( sSpaceCache != NULL );

    sLstGGID = sSpaceCache->mMaxGGID;

    for( i = 0, sFileID = 0; i <= sLstGGID; i++ )
    {
        sdptbBit::findBitFromHint( (void *)sSpaceCache->mFreenessOfGGs,
                                   sLstGGID + 1,   //�˻�����Ʈ��
                                   sFileID,
                                   &sIdx );

        if( sIdx == SDPTB_BIT_NOT_FOUND )
        {
            break;
        }

        /* �� File�� GG�� ���ؼ� FreeExt ������ �����ϵ��� �Ѵ�. */
        sFileID = sIdx;

        IDE_TEST( buildRecord4FreeExtOfGG( aHeader,
                                           aMemory,
                                           sSpaceCache,
                                           sFileID )
                  != IDE_SUCCESS );

        /*
         * BUG-28059 [SD] drop �� ������ �ִ°�� D$disk_tbs_free_extlist 
         *           ��ȸ�� �״� ��찡 ����. 
         */
        if( sIdx == sLstGGID )
        {
            break;
        }

        sFileID++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_EMPTY_OBJECT);
    {
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/******************************************************************************
 * Description : TBS���� D$DISK_TBS_FREE_EXTLIST�� ���ڵ��� aFID�� �ش��ϴ� ��
 *               ���� Record�� Build�Ѵ�.
 *
 *  aHeader   - [IN]
 *  aMemory   - [IN]
 *  aCache    - [IN] TableSpace Cache
 *  sdFileID  - [IN] Dump�Ϸ��� File�� ID
 ******************************************************************************/
IDE_RC sdptbFT::buildRecord4FreeExtOfGG( void                * aHeader,
                                         iduFixedTableMemory * aMemory,
                                         sdptbSpaceCache     * aCache,
                                         sdFileID              aFID )
{
    UInt               i;
    UInt               sSpaceID;
    scPageID           sGGPID;
    sdptbGGHdr       * sGGHdrPtr;
    UChar            * sPagePtr;
    sdptbLGFreeInfo  * sLFFreeInfo;
    UInt               sFreeLGID;
    scPageID           sLGHdrPID;
    UInt               sTotLGCnt;
    UInt               sAllocIdx;
    UInt               sDeAllocIdx;
    UInt               sIdx;
    UInt               sState = 0;

    IDE_ASSERT( aCache != NULL );

    sSpaceID = aCache->mCommon.mSpaceID;
    sGGPID   = SDPTB_GET_GGHDR_PID_BY_FID( aFID );

    IDE_TEST( sdbBufferMgr::getPageByPID( NULL, /* Statistics */
                                          sSpaceID,
                                          sGGPID,
                                          SDB_S_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          NULL, /* Mini Trans */
                                          (UChar**)&sPagePtr,
                                          NULL, /*TrySuccess*/
                                          NULL /*IsCorruptPage*/ )
              != IDE_SUCCESS );
    sState = 1;

    sGGHdrPtr   = sdptbGroup::getGGHdr( sPagePtr );
    sAllocIdx   = sdptbGroup::getAllocLGIdx( sGGHdrPtr );
    sDeAllocIdx = sdptbGroup::getDeallocLGIdx( sGGHdrPtr );
    sLFFreeInfo = sGGHdrPtr->mLGFreeness + sAllocIdx;
    sTotLGCnt   = sGGHdrPtr->mLGCnt;

    for( i = 0, sFreeLGID = 0; i < sTotLGCnt; i++ )
    {
        sdptbBit::findBitFromHint( &sLFFreeInfo->mBits,
                                   sTotLGCnt, //�˻�����Ʈ��
                                   sFreeLGID,
                                   &sIdx );
        if( sIdx == SDPTB_BIT_NOT_FOUND )
        {
            break;
        }

        /* �� File�� GG�� ���ؼ� FreeExt ������ �����ϵ��� �Ѵ�. */
        sFreeLGID = sIdx;

        sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aFID,
                                                sFreeLGID,
                                                sDeAllocIdx,
                                                sGGHdrPtr->mPagesPerExt );

        IDE_TEST( buildRecord4FreeExtOfLG( aHeader,
                                           aMemory,
                                           aCache,
                                           sGGHdrPtr,
                                           sLGHdrPID )
                  != IDE_SUCCESS );

        sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aFID,
                                                sFreeLGID,
                                                sAllocIdx,
                                                sGGHdrPtr->mPagesPerExt );

        IDE_TEST( buildRecord4FreeExtOfLG( aHeader,
                                           aMemory,
                                           aCache,
                                           sGGHdrPtr,
                                           sLGHdrPID )
                  != IDE_SUCCESS );

        sFreeLGID++;
    }

    sState  = 0;
    IDE_TEST( sdbBufferMgr::releasePage( NULL, /* Statistics */
                                         sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( NULL, /* Statistics */
                                               sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : TBS���� D$DISK_TBS_FREEEXTLIST�� ���ڵ��� aGGPtr�� aLGHdrPID
 *               �� ����Ű�� LG�� Record���� Build�Ѵ�.
 *
 *  aHeader   - [IN]
 *  aMemory   - [IN]
 *  aCache    - [IN] TableSpace Cache
 *  aGGPtr    - [IN] GGHeader Pointer
 *  aLGHdrPID - [IN] LGHeader PID
 ******************************************************************************/
IDE_RC sdptbFT::buildRecord4FreeExtOfLG( void                * aHeader,
                                         iduFixedTableMemory * aMemory,
                                         sdptbSpaceCache     * aSpaceCache,
                                         sdptbGGHdr          * aGGPtr,
                                         scPageID              aLGHdrPID )
{
    UInt            i;
    sdptbLGHdr    * sLGHdrPtr;
    UInt            sBitIdx;
    UChar         * sPagePtr;
    UInt            sFreeExtIdx;
    sdpDumpTBSInfo  sDumpFreeExtInfo;
    ULong           sBitmap[ SD_PAGE_SIZE / ID_SIZEOF( ULong ) ];
    UInt            sState = 0;

    IDE_TEST(sdbBufferMgr::getPageByPID( NULL, /* Statistics */
                                         aSpaceCache->mCommon.mSpaceID,
                                         aLGHdrPID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         NULL, /* Mini Mtx */
                                         (UChar**)&sPagePtr,
                                         NULL, /*TrySuccess*/
                                         NULL /*IsCorruptPage*/ )
             != IDE_SUCCESS );
    sState = 1;

    sLGHdrPtr = sdptbGroup::getLGHdr( sPagePtr);

    idlOS::memcpy( sBitmap,
                   sLGHdrPtr->mBitmap,
                   sdptbGroup::getLenBitmapOfLG() );

    for( i = 0, sFreeExtIdx = 0; i < sLGHdrPtr->mFree; i++ )
    {
        //LG���� ���� ������� mValidBits���� ��ŭ�� �˻��ؾ��Ѵ�.
        sdptbBit::findZeroBitFromHint( sBitmap,
                                       sLGHdrPtr->mValidBits,
                                       sFreeExtIdx,
                                       &sBitIdx );

        sdptbBit::setBit( sBitmap, sBitIdx );

        sFreeExtIdx = sBitIdx;

        //�ش� LG�� free�� �ִ°��� ���� �������Ƿ� �̰� �����̵ȴ�
        //�� �ɰ��� ������Ȳ�̴�.
        IDE_ASSERT( sBitIdx <  sLGHdrPtr->mValidBits );

        sDumpFreeExtInfo.mExtRID       = SD_MAKE_RID( aLGHdrPID, sFreeExtIdx );
        sDumpFreeExtInfo.mPID          = aLGHdrPID;
        sDumpFreeExtInfo.mOffset       = sFreeExtIdx;
        sDumpFreeExtInfo.mFstPID       = sLGHdrPtr->mStartPID + aSpaceCache->mCommon.mPagesPerExt*sBitIdx;
        sDumpFreeExtInfo.mPageCntInExt = aGGPtr->mPagesPerExt;

        IDE_TEST( iduFixedTable::buildRecord( aHeader,
                                              aMemory,
                                              (void *)&sDumpFreeExtInfo )
                  != IDE_SUCCESS );

        sFreeExtIdx++;
    }

    sState  = 0;
    IDE_TEST( sdbBufferMgr::releasePage( NULL, /* Statistics */
                                         sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( NULL, /* Statistics */
                                               sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

