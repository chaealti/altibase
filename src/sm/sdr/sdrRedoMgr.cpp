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
 * $Id: sdrRedoMgr.cpp 89217 2020-11-11 12:31:50Z et16 $
 *
 * Description :
 *
 * DRDB �������������� ����� �����ڿ� ���� ���������̴�.
 *
 **********************************************************************/

#include <sdb.h>
#include <smu.h>
#include <smErrorCode.h>
#include <smDef.h>
#include <smiDef.h>
#include <smx.h>
#include <sct.h>

#include <sdrDef.h>
#include <sdrRedoMgr.h>
#include <sdrMiniTrans.h>
#include <sdrUpdate.h>
#include <sdrCorruptPageMgr.h>
#include <sdrReq.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h>


iduMutex       sdrRedoMgr::mMutex;
smuHashBase    sdrRedoMgr::mHash;
UInt           sdrRedoMgr::mHashTableSize;
UInt           sdrRedoMgr::mApplyPageCount;
smiRecoverType sdrRedoMgr::mRecvType;     /* restart, complete, incomplete */

/*  for media recovery */
smuHashBase    sdrRedoMgr::mRecvFileHash;
UInt           sdrRedoMgr::mNoApplyPageCnt;
UInt           sdrRedoMgr::mRecvFileCnt; /* ������ ���� ���� */

// for properties
UInt           sdrRedoMgr::mMaxRedoHeapSize;    /* maximum heap memory */

/* PROJ-2118 BUG Reporting - debug info */
scSpaceID       sdrRedoMgr::mCurSpaceID = SC_NULL_SPACEID;
scPageID        sdrRedoMgr::mCurPageID  = SM_SPECIAL_PID;
UChar*          sdrRedoMgr::mCurPagePtr = NULL;
sdrRedoLogData* sdrRedoMgr::mRedoLogPtr = NULL;

/* BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�. */
smLSN           sdrRedoMgr::mLstDRDBPageUpdateLSN   = { 0, 0 };
scPageID        sdrRedoMgr::mLstUpdatedPageID = SD_NULL_PID;
scSpaceID       sdrRedoMgr::mLstUpdatedSpaceID = SC_NULL_SPACEID;

/***********************************************************************
 * Description : ����� �������� �ʱ�ȭ �Լ�
 *
 * restart recovery�� redoall pass�������� ȣ��ȴ�. aMaxPageCount��
 * ���۰������� ����Ǯ�� fix�� �� �ִ� �ִ� page �����̸�, �̴� redo �α���
 * �ش� page�� ���Ͽ� fix�� �� �ִ� �ִ밳���� ��Ÿ����. �� ���� �����ϸ�,
 * hash�� ����� redo �α׵��� �������� �ݿ��� ��, ����Ǯ�� flush�ϰ�, hash��
 * ����, �� �� ����ؼ� redo �α׸� �ٽ� hash�� �����Ѵ�.
 *
 * + 2nd. code design
 *   - mutex�� �ʱ�ȭ�Ѵ�.
 *   - �ؽ� ���̺� �ʱ�ȭ�Ѵ�.
 **********************************************************************/
IDE_RC sdrRedoMgr::initialize(UInt             aHashTableSize,
                              smiRecoverType   aRecvType)
{

    IDE_DASSERT(aHashTableSize > 0);

    IDE_TEST_RAISE( mMutex.initialize((SChar*)"redo manager mutex",
                                      IDU_MUTEX_KIND_NATIVE,
                                      IDV_WAIT_INDEX_NULL)
                    != IDE_SUCCESS,
                    error_mutex_init );

    mHashTableSize = aHashTableSize;
    mRecvType      = aRecvType;

    // HashKey : <space, pageID, unused>
    IDE_TEST( smuHash::initialize(&mHash,
                                  1,
                                  mHashTableSize,    /* aBucketCount */
                                  ID_SIZEOF(scGRID),  /* aKeyLength */
                                  ID_FALSE,          /* aUseLatch */
                                  genHashValueFunc,
                                  compareFunc) != IDE_SUCCESS );

    if (getRecvType() != SMI_RECOVER_RESTART)
    {
        // HashKey : <space, fileID, unused>
        IDE_TEST( smuHash::initialize(&mRecvFileHash,
                                      1,
                                      mHashTableSize,   /* aBucketCount */
                                      ID_SIZEOF(scGRID), /* aKeyLength */
                                      ID_FALSE,         /* aUseLatch */
                                      genHashValueFunc,
                                      compareFunc) != IDE_SUCCESS );
    }

    mApplyPageCount  = 0;
    mNoApplyPageCnt  = 0;
    mRecvFileCnt     = 0;
    SM_SET_LSN( mLstDRDBPageUpdateLSN, 0, 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_mutex_init );
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_ThrMutexInit));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : ����� �������� ���� �Լ�
 *
 * restart recovery�� redoall pass�������� ��� ��ũ redo log����
 * DB �������� �ݿ��� �� ���� ȣ��ȴ�.
 *
 * + 2nd. code design
 *   - �ؽ� ���̺��� �����Ѵ�.
 *   - mutex�� �����Ѵ�.
 **********************************************************************/
IDE_RC sdrRedoMgr::destroy()
{
    // BUG-28709 Media recovery�� disk�� media recovery���� ������
    //           mApplyPageCount�� 0�� �ƴϰ�,
    //           mApplyPageCount - mNoApplyPageCnt �� 0 �̾�� ��
    IDE_DASSERT( (mApplyPageCount - mNoApplyPageCnt) == 0 );

    if (getRecvType() != SMI_RECOVER_RESTART)
    {
        IDE_DASSERT( mRecvFileCnt == 0 );
        IDE_TEST( smuHash::destroy(&mRecvFileHash) != IDE_SUCCESS );
    }

    IDE_TEST( smuHash::destroy(&mHash) != IDE_SUCCESS );

    IDE_TEST_RAISE( mMutex.destroy() != IDE_SUCCESS, error_mutex_destroy );

    return IDE_SUCCESS;

    IDE_EXCEPTION( error_mutex_destroy );
    {
        IDE_SET(ideSetErrorCode (smERR_FATAL_ThrMutexDestroy));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : BUGBUG - 7983 MRDB runtime memory�� ���� redo
 *
 * �αװ������� restart recovery�� ����� �ܰ迡�� runtime memory�� ����
 * redo�� �ʿ��� drdb Ÿ���� �α׸� �ǵ� ȣ��ȴ�.
 **********************************************************************/
void sdrRedoMgr::redoRuntimeMRDB( void   * aTrans,
                                  SChar  * aLogRec )
{

    scGRID      sGRID;
    sdrLogType  sLogType;
    UInt        sValueLen;
    SChar*      sValue;

    IDE_DASSERT( aTrans  != NULL );
    IDE_DASSERT( aLogRec != NULL );

    parseRedoLogHdr( aLogRec,
                     &sLogType,
                     &sGRID,
                     &sValueLen );

    if (sValueLen != 0)
    {
        sValue  = aLogRec + ID_SIZEOF(sdrLogHdr);
    }
    else
    {
        sValue = NULL;
    }

    applyLogRecToRuntimeMRDB(aTrans, sLogType, sValue, sValueLen);
}

/***********************************************************************
 * Description : BUGBUG - 7983 runtime memory�� ���� redo
 *
 * redoRuntimeMRDB, applyHashedLogRec �Լ����� ȣ��ȴ�.
 **********************************************************************/
void sdrRedoMgr::applyLogRecToRuntimeMRDB( void*      aTrans,
                                           sdrLogType aLogType,
                                           SChar*     aValue,
                                           UInt       aValueLen )
{
    sdRID       sExtRID4TSS;
    sdSID       sTSSlotSID;
    SChar     * sValuePtr;
    smxTrans  * sTrans;
    UInt        sTXSegID;

    IDE_DASSERT( aTrans != NULL );

    sTrans    = (smxTrans*)aTrans;
    sValuePtr = aValue;

    switch(aLogType)
    {
        case SDR_SDC_BIND_TSS :

            IDE_ASSERT( aValue != NULL );
            IDE_ASSERT( (ID_SIZEOF(sdSID) +
                         ID_SIZEOF(sdRID) +
                         ID_SIZEOF(smTID) +
                        ID_SIZEOF(UInt) ) == aValueLen );

            idlOS::memcpy(&sTSSlotSID, sValuePtr, ID_SIZEOF(sdSID));
            sValuePtr += ID_SIZEOF(sdSID);

            idlOS::memcpy(&sExtRID4TSS, sValuePtr, ID_SIZEOF(sdRID));
            sValuePtr += ID_SIZEOF(sdRID);
            sValuePtr += ID_SIZEOF(smTID);

            idlOS::memcpy(&sTXSegID, sValuePtr, ID_SIZEOF(UInt));

            IDE_ASSERT( sTrans->mTXSegEntry == NULL );
            sdcTXSegMgr::tryAllocEntryByIdx( sTXSegID, &(sTrans->mTXSegEntry) );
            IDE_ASSERT( sTrans->mTXSegEntry != NULL );

            sTrans->setTSSAllocPos( sExtRID4TSS, sTSSlotSID );
            break;

        default:
            break;
    }

    return;

}

 /***********************************************************************
 * Description : LogBuffer���� DRDB�� RedoLog�� �и��ؼ� List�� ������
 *
 * aTransID       - [IN]  Log�� ���� TID
 * aLogBuffer     - [IN]  ���� Log
 * aLogBufferSize - [IN]  ���� Log�� ����ִ� Buffer�� ũ��
 * aBeginLSN      - [IN]  ���� Log�� LSN
 * aEndLSN        - [IN]  ���� Log�� LSN�� ����ġ
 * aLogDataList   - [OUT] Parsing�� ��� ( sdrRedoLogData )
 ***********************************************************************/
IDE_RC sdrRedoMgr::generateRedoLogDataList( smTID             aTransID,
                                            SChar           * aLogBuffer,
                                            UInt              aLogBufferSize,
                                            smLSN           * aBeginLSN,
                                            smLSN           * aEndLSN,
                                            void           ** aLogDataList )
{
    scGRID              sGRID;
    sdrLogType          sLogType;
    SChar             * sLogRec;
    SChar             * sValue;
    UInt                sParseLen   = 0;
    UInt                sValueLen;
    sdrRedoLogData    * sData   = NULL;
    sdrRedoLogData    * sHead   = NULL;
    UInt                sState  = 0;

    IDE_DASSERT( aLogBuffer != NULL );
    IDE_DASSERT( aLogBufferSize > 0 );

    (*aLogDataList) = NULL;

    while (sParseLen < aLogBufferSize)
    {
        sLogRec = aLogBuffer + sParseLen;

        parseRedoLogHdr( sLogRec,
                         &sLogType,
                         &sGRID,
                         &sValueLen );

        if (sValueLen != 0)
        {
            sValue  = sLogRec + ID_SIZEOF(sdrLogHdr);
        }
        else
        {
            sValue = NULL;
        }

        // Redo Skip�� TableSpace ���� üũ�Ѵ�.
        // 1) Restart Recovery
        // DROPPED, DISCARDED, OFFLINE
        // 2) Media Recovery �� ���
        // DROPPED�� DISCARDED

        /* TableSpace�� ���� üũ�̱� ������, RestartRecovery�� �ƴ϶� ��*/
        if ( smLayerCallback::isSkipRedo( SC_MAKE_SPACE(sGRID), ID_FALSE )
             == ID_FALSE )
        {
            /* TC/FIT/Limit/sm/sdr/sdrRedoMgr_addRedoLogToHashTable_malloc2.sql */
            IDU_FIT_POINT_RAISE( "sdrRedoMgr::addRedoLogToHashTable::malloc2",
                                  insufficient_memory );
			
            IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SDR,
                                              ID_SIZEOF(sdrRedoLogData),
                                              (void**)&sData) != IDE_SUCCESS,
                            insufficient_memory );
            sState = 1;

            sData->mType        = sLogType;
            sData->mSpaceID     = SC_MAKE_SPACE(sGRID);
            sData->mPageID      = SC_MAKE_PID(sGRID);
            sData->mValue       = sValue;
            sData->mValueLength = sValueLen;
            sData->mTransID     = aTransID;
            sData->mBeginLSN    = *aBeginLSN;
            sData->mEndLSN      = *aEndLSN;

            if ( SC_GRID_IS_WITH_SLOTNUM(sGRID) )
            {
                sData->mOffset      = SC_NULL_OFFSET;
                sData->mSlotNum     = SC_MAKE_SLOTNUM(sGRID);
            }
            else
            {
                sData->mOffset      = SC_MAKE_OFFSET(sGRID);
                sData->mSlotNum     = SC_NULL_SLOTNUM;
            }

            SMU_LIST_INIT_BASE( &sData->mNode4RedoLog );
            sData->mNode4RedoLog.mData = (void*)sData;

            if ( sHead == NULL )
            {
                sHead           = sData;
                (*aLogDataList) = (void*)sData;
            }
            else
            {
                SMU_LIST_GET_PREV(&sData->mNode4RedoLog) = NULL;
                SMU_LIST_GET_NEXT(&sData->mNode4RedoLog) = NULL;
                SMU_LIST_ADD_LAST(&sHead->mNode4RedoLog, &sData->mNode4RedoLog);
            }
        }
        else
        {
            /*do nothing*/
        }

        sParseLen += ID_SIZEOF(sdrLogHdr) + sValueLen;
    }

    /* Mtx�� �߸� ��ϵǸ� �� ���̰� �޶�����. ���� �߸��� �α��ε�,
     * �׷��ٰ� Assert�� ó���ϸ� ���� ����� ����. */
    IDE_ERROR_MSG( sParseLen == aLogBufferSize,
                   "sParseLen      : %"ID_UINT32_FMT"\n"
                   "aLogBufferSize : %"ID_UINT32_FMT"\n"
                   "mFileNo        : %"ID_UINT32_FMT"\n"
                   "mOffset        : %"ID_UINT32_FMT"\n",
                   sParseLen,
                   aLogBufferSize,
                   aBeginLSN->mFileNo,
                   aBeginLSN->mOffset );  

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sData ) == IDE_SUCCESS );
            sData = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/***********************************************************************
 * PROJ-2162 RestartRiskReduction
 * Description : RedoLogList�� Free��
 *
 * aLogDataList - [IN/OUT] Free�� ���
 ***********************************************************************/
