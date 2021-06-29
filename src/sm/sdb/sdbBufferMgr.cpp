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
 * $$Id: sdbBufferMgr.cpp 85333 2019-04-26 02:34:41Z et16 $
 **********************************************************************/

/***********************************************************************
 * PROJ-1568 BUFFER MANAGER RENEWAL
 **********************************************************************/
#include <idl.h>
#include <ide.h>
#include <smDef.h>
#include <sdbDef.h>
#include <sdbBCB.h>
#include <sdbReq.h>
#include <iduLatch.h>
#include <smErrorCode.h>

#include <sdbBufferPool.h>
#include <sdbFlushMgr.h>
#include <sdbBufferArea.h>
#include <sdbMPRMgr.h>

#include <sdbBufferMgr.h>
#include <smuQueueMgr.h>

#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h> 

#define SDB_BCB_LIST_MIN_LENGTH  (32)

UInt          sdbBufferMgr::mPageCount;
iduMutex      sdbBufferMgr::mBufferMgrMutex;
sdbBufferArea sdbBufferMgr::mBufferArea;
sdbBufferPool sdbBufferMgr::mBufferPool;

/******************************************************************************
 * Description :
 *  BufferManager�� �ʱ�ȭ �Ѵ�.
 ******************************************************************************/
IDE_RC sdbBufferMgr::initialize()
{
    UInt    sHashBucketDensity;
    UInt    sHashLatchDensity;
    UInt    sLRUListCnt;
    UInt    sPrepareListCnt;
    UInt    sFlushListCnt;
    UInt    sCPListCnt;

    ULong   sAreaSize;
    ULong   sAreaChunkSize;
    UInt    sBCBCntPerAreaChunk;
    UInt    sAreaChunkCnt;

    UInt    sTotalBCBCnt;
    UInt    sHashBucketCnt;

    UInt    sState = 0;

    sdbBCB *sFirstBCB = NULL;
    sdbBCB *sLastBCB  = NULL;

    UInt    sPageSize = SD_PAGE_SIZE;
    UInt    sBCBCnt;

    SChar   sMutexName[128];

    sdbBCB::mTouchUSecInterval = (ULong)smuProperty::getTouchTimeInterval() * (ULong)1000000;

    // �ϳ��� hash bucket�� ���� BCB����
    sHashBucketDensity  = smuProperty::getBufferHashBucketDensity();
    // �ϳ��� hash chains latch�� BCB����
    sHashLatchDensity   = smuProperty::getBufferHashChainLatchDensity();
    sLRUListCnt         = smuProperty::getBufferLRUListCnt();
    sPrepareListCnt     = smuProperty::getBufferPrepareListCnt();
    sFlushListCnt       = smuProperty::getBufferFlushListCnt();
    sCPListCnt          = smuProperty::getBufferCheckPointListCnt();

    sAreaSize           = smuProperty::getBufferAreaSize();
    sAreaChunkSize      = smuProperty::getBufferAreaChunkSize();

    if (sAreaSize < sAreaChunkSize)
    {
        sAreaChunkSize = sAreaSize;
    }

    // ����ڰ� ���� sAreaSize�� �״�� ���۸Ŵ����� ũ�Ⱑ ���� �ʰ�,
    // ������ size�� buffer area chunkũ��� align down�ȴ�.
    sTotalBCBCnt    = sAreaSize / sPageSize;
    sHashBucketCnt  = sTotalBCBCnt / sHashBucketDensity;

    sBCBCntPerAreaChunk = sAreaChunkSize / sPageSize;

    // align down
    sAreaChunkCnt   = sTotalBCBCnt / sBCBCntPerAreaChunk;
    sTotalBCBCnt    = sAreaChunkCnt * sBCBCntPerAreaChunk;
    mPageCount      = sAreaChunkCnt * sBCBCntPerAreaChunk;

    // ����Ʈ ���� ����: ���� �����Ǵ� BCB������ ����
    // ����Ʈ�� ������ ����ġ�� ���� ��쿡 �� ����Ʈ ������ ����ؼ� ����.
    // ����Ʈ �ϳ��� �ּ� SDB_BCB_LIST_MIN_LENGTH ��ŭ BCB�� ������ �Ѵ�.
    if (mPageCount <= SDB_SMALL_BUFFER_SIZE)
    {
        sLRUListCnt     = 1;
        sPrepareListCnt = 1;
        sFlushListCnt   = 1;
    }
    else
    {
        while ((sLRUListCnt + sPrepareListCnt + sFlushListCnt)
               * SDB_BCB_LIST_MIN_LENGTH > mPageCount)
        {
            if (sLRUListCnt > 1)
            {
                sLRUListCnt /= 2;
            }
            if (sPrepareListCnt > 1)
            {
                sPrepareListCnt /= 2;
            }
            if (sFlushListCnt > 1)
            {
                sFlushListCnt /= 2;
            }

            if ((sLRUListCnt     == 1) &&
                (sPrepareListCnt == 1) &&
                (sFlushListCnt   == 1))
            {
                break;
            }
        }
    }

    idlOS::snprintf( sMutexName, 128, "BUFFER_MANAGER_EXPAND_MUTEX");
    IDE_TEST( sdbBufferMgr::mBufferMgrMutex.initialize(
                  sMutexName,
                  IDU_MUTEX_KIND_NATIVE,
                  IDV_WAIT_INDEX_LATCH_FREE_DRDB_BUFFER_MANAGER_EXPAND_MUTEX)
              != IDE_SUCCESS );
    sState =1;

    // mBufferPool�� ó���� �����ϸ� ���� BCB�� �������� �ʴ´�.
    // �׷��Ƿ� mBufferArea���� �����ٰ� mBufferPool�� �־��ش�.
    // �̰��� ���Ŀ� Buffer Pool�� multiple buffer pool��
    // �� ��츦 ����� ���̴�.(����� buffer pool�� ������ �Ѱ��̴�.)
    IDE_TEST( sdbBufferMgr::mBufferArea.initialize( sBCBCntPerAreaChunk,
                                                    sAreaChunkCnt,
                                                    sPageSize )
              != IDE_SUCCESS );
    sState =2;

    IDE_TEST( sdbBufferMgr::mBufferPool.initialize( sTotalBCBCnt,
                                                    sHashBucketCnt,
                                                    sHashLatchDensity,
                                                    sLRUListCnt,
                                                    sPrepareListCnt,
                                                    sFlushListCnt,
                                                    sCPListCnt)
              != IDE_SUCCESS );
    sState =3;

    sdbBufferMgr::mBufferArea.getAllBCBs( NULL,
                                          &sFirstBCB,
                                          &sLastBCB,
                                          &sBCBCnt);

    IDE_DASSERT( sBCBCnt == (sAreaChunkCnt * sBCBCntPerAreaChunk));

    sdbBufferMgr::mBufferPool.distributeBCB2PrepareLsts( NULL,
                                                         sFirstBCB,
                                                         sLastBCB,
                                                         sBCBCnt);

    IDE_TEST( sdbMPRMgr::initializeStatic() != IDE_SUCCESS );
    sState = 4;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    switch(sState)
    {
        case 4:
            (void)sdbMPRMgr::destroyStatic();
        case 3:
            sdbBufferMgr::mBufferPool.destroy();
        case 2:
            IDE_ASSERT( sdbBufferMgr::mBufferArea.destroy()  == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( sdbBufferMgr::mBufferMgrMutex.destroy() == IDE_SUCCESS );
        default:
            break;
    }
    return IDE_FAILURE;
}


/******************************************************************************
 * Description :
 *  BufferManager�� �����Ѵ�.
 ******************************************************************************/
IDE_RC sdbBufferMgr::destroy(void)
{
    IDE_ASSERT( sdbMPRMgr::destroyStatic() == IDE_SUCCESS );
    IDE_ASSERT( sdbBufferMgr::mBufferPool.destroy() == IDE_SUCCESS );
    IDE_ASSERT( sdbBufferMgr::mBufferArea.destroy() == IDE_SUCCESS );
    IDE_ASSERT( sdbBufferMgr::mBufferMgrMutex.destroy() == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/******************************************************************************
 * Description :
 *  BufferManager ũ�⸦ �����Ѵ�.
 *  ���� ���۸Ŵ����� ũ�⸦ ���̴� ����� �������� �ʴ´�.
 *  ���� ���̰� �ʹٸ� ������ ������, ���� �� �ٽ� ������ �÷��� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aAskedSize  - [IN]  ����ڰ� ��û�� ũ��
 *  aTrans      - [IN]  �ش� ������ ������ Ʈ�����
 *  aNewSize    - [OUT] resize�ǰ� �� ���� ���� buffer manager ũ��
 ******************************************************************************/
IDE_RC sdbBufferMgr::resize( idvSQL *aStatistics,
                             ULong   aAskedSize,
                             void   *aTrans ,
                             ULong  *aNewSize)
{
    ULong sAddSize;
    ULong sBufferAreaSize ;

    sBufferAreaSize = mBufferArea.getTotalCount() * SD_PAGE_SIZE;
    if( sBufferAreaSize > aAskedSize )
    {
        IDE_RAISE( shrink_func_is_not_supported );
    }
    else
    {
        sAddSize = aAskedSize - sBufferAreaSize;

        IDE_TEST( expand( aStatistics, sAddSize, aTrans,aNewSize )
                  != IDE_SUCCESS );

        ideLog::log(SM_TRC_LOG_LEVEL_BUFFER, SM_TRC_BUFFER_MGR_RESIZE,
                    aAskedSize, *aNewSize );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( shrink_func_is_not_supported );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_ILLEGAL_REQ_BUFFER_SIZE));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/******************************************************************************
 * Description :
 *  BufferManager�� ũ�⸦ ���� �Ѵ�.
 *
 * Implementation :
 *  1. "aAddedSize+���� ũ��"�� ���� ���� Ȯ��Ǿ��� ũ�⸦ ���Ѵ�.
 *  2. buffer pool�� hash table�� ũ�⸦ ���� Ȯ��Ǿ��� ũ�⿡ �°�
 *      ���� ���� ��Ų��. hash table�� ������Ű������ hash table��
 *      ������ ���ɼ��� �ִ� ��� Ʈ����� �� gc�� ������ ���´�.
 *  3. buffer area�� aAddedSize��ŭ�� ��û�Ѵ�.
 *  4. buffer area�� ���� BCB���� Buffer Pool�� �־��ش�.
 *
 *  aStatistics - [IN]  �������
 *  aAddedSize  - [IN]  ���� BufferManager�� �����Ǵ� ũ��. ������
 *                      sdbBufferArea�� chunkũ�⿡ ���߾� aling down
 *                      �Ǿ ��������.
 *  aTrans      - [IN]  �ش� ������ ������ Ʈ�����
 *  aNewSize    - [OUT] expand�ǰ� �� ���� BufferManager�� ũ�⸦ ����
 ******************************************************************************/
IDE_RC sdbBufferMgr::expand( idvSQL *aStatistics,
                             ULong   aAddedSize ,
                             void   *aTrans,
                             ULong  *aNewSize)
{
    UInt    sChunkPageCnt;
    UInt    sAddedChunkCnt;

    UInt    sCurrBCBCnt;
    UInt    sAddedBCBCnt;
    UInt    sTotalBCBCnt;

    sdbBCB *sFirstBCB;
    sdbBCB *sLastBCB;
    UInt    sCount;

    UInt    sHashBucketCnt;
    UInt    sHashBucketCntPerLatch = 0;

    UInt    sState      = 0;

    idBool  sSuccess    = ID_FALSE;

    sCurrBCBCnt     = sdbBufferMgr::mBufferArea.getTotalCount();
    sChunkPageCnt   = sdbBufferMgr::mBufferArea.getChunkPageCount();

    sAddedBCBCnt    = aAddedSize / SD_PAGE_SIZE;

    //align down
    sAddedChunkCnt  = sAddedBCBCnt / sChunkPageCnt;

    if( sAddedChunkCnt == 0 )
    {
        IDE_RAISE( invalid_buffer_expand_size );
    }
    else
    {
        //  1. "aAddedSize+���� ũ��"�� ���� ���� Ȯ��Ǿ��� ũ�⸦ ���Ѵ�.
        sTotalBCBCnt   = sCurrBCBCnt + sAddedChunkCnt * sChunkPageCnt;
        sHashBucketCnt = sTotalBCBCnt / smuProperty::getBufferHashBucketDensity();
        sHashBucketCntPerLatch = smuProperty::getBufferHashChainLatchDensity();

        // 2. buffer pool�� hash table�� ũ�⸦ ���� Ȯ��Ǿ��� ũ�⿡ �°�
        //    ���� ���� ��Ų��. hash table�� ������Ű������ hash table��
        //    ������ ���ɼ��� �ִ� ��� Ʈ����� �� gc�� ������ ���´�.
        blockAllApprochBufHash( aTrans, &sSuccess );

        // Ʈ����� block�� �����Ѵٸ� ������ �����ϰ� ���̻�
        // resize�� �������� �ʴ´�.
        IDE_TEST_RAISE ( sSuccess == ID_FALSE, buffer_manager_busy );
        sState = 1;

        IDE_TEST( sdbBufferMgr::mBufferPool.resizeHashTable( sHashBucketCnt,
                                                             sHashBucketCntPerLatch)
                  != IDE_SUCCESS );

        // 3. buffer area�� aAddedSize��ŭ�� ��û�Ѵ�.
        IDE_TEST( sdbBufferMgr::mBufferArea.expandArea( aStatistics, sAddedChunkCnt)
                  != IDE_SUCCESS );

        // 4. buffer area�� ���� BCB���� Buffer Pool�� �־��ش�.
        IDE_ASSERT( sdbBufferMgr::mBufferArea.getTotalCount() == sTotalBCBCnt );

        if( aNewSize != NULL )
        {
            *aNewSize = sTotalBCBCnt * SD_PAGE_SIZE;
        }

        sdbBufferMgr::mBufferArea.getAllBCBs( aStatistics,
                                              &sFirstBCB,
                                              &sLastBCB,
                                              &sCount);

        sdbBufferMgr::mBufferPool.expandBufferPool( aStatistics,
                                                    sFirstBCB,
                                                    sLastBCB,
                                                    sCount );

        unblockAllApprochBufHash();
        sState = 0;
        /* ����!!
         * �ϴ� bufferPool�� distributBCB�� �����ϰ� ����,
         * rollback�� �� ���� ����. �ֳĸ�, bufferPool�� ������ �ϰ� ����,
         * �ٸ� Ʈ����ǵ��� �ٷ� �������� ������� �����̴�. �׷��� ������
         * distributeBCB���Ŀ� abort�� ���� ������ �׿��� �Ѵ�.
         * �̰��� ���۸Ŵ��� ��ұ�ɰ� ������ ������ �ִ� �κ�����,
         * ���۸Ŵ��� ��� ����� �߰��Ǹ� �ذ�ȴ�.
         * */
        return IDE_SUCCESS;
    }

    IDE_EXCEPTION( invalid_buffer_expand_size );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_INVALID_BUFFER_EXPAND_SIZE ));
    }
    IDE_EXCEPTION( buffer_manager_busy);
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_BUFFER_MANAGER_BUSY));
    }
    IDE_EXCEPTION_END;

    if( sState == 1 )
    {
        unblockAllApprochBufHash();
    }

    return IDE_FAILURE;
}
/******************************************************************************
 * Description :
 *  BufferManager�� �����ϴ� Dirty BCB���� flush�Ͽ� clean���·� �����.
 *  �̶�, ��� BCB�� ���ؼ� �ϴ� ���� �ƴϰ�, checkpoint ����Ʈ�� �޷��ְ�,
 *  restart recovery LSN�� ���� �ͺ��� ������� flush�Ѵ�.
 *  �ַ� �ֱ����� üũ ����Ʈ�� ���� ȣ��ȴ�.
 *
 *  aStatistics - [IN]  �������
 *  aFlushAll   - [IN]  ��� �������� flush �ؾ��ϴ��� ����
 ******************************************************************************/
