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
 * $Id: smrDirtyPageList.cpp 90522 2021-04-09 01:29:20Z emlee $
 **********************************************************************/

#include <smErrorCode.h>
#include <smm.h>
#include <sct.h>
#include <smr.h>
#include <smrReq.h>

#define SMR_INIT_DIRTYPAGE_CNT (1000)

extern "C" SInt
smrComparePageGRID( const void* aElem1, const void* aElem2 )
{

    scSpaceID sSpaceID1 = SC_MAKE_SPACE(*(scGRID*)aElem1);
    scSpaceID sSpaceID2 = SC_MAKE_SPACE(*(scGRID*)aElem2);
    scPageID  sPageID1 = SC_MAKE_PID(*(scGRID*)aElem1);
    scPageID  sPageID2 = SC_MAKE_PID(*(scGRID*)aElem2);

    if ( sSpaceID1 > sSpaceID2 )
    {
        return 1;
    }
    else
    {
        if ( sSpaceID1 < sSpaceID2 )
        {
            return -1;
        }
    }
    
    if ( sPageID1 > sPageID2 )
    {
        return 1;
    }
    else
    {
        if ( sPageID1 < sPageID2 )
        {
            return -1;
        }
    }

    return 0;

}


smrDirtyPageList::smrDirtyPageList()
{

}

smrDirtyPageList::~smrDirtyPageList()
{

}

/*  Dirty Page�����ڸ� �ʱ�ȭ�Ѵ�.
  
    [IN] aSpaceID - �� Dirty Page�����ڰ� ������ Page���� ����
                    Tablespace�� ID
 */
IDE_RC smrDirtyPageList::initialize( scSpaceID aSpaceID )
{
    mSpaceID                   = aSpaceID;
    
    mDirtyPageCnt              = 0;
    mFstPCH.m_pnxtDirtyPCH     = &mFstPCH;
    mFstPCH.m_pprvDirtyPCH     = &mFstPCH;

    mMaxDirtyPageCnt           = SMR_INIT_DIRTYPAGE_CNT;
    mArrPageGRID               = NULL;

    /* TC/FIT/Limit/sm/smr/smrDirtyPageList_initialize_malloc.sql */
    IDU_FIT_POINT_RAISE( "smrDirtyPageList::initialize::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SMR,
                                ID_SIZEOF(scGRID) * mMaxDirtyPageCnt,
                                (void**)&mArrPageGRID) != IDE_SUCCESS,
                    insufficient_memory );
    
    /*
     * BUG-34125 Posix mutex must be used for cond_timedwait(), cond_wait().
     */
    IDE_TEST( mMutex.initialize((SChar*)"DIRTY_PAGE_LIST_MUTEX",
                               IDU_MUTEX_KIND_POSIX,
                               IDV_WAIT_INDEX_NULL )
             != IDE_SUCCESS );
    
    IDE_TEST_RAISE(mCV.initialize((SChar *)"DIRTY_PAGE_LIST_COND")
                   != IDE_SUCCESS, err_cond_var_init);
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_var_init );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondInit));
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

IDE_RC smrDirtyPageList::destroy()
{

    if(mArrPageGRID != NULL)
    {
        IDE_TEST( iduMemMgr::free((void*)mArrPageGRID)
                  != IDE_SUCCESS );
        mArrPageGRID = NULL;
    }

    IDE_TEST_RAISE(mCV.destroy() != IDE_SUCCESS, err_cond_var_destroy);
    
    IDE_TEST( mMutex.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_cond_var_destroy);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondDestroy));
    }

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * �ߺ��� PID�� ������ üũ�Ѵ�.
 *
 * return [OUT] �ߺ��� PID�� ������ ID_TRUE
 */
idBool smrDirtyPageList::isAllPageUnique()
{
    UInt  i, j;

    idBool sIsUnique = ID_TRUE;
    
    for( i = 0; i < mDirtyPageCnt; i++ )
    {
        for ( j = 0; j < mDirtyPageCnt; j++ )
        {
            if ( i < j )
            {
                if( SC_GRID_IS_EQUAL(mArrPageGRID[i], mArrPageGRID[j]) )
                {
                    sIsUnique = ID_FALSE;
                    IDE_RAISE(finish);
                }
            }
        }
    }

    /* BUG-40385 sResult ���� ���� Failure ������ �� �����Ƿ�,
     * ���� IDE_TEST_RAISE -> IDE_TEST_CONT �� ��ȯ���� �ʴ´�. */
    IDE_EXCEPTION_CONT(finish);
    
    return sIsUnique ;
}


