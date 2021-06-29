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
 * $Id: sdptbSpaceDDL.cpp 27228 2008-07-23 17:36:52Z newdaily $
 *
 * DDL�� ���õ� �Լ����̴�.
 **********************************************************************/

#include <smErrorCode.h>
#include <sdp.h>
#include <sdptb.h>
#include <sdd.h>
#include <sdpModule.h>
#include <sdpPhyPage.h>
#include <sdpReq.h>         //gSdpReqFuncList
#include <sctTableSpaceMgr.h>
#include <sdptbDef.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h>

/***********************************************************************
 * Description:
 *  ��� create TBS���� ȣ��Ǿ����� �������� �ٽɷ�ƾ
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::createTBS( idvSQL             * aStatistics,
                                 sdrMtxStartInfo    * aStartInfo,
                                 smiTableSpaceAttr  * aTableSpaceAttr,
                                 smiDataFileAttr   ** aFileAttr,
                                 UInt                 aFileAttrCount )
{
    scSpaceID         sSpaceID;
    sdptbSpaceCache * sCache;
    UInt              sPagesPerExt;
    UInt              sValidSmallSize;
    UInt              i;

    IDE_ASSERT( aTableSpaceAttr != NULL );
    IDE_ASSERT( aFileAttr       != NULL );
    IDE_ASSERT( aStartInfo      != NULL );
    IDE_ASSERT( aStartInfo->mTrans  != NULL );
    IDE_ASSERT( aTableSpaceAttr->mDiskAttr.mExtPageCount > 0 );

    /* FIT/ART/sm/Design/Resource/Bugs/BUG-14900/BUG-14900.tc */
    IDU_FIT_POINT( "1.TASK-1842@sdptbSpaceDDL::createTBS" );

    //1025�� �̻��� ������ �����Ҽ� ����.
    IDE_TEST_RAISE( aFileAttrCount > SD_MAX_FID_COUNT,
                    error_data_file_is_too_many );

    sPagesPerExt = aTableSpaceAttr->mDiskAttr.mExtPageCount;

    sValidSmallSize = sPagesPerExt + 
                      SDPTB_GG_HDR_PAGE_CNT + SDPTB_LG_HDR_PAGE_CNT;

    IDE_TEST( sdpTableSpace::checkPureFileSize( aFileAttr,
                                                aFileAttrCount,
                                                sValidSmallSize )
              != IDE_SUCCESS );

    //auto extend mode ���� �� next ������ ���� üũ�Ѵ�.
    checkDataFileSize( aFileAttr,
                       aFileAttrCount,
                       sPagesPerExt );

    /* ------------------------------------------------
     * disk �����ڸ� ���� tablespace ����
     * ----------------------------------------------*/
    IDE_TEST(sddDiskMgr::createTableSpace(aStatistics,
                                          aStartInfo->mTrans,
                                          aTableSpaceAttr,
                                          aFileAttr,
                                          aFileAttrCount,
                                          SMI_EACH_BYMODE) != IDE_SUCCESS);
    sSpaceID = aTableSpaceAttr->mID;

    /* Space ����� ���� Space Cache�� �Ҵ� �� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( sdptbGroup::allocAndInitSpaceCache(
                          sSpaceID,
                          aTableSpaceAttr->mDiskAttr.mExtMgmtType,
                          aTableSpaceAttr->mDiskAttr.mSegMgmtType,
                          aTableSpaceAttr->mDiskAttr.mExtPageCount )
              != IDE_SUCCESS );

    sCache = sddDiskMgr::getSpaceCache( sSpaceID );
    IDE_ERROR_MSG( sCache != NULL , 
                   "Unable to create tablespace. "
                   "(tablespace ID :%"ID_UINT32_FMT")\n",
                    sSpaceID );

    /* BUG-27368 [SM] ���̺����̽��� Data File ID�� ���������� ���� 
     *           ��쿡 ���� ����� �ʿ��մϴ�. */
    for( i = 0 ; i < aFileAttrCount ; i++ )
    {
        aFileAttr[i]->mID = i ;
    }

    IDE_TEST( sdptbGroup::makeMetaHeaders( aStatistics,
                                           aStartInfo,
                                           sSpaceID,
                                           sCache,
                                           aFileAttr,
                                           aFileAttrCount )   
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_data_file_is_too_many )
    {
          IDE_SET( ideSetErrorCode( smERR_ABORT_TOO_MANY_DATA_FILE ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 * redo_SCT_UPDATE_DRDB_CREATE_TBS ���� ȣ���ϴ� redo ���� ��ƾ
 * sdptbSpaceDDL::createTBS()�� ������ redo �Լ�
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::createTBS4Redo( void                * aTrans,
                                      smiTableSpaceAttr   * aTableSpaceAttr )
{
    scSpaceID         sSpaceID;
    sdptbSpaceCache * sCache;

    /* ------------------------------------------------
     * disk �����ڸ� ���� tablespace ����
     * ----------------------------------------------*/
    IDE_TEST( sddDiskMgr::createTableSpace4Redo( aTrans,
                                                 aTableSpaceAttr )
              != IDE_SUCCESS );

    sSpaceID = aTableSpaceAttr->mID;

    /* Space ����� ���� Space Cache�� �Ҵ� �� �ʱ�ȭ�Ѵ�. */
    IDE_TEST( sdptbGroup::allocAndInitSpaceCache(
                  sSpaceID,
                  aTableSpaceAttr->mDiskAttr.mExtMgmtType,
                  aTableSpaceAttr->mDiskAttr.mSegMgmtType,
                  aTableSpaceAttr->mDiskAttr.mExtPageCount )
              != IDE_SUCCESS );

    sCache = sddDiskMgr::getSpaceCache( sSpaceID );
    IDE_ASSERT( sCache != NULL );

    // sdptbGroup::makeMetaHeaders() �� ����������, �Ʒ��� �ؾ� ��.
    sCache->mGGIDHint   = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 * PROJ-1923 ALTIBASE HDB Disaster Recovery
 * redo_SCT_UPDATE_DRDB_CREATE_DBF ���� ȣ���ϴ� redo ���� ��ƾ
 * sdptbSpaceDDl::createDataFilesFEBT()�� ����
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::createDBF4Redo( void            * aTrans,
                                      smLSN             aCurLSN,
                                      scSpaceID         aSpaceID,
                                      smiDataFileAttr * aDataFileAttr )
{
    sdptbSpaceCache   * sCache          = NULL;
    UInt                sValidSmallSize = 0;
    UInt                sNewFileID      = 0;

    sddTableSpaceNode * sSpaceNode;

    IDE_ASSERT( aTrans          != NULL );
    IDE_ASSERT( aDataFileAttr   != NULL );

    IDU_FIT_POINT( "1.PROJ-1548@sdpTableSpace::createDataFiles" );

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void **)&sSpaceNode)
              != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode  != NULL );
    IDE_ASSERT( sCache      != NULL );

    /* �Ʒ� sddDiskMgr::createDataFiles()���� mNewFileID�� ����� �� �ִ�. */
    sNewFileID  = sSpaceNode->mNewFileID;

    /* �α� ��Ŀ�� newFileID �� redo �α��� mID�� ���� �ʴٸ�,
     * �α� ��Ŀ�� redo �αװ� ¦�� ���� �ʴٴ� ���̹Ƿ�,
     * redo�ϸ� �ȵȴ� */
    IDE_TEST( sNewFileID != aDataFileAttr->mID );

    /* 1025�� �̻��� ������ �����Ҽ� ����. */
    IDE_TEST_RAISE( (sSpaceNode->mNewFileID + (UInt)1) > SD_MAX_FID_COUNT,
                    error_data_file_is_too_many );

    IDE_TEST( sdpTableSpace::checkPureFileSize( &aDataFileAttr,
                                                1,
                                                sValidSmallSize )
              != IDE_SUCCESS );

    /* redo �̹Ƿ� TBS lock / unlock�� �����Ѵ�. */

    /* auto extend mode ���� �� next ������ ���� üũ�Ѵ�. */
    checkDataFileSize( &aDataFileAttr,
                       1,
                       sCache->mCommon.mPagesPerExt );

    /* �Ʒ� �Լ����� ����Ÿ���� ��忡 ���� (X) ����� ȹ���Ѵ�. */
    /* ------------------------------------------------
     * disk �����ڸ� ���� data file ����
     * ----------------------------------------------*/
    /* redo log 1���� ���� �ϵ��� �Ѵ�. */
    IDE_TEST( sddDiskMgr::createDataFile4Redo( aTrans,
                                               aCurLSN,
                                               aSpaceID,
                                               aDataFileAttr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_data_file_is_too_many )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_TOO_MANY_DATA_FILE ));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  reset��ƾ�� �ٽɷ�ƾ��.
 *  reset undoTBS, reset tempTBS���� ���������� ���Ǿ�����.
 *
 *  �� �Լ��ȿ����� IDE_TEST ���  IDE_ASSERT�� ����Ѵ�. �ֳ��ϸ�, 
 *  start up�ø� �ݵǹǷ� ����ó���� �ʿ���� �����̴�.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::resetTBSCore( idvSQL             *aStatistics,
                                    void               *aTransForMtx,
                                    scSpaceID           aSpaceID )
{
    smiTableSpaceAttr      sSpaceAttr;
    smiDataFileAttr      * sArrDataFileAttr;
    smiDataFileAttr     ** sArrDataFileAttrPtr;
    UInt                   sDataFileAttrCount = 0;

    sddTableSpaceNode    * sSpaceNode = NULL;
    sddDataFileNode      * sFileNode = NULL;

    sdrMtxStartInfo        sStartInfo;
    sdptbSpaceCache      * sCache;
    UInt                   i;

    IDE_ASSERT( aTransForMtx != NULL );

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );


    IDE_ASSERT(sctTableSpaceMgr::getTBSAttrByID( aStatistics,
                                                 aSpaceID,
                                                 &sSpaceAttr )
             == IDE_SUCCESS);

    IDE_ASSERT( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode)
                       == IDE_SUCCESS );
    IDE_ASSERT( sSpaceNode != NULL );
    IDE_ASSERT( sCache != NULL );

    IDE_ASSERT(iduMemMgr::malloc(
                    IDU_MEM_SM_SDP,
                    sSpaceNode->mDataFileCount * ID_SIZEOF(smiDataFileAttr*),
                    (void**)&sArrDataFileAttrPtr,
                    IDU_MEM_FORCE )
             == IDE_SUCCESS);

    IDE_ASSERT(iduMemMgr::malloc(
                    IDU_MEM_SM_SDP,
                    sSpaceNode->mDataFileCount * ID_SIZEOF(smiDataFileAttr),
                    (void**)&sArrDataFileAttr,
                    IDU_MEM_FORCE )
             == IDE_SUCCESS);

    for (i=0; i < ((sddTableSpaceNode*)sSpaceNode)->mNewFileID ;i++)
    {
        sFileNode=((sddTableSpaceNode*)sSpaceNode)->mFileNodeArr[i];

        if(sFileNode != NULL )
        {
            if( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) )
            {
                continue;
            }

            /* read from disk manager */
            sddDataFile::getDataFileAttr(sFileNode,
                                         &sArrDataFileAttr[sDataFileAttrCount]);

            sArrDataFileAttrPtr[sDataFileAttrCount] =
                     &sArrDataFileAttr[sDataFileAttrCount];


            sDataFileAttrCount++;
        }
    }

    sStartInfo.mTrans   = aTransForMtx;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    //reset�ÿ��� �̹� �����ϴ� file node8&  getDataFileAttr�� �о���Ե�.
    IDE_ASSERT( sdptbGroup::makeMetaHeaders(
                            aStatistics,
                            &sStartInfo,
                            aSpaceID,
                            sCache,
                            sArrDataFileAttrPtr,
                            sDataFileAttrCount )  
              == IDE_SUCCESS );

    IDE_ASSERT(iduMemMgr::free(sArrDataFileAttr) == IDE_SUCCESS);
    IDE_ASSERT(iduMemMgr::free(sArrDataFileAttrPtr) == IDE_SUCCESS);

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description:
 *  RID�� ��Ʈ�ϴ� �ٽ��Լ� 
 * 
 *  ���⼭�� ��κ��� ���ڿ����� assertó�� ���Կ� ����.
 *  �տ��� �������Ƿ� �ʿ����
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::setPIDCore( idvSQL        * aStatistics,
                                  sdrMtx        * aMtx,
                                  scSpaceID       aSpaceID,
                                  UInt            aIndex,
                                  scPageID        aTSSPID,
                                  sdptbRIDType    aRIDType)
{
    UChar           * sPagePtr;
    idBool            sDummy;
    scPageID        * sArrSegPID;
    sdptbGGHdr      * sGGHdr;

    IDE_ERROR_MSG( aRIDType < SDPTB_RID_TYPE_MAX,
                   "The RID TYPE is not valid. "
                   "(RID TYPE : %"ID_UINT32_FMT")\n", 
                   aRIDType );

    //ù��° ������ GG header�� TSS���������� ����Ǿ� �ִ�.
    IDE_TEST(sdbBufferMgr::getPageByPID( aStatistics,
                                         aSpaceID,
                                         SDPTB_GET_GGHDR_PID_BY_FID( 0 ),
                                         SDB_X_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_SINGLE_PAGE_READ,
                                         aMtx,
                                         &sPagePtr,
                                         &sDummy,
                                         NULL /*IsCorruptPage*/ ) != IDE_SUCCESS);

    sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

    if( aRIDType == SDPTB_RID_TYPE_TSS )
    {
        sArrSegPID = sGGHdr->mArrTSSegPID;
    }
    else
    {
        IDE_ERROR_MSG( aRIDType == SDPTB_RID_TYPE_UDS,
                       "The RID TYPE is not valid. "
                       "(RID TYPE : %"ID_UINT32_FMT")\n",
                       aRIDType );
        sArrSegPID = sGGHdr->mArrUDSegPID;
    }

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sArrSegPID[ aIndex ],
                                         (void*)&aTSSPID,
                                         ID_SIZEOF(aTSSPID) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  RID�� ���� �ٽ��Լ� 
 * 
 *  ���⼭�� ��κ��� ���ڿ����� assertó�� ���Կ� ����.
 *  �տ��� �������Ƿ� �ʿ����
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::getPIDCore( idvSQL        * aStatistics,
                                  scSpaceID       aSpaceID,
                                  UInt            aIndex,
                                  sdptbRIDType    aRIDType,
                                  scPageID      * aTSSPID )
{
    UChar           * sPagePtr;
    scPageID        * sArrSegPID;
    UInt              sState=0;
    sdptbGGHdr      * sGGHdr;

    IDE_ERROR_MSG( aRIDType < SDPTB_RID_TYPE_MAX, 
                   "The RID TYPE is not valid. "
                   "(RID TYPE : %"ID_UINT32_FMT")\n", 
                   aRIDType );

    //ù��° ������ GG header�� TSS���������� ����Ǿ� �ִ�.
    IDE_TEST( sdbBufferMgr::fixPageByPID( aStatistics,
                                          aSpaceID,
                                          SDPTB_GET_GGHDR_PID_BY_FID( 0 ),
                                          &sPagePtr )
              != IDE_SUCCESS );
    sState = 1;

    sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

    if( aRIDType == SDPTB_RID_TYPE_TSS )
    {
        sArrSegPID = sGGHdr->mArrTSSegPID;
    }
    else
    {
        IDE_ERROR_MSG( aRIDType == SDPTB_RID_TYPE_UDS,
                       "The RID TYPE is not valid. "
                       "(RID TYPE : %"ID_UINT32_FMT")\n",
                       aRIDType );
        sArrSegPID = sGGHdr->mArrUDSegPID;
    }

    *aTSSPID = sArrSegPID[ aIndex ];

    sState = 0;
    IDE_TEST( sdbBufferMgr::unfixPage( aStatistics,
                                       sPagePtr )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState != 0 )
    {
        IDE_ASSERT( sdbBufferMgr::unfixPage( aStatistics,
                                             sPagePtr )
                    == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  �ϳ��� ����Ÿ ȭ���� ������ �ø��ų� ���δ�.
 *
 * aFileName              - [IN] ũ�⸦ ������ ������ �̸�
 * aSizeWanted            - [IN] QP���� ��û�� ������ ����ũ��
 * aSizeChanged           - [OUT] ������ ����� ����ũ�� 
 * aValidDataFileName     - [OUT] ȣȯ���� ���ؼ��� ����.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterDataFileReSizeFEBT( idvSQL     *aStatistics,
                                               void       *aTrans,
                                               scSpaceID   aSpaceID,
                                               SChar      *aFileName,
                                               ULong       aSizeWanted,
                                               ULong      *aSizeChanged,
                                               SChar      *aValidDataFileName )
{
    sdrMtx              sMtx;
    UChar             * sPagePtr;
    idBool              sDummy;
    UInt                sState=0;
    sdptbGGHdr        * sGGHdr;
    sddDataFileNode   * sFileNode;
    sddTableSpaceNode * sSpaceNode;
    smLSN               sOpNTA;
    ULong               sData[2]; //����ũ�⸦ ������������ �����Ѵ�.
    UInt                sPageCntOld;
    sdptbSpaceCache  *  sCache;
    ULong               sMinSize; //�ּ����� ũ��
    ULong               sUsedPageCount; //������� ũ��

    IDE_DASSERT( aTrans       != NULL );
    IDE_DASSERT( aFileName    != NULL );
    IDE_DASSERT( aSizeChanged != NULL );
    IDE_DASSERT( aValidDataFileName != NULL );

    IDU_FIT_POINT( "1.TASK-1842@dpTableSpace::alterDataFileReSize" );
    /*
     * ���� ���� �����ҷ��� ����ũ��� �ϳ���  extent ���� ������ ���Ѵٸ�
     * �����޽����� ���
     */
    sCache = sddDiskMgr::getSpaceCache( aSpaceID );
    IDE_ERROR_MSG( sCache != NULL,
                   "The data file cannot be resized "
                   "because the tablespace cache does not exist or is not valid. "
                   "(spaceID : %"ID_UINT32_FMT")\n",
                   aSpaceID );

    // PRJ-1548 User Memory Tablespace
    // Ʈ������� �Ϸ�ɶ�(commit or abort) DataFile ����� �����Ѵ�.
    // -------- TBS List (IX) -> TBS Node(IX) -> DBF Node (X) -----------
    // �������� DBF Node�� ���ؼ� ����� ����ϴ� ���
    //
    // A. Ʈ����� COMMIT���� ���� DBF Node�� ONLINE�̴� .
    //    -> ����� ȹ���ϰ� resize�� �����Ѵ�.
    // B. Ʈ����� ROLLBACK���� DBF Node�� DROPPED�̴�.
    //    -> ����� ȹ�������� DBF Node ���°� DROPPED���� Ȯ���ϰ�
    //       exception�߻�
    //
    // # alter/drop/create dbf ����
    // 1. �̹� TBS Node (X) ����� ȹ���� ����
    // 2. TBS META PAGE (S) Latch ȹ��
    // 4. ���Ͽ���
    // 5. TBS META PAGE (S) Latch ����
    // 6. Ʈ����� �Ϸ�(commit or abort)���� ��� ��� ����

    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                 aTrans,
                                 aSpaceID,
                                 ID_FALSE, /* non-intent lock */
                                 ID_TRUE,   /* exclusive lock */
                                 SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState=1;

    IDE_TEST( getSpaceNodeAndFileNode( aSpaceID,
                                       aFileName,
                                       &sSpaceNode,
                                       &sFileNode,
                                       aValidDataFileName )
               != IDE_SUCCESS );

    IDE_ERROR_RAISE_MSG( sSpaceNode != NULL,  /* must be exist */
                         error_spaceNode_was_not_found, 
                         "The data file cannot be resized "
                         "because the tablespace node does not exist or is not valid. "
                         "(SpaceID : %"ID_UINT32_FMT")\n",
                           aSpaceID ); 
    IDE_ERROR_RAISE_MSG( sFileNode != NULL,  /* must be exist */
                         error_fileNode_was_not_found, 
                         "The data file cannot be resized "
                         "because the file node does not exist or is not valid. "
                         "(SpaceID : %"ID_UINT32_FMT", "
                         "FileName : %s)\n",
                         aSpaceID, 
                         aFileName ); 
    IDE_ERROR_MSG( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ),
                   "The data file cannot be resized "
                   "because the file was dropped. "
                   "(SpaceID : %"ID_UINT32_FMT", "
                   "FileName : %s)\n",
                   aSpaceID, 
                   aFileName );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          SDPTB_GET_GGHDR_PID_BY_FID(sFileNode->mID),
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          &sMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

    /*
     *   HWM ���� �۰� ������ ���ϼ��� ����.
     */
    // HWM�� PageID�� �� ���� Page�� ���̴�.
    sUsedPageCount = SD_MAKE_FPID( sGGHdr->mHWM );

    // �ּ� 1���� Extent Size���ٴ� Ŀ�� �Ѵ�.
    sMinSize = SDPTB_GG_HDR_PAGE_CNT +
               SDPTB_LG_HDR_PAGE_CNT +
               sCache->mCommon.mPagesPerExt;

    // BUG-29566 ������ ������ ũ�⸦ 32G �� �ʰ��Ͽ� �����ص� ������
    //           ������� �ʽ��ϴ�.
    // ����ڰ� �����ϱ� ���ϰ� �ϱ� ���� ū ���� ���ؼ� ������ ��ȯ�մϴ�.
    if( sUsedPageCount > sMinSize )
    {
        if( sUsedPageCount > aSizeWanted )
        {
            sState=0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            IDE_RAISE( error_cant_shrink_below_HWM );
        }
        else
        {
            /* nothing to do ... */
        }
    }
    else
    {
        if( sMinSize > aSizeWanted )
        {
            sState=0;
            IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
            IDE_RAISE( error_file_size_is_too_small );
        }
        else
        {
            /* nothing to do ... */
        }
    }

    sPageCntOld = sGGHdr->mTotalPages;

    // �Ʒ� �Լ����� ����Ÿ���� ��忡 ���� (X) ����� ȹ���Ѵ�.
    if( sddDiskMgr::alterResizeFEBT( aStatistics,
                                     aTrans,
                                     aSpaceID,
                                     aFileName , //valid�� ���ϸ���
                                     sGGHdr->mHWM,
                                     aSizeWanted,
                                     sFileNode) != IDE_SUCCESS )
    {
        sdrMiniTrans::setIgnoreMtxUndo( &sMtx );
        IDE_TEST( 1 );
    }
    else
    {
        /* nothing to do ... */
    }

    /* To Fix BUG-23868 [AT-F5 ART] Disk TableSpace�� Datafile 
     * Resize�� ���� ������ ���� ����. 
     * Tablspace�� Resize������ �����ϵ��� NTA������ �����ϸ�, 
     * ���� Rollback �߻��� GG�� �����ǰ� DataFile ���¿� ����ũ�Ⱑ 
     * �������� �ʴ´�. */
    if( sdrMiniTrans::getTrans(&sMtx) != NULL )
    {
       sOpNTA = smLayerCallback::getLstUndoNxtLSN( sdrMiniTrans::getTrans( &sMtx ) );
    }
    else
    {
        /* Temporary Table �����ÿ��� Ʈ������� NULL��
         * ������ �� �ִ�. */
    }

    if( sPageCntOld < aSizeWanted ) //Ȯ��
    {
        IDE_ERROR_MSG( sFileNode->mCurrSize == aSizeWanted,
                       "The data file cannot be resized "
                       "because the requested size of the data file is wrong. "
                       "(SpaceID : %"ID_UINT32_FMT", "
                       "Request Size : %"ID_UINT32_FMT", "
                       "Current Size : %"ID_UINT32_FMT")\n",
                       aSpaceID,
                       aSizeWanted,
                       sFileNode->mCurrSize );

        IDE_TEST( sdptbGroup::resizeGGCore( aStatistics,
                                            &sMtx,
                                            aSpaceID,
                                            sGGHdr,
                                            sFileNode->mCurrSize )
                    != IDE_SUCCESS );
    }
    else   //���
    {

        IDE_TEST( sdptbGroup::resizeGGCore( aStatistics,
                                            &sMtx,
                                            aSpaceID,
                                            sGGHdr,
                                            aSizeWanted )
                    != IDE_SUCCESS );
    }

    /*
     * ���� Ȯ���� �ߴٸ� cache�� FID��Ʈ�� ���ش�.
     */
    if( sFileNode->mCurrSize > sPageCntOld  )
    {
        /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
        sdptbBit::atomicSetBit32( (UInt*)sCache->mFreenessOfGGs, sFileNode->mID);
    }

    *aSizeChanged = aSizeWanted;

    sData[0] = sGGHdr->mGGID;
    sData[1] = sPageCntOld;

    sdrMiniTrans::setNTA( &sMtx,
                          aSpaceID,
                          SDR_OP_SDPTB_RESIZE_GG,
                          &sOpNTA,
                          sData,
                          2 /* Data Count */ );

    sState=0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;


    IDE_EXCEPTION( error_file_size_is_too_small );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_SHRINK_SIZE_IS_TOO_SMALL,
                                  aSizeWanted,
                                  sMinSize ));
    }
    IDE_EXCEPTION( error_cant_shrink_below_HWM );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CANT_SHRINK_BELOW_HWM,
                                  aSizeWanted,
                                  sUsedPageCount ));
    }
    IDE_EXCEPTION(error_spaceNode_was_not_found)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION(error_fileNode_was_not_found)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;

}


