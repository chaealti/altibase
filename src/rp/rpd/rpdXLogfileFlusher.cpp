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
 * $Id: rpdXLogfileFlusher.cpp
 ******************************************************************************/

#include <mtc.h>
#include <rpdXLogfileFlusher.h>
#include <rpdXLogfileMgr.h>
#include <rpuProperty.h>

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::initialize             *
 * ------------------------------------------------------------------*
 * xlogfile flush thread�� �ʱ�ȭ�� ���� ȣ��Ǵ� �Լ�
 *
 * aReplName      - [IN] replication�� �̸�
 * aXLogfileMgr   - [IN] xlogfile manager
 *********************************************************************/
IDE_RC rpdXLogfileFlusher::initialize( SChar *          aReplName,
                                       rpdXLogfileMgr * aXLogfileMgr )
{
    SChar   sBuffer[IDU_MUTEX_NAME_LEN] = {0,};
    UInt    sState = 0;
    SInt    sLengthCheck = 0;

    /* ���� �ʱ�ȭ */
    mXLogfileMgr = aXLogfileMgr;

    /* flush list �ʱ�ȭ */
    IDU_LIST_INIT( &mFlushList );

    /* thread ���� ����*/
    mExitFlag      = ID_FALSE;
    mEndFlag       = ID_FALSE;
    mIsThreadSleep = ID_FALSE;

    /* mutex �� CV �ʱ�ȭ */
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_FLUSHLIST_%s",
                                    aReplName);
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mFlushMutex.initialize( sBuffer,
                                      IDU_MUTEX_KIND_POSIX,
                                      IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "XLOGFILE_FLUSHER_CV_%s",
                                    aReplName);
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );
    IDE_TEST( mThreadWaitMutex.initialize( sBuffer,
                                           IDU_MUTEX_KIND_POSIX,
                                           IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mThreadWaitCV.initialize() != IDE_SUCCESS );
    sState = 3;

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_mutex_name )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_MUTEX_NAME, sBuffer ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    IDE_PUSH();

    switch( sState )
    {
        case 3:
            (void)mThreadWaitCV.destroy();
        case 2:
            (void)mThreadWaitMutex.destroy();
        case 1:
            (void)mFlushMutex.destroy();
        case 0:
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::finalize               *
 * ------------------------------------------------------------------*
 * xlogfile flush thread�� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileFlusher::finalize()
{
    mEndFlag = ID_TRUE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::destroy                *
 * ------------------------------------------------------------------*
 * xlogfile flush thread�� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileFlusher::destroy()
{
    /* flush thread���� ����ϴ� CV�� mutex�� �����Ѵ�. */
    if( mThreadWaitCV.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if( mThreadWaitMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    if( mFlushMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    return;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::run                    *
 * ------------------------------------------------------------------*
 * xlogfile flusher�� ���� �۾��� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileFlusher::run()
{
    idBool      sIsListEmpty = ID_TRUE;

    while( mExitFlag == ID_FALSE )
    {
        IDU_FIT_POINT( "rpdXLogfileFlusher::run" );

        sIsListEmpty = checkListEmpty(); 

        if( sIsListEmpty == ID_TRUE )
        {
            /* flush �� XLogfile�� ���� ��� ����Ѵ�. */
            sleepFlusher();
        }
        else
        {
            /* flush �� XLogfile�� ���� ��� flush�� �����Ѵ�. */
            IDE_TEST( flushXLogfiles() != IDE_SUCCESS );
        }
    }

    /* wake�� exitflag setting�� Ÿ�̹��� ���� ��� flush ����� �ִ� ���·� finalize�� ������ �� �ִ�.   *
     * �� ��츦 ����� finalize�� �ѹ� �� list�� üũ�� list�� ������� �ʴٸ� flush�� �����ϵ��� �Ѵ�. */
    sIsListEmpty = checkListEmpty(); 

    if( sIsListEmpty == ID_FALSE )
    {
        IDE_TEST( flushXLogfiles() != IDE_SUCCESS );
    }

    IDE_EXCEPTION_END;

    (void)finalize();

    return;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::flushXLogfile          *
 * ------------------------------------------------------------------*
 * �ش� �Լ��� write�� ���� �ϳ��� ���Ⱑ ������ flush thread������ ����Ѵ�.
 * write�� ���� flush�� ��ٸ��� ���ϵ鿡 ���� sync �� close�� �����Ѵ�.
 * sync���� �ش� ������ read���� ��������� üũ�ϰ� ������� �ƴ� ��� close�� �Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileFlusher::flushXLogfiles()
{
    rpdXLogfile *   sFlushFile = NULL;
    iduListNode *   sNode = NULL;

    sNode = getFileFromFlushList();

    while( sNode != NULL )
    {
        sFlushFile = (rpdXLogfile*)sNode->mObj;

        /* sync�� �����Ѵ�. */
        IDE_TEST( sFlushFile->mFile.syncUntilSuccess( rpdXLogfileMgr::rpdXLogfileEmergency ) != IDE_SUCCESS );

        IDU_FIT_POINT( "rpdXLogfileFlusher::flushXLogfiles" );
        mXLogfileMgr->decFileRefCnt( sFlushFile );

        /* ����� ���� ��带 �޸𸮿��� �����Ѵ�. */
        (void)iduMemMgr::free( sNode ); 

        sNode = getFileFromFlushList();
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::addFileToFlushList     *
 * ------------------------------------------------------------------*
 * flush�� ������ flush list�� ����ϴ� �Լ�
 * ��ϵ� ������ flush�� ����� �� close�ȴ�.
 * ���� : �ش� �Լ��� write������ ȣ��ȴ�.(flush���� ȣ������ �ʴ´�.)
 *
 * aXLogfile    - [IN] flush�� ������ xlogfile
 *********************************************************************/
IDE_RC rpdXLogfileFlusher::addFileToFlushList( void * aXLogfile )
{
    iduListNode *   sNode;
    UInt            sState = 0;

    IDE_ASSERT( aXLogfile != NULL );

    /* flush list�� �߰��� node�� �����Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                 ID_SIZEOF( iduListNode ),
                                 (void**)&(sNode),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );
    sState = 1;

    IDU_LIST_INIT_OBJ( sNode, (void*)aXLogfile );

    /* flush mutex�� ��� flush list�� �����Ѵ�. */
    IDE_ASSERT( mFlushMutex.lock( NULL ) == IDE_SUCCESS );
    IDU_LIST_ADD_LAST( &mFlushList, sNode );
    IDE_ASSERT( mFlushMutex.unlock() == IDE_SUCCESS );

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    switch( sState )
    {
        case 1:
            (void)iduMemMgr::free( sNode );
        case 0:
        default:
            break;
    }

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::getFileFromFlushList   *
 * ------------------------------------------------------------------*
 * flush list���� flush�� file�� ������ �Լ�
 * flush list�� �� �� ��带 ��������, ������ ���� ����Ʈ���� ���ŵȴ�.
 * write(transfer)�� flush list �߰� �۾��� ���ü� ������ �߻����� �ʵ��� mutex�� ��´�.
 *********************************************************************/
iduListNode * rpdXLogfileFlusher::getFileFromFlushList()
{
    iduListNode *   sNode = NULL;

    /* flush mutex�� ��� list���� node�� ������. */
    IDE_ASSERT( mFlushMutex.lock( NULL ) == IDE_SUCCESS );

    if( IDU_LIST_IS_EMPTY( &mFlushList ) == ID_TRUE )
    {
        /* flush list�� ����ִٸ� null�� �����Ѵ�. */
    }
    else
    {
        /* list���� node�� ������ �����Ѵ�. */
        sNode = IDU_LIST_GET_FIRST( &mFlushList );

        IDU_LIST_REMOVE( sNode );
    }

    IDE_ASSERT( mFlushMutex.unlock() == IDE_SUCCESS );

    return sNode;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::checkListEmpty         *
 * ------------------------------------------------------------------*
 * flush list�� ����ִ��� Ȯ���ϴ� �Լ�
 * ���ü� ��� ���� lock�� ��� Ȯ���Ѵ�.
 *********************************************************************/
idBool rpdXLogfileFlusher::checkListEmpty()
{
    idBool  sIsListEmpty = ID_FALSE;

    IDE_ASSERT( mFlushMutex.lock( NULL ) == IDE_SUCCESS );

    if( IDU_LIST_IS_EMPTY( &mFlushList ) == ID_TRUE )
    {
        sIsListEmpty = ID_TRUE;
    }
    else
    {
        sIsListEmpty = ID_FALSE;
    }

    IDE_ASSERT( mFlushMutex.unlock() == IDE_SUCCESS );

    return sIsListEmpty;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::wakeupFlusher      *
 * ------------------------------------------------------------------*
 * xlogfile�� flush thread�� ����� �Լ�
 * ���� : �ش� �Լ��� write������ ȣ��ȴ�.(flush���� ȣ������ �ʴ´�.)
 *********************************************************************/
void rpdXLogfileFlusher::wakeupFlusher()
{
    IDE_ASSERT( mThreadWaitMutex.lock( NULL ) == IDE_SUCCESS );

    IDU_FIT_POINT( "rpdXLogfileFlusher::wakeupFlusher" );

    if( mIsThreadSleep == ID_TRUE )
    {
        IDE_ASSERT( mThreadWaitCV.signal() == IDE_SUCCESS );
    }

    IDE_ASSERT( mThreadWaitMutex.unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_END;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::sleepFlusher       *
 * ------------------------------------------------------------------*
 * �۾��� ������ flush thread�� ��� ���·� �����Ѵ�.
 * ���� signal�� ���� �ʾƵ� 1�ʿ� �ѹ��� thread�� �����.
 * ���� : �ش� �Լ��� flusher������ ȣ��ȴ�.
 *********************************************************************/
void rpdXLogfileFlusher::sleepFlusher()
{
    PDL_Time_Value    sCheckTime;
    PDL_Time_Value    sSleepSec;

    sCheckTime.initialize();
    sSleepSec.initialize();

    sSleepSec.set( 1 );

    IDE_ASSERT( mThreadWaitMutex.lock( NULL ) == IDE_SUCCESS );
    sCheckTime = idlOS::gettimeofday() + sSleepSec;

    mIsThreadSleep = ID_TRUE; 
    (void)mThreadWaitCV.timedwait( &mThreadWaitMutex, &sCheckTime );
    mIsThreadSleep = ID_FALSE;

    IDE_ASSERT( mThreadWaitMutex.unlock() == IDE_SUCCESS );

    return;
}
