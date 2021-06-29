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
 * $Id: smrDPListMgr.cpp 19996 2007-01-18 13:00:36Z bskim $
 **********************************************************************/

#include <smErrorCode.h>
#include <smm.h>
#include <smu.h>
#include <smr.h>
#include <sctTableSpaceMgr.h>

iduHash smrDPListMgr::mMgrHash;

/*
    Tablespace�� Dirty Page�����ڸ� �����ϴ� smrDPListMgr�� �ʱ�ȭ
 */
IDE_RC smrDPListMgr::initializeStatic()
{
    IDE_TEST( mMgrHash.initialize( IDU_MEM_SM_SMR,
                                   256, /* aInitFreeBucketCount */
                                   256) /* aHashTableSpace */
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Tablespace�� Dirty Page�����ڸ� �����ϴ� smrDPListMgr�� �ı�
 */
IDE_RC smrDPListMgr::destroyStatic()
{

    // ��� TBS�� ���� Dirty Page������ ����
    IDE_TEST( destroyAllMgrs() != IDE_SUCCESS );

    IDE_TEST( mMgrHash.destroy() != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    ������ Hash Element�� ���� ȣ��� Visitor�Լ�

    Shutdown�ÿ� ȣ��Ǹ� mMgrHash�� �����ϴ� TBS�� ���� ����ȴ�.
 */
IDE_RC smrDPListMgr::destoyingVisitor( vULong   aKey,
                                       void   * aData,
                                       void   * /*aVisitorArg*/ )
{
    scSpaceID          sSpaceID = (scSpaceID) aKey;
    smrDirtyPageList * sDPMgr   = (smrDirtyPageList *) aData;

    IDE_DASSERT( sDPMgr != NULL );

    // DROP/DISCARD/OFFLINE���� ���� Tablespace��� ?
    if ( sctTableSpaceMgr::isOnlineTBS( sSpaceID ) == ID_TRUE )
    {
        // BUGBUG-1548 - smrRecoveryMgr::destroy�ÿ�
        //               Checkpoint�� 2�� �Ϸ�Ǹ� ���̻� Dirty Page��
        //               ���� ���� ����.
        //               removeAll��� Dirty Page�� ������
        //               ASSERT�� Ȯ���ϵ��� ���� (�����ʿ�)
        //

        // Dirty Page�� PCH�� �����Ͽ� Dirty���°� �ƴ� ���·� �������ش�.
        sDPMgr->removeAll(ID_FALSE); // return void
    }
    else
    {
        // DROP/DISCARD/OFFLINE �� Tablespace��
        // PCH�� �̹� Free�Ǿ� ���� �����̴�.
        // �ƹ��� ó���� �ʿ����� �ʴ�.
    }

    // Dirt Page List�� �ı��ϰ�
    // Hash���� �����Ѵ�. ( Visiting���� ���Ű� �����ϴ� )
    IDE_TEST( removeDPList( aKey ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * Hashtable�� ��ϵ� ��� smrDirtyPageList�� destroy�Ѵ�.
 */
IDE_RC smrDPListMgr::destroyAllMgrs()
{

    IDE_TEST( mMgrHash.traverse( destoyingVisitor,
                                 NULL /* Visitor Arg */ )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Ư�� Tablespace�� Dirty Page�����ڿ� Dirty Page�� �߰��Ѵ�.
 */
IDE_RC smrDPListMgr::add(scSpaceID aSpaceID,
                         smmPCH*   aPCHPtr,
                         scPageID  aPageID)
{
    smrDirtyPageList * sDPList ;

    IDE_DASSERT( aPCHPtr != NULL );

    IDE_TEST( findDPList( aSpaceID, & sDPList ) != IDE_SUCCESS );

    if ( sDPList == NULL )
    {
        // Tablespace�� Dirty Page�����ڰ� ���� ��� ���� ������ش�.
        IDE_TEST( createDPList( aSpaceID ) != IDE_SUCCESS );

        IDE_TEST( findDPList( aSpaceID, & sDPList ) != IDE_SUCCESS );

        IDE_ASSERT( sDPList != NULL );
    }

    sDPList->add( aPCHPtr, aPageID ); // return void

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
 * smrDPListMgr::flushDP �� ���� Action����
 */
typedef struct smrFlushDPArg
{
    ULong                       mTotalCnt;
    ULong                       mRemoveCnt;
    ULong                       mTotalWaitTime;  /* microsecond */
    ULong                       mTotalSyncTime;  /* microsecond */
    /* Shutdown������ ������ Checkpoint���� ����*/
    idBool                      mIsFinalWrite;
    smmGetFlushTargetDBNoFunc   mGetFlushTargetDBNoFunc;
    sctStateSet                 mStateSet;
} smrFlushDPArg;


/*
   smrDirtyPageList::writeDirtyPage�� �����ϴ� Action�Լ�

   [ OFFLINE�̰ų� DROPPED�� TBS => Dirty Page Flush���� ���� ]

      Tablespace�� Drop�ϴ� ��� Checkpoint Image File��
      ���� ���� �ִ�.  Checkpoint������ �������� ���߿� �ִ�
      (Ȥ�� �̹� ������) Checkpoint Image�� ����
      Dirty Page�� Flush���� �ʴ´�.

      Tablespace�� Offline�̳� Drop�ϴ°��
      PCH�� PCH Array�� �߰��� ������ ���� �ִ�.
        => PCH / PCH Array�� Dirty Page Flush�ϴµ� �ʼ� �����ͱ����̴�.

   [ ���ü� ���� ]

      TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ���
      Dirty Page�� Checkpoint Image�� ����ϴ� ����
      TBS.SyncMutex�� ȹ���Ѵ�.

      - ���� : TBS���¸� DROP�̳� OFFLINE���� ���̽�Ű�� Tx��
                TBS.SyncMutex�� ��´�.

 */
IDE_RC smrDPListMgr::writeDirtyPageAction( idvSQL            * /* aStatistics */,
                                           sctTableSpaceNode * aSpaceNode,
                                           void * aActionArg )
{
    UInt               sStage = 0;
    UInt               sTotalCnt;
    UInt               sRemoveCnt;
    ULong              sWaitTime;
    ULong              sSyncTime;

    smrFlushDPArg    * sActionArg = (smrFlushDPArg * ) aActionArg;

    smrDirtyPageList * sDPList;

    IDE_DASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ��� ����
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
        sStage = 1;

        if ( sctTableSpaceMgr::hasState(
                                   aSpaceNode,
                                   sActionArg->mStateSet ) == ID_TRUE )
        {
            /* ���¸� �������� �ʴ� ��� */
            switch ( sActionArg->mStateSet )
            {
                case SCT_SS_SKIP_CHECKPOINT:
                    // Chkpt�ÿ���
                    // DROP/DISCARD/OFFLINE�� Tablespace���
                    // �ش� Tablespace�� Dirty Page�����ڸ� �ı�, �����Ѵ�.
                    // (����. removeDPList�� PCH�� Dirty Page��
                    // �ǵ帮�� �ʴ´�. )
                    IDE_TEST( removeDPList( aSpaceNode->mID ) != IDE_SUCCESS );
                    break;
                case SCT_SS_UNABLE_MEDIA_RECOVERY:
                    // DROP/DISCARD�� Tablespace�� �����Ѵ�.
                    IDE_TEST( removeDPList( aSpaceNode->mID ) != IDE_SUCCESS );
                    break;
                default:
                    IDE_ASSERT( 0 );
                    break;
            }
        }
        else
        {
            // ���¸� �����ϴ� ���
            switch ( sActionArg->mStateSet )
            {
                case SCT_SS_SKIP_CHECKPOINT:
                    // TableSpace�� ONLINE���� ��� Flush ����
                    break;
                case SCT_SS_UNABLE_MEDIA_RECOVERY:
                    // TableSpace�� ONLINE/OFFLINE�����̹Ƿ� Flush ����
                    break;
                default:
                    IDE_ASSERT( 0 );
                    break;
            }

            // SMR Dirty Page List ��ü ������
            IDE_TEST( findDPList( aSpaceNode->mID,
                                  & sDPList ) != IDE_SUCCESS );

            if ( sDPList != NULL )
            {
                // FLush�ǽ�
                IDE_TEST( sDPList->writeDirtyPages(
                            (smmTBSNode*) aSpaceNode,
                            sActionArg->mGetFlushTargetDBNoFunc,
                            sActionArg->mIsFinalWrite,
                            sActionArg->mTotalCnt,
                            &sTotalCnt,
                            &sRemoveCnt,
                            &sWaitTime,
                            &sSyncTime )
                        != IDE_SUCCESS );

                sActionArg->mTotalCnt      += sTotalCnt;
                sActionArg->mRemoveCnt     += sRemoveCnt;
                sActionArg->mTotalWaitTime += sWaitTime;
                sActionArg->mTotalSyncTime += sSyncTime;
            }
            else
            {
                // TBS�� �ش��ϴ� SMR Dirty Page List�� ����
                // => Flush�� Dirty Page �� ���� ���̹Ƿ� �ƹ��͵� ���� �ʴ´�.
            }
        }

        sStage = 0;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex(
                                aSpaceNode ) == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();
    return IDE_FAILURE;
}




/*
   smrDirtyPageList::writePIDLogs�� �����ϴ� Action�Լ�

   [ OFFLINE�̰ų� DROPPED�� TBS => Dirty Page Flush���� ���� ]
       writeDirtyPageAction �� ����

   [ ���ü� ���� ]
       writeDirtyPageAction �� ����

 */
IDE_RC smrDPListMgr::writePIDLogAction( idvSQL*             /* aStatistics */,
                                        sctTableSpaceNode * aSpaceNode,
                                        void * /* aActionArg */ )
{
    UInt               sStage = 0;

    smrDirtyPageList * sDPList;

    IDE_DASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ��� ����
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
        sStage = 1;

        // DROP/DISCARD/OFFLINE�� Tablespace��� ?
        if ( sctTableSpaceMgr::hasState( aSpaceNode, SCT_SS_SKIP_CHECKPOINT )
             == ID_TRUE )
        {
            // DROP/DISCARD/OFFLINE�� Tablespace���
            // �ƹ��� ó���� �������� �ʴ´�.
        }
        else
        {
            // SMR Dirty Page List ��ü ������
            IDE_TEST( findDPList( aSpaceNode->mID,
                                  & sDPList ) != IDE_SUCCESS );
            if ( sDPList != NULL )
            {
                // Page Write�ǽ�
                IDE_TEST( sDPList->writePIDLogs() != IDE_SUCCESS );
            }
            else
            {
                // TBS�� �ش��ϴ� SMR Dirty Page List�� ���� ��Ȳ
                // Dirty Page�� ���� ��Ȳ�̹Ƿ� Page ID�α뵵 ���� �ʴ´�.
            }
        }

        sStage = 0;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex(
                                aSpaceNode ) == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}



/*
    ��� Tablespace�� Dirty Page�� ����
    PID�� �α��ϰ� Page Image�� Checkpoint Image�� ���

    [OUT] aTotalCnt  - Flush �ǽ��� DIRTY PAGE ����
    [OUT] aRemoveCnt - Dirty Page List���� ���ŵ� Page�� ��
    [IN]  aOption    - Dirty Page��Ͽɼ�
                       ex> PID�α� ���ϱ� (Media Recovery�� ����)

    [�˰���]
       (010) ��� TBS�� ���� Dirty Page ID�� 0�� LFG�� �α�
       (020) Page ID Log�� ��ϵ� DISK(0��) LFG�� FLUSH
       (030) ��� TBS�� ���� Dirty Page�� Checkpoint Image�� ���
 */
IDE_RC smrDPListMgr::writeDirtyPages4AllTBS(
                     sctStateSet                 aStateSet,
                     ULong                     * aTotalCnt,
                     ULong                     * aRemoveCnt,
                     ULong                     * aWaitTime,
                     ULong                     * aSyncTime,
                     smmGetFlushTargetDBNoFunc   aGetFlushTargetDBNoFunc,
                     smrWriteDPOption            aWriteOption )
{
    smLSN         sSyncLstLSN;
    smrFlushDPArg sActArg;

    /* BUG-39925 [PROJ-2506] Insure++ Warning
     * - ���� �ʱ�ȭ�� �ʿ��մϴ�.
     */
    idlOS::memset( &sActArg, 0, ID_SIZEOF( sActArg ) );

    sActArg.mGetFlushTargetDBNoFunc = aGetFlushTargetDBNoFunc;
    sActArg.mStateSet               = aStateSet;

    IDE_DASSERT( aTotalCnt               != NULL );
    IDE_DASSERT( aRemoveCnt              != NULL );
    IDE_DASSERT( aWaitTime               != NULL );
    IDE_DASSERT( aSyncTime               != NULL );
    IDE_DASSERT( aGetFlushTargetDBNoFunc != NULL );

    IDE_TEST(smmDatabase::checkMembaseIsValid() != IDE_SUCCESS);

    // Checkpoint ������ ������ dirty page write����?
    if ( ( aWriteOption & SMR_WDP_FINAL_WRITE ) != SMR_WDP_FINAL_WRITE )
    {
        /* BUG-28554 [SM] CHECKPOINT_BULK_WRITE_SLEEP_[U]SEC�� ����� 
         * �������� �ʽ��ϴ�.
         * FINAL_WRITE�� �ƴ� ���, �� Shutdown���� Checkpoint�� �ƴϸ�
         * ���� mIsFinalWriteFlag�� False�� �����Ǿ�� CHECKPOINT_BULK-
         * _WRITE_SLEEP �� ����� �����մϴ�. */
        sActArg.mIsFinalWrite = ID_FALSE;
    }
    else
    {
        sActArg.mIsFinalWrite = ID_TRUE;
    }

    if ( ( aWriteOption & SMR_WDP_NO_PID_LOGGING ) != SMR_WDP_NO_PID_LOGGING )
    {
        /////////////////////////////////////////////////////////////////
        // (010) ��� TBS�� ���� Dirty Page ID�� 0�� LFG�� �α�
        IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                      writePIDLogAction,
                                                      (void*) & sActArg,
                                                      SCT_ACT_MODE_NONE )
                  != IDE_SUCCESS );


        /////////////////////////////////////////////////////////////////
        // (020) Page ID Log�� ��ϵ� DISK(0��) LFG�� FLUSH
        smrLogMgr::getLstLSN(&sSyncLstLSN) ;
        IDE_TEST(smrLogMgr::syncLFThread( SMR_LOG_SYNC_BY_CKP,
                                                &sSyncLstLSN )
                 != IDE_SUCCESS);
    }

    /////////////////////////////////////////////////////////////////
    // (030) ��� TBS�� ���� Dirty Page�� Checkpoint Image�� ���
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  writeDirtyPageAction,
                                                  (void*) & sActArg,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    *aTotalCnt  = (ULong) sActArg.mTotalCnt;
    *aRemoveCnt = (ULong) sActArg.mRemoveCnt;
    *aWaitTime  = (ULong) sActArg.mTotalWaitTime;
    *aSyncTime  = (ULong) sActArg.mTotalSyncTime;

    IDE_TEST(smmDatabase::checkMembaseIsValid() != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * getTotalDirtyPageCnt �� ���� Action����
 */
typedef struct smrCountDPArg
{
    ULong mTotalCnt;
} smrCountDPArg;


/*
 * getTotalDirtyPageCnt �� ���� Action�Լ�
 */
IDE_RC smrDPListMgr::countDPAction( idvSQL*             /*aStatistics */,
                                    sctTableSpaceNode * aSpaceNode,
                                    void * aActionArg )
{
    UInt                sStage = 0;
    smrDirtyPageList  * sDPList ;

    smrCountDPArg * sActionArg = (smrCountDPArg*) aActionArg;

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ��� ����
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
        sStage = 1;

        // DROP/DISCARD/OFFLINE���� ���� Tablespace��� ?
        if ( sctTableSpaceMgr::isOnlineTBS( aSpaceNode->mID ) == ID_TRUE )
        {
            IDE_TEST( findDPList( aSpaceNode->mID,
                                  & sDPList ) != IDE_SUCCESS );

            if ( sDPList != NULL )
            {
                sActionArg->mTotalCnt += sDPList->getDirtyPageCnt();
            }
            else
            {
                // TBS�� �ش��ϴ� SMR Dirty Page List�� ���ٴ� ����
                // ���� Dirty Page�� �ϳ��� ���� ����̴�
            }
        }
        else
        {
            // DROP/DISCARD/OFFLINE�� Tablespace��� ?

            // Dirty Page�� ���� �ʴ´�.
        }

        sStage = 0;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex(
                                aSpaceNode ) == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;

}

/*
   ��� Tablespace�� Dirty Page���� ���
    [OUT] aDirtyPageCount - Dirty Page��
*/
IDE_RC smrDPListMgr::getTotalDirtyPageCnt( ULong * aDirtyPageCount)
{
    smrCountDPArg sActArg;

    IDE_DASSERT( aDirtyPageCount != NULL );

    sActArg.mTotalCnt = 0;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  countDPAction,
                                                  (void*) & sActArg,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    *aDirtyPageCount = sActArg.mTotalCnt;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Ư�� Tablespace�� Dirty Page���� ����Ѵ�.

    [IN]  aTBSID          - Dirty Page���� ����� Tablespace ID
    [OUT] aDirtyPageCount - Dirty Page��
*/
IDE_RC smrDPListMgr::getDirtyPageCountOfTBS( scSpaceID   aTBSID,
                                             UInt      * aDirtyPageCount )
{
    smrDirtyPageList * sDPList ;

    IDE_DASSERT( aDirtyPageCount != NULL );

    IDE_TEST( findDPList( aTBSID, & sDPList ) != IDE_SUCCESS );
    if ( sDPList != NULL )
    {
        *aDirtyPageCount = sDPList->getDirtyPageCnt();
    }
    else
    {
        // aTBSID�� Dirty Page���� �����ϴ� SMR Dirty Page List �� ����.
        // => Dirty Page������ 0��
        *aDirtyPageCount = 0;
    }


    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}



/*
    Ư�� Tablespace�� ���� Dirty Page�����ڸ� �����Ѵ�.

    [IN] aSpaceID - �����ϰ��� �ϴ� Dirty Page�����ڰ� ���� Tablespace�� ID
 */
IDE_RC smrDPListMgr::createDPList(scSpaceID aSpaceID )
{
    smrDirtyPageList  * sDPList;
    UInt                sState  = 0;

    /* TC/FIT/Limit/sm/smr/smrDPListMgr_createDPList_malloc.sql */
    IDU_FIT_POINT_RAISE( "smrDPListMgr::createDPList::malloc", 
                          insufficient_memory );

    IDE_TEST_RAISE(iduMemMgr::malloc(IDU_MEM_SM_SMR,
                               ID_SIZEOF(smrDirtyPageList),
                               (void**) & sDPList ) != IDE_SUCCESS,
                   insufficient_memory );
    sState = 1;

    new ( sDPList ) smrDirtyPageList;

    IDE_TEST( sDPList->initialize( aSpaceID ) != IDE_SUCCESS);

    IDE_TEST( mMgrHash.insert( aSpaceID, sDPList ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION( insufficient_memory );
    {
        IDE_SET(ideSetErrorCode(idERR_ABORT_InsufficientMemory));
    }
    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            IDE_ASSERT( iduMemMgr::free( sDPList ) == IDE_SUCCESS );
            sDPList = NULL;
        default:
            break;
    }

    return IDE_FAILURE;
}

/*
   Ư�� Tablespace�� ���� Dirty Page�����ڸ� ã�Ƴ���.
   ã�� ���� ��� NULL�� ���ϵȴ�.

   [IN]  aSpaceID - ã���� �ϴ� DirtyPage�����ڰ� ���� Tablespace�� ID
   [OUT] aDPList   - ã�Ƴ� Dirty Page ������
*/

IDE_RC smrDPListMgr::findDPList( scSpaceID           aSpaceID,
                                 smrDirtyPageList ** aDPList )
{
    *aDPList = (smrDirtyPageList * ) mMgrHash.search( aSpaceID );

    return IDE_SUCCESS;
}

/*
    Ư�� Tablespace�� Dirty Page�����ڸ� �����Ѵ�.

    [IN] aSpaceID - �����ϰ��� �ϴ� Dirty Page�����ڰ� ���� Tablespace ID
 */
IDE_RC smrDPListMgr::removeDPList( scSpaceID aSpaceID )
{
    smrDirtyPageList * sDPList;

    sDPList = (smrDirtyPageList *) mMgrHash.search( aSpaceID );

    // SMR Dirty Page List�� ������ ���
    if ( sDPList != NULL )
    {
        // ���� �ǽ�
        IDE_TEST( sDPList->destroy() != IDE_SUCCESS );

        IDE_TEST( iduMemMgr::free( sDPList ) != IDE_SUCCESS );

        IDE_TEST( mMgrHash.remove( aSpaceID ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Ư�� Tablespace�� Dirty Page�� SMM=>SMR�� �̵��Ѵ�.

    [IN] aSpaceID - Dirty Page�� �̵��ϰ��� �ϴ� Tablespace�� ID
    [OUT] aNewCnt - ���� �߰��� Dirty Page ��
    [OUT] aDupCnt - ������ �����Ͽ��� Dirty Page ��
 */
IDE_RC smrDPListMgr::moveDirtyPages4TBS( scSpaceID   aSpaceID,
                                         UInt      * aNewCnt,
                                         UInt      * aDupCnt )
{
    smmDirtyPageMgr  * sSmmDPMgr;
    smrDirtyPageList * sSmrDPList;


    IDE_TEST( smmDirtyPageMgr::findDPMgr( aSpaceID, & sSmmDPMgr )
              != IDE_SUCCESS );

    // SMM Dirty Page Mgr�� TBS�� �ʱ�ȭ�Ǹ鼭 �Բ� �ʱ�ȭ �ȴ�.
    // �� �Լ��� ONLINE������ Tablespace�� ���� ȣ��Ǳ� ������
    // SMM Dirty Page Mgr�� �����ؾ� �Ѵ�.
    IDE_ASSERT( sSmmDPMgr != NULL );

    IDE_TEST( findDPList( aSpaceID, &sSmrDPList ) != IDE_SUCCESS );
    if( sSmrDPList == NULL )
    {
        // ���� �ѹ��� SMM => SMR�� Dirty Page�̵��� ���� ���� ���
        // Tablespace�� Dirty Page�����ڰ� ���� ��� ���� ������ش�.
        IDE_TEST( createDPList( aSpaceID ) != IDE_SUCCESS );

        IDE_TEST( findDPList( aSpaceID, & sSmrDPList ) != IDE_SUCCESS );

        IDE_ASSERT( sSmrDPList != NULL );
    }

    IDE_TEST( sSmrDPList->moveDirtyPagesFrom( sSmmDPMgr,
                                              aNewCnt,
                                              aDupCnt ) != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*
 * moveDirtyPages4AllTBS �� ���� Action����
 */
typedef struct smrMoveDPArg
{
    sctStateSet mStateSet;
    ULong       mNewDPCount;
    ULong       mDupDPCount;
} smrMoveDPArg;

/*
 * moveDirtyPages4AllTBS �� ���� Action�Լ�
 */
IDE_RC smrDPListMgr::moveDPAction( idvSQL*            /*  aStatistics */,
                                   sctTableSpaceNode * aSpaceNode,
                                   void * aActionArg )
{
    UInt               sStage = 0;
    scPageID           sNewDPCount;
    scPageID           sDupDPCount;

    smrMoveDPArg     * sActionArg = (smrMoveDPArg *) aActionArg ;

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // TBS���°� DROP�̳� OFFLINE���� ���̵��� �ʵ��� ����
        IDE_TEST( sctTableSpaceMgr::latchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
        sStage = 1;

        if ( sctTableSpaceMgr::hasState(
                               aSpaceNode,
                               sActionArg->mStateSet) == ID_TRUE )
        {
            switch ( sActionArg->mStateSet )
            {
                case SCT_SS_SKIP_CHECKPOINT:
                    // Chkpt�ÿ��� DROP/DISCARD/OFFLINE�� Tablespace���
                    // �ƹ��͵� ���� �ʴ´�.
                    break;
                case SCT_SS_UNABLE_MEDIA_RECOVERY:
                    // DROP/DISCARD�� Tablespace�� �����Ѵ�.
                    break;
                default:
                    IDE_ASSERT( 0 );
                    break;
            }
        }
        else
        {
            IDE_TEST( moveDirtyPages4TBS( aSpaceNode->mID,
                                          & sNewDPCount,
                                          & sDupDPCount )
                    != IDE_SUCCESS );

            sActionArg->mNewDPCount += sNewDPCount;
            sActionArg->mDupDPCount += sDupDPCount;
        }

        sStage = 0;
        IDE_TEST( sctTableSpaceMgr::unlatchSyncMutex( aSpaceNode )
                  != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    IDE_PUSH();
    {
        switch( sStage )
        {
            case 1:
                IDE_ASSERT( sctTableSpaceMgr::unlatchSyncMutex(
                                aSpaceNode ) == IDE_SUCCESS );
            default:
                break;
        }
    }
    IDE_POP();

    return IDE_FAILURE;
}



/*
    ��� Tablespace�� Dirty Page�� SMM=>SMR�� �̵��Ѵ�.

    [IN]  aIsOnChkpt - Chkpt �Ǵ� Media Recovery
    [OUT] aNewCnt    - ���� �߰��� Dirty Page ��
    [OUT] aDupCnt    - ������ �����Ͽ��� Dirty Page ��
 */
IDE_RC smrDPListMgr::moveDirtyPages4AllTBS( sctStateSet aStateSet,
                                            ULong *     aNewCnt,
                                            ULong *     aDupCnt )
{

    smrMoveDPArg sActionArg;

    IDE_DASSERT( aNewCnt != NULL );
    IDE_DASSERT( aDupCnt != NULL );

    sActionArg.mStateSet   = aStateSet;
    sActionArg.mNewDPCount = 0;
    sActionArg.mDupDPCount = 0;

    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  moveDPAction,
                                                  (void*) & sActionArg,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    *aNewCnt = sActionArg.mNewDPCount;
    *aDupCnt = sActionArg.mDupDPCount;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
    ��� Tablespace�� Dirty Page�� discard��Ų��.
    �̵��������� ȣ��ȴ�.
*/
IDE_RC smrDPListMgr::discardDirtyPages4AllTBS()
{
    IDE_TEST( sctTableSpaceMgr::doAction4EachTBS( NULL, /* idvSQL* */
                                                  discardDPAction,
                                                  (void*)NULL,
                                                  SCT_ACT_MODE_NONE )
              != IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;

}

/*
 * discardDirtyPages4AllTBS �� ���� Action�Լ�
 */
IDE_RC smrDPListMgr::discardDPAction( idvSQL            * /* aStatistics */,
                                      sctTableSpaceNode * aSpaceNode,
                                      void              * /* aActionArg */ )
{
    IDE_DASSERT( aSpaceNode != NULL );

    if ( sctTableSpaceMgr::isMemTableSpace( aSpaceNode ) == ID_TRUE )
    {
        // ONLINE/OFFLINE�� ���̺����̽��� ���ؼ��� Media Recovery��
        // �����ϹǷ� ���� ���¿� ASSERT ó���Ѵ�.
        if ( sctTableSpaceMgr::hasState( aSpaceNode,
                                         SCT_SS_UNABLE_MEDIA_RECOVERY )
             == ID_TRUE )
        {
            // �̵����� �Ұ����� TBS�� ����
            // discard�� dirty page�� �������� �ʴ´�.
        }
        else
        {
            IDE_TEST( discardDirtyPages4TBS( aSpaceNode->mID )
                    != IDE_SUCCESS );
        }

    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*
    Ư�� Tablespace�� ������ Dirty Page�� �����Ѵ�.

    [IN] aSpaceID - Dirty Page�� �����ϰ��� �ϴ� Tablespace�� ID
 */
IDE_RC smrDPListMgr::discardDirtyPages4TBS( scSpaceID   aSpaceID )
{
    smrDirtyPageList * sSmrDPList;

    IDE_TEST( findDPList( aSpaceID, &sSmrDPList ) != IDE_SUCCESS );

    if( sSmrDPList != NULL )
    {
        // SMR�� dirty page�� �����ϴ� ���
        sSmrDPList->removeAll( ID_TRUE );  // aIsForce
    }
    else
    {
        // SMR�� dirty page�� �������� �ʴ� ���
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
