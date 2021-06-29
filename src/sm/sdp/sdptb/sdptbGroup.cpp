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
 * $Id: sdptbGroup.cpp 27228 2008-07-23 17:36:52Z newdaily $
 *
 * Bitmap based TBS���� Global Group( Space Header) �� Local Group��
 * �����ϱ� ���� �Լ����̴�.
 **********************************************************************/

#include <smErrorCode.h>
#include <sdptb.h>
#include <sctTableSpaceMgr.h>
#include <sdp.h>
#include <sdpReq.h>

/***********************************************************************
 * Description:
 *  ���̺����̽� ��忡 Space Cache�� �Ҵ��ϰ� �ʱ�ȭ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::allocAndInitSpaceCache( scSpaceID         aSpaceID,
                                           smiExtMgmtType    aExtMgmtType,
                                           smiSegMgmtType    aSegMgmtType,
                                           UInt              aExtPageCount )
{
    sdptbSpaceCache     * sSpaceCache;
    UInt                  sState = 0;

    IDE_ASSERT( aExtMgmtType == SMI_EXTENT_MGMT_BITMAP_TYPE);

    /* sdptbGroup_allocAndInitSpaceCache_malloc_SpaceCache.tc */
    IDU_FIT_POINT("sdptbGroup::allocAndInitSpaceCache::malloc::SpaceCache");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDP,
                                 ID_SIZEOF(sdptbSpaceCache),
                                 (void**)&sSpaceCache,
                                 IDU_MEM_FORCE )
              != IDE_SUCCESS );
    sState = 1;

    idlOS::memset( sSpaceCache, 0x00 , ID_SIZEOF(sdptbSpaceCache));

    /* Tablespace�� ID */
    sSpaceCache->mCommon.mSpaceID       = aSpaceID;

    /* Extent ���� ��� ���� */
    sSpaceCache->mCommon.mExtMgmtType   = aExtMgmtType;
    sSpaceCache->mCommon.mSegMgmtType   = aSegMgmtType;

    /* Extent�� ������ ���� ���� */
    sSpaceCache->mCommon.mPagesPerExt   = aExtPageCount;


    /* TBS ����Ȯ���� ���� Mutex*/
    IDE_ASSERT( sSpaceCache->mMutexForExtend.initialize(
                                        (SChar*)"FEBT_EXTEND_MUTEX",
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_NULL ) == IDE_SUCCESS );

    // Condition Variable �ʱ�ȭ
    IDE_TEST_RAISE( sSpaceCache->mCondVar.initialize((SChar *)"FEBT_EXTEND_COND") != IDE_SUCCESS,
                    error_cond_init );

    sSpaceCache->mWaitThr4Extend = 0;

    /* BUG-31608 [sm-disk-page] add datafile during DML
     * TBS Add Datafile�� ���� Mutex */
    IDE_ASSERT( sSpaceCache->mMutexForAddDataFile.initialize(
                                        (SChar*)"FEBT_ADD_DATAFILE_MUTEX",
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_NULL ) == IDE_SUCCESS );

    sSpaceCache->mArrIsFreeExtDir[ SDP_TSS_FREE_EXTDIR_LIST ] = ID_TRUE;
    sSpaceCache->mArrIsFreeExtDir[ SDP_UDS_FREE_EXTDIR_LIST ] = ID_TRUE;

    /* Tablespace ��忡 Space Cache ���� */
    sddDiskMgr::setSpaceCache( aSpaceID, sSpaceCache );

    if( sctTableSpaceMgr::isTempTableSpace( aSpaceID ) == ID_TRUE )
    {
        /* Extent Pool �ʱ�ȭ */
        IDE_TEST( sSpaceCache->mFreeExtPool.initialize(
                      IDU_MEM_SM_TBS_FREE_EXTENT_POOL,
                      ID_SIZEOF( scPageID ) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_cond_init );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondInit) );
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( iduMemMgr::free(sSpaceCache) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  ���̺����̽� ��忡�� Space Cache�� �޸𸮸� �����Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::destroySpaceCache( sctTableSpaceNode * aSpaceNode )
{
    sdptbSpaceCache  * sSpaceCache;

    /* Tablespace ���κ��� Space Cache Ptr ��ȯ */
    sSpaceCache = sddDiskMgr::getSpaceCache( aSpaceNode );
    IDE_ASSERT( sSpaceCache != NULL );

    if( sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) == ID_TRUE )
    {
        IDE_TEST( sSpaceCache->mFreeExtPool.destroy() != IDE_SUCCESS );
    }
    IDE_TEST_RAISE( sSpaceCache->mCondVar.destroy() != IDE_SUCCESS,
                    error_cond_destroy );

    /* TBS Extend Mutex�� �����Ѵ�. */
    IDE_ASSERT( sSpaceCache->mMutexForExtend.destroy() == IDE_SUCCESS );
    /* BUG-31608 [sm-disk-page] add datafile duringDML
     * AddDataFile�� �ϱ� ���� Mutex�� �����Ѵ�. */
    IDE_ASSERT( sSpaceCache->mMutexForAddDataFile.destroy() == IDE_SUCCESS );

    IDE_ASSERT( iduMemMgr::free( sSpaceCache ) == IDE_SUCCESS );

    /* �޸� ���������Ƿ� NULL ptr �����Ѵ�. */
    sddDiskMgr::setSpaceCache( (sddTableSpaceNode*)aSpaceNode, NULL );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_cond_destroy );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  TBS Ȯ�忡 ����  Mutex�� ȹ�� Ȥ�� ���
 ***********************************************************************/
IDE_RC sdptbGroup::prepareExtendFileOrWait( idvSQL            * aStatistics,
                                            sdptbSpaceCache   * aCache,
                                            idBool            * aDoExtend )
{
    IDE_ASSERT( aCache != NULL );
    IDE_ASSERT( aDoExtend != NULL );

    lockForExtend( aStatistics, aCache );

    // Extent ���� ���θ� �Ǵ��Ѵ�.
    if ( isOnExtend( aCache ) == ID_TRUE )
    {
        /* BUG-44834 Ư�� ��񿡼� sprious wakeup ������ �߻��ϹǷ� 
                     wakeup �Ŀ��� �ٽ� Ȯ�� �ϵ��� while������ üũ�Ѵ�.*/
        while ( isOnExtend( aCache ) == ID_TRUE )
        {
            // ExrExt Mutex�� ȹ���� ���¿��� ����ڸ� ������Ų��.
            aCache->mWaitThr4Extend++;

            IDE_TEST_RAISE( aCache->mCondVar.wait(&(aCache->mMutexForExtend))
                            != IDE_SUCCESS, error_cond_wait );

            aCache->mWaitThr4Extend--;
        }
        // �̹� Extend�� �Ϸ�Ǿ��� ������ Extent Ȯ����
        // ���̾� ���ʿ���� ������� Ž���� �����Ѵ�.
        *aDoExtend = ID_FALSE;
    }
    else
    {
        // ���� ���� Ȯ���� �����ϱ� ���� OnExtend�� On��Ų��.
        aCache->mOnExtend = ID_TRUE;
        *aDoExtend = ID_TRUE;
    }

    unlockForExtend( aCache );

    /* BUG-31608 [sm-disk-page] add datafile during DML */
    if ( *aDoExtend == ID_TRUE )
    {
        /* ������ Ȯ���� �����ϴ� Transaction�� ���, ( DoExtend == true )
         * ���ü� ó���� ���� AddDataFile�� ���´�. */
         lockForAddDataFile( aStatistics, aCache );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_cond_wait );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 *  TBS Ȯ�忡 ���� Mutex�� ������ �Բ� ����ϴ� Ʈ����� �����.
 *  (sdpst��ƾ ������)
 ***********************************************************************/
IDE_RC sdptbGroup::completeExtendFileAndWakeUp( idvSQL          * aStatistics,
                                                sdptbSpaceCache * aCache )
{
    IDE_ASSERT( aCache != NULL );
    IDE_ASSERT( isOnExtend(aCache) == ID_TRUE );

    /* BUG-31608 [sm-disk-page] add datafile during DML 
     * Ȯ���� �Ϸ��Ͽ��� ������ addDataFile�� ����Ѵ� */
    unlockForAddDataFile( aCache );

    lockForExtend( aStatistics, aCache );

    if ( aCache->mWaitThr4Extend > 0 )
    {
        // ��� Ʈ������� ��� �����.
        IDE_TEST_RAISE( aCache->mCondVar.broadcast() != IDE_SUCCESS,
                        error_cond_signal );
    }

    // Segment Ȯ�� ������ �Ϸ��Ͽ����� �����Ѵ�.
    aCache->mOnExtend = ID_FALSE;

    unlockForExtend( aCache );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_cond_signal );
    {
        IDE_SET( ideSetErrorCode(smERR_FATAL_ThrCondSignal) );
    }
    IDE_EXCEPTION_END;
 
    return IDE_FAILURE;
}


/***********************************************************************
 * Description:
 *  AddDataFile�� �ϱ� ���� Mutex�� ȹ���ϰ� ����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aCache         - [IN] ���̺����̽� ��Ÿ�� Cache*
 ***********************************************************************/
void sdptbGroup::prepareAddDataFile( idvSQL          * aStatistics,
                                     sdptbSpaceCache * aCache )
{
    IDE_DASSERT( aCache != NULL );

    lockForAddDataFile( aStatistics, aCache );
}
/***********************************************************************
 * Description:
 *  AddDataFile �۾��� �����Ͽ����Ƿ�, mutex�� �����Ѵ�.
 *
 * aCache         - [IN] ���̺����̽� ��Ÿ�� Cache*
 ***********************************************************************/
