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
 * $Id: sdnbTDBuild.cpp
 *
 * Description :
 *
 * Implementation Of Parallel Building Disk Index
 *
 * # Function
 *
 *
 **********************************************************************/
#include <smErrorCode.h>
#include <ide.h>
#include <smu.h>
#include <sdp.h>
#include <smc.h>
#include <sdc.h>
#include <smn.h>
#include <sdn.h>
#include <sdnReq.h>
#include <smxTrans.h>
#include <sdbMPRMgr.h>

sdnbTDBuild::sdnbTDBuild() : idtBaseThread()
{


}

sdnbTDBuild::~sdnbTDBuild()
{


}

/* ------------------------------------------------
 * Description :
 *
 * Index build ������ �ʱ�ȭ
 * ----------------------------------------------*/
IDE_RC sdnbTDBuild::initialize( UInt             aTotalThreadCnt,
                                UInt             aID,
                                smcTableHeader * aTable,
                                smnIndexHeader * aIndex,
                                smSCN            aFstDskViewSCN,
                                idBool           aIsNeedValidation,
                                UInt             aBuildFlag,
                                idvSQL*          aStatistics,
                                idBool         * aContinue )
{

    UInt sStatus = 0;

    mTotalThreadCnt = aTotalThreadCnt;
    mID = aID;
    mTable = aTable;
    mIndex = aIndex;
    mIsNeedValidation = aIsNeedValidation;
    mBuildFlag = aBuildFlag;
    mStatistics = aStatistics;

    mFinished = ID_FALSE;
    mContinue = aContinue;
    mErrorCode = 0;

    mTrans = NULL;
    IDE_TEST( smLayerCallback::allocNestedTrans( &mTrans )
              != IDE_SUCCESS );
    sStatus = 1;

    /*
      * TASK-2356 [��ǰ�����м�] altibase wait interface
      * ��Ȯ�� ��������� ���ϱ� ���ؼ���
      * Thread ������ŭ idvSQL �� �����ؾ� �Ѵ�.
      */
    IDE_TEST( smLayerCallback::beginTrans( mTrans,
                                           SMI_TRANSACTION_REPL_NONE,
                                           NULL )
              != IDE_SUCCESS );

    smLayerCallback::setFstDskViewSCN( mTrans, &aFstDskViewSCN );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    if(sStatus == 1)
    {
        IDE_ASSERT( smLayerCallback::freeTrans( mTrans )
                    == IDE_SUCCESS );
        mTrans = NULL;
    }

    return IDE_FAILURE;
}

/* ------------------------------------------------
 * Description :
 *
 * buffer flush ������ ����
 * ----------------------------------------------*/
