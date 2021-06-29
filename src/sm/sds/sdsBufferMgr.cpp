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
 * $Id$
 **********************************************************************/

/***********************************************************************
 * PROJ-2102 The Fast Secondary Buffer 
 **********************************************************************/

#include <idl.h>
#include <ide.h>
#include <iduLatch.h>
#include <smErrorCode.h>

#include <smDef.h>
#include <sdb.h>
#include <smrRecoveryMgr.h>
#include <sds.h>
#include <sdsReq.h>

#define SDS_REMOVE_BCB_WAIT_USEC   (50000)

UInt              sdsBufferMgr::mPageSize;
UInt              sdsBufferMgr::mFrameCntInExtent;
UInt              sdsBufferMgr::mCacheType;
sdbCPListSet      sdsBufferMgr::mCPListSet;
sdbBCBHash        sdsBufferMgr::mHashTable;
sdsBufferArea     sdsBufferMgr::mSBufferArea;
sdsFile           sdsBufferMgr::mSBufferFile;
sdsMeta           sdsBufferMgr::mMeta;
sdsBufferMgrStat  sdsBufferMgr::mStatistics;

sdsSBufferServieStage sdsBufferMgr::mServiceStage;

/******************************************************************************
 * Description : Secondary Buffer Manager �� �ʱ�ȭ �Ѵ�.
 ******************************************************************************/
