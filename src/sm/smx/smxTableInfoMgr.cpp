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
 * $Id: smxTableInfoMgr.cpp 90158 2021-03-10 00:30:31Z emlee $
 **********************************************************************/

# include <smErrorCode.h>
# include <smp.h>
# include <smx.h>
# include <smr.h>
# include <sdp.h>

/***********************************************************************
 * Description : Table Info Manager ���� �ʱ�ȭ
 *
 * Ʈ����Ǹ��� ���̺����������ڸ� ������ ������, �������������� Ʈ�����
 * �����ʱ�ȭ�ÿ� ���̺����������ڵ� �Բ� �ʱ�ȭ�ȴ�.
 *
 * - ��� �޸��Ҵ��� �����ϱ� ���� SMX_TABLEINFO_CACHE_COUNT��ŭ
 *   �̸� smxTableInfo ��ü�� �Ҵ��ϸ�, smxTableInfo�� freelist�� �����Ѵ�.
 * - Hash�� ����Ͽ� table OID�� �Է��Ͽ� smxTableInfo ��ü�� �˻������ϰ� �Ѵ�.
 * - Hash�� ���Ե� smxTableInfo ��ü�鿡 ���Ͽ� �߰������� smxTableInfo
 *   ��ü ���� ����Ʈ�� �����Ѵ�. ��ü ���� ����Ʈ�� table OID ������ ������
 *   �Ǿ� �ִ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::initialize()
{
    UInt          i;
    smxTableInfo *sCurTableInfo;

    mCacheCount = SMX_TABLEINFO_CACHE_COUNT;
    mFreeTableInfoCount = mCacheCount;

    IDE_TEST( mHash.initialize( IDU_MEM_SM_TABLE_INFO,
                                SMX_TABLEINFO_CACHE_COUNT,
                                SMX_TABLEINFO_HASHTABLE_SIZE ) 
              != IDE_SUCCESS );

    mTableInfoFreeList.mPrevPtr = &mTableInfoFreeList;
    mTableInfoFreeList.mNextPtr = &mTableInfoFreeList;
    mTableInfoFreeList.mRecordCnt = ID_ULONG_MAX;

    /* TMS ���׸�Ʈ�� ���� ������� Ž�� ���� Data �������� PID */
    mTableInfoFreeList.mHintDataPID = SD_NULL_PID;

    mTableInfoList.mPrevPtr = &mTableInfoList;
    mTableInfoList.mNextPtr = &mTableInfoList;
    mTableInfoList.mRecordCnt = ID_ULONG_MAX;

    for ( i = 0 ; i < mCacheCount ; i++ )
    {
        /* TC/FIT/Limit/sm/smx/smxTableInfoMgr_initialize_malloc.sql */
        IDU_FIT_POINT_RAISE( "smxTableInfoMgr::initialize::malloc",
                              insufficient_memory );

        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_TABLE_INFO,
                                           ID_SIZEOF(smxTableInfo),
                                           (void**)&sCurTableInfo) 
                        != IDE_SUCCESS,
                        insufficient_memory );

        initTableInfo(sCurTableInfo);
        addTableInfoToFreeList(sCurTableInfo);
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table Info Manager ���� ����
 *
 * ������������� Ʈ����� ���������ÿ� ���̺����������ڵ� �Բ� �����ȴ�.
 * - Hash �� �̸� �Ҵ�Ǿ� �ִ� smxTableInfo ��ü���� ��� �����Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::destroy()
{
    smxTableInfo *sCurTableInfo;
    smxTableInfo *sNxtTableInfo;

    IDE_TEST( init() != IDE_SUCCESS );

    sCurTableInfo = mTableInfoFreeList.mNextPtr;

    while( sCurTableInfo != &mTableInfoFreeList )
    {
        sNxtTableInfo = sCurTableInfo->mNextPtr;

        removeTableInfoFromFreeList(sCurTableInfo);

        IDE_TEST( iduMemMgr::free(sCurTableInfo) != IDE_SUCCESS );

        sCurTableInfo = sNxtTableInfo;
    }

    mFreeTableInfoCount = 0;

    IDE_TEST( mHash.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Table Info Manager ���� ����
 *
 * ������������� Ʈ����� ���������ÿ� ���̺����������ڵ� �Բ� �����ȴ�.
 * - Hash �� �̸� �Ҵ�Ǿ� �ִ� smxTableInfo ��ü���� ��� �����Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::init()
{
    smxTableInfo   *sCurTableInfo;
    smxTableInfo   *sNxtTableInfo;

    sCurTableInfo = mTableInfoList.mNextPtr;

    // Update Table Record And Unlock Mutex.
    while ( sCurTableInfo != &mTableInfoList )
    {
        sNxtTableInfo = sCurTableInfo->mNextPtr;

        IDE_TEST( mHash.remove( sCurTableInfo->mTableOID )
                  != IDE_SUCCESS );

        removeTableInfoFromList( sCurTableInfo );
        IDE_TEST( freeTableInfo( sCurTableInfo ) != IDE_SUCCESS );

        sCurTableInfo = sNxtTableInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� �˻��Ͽ� ��ȯ�Ѵ�.
 *
 * ���̺� Cursor open �Ǵ� Ʈ����� öȸ�������� ȣ��Ǹ�, �˻��ϰ����ϴ� ���̺���
 * OID�� �Է��Ͽ� �ش� ���̺��� smxTableInfo ��ü�� ��ȯ�Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::getTableInfo( smOID          aTableOID,
                                      smxTableInfo **aTableInfoPtr,
                                      idBool         aIsSearch )
{
    smxTableInfo   *sTableInfoPtr;

    IDE_DASSERT( aTableOID  != SM_NULL_OID );
    IDE_DASSERT( aTableInfoPtr != NULL );

    sTableInfoPtr = NULL;
    sTableInfoPtr = (smxTableInfo*)(mHash.search(aTableOID));

    if ( ( sTableInfoPtr == NULL ) && ( aIsSearch == ID_FALSE ) )
    {
        IDE_TEST( allocTableInfo(&sTableInfoPtr) != IDE_SUCCESS );

        sTableInfoPtr->mTableOID    = aTableOID;

        sTableInfoPtr->mDirtyPageID = ID_UINT_MAX;
        
        IDE_TEST( mHash.insert(aTableOID, sTableInfoPtr) != IDE_SUCCESS );

        addTableInfoToList(sTableInfoPtr);
    }

    *aTableInfoPtr = sTableInfoPtr;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���  smxTableInfo ��ü �� TableHeader  �� �����ϸ�
 * ModifyCount�� ���������ش�.
 ***********************************************************************/
void smxTableInfoMgr::addTablesModifyCount( void )
{

    smxTableInfo   * sTableInfo = NULL;
    smcTableHeader * sTableHeader = NULL;

    sTableInfo = mTableInfoList.mNextPtr;

    while ( sTableInfo != &mTableInfoList )
    {
        IDE_ASSERT( smcTable::getTableHeaderFromOID( sTableInfo->mTableOID,
                                                     (void**)&sTableHeader )
                     == IDE_SUCCESS );

        smcTable::addModifyCount( sTableHeader );
        sTableInfo = sTableInfo->mNextPtr;
    }
}

/***********************************************************************
 * Description : Ʈ����� commit�� Ʈ������� ������ ���̺�鿡 ���ؼ�
 *               max rows�� �˻��Ѵ�.
 *
 * ���̺������������� smxTableInfo ��ü ���� ����Ʈ(order by table OID)�� ��ȸ�ϸ鼭
 * ������ ���̺���� page list entry�� mutex�� ��´�.
 *
 * - �޸����̺��� row count�� ������ ���̺� ���ؼ��� mutex�� ��´�.
 * - disk ���̺��� ��� ��´�.
 * - deadlock�� �����ϱ� ���� �ѹ������� mutex�� ��´�.
 ***********************************************************************/

IDE_RC smxTableInfoMgr::requestAllEntryForCheckMaxRow()
{
    smxTableInfo   *sCurTableInfoPtr;
    smxTableInfo   *sNxtTableInfoPtr;
    smcTableHeader *sCurTableHeaderPtr;
    ULong           sTotalRecord;
    UInt            i, j;
    UInt            sTableType;

    i = 0;
    sTotalRecord = 0;

    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    // Check And Lock Mutex.
    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        i++;
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                     == IDE_SUCCESS );
       
        sTableType = SMI_GET_TABLE_TYPE( sCurTableHeaderPtr );

        /* BUG-47368: �̹� ID_ULONG_MAX ���̸� üũ �� �ʿ� ����. �� ��쿡��
         * ���� ���� �ʰ�, �� ������ ȣ��Ǵ� ������ �Լ��� 
         * releaseEntryAndUpdateMemTableInfoForIns(), 
         * updateMemTableInfoForDe|()���� Atomic ����ó�� �Ѵ�.  
         */  
        if ( ( sCurTableHeaderPtr->mMaxRow == ID_ULONG_MAX ) &&
             ( sTableType != SMI_TABLE_DISK ) )    
        {
            sCurTableInfoPtr = sNxtTableInfoPtr;
            continue;
        }

        if ( ( sTableType == SMI_TABLE_META )   ||
             ( sTableType == SMI_TABLE_MEMORY ) ||
             ( sTableType == SMI_TABLE_FIXED ) )
        {
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                IDE_TEST_RAISE(
                    sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mMutex.lock(NULL /* idvSQL* */)
                               != IDE_SUCCESS, err_mutex_lock);

                sTotalRecord = sCurTableInfoPtr->mRecordCnt +
                    sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mInsRecCnt;
            }
        }
        else if ( sTableType == SMI_TABLE_VOLATILE )
        {
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                IDE_TEST_RAISE(
                    sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mMutex.lock(NULL /* idvSQL* */)
                               != IDE_SUCCESS, err_mutex_lock);
                sTotalRecord = sCurTableInfoPtr->mRecordCnt +
                               sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mInsRecCnt;
            }
        }
        else if ( sTableType == SMI_TABLE_DISK )
        {
            if ( sCurTableInfoPtr->mRecordCnt != 0 )
            {
                IDE_TEST_RAISE( (sCurTableHeaderPtr->mFixed.mDRDB.mMutex)->lock(NULL /* idvSQL* */)
                                != IDE_SUCCESS, err_mutex_lock );

                if ( sCurTableInfoPtr->mRecordCnt > 0 )
                {
                    sTotalRecord = sCurTableInfoPtr->mRecordCnt +
                                   sCurTableHeaderPtr->mFixed.mDRDB.mRecCnt;
                }
            }
        }
        else
        {
            IDE_ASSERT(0);
        }

        IDE_TEST_RAISE( sCurTableHeaderPtr->mMaxRow < sTotalRecord,
                        err_exceed_maxrow );

        sTotalRecord = 0;
        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_mutex_lock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexLock));
    }
    IDE_EXCEPTION(err_exceed_maxrow);
    {
        IDE_SET(ideSetErrorCode(smERR_ABORT_ExceedMaxRows));
    }
    IDE_EXCEPTION_END;

    j = 0;
    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    while ( j != i )
    {
        j++;
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;
        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                    == IDE_SUCCESS );
        sTableType = SMI_GET_TABLE_TYPE( sCurTableHeaderPtr );

        if ( ( sTableType == SMI_TABLE_MEMORY ) ||
             ( sTableType == SMI_TABLE_META )   ||
             ( sTableType == SMI_TABLE_FIXED ) )
        {
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                (void)(sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mMutex.unlock());
            }
        }
        else if ( sTableType == SMI_TABLE_VOLATILE )
        {
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                (void)(sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mMutex.unlock());
            }
        }
        else if ( sTableType == SMI_TABLE_DISK )
        {
            if ( sCurTableInfoPtr->mRecordCnt != 0 )
            {
                (void)(sCurTableHeaderPtr->mFixed.mDRDB.mMutex)->unlock();
            }
        }
        else
        {
            IDE_ASSERT(0);
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �޸� ���̺��� ������ row ������ �����ϰ�
 * entry mutex�� release�Ѵ�.
 ***********************************************************************/

IDE_RC smxTableInfoMgr::releaseEntryAndUpdateMemTableInfoForIns()
{
    smxTableInfo   *sCurTableInfoPtr;
    smxTableInfo   *sNxtTableInfoPtr;
    smcTableHeader *sCurTableHeaderPtr;
    UInt            sTableType;

    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                    == IDE_SUCCESS );
        sTableType = SMI_GET_TABLE_TYPE( sCurTableHeaderPtr );

        if ( ( sTableType == SMI_TABLE_MEMORY ) ||
             ( sTableType == SMI_TABLE_META )   ||
             ( sTableType == SMI_TABLE_FIXED ) )
        {
            /* TASK-4990 changing the method of collecting index statistics 
             * ���ŵ� Row ������ ������Ŵ */
            /* BUG-47368: ������ ������ atomic���� ���� �� ��� SINT_MAX ��
             * ���߾� �ʱ�ȭ �� �ʿ�� ���� ����. X$DBMS_STATS ����.
             */
            if ( sCurTableInfoPtr->mRecordCnt < 0 )
            {
                (void)idCore::acpAtomicAdd64 ( &(sCurTableHeaderPtr->mStat.mNumRowChange), 
                                               ( -1 * sCurTableInfoPtr->mRecordCnt ) );
            }
            else
            {
                (void)idCore::acpAtomicAdd64 ( &(sCurTableHeaderPtr->mStat.mNumRowChange), 
                                               sCurTableInfoPtr->mRecordCnt );
            }
            
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                // BUG-47368: ForInsert()
                (void)idCore::acpAtomicAdd64( &(sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mInsRecCnt),
                                              sCurTableInfoPtr->mRecordCnt );

                if ( sCurTableHeaderPtr->mMaxRow != ID_ULONG_MAX ) 
                {
                    IDE_TEST_RAISE(
                        sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mMutex.unlock()
                        != IDE_SUCCESS, err_mutex_unlock);
                }
                
                sCurTableInfoPtr->mRecordCnt = 0;
            }
        }
        else if ( sTableType == SMI_TABLE_VOLATILE )
        {
            if ( sCurTableInfoPtr->mRecordCnt > 0 )
            {
                // BUG-47368: ForInsert() 
                (void)idCore::acpAtomicAdd64( &(sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mInsRecCnt),
                                              sCurTableInfoPtr->mRecordCnt );

                if ( sCurTableHeaderPtr->mMaxRow != ID_ULONG_MAX )
                {
                    IDE_TEST_RAISE(
                        sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mMutex.unlock()
                        != IDE_SUCCESS, err_mutex_unlock);
                }
                
                sCurTableInfoPtr->mRecordCnt = 0;
            }
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_mutex_unlock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/***********************************************************************
 * Description : ��ũ ���̺��� ������ row ������ ���Ű� �Բ� commit�ϰ�
 * entry mutex�� release�Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::releaseEntryAndUpdateDiskTableInfoWithCommit(
                                                   idvSQL    * aStatistics,
                                                   smxTrans  * aTransPtr,
                                                   smSCN       aCommitSCN,
                                                   smLSN     * aEndLSN )
{
    smxTableInfo      *sCurTableInfoPtr;
    smxTableInfo      *sNxtTableInfoPtr;
    smcTableHeader    *sCurTableHeaderPtr;
    smrTransCommitLog  sCommitLog;
    smrTransCommitLog *sCommitLogHead;
    scPageID           sPageID;
    scOffset           sOffset;
    ULong              sRecordCount;
    sdrMtx             sMtx;
    UInt               sState = 0;
    sdcTSS            *sTSSlotPtr;
    idBool             sTrySuccess;
    UInt               sDskRedoSize;
    UInt               sLogSize;
    smSCN              sInitSCN;
    smuDynArrayBase   *sDynArrayPtr;
    sdSID              sTSSlotSID = SD_NULL_SID;

    aTransPtr->initLogBuffer();
 
    IDE_TEST( sdrMiniTrans::begin( aStatistics,
                                   &sMtx,
                                   (void*)aTransPtr,
                                   SDR_MTX_LOGGING,
                                   ID_TRUE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT |
                                   SM_DLOG_ATTR_TRANS_LOGBUFF )
              != IDE_SUCCESS );
    sState = 1;

    sTSSlotSID = smxTrans::getTSSlotSID( aTransPtr );

    if ( sTSSlotSID != SD_NULL_SID )
    {
        aTransPtr->initCommitLog( &sCommitLog, SMR_LT_DSKTRANS_COMMIT, aCommitSCN  );
    }
    else
    {
        /*BUGBUG : mTXSegEntry != NULL �� Ȯ���ϰ� �Դµ� �������� �ü� �ֳ�? */
        IDE_DASSERT( 0 );

        aTransPtr->initCommitLog( &sCommitLog, SMR_LT_MEMTRANS_COMMIT, aCommitSCN );
    }

    /* BUG-29262 TSS �Ҵ翡 ������ Ʈ������� COMMIT �α׸� ����ؾ� �մϴ�.
     * Transaction CommitLog�� logHead�� ����Ѵ�. */
    IDE_TEST( aTransPtr->writeLogToBuffer( &sCommitLog,
                                           SMR_LOGREC_SIZE(smrTransCommitLog) ) 
              != IDE_SUCCESS );

    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                    == IDE_SUCCESS );

        if ( ( SMI_TABLE_TYPE_IS_DISK( sCurTableHeaderPtr ) == ID_TRUE ) &&
             ( sCurTableInfoPtr->mRecordCnt != 0 ) )
        {
            /* TASK-4990 changing the method of collecting index statistics 
             * ���ŵ� Row ������ ������Ŵ */
            if ( sCurTableInfoPtr->mRecordCnt < 0 )
            {
                sCurTableHeaderPtr->mStat.mNumRowChange -=
                                                sCurTableInfoPtr->mRecordCnt;
            }
            else
            {
                sCurTableHeaderPtr->mStat.mNumRowChange +=
                                                sCurTableInfoPtr->mRecordCnt;
            }
            /* �ʱ�ȭ */
            if ( sCurTableHeaderPtr->mStat.mNumRowChange > ID_SINT_MAX )
            {
                sCurTableHeaderPtr->mStat.mNumRowChange = 0;
            }
            
            sRecordCount = sCurTableHeaderPtr->mFixed.mDRDB.mRecCnt +
                sCurTableInfoPtr->mRecordCnt;
            
            sPageID = SM_MAKE_PID(sCurTableHeaderPtr->mSelfOID);
            sOffset = SM_MAKE_OFFSET(sCurTableHeaderPtr->mSelfOID)
                      + SMP_SLOT_HEADER_SIZE;

            IDE_TEST( aTransPtr->writeLogToBuffer( &sPageID,
                                                   ID_SIZEOF( scPageID ) )
                       != IDE_SUCCESS );

            IDE_TEST( aTransPtr->writeLogToBuffer( &sOffset,
                                                   ID_SIZEOF( scOffset ) )
                      != IDE_SUCCESS );

            IDE_TEST( aTransPtr->writeLogToBuffer( &sRecordCount,
                                                   ID_SIZEOF( ULong ) )
                      != IDE_SUCCESS );

            sCurTableInfoPtr->mDirtyPageID = sPageID;
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    if ( sTSSlotSID != SD_NULL_SID )
    {
        IDE_TEST( sdbBufferMgr::getPageBySID( 
                                   aStatistics,
                                   SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                   sTSSlotSID,
                                   SDB_X_LATCH,
                                   SDB_WAIT_NORMAL,
                                   &sMtx,
                                   (UChar**)&sTSSlotPtr,
                                   &sTrySuccess ) != IDE_SUCCESS );

        SM_INIT_SCN( &sInitSCN );

        IDE_TEST( sdcTSSlot::setCommitSCN( &sMtx,
                                           sTSSlotPtr,
                                           &sInitSCN,
                                           ID_FALSE ) // aDoUpdate
                  != IDE_SUCCESS );

        // ���ݱ��� �ۼ��� Mtx���� Log Buffer�� Ʈ����ǿ� �ε��Ѵ�.
        sDynArrayPtr = &(sMtx.mLogBuffer);
        sDskRedoSize = smuDynArray::getSize( sDynArrayPtr );

        IDE_TEST( aTransPtr->writeLogToBufferUsingDynArr( sDynArrayPtr,
                                                          sDskRedoSize )
                  != IDE_SUCCESS );
    }
    else
    {
        // Memory Ʈ����� Ȥ�� Parallel DPath �� ��� Main Thread�� 
        // TSS�� �Ҵ����� �ʴ´�.
        sDskRedoSize = 0;
    }

    /* BUG-29262 TSS �Ҵ翡 ������ Ʈ������� COMMIT �α׸� ����ؾ� �մϴ�.
     * Transaction CommitLog�� logTail�� ����Ѵ�. */
    IDE_TEST( aTransPtr->writeLogToBuffer( &sCommitLog.mHead.mType,
                                           ID_SIZEOF( smrLogType ) ) 
              != IDE_SUCCESS );

    /* BUG-29262 TSS �Ҵ翡 ������ Ʈ������� COMMIT �α׸� ����ؾ� �մϴ�.
     * Transaction log buffer���� CommitLog�� size�� diskRedoSize�� �����Ѵ�. */
    sCommitLogHead = (smrTransCommitLog*)( aTransPtr->getLogBuffer() );
    sLogSize       = aTransPtr->getLogSize();

    smrLogHeadI::setSize( &sCommitLogHead->mHead, sLogSize );
    sCommitLogHead->mDskRedoSize = sDskRedoSize;

    IDU_FIT_POINT( "1.BUG-15608@smxTableInfoMgr::releaseEntryAndUpdateDiskTableInfoWithCommit" );

    sState = 0;
    IDE_TEST( sdrMiniTrans::commit( &sMtx,
                                    SMR_CT_END,
                                    aEndLSN,
                                    SMR_RT_DISKONLY )
              != IDE_SUCCESS );

    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                    == IDE_SUCCESS );

        if ( ( SMI_TABLE_TYPE_IS_DISK( sCurTableHeaderPtr ) == ID_TRUE ) &&
             ( sCurTableInfoPtr->mRecordCnt != 0 ) )
        {
            /* BUG-15608 : Commit�� TableHeader�� Record������ ����
             * Logging�� WAL�� ��Ű�� �ʽ��ϴ�. Commit Log�� ����Ŀ� Table
             * Header�� Record Count�� �����Ѵ�. */

            sCurTableHeaderPtr->mFixed.mDRDB.mRecCnt += sCurTableInfoPtr->mRecordCnt;

            sCurTableInfoPtr->mRecordCnt = 0;

            IDE_TEST_RAISE( (sCurTableHeaderPtr->mFixed.mDRDB.mMutex)->unlock()
                            != IDE_SUCCESS, err_mutex_unlock );

            if (sCurTableInfoPtr->mDirtyPageID != ID_UINT_MAX)
            {
                IDE_TEST( smmDirtyPageMgr::insDirtyPage(
                                              SMI_ID_TABLESPACE_SYSTEM_MEMORY_DIC,
                                              sCurTableInfoPtr->mDirtyPageID )
                          != IDE_SUCCESS );
            }
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_mutex_unlock);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexUnlock));
    }
    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_ASSERT( sdrMiniTrans::rollback( &sMtx ) == IDE_SUCCESS );
    }

    /* BUG-32576 [sm_transaction] The server does not unlock PageListMutex
     * if the commit log writing occurs an exception. */
    sCurTableInfoPtr = mTableInfoList.mNextPtr;
    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                     (void**)&sCurTableHeaderPtr )
                    == IDE_SUCCESS );

        if ( SMI_TABLE_TYPE_IS_DISK( sCurTableHeaderPtr ) &&
             ( sCurTableInfoPtr->mRecordCnt != 0 ) ) 
        {
            sCurTableInfoPtr->mRecordCnt = 0;

            IDE_TEST_RAISE( (sCurTableHeaderPtr->mFixed.mDRDB.mMutex)->unlock()
                            != IDE_SUCCESS, err_mutex_unlock );
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }

    return IDE_FAILURE;
}