IDE_RC sdnbTDBuild::destroy()
{

    UInt sStatus = 0;

    if( mTrans != NULL )
    {
        sStatus = 2;
        IDU_FIT_POINT( "1.BUG-48210@sdnbTDBuild::destroy" );
        IDE_TEST( smLayerCallback::commitTrans( mTrans ) != IDE_SUCCESS );
        sStatus = 1;
        IDE_TEST( smLayerCallback::freeTrans( mTrans ) != IDE_SUCCESS );
        sStatus = 0;
        mTrans = NULL;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    /* BUG-48210 commit ���п� ���� ���� ó�� ���� */
    switch( sStatus )
    {
        case 2:
            (void)smLayerCallback::abortTrans( mTrans );
        case 1:
            (void)smLayerCallback::freeTrans(mTrans);
            mTrans = NULL;
            break;
        default:
            break;
    }

    return IDE_FAILURE;

}


/* ------------------------------------------------
 * Description :
 *
 * ������ ���� ���� ��ƾ
 *
 * - interval �Ⱓ���� wait�ϴٰ� buffer flush
 *   ����
 * ----------------------------------------------*/
void sdnbTDBuild::run()
{
    scSpaceID          sTBSID;
    UChar            * sPage;
    UChar            * sSlotDirPtr;
    UChar            * sSlot;
    SInt               sSlotCount;
    SInt               i;
    smSCN              sInfiniteSCN;
    smSCN              sStmtSCN;
    idBool             sIsSuccess;
    idBool             sIsUniqueIndex = ID_FALSE;
    idBool             sIsNotNullIndex = ID_FALSE;
    sdnbColumn       * sColumn;
    scPageID           sCurPageID;
    sdpPhyPageHdr     *sPageHdr;
    SInt               sState = 0;
    ULong              sRowBuf[SD_PAGE_SIZE/ID_SIZEOF(ULong)];
    sdSID              sRowSID;
    idBool             sIsRowDeleted;
    sdbMPRMgr          sMPRMgr;
    scOffset           sOffset;
    idBool             sIsPageLatchReleased = ID_TRUE;
    UInt               sColumnHeaderLen;
    UInt               sColumnValueLen;
    sdbMPRFilter4PScan sFilter4Scan;

    if( ((sdnRuntimeHeader*)mIndex->mHeader)->mIsUnique == ID_TRUE )
    {
        sIsUniqueIndex = ID_TRUE;
    }
    if( ((sdnRuntimeHeader*)mIndex->mHeader)->mIsNotNull == ID_TRUE )
    {
        sIsNotNullIndex = ID_TRUE;
    }

    ideLog::log( IDE_SM_0, "\
========================================\n\
  [IDX_CRE] BUILD INDEX ( Top-Down )    \n\
            NAME : %s                   \n\
            ID   : %u                   \n\
========================================\n\
          BUILD_THREAD_COUNT     : %d   \n\
========================================\n",
                 mIndex->mName,
                 mIndex->mId,
                 mTotalThreadCnt );
    
    SM_SET_SCN_INFINITE( &sInfiniteSCN );
    SM_MAX_SCN( &sStmtSCN );
    SM_SET_SCN_VIEW_BIT( &sStmtSCN );

    sTBSID = mTable->mSpaceID;

    IDE_TEST( sMPRMgr.initialize(
                  mStatistics,
                  sTBSID,
                  sdpSegDescMgr::getSegHandle( &mTable->mFixed.mDRDB ),
                  sdbMPRMgr::filter4PScan )
              != IDE_SUCCESS );
    sFilter4Scan.mThreadCnt = mTotalThreadCnt;
    sFilter4Scan.mThreadID  = mID;
    sState = 1;

    IDE_TEST( sMPRMgr.beforeFst() != IDE_SUCCESS );

    while( 1 )
    {
        IDE_TEST( sMPRMgr.getNxtPageID( (void*)&sFilter4Scan,
                                        &sCurPageID )
                  != IDE_SUCCESS );
        if( sCurPageID == SD_NULL_PID )
        {
            break;
        }

        IDE_TEST( sdbBufferMgr::getPage( mStatistics,
                                         sTBSID,
                                         sCurPageID,
                                         SDB_S_LATCH,
                                         SDB_WAIT_NORMAL,
                                         SDB_MULTI_PAGE_READ,
                                         &sPage,
                                         &sIsSuccess )
                  != IDE_SUCCESS );
        sIsPageLatchReleased = ID_FALSE;

        sPageHdr = sdpPhyPage::getHdr( sPage );

        if( sdpPhyPage::getPageType( sPageHdr ) == SDP_PAGE_DATA )
        {
            sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sPage);
            sSlotCount  = sdpSlotDirectory::getCount(sSlotDirPtr);

            for( i = 0; i < sSlotCount; i++ )
            {
                if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, i)
                    == ID_TRUE )
                {
                    continue;
                }

                IDE_TEST( sdpSlotDirectory::getPagePtrFromSlotNum(sSlotDirPtr, 
                                                                  i,
                                                                  &sSlot)
                          != IDE_SUCCESS );
                IDE_DASSERT(sSlot != NULL);

                if( sdcRow::isHeadRowPiece(sSlot) != ID_TRUE )
                {
                    continue;
                }
                if( sdcRow::isDeleted(sSlot) == ID_TRUE )
                {
                    continue;
                }

                sRowSID = SD_MAKE_SID( sPageHdr->mPageID, i);

                sColumn = &((sdnbHeader*)mIndex->mHeader)->mColumns[0];

                IDE_TEST( sdnbBTree::makeKeyValueFromRow(
                                                      mStatistics,
                                                      NULL, /* aMtx */
                                                      NULL, /* aSP */
                                                      mTrans,
                                                      mTable,
                                                      mIndex,
                                                      sSlot,
                                                      SDB_MULTI_PAGE_READ,
                                                      sTBSID,
                                                      SMI_FETCH_VERSION_LAST,
                                                      SD_NULL_RID, /* aTssRID */
                                                      NULL,        /* aSCN */
                                                      NULL,        /* aInfiniteSCN */
                                                      (UChar*)sRowBuf,
                                                      &sIsRowDeleted,
                                                      &sIsPageLatchReleased )
                          != IDE_SUCCESS );

                IDE_ASSERT( sIsRowDeleted == ID_FALSE );

                /* BUG-23319
                 * [SD] �ε��� Scan�� sdcRow::fetch �Լ����� Deadlock �߻����ɼ��� ����. */
                /* row fetch�� �ϴ��߿� next rowpiece�� �̵��ؾ� �ϴ� ���,
                 * ���� page�� latch�� Ǯ�� ������ deadlock �߻����ɼ��� �ִ�.
                 * �׷��� page latch�� Ǭ ���� next rowpiece�� �̵��ϴµ�,
                 * ���� �Լ������� page latch�� Ǯ������ ���θ� output parameter�� Ȯ���ϰ�
                 * ��Ȳ�� ���� ������ ó���� �ؾ� �Ѵ�. */
                if( sIsPageLatchReleased == ID_TRUE )
                {
                    /* BUG-25126
                     * [5.3.1 SD] Index Bottom-up Build �� Page fetch�� �������!! */
                    IDE_TEST( sdbBufferMgr::getPage( mStatistics,
                                                     sTBSID,
                                                     sCurPageID,
                                                     SDB_S_LATCH,
                                                     SDB_WAIT_NORMAL,
                                                     SDB_MULTI_PAGE_READ,
                                                     &sPage,
                                                     &sIsSuccess )
                              != IDE_SUCCESS );
                    sIsPageLatchReleased = ID_FALSE;

                    sSlotDirPtr = sdpPhyPage::getSlotDirStartPtr(sPage);
                    sSlotCount  = sdpSlotDirectory::getCount(sSlotDirPtr);

                    /* page latch�� Ǯ�� ���̿� �ٸ� Ʈ�������
                     * ���� �������� �����Ͽ� ���� ������ ������ �� �ִ�. */
                    if( sdpSlotDirectory::isUnusedSlotEntry(sSlotDirPtr, i)
                        == ID_TRUE )
                    {
                        continue;
                    }
                }

                if( sIsNotNullIndex == ID_TRUE )
                {
                    sOffset = 0;
                    sColumn = &((sdnbHeader*)mIndex->mHeader)->mColumns[0];
                    for( ; sColumn != ((sdnbHeader*)mIndex->mHeader)->mColumnFence; sColumn++ )
                    {
                        if( sdnbBTree::isNullColumn(sColumn,
                                                    ((UChar*)sRowBuf) + sOffset ) == ID_TRUE )
                        {
                            IDE_RAISE( ERR_NOTNULL_VIOLATION );
                        }
                        sOffset += sdnbBTree::getColumnLength(
                                            SDNB_GET_COLLENINFO( &(sColumn->mKeyColumn) ),
                                            ((UChar*)sRowBuf) + sOffset,
                                            &sColumnHeaderLen,
                                            &sColumnValueLen,
                                            NULL );
                    }
                }

                IDE_TEST_RAISE( mIndex->mInsert( mStatistics, /* idvSQL* */
                                                 mTrans,
                                                 mTable,
                                                 mIndex,
                                                 sInfiniteSCN,
                                                 (SChar*)sRowBuf,
                                                 NULL,
                                                 sIsUniqueIndex,
                                                 sStmtSCN,
                                                 &sRowSID,
                                                 NULL,
                                                 ID_ULONG_MAX,/* aInsertWaitTime */
                                                 ID_FALSE )   /* aForbiddenToRetry */
                                != IDE_SUCCESS, ERR_INSERT );
            }
        }

        /* BUG-23388: Top Down Build�� Meta Page�� Scan�� ��� SLatch�� Ǯ��
         *            �ʽ��ϴ�. */
        sIsPageLatchReleased = ID_TRUE;
        IDE_TEST( sdbBufferMgr::releasePage( mStatistics,
                                             sPage )
                  != IDE_SUCCESS );

        IDE_TEST(iduCheckSessionEvent(mStatistics) != IDE_SUCCESS);

    }

    sState = 0;
    IDE_TEST( sMPRMgr.destroy() != IDE_SUCCESS );

    return;

    /* PRIMARY KEY�� NULL���� �����Ҽ� ���� */
    IDE_EXCEPTION( ERR_NOTNULL_VIOLATION );
    {
        IDE_SET( ideSetErrorCode( smERR_ABORT_NOT_NULL_VIOLATION ) );
        mErrorCode = ideGetErrorCode();
        ideCopyErrorInfo( &mErrorMgr, ideGetErrorMgr() );
    }
    IDE_EXCEPTION( ERR_INSERT );
    {
        mErrorCode = ideGetErrorCode();
        ideCopyErrorInfo( &mErrorMgr, ideGetErrorMgr() );
    }
    IDE_EXCEPTION_END;

    (*mContinue) = ID_FALSE;

    IDE_PUSH();
    if( sIsPageLatchReleased == ID_FALSE )
    {
        IDE_ASSERT( sdbBufferMgr::releasePage( mStatistics,
                                               sPage )
                    == IDE_SUCCESS );
    }

    if( sState != 0 )
    {
        IDE_ASSERT( sMPRMgr.destroy() == IDE_SUCCESS );
    }

    IDE_ASSERT( smLayerCallback::abortTrans( mTrans )
                == IDE_SUCCESS );
    IDE_ASSERT( smLayerCallback::freeTrans( mTrans )
                == IDE_SUCCESS );
    mTrans = NULL;
    IDE_POP();

    return;

}