IDE_RC sdsBufferMgr::initialize()
{
    SChar     * sPath;
    UInt        sHashBucketDensity;
    UInt        sHashLatchDensity;
    UInt        sCPListCnt;
    UInt        sSBufferEnable;
    UInt        sSBufferPageCnt; 
    UInt        sSBufferExtentCount;
    UInt        sExtCntInGroup; 
    UInt        sSBufferGroupCnt; 
    UInt        sHashBucketCnt;
    SInt        sState  =  0;
 
    mPageSize = SD_PAGE_SIZE;
    /* Secondary Buffer Enable */
    sSBufferEnable  = smuProperty::getSBufferEnable();
    /* Secondary Buffer Size */
    sSBufferPageCnt = smuProperty::getSBufferSize()/mPageSize;
    /* sBuffer file���ִ� ���丮 */    
    sPath = (SChar *)smuProperty::getSBufferDirectoryName();
    /* �ϳ��� hash bucket�� ���� BCB���� */
    sHashBucketDensity = smuProperty::getBufferHashBucketDensity();
    /* �ϳ��� hash chains latch�� BCB���� */
    sHashLatchDensity = smuProperty::getBufferHashChainLatchDensity();
    /* Check point list */
    sCPListCnt = smuProperty::getBufferCheckPointListCnt();
    /* flush �ѹ��� �������� page ���� */    
    mFrameCntInExtent = smuProperty::getBufferIOBufferSize(); 
    /* all/dirty/clean type */
    mCacheType = smuProperty::getSBufferType();

    /* Property�� On �� �Ǿ��ְ� 
     * ����� page count�� ��ΰ� valid �� ��츸 identify�� �����Ѵ�. */
    if( (sSBufferEnable == SMU_SECONDARY_BUFFER_ENABLE ) &&
        ( (sSBufferPageCnt != 0) && (idlOS::strcmp(sPath, "") != 0) ) )
    {
        setIdentifiable(); 
    }
    else
    {
        setUnserviceable();
        IDE_CONT( SKIP_UNSERVICEABLE );
    }

    if( sSBufferPageCnt < SDS_META_MAX_CHUNK_PAGE_COUNT )
    {
        sSBufferPageCnt = SDS_META_MAX_CHUNK_PAGE_COUNT;
    } 

    /* MetaTable�� �����Ҽ� �ִ� ������ ũ��� align down */
    sExtCntInGroup      = SDS_META_MAX_CHUNK_PAGE_COUNT / mFrameCntInExtent;
    sSBufferGroupCnt    = sSBufferPageCnt / (sExtCntInGroup * mFrameCntInExtent); 
    sSBufferExtentCount = sSBufferGroupCnt * sExtCntInGroup;
    sSBufferPageCnt     = sSBufferExtentCount * mFrameCntInExtent;
    sHashBucketCnt      = sSBufferPageCnt / sHashBucketDensity;

    IDE_TEST( mHashTable.initialize( sHashBucketCnt,
                                     sHashLatchDensity,
                                     SD_LAYER_SECONDARY_BUFFER )
              != IDE_SUCCESS );
    sState = 1;
    
    IDE_TEST( mCPListSet.initialize( sCPListCnt, SD_LAYER_SECONDARY_BUFFER ) 
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mSBufferArea.initializeStatic( sSBufferExtentCount,
                                             sSBufferPageCnt /*aBCBCount*/)               
              != IDE_SUCCESS );
    sState = 3; 

    IDE_TEST( mSBufferFile.initialize( sSBufferGroupCnt,
                                       sSBufferPageCnt ) != IDE_SUCCESS );
    sState = 4;

    IDE_TEST( mMeta.initializeStatic( sSBufferGroupCnt, 
                                      sExtCntInGroup, 
                                      &mSBufferFile, 
                                      &mSBufferArea, 
                                      &mHashTable ) 
              != IDE_SUCCESS );
    sState = 5;

    mStatistics.initialize( &mSBufferArea, &mHashTable, &mCPListSet );

    IDE_EXCEPTION_CONT( SKIP_UNSERVICEABLE );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {   
        case 5:
            IDE_ASSERT( mMeta.destroyStatic() == IDE_SUCCESS );
        case 4:
            IDE_ASSERT( mSBufferFile.destroy() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( mSBufferArea.destroyStatic() == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( mCPListSet.destroy() == IDE_SUCCESS );
        case 1:
            IDE_ASSERT( mHashTable.destroy() == IDE_SUCCESS );
        default:
            break;
    }

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  Secondary BufferManager�� �����Ѵ�.
 ******************************************************************************/
IDE_RC sdsBufferMgr::destroy()
{
    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP_SUCCESS );

    IDE_ASSERT( mMeta.destroyStatic() == IDE_SUCCESS );

    IDE_ASSERT( mSBufferFile.destroyStatic() == IDE_SUCCESS );

    IDE_ASSERT( mSBufferArea.destroyStatic() == IDE_SUCCESS );

    IDE_ASSERT( mCPListSet.destroy() == IDE_SUCCESS );

    IDE_ASSERT( mHashTable.destroy() == IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :Server Shutdown�� MetaTable ��ü�� update �Ѵ�
 ****************************************************************/
IDE_RC sdsBufferMgr::finalize()
{
    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP_SUCCESS );

    IDE_TEST( mMeta.finalize( NULL /*aStatistics*/) != IDE_SUCCESS ); 

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description:
 *  BCB�� free ���·� �����Ѵ�. (BCB�� ���ۿ��� ����)
 *  free�� BCB �̹Ƿ� DIRTY ���¿����� �ȵȴ�.
 *  removeBCB�� ����ϴ�.
 *  aSBCB      [IN] : FREE ��ų BCB
 ****************************************************************/
IDE_RC sdsBufferMgr::makeFreeBCB( idvSQL     * aStatistics,
                                  sdsBCB     * aSBCB )
{
    sdsHashChainsLatchHandle  * sHashChainsHandle = NULL;
    PDL_Time_Value   sTV;
    scSpaceID        sSpaceID; 
    scPageID         sPageID; 
    UInt             sState = 0;

    IDE_ASSERT( aSBCB != NULL );

    sSpaceID = aSBCB->mSpaceID; 
    sPageID  = aSBCB->mPageID; 

retry:
    sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                         sSpaceID,
                                                         sPageID );
    sState = 1;
    aSBCB->lockBCBMutex( aStatistics );
    sState = 2;

    switch( aSBCB->mState )
    {
        case SDS_SBCB_CLEAN:
            /* �Ѱܹ��� BCB�� lock ���� BCB�� ���ƾ� �Ѵ�. */
            IDE_DASSERT( ( sSpaceID == aSBCB->mSpaceID ) &&
                         ( sPageID  == aSBCB->mPageID ) );

            mHashTable.removeBCB( aSBCB );

            aSBCB->setFree();
            break;

        case SDS_SBCB_INIOB:
        case SDS_SBCB_OLD:
            /* �Ѱܹ��� BCB�� lock ���� BCB�� ���ƾ� �Ѵ�. */
            IDE_DASSERT( ( sSpaceID == aSBCB->mSpaceID ) &&
                         ( sPageID  == aSBCB->mPageID ) );

            sState = 1;
            aSBCB->unlockBCBMutex();
            sState = 0;
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;

            sTV.set( 0, SDS_REMOVE_BCB_WAIT_USEC ); /* microsec */
            idlOS::sleep( sTV );
            goto retry;

            break;

        case SDS_SBCB_FREE:
            /* nothing to do */
            break;

        case SDS_SBCB_DIRTY:
        default:
            IDE_RAISE( ERROR_INVALID_BCD );
            break;
    }

    sState = 1;
    aSBCB->unlockBCBMutex();
    sState = 0;
    mHashTable.unlockHashChainsLatch( sHashChainsHandle );
    sHashChainsHandle = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                     "invalid bcd to make free \n"
                     "sSpaseID:%u\n"
                     "sPageID :%u\n"
                     "ID      :%u\n"
                     "spaseID :%u\n"
                     "pageID  :%u\n"
                     "state   :%u\n"
                     "CPListNo:%u\n"
                     "HashTableNo:%u\n",
                     sSpaceID,
                     sPageID,
                     aSBCB->mSBCBID,
                     aSBCB->mSpaceID,
                     aSBCB->mPageID,
                     aSBCB->mState,
                     aSBCB->mCPListNo,
                     aSBCB->mHashBucketNo );
        IDE_DASSERT(0);
    }
    IDE_EXCEPTION_END;
    
    switch( sState )
    {
        case 2:
            aSBCB->unlockBCBMutex();
        case 1:
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
            break;
    }

    return IDE_FAILURE;
}

/****************************************************************
 * Description:BCB �� Free ���·� �ٲ۴�. 
 * ���´� OLD �� �����ϰ�  OLD�� �Ǵ� ������ �̹� hash������
 * ���������Ƿ� hash���� ����� ������ �ʿ����� �ʴ�.
 * aSBCB        [IN] : ���� BCB
 ****************************************************************/
IDE_RC sdsBufferMgr::makeFreeBCBForce( idvSQL     * aStatistics,
                                       sdsBCB     * aSBCB )
{
    IDE_ASSERT( aSBCB != NULL );

    IDE_TEST_RAISE( aSBCB->mState != SDS_SBCB_OLD, ERROR_INVALID_BCD );

    if( aSBCB->mCPListNo != SDB_CP_LIST_NONE )
    {
        mCPListSet.remove( aStatistics, aSBCB );
    }

    aSBCB->setFree();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                     "invalid bcd to make free(Force)\n"
                     "ID      :%"ID_UINT32_FMT"\n"
                     "spaseID :%"ID_UINT32_FMT"\n"
                     "pageID  :%"ID_UINT32_FMT"\n"
                     "state   :%"ID_UINT32_FMT"\n"
                     "CPListNo:%"ID_UINT32_FMT"\n"
                     "HashTableNo:%"ID_UINT32_FMT"\n",
                     aSBCB->mSBCBID,
                     aSBCB->mSpaceID,
                     aSBCB->mPageID,
                     aSBCB->mState,
                     aSBCB->mCPListNo,
                     aSBCB->mHashBucketNo );
        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description : victim�� ���´�.
 *  aBCBIndex  [IN]  BCB�� index 
 *  aSBCB      [OUT] ���� ������ BCB
 ****************************************************************/
void sdsBufferMgr::getVictim( idvSQL     * aStatistics,
                              UInt         aBCBIndex,
                              sdsBCB    ** aSBCB )
{
    sdsBCB * sVictimBCB = NULL;

    sVictimBCB = mSBufferArea.getBCB( aBCBIndex );
    /* �б� ���� BCB�� victim ����� �ɼ����� */
    sVictimBCB->lockReadIOMutex( aStatistics );
    (void)makeFreeBCB( aStatistics, sVictimBCB );
    sVictimBCB->unlockReadIOMutex();

    *aSBCB = sVictimBCB;
}

/****************************************************************
 * Description : Secondary Buffer���� Buffer Frame���� �������� ����
 *  aSBCB           [IN]  Secondary Buffer�� BCB
 *  aBCB            [IN]  �ø� ��� BCB
 *  aIsCorruptRead  [Out] �ٸ� BCB�� �ƴ��� ���� 
 *  aIsCorruptPage  [Out] ������ Corrupt ���� 
 ****************************************************************/
IDE_RC sdsBufferMgr::moveUpPage( idvSQL     * aStatistics,
                                 sdsBCB    ** aSBCB,
                                 sdbBCB     * aBCB,
                                 idBool     * aIsCorruptRead,
                                 idBool     * aIsCorruptPage )
{
    scSpaceID   sSpaceID = aBCB->mSpaceID;
    scPageID    sPageID  = aBCB->mPageID;
    idvTime     sBeginTime;
    idvTime     sEndTime; 
    ULong       sReadTime         = 0;
    ULong       sCalcChecksumTime = 0;
    UInt        sState            = 0;
    idBool      sIsLock        = ID_FALSE;
    idBool      sIsCorruptRead = ID_FALSE;
    idBool      sIsCorruptPage = ID_FALSE;
    sdsBCB    * sExistSBCB     = *aSBCB;

    IDE_TEST( isServiceable() != ID_TRUE );

    /* �б� ���� BCB�� victim ����� �Ǹ� �ȵȴ�. */
    sExistSBCB->lockReadIOMutex( aStatistics );
    sIsLock = ID_TRUE;
    
    if ( ( sExistSBCB->mSpaceID != sSpaceID ) ||
         ( sExistSBCB->mPageID != sPageID ) )
    {
        sIsLock = ID_FALSE;
        sExistSBCB->unlockReadIOMutex();

        IDE_TEST( findBCB( aStatistics, sSpaceID, sPageID, &sExistSBCB )
                  != IDE_SUCCESS );

        if ( sExistSBCB == NULL )
        {
            sIsCorruptRead = ID_TRUE;        
            IDE_CONT( SKIP_CORRUPT_READ );
        }
        else 
        {
            *aSBCB = sExistSBCB;

            sExistSBCB->lockReadIOMutex( aStatistics );
            sIsLock = ID_TRUE;
        }
    }

    IDE_ASSERT( sExistSBCB->mState != SDS_SBCB_OLD );

    IDV_TIME_GET(&sBeginTime);

    mStatistics.applyBeforeSingleReadPage( aStatistics );
    sState = 1;

    IDE_TEST_RAISE( mSBufferFile.open( aStatistics ) != IDE_SUCCESS,
                    ERROR_PAGE_MOVEUP );

    IDE_TEST_RAISE( mSBufferFile.read( aStatistics,
                                 sExistSBCB->mSBCBID,   /* ID */
                                 mMeta.getFrameOffset( sExistSBCB->mSBCBID ),  
                                 1,                     /* page count */
                                 aBCB->mFrame )         /* to */
                    != IDE_SUCCESS,
                    ERROR_PAGE_MOVEUP );

    sState = 0;
    mStatistics.applyAfterSingleReadPage( aStatistics );

    IDV_TIME_GET(&sEndTime);
    sReadTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
    sBeginTime = sEndTime;

    if( sdpPhyPage::checkAndSetPageCorrupted( aBCB->mSpaceID,
                                              aBCB->mFrame ) 
        == ID_TRUE )
    {   
        sIsCorruptPage = ID_TRUE;

        if ( smLayerCallback::getCorruptPageReadPolicy()
             != SDB_CORRUPTED_PAGE_READ_PASS )
        {
            IDE_RAISE( ERROR_PAGE_CORRUPTION );
        }
    }
#ifdef DEBUG
    IDE_TEST_RAISE(
        sdpPhyPage::isConsistentPage( (UChar*) sdpPhyPage::getHdr( aBCB->mFrame ) )
        == ID_FALSE ,
        ERROR_PAGE_INCONSISTENT  );
#endif
    IDE_DASSERT( validate( aBCB ) == IDE_SUCCESS );

    setFrameInfoAfterReadPage( sExistSBCB,
                               aBCB,
                               ID_TRUE ); // check to online tablespace

    IDV_TIME_GET( &sEndTime );
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO( &sBeginTime, &sEndTime );

    mStatistics.applyReadPages( sCalcChecksumTime,
                                sReadTime );

    sIsLock = ID_FALSE;
    sExistSBCB->unlockReadIOMutex();

    IDE_EXCEPTION_CONT( SKIP_CORRUPT_READ );

    if(aIsCorruptRead != NULL )
    {
        *aIsCorruptRead = sIsCorruptRead;
    } 
    if( aIsCorruptPage != NULL ) 
    {
        *aIsCorruptPage = sIsCorruptPage;
    }

    return IDE_SUCCESS;
   
    IDE_EXCEPTION( ERROR_PAGE_MOVEUP );
    {
        setUnserviceable();
        IDE_SET( ideSetErrorCode( smERR_ABORT_PageMoveUpStopped ) );
    }
    IDE_EXCEPTION( ERROR_PAGE_CORRUPTION );
    {
        if( aIsCorruptPage != NULL ) 
        {
            *aIsCorruptPage = ID_TRUE;
        }

        switch ( smLayerCallback::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
                IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                          aBCB->mSpaceID,
                                          aBCB->mPageID ) );

                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                // PROJ-1665 : ABORT Error�� ��ȯ��
                //����: ����������, mFrame�� �޴� �ʿ��� �̰� ȣ���ؼ� ���.
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          aBCB->mSpaceID,
                                          aBCB->mPageID ) );
                break;
            default:
                break;
        }
    }