/***********************************************************************
 * Description:
 *  File �� �ش��ϴ� FileNode�� ��ȯ�ϰ� �ش� SpaceNode�� �Բ� ��ȯ�Ѵ�. 
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::getSpaceNodeAndFileNode(
                         scSpaceID               aSpaceID,
                         SChar                 * aFileName,
                         sddTableSpaceNode    ** aSpaceNode,
                         sddDataFileNode      ** aFileNode,
                         SChar                 * aValidDataFileName )
{
    UInt                  sNameLength;
    sddTableSpaceNode   * sSpaceNode;

    IDE_ASSERT( aFileName != NULL);
    IDE_ASSERT( aSpaceNode != NULL);
    IDE_ASSERT( aFileNode != NULL);
    IDE_ASSERT( aValidDataFileName != NULL);

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID(
                                    aSpaceID,
                                    (void**)&sSpaceNode)
                       != IDE_SUCCESS );

    IDE_ERROR_RAISE_MSG( sSpaceNode->mHeader.mID == aSpaceID,
                         error_spaceNode_was_not_found,
                         "The tablespace was not found "
                         "because the tablespace node does not exist or is not valid. "
                         "(SpaceID : %"ID_UINT32_FMT", "
                         "TapleSpace Node : %"ID_UINT32_FMT")",
                         aSpaceID,
                         sSpaceNode->mHeader.mID );

    idlOS::strcpy( aValidDataFileName, aFileName ); //useless

    sNameLength = idlOS::strlen(aFileName);

    IDE_TEST( sctTableSpaceMgr::makeValidABSPath(
                                ID_TRUE,
                                aFileName,
                                &sNameLength,
                                SMI_TBS_DISK)
              != IDE_SUCCESS );

    IDE_TEST( sddTableSpace::getDataFileNodeByName( sSpaceNode,
                                                    aFileName,
                                                    aFileNode ) != IDE_SUCCESS);

    *aSpaceNode = sSpaceNode;

    return IDE_SUCCESS;

    IDE_EXCEPTION(error_spaceNode_was_not_found)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  �ϳ��� ����Ÿ ȭ���� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::removeDataFile( idvSQL      * aStatistics,
                                      void        * aTrans,
                                      scSpaceID     aSpaceID,
                                      SChar       * aFileName,
                                      SChar       * aValidDataFileName )
{
    sdrMtx                  sMtx;
    UInt                    sState  = 0;
    UChar                 * sPagePtr;
    idBool                  sDummy;
    sdptbGGHdr            * sGGHdr;
    sddDataFileNode       * sFileNode;
    sddTableSpaceNode     * sSpaceNode;
    sdptbSpaceCache       * sCache;
    sctPendingOp          * sPendingOp;

    IDE_ASSERT( aTrans != NULL );

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );

    IDE_ERROR_MSG( sCache != NULL,
                   "The data file cannot be resized "
                   "because the tablespace cache does not exist or is not valid. "
                   "(spaceID : %"ID_UINT32_FMT")\n",
                   aSpaceID );

    // PRJ-1548 User Memory Tablespace
    // Ʈ������� �Ϸ�ɶ�(commit or abort) DataFile ����� �����Ѵ�.
    // ��߿��� ������ DBF Node�� ���ؼ� PENDING �������ε� DBF Node��
    // free���� �ʱ� ������ ����� commit �Ŀ� ����� �����ص� ������ ���� �ʴ´�

    // # alter/drop/create dbf ����
    // 1. �̹� TBS Node (X) ����� ȹ���� ����
    // 2. TBS META PAGE (S) Latch ȹ��
    // 4. ���Ͽ���
    // 5. TBS META PAGE (S) Latch ����
    // 6. Ʈ����� �Ϸ�(commit or abort)���� ��� ��� ����

    // --------- TBS NODE (IX) --------------- //
    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                 aTrans,
                                 aSpaceID,
                                 ID_FALSE,   /* non-intent lock */
                                 ID_TRUE,    /* exclusive lock */
                                 SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState=1;


    IDE_TEST( getSpaceNodeAndFileNode( aSpaceID,
                                       aFileName,
                                       &sSpaceNode,
                                       &sFileNode,
                                       aValidDataFileName )
               != IDE_SUCCESS );

    IDE_ERROR_RAISE_MSG( sSpaceNode != NULL,
                         error_spaceNode_was_not_found,
                         "The data file cannot be removed "             
                         "because the tablespace node does not exist or is not valid. " 
                         "(SpaceID : %"ID_UINT32_FMT")\n",
                          aSpaceID );  /* must be exist */
    IDE_ERROR_RAISE_MSG( sFileNode != NULL,
                          error_fileNode_was_not_found,
                         "The data file cannot be removed "             
                         "because the file node does not exist or is not valid. " 
                         "(SpaceID : %"ID_UINT32_FMT", "
                         "FileName : %s)\n", 
                         aSpaceID, aFileName );  /* must be exist */

    IDE_ERROR_MSG( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ),
                   "The data file cannot be removed "
                   "because the file was dropped. " 
                   "(SpaceID : %"ID_UINT32_FMT", "
                   "FileName : %s)\n",
                   aSpaceID, aFileName );

    /*
     *  ������ ���� �ȴٰ��ؼ� �����Ǵ� ������ GG�� ����� �ʿ�� ����.
     *  space cache�� �����Ǹ� �ȴ�. �׷��Ƿ�, S�� ������ �ȴ�.
     */
    IDE_TEST( sdbBufferMgr::getPageByPID( 
                                   aStatistics,
                                   aSpaceID,
                                   SDPTB_GET_GGHDR_PID_BY_FID(sFileNode->mID),
                                   SDB_S_LATCH,
                                   SDB_WAIT_NORMAL,
                                   SDB_SINGLE_PAGE_READ,
                                   &sMtx,
                                   &sPagePtr,
                                   &sDummy,
                                   NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    sGGHdr = sdptbGroup::getGGHdr(sPagePtr);


    /* ù��° ������ �������� ���ϵ��� �ؾ��Ѵ�.*/
    IDE_TEST_RAISE( sFileNode->mID == SDPTB_FIRST_FID,
                    error_can_not_remove_data_file);

    /*
     * ������ ������ HWM�� 0�϶�,�� �ش����Ͽ����� ��� �Ҵ絵 �̷��� ����
     * ���� ��쿡�� �����ϴ�.
     */
    IDE_TEST_RAISE( sGGHdr->mHWM != SD_CREATE_PID( sFileNode->mID, 0),
                    error_can_not_remove_data_file );

    // TBS META PAGE (S) Latch ����
    sState=0;
    IDE_TEST( sdrMiniTrans::commit(&sMtx) != IDE_SUCCESS );

    /* BUGBUG-6878 */
    IDE_TEST( sddDiskMgr::removeDataFileFEBT( aStatistics,
                                              aTrans,
                                              sSpaceNode,
                                              sFileNode,
                                              SMI_ALL_NOTOUCH) != IDE_SUCCESS );

    /*
     * To Fix BUG-23874 [AT-F5 ART] alter tablespace add datafile ��
     * ���� ������ �ȵǴ� �� ����.
     *
     * ���ŵ� ���Ͽ� ���� ���뵵�� SpaceNode�� �ݿ��Ҷ��� Ʈ����� Commit Pending����
     * ó���ؾ� �Ѵ�.
     */

    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                  aTrans,
                  sSpaceNode->mHeader.mID,
                  ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                  SCT_POP_UPDATE_SPACECACHE,
                  & sPendingOp )
              != IDE_SUCCESS );

    sPendingOp->mPendingOpFunc  = sdptbSpaceDDL::alterDropFileCommitPending;
    sPendingOp->mFileID         = sFileNode->mID;
    sPendingOp->mPendingOpParam = (void*)sCache;

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_can_not_remove_data_file)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_CannotRemoveDataFileNode  ) );
    }
    IDE_EXCEPTION(error_spaceNode_was_not_found)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundTableSpaceNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION(error_fileNode_was_not_found)
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileNode,
                                  aSpaceID ));
    }
    IDE_EXCEPTION_END;

    if(sState == 1)
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  ������ ũ�⸦ üũ�Ѵ�.
 ***********************************************************************/