void smxTableInfoMgr::updateMemTableInfoForDel()
{
    smxTableInfo   *sCurTableInfoPtr;
    smxTableInfo   *sNxtTableInfoPtr;
    smcTableHeader *sCurTableHeaderPtr;
    UInt            sTableType;

    sCurTableInfoPtr = mTableInfoList.mNextPtr;

    while ( sCurTableInfoPtr != &mTableInfoList )
    {
        sNxtTableInfoPtr = sCurTableInfoPtr->mNextPtr;

        if ( sCurTableInfoPtr->mRecordCnt < 0 )
        {
            IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfoPtr->mTableOID,
                                                         (void**)&sCurTableHeaderPtr )
                        == IDE_SUCCESS );

            sTableType = SMI_GET_TABLE_TYPE( sCurTableHeaderPtr );

            if ( ( sTableType == SMI_TABLE_MEMORY ) ||
                 ( sTableType == SMI_TABLE_META )   ||
                 ( sTableType == SMI_TABLE_FIXED ) )
            {
                // BUG-47368: ForDelete() 
                (void)idCore::acpAtomicAdd64( &(sCurTableHeaderPtr->mFixed.mMRDB.mRuntimeEntry->mInsRecCnt),
                                sCurTableInfoPtr->mRecordCnt );

                sCurTableInfoPtr->mRecordCnt = 0;
            }
            else if (sTableType == SMI_TABLE_VOLATILE)
            {
                // BUG-47368: ForDelete()
                (void)idCore::acpAtomicAdd64( &(sCurTableHeaderPtr->mFixed.mVRDB.mRuntimeEntry->mInsRecCnt),
                                sCurTableInfoPtr->mRecordCnt );

                sCurTableInfoPtr->mRecordCnt = 0;
            }
        }

        sCurTableInfoPtr = sNxtTableInfoPtr;
    }
}