#ifdef DEBUG
    IDE_EXCEPTION( ERROR_PAGE_INCONSISTENT );
    {
        sdpPhyPageHdr   * sPhyPageHdr;
        sPhyPageHdr = sdpPhyPage::getHdr(aBCB->mFrame);

        ideLog::log( IDE_SM_0,
                 "moveup : Page Dump -----------------\n"
                 "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                 "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                 "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                 "Total Free Size           : %"ID_UINT32_FMT"\n"
                 "Available Free Size       : %"ID_UINT32_FMT"\n"
                 "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                 "Size of CTL               : %"ID_UINT32_FMT"\n"
                 "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                 "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                 "Page Type                 : %"ID_UINT32_FMT"\n"
                 "Page State                : %"ID_UINT32_FMT"\n"
                 "Page ID                   : %"ID_UINT32_FMT"\n"
                 "is Consistent             : %"ID_UINT32_FMT"\n"
                 "LinkState                 : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                 "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                 "TableOID                  : %"ID_vULONG_FMT"\n"
                 "Seq No                    : %"ID_UINT64_FMT"\n"
                 "---------------------------------------------\n",
                 (UChar*)sPhyPageHdr,
                 sPhyPageHdr->mFrameHdr.mCheckSum,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                 sPhyPageHdr->mFrameHdr.mIndexSMONo,
                 sPhyPageHdr->mFrameHdr.mSpaceID,
                 sPhyPageHdr->mFrameHdr.mBCBPtr,
                 sPhyPageHdr->mTotalFreeSize,
                 sPhyPageHdr->mAvailableFreeSize,
                 sPhyPageHdr->mLogicalHdrSize,
                 sPhyPageHdr->mSizeOfCTL,
                 sPhyPageHdr->mFreeSpaceBeginOffset,
                 sPhyPageHdr->mFreeSpaceEndOffset,
                 sPhyPageHdr->mPageType,
                 sPhyPageHdr->mPageState,
                 sPhyPageHdr->mPageID,
                 sPhyPageHdr->mIsConsistent,
                 sPhyPageHdr->mLinkState,
                 sPhyPageHdr->mParentInfo.mParentPID,
                 sPhyPageHdr->mParentInfo.mIdxInParent,
                 sPhyPageHdr->mListNode.mPrev,
                 sPhyPageHdr->mListNode.mNext,
                 sPhyPageHdr->mTableOID,
                 sPhyPageHdr->mSeqNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aBCB->mSpaceID,
                                  aBCB->mPageID) );
    }
#endif
    IDE_EXCEPTION_END;

    if( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        sExistSBCB->unlockReadIOMutex();
    }
 
    if ( sState != 0 )
    {
        mStatistics.applyAfterSingleReadPage( aStatistics );
    }
    
    return IDE_FAILURE;
}

/****************************************************************
 * Description : Buffer Frame����  Secondary Buffer�� ����
 *  aStatistics - [IN] �������
 *  aBCB          [IN] ������ ��� BCB  
 *  aBuffer       [IN] ������ ��� Frame ����
 *  aExtentIndex  [IN] ������ ��ġ (extent)
 *  aFrameIndex   [IN] ������ ��ġ (frame)
 *  aSBCB         [OUT] ������ SBCB
 ****************************************************************/
IDE_RC sdsBufferMgr::moveDownPage( idvSQL       * aStatistics,
                                   sdbBCB       * aBCB,
                                   UChar        * aBuffer,
                                   ULong          aExtentIndex, 
                                   UInt           aFrameIndex,
                                   sdsBCB      ** aSBCB ) 
{
    sdsBCB      * sSBCB = NULL;
    sdsBCB      * sExistSBCB = NULL;
    scSpaceID     sSpaceID = aBCB->mSpaceID;
    scPageID      sPageID  = aBCB->mPageID;
    UInt          sState   = 0;
    idvTime       sBeginTime;
    idvTime       sEndTime;
    ULong         sWriteTime = 0;
    ULong         sCalcChecksumTime = 0;

    sdsHashChainsLatchHandle  * sHashChainsHandle = NULL;

    /* 1.������ ���� �������� ������ ���� �ִ� Ȯ�� �Ѵ�.*/
    IDE_TEST( findBCB( aStatistics, sSpaceID, sPageID, &sExistSBCB )
              != IDE_SUCCESS );

    /* 2.���� �������� �־��ٸ� ������ �Ѵ�. */
    if ( sExistSBCB != NULL )
    {
        (void)removeBCB( aStatistics, sExistSBCB );
    }

    /* 3.target Extent���� �� Frame�� ��� �´�.*/
    getVictim( aStatistics, 
               (aExtentIndex*mFrameCntInExtent)+aFrameIndex, 
               &sSBCB );

    /* 4.getVictim �� �� �����ؾ� �Ѵ�. */
    IDE_ASSERT( sSBCB != NULL );

    IDV_TIME_GET( &sBeginTime );

    IDE_TEST_RAISE( mSBufferFile.open( aStatistics ) != IDE_SUCCESS,
                    ERROR_PAGE_MOVEDOWN );

    mStatistics.applyBeforeSingleWritePage( aStatistics );
    sState = 1;
    
    /* 5.Secondary Buffer �� ��������. */
    IDE_TEST_RAISE( mSBufferFile.write( aStatistics,
                                        sSBCB->mSBCBID,   /* ID */
                                        mMeta.getFrameOffset( sSBCB->mSBCBID ),
                                        1,                /* page count */ 
                                        aBuffer ) 
                    != IDE_SUCCESS,
                    ERROR_PAGE_MOVEDOWN );         

    sState = 0;
    mStatistics.applyAfterSingleWritePage( aStatistics );

    IDV_TIME_GET( &sEndTime );
    sWriteTime = IDV_TIME_DIFF_MICRO( &sBeginTime, &sEndTime );
    sBeginTime = sEndTime;
   
    /* 6.SBCB ���� ���� */
    /*  hash ���� ������ PBuffer���� BCB�� �ִ� ������ �̿��� �����Ҽ��־ 
        lock�� ��� BCB�� ���� �ѵ� hash�� ����.. */
    sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                         sSpaceID,
                                                         sPageID );
    sSBCB->lockBCBMutex( aStatistics );

    sSBCB->mSpaceID = sSpaceID;
    sSBCB->mPageID  = sPageID;
    SM_GET_LSN( sSBCB->mRecoveryLSN, aBCB->mRecoveryLSN );
    SM_GET_LSN( sSBCB->mPageLSN, smLayerCallback::getPageLSN( aBCB->mFrame ) );
    sSBCB->mBCB = aBCB;
    *aSBCB = sSBCB;

    if( aBCB->mPrevState == SDB_BCB_CLEAN ) 
    {
        sSBCB->mState  = SDS_SBCB_CLEAN;
    }
    else
    {
        IDE_DASSERT( aBCB->mPrevState == SDB_BCB_DIRTY ); 
      
        sSBCB->mState  = SDS_SBCB_DIRTY;

        if( ( !SM_IS_LSN_INIT(sSBCB->mRecoveryLSN) ) )
        {
            mCPListSet.add( aStatistics, sSBCB );
        }
    }
   
    /* 7. Hash �� ���� */
    mHashTable.insertBCB( sSBCB, (void**)&sExistSBCB );

    if( sExistSBCB != NULL )
    {
#ifdef DEBUG
        /* #2 ���� �����⿡ �߻��Ҽ� ���� :
         * �����ɰ� ������ Debug������ ���ó�� �ϰ� ���� */
        IDE_RAISE( ERROR_INVALID_BCD )
#endif
        (void)removeBCB( aStatistics, sExistSBCB );

        mHashTable.insertBCB( aSBCB, (void**)&sExistSBCB );
    }

    sSBCB->unlockBCBMutex();
    mHashTable.unlockHashChainsLatch( sHashChainsHandle );
    sHashChainsHandle = NULL;

    IDV_TIME_GET( &sEndTime );
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO( &sBeginTime, &sEndTime );

    mStatistics.applyWritePages( sCalcChecksumTime,
                                 sWriteTime );

#ifdef DEBUG
    /* �̹� Ȯ���ϰ� ���������� ����׿����� �ٽ� Ȯ�� */
    IDE_TEST_RAISE(
        sdpPhyPage::isConsistentPage( (UChar*) sdpPhyPage::getHdr( aBCB->mFrame ) )
        == ID_FALSE ,
        ERROR_PAGE_INCONSISTENT  );
