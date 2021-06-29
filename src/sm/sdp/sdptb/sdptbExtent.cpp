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
 * $Id: sdptbExtent.cpp 27228 2008-07-23 17:36:52Z newdaily $
 *
 * TBS���� extent�� �Ҵ��ϰ� �����ϴ� ��ƾ�� ���õ� �Լ����̴�.
 **********************************************************************/
#include <sdp.h>
#include <sdptb.h>
#include <sctTableSpaceMgr.h>
#include <smErrorCode.h>
#include <sdpst.h>
#include <sdpReq.h>

/***********************************************************************
 * Description:
 *   [INTERFACE] tablespace�κ��� extent�� �Ҵ�޴´�.
 *
 *
 * aStatistics - [IN] �������
 * aStartInfo  - [IN] Mini Transaction Start Info
 * aSpaceID    - [IN] TableSpace ID
 * aOrgNrExts  - [IN] �Ҵ��� ��û�� Extent ����
 *
 * aExtSlot    - [OUT] �Ҵ�� Extent Desc Array Ptr
 ***********************************************************************/
IDE_RC sdptbExtent::allocExts( idvSQL          * aStatistics,
                               sdrMtxStartInfo * aStartInfo,
                               scSpaceID         aSpaceID,
                               UInt              aOrgNrExts,
                               sdpExtDesc      * aExtSlot )
{
    UInt                sNrExts; //�Ҵ��ؾ��ϴ� ext�� ������ �����Ѵ�.
    sdptbSpaceCache   * sSpaceCache;
    sdFileID            sFID = SD_MAKE_FID(SD_NULL_PID);
    UInt                sNrDone;
    sddTableSpaceNode * sTBSNode;
    UInt                sNeededPageCnt; //����Ȯ��� ��û�� ������ ����
    UInt                i;
    scPageID            sExtFstPID[4]; // �ִ� 4������ �ѹ��� �Ҵ������ �ִ�.
    scPageID          * sCurExtFstPIDPtr;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aOrgNrExts == 1 ); // segment���� ȣ��ɶ� 1�� �Ѱ��ش�.

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sTBSNode )
              != IDE_SUCCESS );

    IDE_ERROR_MSG( sTBSNode != NULL,
                   "Tablespace node not found (ID : %"ID_UINT32_FMT")",
                   aSpaceID );

    IDE_DASSERT_MSG( sctTableSpaceMgr::isDiskTableSpace( sTBSNode ) == ID_TRUE,
                     "Fatal error during alloc extent (Tablespace ID : %"ID_UINT32_FMT") ",
                      aSpaceID );
                    
    sSpaceCache = sddDiskMgr::getSpaceCache( sTBSNode );
    IDE_ERROR_MSG( sSpaceCache != NULL,
                   "Tablespace cache not found (ID : %"ID_UINT32_FMT")",
                    aSpaceID );

    sNrExts          = aOrgNrExts;
    sCurExtFstPIDPtr = sExtFstPID;

    while( sNrExts > 0 )
    {
        IDE_TEST( getAvailFID( sSpaceCache, &sFID ) != IDE_SUCCESS );

        if( sFID != SDPTB_NOT_FOUND )
        {
            IDE_TEST( tryAllocExtsInGG( aStatistics,
                                        aStartInfo,
                                        sSpaceCache,
                                        sFID,
                                        sNrExts,
                                        sCurExtFstPIDPtr,
                                        &sNrDone)
                       != IDE_SUCCESS );

            sCurExtFstPIDPtr += sNrDone;
            sNrExts          -= sNrDone;
        }
        else
        {
            sNeededPageCnt = sNrExts * sSpaceCache->mCommon.mPagesPerExt;

            IDE_TEST( autoExtDatafileOnDemand( aStatistics,
                                               aSpaceID,
                                               sSpaceCache,
                                               aStartInfo->mTrans,
                                               sNeededPageCnt )
                       != IDE_SUCCESS  );

            //����Ȯ�忡 �����ߴٸ�,  �ٽ� ������ ���鼭 �Ҵ��� �Ѵ�.

            /* [����]
             * ������� �Դٴ°��� ����Ȯ�忡 �����ߴٴ� ���̴�.
             * ���� Ȯ�尡���� ������ ���� ���������� Ȯ�忡 �����ߴٸ�
             * autoExtDatafileonDemand�ȿ��� ��� ó���ȴ�.
             */

            continue;
        }

        // ����!
        // cache�� ������ ���� ���Ͽ� ���°��̹Ƿ� sNrDone�� 1�ϼ��� �ִ�.
        // cache�� dirty read �ϹǷ�.....
        // IDE_ASSERT( sNrDone == 1 );

    }//while

    //������ �߻����� �ʾҴٸ�  sNrExts�� 0�� �ɰ��̴�.(0�� �����Ȳ)
    IDE_ERROR_MSG( sNrExts == 0,
                   "Error occurred while new extents alloc"
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "ExtCnt : %"ID_UINT32_FMT")",
                   aSpaceID,
                   sNrExts );

    //�Ҵ���� extent�� ù��° PID�� ���ڷι��� aExtSlot�� �����Ѵ�.
    for( i=0 ; i < aOrgNrExts ; i++ )
    {
        aExtSlot[i].mExtFstPID = sExtFstPID[i];
        aExtSlot[i].mLength    = sSpaceCache->mCommon.mPagesPerExt;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 *   GG �ȿ��� extent�� �Ҵ�޴´�.
 *
 * aStatistics - [IN] �������
 * aStartInfo  - [IN] Mini Transaction Start Info
 * aNrExts     - [IN] �Ҵ��� ��û�� Extent ����
 * aExtFstPID  - [OUT] �Ҵ��� extent�� ù��° pid array
 * aNrDone     - [OUT] �Ҵ�� Extent ��
 ***********************************************************************/
IDE_RC sdptbExtent::tryAllocExtsInGG( idvSQL             * aStatistics,
                                      sdrMtxStartInfo    * aStartInfo,
                                      sdptbSpaceCache    * aCache,
                                      sdFileID             aFID,
                                      UInt                 aNrExts,
                                      scPageID           * aExtFstPID,
                                      UInt               * aNrDone )
{
    UInt        i;
    UInt        sSpaceID;
    scPageID    sGGPID;
    scPageID    sLGHdrPID;
    idBool      sDummy;
    UInt        sLGID;  //�Ҵ��� �����  LGID
    sdptbGGHdr* sGGHdrPtr;
    UInt        sAllocLGIdx;  //������� LG index
    UChar     * sPagePtr;
    idBool      sSwitching;//�ӽú���
    UInt        sFreeInLG;
    sdrMtx      sMtx;
    smLSN       sOpNTA;
    ULong       sData[5]; //extent���� ,4���� PID���� ������ ������ �����.
    UInt        sBitIdx;
    UInt        sState      = 0;
    UInt        sNrDoneInLG = 0;

    IDE_ASSERT( aStartInfo  != NULL );
    IDE_ASSERT( aCache      != NULL );
    IDE_ASSERT( aExtFstPID  != NULL );
    IDE_ASSERT( aNrDone     != NULL );
    IDE_ASSERT( aNrExts   == 1 ); // segment���� ȣ��ɶ� 1�� �Ѱ��ش�.

    sSpaceID = aCache->mCommon.mSpaceID;
    sGGPID   = SDPTB_GET_GGHDR_PID_BY_FID( aFID );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 1;

    if( sdrMiniTrans::getTrans(&sMtx) != NULL )
    {
       sOpNTA = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );
    }
    else
    {
        /* Temporary Table �����ÿ��� Ʈ������� NULL�� ������ �� �ִ�. */
    }

    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         sSpaceID,
                                         sGGPID,
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         (void*)&sMtx,
                                         (UChar**)&sPagePtr,
                                         &sDummy,
                                         NULL /*IsCorruptPage*/ )
             != IDE_SUCCESS);

    sGGHdrPtr   = sdptbGroup::getGGHdr( sPagePtr );
    sAllocLGIdx = sdptbGroup::getAllocLGIdx(sGGHdrPtr);
    /*
     * �Ҵ��� �õ���  LG�� ã�´�
     *
     * ���� LG������ŭ�� ������� �˻��ؾ��Ѵ�.
     */
    sdptbBit::findBit( sGGHdrPtr->mLGFreeness[sAllocLGIdx].mBits,
                       sGGHdrPtr->mLGCnt,
                       &sLGID );
    //�Ʒ� �ΰ��� ������ �ϳ���  �ƴ϶�� ������.
    IDE_ERROR_MSG( (sLGID < sGGHdrPtr->mLGCnt) ||
                   (sLGID ==  SDPTB_BIT_NOT_FOUND),
                   "Error occurred while new extents find" 
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "Page ID : %"ID_UINT32_FMT", "
                   "Local Group ID : %"ID_UINT32_FMT", "
                   "Local Group Cnt : %"ID_UINT32_FMT")",
                   sSpaceID,
                   sGGPID,
                   sLGID,
                   sGGHdrPtr->mLGCnt );

    //�ϴ� �ش� LG���� �ϳ��� ext�� �Ҵ��Ѵ�.
    if( sLGID < sGGHdrPtr->mLGCnt ) //free�� �ִ� LG�� ã�Ҵ�.
    {
        sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aFID,
                                                sLGID,
                                                sAllocLGIdx,
                                                sGGHdrPtr->mPagesPerExt );

        IDE_TEST( allocExtsInLG( aStatistics,
                                 &sMtx,
                                 aCache,
                                 sGGHdrPtr,
                                 sLGHdrPID,
                                 aNrExts, //�Ҵ��û����
                                 aExtFstPID,   //�Ҵ��� extent�� ù��° pid array
                                 &sNrDoneInLG, //���� �Ҵ��� ����
                                 &sFreeInLG )  //free extent ��
          != IDE_SUCCESS );

       /*
        * LG�� free�� �ִٴ� ������ mLGFreeness���� �а� �������Ƿ�
        * �ϳ��� �Ҵ��ؾ߸��Ѵ�. �ƴ϶�� ġ��������
        */
        IDE_ERROR_MSG( sNrDoneInLG != 0,
                       "Error occurred while new extents find"
                       "(Tablespace ID : %"ID_UINT32_FMT", "
                       "Page ID : %"ID_UINT32_FMT", "
                       "ExtCnt : %"ID_UINT32_FMT")",
                       sSpaceID,
                       sGGPID,
                       sNrDoneInLG );


       if( sFreeInLG == 0 )
       {
           /*
            * extent�� ���� �Ҵ��Ѵ��� ��û�� LG�� free�� ���ٸ�
            *  LGFreeness��Ʈ�� ���߸� �Ѵ�.!
            */

           sBitIdx = SDPTB_GET_LGID_BY_PID( sLGHdrPID,
                                            sGGHdrPtr->mPagesPerExt);

           IDE_TEST( sdptbGroup::logAndSetLGFNBitsOfGG( &sMtx,
                                                        sGGHdrPtr,
                                                        sBitIdx,
                                                        0 ) //��Ʈ��0���� ��Ʈ
                     != IDE_SUCCESS );
       }
    }
    else
    {
        /*  !( sLGID <  sGGHdrPtr->mLGCnt)�ΰ���
         *  sLGID�� SDPTB_BIT_NOT_FOUND �� ���ۿ� ������Ѵ�.
         *  �ƴϸ� ġ���� ����.
         */
        IDE_ERROR_MSG( sLGID == SDPTB_BIT_NOT_FOUND,
                       "Error occurred while new extents find"
                       "(Tablespace ID : %"ID_UINT32_FMT", "
                       "Page ID : %"ID_UINT32_FMT", "
                       "Local Group ID : %"ID_UINT32_FMT")",
                       sSpaceID,
                       sGGPID,
                       sLGID );

        /*
         * space cache�� ���� ���� �����Ƿ� space cache ������ ����
         * GG�� �������� �ٸ� Tx�� ������ exts�� �Ҵ��� ���ϰ��
         * �̰����� ���ü��� �ִ�.
         */


        //�� GG�� free�� �ִ� LG�� ��� ����������Ƿ� NTA�ʿ����.
        IDE_CONT( return_anyway );
    }

    //4�����Ͽ�����(sdpst�� ���õǾ��ִ�)
    //setNTA�ϱ����� üũ.
    IDE_ERROR_MSG( (0 < sNrDoneInLG) && (sNrDoneInLG<= 4), 
                   "Error occurred while new extents find"
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "Page ID : %"ID_UINT32_FMT", "
                   "ExtCnt : %"ID_UINT32_FMT")",
                   sSpaceID,
                   sGGPID,
                   sNrDoneInLG );

    /*
     * sData[0] ~  sData[3] : �Ҵ��� extent�� ù��° pid����
     */
    for(i=0; i < sNrDoneInLG ; i++)
    {
        sData[i] = (ULong)aExtFstPID[i];
    }

    // Undo �ɼ� �ֵ��� OP NTA ó���Ѵ�.
    // TBS���� extent�� �����ϴµ� �ʿ��� ������ �����Ѵ�.
    sdrMiniTrans::setNTA( &sMtx,
                          sSpaceID,
                          SDR_OP_SDPTB_ALLOCATE_AN_EXTENT_FROM_TBS,
                          &sOpNTA,
                          sData,
                          sNrDoneInLG /* Data Count */ );

    IDE_EXCEPTION_CONT( return_anyway );

    //switching�� �õ��غ���.
    IDE_TEST( trySwitch( &sMtx,
                         sGGHdrPtr,
                         &sSwitching,
                         aCache ) != IDE_SUCCESS );

    sState=0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    *aNrDone = sNrDoneInLG;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState==1)
    {
       IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) ==  IDE_SUCCESS);
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *   LG �ȿ��� extent�� �Ҵ�޴´�.
 *
 * aStatistics - [IN] �������
 * aStartInfo  - [IN] Mini Transaction Start Info
 * aOrgNrExts  - [IN] �Ҵ��� ��û�� Extent ����
 * aExtFstPID  - [OUT] �Ҵ��� extent�� ù��° pid array
 * aNrDone     - [OUT] �Ҵ�� Extent ��
 * aFreeInLG   - [OUT] free Extent ��
 ***********************************************************************/