IDE_RC sdrRedoMgr::freeRedoLogDataList( void  * aLogDataList )
{
    sdrRedoLogData   * sData;
    sdrRedoLogData   * sNext;

    sData = (sdrRedoLogData*) aLogDataList;
    while( sData != NULL )
    {
        sNext = (sdrRedoLogData*)SMU_LIST_GET_NEXT( &(sData->mNode4RedoLog) )->mData;
        if ( sNext == sData )
        {
            sNext = NULL;
        }
        SMU_LIST_DELETE( &sData->mNode4RedoLog );
        IDE_TEST( iduMemMgr::free( sData ) != IDE_SUCCESS );
        sData = sNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α׿��� drdb �α׸� �����Ͽ� �Ľ��ϰ� Hasing��
 *
 * PROJ-2162 RestartRiskReduction
 * OnlineREDO�� ���� DRDB Page�� ���� ������ 
 *
 * aStatistics       - [IN]     dummy �������
 * aSpaceID          - [IN]     Redo�� Page�� TablespaceID
 * aPageID           - [IN]     Redo�� Page�� PageID
 * aReadFromDisk     - [IN]     Disk�� File�κ��� Page�� �о �ø� ���ΰ�?
 * aOnlineTBSLSN4Idx - [OUT]    TBS Online/Offline�� Index SMO �� ������ ����
 * aOutPagePtr       - [IN/OUT] aReadFromDisk�� FALSE�� ��� �������� ������ 
 *                              �;��ϰ�, �װ� �ƴϴ��� Align�� Page�� 
 *                              �;� �Ѵ�.
 * aSuccess          - [OUT]    ��������
 ***********************************************************************/
IDE_RC sdrRedoMgr::generatePageUsingOnlineRedo( idvSQL    * aStatistics,
                                                scSpaceID   aSpaceID,
                                                scPageID    aPageID,
                                                idBool      aReadFromDisk,
                                                UChar     * aOutPagePtr,
                                                idBool    * aSuccess )
{
    UInt                     sDummy;
    smLSN                    sCurLSN;
    smLSN                    sLstLSN;
    smLSN                    sLastModifyLSN;
    smrLogFile             * sLogFilePtr = NULL;
    smrLogHead               sLogHead;
    SChar                  * sLogPtr;
    idBool                   sIsValid;
    UInt                     sLogSizeAtDisk = 0;
    smLSN                    sBeginLSN;
    smLSN                    sEndLSN;
    sdrRedoLogData         * sRedoLogDataList;
    iduReusedMemoryHandle  * sDecompBufferHandle;
    SChar                  * sRedoLogBuffer;
    UInt                     sRedoLogSize;
    sdrMtx                   sMtx;
    idBool                   sSuccess = ID_FALSE;
    UInt                     sState = 0;  
    /* PROJ-2102 */
    smLSN                    sCurSecondaryLSN;

    IDE_ERROR( aOutPagePtr != NULL );
    IDE_ERROR( aSuccess    != NULL );

    if ( aReadFromDisk == ID_TRUE )
    {
        IDE_TEST( sddDiskMgr::read( aStatistics,
                                    aSpaceID,
                                    aPageID,
                                    aOutPagePtr,
                                    &sDummy ) // DataFileID
                  != IDE_SUCCESS );

        /* Page�� �ʱ�ȭ���� �ʰų� Inconsistent�ϴٸ� �������� �ʴ´�. */
        IDE_TEST_CONT( ( sdpPhyPage::checkAndSetPageCorrupted( aSpaceID, aOutPagePtr )
                          == ID_TRUE ),
                        SKIP );
    }

    /* sdrRedoMgr_generatePageUsingOnlineRedo_malloc_DecompBufferHandle.tc */
    IDU_FIT_POINT("sdrRedoMgr::generatePageUsingOnlineRedo::malloc::DecompBufferHandle");
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_SM_SDR,
                                 ID_SIZEOF( iduGrowingMemoryHandle ),
                                 (void**)&sDecompBufferHandle,
                                 IDU_MEM_FORCE)
             != IDE_SUCCESS );
    sState = 1;

    /* �α� ���������� ���� �ڵ��� �ʱ�ȭ
     * Hash�Ͽ� ������ ���� �ƴϱ� ������, Reuse�� �ٷιٷ� ����� */
    IDE_TEST( sDecompBufferHandle->initialize( IDU_MEM_SM_SDR )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( sdrMiniTrans::begin( aStatistics, /* idvSQL* */
                                   &sMtx,
                                   (void*)NULL,
                                   SDR_MTX_NOLOGGING,
                                   ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                   SM_DLOG_ATTR_DEFAULT )
              != IDE_SUCCESS );
    sState = 3;

    /* BUG-42785
     * MinRecoveryLSN�� ȹ���� �Ŀ� Checkpoint�� ����� ���
     * MinRecoveryLSN�� ����Ǹ� LogFile�� �������� �ִ�.
     * ���� OnlineDRDBRedo�� ���� ������ �˷�
     * �α������� �������� �ʵ��� �Ͽ��� �Ѵ�. */
    IDE_TEST( smrRecoveryMgr::incOnlineDRDBRedoCnt() != IDE_SUCCESS );
    sState = 4;

    /* MinRecoveryLSN�� Page�� LastModifyLSN �� ū ���� �����Ѵ�.
     * - MinRecoveryLSN < PageLSN : �׳� PageLSN���� ū Log�� �ݿ��Ѵ�.
     * - MinRecoveryLSN > PageLSN :
     *   �� PageLSN�� '�ڽ��� ���Ž�Ų Log�� ���� LSN'�̴�.
     *   �� �� page�� ���� ���� ���� Log�� ����Ű�� ���� �ƴϱ� ������,
     *   MinRecoveryLSN�� �����Ͽ� �ݿ��Ѵ�.
     * ���⼭ PageRecoveryLSN�� �����ϸ� �ȵȴ�. Flush�ϱ� ����
     * IOB�� �ű� ���� ���⼭ Page�� Read�ع����� ������ �߻��� ��
     * �ֱ� �����̴�. */
    sdbBufferMgr::getMinRecoveryLSN( aStatistics,
                                     &sCurLSN );
    /* MinRecoveryLSN�� PBuffer�� Secondary Buffer�� ���� �� ���������� ���� */
    sdsBufferMgr::getMinRecoveryLSN( aStatistics,
                                     &sCurSecondaryLSN );
    if ( smrCompareLSN::isGT( &sCurLSN, &sCurSecondaryLSN ) == ID_TRUE )
    {
        SM_GET_LSN( sCurLSN, sCurSecondaryLSN );
    }

    sLastModifyLSN = smLayerCallback::getPageLSN( (UChar*)aOutPagePtr );
    if ( smrCompareLSN::isLT( &sCurLSN, &sLastModifyLSN ) == ID_TRUE )
    {
        SM_GET_LSN( sCurLSN, sLastModifyLSN );
    }

    smrLogMgr::getLstLSN( &sLstLSN );

    IDU_FIT_POINT( "3.BUG-42785@sdrRedoMgr::generatePageUsingOnlineRedo::wakeup" );
    IDU_FIT_POINT( "4.BUG-42785@sdrRedoMgr::generatePageUsingOnlineRedo::sleep" );

    while( smrCompareLSN::isLT( &sCurLSN , &sLstLSN ) == ID_TRUE )
    {
        sCurLSN.mOffset += sLogSizeAtDisk;
        IDE_TEST( smrLogMgr::readLog( sDecompBufferHandle,
                                      &sCurLSN,
                                      ID_FALSE, /* Close Log File  */
                                      &sLogFilePtr,
                                      &sLogHead,
                                      &sLogPtr,
                                      &sIsValid,         /* Is Valid */
                                      &sLogSizeAtDisk )   /* Log Size */
                 != IDE_SUCCESS );

        if ( sIsValid == ID_FALSE )
        {
            /* Done. */
            break;
        }

        IDE_ASSERT( smrCompareLSN::isLTE(&sLastModifyLSN,
                                         &sCurLSN) == ID_TRUE );

        if ( smrLogMgr::isDiskLogType( sLogHead.mType ) == ID_TRUE )
        {
            SM_GET_LSN(sBeginLSN, sCurLSN);
            SM_GET_LSN(sEndLSN, sCurLSN);
            sEndLSN.mOffset += sLogSizeAtDisk;

            smrRecoveryMgr::getDRDBRedoBuffer( smrLogHeadI::getType(&sLogHead),
                                               smrLogHeadI::getSize(&sLogHead),
                                               sLogPtr,
                                               &sRedoLogSize,
                                               &sRedoLogBuffer );

            /* �Ұ� ���� */
            if ( sRedoLogSize == 0 )
            {
                continue;
            }
            IDE_TEST( generateRedoLogDataList( smrLogHeadI::getTransID(&sLogHead),
                                               sRedoLogBuffer,
                                               sRedoLogSize,
                                               &sBeginLSN,
                                               &sEndLSN,
                                               (void**)&sRedoLogDataList )
                      != IDE_SUCCESS );

            /* Redo�ؾ��� Tablespace�� Drop�Ǵµ� �Ͽ�
             * �ǹ̾��� Log�� �� ����. �ֳ��ϸ� �� Page
             * �� ���õ� �α׸��� �ƴ�, ��� �α׸�
             * �о���� ����. */
            if ( sRedoLogDataList != NULL )
            {
                IDE_TEST( applyListedLogRec( &sMtx,
                                             aSpaceID,
                                             aPageID,
                                             aOutPagePtr,
                                             sEndLSN,
                                             sRedoLogDataList )
                          != IDE_SUCCESS );

                IDE_TEST( freeRedoLogDataList( (void*)sRedoLogDataList ) 
                          != IDE_SUCCESS );
            }
        }
        else
        {
            /* LogFile EndLog�� ���,
             * �Ϲ����� Recovery�� �޸� LogFile�� Ȯ���� �����ϴ� ��쿡
             * �Ҹ���� ���̱� ������, ���� Logfile�� ��� Ȯ���ϴ�
             * smrRecoveryMgr::redo_FILE_END ���� �۾��� �ʿ� ����. */
            if ( sLogHead.mType == SMR_LT_FILE_END )
            {
                sCurLSN.mFileNo++;
                sCurLSN.mOffset = 0;
                sLogSizeAtDisk  = 0;
            }
        }
    }

    /* BUG-42785 OnlineDRDBRedo�� �Ϸ�Ǿ����Ƿ�
     * Count�� �ٿ��ش�. */
    sState = 3;
    IDE_TEST( smrRecoveryMgr::decOnlineDRDBRedoCnt() != IDE_SUCCESS );

    sSuccess = ID_TRUE;

    sState = 2;
    IDE_TEST( sdrMiniTrans::commit(&sMtx ) != IDE_SUCCESS );

    sState = 1;
    IDE_TEST( sDecompBufferHandle->destroy() != IDE_SUCCESS );

    sState = 0;
    IDE_TEST( iduMemMgr::free( sDecompBufferHandle ) != IDE_SUCCESS );

    IDE_EXCEPTION_CONT( SKIP );

    (*aSuccess) = sSuccess;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    ideLog::log( IDE_DUMP_0, 
                 "SpaceID  :%"ID_UINT32_FMT"\n"
                 "PageID   :%"ID_UINT32_FMT"\n",
                 aSpaceID,
                 aPageID );

    switch( sState )
    {
        case 4:
            IDE_ASSERT( smrRecoveryMgr::decOnlineDRDBRedoCnt() == IDE_SUCCESS );
        case 3:
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        case 2:
            IDE_ASSERT( sDecompBufferHandle->destroy() == IDE_SUCCESS );
        case 1:
            (void) iduMemMgr::free( sDecompBufferHandle );
            break;
        default:
            break;
    }

    return IDE_FAILURE;
}

IDE_RC sdrRedoMgr::applyListedLogRec( sdrMtx         * aMtx,
                                      scSpaceID        aSpaceID,
                                      scPageID         aPageID,
                                      UChar          * aPagePtr,
                                      smLSN            aLSN,
                                      sdrRedoLogData * aLogDataList )
{
    sdrRedoLogData * sData;
    sdpPhyPageHdr  * sPhyPageHdr;

    sPhyPageHdr = (sdpPhyPageHdr*)aPagePtr;
    sData       = aLogDataList;
    do // List�� ��ȸ��.
    {
        // �� page�� ���� Log�� ã�Ƽ� ������.
        if ( ( sData->mSpaceID == aSpaceID ) &&
             ( sData->mPageID  == aPageID )  &&
            /* ���ž��� Logging�� �ϴ� ���̱� ������, OnlineRedo������
             * �����ϸ� �ȵ� */
             ( sData->mType != SDR_SDC_SET_INITSCN_TO_TSS ) )
        {
            IDE_TEST( applyLogRec( aMtx,
                                   aPagePtr,
                                   sData )
                      != IDE_SUCCESS );

            SM_GET_LSN( sPhyPageHdr->mFrameHdr.mPageLSN, aLSN );
        }

        sData = (sdrRedoLogData*)SMU_LIST_GET_NEXT( &(sData->mNode4RedoLog) )->mData;
    }
    while( sData != aLogDataList );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �α׿��� drdb �α׸� �����Ͽ� �Ľ��ϰ� Hasing��
 *
 * smrLogMgr���� ȣ��ǰų�, redo �α׸� hash�� �����ϴ� ��������
 * ȣ��Ǳ⵵ �Ѵ�.
 * hash���� bucket�� ���ʴ�� �ǵ��ϸ鼭 bucket�� �ִ� ��� redo �α׵���
 * �ش� tablespace�� page�� �ݿ��Ѵ�. �̶�, mtx�� �̿��Ͽ� no-logging����
 * ó���Ѵ�. ������ ���۰������� flush list�� ��� BCB�� flush ��Ų��.
 *
 * + 2nd. code design
 *   - hash�� traverse �ϱ� ���� open�Ѵ�.
 *   - hash�κ��� ù��° ��带 �߶�´�.
 *   - while( ��尡 NULL�� �ƴҶ����� )
 *     {
 *        - ��� ���¸� recovery ���ۻ��·� �����Ѵ�.
 *        - mtx�� begin�Ѵ�.
 *        - ����� space ID, page ID�� �̿��Ͽ� ���۰����ڷκ��� ��������
 *          ��´�. (X-LATCH)
 *        - for ( redo �α� ����Ÿ ������ŭ )
 *          {
 *              ��尡 ���� redo �α׵���Ÿ  ����Ʈ�� �ϳ��� �������� �ݿ��Ѵ�.
 *              �ݿ��� redo �α׵���Ÿ �� �޸� �����Ѵ�.
 *              ����� redo �α� ����Ÿ ������ ���ҽ�Ų��.
 *          }
 *        - mtx�� commit�Ѵ�.
 *        - hash�κ��� ���� ��带 �߶�´�.
 *        - �̹� �ݿ��� ��带 �޸����� �Ѵ�.
 *        - hash�� ������ ��� ������ ���ҽ�Ų��.
 *     }
 *     - hash�� close�Ѵ�.
 *     - ������ ��� ������ 0���� Ȯ���Ѵ�.
 *     - ���۰������� flush�� ��û�Ѵ�.
 ***********************************************************************/
IDE_RC sdrRedoMgr::applyHashedLogRec(idvSQL * aStatistics)
{

    idBool                      sTry;
    sdrMtx                      sMtx;
    UChar*                      sPagePtr;
    smuList*                    sBase;
    smuList*                    sList;
    smuList*                    sNextList;
    sdrRedoHashNode*            sNode;
    sdrRedoLogData*             sData;
    smLSN                       sLastModifyLSN;
    smLSN                       sMaxApplyLSN;
    smLSN                       sInitLSN;
    UInt                        sState = 0;
    idBool                      sApplied;
    idBool                      sSkipRedo;
    idBool                      sIsOverWriteLog;
    idBool                      sIsCorruptPage;
    UInt                        sHashState;
    smiTableSpaceAttr           sTBSAttr;
    UInt                        sIBChunkID;
    sddTableSpaceNode         * sSpaceNode;
    sddDataFileNode           * sDataFileNode = NULL;
    smiDataFileDescSlotID     * sDataFileDescSlotID;
    smiDataFileDescSlotID     * sAllocDataFileDescSlotID;
    sdFileID                    sDataFileID;
    idBool                      sResult; 

    sPagePtr = NULL;
    sNode    = NULL;
    sData    = NULL;
    sHashState = 0;

    SM_LSN_INIT(sInitLSN);
    SM_LSN_INIT(sLastModifyLSN);

    // Redo Log�� ����� Hash Table Open
    IDE_TEST( smuHash::open(&mHash) != IDE_SUCCESS );
    sHashState = 1;

    IDE_TEST( smuHash::cutNode(&mHash, (void **)&sNode) != IDE_SUCCESS );

    sdrCorruptPageMgr::allowReadCorruptPage();

    while(sNode != NULL)
    {
        SM_LSN_INIT(sMaxApplyLSN);
        sApplied = ID_FALSE;

        if (sNode->mState == SDR_RECV_INVALID)
        {
            IDE_TEST( iduMemMgr::free( sNode ) != IDE_SUCCESS );
            mApplyPageCount--;

            if (getRecvType() != SMI_RECOVER_RESTART)
            {
                mNoApplyPageCnt--;
            }

            IDE_TEST( smuHash::cutNode( &mHash, (void **)&sNode ) != IDE_SUCCESS );

            continue;
        }

        IDE_DASSERT(sNode->mState == SDR_RECV_NOT_START);

        sNode->mState = SDR_RECV_START;

        if (sNode->mRecvItem != NULL)
        {
            sNode->mRecvItem->mState = SDR_RECV_START;
        }

        IDE_TEST( sdrMiniTrans::begin(aStatistics, /* idvSQL* */
                                      &sMtx,
                                      (void*)NULL,
                                      SDR_MTX_NOLOGGING,
                                      ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                      SM_DLOG_ATTR_DEFAULT)
                  != IDE_SUCCESS );
        sState = 1;

        //---------------------
        // Redo Page �� ����
        //---------------------
        sSkipRedo = ID_FALSE;
        sIsOverWriteLog = ID_FALSE;
        sIsCorruptPage = ID_FALSE;

        if ( sdbBufferMgr::getPageByPID(aStatistics,
                                        sNode->mSpaceID,
                                        sNode->mPageID,
                                        SDB_X_LATCH,
                                        SDB_WAIT_NORMAL,
                                        SDB_SINGLE_PAGE_READ,
                                        &sMtx,
                                        &sPagePtr,
                                        &sTry,
                                        &sIsCorruptPage ) != IDE_SUCCESS )
        {
            switch( ideGetErrorCode() )
            {
                case smERR_ABORT_PageCorrupted :

                    IDE_TEST( sdrCorruptPageMgr::addCorruptPage( sNode->mSpaceID,
                                                                 sNode->mPageID )
                              != IDE_SUCCESS );

                case smERR_ABORT_NotFoundTableSpaceNode :
                case smERR_ABORT_NotFoundDataFile :
                    // To fix BUG-14949
                    // ���̺� �����̽��� ���������� ������������
                    // ���� ��찡 �ֱ� ������ �Ʒ��� ���� �ΰ���
                    // �������� �˻��ؾ߸� �Ѵ�.
                    sSkipRedo = ID_TRUE;

                    ideClearError();

                    ideLog::log(SM_TRC_LOG_LEVEL_WARNNING,
                                SM_TRC_DRECOVER_WARNNING1,
                                sNode->mSpaceID);

                    break;
                default :
                    IDE_ASSERT(ID_FALSE); // �ٸ������� �볳���� �ʴ´�.
                    break;
            }
        }
        else
        {
            /* PROJ-2118 Bug Reporting
             * Fatal�� ����ϱ� ���� Redo ��� Page�� ���� */
            mCurSpaceID = sNode->mSpaceID;
            mCurPageID  = sNode->mPageID;
            mCurPagePtr = sPagePtr;
        }

        if ( sIsCorruptPage == ID_TRUE )
        {
            // PROJ-1867
            // Corrupted page�� ���� ����̴�.
            //
            // Page�� �����ؾ� �� ù Redo Log�� Page Image
            // Log�� �ƴ϶�� Corrupted Page Hash�� ����Ѵ�.
            //
            // �ݴ�� Page�� �����ؾ� �� ù Redo Log�� Page
            // Img Log�̸� Corrupted Page Hash �� �ִٸ� �����ϰ�
            // Redo Log�� �����ؼ� Page Image Log�� �����.
            //
            // log hash�������� PILog�� ������ �ش� Page��
            // ���� Redo Log�� hash���� ��� ����� ������
            // Page�� ù Redo Log�� Ȯ���ϸ� �ȴ�.

            sList = SMU_LIST_GET_FIRST( &sNode->mRedoLogList );
            sData = (sdrRedoLogData*)sList->mData;

            if ( sdrCorruptPageMgr::isOverwriteLog( sData->mType )
                 == ID_TRUE )
            {
                // Hash���� log �� Overwrite log�� �ִ�.
                // sIsOverWriteLog�� �����ϰ� ����
                // Corrupt Page Hash�� �ִٸ� �����Ѵ�.

                sctTableSpaceMgr::getTBSAttrByID( aStatistics,
                                                  sNode->mSpaceID,
                                                  &sTBSAttr );
                ideLog::log( SM_TRC_LOG_LEVEL_WARNNING,
                             SM_TRC_DRECOVER_OVERWRITE_CORRUPTED_PAGE,
                             sTBSAttr.mName,
                             sNode->mPageID,
                             sData->mType );

                sIsOverWriteLog = ID_TRUE;

                IDE_TEST( sdrCorruptPageMgr::delCorruptPage( sNode->mSpaceID,
                                                             sNode->mPageID )
                          != IDE_SUCCESS );
            }
            else
            {
                // Hash���� log �� Overwrite log�� ����.
                // Corrupt Page Hash�� �߰��Ѵ�.

                IDE_TEST( sdrCorruptPageMgr::addCorruptPage( sNode->mSpaceID,
                                                             sNode->mPageID )
                          != IDE_SUCCESS );

                sSkipRedo = ID_TRUE;
            }
        }

        if ( sSkipRedo == ID_TRUE )
        {
            IDE_TEST( clearRedoLogList( sNode ) != IDE_SUCCESS );

            sState = 0;
            IDE_TEST( sdrMiniTrans::rollback(&sMtx) != IDE_SUCCESS );

            sNode->mState = SDR_RECV_FINISHED;

            IDE_TEST( iduMemMgr::free( sNode ) != IDE_SUCCESS );
            mApplyPageCount--;

            IDE_TEST( smuHash::cutNode( &mHash, (void **)&sNode )
                      != IDE_SUCCESS );

            continue;
        }
        else
        {
            // redo ����
        }

        //---------------------
        // Redo ����
        //---------------------

        sLastModifyLSN = smLayerCallback::getPageLSN( sPagePtr );
        sBase = &sNode->mRedoLogList;

        for ( sList = SMU_LIST_GET_FIRST(sBase);
              sList != sBase;
              sList = sNextList)
        {
            sData = (sdrRedoLogData*)sList->mData;
            mRedoLogPtr = sData;

            /* ------------------------------------------------
             * Redo : PAGE�� update LSN�� �α��� LSN���� ���� ���
             * ----------------------------------------------*/

            if ( ( smLayerCallback::isLSNLT( &sLastModifyLSN,
                                             &sData->mEndLSN ) == ID_TRUE ) ||
                 ( smLayerCallback::isLSNEQ( &sLastModifyLSN,
                                             &sInitLSN ) == ID_TRUE ) ||
                 ( sIsOverWriteLog == ID_TRUE ) )
            {
                //------------------------------------------------
                //  PAGE�� update LSN�� �α��� LSN���� ���� ���,
                //  Redo ����
                //------------------------------------------------

                sApplied = ID_TRUE;

                IDE_TEST( applyLogRec( &sMtx,
                                       sPagePtr,
                                       sData ) != IDE_SUCCESS );
            }
            else
            {
                // PAGE�� update LSN�� �α��� LSN���� ũ�ų� ���� ���,
                // �̹� �α��� ������ �ݿ��Ǿ������� �ǹ�

                /* PROJ-2162 PageLSN�� �������� �ֽ� UpdateLSN ����
                 * BUG-48275 recovery ���� �߸��� ���� �޽����� ��� �˴ϴ�.
                 * �߰� ���� ��� */
                 if ( smrCompareLSN::isLT( &mLstDRDBPageUpdateLSN,
                                           &sLastModifyLSN ) == ID_TRUE )
                {
                    SM_GET_LSN( mLstDRDBPageUpdateLSN, sLastModifyLSN );
                    mLstUpdatedSpaceID = mCurSpaceID;
                    mLstUpdatedPageID  = mCurPageID;
                }
            }


            /* ------------------------------------------------
             * redo ����(nologging ���)�� page�� �ݿ��� sMaxApplyLSN��
             * mtx�� �����Ѵ�.(force)
             * ----------------------------------------------*/
            SM_SET_LSN(sMaxApplyLSN,
                       sData->mEndLSN.mFileNo,
                       sData->mEndLSN.mOffset);

            sNextList = SMU_LIST_GET_NEXT(sList);

            SMU_LIST_DELETE(sList);

            mRedoLogPtr = NULL;
            IDE_TEST( iduMemMgr::free(sData) != IDE_SUCCESS );
            sNode->mRedoLogCount--;

            if (sNode->mRecvItem != NULL)
            {
                IDE_DASSERT(getRecvType() != SMI_RECOVER_RESTART);

                sNode->mRecvItem->mApplyRedoLogCnt--;
            }
        }

        //PROJ-2133 incremental backup
        if ( ( smrRecoveryMgr::isCTMgrEnabled() == ID_TRUE ) && 
             ( getRecvType() == SMI_RECOVER_RESTART ) )
        {
            IDE_TEST_CONT( sctTableSpaceMgr::isTempTableSpace(sNode->mSpaceID) == ID_TRUE,
                           skip_change_tracking );

            IDE_TEST( sctTableSpaceMgr::findSpaceNodeBySpaceID( 
                                                    sNode->mSpaceID, 
                                                    (void**)&sSpaceNode )
                      != IDE_SUCCESS );

            sDataFileID   = SD_MAKE_FID(sNode->mPageID);
            sDataFileNode = sSpaceNode->mFileNodeArr[ sDataFileID ];

            IDE_ERROR( sDataFileNode != NULL );

            sDataFileDescSlotID = 
                            &sDataFileNode->mDBFileHdr.mDataFileDescSlotID;
        
            IDE_TEST_CONT( smriChangeTrackingMgr::isAllocDataFileDescSlotID( 
                                    sDataFileDescSlotID ) != ID_TRUE,
                            skip_change_tracking );

            IDE_TEST( smriChangeTrackingMgr::isNeedAllocDataFileDescSlot( 
                                                    sDataFileDescSlotID,
                                                    sNode->mSpaceID,
                                                    sDataFileID,
                                                    &sResult )
                  != IDE_SUCCESS );

            if ( sResult == ID_TRUE )
            {
                IDE_TEST( smriChangeTrackingMgr::addDataFile2CTFile( 
                                                  sNode->mSpaceID,
                                                  sDataFileID,
                                                  SMRI_CT_DISK_TBS,
                                                  &sAllocDataFileDescSlotID )
                          != IDE_SUCCESS );

                sddDataFile::setDBFHdr(
                            &(sDataFileNode->mDBFileHdr),
                            NULL, // DiskRedoLSN
                            NULL, // DiskCreateLSN
                            NULL, // MustRedoLSN
                            sAllocDataFileDescSlotID);

                IDE_TEST( smrRecoveryMgr::updateDBFNodeToAnchor( 
                                                        sDataFileNode )
                          != IDE_SUCCESS );
            }
            
            sIBChunkID = smriChangeTrackingMgr::calcIBChunkID4DiskPage(
                                                            sNode->mPageID );

            IDE_TEST( smriChangeTrackingMgr::changeTracking( 
                                                 sDataFileNode,
                                                 NULL, //smmDatabaseFile
                                                 sIBChunkID ) 
                      != IDE_SUCCESS );

            IDE_EXCEPTION_CONT( skip_change_tracking );

        }
        else
        {
            //nothing to do
        }

        mCurSpaceID = SC_NULL_SPACEID;
        mCurPageID  = SM_SPECIAL_PID;
        mCurPagePtr = NULL;

        sState = 0;
        if (sApplied == ID_TRUE )
        {
            IDE_ASSERT( ( smLayerCallback::isLSNLT( &sLastModifyLSN, &sMaxApplyLSN ) == ID_TRUE ) ||
                        ( smLayerCallback::isLSNEQ( &sLastModifyLSN, &sInitLSN ) == ID_TRUE )     ||
                        ( sIsOverWriteLog == ID_TRUE ) );

            IDE_TEST( sdrMiniTrans::commit(&sMtx,
                                           (UInt)0, // SMR_CT_END�� �ǹ�
                                           &sMaxApplyLSN) != IDE_SUCCESS );
        }
        else
        {
            IDE_ASSERT( (!( ( smLayerCallback::isLSNLT( &sLastModifyLSN, &sMaxApplyLSN ) == ID_TRUE ) ||
                            ( smLayerCallback::isLSNEQ( &sLastModifyLSN, &sInitLSN ) == ID_TRUE ) ))  ||
                        ( sIsOverWriteLog == ID_TRUE ) );

            IDE_TEST( sdrMiniTrans::rollback(&sMtx) != IDE_SUCCESS );
        }

        sNode->mState = SDR_RECV_FINISHED;

        IDE_TEST( iduMemMgr::free( sNode ) != IDE_SUCCESS );
        mApplyPageCount--;

        IDE_TEST( smuHash::cutNode(&mHash, (void **)&sNode) != IDE_SUCCESS );
    }

    // Redo Log�� ����� Hash Table Close
    sHashState = 0;
    IDE_TEST( smuHash::close(&mHash) != IDE_SUCCESS );

    IDE_DASSERT( mApplyPageCount == 0 ); // ��� ����Ǿ�� �Ѵ�.

    IDE_TEST( sdsBufferMgr::flushDirtyPagesInCPList( aStatistics,
                                                     ID_TRUE ) // FLUSH ALL
              != IDE_SUCCESS );


    IDE_TEST( sdbBufferMgr::flushDirtyPagesInCPList( aStatistics,
                                                     ID_TRUE ) // FLUSH ALL
              != IDE_SUCCESS );

    // BUG-45598: ����ŸƮ ��Ŀ���� �� �� ���� ������ ó�� ��å�� ������Ƽ�� �°� ����
    sdrCorruptPageMgr::setPageErrPolicyByProp();

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    // PROJ-2118 BUG Reporting - init Debug Info
    mCurSpaceID = SC_NULL_SPACEID;
    mCurPageID  = SM_SPECIAL_PID;
    mCurPagePtr = NULL;
    mRedoLogPtr = NULL;

    IDE_PUSH();
    {
        if ( sState != 0 )
        {
            IDE_ASSERT( sdrMiniTrans::rollback(&sMtx) == IDE_SUCCESS );
        }

        if ( sHashState != 0 )
        {
            IDE_ASSERT( smuHash::close(&mHash) == IDE_SUCCESS );
        }
    }
    IDE_POP();

    sdrCorruptPageMgr::fatalReadCorruptPage();

    return IDE_FAILURE;

}

/***********************************************************************
 *
 * Description : redo �α��� parsing
 *
 * ��ȿ�� �α����� �˻��ϰ�, disk �α��� Ÿ��, RID, value���̸� ��ȯ�Ѵ�.
 *
 * aStatistics - [IN]  �������
 * aLogRec     - [IN]  �α׷��ڵ� ������
 * aLogType    - [OUT] �α�Ÿ��
 * aLogGRID    - [OUT] ����� �������� GRID
 * aValueLen   - [OUT] �α׽����� ����
 *
 ***********************************************************************/
void sdrRedoMgr::parseRedoLogHdr( SChar      * aLogRec,
                                  sdrLogType * aLogType,
                                  scGRID     * aLogGRID,
                                  UInt       * aValueLen )
{
    sdrLogHdr sLogHdr;

    IDE_DASSERT( aLogRec   != NULL );
    IDE_DASSERT( aLogType  != NULL );
    IDE_DASSERT( aLogGRID  != NULL );
    IDE_DASSERT( aValueLen != NULL );

    /* �αװ� align �Ǿ� ��ϵ��� �ʾұ� ������ memcpy�� �̿� */
    idlOS::memcpy(&sLogHdr, aLogRec, ID_SIZEOF(sdrLogHdr));

    IDE_ASSERT( validateLogRec( &sLogHdr ) == ID_TRUE );

    *aLogType    = sLogHdr.mType;
    *aLogGRID    = sLogHdr.mGRID;
    *aValueLen   = sLogHdr.mLength;
}

/***********************************************************************
 *
 * Description : ��ũ �α� ������ ��ȿ�� ����
 *
 * aLogHdr - [IN] ��ũ �α� ���� ���
 *
 ***********************************************************************/
idBool sdrRedoMgr::validateLogRec( sdrLogHdr * aLogHdr )
{
    idBool      sIsValid = ID_FALSE;

    IDE_TEST_CONT(
        sctTableSpaceMgr::isSystemMemTableSpace(
            SC_MAKE_SPACE(aLogHdr->mGRID) ) == ID_TRUE,
            ERR_INVALID_LOGREC );

    if ( SC_GRID_IS_WITH_SLOTNUM(aLogHdr->mGRID) )
    {
        IDE_TEST_CONT( SC_MAKE_SLOTNUM(aLogHdr->mGRID) >
                            (SD_PAGE_SIZE / ID_SIZEOF(sdpSlotEntry)),
                        ERR_INVALID_LOGREC );
    }
    else /* OFFSET ���� ���� GRID�� �� */
    {
        IDE_TEST_CONT( SC_MAKE_OFFSET(aLogHdr->mGRID) > SD_PAGE_SIZE,
                        ERR_INVALID_LOGREC );
    }

    switch( aLogHdr->mType )
    {
        case SDR_SDP_1BYTE :
        case SDR_SDP_2BYTE :
        case SDR_SDP_4BYTE :
        case SDR_SDP_8BYTE :
        case SDR_SDP_BINARY:
        case SDR_SDP_PAGE_CONSISTENT :
        case SDR_SDP_INIT_PHYSICAL_PAGE :
        case SDR_SDP_INIT_LOGICAL_HDR :
        case SDR_SDP_INIT_SLOT_DIRECTORY :
        case SDR_SDP_FREE_SLOT:
        case SDR_SDP_FREE_SLOT_FOR_SID:
        case SDR_SDP_RESTORE_FREESPACE_CREDIT:
        case SDR_SDP_RESET_PAGE :
        case SDR_SDP_WRITE_PAGEIMG :
        case SDR_SDP_WRITE_DPATH_INS_PAGE : // PROJ-1665

        case SDR_SDPST_INIT_SEGHDR:
        case SDR_SDPST_INIT_BMP:
        case SDR_SDPST_INIT_LFBMP:
        case SDR_SDPST_INIT_EXTDIR:
        case SDR_SDPST_ADD_RANGESLOT:
        case SDR_SDPST_ADD_SLOTS:
        case SDR_SDPST_ADD_EXTDESC:
        case SDR_SDPST_ADD_EXT_TO_SEGHDR:
        case SDR_SDPST_UPDATE_WM:
        case SDR_SDPST_UPDATE_MFNL:
        case SDR_SDPST_UPDATE_LFBMP_4DPATH:
        case SDR_SDPST_UPDATE_PBS:

        case SDR_SDPSC_INIT_SEGHDR :
        case SDR_SDPSC_INIT_EXTDIR :
        case SDR_SDPSC_ADD_EXTDESC_TO_EXTDIR :

        case SDR_SDPTB_INIT_LGHDR_PAGE :
        case SDR_SDPTB_ALLOC_IN_LG:
        case SDR_SDPTB_FREE_IN_LG :

        case SDR_SDC_INSERT_ROW_PIECE :
        case SDR_SDC_INSERT_ROW_PIECE_FOR_UPDATE :
        case SDR_SDC_INSERT_ROW_PIECE_FOR_DELETEUNDO :
        case SDR_SDC_UPDATE_ROW_PIECE :
        case SDR_SDC_OVERWRITE_ROW_PIECE :
        case SDR_SDC_CHANGE_ROW_PIECE_LINK :
        case SDR_SDC_DELETE_FIRST_COLUMN_PIECE :
        case SDR_SDC_ADD_FIRST_COLUMN_PIECE :
        case SDR_SDC_DELETE_ROW_PIECE_FOR_UPDATE :
        case SDR_SDC_DELETE_ROW_PIECE :
        case SDR_SDC_LOCK_ROW :
        case SDR_SDC_PK_LOG:

        case SDR_SDC_INIT_CTL :
        case SDR_SDC_EXTEND_CTL :
        case SDR_SDC_BIND_CTS :
        case SDR_SDC_UNBIND_CTS :
        case SDR_SDC_BIND_ROW :
        case SDR_SDC_UNBIND_ROW :
        case SDR_SDC_ROW_TIMESTAMPING:
        case SDR_SDC_DATA_SELFAGING:

        case SDR_SDC_BIND_TSS :
        case SDR_SDC_UNBIND_TSS :
        case SDR_SDC_SET_INITSCN_TO_TSS:
        case SDR_SDC_INIT_TSS_PAGE :
        case SDR_SDC_INIT_UNDO_PAGE :
        case SDR_SDC_INSERT_UNDO_REC :

        case SDR_SDN_INSERT_INDEX_KEY :
        case SDR_SDN_FREE_INDEX_KEY :
        case SDR_SDN_INSERT_UNIQUE_KEY :
        case SDR_SDN_INSERT_DUP_KEY :
        case SDR_SDN_DELETE_KEY_WITH_NTA :
        case SDR_SDN_FREE_KEYS :
        case SDR_SDN_COMPACT_INDEX_PAGE :
        case SDR_SDN_KEY_STAMPING :
        case SDR_SDN_INIT_CTL :
        case SDR_SDN_EXTEND_CTL :
        case SDR_SDN_FREE_CTS :

        /*
         * PROJ-2047 Strengthening LOB
         */
        case SDR_SDC_LOB_UPDATE_LOBDESC:
        case SDR_SDC_LOB_INSERT_INTERNAL_KEY:
        case SDR_SDC_LOB_INSERT_LEAF_KEY:
        case SDR_SDC_LOB_UPDATE_LEAF_KEY:
        case SDR_SDC_LOB_OVERWRITE_LEAF_KEY:
        case SDR_SDC_LOB_FREE_INTERNAL_KEY:
        case SDR_SDC_LOB_FREE_LEAF_KEY:
        case SDR_SDC_LOB_WRITE_PIECE:
        case SDR_SDC_LOB_WRITE_PIECE4DML:
        case SDR_SDC_LOB_WRITE_PIECE_PREV:
        case SDR_SDC_LOB_ADD_PAGE_TO_AGINGLIST:

        /*
         * PROJ-1591: Disk Spatial Index
         */
        case SDR_STNDR_INSERT_INDEX_KEY :
        case SDR_STNDR_UPDATE_INDEX_KEY :
        case SDR_STNDR_FREE_INDEX_KEY :
        case SDR_STNDR_INSERT_KEY :
        case SDR_STNDR_DELETE_KEY_WITH_NTA :
        case SDR_STNDR_FREE_KEYS :
        case SDR_STNDR_COMPACT_INDEX_PAGE :
        case SDR_STNDR_KEY_STAMPING :

            sIsValid = ID_TRUE;
            break;

        default:
            break;
    }

    IDE_EXCEPTION_CONT( ERR_INVALID_LOGREC );

    return sIsValid;
}


/***********************************************************************
 *
 * Description : hash�� ����� �α׵��� ��� db�� �ݿ�
 *
 * ��ȿ�� �α� Ÿ������ �˻��ϰ�, mtx�� page offset�� NULL�� �ƴϸ�
 * �ش� �α� Ÿ���� redo �Լ��� ȣ���Ͽ� �α��� redo�� �����Ѵ�.
 *
 * aStatistics - [IN] statistics
 * aMtx        - [IN] Mtx ������
 * aPagePtr    - [IN] ������ ������
 * aData       - [IN] sdrRedoLogData ������
 *
 ***********************************************************************/
IDE_RC sdrRedoMgr::applyLogRec( sdrMtx         * aMtx,
                                UChar          * aPagePtr,
                                sdrRedoLogData * aLogData )
{
    sdrRedoInfo  sRedoInfo;
    smrRTOI      sRTOI;
    idBool       sConsistency = ID_FALSE;

    IDE_ASSERT( aMtx      != NULL );
    IDE_ASSERT( aPagePtr  != NULL );
    IDE_ASSERT( aLogData  != NULL );

    sRedoInfo.mLogType = aLogData->mType;

    // PROJ-1705
    // DATA PAGE�� SID(Slot ID)�� ����ϱ� ������ mSlotNum �ʵ带 ����Ѵ�.
    sRedoInfo.mSlotNum = aLogData->mSlotNum;

    smrRecoveryMgr::prepareRTOI( NULL, /*smrLog */
                                 NULL, /*smrLogHead*/
                                 &aLogData->mBeginLSN,
                                 aLogData,
                                 aPagePtr, 
                                 ID_TRUE, /* aIsRedo */
                                 &sRTOI );

    smrRecoveryMgr::checkObjectConsistency( &sRTOI, 
                                            &sConsistency );

    /* Redo �ص� �Ǵ� �ٸ� ��ü�̴� */
    if ( sConsistency == ID_TRUE )
    {
        if ( sdrUpdate::doRedoFunction( aLogData->mValue,
                                        aLogData->mValueLength,
                                        aPagePtr + aLogData->mOffset,
                                        &sRedoInfo,
                                        aMtx )
            != IDE_SUCCESS )
        {
            sConsistency = ID_FALSE;
        }
    }

    /* � ����������, Redo�� �����ߴ�. */
    if ( sConsistency == ID_FALSE )
    {
        IDE_TEST( smrRecoveryMgr::startupFailure( &sRTOI,
                                                  ID_TRUE ) // isRedo
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : �ؽ����̺� redo log�� ����
 *
 * parsing�� hash�� space ID,page ID ���� �ؽ����̺� �����Ѵ�.
 * redo �α׸� �����ϴٺ��� ���õ� page������ ���۰�������
 * MaxPageCount �� �����ϸ� �̹� hash�� ����� redo �α׵��� ���
 * DB �������� �ݿ��Ͽ�, �ٸ� page�� fix �� �� �ֵ��� �Ѵ�.
 *
 * + 2nd. code design
 *   - Hash���� �ش� rid�� ���� ��带 �˻��Ѵ�.
 *   - if ( ��带 �˻����� ���ϸ� )
 *     {
 *        - ���� ������ ��尡 max page count�� �����ϴٸ�
 *          ���ݱ��� ����� redo �α׵��� ��� db �������� �����Ű��
 *          hash�� �ʱ�ȭ�Ѵ�.
 *        - sdrRedoHashNodeŸ���� ��带 �Ҵ��Ѵ�.
 *        - �����¸� �����Ѵ�.
 *        - ����� space id�� �����Ѵ�.
 *        - ����� page id�� �����Ѵ�.
 *        - ����� redo �α� ����Ʈ�� �ʱ�ȭ�Ѵ�.
 *        - hash�� ��带 �߰��Ѵ�.
 *        - ������ ��� ������ ������Ų��.
 *        - media recovery ������ ����
 *          2�� hash���� �������ϳ�带 �˻��Ѵ�.
 *          : �����Ѵٸ�, 1��hash��忡 2�� hash��� �����͸� �����Ѵ�.
 *          : �������� �ʴ´ٸ� �α׸� �Ŵ��� �ʴ´�.
 *     }
 *     else
 *     {
 *        - �˻��� ����� space id�� page id�� ��Ȯ���� �˻��Ѵ�.
 *     }
 *
 *   - 2�� hash��尡 �����Ѵٸ�, �ش� �αװ� �����������
 *     �˻��Ѵ�. ==> filterRecvRedoLog()
 *   - RECVŸ���� RESTART �̰ų� filter�� ����� �α׿� ���ؼ��� ����
 *     �۾��� ��� �����Ѵ�.
 *   - sdrRedoLogDataŸ���� ����Ÿ��带 �Ҵ��Ѵ�.
 *   - ����Ÿ��忡 redo �α� Ÿ���� �����Ѵ�.
 *   - ����Ÿ��忡 offset�� �����Ѵ�.
 *   - ����Ÿ��忡 value�� �����Ѵ�.
 *   - ����Ÿ��忡 value ���̸� �����Ѵ�.
 *   - ����� redo �α� ����Ʈ�� ���� ����Ÿ ��带 �߰��Ѵ�.
 ***********************************************************************/
IDE_RC sdrRedoMgr::addRedoLogToHashTable( void * aLogDataList )
{

    UInt                 sState = 0;
    sdrRedoHashNode*     sNode;
    sdrRedoLogData*      sData;
    sdrRedoLogData*      sNext;
    scGRID               sGRID;
    idBool               sIsRedo;
    scGRID               sRecvFileKey;
    sdFileID             sFileID;
    sdrRecvFileHashNode* sRecvFileNode;

    sNode   = NULL;
    sData   = (sdrRedoLogData*) aLogDataList;
    sIsRedo = ID_FALSE;

    while( sData != NULL )
    {
        sState = 0;
        /* ���� Node�� ����, ���� Node�� ��� */
        sNext = (sdrRedoLogData*)SMU_LIST_GET_NEXT( 
                                   &(sData->mNode4RedoLog) )->mData;
        /* circurlarList�̱� ������, �����ΰ� �����θ� ����Ű�� List��
         * �������̶�� ��. */
        if ( sNext == sData )
        {
            sNext = NULL;
        }

        SMU_LIST_DELETE( &sData->mNode4RedoLog );

        /* Hash Table���� SpaceID�� PageID���� key�� ����ϹǷ� offset��
         * NULL�� �����Ѵ�. */
        SC_MAKE_GRID( sGRID, sData->mSpaceID, sData->mPageID, SC_NULL_OFFSET );
        IDE_DASSERT( sData->mType >= SDR_SDP_1BYTE ||
                     sData->mType <= SDR_SDC_INSERT_UNDO_REC );
        IDE_DASSERT( SC_GRID_IS_NOT_NULL(sGRID) );


        IDE_TEST( smuHash::findNode(&mHash,
                                    &sGRID,
                                    (void **)&sNode) != IDE_SUCCESS );

        if (sNode == NULL) // not found
        {
            /* FIT/Limit/sm/sdr/sdrRedoMgr_addRedoLogToHashTable_malloc1.sql */
            IDU_FIT_POINT_RAISE( "sdrRedoMgr::addRedoLogToHashTable::malloc1",
                                  insufficient_memory );

            IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SDR,
                                              ID_SIZEOF(sdrRedoHashNode),
                                              (void**)&sNode) != IDE_SUCCESS,
                            insufficient_memory );
            sState = 1;
            idlOS::memset(sNode, 0x00, ID_SIZEOF(sdrRedoHashNode));

            sNode->mState = SDR_RECV_NOT_START;

            sNode->mSpaceID = SC_MAKE_SPACE(sGRID);
            sNode->mPageID  = SC_MAKE_PID(sGRID);

            SMU_LIST_INIT_BASE(&sNode->mRedoLogList);
            sNode->mRedoLogCount = 0;
            sNode->mRecvItem     = NULL;


            /* ------------------------------------------------
             * MEDIA RECOVERY ���� �ش� ���������� 2�� Hash����
             * �˻��Ͽ� �������� ������, 1�� HashNode�� 2�� HashNode
             * �����͸� NULL�� �ʱ�ȭ�ϰ� �����Ѵٸ� 2�� HashNode
             * �����͸� ������ ��, 2�� Hash����� ���� �����ȿ�
             * �ش� �αװ� ���ԵǴ����� �Ǵ��Ѵ�.
             * ----------------------------------------------*/
            if (getRecvType() != SMI_RECOVER_RESTART)
            {
                sFileID = SD_MAKE_FID( sNode->mPageID );

                SC_MAKE_GRID(sRecvFileKey, sNode->mSpaceID, sFileID, 0);

                // 2�� hash���� �ش� �αװ� ������� �������� �˻�
                IDE_TEST( smuHash::findNode(&mRecvFileHash,
                                            &sRecvFileKey,
                                            (void **)&sRecvFileNode) != IDE_SUCCESS );

                if (sRecvFileNode != NULL)
                {
                    IDE_ASSERT(sRecvFileNode->mFileID == sFileID);

                    if ( (!(sRecvFileNode->mFstPageID <= sNode->mPageID) &&
                           (sRecvFileNode->mLstPageID >= sNode->mPageID)) )
                    {

                        ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DRECOVER_REDO_LOG_HASHING_FATAL1,
                                    sRecvFileNode->mFileNode->mName,
                                    sNode->mSpaceID,
                                    sNode->mPageID);

                        ideLog::log(SM_TRC_LOG_LEVEL_FATAL,
                                    SM_TRC_DRECOVER_REDO_LOG_HASHING_FATAL2,
                                    sRecvFileNode->mFstPageID,
                                    sRecvFileNode->mLstPageID);

                        IDE_ASSERT(0);
                    }

                    sNode->mRecvItem = sRecvFileNode;
                }
                else
                {
                    sNode->mState = SDR_RECV_INVALID;
                }
            }
        }
        else
        {
            IDE_DASSERT( SC_MAKE_SPACE(sGRID) == sNode->mSpaceID );
            IDE_DASSERT( SC_MAKE_PID(sGRID) == sNode->mPageID );

            // PROJ-1867 Page Img Log�� Page Init Log�� ���
            // ������ Log�� �ʿ� �����Ƿ� �����Ѵ�.
            if ( sdrCorruptPageMgr::isOverwriteLog( sData->mType )
                == ID_TRUE )
            {
                IDE_TEST( clearRedoLogList( sNode ) != IDE_SUCCESS );
            }
        }

        if ( sNode->mRecvItem != NULL )
        {
            IDE_DASSERT( getRecvType() != SMI_RECOVER_RESTART );

            (void)filterRecvRedoLog( sNode->mRecvItem,
                                     &sData->mBeginLSN,
                                     &sIsRedo );

            // To fix BUG-14883
            if ( sIsRedo == ID_TRUE )
            {
                sNode->mRecvItem->mApplyRedoLogCnt++;
            }
        }

        if ( ( getRecvType() == SMI_RECOVER_RESTART ) ||
             ( (getRecvType() != SMI_RECOVER_RESTART) && (sIsRedo == ID_TRUE ) ) )
        {
            /* BUG-40107 Media Reocvery �� Recv LSN ������ �����ؾ� �ϱ� ������ Node�� �����Ǿ����� 
             * �ش� �αװ� Skip�Ǿ� Log List�� �αװ� ���� FATAL �߻��ϴ� ��찡 �ֽ��ϴ�.
             * Node�� �������� ��� �ش� �α��� Skip���θ� �Ǻ��� �� insert �ϵ��� �����մϴ� */
            if ( sState != 0 )
            {
                IDE_TEST( smuHash::insertNode( &mHash,
                                               &sGRID,
                                               sNode ) != IDE_SUCCESS );
                mApplyPageCount++;
                if ( sNode->mState == SDR_RECV_INVALID )
                {
                    mNoApplyPageCnt++;
                }

            }

            SMU_LIST_GET_PREV(&sData->mNode4RedoLog) = NULL;
            SMU_LIST_GET_NEXT(&sData->mNode4RedoLog) = NULL;
            sData->mNode4RedoLog.mData = (void*)sData;

            SMU_LIST_ADD_LAST(&sNode->mRedoLogList, &sData->mNode4RedoLog);
            sNode->mRedoLogCount++;
        }
        else
        {
            /* BUG-40107 Node�� �����Ͽ����� Skip��� Log�� ��� 
             * ������ Node�� ������ �ֵ��� �����մϴ�. */
            if ( sState != 0 )
            {
                IDE_TEST( iduMemMgr::free( sNode ) != IDE_SUCCESS );
                sState = 0;
                sNode  = NULL;
            }
        }

        sData = sNext;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    if (sState != 0)
    {
        (void)iduMemMgr::free(sNode);
    }

    return IDE_FAILURE;

}

/***********************************************************************
 * Description : redoHashNode�� RedoLogList���� log data���� ��� �����Ѵ�.
 *
 * aRedoHashNode - [IN] RedoLogList���� log data���� ������ RedoHashNode
 **********************************************************************/
IDE_RC sdrRedoMgr::clearRedoLogList( sdrRedoHashNode*  aRedoHashNode )
{
    smuList*        sBase;
    smuList*        sList;
    smuList*        sNextList;
    sdrRedoLogData* sData;

    IDE_ASSERT( aRedoHashNode != NULL );

    sBase = &aRedoHashNode->mRedoLogList;

    for ( sList = SMU_LIST_GET_FIRST(sBase);
          sList != sBase;
          sList = sNextList)
    {
        sData = (sdrRedoLogData*)sList->mData;

        sNextList = SMU_LIST_GET_NEXT(sList);

        SMU_LIST_DELETE(sList);
        IDE_TEST( iduMemMgr::free(sData) != IDE_SUCCESS );
        aRedoHashNode->mRedoLogCount--;

        if ( aRedoHashNode->mRecvItem != NULL )
        {
            IDE_DASSERT(getRecvType() != SMI_RECOVER_RESTART);
            aRedoHashNode->mRecvItem->mApplyRedoLogCnt--;
        }
    }
    IDE_ASSERT( aRedoHashNode->mRedoLogCount == 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ���� hash��忡 �ش��ϴ� redo �α����� �˻�
 ***********************************************************************/
IDE_RC sdrRedoMgr::filterRecvRedoLog(sdrRecvFileHashNode* aHashNode,
                                     smLSN*               aBeginLSN,
                                     idBool*              aIsRedo)
{

    IDE_DASSERT(aHashNode != NULL);
    IDE_DASSERT(aBeginLSN != NULL);
    IDE_DASSERT(aIsRedo   != NULL);
    IDE_DASSERT(getRecvType() != SMI_RECOVER_RESTART);

    *aIsRedo = ID_FALSE;

    if ( smLayerCallback::isLSNGTE( &aHashNode->mToLSN, aBeginLSN )
         == ID_TRUE )
    {
        if ( smLayerCallback::isLSNGTE( aBeginLSN, &aHashNode->mFromLSN )
             == ID_TRUE )
        {
            *aIsRedo = ID_TRUE;
        }
    }

    return IDE_SUCCESS;

}

/*
   ������ ��� �����ϰ� hash�κ��� RecvFileHashNode�� ���
   �����Ѵ�.

   [IN] aResetLogsLSN - �ҿ��������� ��������� ������ ResetLogsLSN
*/
IDE_RC sdrRedoMgr::repairFailureDBFHdr( smLSN*    aResetLogsLSN )
{
    SChar                 sMsgBuf[ SM_MAX_FILE_NAME ];
    sdrRecvFileHashNode*  sHashNode;

    IDE_ASSERT( getRecvType() != SMI_RECOVER_RESTART );

    IDE_TEST( smuHash::open(&mRecvFileHash) != IDE_SUCCESS );

    IDE_TEST( smuHash::cutNode( &mRecvFileHash,
                                (void **)&sHashNode )
              != IDE_SUCCESS );

    while ( sHashNode != NULL )
    {
        IDE_DASSERT( sHashNode->mApplyRedoLogCnt == 0 );

        IDE_ASSERT( sHashNode->mState != SDR_RECV_FINISHED );

        if ( aResetLogsLSN != NULL)
        {
            IDE_ASSERT( getRecvType() != SMI_RECOVER_COMPLETE );

            IDE_ASSERT( (sHashNode->mToLSN.mFileNo == ID_UINT_MAX) &&
                        (sHashNode->mToLSN.mOffset == ID_UINT_MAX) );

            IDE_ASSERT( (aResetLogsLSN->mFileNo != ID_UINT_MAX) &&
                        (aResetLogsLSN->mOffset != ID_UINT_MAX) );

            SM_GET_LSN( sHashNode->mFileNode->mDBFileHdr.mRedoLSN,
                        *aResetLogsLSN );
        }

        IDE_TEST( sddDiskMgr::writeDBFHdr( NULL, /* idvSQL* */
                                           sHashNode->mFileNode )
                != IDE_SUCCESS );

        idlOS::snprintf( sMsgBuf,
                SM_MAX_FILE_NAME,
                "  \t\tWriting to ..[%s.DBF<ID:%"ID_UINT32_FMT">]",
                sHashNode->mSpaceName,
                sHashNode->mFileID );

        IDE_CALLBACK_SEND_MSG(sMsgBuf);

        sHashNode->mState = SDR_RECV_FINISHED;

        IDE_TEST( iduMemMgr::free( sHashNode ) != IDE_SUCCESS );
        mRecvFileCnt--;

        IDE_TEST( smuHash::cutNode(&mRecvFileHash, (void **)&sHashNode)
                 != IDE_SUCCESS );
    }

    IDE_TEST( smuHash::close(&mRecvFileHash) != IDE_SUCCESS );

    IDE_DASSERT( mRecvFileCnt == 0 ); // ��� ����Ǿ�� �Ѵ�.

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
   ������ hash�κ��� RecvFileHashNode�� ��� �����Ѵ�.
*/
IDE_RC sdrRedoMgr::removeAllRecvDBFHashNodes()
{
    sdrRecvFileHashNode*  sHashNode;

    IDE_ASSERT( getRecvType() != SMI_RECOVER_RESTART );

    IDE_TEST( smuHash::open(&mRecvFileHash) != IDE_SUCCESS );

    IDE_TEST( smuHash::cutNode( &mRecvFileHash,
                                (void **)&sHashNode )
              != IDE_SUCCESS );

    while ( sHashNode != NULL )
    {
        IDE_TEST( iduMemMgr::free( sHashNode ) != IDE_SUCCESS );
        mRecvFileCnt--;

        IDE_TEST( smuHash::cutNode( &mRecvFileHash, (void **)&sHashNode )
                  != IDE_SUCCESS );
    }

    IDE_TEST( smuHash::close(&mRecvFileHash) != IDE_SUCCESS );

    // ��� ����Ǿ�� �Ѵ�.
    IDE_DASSERT( mRecvFileCnt == 0 );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : ������ datafile�� �м��Ͽ� RecvFileHash(2�� �ؽ�)�� ����
 *
 * + ����
 *   - ���ϸ� ���ؼ� ��ũ �����ڷκ��� DBF��带 ���Ѵ�.
 *     : �ش� ���ϸ��� �������� �ʴ´ٸ� ������� ������ �ƴϴ�.
 *       -> IDE_FAILURE ��ȯ (smERR_ABORT_NoRecvDataFile)
 *
 *   - �˻��� spaceID�� ������ ���ϸ� ���� ��ȿ�� �˻縦 ����
 *     : ��ȿ���� ������, ������� ������ �ƴϴ�.
 *       -> IDE_FAILURE ��ȯ (smERR_ABORT_NoRecvDataFile)
 *
 *   - �ش� ������ file Header�� smVersion�� ���ϰ�,
 *     ��������� �ǵ��Ѵ�.
 *     : ������ �ٸ��ٸ�
 *       -> IDE_FAILURE ��ȯ (smERR_ABORT_NoRecvDataFile)
 *
 *   - �ǵ��� ��������� DBF���� ���� ���������� �����Ѵ�.
 *     : DBF ����� OldestLSN�� ��������� OldestLSN���� ũ�ų� ���ٸ�
 *       ������ �ʿ䰡 ���� �����̸�, SKIP�Ѵ�.
 *       -> ����������Ͼƴ� ������, IDE_SUCCESS ��ȯ
 *
 *     : ������ ���λ����� ���� createLSN���� DBF�����
 *       OldestLSN������ ���� ������ �����ϰ�, �׷��� ���� �����,
 *       DBF����� OldestLSN���� ���������� �����Ѵ�.
 *
 *   - ���� ������ �ִ� �ּ� LSN�� ���Ѵ�.
 *     : �ѹ��� �α׽�ĵ���� ��� ������� ������ �����ϱ� ���ؼ��̴�.
 *
 *   aDBFileHdr   - [IN]  DBFile ���� ���� DBFileHeader
 *   aFileName    - [IN]  DBFile��
 *   aRedoFromLSN - [OUT] media recovery redo start LSN
 *   aRedoToLSN   - [OUT] media recovery redo end LSN
 **********************************************************************/
IDE_RC sdrRedoMgr::addRecvFileToHash( sddDataFileHdr*     aDBFileHdr,
                                      SChar*              aFileName,
                                      smLSN*              aRedoFromLSN,
                                      smLSN*              aRedoToLSN )
{
    UInt                  sState = 0;
    scSpaceID             sSpaceID;
    smLSN                 sInitLSN;
    smLSN                 sFromLSN;
    smLSN                 sToLSN;
    scPageID              sFstPageID = SC_NULL_PID;
    scPageID              sLstPageID = SC_NULL_PID;
    scGRID                sHashKey;
    sddDataFileNode*      sFileNode;
    sdrRecvFileHashNode*  sHashNode;
    SChar*                sSpaceName = NULL;

    IDE_DASSERT(aDBFileHdr    != NULL);
    IDE_DASSERT(aFileName     != NULL);
    IDE_DASSERT(aRedoToLSN    != NULL);
    IDE_DASSERT(aRedoFromLSN  != NULL);
    IDE_DASSERT(getRecvType() != SMI_RECOVER_RESTART);

    SM_LSN_INIT( sInitLSN );

    // [0] ���� ��ο� ����Ÿ������ �̹� �����ϴ��� �˻�
    IDE_TEST_RAISE( idf::access( aFileName, F_OK ) != 0,
                    err_does_not_exist_datafile );


    // [1] �� �Լ� ȣ�������� aFileName�� ���� �̹� ������
    // �����Ѵٴ� ���� Ȯ���Ͽ���, ���������� �����Ѵ�.
    // ����Ÿ������ ������ ����... ���� ���Ѵ�.

    sctTableSpaceMgr::getDataFileNodeByName( aFileName,
                                             &sFileNode,
                                             &sSpaceID,
                                             &sFstPageID,
                                             &sLstPageID,
                                             &sSpaceName );

    IDE_ASSERT( sFileNode != NULL );
    IDE_DASSERT( sFileNode->mSpaceID == sSpaceID );

    // BUG-24250   PRJ-1867/MustRedoToLSN.sql Diff
    // ���� Ȥ�� �ҿ��� ���� ��, FileNode�� FileHdr�� DBFile��
    // FileHdr�� ����� DBFile�� MustRedoToLSN�� �о������ �˴ϴ�.
    // MustRedoToLSN�� �����ϱ� ���� DBFile�� MustRedoToLSN��
    // DBFileNode�� �����صε��� �մϴ�.
    sFileNode->mDBFileHdr.mMustRedoToLSN = aDBFileHdr->mMustRedoToLSN;

    // [2] TO REDO LSN �����ϱ�
    if ( mRecvType == SMI_RECOVER_COMPLETE )
    {
        // loganchor�� DBFileHdr�� DiskRedoLSN
        sToLSN = sFileNode->mDBFileHdr.mRedoLSN;

        // BUG-24250
        // MustRedoToLSN ������ ���� ������ Restart Recovery���� �����մϴ�.
        // �������������� LogAnchor�� RedoLSN������ �����ϸ� �ǹǷ�
        // MustRodoToLSN�� ToLSN�� �ݿ��ϴ� �ڵ带 �����մϴ�.
    }
    else
    {
        // �ҿ��� �����ϰ�� �Ҽ� �ִµ�����
        // �̵�� ������ �����Ѵ�.
        IDE_ASSERT((mRecvType == SMI_RECOVER_UNTILCANCEL) ||
                   (mRecvType == SMI_RECOVER_UNTILTIME));
        SM_LSN_MAX(sToLSN);
    }

    /*
      [3] FROM REDOLSN �����ϱ�
      ���� ��������� REDOLSN�� INITLSN�̶�� EMPTY
      ����Ÿ�����̴�.
    */
    if ( smLayerCallback::isLSNEQ( &aDBFileHdr->mRedoLSN,
                                   &sInitLSN ) == ID_TRUE )
    {
        // �ҿ������� �䱸�� EMPTY ������ �����Ѵٸ�
        // ���������� �����Ͽ����ϱ� ������ ����ó���Ѵ�.
        IDE_TEST_RAISE( ( mRecvType == SMI_RECOVER_UNTILTIME ) ||
                        ( mRecvType == SMI_RECOVER_UNTILCANCEL ),
                        err_incomplete_media_recovery);

        // EMPTY ����Ÿ������ ��쿡�� CREATELSN
        // ���� �̵����� �����Ѵ�.

        sFromLSN = sFileNode->mDBFileHdr.mCreateLSN;
    }
    else
    {
        // �������� �Ǵ� �ҿ��� ����������
        // FROM REDOLSN�� ��������� REDO LSN����
        // �����Ѵ�.
        sFromLSN = aDBFileHdr->mRedoLSN;
    }

    /* BUG-19272: �߸��� ���Ϸ� ���� ���� �õ��� error�� �����ʰ� core��
     * �߻���.
     *
     * smERR_ABORT_Invalid_DataFile_Create_LSN �� �߻��ϵ��� ������.
     * */
    // �̵���������
    // FROM REDOLSN < TO REDOLSN�� ������ �����ؾ��Ѵ�.
    IDE_TEST_RAISE( smLayerCallback::isLSNGT( &sToLSN, &sFromLSN ) == ID_FALSE,
                    err_datafile_invalid_create_lsn );


    // 2�� hashkey�� GRID�� �ƴ����� GRID ������
    // ����Ͽ� �����Ͽ���.
    SC_MAKE_GRID( sHashKey,
                  sFileNode->mSpaceID,
                  sFileNode->mID,
                  0 );

    IDE_TEST( smuHash::findNode(&mRecvFileHash,
                                &sHashKey,
                                (void **)&sHashNode) != IDE_SUCCESS );

    // ������ ������ �ѹ��� ��ϵǾ�� �Ѵ�.
    IDE_ASSERT( sHashNode == NULL );

    /* TC/FIT/Limit/sm/sdr/sdrRedoMgr::addRecvFileToHash::malloc.sql */
    IDU_FIT_POINT_RAISE( "sdrRedoMgr::addRecvFileToHash::malloc",
                          insufficient_memory );

    IDE_TEST_RAISE( iduMemMgr::malloc(IDU_MEM_SM_SDR,
                                ID_SIZEOF(sdrRecvFileHashNode),
                                (void**)&sHashNode) != IDE_SUCCESS,
                    insufficient_memory );

    sState = 1;
    idlOS::memset(sHashNode, 0x00, ID_SIZEOF(sdrRecvFileHashNode));

    sHashNode->mState           = SDR_RECV_NOT_START;
    sHashNode->mSpaceID         = sFileNode->mSpaceID;
    sHashNode->mFileID          = sFileNode->mID;
    sHashNode->mFstPageID       = sFstPageID;
    sHashNode->mLstPageID       = sLstPageID;
    sHashNode->mFromLSN         = sFromLSN;
    sHashNode->mToLSN           = sToLSN;
    sHashNode->mApplyRedoLogCnt = 0;
    sHashNode->mFileNode        = sFileNode;
    sHashNode->mSpaceName       = sSpaceName;

    IDE_TEST( smuHash::insertNode(&mRecvFileHash,
                                  &sHashKey,
                                  sHashNode) != IDE_SUCCESS );
    mRecvFileCnt++;

    *aRedoFromLSN = sFromLSN;
    *aRedoToLSN   = sToLSN;

    ideLog::log(SM_TRC_LOG_LEVEL_DRECOV,
                SM_TRC_DRECOVER_ADD_DATA_FILE1,
                sHashNode->mFileNode->mName,
                sHashNode->mSpaceID,
                sHashNode->mFileID);


    ideLog::log(SM_TRC_LOG_LEVEL_DRECOV,
                SM_TRC_DRECOVER_ADD_DATA_FILE2,
                sHashNode->mFstPageID,
                sHashNode->mLstPageID);

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_does_not_exist_datafile );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NotFoundDataFileByPath, aFileName ) );
    }
    IDE_EXCEPTION( err_incomplete_media_recovery );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NeedMediaRecovery ) );
    }
    IDE_EXCEPTION( err_datafile_invalid_create_lsn );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_InvalidDataFileCreateLSN,
                                  sFromLSN.mFileNo,
                                  sFromLSN.mOffset,
                                  aFileName,
                                  sToLSN.mFileNo,
                                  sToLSN.mOffset ) );
    }
    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET( ideSetErrorCode( idERR_ABORT_InsufficientMemory ) );
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        if (sState != 0)
        {
            IDE_ASSERT( iduMemMgr::free(sHashNode) == IDE_SUCCESS );
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}