/*
    Dirty Page ID���� �����ϰ� Page ID�� �����ͺ��� �α��Ѵ�.

    [ �α��ϴ� ���� ]
      Dirty Page Flush�� Unstable DB�� �����ϰ� �Ǵµ�,
      Dirty Page Flush���� ������ ����ϰ� �Ǹ�,
      Unstable DB�� �Ϻ� Page�� �߸��� Page �̹����� ������ �ȴ�.

      �̸� �����ϱ� ���� Restart Redo�� Dirty Page ID�� Log�κ��� �о
      Stable DB�κ��� Unstable DB�� �ش� Page���� �����ϰ� �ȴ�.

    [���ü� ����]
       TBSNode.SyncMutex�� ��´�.
       - Tablespace�� Drop/Offline��Ű���� Tx���� ���ü� ��� ����
 */
IDE_RC smrDirtyPageList::writePIDLogs()
{
    scGRID*              sArrPageGRID;
    ULong                sBuffer[SM_PAGE_SIZE / ID_SIZEOF(ULong)];
    scSpaceID            sSpaceID;
    scPageID             sPageID;
    smmPCH*              sCurPCHPtr = NULL;
    UInt                 sDirtyStat;
    vULong               sDPageCnt = 0;
    vULong               sTotalCnt;

    // ����� scGRID�� ������ ������ ũ�Ⱑ �۾Ƽ��� �ȵȴ�.
    IDE_DASSERT( SM_PAGE_SIZE >=
                 ((MAX_PAGE_INFO * ID_SIZEOF(scGRID)) + SMR_DEF_LOG_SIZE) );

    sArrPageGRID  = (scGRID*)( ((smrLogHead*)sBuffer) + 1 );

    if(mDirtyPageCnt != 0)
    {
        sTotalCnt = 0;

        /*
          ����Ÿ���̽� �������� Physical Disk�� ������ ID������ ���������� Wirte�ǵ���
          ������ ID�� Sorting.. �̷��� �����ν� Restart Reload�� ���������� Loading�ȴ�.
        */

        idlOS::qsort((void*)mArrPageGRID,
                     mDirtyPageCnt,
                     ID_SIZEOF(scGRID),
                     smrComparePageGRID);

        while(sTotalCnt != mDirtyPageCnt)
        {
            sSpaceID = SC_MAKE_SPACE(mArrPageGRID[sTotalCnt]);
            sPageID = SC_MAKE_PID(mArrPageGRID[sTotalCnt]);

            sCurPCHPtr = smmManager::getPCH( sSpaceID, sPageID );

            sDirtyStat = sCurPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK;
            IDE_ASSERT(sDirtyStat != SMM_PCH_DIRTY_STAT_INIT);

            if (sDirtyStat != SMM_PCH_DIRTY_STAT_INIT)
            {
                SC_COPY_GRID(mArrPageGRID[sTotalCnt], sArrPageGRID[sDPageCnt]);
                sDPageCnt++;

                if(sDPageCnt >= MAX_PAGE_INFO)
                {
                    IDE_TEST( writePIDLogRec( (SChar*) sBuffer, sDPageCnt )
                              != IDE_SUCCESS );

                    sDPageCnt = 0;
                }
            }

            sTotalCnt++;
        }

        if(sDPageCnt != 0)
        {

            IDE_TEST( writePIDLogRec( (SChar*) sBuffer, sDPageCnt )
                      != IDE_SUCCESS );

            sDPageCnt = 0;
        }
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Page ID Array�� ��ϵ� Log Buffer�� Log Record�� ����Ѵ�.

    [IN] aLogBuffer - �α׹���
                        Head, Tail : �ʱ�ȭ �ȵȻ���
                        Body : Dirty Page GRID�� ��ϵǾ� ����

    [IN] aDirtyPageCount - �α� ������ Body�� ��ϵ� Page�� ���� 
 */
IDE_RC smrDirtyPageList::writePIDLogRec(SChar * aLogBuffer,
                                        UInt    aDirtyPageCount)
{
    smrLogHead * sLogHeadPtr = (smrLogHead*)aLogBuffer;

    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aDirtyPageCount != 0 );

    smrLogHeadI::setType(sLogHeadPtr, SMR_LT_DIRTY_PAGE);
    smrLogHeadI::setTransID(sLogHeadPtr, SM_NULL_TID);
    smrLogHeadI::setSize(sLogHeadPtr,
                         ID_SIZEOF(smrLogHead) +
                         ID_SIZEOF(scGRID) * aDirtyPageCount +
                         ID_SIZEOF(smrLogTail));

    smrLogHeadI::setPrevLSN(sLogHeadPtr,
                            ID_UINT_MAX,
                            ID_UINT_MAX);

    smrLogHeadI::setFlag(sLogHeadPtr, SMR_LOG_TYPE_NORMAL);

    smrLogHeadI::copyTail( aLogBuffer + smrLogHeadI::getSize(sLogHeadPtr)
                                      - ID_SIZEOF(smrLogTail),
                           sLogHeadPtr);

    IDE_TEST( smrLogMgr::writeLog( NULL, /* idvSQL* */
                                   NULL,
                                   (SChar*)aLogBuffer,
                                   NULL,  // Previous LSN Ptr
                                   NULL,  // Log LSN Ptr
                                   NULL,  // End LSN Ptr
                                   SM_NULL_OID )
             != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
     Dirty Page���� Checkpoint Image�� Write�Ѵ�.

     [ Latch�� Deadlockȸ�Ǹ� ���� Latch��� ���� ]
       1. sctTableSpaceMgr::lock()           // TBS LIST
       2. sctTableSpaceMgr::latchSyncMutex() // TBS NODE
       3. smmPCH.mMutex.lock()               // PAGE

     [���ü� ����]
        TBSNode.SyncMutex�� ����ä�� �� �Լ��� ȣ���ؾ� �Ѵ�.
        - Tablespace�� Drop/Offline��Ű���� Tx���� ���ü� ��� ����
 */
IDE_RC smrDirtyPageList::writeDirtyPages(
                         smmTBSNode                * aTBSNode,
                         smmGetFlushTargetDBNoFunc   aGetFlushTargetDBNoFunc,
                         idBool                      aIsFinalWrite,
                         UInt                        aTotalCnt,
                         UInt                      * aWriteCnt,
                         UInt                      * aRemoveCnt,
                         ULong                     * aWaitTime,
                         ULong                     * aSyncTime )
{
    SInt                 sWhichDB;
    scSpaceID            sSpaceID;
    scPageID             sPageID;
    smmPCH*              sCurPCHPtr = NULL;
    smPCSlot           * sPCHSlot = NULL;
    UInt                 sDirtyPageCnt;
    UInt                 sWriteCnt  = 0;
    UInt                 sRemoveCnt = 0;
    ULong                sWaitTime  = 0;  /* microsecond */
    ULong                sSyncTime  = 0;  /* microsecond */
    UInt                 sDirtyStat;
    UInt                 sStage     = 0;
    PDL_Time_Value       sTV;
    PDL_Time_Value       sAddTV;
    IDE_RC               sRC;
    UInt                 sWritePageCnt = 0;
    UInt                 sSleepSec;
    UInt                 sSleepUSec;
    UInt                 sDPMutexState;
    UInt                 sPropSyncPageCount;
    idvTime              sBeginTime;
    /* LSN�� �������� Sync�� Log�� ũ�⸦ �� �� �ִ�. */
    idvTime              sEndTime;

    
    IDE_DASSERT( aTBSNode   != NULL );
    IDE_DASSERT( aWriteCnt  != NULL );
    IDE_DASSERT( aRemoveCnt != NULL );
    IDE_DASSERT( aWaitTime  != NULL );
    IDE_DASSERT( aSyncTime  != NULL );
    IDE_DASSERT( aGetFlushTargetDBNoFunc != NULL );
    
    sDPMutexState     = 0;
    sSleepSec  = 0;
    sSleepUSec = 0;
    sPropSyncPageCount = smuProperty::getChkptBulkSyncPageCount();

    sDirtyPageCnt = mDirtyPageCnt;

    // ���� Flush�� Ping Pong DB��� 
    // 1. ���� ��߿��� 
    // smmManager::getNxtStableDB
    // 2. �̵�� �����ÿ��� 
    // smmManager::getCurrentDB
    sWhichDB = aGetFlushTargetDBNoFunc( aTBSNode );
    
    while(sWriteCnt != sDirtyPageCnt)
    {
        sSpaceID = SC_MAKE_SPACE(mArrPageGRID[sWriteCnt]);
        IDE_ASSERT( sSpaceID == mSpaceID );
        
        sPageID = SC_MAKE_PID(mArrPageGRID[sWriteCnt]);

        sPCHSlot = smmManager::getPCHSlot(sSpaceID, sPageID);
        sCurPCHPtr = (smmPCH*)sPCHSlot->mPCH;

        sDirtyStat = sCurPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK;
        IDE_ASSERT( sDirtyStat != SMM_PCH_DIRTY_STAT_INIT );
            
        // Free Page�� ������ �޸𸮸�  �ݳ��Ϸ��� Thread��
        // Dirty Page Flush�Ϸ��� Thread���� ���ü� ����
        
        IDE_TEST( sCurPCHPtr->mMutex.lock( NULL /* idvSQL* */ )
                  != IDE_SUCCESS );
        sStage = 1;
        
        {
            
            /* --------------------------------------------
             * 1. dirty flag turn off
             * -------------------------------------------- */
            sCurPCHPtr->m_dirty = ID_FALSE;
            
            // m_dirty �÷��װ� ID_TRUE�̸� smmDirtyPageMgr�� insDirtyPage��
            // ȣ���Ͽ��� Dirty Page�� �߰����� �ʴ´�.
            // ���� m_dirty �÷��װ� ID_TRUE�� ä�� Page�� Flush�Ǹ�,
            // Page�� Flush�Ǵ� ���߿� Dirty Page�߰� �õ��� ����
            // Dirty Page�� �߰����� ���� �ʰ� �ȴ�.
            // �׷��Ƿ� �Ʒ� �۾�(Page�� ��ũ�� Flush )�� ����Ǳ� ����
            // �ݵ�� m_dirty �� ID_FALSE�� ���õǾ�� �Ѵ�.
            IDL_MEM_BARRIER;
            
            // ������ �޸𸮰� �����ִ� ��쿡�� Flush �ǽ�.
            if ( sPCHSlot->mPagePtr != NULL )
            {
                IDE_TEST( writePageImage( aTBSNode,
                                          sWhichDB,
                                          sPageID ) != IDE_SUCCESS );
                
            }

            /* FIT/ART/sm/Bugs/BUG-13894/BUG-13894.sql */
            IDU_FIT_POINT( "1.BUG-13894@smrDirtyPageList::writeDirtyPages" );
            
            // To Fix BUG-9366
            sWritePageCnt++;
            
            if ((smuProperty::getChkptBulkWritePageCount() == sWritePageCnt) &&
                (aIsFinalWrite == ID_FALSE))
            {
                sSleepSec  = smuProperty::getChkptBulkWriteSleepSec();
                sSleepUSec = smuProperty::getChkptBulkWriteSleepUSec();
                
                if ((sSleepSec != 0) || (sSleepUSec != 0))
                {
                    /* BUG-32670    [sm-disk-resource] add IO Stat information 
                     * for analyzing storage performance.
                     * Microsecond�� ��ȯ�� �� WaitTime�� �����. */
                    sWaitTime += sSleepSec * 1000 * 1000 + sSleepUSec;
                    sTV.set(idlOS::time(NULL),0);
                    
                    sAddTV.set((sSleepSec + (sSleepUSec/1000000)),
                               (sSleepUSec%1000000));
                    
                    sTV += sAddTV;
                    
                    IDE_TEST( lock() != IDE_SUCCESS );
                    sDPMutexState = 1;
                    
                    while ( 1 )
                    {
                        sDPMutexState = 0;
                        
                        sRC = mCV.timedwait(&mMutex, &sTV);
                        sDPMutexState = 1;
                        
                        if ( sRC != IDE_SUCCESS )
                        {
                            IDE_TEST_RAISE(mCV.isTimedOut() != ID_TRUE, err_cond_wait);
                            
                            errno = 0;
                            break;
                        }
                    }
                    
                    sDPMutexState = 0;
                    IDE_TEST( unlock() != IDE_SUCCESS );
                }
                sWritePageCnt = 0;
            }
            
        }
        
        sStage = 0;
        IDE_TEST( sCurPCHPtr->mMutex.unlock() != IDE_SUCCESS );
        
        if(sDirtyStat == SMM_PCH_DIRTY_STAT_REMOVE)
        {
            remove( sCurPCHPtr );
            
            SC_MAKE_GRID(mArrPageGRID[sWriteCnt],
                         SC_MAX_SPACE_COUNT,
                         SC_MAX_PAGE_COUNT,
                         SC_MAX_OFFSET);
            
            sRemoveCnt++;
        }
        else
        {
            sCurPCHPtr->m_dirtyStat = SMM_PCH_DIRTY_STAT_REMOVE;
        }
        sWriteCnt++;

        // ������Ƽ�� ������ Sync Page ������ŭ Write Page�� 
        // �Ǹ� Sync�� �����Ѵ�.
        // ��, ������Ƽ ���� 0�̸� ���⿡�� DB Sync���� �ʰ�
        // ��� page�� ��� write�ϰ� checkpoint �������� �ѹ��� Sync�Ѵ�.
        if( (sPropSyncPageCount > 0) &&
            (((aTotalCnt + sWriteCnt) % sPropSyncPageCount) == 0) )
        {
            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_MRECOVERY_CHKP_FLUSH_SYNC_BEGIN, 
                         sPropSyncPageCount, sWriteCnt + aTotalCnt );

            /* BUG-32670    [sm-disk-resource] add IO Stat information 
             * for analyzing storage performance.
             * Sync�ϴµ� �ɸ� �ð��� �����. */
            IDV_TIME_GET(&sBeginTime);
            IDE_TEST( smmManager::syncDB(
                         SCT_SS_SKIP_CHECKPOINT, 
                         ID_FALSE /* synclatch ȹ���� �ʿ���� */) 
                      != IDE_SUCCESS );

            IDV_TIME_GET(&sEndTime);
            sSyncTime +=  IDV_TIME_DIFF_MICRO(&sBeginTime, &sEndTime);

            ideLog::log( SM_TRC_LOG_LEVEL_MRECOV,
                         SM_TRC_MRECOVERY_CHKP_FLUSH_SYNC_END );
        }
    } // while(sWriteCnt != sDirtyPageCnt)

    IDE_ASSERT(sWriteCnt == sDirtyPageCnt);
    
    *aWriteCnt  = sWriteCnt;
    *aRemoveCnt = sRemoveCnt;
    *aWaitTime  = sWaitTime;
    *aSyncTime  = sSyncTime;

    idlOS::qsort((void*)mArrPageGRID,
                 sDirtyPageCnt,
                 ID_SIZEOF(scGRID),
                 smrComparePageGRID);

    // �ߺ��� PID�� ������ Ȯ���Ѵ�.
    IDE_DASSERT( isAllPageUnique() == ID_TRUE );
    
    return IDE_SUCCESS;

    IDE_EXCEPTION( err_cond_wait );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrCondWait));
    }

    IDE_EXCEPTION_END;

    if (sDPMutexState != 0)
    {
        (void)unlock();
    }

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1 :
                IDE_ASSERT( sCurPCHPtr->mMutex.unlock()
                            == IDE_SUCCESS );

                break;
            default:
                break;
        }
    }
    IDE_POP();
        
    return IDE_FAILURE;
    
    
}