void sdptbGroup::completeAddDataFile( sdptbSpaceCache * aCache )
{
    IDE_DASSERT( aCache != NULL );

    unlockForAddDataFile( aCache );
}

/***********************************************************************
 *
 * Description:  GG(Global Group) �� LG(Local Group) header�鸦 �����Ѵ�.
 *
 * aStatistics    - [IN] �������
 * aStartInfo     - [IN] Mtx ���� ����
 * aSpaceID       - [IN] ���̺����̽� ID
 * aCache         - [IN] ���̺����̽� ��Ÿ�� Cache
 * aFileAttr      - [IN] ����Ÿ���� �Ӽ� Array
 * aFileAttrCount - [IN] ����Ÿ���� �Ӽ� ����
 ***********************************************************************/
IDE_RC sdptbGroup::makeMetaHeaders( idvSQL            * aStatistics,
                                    sdrMtxStartInfo   * aStartInfo,
                                    UInt                aSpaceID,
                                    sdptbSpaceCache   * aCache,
                                    smiDataFileAttr  ** aFileAttr,
                                    UInt                aFileAttrCount )
{
    UInt            i;
    UInt            sPageCnt;  //�Ҵ��� ����� ������ ����
    UInt            sLGCnt;    //local group ����
    sdrMtx          sMtx;
    UInt            sPagesPerExt    = aCache->mCommon.mPagesPerExt;
    UInt            sState  = 0;
    scPageID        sGGHdrPID;
    sdptbGGHdr    * sGGHdrPtr;
    UChar         * sPagePtr;
    UInt            sGGID;
    idBool          sIsExtraLG; // ��� ��Ʈ�� ä���������� extra LG�� �����ϴ���
    sctPendingOp  * sPendingOp;

    IDE_ASSERT( aCache              != NULL );
    IDE_ASSERT( aFileAttr           != NULL );
    IDE_ASSERT( aStartInfo          != NULL );
    IDE_ASSERT( aStartInfo->mTrans  != NULL );
    IDE_ASSERT( aFileAttrCount <= SD_MAX_FID_COUNT);

    for( i = 0 ; i < aFileAttrCount ; i++ )
    {
        IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                       &sMtx,
                                       aStartInfo,
                                       ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                       SM_DLOG_ATTR_DEFAULT )
                  != IDE_SUCCESS );
        sState = 1;

        /* BUG-24369 Undo Tablespace Reset�� DataFile�� CurrSize ��������
         *           Reset �ؾ���.
         * Create Tablespace/Alter Add DataFile/Reset�ÿ� makeMetaHeaders
         * �� ȣ��ȴ�. ó�� �����ϴ� ��쿡�� mCurrSize�� mInitSize�� �����ϰ�,
         * Reset �ÿ��� mCurrSize �� mInitSize �ٸ��� ������ ��������
         * MetaHeader�� mCurrSize�� �����Ѵ�. */
        sPageCnt = aFileAttr[i]->mCurrSize;
       
        sGGID     = aFileAttr[i]->mID;
   
        IDE_ASSERT( sGGID < SD_MAX_FID_COUNT ) ; 

        //������ ũ��� �ּ��� �ϳ��� extent�� �Ҵ���� �� �ִ� ������ �Ǿ��Ѵ�.
        //������ �����ϴ� ���̹Ƿ� ���� �䱸������ �������� ������ assert
        IDE_ASSERT( sPageCnt >=
                    (sPagesPerExt+SDPTB_GG_HDR_PAGE_CNT+ SDPTB_LG_HDR_PAGE_CNT) );

        sLGCnt = getNGroups( sPageCnt,
                             aCache ,
                             &sIsExtraLG );

        IDE_ASSERT( ( 0 < sLGCnt ) && ( sLGCnt <= SDPTB_LG_CNT_MAX ) );

        /*********************************************
         * GG header�� �����.
         *********************************************/
        sGGHdrPID = SDPTB_GLOBAL_GROUP_HEADER_PID( sGGID ); // GGID

        IDE_TEST( sdpPhyPage::create( aStatistics,
                                      aSpaceID,
                                      sGGHdrPID,
                                      NULL,    /* Parent Info */
                                      0,       /* Page Insertable State */
                                      SDP_PAGE_FEBT_GGHDR,
                                      SM_NULL_OID, // TBS
                                      SM_NULL_INDEX_ID,
                                      &sMtx,   /* BM Create Mtx */
                                      &sMtx,   /* Init Page Mtx */
                                      &sPagePtr) 
                  != IDE_SUCCESS );

        IDE_TEST( sdpPhyPage::logAndInitLogicalHdr(
                                          (sdpPhyPageHdr  *)sPagePtr,
                                          ID_SIZEOF(sdptbGGHdr),
                                          &sMtx,
                                          (UChar**)&sGGHdrPtr )
                  != IDE_SUCCESS );

        IDE_ASSERT( sGGHdrPtr != NULL);

        /* GG header�� ������ ������ ä���. & LOGGING */
        IDE_TEST( logAndInitGGHdrPage( &sMtx,
                                       aSpaceID,
                                       sGGHdrPtr,
                                       sGGID,
                                       sPagesPerExt,
                                       sLGCnt,
                                       sPageCnt,
                                       sIsExtraLG )
                  != IDE_SUCCESS );

        /*********************************************
         * LG header���� �����.
         *********************************************/
        IDE_TEST( makeNewLGHdrs( aStatistics,
                                 &sMtx,
                                 aSpaceID,
                                 sGGHdrPtr,
                                 0,         //aStartLGID,
                                 sLGCnt,    //aLGCntOfGG,
                                 sGGID,     //aGGID
                                 sPagesPerExt,
                                 sPageCnt)  
                  != IDE_SUCCESS );

        /* To Fix BUG-23874 [AT-F5 ART] alter tablespace add datafile ��
         * ���� ������ �ȵǴ� �� ����.
         *
         * ������ ���Ͽ� ���� ���뵵�� SpaceNode�� �ݿ��Ҷ���
         * Ʈ����� Commit Pending���� ó���ؾ� �Ѵ�. */
        IDE_TEST( sctTableSpaceMgr::addPendingOperation(
                                                  sMtx.mTrans,
                                                  aSpaceID,
                                                  ID_TRUE, /* Pending ���� ���� ���� : Commit �� */
                                                  SCT_POP_UPDATE_SPACECACHE,
                                                  &sPendingOp ) 
                  != IDE_SUCCESS );

        sPendingOp->mPendingOpFunc  = sdptbSpaceDDL::alterAddFileCommitPending;
        sPendingOp->mFileID         = sGGID;
        sPendingOp->mPendingOpParam = (void*)aCache;

        sState = 0;
        IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );
    } // end of for

    aCache->mGGIDHint = 0 ;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );

    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  Ondemand�� ���� ũ�⸦ �ø��� ��Ÿ����� �����
 ***********************************************************************/
