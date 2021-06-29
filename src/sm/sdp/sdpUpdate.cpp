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
 * $Id: sdpUpdate.cpp 85791 2019-07-08 05:44:14Z et16 $
 **********************************************************************/

#include <smr.h>
#include <sdr.h>
#include <sdpDef.h>
#include <sdpReq.h>
#include <sdpPhyPage.h>
#include <sdpUpdate.h>
#include <sdptbGroup.h>
#include <sdptbSpaceDDL.h>
#include <sdpDPathInfoMgr.h>

/***********************************************************************
 * redo type:  SDR_SDP_1BYTE, SDR_SDP_2BYTE, SDR_SDP_4BYTE, SDR_SDP_8BYTE
 *             SDR_SDP_BINARY
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_NBYTE( SChar       * aData,
                                      UInt          aLength,
                                      UChar       * aPagePtr,
                                      sdrRedoInfo * /*aRedoInfo*/,
                                      sdrMtx      * aMtx )
{

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );
    IDE_ERROR( (aLength == SDR_SDP_1BYTE) ||
               (aLength == SDR_SDP_2BYTE) ||
               (aLength == SDR_SDP_4BYTE) ||
               (aLength == SDR_SDP_8BYTE) );

    idlOS::memcpy(aPagePtr, aData, aLength);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * redo type: SDR_SDP_BINARY
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_BINARY( SChar       * aData,
                                       UInt          aLength,
                                       UChar       * aPagePtr,
                                       sdrRedoInfo * /*aRedoInfo*/,
                                       sdrMtx      * aMtx )
{

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aMtx != NULL );

    if( aLength == SD_PAGE_SIZE)
    {
        IDE_ERROR( aPagePtr == sdpPhyPage::getPageStartPtr( aPagePtr));
        /*
         * sdbFrameHdr�� ���� mBCBPtr�̶�� member�� �ִ�.  �̰��� �� frame�� ����
         * ������ ������ �ִ� sdbBCB�� ���� �������̴�.  �̰��� ���ؼ� frame��
         * ���� BCB�� ���� ���� �ִ�.
         * ��� �������� ��ü image�� �α��ϰ� �̰��� redo�Ѵٰ� �غ���.
         * �α��� ����� frame�� ���� BCB�� redo�ϴ� �������� frame������
         * BCB�� �ٸ� ���̴�. ��, mBCBPtr�� �α�Ǿ �ȵǴ� ���̰�,
         * �׷��� ������ �翬�� redo�� �Ǿ�� �ȵȴ�.
         * */
        idlOS::memcpy(aPagePtr + ID_SIZEOF(sdbFrameHdr),
                      aData + ID_SIZEOF(sdbFrameHdr),
                      aLength - ID_SIZEOF(sdbFrameHdr));
    }
    else
    {
        idlOS::memcpy(aPagePtr, aData, aLength);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * redo type: SDR_SDP_WRITE_PAGEIMG, SDR_SDP_DPATH_INS_PAGE
 *            Backup ���̳�, Direct-Path INSERT�� �����
 *            Page ��ü�� ���� Image log redo
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_WRITE_PAGEIMG( SChar       * aData,
                                              UInt          aLength,
                                              UChar       * aPagePtr,
                                              sdrRedoInfo * /* aRedoInfo */,
                                              sdrMtx      * aMtx )
{
    IDE_ERROR( aData     != NULL );
    IDE_ERROR( aPagePtr  != NULL );
    IDE_ERROR( aMtx      != NULL );
    IDE_ERROR( aLength   == SD_PAGE_SIZE );
    IDE_ERROR( aPagePtr  == sdpPhyPage::getPageStartPtr(aPagePtr));

    /*
     * sdbFrameHdr�� ���� mBCBPtr�̶�� member�� �ִ�.  �̰��� �� frame�� ����
     * ������ ������ �ִ� sdbBCB�� ���� �������̴�.  �̰��� ���ؼ� frame��
     * ���� BCB�� ���� ���� �ִ�.
     * ��� �������� ��ü image�� �α��ϰ� �̰��� redo�Ѵٰ� �غ���.
     * �α��� ����� frame�� ���� BCB�� redo�ϴ� �������� frame������
     * BCB�� �ٸ� ���̴�. ��, mBCBPtr�� �α�Ǿ �ȵǴ� ���̰�,
     * �׷��� ������ �翬�� redo�� �Ǿ�� �ȵȴ�.
     * */
    idlOS::memcpy( aPagePtr + ID_SIZEOF(sdbFrameHdr),
                   aData + ID_SIZEOF(sdbFrameHdr),
                   aLength - ID_SIZEOF(sdbFrameHdr));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * redo type: SDR_SDP_PAGE_CONSISTENT
 *            Page�� Consistent ���¿� ���� log
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_PAGE_CONSISTENT( SChar       * aData,
                                                UInt          aLength,
                                                UChar       * aPagePtr,
                                                sdrRedoInfo * /*aRedoInfo*/,
                                                sdrMtx*       /*aMtx*/ )
{
    sdpPhyPageHdr* sPagePhyHdr = (sdpPhyPageHdr*)aPagePtr;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength  == 1 );
    IDE_ERROR( ( *aData == SDP_PAGE_INCONSISTENT ) ||
               ( *aData == SDP_PAGE_CONSISTENT ) );

    sPagePhyHdr->mIsConsistent = *aData ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 * redo type:  SDR_SDP_INIT_PHYSICAL_PAGE
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_INIT_PHYSICAL_PAGE( SChar       * aData,
                                                   UInt          aLength,
                                                   UChar       * aPagePtr,
                                                   sdrRedoInfo * /*aRedoInfo*/,
                                                   sdrMtx      * /*aMtx*/ )
{
    sdrInitPageInfo  sInitPageInfo;
    sdpParentInfo    sParentInfo;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF( sdrInitPageInfo ) );

    idlOS::memcpy( &sInitPageInfo, aData, ID_SIZEOF(sInitPageInfo) );

    sParentInfo.mParentPID   = sInitPageInfo.mParentPID;
    sParentInfo.mIdxInParent = sInitPageInfo.mIdxInParent;

    IDE_TEST( sdpPhyPage::initialize( sdpPhyPage::getHdr(aPagePtr),
                                      sInitPageInfo.mPageID,
                                      &sParentInfo,
                                      sInitPageInfo.mPageState,
                                      (sdpPageType)sInitPageInfo.mPageType,
                                      sInitPageInfo.mTableOID,
                                      sInitPageInfo.mIndexID )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * redo type:  SDR_SDP_INIT_LOGICAL_HDR
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_INIT_LOGICAL_HDR( SChar       * aData,
                                                 UInt          aLength,
                                                 UChar       * aPagePtr,
                                                 sdrRedoInfo * /*aRedoInfo*/,
                                                 sdrMtx      * /*aMtx*/ )
{
    UInt sLogicalHdrSize;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength  == ID_SIZEOF(UInt) );

    idlOS::memcpy(&sLogicalHdrSize, aData, aLength);

    sdpPhyPage::initLogicalHdr( sdpPhyPage::getHdr(aPagePtr),
                                sLogicalHdrSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * redo type:  SDR_SDP_INIT_SLOT_DIRECTORY
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_INIT_SLOT_DIRECTORY( SChar       * /*aData*/,
                                                    UInt          aLength,
                                                    UChar       * aPagePtr,
                                                    sdrRedoInfo * /*aRedoInfo*/,
                                                    sdrMtx      * /*aMtx*/ )
{
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength  == 0 );

    sdpSlotDirectory::init( sdpPhyPage::getHdr(aPagePtr) );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * redo type:  SDR_SDP_FREE_SLOT
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_FREE_SLOT( SChar       * aData,
                                          UInt          aLength,
                                          UChar       * aPagePtr,
                                          sdrRedoInfo * /*aRedoInfo*/,
                                          sdrMtx*         /*aMtx*/ )
{

    sdpPhyPageHdr*  sPageHdr;
    UChar*          sFreeSlotPtr;
    UShort          sSlotLen;

    IDE_ERROR( aData    != NULL );
    IDE_ERROR( aLength   > 0 );
    IDE_ERROR( aPagePtr != NULL );

    sFreeSlotPtr = aPagePtr;

    sPageHdr = (sdpPhyPageHdr*)sdpPhyPage::getPageStartPtr(sFreeSlotPtr);
    IDE_ERROR( sdpPhyPage::getPageType(sPageHdr) != SDP_PAGE_DATA );

    IDE_ERROR( aLength == ID_SIZEOF(UShort) );
    idlOS::memcpy(&sSlotLen, aData, ID_SIZEOF(UShort));

    IDE_TEST( sdpPhyPage::freeSlot(sPageHdr,
                                   sFreeSlotPtr,
                                   sSlotLen,
                                   ID_TRUE,
                                   SDP_8BYTE_ALIGN_SIZE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * redo type:  SDR_SDP_FREE_SLOT_FOR_SID
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_FREE_SLOT_FOR_SID( SChar       * aData,
                                                  UInt          aLength,
                                                  UChar       * aPagePtr,
                                                  sdrRedoInfo * aRedoInfo,
                                                  sdrMtx      * /* aMtx */ )
{

    sdpPhyPageHdr*  sPageHdr;
    UChar*          sFreeSlotPtr;
    scSlotNum       sSlotNum;
    UShort          sSlotLen;

    IDE_ERROR( aData     != NULL );
    IDE_ERROR( aLength    > 0 );
    IDE_ERROR( aPagePtr  != NULL );
    IDE_ERROR( aRedoInfo != NULL );

    sPageHdr = (sdpPhyPageHdr*)sdpPhyPage::getPageStartPtr(aPagePtr);
    IDE_ERROR( sdpPhyPage::getPageType(sPageHdr) == SDP_PAGE_DATA );

    IDE_ERROR( aLength == ID_SIZEOF(UShort) );
    idlOS::memcpy(&sSlotLen, aData, ID_SIZEOF(UShort));

    sSlotNum = aRedoInfo->mSlotNum;

    IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(
                                sdpPhyPage::getSlotDirStartPtr((UChar*)sPageHdr), 
                                sSlotNum,
                                &sFreeSlotPtr)
              != IDE_SUCCESS );

    IDE_DASSERT( sSlotLen == smLayerCallback::getRowPieceSize( sFreeSlotPtr ) );

    IDE_TEST( sdpPhyPage::freeSlot4SID( sPageHdr,
                                        sFreeSlotPtr,
                                        sSlotNum,
                                        sSlotLen )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * redo type:  SDR_SDP_RESTORE_FREESPACE_CREDIT
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_RESTORE_FREESPACE_CREDIT( SChar       *   aData,
                                                         UInt            aLength,
                                                         UChar       *   aPagePtr,
                                                         sdrRedoInfo * /*aRedoInfo*/,
                                                         sdrMtx      * /*aMtx*/ )
{

    sdpPhyPageHdr*  sPageHdr;
    UShort          sRestoreSize;
    UShort          sAvailableFreeSize;

    IDE_ERROR( aPagePtr != NULL );

    sPageHdr = (sdpPhyPageHdr*)sdpPhyPage::getPageStartPtr(aPagePtr);

    IDE_ERROR( aLength == ID_SIZEOF(UShort) );
    idlOS::memcpy(&sRestoreSize, aData, ID_SIZEOF(UShort));

    sAvailableFreeSize =
        sdpPhyPage::getAvailableFreeSize(sPageHdr);

    sAvailableFreeSize += sRestoreSize;

    sdpPhyPage::setAvailableFreeSize( sPageHdr,
                                      sAvailableFreeSize );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * redo type:  SDR_SDP_RESET_PAGE
 ***********************************************************************/
IDE_RC sdpUpdate::redo_SDR_SDP_RESET_PAGE( SChar       * aData,
                                           UInt          aLength,
                                           UChar       * aPagePtr,
                                           sdrRedoInfo * /* aRedoInfo */,
                                           sdrMtx      * /* aMtx */ )
{
    UInt      sLogicalHdrSize;

    IDE_ERROR( aData != NULL );
    IDE_ERROR( aPagePtr != NULL );
    IDE_ERROR( aLength == ID_SIZEOF(UInt) );

    idlOS::memcpy(&sLogicalHdrSize, aData, aLength);

    IDE_TEST( sdpPhyPage::reset(sdpPhyPage::getHdr(aPagePtr),
                                sLogicalHdrSize,
                                NULL) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * undo type: SDR_OP_SDP_DPATH_ADD_SEGINFO
 ******************************************************************************/
IDE_RC sdpUpdate::undo_SDR_OP_SDP_DPATH_ADD_SEGINFO(
                                        idvSQL          * /*aStatistics*/,
                                        sdrMtx          * aMtx,
                                        scSpaceID         /*aSpaceID*/,
                                        UInt              aSegInfoSeqNo )
{
    void              * sTrans;
    smTID               sTID;
    sdpDPathInfo      * sDPathInfo;
    sdpDPathSegInfo   * sDPathSegInfo;
    smuList           * sBaseNode;
    smuList           * sCurNode;
    
    sDPathSegInfo = NULL;

    //-----------------------------------------------------------------------
    // �� undo �Լ��� Rollback �ÿ��� ���Ǵ� �Լ���, Restart Recovery�ʹ�
    // ������ ����. ���� Restart Recovery�� �ƴ� ���� ���� �ڵ带 �����Ѵ�.
    //-----------------------------------------------------------------------
    if ( smLayerCallback::isRestartRecoveryPhase() == ID_FALSE )
    {
        sTrans      = sdrMiniTrans::getTrans( aMtx );
        sTID        = smLayerCallback::getTransID( sTrans );
        sDPathInfo  = (sdpDPathInfo*)smLayerCallback::getDPathInfo( sTrans );

        if( sDPathInfo == NULL )
        {
            ideLog::log( IDE_DUMP_0,
                         "Undo Trans ID : %u",
                         sTID );
            (void)smLayerCallback::dumpDPathEntry( sTrans );

            IDE_ERROR( 0 );
        }

        sBaseNode = &sDPathInfo->mSegInfoList;
        sCurNode = SMU_LIST_GET_LAST(sBaseNode);
        sDPathSegInfo = (sdpDPathSegInfo*)sCurNode;

        //-----------------------------------------------------------------
        // �α�� DPathSegInfo�� SeqNo�� ���� �޸𸮿� �����ϴ� ������
        // DPathSegInfo�� SeqNo�� ��ġ�ϴ��� Ȯ���Ͽ� Rollback�� �����Ѵ�.
        //-----------------------------------------------------------------
        if( (sDPathSegInfo != NULL) && (sDPathSegInfo->mSeqNo == aSegInfoSeqNo) )
        {
            IDE_ERROR( sdpDPathInfoMgr::destDPathSegInfo(
                                            sDPathInfo,
                                            sDPathSegInfo,
                                            ID_TRUE ) // Move LastFlag
                        == IDE_SUCCESS );
        }
        else
        {
            //----------------------------------------------------------------
            // �α�� DPathSegInfo�� SeqNo�� �޸𸮿� �����ϴ� ������
            // DPathSegInfo�� SeqNo�� ��ġ���� �ʴ� ���.
            //
            // �α��� �Ϸ� �Ǿ��ٸ�, �α�� SeqNo�� DPathSegInfo�� �޸𸮿�
            // �����ؾ� �ϴµ�, �������� �ʴ´ٴ� ���� �޸𸮿� ���������� ����
            // ������ �߻��ߴٴ� �ǹ��̹Ƿ� ASSERT ó���Ѵ�.
            //----------------------------------------------------------------

            ideLog::log( IDE_DUMP_0,
                         "Requested DPath INSERT Undo Seq. No : %u",
                         aSegInfoSeqNo );
            (void)sdpDPathInfoMgr::dumpDPathInfo( sDPathInfo );
            IDE_ERROR( 0 );
        }
    }
    else
    {
        // Restart Recovery�� ���, �����Ѵ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

