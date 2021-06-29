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
 

/*******************************************************************************
 * $Id: sdpDPathInfoMgr.cpp 83870 2018-09-03 04:32:39Z kclee $
 *
 * Description : Direct-Path INSERT�� ������ �� ���� Table�� �ݿ��� Segment��
 *          ������ �����ϴ� �ڷᱸ���� DPathInfo�̴�. sdpDPathInfoMgr Ŭ������
 *          �� DPathInfo�� �����ϱ� ���� �Լ����� �����Ѵ�.
 ******************************************************************************/

#include <sdb.h>
#include <sdr.h>
#include <sdpReq.h>
#include <sdpPageList.h>
#include <sdpSegDescMgr.h>
#include <sdpDPathInfoMgr.h>
#include <smrUpdate.h>

iduMemPool sdpDPathInfoMgr::mSegInfoPool;

/*******************************************************************************
 * Description : Static �������� �ʱ�ȭ�� �����Ѵ�.
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::initializeStatic()
{
    IDE_TEST( mSegInfoPool.initialize(
                 IDU_MEM_SM_SDP,
                 (SChar*)"DIRECT_PATH_SEGINFO_MEMPOOL",
                 1, /* List Count */
                 ID_SIZEOF( sdpDPathSegInfo ),
                 128, /* ������ ������ �ִ� Item���� */
                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                 ID_TRUE,							/* UseMutex */
                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                 ID_FALSE,							/* ForcePooling */
                 ID_TRUE,							/* GarbageCollection */
                 ID_TRUE,                           /* HWCacheLine */
                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/) 
              != IDE_SUCCESS);			

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Static �������� �ı��Ѵ�.
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::destroyStatic()
{
    IDE_TEST( mSegInfoPool.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Direct Path Info�� �����Ͽ� ��ȯ�Ѵ�.
 *
 * Parameters :
 *      aDPathInfo - [OUT] ���� ������ Direct Path Info
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::initDPathInfo( sdpDPathInfo *aDPathInfo )
{
    IDE_DASSERT( aDPathInfo != NULL );

    /* FIT/ART/sm/Projects/PROJ-1665/createAlterTableTest.tc */
    IDU_FIT_POINT( "1.PROJ-1665@sdpDPathInfoMgr::initDPathInfo" );

    SMU_LIST_INIT_BASE( &aDPathInfo->mSegInfoList );

    aDPathInfo->mNxtSeqNo = 0;
    aDPathInfo->mInsRowCnt = 0;

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
#endif
}

/*******************************************************************************
 * Description : Direct Path Info�� �޷� �ִ� ��� DPathSegInfo�� �ı��Ѵ�.
 *
 * Parameters :
 *      aDPathInfo - [IN] �ı��� Direct Path Info
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::destDPathInfo( sdpDPathInfo *aDPathInfo )
{
    IDE_DASSERT( aDPathInfo != NULL );

    IDE_TEST( sdpDPathInfoMgr::destAllDPathSegInfo( aDPathInfo )
              != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathSegInfo�� �����ϰ� �ʱ�ȭ �Ѵ�.
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aTrans          - [IN] DPath INSERT�� �����ϴ� Transaction
 *      aTableOID       - [IN] ������ DPathSegInfo�� �ش��ϴ� Table�� OID
 *      aDPathInfo      - [IN] ������ DPathSegInfo�� ������ DPathInfo
 *      aDPathSegInfo   - [OUT] ������ DPathSegInfo�� ��ȯ
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::createDPathSegInfo( idvSQL           * aStatistics,
                                            void*              aTrans,
                                            smOID              aTableOID,
                                            sdpDPathInfo     * aDPathInfo, 
                                            sdpDPathSegInfo ** aDPathSegInfo )
{
    SInt                sState = 0;
    sdpPageListEntry  * sPageEntry;
    sdpSegmentDesc    * sSegDesc;
    scSpaceID           sSpaceID;
    scPageID            sSegPID;
    sdpDPathSegInfo   * sDPathSegInfo;

    sdpDPathSegInfo   * sLstSegInfo;
    sdpSegMgmtOp      * sSegMgmtOp;
    sdpSegInfo          sSegInfo;

    sdrMtx              sMtx;
    sdrMtxStartInfo     sStartInfo;
    smLSN               sNTA;
    ULong               sArrData[1];

    IDE_DASSERT( aStatistics != NULL );
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTableOID != SM_NULL_OID );
    IDE_DASSERT( aDPathInfo != NULL );
    IDE_DASSERT( *aDPathSegInfo == NULL );

    *aDPathSegInfo  = NULL;

    //--------------------------------------------------------------------
    // ���� ���� �ʱ�ȭ
    //--------------------------------------------------------------------
    sPageEntry  = (sdpPageListEntry*)smLayerCallback::getPageListEntry( aTableOID );
    sSegDesc    = &sPageEntry->mSegDesc;
    sSpaceID    = sSegDesc->mSegHandle.mSpaceID;
    sSegPID     = sdpPageList::getTableSegDescPID( sPageEntry );

    //--------------------------------------------------------------------
    // DPathSegInfo �ϳ��� �Ҵ��Ѵ�.
    //--------------------------------------------------------------------
    /* sdpDPathInfoMgr_createDPathSegInfo_alloc_DPathSegInfo.tc */
    IDU_FIT_POINT("sdpDPathInfoMgr::createDPathSegInfo::alloc::DPathSegInfo");
    IDE_TEST( mSegInfoPool.alloc( (void**)&sDPathSegInfo ) != IDE_SUCCESS );
    sState = 1;
    
    //--------------------------------------------------------------------
    // DPathSegInfo�� �׻� �����ϰ� �ʱ�ȭ �Ǵ� �κе��� ���� �ʱ�ȭ
    //--------------------------------------------------------------------
    SMU_LIST_INIT_NODE( &sDPathSegInfo->mNode );
    (&sDPathSegInfo->mNode)->mData = sDPathSegInfo;

    sDPathSegInfo->mSpaceID         = sSpaceID;
    sDPathSegInfo->mTableOID        = aTableOID;
    sDPathSegInfo->mFstAllocPID     = SD_NULL_PID;
    sDPathSegInfo->mLstAllocPagePtr = NULL;
    sDPathSegInfo->mRecCount        = 0;
    sDPathSegInfo->mSegDesc         = sSegDesc;
    sDPathSegInfo->mIsLastSeg       = ID_TRUE;

    sLstSegInfo = findLastDPathSegInfo( aDPathInfo, aTableOID );

    if( sLstSegInfo != NULL )
    {
        //------------------------------------------------------------
        //  ���� Statement���� ���� Insert�� �����Ϸ��� Segment��
        // Direct Path Insert�� ������ ���� �ִ� ���, ������ �����ߴ�
        // Direct Path Insert�� �̾� ���̵��� DPathSegInfo�� �����Ѵ�.
        //------------------------------------------------------------
        sDPathSegInfo->mLstAllocExtRID      = sLstSegInfo->mLstAllocExtRID;
        sDPathSegInfo->mLstAllocPID         = sLstSegInfo->mLstAllocPID;
        sDPathSegInfo->mFstPIDOfLstAllocExt = sLstSegInfo->mFstPIDOfLstAllocExt;
        sDPathSegInfo->mTotalPageCount      = sLstSegInfo->mTotalPageCount;

        // ���� SegInfo�� mIsLastSeg �÷��״� FALSE�� �������ش�.
        sLstSegInfo->mIsLastSeg           = ID_FALSE;
    }
    else
    {
        //-------------------------------------------------------------
        //  ���� Transaction ������ ó�� Direct Path Insert�� �����ϴ�
        // Segment�� ���, Insert ��� ���̺��� Segment ������ �޾ƿͼ�
        // DPathSegInfo�� �����Ѵ�.
        //-------------------------------------------------------------
        sSegMgmtOp  = sdpSegDescMgr::getSegMgmtOp( sSpaceID );

        IDE_ERROR( sSegMgmtOp != NULL );

        IDE_TEST( sSegMgmtOp->mGetSegInfo( aStatistics,
                                           sSpaceID,
                                           sSegPID,
                                           NULL, /* aTableHeader */
                                           &sSegInfo )
                  != IDE_SUCCESS );

        if( (sSegInfo.mLstAllocExtRID == SD_NULL_RID) ||
            (sSegInfo.mHWMPID == SD_NULL_PID) )
        {
            ideLog::logMem( IDE_SERVER_0,
                            (UChar*)&sSegInfo,
                            ID_SIZEOF(sdpSegInfo),
                            "Create DPath Segment Info Dump..\n"
                            " Space ID     : %u\n"
                            " Segment PID  : %u",
                            sSpaceID,
                            sSegPID );

            IDE_ASSERT( 0 );
        }

        //--------------------------------------------------------------------
        //  �Ҵ��� Page�� ã�� �������� LstAllocPID�� HWMPID�� ���������ν�
        // HWM ������ ���������� �Ҵ��ϵ��� �����Ѵ�.
        //
        //  TMS���� HWM�� Extent ������ �����ȴ�. ���� ���ʷ� Page�� �Ҵ���
        // ���� ���ο� Extent�� �Ҵ�޾Ƽ� ����Ѵ�.
        //--------------------------------------------------------------------
        sDPathSegInfo->mLstAllocExtRID      = sSegInfo.mLstAllocExtRID;
        sDPathSegInfo->mLstAllocPID         = sSegInfo.mHWMPID;
        sDPathSegInfo->mFstPIDOfLstAllocExt = sSegInfo.mFstPIDOfLstAllocExt;
        sDPathSegInfo->mTotalPageCount      = sSegInfo.mFmtPageCnt;
    }

    //-------------------------------------------------------------------
    // ���� ������ DPathSegInfo�� DPathInfo�� �߰��Ѵ�.
    // Rollback�� �����ϱ� DPathSegInfo�� �߰��� �� ���� �α��� �� �д�.
    //-------------------------------------------------------------------
    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   &sStartInfo,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
                != IDE_SUCCESS );
    sState = 2;

    sNTA = smLayerCallback::getLstUndoNxtLSN( sStartInfo.mTrans );

    sDPathSegInfo->mSeqNo = aDPathInfo->mNxtSeqNo;
    SMU_LIST_ADD_LAST( &aDPathInfo->mSegInfoList,
                       &sDPathSegInfo->mNode );
    aDPathInfo->mNxtSeqNo++;
    sState = 3;

    /* FIT/ART/sm/Projects/PROJ-2068/PROJ-2068.tc */
    IDU_FIT_POINT( "1.PROJ-2068@sdpDPathInfoMgr::createDPathSegInfo" );

    sArrData[0] = sDPathSegInfo->mSeqNo;

    sdrMiniTrans::setNTA( &sMtx,
                          sSpaceID,
                          SDR_OP_SDP_DPATH_ADD_SEGINFO,
                          &sNTA,
                          sArrData,
                          1 ); // sArrData Size

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx ) != IDE_SUCCESS );

    *aDPathSegInfo = sDPathSegInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 3:
            IDE_ASSERT( destDPathSegInfo( aDPathInfo,
                                          sDPathSegInfo,
                                          ID_TRUE ) // Move LastFlag
                        == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
            break;
        case 1:
            IDE_ASSERT( mSegInfoPool.memfree( sDPathSegInfo ) == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    *aDPathSegInfo = NULL;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathSegInfo�� �ı��Ѵ�.
 *
 * Parameters :
 *      aDPathInfo      - [IN] �ı��� DPathSegInfo�� �޸� DPathInfo
 *      aDPathSegInfo   - [IN] �ı��� DPathSegInfo
 *      aMoveLastFlag   - [IN] Last Flag�� �Ű����� ���� ����
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::destDPathSegInfo( sdpDPathInfo    * aDPathInfo,
                                          sdpDPathSegInfo * aDPathSegInfo,
                                          idBool            aMoveLastFlag )
{
    sdpDPathSegInfo   * sPrvDPathSegInfo;

    IDE_DASSERT( aDPathInfo != NULL );
    IDE_DASSERT( aDPathSegInfo != NULL );

    // BUG-30262 �ı��� DPathSegInfo�� ���� ����Ʈ���� ������ �־�� �մϴ�.
    SMU_LIST_DELETE( &aDPathSegInfo->mNode );

    // rollback�� ������ ���� ���� LastFlag�� �Ű���� �� ���
    if( aMoveLastFlag == ID_TRUE )
    {
        // ���� �ֱ��� DPathSegInfo�� ã�Ƽ�,
        sPrvDPathSegInfo = findLastDPathSegInfo( aDPathInfo,
                                                 aDPathSegInfo->mTableOID );

        if( sPrvDPathSegInfo != NULL )
        {
            sPrvDPathSegInfo->mIsLastSeg = ID_TRUE;
        }
    }

    IDE_TEST( mSegInfoPool.memfree( aDPathSegInfo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathInfo�� �޷��ִ� ��� DPathSegInfo�� �ı��Ѵ�.
 *
 * Parameters :
 *      aDPathInfo  - �޷��ִ� ��� DPathSegInfo�� �ı��� DPathInfo
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::destAllDPathSegInfo( sdpDPathInfo  * aDPathInfo )
{
    sdpDPathSegInfo * sDPathSegInfo;
    smuList         * sBaseNode;
    smuList         * sCurNode;

    IDE_DASSERT( aDPathInfo != NULL );

    //----------------------------------------------------------------------
    // DPathInfo�� SegInfoList�� ���鼭 ��� �ı����ش�.
    //----------------------------------------------------------------------
    sBaseNode = &aDPathInfo->mSegInfoList;

    for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
         !SMU_LIST_IS_EMPTY(sBaseNode);
         sCurNode = SMU_LIST_GET_FIRST(sBaseNode) )
    {
        sDPathSegInfo = (sdpDPathSegInfo*)sCurNode;
        IDE_TEST( destDPathSegInfo( aDPathInfo,
                                    sDPathSegInfo,
                                    ID_FALSE ) // Don't move LastFlag
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aDPathSegInfo�� �޸� ��� DPathSegInfo�� Table Segment��
 *          merge �Ѵ�.
 *
 * Implementation :
 *    (1) DPath Insert �� record ������ table info�� �ݿ�
 *    (3) Table Segment�� HWM ����
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aTrans          - [IN] merge�� �����ϴ� TX
 *      aDPathInfo      - [IN] merge�� ������ ��� DPathInfo
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::mergeAllSegOfDPathInfo(
                                        idvSQL*         aStatistics,
                                        void*           aTrans,
                                        sdpDPathInfo*   aDPathInfo )
{
    sdrMtxStartInfo     sStartInfo;
    sdpDPathSegInfo   * sDPathSegInfo;
    sdpSegmentDesc    * sSegDesc;
    sdpSegHandle      * sBaseSegHandle;
    smuList           * sBaseNode;
    smuList           * sCurNode;

    IDE_DASSERT( aStatistics != NULL );
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aDPathInfo != NULL );

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    sBaseNode = &aDPathInfo->mSegInfoList;
    for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
         sCurNode != sBaseNode;
         sCurNode = SMU_LIST_GET_NEXT(sCurNode) )
    {
        sDPathSegInfo = (sdpDPathSegInfo*)sCurNode;

        /* FIT/ART/sm/Projects/PROJ-1665/recoveryTest.tc */
        IDU_FIT_POINT( "3.PROJ-1665@sdpDPathInfoMgr::mergeAllSegOfDPathInfo" );

        //------------------------------------------------------
        // INSERT �Ϸ� ���Դµ�, �Ҵ��� Page�� ������ ASSERT
        //------------------------------------------------------
        if( sDPathSegInfo->mFstAllocPID == SD_NULL_PID )
        {
            (void)dumpDPathInfo( aDPathInfo );
            IDE_ASSERT( 0 );
        }

        IDE_TEST( smLayerCallback::incRecordCountOfTableInfo( aTrans,
                                                              sDPathSegInfo->mTableOID,
                                                              sDPathSegInfo->mRecCount )
                  != IDE_SUCCESS );

        // X$DIRECT_PATH_INSERT - INSERT_ROW_COUNT
        aDPathInfo->mInsRowCnt += sDPathSegInfo->mRecCount;

        sSegDesc        = sDPathSegInfo->mSegDesc;
        sBaseSegHandle  = &(sSegDesc->mSegHandle);

        //-----------------------------------------------------------------
        // ������ SegInfo�� ��� �ش� DPath Insert�� ������ ������ �������� 
        // ���Ե� Extent�� ������ ���������� ��� �������Ѵ�.
        // rollback�� �ش� �������� ������ ���� ������ �� �ֱ� ����. 
        //-----------------------------------------------------------------
        if( sDPathSegInfo->mIsLastSeg == ID_TRUE )
        {
            IDE_TEST( sSegDesc->mSegMgmtOp->mReformatPage4DPath(
                                        aStatistics,
                                        &sStartInfo,
                                        sDPathSegInfo->mSpaceID,
                                        sBaseSegHandle,
                                        sDPathSegInfo->mLstAllocExtRID,
                                        sDPathSegInfo->mLstAllocPID )
                      != IDE_SUCCESS );
        }

        if( sDPathSegInfo->mLstAllocExtRID == SD_NULL_RID )
        {
            (void)dumpDPathInfo( aDPathInfo );
            IDE_ASSERT( 0 );
        }

        IDE_TEST( sSegDesc->mSegMgmtOp->mUpdateHWMInfo4DPath(
                                    aStatistics,
                                    &sStartInfo,
                                    sDPathSegInfo->mSpaceID,
                                    sBaseSegHandle,
                                    sDPathSegInfo->mFstAllocPID,
                                    sDPathSegInfo->mLstAllocExtRID,
                                    sDPathSegInfo->mFstPIDOfLstAllocExt,
                                    sDPathSegInfo->mLstAllocPID,
                                    sDPathSegInfo->mTotalPageCount,
                                    ID_FALSE )
                  != IDE_SUCCESS );

        /* FIT/ART/sm/Projects/PROJ-1665/recoveryTest.tc */
        IDU_FIT_POINT( "5.PROJ-1665@sdpDPathInfoMgr::mergeAllSegOfDPathInfo" );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTableOID�� �ش��ϴ� ������ DPathSegInfo�� ã���ش�.
 *
 *           ���� Ʈ����� ����, DPath INSERT�� ������ ���̺� ���� DML ������
 *          �����ϴµ�, Ŀ���� Open�� �� �� �Լ��� ���� Ŀ���� ������ ���̺��� 
 *          ���� Ʈ����� ������ DPath Insert�� �����ߴ� ���̺����� �˻��Ѵ�.
 *
 * Parameters :
 *      aDPathInfo  - [IN] Transaction�� �޷��ִ� sdpDPathInfo
 *      aTableOID   - [IN] ã�� DPathSegInfo�� �ش��ϴ� TableOID
 *
 * Return :
 *       aTableOID�� �ش��ϴ� sdpDPathSegInfo�� aDPathInfo�� �����Ѵٸ� ã�Ƴ�
 *      sdpDPathSegInfo�� ������, �������� �ʴ´ٸ� NULL�� ��ȯ�Ѵ�.
 ******************************************************************************/
sdpDPathSegInfo* sdpDPathInfoMgr::findLastDPathSegInfo(
                                            sdpDPathInfo  * aDPathInfo,
                                            smOID           aTableOID )
{
    sdpDPathSegInfo       * sDPathSegInfo;
    sdpDPathSegInfo       * sLstSegInfo = NULL;   /* BUG-29031 */
    smuList               * sBaseNode;
    smuList               * sCurNode;

    IDE_DASSERT( aDPathInfo != NULL );
    IDE_DASSERT( aTableOID != SM_NULL_OID );

    sBaseNode = &aDPathInfo->mSegInfoList;
    for( sCurNode = SMU_LIST_GET_LAST(sBaseNode);
         sCurNode != sBaseNode;
         sCurNode = SMU_LIST_GET_PREV(sCurNode) )
    {
        sDPathSegInfo = (sdpDPathSegInfo*)sCurNode;

        if( sDPathSegInfo->mTableOID == aTableOID )
        {
            sLstSegInfo = sDPathSegInfo;
            break;
        }
    }

    return sLstSegInfo;
}

/*******************************************************************************
 * Description : DPathInfo�� dump �Ѵ�.
 *
 * Parameters :
 *      aDPathInfo  - [IN] dump�� DPathInfo
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::dumpDPathInfo( sdpDPathInfo *aDPathInfo )
{
    smuList   * sBaseNode;
    smuList   * sCurNode;

    if( aDPathInfo == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     " DPath Info: NULL\n"
                     "-----------------------" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     "  DPath Info Dump...\n"
                     "Next Seq No       : %u\n"
                     "Insert Row Count  : %llu\n"
                     "-----------------------",
                     aDPathInfo->mNxtSeqNo,
                     aDPathInfo->mInsRowCnt );

        sBaseNode = &aDPathInfo->mSegInfoList;
        for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
             sCurNode != sBaseNode;
             sCurNode = SMU_LIST_GET_NEXT(sCurNode) )
        {
            (void)dumpDPathSegInfo( (sdpDPathSegInfo*)sCurNode );
        }
    }

    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description : DPathSegInfo�� dump �Ѵ�.
 *
 * Parameters :
 *      aDPathSegInfo   - [IN] dump�� DPathSegInfo
 ******************************************************************************/
IDE_RC sdpDPathInfoMgr::dumpDPathSegInfo( sdpDPathSegInfo *aDPathSegInfo )
{
    if( aDPathSegInfo == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "-------------------\n"
                     " DPath Segment Info: NULL\n"
                     "-------------------" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "-------------------\n"
                     "  DPath Segment Info Dump...\n"
                     "Seq No                : %u\n"
                     "Space ID              : %u\n"
                     "Table OID             : %lu\n"
                     "First Alloc PID       : %u\n"
                     "Last Alloc PID        : %u\n"
                     "Last Alloc Extent RID : %llu\n"
                     "First PID Of Last Alloc Extent    : %u\n"
                     "Last Alloc PagePtr    : %"ID_XPOINTER_FMT"\n"
                     "Total Page Count      : %u\n"
                     "Record Count          : %u\n"
                     "Last Segment Flag    : %u\n"
                     "-------------------",
                     aDPathSegInfo->mSeqNo,
                     aDPathSegInfo->mSpaceID,
                     aDPathSegInfo->mTableOID,
                     aDPathSegInfo->mFstAllocPID,
                     aDPathSegInfo->mLstAllocPID,
                     aDPathSegInfo->mLstAllocExtRID,
                     aDPathSegInfo->mFstPIDOfLstAllocExt,
                     aDPathSegInfo->mLstAllocPagePtr,
                     aDPathSegInfo->mTotalPageCount,
                     aDPathSegInfo->mRecCount,
                     aDPathSegInfo->mIsLastSeg );

        (void)sdpSegDescMgr::dumpSegDesc( aDPathSegInfo->mSegDesc );
    }

    return IDE_SUCCESS;
}

