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
 * $Id:$
 **********************************************************************/

#include <ide.h>
#include <sdd.h>
#include <smErrorCode.h>
#include <smu.h>
#include <smrDef.h>
#include <sdbDPathBufferMgr.h>
#include <sdbDPathBFThread.h>

sdbDPathBFThread::sdbDPathBFThread() : idtBaseThread()
{
}

sdbDPathBFThread::~sdbDPathBFThread()
{
}

/*******************************************************************************
 * Description : DPathBCBInfo �ʱ�ȭ
 *
 * aDPathBCBInfo - [IN] �ʱ�ȭ �� DPathBuffInfo
 ******************************************************************************/
IDE_RC sdbDPathBFThread::initialize( sdbDPathBuffInfo  * aDPathBCBInfo )
{
    IDE_TEST( sdbDPathBufferMgr::initBulkIOInfo(&mDPathBulkIOInfo)
              != IDE_SUCCESS );

    mFinish       = ID_FALSE;
    mDPathBCBInfo = aDPathBCBInfo;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*******************************************************************************
 * Description : �Ҵ�� Resource�� Free ��Ų��.
 ******************************************************************************/
IDE_RC sdbDPathBFThread::destroy()
{
    IDE_TEST( sdbDPathBufferMgr::destBulkIOInfo(&mDPathBulkIOInfo)
              != IDE_SUCCESS );

    mDPathBCBInfo = NULL;

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : Flush Thread�� ���۽�Ų��.
 **********************************************************************/
IDE_RC sdbDPathBFThread::startThread()
{
    IDE_TEST(start() != IDE_SUCCESS);
    IDE_TEST(waitToStart(0) != IDE_SUCCESS);

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/***********************************************************************
 * Description : mDPathBCBInfo�� ����Ű�� Flush Request List�� ���ؼ�
 *               Flush�۾��� �����Ѵ�. ����� �ֱ��
 *               __DIRECT_BUFFER_FLUSH_THREAD_SYNC_INTERVAL�̴�.
 **********************************************************************/
void sdbDPathBFThread::run()
{
    scSpaceID      sNeedSyncSpaceID = 0;
    PDL_Time_Value sTV;

    sTV.set(0, smuProperty::getDPathBuffFThreadSyncInterval() );

    while(mFinish == ID_FALSE)
    {
        idlOS::sleep( sTV );

        if( mFinish == ID_TRUE )
        {
            break;
        }

        /* Flush Request List�� ���ؼ� Flush�۾��� ��û�Ѵ�. */
        IDE_TEST( sdbDPathBufferMgr::flushBCBInList(
                      NULL, /* idvSQL* */
                      mDPathBCBInfo,
                      &mDPathBulkIOInfo,
                      &(mDPathBCBInfo->mFReqPgList),
                      &sNeedSyncSpaceID )
                  != IDE_SUCCESS );
    }

    IDE_TEST( sdbDPathBufferMgr::writePagesByBulkIO(
                  NULL, /* idvSQL* */
                  mDPathBCBInfo,
                  &mDPathBulkIOInfo,
                  &sNeedSyncSpaceID )
              != IDE_SUCCESS );

    if( sNeedSyncSpaceID != 0 )
    {
        /* ������ writePageȣ��� write�� ����� TableSpace�� ����
         * Sync�� �����Ѵ�. */
        IDE_TEST( sddDiskMgr::syncTBSInNormal(
                      NULL, /* idvSQL* */
                      sNeedSyncSpaceID ) != IDE_SUCCESS );
    }

    return;

    IDE_EXCEPTION_END;

    IDE_CALLBACK_FATAL( "Error In Direct Path Flush Thread" );
}

/***********************************************************************
 * Description : Flush Thread�� �����Ű�� ����ɶ����� ��ٸ���.
 **********************************************************************/
IDE_RC sdbDPathBFThread::shutdown()
{
    mFinish = ID_TRUE;

    IDE_TEST_RAISE(join() != IDE_SUCCESS,
                   err_thr_join);

    return IDE_SUCCESS;

    IDE_EXCEPTION(err_thr_join);
    {
        IDE_SET(ideSetErrorCode(smERR_FATAL_Systhrjoin));
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}