#endif
    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_PAGE_MOVEDOWN );
    {
        setUnserviceable();
        IDE_SET( ideSetErrorCode( smERR_ABORT_PageMovedownStopped ) );
    }
#ifdef DEBUG
    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                     "redundant Exist SBCB \n"
                     "ID      :%u\n"
                     "spaseID :%u\n"
                     "pageID  :%u\n"
                     "state   :%u\n"
                     "CPListNo:%u\n"
                     "HashTableNo:%u\n",
                     sExistSBCB->mSBCBID,
                     sExistSBCB->mSpaceID,
                     sExistSBCB->mPageID,
                     sExistSBCB->mState,
                     sExistSBCB->mCPListNo,
                     sExistSBCB->mHashBucketNo );
    }
    IDE_EXCEPTION( ERROR_PAGE_INCONSISTENT );
    {
        sdpPhyPageHdr   * sPhyPageHdr;
        sPhyPageHdr = sdpPhyPage::getHdr(aBCB->mFrame);
        ideLog::log( IDE_SM_0,
                 "movedown : Page Dump -----------------\n"
                 "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                 "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                 "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                 "Total Free Size           : %"ID_UINT32_FMT"\n"
                 "Available Free Size       : %"ID_UINT32_FMT"\n"
                 "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                 "Size of CTL               : %"ID_UINT32_FMT"\n"
                 "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                 "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                 "Page Type                 : %"ID_UINT32_FMT"\n"
                 "Page State                : %"ID_UINT32_FMT"\n"
                 "Page ID                   : %"ID_UINT32_FMT"\n"
                 "is Consistent             : %"ID_UINT32_FMT"\n"
                 "LinkState                 : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                 "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                 "TableOID                  : %"ID_vULONG_FMT"\n"
                 "Seq No                    : %"ID_UINT64_FMT"\n"
                 "---------------------------------------------\n",
                 (UChar*)sPhyPageHdr,
                 sPhyPageHdr->mFrameHdr.mCheckSum,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                 sPhyPageHdr->mFrameHdr.mIndexSMONo,
                 sPhyPageHdr->mFrameHdr.mSpaceID,
                 sPhyPageHdr->mFrameHdr.mBCBPtr,
                 sPhyPageHdr->mTotalFreeSize,
                 sPhyPageHdr->mAvailableFreeSize,
                 sPhyPageHdr->mLogicalHdrSize,
                 sPhyPageHdr->mSizeOfCTL,
                 sPhyPageHdr->mFreeSpaceBeginOffset,
                 sPhyPageHdr->mFreeSpaceEndOffset,
                 sPhyPageHdr->mPageType,
                 sPhyPageHdr->mPageState,
                 sPhyPageHdr->mPageID,
                 sPhyPageHdr->mIsConsistent,
                 sPhyPageHdr->mLinkState,
                 sPhyPageHdr->mParentInfo.mParentPID,
                 sPhyPageHdr->mParentInfo.mIdxInParent,
                 sPhyPageHdr->mListNode.mPrev,
                 sPhyPageHdr->mListNode.mNext,
                 sPhyPageHdr->mTableOID,
                 sPhyPageHdr->mSeqNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aBCB->mSpaceID,
                                  aBCB->mPageID) );
    }
#endif
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        mStatistics.applyAfterSingleWritePage( aStatistics );
    }
    
    IDE_ASSERT( 0 );

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  pageOut�� �ϱ� ���� �ش� Queue�� �����Ѵ�.
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �� �Լ� �����ϴµ� �ʿ��� �ڷ�.
 ****************************************************************/
IDE_RC sdsBufferMgr::makePageOutTargetQueueFunc( sdsBCB *aBCB,
                                                 void   *aObj )
{
    sdsPageOutObj * sObj = (sdsPageOutObj*)aObj;
    sdsSBCBState    sState;

    if( sObj->mFilter( aBCB , sObj->mEnv ) == ID_TRUE )
    {
        sState = aBCB->mState;

        /* Flush ����� (+ flush�� �Ϸ�Ǳ⸦ ��ٷ��� �ϴ� ) SBCB
          �� ã�� Flush Queue �� ��������.*/
        if( ( sState == SDS_SBCB_DIRTY ) ||
            ( sState == SDS_SBCB_INIOB ) ||
            ( sState == SDS_SBCB_OLD ) )

        {
            IDE_ASSERT(
                sObj->mQueueForFlush.enqueue( ID_FALSE, //mutex�� ���� �ʴ´�.
                                              (void*)&aBCB )
                        == IDE_SUCCESS );
        }
        else 
        {
            /* nothing to do */
        }
 
        /* ���͸� �����ϴ� ��� BCB�� Free ������� �����Ѵ� */ 
        IDE_ASSERT(
            sObj->mQueueForMakeFree.enqueue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                             (void*)&aBCB )
            == IDE_SUCCESS );
    }

    return IDE_SUCCESS;
}

/******************************************************************************
 * Description : filter function �� �̿��� dirty�� BCB�� ť�� �����Ѵ�.
 * aFunc    - [IN]  ���� area�� �� BCB�� ������ �Լ�
 * aObj     - [IN]  aFunc�����Ҷ� �ʿ��� ����
 ******************************************************************************/
IDE_RC sdsBufferMgr::makeFlushTargetQueueFunc( sdsBCB * aBCB,
                                               void   * aObj )
{
    sdsFlushObj   * sObj = (sdsFlushObj*)aObj;
    sdsSBCBState    sState;

    if( sObj->mFilter( aBCB , sObj->mEnv) == ID_TRUE )
    {
        sState = aBCB->mState;
        if( sState == SDS_SBCB_DIRTY )
        {
            IDE_ASSERT( sObj->mQueue.enqueue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                              (void*)&aBCB )
                        == IDE_SUCCESS );
        }
    }

    return IDE_SUCCESS;
}

/*****************************************************************
 * Description :
 *  � BCB�� �Է����� �޾Ƶ� �׻� ID_TRUE�� �����Ѵ�.
 ****************************************************************/
idBool sdsBufferMgr::filterAllBCBs( void   * /*aBCB*/,
                                    void   * /*aObj*/ )
{
    return ID_TRUE;
}

#if 0 //not used
/*****************************************************************
 * Description :
 *  aObj���� spaceID�� ����ִ�. spaceID�� ���� BCB�� ���ؼ�
 *  ID_TRUE�� ����
 *
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �Լ� �����Ҷ� �ʿ��� �ڷᱸ��.
 ****************************************************************/
idBool sdsBufferMgr::filterTBSBCBs( void   *aBCB,
                                    void   *aObj )
{
    sdsBCB    *sBCB     = (sdsBCB*)aBCB;
    scSpaceID *sSpaceID = (scSpaceID*)aObj;

    if( sBCB->mSpaceID == *sSpaceID )
    {
        return ID_TRUE;
    }
    else
    {
        return ID_FALSE;
    }
}
#endif

/*****************************************************************
 * Description :
 *  aObj���� Ư�� pid�� ������ ����ִ�.
 *  BCB�� �� ������ ���Ҷ��� ID_TRUE ����.
 *  aBCB        - [IN]  BCB
 *  aObj        - [IN]  �Լ� �����Ҷ� �ʿ��� �ڷᱸ��. sdsBCBRange�� ĳ�����ؼ�
 ****************************************************************/
idBool sdsBufferMgr::filterRangeBCBs( void   *aBCB,
                                      void   *aObj )
{
    sdsBCB      *sBCB     = (sdsBCB*)aBCB;
    sdsBCBRange *sRange   = (sdsBCBRange*)aObj;

    if( sRange->mSpaceID == sBCB->mSpaceID )
    {
        if( ( sRange->mStartPID <= sBCB->mPageID ) &&
            ( sBCB->mPageID <= sRange->mEndPID ) )
        {
            return ID_TRUE;
        }
    }
    return ID_FALSE;
}

/************************************************************
 * Description :
 *  Discard:
 *  �������� ���۳����� �����Ѵ�.
    iniob�� �ƴ� �̻� free�� ����...
 *
 *  aSBCB   - [IN]  BCB
 *  aObj    - [IN]  �ݵ�� sdsDiscardPageObj�� ����־�� �Ѵ�.
 *                  �� ������ �������� aBCB�� discard���� ���� �����ϴ�
 *                  �Լ� �� �����Ͱ� ����
 ************************************************************/