IDE_RC sdptbExtent::allocExtsInLG( idvSQL                  * aStatistics,
                                   sdrMtx                  * aMtx,
                                   sdptbSpaceCache         * aSpaceCache,
                                   sdptbGGHdr              * aGGPtr,
                                   scPageID                  aLGHdrPID,
                                   UInt                      aOrgNrExts,
                                   scPageID                * aExtFstPID,
                                   UInt                    * aNrDone,
                                   UInt                    * aFreeInLG )
{
    idBool       sDummy;
    sdptbLGHdr * sLGHdrPtr;
    UInt         sBitIdx;
    UInt         sNrExts = aOrgNrExts; //��û�Ѱ���
    UChar      * sPagePtr;
    scPageID     sLastPID;

    IDE_ASSERT( aMtx        != NULL );
    IDE_ASSERT( aSpaceCache != NULL );
    IDE_ASSERT( aGGPtr      != NULL );
    IDE_ASSERT( aExtFstPID  != NULL );
    IDE_ASSERT( aNrDone     != NULL );
    IDE_ASSERT( aFreeInLG   != NULL );
    IDE_ASSERT( aOrgNrExts  == 1 ); // segment���� ȣ��ɶ� 1�� �Ѱ��ش�.

    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         aSpaceCache->mCommon.mSpaceID,
                                         aLGHdrPID,
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         (void*)aMtx,
                                         (UChar**)&sPagePtr,
                                         &sDummy,
                                         NULL /*IsCorruptPage*/ )
             != IDE_SUCCESS );

    sLGHdrPtr = sdptbGroup::getLGHdr( sPagePtr );

    // �ɰ��� ������Ȳ!
    // �̹� GG���� free�� LG�� ã�Ƽ� �������Ƿ�.. �������λ�Ȳ������
    // mFree�� 0�ϼ��� ����.
    IDE_ERROR_MSG( sLGHdrPtr->mFree > 0,
                   "Error occurred while new extents alloc"
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "Page ID : %"ID_UINT32_FMT", "
                   "ExtCnt : %"ID_UINT32_FMT")",
                   aSpaceCache->mCommon.mSpaceID,
                   aLGHdrPID,
                   sLGHdrPtr->mFree );

    IDE_ERROR_MSG( sLGHdrPtr->mValidBits > sLGHdrPtr->mHint,
                   "Error occurred while new extents alloc"
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "Page ID : %"ID_UINT32_FMT", "
                   "Hint Cnt : %"ID_UINT32_FMT", "
                   "ValidBits : %"ID_UINT32_FMT")",
                   aSpaceCache->mCommon.mSpaceID,
                   aLGHdrPID,
                   sLGHdrPtr->mHint,
                   sLGHdrPtr->mValidBits );

    while( ( sNrExts > 0 ) && ( sLGHdrPtr->mFree > 0 ) )
    {
        //LG���� ���� ������� mValidBits���� ��ŭ�� �˻��ؾ��Ѵ�.
        sdptbBit::findZeroBitFromHint( sLGHdrPtr->mBitmap,
                                       sLGHdrPtr->mValidBits,
                                       sLGHdrPtr->mHint,
                                       &sBitIdx);

        //�ش� LG�� free�� �ִ°��� ���� �������Ƿ�
        //�̰� �����̵ȴٸ� �ɰ��� ������Ȳ�̴�.
        IDE_ERROR_MSG( sBitIdx < sLGHdrPtr->mValidBits,
                       "Error occurred while new extents alloc"
                       "(Tablespace ID : %"ID_UINT32_FMT", "
                       "Page ID : %"ID_UINT32_FMT", "
                       "Bit Index : %"ID_UINT32_FMT", "
                       "Valid Bits : %"ID_UINT32_FMT")",
                       aSpaceCache->mCommon.mSpaceID,
                       aLGHdrPID,
                       sBitIdx,
                       sLGHdrPtr->mValidBits );

        if( sBitIdx < sLGHdrPtr->mValidBits ) //�ε������� ��ȿ
        {
            allocByBitmapIndex( sLGHdrPtr,
                                sBitIdx );

            sNrExts--;

            //mFree, mBitmap�� ��� ó����
            IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                                 (UChar*)sLGHdrPtr,
                                                 &sBitIdx,
                                                 SDR_SDP_4BYTE,
                                                 SDR_SDPTB_ALLOC_IN_LG)
                      != IDE_SUCCESS );

            IDE_TEST( sdptbGroup::logAndModifyFreeExtsOfGG( aMtx,
                                                            aGGPtr,
                                                            -1 )
                      != IDE_SUCCESS );

            //extent�� ù��° PID���� ��´�.
            *aExtFstPID = sLGHdrPtr->mStartPID +
                         aSpaceCache->mCommon.mPagesPerExt*sBitIdx;

            //���� extent�� ������ pid�� ���Ѵ�.
            sLastPID = SDPTB_LAST_PID_OF_EXTENT(
                                         *aExtFstPID ,
                                         aSpaceCache->mCommon.mPagesPerExt );

            //���� �Ҵ�� extent�� ������ page id �� ���� ������ ������ ũ�ٸ�
            //HWM�� �����Ѵ�.
            if( aGGPtr->mHWM < sLastPID )
            {
                IDE_TEST( sdptbGroup::logAndSetHWMOfGG( aMtx,
                                                        aGGPtr,
                                                        sLastPID )
                          != IDE_SUCCESS );
            }

            aExtFstPID++;
        }// if
    }//while

    *aNrDone = aOrgNrExts - sNrExts;

    *aFreeInLG = sLGHdrPtr->mFree;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 * �Ҵ��� �õ��Ҽ��ִ� ������ FID�� ��´�.
 ***********************************************************************/
