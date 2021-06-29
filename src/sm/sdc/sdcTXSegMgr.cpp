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
 * $Id: sdcTXSegMgr.cpp 88545 2020-09-10 09:14:02Z emlee $
 **********************************************************************/

#include <idl.h>
#include <smuProperty.h>
#include <smx.h>
#include <smrRecoveryMgr.h>

#include <sdcDef.h>
#include <sdcReq.h>
#include <sdcTXSegMgr.h>
#include <sdcTXSegFreeList.h>
#include <sdsFile.h>
#include <sdsBufferArea.h>
#include <sdsMeta.h>
#include <sdsBufferMgr.h>
#include <sdptbSpaceDDL.h>

sdcTXSegFreeList     * sdcTXSegMgr::mArrFreeList;
sdcTXSegEntry        * sdcTXSegMgr::mArrEntry;
UInt                   sdcTXSegMgr::mTotEntryCnt;
UInt                   sdcTXSegMgr::mFreeListCnt;
UInt                   sdcTXSegMgr::mCurFreeListIdx;
UInt                   sdcTXSegMgr::mCurEntryIdx4Steal;
idBool                 sdcTXSegMgr::mIsAttachSegment;

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� ���� ����
 *
 * Ʈ����� ���׸�Ʈ �����ڸ� �ʱ�ȭ�ϱ� ���� ȣ���Ͽ� ����� ������Ƽ�� ������
 * ������ �����ϰ�, FreeList ������ �����Ѵ�.
 * ����, ��� �����ϰԵ� ����� ������Ƽ ������ ������ ������ �ٲ�ġ���Ѵ�.
 * ���� ������Ƽ ���Ͽ��� ����Ǵ� ���� �ƴϴ�.
 *
 * aEntryCnt        - [IN]  �����Ǳ����� Ʈ����� ��Ʈ�� ���� ( ���� ������Ƽ�κ��� �о�� )
 * aAdjustEntryCnt  - [OUT] ������ Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 ***********************************************************************/