IDE_RC sdptbGroup::makeMetaHeadersForAutoExtend(
                                              idvSQL           *aStatistics,
                                              sdrMtxStartInfo  *aStartInfo,
                                              UInt              aSpaceID,
                                              sdptbSpaceCache  *aCache,
                                              UInt              aNeededPageCnt )
{
    sdrMtx              sMtx;
    sddDataFileNode   * sFileNode = NULL;
    sddTableSpaceNode * sSpaceNode= NULL;
    UInt                sState=0;
    UInt                sOldLGCnt;
    UInt                sNewLGCnt;
    idBool              sDummy;
    UInt                sPageCntOld;

    IDE_ASSERT( aCache != NULL );
    IDE_ASSERT( aStartInfo!=NULL );

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   aStartInfo,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
                != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( aSpaceID,
                                                        (void**)&sSpaceNode )
              != IDE_SUCCESS );
    IDE_ASSERT( sSpaceNode != NULL );

    //Ȯ���� ������ �������� ������ ã�´�. (void return)
    sddDiskMgr::getExtendableSmallestFileNode( sSpaceNode,
                                               &sFileNode );
    /*
     * ���� sFileNode�� NULL �̶�� ����Ȯ���� �Ұ����� �����̴�.
     */
    IDE_TEST_RAISE( sFileNode == NULL, error_not_enough_space );

    //������Ȯ�� ������ ������������ �����صд�.
    sPageCntOld = sFileNode->mCurrSize;

    sOldLGCnt = getNGroups( sFileNode->mCurrSize,
                            aCache,
                            &sDummy);

    sNewLGCnt = getNGroups( sFileNode->mCurrSize + aNeededPageCnt,
                            aCache,
                            &sDummy);

    /*
     *( aNeededPageCnt / sPagesPerExt )�� ��û�� extent�� �����̴�.
     * ������ Ȯ��Ǿ����� LG ������� ������־���Ѵٸ� �Ʒ������� ���̵� ��
     * �ִ�.
     */


    if ( sOldLGCnt < sNewLGCnt )
    {
        //���Ӱ� ��������� LG�� ������ŭ LG����� �������Ѵ�.
        aNeededPageCnt +=  SDPTB_LG_HDR_PAGE_CNT*( sOldLGCnt - sNewLGCnt ) ;
    }

    /*
     * ���� page layer���� �ʿ��� �������� ������ ��� ��������.
     * �״������� sdd�� �� ũ���� ����Ȯ���� ��û�Ѵ�.
     *
     * getSmallestFileNode��ƾ���� "�̹�"  extend mode,
     * mNextSize,  mMaxSize���� ����Ͽ���.�׷��Ƿ� ���⼭�� �ش�������
     * ���������� Ȯ�常�ϸ� �ȴ�.
     */
    IDE_TEST( sddDiskMgr::extendDataFileFEBT(
                                          aStatistics,
                                          sdrMiniTrans::getTrans( &sMtx ),
                                          aSpaceID,
                                          sFileNode )
                != IDE_SUCCESS );

    /*
     * sFileNode�� ���������� ������� ��������� �Ϻ��Ҷ� ������ �ڵ尡 �����.
     * ���⼭�� ���������� ���� ũ�⿡ ���� �ʿ��� LG����� �����ϰ�,���Ӱ�
     * �����.
     */
    IDE_TEST( resizeGG( aStatistics,
                        &sMtx,
                        aSpaceID,
                        sFileNode->mID,         //aGGID
                        sFileNode->mCurrSize )  //������Ȯ���ũ����
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    /*
     * ���� Ȯ���� �ߴٸ� cache�� FID��Ʈ�� ���ش�.
     */
    if ( sFileNode->mCurrSize > sPageCntOld )
    {
        /* BUG-47666 mFreenessOfGGs�� ���ü� ��� �ʿ��մϴ�. */
        sdptbBit::atomicSetBit32( (UInt*)aCache->mFreenessOfGGs, sFileNode->mID );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_not_enough_space );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NOT_ENOUGH_SPACE,
                                  sSpaceNode->mHeader.mName ));

        /* BUG-40980 : AUTOEXTEND OFF���¿��� TBS max size�� �����Ͽ� extend �Ұ���
         *             error �޽����� altibase_sm.log���� ����Ѵ�. */
        if ( aSpaceID != SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO )
        {
            ideLog::log( IDE_SM_0, 
                         "The tablespace does not have enough free space ( TBS Name :<%s> ).",
                         sSpaceNode->mHeader.mName );
        }
    }

    IDE_EXCEPTION_END;

    if ( sState == 1 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  ���������� ������ ���������������� ���� resize�� �����Ѵ�.
 *  ���������� ������ ���ڷ� ���� GG�� ����ϸ� ���� ��´�.
 * 
 *  HWM�� ���� �˻簡 �������¿��� �� �Լ��� ȣ��ȴ�.
 *  �� �Լ��� ��Ÿ������� �����ϰų� ���� �����.
 ***********************************************************************/
IDE_RC sdptbGroup::resizeGG( idvSQL             * aStatistics,
                             sdrMtx             * aMtx,
                             scSpaceID            aSpaceID,
                             UInt                 aGGID,
                             UInt                 aNewPageCnt )
{
    UChar             * sPagePtr;
    idBool              sDummy;
    sdptbGGHdr        * sGGHdr;
    scPageID            sPID;

    IDE_ASSERT( aMtx != NULL );

    sPID = SD_CREATE_PID( aGGID, 0 );

    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          sPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    sGGHdr = getGGHdr(sPagePtr);

    IDE_TEST( resizeGGCore( aStatistics,
                            aMtx,
                            aSpaceID,
                            sGGHdr,
                            aNewPageCnt )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 *  GG ptr�� �޾Ƽ� ó���Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::resizeGGCore( idvSQL         * aStatistics,
                                 sdrMtx         * aMtx,
                                 scSpaceID        aSpaceID,
                                 sdptbGGHdr     * aGGHdr,
                                 UInt             aNewPageCnt )
{
    sdptbSpaceCache   * sCache;
    UInt                sPagesPerExt;
    idBool              sHasExtraBefore = ID_FALSE;
    UInt                sExtCntOfLastLGOld;
    UInt                sExtCntOfLastLGNew;
    UInt                sOldPageCnt; //Ȯ������ ����ũ�� ����(page����)
    UInt                sLGCntBefore;
    UInt                sLGCntNow;
    UInt                sExtCntBefore;
    UInt                sExtCntNow;
    scPageID            sAllocLGHdrPID;
    scPageID            sDeallocLGHdrPID;
    UInt                sReducedFreeExtCnt;
    UInt                sAddedFreeExtCnt;
    UInt                sFreeInLG=0;
    UInt                sLastLGID;
    UInt                sValidBits;
    UInt                sBitIdx;
    idBool              sPartialEGNow;

    sCache = sddDiskMgr::getSpaceCache( aSpaceID );

    IDE_ASSERT( sCache != NULL );

    sPagesPerExt = sCache->mCommon.mPagesPerExt;

    /*
     *   HWM ���� �۰� ������ ���ϼ��� ����.
     *   �տ��� üũ����. ���⼭�� assert��.
     */
    IDE_ASSERT( SD_MAKE_FPID( aGGHdr->mHWM ) < aNewPageCnt )

    sOldPageCnt = aGGHdr->mTotalPages;

    sLGCntBefore = getNGroups( sOldPageCnt,
                               sCache,
                               &sHasExtraBefore );

    //�翬�� �Ȱ��ƾ߸� ��.
    IDE_ASSERT( sLGCntBefore == aGGHdr->mLGCnt );

    sLGCntNow = getNGroups( aNewPageCnt,
                            sCache,
                            &sPartialEGNow );

    sExtCntBefore = getExtentCntByPageCnt( sCache,
                                           sOldPageCnt );

    sExtCntNow = getExtentCntByPageCnt( sCache,
                                        aNewPageCnt );

    // just for assert
    sExtCntOfLastLGOld = getExtCntOfLastLG( sOldPageCnt, sPagesPerExt );

    IDE_ASSERT( ( 0 < sExtCntOfLastLGOld ) &&
                ( sExtCntOfLastLGOld  <= sdptbGroup::nBitsPerLG() ) );

    sExtCntOfLastLGNew = getExtCntOfLastLG( aNewPageCnt, sPagesPerExt );

    IDE_ASSERT( ( 0 < sExtCntOfLastLGNew ) &&
                ( sExtCntOfLastLGNew <= sdptbGroup::nBitsPerLG() ) );

    IDE_TEST_CONT( sOldPageCnt == aNewPageCnt, return_anyway );

    /* BUG-29763 DB FileSize ����� Size�� �۾Ƽ� Extent�� ������ ���� ���
     * ������ �����մϴ�.
     *
     * Extent ������ ������ ���� ��� LGHdr�� ����� �ʿ䰡 ����. 
     * GGHdr�� mTotalPages �� �������ָ� �ȴ�. */
    IDE_TEST_CONT( sExtCntBefore == sExtCntNow, skip_resize_lghdr );

    IDE_ASSERT( sOldPageCnt != aNewPageCnt );

    /*  ���������������� �ٲ������� �������������� ���ٸ� ����̴�.
     *  �̶������ؾ��ϴ� ��Ÿ�������� GG�� ��ҵ������� ���� ������ LG�̴�.
     *
     *  ���������������� �ٲ������� �������������� �۴ٸ� Ȯ���̴�.
     *  �̶������ؾ��ϴ� ��Ÿ�������� GG�� ��������ũ�⿡���� ������ LG �׸���
     *  Ȯ��Ǹ鼭 ���Ӱ� ������� LG���̴�.
     *
     * [��ҹ� Ȯ��� ������ ����ؾ��ϴ� GG�ʵ�]
     *   - mLGCnt
     *   - mTotalPages
     *   - alloc LG�鿡���� LG free info ���� mLGFreeness.mFreeExts &
     *                                        mLGFreeness.mBits
     *   - dealloc LG�鿡���� LG free info ���� mLGFreeness.mBits
     *      (dealloc LG���� mFreeExts�� ��ȭ�� ���� ������ ȣ���� �ʿ����.)
     *
     * �������:
     *   - ���� ������̰� mbits�� mLGCnt������ŭ�� ���Ǿ����� ������
     *     mLGFreeness�� mBits�� �������ʿ䰡 ���ٰ� �����Ҽ�������..
     *     NO. �����ؾ��Ҽ��� �ִ�. ���� ������ LG���� HWM�պκ��� ����
     *     ������̾����� HWM�� �߷�������̴�.
     *
     *   - dealloc LG�鿡���� logAndModifyFreeExtsOfGG�� ȣ����ʿ�.
     *     ������ freeExts������ ������ ��ġ�� �����Ƿ�
     */
    if ( sOldPageCnt > aNewPageCnt ) // ���
    {
        IDE_ASSERT( sLGCntBefore >= sLGCntNow );

        /****** GG header log&����. *************************************/

        /*
         * (���� ext���� - �ٲ� ext����)  ��ŭ GG�� free���� ���ش�.
         * �پ�� free extent�� ������ ����Ѵ�.
         * HWM���Ĵ� ��� free�̹Ƿ�. �ܼ��ϰ� ����� �����ϴ�.
         */
        sReducedFreeExtCnt = (sExtCntBefore - sExtCntNow) ;

        //GG hdr���� alloc LG�� ����  Free�� �����Ų��.
        IDE_TEST( logAndModifyFreeExtsOfGG( aMtx,
                                            aGGHdr,
                                            -sReducedFreeExtCnt )
                  != IDE_SUCCESS );

        sLastLGID = sLGCntNow -1;

        //��Ұ� �Ǿ ������ ������ LG�� ��� extent�� �߷���������
        //resizeLGHdr�� ȣ���� �ʿ䰡 ����.

        if ( sPartialEGNow == ID_TRUE )
        {
            sAllocLGHdrPID = getAllocLGHdrPID( aGGHdr, sLastLGID );

            sDeallocLGHdrPID = getDeallocLGHdrPID( aGGHdr, sLastLGID);

            IDE_TEST( resizeLGHdr( aStatistics,
                                   aMtx,
                                   aSpaceID,
                                   sExtCntOfLastLGNew, //������ Ext Cnt
                                   sAllocLGHdrPID,
                                   sDeallocLGHdrPID,
                                   &sFreeInLG ) != IDE_SUCCESS );


            /*  (HWM�ٷ� ���� �������κ�����)
             * ��ҷ� ���Ͽ� LG hdr�� free�� ���ٸ�
             * GG�� mLGFreeness�� �����ؾ��Ѵ�
             */
            if ( sFreeInLG == 0 )
            {

                IDE_TEST( logAndSetLGFNBitsOfGG( aMtx,
                                                 aGGHdr,
                                                 sLastLGID,
                                                 0 ) //�ش��Ʈ�� 0���� ��Ʈ�Ѵ�
                          != IDE_SUCCESS );

            }
            else /* sFreeInLG  != 0 */
            {
                /* nothing to do */
            }
        }
        else
        {
            /*
             * nothing to do
             * partial EG�� ���ٸ�, LG�� ������ �ʿ䰡 ���� EG�� LGFN��
             * �������ʿ��ϴ�.
             */
        }


    }
    else         // if ( sOldPageCnt < aNewPageCnt )    //Ȯ��
    {
        //LG������ �� �۾������� ����. ������ Ȯ�����̹Ƿ�.
        IDE_ASSERT( sLGCntBefore <= sLGCntNow );

        //Ȯ���ε� LGCnt�� ���ٴ°��� ������ extra LG�� �־��ٴ� ���̴�.
        if ( sLGCntBefore == sLGCntNow )
        {
          IDE_ASSERT ( sHasExtraBefore == ID_TRUE );// MUST!!!
        }

        if ( sHasExtraBefore == ID_TRUE )
        {
            sLastLGID = aGGHdr->mLGCnt -1;  //������ ������ LGID

            //������ extra LG�� �����ߴٸ� ���� Ȯ�����̹Ƿ� �� LG�� �����Ѵ�.
            sAllocLGHdrPID = getAllocLGHdrPID(aGGHdr, sLastLGID);

            sDeallocLGHdrPID =getDeallocLGHdrPID( aGGHdr,sLastLGID);

            if ( sLGCntBefore == sLGCntNow )
            {
                sValidBits =  sExtCntOfLastLGNew;
            }
            else    //if ( sLGCntBefore < sLGCntNow )
            {
                sValidBits = sdptbGroup::nBitsPerLG();
            }

            IDE_TEST( resizeLGHdr( aStatistics,
                                   aMtx,
                                   aSpaceID,
                                   sValidBits,
                                   sAllocLGHdrPID,
                                   sDeallocLGHdrPID,
                                   &sFreeInLG )
                        != IDE_SUCCESS );
            /*
             * ���� Ȯ�忡 �����ߴٸ�~~~
             * GG���� LGFreeness��Ʈ�� ���־�� �Ѵ�.
             */
            if ( sFreeInLG > 0 )
            {

                sBitIdx = SDPTB_GET_LGID_BY_PID( sAllocLGHdrPID,
                                                 sPagesPerExt );

                IDE_TEST( logAndSetLGFNBitsOfGG( aMtx,
                                                 aGGHdr,
                                                 sBitIdx,
                                                 1 ) //�ش�LG���� �Ҵ�޵���~
                            != IDE_SUCCESS );
            }
            else
            {
                // sFreeInLG�� 0�� ���� �߻����� ����.( Ȯ�����̹Ƿ�)
            }
        }

        //Ȯ���̱�� �ϳ� �� ��ȭ�� ������ ������ LG�� �����ٸ� �� ������
        //�� �ʿ䰡 ����.
        if ( sLGCntBefore < sLGCntNow )
        {
            IDE_TEST( makeNewLGHdrs( aStatistics,
                                     aMtx,
                                     aSpaceID,
                                     aGGHdr,
                                     aGGHdr->mLGCnt,  //aStartLGID,
                                     sLGCntNow,       //aLGCntOfGG,
                                     aGGHdr->mGGID,
                                     sPagesPerExt,
                                     aNewPageCnt) != IDE_SUCCESS );

        }

        sAddedFreeExtCnt = sExtCntNow - sExtCntBefore;

        //GG hdr���� alloc LG�� ����  Free�� �����Ų��.
        IDE_TEST( logAndModifyFreeExtsOfGG( aMtx,
                                            aGGHdr,
                                            sAddedFreeExtCnt )
                  != IDE_SUCCESS );

    }

    IDE_EXCEPTION_CONT( skip_resize_lghdr );

    IDE_TEST( logAndSetPagesOfGG( aMtx,
                                  aGGHdr,
                                  aNewPageCnt )
              != IDE_SUCCESS );

    if ( sLGCntBefore != sLGCntNow )
    {
        IDE_TEST( logAndSetLGCntOfGG( aMtx,
                                      aGGHdr,
                                      sLGCntNow )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_EXCEPTION_CONT( return_anyway );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description:
 *  ���ο� LG header�� �����.
 *  aStartLGID �κ��� aEndLGID��  �����.
 * 
 *  aStartLGID : ������ LG ID�� ����
 *  aLGCntOfGG : GG������ LG count (��, GG�� mLGCnt�� ���� ����)
 ***********************************************************************/
IDE_RC sdptbGroup::makeNewLGHdrs( idvSQL          * aStatistics,
                                  sdrMtx          * aMtx,
                                  UInt              aSpaceID,
                                  sdptbGGHdr      * aGGHdrPtr,
                                  UInt              aStartLGID,
                                  UInt              aLGCntOfGG,
                                  sdFileID          aFID,
                                  UInt              aPagesPerExt,
                                  UInt              aPageCntOfGG )
{
    UInt                    sLGID;
    UInt                    sValidBits;

    IDE_ASSERT( aMtx != NULL);

    sLGID = aStartLGID;

    for(  ; sLGID < aLGCntOfGG ; sLGID++ )
    {

        if ( sLGID < ( aLGCntOfGG - 1 ) )
        {
            sValidBits = sdptbGroup::nBitsPerLG();
        }
        else //������ LG group�̶��.
        {
            sValidBits = getExtCntOfLastLG( aPageCntOfGG, aPagesPerExt );
        }

        IDE_TEST( logAndInitLGHdrPage( aStatistics,
                                       aMtx,
                                       aSpaceID,
                                       aGGHdrPtr,
                                       aFID,
                                       sLGID,
                                       SDPTB_ALLOC_LG,
                                       sValidBits,
                                       aPagesPerExt )
                  != IDE_SUCCESS );

        IDE_TEST( logAndInitLGHdrPage( aStatistics,
                                       aMtx,
                                       aSpaceID,
                                       aGGHdrPtr,
                                       aFID,
                                       sLGID,
                                       SDPTB_DEALLOC_LG,
                                       sValidBits,
                                       aPagesPerExt )
                  != IDE_SUCCESS );

        if ( sValidBits > 0 )
        {

            IDE_TEST( logAndSetLGFNBitsOfGG( aMtx,
                                             aGGHdrPtr,
                                             sLGID,  //bit index~
                                             1 ) //�ش�LG���� �Ҵ�޵���1��Ʈ
                      != IDE_SUCCESS );
        }
        else
        {
            /* nothing to do */
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  LG hdeaer �ϳ��� �����ϴ� �Լ�
 *  Ȯ��� ��� ��ο� ���Ǿ����� �ִ�.
 * 
 *  resizeLG�� �Ҷ� LG���� ������ �ʵ�
 * 
 *  for alloc LG
 *     - mValidBits
 *     - mFree
 *     - mBitmap : 0���� ä��
 * 
 *  for dealloc LG
 *     - mValidBits
 *     //- mFree ���ʿ�. free�� ���� �����Ƿ�.
 *     - mBitmap : 1�� ä��
 ***********************************************************************/
IDE_RC sdptbGroup::resizeLGHdr( idvSQL     * aStatistics,
                                sdrMtx     * aMtx,
                                scSpaceID    aSpaceID,
                                ULong        aValidBitsNew, //������ Ext Cnt
                                scPageID     aAllocLGPID,
                                scPageID     aDeallocLGPID,
                                UInt       * aFreeInLG )
{
    UChar              * sPagePtr;
    idBool               sDummy;
    sdptbLGHdr         * sLGHdr;
    UInt                 sFreeExts;
    SInt                 sDifference;
    sdptbData4InitLGHdr  sData4InitLGHdr;

    IDE_ASSERT( ( 0 < aValidBitsNew ) &&
                ( aValidBitsNew  <= sdptbGroup::nBitsPerLG() ) );

    /*
     * alloc LG header���� log&����.
     */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aAllocLGPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    sLGHdr = getLGHdr( sPagePtr );

    sDifference = aValidBitsNew - sLGHdr->mValidBits;

    //����ũ�Ⱑ �ٲ��� �ʾҴµ� ȣ��Ǿ��ٸ� ����.
    IDE_ASSERT( sDifference != 0 );

    /*
     * sFreeExts = ���� free +  ( ���� LG�� ext���� - ���� ext���� )
     */
    sFreeExts = sLGHdr->mFree + sDifference;

    /*
     * ������ LG�� �����Ҵ�Ǿ��� �ְ�, HWM�� �� LG ������ page��
     * ����Ű�� ������  sFreeExts��  0�� �ɼ��� �ִ�.
     */
    IDE_ASSERT( sFreeExts <= sdptbGroup::nBitsPerLG() );

    IDE_TEST( logAndSetFreeOfLG( aMtx,
                                 sLGHdr,
                                 sFreeExts ) != IDE_SUCCESS );

     /*
      * ���� Ȯ���̶�� ��Ʈ���� �����Ѵ�.
      */
    if ( sDifference > 0 ) //Ȯ���̶�� ��
    {
        sData4InitLGHdr.mBitVal   = 0;
        sData4InitLGHdr.mStartIdx = sLGHdr->mValidBits; //���� LG�� valid bits�� ���
        sData4InitLGHdr.mCount    = sDifference;

        IDE_ASSERT( (sLGHdr->mValidBits + sDifference ) <=
                       sdptbGroup::nBitsPerLG());

        initBitmapOfLG( (UChar*)sLGHdr,
                        0,                  //aBitVal,
                        sLGHdr->mValidBits, //aStartIdx,
                        sDifference);

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sLGHdr,
                                             &sData4InitLGHdr,
                                             ID_SIZEOF( sData4InitLGHdr ),
                                             SDR_SDPTB_INIT_LGHDR_PAGE )
                  != IDE_SUCCESS );
    }

    //������ writeLogRec�� �Ѵ����� ValidBits�� �ٲܰ�.
    IDE_TEST( logAndSetValidBitsOfLG( aMtx,
                                      sLGHdr,
                                      aValidBitsNew )
              != IDE_SUCCESS );

    /*
     * output���� �翬�� alloc LG�� free�̾�� �Ѵ�.
     */

    *aFreeInLG = sLGHdr->mFree;

    /*
     * dealloc LG header log&����.
     */
    IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics,
                                          aSpaceID,
                                          aDeallocLGPID,
                                          SDB_X_LATCH,
                                          SDB_WAIT_NORMAL,
                                          SDB_SINGLE_PAGE_READ,
                                          aMtx,
                                          &sPagePtr,
                                          &sDummy,
                                          NULL /*IsCorruptPage*/ ) != IDE_SUCCESS );

    sLGHdr = getLGHdr(sPagePtr);

     /*
      * ���� Ȯ���̶�� ��Ʈ���� �����Ѵ�.
      */
    if ( sDifference > 0  ) //Ȯ���̶�� ��
    {
        sData4InitLGHdr.mBitVal   = 1;
        sData4InitLGHdr.mStartIdx = sLGHdr->mValidBits; //���� LG�� valid bits�� ���
        sData4InitLGHdr.mCount    = sDifference;

        IDE_ASSERT( (sLGHdr->mValidBits + sDifference ) <=
                      sdptbGroup::nBitsPerLG());

        initBitmapOfLG( (UChar*)sLGHdr,
                        1,                  //aBitVal,
                        sLGHdr->mValidBits, //aStartIdx,
                        sDifference);

        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sLGHdr,
                                             &sData4InitLGHdr,
                                             ID_SIZEOF( sData4InitLGHdr ),
                                             SDR_SDPTB_INIT_LGHDR_PAGE )
                  != IDE_SUCCESS );
    }
    else
    {
        /* nothing to do */
    }

    IDE_TEST( logAndSetValidBitsOfLG( aMtx,
                                      sLGHdr,
                                      aValidBitsNew ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  �Ѱ��� GG�� ���� LG�� ������ �����Ѵ�.
 *  ������ ũ�⸦ page������ �޾Ƽ� ��� LG�� ������ִ��� ����Ѵ�
 * 
 *  aPageCnt   : page������ ����ũ�� (�翬�� GG header���� ���Ե�ũ����)
 *  aIsExtraLG : �κ������� ����ϴ� LG�� �����ϴ³�
 ***********************************************************************/
UInt sdptbGroup::getNGroups( ULong            aPageCnt,
                             sdptbSpaceCache *aCache,
                             idBool          *aIsExtraLG )
{
    UInt sNum;
    ULong sLGSizeInByte;   //������ byte
    ULong sSizeInByte;     //������ byte
    ULong sRest;

    IDE_ASSERT( aCache != NULL);
    IDE_ASSERT( aIsExtraLG != NULL);

    aPageCnt -=  SDPTB_GG_HDR_PAGE_CNT;  //GG header�� �����ϰ� ����ؾ߸� �Ѵ�

    sSizeInByte = aPageCnt * SD_PAGE_SIZE;

    sLGSizeInByte = SDPTB_PAGES_PER_LG(aCache->mCommon.mPagesPerExt)
                      * SD_PAGE_SIZE ;

    sNum = sSizeInByte / sLGSizeInByte;

    sRest = sSizeInByte % sLGSizeInByte;

    //%�������� �������� �ִٰ� �ؼ� ������ LG������ ������Ű�� �ȵȴ�.
    //�ֳ��ϸ� �ش����� ���� �ѹ���Ʈ�� ���������� �ֱ� �����̴�.
    if ( (sRest/SD_PAGE_SIZE) >=
                (SDPTB_LG_HDR_PAGE_CNT + aCache->mCommon.mPagesPerExt) )
    {
        sNum++;
        *aIsExtraLG = ID_TRUE ;
    }
    else
    {
        *aIsExtraLG = ID_FALSE ;
    }
    return sNum;
}

/***********************************************************************
 * Description:
 *  GG��������� �α�ó���� �Ѵ�.
 *
 *  ���Լ��� TBS�� ���鶧 ȣ��Ǵ� sdptbGroup::makeMetaHeaders�� ���ؼ��� 
 *  ȣ��ȴ�. 
 ***********************************************************************/
IDE_RC sdptbGroup::logAndInitGGHdrPage( sdrMtx        * aMtx,
                                        UInt            aSpaceID,
                                        sdptbGGHdr    * aGGHdrPtr,
                                        sdptbGGID       aGGID,
                                        UInt            aPagesPerExt,
                                        UInt            aLGCnt,
                                        UInt            aPageCnt,
                                        idBool          aIsExtraLG )
{
    ULong       sLongVal;
    UInt        sVal;

    IDE_ASSERT( aMtx        != NULL);
    IDE_ASSERT( aGGHdrPtr   != NULL);

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mGGID,
                                         &aGGID,
                                         ID_SIZEOF( aGGID ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mPagesPerExt,
                                         &aPagesPerExt,
                                         ID_SIZEOF( aPagesPerExt ) )
              != IDE_SUCCESS );

    sVal =  SD_CREATE_PID( aGGID,0);

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mHWM,
                                         (scPageID*)&sVal,
                                         ID_SIZEOF( aGGHdrPtr->mHWM ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mLGCnt,
                                         &aLGCnt,
                                         ID_SIZEOF( aLGCnt ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mTotalPages,
                                         &aPageCnt,
                                         ID_SIZEOF( aPageCnt ) )
              != IDE_SUCCESS );

    sVal = SDPTB_ALLOC_LG_IDX_0;

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&aGGHdrPtr->mAllocLGIdx,
                                         &sVal,
                                         ID_SIZEOF( sVal ) )
              != IDE_SUCCESS );

    if ( aIsExtraLG == ID_TRUE ) //�Ϻ� ��Ʈ�� �������  LG�� �����ϴ°�
    {
        //����Ʈ�� ���� LG�� ��Ʈ�� + ������ LG�� ��Ʈ��
        sLongVal = ( sdptbGroup::nBitsPerLG() * (aLGCnt-1)) +
                   getExtCntOfLastLG( aPageCnt, aPagesPerExt );

    }
    else
    {
        //����Ʈ�� ���� LG�� ��Ʈ��
        sLongVal = sdptbGroup::nBitsPerLG() * aLGCnt;
    }

    /*
     * GG���� alloc LG������ �ʵ� �α�.
     */
    IDE_TEST( sdrMiniTrans::writeNBytes(
                                   aMtx,
                                   (UChar*)&aGGHdrPtr->mLGFreeness[0].mFreeExts,
                                   &sLongVal,
                                   ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    /*
     * alloc LG �α�.
     *
     */
    /*
     * LGFreeness������ ������ ���⼭ �����ʰ� makeNewLGHdrs���� �����Ѵ�.
     * ���⼭�� �ϴ� 0���θ� ��Ʈ�Ѵ�.
     */
    sLongVal = 0;
    IDE_TEST( sdrMiniTrans::writeNBytes(
                                    aMtx,
                                    (UChar*)&aGGHdrPtr->mLGFreeness[0].mBits[0],
                                    &sLongVal,
                                    ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                                aMtx,
                                (UChar*)&aGGHdrPtr->mLGFreeness[0].mBits[1],
                                &sLongVal,
                                ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    /*
     * GG���� dealloc LG������ �ʵ� �α�.
     * dealloc LG�� ó�����鶧 free extent�� �����Ƿ� ��� 0���� �����ϸ� �ȴ�.
     */
    IDE_TEST( sdrMiniTrans::writeNBytes(
                                  aMtx,
                                  (UChar*)&aGGHdrPtr->mLGFreeness[1].mFreeExts,
                                  &sLongVal,
                                  ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                                  aMtx,
                                  (UChar*)&aGGHdrPtr->mLGFreeness[1].mBits[0],
                                  &sLongVal,
                                  ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                                  aMtx,
                                  (UChar*)&aGGHdrPtr->mLGFreeness[1].mBits[1],
                                  &sLongVal,
                                  ID_SIZEOF( sLongVal ) ) != IDE_SUCCESS );

    /* PROJ-1704 Disk MVCC Renewal
     * Undo TBS�� ù��° FGH���� Free Extent Dir List�� �����Ѵ�.
     */
    if ( ( sctTableSpaceMgr::isUndoTableSpace( aSpaceID ) ) &&
         ( aGGID == 0 ) )
    {
        IDE_TEST( sdpSglPIDList::initList( 
                                &(aGGHdrPtr->mArrFreeExtDirList[ SDP_TSS_FREE_EXTDIR_LIST ] ),
                                aMtx )
                  != IDE_SUCCESS );

        IDE_TEST( sdpSglPIDList::initList( 
                                &(aGGHdrPtr->mArrFreeExtDirList[ SDP_UDS_FREE_EXTDIR_LIST ] ),
                                aMtx ) 
                  != IDE_SUCCESS );
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
 * Description:
 *  mLGFreeness�� mBits���� aBitIdx��° �ε������� aVal�� �����ϴ� �Լ��̴�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetLGFNBitsOfGG( sdrMtx           * aMtx,
                                          sdptbGGHdr       * aGGHdr,
                                          ULong              aBitIdx,
                                          UInt               aVal )
{
    ULong        sResult;

    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );
    IDE_ASSERT( aBitIdx < SDPTB_BITS_PER_ULONG*2 );
    IDE_ASSERT( (aVal == 1) || (aVal == 0) );

    if ( aVal == 1 )
    {
        sdptbBit::setBit( aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits, aBitIdx );
    }
    else
    {
        sdptbBit::clearBit( aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits, aBitIdx );
    }


    if ( aBitIdx < SDPTB_BITS_PER_ULONG )
    {
        sResult = aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[0];

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&(aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[0]),
                                             &sResult,
                                             ID_SIZEOF( sResult ) )
                  != IDE_SUCCESS );
    }
    else  //if ( aBitIdx >= SDPTB_BITS_PER_ULONG )
    {
        sResult = aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[1];

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&(aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[1]),
                                             &sResult,
                                             ID_SIZEOF( sResult ) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  LG��������� �α�ó���� �Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndInitLGHdrPage( idvSQL          *  aStatistics,
                                        sdrMtx          *  aMtx,
                                        UInt               aSpaceID,
                                        sdptbGGHdr      *  aGGHdrPtr,
                                        sdFileID           aFID,
                                        UInt               aLGID,
                                        sdptbLGType        aLGType,
                                        UInt               aValidBits,
                                        UInt               aPagesPerExt )

{
    UInt                    sLGHdrPID;
    UChar                  *sPagePtr;
    sdptbLGHdr             *sLGHdrPtr;
    UInt                    sStartPID;
    UInt                    sZero=0;
    sdptbData4InitLGHdr     sData4InitLGHdr;
    UInt                    sWhich;

    IDE_ASSERT( aMtx != NULL);
    IDE_ASSERT( aLGType < SDPTB_LGTYPE_CNT );

    if ( aLGType == SDPTB_ALLOC_LG )
    {
        sWhich = sdptbGroup::getAllocLGIdx(aGGHdrPtr);
    }
    else       // if ( aLGType == SDPTB_DEALLOC_LG )
    {
        sWhich = sdptbGroup::getDeallocLGIdx(aGGHdrPtr);
    }

    sLGHdrPID = SDPTB_LG_HDR_PID_FROM_LGID( aFID,
                                            aLGID,
                                            sWhich,
                                            aPagesPerExt );

    IDE_TEST( sdpPhyPage::create( aStatistics,
                                  aSpaceID,
                                  sLGHdrPID,
                                  NULL,    /* Parent Info */
                                  0,       /* Page Insertable State */
                                  SDP_PAGE_FEBT_LGHDR,
                                  SM_NULL_OID, // TBS
                                  SM_NULL_INDEX_ID,
                                  aMtx,    /* Create Page Mtx */
                                  aMtx,    /* Init Page Mtx */
                                  &sPagePtr) != IDE_SUCCESS );

    IDE_ASSERT( sPagePtr !=NULL );

    IDE_TEST( sdpPhyPage::logAndInitLogicalHdr( (sdpPhyPageHdr  *)sPagePtr,
                                                ID_SIZEOF(sdptbLGHdr),
                                                aMtx,
                                                (UChar**)&sLGHdrPtr )
              != IDE_SUCCESS );

    sStartPID = SD_CREATE_PID( aFID,
                               SDPTB_EXTENT_START_FPID_FROM_LGID( aLGID ,aPagesPerExt) );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sLGHdrPtr->mLGID,
                                         &aLGID,
                                         ID_SIZEOF( aLGID ) )
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sLGHdrPtr->mStartPID,
                                         &sStartPID,
                                         ID_SIZEOF( sStartPID ))
              != IDE_SUCCESS );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&sLGHdrPtr->mValidBits,
                                         &aValidBits,
                                         ID_SIZEOF( aValidBits ) )
              != IDE_SUCCESS );

    if ( aLGType == SDPTB_ALLOC_LG )
    {
        //alloc page LG�� ��� free�� �ʱⰪ�� �翬�� mValidBits�� ����.
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sLGHdrPtr->mFree,
                                             &aValidBits,
                                             ID_SIZEOF( aValidBits ) )
                  != IDE_SUCCESS );

        initBitmapOfLG( (UChar*)sLGHdrPtr,
                        0,         //aBitVal,
                        0,         //aStartIdx,
                        aValidBits);

        /*
         * alloc LG �ΰ��� valid�� ��Ʈ���� 0���� ����� ��ƾ�� �ʿ��ϴ�.
         */
        sData4InitLGHdr.mBitVal   = 0;
        sData4InitLGHdr.mStartIdx = 0;
        sData4InitLGHdr.mCount    = aValidBits;
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sLGHdrPtr,
                                             &sData4InitLGHdr,
                                             ID_SIZEOF(sData4InitLGHdr),
                                             SDR_SDPTB_INIT_LGHDR_PAGE )
                  != IDE_SUCCESS );

        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sLGHdrPtr->mHint,
                                             &sZero,
                                             ID_SIZEOF(sZero) ) != IDE_SUCCESS );
    }
    else          // if ( aLGType == SDPTB_DEALLOC_LG )
    {
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sLGHdrPtr->mFree,
                                             &sZero,
                                             ID_SIZEOF( sZero ) )
                  != IDE_SUCCESS );

        initBitmapOfLG( (UChar*)sLGHdrPtr,
                        1,         //aBitVal,
                        0,         //aStartIdx,
                        aValidBits);
        /*
         * dealloc LG �ΰ��� valid�� ��Ʈ���� 1�� ����� ��ƾ�� �ʿ��ϴ�.
         */
        sData4InitLGHdr.mBitVal   = 1;
        sData4InitLGHdr.mStartIdx = 0;
        sData4InitLGHdr.mCount    = aValidBits;
        IDE_TEST( sdrMiniTrans::writeLogRec( aMtx,
                                             (UChar*)sLGHdrPtr,
                                             &sData4InitLGHdr,
                                             ID_SIZEOF(sData4InitLGHdr),
                                             SDR_SDPTB_INIT_LGHDR_PAGE )
          != IDE_SUCCESS );

        /*
         * Dealloc LG �ΰ���  hint�� (�ִ� extent id +1 )�� �����ϵ��� �Ѵ�.
         */
        IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                             (UChar*)&sLGHdrPtr->mHint,
                                             &aValidBits,
                                             ID_SIZEOF(aValidBits) )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  TBS�� ��������� ���鼭 ��밡���� file�� ã�Ƽ� space cache�� �����ϰ�
 *  avail ��Ʈ�� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::doRefineSpaceCacheCore( sddTableSpaceNode * aSpaceNode )