/*
   Page Image�� Checkpoint Image�� ����Ѵ�.
   
   [IN] aTBSNode - Page�� ���� Tablespace
   [IN] aUnstableDBNum - 0 or 1 => Page Image�� ����� Ping Pong��ȣ 
   [IN] aPageID  - ����Ϸ��� PageID
 */
IDE_RC smrDirtyPageList::writePageImage( smmTBSNode * aTBSNode,
                                         SInt         aUnstableDBNum,
                                         scPageID     aPageID )
{
    UInt                        sDBFileNo;
    smmDatabaseFile           * sDBFilePtr;
    smmChkptImageHdr            sChkptImageHdr;
    smiDataFileDescSlotID     * sDataFileDescSlotID;
    smiDataFileDescSlotID     * sAllocDataFileDescSlotID;
    idBool                      sResult;
    smmChkptImageAttr           sChkptImageAttr; 
    SInt                        sCurrentDB;
    smmDatabaseFile           * sCurrentDBFilePtr;
    


    IDE_DASSERT( aTBSNode != NULL );
    IDE_DASSERT( ( aUnstableDBNum == 0 ) || ( aUnstableDBNum == 1 ));
    
    /* --------------------------------------------
     * (010) get memory object of that database file
     * -------------------------------------------- */
    sDBFileNo = smmManager::getDbFileNo(aTBSNode, aPageID);
    
    IDE_TEST( smmManager::getDBFile( aTBSNode,
                                     aUnstableDBNum,
                                     sDBFileNo,
                                     SMM_GETDBFILEOP_NONE,
                                     &sDBFilePtr )
              != IDE_SUCCESS );
    
    /* ------------------------------------------------
     * (020) if the db file doesn't create, create and open
     * ------------------------------------------------ */

    if( smmManager::getCreateDBFileOnDisk( aTBSNode,
                                           aUnstableDBNum,
                                           sDBFileNo ) == ID_FALSE )
    {
        // BUG-46574 Checkpoint Image �������߿� ������ ���� Ȥ�� ��ũ full ������ 
        // ����� ���� �������� �̹����� �������� ���ϸ� 
        // ��ũ���� ������ ���������� TBSNode�� �α׾�Ŀ���� ���� ������ ��
        // ������ ��ȯ���� �ʰ� ���� �� ������Ѵ�.
        if ( sDBFilePtr->isOpen() == ID_TRUE )
        {
            (void)sDBFilePtr->close();
        }
        (void)idf::unlink( sDBFilePtr->getFileName() );
        
        if ( sDBFileNo > aTBSNode->mLstCreatedDBFile )
        {
            aTBSNode->mLstCreatedDBFile = sDBFileNo;
        }

        // ������ �����Ǹ鼭 DBF Hdr�� ��ϵȴ�. 
        IDE_TEST( sDBFilePtr->createDbFile( aTBSNode,
                                            aUnstableDBNum,
                                            sDBFileNo,
                                            0/* DB File Header�� ���*/)
                  != IDE_SUCCESS);
    }
    else
    {
        IDE_ASSERT( sDBFilePtr->isOpen() == ID_TRUE );
    }

    /* PROJ-2133 Incremental backup */
    if( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE )
    {
        sDBFilePtr->getChkptImageHdr( &sChkptImageHdr );

        sDataFileDescSlotID = &sChkptImageHdr.mDataFileDescSlotID;

        IDE_TEST_CONT( smriChangeTrackingMgr::isAllocDataFileDescSlotID(
                                                        sDataFileDescSlotID ) 
                       != ID_TRUE, skip_change_tracking );

        IDE_TEST( smriChangeTrackingMgr::isNeedAllocDataFileDescSlot( 
                                                          sDataFileDescSlotID,
                                                          sChkptImageHdr.mSpaceID,
                                                          sDBFileNo,
                                                          &sResult )
                  != IDE_SUCCESS );

        if( sResult == ID_TRUE )
        {
            IDE_TEST( smriChangeTrackingMgr::addDataFile2CTFile( 
                                                          sChkptImageHdr.mSpaceID,
                                                          sDBFileNo,
                                                          SMRI_CT_MEM_TBS,
                                                          &sAllocDataFileDescSlotID )
                      != IDE_SUCCESS );

            sDBFilePtr->setChkptImageHdr( NULL, // aMemRedoLSN
                                          NULL, // aMemCreateLSN
                                          NULL, // spaceID
                                          NULL, // smVersion
                                          sAllocDataFileDescSlotID );

            sCurrentDB = smmManager::getCurrentDB(aTBSNode);

            /* chkptImage�� ���� DataFileDescSlot�� ���Ҵ��� restart
             * recovery�ÿ��� ����ȴ�. restart recovery�� checkpoint��
             * nextStableDB�� ����ǰ� media recovery�� checkpoint�� currentDB��
             * ����ȴ�. ��,sCurrentDB�� aUnstableDBNum�� �޶���Ѵ�.
             */
            IDE_DASSERT( sCurrentDB != aUnstableDBNum );

            IDE_TEST( smmManager::getDBFile( aTBSNode,
                                             sCurrentDB,
                                             sDBFileNo,
                                             SMM_GETDBFILEOP_NONE,
                                             &sCurrentDBFilePtr )
                      != IDE_SUCCESS );

            sCurrentDBFilePtr->setChkptImageHdr( NULL, // aMemRedoLSN
                                                 NULL, // aMemCreateLSN
                                                 NULL, // spaceID
                                                 NULL, // smVersion
                                                 sAllocDataFileDescSlotID );

            sDBFilePtr->getChkptImageAttr( aTBSNode, &sChkptImageAttr ); 

            IDE_ASSERT( smrRecoveryMgr::updateChkptImageAttrToAnchor(
                                               &(aTBSNode->mCrtDBFileInfo[ sDBFileNo ]),
                                               &sChkptImageAttr )
                        == IDE_SUCCESS );
        }
        
        IDE_EXCEPTION_CONT( skip_change_tracking );
    }

    /* --------------------------------------------
     * (030) write dirty page to database file
     * -------------------------------------------- */
    IDE_TEST( writePageNormal( aTBSNode,
                               sDBFilePtr,
                               aPageID ) != IDE_SUCCESS );

    return IDE_SUCCESS ;
    
    IDE_EXCEPTION_END;
    
    return IDE_FAILURE;
    
}

