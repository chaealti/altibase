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
 * $Id: rpdXLogfileCreater.cpp
 ******************************************************************************/

#include <mtc.h>
#include <rpdXLogfileCreater.h>
#include <rpdXLogfileMgr.h>
#include <rpxReceiver.h>
#include <rpuProperty.h>

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::initialize             *
 * ------------------------------------------------------------------*
 * xlogfile create thread�� �ʱ�ȭ�� ���� ȣ��Ǵ� �Լ�
 * �÷��׸� �ʱⰪ���� ������ �� mutex�� conditional variable�� �ʱ�ȭ �Ѵ�.
 *
 * aReplName         - [IN] replication�� �̸�
 * aReadXLogLSN      - [IN] ���� xlogfile�� ���� ��ġ
 * aXLogfileMgr      - [IN] xlogfile manager
 *********************************************************************/
IDE_RC rpdXLogfileCreater::initialize( SChar *          aReplName,
                                       rpXLogLSN        aReadXLogLSN,
                                       rpdXLogfileMgr * aXLogfileMgr,
                                       void *           aReceiverPtr )
{
    SChar           sBuffer[IDU_MUTEX_NAME_LEN] = {0,};
    UInt            sState = 0;
    UInt            sFileNo = 0;
    SInt            sLengthCheck = 0;

    IDE_DASSERT( aReceiverPtr != NULL );

    /* ���� �ʱ�ȭ */
    mXLogfileMgr = aXLogfileMgr;
    mReceiverPtr = aReceiverPtr;

    /* ���� ���� ���� ���� */
    mLastCreatedFileNo = ID_UINT_MAX;
    mPrepareXLogfileCnt = 0;
    idlOS::strncpy( mRepName, aReplName, QCI_MAX_NAME_LEN + 1 );
    IDE_TEST( idp::readPtr( "XLOGFILE_DIR", (void**)&mXLogDirPath ) != IDE_SUCCESS );

    /* ���� ���� ���� ���� */
    mFileCreateCount = 0;

    /* thread ���� ���� */
    mExitFlag = ID_FALSE;
    mEndFlag = ID_FALSE;
    mIsThreadSleep = ID_FALSE;

    /* mutex �� CV �ʱ�ȭ */
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "PREOPENFILE_COUNT_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );

    IDE_TEST( mFileCntMutex.initialize( sBuffer,
                                        IDU_MUTEX_KIND_POSIX,
                                        IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 1;

    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "CREATER_CV_%s",
                                    aReplName );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );

    IDE_TEST( mThreadWaitMutex.initialize( sBuffer,
                                           IDU_MUTEX_KIND_POSIX,
                                           IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );
    sState = 2;

    IDE_TEST( mThreadWaitCV.initialize() != IDE_SUCCESS );
    sState = 3;

    if( aReadXLogLSN == RP_XLOGLSN_INIT )
    {
        /* ���� meta data�� ���� ��� �� ������ ����� ����ؾ� �Ѵ�. */
        IDE_TEST( createXLogfile( 0 ) != IDE_SUCCESS );

        mOldestExistXLogfileNo = 0;

        /* read ���� ������ ���� ������ ���Ϸ� �����Ѵ�. */
        mXLogfileMgr->mCurReadXLogfile = mXLogfileMgr->mXLogfileListTail;
    }
    else
    {
        /* ���� ������ ���� ��� ���� ������ xlogfile ��ȣ�� �����Ѵ�. */
        RP_GET_FILENO_FROM_XLOGLSN( sFileNo, aReadXLogLSN );

        IDE_TEST( getOldestXLogfileNo( sFileNo ) != IDE_SUCCESS );
    }

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
            (void)mFileCntMutex.destroy();
        case 0:
        default:
            break;
    }

    IDE_POP();

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::finalize               *
 * ------------------------------------------------------------------*
 * xlogfile create thread�� ������Ű�� �Լ�
 *********************************************************************/
void rpdXLogfileCreater::finalize()
{
    idBool              sIsLock = ID_FALSE;
    rpdXLogfileHeader * sHeader = NULL;

    IDE_ASSERT( mXLogfileMgr->mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );
    sIsLock = ID_TRUE;

    /* flusher�� ������ creater���� ���� �̷�����Ƿ� flush ����� ������ �̹� �������ִ�. *
     * ���� ������ read���� ���ϰ� ������ �ϰ� ������� ���� ���ϻ��̹Ƿ�                  *
     * ���⼭�� ������ �ϰ� ������� ���� ���ϸ� ã�� �ݵ��� �Ѵ�.                         */
    while( mXLogfileMgr->mXLogfileListTail != NULL )
    {
        sHeader = (rpdXLogfileHeader*)mXLogfileMgr->mXLogfileListTail->mBase;

        /* �ش� ������ write offset�� �ʱⰪ�̶�� �ش� ������ create�� �ǰ� ������ �ʾҴ�. */
        if( sHeader->mFileWriteOffset == XLOGFILE_HEADER_SIZE_INIT )
        {
            IDE_DASSERT( mXLogfileMgr->mXLogfileListTail->mFileRefCnt == 1 );

            sIsLock = ID_FALSE;
            IDE_ASSERT( mXLogfileMgr->mXLogfileListMutex.unlock() == IDE_SUCCESS );

            mXLogfileMgr->decFileRefCnt( mXLogfileMgr->mXLogfileListTail );

            sIsLock = ID_TRUE;
            IDE_ASSERT( mXLogfileMgr->mXLogfileListMutex.lock( NULL ) == IDE_SUCCESS );
        }
        else
        {
            sIsLock = ID_FALSE;
            IDE_ASSERT( mXLogfileMgr->mXLogfileListMutex.unlock() == IDE_SUCCESS );

            break;
        }
    }

    if( sIsLock == ID_TRUE )
    {
        IDE_ASSERT( mXLogfileMgr->mXLogfileListMutex.unlock() == IDE_SUCCESS );
    }

    mEndFlag = ID_TRUE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::destroy                *
 * ------------------------------------------------------------------*
 * xlogfile create thread�� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileCreater::destroy()
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

    if( mFileCntMutex.destroy() != IDE_SUCCESS )
    {
        IDE_ERRLOG(IDE_RP_0);
    }

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::run                    *
 * ------------------------------------------------------------------*
 * xlogfile create thread�� ���� �۾��� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileCreater::run()
{
    UInt            sPrepareFileCnt = 0;
    time_t          sStartTime;
    time_t          sNowTime;
    //test
    UInt            sFileCntCheck = RPU_REPLICATION_XLOGFILE_PREPARE_COUNT;

    sStartTime = idlOS::time();

    while( mExitFlag == ID_FALSE )
    {
        if( mFileCreateCount > RPU_REPLICATION_XLOGFILE_REMOVE_INTERVAL_BY_FILE_CREATE )
        {
            /* XLOGFILE_REMOVE_INTERVAL_BY_FILE_CREATE ������Ƽ �� �̻��� ������ �������� ���  *
             * ���ð��� ������� ���ʿ��� ������ �����ϵ��� �Ѵ�.                             */

            IDE_TEST( checkAndRemoveOldXLogfiles() != IDE_SUCCESS );

            sStartTime = idlOS::time();
            mFileCreateCount = 0;
        }

        getPrepareFileCnt( &sPrepareFileCnt );

        if( sPrepareFileCnt < sFileCntCheck )
        {
            IDE_TEST( createXLogfiles() != IDE_SUCCESS );
        }
        else
        {
            sNowTime = idlOS::time();

            if( ( sNowTime - sStartTime ) > RPU_REPLICATION_XLOGFILE_REMOVE_INTERVAL_BY_FILE_CREATE )
            {
                /* ������ �����Ѵ�. */
                IDE_TEST( checkAndRemoveOldXLogfiles() != IDE_SUCCESS );

                sStartTime = idlOS::time();
                mFileCreateCount = 0;
            }
            else
            {
                /* �� ������ ���� �ʿ䰡 ���� ��� ��� ����Ѵ�. */
                (void)sleepCreater();
            }
        }
    }

    return;

    IDE_EXCEPTION_END;

    /* ���� �߻��� finalize�� �����ϰ� destroy�� �������� �ʵ��� �Ͽ�           * 
     * �ٸ� thread���� ���ٽ� creater thread�� �����Ǿ��ٴ� ���� �˸����� �Ѵ�. */
    finalize();

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::createXLogfiles        *
 * ------------------------------------------------------------------*
 * �ټ��� xlogfile�� �����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileCreater::createXLogfiles()
{
    UInt            sNewFileNo = 0;
    UInt            sPrepareFileCnt = 0;

    IDE_DASSERT( mLastCreatedFileNo != ID_UINT_MAX );

    getPrepareFileCnt( &sPrepareFileCnt );

    while( sPrepareFileCnt < RPU_REPLICATION_XLOGFILE_PREPARE_COUNT )
    {
        sNewFileNo = mLastCreatedFileNo + 1;

        /* �� ������ �����Ѵ�. */
        IDE_TEST( createXLogfile( sNewFileNo ) != IDE_SUCCESS );

        /* xlogfile count�� ������Ų��. */
        incPrepareFileCnt();

        /* write(transfer)���� signal�� ���� ������� ��� �����. */
        mXLogfileMgr->wakeupWriter();

        sPrepareFileCnt++;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::incPrepareFileCnt      *
 * ------------------------------------------------------------------*
 * xlogfile creater�� ���� �غ�� xlogfile count�� ������Ų��.
 * �ش� �Լ��� ���� �����ÿ��� ȣ��ȴ�.
 *********************************************************************/
void rpdXLogfileCreater::incPrepareFileCnt()
{
    IDE_ASSERT( mFileCntMutex.lock( NULL ) == IDE_SUCCESS );
    mPrepareXLogfileCnt++;
    IDE_ASSERT( mFileCntMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::decPrepareFileCnt      *
 * ------------------------------------------------------------------*
 * xlogfile creater�� ���� �غ�� xlogfile count�� ���ҽ�Ų��.
 * �ش� �Լ��� write(transfer)���� �� ������ ������ �� ȣ��ȴ�.
 *********************************************************************/
void rpdXLogfileCreater::decPrepareFileCnt()
{
    IDE_ASSERT( mFileCntMutex.lock( NULL ) == IDE_SUCCESS );
    mPrepareXLogfileCnt--;
    IDE_ASSERT( mFileCntMutex.unlock() == IDE_SUCCESS );

    return;
}
/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::getPrepareFileCnt      *
 * ------------------------------------------------------------------*
 * xlogfile creater���� �̸� �غ��ص� xlogfile�� ���� �����´�.
 *********************************************************************/
void rpdXLogfileCreater::getPrepareFileCnt( UInt *  aResult )
{
    IDE_ASSERT( aResult != NULL );

    IDE_ASSERT( mFileCntMutex.lock( NULL ) == IDE_SUCCESS );
    *aResult = mPrepareXLogfileCnt;
    IDE_ASSERT( mFileCntMutex.unlock() == IDE_SUCCESS );

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileFlusher::wakeupFileCreater      *
 * ------------------------------------------------------------------*
 * xlogfile�� �����ϴ� thread�� ����� �۾��� �����ϴ� �Լ�
 *********************************************************************/
void rpdXLogfileCreater::wakeupCreater()
{
    IDE_ASSERT( mThreadWaitMutex.lock( NULL ) == IDE_SUCCESS );

    IDU_FIT_POINT( "rpdXLogfileCreater::wakeupCreater" );

    if( mIsThreadSleep == ID_TRUE )
    {
        IDE_ASSERT( mThreadWaitCV.signal() == IDE_SUCCESS );
    }

    IDE_ASSERT( mThreadWaitMutex.unlock() == IDE_SUCCESS );

    IDE_EXCEPTION_END;

    return;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::sleepCreater           *
 * ------------------------------------------------------------------*
 * ����� pre pen file�� �غ�Ǿ��� �� �ӽ÷� ª�� �ð� create thread�� ����.
 * ������� thread�� ����  signal�� ���� ������ �ʾƵ� 1�ʸ��� �ڵ������� �Ͼ��.
 *********************************************************************/
void rpdXLogfileCreater::sleepCreater()
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

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::
 * ------------------------------------------------------------------*
 * Active ���� �ݿ��Ǿ �����ص� �Ǵ� xlogfile no�� (sRemoteCheckpointedFileNo)
 * Standby ���� �ݿ��Ǿ �����ص� �Ǵ� xlogfile no (sUsingFileNo) �� ���ؼ�
 * �� ���� ���� ��ȣ���� xlogfile�� �����Ѵ�.
 *********************************************************************/
IDE_RC rpdXLogfileCreater::checkAndRemoveOldXLogfiles( )
{
    rpxReceiver *   sReceiver = (rpxReceiver*)mReceiverPtr;
    
    UInt            sUsingFileNo = 0;
    UInt            sRemoteCheckpointedFileNo = 0;
    UInt            sAbleToRemoveFileNo = 0;

    IDE_DASSERT( sReceiver != NULL );

    IDE_TEST( sReceiver->findXLogfileNoByRemoteCheckpointSN( &sRemoteCheckpointedFileNo ) 
              != IDE_SUCCESS );
    sUsingFileNo = sReceiver->getMinimumUsingXLogFileNo();

    if ( sRemoteCheckpointedFileNo > sUsingFileNo )
    {
        sAbleToRemoveFileNo = sUsingFileNo;
    }
    else
    {
        sAbleToRemoveFileNo = sRemoteCheckpointedFileNo;
    }
    
    if ( sAbleToRemoveFileNo > mOldestExistXLogfileNo )
    {
        IDE_TEST( removeOldXLogfiles( sAbleToRemoveFileNo ) != IDE_SUCCESS );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::removeOldXLogfiles         *
 * ------------------------------------------------------------------*
 * ���ʿ��� xlogfile�� ��� �����Ѵ�.
 * ���ڷ� ���� ���� ��ȣ���� ������ xlogfile�� ��� �����Ѵ�.
 * �ش� ������ �������� �ʴ´�.
 * ����, ���ڷ� ���� ���� ��ȣ���� read ���� ���� ��ȣ�� ���� ���
 * (read�� �� �տ� ���� ���)���� read ���� ���� �ٷ� �������� �����Ѵ�.
 *
 * aUsingXLogfileNo - [IN] �����ؾ� �� ���� ������ xlogfile�� ��ȣ
 *********************************************************************/
IDE_RC rpdXLogfileCreater::removeOldXLogfiles( UInt aUsingXLogfileNo )
{
    SChar   sFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    UInt    i;
    UInt    sFileNo;
    SInt    sLengthCheck = 0;

    IDE_DASSERT( mOldestExistXLogfileNo <= aUsingXLogfileNo );

    if( mXLogfileMgr->mXLogfileListHeader != NULL )
    {
        sFileNo = mXLogfileMgr->mXLogfileListHeader->mFileNo;
    }
    else
    {
        sFileNo = ID_UINT_MAX;
    }

    for( i = mOldestExistXLogfileNo; i < aUsingXLogfileNo; i++ )
    {
        if( sFileNo <= i )
        {
            /* ���� �޸𸮿� �ö�� �ִ� ������ ����� �ȵȴ�. */
            break;
        }

        sLengthCheck = idlOS::snprintf( sFilePath,
                                        XLOGFILE_PATH_MAX_LENGTH,
                                        "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                        mXLogDirPath,
                                        IDL_FILE_SEPARATOR,
                                        mRepName,
                                        i );
        IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

        if( removeXLogfile ( sFilePath ) != IDE_SUCCESS )
        {
            IDE_ERRLOG(IDE_RP_0);
        }

        mOldestExistXLogfileNo = i + 1;
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::removeXLogfile         *
 * ------------------------------------------------------------------*
 * �Է¹��� path�� xlogfile�� �����Ѵ�.
 *
 * aFilePath - [IN] ������ xlogfile�� path
 *********************************************************************/
IDE_RC rpdXLogfileCreater::removeXLogfile( SChar * aFilePath )
{
    SInt    rc;

    if( isFileExist( aFilePath ) == ID_FALSE )
    {
        /* �����Ϸ��� ������ �̹� ���� */
        ideLog::log( IDE_RP_0, "%s file is not exist.", aFilePath );
    }
    else
    {
        rc = idf::unlink( aFilePath );

        IDE_TEST_RAISE( rc != 0, unlink_fail );
    }

    return IDE_SUCCESS;

    IDE_EXCEPTION( unlink_fail )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_FAILURE_UNLINK_FILE, aFilePath, rc ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::getOldestXLogfileNo    *
 * ------------------------------------------------------------------*
 * ���� ������ xlogfile�� ��ȣ�� ã�´�.
 * ���⼭ ã�� ��ȣ�� ������ xlogfile ������ ���� ��ġ�� �ȴ�.
 * ������ xlogfile ������ �Ź� ȣ��� ��� �δ��� �ʹ� ũ�Ƿ� initialize�ÿ��� �ѹ� ȣ��ȴ�.
 *********************************************************************/
IDE_RC rpdXLogfileCreater::getOldestXLogfileNo( UInt aFileNo )
{
    UInt    sFileNo = 0;
    SChar   sFilePath[XLOGFILE_PATH_MAX_LENGTH];
    UInt    i = 0;
    SInt    sLengthCheck = 0;

    for( i = aFileNo; i > 0; i-- )
    {
        idlOS::memset( sFilePath, 0 , XLOGFILE_PATH_MAX_LENGTH );

        sLengthCheck = idlOS::snprintf( sFilePath,
                                        XLOGFILE_PATH_MAX_LENGTH,
                                        "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                        mXLogDirPath,
                                        IDL_FILE_SEPARATOR,
                                        mRepName,
                                        i );
        IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

        if( isFileExist( sFilePath ) == ID_FALSE )
        {
            sFileNo = i + 1;

            break;
        }
    }

    mOldestExistXLogfileNo = sFileNo;

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}

/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::createXLogfile         *
 * ------------------------------------------------------------------*
 * �� xlogfile�� ������ �� open �� mapping�� �����Ѵ�.
 * �����Ǵ� ������ xlogfile[replication�̸�]_[��ȣ]�� ���¸� ������.
 *
 * aFileNo      - [IN] ������ ������ ��ȣ
 *********************************************************************/
IDE_RC rpdXLogfileCreater::createXLogfile( UInt aFileNo )
{
    rpdXLogfile *       sNewFile;
    SChar               sNewFilePath[XLOGFILE_PATH_MAX_LENGTH] = {0,};
    SChar               sBuffer[IDU_MUTEX_NAME_LEN];
    SInt                sLengthCheck = 0;
    rpdXLogfileHeader * sFileHeader = NULL;

    /* �� ������ path�� �����Ѵ�. */
    idlOS::memset( sNewFilePath, 0, XLOGFILE_PATH_MAX_LENGTH );
    sLengthCheck = idlOS::snprintf( sNewFilePath,
                                    XLOGFILE_PATH_MAX_LENGTH,
                                    "%s%cxlogfile%s_%"ID_UINT32_FMT,
                                    mXLogDirPath,
                                    IDL_FILE_SEPARATOR,
                                    mRepName,
                                    aFileNo );
    IDE_TEST_RAISE( sLengthCheck == XLOGFILE_PATH_MAX_LENGTH, too_long_file_path );

    /* �� ������ �����Ѵ�. */
    IDE_TEST( iduMemMgr::malloc( IDU_MEM_RP_RPD,
                                 ID_SIZEOF( rpdXLogfile ),
                                 (void**)&(sNewFile),
                                 IDU_MEM_IMMEDIATE ) != IDE_SUCCESS );

    IDE_TEST( sNewFile->mFile.initialize( IDU_MEM_RP_RPD,
                                          1, /* Max Open FD Count*/
                                          IDU_FIO_STAT_OFF,
                                          IDV_WAIT_INDEX_NULL )
              != IDE_SUCCESS );

    IDE_TEST( sNewFile->mFile.setFileName( sNewFilePath ) != IDE_SUCCESS );

    if( isFileExist( sNewFilePath ) == ID_FALSE )
    {
        IDE_TEST( sNewFile->mFile.createUntilSuccess( rpdXLogfileMgr::rpdXLogfileEmergency ) != IDE_SUCCESS );
    }
    else
    {
        /* �����Ϸ��� ������ �̹� �ִ� ��쿡�� �ش� ������ ���� �ʿ䰡 ����. */
    }

    /* mapping �� �� �ֵ��� ũ�⸦ �����Ѵ�. */
    IDE_TEST( sNewFile->mFile.open( ID_FALSE ) != IDE_SUCCESS );
    IDE_TEST( sNewFile->mFile.truncate( RPU_REPLICATION_XLOGFILE_SIZE ) != IDE_SUCCESS );
    IDE_TEST( sNewFile->mFile.close() != IDE_SUCCESS );

    /* xlogfile meta(rpdXLogfile)������ mutex�� �ʱ�ȭ �Ѵ�. */
    idlOS::memset( sBuffer, 0, IDU_MUTEX_NAME_LEN );
    sLengthCheck = idlOS::snprintf( sBuffer,
                                    IDU_MUTEX_NAME_LEN,
                                    "xlogfile%s_%"ID_UINT32_FMT,
                                    mRepName,
                                    aFileNo );
    IDE_TEST_RAISE( sLengthCheck == IDU_MUTEX_NAME_LEN, too_long_mutex_name );

    IDE_TEST( sNewFile->mRefCntMutex.initialize( sBuffer,
                                                 IDU_MUTEX_KIND_POSIX,
                                                 IDV_WAIT_INDEX_NULL ) != IDE_SUCCESS );

    /* ������ ������ open �Ѵ�. */
    IDE_TEST( sNewFile->mFile.openUntilSuccess( ID_FALSE, O_RDWR ) != IDE_SUCCESS );

    /* mapping�� �����Ѵ�. */
    IDE_TEST( sNewFile->mFile.mmap( NULL,
                                    RPU_REPLICATION_XLOGFILE_SIZE,
                                    ID_TRUE,
                                    &( sNewFile->mBase ) ) != IDE_SUCCESS );

    /* ��Ÿ������ �����Ѵ�. */
    mXLogfileMgr->setFileMeta( sNewFile, aFileNo );

    mLastCreatedFileNo = aFileNo;

    /* ���� �� ������ file header�� �ʱ�ȭ�Ѵ�. */
    sFileHeader = (rpdXLogfileHeader*)(sNewFile->mBase);
    sFileHeader->mFileWriteOffset = XLOGFILE_HEADER_SIZE_INIT;
    sFileHeader->mFileFirstXLogOffset = XLOGFILE_HEADER_SIZE_INIT;
    sFileHeader->mFileFirstXSN = SM_SN_NULL;

    /* sync�� �����Ѵ�. */
    sNewFile->mFile.syncUntilSuccess( rpdXLogfileMgr::rpdXLogfileEmergency );

    /* xlogfile list�� �߰��Ѵ�. */
    mXLogfileMgr->addToFileListinLast( sNewFile );

    mFileCreateCount++;

    return IDE_SUCCESS;

    IDE_EXCEPTION( too_long_mutex_name )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_MUTEX_NAME, sBuffer ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION( too_long_file_path )
    {
        IDE_SET( ideSetErrorCode( rpERR_ABORT_RPD_TOO_LONG_FILE_PATH, sNewFilePath ) );
        IDE_ERRLOG(IDE_RP_0);
    }
    IDE_EXCEPTION_END;

    return IDE_FAILURE;
}


/*********************************************************************
 * FUNCTION DESCRIPTION : rpdXLogfileCreater::isFileExist            *
 * ------------------------------------------------------------------*
 * �Է¹��� path�� �ش��ϴ� ������ �̹� �����ϴ��� Ȯ���Ѵ�.
 * ���� : �ش� �Լ��� static �Լ��̴�.
 *
 * aFilePath - [IN] ������ xlogfile�� path
 *********************************************************************/
idBool rpdXLogfileCreater::isFileExist( SChar * aFilePath )
{
    idBool  sResult = ID_FALSE;

    if( idf::access( aFilePath, F_OK ) == 0 )
    {
        sResult = ID_TRUE;
    }
    else
    {
        sResult = ID_FALSE;
    }

    return sResult;
}