void sdptbSpaceDDL::checkDataFileSize( smiDataFileAttr   ** aDataFileAttr,
                                       UInt                 aDataFileAttrCount,
                                       UInt                 aPagesPerExt )
{
    UInt                sLoop;
    smiDataFileAttr   * sDataFileAttrPtr;
    UInt                sTemp;
    UInt                sFileHdrSizeInBytes;
    UInt                sFileHdrPageCnt;

    IDE_DASSERT( aDataFileAttr != NULL );

    sFileHdrSizeInBytes = 
                     idlOS::align( SM_DBFILE_METAHDR_PAGE_SIZE, SD_PAGE_SIZE );

    sFileHdrPageCnt = sFileHdrSizeInBytes / SD_PAGE_SIZE;

    for( sLoop = 0; sLoop != aDataFileAttrCount; sLoop++ )
    {
        sDataFileAttrPtr = aDataFileAttr[ sLoop ];

        IDE_ASSERT( sDataFileAttrPtr->mCurrSize != 0 );
        IDE_ASSERT( sDataFileAttrPtr->mInitSize != 0 );

        IDE_ASSERT( sDataFileAttrPtr->mCurrSize ==
                    sDataFileAttrPtr->mInitSize );

        IDE_ASSERT( ((sDataFileAttrPtr->mIsAutoExtend == ID_TRUE) &&
                     (sDataFileAttrPtr->mNextSize != 0)) ||
                     (sDataFileAttrPtr->mIsAutoExtend == ID_FALSE) );

        /*
         * BUG-22351 TableSpace �� MaxSize �� �̻��մϴ�.
         */
        alignSizeWithOSFileLimit( &sDataFileAttrPtr->mInitSize,
                                  sFileHdrPageCnt );
        alignSizeWithOSFileLimit( &sDataFileAttrPtr->mCurrSize,
                                  sFileHdrPageCnt );

        //next����� extent�������  align
        if( sDataFileAttrPtr->mNextSize != 0)
        {
            if( sDataFileAttrPtr->mNextSize % aPagesPerExt )
            {
                sTemp = sDataFileAttrPtr->mNextSize / aPagesPerExt;
                sDataFileAttrPtr->mNextSize = (sTemp + 1) * aPagesPerExt;
            }
        }

        /*
         * BUG-22351 TableSpace �� MaxSize �� �̻��մϴ�.
         */
        if( sDataFileAttrPtr->mMaxSize == 0 )
        {
            // ����ڰ� maxsize�� ������� ���� ���
            // �Ǵ� unlimited�� ��� OS file limit�� ����Ͽ� �����Ѵ�.

            // BUG-17415 autoextend off�� ��� maxsize��
            // �ǹ̰� ���� ������ ���������� OS file limit�� �����Ѵ�.
            sDataFileAttrPtr->mMaxSize = sddDiskMgr::getMaxDataFileSize()
                                         - sFileHdrPageCnt;
        }
        else
        {

            alignSizeWithOSFileLimit( &sDataFileAttrPtr->mMaxSize,
                                      sFileHdrPageCnt );
        }

    }

}