IDE_RC sdbBufferMgr::flushDirtyPagesInCPList( idvSQL  * aStatistics,
                                              idBool    aFlushAll)
{
    IDE_TEST(sdbFlushMgr::flushDirtyPagesInCPList( aStatistics,
                                                   aFlushAll,
                                                   ID_FALSE )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbBufferMgr::flushDirtyPagesInCPListByCheckpoint( idvSQL  * aStatistics,
                                                          idBool    aFlushAll)
{
    IDE_TEST(sdbFlushMgr::flushDirtyPagesInCPList( aStatistics,
                                                   aFlushAll,
                                                   ID_TRUE )
             != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************************
 * Description :
 *  ���� sdbBufferPool�� ���� �Լ��� ȣ���ϴ� ������ ��Ȱ�� �Ѵ�.
 *  �ڼ��� ������ sdbBufferPool�� ���� �Լ��� ����
 *
 *  aBCB        - [IN]  �ش� BCB
 *  aMtx        - [IN]  �ش� aMtx
 ****************************************************************/
IDE_RC sdbBufferMgr::setDirtyPage( void *aBCB,
                                   void *aMtx )
{

    idvSQL *sStatistics = NULL;
    sStatistics = (idvSQL*)smLayerCallback::getStatSQL( aMtx );

    mBufferPool.setDirtyPage( sStatistics, aBCB, aMtx);

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  ���� sdbBufferPool�� ���� �Լ��� ȣ���ϴ� ������ ��Ȱ�� �Ѵ�.
 *  �ڼ��� ������ sdbBufferPool�� ���� �Լ��� ����
 *
 *  aBCB        - [IN]  �ش� BCB
 *  aLatchMode  - [IN]  ��ġ ���
 *  aMtx        - [IN]  �ش� Mini transaction
 ****************************************************************/
IDE_RC sdbBufferMgr::releasePage( void            *aBCB,
                                  UInt             aLatchMode,
                                  void            *aMtx )
{
    idvSQL  *sStatistics;

    sStatistics= (idvSQL*)smLayerCallback::getStatSQL( aMtx );

    mBufferPool.releasePage( sStatistics, aBCB, aLatchMode );

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  ���� sdbBufferPool�� ���� �Լ��� ȣ���ϴ� ������ ��Ȱ�� �Ѵ�.
 *  �ڼ��� ������ sdbBufferPool�� ���� �Լ��� ����
 *
 *  aStatistics - [IN]  �������
 *  aPagePtr    - [IN]  �ش� ������ frame pointer
 ****************************************************************/
IDE_RC sdbBufferMgr::releasePage( idvSQL * aStatistics,
                                  UChar  * aPagePtr )
{
    sdbBCB *sBCB;

    sBCB = getBCBFromPagePtr( aPagePtr );

    mBufferPool.releasePage( aStatistics, sBCB );

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  ���� sdbBufferPool�� ���� �Լ��� ȣ���ϴ� ������ ��Ȱ�� �Ѵ�.
 *  �ڼ��� ������ sdbBufferPool�� ���� �Լ��� ����
 *
 *  aBCB        - [IN]  �ش� BCB
 *  aLatchMode  - [IN]  ��ġ ���
 *  aMtx        - [IN]  �ش� Mini transaction
 ****************************************************************/
IDE_RC sdbBufferMgr::releasePageForRollback( void         *aBCB,
                                             UInt          aLatchMode,
                                             void         *aMtx)
{
    idvSQL  *sStatistics;

    sStatistics = (idvSQL*)smLayerCallback::getStatSQL( aMtx );

    mBufferPool.releasePageForRollback( sStatistics, aBCB, aLatchMode );

    return IDE_SUCCESS;
}

void sdbBufferMgr::setDiskAgerStatResetFlag(idBool  )
{
    //TODO: 1568
    //IDE_ASSERT(0);
}

/****************************************************************
 * Description :
 *  ������ ������ �����͸� �Ѱ� �޾� �װͿ� �ش��ϴ� BCB�� �����Ѵ�.
 *
 *  aPage       - [IN]  �ش� ������ frame pointer
 ****************************************************************/
sdbBCB* sdbBufferMgr::getBCBFromPagePtr( UChar * aPage )
{
    sdbBCB *sBCB;
    UChar  *sPagePtr;

    IDE_ASSERT( aPage != NULL );

    sPagePtr = smLayerCallback::getPageStartPtr( aPage );
    sBCB     = (sdbBCB*)(((sdbFrameHdr*)sPagePtr)->mBCBPtr);

    if( mBufferArea.isValidBCBPtrRange( sBCB ) == ID_FALSE )
    {
        /* BUG-32528 disk page header�� BCB Pointer �� ������ ��쿡 ����
         * ����� ���� �߰�.
         * Disk Page Header�� BCB Pointer �� �������ٸ�
         * �� page���� �ܾ��� ���ɼ��� ����. */

        smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                        sPagePtr - SD_PAGE_SIZE,
                                        "[ Prev Page ]" );
        smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                        sPagePtr,
                                        "[ Curr Page ]" );
        smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                        sPagePtr + SD_PAGE_SIZE,
                                        "[ Next Page ]" );

        sBCB = (sdbBCB*)(((sdbFrameHdr*)(sPagePtr - SD_PAGE_SIZE))->mBCBPtr);
        sdbBCB::dump( sBCB );

        sBCB = (sdbBCB*)(((sdbFrameHdr*)(sPagePtr + SD_PAGE_SIZE))->mBCBPtr);
        sdbBCB::dump( sBCB );

        IDE_ASSERT(0);
    }

    if( sBCB->mSpaceID != ((sdbFrameHdr*)sPagePtr)->mSpaceID )
    {
        smLayerCallback::traceDiskPage( IDE_DUMP_0,
                                        sPagePtr,
                                        NULL );

        sdbBCB::dump( sBCB );

        IDE_ASSERT(0);
    }

    return sBCB;
}



#if 0 // not used
/****************************************************************
 * Description :
 *  GRID�� �ش��ϴ� page frame�� �����͸� �����Ѵ�.
 *  �̶�, �ݵ�� ���ۿ� �����ϴ� ��쿡�� �����ϰ�, �׷��� ������쿡��
 *  ��ũ���� ���� �ʰ� NULL�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aGRID       - [IN]  GRID
 *  aPagePtr    - [OUT] ��û�� ������
 ****************************************************************/
IDE_RC sdbBufferMgr::getPagePtrFromGRID( idvSQL     *aStatistics,
                                         scGRID      aGRID,
                                         UChar     **aPagePtr)
{
    sdbBCB    * sFindBCB;
    UChar     * sSlotDirPtr;

    IDE_DASSERT( SC_GRID_IS_NOT_NULL( aGRID ) );
    IDE_DASSERT(aPagePtr != NULL );

    *aPagePtr = NULL;

    mBufferPool.findBCB( aStatistics,
                         SC_MAKE_SPACE(aGRID),
                         SC_MAKE_PID(aGRID),
                         &sFindBCB);

    if ( sFindBCB != NULL )
    {
        if ( SC_GRID_IS_WITH_SLOTNUM(aGRID) )
        {
            sSlotDirPtr = smLayerCallback::getSlotDirStartPtr(
                                                        sFindBCB->mFrame );

            IDE_TEST( smLayerCallback::getPagePtrFromSlotNum(
                                                        sSlotDirPtr,
                                                        SC_MAKE_SLOTNUM(aGRID),
                                                        aPagePtr )
                       != IDE_SUCCESS );
        }
        else
        {
            *aPagePtr = sFindBCB->mFrame + SC_MAKE_OFFSET(aGRID);
        }
    }
    else
    {
        /* do nothing */
    }

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;

    return IDE_FAILURE;    
}
#endif

/****************************************************************
 * Description :
 *  ���� ���ۿ� �����ϴ� BCB���� recoveryLSN�� ���� ���� ����
 *  �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] ��û�� min recoveryLSN
 ****************************************************************/
void sdbBufferMgr::getMinRecoveryLSN(idvSQL *aStatistics,
                                     smLSN  *aRet)
{
    smLSN         sFlusherMinLSN;
    smLSN         sCPListMinLSN;
    sdbCPListSet *sCPL;

    sCPL = mBufferPool.getCPListSet();

    //����!! ���ü� ���� ������ �Ʒ� 2������ ������ �ݵ�� �����ؾ� �Ѵ�.
    sCPL->getMinRecoveryLSN(aStatistics, &sCPListMinLSN);
    sdbFlushMgr::getMinRecoveryLSN(aStatistics, &sFlusherMinLSN);

    if ( smLayerCallback::isLSNLT( &sCPListMinLSN,
                                   &sFlusherMinLSN )
         == ID_TRUE )
    {
        SM_GET_LSN(*aRet, sCPListMinLSN);
    }
    else
    {
        SM_GET_LSN(*aRet, sFlusherMinLSN);
    }
}

/******************************************************************************
 * Description :
 *    ��� ������ �ý��ۿ� �ݿ��Ѵ�.
 ******************************************************************************/
void sdbBufferMgr::applyStatisticsForSystem()
{
    mBufferPool.applyStatisticsForSystem();
}


/*****************************************************************
 * Description:
 *  [BUG-20861] ���� hash resize�� �ϱ����ؼ� �ٸ� Ʈ����ǵ��� ��� ��������
 *  ���ϰ� �ؾ� �մϴ�.
 *
 *  hash table�� �����Ҽ� �ִ� ��������� ��� ���´�.
 *  �� �Լ� ������ hash�� �����ϴ� �����尡 (�� �Լ��� ������ ������ �ܿ�)
 *  �ƹ��� ������ ���� �� �� �ִ�.
 *
 *  aStatistics - [IN]  �������
 *  aTrans      - [IN]  �� �Լ� ȣ���� Ʈ�����
 *  aSuccess    - [OUT] �������� ����
 ****************************************************************/
void  sdbBufferMgr::blockAllApprochBufHash( void     * aTrans,
                                            idBool   * aSuccess )
{
    ULong  sWaitTimeMicroSec = smuProperty::getBlockAllTxTimeOut() *1000*1000;

    /* Ʈ������� ������ ���´�. */
    smLayerCallback::blockAllTx( aTrans,
                                 sWaitTimeMicroSec,
                                 aSuccess );

    if( *aSuccess == ID_FALSE)
    {
        unblockAllApprochBufHash();
    }

    return;
}

/*****************************************************************
 * Description:
 *  [BUG-20861] ���� hash resize�� �ϱ����ؼ� �ٸ� Ʈ����ǵ��� ��� ��������
 *  ���ϰ� �ؾ� �մϴ�.
 *
 *  sdbBufferMgr::blockAllApprochBufHash ���� ���Ƴ��Ҵ�
 *  ��������� �����Ѵ�. �� �Լ� ȣ�����Ŀ� hash�� ��������� ������ �� �ִ�.
 *
 *  aTrans      - [IN]  �� �Լ� ȣ���� Ʈ�����
 ****************************************************************/
void  sdbBufferMgr::unblockAllApprochBufHash()
{
    smLayerCallback::unblockAllTx();
}

/*****************************************************************
 * Description :
 *  � BCB�� �Է����� �޾Ƶ� �׻� ID_TRUE�� �����Ѵ�.
 ****************************************************************/
idBool sdbBufferMgr::filterAllBCBs( void   */*aBCB*/,
                                    void   */*aObj*/)
{
    return ID_TRUE;
}

/*****************************************************************
 * Description :
 *  aObj���� spaceID�� ����ִ�. spaceID�� ���� BCB�� ���ؼ�
 *  ID_TRUE�� ����
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �Լ� �����Ҷ� �ʿ��� �ڷᱸ��. spaceID�� ĳ�����ؼ� ���
 ****************************************************************/
idBool sdbBufferMgr::filterTBSBCBs( void   *aBCB,
                                    void   *aObj )
{
    scSpaceID *sSpaceID = (scSpaceID*)aObj;
    sdbBCB    *sBCB     = (sdbBCB*)aBCB;

    if( sBCB->mSpaceID == *sSpaceID )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}

/*****************************************************************
 * Description :
 *  aObj���� Ư�� pid�� ������ ����ִ�.
 *  BCB�� �� ������ ���Ҷ��� ID_TRUE ����.
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �Լ� �����Ҷ� �ʿ��� �ڷᱸ��. sdbBCBRange�� ĳ�����ؼ�
 *                      ���
 ****************************************************************/
idBool sdbBufferMgr::filterBCBRange( void   *aBCB,
                                     void   *aObj )
{
    sdbBCBRange *sRange = (sdbBCBRange*)aObj;
    sdbBCB      *sBCB   = (sdbBCB*)aBCB;

    if( sRange->mSpaceID == sBCB->mSpaceID )
    {
        if( (sRange->mStartPID <= sBCB->mPageID) &&
            (sBCB->mPageID <= sRange->mEndPID ))
        {
            return ID_TRUE;
        }
    }
    return ID_FALSE;
}

/****************************************************************
 * �־��� ���ǿ� �´� �� �ϳ��� BCB�� �����ϱ� ���� �Լ�,
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �Լ� �����Ҷ� �ʿ��� �ڷᱸ��. Ư�� BCB������ ������.
 ****************************************************************/
idBool sdbBufferMgr::filterTheBCB( void   *aBCB,
                                   void   *aObj )
{
    sdbBCB *sCmpBCB = (sdbBCB*)aObj;
    sdbBCB *sBCB = (sdbBCB*)aBCB;    

    if((sBCB->mPageID  == sCmpBCB->mPageID ) &&
       (sBCB->mSpaceID == sCmpBCB->mSpaceID))
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}


/****************************************************************
 * Description :
 *  �������� ���۳����� �����Ѵ�. ���� BCB�� dirty��� flush�����ʰ�
 *  ������ ���� ������. ���̺� drop�Ǵ� ���̺� �����̽� drop���꿡
 *  �ش��ϴ� BCB�� �̿����� ���ؼ� ���۳����� �����ϴ� ���� ����.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 ****************************************************************/
IDE_RC sdbBufferMgr::removePageInBuffer( idvSQL     *aStatistics,
                                         scSpaceID   aSpaceID,
                                         scPageID    aPageID )
{
    sdbBCB  *sBCB = NULL;
    sdbBCB   sCmpBCB;
    idBool   sIsSuccess;
    idBool   sDummy;

    IDE_TEST( mBufferPool.findBCB( aStatistics,
                                   aSpaceID,
                                   aPageID,
                                   &sBCB )
              != IDE_SUCCESS );
 
    if ( sBCB != NULL )
    {
        sCmpBCB.mSpaceID = aSpaceID;
        sCmpBCB.mPageID  = aPageID;

        // ������ �ٲ�� �����Ƿ� filter������ �Ѱ��ְ�,
        // ���ο��� Lock��� �ٽ� �˻�
        mBufferPool.discardBCB( aStatistics,
                                sBCB,
                                filterTheBCB,
                                (void*)&sCmpBCB,
                                ID_TRUE,//WAIT MODE true
                                &sIsSuccess,
                                &sDummy );

        IDE_ASSERT( sIsSuccess == ID_TRUE );
    }
    else
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/****************************************************************
 * Description :
 *  �������� �޾Ƽ� �� �������� �ش��ϴ� BCB�� pageType�� �����Ѵ�.
 *
 *  aPagePtr    - [IN]  page frame pointer
 *  aPageType   - [IN]  ������ page type
 ****************************************************************/
void sdbBufferMgr::setPageTypeToBCB( UChar *aPagePtr, UInt aPageType )
{
    sdbBCB   *sBCB;

    IDE_DASSERT( aPagePtr == idlOS::align( (void*)aPagePtr, SD_PAGE_SIZE ) );

    sBCB = getBCBFromPagePtr( aPagePtr );

    sBCB->mPageType = aPageType;
}

void sdbBufferMgr::setDirtyPageToBCB( idvSQL * aStatistics,
                                      UChar  * aPagePtr )
{
    sdbBCB   *sBCB;

    IDE_DASSERT( aPagePtr == idlOS::align( (void*)aPagePtr, SD_PAGE_SIZE ) );

    sBCB = getBCBFromPagePtr( aPagePtr );
    mBufferPool.setDirtyPage( aStatistics, sBCB, NULL);
}

/************************************************************
 * Description :
 *  Discard:
 *  �������� ���۳����� �����Ѵ�. ���� BCB�� dirty��� flush�����ʰ�
 *  ������ ���� ������. ���̺� drop�Ǵ� ���̺� �����̽� drop���꿡
 *  �ش��ϴ� BCB�� �̿����� ���ؼ� ���۳����� �����ϴ� ���� ����.
 *
 *  aBCB    - [IN]  BCB
 *  aObj    - [IN]  �ݵ�� sdbDiscardPageObj�� ����־�� �Ѵ�.
 *                  �� ������ �������� aBCB�� discard���� ���� �����ϴ�
 *                  �Լ� �� ������, �׸��� discard�� ���������� �ش� BCB��
 *                  ������ ť�� ������ �ִ�.
 ************************************************************/
IDE_RC sdbBufferMgr::discardNoWaitModeFunc( sdbBCB    *aBCB,
                                            void      *aObj)
{
    idBool                  sIsSuccess;
    smuQueueMgr            *sWaitingQueue;
    sdbDiscardPageObj      *sObj = (sdbDiscardPageObj*)aObj;
    idBool                  sMakeFree;
    idvSQL                 *sStat = sObj->mStatistics;
    sdbBufferPool          *sPool = sObj->mBufferPool;

    if( sObj->mFilter( aBCB, sObj->mEnv ) == ID_TRUE )
    {
        if( aBCB->isFree() == ID_TRUE)
        {
            return IDE_SUCCESS;
        }

        sPool->discardBCB( sStat,
                           aBCB ,
                           sObj->mFilter,
                           sObj->mEnv,
                           ID_FALSE, // no wait mode
                           &sIsSuccess,
                           &sMakeFree);

        if( sMakeFree == ID_TRUE )
        {
            /* [BUG-20630] alter system flush buffer_pool�� �����Ͽ�����,
             * hot������ freeBCB��� ������ �ֽ��ϴ�.
             * free�� ���� BCB�� ���ؼ��� ����Ʈ���� �����Ͽ�
             * sdbPrepareList�� �����Ѵ�.*/
            if( sPool->removeBCBFromList(sStat, aBCB ) == ID_TRUE )
            {
                IDE_ASSERT( sPool->addBCB2PrepareLst( sStat, aBCB )
                            == IDE_SUCCESS );
            }
        }

        sWaitingQueue = &(sObj->mQueue);

        if( sIsSuccess == ID_FALSE)
        {
            /* discard�� NoWait�� �����ϱ� ������, ������ ���ִ�.
             * �̰�쿡�� waitingQueue�� �����ϴµ�, ���߿� �� ť�� ���Ե� BCB��
             * �ٽ� discard�ϱ⸦ �õ��ϵ���.. �ƴ� �����ϴ���.. ��¶�� �����Ѵ�.
             * �����ϸ� waitingQueue�� �������� �ʴ´�.
             * */
            IDE_ASSERT( sWaitingQueue->enqueue( ID_FALSE,//mutex������ ���� ����
                                                (void*)&aBCB )
                        == IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
}

/************************************************************
 * Description :
 *  BCB�� ����� �ִ� Queue�� �Է����� �޾� �� queue���� �����ϴ� ��� BCB��
 *  ���ؼ� discard�� �����Ѵ�. �̶�, aFilter���ǿ� �´°͸� discard��Ű��,
 *  �׷��� ���� BCB�� ť���� �׳� �����Ѵ�.
 *
 *  Wait mode��? BCB�� aFilter������ ����������, ���������� �ٷ� ������ ��
 *  ���� ��찡 �ִ�. ���� ���, BCB���°� inIOB�Ǵ� Redirty������ ��쿣
 *  ������ �� ����.
 *  [���ü�] �ֳĸ�, inIOB�Ǵ� redirty������ ��쿣 �÷��Ű� ��ũ��
 *  ���⸦ ����ϰ� �ִٴ� ���̵ȴ�. �׷��� ������, ���Ҹ� discard�� ����
 *  ������, ���� pid�� ���� BCB�� ���۳����� �����Ǿ��ٰ�, �ٸ� Ʈ�����
 *  �� ���� ���۷� �ٽ� �ö�� ���ְ�, �� Ʈ������� BCB�� ������ ����ϰ�
 *  ���� �� ���Ŀ�, flusher�� �ڽ��� IOB�� ������ ����� ���� �� �ֱ� ������
 *  ���ü��� ���� ����� �ִ�.
 *  �׷��� �׳� �������� �ʰ�, ���°� ����Ǳ⸦ ��ٸ���.
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  aQueue�� �ִ� BCB�� aFilter���ǿ� �´� BCB�� discard
 *  aFiltAgr    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 *  aQueue      - [IN]  BCB���� ��� �ִ� queue
 ************************************************************/
void sdbBufferMgr::discardWaitModeFromQueue( idvSQL      *aStatistics,
                                             sdbFiltFunc  aFilter,
                                             void        *aFiltAgr,
                                             smuQueueMgr *aQueue )
{
    sdbBCB *sBCB;
    idBool  sEmpty;
    idBool  sIsSuccess;
    idBool  sMakeFree;

    while(1)
    {
        IDE_ASSERT( aQueue->dequeue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                     (void*)&sBCB, &sEmpty)
                    == IDE_SUCCESS );

        if( sEmpty == ID_TRUE )
        {
            break;
        }

        mBufferPool.discardBCB(  aStatistics,
                                 sBCB,
                                 aFilter,
                                 aFiltAgr,
                                 ID_TRUE, // wait mode
                                 &sIsSuccess,
                                 &sMakeFree);

        IDE_ASSERT( sIsSuccess == ID_TRUE );
        
        if( sMakeFree == ID_TRUE )
        {
            /* [BUG-20630] alter system flush buffer_pool�� �����Ͽ�����,
             * hot������ freeBCB��� ������ �ֽ��ϴ�.
             * free�� ���� BCB�� ���ؼ��� ����Ʈ���� �����Ͽ�
             * sdbPrepareList�� �����Ѵ�.*/
            if( mBufferPool.removeBCBFromList( aStatistics, sBCB ) == ID_TRUE )
            {
                IDE_ASSERT( mBufferPool.addBCB2PrepareLst( aStatistics, sBCB )
                            == IDE_SUCCESS );
            }
        }
    }

    return;
}

/****************************************************************
 * Description :
 *  BufferManager�� �����ϴ� ��� BCB�� ���ؼ�, Filt������ �����ϴ� BCB����
 *  ��� discard��Ų��.
 * Implementation:
 *  bufferArea�� ��� BCB�� �ִ�. �� �� BCB�� ���� discard�� ������� �����Ѵ�.
 *  �׷��� ������, bufferArea���� �޺κп� �ִ� BCB�� ��쿣 discard��
 *  �����ϴµ� �ð��� �ɸ���. �׷��� ������, ó���� filt������ �����ϴٰ���,
 *  discard�� �����Ҷ��� filt�� �������� ���� �� �ִ�.  �̰�쿣 �����ϸ� �ȴ�.
 *
 *  �ݴ��� �����?
 *  bufferArea�� �պκп� �ִ� BCB�� ó���� filt������ �������� �ʴٰ�
 *  (Ȥ�� �����ϴٰ�) discard�Լ� ������ ���� ���Ŀ� filt������ �����ϰ� �ȴٸ�
 *  ��� �Ǵ°�? �̷� ���� ���� �߻����� �ʱ⸦ ���Ѵٸ�, ����
 *  (�� �Լ��� ȣ���ϴ� �κ�)���� ������ ���־�� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  aFilter���ǿ� �´� BCB�� discard
 *  aFiltAgr    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdbBufferMgr::discardPages( idvSQL        *aStatistics,
                                   sdbFiltFunc    aFilter,
                                   void          *aFiltAgr)
{
    sdbDiscardPageObj sObj;

    sObj.mFilter = aFilter;
    sObj.mStatistics = aStatistics;
    sObj.mBufferPool = &mBufferPool;
    sObj.mEnv = aFiltAgr;

    sObj.mQueue.initialize(IDU_MEM_SM_SDB, ID_SIZEOF( sdbBCB*));

    /* sdbBufferArea�� �ִ� ��� BCB�� ���ؼ� discardNoWaitModeFunc�� �����Ѵ�.
     * �̶�, noWailtMode�� ���۽�Ų��. �׸��� ��ó discard���� ���� BCB��
     * ť�� ��Ƶξ��ٰ� wait mode�� discard��Ų��.
     *
     * �ι��� ����� �ϴ� ����? ���� �� �����ϸ�..
     * ���ۿ� 1 ~ 100���� BCB�� �ִµ�, ¦�� ��ȣ�� BCB�� ��� dirty�����̸鼭
     * filt������ �����Ѵ�.
     * �̶�, 1���� inIOB�����̴�.
     * ���� wait���� discard�� ��Ų�ٸ�, 1������ ���°� clean���� ���Ҷ�����
     * ����Ѵ�.  �̶�, dirty�� BCB���� �ٸ� flusher�� ���ؼ� ��ũ�� ������ ��
     * �ִ�. �̶� ��ũ�� ���� ���� �� ���� ���ش�. �ֳĸ� discard�� ��쿣
     * dirty�� flush���� �����ϱ� �����̴�.
     * ���� ���� ������ nowait���� �����ϸ�,
     * 1�� BCB�� skip�� ����(ť�� ����)�� ������� ��� ¦�� BCB�� dirty��
     * �����ϱ� ������ ���� disk IO�� ���� ��Ű�� �ʴ´�.
     * �׸��� ���� 1�� BCB�� wait���� ó���ϰ� �ȴ�.
     * */
    IDE_ASSERT( mBufferArea.applyFuncToEachBCBs( aStatistics,
                                                 discardNoWaitModeFunc,
                                                 &sObj)
                == IDE_SUCCESS );


    discardWaitModeFromQueue( aStatistics,
                              aFilter,
                              aFiltAgr,
                              &sObj.mQueue );

    sObj.mQueue.destroy();

    return IDE_SUCCESS;
}


/****************************************************************
 * Description :
 *  ���۸Ŵ����� Ư�� pid������ ���ϴ� ��� BCB�� discard�Ѵ�.
 *  �̶�, pid������ ���ϸ鼭 ���ÿ� aSpaceID�� ���ƾ� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdbBufferMgr::discardPagesInRange( idvSQL    *aStatistics,
                                          scSpaceID  aSpaceID,
                                          scPageID   aStartPID,
                                          scPageID   aEndPID)
{
    sdbBCBRange sRange;

    sRange.mSpaceID  = aSpaceID;
    sRange.mStartPID = aStartPID;
    sRange.mEndPID   = aEndPID;

    return sdbBufferMgr::discardPages( aStatistics,
                                       filterBCBRange,
                                       (void*)&sRange );
}

/****************************************************************
 * Description :
 *  ���۸Ŵ����� �ִ� ��� BCB�� discard��Ų��.
 *
 *  aStatistics - [IN]  �������
 ****************************************************************/
IDE_RC sdbBufferMgr::discardAllPages(idvSQL     *aStatistics)
{
    return sdbBufferMgr::discardPages( aStatistics,
                                       filterAllBCBs,
                                       NULL );// �ּ��߰�
}


/****************************************************************
 * Description :
 *  �� �Լ������� pageOut�� �ϱ����� ��ó�� �۾��� �����Ѵ�. ��,
 *  ���� pageOut�� ���� �ʰ� BCB�� � �����̳Ŀ� ���� �ش� Queue�� �����Ѵ�.
 *  ��, ���Լ��� PageOutTargetQueue �� make�ϴµ� ����ϴ� Function�̴�.
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �� �Լ� �����ϴµ� �ʿ��� �ڷ�.
 *                      sdbPageOutObj�� ĳ�����ؼ� ����Ѵ�.
 ****************************************************************/
IDE_RC sdbBufferMgr::makePageOutTargetQueueFunc( sdbBCB *aBCB,
                                                 void   *aObj)
{
    sdbPageOutObj *sObj = (sdbPageOutObj*)aObj;
    sdbBCBState sState;

    if( sObj->mFilter( aBCB , sObj->mEnv) == ID_TRUE )
    {
        sState = aBCB->mState;
        if( (sState == SDB_BCB_DIRTY) ||
            // BUG-21135 flusher�� flush�۾��� �Ϸ��ϱ� ���ؼ���
            // INIOB������ BCB���°� ����Ǳ� ��ٷ��� �մϴ�.
            (sState == SDB_BCB_INIOB) ||
            (sState == SDB_BCB_REDIRTY))
        {
            IDE_ASSERT(
                sObj->mQueueForFlush.enqueue( ID_FALSE, //mutex�� ���� �ʴ´�.
                                              (void*)&aBCB)
                        == IDE_SUCCESS );
        }

        IDE_ASSERT(
            sObj->mQueueForMakeFree.enqueue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                             (void*)&aBCB)
            == IDE_SUCCESS );
    }

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  PageOut : ���ۿ� �����ϴ� �������� ���۸Ŵ������� �����ϴ� ���. discard��
 *  �ٸ��� pageOut�� ��쿡 flush�� �� ���Ŀ� ���Ÿ� �Ѵٴ� ���̴�.
 *
 *  �� �Լ��� ���۸Ŵ����� �����ϴ� ������ �� filt ���ǿ� �ش��ϴ� �������� ���ؼ�
 *  pageOut ��Ų��.
 *
 * Implementation:
 *  ť�� 2���� ������ �ִ�.
 *  1. flush�� �ʿ��� BCB�� ��Ƴ��� ť:
 *  2. flush�� �ʿ�ġ �ʰ�, makeFree�� �Ͽ��� BCB�� ��Ƴ��� ť:
 *
 *  ���� BCB�� dirty�̰ų� redirty�� ��쿣 1���� 2�� ��ο� �����Ѵ�.
 *  1���� �����ϴ� ������ dirty�� ���ֱ� ���ؼ� �̰�, 2������ �����ϴ�
 *  ������, �� BCB�� ���� makeFree �ؾ� �ϱ� �����̴�.
 *
 *  dirty�� redirty�� �ƴ� BCB�� ��쿣 2�� ť���� �����Ѵ�.
 *
 *  ���� ó���� dirty�� �ƴϾ��ٰ� ť�� �����ϰ� �� ���Ŀ� dirty�� �Ǹ� ��¼��?
 *  �̷� ���� �߻��ؼ��� �ȵȴ�.  �̰��� ����(�� �Լ��� �̿��ϴ� �κ�)����
 *  ������ �־�� �Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  BCB�� aFilter���ǿ� �´� BCB�� flush
 *  aFiltArg    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdbBufferMgr::pageOut( idvSQL            *aStatistics,
                              sdbFiltFunc        aFilter,
                              void              *aFiltAgr)
{
    sdbPageOutObj sObj;
    sdbBCB       *sBCB;
    sdsBCB       *sSBCB;
    smuQueueMgr  *sQueue;
    idBool        sEmpty;

    // BUG-26476
    // ��� flusher���� stop �����̸� abort ������ ��ȯ�Ѵ�.
    IDE_TEST_RAISE(sdbFlushMgr::getActiveFlusherCount() == 0,
                   ERR_ALL_FLUSHERS_STOPPED);

    sObj.mFilter = aFilter;
    sObj.mEnv    = aFiltAgr;

    sObj.mQueueForFlush.initialize( IDU_MEM_SM_SDB, ID_SIZEOF(sdbBCB *));
    sObj.mQueueForMakeFree.initialize( IDU_MEM_SM_SDB, ID_SIZEOF(sdbBCB *));

    /* buffer Area�� �����ϴ� ��� BCB�� ���鼭 filt���ǿ� �ش��ϴ� BCB�� ���
     * queue�� ������.*/
    IDE_ASSERT( mBufferArea.applyFuncToEachBCBs( aStatistics,
                                                 makePageOutTargetQueueFunc,
                                                 &sObj)
                == IDE_SUCCESS );

    /*���� flush�� �ʿ��� queue������ flush�� �����Ѵ�.*/
    IDE_TEST( sdbFlushMgr::flushObjectDirtyPages( aStatistics,
                                                  &(sObj.mQueueForFlush),
                                                  aFilter,
                                                  aFiltAgr)
              != IDE_SUCCESS );
    sObj.mQueueForFlush.destroy();

    sQueue = &(sObj.mQueueForMakeFree);
    while(1)
    {
        IDE_ASSERT( sQueue->dequeue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                     (void*)&sBCB, &sEmpty)
                    == IDE_SUCCESS );

        if( sEmpty == ID_TRUE )
        {
            break;
        }
        /* BCB�� free���·� �����. free������ BCB�� �ؽø� ���ؼ� ������ ��
         * ������, �ش� pid�� ��ȿ�̴�.  �׷��� ������, ���ۿ��� �����Ȱ�����
         * �� �� �ִ�. */

        /* ����� SBCB�� �ִٸ� delink */
        sSBCB = sBCB->mSBCB;
        if( sSBCB != NULL )
        {
            sdsBufferMgr::pageOutBCB( aStatistics,
                                      sSBCB );
            sBCB->mSBCB = NULL;
        }

        if( mBufferPool.makeFreeBCB( aStatistics, sBCB, aFilter, aFiltAgr )
            == ID_TRUE )
        {
            /* [BUG-20630] alter system flush buffer_pool�� �����Ͽ�����,
             * hot������ freeBCB��� ������ �ֽ��ϴ�.
             * makeFreeBCB�� �����Ͽ� free�� ���� BCB�� ���ؼ��� ����Ʈ���� �����Ͽ�
             * sdbPrepareList�� �����Ѵ�.*/
            if( mBufferPool.removeBCBFromList( aStatistics, sBCB ) == ID_TRUE )
            {
                IDE_ASSERT( mBufferPool.addBCB2PrepareLst( aStatistics, sBCB )
                            == IDE_SUCCESS );
            }
        }
    }
    sQueue->destroy();

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_ALL_FLUSHERS_STOPPED);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AllFlushersStopped));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  �ش� pid������ �ִ� ��� BCB�鿡�� pageout�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdbBufferMgr::pageOutInRange( idvSQL         *aStatistics,
                                     scSpaceID       aSpaceID,
                                     scPageID        aStartPID,
                                     scPageID        aEndPID)
{
    sdbBCBRange sRange;

    sRange.mSpaceID     = aSpaceID;
    sRange.mStartPID    = aStartPID;
    sRange.mEndPID      = aEndPID;

    return sdbBufferMgr::pageOut( aStatistics,
                                  filterBCBRange,
                                  (void*)&sRange );
}

/****************************************************************
 * Description :
 *  �ش� spaceID�� �ش��ϴ� ��� BCB�鿡�� pageout�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 ****************************************************************/
IDE_RC sdbBufferMgr::pageOutTBS( idvSQL          *aStatistics,
                                 scSpaceID        aSpaceID)
{
    return sdbBufferMgr::pageOut( aStatistics,
                                  filterTBSBCBs,
                                  (void*)&aSpaceID);
}

/****************************************************************
 * Description :
 *  ���۸Ŵ����� �ִ� ��� BCB�鿡 pageOut�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 ****************************************************************/
IDE_RC sdbBufferMgr::pageOutAll(idvSQL     *aStatistics)
{
    return sdbBufferMgr::pageOut( aStatistics,
                                  filterAllBCBs,
                                  NULL );
}

/****************************************************************
 * Description :
 *  FlushTargetQueue�� Make�ϴ� Function
 * Implementation:
 *  �ش� filt������ �����ϸ鼭 dirty, redirty�� BCB�� ť�� �����Ѵ�.
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �� �Լ� ���࿡ �ʿ��� �ڷḦ ������ �ִ�.
 *                      sdbFlushObj�� ĳ�����ؼ� ���.
 ****************************************************************/
IDE_RC sdbBufferMgr::makeFlushTargetQueueFunc( sdbBCB *aBCB,
                                               void   *aObj)
{
    sdbFlushObj *sObj = (sdbFlushObj*)aObj;
    sdbBCBState sState;

    if( sObj->mFilter( aBCB , sObj->mEnv) == ID_TRUE )
    {
        sState = aBCB->mState;
        if( (sState == SDB_BCB_DIRTY) ||
            // BUG-21135 flusher�� flush�۾��� �Ϸ��ϱ� ���ؼ���
            // INIOB������ BCB���°� ����Ǳ� ��ٷ��� �մϴ�.
            (sState == SDB_BCB_INIOB) ||
            (sState == SDB_BCB_REDIRTY))
        {
            IDE_ASSERT( sObj->mQueue.enqueue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                              (void*)&aBCB)
                        == IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  filt�� �ش��ϴ� �������� flush�Ѵ�.
 * Implementation:
 *  sdbFlushMgr::flushObjectDirtyPages�Լ��� �̿��ϱ� ���ؼ�,
 *  ���� flush�� �ؾ��� BCB���� queue�� ��Ƴ��´�.
 *  �׸��� sdbFlushMgr::flushObjectDirtyPages�Լ��� �̿��� flush.
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  BCB�� aFilter���ǿ� �´� BCB�� flush
 *  aFiltArg    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdbBufferMgr::flushPages( idvSQL            *aStatistics,
                                 sdbFiltFunc        aFilter,
                                 void              *aFiltAgr)
{
    sdbFlushObj sObj;

    // BUG-26476
    // ��� flusher���� stop �����̸� abort ������ ��ȯ�Ѵ�.
    IDE_TEST_RAISE(sdbFlushMgr::getActiveFlusherCount() == 0,
                   ERR_ALL_FLUSHERS_STOPPED);

    sObj.mFilter = aFilter;
    sObj.mEnv    = aFiltAgr;

    sObj.mQueue.initialize( IDU_MEM_SM_SDB, ID_SIZEOF(sdbBCB *));

    IDE_ASSERT( mBufferArea.applyFuncToEachBCBs( aStatistics,
                                                 makeFlushTargetQueueFunc,
                                                 &sObj)
                == IDE_SUCCESS );

    IDE_TEST( sdbFlushMgr::flushObjectDirtyPages( aStatistics,
                                                  &(sObj.mQueue),
                                                  aFilter,
                                                  aFiltAgr )
              != IDE_SUCCESS );
    sObj.mQueue.destroy();

    return IDE_SUCCESS;

    IDE_EXCEPTION(ERR_ALL_FLUSHERS_STOPPED);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_AllFlushersStopped));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  ���۸Ŵ������� �ش� pid������ �ش��ϴ� BCB�� ��� flush�Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdbBufferMgr::flushPagesInRange( idvSQL         *aStatistics,
                                        scSpaceID       aSpaceID,
                                        scPageID        aStartPID,
                                        scPageID        aEndPID)
{
    sdbBCBRange sRange;

    sRange.mSpaceID     = aSpaceID;
    sRange.mStartPID    = aStartPID;
    sRange.mEndPID      = aEndPID;

    return sdbBufferMgr::flushPages( aStatistics,
                                     filterBCBRange,
                                     (void*)&sRange );
}

/***********************************************************************
 *
 * Description :
 *
 *   - [IN]
 *   - [OUT]
 *
 **********************************************************************/
IDE_RC sdbBufferMgr::getPageByGRID( idvSQL             *aStatistics,
                                    scGRID              aGRID,
                                    sdbLatchMode        aLatchMode,
                                    sdbWaitMode         aWaitMode,
                                    void               *aMtx,
                                    UChar             **aRetPage,
                                    idBool             *aTrySuccess)
{
    IDE_RC    rc;
    sdRID     sRID;
    sdSID     sSID;

    IDE_DASSERT( SC_GRID_IS_NOT_NULL( aGRID ));

    if ( SC_GRID_IS_WITH_SLOTNUM(aGRID) )
    {
        sSID = SD_MAKE_SID_FROM_GRID( aGRID );

        rc = getPageBySID( aStatistics,
                           SC_MAKE_SPACE(aGRID),
                           sSID,
                           aLatchMode,
                           aWaitMode,
                           aMtx,
                           aRetPage,
                           aTrySuccess);
    }
    else
    {
        sRID = SD_MAKE_RID_FROM_GRID( aGRID );

        rc = getPageByRID( aStatistics,
                           SC_MAKE_SPACE(aGRID),
                           sRID,
                           aLatchMode,
                           aWaitMode,
                           aMtx,
                           aRetPage,
                           aTrySuccess);
    }

    return rc;
}


/***********************************************************************
 *
 * Description :
 *
 *   - [IN]
 *   - [OUT]
 *
 **********************************************************************/
IDE_RC sdbBufferMgr::getPageBySID( idvSQL             *aStatistics,
                                   scSpaceID           aSpaceID,
                                   sdSID               aPageSID,
                                   sdbLatchMode        aLatchMode,
                                   sdbWaitMode         aWaitMode,
                                   void               *aMtx,
                                   UChar             **aRetPage,
                                   idBool             *aTrySuccess )
{
    UChar    * sSlotDirPtr;
    idBool     sIsLatched = ID_FALSE;

    IDE_TEST( getPageByPID( aStatistics,
                            aSpaceID,
                            SD_MAKE_PID(aPageSID),
                            aLatchMode,
                            aWaitMode,
                            SDB_SINGLE_PAGE_READ,
                            aMtx,
                            aRetPage,
                            aTrySuccess,
                            NULL )
              != IDE_SUCCESS );
    sIsLatched = ID_TRUE;

    sSlotDirPtr = smLayerCallback::getSlotDirStartPtr( *aRetPage );

    IDU_FIT_POINT( "1.BUG-43091@sdbBufferMgr::getPageBySID::Jump" );
    IDE_TEST( smLayerCallback::getPagePtrFromSlotNum( sSlotDirPtr,
                                                      SD_MAKE_SLOTNUM(aPageSID),
                                                      aRetPage )
              != IDE_SUCCESS );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if ( (sIsLatched == ID_TRUE) && ( aMtx == NULL ) )
    {
        sIsLatched = ID_FALSE;
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar*)*aRetPage );   
    }
    return IDE_FAILURE;
}

/* BUG-43844 : �̹� free�� ����(unused �÷��� üũ��)�� �����Ͽ� fetch�� 
 * �õ� �� ��� ��ŵ�ϵ��� �Ѵ�. getPageByGRID, getPageBySID �Լ��� skipFetch 
 * ���� �߰�. 
 */
IDE_RC sdbBufferMgr::getPageByGRID( idvSQL             *aStatistics,
                                    scGRID              aGRID,
                                    sdbLatchMode        aLatchMode,
                                    sdbWaitMode         aWaitMode,
                                    void               *aMtx,
                                    UChar             **aRetPage,
                                    idBool             *aTrySuccess,
                                    idBool             *aSkipFetch )
{
    IDE_RC    rc;
    sdRID     sRID;
    sdSID     sSID;

    IDE_DASSERT( SC_GRID_IS_NOT_NULL( aGRID ) );

    if ( SC_GRID_IS_WITH_SLOTNUM( aGRID ) )
    {
        sSID = SD_MAKE_SID_FROM_GRID( aGRID );

        rc = getPageBySID( aStatistics,
                           SC_MAKE_SPACE(aGRID),
                           sSID,
                           aLatchMode,
                           aWaitMode,
                           aMtx,
                           aRetPage,
                           aTrySuccess,
                           aSkipFetch );
    }
    else
    {
        sRID = SD_MAKE_RID_FROM_GRID( aGRID );

        rc = getPageByRID( aStatistics,
                           SC_MAKE_SPACE(aGRID),
                           sRID,
                           aLatchMode,
                           aWaitMode,
                           aMtx,
                           aRetPage,
                           aTrySuccess );
    }

    return rc;
}

IDE_RC sdbBufferMgr::getPageBySID( idvSQL             *aStatistics,
                                   scSpaceID           aSpaceID,
                                   sdSID               aPageSID,
                                   sdbLatchMode        aLatchMode,
                                   sdbWaitMode         aWaitMode,
                                   void               *aMtx,
                                   UChar             **aRetPage,
                                   idBool             *aTrySuccess,
                                   idBool             *aSkipFetch )
{
    UChar    * sSlotDirPtr;
    idBool     sIsLatched = ID_FALSE;

    IDE_TEST( getPageByPID( aStatistics,
                            aSpaceID,
                            SD_MAKE_PID(aPageSID),
                            aLatchMode,
                            aWaitMode,
                            SDB_SINGLE_PAGE_READ,
                            aMtx,
                            aRetPage,
                            aTrySuccess,
                            NULL )
              != IDE_SUCCESS );
    sIsLatched = ID_TRUE;

    sSlotDirPtr = smLayerCallback::getSlotDirStartPtr( *aRetPage );

    /* BUG-43844: �̹� free�� ����(unused �÷��� üũ��)�� �����Ͽ� fetch��
     * �õ� �� ��� ��ŵ */
    if ( SDP_IS_UNUSED_SLOT_ENTRY( SDP_GET_SLOT_ENTRY( sSlotDirPtr, 
                                                       SD_MAKE_SLOTNUM( aPageSID ) ) )
                                   == ID_TRUE )
    {
        *aSkipFetch = ID_TRUE;
        IDE_CONT( SKIP_GET_VAL );
    }
    else
    {
        *aSkipFetch = ID_FALSE;
    }

    IDU_FIT_POINT( "99.BUG-43091@sdbBufferMgr::getPageBySID::Jump" );
    IDE_TEST( smLayerCallback::getPagePtrFromSlotNum( sSlotDirPtr,
                                                      SD_MAKE_SLOTNUM(aPageSID),
                                                      aRetPage )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_GET_VAL );

    return IDE_SUCCESS;
    
    IDE_EXCEPTION_END;
    
    if ( ( sIsLatched == ID_TRUE ) && ( aMtx == NULL ) )
    {
        sIsLatched = ID_FALSE;
        (void)sdbBufferMgr::releasePage( aStatistics,
                                         (UChar*)*aRetPage );   
    }
    return IDE_FAILURE;
}
/****************************************************************
 * Description :
 *  Pinning�� ���� ���̴� ���, aEnv���� �ش� page�� �����ϴ��� �˻��Ͽ�
 *  �ִٸ� �ش� BCB�� �����ϰ�, �׷��� �ʴٸ� NULL�� �����Ѵ�.
 *
 *  aEnv        - [IN]  pinning env
 *  aSpaceID    - [IN]  table space ID
 *  aPageID     - [IN]  page ID
 ****************************************************************/
sdbBCB* sdbBufferMgr::findPinnedPage( sdbPinningEnv *aEnv,
                                      scSpaceID      aSpaceID,
                                      scPageID       aPageID )
{
    UInt i;

    IDE_DASSERT(aEnv != NULL);
    IDE_DASSERT(aSpaceID != SC_NULL_SPACEID);
    IDE_DASSERT(aPageID != SC_NULL_PID);

    for (i = 0; i < aEnv->mPinningCount; i++)
    {
        if ((aEnv->mPinningList[i].mSpaceID == SC_NULL_SPACEID) &&
            (aEnv->mPinningList[i].mPageID == SC_NULL_PID))
        {
            break;
        }
        if ((aSpaceID == aEnv->mPinningList[i].mSpaceID) &&
            (aPageID == aEnv->mPinningList[i].mPageID))
        {
            return aEnv->mPinningBCBList[i];
        }
    }
    return NULL;
}

/****************************************************************
 * Description :
 *  Pinning ����� ���� ���̴� �Լ�.
 *  mAccessHistory�� ���ٸ�, ���� mAccessHistory�� �����Ѵ�.
 *  ������ mAccessHistroy�� �����Ѵٸ�, mPinningList�� �����Ѵ�.
 *  ��������, (������ ����������) ���� mPinningList�� ���� pid�� ������
 *  �����ϱ� ���� findPinnedPage�� ���� ȣ���Ѵ�.
 *
 *  ���� mPinningList�� ������ ��쿣 ������ �����ϴ� BCB�� unPin��Ű��,
 *  ���� BCB�� pinning���Ѿ� �Ѵ�.
 *
 *  aEnv        - [IN]  pinning env
 *  aBCB        - [IN]  BCB
 *  aBCBPinned  - [IN]  mAccessHistory���� �����ϰ�, Pinning��Ű��
 *                      �ʾҴٸ� ID_FALSE�� ����, mPinningList�� �����ߴٸ�
 *                      ID_TRUE�� �����Ѵ�.
 *  aBCBToUnpin - [OUT] mPinningList�� �����Ҷ�, �̹� ������ mPinningList���
 *                      ������ �����ϴ� BCB�� Unpin���Ѿ� �Ѵ�. �� Unpin���Ѿ�
 *                      �ϴ� BCB�� �����Ѵ�.
 ****************************************************************/
void sdbBufferMgr::addToHistory( sdbPinningEnv     *aEnv,
                                 sdbBCB            *aBCB,
                                 idBool            *aBCBPinned,
                                 sdbBCB           **aBCBToUnpin )
{
    UInt i;

    IDE_DASSERT(aEnv != NULL);
    IDE_DASSERT(aBCB != NULL);

    *aBCBPinned  = ID_FALSE;
    *aBCBToUnpin = NULL;

    for (i = 0; i < aEnv->mAccessHistoryCount; i++)
    {
        if ((aEnv->mAccessHistory[i].mSpaceID == SC_NULL_SPACEID) &&
            (aEnv->mAccessHistory[i].mPageID == SC_NULL_PID))
        {
            break;
        }

        if ((aBCB->mSpaceID == aEnv->mAccessHistory[i].mSpaceID) &&
            (aBCB->mPageID == aEnv->mAccessHistory[i].mPageID))
        {
            if ((aEnv->mPinningList[aEnv->mPinningInsPos].mSpaceID != SC_NULL_SPACEID) &&
                (aEnv->mPinningList[aEnv->mPinningInsPos].mPageID != SC_NULL_PID))
            {
                *aBCBToUnpin = aEnv->mPinningBCBList[aEnv->mPinningInsPos];
            }

            aEnv->mPinningList[aEnv->mPinningInsPos].mSpaceID = 
                                                          aBCB->mSpaceID;
            aEnv->mPinningList[aEnv->mPinningInsPos].mPageID = 
                                                          aBCB->mPageID;
            aEnv->mPinningBCBList[aEnv->mPinningInsPos] = aBCB;
            aEnv->mPinningInsPos++;

            if (aEnv->mPinningInsPos == aEnv->mPinningCount)
            {
                aEnv->mPinningInsPos = 0;
            }

            *aBCBPinned = ID_TRUE;

            break;
        }
    }

    if( *aBCBPinned == ID_FALSE )
    {
        aEnv->mAccessHistory[aEnv->mAccessInsPos].mSpaceID = 
                                                          aBCB->mSpaceID;
        aEnv->mAccessHistory[aEnv->mAccessInsPos].mPageID = 
                                                          aBCB->mPageID;
        aEnv->mAccessInsPos++;

        if( aEnv->mAccessInsPos == aEnv->mAccessHistoryCount )
        {
            aEnv->mAccessInsPos = 0;
        }
    }
}

IDE_RC sdbBufferMgr::createPage( idvSQL          *aStatistics,
                                 scSpaceID        aSpaceID,
                                 scPageID         aPageID,
                                 UInt             aPageType,
                                 void*            aMtx,
                                 UChar**          aPagePtr)
{
    IDE_RC rc;

    IDV_SQL_OPTIME_BEGIN(aStatistics, IDV_OPTM_INDEX_DRDB_CREATE_PAGE);

    rc = mBufferPool.createPage( aStatistics,
                                 aSpaceID,
                                 aPageID,
                                 aPageType,
                                 aMtx,
                                 aPagePtr);

    IDV_SQL_OPTIME_END(aStatistics, IDV_OPTM_INDEX_DRDB_CREATE_PAGE);

    return rc;
}

IDE_RC sdbBufferMgr::getPageByPID( idvSQL               * aStatistics,
                                   scSpaceID              aSpaceID,
                                   scPageID               aPageID,
                                   sdbLatchMode           aLatchMode,
                                   sdbWaitMode            aWaitMode,
                                   sdbPageReadMode        aReadMode,
                                   void                 * aMtx,
                                   UChar               ** aRetPage,
                                   idBool               * aTrySuccess,
                                   idBool               * aIsCorruptPage )
{
    IDE_RC rc;

    IDV_SQL_OPTIME_BEGIN(aStatistics, IDV_OPTM_INDEX_DRDB_GET_PAGE);

    rc = mBufferPool.getPage( aStatistics,
                              aSpaceID,
                              aPageID,
                              aLatchMode,
                              aWaitMode,
                              aReadMode,
                              aMtx,
                              aRetPage,
                              aTrySuccess,
                              aIsCorruptPage );

    IDV_SQL_OPTIME_END(aStatistics, IDV_OPTM_INDEX_DRDB_GET_PAGE);

    return rc;
}

IDE_RC sdbBufferMgr::getPage( idvSQL             *aStatistics,
                              scSpaceID           aSpaceID,
                              scPageID            aPageID,
                              sdbLatchMode        aLatchMode,
                              sdbWaitMode         aWaitMode,
                              sdbPageReadMode     aReadMode,
                              UChar             **aRetPage,
                              idBool             *aTrySuccess )
{
    IDE_RC rc;

    rc = getPageByPID( aStatistics,
                       aSpaceID,
                       aPageID,
                       aLatchMode,
                       aWaitMode,
                       aReadMode,
                       NULL, /* aMtx */
                       aRetPage,
                       aTrySuccess,
                       NULL /*IsCorruptPage*/);

    return rc;
}

IDE_RC sdbBufferMgr::getPageByRID( idvSQL             *aStatistics,
                                   scSpaceID           aSpaceID,
                                   sdRID               aPageRID,
                                   sdbLatchMode        aLatchMode,
                                   sdbWaitMode         aWaitMode,
                                   void               *aMtx,
                                   UChar             **aRetPage,
                                   idBool             *aTrySuccess )
{
    IDE_RC rc;

    rc = getPageByPID( aStatistics,
                       aSpaceID,
                       SD_MAKE_PID( aPageRID),
                       aLatchMode,
                       aWaitMode,
                       SDB_SINGLE_PAGE_READ,
                       aMtx,
                       aRetPage,
                       aTrySuccess,
                       NULL /*IsCorruptPage*/);

    // BUG-27328 CodeSonar::Uninitialized Variable
    if( rc == IDE_SUCCESS )
    {
        *aRetPage += SD_MAKE_OFFSET( aPageRID );
    }

    return rc;
}


/****************************************************************
 * Description :
 *  pinning ��ɰ� �Բ� ���̴� getPage.
 *  sdbPinningEnv�� �ݵ�� ���ڷ� �Ѱ� �־�� �Ѵ�.
 *  ���� findPinnedPage�� ���ؼ� �ش� pid�� mPinningList�� �����ϴ���
 *  Ȯ���Ѵ�.
 *  �ִٸ� �װ��� �����ϰ�, ���ٸ�, ���� Ǯ�κ��� �ش� BCB�� �����ͼ�
 *  mAccessHistory����ϰ�,
 *  ���� ������ mAccessHistory�� �ش� pid�� ���� ����� �ִٸ�
 *  mPinningList���� ����Ѵ�. �̶�, mPinningList�� ����á�ٸ�,
 *  ������ �����ϴ� BCB�� unpin��Ű��, ������ BCB�� �����Ѵ�.
 *
 *  ����!!
 *  pinning ����� ���� �׽����� ���� �ʾҴ�.
 *
 * Implementation:
 ****************************************************************/
IDE_RC sdbBufferMgr::getPage( idvSQL               *aStatistics,
                              void                 *aPinningEnv,
                              scSpaceID             aSpaceID,
                              scPageID              aPageID,
                              sdbLatchMode          aLatchMode,
                              sdbWaitMode           aWaitMode,
                              void                 *aMtx,
                              UChar               **aRetPage,
                              idBool               *aTrySuccess )
{
    sdbPinningEnv *sPinningEnv;
    sdbBCB        *sBCB;
    sdbBCB        *sOldBCB;
    idBool         sPinned;

    if ( aPinningEnv == NULL )
    {
        return getPageByPID( aStatistics,
                             aSpaceID,
                             aPageID,
                             aLatchMode,
                             aWaitMode,
                             SDB_SINGLE_PAGE_READ,
                             aMtx,
                             aRetPage,
                             aTrySuccess,
                             NULL /*IsCorruptPage*/ );
    }

    sPinningEnv = (sdbPinningEnv*)aPinningEnv;
    sBCB = findPinnedPage(sPinningEnv, aSpaceID, aPageID);
    if ( sBCB == NULL )
    {
        IDE_TEST( getPageByPID( aStatistics,
                                aSpaceID,
                                aPageID,
                                aLatchMode,
                                aWaitMode,
                                SDB_SINGLE_PAGE_READ,
                                aMtx,
                                aRetPage,
                                aTrySuccess,
                                NULL /*IsCorruptPage*/ )
                  != IDE_SUCCESS );

        // ���� �������κ��� BCB �����͸� ��´�.
        // ������������ ��¿���� ����.
        // �̷��Զ� ����.
        sBCB = (sdbBCB*)((sdbFrameHdr*)*aRetPage)->mBCBPtr;
        addToHistory(sPinningEnv, sBCB, &sPinned, &sOldBCB);
        if ( sPinned == ID_TRUE )
        {
            // pin ��Ű�� ���� fix count�� �ϳ� �ø���.
            sBCB->lockBCBMutex(aStatistics);
            sBCB->incFixCnt();
            sBCB->unlockBCBMutex();
            if ( sOldBCB != NULL )
            {
                sOldBCB->lockBCBMutex(aStatistics);
                sOldBCB->decFixCnt();
                sOldBCB->unlockBCBMutex();
            }
            else
            {
                /* nithing to do */
            }
        }
        else
        {
            /* nithing to do */
        }
    }
    else
    {
        mBufferPool.latchPage( aStatistics,
                               sBCB,
                               aLatchMode,
                               aWaitMode,
                               aTrySuccess );

        if(aMtx != NULL)
        {
            IDE_ASSERT( smLayerCallback::pushToMtx( aMtx, sBCB, aLatchMode )
                        == IDE_SUCCESS );
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbBufferMgr::fixPageByPID( idvSQL     * aStatistics,
                                   scSpaceID    aSpaceID,
                                   scPageID     aPageID,
                                   UChar     ** aRetPage,
                                   idBool     * aTrySuccess )
{
    IDE_RC rc;

    IDV_SQL_OPTIME_BEGIN(aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE);

    rc = mBufferPool.fixPage( aStatistics,
                              aSpaceID,
                              aPageID,
                              aRetPage );

    if( aTrySuccess != NULL )
    {
        if( rc == IDE_FAILURE )
        {
            *aTrySuccess = ID_FALSE;
        }
        else
        {
            *aTrySuccess = ID_TRUE;
        }
    }

    IDV_SQL_OPTIME_END(aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE);

    return rc;
}
#if 0
IDE_RC sdbBufferMgr::fixPageByGRID( idvSQL   * aStatistics,
                                    scGRID     aGRID,
                                    UChar   ** aRetPage,
                                    idBool   * aTrySuccess )
{
    scSpaceID   sSpaceID;
    sdRID       sRID;

    IDE_DASSERT( SC_GRID_IS_NOT_NULL( aGRID ));
    IDE_DASSERT( SC_GRID_IS_WITH_SLOTNUM( aGRID ) );

    if ( SC_GRID_IS_NOT_WITH_SLOTNUM(aGRID) )
    {
        sSpaceID = SC_MAKE_SPACE( aGRID );
        sRID     = SD_MAKE_RID_FROM_GRID( aGRID );

        IDE_TEST( fixPageByRID( aStatistics,
                                sSpaceID,
                                sRID,
                                aRetPage,
                                aTrySuccess )
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_TEST( 1 );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
#endif

IDE_RC sdbBufferMgr::fixPageByRID( idvSQL     * aStatistics,
                                   scSpaceID    aSpaceID,
                                   sdRID        aRID,
                                   UChar     ** aRetPage,
                                   idBool     * aTrySuccess )
{
    IDE_TEST( fixPageByPID( aStatistics,
                            aSpaceID,
                            SD_MAKE_PID(aRID),
                            aRetPage,
                            aTrySuccess  )
              != IDE_SUCCESS );

    *aRetPage += SD_MAKE_OFFSET( aRID );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC sdbBufferMgr::fixPageByPID( idvSQL             * aStatistics,
                                   scSpaceID            aSpaceID,
                                   scPageID             aPageID,
                                   UChar             ** aRetPage )
{
    IDE_RC rc;

    IDV_SQL_OPTIME_BEGIN( aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE );

    rc = mBufferPool.fixPage( aStatistics,
                              aSpaceID,
                              aPageID,
                              aRetPage );

    IDV_SQL_OPTIME_END( aStatistics, IDV_OPTM_INDEX_DRDB_FIX_PAGE );

    return rc;
}

#if 0 //not used!
//
///******************************************************************************
// * Description :
// *  Pinning�� ������ �Լ�.. (�ڼ��Ѱ��� proj-1568 ���� �޴��� ����)
// *  Pin ����� ����ϰ��� �ϴ� �ʿ��� �Ʒ��� ȣ���Ͽ� aEnv�� �����Ѵ���,
// *  ���� ���ٶ� ���ʹ� aEnv�� ���ؼ� �����Ѵ�.
// *  ����!!
// *      ���� �׽�Ʈ�� �ȵǾ� �ִ� �ڵ��̴�.
// *
// *  aEnv    - [OUT] ������ sdbPinningEnv
// ******************************************************************************/
//IDE_RC sdbBufferMgr::createPinningEnv(void **aEnv)
//{
//    UInt           sPinningCount = smuProperty::getBufferPinningCount();
//    UInt           i;
//    sdbPinningEnv *sPinningEnv;
//
//    if (sPinningCount > 0)
//    {
//        // ���Լ��� ȣ����� �����Ƿ� limit point ��������.
//        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDB,
//                                   ID_SIZEOF(sdbPinningEnv),
//                                   aEnv)
//                 != IDE_SUCCESS);
//
//        sPinningEnv = (sdbPinningEnv*)*aEnv;
//
//        sPinningEnv->mAccessInsPos = 0;
//        sPinningEnv->mAccessHistoryCount = smuProperty::getBufferPinningHistoryCount();
//        sPinningEnv->mPinningInsPos = 0;
//        sPinningEnv->mPinningCount = sPinningCount;
//
//        // ���Լ��� ȣ����� �����Ƿ� limit point ��������.
//        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDB,
//                                   ID_SIZEOF(sdbPageID) * sPinningEnv->mAccessHistoryCount,
//                                   (void**)&sPinningEnv->mAccessHistory)
//                 != IDE_SUCCESS);
//
//        // ���Լ��� ȣ����� �����Ƿ� limit point ��������.
//        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDB,
//                                   ID_SIZEOF(sdbPageID) * sPinningEnv->mPinningCount,
//                                   (void**)&sPinningEnv->mPinningList)
//                 != IDE_SUCCESS);
//
//        // ���Լ��� ȣ����� �����Ƿ� limit point ��������.
//        IDE_TEST(iduMemMgr::malloc(IDU_MEM_SM_SDB,
//                                   ID_SIZEOF(sdbBCB*) * sPinningEnv->mPinningCount,
//                                   (void**)&sPinningEnv->mPinningBCBList)
//                 != IDE_SUCCESS);
//
//        for (i = 0; i < sPinningEnv->mAccessHistoryCount; i++)
//        {
//            sPinningEnv->mAccessHistory[i].mSpaceID = SC_NULL_SPACEID;
//            sPinningEnv->mAccessHistory[i].mPageID = SC_NULL_PID;
//        }
//        for (i = 0; i < sPinningEnv->mPinningCount; i++)
//        {
//            sPinningEnv->mPinningList[i].mSpaceID = SC_NULL_SPACEID;
//            sPinningEnv->mPinningList[i].mPageID = SC_NULL_PID;
//            sPinningEnv->mPinningBCBList[i] = NULL;
//        }
//    }
//    else
//    {
//        *aEnv = NULL;
//    }
//
//    return IDE_SUCCESS;
//
//    IDE_EXCEPTION_END;
//
//    return IDE_FAILURE;
//}
//
///******************************************************************************
// * Description :
// *  Pinning�� ������ �Լ�.. (�ڼ��Ѱ��� proj-1568 ���� �޴��� ����)
// *  createPinningEnv���� ������ aEnv�� destroy�ϴ� �ڵ�.
// *  createPinningEnv�Ѱ��� "�ݵ��" destroyPinningEnvȣ���ؼ� destroy�ؾ� �Ѵ�.
// *  �׷��� ������ �ɰ��� ��Ȳ �ʷ�(�ش� BCB�� �ٸ�Tx�� replace �� �� ���� �ȴ�.)
// *  ����!!
// *      ���� �׽�Ʈ�� �ȵǾ� �ִ� �ڵ��̴�.
// *  aStatistics - [IN]  �������
// *  aEnv        - [IN]  createPinningEnv ���� ������ ����
// ******************************************************************************/
//IDE_RC sdbBufferMgr::destroyPinningEnv(idvSQL *aStatistics, void *aEnv)
//{
//    sdbBCB        *sBCB;
//    UInt           i;
//    sdbPinningEnv *sPinningEnv;
//
//    if (aEnv != NULL)
//    {
//        sPinningEnv = (sdbPinningEnv*)aEnv;
//        for (i = 0; i < sPinningEnv->mPinningCount; i++)
//        {
//            if ((sPinningEnv->mPinningList[i].mSpaceID == SC_NULL_SPACEID) &&
//                (sPinningEnv->mPinningList[i].mPageID == SC_NULL_PID))
//            {
//                break;
//            }
//            sBCB = sPinningEnv->mPinningBCBList[i];
//            IDE_DASSERT(sBCB != NULL);
//
//            sBCB->lockBCBMutex(aStatistics);
//            sBCB->decFixCnt();
//            sBCB->unlockBCBMutex();
//        }
//
//        IDE_TEST(iduMemMgr::free(sPinningEnv->mAccessHistory) != IDE_SUCCESS);
//        IDE_TEST(iduMemMgr::free(sPinningEnv->mPinningList) != IDE_SUCCESS);
//        IDE_TEST(iduMemMgr::free(sPinningEnv->mPinningBCBList) != IDE_SUCCESS);
//        IDE_TEST(iduMemMgr::free(sPinningEnv) != IDE_SUCCESS);
//    }
//
//    return IDE_SUCCESS;
//
//    IDE_EXCEPTION_END;
//
//    return IDE_FAILURE;
//}
//
///****************************************************************
// * Description :
// *  full scan�� ��쿡�� LRU�� ������ �� ���ɼ��� �ִ�. �׷��Ƿ�,
// *  �̰�쿡�� LRU���ٴ� MRU�� ȣ���� �ϰ� �ȴ�.
// *  �� �Լ��� ���ؼ� ���� BCB�� ���� Ʈ������� replace�� ��û�Ҷ�,
// *  victim���� ������ Ȯ���� ����.
// *
// *  �� ����� ���۸Ŵ����� Multiple Buffer Read(MBR)����� ���ؼ� ����
// *  �� ����. ���� MBR�� ����Ǿ� ���� �ʴ�.
// *
// *  aStatistics - [IN]  �������
// *  aSpaceID    - [IN]  table space ID
// *  aPageID     - [IN]  page ID
// *  aLatchMode  - [IN]  ��ġ ���, shared, exclusive, nolatch�� �ִ�.
// *  aWaitMode   - [IN]  ��ġ�� ��� ���� ��� ���� ���� ����
// *  aMtx        - [IN]  Mini transaction
// *  aRetPage    - [OUT] ��û�� ������
// *  aTrySuccess - [OUT] Latch mode�� try�ΰ�쿡 �������θ���
// ****************************************************************/
//IDE_RC sdbBufferMgr::getPageWithMRU( idvSQL             *aStatistics,
//                                     scSpaceID           aSpaceID,
//                                     scPageID            aPageID,
//                                     sdbLatchMode        aLatchMode,
//                                     sdbWaitMode         aWaitMode,
//                                     void               *aMtx,
//                                     UChar             **aRetPage,
//                                     idBool             *aTrySuccess)
//{
//    return getPageByPID( aStatistics,
//                         aSpaceID,
//                         aPageID,
//                         aLatchMode,
//                         aWaitMode,
//                         SDB_SINGLE_PAGE_READ,
//                         aMtx,
//                         aRetPage,
//                         aTrySuccess,
//                         NULL /*IsCorruptPage*/);
//}
#endif