IDE_RC smrDirtyPageList::writePageNormal( smmTBSNode       * aTBSNode,
                                          smmDatabaseFile  * aDBFilePtr,
                                          scPageID           aPID)
{
    UInt                 sState = 0;
    SInt                 sSystemErrno;

    while(1)
    {
        /* ------------------------------------------------
         *  errno�� �ʱ�ȭ �Ѵ�. ������ �Ǵ��ϱ� ���ؼ���.
         * ----------------------------------------------*/
        if( aPID == 0 )
        {
            IDE_TEST( smmManager::lockBasePage(aTBSNode) != IDE_SUCCESS );
            sState = 1;
        }

        errno = 0;
        if ( aDBFilePtr->writePage(aTBSNode, aPID) == IDE_SUCCESS )
        {
            if ( sState == 1 )
            {
                sState = 0;
                IDE_TEST( smmManager::unlockBasePage(aTBSNode)
                          != IDE_SUCCESS );
            }

            break;
        }

        if ( sState == 1 )
        {
            sState = 0;
            IDE_TEST( smmManager::unlockBasePage(aTBSNode) != IDE_SUCCESS );
        }

        sSystemErrno = ideGetSystemErrno();

        /* ------------------------------------------------
         *  Write���� ������ �߻��� ��Ȳ��
         * ----------------------------------------------*/
        IDE_TEST( (sSystemErrno != 0) && (sSystemErrno != ENOSPC) );

        while ( idlVA::getDiskFreeSpace(aDBFilePtr->getDir())
                < (SLong)SM_PAGE_SIZE )
        {
            smLayerCallback::setEmergency( ID_TRUE );
            if ( smrRecoveryMgr::isFinish() == ID_TRUE )
            {
                ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                            SM_TRC_MRECOVERY_DIRTY_PAGE_LIST_FATAL1);
                IDE_ASSERT(0);
            }
            else
            {
                ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                            SM_TRC_MRECOVERY_DIRTY_PAGE_LIST_WARNNING);
                idlOS::sleep(2);
            }
        }

        smLayerCallback::setEmergency( ID_FALSE );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sState == 1)
    {
        IDE_ASSERT( smmManager::unlockBasePage(aTBSNode) == IDE_SUCCESS );
    }

    return IDE_FAILURE;
}