/***********************************************************************
 * Description:
 *  space cache�� ����������Ѵ�.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::createDataFilesFEBT( idvSQL             * aStatistics,
                                           void               * aTrans,
                                           scSpaceID            aSpaceID,
                                           smiDataFileAttr   ** aDataFileAttr,
                                           UInt                 aDataFileAttrCount)
{
    sdptbSpaceCache     * sCache;
    UInt                  sPagesPerExt;
    UInt                  sStartNewFileID;
    sddTableSpaceNode   * sSpaceNode;
    sdrMtxStartInfo       sStartInfo;
    UInt                  sValidSmallSize;
    UInt                  sState    = 0;
    UInt                  i;

    IDE_ASSERT( aTrans          != NULL );
    IDE_ASSERT( aDataFileAttr   != NULL );

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void **)&sSpaceNode)
                       != IDE_SUCCESS );

    IDE_ASSERT( sSpaceNode  != NULL );
    IDE_ASSERT( sCache      != NULL );


    /* 1025�� �̻��� ������ �����Ҽ� ����. */
    IDE_TEST_RAISE( (sSpaceNode->mNewFileID + aDataFileAttrCount) > SD_MAX_FID_COUNT,
                    error_data_file_is_too_many );

    sPagesPerExt = sCache->mCommon.mPagesPerExt;

    sValidSmallSize = sPagesPerExt + 
                      SDPTB_GG_HDR_PAGE_CNT + SDPTB_LG_HDR_PAGE_CNT;

    IDE_TEST( sdpTableSpace::checkPureFileSize( aDataFileAttr,
                                                aDataFileAttrCount,
                                                sValidSmallSize )
              != IDE_SUCCESS );

    /* Ʈ������� �Ϸ�ɶ�(commit or abort) TableSpace ����� �����Ѵ�. */
    if( aTrans != NULL )
    {
       /* # alter/create/drop dbf ����
        * 1. �̹� TBS Node (X) ����� ȹ���� ����
        * 2. TBS META PAGE (S) Latch ȹ��
        * 4. ���Ͽ���
        * 5. TBS META PAGE (S) Latch ����
        * 6. Ʈ����� �Ϸ�(commit or abort)���� ��� ��� ���� */
       /* PRJ-1548 : --------- TBS NODE (IX) --------------- */

        /* BUG-31608 [sm-disk-page] add datafile during DML
         * Intensive Lock���� �����Ͽ� AddDataFile���� DML�� �����ϵ���
         * �����Ѵ�. */
        IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                     aTrans,
                                     aSpaceID,
                                     ID_TRUE,   /* intention lock */
                                     ID_TRUE,   /* exclusive lock */
                                     SCT_VAL_DDL_DML )
                  != IDE_SUCCESS );
    }

    sdptbGroup::prepareAddDataFile( aStatistics, sCache );
    sState = 1;

    /* �Ʒ� sddDiskMgr::createDataFiles()���� mNewFileID�� ����� �� �ִ�. */
    sStartNewFileID = sSpaceNode->mNewFileID;

    /* auto extend mode ���� �� next ������ ���� üũ�Ѵ�. */
    checkDataFileSize( aDataFileAttr,
                       aDataFileAttrCount,
                       sCache->mCommon.mPagesPerExt );

    /* �Ʒ� �Լ����� ����Ÿ���� ��忡 ���� (X) ����� ȹ���Ѵ�. */
    IDE_TEST( sddDiskMgr::createDataFiles( aStatistics,
                                           aTrans,
                                           aSpaceID,
                                           aDataFileAttr,
                                           aDataFileAttrCount,
                                           SMI_EACH_BYMODE )
              != IDE_SUCCESS );



    /* sdptb�� ���� ��Ÿ ������� ������ش�. */
    sStartInfo.mTrans = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    for( i=0 ; i < aDataFileAttrCount ; i++ )
    {
        aDataFileAttr[i]->mID =  sStartNewFileID + i;
    }

    IDE_TEST( sdptbGroup::makeMetaHeaders( aStatistics,
                                           &sStartInfo,
                                           aSpaceID,
                                           sCache,
                                           aDataFileAttr,
                                           aDataFileAttrCount ) 
              != IDE_SUCCESS );

    sState = 0;
    sdptbGroup::completeAddDataFile( sCache );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_data_file_is_too_many )
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_TOO_MANY_DATA_FILE ));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        sdptbGroup::completeAddDataFile( sCache );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  autoextend mode�� set�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterDataFileAutoExtendFEBT( idvSQL   *aStatistics,
                                                   void     *aTrans,
                                                   scSpaceID aSpaceID,
                                                   SChar    *aFileName,
                                                   idBool    aAutoExtend,
                                                   ULong     aNextSize,
                                                   ULong     aMaxSize,
                                                   SChar    *aValidDataFileName)
{
    sdrMtx                 sMtx;
    UChar                * sPagePtr;
    idBool                 sDummy;
    UInt                   sState=0;
    sdptbSpaceCache      * sCache;
    sddDataFileNode      * sFileNode;
    sddTableSpaceNode    * sSpaceNode;

    IDE_ASSERT( aTrans              != NULL );
    IDE_ASSERT( aFileName           != NULL );
    IDE_ASSERT( aValidDataFileName  != NULL );

    IDU_FIT_POINT( "1.TASK-1842@sdpTableSpace::alterDataFileAutoExtend" );

    // PRJ-1548 User Memory Tablespace
    // Ʈ������� �Ϸ�ɶ�(commit or abort) DataFile ����� �����Ѵ�.
    //
    // A. Ʈ����� COMMIT���� ���� DBF Node�� ONLINE�̴� .
    //    -> ����� ȹ���ϰ� resize�� �����Ѵ�.
    //
    // B. Ʈ����� ROLLBACK���� DBF Node�� DROPPED�̴�.
    //    -> ����� ȹ�������� DBF Node ���°� DROPPED���� Ȯ���ϰ�
    //       exception�߻�
    //
    // DBF ������ �ڵ�Ȯ�忬����� ���ü������� ������ ���� ������
    // ����� ȹ���ϰ� ������ �����Ѵ�.
    //
    // # alter/create/drop dbf ����
    // 1. �̹� TBS Node (X) ����� ȹ���� ����
    // 2. TBS META PAGE (S) Latch ȹ��
    // 4. ����Ȯ��
    // 5. TBS META PAGE (S) Latch ����
    // 6. Ʈ����� �Ϸ�(commit or abort)���� ��� ��� ����

    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                              aTrans,
                                              aSpaceID,
                                              ID_FALSE,    /* non-intent lock */
                                              ID_TRUE,    /* exclusive lock */
                                              SCT_VAL_DDL_DML )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aTrans,
                                   SDR_MTX_LOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState=1;

    IDE_TEST( getSpaceNodeAndFileNode( aSpaceID,
                                       aFileName,
                                       &sSpaceNode,
                                       &sFileNode,
                                       aValidDataFileName )
               != IDE_SUCCESS );

    sCache = sddDiskMgr::getSpaceCache( sSpaceNode );

    IDE_ASSERT( sSpaceNode != NULL );  /* must be exist */
    IDE_ASSERT( sFileNode != NULL );  /* must be exist */
    IDE_ASSERT( sCache != NULL );

    IDE_ASSERT( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ) );

    IDE_TEST( sdbBufferMgr::getPageByPID(
                               aStatistics,
                               aSpaceID,
                               SDPTB_GET_GGHDR_PID_BY_FID(sFileNode->mID),
                               SDB_S_LATCH,
                               SDB_WAIT_NORMAL,
                               SDB_SINGLE_PAGE_READ,
                               &sMtx,
                               &sPagePtr,
                               &sDummy,
                               NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    if( aMaxSize == 0 )
    {
        aMaxSize = sddDiskMgr::getMaxDataFileSize();
    }

    // �Ʒ� �Լ����� ����Ÿ���� ��忡 ���� (X) ����� ȹ���Ѵ�.
    IDE_TEST( sddDiskMgr::alterAutoExtendFEBT( aStatistics,
                                               aTrans,
                                               sSpaceNode,
                                               aFileName,
                                               sFileNode,
                                               aAutoExtend,
                                               aNextSize,
                                               aMaxSize ) != IDE_SUCCESS );

    // TBS META PAGE (S) Latch ����
    sState=0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : 
 *  ���� ��������� ����� ũ�Ⱑ OS limit���� ũ�ٸ� OS limit�� �����.
 *  (aFileHdrPageCnt�� �Ϲ������� 1�̴�)
 *
 *  aAlignDest          - [IN][OUT] ������ ���
 *  aFileHdrPageCnt     - [IN]      ��������� ����������(�Ϲ������� 1��)
 * 
 * BUG-22351 TableSpace �� MaxSize �� �̻��մϴ�.
 **********************************************************************/
void sdptbSpaceDDL::alignSizeWithOSFileLimit( ULong *aAlignDest,
                                              UInt   aFileHdrPageCnt )
{
    IDE_ASSERT( aAlignDest != NULL);

    if( (*aAlignDest + aFileHdrPageCnt) > sddDiskMgr::getMaxDataFileSize() )
    {
        *aAlignDest = sddDiskMgr::getMaxDataFileSize() - aFileHdrPageCnt;
    }
}



/***********************************************************************
 * Description : TableSpace�� Drop�Ѵ�.
 *
 * Implementation :
 *     sddDiskMgr::removeTableSpace( aSpace )
 *
 * aStatistics        - [IN] �������
 * aTrans             - [IN] Transaction Pointer
 * aSpaceID           - [IN] TableSpace ID
 * aTouchMode         - [IN] TableSpace �� ���� ������ ������ �� �����Ѵ�.
 *
 **********************************************************************/
IDE_RC sdptbSpaceDDL::dropTBS( idvSQL      * aStatistics,
                               void        * aTrans,
                               scSpaceID     aSpaceID,
                               smiTouchMode  aTouchMode )
{
    IDE_DASSERT( aTrans != NULL );
    
    /* FIT/ART/sm/Design/Resource/TASK-1842/DROP_TABLESPACE.tc */
    IDU_FIT_POINT( "1.PROJ-1548@sdpTableSpace::dropTBS" );

    // PRJ-1548 User Memory Tablespace
    // Ʈ������� �Ϸ�ɶ�(commit or abort) TableSpace ����� �����Ѵ�.
    // ����� ȹ��� ���Ŀ� �ش� ���̺� �����̽��� �䱸�ϴ� ���
    // Ʈ����ǵ� ���ۿ� ���ٵǾ�� �ȵȴ�.
    // -------------- TBS Node (X) ------------------ //

    IDE_TEST( sctTableSpaceMgr::lockTBSNodeByID(
                                    aTrans,
                                    aSpaceID,
                                    ID_FALSE,   /* non-intent */
                                    ID_TRUE,    /* exclusive */
                                    SCT_VAL_DROP_TBS) /* aValidation */
              != IDE_SUCCESS );

    IDE_TEST( sddDiskMgr::removeTableSpace( aStatistics,
                                            aTrans,
                                            aSpaceID,
                                            aTouchMode )
              != IDE_SUCCESS );

    IDU_FIT_POINT( "1.PROJ-1548@sdptbSpaceDDL::dropTBS" );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  Disk Tablespace�� ���� Alter Tablespace Online/Offline�� ����
 *
 * aTrans        - [IN] ���¸� �����Ϸ��� Transaction
 * aTableSpaceID - [IN] ���¸� �����Ϸ��� Tablespace�� ID
 * aState        - [IN] ���� ������ ���� ( Online or Offline )
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterTBSStatus( idvSQL*             aStatistics,
                                      void              * aTrans,
                                      sddTableSpaceNode * aSpaceNode,
                                      UInt                aState )
{
    IDE_DASSERT( aTrans     != NULL );
    IDE_DASSERT( aSpaceNode != NULL );

    switch ( aState )
    {
        case SMI_TBS_ONLINE:
            IDE_TEST( alterTBSonline( aStatistics,
                                      aTrans,
                                      aSpaceNode )
                      != IDE_SUCCESS );
            break;

        case SMI_TBS_OFFLINE:
            IDE_TEST( alterTBSoffline( aStatistics,
                                       aTrans,
                                       aSpaceNode )
                      != IDE_SUCCESS );
            break;

        default:
            IDE_ASSERT( 0 );
            break;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *   Tablespace�� DISCARDED���·� �ٲٰ�, Loganchor�� Flush�Ѵ�.
 *
 *   *Discard�� ���� :
 *      - �� �̻� ����� �� ���� Tablespace
 *      - ���� Drop���� ����
 *
 *   *��뿹 :
 *      - Disk�� ������ ������ ���̻� ���Ұ��� ��
 *        �ش� Tablespace�� discard�ϰ� ������ Tablespace���̶�
 *        ��ϰ� ������, CONTROL�ܰ迡�� Tablespace�� DISCARD�Ѵ�.
 *
 *    *���ü����� :
 *      - CONTROL�ܰ迡���� ȣ��Ǳ� ������, sctTableSpaceMgr��
 *        Mutex�� ���� �ʿ䰡 ����.
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterTBSdiscard( sddTableSpaceNode  * aTBSNode )
{
    IDE_DASSERT( aTBSNode != NULL );

    IDE_TEST_RAISE ( SMI_TBS_IS_DISCARDED( aTBSNode->mHeader.mState),
                     error_already_discarded );

    aTBSNode->mHeader.mState = SMI_TBS_DISCARDED;

    IDE_TEST( smrRecoveryMgr::updateTBSNodeToAnchor( & aTBSNode->mHeader )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_already_discarded );
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_TBS_ALREADY_DISCARDED));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  ȭ���� �̸��� �ٲ۴�.
 *  sddDiskMgr::alterDataFileName �� �����Ǿ��� �ִ�.
 **********************************************************************/
IDE_RC sdptbSpaceDDL::alterDataFileName( idvSQL      *aStatistics,
                                         scSpaceID    aSpaceID,
                                         SChar       *aOldName,
                                         SChar       *aNewName )
{

    IDE_TEST( sddDiskMgr::alterDataFileName( aStatistics,
                                             aSpaceID,
                                             aOldName,
                                             aNewName )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 *   META/SERVICE�ܰ迡�� Tablespace�� Online���·� �����Ѵ�.
 *
 *   [ �˰��� ]
 *     (010) TBSNode�� X�� ȹ��
 *     (020) Tablespace�� Backup���̶�� Backup����ñ��� ���
 *     (030) "TBSNode.Status := ONLINE"�� ���� �α�
 *     (040) TBS Commit Pending���
 *     (050) "DBFNode.Status := ONLINE"�� ���� �α�
 *     (060) DBF Commit Pending���
 *     (note-1) Log anchor�� TBSNode�� flush���� �ʴ´�.
 *              (commit pending���� ó��)
 *     (note-2) Log anchor�� DBFNode�� flush���� �ʴ´�.
 *              (commit pending���� ó��)
 *
 *   [ Commit�� ]
 *     * TBS pending
 *     (c-010) TBSNode.Status := ONLINE    (��1)
 *     (c-020) Table�� Runtime���� �ʱ�ȭ
 *     (c-030) �ش� TBS�� ���� ��� Table�� ���� Index Header Rebuilding �ǽ�
 *     (c-040) Flush TBSNode To LogAnchor
 *
 *     * DBF pending
 *     (c-030) DBFNode.Status := ONLINE    (��2)
 *     (c-040) Flush DBFNode To LogAnchor
 *
 *   [ Abort�� ]
 *     [ UNDO ] ����
 *
 *   [ TBS REDO ]
 *     (r-010) (030)�� ���� REDO�� Commit Pending ���
 *     (note-1) TBSNode�� loganchor�� flush���� ����
 *              -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
 *     (note-2) Restart Recovery�Ϸ��� (070), (080)�� �۾��� ����ǹǷ�
 *              Redo�߿� �̸� ó������ �ʴ´�.
 *
 *   [ TBS UNDO ]
 *     (u-040) (030)�� ���� UNDO�� TBSNode.Status := Before Image(OFFLINE)
 *             -> TBSNode.Status�� Commit Pending���� ����Ǳ� ������
 *                ���� undo�߿� Before Image�� ����ĥ �ʿ�� ����.
 *                �׷��� �ϰ����� �����ϱ� ���� TBSNode.Status��
 *                Before Image�� �����ϵ��� �Ѵ�.
 *
 *     (note-1) TBSNode�� loganchor�� flush���� ����
 *              -> ALTER TBS ONLINEE�� Commit Pending�� ����
 *                 COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
 *
 *   [ �������� ]
 *      �� �Լ��� META, SERVICE�ܰ迡�� ONLINE���� �ø� ��쿡�� ȣ��ȴ�.
 *
 *   aTrans         - [IN] ���¸� �����Ϸ��� Transaction
 *   aSpaceNode     - [IN] ���¸� ������ Tablespace�� Node
 ************************************************************************/
IDE_RC sdptbSpaceDDL::alterTBSonline(idvSQL*              aStatistics,
                                     void               * aTrans,
                                     sddTableSpaceNode  * aSpaceNode )
{
    sctPendingOp    * sPendingOp;
    UInt              sBeforeState;
    UInt              sAfterState;
    sddDataFileNode * sFileNode;
    smLSN             sOnlineLSN;
    UInt              i;

    SM_LSN_INIT( sOnlineLSN );

    ///////////////////////////////////////////////////////////////////////////
    //  (010) TBSNode�� X�� ȹ��
    //
    // Tablespace�� Offline���¿��� ������ ���� �ʴ´�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                                   aTrans,
                                   & aSpaceNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   SCT_VAL_ALTER_TBS_ONOFF) /* validation */
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (e-010) Tablespace���¿� ���� ����ó��
    IDE_TEST( sctTableSpaceMgr::checkError4AlterStatus(
                                      (sctTableSpaceNode*)aSpaceNode,
                                      SMI_TBS_ONLINE /* New State */ )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (020) Tablespace�� Backup���̶�� Backup����ñ��� ���
    IDE_TEST( sctTableSpaceMgr::wait4BackupAndBlockBackup( aStatistics,
                                                           (sctTableSpaceNode*)aSpaceNode,
                                                           SMI_TBS_SWITCHING_TO_ONLINE )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (030) "TBSNode.Status := ONLINE"�� ���� �α�

    sBeforeState = aSpaceNode->mHeader.mState ;

    // �α��ϱ� ���� Backup�����ڿ��� ���ü� ��� ���� �ӽ� Flag�� ����
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_OFFLINE;
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_ONLINE;

    sAfterState  = sBeforeState;
    sAfterState &= ~SMI_TBS_OFFLINE;
    sAfterState |=  SMI_TBS_ONLINE;

    IDE_TEST( smrUpdate::writeTBSAlterOnOff( aStatistics,
                                             aTrans,
                                             aSpaceNode->mHeader.mID,
                                             SCT_UPDATE_DRDB_ALTER_TBS_ONLINE,
                                             sBeforeState,
                                             sAfterState,
                                             &sOnlineLSN )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (040) TBS Commit Pending���
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                  aTrans,
                  aSpaceNode->mHeader.mID,
                  ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                  SCT_POP_ALTER_TBS_ONLINE,
                  & sPendingOp ) != IDE_SUCCESS );

    sPendingOp->mPendingOpFunc = sdptbSpaceDDL::alterOnlineCommitPending;
    sPendingOp->mNewTBSState   = sAfterState;

    // fix BUG-17456 Disk Tablespace online���� update �߻��� index ���ѷ���
    SM_GET_LSN( sPendingOp->mOnlineTBSLSN, sOnlineLSN );

    ///////////////////////////////////////////////////////////////////////////
    //  (050) DBF Online �α�
    //  (060) DBF Commit Pending���

    // Transaction Commit�ÿ� ������ DBFNode�� ���¸�
    // Offline���� �����ϴ� Pending Operation ���
    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL)
        {
            continue;
        }

        sBeforeState = sFileNode->mState ;

        sAfterState  = sBeforeState;
        sAfterState &= ~SMI_FILE_OFFLINE;
        sAfterState |=  SMI_FILE_ONLINE;

        IDE_TEST( smrUpdate::writeDiskDBFAlterOnOff(
                      aStatistics,
                      aTrans,
                      aSpaceNode->mHeader.mID,
                      sFileNode->mID,
                      SCT_UPDATE_DRDB_ALTER_DBF_ONLINE,
                      sBeforeState,
                      sAfterState )
                != IDE_SUCCESS );

        IDE_TEST( sddDataFile::addPendingOperation(
                  aTrans,
                  sFileNode,
                  ID_TRUE,        /* Pending ���� ���� ���� : Commit �� */
                  SCT_POP_ALTER_DBF_ONLINE,
                  &sPendingOp ) != IDE_SUCCESS );

        sPendingOp->mNewDBFState   = sAfterState;
        sPendingOp->mPendingOpFunc = NULL; // pending �� ó���� �Լ��� ����.

        // PRJ-1548 User Memory Tablespace
        // TBS Node�� X ����� ȹ���ϱ� ������ DBF Node�� X �����
        // ȹ���� �ʿ䰡 ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  META, SERVICE�ܰ迡�� Tablespace�� Offline���·� �����Ѵ�.
 *
 *  [ �˰��� ]
 *    (010) TBSNode�� X�� ȹ��
 *    (020) Tablespace�� Backup���̶�� Backup����ñ��� ���
 *    (030) "TBSNode.Status := Offline" �� ���� �α�
 *    (040) TBSNode.OfflineSCN := Current System SCN
 *    (050) Instant Disk Aging �ǽ� - Aging �����߿��� ��� Ager ��ġȹ��
 *    (060) Dirty Page Flush �ǽ�
 *    (070) Commit Pending���
 *
 *  [ Commit�� : (Pending) ]
 *    (c-010) TBSNode.Status := Offline
 *    (c-050) Free All Index Header of TBS
 *    (c-060) Free Runtime Info At Table Header
 *    (c-070) Free Runtime Info At TBSNode ( Expcet Lock )
 *    (c-080) flush TBSNode to loganchor
 *
 *  [ Abort�� ]
 *    [ UNDO ] ����
 *
 *  [ REDO ]
 *    (u-010) (020)�� ���� REDO�� TBSNode.Status := After Image(OFFLINE)
 *    (note-1) TBSNode�� loganchor�� flush���� ����
 *             -> Restart Recovery�Ϸ��� ��� TBS�� loganchor�� flush�ϱ� ����
 *    (note-2) Commit Pending�� �������� ����
 *             -> Restart Recovery�Ϸ��� OFFLINE TBS�� ���� Resource������ �Ѵ�
 *
 *  [ UNDO ]
 *    (u-010) (020)�� ���� UNDO�� TBSNode.Status := Before Image(ONLINE)
 *            TBSNode.Status�� Commit Pending���� ����Ǳ� ������
 *            ���� undo�߿� Before Image�� ����ĥ �ʿ�� ����.
 *            �׷��� �ϰ����� �����ϱ� ���� TBSNode.Status��
 *            Before Image�� �����ϵ��� �Ѵ�.
 *    (note-1) TBSNode�� loganchor�� flush���� ����
 *             -> ALTER TBS OFFLINE�� Commit Pending�� ����
 *                COMMIT���Ŀ��� ����� TBS���°� log anchor�� flush�Ǳ� ����
 *
 *  aTrans   - [IN] ���¸� �����Ϸ��� Transaction
 *  aTBSNode - [IN] ���¸� ������ Tablespace�� Node
 **********************************************************************/
IDE_RC sdptbSpaceDDL::alterTBSoffline( idvSQL*              aStatistics,
                                       void               * aTrans,
                                       sddTableSpaceNode  * aSpaceNode )
{
    UInt              sBeforeState;
    UInt              sAfterState;
    sctPendingOp    * sPendingOp;
    sddDataFileNode * sFileNode;
    UInt              i;

    ///////////////////////////////////////////////////////////////////////////
    //  (010) TBSNode�� X�� ȹ��
    //
    // Tablespace�� Offline���¿��� ������ ���� �ʴ´�.
    IDE_TEST( sctTableSpaceMgr::lockTBSNode(
                                   aTrans,
                                   & aSpaceNode->mHeader,
                                   ID_FALSE,   /* intent */
                                   ID_TRUE,    /* exclusive */
                                   SCT_VAL_ALTER_TBS_ONOFF) /* validation */
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (e-010) Tablespace���¿� ���� ����ó��
    IDE_TEST( sctTableSpaceMgr::checkError4AlterStatus(
                                     (sctTableSpaceNode*)aSpaceNode,
                                     SMI_TBS_OFFLINE  /* New State */ )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (020) Tablespace�� Backup���̶�� Backup����ñ��� ���
    IDE_TEST( sctTableSpaceMgr::wait4BackupAndBlockBackup( aStatistics,
                                                           (sctTableSpaceNode*)aSpaceNode,
                                                           SMI_TBS_SWITCHING_TO_OFFLINE )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (030) "TBSNode.Status := Offline" �� ���� �α�
    sBeforeState = aSpaceNode->mHeader.mState ;

    // �α��ϱ� ���� Backup�����ڿ��� ���ü� ��� ���� �ӽ� Flag�� ����
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_OFFLINE;
    sBeforeState &= ~SMI_TBS_SWITCHING_TO_ONLINE;

    sAfterState  = sBeforeState;
    sAfterState &= ~SMI_TBS_ONLINE;
    sAfterState |=  SMI_TBS_OFFLINE;

    IDE_TEST( smrUpdate::writeTBSAlterOnOff( aStatistics,
                                             aTrans,
                                             aSpaceNode->mHeader.mID,
                                             SCT_UPDATE_DRDB_ALTER_TBS_OFFLINE,
                                             sBeforeState,
                                             sAfterState,
                                             NULL )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (050) Instant Disk Aging �ǽ�
    //        - Aging �����߿��� ��� Ager ��ġȹ��
    /* xxxxxxxxxxxx
    IDE_TEST( smLayerCallback::doInstantAgingWithDiskTBS(
                  aStatistics,
                  aTrans,
                  aSpaceNode->mHeader.mID )
              != IDE_SUCCESS );
              */

    ///////////////////////////////////////////////////////////////////////////
    //  (060) all dirty pages flush in tablespace

    // invalidatePages ������ ���ü��� ������� �ʱ� ������
    // �����ϴ� ���ȿ��� �ش� ���̺����̽��� ���� DML��
    // ������� �ʾƾ��ϰ�(X-LOCK) DISKGC�� Block�Ǿ� �־��
    // �Ѵ�. �������� ������� ������ Flush-List�� ������
    // ������ �߻��Ѵ�.
    // Ʈ������� �Ϸ�ɶ� (commit or abort) disk GC�� unblock �Ѵ�.

    /* xxxxxxxxxxxxx
    smLayerCallback::blockDiskGC( aStatistics, aTrans );
    */

    //BUG-21392 table spabe offline ���Ŀ� �ش� table space�� ���ϴ� BCB���� buffer
    //�� ���� �ֽ��ϴ�.

    /* 1.replacement flush�� ���� secondary buffer�� pageout��� ��������
       �߰� �߻��Ҽ� �־� flushPageȣ�� 3.���� pageOut ���� */
    IDE_TEST( sdsBufferMgr::flushPagesInRange( aStatistics,
                                               aSpaceNode->mHeader.mID,/*aSpaceID*/
                                               0,                      /*StartPID*/
                                               SD_MAX_PAGE_COUNT - 1 )
              != IDE_SUCCESS );
    /* 2.buffer pool�� dirtypage�� �ֽ� �̹Ƿ� 2nd->bufferpool�� ������ ���� */
    IDE_TEST( sdbBufferMgr::pageOutInRange( aStatistics,
                                            aSpaceNode->mHeader.mID,
                                            0,
                                            SD_MAX_PAGE_COUNT - 1 )
              != IDE_SUCCESS );
    /* 3.replacement flush�� ���� secondary buffer�� pageout��� �������� �����Ҽ��� �ֽ��ϴ�.*/
    IDE_TEST( sdsBufferMgr::pageOutInRange( aStatistics,
                                            aSpaceNode->mHeader.mID,/*aSpaceID*/
                                            0,                      /*StartPID*/
                                            SD_MAX_PAGE_COUNT - 1 )
              != IDE_SUCCESS );

    ///////////////////////////////////////////////////////////////////////////
    //  (070) Commit Pending���
    //
    // Transaction Commit�ÿ� ������ Pending Operation���
    IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                              aTrans,
                              aSpaceNode->mHeader.mID,
                              ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                              SCT_POP_ALTER_TBS_OFFLINE,
                              & sPendingOp )
              != IDE_SUCCESS );

    // Commit�� sctTableSpaceMgr::executePendingOperation����
    // ������ Pending�Լ� ����
    sPendingOp->mPendingOpFunc = sdptbSpaceDDL::alterOfflineCommitPending;
    sPendingOp->mNewTBSState   = sAfterState;

    ///////////////////////////////////////////////////////////////////////////
    //  (080) DBF Online �α�
    //  (090) DBF Commit Pending���

    // Transaction Commit�ÿ� ������ DBFNode�� ���¸�
    // Offline���� �����ϴ� Pending Operation ���
    for (i=0; i < aSpaceNode->mNewFileID ; i++ )
    {
        sFileNode = aSpaceNode->mFileNodeArr[i] ;

        if( sFileNode == NULL)
        {
            continue;
        }

        sBeforeState = sFileNode->mState ;

        sAfterState  = sBeforeState;
        sAfterState &= ~SMI_FILE_ONLINE;
        sAfterState |=  SMI_FILE_OFFLINE;

        IDE_TEST( smrUpdate::writeDiskDBFAlterOnOff(
                                          aStatistics,
                                          aTrans,
                                          aSpaceNode->mHeader.mID,
                                          sFileNode->mID,
                                          SCT_UPDATE_DRDB_ALTER_DBF_OFFLINE,
                                          sBeforeState,
                                          sAfterState ) != IDE_SUCCESS );

        IDE_TEST( sddDataFile::addPendingOperation(
                          aTrans,
                          sFileNode,
                          ID_TRUE,        /* Pending ���� ���� ���� : Commit �� */
                          SCT_POP_ALTER_DBF_OFFLINE,
                          &sPendingOp ) != IDE_SUCCESS );

        sPendingOp->mNewDBFState   = sAfterState;
        sPendingOp->mPendingOpFunc = NULL; // pending �� ó���� �Լ��� ����.

        // PRJ-1548 User Memory Tablespace
        // TBS Node�� X ����� ȹ���ϱ� ������ DBF Node�� X �����
        // ȹ���� �ʿ䰡 ����.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterOnlineCommitPending(
                                          idvSQL            * aStatistics,
                                          sctTableSpaceNode * aSpaceNode,
                                          sctPendingOp      * aPendingOp )
{
    sdpActOnlineArgs  sActionArgs;

    // ���� ������ Tablespace�� �׻� Disk Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode->mID )
                == ID_TRUE );

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� ������ �ȵȴ�.
    IDE_ASSERT( ( aSpaceNode->mState & SMI_TBS_SWITCHING_TO_ONLINE )
                  == SMI_TBS_SWITCHING_TO_ONLINE );

    /////////////////////////////////////////////////////////////////////
    // (010) aSpaceNode.Status := ONLINE
    aSpaceNode->mState = aPendingOp->mNewTBSState;

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� ������ �ȵȴ�.
    IDE_ASSERT( ( aSpaceNode->mState & SMI_TBS_SWITCHING_TO_ONLINE )
                != SMI_TBS_SWITCHING_TO_ONLINE );

    if ( smrRecoveryMgr::isRestart() == ID_FALSE )
    {
        ///////////////////////////////////////////////////////////////////////////
        //  (020) Table�� Runtime���� �ʱ�ȭ
        //  (030) �ش� TBS�� ���� ��� Table�� ���� Index Header Rebuilding �ǽ�
        //        TBS ���¸� ONLINE���� ������ �Ŀ� Index Header Rebuilding��
        //        �����Ͽ��� ����Ÿ���Ͽ� Read�� ������ �� �ִ�.

        sActionArgs.mTrans = NULL;

        IDE_TEST( smLayerCallback::alterTBSOnline4Tables( aStatistics,
                                                          (void*)&sActionArgs,
                                                          aSpaceNode->mID )
                  != IDE_SUCCESS );

        /////////////////////////////////////////////////////////////////////
        //  (040) flush aSpaceNode to loganchor
        IDE_TEST( smrRecoveryMgr::updateTBSNodeToAnchor( aSpaceNode )
                != IDE_SUCCESS );
    }
    else
    {
        // restart recovery�ÿ��� ���¸� �����Ѵ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  PROJ-1548 User Memory Tablespace
 *
 *  Tablespace�� OFFLINE��Ų Tx�� Commit�Ǿ��� �� �Ҹ��� Pending�Լ�
 *
 *  Tablespace�� ���õ� ��� ���ҽ��� �ݳ��Ѵ�.
 *  - ���� : Tablespace�� Lock������ �ٸ� Tx���� ����ϸ鼭
 *            ������ �� �ֱ� ������ �����ؼ��� �ȵȴ�.
 *
 *   [����] sctTableSpaceMgr::executePendingOperation ���� ȣ��ȴ�.
 *
 *  [ �˰��� ] ======================================================
 *     (c-010) TBSNode.Status := OFFLINE
 *     (c-020) Free All Index Header of TBS
 *     (c-030) Destroy/Free Runtime Info At Table Header
 *     (c-040) flush TBSNode to loganchor
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterOfflineCommitPending(
                          idvSQL            * aStatistics,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp )
{
    IDE_DASSERT( aSpaceNode != NULL );

    // ���� ������ Tablespace�� �׻� Disk Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode->mID )
                == ID_TRUE );

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� �־�� �Ѵ�.
    IDE_ASSERT( ( aSpaceNode->mState & SMI_TBS_SWITCHING_TO_OFFLINE )
                  == SMI_TBS_SWITCHING_TO_OFFLINE );

    /////////////////////////////////////////////////////////////////////
    // (c-010) TBSNode.Status := OFFLINE
    aSpaceNode->mState = aPendingOp->mNewTBSState;

    // SMI_TBS_SWITCHING_TO_OFFLINE �� �����Ǿ� ������ �ȵȴ�.
    IDE_ASSERT( ( aSpaceNode->mState & SMI_TBS_SWITCHING_TO_OFFLINE )
                != SMI_TBS_SWITCHING_TO_OFFLINE );

    if ( smrRecoveryMgr::isRestart() == ID_FALSE )
    {
        /////////////////////////////////////////////////////////////////////
        //  (c-020) Free All Index Header of TBS
        //  (c-030) Destroy/Free Runtime Info At Table Header
        IDE_TEST( smLayerCallback::alterTBSOffline4Tables( aStatistics,
                                                           aSpaceNode->mID )
                  != IDE_SUCCESS );

        /////////////////////////////////////////////////////////////////////
        //  (c-050) flush TBSNode to loganchor
        IDE_TEST( smrRecoveryMgr::updateTBSNodeToAnchor( aSpaceNode )
                  != IDE_SUCCESS );
    }
    else
    {
        // restart recovery �ÿ��� ���¸� �����Ѵ�.
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description:
 *
 *    ALTER TABLESPACE ADD DATAFILE�� ���� Bitmap TBS�� Commit Pending����
 *    �� �����Ѵ�.
 *
 *    �߰��� ����Ÿ���Ͽ� ���ؼ� SpaceCache�� MaxGGID�� Freeness Bit�� �ݿ��Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceNode  - [IN] ���̺����̽� ��� ������
 * aPendingOp  - [IN] Pending���� �ڷᱸ���� ���� ������
 *
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterAddFileCommitPending(
                          idvSQL            * /* aStatistics */,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp )
{
    UInt              sGGID;
    sdptbSpaceCache * sCache;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aPendingOp != NULL );

    /* Restart Recovery�ÿ��� ȣ����� �ʴ� Pending �����̴�.
     * �ֳ��ϸ�, Recovery ���Ŀ� �ٽ� SpaceCache�� refine ������ ���ؼ�
     * �ٽ� ���ϱ� �����̴�. */
    IDE_ASSERT( smrRecoveryMgr::isRestart() == ID_FALSE );

    sCache = (sdptbSpaceCache*)aPendingOp->mPendingOpParam;
    IDE_ASSERT( sCache != NULL );

    sGGID  = aPendingOp->mFileID;
    IDE_ASSERT( sGGID < SD_MAX_FID_COUNT );

    /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
    sdptbBit::atomicSetBit32( (UInt*)sCache->mFreenessOfGGs, sGGID );

    if ( sCache->mMaxGGID < sGGID )
    {
        sCache->mMaxGGID = sGGID;
    }

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description:
 *
 *    ALTER TABLESPACE DROP DATAFILE�� ���� Bitmap TBS�� Commit Pending����
 *    �� �����Ѵ�.
 *
 *    ���ŵ� ����Ÿ���Ͽ� ���ؼ� SpaceCache�� MaxGGID�� Freeness Bit�� �ݿ��Ѵ�.
 *
 * aStatistics - [IN] �������
 * aSpaceNode  - [IN] ���̺����̽� ��� ������
 * aPendingOp  - [IN] Pending���� �ڷᱸ���� ���� ������
 *
 ***********************************************************************/
IDE_RC sdptbSpaceDDL::alterDropFileCommitPending(
                          idvSQL            * /* aStatistics */,
                          sctTableSpaceNode * aSpaceNode,
                          sctPendingOp      * aPendingOp )
{
    UInt              sGGID;
    sdptbSpaceCache * sCache;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aPendingOp != NULL );

    /* Restart Recovery�ÿ��� ȣ����� �ʴ� Pending �����̴�.
     * �ֳ��ϸ�, Recovery ���Ŀ� �ٽ� SpaceCache�� refine ������ ���ؼ�
     * �ٽ� ���ϱ� �����̴�. */
    IDE_ASSERT( smrRecoveryMgr::isRestart() == ID_FALSE );

    // ���� ������ Tablespace�� �׻� Disk Tablespace���� �Ѵ�.
    IDE_ASSERT( sctTableSpaceMgr::isDiskTableSpace( aSpaceNode->mID )
                == ID_TRUE );

    sCache = (sdptbSpaceCache*)aPendingOp->mPendingOpParam;
    IDE_ASSERT( sCache != NULL );

    sGGID = aPendingOp->mFileID;
    IDE_ASSERT( sGGID < SD_MAX_FID_COUNT );

    //���� �������� �ִ� ������ �����ϴ°��̶�� max ggid�� ���������ʿ䰡 �ִ�.
    if(  sCache->mMaxGGID == sGGID )
    {
        sCache->mMaxGGID--;
    }

    /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
    sdptbBit::atomicClearBit32( (UInt*)sCache->mFreenessOfGGs, sGGID );

    //���� ���ݻ����ϴ������� ��Ʈ�� �������ִٸ� ��Ʈ�� ������ 0����
    //(�̰��� �����־���ϴ°��� �ƴϴ�. ������ ��Ʈ���˻��� �����������θ�
    //���Ǿ����Ƿ�..)
    if(  sCache->mGGIDHint == sGGID )
    {
        sCache->mGGIDHint = 0;
    }

    return IDE_SUCCESS;
}
