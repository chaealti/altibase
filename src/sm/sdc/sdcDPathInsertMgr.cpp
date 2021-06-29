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
 * $Id: sdcDPathInsertMgr.cpp 83870 2018-09-03 04:32:39Z kclee $
 ******************************************************************************/

#include <sdcReq.h>
#include <sdcDPathInsertMgr.h>
#include <sdbDPathBufferMgr.h>
#include <sdpDef.h>
#include <sdpDPathInfoMgr.h>

iduMemPool      sdcDPathInsertMgr::mDPathEntryPool;

sdcDPathStat    sdcDPathInsertMgr::mDPathStat;
iduMutex        sdcDPathInsertMgr::mStatMtx;

/*******************************************************************************
 * Description : static �������� �ʱ�ȭ �Ѵ�.
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::initializeStatic()
{
    SInt    sState = 0;

    IDE_TEST( sdbDPathBufferMgr::initializeStatic() != IDE_SUCCESS );
    sState = 1;
    IDE_TEST( sdpDPathInfoMgr::initializeStatic() != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mDPathEntryPool.initialize(
                            IDU_MEM_SM_SDC,
                            (SChar*)"DIRECT_PATH_ENTRY_MEMPOOL",
                            1, /* List Count */
                            ID_SIZEOF( sdcDPathEntry ),
                            16, /* ������ ������ �ִ� Item���� */
                            IDU_AUTOFREE_CHUNK_LIMIT,			/* ChunkLimit */
                            ID_TRUE,							/* UseMutex */
                            IDU_MEM_POOL_DEFAULT_ALIGN_SIZE,	/* AlignSize */
                            ID_FALSE,							/* ForcePooling */
                            ID_TRUE,							/* GarbageCollection */
                            ID_TRUE,                            /* HWCacheLine */
                            IDU_MEMPOOL_TYPE_LEGACY             /* mempool type */) 
              != IDE_SUCCESS);			
    sState = 3;

    // ��� ���� ���� ��, ���ü� ��� ���� Mutex �ʱ�ȭ
    IDE_TEST( mStatMtx.initialize( (SChar*)"DIRECT_PATH_INSERT_STAT_MUTEX",
                                    IDU_MUTEX_KIND_NATIVE,
                                    IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 4;

    // ��� ���� �ڷᱸ�� mDPathStat �ʱ�ȭ
    mDPathStat.mCommitTXCnt    = 0;
    mDPathStat.mAbortTXCnt     = 0;
    mDPathStat.mInsRowCnt      = 0;
    mDPathStat.mAllocBuffPageTryCnt    = 0;
    mDPathStat.mAllocBuffPageFailCnt   = 0;
    mDPathStat.mBulkIOCnt      = 0;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 4:
            IDE_ASSERT( mStatMtx.destroy() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( mDPathEntryPool.destroy() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sdpDPathInfoMgr::destroyStatic() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdbDPathBufferMgr::destroyStatic() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : static �������� �ı��Ѵ�.
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::destroyStatic()
{
    IDE_TEST( sdbDPathBufferMgr::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( sdpDPathInfoMgr::destroyStatic() != IDE_SUCCESS );
    IDE_TEST( mDPathEntryPool.destroy( ID_TRUE ) != IDE_SUCCESS );
    IDE_TEST( mStatMtx.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Transaction�� Direct-Path INSERT ������ �ڷᱸ���� DPathEntry��
 *          �����Ѵ�.
 *
 * Implementation :
 *          1. sdcDPathEntry �ϳ� �޸� �Ҵ�
 *          2. ��� ���� �ʱ�ȭ
 *          3. DPathBuffInfo ����
 *          4. DPathInfo ����
 *          5. aDPathEntry �Ű������� �Ҵ��Ͽ� OUT
 *
 * Parameters :
 *      aDPathEntry - [OUT] ������ DPathEntry�� �Ҵ��Ͽ� ��ȯ�� OUT �Ű�����
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::allocDPathEntry( void **aDPathEntry )
{
    SInt            sState = 0;
    sdcDPathEntry * sDPathEntry;

    IDE_DASSERT( aDPathEntry != NULL );

    // DPathEntry �ϳ��� �޸� �Ҵ��ϰ� �ʱ�ȭ �Ѵ�.
    /* sdcDPathInsertMgr_allocDPathEntry_alloc_DPathEntry.tc */
    IDU_FIT_POINT("sdcDPathInsertMgr::allocDPathEntry::alloc::DPathEntry");
    IDE_TEST( mDPathEntryPool.alloc( (void**)&sDPathEntry ) != IDE_SUCCESS );
    sState = 1;

    sDPathEntry = new ( sDPathEntry ) sdcDPathEntry();

    // DPathBuffInfo�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( sdbDPathBufferMgr::initDPathBuffInfo(&sDPathEntry->mDPathBuffInfo)
              != IDE_SUCCESS );
    sState = 2;

    // DPathInfo�� �ʱ�ȭ�Ѵ�.
    IDE_TEST( sdpDPathInfoMgr::initDPathInfo(&sDPathEntry->mDPathInfo)
              != IDE_SUCCESS );
    sState = 3;

    /* FIT/ART/sm/Projects/PROJ-2068/PROJ-2068.ts  */
    IDU_FIT_POINT( "1.PROJ-2068@sdcDPathInsertMgr::allocDPathEntry" );

    // OUT �Ű������� �޾��ش�.
    *aDPathEntry = (void*)sDPathEntry;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 3:
            IDE_ASSERT( sdpDPathInfoMgr::destDPathInfo(&sDPathEntry->mDPathInfo)
                        == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sdbDPathBufferMgr::destDPathBuffInfo(
                                                &sDPathEntry->mDPathBuffInfo)
                        == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mDPathEntryPool.memfree( sDPathEntry ) == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : Transaction�� Direct-Path INSERT ������ �ڷᱸ���� DPathEntry��
 *          �ı��Ѵ�.
 *
 * Implementation :
 *          1. mDPathBuffInfo �ı�
 *          2. mDPathInfo �ı�
 *          3. DPathEntry �޸� �Ҵ� ����
 *
 * Parameters :
 *      aDPathEntry - [IN] �ı��� DPathEntry
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::destDPathEntry( void *aDPathEntry )
{
    sdcDPathEntry  * sDPathEntry;

    IDE_DASSERT( aDPathEntry != NULL );

    sDPathEntry = (sdcDPathEntry*)aDPathEntry;

    // mDPathBuffInfo �ı�
    IDE_TEST( sdbDPathBufferMgr::destDPathBuffInfo(
                                                &sDPathEntry->mDPathBuffInfo)
              != IDE_SUCCESS );

    // mDPathInfo �ı�
    IDE_TEST( sdpDPathInfoMgr::destDPathInfo(&sDPathEntry->mDPathInfo)
              != IDE_SUCCESS );

    IDE_TEST( mDPathEntryPool.memfree( sDPathEntry ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathSegInfo�� ���´�.
 *          ���⼭ �Ҵ� �� ������ DPathSegInfo�� Commit�ÿ� merge����, �� ��
 *          ���� ������ ��� DPathSegInfo�� �ѹ��� �ı��� �ش�.
 *
 * Implementation :
 *      1. aTrans�� �޷��ִ� DPathEntry�� ���´�.
 *      2. aTableOID�� �����ϴ� DPathSegInfo�� ���� DPathEntry->DPathInfo��
 *         �Ŵް�, ��ȯ�� �ش�.
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aTrans          - [IN] DPath INSERT�� �����ϴ� Transaction�� ������
 *      aTableOID       - [IN] DPath INSERT�� ������ ��� Table�� OID
 *                             �����ϴ� DPathSegInfo�� ������ ���� ��
 *      aDPathSegInfo   - [OUT] ���� DPathSegInfo�� �Ҵ��Ͽ� ��ȯ
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::allocDPathSegInfo(
                                        idvSQL             * aStatistics,
                                        void               * aTrans,
                                        smOID                aTableOID,
                                        void              ** aDPathSegInfo )
{
    smTID               sTID;
    sdcDPathEntry     * sDPathEntry;
    sdpDPathInfo      * sDPathInfo;
    sdpDPathSegInfo   * sDPathSegInfo;

    IDE_DASSERT( aStatistics != NULL );
    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTableOID != SM_NULL_OID );
    IDE_DASSERT( aDPathSegInfo != NULL );

    sDPathSegInfo = (sdpDPathSegInfo*)*aDPathSegInfo;
    
    // DPathEntry�� ���´�.
    sDPathEntry = (sdcDPathEntry*)smLayerCallback::getDPathEntry( aTrans );
    if( sDPathEntry == NULL )
    {
        // aTrans�� ���Ͽ� allocDPathEntry�� ���� �������� ���� ���¿���
        // allocDPathSegInfo�� ȣ��� ��Ȳ.
        //
        // allocDPathEntry�� ���� ȣ���� �־�� �Ѵ�.
        sTID = smLayerCallback::getTransID( aTrans );
        ideLog::log( IDE_SERVER_0,
                     "Trans ID  : %u",
                     sTID );
        IDE_ASSERT( 0 );
    }

    // DPathSegInfo�� ���´�.
    sDPathInfo = &sDPathEntry->mDPathInfo;

    IDE_TEST( sdpDPathInfoMgr::createDPathSegInfo(aStatistics,
                                                  aTrans,
                                                  aTableOID,
                                                  sDPathInfo,
                                                  &sDPathSegInfo)
              != IDE_SUCCESS );

    *aDPathSegInfo = sDPathSegInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : BUG-30109 Direct-Path Buffer�� APPEND PAGE LIST�� �޷��ִ�
 *          ������ ���� ���ο� ������ �Ҵ� �õ��� ���� ��, ������ �Ҵ�� ������
 *          �� Update Only �������� Ȯ���Ͽ� setDirtyPage�� �����Ѵ�.
 *           �� Ŀ�� �������� ���������� �Ҵ�� ��������, �� �� �߰����� ������
 *          �Ҵ��� �̷�� ���� �ʱ� ������ setDirtyPage�� ��ȸ�� ���� �� ����.
 *          ���� Cursor�� close �� ��, aDPathSegInfo���� ���������� �Ҵ��
 *          �������� ������ setDirtyPage()�� ������ �־�� �Ѵ�.
 * 
 * Parameters :
 *      aDPathSegInfo   - [IN] sdpDPathSegInfo�� ������
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::setDirtyLastAllocPage( void *aDPathSegInfo )
{
    sdpDPathSegInfo   * sDPathSegInfo;

    IDE_DASSERT( aDPathSegInfo != NULL );

    sDPathSegInfo = (sdpDPathSegInfo*)aDPathSegInfo;

    if( sDPathSegInfo->mLstAllocPagePtr != NULL )
    {
        IDE_TEST( sdbDPathBufferMgr::setDirtyPage(
                                        sDPathSegInfo->mLstAllocPagePtr )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTID�� �ش��ϴ� Transaction�� DPath INSERT�� commit �۾���
 *          ������ �ش�.
 *
 * Implementation :
 *          1. aTID�� �ش��ϴ� DPathEntry ���
 *          2. Flush ����
 *          3. Merge ����
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aDathEntry      - [IN] Commit �۾��� ������ DPathEntry
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::commit( idvSQL  * aStatistics,
                                  void    * aTrans,
                                  void    * aDPathEntry )
{
    SInt            sState = 0;
    sdcDPathEntry * sDPathEntry;

    IDE_DASSERT( aStatistics != NULL );
    IDE_DASSERT( aDPathEntry != NULL );

    // DPathEntry�� ���´�.
    sDPathEntry = (sdcDPathEntry*)aDPathEntry;

    // ��� Page�� Flush �Ѵ�.
    IDE_TEST_RAISE( sdbDPathBufferMgr::flushAllPage(
                                        aStatistics,
                                        &sDPathEntry->mDPathBuffInfo)
                    != IDE_SUCCESS, commit_failed );

    // DPath INSERT�� ������ ��� Segment�� ��ȭ�� merge�� �ش�.
    IDE_TEST_RAISE( sdpDPathInfoMgr::mergeAllSegOfDPathInfo(
                                                 aStatistics,
                                                 aTrans,
                                                 &sDPathEntry->mDPathInfo)
                    != IDE_SUCCESS, commit_failed );

    // X$DIRECT_PATH_INSERT - COMMIT_TX_COUNT
    IDE_TEST( mStatMtx.lock( NULL ) != IDE_SUCCESS );
    sState = 1;

    mDPathStat.mCommitTXCnt++;
    mDPathStat.mInsRowCnt += sDPathEntry->mDPathInfo.mInsRowCnt;
    mDPathStat.mAllocBuffPageTryCnt +=
                            sDPathEntry->mDPathBuffInfo.mAllocBuffPageTryCnt;
    mDPathStat.mAllocBuffPageFailCnt +=
                            sDPathEntry->mDPathBuffInfo.mAllocBuffPageFailCnt;
    mDPathStat.mBulkIOCnt += sDPathEntry->mDPathBuffInfo.mBulkIOCnt;

    sState = 0;
    IDE_TEST( mStatMtx.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( commit_failed );
    {
        (void)dumpDPathEntry( sDPathEntry );
        IDE_ASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( mStatMtx.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTID�� �ش��ϴ� Transaction�� DPath INSERT�� abort �۾���
 *          ������ �ش�.
 *
 * Implementation :
 *          1. DPathBuffInfo�� ������ ������ ��� ����
 *
 * Parameters :
 *      aStatistics     - [IN] ���
 *      aDPathEntry     - [IN] Abort �۾��� ������ ��� DPathEntry
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::abort( void * aDPathEntry )
{
    SInt                sState = 0;
    sdcDPathEntry     * sDPathEntry;

    IDE_DASSERT( aDPathEntry != NULL );

    // DPathEntry�� ���´�.
    sDPathEntry = (sdcDPathEntry*)aDPathEntry;

    IDE_TEST( sdbDPathBufferMgr::cancelAll(&sDPathEntry->mDPathBuffInfo)
              != IDE_SUCCESS );

    // X$DIRECT_PATH_INSERT - ABORT_TX_COUNT
    IDE_TEST( mStatMtx.lock( NULL ) != IDE_SUCCESS );
    sState = 1;

    mDPathStat.mAbortTXCnt++;
    mDPathStat.mAllocBuffPageTryCnt +=
                            sDPathEntry->mDPathBuffInfo.mAllocBuffPageTryCnt;
    mDPathStat.mAllocBuffPageFailCnt +=
                            sDPathEntry->mDPathBuffInfo.mAllocBuffPageFailCnt;
    mDPathStat.mBulkIOCnt += sDPathEntry->mDPathBuffInfo.mBulkIOCnt;

    sState = 0;
    IDE_TEST( mStatMtx.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    switch( sState )
    {
        case 1:
            IDE_ASSERT( mStatMtx.unlock() == IDE_SUCCESS );
        default:
            break;
    }
    IDE_POP();

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : aTrans�� �ش��ϴ� Transaction�� DPathBuffInfo�� ��ȯ�Ѵ�.
 * 
 * Implementation :
 *          1. aTrans�� �ش��ϴ� DPathEntry ���
 *          2. DPathBuffInfo Ȯ�� �� ��ȯ
 *
 * Parameters :
 *      aTrans  - [IN] ã�� ��� DPathEntry�� Transaction
 *
 * Return : �����ϸ� DPathBuffInfo�� ������, ������ NULL
 ******************************************************************************/
void* sdcDPathInsertMgr::getDPathBuffInfo( void *aTrans )
{
    sdcDPathEntry     * sDPathEntry;
    sdbDPathBuffInfo  * sDPathBuffInfo = NULL;

    IDE_DASSERT( aTrans != NULL );

    sDPathEntry = (sdcDPathEntry*)smLayerCallback::getDPathEntry( aTrans );

    if( sDPathEntry != NULL )
    {
        sDPathBuffInfo = &sDPathEntry->mDPathBuffInfo;
    }

    return sDPathBuffInfo;
}

/*******************************************************************************
 * Description : aTrans�� �ش��ϴ� DPathInfo�� ã���ش�.
 *
 * Parameters :
 *      aTrans    - [IN] ã�� DPathInfo�� Transaction
 *
 * Return : �����ϸ� DPathInfo�� ������, ������ NULL
 ******************************************************************************/
void* sdcDPathInsertMgr::getDPathInfo( void *aTrans )
{
    sdcDPathEntry  * sDPathEntry;
    sdpDPathInfo   * sDPathInfo = NULL;

    IDE_DASSERT( aTrans != NULL );

    sDPathEntry = (sdcDPathEntry*)smLayerCallback::getDPathEntry( aTrans );

    if( sDPathEntry != NULL )
    {
        sDPathInfo = &sDPathEntry->mDPathInfo;
    }

    return sDPathInfo;
}

/*******************************************************************************
 * Description : aTID�� �ش��ϴ� DPathInfo�� ã���ش�.
 *
 * Parameters :
 *      aTrans      - [IN] ã�� DPathInfo�� Transaction
 *      aTableOID   - [IN] ã�� DPathSegInfo�� ��� Table�� OID
 *
 * Return : �����ϸ� DPathSegInfo�� ������, ������ NULL
 ******************************************************************************/
void* sdcDPathInsertMgr::getDPathSegInfo( void    * aTrans,
                                          smOID     aTableOID )
{
    sdcDPathEntry    * sDPathEntry;
    sdpDPathInfo     * sDPathInfo;
    sdpDPathSegInfo  * sDPathSegInfo = NULL;

    IDE_DASSERT( aTrans != NULL );
    IDE_DASSERT( aTableOID != SM_NULL_OID );

    sDPathEntry = (sdcDPathEntry*)smLayerCallback::getDPathEntry( aTrans );

    if( sDPathEntry != NULL )
    {
        sDPathInfo = &sDPathEntry->mDPathInfo;
        sDPathSegInfo = sdpDPathInfoMgr::findLastDPathSegInfo( sDPathInfo,
                                                               aTableOID );
    }

    return sDPathSegInfo;
}

/*******************************************************************************
 * Description : X$DIRECT_PATH_INSERT�� ���� ��� ���� ��ȯ�Ѵ�.
 *
 * Parameters :
 *      aDPathStat  - [OUT] DPath ��� ���� �Ҵ��Ͽ� ��ȯ���� OUT �Ķ����
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::getDPathStat( sdcDPathStat *aDPathStat )
{
    SInt    sState = 0;

    IDE_DASSERT( aDPathStat != NULL );

    IDE_TEST( mStatMtx.lock( NULL ) != IDE_SUCCESS );
    sState = 1;
    *aDPathStat = mDPathStat;
    sState = 0;
    IDE_TEST( mStatMtx.unlock() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        IDE_ASSERT( mStatMtx.unlock() == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : DPathEntry ���� dump �ϱ� ���� �Լ�
 *
 * Parameters :
 *      aDPathEntry - [IN] dump�� DPathEntry
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::dumpDPathEntry( sdcDPathEntry *aDPathEntry )
{
    if( aDPathEntry == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "========================================\n"
                     " DPathEntry: NULL\n"
                     "========================================" );
    }
    else
    {
        ideLog::log( IDE_SERVER_0,
                     "========================================\n"
                     " DPath Entry Dump Begin...\n"
                     "========================================" );

        (void)sdbDPathBufferMgr::dumpDPathBuffInfo(
                                            &aDPathEntry->mDPathBuffInfo );
        (void)sdpDPathInfoMgr::dumpDPathInfo( &aDPathEntry->mDPathInfo );
    }

    ideLog::log( IDE_SERVER_0,
                 "========================================\n"
                 " DPath Entry Dump End...\n"
                 "========================================" );

    return IDE_SUCCESS;
}

/*******************************************************************************
 * Description : aTrans�� �޸� DPathEntry ���� dump �ϱ� ���� �Լ�
 *
 * Parameters :
 *      aTrans  - [IN] �Ŵ޸� DPathEntry�� dump�� Transaction
 ******************************************************************************/
IDE_RC sdcDPathInsertMgr::dumpDPathEntry( void *aTrans )
{
    sdcDPathEntry     * sDPathEntry;

    if( aTrans == NULL )
    {
        ideLog::log( IDE_SERVER_0,
                     "========================================\n"
                     " Transaction  : NULL\n"
                     "========================================" );
    }
    else
    {
        sDPathEntry = (sdcDPathEntry*)smLayerCallback::getDPathEntry( aTrans );

        (void)dumpDPathEntry( sDPathEntry );
    }

    return IDE_SUCCESS;
}