IDE_RC sdsBufferMgr::discardNoWaitModeFunc( sdsBCB    * aSBCB,
                                            void      * aObj )
{
    sdsDiscardPageObj      * sObj = (sdsDiscardPageObj*)aObj;
    idvSQL                 * sStat = sObj->mStatistics;

    sdsHashChainsLatchHandle  * sHashChainsHandle = NULL;
    scSpaceID      sSpaceID; 
    scPageID       sPageID; 
    UInt           sState = 0;

    sSpaceID = aSBCB->mSpaceID; 
    sPageID  = aSBCB->mPageID; 

    /*1. FilterFunc���� �˻��Ͽ� ��� BCB�� �˻��Ѵ�.
         ��� BCB�� �ƴϸ� SKIP  */
    IDE_TEST_CONT( sObj->mFilter( aSBCB, sObj->mEnv ) 
                    != ID_TRUE,
                    SKIP_SUCCESS );

    sHashChainsHandle = mHashTable.lockHashChainsXLatch( sStat,
                                                         sSpaceID,
                                                         sPageID );
    sState = 1;
    aSBCB->lockBCBMutex( sStat ); 
    sState = 2;

    /* 2. spaceID, pageID�� ������ ����ߴµ� FREE ���¶���� 
          �� ���� getvictim ���������� ������ skip */
    IDE_TEST_CONT( aSBCB->mState == SDS_SBCB_FREE, 
                    SKIP_SUCCESS );

    /* 3. �ش� aSBCB�� �ٸ� ������ ���ߴ��� ���� �˻�.
     * ������ ���� ���Ѵٴ� ���� getvictim ���������� ������..
     * 2�� ���� ���� ���� ���̱� ��.  */
    IDE_TEST_CONT( sObj->mFilter( aSBCB, sObj->mEnv ) 
                    != ID_TRUE,
                    SKIP_SUCCESS );
#ifdef DEBUG
    /* ������� ������ ������ �� �����Ѱ�. Debug������ �ѹ��� �˻� */
    IDE_TEST_RAISE( ( sSpaceID != aSBCB->mSpaceID ) ||
                    ( sPageID  != aSBCB->mPageID ),
                    ERROR_INVALID_BCD );
#endif
    /* 4.make free */
    switch( aSBCB->mState )
    {
        case SDS_SBCB_DIRTY:
            if( aSBCB->mCPListNo != SDB_CP_LIST_NONE )
            {
                mCPListSet.remove( sStat, aSBCB );
            }
            mHashTable.removeBCB( aSBCB );

            aSBCB->setFree();
            break;

        case SDS_SBCB_CLEAN:
            mHashTable.removeBCB( aSBCB );

            aSBCB->setFree();
            break;

        case SDS_SBCB_INIOB:
            /* IOB�� ���¸� �ٲ� ������ Flusher�� ���� */
            aSBCB->mState = SDS_SBCB_OLD;
            mHashTable.removeBCB( aSBCB );

            break;  

        case SDS_SBCB_OLD:
            /* Flusher�� ������� 
             * nothign to do */   
            break;

        case SDS_SBCB_FREE:
        default:
            IDE_RAISE( ERROR_INVALID_BCD );
            break;
    }

    IDE_EXCEPTION_CONT( SKIP_SUCCESS );

    switch ( sState )
    {
        case 2:
            aSBCB->unlockBCBMutex();
        case 1:
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
        break;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                "invalid bcd to discard \n"
                "ID      :%"ID_UINT32_FMT"\n"
                "spaseID :%"ID_UINT32_FMT"\n"
                "pageID  :%"ID_UINT32_FMT"\n"
                "state   :%"ID_UINT32_FMT"\n"
                "CPListNo:%"ID_UINT32_FMT"\n"
                "HashTableNo:%"ID_UINT32_FMT"\n",
                aSBCB->mSBCBID,
                aSBCB->mSpaceID,
                aSBCB->mPageID,
                aSBCB->mState,
                aSBCB->mCPListNo,
                aSBCB->mHashBucketNo );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;
    
    switch ( sState )
    {
        case 2:
            aSBCB->unlockBCBMutex();
        case 1:
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
        break;
    }

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *   Filt������ �����ϴ� ���  BCB����  discard��Ų��.
     for removeFile, shrinkFile ...
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  aFilter���ǿ� �´� BCB�� discard
 *  aFiltAgr    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdsBufferMgr::discardPages( idvSQL        * aStatistics,
                                   sdsFiltFunc     aFilter,
                                   void          * aFiltAgr )
{
    sdsDiscardPageObj sObj;

    sObj.mFilter = aFilter;
    sObj.mStatistics = aStatistics;
    sObj.mEnv = aFiltAgr;

    IDE_ASSERT( mSBufferArea.applyFuncToEachBCBs( discardNoWaitModeFunc,
                                                  &sObj )
                == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/****************************************************************
 * Description :
 *  ���۸Ŵ����� Ư�� pid������ ���ϴ� ��� BCB�� discard�Ѵ�.
 *  �̶�, pid������ ���ϸ鼭 ���ÿ� aSpaceID�� ���ƾ� �Ѵ�.
 *  removeFilePending, shrinkFilePending ��� ȣ��
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdsBufferMgr::discardPagesInRange( idvSQL    *aStatistics,
                                          scSpaceID  aSpaceID,
                                          scPageID   aStartPID,
                                          scPageID   aEndPID)
{
    sdsBCBRange sRange;

    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP )

    sRange.mSpaceID  = aSpaceID;
    sRange.mStartPID = aStartPID;
    sRange.mEndPID   = aEndPID;

    IDE_TEST( sdsBufferMgr::discardPages( aStatistics,
                                          filterRangeBCBs,
                                          (void*)&sRange ) 
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#if 0 //not used
/****************************************************************
 * Description :
 *  ���۸Ŵ����� �ִ� ��� BCB�� discard��Ų��.
 *
 *  aStatistics - [IN]  �������
 ****************************************************************/
IDE_RC sdsBufferMgr::discardAllPages(idvSQL     *aStatistics)
{
    return sdsBufferMgr::discardPages( aStatistics,
                                       filterAllBCBs,
                                       NULL /* parameter for filter */ ); 
}
#endif

/****************************************************************
 * Description :
 *  filter�� �ش��ϴ� �������� flush�Ѵ�. 
 *  discard�� �ٸ��� pageOut�� ��쿡 flush�� �� ���Ŀ� ���Ÿ� �Ѵٴ� ��
 * Implementation:
 *   sdsFlushMgr::flushObjectDirtyPages�Լ��� �̿��� flush.
 *   mQueueForFlush : flush �� BCB list
     mQueueForMakeFree : setfree �� list
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  BCB�� aFilter���ǿ� �´� BCB�� flush
 *  aFiltArg    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdsBufferMgr::pageOut( idvSQL        * aStatistics,
                              sdsFiltFunc     aFilter,
                              void          * aFiltAgr)
{
    sdsPageOutObj    sObj;
    sdsBCB         * sSBCB;
    smuQueueMgr    * sQueue;
    idBool           sEmpty;

    // BUG-26476
    // ��� flusher���� stop �����̸� abort ������ ��ȯ�Ѵ�.
    IDE_TEST_RAISE( sdsFlushMgr::getActiveFlusherCount() == 0,
                    ERROR_ALL_FLUSHERS_STOPPED );

    sObj.mFilter = aFilter;
    sObj.mEnv    = aFiltAgr;

    sObj.mQueueForFlush.initialize( IDU_MEM_SM_SDS, ID_SIZEOF(sdsBCB *) );
    sObj.mQueueForMakeFree.initialize( IDU_MEM_SM_SDS, ID_SIZEOF(sdsBCB *) );

    /* buffer Area�� �����ϴ� ��� BCB�� ���鼭 filt���ǿ� �ش��ϴ� BCB�� ���
     * 1.flush ��� queue 
     * 2.make free ��� queue�� ������.*/
    IDE_ASSERT( mSBufferArea.applyFuncToEachBCBs( makePageOutTargetQueueFunc,
                                                  &sObj )

                == IDE_SUCCESS );

    /* Queue�� flush �Ѵ� */
    IDE_TEST( sdsFlushMgr::flushObjectDirtyPages( aStatistics,
                                                  &(sObj.mQueueForFlush),
                                                  aFilter,
                                                  aFiltAgr )
            != IDE_SUCCESS );
    
    sObj.mQueueForFlush.destroy();

    /* Free ��� Queue�� �޾ƿ´� */
    sQueue = &(sObj.mQueueForMakeFree);

    while(1)
    {
        IDE_ASSERT( sQueue->dequeue( ID_FALSE, // mutex�� ���� �ʴ´�.
                                     (void*)&sSBCB, &sEmpty )
                    == IDE_SUCCESS );

        if( sEmpty == ID_TRUE )
        {
            break;
        }
        /* Free ��� Queue���� ���� BCB�� �˻��ؼ� free ��Ŵ */
        if( aFilter( sSBCB, aFiltAgr ) == ID_TRUE )
        {
            IDE_TEST( makeFreeBCB( aStatistics, sSBCB ) 
                      != IDE_SUCCESS );
        }
    }

    sQueue->destroy();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_ALL_FLUSHERS_STOPPED );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_AllSecondaryFlushersStopped ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  ���۸Ŵ����� �ִ� ��� BCB�鿡 pageOut�� �����Ѵ�.
   -> alter system flush secondary_buffer;
 *  aStatistics - [IN]  �������
 ****************************************************************/
IDE_RC sdsBufferMgr::pageOutAll( idvSQL  *aStatistics )
{
    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP );
    
    IDE_TEST( pageOut( aStatistics,
                       filterAllBCBs,
                       NULL )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#if 0 //not used
/****************************************************************
 * Description :
 *  �ش� spaceID�� �ش��ϴ� ��� BCB�鿡�� pageout�� �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 ****************************************************************/
IDE_RC sdsBufferMgr::pageOutTBS( idvSQL          *aStatistics,
                                 scSpaceID        aSpaceID )
{
    return pageOut( aStatistics,
                    filterTBSBCBs,
                    (void*)&aSpaceID);
}
#endif

/****************************************************************
 * Description :
 *  ���۸Ŵ������� �ش� pid������ �ش��ϴ� BCB�� ��� flush�Ѵ�.
 *  alter tablespace tbs offline;
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdsBufferMgr::pageOutInRange( idvSQL         *aStatistics,
                                     scSpaceID       aSpaceID,
                                     scPageID        aStartPID,
                                     scPageID        aEndPID )
{
    sdsBCBRange sRange;

    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP );
    
    sRange.mSpaceID     = aSpaceID;
    sRange.mStartPID    = aStartPID;
    sRange.mEndPID      = aEndPID;

    IDE_TEST( pageOut( aStatistics,
                       filterRangeBCBs,
                       (void*)&sRange )
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  filt�� �ش��ϴ� �������� flush�Ѵ�.
 * Implementation:
 *  sdsFlushMgr::flushObjectDirtyPages�Լ��� �̿��ϱ� ���ؼ�,
 *  ���� flush�� �ؾ��� BCB���� queue�� ��Ƴ��´�.
 *  �׸��� sdsFlushMgr::flushObjectDirtyPages�Լ��� �̿��� flush.
 *
 *  aStatistics - [IN]  �������
 *  aFilter     - [IN]  BCB�� aFilter���ǿ� �´� BCB�� flush
 *  aFiltArg    - [IN]  aFilter�� �Ķ���ͷ� �־��ִ� ��
 ****************************************************************/
IDE_RC sdsBufferMgr::flushPages( idvSQL            *aStatistics,
                                 sdsFiltFunc        aFilter,
                                 void              *aFiltAgr )
{
    sdsFlushObj sObj;

    // BUG-26476
    // ��� flusher���� stop �����̸� abort ������ ��ȯ�Ѵ�.
    IDE_TEST_RAISE( sdsFlushMgr::getActiveFlusherCount() == 0,
                    ERR_ALL_FLUSHERS_STOPPED );

    sObj.mFilter = aFilter;
    sObj.mEnv    = aFiltAgr;

    sObj.mQueue.initialize( IDU_MEM_SM_SDS, ID_SIZEOF(sdsBCB*) );

    IDE_ASSERT( mSBufferArea.applyFuncToEachBCBs( makeFlushTargetQueueFunc,
                                                  &sObj )
                == IDE_SUCCESS );

    IDE_TEST( sdsFlushMgr::flushObjectDirtyPages( aStatistics,
                                                  &(sObj.mQueue),
                                                  aFilter,
                                                  aFiltAgr )
              != IDE_SUCCESS );

    sObj.mQueue.destroy();

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERR_ALL_FLUSHERS_STOPPED );
    {
        IDE_SET( ideSetErrorCode(smERR_ABORT_AllSecondaryFlushersStopped) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/****************************************************************
 * Description :
 *  ���۸Ŵ������� �ش� pid������ �ش��ϴ� BCB�� ��� flush�Ѵ�.
 *  index bottomUp build
 *  aStatistics - [IN]  �������
 *  aSpaceID    - [IN]  table space ID
 *  aStartPID   - [IN]  pid ������ ����
 *  aEndPID     - [IN]  pid ������ ��
 ****************************************************************/
IDE_RC sdsBufferMgr::flushPagesInRange( idvSQL        * aStatistics,
                                        scSpaceID       aSpaceID,
                                        scPageID        aStartPID,
                                        scPageID        aEndPID )
{
    sdsBCBRange sRange;

    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP );

    sRange.mSpaceID     = aSpaceID;
    sRange.mStartPID    = aStartPID;
    sRange.mEndPID      = aEndPID;

    IDE_TEST( flushPages( aStatistics,
                          filterRangeBCBs,
                          (void*)&sRange ) 
              != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description :
 *  alter system checkpoint;
 *  aStatistics - [IN]  �������
 *  aFlushAll   - [IN]  ��� �������� flush �ؾ��ϴ��� ����
 ******************************************************************************/
IDE_RC sdsBufferMgr::flushDirtyPagesInCPList( idvSQL *aStatistics, 
                                              idBool aFlushAll)
{
    IDE_TEST_CONT( isServiceable() != ID_TRUE, SKIP );

    IDE_TEST( sdsFlushMgr::flushDirtyPagesInCPList( aStatistics,
                                                    aFlushAll )
             != IDE_SUCCESS);

    IDE_EXCEPTION_CONT( SKIP );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/******************************************************************************
 * Description : Secondary Buffer���� mFrame���� �о�´�( MPR )
 *  aStatistics - [IN]  �������
 ******************************************************************************/
IDE_RC sdsBufferMgr::moveUpbySinglePage( idvSQL     * aStatistics,
                                         sdsBCB    ** aSBCB,
                                         sdbBCB     * aBCB,
                                         idBool     * aIsCorruptRead )
{
    scSpaceID   sSpaceID = aBCB->mSpaceID;
    scPageID    sPageID  = aBCB->mPageID;
    idvTime     sBeginTime;
    idvTime     sEndTime;
    ULong       sReadTime         = 0;
    ULong       sCalcChecksumTime = 0;
    SInt        sState            = 0;
    idBool      sIsLock        = ID_FALSE;
    idBool      sIsCorruptRead = ID_FALSE;
    sdsBCB    * sExistSBCB     = *aSBCB;

    sExistSBCB->lockReadIOMutex( aStatistics );
    sIsLock = ID_TRUE;

    if ( ( sExistSBCB->mSpaceID != sSpaceID) ||
         ( sExistSBCB->mPageID != sPageID) )
    { 
        sIsLock = ID_FALSE;
        sExistSBCB->unlockReadIOMutex();

        IDE_TEST( findBCB( aStatistics, sSpaceID, sPageID, &sExistSBCB )
                  != IDE_SUCCESS );

        if ( sExistSBCB == NULL )
        {
            sIsCorruptRead = ID_TRUE;        
            IDE_CONT( SKIP_CORRUPT_READ );
        }
        else 
        {
            *aSBCB = sExistSBCB;

            sExistSBCB->lockReadIOMutex( aStatistics );
            sIsLock = ID_TRUE;
        }
    }

    
    IDE_DASSERT( sExistSBCB->mState != SDS_SBCB_OLD );

    IDV_TIME_GET(&sBeginTime);

    mStatistics.applyBeforeSingleReadPage( aStatistics );
    sState = 1;

    IDE_TEST_RAISE( mSBufferFile.open( aStatistics ) != IDE_SUCCESS,
                    ERROR_PAGE_MOVEUP );

    IDE_TEST_RAISE( mSBufferFile.read( aStatistics,
                                 sExistSBCB->mSBCBID, 
                                 mMeta.getFrameOffset( sExistSBCB->mSBCBID ),
                                 1,
                                 aBCB->mFrame )
                    != IDE_SUCCESS,
                    ERROR_PAGE_MOVEUP );     

    sState = 0;
    mStatistics.applyAfterSingleReadPage( aStatistics );

    IDV_TIME_GET(&sEndTime);
    sReadTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);
    sBeginTime = sEndTime;

    if( sdpPhyPage::checkAndSetPageCorrupted( sExistSBCB->mSpaceID,
                                              aBCB->mFrame ) 
        == ID_TRUE )
    {
        IDE_RAISE( ERROR_PAGE_CORRUPTION );
    }

#ifdef DEBUG
    IDE_TEST_RAISE(
        sdpPhyPage::isConsistentPage( (UChar*) sdpPhyPage::getHdr( aBCB->mFrame ) )
        == ID_FALSE ,
        ERROR_PAGE_INCONSISTENT  );
#endif
    IDE_DASSERT( validate( aBCB ) == IDE_SUCCESS );

    setFrameInfoAfterReadPage( sExistSBCB,
                               aBCB,
                               ID_FALSE ); // check to online tablespace

    IDV_TIME_GET(&sEndTime);
    sCalcChecksumTime = IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

    mStatistics.applyMultiReadPages( sCalcChecksumTime,
                                     sReadTime,
                                     1 ); /*Page Count */

    sIsLock = ID_FALSE;
    sExistSBCB->unlockReadIOMutex();

    IDE_EXCEPTION_CONT( SKIP_CORRUPT_READ );
    
    if( aIsCorruptRead != NULL )
    {
        *aIsCorruptRead = sIsCorruptRead;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( ERROR_PAGE_MOVEUP );
    {
        setUnserviceable();
        IDE_SET( ideSetErrorCode( smERR_ABORT_PageMoveUpStopped ) );
    }
    IDE_EXCEPTION( ERROR_PAGE_CORRUPTION );
    {
        switch ( smLayerCallback::getCorruptPageReadPolicy() )
        {
            case SDB_CORRUPTED_PAGE_READ_FATAL :
            IDE_SET( ideSetErrorCode( smERR_FATAL_PageCorrupted,
                                      aBCB->mSpaceID,
                                      aBCB->mPageID ));

                break;
            case SDB_CORRUPTED_PAGE_READ_ABORT :
                IDE_SET( ideSetErrorCode( smERR_ABORT_PageCorrupted,
                                          aBCB->mSpaceID,
                                          aBCB->mPageID));
                break;
            default:
                break;
        }
    }
#ifdef DEBUG
    IDE_EXCEPTION( ERROR_PAGE_INCONSISTENT );
    {
        sdpPhyPageHdr   * sPhyPageHdr;
        sPhyPageHdr = sdpPhyPage::getHdr(aBCB->mFrame);
        ideLog::log( IDE_SM_0,
                 "moveUpbySinglePage : Page Dump -----------------\n"
                 "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                 "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                 "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                 "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                 "Total Free Size           : %"ID_UINT32_FMT"\n"
                 "Available Free Size       : %"ID_UINT32_FMT"\n"
                 "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                 "Size of CTL               : %"ID_UINT32_FMT"\n"
                 "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                 "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                 "Page Type                 : %"ID_UINT32_FMT"\n"
                 "Page State                : %"ID_UINT32_FMT"\n"
                 "Page ID                   : %"ID_UINT32_FMT"\n"
                 "is Consistent             : %"ID_UINT32_FMT"\n"
                 "LinkState                 : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                 "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                 "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                 "TableOID                  : %"ID_vULONG_FMT"\n"
                 "Seq No                    : %"ID_UINT64_FMT"\n"
                 "---------------------------------------------\n",
                 (UChar*)sPhyPageHdr,
                 sPhyPageHdr->mFrameHdr.mCheckSum,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                 sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                 sPhyPageHdr->mFrameHdr.mIndexSMONo,
                 sPhyPageHdr->mFrameHdr.mSpaceID,
                 sPhyPageHdr->mFrameHdr.mBCBPtr,
                 sPhyPageHdr->mTotalFreeSize,
                 sPhyPageHdr->mAvailableFreeSize,
                 sPhyPageHdr->mLogicalHdrSize,
                 sPhyPageHdr->mSizeOfCTL,
                 sPhyPageHdr->mFreeSpaceBeginOffset,
                 sPhyPageHdr->mFreeSpaceEndOffset,
                 sPhyPageHdr->mPageType,
                 sPhyPageHdr->mPageState,
                 sPhyPageHdr->mPageID,
                 sPhyPageHdr->mIsConsistent,
                 sPhyPageHdr->mLinkState,
                 sPhyPageHdr->mParentInfo.mParentPID,
                 sPhyPageHdr->mParentInfo.mIdxInParent,
                 sPhyPageHdr->mListNode.mPrev,
                 sPhyPageHdr->mListNode.mNext,
                 sPhyPageHdr->mTableOID,
                 sPhyPageHdr->mSeqNo );

        IDE_SET( ideSetErrorCode( smERR_ABORT_INCONSISTENT_PAGE,
                                  aBCB->mSpaceID,
                                  aBCB->mPageID) );
    }
#endif
    IDE_EXCEPTION_END;

    if( sIsLock == ID_TRUE )
    {
        sIsLock = ID_FALSE;
        sExistSBCB->unlockReadIOMutex();
    }

    if( sState != 0 )
    {
        mStatistics.applyAfterSingleReadPage( aStatistics );
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Descrition:
 *  ��ũ���� �������� �о�� ��, BCB�� �о�� frame�� �������� ������ �����Ѵ�
 *
 *  aBCB            - [IN]  BCB
 *  aChkOnlineTBS   - [IN] TBS Online ���θ� Ȯ���ؼ� SMO NO ���� (Yes/No)
 **********************************************************************/
void sdsBufferMgr::setFrameInfoAfterReadPage( sdsBCB * aSBCB,
                                              sdbBCB * aBCB,
                                              idBool   aChkOnlineTBS )
{
    aBCB->mPageType = smLayerCallback::getPhyPageType( aBCB->mFrame );
    IDV_TIME_GET(&aBCB->mCreateOrReadTime);

    /*  �������� ���� �� �ݵ�� ��������� �ϴ� ������ */
    sdbBCB::setBCBPtrOnFrame( (sdbFrameHdr*)aBCB->mFrame, aBCB );
    sdbBCB::setSpaceIDOnFrame( (sdbFrameHdr*)aBCB->mFrame, 
                                aBCB->mSpaceID );
    /* Secondaty Buffer�� SBCB�� ����Ű�� BCB�� ���� ���� BCB�� ���� */ 
    aSBCB->mBCB = aBCB;

    // BUG-47429 SMO NO�� 0�� �ƴ� ��쿡�� SMO NO �ʱ�ȭ �ʿ� ���θ� Ȯ���Ѵ�.
    if ( smLayerCallback::getIndexSMONo( aBCB->mFrame ) != 0 )
    {
        // SMO no�� �ʱ�ȭ�ؾ� �Ѵ�.
        smLayerCallback::resetIndexSMONo( aBCB->mFrame,
                                          aBCB->mSpaceID,
                                          aChkOnlineTBS );
    }
}

/****************************************************************
 * Description :
 *  ���� ���ۿ� �����ϴ� BCB���� recoveryLSN�� ���� ���� ����
 *  �����Ѵ�.
 *
 *  aStatistics - [IN]  �������
 *  aRet        - [OUT] ��û�� min recoveryLSN
 ****************************************************************/
void sdsBufferMgr::getMinRecoveryLSN( idvSQL * aStatistics,
                                      smLSN  * aMinLSN )
{
    smLSN         sFlusherMinLSN;
    smLSN         sCPListMinLSN;

    if( isServiceable() != ID_TRUE )
    {
        SM_LSN_MAX( *aMinLSN );
    }
    else 
    {
        /* CPlist�� �Ŵ޸� BCB �� ��������  recoveryLSN�� ��´�. */
        mCPListSet.getMinRecoveryLSN( aStatistics, &sCPListMinLSN );
        /* IOB�� ����� BCB�� �� ���� ���� recoveryLSN�� ��´�. */
        sdsFlushMgr::getMinRecoveryLSN(aStatistics, &sFlusherMinLSN );

        if ( smLayerCallback::isLSNLT( &sCPListMinLSN,
                                       &sFlusherMinLSN )
             == ID_TRUE)
        {
            SM_GET_LSN( *aMinLSN, sCPListMinLSN );
        }
        else
        {
            SM_GET_LSN( *aMinLSN, sFlusherMinLSN );
        }
    }
}

/****************************************************************
 * Description : Secondary Buffer�� nodeȮ�� �� ����
    �ܰ� : off -> identifiable -> serviceable
 ************** **************************************************/
IDE_RC sdsBufferMgr::identify( idvSQL * aStatistics )
{
    if( isIdentifiable() == ID_TRUE )
    {
        IDE_TEST( mSBufferFile.identify( aStatistics ) != IDE_SUCCESS );

        setServiceable();
       
        smLayerCallback::setSBufferServiceable();    
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
 * Description:BCB �� ������ 
 *  ����� ������ OLD�� ���� ���� 
 *  aSBCB        [IN] : ���� BCB
 ****************************************************************/
IDE_RC sdsBufferMgr::removeBCB( idvSQL     * aStatistics,
                                sdsBCB     * aSBCB )
{
    sdsHashChainsLatchHandle  * sHashChainsHandle = NULL;
    scSpaceID     sSpaceID; 
    scPageID      sPageID; 
    UInt          sState = 0;

    IDE_ASSERT( aSBCB != NULL );

    sSpaceID = aSBCB->mSpaceID; 
    sPageID  = aSBCB->mPageID; 

    sHashChainsHandle = mHashTable.lockHashChainsXLatch( aStatistics,
                                                         sSpaceID,
                                                         sPageID );
    sState = 1;
    aSBCB->lockBCBMutex( aStatistics );
    sState = 2;

    /* �Ѱܹ��� BCB�� lock ���� BCB�� ���ƾ� �Ѵ�. */
    IDE_TEST_CONT( (( sSpaceID != aSBCB->mSpaceID ) ||
                     ( sPageID  != aSBCB->mPageID ) ) ,
                    SKIP_REMOVE_BCB );

    switch( aSBCB->mState )
    {
        case SDS_SBCB_DIRTY:
            if( aSBCB->mCPListNo != SDB_CP_LIST_NONE )
            {
                mCPListSet.remove( aStatistics, aSBCB );
            }
            mHashTable.removeBCB( aSBCB );
            aSBCB->setFree();
            break;

        case SDS_SBCB_CLEAN:
            mHashTable.removeBCB( aSBCB );
            aSBCB->setFree();
            break;

        case SDS_SBCB_INIOB:
            aSBCB->mState = SDS_SBCB_OLD;
            mHashTable.removeBCB( aSBCB );
            break;  

        case SDS_SBCB_OLD:
        case SDS_SBCB_FREE:
            /* nothing to do */
            break;

        default:
            IDE_RAISE( ERROR_INVALID_BCD );
            break;
    }

    IDE_EXCEPTION_CONT( SKIP_REMOVE_BCB );

    sState = 1;
    aSBCB->unlockBCBMutex();
    sState = 0;
    mHashTable.unlockHashChainsLatch( sHashChainsHandle );
    sHashChainsHandle = NULL;

    return IDE_SUCCESS;
    
    IDE_EXCEPTION( ERROR_INVALID_BCD );
    {
        ideLog::log( IDE_ERR_0,
                     "Invalid bcd to remove \n"
                     "ID :%u\n"
                     "spaseID :%u\n"
                     "pageID  :%u\n"
                     "state   :%u\n"
                     "CPListNo:%u\n"
                     "HashTableNo:%u\n",
                     aSBCB->mSBCBID,
                     aSBCB->mSpaceID,
                     aSBCB->mPageID,
                     aSBCB->mState,
                     aSBCB->mCPListNo,
                     aSBCB->mHashBucketNo );

        IDE_DASSERT( 0 );
    }
    IDE_EXCEPTION_END;
    
    switch( sState )
    { 
        case 2:
            aSBCB->unlockBCBMutex(); 
        case 1:
            mHashTable.unlockHashChainsLatch( sHashChainsHandle );
            sHashChainsHandle = NULL;
            break;
    }

    return IDE_FAILURE;
}

#ifdef DEBUG
/***********************************************************************
 * Description : ������
 **********************************************************************/
IDE_RC sdsBufferMgr::validate( sdbBCB  * aBCB )
{
    IDE_DASSERT( aBCB != NULL );

    sdpPhyPageHdr   * sPhyPageHdr;
    smcTableHeader  * sTableHeader;
    smOID             sTableOID;

    sPhyPageHdr = sdpPhyPage::getHdr(aBCB->mFrame);

    if( sdpPhyPage::getPageType( sPhyPageHdr ) == SDP_PAGE_DATA )
    {
        if( sPhyPageHdr->mPageID != aBCB->mPageID )
        {
            ideLog::log( IDE_ERR_0,
                     "----------------- Page Dump -----------------\n"
                     "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                     "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                     "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                     "Total Free Size           : %"ID_UINT32_FMT"\n"
                     "Available Free Size       : %"ID_UINT32_FMT"\n"
                     "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                     "Size of CTL               : %"ID_UINT32_FMT"\n"
                     "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                     "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                     "Page Type                 : %"ID_UINT32_FMT"\n"
                     "Page State                : %"ID_UINT32_FMT"\n"
                     "Page ID                   : %"ID_UINT32_FMT"\n"
                     "is Consistent             : %"ID_UINT32_FMT"\n"
                     "LinkState                 : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                     "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "TableOID                  : %"ID_vULONG_FMT"\n"
                     "Seq No                    : %"ID_UINT64_FMT"\n"
                     "---------------------------------------------\n",
                     (UChar*)sPhyPageHdr,
                     sPhyPageHdr->mFrameHdr.mCheckSum,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                     sPhyPageHdr->mFrameHdr.mIndexSMONo,
                     sPhyPageHdr->mFrameHdr.mSpaceID,
                     sPhyPageHdr->mFrameHdr.mBCBPtr,
                     sPhyPageHdr->mTotalFreeSize,
                     sPhyPageHdr->mAvailableFreeSize,
                     sPhyPageHdr->mLogicalHdrSize,
                     sPhyPageHdr->mSizeOfCTL,
                     sPhyPageHdr->mFreeSpaceBeginOffset,
                     sPhyPageHdr->mFreeSpaceEndOffset,
                     sPhyPageHdr->mPageType,
                     sPhyPageHdr->mPageState,
                     sPhyPageHdr->mPageID,
                     sPhyPageHdr->mIsConsistent,
                     sPhyPageHdr->mLinkState,
                     sPhyPageHdr->mParentInfo.mParentPID,
                     sPhyPageHdr->mParentInfo.mIdxInParent,
                     sPhyPageHdr->mListNode.mPrev,
                     sPhyPageHdr->mListNode.mNext,
                     sPhyPageHdr->mTableOID,
                     sPhyPageHdr->mSeqNo );
            IDE_DASSERT( 0 );
        }

        sTableOID = sdpPhyPage::getTableOID( (UChar*)sdpPhyPage::getHdr( aBCB->mFrame ));

        (void)smcTable::getTableHeaderFromOID( sTableOID,
                                               (void**)&sTableHeader );

        if( sTableOID != sTableHeader->mSelfOID )
        {
            ideLog::log( IDE_ERR_0,
                     "----------------- Page Dump -----------------\n"
                     "Page Start Pointer        : %"ID_xPOINTER_FMT"\n"
                     "FrameHdr.mCheckSum        : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mFileNo : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mPageLSN.mOffset : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mIndexSMONo      : %"ID_vULONG_FMT"\n"
                     "FrameHdr.mSpaceID         : %"ID_UINT32_FMT"\n"
                     "FrameHdr.mBCBPtr          : 0x%"ID_xPOINTER_FMT"\n"
                     "Total Free Size           : %"ID_UINT32_FMT"\n"
                     "Available Free Size       : %"ID_UINT32_FMT"\n"
                     "Logical Hdr Size          : %"ID_UINT32_FMT"\n"
                     "Size of CTL               : %"ID_UINT32_FMT"\n"
                     "Free Space Begin Offset   : %"ID_UINT32_FMT"\n"
                     "Free Space End Offset     : %"ID_UINT32_FMT"\n"
                     "Page Type                 : %"ID_UINT32_FMT"\n"
                     "Page State                : %"ID_UINT32_FMT"\n"
                     "Page ID                   : %"ID_UINT32_FMT"\n"
                     "is Consistent             : %"ID_UINT32_FMT"\n"
                     "LinkState                 : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mParentPID     : %"ID_UINT32_FMT"\n"
                     "ParentInfo.mIdxInParent   : %"ID_UINT32_FMT"\n"
                     "Prev Page ID, Nex Page ID : %"ID_UINT32_FMT", %"ID_UINT32_FMT"\n"
                     "TableOID                  : %"ID_vULONG_FMT"\n"
                     "Seq No                    : %"ID_UINT64_FMT"\n"
                     "---------------------------------------------\n",

                     (UChar*)sPhyPageHdr,
                     sPhyPageHdr->mFrameHdr.mCheckSum,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mFileNo,
                     sPhyPageHdr->mFrameHdr.mPageLSN.mOffset,
                     sPhyPageHdr->mFrameHdr.mIndexSMONo,
                     sPhyPageHdr->mFrameHdr.mSpaceID,
                     sPhyPageHdr->mFrameHdr.mBCBPtr,
                     sPhyPageHdr->mTotalFreeSize,
                     sPhyPageHdr->mAvailableFreeSize,
                     sPhyPageHdr->mLogicalHdrSize,
                     sPhyPageHdr->mSizeOfCTL,
                     sPhyPageHdr->mFreeSpaceBeginOffset,
                     sPhyPageHdr->mFreeSpaceEndOffset,
                     sPhyPageHdr->mPageType,
                     sPhyPageHdr->mPageState,
                     sPhyPageHdr->mPageID,
                     sPhyPageHdr->mIsConsistent,
                     sPhyPageHdr->mLinkState,
                     sPhyPageHdr->mParentInfo.mParentPID,
                     sPhyPageHdr->mParentInfo.mIdxInParent,
                     sPhyPageHdr->mListNode.mPrev,
                     sPhyPageHdr->mListNode.mNext,
                     sPhyPageHdr->mTableOID,
                     sPhyPageHdr->mSeqNo );
            IDE_DASSERT( 0 );
        }
    }
    return IDE_SUCCESS;
}
#endif