/***********************************************************************
 * Description : redo log�� rid�� �̿��� hash value�� ����
 *
 * space id�� page id�� ������ ��ȯ�Ͽ� ������ ����� �����Ѵ�.
 * �� �Լ��� redo log�� ���� hash function���� ���ȴ�. hash key�� RID�̴�.
 **********************************************************************/
UInt sdrRedoMgr::genHashValueFunc(void* aGRID)
{

    IDE_DASSERT( aGRID != NULL );
    IDE_DASSERT( sctTableSpaceMgr::isSystemMemTableSpace( SC_MAKE_SPACE(*(scGRID*)aGRID) )
                 == ID_FALSE );

    return ((UInt)SC_MAKE_SPACE(*(scGRID*)aGRID) +
            (UInt)SC_MAKE_PID(*(scGRID*)aGRID));

}

/***********************************************************************
 * Description : hash-key ���Լ�
 *
 * 2���� RID�� ������ ���Ѵ�. ������ 0�� �����Ѵ�.
 * �� �Լ��� redo log�� ���� hash compare function���� ���ȴ�.
 **********************************************************************/
SInt sdrRedoMgr::compareFunc(void*  aLhs,
                             void*  aRhs )
{

    IDE_DASSERT( aLhs != NULL );
    IDE_DASSERT( aRhs != NULL );

    scGRID  *sLhs = (scGRID *)aLhs;
    scGRID  *sRhs = (scGRID *)aRhs;

    return ( ( SC_MAKE_SPACE( *sLhs ) == SC_MAKE_SPACE( *sRhs ) ) &&
             ( SC_MAKE_PID( *sLhs )  == SC_MAKE_PID( *sRhs ) ) ? 0 : -1 );

}

