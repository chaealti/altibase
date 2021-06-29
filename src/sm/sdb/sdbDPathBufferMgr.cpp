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
 * $Id: sdbDPathBufferMgr.cpp 86110 2019-09-02 04:52:04Z et16 $
 *
 * Description : DPath Insert�� ������ �� ����ϴ� ���� ���۸� �����Ѵ�.
 ******************************************************************************/

#include <smErrorCode.h>
#include <sdbReq.h>
#include <sdbBufferMgr.h>
#include <sdbDPathBFThread.h>
#include <sdbDPathBufferMgr.h>
#include <sddDiskMgr.h>
#include <smriChangeTrackingMgr.h>

iduMemPool sdbDPathBufferMgr::mBCBPool;
iduMemPool sdbDPathBufferMgr::mPageBuffPool;
iduMemPool sdbDPathBufferMgr::mFThreadPool;

iduMutex   sdbDPathBufferMgr::mBuffMtx;
UInt       sdbDPathBufferMgr::mMaxBuffPageCnt;
UInt       sdbDPathBufferMgr::mAllocBuffPageCnt;

/***********************************************************************
 * Description : �ʱ�ȭ Function���� ������ Memory Pool�� �ʱ�ȭ�Ѵ�.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::initializeStatic()
{
    SInt    sState = 0;

    IDE_TEST( mBCBPool.initialize(
                 IDU_MEM_SM_SDB,
                 (SChar*)"DIRECT_PATH_BUFFER_BCB_MEMPOOL",
                 1, /* List Count */
                 ID_SIZEOF( sdbDPathBCB ),
                 1024, /* ������ ������ �ִ� Item���� */
                 IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                 ID_TRUE,							/* UseMutex */
                 IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                 ID_FALSE,							/* ForcePooling */
                 ID_TRUE,							/* GarbageCollection */
                 ID_TRUE, 							/* HwCacheLine */
                 IDU_MEMPOOL_TYPE_LEGACY            /* mempool type*/)
              != IDE_SUCCESS );
    sState = 1;

    mAllocBuffPageCnt = 0;
    mMaxBuffPageCnt   = smuProperty::getDPathBuffPageCnt();

    IDE_TEST( mPageBuffPool.initialize(
                 IDU_MEM_SM_SDB,
                 (SChar*)"DIRECT_PATH_BUFFER_PAGE_MEMPOOL",
                 1, /* List Count */
                 SD_PAGE_SIZE,
                 1024,
                 1,
                 ID_TRUE,       /* Use Mutex */
                 SD_PAGE_SIZE, 	/* AlignByte */
                 ID_FALSE,		/* ForcePooling */
                 ID_TRUE,		/* GarbageCollection */
                 ID_TRUE,       /* HWCacheLine */
                 IDU_MEMPOOL_TYPE_TIGHT /* mempool type*/) 
              != IDE_SUCCESS);			
    sState = 2;

    IDE_TEST( mFThreadPool.initialize(
                  IDU_MEM_SM_SDB,
                  (SChar*)"DIRECT_PATH_BUFFER_FLUSH_THREAD_MEMPOOL",
                  1, /* List Count */
                  ID_SIZEOF( sdbDPathBFThread ),
                  32, /* ������ ������ �ִ� Item���� */
                  IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                  ID_TRUE,							/* UseMutex */
                  IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignByte */
                  ID_FALSE,							/* ForcePooling */
                  ID_TRUE,							/* GarbageCollection */
                  ID_TRUE,                          /* HWCacheLine */
                  IDU_MEMPOOL_TYPE_LEGACY           /* mempool type*/) 
             != IDE_SUCCESS );			
    sState = 3;

    IDE_TEST( mBuffMtx.initialize( (SChar*)"DIRECT_PATH_BUFFER_POOL_MUTEX",
                                   IDU_MUTEX_KIND_POSIX,
                                   IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 4;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 4:
            IDE_ASSERT( mBuffMtx.destroy() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( mFThreadPool.destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mPageBuffPool.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mBCBPool.destroy() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ �޸� Pool�� �����Ѵ�.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::destroyStatic()
{
    IDE_TEST( mBCBPool.destroy() != IDE_SUCCESS );
    IDE_TEST( mPageBuffPool.destroy() != IDE_SUCCESS );
    IDE_TEST( mFThreadPool.destroy() != IDE_SUCCESS );

    IDE_TEST( mBuffMtx.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : <aSpaceID, aPageID>�� �ش��ϴ� Direct Path Buffer�� �����.
 *
 * aStatistics  - [IN] �������
 * aSpaceID     - [IN] Space ID
 * aPageID      - [IN] Page ID
 * aDPathInfo   - [IN] DPathBuffInfo
 *
 * aIsLogging   - [IN] Logging Mode 
 * aPagePtr     - [OUT] Page Pointer
 *
 * �˰���:
 * 1. <aSpaceID, aPageID>�� �ش��ϴ� �������� Normal Buffer�� ������ ���
 *    �� �������� Direct Path Buffer�� ����� ���� ����. �ֳ��ϸ� Normal
 *    Buffer�� Direct Path Buffer���� ���ÿ� �� �������� ���ؼ� �����Ͽ�
 *    Consistency�� ������ �����̴�. *aPagePtr = NULL�� �ϰ� Return�Ѵ�.
 *    �ƴϸ� ��� ����.
 * 2. sDPathBCB = DPathBCB Pool���� BCB�� �Ҵ��Ѵ�.
 * 3. sDPathBCB�� aSpaceID, aPageID�� �ʱ�ȭ�����Ѵ�.
 * 4. sDPathBCB.mPage = Direct Buffer Pool�� ���ο� Page �� �Ҵ��Ѵ�.
 * 5. sDPathBCB�� ���¸� SDB_DPB_APPEND�� �����.
 * 6. sDPathBCB�� Transaction�� DirectPathBuffer Info�� Append Page List��
 *    �߰��Ѵ�.
 * 7. aPagePtr = sNewFrameBuf�� ���ش�.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::createPage( idvSQL           * aStatistics,
                                      void             * aTrans,
                                      scSpaceID          aSpaceID,
                                      scPageID           aPageID,
                                      idBool             aIsLogging,
                                      UChar**            aPagePtr )
{
    SInt                sState = 0;
    smTID               sTID;
    idBool              sIsExist;
    sdbDPathBCB        *sNewBCB;
    sdbFrameHdr        *sFrameHdr;
    sdbDPathBuffInfo   *sDPathBuffInfo;

    IDE_DASSERT( aTrans   != NULL );
    IDE_DASSERT( aSpaceID <= SC_MAX_SPACE_COUNT - 1);
    IDE_DASSERT( aPageID  != SD_NULL_PID );
    IDE_DASSERT( aPagePtr != NULL );

    *aPagePtr = NULL;
    sIsExist  = ID_FALSE;

    sTID = smLayerCallback::getTransID( aTrans );
    sDPathBuffInfo = (sdbDPathBuffInfo*)smLayerCallback::getDPathBuffInfo(
                                                                    aTrans );
    if( sDPathBuffInfo == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "Trans ID  : %u\n"
                     "Space ID  : %u\n"
                     "Page ID   : %u",
                     sTID,
                     aSpaceID,
                     aPageID );
        (void)smLayerCallback::dumpDPathEntry( aTrans );

        IDE_ASSERT( 0 );
    }

    /* Step 1. <aSpaceID, aPageID>�� �ش��ϴ� �������� Normal Buffer��
     * �����ϸ� �����Ѵ�.  */
    IDE_TEST( sdbBufferMgr::removePageInBuffer( aStatistics,
                                                aSpaceID,
                                                aPageID )
              != IDE_SUCCESS );

    /* Buffer���� ���������Ƿ� ���ŵǾ����� Ȯ���Ѵ�. ���ϰ� Ŭ������
     * �����Ǿ� Debug�϶��� üũ�Ѵ�.
     * Normal Buffer�� Direct Path Buffer���� ���ÿ� ����
     * �������� �ִ� ��� Consistency�� ������ �ִ�.
     * ��) ���ÿ� Disk�� �������� ��ϵǴ� ��� */
    IDE_DASSERT( sdbBufferMgr::isPageExist( aStatistics,
                                            aSpaceID,
                                            aPageID ) == ID_FALSE );
    
    if ( sIsExist == ID_FALSE )
    {
        sDPathBuffInfo->mTotalPgCnt++;

        /* Step 2. DPathBCB Pool���� BCB�� �Ҵ��Ѵ� */
        /* sdbDPathBufferMgr_createPage_alloc_NewBCB.tc */
        IDU_FIT_POINT("sdbDPathBufferMgr::createPage::alloc::NewBCB");
        IDE_TEST( mBCBPool.alloc( (void**)&sNewBCB ) != IDE_SUCCESS );
        sState = 1;

        /* Step 3. BCB�� �ʱ�ȭ�Ѵ�. */
        sNewBCB->mIsLogging     = aIsLogging;
        sNewBCB->mSpaceID       = aSpaceID;
        sNewBCB->mPageID        = aPageID;
        sNewBCB->mDPathBuffInfo = sDPathBuffInfo;
    
        SMU_LIST_INIT_NODE(&(sNewBCB->mNode));
        sNewBCB->mNode.mData = sNewBCB;

        /* Step 4. Page Buffer�� �Ҵ��Ͽ� BCB�� �Ŵܴ�. */
        IDE_TEST( allocBuffPage( aStatistics,
                                 sDPathBuffInfo,
                                 (void**)&(sNewBCB->mPage) )
                  != IDE_SUCCESS );
        sState = 2;

        sFrameHdr = (sdbFrameHdr*) ( sNewBCB->mPage );
        sFrameHdr->mBCBPtr = sNewBCB;
        sFrameHdr->mSpaceID = aSpaceID;

        /* Step 5. BCB�� Append���·� �ٲ۴�. */
        sNewBCB->mState   = SDB_DPB_APPEND;

        /* Step 6. Append Page List�� �ڿ� �߰��Ѵ�. */
        SMU_LIST_ADD_LAST(&(sDPathBuffInfo->mAPgList),
                          &(sNewBCB->mNode));

        /* Step 7. */
        *aPagePtr = sNewBCB->mPage;
    }
    else
    {
        /* Normal Buffer�� Direct Path Buffer���� ���ÿ� ����
         * �������� �ִ� ��� Consistency�� ������ �ִ�.
         * ��) ���ÿ� Disk�� �������� ��ϵǴ� ���
         * */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 2:
            IDE_ASSERT( freeBuffPage( sNewBCB->mPage ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mBCBPool.memfree( sNewBCB ) == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aPagePtr�� �ش��ϴ� BCB�� ���¸� SDB_DPB_DIRTY�� �����
 *               Flush Request List�� ����Ѵ�.
 *
 * aPagePtr - [IN] Page Pointer
 *
 * �˰���
 * 1. aPagePtr�� �ش��ϴ� BCB�� ã�´�.
 * 2. APPEND List���� �����Ѵ�.
 * 3. BCB�� ���¸� SDB_DPB_DIRTY�� �����.
 * 4. Flush Request List�� �߰��Ѵ�.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::setDirtyPage( void* aPagePtr )
{
    SInt              sState = 0;
    sdbDPathBCB*      sBCB;
    sdbDPathBuffInfo *sDPathBCBInfo;
    sdbFrameHdr      *sFrameHdr;

    IDE_DASSERT( aPagePtr != NULL );


    /* 1. �������� ������ġ �տ� BCB�� Pointer����Ǿ� �ִ� .*/
    sFrameHdr = (sdbFrameHdr*) aPagePtr;
    sBCB = (sdbDPathBCB*)(sFrameHdr->mBCBPtr);

    sDPathBCBInfo = sBCB->mDPathBuffInfo;

    /* Transaction�� Flush Request List�� Direct Path Flush Thread��
     * ���ÿ� �����ϴ� ����Ʈ�̱⶧���� �ݵ�� Mutex�� ��ƾ� �Ѵ�.*/
    IDE_TEST( sDPathBCBInfo->mMutex.lock( NULL /* idvSQL* */ )
              != IDE_SUCCESS );
    sState = 1;

    /* 2. APPEND List���� �����Ѵ�. */
    SMU_LIST_DELETE( &(sBCB->mNode) );

    /* 3. BCB�� ���¸� SDB_DPB_DIRTY�� �����. */
    sBCB->mState = SDB_DPB_DIRTY;

    /* 4. Flush Request List�� �߰��Ѵ�. */
    SMU_LIST_ADD_LAST(&(sDPathBCBInfo->mFReqPgList),
                      &(sBCB->mNode));

    sState = 0;
    IDE_TEST( sDPathBCBInfo->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( sDPathBCBInfo->mMutex.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aDPathBuffInfo�� ������ ��� Direct Path Buffer��
 *               ��ũ�� ������. �̴� Transaction�� Commit���� ȣ���Ѵ�.
 *
 * aTrans - [IN] Transaction Pointer
 *
 * �˰���
 * 1. aTrans�� ���� DPBI(Direct Path Buffer Info)�� ���Ѵ�.
 * 2. APPEND List���� �������� Disk�� ������.
 * 3. Flush List�� �ִ� �������� BCB�� ���°� Dirty�ΰ��� Flag�� FLUSH��
 *    �ٲٰ� ����������.
 * 4. Flush Request List�� ������ �������� ��ٸ���. �� Flush Thread��
 *    IO�۾��� �Ϸ��Ҷ����� ��ٸ���.
 *
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::flushAllPage( idvSQL            * aStatistics,
                                        sdbDPathBuffInfo  * aDPathBuffInfo )
{
    UInt                sState = 0;
    sdbDPathBulkIOInfo  sDPathBulkIOInfo;

    IDE_DASSERT( aDPathBuffInfo != NULL );

    IDE_TEST( sdbDPathBufferMgr::initBulkIOInfo( &sDPathBulkIOInfo )
              != IDE_SUCCESS );
    sState = 1;

    //---------------------------------------------------------------
    // Flush Thread�� ���� �����Ų��.
    //---------------------------------------------------------------
    IDE_TEST( destFlushThread( aDPathBuffInfo ) != IDE_SUCCESS );

    //---------------------------------------------------------------------
    // DPathBuffInfo�� �ִ� ��� �������鿡 ���Ͽ� Flush�� �����Ѵ�.
    //---------------------------------------------------------------------
    IDE_TEST( flushBCBInDPathInfo( aStatistics,
                                   aDPathBuffInfo,
                                   &sDPathBulkIOInfo )
              != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( sdbDPathBufferMgr::destBulkIOInfo( &sDPathBulkIOInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            // BulkIOInfo�� �Ŵ޷� �ִ� Page�鵵 ���� free ���ش�.
            IDE_ASSERT( freeBCBInList( &sDPathBulkIOInfo.mBaseNode )
                        == IDE_SUCCESS );
            IDE_ASSERT( sdbDPathBufferMgr::destBulkIOInfo( &sDPathBulkIOInfo )
                        == IDE_SUCCESS );
        default:
            break;
    }

    // �� �Լ��� ȣ�� �Ǹ� flush�� �Ƶ�, �����Ͽ� free�ϰ� �ǵ�
    // DPathBuffInfo�� �޷��ִ� BCB�� Page���� free���ְ� �������� �Ѵ�.
    IDE_ASSERT( freeBCBInDPathInfo( aDPathBuffInfo ) == IDE_SUCCESS );
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aDPathBuffInfo�� ��� Direct Path Buffer ��������
 *               Flush ��û�� ��� �ּ��Ѵ�. �̴� Rollback�ÿ� ȣ���Ѵ�.
 *
 * aDPathBuffInfo  - [IN] Direct Path Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::cancelAll( sdbDPathBuffInfo  * aDPathBuffInfo )
{
    //---------------------------------------------------------------
    // Flush Thread�� ���� �����Ų��.
    //---------------------------------------------------------------
    IDE_TEST( destFlushThread( aDPathBuffInfo )
                != IDE_SUCCESS );

    IDE_TEST( freeBCBInDPathInfo( aDPathBuffInfo ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Buffer Info�� �ִ� ��� BCB�� ���� Flush��
 *               �����ϰ� �Ϸ�Ǹ� BCB�� �Ҵ�� Buffer Page�� Free��Ų��.
 *               �̶� aNeedWritePage�� ID_TRUE�� Flush�� �����ϰ� ID_FALSE
 *               �̸� Free���Ѵ�.
 *
 * aDPathBCBInfo   - [IN] Direct Path Buffer Info
 * aNeedWritePage  - [IN] if ID_TRUE, BCB�� �������� ���� WritePage����,
 *                        �ƴϸ� writePage�� �������� �ʰ� ���� Free��
 *                        ��Ų��.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::flushBCBInDPathInfo(
    idvSQL             * aStatistics,
    sdbDPathBuffInfo   * aDPathBCBInfo,
    sdbDPathBulkIOInfo * aDPathBulkIOInfo )
{
    IDE_DASSERT( aDPathBCBInfo != NULL );
    IDE_DASSERT( aDPathBulkIOInfo != NULL );

    scSpaceID   sNeedSyncSpaceID = 0;

    if( aDPathBCBInfo->mTotalPgCnt != 0 )
    {
        /* Append List�� �ִ� ��� Page�� write �Ѵ�. */
        IDE_TEST( flushBCBInList( aStatistics,
                                  aDPathBCBInfo,
                                  aDPathBulkIOInfo,
                                  &(aDPathBCBInfo->mAPgList),
                                  &sNeedSyncSpaceID )
                  != IDE_SUCCESS );
        /* Flush Request List�� �ִ� ��� �������� write�Ѵ� */
        IDE_TEST( flushBCBInList( aStatistics,
                                  aDPathBCBInfo,
                                  aDPathBulkIOInfo,
                                  &(aDPathBCBInfo->mFReqPgList),
                                  &sNeedSyncSpaceID )
                  != IDE_SUCCESS );

        /* Bulk IO�̱⶧���� ���� ���۰� ���� �ʾƼ� Disk�� �������
         * ���� �������� ������ �ִ�. ������ write�� ��û�Ѵ�.*/
        IDE_TEST( writePagesByBulkIO( aStatistics,
                                      aDPathBCBInfo,
                                      aDPathBulkIOInfo,
                                      &sNeedSyncSpaceID )
                  != IDE_SUCCESS );

        if( sNeedSyncSpaceID != 0 )
        {
            IDE_TEST( sddDiskMgr::syncTBSInNormal( aStatistics,
                                                   sNeedSyncSpaceID )
                      != IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aBaseNode�� BCB�� ���� Flush�۾��� �Ѵ�.
 * 
 * �˰���
 * + aBaseNode�� ������ Node�� ������ ���� ������ �����Ѵ�.
 *  1. ���� Node�� BCB�� ���°� SDB_DPB_FLUSH�� �ƴϸ�
 *   1.1 aFlushPage�� ID_TRUE��
 *    - BCB���¸� SDB_DPB_FLUSH�� �ٲ۴�.
 *    - BCB�� ����Ű�� �������� Disk�� ����Ѵ�.
 *   1.2 aBaseNode�� ����Ʈ���� ���� BCB�� �����Ѵ�.
 *   1.3 BCB�� BCB�� �Ҵ�� Buffer Page�� Free�Ѵ�.

 *
 * aDPathBCBInfo    - [IN] Direct Path Buffer Info
 * aDPathBulkIOInfo - [IN] Direct Path Bulk IO Info
 * aBaseNode        - [IN] Flush�۾� �� Free�� List�� BaseNode
 * aFlushPage       - [IN] if ID_TRUE, BCB�� �������� ���� WritePage����, �ƴϸ�
 *                         WritePage�� �������� �ʰ� ���� Free�� ��Ų��.
 * aNeedSyncSpaceID - [IN-OUT] Flush�ؾ��� TableSpaceID�� ����Ų��. �� ����
 *                        ���� ����ؾ��� BCB�� TableSpaceID�� �ٸ���
 *                        aNeedSyncSpaceID�� ����Ű�� TableSpace�� Flush�Ѵ�.
 *                        �׸��� aNeedSyncSpaceID�� BCB�� TableSpaceID��
 *                        �����Ѵ�.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::flushBCBInList(
    idvSQL*              aStatistics,
    sdbDPathBuffInfo*    aDPathBCBInfo,
    sdbDPathBulkIOInfo*  aDPathBulkIOInfo,
    smuList*             aBaseNode,
    scSpaceID*           aNeedSyncSpaceID )
{
    SInt          sState = 0;
    smuList      *sCurNode;
    smuList      *sNxtNode;
    sdbDPathBCB  *sCurBCB;

    IDE_DASSERT( aDPathBCBInfo != NULL );
    IDE_DASSERT( aBaseNode != NULL );

    IDE_TEST( aDPathBCBInfo->mMutex.lock( NULL /* idvSQL* */ )
              != IDE_SUCCESS );
    sState = 1;

    sCurNode  = SMU_LIST_GET_FIRST(aBaseNode);
    while( sCurNode != aBaseNode )
    {
        sCurBCB = (sdbDPathBCB*)sCurNode->mData;
        sNxtNode = SMU_LIST_GET_NEXT( sCurNode );

        /* List���� �����Ѵ�. */
        SMU_LIST_DELETE( sCurNode );

        /* ���� BCB�� ���� Flush�� �������̶�� ǥ���Ѵ�. */
        sCurBCB->mState = SDB_DPB_FLUSH;

        if( isNeedBulkIO( aDPathBulkIOInfo, sCurBCB ) == ID_TRUE )
        {
            sState = 0;
            IDE_TEST( aDPathBCBInfo->mMutex.unlock() != IDE_SUCCESS );

            /* Bulk IO�� �����Ѵ�. ���ÿ� ����Ʈ���� �����ϰ� free �Ѵ�. */
            IDE_TEST( writePagesByBulkIO( aStatistics,
                                          aDPathBCBInfo,
                                          aDPathBulkIOInfo,
                                          aNeedSyncSpaceID )
                      != IDE_SUCCESS );

            IDE_TEST( aDPathBCBInfo->mMutex.lock( NULL /* idvSQL* */ )
                      != IDE_SUCCESS );
            sState = 1;
            /* MutexǮ������ ���� sNxtNode�� Valid�ϴٴ� ���� ������ �� ����.
             * �ֳ��ϸ� �� ����Ʈ�� Transactoin�� Flush Thread�� ���ÿ�
             * �����ؼ� IO�� �����Ͽ� Mutex�� Ǭ���̿� �ٸ� Thread��
             * �� nextnode�� ���ؼ� �۾��� �Ϸ��Ͽ� free���ѹ������� �ֱ⶧���̴�.*/
            sNxtNode  = SMU_LIST_GET_FIRST(aBaseNode);
        }

        IDE_TEST( addNode4BulkIO( aDPathBulkIOInfo,
                                  sCurNode )
                    != IDE_SUCCESS );

        sCurNode = sNxtNode;
    }

    sState = 0;
    IDE_TEST( aDPathBCBInfo->mMutex.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( aDPathBCBInfo->mMutex.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Buffer Info�� �ִ� ��� BCB�� ���� Free��
 *               ���� �Ѵ�.
 *
 * aDPathBCBInfo   - [IN] Direct Path Buffer Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::freeBCBInDPathInfo(
    sdbDPathBuffInfo   * aDPathBCBInfo )
{
    SInt          sState = 0;

    IDE_DASSERT( aDPathBCBInfo != NULL );

    if( aDPathBCBInfo->mTotalPgCnt != 0 )
    {
        IDE_TEST( aDPathBCBInfo->mMutex.lock( NULL /* idvSQL* */ )
                != IDE_SUCCESS );
        sState = 1;

        /* Append List�� �ִ� ��� Page�� free �Ѵ�. */
        IDE_TEST( freeBCBInList( &(aDPathBCBInfo->mAPgList) )
                  != IDE_SUCCESS );
        /* Flush Request List�� �ִ� ��� �������� free �Ѵ� */
        IDE_TEST( freeBCBInList( &(aDPathBCBInfo->mFReqPgList) )
                  != IDE_SUCCESS );

        sState = 0;
        IDE_TEST( aDPathBCBInfo->mMutex.unlock() != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( aDPathBCBInfo->mMutex.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aBaseNode�� BCB�� ���� Free�۾��� �Ѵ�.
 *
 * aBaseNode        - [IN] Flush�۾� �� Free�� List�� BaseNode
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::freeBCBInList( smuList* aBaseNode )
{
    smuList      *sCurNode;
    sdbDPathBCB  *sCurBCB;

    IDE_DASSERT( aBaseNode != NULL );

    for( sCurNode = SMU_LIST_GET_FIRST(aBaseNode);
         sCurNode != aBaseNode;
         sCurNode = SMU_LIST_GET_FIRST(aBaseNode) ) /* BUG-30076 */
    {
        sCurBCB = (sdbDPathBCB*)sCurNode->mData;

        /* List���� �����Ѵ�. */
        SMU_LIST_DELETE( sCurNode );

        /* BCB�� �Ҵ�� ���۸� Free�Ѵ� */
        IDE_TEST( freeBuffPage( sCurBCB->mPage )
                  != IDE_SUCCESS );
        IDE_TEST( mBCBPool.memfree( sCurBCB ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Info�� ���ؼ� ��׶���� Flush�� Flush Thread
 *               ���� �� ���۽�Ų��.
 *
 * aDPathBCBInfo - [IN] Direct Path Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::createFlushThread( sdbDPathBuffInfo*  aDPathBCBInfo )
{
    SInt    sState = 0;
    IDE_DASSERT( aDPathBCBInfo != NULL );

    /* sdbDPathBufferMgr_createFlushThread_alloc_FlushThread.tc */
    IDU_FIT_POINT("sdbDPathBufferMgr::createFlushThread::alloc::FlushThread");
    IDE_TEST( mFThreadPool.alloc((void**)&(aDPathBCBInfo->mFlushThread))
              != IDE_SUCCESS );
    sState = 1;

    new (aDPathBCBInfo->mFlushThread) sdbDPathBFThread();

    IDE_TEST( aDPathBCBInfo->mFlushThread->initialize( aDPathBCBInfo )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( aDPathBCBInfo->mFlushThread->startThread()
              != IDE_SUCCESS );
    sState = 3;

    IDU_FIT_POINT_RAISE( "1.PROJ-2068@sdbDPathBufferMgr::createFlushThread", err_ART );

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION(err_ART);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ART));
    }
#endif
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 3:
            IDE_ASSERT( aDPathBCBInfo->mFlushThread->shutdown()
                        == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( aDPathBCBInfo->mFlushThread->destroy()
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mFThreadPool.memfree( aDPathBCBInfo->mFlushThread )
                        == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Info�� Flush Thread�� �����Ų��.
 *
 * aDPathBCBInfo - [IN] Direct Path Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::destFlushThread( sdbDPathBuffInfo*  aDPathBCBInfo )
{
    IDE_DASSERT( aDPathBCBInfo != NULL );

    // BUG-30216 destFlushThread �Լ��� ���� TX ������ ������ �Ҹ� �� �ֱ�
    // ������ Flush Thread�� ���� �� ���� �ı��� �ش�.
    if( aDPathBCBInfo->mFlushThread != NULL )
    {
        IDE_TEST( aDPathBCBInfo->mFlushThread->shutdown()
                != IDE_SUCCESS );

        IDE_TEST( aDPathBCBInfo->mFlushThread->destroy()
                != IDE_SUCCESS );

        IDE_TEST( mFThreadPool.memfree( aDPathBCBInfo->mFlushThread )
                != IDE_SUCCESS );

        aDPathBCBInfo->mFlushThread = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Info�� �ʱ�ȭ�Ѵ�.
 *
 * aDPathBCBInfo - [IN-OUT] Direct Path Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::initDPathBuffInfo( sdbDPathBuffInfo *aDPathBuffInfo )
{
    SInt    sState = 0;

    IDE_ASSERT( aDPathBuffInfo != NULL );

    aDPathBuffInfo->mTotalPgCnt = 0;

    SMU_LIST_INIT_BASE(&(aDPathBuffInfo->mAPgList));
    SMU_LIST_INIT_BASE(&(aDPathBuffInfo->mFReqPgList));

    IDE_TEST( aDPathBuffInfo->mMutex.initialize(
                    (SChar*)"DIRECT_PATH_BUFFER_INFO_MUTEX",
                    IDU_MUTEX_KIND_NATIVE,
                    IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    IDE_TEST( createFlushThread( aDPathBuffInfo )
              != IDE_SUCCESS );
    sState = 2;

    aDPathBuffInfo->mAllocBuffPageTryCnt = 0;
    aDPathBuffInfo->mAllocBuffPageFailCnt = 0;
    aDPathBuffInfo->mBulkIOCnt = 0;

    IDU_FIT_POINT_RAISE( "1.PROJ-2068@sdbDPathBufferMgr::initDPathBuffInfo", err_ART );

    return IDE_SUCCESS;
#ifdef ALTIBASE_FIT_CHECK
    IDE_EXCEPTION(err_ART);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ART));
    }
#endif
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 2:
            IDE_ASSERT( destFlushThread( aDPathBuffInfo ) == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( aDPathBuffInfo->mMutex.destroy() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Info�� �Ҹ��Ų��.
 *
 * aDPathBCBInfo - [IN] Direct Path Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::destDPathBuffInfo( sdbDPathBuffInfo *aDPathBuffInfo )
{
    IDE_DASSERT( aDPathBuffInfo != NULL );

    IDE_TEST( destFlushThread( aDPathBuffInfo )
              != IDE_SUCCESS );

    if( !SMU_LIST_IS_EMPTY(&(aDPathBuffInfo->mAPgList)) ||
        !SMU_LIST_IS_EMPTY(&(aDPathBuffInfo->mFReqPgList)) )
    {
        (void)dumpDPathBuffInfo( aDPathBuffInfo );
        IDE_ASSERT( 0 );
    }

    IDE_TEST( aDPathBuffInfo->mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Bulk IO�� ���� structure�� �ʱ�ȭ�Ѵ�.
 *
 * aDPathBulkIOInfo - [IN] Direct Path Bulk IO Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::initBulkIOInfo( sdbDPathBulkIOInfo *aDPathBulkIOInfo )
{
    /* Bulk IO�� �����Ҷ� �ѹ��� ��ϵ� �������� ������ �����´�. */
    aDPathBulkIOInfo->mDBufferSize =
        smuProperty::getBulkIOPageCnt4DPInsert();

    aDPathBulkIOInfo->mIORequestCnt = 0;

    aDPathBulkIOInfo->mLstSpaceID   = SC_NULL_SPACEID;
    aDPathBulkIOInfo->mLstPageID    = SC_NULL_PID;

    /* Bulk IO�� �����Ҷ� �ѹ��� ����ϱ� ���� �ʿ��� ���۰����� �Ҵ��Ѵ� . */
    IDE_TEST( iduFile::allocBuff4DirectIO(
                  IDU_MEM_SM_SDB,
                  aDPathBulkIOInfo->mDBufferSize * SD_PAGE_SIZE,
                  (void**)&( aDPathBulkIOInfo->mRDIOBuffPtr ),
                  (void**)&( aDPathBulkIOInfo->mADIOBuffPtr ) )
              != IDE_SUCCESS );

    SMU_LIST_INIT_BASE(&(aDPathBulkIOInfo->mBaseNode));

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Bulk IO�� ���� structure�� �����Ѵ�.
 *
 * aDPathBulkIOInfo - [IN] Direct Path Bulk IO Info
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::destBulkIOInfo( sdbDPathBulkIOInfo *aDPathBulkIOInfo )
{
    if( aDPathBulkIOInfo->mRDIOBuffPtr == NULL ||
        aDPathBulkIOInfo->mADIOBuffPtr == NULL ||
        !SMU_LIST_IS_EMPTY(&aDPathBulkIOInfo->mBaseNode) )
    {
        (void)dumpDPathBulkIOInfo( aDPathBulkIOInfo );
        IDE_ASSERT( 0 );
    }

    IDE_TEST( iduMemMgr::free( aDPathBulkIOInfo->mRDIOBuffPtr )
              != IDE_SUCCESS );
    aDPathBulkIOInfo->mRDIOBuffPtr = NULL;
    aDPathBulkIOInfo->mADIOBuffPtr = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : aBCB�� Bulk IO Buffer�� �߰��������� �����Ѵ�. �Ұ��ϸ�
 *               Buffer�� �ִ� ���� ������ �Ѵ�. ������ ID_TRUE, �ƴϸ�
 *               ID_FALSE.
 *
 * aDPathBulkIOInfo - [IN] Direct Path Bulk IO Info
 * aBCB             - [IN] aDPathBulkIOInfo�� mBaseNode�� �߰��� BCB
 **********************************************************************/
idBool sdbDPathBufferMgr::isNeedBulkIO( sdbDPathBulkIOInfo *aDPathBulkIOInfo,
                                        sdbDPathBCB        *aBCB )
{
    idBool  sIsNeed = ID_TRUE;

    /* ������ ���� ��� Bulk IO�� �����Ѵ�.
     * 1. Bulk IO Buffer�� Full
     * 2. ���ӵ� �������� �ƴѰ��
     *   - SpaceID�� Ʋ�����
     *   - PageID�� ������ Add�� �������� ���ӵ��� ���� �� */
    if( ( aDPathBulkIOInfo->mIORequestCnt >= aDPathBulkIOInfo->mDBufferSize ) ||
        ( (aBCB->mSpaceID != aDPathBulkIOInfo->mLstSpaceID) &&
          (aDPathBulkIOInfo->mLstSpaceID != 0 )) ||
        ( (aBCB->mPageID  != aDPathBulkIOInfo->mLstPageID + 1) &&
          (aDPathBulkIOInfo->mLstPageID != 0) ) )
    {
        sIsNeed = ID_TRUE;
    }
    else
    {
        sIsNeed = ID_FALSE;
    }

    return sIsNeed;
}

/***********************************************************************
 * Description : Bulk IO Info�� IO Request�� �߰��Ѵ�.
 *
 * aDPathBCBInfo - [IN] Direct Path Info
 * aDPathBCB     - [IN] Direct Path BCB Pointer
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::addNode4BulkIO( sdbDPathBulkIOInfo *aDPathBulkIOInfo,
                                          smuList            *aFlushNode )
{
    sdbDPathBCB  *sCurBCB;

    IDE_DASSERT( aDPathBulkIOInfo != NULL );
    IDE_DASSERT( aFlushNode != NULL );

    sCurBCB  = (sdbDPathBCB*)aFlushNode->mData;

    SMU_LIST_ADD_LAST( &( aDPathBulkIOInfo->mBaseNode ),
                       aFlushNode );

    aDPathBulkIOInfo->mLstSpaceID = sCurBCB->mSpaceID;
    aDPathBulkIOInfo->mLstPageID = sCurBCB->mPageID;
    aDPathBulkIOInfo->mIORequestCnt++;

    return IDE_SUCCESS;
}

/***********************************************************************
 * Description : Bulk IO�� �����Ѵ�.
 *
 * aDPathBulkIOInfo - [IN] Direct Path Bulk IO Info
 * aPrevSpaceID     - ������ writePage�� ������ BCB�� TableSpace�� ID
 *                    return�� ���� BCB�� SpaceID�� ����.
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::writePagesByBulkIO(
    idvSQL             *aStatistics,
    sdbDPathBuffInfo   *aDPathBuffInfo,
    sdbDPathBulkIOInfo *aDPathBulkIOInfo,
    scSpaceID          *aPrevSpaceID )
{
    smLSN               sLstLSN;
    UChar               *sBuffFrame;
    scSpaceID           sSpaceID;
    scPageID            sPageID;
    smuList             *sCurNode;
    smuList             *sNxtNode;
    sdbDPathBCB         *sCurBCB;
    scGRID              sPageGRID;
    UInt                sIBChunkID;
    UInt                i;
    sddTableSpaceNode   *sSpaceNode;  
    sddDataFileNode     *sFileNode;

    if( aDPathBulkIOInfo->mIORequestCnt > 0 )
    {
        sCurNode  = SMU_LIST_GET_FIRST( &(aDPathBulkIOInfo->mBaseNode) );
        sCurBCB   = (sdbDPathBCB*)sCurNode->mData;

        /* sddDiskMgr::syncTableSpaceInNormal�� ȣ��Ƚ���� ���̱� ���ؼ�
         * writePage������ ���� ���� �ʰ� writePage�� ������ writePage��
         * ��û�� BCB�� SpaceID�� ���� writePage�� ��û�� BCB�� SpaceID��
         * �ٸ����� syncTableSpaceInNormal�� �����Ѵ�. �׸��� ������
         * writePage�� aPrevSpaceID�� ����Ű�� TableSpace�� ���ؼ�
         * syncTableSpaceInNormal�� �����Ѵ�. */
        if( *aPrevSpaceID != 0 )
        {
            if( *aPrevSpaceID != sCurBCB->mSpaceID )
            {
                IDE_TEST( sddDiskMgr::syncTBSInNormal( aStatistics, *aPrevSpaceID )
                          != IDE_SUCCESS );
            }
        }

        *aPrevSpaceID = sCurBCB->mSpaceID;

        IDU_FIT_POINT( "3.PROJ-1665@sdbDPathBufferMgr::writePagesByBulkIO" );

        sSpaceID = sCurBCB->mSpaceID;
        sPageID  = sCurBCB->mPageID;

        /* �� �������� ���ؼ� �α��� ���� ���� ��� redo�� LSN��
         * Page�� write�Ǵ� ������ System LSN���� LSN�� �۰� �����Ǹ�
         * ������ �� Page�� update ���� Log�� Redo�� �� �� �ִ�.
         * ������ LSN�� ���� write������ system last lsn���� �����Ѵ�. */
        smLayerCallback::getLstLSN( &sLstLSN );

        for( i = 0; i < aDPathBulkIOInfo->mIORequestCnt; i++ )
        {
            if ( sCurBCB->mIsLogging == ID_TRUE ) 
            {
                /* PROJ-1665 : Page�� Physical Image�� Logging�Ѵ� */
                SC_MAKE_GRID(sPageGRID,sCurBCB->mSpaceID,sCurBCB->mPageID,0);

                IDE_TEST( smLayerCallback::writeDPathPageLogRec( aStatistics,
                                                                 sCurBCB->mPage,
                                                                 sPageGRID,
                                                                 &sLstLSN )
                          != IDE_SUCCESS );
            }
            else
            {
                /* nothing to do */
            }

            sBuffFrame = sCurBCB->mPage;

            smLayerCallback::setPageLSN( sBuffFrame, &sLstLSN );
            smLayerCallback::calcAndSetCheckSum( sBuffFrame );

            idlOS::memcpy( aDPathBulkIOInfo->mADIOBuffPtr + SD_PAGE_SIZE * i,
                           sBuffFrame,
                           SD_PAGE_SIZE );

            sNxtNode = SMU_LIST_GET_NEXT( sCurNode );
            SMU_LIST_DELETE( sCurNode );

            /* BCB�� �Ҵ�� ���۸� Free�Ѵ� */
            IDE_TEST( freeBuffPage( sCurBCB->mPage )
                      != IDE_SUCCESS );

            IDE_TEST( mBCBPool.memfree( sCurBCB ) != IDE_SUCCESS );

            sCurNode = sNxtNode;
            sCurBCB  = (sdbDPathBCB*)sCurNode->mData;
        }

        /* Bulk IO�� �����Ѵ�. */

        IDE_TEST( sddDiskMgr::writeMultiPage( aStatistics,
                                              sSpaceID,
                                              sPageID,
                                              aDPathBulkIOInfo->mIORequestCnt,
                                              aDPathBulkIOInfo->mADIOBuffPtr )
                  != IDE_SUCCESS );
        
        if ( smLayerCallback::isCTMgrEnabled() == ID_TRUE )
        {
            
            IDE_ASSERT( sctTableSpaceMgr::isTempTableSpace( sSpaceID ) 
                        != ID_TRUE );

            IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( sSpaceID,
                                                                (void**)&sSpaceNode)
                      != IDE_SUCCESS );

            IDE_ASSERT( sSpaceNode->mHeader.mID == sSpaceID );

            IDE_TEST( sddTableSpace::getDataFileNodeByPageID(sSpaceNode,
                                                             sPageID,
                                                             &sFileNode )
              != IDE_SUCCESS );

            for( i = 0; i < aDPathBulkIOInfo->mIORequestCnt; i++ )
            {
                sIBChunkID = 
                   smriChangeTrackingMgr::calcIBChunkID4DiskPage( sPageID + i );

                IDE_TEST( smriChangeTrackingMgr::changeTracking( sFileNode,
                                                                 NULL,
                                                                 sIBChunkID )
                          != IDE_SUCCESS );
            }
        }

        aDPathBulkIOInfo->mIORequestCnt = 0;

        // X$DIRECT_PATH_INSERT - BULK_IO_COUNT
        aDPathBuffInfo->mBulkIOCnt++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Buffer�� �Ҵ��Ѵ�. mPageBuffPool�� ���
 *               Direct Path Insert�� �����ϴ� Transaction���� �����Ѵ�. ������
 *               ���ü� ��� �ʿ��ϰ� ������ ������ �þ�� ���� �ʱ⶧����
 *               DIRECT_PATH_BUFFER_PAGE_COUNT��� �Ӽ��� �ΰ� �� �� �̻�����
 *               �޸𸮰� �Ҵ���� �ʵ��� �Ͽ���. �� �Ӽ��� Alter������ �̿��ؼ�
 *               �ٲ� ���� �ִ�. �Ҵ�Ǵ� �޸� ũ��� SD_PAGE_SIZE�̴�.
 *
 * aPage - [OUT] �Ҵ�� �޸𸮸� �޴´�.
 *
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::allocBuffPage( idvSQL            * aStatistics,
                                         sdbDPathBuffInfo  * aDPathBuffInfo,
                                         void             ** aPage )
{
    SInt             sState = 0;
    SInt             sAllocState = 0;
    PDL_Time_Value   sTV;

    sTV.set(0, smuProperty::getDPathBuffPageAllocRetryUSec() );

    while(1)
    {
        IDE_TEST( mBuffMtx.lock( NULL /* idvSQL* */) != IDE_SUCCESS );
        sState = 1;

        // X$DIRECT_PATH_INSERT - TRY_ALLOC_BUFFER_PAGE_COUNT
        aDPathBuffInfo->mAllocBuffPageTryCnt++;

        if( mAllocBuffPageCnt < mMaxBuffPageCnt )
        {
            break;
        }

        sState = 0;
        IDE_TEST( mBuffMtx.unlock() != IDE_SUCCESS );

        // X$DIRECT_PATH_INSERT - FAIL_ALLOC_BUFFER_PAGE_COUNT
        aDPathBuffInfo->mAllocBuffPageFailCnt++;

        IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );

        idlOS::sleep( sTV );
    }

    mAllocBuffPageCnt++;
    IDE_TEST( mPageBuffPool.alloc( aPage ) != IDE_SUCCESS );
    sAllocState = 1;

    sState = 0;
    IDE_TEST( mBuffMtx.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    if( sState != 0 )
    {
        IDE_ASSERT( mBuffMtx.unlock() == IDE_SUCCESS );
    }
    if( sAllocState != 0 )
    {
        IDE_ASSERT( mPageBuffPool.memfree( aPage ) == IDE_SUCCESS );
        mAllocBuffPageCnt--;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Direct Path Buffer�� ��ȯ�Ѵ�.
 *
 * aPage - [IN] ��ȯ�� �޸� ������ �����ּ�.
 *
 **********************************************************************/
IDE_RC sdbDPathBufferMgr::freeBuffPage( void* aPage )
{
    SInt    sState = 0;

    IDE_TEST( mBuffMtx.lock( NULL /* idvSQL* */) != IDE_SUCCESS );
    sState = 1;

    IDE_ASSERT( mAllocBuffPageCnt != 0 );

    mAllocBuffPageCnt--;
    IDE_TEST( mPageBuffPool.memfree( aPage ) != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( mBuffMtx.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( mBuffMtx.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathBuffInfo�� dump�Ѵ�.
 *
 * Parameters :
 *      aDPathBuffInfo - [IN] dump�� DPathBuffInfo
 ******************************************************************************/
IDE_RC sdbDPathBufferMgr::dumpDPathBuffInfo( sdbDPathBuffInfo *aDPathBuffInfo )
{
    smuList   * sBaseNode;
    smuList   * sCurNode;

    if( aDPathBuffInfo == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     " DPath Buff Info: NULL\n"
                     "-----------------------" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     "  DPath Buffer Info Dump...\n"
                     "Total Page Count  : %llu\n"
                     "Last Space ID     : %u\n"
                     "-----------------------",
                     aDPathBuffInfo->mTotalPgCnt,
                     aDPathBuffInfo->mLstSpaceID );


        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     "  Append Page List Dump...\n"
                     "-----------------------" );

        sBaseNode = &aDPathBuffInfo->mAPgList;
        for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
             sCurNode != sBaseNode;
             sCurNode = SMU_LIST_GET_NEXT(sCurNode) )
        {
            (void)dumpDPathBCB( (sdbDPathBCB*)sCurNode->mData );
        }

        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     "  Flush Request Page List Dump...\n"
                     "-----------------------" );

        sBaseNode = &aDPathBuffInfo->mFReqPgList;
        for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
             sCurNode != sBaseNode;
             sCurNode = SMU_LIST_GET_NEXT(sCurNode) )
        {
            (void)dumpDPathBCB( (sdbDPathBCB*)sCurNode->mData );
        }
    }

    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description : DPathBulkIOInfo�� dump�Ѵ�.
 *
 * Parameters :
 *      aDPathBulkIOInfo - [IN] dump�� DPathBulkIOInfo
 ******************************************************************************/
IDE_RC sdbDPathBufferMgr::dumpDPathBulkIOInfo(
                                    sdbDPathBulkIOInfo *aDPathBulkIOInfo )
{
    smuList   * sBaseNode;
    smuList   * sCurNode;

    if( aDPathBulkIOInfo == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     " DPath Bulk IO Info: NULL\n"
                     "-----------------------" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "-----------------------\n"
                     "  DPath Bulk IO Info Dump...\n"
                     "Real Direct IO Buffer Ptr     : %"ID_XPOINTER_FMT"\n"
                     "Aligned Direct IO Buffer Ptr  : %"ID_XPOINTER_FMT"\n"
                     "Buffer Size                   : %u\n"
                     "IO Request Count              : %u\n"
                     "Last Space ID                 : %u\n"
                     "Last Page ID                  : %u\n"
                     "-----------------------",
                     aDPathBulkIOInfo->mRDIOBuffPtr,
                     aDPathBulkIOInfo->mADIOBuffPtr,
                     aDPathBulkIOInfo->mDBufferSize,
                     aDPathBulkIOInfo->mIORequestCnt,
                     aDPathBulkIOInfo->mLstSpaceID,
                     aDPathBulkIOInfo->mLstPageID );

        sBaseNode = &aDPathBulkIOInfo->mBaseNode;
        for( sCurNode = SMU_LIST_GET_FIRST(sBaseNode);
             sCurNode != sBaseNode;
             sCurNode = SMU_LIST_GET_NEXT(sCurNode) )
        {
            (void)dumpDPathBCB( (sdbDPathBCB*)sCurNode->mData );
        }
    }

    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description : DPathBCB�� ��� ����Ѵ�.
 *
 * Parameters :
 *      aDPathBCB   - [IN] dump�� DPathBCB
 ******************************************************************************/
IDE_RC sdbDPathBufferMgr::dumpDPathBCB( sdbDPathBCB *aDPathBCB )
{
    if( aDPathBCB == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "------------------\n"
                     " DPath BCB: NULL\n"
                     "------------------" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "---------------------\n"
                     "  DPath BCB Dump...\n"
                     "Space ID      : %u\n"
                     "Page ID       : %u\n"
                     "State         : %u\n"
                     "Logging       : %u\n"
                     "---------------------",
                     aDPathBCB->mSpaceID,
                     aDPathBCB->mPageID,
                     aDPathBCB->mState,
                     aDPathBCB->mIsLogging );
    }

    return IDE_SUCCESS;
}