void smrDirtyPageList::removeAll( idBool aIsForce )
{

    smmPCH*  sLstPCHPtr;
    smmPCH*  sCurPCHPtr;

    sCurPCHPtr = mFstPCH.m_pnxtDirtyPCH;

    while( sCurPCHPtr != &mFstPCH )
    {
        if ( aIsForce == ID_FALSE )
        {
            // �Ϲ����� ��쿡�� ���ŵǴ� ��� PCH�� ���°� REMOVE �����Ѵ�. 
            IDE_ASSERT((sCurPCHPtr->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
                       == SMM_PCH_DIRTY_STAT_REMOVE);
        }
        else
        {
            // ������ �����ϴ� ��쿡�� �˻����� �ʴ´�. 
        }

        sCurPCHPtr->m_dirty = ID_FALSE;
        sCurPCHPtr->m_dirtyStat = SMM_PCH_DIRTY_STAT_INIT;

        sLstPCHPtr = sCurPCHPtr;

        sCurPCHPtr->m_pnxtDirtyPCH->m_pprvDirtyPCH
            = sCurPCHPtr->m_pprvDirtyPCH;
        sCurPCHPtr->m_pprvDirtyPCH->m_pnxtDirtyPCH
            = sCurPCHPtr->m_pnxtDirtyPCH;
        sCurPCHPtr = sCurPCHPtr->m_pnxtDirtyPCH;

        sLstPCHPtr->m_pnxtDirtyPCH = NULL;
        sLstPCHPtr->m_pprvDirtyPCH = NULL;

        mDirtyPageCnt--;
    }

    IDE_ASSERT(mDirtyPageCnt == 0);
    IDE_ASSERT(mFstPCH.m_pnxtDirtyPCH == &mFstPCH);
    IDE_ASSERT(mFstPCH.m_pprvDirtyPCH == &mFstPCH);

    return;
}

/*
   SMM Dirty Page Mgr�κ��� Dirty Page���� �����´�
   
   [IN]  aSmmDPMgr - SMM Dirty Page Mgr
   [OUT] aNewCnt   - ���� �߰��� Dirty Page ��
   [OUT] aDupCnt   - ������ �����Ͽ��� Dirty Page ��

   [���ü�����]
      TBS.SyncMutex�� ���� ä�� �� �Լ��� ȣ��Ǿ�� �Ѵ�.
      
      ���� : TBS.SyncMutex�� �������� TBS�� DROP/OFFLINE���� ���̵���
             ������ ������ �� �ֱ� ����
             smrDPListMgr::writeDirtyPageAction �ּ� ���� 
 */
IDE_RC smrDirtyPageList::moveDirtyPagesFrom( smmDirtyPageMgr * aSmmDPMgr,
                                             UInt            * aNewCnt,
                                             UInt            * aDupCnt)
{
    UInt               i;
    UInt               sDirtyPageListCount;
    smmDirtyPageList * sDirtyPageList;

    smPCSlot         * sPCHSlot;
    smmPCH           * sPCH;
    idBool             sIsPCHLocked = ID_FALSE;
    idBool             sIsCount = ID_TRUE;
    UInt               sDirtyStat;
    UInt               sNewCnt    = 0;
    UInt               sDupCnt    = 0;
    
    IDE_DASSERT( aSmmDPMgr != NULL );
    IDE_DASSERT( aNewCnt   != NULL );
    IDE_DASSERT( aDupCnt   != NULL );
    
    sDirtyPageListCount = aSmmDPMgr->getDirtyPageListCount();
    for(i = 0; i < sDirtyPageListCount; i++)
    {
        sDirtyPageList = aSmmDPMgr->getDirtyPageList(i);
        IDE_TEST( sDirtyPageList->open() != IDE_SUCCESS );

        while(1)
        {
            IDE_TEST( sDirtyPageList->read( &sPCHSlot ) != IDE_SUCCESS );
            if( sPCHSlot == NULL )
            {
                break;
            }
            sPCH = (smmPCH*)sPCHSlot->mPCH;
            IDE_ASSERT(sPCH != NULL);

            if (((sPCH->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
                 == SMM_PCH_DIRTY_STAT_FLUSHDUP) ||
                ((sPCH->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK)
                 == SMM_PCH_DIRTY_STAT_FLUSH))
            {
                sIsCount = ID_FALSE;
            }
            else
            {
                sIsCount = ID_TRUE;
            }
            
            // Free Page �޸𸮰� �������� �ݳ��Ǿ� m_page �� NULL�� ���� �ִ�.
            // �̹� �ݳ��� �޸𸮶�� Dirty��ų �ʿ䰡 ����.
            // ���� ! �ڵ尡 ����Ǿ� lock/unlock ���̿���
            //       ���� �߻��ϰ� �Ǹ� stageó�� �ؾ���
            IDE_TEST( sPCH->mMutex.lock( NULL /* idvSQL* */ )
                      != IDE_SUCCESS );
            
            sIsPCHLocked = ID_TRUE;

            if ( sPCHSlot->mPagePtr != NULL ) // Dirty�Ǿ��ٰ� �̹� Free�� Page
            {
//                IDE_ASSERT( smLayerCallback::getPersPageID(sPCH->m_page) <
//                            smmManager::getDBMaxPageCount() );

                add( sPCH,
                     smLayerCallback::getPersPageID( sPCHSlot->mPagePtr ) );
            }
            
            sIsPCHLocked = ID_FALSE;
            IDE_TEST( sPCH->mMutex.unlock() != IDE_SUCCESS );
            
            sDirtyStat = sPCH->m_dirtyStat & SMM_PCH_DIRTY_STAT_MASK;

            if ((sIsCount == ID_TRUE) && (sDirtyStat == SMM_PCH_DIRTY_STAT_FLUSHDUP))
            {
                sDupCnt++;
            }
            else if (sDirtyStat == SMM_PCH_DIRTY_STAT_FLUSH)
            {
                sNewCnt++;
            }
        }

        IDE_TEST( sDirtyPageList->close() != IDE_SUCCESS );
    }

    *aNewCnt = sNewCnt;
    *aDupCnt = sDupCnt;
    
    return IDE_SUCCESS;

    IDE_EXCEPTION_END;
    
    IDE_PUSH();
    {
        if ( sIsPCHLocked == ID_TRUE )
        {
            IDE_ASSERT( sPCH->mMutex.unlock() == IDE_SUCCESS );
        }
    }
    IDE_POP();
    
    return IDE_FAILURE;
}