/**********************************************************************
 * Description : PROJ-2118 BUG Reporting
 *               Server Fatal ������ Signal Handler �� ȣ����
 *               Debugging ���� ����Լ�
 *
 *               �̹� altibase_dump.log �� lock�� ��� �����Ƿ�
 *               lock�� �����ʴ� trace ��� �Լ����� ����ؾ� �Ѵ�.
 *
 **********************************************************************/
void sdrRedoMgr::writeDebugInfo()
{
    if (( mCurSpaceID != SC_NULL_SPACEID ) &&
        ( mCurPageID  != SM_SPECIAL_PID ))
    {
        ideLog::log( IDE_DUMP_0,
                     "[ Cur Disk Page ] ( "
                     "SpaceID : %"ID_UINT32_FMT", "
                     "PageID  : %"ID_UINT32_FMT" )\n",
                     mCurSpaceID,
                     mCurPageID );

        if ( mCurPagePtr != NULL )
        {
            (void)sdpPhyPage::tracePage( IDE_DUMP_0,
                                         mCurPagePtr,
                                         "[ Page In Buffer ]" );

        }

        (void)sddDiskMgr::tracePageInFile( IDE_DUMP_0,
                                           mCurSpaceID,
                                           mCurPageID,
                                           "[ Page In File ]" );
        if ( mRedoLogPtr != NULL )
        {
            ideLog::log( IDE_DUMP_0,
                         "[ Disk Redo Log Info ]\n"
                         "Log Type : %"ID_UINT32_FMT"\n"
                         "Offset   : %"ID_UINT32_FMT"\n"
                         "SlotNum  : %"ID_UINT32_FMT"\n"
                         "Length   : %"ID_UINT32_FMT"\n"
                         "TransID  : %"ID_UINT32_FMT"\n"
                         "BeginLSN : [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n"
                         "EndLSN   : [ %"ID_UINT32_FMT", %"ID_UINT32_FMT" ]\n",
                         mRedoLogPtr->mType,
                         mRedoLogPtr->mOffset,
                         mRedoLogPtr->mSlotNum,
                         mRedoLogPtr->mValueLength,
                         mRedoLogPtr->mTransID,
                         mRedoLogPtr->mBeginLSN.mFileNo,
                         mRedoLogPtr->mBeginLSN.mOffset,
                         mRedoLogPtr->mEndLSN.mFileNo,
                         mRedoLogPtr->mEndLSN.mOffset );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)mRedoLogPtr,
                            ID_SIZEOF( sdrRedoLogData ),
                            "[ Disk Log Head ]" );

            ideLog::logMem( IDE_DUMP_0,
                            (UChar*)mRedoLogPtr->mValue,
                            mRedoLogPtr->mValueLength,
                            "[ Disk Log Body ]" );
        }
    }
}