{
    UInt                sMaxGGID=0;
    idBool              sIsFirstLoop= ID_TRUE;
    UInt                i;
    sddDataFileNode   * sFileNode;
    sdptbSpaceCache   * sCache;

    UChar             * sPagePtr;
    sdptbGGHdr        * sGGHdrPtr;
    idBool              sDummy;
    sctTableSpaceNode * sSpaceNode = (sctTableSpaceNode *)aSpaceNode;
    idBool              sInvalidTBS;

    IDE_ASSERT( aSpaceNode != NULL );
    IDE_ASSERT( aSpaceNode->mExtMgmtType == SMI_EXTENT_MGMT_BITMAP_TYPE );

    sInvalidTBS = sctTableSpaceMgr::hasState( sSpaceNode->mID,
                                              SCT_SS_INVALID_DISK_TBS );

    if ( ( sctTableSpaceMgr::isDiskTableSpace(sSpaceNode->mID) == ID_TRUE ) &&
         ( sInvalidTBS ==  ID_FALSE ) )
    {
        sCache = (sdptbSpaceCache *)aSpaceNode->mSpaceCache;
        IDE_ASSERT( sCache != NULL );
        
        idlOS::memset( sCache->mFreenessOfGGs, 0x00 , ID_SIZEOF(sCache->mFreenessOfGGs) );
        
        //TBS�� ��� ������ ���鼭 ��밡���� File�� ã������ ��Ʈ��Ʈ.
        for ( i=0 ; i < aSpaceNode->mNewFileID ; i++ )
        {
            sFileNode = aSpaceNode->mFileNodeArr[i] ;

            if ( sFileNode != NULL )
            {
                if ( SMI_FILE_STATE_IS_NOT_DROPPED( sFileNode->mState ) )
                {
                    /* ���࿡ GGid�� free extent�� ���ٸ� ��Ʈ���� ���� */
                    IDE_TEST( sdbBufferMgr::fixPageByPID( NULL,
                                                          sSpaceNode->mID,
                                                          SDPTB_GET_GGHDR_PID_BY_FID( sFileNode->mID),
                                                          (UChar**)&sPagePtr,
                                                          &sDummy )
                              != IDE_SUCCESS );

                    sGGHdrPtr = getGGHdr(sPagePtr);
                    
                    sMaxGGID = i ;

                    if ( sGGHdrPtr->mLGFreeness[ sGGHdrPtr->mAllocLGIdx ].mFreeExts == 0 )
                    {
                        IDE_TEST( sdbBufferMgr::unfixPage( NULL,
                                                           sPagePtr )
                                  != IDE_SUCCESS );

                        continue;
                    }
                }
                else
                {
                    // BUG-27329 CodeSonar::Uninitialized Variable (2)
                    continue;
                }

                // PROJ-1704 Disk MVCC Renewal
                // Undo TBS���� �ʿ��� Free ExtDir List ������ Ȯ���Ѵ�
                if ( ( sctTableSpaceMgr::isUndoTableSpace(sSpaceNode->mID) )&& 
                     ( sFileNode->mID == 0 ) )
                {
                    if ( sdpSglPIDList::getNodeCnt( 
                            &(sGGHdrPtr->mArrFreeExtDirList[ SDP_TSS_FREE_EXTDIR_LIST ])) > 0 )
                    {
                       sCache->mArrIsFreeExtDir[ SDP_TSS_FREE_EXTDIR_LIST ] = ID_TRUE;    
                    }
                    else
                    {
                        /* nothing to do */
                    }

                    if ( sdpSglPIDList::getNodeCnt( 
                            &(sGGHdrPtr->mArrFreeExtDirList[ SDP_UDS_FREE_EXTDIR_LIST ])) > 0 )
                    {
                        sCache->mArrIsFreeExtDir[ SDP_UDS_FREE_EXTDIR_LIST ] = ID_TRUE;    
                    }
                    else
                    {
                        /* nothing to do */
                    }
                }

                IDE_TEST( sdbBufferMgr::unfixPage( NULL,
                                                   sPagePtr )
                          != IDE_SUCCESS );

                /* BUG-47666 server start�����̶� ���ü� ��� �� �ʿ�� ������
                 *           atomic �Լ��� ���� ���� ���θ� Ȯ���ϱ� ���� ����*/
                IDE_ASSERT( sdptbBit::getBit( sCache->mFreenessOfGGs, i ) == ID_FALSE );

                sdptbBit::atomicSetBit32( (UInt*)sCache->mFreenessOfGGs, i );

                IDE_ASSERT( sdptbBit::getBit( sCache->mFreenessOfGGs, i ) == ID_TRUE );

                //�Ҵ��� ����� ù��° GG ID�� ��Ʈ�Ѵ�.
                if ( sIsFirstLoop == ID_TRUE )
                {
                    sCache->mGGIDHint = i;
                    sIsFirstLoop = ID_FALSE;
                }
                else
                {
                    /* nothing to do */
                }
            }
        }

        //�Ҵ簡���� ����ū GG ID�� ��Ʈ�Ѵ�.
        //���帶���� ������ DROP�Ȱ�쵵 �������̹Ƿ� �̷��� �Ѵ�.
        sCache->mMaxGGID = sMaxGGID;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/////////////////////////////////////////////////////////////
//     GG       Logging ���� �Լ���.(����)
/////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 *  GG�� HWM �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetHWMOfGG( sdrMtx      * aMtx,
                                     sdptbGGHdr  * aGGHdr,
                                     UInt          aHWM )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aGGHdr->mHWM),
                                         &aHWM,
                                         ID_SIZEOF(aHWM) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mLGCnt �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetLGCntOfGG( sdrMtx      * aMtx,
                                       sdptbGGHdr  * aGGHdr,
                                       UInt          aGroups )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aGGHdr->mLGCnt),
                                         &aGroups,
                                         ID_SIZEOF(aGroups) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mTotalPages �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetPagesOfGG( sdrMtx     *  aMtx,
                                       sdptbGGHdr  * aGGHdr,
                                       UInt          aPages )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aGGHdr->mTotalPages),
                                         &aPages,
                                         ID_SIZEOF(aPages) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mAllocLGIdx �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetLGTypeOfGG( sdrMtx       * aMtx,
                                        sdptbGGHdr   * aGGHdr,
                                        UInt           aLGType )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aGGHdr->mAllocLGIdx),
                                         &aLGType,
                                         ID_SIZEOF(aLGType) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mFree �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 *  aVal��ŭ ���� ��ȭ��Ų��. aVal �� ������� ��� �����ϴ�.
 *  ���Լ��� ���� Ȱ��ȭ�Ǿ��ִ� alloc LG�� ���� ��ȭ��Ų��.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndModifyFreeExtsOfGG( sdrMtx          * aMtx,
                                             sdptbGGHdr      * aGGHdr,
                                             SInt              aVal )
{
    ULong sResult;

    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    sResult = aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mFreeExts + aVal;

    IDE_TEST( sdrMiniTrans::writeNBytes(
                      aMtx,
                      (UChar*)&(aGGHdr->mLGFreeness[ aGGHdr->mAllocLGIdx ].mFreeExts),
                      (void*)&sResult,
                      ID_SIZEOF( sResult ) ) 
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mFree �ʵ带 �α��ϰ� ��Ʈ�Ѵ�.
 *  aVal��ŭ ���� ��ȭ��Ų��. aVal �� ������� ��� �����ϴ�.
 *
 *  ���Լ��� LG type�� ���ڷ� �޾Ƽ� ó���ϴ°��� �����
 *  logAndModifyFreeExtsOfGG�� ����.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndModifyFreeExtsOfGGByLGType( sdrMtx       * aMtx,
                                                     sdptbGGHdr   * aGGHdr,
                                                     UInt           aLGType,
                                                     SInt           aVal )
{
    ULong sResult;

    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    sResult = aGGHdr->mLGFreeness[aLGType].mFreeExts + aVal;

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                         (UChar*)&(aGGHdr->mLGFreeness[aLGType].mFreeExts),
                         (void*)&sResult,
                         ID_SIZEOF( sResult ) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mLGFreeness���� ULong�迭�� 0��°��Ҹ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetLowBitsOfGG( sdrMtx     * aMtx,
                                         sdptbGGHdr * aGGHdr,
                                         ULong        aBits )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                  aMtx,
                  (UChar*)&(aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[0]),
                  &aBits,
                  ID_SIZEOF(aBits) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  GG�� mLGFreeness���� ULong�迭�� 1��°��Ҹ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetHighBitsOfGG( sdrMtx           * aMtx,
                                          sdptbGGHdr       * aGGHdr,
                                          ULong              aBits )
{
    IDE_ASSERT( aGGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes(
                 aMtx,
                 (UChar*)&(aGGHdr->mLGFreeness[aGGHdr->mAllocLGIdx].mBits[1]),
                 &aBits,
                 ID_SIZEOF(aBits) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/////////////////////////////////////////////////////////////
//     LG       Logging ���� �Լ���.(����)
/////////////////////////////////////////////////////////////

/***********************************************************************
 * Description:
 *  LG�� mStartPID(���� extent�� pid��)�� �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetStartPIDOfLG( sdrMtx     * aMtx,
                                          sdptbLGHdr * aLGHdr,
                                          UInt         aStartPID )
{
    IDE_ASSERT( aLGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aLGHdr->mStartPID),
                                         &aStartPID,
                                         ID_SIZEOF(aStartPID) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  LG�� mHint���� �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetHintOfLG( sdrMtx        * aMtx,
                                      sdptbLGHdr    * aLGHdr,
                                      UInt            aHint )
{
    IDE_ASSERT( aLGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aLGHdr->mHint),
                                         &aHint,
                                         ID_SIZEOF(aHint) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  LG�� mValidBits���� �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetValidBitsOfLG( sdrMtx       * aMtx,
                                           sdptbLGHdr   * aLGHdr,
                                           UInt           aValidBits )
{
    IDE_ASSERT( aLGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aLGHdr->mValidBits),
                                         &aValidBits,
                                         ID_SIZEOF(aValidBits) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  LG�� mFree���� �α��ϰ� ��Ʈ�Ѵ�.
 ***********************************************************************/
IDE_RC sdptbGroup::logAndSetFreeOfLG( sdrMtx            * aMtx,
                                      sdptbLGHdr        * aLGHdr,
                                      UInt                aFree )
{
    IDE_ASSERT( aLGHdr != NULL );
    IDE_ASSERT( aMtx   != NULL );

    IDE_TEST( sdrMiniTrans::writeNBytes( aMtx,
                                         (UChar*)&(aLGHdr->mFree),
                                         &aFree,
                                         ID_SIZEOF(aFree) )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  [ redo routine ]
 *  GG header�� ���� redo ��ƾ�̴�.
 *  ���� �������� GG header������ �ʱ�ȭ�� writeNBytes�� ����ؼ� �����Ͽ����Ƿ�
 *  �� �Լ��� ��ǻ� ���ʿ��ϴ�. ������, ������ ����Ͽ� ����� ���Ҵ�.
 ***********************************************************************/
/* BUG-46036 codesonar warning ����
IDE_RC sdptbGroup::initGG( sdrMtx      * aMtx,
                           UChar       * aPagePtr )
{
    aPagePtr = aPagePtr;
    aMtx = aMtx;

    return IDE_SUCCESS;
}
*/

/***********************************************************************
 * Description:
 *  [ redo routine ]
 *  LG header�� ���� redo ��ƾ�̴�.
 * 
 *  aPagePtr  : LG header�� ���� �����Ͱ� �Ѿ�´�.
 *  aBitVal   : �ʱ�ȭ�� ��Ʈ��
 *              (0�̸� 0���� �ʱ�ȭ�ϰ� 1�̸� ����Ʈ�� 1�� �ʱ�ȭ�Ѵ�.)
 *  aStartIdx : mBitmap���κ����� ��Ʈ�� ��Ʈ�ؾ��ϴ� ���������ε���
 *  aCount    : ��Ʈ�� ��Ʈ�� ����
 ***********************************************************************/
void sdptbGroup::initBitmapOfLG( UChar       * aPagePtr,
                                 UChar         aBitVal,
                                 UInt          aStartIdx,
                                 UInt          aCount )
{
    sdptbLGHdr              *    sLGHdrPtr;
    UInt                         sNrBytes;
    UInt                         sRest;
    UChar                        sLastByte;
    UChar                   *    sStartBitmap =NULL;
    UChar                        sByteForMemset;

    //extent��  �ϳ��� ���� LG�� ����°��� �Ұ�
    IDE_ASSERT( aCount != 0 );
    IDE_ASSERT( (aBitVal == 0) || (aBitVal == 1) );
    IDE_ASSERT( aStartIdx < sdptbGroup::nBitsPerLG() );

    //aLGHdrPtr���� LG header�� �����Ͱ� �Ѿ�´�.
    sLGHdrPtr = (sdptbLGHdr *)aPagePtr;

    /*
     * free�� ���� ������ ��Ʈ�� �˻��� �������ֵ��� �ϴ� Ʈ���̴�.
     * mBitmap�� �ش�Ǵ� ��Ʈ �ٷδ����� 0���� ä�����μ�
     * ��Ʈ���� �˻��Ҷ����� index��ȣ�� �˻��ϴ� ����� ���δ�.
     *
     * �̵����� �Ź����ʿ�� ���� ó�� LG����� ����������� �ϸ� �ȴ�.
     */
    if ( aStartIdx == 0 )
    {
        idlOS::memset( (UChar*)sLGHdrPtr->mBitmap + getLenBitmapOfLG() + 1,
                       SDPTB_LG_BITMAP_MAGIC ,
                       ID_SIZEOF(UChar) );

    }
    else
    {
        /* nothing to do */
    }

    IDE_ASSERT( *((UChar*)sLGHdrPtr->mBitmap + getLenBitmapOfLG() + 1 )
                == SDPTB_LG_BITMAP_MAGIC );

    /*
     * aStartIdx�� byte�� ���ĵǼ� �Ѿ�´ٴ� ������ ����.
     * �ϴ�, ����Ʈ�� ���ĵɶ����� ¥������Ʈ�� ��Ʈ�Ѵ�.
     */
    while( ((aStartIdx % SDPTB_BITS_PER_BYTE) != 0) && (aCount > 0) )
    {
        if ( aBitVal == 1 )
        {
            sdptbBit::setBit( sLGHdrPtr->mBitmap, aStartIdx );
        }
        else
        {
            sdptbBit::clearBit( sLGHdrPtr->mBitmap, aStartIdx );
        }

        aStartIdx++;
        aCount--;
    }

    sNrBytes = aCount / SDPTB_BITS_PER_BYTE;
    sRest = aCount % SDPTB_BITS_PER_BYTE;

    sStartBitmap = (UChar*)sLGHdrPtr->mBitmap + aStartIdx/SDPTB_BITS_PER_BYTE ;

    /*
     * ���� start index�� byte�� ���ĵǾ���.
     * ���� ����Ʈ������ ��Ʈ�� ���� �ϳ��� �ִٸ� ��Ʈ�Ѵ�.
     */
    if ( sNrBytes > 0 )
    {

        if ( aBitVal == 1 )
        {
            sByteForMemset = 0xFF;
        }
        else
        {
            sByteForMemset = 0x00;
        }

        idlOS::memset( sStartBitmap, sByteForMemset, sNrBytes );
    }
    else
    {
        /* nothing to do */
    }

    /*
     * ���������� ¥���� ��Ʈ���� �����Ѵٸ� �� ��Ʈ���� ��Ʈ�Ѵ�.
     */
    if ( sRest != 0 )
    {
        sLastByte = 0xFF;

        if ( aBitVal == 1 )
        {
            sLastByte = ~(sLastByte << sRest);
        }
        else
        {
            sLastByte = sLastByte << sRest;
        }

        idlOS::memset( sStartBitmap + sNrBytes,
                       sLastByte,
                       ID_SIZEOF(UChar) );
    }
    else
    {
        /* nothing to do */
    }

    IDE_ASSERT( *((UChar*)sLGHdrPtr->mBitmap + getLenBitmapOfLG() + 1 )
                == SDPTB_LG_BITMAP_MAGIC );
}

/***********************************************************************
 * Description:
 *  TBS���� ������ ���Ǿ����� �Ҵ��(free�� �ƴ�)������������ ����Ѵ�.
 *
 *  aStatistics     - [IN] �������
 *  aSpaceID        - [IN] Space ID
 *  aAllocPageCount - [OUT] �Ҵ�� ������ ������ ����
 **********************************************************************/
IDE_RC sdptbGroup::getAllocPageCount( idvSQL            * aStatistics,
                                      sddTableSpaceNode * aSpaceNode,
                                      ULong             * aAllocPageCount )
{
    UChar               * sPagePtr;
    UInt                  sState=0;
    sdptbGGHdr          * sGGHdr;
    UInt                  sGGID;
    sddDataFileNode     * sFileNode;
    UInt                  sGGPID;
    UInt                  sFreePages=0; //TBS���� ������� �ʴ� free page����
    UInt                  sTotalPages=0;//TBS�� ��ü page���� 

    IDE_ASSERT( aAllocPageCount != NULL );

    /* BUG-41895 When selecting the v$tablespaces table, 
     * the server doesn`t check the tablespace`s state whether that is discarded or not */
    if (((aSpaceNode->mHeader.mState & SMI_TBS_DROPPED)   != SMI_TBS_DROPPED) &&
        ((aSpaceNode->mHeader.mState & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED))
    {
        for ( sGGID=0 ; sGGID < aSpaceNode->mNewFileID ; sGGID++ )
        {
            sFileNode = aSpaceNode->mFileNodeArr[ sGGID ] ;

            if ( sFileNode == NULL )
            {
                continue;
            }

            // Page�� �������� ���� FileNode�� Drop�Ǹ� �ȵȴ�.
            sddDiskMgr::lockGlobalPageCountCheckMutex( aStatistics );
            sState = 1;
            /* BUG-33919 - [SM] when selecting the X$TABLESPACES and adding datafiles
             *             in tablespace are executed concurrently, server can be aborted
             * ������ datafile �Ӹ� �ƴ϶� �������� datafile �� ����ϸ� �ȵȴ�.
             * �ֳ��ϸ�, ���� �������̱� ������ GG ����� �ʱ�ȭ����
             * �ʾ��� �� �ֱ� �����̴�. */
            if ( SMI_FILE_STATE_IS_DROPPED( sFileNode->mState ) ||
                 SMI_FILE_STATE_IS_CREATING( sFileNode->mState ) )
            {
                sState = 0;
                sddDiskMgr::unlockGlobalPageCountCheckMutex();
                continue;
            }

            sGGPID = SDPTB_GLOBAL_GROUP_HEADER_PID( sGGID ); 

            IDE_TEST( sdbBufferMgr::getPageByPID( aStatistics, 
                                                  aSpaceNode->mHeader.mID,
                                                  sGGPID,
                                                  SDB_S_LATCH,
                                                  SDB_WAIT_NORMAL,
                                                  SDB_SINGLE_PAGE_READ,
                                                  NULL, /* sdrMtx */
                                                  &sPagePtr,
                                                  NULL, /* aTrySuccess */
                                                  NULL  /* IsCorruptPage */ )
                      != IDE_SUCCESS );
            sState = 2;

            sGGHdr = sdptbGroup::getGGHdr(sPagePtr);

            sTotalPages += sGGHdr->mTotalPages;

            /* 
             * �� FSB���� Alloc EG�� Free EG�� free extent������ ���ϸ�
             * �ش� TBS�� free extent������ �������.
             */
            sFreePages += sGGHdr->mLGFreeness[0].mFreeExts * sGGHdr->mPagesPerExt;
            sFreePages += sGGHdr->mLGFreeness[1].mFreeExts * sGGHdr->mPagesPerExt;

            sState = 1;
            IDE_TEST( sdbBufferMgr::releasePage( aStatistics, sPagePtr )
                      != IDE_SUCCESS );

            sState = 0;
            sddDiskMgr::unlockGlobalPageCountCheckMutex();
        }

        *aAllocPageCount = sTotalPages - sFreePages;
    }
    else
    {
        *aAllocPageCount = 0;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch ( sState )
    {
        case 2:
            IDE_ASSERT( sdbBufferMgr::releasePage( aStatistics,
                                                   sPagePtr ) == IDE_SUCCESS );
        case 1:
            sddDiskMgr::unlockGlobalPageCountCheckMutex();
            break;
        default:
            break;
    }
    return IDE_FAILURE;
}

/***********************************************************************
 * Description:
 *  Tablespace���� Cached�� Free Extent������ Return�Ѵ�.
 *
 * aSpaceID - [IN] Space ID
 *
 **********************************************************************/
ULong sdptbGroup::getCachedFreeExtCount( sddTableSpaceNode* aSpaceNode )
{
    sdptbSpaceCache  *  sSpaceCache;
    ULong               sPageCount = 0;

    if( sctTableSpaceMgr::isTempTableSpace( aSpaceNode ) == ID_TRUE )
    {
        sctTableSpaceMgr::lockSpaceNode( NULL /* idvSQL* */,
                                         aSpaceNode );

        if (((aSpaceNode->mHeader.mState & SMI_TBS_DROPPED)   != SMI_TBS_DROPPED ) &&
            ((aSpaceNode->mHeader.mState & SMI_TBS_DISCARDED) != SMI_TBS_DISCARDED ))
        {
            sSpaceCache = (sdptbSpaceCache*)aSpaceNode->mSpaceCache;

            if( sSpaceCache != NULL )
            {
                sPageCount = sSpaceCache->mFreeExtPool.getTotItemCnt();
            }
        }

        sctTableSpaceMgr::unlockSpaceNode( aSpaceNode );
    }

    return sPageCount;
}