IDE_RC  sdcTXSegMgr::adjustEntryCount( UInt    aEntryCnt,
                                       UInt  * aAdjustEntryCnt )
{
    SChar sBuffer[ sdcTXSegMgr::CONV_BUFF_SIZE ]="";

    if ( aEntryCnt > 1 )
    {
        mFreeListCnt = IDL_MIN(smuUtility::getPowerofTwo( ID_SCALABILITY_CPU ), 512);
        mTotEntryCnt = IDL_MIN(smuUtility::getPowerofTwo( aEntryCnt ), 512);
    }
    else
    {
        // For Test
        mFreeListCnt = 1;
        mTotEntryCnt = 2;
    }

    if ( mTotEntryCnt < mFreeListCnt )
    {
        /* BUG-47681 undo tablespace full */
        mFreeListCnt =  mTotEntryCnt;
    }

    idlOS::snprintf( sBuffer, sdcTXSegMgr::CONV_BUFF_SIZE,
                     "%"ID_UINT32_FMT"", mTotEntryCnt );

    // PROJ-2446 bugbug
    IDE_TEST( idp::update( NULL, "TRANSACTION_SEGMENT_COUNT", sBuffer, 0 )
              != IDE_SUCCESS );

    if ( aAdjustEntryCnt != NULL )
    {
        *aAdjustEntryCnt = mTotEntryCnt;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}


/***********************************************************************
 *
 * Description : Undo TBS�� �ʱ� Ʈ����� ���׸�Ʈ�� �����Ѵ�.
 *
 * CreateDB �������� Undo TBS�� TSS���׸�Ʈ�� Undo ���׸�Ʈ��
 * TRANSACTION_SEGMENT_COUNT��ŭ ���� �����Ѵ�.
 *
 * aStatistics - [IN] �������
 * aTrans      - [IN] Ʈ����� ������
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::createSegs( idvSQL   * aStatistics,
                                void     * aTrans )
{
    sdrMtxStartInfo    sStartInfo;

    IDE_ASSERT( aTrans != NULL );

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;

    IDE_TEST( createTSSegs( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    IDE_TEST( createUDSegs( aStatistics, &sStartInfo )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ���� ����
 *
 * Undo TBS�� �����Ͽ�( HWM�� �������� �ʴ´� ) TSSEG�� UDSEG��
 * �ٽ� �����ϰ� �ʱ�ȭ�ϸ�, System Reused SCN�� �ʱ�ȭ �Ѵ�.
 *
 * aStatistics - [IN] �������
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::resetSegs( idvSQL* aStatistics )
{
    void * sTrans;

    IDE_ASSERT( smLayerCallback::allocTrans( &sTrans ) == IDE_SUCCESS );

    IDE_ASSERT( smLayerCallback::beginTrans( sTrans,
                                             (SMI_TRANSACTION_REPL_DEFAULT |
                                              SMI_COMMIT_WRITE_NOWAIT),
                                             NULL )
                == IDE_SUCCESS );

    IDE_TEST( sdpTableSpace::resetTBS( aStatistics,
                                       SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                       sTrans )
              != IDE_SUCCESS );

    /*
     * resetTBS ����� �߻��� pending�۾��� �Ϸ��ϱ� ���ؼ��� �ݵ��
     * createSegs������ Commit�Ǿ�� �Ѵ�.
     */
    IDE_ASSERT( smLayerCallback::commitTrans( sTrans ) == IDE_SUCCESS );

    IDE_ASSERT( smLayerCallback::beginTrans( sTrans,
                                             (SMI_TRANSACTION_REPL_DEFAULT |
                                              SMI_COMMIT_WRITE_NOWAIT),
                                             NULL )
                == IDE_SUCCESS );

    IDE_TEST( createSegs( aStatistics, sTrans ) != IDE_SUCCESS );

    IDE_ASSERT( smLayerCallback::commitTrans( sTrans ) == IDE_SUCCESS );

    IDE_ASSERT( smLayerCallback::freeTrans( sTrans ) == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ������ �ʱ�ȭ
 *
 * Mutex, Ʈ����� ���׸�Ʈ ��Ʈ��, FreeList�� �ʱ�ȭ �Ѵ�.
 * �ʱ�ȭ ������ Restart ������ �Ϸ�� ���Ŀ� �ʱ�ȭ�ȴ�.
 * Restart ���������� Ʈ����� ���׸�Ʈ ��Ʈ���� ������� ������,
 * ���� �������� ��Ʈ�� ������ ����� ���� �ֱ� ������ �ʱ�ȭ������
 * ���� �������� �Ѵ�.
 *
 * Ʈ����� ���׸�Ʈ�� �ش��ϴ� UDS/TSS ���׸�Ʈ�� Attach�Ͽ� �ʱ�ȭ�ϱ⵵�Ѵ�.
 * �������, Create Database ���������� Segment �������Ŀ� �ʱ�ȭ�ǹǷ� Attach
 * �ϸ�, Restart Recovery �������� �������̱� ������ Segment�� Attach�ؼ���
 * �ȵȴ�.
 *
 * aIsAttachSegment - [IN] Ʈ����� ���׸�Ʈ ������ �ʱ�ȭ�� ���׸�Ʈ Attach ����
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::initialize( idBool   aIsAttachSegment )
{

    UInt i;
    UInt sFreeEntryCnt;
    UInt sFstFreeEntry;
    UInt sLstFreeEntry;

    mArrEntry         = NULL;

    IDE_ASSERT( iduMemMgr::malloc( IDU_MEM_SM_TRANSACTION_SEGMENT_TABLE,
                                   (ULong)ID_SIZEOF(sdcTXSegEntry) * mTotEntryCnt,
                                   (void**)&mArrEntry)
                == IDE_SUCCESS );

    mArrFreeList      = NULL;
    IDE_ASSERT( iduMemMgr::malloc( IDU_MEM_SM_TRANSACTION_SEGMENT_TABLE,
                                   (ULong)ID_SIZEOF(sdcTXSegFreeList) * mFreeListCnt,
                                   (void**)&mArrFreeList )
                == IDE_SUCCESS );

    for ( i = 0; i < mTotEntryCnt; i++ )
    {
        initEntry( &mArrEntry[i], i /* aEntryIdx */ );
    }

    sFreeEntryCnt = mTotEntryCnt/mFreeListCnt;
    for ( i = 0; i < mFreeListCnt; i++ )
    {
        (void)new ( mArrFreeList + i ) sdcTXSegFreeList;

        sFstFreeEntry = i * sFreeEntryCnt;
        sLstFreeEntry = (i + 1) * sFreeEntryCnt -1;

        IDE_ASSERT( mArrFreeList[i].initialize( mArrEntry,
                                                i,
                                                sFstFreeEntry,
                                                sLstFreeEntry )
                    == IDE_SUCCESS );
    }

    if ( aIsAttachSegment == ID_TRUE )
    {
        for ( i = 0; i < mTotEntryCnt; i++ )
        {
            IDE_ASSERT( attachSegToEntry( &mArrEntry[i],
                                          i /* aEntryIdx */ )
                        == IDE_SUCCESS );
        }
    }
    mIsAttachSegment = aIsAttachSegment;

    mCurFreeListIdx    = 0;
    mCurEntryIdx4Steal = 0;

    return IDE_SUCCESS;
}
/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 * Ʈ����� ���׸�Ʈ�� Prepare Ʈ������� �������� ��������
 * ����������, �����ϸ� �������� ���ϰ�, �ʱ�ȭ�ؼ� ��������
 * �������� ����ϴ� �� ���׸�Ʈ���� ��� ����ؾ��Ѵ�.
 *
 * ���� ������ ���� �������� Ʈ����� ���׸�Ʈ ������ �ٸ���
 * ������ ��� Ʈ����� ���׸�Ʈ���� ������ ������ ������ϰ�
 * �ʱ�ȭ�Ѵ�. �̶��� prepare Ʈ������� �����ϴ� ��쿡��
 * ������ �� �����Ƿ� ������ ����ϰ� ���������� �����ϵ��� ó���Ѵ�.
 *
 * ���������������� Reset Ʈ����� ���׸�Ʈ ���Ŀ� ������ ���Ḧ
 * ����Ͽ� EntryCnt�� ����Ǿ�� �ϴ� ��� Reset �ϱ����� �ݵ��
 * Buffer Pool �� Log���� ��� Flush ���� checkpoint�� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::rebuild()
{
    UInt     i;
    UInt     sPropEntryCnt;
    UInt     sNewAdjustEntryCnt;

    if ( smxTransMgr::getPreparedTransCnt() == 0 )
    {
        ideLog::log(IDE_SERVER_0,
           "              Reset Undo Tablespace ...");

        IDE_ASSERT( destroy() == IDE_SUCCESS );

        sPropEntryCnt = smuProperty::getTXSEGEntryCnt();

        IDE_ASSERT( adjustEntryCount( sPropEntryCnt,
                                      &sNewAdjustEntryCnt )
                    == IDE_SUCCESS );

        /* Entry ���� üũ�� �ؼ� �ٸ� ��츸 flush ���� */
        if ( isModifiedEntryCnt( sNewAdjustEntryCnt ) == ID_TRUE )
        {
            IDE_TEST( sdsBufferMgr::flushDirtyPagesInCPList(
                                        NULL,      /* aStatistics */
                                        ID_TRUE )  /* flushAll */
                      != IDE_SUCCESS );

            IDE_TEST( sdbBufferMgr::flushDirtyPagesInCPList(
                                        NULL,      /* aStatistics */
                                        ID_TRUE )  /* flushAll */
                      != IDE_SUCCESS );

            IDE_TEST( smrRecoveryMgr::checkpoint(
                                        NULL /* aStatistics */,
                                        SMR_CHECKPOINT_BY_SYSTEM,
                                        SMR_CHKPT_TYPE_BOTH )
                      != IDE_SUCCESS );
        }

        IDE_TEST( resetSegs( NULL /* idvSQL */ ) != IDE_SUCCESS );

        IDE_ASSERT( smrRecoveryMgr::updateTXSEGEntryCnt(
                                          sNewAdjustEntryCnt ) == IDE_SUCCESS );

        IDE_TEST( initialize( ID_TRUE /* aIsAttachSegment */ )
                  != IDE_SUCCESS );
    }
    else
    {
        ideLog::log(IDE_SERVER_0,
           "              Attach Undo Tablespace ...");
        /* prepare Ʈ������� �ִ� ��쿡�� �̹� ON_SEG�� �ʱ�ȭ��
         * Ʈ����� ���׸�Ʈ �����ڸ� �״�� ����Ѵ�.
         * FreeList �� Entry ���µ� ��� �����Ǿ� �ִ� */
        for ( i = 0; i < mTotEntryCnt; i++ )
        {
            IDE_ASSERT( attachSegToEntry( &mArrEntry[i],
                                          i /* aEntryIdx */ )
                        == IDE_SUCCESS );
        }
        mIsAttachSegment = ID_TRUE;

        // BUG-27024 prepare trans ���� FstDskViewSCN�� ���� ����
        // ������ prepare trans ���� Oldest View SCN�� ������
        smxTransMgr::rebuildPrepareTransOldestSCN();

   }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ������ ����
 *
 * Mutex, Ʈ����� ���׸�Ʈ ��Ʈ��, FreeList�� �����Ѵ�.
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::destroy()
{
    UInt      i;

    for( i = 0; i < mTotEntryCnt; i++ )
    {
        finiEntry( &mArrEntry[i] );
    }

    for(i = 0; i < mFreeListCnt; i++)
    {
        IDE_ASSERT( mArrFreeList[i].destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT( iduMemMgr::free( mArrEntry )    == IDE_SUCCESS );
    IDE_ASSERT( iduMemMgr::free( mArrFreeList ) == IDE_SUCCESS);

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �ʱ�ȭ
 *
 * Ʈ����� ��Ʈ�� �ʱ�ȭ�� �� ���׸�Ʈ�� ��� ������ ���¿����� �����ϴ�.
 * ������ ���׸�Ʈ�κ��� SegHdr PID�� �Ҵ��� �� �ִ� PID�� �޾Ƽ�
 * ��Ʈ���� �����Ѵ�.
 *
 * aEntry         - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� Pointer
 * aEntryIdx      - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 ***********************************************************************/
void sdcTXSegMgr::initEntry( sdcTXSegEntry  * aEntry,
                             UInt             aEntryIdx )
{
    IDE_ASSERT( aEntry    != NULL );
    IDE_ASSERT( aEntryIdx != ID_UINT_MAX );

    aEntry->mEntryIdx       = aEntryIdx;
    aEntry->mStatus         = SDC_TXSEG_OFFLINE;
    aEntry->mFreeList       = NULL;

    SMU_LIST_INIT_NODE( &aEntry->mListNode );
    aEntry->mListNode.mData = (void*)aEntry;

    /*
     * BUG-23649 Undo TBS�� �������� ���������� ���� Steal ��å ����
     *
     * Steal ���꿡�� Expired�� ExtDir �����ϴ�����
     * ���� Next ExtDir�� ������ �ʾƵ� (I/O) ���� Ȯ���� �� �ִ�.
     */
    SM_MAX_SCN( &aEntry->mMaxCommitSCN );
}


/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ���� Segment ����
 *
 * Ʈ����� ���׸�Ʈ�� Segment�� �Ҵ������� �����Ѵ�.
 *
 * aEntry         - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� Pointer
 * aEntryIdx      - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::attachSegToEntry( sdcTXSegEntry  * aEntry,
                                      UInt             aEntryIdx )
{
    scPageID       sSegHdrPID;

    IDE_ASSERT( sdptbSpaceDDL::getTSSPID(
                    NULL, /* aStatistics */
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    aEntryIdx,
                    &sSegHdrPID ) == IDE_SUCCESS );

    IDE_ASSERT( aEntry->mTSSegmt.initialize( NULL, /* aStatistics */
                                             aEntry,
                                             sSegHdrPID )
                == IDE_SUCCESS );

    IDE_ASSERT( sdptbSpaceDDL::getUDSPID(
                    NULL, /* aStatistics */
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    aEntryIdx,
                    &sSegHdrPID ) == IDE_SUCCESS );

    IDE_ASSERT( aEntry->mUDSegmt.initialize( NULL, /* aStatistics */
                                             aEntry,
                                             sSegHdrPID )
                == IDE_SUCCESS );

    return IDE_SUCCESS;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 * Ʈ����� ���׸�Ʈ�� ������ ��� �ʱ�ȭ �Ѵ�.
 *
 * aEntry         - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� Pointer
 * aEntryIdx      - [IN] Ʈ����� ���׸�Ʈ ��Ʈ�� ����
 *
 ***********************************************************************/
void sdcTXSegMgr::finiEntry( sdcTXSegEntry * aEntry )
{
    IDE_ASSERT( aEntry != NULL );

    SMU_LIST_DELETE( &aEntry->mListNode );
    SMU_LIST_INIT_NODE( &aEntry->mListNode );
    aEntry->mListNode.mData = NULL;

    aEntry->mFreeList       = NULL;

    if ( mIsAttachSegment == ID_TRUE )
    {
        IDE_ASSERT( aEntry->mTSSegmt.destroy() == IDE_SUCCESS );
        IDE_ASSERT( aEntry->mUDSegmt.destroy() == IDE_SUCCESS );
    }
    aEntry->mStatus        = SDC_TXSEG_OFFLINE;
    aEntry->mEntryIdx      = ID_UINT_MAX;

    SM_MAX_SCN( &aEntry->mMaxCommitSCN );
}


/***********************************************************************
 *
 * Description : Steal ����� Expired�� Entry �Ҵ�
 *
 * �������� Entry�߿��� Entry�� Max CommitSCN�� OldestTransBSCN��
 * ���غ��� Expired�Ǿ����� Ȯ���� ����, Entry�� �Ҵ��Ѵ�.
 *
 * aStatistics      - [IN]  �������
 * aStartEntryIdx   - [IN]  �Ҵ��� Entry Idx
 * aSegType         - [IN]  Segment Type
 * aOldestTransBSCN - [IN]  ���� �������� ������ Statement SCN
 * aEntry           - [OUT] Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 ***********************************************************************/
void sdcTXSegMgr::tryAllocExpiredEntry( UInt             aStartEntryIdx,
                                        sdpSegType       aSegType,
                                        smSCN          * aOldestTransBSCN,
                                        sdcTXSegEntry ** aEntry )
{
    sdcTXSegEntry  * sEntry;
    UInt             sCurEntryIdx;

    sEntry       = NULL;
    sCurEntryIdx = aStartEntryIdx;

    while( 1 )
    {
        sdcTXSegFreeList::tryAllocExpiredEntryByIdx( sCurEntryIdx,
                                                     aSegType,
                                                     aOldestTransBSCN,
                                                     &sEntry );

        if ( sEntry != NULL )
        {
            break;
        }

        sCurEntryIdx++;

        if( sCurEntryIdx >= mTotEntryCnt )
        {
            sCurEntryIdx = 0;
        }

        if( sCurEntryIdx == aStartEntryIdx )
        {
            break;
        }
    }

    if ( sEntry != NULL )
    {
        // Ʈ����� ���׸�Ʈ ��Ʈ�� �ʱ�ȭ�ÿ� ��� �����Ǿ� �־�� �Ѵ�.
        IDE_ASSERT( sEntry->mEntryIdx != ID_UINT_MAX );
        IDE_ASSERT( sEntry->mStatus   == SDC_TXSEG_ONLINE );
    }

    *aEntry = sEntry;
}

/***********************************************************************
 *
 * Description : Entry Idx�� �ش��ϴ� Entry�� ���������̸� �Ҵ�
 *
 * (a) Steal
 *
 * (b) Restart Recovery�� Active Entry ���ε�
 *
 * Restart Recovery ���������� ���������� ������ �Ҵ�Ǿ��� Entry ID�� �ݵ��
 * ���ε��� �� �ֵ��� TRANSACTION_SEGMENT_COUNT ������Ƽ�� ���������� ��������
 * ����Ǿ�� �ϹǷ�, loganchor�� ����� ������ �ʱ�ȭ�� �ϸ�, Restart Recovery
 * ���Ŀ� ����� TRANSACTION_SEGMENT_COUNT�� ������ �� �ִ�.
 *
 *
 * aEntryIdx     - [IN]  Ž�� ���� Entry Idx
 * aEntry        - [OUT] Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 ***********************************************************************/
void sdcTXSegMgr::tryAllocEntryByIdx( UInt             aEntryIdx,
                                      sdcTXSegEntry ** aEntry )
{
    sdcTXSegEntry  * sEntry = NULL;

    IDE_ASSERT( mTotEntryCnt > aEntryIdx );

    sdcTXSegFreeList::tryAllocEntryByIdx( aEntryIdx,
                                          &sEntry );

    if ( sEntry != NULL )
    {
        // Ʈ����� ���׸�Ʈ ��Ʈ�� �ʱ�ȭ�ÿ� ��� �����Ǿ� �־�� �Ѵ�.
        IDE_ASSERT( sEntry->mEntryIdx != ID_UINT_MAX );
        IDE_ASSERT( sEntry->mStatus   == SDC_TXSEG_ONLINE );
    }

    *aEntry = sEntry;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �Ҵ�
 *
 * Ʈ����� ���׸�Ʈ ��Ʈ���� �Ҵ��� ������ FreeList�� �����Ͽ� �Ҵ� �õ��Ѵ�.
 * ���� ��� ��Ʈ���� ONLINE ���¶�� TXSEG_ALLOC_WAIT_TIME ��ŭ ����� ��
 * �ٽ� �õ��Ѵ�.
 *
 * aStatistics - [IN]  �������
 * aStartInfo  - [IN] Mini Transaction Start Info
 * aEntry      - [OUT] Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::allocEntry( idvSQL                   * aStatistics,
                                sdrMtxStartInfo          * aStartInfo,
                                sdcTXSegEntry           ** aEntry )
{

    UInt             sCurFreeListIdx;
    UInt             sStartFreeListIdx;
    PDL_Time_Value   sWaitTime;
    idvWeArgs        sWeArgs;
    /* BUG-40266 TRANSACTION_SEGMENT_COUNT �� �����ߴ��� trc�α׿� �����. */
    idBool           sAddTXSegEntryFull = ID_FALSE;
    smTID            sTransID = SM_NULL_TID;

    sStartFreeListIdx = (mCurFreeListIdx++ % mFreeListCnt);
    sCurFreeListIdx   = sStartFreeListIdx;
    *aEntry           = NULL;
    
    IDU_FIT_POINT( "BUG-45401@sdcTXSegMgr::allocEntry::iduCheckSessionEvent" );

    while(1)
    {
        mArrFreeList[sCurFreeListIdx].allocEntry( aEntry );

        if ( *aEntry != NULL )
        {
            break;
        }

        sCurFreeListIdx++;

        if( sCurFreeListIdx >= mFreeListCnt )
        {
            sCurFreeListIdx = 0;
        }

        if( sCurFreeListIdx == sStartFreeListIdx )
        {
            IDE_TEST( iduCheckSessionEvent( aStatistics ) != IDE_SUCCESS );

            IDV_WEARGS_SET(
                &sWeArgs,
                IDV_WAIT_INDEX_ENQ_ALLOCATE_TXSEG_ENTRY,
                0, /* WaitParam1 */
                0, /* WaitParam2 */
                0  /* WaitParam3 */ );

            IDV_BEGIN_WAIT_EVENT( aStatistics, &sWeArgs );

            if( sAddTXSegEntryFull == ID_FALSE )
            {  
                ideLog::log( IDE_SERVER_0,
                             " TRANSACTION_SEGMENT_COUNT is full !!\n"
                             " Current TRANSACTION_SEGMENT_COUNT is %"ID_UINT32_FMT"(Entry count:%"ID_UINT32_FMT")\n"
                             " Please check TRANSACTION_SEGMENT_COUNT\n",
                             smuProperty::getTXSEGEntryCnt(),
                             mTotEntryCnt );
                
                sTransID = smxTrans::getTransID(aStartInfo->mTrans);
                ideLog::log( IDE_SERVER_0,
                             " Transaction (TID:%"ID_UINT32_FMT") is waiting for TXSegs allocation.",
                             sTransID );

                /* trc �� �ѹ��� ���� */
                sAddTXSegEntryFull = ID_TRUE;
            }
            else 
            {
                /* nothing to do */
            }

            sWaitTime.set( 0, smuProperty::getTXSegAllocWaitTime() );
            idlOS::sleep( sWaitTime );

            IDV_END_WAIT_EVENT( aStatistics, &sWeArgs );
        }
    }

    // Ʈ����� ���׸�Ʈ ��Ʈ�� �ʱ�ȭ�ÿ� ��� �����Ǿ� �־�� �Ѵ�.
    IDE_ASSERT( (*aEntry)->mEntryIdx != ID_UINT_MAX );
    IDE_ASSERT( (*aEntry)->mStatus   == SDC_TXSEG_ONLINE );
     
    /* ����ϴ� Trans �� �� �������� Ȯ�� */
    if (sAddTXSegEntryFull == ID_TRUE )
    {
        IDE_DASSERT ( sTransID != SM_NULL_TID );
        ideLog::log( IDE_SERVER_0,
                     " Transaction (TID:%"ID_UINT32_FMT") acquired TXSegs allocation.\n",
                     sTransID );
    }
    else 
    {
        /* nothing to do */
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* ����ϴ� Trans �� �� �������� Ȯ�� */
    if (sAddTXSegEntryFull == ID_TRUE )
    {
        IDE_DASSERT ( sTransID != SM_NULL_TID );
        ideLog::log( IDE_SERVER_0,
                     " TXSegs allocation failed (TID:%"ID_UINT32_FMT").\n",
                     sTransID );
    }
    else 
    {
        /* nothing to do */
    }

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ�� �Ҵ�
 *
 * BUG-29839 ����� undo page���� ���� CTS�� ������ �� �� ����.
 * transaction�� Ư�� segment entry�� binding�ϴ� ��� �߰�
 *
 * aEntryID    - [IN]  �Ҵ���� Ʈ����� ���׸�Ʈ Entry ID
 * aEntry      - [OUT] Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::allocEntryBySegEntryID( UInt             aEntryID,
                                            sdcTXSegEntry ** aEntry )
{
    UInt             sFreeListIdx;

    IDE_TEST_RAISE( mTotEntryCnt <= aEntryID, err_WRONG_ENTRY_ID );

    // entry ID�� �ش��ϴ� segment entry�� ����� free list�� ã��
    sFreeListIdx = aEntryID / (mTotEntryCnt/mFreeListCnt);
    *aEntry      = NULL;

    mArrFreeList[sFreeListIdx].allocEntryByEntryID( aEntryID, aEntry );

    IDE_TEST_RAISE( *aEntry == NULL, err_NOT_AVAILABLE_ENTRY );

    return IDE_SUCCESS;

    IDE_EXCEPTION( err_WRONG_ENTRY_ID );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_WRONG_ENTRY_ID, aEntryID, mTotEntryCnt ) );
    }
    IDE_EXCEPTION( err_NOT_AVAILABLE_ENTRY );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NOT_AVAILABLE_ENTRY, aEntryID ) );
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 *
 * Description : TSS( Transaction Slot Segment)�� �����Ѵ�. �⺻������
 *               TRANSACTION_SEGMENT_COUNT ������ŭ�� TSS�� �����Ѵ�.
 *               ������ TSS�� �⺻������ Free TS List�� ������.
 *               �� ù��° �����Ǵ� System TSS�� Commit TS List�� ������.
 *
 * aStatistics   - [IN] ��� ����
 * aStartInfo    - [IN] Mini Transaction Start Info
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::createTSSegs( idvSQL          * aStatistics,
                                  sdrMtxStartInfo * aStartInfo )
{
    UInt              i;
    scPageID          sTSSegPID;
    sdrMtx            sMtx;

    for( i = 0; i < mTotEntryCnt ; i++ )
    {
        IDE_TEST( sdcTSSegment::create( aStatistics,
                                        aStartInfo,
                                        &sTSSegPID ) != IDE_SUCCESS );

        IDE_ASSERT( sdrMiniTrans::begin( aStatistics,
                                         &sMtx,
                                         aStartInfo,
                                         ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                         SM_DLOG_ATTR_DEFAULT )
                    == IDE_SUCCESS );

        IDE_ASSERT( sdptbSpaceDDL::setTSSPID( aStatistics,
                                              &sMtx,
                                              SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                              i,
                                              sTSSegPID )
                    == IDE_SUCCESS );

        IDE_ASSERT( sdrMiniTrans::commit( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : Undo Segment���� SDP_MAX_UDS_CNT��ŭ �����ϰ� ������
 *               UDS�� SegPID�� TBS�� Header�� �����Ѵ�.
 *
 * aStatistics - [IN] ��� ����
 * aStartInfo  - [IN] Mini Transaction ���� ����
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::createUDSegs( idvSQL          * aStatistics,
                                  sdrMtxStartInfo * aStartInfo )
{
    UInt              i;
    scPageID          sUDSegPID;
    sdrMtx            sMtx;

    for( i = 0; i < mTotEntryCnt; i++ )
    {
        IDE_TEST( sdcUndoSegment::create( aStatistics,
                                          aStartInfo,
                                          &sUDSegPID ) != IDE_SUCCESS );

        IDE_ASSERT( sdrMiniTrans::begin( aStatistics,
                                         &sMtx,
                                         aStartInfo,
                                         ID_FALSE,/*MtxUndoable(PROJ-2162)*/
                                         SM_DLOG_ATTR_DEFAULT )
                    == IDE_SUCCESS );

        IDE_ASSERT( sdptbSpaceDDL::setUDSPID( aStatistics,
                                              &sMtx,
                                              SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                              i,
                                              sUDSegPID )
                    == IDE_SUCCESS );

        IDE_ASSERT( sdrMiniTrans::commit( &sMtx ) == IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/***********************************************************************
 *
 * Description : TSS ���׸�Ʈ PID ���� ��ȯ
 *
 ***********************************************************************/
idBool sdcTXSegMgr::isTSSegPID( scPageID aSegPID )
{
    UInt i;

    for( i = 0; i < mTotEntryCnt; i++ )
    {
        if( mArrEntry[i].mTSSegmt.getSegPID() == aSegPID )
        {
            return ID_TRUE;
        }
    }

    return ID_FALSE;
}

/***********************************************************************
 *
 * Description : Undo ���׸�Ʈ PID ���� ��ȯ
 *
 ***********************************************************************/
idBool sdcTXSegMgr::isUDSegPID( scPageID aSegPID )
{
    UInt i;

    for( i = 0; i < mTotEntryCnt; i++ )
    {
        if( mArrEntry[i].mUDSegmt.getSegPID() == aSegPID )
        {
            return ID_TRUE;
        }
    }

    return ID_FALSE;
}

/***********************************************************************
 *
 * Description : Ʈ����� Commit/Abort�� �Ŀ� TSS�� UDS�κ��� ����� Extent Dir.
 *               �������� CSCN/ASCN ����
 *
 * Ʈ����� Ŀ�� ���� �߿� �Ϻ��̸�, Ʈ������� ����� Extent Dir.�������� Last CSCN��
 * No-Logging���� �����Ͽ� �ٸ� Ʈ������� ���뿩�θ� �Ǵ��� �� �ְ� �Ѵ�.
 *
 * No-Logging���� ó���� ���� ������ ������ ����Ǹ� GSCN����
 * ���� ������ �����Ǿ� ���� ���̰�, ������ ���밡���ϰ� �Ǵ��� �� ���� ���̱� �����̴�.
 *
 * aStatistics - [IN] �������
 * aEntry      - [IN] Ŀ���ϴ� Ʈ������� Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 * aCommitSCN  - [IN] Ʈ������� CommitSCN Ȥ�� AbortSCN(GSCN)
 *
 ***********************************************************************/
IDE_RC sdcTXSegMgr::markSCN( idvSQL        * aStatistics,
                             sdcTXSegEntry * aEntry,
                             smSCN         * aCSCNorASCN )
{
    scPageID         sTSSegPID;
    scPageID         sUDSegPID;
    sdcTSSegment   * sTSSegPtr;
    sdcUndoSegment * sUDSegPtr;

    IDE_ASSERT( aEntry->mExtRID4TSS != SD_NULL_RID );
    IDE_ASSERT( aCSCNorASCN         != NULL );
    IDE_ASSERT( SM_SCN_IS_NOT_INIT( *aCSCNorASCN ) );

    sTSSegPtr = getTSSegPtr( aEntry );
    sTSSegPID = sTSSegPtr->getSegPID();

    IDE_TEST( sTSSegPtr->markSCN4ReCycle(
                    aStatistics,
                    SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                    sTSSegPID,
                    aEntry->mExtRID4TSS,
                    aCSCNorASCN ) != IDE_SUCCESS );

    if ( aEntry->mFstExtRID4UDS != SD_NULL_RID )
    {
        sUDSegPtr = getUDSegPtr( aEntry );
        sUDSegPID = sUDSegPtr->getSegPID();

        IDE_TEST( sUDSegPtr->markSCN4ReCycle(
                               aStatistics,
                               SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                               sUDSegPID,
                               aEntry->mFstExtRID4UDS,
                               aEntry->mLstExtRID4UDS,
                               aCSCNorASCN ) != IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT( aEntry->mLstExtRID4UDS == SD_NULL_RID );
    }

    /*
     * Ʈ����� ���׸�Ʈ�� �ѹ��� �� Ʈ����Ǹ� ����ϹǷ�,
     * ���� �������� ����� Ʈ����� CSCN�� ���� ũ��.
     */
    SM_SET_SCN( &aEntry->mMaxCommitSCN, aCSCNorASCN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/* BUG-31055 Can not reuse undo pages immediately after it is used to 
 * aborted transaction 
 * ��� ��Ȱ�� �� �� �ֵ���, ED���� Shrink�Ѵ�. */
IDE_RC sdcTXSegMgr::shrinkExts( idvSQL        * aStatistics,
                                void          * aTrans,
                                sdcTXSegEntry * aEntry )
{
    scPageID         sUDSegPID;
    sdcUndoSegment * sUDSegPtr;
    sdrMtxStartInfo  sStartInfo;

    IDE_ASSERT( aTrans != NULL );

    sStartInfo.mTrans   = aTrans;
    sStartInfo.mLogMode = SDR_MTX_LOGGING;


    if ( aEntry->mFstExtRID4UDS != SD_NULL_RID )
    {
        /* Undo�� shrink���ش�. TxSeg�� abort���� row�� �� ��
         * �����Ƿ� shrink �ؼ��� �ȵȴ�. */
        sUDSegPtr = getUDSegPtr( aEntry );
        sUDSegPID = sUDSegPtr->getSegPID();

        IDE_TEST( sUDSegPtr->shrinkExts( aStatistics,
                                         &sStartInfo,
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         sUDSegPID,
                                         aEntry->mFstExtRID4UDS,
                                         aEntry->mLstExtRID4UDS ) 
                  != IDE_SUCCESS );
    }
    else
    {
        IDE_ASSERT( aEntry->mLstExtRID4UDS == SD_NULL_RID );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

#if 0
/******************************************************************************
 *
 * Description : �Ҵ�� Ʈ����� ���׸�Ʈ ��Ʈ�� ���� ��ȯ
 *
 ******************************************************************************/
SInt sdcTXSegMgr::getOnlineEntryCount()
{
    UInt i;
    SInt sTotOnlineEntryCnt = 0;

    for ( i = 0 ; i < mFreeListCnt; i++ )
    {
        sTotOnlineEntryCnt += mArrFreeList[i].getAllocEntryCount();
    }

    IDE_ASSERT( sTotOnlineEntryCnt >= 0 );

    return sTotOnlineEntryCnt;
}
#endif

/******************************************************************************
 *
 * Description : TRANSACTION_SEGMENT_ENTRY_COUNT ������Ƽ�� ����Ǿ����� üũ�Ѵ�.
 *
 ******************************************************************************/
idBool sdcTXSegMgr::isModifiedEntryCnt( UInt  aNewAdjustEntryCnt )
{
    UInt  sPrvAdjustEntryCnt;

    sPrvAdjustEntryCnt = smrRecoveryMgr::getTXSEGEntryCnt();

    if ( sPrvAdjustEntryCnt == aNewAdjustEntryCnt )
    {
        return ID_FALSE;
    }

    return ID_TRUE;
}

/******************************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ���κ��� ������ ������ ����´�.
 *
 * Optimisitics ������� �ѹ� üũ�غ� �����̱� ������ MaxSCN�� Ȯ���� �� �ʿ����.
 * �ٸ�, ���� NxtExtDir�� ������ ���� �ֻ��̴�. ������ True�� ��ȯ�Ѵ�.
 * Pessmistics�� ������� ��� ��Ʈ���� ���ؼ� �����غ���.
 *
 * aStatistics       - [IN] �������
 * aStartInfo        - [IN] Mtx ��������
 * aFromSegType      - [IN] From ���׸�Ʈ Ÿ��
 * aToSegType        - [IN] To ���׸�Ʈ Ÿ��
 * aToEntry          - [IN] To Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 * aSysMinDskViewSCN - [IN] Active Ʈ������� ���� Statment �߿���
 *                          ���� �������� ������ Statement�� SCN
 * aTrySuccess       - [OUT] Steal ��������
 *
 ******************************************************************************/
IDE_RC sdcTXSegMgr::tryStealFreeExtsFromOtherEntry(
                                            idvSQL          * aStatistics,
                                            sdrMtxStartInfo * aStartInfo,
                                            sdpSegType        aFromSegType,
                                            sdpSegType        aToSegType,
                                            sdcTXSegEntry   * aToEntry,
                                            smSCN           * aSysMinDskViewSCN,
                                            idBool          * aTrySuccess )
{
    sdcTXSegEntry  * sFrEntry;
    UInt             sStartIdx;
    UInt             sCurEntryIdx;
    UInt             sState = 0;
    idBool           sTrySuccess;
    UInt             sPropRetryStealCnt;
    UInt             sRetryCnt;

    IDE_ASSERT( aStartInfo        != NULL );
    IDE_ASSERT( aToEntry          != NULL );
    IDE_ASSERT( aSysMinDskViewSCN != NULL );

    *aTrySuccess = ID_FALSE;
    sTrySuccess  = ID_FALSE;
    sStartIdx    = (mCurEntryIdx4Steal++ % mTotEntryCnt);
    sCurEntryIdx = sStartIdx;

    sPropRetryStealCnt = smuProperty::getRetryStealCount();

    IDE_TEST_CONT( sPropRetryStealCnt == 0, CONT_FINISH_STEAL );
    
    /* 1.target segType���� ExpiredEntry�� �ִ��� Ȯ�� */
    tryAllocExpiredEntry( sCurEntryIdx,
                          aFromSegType,
                          aSysMinDskViewSCN,
                          &sFrEntry );
    
    /* 2. (1)�� ���������� sFrEntry�� Steal �õ� */
    if ( sFrEntry != NULL )
    {
        sState = 1;
        IDE_TEST( tryStealFreeExts( aStatistics,
                                    aStartInfo,
                                    aFromSegType,
                                    aToSegType,
                                    sFrEntry,
                                    aToEntry,
                                    &sTrySuccess ) 
                  != IDE_SUCCESS );

        sState = 0;
        freeEntry( sFrEntry,
                   ID_FALSE /* aMoveToFirst */ );

        IDE_TEST_CONT( sTrySuccess == ID_TRUE, CONT_FINISH_STEAL );
    }

    sRetryCnt = 0;

    /* 3. target segType���� �� entry �� ��ȸ�ϸ鼭 Steal �õ� */
    while( 1 )
    {
        sFrEntry = NULL;

        tryAllocEntryByIdx( sCurEntryIdx, &sFrEntry );

        if ( sFrEntry != NULL )
        {
            sState = 1;
            IDE_TEST( tryStealFreeExts( aStatistics,
                                        aStartInfo,
                                        aFromSegType,
                                        aToSegType,
                                        sFrEntry,
                                        aToEntry,
                                        &sTrySuccess ) 
                      != IDE_SUCCESS );

            sState = 0;
            freeEntry( sFrEntry,
                       ID_FALSE /* aMoveToFirst */ );

            IDE_TEST_CONT( sTrySuccess == ID_TRUE, CONT_FINISH_STEAL );
        }

        sCurEntryIdx++;

        if ( sCurEntryIdx >= mTotEntryCnt )
        {
            sCurEntryIdx = 0;
        }

        sRetryCnt++;

        if ( (sCurEntryIdx == sStartIdx) || (sRetryCnt >= sPropRetryStealCnt) )
        {
            break;
        }
    }
    
    IDE_EXCEPTION_CONT( CONT_FINISH_STEAL );

    *aTrySuccess = sTrySuccess;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if ( sState != 0 )
    {
        IDE_PUSH();
        sdcTXSegMgr::freeEntry( sFrEntry,
                                ID_FALSE /* aMoveToFirst */ );
        IDE_POP();
    }

    return IDE_FAILURE;
}

/******************************************************************************
 *
 * Description : Ʈ����� ���׸�Ʈ ��Ʈ���κ��� ������ ������ ����´�.
 *
 * Optimisitics ������� �ѹ� üũ�غ� �����̱� ������ MaxSCN�� Ȯ���� �� �ʿ����.
 * �ٸ�, ���� NxtExtDir�� ������ ���� �ֻ��̴�. ������ True�� ��ȯ�Ѵ�.
 * Pessmistics�� ������� ��� ��Ʈ���� ���ؼ� �����غ���.
 *
 * aStatistics      - [IN] �������
 * aStartInfo       - [IN] Mtx ��������
 * aFromSegType     - [IN] From ���׸�Ʈ Ÿ��
 * aToSegType       - [IN] To ���׸�Ʈ Ÿ��
 * aFrEntry         - [IN] From Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 * aToEntry         - [IN] To Ʈ����� ���׸�Ʈ ��Ʈ�� ������
 *
 ******************************************************************************/
IDE_RC sdcTXSegMgr::tryStealFreeExts( idvSQL          * aStatistics,
                                      sdrMtxStartInfo * aStartInfo,
                                      sdpSegType        aFromSegType,
                                      sdpSegType        aToSegType,
                                      sdcTXSegEntry   * aFrEntry,
                                      sdcTXSegEntry   * aToEntry,
                                      idBool          * aTrySuccess )
{
    sdpSegHandle   * sFrSegHandle;
    scPageID         sFrSegPID;
    scPageID         sFrCurExtDirPID;
    sdpSegHandle   * sToSegHandle;
    scPageID         sToSegPID;
    scPageID         sToCurExtDirPID;
    sdpSegMgmtOp   * sSegMgmtOp;

    switch (aFromSegType)
    {
        case SDP_SEG_TYPE_UNDO:
            sFrSegPID       = aFrEntry->mUDSegmt.getSegPID();
            sFrCurExtDirPID = aFrEntry->mUDSegmt.getCurAllocExtDir();
            sFrSegHandle    = aFrEntry->mUDSegmt.getSegHandle();
            break;

        case SDP_SEG_TYPE_TSS:
            sFrSegPID       = aFrEntry->mTSSegmt.getSegPID();
            sFrCurExtDirPID = aFrEntry->mTSSegmt.getCurAllocExtDir();
            sFrSegHandle    = aFrEntry->mTSSegmt.getSegHandle();
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    switch (aToSegType)
    {
        case SDP_SEG_TYPE_UNDO:
            sToSegPID       = aToEntry->mUDSegmt.getSegPID();
            sToCurExtDirPID = aToEntry->mUDSegmt.getCurAllocExtDir();
            sToSegHandle    = aToEntry->mUDSegmt.getSegHandle();
            break;

        case SDP_SEG_TYPE_TSS:
            sToSegPID       = aToEntry->mTSSegmt.getSegPID();
            sToCurExtDirPID = aToEntry->mTSSegmt.getCurAllocExtDir();
            sToSegHandle    = aToEntry->mTSSegmt.getSegHandle();
            break;

        default:
            IDE_ASSERT(0);
            break;
    }

    sSegMgmtOp = sdpSegDescMgr::getSegMgmtOp( SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO );
    // codesonar::Null Pointer Dereference
    IDE_ERROR( sSegMgmtOp != NULL );

    IDE_TEST( sSegMgmtOp->mTryStealExts( aStatistics,
                                         aStartInfo,
                                         SMI_ID_TABLESPACE_SYSTEM_DISK_UNDO,
                                         sFrSegHandle,
                                         sFrSegPID,
                                         sFrCurExtDirPID,
                                         sToSegHandle,
                                         sToSegPID,
                                         sToCurExtDirPID,
                                         aTrySuccess )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    *aTrySuccess = ID_FALSE;

    return IDE_FAILURE;
}