IDE_RC sdptbExtent::getAvailFID( sdptbSpaceCache   * aCache,
                                 sdFileID          * aFID)
{
    UInt sIdx;

    sdptbBit::findBitFromHintRotate( (void *)aCache->mFreenessOfGGs,
                                     aCache->mMaxGGID+1,   //�˻�����Ʈ��
                                     aCache->mGGIDHint,
                                     &sIdx );

    if( sIdx != SDPTB_BIT_NOT_FOUND )
    {
        IDE_ERROR_MSG( sIdx <= aCache->mMaxGGID,
                       "Error occurred while new extents alloc " 
                       "(Tablespace ID : %"ID_UINT32_FMT", "
                       "index : %"ID_UINT32_FMT", "
                       "MaxID : %"ID_UINT32_FMT")",
                       aCache->mCommon.mSpaceID,
                       sIdx,
                       aCache->mMaxGGID );

        *aFID = sIdx;

        /*
         * MaxGGID�� GG�� �׻� free extent�� �ִ°��� �ƴϴ�
         *
         * �������,
         *   MaxGGID�� 5�϶�
         *   ������ ���� ��Ʈ���� mFreenessOfGGs�� ����Ǿ��� �� �ִ�.
         *
         *   11000
         *
         * �׷��Ƿ� if( sIdx >=  aCache->mMaxGGID )  ���ؾ��Ѵ�.
         */
        if( sIdx >=  aCache->mMaxGGID )
        {
            aCache->mGGIDHint = 0;
        }
        else
        {
            aCache->mGGIDHint = sIdx + 1;
        }

    }
    else
    {
        *aFID = SDPTB_NOT_FOUND;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE; 
}

/***********************************************************************
 * Description:
 *   deallcation LG hdr�� free�� �ִٸ� switching�� �Ѵ�.
 *   switching �� �ߴٸ� aSwitching = ID_TRUE �� �ȴ�.
 ***********************************************************************/
IDE_RC sdptbExtent::trySwitch( sdrMtx                 * aMtx,
                               sdptbGGHdr             * aGGHdrPtr,
                               idBool                 * aSwitching,
                               sdptbSpaceCache        * aCache )
{
    UInt                 sOldLGType; //alloc lg
    UInt                 sNewLGType; //dealloc lg
    sdptbLGFreeInfo     *sOldFNPtr;
    sdptbLGFreeInfo     *sNewFNPtr;

    IDE_ASSERT( aGGHdrPtr != NULL);
    IDE_ASSERT( aSwitching != NULL);
    IDE_ASSERT( aCache != NULL );
    IDE_ASSERT( aGGHdrPtr->mAllocLGIdx < SDPTB_ALLOC_LG_IDX_CNT );

    sOldLGType =  sdptbGroup::getAllocLGIdx(aGGHdrPtr);
    sNewLGType =  sdptbGroup::getDeallocLGIdx(aGGHdrPtr);

    sOldFNPtr = &aGGHdrPtr->mLGFreeness[sOldLGType];
    sNewFNPtr = &aGGHdrPtr->mLGFreeness[sNewLGType];

    //switching����.
    if( (sOldFNPtr->mFreeExts == 0) && (sNewFNPtr->mFreeExts > 0) )
    {
        *aSwitching = ID_TRUE;

        IDE_TEST( sdptbGroup::logAndSetLGTypeOfGG( aMtx,
                                                   aGGHdrPtr,
                                                   sNewLGType )
                  != IDE_SUCCESS );

        /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
        sdptbBit::atomicSetBit32( (UInt*)aCache->mFreenessOfGGs,
                                  aGGHdrPtr->mGGID);
    }
    else
    {
        *aSwitching = ID_FALSE;

        //switching�� �����ߴٸ�,
        //������ free extents�� ���� 0�϶� cache�� freeness��Ʈ�� �����Ѵ�.
        if( sOldFNPtr->mFreeExts == 0 )
        {
            /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
            sdptbBit::atomicClearBit32( (UInt*)aCache->mFreenessOfGGs,
                                        aGGHdrPtr->mGGID);
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 *   on demand �� datafile�� auto extend �Ѵ�.
 ***********************************************************************/
IDE_RC sdptbExtent::autoExtDatafileOnDemand( idvSQL           *  aStatistics,
                                             UInt                aSpaceID,
                                             sdptbSpaceCache  *  aCache,
                                             void*               aTransForMtx,
                                             UInt                aNeededPageCnt)
{
    sdrMtxStartInfo sStartInfo;
    idBool          sDoExtend;
    UInt            sState=0;

    /*
     * allocExt���� ������ extent�� ũ���� ����� �� ���� �Ѱ��ֱ� ������
     * �̷����� �������̴�. Ȯ���� �ϱ����� assert
     */
    IDE_ASSERT( aCache         != NULL );
    IDE_ASSERT( aNeededPageCnt >= aCache->mCommon.mPagesPerExt );

    if ( aTransForMtx != NULL)
    {
        sStartInfo.mLogMode = SDR_MTX_LOGGING;
    }
    else
    {
        // Temproary Tablespace�� ���
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;
    }
    sStartInfo.mTrans = aTransForMtx;
    IDE_ERROR( sdptbGroup::prepareExtendFileOrWait( aStatistics,
                                                    aCache,
                                                    &sDoExtend)
                == IDE_SUCCESS);
    sState=1;

    IDE_TEST_CONT( sDoExtend == ID_FALSE, already_extended);

    IDE_TEST( sdptbGroup::makeMetaHeadersForAutoExtend( aStatistics,
                                                        &sStartInfo,
                                                        aSpaceID,
                                                        aCache,
                                                        aNeededPageCnt)
              != IDE_SUCCESS );

    sState=0;
    IDE_ERROR( sdptbGroup::completeExtendFileAndWakeUp( aStatistics,
                                                        aCache ) 
               == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( already_extended );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdptbGroup::completeExtendFileAndWakeUp( aStatistics,
                                                             aCache )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}


/***********************************************************************
 * Description: Temp Tablespace���� extent�� �̸� �Ҵ��ؼ� cache�Ѵ�.
 *              �߰� Ȯ�������� �ʰ�, Ȯ��� ���������� �Ҵ��Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aSpaceID       - [IN] Space ID
 **********************************************************************/
IDE_RC sdptbExtent::prepareCachedFreeExts( idvSQL           * aStatistics,
                                           sctTableSpaceNode* aSpaceNode )
{
    sdptbSpaceCache  *  sSpaceCache;
    sdrMtxStartInfo     sStartInfo;
    sdFileID            sFID = SD_MAKE_FID(SD_NULL_PID);
    UInt                sNrDone;
    scPageID            sExtFstPID;

    sStartInfo.mTrans   = NULL;
    sStartInfo.mLogMode = SDR_MTX_NOLOGGING;

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceNode );
    IDE_ASSERT( sSpaceCache != NULL );

    IDE_ASSERT( sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) == ID_TRUE );

    IDE_TEST( getAvailFID( sSpaceCache, &sFID ) != IDE_SUCCESS );

    while( sFID != SDPTB_NOT_FOUND )
    {
        IDE_TEST( tryAllocExtsInGG( aStatistics,
                                    &sStartInfo,
                                    sSpaceCache,
                                    sFID,
                                    1,
                                    &sExtFstPID,
                                    &sNrDone)
                  != IDE_SUCCESS );

        if( sNrDone == 0 )
        {
            break;
        }
        else
        {
            IDE_TEST( sSpaceCache->mFreeExtPool.push( ID_TRUE, /* Lock */
                                                      (void*)&sExtFstPID )
                      != IDE_SUCCESS );
        }

        IDE_TEST( getAvailFID( sSpaceCache, &sFID ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: [INTERFACE] temp tablespace�κ��� extent�� �Ҵ�޴´�.
 *
 * BUG-24730: Drop�� Temp Segment�� Extent�� ������ ���� �Ǿ��
 *            �մϴ�.
 *
 * Temp Segment�� ���� Drop, Create�ǰ� Bitmap TablespaceƯ����
 * Extent�� Free�ϰ� �ٽ� �Ҵ�� Free�� Extent�� �ٷ� �Ҵ�Ǵ� ����
 * �ƴϴ�. ���� ��� TBS�� Free Extent�� < 1, 2, 3 >�� �ִٰ� ����
 *
 *  1. alloc Extent = alloc:1, free extent< 2, 3 >
 *  2. free  Extent = free extent< 1, 2, 3 >
 *  3. alloc Extent = alloc:2, free extent< 1, 3 >
 *  4. free  Extent = free extent< 1, 2, 3 >
 *  5. alloc Extent = alloc:3, free extent< 1, 2 >
 *
 * Bitmap TBS�� Extent�Ҵ�� ������ �Ҵ�� Extent�� Free�Ǿ����ϴ�
 * �� �Ҵ�ÿ��� File�� �� �κ����� free extent�� ã�´�. File
 * ������ Free Extent�� ��� ã�Ҵٸ� �ٽ� ó������ free extent��
 * ã�´�.
 *
 * aStatistics - [IN] �������
 * aSpaceID    - [IN] TableSpace ID
 * aExtSlot    - [OUT] �Ҵ�� Extent Desc Array Ptr
 ***********************************************************************/
IDE_RC sdptbExtent::allocTmpExt( idvSQL          * aStatistics,
                                 scSpaceID         aSpaceID,
                                 sdpExtDesc      * aExtSlot )
{
    sdptbSpaceCache * sSpaceCache;
    sdrMtxStartInfo   sStartInfo;
    idBool            sIsEmpty;

    IDE_DASSERT( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) == ID_TRUE );

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ASSERT( sSpaceCache != NULL );

    IDE_TEST( sSpaceCache->mFreeExtPool.pop( ID_TRUE, /* Lock */
                                             (void*)&(aExtSlot->mExtFstPID),
                                             &sIsEmpty )
              != IDE_SUCCESS );

    if( sIsEmpty == ID_FALSE )
    {
        aExtSlot->mLength = sSpaceCache->mCommon.mPagesPerExt;
    }
    else
    {
        sStartInfo.mTrans   = NULL;
        sStartInfo.mLogMode = SDR_MTX_NOLOGGING;

        IDE_TEST( allocExts( aStatistics,
                             &sStartInfo,
                             aSpaceID,
                             1, /*need extent count */
                             aExtSlot ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description: �ϳ��� exts�� TBS�� �ݳ��Ѵ�.
 *   NTAó���� ����.
 *
 * aStatistics - [IN] �������
 * aMtx        - [IN] Mini transaction
 * aSpaceID    - [IN] ��ȯ�� tablespace id
 * aExtFstPID  - [IN] ��ȯ�� Extent�� ù��° PageID
 * aNrDone     - [OUT] free�� ������ ext�� ����
 ***********************************************************************/
IDE_RC sdptbExtent::freeExt( idvSQL           *  aStatistics,
                             sdrMtx           *  aMtx,
                             scSpaceID           aSpaceID,
                             scPageID            aExtFstPID,
                             UInt            *   aNrDone )
{
    UInt                 sLGID;
    UInt                 sEndIdx;
    sdptbSpaceCache    * sSpaceCache;
    sdptbSortExtSlot     sExtSlot;

    IDE_ASSERT( aNrDone != NULL);

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ERROR_MSG( sSpaceCache != NULL,
                   "Tablespace cache not found (ID : %"ID_UINT32_FMT")",
                    aSpaceID );

    sLGID = SDPTB_GET_LGID_BY_PID( aExtFstPID,
                                   sSpaceCache->mCommon.mPagesPerExt);

    sExtSlot.mExtFstPID    = aExtFstPID;
    sExtSlot.mLength       = 1;
    sExtSlot.mLocalGroupID = sLGID  ;

    IDE_ERROR( sctTableSpaceMgr::isDiskTableSpace(aSpaceID ) == ID_TRUE );

    IDE_TEST( freeExtsInLG( aStatistics,
                            aMtx,
                            aSpaceID,
                            &sExtSlot,
                            1,   // aNrSortedExt
                            0,   // aBeginIdx
                            &sEndIdx,
                            aNrDone)
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description: �ϳ��� extent�� Temp TBS�� �ݳ��Ѵ�.
 *
 * aSpaceID    - [IN] ��ȯ�� tablespace id
 * aExtFstPID  - [IN] ��ȯ�� Extent�� ù��° PageID
 ***********************************************************************/
IDE_RC sdptbExtent::freeTmpExt( scSpaceID   aSpaceID,
                                scPageID    aExtFstPID )
{
    sdptbSpaceCache    * sSpaceCache;

    // Temp Segment�� ��Ȱ��ȴ�.
    IDE_ASSERT_MSG( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) == ID_TRUE,
                   "Error occurred during drop Temp Table "
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "PID : %"ID_UINT32_FMT")",
                   aSpaceID,
                   aExtFstPID );

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ERROR_MSG( sSpaceCache != NULL,
                   "Tablespace cache not found (ID : %"ID_UINT32_FMT")",
                    aSpaceID );

    IDE_ERROR_MSG( aExtFstPID != SC_NULL_PID,
                   "Error occurred during drop Temp Table "
                   "(Tablespace ID : %"ID_UINT32_FMT", "
                   "PID : %"ID_UINT32_FMT")",
                   aSpaceID,
                   aExtFstPID );

    IDE_TEST( sSpaceCache->mFreeExtPool.push( ID_TRUE, /* Lock */
                                              (void*)&(aExtFstPID) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  extent���� TBS�� �ݳ��Ѵ�.  NTAó���� ���ȴ�.
 *  free�� ��� extent�� ���� LG�� �ִ�.(setNTA�� �׷��� ����)
 *
 *  aNrDone          - [OUT] free�� ������ ext�� ����
 ***********************************************************************/
IDE_RC sdptbExtent::freeExts( idvSQL           *  aStatistics,
                              sdrMtx           *  aMtx,
                              scSpaceID           aSpaceID,
                              ULong            *  aExtFstPIDs,
                              UInt                aArrElements)
{
    UInt                   sLGID;
    sdptbSortExtSlot       sExtSlot[4]; //allocExt���� �ִ� extent 4���Ҵ��ϹǷ�.
    UInt                   sDummy;
    UInt                   sNrDone;
    UInt                   i;
    sdptbSpaceCache      * sSpaceCache;

    IDE_ASSERT( aMtx != NULL);
    IDE_ASSERT( aExtFstPIDs != NULL );
    IDE_ASSERT( aArrElements > 0 );

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ASSERT( sSpaceCache != NULL );

    /*
     * �ϳ��� LG�� �ִ� extent�鿡 ���ؼ��� NTA�� ������.
     * �׷��Ƿ�, free�� ��� extent���� ���� LG�� �ִ�.
     */
    sLGID = SDPTB_GET_LGID_BY_PID( *aExtFstPIDs,
                                   sSpaceCache->mCommon.mPagesPerExt );

    //�� �Լ��� NTA Undo�ÿ��� ȣ��Ǹ�, TempTablespace�� Undo���� �ʴ´�.
    IDE_ASSERT( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) != ID_TRUE );

    for( i=0 ; i < aArrElements ; i++ )
    {
        sExtSlot[i].mExtFstPID    = *aExtFstPIDs;
        sExtSlot[i].mLength       = sSpaceCache->mCommon.mPagesPerExt;
        sExtSlot[i].mLocalGroupID = sLGID  ;
        aExtFstPIDs++;
    }

    IDE_TEST( freeExtsInLG( aStatistics,
                            aMtx,
                            aSpaceID,
                            sExtSlot,
                            aArrElements,
                            0,       // aBeginIdx
                            &sDummy, //aEndIdx
                            &sNrDone)
              != IDE_SUCCESS );

    // BUG-27329 CodeSonar::Uninitialized Variable (2)
    //��û�� ��� extent�� �����ؾ��Ѵ�.
    IDE_ASSERT( sNrDone == aArrElements );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *   [INTERFACE] free extents�� LG�� �ݳ��Ѵ�.
 *
 * aSortedExts  - [IN] sdptbSortExtSlot�� ��ҷ� ���� �迭�� �����ּ�
 * sNrElement   - [IN] �� �迭�� ����� ����
 *                     (segment���� �Ѱ��ִ� aSortedExts��
 *                     sNrElement�� ���� ���Լ� �ȿ��� ������ �ʴ´�.)
 * aEndIdx      - [OUT] free�� ������ ������ index
 * aNrDone      - [OUT] free�� ������ ext�� ����
 ***********************************************************************/
IDE_RC sdptbExtent::freeExtsInLG( idvSQL           *  aStatistics,
                                  sdrMtx           *  aMtx,
                                  scSpaceID           aSpaceID,
                                  sdptbSortExtSlot *  aSortedExts,
                                  UInt                aNrElement,
                                  UInt                aBeginIndex,
                                  UInt             *  aEndIndex,
                                  UInt             *  aNrDone)
{
    UInt                  sGGHdrPID;
    sdptbGGHdr        *   sGGHdrPtr;
    sdptbLGHdr        *   sLGHdrPtr;
    idBool                sDummy;
    sdptbSpaceCache   *   sCache;
    UInt                  i;
    sdFileID              sFID;
    UInt                  sLGID;
    UInt                  sStartFPID;
    UInt                  sExtentIDInLG; //LG�� mBitmap�� ��Ʈ�Ҷ� ���
    UChar             *   sPagePtr;
    idBool                sSwitching;
    ULong                 sTemp;
    UInt                  sDeallocLGType;

    IDE_ASSERT( aSortedExts != NULL );
    IDE_ASSERT( aEndIndex != NULL );
    IDE_ASSERT( aNrDone != NULL );

    sCache  = sddDiskMgr::getSpaceCache( aSpaceID );
    /* writeCommitLog ���� / undo �۾��̹Ƿ� ����ó�� ���� �ʴ´�. */
    IDE_ASSERT_MSG( sCache != NULL,
                   "Tablespace cache not found (ID : %"ID_UINT32_FMT")",
                   aSpaceID );

    i       = aBeginIndex;
    sFID    = SD_MAKE_FID( aSortedExts[i].mExtFstPID );
    sLGID   = aSortedExts[i].mLocalGroupID;

    IDE_ASSERT_MSG( sLGID < SDPTB_LGID_MAX ,
                   "Tablespace cache not found "
                   "(TableSpace ID : %"ID_UINT32_FMT", "
                   "Page ID : %"ID_UINT32_FMT", "
                   "ID : %"ID_UINT32_FMT")",
                   aSpaceID,
                   sFID,
                   sLGID );

    sGGHdrPID =SDPTB_GET_GGHDR_PID_BY_FID( sFID );
    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         aSpaceID,
                                         sGGHdrPID,
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         (void*)aMtx,
                                         (UChar**)&sPagePtr,
                                         &sDummy,
                                         NULL /*IsCorruptPage*/ )
             != IDE_SUCCESS);

    sGGHdrPtr = sdptbGroup::getGGHdr( sPagePtr);

    sStartFPID = SDPTB_EXTENT_START_FPID_FROM_LGID(sLGID, sGGHdrPtr->mPagesPerExt);

    //�ش� LG�� dealloc page���� ext���� �����Ѵ�.
    IDE_TEST(sdbBufferMgr::getPageByPID(
                                 aStatistics,
                                 aSpaceID,
                                 sdptbGroup::getDeallocLGHdrPID( sGGHdrPtr, sLGID ),
                                 SDB_X_LATCH,
                                 SDB_WAIT_NORMAL,
                                 SDB_SINGLE_PAGE_READ,
                                 (void*)aMtx,
                                 (UChar**)&sPagePtr,
                                 &sDummy,
                                 NULL /*IsCorruptPage*/ )
             != IDE_SUCCESS);

    sLGHdrPtr = sdptbGroup::getLGHdr( sPagePtr);

    ////////////////////////////////////////////////////////
    // ������ LG�� �ִ� ��� extent�� �Ѳ����� �����Ѵ�.
    ////////////////////////////////////////////////////////
    while( (sFID == SD_MAKE_FID( aSortedExts[i].mExtFstPID) )
           && ( sLGID == aSortedExts[i].mLocalGroupID ) )
    {
        sExtentIDInLG = (SD_MAKE_FPID(
                        aSortedExts[i].mExtFstPID )
                        - sStartFPID ) /sGGHdrPtr->mPagesPerExt;

        freeByBitmapIndex( sLGHdrPtr,
                           sExtentIDInLG );

        //LG hdr�� mFree, mBitmap ������ �α볲��
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sLGHdrPtr,
                                             &sExtentIDInLG,
                                             ID_SIZEOF( sExtentIDInLG ),
                                             SDR_SDPTB_FREE_IN_LG)
                  != IDE_SUCCESS );

        sDeallocLGType = sdptbGroup::getDeallocLGIdx(sGGHdrPtr);

        /*
         * GG hdr�� mFreeExts , mBits ������ �α��� �����.
         */
        IDE_TEST( sdptbGroup::logAndModifyFreeExtsOfGGByLGType( aMtx,
                                                                sGGHdrPtr,
                                                                sDeallocLGType,
                                                                +1 )
                  != IDE_SUCCESS );


        //dealloc LG�� �ش��Ʈ�� ������������ �α�
        if( sdptbBit::getBit( sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits,
                              sLGID) == SDPTB_BIT_OFF)
        {
            sdptbBit::setBit(sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits , sLGID );

            if( sLGID < SDPTB_BITS_PER_ULONG )
            {
                sTemp = sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits[0];

                IDE_TEST( sdrMiniTrans::writeNBytes(
                            aMtx,
                            (UChar*)
                            &(sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits[0]),
                            &sTemp,
                            ID_SIZEOF(sTemp) ) != IDE_SUCCESS );
            }
            else
            {
                sTemp = sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits[1];

                IDE_TEST( sdrMiniTrans::writeNBytes(
                            aMtx,
                            (UChar*)
                            &(sGGHdrPtr->mLGFreeness[sDeallocLGType].mBits[1]),
                            &sTemp,
                            ID_SIZEOF(sTemp) ) != IDE_SUCCESS );
            }
        }

        i++;

        if( i ==  aNrElement )
        {
            break;
        }
    }
    
    IDE_ASSERT_MSG( i > aBeginIndex,
                    "Error occurred while extent free "
                    "(Tablespace ID : %"ID_UINT32_FMT", "
                    "PageID : %"ID_UINT32_FMT", "
                    "BeginIndex : %"ID_UINT32_FMT", "
                    "Index : %"ID_UINT32_FMT")",
                    aSpaceID,
                    sdptbGroup::getDeallocLGHdrPID( sGGHdrPtr, sLGID ),
                    aBeginIndex,
                    i );

    /*
     * switching�� ����Ѵ�.
     */
    IDE_TEST( trySwitch( aMtx,
                         sGGHdrPtr,
                         &sSwitching,
                         sCache )
              != IDE_SUCCESS );

    *aNrDone = i - aBeginIndex;

    /*
     * end index�� ������ ��.��.�� ������ �ε��� ��ȣ�̴�
     * �׷��Ƿ� i���� ���ҽ�Ų�� �����ؾ��Ѵ�.
     */
    *aEndIndex = --i;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 ***********************************************************************/
void sdptbExtent::allocByBitmapIndex( sdptbLGHdr * aLGHdr,
                                      UInt         aIndex )
{
    IDE_ASSERT( aLGHdr != NULL );

    sdptbBit::setBit( aLGHdr->mBitmap, aIndex);

    IDE_ASSERT( aLGHdr->mFree > 0 ); // free extent�� ���� �־�� �Ҵ� ����
    aLGHdr->mFree-- ;

    // Hint ������ ��Ʈ�Ѵ�.
    aLGHdr->mHint = aIndex + 1;
}

/***********************************************************************
 * Description:
 ***********************************************************************/
void sdptbExtent::freeByBitmapIndex( sdptbLGHdr * aLGHdr,
                                     UInt         aIndex)
{
    IDE_ASSERT( aLGHdr != NULL );

    sdptbBit::clearBit( aLGHdr->mBitmap, aIndex);
    aLGHdr->mFree++ ;

    IDE_ASSERT( aLGHdr->mValidBits >= aLGHdr->mFree );

    if( aLGHdr->mHint > aIndex )
    {
        aLGHdr->mHint = aIndex;
    }
}

/***********************************************************************
 *
 * Description : Undo TBS�� Free ExtDir �������� �Ҵ��Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdptbExtent::tryAllocExtDir( idvSQL            * aStatistics,
                                    sdrMtxStartInfo   * aStartInfo,
                                    scSpaceID           aSpaceID,
                                    sdpFreeExtDirType   aFreeListIdx,
                                    scPageID          * aExtDirPID )
{
    UInt                 sState = 0;
    smLSN                sOpLSN;
    sdrMtx               sMtx;
    UChar              * sPagePtr;
    ULong                sNodeCnt;
    ULong                sData[2];
    sdptbGGHdr         * sGGHdrPtr;
    sdpSglPIDListNode  * sNode;
    sdptbSpaceCache    * sSpaceCache;
    idBool               sCurPageLatched;

    IDE_ASSERT( aStartInfo != NULL );
    IDE_ASSERT( aExtDirPID != NULL );
    IDE_ASSERT( sctTableSpaceMgr::isUndoTableSpace(aSpaceID) == ID_TRUE );

    *aExtDirPID     = SD_NULL_PID;

    sPagePtr        = NULL;
    sCurPageLatched = ID_FALSE;
    sSpaceCache     = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ERROR_MSG( sSpaceCache != NULL,
                   "Tablespace cache not found (ID : %"ID_UINT32_FMT")",
                   aSpaceID );

    IDE_TEST_CONT( sSpaceCache->mArrIsFreeExtDir[ aFreeListIdx ] == ID_FALSE,
                    CONT_NOT_FOUND_FREE_EXTDIR );

    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          SDPTB_GET_GGHDR_PID_BY_FID(0),
                                          &sPagePtr ) 
              != IDE_SUCCESS );
    sState = 1;

    sGGHdrPtr = sdptbGroup::getGGHdr( sPagePtr );

    sNodeCnt  = sdpSglPIDList::getNodeCnt(
                &(sGGHdrPtr->mArrFreeExtDirList[aFreeListIdx]));

    IDE_TEST_CONT( sNodeCnt == 0,
                    CONT_NOT_FOUND_FREE_EXTDIR_ONLYFIX );

    /*
     * BUG-25708 [5.3.1] UndoTBS�� Free ExtDirPage List�� �����ص� Race
     *           �߻��ϸ� ���ݾ� HWM�� ������ �� ����.
     */
     sdbBufferMgr::latchPage( aStatistics,
                              sPagePtr,
                              SDB_X_LATCH,
                              SDB_WAIT_NORMAL,
                              &sCurPageLatched  );

    IDE_TEST_CONT( sCurPageLatched == ID_FALSE,
                    CONT_NOT_FOUND_FREE_EXTDIR_ONLYFIX );
    sState = 2;
    sNodeCnt  = sdpSglPIDList::getNodeCnt(
                           &(sGGHdrPtr->mArrFreeExtDirList[aFreeListIdx]));

    if ( sNodeCnt > 0 )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 3;

        IDE_ASSERT( sdrMiniTrans::getTrans(&sMtx) != NULL );

        sOpLSN = smLayerCallback::getLstUndoNxtLSN( aStartInfo->mTrans );

        IDE_TEST( sdrMiniTrans::setDirtyPage( &sMtx, sPagePtr )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSglPIDList::removeNodeAtHead(
                              aStatistics,
                              &(sGGHdrPtr->mArrFreeExtDirList[aFreeListIdx]),
                              &sMtx,
                              aExtDirPID,
                              &sNode ) 
                   != IDE_SUCCESS );

        IDE_ASSERT( *aExtDirPID != SC_NULL_PID );

        sData[0] = aFreeListIdx;
        sData[1] = *aExtDirPID;

        sdrMiniTrans::setNTA( &sMtx,
                              aSpaceID,
                              SDR_OP_SDPTB_ALLOCATE_AN_EXTDIR_FROM_LIST,
                              &sOpLSN,
                              sData,
                              2 /* Data Count */);

        if ( sNodeCnt == 1 )
        {
            sSpaceCache->mArrIsFreeExtDir[ aFreeListIdx ] = ID_FALSE;
        }

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
    }

    sState = 0;
    IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_CONT( CONT_NOT_FOUND_FREE_EXTDIR_ONLYFIX );

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( CONT_NOT_FOUND_FREE_EXTDIR );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 3:
            sSpaceCache->mArrIsFreeExtDir[ aFreeListIdx ] = ID_TRUE;
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
            break;

        case 2:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                        == IDE_SUCCESS );
            break;

        case 1:
            IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics, sPagePtr )
                        == IDE_SUCCESS );
            break;

        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : ExtDir �������� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdptbExtent::freeExtDir( idvSQL            * aStatistics,
                                sdrMtx            * aMtx,
                                scSpaceID           aSpaceID,
                                sdpFreeExtDirType   aFreeListIdx,
                                scPageID            aExtDirPID )
{

    idBool               sDummy;
    UChar              * sPagePtr;
    UChar              * sExtDirPagePtr;
    ULong                sNodeCnt;
    sdptbGGHdr         * sGGHdrPtr;
    sdptbSpaceCache    * sSpaceCache;

    IDE_ASSERT( aExtDirPID != SD_NULL_PID );
    IDE_ASSERT( aMtx       != NULL );

    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ASSERT( sSpaceCache != NULL );

    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         aSpaceID,
                                         SDPTB_GET_GGHDR_PID_BY_FID(0),
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         aMtx,
                                         &sPagePtr,
                                         &sDummy,
                                         NULL ) != IDE_SUCCESS);

    sGGHdrPtr = sdptbGroup::getGGHdr( sPagePtr );

    sNodeCnt  = sdpSglPIDList::getNodeCnt(
                &(sGGHdrPtr->mArrFreeExtDirList[aFreeListIdx]));

    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         aSpaceID,
                                         aExtDirPID,
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         aMtx,
                                         &sExtDirPagePtr,
                                         &sDummy,
                                         NULL ) != IDE_SUCCESS);

    IDE_TEST( sdpSglPIDList::addNode2Head(
              &(sGGHdrPtr->mArrFreeExtDirList[aFreeListIdx]),
              sdpPhyPage::getSglPIDListNode( (sdpPhyPageHdr*)sExtDirPagePtr ),
              aMtx) != IDE_SUCCESS );

    if ( sNodeCnt == 0 )
    {
        sSpaceCache->mArrIsFreeExtDir[ aFreeListIdx ] = ID_TRUE;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  space id�� page id�� �Է¹޾Ƽ�
 *  page �� ���� extent�� free �������� �˻� �Ѵ�.
 *  �ʿ��ϴٸ� �ش� extent�� fst pid�� lst pid�� ��ȯ�� �� �ִ�.
 *
 * aStatistics - [IN]  �������
 * aSpaceID    - [IN]  Ȯ���ϰ��� �ϴ� page�� space id
 * aPageID     - [IN]  Ȯ���ϰ��� �ϴ� page�� page id
 * aIsFreeExt  - [OUT] page�� ���� extent�� free ���θ� ��ȯ�Ѵ�.
 ******************************************************************************/
IDE_RC sdptbExtent::isFreeExtPage( idvSQL      * aStatistics,
                                   scSpaceID     aSpaceID,
                                   scPageID      aPageID,
                                   idBool      * aIsFreeExt )
{
    UChar           * sGGHdrPagePtr ;
    UChar           * sLGHdrPagePtr ;
    UInt              sPageState = 0;
    scPageID          sGGPID;
    scPageID          sLGPID;
    scPageID          sExtFstPID = 0;
    UInt              sLGID;
    UInt              sPagesPerExt;
    UInt              sExtentIdx;
    sdptbGGHdr      * sGGHdr;
    sdptbLGHdr      * sLGHdr;
    idBool            sIsAllocExt;
    idBool            sTry;
    smiTableSpaceAttr sTBSAttr;

    IDE_ASSERT( sdpTableSpace::getExtMgmtType( aSpaceID ) == SMI_EXTENT_MGMT_BITMAP_TYPE );
    IDE_ASSERT( aIsFreeExt != NULL );

    /* BUG-27608 CodeSonar::Division By Zero (3)
     * Tablespace�� Drop�� ��� Page ������� �����Ƿ�,
     * Corrupt Page� ���� �� �� ����
     */
    if( sctTableSpaceMgr::hasState( aSpaceID, SCT_SS_INVALID_DISK_TBS ) == ID_TRUE )
    {
        *aIsFreeExt = ID_TRUE;

        IDE_CONT( skip_check_free_page );
    }

    *aIsFreeExt = ID_FALSE;

    sPagesPerExt = sdpTableSpace::getPagesPerExt( aSpaceID );

    // BUG-27608 CodeSonar::Division By Zero (3)
    IDE_ASSERT( 0 < sPagesPerExt );

    //------------------------------------------
    // GG Hdr PID�� LG Hdr PID�� ���ؼ�
    // ���� aPageID�� GG,LG Hdr���� Ȯ���Ѵ�.
    //------------------------------------------

    sGGPID = SDPTB_GET_GGHDR_PID_BY_FID( SD_MAKE_FID( aPageID ) );

    IDE_TEST_RAISE( aPageID == sGGPID , fail_read_gg );

    IDE_TEST_RAISE( sdbBufferMgr::getPageByPID( aStatistics,
                                                aSpaceID,
                                                sGGPID,
                                                SDB_S_LATCH,
                                                SDB_WAIT_NORMAL,
                                                SDB_SINGLE_PAGE_READ,
                                                NULL, /* aMtx */
                                                &sGGHdrPagePtr,
                                                &sTry,
                                                NULL /* isCorruptPage*/)
                    != IDE_SUCCESS , fail_read_gg );

    sPageState = 1 ;

    sGGHdr = sdptbGroup::getGGHdr( sGGHdrPagePtr );

    sLGID = SDPTB_GET_LGID_BY_PID( aPageID, sPagesPerExt );

    sLGPID = SDPTB_LG_HDR_PID_FROM_LGID( SD_MAKE_FID( aPageID ),
                                         sLGID,
                                         sdptbGroup::getAllocLGIdx( sGGHdr ),
                                         sPagesPerExt );

    IDE_TEST_RAISE( aPageID == sLGPID , fail_read_lg );

    // alloc Group Header pid�� �̿��ؼ�
    // dealloc Group Header�� pid�� ����Ѵ�.
    sLGPID = sLGPID + ( sdptbGroup::getAllocLGIdx( sGGHdr ) * (-2) + 1 ) ;

    IDE_TEST_RAISE( aPageID == sLGPID , fail_read_lg );

    sExtFstPID = SDPTB_GET_EXTENT_PID_BY_PID( aPageID, sPagesPerExt );

    //------------------------------------------
    // Hdr�� �о page�� ���� extent�� free���� Ȯ���Ѵ�.
    //------------------------------------------

    // extent�� HWM�̳��� �־�� �Ѵ�. LGID�� LGCnt���� Ŭ�� ����.
    // ������ �������� ������ ���� �Ҵ�� ���� ���� extent�̴�.
    if( ( sGGHdr->mHWM >= sExtFstPID ) && ( sGGHdr->mLGCnt > sLGID ) )
    {
        // �Ҵ� �� ���� �ִ� extent�̴�.
        // dealloc LG Hdr�� bitmap�� Ȯ���ؼ� free�� �Ǿ����� Ȯ���Ѵ�.

        IDE_TEST_RAISE( sdbBufferMgr::getPageByPID( aStatistics,
                                                    aSpaceID,
                                                    sLGPID,
                                                    SDB_S_LATCH,
                                                    SDB_WAIT_NORMAL,
                                                    SDB_SINGLE_PAGE_READ,
                                                    NULL, /* aMtx */
                                                    &sLGHdrPagePtr,
                                                    &sTry,
                                                    NULL /* isCorruptPage*/ )
                        != IDE_SUCCESS , fail_read_lg );

        sPageState = 2 ;

        sLGHdr = sdptbGroup::getLGHdr( sLGHdrPagePtr );

        // bit�� Ȯ���Ͽ� extent�� free���θ� �˾Ƴ���.

        sExtentIdx  = SDPTB_EXTENT_IDX_AT_LG_BY_PID( sExtFstPID, sPagesPerExt );

        sIsAllocExt = sdptbBit::getBit( sLGHdr->mBitmap, sExtentIdx );

        *aIsFreeExt = ( sIsAllocExt == ID_FALSE ) ? ID_TRUE : ID_FALSE ;

        sPageState = 1 ;

        IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                             sLGHdrPagePtr )
                  != IDE_SUCCESS );
    }
    else
    {
        // �Ҵ�� ���� ���� extent�̹Ƿ� Free �̴�.
        *aIsFreeExt = ID_TRUE;
    }

    sPageState = 0 ;

    IDE_TEST( sdbBufferMgr::releasePage( aStatistics,
                                         sGGHdrPagePtr )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( skip_check_free_page );

    return IDE_SUCCESS;

    // GG,LG Hdr�� corrupted page�� ���
    IDE_EXCEPTION( fail_read_gg );
    {
        sctTableSpaceMgr::getTBSAttrByID( aStatistics,
                                          aSpaceID,
                                          &sTBSAttr );
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_DRECOVER_GROUPHDR_IS_CORRUPTED,
                     sTBSAttr.mName,
                     sGGPID,
                     aPageID );

        IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                  aSpaceID,
                                  sGGPID ));
    }
    IDE_EXCEPTION( fail_read_lg );
    {
        sctTableSpaceMgr::getTBSAttrByID( aStatistics,
                                          aSpaceID,
                                          &sTBSAttr );
        ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                     SM_TRC_DRECOVER_GROUPHDR_IS_CORRUPTED,
                     sTBSAttr.mName,
                     sLGPID,
                     aPageID );

        IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                  aSpaceID,
                                  sLGPID ));
    }
    IDE_EXCEPTION_END;

    switch( sPageState )
    {
        case 2 :
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   sLGHdrPagePtr )
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   sGGHdrPagePtr )
                        == IDE_SUCCESS );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}