/*******************************************************************************
 * Description: DPath INSERT�� ������ TX�� commit �� ��, �� table info�� ����
 *      �ʿ��� �۾����� ó���Ѵ�.
 *
 *      1. DPath INSERT�� ������ table�� index�� ������ ������ inconsistent
 *          �÷��׸� �����Ѵ�.
 *      2. DPath INSERT�� ������ table�� nologging �ɼ��� �����Ǿ� ������
 *          table header�� inconsistent �÷��׸� �����ϴ� redo �α׸� �����.
 ******************************************************************************/
IDE_RC smxTableInfoMgr::processAtDPathInsCommit()
{
    smxTableInfo    * sCurTableInfo;
    smxTableInfo    * sNxtTableInfo;
    smcTableHeader  * sTableHeader;
    UInt              sTableType;

    sCurTableInfo = mTableInfoList.mNextPtr;

    while( sCurTableInfo != &mTableInfoList )
    {
        sNxtTableInfo = sCurTableInfo->mNextPtr;

        if ( sCurTableInfo->mExistDPathIns == ID_TRUE )
        {
            IDE_ASSERT( smcTable::getTableHeaderFromOID( sCurTableInfo->mTableOID,
                                                         (void**)&sTableHeader )
                        == IDE_SUCCESS );

            sTableType = (sTableHeader->mFlag & SMI_TABLE_TYPE_MASK);

            IDE_ERROR( sTableType == SMI_TABLE_DISK );

            IDE_TEST( smcTable::setAllIndexInconsistency( sTableHeader )
                      != IDE_SUCCESS );

            if ( smcTable::isLoggingMode( sTableHeader ) == ID_FALSE )
            {
                //------------------------------------------------------------
                // NOLOGGING���� Direct-Path INSERT�� ����Ǵ� ���,
                // Redo Log�� ���� ������ Media Recovery �ÿ�
                // Direct-Path INSERT�� ����� page���� �ƹ��͵� ��ϵ���
                // �ʴ´�. �̶�, Table�� ���´� consistent ���� �ʴ�.
                // ���� �̸� detect �ϱ� ���Ͽ�
                // Direct-Path INSER�� NOLOGGING���� ó�� ����� ���,
                // table header�� inconsistent flag�� �����ϴ� �α׸� �α��Ѵ�.
                //------------------------------------------------------------
                IDE_TEST( smrUpdate::setInconsistencyAtTableHead(
                                                    sTableHeader,
                                                    ID_TRUE ) // media recovery �ÿ��� redo ������
                          != IDE_SUCCESS );
            }
            else
            {
                // LOGGING MODE�� Direct-Path INSERT�� ����Ǵ� ���
            }
        }

        sCurTableInfo = sNxtTableInfo;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� �޸� �Ҵ��Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::allocTableInfo( smxTableInfo  ** aTableInfoPtr )
{
    smxTableInfo  * sFreeTableInfo;
    UInt            sState  = 0;

    IDE_DASSERT( aTableInfoPtr != NULL );

    if ( mFreeTableInfoCount != 0 )
    {
        mFreeTableInfoCount--;
        sFreeTableInfo = mTableInfoFreeList.mNextPtr;
        removeTableInfoFromFreeList(sFreeTableInfo);
    }
    else
    {
        /* TC/FIT/Limit/sm/smx/smxTableInfoMgr_allocTableInfo_malloc.sql */
        IDU_FIT_POINT_RAISE( "smxTableInfoMgr::allocTableInfo::malloc",
                              insufficient_memory );

        IDE_TEST_RAISE( iduMemMgr::malloc( IDU_MEM_SM_TABLE_INFO,
                                           ID_SIZEOF(smxTableInfo),
                                           (void**)&sFreeTableInfo) != IDE_SUCCESS,
                        insufficient_memory );
        sState = 1;
    }

    initTableInfo(sFreeTableInfo);

    *aTableInfoPtr = sFreeTableInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sFreeTableInfo ) == IDE_SUCCESS );
            sFreeTableInfo = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� �޸� �����Ѵ�.
 ***********************************************************************/
IDE_RC smxTableInfoMgr::freeTableInfo( smxTableInfo  * aTableInfo )
{
    if ( mFreeTableInfoCount >= mCacheCount )
    {
        IDE_TEST( iduMemMgr::free(aTableInfo) != IDE_SUCCESS );
    }
    else
    {
        mFreeTableInfoCount++;
        addTableInfoToFreeList(aTableInfo);
    }
    return IDE_SUCCESS;
    IDE_EXCEPTION_END;
    return IDE_FAILURE;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� row count�� 1������Ų��.
 ***********************************************************************/
void  smxTableInfoMgr::incRecCntOfTableInfo( void  * aTableInfoPtr )
{
    IDE_DASSERT( aTableInfoPtr != NULL );

    ((smxTableInfo*)aTableInfoPtr)->mRecordCnt++;

    return;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� row count�� 1���ҽ�Ų��.
 ***********************************************************************/
void  smxTableInfoMgr::decRecCntOfTableInfo( void  * aTableInfoPtr )
{
    IDE_DASSERT( aTableInfoPtr != NULL );
    ((smxTableInfo*)aTableInfoPtr)->mRecordCnt--;
    return;
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� row count�� ��ȯ�Ѵ�.
 ***********************************************************************/
SLong smxTableInfoMgr::getRecCntOfTableInfo( void  * aTableInfoPtr )
{
    IDE_DASSERT( aTableInfoPtr != NULL );
    return (((smxTableInfo*)aTableInfoPtr)->mRecordCnt);
}

/***********************************************************************
 * Description : smxTableInfo ��ü�� ExistDPathIns�� �����Ѵ�.
 ***********************************************************************/
void smxTableInfoMgr::setExistDPathIns( void  * aTableInfoPtr,
                                        idBool  aExistDPathIns )
{
    IDE_DASSERT( aTableInfoPtr != NULL );
    ((smxTableInfo*)aTableInfoPtr)->mExistDPathIns = aExistDPathIns;
}
